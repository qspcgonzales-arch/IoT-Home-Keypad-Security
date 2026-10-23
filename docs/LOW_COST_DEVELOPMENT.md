# Low-cost development

## Principles (applied)

- Flutter and Dart: open source, no license cost.
- Firebase Authentication (email/password) and Realtime Database on the no-cost Spark plan. Confirm current quotas on the Firebase pricing page before relying on them.
- Android Emulator and Node virtual devices instead of hardware.
- Free tooling: Node's built-in `node --test`, Flutter test, PlatformIO and Arduino CLI for later.
- No paid APIs, no extra cloud services, no extra hardware without approval.
- Security is not reduced to save cost: default-deny rules, admin UID checks, command validation (type, status, 60 s maximum window) and the UART validation limits stay as they are.

## Code review against the optimization goals

| Goal | Finding | Action |
| --- | --- | --- |
| Reuse widgets | `StatusBadge` and `EventTile` are shared by the dashboard and audit screens | None needed |
| Fewer reads | `FirebaseRealtimeDatabaseService` listens to individual leaf paths, not whole `devices/` or `commands/` trees. The command listener exists only while a command is active and is cancelled afterwards | None needed |
| Bounded access logs | `newestAuditEvents({limit = 100})` uses `orderByChild('timestamp').limitToLast`, backed by `.indexOn: ["timestamp"]` in the rules | None needed |
| Error handling | `FirebaseException` is caught on writes; listeners have `onError`; user-facing feedback strings are set | None needed |
| Simulation vs live | `DoorService` interface, `MockDoorService` vs `FirebaseRealtimeDatabaseService`; `isSimulated` is read from `devices/gateway/mode` | None needed |
| Reusable hardware interfaces | UART v1 spec and shared command and audit schema; virtual and real devices use the same paths | None needed |

Rationale for no source change: the review found these goals already met. Changing working
security-sensitive code before the deadline carries more risk than benefit.

## Proposed optimizations (not implemented, need approval)

1. `audit_logs_screen.dart` builds up to 100 tiles in a non-lazy `ListView(children:)`. Switching to `ListView.builder` is a safe, small change.
2. The dashboard `Timer.periodic` (5 s) calls `notifyListeners()` for the whole screen. A narrower rebuild is possible but low value.
3. Add a fake-database test layer (see Iteration 7).
4. Replace the stock `README.md` with project instructions.

## Not done and requiring approval

- Any paid Firebase plan, Cloud Functions, or hosted CI.
- Purchase of Arduino Uno R3, ESP32, level shifter, servo supply or other components.
