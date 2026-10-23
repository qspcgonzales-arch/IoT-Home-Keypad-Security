'use strict';

const assert = require('node:assert/strict');
const test = require('node:test');

const {
  ADMIN_UID,
  DeviceSimulator,
  validateCommand,
} = require('../device_simulator');

class FakeDatabase {
  constructor() {
    this.data = {};
    this.writes = [];
    this.listeners = new Map();
    this.nextKey = 0;
    this.failWrites = false;
  }

  ref(path = '') {
    return new FakeReference(this, path);
  }

  read(path) {
    return path.split('/').filter(Boolean).reduce(
      (value, segment) => value == null ? undefined : value[segment],
      this.data,
    );
  }

  write(path, value, merge = false) {
    if (this.failWrites) throw new Error('simulated Firebase connection failure');
    const segments = path.split('/').filter(Boolean);
    let parent = this.data;
    for (const segment of segments.slice(0, -1)) {
      parent[segment] ??= {};
      parent = parent[segment];
    }
    const key = segments.at(-1);
    if (merge) parent[key] = { ...(parent[key] || {}), ...value };
    else parent[key] = value;
    this.writes.push({ path, value, merge });
  }
}

class FakeReference {
  constructor(database, path) {
    this.database = database;
    this.path = path.replace(/^\/+|\/+$/g, '');
    this.key = this.path.split('/').at(-1) || null;
  }

  set(value) {
    this.database.write(this.path, value);
    return Promise.resolve();
  }

  update(value) {
    this.database.write(this.path, value, true);
    return Promise.resolve();
  }

  push() {
    this.database.nextKey += 1;
    return new FakeReference(this.database, `${this.path}/event-${this.database.nextKey}`);
  }

  on(event, callback) {
    const callbacks = this.database.listeners.get(this.path) || [];
    callbacks.push({ event, callback });
    this.database.listeners.set(this.path, callbacks);
    return callback;
  }

  off(event, callback) {
    const callbacks = this.database.listeners.get(this.path) || [];
    this.database.listeners.set(
      this.path,
      callbacks.filter((entry) => entry.event !== event || entry.callback !== callback),
    );
  }
}

function makeCommand(overrides = {}, now = 1_000_000) {
  return {
    type: 'unlock',
    status: 'pending',
    requestedBy: ADMIN_UID,
    createdAt: now - 500,
    expiresAt: now + 30_000,
    ...overrides,
  };
}

function makeSimulator(options = {}) {
  const database = options.database || new FakeDatabase();
  const now = options.now || (() => 1_000_000);
  const logs = [];
  const simulator = new DeviceSimulator({
    database,
    now,
    sleep: options.sleep || (async () => {}),
    log: (message) => logs.push(message),
    serverTimestamp: { '.sv': 'timestamp' },
    ...options,
  });
  simulator.running = true;
  simulator.online = true;
  return { simulator, database, logs };
}

test('authorized unlock is acknowledged, audited, held four seconds, and relocked', async () => {
  const { simulator, database } = makeSimulator();
  const command = makeCommand();

  const result = await simulator.handleCommand('cmd-authorized', command);

  assert.deepEqual(result, { status: 'completed' });
  assert.equal(database.read('commands/cmd-authorized/status'), 'completed');
  assert.equal(database.read('devices/frontDoor/isLocked'), true);
  assert.equal(database.read('devices/gateway/isOnline'), undefined);
  assert.deepEqual(
    Object.values(database.read('auditLogs')).map((event) => event.type),
    ['remoteUnlock', 'automaticRelock'],
  );
});

test('unauthorized request is rejected and never unlocks', async () => {
  const { simulator, database } = makeSimulator();
  const result = await simulator.handleCommand(
    'cmd-unauthorized',
    makeCommand({ requestedBy: 'attacker-uid' }),
  );

  assert.equal(result.status, 'failed');
  assert.equal(database.read('devices/frontDoor/isLocked'), undefined);
  assert.equal(database.read('commands/cmd-unauthorized/status'), 'failed');
});

test('expired command is rejected without unlocking', async () => {
  const { simulator, database } = makeSimulator();
  const result = await simulator.handleCommand(
    'cmd-expired',
    makeCommand({ createdAt: 950_000, expiresAt: 999_999 }),
  );

  assert.equal(result.status, 'expired');
  assert.equal(database.read('devices/frontDoor/isLocked'), undefined);
  assert.equal(database.read('commands/cmd-expired/status'), 'expired');
  assert.equal(Object.values(database.read('auditLogs'))[0].type, 'commandExpired');
});

test('same command ID cannot execute twice while it is in flight', async () => {
  let unblock;
  let enteredSleep;
  const sleeping = new Promise((resolve) => { unblock = resolve; });
  const sleepEntered = new Promise((resolve) => { enteredSleep = resolve; });
  const { simulator, database } = makeSimulator({
    sleep: async () => {
      enteredSleep();
      await sleeping;
    },
  });
  const command = makeCommand();
  const firstRun = simulator.handleCommand('cmd-duplicate', command);
  await sleepEntered;
  const duplicate = await simulator.handleCommand('cmd-duplicate', command);
  unblock();
  const first = await firstRun;

  assert.equal(duplicate.status, 'duplicate');
  assert.equal(first.status, 'completed');
  assert.equal(Object.values(database.read('auditLogs')).filter(
    (event) => event.type === 'remoteUnlock',
  ).length, 1);
});

test('separate command is rejected while the device is busy', async () => {
  let unblock;
  let enteredSleep;
  const sleeping = new Promise((resolve) => { unblock = resolve; });
  const sleepEntered = new Promise((resolve) => { enteredSleep = resolve; });
  const { simulator, database } = makeSimulator({
    sleep: async () => {
      enteredSleep();
      await sleeping;
    },
  });
  const firstRun = simulator.handleCommand('cmd-one', makeCommand());
  await sleepEntered;
  const second = await simulator.handleCommand(
    'cmd-two',
    makeCommand({ createdAt: 1_000_001, expiresAt: 1_030_001 }),
  );
  unblock();
  await firstRun;

  assert.equal(second.status, 'failed');
  assert.equal(database.read('commands/cmd-two/status'), 'failed');
});

test('disconnected device rejects command', async () => {
  const { simulator, database } = makeSimulator();
  simulator.online = false;
  const result = await simulator.handleCommand('cmd-offline', makeCommand());

  assert.equal(result.status, 'failed');
  assert.equal(database.read('commands/cmd-offline/status'), 'failed');
  assert.equal(database.read('devices/frontDoor/isLocked'), undefined);
});

test('invalid command structure is rejected', () => {
  const result = validateCommand(
    makeCommand({ unexpected: true }),
    { now: 1_000_000 },
  );

  assert.equal(result.ok, false);
  assert.equal(result.status, 'failed');
});

test('Firebase heartbeat failure marks simulator offline', async () => {
  const database = new FakeDatabase();
  const { simulator } = makeSimulator({ database });
  simulator.running = true;
  await simulator._writeHeartbeat();
  assert.equal(simulator.online, true);
  database.failWrites = true;
  await simulator._writeHeartbeat();
  assert.equal(simulator.online, false);
});

test('simulator startup publishes simulated mode and heartbeat', async () => {
  const { simulator, database } = makeSimulator();
  simulator.running = false;
  await simulator.start();
  clearInterval(simulator.heartbeatTimer);

  assert.equal(database.read('devices/gateway/isOnline'), true);
  assert.equal(database.read('devices/gateway/mode'), 'simulated');
  assert.equal(database.read('devices/frontDoor/isLocked'), true);
});
