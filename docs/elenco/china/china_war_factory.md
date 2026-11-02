# War Factory (China, a forja dos blindados)

## 1. Ficha rápida

| | |
|---|---|
| Papel | a fábrica de veículos: todo blindado da China sai daqui |
| Custo e tempo | $2 000, 15 s |
| Vida e blindagem | 2 000, prédio |
| Energia | −1 |
| Visão | 52,7 m |
| Lote | 24 × 26 m |
| Arma | nenhuma |
| Requer | Supply Center |
| Produz | Battlemaster (Type 99), Gattling Tank, Inferno Cannon e Overlord (os 2 últimos pedem o Propaganda Center) |

## 2. A ideia

**O hangar escuro com o pagode de dois telhados, o forno aceso e o pórtico vermelho sobre a estrela do piso.** A ficha aprovada
(`docs/concept/buildings/china_war_factory.webp`): indústria pesada monumental. O tanque sai pela porta grande, passa debaixo do
pórtico e pisa na estrela.

- **A palavra de cima:** o PAGODE sobre o hangar + o PÓRTICO VERMELHO sobre a ESTRELA do piso.
- **Inspiração real:** as fábricas estatais de blindados (o galpão alto, a ponte rolante, a fundição com o forno) com a tradição
  chinesa por cima (o telhado de pagode, a torre de alto-falantes).
- **Por que existe:** a horda pesada da China nasce aqui. É a peça mais importante da base depois do CC.

## 3. Como funciona

- **No jogo:** $2 000, 15 s, −1 de energia, 2 000 de vida. Fila de até 9. Sem energia, produz a 50–80% da velocidade.
- **O que sai:** o Battlemaster ($800, 10 s), o Gattling Tank ($800, 10 s); com o Propaganda Center, o Inferno Cannon ($900, 15 s)
  e o Overlord ($2 000, 20 s). O tanque nasce dentro e sai pela porta (`exit_lane_m` 6,6 m).
- **Forte contra:** nada; é produção.
- **Fraco contra:** tudo que mira prédio; um tanque derruba em ~67 s, o Overlord em ~29 s.
- **O contra dele:** o raide pesado ou a artilharia. **Ele é o contra de:** o exército inimigo, pela fila.

## 4. Em batalha

- **Temperamento: o ferreiro.** Quente, ruidoso, incansável. O forno nunca apaga.
- **O ritmo:** a cada 10 s um tanque; o barulho da fundição é contínuo e o pórtico bate o compasso.
- **O jogador bom** põe a fábrica atrás do CC, protegida, e o ponto de encontro na frente da linha; mantém a fila cheia.
- **A IA** treina pelo que tem menos (o tanque, 1º da lista, vale por 2); o Inferno segue a regra da artilharia (1 + as defesas
  inimigas conhecidas, com 2 tanques de escolta cada); o Overlord entra quando tem o Propaganda e o dinheiro.
- **Sozinho e ferido:** o forno segue aceso até cair.

## 5. A vida

- **No jogo:** o andaime de aço na obra; abaixo de 35%, o fogo nos 2 pontos do telhado e os buracos.
- **Ideia (parado):** a boca do forno pulsa em laranja (a luz acende o piso em volta e a fumaça que sai da coifa: a fumaça vive na
  luz do mundo); a chaminé listrada solta fumaça escura de fundição; o cacho de cornetas da torre gira devagar; o pórtico (peça
  `_Gantry` já modelada no script, ainda fora dos dados) fica parado em cima da estrela.
- **Ideia (produzindo):** as faíscas de solda dentro do hangar (o brilho azul pela porta); quando o tanque fica pronto, a porta
  sobe, a sirene toca, o pórtico corre pra trás pra dar passagem e o tanque sai com a torre virada pra frente.
- **Ideia (sem energia):** o forno escurece pra brasa, a luz de dentro do hangar apaga, a solda para; a produção segue mais
  devagar (o jogo já faz).
- **Ideia (ferido):** a fumaça da chaminé fica preta e grossa; abaixo de 35%, o fogo no telhado e a calha de escória do forno
  transbordando.

## 6. O visual

- **De cima:** o retângulo do pódio, o pátio rebaixado na frente com a estrela vermelha grande e as faixas amarelas, o hangar com o
  pagode de 2 telhados no meio, o pórtico vermelho atravessado sobre a estrela, a chaminé listrada no canto.
- **Assinatura:** o pórtico vermelho + a estrela do piso + o forno aceso.
- **Cores:** concreto escuro oliva; telha verde; o vermelho do jogador no pórtico, nos 3 estandartes, na estrela do piso e na
  borda dos telhados; o laranja do forno (luz, não pintura).
- **Nunca:** trilho de trem (o pórtico corre num trilho embutido na canaleta, sem dormente); mais de 4 estandartes.

## 7. Tiro, projétil, impacto, destruição

- **Impacto (ideia):** o canhão abre a chapa do hangar (a luz de dentro aparece pelo buraco); o míssil no forno solta uma chuva de
  brasas.
- **A morte (no jogo):** a morte de prédio (a grande no meio, 3 menores, o chão queimado, o entulho: $200 em 14 s pro Dozer).
- **A morte (ideia):** o forno estoura por último (um clarão laranja baixo e o metal derretido escorrendo no piso, brilhando por
  ~15 s); o pagode afunda no hangar; o pórtico tomba sobre a estrela.

## 8. O som

- **Seleção (variantes):** a sirene industrial curta; o martelo-pilão batendo uma vez (o "BAM" grave com o eco do galpão); o forno
  rugindo; a corrente da ponte rolante.
- **As falas** (inglês; o engenheiro-chefe, voz forte, orgulhosa, com o barulho da fábrica atrás):
  - selecionado: "War factory, ready to forge." / "Steel for the motherland." / "What shall we build?" / "The furnace never
    sleeps." / "Assembly line ready."
  - produzindo: "Vehicle on the line." / "Tank rolling out!" / "Another one for the front."
  - sob ataque: "The factory is under attack!" / "Protect the assembly line!" / "They're hitting the furnace!"
  - pronto: "War factory operational." / "The forge is lit."
- **O corpo:** o rugido baixo e constante do forno; o pilão a cada ~6 s; a solda chiando; a chaminé "puxando"; a cada tanque, a
  sirene e a porta de aço subindo.
- **A construção:** as vigas grandes içadas, o rebite pneumático, o telhado de telhas assentado; no fim, o forno acendendo (o
  "fuuump" do gás e o rugido subindo) e a sirene.
- **A morte:** os estouros; o forno estourando (o "whump" grave e o chiado do metal derretido); o pagode caindo; o pórtico tombando
  com o metal longo.

## 9. Ideias pra depois

- **O pátio de reparo:** o veículo da China que para na estrela do piso conserta 2% da vida por segundo (o pórtico desce o braço
  sobre ele); custa nada, mas ocupa a saída (a fábrica fica mais lenta enquanto conserta).
- **A linha de montagem dupla:** um upgrade caro ($1 500) que permite 2 Battlemasters ao mesmo tempo (o tanque barato em dobro), a
  horda literalmente.
- **A sucata vira aço:** a carcaça desmanchada pelo Dozer a até 40 m da fábrica dá 25% a mais (a fundição derrete ali mesmo); de
  cima, o Dozer leva o pedaço até a boca do forno e o forno cospe faísca. A sucata, que hoje é extra, vira estratégia de base.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#china_war_factory` (feito no fim, com o modelo provisório como anexo de forma).
