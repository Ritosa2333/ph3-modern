import {spawnSync} from 'node:child_process';
import {existsSync,mkdirSync} from 'node:fs';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';

const root=fileURLToPath(new URL('../',import.meta.url));
const exceptionMode=process.argv.find(arg=>arg.startsWith('--exceptions='))?.slice(13)||'js';
if(!['js','wasm'].includes(exceptionMode))throw new Error('Unknown exception mode');
const asan=process.argv.includes('--asan');
const sdkCandidates=[resolve(root,'../../../toolchains/emsdk'),resolve(root,'../../toolchains/emsdk')];
const sdk=resolve(process.env.EMSDK||sdkCandidates.find(candidate=>
  ['upstream/emscripten/emcc.py','install/emscripten/emcc.py'].some(path=>existsSync(resolve(candidate,path))))||sdkCandidates[0]);
const compiler=['upstream/emscripten/emcc.py','install/emscripten/emcc.py'].map(p=>resolve(sdk,p)).find(existsSync);
if(!compiler)throw new Error('Emscripten not found; set EMSDK');
mkdirSync(resolve(root,'build'),{recursive:true});
const output=resolve(root,`build/platform-regression-${exceptionMode}${asan?'-asan':''}.cjs`);
const env={...process.env,EMSDK:sdk,EM_CONFIG:process.env.EM_CONFIG||resolve(sdk,'.emscripten')};
const compiled=spawnSync(process.env.PH3_PYTHON||'python',[compiler,
  'portable/tests/platform-regression.cpp','source/GcLib/portable/Encoding.cpp',
  '-std=c++17','-O1',exceptionMode==='wasm'?'-fwasm-exceptions':'-fexceptions',
  ...(asan?['-fsanitize=address','-g','-sALLOW_MEMORY_GROWTH=1','-sINITIAL_MEMORY=134217728']:[]),
  '-sDEFAULT_TO_CXX=1','-sENVIRONMENT=node','-sEXIT_RUNTIME=1','-o',output],
  {cwd:root,env,stdio:'inherit'});
if(compiled.error)throw compiled.error;
if(compiled.status!==0)process.exit(compiled.status||1);
const test=spawnSync(process.execPath,[output],{cwd:root,env,stdio:'inherit'});
if(test.error)throw test.error;
if(test.signal)throw new Error(`Platform regression terminated by ${test.signal}`);
process.exit(test.status===0?0:(test.status||1));
