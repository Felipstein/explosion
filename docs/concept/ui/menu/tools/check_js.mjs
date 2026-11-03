// Confere a sintaxe de todo <script> das páginas (o comentário no meio da linha já quebrou o menu duas vezes).
// Uso: node check_js.mjs <pasta>   -> sai 1 se alguma página não compila
import { readFileSync, readdirSync } from 'node:fs';
import { join } from 'node:path';
const dir = process.argv[2]; let bad = 0;
for (const f of readdirSync(dir).filter(f => f.endsWith('.html'))) {
  const html = readFileSync(join(dir, f), 'utf8');
  const re = /<script(?![^>]*\bsrc=)[^>]*>([\s\S]*?)<\/script>/g; let m, k = 0;
  while ((m = re.exec(html))) { k++; try { new Function(m[1]); } catch (e) { bad++; console.log(`${f} script ${k}: ${e.message}`); } }
}
for (const f of readdirSync(dir).filter(f => f.endsWith('.js'))) { try { new Function(readFileSync(join(dir, f), 'utf8')); } catch (e) { bad++; console.log(`${f}: ${e.message}`); } }
console.log(bad ? `FALHAS: ${bad}` : 'sintaxe ok'); process.exit(bad ? 1 : 0);
