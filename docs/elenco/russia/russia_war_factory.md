# Tank Plant (Rússia, o pavilhão de telhado-serra)

## 1. Ficha rápida

| | |
|---|---|
| Papel | a fábrica de veículos: todo blindado da Rússia sai daqui |
| Nome na tela | Tank Plant (a chave é `russia_war_factory`) |
| Custo e tempo | $2 000, 15 s |
| Vida e blindagem | 2 000, prédio |
| Energia | −1 |
| Visão | 52,7 m |
| Lote | 26 × 28 m |
| Arma | nenhuma |
| Requer | Supply Depot |
| Produz | T-90M, BMPT Terminator, Pantsir-S1; com o Research Institute, o TOS-1A e o T-14 Armata |
| Estado | nos dados (`Units.json`) e no modelo provisório; fora do menu até os modelos da Rússia serem importados |

## 2. A ideia

**O pavilhão comprido de telhado-serra com os trilhos saindo pelo portão.** O salão de montagem soviético: as paredes de painel
pré-moldado com as juntas fundas, o telhado-serra de chapa verde com as vidraças dos dentes, o portão alto de correr com as 2 folhas
verdes abertas, os trilhos saindo pelo pátio de lajes debaixo do pórtico de treliça com os holofotes e a placa vermelha. Do lado, a
administração de 2 andares com o letreiro de telhado; do outro, o telheiro com as rodas de reserva e as lonas.

- **A palavra de cima:** as 6 FAIXAS VERDES da serra (o que a USA e a China não têm), com os trilhos saindo pela frente.
- **Inspiração real:** a Uralvagonzavod (Nizhny Tagil, onde o T-90 nasce) e os pavilhões industriais de painel dos anos 70–80: o
  telhado-serra é a cara da indústria pesada soviética.
- **Por que existe:** o blindado pesado da Rússia nasce aqui; é a peça mais importante da base depois do Shtab.

## 3. Como funciona

- **No jogo:** $2 000, 15 s, −1 de energia, 2 000 de vida. Fila de até 9. Sem energia, produz a 50–80% da velocidade.
- **O que sai:** o T-90M ($950, 11 s), o BMPT Terminator ($900, 10 s), o Pantsir-S1 ($1 000, 10 s); com o Instituto, o TOS-1A
  ($1 200, 15 s) e o T-14 Armata ($1 800, 18 s). São 5 veículos de combate (a China tem 4). O veículo nasce dentro e sai pelo portão
  (`exit_lane_m` 6,6 m).
- **Forte contra:** nada; é produção.
- **Fraco contra:** tudo que mira prédio: um tanque básico a derruba em ~67 s, o T-14 em ~40 s, o Overlord em ~29 s.
- **O contra dela:** o raide pesado ou a artilharia. **Ela é o contra de:** o exército inimigo, pela fila.

## 4. Em batalha

- **Temperamento: o mestre de linha.** Pesado, paciente, pontual. Um tanque de cada vez, sem pressa, sem defeito.
- **O ritmo:** a cada 11 s, um T-90M; o portão de correr, o motor dando a partida, o tanque saindo nos trilhos.
- **O jogador bom** põe a fábrica atrás do Shtab, protegida, e o ponto de encontro atrás da linha; mantém a fila cheia e mistura 1
  BMPT pra cada 2 ou 3 T-90M (a infantaria) e 1 Pantsir-S1 por grupo contra a USA (o ar).
- **A IA** treina pelo que tem menos, com o T-90M (o 1º da lista) valendo por 2; o Pantsir-S1 entra como armado comum (o canhão
  dele atira no chão, então a IA não o trata como antiaéreo puro); o TOS-1A segue a regra da artilharia (tem alcance mínimo: 1 + as
  defesas inimigas conhecidas, com 2 tanques de escolta cada); o T-14 entra quando tem o Instituto e o dinheiro.
- **Sozinha e ferida:** abaixo de 35%, o fogo nos 3 pontos; segue produzindo até cair.

## 5. A vida

- **No jogo:** o andaime de aço na obra (peça `_Scaffold`); as 3 bandeiras lisas na cor do jogador na frente da administração
  (`flags`); abaixo de 35%, o fogo nos 3 pontos (`fire_points_m`). Nada se mexe na malha.
