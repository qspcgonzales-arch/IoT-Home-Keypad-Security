# IoT Smart Home Keypad Security Terminal

Flutter dashboard prototype for an administrator-managed front-door lock, with a local demo mode, a Firebase Realtime Database command workflow, and virtual device tests.

## Release-readiness snapshot

Recorded on 2026-10-24 (Milestone 5.2). This is a software prototype, not a verified physical security product.

### Implemented

- Firebase administrator sign-in and administrator-only dashboard access.
- Realtime door/gateway status, remote-unlock command creation, audit-log display, and 4–30 second duration settings.
- A separate local demo mode that simulates the door and gateway without sending device commands to Firebase.
- A local Firebase device simulator and virtual keypad/Arduino test harness.
- Realtime Database rules in [`database.rules.json`](./database.rules.json) with explicit command validation and lifecycle checks.

### Automated tests passed

- `flutter analyze`: no issues.
- `flutter test`: 8/8.
- `npm test` in `tools/device_simulator`: 36/36, including invalid-PIN lockout simulation.
- `npm test` in `tools/rules_tests`: 19/19 against the local Realtime Database Emulator Suite. The proposed and deployment rules files were byte-identical during this run.
- Android Emulator (Android 17/API 37): release APK installed and launched; local demo login, dashboard, audit logs, simulated unlock, four-second relock, navigation, and logout were exercised. Screenshots are in [`docs/screenshots/`](./docs/screenshots/).
- Milestone 5.2: `flutter analyze` passed and `flutter test` passed 8/8 after dependency resolution.
- Release APK built; details and the Android Emulator/ADB notes are in [`docs/TESTING_AND_FEEDBACK.md`](./docs/TESTING_AND_FEEDBACK.md).

### Live Firebase and hardware verification

- Milestone 5.2 verified Firebase Email/Password sign-in with the approved administrator account and a read-only Realtime Database read of `devices/frontDoor/isLocked` (`LOCKED`). The Console UID matched the app's approved UID.
- Session persistence across an app-process restart, logout, generic incorrect-password/invalid-account errors, and the offline-authentication error were also verified. An attempted unauthorized-account sign-in returned a generic “account unknown” error before the UID authorization check; the UID-specific denial has not been dynamically verified.
- No remote unlock command, account creation, or Firebase rules deployment was performed. Deployed rules were not fetched or compared with the local rules.
- Firebase Core initializes with the configured project, and the Android package includes Firebase Analytics. Passive startup telemetry was not monitored, so zero Firebase traffic cannot be guaranteed.
- Physical keypad, servo, ESP32 Firebase client, wiring, and fail-secure behavior remain unverified. The ESP32 Firebase client is still a stub.
- The existing release APK was also used for the controlled Milestone 5.2 Auth/read check; it was not tested with live remote unlocking or physical hardware.

See [`docs/KNOWN_LIMITATIONS.md`](./docs/KNOWN_LIMITATIONS.md) before treating this as a release candidate. Follow [`docs/FINAL_DEMO_SCRIPT.md`](./docs/FINAL_DEMO_SCRIPT.md) for a local-only demo.

## Run locally

```powershell
flutter pub get
flutter run
```

At the sign-in screen, choose **Try local demo mode** to use simulated state. Firebase sign-in contacts the configured project; use it only for an explicitly approved test.

Run tests from the repository root:

```powershell
flutter analyze
flutter test
Push-Location tools\device_simulator; npm test; Pop-Location
Push-Location tools\rules_tests; npm test; Pop-Location
```

The rules tests use the local Firebase Emulator Suite and a demo project ID. They do not deploy rules or access the live project.

## Rules deployment instructions

Deployment is intentionally not automatic. Before any live change, review and back up the current rules in Firebase Console, confirm that `firebase.json` targets `database.rules.json` and the intended project, run the local rules tests, and obtain explicit approval for the exact project and rules. After approval only, an operator may deploy with:

```powershell
firebase deploy --only database --project smart-home-security-tip
```

Do not run that command as part of local testing. Verify the deployed rules and expected administrator/device access after an approved deployment.
