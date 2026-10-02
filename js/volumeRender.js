// ---------------------------------------------------------------------------
// volumeRender.js — raymarching volumétrico do resultado da simulação.
//
// Modelo de transporte radiativo por passo, energia-conservante:
//   Hillaire, "Physically Based and Unified Volumetric Rendering in
//   Frostbite", SIGGRAPH 2015 —  S∫ = (S − S·e^(−σΔt)) / σ
//
// Espalhamento múltiplo aproximado por octaves:
//   Wrenninge, Kulla & Villemin, "Oz: The Great and Volumetric" /
//   "Art-Directable Multiple Volumetric Scattering", SIGGRAPH 2013 —
//   octave i usa extinção a^i, albedo b^i e anisotropia c^i.
//
// Emissão por lei de Planck (LUT em blackbody.js) com σ_a·B(T) — a lei de
// Kirchhoff garante emissividade = absortividade, então a fuligem que
// absorve é exatamente a que brilha.
//
// Renderizado em meia resolução com jitter interleaved-gradient e
// reconstruído por upsample de profundidade-mais-próxima.
// ---------------------------------------------------------------------------

import { Shader, Target, MRTarget, drawFS, FS_VS } from './gl.js';
import { COMMON, ATMOS, ENVLUT, VOLUME_SHADOW } from './glsl.js';

const MAX_STEPS = 160;

export const VOL_DEFAULTS = {
  sootExt:      13.5,   // coeficiente de extinção da fuligem (1/m por unidade)
  dustExt:      5.5,
  sootAlbedo:   0.30,   // fumaça de hidrocarboneto é bem absorvente
  dustAlbedo:   0.78,
  sootColor:    [0.55, 0.545, 0.535],
  dustColor:    [0.82, 0.755, 0.655],
  // A emissão vive nas MESMAS unidades da irradiância solar (1.0 = topo da
  // atmosfera), então ela não é reescalada com a hora do dia: de noite, com
  // a exposição ~4× maior, a bola de fogo estoura mais — que é exatamente o
  // que uma câmera exposta pro luar faz.
  // Reduzido pra tirar a superfície da bola de fogo do clipping do ACES: a
  // sensação de brilho volta pelo bloom, que é como filme resolve isso. Não
  // afeta a iluminação da cena — a luz usa ganho fixo próprio.
  emissionGain: 0.62,
  // Expoente sobre a curva de Stefan–Boltzmann. 0.58 dava 16× de contraste
  // entre T=1 e T=0.5: o topo estourava muito antes do fogo frio aparecer.
  // 0.44 comprime pra ~7× e deixa as duas pontas legíveis ao mesmo tempo.
  emissionCurve: 0.44,  // compressão da curva de Stefan–Boltzmann (art-direct)
  tempScale:    0.92,
  flameBoost:   1.5,    // brilho extra na frente de chama (onde há combustível)
  phaseG:       0.42,
  phaseBack:    0.22,
  phaseMix:     0.32,
  msOctaves:    3,
  msExt:        0.52,   // a
  msScatter:    0.52,   // b
  msPhase:      0.55,   // c
  steps:        104,
  detailAmp:    0.42,
  detailDens:   0.34,
  erode:        0.042,  // erosão da borda em filamentos   // amplitude da modulação de densidade sub-voxel   // deslocamento por ruído (detalhe sub-voxel)
  detailScale:  0.85,   // período do ruído ~4.7m → detalhe de ~1.2m
  detailAmp2:   0.0,    // 2ª oitava: ligada só no preset ultra
  skyGain:      1.1,
  fireGain:     1.0,
  heatAmount:   1.5,
  shadowDensity: 1.0,
  heatStrength: 1.0,
  volScale:     0.5,    // resolução do raymarch relativa à da cena
  debug: 0,   // 0 off · 1 fuligem · 2 temperatura · 3 combustível · 4 emissão · 5 chama
  lightScale: 0.030,    // potência radiante do volume → irradiância da luz pontual
                        // (mesmas unidades do sol: ~0.7 = sol a pino)
};

