// Atelier: o serviço local com a fila de imagens. Um só por máquina (porta 8140); a tela e as
// sessões (pelo MCP, mcp.mjs) falam com ele por HTTP. Cada pedido mora num JSON em
// captures/atelier/requests/ e as imagens vão pra pasta do pedido, com os anexos em _anexos/.
// Sobrevive a reinício: o que estava gerando volta pra fila.
import http from 'node:http';
import fs from 'node:fs';
import { spawn } from 'node:child_process';
import path from 'node:path';
import { PROVIDERS, hasKey } from './providers.mjs';
import { readSettings, writeSettings, publicKeys, key } from './settings.mjs';

export const PORT = +(process.env.ATELIER_PORT || 8140);
const BOOT = Date.now();
const HERE = path.dirname(new URL(import.meta.url).pathname.replace(/^\/(\w:)/, '$1'));
const REPO = path.resolve(HERE, '../..');
const DATA = path.join(REPO, 'captures', 'atelier');
const REQS = path.join(DATA, 'requests');
const UI = path.join(REPO, 'Atelier', 'ui', 'index.html');
fs.mkdirSync(REQS, { recursive: true });

// Modelos que o pedido escolhe. O GPT sai pela conta do ChatGPT (Codex) ou pela chave da API,
// conforme as Configurações; config.json (opcional) só troca nomes de modelo e paralelismo.
const CONFIG = (() => { try { return JSON.parse(fs.readFileSync(path.join(DATA, 'config.json'), 'utf8')); } catch { return {}; } })();
const MODELS = {
  gpt:   { name: 'GPT Image', short: 'GPT', model: CONFIG.gptModel || 'gpt-image-2',
           get provider() { return readSettings().gptVia === 'api' ? 'openai' : 'codex'; },
           get cost() { return readSettings().gptVia === 'api' ? 0.19 : 0; } },
  pro:   { name: 'Gemini 3 Pro Image', short: 'Gemini Pro', provider: 'gemini', model: CONFIG.proModel || 'gemini-3-pro-image-preview', cost: 0.134 },
  flash: { name: 'Gemini 3.1 Flash Image', short: 'Gemini Flash', provider: 'gemini', model: CONFIG.flashModel || 'gemini-3.1-flash-image-preview', cost: 0.067 },
  fake:  { name: 'Teste', short: 'Teste', provider: 'fake', model: 'fake', cost: 0 },
};
const PAR = Object.assign({ codex: 2, gemini: 3, openai: 3, fake: 4 }, CONFIG.parallel || {});

const log = (...a) => fs.appendFileSync(path.join(DATA, 'atelier.log'), new Date().toISOString() + ' ' + a.join(' ') + '\n');

/* ---------- pedidos ---------- */
const R = new Map();
let SEQ = 0;
for (const f of fs.readdirSync(REQS).filter(f => f.endsWith('.json'))) {
  try {
    const r = JSON.parse(fs.readFileSync(path.join(REQS, f), 'utf8'));
    for (const x of items(r)) { if (x.it.st === 'g') x.it.st = 'q'; SEQ = Math.max(SEQ, x.it.seq + 1); }
    r.thinking = false;
    R.set(r.id, r);
  } catch (e) { log('pedido ilegível', f, e.message); }
}

function items(r) {
  const out = [];
  if (r.kind === 'batch') r.items.forEach((row, ri) => row.cells.forEach((it, ci) => out.push({ it, k: `${r.id}:b:${ri}:${ci}`, r, row, ri, ci })));
  else r.ramos.forEach(rm => rm.rounds.forEach((rd, n) => rd.items.forEach((it, i) => out.push({ it, k: `${r.id}:${rm.id}:${n}:${i}`, r, rm, rd, n, i }))));
  return out;
}
const all = () => [...R.values()].flatMap(items);
const byKey = k => { const r = R.get(k.split(':')[0]); return r && items(r).find(x => x.k === k); };
const status = r => r.closed ? 'closed' : items(r).some(x => x.it.st === 'q' || x.it.st === 'g') ? 'gen' : 'wait';
const slug = s => s.normalize('NFD').replace(/[̀-ͯ]/g, '').toLowerCase().replace(/[^a-z0-9]+/g, '_').replace(/^_|_$/g, '') || 'pedido';
const lines = p => Array.isArray(p) ? p : String(p || '').split(/\n+/).map(s => s.trim()).filter(Boolean);
const newItem = m => ({ m, st: 'q', seq: SEQ++, file: null, err: '', took: 0, cost: MODELS[m].cost, pick: false, rej: false, seen: false });

