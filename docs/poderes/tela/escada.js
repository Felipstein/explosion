// (03/11, em teste) A escada: as patentes viram degraus de cima pra baixo (o 1 em cima, o 5 embaixo), como no ZH e no XCOM 2.
// "parece que não tem ordem, hierarquia, fluxo, sentido, direção". Um eixo só manda: descer = subir de patente. Cada degrau mostra
// os poderes daquela patente; o nível seguinte de um poder fica logo abaixo (a mesma coluna, a linha vertical). O seu degrau acende,
// os de cima ficam conquistados e os de baixo apagados. As colunas são as linhas de antes, na mesma ordem (só em pé).
genRenderTrack = function () {
  const lv = +$('#rlv').textContent, fr = parseFloat($('#rfill').style.width) || 0;
  const lanes = ROWS.length;
  let h = '<svg class="lines"></svg>';
  for (let r = 1; r <= 5; r++) {
    const st = r === lv ? 'cur' : r > lv ? 'fut' : 'past';
    h += `<div class="rh ${st}"><img class="ri" src="a/${rankIcon(r)}" alt=""><div class="rt"><b>${RANKS[r - 1][1]}</b>`
      + `<span class="gv">+${PTS_PER_RANK[r - 1]} ${TOK('', 'm')}</span>${r === lv + 1 ? `<div class="xp"><i style="width:${fr}%"></i></div>` : ''}</div></div>`;
    for (let ri = 0; ri < lanes; ri++) {
      const n = NODES.find(x => x.row === ri && x.rank === r);
      h += `<div class="cell ${st}">${n ? tileHTML(n) : ''}</div>`;
    }
  }
  const t = $('#gen .trk'); t.innerHTML = h; t.classList.add('esc');
  requestAnimationFrame(() => {
    const o = t.getBoundingClientRect(), k = o.width / t.offsetWidth; let L = '';
    NODES.filter(n => n.req).forEach(n => {
      const a = t.querySelector(`.pw[data-id="${n.req}"]`), b = t.querySelector(`.pw[data-id="${n.id}"]`); if (!a || !b) return;
      const ra = a.getBoundingClientRect(), rb = b.getBoundingClientRect(), x = ((ra.left + ra.right) / 2 - o.left) / k;
      L += `<line x1="${x}" y1="${(ra.bottom - o.top) / k}" x2="${x}" y2="${(rb.top - o.top) / k}" stroke="${HAVE[n.id] ? '#4f9b59' : '#3a4757'}" stroke-width="2"/>`;
    });
    t.querySelector('svg.lines').innerHTML = L;
  });
};
