// ---------------------------------------------------------------------------
// scene.js — cena rasterizada: chão + props instanciados, PBR, três fontes
// de sombra (shadow map do sol, shadow map dinâmico do fogo, e sombra
// volumétrica marchada pelo volume de fumaça), céu procedural e névoa
// analítica de altura cuja cor é a MESMA função do céu.
//
// Saída em MRT: 0 = radiância HDR, 1 = (normal.xyz, fração de ambiente),
// pra que o passe de AO possa escurecer só a parcela indireta.
// ---------------------------------------------------------------------------

import { Shader, Target, drawFS, FS_VS, createTexture, enableMipAniso } from './gl.js';
import { COMMON, ATMOS, ENVLUT, PBR, VOLUME_SHADOW } from './glsl.js';
import { Mesh, boxMesh, cylinderMesh, planeMesh } from './geometry.js';
import { buildLayout } from './layout.js';
import { m4 } from './math.js';
// A cena sombreia só as mais próximas: cada explosão custa 2 samplers e o
// limite é 16 unidades de textura no fragment shader.
import { MAX_SHADED as MAX_BLASTS } from './blastpool.js';

const HEAD = `#version 300 es
precision highp float;
precision highp sampler2D;
precision highp sampler2DArray;
precision highp samplerCube;   // o cubo de sombra guarda distância em R32F
`;

// 8 taps em vez de 12: com rotação por pixel + dither o ruído residual some
// no grain, e são 8 fetches de depth a menos em cada uma das DUAS luzes.
const POISSON = `
const vec2 POISSON[8] = vec2[8](
  vec2(-0.326, -0.406), vec2(-0.840, -0.074), vec2(-0.696,  0.457),
  vec2(-0.203,  0.621), vec2( 0.962, -0.195), vec2( 0.473, -0.480),
  vec2( 0.519,  0.767), vec2( 0.185, -0.893));
`;

// Mapa de sombra em CUBO pra luz de explosão (luz pontual/extensa).
// Antes era uma perspectiva de 145° olhando pra baixo: com a bola de fogo a
// 2.3 m do chão ela só cobria ~7 m em volta (2.3·tan 72.5°) e o resto ficava
// "sem sombra" — a sombra nascia no pé do pilar e só se esticava conforme o
// fogo subia, bem atrasada. A bola de fogo nasce rente ao chão: o caso em que
// a luz precisa enxergar o horizonte. O cubo cobre a esfera inteira.
// Guarda DISTÂNCIA RADIAL (R32F), então a comparação é exata em qualquer face.
// A penumbra larga da fonte extensa (PCSS) deixa ruído; ele é filtrado em
// espaço de tela (shadowdenoise.js).
const CUBE_FACES = [
  [[1, 0, 0], [0, -1, 0]], [[-1, 0, 0], [0, -1, 0]],
  [[0, 1, 0], [0, 0, 1]], [[0, -1, 0], [0, 0, -1]],
  [[0, 0, 1], [0, -1, 0]], [[0, 0, -1], [0, -1, 0]],
];
const MAX_MOVER_PARTS = 128;
const CUBE_SIZE = 512, CUBE_NEAR = 0.05, CUBE_FAR = 160.0;
class CubeShadow {
  constructor(gl, size = CUBE_SIZE) {
    this.gl = gl;
    this.size = size;
    this.tex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_CUBE_MAP, this.tex);
    gl.texStorage2D(gl.TEXTURE_CUBE_MAP, 1, gl.R32F, size, size);
    gl.texParameteri(gl.TEXTURE_CUBE_MAP, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_CUBE_MAP, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    for (const w of ['TEXTURE_WRAP_S', 'TEXTURE_WRAP_T', 'TEXTURE_WRAP_R']) {
      gl.texParameteri(gl.TEXTURE_CUBE_MAP, gl[w], gl.CLAMP_TO_EDGE);
    }
    this.depth = gl.createRenderbuffer();
    gl.bindRenderbuffer(gl.RENDERBUFFER, this.depth);
    gl.renderbufferStorage(gl.RENDERBUFFER, gl.DEPTH_COMPONENT24, size, size);
    this.fbo = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.fbo);
    gl.framebufferRenderbuffer(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.RENDERBUFFER, this.depth);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    this.proj = m4.perspective(m4.create(), Math.PI / 2, 1, CUBE_NEAR, CUBE_FAR);
    this.vp = m4.create();
  }
}

/** shadow map: FBO só com profundidade. `tiles` > 1 = atlas horizontal de
 *  vários mapas do mesmo tamanho (um sampler só pra todos) */
class ShadowMap {
  constructor(gl, size, tiles = 1) {
    this.gl = gl;
    this.size = size;
    this.tiles = tiles;
    this.vps = Array.from({ length: tiles }, () => m4.create());
    this.tex = createTexture(gl, size * tiles, size, {
      internalFormat: gl.DEPTH_COMPONENT24, format: gl.DEPTH_COMPONENT,
      type: gl.UNSIGNED_INT, filter: gl.NEAREST,
    });
    this.fbo = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.fbo);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.TEXTURE_2D, this.tex, 0);
    gl.drawBuffers([gl.NONE]);
    if (gl.checkFramebufferStatus(gl.FRAMEBUFFER) !== gl.FRAMEBUFFER_COMPLETE) {
      throw new Error('FBO de shadow map incompleto');
    }
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    this.vp = m4.create();
  }
  bind() {
    const gl = this.gl;
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.fbo);
    gl.viewport(0, 0, this.size * this.tiles, this.size);
    gl.clear(gl.DEPTH_BUFFER_BIT);
    gl.viewport(0, 0, this.size, this.size);
  }
  /** viewport de um tile (depois de bind) */
  tile(k) { this.gl.viewport(k * this.size, 0, this.size, this.size); }
}

