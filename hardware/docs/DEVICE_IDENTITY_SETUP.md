# ESP32 Device Identity Setup (PROCEDURE ONLY — NOTHING HAS BEEN CREATED)

Status: **proposed**. No Firebase Auth user, `deviceRegistry` entry, or rules deployment
exists for the ESP32. Every step below is a manual action requiring explicit approval.
The proposed rules (`database.rules.proposed.json`) are **not deployed**.

## Principles

- The ESP32 gets its own Email/Password Auth user. It never uses the administrator
  account, the `device-simulator` identity, or any Admin SDK service-account key.
- Authorization is data, not code: a `deviceRegistry/<uid>` entry that only the Firebase
  Console / Admin SDK can write. Rules evaluate it on every request, so disabling it takes
  effect immediately.
- The device and the simulator share `devices/*` and `commands`. Run only one at a time.

## 1. Create the Auth user (Console, manual)

1. Firebase Console → project `smart-home-security-tip` → Authentication → Users → Add user.
2. Email: a non-routable address such as `esp32-frontdoor-1@device.invalid`.
3. Password: a long random value (32+ chars) from a password manager. Do not reuse any
   other password.
4. Copy the generated **UID**.

## 2. Create the registry entry (Console, manual)

Realtime Database → Data → add:

```json
"deviceRegistry": {
  "<ESP32_UID>": { "enabled": true, "deviceId": "esp32-frontdoor-1" }
}
```

The proposed rules let a device read only its own entry and nobody write it from a client.

## 3. What the device may do (proposed rules)

| Path | Device permission |
|---|---|
| `commands` | bounded read: `orderByChild('status').equalTo('pending').limitToFirst(≤10)` |
| `commands/<id>` | transitions `pending→accepted/failed/expired`, `accepted→completed/failed/expired`; `deviceId` must equal its registry `deviceId`; accepting requires an unexpired command |
| `devices/frontDoor/isLocked`, `devices/gateway/*` | write; `mode` may be `hardware` |
| `auditLogs` | create validated records; retrying the same event is allowed only when every stored field is identical |
| everything else | denied |

## 4. Configure credentials in firmware

Copy `hardware/esp32/smart_home_gateway/src/secrets.h.example` to the gitignored `secrets.h`. Put in it only: Wi-Fi SSID/password, the Firebase
**Web API key** (identifies the project, not a secret by itself), the device email and
password. Never commit `secrets.h`. Never log these values.

Warning: without ESP32 flash encryption, anyone holding the board can read the flash.
Treat the board as able to leak its device password; that is why the identity is
least-privilege and revocable.

## 5. Revocation

1. Immediate: set `deviceRegistry/<uid>/enabled` to `false` (Console). Rules deny the next request.
2. Disable or delete the Auth user (blocks new sign-ins; existing ID tokens expire within an hour, but step 1 already blocks them).
3. Rotate: create a new user/UID and registry entry for a replacement board.

Verified in emulator tests: revoked and unregistered devices are denied, and revocation applies mid-session.

## 6. Before deployment (all pending)

1. Re-run `node tools/rules_tests/compare_live_rules.js` and review the diff.
2. Run `npm test` in `tools/rules_tests` (17 rules tests + lifecycle tests).
3. Create the user and registry entry above.
4. Approve the hardware-mode app build (`--dart-define=GATEWAY_MODE=hardware`).
5. `firebase deploy --only database` — only after explicit approval. Keep a copy of the
   current rules for rollback (`database.rules.json` equals the live rules as of this review).

Physical electrical and UART behaviour remains **unverified** until hardware testing.
