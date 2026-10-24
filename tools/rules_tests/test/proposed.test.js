// Emulator tests for database.rules.proposed.json. These run against the local
// Realtime Database emulator only; they never touch the production project.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { initializeTestEnvironment, assertSucceeds, assertFails } = require('@firebase/rules-unit-testing');
const { ref, get, set, update, remove, query, orderByChild, equalTo, limitToFirst, serverTimestamp } = require('firebase/database');

const ADMIN = 'ERxtj2dQuQRvv3Duw8Ud4DNMErD3';
const SIM = 'device-simulator';
const ESP = 'esp32-uid-1';
const ESP_ID = 'esp32-frontdoor-1';
const ESP2 = 'esp32-uid-2';
const REVOKED = 'esp32-revoked';
const STRANGER = 'some-other-user';

const rulesPath = process.env.RULES_FILE ||
  path.join(__dirname, '..', '..', '..', 'database.rules.proposed.json');

let env;
let db;

const as = (uid) => env.authenticatedContext(uid).database();
const pendingCmd = (over = {}) => {
  const now = Date.now();
  return { type: 'unlock', status: 'pending', requestedBy: ADMIN, createdAt: now - 1000, expiresAt: now + 30000, ...over };
};
const acceptedCmd = (deviceId) => pendingCmd({ status: 'accepted', acceptedAt: Date.now(), deviceId });
const audit = (over = {}) => ({
  type: 'remoteUnlock', description: 'Unlocked after authorized command.',
  timestamp: Date.now(), commandId: 'cmd1', actorUid: ADMIN, ...over,
});

async function seed(data) {
  await env.withSecurityRulesDisabled(async (ctx) => { await set(ref(ctx.database()), data); });
}

test.before(async () => {
  env = await initializeTestEnvironment({
    projectId: 'demo-smart-home',
    database: { rules: fs.readFileSync(rulesPath, 'utf8') },
  });
});
test.after(async () => { await env.cleanup(); });
test.beforeEach(async () => {
  await env.clearDatabase();
  await seed({
    deviceRegistry: {
      [ESP]: { enabled: true, deviceId: ESP_ID },
      [ESP2]: { enabled: true, deviceId: 'esp32-other' },
      [REVOKED]: { enabled: false, deviceId: 'esp32-revoked-id' },
    },
    commands: { c1: pendingCmd() },
  });
});

test('administrator: may create a valid pending command', async () => {
  await assertSucceeds(set(ref(as(ADMIN), 'commands/new1'), pendingCmd()));
});
test('administrator: cannot forge requestedBy, accept commands, or exceed 60s expiry', async () => {
  await assertFails(set(ref(as(ADMIN), 'commands/n2'), pendingCmd({ requestedBy: 'someone' })));
  await assertFails(set(ref(as(ADMIN), 'commands/n3'), pendingCmd({ expiresAt: Date.now() + 120000 })));
  await assertFails(update(ref(as(ADMIN), 'commands/c1'), { status: 'accepted', acceptedAt: Date.now(), deviceId: ESP_ID }));
});
test('administrator: reads audit logs, cannot write them or the device registry', async () => {
  await assertSucceeds(get(ref(as(ADMIN), 'auditLogs')));
  await assertFails(set(ref(as(ADMIN), 'auditLogs/a1'), audit()));
  await assertFails(set(ref(as(ADMIN), 'deviceRegistry/x'), { enabled: true, deviceId: 'x' }));
  await assertFails(get(ref(as(ADMIN), 'deviceRegistry')));
});
test('administrator: unlock duration limited to 4..30 whole seconds', async () => {
  await assertSucceeds(set(ref(as(ADMIN), 'settings/frontDoor/unlockDurationSeconds'), 10));
  await assertFails(set(ref(as(ADMIN), 'settings/frontDoor/unlockDurationSeconds'), 3));
  await assertFails(set(ref(as(ADMIN), 'settings/frontDoor/unlockDurationSeconds'), 31));
});

test('unauthorized user: no reads or writes anywhere', async () => {
  const u = as(STRANGER);
  await assertFails(get(ref(u, 'commands/c1')));
  await assertFails(get(ref(u, 'auditLogs')));
  await assertFails(get(ref(u, 'devices/frontDoor/isLocked')));
  await assertFails(set(ref(u, 'commands/x'), pendingCmd({ requestedBy: STRANGER })));
  await assertFails(set(ref(u, 'devices/frontDoor/isLocked'), false));
  await assertFails(set(ref(u, `deviceRegistry/${STRANGER}`), { enabled: true, deviceId: 'evil' }));
  await assertFails(set(ref(u, 'auditLogs/a'), audit()));
});
test('unauthenticated client: denied', async () => {
  const anon = env.unauthenticatedContext().database();
  await assertFails(get(ref(anon, 'commands/c1')));
  await assertFails(set(ref(anon, 'devices/frontDoor/isLocked'), false));
});

