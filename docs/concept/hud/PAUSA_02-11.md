# HUD: onde parou (pausa de 02/11, fim do limite de tokens)

Sessão do HUD. Ao retomar: ler isto, `FACCOES.md` (§5–8), `DECISOES.md` (§7–9) e a seção HUD do `docs/APROVADOS.md`.

## Aprovado e no GitHub
- O HUD das 4 facções (USA, China laca, GLA, Rússia), o topo 1x1 na cor de cada jogador com a silhueta, o dinheiro no mostrador,
  a energia verde/amarela/vermelha (GLA sem usina = sem barra), as abas dos grupos em vetor, a tela de poderes (moldura por facção,
  conteúdo igual nas 4), as patentes por facção (nomes, ícones e medalhas), os emblemas novos (China 5 estrelas, Rússia estrela em cima).
- Protótipo: `docs/concept/hud/prototipo/index.html` (o original segue em `captures/concept/hud/recursos2/console_vivo.html`).

## Em andamento: os jogadores conforme o formato da partida
- **Aprovado como ideia:** o HUD muda com o formato (duelo = topo 1x1; dois times = o 1x1 com as fichas dos times; 3+ times e todos
  contra todos = outro arranjo) e o placar no Tab.
- **Reprovado (v1):** o topo de 8 emendando 8 anéis ("sujo"); o placar v1, a lista lateral, as caixinhas das superarmas e o relógio
  recortado (`APROVADOS.md`, 02/11). O código do topo de 8 v1 (`RTSHudPainter::DrawTopMany`, `ImageUV`) está no HUDLab SEM commit:
  é o reprovado, refazer ou tirar.
- **O estudo** (agente, 02/11): `JOGADORES.md` (as regras com números e as referências).
- **A v2 (feita, NÃO mostrada a ele ainda):** `captures/concept/hud/recursos2/jogadores.html` (cópia em `docs/concept/hud/prototipo/jogadores.html`):
  sem lista lateral; a barra do duelo com os painéis alongados (só a faixa lisa estica) e as fichas dos times (inimigos em grupos);
  superarmas sem caixa, nome na cor do dono + tempo, canto superior direito; placar limpo de 7 colunas, linhas de 36 px, respiro entre
  times, economia escondida = um olho fechado. Falta: conferir as fotos (`shots_fac/j2_*.png`, tiradas antes da pausa), corrigir o que
  estiver errado e mostrar a ele. Servidor do protótipo: `.claude/launch.json` "hud-concept" (porta 8133).

## Depois disso
- Levar o HUD aprovado pro jogo (`Game/Plugins/RTSHud`, o HUD novo atrás de -NewHud/F4) só quando os jogadores/placar forem aprovados.
- Os 16 ícones da China na laca já estão no kit (`kit/icons/china`), ainda não vistos por ele.
- A arte do brasão sem os tambores (USA, GLA, Rússia) entra junto com a ida pro jogo.
