# Development iterations

Each entry uses the same fields. "Actual results" are only what was run and observed.
Iterations 1–4 are reconstructed from the repository, so their per-iteration dates and
original test logs were not recorded; only the verification run on 2026-10-21 is real
evidence. Iterations 6+ are **PROPOSED**.

## Iteration 1 — Prototype UI and demo mode (DONE, reconstructed)
- **Requirements:** login, dashboard, audit list usable without hardware.
- **Implemented:** `login_screen`, `dashboard_screen`, `audit_logs_screen`, `settings_screen`, `MockDoorService`, `DemoAuthService`.
- **Tests:** `test/widget_test.dart` (demo sign-in/navigation, login validation, mock unlock + 4 s relock, offline failure).
- **Actual results:** 4/4 pass (2026-10-21).
- **User feedback:** Not yet collected.
- **Changes made:** n/a (no recorded feedback to act on).
- **Limitations:** mock only; no real devices.

## Iteration 2 — Firebase Auth and live status (DONE, reconstructed)
- **Requirements:** administrator-only access, live door/gateway state.
- **Implemented:** `FirebaseAuthService`, `FirebaseRealtimeDatabaseService` (per-leaf listeners, heartbeat freshness 30 s), default-deny rules.
- **Tests:** manual run on the Android Emulator. The app built, installed and launched on emulator-5554 (Android 17, API 37) on 2026-10-21.
- **Actual results:** launched without runtime errors in the log. No automated test covers the live service.
- **User feedback:** Not yet collected.
- **Limitations:** no automated tests with a fake database; rules not run through the Firebase emulator.

## Iteration 3 — Remote unlock workflow and simulator (DONE, reconstructed)
- **Requirements:** admin creates a validated command; a trusted device processes it; result is audited.
- **Implemented:** `commands/$commandId` rules, `UnlockCommand`, `tools/device_simulator/device_simulator.js`.
- **Tests:** `tools/device_simulator/test/device_simulator.test.js` plus the virtual-hardware tests.
- **Actual results:** `npm test`: 25/25 pass (2026-10-21).
- **User feedback:** Not yet collected.
- **Limitations:** the simulator uses a trusted service account (`device-simulator`), not a per-device identity.

## Iteration 4 — Virtual Arduino/ESP32 (DONE, reconstructed)
- **Requirements:** exercise keypad PIN, lockout, IR, UART and Wi-Fi outage logic without hardware.
- **Implemented:** `hardware/virtual/arduino_simulator.js`, `esp32_gateway_simulator.js`, `uart_protocol.md`, firmware sources.
- **Tests:** `virtual_hardware.test.js` (PIN, three-attempt 30 s lockout, duplicate/expired commands, Wi-Fi outage, write failure).
- **Actual results:** included in the 25/25 above.
- **User feedback:** Not yet collected.
- **Limitations:** firmware not compiled; timing, electrical levels, servo power and UART framing unverified on hardware.

## Iteration 5 — Hardening and docs (DONE, this change)
- **Requirements:** RAD documentation, cost baseline, review of reads and error handling.
- **Implemented:** the five documents in `docs/`; code review (findings in `LOW_COST_DEVELOPMENT.md`).
- **Tests:** analyzer, Flutter tests, Node tests rerun.
- **Actual results:** `flutter analyze` no issues; Flutter 4/4; Node 25/25.
- **Changes made:** no production source changes (see rationale in `LOW_COST_DEVELOPMENT.md`).
- **Limitations:** no user feedback session held yet.

## Iteration 6 — Emulator user walkthrough (PROPOSED)
- **Requirements:** two or more users complete login → unlock → audit → settings on the emulator.
- **Tests:** scripted tasks in `TESTING_AND_FEEDBACK.md`.
- **Output:** real feedback notes, then a small UI fix list.

## Iteration 7 — Test coverage for the live service (PROPOSED)
- Add a fake `FirebaseDatabase` (or the Firebase Emulator Suite, free) and test the command state machine and timeouts; run rules tests against the emulator.

## Iteration 8 — ESP32 firmware completion and compile check (PROPOSED)
- Compile both firmware targets (Arduino CLI / PlatformIO are free). Implement Wi-Fi, time sync, device identity and Firebase client. Needs explicit approval before any hardware spend.

## Iteration 9 — Cutover (PROPOSED)
- Publish rules, build the release APK, hardware bring-up, final acceptance run.
