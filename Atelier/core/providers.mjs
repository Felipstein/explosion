// Provedores de imagem do Atelier. Cada um recebe o prompt pronto, os anexos (caminhos, na ordem
// "image 1, image 2...") e a base do arquivo de saída, e devolve o arquivo gravado.
// GPT pela conta do ChatGPT (Codex CLI) não custa por imagem; Gemini e a API da OpenAI precisam de chave.
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { key as keyOf } from './settings.mjs';

const MIME = { '.png': 'image/png', '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg', '.webp': 'image/webp' };
const mimeOf = f => MIME[path.extname(f).toLowerCase()] || 'image/png';
const key = name => (keyOf(name) || {}).value || null;

function writeB64(b64, mime, outBase) {
  const ext = mime.includes('jpeg') ? '.jpg' : mime.includes('webp') ? '.webp' : '.png';
  const file = outBase + ext;
  fs.writeFileSync(file, Buffer.from(b64, 'base64'));
  return file;
}

/* GPT pela conta do ChatGPT: o Codex CLI roda sem janela, chama a ferramenta de imagem e grava em
   ~/.codex/generated_images/<thread>/. O prompt vai pela entrada (o -i engole argumentos soltos). */
const CODEX_IMAGES = path.join(os.homedir(), '.codex', 'generated_images');
async function codex({ prompt, attachments, outBase, signal }) {
  const work = fs.mkdtempSync(path.join(os.tmpdir(), 'atelier-codex-'));
  const args = ['/d', '/s', '/c', 'codex', 'exec', '--ephemeral', '--skip-git-repo-check', '-s', 'read-only', '--json', '-C', work];
  for (const a of attachments) args.push('-i', a);
  const n = attachments.length;
  const text = 'Use your image generation tool to create exactly one image, then stop. Do not run commands, do not write files, do not ask questions.' +
    (n ? ` The ${n} attached image${n > 1 ? 's are' : ' is'} numbered in the order given (image 1${n > 1 ? ', image 2' : ''}${n > 2 ? ' ...' : ''}).` : '') +
    '\n\nImage prompt:\n' + prompt;
  const started = Date.now();
  const out = await new Promise((resolve, reject) => {
    const p = spawn(process.env.ComSpec || 'cmd.exe', args, { windowsHide: true, signal });
    let so = '', se = '';
    p.stdout.on('data', d => so += d);
    p.stderr.on('data', d => se += d);
    p.on('error', reject);
    p.on('close', code => code === 0 ? resolve(so) : reject(new Error(`o Codex saiu com ${code}: ${(se || so).trim().split('\n').slice(-2).join(' ').slice(0, 300)}`)));
    p.stdin.end(text);
  });
  let thread = null, found = null, lastMsg = '';
  for (const line of out.split('\n')) {
    let ev; try { ev = JSON.parse(line); } catch { continue; }
    if (ev.thread_id) thread = thread || ev.thread_id;
    const s = JSON.stringify(ev);
    const m = s.match(/"([^"]*generated_images[^"]*\.(?:png|jpg|jpeg|webp))"/i);
    if (m) found = m[1].replace(/\\\\/g, '\\');
    if (ev.item && ev.item.type === 'agent_message' && ev.item.text) lastMsg = ev.item.text;
  }
  if (!found && thread) {
    const dir = path.join(CODEX_IMAGES, thread);
    if (fs.existsSync(dir)) {
      const fsList = fs.readdirSync(dir).map(f => path.join(dir, f)).sort((a, b) => fs.statSync(b).mtimeMs - fs.statSync(a).mtimeMs);
      found = fsList[0] || null;
    }
  }
  if (!found) {
    // sem o id da conversa: a imagem mais nova criada depois do início
    const recent = [];
    if (fs.existsSync(CODEX_IMAGES)) for (const d of fs.readdirSync(CODEX_IMAGES)) {
      const dd = path.join(CODEX_IMAGES, d);
      if (!fs.statSync(dd).isDirectory()) continue;
      for (const f of fs.readdirSync(dd)) { const ff = path.join(dd, f); const st = fs.statSync(ff); if (st.mtimeMs >= started) recent.push([st.mtimeMs, ff]); }
    }
    if (recent.length === 1) found = recent[0][1];
  }
  fs.rmSync(work, { recursive: true, force: true });
  if (!found || !fs.existsSync(found)) throw new Error('o Codex não devolveu imagem' + (lastMsg ? ': ' + lastMsg.slice(0, 200) : ''));
  const file = outBase + path.extname(found).toLowerCase();
  fs.copyFileSync(found, file);
  return file;
}

