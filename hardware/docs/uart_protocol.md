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
replies `{"type":"ack","commandId":"...","status":"accepted"}` before issuing
the servo command, publishes `STATE unlocked`, then replies with
`{"type":"ack","commandId":"...","status":"executed"}`. `executed` confirms
that the firmware issued the servo command; it does not prove the latch moved.
It replies with `type:nack` for malformed, expired, or duplicate requests. IDs
are retained in RAM for the current boot session.

## Uno to ESP32

Replies use the `ack` or `nack` format above. State events are:

```json
{"type":"STATE","door":"locked|unlocked","source":"keypad|remote|timer"}
{"type":"EVENT","event":"accessGranted|accessDenied|securityLockout|automaticRelock|irProximity"}
```

The ESP32 sends `{"type":"PING","pingId":"..."}` once per second. A matching
`PONG` includes the Uno's logical door state and is required for a fresh UART
ready heartbeat. The Uno also publishes `STATE locked` when its relock timer
fires, followed by an `automaticRelock` event.

No PIN, PIN hash, Wi-Fi secret, Firebase credential, or service-account data may
be sent over UART. The gateway maps completed Firebase commands and hardware
events to the existing `devices/frontDoor`, `commands`, and `auditLogs` schema.

## Timing and recovery

The Uno owns the configurable 4–30 second relock timer (default 4 seconds) and
three-attempt, 30-second keypad lockout. Both use non-blocking `millis()` state. Remote command expiry is
checked again by the Uno. The ESP32 deduplicates Firebase command IDs and rejects
pending commands that expired while Wi-Fi was unavailable.

The ESP32 claims a Firebase command before UART transmission, accepts no more
than one command at a time, and does not replay a command after a restart. If
execution acknowledgement is lost, it records failure rather than retransmit
an unlock. The ESP32 persists an in-flight command and a bounded audit outbox
in NVS for recovery. These are software policies; validate framing and failure
recovery on real hardware.

## Physical link (provisional, unverified)

9600 baud, 8N1. The Uno uses `SoftwareSerial` on A2 (RX) and A3 (TX) so USB
upload on D0/D1 keeps working; the ESP32 uses UART2 on GPIO16 (RX) and GPIO17
(TX). Uno TX must go through a level shifter. Pins and rationale are in
`ARCHITECTURE.md`. The message format above is unchanged, so the virtual
Uno/ESP32 simulators remain compatible.

## Firmware contract checks

- `commandId` must match `[A-Za-z0-9_-]` and be at most 32 characters; otherwise the Uno replies `nack` with `malformed-command` and no `commandId`.
- A valid UNLOCK line is at most 129 bytes; the Uno line buffer holds 143. A longer line, or a `SoftwareSerial` overflow, discards the whole line and returns `nack` `malformed-command`.
- The Uno hashes the `commandId` (32-bit FNV-1a) for its last-16 duplicate check.
- The protocol is stop-and-wait: the ESP32 waits for the execution acknowledgement before it can claim another command. The Uno can emit `STATE` and `EVENT` lines between replies.
- `PING`/`PONG` is implemented in the Uno and ESP32 firmware to verify recent UART communication and read the Uno's logical door state.
- The execution acknowledgement follows the servo command request; it does not verify actual servo position or door/latch movement.
- Firmware compilation and UART timing remain subject to validation; all physical electrical and actuator behavior is unverified.

## Milestone 4.5 - compatibility review (source-level only)

Checked by `tools/device_simulator/test/uart_contract.test.js` (static source checks; no hardware):

- Baud 9600 on both ends; Uno `LINK_RX_PIN=A2`/`LINK_TX_PIN=A3`; ESP32 `UNO_RX_PIN=16`/`UNO_TX_PIN=17`.
- `commandId` rule `[A-Za-z0-9_-]`, max 32: enforced by the Uno and now pre-checked by the ESP32 (`validCommandId`). Firebase push IDs (20 chars) comply.
- Nack reasons, now aligned in the Uno: `duplicate`; `expired` only when `ttlMs == 0`; `malformed-command` for bad JSON/fields, `ttlMs > 60000`, duration outside 4000-30000 ms or not a whole second.
- Status lifecycle: simulator and ESP32 both write `accepted` then `completed`/`failed`/`expired`, all legal under the proposed rules. The virtual gateway writes `accepted` before sending UNLOCK; the planned real client waits for the Uno `ack`.

The virtual Uno and gateway model the same accepted → executed acknowledgement order and the `PING`/`PONG` logical-state exchange. The virtual command dedup set is bounded to 16 IDs; the physical Uno keeps the last 16 FNV hashes since boot. Physical UART, servo, keypad and sensor behavior is **untested**.