// FXFire v1: a lógica do fogo da carcaça, sem desenho. É a ESPECIFICAÇÃO: a Unreal
// (FXLab/Source/FXLab/Private/FXWreckFire.cpp) repete isto linha a linha, com a mesma
// semente, e as duas têm de dar a mesma história quadro a quadro.
//
// Unidades: metros e segundos. Eixos: z pra cima (os da Unreal, em metros); o laboratório
// converte pros do three.js (y pra cima) só na hora de desenhar.
//
// Cada partícula é um cartão virado pra câmera que toca a gravação (cozimento 6-way):
// - bolota: nasce no anel da torre como bola de fogo, ganha casca e vira fumaça, que
//   continua tocando a própria gravação até se desfazer em fiapos (a nuvem se abre na
//   simulação; não é só o alfa caindo);
// - lambida: um pedaço curto do começo da gravação (a bola amarela), no anel, pra o fogo
//   nunca descolar do casco entre uma bolota e outra.

export const MAX_CARDS = 160;
export const LICK_LIFE = 0.5;      // s
export const LICK_SIM = 31;        // a lambida toca até este quadro da simulação (a bola ainda amarela)
export const SMOKE_COOL = 0.35;    // s: a brasa que sobra na casca apaga quando vira fumaça

// mulberry32: o mesmo gerador nos dois lados (inteiros de 32 bits)
export function makeRng(seed) {
  let s = seed >>> 0;
  const rand = () => {
    s = (s + 0x6D2B79F5) >>> 0;
    let t = s;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
  return { rand, rnd: (a, b) => a + rand() * (b - a) };
}

export function smoothstep(x, lo, hi) {
  if (x <= lo) return 0;
  if (x >= hi) return 1;
  const k = (x - lo) / (hi - lo);
  return k * k * (3 - 2 * k);
}

export function sunDir(P) {
  // az 0 = sol na frente da câmera de RTS (contraluz), positivo = à direita dela. Eixos da Unreal.
  const el = P.sunElev * Math.PI / 180, az = P.sunAz * Math.PI / 180;
  return [Math.sin(az) * Math.cos(el), -Math.cos(az) * Math.cos(el), Math.sin(el)];
}

export class FXFire {
  constructor(meta, P, hatch, seed = 7) {
    this.meta = meta;
    this.P = P;
    this.hatch = hatch;                       // [x, y, z] m
    this.rng = makeRng(seed);
    this.cards = [];
    this.spawnClock = 0;
    this.lickClock = 0;
    const times = meta.times_s, n = meta.frames;
    this.fps = (n - 1) / times[times.length - 1];               // quadros por segundo de gravação
    // quadro da lambida: o da simulação mais perto de LICK_SIM
    const sf = meta.sim_frames;
    this.lickEnd = sf.reduce((best, f, i) => Math.abs(f - LICK_SIM) < Math.abs(sf[best] - LICK_SIM) ? i : best, 0);
  }

  fireDur() { return this.meta.fire_end / (this.fps * this.P.speed); }

  spawnPuff() {
    if (this.cards.length >= MAX_CARDS) return;
    const { rnd, rand } = this.rng, h = this.hatch;
    const x = rnd(-0.35, 0.35), y = rnd(-0.35, 0.35);
    this.cards.push({
      p: [h[0] + x, h[1] + y, h[2]], age: 0, rot: rnd(-0.12, 0.12), rotV: rnd(-0.06, 0.06),
      flip: rand() < 0.5 ? -1 : 1, size: rnd(0.85, 1.15), lick: false,
    });
  }

  spawnLick() {
    if (this.cards.length >= MAX_CARDS) return;
    const { rnd, rand } = this.rng, h = this.hatch, P = this.P;
    const a = rnd(0, Math.PI * 2), r = rnd(0, P.baseR), dz = rnd(-0.3, 0.1);
    this.cards.push({
      p: [h[0] + Math.cos(a) * r, h[1] + Math.sin(a) * r, h[2] + dz], age: 0, rot: rnd(-0.2, 0.2), rotV: 0,
      flip: rand() < 0.5 ? -1 : 1, size: rnd(0.4, 0.6), lick: true,
    });
  }

  // avança dt segundos; cam = posição da câmera [x, y, z] m (a ordem de desenho depende dela)
  step(dt, cam) {
    const P = this.P, { rnd } = this.rng, cards = this.cards;
    this.spawnClock += dt;
    while (this.spawnClock > P.interval * rnd(0.75, 1.25)) { this.spawnClock = 0; this.spawnPuff(); }
    this.lickClock += dt;
    while (this.lickClock > P.lickInterval) { this.lickClock -= P.lickInterval * rnd(0.6, 1.4); this.spawnLick(); }
    const fireDur = this.fireDur();
    for (let i = cards.length - 1; i >= 0; i--) {
      const c = cards[i];
      c.age += dt;
      const life = c.lick ? LICK_LIFE : fireDur + P.smokeLife;
      if (c.age > life) { cards.splice(i, 1); continue; }
      if (c.lick) { c.p[2] += 1.2 * dt; continue; }
      c.p[2] += P.rise * (c.age < fireDur ? 1 : 0.7) * dt;
      c.p[0] += P.wind * Math.min(1, (c.p[2] - this.hatch[2]) / 8) * dt;
      c.rot += c.rotV * dt;
    }
    // de trás pra frente (alfa pré-multiplicado em ordem); ordenação estável
    const d2 = c => (c.p[0] - cam[0]) ** 2 + (c.p[1] - cam[1]) ** 2 + (c.p[2] - cam[2]) ** 2;
    cards.sort((a, b) => d2(b) - d2(a));
  }

  // o que o material precisa de cada cartão, na ordem de desenho
  outputs() {
    const P = this.P, m = this.meta, cards = this.cards, fireDur = this.fireDur();
    const side = m.side_m * P.scale, n = m.frames, fe = m.fire_end;
    // sombra entre cartões: quanto de fumaça dos outros fica entre o cartão e o sol
    const sd = sunDir(P);
    for (const c of cards) c.rad = c.lick ? 0 : 0.28 * side * c.size * (1 + P.smokeGrow * Math.max(0, c.age - fireDur));
    for (const c of cards) {
      let T = 1;
      for (const q of cards) {
        if (q === c || q.rad <= 0) continue;
        const dx = q.p[0] - c.p[0], dy = q.p[1] - c.p[1], dz = q.p[2] - c.p[2];
        const along = dx * sd[0] + dy * sd[1] + dz * sd[2];
        if (along <= 0) continue;
        const perp = Math.sqrt(Math.max(0, dx * dx + dy * dy + dz * dz - along * along));
        if (perp >= q.rad) continue;
        T *= 1 - P.shadow * (1 - perp / q.rad);
      }
      c.T = T;
    }
    const out = [];
    let heat = 0;
    for (const c of cards) {
      if (c.lick) {
        const k = c.age / LICK_LIFE;
        out.push({ p: c.p, size: side * c.size, frame: k * this.lickEnd, rot: c.rot, flip: c.flip,
          alpha: Math.min(1, c.age / 0.05) * (1 - smoothstep(k, 0.5, 1)), emit: 1, T: 1, erode: 0 });
        heat += 0.3;
        continue;
      }
      const ts = Math.max(0, c.age - fireDur), life = fireDur + P.smokeLife, u = Math.min(1, ts / P.smokeLife);
      // fogo: o relógio da gravação; fumaça: o resto da gravação (a nuvem se abrindo) espalhado na vida da fumaça
      const frame = c.age < fireDur ? c.age * P.speed * this.fps : fe + (n - 1 - fe) * u;
      out.push({ p: c.p, size: side * c.size * (1 + P.smokeGrow * ts), frame, rot: c.rot, flip: c.flip,
        alpha: Math.min(1, c.age / 0.08) * (1 - smoothstep(c.age, life - 0.15 * P.smokeLife, life)),
        emit: 1 - smoothstep(ts, 0, SMOKE_COOL), T: c.T, erode: P.erode * smoothstep(u, 0.4, 1) });
      if (c.age < fireDur) heat += Math.max(0, 1 - frame / fe);
    }
    return { cards: out, heat };
  }
}
