const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const ino = fs.readFileSync(
  path.join(__dirname, '..', '..', '..', 'hardware', 'arduino', 'smart_home_lock', 'smart_home_lock.ino'),
  'utf8',
);
const constant = (name) => Number(new RegExp(`${name}\\s*=\\s*(\\d+)`).exec(ino)[1]);

// Host-side static checks only. They do not prove behaviour on an Uno.
test('worst-case UNLOCK line fits the Uno line buffer and SoftwareSerial window', () => {
  const line = JSON.stringify({
    type: 'UNLOCK',
    commandId: 'x'.repeat(constant('COMMAND_ID_MAX')),
    expiresAt: 9999999999999,
    ttlMs: 60000,
    unlockDurationMs: 30000,
  });
  assert.ok(line.length <= constant('LINE_MAX'), `line is ${line.length} bytes`);
  // At 9600 baud a byte takes ~1.04 ms; SoftwareSerial RX holds 64 bytes.
  const msToFillRxBuffer = (64 * 10 * 1000) / 9600;
  assert.ok(msToFillRxBuffer > 60 && msToFillRxBuffer < 70);
  assert.ok(line.length > 64, 'a worst-case line exceeds the 64-byte RX buffer, so loop() must drain promptly');
});

test('typical Firebase push-key UNLOCK line fits the buffer', () => {
  const line = JSON.stringify({
    type: 'UNLOCK',
    commandId: '-OabcdefghijklmnopQR',
    expiresAt: 1760000060000,
    ttlMs: 60000,
    unlockDurationMs: 4000,
  });
  assert.ok(line.length <= constant('LINE_MAX'));
});

test('firmware avoids heap Strings, tone() and unbounded blocking delays', () => {
  const code = ino.replace(/\/\/.*$/gm, '');
  assert.doesNotMatch(code, /\bString\b/);
  assert.doesNotMatch(code, /\btone\s*\(/);
  assert.doesNotMatch(code, /\bdelay\s*\(/);
});

test('firmware retains security checks', () => {
  assert.match(ino, /ttl == 0\) \{ sendNack\(F\("expired"\), id\)[\s\S]*ttl > 60000/);
  assert.match(ino, /seenCommand\(hash\)/);
  assert.match(ino, /MAX_ATTEMPTS/);
  assert.match(ino, /LOCKOUT_MS/);
  assert.match(ino, /pinMatches\(\)/);
});
