# Command Center (Rússia, o Shtab)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o quartel-general: onde a base nasce, de onde sai o IMR-3M, quem liga os poderes do general |
| Nome na tela | Command Center (o Shtab, o Штаб: o estado-maior) |
| Custo e tempo | $2 000, 45 s |
| Vida e blindagem | 5 000, prédio |
| Energia | 0 (não gera nem gasta) |
| Visão | 79 m (a maior da base Rússia) |
| Lote | 26 × 28 m (o maior prédio da Rússia, com a fábrica) |
| Arma | nenhuma |
| Requer | nada (a Rússia começa com ele pronto e 1 IMR-3M: o `start` do `elenco_units.py`) |
| Produz | IMR-3M |
| Conta pra vitória | sim |
| Estado | nos dados (`Units.json`) e no modelo provisório; fora do menu até os modelos da Rússia serem importados |

## 2. A ideia

**O bloco de painel em 3 degraus com a torre de Shukhov.** O estado-maior soviético dos anos 70–80 em concreto pré-moldado: o bloco
largo de 2 andares, o 2º degrau e a torre do estado-maior com o frontão e a estrela contornada; no telhado, a torre de treliça
vermelha e branca com a antena girando. Na frente, a garagem com o portão verde de estrelas (por onde o trator sai), a guarita, a
cancela listrada e a cerca de concreto.

- **A palavra de cima:** os 3 RETÂNGULOS em degrau (os terraços de teto azul-acinzentado, um dentro do outro) + o CÍRCULO listrado
  da torre no canto, com a antena girando.
- **Inspiração real:** os prédios de estado-maior dos distritos militares soviéticos (o painel em degraus, o frontão com a
  estrela), a torre de Shukhov da rua Shabolovka (Moscou, 1922: a treliça hiperboloide de vigas retas, a engenharia russa que todo
  mundo reconhece), o portão verde com as estrelas dos quartéis e a cerca PO-2 de losangos. Real no material, nada da guerra de
  hoje.
- **Por que existe:** é a base. Perder o Shtab é perder o trator de reposição e os poderes; ele é a âncora da defesa e o alvo
  final.

## 3. Como funciona

- **No jogo:** 5 000 de vida, visão de 79 m, produz o IMR-3M ($1 000, 5 s) pela garagem (`exit_lane_m` 6,6 m), liga os poderes do
  general (`grants_powers`). Quando a Rússia entrar no menu, a partida começa com ele pronto e 1 IMR-3M.
- **Quanto aguenta:** contra o T-90M (30/s) são ~167 s de um tanque sozinho; o T-14 (50/s) leva ~100 s; o Overlord (69,6/s) ~72 s;
  o fuzil faz 50% no prédio e o tiro rotativo 10%: quase nada. A salva do TOS-1A inimigo faz 210 (o fogo cheio no prédio): 24
  salvas.
- **Forte contra:** o tempo. Ele aguenta; o resto da base cai antes.
- **Fraco contra:** cerco de tanques e artilharia (o Tomahawk a 85 m, o Rocket Buggy e o Inferno a 79 m); não atira em nada.
- **O contra dele:** qualquer exército pesado que chegue com tempo. **Ele é o contra de:** nada; ele é o objetivo.

## 4. Em batalha

- **Temperamento: o chefe do estado-maior.** Frio, metódico, fala pouco. Tudo passa por ele e tudo fica anotado; não reage, planeja.
- **O ritmo:** a antena dá uma volta a cada 10 s (6 rpm): o relógio mais lento da base.
- **O jogador bom** põe a usina a gás longe dele (a usina que cai faz 250 até 9 m da borda do lote dela) e o Pantsir fixo cobrindo o
  céu do Shtab contra o Apache; deixa um IMR-3M de reserva perto pra consertar (2% por segundo: 100 de vida por segundo nele).
- **A IA** começa com ele e usa o trator na lista da base (usina, depósito, fábrica, defesa).
- **Sozinho e ferido:** abaixo de 35%, o fogo nos 3 pontos; segue de pé até o fim, calado.

## 5. A vida

- **No jogo:** a antena de 6 Yagis gira no topo da torre de Shukhov (peça `_Radar`, 6 rpm no `Units.json`); as 4 bandeiras lisas na
  cor do jogador no vento (a do mastro do topo e as 3 do pátio; o `flags`, o pano desenhado pelo jogo); o andaime de aço na obra
  (peça `_Scaffold`); abaixo de 35%, o fogo nos 3 pontos (`fire_points_m`). Os estados de dano do destrutor (`model_damage.py`)
  ainda não entraram nos dados da Rússia.
- **Ideia (parado):** a luz de obstáculo vermelha no topo da antena pisca devagar (o slot `Beacon` já está no modelo); as janelas
  de caixilho branco acesas em fileira à noite (o ritmo do painel); a sentinela no cogumelo verde; a fumaça fina do escape do
  gerador.
