// Mede no Chrome as janelinhas da sala do skirmish abertas (o jogador na vaga 2, a facção, a cor, o time e a largada na vaga 1, e o
// menu do TEAMS): a caixa da janela e de cada item, com a linha de base dos textos. Rodado pelo cap.mjs eval; vai pro
// Tools/skirmish_pops.json.
(() => {
  const st = document.getElementById('stage').getBoundingClientRect(), k = st.width / 1920;
  const box = el => { const r = el.getBoundingClientRect(); return { x: +((r.left - st.left) / k).toFixed(2), y: +((r.top - st.top) / k).toFixed(2), w: +(r.width / k).toFixed(2), h: +(r.height / k).toFixed(2) }; };
  const mark = () => { const m = document.createElement('rts-mark'); m.style.cssText = 'display:inline-block;width:0;height:0;vertical-align:baseline;padding:0;border:0;margin:0'; return m; };
  const base = el => {
    const tn = [...el.childNodes].find(n => n.nodeType === 3 && n.data.trim()); if (!tn) return null;
    const w = document.createElement('rts-mark'); tn.replaceWith(w); w.appendChild(tn); const m = mark(); w.appendChild(m);
    const y = (m.getBoundingClientRect().top - st.top) / k; m.remove(); w.replaceWith(tn); return +y.toFixed(2);
  };
  const pop = document.getElementById('pop');
  const dump = () => ({ ...box(pop), items: [...pop.querySelectorAll('.it, .sep, .grid > div, .tgrid > div')].map(el => ({ cls: el.className, ...box(el), base: base(el), text: el.textContent.trim().slice(0, 30),
    img: el.querySelector('img, .tok, svg') ? box(el.querySelector('img, .tok, svg')) : null, small: el.querySelector('small') ? { ...box(el.querySelector('small')), base: base(el.querySelector('small')) } : null,
    dots: el.querySelector('i') ? box(el.querySelector('i')) : null })) });
  const res = {};
  for (const [name, row, f] of [['who', 1, 'who'], ['fac', 0, 'fac'], ['color', 0, 'color'], ['team', 0, 'team'], ['base', 0, 'base']]) { window.__pop(row, f); res[name] = dump(); }
  document.getElementById('teamsbtn').click(); res.teams = dump();
  return JSON.stringify(res);
})()
