// ---------------------------------------------------------------------------
// exposure.js — adaptação de exposição às EXPLOSÕES, limitada.
//
// A exposição base é analítica (pela altura do sol) e não pulsa com nada. O
// problema: à noite ela é ~8× a de dia, e uma bola de fogo grande e próxima
// estoura pra uma mancha branca sem cor nem estrutura. Uma câmera de verdade
// reage ao clarão: fecha rápido e reabre devagar.
//
// O medidor olha as superfícies (com a luz do fogo nelas) e o volume das
// explosões, nunca o céu, o sol ou a lua — é o "luminância só quando há
// altas luzes" do Ghost of Tsushima: sem explosão, a média fica muito abaixo
// do alvo e vale a exposição analítica intacta, sem bombear. A métrica é uma média de
// potência p=0.5 da luminância exposta — entre a média logarítmica do
// medidor de câmera/olho (Reinhard 2002) e a aritmética: uma explosão pequena
// e longe (poucos % da tela) quase não move a média e não apaga o campo de
// batalha; o chão inteiro iluminado pelo fogo move muito.
//
//   fator alvo = clamp(alvo / M, 2^-EV, 1)        M = (média(L^p))^(1/p)
//
// O alvo fica acima de qualquer cena sem explosão (0.03–0.29 medido) e no
// nível de um chão iluminado de perto por uma bola de fogo: ~1 stop acima do
// meio-dia, que é o que a física dá (~160 klux a 8 m contra 84 klux).
//
// Tudo na GPU: medidor 64×64 → mipmap até 1×1 → adaptação temporal num
// texel (ping-pong) → o passe final lê o fator. Nenhuma leitura pra CPU.
// ---------------------------------------------------------------------------

import { Shader, Target, PingPong, drawFS, FS_VS } from './gl.js';
import { COMMON } from './glsl.js';

const METER = 64;
const LEVELS = 7;   // 64 → 1