const saveTimers = new Map();
function save(r) {
  clearTimeout(saveTimers.get(r.id));
  saveTimers.set(r.id, setTimeout(() => {
    fs.writeFileSync(path.join(REQS, r.id + '.json'), JSON.stringify(r, null, 1));
    // cópia ao lado das imagens: o pedido se refaz igual a partir da pasta
    try { fs.writeFileSync(path.join(abs(r.folder), 'atelier.json'), JSON.stringify(r, null, 1)); } catch {}
  }, 150));
}
const abs = p => path.isAbsolute(p) ? p : path.join(REPO, p);
const rel = p => path.relative(REPO, p).split(path.sep).join('/');

function nextFolder(title) {
  const base = path.join(REPO, 'captures', 'concept');
  const nums = fs.existsSync(base) ? fs.readdirSync(base).map(d => +(d.match(/^(\d+)_/) || [])[1]).filter(n => n) : [];
  return `captures/concept/${String(Math.max(50, ...nums) + 1).padStart(2, '0')}_${slug(title)}/`;
}

function create(b) {
  if (!b.title) throw new Error('falta o título');
  if (!b.prompt) throw new Error('falta o prompt');
  const id = Date.now().toString(36) + Math.random().toString(36).slice(2, 5);
  const folder = (b.folder || nextFolder(b.title)).replace(/\\/g, '/').replace(/\/?$/, '/');
  const dir = abs(folder);
  fs.mkdirSync(path.join(dir, '_anexos'), { recursive: true });
  const atts = (b.attachments || []).map((a, i) => {
    if (a.dataUrl) { // colada ou arrastada na tela
      const m = a.dataUrl.match(/^data:image\/(\w+);base64,(.*)$/); if (!m) throw new Error('imagem inválida');
      const dst = path.join(dir, '_anexos', `${i + 1}_${slug(a.label || 'colada')}.${m[1] === 'jpeg' ? 'jpg' : m[1]}`);
      fs.writeFileSync(dst, Buffer.from(m[2], 'base64'));
      return { file: rel(dst), label: (a.label || 'colada').replace(/\.\w+$/, ''), root: b.root != null ? i === b.root : i === 0 };
    }
    const src = abs(a.path || a);
    if (!fs.existsSync(src)) throw new Error('anexo não existe: ' + src);
    const dst = path.join(dir, '_anexos', `${i + 1}_${path.basename(src)}`);
    if (path.resolve(src) !== path.resolve(dst)) fs.copyFileSync(src, dst);
    return { file: rel(dst), label: a.label || path.basename(src).replace(/\.\w+$/, ''), root: b.root != null ? i === b.root : i === 0 };
  });
  const slots = slotsOf(b.models || { gpt: 1 });
  const r = { id, title: b.title, src: b.session || 'Você', folder, created: new Date().toISOString(), closed: false, aspect: b.aspect || '16:9',
    root: lines(b.prompt), att: atts, thread: [] };
  if (b.items && b.items.length) {
    Object.assign(r, { kind: 'batch', slots, items: b.items.map(name => ({ name, cells: slots.map(newItem) })) });
  } else {
    r.ramos = [{ id: 'a', name: b.branch || 'Raiz', slots, prompt: null, rounds: [] }];
    addRound(r, r.ramos[0]);
  }
  R.set(id, r); save(r); changed(r);
  log('novo', id, r.title, 'de', r.src, slots.join('+'));
  return r;
}

function slotsOf(models) {
  const map = { gpt: 'gpt', gemini_pro: 'pro', pro: 'pro', gemini_flash: 'flash', flash: 'flash', fake: 'fake', test: 'fake' };
  const out = [];
  for (const [k, n] of Object.entries(models)) { const m = map[k]; if (!m) throw new Error('modelo desconhecido: ' + k); for (let i = 0; i < Math.min(+n || 0, 4); i++) out.push(m); }
  if (!out.length) throw new Error('nenhum modelo escolhido');
  return out;
}
function addRound(r, rm, slots) {
  rm.rounds.push({ prompt: rm.prompt || null, items: (slots || rm.slots).map(newItem) });
  r.closed = false;
}
function ramoOf(r, ref) { return r.ramos.find(x => x.id === ref || x.name.toLowerCase() === String(ref || '').toLowerCase()) || r.ramos[r.ramos.length - 1]; }

