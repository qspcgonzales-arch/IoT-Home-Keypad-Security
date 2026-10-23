'use strict';

const ADMIN_UID = 'ERxtj2dQuQRvv3Duw8Ud4DNMErD3';
const DATABASE_URL =
  'https://smart-home-security-tip-default-rtdb.asia-southeast1.firebasedatabase.app';
const DEVICE_ID = 'local-device-simulator';
const COMMAND_TTL_MS = 60_000;
const UNLOCK_DURATION_MS = 4_000;
const HEARTBEAT_INTERVAL_MS = 5_000;

function validateCommand(command, { adminUid = ADMIN_UID, now = Date.now() } = {}) {
  if (!command || typeof command !== 'object' || Array.isArray(command)) {
    return { ok: false, status: 'failed', message: 'Malformed command record.' };
  }
  const fields = Object.keys(command).sort();
  const expected = ['createdAt', 'expiresAt', 'requestedBy', 'status', 'type'];
  if (fields.length !== expected.length ||
      fields.some((field, index) => field !== expected[index])) {
    return { ok: false, status: 'failed', message: 'Unexpected command fields.' };
  }
  if (command.requestedBy !== adminUid) {
    return { ok: false, status: 'failed', message: 'Requester is not an authorized administrator.' };
  }
  if (command.type !== 'unlock' || command.status !== 'pending') {
    return { ok: false, status: 'failed', message: 'Unsupported command type or status.' };
  }
  if (!Number.isFinite(command.createdAt) || !Number.isFinite(command.expiresAt)) {
    return { ok: false, status: 'failed', message: 'Command timestamps are invalid.' };
  }
  if (command.createdAt > now + 5_000 ||
      command.expiresAt <= command.createdAt ||
      command.expiresAt > command.createdAt + COMMAND_TTL_MS) {
    return { ok: false, status: 'failed', message: 'Command timestamp or expiry is invalid.' };
  }
  if (command.expiresAt <= now) {
    return { ok: false, status: 'expired', message: 'Command expired before device acceptance.' };
  }
  return { ok: true };
}

class DeviceSimulator {
  constructor({
    database,
    adminUid = ADMIN_UID,
    deviceId = DEVICE_ID,
    now = Date.now,
    sleep = (milliseconds) => new Promise((resolve) => setTimeout(resolve, milliseconds)),
    log = console.log,
    serverTimestamp = { '.sv': 'timestamp' },
    heartbeatIntervalMs = HEARTBEAT_INTERVAL_MS,
    unlockDurationMs = UNLOCK_DURATION_MS,
  }) {
    if (!database) throw new Error('A Realtime Database instance is required.');
    this.database = database;
    this.adminUid = adminUid;
    this.deviceId = deviceId;
    this.now = now;
    this.sleep = sleep;
    this.log = log;
    this.serverTimestamp = serverTimestamp;
    this.heartbeatIntervalMs = heartbeatIntervalMs;
    this.unlockDurationMs = unlockDurationMs;
    this.online = false;
    this.running = false;
    this.busy = false;
    this.inFlight = new Set();
    this.processed = new Set();
    this.jobs = new Set();
    this.heartbeatTimer = null;
    this.commandListener = null;
    this.commandCancelListener = null;
  }

  async start() {
    if (this.running) return;
    this.running = true;
    await this.database.ref('devices/frontDoor/isLocked').set(true);
    await this._writeHeartbeat();
    this.heartbeatTimer = setInterval(() => {
      void this._writeHeartbeat();
    }, this.heartbeatIntervalMs);
    this.heartbeatTimer.unref?.();

    const commands = this.database.ref('commands');
    this.commandListener = (snapshot) => {
      if (!snapshot || !snapshot.key) return;
      const job = this.handleCommand(snapshot.key, snapshot.val())
        .catch((error) => this._log(`Command ${snapshot.key} processing error: ${error.message}`));
      this.jobs.add(job);
      void job.finally(() => this.jobs.delete(job));
    };
    this.commandCancelListener = (error) => {
      this.online = false;
      this._log(`Command listener disconnected: ${error?.message || error}`);
    };
    commands.on('child_added', this.commandListener, this.commandCancelListener);
    this._log(`Simulator ${this.deviceId} started in SIMULATED mode; listening for commands.`);
  }

