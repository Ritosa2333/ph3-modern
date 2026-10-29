import assert from 'node:assert/strict';
import {installGLMemoryObserver} from '../tools/lib/observe-gl-memory.mjs';
class FakeGL {
  createTexture() {return {};}
  createBuffer() {return {};}
  createRenderbuffer() {return {};}
}
for (const name of ['deleteTexture','activeTexture','bindTexture','texImage2D','texStorage2D',
  'generateMipmap','deleteBuffer','bindBuffer','bufferData','deleteRenderbuffer','bindRenderbuffer','renderbufferStorage'])
  FakeGL.prototype[name] = function(...args) {this.lastCall = {name, args};};
globalThis.WebGL2RenderingContext = FakeGL;
installGLMemoryObserver(); installGLMemoryObserver();
const gl = new FakeGL(), snapshot = () => globalThis.__ph3GLMemory.snapshot();
const texture = gl.createTexture(); gl.bindTexture(3553, texture);
gl.texImage2D(3553, 0, 32856, 64, 64, 0, 6408, 5121, null);
assert.equal(snapshot().textureBytes, 64 * 64 * 4);
gl.generateMipmap(3553); assert.equal(snapshot().textureBytes, 21844);
const buffer = gl.createBuffer(); gl.bindBuffer(34962, buffer);
gl.bufferData(34962, new Uint8Array(4096), 35040, 1024, 512);
assert.equal(snapshot().bufferBytes, 512);
assert.deepEqual(gl.lastCall.args.slice(2), [35040, 1024, 512]);
gl.bufferData(34962, 128, 35040); assert.equal(snapshot().bufferBytes, 128);
const depth = gl.createRenderbuffer(); gl.bindRenderbuffer(36161, depth);
gl.renderbufferStorage(36161, 33189, 64, 64); assert.equal(snapshot().renderbufferBytes, 8192);
gl.deleteTexture(texture); gl.deleteBuffer(buffer); gl.deleteRenderbuffer(depth);
assert.equal(snapshot().textureBytes + snapshot().bufferBytes + snapshot().renderbufferBytes, 0);
assert.equal(snapshot().textureCount + snapshot().bufferCount + snapshot().renderbufferCount, 0);
assert(snapshot().peakCombinedBytes > 21844);
console.log('PASS observational GL allocation accounting, subrange uploads, deletion and idempotence');
