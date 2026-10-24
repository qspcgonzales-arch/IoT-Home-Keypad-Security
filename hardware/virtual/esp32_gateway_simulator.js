'use strict';

const { DeviceSimulator, validateCommand } = require('../../tools/device_simulator/device_simulator');

/** Virtual ESP32/UART bridge. Firebase identity and command validation match Milestone 3. */
class VirtualEsp32Gateway extends DeviceSimulator {
  constructor({ arduino, ...options }) {
    super(options);
    if (!arduino) throw new Error('A virtual Arduino module is required.');
    this.arduino = arduino;
    this.wifiConnected = true;
    this.queuedCommands = new Map();
    this.hardwareEvents = [];
    this.pendingUartWrites = [];
    this.lastRemoteCommand = null;
    this.unlockDurationSeconds = 4;
    this.settingsListener = null;
    this.arduino.writeUart = (line) => {
      this.pendingUartWrites.push(this._onUartLine(line));
    };
    this.arduino.onEvent = (event) => { void this._onArduinoEvent(event); };
  }

  async setWifiConnected(connected) {
    this.wifiConnected = Boolean(connected);
    if (!this.wifiConnected) {
      this.online = false;
      await this.database.ref('devices/gateway').update({ isOnline: false, mode: 'simulated' });
      this._log('Wi-Fi disconnected; keypad remains available locally.');
      return;
    }
    this._log('Wi-Fi reconnected; checking queued Firebase commands.');
    try {
      const settingSnapshot = await this.database
        .ref('settings/frontDoor/unlockDurationSeconds')
        .once('value');
      this.applyUnlockDuration(settingSnapshot.val());
    } catch (error) {
      this._log(`Could not refresh unlock duration after reconnect; using ${this.unlockDurationSeconds} seconds: ${error.message}`);
    }
    await this._writeHeartbeat();
    const snapshot = await this.database.ref('commands').once('value');
    const rows = snapshot.val() || {};
    for (const [id, command] of Object.entries(rows)) {
      if (command?.status === 'pending') await this.handleCommand(id, command);
    }
    for (const event of this.hardwareEvents.splice(0)) await this._onArduinoEvent(event);
  }

  async start() {
    await super.start();
    const settingRef = this.database.ref('settings/frontDoor/unlockDurationSeconds');
    this.settingsListener = (snapshot) => {
      this.applyUnlockDuration(snapshot?.val());
    };
    settingRef.on('value', this.settingsListener, (error) => {
      this._log(`Unlock duration setting listener failed: ${error?.message || error}`);
    });
    try {
      const snapshot = await settingRef.once('value');
      this.applyUnlockDuration(snapshot.val());
    } catch (error) {
      this._log(`Could not read unlock duration; using 4 seconds: ${error.message}`);
    }
    this.hardwareTimer = setInterval(() => this.arduino.tick(this.now()), 50);
    this.hardwareTimer.unref?.();
  }

  async stop() {
    if (this.hardwareTimer) clearInterval(this.hardwareTimer);
    this.hardwareTimer = null;
    if (this.settingsListener) {
      this.database.ref('settings/frontDoor/unlockDurationSeconds').off('value', this.settingsListener);
      this.settingsListener = null;
    }
    return super.stop();
  }

  applyUnlockDuration(value) {
    if (typeof value !== 'number' || !Number.isInteger(value) || value < 4 || value > 30) {
      this.unlockDurationSeconds = 4;
      this.arduino.setUnlockDurationMs(4_000);
      return false;
    }
    this.unlockDurationSeconds = value;
    this.arduino.setUnlockDurationMs(value * 1_000);
    this._log(`Automatic relock duration set to ${value} seconds.`);
    return true;
  }

  async _writeHeartbeat() {
    if (!this.running || !this.wifiConnected) {
      this.online = false;
      return;
    }
    return super._writeHeartbeat();
  }

