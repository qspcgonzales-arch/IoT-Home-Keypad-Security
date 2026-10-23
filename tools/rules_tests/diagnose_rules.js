// Diagnostic (not part of the pass/fail suite): isolates WHY the repository production rules reject
// the three operations. It applies single, minimal edits to a copy of database.rules.json in memory
// and reports which operations then succeed. Run:  npm run diagnose
const fs = require('node:fs');
const path = require('node:path');
const { initializeTestEnvironment } = require('@firebase/rules-unit-testing');
const { ref, set, update, serverTimestamp } = require('firebase/database');

const ADMIN = 'ERxtj2dQuQRvv3Duw8Ud4DNMErD3';
const SIM = 'device-simulator';
const base = () => JSON.parse(fs.readFileSync(path.join(__dirname, '..', '..', 'database.rules.json'), 'utf8'));

const FIELD_VALIDATORS = {
  type: "newData.val() === 'unlock'",
  status: "newData.isString()",
  requestedBy: 'newData.isString()',
  createdAt: 'newData.isNumber()',
  expiresAt: 'newData.isNumber()',
};
const withValidators = (names) => (rules) => {
  const node = rules.rules.commands['$commandId'];
  for (const n of names) node[n] = { '.validate': FIELD_VALIDATORS[n] };
  return rules;
};

const VARIANTS = [
  ['A. production file, unmodified', (r) => r],
  ['B. delete the $other: false rule only', (r) => { delete r.rules.commands['$commandId']['$other']; return r; }],
  ['C. add validators for status only', withValidators(['status'])],
  ['D. add validators for type, requestedBy, createdAt, expiresAt (not status)', withValidators(['type', 'requestedBy', 'createdAt', 'expiresAt'])],
  ['E. add validators for all five missing fields', withValidators(Object.keys(FIELD_VALIDATORS))],
];

async function run(rules) {
  const env = await initializeTestEnvironment({ projectId: 'demo-smart-home', database: { rules: JSON.stringify(rules) } });
  const now = Date.now();
  const result = {};
  const attempt = async (name, fn) => { try { await fn(); result[name] = 'allowed'; } catch (e) { result[name] = 'DENIED'; } };
  await env.clearDatabase();
  await attempt('admin create pending command', () => set(ref(env.authenticatedContext(ADMIN).database(), 'commands/new'), {
    type: 'unlock', status: 'pending', requestedBy: ADMIN, createdAt: serverTimestamp(), expiresAt: now + 30000,
  }));
  await env.withSecurityRulesDisabled(async (c) => set(ref(c.database(), 'commands/c1'), {
    type: 'unlock', status: 'pending', requestedBy: ADMIN, createdAt: now - 1000, expiresAt: now + 30000,
  }));
  const s = env.authenticatedContext(SIM).database();
  await attempt('simulator pending -> accepted', () => update(ref(s, 'commands/c1'), { status: 'accepted', acceptedAt: serverTimestamp(), deviceId: 'local-device-simulator' }));
  await attempt('simulator accepted -> completed', () => update(ref(s, 'commands/c1'), { status: 'completed', acknowledgedAt: serverTimestamp(), message: 'ok' }));
  await env.cleanup();
  return result;
}

(async () => {
  for (const [label, edit] of VARIANTS) {
    const r = await run(edit(base()));
    console.log(label);
    for (const [k, v] of Object.entries(r)) console.log(`    ${k.padEnd(34)} ${v}`);
  }
})();