export class Scene {
  constructor(gl, grid, macroGrid, bake, seed = 20250819) {
    this.gl = gl;
    this.grid = grid;
    const L = buildLayout(seed);
    this.meshBoxes = new Mesh(gl, boxMesh(), L.boxes);
    this.meshCyls = new Mesh(gl, cylinderMesh(22), L.cyls);
    this.meshGround = new Mesh(gl, planeMesh(1400, 1), L.ground);
    // o que passa pelo campo (aviões, mísseis, projéteis): instâncias
    // reescritas a cada quadro (ver movers.js)
    this.meshMovBoxes = new Mesh(gl, boxMesh(), new Float32Array(16 * MAX_MOVER_PARTS));
    this.meshMovCyls = new Mesh(gl, cylinderMesh(14), new Float32Array(16 * MAX_MOVER_PARTS));
    this.meshMovBoxes.instCount = this.meshMovCyls.instCount = 0;
    // textura 1×1 vazia pros slots de explosão ao vivo sem ninguém (ver render)
    this._emptyTex = createTexture(gl, 1, 1, {
      internalFormat: gl.RGBA8, format: gl.RGBA, type: gl.UNSIGNED_BYTE,
      filter: gl.NEAREST, data: new Uint8Array(4),
    });
    this.sunSM = new ShadowMap(gl, 2048);
    // Sombras das luzes de explosão: 2 cubos COMPARTILHADOS entre a explosão
    // ao vivo e as do clique — as 2 luzes mais fortes do quadro levam. Um
    // cubo por luz estouraria o limite de 16 texturas do shader da cena.
    this.fireCubes = [new CubeShadow(gl), new CubeShadow(gl)];
    this.groundTile = 18.0;      // metros cobertos por um tile
    this._bakeGround(1024);

    // sufixos K/Q pra sequência assada: 'M' já é a macro do solver ao vivo
    const P = grid.glsl() + macroGrid.glsl('M') + bake.glsl('K', 'Q');

    // Array de sampler exige índice CONSTANTE neste driver (índice de laço
    // não conta), então a consulta a cada explosão é desenrolada aqui.
    const occ = (dir, maxd, se, de, st, cap) => Array.from({ length: MAX_BLASTS }, (_, i) =>
      `(uBlastCount > ${i} ? volumeShadowAt(uFieldsArr[${i}], uMacroArr[${i}], uBlastOrigin[${i}],`
      + ` vWP + N * 0.05, ${dir}, ${maxd}, ${se}, ${de}, ${st}, jit, ${cap}, uErodeMean) : 1.0)`
    ).join(' * ');
    const sunOccChain = occ('uKeyDir', '140.0', 'uSootExt', 'uDustExt', '12', '7.0');
    const fireOccChain = occ('Ld', 'dist * 0.90', 'uSootExt * uFireOcclude',
                             'uDustExt * uFireOcclude', '14', 'uFireTauCap');

    // ---- VS comum: instância com quaternion ----------------------------
    const INST_VS = `
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec4 iA;   // pos.xyz, seed
layout(location=3) in vec4 iB;   // scale.xyz, matId
layout(location=4) in vec4 iC;   // tint.rgb, rough
layout(location=5) in vec4 iD;   // quat

vec3 qrot(vec4 q, vec3 v){ return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v); }
`;

    // ---- passe de profundidade (shadow maps) ---------------------------
    this.shDepth = new Shader(gl, HEAD + INST_VS + `
uniform mat4 uVP;
void main(){
  vec3 lp = aPos * iB.xyz;
  vec3 wp = qrot(iD, lp) + iA.xyz;
  gl_Position = uVP * vec4(wp, 1.0);
}`, HEAD + `void main(){}`, 'depth');

    // distância radial até a luz, pros cubos de sombra
    this.shDist = new Shader(gl, HEAD + INST_VS + `
uniform mat4 uVP;
out vec3 vWP;
void main(){
  vec3 lp = aPos * iB.xyz;
  vWP = qrot(iD, lp) + iA.xyz;
  gl_Position = uVP * vec4(vWP, 1.0);
}`, HEAD + `
in vec3 vWP;
uniform vec3 uLightPos;
out vec4 oDist;
void main(){ oDist = vec4(length(vWP - uLightPos), 0.0, 0.0, 1.0); }`, 'cubeDist');

    // ---- céu ------------------------------------------------------------
    this.shSky = new Shader(gl, FS_VS, HEAD + `in vec2 vUV;\n` + COMMON + ENVLUT + ATMOS + `
uniform mat4 uInvViewProj;
uniform vec3 uCamPos;
layout(location=0) out vec4 oCol;
layout(location=1) out vec4 oNrm;
layout(location=2) out vec4 oF0;   // luz do fogo que o cubo 0 multiplica · fator ruidoso
layout(location=3) out vec4 oF1;
void main(){
  vec4 h = uInvViewProj * vec4(vUV * 2.0 - 1.0, 1.0, 1.0);
  vec3 dir = normalize(h.xyz / h.w - uCamPos);
  oCol = vec4(skyRadiance(dir) + celestialDisks(dir), 1.0);
  oNrm = vec4(0.0, 0.0, 0.0, 0.0);   // ambFrac = 0: o céu não recebe AO
  oF0 = vec4(0.0, 0.0, 0.0, 1.0);     // sem luz de fogo, fator 1
  oF1 = vec4(0.0, 0.0, 0.0, 1.0);
}`, 'sky');

    // ---- passe principal da cena ---------------------------------------
    this.shScene = new Shader(gl, HEAD + INST_VS + `
uniform mat4 uViewProj;
out vec3 vWP;
out vec3 vN;
out vec3 vLP;
out vec4 vB;
out vec4 vC;
out float vSeed;
void main(){
  vec3 lp = aPos * iB.xyz;
  vWP = qrot(iD, lp) + iA.xyz;
  vN = normalize(qrot(iD, aNrm / max(abs(iB.xyz), vec3(1e-4))));
  vLP = aPos;
  vB = iB; vC = iC; vSeed = iA.w;
  gl_Position = uViewProj * vec4(vWP, 1.0);
}`, HEAD + `
in vec3 vWP; in vec3 vN; in vec3 vLP; in vec4 vB; in vec4 vC; in float vSeed;
` + COMMON + ENVLUT + ATMOS + PBR + P + VOLUME_SHADOW + POISSON + `
uniform vec3 uCamPos;
uniform mat4 uSunVP;
uniform sampler2D uSunShadow, uGroundA, uGroundN;
// cubos de sombra das luzes de explosão (ver CubeShadow)
uniform samplerCube uFireCube0, uFireCube1;
uniform vec3 uCubePos[2];
uniform float uCubeRad[2];      // raio da fonte extensa (m)
uniform int uCubeKind[2];       // 0 vazio · 1 explosão ao vivo (luz 0) · 2 luz de instância
uniform int uCubeIdx[2];        // índice da luz de instância
uniform float uSMDebug;         // depuração: 1 = pinta o fator de sombra do cubo 0
// Uma entrada por explosão ativa. Em GLSL ES 3.0 índice de laço com limites
// constantes conta como constant-index-expression, então indexar array de
// sampler assim é legal.
#define MAX_BLASTS ${MAX_BLASTS}
uniform sampler2D uFieldsArr[MAX_BLASTS];
uniform sampler2D uMacroArr[MAX_BLASTS];
uniform vec3 uBlastOrigin[MAX_BLASTS];
uniform vec3 uFirePosArr[MAX_BLASTS];
uniform vec3 uFireColArr[MAX_BLASTS];
uniform int uBlastCount;

// Luzes das explosões INSTANCIADAS. Elas não têm volume próprio pra sombrear
// (todas compartilham a mesma sequência assada), então entram como luzes
// pontuais sem sombra volumétrica — barato e é o que ilumina o terreno numa
// barragem de dezenas de explosões.
#define MAX_INST_LIGHTS 8
uniform vec3 uInstLightPos[MAX_INST_LIGHTS];
uniform vec3 uInstLightCol[MAX_INST_LIGHTS];
// escala² da magnitude: a bola de fogo é uma fonte EXTENSA de raio ∝ s
uniform float uInstLightS2[MAX_INST_LIGHTS];
uniform int uInstLightCount;

// Sombra volumétrica das INSTÂNCIAS. Todas compartilham o mesmo texture
// array, então isto custa 2 samplers no total — independente de quantas
// explosões existam na tela.
#define MAX_INST_SHADOW 4
uniform vec4 uInstShX[MAX_INST_SHADOW];     // posição.xyz, escala
uniform float uInstShF[MAX_INST_SHADOW];    // quadro (já com base da variante)
uniform float uInstShK[MAX_INST_SHADOW];    // fração da fumaça que ainda é da explosão (o resto foi entregue)
uniform int uInstShId[MAX_INST_SHADOW];
uniform int uInstLightId[MAX_INST_LIGHTS];
uniform int uInstShCount;
// a fumaça de UMA explosão bloqueia a luz de OUTRA? É luzes × oclusores
// marchas por pixel (~280 amostras com 8×4) e quase não aparece: opção.
uniform float uInstLightOcc;

float bakeShadow(vec3 wp, vec3 dir, float maxDist, vec4 xf, float frame,
                 float se, float de, float tauCap){
  vec3 lo = (wp - xf.xyz) / xf.w;          // espaço local da instância
  float lmax = maxDist / xf.w;
  vec2 hit = rayBox(lo, dir, BASE_MINK, BASE_MINK + DOMAIN_SIZEK);
  hit.x = max(hit.x, 0.0);
  hit.y = min(hit.y, lmax);
  if (hit.y <= hit.x) return 1.0;
  float dt = (hit.y - hit.x) / 10.0;
  float tau = 0.0;
  float t = hit.x + dt * 0.5;
  for (int i = 0; i < 20; i++){
    if (t >= hit.y) break;
    vec3 lp = lo + dir * t;
    if (bakeMacroAtQ(worldToVoxelAtQ(lp, vec3(0.0)), frame) < 0.004){
      t += bakeMacroExitQ(lp, dir) + 1e-3;
      continue;
    }
    vec4 f = sampleBake4K(worldToVoxelAtK(lp, vec3(0.0)), frame);
    tau += (se * f.r + de * f.b) * dt * xf.w;
    if (tau > tauCap) break;
    t += dt;
  }
  return exp(-min(tau, tauCap));
}
uniform float uGroundTile;
uniform vec3 uKeyDir;
uniform float uSunSMTexel;
uniform float uSootExt, uDustExt, uFireOcclude, uFireTauCap, uFireFill, uErodeMean;
uniform vec4 uScorch[16];   // x, z, raio, intensidade — um por explosão
uniform int uScorchCount;
uniform float uFogDensity, uFogFalloff, uFogFireGain, uAmbient;
uniform float uFrameJitter;
layout(location=0) out vec4 oCol;
layout(location=1) out vec4 oNrm;
layout(location=2) out vec4 oF0;   // luz do fogo que o cubo 0 multiplica · fator ruidoso
layout(location=3) out vec4 oF1;

// ================= materiais procedurais =================

// Fator de LOD pra ruído procedural: quanto do detalhe de frequência freq
// ainda cabe num pixel. Ruído procedural não tem mipmap — sem isto ele
// aliasa violentamente à distância, que é a origem do chão salpicado.
float detailLOD(vec3 w, float freq){
  float px = max(length(fwidth(w)), 1e-5);
  return saturate(0.6 / (px * freq));
}

// campo de altura do chão: 2 oitavas + 1 rachadura. Chamado 3× por pixel
// (valor + 2 derivadas), então cada snoise aqui custa 3×.
vec3 gEmit = vec3(0.0);   // radiância própria (escape de míssil, traçante)
void material(out vec3 alb, out float rough, out float metal, out vec3 N, out float ao){
  int id = int(vB.w + 0.5);
  vec3 w = vWP;
  N = normalize(vN);
  rough = vC.w;
  metal = 0.0;
  ao = 1.0;

  if (id == 0){
    // ---- chão: material assado (ver _bakeGround) ------------------------
    // Duas amostras em escalas e rotações diferentes quebram a repetição do
    // tile; ainda são 4 fetches contra ~20 snoise da versão procedural.
    vec2 uv1 = w.xz / uGroundTile;
    vec2 uv2 = (mat2(0.80, -0.60, 0.60, 0.80) * w.xz) / (uGroundTile * 2.37);
    vec4 a1 = texture(uGroundA, uv1), a2 = texture(uGroundA, uv2);
    vec4 n1 = texture(uGroundN, uv1), n2 = texture(uGroundN, uv2);
    vec4 ga = mix(a1, a2, 0.42);
    vec4 gn = mix(n1, n2, 0.42);

    // variação de larga escala continua procedural: o comprimento de onda
    // dela é maior que o tile, então assá-la criaria repetição visível
    // (frequência baixa, não precisa de LOD)
    float m = fbm(w * 0.042, 3, 2.1, 0.55) * 0.5 + 0.5;
    alb = (ga.rgb / 6.0) * (0.72 + 0.62 * m);
    rough = ga.a;

    // ---- camadas de sujeira em larga escala -----------------------------
    // Um chão com albedo uniforme lê como superfície de teste. Manchas de
    // óleo e areia acumulada em escalas DIFERENTES da do tile são o que
    // quebram a sensação de material procedural repetido.
    float stain = smoothstep(0.58, 0.88, fbm(w * 0.115 + 7.3, 4, 2.2, 0.5) * 0.5 + 0.5);
    float sand  = smoothstep(0.42, 0.82, fbm(w * 0.068 - 3.1, 3, 2.0, 0.55) * 0.5 + 0.5);
    alb = mix(alb, alb * 0.42, stain * 0.75);
    alb = mix(alb, vec3(0.102, 0.088, 0.070), sand * 0.50);
    rough = mix(rough, 0.985, sand * 0.55);
    rough = mix(rough, 0.70, stain * 0.45);
    N = normalize(vec3((gn.x - 0.5) * 2.0, 1.0, (gn.y - 0.5) * 2.0));

    // marcas de queimado: uma por explosão, persistentes
    float sc = 0.0;
    for (int k = 0; k < 16; k++){
      if (k >= uScorchCount) break;
      vec4 c = uScorch[k];
      float dk = length(w.xz - c.xy);
      sc = max(sc, (1.0 - smoothstep(c.z * 0.30, c.z, dk)) * c.w);
    }
    if (sc > 0.0){
      sc *= 0.62 + 0.38 * fbm(w * 0.55 + 4.1, 3, 2.2, 0.5);
      alb *= 1.0 - saturate(sc) * 0.72;
      rough = mix(rough, 0.97, saturate(sc));
    }
    float d = length(w.xz);

    // escurecimento suave por distância (evita chão "infinito" chapado)
    ao = mix(1.0, 0.55, saturate(d / 90.0));
  } else if (id == 2){
    // ---- tambor de aço pintado e enferrujado ---------------------------
    float rust = fbm(w * 2.1 + vSeed, 4, 2.25, 0.55) * 0.5 + 0.5;
    rust = saturate(rust * 1.15 + fbm(w * 7.5, 2, 2.0, 0.5) * 0.14 * detailLOD(w, 7.5));
    float rings = smoothstep(0.40, 0.47, abs(fract(vLP.y * 3.0 + 0.5) - 0.5));
    vec3 rustC = vec3(0.1050, 0.0450, 0.0225);
    alb = mix(vC.rgb * (0.55 + 0.35 * rings), rustC, smoothstep(0.42, 0.78, rust));
    metal = 0.45 * (1.0 - smoothstep(0.42, 0.78, rust));
    rough = mix(0.38, 0.90, smoothstep(0.35, 0.80, rust));
    {
      float e = 0.02;
      float amp = 0.016 * detailLOD(w, 6.0);
      vec3 t = normalize(abs(N.y) < 0.98 ? cross(vec3(0.0, 1.0, 0.0), N) : vec3(1.0, 0.0, 0.0));
      vec3 b = cross(N, t);
      float n0 = fbm(w * 6.0, 2, 2.0, 0.5);
      N = normalize(N - (t * (fbm((w + t * e) * 6.0, 2, 2.0, 0.5) - n0)
                       + b * (fbm((w + b * e) * 6.0, 2, 2.0, 0.5) - n0)) * amp / e);
    }
    ao = mix(0.75, 1.0, saturate(vLP.y * 1.6 + 0.25));
  } else if (id == 4){
    // ---- pintura militar fosca (avião, míssil) --------------------------
    float n = mix(0.5, fbm(w * 1.3 + vSeed, 3, 2.2, 0.5) * 0.5 + 0.5, detailLOD(w, 1.3));
    alb = vC.rgb * (0.85 + 0.3 * n);
    metal = 0.15;
    rough = mix(vC.w - 0.08, vC.w + 0.08, n);
  } else if (id == 5){
    // ---- emissivo: chama do motor, traçante (vC.rgb = radiância) --------
    alb = vec3(0.0);
    rough = 1.0;
    gEmit = vC.rgb;
  } else if (id == 3){
    // ---- aço escuro / tubos --------------------------------------------
    float n = mix(0.5, fbm(w * 3.4 + vSeed, 3, 2.2, 0.5) * 0.5 + 0.5, detailLOD(w, 3.4));
    alb = vC.rgb * (0.7 + 0.6 * n);
    metal = 0.6;
    rough = mix(0.42, 0.80, n);
  } else {
    // ---- concreto / escombros ------------------------------------------
    float ld = detailLOD(w, 9.0);
    float n = fbm(w * 1.5 + vSeed, 4, 2.2, 0.5) * 0.5 + 0.5;
    float fine = mix(0.5, fbm(w * 9.0, 2, 2.3, 0.5) * 0.5 + 0.5, ld);
    // Concreto tem albedo quase uniforme; o que varia é a RUGOSIDADE. A
    // versão anterior punha a variação toda no albedo (±20% em escala de
    // 0.7m, mais clareamento de aresta) e os blocos viravam isopor.
    alb = vC.rgb * (0.86 + 0.26 * n);
    rough = vC.w - 0.16 * n - 0.10 * (fine - 0.5) * ld;
    // desgaste de aresta: aproximado pelo 2º maior |posição local|
    vec3 a = abs(vLP);
    float mx = maxc(a);
    float second = a.x + a.y + a.z - mx - min(a.x, min(a.y, a.z));
    float edge = smoothstep(0.40, 0.50, second);
    alb *= 1.0 + edge * 0.10;
    rough -= edge * 0.10;

    // ---- sujeira: o que separa "concreto" de "cubo cinza" ---------------
    float ldD = detailLOD(w, 2.0);
    float up = saturate(N.y);
    // pó assenta nas faces viradas pra cima
    float dust = up * up * smoothstep(0.30, 0.78,
                   fbm(w * 0.85 + vSeed, 3, 2.2, 0.5) * 0.5 + 0.5);
    alb = mix(alb, vec3(0.118, 0.100, 0.076), dust * 0.55);
    rough = mix(rough, 0.98, dust * 0.6);
    // respingo de terra acumulado na base
    float grime = (1.0 - smoothstep(0.0, 1.1, vWP.y))
                * (0.45 + 0.55 * fbm(w * 2.0 + vSeed, 2, 2.0, 0.5) * 0.5 + 0.5) * ldD;
    alb *= 1.0 - saturate(grime) * 0.38;
    // escorrido vertical nas faces laterais (ruído esticado em Y)
    float streak = (1.0 - up) * smoothstep(0.58, 0.90,
                     fbm(vec3(w.x * 3.2, w.y * 0.30, w.z * 3.2) + vSeed, 3, 2.2, 0.5) * 0.5 + 0.5);
    alb *= 1.0 - streak * 0.28 * ldD;
    // Bump map por diferenças finitas. A amplitude é dividida por e=0.03, então
    // o fator precisa ser pequeno: com 0.09 a perturbação chegava a ~0.9, da
    // mesma ordem da própria normal, e a superfície virava normal aleatória —
    // o padrão dálmata. E a atenuação por LOD entra como MULTIPLICAÇÃO suave:
    // um if() sobre valor contínuo faz pixels vizinhos alternarem entre
    // perturbado e liso, o que produz exatamente o mesmo salpicado.
    {
      float e = 0.03;
      float amp = 0.020 * detailLOD(w, 5.0);
      vec3 t = normalize(abs(N.y) < 0.98 ? cross(vec3(0.0, 1.0, 0.0), N) : vec3(1.0, 0.0, 0.0));
      vec3 b = cross(N, t);
      float n0 = fbm(w * 5.0, 2, 2.1, 0.5);
      N = normalize(N - (t * (fbm((w + t * e) * 5.0, 2, 2.1, 0.5) - n0)
                       + b * (fbm((w + b * e) * 5.0, 2, 2.1, 0.5) - n0)) * amp / e);
    }
    ao = mix(0.72, 1.0, saturate(vWP.y * 0.9 + 0.25));
  }
  rough = clamp(rough, 0.05, 1.0);
}

// ================= sombras =================

// PCSS (Fernando 2005) num CUBO de distância radial, pra fonte extensa de
// raio R. A bola de fogo tem metros de raio: a sombra é dura no pé do objeto
// e abre em penumbra com a distância entre ele e o chão. Tudo em ÂNGULO visto
// do centro da fonte (vale igual em qualquer face do cubo):
//   1) busca de bloqueadores numa janela ~ 2.3·R/d
//   2) penumbra angular  θ = R (dR − dB) / (dR · dB)
//   3) PCF com esse raio
// disco de Vogel: N amostras na espiral do ângulo áureo, girada por pixel.
// Distribui uniforme pra qualquer N (o Poisson fixo de 8 deixava a penumbra
// larga granulada, e não há TAA pra alisar).
vec2 vogel(int i, int n, float rot){
  float r = sqrt((float(i) + 0.5) / float(n));
  float a = float(i) * 2.39996323 + rot;
  return vec2(cos(a), sin(a)) * r;
}
// Comparação com filtro bilinear (o PCF 2×2 que o hardware faz num mapa de
// profundidade): o R32F não filtra, então os 4 texels vizinhos são lidos no
// centro e o resultado da comparação é interpolado. A grade de texels de uma
// face é simétrica em torno do centro, então qualquer orientação de u,v serve.
float cubeCmp(samplerCube cm, vec3 d, float ref){
  const float SZ = ${CUBE_SIZE}.0;
  vec3 a = abs(d);
  vec3 n, u, v; float ma;
  if (a.x >= a.y && a.x >= a.z){ ma = a.x; n = vec3(sign(d.x), 0.0, 0.0); u = vec3(0.0, 0.0, 1.0); v = vec3(0.0, 1.0, 0.0); }
  else if (a.y >= a.z)        { ma = a.y; n = vec3(0.0, sign(d.y), 0.0); u = vec3(1.0, 0.0, 0.0); v = vec3(0.0, 0.0, 1.0); }
  else                        { ma = a.z; n = vec3(0.0, 0.0, sign(d.z)); u = vec3(1.0, 0.0, 0.0); v = vec3(0.0, 1.0, 0.0); }
  vec2 st = vec2(dot(d, u), dot(d, v)) / ma;
  vec2 px = (st * 0.5 + 0.5) * SZ - 0.5;
  vec2 f = fract(px), b0 = floor(px);
  float r = 0.0;
  for (int j = 0; j < 2; j++)
    for (int i = 0; i < 2; i++){
      vec2 c = clamp((b0 + vec2(float(i), float(j)) + 0.5) / SZ, 0.5 / SZ, 1.0 - 0.5 / SZ) * 2.0 - 1.0;
      float w = (i == 0 ? 1.0 - f.x : f.x) * (j == 0 ? 1.0 - f.y : f.y);
      r += w * step(ref, textureLod(cm, n + u * c.x + v * c.y, 0.0).r);
    }
  return r;
}
// PCSS (Fernando 2005) sobre o cubo: oclusor médio → penumbra da fonte
// extensa de raio R → PCF bilinear num disco de Vogel. O ruído da rotação por
// pixel é tirado depois, em espaço de tela (shadowdenoise.js).
float cubeShadow(samplerCube cm, vec3 Lp, vec3 P, float R, float jit){
  vec3 v = P - Lp;
  float dR = length(v);
  if (dR < 1e-3) return 1.0;
  vec3 dir = v / dR;
  vec3 t = normalize(cross(dir, abs(dir.y) < 0.95 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0)));
  vec3 b = cross(dir, t);
  float bias = 0.05 + 0.012 * dR;
  const float TEXA = ${(2 / CUBE_SIZE).toFixed(5)};   // ângulo de um texel no centro da face
  float rot = jit * TAU;
  float thS = clamp(2.3 * R / dR, 2.0 * TEXA, 0.5);
  float zB = 0.0, nB = 0.0;
  for (int i = 0; i < 8; i++){
    vec2 o = vogel(i, 8, rot);
    float z = texture(cm, dir + (t * o.x + b * o.y) * thS).r;
    if (z < dR - bias){ zB += z; nB += 1.0; }
  }
  if (nB < 0.5) return 1.0;                         // nada entre o ponto e o fogo
  zB /= nB;
  float thP = clamp(R * (dR - zB) / (dR * max(zB, 0.2)), TEXA, 0.5);
  float lit = 0.0;
  for (int i = 0; i < 8; i++){
    vec2 o = vogel(i, 8, rot + 1.3);
    lit += cubeCmp(cm, dir + (t * o.x + b * o.y) * thP, dR - bias);
  }
  return lit / 8.0;
}
// os dois cubos têm samplers diferentes: escolha com índice constante
float fireCube(int k, vec3 P, float jit){
  return k == 0 ? cubeShadow(uFireCube0, uCubePos[0], P, uCubeRad[0], jit)
                : cubeShadow(uFireCube1, uCubePos[1], P, uCubeRad[1], jit + 0.61);
}
// O que cada cubo multiplica neste pixel (luz do fogo antes da sombra) e o
// fator ruidoso — vão pro MRT pro composite trocar pelo fator filtrado.
vec3 gCubeL0 = vec3(0.0), gCubeL1 = vec3(0.0);
float gCubeF0 = 1.0, gCubeF1 = 1.0;
void addCube(int k, vec3 L, float f){
  if (k == 0){ gCubeL0 += L; gCubeF0 = f; } else { gCubeL1 += L; gCubeF1 = f; }
}

float pcf(sampler2D sm, vec3 s, float texel, float bias, float radius, float jit){
  if (s.x < 0.001 || s.x > 0.999 || s.y < 0.001 || s.y > 0.999 || s.z > 1.0) return 1.0;
  float ang = jit * TAU;
  float c = cos(ang), sn = sin(ang);
  mat2 rot = mat2(c, -sn, sn, c);
  float sum = 0.0;
  for (int i = 0; i < 8; i++){
    vec2 o = rot * POISSON[i] * texel * radius;
    sum += step(s.z - bias, texture(sm, s.xy + o).r);
  }
  return sum * 0.125;
}

void main(){
  vec3 alb, N; float rough, metal, ao;
  material(alb, rough, metal, N, ao);
  vec3 V = normalize(uCamPos - vWP);
  if (dot(N, V) < 0.0) N = normalize(N - V * dot(N, V) * 1.6);
  float jit = ignoise(gl_FragCoord.xy + uFrameJitter);

  vec3 direct = vec3(0.0);

  // ---- luz-chave (sol de dia, lua de noite): shadow map + sombra
  //      volumétrica marchada pela própria fumaça -------------------------
  vec3 keyCol = envKeyColor();
  if (maxc(keyCol) > 1e-6){
    vec4 lp = uSunVP * vec4(vWP + N * 0.10 + uKeyDir * 0.06, 1.0);
    vec3 s = lp.xyz / lp.w * 0.5 + 0.5;
    float sm = pcf(uSunShadow, s, uSunSMTexel, 0.0022, 2.6, jit);
    // Todas as explosões sombreiam o sol. O teste de caixa rejeita na hora
    // quem não está no caminho, então o custo extra por volume inativo é
    // praticamente um rayBox.
    float sv = ${sunOccChain};
    for (int i = 0; i < MAX_INST_SHADOW; i++){
      if (i >= uInstShCount) break;
      sv *= bakeShadow(vWP + N * 0.05, uKeyDir, 140.0, uInstShX[i], uInstShF[i],
                       uSootExt * uInstShK[i], uDustExt * uInstShK[i], 3.0);
    }
    direct += brdf(N, V, uKeyDir, alb, rough, metal) * keyCol * sm * sv;
  }

  // ---- cada bola de fogo é uma luz pontual real -------------------------
  vec3 fillTerm = vec3(0.0);
  for (int i = 0; i < MAX_BLASTS; i++){
    if (i >= uBlastCount) break;
    vec3 fcol = uFireColArr[i];
    if (maxc(fcol) < 1e-5) continue;
    vec3 Ld = uFirePosArr[i] - vWP;
    float dist = max(length(Ld), 1e-3);
    Ld /= dist;
    float NoL = dot(N, Ld);
    // sombra dos props pra explosão ao vivo principal (luz 0), se ela
    // levou um dos cubos neste quadro
    float fsm = 1.0; int fk = -1;
    if (i == 0){
      for (int k = 0; k < 2; k++){
        if (uCubeKind[k] == 1){ fsm = fireCube(k, vWP + N * 0.08, jit); fk = k; }
      }
    }
    if (NoL > -0.05){
      float att = 1.0 / (1.0 + dist * dist * 0.020);
      // a fumaça de QUALQUER explosão bloqueia a luz desta
      float fv = ${fireOccChain};
      vec3 Lsh = brdf(N, V, Ld, alb, rough, metal) * fcol * att * fv;
      direct += Lsh * fsm;
      if (fk >= 0) addCube(fk, Lsh, fsm);
    }
    float wrap = saturate((dot(N, Ld) + 0.75) / 1.75);
    float attF = 1.0 / (1.0 + dist * dist * 0.012);
    vec3 fillI = alb * (1.0 / PI) * fcol * attF * uFireFill * wrap;
    fillTerm += fillI * mix(1.0, fsm, 0.65);
    if (fk >= 0) addCube(fk, fillI * 0.65, fsm);
  }
  direct += fillTerm;

  // ---- luzes das instâncias --------------------------------------------
  for (int i = 0; i < MAX_INST_LIGHTS; i++){
    if (i >= uInstLightCount) break;
    vec3 c = uInstLightCol[i];
    vec3 Ld = uInstLightPos[i] - vWP;
    float d2 = dot(Ld, Ld);
    float dist = sqrt(max(d2, 1e-6));
    Ld /= dist;
    // Fonte extensa: E = P/(s² + 0.02d²). Perto, o brilho é limitado pela
    // radiância da superfície da bola de fogo (igual em toda magnitude); longe,
    // cai como P/d². Com o ponto puro (1 + 0.02d²) um Paiol estourava o chão
    // pra branco a 2.5× a Carga Pesada. Em s=1 é a fórmula de sempre.
    float s2 = uInstLightS2[i];
    float att = 1.0 / (s2 + d2 * 0.020);
    // A luz de uma explosão NÃO pode ser ocluída pelo próprio volume dela: a
    // aproximação de luz pontual coloca a fonte no centro da bola de fogo, e
    // o raio sairia atravessando ela inteira. Era isso que zerava o terreno —
    // com 4 oclusores empilhando um teto de 4%, 0.04⁴ ≈ 2.6e-6.
    float iv = 1.0;
    for (int j = 0; j < MAX_INST_SHADOW; j++){
      if (uInstLightOcc < 0.5 || j >= uInstShCount) break;
      if (uInstShId[j] == uInstLightId[i]) continue;
      iv *= bakeShadow(vWP + N * 0.05, Ld, dist * 0.90, uInstShX[j], uInstShF[j],
                       uSootExt * uFireOcclude * uInstShK[j], uDustExt * uFireOcclude * uInstShK[j], 1.4);
    }
    // sombra dos props pra esta luz, se ela levou um dos cubos neste quadro
    float ish = 1.0; int ik = -1;
    for (int k = 0; k < 2; k++){
      if (uCubeKind[k] == 2 && uCubeIdx[k] == i){ ish = fireCube(k, vWP + N * 0.08, jit); ik = k; }
    }
    if (uSMDebug > 0.5 && uCubeKind[0] == 2 && uCubeIdx[0] == i){
      oCol = vec4(vec3(ish) * 0.02, 1.0); oNrm = vec4(N * 0.5 + 0.5, 0.0);
      oF0 = vec4(0.0, 0.0, 0.0, 1.0); oF1 = vec4(0.0, 0.0, 0.0, 1.0); return;
    }
    vec3 Lsh = brdf(N, V, Ld, alb, rough, metal) * c * att * iv;
    direct += Lsh * ish;
    float wrap = saturate((dot(N, Ld) + 0.75) / 1.75);
    vec3 fillI = alb * (1.0 / PI) * c * (1.0 / (s2 + d2 * 0.012)) * uFireFill * wrap;
    direct += fillI * mix(1.0, ish, 0.65);
    if (ik >= 0) addCube(ik, Lsh + fillI * 0.65, ish);
  }

  // ---- ambiente hemisférico -------------------------------------------
  vec3 ambient = ambientIBL(N, V, alb, rough, metal,
                            envSkyUp() * uAmbient, envSkyDn() * uAmbient, ao);

  vec3 col = direct + ambient + gEmit;

  // ---- névoa de altura analítica, cor = céu naquela direção ------------
  vec3 d = vWP - uCamPos;
  float dist = length(d);
  vec3 vd = d / max(dist, 1e-4);
  float hc = exp(-max(uCamPos.y, 0.0) * uFogFalloff);
  float hp = exp(-max(vWP.y, 0.0) * uFogFalloff);
  float integ = abs(d.y) > 1e-3 ? (hc - hp) / (d.y * uFogFalloff) : hc;
  float fog = 1.0 - exp(-uFogDensity * dist * max(integ, 0.0));
  vec3 fogCol = skyBase(vd);
  // a névoa entre a câmera e a superfície também é iluminada pelo fogo
  vec3 mid = uCamPos + d * 0.5;
  float fd = length(uFirePosArr[0] - mid);
  fogCol += uFireColArr[0] * (uFogFireGain / (1.0 + fd * fd * 0.12));
  col = mix(col, fogCol, saturate(fog));

  // Fração da cor que veio de luz INDIRETA — é só isso que o SSAO pode
  // escurecer. O preenchimento do fogo é uma aproximação de espalhamento
  // múltiplo, então entra aqui junto com o ambiente hemisférico.
  float ambFrac = saturate(luma(ambient + fillTerm) / max(luma(col), 1e-5))
                * (1.0 - saturate(fog));
  oCol = vec4(col, 1.0);
  oNrm = vec4(N * 0.5 + 0.5, ambFrac);
  // a névoa atenua a luz do fogo como o resto da cor
  float tFog = 1.0 - saturate(fog);
  oF0 = vec4(gCubeL0 * tFog, gCubeF0);
  oF1 = vec4(gCubeL1 * tFog, gCubeF1);
}`, 'scene');
  }

