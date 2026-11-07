# As telas do jogo: o menu novo, o SCRIPT e o LAB nos testes

Desde 01/11/2026 o jogo abre nas **telas aprovadas** (menu com o radar, skirmish, multiplayer, opções), primeiro como as páginas HTML
do protótipo no navegador embutido da Unreal e, desde 03–06/11, **refeitas nativas em Slate** no mesmo plugin `Game/Plugins/RTSShell`
(o lado do jogo em `Game/Source/RTSGame/*/RTSShellGame.*`). O menu antigo, desenhado no HUD, ficou só pro teste automático. Dono: a
sessão das telas (`docs/concept/ui/TELAS.md`).

**06/11: todas as telas fora da partida são nativas** (o menu, o skirmish, a sala da LAN, a lista do multiplayer, as opções, o
SCRIPT); nenhuma usa mais o navegador. Como foi feito e como fotografar: "A noite de 05→06/11", mais abaixo.

**06/11 à tarde (a lista dele depois do 1º teste): as telas da partida também são nativas, e a sala da LAN é a do skirmish.** Detalhe
em "A tarde de 06/11", mais abaixo. Em resumo:
- **As posições espelhadas:** o losango da sala estava girado 180° em relação à câmera da partida; a sala, as miniaturas, a prévia
  do multiplayer e os mockups giram junto agora.
- **A sala da LAN:** cada campo abre a mesma janelinha do skirmish e manda o valor exato pela rede (a vaga aberta, fechada ou IA
  com o nível, a facção, a cor das 24, o time, a base).
  - O anfitrião troca o mapa (CHANGE MAP), põe IA na vaga clicada e tira quem entrou (✕).
  - O chat da sala, o READY de quem entrou e a contagem de 5 s do START, que cancela se algo mudar.
  - O protocolo da rede subiu pra 0005 (`docs/MULTIPLAYER.md`).
- **As telas da partida (o mockup `screens.html`):** o carregando (também no filme do carregar do mapa), o Esc, VICTORY/DEFEAT com
  a tabela e o gráfico, e os avisos da rede, num widget só (`SRTSMatch`) por cima do jogo; o HUD não desenha mais as velhas
  enquanto a nativa está.

## Pra testar o seu trabalho

**Uma cena de teste à mão** (o que antes era `game.ps1 view -Scenario <nome>` e clicar em Skirmish):

```
powershell -File Game\Tools\game.ps1 view -Scenario <nome>
```

No menu, embaixo da lista, o bloco **DEVELOPMENT** tem o **SCRIPT** com a cena armada (`▸ <nome>`). Clique nele e aperte
**Enter** (ou RUN SCENE). A tela do SCRIPT lista todas as cenas do `Game/Config/RTS/Scenarios.json` (a mais nova primeiro), com busca
por nome, mapa ou texto, e roda qualquer uma; uma cena nova aparece sozinha, sem compilar. Duplo clique numa linha também roda.

**O laboratório:** o **LAB**, no mesmo bloco, abre o mundo de testes de sempre (`RTSLab.h`).

**O teste automático** (`game.ps1 view -Folder ...`, que usa `-RTSAutoTest`): nada muda. Ele abre no menu antigo e clica no
Play Skirmish como antes.

**Se as telas novas atrapalharem:** `-RTSOldMenu` na linha de comando abre o menu antigo.

## O que as telas fazem de verdade no jogo

- **SKIRMISH:** a sala monta a partida: até 8 vagas (você e as IAs), com a facção, a cor (as 24 da tela), o time e a base de cada
  uma, e o dinheiro inicial. Mapas: SALT FLATS (`World_TestSalt2`), DUNE SEA (`World_TestDunesSim`) e BROKEN HILLS
  (`World_TestHills`). A dificuldade da IA fica guardada, mas a IA de hoje tem um nível só; as superarmas ainda não mudam nada.
- **MULTIPLAYER:** a LAN de verdade (as salas da rede, HOST e JOIN) e **a sala da LAN igual à do skirmish** (06/11).
  - Cada campo com a janelinha e o valor exato (a vaga, a facção, a cor das 24, o time, a base) e o mapa (o anfitrião).
  - O ✕ de tirar quem entrou, o ping, o chat, o READY de quem entrou e a contagem de 5 s do START.
  - As superarmas ficam fora da sala da rede: a partida ainda não lê a escolha (nem no skirmish).
- **OPTIONS:** o preset muda a qualidade de verdade (LOW, MEDIUM, HIGH, ULTRA; o CLASSIC ainda vai na média). Os outros itens
  ficam guardados pra quando o jogo tiver cada um.
  - **HUD (05/11, pedido dele pela sessão do HUD), na aba GAME:** BUTTON LABELS (CLEAN / HOTKEYS & PRICES) e EMPTY SLOTS
    (HIDDEN / SHOWN), padrão CLEAN e HIDDEN.
    - O APPLY grava no GameUserSettings, seção `[RTS]`: `HudCleanSlots` e `HudHideEmptySlots` (`True`/`False`). O HUD relê a
      cada segundo.
    - A ponte: `window.ue.shell.setoption(chave, valor)` / `getoption(chave)` (`URTSShellBridge`, só chaves de letras e números).
      Toda vez que as opções abrem, a página lê o que está gravado.
    - Com as casas limpas o preço só aparece na dica, então o TOOLTIPS OFF fica riscado (indisponível) e volta pro NORMAL.
    - Na tela de opções nativa, as duas linhas vão junto, com as mesmas chaves.
- **QUIT** fecha o jogo.
- O **LAB** e o **SCRIPT** só aparecem no desenvolvimento (o editor, `game.ps1 view`, ou `-RTSLab`): o pacote do pai não mostra.
- As telas da partida (Esc, resultado, avisos de rede, carregando) ainda são as antigas.

## Pra mudar uma tela

**Desde 06/11 as telas são nativas:** mudar uma tela é mudar o widget dela (`Source/RTSShell/Private/SRTS*.cpp`) e compilar.
- O mockup continua sendo a referência do desenho: muda-se primeiro no protótipo, ele aprova, e o nativo copia.
- As medidas saem do Chrome (`Tools/measure_*.js`).
- A comparação: `Tools/compare_skirmish.py` e `Tools/side_by_side.py`.
- As fotos no jogo: `Tools/native_shot.ps1`.

