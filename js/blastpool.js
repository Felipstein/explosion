// ---------------------------------------------------------------------------
// blastpool.js — várias explosões simultâneas.
//
// Havia UMA simulação, e detonar resetava ela: clicar num segundo ponto
// apagava o primeiro. Num RTS isso não serve.
//
// Cada explosão ocupa um SLOT com sua própria simulação e suas partículas.
// Todos os slots compartilham resolução e tamanho de domínio e diferem só na
// ORIGEM — por isso os shaders são idênticos e basta trocar texturas e
// `uDomainOrigin` entre as passagens.
//
// Política de passo: simular N fluidos por frame seria N× o custo. A fase
// quente (primeiro ~1.2s) é a única que precisa de 60Hz; depois a fumaça se
// move devagar e um passo a cada K frames com dt K× maior é visualmente
// equivalente — advecção semi-Lagrangiana é incondicionalmente estável, então
// dt maior não desestabiliza.
// ---------------------------------------------------------------------------

import { FluidSim } from './fluid.js';
import { Particles } from './particles.js';

// Quantas explosões podem coexistir. O limite real é memória: cada slot é
// uma simulação completa. Compartilhando os buffers transitórios (curl,
// divergência, níveis intermediários do macro e o ruído) o custo por slot
// cai ~30%, o que permite mais explosões simultâneas.
export const MAX_BLASTS = 8;
// Quantas afetam a ILUMINAÇÃO da cena. Sombrear N explosões com N luzes é
// N² marchas de sombra por pixel; renderizar todas é barato (o teste de
// caixa rejeita de imediato quem não está no caminho).
export const MAX_SHADED = 3;
const YOUNG = 1.0;      // s — abaixo disto a explosão roda a cada frame
const MAX_LIFE = 11.0;  // s — depois disto o slot é reciclado

export class BlastPool {
  constructor(gl, res, domainSize, bbTex, maxSlots = MAX_BLASTS) {
    this.gl = gl;
    this.slots = [];
    // o primeiro slot cria os buffers; os demais reusam os transitórios
    let shared = null;
    for (let i = 0; i < maxSlots; i++) {
      const fluid = new FluidSim(gl, res, domainSize, shared);
      if (!shared) {
        shared = {
          curl: fluid.curl, div: fluid.div, divC: fluid.divC,
          macroChain: fluid.macroChain, noiseTex: fluid.noiseTex,
        };
      }
      const particles = new Particles(gl, fluid.grid, bbTex);
      particles.params.count = Math.max(4096, Math.round(particles.params.count / maxSlots));
      this.slots.push({
        i, fluid, particles,
        age: Infinity,       // Infinity = livre
        alive: false,
        seq: 0,              // ordem de detonação, pra reciclar o mais velho
      });
    }
    this.grid = this.slots[0].fluid.grid;
    this.lightGrid = this.slots[0].fluid.lightGrid;
    this.macroGrid = this.slots[0].fluid.macroGrid;
    this._seq = 0;
    this._rr = 0;
  }

  get params() { return this.slots[0].fluid.params; }

  /** aplica a mesma configuração a todos os slots */
  configure(fn) { for (const s of this.slots) fn(s.fluid.params, s.particles.params); }

  /** slot livre; se não houver, recicla o mais antigo */
  _acquire() {
    let best = null;
    for (const s of this.slots) if (!s.alive) { best = s; break; }
    if (!best) {
      best = this.slots.reduce((a, b) => (a.seq < b.seq ? a : b));
    }
    return best;
  }

  detonate(pos, seed = Math.random() * 1000) {
    const s = this._acquire();
    s.fluid.detonate(pos, seed);
    s.particles.domainOrigin = s.fluid.domainOrigin;
    s.particles.reset(pos, seed);
    s.age = 0;
    s.alive = true;
    s.seq = ++this._seq;
    s.pos = Float32Array.from(pos);
    return s;
  }

  get active() { return this.slots.filter((s) => s.alive); }

  /** ativos ordenados do mais PRÓXIMO ao mais distante da câmera.
   *  A composição volumétrica é front-to-back, então a ordem importa. */
  sortedFor(camPos) {
    return this.active
      .map((s) => {
        const dx = s.pos[0] - camPos[0], dy = s.pos[1] - camPos[1], dz = s.pos[2] - camPos[2];
        return { s, d: dx * dx + dy * dy + dz * dz };
      })
      .sort((a, b) => a.d - b.d)
      .map((e) => e.s);
  }

  step(dt, keyDir, sootExt, dustExt, erodeMean) {
    const act = this.active;
    if (!act.length) return;

    // jovens avançam sempre; o resto entra em rodízio de um por frame
    const young = act.filter((s) => s.age < YOUNG);
    const old = act.filter((s) => s.age >= YOUNG);
    const toStep = new Map();
    for (const s of young) toStep.set(s, dt);
    if (old.length) {
      const pick = old[this._rr++ % old.length];
      toStep.set(pick, dt * old.length);     // compensa os frames pulados
    }

    for (const s of act) {
      s.age += dt;
      if (s.age > MAX_LIFE) { s.alive = false; s.age = Infinity; continue; }
      const sdt = toStep.get(s);
      if (!sdt) continue;
      const fire = this.fireLightOf(s);
      s.fluid.step(sdt, keyDir, sootExt, dustExt, fire, erodeMean);
      s.particles.step(sdt, s.fluid.vel.read.tex, s.fluid.domainOrigin);
    }
  }

  /** posição da luz do fogo de um slot (preenchida pelo VolumeRenderer) */
  fireLightOf(s) { return s.fire ? s.fire.pos : s.fluid.blastPos; }

  reset() {
    for (const s of this.slots) { s.alive = false; s.age = Infinity; }
  }
}
