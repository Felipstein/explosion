// Tela do Atelier: o mesmo desenho do mockup, agora ligado ao serviço (atelier.mjs).
// O estado vem de /api/requests e se atualiza pelo /api/events; toda ação é um POST.
(() => {
const $ = s => document.querySelector(s);
const esc = s => String(s ?? '').replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const usd = v => 'US$ ' + v.toFixed(2).replace('.', ',');
const mmss = s => Math.floor(s / 60) + ':' + String(Math.floor(s % 60)).padStart(2, '0');
const now = () => performance.now();
const SRC = f => '/file?p=' + encodeURIComponent(f);

const ICONS = {
  star:'<path d="M12 3.6l2.6 5.3 5.8.9-4.2 4.1 1 5.8L12 17l-5.2 2.7 1-5.8-4.2-4.1 5.8-.9z"/>',
  x:'<path d="M6.5 6.5l11 11M17.5 6.5l-11 11"/>',
  branch:'<circle cx="6.5" cy="5.5" r="2"/><circle cx="6.5" cy="18.5" r="2"/><circle cx="17.5" cy="8" r="2"/><path d="M6.5 7.5v9M17.5 10c0 4.5-7 3.5-10.2 7"/>',
  clip:'<path d="M19.5 11.5l-7.6 7.6a4.8 4.8 0 01-6.8-6.8l8.2-8.2a3.2 3.2 0 014.5 4.5l-8.2 8.2a1.6 1.6 0 01-2.3-2.3l7.5-7.5"/>',
  expand:'<path d="M4.5 9V4.5H9M19.5 9V4.5H15M4.5 15v4.5H9M19.5 15v4.5H15"/>',
  refresh:'<path d="M19.5 12a7.5 7.5 0 11-2.2-5.3M19.5 4.5v4.5H15"/>',
  plus:'<path d="M12 5v14M5 12h14"/>',
  search:'<circle cx="11" cy="11" r="6.5"/><path d="M20 20l-4.2-4.2"/>',
  folder:'<path d="M3.5 6.5h6l2 2h9v10h-17z"/>',
  send:'<path d="M5 12h13M13 6.5l5.5 5.5-5.5 5.5"/>',
  anchor:'<circle cx="12" cy="5" r="2"/><path d="M12 7v13M5 13a7 7 0 0014 0M8.5 10h7"/>',
  compare:'<rect x="3.5" y="5" width="7.5" height="14" rx="1.2"/><rect x="13" y="5" width="7.5" height="14" rx="1.2"/>',
  chat:'<path d="M4.5 5.5h15v10h-9l-4 3.5v-3.5h-2z"/>',
  spark:'<path d="M12 3.5v17M3.5 12h17M6 6l12 12M18 6L6 18"/>',
  check:'<path d="M5 12.5l4.5 4.5L19 7.5"/>',
  left:'<path d="M14.5 6l-6 6 6 6"/>', right:'<path d="M9.5 6l6 6-6 6"/>',
  down:'<path d="M6 9.5l6 6 6-6"/>',
  copy:'<rect x="8.5" y="8.5" width="11" height="11" rx="1.6"/><path d="M15.5 8.5v-3A1.5 1.5 0 0014 4H5.5A1.5 1.5 0 004 5.5V14a1.5 1.5 0 001.5 1.5h3"/>',
  sliders:'<path d="M4 7h9M17 7h3M4 17h3M11 17h9"/><circle cx="15" cy="7" r="2"/><circle cx="9" cy="17" r="2"/>'
};
const ic = n => `<svg class="i" viewBox="0 0 24 24" aria-hidden="true">${ICONS[n]}</svg>`;

// tempo típico de cada modelo: só pra barra de progresso (as APIs não dizem quanto falta)
const M = {
  gpt:   {name:'GPT Image', short:'GPT', cls:'m-gpt', typ:50, key:'gpt'},
  pro:   {name:'Gemini 3 Pro Image', short:'Gemini Pro', cls:'m-pro', typ:25, key:'gemini_pro'},
  flash: {name:'Gemini 3.1 Flash Image', short:'Gemini Flash', cls:'m-flash', typ:10, key:'gemini_flash'},
  fake:  {name:'Teste (não gera de verdade)', short:'Teste', cls:'m-flash', typ:3, key:'fake'},
};
let HEALTH = {models:{}, repo:''};
let R = [];
const S = { sel:null, selK:null, claude:false, sess:null, cmp:null, lpT:false, ov:null, rail:false, openPrompt:{}, showDiff:{} };

/* ---------- serviço ---------- */
async function api(route, body) {
  const r = await fetch(route, body ? {method:'POST', headers:{'content-type':'application/json'}, body:JSON.stringify(body)} : {});
  const j = await r.json().catch(() => ({}));
  if (!r.ok) { toast(j.error || 'O Atelier não respondeu'); throw new Error(j.error); }
  return j;
}
let loading = null, again = false;
async function load() {
  if (loading) { again = true; return loading; }
  loading = (async () => {
    const old = new Map(R.map(r => [r.id, r]));
    const list = await api('/api/requests');
    const t = now();
    for (const r of list) for (const x of allOf(r)) if (x.it.st === 'g') x.it._t = t - (x.it.elapsed || 0) * 1000;
    R = list;
    notifyChanges(old);
    if (!S.sel || !req(S.sel)) S.sel = (R.find(r => r.status !== 'closed') || R[0] || {}).id || null;
    render();
  })().finally(() => { loading = null; if (again) { again = false; load(); } });
  return loading;
}
function notifyChanges(old) {
  if (!old.size) return;
  for (const r of R) {
    const o = old.get(r.id);
    if (!o) { if (r.src !== 'Você') toast(`A sessão ${r.src} pediu: ${r.title}`, {label:'Ver', fn:() => select(r.id)}, r.id); continue; }
    if (r.id === S.sel) continue;
    const was = new Set(allOf(o).filter(x => x.it.st === 'd').map(x => x.k));
    const fresh = allOf(r).filter(x => x.it.st === 'd' && !was.has(x.k));
    if (fresh.length) {
      const unseen = allOf(r).filter(x => x.it.st === 'd' && !x.it.seen).length;
      toast(`${r.title}: ${unseen} nova${unseen > 1 ? 's' : ''}`, {label:'Ver', fn:() => select(r.id, fresh[0].k)}, r.id);
    }
  }
}
function connect() {
  const es = new EventSource('/api/events');
  let t = null;
  es.onmessage = () => { clearTimeout(t); t = setTimeout(load, 120); };
  // serviço reiniciado com código novo: a tela se recarrega sozinha
  es.onopen = () => { $('#conn').textContent = ''; api('/api/health').then(h => { if (HEALTH.boot && h.boot !== HEALTH.boot) location.reload(); HEALTH = h; renderMeters(); }); load(); };
  es.onerror = () => { $('#conn').textContent = 'reconectando ao serviço…'; };
}

/* ---------- consultas ---------- */
const req = id => R.find(r => r.id === id);
const cur = () => req(S.sel);
function allOf(r) {
  const out = [];
  if (r.kind === 'batch') r.items.forEach((row, ri) => row.cells.forEach((it, ci) => out.push({it, k:`${r.id}:b:${ri}:${ci}`, r, row, ri})));
  else r.ramos.forEach(rm => rm.rounds.forEach((rd, n) => rd.items.forEach((it, i) => out.push({it, k:`${r.id}:${rm.id}:${n}:${i}`, r, rm, n}))));
  return out;
}
const everything = () => R.flatMap(allOf);
const byKey = k => { const r = req(k.split(':')[0]); return r && allOf(r).find(x => x.k === k); };
const diff = (a, b) => ({del: a.filter(l => !b.includes(l)), add: b.filter(l => !a.includes(l))});
function qpos(it) {
  const q = everything().filter(x => x.it.m === it.m && x.it.st === 'q').sort((a, b) => a.it.seq - b.it.seq);
  return q.findIndex(x => x.it === it) + 1;
}
const rootAtt = r => r.att.find(a => a.root) || r.att[0];
function ago(iso) {
  const s = (Date.now() - new Date(iso)) / 1000;
  if (s < 90) return 'agora';
  if (s < 3600) return `há ${Math.round(s / 60)} min`;
  if (s < 86400) return `há ${Math.round(s / 3600)} h`;
  return new Date(iso).toLocaleDateString('pt-BR', {day:'2-digit', month:'2-digit'});
}

/* ---------- desenho ---------- */
function render() {
  const d = $('#desk'), st = d.scrollTop;
  renderMeters(); renderRail(); renderDesk(); renderClaude();
  d.scrollTop = st;
  $('#body').classList.toggle('no-claude', !S.claude);
  $('#body').classList.toggle('show-rail', S.rail);
  $('#btn-claude').classList.toggle('on', S.claude);
  progress();
}

function renderMeters() {
  const all = everything();
  $('#meters').innerHTML = ['gpt', 'pro', 'flash'].map(k => {
    const m = M[k], ready = (HEALTH.models[k] || {}).ready !== false;
    const g = all.filter(x => x.it.m === k && x.it.st === 'g').length;
    const q = all.filter(x => x.it.m === k && x.it.st === 'q').length;
    const viaApi = k === 'gpt' && HEALTH.gptVia === 'api';
    const txt = !ready ? (k === 'gpt' && !viaApi ? 'sem login' : 'sem chave') : g ? g + ' gerando' : 'livre';
    const tip = `${m.name}${k === 'gpt' ? (viaApi ? ' · pela chave da API' : ' · pela sua conta do ChatGPT') : ''}`;
    return `<button class="meter ${m.cls} ${g ? 'on' : ''}" data-act="settings" title="${tip}"><i class="dot ${!ready ? 'q' : g ? 'g' : 'd'}"></i><b>${m.short}</b><span>${txt}${q ? ' · ' + q + ' na fila' : ''}</span></button>`;
  }).join('');
}

function dotsOf(r) {
  const cls = it => it.pick ? 'p' : it.st;
  if (r.kind === 'batch') return r.slots.map((m, ci) => {
    const cells = r.items.map(row => row.cells[ci]);
    const st = cells.some(c => c.st === 'g') ? 'g' : cells.some(c => c.st === 'q') ? 'q' : cells.some(c => c.st === 'f') ? 'f' : 'd';
    return `<i class="dot ${st} ${M[m].cls}"></i>`;
  }).join('');
  return r.ramos.map(rm => (rm.rounds[rm.rounds.length - 1]?.items || []).map(it => `<i class="dot ${cls(it)} ${M[it.m].cls}"></i>`).join('')).join('');
}

function rowHTML(r) {
  const done = allOf(r).filter(x => x.it.st === 'd');
  const pickd = done.filter(x => x.it.pick);
  const show = (pickd.length ? pickd : done).slice(r.kind === 'batch' ? 0 : -2).slice(0, r.kind === 'batch' ? 4 : 2);
  const unseen = done.filter(x => !x.it.seen).length;
  const mos = show.length ? show.map(x => `<img src="${SRC(x.it.file)}" alt="" loading="lazy">`).join('') : '<i class="shim"></i>';
  return `<button class="req ${S.sel === r.id ? 'on' : ''}" data-act="sel" data-id="${r.id}">
    <span class="mosaic ${r.kind === 'batch' ? 'b4' : ''}">${mos}</span>
    <span class="req-t"><b>${esc(r.title)}</b><span>${ic('chat')}${esc(r.src)} · ${ago(r.created)}</span></span>
    <span class="req-s"><span class="dots">${dotsOf(r)}</span>${unseen ? `<span class="unseen">${unseen}</span>` : ''}</span>
  </button>`;
}

function renderRail() {
  const g = {wait:[], gen:[], closed:[]};
  R.forEach(r => { if (!S.sess || r.src === S.sess) g[r.status].push(r); });
  const names = {wait:'Esperando você', gen:'Gerando', closed:'Fechados'};
  $('#rail').innerHTML = R.length ? ['wait', 'gen', 'closed'].filter(k => g[k].length).map(k =>
    `<div class="grp"><div class="grp-h"><span>${names[k]}</span><span>${g[k].length}</span></div>${g[k].map(rowHTML).join('')}</div>`).join('')
    : `<div class="grp"><div class="grp-h"><span>Nenhum pedido ainda</span></div></div>`;
  const today = new Date().toDateString();
  const spent = everything().filter(x => x.it.st === 'd' && new Date(x.r.created).toDateString() === today).reduce((s, x) => s + (x.it.cost || 0), 0);
  const sess = [...new Set(R.map(r => r.src).filter(s => s && s !== 'Você'))];
  $('#sessions').innerHTML = (sess.length ? `<div class="sess-h">Sessões</div>` + sess.map(n => {
    const open = R.filter(r => r.src === n && r.status !== 'closed').length;
    return `<button class="sess ${S.sess === n ? 'on' : ''}" data-act="sess" data-v="${esc(n)}"><i class="dot ${open ? 'live' : 'q'}"></i><span>${esc(n)}</span><span class="mono dim">${open || ''}</span></button>`;
  }).join('') : '') + `<div class="budget" title="Gasto em API hoje; o GPT pela conta do ChatGPT não entra"><div class="b-row"><span>Hoje</span><span class="mono">${usd(spent)}</span></div></div>`;
}

function tileHTML(x, o = {}) {
  const {it, k} = x, m = M[it.m] || M.fake;
  const c = ['tile', 'st-' + it.st, m.cls];
  if (S.selK === k) c.push('sel');
  if (it.pick) c.push('pick');
  if (it.rej) c.push('rej');
  if (it.st === 'd' && !it.seen) c.push('new');
  if (o.small) c.push('small');
  let inner = '';
  if (it.st === 'd') inner = `<img src="${SRC(it.file)}" alt="" loading="lazy">`;
  else if (it.st === 'g') inner = `<i class="shim"></i><span class="clock mono">0:00</span><i class="prog"></i>`;
  else if (it.st === 'q') inner = o.small ? '' : `<span class="qpos mono">${qpos(it)}º na fila</span>`;
  else inner = `<span class="err">${esc(it.err)}</span><button class="mini" data-act="retry" data-k="${k}">${ic('refresh')}Tentar de novo</button>`;
  const lab = o.small ? '' : `<span class="lab"><span class="chip ov ${m.cls}">${o.mini ? m.short.replace('Gemini ', '') : m.short}</span>${o.r ? `<span class="rtag mono">r${o.r}</span>` : ''}</span>`;
  const b = (a, i, t, on) => `<button data-act="${a}" data-k="${k}" title="${t}" class="${on ? 'on' : ''}">${ic(i)}</button>`;
  let acts = '';
  if (it.st === 'd' && !o.small) acts = o.mini
    ? `<span class="acts">${b('pick','star','Escolher (P)', it.pick)}${b('copy','copy','Copiar a imagem (Ctrl C)')}${b('regen','refresh','Gerar de novo (R)')}</span>`
    : `<span class="acts">${b('pick','star','Escolher (P)', it.pick)}${b('rej','x','Reprovar (X)', it.rej)}${b('branch','branch','Ramificar daqui (B)')}${b('attach','clip','Usar como anexo (A)')}${b('copy','copy','Copiar a imagem (Ctrl C)')}</span>`;
  const foot = it.st === 'd' && !o.small && !o.mini ? `<span class="foot mono"><span>${Math.round(it.took)} s</span><span>${it.cost ? usd(it.cost) : 'conta'}</span></span>` : '';
  const star = it.pick ? `<span class="star">${ic('star')}</span>` : '';
  const tip = o.small ? ` title="${m.short} · r${o.r}"` : '';
  return `<figure class="${c.join(' ')}" data-k="${k}"${tip}>${inner}${lab}${star}${acts}${foot}</figure>`;
}

function ramoHTML(r, rm) {
  const n = rm.rounds.length, last = rm.rounds[n - 1];
  const d = diff(r.root, rm.prompt || r.root);
  const has = d.add.length || d.del.length;
  const pre = `${r.id}:${rm.id}`;
  const hist = rm.rounds.slice(0, -1).map((rd, i) => ({rd, i})).reverse();
  const head = `<div class="ramo-h"><h3>${esc(rm.name)}</h3><span class="slots">${rm.slots.map(m => `<span class="chip ${M[m].cls}">${M[m].short}</span>`).join('')}</span>
    ${has ? `<button class="pdiff" data-act="diff" data-ramo="${rm.id}" title="O que muda em relação ao prompt da raiz"><span class="a">+${d.add.length}</span><span class="dl">−${d.del.length}</span><span>no prompt</span></button>` : `<span class="praw">prompt da raiz</span>`}
    <span class="sp"></span>
    <button class="ghost sm" data-act="round" data-ramo="${rm.id}" title="Nova rodada com o prompt deste ramo">${ic('refresh')}r${n + 1}</button></div>`;
  const db = has && S.showDiff[pre] ? `<div class="diffbox">${d.del.map(l => `<div class="d-del">${esc(l)}</div>`).join('')}${d.add.map(l => `<div class="d-add">${esc(l)}</div>`).join('')}</div>` : '';
  const latest = last ? `<div class="latest" style="--n:${last.items.length}">${last.items.map((it, i) => tileHTML({it, k:`${pre}:${n - 1}:${i}`}, {r:n})).join('')}</div>` : '';
  const histH = hist.length ? `<div class="hist">${hist.map(({rd, i}) => `<div class="h-row"><span class="mono">r${i + 1}</span><div class="h-tiles" style="--n:${rd.items.length}">${rd.items.map((it, j) => tileHTML({it, k:`${pre}:${i}:${j}`}, {small:true, r:i + 1})).join('')}</div></div>`).join('')}</div>` : '';
  return `<section class="ramo">${head}${db}<div class="ramo-b ${hist.length ? '' : 'solo'}">${latest}${histH}</div></section>`;
}

function renderDesk() {
  const r = cur();
  if (!r) { $('#desk').innerHTML = `<div class="d-head"><div class="d-title"><h1>Nenhum pedido aberto</h1></div><div class="d-acts"><button class="primary sm" data-act="new">${ic('plus')}Novo pedido <kbd>N</kbd></button></div></div>`; return; }
  const all = allOf(r), picks = all.filter(x => x.it.pick && x.it.st === 'd').length;
  const spent = all.filter(x => x.it.st === 'd').reduce((s, x) => s + (x.it.cost || 0), 0);
  let h = `<div class="d-head">
    <div class="d-title"><h1>${esc(r.title)}</h1>${r.closed ? `<span class="pill ok">${ic('check')}${r.approved ? 'Aprovada' : 'Fechado'}</span>` : ''}</div>
    <div class="d-acts">
      ${picks && !r.closed ? `<button class="primary sm" data-act="approve">${ic('check')}Aprovar ${picks > 1 ? picks + ' escolhidas' : 'a escolhida'}</button>` : ''}
      <button class="ghost sm" data-act="compare">${ic('compare')}Comparar <kbd>C</kbd></button>
    </div>
    <div class="d-meta"><span class="src">${ic('chat')}${esc(r.src)}</span>
      <button class="path" data-act="open-folder" title="Abrir a pasta no Explorer">${ic('folder')}${esc(r.folder)}</button>
      ${spent ? `<span class="mono">${usd(spent)}</span>` : ''}${r.approved ? `<span class="dim">→ ${esc(r.approved.files.join(', '))}</span>` : ''}</div>
  </div>
  <section class="prompt ${S.openPrompt[r.id] ? 'open' : ''}" id="prompt-card">
    <div class="p-text" data-act="expand" title="${S.openPrompt[r.id] ? 'Recolher' : 'Ver o prompt inteiro'}">${esc(r.root.join(' '))}</div>
    <div class="atts">${r.att.map((a, i) => `<figure class="att ${a.root ? 'root' : ''} ${a.derived ? 'derived' : ''}" title="${a.root ? 'Referência: a imagem aprovada que o resultado tem de seguir' : a.derived ? 'Saiu de uma rodada. Anexar derivada encadeia: a próxima herda o desvio dela.' : ''}"><div class="att-img" data-act="view-att" data-i="${i}"><img src="${SRC(a.file)}" alt=""><span class="n">${i + 1}</span>${a.root ? `<span class="anc">${ic('anchor')}</span>` : ''}${r.closed ? '' : `<button class="rm" data-act="detach" data-i="${i}" title="Tirar o anexo (as próximas rodadas não usam mais)">${ic('x')}</button>`}</div><figcaption>${esc(a.label)}</figcaption></figure>`).join('')}
      <button class="att add" data-act="attach-file" title="Anexar imagem: escolher, arrastar ou colar (Ctrl V)">${ic('plus')}</button></div>
  </section>`;
  if (r.kind === 'batch') {
    const tot = all.length, done = all.filter(x => x.it.st === 'd').length;
    h += `<div class="b-head"><span class="slots">${r.slots.map(m => `<span class="chip ${M[m].cls}">${M[m].short}</span>`).join('')}</span><span class="bbar"><i style="width:${done / tot * 100}%"></i></span><span class="mono dim">${done} de ${tot}</span></div>
      <div class="bgrid">${r.items.map((row, ri) => `<div class="bcard"><div class="bname">${esc(row.name)}</div><div class="bpair">${row.cells.map((it, ci) => tileHTML({it, k:`${r.id}:b:${ri}:${ci}`}, {mini:true})).join('')}</div></div>`).join('')}</div>`;
  } else {
    h += r.ramos.map(rm => ramoHTML(r, rm)).join('');
    if (!r.closed) h += `<button class="add-ramo" data-act="new-ramo">${ic('branch')}Ramo novo: outra visão, outro modelo <kbd>B</kbd></button>`;
  }
  $('#desk').innerHTML = h;
}

function selLabel() {
  const x = S.selK && byKey(S.selK);
  if (!x || x.r.id !== S.sel) return '';
  return x.rm ? `${M[x.it.m].short} · ${x.rm.name} · r${x.n + 1}` : `${M[x.it.m].short} · ${x.row.name}`;
}

function sugHTML(r, s, i) {
  const lines = `<div class="dl">${(s.del || []).map(l => `<div class="d-del">${esc(l)}</div>`).join('')}${(s.add || []).map(l => `<div class="d-add">${esc(l)}</div>`).join('')}</div>`;
  if (s.kind === 'branch') {
    const slots = Object.entries(s.models || {gpt:1}).flatMap(([k, n]) => Array(+n || 0).fill(Object.keys(M).find(m => M[m].key === k) || 'gpt'));
    return `<div class="sug"><div class="sug-h">${ic('branch')}Ramo novo · ${esc(s.name)}</div>${lines}
      ${s.done ? `<div class="sug-done">${ic('check')}Ramo criado</div>` : `<div class="sug-acts"><button class="primary sm" data-act="sug" data-i="${i}">Criar e gerar · ${slots.map(m => M[m].short).join(' + ')}</button></div>`}</div>`;
  }
  const rm = r.ramos && r.ramos.find(x => x.name === s.branch);
  const nx = rm ? rm.rounds.length + 1 : 1;
  return `<div class="sug"><div class="sug-h">${ic('refresh')}${esc(s.branch || '')} · ${s.done || 'r' + nx}</div>${lines}
    ${s.done ? `<div class="sug-done">${ic('check')}Enviado</div>` : rm ? `<div class="sug-acts"><button class="primary sm" data-act="sug" data-i="${i}">Gerar r${nx} · ${rm.slots.map(m => M[m].short).join(' + ')}</button>${rm.slots.length > 1 ? `<button class="ghost sm" data-act="sug-one" data-i="${i}">Só no ${M[rm.slots[0]].short}</button>` : ''}</div>` : ''}</div>`;
}

function renderClaude() {
  const r = cur();
  const sl = selLabel();
  $('#cl-head').innerHTML = `<div class="cl-title"><span class="spark">${ic('spark')}</span>Claude</div><div class="cl-ctx">${r ? esc(r.title) : ''}${sl ? ' · ' + esc(sl) : ''}</div>`;
  const th = $('#cl-thread');
  const atBottom = th.scrollHeight - th.scrollTop - th.clientHeight < 40;
  const msgs = r ? r.thread : [];
  th.innerHTML = msgs.map((m, i) => m.who === 'me'
    ? `<div class="msg me">${esc(m.text)}</div>`
    : `<div class="msg">${esc(m.text).replace(/\n/g, '<br>')}${m.sug ? sugHTML(r, m.sug, i) : ''}</div>`).join('') + (r && r.thinking ? '<div class="typing" title="O Claude está olhando as imagens"><i></i><i></i><i></i></div>' : '');
  if (atBottom || S.scrollThread) { th.scrollTop = th.scrollHeight; S.scrollThread = false; }
}

function progress() {
  const t = now(), map = new Map(everything().map(x => [x.k, x.it]));
  document.querySelectorAll('.tile.st-g').forEach(el => {
    const it = map.get(el.dataset.k); if (!it) return;
    const s = (t - (it._t || t)) / 1000, typ = (M[it.m] || M.fake).typ;
    el.style.setProperty('--p', Math.min(.95, s / typ).toFixed(3));
    const c = el.querySelector('.clock'); if (c) c.textContent = mmss(s);
  });
}

/* ---------- ações ---------- */
function select(id, k) {
  S.sel = id; S.rail = false;
  const r = req(id); if (!r) return;
  if (k) S.selK = k;
  else { const ds = allOf(r).filter(x => x.it.st === 'd'); S.selK = ds.length ? ds[ds.length - 1].k : null; }
  S.scrollThread = true;
  render();
  $('#desk').scrollTop = 0;
}
function setSel(k) {
  S.selK = k; const x = byKey(k);
  if (x && x.it.st === 'd' && !x.it.seen) { x.it.seen = true; api('/api/item', {key:k, op:'seen'}); }
  document.querySelectorAll('.tile.sel').forEach(e => e.classList.remove('sel'));
  const el = document.querySelector(`.tile[data-k="${k}"]`);
  if (el) { el.classList.add('sel'); el.classList.remove('new'); }
  renderRail(); renderClaude();
}
const item = (k, op) => api('/api/item', {key:k, op});
function togglePick(k) { const x = k && byKey(k); if (x && x.it.st === 'd') { x.it.pick = !x.it.pick; if (x.it.pick) x.it.rej = false; render(); if (S.ov === 'cmp') renderCompare(); if (S.ov === 'loupe') renderLoupe(); item(k, 'pick'); } }
function toggleRej(k) { const x = k && byKey(k); if (x && x.it.st === 'd') { x.it.rej = !x.it.rej; if (x.it.rej) x.it.pick = false; render(); if (S.ov === 'loupe') renderLoupe(); item(k, 'reject'); } }
function regen(k) { const x = k && byKey(k); if (!x) return; item(k, 'regen'); if (x.rm) toast(`${x.rm.name}: r${x.rm.rounds.length + 1} no ${M[x.it.m].short}`); }
function attachFrom(k) {
  const x = k && byKey(k); if (!x || x.it.st !== 'd') return;
  const label = x.rm ? `${M[x.it.m].short} r${x.n + 1}` : x.row.name;
  api('/api/attach', {id:x.r.id, key:k, label}).then(() => toast(`Anexo ${x.r.att.length + 1}: ${label}`));
}
function askClaude(text) {
  const r = cur(); if (!r || !text.trim()) return;
  if (r.thinking) return toast('O Claude ainda está respondendo');
  r.thread.push({who:'me', text}); r.thinking = true; S.scrollThread = true; renderClaude();
  api('/api/claude', {id:r.id, text, key:S.selK}).catch(() => load());
}

/* ---------- camadas ---------- */
function openOv(kind, html) { S.ov = kind; const o = $('#ov'); o.hidden = false; o.className = kind === 'cmp' || kind === 'loupe' ? '' : 'overlay'; o.innerHTML = html; }
function closeOv() { S.ov = null; S.lpAtt = null; $('#ov').hidden = true; $('#ov').innerHTML = ''; }

function stepper(id, v, off) { return `<div class="step ${v ? '' : 'zero'}" data-step="${id}"><button type="button" data-d="-1" aria-label="menos" ${off ? 'disabled' : ''}>−</button><b>${v}</b><button type="button" data-d="1" aria-label="mais" ${off ? 'disabled' : ''}>+</button></div>`; }
function modelRows(counts) {
  return ['gpt', 'pro', 'flash'].map(k => {
    const m = M[k], h = HEALTH.models[k] || {}, ready = h.ready !== false;
    const price = !ready ? 'falta a chave' : h.cost ? `${usd(h.cost)} · ~${m.typ} s` : `pela conta · ~${m.typ} s`;
    return `<div class="mrow"><span class="mname"><span class="chip ${m.cls}">${m.short}</span><b>${m.name}</b></span><span class="mono dim">${price}</span>${stepper(k, ready ? counts[k] || 0 : 0, !ready)}</div>`;
  }).join('');
}
function wireSteppers(root, counts, onChange) {
  root.querySelectorAll('.step').forEach(st => st.addEventListener('click', e => {
    const b = e.target.closest('button'); if (!b || b.disabled) return;
    const k = st.dataset.step; counts[k] = Math.max(0, Math.min(4, (counts[k] || 0) + +b.dataset.d));
    st.querySelector('b').textContent = counts[k]; st.classList.toggle('zero', !counts[k]); onChange();
  }));
}
const modelsBody = c => Object.fromEntries(Object.entries(c).filter(([, n]) => n).map(([k, n]) => [M[k].key, n]));
const costOf = c => Object.entries(c).reduce((s, [k, n]) => s + ((HEALTH.models[k] || {}).cost || 0) * n, 0);

let newState = null, pickTarget = null;
async function openNew() {
  if (!HEALTH.boot) await refreshHealth();
  const ready = k => (HEALTH.models[k] || {}).ready !== false;
  const st = {counts:{gpt:1, pro:ready('pro') ? 1 : 0, flash:0}, att:[], batch:false};
  openOv('new', `<form class="sheet" id="nf" autocomplete="off">
    <h2>Novo pedido <span class="seg" style="margin-left:auto"><button type="button" class="on" data-mode="one">Um pedido</button><button type="button" data-mode="batch">Lote</button></span></h2>
    <div class="field"><input class="inp" id="nf-title" placeholder="Nome (ex.: Comanche, ficha)"></div>
    <div class="field"><textarea class="ta" id="nf-prompt" placeholder="Prompt completo, em inglês. Uma ideia por linha deixa a diferença entre ramos legível."></textarea></div>
    <div class="field" id="nf-items-f" hidden><span class="flabel">Itens, um por linha: cada um vira uma imagem com o mesmo prompt, trocando {item}</span><textarea class="ta" id="nf-items" style="min-height:80px" placeholder="sell&#10;repair&#10;guard"></textarea></div>
    <div class="field"><div class="drop" id="nf-drop"></div></div>
    <div class="field">${modelRows(st.counts)}</div>
    <div class="field fpick" id="nf-fp">
      <button type="button" class="fp-btn" id="fp-btn" title="Onde as imagens vão ficar">${ic('folder')}<span class="fp-name" id="fp-name"></span><span class="mono fp-path" id="fp-path"></span>${ic('down')}</button>
      <button type="button" class="ghost" id="fp-browse" title="Escolher qualquer pasta no seletor do Windows">Procurar…</button>
      <div class="fp-pop" id="fp-pop" hidden><input class="inp" id="fp-q" placeholder="Buscar pasta" autocomplete="off"><div class="fp-list" id="fp-list"></div></div>
    </div>
    <div class="sheet-acts"><button type="button" class="ghost" data-act="close">Cancelar</button><button type="submit" class="primary" id="nf-go">Gerar</button></div>
  </form>`);
  const f = $('#nf');
  const upd = () => {
    const n = Object.values(st.counts).reduce((a, b) => a + b, 0);
    const items = st.batch ? $('#nf-items').value.split('\n').filter(s => s.trim()).length : 1;
    const tot = n * items, cost = costOf(st.counts) * items;
    $('#nf-go').innerHTML = `Gerar ${tot} ${tot === 1 ? 'imagem' : 'imagens'}${cost ? ' · ' + usd(cost) : ''} <kbd>Ctrl Enter</kbd>`;
    $('#nf-go').disabled = !tot;
  };
  const drawAtt = () => {
    $('#nf-drop').innerHTML = st.att.map((a, i) => `<figure class="att ${i === 0 ? 'root' : ''}"><div class="att-img"><img src="${a.dataUrl}" alt=""><span class="n">${i + 1}</span>${i === 0 ? `<span class="anc">${ic('anchor')}</span>` : ''}<button type="button" class="rm" data-rm="${i}" title="Tirar">${ic('x')}</button></div><figcaption>${esc(a.label)}</figcaption></figure>`).join('') +
      `<span class="hint">${st.att.length ? '' : 'Arraste, cole (Ctrl V) ou '}<button type="button" class="mini" id="nf-pick">${st.att.length ? '+ imagem' : 'escolha imagens'}</button></span>`;
    $('#nf-pick').onclick = () => { pickTarget = st; $('#file-att').click(); };
    $('#nf-drop').querySelectorAll('[data-rm]').forEach(b => b.onclick = () => { st.att.splice(+b.dataset.rm, 1); drawAtt(); });
  };
  st.redraw = drawAtt;
  drawAtt(); wireSteppers(f, st.counts, upd); upd();
  f.querySelectorAll('[data-mode]').forEach(b => b.onclick = () => {
    st.batch = b.dataset.mode === 'batch';
    f.querySelectorAll('[data-mode]').forEach(x => x.classList.toggle('on', x === b));
    $('#nf-items-f').hidden = !st.batch; upd();
  });
  $('#nf-items').addEventListener('input', upd);
  wireFolderPicker(st, f);
  const go = async e => {
    e.preventDefault();
    const body = {title:$('#nf-title').value.trim(), prompt:$('#nf-prompt').value.trim(), session:'Você', attachments:st.att, models:modelsBody(st.counts), folder:st.folder || undefined};
    if (st.batch) body.items = $('#nf-items').value.split('\n').map(s => s.trim()).filter(Boolean);
    if (!body.title || !body.prompt) return toast('Falta o nome ou o prompt');
    $('#nf-go').disabled = true;
    try { const r = await api('/api/requests', body); closeOv(); await load(); select(r.id); } catch { $('#nf-go').disabled = false; }
  };
  f.addEventListener('submit', go);
  f.addEventListener('keydown', e => { if (e.key === 'Enter' && (e.ctrlKey || e.metaKey)) go(e); });
  newState = st;
  setTimeout(() => $('#nf-title').focus(), 30);
}
/* A pasta do pedido: a lista das de captures/concept (a mais nova primeiro, com miniatura e busca),
   "Nova pasta" com o próximo número no topo, e Procurar… pro seletor do Windows. */
const slugOf = s => s.normalize('NFD').replace(/[\u0300-\u036f]/g, '').toLowerCase().replace(/[^a-z0-9]+/g, '_').replace(/^_|_$/g, '') || 'pedido';
function wireFolderPicker(st, form) {
  st.folder = null;
  let folders = [], items = [], idx = 0;
  const pop = $('#fp-pop'), q = $('#fp-q');
  const nextNew = () => {
    const nums = folders.map(f => +(f.name.match(/^(\d+)_/) || [])[1]).filter(Boolean);
    return `captures/concept/${String(Math.max(50, ...nums) + 1).padStart(2, '0')}_${slugOf($('#nf-title').value)}/`;
  };
  const show = () => {
    const p = st.folder || nextNew();
    $('#fp-name').textContent = st.folder ? p.replace(/\/$/, '').split('/').pop() : 'Nova pasta';
    $('#fp-path').textContent = p;
  };
  const draw = () => {
    const t = q.value.toLowerCase();
    items = [{nova:true}].concat(folders.filter(f => f.name.toLowerCase().includes(t)));
    idx = Math.min(idx, items.length - 1);
    $('#fp-list').innerHTML = items.map((f, i) => {
      const chosen = f.nova ? !st.folder : st.folder === f.path;
      const pic = f.nova ? `<span class="ph">${ic('plus')}</span>` : f.thumb ? `<img src="${SRC(f.thumb)}" alt="" loading="lazy">` : `<span class="ph">${ic('folder')}</span>`;
      const sub = f.nova ? esc(nextNew()) : `${f.images} ${f.images === 1 ? 'imagem' : 'imagens'} · ${ago(new Date(f.mtime).toISOString())}`;
      return `<button type="button" class="fp-it ${i === idx ? 'on' : ''}" data-i="${i}">${pic}<span class="fp-t"><b>${esc(f.nova ? 'Nova pasta' : f.name)}</b><span class="m">${sub}</span></span>${chosen ? ic('check') : ''}</button>`;
    }).join('');
    const on = $('#fp-list .fp-it.on'); if (on) on.scrollIntoView({block:'nearest'});
  };
  const choose = i => { const f = items[i]; if (!f) return; st.folder = f.nova ? null : f.path; show(); pop.hidden = true; $('#fp-btn').focus(); };
  api('/api/folders').then(f => { folders = f; show(); if (!pop.hidden) draw(); });
  show();
  $('#fp-btn').onclick = () => { pop.hidden = !pop.hidden; if (!pop.hidden) { q.value = ''; idx = 0; draw(); q.focus(); } };
  $('#fp-list').onclick = e => { const b = e.target.closest('[data-i]'); if (b) choose(+b.dataset.i); };
  q.oninput = () => { idx = 0; draw(); };
  q.onkeydown = e => {
    if (e.key === 'ArrowDown') { idx = Math.min(items.length - 1, idx + 1); draw(); e.preventDefault(); }
    if (e.key === 'ArrowUp') { idx = Math.max(0, idx - 1); draw(); e.preventDefault(); }
    if (e.key === 'Enter' && !e.ctrlKey) { e.preventDefault(); e.stopPropagation(); choose(idx); }
    if (e.key === 'Escape') { e.stopPropagation(); pop.hidden = true; $('#fp-btn').focus(); }
  };
  form.addEventListener('click', e => { if (!e.target.closest('#nf-fp')) pop.hidden = true; });
  $('#nf-title').addEventListener('input', () => { if (!st.folder) show(); });
  $('#fp-browse').onclick = async () => {
    const b = $('#fp-browse'); b.disabled = true; b.textContent = 'Abrindo…';
    try { const r = await api('/api/pick-folder', {start:st.folder || 'captures/concept'}); if (r.path) { st.folder = r.path; show(); } }
    finally { b.disabled = false; b.textContent = 'Procurar…'; }
  };
}

function readFiles(files, target) {
  [...files].filter(f => f.type.startsWith('image/')).forEach(f => {
    const fr = new FileReader();
    fr.onload = () => {
      const label = (f.name || 'colada').replace(/\.\w+$/, '');
      if (target && target.redraw) { target.att.push({dataUrl:fr.result, label}); target.redraw(); }
      else { const r = cur(); if (r) api('/api/attach', {id:r.id, dataUrl:fr.result, label}).then(() => toast(`Anexo ${r.att.length + 1}: ${label}`)); }
    };
    fr.readAsDataURL(f);
  });
}

function openBranch(fromK) {
  const r = cur(); if (!r || r.kind === 'batch') return;
  const x = fromK && byKey(fromK);
  const counts = {gpt:0, pro:0, flash:0};
  if (x) counts[x.it.m] = 1; else counts.gpt = 1;
  const base = x && x.rm ? (x.rm.prompt || r.root) : r.root;
  let mode = 'same';
  openOv('branch', `<form class="sheet" id="bf" autocomplete="off">
    <h2>${ic('branch')}Ramo novo${x ? ` <span class="dim" style="font-weight:400;font-size:12.5px">a partir de ${M[x.it.m].short} · ${esc(x.rm.name)} · r${x.n + 1}</span>` : ''}</h2>
    <div class="field"><input class="inp" id="bf-name" value="Outra visão"></div>
    <div class="field"><span class="seg"><button type="button" data-m="same" class="on">Mesmo prompt</button><button type="button" data-m="edit">Eu escrevo</button><button type="button" data-m="claude">Claude propõe</button></span>
      <textarea class="ta" id="bf-prompt" hidden>${esc(base.join('\n'))}</textarea></div>
    <div class="field" id="bf-models">${modelRows(counts)}</div>
    <div class="sheet-acts"><button type="button" class="ghost" data-act="close">Cancelar</button><button type="submit" class="primary" id="bf-go">Criar e gerar</button></div>
  </form>`);
  const f = $('#bf');
  const upd = () => {
    const n = Object.values(counts).reduce((a, b) => a + b, 0), cost = costOf(counts);
    $('#bf-go').innerHTML = mode === 'claude' ? `Pedir ao Claude <kbd>Ctrl Enter</kbd>` : `Criar e gerar ${n} ${n === 1 ? 'imagem' : 'imagens'}${cost ? ' · ' + usd(cost) : ''} <kbd>Ctrl Enter</kbd>`;
    $('#bf-go').disabled = mode !== 'claude' && !n;
  };
  wireSteppers(f, counts, upd); upd();
  f.querySelectorAll('[data-m]').forEach(b => b.onclick = () => {
    mode = b.dataset.m; f.querySelectorAll('[data-m]').forEach(y => y.classList.toggle('on', y === b));
    $('#bf-prompt').hidden = mode !== 'edit'; $('#bf-models').hidden = mode === 'claude'; upd();
  });
  const go = async e => {
    e.preventDefault();
    const name = $('#bf-name').value.trim() || 'Outra visão';
    if (mode === 'claude') { closeOv(); S.claude = true; render(); return askClaude(`Proponha um ramo novo chamado "${name}" com uma visão bem diferente das que já tem${x ? `, partindo da ${M[x.it.m].short} ${x.rm.name} r${x.n + 1}` : ''}.`); }
    const prompt = mode === 'edit' ? $('#bf-prompt').value : base.join('\n');
    await api('/api/branch', {id:r.id, name, prompt, models:modelsBody(counts)});
    closeOv(); await load();
    requestAnimationFrame(() => { const d = $('#desk'); d.scrollTop = d.scrollHeight; });
  };
  f.addEventListener('submit', go);
  f.addEventListener('keydown', e => { if (e.key === 'Enter' && (e.ctrlKey || e.metaKey)) go(e); });
  setTimeout(() => $('#bf-name').select(), 30);
}

function openApprove() {
  const r = cur(); const picks = allOf(r).filter(x => x.it.pick && x.it.st === 'd'); if (!picks.length) return;
  openOv('approve', `<form class="sheet" id="af">
    <h2>${ic('check')}Aprovar</h2>
    <div class="ap-imgs">${picks.map(x => `<img src="${SRC(x.it.file)}" alt="">`).join('')}</div>
    <div class="field"><label class="pathsel mono">docs/aprovados/<select id="af-area"><option>unidades</option><option>construcoes</option><option selected>cenas</option><option>explosoes</option><option>hud</option><option>chao</option></select>/</label></div>
    <div class="field"><input class="inp" id="af-said" placeholder="O que você achou, nas suas palavras"></div>
    <div class="sheet-acts"><button type="button" class="ghost" data-act="close">Voltar</button><button type="submit" class="primary">Aprovar${r.src !== 'Você' ? ' e mandar pra ' + esc(r.src) : ''} <kbd>Ctrl Enter</kbd></button></div>
  </form>`);
  const go = async e => {
    e.preventDefault();
    await api('/api/approve', {id:r.id, area:$('#af-area').value, said:$('#af-said').value.trim()});
    closeOv(); toast(`${r.title}: aprovada`); load();
  };
  $('#af').addEventListener('submit', go);
  $('#af').addEventListener('keydown', e => { if (e.key === 'Enter' && (e.ctrlKey || e.metaKey)) go(e); });
  setTimeout(() => $('#af-said').focus(), 30);
}

function cmpList(r) {
  const all = allOf(r).filter(x => x.it.st === 'd');
  const picked = all.filter(x => x.it.pick);
  if (picked.length >= 2) return picked;
  if (r.kind === 'batch') return all.slice(0, 8);
  return r.ramos.flatMap(rm => { const n = rm.rounds.length - 1; return all.filter(x => x.rm === rm && x.n === n); });
}
function openCompare() {
  const r = cur(); if (!r) return;
  S.cmp = {list:cmpList(r).map(x => x.k), mode:'grid', s:.5};
  openOv('cmp', ''); renderCompare();
}
function renderCompare() {
  const r = cur(), c = S.cmp;
  const list = c.list.map(byKey).filter(Boolean);
  const tgt = rootAtt(r);
  const cap = x => `<span class="chip ov ${M[x.it.m].cls}">${M[x.it.m].short}</span><span class="lbl">${x.rm ? esc(x.rm.name) + ' · r' + (x.n + 1) : esc(x.row.name)}</span>`;
  let body;
  if (c.mode === 'slide' && list.length >= 2) {
    const [a, b] = list;
    body = `<div class="slide" id="sld" style="--s:${c.s}"><img src="${SRC(a.it.file)}" alt=""><div class="b"><img src="${SRC(b.it.file)}" alt=""></div><i class="bar"></i>
      <span class="sl" style="left:10px">${cap(a)}</span><span class="sl" style="right:10px">${cap(b)}</span></div>`;
  } else {
    const n = list.length + (tgt ? 1 : 0), cols = n <= 4 ? 2 : n <= 6 ? 3 : 4;
    body = `<div class="cmp-grid" style="--c:${cols}">
      ${tgt ? `<figure class="cc target"><img src="${SRC(tgt.file)}" alt=""><figcaption><span class="n">${ic('anchor').replace('class="i"', 'class="i" style="width:12px;height:12px;margin:4px"')}</span><span class="lbl">${esc(tgt.label)}</span></figcaption></figure>` : ''}
      ${list.map((x, i) => `<figure class="cc ${x.it.pick ? 'pick' : ''}" data-act="cmp-pick" data-k="${x.k}" title="Escolher (${i + 1})"><img src="${SRC(x.it.file)}" alt=""><figcaption><span class="n">${i + 1}</span>${cap(x)}${x.it.pick ? `<span class="star" style="position:static;width:20px;height:20px">${ic('star')}</span>` : ''}</figcaption></figure>`).join('')}
    </div>`;
  }
  $('#ov').innerHTML = `<div class="cmp"><div class="cmp-h"><b>Comparar</b><span class="dim">${esc(r.title)}</span>
    <span class="seg"><button data-act="cmp-mode" data-v="grid" class="${c.mode === 'grid' ? 'on' : ''}">Lado a lado</button><button data-act="cmp-mode" data-v="slide" class="${c.mode === 'slide' ? 'on' : ''}" ${list.length < 2 ? 'disabled' : ''}>Deslizar</button></span>
    <span class="sp"></span>${allOf(r).some(x => x.it.pick) && !r.closed ? `<button class="primary sm" data-act="approve">${ic('check')}Aprovar</button>` : ''}<button class="ghost sm" data-act="close">Fechar <kbd>Esc</kbd></button></div>${body}</div>`;
  const sld = $('#sld');
  if (sld) {
    const mv = e => { const b = sld.getBoundingClientRect(); c.s = Math.max(0, Math.min(1, ((e.touches ? e.touches[0].clientX : e.clientX) - b.left) / b.width)); sld.style.setProperty('--s', c.s); };
    sld.addEventListener('mousemove', mv); sld.addEventListener('touchmove', mv, {passive:true});
  }
}

function doneList(r) { return allOf(r).filter(x => x.it.st === 'd'); }
function openLoupe(k) { const x = k && byKey(k); if (!x || x.it.st !== 'd') return; S.selK = k; S.lpAtt = null; openOv('loupe', ''); renderLoupe(); }
function openAtt(i) { const r = cur(); if (!r || !r.att[i]) return; openOv('loupe', ''); S.lpAtt = {id:r.id, i}; renderLoupe(); }
const copyBtn = file => `<button class="ghost sm" data-act="copy" data-f="${esc(file)}" title="Copiar a imagem">${ic('copy')}Copiar <kbd>Ctrl C</kbd></button>`;
function viewFile() {
  if (S.lpAtt) { const r = req(S.lpAtt.id); return r && r.att[S.lpAtt.i] && r.att[S.lpAtt.i].file; }
  const x = S.selK && byKey(S.selK); return x && x.it.st === 'd' ? x.it.file : null;
}
function renderLoupe() {
  if (S.lpAtt) {
    const r = req(S.lpAtt.id), a = r && r.att[S.lpAtt.i]; if (!a) return closeOv();
    $('#ov').innerHTML = `<div class="loupe"><div class="lp-h"><span class="chip">Anexo ${S.lpAtt.i + 1}</span><b>${esc(a.label)}</b>${a.root ? `<span class="pill ok">${ic('anchor')}referência</span>` : ''}<span class="mono dim">${esc(a.file)}</span>
      <span class="sp"></span>
      ${r.att.length > 1 ? `<button class="ghost sm" data-act="lp-prev" title="Anterior (←)">${ic('left')}</button><button class="ghost sm" data-act="lp-next" title="Próximo (→)">${ic('right')}</button>` : ''}
      ${copyBtn(a.file)}<button class="ghost sm" data-act="close">Fechar <kbd>Esc</kbd></button></div>
      <div class="lp-body"><figure><img src="${SRC(a.file)}" alt=""></figure></div></div>`;
    return;
  }
  const x = byKey(S.selK); if (!x) return closeOv();
  const r = x.r, tgt = rootAtt(r), m = M[x.it.m];
  $('#ov').innerHTML = `<div class="loupe"><div class="lp-h"><span class="chip ${m.cls}">${m.short}</span><b>${x.rm ? esc(x.rm.name) + ' · r' + (x.n + 1) : esc(x.row.name)}</b><span class="mono dim">${Math.round(x.it.took)} s · ${esc(x.it.file)}</span>
    <span class="sp"></span>
    <button class="ghost sm" data-act="lp-prev" title="Anterior (←)">${ic('left')}</button><button class="ghost sm" data-act="lp-next" title="Próxima (→)">${ic('right')}</button>
    <button class="ghost sm" data-act="pick" data-k="${x.k}" style="${x.it.pick ? 'color:var(--accent);border-color:var(--accent)' : ''}">${ic('star')}Escolher <kbd>P</kbd></button>
    ${tgt ? `<button class="ghost sm" data-act="lp-t">${ic('anchor')}${S.lpT ? 'Esconder o alvo' : 'Alvo ao lado'} <kbd>T</kbd></button>` : ''}
    ${copyBtn(x.it.file)}<button class="ghost sm" data-act="close">Fechar <kbd>Esc</kbd></button></div>
    <div class="lp-body ${S.lpT && tgt ? 'two' : ''}">${S.lpT && tgt ? `<figure><img src="${SRC(tgt.file)}" alt=""><figcaption><span class="chip ov">${esc(tgt.label)}</span></figcaption></figure>` : ''}<figure><img src="${SRC(x.it.file)}" alt=""></figure></div></div>`;
  if (!x.it.seen) { x.it.seen = true; item(x.k, 'seen'); }
}
function loupeStep(d) {
  if (S.lpAtt) { const r = req(S.lpAtt.id); if (!r || !r.att.length) return; S.lpAtt.i = (S.lpAtt.i + d + r.att.length) % r.att.length; return renderLoupe(); }
  const x = byKey(S.selK); if (!x) return;
  const l = doneList(x.r); const i = l.findIndex(y => y.k === S.selK);
  S.selK = l[(i + d + l.length) % l.length].k; renderLoupe(); render();
}

async function copyImage(file) {
  if (!file) return;
  try {
    const png = fetch(SRC(file)).then(r => r.blob()).then(async b => {
      if (b.type === 'image/png') return b;
      const bmp = await createImageBitmap(b), c = document.createElement('canvas');
      c.width = bmp.width; c.height = bmp.height; c.getContext('2d').drawImage(bmp, 0, 0);
      return new Promise(res => c.toBlob(res, 'image/png'));
    });
    await navigator.clipboard.write([new ClipboardItem({'image/png': png})]);
    toast('Imagem copiada', null, 'copy');
  } catch (e) { toast('Não deu pra copiar: ' + (e.message || e), null, 'copy'); }
}

/* Configurações: por onde o GPT sai e as chaves. A chave digitada vai direto pro serviço; a tela só
   recebe de volta os 4 últimos caracteres. */
async function openSettings() {
  openOv('settings', `<div class="sheet"><h2>${ic('sliders')}Configurações</h2><div class="dim">Lendo…</div></div>`);
  let s;
  try { s = await api('/api/settings'); } catch { return closeOv(); }
  if (S.ov !== 'settings') return;
  let via = s.gptVia;
  const keyRow = (p, ph) => {
    const k = s.keys[p];
    const saved = k.set ? (k.from === 'env' ? `vem da variável ${k.env} (••••${esc(k.tail)})` : `salva: ••••${esc(k.tail)}`) : '';
    return `<div class="cfg-row"><input class="inp" type="text" style="-webkit-text-security:disc" id="k-${p}" autocomplete="off" spellcheck="false" placeholder="${k.set ? saved + ' · cole outra pra trocar' : ph}">
      <button type="button" class="mini" data-test="${p}">Testar</button>${k.set && k.from === 'file' ? `<button type="button" class="mini" data-del="${p}" title="Apagar a chave salva">Apagar</button>` : ''}</div>
      <div class="cfg-msg" id="m-${p}"></div>`;
  };
  $('#ov').innerHTML = `<form class="sheet" id="sf" autocomplete="off">
    <h2>${ic('sliders')}Configurações</h2>
    <section class="cfg">
      <div class="cfg-h"><span class="chip m-gpt">GPT</span><b>GPT Image</b><span class="sp"></span>
        <span class="seg"><button type="button" data-via="codex" class="${via === 'codex' ? 'on' : ''}">Conta do ChatGPT</button><button type="button" data-via="api" class="${via === 'api' ? 'on' : ''}">Chave da API</button></span></div>
      <div data-show="codex" ${via === 'codex' ? '' : 'hidden'}>
        <div class="cfg-row"><span class="cfg-st"><i class="dot ${s.codex.loggedIn ? 'live' : 'q'}"></i>${esc(s.codex.text || '')}</span><span class="dim">pelo Codex deste PC · sem custo por imagem, gasta o limite do plano</span>
          ${s.codex.loggedIn ? '' : `<button type="button" class="mini" data-act-cfg="codex-login">Entrar no Codex</button>`}</div>
      </div>
      <div data-show="api" ${via === 'api' ? '' : 'hidden'}>${keyRow('openai', 'sk-… (platform.openai.com/api-keys)')}<div class="dim">~US$ 0,19 por imagem, cobrado na conta da API</div></div>
    </section>
    <section class="cfg">
      <div class="cfg-h"><span class="chip m-pro">Gemini</span><b>Gemini Pro e Flash</b><span class="sp"></span><a href="https://aistudio.google.com/apikey" target="_blank" rel="noopener">criar chave no AI Studio</a></div>
      ${keyRow('gemini', 'AIza…')}
      <div class="dim">Pro ~US$ 0,13 e Flash ~US$ 0,07 por imagem; o Pro precisa do faturamento ligado no projeto</div>
    </section>
    <section class="cfg">
      <div class="cfg-h"><span class="spark">${ic('spark')}</span><b>Claude</b></div>
      <div class="cfg-st"><i class="dot live"></i>pela sua assinatura, com o Claude Code deste PC</div>
    </section>
    <div class="sheet-acts"><button type="button" class="ghost" data-act="close">Cancelar</button><button type="submit" class="primary">Salvar <kbd>Ctrl Enter</kbd></button></div>
  </form>`;
  const f = $('#sf');
  f.querySelectorAll('[data-via]').forEach(b => b.onclick = () => {
    via = b.dataset.via;
    f.querySelectorAll('[data-via]').forEach(x => x.classList.toggle('on', x === b));
    f.querySelectorAll('[data-show]').forEach(x => x.hidden = x.dataset.show !== via);
  });
  f.querySelectorAll('[data-test]').forEach(b => b.onclick = async () => {
    const p = b.dataset.test, msg = $('#m-' + p);
    msg.className = 'cfg-msg'; msg.textContent = 'Testando…';
    try { const r = await api('/api/settings/test', {provider:p, key:$('#k-' + p).value.trim() || undefined}); msg.className = 'cfg-msg ' + (r.ok ? 'ok' : 'bad'); msg.textContent = r.msg; }
    catch { msg.className = 'cfg-msg bad'; msg.textContent = 'O teste não respondeu.'; }
  });
  f.querySelectorAll('[data-del]').forEach(b => b.onclick = async () => {
    await api('/api/settings', {keys:{[b.dataset.del]: null}}); toast('Chave apagada'); openSettings(); refreshHealth();
  });
  const login = f.querySelector('[data-act-cfg="codex-login"]');
  if (login) login.onclick = () => { api('/api/codex-login', {}); toast('Abri o login do Codex numa janela; termine no navegador e reabra as Configurações'); };
  const save = async e => {
    e.preventDefault();
    await api('/api/settings', {gptVia:via, keys:{openai:$('#k-openai') ? $('#k-openai').value : '', gemini:$('#k-gemini').value}});
    closeOv(); toast('Configurações salvas'); refreshHealth();
  };
  f.addEventListener('submit', save);
  f.addEventListener('keydown', e => { if (e.key === 'Enter' && (e.ctrlKey || e.metaKey)) save(e); });
}
function refreshHealth() { return api('/api/health').then(h => { HEALTH = h; renderMeters(); }); }

function openPalette() {
  openOv('pal', `<div class="pal"><input id="pal-in" placeholder="Pedido, sessão ou ação"><div class="pal-list" id="pal-list"></div></div>`);
  const acts = [
    {t:'Novo pedido', h:'N', fn:() => { closeOv(); openNew(); }},
    {t:'Comparar o pedido aberto', h:'C', fn:() => { closeOv(); openCompare(); }},
    {t:'Ramo novo', h:'B', fn:() => { closeOv(); openBranch(); }},
    {t:'Mostrar ou esconder o Claude', h:'/', fn:() => { closeOv(); S.claude = !S.claude; render(); }},
  ];
  let idx = 0;
  const draw = () => {
    const q = $('#pal-in').value.toLowerCase();
    const items = R.filter(r => (r.title + ' ' + r.src).toLowerCase().includes(q)).map(r => ({t:r.title, h:r.src, fn:() => { closeOv(); select(r.id); }}))
      .concat(acts.filter(a => a.t.toLowerCase().includes(q)));
    idx = Math.min(idx, Math.max(0, items.length - 1));
    $('#pal-list').innerHTML = items.map((it, i) => `<button class="pal-it ${i === idx ? 'on' : ''}" data-i="${i}"><span>${esc(it.t)}</span><span>${esc(it.h)}</span></button>`).join('');
    $('#pal-list').onclick = e => { const b = e.target.closest('[data-i]'); if (b) items[+b.dataset.i].fn(); };
    return items;
  };
  let items = draw();
  $('#pal-in').addEventListener('input', () => { idx = 0; items = draw(); });
  $('#pal-in').addEventListener('keydown', e => {
    if (e.key === 'ArrowDown') { idx = Math.min(items.length - 1, idx + 1); items = draw(); e.preventDefault(); }
    if (e.key === 'ArrowUp') { idx = Math.max(0, idx - 1); items = draw(); e.preventDefault(); }
    if (e.key === 'Enter' && items[idx]) items[idx].fn();
  });
  setTimeout(() => $('#pal-in').focus(), 20);
}

// Um aviso por assunto: o mesmo pedido atualiza o aviso que já está na tela em vez de empilhar.
function toast(text, act, key) {
  let el = key && document.querySelector(`.toast[data-key="${key}"]`);
  if (el) { clearTimeout(el._t1); clearTimeout(el._t2); el.remove(); }
  el = document.createElement('div');
  el.className = 'toast';
  if (key) el.dataset.key = key;
  el.innerHTML = `<span></span>${act ? `<button class="mini">${act.label}</button>` : ''}`;
  el.firstChild.textContent = text;
  if (act) el.querySelector('button').onclick = () => { act.fn(); el.remove(); };
  $('#toasts').append(el);
  el._t1 = setTimeout(() => el.classList.add('out'), 5200);
  el._t2 = setTimeout(() => el.remove(), 5600);
}

/* ---------- eventos ---------- */
document.addEventListener('click', e => {
  const t = e.target.closest('[data-act]');
  if (!t) {
    const tile = e.target.closest('.tile[data-k]');
    if (tile) { setSel(tile.dataset.k); openLoupe(tile.dataset.k); }
    if (e.target.id === 'ov' && S.ov !== 'cmp' && S.ov !== 'loupe') closeOv();
    return;
  }
  const a = t.dataset.act, k = t.dataset.k, r = cur();
  switch (a) {
    case 'sel': select(t.dataset.id); break;
    case 'new': openNew(); break;
    case 'settings': openSettings(); break;
    case 'palette': openPalette(); break;
    case 'claude-toggle': S.claude = !S.claude; render(); break;
    case 'toggle-rail': S.rail = !S.rail; render(); break;
    case 'sess': S.sess = S.sess === t.dataset.v ? null : t.dataset.v; render(); break;
    case 'pick': togglePick(k); break;
    case 'rej': toggleRej(k); break;
    case 'regen': regen(k); break;
    case 'retry': item(k, 'retry'); break;
    case 'branch': setSel(k); openBranch(k); break;
    case 'new-ramo': openBranch(); break;
    case 'attach': attachFrom(k); break;
    case 'loupe': openLoupe(k); break;
    case 'round': api('/api/round', {id:r.id, branch:t.dataset.ramo}); break;
    case 'diff': { const key = `${r.id}:${t.dataset.ramo}`; S.showDiff[key] = !S.showDiff[key]; render(); break; }
    case 'expand': S.openPrompt[r.id] = !S.openPrompt[r.id]; render(); break;
    case 'attach-file': pickTarget = null; $('#file-att').click(); break;
    case 'detach': api('/api/detach', {id:r.id, i:+t.dataset.i}); break;
    case 'open-folder': api('/api/open', {id:r.id}); break;
    case 'compare': openCompare(); break;
    case 'approve': openApprove(); break;
    case 'chip': askClaude(t.textContent); break;
    case 'sug': api('/api/sug', {id:r.id, i:+t.dataset.i}); break;
    case 'sug-one': api('/api/sug', {id:r.id, i:+t.dataset.i, only:true}); break;
    case 'close': closeOv(); break;
    case 'cmp-mode': S.cmp.mode = t.dataset.v; renderCompare(); break;
    case 'cmp-pick': togglePick(k); break;
    case 'lp-prev': loupeStep(-1); break;
    case 'lp-next': loupeStep(1); break;
    case 'lp-t': S.lpT = !S.lpT; renderLoupe(); break;
    case 'view-att': openAtt(+t.dataset.i); break;
    case 'copy': copyImage(t.dataset.f || (k && byKey(k)?.it.file)); break;
  }
});
document.addEventListener('dblclick', e => { const tile = e.target.closest('.tile[data-k]'); if (tile && !e.target.closest('button')) openLoupe(tile.dataset.k); });
document.addEventListener('mouseover', e => {
  const tile = e.target.closest('.tile.new[data-k]'); if (!tile) return;
  const x = byKey(tile.dataset.k); if (x && !x.it.seen) { x.it.seen = true; tile.classList.remove('new'); renderRail(); item(x.k, 'seen'); }
});
$('#file-att').addEventListener('change', e => { readFiles(e.target.files, pickTarget); e.target.value = ''; });
document.addEventListener('paste', e => {
  const files = [...(e.clipboardData?.files || [])].filter(f => f.type.startsWith('image/'));
  if (!files.length) return;
  e.preventDefault(); readFiles(files, S.ov === 'new' ? newState : null);
});
/* Arrastar arquivos pra dentro da janela. Com o pedido novo aberto, soltar em qualquer lugar vira
   anexo dele; sem janela aberta, soltar na mesa anexa ao pedido aberto. Em qualquer outro ponto o
   soltar não faz nada (sem isso o navegador abria a imagem no lugar do app). */
let dragDepth = 0;
const hasFiles = e => [...(e.dataTransfer?.types || [])].includes('Files');
const dropTarget = e => S.ov === 'new' ? 'new' : !S.ov && cur() && e.target.closest('#desk') ? 'desk' : null;
function endDrag() { dragDepth = 0; document.body.classList.remove('dragging'); $('#nf-drop')?.classList.remove('over'); $('#prompt-card')?.classList.remove('dropping'); }
document.addEventListener('dragenter', e => {
  if (!hasFiles(e)) return;
  dragDepth++; document.body.classList.add('dragging');
  if (S.ov === 'new') $('#nf-drop')?.classList.add('over');
});
document.addEventListener('dragleave', e => { if (hasFiles(e) && --dragDepth <= 0) endDrag(); });
document.addEventListener('dragover', e => {
  if (!hasFiles(e)) return;
  e.preventDefault();
  const t = dropTarget(e);
  e.dataTransfer.dropEffect = t ? 'copy' : 'none';
  $('#prompt-card')?.classList.toggle('dropping', t === 'desk');
});
document.addEventListener('drop', e => {
  if (!hasFiles(e)) return;
  e.preventDefault();
  const t = dropTarget(e), files = e.dataTransfer.files;
  endDrag();
  if (t === 'new') readFiles(files, newState);
  else if (t === 'desk') readFiles(files, null);
});

$('#cl-compose').addEventListener('submit', e => { e.preventDefault(); const i = $('#cl-input'); askClaude(i.value); i.value = ''; });
$('#cl-input').addEventListener('keydown', e => { if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); $('#cl-compose').requestSubmit(); } if (e.key === 'Escape') e.target.blur(); });

