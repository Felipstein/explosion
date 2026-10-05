// ---------------------------------------------------------------------------
// fluid.js — solver Euleriano 3D de fumaça/fogo na GPU.
//
// Referências:
//   Stam,   "Stable Fluids", SIGGRAPH 1999
//   Fedkiw, Stam & Jensen, "Visual Simulation of Smoke", SIGGRAPH 2001
//     (vorticity confinement)
//   Nguyen, Fedkiw & Jensen, "Physically Based Modeling and Animation of
//     Fire", SIGGRAPH 2002 (combustão / coordenada de reação)
//   Selle, Fedkiw, Kim, Liu & Rossignac, "An Unconditionally Stable
//     MacCormack Method", J. Sci. Comput. 2008 (advecção de 2ª ordem)
//   Bridson, Hourihan & Nordenstam, "Curl-Noise for Procedural Fluid Flow",
//     SIGGRAPH 2007
//   Taylor–Sedov: raio da onda de choque R(t) ∝ t^(2/5)
// ---------------------------------------------------------------------------

import { Shader, Target, PingPong, drawFS, FS_VS, createTexture3D, renderToTexture3D } from './gl.js';
import { COMMON, VOLUME_SHADOW } from './glsl.js';
import { VolumeGrid } from './volume.js';

const NOISE_RES = 64;

export const DEFAULTS = {
  // combustão
  ignitionT:   0.13,   // temperatura normalizada de ignição
  burnRate:    4.6,    // taxa de queima NA FRENTE DE CHAMA (limitada por mistura)
  heatYield:   3.1,    // temperatura gerada por unidade de combustível
  sootYield:   2.1,    // fuligem gerada por unidade de combustível queimado
  mixK:        1.15,   // quanto os produtos deslocam o oxidante
  oxyFloor:    0.07,   // vazamento mínimo de oxidante (o núcleo acaba queimando)
  sootRich:    0.70,   // fração da fuligem que só nasce no lado rico
  // Oxidação da fuligem. 1.25 consumia quase toda ela: a bola de fogo ficava
  // sem fuligem (que é o que EMITE) e sobrava pouca fumaça depois. Numa bola
  // de fogo rica a maior parte da fuligem sobrevive e vira a nuvem preta.
  sootOxid:    0.30,
  cooling:     0.95,   // resfriamento radiativo (∝ T⁴)
  coolingLin:  0.075,
  sootTrap:    3.2,    // auto-absorção: fuligem densa segura o calor
  sootDecay:   0.10,
  minDensity:  0.006,  // abaixo disto o campo é zerado (sem cauda infinita)
  dustDecay:   0.10,
  tempDiffuse: 0.0,

  // dinâmica
  buoyancy:    38.0,   // m/s² por unidade de temperatura
  sootWeight:  3.4,    // a massa de fuligem é ínfima: ela não pode anular o empuxo
  dustWeight:  8.0,    // poeira mineral é pesada de verdade
  vorticity:   7.4,    // ε do vorticity confinement
  turbulence:  8.5,    // amplitude do curl-noise
  turbScale:   0.115,  // frequência espacial do curl-noise (1/m)
  turbDecay:   1.7,    // decaimento temporal da injeção de turbulência
  drag:        0.30,
  groundFric:  2.2,
  expansion:   2.6,    // fonte de divergência da combustão (o fogo infla)
  blastSrc:    96.0,   // fonte de pressão da detonação (1/s)
  blastTau:    0.105,  // constante de tempo do decaimento da fonte
  blastRadius: 2.6,
  turbIface:   0.85,   // fração da turbulência concentrada na interface
  maxSpeed:    90.0,
  velDissip:   0.999,
  velMacCormack: false,  // limitador de 8 taps só vale a pena no ultra

  // detonação
  coreRadius:  3.20,   // maior = mais células resolvendo a bola de fogo
  coreWarp:    0.46,
  coreLumpScale: 1.15, // frequência do mosqueado volumétrico interno
  fuelAmount:  1.0,
  coreSoot:    0.10,
  coreTemp:    1.02,
  blastSpeed:  8.0,    // só uma semente; a expansão vem da pressão
  injectWindow: 0.055,
  ringStrength: 2.25,
  ringRadius:  1.35,
  ringHeight:  0.55,
  ringCore:    1.5,

  // surto de base (base surge) — poeira rasteira, escala de Taylor–Sedov
  surgeCoef:   5.0,
  surgeWidth:  2.2,
  surgeHeight: 1.1,
  surgeAmount: 0.22,   // era 0.70: virava um toro liso que dominava a cena
  surgeDur:    0.80,
  surgeSpeed:  5.0,
  surgeRagged: 0.42,   // variação do raio com o azimute (quebra o círculo)

  // solver
  pressureCoarse: 16,   // iterações no nível grosso (8× mais baratas)
  pressureFine: 4,      // suavização no nível fino
  lightSteps:  22,
};