  async stop() {
    if (!this.running) return;
    this.running = false;
    if (this.heartbeatTimer) clearInterval(this.heartbeatTimer);
    this.heartbeatTimer = null;
    this.database
      .ref('commands')
      .off('child_added', this.commandListener);
    await Promise.allSettled([...this.jobs]);
    this.online = false;
    await this.database.ref('devices/gateway').update({
      isOnline: false,
      lastSeen: this.serverTimestamp,
      mode: 'simulated',
    });
    this._log(`Simulator ${this.deviceId} stopped and marked offline.`);
  }

  async _writeHeartbeat() {
    if (!this.running) return;
    try {
      const wasOnline = this.online;
      await this.database.ref('devices/gateway').update({
        isOnline: true,
        lastSeen: this.serverTimestamp,
        mode: 'simulated',
      });
      this.online = true;
      if (!wasOnline) {
        this._log(`Firebase heartbeat accepted; simulator ${this.deviceId} is online.`);
      }
    } catch (error) {
      this.online = false;
      this._log(`Heartbeat failed; simulator is offline: ${error.message}`);
    }
  }

  async handleCommand(commandId, command) {
    if (!commandId) return { status: 'failed', message: 'Missing command ID.' };
    if (this.inFlight.has(commandId) || this.processed.has(commandId)) {
      this._log(`Duplicate command ${commandId} ignored.`);
      return { status: 'duplicate' };
    }
    if (command?.status === 'accepted') {
      const expired = Number(command.expiresAt) <= this.now();
      const status = expired ? 'expired' : 'failed';
      const message = expired
        ? 'Command expired while the simulator was restarting.'
        : 'Simulator restarted before this accepted command completed.';
      await this._rejectCommand(commandId, command, status, message);
      return { status };
    }
    if (command?.status !== 'pending') return { status: 'ignored' };

    this.inFlight.add(commandId);
    let doorUnlocked = false;
    let commandCompleted = false;
    try {
      const validation = validateCommand(command, {
        adminUid: this.adminUid,
        now: this.now(),
      });
      if (!validation.ok) {
        await this._rejectCommand(
          commandId,
          command,
          validation.status,
          validation.message,
        );
        return { status: validation.status, message: validation.message };
      }
      if (!this.online || !this.running) {
        const message = 'Device simulator is disconnected.';
        await this._rejectCommand(commandId, command, 'failed', message);
        return { status: 'failed', message };
      }
      if (this.busy) {
        const message = 'Device is busy with another unlock request.';
        await this._rejectCommand(commandId, command, 'failed', message);
        return { status: 'failed', message };
      }

      this.busy = true;
      const commandRef = this.database.ref(`commands/${commandId}`);
      await commandRef.update({
        status: 'accepted',
        acceptedAt: this.serverTimestamp,
        deviceId: this.deviceId,
      });
      this._log(`Command ${commandId} accepted.`);

      await this.database.ref('devices/frontDoor/isLocked').set(false);
      doorUnlocked = true;
      await this._writeAuditEvent({
        type: 'remoteUnlock',
        description: 'Simulated device unlocked after authorized command.',
        commandId,
        actorUid: command.requestedBy,
      });
      await commandRef.update({
        status: 'completed',
        acknowledgedAt: this.serverTimestamp,
        message: 'Unlock execution acknowledged by simulated device.',
      });
      commandCompleted = true;
      this._log(`Command ${commandId} completed; door unlocked.`);

      await this.sleep(this.unlockDurationMs);
      await this.database.ref('devices/frontDoor/isLocked').set(true);
      doorUnlocked = false;
      await this._writeAuditEvent({
        type: 'automaticRelock',
        description: 'Simulated device automatically relocked after 4 seconds.',
        commandId,
        actorUid: command.requestedBy,
      });
      this._log(`Command ${commandId} relocked automatically.`);
      return { status: 'completed' };
    } catch (error) {
      this._log(`Command ${commandId} failed: ${error.message}`);
      if (doorUnlocked) {
        try {
          await this.database.ref('devices/frontDoor/isLocked').set(true);
          await this._writeAuditEvent({
            type: 'automaticRelock',
            description: 'Safety relock after an interrupted simulator command.',
            commandId,
            actorUid: command?.requestedBy || 'unknown',
          });
        } catch (relockError) {
          this._log(`Safety relock failed: ${relockError.message}`);
        }
      }
      if (!commandCompleted) {
        try {
          await this._rejectCommand(
            commandId,
            command,
            'failed',
            `Device execution failed: ${error.message}`,
          );
        } catch (ackError) {
          this._log(`Could not publish command failure: ${ackError.message}`);
        }
      }
      return { status: 'failed', message: error.message };
    } finally {
      this.busy = false;
      this.inFlight.delete(commandId);
      this.processed.add(commandId);
    }
  }

