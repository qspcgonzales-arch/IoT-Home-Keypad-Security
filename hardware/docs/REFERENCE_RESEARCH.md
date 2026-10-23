# Reference research for the Uno R3 + ESP32 integration

Retrieved 2026-10-21 over HTTPS (page fetches plus raw files from `raw.githubusercontent.com`).
Labels used throughout:

- **[Verified]**: read directly in the cited file or page.
- **[Not inspected]**: the file or page content was not accessible or not read. Nothing is claimed about it.
- **[Assumption]**: general engineering knowledge or our own proposal, not from the source.

Nothing here has been tested on hardware. All electrical and physical statements for our project are
**unverified until hardware testing**. None of the sources is copied into this repository; we only
use them as design references. Only source 4 shows a license (MIT); the others show none, so their code must not be copied.

## 1. The eight sources at a glance

| # | Source (original URL) | What was inspected | Platform and lock | Cloud / app link |
| --- | --- | --- | --- | --- |
| 1 | https://github.com/Oshadadamutha/smartdoor | `README.md`, `keypadHandler.cpp`, `FirebaseHandler.cpp`, `smartdoor_offline.ino` (file list via GitHub API). `fingerprint.cpp`, `LCDDisplay.cpp` and the headers were not read | ESP32, keypad, I2C LCD, DY50 fingerprint, 12 V solenoid via a driver module | Flutter + Firebase RTDB, email/password auth |
| 2 | https://github.com/tmthyadms/locknroll | `README.md`, `lib/views/device_control_view.dart`. Other `lib/` files were not read. **The repository tree has no `.ino` or firmware files** | ESP32-CAM + ESP8266 NodeMCU (doorbell, alarm, lock) per the README; no firmware to inspect | Flutter + Auth, Firestore, RTDB, Storage |
| 3 | https://github.com/Arjuns20004/esp32-smart-door-lock | `README.md`, `Door_Lock.ino` | ESP32, keypad, servo | Blynk IoT (virtual pins V0/V1) |
| 4 | https://github.com/SohamAgg/Smart-Home-Project | `README.md`, parts of `esp32/smart_home/smart_home.ino`. Flutter files were not read | ESP32, SG90 servo as door lock, plus unrelated appliances | Flutter + Firebase RTDB, polled by the ESP32 |
| 5 | https://omartronics.com/arduino-password-door-lock/ | Page text through the servo and wiring sections. The sketch section was cut off by the 8000-character fetch limit, so it was **not read** | Uno, keypad, I2C LCD, SG90 | None |
| 6 | https://projecthub.arduino.cc/ballieducation/arduino-keypad-password-lock-system-with-servo-lcd-indicator-alerts-6ffa89 | **Only the page title and meta description.** The page body (code, wiring) did not render or was not accessible | Arduino keypad lock with servo and LCD (title only) | None known |
| 7 | https://github.com/notedwinpiatek/arduino-uno-r3-door-lock | `README.md`, `DoorLockSystem.c` | Uno R3, keypad, I2C LCD, servo, LED | None |
| 8 | https://github.com/Hotsunlok/ESP32-smart-door-system | `README.md`, `ESP32_Pin_Mappings.md` (file list via GitHub API). `Full_Arduino_Code.md`, `Servo Motor Control.md` and the other `.md` files were not read | ESP32, servo, keypad, RFID, fingerprint, LCD, buzzer | Local ESP32 web server (AP mode, WebSocket); no Firebase |

## 2. Source-supported findings (with citations)

### Keypad, LCD and servo wiring on a Uno
- 4×4 keypad on D2–D9, SG90 signal on D10, I2C LCD on A4/A5 (SDA/SCL), LCD I2C address usually `0x27` or `0x3F`. [Verified] — source 5 page, "Circuit" and "Keypad/LCD/Servo connections" tables.
- Alternative Uno map: keypad rows `{11,10,9,8}`, columns `{7,6,5,4}`, servo `12`, LED `13`, LCD `0x27`. [Verified] — source 7, `DoorLockSystem.c`.
- Libraries: Keypad (Stanley/Brevig) and LiquidCrystal I2C (de Brabander). [Verified] — source 5 page, sketch section intro.

### Servo power (the most important electrical finding)
- SG90 draws about 100 mA free-moving and up to about 650 mA at stall. USB gives 500 mA, so the Uno can reset. Use an external 5 V supply rated at 1 A or more, join its GND to the Uno GND, and do **not** connect the supply + to the Uno 5V pin. [Verified] — source 5 page, "Servo connections" and the following paragraph.
- Servo 5 V taken from the ESP32 5V/VIN pin in other projects. [Verified] — source 4 `README.md` (wiring list), source 8 `ESP32_Pin_Mappings.md` (Servo table). We will not copy this for the Uno, because it contradicts source 5's finding.

### Lock behaviour
- Locked 0°, open 90°; `*` relocks manually; wrong password message for 1.5 s. [Verified] — source 5 page, "How it works".
- Servo 0° locked, 90° open; relocks after 5 s with a **blocking** `delay(5000)`. [Verified] — source 3 `Door_Lock.ino` `unlockDoor()`.
- Source 7 uses `write(180)` then `write(-180)`; `-180` is not a valid servo angle. [Verified] — `DoorLockSystem.c`, `loop()`. Treat it as a defect, not a pattern.
- Source 4 limits the angle to 0–90 with `constrain`, attaches the servo with 544–2400 µs pulse bounds, and steps one degree at a time with `delay(30)`. [Verified] — `smart_home.ino`, `applyServo()`.
- Source 8: successful unlock beeps once, a failed attempt beeps twice, 10-second auto-lock countdown. [Verified] — `README.md`.
- Source 1: lock hardware is a 12 V solenoid behind a driver module. [Verified] — `README.md`, "Hardware Components".

