# Testing and feedback

## Milestone 5.2 live Firebase Auth and read-only database check (2026-10-24)

| Check | Procedure | Actual result |
| --- | --- | --- |
| Firebase project configuration | Compare `firebase_options.dart` and Android `google-services.json` | Both target `smart-home-security-tip`; Firebase initialization and live sign-in succeeded |
| Email/Password provider | Confirm in Firebase Console | Enabled, confirmed by the project owner |
| Administrator login and UID | Enter test credentials manually in the emulator; compare Console UID with app/local-rules allowlist | Dashboard opened; Console UID matched the approved UID |
| Incorrect password and invalid account | Submit each in the app | Generic credential error shown; Dashboard did not open |
| Initial unauthorized-account attempt | Attempt sign-in with the purported non-admin account | Generic “account unknown” error appeared. Firebase authentication did not succeed, so the app's UID-specific denial was not verified by this attempt |
| Existing non-admin authorization follow-up | Sign in with an existing non-admin account on the `Medium_Phone` emulator | Project owner reported that the app showed “This account is not authorized as an administrator.” and stayed on Login. Based on this reported result, authentication succeeded and the app's UID check denied Dashboard access |
| Device-state read | Open Dashboard after administrator login; do not use command controls | `devices/frontDoor/isLocked` returned `LOCKED`; no database/rules error was shown |
| Session persistence | Force-stop and relaunch without signing out | Dashboard remained signed in |
| Logout | Use app logout control | Returned to Login |
| Offline authentication | Disconnect emulator network and attempt sign-in | Expected network error shown; emulator networking was restored |
| Firebase writes/deployment | Observe actions during this check | No account was created, no unlock command was sent, and no rules were deployed |
| Static analysis and Flutter tests | `flutter analyze`, `flutter test` | No analysis issues; 8 tests passed |
| Flutter launch | Use existing APK with `flutter run --no-pub --use-application-binary=build/app/outputs/flutter-apk/app-release.apk -d emulator-5554` | Existing APK installed/launched without a rebuild. A normal debug build attempt failed on a Gradle CMake metadata file lock |

The initial unauthorized-account attempt did **not** establish that a valid Firebase user with a different UID is denied by the app: the generic account-unknown authentication error occurred before the UID check. In a later manual follow-up on 2026-10-24, the project owner reported that an existing non-admin account reached the app's administrator-denial message and remained on Login. This closes the app-level UID-check gap based on the reported result; credentials were not recorded. Do not create an account for this test.

The local rules allow the approved administrator UID to read `devices/frontDoor/isLocked`. The successful live read verifies that the configured backend permitted this read at test time; deployed rules were not fetched or compared with `database.rules.json`. No remote command was created or submitted.

## Milestone 5.1 release APK validation (2026-10-24)

| Check | Procedure | Actual result |
| --- | --- | --- |
| Emulator detection | `C:\Android\Sdk\platform-tools\adb.exe devices`, `flutter devices` | `emulator-5554` online; Android 17/API 37 |
| APK verification | Inspect `build\app\outputs\flutter-apk\app-release.apk` | Present; 50,941,251 bytes (48.58 MiB); SHA-256 `A6CEBF0447435CCAC1685CDCF2C06020E4C773E8F05E1148E9E13A33DE005EEE` |
| Installation | `adb install -r build\app\outputs\flutter-apk\app-release.apk` | `Success` |
| Launch | `adb shell monkey -p IoT_SmartHome.Security 1` | One launch event injected; `MainActivity` observed resumed |
| Local demo login | Choose **Try local demo mode** | Dashboard opened as `demo@smarthome.local` |
| Initial lock state | Inspect Dashboard | `LOCKED`; device card clearly says `LOCAL DEMO — not connected to Firebase` and `SIMULATED` |
| Unlock flow | Tap **Remote unlock**, confirm **Unlock** | Confirmation identified local simulation; dashboard showed `UNLOCKED` and the four-second relock message |
| Automatic relock | Wait for simulated timer | Door returned to `LOCKED`; dashboard showed **Door automatically relocked** |
| Audit logs | Open **Audit logs** | Local **Automatic relock**, **Remote unlock**, and **Access granted** entries visible; expected not-connected warning also visible |
| Navigation and logout | Open Settings, then Sign out | Settings opened; Sign out returned to **Welcome home** |
| Firebase/hardware in M5.1 | No live sign-in, account creation, Realtime Database command, or hardware action | Not performed in M5.1. See the Milestone 5.2 Auth/read results above. Firebase Core initialized with the configured project; passive Analytics/network traffic was not monitored. |

The M5.1 release-demo interaction used local mock/demo mode. It did not verify Firebase authentication, live remote unlocking, or physical hardware. The separate M5.2 check verified authentication and a read-only state path only. The app initializes Firebase Core at startup and includes Firebase Analytics; passive telemetry was not monitored, so neither run can be described as proven network-isolated.

### Release-mode screenshots

Captured from the release APK on `emulator-5554`; originals are in `docs/screenshots/`.