export class FluidSim {
  /**
   * @param {object} shared buffers TRANSITÓRIOS compartilhados entre
   *   simulações do pool. curl, divergência e os níveis intermediários do
   *   macro só vivem DENTRO de um passo — mantê-los por slot desperdiçava
   *   ~22MB por explosão sem nenhum ganho.
   */
  constructor(gl, res, domainSize, shared = null) {
    this.gl = gl;
    this.grid = new VolumeGrid(res, domainSize);
    this.lightGrid = new VolumeGrid(Math.max(32, res >> 1), domainSize);
    // grade macro: extinção MÁXIMA por bloco de 8³ voxels. É o que permite
    // ao raymarch pular espaço vazio em passos grandes em vez de amostrar
    // 100 vezes o nada — a explosão ocupa uma fração do domínio.
    this.MACRO = 8;
    // Cadeia de redução: cada nível é um max 2×2×2 do anterior (8 taps por
    // fragmento, boa ocupância). Fazer 8³ de uma vez num único fragmento era
    // 1728 fetches dependentes com 4k threads — 70ms de latência exposta.
    this.macroLevels = [];
    for (let r = res >> 1; r >= Math.max(4, res / this.MACRO | 0); r >>= 1) {
      this.macroLevels.push(new VolumeGrid(r, domainSize));
    }
    this.macroGrid = this.macroLevels[this.macroLevels.length - 1];
    // grade grossa pro solver de pressão (V-cycle de 2 níveis)
    this.coarseGrid = new VolumeGrid(Math.max(16, res >> 1), domainSize);
    this.params = { ...DEFAULTS };
    this.time = 0;
    this.seed = 0;
    this.frame = 0;
    this.domainOrigin = new Float32Array([0, 0, 0]);
    // um slot que nunca detonou ainda é lido pela cena (antes o bake no boot
    // sempre detonava o slot 0 primeiro e escondia isto)
    this.blastPos = new Float32Array([0, 1.85, 0]);

    const G = this.grid, L = this.lightGrid;
    const rgba = { internalFormat: gl.RGBA16F, format: gl.RGBA, type: gl.HALF_FLOAT };
    const r16 = { internalFormat: gl.R16F, format: gl.RED, type: gl.HALF_FLOAT };
    const rg16 = { internalFormat: gl.RG16F, format: gl.RG, type: gl.HALF_FLOAT };

    this.vel = new PingPong(gl, G.atlasW, G.atlasH, rgba);
    this.fields = new PingPong(gl, G.atlasW, G.atlasH, rgba);
    this.press = new PingPong(gl, G.atlasW, G.atlasH, r16);
    const C = this.coarseGrid;
    // transitórios: compartilhados com o pool quando houver
    this.div = shared?.div || new Target(gl, G.atlasW, G.atlasH, r16);
    this.divC = shared?.divC || new Target(gl, C.atlasW, C.atlasH, r16);
    // a pressão é warm-started do frame anterior, então é POR SLOT
    this.pressC = new PingPong(gl, C.atlasW, C.atlasH, r16);
    this.curl = shared?.curl || new Target(gl, G.atlasW, G.atlasH, rgba);
    this.light = new Target(gl, L.atlasW, L.atlasH, rgba);
    const nearestR16 = { ...r16, filter: gl.NEAREST };
    // níveis intermediários são transitórios; só o último (dilatado) é lido
    // durante o render e precisa ser por slot
    this.macroChain = shared?.macroChain
      || this.macroLevels.map((g) => new Target(gl, g.atlasW, g.atlasH, nearestR16));
    // dilatação final num alvo separado (não se pode ler e escrever o mesmo)
    this.macro = new Target(gl, this.macroGrid.atlasW, this.macroGrid.atlasH, nearestR16);

    const P = G.glsl();
    const PL = L.glsl('L');
    const PM = this.macroGrid.glsl('M');
    const PC = this.coarseGrid.glsl('C');
    const head = `#version 300 es\nprecision highp float;\nprecision highp sampler2D;\nprecision highp sampler3D;\nin vec2 vUV;\n`;
    const mk = (name, body, extra = '') =>
      new Shader(gl, FS_VS, head + COMMON + P + extra + body, name);

    // ================= ADVECÇÃO =================
    // Semi-Lagrangiana + correção MacCormack limitada aos 8 vizinhos do
    // backtrace. Sem o limitador o esquema de 2ª ordem faz overshoot e
    // explode; com ele, ganha-se detalhe fino sem custo de estabilidade.
    const ADVECT = `
uniform sampler2D uVel, uSrc;
uniform float uDt, uDissip;
out vec4 oCol;

vec4 advect(vec3 p){
  vec3 u = sampleVol(uVel, p).xyz;
  vec3 back = p - u * (uDt * INV_CELL);
  vec4 phiHat = sampleVol(uSrc, back);

  // passo pra frente a partir do ponto retro-traçado → estimativa de erro
  vec3 u2 = sampleVol(uVel, back).xyz;
  vec3 fwd = back + u2 * (uDt * INV_CELL);
  vec4 phiRev = sampleVol(uSrc, fwd);
  vec4 phi = phiHat + 0.5 * (sampleVol(uSrc, p) - phiRev);

  // limitador: nada pode sair do envelope dos 8 voxels que geraram phiHat
  ivec3 b = ivec3(floor(back - 0.5));
  vec4 mn = vec4(1e20), mx = vec4(-1e20);
  for (int k = 0; k < 8; k++){
    ivec3 o = ivec3(k & 1, (k >> 1) & 1, (k >> 2) & 1);
    vec4 s = fetchVol(uSrc, b + o);
    mn = min(mn, s); mx = max(mx, s);
  }
  return clamp(phi, mn, mx);
}
`;

    this.shAdvectVel = mk('advectVel', ADVECT + `
void main(){
  vec3 p = fragToVoxel(gl_FragCoord.xy);
  oCol = vec4(advect(p).xyz * uDissip, 0.0);
}`);

    // variante semi-Lagrangiana pura: 2 fetches em vez de 16. Perde detalhe
    // de pequena escala na velocidade, que o vorticity confinement repõe.
    this.shAdvectVelFast = mk('advectVelFast', `
uniform sampler2D uVel, uSrc;
uniform float uDt, uDissip;
out vec4 oCol;
void main(){
  vec3 p = fragToVoxel(gl_FragCoord.xy);
  vec3 u = sampleVol(uVel, p).xyz;
  oCol = vec4(sampleVol(uSrc, p - u * (uDt * INV_CELL)).xyz * uDissip, 0.0);
}`);

    // ================= ADVECÇÃO DOS CAMPOS + REAÇÃO + INJEÇÃO =================
    // r=fuligem  g=temperatura  b=combustível  a=poeira
    this.shAdvectFields = mk('advectFields', ADVECT + `
uniform float uTime, uSeed;
uniform vec3 uBlastPos;
uniform float uIgnitionT, uBurnRate, uHeatYield, uSootYield;
uniform float uMixK, uOxyFloor, uSootRich, uSootOxid, uSootTrap;
uniform float uCooling, uCoolingLin, uSootDecay, uDustDecay, uMinDensity;
uniform float uInjectAmt, uCoreRadius, uCoreWarp, uCoreLumpScale, uFuelAmount, uCoreSoot, uCoreTemp;
uniform float uSurgeAmt, uSurgeRadius, uSurgeWidth, uSurgeHeight, uSurgeRagged;

void main(){
  vec3 p = fragToVoxel(gl_FragCoord.xy);
  vec4 f = advect(p);
  vec3 w = voxelToWorld(p);

  // ---- injeção: núcleo da detonação -------------------------------------
  if (uInjectAmt > 0.0){
    vec3 d = w - uBlastPos;
    float r = length(d);
    vec3 dir = d / max(r, 1e-4);
    // deforma o raio com ruído: a bola de fogo nunca é uma esfera
    // A bola de fogo tem ~9 células de diâmetro nos primeiros 200ms: nessa
    // escala a simulação NÃO consegue gerar turbulência sozinha. A estrutura
    // tem que vir na condição inicial.
    //
    // Deformação do raio em duas escalas (forma geral + lóbulos tipo
    // Rayleigh-Taylor) …
    float warp = fbm(dir * 2.6 + uSeed, 4, 2.15, 0.55)
               + 0.45 * fbm(dir * 7.3 - uSeed, 3, 2.4, 0.5);
    float rEff = uCoreRadius * (1.0 + uCoreWarp * warp);

    // … e borda NÍTIDA. O ramp antigo ia de 0.30·r até r — um gradiente suave
    // ocupando 70% do raio, ou seja, uma bola sem superfície. Sem interface
    // definida não há o que instabilizar, e o resultado é um abajur.
    float k = smoothstep(rEff, rEff * 0.82, r);

    // mosqueado VOLUMÉTRICO (ruído em 3D, não só na direção): bolsões ricos
    // e pobres dentro da bola, que é o que produz o salpicado escuro/claro
    float lump = 0.55 + 0.45 * fbm(d * uCoreLumpScale + uSeed * 1.7, 4, 2.3, 0.55);
    // spread largo de temperatura: a bola inteira saturada no topo da LUT
    // de corpo negro só pode produzir uma cor
    float lumpT = 0.58 + 0.42 * fbm(d * uCoreLumpScale * 1.9 - uSeed, 3, 2.2, 0.5);

    f.b += k * lump * uFuelAmount * uInjectAmt;
    f.r += k * lump * uCoreSoot * uInjectAmt;
    f.g = max(f.g, k * lumpT * uCoreTemp);
  }

  // ---- injeção: surto de base rasteiro ----------------------------------
  if (uSurgeAmt > 0.0){
    vec3 d = w - uBlastPos;
    float rh = length(d.xz);
    // o raio varia com o azimute: um anel de raio constante vira um toro de
    // borracha perfeito, que é exatamente o que estava errado antes
    vec3 az = normalize(vec3(d.x, 0.0, d.z) + vec3(1e-4, 0.0, 0.0));
    float ragged = 1.0 + uSurgeRagged * fbm(az * 2.4 + uSeed * 0.7, 4, 2.3, 0.55);
    float ring = smoothstep(1.0, 0.0, abs(rh - uSurgeRadius * ragged) / uSurgeWidth);
    float low = smoothstep(uSurgeHeight, 0.0, w.y - uBlastPos.y + 0.6);
    float grain = 0.40 + 0.60 * fbm(vec3(d.xz * 0.75, uTime * 1.4) + uSeed, 4, 2.2, 0.5);
    f.a += ring * low * grain * uSurgeAmt;
    f.g = max(f.g, ring * low * 0.14);
  }

  // ---- combustão LIMITADA POR MISTURA ------------------------------------
  // Afterburning de explosivo é controlado por mistura, não por temperatura:
  // o combustível só queima onde já encontrou oxigênio. Gatilhar só por
  // temperatura (como antes) acende a bola inteira de uma vez e produz um
  // borrão uniforme — sem frente de chama, sem núcleo rico escuro.
  //
  // O oxidante não é um campo advectado: é aproximado pelo ar fresco que
  // ainda NÃO foi deslocado pelos produtos. Isso já dá o comportamento certo
  // (chama fina na interface, núcleo rico sem queimar) a custo zero.
  float oxy = saturate(1.0 - uMixK * (f.b + f.r * 0.45)) + uOxyFloor;
  float ign = smoothstep(uIgnitionT, uIgnitionT * 1.7, f.g);
  float burn = min(f.b, uBurnRate * ign * oxy * uDt);
  f.b -= burn;
  f.g += burn * uHeatYield;

  // ---- fuligem: nasce no lado RICO, é oxidada no lado POBRE --------------
  // É por isso que fogo real tem chama limpa de um lado e fumaça preta do
  // outro, em vez de produzir fuligem uniformemente em todo lugar.
  float sootForm = burn * uSootYield * (1.0 - oxy * uSootRich);
  float sootOxid = f.r * uSootOxid * oxy * smoothstep(0.32, 0.72, f.g) * uDt;
  f.r += sootForm - min(sootOxid, f.r * 0.6);

  // ---- resfriamento radiativo com auto-absorção -------------------------
  // dT/dt ∝ -T⁴ vale no limite opticamente FINO. Uma bola de fogo carregada
  // de fuligem é opticamente espessa: ela reabsorve a própria radiação e
  // esfria bem mais devagar. Sem este termo o fogo apagava em ~0.5s.
  float T = f.g;
  T -= uDt * (uCooling * T * T * T * T / (1.0 + uSootTrap * f.r) + uCoolingLin * T);
  f.g = max(T, 0.0);

  // ---- dissipação + dissolução nas fronteiras abertas -------------------
  f.r *= 1.0 - uSootDecay * uDt;
  f.a *= 1.0 - uDustDecay * uDt;
  // Corte duro no resíduo. Decaimento exponencial tem cauda infinita: sobrava
  // densidade baixa demais pra ser vista mas alta o bastante pra sombrear, e
  // num RTS isso se acumula explosão após explosão.
  if (f.r < uMinDensity) f.r = 0.0;
  if (f.a < uMinDensity) f.a = 0.0;
  if (f.b < uMinDensity * 0.5) f.b = 0.0;
  float edge = domainFade(p);
  // fator POR SEGUNDO (referência: passo de 1/120). Por passo, uma
  // explosão velha do pool — que avança com dt 5× maior — dissolvia 5× mais
  // devagar na borda do que a mesma explosão no bake.
  f.rga *= mix(vec3(pow(0.955, uDt * 120.0)), vec3(1.0), edge);

  oCol = max(f, vec4(0.0));
}`);

    // ================= VORTICIDADE =================
    this.shCurl = mk('curl', `
uniform sampler2D uVel;
out vec4 oCol;
void main(){
  ivec3 v = ivec3(fragToVoxel(gl_FragCoord.xy));
  vec3 xp = fetchVol(uVel, v + ivec3(1,0,0)).xyz, xm = fetchVol(uVel, v - ivec3(1,0,0)).xyz;
  vec3 yp = fetchVol(uVel, v + ivec3(0,1,0)).xyz, ym = fetchVol(uVel, v - ivec3(0,1,0)).xyz;
  vec3 zp = fetchVol(uVel, v + ivec3(0,0,1)).xyz, zm = fetchVol(uVel, v - ivec3(0,0,1)).xyz;
  float s = 0.5 * INV_CELL;
  // ω = ∇×u
  oCol = vec4(s * vec3((yp.z - ym.z) - (zp.y - zm.y),
                       (zp.x - zm.x) - (xp.z - xm.z),
                       (xp.y - xm.y) - (yp.x - ym.x)), 0.0);
}`);

    // ================= FORÇAS =================
    this.shForces = mk('forces', `
uniform sampler2D uVel, uFields, uCurl;
uniform sampler3D uNoise;
uniform float uDt, uTime, uSeed;
uniform float uBuoy, uSootW, uDustW, uVort, uTurb, uTurbScale, uTurbIface, uDrag, uGroundFric, uMaxSpeed;
uniform vec3 uBlastPos;
uniform float uInjectAmt, uCoreRadius, uCoreWarp, uBlastSpeed;
uniform float uRingStrength, uRingRadius, uRingHeight, uRingCore;
uniform float uSurgeAmt, uSurgeRadius, uSurgeWidth, uSurgeHeight, uSurgeSpeed;
out vec4 oCol;

void main(){
  vec3 p = fragToVoxel(gl_FragCoord.xy);
  ivec3 v = ivec3(p);
  vec3 u = sampleVol(uVel, p).xyz;
  vec4 f = sampleVol(uFields, p);
  vec3 w = voxelToWorld(p);

  // ---- empuxo: gás quente sobe, fuligem e poeira pesam -----------------
  float lift = uBuoy * f.g - uSootW * f.r - uDustW * f.a;
  u.y += lift * uDt;

  // ---- vorticity confinement (Fedkiw et al. 2001) ----------------------
  // f = ε·h·(N̂ × ω),  N̂ = ∇|ω| / |∇|ω||
  // Reinjeta a rotação de pequena escala que a advecção numérica destrói.
  vec3 wc = sampleVol(uCurl, p).xyz;
  float mxp = length(fetchVol(uCurl, v + ivec3(1,0,0)).xyz);
  float mxm = length(fetchVol(uCurl, v - ivec3(1,0,0)).xyz);
  float myp = length(fetchVol(uCurl, v + ivec3(0,1,0)).xyz);
  float mym = length(fetchVol(uCurl, v - ivec3(0,1,0)).xyz);
  float mzp = length(fetchVol(uCurl, v + ivec3(0,0,1)).xyz);
  float mzm = length(fetchVol(uCurl, v - ivec3(0,0,1)).xyz);
  vec3 eta = 0.5 * INV_CELL * vec3(mxp - mxm, myp - mym, mzp - mzm);
  vec3 N = eta / (length(eta) + 1e-8);
  u += uVort * CELL * cross(N, wc) * uDt;

  // ---- turbulência curl-noise concentrada na INTERFACE -----------------
  // Rayleigh–Taylor e Kelvin–Helmholtz geram vorticidade na descontinuidade
  // de densidade, não no volume inteiro. Perturbar uniformemente deixa tudo
  // igualmente difuso; perturbar a interface é o que produz a superfície em
  // couve-flor de uma bola de fogo real.
  //
  // 4·d·(1-d) tem pico em d=0.5, ou seja, no meio da transição — proxy de
  // |∇ρ| que não custa nenhum fetch extra.
  float dens = saturate((f.r + f.b * 1.6 + f.a * 0.8) * 1.5);
  float iface = 4.0 * dens * (1.0 - dens);
  float act = smoothstep(0.015, 0.22, f.r + f.a * 0.7 + f.b * 2.0);
  float amp = mix(act, iface * act, uTurbIface);
  vec3 np = w * uTurbScale + vec3(0.0, -uTime * 0.09, 0.0) + uSeed * 0.31;
  vec3 turb = texture(uNoise, np).xyz * 2.0 - 1.0;
  vec3 turb2 = texture(uNoise, np * 2.7 + 0.37).xyz * 2.0 - 1.0;
  u += (turb + turb2 * 0.45) * uTurb * amp * uDt;

  // ---- impulso da detonação -------------------------------------------
  if (uInjectAmt > 0.0){
    vec3 d = w - uBlastPos;
    float r = length(d);
    vec3 dir = d / max(r, 1e-4);
    float warp = fbm(dir * 2.6 + uSeed, 4, 2.15, 0.55)
               + 0.45 * fbm(dir * 7.3 - uSeed, 3, 2.4, 0.5);
    float rEff = uCoreRadius * (1.0 + uCoreWarp * warp);
    float k = smoothstep(rEff * 1.35, rEff * 0.3, r);
    vec3 imp = dir * uBlastSpeed;
    imp.y *= 0.82;
    u += imp * k * uInjectAmt;

    // ---- semente de anel de vórtice toroidal --------------------------
    // Toda explosão real forma um vortex ring; é o que faz o cogumelo
    // *rolar* pra dentro em vez de só inflar.
    float rh = length(d.xz);
    vec3 radial = vec3(d.x, 0.0, d.z) / max(rh, 1e-4);
    vec3 lp = d - (radial * uRingRadius + vec3(0.0, uRingHeight, 0.0));
    vec3 tang = cross(vec3(0.0, 1.0, 0.0), radial);
    float wRing = exp(-dot(lp, lp) / (uRingCore * uRingCore));
    u += cross(tang, lp) * wRing * uRingStrength * uInjectAmt;
  }

  // ---- empurrão radial do surto de base -------------------------------
  if (uSurgeAmt > 0.0){
    vec3 d = w - uBlastPos;
    float rh = length(d.xz);
    vec3 radial = vec3(d.x, 0.0, d.z) / max(rh, 1e-4);
    float ring = smoothstep(1.0, 0.0, abs(rh - uSurgeRadius) / (uSurgeWidth * 1.6));
    float low = smoothstep(uSurgeHeight * 1.4, 0.0, w.y - uBlastPos.y + 0.6);
    u += radial * uSurgeSpeed * ring * low * uSurgeAmt;
  }

  // ---- arrasto + atrito no chão ---------------------------------------
  u *= 1.0 - min(uDrag * uDt, 0.5);
  float gh = w.y / max(CELL * 2.0, 1e-4);
  if (gh < 1.5){
    float fr = (1.0 - smoothstep(0.0, 1.5, gh)) * uGroundFric;
    u.xz *= 1.0 - min(fr * uDt, 0.7);
    u.y = max(u.y, 0.0);
  }

  float sp = length(u);
  if (sp > uMaxSpeed) u *= uMaxSpeed / sp;
  oCol = vec4(u, 0.0);
}`);

    // ================= DIVERGÊNCIA (com fonte de expansão da combustão) ===
    this.shDiv = mk('divergence', `
uniform sampler2D uVel, uFields;
uniform float uExpansion, uIgnitionT, uMixK, uOxyFloor;
uniform float uBlastSrc, uBlastRadius;
uniform vec3 uBlastPos;
out vec4 oCol;
void main(){
  vec3 p = fragToVoxel(gl_FragCoord.xy);
  ivec3 v = ivec3(p);
  vec3 xp = fetchVol(uVel, v + ivec3(1,0,0)).xyz, xm = fetchVol(uVel, v - ivec3(1,0,0)).xyz;
  vec3 yp = fetchVol(uVel, v + ivec3(0,1,0)).xyz, ym = fetchVol(uVel, v - ivec3(0,1,0)).xyz;
  vec3 zp = fetchVol(uVel, v + ivec3(0,0,1)).xyz, zm = fetchVol(uVel, v - ivec3(0,0,1)).xyz;
  float div = 0.5 * INV_CELL * ((xp.x - xm.x) + (yp.y - ym.y) + (zp.z - zm.z));

  // gás em combustão expande → divergência positiva imposta.
  vec4 f = sampleVol(uFields, p);
  float oxy = saturate(1.0 - uMixK * (f.b + f.r * 0.45)) + uOxyFloor;
  float ign = smoothstep(uIgnitionT, uIgnitionT * 1.7, f.g);
  float src = uExpansion * ign * oxy * f.b;

  // ---- detonação como fonte de PRESSÃO, não de velocidade --------------
  // Impor um campo de velocidade radial produz uma casca dura que viaja com
  // velocidade constante. A detonação real é uma bolha de alta pressão: a
  // projeção converte a fonte de divergência num escoamento radial com o
  // decaimento 1/r² correto e desaceleração natural. É a diferença entre
  // uma bola de fogo que se expande e uma que apenas translada.
  if (uBlastSrc > 0.0){
    vec3 w = voxelToWorld(p);
    float r = length(w - uBlastPos);
    src += uBlastSrc * smoothstep(uBlastRadius, uBlastRadius * 0.25, r);
  }

  oCol = vec4(div - src, 0.0, 0.0, 0.0);
}`);

    // ================= PRESSÃO (Jacobi) =================
    // ∇²p = ∇·u ;  Dirichlet p=0 nas faces abertas (laterais e topo),
    // Neumann no chão (o fetch clampado já dá ∂p/∂n = 0).
    this.shPressure = mk('pressure', `
uniform sampler2D uPress, uDiv;
out vec4 oCol;
void main(){
  ivec3 v = ivec3(fragToVoxel(gl_FragCoord.xy));
  if (v.x == 0 || v.y == GRIDI.y - 1 || v.z == 0 ||
      v.x == GRIDI.x - 1 || v.z == GRIDI.z - 1){
    oCol = vec4(0.0); return;
  }
  float s = fetchVol(uPress, v + ivec3(1,0,0)).x + fetchVol(uPress, v - ivec3(1,0,0)).x
          + fetchVol(uPress, v + ivec3(0,1,0)).x + fetchVol(uPress, v - ivec3(0,1,0)).x
          + fetchVol(uPress, v + ivec3(0,0,1)).x + fetchVol(uPress, v - ivec3(0,0,1)).x;
  float d = fetchVol(uDiv, v).x;
  oCol = vec4((s - d * CELL * CELL) / 6.0, 0.0, 0.0, 0.0);
}`);

    // ================= MULTIGRID DE PRESSÃO =================
    // 18 iterações de Jacobi na grade fina (2.16M px) dominavam o custo da
    // simulação. Jacobi espalha informação um voxel por iteração, então
    // convergir num domínio de 128 voxels é absurdamente lento no nível fino.
    // Resolver primeiro num grid com metade da resolução (8× menos pixels,
    // e o dobro de alcance por iteração) e depois suavizar no fino dá a mesma
    // qualidade visual por uma fração do custo. Isto é um V-cycle de 2 níveis
    // sem correção de resíduo — suficiente porque o campo de pressão de
    // fumaça é suave e o palpite inicial vem do frame anterior.

    // restrição: divergência fina → grossa (média de 2×2×2)
    this.shRestrict = new Shader(gl, FS_VS,
      `#version 300 es\nprecision highp float;\nin vec2 vUV;\n` + COMMON + P + PC + `
uniform sampler2D uDiv;
out vec4 oCol;
void main(){
  ivec3 c = ivec3(fragToVoxelC(gl_FragCoord.xy));
  ivec3 b = c * 2;
  float sum = 0.0;
  for (int k = 0; k < 8; k++)
    sum += fetchVol(uDiv, b + ivec3(k & 1, (k >> 1) & 1, (k >> 2) & 1)).x;
  oCol = vec4(sum * 0.125, 0.0, 0.0, 0.0);
}`, 'restrict');

    // Jacobi na grade grossa (mesma equação, h dobrado)
    this.shPressureC = new Shader(gl, FS_VS,
      `#version 300 es\nprecision highp float;\nin vec2 vUV;\n` + COMMON + PC + `
uniform sampler2D uPress, uDiv;
out vec4 oCol;
void main(){
  ivec3 v = ivec3(fragToVoxelC(gl_FragCoord.xy));
  if (v.x == 0 || v.y == GRIDIC.y - 1 || v.z == 0 ||
      v.x == GRIDIC.x - 1 || v.z == GRIDIC.z - 1){
    oCol = vec4(0.0); return;
  }
  float s = fetchVolC(uPress, v + ivec3(1,0,0)).x + fetchVolC(uPress, v - ivec3(1,0,0)).x
          + fetchVolC(uPress, v + ivec3(0,1,0)).x + fetchVolC(uPress, v - ivec3(0,1,0)).x
          + fetchVolC(uPress, v + ivec3(0,0,1)).x + fetchVolC(uPress, v - ivec3(0,0,1)).x;
  float d = fetchVolC(uDiv, v).x;
  oCol = vec4((s - d * CELLC * CELLC) / 6.0, 0.0, 0.0, 0.0);
}`, 'pressureCoarse');

    // prolongação: pressão grossa → fina (trilinear)
    this.shProlong = new Shader(gl, FS_VS,
      `#version 300 es\nprecision highp float;\nin vec2 vUV;\n` + COMMON + P + PC + `
uniform sampler2D uPressC;
out vec4 oCol;
void main(){
  // centro do voxel fino i+0.5 cai em (i+0.5)/2 no grid grosso
  vec3 p = fragToVoxel(gl_FragCoord.xy);
  oCol = vec4(sampleVolC(uPressC, p * 0.5).x, 0.0, 0.0, 0.0);
}`, 'prolong');

    // ================= PROJEÇÃO =================
    this.shProject = mk('project', `
uniform sampler2D uVel, uPress;
out vec4 oCol;
void main(){
  vec3 p = fragToVoxel(gl_FragCoord.xy);
  ivec3 v = ivec3(p);
  float xp = fetchVol(uPress, v + ivec3(1,0,0)).x, xm = fetchVol(uPress, v - ivec3(1,0,0)).x;
  float yp = fetchVol(uPress, v + ivec3(0,1,0)).x, ym = fetchVol(uPress, v - ivec3(0,1,0)).x;
  float zp = fetchVol(uPress, v + ivec3(0,0,1)).x, zm = fetchVol(uPress, v - ivec3(0,0,1)).x;
  vec3 grad = 0.5 * INV_CELL * vec3(xp - xm, yp - ym, zp - zm);
  vec3 u = fetchVol(uVel, v).xyz - grad;
  if (v.y == 0) u.y = max(u.y, 0.0);              // sem penetração no chão
  if (v.y == GRIDI.y - 1) u.y = max(u.y, 0.0);
  oCol = vec4(u, 0.0);
}`);

    // ================= VOLUME DE ATENUAÇÃO DE LUZ =================
    // Transmitância pré-integrada até o sol e até o céu (zênite), em meia
    // resolução. Custo: o raymarch principal passa a auto-sombrear a fumaça
    // com 1 tap por passo em vez de um march secundário aninhado.
    // Reusa volumeShadow(), que já tem o DDA de blocos macro: a maior parte
    // do volume de luz é vazio, e antes marchava 22 amostras por voxel nele.
    this.shLight = new Shader(gl, FS_VS,
      `#version 300 es\nprecision highp float;\nin vec2 vUV;\n`
      + COMMON + P + PL + PM + VOLUME_SHADOW + `
uniform sampler2D uFields, uMacro;
uniform vec3 uSunDir, uFirePos;
uniform float uSootExt, uDustExt, uSteps, uErodeMean;
out vec4 oCol;

void main(){
  vec3 w = voxelToWorldL(fragToVoxelL(gl_FragCoord.xy));
  int n = int(uSteps);
  vec3 toFire = uFirePos - w;
  float fireDist = length(toFire);
  oCol = vec4(
    volumeShadow(uFields, uMacro, w, uSunDir, 1e4, uSootExt, uDustExt, n, 0.5, 60.0, uErodeMean),
    volumeShadow(uFields, uMacro, w, vec3(0.0, 1.0, 0.0), 1e4, uSootExt, uDustExt, n, 0.5, 60.0, uErodeMean),
    fireDist < 1e-3 ? 1.0
      // tauCap BAIXO aqui, de propósito: a aproximação de luz pontual no
      // centróide dá transmitância ~0 pra fumaça e poeira coladas na bola de
      // fogo (o raio atravessa a bola inteira), quando na realidade elas são
      // banhadas pela superfície próxima do emissor. Sem o cap, a poeira ao
      // redor absorve sem receber luz nenhuma e vira um halo preto na
      // silhueta — exatamente o contorno de "adesivo".
      : volumeShadow(uFields, uMacro, w, toFire / fireDist, fireDist,
                     uSootExt, uDustExt, min(n, 16), 0.5, 2.2, uErodeMean),
    0.0);
}`, 'lightVolume');

    // Nível 0: extinção do campo → max 2×2×2. Níveis seguintes: max do
    // anterior. Último passo dilata 1 célula macro em todas as direções, o
    // que torna o bloco conservador em relação ao deslocamento por ruído do
    // raymarch (o DDA depende disso pra nunca pular detalhe).
    const maxDown = (srcGrid, dstGrid, fromFields) => new Shader(gl, FS_VS,
      `#version 300 es\nprecision highp float;\nin vec2 vUV;\n` + COMMON
      + srcGrid.glsl('S') + dstGrid.glsl('D') + `
uniform sampler2D uSrc;
uniform float uSootExt, uDustExt, uErodeMean;
out vec4 oCol;
// mesma correção da sombra: o bloco macro é um MÁXIMO conservador, então ele
// usa a densidade efetiva (senão o raymarch nunca pula o resíduo invisível)
float effExtinctionSimple(vec4 f, float se, float de, float em){
  float r = max(f.r - em / (1.0 + f.r * 9.0), 0.0);
  float a = max(f.a - em * 0.7 / (1.0 + f.a * 9.0), 0.0);
  return se * r + de * a;
}
void main(){
  ivec3 dv = ivec3(fragToVoxelD(gl_FragCoord.xy));
  ivec3 base = dv * 2;
  float mx = 0.0;
  for (int k = 0; k < 8; k++){
    vec4 f = fetchVolS(uSrc, base + ivec3(k & 1, (k >> 1) & 1, (k >> 2) & 1));
    mx = max(mx, ${fromFields ? 'uSootExt * f.r + uDustExt * f.a' : 'f.r'});
  }
  oCol = vec4(mx, 0.0, 0.0, 0.0);
}`, 'macroDown');

    this.shMacroChain = this.macroLevels.map((g, i) =>
      maxDown(i === 0 ? this.grid : this.macroLevels[i - 1], g, i === 0));

    this.shMacroDilate = new Shader(gl, FS_VS,
      `#version 300 es\nprecision highp float;\nin vec2 vUV;\n` + COMMON
      + this.macroGrid.glsl('M') + `
uniform sampler2D uSrc;
out vec4 oCol;
void main(){
  ivec3 v = ivec3(fragToVoxelM(gl_FragCoord.xy));
  float mx = 0.0;
  for (int z = -1; z <= 1; z++)
    for (int y = -1; y <= 1; y++)
      for (int x = -1; x <= 1; x++)
        mx = max(mx, fetchVolM(uSrc, v + ivec3(x, y, z)).r);
  oCol = vec4(mx, 0.0, 0.0, 0.0);
}`, 'macroDilate');

    this.shClear = mk('clearVol', `out vec4 oCol; void main(){ oCol = vec4(0.0); }`);

    if (shared?.noiseTex) this.noiseTex = shared.noiseTex;
    else this._buildNoise();
    this.reset();
  }

