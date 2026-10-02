// ---------------------------------------------------------------------------
// instances.js — explosões instanciadas a partir de uma sequência assada.
//
// Esta é a camada que ESCALA. Cada instância é só {posição, t0, escala, seed}:
// nenhuma simulação, nenhum alvo de render próprio. Dezenas podem coexistir —
// o custo é raymarch proporcional à área que ela ocupa na tela.
//
// Duas decisões que fazem a conta fechar:
//
// 1. SCISSOR por instância. Desenhar um triângulo fullscreen por instância
//    custaria a tela inteira mesmo quando a explosão ocupa 2% dela. O AABB
//    da caixa do volume é projetado na CPU e vira scissor, então o
//    fragment shader só roda onde a instância realmente aparece.
//
// 2. Luz por LOOKUP. A curva de luz (centróide, cor) foi gravada no bake,
//    então a instância não precisa da redução de GPU nem do readback —
//    ela só interpola a curva no seu tempo local.
// ---------------------------------------------------------------------------

import { Shader, drawFS, FS_VS } from './gl.js';
import { COMMON, ENVLUT, VOLUME_SHADOW } from './glsl.js';
import { VolumeGrid } from './volume.js';
import { SOOT_SCALE, TEMP_SCALE, DUST_SCALE } from './bake.js';
const F = (x) => (Number.isInteger(x) ? x.toFixed(1) : String(x));
const SOOT_F = F(SOOT_SCALE), TEMP_F = F(TEMP_SCALE), DUST_F = F(DUST_SCALE);

const BAKE_HELPERS = `
// Esvaecimento junto ao TETO e às laterais da caixa assada (o chão é chão de
// verdade). Na fase tardia a fumaça sobe até o teto do domínio da simulação e
// fica guardada como uma camada densa cortada reta; com a marcha conjunta o
// corte ainda sombreava a fumaça das vizinhas e aparecia como uma linha. Uma
// faixa larga no teto (~20% da altura) afina a fumaça antes do corte. Cache e
// marcha usam a MESMA função, senão a sombra guardaria o degrau.
float boxFade(vec3 vb){
  float top = smoothstep(0.0, GRIDB.y * 0.2, GRIDB.y - vb.y);
  float side = smoothstep(0.0, 5.0, min(min(vb.x, GRIDB.x - vb.x), min(vb.z, GRIDB.z - vb.z)));
  return top * side;
}

// Camada(s) do bake → (fuligem, temperatura, poeira, céu). O dither de meio
// LSB quebra as curvas de nível dos 8 bits: em superfície de fogo lisa a
// temperatura quantizada em degraus de 0.6% aparecia como contornos.
vec4 bakeAt(vec3 voxel, float frame, float base, float lerpT, float dith){
  float f0 = floor(frame);
  vec4 q = sampleBakeLayerB(voxel, base + f0);
  if (lerpT > 0.5){
    float f1 = min(f0 + 1.0, uBakeBFrames - 1.0);
    q = mix(q, sampleBakeLayerB(voxel, base + f1), frame - f0);
  }
  q.rgb = max(q.rgb + dith, 0.0);
  return vec4(q.r * q.r * ${SOOT_F}, q.g * ${TEMP_F}, q.b * q.b * ${DUST_F}, q.a);
}
`;
const MAX_STEPS = 160;   // a marcha conjunta atravessa várias caixas no mesmo raio
// Membros por grupo marchado junto. O estado por membro vive em arrays
// locais e a pressão de registradores cresce com o tamanho: o shader de 8
// custa ~25% a mais que o de 4 mesmo num grupo pequeno. Então há duas
// variantes compiladas e cada grupo usa a menor que o comporta.
const MJ = 8;
const MJ_SMALL = 4;
const MAX_STEPS_SINGLE = 96;
// Cache de luz: quantas instâncias ganham volume de luz próprio por frame
// (as de maior área em tela) e a resolução dele. 48³ sobre a caixa de 38m dá
// ~0.8m por célula na Carga Pesada — a mesma ordem do volume de luz da
// simulação ao vivo (64³ na mesma caixa).
const LC_SLOTS = 8;      // camadas alocadas; quantas são usadas vem da configuração
const LC_RES = 48;

/**
 * Magnitudes. As grandezas de uma explosão não escalam linearmente juntas:
 * comprimentos vão com W^(1/3) e TEMPOS também (velocidade característica
 * constante). Como a instância é só uma transformação da sequência assada,
 * magnitude = {escala espacial, velocidade de reprodução} — e a física sai
 * certa de graça. Intensidade da luz vai com a área, W^(2/3).
 */
export const MAGNITUDES = [
  { id: 'granada',  nome: 'Granada',      yield: 0.05, meta: 'HE · 1 kg' },
  { id: 'morteiro', nome: 'Morteiro',     yield: 0.22, meta: 'HE · 5 kg' },
  { id: 'tanque',   nome: 'Carga Pesada', yield: 1.00, meta: 'HE · alto rendimento' },
  { id: 'deposito', nome: 'Paiol',        yield: 4.00, meta: 'detonação secundária' },
  { id: 'aereo',    nome: 'Ataque Aéreo', yield: 11.0, meta: 'bomba de demolição' },
];

export function magnitudeOf(id) {
  const m = MAGNITUDES.find((x) => x.id === id) || MAGNITUDES[2];
  const s = Math.cbrt(m.yield);
  return { ...m, scale: s, speed: 1 / s };   // maior = maior E mais lento
}

