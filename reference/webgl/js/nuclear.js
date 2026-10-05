// ---------------------------------------------------------------------------
// nuclear.js — o que a bomba nuclear tem e as armas químicas não: a onda de
// choque correndo pelo chão (o anel de poeira), o ar iluminado pelo clarão e
// pela bola de fogo, e o tranco na câmera quando o choque chega até ela.
//
// A explosão em si é a MESMA sequência assada das outras armas, na escala 9
// (MAGNITUDES em instances.js); o clarão de duas máximas também vive lá
// (lights()). Aqui fica só o que a sequência química não tem.
//
// A ordem do que se vê numa detonação nuclear (filmagens dos testes; Glasstone
// & Dolan, "The Effects of Nuclear Weapons", 1977, caps. II, III e VII):
//   1. o clarão — o primeiro em menos de 1 ms, a segunda máxima em décimos;
//   2. a frente de choque se separa da bola de fogo e corre pelo chão
//      levantando um anel de poeira ("dust skirt"); quem está longe vê o anel
//      chegar e só então sente o tranco — a luz chega antes, o choque anda a
//      pouco mais que a velocidade do som;
//   3. a bola de fogo sobe e puxa a coluna: o cogumelo leva dezenas de
//      segundos (aqui ~54 s, a sequência de 6 s na escala 9).
// No C&C Generals o ataque nuclear também é lido assim: clarão, anel de
// choque no chão, cogumelo.
// ---------------------------------------------------------------------------

import { Shader, drawFS, FS_VS } from './gl.js';
import { COMMON, ENVLUT } from './glsl.js';

const C0 = 340;            // m/s: velocidade do som ao nível do mar (15 °C)
const GAMMA = 1.4;
const KG_PER_UNIT = 20;    // unidade de rendimento do arsenal: granada 0.05 = 1 kg de TNT
// Carga equivalente da onda de choque: numa arma de fissão ~50% do
// rendimento vai pro choque (35% é radiação térmica; Glasstone & Dolan, cap.
// I), e numa detonação junto ao chão o reflexo dobra a carga equivalente (a
// energia toda vai pro hemisfério de cima; Kinney & Graham 1985). Os dois se
// cancelam: carga equivalente em TNT = rendimento.
const BLAST_FRACTION = 0.5, GROUND_REFLECTION = 2.0;
// Vento logo atrás da frente que levanta poeira solta: o limiar de
// arrasto de areia/poeira fica em ~6 m/s de vento (Bagnold); acima de ~40 m/s
// o chão inteiro sobe.
const U_LIFT = 6, U_FULL = 40;
const NT = 32;             // amostras da tabela radial que o shader interpola
const R_CAP = 1500;        // m: além disso o anel não levanta mais nada

/**
 * Sobrepressão de pico da onda de choque, Δp/p0, a Z = R/W^(1/3) m/kg^(1/3)
 * (Kinney & Graham 1985, "Explosive Shocks in Air", ajuste pro TNT ao ar
 * livre).
 */
export function overpressure(Z) {
  const a = Z / 4.5, b = Z / 0.048, c = Z / 0.32, d = Z / 1.35;
  return 808 * (1 + a * a) / Math.sqrt((1 + b * b) * (1 + c * c) * (1 + d * d));
}
// Rankine–Hugoniot (gás ideal, γ = 1.4): Mach da frente e o vento atrás dela
const shockMach = (dp) => Math.sqrt(1 + (GAMMA + 1) / (2 * GAMMA) * dp);
const windBehind = (dp) => C0 * dp / (GAMMA * shockMach(dp));
const smooth = (a, b, x) => { const t = Math.min(Math.max((x - a) / (b - a), 0), 1); return t * t * (3 - 2 * t); };

const HEAD = `#version 300 es
precision highp float;
precision highp sampler2D;
precision highp sampler3D;
in vec2 vUV;
`;
const MAXL = 8;

