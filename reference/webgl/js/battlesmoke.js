// ---------------------------------------------------------------------------
// battlesmoke.js — a fumaça que FICA no campo de batalha.
//
// Cada explosão do clique é uma sequência assada de ~6 s numa caixa de 38 m;
// a fumaça sobe e sai pelo teto dela (91% da massa já está na faixa do teto
// aos 4.3 s, medido), e some. Na vida real ela fica pairando e se espalhando
// por minutos — e num RTS é isso que faz o mapa parecer um campo de batalha.
//
// Aqui um fluido barato (Stam 1999, "Stable Fluids") cobre o campo, em duas
// resoluções — o truque clássico pra ter detalhe barato, porque a velocidade
// é suave e a fumaça não:
//   - VELOCIDADE, pressão e luz numa grade grossa (2 m), deslocada (MAC):
//     advecção semi-Lagrangiana, empuxo do calor que sobrou, turbulência
//     atmosférica, projeção por Jacobi;
//   - FUMAÇA numa grade fina (1 m), transportada por volumes finitos pela
//     velocidade grossa interpolada nas faces finas.
// A explosão entrega a fumaça numa janela depois da fase de fogo (densidade,
// calor e a VELOCIDADE assada — o movimento continua), e some com a mesma
// curva: a soma das duas fica constante e não há estalo. Depois disso a
// instância não é mais marchada: o custo da fumaça tardia vira um custo fixo,
// independente de quantas explosões houve.
//
// Como guarda velocidade de verdade, a grade REAGE ao que passa por ela
// (como o Niagara Fluids da Unreal e as granadas do Counter-Strike 2):
//   - corpo (avião, míssil): o ar é carregado junto e a projeção transforma
//     isso no escoamento em volta dele;
//   - esteira de asa: par de vórtices de Lamb–Oseen nas pontas, circulação
//     Γ = W/(ρ·U·b0), b0 = π/4·envergadura (asa elíptica);
//   - projétil: a onda de choque e a esteira turbulenta abrem um túnel que a
//     própria turbulência fecha (modelo de sub-grade: o tubo real é menor
//     que a célula);
//   - explosão: fonte de divergência (expansão) — a projeção converte numa
//     onda radial que empurra a fumaça antiga pra fora;
//   - escapamento de míssil: jato pra trás e fumaça própria (canal claro).
// ---------------------------------------------------------------------------

import { Shader, Target, PingPong, drawFS, FS_VS } from './gl.js';
import { COMMON, ENVLUT } from './glsl.js';
import { VolumeGrid } from './volume.js';
import { SOOT_SCALE, TEMP_SCALE, DUST_SCALE } from './bake.js';

const F = (x) => (Number.isInteger(x) ? x.toFixed(1) : String(x));

/** grade de caixa (não cúbica) com a mesma API GLSL da VolumeGrid */
class BoxGrid extends VolumeGrid {
  constructor(dims, cell, min) {
    super(1, 1);
    const [nx, ny, nz] = dims;
    this.nx = nx; this.ny = ny; this.nz = nz;
    let best = null;
    for (let tx = 1; tx <= nz; tx++) {
      const ty = Math.ceil(nz / tx);
      const w = tx * nx, h = ty * ny;
      const score = Math.abs(Math.log(w / h)) + (tx * ty - nz) * 0.01;
      if (!best || score < best.score) best = { tx, ty, w, h, score };
    }
    this.tilesX = best.tx; this.tilesY = best.ty;
    this.atlasW = best.w; this.atlasH = best.h;
    this.domainSize = [nx * cell, ny * cell, nz * cell];
    this.domainMin = min.slice();
    this.cell = cell;
  }
  // CELL inteiro (1 m, 2 m) sairia como literal int e quebraria as contas em GLSL
  glsl(s = '') {
    return super.glsl(s)
      .replace(new RegExp(`#define CELL${s} .*`), `#define CELL${s} ${F(this.cell)}`)
      .replace(new RegExp(`#define INV_CELL${s} .*`), `#define INV_CELL${s} ${F(1 / this.cell)}`);
  }
}

export const MAX_DISTURB = 8;
// fumaça de propelente sólido (óxido de alumínio): branca, albedo ~0.95
const EXH_COLOR = [0.9, 0.9, 0.88];
const EXH_ALBEDO = 0.93;

export const BATTLE_DEFAULTS = {
  // ---- simulação ----
  pressureIters: 24,
  // Empuxo menor que o do solver da explosão (38) porque o calor aqui dura
  // mais (coolTau abaixo): o mesmo impulso total, e a nuvem para na mesma
  // altura (~35 m, medido).
  buoyancy: 13.0,      // m/s² por unidade de T
  sootWeight: 0.6,
  // A temperatura média da fumaça na gravação cai com ~5 s de constante
  // entre 3 e 4.5 s (medido: 0.244 → 0.193). Com 1.2 s aqui o brilho morria
  // logo depois da troca, enquanto a explosão gravada ainda brilhava.
  coolTau: 5.0,        // s
  // Quanto a fumaça DURA é decisão de jogo, não de física: numa câmera de
  // RTS a nuvem fica entre a câmera e as unidades. Sem vento, a fumaça de
  // verdade pairaria por minutos (era assim, com 70 s); aqui ela some como
  // nos jogos do gênero, em ~15 s depois da explosão. O jeito de sumir é o
  // natural: dissipação (Houdini/EmberGen chamam assim) + a erosão do ruído,
  // que come primeiro o que está ralo — a nuvem se desfaz pelas bordas e em
  // fiapos em vez de esmaecer por igual.
  sootTau: 10.0,       // s — dissipação: densa até ~10 s, fiapos até ~15 s
  erodeAbs: 1.0,       // 1 = erosão absoluta (desfaz o ralo primeiro) · 0 = na escala do lugar
  idleMass: 0.15,      // abaixo disso (em explosões entregues) o campo é zerado e para de custar
  // Uma térmica desacelera ao misturar ar (Morton–Taylor–Turner: w ∝ t^-1/2):
  // a subida herdada da explosão morre rápido e a nuvem para e paira. O resto
  // do movimento (redemoinhos de avião, turbulência) dura bem mais.
  riseTau: 1.2,        // s — desaceleração da subida da PLUMA (canal próprio)
  // Perda de força do escoamento. Com 6 s o par de vórtices de uma asa
  // guardava 36% da circulação aos 8 s; com 15 s, 69% (medido) — os reais
  // duram dezenas de segundos. A forçante da turbulência caiu à metade pra
  // manter a mesma agitação de fundo (σᵤ ≈ 0.4 m/s).
  velTau: 15.0,
  // Turbulência atmosférica calibrada pela dispersão de um sopro de fumaça
  // em atmosfera neutra (Pasquill–Gifford classe D, fórmulas de Briggs pra
  // terreno aberto): de σ ≈ 7 m na entrega a ~15 m em 60 s, ou seja K ≈ 1.5–
  // 2.5 m²/s ≈ σᵤ²·T_L com σᵤ ≈ 0.4 m/s. Medido: σ horizontal 6.5 → 8.5 →
  // 10 → 12.4 m em 4 / 10 / 20 / 40 s (K ≈ 1.4 m²/s), σᵤ ≈ 0.4 m/s, centro
  // da nuvem parado (< 2.5 m). Os redemoinhos têm o tamanho da nuvem
  // (~10 m): MAIORES que ela a carregam inteira (vira vento, 10 m de deriva
  // em 40 s), MENORES só a reviram no lugar (σ parado em 7.5 m).
  turb: 3.0,           // m/s² de forçante
  turbScale: 0.025,    // 1/m: o ruído curl tem 4 células por unidade → ~10 m
  turbRate: 0.03,      // 1/s em coordenada de ruído: renovação ~8 s
  turbV: 0.5,          // forçante vertical / horizontal (σ_w ≈ 0.5·σ_u)
  vorticity: 0.5,      // ε do vorticity confinement
  // ---- passagem da explosão pra cá ----
  // De UMA VEZ, num quadro, antes de a fumaça bater no teto da caixa
  // assada (dali ela é cortada e esmaecida, ~3.6 s). Com uma janela de
  // mistura as duas versões ficavam visíveis no mesmo lugar, e duas nuvens
  // que se interpenetram só podem ser compostas uma inteira na frente da
  // outra: a cópia na grade tapava o brilho da gravação — à noite o fogo
  // apagava de repente e reacendia (relatado pelo usuário, reproduzido).
  // Entregue de uma vez, a grade já fica quase idêntica à gravação (mesmo
  // sombreamento, mesmo detalhe, e o calor esfriando no mesmo ritmo).
  handoffT0: 3.3,      // s de sequência
  handoffT1: 3.3,      // = T0: troca instantânea
  earlyHandoff: 2.8,   // s — a partir daqui algo que atravessa a explosão a passa antes
  // ---- render ----
  steps: 120,          // amostras dentro da fumaça por raio (passo de 1 m)
  // detalhe: o das explosões (0.42 m / 0.85 por m numa grade de ~0.6 m)
  // levado à grade de 1 m; erosão um pouco menor (0.042 lá), na escala da
  // densidade que se dilui com o tempo
  detailAmp: 0.6,      // m
  detailScale: 0.45,   // 1/m (ruído curl de 4 células por unidade → ~0.55 m)
  detailDens: 0.34,
  erode: 0.025,        // erosão pelo ruído (bordas recortadas; desfaz o ralo primeiro)
  flowPeriod: 0.8,     // s — mais longo estica o detalhe na subida da entrega
  lightEvery: 2,       // atualiza o volume de luz a cada N quadros
};

