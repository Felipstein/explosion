// ---------------------------------------------------------------------------
// main.js — orquestração: simulação → cena → volume → composite → post.
// ---------------------------------------------------------------------------

import { initGL, Target, MRTarget, drawFS, trackGL, createTexture } from './gl.js';
import { FluidSim } from './fluid.js';
import { BlastPool, MAX_BLASTS, MAX_SHADED } from './blastpool.js';
import { Scene } from './scene.js';
import { VolumeRenderer } from './volumeRender.js';
import { Post } from './post.js';
import { SSAO } from './ssao.js';
import { ShadowDenoise } from './shadowdenoise.js';
import { BattleSmoke } from './battlesmoke.js';
import { Movers } from './movers.js';
import { ExplosionBake } from './bake.js';
import { BlastInstances, MAGNITUDES, magnitudeOf } from './instances.js';
import { InstanceSparks } from './sparks.js';
import { AutoExposure } from './exposure.js';
import { loadSettings, saveSettings, matchPreset, PRESETS, PRESET_NAMES, ITEMS, SCHEMA,
         bakeVramMB } from './settings.js';
import { Camera } from './camera.js';
import { buildBlackbodyLUT, uploadBlackbodyLUT } from './blackbody.js';
import { Atmosphere } from './atmosphere.js';
import { sunPosition, moonPosition, EXPOSURE_BASE, EXPOSURE_REF, EXPOSURE_ALPHA,
         SUN_ILLUMINANCE, SUN_LUX, MOON_ILLUMINANCE, MOON_PHYS_RATIO, MOON_TINT,
         NIGHT_GLOW_LUX } from './celestial.js';
import { clamp, smoothstep } from './math.js';
import { GPUProfiler } from './profiler.js';
import { contactSheet, showSheet, saveSheet, fieldSheet, heroShot } from './contactsheet.js';

// Parâmetros do solver AO VIVO por resolução de grade. O resto da qualidade
// (render, bake, luz, pós) vem das configurações — ver settings.js.
const SIM_TIERS = {
  64:  { coarse: 10, fine: 2, steps: 44,  light: 12, mac: false, det2: 0,    slots: 8 },
  96:  { coarse: 14, fine: 3, steps: 60,  light: 16, mac: false, det2: 0,    slots: 6 },
  128: { coarse: 16, fine: 3, steps: 76,  light: 20, mac: true,  det2: 0,    slots: 5 },
  160: { coarse: 22, fine: 5, steps: 128, light: 26, mac: true,  det2: 0.11, slots: 3 },
};

const DOMAIN = 38.0;
const BAKE_FRAMES = 56, BAKE_DURATION = 6.0;   // o cogumelo escapava de um domínio de 31m em ~2.4s
// O bake é um ASSET: resolução e solver fixos, independentes da qualidade
// escolhida. Antes ele usava o slot 0 do pool e herdava a qualidade do boot.
const BAKE_SIM = 128;
const BAKE_SOLVER = { pressureCoarse: 16, pressureFine: 3, velMacCormack: true, lightSteps: 20 };
const BAKE_SEED = 4242;
// direção de luz fixa: o conteúdo assado (fuligem, calor, poeira, céu) não
// depende do sol, e assim o asset não depende da hora em que a página abriu
const BAKE_KEY_DIR = [0.42, 0.72, -0.55];
// fontes que determinam o resultado do bake: mudou alguma, o cache é outro
const BAKE_SOURCES = ['fluid.js', 'glsl.js', 'volume.js', 'bake.js', 'volumeRender.js', 'blackbody.js'];
const BAKE_LIGHT_PARAMS = ['emissionCurve', 'tempScale', 'sootExt', 'sootAlbedo',
                           'flameBoost', 'lightScale', 'dustExt', 'erode'];

// hash de 53 bits (cyrb53): basta pra nomear o asset, sem depender de
// crypto.subtle, que só existe em contexto seguro
function hash53(str, seed = 0) {
  let h1 = 0xdeadbeef ^ seed, h2 = 0x41c6ce57 ^ seed;
  for (let i = 0; i < str.length; i++) {
    const c = str.charCodeAt(i);
    h1 = Math.imul(h1 ^ c, 2654435761);
    h2 = Math.imul(h2 ^ c, 1597334677);
  }
  h1 = Math.imul(h1 ^ (h1 >>> 16), 2246822507) ^ Math.imul(h2 ^ (h2 >>> 13), 3266489909);
  h2 = Math.imul(h2 ^ (h2 >>> 16), 2246822507) ^ Math.imul(h1 ^ (h1 >>> 13), 3266489909);
  return (4294967296 * (2097151 & h2) + (h1 >>> 0)).toString(16).padStart(14, '0');
}

export const ENV = {
  // ---- hora do dia ----
  timeOfDay: 18.55,       // horas decimais [0,24)
  latitude: -23.5,        // graus (Trópico de Capricórnio)
  dayOfYear: 172,
  autoCycle: false,
  cycleSpeed: 0.35,       // horas por segundo

  sunIlluminance: SUN_ILLUMINANCE,
  moonIlluminance: MOON_ILLUMINANCE,
  moonTint: MOON_TINT,
  starBright: 1.0,
  nightGlow: 1.0,
  moonBright: 1.0,
  exposureBias: 1.0,

  ambient: 1.0,
  aoFloor: 0.38,
  aoDebug: false,      // peso mínimo do AO mesmo sob luz direta
  // A cor da névoa agora vem da LUT do céu, que de dia é fisicamente
  // brilhante — a densidade antiga foi afinada contra um céu falso e escuro,
  // e lavava a cena inteira de branco.
  fogDensity: 0.0016,
  fogFalloff: 0.090,
  fireOcclude: 0.95,
  fireFill: 0.085,   // espalhamento múltiplo da luz do fogo (sem sombra)
  fireTauCap: 3.2,   // profundidade óptica máx. da sombra do fogo (e^-3.2 ≈ 4%)
  fogFireGain: 0.010,
  // exposição analítica: calculada na GPU a partir da iluminância da hora
  // (atmosphere.js texel 4, curva em celestial.js); exposureBias multiplica
  expAlpha: EXPOSURE_ALPHA,
  // Purkinje shift (post.js): visão de bastonetes no escuro, Ghost of Tsushima
  purkinje: true,
  purkS: 1.0e5,       // resposta dos cones por cd/m², calibrada no slide 173 do Patry
  // Adaptação às explosões (exposure.js): fecha até aeMaxEV stops quando uma
  // bola de fogo grande domina o quadro, rápido; reabre devagar.
  autoExposure: true,
  aeTarget: 0.45,     // limiar: as cenas sem explosão medem 0.03–0.29 (pôr do sol);
                      // o chão perto de uma bola de fogo recebe ~1 stop a mais
                      // que ao meio-dia, e é nesse nível que ele assenta
  aePower: 0.5,       // p da média: explosão pequena e longe quase não pesa
  aeClamp: 1e5,       // sem teto: o céu já fica fora do medidor
  aeExpDay: EXPOSURE_BASE * 1.14,  // exposição do meio-dia (11.4)
  aeMaxEV: 8.0,       // trava de segurança; o alcance real vem da exposição da hora
  aeAttack: 0.08,     // s
  aeRelease: 1.60,    // s
  // flash da detonação: pulso curto de luz na cena antes da exposição reagir
  fireRad: 3.5,       // raio da fonte extensa da explosão ao vivo (m), pras sombras
  flashOn: true,
  flashGain: 180.0,   // mesma unidade da curva de luz assada (pico ~150)
  flashTau: 0.035,    // s de sequência
  bloomThreshold: 1.70,
  bloomKnee: 0.55,
  bloomClamp: 90.0,
  bloomStrength: 0.052,
  bloomRadius: 1.15,
  streakStrength: 0.026,
  chromatic: 0.0022,
  vignette: 0.38,
  grain: 0.0062,
  huePreserve: 0.45,  // quanto da crominância volta sobre o highlight
  saturation: 1.16,
  contrast: 1.06,
  lift: 0.0,          // o "lift" azul das sombras era um Purkinje falso; o de verdade está no post
  timeScale: 1.0,
  adaptive: true,     // resolução dinâmica
  maxClickRange: 260, // m — além disso o clique é ignorado
  targetMs: 18.0,     // orçamento de frame
  adaptCeiling: 90.0, // acima disso é throttling do navegador, não GPU
  autoReplay: false,  // era resto da fase de explosão única: detonava sozinho no centro
  replayAfter: 15.0,
};

