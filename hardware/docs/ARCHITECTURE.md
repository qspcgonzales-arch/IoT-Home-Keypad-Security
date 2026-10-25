# ESP32-only architecture (Milestone 6.6)

**This is the new target architecture, not a record of completed physical
integration.** Wiring, voltage levels, board identity, component behavior, and
servo mechanics remain unverified. Do not connect to an occupied door, flash
hardware, connect to Firebase, or remotely actuate a servo as part of this
offline preparation.

```text
Local keypad PIN ───────┐
                       ├── ESP32-WROOM-32 ── local LCD status
TCRT5000 digital input ┤                 ├── active buzzer (driver if needed)
                       │                 └── SG90 signal (separate servo supply)
                       └── standalone local diagnostics

Flutter / Firebase: not connected in the Milestone 6.6 standalone build
Arduino Uno / UART: not part of the new hardware path
```

The ESP32 directly owns the 4x4 keypad, 16x2 I2C LCD, digital TCRT5000 input,
active buzzer, and SG90 signal. The standalone firmware has no Wi-Fi, Firebase,
UART, cloud commands, or actuator acknowledgement path. Local keypad
verification uses a salted SHA-256 digest with a constant-time comparison;
the PIN value is neither committed nor logged. Three failed attempts trigger
a 30-second lockout, a valid local PIN starts a four-second logical unlock,
and the device automatically relocks.

## Firmware paths

| Path | Purpose | Status |
| --- | --- | --- |
| `hardware/esp32/esp32_only` | ESP32-only local diagnostic build for this migration | New; offline; physical test pending |
| `hardware/esp32/smart_home_gateway` | Existing ESP32/Firebase gateway using Arduino UART PING/PONG | Historical/legacy architecture; do not mix with ESP32-only pin map |
| `hardware/arduino/smart_home_lock` | Existing Uno keypad/actuator controller and UART endpoint | Historical/legacy architecture; retained unchanged |

The UART firmware remains available in the repository and its commit history;
the new project does not repurpose its build environment or silently change
its protocol. The current standalone build compiles servo actuation out. A
separate supervised servo-test environment is disarmed at boot, requires all
four local peripheral checks to be confirmed, and is not a cloud integration
target.

## ESP32-only pin map

| GPIO | Function | Review |
| --- | --- | --- |
| 32, 33, 25, 26 | Keypad rows 1–4 | Requested assignment; confirm matrix order against wiring |
| 27, 14, 23, 13 | Keypad columns 1–4 | GPIO23 replaces requested GPIO12; do not assume rewiring occurred |
| 19 | Active buzzer signal | Verify signal polarity and current; driver may be needed |
| 18 | SG90 signal | Actuation is compiled out of standalone build |
| 34 | TCRT5000 digital output | Input-only, no internal pull-up/down; external output must have defined 3.3 V logic |
| 21 | LCD SDA | Verify bus pull-up voltage |
| 22 | LCD SCL | Verify bus pull-up voltage |

GPIO12 is an ESP32 strapping pin; a keypad circuit can disturb boot selection.
The firmware and recommended wiring use GPIO23 instead. Confirm the physical
column is actually moved before powering the assembled board. GPIO34 is
input-only and must not be configured with an internal pull resistor.

## Electrical and power constraints

- ESP32 GPIOs are not 5 V tolerant. Many 5 V LCD I2C backpacks pull SDA/SCL up
  to 5 V. Measure the exact module and use a suitable I2C level shifter or
  confirmed 3.3 V pull-ups; never apply 5 V directly to GPIO21/22.
- Verify the exact TCRT5000 board's supply and digital output levels. GPIO34
  must see a defined low and high that remain within ESP32 input limits.
- Power the SG90/clone from a separate regulated supply sized to the exact
  servo's stall-current requirement. Do not use an ESP32 GPIO, 3.3 V pin, or
  USB rail as servo power. Share the signal reference ground, but route servo
  current back to its own supply. Keep the horn unloaded and disconnected from
  any mechanism during a separately approved supervised test.
- Identify whether the buzzer is active or passive and its current. The
  firmware assumes active-high on/off behavior. Add a transistor driver when
  the load exceeds the ESP32 board's permitted GPIO current.
- Verify exact module polarity, voltage, and board pin labels against
  datasheets and measurements. Wire colors are not evidence.

## Safe startup and actuation policy

At boot, the buzzer is set low, servo signal is set low, the sensor is input
only, and no servo library is attached or commanded. The default
`esp32dev_standalone` image has no servo dependency at runtime and implements
only logical lock-state timing. The separately compiled
`esp32dev_supervised_servo_test` can attach the servo only after all local
keypad, LCD, sensor, and buzzer tests have been confirmed and an operator
enters `ARM SERVO` on the local serial console. A local valid PIN is then
required to request movement. Never issue servo commands remotely.

The logical lock state is not proof of bolt position. The TCRT5000's exact
physical meaning and placement must be established before interpreting it as
door/lock state. A hobby servo is not a fail-secure lock and must not be
connected to an occupied entrance.

## Cloud migration boundary

No staging project, least-privilege device account, or administrator account
was supplied for this milestone. The local image intentionally has no cloud
credentials or network code. Before a separate cloud milestone, preserve the
existing gateway's Firebase authentication, device identity and registry,
command expiry/validation, acknowledgement sequence, heartbeat, and audit
schema while replacing the UART endpoint with explicit local peripheral
events. Add outage, stale-heartbeat, expiry, and failed-ack tests; begin with
remote actuation disabled. Do not alter production rules/accounts or Flutter
as part of the local firmware migration.

See [ESP32_ONLY_ACCEPTANCE.md](./ESP32_ONLY_ACCEPTANCE.md) for sequential
component checks, exact local build/test commands, electrical blockers, and
the separately supervised unloaded-servo procedure.
