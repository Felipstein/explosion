// Ponte MCP (stdio) do Atelier pras sessões do Claude Code. Cada sessão abre a sua ponte; todas
// falam com o mesmo serviço (atelier.mjs, porta 8140), que esta ponte liga se não estiver rodando.
// Protocolo MCP mínimo à mão (initialize, tools/list, tools/call), sem dependência.
import { spawn } from 'node:child_process';
import path from 'node:path';
import readline from 'node:readline';

const PORT = +(process.env.ATELIER_PORT || 8140);
const BASE = `http://127.0.0.1:${PORT}`;
const HERE = path.dirname(new URL(import.meta.url).pathname.replace(/^\/(\w:)/, '$1'));

async function api(method, route, body) {
  for (let i = 0; i < 2; i++) {
    try {
      const r = await fetch(BASE + route, { method, headers: { 'content-type': 'application/json' }, body: body ? JSON.stringify(body) : undefined });
      const j = await r.json();
      if (!r.ok) throw new Error(j.error || r.status);
      return j;
    } catch (e) {
      if (i || !/fetch failed|ECONNREFUSED/.test(String(e.cause && e.cause.code || e.message))) throw e;
      spawn(process.execPath, [path.join(HERE, 'atelier.mjs')], { detached: true, stdio: 'ignore', windowsHide: true }).unref();
      for (let k = 0; k < 30; k++) { await new Promise(r => setTimeout(r, 200)); try { await fetch(BASE + '/api/health'); break; } catch {} }
    }
  }
}

const MODELS = {
  type: 'object', description: 'Quantas imagens de cada modelo (0 a 4). GPT vai pela conta do ChatGPT. Ex.: {"gpt":1,"gemini_pro":1}',
  properties: { gpt: { type: 'integer' }, gemini_pro: { type: 'integer' }, gemini_flash: { type: 'integer' } },
};
const TOOLS = [
  { name: 'atelier_generate',
    description: 'Pede imagens ao GPT e/ou Gemini pelo Atelier, sem o Felipe copiar e colar. Os anexos são copiados pra pasta do pedido (_anexos/) na ordem dada: no prompt, "image 1" é o primeiro. O primeiro anexo é a raiz (âncora) se "root" não disser outro. Devolve o id; use atelier_wait pra receber os caminhos. Prompt completo, em inglês. Com "items" vira lote: o mesmo prompt pra cada item, trocando {item}.',
    inputSchema: { type: 'object', required: ['title', 'prompt', 'session'], properties: {
      title: { type: 'string', description: 'Nome curto do pedido, em português (ex.: "Comanche, ficha").' },
      prompt: { type: 'string', description: 'O prompt inteiro, pronto, em inglês.' },
      session: { type: 'string', description: 'Nome curto desta sessão, como ele a conhece (ex.: "HUD", "Sistema aéreo").' },
      attachments: { type: 'array', items: { type: 'string' }, description: 'Caminhos das imagens (absolutos ou a partir da raiz do repositório), na ordem.' },
      root: { type: 'integer', description: 'Índice (0 = primeiro) do anexo que é a raiz aprovada.' },
      models: MODELS,
      folder: { type: 'string', description: 'Pasta de saída (padrão: captures/concept/<próximo número>_<título>/).' },
      aspect: { type: 'string', description: '16:9 (padrão), 1:1, 3:2, 9:16.' },
      items: { type: 'array', items: { type: 'string' }, description: 'Lote: um item por imagem; o prompt usa {item}.' },
      branch: { type: 'string', description: 'Nome do primeiro ramo (padrão "Raiz").' },
    } } },
  { name: 'atelier_wait',
    description: 'Espera o pedido ficar pronto (until="done") ou o Felipe escolher/aprovar (until="pick") e devolve o estado com os caminhos das imagens, as escolhidas e o que ele disse. Se o tempo acabar, chame de novo.',
    inputSchema: { type: 'object', required: ['id'], properties: { id: { type: 'string' }, until: { type: 'string', enum: ['done', 'pick'] }, seconds: { type: 'integer', description: 'Teto de espera, padrão 100, máx. 280.' } } } },
  { name: 'atelier_status', description: 'Estado atual de um pedido: cada imagem, o caminho, falhas, escolhidas e o que ele disse.',
    inputSchema: { type: 'object', required: ['id'], properties: { id: { type: 'string' } } } },
  { name: 'atelier_round', description: 'Nova rodada num ramo do pedido. Com "prompt" (inteiro, em inglês) o ramo passa a usar esse prompt; com "models" muda quem gera só nesta rodada.',
    inputSchema: { type: 'object', required: ['id'], properties: { id: { type: 'string' }, branch: { type: 'string', description: 'Nome do ramo (padrão: o último).' }, prompt: { type: 'string' }, models: MODELS } } },
  { name: 'atelier_branch', description: 'Abre um ramo novo no pedido: outra visão com prompt próprio e os modelos que quiser (ex.: dois Gemini).',
    inputSchema: { type: 'object', required: ['id', 'name'], properties: { id: { type: 'string' }, name: { type: 'string' }, prompt: { type: 'string', description: 'Prompt inteiro do ramo, em inglês (padrão: o da raiz).' }, models: MODELS } } },
  { name: 'atelier_list', description: 'Lista os pedidos do Atelier com o estado de cada um.',
    inputSchema: { type: 'object', properties: { open_only: { type: 'boolean' } } } },
];

