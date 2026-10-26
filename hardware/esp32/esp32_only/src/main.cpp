#include <Arduino.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <mbedtls/sha256.h>

#ifndef SERVO_TEST_BUILD
#define SERVO_TEST_BUILD 0
#endif

#ifndef ESP32_CLOUD_DIAGNOSTICS
#define ESP32_CLOUD_DIAGNOSTICS 0
#endif

#if SERVO_TEST_BUILD
#include <ESP32Servo.h>
#endif

#if ESP32_CLOUD_DIAGNOSTICS
#include "firebase_diagnostics.h"
#endif

#if __has_include("pin_config.h")
#include "pin_config.h"
#endif

#ifndef ESP32_PIN_HASH_HEX
#define ESP32_PIN_HASH_HEX ""
#endif

#ifndef ESP32_PIN_SALT
#define ESP32_PIN_SALT ""
#endif

constexpr uint8_t ROWS = 4;
constexpr uint8_t COLS = 4;
byte rowPins[ROWS] = {32, 33, 25, 26};
byte colPins[COLS] = {27, 14, 23, 13};
constexpr uint8_t BUZZER_PIN = 19;
constexpr uint8_t SERVO_PIN = 18;
constexpr uint8_t SENSOR_PIN = 34;
constexpr uint8_t LCD_SDA_PIN = 21;
constexpr uint8_t LCD_SCL_PIN = 22;
constexpr uint8_t PIN_LENGTH = 4;
constexpr uint8_t MAX_ATTEMPTS = 3;
constexpr uint32_t DEFAULT_UNLOCK_MS = 4000;
constexpr uint32_t LOCKOUT_MS = 30000;
constexpr uint32_t SENSOR_DEBOUNCE_MS = 30;
constexpr uint16_t LOCKED_ANGLE = 0;
constexpr uint16_t UNLOCKED_ANGLE = 90;

char keyMap[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'},
};
Keypad keypad = Keypad(makeKeymap(keyMap), rowPins, colPins, ROWS, COLS);
LiquidCrystal_I2C lcd(0x27, 16, 2);

#if SERVO_TEST_BUILD
Servo lockServo;
#endif

#if ESP32_CLOUD_DIAGNOSTICS
FirebaseDiagnostics cloudDiagnostics;
#endif

enum class ComponentTest : uint8_t { None, Keypad, Lcd, Sensor, Buzzer };

char enteredPin[PIN_LENGTH + 1] = {};
uint8_t enteredPinLength = 0;
uint8_t failedAttempts = 0;
uint32_t lockoutUntil = 0;
uint32_t unlockAt = 0;
bool doorLocked = true;
bool keypadPassed = false;
bool lcdPassed = false;
bool sensorPassed = false;
bool buzzerPassed = false;
bool servoArmed = false;
ComponentTest activeTest = ComponentTest::None;
uint16_t keypadSeenMask = 0;
bool sensorSawLow = false;
bool sensorSawHigh = false;
bool buzzerTestRunning = false;
uint8_t buzzerTestStep = 0;
uint32_t buzzerTestDeadline = 0;
int sensorCandidate = HIGH;
int sensorStable = HIGH;
uint32_t sensorCandidateSince = 0;
char serialLine[48] = {};
uint8_t serialLineLength = 0;
bool serialLineOverflow = false;

void reportSecurityEvent(const String &type, const String &description) {
#if ESP32_CLOUD_DIAGNOSTICS
  cloudDiagnostics.recordEvent(type, description);
#else
  (void)type;
  (void)description;
#endif
}

void secureZero(void *memory, size_t length) {
  volatile uint8_t *bytes = static_cast<volatile uint8_t *>(memory);
  while (length-- > 0) *bytes++ = 0;
}

bool reached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

