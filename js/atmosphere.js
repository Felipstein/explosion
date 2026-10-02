// ---------------------------------------------------------------------------
// atmosphere.js — espalhamento atmosférico fisicamente baseado, em LUTs.
//
// Implementa o esquema de Hillaire, "A Scalable and Production Ready Sky and
// Atmosphere Rendering Technique", EGSR 2020 (o mesmo usado em Frostbite e
// Unreal 5), sobre o modelo de meio de Bruneton & Neyret 2008:
//
//   1. LUT de transmitância   (256×64)  — e^(-τ) de uma altitude r numa
//      direção μ até o topo da atmosfera. Calculada UMA vez.
//   2. LUT de multi-espalhamento (32×32) — a série infinita de ordens de
//      espalhamento colapsada numa geométrica, L₂⁺ = L_f/(1-f_ms).
//      É isso que impede o céu diurno de ficar escuro e saturado demais
//      (espalhamento simples sozinho perde ~40% da energia). UMA vez.
//   3. LUT sky-view          (192×128) — radiância do céu inteiro pra
//      direção do sol atual. Recalculada só quando o sol se move.
//   4. LUT de ambiente       (5×1)     — irradiância da luz-chave,
//      irradiância hemisférica do céu (cima/baixo), cor do disco solar e
//      (texel 4) exposição analítica + iluminância física da hora, que o
//      post usa no Purkinje shift. Tudo na GPU, sem readback pra CPU.
//
// A Lua espalha na mesma atmosfera que o Sol: o céu de luar sai do mesmo
// modelo, só com outra fonte (mais fraca e um pouco mais avermelhada).
//
// O meio inclui ozônio (perfil tenda em 25km), que é o que torna o
// crepúsculo azul-violeta em vez de marrom.
// ---------------------------------------------------------------------------

import { Shader, Target, drawFS, FS_VS } from './gl.js';
import { COMMON } from './glsl.js';

const T_W = 256, T_H = 64;
const MS_RES = 32;
const SKY_W = 192, SKY_H = 128;

// --- meio atmosférico (unidades em km, coeficientes em 1/km) ---------------
const MEDIUM = `
const float Rg = 6360.0;            // raio da Terra
const float Rt = 6460.0;            // topo da atmosfera
const vec3  BETA_R = vec3(5.802, 13.558, 33.100) * 1e-3;  // Rayleigh
const float HR = 8.0;                                      // altura de escala
const float BETA_M_S = 3.996e-3;                           // Mie espalhamento
const float BETA_M_E = 4.400e-3;                           // Mie extinção
const float HM = 1.2;
const vec3  BETA_O = vec3(0.650, 1.881, 0.085) * 1e-3;     // ozônio
const float MIE_G = 0.80;
const vec3  GROUND_ALBEDO = vec3(0.12, 0.11, 0.10);

void mediumAt(float h, out vec3 scatR, out float scatM, out vec3 ext){
  float dR = exp(-max(h, 0.0) / HR);
  float dM = exp(-max(h, 0.0) / HM);
  float dO = max(0.0, 1.0 - abs(h - 25.0) / 15.0);   // perfil tenda do ozônio
  scatR = BETA_R * dR;
  scatM = BETA_M_S * dM;
  ext = scatR + vec3(BETA_M_E * dM) + BETA_O * dO;
}

float phaseRayleigh(float c){ return (3.0 / (16.0 * PI)) * (1.0 + c * c); }
float phaseMie(float c){
  float g = MIE_G, g2 = g * g;
  float d = 1.0 + g2 - 2.0 * g * c;
  return (3.0 / (8.0 * PI)) * ((1.0 - g2) * (1.0 + c * c))
       / ((2.0 + g2) * pow(max(d, 1e-4), 1.5));
}

// distância até a esfera de raio R a partir de (r, mu); -1 se não intersecta
float raySphere(float r, float mu, float R){
  float b = r * r * (mu * mu - 1.0) + R * R;
  if (b < 0.0) return -1.0;
  return max(-r * mu + sqrt(b), 0.0);
}
bool hitsGround(float r, float mu){
  return mu < 0.0 && (r * r * (mu * mu - 1.0) + Rg * Rg) >= 0.0;
}

float unitToUV(float x, float n){ return 0.5 / n + x * (1.0 - 1.0 / n); }
float uvToUnit(float u, float n){ return (u - 0.5 / n) / (1.0 - 1.0 / n); }

// ---- parametrização da LUT de transmitância (Bruneton) ------------------
vec2 transmittanceUV(float r, float mu){
  float H = sqrt(max(Rt * Rt - Rg * Rg, 0.0));
  float rho = sqrt(max(r * r - Rg * Rg, 0.0));
  float d = max(-r * mu + sqrt(max(r * r * (mu * mu - 1.0) + Rt * Rt, 0.0)), 0.0);
  float dmin = Rt - r, dmax = rho + H;
  return vec2(unitToUV((d - dmin) / max(dmax - dmin, 1e-6), ${T_W}.0),
              unitToUV(rho / max(H, 1e-6), ${T_H}.0));
}
void uvToTransmittance(vec2 uv, out float r, out float mu){
  float xmu = uvToUnit(uv.x, ${T_W}.0);
  float xr  = uvToUnit(uv.y, ${T_H}.0);
  float H = sqrt(Rt * Rt - Rg * Rg);
  float rho = H * xr;
  r = sqrt(rho * rho + Rg * Rg);
  float dmin = Rt - r, dmax = rho + H;
  float d = dmin + xmu * (dmax - dmin);
  mu = d <= 0.0 ? 1.0 : clamp((H * H - rho * rho - d * d) / (2.0 * r * d), -1.0, 1.0);
}
`;

