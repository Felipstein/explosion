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
