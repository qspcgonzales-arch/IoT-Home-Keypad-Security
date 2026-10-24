# Physical Assembly Checklist

**Status: not assembled or electrically verified.** This checklist reflects
the current firmware source, not a substitute for exact board/module
datasheets. No `.doc` or `.docx` wiring guide is present in the repository;
the comparison baseline is `provisional_wiring.md`. Keep the actuator horn
disconnected from any latch through bench testing. Stop on heat, smell, smoke,
brownout, or unexpected movement.

## Pin-by-pin source map

| Part / signal | Uno R3 connection | ESP32 connection | Source / check |
| --- | --- | --- | --- |
| 4x4 keypad rows | D2, D3, D4, D5 | — | Firmware `rowPins` |
| 4x4 keypad columns | D6, D7, D8, D9 | — | Firmware `colPins` |
| Servo signal | D10 | — | `SERVO_PIN`; signal only |
| Servo supply | Separate regulated 5 V supply sized to the exact servo's datasheet/measured stall current; join signal ground to Uno GND; do not feed Uno 5 V | — | The earlier 1 A value is provisional, not a verified rating; check polarity from labels/datasheet |
| TCRT5000 module | Digital OUT to D11 | — | `IR_PIN`; verify module output voltage and supply from its own datasheet |
| Active buzzer | D12 | — | `BUZZER_PIN`; active buzzer assumed; use a driver if required by measured load |
| LCD I2C | SDA A4, SCL A5, firmware address `0x27`; power on Uno 5 V side | Do not connect its 5 V I2C lines to ESP32 | Confirm backpack address and voltage |
| Uno UART RX | A2 (SoftwareSerial RX), from ESP32 TX through a direction-fixed push-pull buffer/translator | GPIO17 (UART2 TX) | 9600 baud, 8N1 |
| Uno UART TX | A3 (SoftwareSerial TX), to ESP32 RX through a 5 V-to-3.3 V push-pull translator or suitable divider | GPIO16 (UART2 RX) | 9600 baud, 8N1 |
| USB upload / debug | D0/D1 left unused by firmware | — | Uno has no second hardware UART; use A2/A3 for gateway |
| ESP32 power | — | Board USB or regulated supply supported by the exact dev board | `esp32dev` assumes 4 MB flash; identify the physical board/module before flashing |
| Common ground | Uno GND, ESP32 GND, servo supply GND, and sensor/module grounds | Common signal reference | Required before connecting UART |

## Required level and power precautions

- Use a UART-compatible, push-pull level interface in each direction. Uno A3
  TX is 5 V and must never connect directly to ESP32 GPIO16. ESP32 GPIO17 TX
  is 3.3 V; use a buffer/translator that meets the actual Uno input-high
  requirement rather than relying on a marginal direct connection. Do not
  assume a generic bidirectional BSS138/I2C shifter works for push-pull UART.
- Do not connect the servo's positive lead to the Uno 5 V rail or an I/O pin.
  Use a separate, regulated supply sized for the exact servo's stall current;
  join grounds for signal reference while routing servo current back to its
  supply, not through the Uno. A bulk capacitor does not replace correct
  supply sizing.
- Keep the LCD's Uno-side I2C bus at the Uno's 5 V logic level. Do not connect
  it to ESP32 GPIO21/22.
- Verify the TCRT5000 board's VCC and OUT voltage before wiring to D11. The
  module variant was not identified.
- Confirm whether the buzzer is active and whether its current requires a
  transistor driver. Do not drive a high-current load directly from D12.

## Mismatch report

The repository's provisional guide agrees on the firmware pin numbers. The
requested simplified Word guide was not found, so it could not be compared.
Older notes recommended a generic bidirectional shifter for both UART lines;
this is not a safe blanket recommendation for push-pull UART. Use suitable
direction-fixed UART translators (or a verified down-divider plus a suitable
3.3 V-to-Uno buffer) and do not assume an I2C MOSFET shifter is compatible.
No firmware pin change was made. Exact board/module ratings and actual wiring
remain unverified.

## Before applying power

- [ ] Confirm the exact Uno R3 and ESP32 board variants and their pin labels.
- [ ] Confirm GPIO16/GPIO17 are available on the exact ESP32 module; some
      ESP32 variants reserve these pins for PSRAM.
- [ ] Check module datasheets, connector orientation, and polarity.
- [ ] Inspect all wiring with power removed; verify no shorts between supply
      rails and ground.
- [ ] Measure shifter output levels with the UART disconnected; ESP32 RX must
      not exceed its specified maximum.
- [ ] Verify common ground and independent servo supply; keep servo horn
      disconnected from any latch.
- [ ] Confirm the Uno's placeholder PIN hash has been safely replaced before
      keypad acceptance tests. Do not place a plaintext PIN in source or logs.
- [ ] Confirm ESP32 `secrets.h` points only to the isolated emulator for bench
      work; the example contains placeholders and is not a credential.
- [ ] Keep production rules, production device identity, and remote hardware
      mode inactive.
- [ ] Confirm the ESP32 board's flash ID/capacity and exact module variant. Do
      not flash with the current default ESP32 partition; the clean-build image
      exceeds its application slot.
- [ ] Keep the servo horn disconnected from every latch and keep the assembly
      away from occupied building doors. Confirm a manual mechanical release
      plan before any later mechanism integration.

## Hardware acceptance tests

Complete in order. Mark each item only after observing the physical result.
The Uno firmware does not print successful UART traffic to its USB serial
port; use the ESP32 USB monitor at 115200 baud or a correctly level-shifted,
receive-only UART analyzer at 9600 baud. Do not connect a monitor's TX while
another device is driving that UART.

