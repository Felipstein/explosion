// O código do mapa, comum às telas: o minimapa desenhado do chão (o de verdade ou o gerado pela semente).
function rng(seed) { let a = seed >>> 0; return () => { a = (a + 0x6D2B79F5) >>> 0; let t = a; t = Math.imul(t ^ (t >>> 15), t | 1); t ^= t + Math.imul(t ^ (t >>> 7), t | 61); return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
// ---------- o minimapa: o chão desenhado como no jogo (a luz dos game shots, a sombra pro mesmo lado) ----------
const PAL = {
  DESERT: { sand: [178, 140, 98], sand2: [196, 158, 112], dirt: [206, 174, 130], rock: [124, 96, 72], asph: [72, 68, 64], water: [44, 70, 76], roof: [216, 202, 176], tree: [74, 84, 46] },
  URBAN: { sand: [150, 140, 126], sand2: [166, 156, 140], dirt: [178, 166, 146], rock: [110, 104, 98], asph: [64, 64, 66], water: [42, 64, 74], roof: [198, 194, 186], tree: [70, 86, 52] },
  COAST: { sand: [188, 162, 118], sand2: [204, 180, 134], dirt: [214, 190, 146], rock: [126, 108, 86], asph: [70, 68, 66], water: [36, 82, 98], roof: [222, 214, 196], tree: [66, 96, 52] },
  SNOW: { sand: [214, 222, 230], sand2: [232, 238, 244], dirt: [176, 178, 180], rock: [104, 108, 116], asph: [80, 82, 88], water: [52, 74, 92], roof: [150, 120, 100], tree: [52, 72, 62] },
  FOREST: { sand: [92, 112, 62], sand2: [108, 128, 72], dirt: [146, 128, 92], rock: [112, 108, 96], asph: [72, 72, 70], water: [40, 70, 80], roof: [190, 170, 150], tree: [44, 66, 38] },
  MOUNTAIN: { sand: [150, 132, 108], sand2: [166, 148, 122], dirt: [180, 160, 128], rock: [104, 92, 82], asph: [70, 68, 66], water: [44, 70, 82], roof: [204, 190, 170], tree: [66, 80, 50] } };
function hash(x, y) { let h = x * 374761393 + y * 668265263; h = (h ^ (h >> 13)) * 1274126177; return ((h ^ (h >> 16)) & 0xffff) / 0xffff; }
function vn(x, y) { const xi = Math.floor(x), yi = Math.floor(y), xf = x - xi, yf = y - yi, s = t => t * t * (3 - 2 * t);
  const a = hash(xi, yi), b = hash(xi + 1, yi), c = hash(xi, yi + 1), d = hash(xi + 1, yi + 1); return a + (b - a) * s(xf) + (c - a) * s(yf) + (a - b - c + d) * s(xf) * s(yf); }
const fbm = (x, y) => vn(x, y) * .5 + vn(x * 2.03, y * 2.03) * .25 + vn(x * 4.1, y * 4.1) * .125 + vn(x * 8.3, y * 8.3) * .0625;
function blur(a, n, r) { const o = new Float32Array(a.length), t = new Float32Array(a.length);
  for (let y = 0; y < n; y++) { let s = 0; for (let x = -r; x <= r; x++) s += a[y * n + Math.min(n - 1, Math.max(0, x))]; for (let x = 0; x < n; x++) { t[y * n + x] = s / (2 * r + 1); s += a[y * n + Math.min(n - 1, x + r + 1)] - a[y * n + Math.max(0, x - r)]; } }
  for (let x = 0; x < n; x++) { let s = 0; for (let y = -r; y <= r; y++) s += t[Math.min(n - 1, Math.max(0, y)) * n + x]; for (let y = 0; y < n; y++) { o[y * n + x] = s / (2 * r + 1); s += t[Math.min(n - 1, y + r + 1) * n + x] - t[Math.max(0, y - r) * n + x]; } }
  return o; }
function renderMinimap(M, res) {
  const P = PAL[M.terr] || PAL.DESERT, N = M.n || 512, s = M.surf, br = Math.max(1, Math.round(3 * N / 512));
  const hp = new Float32Array(N * N); for (let i = 0; i < N * N; i++) hp[i] = s[i * 4 + 2] / 255 + s[i * 4 + 1] / 255 * .12 - s[i * 4 + 3] / 255 * .5;
  const hb = blur(blur(hp, N, br), N, Math.max(1, br - 1)), gk = 9 * 512 / N;
  const c = document.createElement('canvas'); c.width = c.height = res; const g = c.getContext('2d'), id = g.createImageData(res, res), d = id.data, k = N / res;
  for (let j = 0; j < res; j++) for (let i = 0; i < res; i++) {
    const u = Math.min(N - 2, Math.floor(i * k)), v = Math.min(N - 2, Math.floor(j * k)), q = (v * N + u) * 4;
    const asph = s[q] / 255, dirt = s[q + 1] / 255, rock = s[q + 2] / 255, wet = s[q + 3] / 255;
    const mx = i / res * 6, my = j / res * 6, n1 = fbm(mx + 3, my + 7), n2 = hash(i, j);
    let r = P.sand[0] + (P.sand2[0] - P.sand[0]) * n1, gg = P.sand[1] + (P.sand2[1] - P.sand[1]) * n1, b = P.sand[2] + (P.sand2[2] - P.sand[2]) * n1;
    const rip = .975 + .05 * Math.sin((i / res * 1024 * .9 + j / res * 1024 * .4) * .35 + fbm(mx * 4, my * 4) * 6); r *= rip; gg *= rip; b *= rip;     // as ondinhas da areia
    r += (P.dirt[0] - r) * dirt * .85; gg += (P.dirt[1] - gg) * dirt * .85; b += (P.dirt[2] - b) * dirt * .85;
    const rn = .82 + .36 * fbm(mx * 9, my * 9); r += (P.rock[0] * rn - r) * rock; gg += (P.rock[1] * rn - gg) * rock; b += (P.rock[2] * rn - b) * rock;
    // o relevo: a luz do oeste (no losango vem do alto à esquerda, como nos game shots)
    const gx = hb[v * N + u + 1] - hb[v * N + Math.max(0, u - 1)], gy = hb[(v + 1) * N + u] - hb[Math.max(0, v - 1) * N + u];
    const sh = Math.max(.55, Math.min(1.3, 1 + (gx * -1.0 + gy * -.35) * gk)); r *= sh; gg *= sh; b *= sh;
    r += (P.asph[0] - r) * asph * .95; gg += (P.asph[1] - gg) * asph * .95; b += (P.asph[2] - b) * asph * .95;
    if (wet > .02) { const e = Math.min(1, wet * 1.4), deep = .75 + .25 * fbm(mx * 3, my * 3); r += (P.water[0] * deep - r) * e; gg += (P.water[1] * deep - gg) * e; b += (P.water[2] * deep - b) * e; }
    const gr = .965 + .07 * n2, o = (j * res + i) * 4; d[o] = r * gr; d[o + 1] = gg * gr; d[o + 2] = b * gr; d[o + 3] = 255;
  }
  g.putImageData(id, 0, 0);
  const sc = res / 1024, X = x => (x + 512) * sc, Y = y => (y + 512) * sc;
  // as docas de suprimento: a laje e as caixas
  for (const [x, y, w, h] of M.docks) { g.fillStyle = 'rgba(40,28,18,.25)'; g.fillRect(X(x - w / 2) + 3 * sc, Y(y - h / 2), w * sc, h * sc); g.fillStyle = 'rgb(178,170,152)'; g.fillRect(X(x - w / 2), Y(y - h / 2), w * sc, h * sc);
    for (let a = 0; a < 4; a++) for (let bb = 0; bb < 4; bb++) { const cx = X(x - w / 2 + 3 + a * (w - 6) / 4), cy = Y(y - h / 2 + 3 + bb * (h - 6) / 4); g.fillStyle = 'rgba(40,28,18,.35)'; g.fillRect(cx + 1.2 * sc, cy, 4.2 * sc, 4.2 * sc); g.fillStyle = (a + bb) % 2 ? 'rgb(120,104,68)' : 'rgb(140,122,80)'; g.fillRect(cx, cy, 4.2 * sc, 4.2 * sc); } }
  // a vila: a sombra cai pro leste (no losango, pra baixo à direita) e o telhado claro
  for (const [x, y, w, h, hh, civ] of M.pieces) if (civ) { const off = Math.min(6, hh * .9) * sc; g.fillStyle = 'rgba(46,30,18,.38)'; g.fillRect(X(x - w / 2) + off, Y(y - h / 2) + off * .25, w * sc, h * sc); }
  for (const [x, y, w, h, hh, civ] of M.pieces) {
    if (civ) { const t = .9 + .1 * hash(Math.round(x), Math.round(y)); g.fillStyle = `rgb(${P.roof[0] * t | 0},${P.roof[1] * t | 0},${P.roof[2] * t | 0})`; g.fillRect(X(x - w / 2), Y(y - h / 2), Math.max(1, w * sc), Math.max(1, h * sc));
      if (w * sc > 3 && h * sc > 3) { g.strokeStyle = 'rgba(70,52,36,.35)'; g.lineWidth = Math.max(.6, .5 * sc); g.strokeRect(X(x - w / 2) + .5, Y(y - h / 2) + .5, w * sc - 1, h * sc - 1); } }
    else { g.fillStyle = 'rgba(70,56,40,.55)'; g.fillRect(X(x) - .8 * sc, Y(y) - .8 * sc, Math.max(.8, 1.6 * sc), Math.max(.8, 1.6 * sc)); }
  }
  for (const [x, y] of M.palms) { const r0 = 3.4 * sc; g.fillStyle = 'rgba(40,28,16,.35)'; g.beginPath(); g.ellipse(X(x) + 2.4 * sc, Y(y) + .6 * sc, r0, r0 * .8, 0, 0, 7); g.fill();
    g.strokeStyle = `rgb(${P.tree})`; g.lineWidth = Math.max(.8, 1.1 * sc); g.beginPath(); for (let a = 0; a < 7; a++) { const an = a / 7 * 6.283 + x; g.moveTo(X(x), Y(y)); g.lineTo(X(x) + Math.cos(an) * r0, Y(y) + Math.sin(an) * r0); } g.stroke(); }
  for (const [x, y] of M.poles) { g.fillStyle = 'rgba(50,40,30,.8)'; g.fillRect(X(x) - .6, Y(y) - .6, 1.4, 1.4); }
  // as condições do mapa (vêm da criação: a hora, o céu, a tempestade)
  if (M.cond === 'DUSK') { g.globalCompositeOperation = 'multiply'; g.fillStyle = 'rgb(236,196,160)'; g.fillRect(0, 0, res, res); g.globalCompositeOperation = 'source-over'; }
  if (M.cond === 'NIGHT') { g.globalCompositeOperation = 'multiply'; g.fillStyle = 'rgb(84,98,140)'; g.fillRect(0, 0, res, res); g.globalCompositeOperation = 'lighter';
    for (const [x, y, w, h, hh, civ] of M.pieces) if (civ && hash(x | 0, y | 0) > .6) { g.fillStyle = 'rgba(255,190,110,.5)'; g.fillRect(X(x) - .8, Y(y) - .8, 1.8, 1.8); } g.globalCompositeOperation = 'source-over'; }
  if (M.cond === 'STORM') { const gr = g.createLinearGradient(0, 0, res, res); gr.addColorStop(0, 'rgba(160,120,80,0)'); gr.addColorStop(1, 'rgba(160,120,80,.35)'); g.fillStyle = gr; g.fillRect(0, 0, res, res); }
  return c;
}
// ---------- os mapas: o de verdade vem do jogo; os outros, gerados pela semente (o mesmo caminho do NEW WORLD) ----------
const MAPS = {};
async function loadReal(name) {
  const im = src => new Promise(res => { const i = new Image(); i.onload = () => res(i); i.src = src; });
  const [data, img, wimg] = await Promise.all([fetch(`maps/${name}.json`).then(r => r.json()), im(`maps/${name}.png`), im(`maps/${name}_water.png`)]);
  const c = document.createElement('canvas'); c.width = c.height = 512; const g = c.getContext('2d', { willReadFrequently: true });
  g.drawImage(img, 0, 0); const px = g.getImageData(0, 0, 512, 512).data; g.drawImage(wimg, 0, 0); const wp = g.getImageData(0, 0, 512, 512).data;
  for (let k = 0; k < px.length; k += 4) px[k + 3] = wp[k];
  return { surf: px, pieces: data.pieces, palms: data.palms, poles: data.poles, docks: data.docks, starts: data.starts };
}
function genMap(e, N = 512) {     // um mapa pela semente: as bases simétricas, os caminhos até o meio, a vila, a água e a rocha
  const r = rng(e.seed), n = e.players, R = 360, k = N / 1024;
  const corners = [[-1, -1], [1, 1], [-1, 1], [1, -1]], mids = [[0, -1], [0, 1], [-1, 0], [1, 0]];
  const order = n === 2 ? [corners[0], corners[1]] : n === 4 ? corners : n === 6 ? [...corners, mids[2], mids[3]] : [...corners, ...mids];
  const starts = order.map(([a, b]) => (a && b) ? [a * R * .82, b * R * .82] : [a * R * 1.05, b * R * 1.05]);
  const c = document.createElement('canvas'); c.width = c.height = N; const g = c.getContext('2d', { willReadFrequently: true });
  g.fillStyle = '#000'; g.fillRect(0, 0, N, N); const P = (x, y) => [(x + 512) * k, (y + 512) * k];
  g.lineCap = 'round'; g.strokeStyle = 'rgb(0,255,0)';            // a terra batida: os caminhos das bases ao meio
  for (const [x, y] of starts) { g.lineWidth = Math.max(1, (6 + r() * 6) * k); g.beginPath(); g.moveTo(...P(x, y)); g.quadraticCurveTo(...P(x * .5 + (r() - .5) * 120, y * .5 + (r() - .5) * 120), ...P(0, 0)); g.stroke();
    g.fillStyle = 'rgb(0,200,0)'; g.beginPath(); g.arc(...P(x, y), (52 + r() * 20) * k, 0, 7); g.fill(); }
  if (e.terr !== 'FOREST' || r() < .5) { g.strokeStyle = 'rgb(255,0,0)'; g.lineWidth = Math.max(1, 8 * k); g.beginPath(); const a = r() * Math.PI; g.moveTo(...P(Math.cos(a) * 560, Math.sin(a) * 560)); g.quadraticCurveTo(...P((r() - .5) * 200, (r() - .5) * 200), ...P(-Math.cos(a) * 560, -Math.sin(a) * 560)); g.stroke(); }
  const px = g.getImageData(0, 0, N, N).data, surf = new Uint8ClampedArray(N * N * 4);
  const kind = e.terr, rockAmt = kind === 'MOUNTAIN' ? .42 : kind === 'URBAN' ? .58 : .52, ox = r() * 100, oy = r() * 100;
  const coast = kind === 'COAST', lake = !coast && r() < .45, la = r() * 6.28, lr = 140 + r() * 120;
  for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
    const q = (j * N + i) * 4, x = (i + .5) / k - 512, y = (j + .5) / k - 512, f = fbm(x / 120 + ox, y / 120 + oy);
    let rock = Math.max(0, Math.min(1, (f - rockAmt) * 7)); if (starts.some(([sx, sy]) => Math.hypot(x - sx, y - sy) < 110) || Math.hypot(x, y) < 120) rock *= .1;
    if (Math.max(Math.abs(x), Math.abs(y)) > 470) rock = Math.max(rock, .8 * fbm(x / 16, y / 16) + .3);
    let wet = 0; if (coast) wet = (x * Math.cos(la) + y * Math.sin(la)) - 300 - fbm(x / 80, y / 80) * 120 > 0 ? 1 : 0;
    if (lake) wet = Math.hypot(x - Math.cos(la) * lr, y - Math.sin(la) * lr) - 60 - fbm(x / 60, y / 60) * 60 < 0 ? 1 : 0;
    surf[q] = px[q] * (1 - wet); surf[q + 1] = px[q + 1] * (1 - wet); surf[q + 2] = rock * 255 * (1 - wet); surf[q + 3] = wet * 255;
  }
  const pieces = [], palms = [], docks = [], nh = kind === 'URBAN' ? 150 : 60 + Math.floor(r() * 50);
  for (let i = 0; i < nh; i++) { const a = r() * 6.28, d = Math.sqrt(r()) * (kind === 'URBAN' ? 260 : 120); const x = Math.round((Math.cos(a) * d) / 12) * 12 + (r() - .5) * 3, y = Math.round((Math.sin(a) * d) / 12) * 12 + (r() - .5) * 3;
    pieces.push([x, y, 7 + r() * 7, 6 + r() * 6, 3 + r() * 6, 1]); if (r() < .5) pieces.push([x + (r() - .5) * 14, y + (r() - .5) * 14, 1.5, 1.5, 1, 0]); }
  for (let i = 0; i < 40; i++) { const a = r() * 6.28, d = 60 + r() * 380; palms.push([Math.cos(a) * d, Math.sin(a) * d]); }
  starts.forEach(([x, y]) => docks.push([x * .78 + (r() - .5) * 40, y * .78 + (r() - .5) * 40, 26, 30]));
  docks.push([(r() - .5) * 60 + 90, (r() - .5) * 60 - 90, 26, 30], [(r() - .5) * 60 - 90, (r() - .5) * 60 + 90, 26, 30]);
  return { n: N, surf, pieces, palms, poles: [], docks, starts };
}
async function getMap(e) { if (MAPS[e.id]) return MAPS[e.id]; const raw = e.real ? await loadReal(e.real) : genMap(e); return MAPS[e.id] = Object.assign(raw, { terr: e.terr, cond: e.cond }); }
