# Provisional wiring notes

These are planning notes only. Check each module's voltage and current
requirements before wiring; do not power a servo from an Arduino GPIO.

| Module | Provisional connection |
| --- | --- |
| 4x4 keypad | Rows D2-D5, columns D6-D9 on Uno |
| SG90 signal | D10; servo supply from a suitable 5 V source |
| I2C 16x2 LCD | SDA A4, SCL A5 on Uno |
| IR proximity digital output | D11 |
| Active buzzer | D12 through a suitable driver if required |
| Uno UART to ESP32 | Superseded: see `ARCHITECTURE.md` (SoftwareSerial A2/A3, level shifter) |
| ESP32 UART from Uno | ESP32 TX to Uno RX; verify 3.3 V logic is accepted |

Use a common ground between the Uno, ESP32, servo supply, and sensor supply.
Never connect Uno 5 V TX directly to an ESP32 input. The final pin map depends
on the exact ESP32 board and peripheral variants and must be reviewed before
powering hardware.
