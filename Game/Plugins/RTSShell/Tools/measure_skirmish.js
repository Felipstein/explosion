// Mede no Chrome a sala do skirmish aprovada (docs/concept/ui/menu/skirmish.html): a caixa de cada peça e a linha de base de cada
// texto, com a fonte, o espaçamento e as cores do CSS. A sala nativa (SRTSSkirmish) põe cada coisa no mesmo pixel. O marcador da base
// é um elemento sem CSS (o <span> pegava as regras das páginas) e entra junto do texto nas caixas flex/grid. Rodado pelo cap.mjs eval;
// devolve JSON (Tools/skirmish_layout.json).
(() => {
  const st = document.getElementById('stage').getBoundingClientRect(), k = st.width / 1920;
  const box = el => { const r = el.getBoundingClientRect(); return { x: +((r.left - st.left) / k).toFixed(2), y: +((r.top - st.top) / k).toFixed(2), w: +(r.width / k).toFixed(2), h: +(r.height / k).toFixed(2) }; };
  const mark = () => { const m = document.createElement('rts-mark'); m.style.cssText = 'display:inline-block;width:0;height:0;vertical-align:baseline;padding:0;border:0;margin:0'; return m; };
  const base = el => {
    const tn = [...el.childNodes].find(n => n.nodeType === 3 && n.data.trim()); if (!tn) return null;
    const w = document.createElement('rts-mark'); tn.replaceWith(w); w.appendChild(tn); const m = mark(); w.appendChild(m);
    const y = (m.getBoundingClientRect().top - st.top) / k; m.remove(); w.replaceWith(tn); return +y.toFixed(2);
  };
  const style = el => { const c = getComputedStyle(el); return { font: `${c.fontWeight} ${c.fontSize} ${c.fontFamily.split(',')[0].replace(/"/g, '')}`, ls: c.letterSpacing, color: c.color,
    bg: c.backgroundColor, border: c.borderTopWidth + ' ' + c.borderTopStyle + ' ' + c.borderTopColor }; };
  const one = (sel, root = document) => { const el = root.querySelector(sel); return el ? { ...box(el), base: base(el), text: el.textContent.trim().slice(0, 40), ...style(el) } : null; };
  const all = (sel, root = document) => [...root.querySelectorAll(sel)].map(el => ({ ...box(el), base: base(el), text: el.textContent.trim().slice(0, 40), ...style(el) }));
  const res = {};
  res.back = one('.backbtn'); res.backSpan = one('.backbtn span'); res.backKbd = one('.backbtn kbd'); res.backSvg = one('.backbtn svg');
  res.kicker = one('.kicker span'); res.opSmall = one('.op small'); res.opName = one('#opname');
  res.mapbar = one('.mapbar'); res.mapName = one('#mapname'); res.mapMeta = one('#mapmeta'); res.change = one('#change');
  res.bulk = all('#bulk > *'); res.bulkSeg = all('#filldiff span'); res.teamsChev = one('#teamsbtn .chev');
  res.th = all('.th > div');
  res.rows = [...document.querySelectorAll('#rows .tr')].map(tr => ({ ...box(tr), ...style(tr), bar: box(tr.querySelector('.bar')),
    cells: [...tr.querySelectorAll('.cell')].map(c => ({ f: c.dataset.f, ...box(c), v: c.querySelector('.v') ? { ...box(c.querySelector('.v')), base: base(c.querySelector('.v')), text: c.querySelector('.v').textContent, ...style(c.querySelector('.v')) } : null,
      tok: c.querySelector('.ftok, .tok') ? box(c.querySelector('.ftok, .tok')) : null, sw: c.querySelector('.sw') ? box(c.querySelector('.sw')) : null,
      mini: c.querySelector('.mini') ? box(c.querySelector('.mini')) : null, chev: c.querySelector('.chev') ? box(c.querySelector('.chev')) : null })) }));
  res.rules = all('.rule'); res.ruleL = all('.rule .l'); res.cash = one('#cashv'); res.fader = one('#fader'); res.track = one('#fader .track'); res.fill = one('#ffill'); res.knob = one('#fknob');
  res.ticks = all('#fader .tick'); res.tickLabels = all('#fader .tick span'); res.seg = all('#sw span');
  res.start = one('#start'); res.startKbd = one('#start kbd');
  res.corners = all('.corner');
  return JSON.stringify(res);
})()
