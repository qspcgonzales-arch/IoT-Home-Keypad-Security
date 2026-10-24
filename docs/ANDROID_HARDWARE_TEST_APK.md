# Android Hardware-Test APK

## Build profile and Firebase safety

The hardware-test APK is an Android ARM64 Flutter **debug** build, signed
with the Android debug key. It is for bench testing only, not production
distribution. The debug Android manifest disables Firebase Analytics
collection, removes the native Firebase auto-initializer, and permits
cleartext HTTP for debug builds to support local emulator endpoints. These
settings apply to debug builds only and are not in the release manifest. The
production `android\app\google-services.json` is unchanged. The app
initializes Firebase with local-only emulator options after the operator
enters the test PC's LAN address; it does not use the production Firebase
options.

The hardware-test build requires both `HARDWARE_TEST_BUILD=true` and
`GATEWAY_MODE=hardware`. The emulator host is entered on the phone at startup,
before Firebase Auth or Realtime Database services are created. The selected
host is applied to both services for that process only and is not saved. A
different host requires force-closing and relaunching the app. There is no
mid-session host switch.

Build the portable APK without embedding any computer's address:

```powershell
flutter build apk --debug --target-platform android-arm64 `
--dart-define=HARDWARE_TEST_BUILD=true `
--dart-define=GATEWAY_MODE=hardware
```

The hardware-test build uses the fixed local Emulator project ID
`demo-smart-home`, fake Firebase app credentials, and explicit emulator
endpoints. It does not accept a production host or switch to production
Firebase when the emulator is unreachable. The **Start local demo without
Firebase** option is explicit and does not initialize Firebase.

## Setup on another Windows test PC

1. Connect the Windows PC and Android phone to the same isolated lab network.
 Find the PC's reachable private IPv4 address:

 ```powershell
 Get-NetIPConfiguration | Format-Table InterfaceAlias, IPv4Address
 ```

 Use the address of the adapter that the phone can reach, not `127.0.0.1`,
 a VPN adapter, or `10.0.2.2`.
2. Keep the checked-in loopback-only emulator config unchanged. From the
 repository root, make a local copy for this test PC:

 ```powershell
 Push-Location tools\rules_tests
 Copy-Item firebase.test.json firebase.hardware-phone.json
 notepad firebase.hardware-phone.json
 ```

 In the copied file, set **both** `emulators.auth.host` and
 `emulators.database.host` to the PC's private IPv4 address. Keep the
 existing ports `9099` and `9000`, respectively, and do not change rules.
 The copied file is local test configuration and should not be committed.