  // ---- campo de ruído curl tileável, gerado uma única vez ----------------
  _buildNoise() {
    const gl = this.gl;
    this.noiseTex = createTexture3D(gl, NOISE_RES, NOISE_RES, NOISE_RES);
    const sh = new Shader(gl, FS_VS, `#version 300 es
precision highp float;
in vec2 vUV;
uniform float uLayer;
out vec4 oCol;

#define PERIOD 4.0

// value-noise periódico: o hash é feito no reticulado mod PERIOD, então o
// campo resultante é perfeitamente tileável (obrigatório: a textura usa REPEAT)
float vhash(vec3 i, float per){
  i = mod(i, vec3(per));
  return fract(sin(dot(i, vec3(127.1, 311.7, 74.7))) * 43758.5453123);
}
float vnoise(vec3 p, float per){
  vec3 i = floor(p), f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float n000 = vhash(i + vec3(0,0,0), per), n100 = vhash(i + vec3(1,0,0), per);
  float n010 = vhash(i + vec3(0,1,0), per), n110 = vhash(i + vec3(1,1,0), per);
  float n001 = vhash(i + vec3(0,0,1), per), n101 = vhash(i + vec3(1,0,1), per);
  float n011 = vhash(i + vec3(0,1,1), per), n111 = vhash(i + vec3(1,1,1), per);
  return mix(mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y),
             mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y), f.z) * 2.0 - 1.0;
}
// fbm mantendo periodicidade: a oitava k tem período PERIOD·2^k
float pfbm(vec3 p){
  float s = 0.0, a = 0.55, n = 0.0, per = PERIOD, sc = 1.0;
  for (int i = 0; i < 3; i++){
    s += a * vnoise(p * sc, per * sc);
    n += a; sc *= 2.0; a *= 0.5;
  }
  return s / n;
}
// ψ = potencial vetorial ; retorna ∇×ψ (garantidamente divergence-free)
void main(){
  vec3 p = vec3(vUV, uLayer) * PERIOD;
  float e = PERIOD / ${NOISE_RES}.0;
  vec3 o2 = vec3(1.7, 0.3, 2.9) * PERIOD;
  vec3 o3 = vec3(2.3, 3.1, 0.7) * PERIOD;
  vec3 dx = vec3(e,0,0), dy = vec3(0,e,0), dz = vec3(0,0,e);
  float p1yp = pfbm(p + dy),      p1ym = pfbm(p - dy);
  float p1zp = pfbm(p + dz),      p1zm = pfbm(p - dz);
  float p2xp = pfbm(p + o2 + dx), p2xm = pfbm(p + o2 - dx);
  float p2zp = pfbm(p + o2 + dz), p2zm = pfbm(p + o2 - dz);
  float p3xp = pfbm(p + o3 + dx), p3xm = pfbm(p + o3 - dx);
  float p3yp = pfbm(p + o3 + dy), p3ym = pfbm(p + o3 - dy);
  vec3 c = vec3((p3yp - p3ym) - (p2zp - p2zm),
                (p1zp - p1zm) - (p3xp - p3xm),
                (p2xp - p2xm) - (p1yp - p1ym)) / (2.0 * e);
  oCol = vec4(c * 0.12 * 0.5 + 0.5, 1.0); // remapeado pra [0,1]
}`, 'noiseGen');
    renderToTexture3D(gl, this.noiseTex, NOISE_RES, NOISE_RES, NOISE_RES, sh);
  }

