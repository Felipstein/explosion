// ---------------------------------------------------------------------------
// movers.js — o que atravessa o campo: avião de ataque, míssil terra-ar e o
// disparo de um tanque.
//
// Existem pra fumaça REAGIR (ver battlesmoke.js). A cada quadro cada um vira
// um "perturbador" do fluido com o modelo do que ele faz no ar:
//   - avião: o corpo empurra/arrasta o ar e abre um túnel; a asa deixa o par
//     de vórtices de ponta de asa (circulação Γ = W/(ρ·U·b0), b0 = π/4·b pra
//     asa elíptica — Prandtl). O par desce sozinho a Γ/(2π·b0) e enrola a
//     fumaça em dois tubos, que é o que se vê quando um jato corta uma nuvem;
//   - míssil: jato do motor pra trás e a fumaça branca do propelente sólido
//     (óxido de alumínio), que fica no campo como fumaça clara;
//   - disparo: o choque e a esteira turbulenta de um projétil supersônico
//     abrem um túnel fino que a turbulência fecha.
// As explosões empurram a fumaça pela própria grade (BattleSmoke.addBlast).
//
// A geometria é de blocos (caixas e cilindros do layout), só pra ver o que
// passou; o que importa aqui é o efeito no ar.
// ---------------------------------------------------------------------------

const MAXP = 128;              // peças por malha (= MAX_MOVER_PARTS da cena)
const G = 9.81, RHO = 1.225;

export const MOVER_SPECS = {
  // avião de ataque ao solo (classe A-10 / Su-25): 20 t, 16 m de envergadura,
  // 140 m/s em passagem baixa
  plane: { speed: 140, mass: 20000, span: 16, thrust: 80000, alt: 34, approach: 260 },
  // míssil terra-ar: 3 m, sai do tubo a 80 m/s e acelera (~15 g) até 450 m/s
  // (empuxo de ~10 kN: um míssil de ~70 kg a 15 g)
  missile: { v0: 80, accel: 150, vmax: 450, thrust: 10000, life: 4, standoff: 70 },
  // tanque: munição de 120 mm a ~1000 m/s com traçante
  shell: { speed: 1000, life: 1.4, standoff: 90 },
};

const v3 = {
  add: (a, b) => [a[0] + b[0], a[1] + b[1], a[2] + b[2]],
  sub: (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]],
  mul: (a, s) => [a[0] * s, a[1] * s, a[2] * s],
  dot: (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2],
  cross: (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]],
  len: (a) => Math.hypot(a[0], a[1], a[2]),
  norm: (a) => { const l = Math.hypot(a[0], a[1], a[2]) || 1; return [a[0] / l, a[1] / l, a[2] / l]; },
};

