const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {
  initializeTestEnvironment,
  assertSucceeds,
  assertFails,
} = require('@firebase/rules-unit-testing');
const {
  ref,
  get,
  set,
  update,
  query,
  orderByChild,
  equalTo,
  limitToFirst,
  serverTimestamp,
} = require('firebase/database');

const PROJECT = 'demo-smart-home';
const ADMIN = 'staging-admin-test-uid';
const DEVICE = 'staging-device-test-uid';
const STAGING_ADMIN_UID = 'fyTwhPR5RMek0Bdd40C08McNZmm2';
const STAGING_DEVICE_UID = '66BkT43W8tPkWYL8jV4dgroMCbN2';
const DEVICE_ID = 'esp32-frontdoor-1';
const STRANGER = 'staging-untrusted-user';
const rulesPath = path.join(__dirname, '..', '.staging.rules.json');
const repositoryRoot = path.resolve(__dirname, '../../..');
const flutterConfigPath = path.join(
  repositoryRoot,
  'staging_firebase_options.json',
);
const firmwareConfigPath = path.join(
  repositoryRoot,
  'hardware',
  'esp32',
  'esp32_only',
  'src',
  'secrets.h',
);
let env;

const as = (uid) => env.authenticatedContext(uid).database();
const pendingCommand = (overrides = {}) => {
  const now = Date.now();
  return {
    type: 'unlock',
    status: 'pending',
    requestedBy: ADMIN,
    createdAt: now - 1000,
    expiresAt: now + 30000,
    targetDeviceId: DEVICE_ID,
    ...overrides,
  };
};
const auditEvent = (overrides = {}) => ({
  type: 'commandFailed',
  description: 'Remote actuation is disabled in this firmware build.',
  timestamp: Date.now(),
  commandId: 'testCommand',
  actorUid: DEVICE,
  ...overrides,
});

async function seed(data) {
  await env.withSecurityRulesDisabled(async (context) => {
    await set(ref(context.database()), data);
  });
}

test.before(async () => {
  let rules = fs.readFileSync(rulesPath, 'utf8');
  const adminUid = fs.existsSync(flutterConfigPath)
    ? JSON.parse(fs.readFileSync(flutterConfigPath, 'utf8'))
        .FIREBASE_ADMIN_UID
    : '';
  const firmwareConfig = fs.existsSync(firmwareConfigPath)
    ? fs.readFileSync(firmwareConfigPath, 'utf8')
    : '';
  const deviceUid =
    /^\s*#define\s+FIREBASE_DEVICE_UID\s+"([^"]*)"/m.exec(firmwareConfig)?.[1] ??
    '';
  rules = rules
    .replaceAll('__STAGING_ADMIN_UID__', ADMIN)
    .replaceAll('__STAGING_DEVICE_UID__', DEVICE)
    .replaceAll(STAGING_ADMIN_UID, ADMIN)
    .replaceAll(STAGING_DEVICE_UID, DEVICE);
  if (adminUid) rules = rules.replaceAll(adminUid, ADMIN);
  if (deviceUid) rules = rules.replaceAll(deviceUid, DEVICE);
  env = await initializeTestEnvironment({
    projectId: PROJECT,
    database: { rules },
  });
});

test.after(async () => {
  if (env) await env.cleanup();
});

test.beforeEach(async () => {
  await env.clearDatabase();
  await seed({
    deviceRegistry: {
      [DEVICE]: { enabled: true, deviceId: DEVICE_ID },
    },
    commands: { testCommand: pendingCommand() },
    devices: {
      frontDoor: { isLocked: true },
      gateway: {
        isOnline: true,
        lastSeen: Date.now(),
        mode: 'esp32_only',
        deviceId: DEVICE_ID,
        firmwareVersion: '6.7.0-cloud-diagnostics',
      },
      sensors: { irDigitalState: 'LOW', lastSeen: Date.now() },
    },
  });
});

test('staging administrator can read status and audit data and target the test device', async () => {
  const db = as(ADMIN);
  await assertSucceeds(get(ref(db, 'devices/frontDoor/isLocked')));
  await assertSucceeds(get(ref(db, 'devices/gateway')));
  await assertSucceeds(get(ref(db, 'devices/sensors')));
  await assertSucceeds(get(ref(db, 'auditLogs')));
  await assertSucceeds(
    set(ref(db, 'commands/new-command'), pendingCommand()),
  );
  await assertSucceeds(
    set(ref(db, 'settings/frontDoor/unlockDurationSeconds'), 10),
  );
  await assertFails(
    set(ref(db, 'settings/frontDoor/unlockDurationSeconds'), 31),
  );
  await assertFails(
    set(
      ref(db, 'commands/wrong-target'),
      pendingCommand({ targetDeviceId: 'another-device' }),
    ),
  );
  await assertFails(
    set(ref(db, 'devices/frontDoor/isLocked'), false),
  );
  await assertFails(
    set(ref(db, 'auditLogs/forged'), auditEvent()),
  );
});

