'use strict';

const assert = require('node:assert/strict');
const test = require('node:test');
const { VirtualArduino, pinDigest, LOCKOUT_MS, UNLOCK_MS } = require('../../../hardware/virtual/arduino_simulator');
const { VirtualEsp32Gateway } = require('../../../hardware/virtual/esp32_gateway_simulator');
const { ADMIN_UID } = require('../device_simulator');

class MemoryDb {
  constructor() { this.data = {}; this.listeners = new Map(); this.id = 0; this.fail = false; }
  ref(path = '') { return new Ref(this, path); }
  put(path, value, merge = false) {
    if (this.fail) throw new Error('offline');
    const parts = path.split('/').filter(Boolean); let parent = this.data;
    for (const part of parts.slice(0, -1)) parent = parent[part] ||= {};
    const key = parts.at(-1);
    parent[key] = merge ? { ...(parent[key] || {}), ...value } : value;
  }
  get(path) { return path.split('/').filter(Boolean).reduce((v, k) => v?.[k], this.data); }
}
class Ref {
  constructor(db, path) { this.db = db; this.path = path; this.key = path.split('/').at(-1); }
  set(v) { this.db.put(this.path, v); return Promise.resolve(); }
  update(v) { this.db.put(this.path, v, true); return Promise.resolve(); }
  push() { return new Ref(this.db, `${this.path}/e${++this.db.id}`); }
  on(event, cb) { this.db.listeners.set(this.path, { event, cb }); }
  off() {}
  once() { return Promise.resolve({ val: () => this.db.get(this.path) }); }
}

const SALT = 'test-only-salt';
const TEST_PIN = '2468';
function makeUno(nowRef, events = [], uart = []) {
  return new VirtualArduino({
    pinSalt: SALT, pinHash: pinDigest(TEST_PIN, SALT), now: () => nowRef.value,
    onEvent: (event) => events.push(event), writeUart: (line) => uart.push(JSON.parse(line)),
  });
}
function command(now = 10_000, values = {}) {
  return { type: 'unlock', status: 'pending', requestedBy: ADMIN_UID,
    createdAt: now - 500, expiresAt: now + 20_000, ...values };
}

test('correct PIN unlocks, reports LCD/buzzer state and relocks at four seconds', () => {
  const clock = { value: 10_000 }, events = [], uno = makeUno(clock, events);
  assert.equal(uno.enterPin(TEST_PIN).accepted, true);
  assert.equal(uno.locked, false);
  assert.equal(uno.lcdMessage, 'DOOR UNLOCKED');
  clock.value += UNLOCK_MS; uno.tick();
  assert.equal(uno.locked, true);
  assert.ok(events.some((e) => e.type === 'automaticRelock'));
});

test('configured duration applies to local PIN entry and validates bounds', () => {
  const clock = { value: 10_000 }, uno = makeUno(clock);
  uno.setUnlockDurationMs(8_000);
  assert.equal(uno.enterPin(TEST_PIN).accepted, true);
  assert.equal(uno.relockAt, clock.value + 8_000);
  assert.throws(() => uno.setUnlockDurationMs(3_000), /4 to 30 seconds/);
  assert.throws(() => uno.setUnlockDurationMs(8_500), /4 to 30 seconds/);
});

test('UART command duration is validated and controls the relock deadline', () => {
  const clock = { value: 50_000 }, uno = makeUno(clock);
  const valid = { type: 'UNLOCK', commandId: 'duration-test', expiresAt: 60_000, ttlMs: 10_000, unlockDurationMs: 12_000 };
  assert.equal(uno.handleUartLine(JSON.stringify(valid)).accepted, true);
  assert.equal(uno.relockAt, clock.value + 12_000);
  const invalid = { ...valid, commandId: 'bad-duration', unlockDurationMs: 31_000 };
  assert.equal(uno.handleUartLine(JSON.stringify(invalid)).accepted, false);
});

test('invalid PIN is rejected and never reveals the submitted PIN', () => {
  const clock = { value: 1 }, events = [], uno = makeUno(clock, events);
  assert.equal(uno.enterPin('9999').accepted, false);
  assert.equal(uno.locked, true);
  assert.equal(events[0].type, 'accessDenied');
  assert.equal(JSON.stringify(events).includes('9999'), false);
});

