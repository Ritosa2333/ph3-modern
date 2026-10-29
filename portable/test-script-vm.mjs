import {spawnSync} from 'node:child_process';
import {existsSync,mkdirSync,writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
const root=fileURLToPath(new URL('../',import.meta.url));
const mode=process.argv.find(arg=>arg.startsWith('--mode='))?.slice(7)||'optimized';
if(!['optimized','legacy'].includes(mode))throw Error('Expected --mode=optimized|legacy');
const exceptions=process.argv.find(arg=>arg.startsWith('--exceptions='))?.slice(13)||'wasm';
if(!['wasm','js'].includes(exceptions))throw Error('Expected --exceptions=wasm|js');
const tag=process.argv.find(arg=>arg.startsWith('--tag='))?.slice(6)||'';
if(tag&&!/^[a-z0-9-]{1,60}$/.test(tag))throw Error('Invalid evidence tag');
const asan=process.argv.includes('--asan');
const legacyCodeLayout=process.argv.includes('--legacy-code-layout');
const simd=process.argv.includes('--simd');
const strings=process.argv.includes('--strings');
const opcodes=process.argv.includes('--opcode-profile');
const label=mode+(exceptions==='wasm'?'':'-js')+(asan?'-asan':'')+(legacyCodeLayout?'-legacy-code-layout':'')+(simd?'-simd':'')+(strings?'-strings':'')+(opcodes?'-opcodes':'')+(tag?'-'+tag:'');
const sdkCandidates=[resolve(root,'../../toolchains/emsdk'),resolve(root,'../../../toolchains/emsdk')];
const sdk=resolve(process.env.EMSDK||sdkCandidates.find(candidate=>
  ['upstream/emscripten/emcc.py','install/emscripten/emcc.py'].some(path=>existsSync(resolve(candidate,path))))||sdkCandidates[0]);
const compiler=['upstream/emscripten/emcc.py','install/emscripten/emcc.py'].map(p=>resolve(sdk,p)).find(existsSync);
if(!compiler)throw Error('Emscripten not found');
// Standalone regression: a fresh clone must not need an ignored historical
// game build manifest, HSoB data or the sibling Launcher to test the parser/VM.
const flags=['-std=c++17','-O3','-flto','-g2','-ffp-contract=off',
  '-DUNICODE','-D_UNICODE','-sDEFAULT_TO_CXX=1',
  ...['source','source/GcLib','source/GcLib/gstd','source/GcLib/directx',
    'source/TouhouDanmakufu/Common','source/TouhouDanmakufu/DnhExecutor'].map(path=>'-I'+resolve(root,path)),
  '--use-port=zlib','--use-port=libpng','--use-port=freetype','--use-port=sdl3','--use-port=vorbis',
  exceptions==='wasm'?'-fwasm-exceptions':'-fexceptions','-UNDEBUG','-sASSERTIONS=1'];
if(mode==='legacy')flags.push('-DPH3_LEGACY_VM_DISPATCH=1','-DPH3_LEGACY_SCRIPT_ALLOCATOR=1','-DPH3_LEGACY_SCRIPT_ENVIRONMENT=1','-DPH3_LEGACY_SCRIPT_MOVES=1','-DPH3_LEGACY_SCRIPT_JUMPS=1','-DPH3_LEGACY_SCRIPT_SCALARS=1','-DPH3_LEGACY_SCRIPT_PARAMETERS=1');
if(legacyCodeLayout)flags.push('-DPH3_LEGACY_SCRIPT_CODE_LAYOUT=1');
if(asan)flags.push('-fsanitize=address','-g','-O1','-sINITIAL_MEMORY=134217728');
if(simd)flags.push('-msimd128');
if(strings)flags.push('-DPH3_ENABLE_SCRIPT_STRINGS=1');
if(opcodes)flags.push('-DPH3_VM_OPCODE_PROFILE=1');
const out=resolve(root,'build/vm-regression');mkdirSync(out,{recursive:true});
const output=resolve(out,`script-vm-${label}.cjs`);
const env={...process.env,EMSDK:sdk,EM_CONFIG:process.env.EM_CONFIG||resolve(sdk,'.emscripten')};
const compile=spawnSync(process.env.PH3_PYTHON||'python',[compiler,...flags,
  'portable/tests/script-vm-regression.cpp','source/GcLib/gstd/Script.cpp','source/GcLib/gstd/GstdUtility.cpp',
  'source/GcLib/portable/Encoding.cpp','source/GcLib/portable/Win32Compat.cpp',
  '-sENVIRONMENT=node','-sEXIT_RUNTIME=1','-sALLOW_MEMORY_GROWTH=1','-sSTACK_SIZE=8388608','-o',output],
  {cwd:root,env,encoding:'utf8',maxBuffer:16*1024*1024});
writeFileSync(resolve(out,`${label}-build.log`),(compile.stdout||'')+(compile.stderr||''));
if(compile.error)throw compile.error;
if(compile.status!==0){console.error(compile.stderr);process.exit(compile.status||1);}
const test=spawnSync(process.execPath,[output],{cwd:root,env,encoding:'utf8',maxBuffer:16*1024*1024});
writeFileSync(resolve(out,`${label}.txt`),(test.stdout||'')+(test.stderr||''));
process.stdout.write(test.stdout||'');process.stderr.write(test.stderr||'');
if(test.error)throw test.error;
if(test.signal)throw Error(`VM regression terminated by ${test.signal}`);
process.exit(test.status===0?0:(test.status||1));
