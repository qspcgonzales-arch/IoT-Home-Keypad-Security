# Security architecture (Milestone 4: proposed, nothing here is deployed)

Status legend: **[Verified-by-test]** ran in this repository, **[Compiled]** builds but is untested on hardware,
**[Proposed]** design only, **[Unverified]** needs physical hardware or the live Firebase project.

Nothing in this document changes the Flutter app, the deployed `database.rules.json`, or the simulator.

## 1. Decisions applied

1. The ESP32 gets a dedicated, least-privilege Firebase identity. It is not the simulator's and not the administrator's.
2. No Firebase Admin SDK or service-account credential goes into ESP32 firmware.
3. The simulator and production rules are untouched until the proposed rules pass tests **and you approve deployment**.
4. The demonstration lock is designed to be fail-secure with manual release (section 6).

## 2. Device identity and proposed rules

Files: `database.rules.proposed.json` (proposed) and `database.rules.json` (production, unchanged).

| Principal | How it authenticates | Allowed |
| --- | --- | --- |
| Administrator | Existing Firebase Auth uid in the rules | Creates pending `unlock` commands, reads status and audit logs, sets unlock duration (4 to 30 s) |
| `device-simulator` | Admin SDK with `databaseAuthVariableOverride` (existing) | Unchanged contract: gateway/lock state, command transitions, audit creation; `mode` must stay `simulated` |
| Registered ESP32 | Its own Firebase Auth email/password user; uid listed in `deviceRegistry/$uid` with `enabled: true` | Same device writes as the simulator, but `deviceId` must equal its registry `deviceId`; may set `mode` to `hardware`; reads commands only through a bounded query (`status == 'pending'`, `limitToFirst <= 10`) or a single command by ID |
| Anyone else | | Nothing |

Key properties of the proposed rules:

- `deviceRegistry` has no client write rule. Only the Firebase Console or Admin SDK can enrol or revoke a device. A device may read only its own entry.
- **Revocation is immediate**: rules read `deviceRegistry/$uid/enabled` on every request, so setting it to `false` blocks the device even though its ID token stays valid for up to one hour. Also disable the Auth user so it cannot obtain new tokens.
- The administrator uid is never in the registry, and all admin rules use an explicit uid, so a device user cannot act as administrator.
- New in the proposal: `status` index on `commands`; accept requires the command to be unexpired (`expiresAt >= now`); `acceptedAt`, `acknowledgedAt`, `lastSeen` and audit `timestamp` may not be more than 60 s in the future; field-length limits; command fields explicitly validated (see finding below).
- State transitions enforced: `pending -> accepted | failed | expired`, `accepted -> completed | failed | expired`; `type`, `requestedBy`, `createdAt` and `expiresAt` are immutable; deletion is impossible; terminal states are final.
- Limits of what rules can enforce: a device can still claim a false "completed" for a command it is allowed to accept (rules cannot see the door), and per-device path namespaces (`devices/$deviceId`) would change the database contract, so they are future work. The device also shares the global `devices/frontDoor` and `devices/gateway` paths with the simulator, so run only one of them at a time.
- The Flutter app only enables remote unlock when `devices/gateway/mode == 'simulated'`. A real device writing `hardware` will therefore **not** enable unlock in the current app. Changing that is an app change and needs your approval. Until then the real device is read-only-observable in the app but the end-to-end remote unlock stays on the simulator.

### Tests **[Verified-by-test]**

`tools/rules_tests` runs `node --test` against the Firebase Realtime Database emulator (`demo-smart-home`, no network or live project). Run with `npm test` (needs Java 17+ for the emulator).

Result: 17 of 17 passing against `database.rules.proposed.json`. They cover: authorized administrator; unauthorized and unauthenticated users; authorized device; wrong or borrowed device identity; revoked and unregistered identities; immediate revocation; unauthorized state modifications; invalid command updates (tampering, skipped states, expired accept, terminal states, deletion); invalid audit records (missing field, bad type, extra field, oversize, future timestamp, overwrite, delete); and the existing simulator write shapes. The simulator itself was **not** run against the emulator; its write shapes were reproduced from `tools/device_simulator/device_simulator.js` in the test.

### Finding about the production rules file

