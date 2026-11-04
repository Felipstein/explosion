# Cursores

Pedido do Felipe em 01/11/2026: "precisamos de cursores [...] coisas bonitas também, que ajudam a ler o jogo, a entender,
ficar icônico na cabeça [...] cada ícone bem pensado, o que deveria ser, como deveria ser, o que deve representar, quais
que precisa".

**Estado (01/11):**
- **A v1** teve o rumo elogiado, e a marcação do poder no chão foi chamada de "profissional e sensacional" (`docs/APROVADOS.md`, HUD).
  Está congelada em `captures/concept/cursores/board_v1.html`.
- **A v2 está APROVADA**, menos o ataque andando: "o resto super aprovado, parabéns, um dos melhores". Ela corrige cada
  cursor um por um (§3.1); o antes e depois está em `docs/aprovados/hud/2026-11-01_cursores_v2_APROVADOS_menos_ataque_andando.jpg`.
- **O mestre** (os desenhos em vetor e a prancha animada) é `art/cursors/cursors.html`. Abre direto no navegador.
- **O ataque andando:** o da v2 foi reprovado ("deixou de ser entendido que é um ataque": as setas verdes engoliam a mira
  vermelha pequena). Das três misturas, ele escolheu **a B** ("opção B, gostei"): a mira do atacar inteira manda, e o verde
  vira quatro setas de canto apontando pra ela.
- **No jogo (01/11, falta o teste dele clicando):**
  - os arquivos: `Content/Cursors/<48|64|96>/` (33 cursores; o rolar nas 8 direções), gerados por
    `python art/tools/make_cursors.py`. O Windows confere cada arquivo no fim.
  - o código: `RTSCursors.h/.cpp` carrega e troca. A escolha fica no `TickMatch` do `RTSPlayerController.cpp`, na mesma
    ordem de decisão do clique. Antes eram as setas do Windows e só a chave do conserto era nossa.
  - Sem os arquivos, voltam os do Windows e a chave.
  - `-RTSCursorLog` põe cada troca no log, com o mouse.
  - O teste `game.ps1 view -Folder cursores_cliques -Scenario v4_cliques -Extra '-RTSCursorLog'` passou com 0 falhas.
    Os cursores carregam em 48 px (tela de 1600×900) e trocam na ordem dos cliques: selecionar no Dozer, a chave na usina
    avariada, andar, o encontro com a usina selecionada. A prova com o desenho no ponto do mouse está em
    `captures/concept/cursores/prova_no_jogo_conserto.jpg`.

## 1. O que o ZH faz (pelo código e pelos arquivos dele)

- **40 tipos no código** (`GeneralsGameCode/Core/GameEngine/Include/GameClient/Mouse.h:181-250`, nomes em
  `Core/GameEngine/Source/GameClient/Input/Mouse.cpp:1269`), **~30 ligados** no `Data\INI\Mouse.ini` (lido de dentro do
  `INIZH.big` do ZH instalado; cópia em `captures/concept/cursor_refs/zh/Mouse.ini`).
- **Os arquivos** (`Data\Cursors\*.ani` do ZH instalado, de 2003): 32×32 px, paleta, contorno preto de 1 px. Animação a
  6 qps (10 jiffies). O ponto do clique fica no centro (15,15) nos de mira e no canto (3 a 4 px) nos de ferramenta
  (chave, venda, guarda). Todos, quadro a quadro: `captures/concept/cursor_refs/zh/zh_cursores_folha.png`.
- **A gramática dele**, que o jogador do ZH tem na cabeça:
  - verde = as suas tropas: **mover** = quatro setas em X fechando no ponto;
  - vermelho = contra alguém: **atacar** = anel + quatro triângulos fechando;
  - **ataque andando** = os dois juntos;
  - **não dá** = círculo vermelho cortado;
  - **rolar** = seta verde em 8 direções;
  - **selecionar** = quadrado tracejado branco;
  - **encontro** = quatro setas brancas.
- **Curiosidades que contam:**
  - o Ctrl não muda o cursor: `ForceAttackObj` e `ForceAttackGround` usam a mesma mira do atacar;
  - o "fora de alcance" existe mas foi desligado (`Image = SCCAttack ; SCCOutRange`);
  - a **área** (a guarda, os poderes) não fica no cursor: é um desenho no chão (`RadiusCursor` do `InGameUI.ini`,
    a guarda pulsando de 50 a 100% em 1 s, ver `docs/HUD_PESQUISA.md` §Guard).

## 2. O que os RTS de hoje somam

