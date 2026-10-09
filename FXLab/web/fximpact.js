// FXImpact v2: o tiro de tanque que acerta o chão (arte 4.1, docs/concept/explosions/
// type_4.1_sand_impact.webp). É a ESPECIFICAÇÃO: a Unreal (FXLab/Source/FXLab/Private/FXImpact.cpp)
// repete isto linha a linha, com a mesma semente. Metros e segundos, eixos da Unreal (z pra cima).
//
// Cada camada que tem corpo vem de uma simulação gravada (cartões 6-way), como os jogos fazem;
// desenhado à mão fica só o que os jogos também desenham assim (clarão, faísca, cratera):
// - nuvem de poeira: Dust Impact (JangaFX, CC0), no ponto do impacto;
// - anel de poeira: Dust Shockwave (JangaFX, CC0), a rosca que se abre rente ao chão;
// - jato de areia: grãos balísticos com arrasto e borrão de movimento (FXLab/Tools/bake_debris.py),
//   inclinado pra frente do tiro (o cartão espelha conforme o lado da tela);
// - clarão (~0,1 s) e a luz que pisca junto; faíscas esticadas pela velocidade;
// - pedrinhas que caem e ficam no chão; cratera escura que fica.
import { makeRng, smoothstep } from './fxfire.js';

export const IMPACT_DEFAULTS = {
  dustBake: 'dust_impact4', ringBake: 'shockwave_top', geyserBake: 'debris4',
  dirAz: 30,            // graus: pra onde o jato inclina (a frente do tiro), no chão
  flashSize: 2.6, flashTime: 0.1, flashGain: 30, flashLight: 600,
  sparkCount: 40, sparkSpeed: 26, sparkLife: 0.45,
  chunkCount: 50, chunkSpeed: 12, chunkSize: 0.09,
  dustScale: 1.2, dustSpeed: 1.0, dustLife: 4.0,
  ringStretch: 1.3, ringShift: 1.2,   // a poeira vai mais pra frente do tiro (o crescente da arte)
  ringScale: 0.5, ringSpeed: 0.8, ringLife: 4.5, ringAlbedo: 5.0,   // anel: ~14 m no fim (a arte), claro, demora a assentar
  geyserScale: 1.0, geyserSpeed: 1.0, geyserFade: 0.35,  // o jato some de 35% a 75% da gravação (antes de o leque passar do quadro gravado)
  craterRadius: 1.9,
  sandR: 0.85, sandG: 0.8, sandB: 0.72,        // tom da poeira (linear): claro, pouco saturado (arte; o sol já é quente)
  dustDensity: 1.3, dustAlbedo: 4.0,          // a poeira é clara e mais rala que a fuligem
  geyserAlbedo: 0.35,                          // o jato é areia com terra: escuro
  softDist: 1.0,                               // m: partícula macia (some perto do chão em vez de cortar reto)
  originX: 9.0, originY: 2.0,                  // m: onde o tiro cai (ao lado do tanque, como na arte)
  gravity: 9.8,
};

export class FXImpact {
  // metas: { dust, ring, geyser } (atlas.json de cada cozimento)
  constructor(metas, P, origin, seed = 11) {
    this.metas = metas;
    this.P = P;
    this.origin = origin;
    this.rng = makeRng(seed);
    this.t = -1;
    this.parts = [];
    const fps = m => (m.frames - 1) / m.times_s[m.times_s.length - 1];
    this.fps = { dust: fps(metas.dust), ring: fps(metas.ring), geyser: fps(metas.geyser) };
  }

  fire() {
    const P = this.P, { rnd, rand } = this.rng, o = this.origin;
    this.t = 0;
    this.parts = [];
    const az = P.dirAz * Math.PI / 180;
    const fwd = [Math.cos(az), Math.sin(az), 0];
    const cone = (tiltDeg, spreadDeg) => {
      const tilt = tiltDeg * Math.PI / 180, sp = spreadDeg * Math.PI / 180;
      const a = rnd(0, Math.PI * 2), r = Math.sqrt(rnd(0, 1)) * sp;
      const ax = [fwd[0] * Math.sin(tilt), fwd[1] * Math.sin(tilt), Math.cos(tilt)];
      const side = [-fwd[1], fwd[0], 0];
      const up2 = [ax[1] * side[2] - ax[2] * side[1], ax[2] * side[0] - ax[0] * side[2], ax[0] * side[1] - ax[1] * side[0]];
      const c = Math.cos(r), s = Math.sin(r);
      return [0, 1, 2].map(i => ax[i] * c + (side[i] * Math.cos(a) + up2[i] * Math.sin(a)) * s);
    };
    for (let i = 0; i < P.sparkCount; i++) {
      const d = cone(10, 55), v = P.sparkSpeed * rnd(0.55, 1.3);
      this.parts.push({ kind: 'spark', p: [...o], v: d.map(x => x * v), life: P.sparkLife * rnd(0.5, 1.2), w: 0.04 });
    }
    for (let i = 0; i < P.chunkCount; i++) {
      const d = cone(12, 50), v = P.chunkSpeed * rnd(0.35, 1.3);
      this.parts.push({ kind: 'chunk', p: [o[0], o[1], o[2] + 0.1], v: d.map(x => x * v), w: P.chunkSize * rnd(0.5, 1.6), landed: -1 });
    }
    this.dustRot = rnd(-0.1, 0.1);
    this.dustFlip = rand() < 0.5 ? -1 : 1;
    this.ringRot = rnd(-0.05, 0.05);
  }