  /**
   * Pré-calcula o material do chão (albedo, rugosidade, normal) numa textura
   * TILEÁVEL.
   *
   * O chão cobre a maior parte da tela e avaliava ~20 snoise por pixel — o
   * campo de altura sozinho era chamado 3× (valor + 2 derivadas). Como o
   * chão é plano, tudo isso é função só de XZ: dá pra assar uma vez.
   *
   * O ruído aqui é value-noise PERIÓDICO (hash no reticulado mod período),
   * não simplex — simplex não tem como tilear, e uma emenda visível no chão
   * seria pior que o custo que se economiza.
   */
  _bakeGround(res) {
    const gl = this.gl;
    const rgba8 = { internalFormat: gl.RGBA8, format: gl.RGBA, type: gl.UNSIGNED_BYTE, wrap: gl.REPEAT };
    this.groundA = new Target(gl, res, res, rgba8);   // albedo.rgb + rugosidade
    this.groundN = new Target(gl, res, res, rgba8);   // normal.xz + altura

    const sh = new Shader(gl, FS_VS, HEAD + `in vec2 vUV;\n` + COMMON + `
uniform float uTile;
layout(location=0) out vec4 oA;
layout(location=1) out vec4 oN;

#define PER 8.0

float ph2(vec2 i, float per){
  i = mod(i, vec2(per));
  return fract(sin(dot(i, vec2(127.1, 311.7))) * 43758.5453123);
}
float pn2(vec2 p, float per){
  vec2 i = floor(p), f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  return mix(mix(ph2(i, per), ph2(i + vec2(1,0), per), f.x),
             mix(ph2(i + vec2(0,1), per), ph2(i + vec2(1,1), per), f.x), f.y) * 2.0 - 1.0;
}
// fbm periódico: cada oitava dobra frequência E período, então tilea
float pfbm2(vec2 p, float per, int oct){
  float s = 0.0, a = 0.5, n = 0.0, sc = 1.0;
  for (int i = 0; i < 6; i++){
    if (i >= oct) break;
    s += a * pn2(p * sc, per * sc);
    n += a; sc *= 2.0; a *= 0.5;
  }
  return s / n;
}

float heightAt(vec2 p){
  float grit = pfbm2(p * 4.0, PER * 4.0, 3);
  float cr  = pow(saturate(1.0 - abs(pn2(p * 2.0, PER * 2.0))), 18.0);
  float cr2 = pow(saturate(1.0 - abs(pn2(p * 5.0, PER * 5.0))), 22.0);
  return grit * 0.35 - (cr + cr2 * 0.7) * 1.4;
}

void main(){
  vec2 p = vUV * PER;
  float grit = pfbm2(p * 4.0, PER * 4.0, 3) * 0.5 + 0.5;
  float cr  = pow(saturate(1.0 - abs(pn2(p * 2.0, PER * 2.0))), 18.0);
  float cr2 = pow(saturate(1.0 - abs(pn2(p * 5.0, PER * 5.0))), 22.0);
  float cracks = saturate(cr + cr2 * 0.7);

  vec3 alb = vec3(0.058, 0.055, 0.052);
  alb *= 0.80 + 0.40 * grit;
  alb *= 1.0 - cracks * 0.70;
  float rough = 0.90 - 0.16 * grit;

  // normal por diferenças finitas do campo de altura, em unidades de mundo
  float e = PER / float(${res});
  float h0 = heightAt(p);
  float hx = heightAt(p + vec2(e, 0.0));
  float hy = heightAt(p + vec2(0.0, e));
  float wScale = uTile / PER;              // metros por unidade de p
  vec3 n = normalize(vec3(-(hx - h0) / e * 0.16 / wScale, 1.0,
                          -(hy - h0) / e * 0.16 / wScale));

  oA = vec4(alb * 6.0, rough);             // albedo escalado pra caber em 8 bits
  oN = vec4(n.x * 0.5 + 0.5, n.z * 0.5 + 0.5, h0 * 0.25 + 0.5, 1.0);
}`, 'bakeGround');

    const fbo = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, fbo);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, this.groundA.tex, 0);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT1, gl.TEXTURE_2D, this.groundN.tex, 0);
    gl.drawBuffers([gl.COLOR_ATTACHMENT0, gl.COLOR_ATTACHMENT1]);
    gl.viewport(0, 0, res, res);
    gl.disable(gl.DEPTH_TEST);
    sh.use().set('uTile', this.groundTile);
    drawFS(gl);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    gl.deleteFramebuffer(fbo);

    // sem mipmap + anisotropia o chão vira ruído em ângulo rasante
    enableMipAniso(gl, this.groundA.tex);
    enableMipAniso(gl, this.groundN.tex);
  }

  /** shadow map da luz-chave. A geometria é estática, mas o sol se move com
   *  a hora do dia, então isto é re-renderizado quando a direção muda. */
  renderSunShadow(sunDir) {
    const gl = this.gl, sm = this.sunSM;
    const R = 52, D = 150;
    const eye = [sunDir[0] * D * 0.6, Math.max(sunDir[1], 0.04) * D * 0.6, sunDir[2] * D * 0.6];
    // com a luz no zênite, up=(0,1,0) degenera o lookAt
    const up = Math.abs(sunDir[1]) > 0.985 ? [0, 0, 1] : [0, 1, 0];
    const view = m4.lookAt(m4.create(), eye, [0, 4, 0], up);
    const proj = m4.ortho(m4.create(), -R, R, -R, R, -D, D * 1.5);
    m4.mul(sm.vp, proj, view);
    sm.bind();
    gl.enable(gl.DEPTH_TEST);
    gl.depthFunc(gl.LEQUAL);
    gl.enable(gl.CULL_FACE);
    gl.cullFace(gl.FRONT);
    this.shDepth.use().set('uVP', sm.vp);
    this.meshBoxes.draw();
    this.meshCyls.draw();
    this._drawMovers();
    gl.cullFace(gl.BACK);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }

  /**
   * Renderiza os cubos de sombra das luzes de explosão.
   * @param lights [{pos, radius, kind, idx}] — as escolhidas (no máximo 2)
   * @returns o que o render precisa: [{pos, radius, kind, idx}]
   */
  renderFireShadows(lights) {
    const gl = this.gl, out = [];
    gl.enable(gl.DEPTH_TEST);
    gl.depthFunc(gl.LEQUAL);
    // só faces de trás: superfície da frente nunca se sombreia sozinha
    gl.enable(gl.CULL_FACE);
    gl.cullFace(gl.FRONT);
    const sh = this.shDist;
    lights.slice(0, this.fireCubes.length).forEach((L, k) => {
      const cube = this.fireCubes[k];
      gl.bindFramebuffer(gl.FRAMEBUFFER, cube.fbo);
      gl.viewport(0, 0, cube.size, cube.size);
      sh.use().set('uLightPos', L.pos);
      for (let f = 0; f < 6; f++) {
        gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0,
                                gl.TEXTURE_CUBE_MAP_POSITIVE_X + f, cube.tex, 0);
        gl.drawBuffers([gl.COLOR_ATTACHMENT0]);
        gl.clearColor(1e4, 0, 0, 1);          // "nada até o infinito"
        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
        const [dir, up] = CUBE_FACES[f];
        const view = m4.lookAt(m4.create(), L.pos,
          [L.pos[0] + dir[0], L.pos[1] + dir[1], L.pos[2] + dir[2]], up);
        m4.mul(cube.vp, cube.proj, view);
        sh.set('uVP', cube.vp);
        this.meshBoxes.draw();
        this.meshCyls.draw();
        this._drawMovers();
        // o chão não precisa projetar: nada fica embaixo dele
      }
      out.push({ pos: L.pos, radius: L.radius, kind: L.kind, idx: L.idx });
    });
    gl.clearColor(0, 0, 0, 0);
    gl.cullFace(gl.BACK);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    return out;
  }

  /**
   * Instâncias dos objetos em movimento deste quadro (mesmo formato do
   * layout: 16 floats por peça).
   */
  setMovers(boxes, nBoxes, cyls, nCyls) {
    const gl = this.gl;
    const put = (mesh, data, n) => {
      mesh.instCount = Math.min(n, MAX_MOVER_PARTS);
      if (!mesh.instCount) return;
      gl.bindBuffer(gl.ARRAY_BUFFER, mesh.instBuf);
      gl.bufferSubData(gl.ARRAY_BUFFER, 0, data, 0, mesh.instCount * 16);
      gl.bindBuffer(gl.ARRAY_BUFFER, null);
    };
    put(this.meshMovBoxes, boxes, nBoxes);
    put(this.meshMovCyls, cyls, nCyls);
  }

  _drawMovers() {
    if (this.meshMovBoxes.instCount) this.meshMovBoxes.draw();
    if (this.meshMovCyls.instCount) this.meshMovCyls.draw();
  }

  render(cam, env) {
    const gl = this.gl;
    gl.disable(gl.CULL_FACE);

    // céu primeiro, sem teste de profundidade
    gl.disable(gl.DEPTH_TEST);
    this.shSky.use()
      .set('uInvViewProj', cam.invViewProj).set('uCamPos', cam.pos)
      .set('uSunDir', env.sunDir).set('uMoonDir', env.moonDir).set('uSkyTime', env.skyTime)
      .set('uStarBright', env.starBright).set('uNightGlow', env.nightGlow)
      .set('uMoonBright', env.moonBright).set('uMoonBoost', env.moonBoost ?? 1);
    this.shSky.tex('uSkyView', env.skyView).tex('uEnvLut', env.envLut);
    drawFS(gl);

    // geometria
    gl.enable(gl.DEPTH_TEST);
    gl.depthFunc(gl.LEQUAL);
    gl.enable(gl.CULL_FACE);
    gl.cullFace(gl.BACK);
    const s = this.shScene.use()
      .set('uViewProj', cam.viewProj).set('uCamPos', cam.pos)
      .set('uSunDir', env.sunDir).set('uMoonDir', env.moonDir).set('uKeyDir', env.keyDir)
      .set('uSkyTime', env.skyTime)
      .set('uStarBright', env.starBright).set('uNightGlow', env.nightGlow)
      .set('uMoonBright', env.moonBright).set('uMoonBoost', env.moonBoost ?? 1)
      .set('uSunVP', this.sunSM.vp)
      .set('uSunSMTexel', 1 / this.sunSM.size)

      .set('uSootExt', env.sootExt).set('uDustExt', env.dustExt)
      .set('uFireOcclude', env.fireOcclude).set('uFireTauCap', env.fireTauCap)
      .set('uErodeMean', env.erodeMean).set('uDomainOrigin', env.domainOrigin)
      .set('uFireFill', env.fireFill)

      .set('uFogDensity', env.fogDensity).set('uFogFalloff', env.fogFalloff)
      .set('uFogFireGain', env.fogFireGain)
      .set('uAmbient', env.ambient).set('uFrameJitter', env.frameJitter)
      .set('uGroundTile', this.groundTile);
    s.tex('uSunShadow', this.sunSM.tex)
      .tex('uFireCube0', this.fireCubes[0].tex, gl.TEXTURE_CUBE_MAP)
      .tex('uFireCube1', this.fireCubes[1].tex, gl.TEXTURE_CUBE_MAP)
      .tex('uSkyView', env.skyView).tex('uEnvLut', env.envLut)
      .tex('uGroundA', this.groundA.tex).tex('uGroundN', this.groundN.tex);

    // arrays por explosão ativa
    const gl2 = this.gl, B = env.blasts;
    s.seti('uBlastCount', Math.min(B.length, MAX_BLASTS));
    const org = new Float32Array(MAX_BLASTS * 3), fpos = new Float32Array(MAX_BLASTS * 3),
          fcol = new Float32Array(MAX_BLASTS * 3);
    for (let i = 0; i < Math.min(B.length, MAX_BLASTS); i++) {
      const b = B[i];
      const f = b.fire || { pos: b.fluid.blastPos, color: [0, 0, 0] };
      org.set(b.fluid.domainOrigin, i * 3);
      fpos.set(f.pos, i * 3);
      fcol.set(f.color, i * 3);
      // flash da detonação (mesmo pulso das instâncias), pela idade do slot
      if (env.flashGain > 0 && b.age < 8 * env.flashTau) {
        const k = env.flashGain * Math.exp(-b.age / env.flashTau);
        fcol[i * 3] += k; fcol[i * 3 + 1] += k * 0.86; fcol[i * 3 + 2] += k * 0.64;
      }
      // opção "brilho das explosões" vale também pra simulação ao vivo
      const g = env.lightGain ?? 1;
      fcol[i * 3] *= g; fcol[i * 3 + 1] *= g; fcol[i * 3 + 2] *= g;
      const uF = s.loc(`uFieldsArr[${i}]`), uM = s.loc(`uMacroArr[${i}]`);
      if (uF !== null) { const u = s._unit++; gl2.activeTexture(gl2.TEXTURE0 + u);
        gl2.bindTexture(gl2.TEXTURE_2D, b.fluid.fields.read.tex); gl2.uniform1i(uF, u); }
      if (uM !== null) { const u = s._unit++; gl2.activeTexture(gl2.TEXTURE0 + u);
        gl2.bindTexture(gl2.TEXTURE_2D, b.fluid.macro.tex); gl2.uniform1i(uM, u); }
    }
    // Entradas SEM explosão também precisam de unidade: uniform de sampler
    // guarda o último valor. Quando a explosão ao vivo acabava, o sampler2D
    // dela seguia apontando pra unidade que o bake (sampler2DArray) passa a
    // ocupar — dois tipos na mesma unidade e o WebGL recusa o draw inteiro
    // com INVALID_OPERATION. Era o chão sumindo "depois de algumas explosões".
    let uEmpty = -1;
    for (let i = Math.min(B.length, MAX_BLASTS); i < MAX_BLASTS; i++) {
      const uF = s.loc(`uFieldsArr[${i}]`), uM = s.loc(`uMacroArr[${i}]`);
      if (uF === null && uM === null) continue;
      if (uEmpty < 0) {
        uEmpty = s._unit++;
        gl2.activeTexture(gl2.TEXTURE0 + uEmpty);
        gl2.bindTexture(gl2.TEXTURE_2D, this._emptyTex);
      }
      if (uF !== null) gl2.uniform1i(uF, uEmpty);
      if (uM !== null) gl2.uniform1i(uM, uEmpty);
    }
    gl2.uniform3fv(s.loc('uBlastOrigin[0]'), org);
    gl2.uniform3fv(s.loc('uFirePosArr[0]'), fpos);
    gl2.uniform3fv(s.loc('uFireColArr[0]'), fcol);

    // marcas de queimado
    const SCR = env.scorch || { data: new Float32Array(64), n: 0 };
    s.seti('uScorchCount', SCR.n);
    if (SCR.n) gl2.uniform4fv(s.loc('uScorch[0]'), SCR.data);

    // luzes das instâncias
    const IL = env.instLights || [];
    const n = Math.min(IL.length, 8);
    s.seti('uInstLightCount', n);
    if (n) {
      const ip = new Float32Array(24), ic = new Float32Array(24), is2 = new Float32Array(8);
      const iid = new Int32Array(8);
      for (let i = 0; i < n; i++) {
        ip.set(IL[i].pos, i * 3); ic.set(IL[i].color, i * 3); iid[i] = IL[i].idx ?? -1;
        is2[i] = IL[i].s2 ?? 1;
      }
      gl2.uniform3fv(s.loc('uInstLightPos[0]'), ip);
      gl2.uniform3fv(s.loc('uInstLightCol[0]'), ic);
      gl2.uniform1fv(s.loc('uInstLightS2[0]'), is2);
      gl2.uniform1iv(s.loc('uInstLightId[0]'), iid);
    }

    // volumes instanciados que projetam sombra
    const SC = env.instShadows || [];
    const ns = Math.min(SC.length, 4);
    s.seti('uInstShCount', ns);
    if (ns) {
      const xf = new Float32Array(16), fr = new Float32Array(4), kk = new Float32Array(4);
      const sid = new Int32Array(4);
      for (let i = 0; i < ns; i++) {
        xf.set(SC[i].xform, i * 4); fr[i] = SC[i].frame; kk[i] = SC[i].keep ?? 1;
        sid[i] = SC[i].idx ?? -2;
      }
      gl2.uniform4fv(s.loc('uInstShX[0]'), xf);
      gl2.uniform1fv(s.loc('uInstShK[0]'), kk);
      gl2.uniform1fv(s.loc('uInstShF[0]'), fr);
      gl2.uniform1iv(s.loc('uInstShId[0]'), sid);
    }
    s.set('uInstLightOcc', env.instLightOcc ? 1 : 0).set('uSMDebug', env.smDebug ? 1 : 0);
    {
      const C = env.fireCubes || [];
      const kind = new Int32Array(2), idx = new Int32Array([-1, -1]);
      const rad = new Float32Array(2), pos = new Float32Array(6);
      C.forEach((c, k) => { kind[k] = c.kind; idx[k] = c.idx; rad[k] = c.radius; pos.set(c.pos, k * 3); });
      gl2.uniform1iv(s.loc('uCubeKind[0]'), kind);
      gl2.uniform1iv(s.loc('uCubeIdx[0]'), idx);
      gl2.uniform1fv(s.loc('uCubeRad[0]'), rad);
      gl2.uniform3fv(s.loc('uCubePos[0]'), pos);
    }
    s.set('uTemporalLerpK', 0).set('uBakeKFrames', env.bakeFrames || 1);
    s.tex('uBakeK', env.bakeTex, gl2.TEXTURE_2D_ARRAY)
      .tex('uBakeMacroQ', env.bakeMacroTex, gl2.TEXTURE_2D_ARRAY);
    this.meshGround.draw();
    this.meshBoxes.draw();
    this.meshCyls.draw();
    this._drawMovers();
    gl.disable(gl.CULL_FACE);
    gl.disable(gl.DEPTH_TEST);
  }
}
