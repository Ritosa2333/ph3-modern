const DATA_MAGIC='HSOBFS01';
const COMPRESSED_DATA_MAGIC='HSOBZ001';
const DATA_SCHEMA='eagler-hsob-content/1';
const MAX_DATA_BYTES=512*1024*1024;
const MAX_FILE_BYTES=256*1024*1024;

export function normalizeOptions(value={}){
 const modes=['touch','touch-unlimited','joystick','joystick-free'];
 const sensitivity=Number(value.touchSensitivity??100);
 return {...value,
  limitPresentationTo60:!!value.limitPresentationTo60,
  touchEnabled:!!value.touchEnabled,
  touchMovementMode:modes.includes(value.touchMovementMode)?value.touchMovementMode:'touch',
  touchSensitivity:Number.isFinite(sensitivity)?Math.max(100,Math.min(300,sensitivity)):100,
  touchFocusMode:value.touchFocusMode==='two-finger'?'two-finger':'button',
  doubleTapBombEnabled:!!value.doubleTapBombEnabled,
  alwaysHitbox:!!value.alwaysHitbox,
  oggDecodeMode:value.oggDecodeMode==='full'?'full':'stream',
 };
}

export function touchSnapshot(options,message){
 const controls=message?.controls??message??{};
 const sensitivity=Number(controls.touchSensitivity);
 if(Number.isFinite(sensitivity))options.touchSensitivity=Math.max(100,Math.min(300,sensitivity));
 const axis=value=>Number.isFinite(Number(value))?Math.max(-32767,Math.min(32767,Number(value))):0;
 return {
  fire:!!controls.fireEnabled,
  focus:!!controls.focusEnabled,
  bomb:controls.bombSerial>>>0,
  escape:controls.escapeSerial>>>0,
  x:axis(controls.joystickX),
  y:axis(controls.joystickY),
 };
}

function safeGamePath(path){
 if(path==='/launcher.ini')return true;
 return typeof path==='string'&&path.startsWith('/game/')&&!path.includes('\\')&&!path.includes('//')&&
  !path.split('/').some(part=>part==='.'||part==='..');
}

async function inflateManagedData(bytes){
 if(new TextDecoder('ascii').decode(bytes.subarray(0,8))!==COMPRESSED_DATA_MAGIC)return bytes;
 if(bytes.byteLength<13)throw Error('Invalid compressed HSoB DATA');
 const expected=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength).getUint32(8,true);
 if(expected<16||expected>MAX_DATA_BYTES)throw Error('Invalid compressed HSoB DATA size');
 if(typeof DecompressionStream!=='function')throw Error('This browser cannot decompress HSoB DATA');
 const stream=new Blob([bytes.subarray(12)]).stream().pipeThrough(new DecompressionStream('deflate'));
 const inflated=new Uint8Array(await new Response(stream).arrayBuffer());
 if(inflated.byteLength!==expected)throw Error('HSoB DATA decompression size mismatch');
 return inflated;
}

export async function unpackManagedData(Module,bytes){
 if(!(bytes instanceof Uint8Array)||bytes.byteLength<16||bytes.byteLength>MAX_DATA_BYTES)throw Error('Invalid HSoB DATA size');
 bytes=await inflateManagedData(bytes);
 if(new TextDecoder('ascii').decode(bytes.subarray(0,8))!==DATA_MAGIC)throw Error('Invalid HSoB DATA magic');
 const view=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength);
 const manifestBytes=view.getUint32(8,true);
 if(manifestBytes<=0||manifestBytes>8*1024*1024||12+manifestBytes>bytes.byteLength)throw Error('Invalid HSoB DATA manifest size');
 let manifest;
 try{manifest=JSON.parse(new TextDecoder().decode(bytes.subarray(12,12+manifestBytes)));}
 catch{throw Error('Invalid HSoB DATA manifest');}
 if(manifest?.schema!==DATA_SCHEMA||!Array.isArray(manifest.files)||manifest.files.length<1||manifest.files.length>4096)
  throw Error('Invalid HSoB DATA manifest schema');
 const payload=12+manifestBytes;
 let previousEnd=0;
 for(const file of manifest.files){
  if(!safeGamePath(file?.path)||!Number.isSafeInteger(file.offset)||!Number.isSafeInteger(file.bytes)||file.offset<previousEnd||file.bytes<0||file.bytes>MAX_FILE_BYTES)
   throw Error('Invalid HSoB DATA file declaration');
  const end=file.offset+file.bytes;
  if(end<file.offset||payload+end>bytes.byteLength)throw Error('HSoB DATA file exceeds payload');
  Module.FS.mkdirTree(file.path.slice(0,file.path.lastIndexOf('/')));
  Module.FS.writeFile(file.path,bytes.subarray(payload+file.offset,payload+end),{canOwn:true});
  previousEnd=end;
 }
 if(payload+previousEnd!==bytes.byteLength)throw Error('HSoB DATA has trailing bytes');
 return manifest.files.length;
}

