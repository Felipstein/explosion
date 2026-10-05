// ---------------------------------------------------------------------------
// sparks.js — faíscas e brasas das explosões INSTANCIADAS.
//
// A explosão ao vivo tem um sistema de partículas por slot, advectado pelo
// campo de velocidade do fluido (particles.js). A instância não tem campo de
// velocidade — e a falta das faíscas era metade da diferença de qualidade
// entre o clique e a tecla espaço.
//
// Aqui um ÚNICO estado de GPU guarda várias RAJADAS (uma por explosão, em
// anel). Cada rajada é um bloco de linhas da textura de estado; o update é um
// passe só pra todas, e o draw é uma chamada instanciada só. O fluido é
// substituído por um escoamento analítico com a mesma cara do que a
// simulação produz: expulsão radial curta no início e a pluma térmica
// subindo pelo eixo depois. Faísca rápida é quase balística de qualquer
// jeito; o que importa do fluido é a brasa lenta subir com a coluna.
//
// Escala por magnitude (W^(1/3) = s): raio de origem ∝ s, velocidade ∝ √s
// (a explosão maior lança mais longe), vida ∝ √s.
// ---------------------------------------------------------------------------

import { Shader, PingPong, drawFS, FS_VS } from './gl.js';
import { COMMON } from './glsl.js';
import { PARTICLE_DEFAULTS } from './particles.js';

const BURSTS = 12;           // explosões com faíscas simultâneas
const TEX_W = 256;
const ROWS = 48;             // linhas por rajada → 12288, o mesmo de um slot ao vivo
const PER = TEX_W * ROWS;

export class InstanceSparks {
  constructor(gl, bbTex) {
    this.gl = gl;
    this.bbTex = bbTex;
    this.params = { ...PARTICLE_DEFAULTS };
    this.texW = TEX_W;
    this.texH = ROWS * BURSTS;
    this.count = this.texW * this.texH;
    // por rajada: centro.xyz + escala, e idade/seed/fração viva
    this.bPos = new Float32Array(BURSTS * 4);
    this.bAge = new Float32Array(BURSTS * 4).fill(1e3);
    this._next = 0;
    this.fraction = 1;     // opção "faíscas"

    const f32 = { internalFormat: gl.RGBA32F, format: gl.RGBA, type: gl.FLOAT, filter: gl.NEAREST };
    this.pos = new PingPong(gl, this.texW, this.texH, f32);
    this.vel = new PingPong(gl, this.texW, this.texH, f32);
    this._fbo = gl.createFramebuffer();
    for (const t of [this.pos.read, this.pos.write, this.vel.read, this.vel.write]) {
      t.bind(true);   // vida 0 = morta
    }
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);

    const HEAD = `#version 300 es\nprecision highp float;\nprecision highp sampler2D;\n`;
    const CONST = `
#define BURSTS ${BURSTS}
#define ROWS ${ROWS}
#define TEXW ${TEX_W}.0
uniform vec4 uBPos[BURSTS];   // centro.xyz, escala
uniform vec4 uBAge[BURSTS];   // idade, seed, fração ativa, -
uniform float uLifeMin, uLifeMax, uSizeMin, uSizeMax;
// constantes da partícula derivadas do índice (nada de empacotar em float)
void particleConst(vec2 px, float seed, float s, out float size, out float lifeTotal){
  vec3 h = hash33(vec3(px * 0.7193 + 17.1, seed * 1.7));
  lifeTotal = mix(uLifeMin, uLifeMax, h.x * 0.5 + 0.5) * sqrt(s);
  size = mix(uSizeMin, uSizeMax, pow(h.y * 0.5 + 0.5, 1.6)) * sqrt(s);
}
`;

    // ---- spawn de UMA rajada (viewport restrito às linhas dela) ----------
    this.shSpawn = new Shader(gl, FS_VS, HEAD + `in vec2 vUV;\n` + COMMON + CONST + `
uniform int uBurst;
uniform float uSpeedMin, uSpeedMax, uSpawnRadius, uTempMin, uTempMax;
layout(location=0) out vec4 oPos;
layout(location=1) out vec4 oVel;
void main(){
  vec2 px = floor(gl_FragCoord.xy);
  vec4 B = uBPos[uBurst], A = uBAge[uBurst];
  float s = B.w, seed = A.y;
  vec3 h1 = hash33(vec3(px * 0.3117, seed));
  vec3 h2 = hash33(vec3(px * 0.7193 + 17.1, seed * 1.7));
  float r1 = h1.x * 0.5 + 0.5, r2 = h1.y * 0.5 + 0.5, r3 = h1.z * 0.5 + 0.5;
  float r4 = h2.x * 0.5 + 0.5, r5 = h2.y * 0.5 + 0.5;
  // explosão pequena lança menos faíscas: o resto nasce morto
  float idx = (px.y - float(uBurst * ROWS)) * TEXW + px.x;
  if (idx >= A.z * float(ROWS) * TEXW){ oPos = vec4(0.0); oVel = vec4(0.0); return; }

  // mesma distribuição da simulação ao vivo: esfera enviesada pra cima,
  // velocidade em lei de potência (muitas lentas, poucas riscando longe)
  float ct = 1.0 - 2.0 * r1;
  float st = sqrt(max(1.0 - ct * ct, 0.0));
  float ph = r2 * TAU;
  vec3 dir = normalize(vec3(cos(ph) * st, ct * 0.75 + 0.22, sin(ph) * st));
  float sp = mix(uSpeedMin, uSpeedMax, pow(r3, 2.4)) * sqrt(s);
  float size, lifeTotal;
  particleConst(px, seed, s, size, lifeTotal);
  float delay = pow(h2.z * 0.5 + 0.5, 2.5) * 0.40 * s;
  vec3 pos = B.xyz + dir * (uSpawnRadius * s * pow(r4, 0.33));
  oPos = vec4(pos, delay > 0.004 ? -delay : lifeTotal);
  oVel = vec4(dir * sp, mix(uTempMin, uTempMax, r5));
}`, 'sparkSpawn');