bool hasProvisionedPin() {
  if (strlen(ESP32_PIN_HASH_HEX) != 64 || strlen(ESP32_PIN_SALT) == 0 ||
      strlen(ESP32_PIN_SALT) > 48) {
    return false;
  }
  for (uint8_t i = 0; i < 64; ++i) {
    const char c = ESP32_PIN_HASH_HEX[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}

uint8_t hexNibble(char value) {
  return value <= '9' ? static_cast<uint8_t>(value - '0')
                      : static_cast<uint8_t>(value - 'a' + 10);
}

bool pinMatches() {
  if (enteredPinLength != PIN_LENGTH || !hasProvisionedPin()) return false;

  char hashInput[55] = {};
  const size_t saltLength = strlen(ESP32_PIN_SALT);
  memcpy(hashInput, ESP32_PIN_SALT, saltLength);
  hashInput[saltLength] = ':';
  memcpy(hashInput + saltLength + 1, enteredPin, PIN_LENGTH);

  uint8_t digest[32] = {};
  const int result = mbedtls_sha256(
      reinterpret_cast<const unsigned char *>(hashInput),
      saltLength + 1 + PIN_LENGTH, digest, 0);
  secureZero(hashInput, sizeof(hashInput));
  if (result != 0) {
    secureZero(digest, sizeof(digest));
    return false;
  }

  uint8_t difference = 0;
  for (uint8_t i = 0; i < sizeof(digest); ++i) {
    const uint8_t expected =
        static_cast<uint8_t>((hexNibble(ESP32_PIN_HASH_HEX[2 * i]) << 4) |
                             hexNibble(ESP32_PIN_HASH_HEX[2 * i + 1]));
    difference |= static_cast<uint8_t>(digest[i] ^ expected);
  }
  secureZero(digest, sizeof(digest));
  return difference == 0;
}

void clearEnteredPin() {
  secureZero(enteredPin, sizeof(enteredPin));
  enteredPinLength = 0;
}

bool allPeripheralTestsPassed() {
  return keypadPassed && lcdPassed && sensorPassed && buzzerPassed;
}

void printTestStatus() {
  Serial.printf("Peripheral checks: keypad=%s lcd=%s sensor=%s buzzer=%s\n",
                keypadPassed ? "PASS" : "PENDING",
                lcdPassed ? "PASS" : "PENDING",
                sensorPassed ? "PASS" : "PENDING",
                buzzerPassed ? "PASS" : "PENDING");
#if SERVO_TEST_BUILD
  Serial.printf("Servo test build: %s\n", servoArmed ? "ARMED" : "DISARMED");
#else
  Serial.println("Servo actuation: COMPILED OUT");
#endif
}

void showLockedScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(doorLocked ? "LOCKED - ENTER PIN" : "UNLOCKED");
  lcd.setCursor(0, 1);
  for (uint8_t i = 0; i < enteredPinLength; ++i) lcd.print('*');
}

#if SERVO_TEST_BUILD
bool attachServoForUnlock() {
  lockServo.setPeriodHertz(50);
  if (lockServo.attach(SERVO_PIN, 500, 2400) < 0) {
    servoArmed = false;
    Serial.println("Servo attach failed; unlock request was not carried out.");
    return false;
  }
  return true;
}
#endif

bool setDoorLocked(bool locked, uint32_t now) {
  const bool wasLocked = doorLocked;
#if SERVO_TEST_BUILD
  if (!locked && servoArmed && !lockServo.attached() &&
      !attachServoForUnlock()) {
    return false;
  }
#endif
  doorLocked = locked;
  if (locked) {
    unlockAt = 0;
#if SERVO_TEST_BUILD
    if (servoArmed && lockServo.attached()) lockServo.write(LOCKED_ANGLE);
#endif
    Serial.println("Door state: locked");
    if (!wasLocked) {
      reportSecurityEvent("automaticRelock",
                          "ESP32 automatically relocked after local keypad access.");
    }
  } else {
    unlockAt = now + DEFAULT_UNLOCK_MS;
#if SERVO_TEST_BUILD
    if (servoArmed && lockServo.attached()) lockServo.write(UNLOCKED_ANGLE);
    else Serial.println("Logical unlock only; servo remains disarmed.");
#else
    Serial.println("Logical unlock only; servo is not present in this build.");
#endif
    Serial.println("Door state: unlocked; automatic relock timer started");
  }
  showLockedScreen();
  return true;
}

bool lockoutActive(uint32_t now) {
  if (lockoutUntil == 0) return false;
  if (!reached(now, lockoutUntil)) return true;
  lockoutUntil = 0;
  failedAttempts = 0;
  Serial.println("Keypad lockout ended");
  showLockedScreen();
  return false;
}

void denyPin(uint32_t now) {
  clearEnteredPin();
  ++failedAttempts;
  reportSecurityEvent("accessDenied",
                      "ESP32 local keypad authentication failed.");
  digitalWrite(BUZZER_PIN, HIGH);
  delay(120);
  digitalWrite(BUZZER_PIN, LOW);
  Serial.printf("Access denied; attempts remaining: %u\n",
                MAX_ATTEMPTS - failedAttempts);
  if (failedAttempts >= MAX_ATTEMPTS) {
    failedAttempts = 0;
    lockoutUntil = now + LOCKOUT_MS;
    Serial.println("Security lockout started for 30 seconds");
    reportSecurityEvent("securityLockout",
                        "ESP32 local keypad entered a 30-second security lockout.");
  }
  showLockedScreen();
}

void handleEnteredPin(uint32_t now) {
  const bool accepted = pinMatches();
  clearEnteredPin();
  if (!accepted) {
    denyPin(now);
    return;
  }
  failedAttempts = 0;
  Serial.println("PIN accepted; no PIN value recorded");
  reportSecurityEvent("accessGranted",
                      "ESP32 local keypad access granted.");
  if (!setDoorLocked(false, now)) {
    Serial.println("Door remains logically locked.");
  }
}

int keyBitIndex(char key) {
  for (uint8_t row = 0; row < ROWS; ++row) {
    for (uint8_t col = 0; col < COLS; ++col) {
      if (keyMap[row][col] == key) return row * COLS + col;
    }
  }
  return -1;
}

void handleKeypadTestKey(char key) {
  const int index = keyBitIndex(key);
  if (index >= 0) keypadSeenMask |= static_cast<uint16_t>(1U << index);
  Serial.println("Keypad activity recorded; key value withheld.");
}

void handleNormalKey(char key, uint32_t now) {
  if (!doorLocked || lockoutActive(now)) return;
  if (key >= '0' && key <= '9') {
    if (enteredPinLength < PIN_LENGTH) {
      enteredPin[enteredPinLength++] = key;
      lcd.setCursor(0, 1);
      for (uint8_t i = 0; i < enteredPinLength; ++i) lcd.print('*');
    }
    return;
  }
  if (key == '*') {
    clearEnteredPin();
    showLockedScreen();
    return;
  }
  if (key == '#') {
    if (enteredPinLength == PIN_LENGTH) handleEnteredPin(now);
    else clearEnteredPin();
    showLockedScreen();
  }
}

void pollKeypad(uint32_t now) {
  const char key = keypad.getKey();
  if (!key) return;
  if (activeTest == ComponentTest::Keypad) handleKeypadTestKey(key);
  else if (activeTest == ComponentTest::None) handleNormalKey(key, now);
}

void updateSensorTest(uint32_t now) {
  const int reading = digitalRead(SENSOR_PIN);
  if (reading != sensorCandidate) {
    sensorCandidate = reading;
    sensorCandidateSince = now;
  } else if (reading != sensorStable &&
             static_cast<uint32_t>(now - sensorCandidateSince) >=
                 SENSOR_DEBOUNCE_MS) {
    sensorStable = reading;
    if (activeTest == ComponentTest::Sensor) {
      if (sensorStable == LOW) sensorSawLow = true;
      if (sensorStable == HIGH) sensorSawHigh = true;
      Serial.printf("Sensor stable output recorded: %s\n",
                    sensorStable == HIGH ? "HIGH" : "LOW");
    }
  }
}

void updateBuzzerTest(uint32_t now) {
  if (!buzzerTestRunning || !reached(now, buzzerTestDeadline)) return;
  if (buzzerTestStep >= 6) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerTestRunning = false;
    Serial.println("Buzzer pattern finished; listen for three clear beeps.");
    return;
  }
  const bool on = (buzzerTestStep % 2) == 0;
  digitalWrite(BUZZER_PIN, on ? HIGH : LOW);
  buzzerTestDeadline = now + (on ? 100 : 150);
  ++buzzerTestStep;
}

