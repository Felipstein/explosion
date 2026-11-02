# Command Center (GLA, o pátio do comandante)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o centro de comando: onde a base nasce, de onde sai o Worker, quem liga os poderes do general |
| Custo e tempo | $2 000, 45 s |
| Vida e blindagem | 5 000, prédio |
| Energia | nenhuma (nada da GLA usa energia) |
| Visão | 79 m |
| Lote | 28 × 28 m (o maior lote da GLA) |
| Arma | nenhuma |
| Requer | nada (a GLA começa com ele pronto e **2 Workers**) |
| Produz | Worker ($200, 3 s) |
| Conta pra vitória | sim |

## 2. A ideia

**O pátio de adobe murado com o prédio de cúpula baixa e a antena de rádio torta e alta.** Não é um quartel-general; é uma casa
grande tomada e fortificada: o portal em arco, as duas torres redondas de kasbah na frente, a cúpula, e a antena de sucata com o
prato velho girando. No canto, o Worker já está desmontando uma picape.

- **A palavra de cima:** o CÍRCULO da cúpula no meio + a ANTENA TORTA com o prato e os 3 estais em leque.
- **Inspiração real:** as kasbahs do Marrocos (Aït Benhaddou: as torres redondas afuniladas, o portal em arco), as casas-pátio do
  Iraque e da Síria (o teto plano, a escada maciça por fora, o vergalhão espetado esperando o próximo andar, a caixa d'água no
  teto) e as torres de rádio de sucata com estais.
- **Por que existe:** é a base da GLA, e a única coisa que ela tem que parece "governo". Sem usina pra proteger, a GLA defende o
  CC e o dinheiro.

## 3. Como funciona

- **No jogo:** 5 000 de vida, visão de 79 m, produz o Worker (o construtor a pé, $200, 3 s), liga os poderes (`grants_powers`).
  A partida começa com ele e 2 Workers (`factions`). Não usa energia: nada da GLA usa.
- **O preço do adobe:** a vida é a mesma do CC da China e da USA (5 000); a diferença da GLA está nos outros prédios (sem energia,
  sem o reator pra perder, mas sem defesa que atire longe).
- **Forte contra:** o tempo; o Worker é barato e rápido de repor (3 s).
- **Fraco contra:** cerco de tanques e artilharia; não atira.
- **O contra dele:** exército pesado com tempo. **Ele é o contra de:** nada; é o objetivo.

## 4. Em batalha

- **Temperamento: o chefe de clã.** Desconfiado, orgulhoso, improvisado. Fala alto no rádio e não confia em ninguém.
- **O ritmo:** o rádio chiando o tempo todo; o gerador a diesel tossindo; o prato girando devagar (2 rpm).
- **O jogador bom** espalha a base (sem energia, nada depende de nada perto), põe o Stinger Site cobrindo o CC e usa o CC como
  isca: o inimigo que vem nele passa pela vila onde estão os RPGs.
- **A IA** começa com ele e os 2 Workers; pula a usina da lista (a GLA não tem) e vai do Supply Stash à fábrica.
- **Sozinho e ferido:** a bandeira da antena é a última a cair.

## 5. A vida

- **No jogo:** o prato de satélite velho gira na antena (peça `_Dish`, 2 rpm no `Units.json`); o andaime de **madeira** na obra
  (o da GLA); abaixo de 35%, o fogo nos 2 pontos do telhado e os buracos.
- **Ideia (parado):** a fumaça preta do gerador a diesel no puxadinho de chapa, em tossidas; a bandeira verde no topo da antena
  batendo no vento do mapa; a luz vermelha piscando no alto da antena; a lona do canto do rádio balançando.
- **Ideia (produzindo):** o Worker sai pelo arco do prédio, atravessa o pátio e passa pelo portão (a função aparece).
- **Ideia (ferido):** a antena entorta mais a cada faixa de dano (60%, 35%) e o prato para de girar abaixo de 35%; o gerador
  engasga e a luz da antena falha.

## 6. O visual

- **De cima:** o lote quadrado de muro de adobe com a borda verde, a cúpula grande no meio do prédio, as 2 torres redondas nos cantos
  da frente, a antena torta com o prato e os estais em leque num canto, o canto de sucata debaixo da rede de camuflagem.
- **Assinatura:** a antena torta com o prato velho.
- **Cores:** adobe cor de areia, chapa enferrujada, lona, rede mosqueada; o verde do jogador nas bandeiras, nos rabiscos à mão e na
  borda dos muros.
- **Nunca:** mesquita, minarete, crescente, escrita religiosa; morro ou penhasco no lote; "vila inteira" de barracas.

## 7. Tiro, projétil, impacto, destruição

- **Impacto (ideia):** o adobe absorve: o tiro faz um buraco redondo e solta poeira cor de areia em nuvem (mais poeira que o
  concreto da China); a bala na chapa faz faísca e "plong".
- **A morte (no jogo):** a morte de prédio (a explosão grande no meio e 3 menores em ~1,5 s, o chão queimado, o entulho: $200 em 14 s
  pro Worker).
- **A morte (ideia):** a cúpula racha e afunda pra dentro; os muros de adobe desmoronam em montes de terra (não em blocos como o
  concreto); a antena tomba em arco, os estais chicoteando; uma nuvem de poeira de barro grande e lenta (volume, na luz do mundo).

## 8. O som

- **Seleção (variantes):** o rádio chiando com uma voz cortada; o gerador a diesel tossindo; o portão de chapa rangendo; o prato da
  antena girando com o rangido do mancal seco.
- **As falas** (inglês; o comandante da GLA no rádio, voz rouca, intensa, desconfiada, com o chiado do rádio velho):
  - selecionado: "Command, speak." / "The commander is listening." / "What do you need?" / "We build from nothing." / "The desert
    is ours."
  - produzindo: "Worker sent." / "Another pair of hands." / "Get to work, brother."
  - sob ataque: "They found the compound!" / "Defend the walls!" / "Everyone, to the command center!" / "They will not take this
    house!"
  - pronto: "Command center ready." / "We are here to stay."
- **O corpo:** o rádio sempre ligado ao fundo (vozes cortadas, estática); o gerador tossindo; o vento batendo na lona; de vez em
  quando, um martelo batendo no canto de sucata.
- **A construção:** do andaime de madeira ao pronto: as pás jogando terra, os tijolos de adobe batendo, as chapas sendo pregadas, a
  antena içada na corda; no fim, o gerador dando a partida e o rádio ligando.
- **A morte:** os estouros; o adobe desmoronando (o som surdo de terra caindo, não de concreto quebrando); a antena gemendo e
  caindo; o rádio morrendo em estática.
- **A cara da GLA:** o rádio chiado, o motor velho, o improviso.

## 9. Ideias pra depois

- **O túnel de fuga:** quando o CC cai, os Workers e soldados que estiverem dentro do lote saem por um túnel no Supply Stash mais
  perto (a GLA nunca morre de uma vez).
- **Os informantes:** o prato da antena capta o rádio inimigo: o CC mostra no minimapa, por 3 s, o ponto de onde saiu cada unidade
  nova do inimigo (a GLA vive de informação roubada; ela não tem o satélite da USA).
- **A sucata do pátio:** a Technical sucateada no canto do lote é peça real: se o jogador clicar nela com o Worker, vira $100 uma vez
  por partida (o "dinheiro guardado embaixo do colchão").

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#gla_command_center` (feito no fim, com o modelo provisório como anexo de forma).
