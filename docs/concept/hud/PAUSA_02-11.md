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
- **A v2 (03/11, retomada na outra conta): conferida, corrigida e pronta pra ele revisar** (não vista por ele ainda):
  `captures/concept/hud/recursos2/jogadores.html` (cópia em `docs/concept/hud/prototipo/jogadores.html`; servidor "hud-concept", porta 8133).
  - O topo: a barra do duelo com os painéis alongados (só a faixa lisa estica, os enfeites ficam); você no anel; o seu time à esquerda;
    os inimigos em grupos de fichas à direita (16 px entre times); no anel da direita, quantos inimigos vivos; no todos contra todos, o seu
    nome no painel da esquerda. Sem lista lateral.
  - As superarmas: sem caixa, o nome na cor do dono + o tempo, os dois-pontos numa coluna, contorno preto; uma linha por jogador, no máximo 4
    e "+N more · TAB"; sempre logo abaixo da barra do topo, à direita (com 16 jogadores a barra chegava no canto).
  - O placar (Tab): 7 colunas, linhas de 36 px, o seu time primeiro, os outros por pontuação e os derrotados por último; respiro entre times;
    a economia escondida = um olho fechado; o espionado com o olho ao lado do nome e os números em laranja; o exército sempre em "k".
  - As imagens da revisão: `captures/concept/hud/faccoes/jogadores_v2_*.jpg`.
- **03/11, a v3 (revisão dele da v2):** a barra sempre simétrica (dois times: os dois painéis do comprimento do maior; 3+ times e todos
  contra todos: vidro neutro e as fichas divididas por igual nos dois lados, o seu time colado em você com o traço azul); as superarmas em
  medalhões (o desenho da arma, o anel na cor do dono que enche, o tempo numa plaquinha; pronta = vermelha pulsando); o placar com cada
  linha na cor do jogador (faixas arredondadas, "como as pastas do Mac"). A v2 da barra desigual e da lista em texto: reprovadas (APROVADOS 03/11).
- **Próximo passo:** ele revisar a v3. Aprovada, levar pro HUD (`RTSHudPainter`: trocar o `DrawTopMany` v1 pela barra alongada, a lista
  das superarmas e o placar), registrar em `APROVADOS.md` e commitar.

## Depois disso
- Levar o HUD aprovado pro jogo (`Game/Plugins/RTSHud`, o HUD novo atrás de -NewHud/F4) só quando os jogadores/placar forem aprovados.
- Os 16 ícones da China na laca já estão no kit (`kit/icons/china`), ainda não vistos por ele.
- A arte do brasão sem os tambores (USA, GLA, Rússia) entra junto com a ida pro jogo.

## 03/11: os jogadores e o placar APROVADOS (no protótipo)
- A barra do topo simétrica (duelo e dois times: o seu painel na sua cor, o outro do mesmo comprimento; 3+ times e todos contra todos:
  vidro neutro, as fichas divididas por igual, o seu time colado em você com o traço na sua cor).
- As superarmas na tela de jogo: a C no canto de cima à direita (até 8 linhas, "N READY", "×N"); no Tab, a coluna de todas na lista corrida
  com as plaquinhas de prontas e de carga, agrupada por jogador e tipo, rolando dentro do placar.
- O placar com a cor de cada jogador em faixas.
- **Próximo passo:** levar pro HUD em C++ (`RTSHudPainter`: a barra dos times no lugar do `DrawTopMany` v1, a lista C das superarmas, o placar
  no Tab com a coluna), provar no HUDLab com fotos das 4 facções e dos formatos, e só então pro jogo (`Game/Plugins/RTSHud`).

## 03/11, noite: o C++ começado (parou no limite de uso)
- Feito no HUDLab (compila, sem commit): `RTSHudPainterPlayers.cpp` (a barra dos times `DrawTopTeams`, a lista C `DrawSuperweaponList`, o placar
  `DrawScoreboard` com a coluna das superarmas), os tipos novos em `RTSHudTypes.h` (superarma com tipo/segundos/desenho, jogador com time e
  números do placar, `bScoreboard`), os formatos do protótipo em `HudLabStates.cpp` (`ApplyHudLabFormat`, mesmos números) e
  `hudlab.ps1 shot -Format 3v5v8 -Tab -Abuse -Me -Scroll`. Desenhos novos no kit: `skin/sw_*.png`, `eye_hidden`, `bolt`, `gen_frame/gen_bg`.
- Provado: o placar da GLA 3 × 5 × 8 com abuso (`captures/hudlab/state_2_ptab.png`) sai igual ao protótipo aprovado.
- 03/11, depois: as fotos todas feitas e iguais ao protótipo (lado a lado em `captures/hudlab/ab_*.png`, as 4 facções em
  `folha_topo_faccoes.jpg` e `folha_placar_faccoes.jpg`); os anéis finos com a beira suave (`SoftArc`, o Canvas não suaviza triângulo).
  Mostrado a ele; falta o "aprovado" e depois levar pro jogo (Game/Plugins/RTSHud, a rodinha rolando a coluna, o Tab segurado).

## 03/11, 16h: PAUSA (limite de tokens), no meio de levar pro jogo
- Aprovado e commitado: o HUD dos jogadores no HUDLab (`be8a4c9`, com push).
- **No jogo, SEM commit (compilado e passando):** `Game/Plugins/RTSHud` = cópia do `HUDLab/Plugins/RTSHud` (a fonte única; juntei nele o
  relógio da Drop Zone e o botão do trabalhador que só o jogo tinha); `RTSHUDNew.cpp` preenche o time, a patente, as contas, a pele da
  facção e o Tab segurado (`-RTSTab` abre o placar só no quadro da foto do teste); `RTSMatchSubsystem` conta por jogador pelos eventos
  (`StatsOf`, `IncomeLastMinute`; a pontuação do ZH). Sem superarma, a lista C some e o placar fica sem a coluna (ele: "não mostra nada").
  Também sem commit no HUDLab: a coluna vazia (`bSide`) e `hudlab.ps1 shot -NoSw`.
- Provado no jogo (03/11, à noite): o teste base inteiro com o HUD novo, 0 falhas (`hud_novo_base`), e com o placar nas fotos, 0 falhas
  (`captures/game/hud_jogadores_tab3/`: na batalha, 4/11 contra 11/4, a renda e a pontuação andando). O placar da foto do teste não
  segura o clique (`bScoreboardPassive`); com o Tab segurado, segura. Mostrado a ele; falta o "aprovado" pro commit do jogo.
- **Falta:** ver a barra dos times no jogo (o teste da sala com times quebra no passo `lan_times`, de outra sessão; ou um skirmish com
  mais IAs à mão); as superarmas na simulação; a rodinha rolando a coluna (mexe no controle, que outra sessão edita); o commit do jogo
  quando ele aprovar no jogo.