test('device is limited to own registry, diagnostics, command polling and audit writes', async () => {
  const db = as(DEVICE);
  await assertSucceeds(get(ref(db, `deviceRegistry/${DEVICE}`)));
  await assertFails(get(ref(db, 'deviceRegistry')));
  await assertFails(get(ref(db, 'auditLogs')));
  await assertSucceeds(
    get(
      query(
        ref(db, 'commands'),
        orderByChild('status'),
        equalTo('pending'),
        limitToFirst(10),
      ),
    ),
  );
  await assertFails(get(ref(db, 'commands')));
  await assertSucceeds(set(ref(db, 'devices/frontDoor/isLocked'), true));
  await assertSucceeds(
    update(ref(db, 'devices/gateway'), {
      isOnline: true,
      lastSeen: serverTimestamp(),
      mode: 'esp32_only',
      deviceId: DEVICE_ID,
      firmwareVersion: '6.7.0-cloud-diagnostics',
    }),
  );
  await assertSucceeds(set(ref(db, 'devices/sensors/irDigitalState'), 'HIGH'));
  await assertFails(
    set(ref(db, 'devices/gateway/mode'), 'hardware'),
  );
  await assertFails(
    set(ref(db, 'devices/frontDoor/isLocked'), 'unlocked'),
  );
  await assertFails(
    set(ref(db, 'commands/forged'), pendingCommand()),
  );
  await assertFails(
    set(ref(db, 'settings/frontDoor/unlockDurationSeconds'), 30),
  );
  await assertSucceeds(
    set(
      ref(db, 'auditLogs/failed-ack'),
      auditEvent({ requestedBy: ADMIN }),
    ),
  );
});

test('device audit actor is its UID and requestedBy matches the real command', async () => {
  const db = as(DEVICE);
  await assertSucceeds(
    set(ref(db, 'auditLogs/local-event'), auditEvent()),
  );
  await assertSucceeds(
    set(
      ref(db, 'auditLogs/command-event'),
      auditEvent({ requestedBy: ADMIN }),
    ),
  );
  await assertFails(
    set(
      ref(db, 'auditLogs/impersonated-admin'),
      auditEvent({ actorUid: ADMIN }),
    ),
  );
  await assertFails(
    set(
      ref(db, 'auditLogs/forged-requester'),
      auditEvent({ requestedBy: STRANGER }),
    ),
  );
  await assertFails(
    set(
      ref(db, 'auditLogs/unknown-command-requester'),
      auditEvent({ commandId: 'missing-command', requestedBy: ADMIN }),
    ),
  );
});

test('legacy audit records remain readable and immutable to the device', async () => {
  await seed({
    auditLogs: {
      legacy: auditEvent({ actorUid: ADMIN }),
    },
  });
  const db = as(DEVICE);
  await assertFails(
    update(ref(db, 'auditLogs/legacy'), {
      actorUid: DEVICE,
      requestedBy: ADMIN,
    }),
  );
  const legacy = await get(ref(as(ADMIN), 'auditLogs/legacy'));
  assert.equal(legacy.val().actorUid, ADMIN);
  assert.equal(legacy.val().requestedBy, undefined);
});

test('device cannot mutate existing audit facts', async () => {
  await seed({
    auditLogs: {
      immutable: auditEvent({ requestedBy: ADMIN }),
    },
  });
  await assertFails(
    update(ref(as(DEVICE), 'auditLogs/immutable'), {
      description: 'changed after creation',
    }),
  );
});

test('device acknowledges commands as failed without accepted or completed states', async () => {
  const db = as(DEVICE);
  await assertSucceeds(
    update(ref(db, 'commands/testCommand'), {
      status: 'failed',
      acknowledgedAt: serverTimestamp(),
      deviceId: DEVICE_ID,
      message: 'Remote actuation is disabled.',
    }),
  );
  await assertFails(
    update(ref(db, 'commands/testCommand'), {
      status: 'completed',
      acknowledgedAt: serverTimestamp(),
      deviceId: DEVICE_ID,
      message: 'Unlocked.',
    }),
  );
  await assertFails(
    update(ref(db, 'commands/testCommand'), {
      status: 'accepted',
      acceptedAt: serverTimestamp(),
      deviceId: DEVICE_ID,
    }),
  );
});

test('expired command may be acknowledged as expired and cannot be retargeted', async () => {
  await seed({
    deviceRegistry: {
      [DEVICE]: { enabled: true, deviceId: DEVICE_ID },
    },
    commands: {
      expired: pendingCommand({
        createdAt: Date.now() - 60000,
        expiresAt: Date.now() - 1000,
      }),
    },
  });
  const db = as(DEVICE);
  await assertSucceeds(
    update(ref(db, 'commands/expired'), {
      status: 'expired',
      acknowledgedAt: serverTimestamp(),
      deviceId: DEVICE_ID,
      message: 'Command expired before processing.',
    }),
  );
  await assertFails(
    update(ref(db, 'commands/testCommand'), {
      status: 'failed',
      acknowledgedAt: serverTimestamp(),
      deviceId: DEVICE_ID,
      message: 'no',
      targetDeviceId: 'other-device',
    }),
  );
});

test('unknown identity cannot read or write staging data', async () => {
  const db = as(STRANGER);
  await assertFails(get(ref(db, 'devices')));
  await assertFails(get(ref(db, 'commands')));
  await assertFails(set(ref(db, 'devices/gateway/isOnline'), true));
  await assertFails(set(ref(db, 'auditLogs/forged'), auditEvent()));
});

test('disabling the registry entry immediately revokes device data access', async () => {
  await seed({
    deviceRegistry: {
      [DEVICE]: { enabled: false, deviceId: DEVICE_ID },
    },
  });
  const db = as(DEVICE);
  await assertSucceeds(get(ref(db, `deviceRegistry/${DEVICE}`)));
  await assertFails(get(ref(db, 'devices/gateway')));
  await assertFails(
    get(
      query(
        ref(db, 'commands'),
        orderByChild('status'),
        equalTo('pending'),
        limitToFirst(10),
      ),
    ),
  );
  await assertFails(set(ref(db, 'auditLogs/revoked'), auditEvent()));
});