3. Start only the local Auth and Realtime Database emulators:

 ```powershell
 npx firebase-tools emulators:start --only auth,database --project demo-smart-home `
   --config firebase.hardware-phone.json
 ```

 Run that command in the same `tools\rules_tests` directory. Keep the
 emulator running while testing. Allow inbound TCP ports 9099 and 9000 only
 on the private lab network using Windows Firewall; do not expose them to
 public or untrusted networks.
4. Open the APK on the phone. On the **DEVELOPMENT ONLY — FIREBASE EMULATOR**
 startup screen, enter the same PC IPv4 address and tap **Continue to
 hardware test**. The app configures Auth at port 9099 and RTDB at port 9000
 before either Firebase service is constructed. If the PC address changes,
 force-close and relaunch the app to enter the new one.
5. Alternatively, tap **Start local demo without Firebase**. This uses the
 mock service and does not make an emulator or production Firebase
 connection.

The phone and PC must be able to communicate directly on the lab network.
`10.0.2.2` is only the Android Emulator host alias, not a physical phone's
route to a Windows PC. The app rejects loopback, public, malformed, and
non-private addresses before Firebase initialization.

After submitting the address, the host cannot be changed in that process. If
Firebase setup fails, Auth cannot sign in, or RTDB remains offline, force-close
and reopen the app before entering a corrected address. This avoids unsafe
mid-session changes to process-scoped emulator routing.

The APK build does not start an emulator, create accounts, deploy rules,
issue commands, or connect to live Firebase. The Flutter client accepts only
its allowlisted administrator UID. Use only an explicitly approved identity
and database state in the isolated emulator; do not use production credentials
or modify production accounts. The ESP32 must use its own dedicated emulator
identity, never the administrator identity.

## Install and select a mode

For most current Android phones, install the ARM64 APK:

```powershell
adb devices
adb install -r build\app\outputs\flutter-apk\app-hardware-test-arm64-debug.apk
```

The APK is built for ARM64. Verify the phone supports ARM64 before installing.
Enable USB debugging and authorize the computer, or transfer the APK to the
phone and permit installation from that source. If multiple Android devices
are connected, use `adb -s <serial> install -r <apk-path>`. This debug-signed
APK has the same Android application ID as the existing app; installing it
over an existing compatible installation updates that app. If `adb` is not
on `PATH`, install Android SDK Platform-Tools and run `adb.exe` from its
`platform-tools` directory.

On the login screen:

- **Try local demo mode** opens the local mock service. The dashboard labels
it **LOCAL DEMO** and **SIMULATED**; it does not send Firebase commands.
- Firebase sign-in is for the explicitly provisioned isolated emulator
administrator only. The login screen identifies this build as
**HARDWARE TEST BUILD — isolated Firebase Emulator only**.

The hardware-test gateway mode is fixed at build time. Choosing **Try local
demo mode** is an explicit switch to the mock service, not an automatic
fallback; the dashboard then displays **LOCAL DEMO**.

The hardware-test build expects `mode: hardware`. The dashboard shows
**HARDWARE TEST — physical gateway expected** even while the gateway is
missing. A stale/missing gateway heartbeat is offline; a mode mismatch is
reported as **MODE MISMATCH**; and a missing/freshness-failed Arduino UART
heartbeat is reported as **ARDUINO OFFLINE**. Remote unlock remains disabled
unless the database is connected, the reported gateway mode matches, the
gateway and Arduino heartbeats are fresh, a known locked state exists, and no
command is pending.

The command display distinguishes pending/accepted from completed. The UI
reports a physical unlock only after the command contains a valid device
acknowledgement. Do not treat an online badge as proof that the physical
servo or latch moved.

## Bench checklist

Keep the servo horn disconnected from any latch. Do not connect the assembly
to an occupied building door.

- [ ] Verify power polarity, common ground, UART level shifting, and separate
    regulated servo power before energizing.
- [ ] Test LCD startup and keypad scanning on the Arduino alone.
- [ ] Test active-buzzer feedback and TCRT5000 input indication.
- [ ] Test the servo on external power with no latch attached; confirm the
    four-second relock.
- [ ] Connect level-shifted Arduino–ESP32 UART; verify PING/PONG and Arduino
    heartbeat before enabling remote commands.
- [ ] Connect ESP32 only to the isolated Wi-Fi/Firebase Emulator environment;
    verify authenticated device heartbeat and Arduino readiness.
- [ ] Confirm the phone displays **HARDWARE TEST**, online/offline state, and
    Arduino readiness accurately.
- [ ] If approved for the isolated emulator, issue one remote unlock with the
    unloaded servo; verify accepted/executed acknowledgement and audit
    update, then verify automatic relock.
- [ ] Disconnect Wi-Fi and confirm the gateway becomes offline and remote
    unlock is disabled; restore Wi-Fi and check recovery without stale
    command replay.
- [ ] Record physical results in
    `hardware\docs\INTEGRATION_TEST_RESULTS.md`. Leave all door/latch tests
    pending until separately approved and mechanically reviewed.

## Validation status

- `flutter analyze`: passed, no issues.
- `flutter test`: passed, 16 tests.
- Android debug APK: built and verified; package
`IoT_SmartHome.Security`, version `1.0.0` (code 1), minimum Android API 24,
target API 36, ARM64.
- APK: `build\app\outputs\flutter-apk\app-hardware-test-arm64-debug.apk`,
80,540,106 bytes (76.8 MiB), SHA-256
`09C5CC450E495999A2E66AC84988DCB58BEE231B9C241FBF09EE39EE69BD9516`.
- APK signature: Android debug certificate; APK Signature Scheme v2
verification passed. This is not a production release signature.
- Merged debug manifest: Firebase Analytics collection is disabled and
`FirebaseInitProvider` is absent; cleartext is enabled for local emulator
endpoints in this debug-only manifest.
- The standard build encountered Windows CMake metadata locks; the APK was
rebuilt successfully in an isolated workspace with serialized Gradle tasks.
- A debug launch on the available x86_64 emulator did not complete because
the local Flutter framework references `HitTestRequest`/`HitTestResponse`
APIs missing from its paired engine. No emulator host was submitted and no
Firebase service was initialized.
- The test PC/phone emulator connection and sign-in, physical hardware, and
physical unlock were not tested. No emulator account/database state was
provisioned.