export class VolumeRenderer {
  constructor(gl, grid, lightGrid, macroGrid, bbTex) {
    this.gl = gl;
    this.grid = grid;
    this.bbTex = bbTex;
    this.params = { ...VOL_DEFAULTS };

    const P = grid.glsl();
    const PL = lightGrid.glsl('L');
    const PM = macroGrid.glsl('M');
    const HEAD = `#version 300 es
precision highp float;
precision highp sampler2D;
precision highp sampler3D;
in vec2 vUV;
`;

    // ===================== RAYMARCH =====================
    this.shMarch = new Shader(gl, FS_VS, HEAD + COMMON + ENVLUT + P + PL + PM + VOLUME_SHADOW + `
uniform sampler2D uFields, uLight, uBB, uDepth, uMacro;
uniform sampler3D uNoise;
uniform mat4 uInvViewProj;
uniform vec3 uCamPos, uKeyDir, uFirePos, uFireColor;
uniform vec2 uRes;
uniform float uNear, uFar, uJitter, uTime;
uniform float uSootExt, uDustExt, uSootAlbedo, uDustAlbedo;
uniform vec3 uSootColor, uDustColor;
uniform float uEmissionGain, uEmissionCurve, uTempScale, uFlameBoost;
uniform float uPhaseG, uPhaseBack, uPhaseMix;
uniform float uMsExt, uMsScatter, uMsPhase;
uniform int uMsOctaves, uSteps, uDebug;
uniform float uDetailAmp, uDetailScale, uDetailAmp2, uDetailDens, uErode, uSkyGain, uFireGain, uHeatAmount;

layout(location=0) out vec4 oCol;   // rgb = radiância acumulada, a = transmitância
layout(location=1) out vec4 oAux;   // r = profundidade média, g = calor

// --- amostragem do meio, com deslocamento por ruído pra detalhe sub-voxel
// (a grade tem ~20cm de célula; o deslocamento devolve a aparência de
//  centímetros sem custo de simulação)
vec4 sampleMedium(vec3 w){
  vec3 fp = w * uDetailScale + vec3(0.0, -uTime * 0.035, 0.0);
  vec3 n = texture(uNoise, fp).xyz * 2.0 - 1.0;
  vec3 wp = w + n * uDetailAmp;
  // a 2ª oitava é +1 fetch em CADA passo do raymarch — só vale no ultra
  if (uDetailAmp2 > 0.0)
    wp += (texture(uNoise, fp * 3.1 + 0.41).xyz * 2.0 - 1.0) * uDetailAmp2;
  vec4 f = sampleVol(uFields, worldToVoxel(wp));
  float nm = (n.x + n.y + n.z) * 0.577;
  // modulação de densidade sub-voxel: a célula tem ~30cm, e só deslocar a
  // amostra deforma o contorno sem criar variação dentro dele. Isto reusa o
  // mesmo fetch de ruído, então custa zero fetch a mais.
  f.rb *= 1.0 + uDetailDens * nm;
  // EROSÃO de borda, modulada por ruído: é o que separa fumaça de um recorte
  // com silhueta dura. A subtração é atenuada conforme a densidade sobe —
  // subtrair um valor ABSOLUTO apagava a pluma tardia inteira, que é de
  // densidade baixa, em vez de só desfiar a borda.
  float er = uErode * (0.5 - 0.5 * nm);
  f.r = max(f.r - er / (1.0 + f.r * 9.0), 0.0);
  f.a = max(f.a - er * 0.7 / (1.0 + f.a * 9.0), 0.0);
  return f;
}

vec3 emissionOf(vec4 f){
  float t = saturate(f.g * uTempScale);
  vec4 bb = texture(uBB, vec2(t, 0.5));
  float I = pow(max(bb.a, 0.0), uEmissionCurve);
  // σ_a · B(T): a fuligem incandescente é o emissor. A frente de chama
  // (onde ainda há combustível queimando) recebe um reforço.
  float sigA = uSootExt * f.r * (1.0 - uSootAlbedo);
  float flame = 1.0 + uFlameBoost * smoothstep(0.02, 0.35, f.b) * step(0.12, f.g);
  return bb.rgb * (I * uEmissionGain * flame) * sigA;
}

void main(){
  vec2 uv = vUV;
  vec2 ndc = uv * 2.0 - 1.0;
  vec4 h0 = uInvViewProj * vec4(ndc, -1.0, 1.0);
  vec4 h1 = uInvViewProj * vec4(ndc, 1.0, 1.0);
  vec3 p0 = h0.xyz / h0.w, p1 = h1.xyz / h1.w;
  vec3 ro = uCamPos;
  vec3 rd = normalize(p1 - p0);

  // limite distal: profundidade da cena (o volume é ocluído pela geometria)
  float dz = texture(uDepth, uv).r;
  float tScene = 1e9;
  if (dz < 0.999999){
    vec4 hw = uInvViewProj * vec4(ndc, dz * 2.0 - 1.0, 1.0);
    tScene = length(hw.xyz / hw.w - ro);
  }

  vec2 hit = rayBox(ro, rd, DOMAIN_MIN, DOMAIN_MAX);
  hit.x = max(hit.x, uNear);
  hit.y = min(hit.y, tScene);
  if (hit.y <= hit.x){
    oCol = vec4(0.0, 0.0, 0.0, 1.0);   // neutro no blend acumulativo
    oAux = vec4(0.0, 0.0, 0.0, 1.0);
    return;
  }

  float span = hit.y - hit.x;
  float dt = span / float(uSteps);
  // jitter interleaved-gradient: quebra o banding em ruído de alta frequência
  float jit = ignoise(gl_FragCoord.xy + uJitter * 71.13);
  float t = hit.x + dt * jit;

  // a luz-chave do ambiente ilumina a fumaça: de dia o sol, de noite a lua
  vec3 keyCol = envKeyColor();
  vec3 skyCol = envSkyUp();
  float cosSun = dot(rd, uKeyDir);
  float phSun = phaseDual(cosSun, uPhaseG, uPhaseBack, uPhaseMix);
  float phIso = 1.0 / (4.0 * PI);

  // transmitância por octave de espalhamento múltiplo
  float Tr[4];
  for (int i = 0; i < 4; i++) Tr[i] = 1.0;
  vec3 L = vec3(0.0);
  float depthSum = 0.0, wSum = 0.0, heat = 0.0, dbg = 0.0;

  for (int i = 0; i < ${MAX_STEPS}; i++){
    if (t >= hit.y) break;
    // o corte por opacidade esconde o interior — nos modos de diagnóstico a
    // ideia é justamente ver o que há DENTRO da bola de fogo
    if (uDebug == 0 && Tr[0] < 0.004) break;

    vec3 w = ro + rd * t;

    // ---- empty-space skipping: salta até a fronteira do bloco macro -----
    // Sem isso a maior parte dos passos amostra vazio. O DDA garante que o
    // salto nunca atravessa um bloco ocupado.
    if (macroAt(uMacro, w) < 2e-3){
      t += macroExit(w, rd) + 1e-3;
      continue;
    }

    vec4 f = sampleMedium(w);
    float dens = f.r + f.a;

    if (dens > 1e-4){
      vec3 vpL = worldToVoxelL(w);
      vec4 lv = sampleVolL(uLight, vpL);

      float sigSoot = uSootExt * f.r;
      float sigDust = uDustExt * f.a;
      float sigT = sigSoot + sigDust;
      // cor/albedo de espalhamento = mistura dos dois meios
      float wS = sigSoot / max(sigT, 1e-5);
      vec3  scatCol = mix(uDustColor, uSootColor, wS);
      float albedo  = mix(uDustAlbedo, uSootAlbedo, wS);

      vec3 emit = emissionOf(f);
      heat += f.g * Tr[0] * dt;

      // irradiância da bola de fogo dentro da própria fumaça
      float fd = length(uFirePos - w);
      vec3 fireIrr = uFireColor * uFireGain * lv.z / (1.0 + fd * fd * 0.05);

      // ---- octaves de espalhamento múltiplo ----------------------------
      float ae = 1.0, ab = 1.0, ap = 1.0;
      for (int o = 0; o < 4; o++){
        if (o >= uMsOctaves) break;
        float sig = sigT * ae;
        float sc = sig * albedo * ab;
        // octaves altas: phase mais isotrópica e sombra mais "vazada"
        float ph = mix(phIso, phSun, ap);
        float tSun = mix(1.0, lv.x, ap);
        float tSky = mix(1.0, lv.y, ap * 0.7 + 0.3);

        vec3 src = scatCol * sc * (keyCol * tSun * ph
                                 + skyCol * uSkyGain * tSky * phIso
                                 + fireIrr * mix(phIso, phSun, ap * 0.5))
                 + emit * ab;

        float ext = max(sig, 1e-5);
        float trStep = exp(-ext * dt);
        // integral analítica da fonte no passo (energia conservada)
        L += Tr[o] * (src - src * trStep) / ext;
        Tr[o] *= trStep;
        ae *= uMsExt; ab *= uMsScatter; ap *= uMsPhase;
      }

      // ---- modos de diagnóstico: olhar os CAMPOS, não a imagem final ----
      if (uDebug > 0){
        if (uDebug == 1) dbg = max(dbg, f.r);                 // fuligem (pico)
        else if (uDebug == 2) dbg = max(dbg, f.g);            // temperatura
        else if (uDebug == 3) dbg = max(dbg, f.b);            // combustível
        else if (uDebug == 4) dbg = max(dbg, luma(emit));     // emissão
        else if (uDebug == 5) {                               // oxidante/chama
          float oxy = saturate(1.0 - 1.15 * (f.b + f.r * 0.45)) + 0.07;
          dbg = max(dbg, oxy * smoothstep(0.13, 0.22, f.g) * f.b * 6.0);
        }
      }

      float contrib = (1.0 - Tr[0]);
      depthSum += t * contrib; wSum += contrib;
    }
    t += dt;
  }

  if (uDebug > 0){
    // colormap tipo "inferno": escuro → roxo → laranja → branco
    float v = saturate(uDebug == 4 ? log(1.0 + dbg) * 0.22 : dbg);
    vec3 cm = clamp(vec3(1.9 * v - 0.25, 2.2 * v * v - 0.5, 3.0 * v * v * v - 0.1), 0.0, 1.0);
    oCol = vec4(cm * 1.6, 0.0);           // Tr=0 → substitui a cena
    oAux = vec4(hit.x, 0.0, 0.0, 0.0);
    return;
  }
  // Formato pensado pro blend front-to-back entre VÁRIAS explosões:
  //   RGB: src·dst.a + dst   → radiância atenuada pelo que está na frente
  //   A:   dst.a·src.a       → produto das transmitâncias
  // No aux o alpha fica 1, então o mesmo blend vira soma pura: profundidade
  // ponderada por cobertura e calor somam, e o composite divide no fim.
  float cov = 1.0 - Tr[0];
  oCol = vec4(L, Tr[0]);
  oAux = vec4((wSum > 1e-4 ? depthSum / wSum : hit.x) * cov,
              heat * uHeatAmount, cov, 1.0);
}`, 'volumeMarch');

    // ===================== COMPOSITE / UPSAMPLE =====================
    // Upsample de profundidade-mais-próxima: escolhe entre os 4 texels de
    // meia resolução o que tem profundidade mais parecida com a do pixel
    // cheio. É o que impede halo de fumaça em cima das silhuetas.
    this.shComposite = new Shader(gl, FS_VS, HEAD + COMMON + ENVLUT + ATMOS + `
uniform sampler2D uScene, uVol, uVolAux, uDepth, uHalfDepth, uAO, uSceneNrm;
uniform mat4 uInvViewProj;
uniform vec3 uCamPos;
uniform vec2 uRes, uHalfRes;
uniform float uHeatStrength, uTime;
uniform float uFogDensity, uFogFalloff, uAOFloor, uAODebug;
out vec4 oCol;

float sceneDist(vec2 uv){
  float dz = texture(uDepth, uv).r;
  if (dz >= 0.999999) return 1e9;
  vec2 ndc = uv * 2.0 - 1.0;
  vec4 hw = uInvViewProj * vec4(ndc, dz * 2.0 - 1.0, 1.0);
  return length(hw.xyz / hw.w - uCamPos);
}

void main(){
  vec2 uv = vUV;
  float dFull = sceneDist(uv);

  vec2 hTexel = 1.0 / uHalfRes;
  vec2 base = (floor(uv * uHalfRes - 0.5) + 0.5) * hTexel;
  vec4 vol = vec4(0.0); vec4 aux = vec4(0.0);
  float best = 1e20;
  // CLAMPAR OS DOIS LADOS. Antes só dFull era limitado a 1e8, então céu
  // contra céu dava |1e9 - 1e8| = 9e8: o teste concluía "profundidades
  // discordam" exatamente na silhueta contra o céu e caía em point-sampling
  // bloquiado — o contorno escuro que fazia a explosão parecer um adesivo.
  float dRef = min(dFull, 1e8);
  for (int i = 0; i < 4; i++){
    vec2 o = vec2(float(i & 1), float((i >> 1) & 1)) * hTexel;
    float hd = min(texture(uHalfDepth, base + o).r, 1e8);
    float diff = abs(hd - dRef);
    if (diff < best){
      best = diff;
      vol = texture(uVol, base + o);
      aux = texture(uVolAux, base + o);
    }
  }
  // mistura bilinear quando as profundidades concordam (evita blocagem)
  {
    vec4 vb = texture(uVol, uv), ab = texture(uVolAux, uv);
    float agree = 1.0 - saturate(best / max(dRef * 0.06, 0.25));
    vol = mix(vol, vb, agree);
    aux = mix(aux, ab, agree);
  }

  // distorção por calor: refração pelo ar quente. O gradiente vem de um
  // ruído animado, com amplitude = calor acumulado no raio.
  float heat = aux.g * uHeatStrength;
  vec2 refr = vec2(0.0);
  if (heat > 1e-4){
    vec2 q = gl_FragCoord.xy * 0.018;
    float n1 = snoise(vec3(q, uTime * 1.35));
    float n2 = snoise(vec3(q * 2.3 + 17.0, uTime * 1.9));
    refr = vec2(n1, n2) * heat * 0.010;
    refr /= uRes / min(uRes.x, uRes.y);
  }
  vec2 suv = clamp(uv + refr, vec2(0.001), vec2(0.999));
  vec3 scene = texture(uScene, suv).rgb;

  // ---- oclusão de ambiente, aplicada SÓ na parcela indireta ------------
  if (uAODebug > 0.5){ oCol = vec4(vec3(texture(uAO, suv).r), 1.0); return; }
  {
    float ao = texture(uAO, suv).r;
    float ambFrac = texture(uSceneNrm, suv).w;
    // Peso mínimo mesmo onde a luz direta domina: a oclusão de contato é o
    // sinal que o olho usa pra decidir que um objeto está APOIADO no chão.
    // Puramente indireto é fisicamente mais correto, mas em sol pleno a
    // fração indireta é pequena e os objetos continuam parecendo flutuar.
    float w = max(ambFrac, uAOFloor);
    scene *= 1.0 - w * (1.0 - ao);
  }

  // ---- perspectiva aérea sobre o VOLUME -------------------------------
  // A cena já recebe névoa no seu próprio passe, mas a radiância do volume
  // era somada crua: o fogo chegava à câmera sem atravessar ar nenhum,
  // enquanto tudo atrás dele atravessava 60m. É isso que faz a explosão
  // parecer colada na frente da imagem em vez de estar DENTRO dela.
  vec3 volCol = vol.rgb;
  {
    vec2 ndc = uv * 2.0 - 1.0;
    vec4 h0 = uInvViewProj * vec4(ndc, -1.0, 1.0);
    vec4 h1 = uInvViewProj * vec4(ndc,  1.0, 1.0);
    vec3 rd = normalize(h1.xyz / h1.w - h0.xyz / h0.w);
    float cov = max(aux.b, 1e-4);
    float dv = max(aux.r / cov, 0.0);               // profundidade média ponderada
    vec3 wv = uCamPos + rd * dv;
    // mesma integral analítica de névoa de altura usada na cena
    float hc = exp(-max(uCamPos.y, 0.0) * uFogFalloff);
    float hp = exp(-max(wv.y, 0.0) * uFogFalloff);
    float dy = wv.y - uCamPos.y;
    float integ = abs(dy) > 1e-3 ? (hc - hp) / (dy * uFogFalloff) : hc;
    float fog = saturate(1.0 - exp(-uFogDensity * dv * max(integ, 0.0)));
    vec3 fogCol = skyBase(rd);
    // atenua o que vem do volume e soma a luz espalhada no caminho,
    // ponderada pela cobertura do volume (1 - transmitância)
    volCol = volCol * (1.0 - fog) + fogCol * fog * (1.0 - vol.a);
  }
  oCol = vec4(scene * vol.a + volCol, 1.0);
}`, 'volumeComposite');

    // downsample da profundidade da cena pra meia resolução (mín. do bloco:
    // preserva as silhuetas de frente)
    this.shHalfDepth = new Shader(gl, FS_VS, HEAD + `
uniform sampler2D uDepth;
uniform mat4 uInvViewProj;
uniform vec3 uCamPos;
uniform vec2 uFullRes;
out vec4 oCol;
float dist(vec2 uv){
  float dz = texture(uDepth, uv).r;
  if (dz >= 0.999999) return 1e9;
  vec2 ndc = uv * 2.0 - 1.0;
  vec4 hw = uInvViewProj * vec4(ndc, dz * 2.0 - 1.0, 1.0);
  return length(hw.xyz / hw.w - uCamPos);
}
void main(){
  vec2 t = 1.0 / uFullRes;
  float a = dist(vUV + vec2(-0.5, -0.5) * t), b = dist(vUV + vec2(0.5, -0.5) * t);
  float c = dist(vUV + vec2(-0.5,  0.5) * t), d = dist(vUV + vec2(0.5,  0.5) * t);
  oCol = vec4(min(min(a, b), min(c, d)), 0.0, 0.0, 0.0);
}`, 'halfDepth');

    // ===================== REDUÇÃO: A BOLA DE FOGO COMO LUZ =====================
    // Momentos ponderados pela emissão → centróide + potência + cor.
    // Reduzido em cadeia 4x4 até 1x1 e lido de forma assíncrona (1 pixel,
    // um frame atrasado — sem stall de pipeline).
    this.shMoments = new Shader(gl, FS_VS, HEAD + COMMON + P + `
uniform sampler2D uFields, uBB;
uniform float uEmissionGain, uEmissionCurve, uTempScale, uSootExt, uSootAlbedo, uFlameBoost;
layout(location=0) out vec4 oA;   // (Σe·pos, Σe)
layout(location=1) out vec4 oB;   // (Σ emissão rgb, Σ volume quente)
void main(){
  vec3 sumP = vec3(0.0); float sumE = 0.0; vec3 sumC = vec3(0.0); float sumV = 0.0;
  ivec2 base = ivec2(gl_FragCoord.xy) * 4;
  for (int y = 0; y < 4; y++){
    for (int x = 0; x < 4; x++){
      ivec2 px = base + ivec2(x, y);
      if (px.x >= int(ATLAS.x) || px.y >= int(ATLAS.y)) continue;
      vec4 f = texelFetch(uFields, px, 0);
      float t = saturate(f.g * uTempScale);
      vec4 bb = texture(uBB, vec2(t, 0.5));
      float I = pow(max(bb.a, 0.0), uEmissionCurve);
      float sigA = uSootExt * f.r * (1.0 - uSootAlbedo);
      float flame = 1.0 + uFlameBoost * smoothstep(0.02, 0.35, f.b) * step(0.12, f.g);
      vec3 e = bb.rgb * (I * uEmissionGain * flame) * sigA;
      float el = luma(e);
      vec3 w = voxelToWorld(fragToVoxel(vec2(px) + 0.5));
      sumP += w * el; sumE += el; sumC += e; sumV += el > 1e-4 ? 1.0 : 0.0;
    }
  }
  oA = vec4(sumP, sumE);
  oB = vec4(sumC, sumV);
}`, 'moments');

    this.shReduce = new Shader(gl, FS_VS, HEAD + `
uniform sampler2D uA, uB;
uniform vec2 uSrcSize;
layout(location=0) out vec4 oA;
layout(location=1) out vec4 oB;
void main(){
  vec4 a = vec4(0.0), b = vec4(0.0);
  ivec2 base = ivec2(gl_FragCoord.xy) * 4;
  ivec2 lim = ivec2(uSrcSize);
  for (int y = 0; y < 4; y++){
    for (int x = 0; x < 4; x++){
      ivec2 px = base + ivec2(x, y);
      if (px.x >= lim.x || px.y >= lim.y) continue;
      a += texelFetch(uA, px, 0);
      b += texelFetch(uB, px, 0);
    }
  }
  oA = a; oB = b;
}`, 'reduce');

    this._buildReduceChain();
    this.fireLight = { pos: new Float32Array([0, 2.5, 0]), color: new Float32Array([0, 0, 0]), power: 0, cells: 0 };
    this._readBuf = new Float32Array(8);

    // Leitura assíncrona via PIXEL_PACK_BUFFER + fenceSync. readPixels direto
    // custava 21ms num único pixel: força flush completo do pipeline e a CPU
    // fica esperando a GPU terminar TUDO. Com PBO o comando de cópia entra na
    // fila, e o resultado é colhido 2-3 frames depois, sem bolha.
    this._pbos = [];
    for (let i = 0; i < 3; i++) {
      const buf = gl.createBuffer();
      gl.bindBuffer(gl.PIXEL_PACK_BUFFER, buf);
      gl.bufferData(gl.PIXEL_PACK_BUFFER, 8 * 4, gl.STREAM_READ);
      gl.bindBuffer(gl.PIXEL_PACK_BUFFER, null);
      this._pbos.push({ buf, fence: null, busy: false });
    }
    this._pboNext = 0;
  }

