# Uno R3 + ESP32 architecture (provisional, unverified)

**Nothing in this document has been tested on hardware.** Pin, voltage, current and timing statements
are design proposals. Items marked [Source] come from `REFERENCE_RESEARCH.md`; the rest are
[Assumption] based on general datasheet knowledge and must be checked against the exact boards.

```
Flutter app â”€â”€(Firebase Auth + RTDB, unchanged contract)â”€â”€ Firebase â”€â”€â”€â”€ ESP32 gateway
                                                                          â”‚ UART, 3.3 V side
                                                              level shifter (5 V â†” 3.3 V)
                                                                          â”‚ UART, 5 V side
                                                                     Uno R3 lock controller
                                          keypad Â· I2C LCD Â· IR sensor Â· buzzer Â· servo (own 5 V supply)
```

- **Uno** owns everything that must work offline: PIN check (salted hash), 3-attempt/30 s lockout,
  the relock timer, the servo, LCD and buzzer. It never touches Wi-Fi or Firebase.
- **ESP32** owns Wi-Fi, time, Firebase identity and the command/audit mapping. It cannot actuate the servo;
  it can only send a validated `UNLOCK` line to the Uno.
- The Firebase contract and `database.rules.json` are unchanged by this design.

## Provisional Uno R3 pin map

| Pin | Function | Notes |
| --- | --- | --- |
| D0 / D1 | USB serial (upload and debug) | **Left free.** Using them for the ESP32 link blocks sketch upload and shares the USB serial port |
| D2â€“D5 | Keypad rows | Same group as source 5 (D2â€“D9) |
| D6â€“D9 | Keypad columns | [Source 5] |
| D10 | Servo signal | [Source 5] |
| D11 | IR proximity digital output | |
| D12 | Buzzer through a transistor driver if the buzzer draws more than ~20 mA | [Assumption] |
| D13 | Free | The on-board LED shares it |
| A0, A1 | Free | |
| A2 | `SoftwareSerial` RX (from ESP32 TX, via level shifter) | Analog pins double as digital and support pin-change interrupts |
| A3 | `SoftwareSerial` TX (to ESP32 RX, via level shifter) | |
| A4 / A5 | I2C SDA / SCL to the LCD | [Source 5] |

**Conflicts found and resolved in the proposal**

1. The existing firmware used the hardware `Serial` (D0/D1) at 115200 for the ESP32 link. That conflicts with USB upload and debugging, and the Uno has no second hardware UART. Proposal: `SoftwareSerial` on A2/A3 at **9600 baud**.
2. `SoftwareSerial` disables interrupts while sending, and the `Servo` library also relies on timer interrupts, so servo jitter or dropped bytes are possible. Messages are short and 9600 baud is conservative, but this is **unverified**; test with the servo moving. See the risk assessment in `SECURITY_ARCHITECTURE.md` section 3.
3. The firmware no longer calls `tone()`. The buzzer is now driven on/off with `digitalWrite` and `millis()` (an **active** buzzer is assumed; a passive buzzer would only click). This removes the Timer2 ISR from the picture. [Proposed modification, compiled, unverified on hardware]
4. The `Servo` library disables PWM (`analogWrite`) on D9 and D10. No PWM is used on those pins.

## Provisional ESP32 DevKit pin map

| GPIO | Function | Notes |
| --- | --- | --- |
| 16 (RX2) | UART from Uno | Through the level shifter |
| 17 (TX2) | UART to Uno | Through the level shifter |
| 2 | Optional status LED | On-board LED on many DevKits; a strapping pin, do not drive it hard during boot |
| 21 / 22 | I2C (reserved, unused) | Do **not** connect the 5 V LCD I2C bus here |
| all others | Unused | Avoid GPIO 0, 2, 5, 12, 15 (strapping), 6â€“11 (flash) and 34â€“39 (input only) for outputs |

The reference projects use GPIO 13 for a servo (sources 3, 4, 8) and several strapping pins for keypads
(sources 1, 3). Our design avoids both: the ESP32 has no actuator or keypad.

## Voltage-level conversion

| Signal | Levels | Action |
| --- | --- | --- |
| Uno TX (A3) â†’ ESP32 RX (GPIO16) | 5 V â†’ 3.3 V input (not 5 V tolerant) | **Required:** level shifter or a resistor divider (for example 1 kÎ© + 2 kÎ©). Never connect directly |
| ESP32 TX (GPIO17) â†’ Uno RX (A2) | 3.3 V â†’ 5 V logic | The ATmega328P input-high threshold is about 0.6Ã—VCC (â‰ˆ3 V), so 3.3 V is marginal. Prefer a bidirectional shifter for both lines |
| I2C LCD | 5 V on the Uno bus only | Keep it off the ESP32 |
| Common ground | Uno, ESP32, servo supply, sensors | **Required** |

