// Mede no Chrome o SCRIPT do jogo (Resources/UI/script.html com a ponte de mentira e o Scenarios.json de verdade): a cabeça, a busca,
// os filtros, a faixa da cena armada, o cabeçalho, as linhas (a 1ª e a escolhida), o cartão da direita e o RUN. A tela nativa
// (SRTSScript) põe cada coisa no mesmo pixel. Rodado pelo cap.mjs eval; devolve JSON (Tools/script_layout.json).
(() => {
  const st = document.getElementById('stage').getBoundingClientRect(), k = st.width / 1920;
  const R = r => ({ x: +((r.left - st.left) / k).toFixed(2), y: +((r.top - st.top) / k).toFixed(2), w: +(r.width / k).toFixed(2), h: +(r.height / k).toFixed(2) });
  const box = el => el ? R(el.getBoundingClientRect()) : null;
  const mark = () => { const m = document.createElement('rts-mark'); m.style.cssText = 'display:inline-block;width:0;height:0;vertical-align:baseline;padding:0;border:0;margin:0'; return m; };
  const base = el => { if (!el) return null; const tn = [...el.childNodes].find(n => n.nodeType === 3 && n.data.trim()); if (!tn) return null;
    const w = document.createElement('rts-mark'); tn.replaceWith(w); w.appendChild(tn); const m = mark(); w.appendChild(m);
    const y = (m.getBoundingClientRect().top - st.top) / k; m.remove(); w.replaceWith(tn); return +y.toFixed(2); };
  const lines = el => { const tn = [...el.childNodes].find(n => n.nodeType === 3 && n.data.trim()); if (!tn) return null; const rg = document.createRange(); rg.selectNodeContents(tn); return [...rg.getClientRects()].map(R); };
  const sty = el => { if (!el) return null; const c = getComputedStyle(el); return { font: `${c.fontWeight} ${c.fontSize} ${c.fontFamily.split(',')[0].replace(/"/g, '')}`, ls: c.letterSpacing, lh: c.lineHeight, color: c.color, bg: c.backgroundColor,
    border: c.borderTopWidth + ' ' + c.borderTopStyle + ' ' + c.borderTopColor }; };
  const T = el => el ? { ...box(el), base: base(el), text: el.textContent.trim().slice(0, 80), ...sty(el) } : null;
  const $ = s => document.querySelector(s), $$ = (s, r = document) => [...r.querySelectorAll(s)];
  const res = {};
  res.kicker = T($('.kicker')); res.tag = T($('.kicker .tag')); res.h1 = T($('h1')); res.sub = T($('.sub')); res.code = T($('.sub code'));
  res.search = box($('.search')); res.input = box($('#q')); res.cnt = T($('#cnt'));
  res.chips = $$('.chip').map(T);
  res.armed = { ...box($('#armed')), span: T($('#armed span')), b: T($('#armedn')), em: T($('#armed em')) };
  res.th = $$('.th div').map(T); res.rows = box($('#rows'));
  const rowsEl = $('#rows');
  res.scroll = rowsEl.scrollTop;
  const row = r => ({ ...box(r), cls: r.className, ...sty(r), nm: T(r.querySelector('.nm')), ab: T(r.querySelector('.ab')), mp: T(r.querySelector('.mp')), tm: T(r.querySelector('.tm')) });
  res.row0 = row($$('#rows .sc')[0]); res.rowOn = row($('#rows .sc.on'));
  const card = $('#card');
  res.card = { ...box(card), lab: T(card.querySelector('.lab')), h2: T(card.querySelector('h2')), map: T(card.querySelector('.map')), small: T(card.querySelector('.map small')),
    facts: $$('.facts div', card).map(d => ({ ...box(d), b: T(d.querySelector('b')), span: T(d.querySelector('span')) })),
    p: { ...T(card.querySelector('p')), lines: lines(card.querySelector('p')) }, tags: $$('.tags i', card).map(T), cmd: T(card.querySelector('.cmd')) };
  res.run = T($('#run')); res.runKbd = T($('#run kbd'));
  res.corners = $$('.corner').map(box);
  return JSON.stringify(res);
})()