test('three invalid PIN attempts trigger a 30-second keypad lockout', () => {
  const clock = { value: 1 }, uno = makeUno(clock);
  uno.enterPin('1111'); uno.enterPin('1111'); uno.enterPin('1111');
  assert.equal(uno.lockoutUntil, clock.value + LOCKOUT_MS);
  assert.equal(uno.enterPin(TEST_PIN).reason, 'lockout');
});

test('lockout expires without blocking local entry', () => {
  const clock = { value: 1 }, uno = makeUno(clock);
  uno.enterPin('1111'); uno.enterPin('1111'); uno.enterPin('1111');
  clock.value += LOCKOUT_MS; uno.tick();
  assert.equal(uno.lockoutUntil, 0);
  assert.equal(uno.enterPin(TEST_PIN).accepted, true);
});

test('UART accepts a valid unexpired command and rejects duplicates and expired IDs', () => {
  const clock = { value: 50_000 }, uart = [], uno = makeUno(clock, [], uart);
  const valid = JSON.stringify({
    type: 'UNLOCK',
    commandId: 'a',
    expiresAt: 60_000,
    ttlMs: 10_000,
    unlockDurationMs: 4_000,
  });
  assert.equal(uno.handleUartLine(valid).accepted, true);
  assert.equal(uno.handleUartLine(valid).reason, 'duplicate');
  assert.equal(uno.handleUartLine(JSON.stringify({
    type: 'UNLOCK',
    commandId: 'b',
    expiresAt: 49_999,
    ttlMs: 0,
  })).reason, 'expired');
  assert.equal(uart[0].status, 'accepted');
  assert.deepEqual(uart[1], {
    type: 'STATE',
    door: 'unlocked',
    source: 'remote',
  });
  assert.equal(uart[2].status, 'executed');
});

test('UART ping verifies Arduino presence and current logical door state', () => {
  const clock = { value: 50_000 }, uart = [], uno = makeUno(clock, [], uart);
  assert.equal(uno.handleUartLine(JSON.stringify({
    type: 'PING',
    pingId: 'boot-1',
  })).accepted, true);
  assert.deepEqual(uart[0], {
    type: 'PONG',
    pingId: 'boot-1',
    door: 'locked',
  });
});

test('IR proximity transitions are emitted once per state change', () => {
  const clock = { value: 10 }, events = [], uno = makeUno(clock, events);
  uno.detectProximity(true); uno.detectProximity(true); uno.detectProximity(false);
  assert.equal(events.filter((e) => e.type === 'irProximity').length, 2);
});

test('unauthorized Firebase command is rejected without actuation', async () => {
  const clock = { value: 10_000 }, db = new MemoryDb(), uno = makeUno(clock);
  const gateway = new VirtualEsp32Gateway({ database: db, arduino: uno, now: () => clock.value, log() {} });
  gateway.running = true; gateway.online = true;
  const result = await gateway.handleCommand('bad', command(clock.value, { requestedBy: 'attacker' }));
  assert.equal(result.status, 'failed');
  assert.equal(uno.locked, true);
});

test('remote Flutter command follows accepted, UART acknowledgement, four-second relock and audit sync', async () => {
  const clock = { value: 10_000 }, db = new MemoryDb(), events = [];
  const uno = makeUno(clock, events);
  const gateway = new VirtualEsp32Gateway({ database: db, arduino: uno, now: () => clock.value,
    sleep: async () => {}, log() {}, serverTimestamp: clock.value });
  gateway.running = true; gateway.online = true;
  db.put('commands/cmd-remote', command(clock.value));
  const result = await gateway.handleCommand('cmd-remote', command(clock.value));
  assert.equal(result.status, 'accepted');
  assert.equal(db.get('commands/cmd-remote/status'), 'completed');
  assert.equal(uno.locked, false);
  assert.equal(db.get('devices/frontDoor/isLocked'), false);
  clock.value += UNLOCK_MS; uno.tick();
  await new Promise((resolve) => setImmediate(resolve));
  assert.equal(db.get('commands/cmd-remote/status'), 'completed');
  assert.equal(db.get('devices/frontDoor/isLocked'), true);
  assert.deepEqual(Object.values(db.get('auditLogs')).map((e) => e.type), ['remoteUnlock', 'automaticRelock']);
});

