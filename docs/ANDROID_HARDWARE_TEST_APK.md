# Android Hardware-Test APK

## Build profile and Firebase safety

The hardware-test APK is an Android ARM64 Flutter **debug** build, signed
with the Android debug key. It is for bench testing only, not production
distribution. The debug Android manifest disables Firebase Analytics
collection and removes the native Firebase auto-initializer. The production
`android\app\google-services.json` is unchanged. The app initializes Firebase
with explicit emulator options when built with the command below; it does not
use the production Firebase options.

The hardware-test build requires both `HARDWARE_TEST_BUILD=true` and
`GATEWAY_MODE=hardware`. It also requires an explicit
`FIREBASE_EMULATOR_HOST`. If either required mode or host is missing, the app
shows a blocked-configuration screen before initializing Firebase.

The current workstation's private LAN address was `192.168.1.8` when this APK
was built. Recheck it before testing; if it changed, rebuild with the new
private address:

```powershell
Get-NetIPConfiguration
flutter build apk --debug --target-platform android-arm64 `
  --dart-define=HARDWARE_TEST_BUILD=true `
  --dart-define=GATEWAY_MODE=hardware `
  --dart-define=FIREBASE_EMULATOR_HOST=192.168.1.8 `
  --dart-define=FIREBASE_EMULATOR_PROJECT_ID=demo-smart-home
```

On this workstation, Flutter's Gradle task encountered a Windows CMake
metadata file lock. The same debug variant was successfully built in an
isolated workspace using the generated Gradle `assembleDebug` task with
`--no-daemon --no-parallel --max-workers=1`; the APK was copied to the
deliverable path below. The merged manifest was checked to confirm the
native Firebase auto-initializer is absent and Analytics collection is
disabled. Normal Flutter/Gradle deprecation and Kotlin-plugin compatibility
warnings remain, but the serialized build completed.

The APK build does not start an emulator, create accounts, deploy rules,
issue commands, or connect to live Firebase. The existing
`tools\rules_tests\firebase.test.json` binds to `127.0.0.1`, so a physical
phone cannot reach it as-is. For phone testing, create a separate local copy
of that configuration, edit both Auth and RTDB `host` values to the
workstation's private lab-network address, and select project
`demo-smart-home`. From the rules-test directory:

```powershell
Push-Location tools\rules_tests
Copy-Item firebase.test.json firebase.hardware-phone.json
# Edit auth.host and database.host in the copied file to the LAN address.
npx firebase-tools emulators:start --only auth,database --project demo-smart-home `
  --config firebase.hardware-phone.json
Pop-Location
```

Keep the emulator ports restricted to the isolated lab network with the host
firewall. The phone and workstation must be on a network that permits
device-to-host traffic on TCP ports 9099 (Auth) and 9000 (RTDB). Use the
workstation's reachable private LAN address in the APK; `10.0.2.2` is only the
Android Emulator alias and is not the host address for a physical phone.
Do not expose emulator ports to a public or untrusted network.

The Flutter client accepts only the administrator UID already configured in
the app. Do not use production administrator credentials or create an
unapproved production account. An isolated emulator administrator identity
and matching test database state must be provisioned separately before
authenticated hardware testing; this APK build does not provision either.
The ESP32 must use its own dedicated emulator identity, never the
administrator identity.

## Install and select a mode

For most current Android phones, install the ARM64 APK:

```powershell
& 'C:\Android\Sdk\platform-tools\adb.exe' devices
& 'C:\Android\Sdk\platform-tools\adb.exe' install -r build\app\outputs\flutter-apk\app-hardware-test-arm64-debug.apk
```

The APK is built for ARM64. Verify the phone supports ARM64 before installing.
Enable USB debugging and authorize the computer, or transfer the APK to the
phone and permit installation from that source. If multiple Android devices
are connected, use `adb -s <serial> install -r <apk-path>`. This debug-signed
APK has the same Android application ID as the existing app; installing it
over an existing compatible installation updates that app.

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
- `flutter test`: passed, 9 tests.
- Android debug APK: built and verified; package
  `IoT_SmartHome.Security`, version `1.0.0` (code 1), minimum Android API 24,
  target API 36, ARM64.
- APK: `build\app\outputs\flutter-apk\app-hardware-test-arm64-debug.apk`,
  80,762,471 bytes (77.0 MiB), SHA-256
  `EA842DC10A53457590FFAEB4EFECDB9B14A2718A62F69C22A08A53C92320E745`.
- APK signature: Android debug certificate; APK Signature Scheme v2
  verification passed. This is not a production release signature.
- Merged Android manifest: debug build is debuggable, Firebase Analytics
  collection deactivation is enabled, and the native Firebase auto-initializer
  is removed.
- Firebase Emulator sign-in, phone-to-host LAN routing, physical hardware,
  and physical unlock were not tested. The existing test emulator config is
  loopback-only, and no emulator account/database state was provisioned.
