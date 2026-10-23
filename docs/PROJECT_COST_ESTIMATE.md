# Project cost estimate

All figures are **estimates built on stated assumptions**, not measured costs. No prices
for components are given because none were supplied or verified; fill the blank cells with
local quotes. Nothing here has been purchased or approved.

## Software and services

| Item | Choice | Cash cost |
| --- | --- | --- |
| Framework, IDE, SDKs | Flutter, VS Code, Android Studio, Node | 0 |
| Auth and database | Firebase Spark (no-cost) plan | 0 while within quota (verify) |
| Simulator / tests | Node `node --test`, Flutter test | 0 |
| Android testing | Android Emulator | 0 |
| Firmware tooling | PlatformIO / Arduino CLI | 0 |

Expected load (assumption): one administrator, one gateway heartbeat every 5 s, a few
dozen commands and audit events per day. This is expected to be far below the Spark
storage and download quotas, but it was not measured. Check the Firebase console usage
page after a longer run.

## Hardware (pending, needs approval)

| Item | Qty | Unit price | Note |
| --- | --- | --- | --- |
| Arduino Uno R3 | 1 | _fill_ | |
| ESP32 dev board | 1 | _fill_ | |
| 4x4 keypad | 1 | _fill_ | |
| I2C 16x2 LCD | 1 | _fill_ | |
| SG90 servo | 1 | _fill_ | needs a separate 5 V supply |
| IR proximity module | 1 | _fill_ | |
| Active buzzer | 1 | _fill_ | |
| 5 V to 3.3 V level shifter | 1 | _fill_ | required for Uno TX to ESP32 RX |
| Power supply, wiring | 1 | _fill_ | |

## Expected development-cost savings (assumptions)

| Saving | Assumption | Estimated effect |
| --- | --- | --- |
| Virtual Uno/ESP32 instead of waiting on hardware | The 25 Node tests (PIN, lockout, UART, Wi-Fi outage) find logic faults before wiring | Avoids blocked days; value not measured |
| Emulator instead of physical phone and device fleet | One emulator available | No device cost |
| Reusing the `DoorService` interface | Demo and live share the UI | Avoids duplicate screens |
| Spark plan | Within free quota | 0 recurring cash cost |

No hour or currency savings figure is claimed, because no time-tracking data exists in the repository.
To get a defensible number, record hours per iteration in `DEVELOPMENT_ITERATIONS.md`.

## Risks to cost

- Exceeding the Spark quota (unlikely at the assumed load).
- Hardware faults from skipping level shifting or a separate servo supply (could damage boards).
- The recurring build-folder file-lock problem observed on Windows (see `TESTING_AND_FEEDBACK.md`) wastes time but not cash.
