import {root, browserLibrary, browserExecutable} from './lib/project.mjs';
// Check CDP throttling on BOTH JavaScript and native WebAssembly execution.
// A requested rate alone is not evidence that the Wasm game was slowed down.
import {mkdir,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
const puppeteer = browserLibrary();
const browser=await puppeteer.launch({headless:true,
  executablePath:browserExecutable(),
  args:['--no-proxy-server'],
});
let report;
try{
  const page=await browser.newPage(),session=await page.createCDPSession();
  await page.evaluate(async()=>{
    // (sum n) loops over integers n..1 and returns the wrapping i32 sum.
    // No runtime/game simulation is modified or executed by this probe.
    const body=[1,1,0x7f,0x02,0x40,0x03,0x40,0x20,0,0x45,0x0d,1,
      0x20,1,0x20,0,0x6a,0x21,1,0x20,0,0x41,1,0x6b,0x21,0,0x0c,0,
      0x0b,0x0b,0x20,1,0x0b];
    const bytes=new Uint8Array([0,97,115,109,1,0,0,0,
      1,6,1,0x60,1,0x7f,1,0x7f,3,2,1,0,7,7,1,3,115,117,109,0,0,
      10,body.length+2,1,body.length,...body]);
    const {instance}=await WebAssembly.instantiate(bytes);
    const javascript=count=>{let sum=0;for(let n=count;n>0;--n)sum=(sum+n)|0;return sum;};
    const wasm=instance.exports.sum;
    for(const n of[0,1,37,10000]){
      if(wasm(n)!==javascript(n)||wasm(n)!==((n*(n+1)/2)|0))throw Error('CPU-probe arithmetic failed');
    }
    for(let i=0;i<16;i++){wasm(1000000);javascript(1000000);}
    globalThis.measureCpu=()=>{
      const result={},count=50000000,expected=(count*(count+1)/2)|0;
      for(const [name,fn]of Object.entries({wasm,javascript})){
        const values=[];
        for(let i=0;i<7;i++){
          const start=performance.now(),actual=fn(count);values.push(performance.now()-start);
          if(actual!==expected)throw Error('CPU-probe result changed');
        }
        values.sort((a,b)=>a-b);result[name]={medianMs:values[3],samplesMs:values};
      }
      return result;
    };
  });
  const measurements=[];
  for(const rate of[1,4,1]){
    await session.send('Emulation.setCPUThrottlingRate',{rate});
    await new Promise(resolve=>setTimeout(resolve,150));
    measurements.push({rate,...await page.evaluate(()=>globalThis.measureCpu())});
  }
  const ratios={};
  for(const key of['wasm','javascript'])ratios[key]=measurements[1][key].medianMs/
    ((measurements[0][key].medianMs+measurements[2][key].medianMs)/2);
  report={browser:await browser.version(),measurements,ratios,
    wasmPressureConfirmed:ratios.wasm>=2.5&&ratios.wasm<=6,
    scope:'Isolated main-frame compute calibration; not a phone/GPU/thermal model'};
}finally{await browser.close();}
await mkdir(resolve(root,'build/cpu-throttle-calibration'),{recursive:true});
await writeFile(resolve(root,'build/cpu-throttle-calibration/report.json'),JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify(report));
if(!report.wasmPressureConfirmed)process.exitCode=1;