export class NuclearFX {
  constructor(gl) {
    this.gl = gl;
    this.list = [];
    this.params = {
      // ---- anel de poeira ----
      steps: 28,
      // Parede de poeira logo atrás da frente. Espessura óptica VERTICAL
      // σ0·H ≈ 0.8 com a carga cheia (o chão ainda aparece de cima — numa
      // câmera de RTS o anel não pode apagar o campo), mas de lado o raio
      // atravessa dezenas de metros dela e vira parede.
      h0: 18.0,            // m: altura da parede na frente
      sigma0: 0.07,        // 1/m de extinção com a carga cheia
      // a parede sobe com a turbulência da esteira enquanto corre: +25% da
      // altura por segundo (18 m → ~27 m quando o anel some, a ~2 s)
      rise: 0.25,
      // A parede densa fica colada na frente (faixa de ~20 m: nas filmagens
      // se vê um ANEL correndo, não um disco — logo atrás dela a poeira se
      // mistura pra cima e dilui). A poeira (albedo ~0.64) é ~10× mais clara
      // que este chão (0.07): um véu de 15% atrás da frente já lia como um
      // disco chapado e claro, e um de 5% não se via e custava a marcha do
      // disco inteiro. Fica só a parede.
      band: 20,
      // ---- ar iluminado ----
      airSteps: 24,
      airTop: 60,          // m: a névoa da cena (falloff 0.09/m) acaba aqui pra efeito
      airRange: 900,       // m em volta da luz
      airG: 0.6,           // aerossol: espalhamento pra frente (Henyey–Greenstein)
    };

    // ---- anel de poeira: marcha no alvo do volume, por trás do que já está
    // lá (as explosões), com o mesmo blend front-to-back das instâncias -----
    this.shDust = new Shader(gl, FS_VS, HEAD + COMMON + ENVLUT + `
#define NT ${NT}
#define MAXL ${MAXL}
uniform sampler2D uHalfDepth;
uniform sampler3D uNoise;
uniform mat4 uInvViewProj;
uniform vec3 uCamPos, uKeyDir, uGZ;
uniform float uNear, uJitter;
uniform float uAge, uR, uRIn, uRMax, uTop, uH0, uSigma0, uBand;
uniform float uLoad[NT];
uniform vec3 uDustColor;
uniform float uDustAlbedo, uPhaseG, uPhaseBack, uPhaseMix, uSkyGain, uMsExt, uMsScatter, uMsPhase;
uniform int uSteps, uLCount;
uniform vec4 uLP[MAXL];   // luz das explosões: posição.xyz, s²
uniform vec3 uLC[MAXL];
out vec4 oCol;

vec2 rayCyl(vec3 ro, vec3 rd, vec3 c, float R){
  vec2 o = ro.xz - c.xz, d = rd.xz;
  float a = dot(d, d), b = dot(o, d), cc = dot(o, o) - R * R;
  if (a < 1e-8) return cc <= 0.0 ? vec2(-1e9, 1e9) : vec2(1.0, -1.0);
  float disc = b * b - a * cc;
  if (disc < 0.0) return vec2(1.0, -1.0);
  float s = sqrt(disc);
  return vec2((-b - s) / a, (-b + s) / a);
}
vec2 raySlab(vec3 ro, vec3 rd, float y0, float y1){
  if (abs(rd.y) < 1e-6) return (ro.y >= y0 && ro.y <= y1) ? vec2(-1e9, 1e9) : vec2(1.0, -1.0);
  float t0 = (y0 - ro.y) / rd.y, t1 = (y1 - ro.y) / rd.y;
  return vec2(min(t0, t1), max(t0, t1));
}

// Densidade (extinção, 1/m) no ponto. A PAREDE: uma faixa de ~uBand atrás
// da frente, altura ~uH0 com o topo em rolos, tão densa quanto a poeira que
// o vento daquele raio levantou (tabela uLoad). colAbove: profundidade
// óptica até o topo dela, pra auto-sombra sem marcha.
float dustAt(vec3 p, out float colAbove){
  vec3 q = p - uGZ;
  float r = length(q.xz);
  colAbove = 0.0;
  float x = uR - r;                           // metros atrás da frente
  if (x < -4.0 || x > uBand * 6.0 || r >= uRMax) return 0.0;
  float fi = r / uRMax * float(NT - 1);
  int i = min(int(fi), NT - 2);
  float load = mix(uLoad[i], uLoad[i + 1], fi - float(i));
  // Poeira em rolos: duas oitavas BAIXAS de ruído (~50 m e ~20 m, os
  // turbilhões da esteira), subindo devagar. Uma oitava fina (~12 m) numa
  // camada rasa marchada em passos longos virava risco, textura de chão.
  // A textura (curl remapeado) tem média 0.5 e desvio ~0.05 por canal; a
  // soma ponderada, ~0.036: z é o ruído normalizado (~N(0,1)).
  vec3 np = q * 0.02 + vec3(0.0, -uAge * 0.12, 0.0);
  float n = texture(uNoise, np).x * 0.6
          + texture(uNoise, q * 0.05 + vec3(3.1, -uAge * 0.25, 1.7)).y * 0.4;
  float z = (n - 0.5) / 0.036;
  // face externa macia (~10 m; o choque em si tem milímetros), cauda de
  // uBand atrás dela, topo em rolos entre ~0.3 e 1.7× uH0
  float wall = smoothstep(-4.0, 8.0, x) * exp(-max(x, 0.0) / uBand);
  float top = uH0 * (1.0 + 0.45 * clamp(z, -1.5, 1.5));
  float hw = 1.0 - smoothstep(top * 0.35, top, max(p.y, 0.0));
  float d = load * smoothstep(-1.0, 1.0, z) * wall;
  colAbove = uSigma0 * d * max(top - p.y, 0.0) * 0.5;
  return uSigma0 * d * hw;
}

void main(){
  vec2 ndc = vUV * 2.0 - 1.0;
  vec4 h0 = uInvViewProj * vec4(ndc, -1.0, 1.0);
  vec4 h1 = uInvViewProj * vec4(ndc, 1.0, 1.0);
  vec3 ro = uCamPos;
  vec3 rd = normalize(h1.xyz / h1.w - h0.xyz / h0.w);
  float tScene = texture(uHalfDepth, vUV).r;

  // Só o ANEL onde há parede: cilindro de fora (a frente) menos o de dentro
  // (6 caudas atrás dela), numa fatia só um pouco mais alta que o topo dos
  // rolos. Marchar o disco inteiro numa fatia de 150 m custava ~4 ms.
  vec2 hc = rayCyl(ro, rd, uGZ, min(uR + 4.0, uRMax));
  vec2 hs = raySlab(ro, rd, 0.0, uTop);
  vec2 hit = vec2(max(max(hc.x, hs.x), uNear), min(min(hc.y, hs.y), tScene));
  if (hit.y <= hit.x){ oCol = vec4(0.0, 0.0, 0.0, 1.0); return; }
  vec2 hi = uRIn > 1.0 ? rayCyl(ro, rd, uGZ, uRIn) : vec2(1.0, -1.0);
  hi = vec2(max(hi.x, hit.x), min(hi.y, hit.y));    // trecho dentro do furo
  float len = (hit.y - hit.x) - max(hi.y - hi.x, 0.0);
  if (len <= 1e-3){ oCol = vec4(0.0, 0.0, 0.0, 1.0); return; }

  float dt = len / float(uSteps);
  float t = hit.x + dt * ignoise(gl_FragCoord.xy + uJitter * 61.7);
  float cosSun = dot(rd, uKeyDir);
  float phSun = phaseDual(cosSun, uPhaseG, uPhaseBack, uPhaseMix);
  float phIso = 1.0 / (4.0 * PI);
  vec3 keyCol = envKeyColor(), skyCol = envSkyUp();
  float mu = max(uKeyDir.y, 0.05);

  vec3 L = vec3(0.0);
  float Tr[3];
  for (int i = 0; i < 3; i++) Tr[i] = 1.0;
  for (int i = 0; i < 64; i++){
    if (t > hi.x && t < hi.y) t = hi.y + (t - hi.x);   // pula o furo do anel
    if (i >= uSteps || t >= hit.y || Tr[0] < 0.004) break;
    vec3 w = ro + rd * t;
    float col;
    float sig = dustAt(w, col);
    if (sig > 1e-5){
      // Luz: profundidade óptica até o topo da parede (colAbove) — sombra
      // do sol e do céu sem marcha nenhuma
      float sunT = exp(-col / mu), skyT = exp(-col * 1.6);
      vec3 fireIrr = vec3(0.0);
      for (int l = 0; l < MAXL; l++){
        if (l >= uLCount) break;
        vec3 dl = uLP[l].xyz - w;
        fireIrr += uLC[l] / (uLP[l].w + dot(dl, dl) * 0.05);
      }
      // oitavas de espalhamento múltiplo (Wrenninge et al. 2013), as mesmas
      // constantes das explosões: a poeira bege não fica chapada no escuro
      float ae = 1.0, ab = 1.0, ap = 1.0;
      for (int o = 0; o < 3; o++){
        float sg = sig * ae;
        float sc = sg * uDustAlbedo * ab;
        float ph = mix(phIso, phSun, ap);
        vec3 src = uDustColor * sc * (keyCol * mix(1.0, sunT, ap) * ph
                                    + skyCol * uSkyGain * mix(1.0, skyT, ap * 0.7 + 0.3) * phIso
                                    + fireIrr * phIso);
        float ext = max(sg, 1e-6);
        float trS = exp(-ext * dt);
        L += Tr[o] * (src - src * trS) / ext;
        Tr[o] *= trS;
        ae *= uMsExt; ab *= uMsScatter; ap *= uMsPhase;
      }
    }
    t += dt;
  }
  oCol = vec4(L, Tr[0]);
}`, 'nukeDust');

    // ---- ar iluminado: espalhamento simples da luz da bomba na névoa ----
    // A névoa de altura da cena (σ = fogDensity·e^(-y·falloff)) é ar com
    // aerossol: com uma fonte de ~100× o sol ela brilha, e é isso que faz a
    // cena INTEIRA clarear no clarão, não só o chão. Depois, o mesmo ar em
    // volta da base fica laranja com a bola de fogo. Espalhamento simples ao
    // longo do raio até a superfície, a mesma queda P/(s² + 0.02d²) que a cena
    // usa no chão (o chão e o ar sobre ele concordam), fase de Henyey–
    // Greenstein. Somado à imagem composta; o trecho atrás da fumaça das
    // explosões é atenuado pela transmitância dela (profundidade do aux).
    this.shAir = new Shader(gl, FS_VS, HEAD + COMMON + `
uniform sampler2D uDepth, uVolC, uVolAux;
uniform mat4 uInvViewProj;
uniform vec3 uCamPos;
uniform float uFogDensity, uFogFalloff, uJitter, uTop, uRange, uG;
uniform int uSteps, uLCount;
uniform vec4 uLP[2];
uniform vec3 uLC[2];
out vec4 oCol;

vec2 raySphere(vec3 ro, vec3 rd, vec3 c, float R){
  vec3 o = ro - c;
  float b = dot(o, rd), cc = dot(o, o) - R * R, disc = b * b - cc;
  if (disc < 0.0) return vec2(1.0, -1.0);
  float s = sqrt(disc);
  return vec2(-b - s, -b + s);
}

void main(){
  vec2 ndc = vUV * 2.0 - 1.0;
  vec4 h0 = uInvViewProj * vec4(ndc, -1.0, 1.0);
  vec4 h1 = uInvViewProj * vec4(ndc, 1.0, 1.0);
  vec3 ro = uCamPos;
  vec3 rd = normalize(h1.xyz / h1.w - h0.xyz / h0.w);
  float dz = texture(uDepth, vUV).r;
  float tEnd = 1e5;
  if (dz < 0.999999){
    vec4 hw = uInvViewProj * vec4(ndc, dz * 2.0 - 1.0, 1.0);
    tEnd = length(hw.xyz / hw.w - ro);
  }
  // fumaça das explosões na frente: profundidade média e transmitância
  vec4 aux = texture(uVolAux, vUV);
  float vDepth = aux.z > 0.02 ? aux.x / aux.z : 1e9;
  float vT = texture(uVolC, vUV).a;

  vec3 L = vec3(0.0);
  for (int l = 0; l < 2; l++){
    if (l >= uLCount) break;
    vec3 lp = uLP[l].xyz;
    // só onde há névoa (abaixo de uTop) e luz (perto da bomba)
    vec2 hs = raySphere(ro, rd, lp, uRange);
    float t0 = max(hs.x, 0.0), t1 = min(hs.y, tEnd);
    if (abs(rd.y) > 1e-5){
      float ty = (uTop - ro.y) / rd.y;
      if (rd.y < 0.0) t0 = max(t0, ty); else t1 = min(t1, ty);
    } else if (ro.y > uTop) continue;
    if (t1 <= t0) continue;
    float dt = (t1 - t0) / float(uSteps);
    float t = t0 + dt * ignoise(gl_FragCoord.xy + uJitter * 37.3 + float(l) * 11.0);
    for (int i = 0; i < 48; i++){
      if (i >= uSteps || t >= t1) break;
      vec3 p = ro + rd * t;
      float sig = uFogDensity * exp(-max(p.y, 0.0) * uFogFalloff);
      vec3 dl = p - lp;
      float d2 = dot(dl, dl);
      // fase: luz saindo da bomba (dl) espalhada na direção da câmera (-rd)
      float c = dot(dl * inversesqrt(max(d2, 1e-6)), -rd);
      float ph = mix(1.0 / (4.0 * PI), phaseHG(c, uG), 0.5);
      vec3 E = uLC[l] / (uLP[l].w + 0.02 * d2);
      L += E * (sig * ph * dt) * (t > vDepth ? vT : 1.0);
      t += dt;
    }
  }
  oCol = vec4(L, 0.0);
}`, 'nukeAir');
  }

