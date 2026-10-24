#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <sys/time.h>
#include <time.h>

#include "device_config.h"
#include "firebase_rest_client.h"

constexpr int UNO_RX_PIN = 16;
constexpr int UNO_TX_PIN = 17;
constexpr uint32_t UART_BAUD = 9600;
constexpr uint32_t WIFI_RETRY_MS = 10000;
constexpr uint32_t UART_PING_INTERVAL_MS = 1000;
constexpr uint32_t UART_FRESHNESS_MS = 3000;
constexpr uint64_t MIN_VALID_EPOCH_MS = 1700000000000ULL;
constexpr size_t MAX_UART_LINE_LENGTH = 255;

HardwareSerial UnoSerial(2);
FirebaseRestClient cloud;
String lineBuffer;
uint32_t lastWifiAttempt = 0;
uint32_t lastPingAt = 0;
uint32_t lastPongAt = 0;
uint32_t pingSequence = 0;
String outstandingPingId;
uint64_t lastArduinoSeenEpochMs = 0;
bool hasValidPong = false;
bool lineOverflow = false;
bool doorStateKnown = false;
bool doorLocked = true;

uint64_t nowEpochMs() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return static_cast<uint64_t>(tv.tv_sec) * 1000ULL +
         static_cast<uint64_t>(tv.tv_usec / 1000);
}

bool clockIsValid() { return nowEpochMs() >= MIN_VALID_EPOCH_MS; }

void maintainWifi() {
  if (WiFi.status() == WL_CONNECTED || !WIFI_SSID[0]) return;
  const uint32_t now = millis();
  if (lastWifiAttempt != 0 &&
      static_cast<uint32_t>(now - lastWifiAttempt) < WIFI_RETRY_MS) {
    return;
  }
  lastWifiAttempt = now == 0 ? 1 : now;
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

bool arduinoReady() {
  return hasValidPong &&
         static_cast<uint32_t>(millis() - lastPongAt) <= UART_FRESHNESS_MS;
}

bool validCommandId(const String &id) {
  if (id.isEmpty() || id.length() > 32) return false;
  for (size_t i = 0; i < id.length(); ++i) {
    const char c = id[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
          (c >= 'A' && c <= 'Z') || c == '_' || c == '-')) {
      return false;
    }
  }
  return true;
}

void sendPing() {
  if (static_cast<uint32_t>(millis() - lastPingAt) <
      UART_PING_INTERVAL_MS) {
    return;
  }
  lastPingAt = millis();
  outstandingPingId = "p" + String(++pingSequence);
  JsonDocument ping;
  ping["type"] = "PING";
  ping["pingId"] = outstandingPingId;
  serializeJson(ping, UnoSerial);
  UnoSerial.println();
}

bool sendUnlockToUno(const CloudCommand &command) {
  if (!validCommandId(command.id) ||
      command.expiresAtEpochMs <= nowEpochMs() ||
      command.unlockDurationMs < 4000 ||
      command.unlockDurationMs > 30000 ||
      command.unlockDurationMs % 1000 != 0) {
    return false;
  }
  const uint64_t remaining = command.expiresAtEpochMs - nowEpochMs();
  if (remaining == 0 || remaining > 60000) return false;

  JsonDocument request;
  request["type"] = "UNLOCK";
  request["commandId"] = command.id;
  request["expiresAt"] = command.expiresAtEpochMs;
  request["ttlMs"] = static_cast<uint32_t>(remaining);
  request["unlockDurationMs"] = command.unlockDurationMs;
  if (serializeJson(request, UnoSerial) == 0 || UnoSerial.println() == 0) {
    return false;
  }
  cloud.noteCommandSent();
  return true;
}

void processUnoLine(const String &line) {
  JsonDocument message;
  if (deserializeJson(message, line)) {
    Serial.println("Ignored malformed JSON received from the Arduino UART.");
    return;
  }
  const String type = message["type"] | "";
  if (type == "PONG") {
    const String pingId = message["pingId"] | "";
    const String reportedDoor = message["door"] | "";
    if (pingId != outstandingPingId ||
        (reportedDoor != "locked" && reportedDoor != "unlocked")) {
      Serial.println("Ignored an invalid or stale Arduino PONG.");
      return;
    }
    lastPongAt = millis();
    lastArduinoSeenEpochMs = nowEpochMs();
    hasValidPong = true;
    doorStateKnown = true;
    doorLocked = reportedDoor == "locked";
  } else if (type == "STATE") {
    const String reportedDoor = message["door"] | "";
    if (reportedDoor == "locked" || reportedDoor == "unlocked") {
      doorStateKnown = true;
      doorLocked = reportedDoor == "locked";
    }
  }
  cloud.handleUnoMessage(message);
}

void readArduinoUart() {
  while (UnoSerial.available()) {
    const char c = static_cast<char>(UnoSerial.read());
    if (c == '\n') {
      if (lineOverflow) {
        Serial.println("Discarded an overlong Arduino UART message.");
      } else {
        lineBuffer.trim();
        if (!lineBuffer.isEmpty()) processUnoLine(lineBuffer);
      }
      lineBuffer = "";
      lineOverflow = false;
    } else if (c != '\r') {
      if (lineBuffer.length() < MAX_UART_LINE_LENGTH) {
        lineBuffer += c;
      } else {
        lineOverflow = true;
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  UnoSerial.begin(UART_BAUD, SERIAL_8N1, UNO_RX_PIN, UNO_TX_PIN);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  cloud.begin();
  maintainWifi();
  Serial.println("ESP32 gateway started; physical actuation requires a fresh Uno PONG.");
}

void loop() {
  maintainWifi();
  sendPing();
  readArduinoUart();

  const bool uartReady = arduinoReady();
  cloud.tick(uartReady, doorStateKnown, doorLocked,
             lastArduinoSeenEpochMs);

  if (clockIsValid() && uartReady) {
    CloudCommand command;
    if (cloud.beginNextCommand(command) && !sendUnlockToUno(command)) {
      cloud.failActiveCommand(
          "ESP32 could not safely transmit the accepted command to the Arduino.");
    }
  }
  delay(1);
}
