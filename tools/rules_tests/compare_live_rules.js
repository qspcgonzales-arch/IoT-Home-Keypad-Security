// READ-ONLY comparison of the deployed Realtime Database rules with a repository rules file.
// It only runs `firebase database:get /.settings/rules`, which reads the rules and never writes.
// Usage: node compare_live_rules.js [rulesFile] [--live liveRulesFile]
//   rulesFile defaults to ../../database.rules.json
//   --live reads an already-saved copy instead of calling the Firebase CLI
const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

const PROJECT = process.env.FIREBASE_PROJECT || 'smart-home-security-tip';
const INSTANCE = process.env.FIREBASE_DB_INSTANCE || `${PROJECT}-default-rtdb`;

const args = process.argv.slice(2);
const liveIdx = args.indexOf('--live');
const livePath = liveIdx >= 0 ? args.splice(liveIdx, 2)[1] : null;
const repoPath = args[0] || path.join(__dirname, '..', '..', 'database.rules.json');

function loadLive() {
  if (livePath) return fs.readFileSync(livePath, 'utf8');
  const result = spawnSync(
    `npx firebase database:get /.settings/rules --project ${PROJECT} --instance ${INSTANCE}`,
    { encoding: 'utf8', shell: true, cwd: __dirname },
  );
  if (result.status !== 0) throw new Error(`Firebase CLI failed (is it logged in?): ${result.stderr || result.stdout}`);
  return result.stdout;
}

const parse = (text) => JSON.parse(text.replace(/^\uFEFF/, '').trim());

// Flatten to "path -> rule text" so differences are listed precisely.
function flatten(node, prefix = '', out = {}) {
  for (const [key, value] of Object.entries(node)) {
    const here = `${prefix}/${key}`;
    if (value && typeof value === 'object' && !Array.isArray(value)) flatten(value, here, out);
    else out[here] = JSON.stringify(value);
  }
  return out;
}

const live = flatten(parse(loadLive()));
const repo = flatten(parse(fs.readFileSync(repoPath, 'utf8')));
const keys = [...new Set([...Object.keys(live), ...Object.keys(repo)])].sort();
let diffs = 0;
for (const key of keys) {
  if (live[key] === repo[key]) continue;
  diffs += 1;
  const where = live[key] === undefined ? 'only in repository file' : repo[key] === undefined ? 'only in LIVE rules' : 'differs';
  console.log(`${where}: ${key}`);
}
console.log(diffs === 0 ? `IDENTICAL: live rules match ${path.basename(repoPath)}` : `${diffs} difference(s) between live rules and ${path.basename(repoPath)}`);
process.exitCode = diffs === 0 ? 0 : 1;