## Power

| Load | Proposal | Reason |
| --- | --- | --- |
| Servo (SG90) | Separate 5 V supply rated at 1 A or more, ground joined to the Uno GND, + **not** connected to the Uno 5V pin | [Source 5]: about 100 mA free, up to about 650 mA at stall; USB can brown out the Uno |
| Add a bulk capacitor (for example 470â€“1000 ÂµF) across the servo supply | [Assumption] | Reduces supply dips when the servo starts |
| ESP32 | Own USB or regulated 5 V supply | Wi-Fi peaks are current-hungry [Assumption] |
| Uno, LCD, IR, keypad | USB or a 7â€“12 V barrel jack | Low current |
| Real door lock (solenoid or strike) | **Out of scope.** Needs its own supply, a MOSFET/relay driver and a flyback diode; the servo is a bench stand-in only | [Source 1] mentions a 12 V solenoid with a driver; [Source 5] says a servo is a demo |

Fail-safe behaviour on power loss, and whether the lock should be fail-secure or fail-open, are
## Lock mechanism and power-loss behaviour (design intent, unverified)

The demonstration lock is intended to be **fail-secure with manual release**: with all electrical power
removed, the door stays mechanically locked, and a person on the inside (or with a key) can still open it
by hand. A servo's firmware position does not provide this property; see `SECURITY_ARCHITECTURE.md`
section 6 for the reasoning, the power-loss behaviour and the tests that cannot be run until the physical
mechanism exists. The SG90 is a bench stand-in for the actuator only.

## Firebase identity for the ESP32 (proposed, not deployed)

The ESP32 will use its own Firebase Auth email/password user, registered in a `deviceRegistry/$uid` node
that only the Firebase Console or Admin SDK can write. It never holds the administrator login, the
simulator's service account or any Admin SDK credential. Details, proposed rules and the revocation
procedure are in `SECURITY_ARCHITECTURE.md` and `database.rules.proposed.json`. The deployed
`database.rules.json` is unchanged, so the real ESP32 still cannot write to Firebase and the firmware
keeps the `StubCloudClient`.

## Uno memory budget (compiled, measured)

| Metric | Before | After |
| --- | --- | --- |
| Flash | 26,362 bytes (81%) | 18,574 bytes (57%) |
| Static SRAM | 1,508 bytes (73%) | 1,056 bytes (51%) |
| Free for stack/heap | 540 bytes | 992 bytes |

Changes: heap `String` objects and 16 `String` command IDs replaced by fixed `char` buffers and 16 FNV-1a
hashes; all text moved to flash with `F()`/`PROGMEM`; outgoing JSON is printed directly instead of through
`StaticJsonDocument`s; the incoming document shrank from 256 to 128 bytes and parses in place; the PIN salt
and hash live in flash and are compared in constant time; the unused `Serial.begin` was removed (it also
dropped the 157-byte `Serial` object). Stack depth was **not measured** (`-fstack-usage` produced empty
output with this toolchain); 992 bytes remain for stack and heap, and the SHA-256 context plus the JSON
document are the largest stack users. Check free RAM on real hardware.

Trade-offs: command-ID duplicate detection now compares 32-bit hashes (a false "duplicate" is possible
with probability about 16/2^32 per command; a duplicate is never accepted twice). Command IDs are
restricted to `[A-Za-z0-9_-]`, 32 characters at most; Firebase push keys comply.

## Milestone 4.5 - hardware mode in the Flutter app (prepared, NOT activated)

- `lib/services/gateway_policy.dart` holds `GatewayMode` and the pure `GatewayPolicy`. Mode comes only from the build flag `--dart-define=GATEWAY_MODE=hardware`; the default and any unknown value is `simulated`. The app never switches modes automatically.
- Remote Unlock requires the reported `devices/gateway/mode` to equal the configured mode, a fresh heartbeat (missing or older than 30 s shows OFFLINE), database connectivity, and a known locked door.
- A command is only shown as accepted/UNLOCKED when the device wrote `deviceId` plus `acceptedAt`/`acknowledgedAt`.
- Flutter still never writes lock state or acknowledgements. Identity of the physical device is enforced by the database rules (only a registered device may publish `mode: hardware`), not by the app. Under the live rules hardware mode is inert.
- Setup of the dedicated device identity: `DEVICE_IDENTITY_SETUP.md` (procedure only; nothing created).