function navTiles(dir) {
  const tiles = [...document.querySelectorAll('#desk .tile[data-k]')];
  if (!tiles.length) return;
  let i = tiles.findIndex(t => t.dataset.k === S.selK);
  if (i < 0) return setSel(tiles[0].dataset.k);
  if (dir === 'l' || dir === 'r') i = Math.max(0, Math.min(tiles.length - 1, i + (dir === 'r' ? 1 : -1)));
  else {
    const b = tiles[i].getBoundingClientRect(), cx = b.left + b.width / 2;
    const cands = tiles.map((t, j) => ({j, r:t.getBoundingClientRect()})).filter(o => dir === 'd' ? o.r.top > b.bottom - 4 : o.r.bottom < b.top + 4);
    if (!cands.length) return;
    const rowY = dir === 'd' ? Math.min(...cands.map(o => o.r.top)) : Math.max(...cands.map(o => o.r.top));
    const row = cands.filter(o => Math.abs(o.r.top - rowY) < 6);
    i = row.reduce((best, o) => Math.abs(o.r.left + o.r.width / 2 - cx) < Math.abs(best.r.left + best.r.width / 2 - cx) ? o : best).j;
  }
  setSel(tiles[i].dataset.k);
  tiles[i].scrollIntoView({block:'nearest'});
}