void startComponentTest(ComponentTest test) {
  activeTest = test;
  switch (test) {
    case ComponentTest::Keypad:
      keypadSeenMask = 0;
      Serial.println("Press all 16 keys once; displayed key values are suppressed.");
      break;
    case ComponentTest::Lcd:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("LCD TEST ABCD");
      lcd.setCursor(0, 1);
      lcd.print("1234567890 *#");
      Serial.println("LCD test pattern displayed; confirm only if legible.");
      break;
    case ComponentTest::Sensor:
      sensorSawLow = sensorStable == LOW;
      sensorSawHigh = sensorStable == HIGH;
      Serial.println("Move the target across the sensor; observe both output states.");
      break;
    case ComponentTest::Buzzer:
      buzzerTestStep = 0;
      buzzerTestDeadline = millis();
      buzzerTestRunning = true;
      Serial.println("Playing a short three-beep test.");
      break;
    case ComponentTest::None:
      break;
  }
}

void confirmComponentTest(ComponentTest test) {
  if (test != activeTest) {
    Serial.println("Confirmation refused: start that component test first.");
    return;
  }
  bool passed = false;
  switch (test) {
    case ComponentTest::Keypad:
      passed = keypadSeenMask == 0xFFFF;
      if (passed) keypadPassed = true;
      break;
    case ComponentTest::Lcd:
      lcdPassed = true;
      passed = true;
      break;
    case ComponentTest::Sensor:
      passed = sensorSawLow && sensorSawHigh;
      if (passed) sensorPassed = true;
      break;
    case ComponentTest::Buzzer:
      passed = !buzzerTestRunning && buzzerTestStep >= 6;
      if (passed) buzzerPassed = true;
      break;
    case ComponentTest::None:
      break;
  }
  Serial.println(passed ? "Component check recorded PASS."
                        : "Component check not complete; inspect and retry.");
  if (passed) activeTest = ComponentTest::None;
  printTestStatus();
}