test('gateway applies the current database setting to subsequent remote commands', async () => {
  const clock = { value: 10_000 }, db = new MemoryDb(), uno = makeUno(clock);
  db.put('settings/frontDoor/unlockDurationSeconds', 9);
  const gateway = new VirtualEsp32Gateway({ database: db, arduino: uno, now: () => clock.value, log() {} });
  gateway.running = true; gateway.online = true;
  assert.equal((await gateway.handleCommand('cmd-custom', command(clock.value))).status, 'accepted');
  assert.equal(uno.relockAt, clock.value + 9_000);
});

test('duplicate remote command ID does not execute twice', async () => {
  const clock = { value: 10_000 }, db = new MemoryDb(), uno = makeUno(clock);
  const gateway = new VirtualEsp32Gateway({ database: db, arduino: uno, now: () => clock.value, log() {} });
  gateway.running = true; gateway.online = true;
  const record = command(clock.value);
  assert.equal((await gateway.handleCommand('same', record)).status, 'accepted');
  assert.equal((await gateway.handleCommand('same', record)).status, 'duplicate');
});

test('expired command is rejected on receipt and never actuates servo', async () => {
  const clock = { value: 10_000 }, db = new MemoryDb(), uno = makeUno(clock);
  const gateway = new VirtualEsp32Gateway({ database: db, arduino: uno, now: () => clock.value, log() {} });
  gateway.running = true; gateway.online = true;
  const result = await gateway.handleCommand('expired', command(clock.value, { expiresAt: 9_999 }));
  assert.equal(result.status, 'expired'); assert.equal(uno.locked, true);
});

test('Wi-Fi outage leaves PIN entry local and syncs events after reconnect', async () => {
  const clock = { value: 10_000 }, db = new MemoryDb(), uno = makeUno(clock);
  const gateway = new VirtualEsp32Gateway({ database: db, arduino: uno, now: () => clock.value, log() {} });
  gateway.running = true; gateway.online = true;
  await gateway.setWifiConnected(false);
  assert.equal(uno.enterPin(TEST_PIN).accepted, true);
  assert.equal(uno.locked, false);
  assert.equal(gateway.hardwareEvents.some((e) => e.type === 'accessGranted'), true);
  clock.value += UNLOCK_MS; uno.tick();
  await gateway.setWifiConnected(true);
  assert.equal(db.get('devices/frontDoor/isLocked'), true);
  assert.deepEqual(Object.values(db.get('auditLogs')).map((event) => event.type),
    ['accessGranted', 'automaticRelock']);
});

test('pending command queued during Wi-Fi outage expires and is rejected on reconnect', async () => {
  const clock = { value: 10_000 }, db = new MemoryDb(), uno = makeUno(clock);
  const gateway = new VirtualEsp32Gateway({ database: db, arduino: uno, now: () => clock.value, log() {} });
  gateway.running = true; gateway.online = true;
  const record = command(clock.value, { expiresAt: 10_500 });
  db.put('commands/stale', record);
  await gateway.setWifiConnected(false);
  assert.equal((await gateway.handleCommand('stale', record)).status, 'queued-offline');
  clock.value = 11_000;
  await gateway.setWifiConnected(true);
  assert.equal(db.get('commands/stale/status'), 'expired');
  assert.equal(uno.locked, true);
});

test('Firebase write failure does not actuate a remote command', async () => {
  const clock = { value: 10_000 }, db = new MemoryDb(), uno = makeUno(clock);
  const gateway = new VirtualEsp32Gateway({ database: db, arduino: uno, now: () => clock.value, log() {} });
  gateway.running = true; gateway.online = true;
  db.fail = true;
  await assert.rejects(() => gateway.handleCommand('db-error', command(clock.value)));
  assert.equal(uno.locked, true);
});
