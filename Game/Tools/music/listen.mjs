// (06/11/2026, a trilha da partida, docs/TRILHA_DA_PARTIDA.md) O "ouvido": o Gemini escuta cada faixa e devolve a ficha dela em JSON
// (estilo, instrumentos, energia por trecho com os segundos, como começa e termina, os defeitos, pra que serve no jogo). A chave é a do
// Atelier (Atelier/core/settings.mjs: nada de chave aqui). A faixa vai como mp3 leve (o ffmpeg do imageio_ffmpeg), o limite do pedido
// é 20 MB.
//   node Game/Tools/music/listen.mjs <saída.json> <faixa.mp3|wav> [...]   (as que já estão na saída não são pedidas de novo)
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { execFileSync } from 'node:child_process';
import { key } from '../../../Atelier/core/settings.mjs';

const MODEL = process.env.LISTEN_MODEL || 'gemini-3.1-pro-preview';
const FF = execFileSync('python', ['-c', 'import imageio_ffmpeg;print(imageio_ffmpeg.get_ffmpeg_exe())']).toString().trim();
const [out, ...tracks] = process.argv.slice(2);
const done = fs.existsSync(out) ? JSON.parse(fs.readFileSync(out, 'utf8')) : {};

// (06/11) os fins da partida (os arquivos *_victory / *_defeat) são curtos de propósito: julgados como o tema do fim, não como faixa
const STING = (fac, kind) => `You are the audio director of a AAA modern-warfare RTS (factions USA, China, GLA desert insurgents, Russia).
This is a SHORT ${kind} sting (about 15-20 seconds, played once when the "${fac}" player ${kind === 'victory' ? 'wins' : 'loses'} the match).
Judge it strictly AS A STING (being short is correct). Return ONLY a JSON object:
{ "genre": "...", "instruments": ["..."], "mood": "...", "fits_${kind}": 1-10, "faction_fit": 1-10, "faction_fit_why": "...",
  "production": 1-10, "hook": 1-10, "ending": "natural ending | fade out | abrupt cut", "ending_detail": "last 4 seconds",
  "problems": ["with timestamps"], "role": "${kind}", "verdict": "keep | keep with fixes | replace", "verdict_why": "one sentence" }`;

const PROMPT = (fac) => `You are the audio director of a AAA modern-warfare RTS (top-down camera, in the spirit of Command & Conquer Generals Zero Hour:
factions USA, China, GLA (desert insurgents) and Russia). This track belongs to the "${fac}" faction's in-match soundtrack (or the
main menu if "${fac}" is "menu"). Listen to the WHOLE track carefully and judge it honestly, like a strict music supervisor
(do not be polite: the goal is an iconic soundtrack like Zero Hour's or Red Alert's).

Return ONLY a JSON object with these fields:
{
 "genre": "short style label",
 "instruments": ["main instruments and sounds"],
 "mood": "2-6 words",
 "energy": 1-10, "tension": 1-10,
 "tempo_feel": "e.g. driving 120 bpm march",
 "structure": [ {"start_s": 0, "end_s": 18, "section": "intro|build|main|break|climax|outro", "energy": 1-10, "what": "what happens"} ],
 "intro": "how it starts (cold open, fade in, hit...)",
 "ending": "natural ending | fade out | abrupt cut mid-phrase | ends on a hit",
 "ending_detail": "what happens in the last 10 seconds",
 "role": "calm (base building, economy) | battle | both | theme (load screen / faction anthem) | menu",
 "faction_fit": 1-10, "faction_fit_why": "does it sound like this faction?",
 "production": 1-10,
 "problems": ["concrete flaws with timestamps: AI artifacts, muddy mix, clipping, weak drums, repetitive loop, odd transitions, vocals, abrupt cut..."],
 "hook": 1-10, "hook_where_s": 0, "hook_what": "the most memorable moment",
 "best_entry_s": 0, "best_entry_why": "a downbeat where the track is already strong (to enter mid-track when a battle starts)",
 "verdict": "keep | keep with fixes | replace", "verdict_why": "one sentence"
}`;

async function listen(file) {
  const fac = path.basename(path.dirname(file));
  const tmp = path.join(os.tmpdir(), `listen_${process.pid}_${path.basename(file)}.mp3`);
  execFileSync(FF, ['-v', 'quiet', '-y', '-i', file, '-ac', '2', '-b:a', '96k', tmp]);
  const b64 = fs.readFileSync(tmp).toString('base64');
  fs.rmSync(tmp, { force: true });
  const body = {
    contents: [{ role: 'user', parts: [{ inline_data: { mime_type: 'audio/mp3', data: b64 } }, { text: /_(victory|defeat)(_v\d+)?\.mp3$/.test(file) ? STING(fac, file.match(/_(victory|defeat)/)[1]) : PROMPT(fac) }] }],
    generationConfig: { responseMimeType: 'application/json', temperature: 0.2 },
  };
  for (let attempt = 1; attempt <= 3; attempt++) {
    const r = await fetch(`https://generativelanguage.googleapis.com/v1beta/models/${MODEL}:generateContent`, {
      method: 'POST', headers: { 'content-type': 'application/json', 'x-goog-api-key': key('gemini').value }, body: JSON.stringify(body),
    });
    const j = await r.json().catch(() => ({}));
    if (r.ok) {
      const txt = (((j.candidates || [])[0] || {}).content || {}).parts?.map(p => p.text || '').join('') || '';
      try { const o = JSON.parse(txt); return Array.isArray(o) ? o[0] : o; } catch { return { raw: txt }; }
    }
    console.log(`  ${path.basename(file)}: Gemini ${r.status} ${(j.error && j.error.message || '').slice(0, 160)} (tentativa ${attempt})`);
    await new Promise(res => setTimeout(res, 4000 * attempt));
  }
  return { error: 'falhou' };
}

if (!key('gemini')) { console.log('falta a chave do Gemini no Atelier'); process.exit(1); }
const todo = tracks.filter(t => !done[`${path.basename(path.dirname(t))}/${path.basename(t)}`]);
let i = 0;
async function worker() {
  while (i < todo.length) {
    const t = todo[i++];
    const id = `${path.basename(path.dirname(t))}/${path.basename(t)}`;
    const t0 = Date.now();
    const res = await listen(t);
    done[id] = res;
    fs.writeFileSync(out, JSON.stringify(done, null, 1));
    console.log(`${id}: ${res.role || '?'} · energia ${res.energy ?? '?'} · ${res.ending || ''} · ${res.verdict || res.error || ''} (${((Date.now() - t0) / 1000).toFixed(0)} s)`);
  }
}
await Promise.all([worker(), worker(), worker()]);
console.log('fim');
