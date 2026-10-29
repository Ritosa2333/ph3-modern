import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {copyFileSync, mkdirSync, mkdtempSync, rmSync, writeFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {resolve, sep} from 'node:path';
import {pathToFileURL} from 'node:url';
import {root} from '../tools/lib/project.mjs';

const directory = mkdtempSync(resolve(tmpdir(), 'ph3-project-config-'));
try {
  mkdirSync(resolve(directory, 'tools/lib'), {recursive: true});
  copyFileSync(resolve(root, 'tools/lib/project.mjs'), resolve(directory, 'tools/lib/project.mjs'));
  const sdk = resolve(directory, 'sdk');
  mkdirSync(resolve(sdk, 'upstream/emscripten'), {recursive: true});
  writeFileSync(resolve(sdk, 'upstream/emscripten/emcc.py'), '');
  const dependency = resolve(directory, 'node_modules/puppeteer-core');
  mkdirSync(dependency, {recursive: true});
  writeFileSync(resolve(dependency, 'package.json'), '{"main":"index.cjs"}');
  writeFileSync(resolve(dependency, 'index.cjs'), 'module.exports={local:true};');
  const module = pathToFileURL(resolve(directory, 'tools/lib/project.mjs')).href;
  const script = `import assert from 'node:assert/strict';
    import {root,toolchain,browserLibrary,browserExecutable} from ${JSON.stringify(module)};
    assert.equal(root, ${JSON.stringify(directory)});
    assert.equal(toolchain().sdk, process.env.EMSDK);
    assert.equal(toolchain().python, 'selected-python');
    assert.equal(toolchain().env.EM_CONFIG, 'selected-config');
    assert.equal(browserLibrary().local, true);
    assert.equal(browserExecutable(), 'selected-chrome');`;
  const env = {...process.env, EMSDK: sdk, PH3_PYTHON: 'selected-python',
    EM_CONFIG: 'selected-config', PH3_CHROME: 'selected-chrome', HSOB_LAUNCHER: 'unused-launcher'};
  const valid = spawnSync(process.execPath, ['--input-type=module', '-e', script],
    {env, encoding: 'utf8', windowsHide: true});
  assert.equal(valid.status, 0, valid.stderr);
  const invalid = spawnSync(process.execPath, ['--input-type=module', '-e',
    `import {toolchain} from ${JSON.stringify(module)};toolchain();`],
    {env: {...env, EMSDK: resolve(directory, 'missing')}, encoding: 'utf8', windowsHide: true});
  assert.notEqual(invalid.status, 0);
  assert.match(invalid.stderr, /Set EMSDK/);
  console.log('PASS isolated checkout configuration, explicit SDK selection and local browser dependencies');
} finally {
  assert.ok(directory.startsWith(resolve(tmpdir()) + sep));
  rmSync(directory, {recursive: true, force: true});
}