While building the tests, the administrator's valid command creation, and the simulator's accept/complete updates, were **rejected by the emulator under the current `database.rules.json`**. Cause (by diagnosis): the `commands/$commandId` node declares `"$other": {".validate": false}` but gives no validators for `type`, `status`, `requestedBy`, `createdAt` and `expiresAt`, so those children match `$other` and are rejected. The proposed file adds explicit validators for them, after which all tests pass. This finding comes from the emulator only. The rules deployed to the live project may differ from the file in this repository; I did not read them. Please compare the live rules (Firebase Console, Realtime Database, Rules) with the repository file before relying on either.

### Enrolment and revocation procedure **[Proposed]**

1. Create an Auth user for the board (for example `esp32-frontdoor-1@<your-project>.invalid`) with a long random password; the Console is the only place it is generated.
2. Add `deviceRegistry/<uid> = { enabled: true, deviceId: "esp32-frontdoor-1" }` in the Console.
3. Store the Web API key, email and password only in the untracked `secrets.h`.
4. Revoke: set `enabled` to `false` (immediate), disable the Auth user, and rotate the password. Re-flash with a new credential for the replacement board.

A device-held password is still a secret on flash; anyone with physical access to the board can read it (ESP32 flash encryption is not assumed). That is why the identity is least-privilege and revocable.

## 3. UART, servo and buzzer risk assessment (Uno SoftwareSerial A2/A3, 9600 baud)

Everything here is analysis or host-side checking; **no physical reliability is claimed**.

| # | Risk | Basis | Mitigation in firmware | Status |
| --- | --- | --- | --- | --- |
| 1 | RX buffer overflow: SoftwareSerial buffers 64 bytes (about 67 ms of line time at 9600 baud); a valid UNLOCK line is 117 to 129 bytes, longer than the buffer | Library constant `_SS_MAX_RX_BUFF`; host test computes line size | `loop()` has no blocking calls and drains the link every pass; `link.overflow()` is checked and the line is discarded with `nack malformed-command` | **[Verified-by-test]** for sizes only; real timing **[Unverified]** |
| 2 | Half-duplex: SoftwareSerial cannot receive while transmitting, and each TX byte masks interrupts for about 1 ms | Library behaviour | Replies are short and sent after processing; the ESP32 must wait for the ack before sending anything else (protocol is stop-and-wait) | **[Unverified]** |
| 3 | `Servo` Timer1 interrupts and SoftwareSerial pin-change interrupts can delay each other and corrupt bits | Both are interrupt-driven; the Servo ISR is short but non-zero | 9600 baud (conservative); the 4 s minimum unlock keeps servo moves rare; checksum is absent, so malformed lines are rejected by JSON parsing and validation | **[Unverified]**, test with the servo moving |
| 4 | `tone()` uses Timer2 ISRs and can disturb RX bits | Library behaviour | **Removed**: the buzzer uses `digitalWrite` and `millis()` (active buzzer assumed), no ISR | **[Compiled]** |
| 5 | Blocking in `loop()`: SHA-256 for a PIN check, LCD I2C writes, `keypad.getKey()` | An Uno has no hardware SHA-256; the cost on an Uno is **unmeasured** (estimated tens of ms); LCD writes are about milliseconds | PIN hashing runs only on `#` with four digits; if it exceeds about 60 ms while the ESP32 is sending, an overflow is possible and is handled by discard and nack (the ESP32 retries or fails the command) | **[Unverified]**, measure with `micros()` on hardware |
| 6 | Servo holding torque / jitter while idle | Hobby servo behaviour | Not changed. Detaching the servo when idle was considered and **not** adopted because it could weaken holding behaviour; decide after hardware test | open |
| 7 | Stack and heap exhaustion | 992 bytes remain after static data; stack depth is not measured | `String` and heap use removed; fixed buffers | **[Unverified]** |
| 8 | Floating or noisy ESP32 TX during ESP32 boot | Boot-time garbage on UART pins | Garbage cannot form a valid UNLOCK line (JSON, ID format, TTL and dedup checks); a noisy line produces a `nack` | **[Unverified]** |

