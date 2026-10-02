// ---------------------------------------------------------------------------
// bake.js — "assa" uma explosão simulada numa sequência volumétrica.
//
// POR QUÊ: um RTS dispara explosões várias vezes por segundo. Rodar um solver
// euleriano por instância é inviável — e nenhum jogo faz isso. O padrão da
// indústria (EmberGen, Houdini → engine, e os RTS open-source) é simular
// OFFLINE e instanciar o resultado: o simulador é ferramenta de autoria, não
// de runtime.
//
// Aqui o próprio solver do projeto é o baker. Ele roda uma vez, grava N
// quadros num TEXTURE_2D_ARRAY (uma camada por quadro, cada camada é o mesmo
// atlas de volume achatado que o resto do código já sabe amostrar) e, junto,
// grava a CURVA DE LUZ — centróide, cor e potência por quadro. Assim cada
// instância em runtime custa só raymarch, e a luz dela é um lookup em vez de
// uma redução de GPU.
//
// Quantização: densidade é guardada com gamma (sqrt), não linear. Em 8 bits
// linear o menor passo seria ~2.6× o corte de densidade do solver e a fumaça
// fina ficaria em degraus; com sqrt a precisão se concentra onde a fumaça é
// tênue, que é onde o olho percebe banding.
// ---------------------------------------------------------------------------

import { Shader, drawFS, FS_VS } from './gl.js';
import { COMMON } from './glsl.js';
import { VolumeGrid } from './volume.js';

export const SOOT_SCALE = 4.0;   // faixa representável de fuligem
export const DUST_SCALE = 2.5;
export const TEMP_SCALE = 1.6;
export const FUEL_SCALE = 1.0;
// GLSL não aceita float*int, e `${4.0}` vira "4" em JS. Literais explícitos.
const F = (x) => (Number.isInteger(x) ? x.toFixed(1) : String(x));
const SOOT_F = F(SOOT_SCALE), DUST_F = F(DUST_SCALE), TEMP_F = F(TEMP_SCALE);
const FUEL_F = F(FUEL_SCALE);

export class ExplosionBake {
  /**
   * @param {number} res     resolução do volume assado (cúbica)
   * @param {number} frames  número de quadros
   * @param {number} fps     taxa de amostragem da simulação
   */
  /**
   * @param {number} duration segundos cobertos pela sequência
   * @param {number} curve    >1 concentra quadros no INÍCIO. A bola de fogo
   *   muda tudo nos primeiros 300ms; a fumaça tardia quase não muda. Amostrar
   *   uniformemente gastaria metade da memória em quadros quase idênticos.
   */
  /**
   * @param {number} variants quantas sequências DIFERENTES assar. Todas vivem
   *   no mesmo texture array (layer = variante*quadros + quadro), então custam
   *   um sampler só. Sem variantes, uma barragem repete o mesmo desenho e a
   *   repetição fica óbvia.
   */
  constructor(gl, res, domainSize, frames = 56, duration = 6.0, curve = 1.7, variants = 3) {
    this.gl = gl;
    this.grid = new VolumeGrid(res, domainSize);
    this.frames = frames;
    this.variants = variants;
    this.layers = frames * variants;
    this.duration = duration;
    this.curve = curve;
    this.domainSize = domainSize;

    const { atlasW, atlasH } = this.grid;
    this.tex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D_ARRAY, this.tex);
    gl.texImage3D(gl.TEXTURE_2D_ARRAY, 0, gl.RGBA8, atlasW, atlasH, this.layers, 0,
                  gl.RGBA, gl.UNSIGNED_BYTE, null);
    gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

