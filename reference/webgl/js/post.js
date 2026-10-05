// ---------------------------------------------------------------------------
// post.js — cadeia de post-processing.
//
// Bloom: down/upsample progressivo com filtro de 13 taps e média de Karis no
// primeiro nível (mata o "fireflies" de pixels HDR isolados). Referência:
//   Jimenez, "Next Generation Post Processing in Call of Duty: Advanced
//   Warfare", SIGGRAPH 2014.
// Streak anamórfico separado, tonemap ACES, aberração cromática radial,
// vinheta, grain e dither ordenado.
// ---------------------------------------------------------------------------

import { Shader, Target, drawFS, FS_VS } from './gl.js';
import { COMMON } from './glsl.js';

const HEAD = `#version 300 es
precision highp float;
in vec2 vUV;
`;

export class Post {
  constructor(gl, w, h, levels = 7) {
    this.gl = gl;
    this.levels = levels;
    this.mips = [];
    this.streaks = [];
    this.resize(w, h);

    const rgba = { internalFormat: gl.RGBA16F, format: gl.RGBA, type: gl.HALF_FLOAT };
    this.fmt = rgba;

    // ---- pré-filtro: knee suave + média de Karis ----------------------
    this.shPrefilter = new Shader(gl, FS_VS, HEAD + COMMON + `
uniform sampler2D uTex;
uniform vec2 uTexel;
uniform float uThreshold, uKnee, uClamp;
out vec4 oCol;

vec3 fetch(vec2 uv){ return min(texture(uTex, uv).rgb, vec3(uClamp)); }
float karisWeight(vec3 c){ return 1.0 / (1.0 + luma(c)); }

void main(){
  // 13-tap box em cruz + quadrado (padrão do COD)
  vec3 a = fetch(vUV + uTexel * vec2(-1.0,  1.0));
  vec3 b = fetch(vUV + uTexel * vec2( 1.0,  1.0));
  vec3 c = fetch(vUV + uTexel * vec2(-1.0, -1.0));
  vec3 d = fetch(vUV + uTexel * vec2( 1.0, -1.0));
  vec3 e = fetch(vUV);
  vec3 f = fetch(vUV + uTexel * vec2(-2.0,  2.0));
  vec3 g = fetch(vUV + uTexel * vec2( 0.0,  2.0));
  vec3 h = fetch(vUV + uTexel * vec2( 2.0,  2.0));
  vec3 i = fetch(vUV + uTexel * vec2(-2.0,  0.0));
  vec3 j = fetch(vUV + uTexel * vec2( 2.0,  0.0));
  vec3 k = fetch(vUV + uTexel * vec2(-2.0, -2.0));
  vec3 l = fetch(vUV + uTexel * vec2( 0.0, -2.0));
  vec3 m = fetch(vUV + uTexel * vec2( 2.0, -2.0));

  // média ponderada por Karis: pixels muito brilhantes pesam menos, o que
  // elimina o cintilar de bloom em pontos isolados
  vec3 g0 = (a+b+c+d) * 0.25, g1 = (f+g+i+e) * 0.25, g2 = (g+h+e+j) * 0.25;
  vec3 g3 = (i+e+k+l) * 0.25, g4 = (e+j+l+m) * 0.25;
  float w0 = karisWeight(g0), w1 = karisWeight(g1), w2 = karisWeight(g2);
  float w3 = karisWeight(g3), w4 = karisWeight(g4);
  float wsum = w0*0.5 + (w1+w2+w3+w4)*0.125;
  vec3 col = (g0*w0*0.5 + g1*w1*0.125 + g2*w2*0.125 + g3*w3*0.125 + g4*w4*0.125) / max(wsum, 1e-5);

  // knee quadrático em volta do threshold
  float lum = maxc(col);
  float soft = clamp(lum - uThreshold + uKnee, 0.0, 2.0 * uKnee);
  soft = soft * soft / (4.0 * uKnee + 1e-5);
  float contrib = max(soft, lum - uThreshold) / max(lum, 1e-5);
  oCol = vec4(col * contrib, 1.0);
}`, 'bloomPrefilter');

    this.shDown = new Shader(gl, FS_VS, HEAD + COMMON + `
uniform sampler2D uTex;
uniform vec2 uTexel;
out vec4 oCol;
void main(){
  vec3 a = texture(uTex, vUV + uTexel*vec2(-1.0, 1.0)).rgb;
  vec3 b = texture(uTex, vUV + uTexel*vec2( 1.0, 1.0)).rgb;
  vec3 c = texture(uTex, vUV + uTexel*vec2(-1.0,-1.0)).rgb;
  vec3 d = texture(uTex, vUV + uTexel*vec2( 1.0,-1.0)).rgb;
  vec3 e = texture(uTex, vUV).rgb;
  vec3 f = texture(uTex, vUV + uTexel*vec2(-2.0, 2.0)).rgb;
  vec3 g = texture(uTex, vUV + uTexel*vec2( 0.0, 2.0)).rgb;
  vec3 h = texture(uTex, vUV + uTexel*vec2( 2.0, 2.0)).rgb;
  vec3 i = texture(uTex, vUV + uTexel*vec2(-2.0, 0.0)).rgb;
  vec3 j = texture(uTex, vUV + uTexel*vec2( 2.0, 0.0)).rgb;
  vec3 k = texture(uTex, vUV + uTexel*vec2(-2.0,-2.0)).rgb;
  vec3 l = texture(uTex, vUV + uTexel*vec2( 0.0,-2.0)).rgb;
  vec3 m = texture(uTex, vUV + uTexel*vec2( 2.0,-2.0)).rgb;
  vec3 col = e*0.125 + (a+b+c+d)*0.125 + (f+h+k+m)*0.03125 + (g+i+j+l)*0.0625;
  oCol = vec4(col, 1.0);
}`, 'bloomDown');

    // upsample com filtro tent 3x3
    this.shUp = new Shader(gl, FS_VS, HEAD + `
uniform sampler2D uTex;
uniform vec2 uTexel;
uniform float uRadius;
out vec4 oCol;
void main(){
  vec2 o = uTexel * uRadius;
  vec3 c = texture(uTex, vUV + vec2(-o.x, -o.y)).rgb * 0.0625
         + texture(uTex, vUV + vec2( 0.0, -o.y)).rgb * 0.125
         + texture(uTex, vUV + vec2( o.x, -o.y)).rgb * 0.0625
         + texture(uTex, vUV + vec2(-o.x,  0.0)).rgb * 0.125
         + texture(uTex, vUV                   ).rgb * 0.25
         + texture(uTex, vUV + vec2( o.x,  0.0)).rgb * 0.125
         + texture(uTex, vUV + vec2(-o.x,  o.y)).rgb * 0.0625
         + texture(uTex, vUV + vec2( 0.0,  o.y)).rgb * 0.125
         + texture(uTex, vUV + vec2( o.x,  o.y)).rgb * 0.0625;
  oCol = vec4(c, 1.0);
}`, 'bloomUp');

    // streak anamórfico: blur horizontal largo em passos dobrados
    this.shStreak = new Shader(gl, FS_VS, HEAD + `
uniform sampler2D uTex;
uniform vec2 uTexel;
uniform float uStep;
out vec4 oCol;
void main(){
  vec3 c = vec3(0.0);
  float wsum = 0.0;
  for (int i = -6; i <= 6; i++){
    float fi = float(i);
    float w = exp(-fi*fi*0.09);
    c += texture(uTex, vUV + vec2(uTexel.x*uStep*fi, 0.0)).rgb * w;
    wsum += w;
  }
  oCol = vec4(c / wsum, 1.0);
}`, 'streak');

    // ---- pass final --------------------------------------------------
    this.shFinal = new Shader(gl, FS_VS, HEAD + COMMON + `
uniform sampler2D uScene, uBloom, uStreak, uAdapt, uEnvLut;
uniform vec2 uRes;
uniform float uExposure, uBloomStrength, uStreakStrength, uUseAdapt, uPurkinje;

// ---- Purkinje shift (visão mesópica/escotópica) -------------------------
// Patry, "Real-Time Samurai Cinema" (Ghost of Tsushima, SIGGRAPH 2021),
// sobre Cao et al. 2008. No escuro os bastonetes, que enxergam mais o
// azul-verde e usam as mesmas vias dos cones, somam um sinal próprio: a cena
// fica mais clara nos escuros, azulada e dessaturada; com luz o ganho dos
// cones cai e o efeito some sozinho. Matrizes geradas por tools/purkinje.py
// (cones Smith-Pokorny, V'(λ) CIE 1951, D65, espectros de Smits).
//   PURK_G: RGB → (0.33/m)(q_LMS + k q_R)    PURK_R: RGB → q_R
//   PURK_D: M̂⁻¹ A⁻¹ (K/S) B diag(k) diag(m)⁻¹
const mat3 PURK_G = mat3(0.147629, 0.0751296, 0.00385728, 0.418795, 0.691496, 0.0537957, 0.0720858, 0.145298, 0.136669);
const vec3 PURK_R = vec3(0.0107999, 0.622593, 0.39211);
const mat3 PURK_D = mat3(8.2769, 2.28736, 16.2269, -9.45385, 7.05768, 16.0246, 0.241254, -0.233032, 1.50034);
// s converte a radiância do render pra unidade de resposta dos cones (inclui
// a escala física da hora); vem do texel 4 da LUT de ambiente
vec3 purkinjeShift(vec3 c, float s){
  c = max(c, vec3(0.0));
  vec3 g = inversesqrt(1.0 + s * (PURK_G * c));
  return max(c + (PURK_D * g) * dot(PURK_R, c), vec3(0.0));
}
uniform float uCA, uVignette, uGrain, uTime, uSaturation, uContrast, uLift, uHuePreserve;
out vec4 oCol;

vec3 sampleScene(vec2 uv, float k){
  vec3 s = texture(uScene, uv).rgb;
  s += texture(uBloom, uv).rgb * uBloomStrength;
  s += texture(uStreak, uv).rgb * uStreakStrength;
  return s;
}

void main(){
  vec2 uv = vUV;
  vec2 c = uv - 0.5;
  float r2 = dot(c, c);

  // aberração cromática radial: canais amostrados em raios diferentes
  vec3 col;
  if (uCA > 0.0){
    float k = uCA * r2;
    col.r = sampleScene(uv - c * k * 1.00, 0.0).r;
    col.g = sampleScene(uv,                0.0).g;
    col.b = sampleScene(uv + c * k * 1.00, 0.0).b;
  } else {
    col = sampleScene(uv, 0.0);
  }

  // exposição analítica da hora + escala física (atmosphere.js, texel 4)
  vec4 ex = texelFetch(uEnvLut, ivec2(4, 0), 0);
  // na radiância da cena, antes da exposição: é a luz que chega no olho
  if (uPurkinje > 0.5) col = purkinjeShift(col, ex.w);

  // fator da adaptação às explosões (exposure.js): 1 = exposição analítica
  float adapt = uUseAdapt > 0.5 ? texelFetch(uAdapt, ivec2(0), 0).r : 1.0;
  col *= uExposure * ex.x * adapt;

  // ---- tonemap com preservação de matiz -------------------------------
  // ACES aplicado por canal dessatura highlights em direção ao branco: com
  // a exposição alta do crepúsculo, a bola de fogo virava uma mancha branca
  // em vez de fogo. Reaplicar parte da crominância original sobre a
  // luminância comprimida mantém o laranja mesmo estourado — é o que
  // câmera de cinema faz.
  vec3 tm = tonemapACES(col);
  {
    float lIn = max(luma(col), 1e-4);
    float lOut = luma(tm);
    vec3 chroma = col / lIn;                 // matiz+saturação originais
    vec3 preserved = clamp(chroma * lOut, 0.0, 1.0);
    col = mix(tm, preserved, uHuePreserve);
  }

  // grade: contraste em log, saturação, lift das sombras
  col = max(col, vec3(0.0));
  col = pow(col, vec3(uContrast));
  float l = luma(col);
  col = mix(vec3(l), col, uSaturation);
  col += uLift * (1.0 - saturate(l * 3.0)) * vec3(0.030, 0.042, 0.070);

  // vinheta natural (cos⁴ suavizado)
  float vig = 1.0 - uVignette * smoothstep(0.05, 0.85, r2);
  col *= vig;

  // grain: mais visível nas sombras, como filme de verdade
  // Grão uniforme. Pesar pras sombras (como antes) destrói justamente a
  // parte escura de uma cena noturna, onde o olho procura detalhe.
  float n = hash12(gl_FragCoord.xy + fract(uTime) * 1731.0) - 0.5;
  col += n * uGrain * (0.75 + 0.25 * saturate(luma(col) * 2.0));

  col = linearToSRGB(col);
  // dither ordenado: elimina banding nos gradientes de céu/fumaça
  col += (ignoise(gl_FragCoord.xy) - 0.5) / 255.0;
  oCol = vec4(col, 1.0);
}`, 'final');
  }

