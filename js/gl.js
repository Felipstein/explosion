// ---------------------------------------------------------------------------
// gl.js — camada fina sobre WebGL2. Programas, render targets, ping-pong,
// desenho fullscreen sem VBO (gl_VertexID), e relatório de erro de shader
// com número de linha (essencial pra não debugar tela preta no escuro).
// ---------------------------------------------------------------------------

export const FS_VS = `#version 300 es
precision highp float;
out vec2 vUV;
void main(){
  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
  vUV = p;
  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}`;

function compile(gl, type, src, name) {
  const sh = gl.createShader(type);
  gl.shaderSource(sh, src);
  gl.compileShader(sh);
  if (!gl.getShaderParameter(sh, gl.COMPILE_STATUS)) {
    const log = gl.getShaderInfoLog(sh);
    const lines = src.split('\n');
    let report = `\n=== ${name} (${type === gl.VERTEX_SHADER ? 'VS' : 'FS'}) ===\n${log}\n`;
    // anexa as linhas citadas no log
    const nums = new Set();
    for (const m of log.matchAll(/:(\d+):/g)) nums.add(parseInt(m[1]));
    for (const n of [...nums].sort((a, b) => a - b)) {
      for (let i = Math.max(1, n - 2); i <= Math.min(lines.length, n + 2); i++) {
        report += `${i === n ? '>>' : '  '} ${String(i).padStart(4)} | ${lines[i - 1]}\n`;
      }
      report += '\n';
    }
    console.error(report);
    throw new Error(`shader compile falhou: ${name}`);
  }
  return sh;
}

export class Shader {
  constructor(gl, vsSrc, fsSrc, name = 'shader') {
    this.gl = gl;
    this.name = name;
    const p = gl.createProgram();
    const vs = compile(gl, gl.VERTEX_SHADER, vsSrc, name);
    const fs = compile(gl, gl.FRAGMENT_SHADER, fsSrc, name);
    gl.attachShader(p, vs);
    gl.attachShader(p, fs);
    gl.linkProgram(p);
    if (!gl.getProgramParameter(p, gl.LINK_STATUS)) {
      throw new Error(`link falhou (${name}): ${gl.getProgramInfoLog(p)}`);
    }
    gl.deleteShader(vs);
    gl.deleteShader(fs);
    this.p = p;
    this._loc = new Map();
    this._unit = 0;
  }

  use() {
    this.gl.useProgram(this.p);
    this._unit = 0;
    return this;
  }

  loc(n) {
    let l = this._loc.get(n);
    if (l === undefined) {
      l = this.gl.getUniformLocation(this.p, n);
      this._loc.set(n, l);
    }
    return l;
  }

  // set(nome, valor) — dispatch por tipo/comprimento
  set(n, v) {
    const gl = this.gl, l = this.loc(n);
    if (l === null) return this;
    if (typeof v === 'number') gl.uniform1f(l, v);
    else if (typeof v === 'boolean') gl.uniform1i(l, v ? 1 : 0);
    else if (v.length === 2) gl.uniform2fv(l, v);
    else if (v.length === 3) gl.uniform3fv(l, v);
    else if (v.length === 4) gl.uniform4fv(l, v);
    else if (v.length === 9) gl.uniformMatrix3fv(l, false, v);
    else if (v.length === 16) gl.uniformMatrix4fv(l, false, v);
    else throw new Error(`uniform ${n}: tipo desconhecido`);
    return this;
  }

  seti(n, v) {
    const l = this.loc(n);
    if (l !== null) this.gl.uniform1i(l, v);
    return this;
  }

  // tex(nome, textura) — aloca a unidade automaticamente na ordem de chamada
  tex(n, t, target) {
    const gl = this.gl, l = this.loc(n);
    if (l === null) return this;
    const u = this._unit++;
    gl.activeTexture(gl.TEXTURE0 + u);
    gl.bindTexture(target || gl.TEXTURE_2D, t);
    gl.uniform1i(l, u);
    return this;
  }

  sets(obj) {
    for (const k in obj) this.set(k, obj[k]);
    return this;
  }
}

export function createTexture(gl, w, h, opt = {}) {
  const {
    internalFormat = gl.RGBA16F,
    format = gl.RGBA,
    type = gl.HALF_FLOAT,
    filter = gl.LINEAR,
    wrap = gl.CLAMP_TO_EDGE,
    data = null,
  } = opt;
  const t = gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D, t);
  gl.texImage2D(gl.TEXTURE_2D, 0, internalFormat, w, h, 0, format, type, data);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, filter);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, filter);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, wrap);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, wrap);
  return t;
}

