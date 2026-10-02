// ---------------------------------------------------------------------------
// profiler.js — timing de GPU real via EXT_disjoint_timer_query_webgl2.
// Sem isso, otimizar shader é chute: o tempo de CPU não diz nada sobre onde
// a GPU está presa. Só uma query pode estar ativa por vez, então as seções
// são medidas em rodízio (uma por frame) e acumuladas com média móvel.
// ---------------------------------------------------------------------------

export class GPUProfiler {
  constructor(gl) {
    this.gl = gl;
    this.ext = gl.getExtension('EXT_disjoint_timer_query_webgl2');
    this.enabled = !!this.ext;
    this.ms = {};          // média móvel por seção
    this.order = [];
    this._pending = [];
    this._active = null;
    this._cursor = 0;
    this._pool = [];
    this._frameSections = [];
  }

  frameStart() {
    this._frameSections = [];
    this._poll();
  }

  // Só UMA query pode estar ativa por vez, mas queries sequenciais no mesmo
  // frame são permitidas — cada seção ganha seu próprio objeto de query.
  begin(name) {
    if (!this.enabled) return;
    if (!this.order.includes(name)) this.order.push(name);
    if (this._active) this.end();
    const gl = this.gl;
    const q = this._pool.pop() || gl.createQuery();
    gl.beginQuery(this.ext.TIME_ELAPSED_EXT, q);
    this._active = { q, name };
  }

  end() {
    if (!this._active) return;
    const gl = this.gl;
    gl.endQuery(this.ext.TIME_ELAPSED_EXT);
    this._pending.push(this._active);
    this._active = null;
    // limite de segurança: queries não colhidas não podem crescer sem fim
    while (this._pending.length > 64) {
      const old = this._pending.shift();
      this._pool.push(old.q);
    }
  }

  frameEnd() {
    this._cursor++;
  }

  _poll() {
    if (!this.enabled) return;
    const gl = this.gl;
    const disjoint = gl.getParameter(this.ext.GPU_DISJOINT_EXT);
    for (let i = this._pending.length - 1; i >= 0; i--) {
      const p = this._pending[i];
      if (disjoint) { this._pool.push(p.q); this._pending.splice(i, 1); continue; }
      if (!gl.getQueryParameter(p.q, gl.QUERY_RESULT_AVAILABLE)) continue;
      const ns = gl.getQueryParameter(p.q, gl.QUERY_RESULT);
      const ms = ns / 1e6;
      this.ms[p.name] = this.ms[p.name] === undefined ? ms : this.ms[p.name] * 0.82 + ms * 0.18;
      this._pool.push(p.q);
      this._pending.splice(i, 1);
    }
  }

  report() {
    if (!this.enabled) return 'timer query indisponível';
    const rows = this.order.map((n) => [n, this.ms[n] ?? 0]);
    const total = rows.reduce((a, r) => a + r[1], 0);
    rows.sort((a, b) => b[1] - a[1]);
    return rows.map(([n, v]) => `${n.padEnd(14)} ${v.toFixed(2)}ms`).join('\n')
      + `\n${'TOTAL'.padEnd(14)} ${total.toFixed(2)}ms`;
  }

  totals() {
    const o = {};
    for (const n of this.order) o[n] = +(this.ms[n] ?? 0).toFixed(2);
    o.TOTAL = +Object.values(o).reduce((a, b) => a + b, 0).toFixed(2);
    return o;
  }
}
