# HUD do jogo: a pesquisa (09/10/2026)
O bruto das 4 pesquisas que deram o plano em `docs/HUD.md`. O plano final (revisado por um crítico adversarial) é o `HUD.md`; aqui fica o material de origem, com as fontes.

---

## 1. O HUD do Zero Hour, pelo código GPL

# HUD do C&C Generals: Zero Hour, lido no código GPL e nos dados originais do ZH 1.04

**Fontes.** Os caminhos `arquivo:linha` são relativos a `C:\Users\luisf\newcnc\GeneralsGameCode`. Os dados de layout e regras (`.wnd`/`.ini`) não estão no repositório local: ficam empacotados dentro dos arquivos `.big` do jogo. Li esses dados, só leitura e sem baixar nada, no espelho público dos arquivos originais do ZH 1.04: `TheSuperHackers/GeneralsGamePatch`, pasta `Patch104pZH/GameFilesOriginalZH`. Nesses casos cito o arquivo e o nome do bloco.

**Ressalva.** Os valores dos `.ini`/`.wnd` passaram por um leitor web que resume o conteúdo, sem número de linha. Antes de copiar qualquer número para o código, vale conferir de novo no arquivo cru.

**Base de resolução.** Tudo foi desenhado em 800×600:
- `DEFAULT_DISPLAY_WIDTH 800` / `HEIGHT 600` (`Core/GameEngine/Include/Common/GameDefines.h:177-178`).
- Todos os 14 esquemas de barra têm `ScreenCreationRes = X:800 Y:600` (ControlBarScheme.ini; America8x6, China8x6 etc.).

---

## 1. Layout da barra de comando (coordenadas em 800×600)

**Altura.**
- A câmera 3D é encolhida para 80% da altura da tela. No código original da EA era fixo: `setHeight(getHeight()*0.80f)`. A comunidade só transformou em parâmetro no commit `9f0a55eec` (2025); hoje está em `GeneralsMD/Code/GameEngine/Source/Common/GlobalData.cpp:846` e `Core/.../ControlBar/ControlBar.cpp:3825-3828`.
- Resultado: os 20% de baixo (120 px) são barra opaca, e o mundo nunca é desenhado atrás dela.
- A arte da barra vai além disso. `ImagePart InGameUIAmericaBase` fica em Y:408, com 800×191 (ControlBarScheme.ini, America8x6), ou seja, cerca de 32% da altura. A parte acima de 480 são "abas" que invadem o mundo: moldura do radar, dinheiro, botão do general.
- `ControlBarParent` = 0,416 até 799,599 (ControlBar.wnd).

**O que fica onde** (ControlBar.wnd, salvo indicação):

| Peça | Retângulo 800×600 | Observação |
|---|---|---|
| Radar (`LeftHUD`) | 7,443 até 174,595 (167×152) | Desenhado por `W3DLeftHUDDraw` (`Core/GameEngineDevice/.../W3DControlBar.cpp:62-89`) |
| Brilho "sob ataque" (`WinUAttack`) | 77,416 até 122,461 | Acima do radar |
| Coluna de botões pequenos | x 192-222, cada um 30×22 | Opções, Trabalhador ocioso, Sinalizador, Comunicador (chat/diplomacia), de cima para baixo |
| Grade de comandos | 7 colunas × 2 linhas = 14 botões | Detalhe logo abaixo |
| Dinheiro (`MoneyDisplay`) | 360,437 até 439,456 | Aba no topo-centro; Arial 8 via `HEADERTEMPLATE "LabelSmall"`, centralizado |
| Barra de energia (`PowerWindow`) | 261,473 até 544,480 | Barra fina de 283×7 px logo acima da grade |
| Painel direito (`RightHUD`) | 621,483 até 762,592 (141×109) | Grade 3×3 (`W3DCommandBarGridDraw` desenha linhas em 0,33 e 0,66; `W3DControlBar.cpp:443-466`) |
| Botão do general (estrela) | 736,433 até 792,474 | Aba no topo-direito |
| Minimizar barra (`ButtonLarge`) | 666,445 até 714,473 | |
| Painel de descrição do botão | 0,341 até 224,401 | Acima do radar; cresce para cima (ControlBarPopupDescription.wnd) |

**Grade de comandos em detalhe.**
- Cada botão tem 50×44 px; o passo é 55 px na horizontal e 51 na vertical; a grade ocupa x 223-603 e y 494-589.
- A numeração é por coluna: 01 em cima, 02 embaixo, 03 em cima, e assim por diante.
- São 14 posições visíveis; internamente cabem 18 (`ControlBar.h:410`: "user interface max is 14").

**Painel direito em detalhe.**
- Retrato: 91×69 px, ocupa 2×2 células.
- Até 5 ícones de melhoria nas outras 5 células (`ControlBar.h:411`; `ControlBar.cpp:1185-1200`).
- Ou, no lugar disso, a fila de produção com 9 posições em 3×3 (`MAX_BUILD_QUEUE_BUTTONS = 9`, `ControlBar.h:418`).

**Escala para outras resoluções.**
- Cada janela é esticada por eixo, de forma independente: `xScale = largura/800` e `yScale = altura/600` (`GeneralsMD/.../GUI/GameWindowManagerScript.cpp:528-533`).
- A arte da barra recebe o mesmo multiplicador (`ControlBarScheme.cpp:421-422`, `1125-1126`).
- Em 16:9 os botões deformam: em 1920×1080, um botão de 50×44 vira cerca de 120×79.

**Moldura por facção.** Cada facção e cada general tem um esquema próprio, escolhido pelo `Side` do jogador (`ControlBarScheme.cpp:1095-1131`).

## 2. O que muda conforme a seleção

A lógica fica em `evaluateContextUI` (`ControlBar.cpp:1745-1944`) e `switchToContext` (`ControlBar.cpp:2154-2415`).

**Nada selecionado** (`CB_CONTEXT_NONE`, `ControlBar.cpp:2182-2228`):
- A grade de comandos some.
- O painel direito mostra a arte do esquema da facção (`setPortraitByObject(nullptr)`, `ControlBar.cpp:2707-2717`).
- Radar, dinheiro, energia, botões pequenos e estrela continuam sempre visíveis.

**Uma unidade** (`CB_CONTEXT_COMMAND`):
- A grade mostra o conjunto de comandos do objeto; posições vazias ficam escondidas (`ControlBarCommand.cpp:298-313`).
- Mostra o retrato, a marca de veterania sobre o retrato e as melhorias: acesas se a unidade tem, apagadas se não tem (`ControlBar.cpp:2623-2705`).
- Não há barra de vida nem números no painel; a vida aparece só no mundo.

**Várias unidades** (`ControlBarMultiSelect.cpp`):
- A grade mostra a interseção posição por posição: um botão só aparece se for o mesmo comando, na mesma posição, em todos, e tiver `OK_FOR_MULTI_SELECT` (`:113-139`, `:174-190`).
- Exceção: Attack-Move entra se qualquer unidade tiver (`:157-173`).
- O botão fica habilitado se pelo menos uma unidade puder executar (`:401-419`).
- Não existe painel de grupo misto: o retrato é o da primeira unidade, e a variável que anularia o retrato num grupo misto é calculada mas não é usada (`:272-288`).

**Prédio de produção** (`ControlBar.cpp:2253-2269`; `ControlBarCommand.cpp:728-758`):
- Fila vazia: mostra o retrato.
- Assim que entra algo na fila, a grade 3×3 de até 9 itens substitui o retrato. Clicar num item da fila cancela aquele item (`Command_CancelUnitCreate`, `ControlBarCommand.cpp:546-689`).
- Só o primeiro item mostra progresso: um "relógio" em formato de pizza que vai revelando o ícone no sentido horário, a partir das 12h. É `GadgetButtonDrawInverseClock` (`ControlBarCommand.cpp:783-796`) com `drawRemainingRectClock` (`GeneralsMD/.../W3DDisplay.cpp:2558`), na cor `BuildUpClockColor R0 G0 B0 A160` (ControlBarScheme.ini, America8x6).
- O texto de porcentagem existe mas está comentado (`ControlBarCommand.cpp:790-793`).
- Com a fábrica selecionada, o ponto de encontro aparece no mundo como um objeto marcador na cor do jogador (`ControlBar.cpp:2725-2771`; chamado em `ControlBarCommand.cpp:488-497`).
- Fila cheia (9 itens): os botões de construir ficam restritos (`ControlBarCommand.cpp:1123`, `1194-1197`).
- Não há número de quantidade no botão de construir: nenhum código escreve texto nele.
- Botões da War Factory (CommandSet.ini, `AmericaWarFactoryCommandSet`): 1-8 constroem veículos, 9 e 11 são melhorias, 13 é Rally Point, 14 é Vender.

**Unidade inimiga selecionada** (`ControlBar.cpp:1776-1834`):
- Mostra o retrato, a veterania e as melhorias do dono. Na prática isso revela as melhorias do inimigo.
- Nenhum comando aparece.

## 3. Comandos do tanque e como são acionados

Conjunto de comandos do tanque (CommandSet.ini, `AmericaTankCrusaderCommandSet`; Paladin e Battlemaster seguem o mesmo padrão):
- Posições 1-3: drones (só na USA).
- Posição 11: `Command_AttackMove` (coluna 6, em cima).
- Posição 13: `Command_Guard` (coluna 7, em cima).
- Posição 14: `Command_Stop` (coluna 7, embaixo).

Ou seja, os comandos de "sistema" ficam sempre na coluna da direita.