async function call(name, a) {
  const text = async id => (await api('GET', `/api/request?id=${encodeURIComponent(id)}&text=1`)).text;
  switch (name) {
    case 'atelier_generate': {
      const r = await api('POST', '/api/requests', a);
      return `Pedido criado: id ${r.id}, pasta ${r.folder}. Use atelier_wait com esse id.\n\n` + await text(r.id);
    }
    case 'atelier_wait': {
      const secs = Math.min(a.seconds || 100, 280);
      const r = await api('GET', `/api/wait?id=${encodeURIComponent(a.id)}&until=${a.until || 'done'}&timeout=${secs}`);
      return (r.timedOut ? 'Ainda não terminou; chame de novo.\n\n' : '') + r.text;
    }
    case 'atelier_status': return text(a.id);
    case 'atelier_round': await api('POST', '/api/round', a); return text(a.id);
    case 'atelier_branch': await api('POST', '/api/branch', a); return text(a.id);
    case 'atelier_list': {
      const list = await api('GET', '/api/requests');
      const st = { gen: 'gerando', wait: 'esperando escolha', closed: 'fechado' };
      return list.filter(r => !a.open_only || r.status !== 'closed').map(r => `${r.id} · ${r.title} · ${r.src} · ${st[r.status]} · ${r.folder}`).join('\n') || 'Nenhum pedido.';
    }
  }
  throw new Error('ferramenta desconhecida: ' + name);
}

const out = m => process.stdout.write(JSON.stringify(m) + '\n');
readline.createInterface({ input: process.stdin }).on('line', async line => {
  let m; try { m = JSON.parse(line); } catch { return; }
  if (m.id === undefined) return; // notificações
  try {
    if (m.method === 'initialize') return out({ jsonrpc: '2.0', id: m.id, result: { protocolVersion: (m.params && m.params.protocolVersion) || '2025-06-18', capabilities: { tools: {} }, serverInfo: { name: 'atelier', version: '0.1.0' } } });
    if (m.method === 'ping') return out({ jsonrpc: '2.0', id: m.id, result: {} });
    if (m.method === 'tools/list') return out({ jsonrpc: '2.0', id: m.id, result: { tools: TOOLS } });
    if (m.method === 'tools/call') {
      try { return out({ jsonrpc: '2.0', id: m.id, result: { content: [{ type: 'text', text: await call(m.params.name, m.params.arguments || {}) }] } }); }
      catch (e) { return out({ jsonrpc: '2.0', id: m.id, result: { isError: true, content: [{ type: 'text', text: 'Atelier: ' + (e.message || e) }] } }); }
    }
    out({ jsonrpc: '2.0', id: m.id, error: { code: -32601, message: 'método não existe: ' + m.method } });
  } catch (e) { out({ jsonrpc: '2.0', id: m.id, error: { code: -32603, message: String(e.message || e) } }); }
});