export class BattleSmoke {
  /**
   * @param bake      ExplosionBake (a fonte da fumaça entregue)
   * @param noiseTex  ruído 3D tileável do projeto
   * @param bbTex     LUT de corpo negro
   */
  constructor(gl, bake, noiseTex, bbTex, opts = {}) {
    this.gl = gl;
    this.bake = bake;
    this.noiseTex = noiseTex;
    this.bbTex = bbTex;
    this.params = { ...BATTLE_DEFAULTS, ...opts };
    // 96 × 64 × 96 m centrado na origem. Fumaça a 1 m: com 2 m a nuvem
    // entregue virava uma bolha (os lóbulos que a explosão desenha com células
    // de ~0.6 m não cabiam) e a troca aparecia. Velocidade a 2 m: ela é suave,
    // e a projeção (24 Jacobi) e a luz custam 8× menos.
    const min = [-48, 0, -48];
    this.grid = new BoxGrid([96, 64, 96], 1.0, min);    // fumaça (590 mil células)
    this.vgrid = new BoxGrid([48, 32, 48], 2.0, min);   // velocidade · pressão · luz
    this.macroGrid = new BoxGrid([24, 16, 24], 4.0, min);
    const G = this.grid, V = this.vgrid, M = this.macroGrid;
    const f16 = { internalFormat: gl.RGBA16F, format: gl.RGBA, type: gl.HALF_FLOAT };
    // Fumaça em float32 quando dá pra filtrar: o half-float zera os
    // subnormais (< 6e-5) ao gravar em várias GPUs (Apple), e a borda rarefeita
    // que o transporte espalha era truncada a cada passo.
    const f32 = gl.getExtension('OES_texture_float_linear')
      ? { internalFormat: gl.RGBA32F, format: gl.RGBA, type: gl.FLOAT } : f16;
    const r16 = { internalFormat: gl.R16F, format: gl.RED, type: gl.HALF_FLOAT, filter: gl.NEAREST };
    this.fld = new PingPong(gl, G.atlasW, G.atlasH, f32);   // r fuligem · g calor · b poeira · a fumaça de motor
    this.vel = new PingPong(gl, V.atlasW, V.atlasH, f16);   // faces +½ (MAC) · a = pluma na face y
    this.div = new Target(gl, V.atlasW, V.atlasH, r16);
    this.prs = new PingPong(gl, V.atlasW, V.atlasH, r16);
    this.curl = new Target(gl, V.atlasW, V.atlasH, { ...f16, filter: gl.NEAREST });
    this.light = new Target(gl, V.atlasW, V.atlasH,
      { internalFormat: gl.RGBA8, format: gl.RGBA, type: gl.UNSIGNED_BYTE });
    this.macro = new Target(gl, M.atlasW, M.atlasH,
      { internalFormat: gl.R16F, format: gl.RED, type: gl.HALF_FLOAT, filter: gl.NEAREST });
    for (const t of [this.vel.a, this.vel.b, this.fld.a, this.fld.b, this.prs.a, this.prs.b]) t.bind(true);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    this.disturbers = [];   // [{kind, pos, prev, radius, vel, strength, right, span}]
    this.blasts = [];       // [{pos, radius, src, until}]
    this.time = 0;
    this.frame = 0;
    this.massEstimate = 0;  // fumaça no campo, em explosões entregues (cai com a dissipação)
    this.idle = false;      // campo vazio: passo e render pulados
    this._build();
  }