export class BlastInstances {
  constructor(gl, bake, bbTex, noiseTex) {
    this.gl = gl;
    this.bake = bake;
    this.bbTex = bbTex;
    this.noiseTex = noiseTex;
    this.list = [];
    // Marcas de queimado PERSISTENTES, uma por explosão. Antes havia uma só,
    // global, cravada na origem e amarrada ao tempo global — escurecia o chão
    // onde a câmera olha mesmo sem nada ter explodido ali.
    this.scorches = [];
    this.maxScorch = 16;
    this.params = {
      sootExt: 13.5, dustExt: 5.5,
      sootAlbedo: 0.30, dustAlbedo: 0.78,
      sootColor: [0.55, 0.545, 0.535], dustColor: [0.82, 0.755, 0.655],
      emissionGain: 0.62, emissionCurve: 0.44, tempScale: 0.92,
      phaseG: 0.42, phaseBack: 0.22, phaseMix: 0.32,
      steps: 56, stepsMin: 16, refArea: 0.16, detailAmp: 0.42, detailScale: 0.85, detailDens: 0.34,
      erode: 0.042, skyGain: 1.1, fireGain: 1.0,
      // Intensidade da luz por instância relativa à simulação. Era 0.5 (pra
      // barragem não estourar), e o chão em volta da explosão do clique ficava
      // com metade da luz da explosão da tecla espaço. 1.0 = idêntica à ao
      // vivo; a atenuação 1/(1+0.02d²) já deixa cada luz local, então só um
      // aglomerado muito denso soma de verdade.
      lightScale: 1.0,
      crowdKeep: 0.35,    // fração da luz das vizinhas que ainda soma (ver lights())
      sunOcclude: 1.8,
      // mesmos do render ao vivo (VOL_DEFAULTS)
      flameBoost: 1.5,
      msOctaves: 3, msExt: 0.52, msScatter: 0.52, msPhase: 0.55,
      lightSteps: 20, fireSteps: 16,
      lcMinArea: 0.004,   // fração da tela abaixo da qual não vale cache de luz
    };

    const HEAD = `#version 300 es
precision highp float;
precision highp sampler2D;
precision highp sampler3D;
precision highp sampler2DArray;
in vec2 vUV;
`;
    // ---- cache de luz por instância ---------------------------------------
    // A instância não tinha volume de luz: a sombra do sol era pow(céu, 1.8),
    // sem direção nenhuma, e a fumaça saía chapada perto da simulação ao vivo.
    // Aqui cada uma das instâncias mais visíveis ganha, POR FRAME, o mesmo
    // volume de transmitância que a simulação calcula (até o sol e até a bola
    // de fogo), marchado sobre o quadro assado em que ela está.
    this.lcGrid = new VolumeGrid(LC_RES, bake.domainSize);
    this.lcTex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D_ARRAY, this.lcTex);
    // r = até o sol · g = até o próprio fogo · b = até o fogo VIZINHO que mais
    // ilumina esta explosão (marcha conjunta: a fumaça da pequena sombreia a
    // luz da grande na direção certa, não pela aproximação do céu)
    gl.texImage3D(gl.TEXTURE_2D_ARRAY, 0, gl.RGBA8, this.lcGrid.atlasW, this.lcGrid.atlasH,
                  LC_SLOTS, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
    for (const [k, v] of [['TEXTURE_MIN_FILTER', gl.LINEAR], ['TEXTURE_MAG_FILTER', gl.LINEAR],
                          ['TEXTURE_WRAP_S', gl.CLAMP_TO_EDGE], ['TEXTURE_WRAP_T', gl.CLAMP_TO_EDGE]]) {
      gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl[k], v);
    }
    this.lcFbo = gl.createFramebuffer();
    const LCP = this.lcGrid.glsl('C');
    this.shLight = new Shader(gl, FS_VS, HEAD + COMMON + bake.glsl() + LCP + BAKE_HELPERS + `
uniform vec3 uSunDirL, uFireL, uNbL;
uniform float uHasNb, uFull;
uniform float uInstScale, uInstFrame, uSootExt, uDustExt, uErodeMean;
uniform int uSunSteps, uFireSteps;
out vec4 oCol;

vec2 rayBoxL(vec3 ro, vec3 rd, vec3 bmin, vec3 bmax){
  vec3 inv = 1.0 / rd;
  vec3 t0 = (bmin - ro) * inv, t1 = (bmax - ro) * inv;
  vec3 tn = min(t0, t1), tf = max(t0, t1);
  return vec2(max(max(tn.x, tn.y), tn.z), min(min(tf.x, tf.y), tf.z));
}


// mesma extinção EFETIVA do render (erosão média), como em effExtinction()
float extAt(vec3 lp){
  vec3 vb = worldToVoxelAtB(lp, vec3(0.0));
  vec4 f = sampleBake4B(vb, uInstFrame) * vec4(vec3(boxFade(vb)), 1.0);
  float r = max(f.r - uErodeMean / (1.0 + f.r * 9.0), 0.0);
  float a = max(f.b - uErodeMean * 0.7 / (1.0 + f.b * 9.0), 0.0);
  return uSootExt * r + uDustExt * a;
}
float macroLayer(){ return uVariantBaseB + floor(uInstFrame); }
// transmitância de lp até maxDist na direção dir (espaço local; tau em metros)
float march(vec3 lp, vec3 dir, float maxDist, int steps, float tauCap){
  vec2 hit = rayBoxL(lp, dir, BASE_MINB, BASE_MINB + DOMAIN_SIZEB);
  hit.x = max(hit.x, 0.0);
  hit.y = min(hit.y, maxDist);
  if (hit.y <= hit.x) return 1.0;
  float dt = (hit.y - hit.x) / float(steps);
  float tau = 0.0, t = hit.x + dt * 0.5;
  for (int i = 0; i < 48; i++){
    if (t >= hit.y) break;
    vec3 p = lp + dir * t;
    if (bakeMacroAtM(worldToVoxelAtM(p, vec3(0.0)), macroLayer()) < 0.004){
      t += bakeMacroExitM(p, dir) + 1e-3;
      continue;
    }
    tau += extAt(p) * dt * uInstScale;
    if (tau > tauCap) break;
    t += dt;
  }
  return exp(-min(tau, tauCap));
}
void main(){
  vec3 v = fragToVoxelC(gl_FragCoord.xy);
  vec3 lp = BASE_MINC + v / GRIDC * DOMAIN_SIZEC;
  // Célula vazia desta explosão: pra uma instância SOZINHA não há o que
  // iluminar ali e a marcha é desperdício. Numa marcha conjunta (uFull) a
  // fumaça de uma VIZINHA pode estar exatamente ali e precisa da sombra e da
  // luz do fogo desta — com o atalho ela recebia o fogo inteiro e virava
  // algodão branco, com uma aresta reta na face dos blocos.
  if (uFull < 0.5 && bakeMacroAtM(worldToVoxelAtM(lp, vec3(0.0)), macroLayer()) < 0.004){
    oCol = vec4(1.0); return;
  }
  vec3 tf = uFireL - lp;
  float fd = length(tf);
  vec3 tn = uNbL - lp;
  float nd = length(tn);
  oCol = vec4(march(lp, uSunDirL, 1e4, uSunSteps, 60.0),
              // tauCap baixo, como na simulação: a luz pontual no centróide
              // atravessaria a bola de fogo inteira e apagaria a fumaça colada
              fd < 1e-3 ? 1.0 : march(lp, tf / fd, fd, uFireSteps, 2.2),
              (uHasNb < 0.5 || nd < 1e-3) ? 1.0 : march(lp, tn / nd, nd, uFireSteps, 2.2),
              1.0);
}`, 'instLightCache');