O jeito antigo (as páginas no navegador embutido) vale só pra história:
- As páginas eram editadas no protótipo (`captures/concept/ui/shell/`, o rascunho; a cópia aprovada em `docs/concept/ui/menu/`).
- Iam pro plugin com `python captures/concept/ui/shell/tools/export_game.py`.
- O jogo respondia pelo `window.ue.shell` (`URTSShellBridge`) e pelos JSON em `https://rts.ui/api/` (`RTSShellGame.cpp`). As telas
  nativas usam os mesmos `FRTSShell::Handlers()` (os comandos e o `Api`), sem o navegador.

## DECISÃO DE 03/11: REFAZER NATIVO (em andamento; o plano aprovado: "Pode seguir")

O site dentro do jogo (o navegador embutido, plugin `RTSShell`) foi um erro: o protótipo HTML era só pra ele ver. As telas estão sendo
**refeitas nativas no código do jogo**, idênticas ao mockup aprovado (`docs/concept/ui/menu/`). O plano que ele aprovou:
- Slate em C++ no plugin `RTSShell`, sem CommonUI: uma pilha de telas própria, e o Esc volta.
- O radar em vértices próprios da Slate (leques pré-multiplicados) mais texturas pré-calculadas (`Tools/make_menu_assets.py`, a
  mesma conta do mockup).
- As fontes TTF no plugin (`Resources/Fonts`, OFL).
- O clique e a música como som do jogo, com o abaixar pelo mixer.
- A ligação com o jogo reaproveitada do `RTSShellGame.cpp`.
- No fim, o CEF sai inteiro.

A ordem: menu → skirmish → multiplayer/sala da LAN → opções → SCRIPT/LAB → tirar o CEF. Cada tela é mostrada a ele lado a lado com o
mockup, e só se segue com o ok dele. As telas da partida têm plano à parte, com a sessão do HUD. Memória: "UI nativa; decisão grande
antes".

### A tarde de 06/11: as posições, a sala da LAN e as telas da partida (a lista dele depois do 1º teste)

**O pedido (as frases dele):**
- "a tela de loading após o START MATCH continua com o loading de jogo de criança";
- "o Esc dentro do jogo também";
- "nem defeat, nem victory estão com a nova";
- "as posições que eu escolhi [...] parece que ficou espelhado";
- na sala do host:
  - "apertar o OPEN [...] adiciona uma IA sempre";
  - "qualquer tecla [...] não aparece dropdown";
  - "não tem tela de trocar mapa";
  - "faltou um chat".

**As posições espelhadas: o giro de 180°.**
- **A causa:**
  - a câmera da partida olha com yaw 45: a frente da tela é +X+Y e a direita é Y−X (`RTSCameraPawn.DefaultYawDeg`); o minimapa do
    HUD (`WorldToMinimapScreen`) segue o mesmo;
  - a sala desenhava ao contrário: `(DX + (x−y)/√2·k, DY + (x+y)/√2·k)`;
  - os dados e a ordem das bases estavam certos.
- **A conta agora:** `(DX + (y−x)/√2·k, DY − (x+y)/√2·k)`; as imagens dos mapas são assadas giradas 225° (π/4 + π), não mais 45°.
- **Onde mudou:**
  - a sala nativa: `ToDiamond` em `SRTSSkirmish.cpp`;
  - a prévia do multiplayer: `ToP` em `SRTSMultiplayer.cpp`;
  - os mockups (`docs/concept/ui/menu/skirmish.html`, `multiplayer.html`, `screens.html`) e as cópias em `Resources/UI`;
  - os exportadores: `export_skirmish_maps.js/.mjs` e `export_multiplayer_maps.js/.mjs`. O do multiplayer é novo e manda as imagens
    por POST como o da sala, porque a resposta do Runtime.evaluate não aguentava a prévia.
- A bússola das bases e os times NORTH VS SOUTH saem da posição na tela: viraram junto, sozinhos.

**A sala da LAN = a sala do skirmish (`SRTSSkirmish` no modo LAN).**
- **Antes:** cada clique fazia uma coisa fixa. O OPEN punha IA na 1ª vaga livre, a IA saía no clique, a cor e o time andavam em
  sequência.
- **Agora:**
  - a tela é a mesma do skirmish;
  - o `LanSync` compara a sala com a última leitura e manda cada mudança como o comando exato da rede: `slot:N=open|closed|ai:D`,
    `fac:N=usa`, `color:N=#rrggbb`, `team:N=T`, `base:N=B`;
  - quem pode mexer em quê (`LanMayEdit`): o anfitrião, a vaga dele e as da IA (e as abertas e fechadas); quem entrou, só a dele.
    O resto não abre a janelinha (o `.lock` do mockup).
- **O resto da sala:**
  - o mapa (o CHANGE MAP do anfitrião, `map:<mapa>`);
  - o ✕ de tirar quem entrou (`kick:N`), o ping e a coroa do anfitrião;
  - o chat embaixo das regras (`chat:<texto>`). As linhas do sistema saem da diferença entre as leituras: entrou, saiu, pronto;
  - o READY de quem entrou (`ready:1/0`);
  - a contagem de 5 s do anfitrião (`countdown:N`), que todos veem e que cancela se a montagem mudar.
- As superarmas seguem fora da sala da rede: a partida não lê a escolha (nem no skirmish).
- A sala da LAN não sobe mais 52 px: a barra do FILL/CLEAR e o CHANGE MAP ficam, apagados pra quem entrou (o `.guest` do mockup).
- **O lado do jogo:**
  - o `RTSNetSession`, com o protocolo 0005 (`docs/MULTIPLAYER.md`);
  - o `RTSShellGame::Lan` (os comandos) e o `lobby.json`: cada membro com `fac`, `base`, `diff`, `colorAuto`, `ping` e `ready`; mais
    `closed`, `countdown`, `chat` e `chatTotal`;
  - na partida, as vagas da rede entram pelo mesmo caminho do skirmish local: `NetSession->MatchSlots()` no lugar do
    `RTSShellGame::LocalSlots()`, uma linha no `RTSMatchSubsystem.cpp`.

**As telas da partida (`SRTSMatch`, um widget com quatro modos; o mockup `docs/concept/ui/menu/screens.html`).**
- **O carregando:**
  - o que mostra: a operação, a sua facção grande (o brasão, o nome e o lema), uma linha por jogador com as células (a IA pronta), o
    mapa com as bases escolhidas e o total;
  - onde roda: no MoviePlayer (`FRTSShell::MakeLoadingScreen`, chamado pelo `RTSGameModule` no PreLoadMap) e depois por cima do jogo,
    até a cena ficar pronta e todos chegarem;
  - o filme pinta noutra thread. Ele lê só a largada (`FRTSShell::SetIntro`, que a sala preenche ao começar), e as imagens ficam
    carregadas antes, na thread do jogo (`SRTSMatch::Warm`).