  _build() {
    const gl = this.gl, G = this.grid, V = this.vgrid, M = this.macroGrid, B = this.bake;
    const pre = `#version 300 es\nprecision highp float;\nprecision highp sampler2DArray;\nprecision highp sampler3D;\nin vec2 vUV;\n`
      + COMMON;
    // Passes da VELOCIDADE: a grade grossa sem sufixo, a da fumaça como 'D'.
    // Passes da FUMAÇA: a fina sem sufixo, a da velocidade como 'V'.
    const headV = pre + V.glsl() + G.glsl('D');
    const headD = pre + G.glsl() + V.glsl('V');
    // Grade DESLOCADA (MAC, Harlow & Welch 1965): o texel (i,j,k) guarda
    // u na face i+½, v na face j+½ e w na face k+½. Com divergência, gradiente
    // e o Laplaciano da pressão todos compactos, a projeção zera a divergência
    // célula a célula. Na grade colocada com diferença central ela só zerava a
    // divergência "larga": medido 0.002/s nela contra 0.08/s célula a célula
    // (75% da escala do gradiente) — e a advecção sumia com 26% da fumaça em 4 s.
    // O canal a guarda a SUBIDA PRÓPRIA DA PLUMA (na face y): o empuxo do
    // calor e a subida herdada da explosão, que desaceleram como uma térmica
    // que mistura ar (riseTau). O resto (xyz) é o escoamento: turbulência,
    // esteiras, jatos — esse só perde força como ar de verdade. Com um freio
    // único na vertical, o par de vórtices de uma asa (metade da rotação é
    // vertical) morria em 1 s e o ar parecia "duro".
    const MAC = `
// velocidade física TOTAL no ponto p (coordenada de voxel, centro da célula
// em i+½): escoamento nas faces + a pluma somada na vertical
vec3 velAt(sampler2D vt, vec3 p){
  vec4 sy = sampleVol(vt, p - vec3(0.0, 0.5, 0.0));
  return vec3(sampleVol(vt, p - vec3(0.5, 0.0, 0.0)).x, sy.y + sy.a,
              sampleVol(vt, p - vec3(0.0, 0.0, 0.5)).z);
}
`;
    // velocidade da grade grossa vista da fina: uma componente na posição de
    // MUNDO w (a face u fica meia célula grossa antes do centro do texel)
    const VELV = `
float velCompV(sampler2D vt, vec3 w, int c){
  vec3 o = vec3(0.0);
  o[c] = 0.5;
  vec4 s = sampleVolV(vt, worldToVoxelV(w) - o);
  return c == 1 ? s.y + s.a : s[c];
}
`;
    const mkV = (name, body, extra = '') => new Shader(gl, FS_VS, headV + MAC + extra + body, 'battle-' + name);
    const mkD = (name, body, extra = '') => new Shader(gl, FS_VS, headD + VELV + extra + body, 'battle-' + name);

    // ---- advecção (velocidade e campos) --------------------------------
    // Campos por VOLUMES FINITOS: o que sai de uma célula entra na vizinha,
    // então a fumaça se conserva exatamente. A semi-Lagrangiana perdia 16% da
    // massa em 4 s mesmo com o campo sem divergência (e mais 7% nos
    // subnormais do half-float). A velocidade de cada face FINA é a grossa
    // interpolada no centro dela: as duas células que dividem a face leem o
    // mesmo valor, e o fluxo fecha.
    // O valor na face é reconstruído com inclinação limitada (MUSCL, van
    // Leer 1979, limitador "monotonized central"): o upwind puro difunde
    // ~|u|·h/2 — com a subida de 5–10 m/s herdada da explosão eram 3–5 m²/s,
    // e a nuvem virava uma bolha borrada no primeiro segundo depois da
    // entrega. De 2ª ordem onde o campo é suave, cai pro upwind nos
    // extremos (sem oscilação nem densidade negativa). Estável com
    // |u|·Δt/h < ⅓ por eixo (daí os subpassos).
    this.shAdvect = mkD('advect', `
uniform sampler2D uVel, uSrc;
uniform float uDt;
uniform vec4 uKeep;       // fator por canal neste passo (decaimento)
out vec4 oCol;
// densidade fora da grade: ar limpo pelas faces abertas
vec4 rho(ivec3 v){
  if (any(lessThan(v, ivec3(0))) || any(greaterThanEqual(v, GRIDI))) return vec4(0.0);
  return fetchVol(uSrc, v);
}
vec4 mcSlope(vec4 a, vec4 b){
  return sign(a) * min(min(2.0 * abs(a), 2.0 * abs(b)), 0.5 * abs(a + b)) * step(0.0, a * b);
}
// fluxo pela face entre as células L e R (LL antes de L, RR depois de R),
// velocidade normal un; o valor na face vem da célula a montante + inclinação
vec4 flux(float un, vec4 LL, vec4 L, vec4 R, vec4 RR){
  float k = 0.5 * (1.0 - abs(un) * uDt * INV_CELL);
  return un * (un > 0.0 ? L + k * mcSlope(L - LL, R - L)
                        : R - k * mcSlope(R - L, RR - R));
}
// velocidade normal na face fina centrada em pf (coordenada de voxel fino)
float uN(vec3 pf, int c){ return velCompV(uVel, voxelToWorld(pf), c); }
void main(){
  vec3 p = fragToVoxel(gl_FragCoord.xy);
  ivec3 v = ivec3(p);
  vec4 c = fetchVol(uSrc, v);
  vec4 xp = rho(v + ivec3(1,0,0)), xm = rho(v - ivec3(1,0,0));
  vec4 yp = rho(v + ivec3(0,1,0)), ym = rho(v - ivec3(0,1,0));
  vec4 zp = rho(v + ivec3(0,0,1)), zm = rho(v - ivec3(0,0,1));
  // ar limpo em volta (a maior parte do campo): pula as 12 leituras da
  // velocidade. O limiar corta a franja que a difusão numérica espalharia
  // pela caixa inteira com valores ínfimos — ~1e-6 de 0.01 visível.
  vec4 any7 = c + xp + xm + yp + ym + zp + zm;
  if (any7.r + any7.g + any7.b + any7.a < 1e-6){ oCol = vec4(0.0); return; }
  float uxp = uN(p + vec3(0.5, 0.0, 0.0), 0), uxm = uN(p - vec3(0.5, 0.0, 0.0), 0);
  float uyp = uN(p + vec3(0.0, 0.5, 0.0), 1);
  float uym = v.y > 0 ? uN(p - vec3(0.0, 0.5, 0.0), 1) : 0.0;     // chão sólido
  float uzp = uN(p + vec3(0.0, 0.0, 0.5), 2), uzm = uN(p - vec3(0.0, 0.0, 0.5), 2);
  vec4 xp2 = rho(v + ivec3(2,0,0)), xm2 = rho(v - ivec3(2,0,0));
  vec4 yp2 = rho(v + ivec3(0,2,0)), ym2 = rho(v - ivec3(0,2,0));
  vec4 zp2 = rho(v + ivec3(0,0,2)), zm2 = rho(v - ivec3(0,0,2));
  vec4 net = flux(uxp, xm, c, xp, xp2) - flux(uxm, xm2, xm, c, xp)
           + flux(uyp, ym, c, yp, yp2) - flux(uym, ym2, ym, c, yp)
           + flux(uzp, zm, c, zp, zp2) - flux(uzm, zm2, zm, c, zp);
  oCol = max(c - net * (uDt * INV_CELL), vec4(0.0)) * uKeep;
}`);
    // Cada componente é advectada a partir da PRÓPRIA face (a pluma junto com
    // a vertical). MacCormack na forma de uma passada (Selle et al. 2008, a
    // mesma da simulação ao vivo): a semi-Lagrangiana pura numa grade de 2 m
    // borrava a velocidade a cada passo — medido, um vórtice de asa sem freio
    // nenhum ainda perdia metade da força em 2 s.
    this.shAdvectVel = mkV('advectvel', `
uniform sampler2D uVel;
uniform float uDt;
out vec4 oCol;
// componente c no ponto físico p, com a pluma (que mora na face y)
vec2 compAt(vec3 p, int c){
  vec3 o = vec3(0.0); o[c] = 0.5;
  vec4 s = sampleVol(uVel, p - o);
  return vec2(s[c], s.a);
}
vec2 advectFace(vec3 pf, int c){
  vec3 back = pf - velAt(uVel, pf) * (uDt * INV_CELL);
  vec2 hat = compAt(back, c);
  // ida e volta a partir do ponto de origem: estimativa do erro
  vec3 fwd = back + velAt(uVel, back) * (uDt * INV_CELL);
  vec2 v = hat + 0.5 * (compAt(pf, c) - compAt(fwd, c));
  // limitador: dentro do envelope dos 8 valores que geraram 'hat'
  vec3 o = vec3(0.0); o[c] = 0.5;
  ivec3 b = ivec3(floor(back - o - 0.5));
  vec2 mn = vec2(1e20), mx = vec2(-1e20);
  for (int k = 0; k < 8; k++){
    vec4 s = fetchVol(uVel, b + ivec3(k & 1, (k >> 1) & 1, (k >> 2) & 1));
    vec2 q = vec2(s[c], s.a);
    mn = min(mn, q); mx = max(mx, q);
  }
  return clamp(v, mn, mx);
}
void main(){
  vec3 p = fragToVoxel(gl_FragCoord.xy);
  vec2 vy = advectFace(p + vec3(0.0, 0.5, 0.0), 1);
  oCol = vec4(advectFace(p + vec3(0.5, 0.0, 0.0), 0).x, vy.x,
              advectFace(p + vec3(0.0, 0.0, 0.5), 2).x, vy.y);
}`);

    // vorticidade no centro de cada célula (pro confinement): xyz = ω, w = |ω|
    this.shCurl = mkV('curl', `
uniform sampler2D uVel;
out vec4 oCol;
// velocidade total no centro da célula v: média das duas faces de cada eixo
vec3 cellVel(ivec3 v){
  vec4 c = fetchVol(uVel, v), ym = fetchVol(uVel, v - ivec3(0, 1, 0));
  return 0.5 * vec3(c.x + fetchVol(uVel, v - ivec3(1, 0, 0)).x,
                    c.y + c.a + ym.y + ym.a,
                    c.z + fetchVol(uVel, v - ivec3(0, 0, 1)).z);
}
void main(){
  ivec3 v = ivec3(fragToVoxel(gl_FragCoord.xy));
  vec3 xp = cellVel(v + ivec3(1,0,0)), xm = cellVel(v - ivec3(1,0,0));
  vec3 yp = cellVel(v + ivec3(0,1,0)), ym = cellVel(v - ivec3(0,1,0));
  vec3 zp = cellVel(v + ivec3(0,0,1)), zm = cellVel(v - ivec3(0,0,1));
  vec3 w = 0.5 * INV_CELL * vec3((yp.z - ym.z) - (zp.y - zm.y),
                                 (zp.x - zm.x) - (xp.z - xm.z),
                                 (xp.y - xm.y) - (yp.x - ym.x));
  oCol = vec4(w, length(w));
}`);

    // ---- forças: empuxo, turbulência, objetos passando --------------------
    const DIST = `
#define MAXD ${MAX_DISTURB}
uniform int uDCount;
uniform vec4 uDA[MAXD];   // posição atual.xyz, raio (m)
uniform vec4 uDB[MAXD];   // posição anterior.xyz, tipo (1 corpo · 2 projétil · 4 esteira de asa · 5 fumaça de motor · 6 jato)
uniform vec4 uDC[MAXD];   // velocidade do objeto.xyz (m/s), intensidade / circulação / (empuxo / velocidade / ρ) do jato
uniform vec4 uDD[MAXD];   // eixo "direita" das asas.xyz, envergadura efetiva b0 (m) · no jato: x = velocidade de saída, w = diâmetro do bocal
float segDist(vec3 w, vec3 a, vec3 b, out vec3 c){
  vec3 ab = b - a;
  float l2 = dot(ab, ab);
  float s = l2 > 1e-6 ? clamp(dot(w - a, ab) / l2, 0.0, 1.0) : 0.0;
  c = a + ab * s;
  return length(w - c);
}
// vórtice de Lamb–Oseen em volta do eixo dir que passa por o
vec3 lambOseen(vec3 w, vec3 o, vec3 dir, float gamma, float rc){
  vec3 r = w - o;
  r -= dir * dot(r, dir);
  float rr = length(r);
  if (rr < 1e-3) return vec3(0.0);
  float ut = gamma / (6.2831853 * rr) * (1.0 - exp(-rr * rr / (rc * rc)));
  return cross(dir, r / rr) * ut;
}
`;
    this.shForces = mkV('forces', `
uniform sampler2D uVel, uFld, uCurl;
uniform sampler3D uNoise;
uniform float uDt, uTime, uBuoy, uSootW, uTurb, uTurbScale, uTurbRate, uTurbV;
uniform float uVelKeep, uRiseKeep, uUMax, uVort;
out vec4 oCol;
// Vorticity confinement (Fedkiw, Stam & Jensen 2001) no centro da célula v:
// f = ε·h·(N × ω), N = ∇|ω| / |∇|ω||. Devolve a rotação que a advecção numa
// grade de 2 m apaga.
vec3 confine(ivec3 v){
  vec3 wc = fetchVol(uCurl, v).xyz;
  vec3 eta = vec3(fetchVol(uCurl, v + ivec3(1,0,0)).w - fetchVol(uCurl, v - ivec3(1,0,0)).w,
                  fetchVol(uCurl, v + ivec3(0,1,0)).w - fetchVol(uCurl, v - ivec3(0,1,0)).w,
                  fetchVol(uCurl, v + ivec3(0,0,1)).w - fetchVol(uCurl, v - ivec3(0,0,1)).w);
  return uVort * CELL * cross(eta / (length(eta) + 1e-6), wc);
}
// velocidade nova na face c (posição pf, entre as células v e v + e_c):
// xyz = escoamento, w = pluma (só a face y usa)
vec4 forced(vec3 pf, ivec3 v, int c){
  vec4 sy = sampleVol(uVel, pf - vec3(0.0, 0.5, 0.0));
  vec3 um = vec3(sampleVol(uVel, pf - vec3(0.5, 0.0, 0.0)).x, sy.y,
                 sampleVol(uVel, pf - vec3(0.0, 0.0, 0.5)).z);
  float pl = sy.a;
  vec3 w = voxelToWorld(pf);
  vec4 f = sampleVolD(uFld, worldToVoxelD(w));

  // empuxo: o calor que sobrou da explosão sobe (é a pluma); fuligem pesa
  pl += (uBuoy * f.g - uSootW * f.r) * uDt;

  // turbulência atmosférica: turbilhões de ~10 m que mexem e espalham a
  // nuvem (a difusão turbulenta que dilui a fumaça), mais forte onde há
  // fumaça. A vertical é metade da horizontal: perto do chão, em atmosfera
  // neutra, σ_w ≈ 0.5·σ_u (Panofsky & Dutton 1984). Antes o freio vertical
  // fazia esse papel — e matava os vórtices junto.
  float act = smoothstep(0.002, 0.05, f.r + f.b + f.a);
  vec3 np = w * uTurbScale + vec3(0.6, -1.0, 0.4) * (uTime * uTurbRate);
  vec3 turb = texture(uNoise, np).xyz * 2.0 - 1.0;
  vec3 turb2 = texture(uNoise, np * 2.3 + 0.41).xyz * 2.0 - 1.0;
  um += (turb + 0.5 * turb2) * vec3(1.0, uTurbV, 1.0) * uTurb * (0.35 + 0.65 * act) * uDt;

  if (uVort > 0.0){
    ivec3 e = ivec3(0); e[c] = 1;
    um += 0.5 * (confine(v) + confine(v + e)) * uDt;
  }

  for (int i = 0; i < MAXD; i++){
    if (i >= uDCount) break;
    vec4 A = uDA[i], B = uDB[i], C = uDC[i];
    vec3 cp;
    float d = segDist(w, B.xyz, A.xyz, cp);
    int kind = int(B.w + 0.5);
    vec3 seg = A.xyz - B.xyz;
    float len = length(seg);
    vec3 dir = len > 1e-4 ? seg / len : vec3(1.0, 0.0, 0.0);
    if (kind == 1){
      // corpo: o ar perto é arrastado na direção do voo (camada limite,
      // esteira) até uma fração C.w da velocidade dele; a projeção faz o
      // resto do escoamento contornar. Só a componente AO LONGO do voo: o
      // que gira em volta (esteira de asa) não é apagado.
      float k = smoothstep(A.w * 1.6, A.w * 0.5, d);
      um += dir * max(C.w * length(C.xyz) - dot(um, dir), 0.0) * k;
    } else if (kind == 4){
      // esteira de asa: dois vórtices de ponta de asa, sentidos opostos,
      // SOMADOS ao escoamento que já existe (a fumaça que subia continua
      // subindo, agora girando). Só na fatia que a asa cruzou NESTE quadro
      // (projeção dentro do trecho, sem as pontas): cada ponto recebe o par
      // uma vez só, e não a cada quadro em que o avião ainda está perto.
      vec4 D = uDD[i];
      float sAlong = len > 1e-4 ? dot(w - B.xyz, dir) / len : -1.0;
      float near = smoothstep(D.w * 1.4, D.w * 0.6, d);
      if (near > 0.0 && sAlong > 0.0 && sAlong <= 1.0){
        vec3 v1 = lambOseen(w, cp + D.xyz * (D.w * 0.5), dir,  C.w, A.w);
        vec3 v2 = lambOseen(w, cp - D.xyz * (D.w * 0.5), dir, -C.w, A.w);
        um += (v1 + v2) * near;
      }
    } else if (kind == 2){
      // projétil: a onda de choque e a esteira turbulenta reviram o ar num
      // tubo em volta da trajetória (sinal do redemoinho aleatório por trecho)
      float k = smoothstep(A.w * 2.0, A.w * 0.4, d) * C.w;
      if (k > 0.0){
        vec3 rnd = hash33(floor(cp * 0.6) + 17.0);   // já em [-1, 1]
        vec3 sw = lambOseen(w, cp, dir, (rnd.x > 0.0 ? 1.0 : -1.0) * 30.0, A.w);
        um += (sw + rnd * 7.0) * k;
      }
    } else if (kind == 6){
      // Jato de motor (trecho do bocal A até o fim da região, B). A
      // meia-largura abre ~0.1·x (jato redondo turbulento, Pope §5). A
      // velocidade no eixo é a MENOR entre a de saída (núcleo potencial) e a
      // que a conservação de quantidade de movimento permite: por metro de
      // trajetória o motor deixa no ar empuxo/velocidade (N·s/m), espalhado
      // na seção do jato. Sem esse limite o jato de um avião soprava a nuvem
      // inteira pra trás a 38 m/s; com ele, ~9 m/s a 40 m (A-10, 80 kN).
      // A componente AO LONGO do eixo segue esse perfil — pra cima E pra
      // baixo: conforme o avião se afasta, o jato se abre e o ar que ele
      // soprou desacelera (a região vai até 150 m atrás, ~0.8 m/s). Só
      // somar deixava o ar correndo por segundos e a nuvem virava um risco
      // esticado. O que gira em volta não é tocado — um jato que
      // sobrescrevia a velocidade inteira apagava os vórtices das asas.
      float x = length(A.xyz - cp);
      float Dn = uDD[i].w;
      float b = 0.5 * Dn + 0.1 * x;
      float dU = min(uDD[i].x, C.w * 1.386 / (3.14159 * b * b));
      float k = exp(-0.693 * d * d / (b * b)) * step(d, 1.5 * b);
      float ax = dot(um, -dir);
      um -= dir * (mix(ax, dU, k) - ax);
    }
  }

  um *= uVelKeep;
  pl *= uRiseKeep * uVelKeep;
  // estabilidade do transporte da fumaça na grade fina (ver uUMax no step)
  um = clamp(um, vec3(-uUMax), vec3(uUMax));
  pl = clamp(pl, -uUMax - um.y, uUMax - um.y);
  return vec4(um, pl);
}
void main(){
  vec3 p = fragToVoxel(gl_FragCoord.xy);
  ivec3 v = ivec3(p);
  vec4 fy = forced(p + vec3(0.0, 0.5, 0.0), v, 1);
  oCol = vec4(forced(p + vec3(0.5, 0.0, 0.0), v, 0).x, fy.y,
              forced(p + vec3(0.0, 0.0, 0.5), v, 2).z, fy.w);
}`, DIST);

    // ---- campos: túnel de projétil e fumaça de escape ---------------------
    this.shFieldsFx = mkD('fieldsfx', `
uniform sampler2D uFld;
uniform float uDt;
out vec4 oCol;
void main(){
  vec3 p = fragToVoxel(gl_FragCoord.xy);
  vec4 f = fetchVol(uFld, ivec3(p));
  vec3 w = voxelToWorld(p);
  for (int i = 0; i < MAXD; i++){
    if (i >= uDCount) break;
    vec4 A = uDA[i], B = uDB[i], C = uDC[i];
    int kind = int(B.w + 0.5);
    vec3 c;
    float d = segDist(w, B.xyz, A.xyz, c);
    if (kind == 2){
      // o choque e a esteira turbulenta varrem um tubo de ~2 m logo atrás do
      // projétil: a fumaça de dentro é empurrada pra fora do caminho
      float k = smoothstep(A.w * 1.3, A.w * 0.3, d) * C.w * 0.95;
      f.rba *= 1.0 - k;
    } else if (kind == 1){
      // corpo: o volume que ele varre fica sem fumaça (ela é empurrada pra
      // fora, mais rápido do que a grade de velocidade de 2 m consegue mover)
      float k = smoothstep(A.w * 1.2, A.w * 0.3, d) * 0.7;
      f.rba *= 1.0 - k;
    } else if (kind == 5){
      // fumaça do propelente sólido (óxido de alumínio): canal próprio, branco
      float k = smoothstep(A.w * 1.4, A.w * 0.3, d);
      f.a += k * C.w * 1.6;
      f.g = max(f.g, k * 0.35);
    }
  }
  oCol = f;
}`, DIST);

    // ---- divergência (com as explosões como fonte de expansão) ------------
    this.shDiv = mkV('div', `
uniform sampler2D uVel;
uniform int uBCount;
uniform vec4 uBlast[4];   // posição.xyz, raio
uniform float uBSrc[4];   // fonte de divergência (1/s)
out vec4 oCol;
void main(){
  vec3 p = fragToVoxel(gl_FragCoord.xy);
  ivec3 v = ivec3(p);
  vec4 c4 = fetchVol(uVel, v);
  vec3 c = vec3(c4.x, c4.y + c4.a, c4.z);            // faces +½ desta célula (vertical total)
  // faces −½: o texel vizinho; nas faces abertas o ar de fora tem a mesma
  // velocidade (gradiente zero); a face do chão é sólida (0)
  float ux = v.x > 0 ? fetchVol(uVel, v - ivec3(1,0,0)).x : c.x;
  vec4 ym = fetchVol(uVel, v - ivec3(0,1,0));
  float uy = v.y > 0 ? ym.y + ym.a : 0.0;
  float uz = v.z > 0 ? fetchVol(uVel, v - ivec3(0,0,1)).z : c.z;
  float div = INV_CELL * ((c.x - ux) + (c.y - uy) + (c.z - uz));
  vec3 w = voxelToWorld(p);
  float src = 0.0;
  for (int i = 0; i < 4; i++){
    if (i >= uBCount) break;
    float r = length(w - uBlast[i].xyz);
    src += uBSrc[i] * smoothstep(uBlast[i].w, uBlast[i].w * 0.25, r);
  }
  oCol = vec4(div - src, 0.0, 0.0, 0.0);
}`);

    // ---- pressão (Jacobi): p = 0 nas faces abertas, Neumann no chão --------
    this.shPressure = mkV('pressure', `
uniform sampler2D uPress, uDiv;
out vec4 oCol;
float pAt(ivec3 v, float pc){
  if (v.y < 0) return pc;                                  // chão
  if (any(lessThan(v, ivec3(0))) || any(greaterThanEqual(v, GRIDI))) return 0.0;
  return fetchVol(uPress, v).x;
}
void main(){
  ivec3 v = ivec3(fragToVoxel(gl_FragCoord.xy));
  float pc = fetchVol(uPress, v).x;
  float s = pAt(v + ivec3(1,0,0), pc) + pAt(v - ivec3(1,0,0), pc)
          + pAt(v + ivec3(0,1,0), pc) + pAt(v - ivec3(0,1,0), pc)
          + pAt(v + ivec3(0,0,1), pc) + pAt(v - ivec3(0,0,1), pc);
  float d = fetchVol(uDiv, v).x;
  oCol = vec4((s - d * CELL * CELL) / 6.0, 0.0, 0.0, 0.0);
}`);

    this.shProject = mkV('project', `
uniform sampler2D uVel, uPress;
out vec4 oCol;
float pAt(ivec3 v, float pc){
  if (v.y < 0) return pc;
  if (any(lessThan(v, ivec3(0))) || any(greaterThanEqual(v, GRIDI))) return 0.0;
  return fetchVol(uPress, v).x;
}
void main(){
  ivec3 v = ivec3(fragToVoxel(gl_FragCoord.xy));
  float pc = fetchVol(uPress, v).x;
  // gradiente na face +½ de cada eixo: (p vizinho − p desta célula)/h
  vec3 grad = INV_CELL * vec3(
    pAt(v + ivec3(1,0,0), pc) - pc,
    pAt(v + ivec3(0,1,0), pc) - pc,
    pAt(v + ivec3(0,0,1), pc) - pc);
  // a correção vai toda pro escoamento; a pluma segue como está
  vec4 cur = fetchVol(uVel, v);
  oCol = vec4(cur.xyz - grad, cur.a);
}`);

    // ---- passagem da explosão: lê a sequência assada e soma aqui ----------
    // O mesmo corpo compilado nas duas grades: na fina soma os campos, na
    // grossa impõe a velocidade (uFrame.w diz qual).
    const INJECT = `
uniform sampler2D uSrc;
uniform vec4 uInst;     // posição.xyz, escala
uniform vec4 uFrame;    // quadro contínuo, base da variante, fração entregue neste quadro, alvo (0 campos · 1 velocidade)
uniform vec3 uBoxMin, uBoxMax;
out vec4 oCol;
vec4 bakeQ(vec3 lp, out vec3 vel){
  vec3 vb = worldToVoxelAtB(lp, vec3(0.0));
  float f0 = floor(uFrame.x), a = uFrame.x - f0, f1 = min(f0 + 1.0, uBakeBFrames - 1.0);
  vec4 q = mix(sampleBakeLayerB(vb, uFrame.y + f0), sampleBakeLayerB(vb, uFrame.y + f1), a);
  vel = bakeVel(vb, uFrame.y + f0);
  // o mesmo esmaecimento junto ao teto/laterais que a instância desenha
  // (boxFade): a grade recebe exatamente o que estava na tela
  float fade = smoothstep(0.0, GRIDB.y * 0.2, GRIDB.y - vb.y)
             * smoothstep(0.0, 5.0, min(min(vb.x, GRIDB.x - vb.x), min(vb.z, GRIDB.z - vb.z)));
  // mesma desquantização da instância
  return vec4(q.r * q.r * ${F(SOOT_SCALE)} * fade, q.g * ${F(TEMP_SCALE)}, q.b * q.b * ${F(DUST_SCALE)} * fade, 0.0);
}
void main(){
  vec3 p = fragToVoxel(gl_FragCoord.xy);
  vec4 cur = fetchVol(uSrc, ivec3(p));
  vec3 w = voxelToWorld(p);
  if (any(lessThan(w, uBoxMin - CELL)) || any(greaterThan(w, uBoxMax + CELL))){ oCol = cur; return; }
  // média de 2×2×2 subamostras na pegada da célula (1–2 m contra ~0.6 m assado)
  vec4 acc = vec4(0.0);
  vec3 vacc = vec3(0.0);
  float wv = 0.0, tmax = 0.0;
  for (int k = 0; k < 8; k++){
    vec3 o = (vec3(float(k & 1), float((k >> 1) & 1), float((k >> 2) & 1)) - 0.5) * (CELL * 0.5);
    vec3 lp = (w + o - uInst.xyz) / uInst.w;
    vec3 vb = worldToVoxelAtB(lp, vec3(0.0));
    if (any(lessThan(vb, vec3(0.0))) || any(greaterThan(vb, GRIDB))) continue;
    vec3 vel;
    vec4 q = bakeQ(lp, vel);
    acc += q;
    tmax = max(tmax, q.g);
    float dw = q.r + q.b;
    vacc += vel * dw; wv += dw;
  }
  acc *= 0.125;
  // A emissão cresce muito com T (convexa): a MÉDIA de 8 subamostras apagava
  // os núcleos quentes e a nuvem perdia o brilho na entrega. Pelo pico ela
  // continua brilhando onde a explosão brilhava, até esfriar.
  acc.g = tmax;
  if (uFrame.w < 0.5){
    oCol = cur + vec4(acc.rgb, 0.0) * uFrame.z;
  } else {
    // o movimento continua: onde há fumaça entregue, a velocidade do ar
    // passa a ser a da sequência (velocidade em m/s locais × escala ÷
    // tempo também escalado = a mesma em m/s)
    // A subida da sequência vai pro canal da pluma (que desacelera como
    // térmica); o resto vira escoamento.
    float k = clamp(uFrame.z * 3.0, 0.0, 1.0) * smoothstep(0.0, 0.08, acc.r + acc.b);
    vec3 vb = wv > 1e-5 ? vacc / wv : vec3(cur.x, cur.y + cur.a, cur.z);
    oCol = vec4(mix(cur.x, vb.x, k), mix(cur.y, 0.0, k), mix(cur.z, vb.z, k), mix(cur.a, vb.y, k));
  }
}`;
    const BAKEQ = B.glsl('B', 'Q') + B.fuelGlsl('U');
    this.shInjectD = mkD('injectD', INJECT, BAKEQ);
    this.shInjectV = mkV('injectV', INJECT, BAKEQ);

    // ---- macro: extinção máxima por bloco de 4³ (pra pular o vazio) ------
    this.shMacro = new Shader(gl, FS_VS,
      `#version 300 es\nprecision highp float;\nin vec2 vUV;\n` + COMMON + G.glsl() + M.glsl('M') + `
uniform sampler2D uFld;
uniform float uSootExt, uDustExt;
out vec4 oCol;
void main(){
  ivec3 bm = ivec3(fragToVoxelM(gl_FragCoord.xy));
  float m = 0.0;
  for (int z = 0; z < 4; z++) for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++){
    ivec3 v = bm * 4 + ivec3(x, y, z);
    if (any(greaterThanEqual(v, GRIDI))) continue;
    vec4 f = fetchVol(uFld, v);
    m = max(m, uSootExt * f.r + uDustExt * (f.b + f.a));
  }
  oCol = vec4(m, 0.0, 0.0, 0.0);
}`, 'battle-macro');

    // ---- luz: transmitância até o sol e até o céu (zênite), por célula ----
    // (por célula GROSSA, marchando pela fumaça fina: a sombra é suave)
    this.shLight = mkV('light', `
uniform sampler2D uFld;
uniform vec3 uKeyDir;
uniform float uSootExt, uDustExt;
out vec4 oCol;
float march(vec3 w, vec3 dir, int n, float stepM){
  float tau = 0.0;
  for (int i = 1; i <= 24; i++){
    if (i > n) break;
    vec3 q = worldToVoxelD(w + dir * (float(i) * stepM));
    if (q.y > GRIDD.y || q.x < 0.0 || q.z < 0.0 || q.x > GRIDD.x || q.z > GRIDD.z) break;
    vec4 f = sampleVolD(uFld, q);
    tau += (uSootExt * f.r + uDustExt * (f.b + f.a)) * stepM;
  }
  return exp(-tau);
}
void main(){
  vec3 w = voxelToWorld(fragToVoxel(gl_FragCoord.xy));
  vec3 kd = uKeyDir.y < 0.02 ? normalize(vec3(uKeyDir.x, 0.02, uKeyDir.z)) : uKeyDir;
  oCol = vec4(march(w, kd, 24, 1.6), march(w, vec3(0.0, 1.0, 0.0), 14, 2.0), 0.0, 1.0);
}`);

    // ---- cópia do aux do volume (profundidade da explosão por pixel) ------
    this.shCopy = new Shader(gl, FS_VS, `#version 300 es\nprecision highp float;\nin vec2 vUV;\n
uniform sampler2D uSrc;
out vec4 oCol;
void main(){ oCol = texture(uSrc, vUV); }`, 'battle-copy');

    // ---- render: marcha a meia resolução, em dois trechos -------------------
    // O volume da batalha ENVOLVE as explosões: o trecho atrás da explosão é
    // somado por trás (como as instâncias) e o trecho da frente por cima,
    // separados pela profundidade que a explosão gravou no aux.
    this.shRender = new Shader(gl, FS_VS, headD + ENVLUT + M.glsl('M') + `
uniform sampler2D uFld, uVel, uLight, uMacro, uHalfDepth, uAux, uBB;
uniform sampler3D uNoise;
uniform mat4 uInvViewProj;
uniform vec3 uCamPos, uKeyDir;
uniform float uSeg, uNear, uJitter, uTime;
uniform int uSteps;
uniform float uSootExt, uDustExt, uSootAlbedo, uDustAlbedo;
uniform vec3 uSootColor, uDustColor;
uniform float uEmissionGain, uEmissionCurve, uTempScale;
uniform float uPhaseG, uPhaseBack, uPhaseMix, uSkyGain;
uniform float uDetailAmp, uDetailScale, uDetailDens, uErode, uErodeAbs, uFlowPeriod;
uniform float uMsExt, uMsScatter, uMsPhase;
uniform int uMsOctaves;
#define MAXL 8
uniform int uLCount;
uniform vec4 uLP[MAXL];   // luz das explosões: posição.xyz, s²
uniform vec3 uLC[MAXL];
out vec4 oCol;

vec2 rayBoxW(vec3 ro, vec3 rd, vec3 bmin, vec3 bmax){
  vec3 inv = 1.0 / rd;
  vec3 t0 = (bmin - ro) * inv, t1 = (bmax - ro) * inv;
  vec3 tn = min(t0, t1), tf = max(t0, t1);
  return vec2(max(max(tn.x, tn.y), tn.z), min(min(tf.x, tf.y), tf.z));
}
// distância (m) até sair do bloco macro atual
float macroExit(vec3 w, vec3 rd){
  vec3 pm = worldToVoxelM(w);
  vec3 dv = rd * INV_CELLM;
  vec3 nb = floor(pm) + step(vec3(0.0), dv);
  vec3 tb = (nb - pm) / dv;
  return max(min(min(tb.x, tb.y), tb.z), 0.0);
}
float macroAt(vec3 w){
  ivec3 v = clamp(ivec3(floor(worldToVoxelM(w))), ivec3(0), GRIDIM - 1);
  return fetchVolM(uMacro, v).x;
}
// Ruído de detalhe advectado em duas fases, o MESMO esquema e escala das
// explosões (a forma grande vem da grade de 1 m e da turbulência simulada;
// o ruído só devolve a textura fina que a grade não guarda). Um ruído grande
// e parado aqui deixava a nuvem uma bolha de borda peluda logo na entrega.
vec3 flowNoise(vec3 w, vec3 vel){
  float ph = uTime / uFlowPeriod;
  vec3 r = vec3(0.0);
  float w2 = 0.0;
  for (int k = 0; k < 2; k++){
    float pk = ph + 0.5 * float(k);
    float c = floor(pk), tk = pk - c;
    float wt = 1.0 - abs(2.0 * tk - 1.0);
    vec3 off = hash33(vec3(c, float(k), 3.0)) * 17.0;
    // deslocamento limitado a ~3 grãos do ruído: sem limite, na esteira de um
    // avião o ruído esticava em risco; com limite curto demais (0.7 m) o
    // detalhe ficava parado enquanto a fumaça corria por baixo dele — mais
    // uma razão de ela parecer dura
    vec3 adv = vel * (tk * uFlowPeriod);
    adv *= min(1.0, 1.5 / max(length(adv), 1e-4));
    r += wt * (texture(uNoise, (w - adv) * uDetailScale + off).xyz * 2.0 - 1.0);
    w2 += wt * wt;
  }
  return r * inversesqrt(max(w2, 1e-4));
}

void main(){
  vec2 ndc = vUV * 2.0 - 1.0;
  vec4 h0 = uInvViewProj * vec4(ndc, -1.0, 1.0);
  vec4 h1 = uInvViewProj * vec4(ndc, 1.0, 1.0);
  vec3 ro = uCamPos;
  vec3 rd = normalize(h1.xyz / h1.w - h0.xyz / h0.w);
  float tScene = texture(uHalfDepth, vUV).r;
  vec4 aux = texture(uAux, vUV);
  float split = aux.z > 0.02 ? aux.x / aux.z : 1e9;

  vec2 hit = rayBoxW(ro, rd, DOMAIN_MIN, DOMAIN_MAX);
  hit.x = max(hit.x, uNear);
  hit.y = min(hit.y, tScene);
  if (uSeg < 0.5) hit.x = max(hit.x, split); else hit.y = min(hit.y, split);
  if (hit.y <= hit.x){ oCol = vec4(0.0, 0.0, 0.0, 1.0); return; }

  // Passo fixo de 1 m DENTRO da fumaça; o vazio da caixa (192 m) é pulado
  // pelo macro. Dividir o raio inteiro em N passos dava ~2.6 m por passo,
  // ~6 amostras numa nuvem de 15 m — chuvisco com o jitter por pixel.
  float dt = CELL;
  float t = hit.x + dt * ignoise(gl_FragCoord.xy + uJitter * 53.7);
  int used = 0;

  float cosSun = dot(rd, uKeyDir);
  float phSun = phaseDual(cosSun, uPhaseG, uPhaseBack, uPhaseMix);
  float phIso = 1.0 / (4.0 * PI);
  vec3 keyCol = envKeyColor(), skyCol = envSkyUp();

  vec3 L = vec3(0.0);
  float Tr[4];
  for (int i = 0; i < 4; i++) Tr[i] = 1.0;

  for (int i = 0; i < 400; i++){
    if (t >= hit.y || Tr[0] < 0.004 || used >= uSteps) break;
    vec3 w = ro + rd * t;
    float mac = macroAt(w);
    if (mac < 0.003){
      t += macroExit(w, rd) + 1e-3;      // já em metros
      continue;
    }
    used++;
    vec3 p = worldToVoxel(w);
    vec3 pv = worldToVoxelV(w);
    // velocidade só pro ruído: uma leitura (fatia z mais próxima, sem a
    // reconstrução deslocada) basta pra arrastar o detalhe junto
    vec4 vv = texture(uVel, tileUVV(pv.xy, floor(pv.z)));
    vec3 vel = vec3(vv.x, vv.y + vv.a, vv.z);
    vec3 n = flowNoise(w, vel);
    vec4 f = sampleVol(uFld, worldToVoxel(w + n * uDetailAmp));
    float nm = (n.x + n.y + n.z) * 0.577;
    f.rba *= 1.0 + uDetailDens * nm;
    // Erosão absoluta: a fumaça diluída é comida primeiro, então a nuvem se
    // desfaz pelas bordas. (Na escala do lugar — a extinção do bloco macro —
    // ela duraria enquanto houvesse massa; era o que segurava a fumaça por
    // minutos.)
    float er = uErode * (0.5 - 0.5 * nm) * mix(saturate(mac), 1.0, uErodeAbs);
    f.r = max(f.r - er / (1.0 + f.r * 9.0), 0.0);
    f.b = max(f.b - er * 0.7 / (1.0 + f.b * 9.0), 0.0);
    f.a = max(f.a - er * 0.7 / (1.0 + f.a * 9.0), 0.0);
    // some rente ao teto da grade em vez de cortar reto
    f.rba *= smoothstep(GRID.y, GRID.y * 0.8, p.y);

    if (f.r + f.b + f.a > 1e-5){
      float sigSoot = uSootExt * f.r, sigDust = uDustExt * f.b, sigExh = uDustExt * f.a;
      float sigT = sigSoot + sigDust + sigExh;
      vec3 wk = vec3(sigSoot, sigDust, sigExh) / max(sigT, 1e-5);
      // três espécies: fuligem (escura), poeira do chão (bege) e a fumaça do
      // motor de foguete (Al₂O₃: branca, quase não absorve)
      vec3 scatCol = uSootColor * wk.x + uDustColor * wk.y + vec3(${F(EXH_COLOR[0])}, ${F(EXH_COLOR[1])}, ${F(EXH_COLOR[2])}) * wk.z;
      float albedo = dot(wk, vec3(uSootAlbedo, uDustAlbedo, ${F(EXH_ALBEDO)}));
      // transmitância MARCHADA até o sol (como o cache de luz da explosão);
      // o pow(céu, sunOcclude) é só a aproximação de quando não há cache
      vec4 lt = sampleVolV(uLight, pv);
      float sunT = lt.r, skyT = lt.g;
      // o calor que sobrou ainda brilha fraco
      vec4 bb = texture(uBB, vec2(saturate(f.g * uTempScale), 0.5));
      vec3 emit = bb.rgb * (pow(max(bb.a, 0.0), uEmissionCurve) * uEmissionGain)
                * (sigSoot * (1.0 - uSootAlbedo));
      // luz das explosões vivas, com a mesma queda que a explosão usa dentro
      // do próprio volume. A oclusão do CÉU (que vem de cima) não serve pra
      // uma luz que está dentro da nuvem — apagava o brilho interno.
      vec3 fireIrr = vec3(0.0);
      for (int l = 0; l < MAXL; l++){
        if (l >= uLCount) break;
        vec3 dl = uLP[l].xyz - w;
        fireIrr += uLC[l] / (uLP[l].w + dot(dl, dl) * 0.05);
      }

      float ae = 1.0, ab = 1.0, ap = 1.0;
      for (int o = 0; o < 4; o++){
        if (o >= uMsOctaves) break;
        float sig = sigT * ae;
        float sc = sig * albedo * ab;
        float ph = mix(phIso, phSun, ap);
        vec3 src = scatCol * sc * (keyCol * mix(1.0, sunT, ap) * ph
                                 + skyCol * uSkyGain * mix(1.0, skyT, ap * 0.7 + 0.3) * phIso
                                 + fireIrr * mix(phIso, phSun, ap * 0.5))
                 + emit * ab;
        float ext = max(sig, 1e-5);
        float trStep = exp(-ext * dt);
        L += Tr[o] * (src - src * trStep) / ext;
        Tr[o] *= trStep;
        ae *= uMsExt; ab *= uMsScatter; ap *= uMsPhase;
      }
    }
    t += dt;
  }
  oCol = vec4(L, Tr[0]);
}`, 'battle-render');
  }

