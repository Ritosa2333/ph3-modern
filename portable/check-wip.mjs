// Resource-free checkpoint gates. These do not certify games, phones or FPS.
import {spawnSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {mkdirSync, writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';

const root = fileURLToPath(new URL('../', import.meta.url));
const out = resolve(root, 'build/check-wip');
mkdirSync(out, {recursive: true});
const gates = [
  ['vm-wasm', 'portable/test-script-vm.mjs', '--mode=optimized', '--exceptions=wasm', '--tag=wip'],
  ['vm-legacy-wasm', 'portable/test-script-vm.mjs', '--mode=legacy', '--exceptions=wasm', '--tag=wip'],
  ['vm-js', 'portable/test-script-vm.mjs', '--mode=optimized', '--exceptions=js', '--tag=wip'],
  ['vm-legacy-js', 'portable/test-script-vm.mjs', '--mode=legacy', '--exceptions=js', '--tag=wip'],
  ['vm-old-code-layout', 'portable/test-script-vm.mjs', '--mode=optimized', '--legacy-code-layout', '--tag=wip'],
  ['vm-asan', 'portable/test-script-vm.mjs', '--mode=optimized', '--asan', '--tag=wip'],
  ['platform-wasm', 'portable/test-platform.mjs', '--exceptions=wasm'],
  ['platform-js', 'portable/test-platform.mjs', '--exceptions=js'],
  ['platform-asan', 'portable/test-platform.mjs', '--exceptions=wasm', '--asan'],
  ['containers-wasm', 'portable/test-script-containers.mjs', '--exceptions=wasm'],
  ['containers-js', 'portable/test-script-containers.mjs', '--exceptions=js'],
  ['profile-recorder', 'portable/test-browser-profile-recorder.mjs'],
  ['gl-memory-observer', 'portable/test-observe-gl-memory.mjs'],
];
const git = spawnSync('git', ['rev-parse', 'HEAD'], {cwd: root, encoding: 'utf8'});
const report = {
  scope: 'Resource-free parser/VM, containers, platform and observer regressions; not gameplay, native desktop, full campaign or phone acceptance',
  startedAt: new Date().toISOString(),
  commit: git.status === 0 ? git.stdout.trim() : null,
  node: process.version, platform: process.platform,
  gates: [], vmOutputsIdentical: false, passed: false,
};
const save = () => writeFileSync(resolve(out, 'report.json'), JSON.stringify(report, null, 2) + '\n');
const vmHashes = [];
save();
for (const [name, ...args] of gates) {
  const begin = Date.now();
  const result = spawnSync(process.execPath, args, {
    cwd: root, env: process.env, encoding: 'utf8', maxBuffer: 16 * 1024 * 1024,
    timeout: 240000, windowsHide: true,
  });
  writeFileSync(resolve(out, name + '.log'), (result.stdout || '') + (result.stderr || ''));
  const passed = result.status === 0 && !result.signal && !result.error;
  const entry = {name, command: ['node', ...args], passed, exitCode: result.status,
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
report.vmOutputsIdentical = vmHashes.length === 6 && new Set(vmHashes).size === 1;
report.finishedAt = new Date().toISOString();
report.passed = report.gates.length === gates.length && report.gates.every(g => g.passed) && report.vmOutputsIdentical;
save();
console.log(`${report.passed ? 'PASS' : 'FAIL'} WIP checkpoint; VM outputs identical: ${report.vmOutputsIdentical}`);
console.log(`Evidence: ${resolve(out, 'report.json')}`);
process.exitCode = report.passed ? 0 : 1;