class App {
  constructor(canvas) {
    this.canvas = canvas;
    this.gl = initGL(canvas);
    const gl = this.gl;

    // LUT de corpo negro (Planck × CIE) — calculada na CPU uma vez
    const t0 = performance.now();
    this.lut = buildBlackbodyLUT(3600, 1024);
    this.bbTex = uploadBlackbodyLUT(gl, this.lut);
    this.lutMs = performance.now() - t0;

    // configuração gráfica salva no navegador (ou o preset alta)
    this.settings = loadSettings();
    this.env = { ...ENV };
    this.frame = 0;
    this.time = 0;
    this.paused = false;
    this.stats = { fps: 0, ms: 0, sim: 0 };
    this._acc = [];

    this.bootLog = [];
    const prof = /bootprof/.test(location.search);
    let tb = performance.now();
    this._mark = (name) => {
      if (prof) this._gpuSync();
      const n = performance.now(); this.bootLog.push([name, +(n - tb).toFixed(1)]); tb = n;
    };
    this.cam = new Camera();
    this.prof = new GPUProfiler(gl);
    // flags de ablação: desligar um estágio e medir o frame time é a única
    // forma honesta de atribuir custo. Timer query sozinho mede drenagem de
    // fila junto com o trabalho real.
    this.skip = {};
    this.dynScale = 1.0;
    this._ring = new Float64Array(48);
    this._ringI = 0;
    // LUTs de transmitância e multi-espalhamento: calculadas uma vez
    this.atmo = new Atmosphere(gl);
    this.sun = null; this.moon = null; this._lastKeyDir = null;
    this._mark('lut');
    this.post = new Post(gl, 8, 8);
    this.ssao = new SSAO(gl);
    this.shadowDenoise = new ShadowDenoise(gl);
    this._mark('atmo+post+ssao');
    // AO cinza 1×1: o composite amostra a oclusão sempre; com AO desligado
    // ele precisa de "sem oclusão", não do último AO calculado
    this._noAO = createTexture(gl, 1, 1, {
      internalFormat: gl.RGBA8, format: gl.RGBA, type: gl.UNSIGNED_BYTE,
      filter: gl.NEAREST, data: new Uint8Array([255, 255, 255, 255]),
    });
    this.sparks = new InstanceSparks(gl, this.bbTex);
    this.movers = new Movers();
    this.ae = new AutoExposure(gl);
    // Bake + cena + instâncias. O bake sobrevive à troca de qualidade da
    // simulação ao vivo; só a opção "detalhe das explosões" o recria. NADA
    // de bake síncrono: o primeiro frame sai já, e o arsenal carrega (ou
    // assa) em segundo plano — ver tickBake()
    this._rebuildBake(this.settings.bakeRes);
    this._mark('buildSim');
    this.resize();
    this._updateCelestial(true);
    this._applyLiveSettings();
    this._mark('celestial');

    this._bindInput();
  }

  // ---- bake: asset em cache, ou simulação incremental ---------------------

  async _bakeKey() {
    const src = await Promise.all(BAKE_SOURCES.map((f) =>
      fetch(new URL(`./${f}`, import.meta.url)).then((r) => r.text())));
    const P = {};
    for (const k of BAKE_LIGHT_PARAMS) P[k] = this.vol.params[k];
    return hash53(src.join('\n') + JSON.stringify({
      BAKE_RES: this.bake.grid.nx, BAKE_FRAMES, BAKE_DURATION, BAKE_SIM, BAKE_SOLVER,
      BAKE_SEED, BAKE_KEY_DIR,
      DOMAIN, variants: this.bake.variants, curve: this.bake.curve, P,
    }));
  }

  /**
   * Tenta o asset pronto (assets/bake/<hash>.bin, gzip); se não houver ou
   * estiver velho, assa incrementalmente e salva o asset no fim. Na prática
   * o bake só roda depois que o código do solver muda.
   */
  get bakeAsset() { return `bake/r${this.bake.grid.nx}-${this.bakeKey}.bin`; }

  async _loadOrBake() {
    const t0 = performance.now();
    // a resolução pode mudar no meio de um await: cada carga tem um número
    // e uma carga superada desiste em vez de escrever no bake novo
    const gen = this._bakeLoadId = (this._bakeLoadId || 0) + 1;
    const bake = this.bake;
    try {
      const key = await this._bakeKey();
      if (gen !== this._bakeLoadId) return;
      this.bakeKey = key;
      // ?rebake ignora o asset (pra ver/medir o caminho do bake)
      const r = /rebake/.test(location.search) ? { ok: false }
        : await fetch(`assets/${this.bakeAsset}`);
      if (gen !== this._bakeLoadId) return;
      if (r.ok) {
        this.bakeStatus = { phase: 'carregando', progress: 0.5 };
        const raw = await new Response(r.body.pipeThrough(new DecompressionStream('gzip')))
          .arrayBuffer();
        if (gen !== this._bakeLoadId || bake !== this.bake) return;
        this.bake.unpack(new Uint8Array(raw));
        this.bakeStatus = { phase: 'pronto', progress: 1, source: 'asset',
                            ms: Math.round(performance.now() - t0) };
        this._mark && this._mark('bake(asset)');
        this._onVariantReady();
        return;
      }
    } catch (e) {
      console.warn('bake: asset indisponível, assando de novo —', e);
    }
    if (gen !== this._bakeLoadId) return;
    this._beginBakeJob();
  }

  _beginBakeJob() {
    const gl = this.gl;
    // Solver e redutor de luz DEDICADOS, num escopo de recursos próprio que
    // é liberado no fim: o pool continua livre pra explosões ao vivo e a
    // troca de qualidade no meio do bake não interfere nele.
    this._bakeGen = trackGL(gl, () => {
      const fluid = new FluidSim(gl, BAKE_SIM, DOMAIN);
      Object.assign(fluid.params, BAKE_SOLVER);
      const probe = new VolumeRenderer(gl, fluid.grid, fluid.lightGrid, fluid.macroGrid, this.bbTex);
      for (const k of BAKE_LIGHT_PARAMS) probe.params[k] = this.vol.params[k];
      this.bake.begin(fluid, probe, {
        keyDir: BAKE_KEY_DIR,
        sootExt: probe.params.sootExt, dustExt: probe.params.dustExt,
        erodeMean: probe.params.erode * 0.5,
        pos: [0, 1.85, 0], seed: BAKE_SEED,
      });
    });
    this.bakeStatus = { phase: 'assando', progress: 0, source: 'sim' };
    this._bakeT0 = performance.now();
    this._bakeSteps = 2;
  }

  /**
   * Chamado uma vez por frame, antes do render. Avança o bake com orçamento
   * adaptativo: o intervalo entre frames reflete a fila da GPU, então se o
   * frame passa de ~28ms o bake cede passos; abaixo de ~20ms, pega mais.
   */
  tickBake(realDt, bulk = 0) {
    const st = this.bakeStatus;
    if (st.phase === 'assando') {
      if (!bulk) {
        if (realDt > 0.028) this._bakeSteps = Math.max(1, this._bakeSteps - 1);
        else if (realDt < 0.020) this._bakeSteps = Math.min(10, this._bakeSteps + 1);
      }
      const before = this.bake.variantsReady;
      const done = this.bake.advance(bulk || this._bakeSteps);
      st.progress = this.bake.progress;
      if (this.bake.variantsReady > before && before === 0) this._onVariantReady();
      if (done) {
        this._bakeGen.dispose();
        this._bakeGen = null;
        st.ms = Math.round(performance.now() - this._bakeT0);
        this._mark && this._mark('bake(sim)');
        // grava o asset em fatias: ler 168 camadas de uma vez travaria ~0.3s
        this._save = { buf: new Uint8Array(this.bake.layers * this.bake.layerBytes),
                       fuel: new Uint8Array(this.bake.layers * this.bake.fuelLayerBytes), l: 0 };
        st.phase = 'salvando';
      }
      if (this.onBakeProgress) this.onBakeProgress(st);
    } else if (st.phase === 'salvando') {
      const S = this._save, n = Math.min(this.bake.layers, S.l + (bulk ? 1e9 : 12));
      this.bake.readLayers(S.l, n, S.buf);
      this.bake.readFuelLayers(S.l, n, S.fuel);
      S.l = n;
      if (S.l >= this.bake.layers) {
        st.phase = 'pronto';
        const packed = this.bake.pack(S.buf, S.fuel, { key: this.bakeKey });
        this._save = null;
        this._uploadBake(packed);
        if (this.onBakeProgress) this.onBakeProgress(st);
      }
    }
  }

  async _uploadBake(packed) {
    // o nome é fixado ANTES do await: a resolução pode mudar durante a compressão
    const name = this.bakeAsset;
    try {
      const gz = await new Response(new Blob([packed]).stream()
        .pipeThrough(new CompressionStream('gzip'))).blob();
      const r = await fetch(`/asset?name=${name}`, { method: 'POST', body: gz });
      this.bakeStatus.saved = r.ok ? gz.size : 0;
    } catch (e) {
      // servidor estático sem endpoint de escrita: o bake funciona igual,
      // só não fica em cache pra próxima carga
      this.bakeStatus.saved = 0;
    }
  }

  /** a primeira variante ficou pronta: dá pra instanciar e fazer os ícones */
  _onVariantReady() {
    // thumbnails só depois do bake: elas renderizam instâncias dele
    const tod = this.env.timeOfDay;
    // fim de tarde, não crepúsculo: com a luz da instância igual à da
    // simulação ao vivo, a exposição alta do crepúsculo estourava o chão
    this.setTimeOfDay(17.0);
    this.thumbs = {};
    for (const m of MAGNITUDES) this.thumbs[m.id] = this.makeThumb(m.id);
    this.setTimeOfDay(tod);
    this._mark && this._mark('thumbs');
    if (this.onBake) this.onBake(this.bake, this.thumbs);
  }

