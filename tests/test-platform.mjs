import {spawnSync} from 'node:child_process';
import {mkdirSync} from 'node:fs';
import {resolve} from 'node:path';
import {root, toolchain} from '../tools/lib/project.mjs';

const exceptionMode=process.argv.find(arg=>arg.startsWith('--exceptions='))?.slice(13)||'js';
if(!['js','wasm'].includes(exceptionMode))throw new Error('Unknown exception mode');
const asan=process.argv.includes('--asan');
const {compiler, env, python} = toolchain();
mkdirSync(resolve(root,'build'),{recursive:true});
const output=resolve(root,`build/platform-regression-${exceptionMode}${asan?'-asan':''}.cjs`);
const compiled=spawnSync(python,[compiler,
  'tests/cpp/platform-regression.cpp','source/GcLib/portable/Encoding.cpp',
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