  reset() {
    const gl = this.gl;
    this.time = 0;
    this.frame = 0;
    this.shClear.use();
    // só os buffers PRÓPRIOS: limpar os compartilhados apagaria trabalho
    // em voo de outras explosões do pool
    for (const t of [this.vel.a, this.vel.b, this.fields.a, this.fields.b,
                     this.press.a, this.press.b, this.macro,
                     this.pressC.a, this.pressC.b]) {
      t.bind();
      drawFS(gl);
    }
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }

  /**
   * @param {number[]} pos    posição da detonação, em mundo
   * @param {number} seed
   * @param {number[]} origin centro horizontal do domínio da simulação.
   *   Por padrão acompanha a detonação — é o que permite explodir em
   *   qualquer ponto do terreno em vez de só no centro do mundo.
   */
  detonate(pos = [0, 1.9, 0], seed = Math.random() * 1000, origin = null) {
    this.reset();
    this.blastPos = new Float32Array(pos);
    this.domainOrigin = new Float32Array(origin || [pos[0], 0, pos[2]]);
    this.seed = seed;
  }

  /** raio do surto de base pela lei de Taylor–Sedov (R ∝ t^(2/5)) */
  surgeRadius(t) {
    return this.params.surgeCoef * Math.pow(Math.max(t, 1e-3), 0.4);
  }

