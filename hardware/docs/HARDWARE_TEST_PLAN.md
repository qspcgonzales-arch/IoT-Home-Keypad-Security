# Hardware test plan (all items NOT YET RUN)

No Arduino Uno R3, ESP32, servo, level shifter or lock mechanism is available. Every item below is
**unverified** and has no recorded result. Record actual results in `docs/TESTING_AND_FEEDBACK.md` when run.
Stop immediately on heat, smell, smoke or unexpected motion. Never use a mains-powered or life-safety door
for these tests.

## Phase 0: bench safety (before any power)

| ID | Test | Pass criterion |
| --- | --- | --- |
| 0.1 | Continuity and polarity check against `provisional_wiring.md` | No shorts; common ground present |
| 0.2 | Level shifter orientation: measure Uno TX side (5 V) and ESP32 RX side (<= 3.3 V) with a multimeter, UART unconnected | ESP32 RX pin never above 3.3 V |
| 0.3 | Servo supply separate, 5 V, at least 1 A, ground joined | Uno 5V pin not feeding the servo |

## Phase 1: Uno alone (USB, no ESP32)

| ID | Test | Pass criterion |
| --- | --- | --- |
| 1.1 | Provision real salted hash and salt, flash, boot | LCD "READY"; `STATE locked` |
| 1.2 | Correct PIN, wrong PIN x3 | Unlock then relock after duration; denied beeps; 30 s lockout |
| 1.3 | Free RAM readout (temporary sketch counter) | At least 500 bytes free at idle and during PIN hashing |
| 1.4 | Time the PIN hash with `micros()` | Record value; if over 60 ms, re-assess risk 5 |
| 1.5 | Confirm buzzer is active type and silent when idle | On/off beeps only |

## Phase 2: UART bench (Uno + ESP32 or USB-UART adapter, servo unplugged)

| ID | Test | Pass criterion |
| --- | --- | --- |
| 2.1 | Send valid UNLOCK lines at 9600 baud | `ack` then `STATE unlocked` every time over 200 trials |
| 2.2 | Duplicate, expired TTL, malformed, over-long, bad-ID lines | Correct `nack` each time; no unlock |
| 2.3 | Worst-case 129-byte line sent back-to-back | No lost commands; overflow handling visible as `nack` only |
| 2.4 | Noise: power-cycle the ESP32 repeatedly while the Uno runs | No unlock from boot noise |

## Phase 3: interaction (servo attached on its own supply)

| ID | Test | Pass criterion |
| --- | --- | --- |
| 3.1 | Send UNLOCK while the servo is moving and while the buzzer sounds | Byte loss rate recorded; target zero over 200 trials |
| 3.2 | Servo idle jitter and holding position | Record; decide on servo detach (risk 6) |
| 3.3 | Servo stall current at the mechanism limit | Within supply rating; no Uno resets |
| 3.4 | Keypad press while receiving a command | Both handled |

## Phase 4: ESP32 integration (after Firebase client exists and rules are approved)

| ID | Test | Pass criterion |
| --- | --- | --- |
| 4.1 | Sign-in, token refresh after about 55 min, Wi-Fi drop and reconnect | Recovers without reboot and without leaking credentials in serial output |
| 4.2 | TLS validation: wrong clock, tampered certificate (test proxy) | Connection refused |
| 4.3 | Expired command, duplicate command, command during reconnect | Not executed; correct Firebase status |
| 4.4 | Uno ack timeout (unplug UART) | Command ends `failed`; door unchanged |
| 4.5 | Revoke device in the registry mid-run | Writes denied immediately; no unlock |
| 4.6 | Lock state shown in the app matches the real door | Match in 20 of 20 trials, including keypad unlocks |

## Phase 5: mechanical fail-secure (needs the physical mechanism)

| ID | Test | Pass criterion |
| --- | --- | --- |
| 5.1 | Remove all power while locked | Stays locked |
| 5.2 | Remove all power while unlocked | Returns to locked (spring) within the design time |
| 5.3 | Back-drive or force the bolt with power off | Holds to the target force |
| 5.4 | Manual release from inside with power off, and under door load | Opens without tools, with a bounded force |
| 5.5 | Servo supply drops while the Uno stays powered, and the reverse | Lock remains secure; firmware reports a consistent state |
| 5.6 | Repeat 5.1 to 5.5 after 100 cycles | No degradation |

## Milestone 4.5 additions (all PENDING, require hardware or approval)

- Verify the 9600-baud A2/A3 <-> GPIO16/17 link through the level shifter, including a malformed/oversized line and the new `malformed-command` vs `expired` reasons.
- Run the app with `--dart-define=GATEWAY_MODE=hardware` against a registered ESP32 only after the rules and identity are approved; confirm Remote Unlock stays disabled with the device unplugged and the badge shows OFFLINE after the heartbeat goes stale (30 s).
- Confirm the UI never shows UNLOCKED until the device writes `completed` with `deviceId` and `acknowledgedAt`.
- Revocation test: set `deviceRegistry/<uid>/enabled=false` while connected and confirm the next device write is denied.