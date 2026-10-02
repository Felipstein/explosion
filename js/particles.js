// ---------------------------------------------------------------------------
// particles.js — brasas e detritos incandescentes, simulados na GPU.
//
// Estado em duas texturas RGBA32F com ping-pong:
//   A = posição.xyz, vida restante
//   B = velocidade.xyz, temperatura
//
// As partículas são ADVECTADAS PELO CAMPO DE VELOCIDADE DO FLUIDO, não por um
// ruído à parte: é isso que faz as brasas subirem com a coluna e girarem no
// anel de vórtice em vez de parecerem um sistema de partículas colado por
// cima. Cada uma tem temperatura própria e a cor sai da MESMA LUT de corpo
// negro do volume, então brasa e bola de fogo concordam em cor.
//
// São desenhadas DENTRO do alvo da cena, antes do raymarch do volume. Assim o
// composite (cena·transmitância + radiância) oculta as brasas atrás da fumaça
// automaticamente, sem nenhum teste extra.
// ---------------------------------------------------------------------------

import { Shader, Target, PingPong, drawFS, FS_VS } from './gl.js';
import { COMMON } from './glsl.js';

export const PARTICLE_DEFAULTS = {
  count:        12288,
  speedMin:     6.0,
  speedMax:     38.0,
  lifeMin:      0.9,
  lifeMax:      5.5,
  sizeMin:      0.020,
  sizeMax:      0.052,
  gravity:      9.81,
  dragK:        2.6,    // acoplamento ao escoamento (1/s)
  cool:         1.05,   // resfriamento base
  coolBySpeed:  0.055,  // convecção: brasa rápida esfria mais rápido
  restitution:  0.32,
  friction:     0.55,
  stretch:      0.022,  // motion blur: metros de rastro por m/s
  brightness:   11.0,
  spawnRadius:  2.6,
  tempMin:      0.55,
  tempMax:      1.0,
};

