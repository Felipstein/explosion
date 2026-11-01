# As telas do jogo (o shell), 01/11/2026

O pedido dele (01/11): "vai criando as telas UI do jogo: a principal, a de configurar partida, multiplayer, configurações, menu do
Esc e muitas outras que vai ter que ter no nosso jogo; vai vendo os jogos, o C&C, o nosso jogo, o que precisa de tela".

**Estado (01/11):** primeira rodada de conceito no Atelier (pedido "Telas do jogo", `captures/concept/66_telas_do_jogo/`, a folha em
`captures/concept/ui_telas_r1_folha.jpg`). Saíram 5 das 10: o menu, a sala, o multiplayer, o carregando e o Esc. As outras 5 (resultado, opções,
aviso de rede, replays, primeira abertura) falharam: **o GPT pela conta bateu no limite de uso e só volta em 04/11 às 10h52** (dá pra mandar de
novo no Atelier, "Tentar de novo", ou colar os prompts no ChatGPT). Nada aprovado ainda. O jogo hoje tem um menu de protótipo (Skirmish, LAN, névoa, dinheiro, qualidade, Sair) e o resultado (DEFEAT + Return).

## 1. O estudo (fontes conferidas)

- **O Zero Hour tem 80 telas** (os `.wnd` do `WindowZH.big` do jogo instalado), metade de online (GameSpy/WOL, morto desde 2014).
- **Fundo vivo:** o ShellMap do ZH é uma partida de verdade com semente fixa atrás de quase todas as telas (`Shell.cpp`). O nosso juiz
  é determinístico: o fundo pode ser **um replay escolhido rodando no motor**.
- **Poucos cliques:** o SC2 refez o menu no HotS com "toda tela principal a um clique". Reclamações nos outros: lista de salas que não
  ordena, sala cheia aparecendo livre, chat cobrindo os jogadores (AoM Retold, AoE II DE), recursos cortados (WC3 Reforged).
- **Sala:** cada vaga humano/IA/aberta/fechada, facção, cor, time, a base clicando no mapa (ZH); predefinições salvas (BAR); regras na sala
  desde o começo (o Tempest Rising teve de pôr depois).
- **Carregando (ZH, no código):** o emblema e o texto da sua facção, a música dela, uma barra por jogador na cor dele, o mapa com as posições;
  a IA sem barra (`LoadScreen.cpp`).
- **Fim:** gráficos com marcas de combate (SC2); dá pra olhar o mapa depois de perder (AoE IV); abrir o replay direto do fim (pedido no
  Stormgate); o fundo e a música do placar mudam com a facção (ZH, `ScoreScreen.cpp`).
- **Esc (ZH, no código):** no skirmish pausa; no multi vira "Surrender" e some se você já perdeu (`QuitMenu.cpp`).
- **Rede:** tela de queda com tempo e votos (ZH); "Recover Game" a partir do replay (SC2 HotS); não poder voltar depois de cair é a queixa do AoE IV.
- **Opções:** presets (ZH: Baixa, Média, Alta, Personalizado); o AoE IV ajusta sozinho no 1º boot e avisa quando passa do PC.

Os links estão na conversa de 01/11 e no fim deste documento.

## 2. A lista (P1 = versão jogável; P2 = logo; P3 = futuro)

