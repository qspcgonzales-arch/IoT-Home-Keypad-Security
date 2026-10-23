# RAD methodology — IoT Smart Home Keypad Security Terminal

Status key: **DONE** = present in the repository and verified; **PARTIAL** = present
but not fully verified; **PROPOSED** = not built. Milestone numbers 3 and 4 come from
`hardware/README.md` and `tools/device_simulator/package.json`. Milestones 1 and 2 are
not named in the repository; their labels below are inferred from the code and should be
corrected by the team if they differ.

## Scope preserved (no rebuild)

The Firebase contract in `database.rules.json` is unchanged: `devices/frontDoor/isLocked`,
`devices/gateway/{isOnline,lastSeen,mode}`, `settings/frontDoor/unlockDurationSeconds`,
`commands/$commandId`, `auditLogs/$eventId`. Rules are default-deny and admin/simulator
UIDs are enforced. RAD work must not loosen them.

## Existing features (inspected)

| Feature | Evidence | Status |
| --- | --- | --- |
| Flutter Android app, light theme, reusable `StatusBadge`/`EventTile` | `lib/screens`, `lib/widgets`, `lib/theme` | DONE |
| Email/password login, administrator-UID check, demo mode | `lib/services/firebase_auth_service.dart`, `demo_auth_service.dart`, `lib/main.dart` | DONE |
| Live/mock separation behind one `DoorService` interface | `lib/services/door_service.dart`, `firebase_realtime_database_service.dart`, `mock_door_service.dart` | DONE |
| Remote unlock command (pending → accepted → completed/failed/expired, 30 s lifetime) | `firebase_realtime_database_service.dart`, `lib/models/unlock_command.dart` | DONE (simulator only) |
| Unlock-duration setting 4–30 s | `settings_screen.dart`, rules | DONE |
| Audit logs, newest 100 by `timestamp` | `audit_logs_screen.dart`, `newestAuditEvents()` | DONE |
| Virtual Uno + ESP32 gateway (Node) | `hardware/virtual`, `tools/device_simulator` | DONE |
| UART protocol v1 (provisional) | `hardware/docs/uart_protocol.md` | DONE as a spec |
| Arduino/ESP32 firmware | `hardware/arduino`, `hardware/esp32` | PARTIAL — Arduino logic written; ESP32 Wi-Fi, time sync, Firebase identity, listener and publisher are not implemented (hooks only); neither compiled or flashed |

## The four RAD phases

### 1. Requirements Planning
- Goal: remote-unlock a keypad door lock with auditability, using free-tier services.
- Constraints: Uno R3 and ESP32 unavailable; Sunday deadline; no paid services.
- Milestones mapped here: scope, Firebase data contract, security rules (**DONE**).
- Remaining: written acceptance criteria per iteration (**PROPOSED**, see `DEVELOPMENT_ITERATIONS.md`).

### 2. User Design
- Milestones mapped here: login, dashboard, audit log and settings screens, and the demo mode (**DONE**).
- Remaining: structured user walkthrough and recorded feedback (**PROPOSED**; none recorded to date).

### 3. Rapid Construction
- Milestones mapped here: Firebase auth and RTDB integration, command workflow, device
  simulator, virtual Uno/ESP32, keypad lockout and relock logic (**DONE** in software).
- Remaining: ESP32 firmware completion, compile checks of both firmware targets (**PROPOSED**).

### 4. Cutover
- Remaining, all **PROPOSED**: publish reviewed rules to the Firebase Console, hardware
  bring-up, release APK, per-device provisioning of Firebase identity, move from the trusted
  simulator identity to a device identity, final acceptance test.

## Working rules for iterations

1. Time-box each iteration (target 1–2 days); one demonstrable increment each.
2. Test on the Android Emulator and the virtual device first.
3. Record real results only; leave "Not yet collected" where there is no evidence.
4. Never trade away authentication, rules, command validation or electrical safety for speed.
5. Gate: `flutter analyze`, `flutter test`, `npm test` in `tools/device_simulator` all pass.