  step(dt, sunDir, sootExt, dustExt, firePos, erodeMean = 0) {
    const gl = this.gl, P = this.params, G = this.grid, L = this.lightGrid;
    const t = this.time;
    const inj = t < P.injectWindow ? Math.min(dt, P.injectWindow - t) / P.injectWindow : 0;
    // o surto de base só arranca depois do núcleo (a onda tem que tocar o
    // chão antes de levantar poeira)
    const surgeT = (t - 0.05) / P.surgeDur;
    const surge = surgeT > 0 && surgeT < 1
      ? Math.pow(1 - surgeT, 1.7) * Math.sqrt(Math.min(surgeT * 6, 1)) * dt * 30 : 0;

    gl.disable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);
    gl.viewport(0, 0, G.atlasW, G.atlasH);

    // 1. advecção da velocidade
    (P.velMacCormack ? this.shAdvectVel : this.shAdvectVelFast).use()
      .set('uDt', dt).set('uDissip', Math.pow(P.velDissip, dt * 120))   // por segundo, ref. 1/120
      .tex('uVel', this.vel.read.tex).tex('uSrc', this.vel.read.tex);
    this.vel.write.bind(); drawFS(gl); this.vel.swap();

    // 2. advecção dos campos + combustão + resfriamento + injeção
    this.shAdvectFields.use()
      .set('uDt', dt).set('uDissip', 1.0).set('uTime', t).set('uSeed', this.seed)
      .set('uBlastPos', this.blastPos)
      .set('uIgnitionT', P.ignitionT).set('uBurnRate', P.burnRate)
      .set('uHeatYield', P.heatYield).set('uSootYield', P.sootYield)
      .set('uCooling', P.cooling).set('uCoolingLin', P.coolingLin)
      .set('uMixK', P.mixK).set('uOxyFloor', P.oxyFloor)
      .set('uSootRich', P.sootRich).set('uSootOxid', P.sootOxid)
      .set('uSootTrap', P.sootTrap)
      .set('uSootDecay', P.sootDecay).set('uDustDecay', P.dustDecay)
      .set('uMinDensity', P.minDensity)
      .set('uInjectAmt', inj).set('uCoreRadius', P.coreRadius).set('uCoreWarp', P.coreWarp)
      .set('uFuelAmount', P.fuelAmount).set('uCoreSoot', P.coreSoot).set('uCoreTemp', P.coreTemp)
      .set('uCoreLumpScale', P.coreLumpScale)
      .set('uSurgeAmt', surge * P.surgeAmount).set('uSurgeRadius', this.surgeRadius(t))
      .set('uSurgeWidth', P.surgeWidth).set('uSurgeHeight', P.surgeHeight)
      .set('uSurgeRagged', P.surgeRagged).set('uDomainOrigin', this.domainOrigin)
      .tex('uVel', this.vel.read.tex).tex('uSrc', this.fields.read.tex);
    this.fields.write.bind(); drawFS(gl); this.fields.swap();

