// (06/11/2026, a trilha da partida) Sobe uma faixa pronta pra ElevenLabs Music (o /v1/music/upload) e guarda o song_id: as faixas novas
// podem nascer condicionadas nela (o conditioning_ref do plano, com o trecho), a mesma cara e o mesmo timbre. Pedido dele (06/11): da USA
// só o tema The Last Advance prestou ("a primeira é top"); as novas partem dele. Custa o mesmo que gerar uma faixa do mesmo tamanho.
// O plano que a ElevenLabs lê da faixa (extract_composition_plan) vai junto na receita.
//   node Game/Tools/music/upload.mjs <faixa> <id>
import fs from 'node:fs';
import path from 'node:path';
import { key } from '../../../Atelier/core/settings.mjs';

const ROOT = path.resolve(path.dirname(new URL(import.meta.url).pathname.replace(/^\/(\w:)/, '$1')), '..', '..', '..');
const [file, id] = process.argv.slice(2);
const k = key('elevenlabs');
if (!k) { console.log('falta a chave da ElevenLabs no Atelier'); process.exit(1); }
const credits = async () => { const r = await fetch('https://api.elevenlabs.io/v1/user/subscription', { headers: { 'xi-api-key': k.value } }); const j = await r.json(); return j.character_count; };
const before = await credits();
const fd = new FormData();
fd.append('file', new Blob([fs.readFileSync(file)], { type: 'audio/mpeg' }), path.basename(file));
fd.append('extract_composition_plan', 'music_v2_5');
const r = await fetch('https://api.elevenlabs.io/v1/music/upload', { method: 'POST', headers: { 'xi-api-key': k.value }, body: fd });
const txt = await r.text();
if (!r.ok) { console.log(`ElevenLabs ${r.status}: ${txt.slice(0, 600)}`); process.exit(2); }
const j = JSON.parse(txt);
const after = await credits();
const rec = { id, source: path.relative(ROOT, path.resolve(file)).replace(/\\/g, '/'), uploaded_at: new Date().toISOString(), credits_spent: after - before, response: j };
const out = path.join(ROOT, 'docs', 'trilha', 'receitas', `${id}.upload.json`);
fs.writeFileSync(out, JSON.stringify(rec, null, 1));
console.log(`${id}: song_id ${j.song_id}, ${rec.credits_spent} créditos; o plano lido: ${j.composition_plan ? (j.composition_plan.chunks || j.composition_plan.sections || []).length + ' trechos' : 'nenhum'}`);
