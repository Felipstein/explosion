// ---------------------------------------------------------------------------
// settings.js — configuração gráfica: esquema, presets e persistência.
//
// Num RTS a mesma cena roda em máquinas muito diferentes e com dezenas de
// explosões por vez, então cada custo relevante do pipeline vira uma opção
// com nome de jogador, não de engenheiro. Os presets são só pontos de partida:
// mexer em qualquer opção vira "personalizado".
//
// `apply` diz o que a mudança exige:
//   live     — vale no próximo frame
//   rebuild  — recria pool/cena/volume (troca de grade da simulação ao vivo)
//   rebake   — troca a resolução da sequência assada (carrega outro asset)
// ---------------------------------------------------------------------------

const KEY = 'explosao.settings.v1';

// VRAM da sequência assada: RGBA8 + macro R8 (res/4) + combustível R8 (res/2),
// 56 quadros × 3 variantes
export function bakeVramMB(res) {
  const layers = 56 * 3;
  const vox = res ** 3 * 4 + (res >> 2) ** 3 + (res >> 1) ** 3;
  return Math.round((vox * layers) / (1024 * 1024));
}

export const SCHEMA = [
  { group: 'Explosões', items: [
    { id: 'bakeRes', label: 'Detalhe das explosões', apply: 'rebake',
      hint: 'resolução da sequência assada que as explosões do clique tocam',
      options: [[48, 'baixo'], [64, 'médio'], [80, 'alto'], [96, 'ultra']],
      note: (v) => `${bakeVramMB(v)} MB de vídeo` },
    { id: 'lightCache', label: 'Sombra interna da fumaça', apply: 'live',
      hint: 'quantas explosões ganham volume de luz próprio por frame (as maiores na tela)',
      options: [[0, 'off'], [2, '2'], [4, '4'], [6, '6'], [8, '8']] },
    { id: 'msOctaves', label: 'Espalhamento de luz', apply: 'live',
      hint: 'oitavas de espalhamento múltiplo na fumaça',
      options: [[1, 'simples'], [2, 'duplo'], [3, 'completo']] },
    { id: 'instSteps', label: 'Precisão do volume', apply: 'live',
      hint: 'passos de raymarch das explosões do clique (o LOD reduz nas pequenas)',
      options: [[32, 'baixa'], [44, 'média'], [56, 'alta'], [72, 'ultra']] },
    { id: 'sparks', label: 'Faíscas', apply: 'live',
      options: [[0, 'off'], [0.25, '25%'], [0.5, '50%'], [1, '100%']] },
    { id: 'liveSim', label: 'Simulação ao vivo (espaço)', apply: 'rebuild',
      hint: 'grade do solver de fluido que roda em tempo real',
      options: [[64, '64³'], [96, '96³'], [128, '128³'], [160, '160³']] },
  ] },
  { group: 'Render', items: [
    { id: 'renderScale', label: 'Resolução de render', apply: 'live',
      options: [[0.5, '50%'], [0.67, '67%'], [0.8, '80%'], [0.88, '88%'], [1, '100%']] },
    { id: 'volScale', label: 'Resolução do volume', apply: 'live',
      hint: 'relativa à de render; o volume é reconstruído por upsample',
      options: [[0.5, '50%'], [0.6, '60%'], [0.72, '72%'], [1, '100%']] },
    { id: 'dynamicRes', label: 'Resolução dinâmica', apply: 'live',
      hint: 'baixa a resolução sozinha quando o frame passa da meta',
      options: [[true, 'on'], [false, 'off']] },
    { id: 'targetFps', label: 'Meta de FPS', apply: 'live',
      options: [[30, '30'], [45, '45'], [60, '60']] },
  ] },
  { group: 'Iluminação', items: [
    { id: 'ao', label: 'Oclusão de ambiente', apply: 'live',
      options: [[0, 'off'], [0.55, 'baixa'], [0.8, 'média'], [1, 'alta']] },
    { id: 'instShadows', label: 'Sombra das explosões no chão', apply: 'live',
      hint: 'quantas explosões do clique projetam sombra volumétrica',
      options: [[0, 'off'], [2, '2'], [4, '4']] },
    { id: 'smokeBlocksLight', label: 'Fumaça bloqueia luz de outras', apply: 'live',
      hint: 'a fumaça de uma explosão faz sombra na luz das vizinhas (caro: luzes × explosões por pixel)',
      options: [[false, 'off'], [true, 'on']] },
    { id: 'instLights', label: 'Luzes de explosão', apply: 'live',
      hint: 'quantas explosões do clique iluminam a cena ao mesmo tempo',
      options: [[2, '2'], [4, '4'], [8, '8']] },
    { id: 'lightGain', label: 'Brilho das explosões', apply: 'live',
      options: [[0.6, '60%'], [0.8, '80%'], [1, '100%'], [1.3, '130%'], [1.7, '170%']] },
  ] },
  { group: 'Pós-processo', items: [
    { id: 'bloom', label: 'Bloom', apply: 'live', options: [[true, 'on'], [false, 'off']] },
    { id: 'grain', label: 'Granulação de filme', apply: 'live', options: [[true, 'on'], [false, 'off']] },
    { id: 'chromatic', label: 'Aberração cromática', apply: 'live', options: [[true, 'on'], [false, 'off']] },
  ] },
];

