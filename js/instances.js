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

const MAX_STEPS = 96;
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
    gl.texImage3D(gl.TEXTURE_2D_ARRAY, 0, gl.RG8, this.lcGrid.atlasW, this.lcGrid.atlasH,
                  LC_SLOTS, 0, gl.RG, gl.UNSIGNED_BYTE, null);
    for (const [k, v] of [['TEXTURE_MIN_FILTER', gl.LINEAR], ['TEXTURE_MAG_FILTER', gl.LINEAR],
                          ['TEXTURE_WRAP_S', gl.CLAMP_TO_EDGE], ['TEXTURE_WRAP_T', gl.CLAMP_TO_EDGE]]) {
      gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl[k], v);
    }
    this.lcFbo = gl.createFramebuffer();
    const LCP = this.lcGrid.glsl('C');
    this.shLight = new Shader(gl, FS_VS, HEAD + COMMON + bake.glsl() + LCP + `
uniform vec3 uSunDirL, uFireL;
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
  vec4 f = sampleBake4B(worldToVoxelAtB(lp, vec3(0.0)), uInstFrame);
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
  // célula vazia: não há nada pra iluminar ali, a marcha é desperdício
  if (bakeMacroAtM(worldToVoxelAtM(lp, vec3(0.0)), macroLayer()) < 0.004){
    oCol = vec4(1.0); return;
  }
  vec3 tf = uFireL - lp;
  float fd = length(tf);
  oCol = vec4(march(lp, uSunDirL, 1e4, uSunSteps, 60.0),
              // tauCap baixo, como na simulação: a luz pontual no centróide
              // atravessaria a bola de fogo inteira e apagaria a fumaça colada
              fd < 1e-3 ? 1.0 : march(lp, tf / fd, fd, uFireSteps, 2.2),
              0.0, 1.0);
}`, 'instLightCache');

    this.sh = new Shader(gl, FS_VS, HEAD + COMMON + ENVLUT + bake.glsl() + bake.fuelGlsl('U') + LCP + `
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

// mundo → espaço local da instância (desfaz posição e escala)
vec3 toLocal(vec3 w){ return (w - uInstPos) / uInstScale; }

vec4 sampleInst(vec3 w){
  vec3 lp = toLocal(w);
  // ruído no espaço LOCAL e deslocado pela seed: duas instâncias da mesma
  // sequência não ficam idênticas
  vec3 fp = lp * uDetailScale + vec3(uInstSeed * 7.3, -uTimeAnim * 0.035, uInstSeed * 3.1);
  vec3 n = texture(uNoise, fp).xyz * 2.0 - 1.0;
  vec3 voxel = worldToVoxelAtB(lp + n * uDetailAmp, vec3(0.0));
  vec4 f = sampleBake4B(voxel, uInstFrame);
  float nm = (n.x + n.y + n.z) * 0.577;
  f.rb *= 1.0 + uDetailDens * nm;
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

  vec3 bmin = uInstPos + BASE_MINB * uInstScale;
  vec3 bmax = bmin + DOMAIN_SIZEB * uInstScale;
  vec2 hit = rayBoxI(ro, rd, bmin, bmax);
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

  for (int i = 0; i < ${MAX_STEPS}; i++){
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

    vec4 f = sampleInst(w);
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
      float skyT = f.a, sunT, fireT;
      if (uLightSlot >= 0.0){
        vec3 pc = worldToVoxelAtC(lpw, vec3(0.0));
        float zc = clamp(pc.z, 0.5, GRIDC.z - 0.5);
        float z0 = floor(zc - 0.5), fz = zc - 0.5 - z0;
        vec2 lc = mix(texture(uLightC, vec3(tileUVC(pc.xy, z0), uLightSlot)).rg,
                      texture(uLightC, vec3(tileUVC(pc.xy, z0 + 1.0), uLightSlot)).rg, fz);
        sunT = lc.x; fireT = lc.y;
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

  /** AABB da instância projetado em tela, em pixels — vira scissor */
  _screenRect(o, cam, w, h) {
    const B = this.bake.grid;
    const s = o.scale;
    const mn = [o.pos[0] + B.domainMin[0] * s, B.domainMin[1] * s, o.pos[2] + B.domainMin[2] * s];
    const sz = [B.domainSize[0] * s, B.domainSize[1] * s, B.domainSize[2] * s];
    const m = cam.viewProj;
    let x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9, anyFront = false;
    for (let c = 0; c < 8; c++) {
      const p = [mn[0] + (c & 1 ? sz[0] : 0), mn[1] + (c & 2 ? sz[1] : 0), mn[2] + (c & 4 ? sz[2] : 0)];
      const cw = m[3] * p[0] + m[7] * p[1] + m[11] * p[2] + m[15];
      if (cw <= 1e-4) { return [0, 0, w, h]; }   // atravessa a câmera: tela toda
      anyFront = true;
      const cx = (m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12]) / cw;
      const cy = (m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13]) / cw;
      x0 = Math.min(x0, cx); x1 = Math.max(x1, cx);
      y0 = Math.min(y0, cy); y1 = Math.max(y1, cy);
    }
    if (!anyFront) return null;
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
   * Recalcula o cache de luz das instâncias mais visíveis (maior área em
   * tela). As demais ficam com a aproximação pelo céu — numa barragem de 20
   * explosões, as pequenas no fundo não mostram a diferença.
   * Precisa rodar ANTES do draw, com o alvo de volume ainda não bindado.
   */
  updateLightCache(cam, keyDir, w, h) {
    const gl = this.gl, P = this.params, B = this.bake;
    for (const o of this.list) o.lcSlot = -1;
    if (!this.list.length || !B.variantsReady) return 0;
    const cand = [];
    for (const o of this.list) {
      const r = this._screenRect(o, cam, w, h);
      if (!r) continue;
      const a = (r[2] * r[3]) / (w * h);
      if (a >= P.lcMinArea) cand.push({ o, a });
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
    for (let k = 0; k < n; k++) {
      const o = cand[k].o;
      o.lcSlot = k;
      const L = this.lightOf(o);
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

  /** desenha todas as instâncias no alvo JÁ bindado, front-to-back */
  draw(cam, env, depthTex, w, h) {
    const gl = this.gl, P = this.params;
    if (!this.list.length || !this.bake.variantsReady) return 0;

    const sorted = [...this.list].sort((a, b) => {
      const da = (a.pos[0] - cam.pos[0]) ** 2 + (a.pos[2] - cam.pos[2]) ** 2;
      const db = (b.pos[0] - cam.pos[0]) ** 2 + (b.pos[2] - cam.pos[2]) ** 2;
      return da - db;
    });

    const sh = this.sh.use();
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
      .set('uBakeBFrames', this.bake.frames)
      .set('uFlameBoost', P.flameBoost)
      .set('uMsExt', P.msExt).set('uMsScatter', P.msScatter).set('uMsPhase', P.msPhase);
    sh.seti('uMsOctaves', P.msOctaves);


    gl.enable(gl.SCISSOR_TEST);
    const screenArea = w * h;
    let drawn = 0;
    for (const o of sorted) {
      const r = this._screenRect(o, cam, w, h);
      if (!r) continue;
      gl.scissor(r[0], r[1], r[2], r[3]);

      // LOD por área em tela. Num RTS a maioria das explosões é pequena no
      // quadro: gastar 44 passos numa que ocupa 2% da tela é desperdício
      // puro, e a diferença é invisível nesse tamanho.
      const frac = (r[2] * r[3]) / screenArea;
      const lod = Math.sqrt(Math.min(frac / P.refArea, 1));
      const steps = Math.max(P.stepsMin, Math.round(P.steps * lod));
      sh.seti('uSteps', steps);
      sh.set('uTemporalLerpB', frac > P.refArea * 0.25 ? 1 : 0);
      const L = this.lightOf(o);
      sh._unit = 0;
      sh.set('uInstPos', o.pos).set('uInstScale', o.scale)
        .set('uInstFrame', this.bake.frameOfTime(o.t))
        .set('uInstSeed', o.seed)
        .set('uVariantBaseB', (o.variant || 0) * this.bake.frames)
        .set('uFirePos', L.pos)
        // Dentro do PRÓPRIO volume a luz do fogo é a da simulação, sem o
        // lightScale — esse fator só existe pra barragem não estourar a cena.
        .set('uFireColor', L.color.map((c) => c / P.lightScale))
        .set('uLightSlot', o.lcSlot ?? -1);
      sh.tex('uBakeB', this.bake.tex, gl.TEXTURE_2D_ARRAY)
        .tex('uBakeMacroM', this.bake.macroTex, gl.TEXTURE_2D_ARRAY)
        .tex('uBakeFuelU', this.bake.fuelTex, gl.TEXTURE_2D_ARRAY)
        .tex('uLightC', this.lcTex, gl.TEXTURE_2D_ARRAY)
        .tex('uBB', this.bbTex).tex('uDepth', depthTex)
        .tex('uEnvLut', env.envLut).tex('uNoise', this.noiseTex, gl.TEXTURE_3D);
      drawFS(gl);
      drawn++;
    }
    gl.disable(gl.SCISSOR_TEST);
    return drawn;
  }
}