  resize(w, h) {
    const gl = this.gl;
    for (const t of this.mips) t.dispose();
    for (const t of this.streaks) t.dispose();
    this.mips = [];
    this.streaks = [];
    const fmt = { internalFormat: gl.RGBA16F, format: gl.RGBA, type: gl.HALF_FLOAT };
    let mw = Math.max(1, w >> 1), mh = Math.max(1, h >> 1);
    for (let i = 0; i < this.levels; i++) {
      this.mips.push(new Target(gl, mw, mh, fmt));
      mw = Math.max(1, mw >> 1);
      mh = Math.max(1, mh >> 1);
      if (mw <= 2 || mh <= 2) break;
    }
    const sw = Math.max(1, w >> 2), sh2 = Math.max(1, h >> 2);
    this.streaks.push(new Target(gl, sw, sh2, fmt));
    this.streaks.push(new Target(gl, sw, sh2, fmt));
  }

  /** constrói a pirâmide de bloom + o streak a partir da imagem HDR */
  build(sceneTex, P) {
    const gl = this.gl;
    gl.disable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);

    // pré-filtro no mip 0
    const m0 = this.mips[0];
    this.shPrefilter.use()
      .set('uTexel', [1 / m0.w, 1 / m0.h])
      .set('uThreshold', P.bloomThreshold).set('uKnee', P.bloomKnee)
      .set('uClamp', P.bloomClamp)
      .tex('uTex', sceneTex);
    m0.bind(); drawFS(gl);