/* ---------- fila ---------- */
const running = new Map(); // item → AbortController
function promptFor(x) {
  const r = x.r;
  if (r.kind === 'batch') return r.root.join('\n').replaceAll('{item}', x.row.name);
  return (x.rd.prompt || x.rm.prompt || r.root).join('\n');
}
function outBase(x) {
  const dir = abs(x.r.folder);
  const m = x.it.m;
  if (x.r.kind === 'batch') return path.join(dir, `${slug(x.row.name)}_${m}${x.r.slots.filter(s => s === m).length > 1 ? '_' + (x.ci + 1) : ''}`);
  const same = x.rd.items.filter(i => i.m === m);
  const k = same.length > 1 ? '_' + (same.indexOf(x.it) + 1) : '';
  return path.join(dir, `${slug(x.rm.name)}_r${x.n + 1}_${m}${k}`);
}
function pump() {
  const list = all();
  for (const prov of Object.keys(PAR)) {
    let run = list.filter(x => x.it.st === 'g' && MODELS[x.it.m].provider === prov).length;
    const q = list.filter(x => x.it.st === 'q' && MODELS[x.it.m].provider === prov).sort((a, b) => a.it.seq - b.it.seq);
    while (run < PAR[prov] && q.length) { start(q.shift()); run++; }
  }
}
function start(x) {
  const { it, r } = x, M = MODELS[it.m];
  it.st = 'g'; it.err = ''; it.t0 = Date.now();
  const ac = new AbortController(); running.set(it, ac);
  changed(r);
  const attachments = r.att.map(a => abs(a.file));
  PROVIDERS[M.provider]({ prompt: promptFor(x), attachments, outBase: outBase(x), model: M.model, aspect: r.aspect, signal: ac.signal })
    .then(file => { it.st = 'd'; it.file = rel(file); it.took = Math.round((Date.now() - it.t0) / 1000); log('pronta', x.k, it.file, it.took + 's'); })
    .catch(e => { it.st = 'f'; it.err = String(e.message || e); log('falhou', x.k, it.err); })
    .finally(() => { running.delete(it); save(r); changed(r); pump(); });
}
setInterval(pump, 1000);

/* ---------- avisos: a tela (SSE) e quem espera (sessões) ---------- */
const streams = new Set(), waiters = new Set();
function changed(r) {
  const msg = `data: ${JSON.stringify({ id: r.id })}\n\n`;
  for (const s of streams) s.write(msg);
  for (const w of [...waiters]) if (w.check()) { waiters.delete(w); w.done(); }
}
function view(r) {
  const out = JSON.parse(JSON.stringify(r));
  out.status = status(r);
  for (const x of items(out)) if (x.it.st === 'g') x.it.elapsed = Math.round((Date.now() - x.it.t0) / 1000);
  return out;
}
// Resumo legível pra sessão: o que está pronto, onde, e o que ele escolheu.
function summary(r) {
  const st = { q: 'na fila', g: 'gerando', d: 'pronta', f: 'falhou' };
  const L = [`${r.title} [${r.id}] · ${{ gen: 'gerando', wait: 'esperando escolha', closed: 'fechado' }[status(r)]} · pasta ${r.folder}`];
  if (r.approved) L.push(`APROVADO por ele (${r.approved.area})${r.approved.said ? ': "' + r.approved.said + '"' : ''}`);
  for (const x of items(r)) {
    const where = x.rm ? `${x.rm.name} r${x.n + 1}` : x.row.name;
    L.push(`- ${where} · ${MODELS[x.it.m].short}: ${st[x.it.st]}${x.it.file ? ' → ' + x.it.file : ''}${x.it.err ? ' (' + x.it.err + ')' : ''}${x.it.pick ? ' ★ ESCOLHIDA' : ''}${x.it.rej ? ' ✕ reprovada' : ''}`);
  }
  const said = r.thread.filter(m => m.who === 'me').slice(-3).map(m => m.text);
  if (said.length) L.push('O que ele disse: ' + said.map(s => `"${s}"`).join(' · '));
  return L.join('\n');
}