export class AutoExposure {
  constructor(gl) {
    this.gl = gl;
    // medidor com mipmaps: o nível 6 (1×1) é a média do quadro
    this.meter = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, this.meter);
    gl.texStorage2D(gl.TEXTURE_2D, LEVELS, gl.RGBA16F, METER, METER);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST_MIPMAP_NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    this.meterFbo = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.meterFbo);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, this.meter, 0);

    // fator adaptado (1 = exposição analítica pura), começa em 1
    const f16 = { internalFormat: gl.RGBA16F, format: gl.RGBA, type: gl.HALF_FLOAT, filter: gl.NEAREST };
    this.adapt = new PingPong(gl, 1, 1, f16);
    for (const t of [this.adapt.read, this.adapt.write]) {
      t.bind();
      gl.clearColor(1, 1, 1, 1);
      gl.clear(gl.COLOR_BUFFER_BIT);
    }
    gl.clearColor(0, 0, 0, 0);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);

    const HEAD = `#version 300 es\nprecision highp float;\nprecision highp sampler2D;\nin vec2 vUV;\n`;
    this.shMeter = new Shader(gl, FS_VS, HEAD + COMMON + `
uniform sampler2D uVol, uComp, uDepth, uEnvLut;
uniform float uBias, uP, uClampL;
out vec4 oCol;
void main(){
  // exposição analítica da hora (atmosphere.js, texel 4)
  float uExp = texelFetch(uEnvLut, ivec2(4, 0), 0).r * uBias;
  // 4×4 amostras bilineares por texel do medidor (cada uma já média 2×2)
  vec2 base = floor(gl_FragCoord.xy) / ${METER}.0;
  float acc = 0.0;
  for (int j = 0; j < 4; j++)
    for (int i = 0; i < 4; i++){
      vec2 uv = base + (vec2(float(i), float(j)) + 0.5) / (4.0 * ${METER}.0);
      // superfícies: a imagem composta inteira (o chão que o fogo ilumina
      // conta); céu: só o que o volume soma na frente dele — sol, lua e
      // céu ficam fora, quem fecha a câmera é a explosão e a luz dela
      bool sky = texture(uDepth, uv).r >= 0.99999;
      vec3 c = sky ? texture(uVol, uv).rgb : texture(uComp, uv).rgb;
      float L = min(luma(c) * uExp, uClampL);
      acc += pow(L, uP);
    }
  oCol = vec4(acc / 16.0, 0.0, 0.0, 1.0);
}`, 'aeMeter');

    this.shAdapt = new Shader(gl, FS_VS, HEAD + COMMON + `
uniform sampler2D uMeter, uPrev, uEnvLut;
uniform float uLevel, uTarget, uDt, uAttack, uRelease, uP, uEnable;
uniform float uExpDay, uMaxEV;
out vec4 oCol;
void main(){
  float m = textureLod(uMeter, vec2(0.5), uLevel).r;
  float M = pow(max(m, 0.0), 1.0 / uP);
  // Quanto ela pode fechar: de dia 0.5 EV; à noite, até a exposição do dia
  // (+0.5) — uma cena iluminada pelo fogo está tão clara quanto o dia, e é
  // assim que o olho adaptado a ela a veria.
  float expo = texelFetch(uEnvLut, ivec2(4, 0), 0).r;
  float uMinF = exp2(-clamp(0.5 + log2(max(expo, 1e-6) / uExpDay), 0.5, uMaxEV));
  float target = uEnable > 0.5 ? clamp(uTarget / max(M, 1e-4), uMinF, 1.0) : 1.0;
  float prev = texelFetch(uPrev, ivec2(0), 0).r;
  if (!(prev > 0.0 && prev <= 1.0)) prev = 1.0;     // primeiro quadro / NaN
  // fecha rápido, reabre devagar — em log2 (passo perceptual uniforme)
  float tau = target < prev ? uAttack : uRelease;
  float k = 1.0 - exp(-uDt / max(tau, 1e-3));
  float f = exp2(mix(log2(prev), log2(target), k));
  oCol = vec4(f, M, target, 1.0);
}`, 'aeAdapt');
  }

  /**
   * @param volTex  radiância dos volumes (rgb), antes da composição
   * @param compTex imagem composta (cena + volumes + névoa), antes do post
   * @param depthTex profundidade da cena (1 = céu)
   * @param envLut  LUT de ambiente: o texel 4 tem a exposição analítica e a
   *                iluminância física da hora (atmosphere.js)
   * @param dt      segundos desde o quadro anterior
   */
  update(volTex, compTex, depthTex, envLut, dt, P) {
    const gl = this.gl;
    gl.disable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);
    gl.disable(gl.SCISSOR_TEST);
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.meterFbo);
    gl.drawBuffers([gl.COLOR_ATTACHMENT0]);
    gl.viewport(0, 0, METER, METER);
    this.shMeter.use().set('uBias', P.exposureBias ?? 1).set('uP', P.aePower).set('uClampL', P.aeClamp)
      .tex('uVol', volTex).tex('uComp', compTex).tex('uDepth', depthTex).tex('uEnvLut', envLut);
    drawFS(gl);
    gl.bindTexture(gl.TEXTURE_2D, this.meter);
    gl.generateMipmap(gl.TEXTURE_2D);

    this.adapt.write.bind();
    this.shAdapt.use()
      .set('uLevel', LEVELS - 1).set('uTarget', P.aeTarget)
      .set('uDt', Math.max(dt, 0))
      .set('uAttack', P.aeAttack).set('uRelease', P.aeRelease)
      .set('uP', P.aePower).set('uEnable', P.autoExposure === false ? 0 : 1)
      .set('uExpDay', P.aeExpDay).set('uMaxEV', P.aeMaxEV)
      .tex('uMeter', this.meter).tex('uPrev', this.adapt.read.tex).tex('uEnvLut', envLut);
    drawFS(gl);
    this.adapt.swap();
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }

  /** volta o fator a 1 (exposição analítica pura) */
  reset() {
    const gl = this.gl;
    for (const t of [this.adapt.read, this.adapt.write]) {
      t.bind();
      gl.clearColor(1, 1, 1, 1);
      gl.clear(gl.COLOR_BUFFER_BIT);
    }
    gl.clearColor(0, 0, 0, 0);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }

  /** textura 1×1: r = fator de exposição, g = medida M, b = fator alvo */
  get tex() { return this.adapt.read.tex; }
}
