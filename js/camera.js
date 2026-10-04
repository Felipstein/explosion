// ---------------------------------------------------------------------------
// camera.js — câmera cinemática. Órbita/push-in com keyframes suaves, ruído
// de câmera na mão, e shake de impacto amortecido disparado pela detonação
// (com atraso: o choque leva tempo pra chegar até a lente).
// ---------------------------------------------------------------------------

import { m4, v3, clamp, smoothstep, mix } from './math.js';

const SHOT = [
  // Domínio de 38 m: pra caber a coluna inteira num fov de ~34° a câmera
  // precisa ficar a ~55 m.
  //  t     dist   azim    elev   alvoY   fov
  [ 0.00, 56.0, -0.62, 0.068,  5.0, 0.600],
  [ 0.55, 51.0, -0.66, 0.085,  7.5, 0.590],
  [ 2.20, 47.0, -0.79, 0.142, 13.0, 0.575],
  [ 4.50, 50.0, -0.96, 0.192, 18.0, 0.570],
  [ 8.00, 56.0, -1.18, 0.224, 21.0, 0.585],
  [13.00, 64.0, -1.45, 0.242, 22.0, 0.600],
];

// Rolagem pelo botão direito arrastado, a do C&C Generals / Zero Hour
// (LookAtXlat.cpp no código que a EA liberou em 2025): a câmera anda na
// direção do cursor em relação ao ponto onde o botão desceu, com velocidade
// = fator·distância + um mínimo. Aqui em "alturas de tela por segundo", pra
// valer igual em qualquer zoom e resolução.
const RMB_DEAD = 0.02;   // fração da meia altura da tela: tremida da mão não rola
const RMB_MIN = 0.15;    // velocidade mínima fora da zona morta (telas/s)
const RMB_GAIN = 2.0;    // por meia altura de tela de afastamento (telas/s)
const MAP_HALF = 500;    // m: o centro da câmera fica sobre o chão

function sampleShot(t) {
  let i = 0;
  while (i < SHOT.length - 2 && t > SHOT[i + 1][0]) i++;
  const a = SHOT[i], b = SHOT[i + 1];
  const k = smoothstep(a[0], b[0], t);
  const out = [];
  for (let j = 1; j < 6; j++) out.push(mix(a[j], b[j], k));
  return out;
}

export class Camera {
  constructor() {
    this.view = m4.create();
    this.proj = m4.create();
    this.viewProj = m4.create();
    this.invViewProj = m4.create();
    this.prevViewProj = m4.create();
    this.pos = v3.create(0, 5, 30);
    this.target = v3.create(0, 5, 0);
    this.fov = 0.6;
    this.near = 0.12;
    this.far = 900;

    // RTS: a câmera fica onde o jogador deixou. 'cine' (tecla c) ainda existe
    // como modo de apresentação, mas não é o padrão.
    this.mode = 'free';           // 'cine' | 'free'
    this.frozen = false;          // congela handheld+shake pra comparar frames
    this.center = [0, 0, 0];      // ponto em torno do qual a câmera orbita
    // enquadramento inicial de RTS: alto o bastante pra ver o terreno
    this.dist = 62;
    this.azim = -0.62;
    this.elev = 0.36;
    this.targetY = 2.0;
    this.shake = 0;
    this._t = 0;
  }

  kick(amount) {
    if (!Number.isFinite(amount)) return;
    this.shake = Math.min(Math.max(this.shake, amount), 1.5);
  }

  orbit(dx, dy) {
    this.mode = 'free';
    this.azim -= dx * 0.005;
    // não deixa a câmera passar do horizonte nem abaixo do chão
    this.elev = clamp(this.elev + dy * 0.004, 0.06, 1.35);
  }
  zoom(d) {
    this.mode = 'free';
    this.dist = clamp(this.dist * Math.pow(1.0012, d), 8, 220);
  }
  panY(d) { this.mode = 'free'; this.targetY = clamp(this.targetY + d, 0, 30); }

