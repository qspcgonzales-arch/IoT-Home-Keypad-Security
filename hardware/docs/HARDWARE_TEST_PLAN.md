# Hardware test plan

> **Architecture update:** The sequences below describe the earlier Uno + ESP32
> UART path and are historical, not acceptance steps for the new ESP32-only
> design. For Milestone 6.6, use [ESP32_ONLY_ACCEPTANCE.md](./ESP32_ONLY_ACCEPTANCE.md)
> and its GPIO23 wiring recommendation. Do not mix the old UART pinout with
> the ESP32-only firmware. No physical test should be marked passed without
> direct evidence.

No physical Arduino Uno R3, ESP32, servo, level shifter or lock mechanism was available during
Milestone 6. Every physical item below is **pending**; see `INTEGRATION_TEST_RESULTS.md` for the
separate source/build/emulator results. Record physical results only after observing the real hardware.
Stop immediately on heat, smell, smoke or unexpected motion. Never use a mains-powered or life-safety door
for these tests.

## Phase 0: bench safety (before any power)

| ID | Test | Pass criterion |
| --- | --- | --- |
| 0.1 | Continuity and polarity check against `provisional_wiring.md` | No shorts; common ground present |
| 0.2 | Verify direction-fixed, push-pull UART level conversion both ways, UART disconnected; measure Uno-to-ESP32 output never exceeds the exact ESP32 GPIO limit and ESP32-to-Uno output meets the exact Uno input-high requirement | No direct Uno 5 V signal reaches ESP32; both levels meet board datasheets |
| 0.3 | Servo supply separate, regulated 5 V, sized to the exact servo's stall current; join signal grounds but return servo current to its supply | Uno 5V pin does not feed the servo; correct polarity and no supply short |

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
| 2.1 | Send valid UNLOCK lines at 9600 baud through a UART-compatible push-pull translator | `ack` then `STATE unlocked` every time over 200 trials |
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

## Phase 4: ESP32 integration (Firebase REST client implemented; board tests pending)

| ID | Test | Pass criterion |
| --- | --- | --- |
| 4.1 | Sign-in, token refresh after about 55 min, Wi-Fi drop and reconnect | Recovers without reboot and without leaking credentials in serial output |
| 4.2 | TLS validation: wrong clock, tampered certificate (test proxy) | Connection refused |
| 4.3 | Expired command, duplicate command, command during reconnect | Not executed; correct Firebase status |
| 4.4 | Uno ack timeout (unplug UART) | Command ends `failed`; door unchanged |
| 4.5 | Revoke device in the registry mid-run | Writes denied immediately; no unlock |
| 4.6 | Lock state shown in the app matches the real door | Match in 20 of 20 trials, including keypad unlocks |

## Milestone 6 step-by-step sequence

Run the stages in order. Keep the servo horn disconnected from any latch through test 8.
Tests 1-3 and 5-10 require physical hardware; test 4 is covered by the isolated Auth/RTDB
emulator test and must be repeated with the actual device only after a dedicated test identity
is created. No production identity, rules deployment, or door actuation is authorized here.

| # | Test | Procedure summary | Pass evidence | Current result |
| --- | --- | --- | --- | --- |
| 1 | Arduino-only components | Power Uno alone; inspect LCD/keypad/buzzer/TCRT input, test local PIN and lockout with servo disconnected | Components respond to expected inputs; no exposed PIN in UART/logs | **Pending physical assembly** |
| 2 | ESP32 Wi-Fi | Boot gateway with isolated test Wi-Fi and emulator endpoint; verify reconnect after AP interruption | Serial reports Wi-Fi recovery; no command is processed without synchronized time | **Pending physical assembly** |
| 3 | Arduino–ESP32 UART | Connect level-shifted UART, send periodic `PING`, verify matching `PONG` and door state; test framing/error conditions | Fresh PONGs at 9600 8N1; status becomes unavailable on unplug | **Pending physical assembly** |
| 4 | ESP32 authentication in isolated Firebase | Run `npm run test:device-client` in `tools/rules_tests`; use emulator-only Auth and RTDB | Dedicated Auth token, registered UID, valid scoped writes, token refresh and revocation denial | **Automated emulator contract test; hardware execution pending** |
| 5 | Remote unlock reception | Seed one valid pending command in emulator with servo disconnected; monitor UART | One `UNLOCK`, correct ID/remaining TTL/duration, no repeat | **Pending physical assembly** |
| 6 | Arduino acknowledgement | Capture accepted ACK, remote-unlocked STATE, executed ACK; inject bad/expired/duplicate IDs | Firebase only reaches completed after valid sequence; invalid commands do not actuate | **Pending physical assembly** |
| 7 | Servo movement without latch | Attach only bench servo to separate supply; verify angle while monitoring current | Correct motion; no brownout, overheating, or unexpected motion | **Pending physical assembly** |
| 8 | Four-second automatic relock | Use default 4-second setting with no latch connected; time from servo command to relock | Uno reports locked after approximately four seconds; record timing | **Pending physical assembly** |
| 9 | Door-state and audit synchronization | Compare Uno logical STATE and Firebase leaf/audit records for remote and keypad paths | Correct locked/unlocked values and one matching audit per event | **Pending physical assembly** |
| 10 | Wi-Fi failure and recovery | Interrupt/recover Wi-Fi during idle and a pending command; never disconnect during unsafe servo state | No stale replay; local keypad remains local; cloud state/audits retry after reconnect | **Pending physical assembly** |

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