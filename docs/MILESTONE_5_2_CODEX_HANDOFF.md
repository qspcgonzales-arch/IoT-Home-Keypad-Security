# Milestone 5.2 evaluation and Codex handoff

**Status date:** 2026-10-24
**Project:** IoT Home Keypad Security
**Firebase project:** `smart-home-security-tip`

## Executive summary

Milestone 5.2's controlled Firebase Authentication and read-only Realtime Database checks passed. A later manual test, reported by the project owner, also confirmed that an existing authenticated non-admin account is denied access to the Dashboard by the app's UID check. This closes the milestone's app-level authorization check.

This is **not** approval for production deployment, live unlock testing, or physical door control. Live rules parity, remote command behavior, and physical hardware safety remain unverified. Continue development in an isolated emulator or staging setup.

## Results

| Area | Result | Evidence / caveat |
| --- | --- | --- |
| Firebase project configuration and initialization | PASS | Flutter and Android Firebase configuration target `smart-home-security-tip`; live administrator sign-in succeeded |
| Email/Password sign-in provider | PASS | Confirmed enabled in Firebase Console |
| Approved administrator authentication | PASS | Administrator signed in on the Android emulator; Console UID matched the app's configured allowlist |
| Authenticated non-admin authorization | PASS, based on project-owner report | Existing account reportedly showed “This account is not authorized as an administrator.” and remained on Login; credentials were not recorded |
| Read-only device-state access | PASS | Dashboard read `devices/frontDoor/isLocked` as `LOCKED` |
| Session persistence and logout | PASS | Session remained after app-process restart; logout returned to Login |
| Invalid credentials and offline handling | PASS | Generic credential error and expected network error were shown; protected Dashboard did not open |
| Flutter static analysis and tests | PASS | `flutter analyze` clean; `flutter test` 8/8 |
| Local Realtime Database rules tests | PASS | 19/19 emulator tests; this does not establish that deployed rules match local rules |
| Local device simulator tests | PASS | 36/36, including simulated PIN lockout scenarios |
| Live unlock command | NOT TESTED | No remote command was issued |
| Deployed rules compared with local rules | NOT VERIFIED | Live rules were not fetched or deployed |
| Physical hardware acceptance | NOT TESTED | No keypad, servo, ESP32, or physical lock was tested |

## Important distinction

The first attempted non-admin login returned a generic “account unknown” error. That attempt did not reach the app's UID authorization check. In the later test, the project owner reported seeing the explicit administrator-denial message and remaining on Login. The latter is recorded as a successful manual app-level authorization check; no passwords or account identifiers are included here.

## Current readiness

- Suitable for a local software demonstration and internal review.
- Not ready for production distribution or physical door control.
- The release APK uses debug signing.
- The Flutter app is not wired to a local Firebase Auth/Database Emulator or separate staging project.
- Live production rules have not been compared with the checked-in rules.
- The ESP32 Firebase client remains a stub; no physical hardware behavior is established.

## Recommended next work for Codex

1. Keep live unlocks and production rule changes out of scope until an isolated staging or emulator configuration is available and reviewed.
2. Wire the Flutter app to an approved Auth and Realtime Database Emulator or staging project using an explicit development configuration; ensure normal builds remain pointed at the intended project.
3. Add or run tests covering authenticated non-admin rejection, administrator-only reads/writes, command shape and expiry, device acknowledgement, and audit behavior in the isolated environment.
4. Inspect deployed rules read-only with the project owner's authorization and compare them with `database.rules.json`. Do not deploy rules without a separate reviewed change and explicit approval.
5. Validate command handling with the simulator or an inert, non-actuating bench setup before considering any physical actuator.
6. Treat physical door testing, production signing, credential provisioning, and any live unlock test as separate, explicitly approved release activities.

## Safety constraints for follow-up work

- Do not create Firebase accounts just to reproduce the completed non-admin test.
- Do not record or request passwords, tokens, or private account identifiers.
- Do not issue a live unlock command or connect the prototype to a real door lock as part of emulator/staging work.
- Do not deploy Firebase rules or modify production accounts without explicit project-owner approval.
- Prefer the project's designated `Medium_Phone` emulator for app testing.

## Source records

- `docs/FINAL_RELEASE_CHECKLIST.md`
- `docs/TESTING_AND_FEEDBACK.md`
- `docs/KNOWN_LIMITATIONS.md`