| Comando | Definição (CommandButton.ini) | Acionamento |
|---|---|---|
| Attack-Move | `ATTACK_MOVE`, `OK_FOR_MULTI_SELECT NEED_TARGET_POS`, borda SYSTEM, cursor `AttackMove` (SCCAttMov), cursor inválido `GenericInvalid` | O botão entra em modo de cursor (`ControlBarCommandProcessing.cpp:207-225`). O próximo clique no chão ou no radar executa (`GUICommandTranslator.cpp:465-468`, `ControlBarCallback.cpp:282-290`). O modo sai sozinho depois do uso (`GUICommandTranslator.cpp:491-497`) |
| Guard | `GUARD`, `NEED_TARGET_POS`, `RadiusCursorType GUARD_AREA` | Modo de cursor com um decal circular no chão (`GuardAreaRadiusCursor`, SCCGuard_Generic, opacidade pulsando 50-100% em 1 s; InGameUI.ini). Clique no chão protege a área; clique em objeto protege o objeto (`GUICommandTranslator.cpp:225-250`). Duplo clique no chão também manda proteger a posição (`CommandXlat.cpp:3970-3986`) |
| Stop | `STOP`, `OK_FOR_MULTI_SELECT` | Imediato (`ControlBarCommandProcessing.cpp:627-631`). Tecla **S** (CommandMap.ini em inglês, `STOP`). Continua funcionando com a unidade desabilitada (`ControlBarCommand.cpp:1084`) |
| Scatter | Não tem botão | Só tecla **X** (`SCATTER`; `CommandXlat.cpp:3127-3131`) |
| Force fire | Não tem botão | Segurar **Ctrl** e clicar (`BEGIN_FORCEATTACK`; `CommandXlat.cpp:1488-1526`) |
| Waypoints | Não tem botão | Segurar **Alt** e clicar (`BEGIN_WAYPOINTS`; `CommandXlat.cpp:1002-1004`) |
| Rally (fábrica) | `SET_RALLY_POINT`, `NEED_TARGET_POS`, cursor `SetRallyPoint` | Modo de cursor (`GUICommandTranslator.cpp:472`) |

**Teclas de seleção e câmera** (CommandMap.ini em inglês):
- Shift adiciona à seleção; E seleciona as do mesmo tipo; Q seleciona todas; W todas as aeronaves.
- Ctrl+0-9 cria grupo, 0-9 seleciona, Shift+número adiciona, Alt+número leva a câmera ao grupo.
- H vai para a base; Espaço vai ao último evento do radar; F9 esconde a barra; Ctrl+F forma formação.
- F1-F8 voltam a vistas salvas; Ctrl+F1-F8 salvam.

**Atalhos dos botões.** A letra de cada botão vem do `&` no texto traduzido: `searchHotKey` (`HotKey.cpp:203`) e `addHotKey` (`ControlBar.cpp:2528-2533`). A tecla "clica" o botão visível (`HotKey.cpp:161-191`), então atalho e botão nunca saem de sincronia. As letras de Attack-Move e Guard (A e G, de memória) estão no `generals.csf`, que é binário e não conferi.

**Mouse.** O clique esquerdo no radar com unidades selecionadas manda mover até lá; sem seleção, ou com clique direito, a câmera vai até o ponto (`ControlBarCallback.cpp:255-266`).

## 4. Descrição ao passar o mouse, clique, desabilitado, fila, teclas

**Descrição do botão** (`ControlBarPopupDescription.cpp`):
- Painel fixo de 224×60 px acima do radar, com nome, custo e descrição.
- Inclui os pré-requisitos e o motivo de estar indisponível: sem dinheiro, fila cheia, limite de unidades (`:300-345`, `:390-430`).
- Cresce para cima conforme o texto, com quebra de linha em largura−10 (`:621-679`).
- Os botões de comando têm `TOOLTIPDELAY = 1` (ControlBar.wnd), ou seja, a descrição aparece instantaneamente. O atraso genérico das outras janelas é 800 ms (Mouse.ini `TooltipDelayTime`).

**Tecla no botão.**
- O botão não mostra texto nenhum, só o ícone (`setControlCommand` não escreve rótulo; `ControlBar.cpp:2514-2520`).
- A letra do atalho aparece só no nome dentro da descrição, pintada de amarelo: status `HOTKEY_TEXT` em ControlBarPopupDescription.wnd e `m_hotKeyTextColor = 255,255,0` (`GlobalData.cpp:923`).

**Clique.**
- O botão dispara ao apertar o mouse, não ao soltar: status `ON_MOUSE_DOWN` em ControlBar.wnd e `GadgetPushButton.cpp:72`.
- Sons: "GUICommandBarClick" no clique (`ControlBar.cpp:2534`); "GUIClick" e "GUIClickDisabled" pela tecla (`HotKey.cpp:178`, `187`); som específico da unidade, se houver (`ControlBarCommandProcessing.cpp:198-205`); resposta de voz da unidade (`GUICommandTranslator.cpp:250`, `288`).
- Mouse em cima: sobreposição "Cameo_hilited"; apertado ou ligado: "Cameo_push" (`W3DPushButton.cpp:454-483`).

**Estados do botão** (`ControlBarCommand.cpp:843-866`; `W3DPushButton.cpp:376-396`):
- Restrito (pré-requisito ou fila cheia): a própria arte em tons de cinza.
- Recarregando: arte colorida com o relógio por cima (`ControlBarCommand.cpp:1316-1322`).
- Já tem ou já está pesquisando: escurecido para 144/255 (`0xff909090`).
- Sem dinheiro: aparece cinza; o estado "não pode pagar" está comentado e trocado por "restrito" (`ControlBarCommand.cpp:1162-1164`, `1210-1213`).

**Borda colorida por tipo** (1 px; `W3DPushButton.cpp:437-441`; cores da USA em ControlBarScheme.ini, America8x6):
- Construir: azul (67,108,190).
- Ação: verde (1,175,2).
- Melhoria: laranja (208,108,0).
- Sistema: amarelo (207,195,2).

**Outros sinais.**
- Botão piscando: só por script (tutorial), alternando a cada 10 quadros (`ControlBar.cpp:1498-1511`).
- Brilho de ataque no radar: 150 quadros, alternando a cada 15, ou seja, 5 s a 30 qps (`ControlBar.cpp:3213-3246`).
- Mensagens de texto no canto superior esquerdo, em (10,10), Arial 10 negrito (InGameUI.ini).

## 5. Fontes e resolução

**Quais fontes.**
- Arial em quase tudo.
  - Arial 8: descrição dos botões, dinheiro e `HeaderTemplate LabelSmall`.
  - Arial 10: mensagens, número do grupo e legendas.
- "Generals" (títulos, 20/15 pt) é Arial com largura apertada em 40% da altura (`Core/Libraries/.../render2dsentence.cpp:1480-1497`).
- Courier New 12 nas legendas militares com efeito de digitação (Language.ini, HeaderTemplate.ini, InGameUI.ini).

**Sombra.**
- Sombra preta de 1 px em todo texto (`GeneralsMD/.../W3DDisplayString.cpp:156-159`, 209-215).
- Número do grupo de controle: branco com sombra de 1 px (DrawGroupInfo.ini).

**Por que o texto do ZH é nítido** (é o oposto do nosso defeito):
- Cada combinação de tamanho, fonte e negrito é rasterizada pelo Windows (GDI, com antialiasing) já no tamanho final em pixels: `MulDiv(pt, 96, 72)` e `ANTIALIASED_QUALITY` (`render2dsentence.cpp:1489-1511`).
- O resultado fica guardado por tamanho (`GameFont.cpp`, `FontLibrary::getFont`) e é desenhado 1:1, nunca esticado.

**Como o tamanho escala.**
- `fator = 1 + (largura/800 − 1) × 0,7`, com teto 2,0 no método original (`GlobalLanguage.cpp:213-221`; 0,7 é o `ResolutionFontAdjustment` do Language.ini).
- Exemplo em 1920 de largura: fator 1,98, Arial 8 vira 15 pt, cerca de 20 px de altura.
- Só escalam as fontes que passam por `adjustFontSize`: modelos de cabeçalho (`HeaderTemplate.cpp:223`), mensagens e descrições (`Mouse.cpp:627`).
- A fonte declarada direto no `.wnd` não escala (`GameWindowManagerScript.cpp:622`); só a caixa estica.
- Daí o defeito clássico: texto pequeno em barra gigante nas resoluções altas.
- O padrão atual sem teto é mudança da comunidade, não da EA (`GlobalLanguage.h:73-80`).

## 6. O que vale manter e o que é datado

**Manter (assinatura do ZH):**
- Barra inferior em três blocos (radar à esquerda, grade no centro, retrato ou fila à direita) com moldura por facção, e "abas" sobressaindo no topo para dinheiro e estrela do general.
- Mundo 3D desenhado só acima da barra (80% da altura), para a barra nunca cobrir unidade.
- Posições fixas e consistentes: comandos de sistema na coluna da direita (Attack-Move 11, Guard 13, Stop 14; Rally 13 e Vender 14 nos prédios). Isso dá memória muscular e faz a seleção múltipla funcionar por posição.
- Borda colorida por tipo de botão, para leitura instantânea.
- Fila 3×3 substituindo o retrato, com relógio que revela o ícone e cancelamento por clique.
- Botão que dispara ao apertar o mouse, descrição instantânea com o motivo de estar indisponível, letra do atalho em amarelo.
- Atalho e botão como a mesma coisa (`&` no rótulo).
- Três estados de indisponível distintos: cinza, relógio e escurecido.
- Decal circular no chão no modo Guard.
- Radar clicável (mover ou atacar e mover), brilho de ataque, Espaço para o último evento, F9 para minimizar.