/* ---------- o Claude dentro: `claude -p` na assinatura dele, olhando as imagens pelo Read ---------- */
const CLAUDE = process.env.CLAUDE_BIN || path.join(process.env.USERPROFILE || '', '.local', 'bin', 'claude.exe');
function claudeContext(r, text, selKey) {
  const L = [];
  L.push('Você é o diretor de arte dentro do Atelier, o app do Felipe pra gerar imagens no GPT e no Gemini pro jogo dele, um RTS estilo C&C Zero Hour moderno (câmera de RTS vista de cima; jogabilidade antes do realismo; ancorar sempre na raiz aprovada, nunca encadear derivadas).');
  L.push('Responda em português, curto e direto, como profissional: diga o que vê e o que mudaria, com motivo.');
  L.push(`\nPedido: ${r.title} (sessão ${r.src}), pasta ${r.folder}`);
  L.push('Prompt da raiz:\n' + r.root.join('\n'));
  L.push('\nAnexos, na ordem em que vão pro gerador (image 1, image 2...):');
  r.att.forEach((a, i) => L.push(`image ${i + 1}: ${abs(a.file)} (${a.label}${a.root ? ', RAIZ aprovada' : ''}${a.derived ? ', derivada de uma rodada' : ''})`));
  if (r.kind === 'batch') {
    L.push(`\nLote com ${r.items.length} itens, modelos ${r.slots.map(m => MODELS[m].short).join(' + ')}; o prompt troca {item} pelo nome.`);
    for (const x of items(r).filter(x => x.it.st === 'd').slice(0, 12)) L.push(`- ${x.row.name} · ${MODELS[x.it.m].short}: ${abs(x.it.file)}${x.it.pick ? ' ★' : ''}${x.it.rej ? ' ✕' : ''}`);
  } else for (const rm of r.ramos) {
    L.push(`\nRamo "${rm.name}" (${rm.slots.map(m => MODELS[m].short).join(' + ')})` + (rm.prompt ? ', prompt próprio:\n' + rm.prompt.join('\n') : ', prompt da raiz'));
    rm.rounds.forEach((rd, n) => rd.items.forEach(it => L.push(`- r${n + 1} ${MODELS[it.m].short}: ${it.st === 'd' ? abs(it.file) : { q: 'na fila', g: 'gerando', f: 'falhou: ' + it.err }[it.st]}${it.pick ? ' ★ escolhida' : ''}${it.rej ? ' ✕ reprovada' : ''}`)));
  }
  const sel = selKey && byKey(selKey);
  if (sel && sel.it.file) L.push(`\nImagem que ele está olhando agora: ${abs(sel.it.file)}`);
  const past = r.thread.slice(-8).map(m => `${m.who === 'me' ? 'Felipe' : 'Você'}: ${m.text.replace(/<[^>]+>/g, '')}`);
  if (past.length) L.push('\nConversa até aqui:\n' + past.join('\n'));
  L.push(`\nMensagem dele agora: ${text}`);
  L.push('\nAbra as imagens com a ferramenta Read quando precisar olhar. Responda SÓ com um JSON, sem texto fora dele:');
  L.push('{"reply": "sua resposta curta", "suggestion": null}');
  L.push('ou, pra propor a próxima rodada de um ramo: {"reply": "...", "suggestion": {"kind": "round", "branch": "<nome do ramo>", "prompt": "<prompt INTEIRO novo, em inglês, uma ideia por linha>"}}');
  L.push('ou, pra propor outra visão num ramo novo: {"reply": "...", "suggestion": {"kind": "branch", "name": "<nome curto>", "prompt": "<prompt INTEIRO, em inglês>", "models": {"gpt": 1, "gemini_pro": 1}}}');
  return L.join('\n');
}
function askClaude(r, text, selKey) {
  r.thread.push({ who: 'me', text, at: new Date().toISOString() });
  r.thinking = true; save(r); changed(r);
  const p = spawn(CLAUDE, ['-p', '--output-format', 'json', '--allowedTools', 'Read'], { cwd: REPO, windowsHide: true });
  let so = '', se = '';
  p.stdout.on('data', d => so += d); p.stderr.on('data', d => se += d);
  const finish = msg => { r.thinking = false; r.thread.push(Object.assign({ who: 'claude', at: new Date().toISOString() }, msg)); save(r); changed(r); };
  p.on('error', e => finish({ text: 'Não consegui abrir o Claude: ' + e.message }));
  p.on('close', () => {
    try {
      const out = JSON.parse(so); const res = String(out.result || '');
      const j = JSON.parse(res.slice(res.indexOf('{'), res.lastIndexOf('}') + 1));
      let sug = j.suggestion || null;
      if (sug && sug.prompt) {
        const next = lines(sug.prompt);
        const base = sug.kind === 'round' && r.ramos ? (ramoOf(r, sug.branch).prompt || r.root) : r.root;
        if (sug.kind === 'round' && r.ramos) sug.branch = ramoOf(r, sug.branch).name;
        sug.del = base.filter(l => !next.includes(l)); sug.add = next.filter(l => !base.includes(l)); sug.prompt = next;
      } else sug = null;
      finish({ text: String(j.reply || '').trim() || '(sem resposta)', sug });
    } catch (e) { log('claude ilegível', se.slice(-300), so.slice(-300)); finish({ text: 'O Claude respondeu fora do formato. ' + (so ? '' : se.slice(-200)) }); }
  });
  p.stdin.end(claudeContext(r, text, selKey));
}