  get domain() {
    const G = this.grid;
    return { min: G.domainMin, max: G.domainMin.map((v, k) => v + G.domainSize[k]) };
  }

  /** explosão: a expansão empurra a fumaça antiga pra fora (fonte de divergência) */
  addBlast(pos, scale = 1) {
    // ∇·u = S numa bola de raio R dá u_r(R) = S·R/3: ~18 m/s na borda de 9 m
    // por ~0.15 s, o pulso da onda de choque deslocando o ar uns metros
    const R = 9 * scale;
    this.blasts.push({ pos: Float32Array.from(pos), radius: R, src: 3 * 18 / R, until: this.time + 0.15 });
  }

  /** passo do fluido. instances: lista das instâncias (pra entrega) */
  step(dt, inst, env, P) {
    if (dt <= 0) return;
    const gl = this.gl, S = this.params;
    dt = Math.min(dt, 1 / 30);
    this.time += dt;
    this.frame++;
    gl.disable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);
    gl.disable(gl.SCISSOR_TEST);
    const o0 = [0, 0, 0];

    // 0) campo vazio: nada a simular nem a desenhar (custo zero). A massa
    // estimada cai com a dissipação; abaixo do limiar o resto (invisível pela
    // erosão) é zerado de uma vez.
    this.massEstimate *= Math.exp(-dt / S.sootTau);
    const t0 = Math.min(S.handoffT0, S.earlyHandoff) - 0.05;
    const busy = this.massEstimate > S.idleMass || this.disturbers.length > 0
      || (inst && inst.list.some((o) => !o.handed && !o.outside && o.t >= t0));
    if (!busy) {
      if (!this.idle) { this.reset(); this.idle = true; }
      this.disturbers.length = 0;
      this.blasts.length = 0;
      return;
    }
    this.idle = false;