/* Gemini (API): as imagens vão antes do texto, na ordem, pra "image 1" ser o primeiro anexo. */
async function gemini({ prompt, attachments, outBase, model, aspect, signal }) {
  const k = key('gemini');
  if (!k) throw new Error('falta a chave do Gemini (Configurações)');
  const parts = attachments.map(a => ({ inline_data: { mime_type: mimeOf(a), data: fs.readFileSync(a).toString('base64') } }));
  parts.push({ text: prompt });
  const r = await fetch(`https://generativelanguage.googleapis.com/v1beta/models/${model}:generateContent`, {
    method: 'POST', signal,
    headers: { 'content-type': 'application/json', 'x-goog-api-key': k },
    body: JSON.stringify({ contents: [{ role: 'user', parts }], generationConfig: { responseModalities: ['IMAGE'], imageConfig: { aspectRatio: aspect || '16:9' } } }),
  });
  const j = await r.json().catch(() => ({}));
  if (!r.ok) throw new Error(`Gemini ${r.status}: ${(j.error && j.error.message || '').slice(0, 240)}`);
  const cand = (j.candidates || [])[0] || {};
  const img = ((cand.content || {}).parts || []).map(p => p.inlineData || p.inline_data).find(Boolean);
  if (!img) throw new Error('o Gemini não devolveu imagem' + (cand.finishReason ? ` (${cand.finishReason})` : ''));
  return writeB64(img.data, img.mimeType || img.mime_type || 'image/png', outBase);
}

/* API da OpenAI: com anexo vai pelo /images/edits (várias referências), sem anexo pelo /generations. */
const OPENAI_SIZE = { '16:9': '1536x1024', '3:2': '1536x1024', '1:1': '1024x1024', '9:16': '1024x1536', '2:3': '1024x1536' };
async function openai({ prompt, attachments, outBase, model, aspect, signal }) {
  const k = key('openai');
  if (!k) throw new Error('falta a chave da OpenAI (Configurações)');
  const size = OPENAI_SIZE[aspect] || '1536x1024';
  let r;
  if (attachments.length) {
    const fd = new FormData();
    fd.append('model', model); fd.append('prompt', prompt); fd.append('size', size); fd.append('quality', 'high');
    for (const a of attachments) fd.append('image[]', new Blob([fs.readFileSync(a)], { type: mimeOf(a) }), path.basename(a));
    r = await fetch('https://api.openai.com/v1/images/edits', { method: 'POST', signal, headers: { authorization: `Bearer ${k}` }, body: fd });
  } else {
    r = await fetch('https://api.openai.com/v1/images/generations', { method: 'POST', signal, headers: { authorization: `Bearer ${k}`, 'content-type': 'application/json' },
      body: JSON.stringify({ model, prompt, size, quality: 'high' }) });
  }
  const j = await r.json().catch(() => ({}));
  if (!r.ok) throw new Error(`OpenAI ${r.status}: ${(j.error && j.error.message || '').slice(0, 240)}`);
  const b64 = j.data && j.data[0] && j.data[0].b64_json;
  if (!b64) throw new Error('a OpenAI não devolveu imagem');
  return writeB64(b64, 'image/png', outBase);
}

/* Falso, pros testes: copia uma imagem de exemplo depois de alguns segundos. Não gasta nada. */
const SAMPLES = path.resolve(path.dirname(new URL(import.meta.url).pathname.replace(/^\/(\w:)/, '$1')), '../../captures/atelier/mockup/img');
async function fake({ outBase, signal }) {
  await new Promise((res, rej) => { const t = setTimeout(res, 1500 + Math.random() * 2500); signal && signal.addEventListener('abort', () => { clearTimeout(t); rej(new Error('cancelado')); }); });
  const pool = fs.existsSync(SAMPLES) ? fs.readdirSync(SAMPLES).filter(f => f.startsWith('pool_')) : [];
  if (!pool.length) throw new Error('sem imagens de exemplo');
  const file = outBase + '.jpg';
  fs.copyFileSync(path.join(SAMPLES, pool[Math.floor(Math.random() * pool.length)]), file);
  return file;
}

export const PROVIDERS = { codex, gemini, openai, fake };
export const hasKey = name => !!key(name);
