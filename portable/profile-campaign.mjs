// Original Replay/stage-selection menus only. A timed window or direct Stage 6
// run is NOT full-campaign/golden acceptance. Original game files stay read-only.
import {createRequire} from 'node:module';
import {appendFile, mkdir, readFile, writeFile} from 'node:fs/promises';
import {existsSync} from 'node:fs';
import {resolve, dirname} from 'node:path';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';
import {spawnSync} from 'node:child_process';
import {installGLMemoryObserver} from './observe-gl-memory.mjs';
import {installBrowserProfileRecorder} from './browser-profile-recorder.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const launcherCandidates=[resolve(root,'../eagler-touhou-hsob'),resolve(root,'../../eagler-touhou')];
const launcher = resolve(process.env.HSOB_LAUNCHER || launcherCandidates.find(p=>existsSync(resolve(p,'node_modules/puppeteer-core'))) || launcherCandidates[0]);
const require = createRequire(resolve(launcher, 'package.json'));
const puppeteer = require('puppeteer-core');
const flags = new Map(process.argv.slice(2).map(arg => {
  const [key, ...value] = arg.replace(/^--/, '').split('=');
  return [key, value.length ? value.join('=') : true];
}));
const stage = Number(flags.get('stage') || 1);
const seconds = Number(flags.get('seconds') || 240);
const untilFrame = Number(flags.get('until-frame') || 0);
const throttle = Number(flags.get('throttle') || 1);
const throttleFromFrame = Number(flags.get('throttle-from-frame') || 0);
const cpuFromFrame = Number(flags.get('cpu-from-frame') || 0);
const vmFromFrame = Number(flags.get('vm-from-frame') || 0);
const fast = Number(flags.get('fast') || 0);
const affinity = Number(flags.get('affinity') || 0);
if(!Number.isInteger(affinity)||affinity<0||affinity>0xffffff||(affinity&&process.platform!=='win32'))
  throw Error('CPU affinity expects a Windows logical-processor mask up to 0xffffff');
const traceStages = Number(flags.get('trace-stages') || 1);
const captureFrames = String(flags.get('capture-frames') || '').split(',').filter(Boolean).map(value => {
  const parts=value.split(':').map(Number);
  return {stage:parts.length===2?parts[0]:stage,frame:parts.at(-1)};
});
if (!Number.isInteger(stage) || stage < 1 || stage > 6 ||
    !Number.isFinite(seconds) || seconds < 1 || seconds > 3600 ||
    !Number.isInteger(untilFrame) || untilFrame < 0 ||
    !Number.isInteger(throttleFromFrame) || throttleFromFrame < 0 ||
    !Number.isInteger(cpuFromFrame) || cpuFromFrame < 0 || (cpuFromFrame > 0 && !flags.has('cpu')) ||
    !Number.isInteger(vmFromFrame) || vmFromFrame < 0 || (vmFromFrame>0 && !flags.has('vm-opcodes')) ||
    !Number.isFinite(throttle) || throttle < 1 || throttle > 12)
  throw Error('Invalid stage, seconds, frame, or CPU throttle');
if(!Number.isInteger(fast)||fast<0||fast>32||!Number.isInteger(traceStages)||traceStages<1||traceStages>6||
   captureFrames.length>24||captureFrames.some(p=>!Number.isInteger(p.stage)||p.stage<1||p.stage>6||!Number.isInteger(p.frame)||p.frame<1))
  throw Error('Invalid diagnostic replay or capture settings');
if((fast||flags.has('trace')||captureFrames.length)&&(flags.has('cpu')||throttle!==1))
  throw Error('Diagnostic fast/trace/capture modes are not performance measurements');
const out = resolve(root, String(flags.get('out') || `build/campaign-stage${stage}`));
const fixture = resolve(String(flags.get('fixture') ||
  resolve(root, '../../fangame/games/th_hsob_ver1.12/dnh/script/th_hsob/replay/Main_replay01.dat')));
