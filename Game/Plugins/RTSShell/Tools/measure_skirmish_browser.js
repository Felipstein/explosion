// Mede no Chrome o navegador de mapas do skirmish aprovado (aberto com ?browser=1): o título, a busca, a contagem, os filtros e os
// cartões (a miniatura, o nome, a ficha, as etiquetas e a estrela). Rodado pelo cap.mjs eval; vai pro Tools/skirmish_browser.json.
(() => {
  const st = document.getElementById('stage').getBoundingClientRect(), k = st.width / 1920;
  const box = el => { const r = el.getBoundingClientRect(); return { x: +((r.left - st.left) / k).toFixed(2), y: +((r.top - st.top) / k).toFixed(2), w: +(r.width / k).toFixed(2), h: +(r.height / k).toFixed(2) }; };
  const mark = () => { const m = document.createElement('rts-mark'); m.style.cssText = 'display:inline-block;width:0;height:0;vertical-align:baseline;padding:0;border:0;margin:0'; return m; };
  const base = el => {
    const tn = [...el.childNodes].find(n => n.nodeType === 3 && n.data.trim()); if (!tn) return null;
    const w = document.createElement('rts-mark'); tn.replaceWith(w); w.appendChild(tn); const m = mark(); w.appendChild(m);
    const y = (m.getBoundingClientRect().top - st.top) / k; m.remove(); w.replaceWith(tn); return +y.toFixed(2);
  };
  const one = (sel, root = document) => { const el = root.querySelector(sel); return el ? { ...box(el), base: base(el), text: el.textContent.trim().slice(0, 30) } : null; };
  const res = {};
  res.h2 = one('.bw-head h2'); res.search = one('.search'); res.searchSvg = one('.search svg'); res.input = one('#q'); res.count = one('#bcount');
  res.close = one('#bclose'); res.closeSpan = one('#bclose span');
  res.groups = [...document.querySelectorAll('#filters .fg')].map(g => ({ ...box(g), label: g.querySelector('.l') ? { ...box(g.querySelector('.l')), base: base(g.querySelector('.l')), text: g.querySelector('.l').textContent } : null,
    chips: [...g.querySelectorAll('.cs span')].map(c => ({ ...box(c), base: base(c), text: c.textContent, on: c.classList.contains('on') })) }));
  res.cards = [...document.querySelectorAll('.mc')].slice(0, 3).map(c => ({ cls: c.className, ...box(c), canvas: c.querySelector('canvas') ? box(c.querySelector('canvas')) : null,
    n: one('.n', c), m: one('.m', c), src: one('.src', c), nav: one('.nav', c), fav: c.querySelector('.fav') ? box(c.querySelector('.fav')) : null,
    plus: one('.plus', c), gt: one('.gt', c), gs: one('.gs', c) }));
  res.grid = one('#bgrid');
  return JSON.stringify(res);
})()
