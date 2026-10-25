# Milestone 6.6 — ESP32-only local acceptance

**Status: source and offline-build preparation only.** No firmware was flashed,
no component was physically tested, no servo was actuated, and no Firebase
connection or command was made. The user-provided wiring has not been
independently observed. This procedure is for an unoccupied bench setup only;
do not connect a latch, door, or occupied entrance.

## Architecture and scope

The new project at `hardware/esp32/esp32_only` replaces the Uno as the
controller for local keypad, LCD, TCRT5000 digital output, active buzzer, and
SG90 signal. The existing UART gateway and Uno sketch remain an explicitly
separate Milestone 6.5/legacy path. Do not combine their binaries, wiring, or
acceptance procedures.

`esp32dev_standalone` is the initial diagnostic image: it has no Wi-Fi or
Firebase dependencies and compiles out servo actuation. The local PIN check
uses salted SHA-256 and a constant-time digest comparison. Failed PIN attempts
are limited to three before a 30-second lockout; a successful local PIN
provides a logical four-second unlock and automatic relock. No plaintext PIN
is printed or stored in the committed source.

`esp32dev_supervised_servo_test` includes servo support but boots disarmed.
It permits local arming only after the keypad, LCD, sensor, and buzzer checks
have all been confirmed. A valid local PIN then operates the unloaded servo.
This environment is only prepared for a later, separately approved,
supervised bench test; it is not a remote-control target.

## Pin assignments

The requested GPIO12 keypad column is **not retained**: move that column to
GPIO23 in both firmware and the wiring plan. GPIO12 is an ESP32 boot-strapping
pin. The selected GPIO23 is a recommendation only; no physical rewiring is
assumed. Verify the exact board pin labels and continuity before connection.

| Peripheral | ESP32-WROOM-32 GPIO | Notes |
| --- | --- | --- |
| Keypad rows 1–4 | 32, 33, 25, 26 | Matrix order must match the firmware key map |
| Keypad columns 1–4 | 27, 14, **23**, 13 | GPIO23 substitutes for requested strapping GPIO12 |
| Active buzzer signal | 19 | Confirm active-high/low behavior and current; transistor driver may be required |
| SG90 signal | 18 | Servo is absent from the standalone image; external supply for supervised test |
| TCRT5000 digital output | 34 | Input only, no internal pull-up/down; provide a defined 3.3 V-compatible level |
| LCD SDA | 21 | ESP32 GPIOs are not 5 V tolerant |
| LCD SCL | 22 | Confirm pull-ups do not source 5 V to the ESP32 |

All electrical details remain subject to exact module/board datasheets and
measurement. Connect grounds as appropriate for the signal reference, but
keep servo current out of the ESP32 ground/power path.

## Electrical blockers to clear before wiring or powering

1. **GPIO12:** Confirm the keypad column was physically moved to GPIO23. Do not
   power the board with a keypad or external circuit pulling GPIO12 to an
   incompatible strap level.
2. **GPIO34 sensor input:** It cannot drive outputs and has no internal
   pull-up/down. Verify the sensor module's output is never above the exact
   ESP32 input maximum and has a stable defined low and high; add an external
   pull resistor or level interface only after checking the module datasheet.
3. **LCD I2C:** Inspect the exact backpack and measure SDA/SCL pull-up voltage.
   Many 5 V LCD backpacks pull the bus to 5 V. Use a suitable bidirectional
   I2C level shifter or verified 3.3 V pull-ups; do not connect 5 V I2C directly
   to ESP32 GPIO21/22.
4. **Servo power:** Use a separate regulated supply sized from the exact
   SG90/clone datasheet stall current. Do not power it from the ESP32 3.3 V,
   USB rail, or a GPIO. Share signal ground, verify polarity, and keep the horn
   unloaded and physically disconnected from any mechanism.
5. **Buzzer drive:** Identify the buzzer's voltage, active polarity, and
   current. Use a suitable transistor driver when its current exceeds the
   board's permitted GPIO load. Never assume a module is 3.3 V safe.
6. **Physical board/flash:** `esp32dev` selects a 4 MB build profile; it does
   not prove the actual board model or flash size. Query the physical chip
   before any future upload.

If any module identity, voltage, polarity, or current limit remains unknown,
stop before wiring or powering it.

## PIN verifier provisioning

Use a dedicated local test PIN; do not reuse an account, door, or production
PIN. The helper prompts without echo, verifies the entry twice, creates a
random salt, and writes only the salted digest and salt to the ignored local
`src/pin_config.h`. It does not print the PIN.

From the repository root:

```powershell
python tools\esp32_only\provision_pin.py
```

The generated header is gitignored. Verify it remains untracked/ignored; never
copy it into a tracked file, logs, a cloud record, or firmware documentation.
Without it, the PIN verifier fails closed and PIN entry cannot unlock.

## Offline build and verification

From the repository root, build the default safe image:

```powershell
py -3 -m platformio run -d hardware\esp32\esp32_only -e esp32dev_standalone
```

The resulting image is
`hardware\esp32\esp32_only\.pio\build\esp32dev_standalone\firmware.bin`.
For the 4 MB `ota_nofs_4MB.csv` layout, the app slot is 2,031,616 bytes.
Compare the complete `.bin` file size to that slot after each source or
toolchain change. This verifies only the selected build profile, not physical
flash capacity.

Run software contract and PIN-provisioning tests:

```powershell
Push-Location tools\device_simulator
npm test
Pop-Location
python -m unittest discover -s tools\esp32_only -p test_*.py
```

Do **not** run an upload command as part of this milestone. When hardware
testing is separately authorized, identify the exact board/COM port and
physical flash size first. No upload or live actuation is implied by a passing
build.

## Sequential component acceptance

Only proceed after the electrical blockers above are cleared. Start with the
standalone environment, servo disconnected, and no door/latch connected.
These are planned steps, not recorded passes.

1. **Power-off inspection:** Compare every wire to the table; check for shorts,
   polarity, board model, 3.3 V logic compatibility, common signal ground, and
   isolated servo supply. Record board markings and component part numbers.
2. **First boot:** Connect only USB and the known-safe ESP32 peripherals.
   Confirm 115200 baud Serial Monitor reports local diagnostics, PIN verifier
   status, pending component checks, and disarmed/compiled-out servo. Stop for
   resets, heat, smoke, unexpected buzzer output, or an unexpected display.
3. **LCD:** Enter `test lcd`. Confirm all characters and both rows are legible.
   Only then enter `confirm lcd`. If contrast, I2C address, or bus voltage is
   wrong, stop and correct it with power removed.
4. **Keypad:** Enter `test keypad`. Press all 16 keys once and confirm the
   firmware reports activity without echoing or logging key values; then
   `confirm keypad`. The later local PIN step verifies that digit mapping
   matches the selected test PIN.
5. **TCRT5000:** Enter `test sensor`. Move a safe target through both sensor
   states and observe stable `HIGH` and `LOW` output reports. Then enter
   `confirm sensor`. If either state is missing or the output voltage is
   unsafe, stop; GPIO34 has no internal bias.
6. **Buzzer:** Enter `test buzzer`; listen for the three short beeps. Enter
   `confirm buzzer` only if the expected output is audible and the driver/
   current/temperature are safe. If no beep or unexpected continuous output,
   stop.
7. **Status gate:** Enter `status`. Confirm all four checks show `PASS`.
   Repeat only the failed component test after correcting the cause with power
   removed.
8. **Local PIN behavior (servo still disconnected):** With a local verifier
   provisioned, try one incorrect PIN and confirm only a redacted denial is
   logged. Enter the correct test PIN; verify a logical unlock indication and
   automatic relock at four seconds. Three incorrect attempts must trigger
   the 30-second lockout. Do not capture or share PIN input.
9. **Stop and record:** Power down and record firmware hash, exact board,
   module identities, measured voltages, and observed results. Do not mark
   physical steps passed based on source inspection or a build.

## Separately approved unloaded-servo test (not authorized/performed here)

Requires a distinct explicit approval and an on-site teammate beside the
hardware. Keep a manual power disconnect within reach, use an unloaded servo
horn with no linkage, and verify the correct `esp32dev_supervised_servo_test`
image and physical flash profile first. Perform all four component checks
before arming. Keep servo power disconnected during wiring and component
checks; only connect the correctly rated, separately regulated supply during
the supervised servo stage. A local `arm servo` command and a deliberate local
valid-PIN entry are required. Arming leaves the servo unattached; it is
attached only when the local PIN is accepted. Never arm or issue an unlock
remotely. Be ready to disconnect servo power immediately on unexpected
motion, heat, noise, or current. This document does not authorize flashing,
energizing, or movement.

## Cloud integration blockers and migration

The new standalone project deliberately has no Wi-Fi, Firebase, credentials,
UART, cloud command polling, remote actuation, acknowledgement, or audit
writer. Do not point it at production or staging.

Before a later cloud integration:

- Specify and approve an isolated staging Firebase project and distinct
  least-privilege administrator/device identities; none are provided here.
- Integrate network/authentication without embedding administrator credentials.
- Preserve the existing device identity/registry checks, command expiry and
  validation, duplicate handling, acknowledgement lifecycle, heartbeat, and
  audit schema in a separately tested ESP32-only client.
- Report lock state as *reported* state, not proof of physical bolt position;
  define sensor semantics and safe behavior on stale connectivity.
- Keep physical unlock actuation disabled for initial cloud connectivity tests.
- Add tests for network loss, stale heartbeat, expired commands, and failed
  acknowledgements before any remote actuator path is enabled.
- Re-run the offline build/size check and all software tests. Do not change
  Flutter, production Firebase rules, or production accounts as part of this
  local migration.