  get active() { return this.list.length > 0; }

  /** nova detonação no chão, na escala (W^(1/3)) da instância */
  spawn(pos, scale) {
    const W = KG_PER_UNIT * scale * scale * scale * BLAST_FRACTION * GROUND_REFLECTION;
    const w3 = Math.cbrt(W);
    // Tempo de chegada da frente por raio: dR/dt = c0·M(R), integrado em
    // passos de 0.5 m (no começo ela anda a vários km/s; a ~100 m já é quase
    // som). Até onde o vento atrás dela ainda levanta poeira.
    const rs = [0], ts = [0];
    let R = 0, t = 0;
    while (R < R_CAP) {
      const dR = 0.5, Rm = R + dR * 0.5;
      const dp = overpressure(Math.max(Rm, 0.05) / w3);
      t += dR / (C0 * shockMach(dp));
      R += dR;
      rs.push(R); ts.push(t);
      if (windBehind(dp) < U_LIFT) break;
    }
    const rMax = R;
    // poeira levantada por raio, pelo vento atrás da frente (o shader interpola)
    const load = new Float32Array(NT);
    for (let i = 0; i < NT; i++) {
      const r = (i / (NT - 1)) * rMax;
      load[i] = smooth(U_LIFT, U_FULL, windBehind(overpressure(Math.max(r, 0.05) / w3)));
    }
    this.list.push({
      pos: [pos[0], 0, pos[2]], scale, w3, age: 0, R: 0, rs, ts, rMax, load,
      shook: false,
      // o anel acaba quando a frente passa de onde o vento ainda levanta poeira
      life: ts[ts.length - 1] + 0.2,
    });
  }