  async _rejectCommand(commandId, command, status, message) {
    try {
      await this.database.ref(`commands/${commandId}`).update({
        status,
        acknowledgedAt: this.serverTimestamp,
        deviceId: this.deviceId,
        message,
      });
    } catch (error) {
      this._log(`Could not acknowledge rejected command ${commandId}: ${error.message}`);
    }
    await this._writeAuditEvent({
      type: status === 'expired' ? 'commandExpired' : 'commandFailed',
      description: message,
      commandId,
      actorUid: typeof command?.requestedBy === 'string'
        ? command.requestedBy
        : 'unknown',
    });
    this._log(`Command ${commandId} rejected (${status}): ${message}`);
  }

  async _writeAuditEvent(event) {
    const auditRef = this.database.ref('auditLogs').push();
    await auditRef.set({ ...event, timestamp: this.serverTimestamp });
  }

  _log(message) {
    this.log(`[${new Date(this.now()).toISOString()}] ${message}`);
  }
}

function initializeFirebaseSimulator() {
  const fs = require('node:fs');
  const path = require('node:path');
  const credentialPath = process.env.GOOGLE_APPLICATION_CREDENTIALS;
  if (!credentialPath) {
    throw new Error(
      'Set GOOGLE_APPLICATION_CREDENTIALS to a service-account JSON file outside this Flutter project.',
    );
  }
  const resolvedCredential = path.resolve(credentialPath);
  if (!fs.existsSync(resolvedCredential)) {
    throw new Error(`Credential file not found: ${resolvedCredential}`);
  }
  const projectRoot = path.resolve(__dirname, '../..');
  const relativeCredentialPath = path.relative(projectRoot, resolvedCredential);
  if (relativeCredentialPath &&
      !relativeCredentialPath.startsWith(`..${path.sep}`) &&
      relativeCredentialPath !== '..' &&
      !path.isAbsolute(relativeCredentialPath)) {
    throw new Error('Keep the service-account file outside the Flutter project.');
  }

  const { applicationDefault, initializeApp } = require('firebase-admin/app');
  const { getDatabase, ServerValue } = require('firebase-admin/database');
  const app = initializeApp({
    credential: applicationDefault(),
    databaseURL: process.env.FIREBASE_DATABASE_URL || DATABASE_URL,
    databaseAuthVariableOverride: { uid: 'device-simulator' },
  });
  return {
    database: getDatabase(app),
    serverTimestamp: ServerValue.TIMESTAMP,
  };
}

async function main() {
  const { database, serverTimestamp } = initializeFirebaseSimulator();
  const simulator = new DeviceSimulator({ database, serverTimestamp });
  await simulator.start();
  const stop = () => {
    void simulator.stop().finally(() => process.exit(0));
  };
  process.once('SIGINT', stop);
  process.once('SIGTERM', stop);
}

if (require.main === module) {
  main().catch((error) => {
    console.error(`Simulator startup failed: ${error.message}`);
    process.exitCode = 1;
  });
}

module.exports = {
  ADMIN_UID,
  COMMAND_TTL_MS,
  DEVICE_ID,
  DeviceSimulator,
  initializeFirebaseSimulator,
  validateCommand,
};