- **Ideia (produzindo):** as folhas verdes do portão abrem, a cancela listrada sobe, o IMR-3M sai devagar pelo pátio.
- **Ideia (sem energia):** o Shtab não gasta, mas a Rússia sem energia produz mais devagar (50–80%); as janelas apagam e a antena
  gira a meia velocidade, pra o jogador ler "falta energia" de cima.
- **Ideia (ferido):** o prédio de painel racha pela junta (o jeito real de ele quebrar: o painel solta inteiro, não esfarela); a
  torre de Shukhov entorta.

## 6. O visual

- **De cima:** o retângulo grande de teto azul-acinzentado com os 3 degraus, o círculo vermelho e branco da torre no canto de trás
  com a antena retangular por cima, o letreiro vermelho na borda do telhado, a marquise fina da entrada, a garagem com o portão
  verde aberto, a guarita de telhado verde e o cogumelo verde na frente, a cerca de concreto na borda, os 3 mastros do pátio.
- **Assinatura:** a torre de Shukhov + a estrela contornada no frontão.
- **Cores:** o concreto de painel cinza com as juntas fundas escuras; o teto azul-acinzentado; a chapa verde nos telhados pequenos;
  o vermelho e branco da torre (as faixas de sinalização de aviação: fixas, não são a cor do jogador); a estrela vermelha
  contornada de branco no frontão (no modelo é vermelha fixa, slots `Red` e `White`; o estudo, `docs/FACCAO_RUSSIA.md` §2, quer que
  ela troque com a cor do jogador, como a da China: ideia); a cor do jogador nas 4 bandeiras.
- **Nunca:** Z ou V pintados; a faixa branca; letreiro que forme palavra (o cirílico estêncil é sem sentido de propósito); estrela
  no teto; concreto escuro oliva e telhado de pagode (são da China).

## 7. Tiro, projétil, impacto, destruição

- **Impacto (ideia, por tipo):** o canhão arranca um painel inteiro pela junta (a placa cai e a poeira cinza sobe); o fuzil só risca
  o concreto (faísca pequena, 50%); o míssil abre o buraco e mostra as lajes e os pilares de dentro (o modelo já é oco por
  dentro).
- **A morte (no jogo):** a explosão grande no meio e 3 menores espalhadas em ~1,5 s, o chão queimado na pegada, o entulho que fica
  (cobre soldado; o construtor desmancha: 10% do custo, $200 em 14 s).
- **A morte (ideia):** os degraus desabam de cima pra baixo (o frontão afunda primeiro); a torre de Shukhov torce no eixo e tomba
  inteira, a treliça amassando no telhado; a antena fica no entulho, legível de cima.

## 8. O som

- **Seleção (variantes):** o tom de sintonia do rádio de onda curta (o "piii" e o chiado); o teletipo batendo uma linha; o motor da
  antena com o rangido do mancal; o carimbo batendo no papel.
- **As falas** (inglês, com o sotaque russo leve, nunca caricatura; o oficial de estado-maior, voz grave, baixa, sem pressa):
  - selecionado: "Headquarters." / "Staff is listening." / "Your orders, Commander." / "All districts report." / "The plan is
    ready."
  - produzindo: "Engineer vehicle dispatched." / "The IMR is on its way." / "Sappers, move out."
  - sob ataque: "Headquarters under fire!" / "Hold the perimeter!" / "They are at the gate!" / "Defend the staff!"
  - pronto: "Headquarters established." / "Communications online."
- **O corpo:** o zumbido grave do transformador; o motor da antena (um "uuum" a cada volta: 10 s); o rádio de onda curta ao longe,
  com vozes abafadas lendo números; o teletipo de vez em quando.
- **A construção:** do andaime ao pronto: o guindaste assentando os painéis (o "toc" grave do concreto na junta), a solda das vigas
  da torre, as chapas parafusadas; no fim, o rádio sintoniza (o chiado vira tom) e a antena começa a girar.
- **A morte:** os estouros em série; os painéis caindo em placas; a treliça da torre rangendo e tombando (o metal longo); o rádio
  chiando e morrendo no meio de um número.
- **A cara da Rússia:** o rádio de onda curta, o concreto grave, a voz baixa e seca.

## 9. Ideias pra depois

- **A contrabateria:** com o Instituto pronto, o Shtab marca no minimapa, por 10 s, o ponto de onde a artilharia inimiga atirou (o
  radar de contrabateria da doutrina russa); a artilharia da Rússia responde sabendo onde.
- **O dente de dragão:** o IMR-3M põe uma linha de blocos de concreto antitanque em volta do Shtab (bloqueia veículo, não soldado):
  o pilar "a base com fortificação" (estudo §3) nasce aqui.
- **A torre que fica:** se o Shtab cair, a torre de Shukhov fica no entulho, torta e inteira, até o fim da partida (o monumento que
  sobrevive à ruína: a história do mapa).

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#russia_command_center` (feito no fim, com o modelo provisório como anexo de forma).
