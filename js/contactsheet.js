// ---------------------------------------------------------------------------
// contactsheet.js — captura a evolução temporal da explosão numa folha de
// contato, com a câmera TRAVADA.
//
// Avaliar uma explosão olhando o loop rodando não funciona: os quadros
// interessantes passam em 200ms e a câmera cinemática muda o enquadramento
// junto, o que confunde mudança de simulação com mudança de ponto de vista.
// Aqui a simulação roda uma única vez e é fotografada nos tempos pedidos.
// ---------------------------------------------------------------------------

const DEFAULT_TIMES = [0.04, 0.09, 0.16, 0.26, 0.40, 0.60,
                       0.85, 1.20, 1.70, 2.40, 3.40, 5.00];

export function contactSheet(app, opts = {}) {
  const {
    times = DEFAULT_TIMES,
    cols = 4,
    thumbW = 380,
    seed = 1234,
    dt = 1 / 60,
    view = { dist: 72, azim: -0.62, elev: 0.135, targetY: 15 },
    syncEvery = 8,
  } = opts;

  const gl = app.gl;
  const aspect = app.rw / app.rh;
  const thumbH = Math.round(thumbW / aspect);
  const rows = Math.ceil(times.length / cols);

  const sheet = document.createElement('canvas');
  sheet.width = cols * thumbW;
  sheet.height = rows * thumbH;
  const ctx = sheet.getContext('2d');
  ctx.fillStyle = '#07070a';
  ctx.fillRect(0, 0, sheet.width, sheet.height);

  // ---- trava a câmera e o replay ----
  const saved = {
    mode: app.cam.mode, frozen: app.cam.frozen, auto: app.env.autoReplay,
    dist: app.cam.dist, azim: app.cam.azim, elev: app.cam.elev, ty: app.cam.targetY,
  };
  app.env.autoReplay = false;
  app.cam.mode = 'free';
  app.cam.frozen = true;
  app.cam.dist = view.dist; app.cam.azim = view.azim;
  app.cam.elev = view.elev; app.cam.targetY = view.targetY;

  app.detonate([0, 1.85, 0], seed);
  app.cam.shake = 0;

  let step = 0;
  times.forEach((tTarget, i) => {
    while (app.time < tTarget) {
      app.frameStep(dt);
      // a luz do fogo precisa de leitura bloqueante numa rajada síncrona
      if (step++ % syncEvery === 0) app.pool.active.forEach((b) => app.vol.syncFireLight(b));
    }
    app.pool.active.forEach((b) => app.vol.syncFireLight(b));
    app.frameStep(dt);

    const cx = (i % cols) * thumbW;
    const cy = Math.floor(i / cols) * thumbH;
    ctx.drawImage(app.canvas, cx, cy, thumbW, thumbH);

    ctx.font = '600 15px ui-monospace, Menlo, monospace';
    ctx.textBaseline = 'top';
    ctx.fillStyle = 'rgba(0,0,0,.65)';
    ctx.fillRect(cx + 6, cy + 6, 74, 21);
    ctx.fillStyle = '#ffe9c8';
    ctx.fillText(`${app.time.toFixed(2)}s`, cx + 12, cy + 9);
    ctx.strokeStyle = 'rgba(255,255,255,.10)';
    ctx.strokeRect(cx + 0.5, cy + 0.5, thumbW - 1, thumbH - 1);
  });

  // ---- restaura ----
  app.cam.mode = saved.mode; app.cam.frozen = saved.frozen;
  app.env.autoReplay = saved.auto;
  app.cam.dist = saved.dist; app.cam.azim = saved.azim;
  app.cam.elev = saved.elev; app.cam.targetY = saved.ty;

  return sheet;
}

/** mostra a folha em tela cheia, por cima de tudo (pra screenshot) */
export function showSheet(sheet) {
  let host = document.getElementById('sheet');
  if (!host) {
    host = document.createElement('div');
    host.id = 'sheet';
    host.style.cssText = 'position:fixed;inset:0;z-index:999;background:#07070a;'
      + 'display:flex;align-items:center;justify-content:center;overflow:auto';
    host.addEventListener('click', () => host.remove());
    document.body.appendChild(host);
  }
  host.innerHTML = '';
  sheet.style.cssText = 'max-width:100%;max-height:100%;object-fit:contain';
  host.appendChild(sheet);
  return host;
}

/**
 * Envia a folha pro dev server (POST /save). Permite inspecionar o resultado
 * sem depender de a janela do app estar visível ou em foco.
 */
export function saveSheet(sheet, name = 'sheet.png') {
  return new Promise((resolve, reject) => {
    sheet.toBlob((blob) => {
      fetch(`/save?name=${encodeURIComponent(name)}`, { method: 'POST', body: blob })
        .then((r) => r.text()).then(resolve).catch(reject);
    }, 'image/png');
  });
}

