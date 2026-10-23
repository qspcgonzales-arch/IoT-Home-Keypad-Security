# Local Firebase device simulator

This process simulates a trusted device component. It is separate from Flutter and labels its Firebase gateway record as `simulated`. It accepts only a new, unexpired `unlock` request from the configured administrator, acknowledges it, opens the simulated door for four seconds, records unlock/relock audit events, and sends a heartbeat every five seconds.

## Configure credentials outside the Flutter project

1. In Firebase Console, create a dedicated service account for the simulator and download its JSON key.
2. Store the key somewhere outside this repository, for example `C:\Users\<you>\Documents\FirebaseSecrets\home-simulator.json`. Never copy it into the Flutter project or commit it.
3. The simulator configures the Admin SDK with a Realtime Database auth override (`uid=device-simulator`). The database rules still apply and limit the simulator to command acknowledgements, door/gateway status, heartbeat, and audit writes. Firebase documents this override as the way to downscope Admin SDK database access.

## Run on Windows PowerShell

From the `tools/device_simulator` folder:

```powershell
npm install
$env:GOOGLE_APPLICATION_CREDENTIALS = 'C:\Users\<you>\Documents\FirebaseSecrets\home-simulator.json'
$env:FIREBASE_DATABASE_URL = 'https://smart-home-security-tip-default-rtdb.asia-southeast1.firebasedatabase.app'
npm start
```

The administrator UID is set in the simulator and matches the Flutter administrator allowlist and `database.rules.json`. Keep the process running during the demonstration. Press Ctrl+C to stop it; it marks the gateway offline and waits for an active four-second unlock/relock cycle to finish.

Run simulator unit tests without Firebase credentials or network access:

```powershell
npm test
```

The simulator uses Firebase Admin SDK credentials only at runtime. The test suite uses an in-memory fake database and never needs a real service-account key.