  /** raio da frente no instante age (tabela; além dela, som) */
  _front(n) {
    const ts = n.ts, last = ts.length - 1;
    if (n.age >= ts[last]) return n.rs[last] + C0 * (n.age - ts[last]);
    let lo = 0, hi = last;
    while (hi - lo > 1) { const m = (lo + hi) >> 1; if (ts[m] <= n.age) lo = m; else hi = m; }
    const a = (n.age - ts[lo]) / Math.max(ts[hi] - ts[lo], 1e-9);
    return n.rs[lo] + (n.rs[hi] - n.rs[lo]) * a;
  }

  /**
   * Avança no tempo de jogo. Quando a frente passa pela câmera, o tranco: a
   * luz chegou antes (o clarão é instantâneo), o choque leva d/c — ~1 s a
   * 350 m. A força vem da sobrepressão que chega até ela.
   */
  update(dt, cam) {
    for (const n of this.list) {
      n.age += dt;
      n.R = this._front(n);
      if (!n.shook) {
        const d = Math.hypot(cam.pos[0] - n.pos[0], cam.pos[1] - n.pos[1], cam.pos[2] - n.pos[2]);
        if (n.R >= d) {
          n.shook = true;
          const dp = overpressure(Math.max(d, 1) / n.w3);
          // ~0.8 psi (Δp/p0 = 0.055) a ~400 m: quebra vidro, sacode a câmera
          // no tripé. Amplitude ANGULAR: com a câmera a centenas de metros o
          // kick de posição (centímetros) não se veria.
          cam.quake(0.024 * Math.sqrt(Math.min(dp / 0.05, 1)));
          cam.kick(Math.min(1.5, 0.6 + 12 * dp));
        } else if (n.R > 6000) n.shook = true;   // longe demais pra sentir
      }
    }
    this.list = this.list.filter((n) => n.age < n.life || !n.shook);
  }

