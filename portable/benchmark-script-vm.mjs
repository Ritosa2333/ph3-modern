import {spawnSync} from 'node:child_process';
import {mkdirSync,writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';
import {readFileSync} from 'node:fs';
const root=fileURLToPath(new URL('../',import.meta.url));
const sdk=resolve(process.env.EMSDK||resolve(root,'../../toolchains/emsdk'));
const tag=process.argv.find(a=>a.startsWith('--tag='))?.slice(6)||'';
if(tag&&!/^[a-z0-9-]{1,40}$/.test(tag))throw Error('Invalid benchmark tag');
const out=resolve(root,'build/script-vm-benchmark'+(tag?'-'+tag:''));mkdirSync(out,{recursive:true});
const env={...process.env,EMSDK:sdk,EM_CONFIG:process.env.EM_CONFIG||resolve(sdk,'.emscripten')};
const report={scope:'Isolated generic VM kernels on Node/Wasm. Excludes parsing. Not game FPS or phone acceptance.',runs:[]};
report.sources=Object.fromEntries(['source/GcLib/gstd/Script.hpp','source/GcLib/gstd/Script.cpp',
    'portable/tests/script-vm-benchmark.cpp'].map(p=>[p,createHash('sha256').update(readFileSync(resolve(root,p))).digest('hex')]));
for(const [name,extra] of [['unfused',['-DPH3_LEGACY_SCRIPT_LOAD_ARITHMETIC=1','-DPH3_LEGACY_SCRIPT_ARITHMETIC_RESULT=1']],
                         ['fused',[]],['fused-original-parameters',['-DPH3_LEGACY_SCRIPT_PARAMETERS=1']],
                         ['fused-original-builtins',['-DPH3_LEGACY_SCRIPT_BUILTINS=1']],
                         ['fused-original-load-guards',['-DPH3_LEGACY_SCRIPT_LOAD_GUARDS=1']],['fused-simd',['-msimd128']],
                         ['fused-simd-cached-string',['-msimd128','-DPH3_ENABLE_SCRIPT_STRINGS=1']]]){
    const output=resolve(out,name+'.cjs');
    const flags=['-std=c++17','-O3','-flto','-g2','-ffp-contract=off','-fwasm-exceptions','-UNDEBUG','-DUNICODE','-D_UNICODE',
        '-sDEFAULT_TO_CXX=1','-sENVIRONMENT=node','-sEXIT_RUNTIME=1','-sALLOW_MEMORY_GROWTH=1','-sSTACK_SIZE=8388608',
        ...['source','source/GcLib','source/GcLib/gstd','source/GcLib/directx'].map(p=>'-I'+resolve(root,p)),
        '--use-port=zlib','--use-port=libpng','--use-port=freetype','--use-port=sdl3','--use-port=vorbis',...extra];
    const compiled=spawnSync(process.env.PH3_PYTHON||'python',[resolve(sdk,'upstream/emscripten/emcc.py'),...flags,
        'portable/tests/script-vm-benchmark.cpp','source/GcLib/gstd/Script.cpp','source/GcLib/gstd/GstdUtility.cpp',
        'source/GcLib/portable/Encoding.cpp','source/GcLib/portable/Win32Compat.cpp','-o',output],
        {cwd:root,env,encoding:'utf8',maxBuffer:16*1024*1024});
    writeFileSync(resolve(out,name+'-build.log'),(compiled.stdout||'')+(compiled.stderr||''));
    if(compiled.error||compiled.status!==0)throw Error(compiled.error?.message||compiled.stderr||'Compile failed');
    const run=spawnSync(process.execPath,[output],{cwd:root,env,encoding:'utf8',maxBuffer:16*1024*1024});
    writeFileSync(resolve(out,name+'.txt'),(run.stdout||'')+(run.stderr||''));
    if(run.error||run.status!==0||run.signal)throw Error(run.error?.message||run.stderr||'Benchmark failed');
    const samples=run.stdout.trim().split(/\r?\n/).map(row=>row.split(' ').map(Number));
    const medians=[0,1,2,3].map(kernel=>{
        const times=samples.filter(row=>row[0]===kernel&&row[1]>0).map(row=>row[2]).sort((a,b)=>a-b);
        if(times.length!==6||times.some(v=>!Number.isFinite(v)))throw Error('Incomplete benchmark samples');
        return (times[2]+times[3])/2;
    });
    report.runs.push({name,flags,medians,samples});console.log(name,medians);
}
writeFileSync(resolve(out,'report.json'),JSON.stringify(report,null,2)+'\n');
