// Resource-free parser, VM, platform and tooling regressions.
import {spawnSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {existsSync, mkdirSync, readFileSync, writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {python} from './lib/project.mjs';

const root = fileURLToPath(new URL('../', import.meta.url));
const out = resolve(root, 'build/check');
mkdirSync(out, {recursive: true});
const gates = [
  ['vm-wasm', 'tests/test-script-vm.mjs', '--mode=optimized', '--exceptions=wasm', '--tag=core'],
  ['vm-legacy-wasm', 'tests/test-script-vm.mjs', '--mode=legacy', '--exceptions=wasm', '--tag=core'],
  ['vm-js', 'tests/test-script-vm.mjs', '--mode=optimized', '--exceptions=js', '--tag=core'],
  ['vm-legacy-js', 'tests/test-script-vm.mjs', '--mode=legacy', '--exceptions=js', '--tag=core'],
  ['vm-old-code-layout', 'tests/test-script-vm.mjs', '--mode=optimized', '--legacy-code-layout', '--tag=core'],
  ['vm-asan', 'tests/test-script-vm.mjs', '--mode=optimized', '--asan', '--tag=core'],
  ['vm-simd-asan', 'tests/test-script-vm.mjs', '--mode=optimized', '--simd', '--asan', '--tag=core'],
  ['platform-wasm', 'tests/test-platform.mjs', '--exceptions=wasm'],
  ['platform-js', 'tests/test-platform.mjs', '--exceptions=js'],
  ['platform-asan', 'tests/test-platform.mjs', '--exceptions=wasm', '--asan'],
  ['containers-wasm', 'tests/test-script-containers.mjs', '--exceptions=wasm'],
  ['containers-js', 'tests/test-script-containers.mjs', '--exceptions=js'],
  ['render-node-pool', 'tests/test-render-node-pool.mjs'],
  ['documentation-links', 'tests/test-docs.mjs'],
  ['project-configuration', 'tests/test-project-config.mjs'],
  ['profile-recorder', 'tests/test-browser-profile-recorder.mjs'],
  ['campaign-summary', 'tests/test-campaign-summary.mjs'],
  ['gl-memory-observer', 'tests/test-observe-gl-memory.mjs'],
  ['capture-mask', 'tests/test-campaign-capture-mask.py'],
];
const git = spawnSync('git', ['rev-parse', 'HEAD'], {cwd: root, encoding: 'utf8'});
const listing = spawnSync('git', ['ls-files', '--cached', '--others', '--exclude-standard', '-z'],
  {cwd: root, encoding: 'utf8'});
if (listing.status !== 0 || listing.error) throw new Error(listing.error?.message || listing.stderr);
const inputs = [...new Set(listing.stdout.split('\0'))].sort().filter(path =>
  /^(?:source|portable|tests|tools)\//.test(path) && existsSync(resolve(root, path)));
const report = {
  scope: 'Resource-free parser/VM, containers, platform and observer regressions; not gameplay, native desktop, full campaign or phone acceptance',
  startedAt: new Date().toISOString(),
  commit: git.status === 0 ? git.stdout.trim() : null,
  inputs: Object.fromEntries(inputs.map(path => [path,
    createHash('sha256').update(readFileSync(resolve(root, path))).digest('hex')])),
  node: process.version, platform: process.platform,
  gates: [], vmOutputsIdentical: false, passed: false,
};
const save = () => writeFileSync(resolve(out, 'report.json'), JSON.stringify(report, null, 2) + '\n');
const vmHashes = [];
save();
for (const [name, ...args] of gates) {
  const begin = Date.now();
  const executable = args[0].endsWith('.py') ? python : process.execPath;
  const result = spawnSync(executable, args, {
    cwd: root, env: process.env, encoding: 'utf8', maxBuffer: 16 * 1024 * 1024,
    timeout: 240000, windowsHide: true,
  });
  writeFileSync(resolve(out, name + '.log'), (result.stdout || '') + (result.stderr || ''));
  const passed = result.status === 0 && !result.signal && !result.error;
  const entry = {name, command: [executable, ...args], passed, exitCode: result.status,
    signal: result.signal, error: result.error?.message || null, elapsedMs: Date.now() - begin};
  if (name.startsWith('vm-') && passed) {
    const text = (result.stdout || '').replaceAll('\r\n', '\n');
    entry.outputSha256 = createHash('sha256').update(text).digest('hex');
    vmHashes.push(entry.outputSha256);
  }
  report.gates.push(entry);
  console.log(`${passed ? 'PASS' : 'FAIL'} ${name} (${entry.elapsedMs} ms)`);
  save();
  if (!passed) {
    console.error((result.stderr || result.stdout || result.error?.message || '').slice(-6000));
    break;
  }
}
report.vmOutputsIdentical = vmHashes.length === 7 && new Set(vmHashes).size === 1;
report.changedDuringRun = inputs.filter(path => !existsSync(resolve(root, path)) ||
  createHash('sha256').update(readFileSync(resolve(root, path))).digest('hex') !== report.inputs[path]);
report.finishedAt = new Date().toISOString();
report.passed = report.gates.length === gates.length && report.gates.every(g => g.passed) &&
  report.vmOutputsIdentical && report.changedDuringRun.length === 0;
save();
console.log(`${report.passed ? 'PASS' : 'FAIL'} core regressions; VM outputs identical: ${report.vmOutputsIdentical}`);
console.log(`Evidence: ${resolve(out, 'report.json')}`);
process.exitCode = report.passed ? 0 : 1;