document.addEventListener('keydown', e => {
  const typing = /INPUT|TEXTAREA|SELECT/.test(document.activeElement?.tagName || '');
  if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'k') { e.preventDefault(); openPalette(); return; }
  if (e.key === 'Escape') { if (S.ov) { closeOv(); return; } if (S.rail) { S.rail = false; render(); } return; }
  if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'c' && !typing && !String(getSelection())) {
    const f = S.ov === 'loupe' ? viewFile() : !S.ov ? viewFile() : null;
    if (f) { e.preventDefault(); copyImage(f); return; }
  }
  if (S.ov === 'cmp' && !typing) {
    const n = +e.key; const l = S.cmp.list;
    if (n >= 1 && n <= l.length) togglePick(l[n - 1]);
    if (e.key.toLowerCase() === 's') { S.cmp.mode = S.cmp.mode === 'slide' ? 'grid' : 'slide'; renderCompare(); }
    return;
  }
  if (S.ov === 'loupe') {
    const kk = e.key.toLowerCase();
    if (e.key === 'ArrowLeft') loupeStep(-1);
    if (e.key === 'ArrowRight') loupeStep(1);
    if (S.lpAtt) return;
    if (kk === 'p') togglePick(S.selK);
    if (kk === 'x') toggleRej(S.selK);
    if (kk === 't') { S.lpT = !S.lpT; renderLoupe(); }
    return;
  }
  if (S.ov || typing || e.ctrlKey || e.metaKey || e.altKey) return;
  const kk = e.key.toLowerCase();
  const map = {n:openNew, c:openCompare, p:() => togglePick(S.selK), x:() => toggleRej(S.selK), r:() => S.selK && regen(S.selK),
    b:() => openBranch(S.selK && byKey(S.selK)?.it.st === 'd' && byKey(S.selK)?.rm ? S.selK : null), a:() => S.selK && attachFrom(S.selK),
    enter:() => S.selK && openLoupe(S.selK), '/':() => { S.claude = true; render(); $('#cl-input').focus(); },
    arrowleft:() => navTiles('l'), arrowright:() => navTiles('r'), arrowup:() => navTiles('u'), arrowdown:() => navTiles('d')};
  if (map[kk]) { e.preventDefault(); map[kk](); }
});

document.querySelectorAll('[data-ic]').forEach(el => el.insertAdjacentHTML('afterbegin', ic(el.dataset.ic)));
setInterval(progress, 500);
refreshHealth();
// ?snapshot = uma leitura só, sem o canal de avisos (pras capturas de teste, que esperam a rede parar)
const Q = new URLSearchParams(location.search);
if (Q.has('snapshot')) load(); else connect();
if (Q.get('open') === 'settings') openSettings();
if (Q.get('open') === 'new') { openNew(); if (Q.has('folders')) setTimeout(() => $('#fp-btn').click(), 800); }
})();