### Security practices observed (all weak, for contrast)
- Plaintext PIN in the sketch: `"1234"` (source 3 `Door_Lock.ino`), `"C0D3"` (source 7 `DoorLockSystem.c`), and source 5 states its password is a plain `String` and "do not use it as the only lock on a real door". [Verified]
- Source 1 `FirebaseHandler.cpp` contains hard-coded Wi-Fi and Firebase credentials. [Verified] The values are deliberately not reproduced here. Those credentials should be treated as exposed.
- Source 4 authenticates with a Firebase "database secret" (`FIREBASE_AUTH`). [Verified] — `README.md` and `smart_home.ino` defines. This is a legacy full-access credential.
- Source 1 `keypadHandler.cpp` `Keycheck()` loops with `while (true)`, which blocks everything else. [Verified]

### App and cloud communication
- Source 1: ESP32 signs in with Firebase email/password and uses the RTDB client (`Firebase.RTDB.updateNode`, `getJSON`) on `/lock_control/main_door` with a `lastChangedBy` field. [Verified] — `FirebaseHandler.cpp`.
- Source 2: the app writes `smartDoorLock/isDoorLocked`, `isDoorbellPressed`, `isAlarmActivated` and `isPhotoTaken` directly. [Verified] — `lib/views/device_control_view.dart`. The client writes lock state itself, with no command, acknowledgement or expiry, which is the opposite of our design.
- Source 3: the Blynk button on V0 triggers `unlockDoor()`; status text goes to V1. [Verified] — `Door_Lock.ino`.
- Source 4: the ESP32 polls RTDB every 2 s (`FIREBASE_POLL = 2000`) with per-path `getBool` calls (e.g. `/devices/led`). The whole-node read in `pollFirebase()` is commented out. [Verified] — `smart_home.ino`. Polling adds database reads; our design uses listeners and bounded queries.
- Source 8: no cloud. The ESP32 hosts its own Wi-Fi network (`192.168.4.1`) with logs and a lock toggle. Failed attempts are not logged. [Verified] — `README.md`.

### ESP32 pin maps seen (for conflict analysis)
| Source | Keypad rows / cols | Servo | LCD | Other |
| --- | --- | --- | --- | --- |
| 1 (`keypadHandler.cpp`, `smartdoor_offline.ino`) | rows 13,12,14,27 / cols 26,33,32,4 | none (solenoid) | `Wire.begin(21,22)` | relay 15, button 2 |
| 3 (`Door_Lock.ino`) | rows 19,18,5,17 / cols 16,4,0,2 | 13 | none | |
| 4 (`README.md`) | none | 13 | none | buzzer 32, button 15, relay 23 |
| 8 (`ESP32_Pin_Mappings.md`) | rows 14,27,26,25 / cols 33,32,18,19 | 13 | SDA 21, SCL 22, VCC 5 V | RC522 4,15,2,35,34; buzzer 23; fingerprint 16/17 |

## 3. What we did not verify
Source 6 content; source 5 sketch text; source 1 fingerprint/LCD code; source 2 firmware (none exists in the repository); source 8 detailed `.md` files; all circuit images (not viewed); library versions; whether any of these projects works on real hardware.

## 4. Source findings vs. our project (what must change)

| Topic | Reference practice | Our requirement | Modification |
| --- | --- | --- | --- |
| PIN storage | Plaintext string (3, 5, 7) | Salted hash, never logged | Keep our salted SHA-256 compare in `smart_home_lock.ino` |
| Remote command | Direct state write (2), button (3), polling bool (4) | Command record with `type`, `status`, `requestedBy`, `createdAt`, `expiresAt`, administrator-only create, device-only status updates | Keep `database.rules.json` contract unchanged |
| Cloud credential | Hard-coded secrets / DB secret (1, 4) | No secrets in source, no database secret | Provision per device outside git (see `ARCHITECTURE.md`) |
| Timing | Blocking `delay()` (3, 7) | Non-blocking relock, lockout | Keep `millis()` timers |
| Lockout | None found | 3 attempts → 30 s | Already implemented in `.ino` and virtual Uno |
| Servo supply | Board 5 V/VIN (4, 8); external supply (5) | Separate supply, common ground | Follow source 5 |
| Reads | 2-second polling (4) | Bounded reads | ESP32 should listen to `commands` with a child-added style query, not poll |
| ESP32↔Uno | Not in any source (all are single-MCU) | UART JSON protocol | Our own design; see `uart_protocol.md` |
| Servo vs. lock | Servo as latch (3, 5, 7); solenoid (1) | Servo is a **demonstration actuator** | A real door strike or solenoid needs its own driver, flyback diode and supply; not designed here |
| Feedback | One beep success, two failure (8) | Existing LCD + buzzer + audit events | Keep ours; the beep patterns are optional |

## 5. Key conclusion
No source implements a two-board design (Uno front-end + ESP32 gateway) or our command/ack contract. The sources support only individual building blocks (keypad, LCD, servo wiring, servo power, Firebase RTDB from an ESP32). The UART protocol and the pin map in `ARCHITECTURE.md` are **our proposals**, not source-derived.