  /**
   * Rolagem RTS (botão direito arrastado).
   * @param ox, oy cursor menos âncora, em meias alturas de tela (y pra baixo)
   * @param dt     tempo REAL: a câmera lenta da simulação não a freia
   */
  scrollRMB(ox, oy, dt) {
    const r = Math.hypot(ox, oy);
    if (r < RMB_DEAD || !(dt > 0)) return;
    this.mode = 'free';
    // altura de chão enquadrada no alvo, ~2·dist·tan(fov/2)
    const view = 2 * this.dist * Math.tan(this.fov * 0.5);
    const step = (RMB_MIN + RMB_GAIN * (r - RMB_DEAD)) * view * Math.min(dt, 0.1) / r;
    // frente horizontal (da câmera pro alvo) e direita
    const fx = -Math.cos(this.azim), fz = -Math.sin(this.azim);
    const rx = -fz, rz = fx;
    // cursor à direita → anda pra direita; abaixo da âncora → recua
    this.center[0] = clamp(this.center[0] + (rx * ox - fx * oy) * step, -MAP_HALF, MAP_HALF);
    this.center[2] = clamp(this.center[2] + (rz * ox - fz * oy) * step, -MAP_HALF, MAP_HALF);
  }

  update(aspect, dt, shotTime) {
    // dt NUNCA negativo nem absurdo: o shake é multiplicado por exp(-dt·k), e
    // um único dt negativo o faz crescer exponencialmente — foi assim que a
    // câmera foi parar a bilhões de metros.
    dt = Number.isFinite(dt) ? Math.min(Math.max(dt, 0), 0.25) : 0;
    this._t += dt;
    m4.copy ? 0 : 0;
    this.prevViewProj.set(this.viewProj);

    let dist = this.dist, azim = this.azim, elev = this.elev, ty = this.targetY, fov = this.fov;
    if (this.mode === 'cine') {
      const [d, a, e, y, f] = sampleShot(shotTime);
      dist = d; azim = a; elev = e; ty = y; fov = f;
      this.dist = d; this.azim = a; this.elev = e; this.targetY = y;
    } else {
      fov = 0.58;
    }

    // ruído de câmera na mão: soma de senos incomensuráveis (sem repetição
    // percebida) — bem mais crível que ruído branco
    const T = this.frozen ? 0 : this._t;
    const hn = (p, s) => (Math.sin(T * 0.73 + p) * 0.55 + Math.sin(T * 1.31 + p * 2.1) * 0.3
                        + Math.sin(T * 2.17 + p * 3.7) * 0.15) * s;
    azim += hn(0.0, 0.0075);
    elev += hn(1.7, 0.0042);
    ty += hn(3.4, 0.045);

    // shake de impacto: alta frequência, decaimento exponencial
    this.shake *= Math.exp(-dt * 3.1);
    if (!Number.isFinite(this.shake) || this.shake < 1e-5) this.shake = 0;
    const sh = this.frozen ? 0 : Math.min(this.shake, 1.5);
    let shx = 0, shy = 0, shz = 0;
    if (sh > 1e-4) {
      const w = T * 47.0;
      shx = (Math.sin(w * 1.00) + Math.sin(w * 2.31) * 0.5) * sh * 0.055;
      shy = (Math.sin(w * 1.37 + 2.1) + Math.sin(w * 3.11) * 0.5) * sh * 0.055;
      shz = Math.sin(w * 0.83 + 1.3) * sh * 0.03;
      fov *= 1.0 + sh * 0.012;
    }

    const ce = Math.cos(elev), se = Math.sin(elev);
    v3.set(this.target, this.center[0] + shx * 0.4, ty + shy * 0.4,
                        this.center[2] + shz * 0.4);
    v3.set(this.pos,
      this.target[0] + Math.cos(azim) * ce * dist,
      this.target[1] + se * dist,
      this.target[2] + Math.sin(azim) * ce * dist);
    this.pos[0] += shx; this.pos[1] += shy; this.pos[2] += shz;
    this.pos[1] = Math.max(this.pos[1], 0.45);

    // O shot é enquadrado em termos de extensão HORIZONTAL. Com fov vertical
    // fixo, um painel retrato cortaria as laterais e decapitaria a explosão;
    // então abaixo de 16:9 o fov vertical cresce pra preservar a largura.
    const REF_ASPECT = 16 / 9;
    if (aspect < REF_ASPECT) {
      const halfW = Math.tan(fov * 0.5) * REF_ASPECT;
      fov = 2 * Math.atan(halfW / Math.max(aspect, 0.35));
      fov = Math.min(fov, 2.30);
    }

    this.fov = fov;
    m4.lookAt(this.view, this.pos, this.target, [0, 1, 0]);
    m4.perspective(this.proj, fov, aspect, this.near, this.far);
    m4.mul(this.viewProj, this.proj, this.view);
    m4.invert(this.invViewProj, this.viewProj);
  }
}