    // ---- update de TODAS as rajadas --------------------------------------
    this.shUpdate = new Shader(gl, FS_VS, HEAD + `in vec2 vUV;\n` + COMMON + CONST + `
uniform sampler2D uPos, uVel;
uniform float uDt, uGravity, uDragK, uCool, uCoolBySpeed, uRestitution, uFriction;
layout(location=0) out vec4 oPos;
layout(location=1) out vec4 oVel;

// Escoamento analítico no lugar do campo do fluido, calibrado no campo de
// velocidade da simulação ao vivo: velocidade radial ~29 m/s junto ao núcleo
// aos 50ms, ~6 m/s aos 200ms, e só DEPOIS de ~0.4s a pluma térmica se forma.
// Com a pluma ativa desde o início as faíscas eram puxadas pra cima e pro
// eixo e o leque lateral sumia.
vec3 flowAt(vec3 p, vec4 B, float age){
  float s = B.w;
  vec3 d = p - B.xyz;
  float r = length(d), rh = length(d.xz);
  // pico curto do choque + cauda lenta do escoamento de saída
  float vr = sqrt(s) * (30.0 * exp(-age / (0.07 * s)) + 6.0 * exp(-age / (0.40 * s)))
           / (1.0 + r / (6.0 * s));
  vec3 radial = (d / max(r, 1e-3)) * vr;
  float R = (3.5 + 1.6 * age / s) * s;
  float up = 11.0 * sqrt(s) * smoothstep(0.30, 0.90, age / s) * exp(-age / (2.2 * s))
           * exp(-(rh * rh) / (R * R));
  // o anel de vórtice no topo da pluma puxa as brasas pra dentro
  vec3 inflow = -vec3(d.x, 0.0, d.z) / max(rh, 0.5) * 0.25 * up;
  return radial + vec3(0.0, up, 0.0) + inflow;
}

void main(){
  ivec2 ip = ivec2(gl_FragCoord.xy);
  vec2 px = floor(gl_FragCoord.xy);
  int b = ip.y / ROWS;
  vec4 B = uBPos[b], A4 = uBAge[b];
  vec4 A = texelFetch(uPos, ip, 0);
  vec4 V = texelFetch(uVel, ip, 0);
  float size, lifeTotal;
  particleConst(px, A4.y, B.w, size, lifeTotal);
  float life = A.w;
  if (life < 0.0){                     // esperando a emissão
    float d = life + uDt;
    oPos = vec4(A.xyz, d >= 0.0 ? lifeTotal : d);
    oVel = V;
    return;
  }
  if (life <= 0.0){ oPos = A; oVel = vec4(0.0); return; }

  vec3 p = A.xyz, v = V.xyz;
  float temp = V.w;
  vec3 fluid = flowAt(p, B, A4.x);
  float rel = length(v - fluid);
  v += (fluid - v) * min(uDragK * uDt, 1.0);
  v.y -= uGravity * uDt;
  p += v * uDt;
  if (p.y < 0.02){
    p.y = 0.02;
    v.y = abs(v.y) * uRestitution;
    v.xz *= 1.0 - uFriction;
    temp *= 0.82;
  }
  temp *= exp(-(uCool + uCoolBySpeed * rel) * uDt);
  life -= uDt;
  oPos = vec4(p, max(life, 0.0));
  oVel = vec4(v, temp);
}`, 'sparkUpdate');

