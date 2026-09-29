const canvas = document.querySelector('#canvas');
const start = document.querySelector('#start');
const status = document.querySelector('#status');
const progress = document.querySelector('#progress');
const log = document.querySelector('#log');
const fullscreen = document.querySelector('#fullscreen');
const diagnostics = {phase: 'idle', errors: [], messages: [], loadedBytes: 0, loadedFiles: 0};
window.ph3Diagnostics = diagnostics;
setInterval(() => {
  const output=document.getElementById('performance'), p=diagnostics.performance;
  if (!output || !p || document.hidden) return;
  output.hidden=false;
  output.textContent=`画面更新 ${p.displayFps.toFixed(1)} FPS · 逻辑 ${p.logicHz.toFixed(1)} Hz · 主线程帧耗时 P95 ${p.frameMsP95.toFixed(1)} ms / 最大 ${p.frameMsMax.toFixed(1)} ms`;
}, 1000);
let runtime;
let ready = false;
let running = false;
let syncPending = false;
let logDirty = false;
const logDetails = log.closest('details');
function flushLog() {
  if (!logDirty || !logDetails?.open) return;
  log.textContent = diagnostics.messages.slice(-180).join('\n');
  log.scrollTop = log.scrollHeight;
  logDirty = false;
}
logDetails?.addEventListener('toggle', flushLog);
setInterval(flushLog, 200);

function message(text) {
  const value = String(text);
  diagnostics.messages.push(value);
  if (diagnostics.messages.length > 800) diagnostics.messages.shift();
  // Asset churn during shooting must not force DOM layout for a closed log.
  logDirty = true;
}

function fail(error) {
  const text = error?.stack || error?.message || String(error);
  diagnostics.errors.push(text);
  diagnostics.phase = 'error';
  status.textContent = '运行失败：' + (error?.message || String(error));
  message(text);
  console.error(error);
  start.disabled = true;
}

window.addEventListener('error', event => fail(event.error || event.message));
window.addEventListener('unhandledrejection', event => fail(event.reason));
canvas.addEventListener('contextmenu', event => event.preventDefault());
canvas.addEventListener('pointerdown', () => canvas.focus());
canvas.addEventListener('webglcontextlost', event => {
  event.preventDefault();
  fail(new Error('图形上下文丢失。请刷新页面重新启动；此预览尚不支持上下文恢复。'));
});
window.addEventListener('keydown', event => {
  if (running && ['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown', 'Space', 'Tab'].includes(event.code))
    event.preventDefault();
});

async function syncStorage(populate = false) {
  if (!runtime || syncPending) return;
  syncPending = true;
  try {
    await new Promise((resolve, reject) => runtime.FS.syncfs(populate, error => error ? reject(error) : resolve()));
  } finally {
    syncPending = false;
  }
}

async function install(file, totalBytes) {
  for (let attempt = 0; attempt < 3; ++attempt) {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), file.bytes > 32 * 1048576 ? 120000 : 20000);
    let reader, offset = 0;
    try {
      const response = await fetch(file.url, {cache: 'no-store', signal: controller.signal});
      if (!response.ok) throw new Error(`资源读取失败：${file.path} (${response.status})`);
      const bytes = new Uint8Array(file.bytes);
      reader = response.body.getReader();
      while (true) {
        const {done, value} = await reader.read();
        if (done) break;
        if (offset + value.length > bytes.length) throw new Error(`资源大小在读取时发生变化：${file.path}`);
        bytes.set(value, offset);
        offset += value.length;
        diagnostics.loadedBytes += value.length;
        progress.value = diagnostics.loadedBytes / totalBytes;
        if (diagnostics.phase === 'loading')
          status.textContent = `正在读取本地资源：${(diagnostics.loadedBytes / 1048576).toFixed(1)} / ${(totalBytes / 1048576).toFixed(1)} MB`;
      }
      if (offset !== file.bytes) throw new Error(`资源不完整：${file.path}`);
      runtime.FS.mkdirTree(file.path.slice(0, file.path.lastIndexOf('/')) || '/');
      runtime.FS.writeFile(file.path, bytes, {canOwn: true});
      ++diagnostics.loadedFiles;
      return;
    } catch (error) {
      diagnostics.loadedBytes -= offset;
      if (reader) await reader.cancel().catch(() => {});
      const transient = error instanceof TypeError || error?.name === 'AbortError';
      if (!transient || attempt === 2 || diagnostics.phase === 'error')
        throw new Error(`资源加载失败：${file.path} — ${error.message}`, {cause: error});
      diagnostics.resourceRetries = (diagnostics.resourceRetries || 0) + 1;
      message(`本地连接中断，重试 ${attempt + 1}/2：${file.path}`);
      await new Promise(resolve => setTimeout(resolve, 300 * (attempt + 1)));
    } finally {
      clearTimeout(timeout);
    }
  }
}

