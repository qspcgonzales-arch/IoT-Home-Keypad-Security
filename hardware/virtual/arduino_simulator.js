'use strict';

const crypto = require('node:crypto');

const DEFAULT_UNLOCK_MS = 4_000;
const MIN_UNLOCK_MS = 4_000;
const MAX_UNLOCK_MS = 30_000;
const UNLOCK_MS = DEFAULT_UNLOCK_MS;
const LOCKOUT_MS = 30_000;
const MAX_ATTEMPTS = 3;

function pinDigest(pin, salt) {
  return crypto.createHash('sha256').update(`${salt}:${pin}`).digest('hex');
}

/** Virtual keypad/Uno model. The PIN is only compared locally and never emitted. */
class VirtualArduino {
  constructor({
    pinHash,
    pinSalt,
    onEvent = () => {},
    now = () => 0,
    writeUart = () => {},
    unlockDurationMs = DEFAULT_UNLOCK_MS,
  }) {
    if (!pinHash || !pinSalt) {
      throw new Error('Configure a PIN hash and salt; plaintext PINs are not accepted.');
    }
    this.pinHash = pinHash;
    this.pinSalt = pinSalt;
    this.onEvent = onEvent;
    this.now = now;
    this.writeUart = writeUart;
    this.setUnlockDurationMs(unlockDurationMs);
    this.locked = true;
    this.lockoutUntil = 0;
    this.failedAttempts = 0;
    this.relockAt = null;
    this.lcdMessage = 'READY - ENTER PIN';
    this.buzzerEvents = [];
    this.lastProximity = null;
    this.completedCommands = new Set();
  }

  enterPin(pin) {
    const time = this.now();
    this.tick(time);
    if (time < this.lockoutUntil) {
      this.lcdMessage = 'KEYPAD LOCKED';
      this.buzzerEvents.push('denied');
      return { accepted: false, reason: 'lockout' };
    }
    if (typeof pin !== 'string' || !/^\d{4}$/.test(pin)) {
      return this._deny('PIN MUST BE 4 DIGITS');
    }
    const actual = Buffer.from(pinDigest(pin, this.pinSalt));
    const expected = Buffer.from(this.pinHash);
    const valid = actual.length === expected.length && crypto.timingSafeEqual(actual, expected);
    if (!valid) return this._deny('ACCESS DENIED');
    this.failedAttempts = 0;
    this.unlock('keypad');
    this.onEvent({ type: 'accessGranted', source: 'keypad', timestamp: time });
    return { accepted: true };
  }

  _deny(message) {
    this.failedAttempts += 1;
    this.lcdMessage = message;
    this.buzzerEvents.push('denied');
    this.onEvent({ type: 'accessDenied', source: 'keypad', timestamp: this.now() });
    if (this.failedAttempts >= MAX_ATTEMPTS) {
      this.lockoutUntil = this.now() + LOCKOUT_MS;
      this.failedAttempts = 0;
      this.lcdMessage = 'SECURITY LOCKOUT';
      this.buzzerEvents.push('lockout');
      this.onEvent({ type: 'securityLockout', source: 'keypad', timestamp: this.now() });
    }
    return { accepted: false, reason: message };
  }

  unlock(source = 'remote', durationMs = this.unlockDurationMs) {
    this.locked = false;
    this.relockAt = this.now() + durationMs;
    this.lcdMessage = 'DOOR UNLOCKED';
    this.buzzerEvents.push('unlock');
    this.onEvent({ type: 'doorUnlocked', source, timestamp: this.now() });
  }

  setUnlockDurationMs(durationMs) {
    if (!Number.isInteger(durationMs) || durationMs % 1_000 !== 0 ||
        durationMs < MIN_UNLOCK_MS || durationMs > MAX_UNLOCK_MS) {
      throw new RangeError('Unlock duration must be a whole number from 4 to 30 seconds.');
    }
    this.unlockDurationMs = durationMs;
  }

