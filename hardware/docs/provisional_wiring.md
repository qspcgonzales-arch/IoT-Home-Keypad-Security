# Provisional ESP32-only wiring (not physically verified)

The ESP32-only firmware is a distinct Milestone 6.6 path. Do not use this
pinout with the legacy Uno/UART firmware. The requested GPIO assignments have
not been independently checked against the assembled board; confirm exact
board pin labels, module datasheets, polarity, and continuity before wiring.

| Component | ESP32-WROOM-32 connection | Electrical notes |
| --- | --- | --- |
| 4x4 keypad rows | R1–R4: GPIO32, GPIO33, GPIO25, GPIO26 | Confirm row/column order with the keypad's datasheet |
| 4x4 keypad columns | C1–C4: GPIO27, GPIO14, **GPIO23**, GPIO13 | GPIO23 replaces requested GPIO12; do not assume rewiring has occurred |
| Active buzzer signal | GPIO19 | Verify voltage, active polarity, and current; use transistor driver if needed |
| SG90 signal | GPIO18 | Servo supply is separate; keep horn unloaded |
| TCRT5000 digital output | GPIO34 | Input only; no internal pull-up/down; defined 3.3 V-compatible output required |
| 16x2 I2C LCD SDA | GPIO21 | ESP32 GPIO is not 5 V tolerant; verify pull-up voltage |
| 16x2 I2C LCD SCL | GPIO22 | ESP32 GPIO is not 5 V tolerant; verify pull-up voltage |

## Mandatory electrical checks

- **GPIO12 boot strapping:** The originally requested keypad column GPIO12 is
  not recommended. The firmware selects GPIO23, a non-strapping alternative.
  Physically confirm the keypad wire is moved to GPIO23 before power-on; no
  physical rewiring is assumed or claimed.
- **GPIO34:** It cannot drive outputs and has no internal pull-up/down. Verify
  the exact TCRT5000 module output voltage under every state. It must remain
  within the ESP32 input limits and provide a stable defined high and low.
  Provide an external bias/interface only when the module datasheet confirms
  the proper circuit.
- **LCD I2C:** Inspect and measure the exact backpack's SDA/SCL pull-ups. Many
  5 V backpacks pull to 5 V. Use a suitable bidirectional I2C level shifter or
  verified 3.3 V pull-ups. Do not connect a 5 V bus to ESP32 GPIO21/22.
- **Servo:** Power the servo from a separate regulated supply sized from the
  exact SG90/clone stall-current specification. Never power it from an ESP32
  GPIO, 3.3 V output, or USB rail. Join signal grounds; route servo current
  directly to its supply. Check polarity and keep the horn unloaded with no
  linkage.
- **Buzzer:** Confirm active/passive type, voltage, polarity, and current. The
  test firmware assumes an active buzzer. Use a transistor driver if direct
  GPIO current is above the board's safe rating.
- Never infer polarity or logic levels from wire color. If a board/module
  identity or electrical limit is unknown, stop before powering.

## Legacy wiring is separate

The old A2/A3 Uno ↔ GPIO16/17 ESP32 UART and Uno-centric peripheral wiring
belongs only to `hardware/esp32/smart_home_gateway` and
`hardware/arduino/smart_home_lock`. It is documented in the Milestone 6.5
acceptance record and is not part of this ESP32-only pin map. Do not combine
the wiring diagrams or build targets.