export class Target {
  constructor(gl, w, h, opt = {}) {
    this.gl = gl;
    this.w = w;
    this.h = h;
    this.tex = createTexture(gl, w, h, opt);
    this.fbo = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.fbo);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, this.tex, 0);
    this.depthTex = null;
    if (opt.depth) {
      this.depthTex = createTexture(gl, w, h, {
        internalFormat: gl.DEPTH_COMPONENT24,
        format: gl.DEPTH_COMPONENT,
        type: gl.UNSIGNED_INT,
        filter: gl.NEAREST,
      });
      gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.TEXTURE_2D, this.depthTex, 0);
    }
    const st = gl.checkFramebufferStatus(gl.FRAMEBUFFER);
    if (st !== gl.FRAMEBUFFER_COMPLETE) throw new Error(`FBO incompleto: 0x${st.toString(16)} (${w}x${h})`);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }

  bind(clear = false) {
    const gl = this.gl;
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.fbo);
    gl.viewport(0, 0, this.w, this.h);
    if (clear) {
      gl.clearColor(0, 0, 0, 0);
      gl.clear(gl.COLOR_BUFFER_BIT | (this.depthTex ? gl.DEPTH_BUFFER_BIT : 0));
    }
    return this;
  }

  dispose() {
    const gl = this.gl;
    gl.deleteTexture(this.tex);
    if (this.depthTex) gl.deleteTexture(this.depthTex);
    gl.deleteFramebuffer(this.fbo);
  }
}

export class PingPong {
  constructor(gl, w, h, opt = {}) {
    this.a = new Target(gl, w, h, opt);
    this.b = new Target(gl, w, h, opt);
  }
  get read() { return this.a; }
  get write() { return this.b; }
  swap() { const t = this.a; this.a = this.b; this.b = t; }
  dispose() { this.a.dispose(); this.b.dispose(); }
}

export function drawFS(gl) {
  // os passes fullscreen não usam atributo nenhum; o VAO vazio garante que
  // nenhum resto de VAO de malha (com atributos habilitados) atrapalhe
  gl.bindVertexArray(gl._fsVAO);
  gl.drawArrays(gl.TRIANGLES, 0, 3);
}

export function initGL(canvas) {
  const gl = canvas.getContext('webgl2', {
    alpha: false,
    antialias: false,
    depth: false,
    stencil: false,
    premultipliedAlpha: false,
    preserveDrawingBuffer: false,
    powerPreference: 'high-performance',
    desynchronized: true,
  });
  if (!gl) throw new Error('WebGL2 não disponível');
  const need = ['EXT_color_buffer_float'];
  for (const e of need) {
    if (!gl.getExtension(e)) throw new Error(`extensão obrigatória ausente: ${e}`);
  }
  gl.getExtension('OES_texture_float_linear');
  gl.getExtension('EXT_float_blend');
  // VAO vazio: os passes fullscreen não usam atributo nenhum
  gl._fsVAO = gl.createVertexArray();
  gl.bindVertexArray(gl._fsVAO);
  gl.disable(gl.DEPTH_TEST);
  gl.disable(gl.CULL_FACE);
  gl.disable(gl.BLEND);
  return gl;
}

/**
 * Liga mipmap + filtragem anisotrópica numa textura 2D.
 *
 * Sem isto, uma textura de detalhe amostrada em ângulo rasante tem um pixel
 * cobrindo dezenas de texels, e o resultado é tempestade de aliasing — o
 * chão inteiro vira ruído salpicado. É o defeito visual mais caro em
 * material procedural, e o mais barato de corrigir.
 */
export function enableMipAniso(gl, tex) {
  gl.bindTexture(gl.TEXTURE_2D, tex);
  gl.generateMipmap(gl.TEXTURE_2D);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
  const ext = gl.getExtension('EXT_texture_filter_anisotropic');
  if (ext) {
    const max = gl.getParameter(ext.MAX_TEXTURE_MAX_ANISOTROPY_EXT);
    gl.texParameterf(gl.TEXTURE_2D, ext.TEXTURE_MAX_ANISOTROPY_EXT, Math.min(16, max));
  }
  return tex;
}