const fixtureBytes = await readFile(fixture);
const url = String(flags.get('url') || 'http://127.0.0.1:8855/');
const managed = flags.has('managed');
const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));
const report = {url, stage, seconds, untilFrame, throttle, managed, cpuProfiling: flags.has('cpu'),
  cpuAffinity:affinity?{mask:affinity,processes:[]}:null,
  samplingMethod:'browser-timer-v1',
  throttleFromFrame, throttleAppliedAt:null, cpuFromFrame, cpuStartedAt:null,
  vmInstructionProfiling:flags.has('vm-opcodes'),vmFromFrame,vmStartedAt:null,
  fast, trace:flags.has('trace'), traceStages, requestedCaptures:captureFrames, captures:[],
  glMemoryObserver: flags.has('memory'),
  fixtureSha256: createHash('sha256').update(fixtureBytes).digest('hex'),
  scope: 'original-menu replay window; not full-campaign or golden acceptance',
  outcome: 'not-started', errors: [], consoleErrors: [], checkpoints: [], samples: 0};
await mkdir(out, {recursive: true});
await writeFile(resolve(out, 'samples.jsonl'), '');
await writeFile(resolve(out, 'console.jsonl'), '');
const save = () => writeFile(resolve(out, 'report.json'), JSON.stringify(report, null, 2) + '\n');
let browser;
const browserOptions={
  executablePath: process.env.HSOB_CHROME || 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: !flags.has('headed'),
  args: ['--no-proxy-server', '--autoplay-policy=no-user-gesture-required', '--enable-precise-memory-info'],
};
let page, runtime, cdp, profileActive = false, drainEvidence=async()=>{}, throttleApplied=false;
let samplesActive=false,drainSamples=async()=>null;
const setBrowserAffinity=()=>{
  if(!affinity)return;
  // Only this probe's isolated Chrome process tree. Never alter another
  // browser or background task to manufacture an idle-machine benchmark.
  const rootPid=browser.process().pid;
  const script=`$ph3Nodes=Get-CimInstance Win32_Process -Filter "Name = 'chrome.exe'";
    $ph3Branch=@{${rootPid}=$true};
    do { $ph3Count=$ph3Branch.Count; foreach($ph3Node in $ph3Nodes) {
      if($ph3Branch.ContainsKey([int]$ph3Node.ParentProcessId)) { $ph3Branch[[int]$ph3Node.ProcessId]=$true }
    }} while($ph3Branch.Count -ne $ph3Count);
    $ph3Result=@();foreach($ph3Id in $ph3Branch.Keys){
      $ph3Process=[System.Diagnostics.Process]::GetProcessById($ph3Id);
      $ph3Process.ProcessorAffinity=[IntPtr]${affinity};
      $ph3Result+=@{pid=$ph3Id;mask=$ph3Process.ProcessorAffinity.ToInt64()}
    };ConvertTo-Json -InputObject $ph3Result -Compress`;
  const result=spawnSync('powershell.exe',['-NoProfile','-NonInteractive','-Command',script],
    {encoding:'utf8',windowsHide:true,timeout:20000});
  if(result.error||result.status!==0)throw Error(result.error?.message||result.stderr||'CPU affinity setup failed');
  report.cpuAffinity.processes=JSON.parse(result.stdout.trim());
  if(!report.cpuAffinity.processes.length||report.cpuAffinity.processes.some(p=>p.mask!==affinity))
    throw Error('CPU affinity was not applied to every owned browser process');
};
try {
  browser=await puppeteer.launch(browserOptions);
  report.browserVersion=await browser.version();
  page = await browser.newPage();
  setBrowserAffinity();
  await page.setViewport({width: 1100, height: 1000});
  await page.setBypassServiceWorker(true);
  if (flags.has('memory')) await page.evaluateOnNewDocument(installGLMemoryObserver);
  page.on('pageerror', error => {report.errors.push(String(error));});
  page.on('error', error => {report.errors.push(String(error)); report.outcome = 'browser-crash'; void save();});
  page.on('console', event => {
    const entry = {type: event.type(), text: event.text()};
    if (entry.type === 'error') report.consoleErrors.push(entry.text);
    void appendFile(resolve(out, 'console.jsonl'), JSON.stringify(entry) + '\n').catch(() => {});
  });
  await page.evaluateOnNewDocument(() => {
    window.__hsobFirstFrame = false;
    window.__hsobContextLosses = 0;
    addEventListener('message', event => {
      if (event.data?.event === 'first-frame' && event.data.game === 'hsob') window.__hsobFirstFrame = true;
    });
    addEventListener('webglcontextlost', () => {++window.__hsobContextLosses;}, true);
  });
  await page.goto(url, {waitUntil: 'domcontentloaded', timeout: 60000});
  if (managed) {
    await page.waitForFunction(() => window.__eaglerBoot?.done, {timeout: 60000});
    await page.evaluate(() => {
      const card = document.querySelector('[data-game="hsob"]');
      card.click(); if (!card.classList.contains('selected')) card.click();
      document.querySelector('#launch').click();
    });
    // Older pages can open the asynchronous first-use notice after boot.done
    // or in response to launch. Its close-button handler acknowledges it.
    await page.waitForFunction(() => document.querySelector('#firstUseNoticeDialog')?.open ||
      !document.querySelector('#gameDataImportWindow')?.hidden, {timeout: 30000});
    report.managedUi=await page.evaluate(()=>Object.fromEntries(
      ['firstUseNoticeDialog','firstUseNoticeClose','gameDataImportWindow'].map(id=>{
        const e=document.getElementById(id),r=e?.getBoundingClientRect();
        return[id,e?{open:e.open,hidden:e.hidden,display:getComputedStyle(e).display,width:r.width,height:r.height}:null];
      })));
    if(await page.evaluate(()=>Boolean(document.querySelector('#firstUseNoticeDialog')?.open) &&
      Boolean(document.querySelector('#gameDataImportWindow')?.hidden))){
      // Avoid scrollIntoView on this sliding native dialog: the pending
      // launch can replace it while Puppeteer computes its clickable point.
      await page.evaluate(()=>document.querySelector('#firstUseNoticeClose').click());
      await page.waitForFunction(()=>!document.querySelector('#firstUseNoticeDialog')?.open);
      // The pending launch resumes after acknowledgement and opens import.
    }
    await page.waitForFunction(() => !document.querySelector('#gameDataImportWindow').hidden, {timeout: 30000});
    const zip = resolve(String(flags.get('offline-zip') ||
      resolve(root, '../../dist/offline-packages/hsob-1.12-ja-20260930.zip')));
    await (await page.$('#gameDataImportInput')).uploadFile(zip);
    await page.waitForFunction(() => window.__hsobFirstFrame, {timeout: 180000});
    runtime = page.frames().find(frame => frame.url().includes('/runtime/hsob/'));
    if (!runtime) throw Error('Managed HSoB frame not found');
  } else {
    runtime = page.mainFrame();
    await page.click('#start');
    await runtime.waitForFunction(() => ['ready', 'error'].includes(ph3Diagnostics.phase), {timeout: 180000});
    if (await runtime.evaluate(() => ph3Diagnostics.phase === 'error')) throw Error('Resource preparation failed');
  }
  report.runtimeUrl = runtime.url();
  report.build = await runtime.evaluate(async () => {
    const response = await fetch('build.json', {cache: 'no-store'});
    return response.ok ? await response.json() : {unavailable: true, status: response.status};
  });
  if((flags.has('trace')||captureFrames.length)&&report.build.variant!=='verify')throw Error('Trace/capture requires a verifier build');
  if(fast&&!['verify','boundary'].includes(report.build.variant))throw Error('Fast replay requires a diagnostic build');
  if(fast||flags.has('trace')||captureFrames.length)report.scope='diagnostic original-menu replay; not performance data';
  const vmInstrumented=report.build.flags?.some(f=>f==='-DPH3_VM_OPCODE_PROFILE=1');
  if(vmInstrumented!==flags.has('vm-opcodes'))throw Error('VM opcode probe flag must match the instrumented build');
  if(vmInstrumented)report.scope='VM instruction/native-call attribution, not ordinary performance';
  if(flags.has('trace'))await writeFile(resolve(out,'trace.jsonl'),'');
  await runtime.evaluate(config=>{
    ph3Runtime.wooTraceEnabled=config.trace;
    ph3Runtime.wooTraceStageCount=config.traceStages;
    ph3Runtime.wooReplayFastSteps=config.fast;
    ph3Runtime.wooCaptureFrames=config.captureFrames;
  },{trace:flags.has('trace'),traceStages,fast,captureFrames});
  drainEvidence=async()=>{
    const evidence=await runtime.evaluate(flush=>{
      if(flush&&ph3Runtime._woo_trace_flush)ph3Runtime._woo_trace_flush();
      const chunks=ph3Runtime.wooTraceChunks||[],captures=ph3Runtime.wooCaptures||[];
      ph3Runtime.wooTraceChunks=[];ph3Runtime.wooTraceBytes=0;ph3Runtime.wooCaptures=[];
      return {chunks,captures};
    },flags.has('trace'));
    if(evidence.chunks.length)await appendFile(resolve(out,'trace.jsonl'),evidence.chunks.join(''));
    for(const capture of evidence.captures){
      const {png,...metadata}=capture,name=`stage${capture.stage}-frame${capture.frame}.png`;
      await writeFile(resolve(out,name),Buffer.from(png.split(',')[1],'base64'));
      report.captures.push({...metadata,file:name});
    }
  };
  await runtime.evaluate(({bytes,diagnostic}) => {
    ph3Diagnostics.observeStage = true;
    ph3Diagnostics.observePerformance = !diagnostic;
    const fs = ph3Runtime.FS, dir = '/game/script/th_hsob/replay';
    fs.mkdirTree(dir);
    for (const name of fs.readdir(dir)) if (/^Main_replay\d+\.dat$/.test(name)) fs.unlink(dir + '/' + name);
    fs.writeFile(dir + '/Main_replay01.dat', new Uint8Array(bytes));
  }, {bytes:Array.from(fixtureBytes),diagnostic:!!(fast||flags.has('trace')||captureFrames.length)});
  if (!managed) await page.click('#start');
  await runtime.waitForFunction(() => ph3Diagnostics.messages.some(s => s.includes('title00b3.png')), {timeout: 180000});
  setBrowserAffinity();
  const press = async (key, settle = 500) => {
    await runtime.focus('#canvas');
    await page.keyboard.down(key); await sleep(120); await page.keyboard.up(key); await sleep(settle);
  };
  const snapshot = () => runtime.evaluate(cpuThrottle => {
    const {messages, ...d} = ph3Diagnostics;
    return {time: performance.now(), ...d, cpuThrottle, wasmBytes: ph3Runtime.HEAPU8?.byteLength || 0,
      jsUsedBytes: performance.memory?.usedJSHeapSize || 0,
      jsTotalBytes: performance.memory?.totalJSHeapSize || 0,
      contextLosses: window.__hsobContextLosses || 0,
      graphicsMemory: globalThis.__ph3GLMemory?.snapshot() || null};
  },throttleApplied?throttle:1);
  const checkpoint = async label => {
    await drainEvidence();
    const state = await snapshot(); report.checkpoints.push({label, state});
    await page.screenshot({path: resolve(out, label + '.png')});
    await save(); return state;
  };
  await sleep(2500);
  await runtime.evaluate(diagnostic=>{ph3Diagnostics.observePerformance=!diagnostic;},
    !!(fast||flags.has('trace')||captureFrames.length));
  await checkpoint('00-title');
  // The original title skips locked Extra/Practice entries. Four Down inputs
  // therefore do NOT always select Replay. Cancel selects Quit (index 9);
  // five Up inputs traverse the always-available tail back to Replay (index 4).
  await press('x');
  for (let i = 0; i < 5; ++i) await press('ArrowUp');
  await checkpoint('00-replay-highlight');
  await press('z', 1800); await press('z', 1200);
  // Original authored stage-selection submenu: five Down inputs select Stage 6.
  for (let i = 1; i < stage; ++i) await press('ArrowDown');
  await checkpoint('01-stage-selection'); await press('z', 2000);
  await runtime.waitForFunction(index => ph3Diagnostics.phase === 'error' ||
    (ph3Diagnostics.stage?.replay && ph3Diagnostics.stage.index === index), {timeout: 120000}, stage);
  if (await runtime.evaluate(() => ph3Diagnostics.phase === 'error')) throw Error('Replay entry failed');
  report.start = await checkpoint('02-playing');
  if(vmInstrumented&&vmFromFrame===0){
    await runtime.evaluate(()=>ph3Runtime._hsob_vm_profile_reset());
    report.vmStartedAt={stage:report.start.stage?.index,frame:report.start.stage?.frame};
  }
  report.renderer = await runtime.evaluate(() => {
    const gl = document.querySelector('#canvas').getContext('webgl2');
    const ext = gl.getExtension('WEBGL_debug_renderer_info');
    return gl.getParameter(ext ? ext.UNMASKED_RENDERER_WEBGL : gl.RENDERER);
  });
  cdp = await page.createCDPSession();
  if (throttle !== 1 && throttleFromFrame===0) {
    await cdp.send('Emulation.setCPUThrottlingRate', {rate: throttle});throttleApplied=true;
    report.throttleAppliedAt={stage:report.start.stage?.index,frame:report.start.stage?.frame,time:report.start.time};
  }
  if (flags.has('cpu')) {
    await cdp.send('Profiler.enable'); await cdp.send('Profiler.setSamplingInterval', {interval: 2000});
    if(cpuFromFrame===0){
      await cdp.send('Profiler.start'); profileActive = true;
      report.cpuStartedAt={stage:report.start.stage?.index,frame:report.start.stage?.frame,time:report.start.time};
    }
  }
  await runtime.evaluate(installBrowserProfileRecorder,{cpuThrottle:throttleApplied?throttle:1});
  samplesActive=true;
  drainSamples=async()=>{
    const {rows,...sampling}=await runtime.evaluate(()=>globalThis.__ph3ProfileRecorder.drain());
    report.sampling=sampling;
    report.samplingMethod=sampling.method||report.samplingMethod;
    if(rows.length){
      await appendFile(resolve(out,'samples.jsonl'),rows.map(row=>JSON.stringify(row)+'\n').join(''));
      report.samples+=rows.length;report.last=rows.at(-1);
      const reached=untilFrame>0&&rows.find(row=>row.stage?.index===stage&&row.stage.frame>=untilFrame);
      if(reached&&!report.targetFrameObserved)report.targetFrameObserved={stage,frame:reached.stage.frame,time:reached.time};
    }
    if(sampling.dropped)throw Error(`Browser sample history overflowed: ${sampling.dropped} samples`);
    return rows.at(-1)||report.last||await snapshot();
  };
  report.outcome = 'running'; await save();
  const began = Date.now(); let lastFrame = -1, lastChange = Date.now(),lastReported=0;
  while (Date.now() - began < seconds * 1000) {
    await sleep(1000);
    if(flags.has('trace')||captureFrames.length)await drainEvidence();
    const state=await drainSamples();
    if(vmInstrumented&&!report.vmStartedAt&&state.stage?.index===stage&&state.stage.frame>=vmFromFrame){
      await runtime.evaluate(()=>ph3Runtime._hsob_vm_profile_reset());
      report.vmStartedAt={stage,frame:state.stage.frame};await save();
    }
    if(flags.has('cpu')&&!profileActive&&state.stage?.index===stage&&state.stage.frame>=cpuFromFrame){
      await cdp.send('Profiler.start');profileActive=true;
      report.cpuStartedAt={stage:state.stage.index,frame:state.stage.frame,time:state.time};
      await save();
    }
    if(throttle!==1&&!throttleApplied&&state.stage?.index===stage&&state.stage.frame>=throttleFromFrame){
      await cdp.send('Emulation.setCPUThrottlingRate',{rate:throttle});throttleApplied=true;
      await runtime.evaluate(rate=>globalThis.__ph3ProfileRecorder.setCpuThrottle(rate),throttle);
      report.throttleAppliedAt={stage:state.stage.index,frame:state.stage.frame,time:state.time};
      await save();
    }
    if (report.samples-lastReported >= 15) {
      lastReported=report.samples;
      console.log(JSON.stringify({samples: report.samples, stage: state.stage, performance: state.performance,
        wasmMiB: state.wasmBytes / 1048576, jsMiB: state.jsUsedBytes / 1048576}));
      await save();
    }
    if (state.phase === 'error' || state.contextLosses) throw Error('Runtime/context failure: ' + JSON.stringify(state.errors));
    const ends=state.replayBoundaries?.filter(p=>p.kind==='end')||[];
    if(flags.has('trace')&&ends.length>=traceStages){
      report.stageResults=ends.map(p=>({...p,recordedFrameMatches:p.frame===p.expectedFrame,recordedScoreMatches:p.score===p.expectedScore}));
      report.outcome='requested-stage-boundaries-observed';break;
    }
    if (state.stage?.frame !== lastFrame) {lastFrame = state.stage?.frame; lastChange = Date.now();}
    if (untilFrame > 0 && (report.targetFrameObserved || (state.stage?.index === stage && state.stage.frame >= untilFrame))) {
      report.outcome = 'requested-frame-reached'; break;
    }
    if (state.stage?.index > stage && flags.has('one-stage')) {report.outcome = 'next-stage-observed'; break;}
    if (Date.now() - lastChange > 20000) {report.outcome = 'simulation-stopped-or-replay-ended'; break;}
  }
  if (report.outcome === 'running') report.outcome = 'time-window-complete';
  report.elapsedSeconds = (Date.now() - began) / 1000;
  if(vmInstrumented){
    if(!report.vmStartedAt)throw Error('VM instruction profile start frame was not reached');
    await runtime.evaluate(()=>ph3Runtime._hsob_vm_profile_dump());
    report.vmProfile=await runtime.evaluate(()=>ph3Diagnostics.vmProfile);
  }
  report.end = await checkpoint('03-end');
} catch (error) {
  report.failure = String(error); report.failureStack=error.stack;
  if (report.outcome !== 'browser-crash') report.outcome = 'failed';
  try {await page.screenshot({path: resolve(out, 'failure.png')});} catch {}
} finally {
  if(samplesActive)try{
    await runtime.evaluate(()=>globalThis.__ph3ProfileRecorder.stop());await drainSamples();
  }catch(error){report.samplingError=String(error);}
  try{await drainEvidence();}catch(error){report.evidenceError=String(error);}
  if (profileActive) try {
    const {profile} = await cdp.send('Profiler.stop');
    await writeFile(resolve(out, 'cpu.cpuprofile'), JSON.stringify(profile));
  } catch (error) {report.profileError = String(error);}
  else if(flags.has('cpu')) report.profileError='Requested CPU profile start frame was not reached';
  await save(); if(browser)await browser.close();
}
console.log(JSON.stringify({out, outcome: report.outcome, failure: report.failure, samples: report.samples}));
if (report.failure || report.errors.length || report.last?.errors?.length ||
    report.samplingError || report.evidenceError || report.profileError) process.exitCode = 1;