/* ---------- configurações: o login do Codex e o teste das chaves (sem gerar imagem) ---------- */
const CODEX = { loggedIn: null, text: '' };
function codexStatus() {
  return new Promise(resolve => {
    const p = spawn(process.env.ComSpec || 'cmd.exe', ['/d', '/s', '/c', 'codex', 'login', 'status'], { windowsHide: true });
    let o = ''; p.stdout.on('data', d => o += d); p.stderr.on('data', d => o += d);
    const t = setTimeout(() => p.kill(), 15000);
    p.on('error', () => { clearTimeout(t); Object.assign(CODEX, { loggedIn: false, text: 'Codex não instalado' }); resolve(CODEX); });
    p.on('close', code => {
      clearTimeout(t);
      const line = o.trim().split('\n').pop() || '';
      const ok = code === 0 && /logged in/i.test(line);
      Object.assign(CODEX, { loggedIn: ok, text: ok ? (/chatgpt/i.test(line) ? 'Logado na conta do ChatGPT' : 'Logado') : 'Não está logado' });
      resolve(CODEX);
    });
  });
}
codexStatus();
async function testProvider(p, typed) {
  if (p === 'codex') { await codexStatus(); return { ok: CODEX.loggedIn, msg: CODEX.text }; }
  const k = typed || (key(p) || {}).value;
  if (!k) return { ok: false, msg: 'Nenhuma chave pra testar.' };
  if (p === 'gemini') {
    const r = await fetch('https://generativelanguage.googleapis.com/v1beta/models?pageSize=200', { headers: { 'x-goog-api-key': k } });
    const j = await r.json().catch(() => ({}));
    if (!r.ok) return { ok: false, msg: `O Google recusou a chave (${r.status}): ${(j.error && j.error.message || '').slice(0, 160)}` };
    const img = (j.models || []).map(m => m.name.replace('models/', '')).filter(n => /image/.test(n));
    const want = [MODELS.pro.model, MODELS.flash.model];
    return { ok: true, msg: `Chave aceita. ${want.map(w => `${w}: ${img.includes(w) ? 'liberado' : 'não aparece'}`).join(' · ')}. A cota só aparece no primeiro pedido.` };
  }
  if (p === 'openai') {
    const r = await fetch('https://api.openai.com/v1/models', { headers: { authorization: `Bearer ${k}` } });
    const j = await r.json().catch(() => ({}));
    if (!r.ok) return { ok: false, msg: `A OpenAI recusou a chave (${r.status}): ${(j.error && j.error.message || '').slice(0, 160)}` };
    const img = (j.data || []).map(m => m.id).filter(n => /image/.test(n));
    return { ok: true, msg: `Chave aceita. ${img.includes(MODELS.gpt.model) ? MODELS.gpt.model + ' liberado' : 'modelos de imagem: ' + (img.join(', ') || 'nenhum')}.` };
  }
  return { ok: false, msg: 'provedor desconhecido' };
}
/* ---------- pastas: as de captures/concept pra escolher na tela, e o seletor do Windows ---------- */
const IMG_EXT = /\.(png|jpe?g|webp)$/i;
function listFolders() {
  const base = path.join(REPO, 'captures', 'concept');
  if (!fs.existsSync(base)) return [];
  return fs.readdirSync(base, { withFileTypes: true }).filter(d => d.isDirectory() && !d.name.startsWith('_')).map(d => {
    const dir = path.join(base, d.name);
    let imgs = [];
    try { imgs = fs.readdirSync(dir).filter(f => IMG_EXT.test(f)); } catch {}
    const st = fs.statSync(dir);
    const mtime = Math.max(st.mtimeMs, ...imgs.slice(0, 60).map(f => fs.statSync(path.join(dir, f)).mtimeMs));
    const thumb = imgs.sort((a, b) => fs.statSync(path.join(dir, b)).mtimeMs - fs.statSync(path.join(dir, a)).mtimeMs)[0];
    return { name: d.name, path: `captures/concept/${d.name}/`, images: imgs.length, mtime, thumb: thumb ? `captures/concept/${d.name}/${thumb}` : null };
  }).sort((a, b) => b.mtime - a.mtime);
}
function pickFolder(start) {
  return new Promise(resolve => {
    const p = spawn('powershell.exe', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', path.join(HERE, 'pick-folder.ps1'), '-Start', abs(start || 'captures/concept').replace(/\//g, '\\')], { windowsHide: true });
    let o = ''; p.stdout.on('data', d => o += d);
    p.on('error', () => resolve({ cancelled: true }));
    p.on('close', () => {
      const chosen = o.trim();
      if (!chosen) return resolve({ cancelled: true });
      const inRepo = path.resolve(chosen).startsWith(REPO + path.sep);
      resolve({ path: (inRepo ? rel(chosen) : chosen.replace(/\\/g, '/')).replace(/\/?$/, '/') });
    });
  });
}

const settingsView = () => ({ gptVia: readSettings().gptVia, keys: publicKeys(), codex: CODEX, models: { pro: MODELS.pro.model, flash: MODELS.flash.model, gpt: MODELS.gpt.model } });

/* ---------- HTTP ---------- */
const body = req => new Promise((res, rej) => { let b = ''; req.on('data', d => b += d); req.on('end', () => { try { res(b ? JSON.parse(b) : {}); } catch (e) { rej(new Error('JSON inválido')); } }); });
const send = (res, code, obj, type = 'application/json; charset=utf-8') => { res.writeHead(code, { 'content-type': type, 'cache-control': 'no-store' }); res.end(type.startsWith('application/json') ? JSON.stringify(obj) : obj); };
const TYPES = { '.png': 'image/png', '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg', '.webp': 'image/webp', '.html': 'text/html; charset=utf-8' };

const routes = {
  'POST /api/open': b => { const r = R.get(b.id); if (!r) throw new Error('pedido não existe'); spawn('explorer.exe', [abs(r.folder).replace(/\//g, '\\')], { detached: true, stdio: 'ignore' }).unref(); return { ok: true }; },
  'GET /api/health': () => ({ ok: true, pid: process.pid, boot: BOOT, repo: REPO, gptVia: readSettings().gptVia, models: Object.fromEntries(Object.entries(MODELS).map(([k, m]) => [k, { ...m, ready: m.provider === 'gemini' ? hasKey('gemini') : m.provider === 'openai' ? hasKey('openai') : m.provider === 'codex' ? CODEX.loggedIn !== false : true }])) }),
  'GET /api/folders': () => listFolders(),
  'POST /api/pick-folder': b => pickFolder(b.start),
  'GET /api/settings': async () => { await codexStatus(); return settingsView(); },
  'POST /api/settings': b => { writeSettings(b); log('configurações salvas', 'gpt via', readSettings().gptVia); for (const r of R.values()) changed(r); return settingsView(); },
  'POST /api/settings/test': b => testProvider(b.provider, b.key),
  'POST /api/codex-login': () => { spawn(process.env.ComSpec || 'cmd.exe', ['/d', '/c', 'start', '"Codex login"', 'cmd', '/k', 'codex login'], { detached: true, stdio: 'ignore' }).unref(); return { ok: true }; },
  'GET /api/requests': () => [...R.values()].sort((a, b) => b.created.localeCompare(a.created)).map(view),
  'POST /api/requests': b => view(create(b)),
  'GET /api/request': (b, q) => { const r = R.get(q.get('id')); if (!r) throw new Error('pedido não existe'); return q.get('text') ? { text: summary(r) } : view(r); },
  'POST /api/round': b => { const r = R.get(b.id); if (!r || r.kind === 'batch') throw new Error('pedido não existe'); const rm = ramoOf(r, b.branch);
    if (b.prompt) rm.prompt = lines(b.prompt); addRound(r, rm, b.models ? slotsOf(b.models) : null); save(r); changed(r); return view(r); },
  'POST /api/branch': b => { const r = R.get(b.id); if (!r || r.kind === 'batch') throw new Error('pedido não existe');
    const rm = { id: 'r' + (SEQ++), name: b.name || 'Outra visão', slots: slotsOf(b.models || { gpt: 1 }), prompt: b.prompt ? lines(b.prompt) : null, rounds: [] };
    r.ramos.push(rm); addRound(r, rm); save(r); changed(r); return view(r); },
  'POST /api/item': b => { const x = byKey(b.key); if (!x) throw new Error('imagem não existe'); const it = x.it;
    if (b.op === 'pick') { it.pick = !it.pick; if (it.pick) it.rej = false; }
    if (b.op === 'reject') { it.rej = !it.rej; if (it.rej) it.pick = false; }
    if (b.op === 'seen') it.seen = true;
    if (b.op === 'retry') Object.assign(it, newItem(it.m), { m: it.m });
    if (b.op === 'regen') { if (x.rm) addRound(x.r, x.rm, [it.m]); else Object.assign(it, newItem(it.m)); }
    if (b.op === 'cancel' && running.has(it)) running.get(it).abort();
    save(x.r); changed(x.r); return view(x.r); },
  'POST /api/attach': b => { const r = R.get(b.id); if (!r) throw new Error('pedido não existe');
    let src;
    if (b.key) { const x = byKey(b.key); src = abs(x.it.file); }
    else if (b.dataUrl) { const m = b.dataUrl.match(/^data:image\/(\w+);base64,(.*)$/); if (!m) throw new Error('imagem inválida'); src = path.join(abs(r.folder), '_anexos', `colada_${Date.now()}.${m[1] === 'jpeg' ? 'jpg' : m[1]}`); fs.writeFileSync(src, Buffer.from(m[2], 'base64')); }
    else src = abs(b.path);
    const dst = path.join(abs(r.folder), '_anexos', `${r.att.length + 1}_${path.basename(src)}`);
    if (path.resolve(src) !== path.resolve(dst)) fs.copyFileSync(src, dst);
    r.att.push({ file: rel(dst), label: b.label || path.basename(src).replace(/\.\w+$/, ''), root: false, derived: !!b.key });
    save(r); changed(r); return view(r); },
  // tira o anexo da lista (o arquivo fica em _anexos/; as rodadas feitas já usaram); se era a raiz, a próxima vira a raiz
  'POST /api/detach': b => { const r = R.get(b.id); if (!r || !r.att[b.i]) throw new Error('anexo não existe');
    const [a] = r.att.splice(b.i, 1);
    if (a.root && r.att.length && !r.att.some(x => x.root)) r.att[0].root = true;
    save(r); changed(r); return view(r); },
  'POST /api/say': b => { const r = R.get(b.id); if (!r) throw new Error('pedido não existe'); r.thread.push({ who: b.who || 'me', text: String(b.text || ''), at: new Date().toISOString(), sug: b.sug || undefined }); save(r); changed(r); return view(r); },
  'POST /api/approve': b => { const r = R.get(b.id); if (!r) throw new Error('pedido não existe');
    const picks = items(r).filter(x => x.it.pick && x.it.st === 'd');
    if (!picks.length) throw new Error('nenhuma escolhida');
    const area = slug(b.area || 'cenas'); const dir = path.join(REPO, 'docs', 'aprovados', area); fs.mkdirSync(dir, { recursive: true });
    const copied = picks.map(x => { const src = abs(x.it.file); const dst = path.join(dir, `${slug(r.title)}_${path.basename(src)}`); fs.copyFileSync(src, dst); return rel(dst); });
    r.closed = true; r.approved = { area, said: b.said || '', files: copied, at: new Date().toISOString() };
    save(r); changed(r); log('aprovado', r.id, copied.join(' ')); return view(r); },
  'POST /api/claude': b => { const r = R.get(b.id); if (!r) throw new Error('pedido não existe'); if (r.thinking) throw new Error('o Claude ainda está respondendo'); askClaude(r, String(b.text || '').trim(), b.key); return view(r); },
  'POST /api/sug': b => { const r = R.get(b.id); const m = r && r.thread[b.i]; if (!m || !m.sug || m.sug.done) throw new Error('sugestão não existe');
    const s = m.sug;
    if (s.kind === 'branch') { const rm = { id: 'r' + (SEQ++), name: s.name || 'Outra visão', slots: slotsOf(s.models || { gpt: 1 }), prompt: s.prompt, rounds: [] }; r.ramos.push(rm); addRound(r, rm); s.done = 'ramo'; }
    else { const rm = ramoOf(r, s.branch); rm.prompt = s.prompt; addRound(r, rm, b.only ? [rm.slots[0]] : null); s.done = 'r' + rm.rounds.length; }
    save(r); changed(r); return view(r); },
  'POST /api/close': b => { const r = R.get(b.id); if (!r) throw new Error('pedido não existe'); r.closed = !!b.closed; save(r); changed(r); return view(r); },
};

const server = http.createServer(async (req, res) => {
  const u = new URL(req.url, 'http://x');
  try {
    if (req.method === 'GET' && (u.pathname === '/' || /^\/[\w.-]+\.(js|css|html)$/.test(u.pathname))) {
      const f = u.pathname === '/' ? UI : path.join(path.dirname(UI), u.pathname.slice(1));
      if (fs.existsSync(f)) return send(res, 200, fs.readFileSync(f), { '.js': 'text/javascript; charset=utf-8', '.css': 'text/css; charset=utf-8' }[path.extname(f)] || TYPES['.html']);
      return send(res, 404, 'não achei', 'text/plain; charset=utf-8');
    }
    if (req.method === 'GET' && u.pathname === '/file') {
      const p = path.resolve(abs(u.searchParams.get('p') || ''));
      // só o repositório e as pastas dos pedidos (que podem estar fora dele)
      const allowed = p.startsWith(REPO + path.sep) || [...R.values()].some(r => p.startsWith(path.resolve(abs(r.folder)) + path.sep));
      if (!allowed || !fs.existsSync(p)) return send(res, 404, 'não achei', 'text/plain');
      res.writeHead(200, { 'content-type': TYPES[path.extname(p).toLowerCase()] || 'application/octet-stream', 'cache-control': 'max-age=3600' });
      return fs.createReadStream(p).pipe(res);
    }
    if (req.method === 'GET' && u.pathname === '/api/events') {
      res.writeHead(200, { 'content-type': 'text/event-stream', 'cache-control': 'no-store', connection: 'keep-alive' });
      res.write('retry: 1000\n\n'); streams.add(res); req.on('close', () => streams.delete(res)); return;
    }
    if (req.method === 'GET' && u.pathname === '/api/wait') {
      // a sessão espera até ficar pronto (ou até ele escolher), com teto de tempo
      const r = R.get(u.searchParams.get('id')); if (!r) return send(res, 404, { error: 'pedido não existe' });
      const until = u.searchParams.get('until') || 'done', secs = Math.min(+u.searchParams.get('timeout') || 100, 600);
      const ok = () => until === 'pick' ? (r.closed || items(r).some(x => x.it.pick)) : status(r) !== 'gen';
      const reply = timedOut => send(res, 200, { timedOut, text: summary(r) });
      if (ok()) return reply(false);
      const w = { check: ok, done: () => { clearTimeout(t); reply(false); } };
      const t = setTimeout(() => { waiters.delete(w); reply(true); }, secs * 1000);
      waiters.add(w); req.on('close', () => { waiters.delete(w); clearTimeout(t); }); return;
    }
    const fn = routes[`${req.method} ${u.pathname}`];
    if (!fn) return send(res, 404, { error: 'rota não existe' });
    send(res, 200, await fn(req.method === 'POST' ? await body(req) : {}, u.searchParams));
  } catch (e) { send(res, 400, { error: String(e.message || e) }); }
});
server.on('error', e => { if (e.code === 'EADDRINUSE') { console.log('Atelier já está rodando na porta', PORT); process.exit(0); } throw e; });
server.listen(PORT, '127.0.0.1', () => { log('rodando', PORT, 'pid', process.pid, 'pedidos', R.size); console.log(`Atelier em http://127.0.0.1:${PORT}`); pump(); });
