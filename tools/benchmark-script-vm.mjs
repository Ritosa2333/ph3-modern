import {spawnSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {copyFileSync, mkdirSync, readFileSync, writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {root, toolchain} from './lib/project.mjs';

const options = new Map(process.argv.slice(2).map(arg => {
  const [key, ...value] = arg.replace(/^--/, '').split('=');
  return [key, value.join('=')];
}));
const tag = options.get('tag') || new Date().toISOString().replace(/[^0-9]/g, '').slice(0, 14);
const rounds = Number(options.get('rounds') || 8);
if (!/^[a-z0-9-]{1,48}$/.test(tag) || !Number.isInteger(rounds) || rounds < 4 || rounds > 32)
  throw new Error('Use --tag=a-label and --rounds=4..32.');
const out = resolve(root, 'build', 'vm-benchmark-' + tag);
mkdirSync(resolve(root, 'build'), {recursive: true});
mkdirSync(out);
const {compiler, python, env} = toolchain();
const sha = bytes => createHash('sha256').update(bytes).digest('hex');
const median = values => {
  const ordered = [...values].sort((a, b) => a - b);
  const middle = Math.floor(ordered.length / 2);
  return ordered.length % 2 ? ordered[middle] : (ordered[middle - 1] + ordered[middle]) / 2;
};
function run(command, args, log) {
  const result = spawnSync(command, args, {cwd: root, env, encoding: 'utf8',
    maxBuffer: 32 * 1024 * 1024, timeout: 240000, windowsHide: true});
  if (log) writeFileSync(resolve(out, log), (result.stdout || '') + (result.stderr || ''));
  if (result.error || result.signal || result.status !== 0)
    throw new Error(result.error?.message || result.stderr || `${command} failed (${result.status})`);
  return result.stdout;
}
let baselineRoot = root;
const baseline = options.get('baseline');
let baselineCommit = null;
if (baseline) {
  baselineCommit = run('git', ['rev-parse', '--verify', `${baseline}^{commit}`]).trim();
  const archive = resolve(out, 'baseline.zip');
  run('git', ['archive', '--format=zip', '--output=' + archive, baselineCommit, 'source']);
  baselineRoot = resolve(out, 'baseline');
  run(python, ['-m', 'zipfile', '-e', archive, baselineRoot]);
  mkdirSync(resolve(baselineRoot, 'tests/cpp'), {recursive: true});
  copyFileSync(resolve(root, 'tests/cpp/script-vm-benchmark.cpp'),
    resolve(baselineRoot, 'tests/cpp/script-vm-benchmark.cpp'));
}
const report = {scope: 'Node/Wasm script VM kernels, with parsing outside the timed region',
  baselineCommit, rounds, node: process.version, platform: process.platform,
  toolchain: run(python, [compiler, '--version']).trim(),
  kernels: ['arithmetic', 'function-calls', 'nested-arrays', 'string-conversion'],
  candidates: [], runs: []};
const candidates = [
  {name: 'baseline', directory: baselineRoot, extra: baseline ? [] : [
    '-DPH3_LEGACY_SCRIPT_LOAD_ARITHMETIC=1', '-DPH3_LEGACY_SCRIPT_ARITHMETIC_RESULT=1']},
  {name: 'current', directory: root, extra: []},
];
for (const candidate of candidates) {
  const flags = ['-std=c++17', '-O3', '-flto', '-g2', '-ffp-contract=off',
    '-fwasm-exceptions', '-UNDEBUG', '-DUNICODE', '-D_UNICODE', '-sDEFAULT_TO_CXX=1',
    '-sENVIRONMENT=node', '-sEXIT_RUNTIME=1', '-sALLOW_MEMORY_GROWTH=1', '-sSTACK_SIZE=8388608',
    ...['source', 'source/GcLib', 'source/GcLib/gstd', 'source/GcLib/directx']
      .map(path => '-I' + resolve(candidate.directory, path)),
    '--use-port=zlib', '--use-port=libpng', '--use-port=freetype', '--use-port=sdl3',
    '--use-port=vorbis', ...candidate.extra];
  const sources = ['tests/cpp/script-vm-benchmark.cpp', 'source/GcLib/gstd/Script.cpp',
    'source/GcLib/gstd/GstdUtility.cpp', 'source/GcLib/portable/Encoding.cpp',
    'source/GcLib/portable/Win32Compat.cpp'];
  candidate.binary = resolve(out, candidate.name + '.cjs');
  run(python, [compiler, ...flags, ...sources.map(path => resolve(candidate.directory, path)),
    '-o', candidate.binary], candidate.name + '-build.log');
  report.candidates.push({name: candidate.name, flags,
    wasmSha256: sha(readFileSync(candidate.binary.replace(/\.cjs$/, '.wasm'))),
    sources: Object.fromEntries([...sources, 'source/GcLib/gstd/Script.hpp']
      .map(path => [path, sha(readFileSync(resolve(candidate.directory, path)))]))});
}
// Compile first, then alternate AB/BA to share ordering and host-load effects.
for (let round = 0; round < rounds; ++round) {
  for (const candidate of round % 2 ? [...candidates].reverse() : candidates) {
    const output = run(process.execPath, [candidate.binary], `${round}-${candidate.name}.txt`);
    const samples = output.trim().split(/\r?\n/).map(line => line.trim().split(/\s+/).map(Number));
    const medians = report.kernels.map((_, kernel) => {
      const rows = samples.filter(row => row[0] === kernel);
      if (rows.length !== 7 || rows.some((row, i) => row.length !== 4 || row[1] !== i ||
          row.some(value => !Number.isFinite(value)) || row[2] <= 0))
        throw new Error('Incomplete VM benchmark samples.');
      return median(rows.slice(1).map(row => row[2]));
    });
    report.runs.push({round, candidate: candidate.name, medians, samples});
    console.log(`Round ${round + 1}/${rounds} ${candidate.name}: ${medians.map(n => n.toFixed(3)).join(', ')} ms`);
  }
}
report.results = report.kernels.map((kernel, index) => {
  const costs = Object.fromEntries(candidates.map(({name}) => [name,
    median(report.runs.filter(run => run.candidate === name).map(run => run.medians[index]))]));
  return {kernel, baselineMs: costs.baseline, currentMs: costs.current,
    reductionPercent: 100 * (1 - costs.current / costs.baseline)};
});
writeFileSync(resolve(out, 'report.json'), JSON.stringify(report, null, 2) + '\n');
console.table(report.results);
