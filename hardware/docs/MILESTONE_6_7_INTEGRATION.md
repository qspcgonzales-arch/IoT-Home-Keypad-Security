# Milestone 6.7 — ESP32-only Firebase cloud diagnostics

## Status and safety boundary

The project owner reports that the staging APK was built and the initial
staging rules were deployed. The local configuration checker passes and the
cloud-diagnostics firmware compiles. The audit-attribution hardening now in
this source tree has **not** been deployed yet. No physical cloud connection
or device sign-in has been observed here, and no firmware was flashed. The
cloud profile compiles servo actuation out and only acknowledges targeted
unlock commands as `failed` or `expired`.

The Milestone 6.6 physical checks were reported by the project owner but were
not independently observed here. The Arduino USB-powered servo supply remains
unverified for repeated operation. Do not connect an occupied door or treat
reported lock state as proof of bolt position.

## Implemented architecture

```text
Flutter staging APK
  explicit staging FirebaseOptions + REMOTE HARDWARE TEST indicator
           │ authenticated admin account
           ▼
Isolated Firebase staging project
           ▲
           │ HTTPS/TLS, Firebase Auth device account
ESP32-WROOM-32 cloud-diagnostics profile
  local keypad/PIN/lockout/relock remain active
  Wi-Fi/Firebase status, sensor, audit and command diagnostics
  remote servo actuation compiled out
```

The old `smart_home_gateway` UART implementation remains a separate legacy
project. The new `esp32dev_cloud_diagnostics` environment does not include
Arduino UART code. `esp32dev_standalone` remains offline, and
`esp32dev_supervised_servo_test` remains local-only with its existing build
flag safety gate.

The ESP32 profile uses bounded exponential Wi-Fi retry, HTTPS with the ESP32
certificate bundle, Firebase email/password device sign-in and token refresh,
an exact device-UID check, and a `deviceRegistry/<uid>` enrollment check. It
publishes a five-second heartbeat, logical `isLocked`, raw IR `HIGH`/`LOW`,
device ID, firmware version, and redacted local security events. A small
persistent audit outbox retries after connection loss. Entered PINs are never
sent or logged.

Cloud audit entries always set `actorUid` to the authenticated ESP32 device
UID. Command events retain the administrator who requested the command in the
separate optional `requestedBy` field; the staging rules require it to match
the existing command record. Local events may omit `requestedBy`. When draining
older queued events, firmware migrates a legacy administrator `actorUid` into
`requestedBy` before upload. Existing database records remain readable without
rewriting.

The Flutter dashboard displays heartbeat-derived online/offline state,
reported lock state, IR digital level, firmware version, audit events, and a
visible **REMOTE HARDWARE TEST** indicator. A diagnostic command can be sent
only after an explicit in-app confirmation; the firmware rejects it without
calling the servo path. A successful physical unlock is not reported unless
the existing device acknowledgement contract says `completed`.

## Staging identities and configuration boundary

Confirmed staging values:

| Purpose | Value | Status |
| --- | --- | --- |
| Firebase Project ID | `iothomekeypadsecurity-staging` | Confirmed by project owner |
| Realtime Database URL | `https://iothomekeypadsecurity-staging-default-rtdb.asia-southeast1.firebasedatabase.app` | Confirmed by project owner |
| Firebase project number / sender ID | `637464771451` | Confirmed from supplied Android App ID prefix |
| Android application ID | `IoT_SmartHome.Security` | Confirmed by project owner |
| Device ID | `esp32-frontdoor-1` | Confirmed; use consistently in app, firmware, registry, and rules |
| Full Firebase Android App ID | Configured in ignored local options | Matches the locally registered staging Android app configuration |
| Staging administrator UID | Configured in staging templates and local configuration | Supplied as the confirmed staging Auth UID |
| ESP32 device UID | Configured in staging templates and local configuration | Supplied as the confirmed, separate staging Auth UID |

The short Android App ID prefix previously supplied is not a complete app ID.
Use the full `mobilesdk_app_id` from the ignored staging
`google-services.json`; the Gradle guard compares it with the ignored local
Dart options. API keys, device email/password, and Wi-Fi credentials remain
local-only values and must never be committed.

