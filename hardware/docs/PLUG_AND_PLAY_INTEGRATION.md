# Plug-and-Play Hardware Integration

## Status and safety boundary

Milestone 6 source integration is implemented for the Uno R3 + ESP32 gateway.
This is **not a claim of physical readiness**: no board, wiring, servo, Firebase
device account, or production rules were available/activated during this work.
Firmware defaults remain inert without a local `secrets.h`; emulator mode is
explicit; Flutter defaults to its existing simulator mode. Do not deploy rules,
create production credentials, or connect a latch without separate approval.

## Compatibility matrix

| Contract | Firebase / proposed rules | Virtual Uno + gateway | Physical Uno + ESP32 source | Flutter app | Compatibility |
| --- | --- | --- | --- | --- | --- |
| Command type and record | `commands/<push-id>` has lowercase `type: "unlock"`, `status: "pending"`, `requestedBy`, `createdAt`, `expiresAt` | Validates the record and translates to UART `type: "UNLOCK"` | REST client validates the record and sends UART `UNLOCK`; it does not pass cloud credentials to Uno | Creates the existing lowercase Firebase command | **Matched via gateway translation** |
| Command IDs | Firebase push IDs are accepted; proposed rules validate lifecycle, not UART syntax | Virtual Uno accepts IDs matching `[A-Za-z0-9_-]`, at most 32 characters; keeps 16 IDs | ESP32 pre-validates the ID; Uno applies the same format/length and keeps 16 FNV hashes in RAM | Uses Firebase push IDs, which fit the limit | **Matched; physical Uno dedup state is lost on reboot** |
| Expiry | Cloud command has numeric `createdAt` and `expiresAt`; max lifetime is 60 seconds | Gateway validates expiry and computes a remaining `ttlMs` | ESP32 requires valid SNTP time, validates requester and lifetime, then sends remaining `ttlMs`; Uno rejects zero or >60 seconds | Writes expiry using the existing command lifetime | **Matched; Uno itself has no trusted wall clock** |
| Acknowledgements | Statuses `pending → accepted → completed/failed/expired`; accepted and terminal metadata are validated | Writes `accepted` before UART, then `completed` after Uno `executed` ACK; failures map to terminal state | Claims `accepted` before UART; requires matching `accepted` ACK, remote `STATE unlocked`, then `executed` ACK before `completed`; timeout is not replayed | Shows queued, accepted, executed, failed, or expired; does not write device acknowledgement | **Matched with explicit `accepted` vs `completed` stages** |
| Device identity | Proposed `deviceRegistry/<uid> = { enabled, deviceId }`; registry is provisioned outside clients | Local simulator uses its separate simulator identity / Admin SDK override | ESP32 signs in as a dedicated Email/Password Auth user; checks its own enabled registry entry; no Admin SDK key | Uses administrator Auth only; rules enforce device authorization | **Different identities by design** |
| Door status | `devices/frontDoor/isLocked` is a boolean | Virtual Uno state callback publishes the logical state | ESP32 publishes the Uno's `STATE` value; Firebase state is not a sensor reading of a physical latch | Reads door state; does not write it | **Path and type matched; physical truth unverified** |
| Gateway / Arduino readiness | `devices/gateway/{isOnline,lastSeen,mode,arduinoConnected,arduinoLastSeen}` in proposed rules | Simulator reports `mode: simulated`; no physical UART | ESP32 writes `mode: hardware`, gateway heartbeat, and fresh PING/PONG-derived Arduino heartbeat | Hardware build requires matching mode, fresh gateway heartbeat, and fresh positive Arduino heartbeat | **Matched in proposed rules; production rules remain unchanged** |
| Audit events | `auditLogs/<id>` validates `type`, `description`, `timestamp`, `commandId`, `actorUid`; proposed rules permit exact-content retries only | Maps remote unlock, keypad access, security lockout and relock | Queues and retries stable event IDs via an eight-entry NVS outbox | Reads newest events | **Matched for supported events; `irProximity` is intentionally not an audit type** |

## Firebase paths used by the physical client

- Reads: `deviceRegistry/<authenticated-uid>`, bounded `commands` query
  (`status=pending`, `limitToFirst=10`), and
  `settings/frontDoor/unlockDurationSeconds`.
- Writes: command lifecycle, `devices/frontDoor/isLocked`,
  `devices/gateway`, and `auditLogs/<event-id>` with same-record retry only.
- The deployed `database.rules.json` was **not changed**. Physical access
  requires an explicitly reviewed/deployed rules set; the new client is
  authorized only by the proposed policy and a matching device registry entry.

## Physical protocol and safe-mode behavior

- JSON Lines at 9600 baud, 8N1. Uno SoftwareSerial: RX A2 / TX A3. ESP32
  UART2: RX GPIO16 / TX GPIO17.
- ESP32 polls `PING`; only a matching, fresh Uno `PONG` makes Arduino status
  ready. `PONG` carries the Uno's logical lock state.
- Command flow: Firebase pending → device-side validation → Firebase accepted
  → UART UNLOCK → Uno accepted ACK → remote-unlocked STATE → Uno executed ACK
  → Firebase completed. The Uno's `executed` ACK confirms its servo command
  was issued, not that a latch or door moved.
- The ESP32 persists an active command phase and bounded audit outbox in NVS.
  It will not resend an accepted command after restart. If execution cannot be
  confirmed, it records failure when possible; the physical latch must still be
  bench-tested for every reset/power-loss case.
- Hardware mode is an explicit Flutter build setting. It does not switch to
  simulation if the device is offline. Remote Unlock stays disabled without a
  fresh gateway heartbeat and a fresh Arduino UART heartbeat.

## Remaining plug-and-play blockers

1. Review and test the proposed rules; do not deploy them without approval.
2. Create a dedicated, revocable test Auth identity and registry entry only in
   an isolated environment; no production credentials have been created.
3. Complete physical assembly and test every item in
   `PHYSICAL_ASSEMBLY_CHECKLIST.md` and `HARDWARE_TEST_PLAN.md`.
4. Verify actual servo current, line shifting, SoftwareSerial timing, heartbeat
   behavior, audit queue recovery, and power-loss behavior on the actual boards.
5. The ESP32 image currently uses 98.7% of the selected 1.25 MiB application
   partition. Monitor actual build size and confirm the selected board's
   partition layout before flashing; there is limited flash headroom.

See `DEVICE_SETUP.md` for isolated configuration and build commands and
`INTEGRATION_TEST_RESULTS.md` for evidence and pending tests.
