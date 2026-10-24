# Device Setup and Firmware Commands

## Safe defaults

- The ESP32 does not connect unless a local `secrets.h` contains non-placeholder
  values. `secrets.h` is gitignored.
- The committed example enables only the Firebase Auth and RTDB emulators.
  Production HTTPS endpoints require manually changing
  `FIREBASE_USE_EMULATOR` to `0`; do not do so for this Milestone 6 test.
- No Firebase Auth account, registry record, production key, production rules
  deployment, or physical lock was provisioned or activated.
- Hardware mode in Flutter is opt-in. Omitting `GATEWAY_MODE` keeps the
  existing simulated default. Unknown explicit values fail rather than
  silently falling back.

## Isolated Auth + RTDB emulator

From the repository root, run the rules suite (it starts and stops local
emulators on loopback):

```powershell
Push-Location tools\rules_tests
npm run test:device-client
Pop-Location
```

The test uses project `demo-smart-home`, Auth emulator port 9099, and RTDB
emulator port 9000. Both are bound to `127.0.0.1` in
`tools/rules_tests/firebase.test.json`. The test creates a temporary emulator
device account, seeds a matching registry entry with rules disabled, exercises
the Auth/REST lifecycle and verifies revocation; it does not contact the
production project.

An ESP32 on another device cannot reach a loopback-only emulator. For a
physical bench, use a separate emulator configuration bound only to an
isolated private lab network, restrict host firewall access to that lab, and
use the host's private IP in `secrets.h`. Never expose emulator ports to a
public or untrusted network. A persistent test device account and registry
entry must be created manually in that emulator before flashing; the automated
test cleans its database state and does not provision the physical board.

## ESP32 configuration

Copy the example; edit it locally and never commit the resulting file:

```powershell
Copy-Item hardware\esp32\smart_home_gateway\src\secrets.h.example hardware\esp32\smart_home_gateway\src\secrets.h
```

For the emulator bench, keep `FIREBASE_USE_EMULATOR` set to `1`, replace
`WIFI_SSID`, `WIFI_PASSWORD`, and the emulator host with the isolated lab
values, and provide the emulator's dedicated test device email/password and
matching `deviceId`. Do not use an administrator account. The Auth emulator
address uses port 9099; the RTDB emulator URL uses port 9000 and namespace
`demo-smart-home`. For Android Emulator clients, the host is commonly
`10.0.2.2`; for an ESP32 on Wi-Fi, use the host computer's reachable private
LAN address. Confirm firewall routing before powering the gateway.

The RTDB rules require `deviceRegistry/<device-auth-uid>` to have
`enabled: true` and the exact configured `deviceId`. Registry changes must be
made out-of-band in the emulator; the device cannot enroll or revoke itself.
The device's own UID must not be the admin or simulator UID.

## Build and flash

Install PlatformIO Core if it is not already available:

```powershell
py -3.13 -m pip install --user platformio
```

Build both targets without connecting to Firebase:

```powershell
py -3.13 -m platformio run -d hardware\esp32\smart_home_gateway -e esp32dev
py -3.13 -m platformio run -d hardware\arduino\smart_home_lock -e uno
```

## Verified build environments

Resolved versions from the clean builds:

| Target | Platform / framework | Libraries and tools |
| --- | --- | --- |
| ESP32 `esp32dev` | `espressif32@8.0.0`; Arduino-ESP32 `3.3.12`; board profile assumes 4 MB flash | ArduinoJson `7.4.3`; framework WiFi, HTTPClient, NetworkClientSecure, Preferences `3.3.12`; Xtensa toolchain `14.2.0+20260121`; esptool `5.4.0` |
| Uno `uno` | `atmelavr@5.3.0`; Arduino AVR framework `5.4.0` | Keypad `3.1.1`; LiquidCrystal_I2C `1.1.4`; Servo `1.3.0`; ArduinoJson `7.4.3`; Crypto `0.4.0`; framework SoftwareSerial `1.0.0`; AVR toolchain `7.3.0` |

The `esp32dev` PlatformIO board profile assumes 4 MB flash; this is not proof
of the physical board's flash size. Before selecting an ESP32 partition or
flashing, identify the exact module and query the chip on its confirmed COM
port:

```powershell
py -3.13 -m platformio device list
$unoPort = 'COM5'    # replace with the Uno port identified below
$esp32Port = 'COM7'  # replace with the ESP32 port identified below
& "$env:USERPROFILE\.platformio\packages\tool-esptool\esptool.exe" --port $esp32Port flash-id
```

In Windows Device Manager, identify the entry under **Ports (COM & LPT)**,
then unplug/reconnect the intended board and confirm which entry disappears
and returns. Replace `COM7` below with that port; do not assume the same number
for both boards or future connections.

**ESP32 upload is currently blocked on the default partition.** The clean
build's `firmware.bin` is 1,322,832 bytes, while the selected app slot is
1,310,720 bytes. PlatformIO's ELF size summary does not catch this final-image
overrun. Do not flash the current ESP32 build. See
`INTEGRATION_TEST_RESULTS.md`; first confirm physical flash capacity and choose
a compatible partition, then verify the final binary is no larger than its
app slot. No partition was changed because the physical module is unidentified.

After board identity, partition fit, wiring, and explicit authorization are
confirmed, set each variable to its own confirmed port and use:

```powershell
py -3.13 -m platformio run -d hardware\arduino\smart_home_lock -e uno -t upload --upload-port $unoPort
py -3.13 -m platformio run -d hardware\esp32\smart_home_gateway -e esp32dev -t upload --upload-port $esp32Port
```

Monitor the ESP32 USB serial console at 115200 baud:

```powershell
py -3.13 -m platformio device monitor --port $esp32Port --baud 115200
```

The Uno sketch does not call `Serial.begin()` and emits no USB serial console
output. Its inter-board `SoftwareSerial` is 9600 baud on A2/A3. To observe that
link, use the ESP32 console or a receive-only analyzer with the required
voltage protection; do not connect an analyzer TX while the gateway is driving.
Only one program may own a COM port at a time.

### Upload recovery

1. Stop the monitor/IDE using the port; verify the USB cable carries data,
   reconnect the board, and re-identify the COM port.
2. For Uno upload errors, verify environment `uno`, remove any external wiring
   from D0/D1, close other serial clients, and retry only after the wiring
   checklist is safe.
3. For ESP32 connection errors, verify the exact ESP32 board/port and USB
   driver. If that board has a BOOT button, hold it only when the uploader is
   waiting to connect, then release when writing starts. If needed, reduce
   `upload_speed` for the confirmed board; do not change flash capacity to
   bypass a partition-size failure.
4. If the ESP32 image is larger than its app partition, stop. Do not attempt
   repeated uploads or use `erase-flash`/full-chip erase as a recovery shortcut:
   NVS stores active-command recovery and queued audit state.
5. After any successful upload, inspect serial output and run the bench tests
   with the horn disconnected. No firmware is flashed by these instructions.

## Flutter emulator and physical modes

For a Flutter build pointed at the local emulators, explicitly pass
`FIREBASE_EMULATOR_HOST` (and optionally `FIREBASE_EMULATOR_PROJECT_ID`). On
the Android Emulator use `10.0.2.2` to reach the host computer; do not omit the
flag if you intend an emulator-only test:

```powershell
flutter run --dart-define=FIREBASE_EMULATOR_HOST=10.0.2.2 --dart-define=FIREBASE_EMULATOR_PROJECT_ID=demo-smart-home
```

The sample app's administrator UID is fixed to the UID allowed by the proposed
rules. A usable local Auth-emulator administrator must therefore be created
with that UID through a local emulator-only admin procedure; ordinary sign-up
generates a different UID and is correctly denied. This repository does not
automatically create that user.

Physical mode requires a separate explicit build define, a test Auth identity,
a matching enabled registry entry, proposed rules deployed only to an isolated
test database, and a fresh Uno PONG. Do not combine a production Firebase
configuration with the hardware-mode define without explicit approval.

## Identity revocation

For an isolated test device, set
`deviceRegistry/<uid>/enabled` to `false`; the next rule-checked write must be
denied. Then disable/delete the emulator Auth account. Production revocation
and credential provisioning are out of scope and have not been done.
