// ---------------------------------------------------------------------------
// shadowdenoise.js — desfoque bilateral do fator de sombra dos cubos de fogo.
//
// A bola de fogo é uma fonte de metros de raio: a penumbra das sombras dela é
// larguíssima, e o PCSS com algumas dezenas de amostras deixa granulado — sem
// TAA, nada acumula. Pré-filtrar o mapa (VSM) não serve aqui: com o kernel
// desse tamanho o chão mais distante puxa a média dos momentos pra trás do
// receptor e a sombra some.
//
// Então o fator é filtrado em espaço de tela, como a "light attenuation
// buffer" do Unreal: a cena grava, por cubo, a luz que ele multiplica (RGB) e
// o fator ruidoso (A); aqui o fator é borrado sem atravessar bordas de
// profundidade ou de normal; o composite troca um pelo outro:
//
//   cena += L · (fator_filtrado − fator_ruidoso)
//
// Exato na cor: só a parcela da luz do fogo muda.
// ---------------------------------------------------------------------------

import { Shader, Target, drawFS, FS_VS } from './gl.js';
import { COMMON } from './glsl.js';

const HEAD = `#version 300 es
precision highp float;
in vec2 vUV;
`;

export class ShadowDenoise {
  constructor(gl) {
    this.gl = gl;
    // gaussiana separável de raio 6 px (σ ≈ 3.5): com 16 amostras por pixel o
    // desvio do fator cai ~9× (≈ 77 vizinhos efetivos)
    this.shBlur = new Shader(gl, FS_VS, HEAD + COMMON + `
uniform sampler2D uSrc0, uSrc1, uDepth, uNrm;
uniform vec2 uTexel, uDir;
uniform float uNear, uFar, uFirst;
out vec4 oCol;
float linZ(float d){ return uNear * uFar / (uFar - d * (uFar - uNear)); }
vec2 fetch(vec2 uv){
  return uFirst > 0.5 ? vec2(texture(uSrc0, uv).a, texture(uSrc1, uv).a)
                      : texture(uSrc0, uv).rg;
}
void main(){
  float zC = linZ(texture(uDepth, vUV).r);
  vec3 nC = texture(uNrm, vUV).rgb * 2.0 - 1.0;
  vec2 sum = vec2(0.0); float wsum = 0.0;
  for (int i = -6; i <= 6; i++){
    vec2 uv = vUV + uDir * uTexel * float(i);
    float z = linZ(texture(uDepth, uv).r);
    vec3 n = texture(uNrm, uv).rgb * 2.0 - 1.0;
    // mesma superfície: profundidade a menos de ~3% e normal parecida
    float dz = (z - zC) / (0.03 * zC);
    float w = exp(-float(i * i) / (2.0 * 3.5 * 3.5)) * exp(-dz * dz)
            * pow(saturate(dot(n, nC)), 8.0);
    sum += fetch(uv) * w;
    wsum += w;
  }
  oCol = vec4(sum / max(wsum, 1e-5), 0.0, 1.0);
}`, 'fireShadowBlur');
  }

  resize(w, h) {
    const gl = this.gl;
    this.w = w; this.h = h;
    if (this.a) { this.a.dispose(); this.b.dispose(); }
    const fmt = { internalFormat: gl.RG8, format: gl.RG, type: gl.UNSIGNED_BYTE };
    this.a = new Target(gl, w, h, fmt);
    this.b = new Target(gl, w, h, fmt);
  }

  /** @returns textura RG com os fatores filtrados dos dois cubos */
  render(cam, fire0Tex, fire1Tex, depthTex, nrmTex) {
    const gl = this.gl;
    gl.disable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);
    const texel = [1 / this.w, 1 / this.h];
    this.shBlur.use().set('uTexel', texel).set('uDir', [1, 0])
      .set('uNear', cam.near).set('uFar', cam.far).set('uFirst', 1)
      .tex('uSrc0', fire0Tex).tex('uSrc1', fire1Tex).tex('uDepth', depthTex).tex('uNrm', nrmTex);
    this.b.bind(); drawFS(gl);
    this.shBlur.use().set('uTexel', texel).set('uDir', [0, 1])
      .set('uNear', cam.near).set('uFar', cam.far).set('uFirst', 0)
      .tex('uSrc0', this.b.tex).tex('uSrc1', this.b.tex).tex('uDepth', depthTex).tex('uNrm', nrmTex);
    this.a.bind(); drawFS(gl);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    return this.a.tex;
  }
}