    // 3. vorticidade
    this.shCurl.use().tex('uVel', this.vel.read.tex);
    this.curl.bind(); drawFS(gl);

    // 4. forças
    this.shForces.use()
      .set('uDt', dt).set('uTime', t).set('uSeed', this.seed)
      .set('uBuoy', P.buoyancy).set('uSootW', P.sootWeight).set('uDustW', P.dustWeight)
      .set('uVort', P.vorticity)
      .set('uTurb', P.turbulence * Math.exp(-t * P.turbDecay) + P.turbulence * 0.16)
      .set('uTurbScale', P.turbScale).set('uTurbIface', P.turbIface).set('uDrag', P.drag)
      .set('uGroundFric', P.groundFric).set('uMaxSpeed', P.maxSpeed)
      .set('uBlastPos', this.blastPos)
      .set('uInjectAmt', inj).set('uCoreRadius', P.coreRadius).set('uCoreWarp', P.coreWarp)
      .set('uBlastSpeed', P.blastSpeed)
      .set('uRingStrength', P.ringStrength).set('uRingRadius', P.ringRadius)
      .set('uRingHeight', P.ringHeight).set('uRingCore', P.ringCore)
      .set('uSurgeAmt', surge * 1.0).set('uSurgeRadius', this.surgeRadius(t))
      .set('uSurgeWidth', P.surgeWidth).set('uSurgeHeight', P.surgeHeight)
      .set('uSurgeSpeed', P.surgeSpeed).set('uDomainOrigin', this.domainOrigin)
      .tex('uVel', this.vel.read.tex).tex('uFields', this.fields.read.tex)
      .tex('uCurl', this.curl.tex).tex('uNoise', this.noiseTex, gl.TEXTURE_3D);
    this.vel.write.bind(); drawFS(gl); this.vel.swap();