  /**
   * Thumbnail de uma magnitude: renderiza uma instância dela no próprio
   * motor. O ícone é o efeito de verdade, não arte à parte — se o visual
   * mudar, o ícone muda junto.
   */
  makeThumb(magId, w = 208) {
    const M = magnitudeOf(magId);
    const saved = { mode: this.cam.mode, frozen: this.cam.frozen, dist: this.cam.dist,
                    azim: this.cam.azim, elev: this.cam.elev, ty: this.cam.targetY,
                    center: this.cam.center, list: this.inst.list,
                    scorches: this.inst.scorches };
    // a instância da thumbnail não pode deixar queimado no terreno de verdade
    this.inst.scorches = [];
    // nem mostrar faíscas de explosões que estejam acontecendo no jogo
    const skipP = this.skip.particles;
    this.skip.particles = true;
    this.cam.mode = 'free'; this.cam.frozen = true;
    this.cam.center = [0, 0, 0];
    Object.assign(this.cam, { dist: 27 * M.scale, azim: -0.55,
                              elev: 0.055, targetY: 5.4 * M.scale });
    this.inst.list = [];
    const o = this.inst.spawn([0, 0, 0], { magnitude: magId, seed: 0.37, jitter: 0 });
    o.variant = 0;
    o.t = 0.85;
    this.frameStep(1 / 60);
    const c = document.createElement('canvas');
    c.width = w; c.height = Math.round(w * 3 / 4);
    const srcH = this.canvas.width * 3 / 4;
    c.getContext('2d').drawImage(this.canvas, 0, (this.canvas.height - srcH) * 0.42,
                                 this.canvas.width, srcH, 0, 0, c.width, c.height);
    this.inst.list = saved.list;
    this.inst.scorches = saved.scorches;
    this.skip.particles = skipP;
    Object.assign(this.cam, { mode: saved.mode, frozen: saved.frozen, dist: saved.dist,
                              azim: saved.azim, elev: saved.elev, targetY: saved.ty,
                              center: saved.center });
    return c.toDataURL('image/png');
  }

  /** compat: a explosão mais recente — usado pelas ferramentas de inspeção */
  get fluid() {
    if (!this.pool) return null;
    const a = this.pool.active;
    return (a.length ? a[a.length - 1] : this.pool.slots[0]).fluid;
  }
  get particles() {
    if (!this.pool) return null;
    const a = this.pool.active;
    return (a.length ? a[a.length - 1] : this.pool.slots[0]).particles;
  }

  /**
   * Recalcula sol, lua, luz-chave, LUTs atmosféricas, exposição e shadow map.
   * O trabalho pesado (sky-view) só roda se o sol realmente se moveu.
   */
  _updateCelestial(force = false) {
    const e = this.env;
    const h = ((e.timeOfDay % 24) + 24) % 24;
    this.sun = sunPosition(h, e.latitude, e.dayOfYear);
    this.moon = moonPosition(h, e.latitude, e.dayOfYear);

    // luz-chave: o sol enquanto estiver acima do horizonte, depois a lua.
    // A troca acontece onde ambas as irradiâncias são ~0, então não aparece.
    const keyIsSun = this.sun.altDeg > 0.0;
    this.keyDir = keyIsSun ? this.sun.dir : this.moon.dir;

    this.atmo.update(this.sun.dir, this.moon.dir, {
      sunIlluminance: e.sunIlluminance,
      moonIlluminance: e.moonIlluminance,
      moonTint: e.moonTint,
      keyIsSun,
      camHeight: 0.002,     // km — a câmera está ao nível do solo
      sunLux: SUN_LUX,
      moonPhysRatio: MOON_PHYS_RATIO / Math.max(e.moonIlluminance, 1e-12),
      glowLux: NIGHT_GLOW_LUX,
      expBase: EXPOSURE_BASE, expRef: EXPOSURE_REF, expAlpha: e.expAlpha,
      purkS: e.purkS,
    });
    e.envLut = this.atmo.envLut.tex;   // o post e a adaptação leem a exposição dali
    // fontes da noite em cd/m² → render, com o mesmo reforço da Lua
    e.moonBoost = e.moonIlluminance / MOON_PHYS_RATIO;
    e.nightScale = e.moonBoost / SUN_LUX;

    // shadow map só re-renderiza quando a luz-chave realmente mudou de direção
    const k = this.keyDir;
    if (force || !this._lastKeyDir
        || Math.abs(k[0] - this._lastKeyDir[0])
         + Math.abs(k[1] - this._lastKeyDir[1])
         + Math.abs(k[2] - this._lastKeyDir[2]) > 2e-3) {
      this.scene.renderSunShadow(k);
      this._lastKeyDir = Float32Array.from(k);
    }
  }

  setTimeOfDay(h) {
    this.env.timeOfDay = ((h % 24) + 24) % 24;
    this._updateCelestial();
    if (this.onTime) this.onTime(this.env.timeOfDay);
  }

  _buildSim() {
    const gl = this.gl;
    const S = this.settings, q = SIM_TIERS[S.liveSim] || SIM_TIERS[128];
    // libera a geração anterior INTEIRA. Os alvos que o volume realocou
    // depois (resize) não estão no escopo: são liberados à parte.
    if (this._gen) {
      if (this.vol.volTarget) this.vol.volTarget.dispose();
      if (this.vol.halfDepth) this.vol.halfDepth.dispose();
      this._gen.dispose();
    }
    this._gen = trackGL(gl, () => {
      this.pool = new BlastPool(gl, S.liveSim, DOMAIN, this.bbTex, q.slots);
      this.pool.configure((fp) => {
        fp.pressureCoarse = q.coarse;
        fp.pressureFine = q.fine;
        fp.velMacCormack = q.mac;
        fp.lightSteps = q.light;
      });
      this.scene = new Scene(gl, this.pool.grid, this.pool.macroGrid, this.bake);
      this.vol = new VolumeRenderer(gl, this.pool.grid, this.pool.lightGrid,
                                    this.pool.macroGrid, this.bbTex);
      this.vol.params.steps = q.steps;
      this.vol.params.detailAmp2 = q.det2;
      this.vol.params.volScale = S.volScale;
      this.vol.resize(this.rw || 8, this.rh || 8);
    });
    // o ruído de detalhe das instâncias vinha do pool que acabou de morrer
    if (this.inst) this.inst.noiseTex = this.pool.slots[0].fluid.noiseTex;
    this._lastKeyDir = null;   // força re-render do shadow map na nova cena
  }

  /**
   * Recria a sequência assada numa nova resolução. A cena e as instâncias
   * embutem a geometria da grade assada nos shaders, então vão junto. As
   * explosões em andamento e os queimados no chão são preservados.
   */
  _rebuildBake(res) {
    const gl = this.gl;
    // cancela um bake ou gravação em andamento da resolução anterior
    if (this._bakeGen) { this._bakeGen.dispose(); this._bakeGen = null; }
    this._save = null;
    const keep = this.inst ? { list: this.inst.list, scorches: this.inst.scorches } : null;
    if (this._instScope) this._instScope.dispose();
    if (this._bakeScope) this._bakeScope.dispose();
    this._bakeScope = trackGL(gl, () => {
      this.bake = new ExplosionBake(gl, res, DOMAIN, BAKE_FRAMES, BAKE_DURATION);
    });
    this._buildSim();
    // Instâncias: a camada que ESCALA. O solver ao vivo vira ferramenta de
    // autoria; o runtime só instancia o resultado.
    this._instScope = trackGL(gl, () => {
      this.inst = new BlastInstances(gl, this.bake, this.bbTex,
                                     this.pool.slots[0].fluid.noiseTex);
      // fumaça que fica: lê o bake (formato depende da resolução), então
      // nasce e morre junto com as instâncias
      this.battle = new BattleSmoke(gl, this.bake, this.pool.slots[0].fluid.noiseTex, this.bbTex);
    });
    if (keep) Object.assign(this.inst, keep);
    this.thumbs = null;
    this.bakeStatus = { phase: 'iniciando', progress: 0 };
    if (this.onBakeProgress) this.onBakeProgress(this.bakeStatus);
    this._bakePromise = this._loadOrBake();
  }

  /** aplica as opções que valem no próximo frame */
  _applyLiveSettings() {
    const S = this.settings, env = this.env, P = this.inst.params;
    P.steps = S.instSteps;
    P.msOctaves = S.msOctaves;
    P.lcSlots = S.lightCache;
    P.lightGain = S.lightGain;
    this.vol.params.msOctaves = S.msOctaves;
    this.sparks.fraction = S.sparks;
    for (const sl of this.pool.slots) sl.particles.params.drawFraction = S.sparks;
    this.skip.particles = S.sparks <= 0;
    this.skip.ssao = S.ao <= 0;
    if (S.ao > 0) this.ssao.params.scale = S.ao;
    env.adaptive = S.dynamicRes;
    if (!S.dynamicRes) this.dynScale = 1;
    // a meta fica um pouco acima do período do quadro: medição tem jitter
    env.targetMs = (1000 / S.targetFps) * 1.08;
    env.bloomOn = S.bloom;
    env.grainOn = S.grain;
    env.chromaticOn = S.chromatic;
    env.lightGain = S.lightGain;
    env.instLightOcc = S.smokeBlocksLight;
    env.autoExposure = S.autoExposure;
    env.flashOn = S.flash;
    this.inst.params.flashGain = S.flash ? env.flashGain : 0;
    this.inst.params.flashTau = env.flashTau;
    if (this.vol.params.volScale !== S.volScale) {
      this.vol.params.volScale = S.volScale;
      this._applyRenderScale(true);
    }
  }

  /** nome do preset que bate com a configuração atual, ou null */
  get quality() { return matchPreset(this.settings); }

  /**
   * Troca configurações (uma ou várias). Decide o mínimo de trabalho: só o
   * bake é recriado quando o detalhe das explosões muda, só a simulação ao
   * vivo quando a grade dela muda; o resto vale no frame seguinte.
   */
  applySettings(next) {
    const prev = this.settings;
    const S = { ...prev, ...next };
    for (const k of Object.keys(S)) {
      if (!ITEMS[k] || !ITEMS[k].options.some(([v]) => v === S[k])) S[k] = prev[k];
    }
    this.settings = S;
    if (S.bakeRes !== prev.bakeRes) {
      this._rebuildBake(S.bakeRes);
      this.resize();
      this._updateCelestial(true);
    } else if (S.liveSim !== prev.liveSim) {
      this._buildSim();
      this.resize();
      this._updateCelestial(true);
    } else if (S.renderScale !== prev.renderScale) {
      this._applyRenderScale(true);
    }
    this._applyLiveSettings();
    saveSettings(S);
    if (this.onSettings) this.onSettings(S, this.quality);
  }