- **O Esc:**
  - no skirmish: RESUME, OPTIONS, RESTART e QUIT TO MENU;
  - o OPTIONS abre as opções por cima, e o BACK volta pro Esc;
  - o RESTART repete a mesma montagem, ou a mesma cena do SCRIPT;
  - na rede: PAUSE FOR EVERYONE (a pausa de 13/10, que o mockup não tinha) e SURRENDER com a pergunta.
- **O resultado:**
  - o título e a tabela: VICTORY/DEFEAT, com as contas do placar do Tab (unidades feitas e perdidas, prédios, dinheiro ganho, baixas,
    pontos), os melhores em âmbar e os derrotados apagados;
  - o gráfico ARMY/ECONOMY/KILLS. O RTSShellGame amostra o histórico de 5 em 5 s de partida e no fim; o exército é a soma do preço das
    unidades vivas;
  - os botões:
    - VIEW MAP (o mapa sem nada por cima, com BACK TO RESULTS);
    - BACK TO LOBBY (na rede, a sala; no skirmish, a sala do skirmish);
    - REMATCH (só no skirmish);
    - MAIN MENU;
  - fora: o "+1 STAR" do mockup (as estrelas não existem) e as marcas dos combates no gráfico.
- **Os avisos da rede:**
  - esperando quem sumiu (o anel dos 60 s, o ping, os votos e DROP · v/n);
  - a volta da pausa;
  - a pausa de alguém (com o RESUME);
  - o anfitrião mudo;
  - a conexão perdida;
  - a dessincronia (com o passo);
  - (06/11 à noite, o caso 7, de qualquer partida) a partida que não monta: "MATCH STOPPED / COULDN'T START", a linha "THE MATCH
    COULD NOT BE SET UP", o motivo do log (em maiúsculas, cortado com "…" na largura dos botões, 760) e MAIN MENU. Acontece com o mapa
    sem posições pra todos, ou com uma cena com unidade ou prédio que não existe. O `MatchTick` põe o aviso quando a partida não roda
    (`!IsRunning`) e o controle tem o erro (`GetError`); o `match.json` manda o `net.case` 7 com o `error`. Antes, o texto velho do
    HUD ficava escondido embaixo do carregando, e o jogo parecia preso.
- **Quem decide qual tela:**
  - o `RTSShellGame` (o `MatchTick`), pelo mesmo estado que o HUD usava: `IsReady`/`IsWaitingForPlayers`, `Result` + 3 s, o estado da
    rede e `IsPauseMenuOpen`;
  - os botões chamam o mesmo `OnButton` do controle;
  - o HUD não desenha as velhas enquanto a nativa está: 4 ifs no `RTSHUD.cpp`, pelo `RTSShellGame::IsMatchOverlay`.
- **Os lemas das facções (aprovados em 07/11):**
  - "PRECISION OVER NUMBERS" (USA) é o do mockup;
  - "STEEL AND ARTILLERY" (Rússia), "MASS AND RESOLVE" (China) e "STRIKE AND VANISH" (GLA), no mesmo tom.
- **As trocas em arquivos de outras sessões entram só na vez de compilar (`Tools/patch_shared.py`):**
  - o que entra: o RTSShellGame, os ifs do RTSHUD e a tela de carregar do RTSGameModule;
  - por quê: o RTSShellGame usa a API nova do plugin. Antes do deploy ela não existe, e uma compilação de teste de outra sessão
    quebraria;
  - o `revert` desfaz só o que o último `apply` trocou, na ordem inversa.
- **As fotos de teste:**
  - as telas da partida: `native_shot.ps1 -Page "match.html?m=loading|pause|result|net" -Extra "-RTSMatchFake=<fixtures/match_*.json>"`;
  - a sala da LAN: `-RTSLobbyFake=<fixtures/lobby_host.json|lobby_guest.json>`;
  - as referências do mockup, fotografadas com o `cap2.mjs`: `skirmish.html?mp=1&src=lan&iamhost=1|0` e
    `screens.html?s=load|esc|res&r=win|lose|net`.

**O defeito que derrubou os testes de todos (18:14 a 20:21) e a regra que ficou.**
- **O que aconteceu:**
  - a tela de carregar nova pinta na thread do MoviePlayer (a `SlateLoadingThread`);
  - o kit de texto das telas moldava pelo HarfBuzz (o FullShaping, pelo kerning do GPOS das fontes do mockup);
  - a Unreal só cria a fonte do HarfBuzz na thread do jogo (`FHarfBuzzFontCache::CreateFont`, `check(IsInGameThread())`, a cada
    moldagem). Todo carregar de mapa caía, os testes das outras sessões também.
- **A correção:**
  - fora da thread do jogo, o `RTSShellUI` molda só com o kerning, sem HarfBuzz e sem a lista guardada (o caminho do STextBlock);
  - as fontes e as imagens da tela de carregar são carregadas antes, na thread do jogo (`SRTSMatch::Warm`, no `MakeLoadingScreen`).
- **A regra:** o que pinta no MoviePlayer não toca cache nem serviço da thread do jogo. Antes de instalar, provar num carregar de mapa
  de verdade.
- **Como foi achado:** pelo `lan_room_test.ps1`, que abriu a partida da rede. Mas isso foi depois de instalado pra todos.