    // 5. divergência
    // a fonte de pressão da detonação decai exponencialmente: é um pulso
    const blastSrc = P.blastSrc * Math.exp(-t / P.blastTau);
    this.shDiv.use()
      .set('uExpansion', P.expansion).set('uIgnitionT', P.ignitionT)
      .set('uMixK', P.mixK).set('uOxyFloor', P.oxyFloor)
      .set('uBlastSrc', blastSrc < 0.05 ? 0 : blastSrc)
      .set('uBlastRadius', P.blastRadius).set('uBlastPos', this.blastPos)
      .set('uDomainOrigin', this.domainOrigin)
      .tex('uVel', this.vel.read.tex).tex('uFields', this.fields.read.tex);
    this.div.bind(); drawFS(gl);

    // 6. pressão — V-cycle de 2 níveis, com warm start do frame anterior
    //    nos dois níveis.
    const C = this.coarseGrid;
    gl.viewport(0, 0, C.atlasW, C.atlasH);
    this.shRestrict.use().tex('uDiv', this.div.tex);
    this.divC.bind(); drawFS(gl);

    this.shPressureC.use();
    for (let i = 0; i < P.pressureCoarse; i++) {
      this.shPressureC._unit = 0;
      this.shPressureC.tex('uPress', this.pressC.read.tex).tex('uDiv', this.divC.tex);
      this.pressC.write.bind(); drawFS(gl); this.pressC.swap();
    }