Software validation done: `tools/device_simulator/test/firmware_constraints.test.js` (4 tests) checks the worst-case UNLOCK line fits the Uno line buffer, that the firmware has no `String`, `tone()` or `delay()`, and that the TTL, dedup, attempt-limit, lockout and constant-time PIN checks remain in the source. It is a static host-side check and does not execute the firmware.

## 4. Command safety (existing, preserved)

The Uno still enforces `ttlMs` 1 to 60000, unlock duration 4 to 30 s, duplicate suppression (hash of the last 16 IDs), malformed-line rejection, three-attempt keypad lockout for 30 s and PIN hashing with a salt (the PIN is never stored or logged). The placeholder hash is not valid hex, so **no PIN will match until a real salted hash is provisioned at build time**. New: command IDs are restricted to `[A-Za-z0-9_-]` up to 32 chars.

## 5. ESP32 Firebase client implementation plan **[Proposed, not implemented]**

Nothing below is implemented: it cannot be verified without the board, the Wi-Fi network and a live restricted account. Firmware keeps `StubCloudClient`.

| Requirement | Plan |
| --- | --- |
| Authenticate with the restricted identity | `POST https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=<WEB_API_KEY>` with the device email and password; keep `idToken`, `refreshToken`, `expiresIn` in RAM only |
| Refresh tokens | `POST https://securetoken.googleapis.com/v1/token?key=<WEB_API_KEY>` (`grant_type=refresh_token`) about 5 minutes before expiry; on failure fall back to a fresh sign-in; back off exponentially on repeated failures |
| TLS with certificate validation | `WiFiClientSecure` with the Arduino-ESP32 certificate bundle (or the pinned Google Trust Services roots); never `setInsecure()`; SNTP time must be valid before any TLS request (already implemented) |
| Read commands | Poll `GET https://<db>.firebasedatabase.app/commands.json?orderBy="status"&equalTo="pending"&limitToFirst=10` with `Authorization: Bearer <idToken>` every 2 to 3 s. This matches the proposed bounded-read rule and keeps downloads small. Server-sent-events streaming is a later optimization |
| Reconnect safely | Wi-Fi and HTTPS failures use capped exponential backoff with jitter; never act on stale cached commands after a reconnect; re-read time first |
| Ignore expired commands | Compare `expiresAt` with SNTP time (as the current firmware does) and also bound `ttlMs` sent to the Uno to the remaining lifetime; write `expired` if the command can no longer run |
| No duplicate execution | Keep the last N command IDs in RAM (and the current ID in RTC memory across a soft reboot); only commands whose Firebase status is `pending` are processed; send UNLOCK once per ID; the Uno also deduplicates |
| Wait for the Arduino ack | Send `UNLOCK`; wait up to a bounded timeout (suggest 2 s) for `ack` or `nack`; only then write `accepted` (conditional on a still-valid command); on `nack` or timeout write `failed` with a short message. Note the simulator writes `accepted` first, so this is an intentional ordering difference to retest |
| Report actual lock status | Write `devices/frontDoor/isLocked` only from Uno `STATE` messages (`locked` or `unlocked`); write `completed` after `STATE unlocked` from `source: remote`; never infer state from a command alone; mark `isOnline` and `lastSeen` (server timestamp) as a heartbeat |
| Audit | Map Uno `EVENT` messages to `auditLogs` entries using the existing allowed types; one create per event, never overwrite |
| Never expose credentials | No logging of tokens, the password or the API key; redact request URLs in logs; keep them in the untracked `secrets.h`; do not print on serial in release builds |
| Fail safe | Cloud loss never unlocks; the Uno keeps its own keypad, lockout and relock timer |

## 6. Mechanical fail-secure demonstration design **[Proposed, Unverified]**

**Intent:** with power removed the door stays locked; a person can always open it manually.

