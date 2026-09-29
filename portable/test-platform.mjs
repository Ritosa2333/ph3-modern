import {spawnSync} from 'node:child_process';
import {existsSync,mkdirSync} from 'node:fs';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';

const root=fileURLToPath(new URL('../',import.meta.url));
const sdk=resolve(process.env.EMSDK||resolve(root,'../../../toolchains/emsdk'));
const compiler=['upstream/emscripten/emcc.py','install/emscripten/emcc.py'].map(p=>resolve(sdk,p)).find(existsSync);
if(!compiler)throw new Error('Emscripten not found; set EMSDK');
mkdirSync(resolve(root,'build'),{recursive:true});
const output=resolve(root,'build/platform-regression.cjs');
const env={...process.env,EMSDK:sdk,EM_CONFIG:process.env.EM_CONFIG||resolve(sdk,'.emscripten')};
const compiled=spawnSync(process.env.PH3_PYTHON||'python',[compiler,
  'portable/tests/platform-regression.cpp','source/GcLib/portable/Encoding.cpp',
  '-std=c++17','-O1','-fexceptions','-sDEFAULT_TO_CXX=1','-sENVIRONMENT=node','-sEXIT_RUNTIME=1','-o',output],
  {cwd:root,env,stdio:'inherit'});
if(compiled.error)throw compiled.error;
if(compiled.status!==0)process.exit(compiled.status||1);
const test=spawnSync(process.execPath,[output],{cwd:root,env,stdio:'inherit'});
if(test.error)throw test.error;
process.exit(test.status||0);