export const ITEMS = Object.fromEntries(SCHEMA.flatMap((g) => g.items).map((it) => [it.id, it]));

export const PRESETS = {
  baixa: {
    bakeRes: 48, lightCache: 0, msOctaves: 1, instSteps: 32, sparks: 0.25, liveSim: 64,
    renderScale: 0.67, volScale: 0.5, dynamicRes: true, targetFps: 60,
    ao: 0, instShadows: 0, smokeBlocksLight: false, instLights: 4, lightGain: 1,
    bloom: true, grain: false, chromatic: false,
  },
  media: {
    bakeRes: 64, lightCache: 2, msOctaves: 2, instSteps: 44, sparks: 0.5, liveSim: 96,
    renderScale: 0.8, volScale: 0.6, dynamicRes: true, targetFps: 60,
    ao: 0.55, instShadows: 2, smokeBlocksLight: false, instLights: 4, lightGain: 1,
    bloom: true, grain: true, chromatic: true,
  },
  alta: {
    bakeRes: 64, lightCache: 6, msOctaves: 3, instSteps: 56, sparks: 1, liveSim: 128,
    renderScale: 0.88, volScale: 0.72, dynamicRes: true, targetFps: 60,
    ao: 0.8, instShadows: 4, smokeBlocksLight: false, instLights: 8, lightGain: 1,
    bloom: true, grain: true, chromatic: true,
  },
  ultra: {
    bakeRes: 96, lightCache: 8, msOctaves: 3, instSteps: 72, sparks: 1, liveSim: 160,
    renderScale: 1, volScale: 1, dynamicRes: true, targetFps: 60,
    ao: 1, instShadows: 4, smokeBlocksLight: true, instLights: 8, lightGain: 1,
    bloom: true, grain: true, chromatic: true,
  },
};
export const PRESET_NAMES = { baixa: 'baixa', media: 'média', alta: 'alta', ultra: 'ultra' };

/** qual preset bate exatamente com os valores, ou null (= personalizado) */
export function matchPreset(values) {
  for (const [name, p] of Object.entries(PRESETS)) {
    if (Object.keys(p).every((k) => p[k] === values[k])) return name;
  }
  return null;
}

/**
 * Lê do navegador. localStorage pode lançar (janela privada, site bloqueado)
 * ou vir vazio; nos dois casos vale o preset alta.
 */
export function loadSettings() {
  const base = { ...PRESETS.alta };
  try {
    const raw = localStorage.getItem(KEY);
    if (!raw) return base;
    const saved = JSON.parse(raw);
    for (const [k, v] of Object.entries(saved || {})) {
      const it = ITEMS[k];
      // só aceita valores que existem no esquema atual
      if (it && it.options.some(([ov]) => ov === v)) base[k] = v;
    }
  } catch (e) { /* sem persistência: segue com o padrão */ }
  return base;
}

export function saveSettings(values) {
  try { localStorage.setItem(KEY, JSON.stringify(values)); } catch (e) { /* idem */ }
}
