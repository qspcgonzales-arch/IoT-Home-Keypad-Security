#include <Arduino.h>
#include <avr/pgmspace.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <ArduinoJson.h>
#include <SHA256.h>
#include <SoftwareSerial.h>

// Provisional pins: see hardware/docs/ARCHITECTURE.md. Unverified on hardware.
// The ESP32 link uses A2/A3 so D0/D1 stay free for USB upload and debugging.
constexpr uint8_t LINK_RX_PIN = A2;
constexpr uint8_t LINK_TX_PIN = A3;
constexpr uint32_t LINK_BAUD = 9600;
SoftwareSerial link(LINK_RX_PIN, LINK_TX_PIN);
constexpr uint8_t SERVO_PIN = 10;
constexpr uint8_t IR_PIN = 11;
constexpr uint8_t BUZZER_PIN = 12;  // ACTIVE buzzer assumed (on/off only); see ARCHITECTURE.md
constexpr uint32_t DEFAULT_UNLOCK_MS = 4000;
constexpr uint32_t MIN_UNLOCK_MS = 4000;
constexpr uint32_t MAX_UNLOCK_MS = 30000;
constexpr uint32_t LOCKOUT_MS = 30000;
constexpr uint8_t MAX_ATTEMPTS = 3;
constexpr uint8_t PIN_LENGTH = 4;
constexpr uint8_t LINE_MAX = 143;       // worst-case valid UNLOCK line is 129 bytes (see firmware_constraints.test.js)
constexpr uint8_t COMMAND_ID_MAX = 32;  // Firebase push keys are 20 chars
constexpr uint8_t DEDUP_SLOTS = 16;

const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {{'1','2','3','A'},{'4','5','6','B'},{'7','8','9','C'},{'*','0','#','D'}};
byte rowPins[ROWS] = {2,3,4,5}, colPins[COLS] = {6,7,8,9};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
Servo lockServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Provision a salted SHA-256 hash (lowercase hex, exactly 64 chars) and salt at build time.
// Both live in flash. The placeholder below is not valid hex, so it can never match.
// No plaintext PIN is stored or logged.
const char PIN_HASH_HEX[] PROGMEM = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
const char PIN_SALT[] PROGMEM = "SET_RANDOM_SALT_AT_BUILD";

char entered[PIN_LENGTH + 1];
uint8_t enteredLen = 0;
char lineBuf[LINE_MAX + 1];
uint8_t lineLen = 0;
bool lineOverflow = false;
uint32_t unlockAt = 0, lockoutUntil = 0, buzzerOffAt = 0;
uint32_t unlockDurationMs = DEFAULT_UNLOCK_MS;
uint8_t failedAttempts = 0;
bool isLocked = true;
bool lastIrState = false;
uint32_t recentCommandHashes[DEDUP_SLOTS];  // FNV-1a hashes; 32-bit collision risk documented
uint8_t recentCommandCursor = 0;

// Non-blocking buzzer: no tone()/Timer2 ISR, so SoftwareSerial RX timing is not disturbed.
void beep(uint16_t ms) {
  digitalWrite(BUZZER_PIN, HIGH);
  buzzerOffAt = millis() + ms;
}

uint32_t fnv1a(const char *s) {
  uint32_t h = 2166136261UL;
  while (*s) { h ^= (uint8_t)*s++; h *= 16777619UL; }
  return h;
}

bool validCommandId(const char *id) {
  uint8_t n = 0;
  for (; id[n]; ++n) {
    const char c = id[n];
    const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                    (c >= 'A' && c <= 'Z') || c == '-' || c == '_';
    if (!ok || n >= COMMAND_ID_MAX) return false;
  }
  return n > 0;
}

uint8_t hexNibble(char c) { return c <= '9' ? c - '0' : c - 'a' + 10; }