  /** altura da parede (cresce enquanto corre) e o teto da fatia marchada */
  _h0(n) { return this.params.h0 * (1 + this.params.rise * n.age); }
  _top(n) { return this._h0(n) * 1.75; }   // topo dos rolos: até ~1.7 × h0

  /** caixa do anel no mundo (pro recorte em tela) */
  _box(n) {
    const Rb = Math.min(n.R + 6, n.rMax), top = this._top(n);
    return { min: [n.pos[0] - Rb, 0, n.pos[2] - Rb], max: [n.pos[0] + Rb, top, n.pos[2] + Rb] };
  }

  _rect(box, cam, w, h) {
    const m = cam.viewProj;
    let x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
    for (let c = 0; c < 8; c++) {
      const p = [c & 1 ? box.max[0] : box.min[0], c & 2 ? box.max[1] : box.min[1], c & 4 ? box.max[2] : box.min[2]];
      const cw = m[3] * p[0] + m[7] * p[1] + m[11] * p[2] + m[15];
      if (cw <= 1e-4) return [0, 0, w, h];   // atravessa a câmera: tela toda
      const cx = (m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12]) / cw;
      const cy = (m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13]) / cw;
      x0 = Math.min(x0, cx); x1 = Math.max(x1, cx); y0 = Math.min(y0, cy); y1 = Math.max(y1, cy);
    }
    const px0 = Math.max(0, Math.floor((x0 * 0.5 + 0.5) * w) - 2);
    const px1 = Math.min(w, Math.ceil((x1 * 0.5 + 0.5) * w) + 2);
    const py0 = Math.max(0, Math.floor((y0 * 0.5 + 0.5) * h) - 2);
    const py1 = Math.min(h, Math.ceil((y1 * 0.5 + 0.5) * h) + 2);
    if (px1 <= px0 || py1 <= py0) return null;
    return [px0, py0, px1 - px0, py1 - py0];
  }

  /**
   * Anel de poeira no alvo do volume (meia resolução), DEPOIS das explosões
   * e da fumaça do campo: somado por TRÁS do que já está lá. Certo onde o
   * anel está atrás da coluna; o trecho na frente dela fica escondido atrás
   * da base, que é bola de fogo opaca nesse instante — erro pequeno, e evita
   * a separação por profundidade que a fumaça do campo faz.
   * @param VP parâmetros das instâncias (mesma fase e espalhamento múltiplo)
   */
  drawDust(cam, env, volTarget, halfDepth, noiseTex, lights, VP) {
    const live = this.list.filter((n) => n.age < n.life && n.age > 0);
    if (!live.length) return;
    const gl = this.gl, P = this.params, w = volTarget.w, h = volTarget.h;
    const n8 = Math.min(lights.length, MAXL);
    const lp = new Float32Array(MAXL * 4), lc = new Float32Array(MAXL * 3);
    for (let i = 0; i < n8; i++) {
      const L = lights[i];
      lp.set([L.pos[0], L.pos[1], L.pos[2], L.s2 ?? 1], i * 4);
      lc.set(L.color, i * 3);
    }
    volTarget.bind();
    gl.drawBuffers([gl.COLOR_ATTACHMENT0, gl.NONE]);
    gl.enable(gl.BLEND);
    gl.blendFuncSeparate(gl.DST_ALPHA, gl.ONE, gl.ZERO, gl.SRC_ALPHA);
    gl.enable(gl.SCISSOR_TEST);
    const sh = this.shDust.use();
    sh.set('uInvViewProj', cam.invViewProj).set('uCamPos', cam.pos).set('uKeyDir', env.keyDir)
      .set('uNear', cam.near).set('uJitter', env.frameJitter)
      .set('uSigma0', P.sigma0).set('uBand', P.band)
      .set('uDustColor', VP.dustColor).set('uDustAlbedo', VP.dustAlbedo)
      .set('uPhaseG', VP.phaseG).set('uPhaseBack', VP.phaseBack).set('uPhaseMix', VP.phaseMix)
      .set('uSkyGain', VP.skyGain)
      .set('uMsExt', VP.msExt).set('uMsScatter', VP.msScatter).set('uMsPhase', VP.msPhase);
    sh.seti('uSteps', P.steps).seti('uLCount', n8);
    gl.uniform4fv(sh.loc('uLP[0]'), lp);
    gl.uniform3fv(sh.loc('uLC[0]'), lc);
    sh.tex('uEnvLut', env.envLut).tex('uHalfDepth', halfDepth).tex('uNoise', noiseTex, gl.TEXTURE_3D);
    for (const n of live) {
      const r = this._rect(this._box(n), cam, w, h);
      if (!r) continue;
      gl.scissor(r[0], r[1], r[2], r[3]);
      sh.set('uGZ', n.pos).set('uAge', n.age).set('uR', n.R).set('uRMax', n.rMax)
        .set('uRIn', Math.max(n.R - 6 * P.band, 0))
        .set('uH0', this._h0(n)).set('uTop', this._top(n));
      gl.uniform1fv(sh.loc('uLoad[0]'), n.load);
      drawFS(gl);
    }
    gl.disable(gl.SCISSOR_TEST);
    gl.disable(gl.BLEND);
    gl.drawBuffers([gl.COLOR_ATTACHMENT0, gl.COLOR_ATTACHMENT1]);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }

  /**
   * Ar iluminado pela bomba, somado na imagem composta (antes da exposição
   * automática, que assim o mede junto). Só as luzes marcadas nuclear.
   */
  drawAir(cam, env, compT, depthTex, volTarget, lights) {
    const nuke = lights.filter((L) => L.nuclear && L.power > 1e-3).slice(0, 2);
    if (!nuke.length) return;
    const gl = this.gl, P = this.params;
    const lp = new Float32Array(8), lc = new Float32Array(6);
    nuke.forEach((L, i) => { lp.set([L.pos[0], L.pos[1], L.pos[2], L.s2 ?? 1], i * 4); lc.set(L.color, i * 3); });
    compT.bind();
    gl.disable(gl.DEPTH_TEST);
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.ONE, gl.ONE);
    const sh = this.shAir.use();
    sh.set('uInvViewProj', cam.invViewProj).set('uCamPos', cam.pos)
      .set('uFogDensity', env.fogDensity).set('uFogFalloff', env.fogFalloff)
      .set('uJitter', env.frameJitter).set('uTop', P.airTop).set('uRange', P.airRange).set('uG', P.airG);
    sh.seti('uSteps', P.airSteps).seti('uLCount', nuke.length);
    gl.uniform4fv(sh.loc('uLP[0]'), lp);
    gl.uniform3fv(sh.loc('uLC[0]'), lc);
    sh.tex('uDepth', depthTex).tex('uVolC', volTarget.texs[0]).tex('uVolAux', volTarget.texs[1]);
    drawFS(gl);
    gl.disable(gl.BLEND);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }
}