  step(dt) {
    if (this.t < 0) return;
    this.t += dt;
    const g = this.P.gravity;
    for (const q of this.parts) {
      if (q.kind === 'chunk' && q.landed >= 0) continue;
      q.v[2] -= g * dt;
      if (q.kind === 'spark') q.v = q.v.map(x => x * (1 - 1.5 * dt));
      for (let i = 0; i < 3; i++) q.p[i] += q.v[i] * dt;
      if (q.kind === 'chunk' && q.p[2] <= this.origin[2] + q.w * 0.3) { q.p[2] = this.origin[2] + q.w * 0.3; q.landed = this.t; }
    }
  }

  // cam: posição da câmera; camRight: a direita da tela nos eixos do mundo (pra espelhar o jato)
  outputs(cam, camRight) {
    const P = this.P, t = this.t, o = this.origin;
    const layers = { ring: [], dust: [], geyser: [] }, prims = [];
    if (t < 0) return { layers, prims, light: 0 };
    prims.push({ kind: 'crater', p: [o[0], o[1], o[2] + 0.08], size: P.craterRadius * 2 * Math.min(1, t / 0.05), alpha: 1 });
    const fl = Math.exp(-t / (P.flashTime * 0.5)) * (t < P.flashTime * 3 ? 1 : 0);
    if (fl > 0.002) prims.push({ kind: 'flash', p: [o[0], o[1], o[2] + 0.6], size: P.flashSize * (0.6 + 0.4 * Math.min(1, t / 0.04)), gain: P.flashGain * fl });
    for (const q of this.parts) {
      if (q.kind === 'spark') {
        if (t > q.life) continue;
        const sp = Math.hypot(...q.v), k = t / q.life;
        const dir = sp > 1e-4 ? q.v.map(x => x / sp) : [0, 0, 1];
        prims.push({ kind: 'spark', p: q.p, dir, len: Math.max(0.15, sp * 0.03), w: q.w, gain: 8 * (1 - k * k) });
      } else if (q.landed >= 0) {
        // a pedra voa dentro do jato gravado; aqui ela só aparece quando cai e fica no chão
        prims.push({ kind: 'chunk', p: q.p, w: q.w, alpha: Math.min(1, (t - q.landed) / 0.1) * (1 - smoothstep(t, 4.0, 5.0)), col: [0.045, 0.038, 0.032] });
      }
    }
    // nuvem de poeira
    {
      const m = this.metas.dust, life = P.dustLife, k = Math.min(1, t / life);
      if (t <= life) layers.dust.push({ p: o, size: m.side_m * P.dustScale, frame: Math.min(m.frames - 1, t * P.dustSpeed * this.fps.dust),
        rot: this.dustRot, flip: this.dustFlip, alpha: Math.min(1, t / 0.05) * (1 - smoothstep(k, 0.8, 1)), emit: 0, T: 1, erode: 0.3 * smoothstep(k, 0.5, 1) });
    }
    // anel (a rosca da Shockwave). Deitado no chão, de câmera baixa ele vira uma faixa escura: some
    // abaixo de ~25° (a nuvem em pé cobre esse papel)
    {
      const m = this.metas.ring, life = P.ringLife, k = Math.min(1, t / life);
      const hd = Math.hypot(cam[0] - o[0], cam[1] - o[1]), elev = Math.atan2(cam[2] - o[2], hd);
      const seen = smoothstep(elev, 0.3, 0.55);
      // deitado no chão (ground: o cartão fica no plano do chão, gravado de cima), um pouco acima pra não brigar com a areia
      const az = P.dirAz * Math.PI / 180, sh = P.ringShift * Math.min(1, t / 0.6);
      if (t <= life) layers.ring.push({ p: [o[0] + Math.cos(az) * sh, o[1] + Math.sin(az) * sh, o[2] + 0.25], size: m.side_m * P.ringScale,
        frame: Math.min(m.frames - 1, t * P.ringSpeed * this.fps.ring), rot: az + this.ringRot, flip: 1, stretch: P.ringStretch, alpha: Math.min(1, t / 0.08) * (1 - smoothstep(k, 0.7, 1)) * seen, emit: 0, T: 1, erode: 0.35 * smoothstep(k, 0.5, 1), ground: true });
    }
    // jato de areia: o cozimento abre pra direita do quadro; espelha se o tiro vai pra esquerda da tela
    {
      const m = this.metas.geyser, f = t * P.geyserSpeed * this.fps.geyser;
      const az = P.dirAz * Math.PI / 180;
      const flip = Math.cos(az) * camRight[0] + Math.sin(az) * camRight[1] >= 0 ? 1 : -1;
      if (f <= m.frames - 1) layers.geyser.push({ p: o, size: m.side_m * P.geyserScale, frame: f, rot: 0, flip,
        alpha: 1 - smoothstep(f, (m.frames - 1) * P.geyserFade, (m.frames - 1) * (P.geyserFade + 0.4)), emit: 0, T: 1, erode: 0 });
    }
    const d2 = c => (c.p[0] - cam[0]) ** 2 + (c.p[1] - cam[1]) ** 2 + (c.p[2] - cam[2]) ** 2;
    prims.sort((a, b) => (a.kind === 'crater' ? -1 : b.kind === 'crater' ? 1 : d2(b) - d2(a)));
    return { layers, prims, light: P.flashLight * fl };
  }
}