  _buildReduceChain() {
    const gl = this.gl;
    const f32 = { internalFormat: gl.RGBA32F, format: gl.RGBA, type: gl.FLOAT, filter: gl.NEAREST };
    this.reduceChain = [];
    let w = Math.ceil(this.grid.atlasW / 4), h = Math.ceil(this.grid.atlasH / 4);
    while (true) {
      this.reduceChain.push({ t: new MRTarget(gl, w, h, [f32, f32]), w, h });
      if (w === 1 && h === 1) break;
      w = Math.ceil(w / 4); h = Math.ceil(h / 4);
    }
    this.readFbo = gl.createFramebuffer();
  }

  resize(w, h) {
    const gl = this.gl;
    if (this.volTarget) this.volTarget.dispose();
    if (this.halfDepth) this.halfDepth.dispose();
    // "half" é histórico: a escala virou parâmetro de qualidade. Meia
    // resolução é o maior responsável pela maciez do volume.
    const vs = this.params.volScale || 0.5;
    this.halfW = Math.max(1, Math.round(w * vs));
    this.halfH = Math.max(1, Math.round(h * vs));
    const rgba = { internalFormat: gl.RGBA16F, format: gl.RGBA, type: gl.HALF_FLOAT };
    const r32 = { internalFormat: gl.R32F, format: gl.RED, type: gl.FLOAT, filter: gl.NEAREST };
    this.volTarget = new MRTarget(gl, this.halfW, this.halfH, [rgba, rgba]);
    this.halfDepth = new Target(gl, this.halfW, this.halfH, r32);
  }