    gl.viewport(0, 0, G.atlasW, G.atlasH);
    this.shProlong.use().tex('uPressC', this.pressC.read.tex);
    this.press.write.bind(); drawFS(gl); this.press.swap();

    this.shPressure.use();
    for (let i = 0; i < P.pressureFine; i++) {
      this.shPressure._unit = 0;
      this.shPressure.tex('uPress', this.press.read.tex).tex('uDiv', this.div.tex);
      this.press.write.bind(); drawFS(gl); this.press.swap();
    }

    // 7. projeção
    this.shProject.use()
      .tex('uVel', this.vel.read.tex).tex('uPress', this.press.read.tex);
    this.vel.write.bind(); drawFS(gl); this.vel.swap();

    // 8. grade macro de oclusão (usada pra pular espaço vazio)
    for (let i = 0; i < this.macroLevels.length; i++) {
      const g = this.macroLevels[i], dst = this.macroChain[i];
      gl.viewport(0, 0, g.atlasW, g.atlasH);
      this.shMacroChain[i].use()
        .set('uSootExt', sootExt).set('uDustExt', dustExt).set('uErodeMean', erodeMean)
        .tex('uSrc', i === 0 ? this.fields.read.tex : this.macroChain[i - 1].tex);
      dst.bind(); drawFS(gl);
    }
    gl.viewport(0, 0, this.macroGrid.atlasW, this.macroGrid.atlasH);
    this.shMacroDilate.use().tex('uSrc', this.macroChain[this.macroChain.length - 1].tex);
    this.macro.bind(); drawFS(gl);

    // 9. volume de atenuação de luz
    gl.viewport(0, 0, L.atlasW, L.atlasH);
    this.shLight.use()
      .set('uSunDir', sunDir).set('uSootExt', sootExt).set('uDustExt', dustExt)
      .set('uFirePos', firePos).set('uSteps', P.lightSteps).set('uErodeMean', erodeMean)
      .set('uDomainOrigin', this.domainOrigin)
      .tex('uFields', this.fields.read.tex).tex('uMacro', this.macro.tex);
    this.light.bind(); drawFS(gl);

    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    this.time += dt;
    this.frame++;
  }
}