export async function mountManagedData(Module,{game='hsob',parentWindow=window.parent,query=new URLSearchParams(location.search),emit=()=>{}}={}){
 // Keep the exact bridge symbol visible to Runtime Release validation.
 const provider=window.parent.__eaglerPrepareManagedRuntimeDataV1;
 if(query.get('managedData')!=='1'||typeof provider!=='function')throw Error('请从 eagler-touhou 启动此运行时');
 const epoch=Number(query.get('runtimeEpoch'));if(!Number.isSafeInteger(epoch)||epoch<=0)throw Error('Runtime navigation epoch unavailable');
 const result=await parentWindow.__eaglerPrepareManagedRuntimeDataV1({game,generation:query.get('gameGeneration'),epoch});
 const bytes=new Uint8Array(result.buffer);
 const files=await unpackManagedData(Module,bytes);
 emit('transfer',{mode:'data',loaded:bytes.byteLength,total:bytes.byteLength,files});
}

export function resourcePath(path){
 return path==='/msgothic.ttc'||/^\/game\/script\/th_hsob\/music\/[A-Za-z0-9][A-Za-z0-9._-]*\.ogg$/i.test(path);
}

export async function installResources(Module,resources=[],{emit=()=>{},base=globalThis.location?.href,fetcher=globalThis.fetch}={}){
 if(!Array.isArray(resources)||resources.length>32)throw Error('Invalid resource list');
 for(const resource of resources){
  if(!resourcePath(resource?.path))throw Error('Invalid HSoB runtime resource path');
  const url=new URL(resource.url,base),origin=new URL(base).origin;
  if(url.origin!==origin||!['http:','https:','blob:'].includes(url.protocol))throw Error('Resource must be same-origin');
  const response=await fetcher(url);if(!response.ok)throw Error('Resource download failed: '+resource.path);
  const bytes=new Uint8Array(await response.arrayBuffer()),expected=resource.bytes??resource.size;
  const limit=resource.path==='/msgothic.ttc'?32*1024*1024:128*1024*1024;
  if(bytes.byteLength<=0||bytes.byteLength>limit||(expected!=null&&bytes.byteLength!==Number(expected)))throw Error('Resource size mismatch: '+resource.path);
  Module.FS.mkdirTree(resource.path.slice(0,resource.path.lastIndexOf('/'))||'/');
  Module.FS.writeFile(resource.path,bytes,{canOwn:true});
  emit('transfer',{mode:resource.path.endsWith('/music.dat')?'ogg':'runtime',loaded:bytes.byteLength,total:bytes.byteLength,path:resource.path});
 }
}

export function ensureFontAlias(Module){
 try{Module.FS.stat('/msgothic.ttc');}catch{throw Error('Shared Japanese font is unavailable');}
 try{Module.FS.unlink('/game/msgothic.ttc');}catch{}
 Module.FS.symlink('/msgothic.ttc','/game/msgothic.ttc');
}

export function isSupersededRuntimeError(error){
 return error?.name==='AbortError'&&error?.message==='EAGLER_RUNTIME_SESSION_SUPERSEDED';
}