// Constant-time comparison of salted SHA-256(pin) with the provisioned hash.
bool pinMatches() {
  if (enteredLen != PIN_LENGTH) return false;
  SHA256 sha;
  sha.reset();
  for (uint8_t i = 0; i < sizeof(PIN_SALT) - 1; ++i) {
    const uint8_t b = pgm_read_byte(PIN_SALT + i);
    sha.update(&b, 1);
  }
  const uint8_t colon = ':';
  sha.update(&colon, 1);
  sha.update((const uint8_t *)entered, PIN_LENGTH);
  uint8_t digest[32];
  sha.finalize(digest, sizeof(digest));
  uint8_t diff = 0;
  for (uint8_t i = 0; i < 32; ++i) {
    const char hi = (char)pgm_read_byte(PIN_HASH_HEX + 2 * i);
    const char lo = (char)pgm_read_byte(PIN_HASH_HEX + 2 * i + 1);
    const bool hex = ((hi >= '0' && hi <= '9') || (hi >= 'a' && hi <= 'f')) &&
                     ((lo >= '0' && lo <= '9') || (lo >= 'a' && lo <= 'f'));
    diff |= hex ? (uint8_t)(digest[i] ^ ((hexNibble(hi) << 4) | hexNibble(lo))) : 0xFF;
  }
  memset(digest, 0, sizeof(digest));
  return diff == 0;
}

void emitEvent(const __FlashStringHelper *event, const __FlashStringHelper *source) {
  link.print(F("{\"type\":\"EVENT\",\"event\":\""));
  link.print(event);
  link.print(F("\",\"source\":\""));
  link.print(source);
  link.println(F("\"}"));
}

void sendNack(const __FlashStringHelper *reason, const char *id) {
  link.print(F("{\"type\":\"nack\""));
  if (id) { link.print(F(",\"commandId\":\"")); link.print(id); link.print('"'); }
  link.print(F(",\"reason\":\""));
  link.print(reason);
  link.println(F("\"}"));
}

void sendAck(const char *id, const char *status) {
  link.print(F("{\"type\":\"ack\",\"commandId\":\""));
  link.print(id);
  link.print(F("\",\"status\":\""));
  link.print(status);
  link.println(F("\"}"));
}

void setDoor(bool locked, const __FlashStringHelper *source) {
  const bool changed = isLocked != locked;
  isLocked = locked;
  lockServo.write(locked ? 0 : 90);
  lcd.clear();
  lcd.print(locked ? F("DOOR LOCKED") : F("DOOR UNLOCKED"));
  beep(100);
  link.print(F("{\"type\":\"STATE\",\"door\":\""));
  link.print(locked ? F("locked") : F("unlocked"));
  link.print(F("\",\"source\":\""));
  link.print(source);
  link.println(F("\"}"));
  if (locked && changed) emitEvent(F("automaticRelock"), source);
  unlockAt = locked ? 0 : millis() + unlockDurationMs;
}

bool seenCommand(uint32_t h) {
  for (uint8_t i = 0; i < DEDUP_SLOTS; ++i) if (recentCommandHashes[i] == h) return true;
  return false;
}

void rejectPin() {
  ++failedAttempts; beep(250); lcd.clear(); lcd.print(F("ACCESS DENIED"));
  emitEvent(F("accessDenied"), F("keypad"));
  if (failedAttempts >= MAX_ATTEMPTS) {
    failedAttempts = 0; lockoutUntil = millis() + LOCKOUT_MS;
    lcd.clear(); lcd.print(F("SECURITY LOCKOUT")); beep(800);
    emitEvent(F("securityLockout"), F("keypad"));
  }
}

