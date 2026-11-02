// Configurações do Atelier: por onde o GPT sai (conta do ChatGPT pelo Codex, ou chave da API) e as
// chaves. Ficam em %APPDATA%\Atelier\settings.json, fora do repositório; a tela nunca recebe a chave
// inteira de volta, só os 4 últimos caracteres.
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

// a pasta vem da pasta do usuário, não só do APPDATA: o serviço às vezes é ligado pela ponte MCP de uma
// sessão, com um ambiente enxuto sem APPDATA, e aí as chaves "sumiam" (02/11)
export const SETTINGS_DIR = path.join(process.env.APPDATA || path.join(os.homedir(), 'AppData', 'Roaming'), 'Atelier');
const DIR = SETTINGS_DIR;
const FILE = path.join(DIR, 'settings.json');
const LEGACY = path.join(DIR, 'keys.json');
const ENV = { gemini: ['GEMINI_API_KEY', 'GOOGLE_API_KEY'], openai: ['OPENAI_API_KEY'], elevenlabs: ['ELEVENLABS_API_KEY', 'XI_API_KEY'] };

let cache = null, readAt = 0;
export function readSettings() {
  if (cache && Date.now() - readAt < 3000) return cache;
  let s = {};
  try { s = JSON.parse(fs.readFileSync(FILE, 'utf8')); } catch {}
  if (!s.keys) { try { s.keys = JSON.parse(fs.readFileSync(LEGACY, 'utf8')); } catch { s.keys = {}; } }
  s.gptVia = s.gptVia === 'api' ? 'api' : 'codex';
  cache = s; readAt = Date.now();
  return s;
}

export function writeSettings(patch) {
  const s = JSON.parse(JSON.stringify(readSettings()));
  if (patch.gptVia) s.gptVia = patch.gptVia === 'api' ? 'api' : 'codex';
  for (const [k, v] of Object.entries(patch.keys || {})) {
    if (!(k in ENV)) continue;
    if (v === null) delete s.keys[k];          // apagar
    else if (String(v).trim()) s.keys[k] = String(v).trim(); // vazio = manter a que está
  }
  fs.mkdirSync(DIR, { recursive: true });
  fs.writeFileSync(FILE, JSON.stringify(s, null, 1));
  cache = s; readAt = Date.now();
  return s;
}

// A da tela vale mais que a variável de ambiente.
export function key(name) {
  const s = readSettings();
  if (s.keys[name]) return { value: s.keys[name], from: 'file' };
  for (const v of ENV[name] || []) if (process.env[v]) return { value: process.env[v], from: 'env', env: v };
  return null;
}

export function publicKeys() {
  const out = {};
  for (const name of Object.keys(ENV)) {
    const k = key(name);
    out[name] = k ? { set: true, tail: k.value.slice(-4), from: k.from, env: k.env || null } : { set: false };
  }
  return out;
}