  async handleCommand(commandId, command) {
    if (!this.wifiConnected) {
      this.queuedCommands.set(commandId, command);
      return { status: 'queued-offline' };
    }
    if (this.inFlight.has(commandId) || this.processed.has(commandId)) {
      this._log(`Duplicate command ${commandId} ignored.`);
      return { status: 'duplicate' };
    }
    const validation = validateCommand(command, { adminUid: this.adminUid, now: this.now() });
    if (!validation.ok) {
      await this._rejectCommand(commandId, command, validation.status, validation.message);
      this.processed.add(commandId);
      return { status: validation.status };
    }
    if (!this.online || this.busy) {
      const message = !this.online ? 'Gateway is offline.' : 'Device is busy.';
      await this._rejectCommand(commandId, command, 'failed', message);
      this.processed.add(commandId);
      return { status: 'failed', message };
    }
    this.inFlight.add(commandId);
    // Read current policy at execution time to avoid a stale listener race.
    try {
      const settingSnapshot = await this.database
        .ref('settings/frontDoor/unlockDurationSeconds')
        .once('value');
      this.applyUnlockDuration(settingSnapshot.val());
    } catch (error) {
      this._log(`Could not refresh unlock duration; using current ${this.unlockDurationSeconds} seconds: ${error.message}`);
    }
    this.busy = true;
    this.currentCommand = { id: commandId, record: command };
    await this.database.ref(`commands/${commandId}`).update({
      status: 'accepted', acceptedAt: this.serverTimestamp, deviceId: this.deviceId,
    });
    this.arduino.handleUartLine(JSON.stringify({
      type: 'UNLOCK', commandId, expiresAt: command.expiresAt,
      ttlMs: Math.max(0, Math.min(60_000, command.expiresAt - this.now())),
      unlockDurationMs: this.unlockDurationSeconds * 1_000,
    }));
    await Promise.all(this.pendingUartWrites.splice(0));
    return { status: 'accepted' };
  }

  async _onUartLine(line) {
    let message;
    try { message = JSON.parse(line); } catch (_) { return; }
    if (message.type === 'nack') {
      const current = this.currentCommand;
      if (current && message.commandId === current.id) {
        await this._rejectCommand(current.id, current.record,
          message.reason === 'expired' ? 'expired' : 'failed', message.reason);
        this._finishCurrent();
      }
      return;
    }
    if (message.type === 'ack' && message.status === 'accepted') {
      this._log(`UART acknowledged command ${message.commandId}.`);
      return;
    }
    if (message.type === 'ack' && message.status === 'executed') {
      const current = this.currentCommand;
      if (!current || message.commandId !== current.id) {
        this._log(`Ignored execution acknowledgement for inactive command ${message.commandId}.`);
        return;
      }
      await this.database.ref(`commands/${current.id}`).update({
        status: 'completed', acknowledgedAt: this.serverTimestamp,
        message: 'Virtual Uno acknowledged command execution.',
      });
      this.lastRemoteCommand = current;
      this._finishCurrent();
      return;
    }
    if (message.type === 'STATE' &&
        (message.door === 'locked' || message.door === 'unlocked')) {
      await this.database.ref('devices/frontDoor/isLocked')
        .set(message.door === 'locked');
    }
  }

