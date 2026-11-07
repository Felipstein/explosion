// (06/11/2026, a trilha da partida, docs/TRILHA_DA_PARTIDA.md) Gera uma faixa na ElevenLabs Music (o /v1/music com o plano de composição
// do music_v2_5: os trechos com a duração, os estilos e as direções) pela chave do Atelier (Atelier/core/settings.mjs). A receita (o
// plano inteiro, o modelo, a semente, os créditos gastos, o song-id) fica em docs/trilha/receitas/<id>.json junto do mp3; o áudio vai pra
// captures/musica/gerado/<facção>/<id>.mp3 (o original; o pack tira dele a versão do jogo).
//   node Game/Tools/music/compose.mjs <plano.json>
// O plano.json: { "id": "russia_red_tide", "faction": "russia", "role": "battle", "model_id": "music_v2_5", "seed": 1,
//                 "composition_plan": { "chunks": [ { "text": "[Intro] {...}", "duration_ms": 12000, "positive_styles": [...], "negative_styles": [...] } ] } }
import fs from 'node:fs';
import path from 'node:path';
import { key } from '../../../Atelier/core/settings.mjs';

const ROOT = path.resolve(path.dirname(new URL(import.meta.url).pathname.replace(/^\/(\w:)/, '$1')), '..', '..', '..');
const planPath = process.argv[2];
const plan = JSON.parse(fs.readFileSync(planPath, 'utf8'));
const k = key('elevenlabs');
if (!k) { console.log('falta a chave da ElevenLabs no Atelier'); process.exit(1); }
const H = { 'xi-api-key': k.value };

async function credits() {
  const r = await fetch('https://api.elevenlabs.io/v1/user/subscription', { headers: H });
  const j = await r.json().catch(() => ({}));
  return { used: j.character_count, limit: j.character_limit };
}

const before = await credits();
const body = { model_id: plan.model_id || 'music_v2_5', composition_plan: plan.composition_plan };
if (plan.seed != null) body.seed = plan.seed;
const t0 = Date.now();
const r = await fetch('https://api.elevenlabs.io/v1/music?output_format=mp3_44100_192', {
  method: 'POST', headers: { ...H, 'content-type': 'application/json' }, body: JSON.stringify(body),
});
if (!r.ok) {
  console.log(`ElevenLabs ${r.status}: ${(await r.text()).slice(0, 600)}`);
  process.exit(2);
}
const buf = Buffer.from(await r.arrayBuffer());
const hdr = {};
for (const [hk, hv] of r.headers) if (!/auth|cookie|key/i.test(hk)) hdr[hk] = hv;
const outDir = path.join(ROOT, 'captures', 'musica', 'gerado', plan.faction);
fs.mkdirSync(outDir, { recursive: true });
const out = path.join(outDir, `${plan.id}.mp3`);
fs.writeFileSync(out, buf);
const after = await credits();
const recDir = path.join(ROOT, 'docs', 'trilha', 'receitas');
fs.mkdirSync(recDir, { recursive: true });
const total = plan.composition_plan.chunks.reduce((s, c) => s + c.duration_ms, 0);
const rec = { ...plan, generated_at: new Date().toISOString(), seconds_planned: total / 1000, seconds_wall: (Date.now() - t0) / 1000,
  credits_spent: (after.used ?? 0) - (before.used ?? 0), credits_left: (after.limit ?? 0) - (after.used ?? 0), response_headers: hdr,
  audio: path.relative(ROOT, out).replace(/\\/g, '/') };
fs.writeFileSync(path.join(recDir, `${plan.id}.json`), JSON.stringify(rec, null, 1));
console.log(`${plan.id}: ${(buf.length / 1024).toFixed(0)} KB, ${total / 1000} s planejados, ${rec.credits_spent} créditos (sobram ${rec.credits_left}), ${rec.seconds_wall.toFixed(0)} s`);
console.log('song-id:', hdr['song-id'] || hdr['x-song-id'] || '(nenhum)');
