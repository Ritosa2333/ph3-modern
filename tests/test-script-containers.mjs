import {spawnSync} from 'node:child_process';
import {mkdirSync} from 'node:fs';
import {resolve} from 'node:path';
import {root, toolchain} from '../tools/lib/project.mjs';
const mode=process.argv.find(arg=>arg.startsWith('--exceptions='))?.slice(13)||'wasm';
if(!['js','wasm'].includes(mode))throw Error('Invalid exception mode');
const {compiler, env, python} = toolchain();
mkdirSync(resolve(root,'build'),{recursive:true});
const output=resolve(root,`build/script-containers-${mode}.cjs`);
const compile=spawnSync(python,[compiler,'tests/cpp/script-container-regression.cpp',
  '-std=c++17','-O1',mode==='wasm'?'-fwasm-exceptions':'-fexceptions','-sDEFAULT_TO_CXX=1',
  '-sENVIRONMENT=node','-sEXIT_RUNTIME=1','-o',output],{cwd:root,env,stdio:'inherit'});
if(compile.error)throw compile.error;
if(compile.status!==0)process.exit(compile.status||1);
const test=spawnSync(process.execPath,[output],{cwd:root,env,stdio:'inherit'});
if(test.error)throw test.error;
if(test.signal)throw Error(`Container regression terminated by ${test.signal}`);
process.exit(test.status===0?0:(test.status||1));