function exists(path) { return runtime.FS.analyzePath(path).exists; }

function seedDirectory(source, destination) {
  const fs = runtime.FS;
  fs.mkdirTree(destination);
  for (const name of fs.readdir(source)) {
    if (name === '.' || name === '..') continue;
    const from = source + '/' + name, to = destination + '/' + name;
    if (fs.isDir(fs.stat(from).mode)) seedDirectory(from, to);
    else if (!exists(to)) fs.writeFile(to, fs.readFile(from));
  }
}

async function prepareStorage() {
  const fs = runtime.FS;
  fs.mkdirTree('/save');
  fs.mount(runtime.IDBFS, {autoPersist: true}, '/save');
  await syncStorage(true);
  // Persist only save/replay/configuration data, not hundreds of MB of assets.
  // The native owner still opens its original paths via these filesystem links.
  for (const name of ['data', 'replay']) {
    const nativePath = '/game/script/th_hsob/' + name;
    fs.mkdirTree(nativePath);
    seedDirectory(nativePath, '/save/' + name);
    fs.rename(nativePath, '/initial-' + name);
    fs.symlink('/save/' + name, nativePath);
  }
  if (exists('/game/config.dat')) {
    if (!exists('/save/config.dat')) fs.writeFile('/save/config.dat', fs.readFile('/game/config.dat'));
    fs.unlink('/game/config.dat');
    fs.symlink('/save/config.dat', '/game/config.dat');
  }
  await syncStorage();
}

async function prepare() {
  diagnostics.phase = 'loading';
  start.disabled = true;
  progress.hidden = false;
  status.textContent = '正在加载完整 Woo 引擎…';
  const {default: createWoo} = await import('./ph3-open.mjs');
  runtime = await createWoo({
    canvas,
    noInitialRun: true,
    print: text => { message(text); console.log('[Woo]', text); },
    printErr: text => { message(text); console.error('[Woo]', text); },
    onAbort: reason => fail(new Error('Wasm 中止：' + reason)),
  });
  window.ph3Runtime = runtime;
  const response = await fetch('/local-manifest.json', {cache: 'no-store'});
  if (!response.ok) throw new Error('本地资源清单不可用。请使用 portable/serve-local.py 启动。');
  const manifest = await response.json();
  const totalBytes = manifest.files.reduce((sum, file) => sum + file.bytes, 0);
  let cursor = 0;
  await Promise.all(Array.from({length: 4}, async () => {
    while (cursor < manifest.files.length && diagnostics.phase === 'loading')
      await install(manifest.files[cursor++], totalBytes);
  }));
  if (diagnostics.phase === 'error') throw new Error('资源加载已停止，请刷新后重试。');
  await prepareStorage();
  runtime.FS.chdir('/game');
  ready = true;
  diagnostics.phase = 'ready';
  start.textContent = '开始游戏';
  start.disabled = false;
  progress.hidden = true;
  status.textContent = '资源已就绪。点击开始游戏以启用声音。';
}

function launch() {
  // This synchronous second click keeps browser user activation for SDL audio.
  diagnostics.phase = 'starting';
  start.disabled = true;
  canvas.focus();
  runtime.callMain([]);
  if (diagnostics.phase === 'error') return;
  running = true;
  diagnostics.phase = 'running';
  diagnostics.startedAt = performance.now();
  start.textContent = '游戏运行中';
  fullscreen.disabled = false;
  status.textContent = '游戏运行中。点击画面后使用键盘操作。';
}

start.addEventListener('click', async () => {
  try {
    if (ready) launch();
    else await prepare();
  } catch (error) { fail(error); }
});

fullscreen.addEventListener('click', async () => {
  try {
    await document.querySelector('.screen').requestFullscreen();
    canvas.focus();
  } catch (error) { message('无法进入全屏：' + error.message); }
});
document.addEventListener('visibilitychange', () => {
  if (document.hidden && ready) syncStorage().catch(error => message('存档同步失败：' + error.message));
});
window.addEventListener('pagehide', () => { if (ready) syncStorage().catch(() => {}); });