  setSetting(id, value) { this.applySettings({ [id]: value }); }

  resize() {
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    const cw = Math.max(320, window.innerWidth), ch = Math.max(240, window.innerHeight);
    // O canvas fica fixo; a resolução de RENDER é que varia (resolução
    // dinâmica). O passe final faz o upscale — e ele custa ~0 na medição.
    this.canvas.width = Math.max(2, Math.round(cw * Math.min(dpr, 1.5)));
    this.canvas.height = Math.max(2, Math.round(ch * Math.min(dpr, 1.5)));
    this.canvas.style.width = cw + 'px';
    this.canvas.style.height = ch + 'px';
    this._applyRenderScale(true);
  }

  _applyRenderScale(force = false) {
    const gl = this.gl;
    const s = clamp(this.settings.renderScale * this.dynScale, 0.40, 1.0);
    const rw = Math.max(2, Math.round(this.canvas.width * s));
    const rh = Math.max(2, Math.round(this.canvas.height * s));
    // só realoca se mudou de verdade: churn de textura é caro e pisca
    if (!force && Math.abs(rw - this.rw) < 24 && Math.abs(rh - this.rh) < 24) return;
    this.rw = rw; this.rh = rh;
    const rgba = { internalFormat: gl.RGBA16F, format: gl.RGBA, type: gl.HALF_FLOAT };
    if (this.sceneT) { this.sceneT.dispose(); this.compT.dispose(); }
    // 0 cor · 1 normal+fração indireta · 2,3 luz do fogo de cada cubo de
    // sombra + fator ruidoso (shadowdenoise.js)
    this.sceneT = new MRTarget(gl, rw, rh, [rgba, rgba, rgba, rgba], { depth: true });
    this.shadowDenoise.resize(rw, rh);
    this.compT = new Target(gl, rw, rh, rgba);
    this.vol.resize(rw, rh);
    this.post.resize(rw, rh);
    this.ssao.resize(rw, rh);
  }

  /**
   * Resolução dinâmica. Mede a mediana do frame time e ajusta a escala de
   * render pra segurar o alvo. Ignora frames com a aba oculta — o navegador
   * congela o rAF nesse caso e a adaptação entraria em espiral.
   */
  _adapt() {
    if (!this.env.adaptive) return;
    if (document.visibilityState !== 'visible') return;
    if (this.frame - (this._lastAdapt || 0) < 45) return;
    this._lastAdapt = this.frame;
    const med = this.frameMsMedian;
    if (!med) return;
    // Frame time absurdo não é carga de GPU — é o navegador congelando uma
    // aba oculta/ocluída. Adaptar nesse regime derrubaria a resolução até o
    // piso sem ganho nenhum.
    if (med > this.env.adaptCeiling) return;
    const target = this.env.targetMs;
    let d = this.dynScale;
    if (med > target * 1.3) d *= 0.90;
    else if (med < target * 0.8) d *= 1.05;
    else return;
    this.dynScale = clamp(d, 0.40, 1.0);
    this._applyRenderScale();
  }

  /**
   * Detona num slot do pool. As explosões anteriores CONTINUAM vivas — o
   * pool recicla o slot mais antigo quando acabam as vagas.
   * A câmera não acompanha: num RTS ela fica onde o jogador deixou.
   */
  detonate(pos = [0, 1.85, 0], seed = Math.random() * 500) {
    this.time = 0;
    this.pool.detonate(pos, seed);
    this._kicked = false;
  }

  /**
   * Converte a posição do cursor num ponto do plano y=0.
   * Desprojeta dois pontos do raio no clip space e cruza com o chão.
   */
  pickGround(clientX, clientY) {
    const r = this.canvas.getBoundingClientRect();
    const nx = ((clientX - r.left) / r.width) * 2 - 1;
    const ny = 1 - ((clientY - r.top) / r.height) * 2;
    const un = (z) => {
      const v = [nx, ny, z, 1];
      const m = this.cam.invViewProj;
      const o = [0, 0, 0, 0];
      for (let i = 0; i < 4; i++) {
        o[i] = m[i] * v[0] + m[4 + i] * v[1] + m[8 + i] * v[2] + m[12 + i] * v[3];
      }
      return [o[0] / o[3], o[1] / o[3], o[2] / o[3]];
    };
    const a = un(-1), b = un(1);
    const d = [b[0] - a[0], b[1] - a[1], b[2] - a[2]];
    if (Math.abs(d[1]) < 1e-6) return null;
    const t = -a[1] / d[1];
    if (t < 0) return null;                       // acima da linha do horizonte
    const hit = [a[0] + d[0] * t, 0, a[2] + d[2] * t];
    // Perto do horizonte o raio fica quase paralelo ao chão e t explode: um
    // clique ali punha a explosão a trilhões de metros. Fora do alcance,
    // o clique simplesmente não detona.
    const dx = hit[0] - this.cam.pos[0], dz = hit[2] - this.cam.pos[2];
    if (!Number.isFinite(hit[0]) || dx * dx + dz * dz > this.env.maxClickRange ** 2) return null;
    return hit;
  }

  /** detona no ponto do terreno, reposicionando o domínio da simulação */
  /**
   * Explosão em runtime: INSTÂNCIA da sequência assada, não simulação nova.
   * É o que permite várias por segundo.
   */
  detonateAt(groundPos, opts = {}) {
    if (!this.bake.variantsReady) {
      // arsenal ainda assando: o solver ao vivo responde no lugar
      this.detonate([groundPos[0], 1.85, groundPos[2]]);
      this.cam.kick(0.85);
      if (this.onDetonate) this.onDetonate(groundPos);
      return;
    }
    this._lastBlastPos = [groundPos[0], 0, groundPos[2]];
    const o = this.inst.spawn([groundPos[0], 0, groundPos[2]], {
      magnitude: opts.magnitude || this.selectedBlast || 'tanque',
      seed: Math.random(),
    });
    // faíscas nascem no centro da bola de fogo (a mesma altura da ao vivo)
    this.sparks.spawn([groundPos[0], 1.85 * o.scale, groundPos[2]], o.scale);
    // a expansão empurra a fumaça antiga em volta
    if (this.settings.battleSmoke) this.battle.addBlast([groundPos[0], 2.0 * o.scale, groundPos[2]], o.scale);
    this.cam.kick(Math.min(1.2, 0.5 * o.scale + 0.35));
    this._kicked = true;
    if (this.onDetonate) this.onDetonate(groundPos);
  }

  _bindInput() {
    const c = this.canvas;
    let down = false, lx = 0, ly = 0;
    let dragDist = 0;
    c.addEventListener('pointerdown', (e) => {
      if (e.button !== 0) return;          // o direito é a rolagem (abaixo)
      down = true; lx = e.clientX; ly = e.clientY; dragDist = 0;
      c.setPointerCapture(e.pointerId);
    });
    c.addEventListener('pointerup', (e) => {
      if (!down || e.button !== 0) return;
      down = false;
      c.releasePointerCapture(e.pointerId);
      // clique curto detona; arrasto orbita
      if (dragDist < 5) {
        const p = this.pickGround(e.clientX, e.clientY);
        if (p) this.detonateAt(p);
      }
    });
    c.addEventListener('pointermove', (e) => {
      if (!down) return;
      dragDist += Math.abs(e.clientX - lx) + Math.abs(e.clientY - ly);
      this.cam.orbit(e.clientX - lx, e.clientY - ly);
      lx = e.clientX; ly = e.clientY;
    });
    c.addEventListener('wheel', (e) => { e.preventDefault(); this.cam.zoom(e.deltaY); }, { passive: false });

    // ---- rolagem do C&C Generals / Zero Hour: botão direito arrastado ----
    // A âncora é onde o botão desceu; o quadro anda na direção do cursor,
    // mais rápido quanto mais longe (Camera.scrollRMB). Como no jogo, a
    // âncora é arrastada junto quando o cursor passa de meia tela dela.
    // Eventos de MOUSE (não pointer): com o esquerdo já apertado, o direito
    // não gera pointerdown.
    const rmb = this.rmb = { on: false, ax: 0, ay: 0, x: 0, y: 0 };
    const ind = document.getElementById('rmb');
    const drawRmb = () => {
      if (!ind) return;
      const dx = rmb.x - rmb.ax, dy = rmb.y - rmb.ay, len = Math.hypot(dx, dy);
      ind.querySelector('circle').setAttribute('cx', rmb.ax);
      ind.querySelector('circle').setAttribute('cy', rmb.ay);
      const line = ind.querySelector('line'), head = ind.querySelector('path');
      if (len < 8) { line.setAttribute('visibility', 'hidden'); head.setAttribute('visibility', 'hidden'); return; }
      const ux = dx / len, uy = dy / len;
      line.setAttribute('visibility', 'visible'); head.setAttribute('visibility', 'visible');
      line.setAttribute('x1', rmb.ax + ux * 8); line.setAttribute('y1', rmb.ay + uy * 8);
      line.setAttribute('x2', rmb.x); line.setAttribute('y2', rmb.y);
      // ponta de seta no cursor, apontando pra onde a câmera anda
      const px = -uy, py = ux, b = 11, w = 6;
      head.setAttribute('d', `M${rmb.x - ux * b + px * w},${rmb.y - uy * b + py * w}`
        + `L${rmb.x},${rmb.y}L${rmb.x - ux * b - px * w},${rmb.y - uy * b - py * w}`);
    };
    c.addEventListener('contextmenu', (e) => e.preventDefault());
    c.addEventListener('mousedown', (e) => {
      if (e.button !== 2) return;
      e.preventDefault();
      Object.assign(rmb, { on: true, ax: e.clientX, ay: e.clientY, x: e.clientX, y: e.clientY });
      document.body.classList.add('rmb');
      drawRmb();
    });
    window.addEventListener('mousemove', (e) => {
      if (!rmb.on) return;
      rmb.x = e.clientX; rmb.y = e.clientY;
      const mx = c.clientWidth / 2, my = c.clientHeight / 2;
      rmb.ax = clamp(rmb.ax, rmb.x - mx, rmb.x + mx);
      rmb.ay = clamp(rmb.ay, rmb.y - my, rmb.y + my);
      drawRmb();
    });
    window.addEventListener('mouseup', (e) => {
      if (e.button !== 2 || !rmb.on) return;
      rmb.on = false;
      document.body.classList.remove('rmb');
    });
    window.addEventListener('blur', () => { rmb.on = false; document.body.classList.remove('rmb'); });
    window.addEventListener('resize', () => this.resize());
    window.addEventListener('keydown', (e) => {
      const k = e.key.toLowerCase();
      // espaço = explosão SIMULADA ao vivo (hero), pra comparar com a assada
      if (k === ' ') { e.preventDefault(); this.detonate([this.cam.center[0], 1.85, this.cam.center[2]]); }
      else if (k === 'p') this.paused = !this.paused;
      else if (k === 'c') { this.cam.mode = this.cam.mode === 'cine' ? 'free' : 'cine'; }
      else if (k === '[') this.env.timeScale = clamp(this.env.timeScale / 1.5, 0.05, 4);
      else if (k === ']') this.env.timeScale = clamp(this.env.timeScale * 1.5, 0.05, 4);
      else if (k === '\\') this.env.timeScale = 1.0;
      else if (k === 'h') document.body.classList.toggle('hide-ui');
      else if (k === ',') this.setTimeOfDay(this.env.timeOfDay - 1 / 6);
      else if (k === '.') this.setTimeOfDay(this.env.timeOfDay + 1 / 6);
      else if (k === 't') this.env.autoCycle = !this.env.autoCycle;
      else if (k === 'a') this.spawnMover('plane');
      else if (k === 'm') this.spawnMover('missile');
      else if (k === 'd') this.spawnMover('shell');
      else if (['1', '2', '3', '4'].includes(k)) {
        this.setQuality(['baixa', 'media', 'alta', 'ultra'][+k - 1]);
      }
    });
  }