`.firebaserc` keeps `smart-home-security-tip` as the default and sets the
explicit `staging` alias to `iothomekeypadsecurity-staging`. Production's
`firebase.json` and `database.rules.json`, and the `demo-smart-home` emulator
configuration remain unchanged. `firebase.staging.json` points only to the
ignored local rules file `database.rules.staging.local.json`; deployment
must use that local file and an explicit staging project target.

## Manual Firebase project setup

Do these steps only in the existing isolated staging project. Do not change
production accounts or deploy `database.rules.json`.

1. In the **`iothomekeypadsecurity-staging`** Firebase Console, verify that
   the project ID and Realtime Database URL match the confirmed values above.
   Do not use the production project or `demo-smart-home`.
2. Enable Firebase Authentication Email/Password sign-in. In
   **Authentication → Users**, verify that the administrator and device users
   exist and their UIDs exactly match the supplied IDs above. If either user
   is absent, create a separate staging user and update all staging templates
   and rules to its generated UID before continuing. Do not use the production
   administrator UID. Use a unique, random device password stored in a
   password manager; never send it in chat or commit it.
3. In **Project settings → General → Your apps**, open the registered Android
   app for package `IoT_SmartHome.Security` and download its
   `google-services.json`. Place it exactly at
   `android/app/src/staging/google-services.json` (gitignored). The staging
   flavor intentionally uses the confirmed application ID without a suffix.
   This means an installed production build with the same application ID will
   be replaced by the staging build, and the two variants cannot be installed
   side-by-side.
4. Read the complete `mobilesdk_app_id`, `project_number`, API key, and
   `firebase_url` from that staging JSON. The supplied App ID is incomplete;
   copy the full value exactly rather than constructing it. Use the confirmed
   database URL without a trailing slash consistently in both local configs.
5. After verifying the Console project ID, create the device registry record
   in the staging Realtime Database:

   ```json
   {
     "deviceRegistry": {
       "66BkT43W8tPkWYL8jV4dgroMCbN2": {
         "enabled": true,
         "deviceId": "esp32-frontdoor-1"
       }
     }
   }
   ```

   Rules intentionally provide no client write access to the registry.
6. Copy the templates to local-only files:

   ```powershell
   Copy-Item staging_firebase_options.example.json staging_firebase_options.json
   Copy-Item hardware\esp32\esp32_only\src\secrets.h.example hardware\esp32\esp32_only\src\secrets.h
   Copy-Item database.rules.staging.json database.rules.staging.local.json
   ```

   In `staging_firebase_options.json`, set the staging API key and the exact
   full Android App ID from `google-services.json`; the confirmed project,
   sender ID, database URL, administrator UID, and device ID are prefilled.
   In `secrets.h`, set the staging API key, device email/password, and isolated
   2.4 GHz Wi-Fi credentials; verify its prefilled project, URL, UIDs, and
   device ID. The rules copy already contains the supplied UIDs. Do not copy
   `pin_config.h`; keep the existing local salted PIN provisioning flow.
7. Run the consistency check from repository root:

   ```powershell
   py -3.13 tools\esp32_only\check_staging_config.py
   ```

   It verifies the confirmed project, URL, Android sender/App ID shape,
   matching administrator/device identities, device ID, local rules UID
   consistency, and configured local credentials without printing their
   values. Do not proceed if it fails.
8. Run only the local staging-rules emulator tests:

    ```powershell
    Push-Location tools\rules_tests
    npm run test:staging
    Pop-Location
    ```

    The emulator is configured for loopback (`127.0.0.1:9001`), not a public
    interface. These tests do not contact the staging or production project.
9. Before deployment, explicitly inspect `.firebaserc`, run
    `firebase projects:list`, and verify that the signed-in Firebase account
    has access to **`iothomekeypadsecurity-staging`**. Confirm that
    `firebase.staging.json` points to the local staging rules file, then run
    the consistency check again. Only after this project-target check and
    separate owner approval may the staging rules be deployed:

    ```powershell
    firebase deploy --config firebase.staging.json --only database --project iothomekeypadsecurity-staging
    ```

    This command has not been run. Never omit the explicit `--project`
    argument, use `--project default`, or deploy with production's
    `firebase.json`.

## Local build and automated checks

Run the ESP32-only profiles from repository root:

```powershell
py -3.13 -m platformio run -d hardware\esp32\esp32_only -e esp32dev_standalone
py -3.13 -m platformio run -d hardware\esp32\esp32_only -e esp32dev_supervised_servo_test
py -3.13 -m platformio run -d hardware\esp32\esp32_only -e esp32dev_cloud_diagnostics
```