    // 1) entrega das explosões que estão na janela
    this._handoff(inst);

    // transporte da fumaça em 2 subpassos: |u|·Δt/h < ⅓ por eixo na grade
    // FINA dá até 0.67·h/Δt — 40 m/s a 60 qps, 20 m/s a 30 qps
    const sub = 2, umax = 0.95 * (sub / 3) * this.grid.cell / dt;

    // 2) advecção da velocidade (componente por componente, na face)
    this.shAdvectVel.use().set('uDomainOrigin', o0).set('uDt', dt).tex('uVel', this.vel.read.tex);
    this.vel.write.bind(); drawFS(gl); this.vel.swap();

    // 3) vorticidade e forças (inclui os objetos passando)
    if (S.vorticity > 0) {
      this.shCurl.use().set('uDomainOrigin', o0).tex('uVel', this.vel.read.tex);
      this.curl.bind(); drawFS(gl);
    }
    const D = this._packDisturb();
    for (const d of this.disturbers) if (d.kind === 5) this.massEstimate += 0.02;   // fumaça de motor
    const sf = this.shForces.use().set('uDomainOrigin', o0).set('uDt', dt).set('uTime', this.time)
      .set('uBuoy', S.buoyancy).set('uSootW', S.sootWeight)
      .set('uTurb', S.turb).set('uTurbScale', S.turbScale).set('uTurbRate', S.turbRate)
      .set('uTurbV', S.turbV).set('uVort', S.vorticity)
      .set('uVelKeep', Math.exp(-dt / S.velTau)).set('uRiseKeep', Math.exp(-dt / S.riseTau))
      .set('uUMax', umax);
    this._setDisturb(sf, D);
    sf.tex('uVel', this.vel.read.tex).tex('uFld', this.fld.read.tex).tex('uCurl', this.curl.tex)
      .tex('uNoise', this.noiseTex, gl.TEXTURE_3D);
    this.vel.write.bind(); drawFS(gl); this.vel.swap();

