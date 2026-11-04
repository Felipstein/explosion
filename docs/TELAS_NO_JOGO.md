# As telas do jogo: o menu novo, o SCRIPT e o LAB nos testes

Desde 01/11/2026 o jogo abre nas **telas aprovadas** (menu com o radar, skirmish, multiplayer, opções), as mesmas páginas HTML do
protótipo rodando dentro do jogo no navegador embutido da Unreal (o plugin `Game/Plugins/RTSShell`; o lado do jogo em
`Game/Source/RTSGame/*/RTSShellGame.*`). O menu antigo, desenhado no HUD, ficou só pro teste automático. Dono: a sessão das telas
(`docs/concept/ui/TELAS.md`).

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
- **MULTIPLAYER:** a LAN de verdade (as salas da rede, HOST e JOIN) e **a sala nova** (03/11, provada no jogo: criar, pôr IA, trocar a
  cor, começar): os jogadores da rede, a cor e o time em sequência (a paleta de 8 do jogo), pôr e tirar IA, o dinheiro, começar e
  sair. A facção por jogador, o mapa, as superarmas e a conversa ainda não existem no protocolo da rede: ficam fora da sala no jogo.
- **OPTIONS:** o preset muda a qualidade de verdade (LOW, MEDIUM, HIGH, ULTRA; o CLASSIC ainda vai na média). Os outros itens
  ficam guardados pra quando o jogo tiver cada um.
- **QUIT** fecha o jogo.
- O **LAB** e o **SCRIPT** só aparecem no desenvolvimento (o editor, `game.ps1 view`, ou `-RTSLab`): o pacote do pai não mostra.
- As telas da partida (Esc, resultado, avisos de rede, carregando) ainda são as antigas.

## Pra mudar uma tela

As páginas são editadas no protótipo (`captures/concept/ui/shell/`, o rascunho; a cópia aprovada em `docs/concept/ui/menu/`) e
copiadas pro plugin com `python captures/concept/ui/shell/tools/export_game.py` (sem compilar: o jogo lê os arquivos na hora). O
jogo responde às páginas pelo `window.ue.shell` (`URTSShellBridge`) e pelos JSON em `https://rts.ui/api/` (`RTSShellGame.cpp`).

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