test('registered device: bounded pending query and single-command read allowed', async () => {
  const d = as(ESP);
  await assertSucceeds(get(query(ref(d, 'commands'), orderByChild('status'), equalTo('pending'), limitToFirst(5))));
  await assertSucceeds(get(ref(d, 'commands/c1')));
  await assertSucceeds(get(ref(d, `deviceRegistry/${ESP}`)));
});
test('registered device: unbounded or foreign reads denied', async () => {
  const d = as(ESP);
  await assertFails(get(ref(d, 'commands')));
  await assertFails(get(query(ref(d, 'commands'), orderByChild('status'), equalTo('pending'), limitToFirst(50))));
  await assertFails(get(ref(d, `deviceRegistry/${ESP2}`)));
  await assertFails(get(ref(d, 'auditLogs')));
});
test('registered device: full accept -> complete flow with its own deviceId', async () => {
  const d = as(ESP);
  await assertSucceeds(update(ref(d, 'commands/c1'), { status: 'accepted', acceptedAt: serverTimestamp(), deviceId: ESP_ID }));
  await assertSucceeds(update(ref(d, 'commands/c1'), { status: 'completed', acknowledgedAt: serverTimestamp(), message: 'Door unlocked.' }));
  await assertSucceeds(set(ref(d, 'devices/frontDoor/isLocked'), false));
  await assertSucceeds(set(ref(d, 'devices/gateway/isOnline'), true));
  await assertSucceeds(set(ref(d, 'devices/gateway/lastSeen'), serverTimestamp()));
  await assertSucceeds(set(ref(d, 'devices/gateway/mode'), 'hardware'));
});
test('incorrect device identity: wrong or borrowed deviceId rejected', async () => {
  await assertFails(update(ref(as(ESP), 'commands/c1'), { status: 'accepted', acceptedAt: serverTimestamp(), deviceId: 'esp32-other' }));
  await assertFails(update(ref(as(ESP), 'commands/c1'), { status: 'accepted', acceptedAt: serverTimestamp(), deviceId: 'local-device-simulator' }));
  await assertFails(update(ref(as(ESP2), 'commands/c1'), { status: 'accepted', acceptedAt: serverTimestamp(), deviceId: ESP_ID }));
});
test('revoked or unregistered identities: all device access denied', async () => {
  for (const uid of [REVOKED, STRANGER]) {
    const d = as(uid);
    await assertFails(get(ref(d, 'commands/c1')));
    await assertFails(update(ref(d, 'commands/c1'), { status: 'failed', acknowledgedAt: serverTimestamp(), deviceId: 'x', message: 'no' }));
    await assertFails(set(ref(d, 'devices/frontDoor/isLocked'), false));
    await assertFails(set(ref(d, 'auditLogs/a1'), audit()));
  }
});
test('revocation takes effect immediately when the registry entry is disabled', async () => {
  await assertSucceeds(get(ref(as(ESP), 'commands/c1')));
  await seed({ deviceRegistry: { [ESP]: { enabled: false, deviceId: ESP_ID } }, commands: { c1: pendingCmd() } });
  await assertFails(get(ref(as(ESP), 'commands/c1')));
  await assertFails(set(ref(as(ESP), 'devices/gateway/isOnline'), true));
});