export class Particles {
  constructor(gl, grid, bbTex) {
    this.gl = gl;
    this.grid = grid;
    this.bbTex = bbTex;
    this.params = { ...PARTICLE_DEFAULTS };
    this._seed = 0;

    const n = this.params.count;
    this.texW = 256;
    this.texH = Math.ceil(n / this.texW);
    this.count = this.texW * this.texH;

    const f32 = { internalFormat: gl.RGBA32F, format: gl.RGBA, type: gl.FLOAT, filter: gl.NEAREST };
    this.pos = new PingPong(gl, this.texW, this.texH, f32);
    this.vel = new PingPong(gl, this.texW, this.texH, f32);

    const P = grid.glsl();
    const HEAD = `#version 300 es\nprecision highp float;\nprecision highp sampler2D;\n`;

    // ---- spawn: estado inicial derivado do índice (nada vem da CPU) ------
    this.shSpawn = new Shader(gl, FS_VS, HEAD + `in vec2 vUV;\n` + COMMON + `
uniform vec3 uBlastPos;
uniform float uSeed, uSpeedMin, uSpeedMax, uLifeMin, uLifeMax;
uniform float uSpawnRadius, uTempMin, uTempMax, uSizeMin, uSizeMax;
layout(location=0) out vec4 oPos;
layout(location=1) out vec4 oVel;

// Tamanho e tempo de vida são DERIVADOS do índice, não armazenados. Empacotar
// vida+tamanho+atraso num float32 quantizava a vida a zero: com valores na
// casa de 4e6 o ULP já vale 0.5.
void particleConst(vec2 uv, float seed, out float size, out float lifeTotal){
  vec3 h = hash33(vec3(uv * 719.3 + 17.1, seed * 1.7));
  lifeTotal = mix(uLifeMin, uLifeMax, h.x * 0.5 + 0.5);
  size = mix(uSizeMin, uSizeMax, pow(h.y * 0.5 + 0.5, 1.6));
}

void main(){
  vec3 h1 = hash33(vec3(vUV * 311.7, uSeed));
  vec3 h2 = hash33(vec3(vUV * 719.3 + 17.1, uSeed * 1.7));
  float r1 = h1.x * 0.5 + 0.5, r2 = h1.y * 0.5 + 0.5, r3 = h1.z * 0.5 + 0.5;
  float r4 = h2.x * 0.5 + 0.5, r5 = h2.y * 0.5 + 0.5;

  // direção uniforme na esfera, enviesada pro hemisfério superior
  float ct = 1.0 - 2.0 * r1;
  float st = sqrt(max(1.0 - ct * ct, 0.0));
  float ph = r2 * TAU;
  vec3 dir = normalize(vec3(cos(ph) * st, ct * 0.75 + 0.22, sin(ph) * st));

  // velocidade em lei de potência: muitas lentas, poucas muito rápidas —
  // é o que produz os riscos longos isolados em vez de um chuveiro uniforme
  float sp = mix(uSpeedMin, uSpeedMax, pow(r3, 2.4));
  float temp = mix(uTempMin, uTempMax, r5);
  float size, lifeTotal;
  particleConst(vUV, uSeed, size, lifeTotal);

  // A.w NEGATIVO = esperando emissão (o valor é o atraso restante);
  // positivo = vida restante. Escalonar a emissão transforma o chafariz
  // uniforme em jatos e brasas retardatárias.
  float delay = pow(h2.z * 0.5 + 0.5, 2.5) * 0.40;
  vec3 pos = uBlastPos + dir * (uSpawnRadius * pow(r4, 0.33));
  oPos = vec4(pos, delay > 0.004 ? -delay : lifeTotal);
  oVel = vec4(dir * sp, temp);
}`, 'particleSpawn');

    // ---- update ---------------------------------------------------------
    this.shUpdate = new Shader(gl, FS_VS, HEAD + `in vec2 vUV;\n` + COMMON + P + `
uniform sampler2D uPos, uVel, uFluidVel;
uniform float uDt, uGravity, uDragK, uCool, uCoolBySpeed, uRestitution, uFriction;
uniform float uSeed, uLifeMin, uLifeMax, uSizeMin, uSizeMax;
layout(location=0) out vec4 oPos;
layout(location=1) out vec4 oVel;

// Tamanho e tempo de vida são DERIVADOS do índice da partícula, não guardados.
// Empacotá-los junto com a vida num único float32 quantizava a vida a zero:
// com valores na casa de 4.5e6 o ULP já é 0.5.
void particleConst(vec2 uv, float seed, out float size, out float lifeTotal){
  vec3 h = hash33(vec3(uv * 719.3 + 17.1, seed * 1.7));
  float r4 = h.x * 0.5 + 0.5, r5 = h.y * 0.5 + 0.5;
  lifeTotal = mix(uLifeMin, uLifeMax, r4);
  size = mix(uSizeMin, uSizeMax, pow(r5, 1.6));
}

void main(){
  vec4 A = texture(uPos, vUV);
  vec4 B = texture(uVel, vUV);
  float size, lifeTotal;
  particleConst(vUV, uSeed, size, lifeTotal);
  float life = A.w;

  if (life < 0.0){            // ainda esperando a emissão
    float d = life + uDt;
    oPos = vec4(A.xyz, d >= 0.0 ? lifeTotal : d);
    oVel = B;
    return;
  }
  if (life <= 0.0){ oPos = A; oVel = vec4(0.0); return; }

  vec3 p = A.xyz, v = B.xyz;
  float temp = B.w;

  // acopla ao escoamento do fluido (só dentro do domínio da simulação)
  vec3 vp = worldToVoxel(p);
  vec3 fluid = vec3(0.0);
  if (all(greaterThan(vp, vec3(0.0))) && all(lessThan(vp, GRID))){
    fluid = sampleVol(uFluidVel, vp).xyz;
  }
  float rel = length(v - fluid);
  v += (fluid - v) * min(uDragK * uDt, 1.0);
  v.y -= uGravity * uDt;
  p += v * uDt;

  // colisão com o chão: quica e perde energia tangencial
  if (p.y < 0.02){
    p.y = 0.02;
    v.y = abs(v.y) * uRestitution;
    v.xz *= 1.0 - uFriction;
    temp *= 0.82;
  }

  // convecção: quanto mais rápido em relação ao ar, mais rápido esfria
  temp *= exp(-(uCool + uCoolBySpeed * rel) * uDt);
  life -= uDt;

  oPos = vec4(p, max(life, 0.0));
  oVel = vec4(v, temp);
}`, 'particleUpdate');

    // ---- render: quads instanciados, esticados pela velocidade ----------
    this.shDraw = new Shader(gl, HEAD + COMMON + `
uniform sampler2D uPos, uVel;
uniform mat4 uViewProj;
uniform vec3 uCamRight, uCamUp, uCamFwd;
uniform float uStretch, uTexW, uTexH, uSeed, uLifeMin, uLifeMax, uSizeMin, uSizeMax;
out float vTemp;
out vec2 vLocal;
out float vFade;

// Tamanho e tempo de vida são DERIVADOS do índice, não armazenados. Empacotar
// vida+tamanho+atraso num float32 quantizava a vida a zero: com valores na
// casa de 4e6 o ULP já vale 0.5.
void particleConst(vec2 uv, float seed, out float size, out float lifeTotal){
  vec3 h = hash33(vec3(uv * 719.3 + 17.1, seed * 1.7));
  lifeTotal = mix(uLifeMin, uLifeMax, h.x * 0.5 + 0.5);
  size = mix(uSizeMin, uSizeMax, pow(h.y * 0.5 + 0.5, 1.6));
}

void main(){
  // quad via TRIANGLE_STRIP, sem nenhum atributo de vértice
  vec2 corner = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1)) * 2.0 - 1.0;
  int idx = gl_InstanceID;
  ivec2 px = ivec2(idx % int(uTexW), idx / int(uTexW));
  vec4 A = texelFetch(uPos, px, 0);
  vec4 B = texelFetch(uVel, px, 0);
  float life = A.w;
  if (life <= 0.0){ gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }  // morta/esperando

  vec2 uv = (vec2(px) + 0.5) / vec2(uTexW, uTexH);
  float size, lifeTotal;
  particleConst(uv, uSeed, size, lifeTotal);

  // alonga no eixo da velocidade projetada no plano do billboard:
  // motion blur sem passe de velocidade
  vec3 vb = B.xyz - dot(B.xyz, uCamFwd) * uCamFwd;
  float vlen = length(vb);
  vec3 tdir = vlen > 1e-3 ? vb / vlen : uCamRight;
  vec3 pdir = normalize(cross(uCamFwd, tdir));
  float along = size + min(vlen * uStretch, size * 14.0);

  vec3 wp = A.xyz + tdir * (corner.x * along) + pdir * (corner.y * size);
  vTemp = B.w;
  vLocal = corner;
  vFade = smoothstep(0.0, 0.30, life);
  gl_Position = uViewProj * vec4(wp, 1.0);
}`, HEAD + `
in float vTemp; in vec2 vLocal; in float vFade;
` + COMMON + `
uniform sampler2D uBB;
uniform float uBrightness, uTempScale, uEmissionCurve;
layout(location=0) out vec4 oCol;
layout(location=1) out vec4 oNrm;
void main(){
  float d = length(vLocal);
  if (d > 1.0) discard;
  float a = pow(1.0 - d, 1.8);
  // mesma LUT de corpo negro do volume: brasa e bola de fogo concordam em cor
  vec4 bb = texture(uBB, vec2(saturate(vTemp * uTempScale), 0.5));
  vec3 c = bb.rgb * pow(max(bb.a, 0.0), uEmissionCurve) * uBrightness;
  oCol = vec4(c * a * vFade, 1.0);
  oNrm = vec4(0.0);
}`, 'particleDraw');
  }