- [Login screen](./screenshots/01-release-login.png)
- [Dashboard in local demo mode](./screenshots/02-release-dashboard-demo.png)
- [Unlock confirmation](./screenshots/03-release-unlock-confirmation.png)
- [Unlocked state](./screenshots/04-release-unlocked.png)
- [Automatic relock](./screenshots/05-release-relocked.png)
- [Audit logs](./screenshots/06-release-audit-logs.png)
- [Settings navigation](./screenshots/07-release-settings.png)
- [Logout returned to login](./screenshots/08-release-logout-login.png)

## Milestone 5 verification (2026-10-24)

| Check | Command / procedure | Actual result |
| --- | --- | --- |
| Dependency resolution | `flutter pub get` | Passed |
| Static analysis | `flutter analyze` | No issues found |
| Flutter tests | `flutter test` | 8 passed, 0 failed |
| Device and virtual hardware | `npm test` in `tools/device_simulator` | 36 passed, 0 failed; includes three-invalid-PIN/30-second lockout simulation |
| Realtime Database rules | `npm test` in `tools/rules_tests` | 19 passed, 0 failed using the local Emulator Suite |
| Android device discovery | `flutter doctor -v`, `flutter devices`, `C:\Android\Sdk\platform-tools\adb.exe devices` | Emulator `emulator-5554` detected and online (Android 17/API 37). `adb` is not on PATH. |
| Android debug UI | `flutter run -d emulator-5554 --debug` | Installed and launched. Local demo login, dashboard, Audit Logs, simulated-mode indicator, unlock confirmation, unlocked state, four-second relock, and relock audit activity were observed. |
| Release APK | `flutter build apk --release` | Built in an isolated workspace after serialized Gradle `assembleRelease`; installation and release-mode local-demo checks are recorded in the Milestone 5.1 section above. |
| Live Firebase | Not run | No live Firebase feature was verified and no live rules were deployed. |
| Physical hardware | Not run | No physical keypad, servo, or ESP32 was used. |

### Release APK artifact

- Path: `build\app\outputs\flutter-apk\app-release.apk`
- Size: 50,941,251 bytes (48.58 MiB)
- At the Milestone 5 checkpoint, the APK had not yet been installed or launched; Milestone 5.1 above supersedes that installation status.
- The current Android Gradle configuration signs release builds with the debug signing configuration. Do not distribute this as a production-signed Play Store release.

### Local Firebase Emulator Suite scenarios

The Realtime Database test configuration runs the Database Emulator only (not the Auth Emulator). Its test authentication context uses the configured administrator UID; this verifies rules authorization, not a real Firebase administrator login.

| Scenario | Actual result |
| --- | --- |
| Administrator authorization | Authenticated administrator test context was accepted by the rules; actual Firebase Auth login was not exercised. |
| Remote unlock request | Authorized administrator command creation passed. |
| Simulator acknowledgement | `DeviceSimulator` accepted and acknowledged the command under the rules. |
| Door state | Emulator state changed from locked to unlocked and back. |
| Four-second automatic relock | Lifecycle test used a 4,000 ms simulated unlock duration and observed relock. |
| Audit-log updates | Unlock and automatic-relock audit events were observed in the lifecycle test. |
| Invalid PIN lockout | Verified in the separate virtual hardware suite: three invalid entries trigger a 30-second lockout. PIN entry is not wired to the Realtime Database emulator or Flutter UI. |
| Expired command | Expired command was rejected/marked terminal and did not unlock the door. |
| Unauthorized access | Unauthenticated and unauthorized access attempts were denied by the rules tests. |

The 17 policy tests load `database.rules.proposed.json`; the two lifecycle tests exercise both that file and `database.rules.json`. The two rule files had identical SHA-256 hashes during this run. The Emulator Suite is local-only; this does not establish that production rules or live Firebase behavior match.

### Android environment notes

- `flutter doctor -v` reports Android SDK license status as unknown. The installed Android 17 emulator nevertheless built, installed, and launched debug and release apps. Resolve the warning with `flutter doctor --android-licenses` when prompted before setting up another machine.
- The Visual Studio warning is for Windows desktop builds and did not block Android.
- The `adb` executable is installed at `C:\Android\Sdk\platform-tools\adb.exe`; add `C:\Android\Sdk\platform-tools` to PATH if `adb` should work by name.
- Release builds in the shared workspace hit intermittent CMake metadata file-lock errors. The release artifact was produced in an isolated workspace using `gradlew --no-daemon --no-parallel --max-workers=1 assembleRelease`, then the Flutter release command completed there. If it recurs, stop competing builds, run `android\gradlew.bat --stop`, and retry one build at a time.

## Evidence categories

- **Implemented:** Firebase-auth service, database-backed status/command/audit UI, local demo mode, local device simulator, and virtual keypad/Arduino model.
- **Automated tests passed:** The commands and results above are local test evidence only.
- **Live Firebase features verified:** M5.2 administrator authentication and read-only device-state access only. No remote command or rules deployment was made; live command behavior remains unverified.
- **Hardware features verified:** None. Firmware compilation is not hardware testing; the ESP32 Firebase client remains a stub.
- **User feedback:** No user feedback has been collected.

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
- Live Firebase path with automated tests; Firebase Authentication was manually tested in M5.2, while rules tests use the local Realtime Database Emulator.
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