    // downsample
    for (let i = 1; i < this.mips.length; i++) {
      const src = this.mips[i - 1], dst = this.mips[i];
      this.shDown.use().set('uTexel', [1 / src.w, 1 / src.h]).tex('uTex', src.tex);
      dst.bind(); drawFS(gl);
    }

    // upsample aditivo
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.ONE, gl.ONE);
    for (let i = this.mips.length - 1; i > 0; i--) {
      const src = this.mips[i], dst = this.mips[i - 1];
      this.shUp.use().set('uTexel', [1 / src.w, 1 / src.h])
        .set('uRadius', P.bloomRadius).tex('uTex', src.tex);
      dst.bind(); drawFS(gl);
    }
    gl.disable(gl.BLEND);

    // streak anamórfico: 3 passos com raio dobrando
    const [s0, s1] = this.streaks;
    this.shStreak.use().set('uTexel', [1 / s0.w, 1 / s0.h]).set('uStep', 1.0)
      .tex('uTex', this.mips[Math.min(2, this.mips.length - 1)].tex);
    s0.bind(); drawFS(gl);
    let src = s0, dst = s1;
    for (const st of [4.0, 13.0]) {
      this.shStreak.use().set('uTexel', [1 / src.w, 1 / src.h]).set('uStep', st).tex('uTex', src.tex);
      dst.bind(); drawFS(gl);
      const t = src; src = dst; dst = t;
    }
    this.streakTex = src.tex;
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }

  final(sceneTex, w, h, P, time) {
    const gl = this.gl;
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    gl.viewport(0, 0, w, h);
    this.shFinal.use()
      .set('uRes', [w, h])
      // bloom desligado: a pirâmide não é reconstruída e guarda o quadro
      // antigo, então a força tem que ir a zero aqui
      .set('uExposure', P.exposureBias ?? 1)
      .set('uPurkinje', P.purkinje === false ? 0 : 1)
      .set('uBloomStrength', P.bloomOn === false ? 0 : P.bloomStrength)
      .set('uStreakStrength', P.bloomOn === false ? 0 : P.streakStrength)
      .set('uCA', P.chromaticOn === false ? 0 : P.chromatic).set('uVignette', P.vignette)
      .set('uGrain', P.grainOn === false ? 0 : P.grain)
      .set('uSaturation', P.saturation).set('uContrast', P.contrast).set('uLift', P.lift)
      .set('uHuePreserve', P.huePreserve ?? 0.35)
      .set('uTime', time)
      .set('uUseAdapt', P.aeTex ? 1 : 0)
      .tex('uScene', sceneTex).tex('uBloom', this.mips[0].tex).tex('uStreak', this.streakTex)
      .tex('uAdapt', P.aeTex || this.mips[0].tex)
      .tex('uEnvLut', P.envLut);
    drawFS(gl);
  }
}