  async _onArduinoEvent(event) {
    if (!this.wifiConnected) {
      this.hardwareEvents.push(event);
      return;
    }
    if (event.type === 'doorUnlocked' && event.source === 'remote') {
      await this.database.ref('devices/frontDoor/isLocked').set(false);
      const command = this.currentCommand;
      if (command) await this._writeAuditEvent({
        type: 'remoteUnlock', description: 'Virtual Uno accepted remote unlock via UART.',
        commandId: command.id, actorUid: command.record.requestedBy,
      });
    } else if (event.type === 'automaticRelock') {
      await this.database.ref('devices/frontDoor/isLocked').set(true);
      const command = this.currentCommand || this.lastRemoteCommand;
      if (command) {
        await this._writeAuditEvent({
          type: 'automaticRelock', description: `Virtual Uno automatically relocked after ${this.unlockDurationSeconds} seconds.`,
          commandId: command.id, actorUid: command.record.requestedBy,
        });
        if (this.lastRemoteCommand === command) this.lastRemoteCommand = null;
        if (this.currentCommand === command) this._finishCurrent();
      } else if (event.source === 'timer') {
        await this._writeAuditEvent({
          type: 'automaticRelock', description: `Local keypad access automatically relocked after ${this.unlockDurationSeconds} seconds.`,
          commandId: 'local-keypad', actorUid: 'local-keypad',
        });
      }
    } else if (['accessGranted', 'accessDenied', 'securityLockout'].includes(event.type)) {
      await this._publishLocalEvent(event);
      if (event.type === 'accessGranted') {
        await this.database.ref('devices/frontDoor/isLocked').set(false);
      }
    }
  }

  async _publishLocalEvent(event) {
    await this._writeAuditEvent({
      type: event.type,
      description: event.type === 'accessGranted' ? 'Local keypad access granted.'
        : event.type === 'securityLockout' ? 'Keypad locked for 30 seconds after three invalid PIN attempts.'
          : 'Local keypad access denied.',
      commandId: 'local-keypad', actorUid: 'local-keypad',
    });
    if (event.type === 'accessGranted') {
      await this.database.ref('devices/frontDoor/isLocked').set(false);
    }
  }

  _finishCurrent() {
    if (this.currentCommand) {
      this.inFlight.delete(this.currentCommand.id);
      this.processed.add(this.currentCommand.id);
    }
    this.currentCommand = null;
    this.busy = false;
  }
}

function startVirtualGateway() {
  const { initializeFirebaseSimulator } = require('../../tools/device_simulator/device_simulator');
  const { VirtualArduino } = require('./arduino_simulator');
  const pinSalt = process.env.KEYPAD_PIN_SALT;
  const pinHash = process.env.KEYPAD_PIN_SHA256;
  if (!pinSalt || !pinHash || !/^[a-f0-9]{64}$/i.test(pinHash)) {
    throw new Error('Set KEYPAD_PIN_SALT and KEYPAD_PIN_SHA256 (SHA-256 of salt:PIN); never set a plaintext PIN.');
  }
  const { database, serverTimestamp } = initializeFirebaseSimulator();
  const arduino = new VirtualArduino({
    pinSalt, pinHash, now: Date.now,
    onEvent: () => {},
  });
  const gateway = new VirtualEsp32Gateway({ database, serverTimestamp, arduino });
  void gateway.start().catch((error) => {
    console.error(`Virtual gateway startup failed: ${error.message}`);
    process.exitCode = 1;
  });
  const readline = require('node:readline');
  const input = readline.createInterface({ input: process.stdin, output: process.stdout });
  console.log('Virtual controls: /pin 1234, /wifi off|on, /ir on|off, /quit');
  input.on('line', (line) => {
    const [command, value] = line.trim().split(/\s+/, 2);
    if (command === '/pin') console.log(gateway.arduino.enterPin(value || ''));
    else if (command === '/wifi') void gateway.setWifiConnected(value === 'on');
    else if (command === '/ir') gateway.arduino.detectProximity(value === 'on');
    else if (command === '/quit') process.emit('SIGINT');
  });
  const stop = () => { void gateway.stop().finally(() => process.exit(0)); };
  process.once('SIGINT', stop);
  process.once('SIGTERM', stop);
  return gateway;
}

if (require.main === module) {
  try { startVirtualGateway(); } catch (error) {
    console.error(`Virtual gateway startup failed: ${error.message}`);
    process.exitCode = 1;
  }
}

module.exports = { VirtualEsp32Gateway, startVirtualGateway };
