import {spawnSync} from 'node:child_process';
import {existsSync, mkdirSync, readFileSync, rmSync, writeFileSync} from 'node:fs';
import {dirname, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';

const root = resolve(fileURLToPath(new URL('../', import.meta.url)));
const workspace = resolve(root, '../../..');
const sdk = resolve(process.env.EMSDK ?? resolve(workspace, 'toolchains/emsdk'));
const emcc = ['upstream/emscripten/emcc.py', 'install/emscripten/emcc.py']
  .map(path => resolve(sdk, path)).find(existsSync);
if (!emcc) throw Error('Emscripten not found; set EMSDK.');

const python = process.env.PH3_PYTHON ?? 'python';
const env = {...process.env, EMSDK: sdk, EM_CONFIG: process.env.EM_CONFIG ?? resolve(sdk, '.emscripten')};
const project = resolve(root, 'DnhExecutor.vcxproj');
const out = resolve(root, 'build/web-audit');
rmSync(out, {recursive: true, force: true});
mkdirSync(out, {recursive: true});

const xml = readFileSync(project, 'utf8');
const sources = [...xml.matchAll(/<ClCompile Include="([^"]+)"\s*\/>/g)]
  .map(match => match[1].replaceAll('\\', '/'))
  .filter(path => !path.includes('/zlib/'));

const platformSources = new Set([
  'source/GcLib/directx/DirectGraphics.cpp',
  'source/GcLib/directx/DirectInput.cpp',
  'source/GcLib/directx/DirectSound.cpp',
  'source/GcLib/gstd/Application.cpp',
  'source/GcLib/gstd/File.cpp',
  'source/GcLib/gstd/FpsController.cpp',
  'source/GcLib/gstd/Logger.cpp',
  'source/GcLib/gstd/Thread.cpp',
  'source/GcLib/gstd/Window.cpp',
  'source/TouhouDanmakufu/DnhExecutor/WinMain.cpp',
]);

const flags = [
  '-std=c++17', '-O0', '-fexceptions', '-DUNICODE', '-D_UNICODE',
  '-I' + resolve(root, 'source'),
  '-I' + resolve(root, 'source/GcLib'),
  '-I' + resolve(root, 'source/GcLib/gstd'),
  '-I' + resolve(root, 'source/GcLib/directx'),
  '-I' + resolve(root, 'source/TouhouDanmakufu/Common'),
  '-I' + resolve(root, 'source/TouhouDanmakufu/DnhExecutor'),
  '--use-port=zlib', '--use-port=libpng', '--use-port=freetype', '--use-port=sdl3',
];

const results = [];
for (const source of sources) {
  const input = resolve(root, source);
  const object = resolve(out, source.replace(/^source\//, '').replace(/\.(cpp|c)$/i, '.o'));
  mkdirSync(dirname(object), {recursive: true});
  const result = spawnSync(python, [emcc, ...flags, '-c', input, '-o', object], {
    cwd: root, env, encoding: 'utf8', windowsHide: true,
  });
  const rawDiagnostic = ((result.stderr || result.stdout) ?? '').trim().split(/\r?\n/);
  const errorLines = rawDiagnostic.filter(line => /(?:fatal )?error:/.test(line));
  results.push({
    source,
    category: platformSources.has(source) ? 'platform'
      : source.startsWith('source/GcLib/directx/') ? 'dx-adapter'
      : 'game',
    ok: result.status === 0,
    status: result.status,
    diagnostic: (errorLines.length ? errorLines : rawDiagnostic).slice(0, 12),
  });
}

const count = category => ({
  passed: results.filter(x => x.category === category && x.ok).length,
  failed: results.filter(x => x.category === category && !x.ok).length,
});
const report = {
  upstreamCommit: '97ca3b5',
  project: 'DnhExecutor.vcxproj',
  game: count('game'),
  dxAdapter: count('dx-adapter'),
  platform: count('platform'),
  failures: results.filter(x => !x.ok),
};
writeFileSync(resolve(out, 'compile-report.json'), JSON.stringify(report, null, 2) + '\n');

console.log(`Woo Web audit: game ${report.game.passed}/${report.game.passed + report.game.failed}, ` +
  `dx-adapter ${report.dxAdapter.passed}/${report.dxAdapter.passed + report.dxAdapter.failed}, ` +
  `platform ${report.platform.passed}/${report.platform.passed + report.platform.failed}`);
for (const failure of report.failures.slice(0, 12)) {
  console.log(`FAIL [${failure.category}] ${failure.source}`);
  console.log(failure.diagnostic.join('\n'));
}
if (report.failures.length) process.exitCode = 2;