// --- texturas 3D reais -----------------------------------------------------
// Usadas só pra dados estáticos que precisam de wrap REPEAT + trilinear em
// hardware (o campo de ruído curl). Escrever exige uma draw por layer, o que
// é irrelevante quando se escreve uma única vez na inicialização.
export function createTexture3D(gl, w, h, d, opt = {}) {
  const {
    internalFormat = gl.RGBA16F, format = gl.RGBA, type = gl.HALF_FLOAT,
    filter = gl.LINEAR, wrap = gl.REPEAT,
  } = opt;
  const t = gl.createTexture();
  gl.bindTexture(gl.TEXTURE_3D, t);
  gl.texImage3D(gl.TEXTURE_3D, 0, internalFormat, w, h, d, 0, format, type, null);
  gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_MIN_FILTER, filter);
  gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_MAG_FILTER, filter);
  gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_WRAP_S, wrap);
  gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_WRAP_T, wrap);
  gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_WRAP_R, wrap);
  return t;
}

/** renderiza um shader fullscreen em cada layer de uma textura 3D */
export function renderToTexture3D(gl, tex, w, h, d, shader, setup) {
  const fbo = gl.createFramebuffer();
  gl.bindFramebuffer(gl.FRAMEBUFFER, fbo);
  gl.viewport(0, 0, w, h);
  shader.use();
  for (let z = 0; z < d; z++) {
    gl.framebufferTextureLayer(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, tex, 0, z);
    shader.set('uLayer', (z + 0.5) / d);
    if (setup) setup(shader, z);
    drawFS(gl);
  }
  gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  gl.deleteFramebuffer(fbo);
}

// --- MRT: múltiplos color attachments -------------------------------------
export class MRTarget {
  constructor(gl, w, h, formats, opt = {}) {
    this.gl = gl; this.w = w; this.h = h;
    this.texs = formats.map((f) => createTexture(gl, w, h, f));
    this.fbo = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.fbo);
    const bufs = [];
    this.texs.forEach((t, i) => {
      gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0 + i, gl.TEXTURE_2D, t, 0);
      bufs.push(gl.COLOR_ATTACHMENT0 + i);
    });
    gl.drawBuffers(bufs);
    this.depthTex = null;
    if (opt.depth) {
      this.depthTex = createTexture(gl, w, h, {
        internalFormat: gl.DEPTH_COMPONENT24, format: gl.DEPTH_COMPONENT,
        type: gl.UNSIGNED_INT, filter: gl.NEAREST,
      });
      gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.TEXTURE_2D, this.depthTex, 0);
    }
    const st = gl.checkFramebufferStatus(gl.FRAMEBUFFER);
    if (st !== gl.FRAMEBUFFER_COMPLETE) throw new Error(`MRT incompleto: 0x${st.toString(16)}`);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }
  get tex() { return this.texs[0]; }
  bind(clear = false) {
    const gl = this.gl;
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.fbo);
    gl.viewport(0, 0, this.w, this.h);
    if (clear) {
      gl.clearColor(0, 0, 0, 0);
      gl.clear(gl.COLOR_BUFFER_BIT | (this.depthTex ? gl.DEPTH_BUFFER_BIT : 0));
    }
    return this;
  }
  dispose() {
    const gl = this.gl;
    this.texs.forEach((t) => gl.deleteTexture(t));
    if (this.depthTex) gl.deleteTexture(this.depthTex);
    gl.deleteFramebuffer(this.fbo);
  }
}

// ---------------------------------------------------------------------------
// Escopo de recursos. Registra todo objeto GL criado durante `fn` e devolve
// um handle que apaga todos de uma vez. Uma troca de qualidade reconstrói
// pool, cena e volume — e nada disso era liberado: cada troca vazava ~600MB
// de VRAM até o driver começar a falhar alocações em silêncio (textura
// incompleta amostra preto). Rastrear na criação cobre todas as classes sem
// um dispose() escrito à mão em cada uma.
// ---------------------------------------------------------------------------
const GL_KINDS = [
  ['createTexture', 'deleteTexture'], ['createFramebuffer', 'deleteFramebuffer'],
  ['createBuffer', 'deleteBuffer'], ['createProgram', 'deleteProgram'],
  ['createVertexArray', 'deleteVertexArray'], ['createRenderbuffer', 'deleteRenderbuffer'],
];

export function trackGL(gl, fn) {
  const objs = [];
  for (const [c, d] of GL_KINDS) {
    const orig = Object.getPrototypeOf(gl)[c];
    gl[c] = (...a) => { const o = orig.apply(gl, a); if (o) objs.push([d, o]); return o; };
  }
  let result;
  try { result = fn(); } finally { for (const [c] of GL_KINDS) delete gl[c]; }
  return {
    result,
    // apagar duas vezes é no-op em WebGL, então quem já liberou algo por
    // conta própria (resize) não atrapalha
    dispose() { for (const [d, o] of objs) gl[d](o); objs.length = 0; },
  };
}
