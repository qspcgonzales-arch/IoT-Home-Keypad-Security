# Milestone 4 virtual hardware

The virtual Uno and ESP32 use the Milestone 3 Firebase paths and command
records. Start from `tools/device_simulator` in PowerShell:

```powershell
. ..\..\hardware\virtual\set_pin.ps1
$env:GOOGLE_APPLICATION_CREDENTIALS = 'C:\path\outside\project\simulator-service-account.json'
npm run start:virtual
```

Use `/pin 2468`, `/wifi off`, `/wifi on`, `/ir on`, `/ir off`, and `/quit` at
the simulator prompt. Replace `2468` with the PIN selected in the hidden prompt.
The PIN hash and random salt exist only in that PowerShell process environment;
the PIN is not placed in the source, database, audit events, or UART records.
Keep the service account file outside the repository.

Before deploying the local `database.rules.json` to Firebase Console, review
and publish the added `accessGranted`, `accessDenied`, and `securityLockout`
audit event types, plus the admin-writable setting
`settings/frontDoor/unlockDurationSeconds` (whole seconds, 4–30; default 4).
The simulator reads this setting for keypad and remote unlocks. Existing paths
and remote command records remain unchanged.

`hardware/arduino` and `hardware/esp32` are firmware preparation. Read
`docs/REFERENCE_RESEARCH.md` (sources) and `docs/ARCHITECTURE.md` (pin map, level
shifting, servo power, open decisions) first. Everything electrical is unverified
until hardware testing.
Arduino firmware needs the Keypad, LiquidCrystal_I2C, Servo, ArduinoJson, and
SHA256 (Crypto) libraries. The ESP32 PlatformIO profile uses ArduinoJson. The
ESP32 firmware target still needs its Wi-Fi, time synchronization, secure
Firebase identity/provisioning, command listener, and Firebase event publishing
implementation; `sendUnlockToUno` and the UART parser are protocol hooks. The
virtual gateway uses the trusted local simulator identity and Firebase Admin
integration from Milestone 3.