  tick(now = this.now()) {
    if (!this.locked && this.relockAt !== null && now >= this.relockAt) {
      this.locked = true;
      this.relockAt = null;
      this.lcdMessage = 'DOOR RELocked'.toUpperCase();
      this.buzzerEvents.push('relock');
      this.onEvent({ type: 'automaticRelock', source: 'timer', timestamp: now });
      this.writeUart(JSON.stringify({
        type: 'STATE',
        door: 'locked',
        source: 'timer',
      }));
      this.writeUart(JSON.stringify({
        type: 'EVENT',
        event: 'automaticRelock',
        source: 'timer',
        timestamp: now,
      }));
    }
    if (this.lockoutUntil && now >= this.lockoutUntil) {
      this.lockoutUntil = 0;
      this.failedAttempts = 0;
      this.lcdMessage = 'READY - ENTER PIN';
      this.onEvent({ type: 'keypadReady', source: 'timer', timestamp: now });
    }
  }

  detectProximity(present) {
    if (typeof present !== 'boolean' || present === this.lastProximity) return;
    this.lastProximity = present;
    this.buzzerEvents.push(present ? 'proximity' : 'proximity-clear');
    this.onEvent({ type: 'irProximity', present, timestamp: this.now() });
  }

  handleUartLine(line) {
    let message;
    try { message = JSON.parse(line); } catch (_) {
      this.writeUart(JSON.stringify({ type: 'nack', reason: 'malformed-json' }));
      return { accepted: false, reason: 'malformed-json' };
    }
    if (message?.type === 'PING') {
      if (typeof message.pingId !== 'string' ||
          !/^[A-Za-z0-9_-]{1,32}$/.test(message.pingId)) {
        this.writeUart(JSON.stringify({ type: 'nack', reason: 'malformed-command' }));
        return { accepted: false, reason: 'malformed-command' };
      }
      this.writeUart(JSON.stringify({
        type: 'PONG',
        pingId: message.pingId,
        door: this.locked ? 'locked' : 'unlocked',
      }));
      return { accepted: true };
    }
    const ttlValid = Number.isFinite(message?.ttlMs) &&
      Number.isInteger(message.ttlMs) && message.ttlMs >= 0 &&
      message.ttlMs <= 60_000;
    const durationValid = message?.unlockDurationMs === undefined ||
      (Number.isInteger(message.unlockDurationMs) && message.unlockDurationMs >= MIN_UNLOCK_MS && message.unlockDurationMs <= MAX_UNLOCK_MS);
    const validId = typeof message?.commandId === 'string' &&
      /^[A-Za-z0-9_-]{1,32}$/.test(message.commandId);
    if (message?.type !== 'UNLOCK' || !validId || !ttlValid || !durationValid) {
      this.writeUart(JSON.stringify({ type: 'nack', commandId: message?.commandId, reason: 'malformed-command' }));
      return { accepted: false, reason: 'malformed-command' };
    }
    if (this.completedCommands.has(message.commandId)) {
      this.writeUart(JSON.stringify({ type: 'nack', commandId: message.commandId, reason: 'duplicate' }));
      return { accepted: false, reason: 'duplicate' };
    }
    if (message.ttlMs <= 0) {
      this.writeUart(JSON.stringify({ type: 'nack', commandId: message.commandId, reason: 'expired' }));
      return { accepted: false, reason: 'expired' };
    }
    this.writeUart(JSON.stringify({ type: 'ack', commandId: message.commandId, status: 'accepted' }));
    this.completedCommands.add(message.commandId);
    while (this.completedCommands.size > DEDUP_COMMANDS) {
      this.completedCommands.delete(this.completedCommands.values().next().value);
    }
    this.unlock('remote', message.unlockDurationMs);
    this.writeUart(JSON.stringify({
      type: 'STATE',
      door: 'unlocked',
      source: 'remote',
    }));
    this.writeUart(JSON.stringify({ type: 'ack', commandId: message.commandId, status: 'executed' }));
    return { accepted: true };
  }
}

const DEDUP_COMMANDS = 16;

module.exports = { DEFAULT_UNLOCK_MS, LOCKOUT_MS, MAX_ATTEMPTS, MAX_UNLOCK_MS, MIN_UNLOCK_MS, UNLOCK_MS, VirtualArduino, pinDigest };