// Parses in place: strings point into lineBuf, so the document only holds object slots.
void handleUart(char *line) {
  StaticJsonDocument<128> input;
  if (deserializeJson(input, line)) {
    sendNack(F("malformed-command"), nullptr); return;
  }
  const char *type = input["type"] | "";
  if (strcmp(type, "PING") == 0) {
    const char *pingId = input["pingId"] | "";
    if (!validCommandId(pingId)) {
      sendNack(F("malformed-command"), nullptr); return;
    }
    link.print(F("{\"type\":\"PONG\",\"pingId\":\""));
    link.print(pingId);
    link.print(F("\",\"door\":\""));
    link.print(isLocked ? F("locked") : F("unlocked"));
    link.println(F("\"}"));
    return;
  }
  if (strcmp(type, "UNLOCK") != 0 ||
      !input["commandId"].is<const char *>() ||
      !input["ttlMs"].is<uint32_t>()) {
    sendNack(F("malformed-command"), nullptr); return;
  }
  const char *id = input["commandId"];
  if (!validCommandId(id)) { sendNack(F("malformed-command"), nullptr); return; }
  const uint32_t ttl = input["ttlMs"].as<uint32_t>();
  const uint32_t requestedDuration = input["unlockDurationMs"] | DEFAULT_UNLOCK_MS;
  const uint32_t hash = fnv1a(id);
  const bool duplicate = seenCommand(hash);
  if (duplicate) { sendNack(F("duplicate"), id); return; }
  if (ttl == 0) { sendNack(F("expired"), id); return; }
  if (ttl > 60000 || requestedDuration < MIN_UNLOCK_MS ||
      requestedDuration > MAX_UNLOCK_MS || requestedDuration % 1000 != 0) {
    sendNack(F("malformed-command"), id); return;
  }
  unlockDurationMs = requestedDuration;
  recentCommandHashes[recentCommandCursor] = hash;
  recentCommandCursor = (recentCommandCursor + 1) % DEDUP_SLOTS;
  sendAck(id, "accepted");
  setDoor(false, F("remote"));
  sendAck(id, "executed");
}

void pollLink() {
  if (link.overflow()) { lineLen = 0; lineOverflow = true; }
  while (link.available()) {
    const char c = (char)link.read();
    if (c == '\n') {
      if (lineOverflow) sendNack(F("malformed-command"), nullptr);
      else { lineBuf[lineLen] = '\0'; handleUart(lineBuf); }
      lineLen = 0; lineOverflow = false;
    } else if (c == '\r') {
      continue;
    } else if (lineLen < LINE_MAX) {
      lineBuf[lineLen++] = c;
    } else {
      lineOverflow = true;
    }
  }
}

void setup() {
  link.begin(LINK_BAUD); pinMode(IR_PIN, INPUT); pinMode(BUZZER_PIN, OUTPUT);
  lockServo.attach(SERVO_PIN); lcd.init(); lcd.backlight(); setDoor(true, F("boot"));
  lcd.clear(); lcd.print(F("READY - ENTER PIN"));
}

void loop() {
  const uint32_t now = millis();
  if (buzzerOffAt && (int32_t)(now - buzzerOffAt) >= 0) { digitalWrite(BUZZER_PIN, LOW); buzzerOffAt = 0; }
  if (unlockAt && (int32_t)(now - unlockAt) >= 0) setDoor(true, F("timer"));
  if (lockoutUntil && (int32_t)(now - lockoutUntil) >= 0) {
    lockoutUntil = 0; failedAttempts = 0; lcd.clear(); lcd.print(F("READY - ENTER PIN"));
  }
  const bool irState = digitalRead(IR_PIN) == HIGH;
  if (irState != lastIrState) { lastIrState = irState; emitEvent(F("irProximity"), F("sensor")); }
  pollLink();
  if (!lockoutUntil) {
    const char key = keypad.getKey();
    if (key == '#') {
      if (pinMatches()) {
        failedAttempts = 0; emitEvent(F("accessGranted"), F("keypad")); setDoor(false, F("keypad"));
      } else rejectPin();
      enteredLen = 0; entered[0] = '\0';
    } else if (key == '*') { enteredLen = 0; entered[0] = '\0'; }
    else if (key && key >= '0' && key <= '9' && enteredLen < PIN_LENGTH) {
      entered[enteredLen++] = key; entered[enteredLen] = '\0';
    }
  }
}
