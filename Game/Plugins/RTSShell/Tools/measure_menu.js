// Mede no Chrome o menu aprovado (docs/concept/ui/menu/menu.html): onde cai a linha de base de cada texto, as quebras de linha, as
// caixas da lista e do bloco de desenvolvimento, e as métricas das fontes (altura da linha "normal" e a base a partir do topo).
// O menu nativo põe cada texto com a base no mesmo pixel. Rodado pelo cap.mjs eval; devolve JSON.
(() => {
  const st = document.getElementById('stage').getBoundingClientRect(), k = st.width / 1920;
  const L = r => ({ x: (r.left - st.left) / k, y: (r.top - st.top) / k, w: r.width / k, h: r.height / k });
  // a base de um texto: um marcador de altura zero alinhado na base. (04/11) O marcador é um elemento sem CSS nenhum (o <span> pegava
  // o '.devh span' do mockup, com borda e respiro, e saía 6 px acima); numa caixa flex, o marcador entra junto do texto, senão ele
  // vira um item da caixa (centralizado) e não fica na linha do texto
  const mark = () => { const m = document.createElement('rts-mark'); m.style.cssText = 'display:inline-block;width:0;height:0;vertical-align:baseline;padding:0;border:0;margin:0'; return m; };
  const base = el => { const m = mark();
    if (/flex|grid/.test(getComputedStyle(el).display)) { const tn = [...el.childNodes].find(n => n.nodeType === 3 && n.data.trim()); const w = document.createElement('rts-mark'); tn.replaceWith(w); w.appendChild(tn); w.appendChild(m);
      const y = (m.getBoundingClientRect().top - st.top) / k; m.remove(); w.replaceWith(tn); return y; }
    el.appendChild(m); const y = (m.getBoundingClientRect().top - st.top) / k; m.remove(); return y; };
  // as linhas de um bloco de texto (as quebras do navegador)
  const lines = el => { const out = []; const tn = [...el.childNodes].filter(n => n.nodeType === 3 || n.nodeName === 'BR');
    let cur = null;
    for (const n of tn) { if (n.nodeName === 'BR') { cur = null; continue; }
      for (let i = 0; i < n.length; i++) { const r = document.createRange(); r.setStart(n, i); r.setEnd(n, i + 1); const b = r.getBoundingClientRect(); if (!b.width && n.data[i] !== ' ') continue;
        const top = Math.round((b.top - st.top) / k);
        if (!cur || Math.abs(cur.top - top) > 3) { cur = { top, x: (b.left - st.left) / k, text: '' }; out.push(cur); } cur.text += n.data[i]; } }
    return out.map(o => ({ ...o, text: o.text.trim() })); };
  const q = s => document.querySelector(s);
  const res = {};
  const side = q('.side');
  res.kicker = { ...L(q('.kicker').getBoundingClientRect()), base: base(q('.kicker')) };
  res.big = { ...L(q('.big').getBoundingClientRect()), lines: lines(q('.big')) };
  // a base de cada linha do título: marcador depois do <br> e no fim
  { const b = q('.big'), br = b.querySelector('br'); const m1 = mark(); b.insertBefore(m1, br);
    res.big.base1 = (m1.getBoundingClientRect().top - st.top) / k; m1.remove(); res.big.base2 = base(b); }
  res.sub = { ...L(q('.sub').getBoundingClientRect()), lines: lines(q('.sub')) };
  { const s = q('.sub'), ln = res.sub.lines; const r = document.createRange(); const tn = s.firstChild;
    // a base da 1ª e da 2ª linha: marcadores nos pontos de quebra
    const m = mark();
    tn.splitText(ln[0].text.length + 1); s.insertBefore(m, s.childNodes[1]); res.sub.base1 = (m.getBoundingClientRect().top - st.top) / k; m.remove(); s.normalize(); res.sub.base2 = base(s); }
  res.items = [...document.querySelectorAll('#menu li')].map(li => { const t = li.querySelector('.t'), d = li.querySelector('.d');
    return { k: li.dataset.k, li: L(li.getBoundingClientRect()), t: { ...L(t.getBoundingClientRect()), base: base(t), text: t.textContent }, d: { text: d.textContent } }; });
  const dev = q('#dev'); const was = dev.classList.contains('on'); dev.classList.add('on');
  res.devh = { ...L(q('.devh').getBoundingClientRect()), base: base(q('.devh')), tag: L(q('.devh span').getBoundingClientRect()), tagBase: base(q('.devh span')),
    textW: (() => { const r = document.createRange(); r.selectNodeContents(q('.devh').firstChild); return r.getBoundingClientRect().width / k; })() };
  res.tiles = [...document.querySelectorAll('.dv')].map(dv => ({ k: dv.dataset.k, box: L(dv.getBoundingClientRect()), b: { ...L(dv.querySelector('b').getBoundingClientRect()), base: base(dv.querySelector('b')) },
    span: { ...L(dv.querySelector('span').getBoundingClientRect()), base: base(dv.querySelector('span')) }, em: dv.querySelector('em') ? { ...L(dv.querySelector('em').getBoundingClientRect()), base: base(dv.querySelector('em')) } : null }));
  if (!was) dev.classList.remove('on');
  res.ver = { ...L(q('.ver').getBoundingClientRect()), base: base(q('.ver')) };
  // (03/11) o hover vem depois do bloco do desenvolvimento: medido antes, o item ainda fechava (a transição da altura) e o bloco
  // saía 30 px abaixo
  // o mesmo item com o hover ligado (a descrição aberta): a base da descrição e a altura nova
  { const li = document.querySelector('#menu li'); li.classList.add('hv'); li.querySelectorAll('*').forEach(e => e.style.transition = 'none');
    const t = li.querySelector('.t'), d = li.querySelector('.d'); li.offsetHeight;
    res.hover = { li: L(li.getBoundingClientRect()), t: { ...L(t.getBoundingClientRect()), base: base(t) }, d: { ...L(d.getBoundingClientRect()), base: base(d) } };
    li.classList.remove('hv'); li.querySelectorAll('*').forEach(e => e.style.transition = ''); }
  // o número do item (o ::after): mede uma cópia com o mesmo estilo
  { const li = document.querySelector('#menu li'), m = document.createElement('div'); m.style.cssText = "position:absolute;left:0;top:20px;font:600 16px 'Barlow Semi Condensed';letter-spacing:2px"; m.textContent = '01'; li.appendChild(m);
    res.num = { ...L(m.getBoundingClientRect()), base: base(m), dy: (m.getBoundingClientRect().top - li.getBoundingClientRect().top) / k }; m.remove(); }
  // as métricas das fontes usadas no canvas (fillText põe a base no y): altura e base de cada uma
  res.fonts = {};
  for (const f of ["500 11px 'IBM Plex Mono'", "500 12px 'IBM Plex Mono'", "600 10px 'IBM Plex Mono'", "500 10px 'IBM Plex Mono'", "500 10.5px 'IBM Plex Mono'", "500 22px 'IBM Plex Mono'", "600 9.5px 'IBM Plex Mono'"]) {
    const c = document.createElement('canvas').getContext('2d'); c.font = f; const mt = c.measureText('ANG 0123456789');
    res.fonts[f] = { asc: mt.fontBoundingBoxAscent, desc: mt.fontBoundingBoxDescent, w: mt.width }; }
  return JSON.stringify(res);
})()
