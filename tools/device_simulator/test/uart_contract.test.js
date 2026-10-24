// Static contract checks between the Uno firmware, ESP32 firmware, and the
// documented UART protocol. These read source text only: they do NOT prove any
// physical UART, servo, keypad or sensor behaviour.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const root = path.join(__dirname, '..', '..', '..');
const read = (p) => fs.readFileSync(path.join(root, p), 'utf8');
const uno = read('hardware/arduino/smart_home_lock/smart_home_lock.ino');
const esp = read('hardware/esp32/smart_home_gateway/src/main.cpp');
const espClient = read('hardware/esp32/smart_home_gateway/src/firebase_rest_client.cpp');
const proto = read('hardware/docs/uart_protocol.md');
const sim = read('tools/device_simulator/device_simulator.js');

test('baud rate is 9600 on both ends and in the protocol doc', () => {
  assert.match(uno, /\.begin\(9600\)|LINK_BAUD\s*=\s*9600/);
  assert.match(esp, /9600/);
  assert.match(proto, /9600/);
});

test('pin assignments: Uno A2(RX)/A3(TX), ESP32 GPIO16(RX2)/GPIO17(TX2)', () => {
  assert.match(uno, /LINK_RX_PIN\s*=\s*A2/);
  assert.match(uno, /LINK_TX_PIN\s*=\s*A3/);
  assert.match(esp, /UNO_RX_PIN\s*=\s*16/);
  assert.match(esp, /UNO_TX_PIN\s*=\s*17/);
  assert.match(proto, /A2/);
  assert.match(proto, /GPIO16|GPIO 16/);
});

test('command-id rule [A-Za-z0-9_-], max 32 is enforced on both firmwares', () => {
  assert.match(uno, /COMMAND_ID_MAX\s*=\s*32/);
  assert.match(esp, /length\(\) > 32/);
  assert.match(esp, /'_'/);
  assert.match(esp, /'-'/);
});

test('Uno nack reasons use the documented set only', () => {
  const reasons = [...uno.matchAll(/sendNack\(F\("([^"]+)"\)/g)].map((m) => m[1]);
  const allowed = new Set(['malformed-command', 'duplicate', 'expired']);
  assert.ok(reasons.length > 0);
  for (const r of reasons) assert.ok(allowed.has(r), `undocumented reason ${r}`);
  for (const r of allowed) assert.match(proto, new RegExp(r));
});

test('Uno range limits match ESP32 limits (ttl <= 60000 ms, duration 4-30 s whole seconds)', () => {
  assert.match(uno, /ttl > 60000/);
  assert.match(uno, /requestedDuration % 1000 != 0/);
  assert.match(esp, /60000/);
});

test('UART presence and execution acknowledgements match the firmware contract', () => {
  assert.match(uno, /PONG/);
  assert.match(esp, /PING/);
  assert.match(uno, /sendAck\(id, "accepted"\)[\s\S]*setDoor\(false, F\("remote"\)\)[\s\S]*sendAck\(id, "executed"\)/);
  assert.match(espClient, /status.*executed/);
  assert.match(proto, /PONG/);
  assert.match(proto, /"status":"executed"/);
});

test('simulator command contract: pending -> accepted -> completed, TTL <= 60 s', () => {
  assert.match(sim, /status:\s*'accepted'/);
  assert.match(sim, /status:\s*'completed'/);
  assert.match(sim, /60_?000/);
});

test('ESP32 firmware contains no admin credentials or service-account material', () => {
  assert.doesNotMatch(esp, /private_key|service_account|BEGIN PRIVATE KEY/i);
});
