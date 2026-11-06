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

## 04/11, madrugada: o console aprovado em C++ e no jogo
- No GitHub: o console no HUDLab e a moldura do painel de cada facção (`a49fc27`; a da GLA aprovada: "o GLA ficou certo").
- No jogo, SEM commit: o RTSHud do jogo = o do HUDLab (o console no lugar do painel do dinheiro e das abas de antes) e, no
  `RTSHUDNew.cpp`, as plaquetas no lugar dos avisos de energia baixa e de promoção. Teste base com `-NewHud`: 0 falhas (`hud_console`).
- Depois, também sem commit, os pedidos dele: as células de energia dentro do canal da arte de cada facção ("os bloquinhos um pouco
  maiores do que a barra"); sem seleção, a barra de energia termina antes do tubo e o tubo desce até a mesma linha ("mal encaixado");
  o tubo acompanha o painel de 9 colunas; o LOW POWER só quando gasta mais do que gera (o `hasSufficientPower` do ZH; 0 / 0 sem aviso,
  a produção mais lenta continua na simulação, a decisão de 10/10). Na fila: compilar e o teste em 1920×1080 (`hud_console_1080`).
- Ele: "no protótipo tudo tem mais qualidade". Ampliado 3×, o console bate pixel a pixel com o protótipo; as fotos do jogo eram de
  1600×900 (o HUD reduzido). Próximo: comparar a foto do jogo em 1920×1080 com o protótipo, peça por peça, e caçar o que ainda perde.
- Falta: o commit do jogo (com o aprovado dele), a tela de poderes do general em C++, o "INSUFFICIENT FUNDS" no jogo (o clique sem
  dinheiro), o escurecer do HUD sem energia (DECISOES §7), o hover que sobe e acende (§7) e o chat (§7).
- 04/11, 5h: no jogo (compilado, teste 1920×1080 com 0 falhas, `captures/game/hud_console_1080b`, os detalhes ampliados em
  `captures/game/hud_console_detalhes.png`): a energia no canal da arte de cada facção, lida na régua (a China estava deslocada pra
  baixo), começando logo depois da ponta; a seta da patente no metal do tubo; a luz das células só nas células, acendendo e apagando.
  Tudo SEM commit: espera ele ver no jogo.

## 04/11, tarde: PAUSA (limite de uso) no meio dos itens 1–8 do HUD
- Feito: a tela do general em C++ (HUDLab e jogo; DrawGeneral, RTSHudGeneral.cpp, docs/poderes/tela/export_game.py), igual ao protótipo nas
  4 facções; no jogo o cubo abre, clicar compra (local, `ARTSHUD::GeneralOwned`), X/Esc/fora fecha. Combinado com a sessão "pendências/jogo"
  (local_03dc2ce1): ela faz a simulação (BuyPower, pontos, recargas, efeitos) e expõe GeneralPointsLeft/IsPowerOwned/CanBuyPower/
  OrderBuyPower/OwnedPowers; eu ligo a tela e a barra de poderes (> 4 casas, teclas) quando ela avisar.
- ATENÇÃO: a compilação do jogo às 10:44 falhou no código dela (em edição) e o RTSHud.dll foi refeito com o FRTSHudState novo, com o
  RTSGame.dll velho. Os Cursores compilavam às 10:55: conferir se passou (UnrealEditor-RTSGame.dll mais novo que o RTSHud.dll).
- Item 1 (hover aprovado) no HUDLab, compila: RTSHudPainterHover.cpp (HoverXform/HoverFx/GridFill, DrawTint, State.HoverAge/PressAge),
  nas casas, na coluna de poderes e nas abas. As fotos (captures/hudlab/hover_*) rodavam em segundo plano: conferir ampliado. Falta levar
  pro jogo (RTSHUDNew.cpp: medir o HoverAge/PressAge quando o HoverId muda).
- Item 2 (chat) escrito e NÃO ligado: Private/RTSHudPainterChat.cpp.pendente (renomeado pra não quebrar a compilação). Falta: os tipos
  FRTSHudChatLine/FRTSHudChat em RTSHudTypes.h (Who, Text, Color, bAllies, bSystem, Age; bTyping, bAllies, Draft, TypingAge, PingAge),
  DrawChat no .h e no Draw, o ping no botão mm_chat, o estado de teste no laboratório, e no jogo: preencher do URTSNetSession::ChatLines
  + as derrotas como linha do sistema, e não chamar o DrawChat antigo nem o letreiro de derrota com o HUD novo.
- Itens 3–8 não começados: o cubo com os pontos, INSUFFICIENT FUNDS, sem energia (tranco/escurecer), o dinheiro animado, a barra de 3+
  no jogo, a rodinha do placar. Nada disso commitado.

## 04/11, 15h30: PAUSA de novo (limite semanal perto)
- Conferido: o RTSGame.dll do jogo é das 11:09 (mais novo que o RTSHud.dll das 10:46): o jogo está consistente.
- Item 1 (hover): as fotos das 4 facções refeitas com prazo maior (script no scratchpad hover_shots.ps1; saem em captures/hudlab/hover_*).
  Falta conferir ampliado contra o protótipo e levar pro jogo.
- Item 2 (chat): LIGADO no HUDLab, AINDA NÃO COMPILADO. RTSHudPainterChat.cpp reescrito com as curvas do protótipo (clIn
  cubic-bezier(.2,1.2,.4,1), o brilho 2,2→1,3→1, a saída em ease 1,2 s, o gap 8, no máximo 6 linhas, o cursor em 4 degraus);
  FRTSHudChatLine/FRTSHudChat + State.Chat no RTSHudTypes.h; DrawChat no Draw (depois de DrawPowers); o Bezier do CSS virou
  FRTSHudPainter::Bezier (static, no Hover.cpp); o ping no botão mm_chat (DrawMinimap). Laboratório: -Chat (a demo do protótipo em 16 s;
  foto com -Time=7). Próximo: compilar o HUDLab, foto -Chat -Time=7 e -Time=4 (o ping) e um vídeo; depois o jogo.
- Item 3 (o cubo com os pontos), estudado, nada escrito. O que o aprovado tem (foto docs/aprovados/hud/2026-11-01_plaqueta_promoted_enxuta_APROVADA.jpg,
  protótipo v12/v20 e #hub .pts) e o C++ ainda não:
  - a moeda skin:token_gold 31×27 em (cubo -27, +13) com o número (16 px bold #2a1d05) e a luz 6 px dourada;
  - o aro dourado borrado no cubo (r 20,6, traço 1,6, blur 2,5) e o rglow (1 s: brilho 1,35 + luz dourada 8 px), enquanto sobrar ponto;
  - a plaqueta: 39 px de alto (padding 6/18/6/8), topo 56 acima do cubo, a ponta dourada 16×9 com o miolo escuro 12×6, a entrada
    promoIn (.38 s, cubic-bezier(.2,1.4,.4,1), sobe 20 e estica de .35 pela base) e o promoGlow (1,4 s: aro #f6c850, luz 26 px .65);
    FIRST POWER sem texto na patente 1; some quando abre a tela do general (o cubo continua brilhando);
  - o hover próprio do cubo (scale 1.08, brilho 1.25, luz azul 8 px), não o das casas.
  No jogo: S.Promote = pontos que sobram (a árvore carregada sempre), a plaqueta até abrir a tela (GeneralSeenRank), a idade pela troca de patente.
- Itens 4–8 como antes. Nada commitado.
- As fotos do hover (hover_shots.ps1) falharam de novo nas 4 (só a usa 1.0 tinha saído antes, com -HoverAge=1.0): investigar antes de repetir (o log do HUDLab; talvez trave/caia com HoverAge < 1).

## 05/11, noite: os itens 1–8 feitos no HUDLab, o jogo na fila
- Itens 1–6 e 8 no HUDLab, compilados e fotografados (captures/hudlab/c_*.png e z_*.png, conferidos ampliados):
  - hover das 4 facções (RTSHudPainterHover.cpp); o jogo mede o HoverAge/PressAge pela troca do botão;
  - chat (RTSHudPainterChat.cpp, FRTSHudChat; o ping no botão do chat; laboratório -Chat -Time=7);
  - cubo com os pontos: a moeda, o aro dourado borrado, o rglow, a plaqueta presa pela ponta (39 px, promoIn/promoGlow), FIRST POWER na
    patente 1, o hover próprio do cubo (laboratório: estado 4, -FirstPower, -Time=0.5 = o pico);
  - clique sem dinheiro: a casa pisca (hflash), o brasão treme, a plaqueta INSUFFICIENT FUNDS some em 2,2 s (PlateLife), a dica sobe
    acima da plaqueta (o showTip do protótipo);
  - sem energia: o apagão de 12 s (BrownAge; o preto a 58% nos 4 pedaços do protótipo), as células piscando, o raio na casa em produção
    (laboratório -Brown -BrownAge=6);
  - o dinheiro animado: "+$" voando pela curva, "−$" caindo, o número rolando (laboratório -MoneyDemo -Time=0.6 / 2.9);
  - a rodinha do placar: FRTSHudPainter::LastSwMaxScroll; no jogo, com o Tab apertado, a rodinha rola e não dá zoom.
- Item 7: a cena hud_completo_4p (4 jogadores, cada um contra todos) no Scenarios.json.
- O jogo: o patch inteiro em scratchpad/patch_game_hud.py (RTSHUD.h, RTSHUDNew.cpp, RTSHUD.cpp, RTSHUDEconomy.cpp, RTSPlayerController.cpp),
  conferido no espelho (cc_check); scratchpad/game_hud.ps1 entra na fila, confere o patch num espelho, copia o plugin do HUDLab, aplica,
  compila e roda as cenas hud_completo_{usa,china,gla,russia,4p} (pasta hud_completo5_*). Rodando desde 05/11 19h20.
- A trava das fotos do HUDLab desde 04/11 15h: a varredura do registro sem cache (271 s com os Blenders); -AssetGatherAll=false no
  hudlab.ps1 e nas fotos (a memória das armadilhas tem o caso).
- Falta: as fotos no jogo (todas as facções + 4p), conferidas ampliadas, e mandar a folha pra ele; ligar a tela do general e a barra
  na API da sessão do jogo quando ela avisar (GeneralPointsLeft, FindPower, IsPowerOwned, BuyPowerRefusal, OrderBuyPower, PowerBought).
  Nada commitado.

## 06/11, 01h20: o estado
- No jogo (compilado 05/11 23:15, a rodada 3: o HUD novo + a tela do general na simulação + as casas limpas sem as vazias + o hover
  contido + o clique sem dinheiro só em casa destrancada + as opções [RTS] HudCleanSlots/HudHideEmptySlots, que a sessão das Telas pôs
  na aba GAME): as cenas hud_completo7_{usa,china} conferidas ampliadas (certas); GLA, Rússia e 4p na fila (scratchpad/game_hud3.ps1,
  para se a cena cair). Folha: scratchpad/folha_hud.py <pasta> <saída>.
- Escolhido por ele (APROVADOS 05/11): as casas limpas + sem as vazias; a fila do cartão com os espaços; o hover das casas contido.
- Em teste: a tela do general B (DECISOES, a última entrada; laboratório -General -GenB; fotos captures/hudlab/gen_{a,b}_<facção>.png).
  No jogo continua a A até ele escolher.
- Nada commitado (ele quer ver todas as fotos dos HUDs no jogo antes).
- 06/11, 06h30: as 5 cenas no jogo saíram (hud_completo7_{usa,china,gla,russia,4p}), 0 falhas, conferidas ampliadas; folhas em
  captures/game/folha_hud7_*.jpg (mandadas pra ele). A barra de 4 jogadores segue o FFA aprovado (03/11). Falta: ele ver as folhas e a
  tela do general B; o commit do HUD (jogo + HUDLab + docs) só depois do ok dele.
- 06/11, 10h: a tela do general B APROVADA ("vai, bem melhor") e no jogo (compilado 09:37; G.bLegible = true no RTSHUDNew.cpp);
  conferida no jogo na USA e na Rússia (captures/game/hud_completo8_*). Falta só o ok dele pro commit do HUD.
