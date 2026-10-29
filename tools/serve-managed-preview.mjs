import http from 'node:http';
import {readFile} from 'node:fs/promises';
import {resolve,extname,sep} from 'node:path';
const [directory,rawPort='8863']=process.argv.slice(2);
if(!directory)throw Error('Specify the generated local preview directory');
const root=resolve(directory),port=Number(rawPort);
if(!Number.isInteger(port)||port<1024||port>65535)throw Error('Invalid loopback port');
const types={'.html':'text/html','.mjs':'application/javascript','.js':'application/javascript',
  '.wasm':'application/wasm','.json':'application/json','.css':'text/css','.svg':'image/svg+xml',
  '.webmanifest':'application/manifest+json'};
http.createServer(async(req,res)=>{
  if(!['GET','HEAD'].includes(req.method)){res.writeHead(405);res.end();return;}
  if(!['127.0.0.1','localhost'].includes((req.headers.host||'').split(':')[0])){res.writeHead(403);res.end();return;}
  try{
    const relative=decodeURIComponent(new URL(req.url,'http://local').pathname).slice(1)||'index.html';
    const file=resolve(root,relative);
    if(!file.startsWith(root+sep))throw Error('Outside preview');
    const bytes=await readFile(file);
    res.setHeader('Content-Type',types[extname(file)]||'application/octet-stream');
    res.setHeader('Content-Length',bytes.length);res.setHeader('Cache-Control','no-store');
    res.setHeader('X-Content-Type-Options','nosniff');res.end(req.method==='HEAD'?undefined:bytes);
  }catch{res.writeHead(404);res.end();}
}).listen(port,'127.0.0.1',()=>console.log(`Local HSoB integration preview: http://127.0.0.1:${port}/`));
