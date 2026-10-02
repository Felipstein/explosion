// ---------------------------------------------------------------------------
// ssao.js — oclusão de ambiente em espaço de tela.
//
// Estimador tipo Alchemy/SAO (McGuire et al., HPG 2011/2012): amostra pontos
// numa espiral sobre o hemisfério orientado pela normal, reprojeta em tela,
// lê a profundidade e mede o quanto cada vizinho se eleva acima do plano
// tangente. É robusto a escala e não precisa de kernel pré-gerado.
//
// A cena já escrevia normal + fração-de-ambiente num MRT desde o começo e
// ninguém consumia. A oclusão multiplica SÓ a parcela indireta — aplicar no
// resultado final escureceria a luz direta, que é fisicamente errado e dá
// aquele aspecto de "sujeira cinza" em volta dos objetos.
// ---------------------------------------------------------------------------

import { Shader, Target, drawFS, FS_VS } from './gl.js';
import { COMMON } from './glsl.js';

const HEAD = `#version 300 es
precision highp float;
in vec2 vUV;
`;

export const SSAO_DEFAULTS = {
  // Raio PEQUENO. Com 2.6m quase toda amostra cai em chão vazio numa cena
  // aberta e o AO fica branco. Oclusão de contato — o sinal que apoia o
  // objeto no chão — vive nos primeiros centímetros do encontro.
  radius: 0.70,
  intensity: 6.00,
  bias: 0.012,
  samples: 12,
  blurDepthTol: 0.55,
  scale: 0.75,       // resolução relativa
};

export class SSAO {
  constructor(gl) {
    this.gl = gl;
    this.params = { ...SSAO_DEFAULTS };

    this.shAO = new Shader(gl, FS_VS, HEAD + COMMON + `
uniform sampler2D uDepth, uNormal;
uniform mat4 uInvViewProj, uViewProj;
uniform vec3 uCamPos;
uniform float uRadius, uIntensity, uBias, uJitter, uFar;
uniform vec2 uRes;
out vec4 oCol;

vec3 worldAt(vec2 uv, out float rawD){
  rawD = texture(uDepth, uv).r;
  vec4 h = uInvViewProj * vec4(uv * 2.0 - 1.0, rawD * 2.0 - 1.0, 1.0);
  return h.xyz / h.w;
}

void main(){
  float d0;
  vec3 P = worldAt(vUV, d0);
  if (d0 >= 0.999999){ oCol = vec4(1.0); return; }      // céu não ocluí

  // Normal GEOMÉTRICA, derivada da própria profundidade. Usar a normal
  // mapeada do G-buffer descasa da geometria que o teste de profundidade
  // enxerga, e o AO passa a marcar cada rachadura do normal map como se
  // fosse oclusão real.
  vec2 tx = 1.0 / uRes;
  float dxa, dxb, dya, dyb;
  vec3 Pxa = worldAt(vUV + vec2(tx.x, 0.0), dxa);
  vec3 Pxb = worldAt(vUV - vec2(tx.x, 0.0), dxb);
  vec3 Pya = worldAt(vUV + vec2(0.0, tx.y), dya);
  vec3 Pyb = worldAt(vUV - vec2(0.0, tx.y), dyb);
  // escolhe o lado com menor salto de profundidade: não atravessa silhuetas
  vec3 ddx = abs(dxa - d0) < abs(dxb - d0) ? (Pxa - P) : (P - Pxb);
  vec3 ddy = abs(dya - d0) < abs(dyb - d0) ? (Pya - P) : (P - Pyb);
  vec3 N = normalize(cross(ddx, ddy));
  if (dot(N, uCamPos - P) < 0.0) N = -N;

  // raio encolhe com a distância pra não virar oclusão de paisagem inteira
  float dist = length(P - uCamPos);
  // cresce um pouco com a distância pra o contato não sumir de longe
  float r = uRadius * clamp(dist / 12.0, 0.7, 3.0);

  vec3 T = normalize(abs(N.y) < 0.98 ? cross(vec3(0.0, 1.0, 0.0), N) : vec3(1.0, 0.0, 0.0));
  vec3 B = cross(N, T);
  float ang = ignoise(gl_FragCoord.xy + uJitter * 37.0) * TAU;

  float occ = 0.0;
  const int NS = 12;
  for (int i = 0; i < NS; i++){
    float fi = (float(i) + 0.5) / float(NS);
    float theta = fi * TAU * 5.0 + ang;      // espiral: cobre raio e ângulo
    float rad = r * (0.18 + 0.82 * sqrt(fi));   // evita amostras degeneradas
    vec3 S = P + (T * cos(theta) + B * sin(theta)) * rad + N * (rad * 0.30);

    vec4 clip = uViewProj * vec4(S, 1.0);
    if (clip.w <= 0.0) continue;
    vec2 suv = clip.xy / clip.w * 0.5 + 0.5;
    if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0) continue;

    float dS;
    vec3 Q = worldAt(suv, dS);
    if (dS >= 0.999999) continue;

    vec3 dv = Q - P;
    float l = length(dv);
    if (l < 1e-4) continue;
    // atenuação por alcance: geometria muito além do raio não deve ocluir
    float range = r / (r + l * l / max(r, 1e-3));
    occ += max(0.0, dot(dv / l, N) - uBias) * range;
  }
  oCol = vec4(saturate(1.0 - occ * uIntensity / float(NS)));
}`, 'ssao');

    // desfoque bilateral: borra o ruído da espiral sem vazar entre
    // superfícies de profundidades diferentes
    this.shBlur = new Shader(gl, FS_VS, HEAD + COMMON + `
uniform sampler2D uAO, uDepth;
uniform vec2 uTexel;
uniform vec2 uDir;
uniform float uDepthTol;
out vec4 oCol;
void main(){
  float dC = texture(uDepth, vUV).r;
  float sum = 0.0, wsum = 0.0;
  for (int i = -3; i <= 3; i++){
    vec2 uv = vUV + uDir * uTexel * float(i);
    float d = texture(uDepth, uv).r;
    float w = exp(-float(i * i) * 0.18)
            * exp(-abs(d - dC) * 2400.0 / max(uDepthTol, 1e-3));
    sum += texture(uAO, uv).r * w;
    wsum += w;
  }
  oCol = vec4(sum / max(wsum, 1e-5));
}`, 'ssaoBlur');
  }