const SAMPLERS = `
uniform sampler2D uTransLut;
uniform sampler2D uMsLut;

vec3 transmittanceTo(float r, float mu){
  return texture(uTransLut, transmittanceUV(clamp(r, Rg, Rt), mu)).rgb;
}
// transmitância do sol considerando a sombra da própria Terra
vec3 sunTransmittance(float r, float muS){
  if (hitsGround(r, muS)) return vec3(0.0);
  return transmittanceTo(r, muS);
}
vec3 multiScatter(float r, float muS){
  vec2 uv = vec2(unitToUV(muS * 0.5 + 0.5, ${MS_RES}.0),
                 unitToUV(clamp((r - Rg) / (Rt - Rg), 0.0, 1.0), ${MS_RES}.0));
  return texture(uMsLut, uv).rgb;
}
`;

const HEAD = `#version 300 es
precision highp float;
in vec2 vUV;
`;

export class Atmosphere {
  constructor(gl) {
    this.gl = gl;
    const fmt = { internalFormat: gl.RGBA16F, format: gl.RGBA, type: gl.HALF_FLOAT };
    const fmt32 = { internalFormat: gl.RGBA32F, format: gl.RGBA, type: gl.FLOAT, filter: gl.NEAREST };

    this.trans = new Target(gl, T_W, T_H, fmt);
    this.ms = new Target(gl, MS_RES, MS_RES, fmt);
    this.skyView = new Target(gl, SKY_W, SKY_H, fmt);
    this.envLut = new Target(gl, 5, 1, fmt32);

    this._lastSun = new Float32Array([9, 9, 9]);
    this._lastMoon = new Float32Array([9, 9, 9]);
    this._lastParams = '';

    // Cópia na CPU do texel 4 (exposição, lux, E_render, escala do Purkinje),
    // lida de forma assíncrona (PBO + fence) quando a LUT muda: quem decide
    // na CPU o que vale a pena desenhar precisa saber a luz da hora, e um
    // readPixels direto travava o quadro (21 ms).
    this.envCPU = null;
    this._envBuf = gl.createBuffer();
    gl.bindBuffer(gl.PIXEL_PACK_BUFFER, this._envBuf);
    gl.bufferData(gl.PIXEL_PACK_BUFFER, 16, gl.STREAM_READ);
    gl.bindBuffer(gl.PIXEL_PACK_BUFFER, null);
    this._envFence = null;
    this._envDirty = false;
    this._envRead = new Float32Array(4);

    // ---- 1. transmitância -------------------------------------------------
    this.shTrans = new Shader(gl, FS_VS, HEAD + COMMON + MEDIUM + `
out vec4 oCol;
void main(){
  float r, mu;
  uvToTransmittance(vUV, r, mu);
  // marcha de r na direção mu até o topo da atmosfera acumulando extinção
  float d = raySphere(r, mu, Rt);
  const int N = 40;
  float dt = d / float(N);
  vec3 tau = vec3(0.0);
  for (int i = 0; i < N; i++){
    float t = (float(i) + 0.5) * dt;
    float rr = sqrt(max(t * t + 2.0 * r * mu * t + r * r, 0.0));
    vec3 sR, ext; float sM;
    mediumAt(rr - Rg, sR, sM, ext);
    tau += ext * dt;
  }
  oCol = vec4(exp(-tau), 1.0);
}`, 'atmosTransmittance');

    // ---- 2. multi-espalhamento -------------------------------------------
    // Integra sobre a esfera de direções o espalhamento simples com fase
    // isotrópica (L_f) e a fração reespalhada (f_ms); a soma da série
    // infinita vira L_f/(1-f_ms). Hillaire 2020, seção 4.
    this.shMs = new Shader(gl, FS_VS, HEAD + COMMON + MEDIUM + `
uniform sampler2D uTransLut;
vec3 transmittanceTo(float r, float mu){
  return texture(uTransLut, transmittanceUV(clamp(r, Rg, Rt), mu)).rgb;
}
out vec4 oCol;

#define DIRS 48
#define MSTEPS 18

void main(){
  float muS = uvToUnit(vUV.x, ${MS_RES}.0) * 2.0 - 1.0;
  float r = Rg + uvToUnit(vUV.y, ${MS_RES}.0) * (Rt - Rg);
  r = clamp(r, Rg + 1e-3, Rt - 1e-3);
  vec3 sunDir = vec3(sqrt(max(1.0 - muS * muS, 0.0)), muS, 0.0);
  vec3 origin = vec3(0.0, r, 0.0);

  vec3 Lsum = vec3(0.0), fmsSum = vec3(0.0);

  for (int i = 0; i < DIRS; i++){
    // esfera de Fibonacci: distribuição uniforme sem viés de polo
    float fi = float(i) + 0.5;
    float ct = 1.0 - 2.0 * fi / float(DIRS);
    float st = sqrt(max(1.0 - ct * ct, 0.0));
    float ph = fi * 2.39996323;              // ângulo áureo
    vec3 dir = vec3(cos(ph) * st, ct, sin(ph) * st);

    float mu = dir.y;
    float tMax = hitsGround(r, mu) ? raySphere(r, mu, Rg) : raySphere(r, mu, Rt);
    float dt = tMax / float(MSTEPS);

    vec3 L = vec3(0.0), fms = vec3(0.0), tp = vec3(1.0);
    for (int s = 0; s < MSTEPS; s++){
      float t = (float(s) + 0.5) * dt;
      vec3 p = origin + dir * t;
      float rr = length(p);
      vec3 sR, ext; float sM;
      mediumAt(rr - Rg, sR, sM, ext);
      vec3 sctr = sR + vec3(sM);
      ext = max(ext, vec3(1e-9));
      vec3 stepT = exp(-ext * dt);

      float muSl = dot(normalize(p), sunDir);
      vec3 sunT = hitsGround(rr, muSl) ? vec3(0.0) : transmittanceTo(rr, muSl);

      // integral analítica da fonte no passo (energia conservada)
      vec3 Sms = (sctr - sctr * stepT) / ext;
      fms += tp * Sms;
      vec3 S = sunT * sctr * (1.0 / (4.0 * PI));
      L += tp * (S - S * stepT) / ext;
      tp *= stepT;
    }
    // bounce do chão
    if (hitsGround(r, mu)){
      vec3 p = origin + dir * tMax;
      float muSg = dot(normalize(p), sunDir);
      if (muSg > 0.0){
        L += tp * GROUND_ALBEDO * muSg * transmittanceTo(Rg, muSg) / PI;
      }
    }
    Lsum += L / float(DIRS);
    fmsSum += fms / float(DIRS);
  }
  // soma da série geométrica de ordens de espalhamento
  oCol = vec4(Lsum / max(1.0 - fmsSum, vec3(1e-4)), 1.0);
}`, 'atmosMultiScatter');

    // ---- 3. sky-view ------------------------------------------------------
    this.shSkyView = new Shader(gl, FS_VS, HEAD + COMMON + MEDIUM + SAMPLERS + `
uniform vec3 uSunDir, uMoonDir, uMoonTint;
uniform float uSunIlluminance, uMoonIlluminance, uCamHeight;
out vec4 oCol;

#define VSTEPS 32

void main(){
  float r = Rg + max(uCamHeight, 1e-4);
  // v: 0 zênite, 0.5 horizonte, 1 nadir — distorção sqrt concentra amostras
  // perto do horizonte, que é onde o gradiente é mais forte
  float s = (0.5 - vUV.y) * 2.0;
  float tt = sign(s) * s * s;
  float ang = (0.5 - tt * 0.5) * PI;
  float mu = cos(ang);
  float sinV = sqrt(max(1.0 - mu * mu, 0.0));

  // u: azimute RELATIVO ao sol — a atmosfera é simétrica em volta dele
  float az = vUV.x * PI;
  vec3 sunH = normalize(vec3(uSunDir.x, 0.0, uSunDir.z) + vec3(1e-5, 0.0, 0.0));
  vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), sunH));
  vec3 dir = normalize(sunH * (cos(az) * sinV) + right * (sin(az) * sinV) + vec3(0.0, mu, 0.0));

  float muS = uSunDir.y;
  float cosT = dot(dir, uSunDir);
  float phR = phaseRayleigh(cosT), phM = phaseMie(cosT);
  // a Lua espalha na mesma atmosfera, com a própria fase. Ela é a antípoda do
  // Sol (celestial.js: lua cheia), então fica no mesmo plano vertical e a
  // simetria azimutal da LUT continua valendo.
  float cosTm = dot(dir, uMoonDir);
  float phRm = phaseRayleigh(cosTm), phMm = phaseMie(cosTm);

  bool ground = hitsGround(r, mu);
  float tMax = ground ? raySphere(r, mu, Rg) : raySphere(r, mu, Rt);
  vec3 origin = vec3(0.0, r, 0.0);

  vec3 L = vec3(0.0), Lm = vec3(0.0), tp = vec3(1.0);
  float prevT = 0.0;
  for (int i = 0; i < VSTEPS; i++){
    // distribuição quadrática: passos curtos perto da câmera, onde a
    // densidade (e portanto a contribuição) é maior
    float f0 = float(i) / float(VSTEPS), f1 = float(i + 1) / float(VSTEPS);
    float t0 = tMax * f0 * f0, t1 = tMax * f1 * f1;
    float dt = t1 - t0;
    if (dt <= 0.0) continue;
    vec3 p = origin + dir * (t0 + dt * 0.5);
    float rr = length(p);
    vec3 sR, ext; float sM;
    mediumAt(rr - Rg, sR, sM, ext);
    ext = max(ext, vec3(1e-9));
    vec3 stepT = exp(-ext * dt);

    float muSl = dot(normalize(p), uSunDir);
    vec3 sunT = sunTransmittance(rr, muSl);
    vec3 ms = multiScatter(rr, muSl);

    vec3 S = sunT * (sR * phR + vec3(sM * phM)) + (sR + vec3(sM)) * ms;
    float muMl = dot(normalize(p), uMoonDir);
    vec3 Sm = sunTransmittance(rr, muMl) * (sR * phRm + vec3(sM * phMm))
            + (sR + vec3(sM)) * multiScatter(rr, muMl);
    vec3 w = tp * (1.0 - stepT) / ext;
    L += w * S;
    Lm += w * Sm;
    tp *= stepT;
  }
  if (ground){
    vec3 p = origin + dir * tMax;
    float muSg = dot(normalize(p), uSunDir);
    if (muSg > 0.0) L += tp * GROUND_ALBEDO * muSg * transmittanceTo(Rg, muSg) / PI;
    float muMg = dot(normalize(p), uMoonDir);
    if (muMg > 0.0) Lm += tp * GROUND_ALBEDO * muMg * transmittanceTo(Rg, muMg) / PI;
  }
  vec3 moon = Lm * uMoonIlluminance * uMoonTint;
  // alfa = a parte do céu que vem da Lua (luminância): o passe de ambiente
  // precisa dela pra saber quanto da luz da cena é luar
  oCol = vec4(L * uSunIlluminance + moon, luma(moon));
}`, 'atmosSkyView');

    // ---- 4. LUT de ambiente (4×1) ----------------------------------------
    // Tudo que a cena precisa saber sobre a iluminação do ambiente, montado
    // na GPU. Nenhum readback: o readPixels de 1 pixel custava 21ms.
    this.shEnv = new Shader(gl, FS_VS, HEAD + COMMON + MEDIUM + SAMPLERS + `
uniform sampler2D uSkyView;
uniform vec3 uSunDir, uMoonDir;
uniform float uSunIlluminance, uMoonIlluminance, uKeyIsSun, uCamHeight;
uniform vec3 uMoonTint;
// exposição e visão noturna (texel 4) — ver celestial.js
uniform float uSunLux, uMoonPhysRatio, uGlowLux;
uniform float uExpBase, uExpRef, uExpAlpha, uPurkS;
out vec4 oCol;

vec2 skyViewUV(vec3 dir){
  vec3 sunH = normalize(vec3(uSunDir.x, 0.0, uSunDir.z) + vec3(1e-5, 0.0, 0.0));
  vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), sunH));
  vec3 dh = vec3(dir.x, 0.0, dir.z);
  float lh = length(dh);
  float az = lh > 1e-5 ? atan(dot(dh, right), dot(dh, sunH)) : 0.0;
  float u = abs(az) / PI;
  float t = clamp(dir.y, -1.0, 1.0);
  float ang = acos(t);
  float ss = 1.0 - 2.0 * (ang / PI);
  float v = 0.5 - 0.5 * sign(ss) * sqrt(abs(ss));
  return vec2(u, clamp(v, 0.0, 1.0));
}
vec3 sky(vec3 d){ return texture(uSkyView, skyViewUV(d)).rgb; }
float skyMoon(vec3 d){ return texture(uSkyView, skyViewUV(d)).a; }

// Σ L·cosθ·dω / π no hemisfério de cima, só da parte lunar (luminância)
float skyUpMoon(){
  const int N = 48;
  float acc = 0.0;
  for (int i = 0; i < N; i++){
    float fi = float(i) + 0.5;
    float ct = 1.0 - fi / float(N);
    float st = sqrt(max(1.0 - ct * ct, 0.0));
    float ph = fi * 2.39996323;
    acc += skyMoon(vec3(cos(ph) * st, ct, sin(ph) * st)) * ct;
  }
  return acc * (2.0 / float(N));
}

void main(){
  int idx = int(gl_FragCoord.x);
  float r = Rg + max(uCamHeight, 1e-4);
  vec3 sunIrr = uSunIlluminance * sunTransmittance(r, uSunDir.y) * max(uSunDir.y, 0.0);
  vec3 moonIrr = uMoonIlluminance * uMoonTint
               * sunTransmittance(r, uMoonDir.y) * max(uMoonDir.y, 0.0);

  if (idx == 0){
    // irradiância da luz-chave, na direção da própria luz-chave
    oCol = vec4(uKeyIsSun > 0.5 ? sunIrr : moonIrr, 1.0);
  } else if (idx == 4){
    // ---- exposição analítica + escala física da visão noturna ----------
    // Iluminância horizontal que a cena recebe (chave + céu), separada em
    // parte solar e lunar. A Lua do render é mais forte que a real (ver
    // MOON_ILLUMINANCE); a parte lunar volta à escala física por
    // uMoonPhysRatio, e o brilho do céu noturno (estrelas + airglow) entra
    // como piso físico — ele não ilumina o render.
    vec3 upRgb = vec3(0.0);
    {
      const int N = 48;
      for (int i = 0; i < N; i++){
        float fi = float(i) + 0.5;
        float ct = 1.0 - fi / float(N);
        float st = sqrt(max(1.0 - ct * ct, 0.0));
        float ph = fi * 2.39996323;
        upRgb += sky(vec3(cos(ph) * st, ct, sin(ph) * st)) * ct;
      }
      upRgb *= 2.0 / float(N);
    }
    float eKey = luma(uKeyIsSun > 0.5 ? sunIrr : moonIrr);
    float eRender = eKey + PI * luma(upRgb);
    float eMoon = (uKeyIsSun > 0.5 ? 0.0 : eKey) + PI * skyUpMoon();
    float eSun = max(eRender - eMoon, 0.0);
    float eLux = uSunLux * (eSun + eMoon * uMoonPhysRatio) + uGlowLux;
    eRender = max(eRender, 1e-9);
    // Exposição: compensação parcial da iluminância. Compensação total
    // (∝ 1/E) deixaria a noite igual ao dia; aqui o brilho exibido cai
    // devagar com a luz física, (E/E_ref)^α, e a noite lê como noite.
    float expo = uExpBase * (uExpRef / eRender)
               * pow(eLux / (uSunLux * uExpRef), uExpAlpha);
    // radiância do render → cd/m²: E_lux / E_render (mesma razão de π);
    // uPurkS é a escala de resposta dos cones calibrada no slide do Ghost
    oCol = vec4(expo, eLux, eRender, uPurkS * eLux / eRender);
  } else if (idx == 1 || idx == 2){
    // irradiância hemisférica do céu / π (o lóbulo difuso já divide por π)
    // Integração de Fibonacci sobre o hemisfério, ponderada por cosseno.
    const int N = 48;
    vec3 up = idx == 1 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, -1.0, 0.0);
    vec3 acc = vec3(0.0);
    for (int i = 0; i < N; i++){
      float fi = float(i) + 0.5;
      float ct = 1.0 - fi / float(N);          // hemisfério
      float st = sqrt(max(1.0 - ct * ct, 0.0));
      float ph = fi * 2.39996323;
      vec3 d = vec3(cos(ph) * st, ct, sin(ph) * st);
      if (idx == 2) d.y = -d.y;
      acc += sky(d) * ct;                      // peso cosseno
    }
    // Σ L·cosθ·dω / π, com dω = 2π/N
    vec3 res = acc * (2.0 / float(N));
    if (idx == 2){
      // o hemisfério inferior é o rebote do chão, não céu: aproxima pelo
      // albedo do solo vezes a irradiância que chega nele
      vec3 down = res * 0.35;
      vec3 bounce = GROUND_ALBEDO * (sunIrr + moonIrr + res * PI) / PI;
      res = down + bounce * 0.55;
    }
    oCol = vec4(res, 1.0);
  } else {
    // cor pura do disco solar/lunar, pro passe de céu
    oCol = vec4(uSunIlluminance * sunTransmittance(r, uSunDir.y), 1.0);
  }
}`, 'atmosEnv');

    this._buildStatic();
  }

