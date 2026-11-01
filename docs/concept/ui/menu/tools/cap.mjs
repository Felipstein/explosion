// Captura do menu pelo Chrome sem janela (CDP), em 1920x1080, com a placa de vídeo.
//  node cap.mjs shot <url> <saida.jpg>
//  node cap.mjs rec <url> <pasta> <quadros> [roteiro.json]   (roteiro: [{f: quadro, js: "..."}])
import { spawn } from 'node:child_process';
import { mkdirSync, writeFileSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { tmpdir } from 'node:os';

const [mode, url, out, nArg, scriptPath] = process.argv.slice(2);
const PORT = 9333 + Math.floor(Math.random() * 400);
const chrome = 'C:/Program Files/Google/Chrome/Application/chrome.exe';
const prof = join(tmpdir(), 'menu-cap-' + PORT);
const proc = spawn(chrome, ['--headless=new', `--remote-debugging-port=${PORT}`, `--user-data-dir=${prof}`, '--window-size=1920,1080', '--hide-scrollbars',
  '--enable-gpu', '--use-angle=d3d11', '--ignore-gpu-blocklist', '--autoplay-policy=no-user-gesture-required', '--mute-audio', 'about:blank'], { stdio: 'ignore' });
const sleep = ms => new Promise(r => setTimeout(r, ms));
let ws, id = 0; const pend = new Map();
function send(method, params = {}) { return new Promise(res => { const i = ++id; pend.set(i, res); ws.send(JSON.stringify({ id: i, method, params })); }); }
async function evalJs(expr) { const r = await send('Runtime.evaluate', { expression: expr, awaitPromise: true, returnByValue: true }); return r.result?.result?.value; }
try {
  let list; for (let i = 0; i < 50; i++) { try { list = await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json(); if (list.length) break; } catch { } await sleep(200); }
  const page = list.find(t => t.type === 'page');
  ws = new WebSocket(page.webSocketDebuggerUrl);
  await new Promise(r => ws.onopen = r);
  ws.onmessage = m => { const d = JSON.parse(m.data); if (d.id && pend.has(d.id)) { pend.get(d.id)(d); pend.delete(d.id); } else if (d.method === 'Runtime.consoleAPICalled' && d.params.type === 'error') console.error('[page]', d.params.args.map(a => a.value).join(' ')); else if (d.method === 'Runtime.exceptionThrown') console.error('[page]', d.params.exceptionDetails.exception?.description || d.params.exceptionDetails.text); };
  await send('Runtime.enable');
  await send('Emulation.setDeviceMetricsOverride', { width: 1920, height: 1080, deviceScaleFactor: 1, mobile: false });
  await send('Page.navigate', { url });
  for (let i = 0; i < 200; i++) { if (await evalJs('window.__ready === true')) break; await sleep(150); }
  await evalJs('document.fonts.ready.then(() => true)'); await sleep(300);
  const shot = async file => { const r = await send('Page.captureScreenshot', { format: 'jpeg', quality: 92 }); writeFileSync(file, Buffer.from(r.result.data, 'base64')); };
  if (mode === 'shot') { await shot(out); console.log('ok', out, await evalJs('navigator.userAgent.includes("Headless") + " " + (document.querySelector("canvas").getContext("webgl2").getParameter(0x1F01))')); }
  else {
    mkdirSync(out, { recursive: true }); const n = +nArg; const script = scriptPath ? JSON.parse(readFileSync(scriptPath, 'utf8')) : [];
    for (let f = 0; f < n; f++) {
      for (const s of script) if (s.f === f) await evalJs(s.js);
      await evalJs('__step(1/30)'); await shot(join(out, `f_${String(f).padStart(5, '0')}.jpg`));
    }
    console.log('ok', n, 'quadros em', out);
  }
} catch (e) { console.error(e); }
finally { try { ws?.close(); } catch { } proc.kill(); }