  /**
   * Teste da interação com a fumaça: avião, míssil ou disparo de tanque
   * atravessando a fumaça da última explosão, cruzando a tela da esquerda
   * pra direita.
   */
  spawnMover(kind) {
    const tgt = this._lastBlastPos || [this.cam.center[0], 0, this.cam.center[2]];
    const fx = this.cam.center[0] - this.cam.pos[0], fz = this.cam.center[2] - this.cam.pos[2];
    const l = Math.hypot(fx, fz) || 1;
    // passa pela altura onde a fumaça está; se ali só há uma explosão ainda
    // na fase assada, pelo meio dela
    let h = this.settings.battleSmoke ? this.battle.smokeHeightAt(tgt[0], tgt[2]) : null;
    if (h === null) {
      const o = this.inst.list.filter((q) => !q.handed
        && Math.hypot(q.pos[0] - tgt[0], q.pos[2] - tgt[2]) < 20).pop();
      if (o) h = 18 * o.scale;
    }
    return this.movers.spawn(kind, tgt, [-fz / l, 0, fx / l], h);   // direita da câmera
  }

  /** aplica um preset inteiro (baixa/media/alta/ultra) */
  setQuality(q) {
    if (!PRESETS[q]) return;
    this.applySettings({ ...PRESETS[q] });
    if (this.onQuality) this.onQuality(q);
  }

  /** avança determinístico até t segundos e renderiza — pra inspeção visual */
  renderAt(tTarget, dt = 1 / 60) {
    this.detonate();
    const auto = this.env.autoReplay;
    this.env.autoReplay = false;
    let n = 0;
    while (this.time < tTarget && n < 3000) { this.frameStep(dt); n++; }
    this.env.autoReplay = auto;
    return { t: this.time, frames: n };
  }

  /**
   * Custo de GPU por estágio, medido ISOLADAMENTE.
   *
   * Ablação por diferença tinha ruído maior que vários dos sinais (chegou a
   * dar custo negativo). Repetir só o estágio entre duas barreiras de sync
   * faz o sinal ser a medição inteira, não uma diferença pequena. O custo da
   * própria barreira é medido com um no-op e subtraído.
   */
  bench(reps = 10) {
    const gl = this.gl, env = this.env;
    const t = this.time, jitter = 0.5;
    if (!this.pool.active.length) this.detonate();
    // Todas são renderizadas (o ray-box descarta as fora do caminho na hora);
    // só as mais próximas entram na iluminação da cena, que é N² em sombras.
    if (dt > 0) this.inst.update(dt);
    if (dt > 0 && !this.skip.particles) this.sparks.step(dt);
    const blasts = this.pool.sortedFor(this.cam.pos);
    const shaded = blasts.slice(0, MAX_SHADED);
    const lead = blasts[0];
    const sceneEnv = {
      sunDir: this.sun.dir, moonDir: this.moon.dir, keyDir: this.keyDir,
      skyView: this.atmo.skyView.tex, envLut: this.atmo.envLut.tex,
      starBright: env.starBright * env.nightScale, nightGlow: env.nightGlow * env.nightScale,
      moonBright: env.moonBright * env.nightScale, moonBoost: env.moonBoost,
      skyTime: t, blasts: this.pool.sortedFor(this.cam.pos).slice(0, MAX_SHADED),
      instLights: this.inst.lights(this.settings.instLights),
      instShadows: this.settings.instShadows ? this.inst.shadowCasters(this.cam, this.settings.instShadows) : [],
      scorch: this.inst.scorchData(),
      bakeTex: this.bake.tex, bakeMacroTex: this.bake.macroTex,
      bakeFrames: this.bake.frames,
      sootExt: this.vol.params.sootExt, dustExt: this.vol.params.dustExt,
      fireOcclude: env.fireOcclude, fireTauCap: env.fireTauCap, fireFill: env.fireFill,
      erodeMean: this.vol.params.erode * 0.5,
      domainOrigin: lead.fluid.domainOrigin,
      scorchR: 8, scorchAmt: 0.9, blastXZ: [0, 0],
      fogDensity: env.fogDensity, fogFalloff: env.fogFalloff,
      fogFireGain: env.fogFireGain, ambient: env.ambient, frameJitter: jitter,
    };
    const volEnv = {
      keyDir: this.keyDir, envLut: this.atmo.envLut.tex,
      frameJitter: jitter, time: t,
    };
    const compEnv = {
      fogDensity: env.fogDensity, fogFalloff: env.fogFalloff,
      sunDir: this.sun.dir, moonDir: this.moon.dir,
      skyView: this.atmo.skyView.tex, envLut: this.atmo.envLut.tex,
      ao: this.ssao.a.tex, sceneNrm: this.sceneT.texs[1],
      aoFloor: env.aoFloor, aoDebug: false,
    };
    const timeIt = (fn) => {
      fn(); this._gpuSync();
      let best = Infinity;
      for (let r = 0; r < 3; r++) {
        const t0 = performance.now();
        for (let i = 0; i < reps; i++) fn();
        this._gpuSync();
        best = Math.min(best, (performance.now() - t0) / reps);
      }
      return best;
    };
    const nul = timeIt(() => {});
    const net = (v) => +Math.max(v - nul, 0).toFixed(2);

    const r = {};
    r.sim = net(timeIt(() => this.pool.step(1 / 600, this.keyDir,
      this.vol.params.sootExt, this.vol.params.dustExt, this.vol.params.erode * 0.5)));
    r.luzFogo = net(timeIt(() => this.vol.updateFireLight(lead)));
    r.sombraFogo = net(timeIt(() => this.scene.renderFireShadows([{ pos: lead.fluid.blastPos, radius: 3.5, kind: 1, idx: 0 }])));
    r.cena = net(timeIt(() => { this.sceneT.bind(true); this.scene.render(this.cam, sceneEnv); }));
    r.ssao = net(timeIt(() => this.ssao.render(this.cam, this.sceneT.depthTex, this.sceneT.texs[1], 0.5)));
    r.volume = net(timeIt(() => this.vol.march(this.cam, volEnv, blasts,
      this.pool.slots[0].fluid.noiseTex, this.sceneT.depthTex, this.rw, this.rh)));
    r.composite = net(timeIt(() => this.vol.composite(this.compT, this.cam,
      this.sceneT.texs[0], this.sceneT.depthTex, this.rw, this.rh, t, compEnv)));
    r.bloom = net(timeIt(() => this.post.build(this.compT.tex, env)));
    r.final = net(timeIt(() => this.post.final(this.compT.tex,
      this.canvas.width, this.canvas.height, env, t)));
    r.TOTAL = +Object.values(r).reduce((a, b) => a + b, 0).toFixed(2);
    r._res = `${this.rw}x${this.rh}`;
    r._grade = this.pool.grid.nx;
    r._explosoes = this.pool.active.length;
    r._passos = this.vol.params.steps;
    return r;
  }





  /** mediana do frame time real das últimas ~48 iterações do rAF */
  get frameMsMedian() {
    const v = [...this._ring].filter((x) => x > 0).sort((a, b) => a - b);
    return v.length ? +v[v.length >> 1].toFixed(1) : 0;
  }