    this.shSingle = new Shader(gl, FS_VS, HEAD + COMMON + ENVLUT + bake.glsl() + bake.fuelGlsl('U') + LCP + BAKE_HELPERS + `
uniform sampler2D uBB, uDepth;
uniform sampler2DArray uLightC;
uniform float uLightSlot;   // camada do cache de luz; < 0 = sem cache
uniform float uFlameBoost, uMsExt, uMsScatter, uMsPhase;
uniform int uMsOctaves;
uniform sampler3D uNoise;
uniform mat4 uInvViewProj;
uniform vec3 uCamPos, uKeyDir;
uniform vec3 uInstPos, uFirePos, uFireColor;
uniform float uInstScale, uInstFrame, uInstSeed, uNear;
uniform vec3 uBoxMinW, uBoxMaxW;   // caixa JUSTA no mundo (onde a fumaça existe)
uniform float uSootExt, uDustExt, uSootAlbedo, uDustAlbedo;
uniform vec3 uSootColor, uDustColor;
uniform float uEmissionGain, uEmissionCurve, uTempScale;
uniform float uPhaseG, uPhaseBack, uPhaseMix, uSunOcclude;
uniform float uDetailAmp, uDetailScale, uDetailDens, uErode;
uniform float uSkyGain, uFireGain, uJitter, uTimeAnim;
uniform int uSteps;

layout(location=0) out vec4 oCol;
layout(location=1) out vec4 oAux;

vec2 rayBoxI(vec3 ro, vec3 rd, vec3 bmin, vec3 bmax){
  vec3 inv = 1.0 / rd;
  vec3 t0 = (bmin - ro) * inv, t1 = (bmax - ro) * inv;
  vec3 tn = min(t0, t1), tf = max(t0, t1);
  return vec2(max(max(tn.x, tn.y), tn.z), min(min(tf.x, tf.y), tf.z));
}

// ---- caminho de UMA instância (o comum num RTS): o shader enxuto ------
// mundo → espaço local da instância (desfaz posição e escala)
vec3 toLocal(vec3 w){ return (w - uInstPos) / uInstScale; }

vec4 sampleInst(vec3 w, float dith){
  vec3 lp = toLocal(w);
  // ruído no espaço LOCAL e deslocado pela seed: duas instâncias da mesma
  // sequência não ficam idênticas
  vec3 fp = lp * uDetailScale + vec3(uInstSeed * 7.3, -uTimeAnim * 0.035, uInstSeed * 3.1);
  vec3 n = texture(uNoise, fp).xyz * 2.0 - 1.0;
  vec3 voxel = worldToVoxelAtB(lp + n * uDetailAmp, vec3(0.0));
  vec4 f = bakeAt(voxel, uInstFrame, uVariantBaseB, uTemporalLerpB, dith);
  float nm = (n.x + n.y + n.z) * 0.577;
  f.rb *= (1.0 + uDetailDens * nm) * boxFade(voxel);
  float er = uErode * (0.5 - 0.5 * nm);
  f.r = max(f.r - er / (1.0 + f.r * 9.0), 0.0);
  f.b = max(f.b - er * 0.7 / (1.0 + f.b * 9.0), 0.0);
  return f;   // r=fuligem  g=temperatura  b=poeira  a=transmitância do céu
}

void main(){
  vec2 ndc = vUV * 2.0 - 1.0;
  vec4 h0 = uInvViewProj * vec4(ndc, -1.0, 1.0);
  vec4 h1 = uInvViewProj * vec4(ndc, 1.0, 1.0);
  vec3 ro = uCamPos;
  vec3 rd = normalize(h1.xyz / h1.w - h0.xyz / h0.w);

  float dz = texture(uDepth, vUV).r;
  float tScene = 1e9;
  if (dz < 0.999999){
    vec4 hw = uInvViewProj * vec4(ndc, dz * 2.0 - 1.0, 1.0);
    tScene = length(hw.xyz / hw.w - ro);
  }

  vec2 hit = rayBoxI(ro, rd, uBoxMinW, uBoxMaxW);
  hit.x = max(hit.x, uNear);
  hit.y = min(hit.y, tScene);
  if (hit.y <= hit.x){ oCol = vec4(0.0, 0.0, 0.0, 1.0); oAux = vec4(0.0, 0.0, 0.0, 1.0); return; }

  float span = hit.y - hit.x;
  float dt = span / float(uSteps);
  float t = hit.x + dt * ignoise(gl_FragCoord.xy + uJitter * 71.13);

  float cosSun = dot(rd, uKeyDir);
  float phSun = phaseDual(cosSun, uPhaseG, uPhaseBack, uPhaseMix);
  float phIso = 1.0 / (4.0 * PI);
  vec3 keyCol = envKeyColor(), skyCol = envSkyUp();
  // o macro é por camada: sem a base da variante, as variantes 1 e 2 pulavam
  // espaço pelo macro da variante 0 e perdiam pedaços da fumaça
  float macroLayer = uVariantBaseB + floor(uInstFrame);

  vec3 L = vec3(0.0);
  // transmitância por oitava de espalhamento múltiplo (como no render ao vivo)
  float Tr[4];
  for (int i = 0; i < 4; i++) Tr[i] = 1.0;
  float depthSum = 0.0, wSum = 0.0, heat = 0.0;

  for (int i = 0; i < ${MAX_STEPS_SINGLE}; i++){
    if (i >= uSteps || t >= hit.y || Tr[0] < 0.004) break;
    vec3 w = ro + rd * t;

    // ---- pula espaço vazio pelo macro assado --------------------------
    // A caixa tem 38m mas a bola de fogo ocupa uma fração dela na maior
    // parte da sequência. Sem isto a marcha gasta quase todos os passos no
    // nada: medido 11ms por instância, contra menos de 1ms com o salto.
    vec3 lpw = toLocal(w);
    if (bakeMacroAtM(worldToVoxelAtM(lpw, vec3(0.0)), macroLayer) < 0.004){
      t += (bakeMacroExitM(lpw, rd) + 1e-3) * uInstScale;
      continue;
    }

    float dith = (ignoise(gl_FragCoord.xy + float(i) * vec2(5.588, 3.17)) - 0.5) / 255.0;
    vec4 f = sampleInst(w, dith);
    if (f.r + f.b > 1e-4){
      float sigSoot = uSootExt * f.r, sigDust = uDustExt * f.b;
      float sigT = sigSoot + sigDust;
      float wS = sigSoot / max(sigT, 1e-5);
      vec3 scatCol = mix(uDustColor, uSootColor, wS);
      float albedo = mix(uDustAlbedo, uSootAlbedo, wS);

      // emissão de corpo negro, mesma LUT do solver ao vivo, com o mesmo
      // reforço na frente de chama (onde ainda há combustível queimando)
      float tN = saturate(f.g * uTempScale);
      vec4 bb = texture(uBB, vec2(tN, 0.5));
      float flame = 1.0;
      if (f.g > 0.12){
        float fuel = sampleFuelU(lpw, uInstFrame, uBakeBFrames, uVariantBaseB);
        flame += uFlameBoost * smoothstep(0.02, 0.35, fuel);
      }
      vec3 emit = bb.rgb * (pow(max(bb.a, 0.0), uEmissionCurve) * uEmissionGain * flame)
                * (sigSoot * (1.0 - uSootAlbedo));
      heat += f.g * Tr[0] * dt;

      // Luz: do cache (direcional, marchado neste quadro) quando a instância
      // tem um; senão a transmitância do céu assada serve de aproximação.
      float fadeM = boxFade(worldToVoxelAtB(lpw, vec3(0.0)));
      float skyT = mix(1.0, f.a, fadeM), sunT, fireT;
      if (uLightSlot >= 0.0){
        vec3 pc = worldToVoxelAtC(lpw, vec3(0.0));
        float zc = clamp(pc.z, 0.5, GRIDC.z - 0.5);
        float z0 = floor(zc - 0.5), fz = zc - 0.5 - z0;
        vec2 lc = mix(texture(uLightC, vec3(tileUVC(pc.xy, z0), uLightSlot)).rg,
                      texture(uLightC, vec3(tileUVC(pc.xy, z0 + 1.0), uLightSlot)).rg, fz);
        sunT = mix(1.0, lc.x, fadeM); fireT = mix(1.0, lc.y, fadeM);
      } else {
        sunT = pow(skyT, uSunOcclude);
        fireT = skyT;
      }

      float fd = length(uFirePos - w);
      vec3 fireIrr = uFireColor * uFireGain * fireT / (1.0 + fd * fd * 0.05);

      // ---- oitavas de espalhamento múltiplo (Wrenninge et al. 2013) ----
      float ae = 1.0, ab = 1.0, ap = 1.0;
      for (int o = 0; o < 4; o++){
        if (o >= uMsOctaves) break;
        float sig = sigT * ae;
        float sc = sig * albedo * ab;
        float ph = mix(phIso, phSun, ap);
        float tSun = mix(1.0, sunT, ap);
        float tSky = mix(1.0, skyT, ap * 0.7 + 0.3);
        vec3 src = scatCol * sc * (keyCol * tSun * ph
                                 + skyCol * uSkyGain * tSky * phIso
                                 + fireIrr * mix(phIso, phSun, ap * 0.5))
                 + emit * ab;
        float ext = max(sig, 1e-5);
        float trStep = exp(-ext * dt);
        L += Tr[o] * (src - src * trStep) / ext;
        Tr[o] *= trStep;
        ae *= uMsExt; ab *= uMsScatter; ap *= uMsPhase;
      }

      float c = 1.0 - Tr[0];
      depthSum += t * c; wSum += c;
    }
    t += dt;
  }

  float cov = 1.0 - Tr[0];
  oCol = vec4(L, Tr[0]);
  oAux = vec4((wSum > 1e-4 ? depthSum / wSum : hit.x) * cov, heat, cov, 1.0);
}`, 'blastInstance');