| # / test | Procedure and expected output | Failure symptoms and troubleshooting | Result |
| --- | --- | --- | --- |
| 1. LCD | Power the Uno with the LCD on its 5 V side. On boot expect backlight and `READY - ENTER PIN`; on a state change expect `DOOR LOCKED` or `DOOR UNLOCKED`. | Blank/no backlight: check power and polarity. Backlight but no text: check A4 SDA/A5 SCL, common ground, 0x27 address, and backpack. Keep the LCD bus off ESP32 GPIOs. | [ ] Pass [ ] Fail [x] Not run |
| 2. Keypad | Keep servo unplugged. Provision a temporary test PIN hash/salt (the committed placeholder intentionally cannot match); enter the test PIN and `#`, then verify `*` clears and three bad attempts trigger lockout. No PIN is displayed. | Wrong keys: check row D2-D5 and column D6-D9 order and connector orientation. No access: verify the temporary hash format/salt and 4-digit entry. Do not use a production PIN. | [ ] Pass [ ] Fail [x] Not run |
| 3. Active buzzer | Confirm module type, polarity, and current before connecting D12. Boot/state changes and denied attempts should produce short on/off beeps; lockout uses a longer beep. | Silent: check active-vs-passive type, polarity, D12, and ground. Weak output/reset: use a suitably rated transistor driver; do not exceed the Uno pin rating. | [ ] Pass [ ] Fail [x] Not run |
| 4. TCRT5000 | Check module voltage/output datasheet, then connect digital OUT to D11 and ground to Uno GND. Move a target through detection range while observing UART events; expect one `irProximity` event per HIGH/LOW transition. | No/constant events: adjust threshold/target distance and verify OUT actively drives both levels. Firmware uses `INPUT`, not `INPUT_PULLUP`; an open-collector output may need a datasheet-approved pull-up. Ensure OUT is within Uno input limits. | [ ] Pass [ ] Fail [x] Not run |
| 5. SG90 with external power | Keep horn disconnected from any linkage. Use a separate regulated supply sized for the exact servo's stall current. Share ground with Uno; never join servo +5 V to Uno +5 V. Boot requests 0 degrees; test unlock requests 90 degrees, then relocks. | Jitter, brownout, heat, or unexpected motion: stop; check supply current/voltage, polarity, common ground, signal D10, and free movement. Do not connect a latch. | [ ] Pass [ ] Fail [x] Not run |
| 6. Arduino-to-ESP32 UART | With both boards off, cross TX-to-RX: Uno A3 to ESP32 GPIO16 through 5 V-to-3.3 V conversion; ESP32 GPIO17 to Uno A2 through a suitable 3.3 V-to-Uno buffer. Join grounds. Inspect JSON Lines at 9600, 8N1. | No/garbled bytes: check crossed directions, converter direction/rails, common ground, 9600 8N1, pin conflicts, and line endings. Never connect Uno 5 V TX directly to ESP32 RX. | [ ] Pass [ ] Fail [x] Not run |
| 7. PING/PONG heartbeat | With UART connected and servo unloaded, expect one-second `PING` requests and a matching `PONG` with the same ID and valid door value. After more than 3 seconds without a matching PONG, firmware readiness becomes false; verify the next isolated-emulator heartbeat reports that state. | Stale/offline: inspect UART traces, IDs, framing, Uno boot state, and logic levels. Invalid/stale replies are logged on ESP32 serial; valid PONGs are not printed as success. | [ ] Pass [ ] Fail [x] Not run |
| 8. Wi-Fi | Use a dedicated isolated 2.4 GHz test network; check the ESP32 receives a lease and can reach only the isolated test services. Interrupt/recover the AP and verify reconnection without reboot. Do not issue a command in this test. | No lease/recovery: check SSID/password, 2.4 GHz support, signal, DHCP, and retry interval. Firmware has no explicit log for every successful association; confirm at the AP and later via emulator heartbeat. | [ ] Pass [ ] Fail [x] Not run |
| 9. ESP32 Auth in Firebase Emulator | Use only a preconfigured isolated Auth + RTDB emulator reachable on the lab network, a dedicated non-admin device account, and its matching enabled registry entry. Expect successful Auth, registry validation, and a fresh gateway heartbeat. | Auth denied: check emulator host/ports, account, UID/deviceId registry match, time sync, and firewall. The automated emulator binds loopback and is not reachable from a physical ESP32. If isolated test policy is not already configured, stop; do not change rules or point to production. | [ ] Pass [ ] Fail [x] Not run |
| 10. Remote unlock, unloaded servo | Only after tests 0-9 pass and emulator-only configuration is independently confirmed, manually issue one short-lived command. Keep horn disconnected and no latch/door attached. Expect one ID, accepted then executed acknowledgement, and emulator completion after the firmware acknowledgement sequence. | Pending/failed: inspect PONG, expiry, registry, UART trace, and acknowledgement order. Any motion without the test command, wrong endpoint, or unexpected output means stop and remove power. Never retry against production. | [ ] Pass [ ] Fail [x] Not run |
| 11. Four-second relock | With no latch attached, use the isolated test setting of 4 seconds; a missing setting defaults to 4 seconds. Time from the Arduino unlock request to the locked state and record the measured result. | Early/late/no relock: verify received duration, `STATE` sequence, timer measurement, and that the servo is unloaded. Firebase `completed` means the command was acknowledged; it does not prove physical movement or latch state. | [ ] Pass [ ] Fail [x] Not run |

Record the board/module markings, firmware build ID, test date, supply
voltages, measured servo current, UART trace, measured relock time, and
operator. Do not proceed to a latch or door mechanism until the unloaded
bench stages pass and a manual mechanical release has been demonstrated.

## Current physical verification

No continuity, voltage, current, UART, component, servo, or lock checks were
performed for this report. All checkboxes remain unchecked until observed.