  /**
   * Barreira de sincronização GPU→CPU.
   *
   * gl.finish() é no-op neste runtime e clientWaitSync não tem permissão de
   * bloquear no Chrome. Mas um readPixels de 1 pixel bloqueia de verdade:
   * é exatamente por isso que ele custava 21ms quando estava no caminho
   * quente. Aqui esse defeito vira a ferramenta de medição.
   */
  _gpuSync() {
    const gl = this.gl;
    if (!this._syncBuf) this._syncBuf = new Uint8Array(4);
    // Lê do framebuffer PADRÃO, não de um alvo auxiliar. Ler de um FBO
    // qualquer só obriga o driver a terminar o trabalho daquele FBO — o
    // resto do frame pode estar reordenado e ainda pendente. O canvas é
    // onde todo o frame desemboca, então aqui a espera é real.
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, this._syncBuf);
  }

  /**
   * Ablação: desliga um estágio, roda N frames com barreira de sync no fim,
   * e compara com o baseline. A diferença é o custo real do estágio.
   */
  ablate(frames = 5, rounds = 5) {
    const stages = ['baseline', 'sim', 'luzFogo', 'sombraFogo', 'cena',
                    'volume', 'composite', 'bloom', 'post'];
    const out = {};
    const measure = () => {
      this.frameStep(1 / 60);          // aquece o estado do pipeline
      this._gpuSync();
      const t0 = performance.now();
      for (let i = 0; i < frames; i++) this.frameStep(1 / 60);
      this._gpuSync();
      return (performance.now() - t0) / frames;
    };
    // Round-robin + mínimo. Medir estágio a estágio em sequência dava uma
    // rampa monótona — era clock da GPU subindo, não custo. Intercalar e
    // ficar com o mínimo é imune a ramp e a throttling.
    for (const st of stages) out[st] = Infinity;
    for (let r = 0; r < rounds; r++) {
      for (const st of stages) {
        this.skip = st === 'baseline' ? {} : { [st]: true };
        out[st] = Math.min(out[st], measure());
      }
    }
    for (const st of stages) out[st] = +out[st].toFixed(2);
    this.skip = {};
    const base = out.baseline;
    const custo = {};
    for (const k of stages) if (k !== 'baseline') custo[k] = +(base - out[k]).toFixed(2);
    return {
      frameMs: out, custoMs: custo,
      fpsBase: +(1000 / base).toFixed(1),
      res: `${this.rw}x${this.rh}`, canvas: `${this.canvas.width}x${this.canvas.height}`,
      grade: this.fluid.grid.nx,
    };
  }

  frameStep(realDt) {
    const gl = this.gl, env = this.env, S = this.settings;
    const dt = this.paused ? 0 : clamp(realDt, 1 / 480, 1 / 24) * env.timeScale;
    this.time += dt;
    const t = this.time;

    // shake da câmera quando a onda de choque chega na lente (~2 quadros)
    if (!this._kicked && t > 0.03) { this.cam.kick(1.0); this._kicked = true; }
    if (env.autoReplay && t > env.replayAfter) this.detonate();

    // rolagem pelo botão direito, em tempo real (a câmera lenta não a freia)
    if (this.rmb && this.rmb.on) {
      const h = Math.max(this.canvas.clientHeight * 0.5, 1);
      this.cam.scrollRMB((this.rmb.x - this.rmb.ax) / h, (this.rmb.y - this.rmb.ay) / h, realDt);
    }
    this.cam.update(this.rw / this.rh, realDt, t);

    if (env.autoCycle) this.setTimeOfDay(env.timeOfDay + realDt * env.cycleSpeed);

    const prof = this.prof;
    prof.frameStart();

    // ---- 1. simulação ---------------------------------------------------
    if (dt > 0 && !this.skip.sim) {
      prof.begin('sim');
      this.pool.step(dt, this.keyDir, this.vol.params.sootExt, this.vol.params.dustExt,
        this.vol.params.erode * 0.5);
      prof.end();
    }
    // Todas são renderizadas (o ray-box descarta as fora do caminho na hora);
    // só as mais próximas entram na iluminação da cena, que é N² em sombras.
    if (dt > 0) this.inst.update(dt);
    if (dt > 0 && !this.skip.particles) this.sparks.step(dt);
    // o que atravessa o campo (avião, míssil, disparo): anda e empurra a
    // fumaça (os perturbadores são consumidos pelo passo da batalha logo abaixo)
    const battleOn = this.settings.battleSmoke && !this.skip.battle;
    this.movers.update(dt, battleOn ? this.battle : null);
    this.scene.setMovers(this.movers.boxes, this.movers.nBoxes, this.movers.cyls, this.movers.nCyls);
    {
      // o mapa de sombra do sol é cacheado; enquanto algo se move, refaz
      const n = this.movers.nBoxes + this.movers.nCyls;
      if (n || this._movParts) this.scene.renderSunShadow(this.keyDir);
      this._movParts = n;
    }
    // ---- 1b. fumaça que fica: entrega das explosões + fluido do campo ----
    if (battleOn) {
      prof.begin('batalha');
      this.battle.noiseTex = this.pool.slots[0].fluid.noiseTex;   // o sim pode ter sido recriado
      this.battle.step(dt, this.inst, { keyDir: this.keyDir }, this.inst.params);
      prof.end();
    }
    const blasts = this.pool.sortedFor(this.cam.pos);
    const shaded = blasts.slice(0, MAX_SHADED);
    const lead = blasts[0];

    // ---- 2. luz de cada bola de fogo (uma redução por frame, em rodízio) -
    if (!this.skip.luzFogo && this.pool.active.length) {
      prof.begin('luzFogo');
      const act = this.pool.active;
      this.vol.updateFireLight(act[this.frame % act.length]);
      prof.end();
    }

    // ---- 3. sombras das luzes de explosão ---------------------------------
    // As 2 luzes mais fortes do quadro — a explosão ao vivo principal ou as
    // do clique — ganham cubo de sombra. O flash de uma explosão nova já a
    // põe na frente, então a sombra nasce junto com o clarão.
    const instLights = this.inst.lights(S.instLights);
    let fireCubes = [];
    if (S.fireShadows > 0 && !this.skip.sombraFogo) {
      const cands = [];
      if (lead && lead.fire) {
        const c = lead.fire.color;
        cands.push({ pos: lead.fire.pos, power: c[0] + c[1] + c[2], radius: env.fireRad, kind: 1, idx: 0 });
      }
      instLights.forEach((L, i) => {
        cands.push({ pos: L.pos, power: L.power, radius: 2.2 * Math.sqrt(L.s2 ?? 1), kind: 2, idx: i,
          s2: L.s2 ?? 1 });
      });
      // Uma luz merece cubo enquanto a sombra dela for VISÍVEL, e isso é
      // relativo à luz do ambiente: o contraste de uma sombra do fogo é
      // E_f/(E_f + E_amb). Abaixo de ~3% (1.5–3× a fração de Weber pra campos
      // grandes, 1–2%) ninguém vê. Um limiar absoluto (potência > 2) cortava a
      // sombra à noite logo que o fogo ficava vermelho, com ele ainda ~100×
      // mais forte que o luar. E_f é a iluminância a 5 m, mesma queda do
      // sombreador; sem a leitura da CPU ainda, toda luz entra.
      this.atmo.pollEnv();
      const eAmb = this.atmo.envCPU ? this.atmo.envCPU.eRender : 0;
      const eAt5 = (c) => (c.power / 3) / (c.kind === 1 ? 1.5 : c.s2 + 0.5);
      const pick = cands.filter((c) => c.power > 1e-4 && eAt5(c) >= 0.03 * eAmb)
        .sort((x, y) => y.power - x.power)
        .slice(0, S.fireShadows);
      prof.begin('sombraFogo');
      fireCubes = this.scene.renderFireShadows(pick);
      prof.end();
      this._lastFireCubes = fireCubes;   // inspeção
    }

    // ---- 4. cena ---------------------------------------------------------
    const jitter = (this.frame % 8) + 0.5;
    const scorchR = Math.min(11.5, 4.6 * Math.pow(Math.max(t, 0.001), 0.32));
    const sceneEnv = {
      sunDir: this.sun.dir, moonDir: this.moon.dir, keyDir: this.keyDir,
      skyView: this.atmo.skyView.tex, envLut: this.atmo.envLut.tex,
      starBright: env.starBright * env.nightScale, nightGlow: env.nightGlow * env.nightScale,
      moonBright: env.moonBright * env.nightScale, moonBoost: env.moonBoost,
      skyTime: t,
      blasts: shaded, instLights, fireCubes, smDebug: env.smDebug,
      instShadows: S.instShadows ? this.inst.shadowCasters(this.cam, S.instShadows) : [],
      lightGain: S.lightGain, instLightOcc: env.instLightOcc,
      flashGain: env.flashOn ? env.flashGain : 0, flashTau: env.flashTau,
      scorch: this.inst.scorchData(),
      bakeTex: this.bake.tex, bakeMacroTex: this.bake.macroTex,
      bakeFrames: this.bake.frames,
      sootExt: this.vol.params.sootExt, dustExt: this.vol.params.dustExt,
      fireOcclude: env.fireOcclude,
      scorchR, scorchAmt: smoothstep(0.0, 0.35, t) * 0.95,
      blastXZ: [this.fluid.blastPos[0], this.fluid.blastPos[2]],
      fogDensity: env.fogDensity, fogFalloff: env.fogFalloff,
      fogFireGain: env.fogFireGain, fireTauCap: env.fireTauCap, fireFill: env.fireFill,
      erodeMean: this.vol.params.erode * 0.5,
      domainOrigin: this.fluid.domainOrigin,
      ambient: env.ambient, frameJitter: jitter,
    };
    prof.begin('cena');
    this.sceneT.bind(true);
    gl.clearColor(0, 0, 0, 1);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    if (!this.skip.cena) this.scene.render(this.cam, sceneEnv);
    // brasas desenhadas AQUI, dentro do alvo da cena: o composite do volume
    // multiplica tudo pela transmitância, então a fumaça as oculta de graça
    if (!this.skip.particles) {
      // as brasas só escrevem cor e normal; os alvos da luz do fogo ficam
      // com o que a cena escreveu (sem isso o conteúdo seria indefinido)
      gl.drawBuffers([gl.COLOR_ATTACHMENT0, gl.COLOR_ATTACHMENT1, gl.NONE, gl.NONE]);
      for (const b of blasts) {
        b.particles.draw(this.cam, this.vol.params.tempScale, this.vol.params.emissionCurve);
      }
      this.sparks.draw(this.cam, this.vol.params.tempScale, this.vol.params.emissionCurve);
      this.sceneT.bind();
      gl.drawBuffers([gl.COLOR_ATTACHMENT0, gl.COLOR_ATTACHMENT1, gl.COLOR_ATTACHMENT2, gl.COLOR_ATTACHMENT3]);
    }
    prof.end();

    // ---- 4a. sombra do fogo filtrada em espaço de tela -------------------
    this._fireMask = null;
    if (sceneEnv.fireCubes && sceneEnv.fireCubes.length) {
      this._fireMask = this.shadowDenoise.render(this.cam, this.sceneT.texs[2], this.sceneT.texs[3],
        this.sceneT.depthTex, this.sceneT.texs[1]);
    }

    // ---- 4b. oclusão de ambiente ----------------------------------------
    prof.begin('ssao');
    this._aoTex = this.skip.ssao
      ? null
      : this.ssao.render(this.cam, this.sceneT.depthTex, this.sceneT.texs[1], jitter);
    prof.end();

    // ---- 5. raymarch do volume + composite ------------------------------
    prof.begin('volume');
    if (!this.skip.volume) this.vol.march(this.cam, {
      keyDir: this.keyDir, envLut: this.atmo.envLut.tex,
      frameJitter: jitter, time: t,
    }, blasts, this.pool.slots[0].fluid.noiseTex, this.sceneT.depthTex, this.rw, this.rh);
    // instâncias assadas entram no MESMO alvo, com o mesmo blend front-to-back
    if (!this.skip.volume && this.inst.list.length) {
      const gl2 = this.gl;
      // volume de luz das instâncias mais visíveis, marchado neste quadro
      this._lcCount = this.skip.lightCache ? 0
        : this.inst.updateLightCache(this.cam, this.keyDir, this.vol.halfW, this.vol.halfH);
      this.vol.volTarget.bind();
      gl2.enable(gl2.BLEND);
      gl2.blendFuncSeparate(gl2.DST_ALPHA, gl2.ONE, gl2.ZERO, gl2.SRC_ALPHA);
      this._instDrawn = this.inst.draw(this.cam, {
        keyDir: this.keyDir, envLut: this.atmo.envLut.tex,
        frameJitter: jitter, time: t,
      }, this.sceneT.depthTex, this.vol.halfW, this.vol.halfH);
      gl2.disable(gl2.BLEND);
      gl2.bindFramebuffer(gl2.FRAMEBUFFER, null);
    }
    // fumaça que fica: na frente e atrás das explosões, no mesmo alvo
    if (!this.skip.volume && this.settings.battleSmoke && !this.skip.battle) {
      this.battle.render(this.cam, {
        keyDir: this.keyDir, envLut: this.atmo.envLut.tex, frameJitter: jitter,
      }, this.vol.volTarget, this.vol.halfDepth.tex, instLights.concat(this.movers.lights), this.inst.params);
    }
    prof.end();

    prof.begin('composite');
    this.vol.composite(this.compT, this.cam, this.sceneT.texs[0], this.sceneT.depthTex,
      this.rw, this.rh, t, {
        fogDensity: env.fogDensity, fogFalloff: env.fogFalloff,
        sunDir: this.sun.dir, moonDir: this.moon.dir,
        skyView: this.atmo.skyView.tex, envLut: this.atmo.envLut.tex,
        ao: this.skip.ssao ? this._noAO : (this._aoTex || this.ssao.a.tex),
        sceneNrm: this.sceneT.texs[1],
        aoFloor: env.aoFloor, aoDebug: env.aoDebug,
        fireL0: this.sceneT.texs[2], fireL1: this.sceneT.texs[3], fireMask: this._fireMask,
      });
    prof.end();

    // ---- 5b. adaptação de exposição às explosões ------------------------
    if (!this.skip.ae && env.autoExposure !== false) {
      // Quanto ela pode fechar acompanha a escuridão do ambiente: de dia a
      // bola de fogo quase não estoura e fechar a cena inteira a cada
      // explosão de uma batalha seria um bombeamento constante; à noite
      // é onde o fogo vira mancha branca. O alcance é calculado na GPU a
      // partir da iluminância física da hora (exposure.js).
      this.ae.update(this.vol.volTarget.texs[0], this.compT.tex, this.sceneT.depthTex, this.atmo.envLut.tex,
        Math.min(Math.max(realDt, 0), 0.1), env);
      env.aeTex = this.ae.tex;
    } else {
      // desligada = exposição fixa exata, e o estado volta a 1 pra religar limpo
      if (env.aeTex) this.ae.reset();
      env.aeTex = null;
    }

    // ---- 6. post ---------------------------------------------------------
    prof.begin('post');
    if (this.skip.post) {
      this.post.final(this.compT.tex, this.canvas.width, this.canvas.height, env, t);
    } else if (this.skip.bloom || env.bloomOn === false) {
      this.post.final(this.compT.tex, this.canvas.width, this.canvas.height, env, t);
    } else {
      this.post.build(this.compT.tex, env);
      this.post.final(this.compT.tex, this.canvas.width, this.canvas.height, env, t);
    }
    prof.end();

    prof.frameEnd();
    this._ring[this._ringI++ % this._ring.length] = realDt * 1000;
    this.frame++;
    this._adapt();

  }
}

