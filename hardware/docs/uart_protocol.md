# UART protocol, provisional version 1

Both directions use one UTF-8 JSON object per line, terminated by `\n`. The
protocol carries no PIN or credential.

## ESP32 to Uno

```json
{"type":"UNLOCK","commandId":"<Firebase push key>","expiresAt":1792600000000,"ttlMs":30000,"unlockDurationMs":8000}
```

The ESP32 rejects commands whose absolute `expiresAt` has passed and computes
the remaining `ttlMs`. Because an Uno has no trusted wall clock, it accepts only
a non-empty ID, `ttlMs` from 1 through 60000, an `unlockDurationMs` from 4000
through 30000 in whole-second increments, and an ID not seen since boot. The
duration is selected from the administrator-only RTDB setting
`settings/frontDoor/unlockDurationSeconds`; it is not supplied by the app's
command request. The Uno applies this duration to keypad and remote unlocks. It
replies `{"type":"ack","commandId":"...","status":"accepted"}`
before operating the servo. It replies with `type:nack` for malformed, expired,
or duplicate requests. IDs are retained in RAM for the current boot session.

## Uno to ESP32

Replies use the `ack` or `nack` format above. State events are:

```json
{"type":"STATE","door":"locked|unlocked","source":"keypad|remote|timer"}
{"type":"EVENT","event":"accessGranted|accessDenied|securityLockout|automaticRelock|irProximity"}
```

No PIN, PIN hash, Wi-Fi secret, Firebase credential, or service-account data may
be sent over UART. The gateway maps completed Firebase commands and hardware
events to the existing `devices/frontDoor`, `commands`, and `auditLogs` schema.

## Timing and recovery

The Uno owns the configurable 4–30 second relock timer (default 4 seconds) and
three-attempt, 30-second keypad lockout. Both use non-blocking `millis()` state. Remote command expiry is
checked again by the Uno. The ESP32 deduplicates Firebase command IDs and rejects
pending commands that expired while Wi-Fi was unavailable.

This is a provisional protocol for software and firmware alignment. Validate
electrical levels, framing, and recovery behavior on real hardware.

## Physical link (provisional, unverified)

9600 baud, 8N1. The Uno uses `SoftwareSerial` on A2 (RX) and A3 (TX) so USB
upload on D0/D1 keeps working; the ESP32 uses UART2 on GPIO16 (RX) and GPIO17
(TX). Uno TX must go through a level shifter. Pins and rationale are in
`ARCHITECTURE.md`. The message format above is unchanged, so the virtual
Uno/ESP32 simulators remain compatible.

## Milestone 4 changes (Uno firmware, same wire format)

- The wire format is unchanged. `commandId` must now match `[A-Za-z0-9_-]` and be at most 32 characters; otherwise the Uno replies `nack` with `malformed-command` and no `commandId`.
- A valid UNLOCK line is at most 129 bytes; the Uno line buffer holds 143. A longer line, or a `SoftwareSerial` overflow, discards the whole line and returns `nack` `malformed-command`.
- The Uno hashes the `commandId` (32-bit FNV-1a) for its last-16 duplicate check.
- The protocol is stop-and-wait: the ESP32 must wait for `ack`/`nack` (suggested timeout 2 s) before sending another command. The Uno cannot receive while it transmits, so the ESP32 should also tolerate `STATE` and `EVENT` lines arriving between replies.
- Proposed v1.1, not implemented: a `STATUS`/`PING` request so the ESP32 can read the real door state after a reconnect.
- Everything above is compiled and host-checked only; UART timing on the real SoftwareSerial link is unverified (see `SECURITY_ARCHITECTURE.md` section 3).

## Milestone 4.5 - compatibility review (source-level only)

Checked by `tools/device_simulator/test/uart_contract.test.js` (static source checks; no hardware):

- Baud 9600 on both ends; Uno `LINK_RX_PIN=A2`/`LINK_TX_PIN=A3`; ESP32 `UNO_RX_PIN=16`/`UNO_TX_PIN=17`.
- `commandId` rule `[A-Za-z0-9_-]`, max 32: enforced by the Uno and now pre-checked by the ESP32 (`validCommandId`). Firebase push IDs (20 chars) comply.
- Nack reasons, now aligned in the Uno: `duplicate`; `expired` only when `ttlMs == 0`; `malformed-command` for bad JSON/fields, `ttlMs > 60000`, duration outside 4000-30000 ms or not a whole second.
- Status lifecycle: simulator and ESP32 both write `accepted` then `completed`/`failed`/`expired`, all legal under the proposed rules. The virtual gateway writes `accepted` before sending UNLOCK; the planned real client waits for the Uno `ack`.

Known remaining differences (virtual Uno, `hardware/virtual/arduino_simulator.js`, deliberately unchanged):
it replies `malformed-json` for unparseable JSON, accepts `expiresAt` without `ttlMs`, treats `ttlMs <= 0` as malformed, does not validate command-id format, and keeps an unbounded dedup set (firmware: last 16 FNV hashes, lost on reboot; ESP32: last 32 ids). Physical UART, servo, keypad and sensor behaviour is **untested**.