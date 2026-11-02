// Dirige as telas DENTRO do jogo pela porta de depuração do navegador embutido (-cefdebug=<porta>): fotos da página (captura
// interna, não da tela), cliques por JS e o retorno da ponte window.ue.shell. Uso: node shell_cdp.mjs <porta> <pasta> <passos.json>
// passos: [{ "go": "skirmish.html" } | { "wait": 2000 } | { "shot": "nome" } | { "js": "expr" } | { "log": "expr" }]
import { writeFileSync, readFileSync, mkdirSync } from 'node:fs';
import { join } from 'node:path';
const [port, out, stepsPath] = process.argv.slice(2);
mkdirSync(out, { recursive: true });
const sleep = ms => new Promise(r => setTimeout(r, ms));
let ws, id = 0; const pend = new Map();
const send = (method, params = {}) => new Promise(res => { const i = ++id; pend.set(i, res); ws.send(JSON.stringify({ id: i, method, params })); });
const evalJs = async e => { const r = await send('Runtime.evaluate', { expression: e, awaitPromise: true, returnByValue: true }); return r.result?.exceptionDetails ? 'ERRO: ' + (r.result.exceptionDetails.exception?.description || r.result.exceptionDetails.text) : r.result?.result?.value; };
async function connect() {
  for (let i = 0; i < 40; i++) {   // 20 s: o game_test.ps1 só chama depois do "RTS telas: menu.html" no log
    try { const list = await (await fetch(`http://127.0.0.1:${port}/json/list`)).json(); const p = list.find(t => t.type === 'page' && t.url.startsWith('https://rts.ui/'));
      if (p) { ws = new WebSocket(p.webSocketDebuggerUrl); await new Promise(r => ws.onopen = r);
        ws.onmessage = m => { const d = JSON.parse(m.data); if (d.id && pend.has(d.id)) { pend.get(d.id)(d); pend.delete(d.id); }
          else if (d.method === 'Runtime.exceptionThrown') console.log('[página] erro:', d.params.exceptionDetails.exception?.description || d.params.exceptionDetails.text);
          else if (d.method === 'Runtime.consoleAPICalled' && (d.params.type === 'error' || d.params.type === 'warning')) console.log('[página]', d.params.type, d.params.args.map(a => a.value).join(' ')); };
        await send('Runtime.enable'); await send('Page.enable'); return p.url; } } catch { }
    await sleep(500);
  }
  throw new Error('a página das telas não apareceu na porta ' + port);
}
const steps = JSON.parse(readFileSync(stepsPath, 'utf8'));
console.log('conectado:', await connect());
for (const s of steps) {
  if (s.go) { await send('Page.navigate', { url: 'https://rts.ui/' + s.go }); await sleep(400); for (let i = 0; i < 60 && !(await evalJs('window.__ready === true')); i++) await sleep(250); console.log('página:', s.go); }
  if (s.wait) await sleep(s.wait);
  if (s.js) console.log('js:', String(JSON.stringify(await evalJs(s.js))).slice(0, 400));
  if (s.log) console.log(s.label || 'valor:', JSON.stringify(await evalJs(s.log)).slice(0, 600));
  if (s.shot) { const r = await send('Page.captureScreenshot', { format: 'jpeg', quality: 88 }); writeFileSync(join(out, s.shot + '.jpg'), Buffer.from(r.result.data, 'base64')); console.log('foto:', s.shot); }
  if (s.end) break;
}
try { ws.close(); } catch { }
console.log('fim');
