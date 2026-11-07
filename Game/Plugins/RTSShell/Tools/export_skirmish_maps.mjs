// Tira do mockup aprovado do skirmish o que é imagem pronta de cada mapa de verdade (o export_skirmish_maps.js roda na página) e grava
// em Resources/Skirmish: maps/<mapa>_diamond.png e _thumb.png, o maps.json (a ficha: o nome, as bases, as docas, o tipo) e os
// brasões dos exércitos (docs/concept/factions/small). As imagens chegam por POST num receptor local que este script abre: trazê-las
// pela conexão de controle do Chrome (a resposta do Runtime.evaluate) emperrava depois do 1º mapa. A pasta docs tem de estar servida
// em http://127.0.0.1:8139 (python -m http.server 8139 em docs).
//   node Game/Plugins/RTSShell/Tools/export_skirmish_maps.mjs
import { spawn } from 'node:child_process';
import { createServer } from 'node:http';
import { mkdirSync, writeFileSync, readFileSync, readdirSync, copyFileSync } from 'node:fs';
import { join, dirname, basename } from 'node:path';
import { tmpdir } from 'node:os';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, '..', '..', '..', '..');
const OUT = join(HERE, '..', 'Resources', 'Skirmish');
const URL = 'http://127.0.0.1:8139/concept/ui/menu/skirmish.html?mute=1&fresh=1&still=1';
const PORT = 9733 + Math.floor(Math.random() * 200), SINK = PORT + 300;
mkdirSync(join(OUT, 'maps'), { recursive: true });
// o receptor: cada POST /<arquivo> vira Resources/Skirmish/maps/<arquivo>
let received = 0;
const sink = createServer((req, res) => {
  const chunks = [];
  req.on('data', c => chunks.push(c));
  req.on('end', () => { writeFileSync(join(OUT, 'maps', basename(decodeURIComponent(req.url))), Buffer.concat(chunks)); received++; res.writeHead(204, { 'Access-Control-Allow-Origin': '*' }); res.end(); });
}).listen(SINK, '127.0.0.1');
const chrome = 'C:/Program Files/Google/Chrome/Application/chrome.exe';
const proc = spawn(chrome, ['--headless=new', `--remote-debugging-port=${PORT}`, `--user-data-dir=${join(tmpdir(), 'sk-export-' + PORT)}`, '--window-size=1920,1080',
  '--hide-scrollbars', '--mute-audio', 'about:blank'], { stdio: 'ignore' });
const sleep = ms => new Promise(r => setTimeout(r, ms));
let ws, seq = 0;
const pend = new Map();
const send = (method, params = {}) => new Promise(res => { const id = ++seq; pend.set(id, res); ws.send(JSON.stringify({ id, method, params })); });
const evalJs = async expr => { const r = await send('Runtime.evaluate', { expression: expr, awaitPromise: true, returnByValue: true }); if (r.result?.exceptionDetails) throw new Error(JSON.stringify(r.result.exceptionDetails).slice(0, 400)); return r.result?.result?.value; };
try {
  let list;
  for (let i = 0; i < 50; i++) { try { list = await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json(); if (list.length) break; } catch { } await sleep(200); }
  ws = new WebSocket(list.find(t => t.type === 'page').webSocketDebuggerUrl);
  await new Promise(r => ws.onopen = r);
  ws.onmessage = m => { const d = JSON.parse(m.data); if (d.id && pend.has(d.id)) { pend.get(d.id)(d); pend.delete(d.id); } };
  await send('Runtime.enable');
  await send('Emulation.setDeviceMetricsOverride', { width: 1920, height: 1080, deviceScaleFactor: 1, mobile: false });
  await send('Page.navigate', { url: URL });
  let ready = false;
  for (let i = 0; i < 200 && !ready; i++) { ready = await evalJs('window.__ready === true'); if (!ready) await sleep(150); }
  console.log('pagina', ready ? 'pronta' : 'NAO ficou pronta em 30 s');
  const n = await evalJs(readFileSync(join(HERE, 'export_skirmish_maps.js'), 'utf8'));
  console.log(n, 'mapas de verdade');
  const catalog = [];
  for (let i = 0; i < n; i++) {
    const r = JSON.parse(await evalJs(`window.__exportOne(${i}, ${SINK})`));
    console.log('mapa', i, r.meta.real, 'tempos (ms):', JSON.stringify(r.ms));
    catalog.push(r.meta);
  }
  for (let i = 0; i < 100 && received < n * 2; i++) await sleep(100);
  // (07/11) os mapas que não vêm do mockup (o World_Ozerki, do WorldForge/Tools/plans/ozerki_skirmish.py) ficam, depois dos do mockup
  let kept = [];
  try {
    const old = JSON.parse(readFileSync(join(OUT, 'maps.json'), 'utf8'));
    const mine = new Set(catalog.map(m => m.real));
    kept = (Array.isArray(old) ? old : []).filter(m => m && !mine.has(m.real));
  } catch { }
  if (kept.length) console.log('mantidos (fora do mockup):', kept.map(m => m.real).join(', '));
  writeFileSync(join(OUT, 'maps.json'), JSON.stringify(catalog.concat(kept), null, 1));
  const fac = join(ROOT, 'docs', 'concept', 'factions', 'small');
  for (const f of readdirSync(fac)) copyFileSync(join(fac, f), join(OUT, f));
  console.log('ok:', received, 'imagens, a ficha e os brasoes em', OUT);
} catch (e) { console.error(e); process.exitCode = 1; }
finally { try { ws?.close(); } catch { } proc.kill(); sink.close(); }
