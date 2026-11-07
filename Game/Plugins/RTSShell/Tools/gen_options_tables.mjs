// As tabelas das opções do mockup (PRESETS, GFX, SND, CTL, GAMEO, ACC, KEYS do Resources/UI/screens.html) em JSON, pro
// gen_options_tables.py gerar o bloco do SRTSOptions::Construct. Uso: node gen_options_tables.mjs > options_tables.json
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
const s = readFileSync(join(dirname(fileURLToPath(import.meta.url)), '..', 'Resources', 'UI', 'screens.html'), 'utf8');
const pick = n => { const i = s.indexOf(`const ${n} = [`); let d = 0, j = s.indexOf('[', i); const a = j; for (; j < s.length; j++) { if (s[j] === '[') d++; else if (s[j] === ']') { d--; if (!d) break; } } return eval(s.slice(a, j + 1)); };
const out = {}; for (const n of ['PRESETS', 'GFX', 'SND', 'CTL', 'GAMEO', 'ACC', 'KEYS']) out[n] = pick(n);
console.log(JSON.stringify(out));
