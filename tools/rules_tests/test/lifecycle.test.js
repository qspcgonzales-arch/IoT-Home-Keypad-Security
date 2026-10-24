// Full remote-unlock lifecycle against the Realtime Database EMULATOR.
// The device side is the REAL DeviceSimulator class (firebase-admin with a
// database auth override, so security rules are enforced). The admin side and
// the Flutter-style listener use the web SDK, as the Flutter app does.
// The Dart service itself is NOT executed here.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { createRequire } = require('node:module');
const { initializeTestEnvironment, assertSucceeds, assertFails } = require('@firebase/rules-unit-testing');
const { ref, get, set, update, push, onValue, serverTimestamp } = require('firebase/database');

const simRequire = createRequire(path.join(__dirname, '..', '..', 'device_simulator', 'package.json'));
const adminSdk = simRequire('firebase-admin');
const { DeviceSimulator } = simRequire('./device_simulator.js');

const PROJECT = 'demo-smart-home';
const ADMIN = 'ERxtj2dQuQRvv3Duw8Ud4DNMErD3';
const STRANGER = 'some-other-user';
const root = path.join(__dirname, '..', '..', '..');
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const until = async (fn, ms = 8000) => {
  const end = Date.now() + ms;
  while (Date.now() < end) { const v = await fn(); if (v) return v; await sleep(50); }
  throw new Error('timed out waiting for condition');
};

async function setup(rulesFile) {
  const env = await initializeTestEnvironment({
    projectId: PROJECT,
    database: { rules: fs.readFileSync(path.join(root, rulesFile), 'utf8') },
  });
  await env.clearDatabase();
  const [host, port] = process.env.FIREBASE_DATABASE_EMULATOR_HOST.split(':');
  const app = adminSdk.initializeApp({
    projectId: PROJECT,
    databaseURL: `http://${host}:${port}?ns=${PROJECT}`,
    databaseAuthVariableOverride: { uid: 'device-simulator' },
  }, `sim-${rulesFile}-${Date.now()}`);
  const sim = new DeviceSimulator({
    database: adminSdk.database(app),
    adminUid: ADMIN,
    heartbeatIntervalMs: 60000,
    unlockDurationMs: 4000,
    log: () => {},
  });
  return { env, app, sim };
}
const cmd = (over = {}) => {
  const now = Date.now();
  return { type: 'unlock', status: 'pending', requestedBy: ADMIN, createdAt: serverTimestamp(), expiresAt: now + 30000, ...over };
};