  /** colhe leituras prontas (sem stall) e dispara a redução deste frame */
  /**
   * Redução da emissão de UMA explosão → centróide, potência e cor da luz.
   * Com várias explosões ativas, é chamada em rodízio (uma por frame): a
   * leitura chega 2-3 frames depois de qualquer jeito, e a luz é suavizada.
   */
  updateFireLight(slot) {
    const gl = this.gl, P = this.params;

    // 1) colhe os PBOs cuja fence já sinalizou. Cada registro carrega o SLOT
    //    de origem, porque a leitura chega frames depois e nesse meio tempo
    //    a redução já rodou pra outras explosões.
    if (!this.noReadback) {
      for (const pbo of this._pbos) {
        if (!pbo.busy || !pbo.fence) continue;
        if (gl.getSyncParameter(pbo.fence, gl.SYNC_STATUS) !== gl.SIGNALED) continue;
        gl.deleteSync(pbo.fence);
        pbo.fence = null;
        pbo.busy = false;
        gl.bindBuffer(gl.PIXEL_PACK_BUFFER, pbo.buf);
        gl.getBufferSubData(gl.PIXEL_PACK_BUFFER, 0, this._readBuf);
        gl.bindBuffer(gl.PIXEL_PACK_BUFFER, null);
        this._applyFireLight(pbo.owner);
      }
    }

    // 2) redução desta explosão
    gl.disable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);
    const c0 = this.reduceChain[0];
    // Ganho FIXO de 1.0: a redução mede potência radiante pra derivar a LUZ.
    // Amarrá-la a emissionGain acoplaria iluminação (física) a faixa dinâmica
    // do tonemap (apresentação).
    this.shMoments.use()
      .set('uEmissionGain', 1.0).set('uEmissionCurve', P.emissionCurve)
      .set('uTempScale', P.tempScale).set('uSootExt', P.sootExt)
      .set('uSootAlbedo', P.sootAlbedo).set('uFlameBoost', P.flameBoost)
      .set('uDomainOrigin', slot.fluid.domainOrigin)
      .tex('uFields', slot.fluid.fields.read.tex).tex('uBB', this.bbTex);
    c0.t.bind(); drawFS(gl);
    for (let i = 1; i < this.reduceChain.length; i++) {
      const src = this.reduceChain[i - 1], dst = this.reduceChain[i];
      this.shReduce.use().set('uSrcSize', [src.w, src.h])
        .tex('uA', src.t.texs[0]).tex('uB', src.t.texs[1]);
      dst.t.bind(); drawFS(gl);
    }

