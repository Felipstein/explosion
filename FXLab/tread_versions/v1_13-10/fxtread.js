// FXTread: a poeira do andar (o rastro de poeira atrás das lagartas do tanque em movimento). É a
// ESPECIFICAÇÃO: a Unreal (FXLab/Source/FXLab/Private/FXTread.cpp) repete isto linha a linha, com a mesma
// semente. Metros e segundos, eixos da Unreal (z pra cima).
//
// Como os jogos fazem (docs/BASE_JOGAVEL.md §7):
// - Zero Hour (W3DTankDraw.cpp, código liberado): um emissor por lagarta (TreadDebrisLeft/Right), preso no
//   tanque, ligado só com o tanque andando (velocidade² > 0,00001) e fora da névoa; a velocidade escala a
//   quantidade (setBurstCountMultiplier = velocidade + 0,1, até 1) e a velocidade das partículas
//   (0,5 × velocidade + 0,1, até 1). Prioridade baixa: é o primeiro efeito a sair quando falta orçamento.
// - Jogos de hoje (Niagara, CoH3): partícula por distância andada (não por tempo: o rastro fica igual em
//   qualquer quadro), cor pelo tipo de chão, cartão de fumaça gravado com luz 6-way.
// Aqui: cartões da Dust Impact (JangaFX, CC0; o cozimento dust_impact4), que já é poeira de areia
// simulada. Cada cartão toca a parte em que a nuvem já está aberta e se desfaz sozinha (quadros 12 a 44).
//
// O alvo são as cenas aprovadas (docs/concept/scenes/*/best/; recortes em captures/concept/fx_refs/):
// - rastro BAIXO: nasce no pé da lagarta de trás e fica abaixo do casco perto do tanque; só sobe até
//   ~1,5 casco na cauda, pra não tampar a unidade de trás (regra do Felipe: jogabilidade antes);
// - LARGO e CLARO: ocre claro pouco saturado na areia, ralo, mais denso junto da lagarta;
// - COMPRIDO na razão da velocidade: 1 a 2 tanques de comprimento andando rápido, quase nada devagar;
// - na neve: branco, mais grosso e mais curto (a neve cai rápido); no asfalto: pouco e cinza.
import { makeRng, smoothstep } from './fxfire.js';

export const TREAD_DEFAULTS = {
  bake: 'dust_impact4',
  ground: 'sand',          // sand | snow | mud | asphalt
  speed: 7.0,              // m/s: a velocidade do tanque no laboratório (~25 km/h)
  pathR: 26,               // m: o raio da volta que ele dá
  spacing: 0.4,            // m andados entre dois cartões, por lagarta
  fullSpeed: 6.0,          // m/s: daqui pra cima a poeira é a cheia
  life: 2.6,               // s
  frame0: 16, frame1: 50,   // os quadros do cozimento que cada cartão toca, do nascer ao sumir
  size0: 0.24, size1: 0.75, // × lado do cartão gravado (12 m): ~2,9 m nascendo, ~9 m no fim (as duas lagartas se juntam) (a nuvem ocupa ~40% do cartão)
  kick: 1.5,               // m/s pra trás (a lagarta joga a areia)
  side: 1.8,               // m/s pros lados, sorteado
  up: 0.35,                // m/s pra cima
  drag: 1.6,               // 1/s: o ar segura a poeira
  inherit: 0.08,           // fração da velocidade do tanque que a poeira leva
  windX: 0.6, windY: 0.3,  // m/s
  sink: 0.08,              // m/s: a poeira pesada baixa um pouco
  alpha: 0.55,             // opacidade no auge
  erode: 0.35,             // as bordas comem no fim
  rearX: -2.7, halfW: 1.55, // m: onde a lagarta de trás toca o chão (tanque de 7,9 m com o cano)
  along: 2.6,              // m: a poeira sobe da metade de trás da lagarta inteira, não só do canto
  rotMax: 0.6,             // rad: o giro sorteado de cada cartão (a gravação do EmberGen é larga: giro pequeno)
  // por chão: o tom (linear), a claridade, a densidade, e quanto a vida, a quantidade, a opacidade e o tamanho mudam
  sand: { r: 1.0, g: 0.78, b: 0.52, albedo: 4.0, density: 0.9, life: 1.0, amount: 1.0, alpha: 1.0, size: 1.0 },
  snow: { r: 0.95, g: 0.97, b: 1.0, albedo: 5.5, density: 2.2, life: 0.45, amount: 1.6, alpha: 1.5, size: 0.7 },   // a neve é jogada em leque grosso e cai rápido
  mud: { r: 0.42, g: 0.34, b: 0.27, albedo: 2.2, density: 1.2, life: 0.5, amount: 0.6, alpha: 0.7, size: 0.7 },
  asphalt: { r: 0.6, g: 0.58, b: 0.55, albedo: 3.0, density: 0.9, life: 0.7, amount: 0.0, alpha: 0.6, size: 0.9 },   // a areia não levanta no asfalto (o Felipe, 13/10)
};

