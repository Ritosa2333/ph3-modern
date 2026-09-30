// Platform shell for the eagler-touhou/1 contract. Danmakufu owns gameplay;
// this file owns only Host protocol, managed bytes, persistence and input IO.
import createWoo from './ph3-open.mjs';
import {ensureFontAlias,installResources,mountManagedData,normalizeOptions,touchSnapshot,isSupersededRuntimeError} from './eagler-host.mjs';

const protocol='eagler-touhou/1',game='hsob';
const RUNTIME_EPOCH_QUERY_PARAMETER='runtimeEpoch';
const query=new URLSearchParams(location.search),managed=query.get('managedData')==='1';
const epoch=Number(query.get(RUNTIME_EPOCH_QUERY_PARAMETER)),validEpoch=Number.isSafeInteger(epoch)&&epoch>0;
const canvas=document.querySelector('#canvas'),start=document.querySelector('#start'),errorBox=document.querySelector('#error');
document.documentElement.dataset.standalone=managed?'0':'1';
const diagnostics=window.ph3Diagnostics={phase:'idle',errors:[],messages:[],loadedBytes:0,loadedFiles:0,observeStage:true,observePerformance:true};
const emit=(event,fields={})=>{if(managed&&validEpoch)parent.postMessage({protocol,game,epoch,event,...fields},location.origin);};
const log=text=>{const value=String(text);diagnostics.messages.push(value);if(diagnostics.messages.length>1000)diagnostics.messages.shift();};
let Module=null,configured=false,launched=false,ready=false,firstFrame=false,reportedError=false;
let options=normalizeOptions(),musicMode='ogg',syncTail=Promise.resolve(),healthTimer=null;
const saveRoot='/savehsob';

function fail(reason){
 const message=reason?.stack||reason?.message||String(reason);diagnostics.errors.push(message);diagnostics.phase='error';errorBox.textContent=message;
 if(!reportedError){reportedError=true;emit('error',{message,error:message});}console.error(reason);
}
function exists(path){return !!Module?.FS?.analyzePath(path).exists;}
function sync(populate=false){
 const next=syncTail.then(()=>new Promise((resolve,reject)=>Module.FS.syncfs(populate,error=>error?reject(error):resolve())));
 syncTail=next.catch(()=>{});return next;
}
function seedDirectory(source,destination){
 if(!exists(source))return;Module.FS.mkdirTree(destination);
 for(const name of Module.FS.readdir(source)){
  if(name==='.'||name==='..')continue;const from=source+'/'+name,to=destination+'/'+name,s=Module.FS.stat(from);
  if(Module.FS.isDir(s.mode))seedDirectory(from,to);else if(!exists(to))Module.FS.writeFile(to,Module.FS.readFile(from));
 }
}
function moveAside(path,label){
 if(!exists(path))return;const backup='/initial-hsob-'+label;try{if(exists(backup))return;}catch{}
 Module.FS.rename(path,backup);
}
async function prepareStorage(seedOriginal=false){
 Module.FS.mkdirTree(saveRoot);Module.FS.mount(Module.IDBFS,{autoPersist:true},saveRoot);await sync(true);
 Module.FS.mkdirTree(saveRoot+'/replay');
 const data='/game/script/th_hsob/data',replay='/game/script/th_hsob/replay',config='/game/config.dat';
 if(seedOriginal){seedDirectory(data,saveRoot);seedDirectory(replay,saveRoot+'/replay');if(exists(config)&&!exists(saveRoot+'/config.dat'))Module.FS.writeFile(saveRoot+'/config.dat',Module.FS.readFile(config));}
 moveAside(data,'data');moveAside(replay,'replay');moveAside(config,'config.dat');
 Module.FS.mkdirTree('/game/script/th_hsob');Module.FS.symlink(saveRoot,data);Module.FS.symlink(saveRoot+'/replay',replay);Module.FS.symlink(saveRoot+'/config.dat',config);
 if(seedOriginal)await sync(false);
}