// ---------------------------------------------------------------------------

const canvas = document.getElementById('gl');
let app;
try {
  window.__appStartAt = performance.now();
  app = new App(canvas);
  window.__app = app;
  window.__appReadyAt = performance.now();
  // ferramenta de inspeção frame a frame
  window.__sheet = (opts) => showSheet(contactSheet(app, opts));
  window.__capture = (name, opts) => saveSheet(contactSheet(app, opts), name);
  window.__fields = (name, opts) => saveSheet(fieldSheet(app, opts), name);
  window.__hero = (name, opts) => saveSheet(heroShot(app, opts), name);
} catch (e) {
  document.getElementById('err').style.display = 'block';
  document.getElementById('err').textContent = 'Falha ao iniciar: ' + e.message;
  console.error(e);
  throw e;
}

// ---------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------
const $ = (id) => document.getElementById(id);
const todRange = $('todRange'), todClock = $('todClock'), todPhase = $('todPhase');
const cycleBtn = $('cycleBtn'), mark = $('mark');
const presetBtns = [...document.querySelectorAll('#presets .btn[data-h]')];
const qBtns = [...document.querySelectorAll('#qrow .btn[data-q]')];
const cardsBox = $('cards');

// Cartões montados a partir das magnitudes: a lista é dado, não markup.
function buildCards(thumbs) {
  cardsBox.innerHTML = '';
  for (const m of MAGNITUDES) {
    const b = document.createElement('button');
    b.className = 'card' + (m.id === app.selectedBlast ? ' on' : '');
    b.dataset.blast = m.id;
    b.title = `${m.nome} — ${m.meta}`;
    b.innerHTML = `<div class="card-img"><img alt="${m.nome}" src="assets/thumb_blast.png"><div class="card-prog"></div></div>
      <div class="card-yield">${magnitudeOf(m.id).scale.toFixed(2)}×</div>
      <div class="card-body"><div class="card-name">${m.nome}</div></div>`;
    if (thumbs && thumbs[m.id]) b.querySelector('img').src = thumbs[m.id];
    b.addEventListener('click', () => {
      app.selectedBlast = m.id;
      for (const o of cardsBox.children) o.classList.toggle('on', o === b);
    });
    cardsBox.appendChild(b);
  }
}

const fmtTime = (h) => {
  const m = ((Math.round(h * 60) % 1440) + 1440) % 1440;
  return `${String(Math.floor(m / 60)).padStart(2, '0')}:${String(m % 60).padStart(2, '0')}`;
};

// fase nomeada pela elevação solar, que é a definição de verdade
function solarPhase(alt) {
  if (alt > 25) return 'dia pleno';
  if (alt > 8) return 'manhã/tarde';
  if (alt > 0) return 'hora dourada';
  if (alt > -6) return 'crep. civil';
  if (alt > -12) return 'crep. náutico';
  if (alt > -18) return 'crep. astron.';
  return 'noite';
}

function syncTimeUI(h) {
  todRange.value = String(((Math.round(h * 60) % 1440) + 1440) % 1440);
  const alt = app.sun ? app.sun.altDeg : 0;
  todClock.textContent = fmtTime(h);
  todPhase.textContent = `${alt >= 0 ? '+' : ''}${alt.toFixed(0)}° · ${solarPhase(alt)}`;
  for (const b of presetBtns) {
    b.classList.toggle('on', Math.abs(((+b.dataset.h - h + 36) % 24) - 12) > 11.96);
  }
  cycleBtn.classList.toggle('on', app.env.autoCycle);
}

