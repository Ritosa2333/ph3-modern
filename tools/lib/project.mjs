import {existsSync} from 'node:fs';
import {createRequire} from 'node:module';
import {dirname, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';

export const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
export const python = process.env.PH3_PYTHON || 'python';

export function toolchain() {
  const candidates = process.env.EMSDK ? [resolve(process.env.EMSDK)] : [
    resolve(root, 'toolchains/emsdk'),
    resolve(root, '../../toolchains/emsdk'),
    resolve(root, '../../../toolchains/emsdk'),
  ];
  for (const sdk of candidates) {
    const compiler = ['upstream/emscripten/emcc.py', 'install/emscripten/emcc.py']
      .map(path => resolve(sdk, path)).find(existsSync);
    if (compiler) return {sdk, compiler, python, env: {
      ...process.env, EMSDK: sdk,
      EM_CONFIG: process.env.EM_CONFIG || resolve(sdk, '.emscripten'),
    }};
  }
  throw new Error('Set EMSDK to an installed Emscripten SDK. See docs/DEVELOPMENT.md.');
}

export function browserLibrary() {
  const locations = [root, process.env.HSOB_LAUNCHER, resolve(root, '../../eagler-touhou')]
    .filter(Boolean);
  for (const location of locations) {
    const require = createRequire(resolve(location, 'package.json'));
    let path;
    try { path = require.resolve('puppeteer-core'); }
    catch (error) {
      if (error.code !== 'MODULE_NOT_FOUND') throw error;
      continue;
    }
    return require(path);
  }
  throw new Error('Run npm ci to install browser test dependencies.');
}

export function browserExecutable() {
  const explicit = process.env.PH3_CHROME || process.env.HSOB_CHROME;
  if (explicit) return explicit;
  const candidates = process.platform === 'win32' ? [
    'C:/Program Files/Google/Chrome/Application/chrome.exe',
    'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  ] : process.platform === 'darwin' ? [
    '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  ] : ['/usr/bin/google-chrome', '/usr/bin/chromium', '/usr/bin/chromium-browser'];
  const executable = candidates.find(existsSync);
  if (!executable) throw new Error('Set PH3_CHROME to a Chrome or Chromium executable.');
  return executable;
}