function relativeSave(value){
 if(typeof value!=='string'||value.length<1||value.length>220)throw Error('Invalid save path');
 let path=value.replaceAll('\\','/').replace(/^\/+/, '');
 if(path.toLowerCase().startsWith('savehsob/'))path=path.slice('savehsob/'.length);
 if(/^config\.dat$/i.test(path)||/^Package_common_[A-Za-z0-9_-]+\.dat$/i.test(path)||/^replay\/Main_replay(?:\d{2}|ud[a-z0-9]{4})\.dat$/i.test(path))return path;
 throw Error('Invalid HSoB save path: '+path);
}
function listFiles(){
 const files=[];
 for(const dir of ['', '/replay']){
  const root=saveRoot+dir;if(!exists(root))continue;
  for(const name of Module.FS.readdir(root)){
   if(name==='.'||name==='..')continue;const path=(dir+'/'+name).replace(/^\//,'');
   try{relativeSave(path);}catch{continue;}const full=saveRoot+'/'+path,s=Module.FS.stat(full);
   if(Module.FS.isFile(s.mode))files.push({path,size:s.size});
  }
 }
 return files;
}

const DIK=Object.freeze({Escape:0x01,Enter:0x1c,ControlLeft:0x1d,ControlRight:0x9d,KeyR:0x13,ShiftLeft:0x2a,ShiftRight:0x36,KeyZ:0x2c,KeyX:0x2d,ArrowUp:0xc8,ArrowDown:0xd0,ArrowLeft:0xcb,ArrowRight:0xcd,Home:0xc7,Tab:0x0f,Backspace:0x0e});
function runtimeKeyboardCode(message){
 const code=String(message.code||'');if(code&&code!=='Unidentified')return code;
 const key=String(message.key||'').toLowerCase(),location=Number(message.location)||0;
 const byKey={z:'KeyZ',x:'KeyX',r:'KeyR',shift:location===2?'ShiftRight':'ShiftLeft',control:location===2?'ControlRight':'ControlLeft',escape:'Escape',esc:'Escape',arrowup:'ArrowUp',arrowdown:'ArrowDown',arrowleft:'ArrowLeft',arrowright:'ArrowRight',home:'Home',enter:'Enter',tab:'Tab',backspace:'Backspace'};
 return byKey[key]||'';
}
const keyboard=new Map(),touch=new Map();
function ccall(name,args=[],types=[]){return Module.ccall(name,null,types,args);}
function applyKey(dik){ccall('hsob_input_key',[dik,(keyboard.get(dik)||touch.get(dik))?1:0],['number','number']);}
function setSource(source,dik,down){if(down)source.set(dik,true);else source.delete(dik);applyKey(dik);}
function clearSource(source){const keys=[...source.keys()];source.clear();for(const key of keys)applyKey(key);}
function pulse(dik){ccall('hsob_input_pulse',[dik],['number']);}
let lastBomb=0,lastEscape=0,directId=null;
function cancelTouches(){
 clearSource(touch);directId=null;ccall('hsob_direct_touch',[0,0,0],['number','number','number']);
}
function applyTouchControls(message){
 const state=touchSnapshot(options,message);
 if(!options.touchEnabled){cancelTouches();lastBomb=state.bomb;lastEscape=state.escape;return;}
 setSource(touch,DIK.KeyZ,state.fire);setSource(touch,DIK.ShiftLeft,state.focus);
 if(options.touchMovementMode.startsWith('joystick')){
  const threshold=4096;setSource(touch,DIK.ArrowLeft,state.x<-threshold);setSource(touch,DIK.ArrowRight,state.x>threshold);
  setSource(touch,DIK.ArrowUp,state.y<-threshold);setSource(touch,DIK.ArrowDown,state.y>threshold);
 }else{
  for(const key of [DIK.ArrowLeft,DIK.ArrowRight,DIK.ArrowUp,DIK.ArrowDown])setSource(touch,key,false);
 }
 if(state.bomb!==lastBomb){lastBomb=state.bomb;pulse(DIK.KeyX);}if(state.escape!==lastEscape){lastEscape=state.escape;pulse(DIK.Escape);}
}
function directTouch(message){
 if(!options.touchEnabled||options.touchMovementMode.startsWith('joystick'))return;
 const type=message.type??message.action,id=Number(message.id),x=Number(message.x),y=Number(message.y);
 if(!Number.isSafeInteger(id))return;
 if(type==='up'||type==='cancel'){
  if(id===directId){directId=null;ccall('hsob_direct_touch',[0,0,0],['number','number','number']);}return;
 }
 if(type!=='down'&&type!=='move'||!Number.isFinite(x)||!Number.isFinite(y))return;
 if(directId===null)directId=id;if(directId!==id)return;
 const rect=canvas.getBoundingClientRect();if(rect.width<=0||rect.height<=0)return;
 const clientX=x*innerWidth,clientY=y*innerHeight;
 const logicalX=(clientX-rect.left)*640/rect.width,logicalY=(clientY-rect.top)*480/rect.height;
 ccall('hsob_direct_touch',[1,logicalX,logicalY],['number','number','number']);
}

async function installLocalManifest(){
 const localManifestUrl=new URL(['local','manifest.json'].join('-'),location.origin+'/');
 const response=await fetch(localManifestUrl,{cache:'no-store'});if(!response.ok)throw Error('本地资源清单不可用');
 const manifest=await response.json(),total=manifest.files.reduce((sum,file)=>sum+file.bytes,0);let cursor=0;
 const install=async file=>{
  for(let attempt=0;attempt<3;++attempt){
   const controller=new AbortController(),timeout=setTimeout(()=>controller.abort(),file.bytes>32*1048576?120000:20000);
   try{
    const res=await fetch(file.url,{cache:'no-store',signal:controller.signal});if(!res.ok)throw Error('HTTP '+res.status);
    const bytes=new Uint8Array(await res.arrayBuffer());if(bytes.length!==file.bytes)throw Error('资源大小不一致');
    Module.FS.mkdirTree(file.path.slice(0,file.path.lastIndexOf('/'))||'/');Module.FS.writeFile(file.path,bytes,{canOwn:true});diagnostics.loadedBytes+=bytes.length;diagnostics.loadedFiles++;return;
   }catch(reason){if(attempt===2)throw Error('资源读取失败：'+file.path+' — '+reason,{cause:reason});await new Promise(resolve=>setTimeout(resolve,300*(attempt+1)));}
   finally{clearTimeout(timeout);}
  }
 };
 await Promise.all(Array.from({length:4},async()=>{while(cursor<manifest.files.length)await install(manifest.files[cursor++]);}));
 if(diagnostics.loadedBytes!==total)throw Error('本地资源加载不完整');
}

async function installConfiguredResources(message){
 for(const group of [message.sharedResources,message.runtimeResources,message.resources])if(group)await installResources(Module,group,{emit});
}
async function configure(message){
 if(launched)throw Error('Cannot configure a running game');
 if(!['ogg','none'].includes(message.music))throw Error('HSoB supports OGG or no music');
 options=normalizeOptions(message.options);musicMode=message.music;Module.hsobLimitPresentationTo60=options.limitPresentationTo60;Module.hsobAlwaysHitbox=options.alwaysHitbox;Module.hsobOggDecodeMode=options.oggDecodeMode;
 await installConfiguredResources(message);configured=true;return {};
}
function launch(){
 if(launched)return;if(managed&&!configured)throw Error('Runtime must be configured before launch');
 if(exists('/msgothic.ttc'))ensureFontAlias(Module);else if(!exists('/game/msgothic.ttc'))throw Error('Japanese font is unavailable');
 if(musicMode==='none'&&exists('/game/script/th_hsob/music/music.dat'))Module.FS.unlink('/game/script/th_hsob/music/music.dat');
 Module.FS.chdir('/game');diagnostics.observeStage=true;diagnostics.observePerformance=true;diagnostics.phase='running';
 launched=true;firstFrame=false;reportedError=false;canvas.focus({preventScroll:true});Module.callMain([]);
 emit('runtime-info',{renderer:'SDL3 / WebGL2 / Danmakufu ph3',architecture:'eagler-touhou/1',version:'HSoB 1.12 / ph3-open-web'});
 start.disabled=true;start.textContent='游戏运行中';start.style.display=managed?'none':'';
}
async function command(message){
 switch(message.command){
 case 'configure':return configure(message);
 case 'resources':if(launched)throw Error('Runtime resources cannot change after launch');await installResources(Module,message.resources,{emit});return {};
 case 'keyboard':{const dik=DIK[runtimeKeyboardCode(message)];if(dik!==undefined)setSource(keyboard,dik,!!message.down);return {};}
 case 'keyboard-clear':clearSource(keyboard);return {};
 case 'touch-controls':applyTouchControls(message);return {};
 case 'touch-cancel':cancelTouches();return {};
 case 'direct-touch':directTouch(message);return {};
 case 'launch':launch();return {};
 case 'sync':await sync(false);return {};
 case 'list':return {files:listFiles()};
 case 'read':{const path=relativeSave(message.path);return {bytes:Array.from(Module.FS.readFile(saveRoot+'/'+path))};}
 case 'write':{if(!Array.isArray(message.bytes)||message.bytes.length>16*1024*1024||message.bytes.some(byte=>!Number.isInteger(byte)||byte<0||byte>255))throw Error('Invalid save bytes');const path=relativeSave(message.path),full=saveRoot+'/'+path;Module.FS.mkdirTree(full.slice(0,full.lastIndexOf('/')));Module.FS.writeFile(full,new Uint8Array(message.bytes));await sync(false);return {};}
 case 'remove':{const path=relativeSave(message.path);Module.FS.unlink(saveRoot+'/'+path);await sync(false);return {};}
 default:throw Error('Unsupported runtime command: '+message.command);
 }
}

let queue=Promise.resolve();
window.addEventListener('message',event=>{const m=event.data;if(!managed||!validEpoch||event.source!==parent||event.origin!==location.origin||m?.protocol!==protocol||m.game!==game||m.epoch!==epoch||typeof m.command!=='string')return;
 queue=queue.then(async()=>{if(await initialized===false)return;try{const result=await command(m);if(typeof m.request==='string')parent.postMessage({protocol,game,epoch,request:m.request,ok:true,...result},location.origin);}catch(reason){if(typeof m.request==='string')parent.postMessage({protocol,game,epoch,request:m.request,ok:false,error:String(reason)},location.origin);else fail(reason);}}).catch(fail);
});

function audioNode(){return Module?.SDL3?.audio_playback?.scriptProcessorNode;}
const detachedAudioNodes=new WeakSet();
function suspendAudio(){const node=audioNode();if(node&&!detachedAudioNodes.has(node))try{node.disconnect();detachedAudioNodes.add(node);}catch{}}
async function resumeAudio(){const context=Module?.SDL3?.audioContext;if(context&&context.state!=='running')try{await context.resume();}catch{}const node=audioNode();if(node&&detachedAudioNodes.has(node)&&context)try{node.connect(context.destination);detachedAudioNodes.delete(node);}catch{}}
document.addEventListener('visibilitychange',()=>{if(!Module)return;clearSource(keyboard);cancelTouches();if(document.hidden){suspendAudio();if(ready)queue=queue.then(()=>sync(false)).catch(fail);}else void resumeAudio();});
window.addEventListener('blur',()=>{if(Module){clearSource(keyboard);cancelTouches();}});
window.addEventListener('pagehide',()=>{if(Module&&ready)void sync(false).catch(console.error);});
for(const event of ['pointerdown','keydown'])window.addEventListener(event,()=>void resumeAudio(),{capture:true});
canvas.addEventListener('contextmenu',event=>event.preventDefault());
canvas.addEventListener('pointerdown',()=>canvas.focus({preventScroll:true}));
canvas.addEventListener('webglcontextlost',event=>{event.preventDefault();fail('图形环境已失效，请退出后重新开始。');});

function startHealth(){
 if(healthTimer!==null)return;let lastPresented=Number(diagnostics.presentedFrames||0),lastTime=performance.now();
 healthTimer=setInterval(()=>{
  if(!launched)return;const now=performance.now(),presented=Number(diagnostics.presentedFrames||0),delta=presented-lastPresented,elapsed=now-lastTime;
  if(!firstFrame&&presented>0){firstFrame=true;emit('first-frame');}
  const perf=diagnostics.performance||{};emit('frame-health',{fps:Number(perf.displayFps)||(elapsed>0?delta*1000/elapsed:0),logicHz:Number(perf.logicHz)||0,frameMs:Number(perf.frameMsP95)||0,maxGapMs:Number(perf.frameMsMax)||0});
  emit('audio-health',{backend:'SDL3/WebAudio',activeVoices:Number(diagnostics.audioVoices)||0,peak:Number(diagnostics.audioPeak)||0,samples:Number(diagnostics.audioSamples)||0,robust:true});
  if(diagnostics.phase==='error'&&!reportedError)fail(diagnostics.errors.at(-1)||'Runtime error');lastPresented=presented;lastTime=now;
 },1000);
}

async function createRuntime(){
 let audioContext;try{audioContext=parent.__touhouAudioContext;}catch{}
 Module=await createWoo({canvas,noInitialRun:true,...(audioContext?{SDL3:{audioContext}}:{}),print:text=>{log(text);console.log('[HSoB]',text);},printErr:text=>{log(text);console.error('[HSoB]',text);},onAbort:reason=>fail(new Error('Wasm aborted: '+reason))});
 window.ph3Runtime=Module;window.FS=Module.FS;return Module;
}
async function prepareManaged(){
 await createRuntime();Module.FS.mkdirTree(saveRoot);Module.FS.mount(Module.IDBFS,{autoPersist:true},saveRoot);await sync(true);
 await mountManagedData(Module,{game,parentWindow:parent,query,emit});Module.FS.mkdirTree(saveRoot+'/replay');
 const data='/game/script/th_hsob/data',replay='/game/script/th_hsob/replay';Module.FS.mkdirTree('/game/script/th_hsob');Module.FS.symlink(saveRoot,data);Module.FS.symlink(saveRoot+'/replay',replay);Module.FS.symlink(saveRoot+'/config.dat','/game/config.dat');
 ready=true;diagnostics.phase='ready';startHealth();emit('ready');
}
async function prepareStandalone(){
 if(ready)return;diagnostics.phase='loading';start.disabled=true;start.textContent='正在加载';if(!Module)await createRuntime();await installLocalManifest();await prepareStorage(true);Module.FS.chdir('/game');ready=true;configured=true;musicMode='ogg';diagnostics.phase='ready';start.textContent='开始游戏';start.disabled=false;startHealth();
}
start.addEventListener('click',()=>{queue=queue.then(async()=>{if(!ready)await prepareStandalone();else launch();}).catch(fail);});
const initialized=(managed?prepareManaged():createRuntime().then(()=>true)).then(()=>true).catch(reason=>{if(isSupersededRuntimeError(reason)){console.debug('Runtime navigation superseded');return false;}fail(reason);throw reason;});
