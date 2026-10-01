// Compare actual GLSL on a WebGL2 device. This is a correctness test, not a
// gameplay or phone-performance benchmark.
import assert from 'node:assert/strict';
import {createRequire} from 'node:module';
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {resolve} from 'node:path';
import {createHash} from 'node:crypto';
const root=fileURLToPath(new URL('../',import.meta.url));
const require=createRequire(resolve(root,'../eagler-touhou-hsob/package.json'));
const puppeteer=require('puppeteer-core');
const device=await readFile(resolve(root,'source/GcLib/portable/DxDevice.cpp'),'utf8');
const fixed=await readFile(resolve(root,'source/GcLib/portable/FixedModulate.hpp'),'utf8');
const extract=(text,name)=>{
  const match=text.match(new RegExp(`${name}=R"GLSL\\(([\\s\\S]*?)\\)GLSL";`));
  if(!match)throw Error(`GLSL ${name} not found`);return match[1];
};
const body=fixed.match(/source\+=R"GLSL\(([\s\S]*?)\)GLSL";/)?.[1];
assert.ok(body,'Specialized GLSL body must be the production header');
const source={vertex:extract(device,'VertexSource'),generic:extract(device,'FragmentSource'),body};
const browser=await puppeteer.launch({
  executablePath:process.env.HSOB_CHROME||'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless:true,args:['--no-proxy-server'],
});
let result;
try{
  const page=await browser.newPage();
  result=await page.evaluate(({vertex,generic,body})=>{
    const canvas=document.createElement('canvas');canvas.width=64;canvas.height=32;
    const gl=canvas.getContext('webgl2',{antialias:false,preserveDrawingBuffer:true});
    if(!gl)throw Error('WebGL2 is required');
    const program=fragment=>{
      const p=gl.createProgram();
      for(const [type,source]of[[gl.VERTEX_SHADER,vertex],[gl.FRAGMENT_SHADER,fragment]]){
        const shader=gl.createShader(type);gl.shaderSource(shader,source);gl.compileShader(shader);
        if(!gl.getShaderParameter(shader,gl.COMPILE_STATUS))throw Error(gl.getShaderInfoLog(shader));
        gl.attachShader(p,shader);gl.deleteShader(shader);
      }
      gl.linkProgram(p);
      if(!gl.getProgramParameter(p,gl.LINK_STATUS))throw Error(gl.getProgramInfoLog(p));
      return p;
    };
    const reference=program(generic);
    const candidates=Array.from({length:4},(_,i)=>program('#version 300 es\n'+
      (i>=2?'#define WOO_TEXTURE_ALPHA\n':'')+(i%2?'#define WOO_POSITIVE_ALPHA\n':'')+body));
    const texture=gl.createTexture();gl.activeTexture(gl.TEXTURE0);gl.bindTexture(gl.TEXTURE_2D,texture);
    gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MIN_FILTER,gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MAG_FILTER,gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_S,gl.REPEAT);
    gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_T,gl.REPEAT);
    const pixels=new Uint8Array(16*16*4);
    let seed=0x21747bc5;
    const random=()=>{seed^=seed<<13;seed^=seed>>>17;seed^=seed<<5;return seed>>>0;};
    for(let i=0;i<pixels.length;i++)pixels[i]=random()&255;
    for(let i=0;i<256;i+=4){pixels[4*i+3]=0;pixels[4*(i+1)+3]=1;pixels[4*(i+2)+3]=254;pixels[4*(i+3)+3]=255;}
    gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA8,16,16,0,gl.RGBA,gl.UNSIGNED_BYTE,pixels);
    const vao=gl.createVertexArray();gl.bindVertexArray(vao);
    const buffer=gl.createBuffer();gl.bindBuffer(gl.ARRAY_BUFFER,buffer);
    const vertices=new Float32Array([-1,-1,0,1,0,0, 1,-1,0,1,1,0, -1,1,0,1,0,1, 1,1,0,1,1,1]);
    gl.bufferData(gl.ARRAY_BUFFER,vertices,gl.STATIC_DRAW);
    gl.enableVertexAttribArray(0);gl.vertexAttribPointer(0,4,gl.FLOAT,false,24,0);
    gl.enableVertexAttribArray(1);gl.vertexAttribPointer(1,2,gl.FLOAT,false,24,16);
    gl.vertexAttrib2f(3,0,0);gl.vertexAttrib1f(4,1);
    const behind=program(`#version 300 es\nprecision highp float;out vec4 outputColor;
      void main(){gl_FragDepth=0.75;outputColor=vec4(0.73,0.11,0.29,1.0);}`);
    gl.viewport(0,0,64,32);gl.disable(gl.BLEND);gl.enable(gl.DEPTH_TEST);gl.depthFunc(gl.LESS);
    const configure=(p,mode)=>{
      gl.useProgram(p);
      const set=(name,method,...args)=>{const location=gl.getUniformLocation(p,name);if(location!==null)gl[method](location,...args);};
      set('wooTexture','uniform1i',0);set('wooColorStage','uniform3i',4,2,0);
      set('wooAlphaStage','uniform3i',mode>=2?2:4,2,0);set('wooAlphaTest','uniform1i',mode%2);
      set('wooAlphaFunc','uniform1i',5);set('wooAlphaRef','uniform1f',0);set('wooFogEnable','uniform1i',0);
    };
    const draw=(p,mode,tint,depthCheck)=>{
      gl.clearColor(0,0,0,0);gl.clearDepth(1);gl.depthMask(true);gl.clear(gl.COLOR_BUFFER_BIT|gl.DEPTH_BUFFER_BIT);
      configure(p,mode);gl.vertexAttrib4fv(2,tint);gl.drawArrays(gl.TRIANGLE_STRIP,0,4);
      // Alpha-discarded fragments must leave depth available for this object.
      if(depthCheck){gl.useProgram(behind);gl.drawArrays(gl.TRIANGLE_STRIP,0,4);}
      const out=new Uint8Array(64*32*4);gl.readPixels(0,0,64,32,gl.RGBA,gl.UNSIGNED_BYTE,out);
      const error=gl.getError();if(error!==gl.NO_ERROR)throw Error(`GL error ${error}`);
      return out;
    };
    let cases=0;
    for(let mode=0;mode<4;mode++)for(let i=0;i<36;i++)for(const depthCheck of[false,true]){
      const tint=i===0?[1,1,1,1]:i===1?[0,0,0,0]:i===2?[-.25,1.5,.3,-.1]:Array.from({length:4},()=>random()%256/255);
      const expected=draw(reference,mode,tint,depthCheck),actual=draw(candidates[mode],mode,tint,depthCheck);
      for(let p=0;p<actual.length;p++)if(actual[p]!==expected[p])
        throw Error(`Mode ${mode} case ${i} depth=${depthCheck} byte ${p}: ${actual[p]} != ${expected[p]}`);
      ++cases;
    }
    const debug=gl.getExtension('WEBGL_debug_renderer_info');
    return{passed:true,cases,comparedBytes:cases*64*32*4,
      renderer:gl.getParameter(debug?debug.UNMASKED_RENDERER_WEBGL:gl.RENDERER),
      scope:'WebGL2 shader equality including alpha-test depth writes; not phone performance'};
  },source);
}finally{await browser.close();}
result.sourceSha256={device:createHash('sha256').update(device).digest('hex'),fixed:createHash('sha256').update(fixed).digest('hex')};
await mkdir(resolve(root,'build/fixed-modulate-regression'),{recursive:true});
await writeFile(resolve(root,'build/fixed-modulate-regression/report.json'),JSON.stringify(result,null,2)+'\n');
console.log(JSON.stringify(result));
