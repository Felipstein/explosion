// Mede no Chrome a tela de opções do jogo (Resources/UI/screens.html?s=opt): a coluna da esquerda, os presets, e em cada aba as
// posições de cada grupo e de cada linha (relativas ao topo do conteúdo que rola), os botões de opção, o deslizante, as teclas, o cartão
// da direita, o medidor e os botões de baixo. A tela nativa (SRTSOptions) faz o mesmo fluxo do CSS; isto confere. Rodado pelo cap.mjs
// eval (UE_MOCK); devolve JSON (Tools/options_layout.json).
(async () => {
  const st = document.getElementById('stage').getBoundingClientRect(), k = st.width / 1920;
  const R = r => ({ x: +((r.left - st.left) / k).toFixed(2), y: +((r.top - st.top) / k).toFixed(2), w: +(r.width / k).toFixed(2), h: +(r.height / k).toFixed(2) });
  const box = el => el ? R(el.getBoundingClientRect()) : null;
  const mark = () => { const m = document.createElement('rts-mark'); m.style.cssText = 'display:inline-block;width:0;height:0;vertical-align:baseline;padding:0;border:0;margin:0'; return m; };
  const base = el => { if (!el) return null; const tn = [...el.childNodes].find(n => n.nodeType === 3 && n.data.trim()); if (!tn) return null;
    const w = document.createElement('rts-mark'); tn.replaceWith(w); w.appendChild(tn); const m = mark(); w.appendChild(m);
    const y = (m.getBoundingClientRect().top - st.top) / k; m.remove(); w.replaceWith(tn); return +y.toFixed(2); };
  const lines = el => { const tn = [...el.childNodes].find(n => n.nodeType === 3 && n.data.trim()); if (!tn) return null; const rg = document.createRange(); rg.selectNodeContents(tn); return [...rg.getClientRects()].map(R); };
  const sty = el => { if (!el) return null; const c = getComputedStyle(el); return { font: `${c.fontWeight} ${c.fontSize} ${c.fontFamily.split(',')[0].replace(/"/g, '')}`, ls: c.letterSpacing, lh: c.lineHeight, color: c.color, bg: c.backgroundColor,
    border: c.borderTopWidth + ' ' + c.borderTopStyle + ' ' + c.borderTopColor, op: c.opacity }; };
  const T = el => el ? { ...box(el), base: base(el), text: el.textContent.trim().slice(0, 80), ...sty(el) } : null;
  const $ = s => document.querySelector(s), $$ = (s, r = document) => [...r.querySelectorAll(s)];
  const wait = ms => new Promise(r => setTimeout(r, ms));
  const res = {};
  res.kicker = T($('#opt .kicker')); res.big = T($('#opt .big'));
  res.tabs = $$('#otabs .tab').map(T);
  res.main = box($('#omain'));
  res.info = { card: box($('#ocard')), meter: box($('#ometer')) };
  res.foot = $$('#opt .foot .btn').map(T);
  const main = $('#omain');
  const rel = el => { const b = box(el), m = box(main); return b && { ...b, y: +(b.y - m.y + main.scrollTop).toFixed(2) }; };
  res.panes = {};
  for (const t of ['gfx', 'snd', 'keys', 'game', 'acc']) {
    $(`#otabs .tab[data-p="${t}"]`).click(); await wait(80);
    const pane = $(`#p-${t}`), m = box(main);
    const items = [];
    for (const el of $$('.grp, .r, .presets, .custom, .search', pane)) {
      const it = { cls: el.className, ...rel(el) };
      if (el.classList.contains('grp')) { it.text = el.textContent; it.base = +(base(el) - m.y + main.scrollTop).toFixed(2); }
      if (el.classList.contains('r')) {
        const n = el.querySelector('.n'); it.n = { ...T(n), base: +(base(n) - m.y + main.scrollTop).toFixed(2) };
        it.seg = $$('.seg span', el).map(s => ({ ...rel(s), base: +(base(s) - m.y + main.scrollTop).toFixed(2), text: s.textContent, cls: s.className, ...sty(s) }));
        const sl = el.querySelector('.sl'); if (sl) it.sl = { ...rel(sl), i: rel(sl.querySelector('i')), b: rel(sl.querySelector('b')) };
        const v = el.querySelector('.v'); if (v) it.v = { ...rel(v), base: +(base(v) - m.y + main.scrollTop).toFixed(2), text: v.textContent, ...sty(v) };
        const kb = el.querySelector('kbd.k'); if (kb) it.kbd = { ...rel(kb), base: +(base(kb) - m.y + main.scrollTop).toFixed(2), text: kb.textContent, ...sty(kb) };
        const cf = el.querySelector('.cf'); if (cf) it.cf = { ...rel(cf), base: +(base(cf) - m.y + main.scrollTop).toFixed(2), text: cf.textContent, ...sty(cf) };
      }
      if (el.classList.contains('search')) { it.svg = rel(el.querySelector('svg')); it.input = rel(el.querySelector('input')); }
      items.push(it);
    }
    res.panes[t] = { scrollH: main.scrollHeight, items };
  }
  // os presets e o cartão (na aba dos gráficos, sem hover) e o medidor
  $('#otabs .tab[data-p="gfx"]').click(); await wait(80);
  res.presets = $$('#presets .pr').map(p => ({ ...box(p), ...sty(p), cls: p.className, b: T(p.querySelector('b')), span: { ...T(p.querySelector('span')), lines: lines(p.querySelector('span')) },
    fps: T(p.querySelector('.fps')), em: T(p.querySelector('.fps em')), bars: $$('.bars i', p).map(i => ({ ...box(i), bg: getComputedStyle(i).backgroundColor })), warn: T(p.querySelector('.warn')) }));
  const card = $('#ocard');
  res.card0 = { lab: T(card.querySelector('.lab')), h4: T(card.querySelector('h4')), p: $$('p', card).map(p => ({ ...T(p), lines: lines(p) })) };
  card.dataset.k = ''; window.card && window.card('shadow');
  res.cardShadow = { lab: T(card.querySelector('.lab')), h4: T(card.querySelector('h4')), p: $$('p', card).map(p => ({ ...T(p), lines: lines(p) })),
    cost: T(card.querySelector('.cost')), costSpans: $$('.cost span', card).map(T), pips: $$('.pips i', card).map(i => ({ ...box(i), cls: i.className, bg: getComputedStyle(i).backgroundColor })) };
  const met = $('#ometer');
  res.meter = { b: T(met.querySelector('b')), em: T(met.querySelector('b em')), lab: T(met.querySelector('.lab')), vbar: box(met.querySelector('.vbar')), fill: box(met.querySelector('.vbar i')),
    vt: $$('.vt span', met).map(T), gpu: T(met.querySelector('.gpu')) };
  return JSON.stringify(res);
})()