    // Volume MACRO: extinção máxima por bloco, uma camada por quadro. É o
    // que permite a instância pular espaço vazio — sem ele a marcha gasta
    // quase todos os passos no nada, porque a caixa assada é muito maior que
    // a bola de fogo durante a maior parte da sequência.
    this.macroGrid = new VolumeGrid(Math.max(8, res >> 2), domainSize);
    this.macroTex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D_ARRAY, this.macroTex);
    gl.texImage3D(gl.TEXTURE_2D_ARRAY, 0, gl.R8,
                  this.macroGrid.atlasW, this.macroGrid.atlasH, this.layers, 0,
                  gl.RED, gl.UNSIGNED_BYTE, null);
    for (const pn of ['TEXTURE_MIN_FILTER', 'TEXTURE_MAG_FILTER']) {
      gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl[pn], gl.NEAREST);
    }
    gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

    // COMBUSTÍVEL em meia resolução, uma camada por quadro. É o que marca a
    // frente de chama: o render ao vivo dá +150% de brilho onde ainda há
    // combustível queimando, e sem este canal a instância saía com o fogo
    // chapado. É um multiplicador suave, então 32³ basta (32 KB por quadro).
    this.fuelGrid = new VolumeGrid(Math.max(16, res >> 1), domainSize);
    this.fuelTex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D_ARRAY, this.fuelTex);
    gl.texImage3D(gl.TEXTURE_2D_ARRAY, 0, gl.R8,
                  this.fuelGrid.atlasW, this.fuelGrid.atlasH, this.layers, 0,
                  gl.RED, gl.UNSIGNED_BYTE, null);
    gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

    // curva de luz: posição relativa ao centro da explosão + cor, por quadro
    this.lightPos = new Float32Array(this.layers * 3);
    this.lightCol = new Float32Array(this.layers * 3);

    this._fbo = gl.createFramebuffer();
    this._shM = this._macroShader();
    this.variantsReady = 0;
    this.bounds = null;      // caixa justa por quadro (computeBounds)
    this.frameTimes = new Float32Array(frames);
    for (let f = 0; f < frames; f++) this.frameTimes[f] = this.timeOfFrame(f);
    this.ready = false;
  }

  /** tempo coberto por um quadro (não-uniforme: denso no início) */
  timeOfFrame(f) {
    return this.duration * Math.pow(f / (this.frames - 1), this.curve);
  }

  /** inverso: quadro contínuo correspondente a um tempo */
  frameOfTime(t) {
    const u = Math.min(Math.max(t / this.duration, 0), 1);
    return Math.pow(u, 1 / this.curve) * (this.frames - 1);
  }

  /** max-reduce do atlas assado para o atlas macro, dilatado 1 bloco */
  _macroShader() {
    const gl = this.gl;
    const R = Math.max(1, Math.round(this.grid.nx / this.macroGrid.nx));
    return new Shader(gl, FS_VS,
      `#version 300 es\nprecision highp float;\nprecision highp sampler2DArray;\nin vec2 vUV;\n`
      + COMMON + this.grid.glsl('B') + this.macroGrid.glsl('M') + `
uniform sampler2DArray uBake;
uniform float uLayer;
out vec4 oCol;
#define R ${R}
float at(ivec3 v){
  v = clamp(v, ivec3(0), GRIDIB - 1);
  int tx = v.z % TILES_XB, ty = v.z / TILES_XB;
  vec4 q = texelFetch(uBake, ivec3(tx * GRIDIB.x + v.x, ty * GRIDIB.y + v.y, int(uLayer)), 0);
  // densidade está em gamma; aqui basta a ordem de grandeza
  return max(q.r, q.b);
}
void main(){
  ivec3 mv = ivec3(fragToVoxelM(gl_FragCoord.xy));
  // Dilata MEIO BLOCO pra cada lado. Com apenas 1 voxel de folga o
  // deslocamento por ruído da marcha já saía do bloco testado e apareciam
  // arestas retas na fumaça — pedaços inteiros pulados indevidamente.
  float mx = 0.0;
  for (int z = -R/2; z <= R + R/2; z++)
    for (int y = -R/2; y <= R + R/2; y++)
      for (int x = -R/2; x <= R + R/2; x++)
        mx = max(mx, at(mv * R + ivec3(x, y, z)));
  oCol = vec4(mx, 0.0, 0.0, 1.0);
}`, 'bakeMacro');
  }

  /**
   * Shader que reduz o atlas da simulação para o atlas assado.
   *
   * A temperatura NÃO é a média simples. Emissão ∝ fuligem·I(T) com I muito
   * convexo, e uma frente de chama fina e quente fica diluída na média com
   * as células frias ao lado: a média de T apagava justamente o que brilha.
   * Aqui a temperatura assada é a que reproduz a emissão MÉDIA do bloco,
   * I(T*) = Σ fuligem·I(T) / Σ fuligem — a energia emitida se conserva na
   * resolução menor. I é invertida por bisseção na própria LUT de corpo negro.
   */
  _downsampleShader(srcGrid, lightGrid) {
    const gl = this.gl;
    // Razão entre as grades e subamostras por eixo. A razão pode ser
    // fracionária (128→96, 128→80): cada voxel assado é a média de N³
    // amostras TRILINEARES espalhadas na pegada dele. Com razão 2 (128→64)
    // as amostras caem exatamente nos centros dos voxels da simulação, e o
    // resultado é idêntico à média de blocos 2³.
    const ratio = srcGrid.nx / this.grid.nx;
    const N = Math.max(1, Math.ceil(ratio - 1e-6));
    return new Shader(gl, FS_VS,
      `#version 300 es\nprecision highp float;\nin vec2 vUV;\n` + COMMON
      + srcGrid.glsl('S') + lightGrid.glsl('L') + this.grid.glsl('B') + `
uniform sampler2D uFields, uLight, uBB;
uniform float uTempScale, uEmissionCurve;
out vec4 oCol;
#define N ${N}
#define RATIO ${F(ratio)}
float Iof(float T){
  return pow(max(texture(uBB, vec2(saturate(T * uTempScale), 0.5)).a, 0.0), uEmissionCurve);
}
void main(){
  vec3 bv = floor(fragToVoxelB(gl_FragCoord.xy));
  vec4 sum = vec4(0.0);
  float sE = 0.0;
  for (int z = 0; z < N; z++)
    for (int y = 0; y < N; y++)
      for (int x = 0; x < N; x++){
        vec3 sp = (bv + (vec3(x, y, z) + 0.5) / float(N)) * RATIO;
        vec4 q = sampleVolS(uFields, sp);
        sum += q;
        sE += q.r * Iof(q.g);
      }
  vec4 f = sum / float(N * N * N);
  float T = f.g;
  if (sum.r > 1e-5){
    float target = sE / sum.r;
    float lo = 0.0, hi = 1.0 / uTempScale;
    for (int k = 0; k < 14; k++){
      float mid = 0.5 * (lo + hi);
      if (Iof(mid) < target) lo = mid; else hi = mid;
    }
    // nunca abaixo da média: em fumaça fria a média já é a resposta certa
    T = max(f.g, 0.5 * (lo + hi));
  }
  // alpha = transmitância do CÉU, que é independente de vista e quase
  // independente da direção do sol. É a oclusão que as instâncias usam em
  // runtime quando não têm cache de luz.
  float skyT = sampleVolL(uLight, worldToVoxelL(voxelToWorldB(bv + 0.5))).y;
  // gamma na densidade: 8 bits lineares deixariam a fumaça fina em degraus
  oCol = vec4(sqrt(saturate(f.r / ${SOOT_F})),
              saturate(T / ${TEMP_F}),
              sqrt(saturate(f.a / ${DUST_F})),
              saturate(skyT));
}`, 'bakeDownsample');
  }

  /** combustível: média na pegada de cada voxel da grade de meia resolução */
  _fuelShader(srcGrid) {
    const gl = this.gl;
    const ratio = srcGrid.nx / this.fuelGrid.nx;
    const N = Math.max(1, Math.ceil(ratio - 1e-6));
    return new Shader(gl, FS_VS,
      `#version 300 es\nprecision highp float;\nin vec2 vUV;\n` + COMMON
      + srcGrid.glsl('S') + this.fuelGrid.glsl('U') + `
uniform sampler2D uFields;
out vec4 oCol;
#define N ${N}
#define RATIO ${F(ratio)}
void main(){
  vec3 bv = floor(fragToVoxelU(gl_FragCoord.xy));
  float s = 0.0;
  for (int z = 0; z < N; z++)
    for (int y = 0; y < N; y++)
      for (int x = 0; x < N; x++)
        s += sampleVolS(uFields, (bv + (vec3(x, y, z) + 0.5) / float(N)) * RATIO).b;
  // gamma: o combustível relevante pra chama fica entre 0.02 e 0.35
  oCol = vec4(sqrt(saturate(s / float(N * N * N) / ${FUEL_F})), 0.0, 0.0, 1.0);
}`, 'bakeFuel');
  }

  /**
   * Passo do solver: 1/60, o MESMO da explosão ao vivo a 60 FPS. A
   * combustão é integrada de forma explícita e depende do passo; com 1/120
   * a instância queimava menos e subia menos que a explosão da tecla espaço
   * (comparado lado a lado), e é a ao vivo que define o visual de
   * referência. Passo variável foi descartado: a difusão numérica depende do
   * número de passos e a fumaça tardia mudava de duração.
   */
  dtAt() { return 1 / 60; }

  /**
   * Prepara o bake INCREMENTAL. O antigo run() fazia tudo de uma vez e
   * travava a página ~16s antes do primeiro frame: tela preta, HUD morto.
   * Agora o loop chama advance() com um orçamento por frame e a cena, o HUD
   * e o clique funcionam desde o início.
   *
   * @param {FluidSim} fluid        solver DEDICADO (não um slot do pool)
   * @param {VolumeRenderer} probe  redutor da curva de luz, na grade do solver
   * @param {object} opts           { keyDir, sootExt, dustExt, erodeMean, pos, seed }
   */
  begin(fluid, probe, opts) {
    this._job = {
      fluid, probe, opts,
      pos: opts.pos || [0, 1.85, 0],
      sh: this._downsampleShader(fluid.grid, fluid.lightGrid),
      shFuel: this._fuelShader(fluid.grid),
      slot: { fluid, fire: null },
      v: 0, f: 0, simT: 0, started: false,
      // leituras da curva de luz em voo: {layer, buf, fence}, em ordem
      pending: [], freeBufs: [], harvested: 0,
    };
    this.variantsReady = 0;
    this.ready = false;
    this.bounds = null;
  }

  /** fração concluída, pelo tempo simulado */
  get progress() {
    if (this.ready) return 1;
    const J = this._job;
    if (!J) return 0;
    return Math.min(1, (J.v * this.duration + Math.min(J.simT, this.duration))
                       / (this.variants * this.duration));
  }

  /**
   * Avança até `maxSteps` passos do solver. Variantes ficam prontas uma a
   * uma (`variantsReady`) e já podem ser instanciadas antes do bake acabar.
   * @returns {boolean} true quando todas as variantes terminaram
   */
  advance(maxSteps, force = false) {
    const J = this._job;
    if (!J) return this.ready;
    const gl = this.gl, o = J.opts;
    this._harvest(J, false);
    gl.disable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);
    let n = 0;
    while (n < maxSteps && J.v < this.variants) {
      if (!J.started) {
        J.fluid.detonate(J.pos, (o.seed ?? 1234) + J.v * 977);
        J.slot.fire = null;
        J.simT = 0;
        J.started = true;
      }
      if (J.simT < this.timeOfFrame(J.f) - 1e-6) {
        const dt = this.dtAt(J.simT);
        J.fluid.step(dt, o.keyDir, o.sootExt, o.dustExt,
                     J.slot.fire ? J.slot.fire.pos : J.fluid.blastPos, o.erodeMean);
        J.simT += dt;
        n++;
        continue;
      }
      this._writeFrame(J, J.v * this.frames + J.f);
      if (++J.f === this.frames) { J.f = 0; J.v++; J.started = false; }
    }
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    // tudo simulado: só falta colher as últimas leituras de luz
    if (J.v === this.variants) this._harvest(J, force);
    // uma variante só é exposta quando a curva de luz dela inteira chegou
    this.variantsReady = Math.min(J.v, Math.floor(J.harvested / this.frames));
    if (this.variantsReady === this.variants) {
      for (const b of J.freeBufs) gl.deleteBuffer(b);
      this.ready = true;
      this._job = null;
    }
    return this.ready;
  }

  /**
   * Colhe leituras de luz cuja fence já sinalizou, EM ORDEM (a suavização
   * do centróide é sequencial). readPixels síncrono custava ~12ms de
   * ida-e-volta ao processo da GPU por quadro assado; assim custa zero.
   */
  _harvest(J, force) {
    const gl = this.gl, vol = J.probe;
    while (J.pending.length) {
      const r = J.pending[0];
      if (!force && gl.getSyncParameter(r.fence, gl.SYNC_STATUS) !== gl.SIGNALED) break;
      J.pending.shift();
      gl.deleteSync(r.fence);
      gl.bindBuffer(gl.PIXEL_PACK_BUFFER, r.buf);
      gl.getBufferSubData(gl.PIXEL_PACK_BUFFER, 0, vol._readBuf);
      gl.bindBuffer(gl.PIXEL_PACK_BUFFER, null);
      J.freeBufs.push(r.buf);
      vol._applyFireLight(J.slot);
      const L = J.slot.fire, l = r.layer;
      this.lightPos[l * 3] = L.pos[0] - J.pos[0];
      this.lightPos[l * 3 + 1] = L.pos[1];
      this.lightPos[l * 3 + 2] = L.pos[2] - J.pos[2];
      this.lightCol.set(L.color, l * 3);
      J.harvested++;
    }
  }

  /** compat: bake inteiro, síncrono (ferramentas de inspeção) */
  run(fluid, probe, opts) {
    this.begin(fluid, probe, opts);
    while (!this.advance(1e9, true));
    return this;
  }

  _writeFrame(J, layer) {
    const gl = this.gl, fluid = J.fluid, vol = J.probe;
    // redução da luz do quadro (sem leitura: noReadback) e cópia ASSÍNCRONA
    // do resultado 1×1 para um PBO, colhida frames depois em _harvest
    vol.noReadback = true;
    vol.updateFireLight(J.slot);
    const last = vol.reduceChain[vol.reduceChain.length - 1].t;
    let buf = J.freeBufs.pop();
    if (!buf) {
      buf = gl.createBuffer();
      gl.bindBuffer(gl.PIXEL_PACK_BUFFER, buf);
      gl.bufferData(gl.PIXEL_PACK_BUFFER, 8 * 4, gl.STREAM_READ);
    }
    gl.bindFramebuffer(gl.FRAMEBUFFER, last.fbo);
    gl.bindBuffer(gl.PIXEL_PACK_BUFFER, buf);
    gl.readBuffer(gl.COLOR_ATTACHMENT0);
    gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.FLOAT, 0);
    gl.readBuffer(gl.COLOR_ATTACHMENT1);
    gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.FLOAT, 16);
    gl.readBuffer(gl.COLOR_ATTACHMENT0);
    gl.bindBuffer(gl.PIXEL_PACK_BUFFER, null);
    J.pending.push({ layer, buf, fence: gl.fenceSync(gl.SYNC_GPU_COMMANDS_COMPLETE, 0) });

    gl.disable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);
    gl.bindFramebuffer(gl.FRAMEBUFFER, this._fbo);
    gl.framebufferTextureLayer(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, this.tex, 0, layer);
    gl.drawBuffers([gl.COLOR_ATTACHMENT0]);
    gl.viewport(0, 0, this.grid.atlasW, this.grid.atlasH);
    J.sh.use().set('uDomainOrigin', fluid.domainOrigin)
      .set('uTempScale', vol.params.tempScale).set('uEmissionCurve', vol.params.emissionCurve)
      .tex('uFields', fluid.fields.read.tex).tex('uLight', fluid.light.tex).tex('uBB', vol.bbTex);
    drawFS(gl);
    this._writeMacro(layer);

    gl.bindFramebuffer(gl.FRAMEBUFFER, this._fbo);
    gl.framebufferTextureLayer(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, this.fuelTex, 0, layer);
    gl.drawBuffers([gl.COLOR_ATTACHMENT0]);
    gl.viewport(0, 0, this.fuelGrid.atlasW, this.fuelGrid.atlasH);
    J.shFuel.use().set('uDomainOrigin', fluid.domainOrigin).tex('uFields', fluid.fields.read.tex);
    drawFS(gl);
  }

  /** macro de um quadro, a partir do que já está gravado na camada */
  _writeMacro(layer) {
    const gl = this.gl;
    gl.bindFramebuffer(gl.FRAMEBUFFER, this._fbo);
    gl.framebufferTextureLayer(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, this.macroTex, 0, layer);
    gl.drawBuffers([gl.COLOR_ATTACHMENT0]);
    gl.viewport(0, 0, this.macroGrid.atlasW, this.macroGrid.atlasH);
    this._shM.use().set('uLayer', layer).tex('uBake', this.tex, gl.TEXTURE_2D_ARRAY);
    drawFS(gl);
  }

  // ---- cache em disco -----------------------------------------------------
  // Um jogo não simula a explosão no boot: ela vem pronta, como asset. O
  // bake só roda quando o código do solver muda (a chave é um hash das
  // fontes); o resultado vira um arquivo que as próximas cargas só baixam.

  get layerBytes() { return this.grid.atlasW * this.grid.atlasH * 4; }

  /**
   * Caixa JUSTA de cada quadro (espaço local, metros): onde a fumaça existe
   * de fato. A caixa do domínio tem 38 m, e no começo a bola de fogo ocupa
   * uma fração dela; com a caixa justa o recorte em tela e o trecho de marcha
   * encolhem, e só se agrupam pra marcha conjunta explosões que se tocam.
   * Amostra 1 voxel a cada 2 e alarga 2 células pra compensar.
   */
  computeBounds(vox) {
    const G = this.grid, { nx, ny, nz, tilesX, atlasW, cell } = G, mn = G.domainMin;
    const out = new Float32Array(this.layers * 6);
    const per = this.layerBytes;
    for (let l = 0; l < this.layers; l++) {
      let x0 = 1e9, y0 = 1e9, z0 = 1e9, x1 = -1e9, y1 = -1e9, z1 = -1e9;
      const base = l * per;
      for (let z = 0; z < nz; z += 2) {
        const tx = z % tilesX, ty = (z / tilesX) | 0;
        for (let y = 0; y < ny; y += 2) {
          let i = base + ((ty * ny + y) * atlasW + tx * nx) * 4;
          for (let x = 0; x < nx; x += 2, i += 8) {
            if (vox[i] > 3 || vox[i + 2] > 3) {
              if (x < x0) x0 = x; if (x > x1) x1 = x;
              if (y < y0) y0 = y; if (y > y1) y1 = y;
              if (z < z0) z0 = z; if (z > z1) z1 = z;
            }
          }
        }
      }
      if (x1 < x0) continue;   // quadro vazio: caixa nula (zeros)
      const pad = 2;
      x0 = Math.max(0, x0 - pad); y0 = Math.max(0, y0 - pad); z0 = Math.max(0, z0 - pad);
      x1 = Math.min(nx, x1 + 1 + pad); y1 = Math.min(ny, y1 + 1 + pad); z1 = Math.min(nz, z1 + 1 + pad);
      out.set([mn[0] + x0 * cell, mn[1] + y0 * cell, mn[2] + z0 * cell,
               mn[0] + x1 * cell, mn[1] + y1 * cell, mn[2] + z1 * cell], l * 6);
    }
    this.bounds = out;
    return out;
  }
  get fuelLayerBytes() { return this.fuelGrid.atlasW * this.fuelGrid.atlasH; }

  /** lê o combustível das camadas [from, to) — R8 só sai como RGBA */
  readFuelLayers(from, to, out) {
    const gl = this.gl, { atlasW, atlasH } = this.fuelGrid;
    const tmp = new Uint8Array(atlasW * atlasH * 4);
    gl.bindFramebuffer(gl.FRAMEBUFFER, this._fbo);
    for (let l = from; l < to; l++) {
      gl.framebufferTextureLayer(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, this.fuelTex, 0, l);
      gl.readBuffer(gl.COLOR_ATTACHMENT0);
      gl.readPixels(0, 0, atlasW, atlasH, gl.RGBA, gl.UNSIGNED_BYTE, tmp);
      const o = l * this.fuelLayerBytes;
      for (let i = 0, n = atlasW * atlasH; i < n; i++) out[o + i] = tmp[i * 4];
    }
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }

  /** lê camadas [from, to) da textura para `out` (síncrono, ~1MB cada) */
  readLayers(from, to, out) {
    const gl = this.gl, { atlasW, atlasH } = this.grid;
    gl.bindFramebuffer(gl.FRAMEBUFFER, this._fbo);
    for (let l = from; l < to; l++) {
      gl.framebufferTextureLayer(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, this.tex, 0, l);
      gl.readBuffer(gl.COLOR_ATTACHMENT0);
      gl.readPixels(0, 0, atlasW, atlasH, gl.RGBA, gl.UNSIGNED_BYTE,
                    out, l * this.layerBytes);
    }
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  }

  /** cabeçalho + curva de luz + voxels, num único buffer */
  pack(voxels, fuel, meta = {}) {
    if (!this.bounds) this.computeBounds(voxels);
    const head = new TextEncoder().encode(JSON.stringify({
      ...meta, bounds: Array.from(this.bounds, (v) => +v.toFixed(2)), res: this.grid.nx, fuelRes: this.fuelGrid.nx, frames: this.frames,
      variants: this.variants, duration: this.duration, curve: this.curve,
    }));
    const headLen = (head.length + 3) & ~3;
    const fl = this.layers * 3 * 4;
    const out = new Uint8Array(8 + headLen + fl * 2 + voxels.length + fuel.length);
    const dv = new DataView(out.buffer);
    dv.setUint32(0, 0x314b4258, true);   // 'XBK1'
    dv.setUint32(4, headLen, true);
    out.set(head, 8);
    out.set(new Uint8Array(this.lightPos.buffer), 8 + headLen);
    out.set(new Uint8Array(this.lightCol.buffer), 8 + headLen + fl);
    out.set(voxels, 8 + headLen + fl * 2);
    out.set(fuel, 8 + headLen + fl * 2 + voxels.length);
    return out;
  }

  /** inverso de pack(): sobe tudo pra GPU e reconstrói o macro */
  unpack(buf) {
    const gl = this.gl;
    const dv = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
    if (dv.getUint32(0, true) !== 0x314b4258) throw new Error('bake: formato desconhecido');
    const headLen = dv.getUint32(4, true);
    const meta = JSON.parse(new TextDecoder().decode(buf.subarray(8, 8 + headLen)).replace(/\s+$/, '').replace(/\0+$/, ''));
    if (meta.res !== this.grid.nx || meta.frames !== this.frames || meta.variants !== this.variants) {
      throw new Error('bake: dimensões não batem');
    }
    const fl = this.layers * 3 * 4, off = 8 + headLen;
    this.lightPos.set(new Float32Array(buf.slice(off, off + fl).buffer));
    this.lightCol.set(new Float32Array(buf.slice(off + fl, off + 2 * fl).buffer));
    const nv = this.layers * this.layerBytes, nf = this.layers * this.fuelLayerBytes;
    if (meta.fuelRes !== this.fuelGrid.nx || buf.length - off - 2 * fl !== nv + nf) {
      throw new Error('bake: tamanho inválido');
    }
    const vox = buf.subarray(off + 2 * fl, off + 2 * fl + nv);
    if (Array.isArray(meta.bounds) && meta.bounds.length === this.layers * 6) {
      this.bounds = Float32Array.from(meta.bounds);
    } else {
      this.computeBounds(vox);   // asset anterior às caixas justas
    }
    const fuel = buf.subarray(off + 2 * fl + nv);
    gl.bindTexture(gl.TEXTURE_2D_ARRAY, this.tex);
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
    gl.texSubImage3D(gl.TEXTURE_2D_ARRAY, 0, 0, 0, 0, this.grid.atlasW, this.grid.atlasH,
                     this.layers, gl.RGBA, gl.UNSIGNED_BYTE, vox);
    gl.bindTexture(gl.TEXTURE_2D_ARRAY, this.fuelTex);
    gl.texSubImage3D(gl.TEXTURE_2D_ARRAY, 0, 0, 0, 0, this.fuelGrid.atlasW, this.fuelGrid.atlasH,
                     this.layers, gl.RED, gl.UNSIGNED_BYTE, fuel);
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 4);
    gl.disable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);
    for (let l = 0; l < this.layers; l++) this._writeMacro(l);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    this._job = null;
    this.variantsReady = this.variants;
    this.ready = true;
    return meta;
  }

  /** prelude do combustível (grade de 32³ com sufixo próprio) */
  fuelGlsl(s = 'U') {
    return this.fuelGrid.glsl(s) + `
uniform sampler2DArray uBakeFuel${s};
float fuelLayer${s}(vec3 p, float layer){
  float zc = clamp(p.z, 0.5, GRID${s}.z - 0.5);
  float z0 = floor(zc - 0.5), fz = zc - 0.5 - z0;
  return mix(texture(uBakeFuel${s}, vec3(tileUV${s}(p.xy, z0), layer)).r,
             texture(uBakeFuel${s}, vec3(tileUV${s}(p.xy, z0 + 1.0), layer)).r, fz);
}
// combustível no ponto LOCAL lp, no quadro contínuo (base da variante já somada)
float sampleFuel${s}(vec3 lp, float frame, float frames, float base){
  vec3 p = worldToVoxelAt${s}(lp, vec3(0.0));
  float f0 = floor(frame), ft = frame - f0;
  float f1 = min(f0 + 1.0, frames - 1.0);
  float q = mix(fuelLayer${s}(p, base + f0), fuelLayer${s}(p, base + f1), ft);
  return q * q * ${FUEL_F};
}
`;
  }

  /**
   * Prelude GLSL. Os sufixos são parametrizáveis porque a CENA já usa 'M'
   * para a grade macro do solver ao vivo e precisa dos dois ao mesmo tempo.
   */
  glsl(g = 'B', m = 'M') {
    return this.grid.glsl(g) + this.macroGrid.glsl(m) + `
uniform sampler2DArray uBake${g};
uniform float uBake${g}Frames;   // quadro contínuo vem pronto da CPU

// Uma camada = um quadro, e cada camada é o mesmo atlas achatado do resto do
// projeto. Amostrar custa 4 taps: 2 pro lerp em Z, ×2 pro lerp TEMPORAL.
// Sem o lerp temporal a 24 quadros a animação pisca visivelmente.
vec4 sampleBakeLayer${g}(vec3 p, float layer){
  float zc = clamp(p.z, 0.5, GRID${g}.z - 0.5);
  float z0 = floor(zc - 0.5);
  float fz = zc - 0.5 - z0;
  vec2 uv0 = tileUV${g}(p.xy, z0), uv1 = tileUV${g}(p.xy, z0 + 1.0);
  return mix(texture(uBake${g}, vec3(uv0, layer)), texture(uBake${g}, vec3(uv1, layer)), fz);
}

// retorna (fuligem, temperatura, poeira) já desquantizados
uniform sampler2DArray uBakeMacro${m};

// ocupação do bloco que contém p (coordenada de voxel macro), no quadro dado
float bakeMacroAt${m}(vec3 pm, float layer){
  ivec3 v = clamp(ivec3(floor(pm)), ivec3(0), GRIDI${m} - 1);
  int tx = v.z % TILES_X${m}, ty = v.z / TILES_X${m};
  return texelFetch(uBakeMacro${m}, ivec3(tx * GRIDI${m}.x + v.x, ty * GRIDI${m}.y + v.y, int(layer)), 0).r;
}

// distância até sair do bloco macro atual, no espaço LOCAL da instância
float bakeMacroExit${m}(vec3 lp, vec3 dirLocal){
  vec3 vp = worldToVoxelAt${m}(lp, vec3(0.0));
  vec3 dv = dirLocal * INV_CELL${m};
  vec3 nb = floor(vp) + step(vec3(0.0), dv);
  vec3 tb = (nb - vp) / dv;
  return max(min(min(tb.x, tb.y), tb.z), 0.0) ;
}

// retorna também a transmitância do céu no alpha
uniform float uTemporalLerp${g};   // 0 = um quadro só (metade dos taps)

uniform float uVariantBase${g};    // variante * quadros

vec4 sampleBake4${g}(vec3 voxel, float frame){
  float f0 = floor(frame);
  float ft = frame - f0;
  if (uTemporalLerp${g} < 0.5){
    vec4 q = sampleBakeLayer${g}(voxel, uVariantBase${g} + f0);
    return vec4(q.r * q.r * ${SOOT_F}, q.g * ${TEMP_F}, q.b * q.b * ${DUST_F}, q.a);
  }
  float f1 = min(f0 + 1.0, uBake${g}Frames - 1.0);
  vec4 m = mix(sampleBakeLayer${g}(voxel, uVariantBase${g} + f0),
               sampleBakeLayer${g}(voxel, uVariantBase${g} + f1), ft);
  return vec4(m.r * m.r * ${SOOT_F}, m.g * ${TEMP_F},
              m.b * m.b * ${DUST_F}, m.a);
}

vec3 sampleBake(vec3 voxel, float frame){
  float f0 = floor(frame);
  float ft = frame - f0;
  float f1 = min(f0 + 1.0, uBake${g}Frames - 1.0);
  vec4 a = sampleBakeLayer${g}(voxel, f0);
  vec4 b = sampleBakeLayer${g}(voxel, f1);
  vec4 m = mix(a, b, ft);
  return vec3(m.r * m.r * ${SOOT_F}, m.g * ${TEMP_F}, m.b * m.b * ${DUST_F});
}
`;
  }
}
