import assert from 'node:assert/strict';
import vm from 'node:vm';
import {installBrowserProfileRecorder} from '../tools/lib/browser-profile-recorder.mjs';

function fixture(capacity=32){
  let now=0,timer,clears=0;
  const diagnostics={phase:'running',messages:['do not retain logs'],stage:{index:1,frame:100},
    performance:{logicHz:60,frameMsP95:8},errors:[]};
  const context={ph3Diagnostics:diagnostics,ph3Runtime:{HEAPU8:new Uint8Array(128)},
    performance:{now:()=>now,memory:{usedJSHeapSize:10,totalJSHeapSize:20}},
    setInterval(fn){timer=fn;return 1;},clearInterval(){++clears;}};
  vm.runInNewContext(`(${installBrowserProfileRecorder})({capacity:${capacity}})`,context);
  return {context,diagnostics,get recorder(){return context.__ph3ProfileRecorder;},
    get clears(){return clears;},tick(ms=1000){now+=ms;diagnostics.stage.frame+=60;timer();}};
}

{
  const f=fixture();f.tick();f.diagnostics.stage.frame=999;
  f.diagnostics.performance.frameMsP95=77;
  const first=f.recorder.drain();
  assert.deepEqual(Array.from(first.rows,r=>r.stage.frame),[100,160]);
  assert.deepEqual(Array.from(first.rows,r=>r.performance.frameMsP95),[8,8]);
  assert.equal('messages' in first.rows[0],false);
  // Fourteen seconds with no host/CDP reads still retain fourteen distinct
  // observations, instead of one current snapshot with the middle missing.
  for(let i=0;i<14;i++)f.tick();
  const delayed=f.recorder.drain();
  assert.equal(delayed.rows.length,14);assert.equal(delayed.dropped,0);
  assert.equal(delayed.maxIntervalMs,1000);
  assert.deepEqual(Array.from(delayed.rows,r=>r.sampleIndex),Array.from({length:14},(_,i)=>i+2));
  f.recorder.setCpuThrottle(4);f.tick(2400);
  const slow=f.recorder.drain();
  assert.equal(slow.rows[0].cpuThrottle,4);assert.equal(slow.maxIntervalMs,2400);
  assert.equal(slow.rows[0].sampleIntervalMs,2400);
  f.recorder.stop();f.recorder.stop();f.tick();
  assert.equal(f.clears,1);assert.equal(f.recorder.drain().rows.length,0);
}
{
  const f=fixture(4);
  for(let i=0;i<10;i++)f.tick();
  const overflow=f.recorder.drain();
  assert.equal(overflow.rows.length,4);assert.equal(overflow.produced,11);
  assert.equal(overflow.dropped,7);
  assert.deepEqual(Array.from(overflow.rows,r=>r.sampleIndex),[7,8,9,10]);
  f.recorder.stop();
}
{
  let now=0,intervals=0;
  const d={messages:[],stage:{index:1,frame:10},performance:{timingVersion:3,windowHook:true,sequence:1}};
  const context={ph3Diagnostics:d,ph3Runtime:{},performance:{now:()=>now},
    setInterval(){++intervals;},clearInterval(){}};
  vm.runInNewContext(`(${installBrowserProfileRecorder})()`,context);
  const recorder=context.__ph3ProfileRecorder;
  assert.equal(intervals,0);assert.equal(recorder.drain().rows.length,0);
  for(let sequence=2;sequence<=20;++sequence){
    now+=1040;d.performance.sequence=sequence;
    recorder.recordPerformanceWindow();
  }
  const saved=recorder.drain();
  assert.equal(saved.method,'runtime-window-v1');
  assert.deepEqual(Array.from(saved.rows,r=>r.performance.sequence),Array.from({length:19},(_,i)=>i+2));
  recorder.stop();now+=5000;recorder.recordPerformanceWindow();
  assert.equal(recorder.drain().rows.length,0);
  // Fast verifier playback disables timing, but still needs timer-driven
  // state/error evidence. A stale menu timing object must not disable it.
  d.observePerformance=false;
  vm.runInNewContext(`(${installBrowserProfileRecorder})()`,context);
  assert.equal(intervals,1);
  assert.equal(context.__ph3ProfileRecorder.drain().method,'browser-timer-v1');
}
console.log('PASS browser-side sampling, immutable history, delayed host reads, gap reporting and bounded overflow');
