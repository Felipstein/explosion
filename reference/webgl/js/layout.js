// ---------------------------------------------------------------------------
// layout.js — composição procedural do cenário (pátio industrial ao
// crepúsculo). Determinístico: mesma seed = mesma cena.
//
// Cada instância: 16 floats
//   iA = pos.xyz, seed    iB = scale.xyz, matId
//   iC = tint.rgb, rough  iD = quat(xyzw)
// ---------------------------------------------------------------------------

import { rng } from './math.js';

export const MAT = { GROUND: 0, CONCRETE: 1, RUST: 2, METAL: 3, RUBBLE: 4 };

function quatAxis(ax, ay, az, ang) {
  const l = Math.hypot(ax, ay, az) || 1;
  const s = Math.sin(ang / 2);
  return [(ax / l) * s, (ay / l) * s, (az / l) * s, Math.cos(ang / 2)];
}
function quatMul(a, b) {
  return [
    a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
    a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
    a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
    a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2],
  ];
}

export function buildLayout(seed = 20250819) {
  const r = rng(seed);
  const boxes = [], cyls = [];
  const push = (arr, pos, scale, mat, tint, rough, quat, sd) => {
    arr.push(pos[0], pos[1], pos[2], sd,
             scale[0], scale[1], scale[2], mat,
             tint[0], tint[1], tint[2], rough,
             quat[0], quat[1], quat[2], quat[3]);
  };
  const ring = (min, max) => {
    const a = r() * Math.PI * 2;
    const d = min + (max - min) * Math.sqrt(r());
    return [Math.cos(a) * d, Math.sin(a) * d, d];
  };
  const CONC = () => [0.082 + r() * 0.022, 0.080 + r() * 0.022, 0.076 + r() * 0.020];

  // --- barreiras de concreto tipo "jersey", algumas tombadas -------------
  for (let i = 0; i < 22; i++) {
    const [x, z, d] = ring(8, 19);
    const tipped = r() < 0.3;
    const yaw = Math.atan2(z, x) + Math.PI / 2 + (r() - 0.5) * 0.7;
    let q = quatAxis(0, 1, 0, yaw);
    let h = 1.05;
    if (tipped) {
      q = quatMul(quatAxis(Math.cos(yaw), 0, Math.sin(yaw), (r() < 0.5 ? 1 : -1) * (1.2 + r() * 0.45)), q);
      h = 0.45;
    }
    push(boxes, [x, h * 0.5 + 0.02, z], [3.0 + r() * 0.8, 1.05, 0.62], MAT.CONCRETE, CONC(), 0.9 + r() * 0.08, q, r() * 100);
  }

  // --- pilares / colunas quebradas: verticais que capturam a luz --------
  for (let i = 0; i < 7; i++) {
    const [x, z] = ring(11, 24);
    const h = 4.2 + r() * 5.0;
    const lean = (r() - 0.5) * 0.16;
    const q = quatMul(quatAxis(Math.cos(r() * 6.28), 0, Math.sin(r() * 6.28), lean), quatAxis(0, 1, 0, r() * 6.28));
    push(boxes, [x, h * 0.5, z], [0.75 + r() * 0.4, h, 0.75 + r() * 0.4], MAT.CONCRETE, CONC(), 0.88, q, r() * 100);
  }

  // --- placas / lajes de concreto arrancadas -----------------------------
  for (let i = 0; i < 16; i++) {
    const [x, z] = ring(5, 22);
    const ang = 0.15 + r() * 1.3;
    const q = quatMul(quatAxis(Math.cos(r() * 6.28), 0, Math.sin(r() * 6.28), ang), quatAxis(0, 1, 0, r() * 6.28));
    const w = 1.6 + r() * 2.6;
    push(boxes, [x, 0.35 + r() * 0.9, z], [w, 0.22 + r() * 0.14, w * (0.5 + r() * 0.7)], MAT.CONCRETE, CONC(), 0.92, q, r() * 100);
  }

  // --- escombros: massa de pedaços pequenos, mais denso perto do centro --
  for (let i = 0; i < 260; i++) {
    const a = r() * Math.PI * 2;
    const d = 2.0 + Math.pow(r(), 0.55) * 27;
    const s = 0.10 + Math.pow(r(), 2.2) * 0.85;
    const q = quatMul(quatAxis(r() - 0.5, r() - 0.5, r() - 0.5, r() * 6.28), quatAxis(0, 1, 0, r() * 6.28));
    const g = 0.068 + r() * 0.034;
    push(boxes, [Math.cos(a) * d, s * 0.35, Math.sin(a) * d],
         [s * (0.7 + r() * 0.9), s * (0.5 + r() * 0.7), s * (0.7 + r() * 0.9)],
         MAT.RUBBLE, [g, g * 0.95, g * 0.88], 0.94, q, r() * 100);
  }

  // --- silhuetas de fundo: galpões / muros ------------------------------
  for (let i = 0; i < 13; i++) {
    const a = r() * Math.PI * 2;
    const d = 38 + r() * 46;
    const h = 4 + r() * 11;
    push(boxes, [Math.cos(a) * d, h * 0.5, Math.sin(a) * d],
         [7 + r() * 16, h, 6 + r() * 14], MAT.CONCRETE,
         [0.055, 0.055, 0.058], 0.93, quatAxis(0, 1, 0, r() * 6.28), r() * 100);
  }

  // --- tambores de 200L, alguns tombados --------------------------------
  const DRUM_TINTS = [[0.26, 0.055, 0.035], [0.05, 0.10, 0.17], [0.24, 0.17, 0.03], [0.09, 0.11, 0.08]];
  for (let i = 0; i < 26; i++) {
    const [x, z] = ring(4.5, 20);
    const tint = DRUM_TINTS[(r() * DRUM_TINTS.length) | 0];
    const tipped = r() < 0.42;
    let q = quatAxis(0, 1, 0, r() * 6.28), y = 0.0;
    if (tipped) {
      q = quatMul(quatAxis(Math.cos(r() * 6.28), 0, Math.sin(r() * 6.28), Math.PI / 2), q);
      y = 0.29;
    }
    push(cyls, [x, y, z], [0.58, 0.88, 0.58], MAT.RUST, tint, 0.7, q, r() * 100);
  }

  // --- tubos de aço espalhados ------------------------------------------
  for (let i = 0; i < 18; i++) {
    const [x, z] = ring(3.5, 23);
    const len = 2.2 + r() * 4.5;
    const rad = 0.12 + r() * 0.2;
    const q = quatMul(quatAxis(0, 1, 0, r() * 6.28), quatAxis(1, 0, 0, Math.PI / 2 + (r() - 0.5) * 0.25));
    push(cyls, [x, rad + 0.01, z], [rad * 2, len, rad * 2], MAT.METAL, [0.075, 0.072, 0.068], 0.62, q, r() * 100);
  }

  // --- postes / vergalhões: verticais finos que dão escala --------------
  for (let i = 0; i < 14; i++) {
    const [x, z] = ring(6, 30);
    const h = 3.0 + r() * 6.5;
    const q = quatMul(quatAxis(Math.cos(r() * 6.28), 0, Math.sin(r() * 6.28), (r() - 0.5) * 0.5), quatAxis(0, 1, 0, r() * 6.28));
    push(cyls, [x, 0, z], [0.09 + r() * 0.07, h, 0.09 + r() * 0.07], MAT.METAL, [0.06, 0.055, 0.05], 0.7, q, r() * 100);
  }

  return {
    boxes: new Float32Array(boxes),
    cyls: new Float32Array(cyls),
    ground: new Float32Array([0, 0, 0, 0, 1, 1, 1, MAT.GROUND, 0.07, 0.068, 0.064, 0.9, 0, 0, 0, 1]),
  };
}