void armServo() {
#if SERVO_TEST_BUILD
  if (activeTest != ComponentTest::None) {
    Serial.println("Servo arm refused: finish the active component test first.");
    return;
  }
  if (!allPeripheralTestsPassed()) {
    Serial.println("Servo arm refused: all four peripheral checks must pass.");
    return;
  }
  if (servoArmed) {
    Serial.println("Servo test is already armed.");
    return;
  }
  servoArmed = true;
  Serial.println("Servo armed but remains unattached until a valid local PIN.");
#else
  Serial.println("Servo actuation is compiled out of this standalone build.");
#endif
}

void processSerialCommand(char *command) {
  for (char *cursor = command; *cursor; ++cursor) {
    if (*cursor >= 'a' && *cursor <= 'z') *cursor -= ('a' - 'A');
  }
  if (strcmp(command, "TEST KEYPAD") == 0) startComponentTest(ComponentTest::Keypad);
  else if (strcmp(command, "CONFIRM KEYPAD") == 0) confirmComponentTest(ComponentTest::Keypad);
  else if (strcmp(command, "TEST LCD") == 0) startComponentTest(ComponentTest::Lcd);
  else if (strcmp(command, "CONFIRM LCD") == 0) confirmComponentTest(ComponentTest::Lcd);
  else if (strcmp(command, "TEST SENSOR") == 0) startComponentTest(ComponentTest::Sensor);
  else if (strcmp(command, "CONFIRM SENSOR") == 0) confirmComponentTest(ComponentTest::Sensor);
  else if (strcmp(command, "TEST BUZZER") == 0) startComponentTest(ComponentTest::Buzzer);
  else if (strcmp(command, "CONFIRM BUZZER") == 0) confirmComponentTest(ComponentTest::Buzzer);
  else if (strcmp(command, "ARM SERVO") == 0) armServo();
  else if (strcmp(command, "STATUS") == 0) printTestStatus();
  else Serial.println("Unknown command. See ESP32_ONLY_ACCEPTANCE.md.");
}

void pollSerial() {
  while (Serial.available()) {
    const char value = static_cast<char>(Serial.read());
    if (value == '\r') continue;
    if (value == '\n') {
      if (serialLineOverflow) {
        Serial.println("Command rejected: line too long.");
      } else {
        serialLine[serialLineLength] = '\0';
        processSerialCommand(serialLine);
      }
      secureZero(serialLine, sizeof(serialLine));
      serialLineLength = 0;
      serialLineOverflow = false;
    } else if (serialLineLength < sizeof(serialLine) - 1) {
      serialLine[serialLineLength++] = value;
    } else {
      serialLineOverflow = true;
    }
  }
}

void setup() {
  pinMode(SERVO_PIN, OUTPUT);
  digitalWrite(SERVO_PIN, LOW);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(SENSOR_PIN, INPUT);

  Serial.begin(115200);
  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
  lcd.init();
  lcd.backlight();
  sensorCandidate = digitalRead(SENSOR_PIN);
  sensorStable = sensorCandidate;
  sensorCandidateSince = millis();
  if (hasProvisionedPin()) {
    showLockedScreen();
  } else {
    lcd.clear();
    lcd.print("PIN NOT CONFIGURED");
    lcd.setCursor(0, 1);
    lcd.print("Access disabled");
  }

#if ESP32_CLOUD_DIAGNOSTICS
  Serial.println("ESP32-only local diagnostics with staging cloud reporting.");
  Serial.println("Cloud diagnostics profile selected; remote servo actuation is compiled out.");
  cloudDiagnostics.begin();
#else
  Serial.println("ESP32-only local diagnostics; Wi-Fi/Firebase are not included.");
#endif
  Serial.println(hasProvisionedPin()
                     ? "PIN verifier provisioned."
                     : "PIN verifier unavailable; access fails closed.");
  Serial.println("Servo output is disarmed at boot. Run component checks first.");
  printTestStatus();
}

void loop() {
  const uint32_t now = millis();
  pollSerial();
  updateSensorTest(now);
  updateBuzzerTest(now);
  pollKeypad(now);
  lockoutActive(now);
  if (!doorLocked && reached(now, unlockAt)) setDoorLocked(true, now);
#if ESP32_CLOUD_DIAGNOSTICS
  cloudDiagnostics.tick(doorLocked, sensorStable == HIGH);
#endif
  delay(1);
}