- **A cor diz a relação.** O SC2 acende o cursor e o anel em verde (seu), amarelo (aliado/neutro) e vermelho (inimigo)
  ([guia do editor do SC2](https://s2editor-guides.readthedocs.io/New_Tutorials/03_Trigger_Editor/048_Unit_Selection_Events/));
  o modo daltônico troca por azul, ciano e laranja ([Team Liquid](https://tl.net/forum/starcraft-2/423420-new-color-mode-with-patch-2010)).
- **A área vai no chão**, não no cursor: SC2, CoH e o próprio ZH.
- **Ícones refeitos por legibilidade e consistência**: o CoH3 refez os ícones depois do lançamento porque os jogadores
  não liam ([Retrostyle Games](https://retrostylegames.com/blog/analyzing-company-of-heroes-3-game-design/),
  [PC Gamer](https://pcgamer.com/company-of-heroes-3-has-changed-a-lot-thanks-to-player-feedback-from-its-interface-to-its-major-features)).
- **Alta resolução**: um tamanho por escala de tela (100, 150, 200%), nunca o de 32 px esticado.

## 3. As regras do nosso

1. **A gramática do ZH, o acabamento de hoje.** As formas que ele reconhece (setas em X, mira com triângulos, círculo
   cortado), em vetor nítido em qualquer resolução. Igual ao HUD: o ZH modernizado.
2. **A forma diz o quê; a cor diz pra quem.** Quem não enxerga cor ainda lê: mover é X diagonal, atacar é cruz reta.
3. **Seis cores, cada uma com um sentido só:**
   - **branco** = sistema (apontar, selecionar, rolar, construir);
   - **verde** = as suas tropas vão fazer;
   - **vermelho** = contra alguém;
   - **amarelo** = trabalho do Dozer e do caminhão (a cor da máquina de obra; o Dozer é amarelo);
   - **a cor do jogador** = o que é seu, marcado no mapa (a bandeira do encontro, o poder de apoio);
   - **cinza com a barra vermelha** = não dá.
4. **Movimento = vai acontecer.** As ordens respiram (0,7 a 1 s por volta); o que não dá fica parado.
5. **O ponto do clique se vê.** Nos de mira, o centro. Nos de trabalho do Dozer e do caminhão (consertar, retomar, desmanchar, coletar), uma
   seta pequena aponta e o símbolo fica do lado: a ponta é o clique, sem adivinhar onde a chave "pega".
6. **O tamanho sai da proporção do ZH:** 32 px em 768 linhas = 4,2% da altura. Vira **48 px em 1080p, 64 em 1440p e
   96 em 4K**.
7. **Lê em qualquer chão:** contorno escuro + uma sombra curta. Conferido na areia ao sol, na sombra dos prédios e no
   asfalto (a prancha mostra cada um metade em um chão, metade em outro).
8. **O botão e o cursor falam a mesma língua:** o símbolo do botão do HUD é o do cursor (ataque andando, guarda,
   encontro, conserto).
9. **A área vai no chão** (guarda, poderes), como no ZH; o cursor só marca o centro.
10. **Uma família só pras três facções**, pela leitura. Se ele quiser a facção no cursor, ela entra só na seta comum.

### 3.1 O acabamento (v2, medido em 48 px)

A prancha tem um modo de medida (`art/cursors/cursors.html#raster=48`, cada cursor no tamanho exato, sem sombra; `python art/tools/cursor_measure.py 48` roda e imprime). Ele mede a caixa,
o centro de massa contra o ponto do clique e o borrão (a parte da borda que fica meio transparente).
- **As retas na grade dos pixels.** Traço de 4 centrado em coordenada 4k+2 da grade 64: as bordas caem em múltiplos de 4,
  que é pixel inteiro em 48, 64 e 96 px. Os cantos do selecionar e do construir passaram de 35% de borrão pra 6–8%.
  Diagonais e curvas ficam suavizadas (não tem como).
- **O contorno: 2 px em 48 e 64, 3 px em 96**, gerado por tamanho e não esticado.
- **Nenhuma ponta "cortada":** toda ponta de traço é quadrada ou redonda, e o contorno repete a ponta. Na v1, a cauda das
  setas e as pontas dos tracejados encostavam a cor direto no chão.
- **Ponta viva nas setas e triângulos** (contorno em quina); o resto com o contorno arredondado.
- **Tamanho de família:** as miras de centro com ~40 px em 48. A v1 ia de 28 a 42 sem motivo: o "não dá" com 30, a
  patrulha com 32. O tiro forçado e os poderes ficam com 42–44 (são especiais), e o construir com 34 (o fantasma do
  prédio faz o resto).
- **A luz de cima:** cada peça ganha um degradê leve (30% mais clara em cima, 16% mais escura embaixo), e as peças grandes
  ganham a face na sombra (a seta, o rolar, o escudo, o caixote), no acabamento do HUD.
- **O que não funcionou e voltou:**
  - o anel do atacar com aberturas largas virou uma "flor"; ficou com 4 cortes estreitos;
  - a mira pequena do ataque andando com aberturas virava 4 pontinhos; ficou inteira;
  - a área do guardar inteira fazia o escudo parecer um alfinete de mapa; ficou em 6 traços longos.

## 4. A lista (o que o jogo precisa hoje: 24, mais as 8 direções do rolar)

| Cursor | Quando aparece | Forma | Cor | Movimento | Clique | No ZH |
|---|---|---|---|---|---|---|
| **Seta** | HUD, menus, nada selecionado | a seta clássica | branco | parada | ponta | `Arrow` |
| **Selecionar** | sobre o que é seu | quatro cantos em volta do ponto | branco | os cantos apertam | centro | `Select` |
| **Rolar** (×8) | na borda da tela | seta grossa na direção | branco | parada | ponta | `Scroll` (verde) |
| **Arrastar a vista** | segurando o direito | quatro pontas + ponto | branco | parada | centro | não tem |
| **Não dá** | ordem que não vale ali (penhasco, água, fora do mapa) | círculo cinza com a barra vermelha | cinza | parada | centro | `GenericInvalid` |
| **Mover** | tropa selecionada, no chão | quatro setas em X fechando | verde | fecham | centro | `Move` |
| **Fila** | com o Alt, em toda ordem que entra na fila (andar, ataque andando, atacar, tiro forçado, guarda, patrulha, embarcar, as obras e a coleta) | o cursor da ordem no tamanho de sempre + **a ficha** (a B, 01/11): a bolinha na cor da linha do plano com o número da vez; a mesma ficha marca cada ponto do plano no chão, centrada no ponto exato | o da ordem | o da ordem | o da ordem | `Waypoint` |
| **Atacar** | sobre o inimigo que a seleção alcança | anel fino com 4 cortes + quatro triângulos fechando | vermelho | fecham | centro | `AttackObj` |
| **Não alcança** | sobre o inimigo que ninguém da seleção acerta (o tanque e o avião) | a mira cinza cortada | cinza | parada | centro | não tem (novo: diz "precisa de antiaéreo") |
| **Tiro forçado** | Ctrl, sobre qualquer unidade | a mira (sem triângulos) travada por quatro cantos | vermelho | os cantos apertam | centro | `ForceAttackObj` (igual ao atacar) |
| **Atirar no chão** | Ctrl, no chão | os triângulos + um X no lugar do anel | vermelho | fecham | centro | `ForceAttackGround` (igual ao atacar) |
| **Ataque andando** | A armado | a mira do atacar inteira + quatro setas verdes de canto apontando pra ela (a B) | vermelho e verde | fecham | centro | `AttackMove` |
| **Guardar a área** | G, no chão | escudo + a área em 6 traços no chão | verde | a área pulsa (50–100% em 1 s, o do ZH) | centro da área | `Guard` + decal |
| **Escoltar** | G, sobre o que é seu | escudo dentro dos cantos | verde | os cantos apertam | centro | a mesma guarda |
| **Patrulhar** | avião e helicóptero | duas setas em roda | verde | gira devagar | centro | não tem |
| **Embarcar** | soldado sobre o transporte seu (depois: guarnecer prédio) | duas setas pra baixo entrando na caixa | verde | descem | centro (v2: o clique é no transporte) | `EnterFriendly` |
| **Ponto de encontro** | fábrica selecionada, no chão | a bandeira fincada | a cor do jogador | a bandeira tremula | pé do mastro | `SetRallyPoint` |
| **Construir aqui** | pondo o prédio (o fantasma faz o resto) | quatro cantos + cruz | branco | parada | centro | `Build` |
| **Não cabe aqui** | pondo o prédio onde não dá | os cantos cinza + a barra | cinza e vermelho | parada | centro | `InvalidBuild` |
| **Consertar** | Dozer sobre prédio seu avariado | a chave | amarelo | balança | ponta da seta | `DoRepair` |
| **Retomar a obra** | Dozer sobre obra parada | o martelo | amarelo | bate | ponta da seta | `ResumeConstruction` |
| **Desmanchar** | Dozer sobre carcaça ou entulho | o gancho do guincho | amarelo | balança | ponta da seta | não tem (hoje usa a chave) |
| **Coletar** | caminhão sobre o depósito | o caixote + a seta descendo | amarelo | a seta desce | ponta da seta | `Dock` |
| **Poder ofensivo** | mirando um ataque do general | a mira grande com o anel girando | laranja | o anel gira | centro (a área vai no chão) | um por poder (`ParticleUplinkCannon`...) |
| **Poder de apoio** | paraquedas, drone espião | a mesma mira | a cor do jogador | o anel gira | centro | idem |

**Não precisam de cursor** (botões que valem na hora): parar, voltar à base, desembarcar, vender (hoje é botão do
prédio; se um dia virar o modo "$" do C&C clássico, o cursor já está desenhado na prancha).

**Entram quando a coisa existir**, na mesma gramática: guarnecer prédio (o embarcar), capturar prédio, ir se consertar
ou se curar (a unidade sobre a oficina ou o hospital), o sinalizador do minimapa, desembarcar num ponto e os especiais
de cada unidade (sniper, C4, hacker...), cada um nascendo junto com a unidade.

## 5. Como entra no jogo (feito em 01/11)

- **O mestre em vetor** (SVG, grade 64) é `art/cursors/cursors.html`. O modo `#export&t=<s>` desenha os 33 cursores do
  jogo nas faixas de 48, 64 e 96 px, com a sombra, no instante t, e escreve o manifesto: o nome, o clique, a volta e os
  quadros de cada um.
- **O gerador** é `python art/tools/make_cursors.py`:
  - tira um quadro por instante pelo Edge headless e corta cada cursor;
  - grava `.cur` (parado) ou `.ani` (animado) em `Game/Content/Cursors/<px>/`, com os quadros em PNG dentro;
  - roda a 20 quadros por segundo, e a patrulha a 12 (a volta dela é de 3 s);
  - no fim, confere cada arquivo no próprio Windows (`LoadImage`). O 1º quadro de cada um fica em
    `captures/concept/cursores/export/` pra prova.
- **Cursor de hardware: o Windows desenha e anima.** Não espera o quadro do jogo (a 30 qps um cursor desenhado pela
  Unreal atrasaria 33 ms) e não custa nada na placa.
- **A Unreal carrega `.ani`/`.cur`** por `ICursor::CreateCursorFromFile`, inclusive de dentro do pacote
  (`WindowsCursor.cpp:170-200`, por arquivo temporário). O `DefaultGame.ini` põe `Cursors` no pacote como arquivo solto.
  - A seta toma o lugar do `Default` do Windows, então vale no HUD e nos menus.
  - Os outros entram todos pelo `Custom`, trocando a forma: o `EMouseCursor` tem poucas vagas.
- **O tamanho segue a altura da tela:** 48 px até 1200 linhas, 64 até 1800 e 96 acima.
- **A escolha (`TickMatch`)** segue a mesma ordem de decisão do clique (`HandleLeftClick`, `HandleEconomyClick`):
  1. arrastar a vista e a borda;
  2. a linha dos dois botões e o minimapa;
  3. posicionar o prédio, o laboratório, o poder (o de mísseis = ofensivo), a guarda, a patrulha, o ataque andando e o Ctrl;
  4. a unidade debaixo: o inimigo (atacar ou não alcança), a obra parada, o conserto, embarcar e selecionar;
  5. o chão: a sucata, a coleta, o encontro (só prédios selecionados) e andar (com o Alt, a fila).
- **O "não alcança" mostra o que acontece:** a mira cortada, mas o clique continua mandando andar, como antes. Mudar
  isso é regra de jogo, não cursor.

## 6. Próximos passos

1. O teste dele no jogo, clicando.
2. Os cursores que nascem com as unidades e os sistemas novos (guarnecer, capturar, o sinalizador, os especiais) entram
   no mestre na mesma gramática, e o gerador refaz tudo.

## Onde parou (03/11, retomado na outra conta; esperando a revisão dele)

- **Ponto de encontro:** aprovado no jogo (cf34cc0). O pano novo (995d0ee, `Dye = Pow(V, 1.3)`, esticado no mínimo 0,85) mede
  (102, 193, 231) contra o 59B2FF da prévia (o aprovado v2: (72, 159, 213)); A/B em `captures/concept/cursores/pano_antes_agora.jpg`.
  Se ele preferir o v2, voltar o trecho do `Dye` e do `Lift` pro cf34cc0.
- **Marca da ordem no chão (a A, no jogo, sem commit):** `RTSOrderMarker.h/.cpp` no lugar do anel fino do `ARTSHUD::DrawMarkers`; a
  marca dura 1 s (`MarkerSeconds`). Prova: a cena `rally_ponto` com `-RTSMarkerAt=-262,-318` (`captures/game/rally_ponto_v4/`,
  `captures/concept/cursores/marca_no_jogo.gif`); as três direções em `art/cursors/marca_ordem.html` e no GIF
  `captures/concept/cursores/marca_ordem_movimento.gif`. Falta ele escolher (A, B ou C).
- **Os traços lisos num lugar só:** `RTSSoftDraw.h/.cpp` (a faixa com a textura do perfil, o disco, o polígono com a borda lisa e o
  contorno de cantos redondos), usado pelo ponto de encontro, pela marca e pela fila.
- **A fila do Alt no jogo refeita como a prancha aprovada (sem commit):** `ARTSHUD::DrawOrderQueues`; o mesmo passo das unidades
  vira uma ficha no meio delas, as linhas saem de cada unidade até a 1ª ficha e depois de ficha em ficha, a prévia tracejada até o
  mouse (antes: cada tanque até a vaga dele, um novelo de linhas de 2 px serrilhadas, números repetidos). Prova:
  `captures/game/cursores_fila_v4/`, antes × agora em `captures/concept/cursores/fila_jogo_antes_agora.jpg`.
- **O cursor depois das telas novas (sem commit):** `FRTSCursors::Invalidate()`, chamado no BeginPlay da partida (o navegador do
  RTSShell troca o Custom da aplicação; pedido da sessão das Telas).
- **(03/11) Pedidos novos dele, em proposta (sem nada no jogo ainda):** os alertas (o "sob ataque" do minimapa refeito, a seta da
  borda decidida no HUD e nunca feita, o catálogo dos outros alertas do radar, na língua dos cursores):
  `captures/concept/cursores/alertas.html`; e o minimapa inteiro ("tudo feito às pressas"): a prévia em 3 jeitos e os indicadores
  (a planta dos prédios, o losango dos veículos, a caixa âmbar da doca, a câmera em branco): `captures/concept/cursores/minimapa_board.py`
  → `minimapa_v1.png`. A marca B (escolhida: "gostei mais do B") está no jogo sem commit, a prova esperando a fila da placa.
- **(03/11, fim da conta) Onde parou:** aprovados e registrados o minimapa v4 e os alertas (seta B + brilho C; sem alerta pra obra e
  unidade prontas). Feitos no jogo, sem commit e sem prova (compilam no espelho): `RTSAlerts.h/.cpp` (o minimapa, o calor, o radar
  fora do ar, a seta da borda; teste `-RTSAlertTest` e `-RTSAlertTest=radar` na cena `rally_ponto`), a marca B (`RTSOrderMarker`),
  a fila do Alt refeita, `RTSSoftDraw`, o v4 no `DrawMinimap`, o `Invalidate` dos cursores. Falta: compilar pela fila, provar
  ampliado (o v4 e a marca B: `captures/game/marca_b_v2/`; os alertas), mostrar e commitar só o aprovado.
- **(03/11, pausa pelo limite) O que ele viu dos alertas no jogo e falta corrigir:** (1) com o radar fora do ar o minimapa ainda
  mostrava os alertas ("isso não pode"): no `ARTSHUD::DrawMinimap`, com `bRadarOff` não desenhar os avisos nem o calor; (2) a seta
  vermelha "não ficou guardada no canto da tela" (encostar na borda, margem pequena) e o brilho da borda ficou por baixo do HUD
  (desenhar por cima do HUD ou fora das peças): `RTSAlerts::DrawEdgeArrows`. A cena `rally_ponto` ganhou a usina (sem ela o radar cai);
  a prova `captures/game/alertas_v3/` estava rodando. O minimapa v4 e a marca B estão provados no jogo (`captures/game/marca_b_v2/`);
  nada disso commitado (só o registro no APROVADOS).
- **(03/11, pausa) A seta da borda, de novo reprovada no jeito:** "a seta ainda não tá no canto da tela, e a borda brilhante tá embaixo
  do placar do HUD, bem mal feito; tem que ficar por cima do HUD mesmo". Então: a seta encostada na borda da TELA (não na peça do HUD)
  e a seta e o brilho desenhados POR CIMA do HUD (hoje o Canvas do `ARTSHUD::DrawMarkers` sai antes das peças do HUD novo; desenhar
  num passe depois delas, ou numa camada acima). Desfazer o desvio das peças em `RTSAlerts::DrawEdgeArrows` (o `InPiece` que empurra
  a seta e o brilho pra dentro). O resto dos alertas, a marca B, o minimapa v4 e a fila do Alt seguem provados, sem commit.