- **Why servo position alone is not fail-secure.** The firmware commands a servo angle. Once power is removed, an SG90 holds no torque; a plastic gear train can be back-driven by hand or by a small force, and the horn stays wherever it was left. If the horn was at the unlocked angle when power failed, the lock stays open; if it was at the locked angle, a modest force can open it. Firmware does not run during power loss, so no code can set a safe state. Brown-outs and resets can also move the servo momentarily.
- **What happens when electrical power is removed (intended design).** The latch or bolt is held in the locked position by a spring or by gravity, and the servo (or a solenoid) only retracts it while powered. When the servo supply drops, the spring returns the bolt to locked, so the door is secure. This depends on the mechanism, which does not exist yet; it is a design intent, not an observed behaviour.
- **Why a manual mechanical release is needed.** A fail-secure door with no manual release traps people inside during a power failure or a fault, which is a life-safety hazard and the reason real building egress hardware requires free mechanical egress. The prototype must let a person open the door from inside (and optionally by key outside) with no electricity and no firmware. This prototype is a demonstration, not a certified security or life-safety product.
- **Prototype guidance.** Use a spring-return latch or a purpose-built lock; use the servo only to move a bolt or slide that is spring-biased to locked; drive the servo from its own supply with a common ground; keep the keypad and firmware logic as is.
- **Tests that remain impossible without the mechanism:** removing power while locked and while unlocked; back-drive and tamper force on the bolt; manual-release force and operation with the door under load; servo stall current and brown-out/reset behaviour; spring return reliability over cycles; behaviour when the servo supply drops but the Uno stays powered (and the reverse); operation after repeated power cycles. These are listed in `HARDWARE_TEST_PLAN.md`.

## 6a. Milestone 4.5 - rules root cause and verification (supersedes the earlier "live rules not inspected" note)

**Live vs repository.** A read-only fetch (`npx firebase database:get /.settings/rules`, wrapped by
`tools/rules_tests/compare_live_rules.js`) showed the live rules are **identical** to `database.rules.json`.
Live data is consistent with the defect: `/commands` is empty, while `devices/gateway` and keypad audit
entries exist (those paths do not pass through the command validators). To re-check without any write, use the
Console Rules Playground. The root cause below is proven in the emulator only.

**Root cause.** Under `commands/$commandId`, the production rules declare validators only for `acceptedAt`,
`acknowledgedAt`, `deviceId`, `message` and a catch-all `$other: {".validate": false}`. The fields `type`,
`status`, `requestedBy`, `createdAt`, `expiresAt` have no explicit validator, so they match `$other` and every
write that touches them is rejected. Bisection (`tools/rules_tests/diagnose_rules.js`):

| Operation | Fields written | Why rejected | Fix |
|---|---|---|---|
| Admin creates command | all five | all hit `$other` | all five validators (or removing `$other`) |
| Simulator pending?accepted | `status`, `acceptedAt`, `deviceId` | `status` hits `$other` | `status` validator |
| Simulator accepted?completed | `status`, `acknowledgedAt`, `message` | `status` hits `$other` | `status` validator |

**Does the proposed file address it?** Yes: it adds explicit validators for the five fields (17/17 rules tests).
It also adds the device registry, bounded device reads, expiry/clock-skew checks and a `mode` restriction
(see section 3). Admin reads of `commands` are unchanged.

**Lifecycle verification** (`tools/rules_tests/test/lifecycle.test.js`, emulator, real `DeviceSimulator` via
firebase-admin with a database auth override so rules apply): with the proposed rules, an admin command with a
server-timestamp `createdAt` is created, accepted, completed, `isLocked` goes true?false?true as seen by a
Flutter-style single-leaf listener, audit `remoteUnlock` and `automaticRelock` entries are written, a stranger
cannot create commands, write `isLocked`, `devices/gateway` or audit logs, terminal commands cannot be
reverted, an expired command is rejected without unlocking, and a replayed command id does not unlock again
(that last check exercises the simulator's in-memory dedup, not the rules). Under the production rules, the
same admin create fails and a seeded pending command stays `pending`. **Not covered:** the Dart service itself
was not run against the emulator.

**Conflicts with existing behaviour.** Under the live rules `mode` must be `simulated`; the proposed rules keep
that for `device-simulator`. Hardware `mode` is accepted only from a registered device, so hardware mode in the
app stays inert until both the rules and a registered device exist.

## 7. Open approvals

1. Deploy `database.rules.proposed.json` (not done, awaiting your approval and a check of the live rules).
2. Allow the app to treat `mode == 'hardware'` as eligible for remote unlock (app change, not made).
3. Create the ESP32 Auth user and registry entry in the Firebase Console (needs your Console access).
4. Confirm the buzzer is an active type.