    // 4) divergência (+ explosões) → pressão → projeção
    this.blasts = this.blasts.filter((b) => b.until > this.time);
    const nb = Math.min(this.blasts.length, 4);
    const bp = new Float32Array(16), bs = new Float32Array(4);
    for (let i = 0; i < nb; i++) {
      const b = this.blasts[i];
      bp.set([b.pos[0], b.pos[1], b.pos[2], b.radius], i * 4);
      bs[i] = b.src;
    }
    const sd = this.shDiv.use().set('uDomainOrigin', o0).tex('uVel', this.vel.read.tex);
    sd.seti('uBCount', nb);
    gl.uniform4fv(sd.loc('uBlast[0]'), bp);
    gl.uniform1fv(sd.loc('uBSrc[0]'), bs);
    this.div.bind(); drawFS(gl);
    // pressão: partida a quente do quadro anterior
    const sp = this.shPressure.use().set('uDomainOrigin', o0).tex('uDiv', this.div.tex);
    for (let i = 0; i < S.pressureIters; i++) {
      sp._unit = 0;
      sp.tex('uPress', this.prs.read.tex).tex('uDiv', this.div.tex);
      this.prs.write.bind(); drawFS(gl); this.prs.swap();
    }
    this.shProject.use().set('uDomainOrigin', o0)
      .tex('uVel', this.vel.read.tex).tex('uPress', this.prs.read.tex);
    this.vel.write.bind(); drawFS(gl); this.vel.swap();