**Datado:**
- Layout absoluto em 800×600 esticado por eixo: arte e radar deformam em 16:9.
- Fontes fixas no `.wnd` e teto de 2× no fator: texto minúsculo em 1440p e 4K.
- Barra de largura total ocupando 20% sólido, e até cerca de 32% com as abas, num widescreen.
- Botões só com ícone, sem a tecla visível no próprio botão.
- Seleção mista sem painel de grupo (só o retrato da primeira unidade) e sem contagem ou vida por unidade.
- Nenhum contador de quantidade no botão de construir.
- Fila limitada a 9.
- Descrição presa no canto esquerdo, longe do cursor e do botão.
- Seleção inimiga revelando as melhorias do inimigo.

**Para o nosso escopo atual** (um tanque, War Factory, sem economia), seguindo a mesma convenção:
- **Tanque:** Attack-Move na coluna 6 em cima, Guard na 7 em cima, Stop na 7 embaixo. Scatter (X) e Force fire (Ctrl) só por teclado.
- **Fábrica:** construir tanque na coluna 1 em cima, Rally na 7 em cima. Sem Vender, porque não há economia.

Fontes web:
- [TheSuperHackers/GeneralsGamePatch, arquivos originais do ZH 1.04](https://github.com/TheSuperHackers/GeneralsGamePatch/tree/main/Patch104pZH/GameFilesOriginalZH)
- [ControlBar.wnd](https://raw.githubusercontent.com/TheSuperHackers/GeneralsGamePatch/main/Patch104pZH/GameFilesOriginalZH/Window/ControlBar.wnd)
- [ControlBarPopupDescription.wnd](https://raw.githubusercontent.com/TheSuperHackers/GeneralsGamePatch/main/Patch104pZH/GameFilesOriginalZH/Window/ControlBarPopupDescription.wnd)
- [ControlBarScheme.ini](https://raw.githubusercontent.com/TheSuperHackers/GeneralsGamePatch/main/Patch104pZH/GameFilesOriginalZH/Data/INI/ControlBarScheme.ini)
- [CommandSet.ini](https://raw.githubusercontent.com/TheSuperHackers/GeneralsGamePatch/main/Patch104pZH/GameFilesOriginalZH/Data/INI/CommandSet.ini)
- [CommandButton.ini](https://raw.githubusercontent.com/TheSuperHackers/GeneralsGamePatch/main/Patch104pZH/GameFilesOriginalZH/Data/INI/CommandButton.ini)
- [CommandMap.ini (inglês)](https://raw.githubusercontent.com/TheSuperHackers/GeneralsGamePatch/main/Patch104pZH/GameFilesOriginalZH/Data/English/CommandMap.ini)
- [Language.ini](https://raw.githubusercontent.com/TheSuperHackers/GeneralsGamePatch/main/Patch104pZH/GameFilesOriginalZH/Data/English/Language.ini)
- [HeaderTemplate.ini](https://raw.githubusercontent.com/TheSuperHackers/GeneralsGamePatch/main/Patch104pZH/GameFilesOriginalZH/Data/English/HeaderTemplate.ini)
- [Mouse.ini](https://raw.githubusercontent.com/TheSuperHackers/GeneralsGamePatch/main/Patch104pZH/GameFilesOriginalZH/Data/INI/Mouse.ini)
- [InGameUI.ini](https://raw.githubusercontent.com/TheSuperHackers/GeneralsGamePatch/main/Patch104pZH/GameFilesOriginalZH/Data/INI/InGameUI.ini)
- [DrawGroupInfo.ini](https://raw.githubusercontent.com/TheSuperHackers/GeneralsGamePatch/main/Patch104pZH/GameFilesOriginalZH/Data/INI/DrawGroupInfo.ini)

Buscas consultadas, sem o dado decisivo:
- [Playing with Buttons (PPM)](https://cnc.projectperfectmod.com/gennewbuttons/)
- [magicgameworld, atalhos do ZH](https://www.magicgameworld.com/?p=99817)

---

## 2. Os RTS modernos

# Pesquisa: HUD de comando em RTS modernos, para o nosso RTS no estilo ZH

**Como fiz.** Não abri a Unreal e não editei nada. Separei as fontes oficiais (notas de patch, blogs dos estúdios, diretrizes Xbox/W3C) das de fórum e guia de terceiros. Não achei nenhuma GDC talk dedicada ao HUD de SC2, AoE4 ou Tempest. As fontes de desenvolvedor mais fortes foram três:
- a entrevista com Dave Pottinger (Ensemble, AoE e Halo Wars);
- o texto da própria EA sobre a interface do C&C3;
- as notas de patch.

Alguns sites bloquearam a leitura (403): Liquipedia, Prima, PCGH, AoEZone. Quando cito um deles, a informação veio só do resumo da busca e eu marco isso.

---

## 1. Padrões convergentes

**Posição da barra**
- **A faixa inteira embaixo virou o padrão do gênero.** Bycer resume: "a bottom information pane became standard for RTS design" (https://www.gamedeveloper.com/design/ui-strategy-game-design-dos-and-don-ts, mesmo texto em https://game-wisdom.com/?p=10231). No SC2 o minimapa fica no canto inferior esquerdo e o aviso de trabalhador ocioso fica "above the minimap" (https://news.blizzard.com/en-us/article/4552955/game-guide-special-control). O centro tem retrato e painel de informação (https://zhenyi.gibber.blog/the-user-interface-of-starcraft, blog de fã).
- **A família C&C alterna entre dois modelos:**
  - *Barra lateral global:* TD/RA, C&C3, Remastered e Tempest Rising.
  - *Barra inferior que muda conforme a seleção:* Generals/ZH. Fonte: "the current selection determines the contents of the command bar" (https://www.giantbomb.com/wiki/Games/Command_And_Conquer_Generals). Cada fábrica tem fila própria "in the same fashion as in Blizzard's RTS" (https://www.mobygames.com/game/8387).
- **O C&C3 juntou os dois de propósito.** A barra lateral ganhou "the contextual information and object level control of the Generals UI". A produção é controlada de qualquer lugar do mapa, "without use of control groups" (https://worthplaying.com/article/2006/10/17/news/37076-command-conquer-tiberium-wars-user-interface-details-screens/).
- **Tempest Rising (feito em Unreal, 2025):**
  - um menu à direita que "does everything from build structures to recruit units" (https://www.shacknews.com/article/142851/tempest-rising-preview);
  - minimapa "on the upper right" (https://tempestrising.wiki.gg/wiki/Radar);
  - barra de habilidades à esquerda, segundo um jogador no Steam.

**Quanto da tela o HUD ocupa**
- **Nenhuma fonte publica a porcentagem da altura** para SC2, AoE4, ZH ou Tempest. *Inferência:* medir em capturas 1080p do https://www.gameuidatabase.com antes de fixar um número. O único número que achei é queixa de jogador de AoE4 no beta: "20% of screen" só para as abas de informação (aoezone.net, 403, via busca).
- **A tendência é cortar:**
  - O Stormgate 0.5.0 removeu as barras do topo porque "occupied too much UI space relative to their gameplay impact" e "contributed to a sense of overwhelmingness" (https://playstormgate.com/news/stormgate-update-0-5-0).
  - Pottinger conta que o HUD do AoE3 ficou grande demais e depois saiu uma versão menor (https://waywardstrategy.com/2015/05/04/lets-talk-rts-user-interface-part-1-interview-with-dave-pottinger/).
  - O AoE3 DE tem HUD "appearing and disappearing as you select", três layouts e opção de escala (https://www.ageofempires.com/news/aoe3de-hud-and-ui/).
  - O C&C Remastered subiu dinheiro e opções para cima da barra lateral para liberar campo (https://cncnz.com/cc-remastered-update-sidebar-and-full-production/).
- **O ZH já tinha a barra recolhível.** Estados `DEFAULT` / `LOW` ("a la minimalist") / `HIDDEN`: `GeneralsGameCode\Core\GameEngine\Include\GameClient\ControlBar.h:638-645`, alternados em `ControlBar.cpp:3019`.

**Transparência sobre o campo**
- **Painéis do HUD:** *inferência* — nenhum dos jogos analisados usa painel translúcido grande.
- **O que flutua sobre o campo usa opacidade parcial.** No AoE4 (patch 5.1.148), as barras de vida ficam em "90% opacity", sobem a "100% when hovered or selected", e o contorno de seleção é "50% opacity outline and a black inner border for improved contrast" (https://ageofempires.com/news/age-of-empires-iv-patch-51148).
- **Diretriz Xbox (XAG 102):**
  - fundo sólido atrás do texto, com opacidade ajustável;
  - contraste de 4.5:1, medido "against the lowest-contrasting part of a non-solid background" (https://devdocs.xbox.com/build/game-principles/accessibility/xag-deep-dives/xag-102-contrast.md).
  - *Inferência:* painel translúcido sobre terreno que muda tende a falhar nesse contraste. Melhor painel pequeno e opaco do que grande e translúcido.

## 2. Grade de comandos

**Tamanho da grade e teclas por jogo**
- **SC2:** grade 5×3 = 15 botões, ligada a QWERT/ASDFG/ZXCVB (https://spwhitton.name/blog/entry/gridhotkeys/).
  - O patch 1.2 adicionou a opção "show the primary hotkey on top of command buttons" (https://www.co-optimus.com/article/5276/starcraft-ii-s-1-2-patch-brings-bug-fixes-and-editor-improvements-to-players.html).
  - O tooltip mostra "the name of the command and the associated hotkey" (https://news.blizzard.com/en-us/article/4552955/game-guide-special-control).
- **WC3:** cartão 4×3 = 12, segundo um fórum que cita a demo do Reforged (https://us.forums.blizzard.com/en/warcraft3/t/ui-bottom-bar-customization/1080). A opção de grade veio no patch 1.32. O patch 2.0.0 trouxe (https://news.blizzard.com/en-us/article/24167122/warcraft-iii-reforged-patch-notes-patch-2-0-0):
  - edição de teclas dentro do jogo;
  - "Numbers on cooldown … on by default";
  - HUD escalável.
- **ZH:** "user interface max is 14" botões (`ControlBar.h:410`), fila de produção de 9 (`:418`) e 11 atalhos de poder especial (`:419`).
- **Tempest Rising** (Prima via busca, 403): abas E/R/T/Y/U, itens em F1–F12, habilidades em Z–B.
- **BAR:** dois esquemas, GRID e LEGACY. No GRID, "you can select every unit with only keys" e as teclas "are shown in the build-options" (https://www.beyondallreason.info/commands-2-0-grid).

**Grade ou letra que lembra o comando (mnemônica)?**
- O AoE4 vem com grade de fábrica. A crítica: "visually clean rather than mechanically efficient" (https://forums.ageofempires.com/t/grid-hotkeys-feel-alien-coming-from-other-franchises-criticism-and-a-brief-history-of-grid-in-other-games/184268).
- O Stormgate 0.3.0 passou a aceitar tecla por comando além de por posição ("Grid position hotkeys are still available") e duas teclas por ação (https://playstormgate.com/news/stormgate-patch-0-3-0).
- Pottinger não gosta de QWERTY mas defende tê-la como opção (link do item 1).

**Estado dos botões**
- **Recarga:** número de recarga (WC3 2.0); recarga mostrada no painel de macro do Stormgate 0.5.0.
- **Fila:** no AoE4, Shift soma 5 à fila (dotesports via busca). No BAR: Shift +5, Ctrl −1, Alt põe na frente (link GRID acima).
- **Desabilitado:** texto em 3:1 (XAG 102).
- **Custo no tooltip:** *de memória, não confirmei.*

## 3. Painel de seleção

**Uma unidade**
O C&C3 mostra retrato, vida, upgrades, "combat chain", veterania e habilidades. Dá para mandar ordem a uma parte da seleção "without losing his top level group selection" (WorthPlaying, item 1).

**Várias unidades**
- **SC2, no mundo** (https://news.blizzard.com/en-us/article/6640645/game-guide-simplified-controls):
  - Shift+clique adiciona ou remove;
  - Ctrl+clique ou duplo clique pega o tipo inteiro na tela;
  - Tab percorre os subgrupos.
- **SC2, no painel:** que os mesmos cliques funcionam nos ícones do painel é *de memória*. As 24 por página (3 linhas), com páginas além disso, vêm de dica de jogador.
- **CoH2:** clicar no painel ou usar Tab; Shift+clique remove (https://coh2.org/guides/110897/tomy-s-beginner-guide).
- **Tempest Rising:** planeja "show or hide the unit selection" (PCGH, março/2026, 403, via busca).

**Grupos de controle**
- Todos convergem em: Ctrl+n atribui, n seleciona, n duas vezes centraliza, Shift+n adiciona. Fontes: SC2 (link acima) e AoE4 (https://news.xbox.com/en-us/2021/10/22/age-of-empires-iv-hotkeys-revealed/).
- O SC2 tem um "control group display" na tela (https://liquipedia.net/starcraft2/Replay_Features, 403, via busca).
- **Grupos automáticos:**
  - Stormgate: o exército vai sozinho para o grupo 0, e é configurável (patch 0.3.0);
  - BAR: "Newly build units will get auto assigned" (https://www.beyondallreason.info/commands/group-units-auto).
- **Alerta sobre "selecionar todos":** no Halo Wars, o botão "became all anyone ever used" (Pottinger).

## 4. Painel de produção

**Três modelos**
- **(a) Barra lateral global (C&C):** produz sem selecionar o prédio. Exemplos:
  - C&C3 (WorthPlaying);
  - Remastered, com 18 botões e abas "to reduce the need for scrolling" (cncnz, item 1);
  - Tempest Rising, onde se produz "without having to pan back to the base" (https://www.gamewatcher.com/reviews/tempest-rising-review/13449). Ganha filas paralelas com prédios repetidos (https://tempestrising.wiki.gg/wiki/Tempest_Rising) e ganha botões de ordenar na fila global (PCGH, via busca).
- **(b) Fila por prédio (Blizzard/ZH):** a fábrica precisa estar selecionada, então dependem de atalhos:
  - AoE4: F1 "Select all Military Production Buildings" (Xbox Wire);
  - Stormgate: o painel de macro deixa "access any building … at all times" (https://www.gamedeveloper.com/press-release/stormgate-enters-steam-early-access-august-13), e Backspace percorre as bases.
- **(c) Painel de todos os jogadores:** o WC3 1.36 mostra "which units each player is currently building", mas só no modo observador (https://news.blizzard.com/en-us/article/23951676/warcraft-iii-reforged-version-1-36-patch-now-live).

**Contrapeso**
Frost Giant teria dito que a produção "will not be on screen at all times" para não sobrecarregar. A fonte é Reddit e não confirmei quem escreveu.

**Ponto de encontro**
SC2 e AoE4 usam clique direito com o prédio selecionado. O BAR tem "Repeat" na fila.

## 5. Escala, alvos de clique e fontes

**Tamanho de texto e contraste**
- **XAG 101:** PC a 1080p = **18 px de altura de corpo** (da descendente à ascendente); 4K = 36 px; console = 26 px. Deve escalar até 200%, e "screen magnifiers are not a substitute" (https://devdocs.xbox.com/build/game-principles/accessibility/xag-deep-dives/xag-101-text-display.md).
- **XAG 102:** 4.5:1 no geral; 3:1 para texto grande ou desabilitado; 7:1 no modo de alto contraste.

**Alvos de clique**
WCAG 2.5.8: 24×24 px no mínimo (AA). WCAG 2.5.5: 44×44 px (AAA) (https://www.w3.org/WAI/WCAG22/Understanding/target-size-minimum.html).

**O que os jogos fazem com escala**
- **AoE4:** texto em 100–150% (Default/Large/Largest) mais "Strong Contrast" (https://www.ageofempires.com/age-iv-accessibility/). Cliques na seleção quebram com escala do HUD acima de 120% a 1440p (https://forums.ageofempires.com/t/bug-using-hud-scale-breaks-the-unit-selection-research-creation-progress-buttons/62644).
- **CoH3:** sem escala no lançamento (https://steamcommunity.com/app/1677280/discussions/0/3770113150031281201).
- **Tempest Rising (Unreal) — a lição mais direta para nós:**
  - a escala vai no máximo até 1,0 e fica pequena a 1440p;
  - forçar 1,15 no `.ini` cortou botões e travou a campanha (https://steamcommunity.com/app/1486920/discussions/0/603027953652124957, 18/04/2025);
  - outra pessoa relatou o HUD ilegível numa TV de 52 polegadas.

**Na Unreal e no ZH**
- **Unreal:** escala pelo lado menor da tela; curva 720→0,666 e 1080→1,0 (`UE_5.8\Engine\Config\BaseEngine.ini:1474-1475`).
- **ZH:** desenhado em 800×600 (`GameDefines.h:177-178`), escala X e Y separadamente (`ControlBarResizer.cpp:183-184`). *Inferência:* estica em 16:9.

**Fontes tipográficas e ícones**
- A Blizzard não publica a fonte do SC2.
- Tempest, WC3 e Remastered trocaram texto por ícone. No Remastered o motivo citado foi a tradução.
- No CoH3, jogadores preferiram ícones 2D pintados a renderizações 3D para leitura rápida (https://retrostylegames.com/blog/analyzing-company-of-heroes-3-game-design/).
- **Toque icônico do ZH:** a barra muda de visual por facção (`ControlBarScheme.cpp:79,1083-1113`).

## 6. Recomendação para o nosso escopo (1 tanque, 1 fábrica, sem economia, 1v1)

Esta parte é inferência minha a partir das fontes acima.

**P0 — fazer agora**
1. **Texto:** corpo de pelo menos 18 px a 1080p, escalando pela altura com a curva da Unreal (dá 24 px a 1440p e 36 px no 4K). Fundo opaco atrás do texto com 4.5:1 de contraste. Isso já está sendo corrigido à parte.
2. **Barra inferior compacta no estilo ZH**, com o visual da facção:
   - minimapa à esquerda (já existe);
   - seleção no meio;
   - grade 5×3 à direita.
   - Comandos do tanque em posições fixas na linha de cima: Mover, Parar, Manter posição, Patrulha, Atacar-mover.
   - Teclas padrão que lembram o comando, A/S/H/P como no SC2, impressas no botão. Tooltip com nome e tecla. Botões com no mínimo 44 px a 1080p.
   - Perfil de teclas por grade só depois.
3. **Fábrica sem precisar selecionar:** um cartão fixo de produção ao estilo C&C/Tempest:
   - ícone do tanque, barra de progresso com %, contador da fila;
   - Shift = +5 e clique direito = cancelar (padrão AoE4/BAR);
   - uma tecla global para produzir;
   - ponto de encontro com clique direito e bandeira visível.

   Com uma fábrica só, isso cabe num único cartão, sem poluir a tela como o Stormgate temia.
4. **Seleção:**
   - uma unidade: barra de vida e número;
   - várias: grade de ícones com vida em cada um; clique pega só aquele, Shift+clique tira.
5. **Grupos 1–0 visíveis acima da barra**, com o número e quantos tanques há em cada um. Ctrl+n, n, n duas vezes, Shift+n. Uma tecla "todo o exército", mas sem esconder os grupos (o alerta do Pottinger).

**P1 — depois**
- Estados visuais nos botões: recarga com número e desabilitado em 3:1.
- Backspace leva à base ou fábrica; Espaço vai ao último alerta (padrão SC2).
- Barra recolhível como os estados `LOW` / `HIDDEN` do ZH.
- Escala do HUD em presets de 100/125/150%, com layout testado acima de 1,0 (a lição do Tempest).
- Barras de vida a 90% de opacidade, subindo para 100% ao passar o mouse ou selecionar (como no AoE4).

**P2 — para mais tarde**
- Painel de produção de N jogadores e de observador (como o WC3 1.36).
- Grupos automáticos (Stormgate/BAR).
- Câmeras salvas em F5–F8.

**Cuidado com a câmera** (*inferência, não achei fonte*): a barra de baixo cobre a parte mais próxima da câmera inclinada a 37,5°. Os limites da câmera precisam deixar a borda sul do mapa aparecer acima da barra.

---

## 3. A técnica na Unreal 5.8

# Técnica do HUD na Unreal 5.8: o que recomendo

**Recomendação:** o HUD da tela inteira (painéis, botões, textos, minimapa, menus, dicas) passa a ser **Slate puro em C++**, como o painel do WorldForge. O **Canvas fica só para o que é preso ao mundo**: anel, barra de vida, marca de ordem, caixa de seleção e seta. **UMG e CommonUI ficam de fora agora.** Nada aqui foi medido em tempo de quadro. Toda afirmação vem de leitura do fonte.

Prefixos dos caminhos: `RT/` = `C:\Program Files\Epic Games\UE_5.8\Engine\Source\Runtime\`, `CUI/` = `...\Engine\Plugins\Runtime\CommonUI\Source\CommonUI\`, `G/` = `C:\Users\luisf\explosion\Game\Source\RTSGame\`, `WF/` = `C:\Users\luisf\explosion\WorldForge\Source\WorldForge\Private\`, `ZH/` = `C:\Users\luisf\newcnc\GeneralsGameCode\`.

---

## 1. Comparação das quatro técnicas

| | Canvas/AHUD (hoje) | Slate em C++ | UMG por código | CommonUI |
|---|---|---|---|---|
| **Texto** | Fica nítido só com `FSlateFontInfo`, escala 1 e posição em pixel inteiro, tudo feito à mão (ver §3). O tamanho não acompanha a resolução: a escala do Canvas é a do Windows (`RT/Engine/Private/UnrealClient.cpp:1863`, `GameViewportClient.cpp:1007-1018`). | Desenha a letra já no tamanho final, com a escala da tela aplicada, e arredonda a posição (§3). Automático. | É o próprio Slate (`UTextBlock` vira `STextBlock`). | É o UMG, com estilos de texto em classes. |
| **Escala por resolução** | Manual. Hoje só o minimapa escala (30% da altura, `G/Private/RTSHUD.cpp:39,626`). Botões de 340×54 px e barra do topo de 480 px são fixos. | Automática: o `SDPIScaler` (`RT/Engine/Private/Slate/SGameLayerManager.cpp:113-115,467-498`) aplica a curva padrão da Unreal, que olha o lado menor da tela. Fica 1,0 em 1080p, 0,83 em 900p, 1,33 em 1440p e 2,0 em 4K (`Engine/Config/BaseEngine.ini:1474-1475`, cálculo em `RT/Engine/Private/UserInterfaceSettings.cpp:87-110`). A curva muda por configuração, sem asset. | Igual ao Slate: o UMG usa o mesmo caminho (`RT/UMG/Private/Blueprint/GameViewportSubsystem.cpp:184-193`). | Igual. |
| **CPU/GPU com HUD que muda todo quadro** | Redesenha tudo a cada quadro e mede o texto a cada chamada. É barato. | Sem invalidação global, que vem desligada (`RT/SlateCore/Private/SlateCoreClasses.cpp:52-57`), repinta a árvore visível todo quadro. Partes paradas podem ser guardadas com `SInvalidationPanel` (`RT/Slate/Public/Widgets/SInvalidationPanel.h:28`). Medir com `stat slate` (`SlateGlobals.h:63`) e o trecho de GPU "SlateUI" (`RT/SlateRHIRenderer/Private/SlateRHIRenderer.cpp:1232`). | Igual, mais um objeto da Unreal por widget. Tem o `URetainerBox` (pinta a cada N quadros numa textura: `RT/UMG/Public/Slate/SRetainerWidget.h:48-58`). | Igual ao UMG. |
| **Formas** (losango, barras) | `DrawLine`/`DrawRect`/`FCanvasTriangleItem`/material. Linha grossa vira quadrilátero **sem antisserrilhado** (`RT/Engine/Private/BatchedElements.cpp:1074-1085`). | `MakeBox`, `MakeRotatedBox` (o losango com a textura numa chamada), `MakeLines` com antisserrilhado, `MakeCustomVerts`, `MakeGradient` (`RT/SlateCore/Public/Rendering/DrawElementTypes.h:102-119,228,305,180`). Cantos arredondados (já usados no WorldForge, `WF/SForgePanel.cpp:68-76`). Vidro com `SBackgroundBlur`, que tem modo de baixa qualidade ligável por preset (`RT/Slate/Public/Widgets/Layout/SBackgroundBlur.h:41-45`, `.cpp:17`). | `NativePaint` faz o mesmo (`RT/UMG/Private/UserWidget.cpp:2383`). | Igual ao UMG. |
| **Foco e navegação** | Não tem. | Completos, com as armadilhas do §2. | Igual ao Slate. | Pilha de telas e entrada própria. É o forte dele para controle de videogame. |
| **Dicas ao passar o mouse** | Não tem; seria feito à mão. | Nativas: a camada do jogo já tem o espaço delas (`SGameLayerManager.cpp:160-166`), com atraso de 0,15 s ajustável (`RT/Slate/Private/Framework/Application/SlateUser.cpp:52-55`). | Iguais. | Iguais. |
| **Sem asset feito no editor** | Sim, mas uma fonte final própria exigiria um asset de fonte: o Canvas só aceita uma fonte que seja asset (`RT/Engine/Public/CanvasItem.h:450`). | Sim. A fonte final pode vir direto de um arquivo .ttf, escolhendo o hinting (`RT/SlateCore/Public/Fonts/SlateFontInfo.h:246`, `CompositeFont.h:442`). | Sim, mas montar a árvore de widgets por código é verboso; o ADR-0021 do Vanguard rejeitou por isso. | Não de verdade: exige trocar o cliente da viewport (`CUI/Private/Input/CommonUIActionRouterBase.cpp:333-337`), uma classe de dados de entrada na configuração (`CommonInput/Public/CommonInputSettings.h:80-82`) e classes de estilo feitas para Blueprint (`CUI/Public/CommonButtonBase.h:68-69`). |

**Por que não UMG:** ele vira o mesmo Slate na viewport. O que ganharíamos é pouco: a remoção automática ao trocar de mapa (`GameViewportSubsystem.cpp:296-322`) e o `RetainerBox`. Em troca, pagamos objetos da Unreal e mais código.

**Por que não CommonUI:** ele assume a captura e a trava do mouse por conta própria (`CommonUIActionRouterBase.cpp:1866-1912`). Isso briga com o `LockAlways` e a rolagem pela borda de hoje (`G/Private/RTSPlayerController.cpp:54-58`). Só vale se controle de videogame entrar no escopo. O `docs/JOGO.md:247` ainda cita CommonUI no front-end; essa linha deveria mudar.

**A prova em casa:** o painel do WorldForge já é Slate puro (`WF/SForgePanel.cpp`), e o teste dele clica pelo `FSlateApplication` (`WF/ForgeUITest.cpp:88-119`). O teste automático do jogo já injeta os cliques do mesmo jeito (`G/Private/RTSAutoTest.cpp:150-203`).

---

## 2. Como o clique chega no botão ou no mundo

**Como funciona hoje no motor:**
- A camada de widgets mora **dentro** do widget da viewport do jogo (`RT/Engine/Private/GameViewportClient.cpp:1302-1326`), e o HUD entra nela por `AddViewportWidgetContent` (linhas 3376-3385).
- O clique sobe do widget mais fundo até a raiz (`RT/Slate/Private/Framework/Application/SlateApplication.cpp:5443-5464`). Se um widget responde `FReply::Handled()`, o clique para nele. A viewport nunca o recebe, o controle do jogador não vê o botão apertado e não sai ordem.
- Se ninguém trata o clique, ele segue até a viewport e vira entrada do jogo (`RT/Slate/Private/Widgets/SViewport.cpp:292-295`, `RT/Engine/Private/Slate/SceneViewport.cpp:705`).
- É o mesmo modelo do ZH. Lá, o tradutor de janelas consome o clique antes dos tradutores de seleção e de ordem (`ZH/Core/GameEngine/Source/GameClient/MessageStream/WindowXlat.cpp:222-249`; ordem em `ZH/GeneralsMD/Code/GameEngine/Source/GameClient/GameClient.cpp:291-298`). O próprio código do ZH comenta o bug de "ordem atravessando a barra" (`WindowXlat.cpp:176-180`).

**Arrasto de caixa:** o modo de entrada atual (jogo e interface juntos) faz a viewport segurar o mouse só enquanto o botão está apertado (`RT/Engine/Private/PlayerController.cpp:6398-6413`; `SceneViewport.cpp:692-722,746`).
- Caixa que começa no mundo continua mesmo passando por cima de um painel, porque a soltura volta para a viewport.
- Caixa que começa num painel é consumida pelo painel. É a regra do ADR-0021 do Vanguard: o arrasto não começa em cima da barra.
- A posição do mouse continua valendo sobre o HUD: o movimento sobe até a viewport (`SceneViewport.cpp:895-903`), porque o botão do Slate não trata movimento (`RT/Slate/Private/Widgets/Input/SButton.cpp:466-473`). A rolagem pela borda segue funcionando.
- **Manter esse modo.** O modo só-jogo prende o mouse na viewport e os botões não receberiam clique (`PlayerController.cpp:6450`). O modo só-interface manda o jogo ignorar a entrada (6384).

**Minimapa:** vira um widget próprio que trata o clique sozinho, prende o mouse para o arrasto, acompanha o movimento e solta no fim. É o padrão do minimapa do WorldForge (`WF/SForgePanel.cpp:472,796-814`) e do radar do ZH, que é uma janela com desenho e entrada próprios (`ZH/Core/GameEngineDevice/.../W3DControlBar.cpp:62`, `ZH/Core/GameEngine/.../ControlBarCallback.cpp:65`). A lógica dos gestos fica no controle do jogador (`RTSPlayerController.cpp:399-466`), só que alimentada por chamadas do widget em vez de ler o mouse direto.

**Armadilhas, todas confirmadas no fonte:**
- **O botão rouba o teclado.** Depois de um clique, o Slate dá o foco do teclado ao widget focável mais fundo (`SlateApplication.cpp:5485-5505`). Com o foco no botão, Enter e Espaço apertam o botão de novo (`SButton.cpp:293-311`; `NavigationConfig.cpp:32-33`), e Espaço é a tecla "vai até o ataque" (`RTSPlayerController.cpp:462-465`). Correção: botões do HUD com `.IsFocusable(false)`. Como reforço, dá pra desligar a navegação por setas e Tab, que são teclas de câmera (`NavigationConfig.h:97-101`, `SlateApplication.h:1426`).
- **Clique direito e do meio atravessam o botão.** O botão do Slate só trata o esquerdo (`SButton.cpp:354`). O direito subiria até a viewport e desfaria a seleção. Correção: o fundo de cada painel consome os três botões.
- **Cursor errado em cima do HUD.** Sem cursor definido, a pergunta sobe até a viewport, que devolve o cursor do jogo; a mira apareceria em cima dos botões (`SViewport.cpp:268-271`, `SceneViewport.cpp:611-622`). Correção: os painéis declaram o cursor normal (`RT/SlateCore/Private/Widgets/SWidget.cpp:542-547`). O minimapa mantém o cursor de ordem, como hoje (`RTSPlayerController.cpp:264-268`).
- **Destaque de unidade embaixo do HUD.** Desligar o destaque quando o mouse estiver sobre o HUD, perguntando ao Slate se a raiz do HUD está sob o cursor (`SlateUser.h:103`) ou se ela está com o mouse em cima (`SWidget.h:1039`, o padrão `IsCursorOver` do WorldForge em `WF/SForgePanel.cpp:496-499`). Não existe `GetWidgetUnderCursor` no Slate; o equivalente é o caminho de widgets sob o cursor (`SlateUser.h:111`).
- **Widgets que sobrevivem à troca de mapa.** Um widget Slate posto direto na viewport não sai sozinho ao trocar de mapa (só sai com a viewport: `GameViewportClient.cpp:462-481`). Tem que tirar no fim da partida, como o WorldForge faz (`WF/ForgePlayerController.cpp:58-61`).
- **Fotos do teste.** A foto com interface usa a captura do Slate (`GameViewportClient.cpp:2392-2413`), então as fotos do teste continuam mostrando o HUD novo.

---

## 3. Texto nítido: o que o fonte confirma

- **Canvas, causa da letra borrada:** a letra é desenhada no tamanho da fonte recebida, vezes a escala do Windows (`RT/Engine/Private/UserInterface/CanvasItem.cpp:1193-1195`). Já o quadradinho de cada letra é esticado pela escala do item (1299-1300). Com a fonte do motor no tamanho de fábrica e `Scale = Size·1,333/tamanho de fábrica`, sempre havia reamostragem. A textura das letras usa filtro bilinear (`RT/SlateRHIRenderer/Private/SlateRHIFontTexture.cpp:55`), então qualquer meio pixel também borra. O negrito falso (duas cópias a 0,6 px) e a sombra a 1,5 px caíam sempre em meio pixel.
- **Por que "a fonte do Slate não aparecia no Canvas" (teste t2):** o Canvas exige que a fonte seja um asset de fonte (`CanvasItem.h:450`); senão ele não desenha nada (`CanvasItem.cpp:951-953`). A fonte padrão do Slate não é um asset.
- **O conserto da outra sessão está certo pelo fonte** (`G/Private/RTSHUD.cpp:77-86,143-157`, ainda sem commit): a fonte do motor com o tamanho exato e o negrito de verdade (o asset Roboto tem Regular, Bold, Italic e Light), escala 1, posição arredondada e sombra de pixel inteiro. As medidas das letras são inteiras (`RT/SlateCore/Public/Fonts/FontCache.h:631-647`), então com a posição inteira cada letra cai exata nos pixels.
- **O que ainda pode borrar ou sair errado no Canvas:**
  - qualquer escala de item diferente de 1;
  - uma posição calculada sem arredondar;
  - o tamanho fixo em pixel. Em 4K o texto fica pequeno (nítido, mas pequeno). A saída é multiplicar o tamanho da fonte pela curva da tela (`UserInterfaceSettings.cpp:87`), nunca a escala do item.
- **Não é a qualidade gráfica:** o Canvas desenha na resolução cheia da tela, depois da imagem do jogo ser ampliada (`GameViewportClient.cpp:2166`). Os presets que baixam a resolução não afetam o texto.
- **Slate:** desenha a letra já com a escala da tela (`RT/SlateCore/Private/Rendering/ElementBatcher.cpp:1256-1268`) e arredonda a posição (`RT/SlateCore/Public/Rendering/RenderingCommon.h:364-368`; `ElementBatcher.cpp:3814-3817`). Ficaria borrado se uma animação esticasse o texto depois; para isso existe a fonte por campo de distância (`ElementBatcher.cpp:3804-3810`).
- **Hinting:** a fonte padrão do Slate usa o hinting padrão (`RT/SlateCore/Private/Fonts/LegacySlateFontInfoCache.cpp:111-120`, `SlateFontRenderer.cpp:68-75`). O da fonte de asset do motor está no asset e não conferi.

---

## 4. Anel, barra de vida e marca de ordem

- **Ficam no Canvas por enquanto.** O Canvas é desenhado dentro da imagem do jogo (`GameViewportClient.cpp:2166-2181`), antes do Slate. Fica naturalmente **embaixo** dos painéis e na resolução cheia.
- **Barra de vida** em 2D na tela, como o ZH faz (`ZH/GeneralsMD/Code/GameEngine/Source/GameClient/Drawable.cpp:3779-3891`, mostrada só se a unidade está selecionada ou sob o mouse: 3789). O Vanguard rejeitou um widget por unidade no ADR-0021.
- **Anel e marca de ordem, defeitos que vêm da técnica:**
  - linha sem antisserrilhado (`BatchedElements.cpp:1074-1085`); a lista viva já aponta o anel cintilando;
  - sem profundidade, o arco de trás passa **por cima do casco**;
  - altura única de +25 cm, então em ladeira o anel flutua ou afunda (`RTSHUD.cpp:331`).
- **Alternativas para depois**, cada uma exigindo pesquisa antes porque muda o visual: uma marca no chão com profundidade, ou uma camada Slate com linhas antisserrilhadas.
- **Ao abrir pausa ou resultado**, esconder essa camada do Canvas. Resolve a barra que atravessa o "VITÓRIA" (`docs/BASE_JOGAVEL.md:171`).

---

## 5. Migração em passos pequenos

Cada passo é testável e mantém o teste automático passando. O teste acha botões por `ARTSHUD::FindButton` (`RTSAutoTest.cpp:114-124`) e converte de pixel da viewport para posição do Slate (95-106).

0. **(Em curso, outra sessão)** Texto do Canvas nítido. Teste: o teste automático inteiro e um recorte 1:1 de antes e depois.
1. **Esqueleto invisível.** Um widget raiz do HUD que não bloqueia o mouse, posto na viewport no começo e tirado no fim. Cada botão fica registrado por nome, como no WorldForge (`WF/SForgePanel.cpp:483-484,536`). `FindButton` procura primeiro nos widgets: acha o caminho até o widget (`SlateApplication.h:1623`), pega a posição dele e converte para pixel da viewport. Se não achar, cai no Canvas. Nada muda na tela; o teste passa igual.
2. **Pausa em Slate.** Com a partida pausada, o controle já ignora o mundo (`RTSPlayerController.cpp:215-234`). Os botões chamam o `OnButton` que já existe (131-165), com foco desligado e um fundo que consome o clique. Teste novo: Esc, clicar "continuar", conferir que despausou.
3. **Resultado, "montando a partida", carregando e objetivo em Slate.** Os passos `resultado` e `voltar_ao_menu` (`RTSAutoTest.cpp:499-515`) já testam. Junto, esconder a camada do Canvas com painel aberto.
4. **Menu principal em Slate.** O passo `jogar_skirmish` testa (273-277). O teste de clique manual do menu sai (`RTSPlayerController.cpp:167-182`).
5. **Barra do topo e avisos em Slate**, sem entrada. Teste: fotos lado a lado.
6. **Minimapa como widget próprio, junto com as regras de entrada do §2.** As funções que o controle e o teste usam (`IsOverMinimap`, `MinimapToWorld`, `WorldToMinimapScreen`) passam a ler a posição do widget, então `minimapa_camera` e `minimapa_ordem` (`RTSAutoTest.cpp:365-430`) seguem iguais. Testes novos:
   - com seleção, clicar em cima de um painel não dá ordem;
   - caixa que começa no mundo e termina sobre o painel seleciona;
   - Espaço depois de clicar num botão não aperta o botão de novo.

   No teste, soltar a captura do mouse antes de cada clique sintético, lição do WorldForge (`WF/ForgeUITest.cpp:99-105`).
7. **Conteúdo novo** (painel da fábrica, seleção, dicas) direto em Slate. Só depois da arte de referência do HUD, que hoje não existe (`docs/BASE_JOGAVEL.md:240`), e da pesquisa visual.

**Medição em cada passo:** `stat unit`, `stat slate` e o trecho de GPU "SlateUI". Ligar a invalidação global ou o `SInvalidationPanel` só se a medida mostrar custo. O desfoque de vidro (`SBackgroundBlur`) é o único item com custo de GPU que pode pesar; tem que ser medido na 1660.

**Arquivos citados do projeto:** `G/Private/RTSHUD.cpp`, `G/Public/RTSHUD.h`, `G/Private/RTSPlayerController.cpp`, `G/Private/RTSAutoTest.cpp`, `WF/SForgePanel.cpp`, `WF/ForgePlayerController.cpp`, `WF/ForgeUITest.cpp`, `C:\Users\luisf\newcnc\docs\adr\0021-hud-readability-layer.md`, `C:\Users\luisf\explosion\docs\JOGO.md`, `C:\Users\luisf\explosion\docs\BASE_JOGAVEL.md`.

---

## 4. O HUD e os controles de hoje

**Inventário do HUD e dos controles (1600×900, `Game/Tools/game.ps1:20`)**

As linhas do `RTSHUD.cpp` citadas são as do commit `e927a02`. A cópia de trabalho está sendo mexida pela correção da fonte, com cerca de 13 linhas a mais, mas os tamanhos não mudaram. O tamanho em px é pt×96/72. A largura dos textos foi medida com a Roboto da própria engine, então é aproximada.

## 1. O que aparece na tela

**Botão padrão** (`RTSHUD.cpp:146-155`): fundo escuro com 80% de opacidade, filete de 3 px à esquerda (laranja sob o mouse) e texto em 15 pt negrito (20 px), a 24 px da borda.

**Menu** (`RTSHUD.cpp:183-215`)
- **Painel:** 480×900 à esquerda (max(440, 30% da largura)) mais um degradê de 96 px.
- **Textos, todos em x=64:**
  - "RTS", 54 pt negrito (72 px), em y=180;
  - filete laranja de 64×3 em y=262;
  - "USA · CHINA · GLA", 13 pt negrito, em y=278;
  - "protótipo · skirmish local", 12 pt, em y=304.
- **Botões, com 352 px de largura:**
  - JOGAR SKIRMISH: y=450, 58 px de altura, com a nota em 11 pt em y=518;
  - NÉVOA DE GUERRA: LIGADA/DESLIGADA: y=546, 44 px, com a nota "depuração: F9…" em 10 pt em y=598;
  - SAIR: y=642, 58 px.

**Telas de espera**
- "Montando a partida…" (ou o erro) em 15 pt centrado em y=405, com o botão VOLTAR AO MENU (340×52) em (630, 495). Fica sobre um escurecido de 60% (`:172-179`).
- "CARREGANDO" em 26 pt negrito em y=420, com os pontinhos em 20 pt, sobre tela preta (`:219-227`).

**Partida**

| Peça | Posição e tamanho | Fonte | Quando | Fonte no código |
|---|---|---|---|---|
| Anel no chão | raio 1,3× o de colisão (4,68 m ≈ 160×97 px no zoom padrão); 2,5 px selecionado, 2 px sob o mouse ou inimigo, 1,5 px o seu | — | **sempre, em todas as unidades** | `:323-334` |
| Barra de vida | 54×5 px mais 1 px de borda na cor do time, a 4,3 m de altura e 12 px acima | — | ferida, selecionada ou sob o mouse | `:336-352` |
| Marca de ordem | anel de 2,8 m→0,7 m em 0,7 s | — | depois de cada ordem | `:356-364` |
| Caixa de seleção | verde a 7% com linha de 1 px | — | arrastando | `:233-242` |
| "ATAQUE ANDANDO" | no cursor +20,+16, ~134×18 px | 11 pt negrito | com o A armado | `:243-247` |
| Barra do topo | (560,0), 480×36. Relógio centrado em y=7; "Seus tanques N" em x=580; "Destruídos N" (com névoa) ou "Inimigo N" em x=920 | relógio 13 pt negrito, resto 11 pt negrito | sempre | `:366-382` |
| Objetivo | painel (500,166), 600×112: "OBJETIVO" / "Destrua os 6 tanques inimigos" / "…siga a seta vermelha" | 11 / 20 / 11 pt | 7 s | `:384-398` |
| Seta pro inimigo | triângulo de 26 px com linha de 3 px, a 46 px da borda; "N m" ou "base inimiga N m" | 10 pt negrito | sem inimigo na tela | `:400-489` |
| Minimapa | moldura (16,614), 270×270 (30% da altura); losango de 185 px de lado; pontos de 4,9 px; caixa da câmera de 1,5 px | — | sempre | `:613-618, 649, 690-693` |
| "Unidade sob ataque (Espaço vai até lá)" | (18,592), ~262×18 px | 11 pt negrito | 3 s | `:733-736`, `RTSPlayerController.h:77` |
| Linha de ajuda | centrada em y=866, ~804×17 px | 10 pt | primeiros 30 s, enquanto houver menos de 2 ordens | `:257-261` |
| Linha de depuração | (16,46), ~526×17 px | 10 pt | F10 | `:262-265` |
| Entrada | preto que abre em 1,2 s | — | início | `:276-281` |

**Pausa** (`:491-502`): escurecido de 55% sobre a tela toda. "PAUSADO" em 36 pt negrito (48 px) em y=288. Botões em x=630, todos com 340 px de largura:
- CONTINUAR: y=405, 54 px;
- SAIR PARA O MENU: y=475, 54 px;
- NÉVOA: … (F9): y=545, 44 px.

**Resultado** (`:504-519`, entra 3 s depois do golpe final)
- Faixa de 1600×216 em y=108, com 50% de opacidade.
- "VITÓRIA", "DERROTA" ou "EMPATE" em 44 pt negrito (59 px) em y=126.
- Linha de tempo, destruídos, perdidos e "replay salvo" em 12 pt em y=200.
- Botão VOLTAR AO MENU (340×54) em (630, 342).

## 2. Controles de hoje (`RTSPlayerController.cpp`)

**Mouse**

**Esquerdo**
- Clique na própria unidade: seleciona. Shift soma ou tira. Dois cliques em menos de 0,3 s pegam os iguais na tela (`:541-563`, `:630-655`).
- Com seleção:
  - clique no inimigo = Attack (`:564-574`);
  - clique no chão = Move (`:577-583`).
- Arrastar mais de 6 px: caixa, só com unidades próprias. Shift soma, e caixa vazia não mexe na seleção (`:586-628`).
- Com o A armado, o clique vira ataque andando no chão, mesmo em cima de um inimigo (`:528-539`).

**Direito**
- Clique rápido: desfaz a seleção e desarma o A (`:350-357`).
- Arrastar: rola a câmera; 180 px de distância = velocidade 1×, teto de 2,5× (`:332-349`).

**Meio e roda**
- Meio arrastado gira; Alt+meio inclina; dois cliques no meio voltam ao padrão (`:369-395`).
- Roda: zoom (`:363`).

**Borda da tela:** 16 px, 1,6×, e a vertical ×1,25. Não vale em cima do minimapa (`:321-328`).

**Minimapa** (`:399-466`)
- Esquerdo com seleção: andar, ou ataque andando com o A armado.
- Sem seleção, ou com o direito: a câmera pula e arrasta junto.
- Clique na moldura fora do losango é engolido.

**Mouse parado:** o anel acende e o cursor muda (`:259-278`). Mão sobre as suas unidades; mira sobre o inimigo com seleção ou com o A armado; cruz sobre o minimapa com seleção.

**Teclado**

| Tecla | O que faz | Onde |
|---|---|---|
| A | arma o ataque andando (precisa de seleção) | `:248-251` |
| S | Stop | `:252-256` |
| 1–9 e 0 | chamam o grupo; dois toques em menos de 0,4 s levam a câmera até ele | `:683-711` |
| Ctrl+número | grava o grupo. **Obs.:** sem seleção, grava o grupo vazio, ou seja, apaga | `:668-671` |
| Shift+número | soma a seleção ao grupo | `:673-681` |
| ↑ ↓ | rolam | `:319` |
| ← → | giram | `:320` |
| = − Num+ Num− PgUp PgDn | zoom | `:365-367` |
| Espaço | vai até o último "sob ataque" | `:461-465` |
| Esc | pausa local; não funciona depois do fim | `:207-214` |
| F9 | névoa, depuração | `:203-206` |
| F5–F8 | preset baixa/média/alta/ultra, sem nenhum aviso na tela de qual ficou | `:105-113`, `:120-129` |
| F10 | linha de depuração | `:114-117` |

**Botões do HUD** (`:131-165`): skirmish, quit, resume, menu, fog e fogsetting. Na partida o cursor fica preso à janela (`:57`).

## 3. Comandos que a simulação aceita (`SimCore/Public/Sim/Commands.h:20-28`, `World.cpp:389-474`)

| Comando | Como o jogador dá | Botão na tela? |
|---|---|---|
| SetGroup (1) | automático, antes de cada ordem (`RTSMatchSubsystem.cpp:817-838`) | não precisa |
| Move (2) | esquerdo no chão ou no minimapa | não |
| Attack (3) | esquerdo no inimigo; alvo inválido é ignorado. O próprio código anota "ataque forçado vem depois" (`World.cpp:443-446`) | não |
| Stop (4) | só a tecla S | **não** |
| AttackMove (5) | só A + clique | **não** |
| SetFog (6) | F9 ou o botão da pausa; só vale com `bAllowDebugCommands` (`World.cpp:349`) | sim (depuração) |

Nenhum comando fica sem tecla. Mas também não existe nenhum botão de ordem na tela: Stop e AttackMove só são descobertos pela linha de ajuda, que fica 30 s.

A simulação já calcula informação que o HUD não recebe. `Sim::UnitView` tem Order, OrderPoint, FireTarget, Engage, PathLeft e Cooldown (`World.h:75-96`). A ponte só repassa posição, giros, vida e "andando" (`FRTSUnitState`, `RTSMatchSubsystem.h:31-42`). O nome "Tanque básico USA" é lido (`RTSGameData.cpp:140`), mas não aparece em lugar nenhum.

## 4. O que falta pra ter "mais controles"

**Comandos novos na simulação**
- **Guarda / segurar posição.** O tanque parado persegue quem atira nele até 1,5× a visão (39,5 m × 1,5 ≈ 59 m) e depois volta (`World.cpp:24, 859-875`). Não há como mandar ficar no lugar. Referência: ZH `GUI_COMMAND_GUARD` e `GUARD_WITHOUT_PURSUIT` (`GeneralsGameCode/Core/GameEngine/Include/GameClient/ControlBar.h:175-176`).
- **Ataque forçado no chão ou em unidade própria** (Ctrl+clique). Referências: ZH `BEGIN_FORCEATTACK` (`Core/.../MessageStream/MetaEvent.cpp:164`); Vanguard ADR-0007, linha 11.
- **Pontos de passagem / fila de ordens.** Referência: ZH `BEGIN_WAYPOINTS` (`MetaEvent.cpp:168`) e `GUI_COMMAND_WAYPOINTS` (`ControlBar.h:179`).
- **Fábrica:** produzir, cancelar e ponto de encontro. Referências: `ControlBar.h:169-170, 184`; `BASE_JOGAVEL.md:120-126`.
- **Pausar, retomar, render-se e sair como comando de rede** (`BASE_JOGAVEL.md:108`). No ZH, em multiplayer o Esc não pausa e o botão "Reiniciar" vira "Render-se" (`GeneralsMD/.../Menus/QuitMenu.cpp:384-400`).

**Controles só do nosso lado, sem mexer na simulação**
- Selecionar todos (ZH `SELECT_ALL`, `MetaEvent.cpp:139`).
- Iguais por tecla (`SELECT_MATCHING_UNITS`, `:132`).
- Próxima unidade (`:133`).
- Ir pra base/fábrica (`VIEW_COMMAND_CENTER`, `:141`).
- Marcadores de câmera (`SAVE_VIEW` / `VIEW_VIEW`, `:76-91`).
- Opções (`OPTIONS`, `:154`).

**Informação que falta na tela**
- **Painel da seleção.** Com 1 unidade: nome, barra e vida "atual/máx"; com várias: quantidade e vida média (Vanguard ADR-0021, linhas 47-53). Também o estado da ordem e a recarga, que exigem abrir Order e Cooldown na ponte.
- **Botões de ordem:** Atacar andando, Parar, Guarda.
- **Grupos visíveis.** Hoje eles só existem na memória do controle (`RTSPlayerController.h:108`).
- **Painel da fábrica:** fila, %, botão com ícone e bandeira do ponto de encontro (`BASE_JOGAVEL.md:125-126, 176, 237`).
- **Aviso "tanque pronto"** (`BASE_JOGAVEL.md:52, 81`).
- **Tela de opções** com o preset visível (`BASE_JOGAVEL.md:172`; `JOGO.md:66`).
- **Pausa sem "Opções" e sem "Render-se".** O resultado só tem "Voltar ao menu".
- **Cor do inimigo fixa no jogador 1** (`RTSHUD.cpp:381`; `BASE_JOGAVEL.md:109`).
- **Tamanhos em px fixos.** Só o minimapa (30% da altura) e o painel do menu (30% da largura) crescem com a resolução.

## 5. O que tapa o campo ou polui (1600×900 = 1,44 Mpx)

| Peça | Área | Observação |
|---|---|---|
| Barra do topo | 480×36 = **1,2%** | permanente |
| Minimapa | 270×270 = **5,1%** (30% da altura, 17% da largura) | o losango útil é só 2,4%. Os 4 cantos (**2,7%**) são painel escuro a 82% que tapa o mundo e engole o clique (`RTSPlayerController.cpp:421-424`) |
| Linha de ajuda | ~804×17 = **0,95%** | primeiros 30 s; texto de 10 pt (13 px) |
| Linha de depuração | ~526×17 = **0,6%** | só com F10 |
| Objetivo | 600×112 = **4,7%** | 7 s, antes do contato (que vem entre 30 e 65 s, `JOGO.md:499`) |
| Resultado | 1600×216 = **24%** | anéis e barras continuam por baixo, a 50% (`BASE_JOGAVEL.md:171`) |
| Pausa | 100% a 55% | — |

- **Fixo na tela durante a partida:** cerca de 6,3%, mais as peças do mundo.
- **Anéis, o maior poluidor.** Estão em todas as unidades, as suas e as inimigas, o tempo todo. São ~160×97 px no zoom padrão, contra ~135 px do tanque (câmera em `RTSCameraPawn.h:55-60`). Com as vagas do grupo a 8,4 m eles se sobrepõem (`JOGO.md:598`). É o defeito 3, "anéis grossos" (`BASE_JOGAVEL.md:24`). A versão final já combinada é só no selecionado ou sob o mouse, sem anel no inimigo (`BASE_JOGAVEL.md:145-149`).
- **Barra de vida.** Tem 7 px de altura, contra 3 px no ZH (`GeneralsMD/.../GameClient/Drawable.cpp:2694`, borda de 1 px em `:3884`). Ela fica sempre ligada em qualquer tanque ferido: essa regra é do Vanguard (ADR-0021, linhas 29-33), não do ZH, onde a barra aparece só no selecionado ou sob o mouse (`BASE_JOGAVEL.md:365`). Depois de um combate, todo mundo fica com barra.
- **Texto pequeno.** O mínimo é 10 pt (letra maiúscula de ~9 px): linha de ajuda, distância da seta, nota do menu.

## 6. O que os documentos já anotam sobre HUD

**`docs/BASE_JOGAVEL.md`**
- **Minimapa e névoa:**
  - alertas e "tanque pronto": `:52`;
  - decisão pendente sobre o minimapa (sempre ligado ou só com radar): `:53`, `:307`;
  - vazamentos de informação pela névoa: `:72-75`;
  - fábrica fantasma e aviso de pronto: `:81`;
  - minimapa na sala de espera: `:84`.
- **Rede:**
  - quem está atrasando e barras de carregamento por jogador: `:103-104`;
  - pausar, render-se e sair como comandos: `:108`;
  - HUD pra N jogadores: `:109`.
- **Fábrica e seleção:**
  - bandeira do ponto de encontro e % da produção: `:125-126`;
  - seleção final: `:145-149`.
- **Visual do HUD:**
  - paleta de cores dos jogadores no HUD: `:166`;
  - marca mínima no inimigo: `:167`;
  - resultado e 3-2-1 pra N jogadores, com letra nítida: `:171`.
- **Telas, cliques e arte:**
  - menu e opções: `:172`;
  - HUD final (painel da fábrica, sala de espera, conexão, cursor e fonte final, tela de carregar com arte): `:175-179`;
  - efeito não pode roubar o clique: `:181`;
  - ícone do tanque: `:237`;
  - arte de referência do HUD, do minimapa e da névoa, hoje sem nenhuma aprovada: `:240`.
- **Referências AAA:** tabela em `:365`.

**`docs/JOGO.md`**
- menu (Skirmish, Multiplayer, Replays, Opções, Sair): `:59-64`;
- opções do jogador: `:66`;
- front-end com CommonUI, e os painéis Slate viram gaveta de depuração: `:247`;
- "HUD final" na lista do depois: `:428`;
- frente "HUD e controles: barra de comando, minimapa, grupos, atalhos": `:437`;
- estado de hoje e controles: `:514-532`, `:559-562`;
- anéis se sobrepondo: `:598`;
- vazamentos do HUD fechados: `:622`.

**Referência pro desenho da barra:** no Vanguard o dono rejeitou painéis soltos. A versão aceita é uma barra única embaixo, com espaços fixos (radar | info da unidade | botões de comando), em que o clique em qualquer parte do HUD não chega ao mundo (`newcnc/docs/adr/0021-hud-readability-layer.md:127-146`).