todRange.addEventListener('input', () => {
  app.env.autoCycle = false;
  app.setTimeOfDay(+todRange.value / 60);
});
for (const b of presetBtns) {
  b.addEventListener('click', () => { app.env.autoCycle = false; app.setTimeOfDay(+b.dataset.h); });
}
cycleBtn.addEventListener('click', () => {
  app.env.autoCycle = !app.env.autoCycle;
  syncTimeUI(app.env.timeOfDay);
});
for (const b of qBtns) b.addEventListener('click', () => app.setQuality(b.dataset.q));
for (const b of document.querySelectorAll('#movrow .btn[data-mov]')) {
  b.addEventListener('click', () => app.spawnMover(b.dataset.mov));
}
app.selectedBlast = app.selectedBlast || 'tanque';
buildCards(app.thumbs);
app.onTime = syncTimeUI;

// ---- configuração gráfica -------------------------------------------------
// A gaveta é montada a partir do esquema: opção nova em settings.js aparece
// aqui sem tocar no markup.
const cfg = $('cfg'), cfgBody = $('cfgBody'), cfgBtn = $('cfgBtn');
const cfgPresets = $('cfgPresets'), cfgPresetName = $('cfgPresetName'), cfgFoot = $('cfgFoot');
const fmtOpt = (v) => (typeof v === 'boolean' ? (v ? 'on' : 'off') : String(v));
function buildCfg() {
  cfgPresets.innerHTML = '';
  for (const [id, nome] of Object.entries(PRESET_NAMES)) {
    const b = document.createElement('button');
    b.className = 'btn'; b.dataset.q = id; b.textContent = nome;
    b.addEventListener('click', () => app.setQuality(id));
    cfgPresets.appendChild(b);
  }
  cfgBody.innerHTML = '';
  for (const g of SCHEMA) {
    const h = document.createElement('div');
    h.className = 'cfg-group'; h.textContent = g.group;
    cfgBody.appendChild(h);
    for (const it of g.items) {
      const row = document.createElement('div');
      row.className = 'cfg-row'; row.dataset.id = it.id;
      if (it.hint) row.title = it.hint;
      row.innerHTML = `<div class="cfg-label">${it.label}<em></em></div><div class="btnrow"></div>`;
      const br = row.querySelector('.btnrow');
      for (const [v, lab] of it.options) {
        const b = document.createElement('button');
        b.className = 'btn'; b.textContent = lab; b.dataset.v = fmtOpt(v);
        b.addEventListener('click', () => app.setSetting(it.id, v));
        br.appendChild(b);
      }
      cfgBody.appendChild(row);
    }
  }
}
function syncCfg(S = app.settings) {
  const q = app.quality;
  for (const b of qBtns) b.classList.toggle('on', b.dataset.q === q);
  for (const b of cfgPresets.children) b.classList.toggle('on', b.dataset.q === q);
  cfgPresetName.textContent = q ? `preset ${PRESET_NAMES[q]}` : 'personalizado';
  cfgBtn.textContent = q ? '⚙ personalizar' : '⚙ personalizado';
  for (const row of cfgBody.querySelectorAll('.cfg-row')) {
    const it = ITEMS[row.dataset.id], v = fmtOpt(S[it.id]);
    for (const b of row.querySelectorAll('.btn')) b.classList.toggle('on', b.dataset.v === v);
    const em = row.querySelector('em');
    em.textContent = it.note ? it.note(S[it.id]) : (it.apply === 'rebake' || it.apply === 'rebuild'
      ? 'recarrega' : '');
  }
  cfgFoot.textContent = `explosões assadas: ${S.bakeRes}³ · ${bakeVramMB(S.bakeRes)} MB de vídeo`
    + ` · simulação ao vivo ${S.liveSim}³ · salvo neste navegador`;
}
buildCfg();
syncCfg();
app.onSettings = (S) => syncCfg(S);
app.onQuality = () => syncCfg();
const toggleCfg = (open = cfg.hidden) => {
  cfg.hidden = !open;
  cfgBtn.classList.toggle('on', open);
};
cfgBtn.addEventListener('click', () => toggleCfg());
$('cfgClose').addEventListener('click', () => toggleCfg(false));
window.addEventListener('keydown', (e) => { if (e.key === 'Escape' && !cfg.hidden) toggleCfg(false); });

// marcador de alvo + flash no cartão a cada detonação
app.onBake = (_, thumbs) => {
  buildCards(thumbs);
  syncBakeUI(app.bakeStatus);
  cardsBox.classList.add('ready');
  setTimeout(() => cardsBox.classList.remove('ready'), 700);
};

// Estado do arsenal. Os cartões ficam no "relógio de construção" até a
// primeira variante existir; as outras duas assam depois, sem bloquear nada.
const armStatus = $('armStatus');
function syncBakeUI(st) {
  const B = app.bake, ready = B.variantsReady > 0 && app.thumbs;
  cardsBox.classList.toggle('building', !ready);
  const p = Math.min(1, B.progress * B.variants);
  cardsBox.style.setProperty('--p', ready ? 1 : p.toFixed(3));
  const pct = `${Math.floor(p * 100)}%`;
  for (const el of cardsBox.querySelectorAll('.card-prog')) el.textContent = pct;
  let txt = '';
  if (!ready) txt = st.phase === 'assando' ? `preparando ${pct}` : 'carregando…';
  else if (st.phase === 'assando') txt = `variações ${B.variantsReady}/${B.variants}`;
  else if (st.phase === 'salvando') txt = 'gravando cache';
  armStatus.textContent = txt;
}
app.onBakeProgress = syncBakeUI;
app.onDetonate = (p) => {
  const sel = document.querySelector('#cards .card.on');
  if (sel) { sel.classList.remove('fired'); void sel.offsetWidth; sel.classList.add('fired'); }
  const r = app.canvas.getBoundingClientRect();
  const m = app.cam.viewProj;
  const v = [p[0], 0, p[2], 1];
  const o = [0, 0, 0, 0];
  for (let i = 0; i < 4; i++) {
    o[i] = m[i] * v[0] + m[4 + i] * v[1] + m[8 + i] * v[2] + m[12 + i] * v[3];
  }
  if (o[3] <= 0) return;
  mark.style.left = `${r.left + (o[0] / o[3] * 0.5 + 0.5) * r.width}px`;
  mark.style.top = `${r.top + (1 - (o[1] / o[3] * 0.5 + 0.5)) * r.height}px`;
  mark.classList.remove('go'); void mark.offsetWidth; mark.classList.add('go');
};

syncTimeUI(app.env.timeOfDay);

const hudFps = $('hudFps'), stGrid = $('stGrid'), stRes = $('stRes'),
      stSteps = $('stSteps'), stTime = $('stTime');

// estado do loop de render
let last = performance.now(), fpsAcc = 0, fpsN = 0, hudT = 0;
const glErrSeen = new Set();

function loop(now) {
  // trava de depuração: captura quadro a quadro sem o loop real interferir
  if (app.hold) { last = now; return; }
  // timestamp do rAF pode vir de um relógio diferente do performance.now()
  // (ou chegar atrasado depois de um bloqueio longo como o bake): piso em 0
  const realDt = Math.min(Math.max((now - last) / 1000, 0), 0.25);
  last = now;
  const t0 = performance.now();
  app.tickBake(realDt);
  app.frameStep(realDt);
  // Erro de GL não lança exceção: o draw é descartado e a tela só fica
  // errada (foi assim que o chão sumia sem nenhuma pista). getError custa uma
  // ida ao processo da GPU, então a checagem é esparsa.
  if (app.frame % 120 === 0) {
    const e = app.gl.getError();
    if (e && !glErrSeen.has(e)) {
      glErrSeen.add(e);
      console.error(`WebGL erro 0x${e.toString(16)} no frame ${app.frame}`);
    }
  }
  const ms = performance.now() - t0;
  fpsAcc += realDt; fpsN++;
  hudT += realDt;
  if (hudT > 0.25) {
    const fps = fpsN / fpsAcc;
    hudFps.textContent = `${fps.toFixed(0)} FPS`;
    stGrid.textContent = `${app.pool.grid.nx}³`;
    stRes.textContent = `${app.rw}×${app.rh}`;
    stSteps.textContent = String(app.vol.params.steps);
    stTime.textContent = `${app.inst.list.length} + ${app.pool.active.length}`;
    if (app.env.autoCycle) syncTimeUI(app.env.timeOfDay);
    syncBakeUI(app.bakeStatus);
    fpsAcc = 0; fpsN = 0; hudT = 0;
  }
}

// Um throw dentro do loop matava o rAF em silêncio e a tela congelava sem
// nenhuma pista. Agora o erro aparece na tela e o loop continua.
function safeLoop(now) {
  if (!window.__firstFrameAt) window.__firstFrameAt = performance.now();
  try {
    loop(now);
  } catch (e) {
    const el = document.getElementById('err');
    el.style.display = 'block';
    el.textContent = 'Erro no loop de render:\n' + (e && e.stack ? e.stack : e);
    console.error(e);
  }
  // re-agenda SEMPRE por aqui — antes o loop re-agendava a si mesmo e a
  // proteção só valia pro primeiro frame
  requestAnimationFrame(safeLoop);
}
requestAnimationFrame(safeLoop);

// Aba oculta: o rAF para e o bake pararia junto. Sem nada pra desenhar, ele
// pode pegar blocos grandes; o navegador ainda acorda timers ~1×/s.
setInterval(() => {
  if (!document.hidden || app.bakeStatus.phase === 'pronto') return;
  try { app.tickBake(0, 120); } catch (e) { console.error(e); }
}, 250);
