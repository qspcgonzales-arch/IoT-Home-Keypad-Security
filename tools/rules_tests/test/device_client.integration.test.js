// Auth and Realtime Database REST contract test. Both emulators are bound to
// loopback by firebase.test.json; no live Firebase project is contacted.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { initializeTestEnvironment } = require('@firebase/rules-unit-testing');
const { ref, set } = require('firebase/database');

const PROJECT = 'demo-smart-home';
const ADMIN = 'ERxtj2dQuQRvv3Duw8Ud4DNMErD3';
const DEVICE_ID = 'esp32-frontdoor-1';
const API_KEY = 'fake-api-key';
const RULES_PATH = path.join(__dirname, '..', '..', '..', 'database.rules.proposed.json');
const databaseEmulator = process.env.FIREBASE_DATABASE_EMULATOR_HOST || '127.0.0.1:9000';
const authEmulator = process.env.FIREBASE_AUTH_EMULATOR_HOST || '127.0.0.1:9099';
const databaseUrl = `http://${databaseEmulator}/`;
const authUrl = `http://${authEmulator}/identitytoolkit.googleapis.com/v1`;
let env;
let uid;
let idToken;
let refreshToken;

async function authPost(url, body, contentType = 'application/json') {
  const response = await fetch(url, {
    method: 'POST',
    headers: { 'Content-Type': contentType },
    body: contentType === 'application/json' ? JSON.stringify(body) : body,
  });
  const payload = await response.json();
  assert.equal(response.ok, true, `Auth emulator returned ${response.status}: ${JSON.stringify(payload)}`);
  return payload;
}

function databaseRequest(pathname, { method = 'GET', query = {}, body, headers = {} } = {}) {
  const url = new URL(pathname ? `${pathname}.json` : '.json', databaseUrl);
  url.searchParams.set('ns', PROJECT);
  if (idToken) url.searchParams.set('auth', idToken);
  for (const [key, value] of Object.entries(query)) url.searchParams.set(key, value);
  return fetch(url, {
    method,
    headers: {
      ...(body === undefined ? {} : { 'Content-Type': 'application/json' }),
      ...headers,
    },
    body: body === undefined ? undefined : JSON.stringify(body),
  });
}

test.before(async () => {
  const [host, port] = databaseEmulator.split(':');
  env = await initializeTestEnvironment({
    projectId: PROJECT,
    database: {
      rules: fs.readFileSync(RULES_PATH, 'utf8'),
      host,
      port: Number(port),
    },
  });
});

test.after(async () => {
  if (env) await env.cleanup();
});

test('device auth, registry, command lifecycle, state/audit writes and revocation', async () => {
  const email = `esp32-${Date.now()}@device.invalid`;
  const password = 'test-only-device-password-1234567890';
  const apiKeyQuery = `?key=${API_KEY}`;
  await authPost(`${authUrl}/accounts:signUp${apiKeyQuery}`, {
    email,
    password,
    returnSecureToken: true,
  });
  const signIn = await authPost(`${authUrl}/accounts:signInWithPassword${apiKeyQuery}`, {
    email,
    password,
    returnSecureToken: true,
  });
  uid = signIn.localId;
  idToken = signIn.idToken;
  refreshToken = signIn.refreshToken;
  assert.ok(uid);
  assert.notEqual(uid, ADMIN);

  await env.clearDatabase();
  await env.withSecurityRulesDisabled(async (context) => {
    await set(ref(context.database(), `deviceRegistry/${uid}`), {
      enabled: true,
      deviceId: DEVICE_ID,
    });
    const now = Date.now();
    await set(ref(context.database(), 'commands/emulator-command'), {
      type: 'unlock',
      status: 'pending',
      requestedBy: ADMIN,
      createdAt: now - 1000,
      expiresAt: now + 30000,
    });
  });

  let response = await databaseRequest(`deviceRegistry/${uid}`);
  assert.equal(response.status, 200);
  assert.deepEqual(await response.json(), { enabled: true, deviceId: DEVICE_ID });

  response = await databaseRequest('commands', {
    query: {
      orderBy: '"status"',
      equalTo: '"pending"',
      limitToFirst: '10',
    },
  });
  assert.equal(response.status, 200);
  assert.ok((await response.json())['emulator-command']);

  response = await databaseRequest('commands/emulator-command', {
    method: 'PATCH',
    body: {
      status: 'accepted',
      acceptedAt: Date.now(),
      deviceId: DEVICE_ID,
    },
  });
  assert.equal(response.status, 200, await response.text());

  response = await databaseRequest('commands/emulator-command', {
    method: 'PATCH',
    body: {
      status: 'completed',
      acknowledgedAt: Date.now(),
      message: 'Arduino acknowledged command execution.',
    },
  });
  assert.equal(response.status, 200, await response.text());

  response = await databaseRequest('devices/frontDoor/isLocked', {
    method: 'PUT',
    body: false,
  });
  assert.equal(response.status, 200, await response.text());

  const now = Date.now();
  response = await databaseRequest('devices/gateway', {
    method: 'PATCH',
    body: {
      isOnline: true,
      lastSeen: now,
      mode: 'hardware',
      arduinoConnected: true,
      arduinoLastSeen: now,
    },
  });
  assert.equal(response.status, 200, await response.text());

  const auditRecord = {
    type: 'remoteUnlock',
    description: 'Emulator integration test.',
    timestamp: now,
    commandId: 'emulator-command',
    actorUid: ADMIN,
  };
  response = await databaseRequest(`auditLogs/${DEVICE_ID}-emulator-command-remote`, {
    method: 'PUT',
    body: auditRecord,
  });
  assert.equal(response.status, 200, await response.text());
  response = await databaseRequest(`auditLogs/${DEVICE_ID}-emulator-command-remote`, {
    method: 'PUT',
    body: auditRecord,
  });
  assert.equal(response.status, 200, await response.text());
  response = await databaseRequest(`auditLogs/${DEVICE_ID}-emulator-command-remote`, {
    method: 'PUT',
    body: { ...auditRecord, description: 'Attempted overwrite.' },
  });
  assert.equal(response.ok, false, 'device may retry an identical audit write but cannot overwrite it');

  const refreshed = await authPost(
    `http://${authEmulator}/securetoken.googleapis.com/v1/token${apiKeyQuery}`,
    `grant_type=refresh_token&refresh_token=${encodeURIComponent(refreshToken)}`,
    'application/x-www-form-urlencoded',
  );
  assert.equal(refreshed.user_id, uid);
  idToken = refreshed.id_token;
  response = await databaseRequest(`deviceRegistry/${uid}`);
  assert.equal(response.status, 200);

  await env.withSecurityRulesDisabled(async (context) => {
    await set(ref(context.database(), `deviceRegistry/${uid}/enabled`), false);
  });
  response = await databaseRequest('devices/frontDoor/isLocked', {
    method: 'PUT',
    body: true,
  });
  assert.equal(response.ok, false, 'revoked device must lose write access');
});
