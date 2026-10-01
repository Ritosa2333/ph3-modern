import {spawn, spawnSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {existsSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync} from 'node:fs';
import {basename, dirname, extname, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';

const root = fileURLToPath(new URL('../', import.meta.url));
const sdkCandidates = [resolve(root, '../../../toolchains/emsdk'), resolve(root, '../../toolchains/emsdk')];
const sdk = resolve(process.env.EMSDK || sdkCandidates.find(candidate =>
  ['upstream/emscripten/emcc.py', 'install/emscripten/emcc.py'].some(path => existsSync(resolve(candidate, path)))) || sdkCandidates[0]);
const emcc = ['upstream/emscripten/emcc.py', 'install/emscripten/emcc.py']
  .map(p => resolve(sdk, p)).find(existsSync);
if (!emcc) throw new Error('Emscripten not found; set EMSDK.');
const python = process.env.PH3_PYTHON || 'python';
const env = {...process.env, EMSDK: sdk, EM_CONFIG: process.env.EM_CONFIG || resolve(sdk, '.emscripten')};
const args = process.argv.slice(2);
const variant = args.find(arg => arg.startsWith('--variant='))?.slice(10) || 'standard';
if (!['standard', 'boundary', 'verify'].includes(variant)) throw new Error('Unknown build variant');
const exceptionMode = args.find(arg => arg.startsWith('--exceptions='))?.slice(13) || 'js';
if (!['js', 'wasm'].includes(exceptionMode)) throw new Error('Unknown exception mode');
const buildLabel = args.find(arg => arg.startsWith('--label='))?.slice(8) || '';
if (buildLabel && !/^[a-z0-9][a-z0-9-]{0,47}$/.test(buildLabel)) throw new Error('Invalid build label');
const only = args.find(arg => arg.startsWith('--only='))?.slice(7);
const compileOnly = args.includes('--compile-only') || !!only;
const force = args.includes('--force');
const jobs = Math.max(1, Math.min(8, Number(process.env.PH3_JOBS || 4)));
const outputStem = 'web' + (variant === 'standard' ? '' : '-' + variant) +
  (exceptionMode === 'wasm' ? '-wasm-exceptions' : '') + (buildLabel ? '-' + buildLabel : '');
const objectDir = resolve(root, 'build/' + outputStem + '-objects');
const out = resolve(root, 'build/' + outputStem);
mkdirSync(objectDir, {recursive: true});
mkdirSync(out, {recursive: true});
const sha = data => createHash('sha256').update(data).digest('hex');
const normalized = path => path.replaceAll('\\', '/');

function sourceTree(dir) {
  return readdirSync(resolve(root, dir), {withFileTypes: true}).flatMap(entry =>
    entry.isDirectory() ? sourceTree(`${dir}/${entry.name}`) : [`${dir}/${entry.name}`]);
}
const original = [...readFileSync(resolve(root, 'DnhExecutor.vcxproj'), 'utf8')
  .matchAll(/<ClCompile Include="([^"]+)"\s*\/>/g)]
  .map(match => normalized(match[1])).filter(path => !path.includes('/zlib/'));
// The full executor project is the authority. Only the OS entry point can be
// replaced; portable implementations otherwise sit behind their existing owner.
const entryReplacement = existsSync(resolve(root, 'portable/BrowserMain.cpp'))
  ? 'portable/BrowserMain.cpp' : null;
const mapped = original.map(path => entryReplacement && path.endsWith('/WinMain.cpp') ? entryReplacement : path);
const portableSources = sourceTree('source/GcLib/portable').filter(path => extname(path) === '.cpp');
const allCandidates = [...new Set([...mapped, ...portableSources])];
const includedCpp = new Set();
for (const source of allCandidates) {
  for (const match of readFileSync(resolve(root, source), 'utf8').matchAll(/#\s*include\s*"([^"]+\.cpp)"/g))
    includedCpp.add(normalized(resolve(root, dirname(source), match[1])));
}
const allSources = allCandidates.filter(path => !includedCpp.has(normalized(resolve(root, path))));
const sources = only ? allSources.filter(path => path.includes(only)) : allSources;
if (!sources.length) throw new Error(`No source matches ${only || 'the executor project'}`);

const flags = [
  '-std=c++17', '-O2', '-g2', exceptionMode === 'wasm' ? '-fwasm-exceptions' : '-fexceptions', '-ffp-contract=off',
  '-DUNICODE', '-D_UNICODE', '-sDEFAULT_TO_CXX=1',
  ...['source', 'source/GcLib', 'source/GcLib/gstd', 'source/GcLib/directx',
    'source/TouhouDanmakufu/Common', 'source/TouhouDanmakufu/DnhExecutor'].map(path => '-I' + resolve(root, path)),
  '--use-port=zlib', '--use-port=libpng', '--use-port=freetype', '--use-port=sdl3',
  '--use-port=vorbis',
];
// Audio ports can be selected without a second ad-hoc compiler command.
if (variant === 'verify') flags.push('-DWOO_REPLAY_VERIFY=1', '-DWOO_TRACE_MKM_RNG=1', '-DPH3_WEB_GL_ERROR_CHECK=1');
if (variant === 'boundary') flags.push('-DPH3_REPLAY_BOUNDARY=1');
if (process.env.PH3_EXTRA_FLAGS) flags.push(...JSON.parse(process.env.PH3_EXTRA_FLAGS));
const toolchain = spawnSync(python, [emcc, '--version'], {env, encoding: 'utf8', windowsHide: true});
if (toolchain.status !== 0) throw new Error(toolchain.stderr || 'Cannot execute Emscripten.');
const headers = [...sourceTree('source'), ...sourceTree('portable')]
  .filter(path => ['.h', '.hpp', '.inl', '.inc'].includes(extname(path)));
const shellSources = sourceTree('portable/web').filter(path => ['.mjs', '.html'].includes(extname(path)));
const snapshotPaths = [...new Set([...allCandidates, ...headers, ...shellSources, 'portable/build-web.mjs'])];
const snapshot = new Map(snapshotPaths.map(path => [path, sha(readFileSync(resolve(root, path)))]));
const headerSignature = sha(JSON.stringify([flags, toolchain.stdout, headers.map(path => [path, snapshot.get(path)])]));

function run(compilerArgs) {
  return new Promise(resolveResult => {
    const child = spawn(python, [emcc, ...compilerArgs], {cwd: root, env, windowsHide: true});
    let stdout = '', stderr = '';
    child.stdout.on('data', bytes => { stdout += bytes.toString(); });
    child.stderr.on('data', bytes => { stderr += bytes.toString(); });
    child.on('error', error => resolveResult({status: -1, stdout, stderr: stderr + error.message}));
    child.on('close', status => resolveResult({status, stdout, stderr}));
  });
}

let cursor = 0;
const results = new Array(sources.length);
await Promise.all(Array.from({length: Math.min(jobs, sources.length)}, async () => {
  while (cursor < sources.length) {
    const index = cursor++, source = sources[index];
    const object = resolve(objectDir, source.replace(/\.cpp$/, '.o'));
    const metadata = object + '.json';
    const signature = sha(JSON.stringify([headerSignature, snapshot.get(source),
      // An included implementation must invalidate its owning translation unit.
      [...includedCpp].map(path => [path, sha(readFileSync(path))]) ]));
    mkdirSync(dirname(object), {recursive: true});
    const cached = !force && existsSync(object) && existsSync(metadata)
      && JSON.parse(readFileSync(metadata, 'utf8')).signature === signature;
    let result = {status: 0, stdout: '', stderr: ''};
    if (!cached) {
      result = await run([...flags, '-c', resolve(root, source), '-o', object]);
      writeFileSync(object + '.log', result.stdout + result.stderr);
      if (result.status === 0) writeFileSync(metadata, JSON.stringify({signature}) + '\n');
    }
    const diagnostic = result.stderr.split(/\r?\n/).filter(line => /(?:fatal )?error:/.test(line));
    results[index] = {source, object, cached, ok: result.status === 0, diagnostic};
    console.log(`${result.status === 0 ? cached ? 'CACHE' : 'PASS' : 'FAIL'} ${source}`);
    if (result.status !== 0) console.log((diagnostic.length ? diagnostic.slice(0, 16).join('\n') : result.stderr.slice(-8000)));
  }
}));
const changed = [...snapshot].filter(([path, hash]) => sha(readFileSync(resolve(root, path))) !== hash).map(([path]) => path);
const report = {
  project: 'DnhExecutor.vcxproj', originalSources: original, sources: allSources,
  entryReplacement, selected: only || null, toolchain: toolchain.stdout.trim(), flags,
  compiled: results.filter(result => result.ok).length, total: results.length,
  failures: results.filter(result => !result.ok), changedDuringBuild: changed,
};
writeFileSync(resolve(objectDir, 'compile-report.json'), JSON.stringify(report, null, 2) + '\n');
if (report.failures.length || changed.length) {
  if (changed.length) console.error('Sources changed during build:', changed.join(', '));
  process.exitCode = 2;
} else if (!compileOnly) {
  const linkFlags = [
    '-sMODULARIZE=1', '-sEXPORT_ES6=1', '-sENVIRONMENT=web', '-sINVOKE_RUN=0',
    '-sALLOW_MEMORY_GROWTH=1', '-sINITIAL_MEMORY=67108864', '-sMAXIMUM_MEMORY=2147483648',
    '-sSTACK_SIZE=8388608', '-sEXIT_RUNTIME=0', '-sFORCE_FILESYSTEM=1',
    '-sMIN_WEBGL_VERSION=2', '-sMAX_WEBGL_VERSION=2',
    '-sEXPORTED_RUNTIME_METHODS=FS,IDBFS,callMain,ccall,UTF8ToString,HEAPU8', '-lidbfs.js',
  ];
  const result = await run([...flags, ...results.map(item => item.object), ...linkFlags,
    '-o', resolve(out, 'ph3-open.mjs')]);
  writeFileSync(resolve(objectDir, 'link.log'), result.stdout + result.stderr);
  if (result.status !== 0) {
    console.error(result.stderr.slice(-24000));
    process.exitCode = 2;
  } else {
    rmSync(resolve(out, 'index.html'), {force: true});
    for (const name of ['hsob.html', 'shell.mjs', 'eagler-host.mjs'])
      writeFileSync(resolve(out, name), readFileSync(resolve(root, 'portable/web', name)));
    const files = ['ph3-open.mjs', 'ph3-open.wasm', 'hsob.html', 'shell.mjs', 'eagler-host.mjs'];
    const buildManifest = {
      project: 'ph3-open-web', variant, exceptionMode, buildLabel,
      gameplayAuthority: '5c92f28e17b6ef3721a3ac4caa9de3af3164b51e',
      datSource: '900aa3ebeb329187e8c2852b8ad41caa5200ca9e',
      builtAt: new Date().toISOString(), toolchain: report.toolchain, sources: Object.fromEntries(snapshot),
      flags, linkFlags, executorSources: original, entryReplacement,
      outputs: Object.fromEntries(files.map(path => {
        const bytes = readFileSync(resolve(out, path));
        return [path, {bytes: bytes.length, sha256: sha(bytes)}];
      })),
      verification: {compiled: true, linked: true, browser: false, gameplay: false},
    };
    writeFileSync(resolve(out, 'build.json'), JSON.stringify(buildManifest, null, 2) + '\n');
    const runtimeManifest = {
      game: 'hsob',
      protocol: 'eagler-touhou/1',
      adapter: 'danmakufu-ph3-eagler',
      profile: variant === 'standard' ? 'production' : 'diagnostic',
      version: 'hsob-1.12-ph3-open-web',
      features: {thprac: false, languages: false, focusHitbox: false},
      music: ['ogg-stream', 'ogg-full', 'none'],
      touchReplay: false,
      execution: {
        kind: 'cpp-sdl3',
        sha256: sha(readFileSync(resolve(out, 'ph3-open.wasm'))),
        loaderSha256: sha(readFileSync(resolve(out, 'ph3-open.mjs'))),
        architecture: {
          loop: 'original-60hz-logic-display-cadence',
          audio: 'sdl3-webaudio-vorbis',
          renderer: 'd3d9-semantic-webgl2',
          files: 'managed-data-memfs-idbfs',
          fonts: 'freetype-ms-gothic',
          input: 'directinput-sdl-launcher-bridge',
          launcher: 'eagler-touhou/1',
        },
      },
    };
    writeFileSync(resolve(out, 'manifest.json'), JSON.stringify(runtimeManifest, null, 2) + '\n');
    const runtimeNames = ['hsob.html', 'shell.mjs', 'eagler-host.mjs', 'manifest.json',
      'ph3-open.mjs', 'ph3-open.wasm', 'build.json'];
    const runtimeFiles = Object.fromEntries(runtimeNames.map(path => {
      const bytes = readFileSync(resolve(out, path));
      return [path, {bytes: bytes.length, sha256: sha(bytes)}];
    }));
    writeFileSync(resolve(out, 'runtime-files.json'), JSON.stringify({
      schema: 'eagler-touhou/runtime-directory/1', files: runtimeFiles,
    }, null, 2) + '\n');
    console.log(`Linked complete Woo Web runtime: ${out}`);
  }
}
