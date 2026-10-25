# Milestone 4 virtual hardware

## Milestone 6.6 ESP32-only firmware

The new local-only ESP32 design is in `esp32\esp32_only`. Read
`docs\ARCHITECTURE.md`, `docs\provisional_wiring.md`, and
`docs\ESP32_ONLY_ACCEPTANCE.md` before any hardware work. It replaces the Uno
as peripheral controller in the new path; the existing Uno/UART firmware
remains a separate historical path. The standalone diagnostic build has no
Wi-Fi/Firebase or servo actuation. Neither firmware flashing nor physical
testing has been performed for Milestone 6.6.

The virtual Uno and ESP32 use the Milestone 3 Firebase paths and command
records. Start from `tools/device_simulator` in PowerShell:

```powershell
. ..\..\hardware\virtual\set_pin.ps1
$env:GOOGLE_APPLICATION_CREDENTIALS = '<external-service-account-file>'
npm run start:virtual
```

Use `/pin <chosen-local-test-PIN>`, `/wifi off`, `/wifi on`, `/ir on`, `/ir off`,
and `/quit` at the simulator prompt. Replace the placeholder with the PIN
selected in the hidden prompt.
The PIN hash and random salt exist only in that PowerShell process environment;
the PIN is not placed in the source, database, audit events, or UART records.
Keep the service account file outside the repository.

Before deploying the local `database.rules.json` to Firebase Console, review
and publish the added `accessGranted`, `accessDenied`, and `securityLockout`
audit event types, plus the admin-writable setting
`settings/frontDoor/unlockDurationSeconds` (whole seconds, 4–30; default 4).
The simulator reads this setting for keypad and remote unlocks. Existing paths
and remote command records remain unchanged.

The legacy Uno and ESP32 gateway firmware remain in their original paths for
the earlier two-controller design. That gateway's UART parser and
`sendUnlockToUno` path are separate from `esp32\esp32_only`; do not mix the
firmware or pin maps. The ESP32-only project uses PlatformIO-managed Keypad,
LiquidCrystal_I2C, and (only in the supervised profile) ESP32Servo libraries.
Its offline diagnostic image has no Wi-Fi/Firebase integration. All electrical
details remain unverified until measured on the exact hardware.

The legacy gateway's Firebase, time synchronization, identity/provisioning,
command listener, and event publishing still require separately approved
cloud integration. The virtual gateway uses the local simulator identity and
Firebase Admin integration from Milestone 3; it is not a physical device
credential.
