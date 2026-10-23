# Testing and feedback

## Recorded test results (2026-10-21, Windows, Flutter SDK in `AcademiaHeightsDev\flutter`)

| Check | Command | Result |
| --- | --- | --- |
| Static analysis | `flutter analyze` | No issues found |
| Flutter tests | `flutter test` | 4 passed, 0 failed (`test/widget_test.dart`) |
| Device and virtual hardware tests | `npm test` in `tools/device_simulator` | 25 passed, 0 failed |
| Debug build | `flutter build apk --debug` | Succeeded after stopping Gradle daemons and cleaning |
| Emulator run | `flutter run -d emulator-5554` (Android 17, API 37) | Installed and launched without runtime errors in the log |
| Uno firmware compile | `arduino-cli compile --fqbn arduino:avr:uno hardware/arduino/smart_home_lock` (arduino-cli 1.5.1, AVR core 1.8.8, ArduinoJson 6.21.5, Keypad 3.1.1, LiquidCrystal I2C 1.1.2, Servo 1.3.0, Crypto 0.4.0) | Compiled: 26,362 bytes flash (81%), 1,508 bytes RAM (73%, 540 bytes left for locals, so RAM is tight). Compile only; nothing flashed |
| ESP32 gateway compile | `arduino-cli compile --fqbn esp32:esp32:esp32` on a copy of `src/main.cpp` (esp32 core 3.3.12, ArduinoJson 7.0.4) | Compiled: 904,562 bytes flash (69%), 47,868 bytes RAM (14%). Compile only; nothing flashed |

### Observed environment problems (real)

- `configureCMakeDebug` failed repeatedly with `FileNotFoundException ... generate_cxx_metadata_*_timing.txt (being used by another process)`. It occurred when more than one Gradle/Flutter process used the `build` folder at once. Workaround: stop other builds, run `gradlew --stop`, and rebuild. A retry often succeeds.
- The emulator once dropped the session ("Lost connection to device"); restarting the run resolved it.
- A stale install can show old UI; reinstalling via `flutter run` fixed it.

## Not tested

- Compiling is not testing: neither firmware has been flashed or run, and the Uno `SoftwareSerial` link and the ESP32 stub have no behavioural test beyond the compile.
- The Uno firmware needs ArduinoJson 6.x and the ESP32 firmware needs ArduinoJson 7.x (different library folders were used for the two compiles).
- Live Firebase path with automated tests; rules via the Firebase emulator.
- ESP32/Arduino firmware compilation and any physical hardware (servo, keypad, LCD, IR, buzzer, UART levels, power).
- Long-run Firebase quota usage.

## Pending physical hardware tests

1. Keypad scan and PIN entry, three-attempt, 30 s lockout.
2. Servo movement and relock timing at 4â€“30 s, with a separate servo supply.
3. UART link at the chosen baud with level shifting; duplicate and expired command rejection.
4. Wi-Fi loss and recovery; event sync.
5. IR proximity events; LCD and buzzer output.
6. Power-loss behavior and fail-secure state of the lock.

## User feedback log

**No user feedback has been collected yet.** Use this table for real sessions only.

| Date | Participant role | Task | Observation | Change requested | Status |
| --- | --- | --- | --- | --- | --- |
| â€” | â€” | â€” | â€” | â€” | â€” |

### Suggested walkthrough tasks (PROPOSED)

1. Sign in as administrator; confirm the status.
2. Request a remote unlock; watch pending â†’ unlocked â†’ relocked.
3. Find that event in the audit logs.
4. Change the unlock duration.
5. Try the demo mode and an unauthorized account.

## Milestone 4 recorded results (agent-run, no hardware, no live Firebase)

| Check | Result |
| --- | --- |
| `flutter analyze` | No issues found |
| `flutter test` | 4 of 4 passed |
| `npm test` in `tools/device_simulator` | 29 of 29 passed (25 earlier plus 4 new static firmware checks in `firmware_constraints.test.js`) |
| `npm test` in `tools/rules_tests` (Realtime Database emulator, proposed rules) | 17 of 17 passed |
| Uno compile (`arduino:avr:uno`, arduino-cli 1.5.1) | Before: flash 26,362 bytes (81%), SRAM 1,508 bytes (73%), 540 free. After: flash 18,574 bytes (57%), SRAM 1,056 bytes (51%), 992 free |
| ESP32 compile (`esp32:esp32:esp32`, core 3.3.12) | Flash 904,666 bytes (69%), RAM 47,868 bytes (14%); ESP32 source not changed in Milestone 4 |
| Uno stack depth | Not measured (`-fstack-usage` gave empty output) |
| Finding | In the emulator, the current `database.rules.json` rejects valid command creation (no validators for `type`, `status`, `requestedBy`, `createdAt`, `expiresAt`). The live project rules were not inspected. Proposed rules add them. See `hardware/docs/SECURITY_ARCHITECTURE.md` |

Not tested: any physical hardware, UART timing, servo/buzzer interaction, the simulator running against the
emulator with the proposed rules, the live Firebase project, and the ESP32 Firebase client (not implemented).
The full list is in `hardware/docs/HARDWARE_TEST_PLAN.md`. No user feedback was collected in this milestone.

## Milestone 4.5 results (agent-run, this machine)

| Check | Result |
|---|---|
| `flutter analyze` | No issues found |
| `flutter test` | 8 of 8 passed (4 existing + 4 new `gateway_policy_test.dart`) |
| `npm test` in `tools/device_simulator` | 36 of 36 passed (29 existing + 7 new `uart_contract.test.js`); one assertion in `firmware_constraints.test.js` was updated because the Uno nack reasons were intentionally changed |
| `npm test` in `tools/rules_tests` | 17 of 17 rules tests + 2 of 2 lifecycle tests passed |
| Live rules vs `database.rules.json` (read-only CLI) | Identical |
| Uno compile | 18,588 bytes flash, 1,056 bytes SRAM (992 free) |
| ESP32 compile | 904,658 bytes flash, 47,868 bytes RAM |

New emulator lifecycle test (`tools/rules_tests/test/lifecycle.test.js`): the real `DeviceSimulator` completes the
create/accept/complete/audit/relock flow under the proposed rules and is blocked under the production rules. The
Dart service was not executed against the emulator. No physical hardware was used and no user feedback was collected.