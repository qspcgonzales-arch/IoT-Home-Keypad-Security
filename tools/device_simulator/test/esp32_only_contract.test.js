const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');

const root = path.resolve(__dirname, '..', '..', '..');
const firmwarePath = path.join(root, 'hardware', 'esp32', 'esp32_only', 'src', 'main.cpp');
const platformioPath = path.join(root, 'hardware', 'esp32', 'esp32_only', 'platformio.ini');
const acceptancePath = path.join(root, 'hardware', 'docs', 'ESP32_ONLY_ACCEPTANCE.md');
const firmware = fs.readFileSync(firmwarePath, 'utf8');
const platformio = fs.readFileSync(platformioPath, 'utf8');
const acceptance = fs.readFileSync(acceptancePath, 'utf8');

test('ESP32-only firmware uses the reviewed GPIO map and avoids GPIO12', () => {
  for (const assignment of [
    'rowPins[ROWS] = {32, 33, 25, 26}',
    'colPins[COLS] = {27, 14, 23, 13}',
    'BUZZER_PIN = 19',
    'SERVO_PIN = 18',
    'SENSOR_PIN = 34',
    'LCD_SDA_PIN = 21',
    'LCD_SCL_PIN = 22',
  ]) {
    assert.ok(firmware.includes(assignment), assignment);
  }
  assert.doesNotMatch(firmware, /\bGPIO12\b|\b12\s*,/);
});

test('standalone diagnostic target excludes networking and servo actuation', () => {
  assert.match(platformio, /\[env:esp32dev_standalone\][\s\S]*?-D SERVO_TEST_BUILD=0/);
  assert.match(platformio, /board_build\.partitions = ota_nofs_4MB\.csv/);
  assert.doesNotMatch(firmware, /#include\s*[<"](?:WiFi|Firebase|HTTPClient)/i);
  assert.doesNotMatch(firmware, /WiFi\.begin|FirebaseAuth|FirebaseClient|HTTPClient/i);
  assert.match(firmware, /pinMode\(SERVO_PIN, OUTPUT\);\s*digitalWrite\(SERVO_PIN, LOW\);/);
  const setupBody = firmware.match(/void setup\(\) \{([\s\S]*?)\n\}/)[1];
  assert.doesNotMatch(setupBody, /lockServo\.(?:attach|write)/);
  assert.match(
    firmware,
    /void armServo\(\)[\s\S]*?if \(!allPeripheralTestsPassed\(\)\)[\s\S]*?servoArmed = true;/,
  );
  const armBody = firmware.match(/void armServo\(\) \{([\s\S]*?)\n\}/)[1];
  assert.doesNotMatch(armBody, /lockServo\.attach/);
  assert.match(
    firmware,
    /bool attachServoForUnlock\(\)[\s\S]*?lockServo\.attach/,
  );
  assert.match(firmware, /activeTest != ComponentTest::None/);
});

test('servo arming requires four confirmed local component checks', () => {
  assert.match(firmware, /return keypadPassed && lcdPassed && sensorPassed && buzzerPassed;/);
  assert.match(firmware, /if \(!allPeripheralTestsPassed\(\)\)/);
  for (const component of ['Keypad', 'Lcd', 'Sensor', 'Buzzer']) {
    assert.ok(firmware.includes(`ComponentTest::${component}`), component);
  }
  assert.match(acceptance, /gpio23/i);
  assert.match(acceptance, /servo power/i);
});

test('PIN verifier uses salted SHA-256, constant-time comparison, and redacted output', () => {
  assert.match(firmware, /mbedtls_sha256/);
  assert.match(firmware, /difference \|= static_cast<uint8_t>\(digest\[i\] \^ expected\)/);
  assert.match(firmware, /secureZero\(enteredPin, sizeof\(enteredPin\)\)/);
  assert.doesNotMatch(firmware, /Serial\.(?:print|printf)\([^;\n]*enteredPin/);
  assert.match(firmware, /ESP32_PIN_HASH_HEX/);
});

test('GPIO34 remains input-only without an internal pull resistor', () => {
  assert.match(firmware, /pinMode\(SENSOR_PIN, INPUT\)/);
  assert.doesNotMatch(firmware, /pinMode\(SENSOR_PIN,\s*INPUT_PULL(?:UP|DOWN)\)/);
  assert.match(acceptance, /defined 3\.3 V-compatible level/i);
});
