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
- **O ataque andando da v2** foi reprovado: "deixou de ser entendido que é um ataque". As setas verdes engoliam a mira
  vermelha pequena. Ele pediu pra misturar os dois. Estão no mestre três misturas pra ele escolher (`amoveA/B/C`):
  - **A**, a do ZH: a mira inteira com as setas nos vãos;
  - **B**: o ataque manda, com setas de canto;
  - **C**: a seta de corpo verde e ponta vermelha.
- **Nada no jogo ainda.** O jogo hoje usa as setas do Windows (seta, cruz, mira, mão), e só a chave do conserto é nossa
  (`Game/Source/RTSGame/Private/RTSPlayerController.cpp:1033-1081`, `RTSPlayerEconomy.cpp:306`).

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
| **Fila** | com o Alt: entra no fim da fila de ordens (`SetQueueOrders`) | o cursor da ordem + a bolinha "+" | o da ordem | o da ordem | o da ordem | `Waypoint` |
| **Atacar** | sobre o inimigo que a seleção alcança | anel fino com 4 cortes + quatro triângulos fechando | vermelho | fecham | centro | `AttackObj` |
| **Não alcança** | sobre o inimigo que ninguém da seleção acerta (o tanque e o avião) | a mira cinza cortada | cinza | parada | centro | não tem (novo: diz "precisa de antiaéreo") |
| **Tiro forçado** | Ctrl, sobre qualquer unidade | a mira (sem triângulos) travada por quatro cantos | vermelho | os cantos apertam | centro | `ForceAttackObj` (igual ao atacar) |
| **Atirar no chão** | Ctrl, no chão | os triângulos + um X no lugar do anel | vermelho | fecham | centro | `ForceAttackGround` (igual ao atacar) |
| **Ataque andando** | A armado | **a escolher** (A, B ou C; a da v2, as setas + a mira pequena, foi reprovada) | verde e vermelho | fecham | centro | `AttackMove` |
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

## 5. Como entra no jogo

- **Mestre em vetor** (SVG, grade 64): `art/cursors/cursors.html`.
- **Um script gera os quadros** em 48, 64 e 96 px e monta os `.ani` (os animados) e `.cur`.
- **Cursor de hardware: o Windows desenha e anima.** Não espera o quadro do jogo (a 30 qps um cursor desenhado pela
  Unreal atrasaria 33 ms) e não custa nada na placa.
- **A Unreal já carrega `.ani`/`.cur`**, inclusive de dentro do pacote (`WindowsCursor.cpp:170-200`, por arquivo
  temporário), ou de um buffer, que é o caminho da chave de hoje. O `EMouseCursor` tem poucas vagas, então os nossos vão
  todos pelo `Custom`, trocando a forma.
- **O tamanho segue a altura da tela.**
- **A troca no `RTSPlayerController`:** a escolha que já existe (`:1033-1081`) ganha os casos novos: não alcança,
  fila, guarda, escolta, patrulha, embarcar, encontro, coletar, desmanchar, retomar e os poderes.

## 6. Próximos passos

1. Ele vê a prancha (forma, cor, movimento) e diz o que muda.
2. Aprovado: mestres em `art/cursors/`, o gerador dos `.ani`, a troca no jogo e a prova **no jogo**, em vídeo, em cima
   da areia, da sombra e da fumaça.