    // 3) enfileira a cópia 1x1 → PBO (assíncrona) e planta a fence
    const pbo = this._pbos[this._pboNext];
    if (!pbo.busy && !this.noReadback) {
      const last = this.reduceChain[this.reduceChain.length - 1].t;
      gl.bindFramebuffer(gl.FRAMEBUFFER, last.fbo);
      gl.bindBuffer(gl.PIXEL_PACK_BUFFER, pbo.buf);
      gl.readBuffer(gl.COLOR_ATTACHMENT0);
      gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.FLOAT, 0);
      gl.readBuffer(gl.COLOR_ATTACHMENT1);
      gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.FLOAT, 16);
      gl.readBuffer(gl.COLOR_ATTACHMENT0);
      gl.bindBuffer(gl.PIXEL_PACK_BUFFER, null);
      pbo.fence = gl.fenceSync(gl.SYNC_GPU_COMMANDS_COMPLETE, 0);
      pbo.busy = true;
      pbo.owner = slot;
      this._pboNext = (this._pboNext + 1) % this._pbos.length;
    }

    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    return slot.fire;
  }



  /**
   * Leitura BLOQUEANTE da luz do fogo. As fences de PBO nunca sinalizam
   * dentro de uma rajada síncrona de JS (a GPU não progride até o task
   * ceder), então a captura determinística precisa deste caminho.
   * Nunca usar no loop normal: custa ~20ms de stall.
   */
  syncFireLight(slot) {
    const gl = this.gl;
    if (!slot) return;
    const last = this.reduceChain[this.reduceChain.length - 1].t;
    gl.bindFramebuffer(gl.FRAMEBUFFER, last.fbo);
    gl.readBuffer(gl.COLOR_ATTACHMENT0);
    gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.FLOAT, this._readBuf, 0);
    gl.readBuffer(gl.COLOR_ATTACHMENT1);
    gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.FLOAT, this._readBuf, 4);
    gl.readBuffer(gl.COLOR_ATTACHMENT0);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    this._applyFireLight(slot);
  }

  _applyFireLight(slot) {
    if (!slot) return;
    if (!slot.fire) {
      slot.fire = { pos: new Float32Array([0, 2.5, 0]), color: new Float32Array([0, 0, 0]), power: 0 };
    }
    const e = this._readBuf[3];
    const F = slot.fire;
    if (e > 1e-3) {
      // centróide da emissão, suavizado (a luz não pode tremer entre frames)
      const k = 0.4;
      F.pos[0] += (this._readBuf[0] / e - F.pos[0]) * k;
      F.pos[1] += (this._readBuf[1] / e - F.pos[1]) * k;
      F.pos[2] += (this._readBuf[2] / e - F.pos[2]) * k;
      F.power = e;
      F.cells = this._readBuf[7];
      const s = this.grid.cell ** 3 * this.params.lightScale;
      F.color[0] = this._readBuf[4] * s;
      F.color[1] = this._readBuf[5] * s;
      F.color[2] = this._readBuf[6] * s;
    } else {
      F.power = 0;
      F.color[0] = F.color[1] = F.color[2] = 0;
    }
  }

  /**
   * Marcha TODAS as explosões ativas num só alvo, compondo front-to-back.
   * A ordem importa: o blend assume que o que já está no alvo está à frente.
   */
  march(cam, env, blasts, noise, depthTex, fullW, fullH) {
    const gl = this.gl, P = this.params;
    gl.disable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);

    // profundidade da cena em meia resolução (uma vez, serve pra todos)
    this.shHalfDepth.use()
      .set('uInvViewProj', cam.invViewProj).set('uCamPos', cam.pos)
      .set('uFullRes', [fullW, fullH]).tex('uDepth', depthTex);
    this.halfDepth.bind(); drawFS(gl);

    this.volTarget.bind();
    gl.clearColor(0, 0, 0, 1);            // radiância 0, transmitância 1
    gl.clear(gl.COLOR_BUFFER_BIT);
    if (!blasts.length) { gl.bindFramebuffer(gl.FRAMEBUFFER, null); return; }

    gl.enable(gl.BLEND);
    gl.blendFuncSeparate(gl.DST_ALPHA, gl.ONE, gl.ZERO, gl.SRC_ALPHA);

    const sh = this.shMarch.use();
    sh.set('uInvViewProj', cam.invViewProj).set('uCamPos', cam.pos)
      .set('uKeyDir', env.keyDir)
      .set('uRes', [this.halfW, this.halfH])
      .set('uNear', cam.near).set('uFar', cam.far)
      .set('uJitter', env.frameJitter).set('uTime', env.time)
      .set('uSootExt', P.sootExt).set('uDustExt', P.dustExt)
      .set('uSootAlbedo', P.sootAlbedo).set('uDustAlbedo', P.dustAlbedo)
      .set('uSootColor', P.sootColor).set('uDustColor', P.dustColor)
      .set('uEmissionGain', P.emissionGain).set('uEmissionCurve', P.emissionCurve)
      .set('uTempScale', P.tempScale).set('uFlameBoost', P.flameBoost)
      .set('uPhaseG', P.phaseG).set('uPhaseBack', P.phaseBack).set('uPhaseMix', P.phaseMix)
      .set('uMsExt', P.msExt).set('uMsScatter', P.msScatter).set('uMsPhase', P.msPhase)
      .set('uDetailAmp', P.detailAmp).set('uDetailScale', P.detailScale)
      .set('uDetailAmp2', P.detailAmp2).set('uDetailDens', P.detailDens)
      .set('uErode', P.erode)
      .set('uSkyGain', P.skyGain).set('uFireGain', P.fireGain).set('uHeatAmount', P.heatAmount)
      .seti('uMsOctaves', P.msOctaves).seti('uSteps', Math.min(P.steps, MAX_STEPS))
      .seti('uDebug', P.debug | 0);

    for (const b of blasts) {
      const f = b.fire || { pos: b.fluid.blastPos, color: [0, 0, 0] };
      sh._unit = 0;
      sh.set('uDomainOrigin', b.fluid.domainOrigin)
        .set('uFirePos', f.pos).set('uFireColor', f.color);
      sh.tex('uFields', b.fluid.fields.read.tex)
        .tex('uLight', b.fluid.light.tex)
        .tex('uBB', this.bbTex)
        .tex('uDepth', depthTex)
        .tex('uMacro', b.fluid.macro.tex)
        .tex('uEnvLut', env.envLut)
        .tex('uNoise', noise, gl.TEXTURE_3D);
      drawFS(gl);
    }

    gl.disable(gl.BLEND);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }

  composite(dst, cam, sceneTex, depthTex, fullW, fullH, time, env) {
    const gl = this.gl;
    this.shComposite.use()
      .set('uInvViewProj', cam.invViewProj).set('uCamPos', cam.pos)
      .set('uRes', [fullW, fullH]).set('uHalfRes', [this.halfW, this.halfH])
      .set('uHeatStrength', this.params.heatStrength ?? 1.0).set('uTime', time)
      .set('uFogDensity', env.fogDensity).set('uFogFalloff', env.fogFalloff)
      .set('uAOFloor', env.aoFloor ?? 0.35).set('uAODebug', env.aoDebug ? 1 : 0)
      .set('uSunDir', env.sunDir).set('uMoonDir', env.moonDir)
      .set('uSkyTime', time).set('uStarBright', 0.0)
      .set('uNightGlow', 0.0).set('uMoonBright', 0.0)
      .tex('uScene', sceneTex).tex('uVol', this.volTarget.texs[0])
      .tex('uVolAux', this.volTarget.texs[1]).tex('uDepth', depthTex)
      .tex('uHalfDepth', this.halfDepth.tex)
      .tex('uSkyView', env.skyView).tex('uEnvLut', env.envLut)
      .tex('uAO', env.ao).tex('uSceneNrm', env.sceneNrm);
    dst.bind(); drawFS(gl);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }
}
