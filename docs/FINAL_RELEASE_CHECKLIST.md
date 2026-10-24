# Final release checklist — Milestone 5.2 update

Recorded 2026-10-24. The M5.1 release-demo results below are retained as a historical snapshot; the M5.2 live Auth/read check is recorded separately.

## APK and Android release-mode test

| Item | Status | Evidence / notes |
| --- | --- | --- |
| Release APK exists | PASS | `build\app\outputs\flutter-apk\app-release.apk` |
| APK size and hash recorded | PASS | 50,941,251 bytes (48.58 MiB); SHA-256 `A6CEBF0447435CCAC1685CDCF2C06020E4C773E8F05E1148E9E13A33DE005EEE` |
| Android emulator detected | PASS | `emulator-5554`, Android 17/API 37 |
| Release APK installed | PASS | ADB reported `Success` |
| Release app launched | PASS | `IoT_SmartHome.Security` `MainActivity` observed resumed |
| Local demo login | PASS | **Try local demo mode** opened the app as `demo@smarthome.local` |
| Dashboard initial state | PASS | Door showed `LOCKED`; UI indicated `LOCAL DEMO` and `SIMULATED` |
| Unlock confirmation and simulated unlock | PASS | Confirmation described local simulation; door showed `UNLOCKED` |
| Four-second automatic relock | PASS | Door returned to `LOCKED`; **Door automatically relocked** appeared |
| Audit log entries | PASS | Local **Remote unlock**, **Automatic relock**, and **Access granted** visible |
| Navigation and logout | PASS | Settings opened; Sign out returned to the login screen |
| Release APK tested with Firebase (M5.1) | NOT RUN | M5.1 used demo mode only; M5.2 Auth and read-only status results are below |
| Release APK tested on physical hardware | NOT RUN | No hardware available/tested |

Screenshots captured from this release-mode run are listed in [TESTING_AND_FEEDBACK.md](./TESTING_AND_FEEDBACK.md) and [FINAL_DEMO_SCRIPT.md](./FINAL_DEMO_SCRIPT.md), under [`screenshots/`](./screenshots/).

## Firebase configuration and local rules

- `firebase.json` maps the Realtime Database rules to `database.rules.json`; `.firebaserc` selects `smart-home-security-tip`.
- Android Firebase options also target `smart-home-security-tip`. The app has no Firebase Emulator wiring; the local test config runs the Database Emulator only.
- `FirebaseAuthService` enforces a configured administrator UID in the app. Realtime Database rules are default-deny and separately enforce administrator and registered-device identities.
- Rules validate command shape, requester, expiry, allowed status values, immutable request fields, permitted transitions, device identity, and audit record shape. Device state is writable only by the simulator or enabled device identities; audit entries are create-only for those identities and readable only by the administrator.
- The Milestone 5 local emulator suite passed 19/19; it does not confirm live rules match local files.
- No rules were deployed or fetched from the live project during this milestone.

## Milestone 5.2 — live Auth and read-only state verification

| Item | Status | Evidence / notes |
| --- | --- | --- |
| Firebase project and initialization | PASS | Android and Flutter Firebase options target `smart-home-security-tip`; live sign-in succeeded |
| Email/Password provider | PASS | Enabled, confirmed in Firebase Console |
| Approved administrator UID | PASS | Console test-user UID matched the app and local-rules allowlist |
| Administrator login | PASS | Credentials entered manually on `emulator-5554`; Dashboard opened |
| Read-only device-state access | PASS | Dashboard showed `LOCKED` from `devices/frontDoor/isLocked`; no database/rules error shown |
| Session persistence | PASS | Dashboard remained signed in after app-process restart |
| Logout | PASS | Returned to Login |
| Incorrect password and invalid account | PASS | Generic credential errors shown; protected Dashboard did not open |
| Unauthorized-account rejection | NOT VERIFIED | Attempt showed generic “account unknown”; Firebase authentication did not reach the app's UID check |
| Offline authentication | PASS | Expected network error shown; emulator networking restored |
| Remote command / account creation / rules deployment | NOT PERFORMED | No remote unlock command, account creation, or rules deployment occurred |
| Flutter checks | PASS | `flutter analyze` clean; `flutter test` 8/8 |

The unauthorized-account attempt is not evidence of the app's UID-specific denial. A conclusive check requires a valid, pre-existing Firebase user with a different UID; no account should be created just for this check. The successful state read shows the configured backend allowed the administrator to read the intended path, but deployed rules were not fetched or compared with the local file.

## Live-service safety decision

**The M5.2 controlled live Auth and read-only state checks were performed; live remote unlocking remains untested and must not be attempted without separate explicit approval.** The app is configured for `smart-home-security-tip` and is not connected to a local Auth/Database emulator. For the local demo, continue using demo mode. Do not create accounts or issue remote commands as part of the demo.

No accounts or passwords were changed, no account was created, no Realtime Database command was issued, and no rules were deployed. Firebase Core was initialized against the configured project; passive Analytics/network traffic was not monitored and cannot be ruled out.

## Remaining release blockers

| Issue | Impact | Required before production/hardware release |
| --- | --- | --- |
| Release APK uses debug signing configuration | Not suitable for production distribution | Configure and protect an approved release keystore; verify signing |
| No isolated Firebase environment wired to the Flutter app | Only controlled Auth and read-only status access were verified; live command behavior and safe staging remain untested | Add/test approved staging or emulator configuration before further live command testing |
| ESP32 Firebase client remains a stub | No physical gateway can process commands | Implement least-privilege device auth, time/heartbeat, command listener, and audit publishing |
| No physical hardware acceptance test | Electrical safety, servo timing, UART, keypad, and fail-secure behavior unknown | Complete documented hardware test plan with safe bench setup |
| Production rules were not checked in this milestone | Live security posture/equality not established | Review current live rules against the tested file only with approval |

## Readiness decision

**Ready for a local software demonstration and internal review. Not ready for production deployment or physical door control.** Do not present the local mock login, unlock, or audit events as live Firebase functionality.
