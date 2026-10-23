#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <time.h>

#if __has_include("secrets.h")
#include "secrets.h"  // gitignored; copy secrets.h.example
#else
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#endif

// Unverified on hardware. Pins and levels: see hardware/docs/ARCHITECTURE.md.
// The Uno TX line must pass through a level shifter before reaching UNO_RX_PIN.
constexpr int UNO_RX_PIN = 16;
constexpr int UNO_TX_PIN = 17;
constexpr uint32_t UART_BAUD = 9600;
constexpr uint32_t WIFI_RETRY_MS = 10000;
constexpr uint64_t MIN_VALID_EPOCH_MS = 1700000000000ULL;
HardwareSerial UnoSerial(2);
String lineBuffer;
String recentIds[32];
uint8_t recentCursor = 0;
uint32_t lastWifiAttempt = 0;

struct CloudCommand {
  String id;
  uint64_t expiresAtEpochMs;
  uint32_t unlockDurationMs;
};

// Cloud transport boundary. A Firebase implementation must reuse the existing
// commands/devices/auditLogs schema and an identity approved in the rules.
class CloudClient {
 public:
  virtual ~CloudClient() {}
  virtual bool nextPendingCommand(CloudCommand &out) = 0;
  virtual void publishUnoMessage(const JsonDocument &message) = 0;
};

// Placeholder until the ESP32 Firebase identity is decided (ARCHITECTURE.md).
class StubCloudClient : public CloudClient {
 public:
  bool nextPendingCommand(CloudCommand &) override { return false; }
  void publishUnoMessage(const JsonDocument &message) override {
    serializeJson(message, Serial); Serial.println();
  }
};

StubCloudClient cloud;

uint64_t nowEpochMs() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return (uint64_t)tv.tv_sec * 1000ULL + tv.tv_usec / 1000;
}

bool clockIsValid() { return nowEpochMs() >= MIN_VALID_EPOCH_MS; }

void maintainWifi() {
  if (WiFi.status() == WL_CONNECTED || !strlen(WIFI_SSID)) return;
  const uint32_t now = millis();
  if (lastWifiAttempt && now - lastWifiAttempt < WIFI_RETRY_MS) return;
  lastWifiAttempt = now ? now : 1;
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

bool hasSeen(const String &commandId) {
  for (const String &seen : recentIds) if (seen == commandId) return true;
  return false;
}

// Call only after the Firebase listener has authenticated an authorized
// command record and checked requestedBy, status, and expiry. nowEpochMs must
// come from a synchronized trusted clock (for example, SNTP).
bool validCommandId(const String &id) {
  if (id.length() == 0 || id.length() > 32) return false;
  for (size_t i = 0; i < id.length(); i++) {
    const char c = id[i];
    if (!isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') return false;
  }
  return true;
}

bool sendUnlockToUno(const String &commandId, uint64_t expiresAtEpochMs,
                     uint64_t nowEpochMs, uint32_t unlockDurationMs = 4000) {
  if (!validCommandId(commandId) || hasSeen(commandId) || expiresAtEpochMs <= nowEpochMs) return false;
  if (unlockDurationMs < 4000 || unlockDurationMs > 30000 || unlockDurationMs % 1000 != 0) return false;
  const uint64_t remaining = expiresAtEpochMs - nowEpochMs;
  if (remaining > 60000) return false;
  JsonDocument command;
  command["type"] = "UNLOCK";
  command["commandId"] = commandId;
  command["expiresAt"] = expiresAtEpochMs;
  command["ttlMs"] = (uint32_t)remaining;
  command["unlockDurationMs"] = unlockDurationMs;
  serializeJson(command, UnoSerial); UnoSerial.println();
  recentIds[recentCursor] = commandId;
  recentCursor = (recentCursor + 1) % 32;
  return true;
}

void processUnoLine(const String &line) {
  JsonDocument message;
  if (deserializeJson(message, line)) return;
  const char *type = message["type"] | "";
  if (strcmp(type, "ack") == 0 || strcmp(type, "nack") == 0 ||
      strcmp(type, "STATE") == 0 || strcmp(type, "EVENT") == 0) {
    cloud.publishUnoMessage(message);
  }
}

void setup() {
  Serial.begin(115200);
  UnoSerial.begin(UART_BAUD, SERIAL_8N1, UNO_RX_PIN, UNO_TX_PIN);
  WiFi.mode(WIFI_STA);
  configTime(0, 0, "pool.ntp.org");
  Serial.println("ESP32 gateway booted. Cloud client is a stub; see ARCHITECTURE.md.");
}

void loop() {
  maintainWifi();
  CloudCommand command;
  // Only relay with a synchronized clock; expiry cannot be checked otherwise.
  if (clockIsValid() && cloud.nextPendingCommand(command)) {
    sendUnlockToUno(command.id, command.expiresAtEpochMs, nowEpochMs(), command.unlockDurationMs);
  }
  while (UnoSerial.available()) {
    const char c = (char)UnoSerial.read();
    if (c == '\n') {
      processUnoLine(lineBuffer);
      lineBuffer = "";
    } else if (lineBuffer.length() < 255) lineBuffer += c;
  }
  delay(1);
}
