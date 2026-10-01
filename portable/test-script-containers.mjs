import {spawnSync} from 'node:child_process';
import {existsSync,mkdirSync} from 'node:fs';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
const root=fileURLToPath(new URL('../',import.meta.url));
const mode=process.argv.find(arg=>arg.startsWith('--exceptions='))?.slice(13)||'wasm';
if(!['js','wasm'].includes(mode))throw Error('Invalid exception mode');
const sdkCandidates=[resolve(root,'../../toolchains/emsdk'),resolve(root,'../../../toolchains/emsdk')];
const sdk=resolve(process.env.EMSDK||sdkCandidates.find(candidate=>
  ['upstream/emscripten/emcc.py','install/emscripten/emcc.py'].some(path=>existsSync(resolve(candidate,path))))||sdkCandidates[0]);
const compiler=['upstream/emscripten/emcc.py','install/emscripten/emcc.py'].map(p=>resolve(sdk,p)).find(existsSync);
if(!compiler)throw Error('Emscripten not found');
mkdirSync(resolve(root,'build'),{recursive:true});
const output=resolve(root,`build/script-containers-${mode}.cjs`);
const env={...process.env,EMSDK:sdk,EM_CONFIG:process.env.EM_CONFIG||resolve(sdk,'.emscripten')};
const compile=spawnSync(process.env.PH3_PYTHON||'python',[compiler,'portable/tests/script-container-regression.cpp',
  '-std=c++17','-O1',mode==='wasm'?'-fwasm-exceptions':'-fexceptions','-sDEFAULT_TO_CXX=1',
  '-sENVIRONMENT=node','-sEXIT_RUNTIME=1','-o',output],{cwd:root,env,stdio:'inherit'});
if(compile.error)throw compile.error;
if(compile.status!==0)process.exit(compile.status||1);
const test=spawnSync(process.execPath,[output],{cwd:root,env,stdio:'inherit'});
if(test.error)throw test.error;
if(test.signal)throw Error(`Container regression terminated by ${test.signal}`);
process.exit(test.status===0?0:(test.status||1));