/**
 * Folha de diagnóstico: um instante, vários CAMPOS lado a lado.
 * Olhar a imagem final não distingue "a simulação está lisa" de "o
 * renderizador está escondendo a estrutura". Isto distingue.
 */
export function fieldSheet(app, opts = {}) {
  const {
    t = 0.2, cols = 3, thumbW = 420, seed = 1234, dt = 1 / 60,
    modes = [[0, 'final'], [2, 'temperatura'], [1, 'fuligem'],
             [3, 'combustível'], [5, 'frente de chama'], [4, 'emissão']],
    view = { dist: 30, azim: -0.62, elev: 0.10, targetY: 4.5 },
  } = opts;

  const aspect = app.rw / app.rh;
  const thumbH = Math.round(thumbW / aspect);
  const rows = Math.ceil(modes.length / cols);
  const sheet = document.createElement('canvas');
  sheet.width = cols * thumbW;
  sheet.height = rows * thumbH;
  const ctx = sheet.getContext('2d');
  ctx.fillStyle = '#07070a';
  ctx.fillRect(0, 0, sheet.width, sheet.height);

  const saved = { mode: app.cam.mode, frozen: app.cam.frozen, auto: app.env.autoReplay,
                  dist: app.cam.dist, azim: app.cam.azim, elev: app.cam.elev, ty: app.cam.targetY,
                  debug: app.vol.params.debug };
  app.env.autoReplay = false;
  app.cam.mode = 'free'; app.cam.frozen = true;
  app.cam.dist = view.dist; app.cam.azim = view.azim;
  app.cam.elev = view.elev; app.cam.targetY = view.targetY;

  // simula UMA vez até t, guardando o estado, e re-renderiza em cada modo
  app.detonate([0, 1.85, 0], seed);
  app.cam.shake = 0;
  let step = 0;
  while (app.time < t) {
    app.frameStep(dt);
    if (step++ % 8 === 0) app.pool.active.forEach((b) => app.vol.syncFireLight(b));
  }
  app.pool.active.forEach((b) => app.vol.syncFireLight(b));

  modes.forEach(([mode, label], i) => {
    app.vol.params.debug = mode;
    app.frameStep(0);            // re-renderiza o MESMO estado
    const cx = (i % cols) * thumbW;
    const cy = Math.floor(i / cols) * thumbH;
    ctx.drawImage(app.canvas, cx, cy, thumbW, thumbH);
    ctx.font = '600 16px ui-monospace, Menlo, monospace';
    ctx.textBaseline = 'top';
    ctx.fillStyle = 'rgba(0,0,0,.7)';
    ctx.fillRect(cx + 6, cy + 6, label.length * 10 + 16, 23);
    ctx.fillStyle = '#ffe9c8';
    ctx.fillText(label, cx + 13, cy + 10);
    ctx.strokeStyle = 'rgba(255,255,255,.12)';
    ctx.strokeRect(cx + 0.5, cy + 0.5, thumbW - 1, thumbH - 1);
  });

  app.vol.params.debug = saved.debug;
  app.cam.mode = saved.mode; app.cam.frozen = saved.frozen;
  app.env.autoReplay = saved.auto;
  app.cam.dist = saved.dist; app.cam.azim = saved.azim;
  app.cam.elev = saved.elev; app.cam.targetY = saved.ty;
  return sheet;
}

/** um único frame grande, pra julgar QUALIDADE em vez de evolução */
export function heroShot(app, opts = {}) {
  const { t = 1.0, w = 1100, seed = 1234, dt = 1 / 60,
          view = { dist: 46, azim: -0.62, elev: 0.10, targetY: 8 } } = opts;
  const aspect = app.rw / app.rh;
  const sheet = document.createElement('canvas');
  sheet.width = w;
  sheet.height = Math.round(w / aspect);
  const ctx = sheet.getContext('2d');

  const saved = { mode: app.cam.mode, frozen: app.cam.frozen, auto: app.env.autoReplay,
                  dist: app.cam.dist, azim: app.cam.azim, elev: app.cam.elev, ty: app.cam.targetY };
  app.env.autoReplay = false;
  app.cam.mode = 'free'; app.cam.frozen = true;
  Object.assign(app.cam, { dist: view.dist, azim: view.azim, elev: view.elev, targetY: view.targetY });
  app.detonate([0, 1.85, 0], seed);
  app.cam.shake = 0;
  let step = 0;
  while (app.time < t) {
    app.frameStep(dt);
    if (step++ % 6 === 0) app.pool.active.forEach((b) => app.vol.syncFireLight(b));
  }
  app.pool.active.forEach((b) => app.vol.syncFireLight(b));
  app.frameStep(dt);
  ctx.drawImage(app.canvas, 0, 0, sheet.width, sheet.height);

  app.cam.mode = saved.mode; app.cam.frozen = saved.frozen;
  app.env.autoReplay = saved.auto;
  Object.assign(app.cam, { dist: saved.dist, azim: saved.azim, elev: saved.elev, targetY: saved.ty });
  return sheet;
}
