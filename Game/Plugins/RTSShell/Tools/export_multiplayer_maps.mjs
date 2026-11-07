// Grava em Resources/Skirmish/maps as imagens da lista de salas (<mapa>_mpicon.png e _mppv.png), tiradas do multiplayer.html aprovado
// pelo export_multiplayer_maps.js. Mesmo esquema do export_skirmish_maps.mjs: o Chrome sem janela, a página servida e um receptor local
// pros POST. A pasta docs tem de estar servida em http://127.0.0.1:8139 (python -m http.server 8139 em docs).
//   node Game/Plugins/RTSShell/Tools/export_multiplayer_maps.mjs
import { spawn } from 'node:child_process';
import { createServer } from 'node:http';
import { writeFileSync, readFileSync } from 'node:fs';
import { join, dirname, basename } from 'node:path';
import { tmpdir } from 'node:os';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
const OUT = join(HERE, '..', 'Resources', 'Skirmish', 'maps');
const URL = 'http://127.0.0.1:8139/concept/ui/menu/multiplayer.html?mute=1';
const PORT = 9733 + Math.floor(Math.random() * 200), SINK = PORT + 300;
let received = 0;
const sink = createServer((req, res) => {
  const chunks = [];
  req.on('data', c => chunks.push(c));
  req.on('end', () => { writeFileSync(join(OUT, basename(decodeURIComponent(req.url))), Buffer.concat(chunks)); received++; res.writeHead(204, { 'Access-Control-Allow-Origin': '*' }); res.end(); });
}).listen(SINK, '127.0.0.1');
const proc = spawn('C:/Program Files/Google/Chrome/Application/chrome.exe', ['--headless=new', `--remote-debugging-port=${PORT}`,
  `--user-data-dir=${join(tmpdir(), 'mp-export-' + PORT)}`, '--window-size=1920,1080', '--hide-scrollbars', '--mute-audio', 'about:blank'], { stdio: 'ignore' });
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
  await send('Page.navigate', { url: URL });
  let ready = false;
  for (let i = 0; i < 200 && !ready; i++) { ready = await evalJs('window.__ready === true'); if (!ready) await sleep(150); }
  console.log('pagina', ready ? 'pronta' : 'NAO ficou pronta em 30 s');
  await evalJs(readFileSync(join(HERE, 'export_multiplayer_maps.js'), 'utf8'));
  console.log(await evalJs(`window.__exportMp(${SINK})`));
  for (let i = 0; i < 100 && received < 6; i++) await sleep(100);
  console.log('ok:', received, 'imagens em', OUT);
} catch (e) { console.error(e); process.exitCode = 1; }
finally { try { ws?.close(); } catch { } proc.kill(); sink.close(); }