export class FXTread {
  // meta: atlas.json do cozimento; P: o ajuste (TREAD_DEFAULTS + preset)
  constructor(meta, P, seed = 5) {
    this.meta = meta;
    this.P = P;
    this.rng = makeRng(seed);
    this.cards = [];
    this.t = 0;
    this.ang = 0;           // onde o tanque está na volta (rad)
    this.acc = [0, 0];      // metros andados desde o último cartão, por lagarta
    this.pose();
  }

  // a pose do tanque na volta (anda no sentido anti-horário): posição, frente e velocidade
  pose() {
    const P = this.P, a = this.ang;
    this.pos = [Math.cos(a) * P.pathR, Math.sin(a) * P.pathR, 0];
    this.fwd = [-Math.sin(a), Math.cos(a), 0];
    this.yaw = Math.atan2(this.fwd[1], this.fwd[0]);
    this.vel = this.fwd.map(x => x * P.speed);
  }

  // o ponto onde a lagarta de trás toca o chão (lado: -1 esquerda, +1 direita)
  contact(sideSign) {
    const P = this.P, f = this.fwd, r = [f[1], -f[0], 0];
    return [0, 1, 2].map(i => this.pos[i] + f[i] * P.rearX + r[i] * P.halfW * sideSign + (i === 2 ? 0.05 : 0));
  }

  step(dt) {
    const P = this.P, G = P[P.ground] || P.sand, { rnd } = this.rng;
    this.t += dt;
    // o tanque anda
    if (P.speed > 0) this.ang += P.speed * dt / P.pathR;
    this.pose();
    const sp = Math.abs(P.speed);
    const amount = Math.min(1, sp / P.fullSpeed) * G.amount;
    // nasce por distância andada, uma fila por lagarta
    if (sp > 0.3 && amount > 0) {
      for (let k = 0; k < 2; k++) {
        this.acc[k] += sp * dt;
        const spacing = P.spacing / Math.max(0.25, amount);
        while (this.acc[k] >= spacing) {
          this.acc[k] -= spacing;
          const sideSign = k === 0 ? -1 : 1;
          const c = this.contact(sideSign);
          // volta o ponto pra onde o tanque estava quando este cartão "nasceu" (o resto do passo)
          const back = this.acc[k] - rnd(0, P.along);   // em algum ponto da metade de trás da lagarta
          const p = c.map((x, i) => x - this.fwd[i] * back + (i < 2 ? rnd(-0.3, 0.3) : 0));
          const r = [this.fwd[1], -this.fwd[0], 0];
          const kick = P.kick * Math.min(1, 0.5 * sp + 0.1);             // a regra do ZH: 0,5 × velocidade + 0,1
          const sideV = rnd(-1, 1) * P.side + sideSign * P.side * 0.5;   // pra fora da lagarta
          const v = [0, 1, 2].map(i => -this.fwd[i] * kick + r[i] * sideV + this.vel[i] * P.inherit + (i === 2 ? P.up * rnd(0.6, 1.3) : 0));
          this.cards.push({
            p, v, age: 0, life: P.life * G.life * rnd(0.8, 1.2),
            s: rnd(0.85, 1.2), rot: rnd(-P.rotMax, P.rotMax), flip: this.rng.rand() < 0.5 ? -1 : 1,
            f0: P.frame0 + rnd(0, 4), a: amount, ga: G.alpha, gs: G.size,
          });
        }
      }
    }
    // a poeira anda: arrasto, vento, um pouco pra baixo (não passa do chão)
    const dk = Math.exp(-P.drag * dt);
    for (const c of this.cards) {
      c.age += dt;
      c.v = [c.v[0] * dk + P.windX * (1 - dk), c.v[1] * dk + P.windY * (1 - dk), c.v[2] * dk - P.sink * (1 - dk)];
      for (let i = 0; i < 3; i++) c.p[i] += c.v[i] * dt;
      if (c.p[2] < 0) c.p[2] = 0;
    }
    this.cards = this.cards.filter(c => c.age < c.life);
  }

  // os cartões pra desenhar (camada 'dust'), do mais longe pro mais perto da câmera
  outputs(cam) {
    const P = this.P, m = this.meta, out = [];
    for (const c of this.cards) {
      const k = c.age / c.life;
      const grow = 1 - Math.pow(1 - k, 2);                          // cresce rápido e assenta
      out.push({
        p: c.p, size: m.side_m * (P.size0 + (P.size1 - P.size0) * grow) * c.s * c.gs,
        frame: Math.min(m.frames - 1, c.f0 + (P.frame1 - P.frame0) * k), rot: c.rot, flip: c.flip,
        alpha: Math.min(1, P.alpha * c.a * c.ga) * smoothstep(c.age, 0, 0.2) * (1 - smoothstep(k, 0.2, 1)),   // denso junto da lagarta, ralo na cauda
        emit: 0, T: 1, erode: P.erode * smoothstep(k, 0.35, 1),
      });
    }
    const d2 = c => (c.p[0] - cam[0]) ** 2 + (c.p[1] - cam[1]) ** 2 + (c.p[2] - cam[2]) ** 2;
    out.sort((a, b) => d2(b) - d2(a));
    return out;
  }
}