/** quaternion (x, y, z, w) da rotação cujas colunas são os eixos a, b, c */
function quatFromBasis(a, b, c) {
  const m00 = a[0], m10 = a[1], m20 = a[2];
  const m01 = b[0], m11 = b[1], m21 = b[2];
  const m02 = c[0], m12 = c[1], m22 = c[2];
  const tr = m00 + m11 + m22;
  if (tr > 0) {
    const s = Math.sqrt(tr + 1) * 2;
    return [(m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s, 0.25 * s];
  }
  if (m00 > m11 && m00 > m22) {
    const s = Math.sqrt(1 + m00 - m11 - m22) * 2;
    return [0.25 * s, (m01 + m10) / s, (m02 + m20) / s, (m21 - m12) / s];
  }
  if (m11 > m22) {
    const s = Math.sqrt(1 + m11 - m00 - m22) * 2;
    return [(m01 + m10) / s, 0.25 * s, (m12 + m21) / s, (m02 - m20) / s];
  }
  const s = Math.sqrt(1 + m22 - m00 - m11) * 2;
  return [(m02 + m20) / s, (m12 + m21) / s, 0.25 * s, (m10 - m01) / s];
}

/** base ortonormal com o eixo `ax` (0 = x, 1 = y) ao longo de dir */
function basisAlong(dir, ax) {
  const up = Math.abs(dir[1]) > 0.98 ? [1, 0, 0] : [0, 1, 0];
  const side = v3.norm(v3.cross(dir, up));          // horizontal
  const top = v3.cross(side, dir);                  // "pra cima" do objeto
  // colunas (x, y, z) destras
  return ax === 0 ? [dir, top, v3.cross(dir, top)] : [side, dir, v3.cross(side, dir)];
}

export class Movers {
  constructor() {
    this.list = [];
    this.boxes = new Float32Array(16 * MAXP);
    this.cyls = new Float32Array(16 * MAXP);
    this.nBoxes = 0;
    this.nCyls = 0;
    this.lights = [];           // chama dos motores: {pos, color, s2}
  }

  /**
   * @param kind   'plane' | 'missile' | 'shell'
   * @param target ponto no chão sob a fumaça
   * @param side   direção horizontal unitária em que ele cruza a tela
   * @param alt    altura da fumaça ali (m), se conhecida — é por onde passam
   */
  spawn(kind, target, side, alt = null) {
    const S = MOVER_SPECS[kind];
    const m = { kind, t: 0, seed: Math.random() };
    const h = Math.max(alt ?? 34, 12);
    if (kind === 'plane') {
      const c = [target[0], Math.max(alt ?? S.alt, 12), target[2]];
      m.pos = v3.add(c, v3.mul(side, -S.approach));
      m.dir = side.slice();
      m.speed = S.speed;
      m.life = (2 * S.approach) / S.speed;
    } else if (kind === 'missile') {
      // do chão, de um lado, subindo pela nuvem
      m.pos = v3.add([target[0], 1.5, target[2]], v3.mul(side, -S.standoff));
      m.dir = v3.norm(v3.sub([target[0], h, target[2]], m.pos));
      m.speed = S.v0;
      m.life = S.life;
    } else {
      // o tanque atira de longe num alvo acima da fumaça
      m.pos = v3.add([target[0], 2.6, target[2]], v3.mul(side, -S.standoff));
      m.dir = v3.norm(v3.sub([target[0], h, target[2]], m.pos));
      m.speed = S.speed;
      m.life = S.life;
    }
    m.prev = m.pos.slice();
    this.list.push(m);
    return m;
  }

  update(dt, battle) {
    this.nBoxes = this.nCyls = 0;
    this.lights.length = 0;
    for (const m of this.list) {
      m.prev = m.pos.slice();
      if (dt > 0) {
        m.t += dt;
        if (m.kind === 'missile') {
          const S = MOVER_SPECS.missile;
          m.speed = Math.min(S.v0 + S.accel * m.t, S.vmax);
        }
        m.pos = v3.add(m.pos, v3.mul(m.dir, m.speed * dt));
        if (battle) this._disturb(m, battle);
      }
      this._geom(m);
    }
    this.list = this.list.filter((m) => m.t < m.life);
  }

  // ---- o efeito no ar ------------------------------------------------------
  _disturb(m, battle) {
    const vel = v3.mul(m.dir, m.speed);
    if (m.kind === 'plane') {
      const S = MOVER_SPECS.plane;
      // corpo: o ar perto é arrastado (camada limite, esteira) e a projeção
      // faz o resto do escoamento contornar; o túnel na fumaça é a fração da
      // célula que o corpo varre
      battle.disturb({ kind: 1, pos: m.pos, prev: m.prev, radius: 1.8, vel, strength: 0.2 });
      // Jato dos dois turbofans (TF34, 2 × 40 kN, somados num bocal
      // equivalente de 1.7 m): sai a ~300 m/s em relação ao avião, 160 m/s
      // acima da velocidade dele; mais atrás, o que manda é a quantidade de
      // movimento que o empuxo deixa no ar (ver o jato em battlesmoke.js).
      const nozzle = v3.add(m.pos, v3.mul(m.dir, -7.2));
      battle.disturb({ kind: 6, pos: nozzle, prev: v3.add(nozzle, v3.mul(m.dir, -150)), radius: 1,
        vel, strength: S.thrust / m.speed / RHO, extra: [300 - m.speed, 0, 0], span: 1.7 });
      // Esteira: Γ = W/(ρ·U·b0). 20 t a 140 m/s → Γ ≈ 91 m²/s, o par desce a
      // ~1.1 m/s. Núcleo real ~0.05·b (0.8 m); 2 m (uma célula) é o menor
      // que a grade de velocidade resolve — a velocidade de pico fica menor e
      // o tubo mais gordo, mas a circulação (o que enrola a fumaça) é a mesma.
      const b0 = (Math.PI / 4) * S.span;
      const gamma = (S.mass * G) / (RHO * m.speed * b0);
      // "direita" orientada pra que o par sopre PRA BAIXO entre os vórtices
      const right = v3.norm(v3.cross([0, 1, 0], m.dir));
      battle.disturb({ kind: 4, pos: m.pos, prev: m.prev, radius: 2.0, vel,
        strength: gamma, right, span: b0 });
    } else if (m.kind === 'missile') {
      // o escape sai da cauda; o trecho varrido neste quadro é onde a pluma está
      const tail = v3.add(m.pos, v3.mul(m.dir, -1.8));
      const tailPrev = v3.add(m.prev, v3.mul(m.dir, -1.8));
      // fumaça do propelente por quadro (canal próprio). O rastro já nasce
      // com ~2.5 m: com 0.9 m de raio ele tinha uma célula de largura e saía
      // pontilhado onde cruzava a grade na diagonal.
      battle.disturb({ kind: 5, pos: tail, prev: tailPrev, radius: 1.3, vel, strength: 0.08 });
      // jato do motor-foguete: ~2 km/s em relação ao bocal de 25 cm
      battle.disturb({ kind: 6, pos: tail, prev: v3.add(tail, v3.mul(m.dir, -150)), radius: 1,
        vel, strength: MOVER_SPECS.missile.thrust / m.speed / RHO, extra: [2000 - m.speed, 0, 0],
        span: 0.25 });
    } else {
      // 120 mm a Mach 3: o choque de proa e a esteira turbulenta misturam um
      // tubo de ~2 m de diâmetro logo atrás dele
      battle.disturb({ kind: 2, pos: m.pos, prev: m.prev, radius: 1.1, vel, strength: 1.0 });
    }
  }

  // ---- geometria -----------------------------------------------------------
  _part(cyl, center, scale, q, mat, tint, rough, seed) {
    const arr = cyl ? this.cyls : this.boxes;
    const n = cyl ? this.nCyls : this.nBoxes;
    if (n >= MAXP) return;
    arr.set([center[0], center[1], center[2], seed,
      scale[0], scale[1], scale[2], mat,
      tint[0], tint[1], tint[2], rough,
      q[0], q[1], q[2], q[3]], n * 16);
    if (cyl) this.nCyls++; else this.nBoxes++;
  }

  _geom(m) {
    if (m.kind === 'plane') {
      // eixos locais: x = frente, y = cima, z = lado
      const [f, u, r] = basisAlong(m.dir, 0);
      const q = quatFromBasis(f, u, r);
      const at = (a, b, c) => v3.add(m.pos, v3.add(v3.mul(f, a), v3.add(v3.mul(u, b), v3.mul(r, c))));
      const paint = [0.17, 0.19, 0.18], s = m.seed;
      this._part(false, at(0, 0, 0), [15, 1.7, 1.7], q, 4, paint, 0.55, s);          // fuselagem
      this._part(false, at(8.2, -0.1, 0), [1.8, 1.1, 1.1], q, 4, paint, 0.55, s);    // nariz
      this._part(false, at(8.9, 0.6, 0), [1.6, 0.5, 0.9], q, 3, [0.08, 0.09, 0.1], 0.2, s);  // cabine
      this._part(false, at(-0.5, -0.2, 0), [3.2, 0.3, 16], q, 4, paint, 0.55, s);    // asa
      this._part(false, at(-6.8, 0.4, 0), [1.8, 0.2, 6], q, 4, paint, 0.55, s);      // profundor
      this._part(false, at(-6.6, 1.9, 0), [2.2, 2.6, 0.25], q, 4, paint, 0.55, s);   // deriva
      // motores (cilindro: eixo y local ao longo da frente)
      const [cx, cy, cz] = basisAlong(m.dir, 1);
      const qc = quatFromBasis(cx, cy, cz);
      for (const side of [-1.6, 1.6]) {
        this._part(true, at(-5.6, 1.1, side), [1.2, 3.2, 1.2], qc, 3, [0.10, 0.10, 0.10], 0.5, s);
      }
    } else if (m.kind === 'missile') {
      const [cx, cy, cz] = basisAlong(m.dir, 1);
      const q = quatFromBasis(cx, cy, cz);
      const tail = v3.add(m.pos, v3.mul(m.dir, -1.6));
      this._part(true, tail, [0.28, 3.0, 0.28], q, 4, [0.55, 0.56, 0.52], 0.45, m.seed);
      // aletas em cruz
      const [f, u, r] = basisAlong(m.dir, 0);
      const qb = quatFromBasis(f, u, r);
      const fin = v3.add(m.pos, v3.mul(m.dir, -1.35));
      this._part(false, fin, [0.45, 0.9, 0.05], qb, 4, [0.4, 0.4, 0.38], 0.5, m.seed);
      this._part(false, fin, [0.45, 0.05, 0.9], qb, 4, [0.4, 0.4, 0.38], 0.5, m.seed);
      // chama do motor: cone de luz atrás do bocal, cintilando
      const flick = 0.8 + 0.4 * Math.random();
      const flame = v3.add(m.pos, v3.mul(m.dir, -1.6 - 1.4 * flick));
      this._part(true, flame, [0.22, 1.4 * flick, 0.22], q, 5, [40, 22, 8], 1, m.seed);
      // ~0.5 MW visíveis (motor de ~2.5 MW térmicos) contra ~200 MW da bola
      // de fogo de um disparo de tanque: 0.3% da luz de pico da explosão.
      // Basta pra fumaça do próprio rastro brilhar laranja perto do bocal.
      this.lights.push({ pos: v3.add(m.pos, v3.mul(m.dir, -2.2)),
        color: [0.30 * flick, 0.16 * flick, 0.06 * flick], s2: 0.25 });
    } else {
      // traçante: o rastro que o olho integra num quadro, fino e brilhante
      const segL = Math.min(v3.len(v3.sub(m.pos, m.prev)), 24);
      if (segL < 0.05) return;
      const [f, u, r] = basisAlong(m.dir, 0);
      const q = quatFromBasis(f, u, r);
      const c = v3.add(m.pos, v3.mul(m.dir, -segL / 2));
      this._part(false, c, [segL, 0.14, 0.14], q, 5, [60, 18, 5], 1, m.seed);
    }
  }
}