    // ---- draw: quads esticados pela velocidade (igual ao ao vivo) --------
    this.shDraw = new Shader(gl, HEAD + COMMON + CONST + `
uniform sampler2D uPos, uVel;
uniform mat4 uViewProj;
uniform vec3 uCamRight, uCamFwd;
uniform float uStretch;
out float vTemp;
out vec2 vLocal;
out float vFade;
void main(){
  vec2 corner = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1)) * 2.0 - 1.0;
  int idx = gl_InstanceID;
  ivec2 ip = ivec2(idx % int(TEXW), idx / int(TEXW));
  vec4 A = texelFetch(uPos, ip, 0);
  vec4 V = texelFetch(uVel, ip, 0);
  if (A.w <= 0.0){ gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
  int b = ip.y / ROWS;
  float size, lifeTotal;
  particleConst(vec2(ip), uBAge[b].y, uBPos[b].w, size, lifeTotal);
  vec3 vb = V.xyz - dot(V.xyz, uCamFwd) * uCamFwd;
  float vlen = length(vb);
  vec3 tdir = vlen > 1e-3 ? vb / vlen : uCamRight;
  vec3 pdir = normalize(cross(uCamFwd, tdir));
  float along = size + min(vlen * uStretch, size * 14.0);
  vec3 wp = A.xyz + tdir * (corner.x * along) + pdir * (corner.y * size);
  vTemp = V.w;
  vLocal = corner;
  vFade = smoothstep(0.0, 0.30, A.w);
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
  vec4 bb = texture(uBB, vec2(saturate(vTemp * uTempScale), 0.5));
  vec3 c = bb.rgb * pow(max(bb.a, 0.0), uEmissionCurve) * uBrightness;
  oCol = vec4(c * a * vFade, 1.0);
  oNrm = vec4(0.0);
}`, 'sparkDraw');
  }

  get active() { return this.bAge.some((_, i) => i % 4 === 0 && this.bAge[i] < 12); }

  _bindMRT(a, b) {
    const gl = this.gl;
    gl.bindFramebuffer(gl.FRAMEBUFFER, this._fbo);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, a.tex, 0);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT1, gl.TEXTURE_2D, b.tex, 0);
    gl.drawBuffers([gl.COLOR_ATTACHMENT0, gl.COLOR_ATTACHMENT1]);
  }

  _common(sh) {
    const P = this.params;
    const gl = this.gl;
    gl.uniform4fv(sh.loc('uBPos[0]'), this.bPos);
    gl.uniform4fv(sh.loc('uBAge[0]'), this.bAge);
    sh.set('uLifeMin', P.lifeMin).set('uLifeMax', P.lifeMax)
      .set('uSizeMin', P.sizeMin).set('uSizeMax', P.sizeMax);
  }

  /** nova rajada no ponto (centro da bola de fogo), na escala da magnitude */
  spawn(pos, scale, seed = Math.random() * 1000) {
    if (this.fraction <= 0) return;
    const gl = this.gl, P = this.params;
    const b = this._next;
    this._next = (this._next + 1) % BURSTS;
    this.bPos.set([pos[0], pos[1], pos[2], scale], b * 4);
    // granada lança ~40% das faíscas da Carga Pesada; as grandes, todas
    this.bAge.set([0, seed, Math.min(1, Math.max(0.35, scale)) * this.fraction, 0], b * 4);
    gl.disable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);
    gl.disable(gl.SCISSOR_TEST);
    this._bindMRT(this.pos.read, this.vel.read);
    gl.viewport(0, b * ROWS, this.texW, ROWS);
    const sh = this.shSpawn.use();
    this._common(sh);
    sh.seti('uBurst', b)
      .set('uSpeedMin', P.speedMin).set('uSpeedMax', P.speedMax)
      .set('uSpawnRadius', P.spawnRadius)
      .set('uTempMin', P.tempMin).set('uTempMax', P.tempMax);
    drawFS(gl);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }

  step(dt) {
    if (!this.active) return;
    const gl = this.gl, P = this.params;
    for (let b = 0; b < BURSTS; b++) this.bAge[b * 4] += dt;
    gl.disable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);
    this._bindMRT(this.pos.write, this.vel.write);
    gl.viewport(0, 0, this.texW, this.texH);
    const sh = this.shUpdate.use();
    this._common(sh);
    sh.set('uDt', dt).set('uGravity', P.gravity).set('uDragK', P.dragK)
      .set('uCool', P.cool).set('uCoolBySpeed', P.coolBySpeed)
      .set('uRestitution', P.restitution).set('uFriction', P.friction)
      .tex('uPos', this.pos.read.tex).tex('uVel', this.vel.read.tex);
    drawFS(gl);
    this.pos.swap();
    this.vel.swap();
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }

  /** no alvo da cena JÁ bindado, aditivo, com teste de profundidade */
  draw(cam, tempScale, emissionCurve) {
    if (!this.active) return;
    const gl = this.gl, P = this.params;
    gl.enable(gl.DEPTH_TEST);
    gl.depthMask(false);
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.ONE, gl.ONE);
    const v = cam.view;
    const sh = this.shDraw.use();
    this._common(sh);
    sh.set('uViewProj', cam.viewProj)
      .set('uCamRight', [v[0], v[4], v[8]]).set('uCamFwd', [-v[2], -v[6], -v[10]])
      .set('uStretch', P.stretch).set('uBrightness', P.brightness)
      .set('uTempScale', tempScale).set('uEmissionCurve', emissionCurve)
      .tex('uPos', this.pos.read.tex).tex('uVel', this.vel.read.tex)
      .tex('uBB', this.bbTex);
    gl.bindVertexArray(gl._fsVAO);
    gl.drawArraysInstanced(gl.TRIANGLE_STRIP, 0, 4, this.count);
    gl.disable(gl.BLEND);
    gl.depthMask(true);
    gl.disable(gl.DEPTH_TEST);
  }
}