test('unauthorized state modifications: device cannot touch admin-owned data', async () => {
  const d = as(ESP);
  await assertFails(set(ref(d, 'settings/frontDoor/unlockDurationSeconds'), 30));
  await assertFails(set(ref(d, 'commands/forged'), pendingCmd()));
  await assertFails(set(ref(d, `deviceRegistry/${ESP}/deviceId`), 'new-name'));
  await assertFails(set(ref(d, 'devices/frontDoor/isLocked'), 'yes'));
  await assertFails(set(ref(d, 'devices/gateway/mode'), 'anything'));
  await assertFails(set(ref(as(SIM), 'devices/gateway/mode'), 'hardware'));
});
test('invalid command updates: tampering and illegal transitions rejected', async () => {
  const d = as(ESP);
  await assertFails(update(ref(d, 'commands/c1'), { status: 'completed', acknowledgedAt: serverTimestamp(), deviceId: ESP_ID, acceptedAt: serverTimestamp(), message: 'skip' }));
  await assertFails(update(ref(d, 'commands/c1'), { status: 'accepted', acceptedAt: serverTimestamp(), deviceId: ESP_ID, expiresAt: Date.now() + 59000 }));
  await assertFails(update(ref(d, 'commands/c1'), { status: 'accepted', acceptedAt: serverTimestamp(), deviceId: ESP_ID, requestedBy: ESP }));
  await assertFails(update(ref(d, 'commands/c1'), { status: 'accepted', acceptedAt: serverTimestamp(), deviceId: ESP_ID, extra: 1 }));
  await assertFails(update(ref(d, 'commands/c1'), { status: 'accepted', acceptedAt: Date.now() + 3600000, deviceId: ESP_ID }));
  await assertFails(remove(ref(d, 'commands/c1')));
});
test('invalid command updates: expired command cannot be accepted; terminal states are final', async () => {
  const now = Date.now();
  await seed({
    deviceRegistry: { [ESP]: { enabled: true, deviceId: ESP_ID } },
    commands: {
      old: pendingCmd({ createdAt: now - 20000, expiresAt: now - 5000 }),
      done: { ...pendingCmd(), status: 'completed', acceptedAt: now - 500, acknowledgedAt: now - 400, deviceId: ESP_ID, message: 'ok' },
    },
  });
  await assertFails(update(ref(as(ESP), 'commands/old'), { status: 'accepted', acceptedAt: serverTimestamp(), deviceId: ESP_ID }));
  await assertSucceeds(update(ref(as(ESP), 'commands/old'), { status: 'expired', acknowledgedAt: serverTimestamp(), deviceId: ESP_ID, message: 'Command expired.' }));
  await assertFails(update(ref(as(ESP), 'commands/done'), { status: 'pending' }));
  await assertFails(update(ref(as(ESP), 'commands/done'), { status: 'failed', message: 'changed' }));
});

test('audit records: valid device write accepted; invalid shapes rejected', async () => {
  const d = as(ESP);
  const event = audit();
  await assertSucceeds(set(ref(d, 'auditLogs/ok1'), event));
  await assertSucceeds(set(ref(d, 'auditLogs/ok1'), event));
  await assertSucceeds(set(ref(d, 'auditLogs/ok2'), audit({ timestamp: serverTimestamp(), type: 'accessDenied' })));
  const { description, ...noDescription } = audit();
  await assertFails(set(ref(d, 'auditLogs/bad1'), noDescription));
  await assertFails(set(ref(d, 'auditLogs/bad2'), audit({ type: 'rootShell' })));
  await assertFails(set(ref(d, 'auditLogs/bad3'), { ...audit(), extra: 'x' }));
  await assertFails(set(ref(d, 'auditLogs/bad4'), audit({ description: 'x'.repeat(201) })));
  await assertFails(set(ref(d, 'auditLogs/bad5'), audit({ timestamp: Date.now() + 86400000 })));
  await assertFails(set(ref(d, 'auditLogs/bad6'), audit({ timestamp: 'now' })));
  await assertFails(set(ref(d, 'auditLogs/ok1'), { ...event, description: 'overwrite attempt' }));
  await assertFails(remove(ref(d, 'auditLogs/ok1')));
});

test('existing simulator write shapes remain accepted', async () => {
  const s = as(SIM);
  await assertSucceeds(update(ref(s, 'commands/c1'), { status: 'accepted', acceptedAt: serverTimestamp(), deviceId: 'local-device-simulator' }));
  await assertSucceeds(update(ref(s, 'commands/c1'), { status: 'completed', acknowledgedAt: serverTimestamp(), message: 'Simulated device unlocked.' }));
  await assertSucceeds(set(ref(s, 'devices/frontDoor/isLocked'), true));
  await assertSucceeds(set(ref(s, 'devices/gateway/mode'), 'simulated'));
  await assertSucceeds(set(ref(s, 'devices/gateway/lastSeen'), serverTimestamp()));
  await assertSucceeds(set(ref(s, 'auditLogs/s1'), audit({ timestamp: serverTimestamp(), actorUid: ADMIN })));
  await assertSucceeds(get(ref(s, 'commands')));
  await assertFails(update(ref(s, 'commands/c1'), { deviceId: ESP_ID }));
});