  /** LUTs independentes do sol: calculadas uma única vez */
  _buildStatic() {
    const gl = this.gl;
    gl.disable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);
    this.shTrans.use();
    this.trans.bind(); drawFS(gl);
    this.shMs.use().tex('uTransLut', this.trans.tex);
    this.ms.bind(); drawFS(gl);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }

  /**
   * Recalcula sky-view + ambiente. Só faz trabalho se o sol/lua realmente
   * se moveram — segurar um slider de hora recalcula; ficar parado não.
   */
  update(sunDir, moonDir, p) {
    const gl = this.gl;
    const key = `${p.sunIlluminance}|${p.moonIlluminance}|${p.moonTint}|${p.keyIsSun}|${p.camHeight}`
              + `|${p.sunLux}|${p.moonPhysRatio}|${p.glowLux}|${p.expBase}|${p.expRef}|${p.expAlpha}|${p.purkS}`;
    const moved = Math.abs(sunDir[0] - this._lastSun[0]) + Math.abs(sunDir[1] - this._lastSun[1])
                + Math.abs(sunDir[2] - this._lastSun[2])
                + Math.abs(moonDir[1] - this._lastMoon[1]) > 1e-4;
    if (!moved && key === this._lastParams) return false;
    this._lastSun.set(sunDir);
    this._lastMoon.set(moonDir);
    this._lastParams = key;

    gl.disable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);
    this.shSkyView.use()
      .set('uSunDir', sunDir).set('uMoonDir', moonDir)
      .set('uSunIlluminance', p.sunIlluminance)
      .set('uMoonIlluminance', p.moonIlluminance).set('uMoonTint', p.moonTint)
      .set('uCamHeight', p.camHeight)
      .tex('uTransLut', this.trans.tex).tex('uMsLut', this.ms.tex);
    this.skyView.bind(); drawFS(gl);

    this.shEnv.use()
      .set('uSunDir', sunDir).set('uMoonDir', moonDir)
      .set('uSunIlluminance', p.sunIlluminance)
      .set('uMoonIlluminance', p.moonIlluminance)
      .set('uMoonTint', p.moonTint)
      .set('uKeyIsSun', p.keyIsSun ? 1 : 0)
      .set('uCamHeight', p.camHeight)
      .set('uSunLux', p.sunLux).set('uMoonPhysRatio', p.moonPhysRatio).set('uGlowLux', p.glowLux)
      .set('uExpBase', p.expBase).set('uExpRef', p.expRef).set('uExpAlpha', p.expAlpha)
      .set('uPurkS', p.purkS)
      .tex('uTransLut', this.trans.tex).tex('uMsLut', this.ms.tex)
      .tex('uSkyView', this.skyView.tex);
    this.envLut.bind(); drawFS(gl);
    this._envDirty = true;
    this.pollEnv();

    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    return true;
  }

  /** chamar uma vez por quadro: recolhe a leitura pronta e pede outra se a LUT mudou */
  pollEnv() {
    const gl = this.gl;
    if (this._envFence && gl.getSyncParameter(this._envFence, gl.SYNC_STATUS) === gl.SIGNALED) {
      gl.deleteSync(this._envFence);
      this._envFence = null;
      gl.bindBuffer(gl.PIXEL_PACK_BUFFER, this._envBuf);
      gl.getBufferSubData(gl.PIXEL_PACK_BUFFER, 0, this._envRead);
      gl.bindBuffer(gl.PIXEL_PACK_BUFFER, null);
      const [expo, lux, eRender, purkS] = this._envRead;
      this.envCPU = { expo, lux, eRender, purkS };
    }
    if (this._envDirty && !this._envFence) {
      this._envDirty = false;
      gl.bindFramebuffer(gl.FRAMEBUFFER, this.envLut.fbo);
      gl.bindBuffer(gl.PIXEL_PACK_BUFFER, this._envBuf);
      gl.readPixels(4, 0, 1, 1, gl.RGBA, gl.FLOAT, 0);
      gl.bindBuffer(gl.PIXEL_PACK_BUFFER, null);
      gl.bindFramebuffer(gl.FRAMEBUFFER, null);
      this._envFence = gl.fenceSync(gl.SYNC_GPU_COMMANDS_COMPLETE, 0);
    }
  }
}
