import {spawnSync} from 'node:child_process';
import {mkdirSync, writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {root, toolchain} from '../tools/lib/project.mjs';

const {compiler, env, python} = toolchain();
const output = resolve(root, 'build/render-node-benchmark.cjs');
mkdirSync(resolve(root, 'build'), {recursive: true});
const compiled = spawnSync(python, [compiler,
  'tests/cpp/render-node-benchmark.cpp', '-std=c++17', '-O3', '-flto', '-UNDEBUG',
  '-fwasm-exceptions', '-sDEFAULT_TO_CXX=1', '-sENVIRONMENT=node', '-sEXIT_RUNTIME=1', '-o', output],
  {cwd: root, env, stdio: 'inherit'});
if (compiled.error) throw compiled.error;
if (compiled.status !== 0) process.exit(compiled.status || 1);
const tested = spawnSync(process.execPath, [output], {cwd: root, env, encoding: 'utf8'});
if (tested.stdout) process.stdout.write(tested.stdout);
if (tested.stderr) process.stderr.write(tested.stderr);
if (tested.error) throw tested.error;
if (tested.signal) throw new Error(`Queue benchmark terminated by ${tested.signal}`);
if (tested.status !== 0) process.exit(tested.status || 1);
const report = JSON.parse(tested.stdout);
if (!report.ownershipAndChecksumsMatch || report.samples?.length !== 8)
  throw new Error('Queue benchmark did not produce a complete result');
writeFileSync(resolve(root, 'build/render-node-benchmark.json'), JSON.stringify(report, null, 2) + '\n');