| # | Tela | O que tem | Prioridade |
|---|---|---|---|
| 1 | **Primeira abertura** | "Preparing effects" (os shaders) com as células enchendo, só na 1ª vez e depois de atualizar; o Auto mede a nossa cena (~20 s, pulável) e diz o preset | P1 |
| 2 | **Menu principal** | fundo vivo; Skirmish, Multiplayer, Replays, Options, Quit; Rematch com a última sala; a versão no canto | P1 |
| 3 | **Sala da partida** (uma só pra skirmish, LAN, Steam e IP) | 8 vagas (humano, IA fácil/média/brutal, aberta, fechada), facção com Random, cor, time; o mapa com as bases clicáveis; regras (dinheiro, superarmas, limite, velocidade); só no online: chat, ping, pronto, expulsar, contagem que cancela se algo mudar, versão conferida | P1 |
| 4 | **Multiplayer** | Steam / LAN / IP direto; lista de salas ordenável que atualiza sozinha; criar, entrar, convidar | P1 |
| 5 | **Carregando** | a sua facção grande (emblema, nome, música), o mapa com as bases, uma barra de células por jogador na cor dele; "Fulano carregando · Expulsar" aos 60 s | P1 |
| 6 | **Menu do Esc** | Resume, Options, Restart (skirmish) ou Surrender (multi), Quit; os dois últimos confirmam | P1 |
| 7 | **Resultado** | VICTORY/DEFEAT sobre o campo parado (dá pra olhar o mapa); a tabela por jogador; o gráfico com as marcas de combate; Watch replay, Rematch, Lobby, Menu | P1 |
| 8 | **Opções** | Gráficos (o preset, resolução, modo, limite de quadros, "medir de novo"), Som, Teclas (com busca e conflito em vermelho), Jogo | P1 |
| 9 | **Avisos de rede** (um desenho só) | esperando jogador (quem, quanto tempo, votos), desconectado (o motivo), versão diferente ("você 0.5.2, anfitrião 0.5.1"), desync (para e salva o relatório) | P1 |
| 10 | **Replays** | a lista (data, mapa, jogadores, duração, resultado, versão); no replay: pausa, velocidade, a visão de cada jogador | P1 |
| 11 | Retomar a partida pelo replay depois de queda ou desync | o lockstep dá quase de graça | P2 |
| 12 | Observador; linha do tempo no replay; o fundo vivo com o replay escolhido; transferir mapas do WorldForge; créditos; silenciar jogador | | P2 |
| 13 | Ranqueado (fila, ladder, cartão do jogador); generais na escolha da facção; o criador de mundos no menu; idioma | | P3 |

## 3. A direção visual

- **O menu é o console da partida, desligado, em cima da guerra.** O mesmo metal escuro com rebites, a mesma fonte e o mesmo hover
  aprovado. Nada de vidro nem cara de aplicativo.
- **A assinatura: as células de energia do console são a barra de progresso do jogo inteiro** (preparando efeitos, o teste do Auto, o
  carregando de cada jogador na cor dele, o ping, a contagem da queda). No 3-2-1 o console acende célula por célula e o HUD sobe.
- **O fundo mostra o campo que muda** (prédio que desaba, palmeira que queima e fica preta) com a câmera de RTS deslizando; sem tela
  preta entre os menus: a câmera do fundo muda de lugar a cada tela e o mapa da sala cresce até virar o carregando.
- **Facção:** o menu, as opções, os replays e o multiplayer são neutros (o aço com o azul); **da escolha na sala em diante, a sua vaga, o
  carregando e o resultado vestem a pele da sua facção** (a mesma regra do HUD). O ZH faz igual: menu fixo, carregando e placar da facção.
- **Custo:** o fundo vivo põe o motor desenhando no menu: limitar a 60 qps e medir; no preset Baixa vira um quadro parado.

## Links do estudo