    // 5) transporte da fumaça (volumes finitos) com decaimento
    const hs = dt / sub, ks = Math.exp(-hs / S.sootTau);
    for (let i = 0; i < sub; i++) {
      const sa = this.shAdvect.use();
      sa._unit = 0;
      sa.set('uDomainOrigin', o0).set('uDt', hs).set('uKeep', [ks, Math.exp(-hs / S.coolTau), ks, ks])
        .tex('uVel', this.vel.read.tex).tex('uSrc', this.fld.read.tex);
      this.fld.write.bind(); drawFS(gl); this.fld.swap();
    }
    if (D.n) {
      const fx = this.shFieldsFx.use().set('uDomainOrigin', o0).set('uDt', dt);
      this._setDisturb(fx, D);
      fx.tex('uFld', this.fld.read.tex);
      this.fld.write.bind(); drawFS(gl); this.fld.swap();
    }
    this.disturbers.length = 0;

    // 6) macro e luz
    this.shMacro.use().set('uDomainOrigin', o0).set('uSootExt', P.sootExt).set('uDustExt', P.dustExt)
      .tex('uFld', this.fld.read.tex);
    this.macro.bind(); drawFS(gl);
    if (this.frame % S.lightEvery === 0) {
      this.shLight.use().set('uDomainOrigin', o0).set('uKeyDir', env.keyDir)
        .set('uSootExt', P.sootExt).set('uDustExt', P.dustExt)
        .tex('uFld', this.fld.read.tex);
      this.light.bind(); drawFS(gl);
    }
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }

  /**
   * Altura média da fumaça numa coluna de raio r em volta de (x, z), lida do
   * macro (9 mil texels: uma leitura síncrona, só pra mirar os testes).
   * null se não há fumaça ali.
   */
  smokeHeightAt(x, z, r = 16) {
    if (this.idle || this.massEstimate <= 0) return null;
    const gl = this.gl, M = this.macroGrid;
    const px = new Float32Array(M.atlasW * M.atlasH * 4);
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.macro.fbo);
    gl.readPixels(0, 0, M.atlasW, M.atlasH, gl.RGBA, gl.FLOAT, px);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    let s = 0, sy = 0;
    for (let k = 0; k < M.nz; k++) {
      const tx = k % M.tilesX, ty = (k / M.tilesX) | 0;
      const wz = M.domainMin[2] + (k + 0.5) * M.cell;
      for (let i = 0; i < M.nx; i++) {
        const wx = M.domainMin[0] + (i + 0.5) * M.cell;
        if (Math.hypot(wx - x, wz - z) > r) continue;
        for (let j = 0; j < M.ny; j++) {
          const v = px[((ty * M.ny + j) * M.atlasW + tx * M.nx + i) * 4];
          s += v; sy += v * (j + 0.5) * M.cell;
        }
      }
    }
    return s > 1e-3 ? sy / s : null;
  }

  /** objetos passando neste quadro (a lista é consumida pelo step) */
  disturb(d) { if (this.disturbers.length < MAX_DISTURB) this.disturbers.push(d); }

  _packDisturb() {
    const n = Math.min(this.disturbers.length, MAX_DISTURB);
    const A = new Float32Array(MAX_DISTURB * 4), B = new Float32Array(MAX_DISTURB * 4);
    const C = new Float32Array(MAX_DISTURB * 4), D = new Float32Array(MAX_DISTURB * 4);
    for (let i = 0; i < n; i++) {
      const d = this.disturbers[i];
      A.set([d.pos[0], d.pos[1], d.pos[2], d.radius], i * 4);
      B.set([d.prev[0], d.prev[1], d.prev[2], d.kind], i * 4);
      C.set([d.vel[0], d.vel[1], d.vel[2], d.strength], i * 4);
      const r = d.right || d.extra || [0, 0, 0];
      D.set([r[0], r[1], r[2], d.span || 0], i * 4);
    }
    return { n, A, B, C, D };
  }

  _setDisturb(sh, D) {
    const gl = this.gl;
    sh.seti('uDCount', D.n);
    gl.uniform4fv(sh.loc('uDA[0]'), D.A);
    gl.uniform4fv(sh.loc('uDB[0]'), D.B);
    gl.uniform4fv(sh.loc('uDC[0]'), D.C);
    gl.uniform4fv(sh.loc('uDD[0]'), D.D);
  }

  /**
   * Fração já entregue de uma instância no tempo de sequência t: rampa suave
   * na janela [handoffT0, handoffT1] (degrau quando as duas são iguais). A
   * instância é desenhada com (1 − w).
   */
  handoffW(t) {
    const { handoffT0: a, handoffT1: b } = this.params;
    if (b <= a) return t >= a ? 1 : 0;
    const x = Math.min(Math.max((t - a) / (b - a), 0), 1);
    return x * x * (3 - 2 * x);
  }

  /** algum perturbador deste quadro passou pela caixa (com folga do raio)? */
  _crossed(box) {
    for (const d of this.disturbers) {
      const r = Math.max(d.radius, d.span || 0);
      let t0 = 0, t1 = 1;
      for (let k = 0; k < 3; k++) {
        const a = d.prev[k], e = d.pos[k] - a;
        const lo = box.min[k] - r, hi = box.max[k] + r;
        if (Math.abs(e) < 1e-9) { if (a < lo || a > hi) { t0 = 2; break; } continue; }
        let u0 = (lo - a) / e, u1 = (hi - a) / e;
        if (u0 > u1) [u0, u1] = [u1, u0];
        t0 = Math.max(t0, u0); t1 = Math.min(t1, u1);
        if (t0 > t1) break;
      }
      if (t0 <= t1) return true;
    }
    return false;
  }

  _handoff(inst) {
    const gl = this.gl, B = this.bake, o0 = [0, 0, 0];
    if (!inst || !B.ready) return;
    const dom = this.domain;
    for (const o of inst.list) {
      if (o.handed || o.outside) continue;
      let w = this.handoffW(o.t);
      // Algo atravessando uma explosão ainda na sequência assada: ela não
      // tem como reagir, então passa pra grade AGORA (de uma vez) e a grade
      // reage. Só no fim do brilho: antes disso a troca apaga o fogo (a
      // grade de 1 m não guarda os núcleos quentes) e a subida da bola de
      // fogo, que a grade amortece como a de uma nuvem já parada — testado
      // a 2.3 s, a nuvem ficou baixa e escura enquanto a original subia.
      const box = inst._box(o);
      if (box && o.t >= this.params.earlyHandoff && this._crossed(box)) w = 1;
      const dw = w - (o.handW || 0);
      if (dw <= 0) continue;
      if (!box) continue;
      // fora do campo coberto pela grade: a explosão toca até o fim, como antes
      if (o.pos[0] < dom.min[0] || o.pos[0] > dom.max[0]
          || o.pos[2] < dom.min[2] || o.pos[2] > dom.max[2]) { o.outside = true; continue; }
      o.handW = w;
      if (w >= 1) o.handed = true;
      const frame = B.frameOfTime(o.t), base = (o.variant || 0) * B.frames;
      for (const which of [0, 1]) {
        const pp = which ? this.vel : this.fld;
        const sh = (which ? this.shInjectV : this.shInjectD).use();
        sh._unit = 0;
        sh.set('uDomainOrigin', o0)
          .set('uInst', [o.pos[0], o.pos[1], o.pos[2], o.scale])
          .set('uFrame', [frame, base, dw, which])
          .set('uBoxMin', box.min).set('uBoxMax', box.max)
          .set('uBakeBFrames', B.frames)
          .tex('uSrc', pp.read.tex)
          .tex('uBakeB', B.tex, gl.TEXTURE_2D_ARRAY)
          .tex('uBakeMacroQ', B.macroTex, gl.TEXTURE_2D_ARRAY)
          .tex('uBakeFuelU', B.fuelTex, gl.TEXTURE_2D_ARRAY);
        pp.write.bind(); drawFS(gl); pp.swap();
      }
      this.massEstimate += dw;
    }
  }

  /**
   * Desenha no alvo do volume (meia resolução), DEPOIS das explosões.
   * @param volTarget MRTarget do volume (cor+transmitância, aux)
   * @param halfDepth distância da cena por pixel (meia resolução)
   * @param VP        aparência (os parâmetros das instâncias: é delas que a
   *                  fumaça vem, e na entrega as duas têm que bater)
   */
  render(cam, env, volTarget, halfDepth, lights, VP) {
    if (this.idle || this.massEstimate <= 0) return;
    const gl = this.gl, S = this.params;
    const w = volTarget.w, h = volTarget.h;
    if (!this.auxCopy || this.auxCopy.w !== w || this.auxCopy.h !== h) {
      if (this.auxCopy) this.auxCopy.dispose();
      this.auxCopy = new Target(gl, w, h, { internalFormat: gl.RGBA16F, format: gl.RGBA,
        type: gl.HALF_FLOAT, filter: gl.NEAREST });
    }
    gl.disable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);
    this.shCopy.use().tex('uSrc', volTarget.texs[1]);
    this.auxCopy.bind(); drawFS(gl);

    const n = Math.min(lights.length, 8);
    const lp = new Float32Array(32), lc = new Float32Array(24);
    for (let i = 0; i < n; i++) {
      const L = lights[i];
      lp.set([L.pos[0], L.pos[1], L.pos[2], L.s2 ?? 1], i * 4);
      lc.set(L.color, i * 3);
    }

    volTarget.bind();
    gl.drawBuffers([gl.COLOR_ATTACHMENT0, gl.NONE]);
    gl.enable(gl.BLEND);
    const sh = this.shRender.use();
    sh.set('uDomainOrigin', [0, 0, 0])
      .set('uInvViewProj', cam.invViewProj).set('uCamPos', cam.pos)
      .set('uKeyDir', env.keyDir).set('uNear', cam.near)
      .set('uJitter', env.frameJitter).set('uTime', this.time)
      .seti('uSteps', S.steps)
      .set('uSootExt', VP.sootExt).set('uDustExt', VP.dustExt)
      .set('uSootAlbedo', VP.sootAlbedo).set('uDustAlbedo', VP.dustAlbedo)
      .set('uSootColor', VP.sootColor).set('uDustColor', VP.dustColor)
      .set('uEmissionGain', VP.emissionGain).set('uEmissionCurve', VP.emissionCurve)
      .set('uTempScale', VP.tempScale)
      .set('uPhaseG', VP.phaseG).set('uPhaseBack', VP.phaseBack).set('uPhaseMix', VP.phaseMix)
      .set('uSkyGain', VP.skyGain)
      .set('uMsExt', VP.msExt).set('uMsScatter', VP.msScatter).set('uMsPhase', VP.msPhase)
      .seti('uMsOctaves', VP.msOctaves)
      .set('uDetailAmp', S.detailAmp).set('uDetailScale', S.detailScale)
      .set('uDetailDens', S.detailDens).set('uErode', S.erode).set('uErodeAbs', S.erodeAbs)
      .set('uFlowPeriod', S.flowPeriod);
    sh.seti('uLCount', n);
    gl.uniform4fv(sh.loc('uLP[0]'), lp);
    gl.uniform3fv(sh.loc('uLC[0]'), lc);
    sh.tex('uFld', this.fld.read.tex).tex('uVel', this.vel.read.tex)
      .tex('uLight', this.light.tex).tex('uMacro', this.macro.tex)
      .tex('uHalfDepth', halfDepth).tex('uAux', this.auxCopy.tex)
      .tex('uBB', this.bbTex).tex('uEnvLut', env.envLut)
      .tex('uNoise', this.noiseTex, gl.TEXTURE_3D);
    // trecho de trás: somado atrás de tudo que já está no alvo
    gl.blendFuncSeparate(gl.DST_ALPHA, gl.ONE, gl.ZERO, gl.SRC_ALPHA);
    sh.set('uSeg', 0);
    drawFS(gl);
    // trecho da frente: por cima (L = Lf + Tf·L, T = Tf·T)
    gl.blendFuncSeparate(gl.ONE, gl.SRC_ALPHA, gl.ZERO, gl.SRC_ALPHA);
    sh.set('uSeg', 1);
    drawFS(gl);
    gl.disable(gl.BLEND);
    gl.drawBuffers([gl.COLOR_ATTACHMENT0, gl.COLOR_ATTACHMENT1]);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }

  reset() {
    const gl = this.gl;
    for (const t of [this.vel.a, this.vel.b, this.fld.a, this.fld.b, this.prs.a, this.prs.b,
      this.curl, this.light, this.macro]) t.bind(true);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    this.massEstimate = 0;
    this.disturbers.length = 0;
    this.blasts.length = 0;
  }
}
