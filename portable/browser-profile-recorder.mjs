// Inject into the Runtime frame, not the Node/CDP polling process. A delayed
// host read must not silently discard several seconds of a dense Boss phase.
// Keep this function self-contained: Puppeteer serializes it into the page.
export function installBrowserProfileRecorder({intervalMs=1000,capacity=512,cpuThrottle=1}={}) {
  if(!Number.isInteger(intervalMs)||intervalMs<250||intervalMs>60000||
     !Number.isInteger(capacity)||capacity<1||capacity>4096||
     !Number.isFinite(cpuThrottle)||cpuThrottle<1||cpuThrottle>12)
    throw Error('Invalid browser profile recorder configuration');
  globalThis.__ph3ProfileRecorder?.stop();
  const rows=[];
  let produced=0,dropped=0,previousTime=null,maxIntervalMs=0,stopped=false;
  const record=()=>{
    if(stopped)return;
    const {messages,...diagnostics}=globalThis.ph3Diagnostics;
    const now=performance.now(),elapsed=previousTime===null?0:now-previousTime;
    previousTime=now;maxIntervalMs=Math.max(maxIntervalMs,elapsed);
    // Diagnostic objects are mutable. Retaining their references would turn
    // old samples into copies of the newest frame when the host reads later.
    const row=JSON.parse(JSON.stringify({time:now,...diagnostics,
      sampleIndex:produced++,sampleIntervalMs:elapsed,cpuThrottle,
      wasmBytes:globalThis.ph3Runtime.HEAPU8?.byteLength||0,
      jsUsedBytes:performance.memory?.usedJSHeapSize||0,
      jsTotalBytes:performance.memory?.totalJSHeapSize||0,
      contextLosses:globalThis.__hsobContextLosses||0,
      graphicsMemory:globalThis.__ph3GLMemory?.snapshot()||null}));
    if(rows.length===capacity){rows.shift();++dropped;}
    rows.push(row);
  };
  const timer=setInterval(record,intervalMs);
  globalThis.__ph3ProfileRecorder={
    setCpuThrottle(rate){
      if(!Number.isFinite(rate)||rate<1||rate>12)throw Error('Invalid CPU throttle');
      cpuThrottle=rate;
    },
    drain(){return {rows:rows.splice(0),produced,dropped,maxIntervalMs,intervalMs,capacity};},
    stop(){if(!stopped){stopped=true;clearInterval(timer);}},
  };
  record();
}