The cloud profile compiles servo support out even when local credentials are
present. Check the resulting binary against the selected 4 MB partition's
2,031,616-byte application slot:

```powershell
Get-Item hardware\esp32\esp32_only\.pio\build\esp32dev_cloud_diagnostics\firmware.bin |
  Select-Object Length
```

This is only a build-partition check; it does not verify the physical board's
flash or authorize upload.

Build the staging APK only after the staging Android JSON and local Dart
options match:

```powershell
flutter build apk --flavor staging --release --dart-define-from-file=staging_firebase_options.json
```

The Gradle staging task fails if the staging Google Services file is missing,
belongs to another project/package, or does not match the local Dart App ID,
sender ID, API key, and confirmed database URL. Dart also blocks startup if
staging is built without the staging flavor/options or with an incomplete
project configuration. The staging flavor uses the confirmed
`IoT_SmartHome.Security` application ID and never connects to the local
emulator. Since production uses the same application ID, installing this APK
replaces the production APK on that device. The configured release signing is
the repository's debug signing setup; do not distribute this APK as a
production release.

Preserve existing Android production behavior by explicitly selecting its
flavor for builds:

```powershell
flutter build apk --flavor production --release
```

Run the repository's automated checks:

```powershell
flutter analyze
flutter test
Push-Location tools\esp32_only
py -3.13 -m unittest discover -s . -p test_*.py
Pop-Location
Push-Location tools\rules_tests
npm test
Pop-Location
Push-Location tools\device_simulator
npm test
Pop-Location
```

## Supervised manual staging diagnostics

The following are future manual steps, not completed tests. Before the first
live staging write, pause and resolve the configuration blockers above, check
that the connected project is the isolated staging project, and obtain
explicit owner approval for that write.

1. Start with the cloud-diagnostics image only after local compile and size
   checks. No upload is authorized by this document.
2. Observe Serial Monitor at 115200 baud. Expected messages are non-sensitive
   Wi-Fi/auth/registry/status diagnostics only; no SSID, password, token, or
   PIN should appear.
3. Confirm the Flutter app shows `REMOTE HARDWARE TEST`, ESP32 online after a
   fresh heartbeat, reported logical lock state, IR `HIGH`/`LOW`, firmware
   version, and audit history. A stale heartbeat must display offline.
4. Confirm local keypad access, three-attempt/30-second lockout, buzzer, LCD,
   sensor, and automatic logical relock still work offline. Entered PINs must
   remain redacted.
5. Only after explicit staging-write approval, use the app's test-command
   confirmation. Expected outcome is `failed` or `expired`, a device
   acknowledgement, and a matching audit event. The staging rules deny
   `accepted` and `completed`; the servo is compiled out, so no physical
   movement can result.
6. Test network loss by disconnecting the staging Wi-Fi at the bench (not by
   exposing any emulator port). Local keypad operation must continue; the app
   must mark the device offline once its heartbeat is older than 30 seconds.
   Restore connectivity and verify a fresh heartbeat and queued audit events.
7. Stop immediately for unexpected motion, heating, resets, unsafe voltage,
   or any mismatch between the reported Firebase project and the staging
   project. Do not connect a latch or occupied door.

Do not use this profile to activate the servo. The only servo procedure remains
the separately approved, on-site, unloaded-servo procedure in
[ESP32_ONLY_ACCEPTANCE.md](./ESP32_ONLY_ACCEPTANCE.md).

## Rollback and remaining blockers

- The previously prepared `esp32dev_standalone` profile remains the offline,
  servo-compiled-out option. The supervised servo profile remains separate.
  No automatic rollback or flash has been performed.
- The project ID, database URL, project number, Android application ID, and
  device ID have been supplied and are enforced by configuration guards.
- The project owner reports that the initial staging rules were deployed.
  The audit-attribution hardening in this source tree still needs an approved,
  explicit-target staging deployment.
- The device registry entry still needs confirmation in the staging database.
- No ESP32 cloud sign-in, heartbeat, command acknowledgement, or audit write
  has been observed against a real Firebase project.
- Physical flash capacity, wiring/voltage compatibility, and repeated servo
  power safety have not been verified in this session. Never infer physical
  test success from a build.
