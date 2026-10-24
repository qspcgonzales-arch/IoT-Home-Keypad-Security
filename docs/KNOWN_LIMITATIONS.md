# Known limitations

Release readiness snapshot: 2026-10-24 (Milestone 5.2). Controlled Auth and read-only database checks do not establish live command or physical-lock safety.

## Firebase and authentication

- Milestone 5.2 verified Email/Password sign-in for the approved administrator and a read-only read of `devices/frontDoor/isLocked` (`LOCKED`). No live rules were deployed or fetched for comparison.
- The Android Firebase configuration and `.firebaserc` target `smart-home-security-tip`; the Flutter app has no Firebase Emulator wiring or separate staging project selected.
- The Console administrator UID matched the app's allowlist. An initial unauthorized-account attempt showed a generic “account unknown” error before the app's UID check. In a later manual follow-up, the project owner reported that an existing non-admin account received the app's “This account is not authorized as an administrator.” message and stayed on Login. This closes the app-level check based on the reported result; no credentials were recorded.
- Incorrect-password and invalid-account attempts showed generic credential errors. Session persistence across process restart, logout, and offline-authentication error handling were verified.
- No remote unlock command, account creation, or rules deployment was performed. Do not attempt live commands without separate explicit approval and a controlled, non-actuating test device.
- App startup calls `Firebase.initializeApp` with the configured project, and the Android package includes Firebase Analytics. Passive SDK telemetry was not monitored; zero Firebase network traffic cannot be guaranteed.
- The local rules suite uses only the Realtime Database Emulator and a test-authenticated administrator UID; it does not exercise Firebase Auth login or the Flutter Firebase service against the emulator.
- `database.rules.json` and `database.rules.proposed.json` were byte-identical during the rules test run. Production rules were not fetched or compared during this milestone.
- The app's database-backed unlock path depends on an authenticated administrator, a current device heartbeat, and a device that acknowledges the command. It is not a direct actuator API.
- The Firebase rules simulator identity is a test/device identity. Secure production provisioning, credential storage, identity rotation, and live deployment review remain operational tasks.
- PIN lockout behavior is tested in the local virtual hardware harness only. PIN entry and lockout are not connected to the Flutter UI or Firebase database.
- Release-mode audit entries came from the mock service. The screen's not-connected warning is expected in local demo mode; those events are not persisted Firebase records.

## Hardware and firmware

- No physical keypad, servo, ESP32, LCD, IR sensor, buzzer, UART wiring, power supply, or fail-secure behavior has been tested.
- The ESP32 Firebase client remains a stub. The local JavaScript simulator is not physical hardware.
- Virtual timing and event tests do not validate real servo timing, electrical levels, power loss, watchdog behavior, network outages, or long-term reliability.
- No firmware flashing or physical-device acceptance test was performed for this milestone.

## Android and release packaging

- The release APK was run on Android Emulator Android 17/API 37 in local demo mode for M5.1 and used for the controlled M5.2 Auth/read check. No live unlock command or physical hardware test was performed.
- `flutter doctor -v` reports Android SDK license status as unknown. The detected emulator still worked; accept licenses using `flutter doctor --android-licenses` to remove the setup warning.
- `adb` is not on PATH, although `C:\Android\Sdk\platform-tools\adb.exe` is available.
- The release Gradle build currently uses the debug signing configuration. A production keystore and signing setup are required before distribution.
- CMake metadata file-lock errors occurred in the shared workspace; the artifact was successfully built in an isolated workspace with serialized Gradle tasks.
- The release APK still uses the debug signing configuration; it is not production-signed.

## Release scope

- This is a prototype and demo, not a production security certification.
- No user feedback session has been held.
- No paid services were added. Live deployment and physical hardware bring-up remain separate, explicitly approved work.