- **Ideia (parado):** pelo portão aberto, a luz de dentro: o casco em montagem nos trilhos e a ponte rolante amarela (os dois já
  modelados dentro do salão); o fio de fumaça da chaminé de aço estaiada; os holofotes do pórtico acesos à noite.
- **Ideia (produzindo):** a faísca de solda azul lá dentro; quando o veículo fica pronto, a sirene curta, o veículo sai pelos
  trilhos e passa debaixo do pórtico com a torre virada pra frente.
- **Ideia (sem energia):** os holofotes e a luz de dentro apagam, a solda para; a produção segue mais devagar (o jogo já faz).
- **Ideia (ferida):** as vidraças da serra estouram em fila, dente por dente.

## 6. O visual

- **De cima:** o retângulo grande com o salão de telhado-serra verde (as faixas paralelas), a administração de teto plano do lado
  com o letreiro vermelho na borda, o telheiro de chapa verde do outro lado com as lonas, o pórtico de treliça atravessado na frente
  do portão com a placa vermelha, os trilhos no pátio, a chaminé de aço fina nos fundos, os mastros das bandeiras na frente.
- **Assinatura:** o telhado-serra.
- **Cores:** o painel cinza com as juntas escuras, a chapa verde; o vermelho fixo na estrela contornada do frontão, na placa do
  pórtico e no letreiro; a cor do jogador nas 3 bandeiras e nas lonas das pilhas do telheiro.
- **Provisório:** as vidraças da serra olham pra trás (`docs/elenco/TAREFAS.md` 32: de cima, a câmera vê a chapa e não o vidro).
- **Nunca:** pagode e forno aceso (são da China); Z ou V; letreiro que forme palavra.

## 7. Tiro, projétil, impacto, destruição

- **Impacto (ideia):** o canhão abre a chapa da serra (a luz de dentro aparece pelo buraco); o míssil no pórtico o deixa torto, os
  holofotes pendurados.
- **A morte (no jogo):** a morte de prédio (a grande no meio, 3 menores, o chão queimado, o entulho: $200 em 14 s pro construtor).
- **A morte (ideia):** a serra cai dente por dente, da frente pro fundo; o pórtico tomba sobre os trilhos; o casco em montagem fica
  no entulho, preto, à vista de cima.

## 8. O som

- **Seleção (variantes):** a sirene curta de turno; o martelo pneumático; a ponte rolante de dentro com a corrente; o V-12 de um
  tanque na bancada de teste roncando ao longe.
- **As falas** (inglês, com o sotaque russo leve; o engenheiro-chefe da linha, voz grave, orgulhosa, sóbria):
  - selecionado: "Tank plant." / "The line is running." / "Steel is ready." / "What do we build, Commander?" / "Quality first."
  - produzindo: "Vehicle on the rails." / "Tank rolling out." / "Another one for the front."
  - sob ataque: "The plant is under attack!" / "Protect the line!" / "They are hitting the hall!"
  - pronto: "Tank plant operational." / "First hull on the rails."
- **O corpo:** o ronco baixo do salão; a solda chiando; a ponte rolante; a cada veículo, o portão de correr nos trilhos e o motor
  dando a partida.
- **A construção:** as tesouras da serra içadas uma a uma, a chapa trapezoidal parafusada (a chave de impacto em série), os trilhos
  assentados; no fim, a sirene e a ponte rolante de dentro correndo.
- **A morte:** os estouros; as vidraças estourando em fila; a chapa da serra caindo (o metal fino e longo); o pórtico tombando.
- **A cara da Rússia:** o metal grave, a indústria pesada, a voz baixa e seca.

## 9. Ideias pra depois

- **A gaiola reforçada:** um upgrade ($800, 30 s): todo blindado que sai ganha a mecânica da gaiola e do Relikt (o estudo §4: o
  1º míssil antitanque a cada 20 s perde 50%); de cima, a gaiola fica mais densa.
- **A linha de modernização:** o T-90M veterano que para no pátio da fábrica pode virar "M2" ($400: +60 de vida e a gaiola nova),
  sem voltar pra fila. A Rússia que reforma o que tem.
- **O trem dos blindados:** nos mapas com trilho (o tipo de mapa), a fábrica construída perto do trilho recebe o tanque pronto pelo
  trem, 20% mais rápido (a Uralvagonzavod manda tudo por trem, de verdade).

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#russia_war_factory` (feito no fim, com o modelo provisório como anexo de forma).
