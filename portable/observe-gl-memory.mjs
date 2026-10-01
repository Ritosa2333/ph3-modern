// Optional browser-test observer. These are logical WebGL allocations, NOT the
// driver's physical resident memory, transient orphan buffers, or a mobile OOM
// threshold. It is deliberately disabled for timing comparisons.
export function installGLMemoryObserver() {
  if (globalThis.__ph3GLMemory || !globalThis.WebGL2RenderingContext) return;
  const prototype = globalThis.WebGL2RenderingContext.prototype;
  const contexts = new WeakMap(), textures = new Map(), buffers = new Map(), renderbuffers = new Map();
  const stats = {textureBytes: 0, bufferBytes: 0, renderbufferBytes: 0,
    peakTextureBytes: 0, peakBufferBytes: 0, peakRenderbufferBytes: 0,
    peakCombinedBytes: 0, uploadedBufferBytes: 0, textureCreations: 0};
  const state = gl => {
    let value = contexts.get(gl);
    if (!value) {value = {unit: 33984, textures: new Map(), buffers: new Map(), renderbuffer: null}; contexts.set(gl, value);}
    return value;
  };
  const peaks = () => {
    stats.peakTextureBytes = Math.max(stats.peakTextureBytes, stats.textureBytes);
    stats.peakBufferBytes = Math.max(stats.peakBufferBytes, stats.bufferBytes);
    stats.peakRenderbufferBytes = Math.max(stats.peakRenderbufferBytes, stats.renderbufferBytes);
    stats.peakCombinedBytes = Math.max(stats.peakCombinedBytes, stats.textureBytes + stats.bufferBytes + stats.renderbufferBytes);
  };
  const texBytes = value => [...value.levels.values()].reduce((sum, level) => sum + level.bytes, 0);
  const bpp = format => format === 33189 ? 2 : format === 33321 ? 1 : format === 33323 ? 2 : format === 34842 ? 8 : format === 34836 ? 16 : 4;
  const boundTexture = gl => {
    const s = state(gl); return textures.get(s.textures.get(s.unit));
  };
  const replaceLevel = (value, level, width, height, format) => {
    if (!value || width < 0 || height < 0) return;
    stats.textureBytes -= texBytes(value);
    value.levels.set(level, {width, height, format, bytes: width * height * bpp(format)});
    stats.textureBytes += texBytes(value); peaks();
  };
  const wrap = (name, observe) => {
    const original = prototype[name]; if (typeof original !== 'function') return;
    Object.defineProperty(prototype, name, {value: function(...args) {
      const result = Reflect.apply(original, this, args);
      observe(this, args, result); return result;
    }});
  };
  wrap('createTexture', (gl, args, texture) => {
    if (texture) {textures.set(texture, {levels: new Map()}); ++stats.textureCreations;}
  });
  wrap('deleteTexture', (gl, [texture]) => {
    const value = textures.get(texture);
    if (value) {stats.textureBytes -= texBytes(value); textures.delete(texture);}
    const s = state(gl);
    for (const [unit, bound] of s.textures) if (bound === texture) s.textures.delete(unit);
  });
  wrap('activeTexture', (gl, [unit]) => {state(gl).unit = unit;});
  wrap('bindTexture', (gl, [target, texture]) => {if (target === 3553) state(gl).textures.set(state(gl).unit, texture);});
  wrap('texImage2D', (gl, args) => {
    if (args[0] !== 3553) return;
    if (args.length >= 9) replaceLevel(boundTexture(gl), args[1], args[3], args[4], args[2]);
    else if (args.length === 6) {
      const image = args[5]; replaceLevel(boundTexture(gl), args[1], image?.width || 0, image?.height || 0, args[2]);
    }
  });
  wrap('texStorage2D', (gl, [target, levels, format, width, height]) => {
    if (target !== 3553) return;
    for (let level = 0; level < levels; ++level) {
      replaceLevel(boundTexture(gl), level, width, height, format);
      width = Math.max(1, width >> 1); height = Math.max(1, height >> 1);
    }
  });
  wrap('generateMipmap', (gl, [target]) => {
    if (target !== 3553) return;
    const value = boundTexture(gl), base = value?.levels.get(0); if (!base) return;
    let {width, height, format} = base, level = 0;
    while (width > 1 || height > 1) {
      width = Math.max(1, width >> 1); height = Math.max(1, height >> 1);
      replaceLevel(value, ++level, width, height, format);
    }
  });
  wrap('createBuffer', (gl, args, buffer) => {if (buffer) buffers.set(buffer, 0);});
  wrap('deleteBuffer', (gl, [buffer]) => {
    stats.bufferBytes -= buffers.get(buffer) || 0; buffers.delete(buffer);
    for (const [target, bound] of state(gl).buffers) if (bound === buffer) state(gl).buffers.delete(target);
  });
  wrap('bindBuffer', (gl, [target, buffer]) => {state(gl).buffers.set(target, buffer);});
  wrap('bufferData', (gl, args) => {
    const buffer = state(gl).buffers.get(args[0]); if (!buffers.has(buffer)) return;
    const data = args[1], elementBytes = data?.BYTES_PER_ELEMENT || 1;
    const bytes = typeof data === 'number' ? data : args[4] ? args[4] * elementBytes : Math.max(0, (data?.byteLength || 0) - (args[3] || 0) * elementBytes);
    stats.bufferBytes += bytes - buffers.get(buffer); buffers.set(buffer, bytes);
    if (typeof data !== 'number') stats.uploadedBufferBytes += bytes;
    peaks();
  });
  wrap('createRenderbuffer', (gl, args, buffer) => {if (buffer) renderbuffers.set(buffer, 0);});
  wrap('bufferSubData', (gl, args) => {
    const data=args[2], elementBytes=data?.BYTES_PER_ELEMENT||1;
    stats.uploadedBufferBytes+=args[4]?args[4]*elementBytes:Math.max(0,(data?.byteLength||0)-(args[3]||0)*elementBytes);
  });
  wrap('deleteRenderbuffer', (gl, [buffer]) => {
    stats.renderbufferBytes -= renderbuffers.get(buffer) || 0; renderbuffers.delete(buffer);
    if (state(gl).renderbuffer === buffer) state(gl).renderbuffer = null;
  });
  wrap('bindRenderbuffer', (gl, [target, buffer]) => {state(gl).renderbuffer = buffer;});
  wrap('renderbufferStorage', (gl, [target, format, width, height]) => {
    const buffer = state(gl).renderbuffer; if (!renderbuffers.has(buffer)) return;
    const bytes = width * height * bpp(format);
    stats.renderbufferBytes += bytes - renderbuffers.get(buffer); renderbuffers.set(buffer, bytes); peaks();
  });
  globalThis.__ph3GLMemory = {snapshot: () => ({...stats,
    textureCount: textures.size, bufferCount: buffers.size, renderbufferCount: renderbuffers.size,
    estimated: true, excludesDriverAndCanvasMemory: true})};
}