  reset(blastPos, seed) {
    const gl = this.gl, P = this.params;
    this._seed = seed;
    gl.disable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);
    this._bindMRT(this.pos.read, this.vel.read);
    gl.viewport(0, 0, this.texW, this.texH);
    this.shSpawn.use()
      .set('uBlastPos', blastPos).set('uSeed', seed)
      .set('uSpeedMin', P.speedMin).set('uSpeedMax', P.speedMax)
      .set('uLifeMin', P.lifeMin).set('uLifeMax', P.lifeMax)
      .set('uSpawnRadius', P.spawnRadius)
      .set('uTempMin', P.tempMin).set('uTempMax', P.tempMax)
      .set('uSizeMin', P.sizeMin).set('uSizeMax', P.sizeMax);
    drawFS(gl);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }

  _bindMRT(a, b) {
    const gl = this.gl;
    if (!this._fbo) this._fbo = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, this._fbo);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, a.tex, 0);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT1, gl.TEXTURE_2D, b.tex, 0);
    gl.drawBuffers([gl.COLOR_ATTACHMENT0, gl.COLOR_ATTACHMENT1]);
    gl.viewport(0, 0, this.texW, this.texH);
  }

  step(dt, fluidVelTex, domainOrigin) {
    this.domainOrigin = domainOrigin || this.domainOrigin;
    const gl = this.gl, P = this.params;
    gl.disable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);
    this._bindMRT(this.pos.write, this.vel.write);
    this.shUpdate.use()
      .set('uDt', dt).set('uGravity', P.gravity).set('uDragK', P.dragK)
      .set('uCool', P.cool).set('uCoolBySpeed', P.coolBySpeed)
      .set('uRestitution', P.restitution).set('uFriction', P.friction)
      .set('uSeed', this._seed).set('uLifeMin', P.lifeMin).set('uLifeMax', P.lifeMax)
      .set('uSizeMin', P.sizeMin).set('uSizeMax', P.sizeMax)
      .set('uDomainOrigin', this.domainOrigin || [0, 0, 0])
      .tex('uPos', this.pos.read.tex).tex('uVel', this.vel.read.tex)
      .tex('uFluidVel', fluidVelTex);
    drawFS(gl);
    this.pos.swap();
    this.vel.swap();
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }

  /** desenha no alvo da cena JÁ BINDADO, aditivo, com teste de profundidade */
  draw(cam, tempScale, emissionCurve) {
    const gl = this.gl, P = this.params;
    gl.enable(gl.DEPTH_TEST);
    gl.depthMask(false);            // aditivo não escreve profundidade
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.ONE, gl.ONE);

    // eixos da câmera a partir da matriz de view (linhas = eixos do mundo)
    const v = cam.view;
    const right = [v[0], v[4], v[8]];
    const up = [v[1], v[5], v[9]];
    const fwd = [-v[2], -v[6], -v[10]];

    this.shDraw.use()
      .set('uViewProj', cam.viewProj)
      .set('uCamRight', right).set('uCamUp', up).set('uCamFwd', fwd)
      .set('uStretch', P.stretch).set('uTexW', this.texW).set('uTexH', this.texH)
      .set('uSeed', this._seed).set('uLifeMin', P.lifeMin).set('uLifeMax', P.lifeMax)
      .set('uSizeMin', P.sizeMin).set('uSizeMax', P.sizeMax)
      .set('uBrightness', P.brightness)
      .set('uTempScale', tempScale).set('uEmissionCurve', emissionCurve)
      .tex('uPos', this.pos.read.tex).tex('uVel', this.vel.read.tex)
      .tex('uBB', this.bbTex);
    gl.bindVertexArray(gl._fsVAO);
    // opção "faíscas": desenha uma fração (o índice não tem ordem espacial,
    // então os primeiros N são uma amostra aleatória do todo)
    const n = Math.round(this.count * Math.min(1, P.drawFraction ?? 1));
    if (n > 0) gl.drawArraysInstanced(gl.TRIANGLE_STRIP, 0, 4, n);

    gl.disable(gl.BLEND);
    gl.depthMask(true);
    gl.disable(gl.DEPTH_TEST);
  }
}