- [Shell.cpp](https://github.com/TheSuperHackers/GeneralsGameCode/blob/ca9358fcc7/GeneralsMD/Code/GameEngine/Source/GameClient/GUI/Shell/Shell.cpp),
  [LoadScreen.cpp](https://github.com/TheSuperHackers/GeneralsGameCode/blob/ca9358fcc7/Core/GameEngine/Source/GameClient/GUI/LoadScreen.cpp),
  [ScoreScreen.cpp](https://github.com/TheSuperHackers/GeneralsGameCode/blob/ca9358fcc7/GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/ScoreScreen.cpp),
  [QuitMenu.cpp](https://github.com/TheSuperHackers/GeneralsGameCode/blob/ca9358fcc7/GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/QuitMenu.cpp),
  [DisconnectMenu.cpp](https://github.com/TheSuperHackers/GeneralsGameCode/blob/ca9358fcc7/GeneralsMD/Code/GameEngine/Source/GameClient/GUI/DisconnectMenu/DisconnectMenu.cpp)
- [Blizzard, UI do HotS](https://news.blizzard.com/en-gb/starcraft2/10054118/heart-of-the-swarm-ui-update) ·
  [AoM Retold, fórum](https://forums.ageofempires.com/t/lets-talk-about-the-multiplayer-uis/260686) ·
  [AoE II DE, fórum](https://forums.ageofempires.com/t/lobby-browser-should-work-like-if-we-are-in-21th-century/223285) ·
  [BAR, sala](https://www.beyondallreason.info/microblogs/139) ·
  [Tempest Rising, regras da skirmish](https://www.pcgamesn.com/tempest-rising/update-skirmish-custom-settings) ·
  [AoE IV, inverno 2021](https://www.ageofempires.com/news/age-of-empires-iv-winter2021-update/) ·
  [AoE IV, avisos de gráfico](https://support.ageofempires.com/hc/en-us/articles/4407781367700-I-am-Getting-Warning-Popups-While-Changing-My-Graphics-Settings) ·
  [SC2, Recover Game](https://news.softpedia.com/news/Starcraft-2-Heart-of-the-Swarm-s-New-Replay-Feature-Recovers-Interrupted-Games-324080.shtml)

## 5. O fundo do menu principal (01/11, APROVADO)

Reprovados no caminho (registro em `docs/APROVADOS.md`, seção Telas): as três cenas (é cena de jogo; "tipo Red Alert 3" quer dizer sem
cena), a maquete v2 ("muito forte"), o radar copiado do assgets ("não era só ctrl+c ctrl+v"). O pedido que ficou: **"um painel de mapa
radar, mas criativo, detalhado, cheio de coisa criativa, fora da curva, bonito"**, discreto atrás do menu B aprovado.

**Aprovado ("aprovado", 01/11): o painel do radar de vigilância.** A cópia aprovada mora em `docs/concept/ui/menu/menu.html` (com os
emblemas em `a/`, o clique do assgets em `snd/`; a trilha do menu, 28 MB em WAV, fica fora do git em `captures/concept/ui/shell/snd_dl/`).
O rascunho vivo é `captures/concept/ui/shell/menu.html` (servidor `ui-shell`, porta 8134); o vídeo, `shots/v4/menu_radar_painel.mp4`.
A captura em 1920×1080 pelo Chrome sem janela: `node tools/cap.mjs shot|rec ...` (o node de verdade, não o atalho do Volta). Tudo vem
de radar de verdade:
- o tubo PPI de fósforo de longa persistência (o tipo P7: o eco acende claro na passagem do feixe e fica âmbar, apagando devagar). A
  persistência sai do ângulo desde a passagem (máscara cônica), sem acumular quadro, então não deixa fantasma;
- o eco do chão sai do relevo: a encosta virada pro radar devolve mais, e atrás do morro fica a sombra do radar (o radar mora no alto de
  um morro, a 40 m do chão);
- o A-scope (o eco ao longo do feixe agora), a cascata do ESM (ouve a trilha do menu pelo `AnalyserNode` quando ela toca), a lista de
  trilhas, a hora Zulu, a simbologia APP-6 (os quatro exércitos com moldura, escalão, vetor e rótulo), os caças, o helicóptero em
  volta da vila, a chuva, o bloqueio de vez em quando, o míssil ("VAMPIRE") com o eco do impacto, que fica no eco do chão (o chão muda);
- o hover usa o próprio radar: SKIRMISH trava o cursor de rumo e o estrobo de distância no objetivo; MULTIPLAYER interroga o IFF e as
  trilhas respondem com o emblema; OPTIONS gira os botões GAIN e BRILL (o eco sobe); QUIT apaga o tubo, e o clique fecha a imagem
  numa linha e num ponto, como os tubos antigos;
- a base é a do assgets (o fundo, a grade de 48 px, as cantoneiras douradas); a música e o clique são os do site (`snd_dl/`).
- Custo: só canvas 2D. Os rascunhos anteriores ficam em `menu_scenes.html`, `menu_table.html`, `menu_table2.html` e `menu_radar_copia.html`.

## 4. A primeira rodada, olhada com rigor

- **Menu:** forte. O console desligado na borda de baixo é a assinatura que pedimos. Veio uma fileira com os 4 emblemas que não pedi: lê
  como escolha de facção, e o menu é neutro; pode ficar como "os 4 exércitos" ou sair. A placa do logo está vazia (falta o nome do jogo).
- **Sala:** a melhor. Vagas, facção com emblema, time, pronto, ping em células, o mapa em losango com as bases, as regras, o chat e o START
  verde. Ressalva: as abas SKIRMISH/LAN/ONLINE dentro da sala; o modo vem de onde você entrou (no multi a sala é a mesma, com o chat e o ping).
- **Multiplayer:** limpo e denso. Sobrou uma placa de logo vazia no canto. O logo da Steam é de terceiros (no jogo, o ícone dela vem do kit oficial).
- **Carregando:** a facção grande com o lema, o mapa, os comandantes com as barras de células na cor de cada um, a IA com READY. Muito perto do
  ZH (`LoadScreen.cpp`) e do que o estudo pedia.
- **Esc:** pequeno e certo (Resume verde, Surrender vermelho). O fundo usou o HUD antigo de referência (com o painel do comandante no canto);
  no jogo fica o HUD novo atrás.