test('PROPOSED rules: complete remote-unlock lifecycle', async () => {
  const { env, app, sim } = await setup('database.rules.proposed.json');
  const adminDb = env.authenticatedContext(ADMIN).database();
  const strangerDb = env.authenticatedContext(STRANGER).database();
  const lockValues = [];
  const unsubscribe = onValue(ref(adminDb, 'devices/frontDoor/isLocked'), (s) => lockValues.push(s.val()));
  try {
    await sim.start();
    await until(() => lockValues.includes(true));

    // 2, 11, 12: unauthorized clients
    await assertFails(set(ref(strangerDb, 'commands/evil'), cmd({ requestedBy: STRANGER })));
    await assertFails(set(ref(strangerDb, 'commands/evil2'), cmd()));
    await assertFails(set(ref(strangerDb, 'devices/frontDoor/isLocked'), false));
    await assertFails(update(ref(strangerDb, 'devices/gateway'), { isOnline: true, mode: 'simulated', lastSeen: serverTimestamp() }));
    await assertFails(set(ref(strangerDb, 'auditLogs/forged'), { type: 'remoteUnlock', description: 'x', timestamp: Date.now(), commandId: 'c', actorUid: STRANGER }));

    // 1: admin creates a valid command (server-timestamp createdAt, as the app does)
    const newRef = push(ref(adminDb, 'commands'));
    await assertSucceeds(set(newRef, cmd()));
    const id = newRef.key;

    // 3-6: simulator reads pending, accepts, completes, lock state updates
    const done = await until(async () => {
      const v = (await get(ref(adminDb, `commands/${id}`))).val();
      return v && v.status === 'completed' ? v : null;
    });
    assert.equal(done.deviceId, 'local-device-simulator');
    assert.ok(typeof done.acceptedAt === 'number' && typeof done.acknowledgedAt === 'number');
    await until(() => lockValues.includes(false));
    await until(() => lockValues.at(-1) === true); // automatic relock

    // 7: audit entries
    await until(async () => {
      const logs = Object.values((await get(ref(adminDb, 'auditLogs'))).val() || {});
      return logs.some((l) => l.commandId === id && l.type === 'automaticRelock');
    });
    const logs = Object.values((await get(ref(adminDb, 'auditLogs'))).val() || {});
    const types = logs.filter((l) => l.commandId === id).map((l) => l.type);
    assert.ok(types.includes('remoteUnlock'), `audit types: ${types}`);
    assert.ok(logs.some((l) => l.type === 'automaticRelock'));

    // 8: Flutter-style listener saw the full sequence true -> false -> true
    const seq = lockValues.filter((v, i) => i === 0 || v !== lockValues[i - 1]);
    assert.deepEqual(seq.slice(0, 3), [true, false, true]);

    // 9: invalid transitions on a terminal command
    await assertFails(update(ref(adminDb, `commands/${id}`), { status: 'pending' }));
    await assertFails(update(ref(adminDb, `commands/${id}`), { status: 'accepted' }));
    await assertFails(set(ref(strangerDb, `commands/${id}/status`), 'pending'));

    // 10: expired command (seeded) is rejected, never unlocks
    const t0 = lockValues.length;
    await env.withSecurityRulesDisabled(async (ctx) => {
      await set(ref(ctx.database(), 'commands/expired1'), {
        type: 'unlock', status: 'pending', requestedBy: ADMIN, createdAt: Date.now() - 50000, expiresAt: Date.now() - 20000,
      });
    });
    const exp = await until(async () => {
      const v = (await get(ref(adminDb, 'commands/expired1'))).val();
      return v && v.status !== 'pending' ? v : null;
    });
    assert.ok(['expired', 'failed'].includes(exp.status), `expired status: ${exp.status}`);
    assert.equal(lockValues.length, t0, 'expired command must not change lock state');

    // 10: completed command replayed (status reset by privileged seed) is not re-executed in-process
    await env.withSecurityRulesDisabled(async (ctx) => {
      await update(ref(ctx.database(), `commands/${id}`), { status: 'pending' });
    });
    await sleep(600);
    assert.equal(lockValues.length, t0, 'duplicate command id must not unlock again');
  } finally {
    unsubscribe();
    await sim.stop().catch(() => {});
    await app.delete();
    await env.cleanup();
  }
});

test('deployment rules: simulator completes an authorized command lifecycle', async () => {
  const { env, app, sim } = await setup('database.rules.json');
  const adminDb = env.authenticatedContext(ADMIN).database();
  const lockValues = [];
  const unsubscribe = onValue(ref(adminDb, 'devices/frontDoor/isLocked'), (s) => lockValues.push(s.val()));
  try {
    await sim.start();
    await until(() => lockValues.includes(true));
    const r = push(ref(adminDb, 'commands'));
    await assertSucceeds(set(r, cmd()));
    const result = await until(async () => {
      const v = (await get(ref(adminDb, `commands/${r.key}`))).val();
      return v && v.status === 'completed' ? v : null;
    });
    assert.equal(result.deviceId, 'local-device-simulator');
    await until(() => lockValues.includes(false));
    await until(() => lockValues.at(-1) === true);
  } finally {
    unsubscribe();
    await sim.stop().catch(() => {});
    await app.delete();
    await env.cleanup();
  }
});