**A caça às telas velhas (06/11 à noite; ele: "após o seu loading, ele entra em mais uma tela de loading de criança [...] a tela de
loading tem que ser a única que você fez [...] cace por todas essas telas feias e antigas e elimine-as da face da terra").**
- **A causa:**
  - com o `-RTSAutoTest` ou o `-RTSOldMenu` (o `Wanted()` falso), as telas da partida nativas não se instalavam;
  - depois do filme novo, o HUD desenhava o "LOADING" velho, e no fim o resultado, o Esc e os avisos velhos.
- **O que mudou, em todo modo:**
  - as telas da partida (o `InstallMatch`, separado do menu novo);
  - o filme do carregar é sempre o novo: o widget velho saiu do `RTSGameModule.cpp`;
  - o "LOADING" do HUD saiu do código. Se a tela nativa ainda não estiver por cima num quadro, o HUD pinta só o escuro dela.
- **O teste automático:**
  - ele clica nos botões pela posição. O `URTSAutoTest::ButtonCenter` agora acha o botão nativo de mesmo nome ("menu", "lobby") pela
    ponte `FRTSShell::MatchButtonCenter` (o centro em pixels do viewport, o caminho de volta do `ToSlate`);
  - o `match_flow_test.ps1` clica assim, de verdade, pela Slate.
- **O menu antigo do HUD também saiu (a principal, a lista e a sala da LAN):**
  - o teste automático usa o menu nativo. Quando o botão velho não existe mais, o `URTSAutoTest::ButtonCenter` devolve uma marca, e o
    `QueueClick` chama a função do botão direto no controle (o mesmo `OnButton` que o clique chamava). Os roteiros não mudaram;
  - o `-RTSOldMenu` deixou de existir: o `Wanted()` só pede as telas no disco;
  - as provas: `Tools/auto_menu_test.ps1` (o roteiro `base` com a cena `resultado`, direto, dentro de uma vez da fila) e o
    `Tools/lan_auto_test.ps1` (o roteiro `lanhost` com um robô, o anfitrião e o robô dentro da vez; o robô manda a fala do chat).
    Em 07/11 a LAN deu 58 passos ok e nenhuma falha em 330 s, sem dessincronia: o menu, a sala nativa (cor, névoa), o START, o
    carregando, o chat nos dois sentidos, a base de USA, a produção, as ordens e o minimapa.
    - Ela achou um defeito do protocolo 0005: na sala da rede, todo humano entrava em "random" (o anfitrião caiu de China). Agora
      cada um entra com a sua facção salva, como na sala do skirmish (`docs/MULTIPLAYER.md`).
- **A partida que não monta (o "Setting up the match…" / "Couldn't set up the match" e o "RETURN TO MENU" velhos do `DrawHUD`):**
  - virou o aviso nativo, o caso 7 (acima);
  - o 4º if do `RTSHUD.cpp` fica no `!IsRunning` do `DrawHUD`: só o escuro;
  - a prova: `match_flow_test.ps1 -Scene nao_existe -OldMenu -Tag erro`. A partida começa pelo botão, a cena não existe e a montagem
    falha; o teste fotografa o aviso e clica no MAIN MENU pela ponte. As fotos ficam em `partida_real_menu_antigo_erro/`.
- **Código que nunca mais roda, no `RTSHUD.cpp` da sessão do HUD (avisada; apagar é com ela):**
  - o DrawMenu e o DrawLanMenu (o menu antigo);
  - o DrawPause, o DrawResult e o DrawNetStatus;
  - o texto e o botão do `!IsRunning` no `DrawHUD`.
- **O que sobra de velho, de outra sessão:** o letreiro "DEFEATED / You are now watching the match" do `RTSHUDEconomy.cpp`. Aparece
  quando o jogador cai e a partida continua, e não tem mockup aprovado. Fica pra quem é o dono do espectador.

**Os testes de ponta a ponta (sem clicar, na vez da fila).**
- `Tools/lan_room_test.ps1 [-Seconds 150]`: a sala da LAN com um robô da rede.
  - Este PC hospeda com as telas nativas (`-RTSMpTestHost`) e segue o roteiro `fixtures/room_test_host.txt` pelo `-RTSRoomTest`:
    cada linha "segundos|comando", e o log escreve a sala a cada mudança.
  - Um robô sem placa (`-RTSNetBot -RTSNetJoin=127.0.0.1`) entra e aperta o READY sozinho.
  - Provado em 06/11:
    - a IA na vaga pedida e no nível pedido;
    - a facção, a cor, o time e a vaga fechada;
    - o mapa trocado com o robô dentro (o robô abriu o World_TestHills);
    - o chat chegando;
    - a contagem de 5 a 1 e a partida começando com 3.
- `Tools/match_flow_test.ps1`: as telas da partida numa partida de verdade.
  - O SCRIPT roda a cena `resultado` sozinho (`-RTSScriptAutoRun`). O `-RTSMatchTest=<pasta>` escreve cada tela da partida no log e
    tira a foto do jogo inteiro em cada uma.
  - Aperta o Esc de verdade, depois o RESUME, e no resultado o BACK TO LOBBY.

### A noite de 05→06/11: todas as telas fora da partida nativas ("todas telas prontas", ele testa de manhã)

Ele pediu antes de dormir: "continue esse trabalho foda ai de qualidade, todas telas prontas hein! quando eu acordar eu testo tudo".
Sem commit até o ok dele. As telas, cada uma num `SLeafWidget` do plugin `RTSShell`, desenhada pelo kit `RTSShellUI`:

| Página do mockup | Tela nativa | O que faz no jogo |
|---|---|---|
| `menu.html` | `SRTSMenu` (03/11, aprovado) | o menu com o radar e a abertura |
| `skirmish.html` | `SRTSSkirmish` | a sala do skirmish, o navegador de mapas, a dica das cores |
| `skirmish.html?mp=1&lan=1` | `SRTSSkirmish` no modo da rede (`SetLan`) | a sala da LAN: o estado do `api/lobby.json` a cada 0,5 s |
| `multiplayer.html` | `SRTSMultiplayer` | a lista das salas da LAN (`api/lan.json` a cada 1 s), HOST, JOIN |
| `screens.html?s=opt` | `SRTSOptions` | as 5 abas, os presets com os quadros estimados, o cartão, o APPLY |
| `script.html` | `SRTSScript` | as cenas de teste do `Scenarios.json`, a armada, o RUN |

- **A sala da LAN** usa a mesma sala nativa: sem a barra do FILL/CLEAR (tudo abaixo do mapa sobe 52 px), só o dinheiro nas regras,
  o nome com o YOU âmbar e o pronto (o círculo verde), as setinhas só onde a rede de hoje deixa mexer. Os cliques viram os comandos da
  LAN (`color:<n>`, `team:<n>`, `addai`, `kick:<n>`, `money:<v>`, `start`, `leave`). Sem arrastar fichas (a rede escolhe as bases).
- **A lista** mostra o ONLINE e o DIRECT IP apagados (ainda não existem), como a página do jogo. A prévia do mapa e o ícone saem do
  próprio mockup (`Tools/export_multiplayer_maps.js` e `.py` → `Resources/Skirmish/maps/<mapa>_mppv.png` e `_mpicon.png`).
  - O ping é o que o jogo dá (2 ms na LAN). A página fazia o número andar sozinho a cada 5 s, um número inventado; aqui não.
- **As opções:** as tabelas dos 65 itens saem do mockup por código (`Tools/gen_options_tables.mjs` e `.py`), sem transcrever à mão.
  - O layout é o fluxo do CSS (o grupo com 24 acima, a linha de 48 + 2 com 5 abaixo, as margens que se fundem), conferido pelas
    posições medidas (`Tools/options_layout.json`).
  - O APPLY manda a qualidade, grava o HUD e **os volumes**: os deslizantes do SOUND mexem de verdade no `SM_UserVolumes`.
    - Cada classe (`SC_Master`, `SC_Music`, `SC_Effects`, `SC_Voice`, `SC_UI`, `SC_Ambience`) vai relativa ao padrão do item. No
      padrão nada muda do som aprovado: a música 0,55, o clique 0,6, o resto 1. O MASTER multiplica tudo (o 80 = 1).
    - Enquanto arrasta, toca a prévia. O BACK sem APPLY volta ao guardado. Os valores ficam no `[RTS]` (`Vol_master`...).
  - **Defeito da página do jogo que a nativa corrige:** o BACK e o Esc das opções não faziam nada no jogo. A linha que volta pro menu
    estava num trecho só do protótipo e foi cortada no export. Por isso o APPLY também não saía da tela.
- **O SCRIPT:** a página chamava `window.ue.shell.navigate` no BACK, que não existe na ponte. A nativa volta pro menu.
  - Pra foto com uma cena armada sem rodar a cena: `-RTSScriptArmed=<nome>` (o `-RTSScenario` roda).
- **Fotos de teste:** `native_shot.ps1 -Name <n> -Page <página> [-Extra '<args>']` abre a tela pela foto genérica
  (`-RTSScreenShot`).
  - Os dados de mentira da rede ficam em `Tools/fixtures/` (os mesmos do Chrome): `-RTSLanFake=...` e `-RTSLobbyFake=...`.
  - As abas das opções: `-RTSOptTab=<0..4>`.
- **As referências do Chrome** são as páginas do jogo (`Resources/UI`) com a ponte e os dados de mentira injetados antes da página.
  Elas ficam em `captures/concept/ui/shell/shots/native/mock_*.jpg`; as medidas em `Tools/measure_multiplayer.js`, `measure_options.js`
  e `measure_script.js`.
- **Lição do arquivo unificado (unity build):** a Unreal junta os `.cpp` do módulo num arquivo só, e os namespaces anônimos viram um.
  Enquanto o arquivo não está commitado ela o deixa de fora (por isso compilava). Depois do commit ia quebrar:
  - o `Smooth` do menu e o da sala colidiam;
  - o `R` (raio do radar) do menu escondia os `R` locais da sala (C4459 vira erro).
  - O conserto: o `R` virou `RADAR_R`; as peças comuns foram pro kit (`BoxRect`, `GlowRect`, `Disc`, `Chev`, `Hex`, `Ac`, `In`,
    `StageFit`, `Backdrop`, `BackButton`, `StageCorners`). Cada tela nova guarda as constantes num namespace com nome
    (`RTSMpScreen`, `RTSOptScreen`, `RTSScriptScreen`).
  - O preflight agora compila também um `UnityCheck.cpp` que inclui todos os `.cpp` (no rascunho, não vai pro plugin).
- **Lição do kerning (06/11):** todo texto saía um pouco mais largo que no Chrome. "FIND A BATTLE" tinha 6 px a mais, o "AT" de
  BATTLE separado, e os parágrafos quebravam em outro lugar.
  - A causa: a Slate só liga o kerning do HarfBuzz quando a fonte tem a tabela `kern` antiga, ou quando o texto tem espaçamento
    (`SlateTextShaper.cpp`, `bHasKerning`). As fontes do mockup (Big Shoulders, Barlow, Plex Mono) têm o kerning no GPOS, e o kit
    moldava sem espaçamento.
  - O conserto, no `RTSShellUI.cpp`: o texto é moldado com espaçamento de 1/1000 do tamanho (vira 0 px no arredondamento da Slate) e
    com o `FullShaping`. Vale pra todas as telas, inclusive o menu aprovado, que fica mais perto do Chrome.
  - Depois do kerning, o texto ainda saía ~1% mais estreito que no Chrome: a Slate arredonda o avanço de cada letra pro pixel
    inteiro, e o navegador põe as letras em posição fracionária.
    - Num parágrafo de 500 px, uma palavra mudava de linha: o cartão do SCRIPT, o "Change" do cartão das opções.
    - O acerto (o `FINE` do kit): medir e posicionar cada letra pelo texto moldado em 4× a escala da tela.
    - Cada letra continua desenhada na escala da tela, então segue nítida.
- **O pacote (06/11):** o `RTSShell.Build.cs` só levava as páginas (`Resources/UI`) como arquivos soltos. As telas nativas leem do
  disco as fontes, as imagens do menu e da sala e os losangos dos mapas (`Resources/Fonts`, `Menu`, `Skirmish`), e o pacote sairia com
  as telas sem letra nem imagem, o menu aprovado inclusive. Agora os três vão junto (`RuntimeDependencies`). O `Resources/UI` sai
  junto com o CEF.
- **O teste de verdade da rede:** `native_shot.ps1 -Name mp_real -Page multiplayer.html -Extra '-RTSMpTestHost -RTSMpTestAddAi'`.
  - A lista nativa hospeda uma sala sozinha.
  - A sala nativa da LAN abre com os dados do jogo, e o anfitrião põe uma IA pelo mesmo clique do OPEN.
  - A foto é da sala. Em 06/11 deu certo: "FE" (o nome deste PC) com o YOU, a IA, o dinheiro da rede e o START MATCH liberado.

### Onde parou o nativo (03/11, 23h, pausa pelo limite de tokens)

- **O menu nativo está no jogo e compilado:** `Private/SRTSMenu.*` mais as peças `Private/RTSShellUI.*`. O `RTSShell.cpp` troca o
  `menu.html` pelo `SRTSMenu`; as outras telas ainda são as páginas do CEF. **Nada nativo tem commit.**
- **Primeira comparação com o mockup** (`captures/concept/ui/shell/shots/native/cmp_none.jpg`), com três defeitos:
  - A foto saiu clara demais: a cor passou duas vezes pela gama, fundo 31 contra 4.
  - O espaçamento entre letras não aparecia: o `MakeText` da Slate ignora o `LetterSpacing`, só o texto moldado aplica.
  - O bloco do LAB/SCRIPT estava 30 px abaixo: a medida no Chrome pegou o item da lista ainda fechando. O `measure_menu.js` foi
    corrigido e o `menu_layout.json` medido de novo.
- **Corrigido e compilado às 22:55** (falta ver as fotos):
  - O texto moldado com cache: `RTSShellUI::Text` e `TextWidth`.
  - O bloco nas medidas certas: `DevY0 752.5`, `TileY0 782.5`.
  - O teste agora também captura a janela de verdade (`<nome>_window.png`), pra saber se a gama errada é só da foto
    (`FWidgetRenderer`) ou da tela.
  - O instante fixo como o `?t=33.93&still=1` do mockup: `-RTSMenuT=33.93`.
  - O custo no log, a cada 2 s: `RTS telas: custo do menu nativo`.
- **As fotos `native_none` e `native_skirmish` estavam na fila** quando pausou. Conferir:
  `python Game/Plugins/RTSShell/Tools/compare_menu.py captures/concept/ui/shell/shots/native/mock_none.jpg <...>/native_none.png <...>/cmp_none.jpg`
  e o mesmo com `_window.png`.
  - Se a janela estiver certa e só a foto clara, a foto do teste passa a ser a da janela.
  - Se a janela também estiver clara, investigar a cor na Slate.
- **Resultado das fotos (23h05):** `native_none.png` e `native_skirmish.png` saíram com o instante fixo, ainda **não comparadas**.
  O `_window.png` **não saiu**: a captura da janela falhou calada, provavelmente porque com `-RenderOffScreen` não há viewport pra
  capturar. Tentar sem `-RenderOffScreen` (janela fora da tela) ou ler o backbuffer de outro jeito.
  - Custo medido, em média de 2 s com o menu parado, na 1660: desenho do menu 1,2 ms de processador; quadro 4,7 ms (jogo 4,7,
    render 2,3, placa 0,25 ms).
- **04/11, a madrugada:**
  - A comparação nova bateu o texto e o bloco com o mockup. Ficou só o brilho.
  - **O brilho era da foto, não da tela:** o `FWidgetRenderer::CreateTargetFor(…, true)` cria a textura sRGB, e o shader da Slate
    já corrige a gama. A foto agora usa uma textura linear (`InitCustomFormat(…, true)`), como a imagem da janela do Windows (sem
    HDR, B8G8R8A8 não sRGB).
  - A captura da janela (`TakeScreenshot`) não funcionou nem com janela de verdade e saiu do código.
- **O som do menu** (04/11), com assets gerados pelo `Tools/import_menu_audio.py` (rodar com o `import_menu_audio.ps1`, na fila):
  - `/Game/Audio/Classes`: `SC_Master` com as filhas `SC_Music`, `SC_Effects`, `SC_Voice`, `SC_UI` e `SC_Ambience`, uma por
    volume da tela de opções.
  - `/Game/Audio/Mix/SM_ShellDuck`: a música × 0,4 em 1,4 s no skirmish, no multiplayer e no SCRIPT.
  - `/Game/Audio/Mix/SM_UserVolumes`: os volumes do jogador; hoje, música 0,55 e interface 0,6, os do mockup.
  - `/Game/Audio/Submix/SMX_Music`: o barramento da música.
  - `/Game/Audio/Music/Main_Menu_1..3` e `/Game/Audio/UI/UI_Menu_Click`.
  - O `Private/RTSShellAudio.*` toca as trilhas em sequência e o clique, e liga ou desliga o abaixar por tela (no `FRTSShell::Show`).
  - O radar ouve o `SMX_Music` com a mesma conta do analisador do navegador: Blackman, FFT de 512, suavização de 0,8, de −100 a
    −30 dB. Isso alimenta a cascata do ESM e o capim do A-scope.
  - As páginas que ainda são do navegador recebem `nomusic=1`: a música do jogo atravessa as telas.
  - **O teste é mudo:** `-RTSMuteOutput` (o `native_shot.ps1` sempre passa) zera a saída principal, e o radar segue ouvindo.
  - Na tela de opções nativa: ligar os seis volumes e o MASTER (padrão 80) ao `SM_UserVolumes`.
  - No pacote: `/Game/Audio` é carregado pelo caminho e precisa entrar no `DirectoriesToAlwaysCook`.
- **04/11, de manhã: o menu nativo bateu com o mockup.** A diferença média é 3,0 de 255 parado e 3,2 com o SKIRMISH em hover; antes
  era 52. O que sobra é antialiasing do texto e o que é sorteado nos dois (a chuva, a cratera).
  - Folhas: `captures/concept/ui/shell/shots/native/lado_a_lado_menu*.jpg` e `cmp_v4*.jpg`.
  - Custo: 1,9 ms de processador pro desenho; o quadro tem 5,2 ms e a placa 0,24 ms.
  - Acertos do dia:
    - A medida do Chrome: o marcador agora é um elemento sem CSS (o `<span>` pegava o `.devh span`) e entra junto do texto nas
      caixas flex.
    - O texto com espaçamento: cada letra vai na posição exata, porque a Slate só espaça em pixel inteiro. Isso vale também pro
      espaçamento que anda no hover.
  - **Falta o ok dele** pra registrar em `docs/APROVADOS.md`, commitar e seguir pra tela do skirmish.
- **04/11: o teste dele parou na abertura** (reprovada: o "LOADING" do `RTSGameModule.cpp`, "jogo de criança"). Ele também pediu
  som no hover dos botões.
  - Medido no log dele:
    - A Unreal de desenvolvimento leva ~50 s pra subir; no pacote, são poucos segundos.
    - Depois, o jogo carrega o `World_TestSalt2` inteiro (11 s) só pra mostrar o menu, que nem desenha o mundo.
  - **A 1ª proposta foi reprovada** por olhar pro ZH ("vai se enviesar com um jogo de 2003 [...] eu tô falando de MODERNIDADE").
  - **A 2ª proposta (esperando o ok dele), pelos jogos de hoje:**
    - **Referências:**
      - a abertura dentro da ficção: o "Breaching…" do Cyberpunk 2077, o VISR OS do Halo Infinite (uma língua só pro menu, o HUD e
        as armas);
      - o title drop, o silêncio, o grave e a música no tempo: o canto do Halo (jogadores deixavam o menu parado pra ouvir), as
        letras do MGS2 que travam com um golpe metálico;
      - o tema em uma imagem: Alien Isolation;
      - a carga escondida: a Epic recomenda segurar a tela até os PSOs compilarem (menos de 1 min na 1ª vez);
      - pular sempre: "splash screens are wrong".
    - **A sequência:** o sistema de comando militar inicializando, a mesma língua do radar e do HUD.
      1. Preto absoluto e 0,8 s de silêncio, sem cursor.
      2. O selo do estúdio, se houver, com o som dele.
      3. O boot do sistema: linhas rápidas de checagem em mono (o enlace, a cripto, o IFF, o teatro 04). **São o progresso real**
         (os shaders na 1ª vez, o mapa leve do menu): a carga vira ficção, sem "LOADING".
      4. O tubo acende com um grave de corpo.
      5. O title drop: FOUR ARMIES. ONE MAP. trava palavra por palavra com golpes no tom da trilha, e a música entra no tempo
         forte (escolher o ponto da trilha).
      6. O menu monta: a coluna desliza, o feixe começa a girar.
    - Na 1ª vez leva ~10 s, e depois ~4 s; sempre pula com tecla ou clique.
    - Os sons da interface são uma família só, no tom da trilha: o hover, o clique, o confirmar e o voltar.
    - O carregamento da partida também é dentro da ficção (a tela aprovada do protótipo).
    - Os sons saem do Atelier (ElevenLabs), com opções pra ele escolher de ouvido.
- **04/11, APROVADOS** ("tá tudo aprovado, mas o título tem que ir embora"):
  - **O menu nativo.**
  - **A abertura:** `SRTSMenu::StartBoot`/`StepBoot`, a checagem do console, o tubo, o golpe com a música.
    - Primeira vez no PC: a inteira; depois, a curta.
    - Tecla ou clique pula.
    - `-RTSNoBoot` pula sempre; os testes nunca passam por ela.
    - `native_shot.ps1 -Boot` e o `boot_video.py` fazem o vídeo com som.
  - **O hover:** o tique digital no jeito do GUIBlip do ZH, `make_menu_sounds.py`.
  - **O jogo abre no `/Game/Maps/FrontEnd`** (vazio). O fim da partida volta pra ele. A tela de carregamento dele é escura, sem
    o LOADING. As linhas nos arquivos compartilhados estão no `Tools/patch_shared.py`.
  - **O título e o kicker saíram** (provisórios). O jogo é **Ground Zero**, por enquanto (`docs/VISAO_DO_JOGO.md`, "O nome").
    O alto da coluna espera o logo.
  - **A entrada no skirmish e no multiplayer** (o golpe grave com a música abaixando) está ligada, mas ele ainda não ouviu.
### O skirmish nativo (04/11, começando; "segue pro skirmish")

- **A régua:** as fotos do mockup aprovado (`docs/concept/ui/menu/skirmish.html`) em `captures/concept/ui/shell/shots/native/`:
  `mock_skirmish_room.jpg` (a sala), `_hs1` (o hover na vaga), `_pop` (a janelinha da facção aberta) e `_browser` (o navegador
  de mapas).
  - Tiradas pelo `scratchpad/native/cap_shot.mjs`, com a URL na variável `CAP_URL`.
  - A pasta `docs` tem de estar servida (`python -m http.server 8139` em `docs`): o mockup lê os mapas por `fetch`, que o
    `file://` bloqueia, e os brasões ficam em `../../factions`.
- **O jeito:** o mesmo do menu.
  - Um `SLeafWidget` pintado com o `RTSShellUI`.
  - A busca de mapas é desenhada no próprio widget (as letras pelo `OnKeyChar`, o Backspace, o Ctrl+V, o cursor piscando): um
    `SLeafWidget` não tem filho, e um `SEditableText` por cima obrigaria a passar o foco de um pro outro. A conversa da sala de rede
    segue o mesmo caminho quando chegar.
  - Os losangos dos mapas de verdade saem do próprio mockup no Chrome (o `bakeDiamond` e as miniaturas, por mapa), salvos no
    plugin. Cada mapa novo roda o export de novo.
  - Os brasões vêm de `docs/concept/factions/small`.
  - No jogo só entram os 3 mapas de verdade e o que o jogo der; o catálogo de mentira é `fora-do-jogo`.
- **As partes, nesta ordem:**
  1. A sala parada: a coluna, as vagas, o losango com as bases, a bandeja, o START e o BACK.
  2. As janelinhas (facção, cor, time, largada, jogador), o TEAMS, o FILL/CLEAR AI, o dinheiro, as superarmas e o arrastar
     das fichas.
  3. O navegador de mapas.
  4. A ligação com o jogo (o START com as vagas, como o `RTSShellGame.cpp` faz hoje).
  5. A comparação com o mockup.
  - A sala de rede (o modo `mp=1`) vem junto da tela do multiplayer.
- **Onde está (05/11):**
  - **Feito:** a sala inteira (as partes 1, 2 e 4), o navegador de mapas (parte 3) e a dica das cores.
    - O navegador: o SELECT MAP, a busca, a contagem, os filtros (ordem, favoritos, naval, jogadores, tamanho, terreno, condição,
      origem) nas medidas do Chrome, a grade de 6 colunas com rolagem e os cartões com a miniatura, a origem, o nome, a ficha e a
      estrela. Os favoritos ficam em `Saved/RTSShell/skirmish_favorites.json`.
    - O cartão NEW WORLD aparece igual ao mockup, mas parado: o jogo ainda não gera mundo pela semente.
    - A dica das cores (o `#tip`): o nome, quem já usa e o aviso "HARD TO SEE ON <terreno>" pela distância Lab ao chão do mapa.
  - **Os fontes:** o rascunho fica no scratchpad da sessão; entram no plugin pelo `deploy_native.ps1 -Src <pasta>`, na vez da fila.
  - **Medidas:** `Tools/skirmish_layout.json`, `skirmish_pops.json` e `skirmish_browser.json`, pelos `measure_skirmish*.js`.
    - A página do jogo (`Resources/UI/skirmish.html`, só os 3 mapas de verdade) é servida em `http://127.0.0.1:8141` pra medir e
      fotografar o navegador.
    - A referência do navegador é `mock_game_browser.jpg`.
  - **Fundos:** `make_menu_assets.py --skirmish` gera `skirmish_glow.png` e `skirmish_vign.png`.
  - **Losangos, miniaturas e fichas dos mapas:** `Tools/export_skirmish_maps.mjs` (as imagens chegam por POST num receptor local).
  - **Fotos no jogo:** `native_shot.ps1 -Skirmish` com `-SkHover`, `-SkPop`, `-SkPopHover`, `-SkBrowser`, `-SkQuery` e `-SkCard`.
    - A comparação: `Tools/compare_skirmish.py <mockup> <nativo> <folha>`.
    - **Várias fotos numa vez só da fila** (05/11: com a fila cheia, cada foto esperava ~45 min): a corrente pega a vez uma vez e
      põe `$env:RTS_QUEUE_OWNER` com o dono dela. O `deploy_native.ps1` e o `native_shot.ps1` usam esse dono e não saem da fila
      no fim; a corrente renova a vez entre as fotos e sai no `finally`.
    - Lição: `FParse::Value` de texto para na vírgula. Valor com vírgula (`-RTSSkPop=0,1`) precisa do `false` no 4º argumento
      (a 1ª foto do menu de exército saiu sem o menu por isso).
  - **O brilho (box-shadow `0 0 Npx`)** é a gaussiana do navegador: sigma = N/2, o perfil `erf` em x vezes o perfil em y, numa
    grade de meio sigma (`Glow` no `SRTSSkirmish.cpp`). Os três retângulos de antes faziam degraus quadrados visíveis.
    - As sombras (`0 Ypx Bpx`) usam a mesma função, com o retângulo deslocado Y pra baixo: a dica, o menu e o botão do dinheiro.
      A sombra em degraus da dica riscava as cores da 1ª linha.
  - **A cor aleatória** é o `conic-gradient` do mockup: a cor sai do ângulo a partir do centro (0 pra cima, sentido horário), num
    leque com os cantos do retângulo. Numa faixa larga a esquerda fica roxa e a direita verde.
  - **A cor ocupada** tem o risco do `::after`: 2 px, a altura da célula + 6, girado 45°, e com a opacidade 0,3 da célula.
  - **O brasão da ficha** vai uma camada acima do disco: na mesma camada a Slate desenhava o disco por cima e o brasão sumia.
  - **Falta:** comparar as fotos com o mockup, acertar e mostrar a ele lado a lado.
- **04/11: os testes automáticos começaram a cair** (aviso da sessão dos Cursores). A queda é no `~FRTSUnitVisual` dentro do
  `FRTSGameData::Load`, não nas telas. A struct mudou às 10:47 sem commit, por outra sessão. A suspeita é `.obj` antigo no
  Intermediate do RTSGame (o `RTSAutoTest.cpp.obj` é de 01/11). A resposta com isso foi mandada à sessão dos Cursores.

- **Falta no menu:**
  - Acertar o que sobrar na comparação.
  - A música e o clique como som do jogo. **A cascata do ESM (embaixo à direita) é desenhada pelo espectro da música no mockup**
    (`analyser`): sem música ela fica quase vazia. Ler o espectro pelo mixer da Unreal.
  - Mostrar a ele lado a lado e esperar o ok.
- **Ferramentas** (em `Game/Plugins/RTSShell/Tools/`):
  - `deploy_native.ps1` compila na vez da fila, espera nenhuma Unreal do jogo aberta, guarda a versão anterior e volta + recompila
    se falhar. Copia os arquivos de um rascunho: ajustar `$NS` pra pasta dos fontes editados.
  - `preflight_native.py` compila à parte com o cl.exe antes.
  - `native_shot.ps1 -Name <n> [-Hover k] [-T 33.93]` tira a foto no jogo, fora da tela.
  - `run_measure.mjs` mais `measure_menu.js` medem o mockup no Chrome.
  - Lição: um `.cpp` novo só entra na árvore do jogo na minha vez de compilar (os builds das outras sessões o pegam).

## Onde parou (02/11, a conta do Felipe sem limite; continuar daqui)

**Feito e testado no jogo:** o plugin `RTSShell` compila e as telas abrem no jogo (menu, skirmish, multiplayer, opções, SCRIPT); a
ponte `window.ue.shell` responde; o SCRIPT roda a cena armada (`RTS telas: SCRIPT <nome>`). A versão do jogo é enxuta: o
`export_game.py` corta tudo marcado `fora-do-jogo` no protótipo (barra de telas, debug, mapas e salas de mentira, as telas da
partida ainda não ligadas). Os ganchos nos arquivos compartilhados estão no commit (só as minhas linhas).

**Falta:**
1. ~~Provar o START e o LAB~~ (03/11, provados no jogo: `RTS telas: skirmish em World_TestHills com 3 vagas, $10000`, as
   largadas com times e as facções; o LAB abriu o `Lab_Flat`). O texto de antes: provar o START do skirmish no jogo (corrigido o passo de tempo negativo do navegador embutido que travava o clique; o reteste
   `t2_skirmish` estava na fila) e o LAB (`t3_lab`, não abriu a tempo porque o jogo compilava shaders de outra sessão).
   Roda: `powershell -File docs/concept/ui/menu/tools/shell_game_test.ps1 -Name t2_skirmish -Steps <steps_t2.json>` (fora da tela,
   desiste se o log parar 90 s ou as telas não vierem em 30 s depois do mapa).
2. ~~Avisar a sessão dos Cursores~~ (03/11, feito por ela): o navegador embutido troca a forma do cursor Custom do Windows; o
   `FRTSCursors::Invalidate()` repõe as dela no BeginPlay da partida. **Quando uma tela do RTSShell abrir no meio da partida**
   (a pausa, o resultado), chamar `PC->Cursors.Invalidate()` ao fechar.
3. Depois: ~~a sala da LAN nova~~ (03/11, feita), o que falta no protocolo da rede pra sala ficar inteira (a facção por jogador, o
   mapa, as 24 cores, a conversa: é da sessão do jogo/multiplayer), as telas da partida (Esc, resultado, avisos, carregando),
   a dificuldade da IA (a IA tem um nível só), as superarmas, o CLASSIC de verdade, a música pelo volume do jogo.
   (Na hora do pacote da versão fechada, não antes: conferir que o CEF e `Resources/UI` vão junto.)
4. A linha no CLAUDE.md apontando este guia (o Felipe pediu pras outras sessões saberem).

**Pra quem continua (o necessário pra seguir sem perguntar):**
- **Primeiro:** ver o resultado do último teste que ficou rodando: `captures/concept/ui/shell/shots/game/t2_skirmish/` e `t3_lab/`
  (o `game.log` de cada um; procurar `RTS telas: skirmish em`, `RTS facção do jogador`, `RTS laboratório: abrindo`).
- **A fonte das páginas** é o protótipo em `captures/concept/ui/shell/` (fora do git, neste PC); `docs/concept/ui/menu/` é a cópia
  aprovada. Mudou página: `python captures/concept/ui/shell/tools/export_game.py` (pro jogo, sem compilar) e
  `python captures/concept/ui/shell/tools/export_docs.py` (pra cópia do git). O que não vai pro jogo fica entre as marcas
  `fora-do-jogo` (pedido dele: no jogo só o que o jogo usa, nada escondido).
- **Mudou C++** (o plugin ou o `RTSShellGame`): compilar só na vez da fila da placa, sem compilação de outra sessão rodando
  (memória "Compilar com segurança"); se falhar, voltar o arquivo pra versão que compilou.
- **Teste no jogo:** sempre curto e fora da tela dele (o `shell_game_test.ps1` já faz; memória "Teste não segura a fila").
- **O site na rede pro pai:** `http://192.168.1.88:8135/menu.html` (servidor Python, PID 15028; depois de reiniciar o PC, subir de
  novo com `python -m http.server 8135 --bind 0.0.0.0 --directory captures/concept/ui/shell`).