    // ---- marcha CONJUNTA ---------------------------------------------------
    // Antes cada instância era marchada sozinha e o resultado inteiro de uma
    // era composto na frente ou atrás da outra pela distância do CENTRO. Com
    // volumes que se interpenetram (pequenas e uma grande em cima) isso é
    // errado: a fumaça escura da pequena que está dentro/atrás da bola de
    // fogo aparecia colada por cima dela. Aqui um grupo de até MJ instâncias
    // que se sobrepõem na tela é marchado num raio só: em cada passo, cada
    // instância que ocupa aquele ponto entra com seu meio, e o transporte é o
    // de uma MISTURA (extinções somam, emissão soma, espalhamento ponderado
    // por σ). A ordem de profundidade fica certa ponto a ponto, e o fogo de
    // uma ilumina a fumaça da outra.
    const joint = (MJN) => new Shader(gl, FS_VS, HEAD + COMMON + ENVLUT + bake.glsl() + bake.fuelGlsl('U') + LCP + BAKE_HELPERS + `
#define MJ ${MJN}
uniform sampler2D uBB, uDepth;
uniform sampler2DArray uLightC;
uniform sampler3D uNoise;
uniform mat4 uInvViewProj;
uniform vec3 uCamPos, uKeyDir;
uniform float uNear;
uniform int uCount;
uniform vec4 uMPos[MJ];     // posição.xyz, escala
uniform vec4 uMFrm[MJ];     // quadro contínuo, base da variante, seed, camada do cache (<0 = sem)
uniform vec4 uMFire[MJ];    // luz do fogo: posição.xyz | interpolação temporal (0/1)
uniform vec4 uMFireC[MJ];   // cor da luz do fogo | passo de marcha (m)
uniform float uMNb[MJ];     // índice (no grupo) do vizinho dominante do membro, ou -1
uniform vec4 uMBoxA[MJ], uMBoxB[MJ];   // caixa justa no mundo (mín, máx)
uniform int uDbg;           // depuração: 1 fogo · 2 sol · 3 att do fogo 0 · 4 contagem de membros
uniform float uDbgK;        // compensa a exposição pra cor de depuração sair legível
uniform float uFlameBoost, uMsExt, uMsScatter, uMsPhase;
uniform int uMsOctaves;
uniform float uSootExt, uDustExt, uSootAlbedo, uDustAlbedo;
uniform vec3 uSootColor, uDustColor;
uniform float uEmissionGain, uEmissionCurve, uTempScale;
uniform float uPhaseG, uPhaseBack, uPhaseMix, uSunOcclude;
uniform float uDetailAmp, uDetailScale, uDetailDens, uErode;
uniform float uSkyGain, uFireGain, uJitter, uTimeAnim;

layout(location=0) out vec4 oCol;
layout(location=1) out vec4 oAux;

vec2 rayBoxI(vec3 ro, vec3 rd, vec3 bmin, vec3 bmax){
  vec3 inv = 1.0 / rd;
  vec3 t0 = (bmin - ro) * inv, t1 = (bmax - ro) * inv;
  vec3 tn = min(t0, t1), tf = max(t0, t1);
  return vec2(max(max(tn.x, tn.y), tn.z), min(min(tf.x, tf.y), tf.z));
}




// amostra do membro m no ponto LOCAL lp (ruído deslocado pela seed: duas
// instâncias da mesma sequência não ficam idênticas)
vec4 sampleMember(int m, vec3 lp, float dith){
  vec4 F = uMFrm[m];
  vec3 fp = lp * uDetailScale + vec3(F.z * 7.3, -uTimeAnim * 0.035, F.z * 3.1);
  vec3 n = texture(uNoise, fp).xyz * 2.0 - 1.0;
  vec3 vp = worldToVoxelAtB(lp + n * uDetailAmp, vec3(0.0));
  vec4 f = bakeAt(vp, F.x, F.y, uMFire[m].w, dith);
  float nm = (n.x + n.y + n.z) * 0.577;
  f.rb *= (1.0 + uDetailDens * nm) * boxFade(vp);
  float er = uErode * (0.5 - 0.5 * nm);
  f.r = max(f.r - er / (1.0 + f.r * 9.0), 0.0);
  f.b = max(f.b - er * 0.7 / (1.0 + f.b * 9.0), 0.0);
  return f;   // r=fuligem  g=temperatura  b=poeira  a=transmitância do céu
}

void main(){
  vec2 ndc = vUV * 2.0 - 1.0;
  vec4 h0 = uInvViewProj * vec4(ndc, -1.0, 1.0);
  vec4 h1 = uInvViewProj * vec4(ndc, 1.0, 1.0);
  vec3 ro = uCamPos;
  vec3 rd = normalize(h1.xyz / h1.w - h0.xyz / h0.w);

  float dz = texture(uDepth, vUV).r;
  float tScene = 1e9;
  if (dz < 0.999999){
    vec4 hw = uInvViewProj * vec4(ndc, dz * 2.0 - 1.0, 1.0);
    tScene = length(hw.xyz / hw.w - ro);
  }

  // intervalo do raio dentro da caixa de cada membro
  float t0[MJ], t1[MJ];
  float tmin = 1e9, tmax = -1e9, dtMin = 1e9;
  for (int m = 0; m < MJ; m++){
    t0[m] = 1e9; t1[m] = -1e9;
    if (m >= uCount) continue;
    vec2 hb = rayBoxI(ro, rd, uMBoxA[m].xyz, uMBoxB[m].xyz);
    hb.x = max(hb.x, uNear);
    hb.y = min(hb.y, tScene);
    if (hb.y > hb.x){
      t0[m] = hb.x; t1[m] = hb.y;
      tmin = min(tmin, hb.x); tmax = max(tmax, hb.y);
      dtMin = min(dtMin, uMFireC[m].w);
    }
  }
  if (tmax <= tmin){ oCol = vec4(0.0, 0.0, 0.0, 1.0); oAux = vec4(0.0, 0.0, 0.0, 1.0); return; }

  float t = tmin + dtMin * ignoise(gl_FragCoord.xy + uJitter * 71.13);

  float cosSun = dot(rd, uKeyDir);
  float phSun = phaseDual(cosSun, uPhaseG, uPhaseBack, uPhaseMix);
  float phIso = 1.0 / (4.0 * PI);
  vec3 keyCol = envKeyColor(), skyCol = envSkyUp();

  vec3 L = vec3(0.0);
  float Tr[4];
  for (int i = 0; i < 4; i++) Tr[i] = 1.0;
  float depthSum = 0.0, wSum = 0.0, heat = 0.0;

  for (int i = 0; i < ${MAX_STEPS}; i++){
    if (t >= tmax || Tr[0] < 0.004) break;
    vec3 w = ro + rd * t;
    float dith = (ignoise(gl_FragCoord.xy + float(i) * vec2(5.588, 3.17)) - 0.5) / 255.0;

    float dt = 1e9, skip = 1e9, nextIn = 1e9, heatMax = 0.0;
    float sigS = 0.0, sigD = 0.0;
    vec3 emit = vec3(0.0), Bsum = vec3(0.0);
    // Luz COMPARTILHADA no ponto, não por membro: a irradiância num ponto não
    // depende de qual fumaça está ali.
    //  - sol: produto das transmitâncias de todos os membros que contêm o
    //    ponto (raios paralelos: o produto é exato);
    //  - fogo k: atenuado pela transmitância do cache DE k até o centro dele.
    //    Sem isso a fumaça das pequenas, logo acima da bola grande, recebia o
    //    fogo inteiro sem passar por ele e virava algodão branco.
    float sunT = 1.0, skyT = 1.0;
    // por membro que contém o ponto: transmitância até o próprio fogo, até o
    // fogo vizinho dominante, e a do céu (aproximação pros demais fogos)
    float fTk[MJ], skyk[MJ], nbF[MJ];
    bool inK[MJ], dense[MJ];
    bool any = false;

    // 1) densidade: só onde o bloco macro do membro está ocupado
    for (int m = 0; m < MJ; m++){
      fTk[m] = 1.0; skyk[m] = 1.0; nbF[m] = 1.0; inK[m] = false; dense[m] = false;
      if (m >= uCount) break;
      if (t < t0[m] || t >= t1[m]){
        if (t < t0[m]) nextIn = min(nextIn, t0[m]);
        continue;
      }
      inK[m] = true;
      vec4 P = uMPos[m], F = uMFrm[m];
      vec3 lp = (w - P.xyz) / P.w;
      if (bakeMacroAtM(worldToVoxelAtM(lp, vec3(0.0)), F.y + floor(F.x)) < 0.004){
        skip = min(skip, (bakeMacroExitM(lp, rd) + 1e-3) * P.w);
        continue;
      }
      any = true;
      dense[m] = true;
      dt = min(dt, uMFireC[m].w);
      vec4 f = sampleMember(m, lp, dith);
      skyk[m] = f.a;
      if (f.r + f.b <= 1e-4) continue;
      float ss = uSootExt * f.r, sd = uDustExt * f.b, st = ss + sd;
      float wS = ss / max(st, 1e-5);
      Bsum += mix(uDustColor, uSootColor, wS) * st * mix(uDustAlbedo, uSootAlbedo, wS);
      // emissão de corpo negro com o reforço da frente de chama
      vec4 bb = texture(uBB, vec2(saturate(f.g * uTempScale), 0.5));
      float flame = 1.0;
      if (f.g > 0.12){
        flame += uFlameBoost * smoothstep(0.02, 0.35, sampleFuelU(lp, F.x, uBakeBFrames, F.y));
      }
      emit += bb.rgb * (pow(max(bb.a, 0.0), uEmissionCurve) * uEmissionGain * flame)
            * (ss * (1.0 - uSootAlbedo));
      heatMax = max(heatMax, f.g);
      sigS += ss; sigD += sd;
    }

    if (!any){
      // ninguém ocupado neste ponto: salta os blocos vazios, mas sem passar
      // da entrada de outra caixa
      float nt = min(t + skip, nextIn);
      if (nt > 1e8) break;
      t = max(nt, t + 1e-3);
      continue;
    }

    // 2) luz: de TODO membro cuja caixa contém o ponto, ocupado aqui ou não.
    // A sombra e a luz do fogo de uma explosão alcançam a fumaça da vizinha
    // mesmo onde ela própria está vazia — pular essa consulta junto com a
    // densidade criava uma aresta reta na face dos blocos macro.
    for (int m = 0; m < MJ; m++){
      if (m >= uCount) break;
      if (!inK[m]) continue;
      vec4 P = uMPos[m], F = uMFrm[m];
      vec3 lp = (w - P.xyz) / P.w;
      vec3 vb = worldToVoxelAtB(lp, vec3(0.0));
      // perto do teto e das laterais da caixa tudo vai a 1 junto com a
      // densidade (boxFade), senão a borda reta da caixa vira uma linha
      float fadeM = boxFade(vb);
      // céu: só de quem tem densidade aqui (quem está vazio no ponto quase
      // não oclui o céu dele; poupa 2 leituras por membro por passo)
      float sky = dense[m] ? mix(1.0, skyk[m], fadeM) : 1.0;
      if (!dense[m] && F.w < 0.0){ skyk[m] = 1.0; continue; }   // sem cache: nada a somar
      vec3 tr3;
      if (F.w >= 0.0){
        vec3 pc = worldToVoxelAtC(lp, vec3(0.0));
        float zc = clamp(pc.z, 0.5, GRIDC.z - 0.5);
        float z0 = floor(zc - 0.5), fz = zc - 0.5 - z0;
        tr3 = mix(texture(uLightC, vec3(tileUVC(pc.xy, z0), F.w)).rgb,
                  texture(uLightC, vec3(tileUVC(pc.xy, z0 + 1.0), F.w)).rgb, fz);
      } else {
        tr3 = vec3(pow(sky, uSunOcclude), sky, sky);
      }
      tr3 = mix(vec3(1.0), tr3, fadeM);
      sunT *= tr3.x;
      skyT *= sky;
      fTk[m] = tr3.y; skyk[m] = sky;
      // fator do fogo vizinho dominante relativo à aproximação pelo céu, que
      // é o que o laço de fogo aplica por padrão a todo membro
      int nb = int(uMNb[m]);
      if (nb >= 0) nbF[nb] *= tr3.z / max(sky, 1e-3);
    }

    // Fogo de cada membro k no ponto: atenuado pela fumaça DELE (cache, até o
    // centro dele) e pela das OUTRAS que contêm o ponto. Pra essas não há
    // marcha na direção de k; a transmitância do céu delas no ponto mede o
    // quanto ele está enterrado ali — a mesma aproximação que a instância sem
    // cache já usa pro próprio fogo. Sem ela, a fumaça das pequenas recebia o
    // fogo da grande por igual no volume inteiro e virava algodão chapado.
    // Atenuação do fogo k = produto, sobre as fumaças que contêm o ponto, de:
    // a própria (cache, até k) · quem tem k como vizinho dominante (cache) ·
    // os demais (aproximação pelo céu). Fatorado como skyT × correções, fica
    // O(N) em vez de O(N²) por passo.
    vec3 fireIrr = vec3(0.0);
    for (int k = 0; k < MJ; k++){
      if (k >= uCount) break;
      vec3 fc = uMFireC[k].rgb;
      if (fc.r + fc.g + fc.b < 1e-4) continue;
      float att = skyT * nbF[k];
      if (inK[k]) att *= fTk[k] / max(skyk[k], 1e-3);
      float fdist = length(uMFire[k].xyz - w);
      fireIrr += fc * (min(att, 1.0) / (1.0 + fdist * fdist * 0.05));
    }
    fireIrr *= uFireGain;
    if (uDbg > 0 && sigS + sigD > 1e-3){
      vec3 dv = uDbg == 1 ? vec3(log(1.0 + luma(fireIrr)) * 0.25)
              : uDbg == 2 ? vec3(sunT)
              : uDbg == 3 ? vec3(fTk[0], nbF[0], skyk[0])
              : vec3(0.0);
      if (uDbg == 4){ float c = 0.0; for (int m = 0; m < MJ; m++){ if (m < uCount && inK[m]) c += 1.0; } dv = vec3(c * 0.25); }
      oCol = vec4(dv * uDbgK, 0.0); oAux = vec4(t, 0.0, 1.0, 1.0); return;
    }

    float sigT = sigS + sigD;
    if (sigT > 1e-6){
      // oitavas de espalhamento múltiplo (Wrenninge et al. 2013) da mistura:
      // octaves altas com fase mais isotrópica e sombra mais "vazada"
      float ae = 1.0, ab = 1.0, ap = 1.0;
      for (int o = 0; o < 3; o++){
        if (o >= uMsOctaves) break;
        float ph = mix(phIso, phSun, ap);
        vec3 Ao = Bsum * (keyCol * mix(1.0, sunT, ap) * ph
                        + skyCol * uSkyGain * mix(1.0, skyT, ap * 0.7 + 0.3) * phIso
                        + fireIrr * mix(phIso, phSun, ap * 0.5));
        vec3 src = Ao * (ae * ab) + emit * ab;
        float ext = max(sigT * ae, 1e-5);
        float trStep = exp(-ext * dt);
        L += Tr[o] * (src - src * trStep) / ext;
        Tr[o] *= trStep;
        ae *= uMsExt; ab *= uMsScatter; ap *= uMsPhase;
      }
      heat += heatMax * Tr[0] * dt;
      float c = 1.0 - Tr[0];
      depthSum += t * c; wSum += c;
    }
    t += dt;
  }

  float cov = 1.0 - Tr[0];
  oCol = vec4(L, Tr[0]);
  oAux = vec4((wSum > 1e-4 ? depthSum / wSum : tmin) * cov, heat, cov, 1.0);
}`, `blastJoint${MJN}`);
    this.shJoint = { [MJ_SMALL]: joint(MJ_SMALL), [MJ]: joint(MJ) };
  }

  spawn(pos, { magnitude = 'tanque', seed = Math.random(), jitter = 0.14 } = {}) {
    const M = magnitudeOf(magnitude);
    const j = 1 + (Math.random() - 0.5) * jitter;
    const o = {
      pos: Float32Array.from(pos), t: 0, seed,
      scale: M.scale * j,
      speed: M.speed / j,
      // só variantes já assadas: o bake é incremental e a primeira fica
      // pronta bem antes das outras
      variant: (Math.random() * Math.max(1, this.bake.variantsReady)) | 0,
      mag: M.id,
    };
    this.list.push(o);
    this.scorches.push({ x: pos[0], z: pos[2], r: 5.2 * M.scale, age: 0 });
    if (this.scorches.length > this.maxScorch) this.scorches.shift();
    return o;
  }

  /** vec4 por marca: (x, z, raio atual, intensidade) */
  scorchData() {
    const out = new Float32Array(this.maxScorch * 4);
    this.scorches.forEach((c, i) => {
      // o queimado acompanha a expansão da bola de fogo (Sedov ~ t^0.4) e
      // escurece rápido; depois fica, como decalque
      const grow = Math.min(1, Math.pow(c.age / 0.6, 0.4));
      out.set([c.x, c.z, c.r * (0.35 + 0.65 * grow), Math.min(1, c.age / 0.25) * 0.9], i * 4);
    });
    return { data: out, n: this.scorches.length };
  }

  update(dt) {
    const dur = this.bake.duration;
    for (const o of this.list) o.t += dt * o.speed;
    for (const c of this.scorches) c.age += dt;
    this.list = this.list.filter((o) => o.t < dur);
  }

  /** luz da instância, interpolada da curva assada */
  lightOf(o) {
    const B = this.bake;
    const f = B.frameOfTime(o.t);
    const i0 = Math.floor(f), i1 = Math.min(B.frames - 1, i0 + 1), a = f - i0;
    const vb = (o.variant || 0) * B.frames;
    const j0 = vb + i0, j1 = vb + i1;
    const lerp = (arr, k) => arr[j0 * 3 + k] + (arr[j1 * 3 + k] - arr[j0 * 3 + k]) * a;
    const s = o.scale, k = this.params.lightScale * s * s;
    return {
      pos: [o.pos[0] + lerp(B.lightPos, 0) * s,
            lerp(B.lightPos, 1) * s,
            o.pos[2] + lerp(B.lightPos, 2) * s],
      color: [lerp(B.lightCol, 0) * k, lerp(B.lightCol, 1) * k, lerp(B.lightCol, 2) * k],
    };
  }

  /**
   * Caixa JUSTA da instância no mundo: a união das caixas assadas dos dois
   * quadros que ela interpola, alargada pelo deslocamento de ruído de
   * detalhe. Sem caixas assadas (bake ainda sem elas), a caixa do domínio.
   * Retorna null se os dois quadros estão vazios.
   */
  _box(o) {
    const B = this.bake, G = B.grid, s = o.scale, b = B.bounds;
    let mn, mx;
    if (b) {
      const f = B.frameOfTime(o.t), base = (o.variant || 0) * B.frames;
      const l0 = base + Math.floor(f), l1 = base + Math.min(B.frames - 1, Math.floor(f) + 1);
      mn = [1e9, 1e9, 1e9]; mx = [-1e9, -1e9, -1e9];
      for (const l of [l0, l1]) {
        if (b[l * 6 + 3] <= b[l * 6]) continue;            // quadro vazio
        for (let k = 0; k < 3; k++) {
          mn[k] = Math.min(mn[k], b[l * 6 + k]);
          mx[k] = Math.max(mx[k], b[l * 6 + 3 + k]);
        }
      }
      if (mx[0] < mn[0]) return null;
      const pad = this.params.detailAmp + 2 * G.cell;
      for (let k = 0; k < 3; k++) {
        mn[k] = Math.max(G.domainMin[k], mn[k] - pad);
        mx[k] = Math.min(G.domainMin[k] + G.domainSize[k], mx[k] + pad);
      }
    } else {
      mn = G.domainMin.slice();
      mx = [mn[0] + G.domainSize[0], mn[1] + G.domainSize[1], mn[2] + G.domainSize[2]];
    }
    return {
      min: [o.pos[0] + mn[0] * s, o.pos[1] + mn[1] * s, o.pos[2] + mn[2] * s],
      max: [o.pos[0] + mx[0] * s, o.pos[1] + mx[1] * s, o.pos[2] + mx[2] * s],
    };
  }

  /** caixa justa projetada em tela, em pixels — vira scissor */
  _screenRect(o, cam, w, h, box = this._box(o)) {
    if (!box) return null;
    const mn = box.min, sz = [box.max[0] - mn[0], box.max[1] - mn[1], box.max[2] - mn[2]];
    const m = cam.viewProj;
    let x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
    for (let c = 0; c < 8; c++) {
      const p = [mn[0] + (c & 1 ? sz[0] : 0), mn[1] + (c & 2 ? sz[1] : 0), mn[2] + (c & 4 ? sz[2] : 0)];
      const cw = m[3] * p[0] + m[7] * p[1] + m[11] * p[2] + m[15];
      if (cw <= 1e-4) { return [0, 0, w, h]; }   // atravessa a câmera: tela toda
      const cx = (m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12]) / cw;
      const cy = (m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13]) / cw;
      x0 = Math.min(x0, cx); x1 = Math.max(x1, cx);
      y0 = Math.min(y0, cy); y1 = Math.max(y1, cy);
    }
    const px0 = Math.max(0, Math.floor((x0 * 0.5 + 0.5) * w) - 2);
    const px1 = Math.min(w, Math.ceil((x1 * 0.5 + 0.5) * w) + 2);
    const py0 = Math.max(0, Math.floor((y0 * 0.5 + 0.5) * h) - 2);
    const py1 = Math.min(h, Math.ceil((y1 * 0.5 + 0.5) * h) + 2);
    if (px1 <= px0 || py1 <= py0) return null;
    return [px0, py0, px1 - px0, py1 - py0];
  }

  /**
   * Luzes das instâncias ativas, com o transform necessário pra cena marchar
   * a sombra volumétrica. Ordenadas por intensidade.
   */
  lights(max = 8) {
    const all = this.list.map((o, idx) => {
      const L = this.lightOf(o);
      L.idx = idx;                 // identidade: o sombreador pula a si mesmo
      L.xform = [o.pos[0], o.pos[1], o.pos[2], o.scale];
      L.frame = (o.variant || 0) * this.bake.frames + this.bake.frameOfTime(o.t);
      // Flash da detonação: pulso curto (~35 ms de sequência) antes da curva
      // assada subir. Ilumina a cena inteira por um instante — o "clarão" que
      // a câmera vê antes de a exposição reagir.
      const fg = this.params.flashGain || 0;
      if (fg > 0 && o.t < 8 * this.params.flashTau) {
        const k = fg * o.scale * o.scale * Math.exp(-o.t / this.params.flashTau);
        L.color[0] += k; L.color[1] += k * 0.86; L.color[2] += k * 0.64;
      }
      L.power = L.color[0] + L.color[1] + L.color[2];
      L.s2 = o.scale * o.scale;    // raio da fonte extensa (ver scene.js)
      return L;
    });
    // Compressão LOCAL da barragem. Com a luz cheia (igual à da simulação ao
    // vivo), 12 explosões juntas à noite somavam e o chão estourava pra
    // branco. Cada luz é atenuada pela luz que as VIZINHAS jogam no ponto
    // dela, com a mesma queda 1/(1+0.02d²) da cena:
    //   f = (P + κ·N) / (P + N)
    // Isolada → N≈0 → f=1, idêntica à ao vivo. Aglomerado de n iguais →
    // f ≈ (1 + κ(n−1))/n: a soma cresce sublinear em vez de linear.
    const K = this.params.crowdKeep;
    for (const L of all) {
      let N = 0;
      for (const M of all) {
        if (M === L) continue;
        const dx = M.pos[0] - L.pos[0], dy = M.pos[1] - L.pos[1], dz = M.pos[2] - L.pos[2];
        N += M.power / (M.s2 + 0.02 * (dx * dx + dy * dy + dz * dz));
      }
      // a própria luz no centro dela vale P/s²: é contra isso que N compete
      const own = L.power / L.s2;
      const f = (own > 1e-6 ? (own + K * N) / (own + N) : 1) * (this.params.lightGain ?? 1);
      for (let c = 0; c < 3; c++) L.color[c] *= f;
      L.power *= f;
    }
    return all.sort((a, b) => b.power - a.power).slice(0, max);
  }

  /** instâncias que mais importam pra SOMBRA (densas, não só luminosas) */
  shadowCasters(cam, max = 4) {
    return this.list
      .map((o, idx) => {
        const dx = o.pos[0] - cam.pos[0], dz = o.pos[2] - cam.pos[2];
        return { o, idx, d: dx * dx + dz * dz };
      })
      .sort((a, b) => a.d - b.d)
      .slice(0, max)
      .map(({ o, idx }) => ({
        idx,
        xform: [o.pos[0], o.pos[1], o.pos[2], o.scale],
        frame: (o.variant || 0) * this.bake.frames + this.bake.frameOfTime(o.t),
      }));
  }

  /**
   * Instâncias visíveis agrupadas pela INTERSEÇÃO 3D das caixas justas
   * (union-find). Só volumes que se interpenetram precisam da marcha
   * conjunta; os demais compõem certo em ordem de profundidade. Cada grupo
   * vem ordenado da mais próxima à mais distante.
   */
  _groups(cam, w, h) {
    const vis = [];
    for (const o of this.list) {
      const box = this._box(o);
      const r = this._screenRect(o, cam, w, h, box);
      if (!r) continue;
      const dx = o.pos[0] - cam.pos[0], dz = o.pos[2] - cam.pos[2];
      vis.push({ o, r, box, d: dx * dx + dz * dz });
    }
    const par = vis.map((_, i) => i);
    const find = (i) => { while (par[i] !== i) i = par[i] = par[par[i]]; return i; };
    for (let i = 0; i < vis.length; i++) {
      const a = vis[i].box;
      for (let j = i + 1; j < vis.length; j++) {
        const b = vis[j].box;
        if (a.min[0] < b.max[0] && b.min[0] < a.max[0] && a.min[1] < b.max[1] && b.min[1] < a.max[1]
            && a.min[2] < b.max[2] && b.min[2] < a.max[2]) {
          par[find(i)] = find(j);
        }
      }
    }
    const groups = new Map();
    vis.forEach((v, i) => {
      const k = find(i);
      if (!groups.has(k)) groups.set(k, []);
      groups.get(k).push(v);
    });
    const out = [...groups.values()];
    for (const g of out) g.sort((x, y) => x.d - y.d);
    return out;
  }

  /**
   * Recalcula o cache de luz das instâncias mais visíveis (maior área em
   * tela). As demais ficam com a aproximação pelo céu — numa barragem de 20
   * explosões, as pequenas no fundo não mostram a diferença.
   * Precisa rodar ANTES do draw, com o alvo de volume ainda não bindado.
   */
  updateLightCache(cam, keyDir, w, h) {
    const gl = this.gl, P = this.params, B = this.bake;
    for (const o of this.list) { o.lcSlot = -1; o.nb = null; o.group = null; }
    if (!this.list.length || !B.variantsReady) return 0;
    const groups = this._groups(cam, w, h);
    const cand = [];
    for (const g of groups) {
      for (const v of g) {
        v.o.group = g.length > 1 ? g : null;
        const a = (v.r[2] * v.r[3]) / (w * h);
        if (a >= P.lcMinArea) cand.push({ o: v.o, a });
      }
    }
    cand.sort((x, y) => y.a - x.a);
    const n = Math.min(cand.length, LC_SLOTS, P.lcSlots ?? LC_SLOTS);
    if (!n) return 0;

    gl.disable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);
    gl.disable(gl.SCISSOR_TEST);
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.lcFbo);
    gl.drawBuffers([gl.COLOR_ATTACHMENT0]);
    gl.viewport(0, 0, this.lcGrid.atlasW, this.lcGrid.atlasH);
    const sh = this.shLight.use();
    sh.set('uSunDirL', keyDir).set('uSootExt', P.sootExt).set('uDustExt', P.dustExt)
      .set('uErodeMean', P.erode * 0.5).set('uBakeBFrames', B.frames)
      // um quadro só: a luz varia devagar e a interpolação temporal dobrava
      // o número de leituras por passo da marcha
      .set('uTemporalLerpB', 0);
    sh.seti('uSunSteps', P.lightSteps).seti('uFireSteps', P.fireSteps);
    // luz de todos (pra achar o vizinho dominante de cada um, só no grupo)
    const lightMap = new Map(this.list.map((o) => [o, this.lightOf(o)]));
    for (let k = 0; k < n; k++) {
      const o = cand[k].o;
      o.lcSlot = k;
      const L = this.lightOf(o);
      // vizinho que mais ilumina o centro desta explosão
      let best = null, bestE = 1.0;
      const peers = o.group ? o.group.map((v) => ({ o: v.o, L: lightMap.get(v.o) })) : [];
      for (const q of peers) {
        if (q.o === o) continue;
        const dx = q.L.pos[0] - o.pos[0], dy = q.L.pos[1] - 1.85 * o.scale, dz = q.L.pos[2] - o.pos[2];
        const c = q.L.color, E = (c[0] + c[1] + c[2]) / (1 + 0.05 * (dx * dx + dy * dy + dz * dz));
        if (E > bestE) { bestE = E; best = q; }
      }
      o.nb = best ? best.o : null;
      sh.set('uHasNb', best ? 1 : 0).set('uFull', o.group ? 1 : 0);
      if (best) {
        sh.set('uNbL', [(best.L.pos[0] - o.pos[0]) / o.scale, best.L.pos[1] / o.scale,
                        (best.L.pos[2] - o.pos[2]) / o.scale]);
      }
      gl.framebufferTextureLayer(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, this.lcTex, 0, k);
      sh._unit = 0;
      sh.set('uInstScale', o.scale).set('uInstFrame', Math.round(B.frameOfTime(o.t)))
        .set('uVariantBaseB', (o.variant || 0) * B.frames)
        .set('uFireL', [(L.pos[0] - o.pos[0]) / o.scale, L.pos[1] / o.scale,
                        (L.pos[2] - o.pos[2]) / o.scale]);
      sh.tex('uBakeB', B.tex, gl.TEXTURE_2D_ARRAY)
        .tex('uBakeMacroM', B.macroTex, gl.TEXTURE_2D_ARRAY);
      drawFS(gl);
    }
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    return n;
  }

  /** uniforms comuns aos dois shaders de instância */
  _setCommon(sh, cam, env, depthTex) {
    const gl = this.gl, P = this.params, B = this.bake;
    sh.set('uInvViewProj', cam.invViewProj).set('uCamPos', cam.pos)
      .set('uKeyDir', env.keyDir).set('uNear', cam.near)
      .set('uJitter', env.frameJitter).set('uTimeAnim', env.time)
      .set('uSootExt', P.sootExt).set('uDustExt', P.dustExt)
      .set('uSootAlbedo', P.sootAlbedo).set('uDustAlbedo', P.dustAlbedo)
      .set('uSootColor', P.sootColor).set('uDustColor', P.dustColor)
      .set('uEmissionGain', P.emissionGain).set('uEmissionCurve', P.emissionCurve)
      .set('uTempScale', P.tempScale)
      .set('uPhaseG', P.phaseG).set('uPhaseBack', P.phaseBack).set('uPhaseMix', P.phaseMix)
      .set('uSunOcclude', P.sunOcclude)
      .set('uDetailAmp', P.detailAmp).set('uDetailScale', P.detailScale)
      .set('uDetailDens', P.detailDens).set('uErode', P.erode)
      .set('uSkyGain', P.skyGain).set('uFireGain', P.fireGain)
      .set('uBakeBFrames', B.frames)
      .set('uFlameBoost', P.flameBoost)
      .set('uMsExt', P.msExt).set('uMsScatter', P.msScatter).set('uMsPhase', P.msPhase);
    sh.seti('uMsOctaves', P.msOctaves);
    sh.tex('uBakeB', B.tex, gl.TEXTURE_2D_ARRAY)
      .tex('uBakeMacroM', B.macroTex, gl.TEXTURE_2D_ARRAY)
      .tex('uBakeFuelU', B.fuelTex, gl.TEXTURE_2D_ARRAY)
      .tex('uLightC', this.lcTex, gl.TEXTURE_2D_ARRAY)
      .tex('uBB', this.bbTex).tex('uDepth', depthTex)
      .tex('uEnvLut', env.envLut).tex('uNoise', this.noiseTex, gl.TEXTURE_3D);
  }

  /** LOD por área em tela: passos e interpolação temporal */
  _lod(r, screenArea) {
    const P = this.params;
    const frac = (r[2] * r[3]) / screenArea;
    const lod = Math.sqrt(Math.min(frac / P.refArea, 1));
    return { frac, steps: Math.max(P.stepsMin, Math.round(P.steps * lod)),
             lerp: frac > P.refArea * 0.25 ? 1 : 0 };
  }

  /**
   * Desenha as instâncias no alvo JÁ bindado. Instância SOZINHA na tela (o
   * caso comum) vai pelo shader enxuto. As que se sobrepõem formam um grupo
   * marchado JUNTO (ver o shader conjunto); um grupo maior que MJ vira
   * pedaços em ordem de profundidade (front-to-back, como o blend espera).
   * Grupos sem sobreposição são independentes: a ordem entre eles não importa.
   */
  draw(cam, env, depthTex, w, h) {
    const gl = this.gl, P = this.params, B = this.bake;
    if (!this.list.length || !B.variantsReady) return 0;
    const groups = this._groups(cam, w, h);
    if (!groups.length) return 0;
    const screenArea = w * h, kVol = 1 / P.lightScale;
    // P.joint = 1 força o caminho antigo (uma por vez) — pra comparar
    const J = Math.max(1, Math.min(MJ, P.joint ?? MJ));

    // Itens de desenho: instâncias sozinhas e pedaços de grupos. Agora dois
    // itens podem se sobrepor em TELA (sem se tocar em 3D), então todos vão
    // em ordem de profundidade, front-to-back, como o blend espera.
    const items = [];
    for (const g of groups) {
      if (g.length === 1 || J === 1) { for (const v of g) items.push({ single: v, d: v.d }); continue; }
      // Grupo que cabe numa marcha (≤ MJ): composição exata. Maior que isso
      // (barragem enorme), a divisão em pedaços já é aproximada de qualquer
      // jeito, e pedaços de MJ_SMALL custam bem menos que de MJ.
      const step = g.length <= Math.min(J, MJ) ? g.length : Math.min(J, MJ_SMALL);
      for (let c0 = 0; c0 < g.length; c0 += step) {
        const chunk = g.slice(c0, c0 + step);
        items.push({ chunk, d: chunk[0].d });
      }
    }
    items.sort((a, b) => a.d - b.d);

    gl.enable(gl.SCISSOR_TEST);
    let drawn = 0, cur = null;
    const mPos = new Float32Array(MJ * 4), mFrm = new Float32Array(MJ * 4);
    const mFire = new Float32Array(MJ * 4), mFireC = new Float32Array(MJ * 4);
    const mNb = new Float32Array(MJ), mBoxA = new Float32Array(MJ * 4), mBoxB = new Float32Array(MJ * 4);
    for (const it of items) {
      if (it.single) {
        const sh = this.shSingle;
        if (cur !== sh) { sh.use(); this._setCommon(sh, cam, env, depthTex); cur = sh; }
        const { o, r, box } = it.single;
        gl.scissor(r[0], r[1], r[2], r[3]);
        const lod = this._lod(r, screenArea);
        const L = this.lightOf(o);
        sh.seti('uSteps', lod.steps);
        sh.set('uTemporalLerpB', lod.lerp)
          .set('uInstPos', o.pos).set('uInstScale', o.scale)
          .set('uBoxMinW', box.min).set('uBoxMaxW', box.max)
          .set('uInstFrame', B.frameOfTime(o.t)).set('uInstSeed', o.seed)
          .set('uVariantBaseB', (o.variant || 0) * B.frames)
          .set('uFirePos', L.pos)
          // dentro do PRÓPRIO volume a luz do fogo é a da simulação, sem o
          // lightScale — esse fator só existe pra luz na cena
          .set('uFireColor', L.color.map((c) => c * kVol))
          .set('uLightSlot', o.lcSlot ?? -1);
        drawFS(gl);
        drawn++;
        continue;
      }
      const chunk = it.chunk;
      const sh = this.shJoint[chunk.length <= MJ_SMALL ? MJ_SMALL : MJ];
      if (cur !== sh) {
        sh.use(); this._setCommon(sh, cam, env, depthTex); cur = sh;
        sh.seti('uDbg', P.debug | 0).set('uDbgK', P.debugK ?? 1);
      }
      let x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
      mPos.fill(0); mFrm.fill(0); mFire.fill(0); mFireC.fill(0); mNb.fill(-1);
      mBoxA.fill(0); mBoxB.fill(0);
      chunk.forEach(({ o, r, box }, m) => {
        x0 = Math.min(x0, r[0]); y0 = Math.min(y0, r[1]);
        x1 = Math.max(x1, r[0] + r[2]); y1 = Math.max(y1, r[1] + r[3]);
        const lod = this._lod(r, screenArea);
        const L = this.lightOf(o);
        const ext = Math.max(box.max[0] - box.min[0], box.max[1] - box.min[1], box.max[2] - box.min[2]);
        mPos.set([o.pos[0], o.pos[1], o.pos[2], o.scale], m * 4);
        mBoxA.set(box.min, m * 4); mBoxB.set(box.max, m * 4);
        mFrm.set([B.frameOfTime(o.t), (o.variant || 0) * B.frames, o.seed, o.lcSlot ?? -1], m * 4);
        mFire.set([L.pos[0], L.pos[1], L.pos[2], lod.lerp], m * 4);
        // passo do membro pela caixa justa: o mesmo número de passos de uma
        // instância sozinha atravessando-a
        mFireC.set([L.color[0] * kVol, L.color[1] * kVol, L.color[2] * kVol,
                    0.8 * ext / lod.steps], m * 4);
        mNb[m] = o.nb ? chunk.findIndex((c) => c.o === o.nb) : -1;
      });
      gl.scissor(x0, y0, x1 - x0, y1 - y0);
      sh.seti('uCount', chunk.length);
      // só o tamanho do array da variante (a de 4 não aceita 8 elementos)
      const nv = chunk.length <= MJ_SMALL ? MJ_SMALL : MJ, q = nv * 4;
      gl.uniform4fv(sh.loc('uMPos[0]'), mPos.subarray(0, q));
      gl.uniform4fv(sh.loc('uMFrm[0]'), mFrm.subarray(0, q));
      gl.uniform4fv(sh.loc('uMFire[0]'), mFire.subarray(0, q));
      gl.uniform4fv(sh.loc('uMFireC[0]'), mFireC.subarray(0, q));
      gl.uniform1fv(sh.loc('uMNb[0]'), mNb.subarray(0, nv));
      gl.uniform4fv(sh.loc('uMBoxA[0]'), mBoxA.subarray(0, q));
      gl.uniform4fv(sh.loc('uMBoxB[0]'), mBoxB.subarray(0, q));
      drawFS(gl);
      drawn += chunk.length;
    }
    gl.disable(gl.SCISSOR_TEST);
    return drawn;
  }
}
