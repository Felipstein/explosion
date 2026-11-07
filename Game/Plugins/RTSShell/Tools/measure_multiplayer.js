// Mede no Chrome a lista de salas do multiplayer como o jogo mostra (Resources/UI/multiplayer.html com a ponte de mentira e duas
// salas da LAN): a caixa de cada peça e a linha de base de cada texto. A tela nativa (SRTSMultiplayer) põe cada coisa no mesmo pixel.
// Rodado pelo cap.mjs eval (com UE_MOCK e os dados de mentira do api/lan.json); devolve JSON (Tools/multiplayer_layout.json).
(() => {
  const st = document.getElementById('stage').getBoundingClientRect(), k = st.width / 1920;
  const R = r => ({ x: +((r.left - st.left) / k).toFixed(2), y: +((r.top - st.top) / k).toFixed(2), w: +(r.width / k).toFixed(2), h: +(r.height / k).toFixed(2) });
  const box = el => el ? R(el.getBoundingClientRect()) : null;
  const mark = () => { const m = document.createElement('rts-mark'); m.style.cssText = 'display:inline-block;width:0;height:0;vertical-align:baseline;padding:0;border:0;margin:0'; return m; };
  const base = el => { if (!el) return null; const tn = [...el.childNodes].find(n => n.nodeType === 3 && n.data.trim()); if (!tn) return null;
    const w = document.createElement('rts-mark'); tn.replaceWith(w); w.appendChild(tn); const m = mark(); w.appendChild(m);
    const y = (m.getBoundingClientRect().top - st.top) / k; m.remove(); w.replaceWith(tn); return +y.toFixed(2); };
  // as linhas de um texto que quebra: a caixa de cada uma
  const lines = el => { const tn = [...el.childNodes].find(n => n.nodeType === 3 && n.data.trim()); if (!tn) return null; const rg = document.createRange(); rg.selectNodeContents(tn);
    return [...rg.getClientRects()].map(R); };
  const sty = el => { if (!el) return null; const c = getComputedStyle(el); return { font: `${c.fontWeight} ${c.fontSize} ${c.fontFamily.split(',')[0].replace(/"/g, '')}`, ls: c.letterSpacing, color: c.color,
    bg: c.backgroundColor, border: c.borderTopWidth + ' ' + c.borderTopStyle + ' ' + c.borderTopColor, op: c.opacity }; };
  const T = el => el ? { ...box(el), base: base(el), text: el.textContent.trim().slice(0, 60), ...sty(el) } : null;
  const $ = s => document.querySelector(s), $$ = s => [...document.querySelectorAll(s)];
  const res = {};
  res.back = T($('.backbtn span')); res.kicker = T($('.kicker span')); res.h1 = T($('h1'));
  res.src = $$('.srcb').map(b => ({ ...box(b), ...sty(b), b: T(b.querySelector('b')), span: T(b.querySelector('span')), n: T(b.querySelector('.n')) }));
  res.host = T($('#host'));
  res.me = { dot: box($('.me .dot')), b: T($('.me b')), all: T($('.me')) };
  res.search = { ...box($('.search')), svg: box($('.search svg')), input: box($('#q')), ...sty($('#q')) };
  res.chips = $$('.chip').map(T);
  res.live = { svg: box($('.live svg')), span: T($('#upd')) };
  res.th = $$('#th div').map(d => ({ ...T(d), i: box(d.querySelector('i')) }));
  res.rows = $$('#rows .rm').map(r => ({ ...box(r), ...sty(r), cls: r.className,
    nm: T(r.querySelector('.nm b')), sub: T(r.querySelector('.nm span')), badges: [...r.querySelectorAll('.badge')].map(T),
    mpc: box(r.querySelector('.mp canvas')), mp: T(r.querySelector('.mp')),
    pips: [...r.querySelectorAll('.pips i')].map(box), pl: T(r.querySelector('.pl small')),
    mode: T(r.children[3]), ping: [...r.querySelectorAll('.ping i')].map(i => ({ ...box(i), bg: getComputedStyle(i).backgroundColor })), pg: T(r.querySelector('.pg small')) }));
  const det = $('#det');
  res.det = { pv: box(det.querySelector('canvas')), h3: T(det.querySelector('h3')), meta: { ...T(det.querySelector('.meta')), lines: lines(det.querySelector('.meta')) },
    who: [...det.querySelectorAll('.p')].map(p => ({ ...box(p), ...sty(p), cls: p.className, tok: box(p.querySelector('.tok')), img: box(p.querySelector('.tok img')), b: T(p.querySelector('b')),
      crown: box(p.querySelector('.crown')), span: T(p.querySelector('span')) })), join: T(det.querySelector('.join')) };
  res.corners = $$('.corner').map(box);
  return JSON.stringify(res);
})()