  resize(fullW, fullH) {
    const gl = this.gl;
    const s = this.params.scale;
    this.w = Math.max(2, Math.round(fullW * s));
    this.h = Math.max(2, Math.round(fullH * s));
    if (this.a) { this.a.dispose(); this.b.dispose(); }
    const fmt = { internalFormat: gl.R8, format: gl.RED, type: gl.UNSIGNED_BYTE };
    this.a = new Target(gl, this.w, this.h, fmt);
    this.b = new Target(gl, this.w, this.h, fmt);
  }

  render(cam, depthTex, normalTex, jitter) {
    const gl = this.gl, P = this.params;
    gl.disable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);

    this.shAO.use()
      .set('uInvViewProj', cam.invViewProj).set('uViewProj', cam.viewProj)
      .set('uCamPos', cam.pos).set('uFar', cam.far)
      .set('uRadius', P.radius).set('uIntensity', P.intensity)
      .set('uBias', P.bias).set('uJitter', jitter).set('uRes', [this.w, this.h])
      .tex('uDepth', depthTex).tex('uNormal', normalTex);
    this.a.bind(); drawFS(gl);

    const texel = [1 / this.w, 1 / this.h];
    this.shBlur.use().set('uTexel', texel).set('uDir', [1, 0])
      .set('uDepthTol', P.blurDepthTol).tex('uAO', this.a.tex).tex('uDepth', depthTex);
    this.b.bind(); drawFS(gl);
    this.shBlur.use().set('uTexel', texel).set('uDir', [0, 1])
      .set('uDepthTol', P.blurDepthTol).tex('uAO', this.b.tex).tex('uDepth', depthTex);
    this.a.bind(); drawFS(gl);

    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    return this.a.tex;
  }
}
