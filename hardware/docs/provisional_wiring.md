# Provisional wiring notes (source-matched, not physically verified)

These are planning notes only. Check each module's voltage and current
requirements before wiring; do not power a servo from an Arduino GPIO.

| Module | Provisional connection |
| --- | --- |
| 4x4 keypad | Rows D2-D5, columns D6-D9 on Uno |
| SG90 signal | D10; servo power from a separate regulated 5 V supply sized for the exact servo's stall current (1 A is only a provisional floor, not a verified rating) |
| I2C 16x2 LCD | SDA A4, SCL A5 on Uno; firmware uses address `0x27` |
| TCRT5000 module | Digital output to D11; verify the exact module's supply and output levels |
| Active buzzer | D12; active type assumed, use a suitable driver if current exceeds the pin rating |
| Uno UART | `SoftwareSerial`: Uno RX A2, TX A3; 9600 baud, 8N1 |
| ESP32 UART2 | RX GPIO16, TX GPIO17; 9600 baud, 8N1 |
| UART level conversion | Convert both crossed TX/RX signals with a direction-fixed, push-pull UART-compatible interface. Uno A3 TX must be reduced to ESP32 3.3 V GPIO16 levels; ESP32 GPIO17 TX must meet the Uno A2 input-high requirement. Do not use a generic I2C BSS138 shifter unless its datasheet explicitly supports push-pull UART. |

Connect grounds for the Uno, ESP32, servo supply, and sensor supply. Do not
connect the servo's positive supply to the Uno 5 V rail. Keep the LCD I2C bus
on the Uno's 5 V side; do not connect it to ESP32 GPIO21/22. The source files
and existing notes match on the pin numbers above; the earlier note to merely
"verify" ESP32 3.3 V TX acceptance understated the risk. Use a translator
appropriate for push-pull UART in both directions; a passive I2C-style
bidirectional board is not an assumed substitute. Return servo current directly
to its supply, with a shared ground reference to the Uno; do not route servo
current through the Uno ground path. Verify every supply polarity from its
board markings and datasheet, not wire color. No occupied door or latch may be
connected during bench tests. Board/module variants, electrical levels, power,
and physical wiring remain unverified.
