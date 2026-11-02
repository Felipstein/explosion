# Command Center (China, o coração da base)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o centro de comando: onde a base nasce, de onde sai o Dozer, quem liga os poderes do general |
| Custo e tempo | $2 000, 45 s |
| Vida e blindagem | 5 000, prédio |
| Energia | 0 (não gera nem gasta) |
| Visão | 79 m (a maior da base China) |
| Lote | 24 × 28 m (o maior prédio da China) |
| Arma | nenhuma |
| Requer | nada (a China começa com ele pronto e 1 Dozer) |
| Produz | Dozer |
| Conta pra vitória | sim |

## 2. A ideia

**O salão de pagode de 3 telhados sobre a fortaleza.** O poder do Estado em concreto: um pódio pesado de paredes inclinadas, o
portão da garagem no meio e, em cima, o salão de colunas vermelhas com os três telhados verdes de borda vermelha. Na frente, a
grande estrela vermelha pintada no piso, por onde o Dozer sai.

- **A palavra de cima:** os TRÊS TELHADOS do pagode, mais o círculo do radar de treliça girando no terraço da esquerda.
- **Inspiração real:** a Porta da Paz Celestial (Tiananmen) e os salões da Cidade Proibida (o pódio-fortaleza com o salão em
  cima), em concreto escuro puxado pro oliva. Real no material, monumental na forma.
- **Por que existe:** é a base. Perder o CC é perder o Dozer de reposição e os poderes; ele é a âncora da defesa e o alvo final.

## 3. Como funciona

- **No jogo:** 5 000 de vida (o prédio mais duro da China), visão de 79 m, produz o Dozer ($1 000, 5 s), liga os poderes do general
  (`grants_powers`). A partida começa com ele pronto e 1 Dozer (`factions` no `Units.json`).
- **Quanto aguenta:** contra o tanque básico (30/s) são ~167 s de um tanque sozinho; contra o fuzil (50% no prédio) quase nada;
  contra o gattling (10% no prédio) nada. Quem derruba CC é tanque em grupo, artilharia e o Overlord (69,6/s: ~72 s sozinho).
- **Forte contra:** o tempo. Ele aguenta; o resto da base cai antes.
- **Fraco contra:** cerco de tanques e artilharia (Tomahawk, Rocket Buggy); não atira em nada.
- **O contra dele:** qualquer exército pesado que chegue com tempo. **Ele é o contra de:** nada; ele é o objetivo.

## 4. Em batalha

- **Temperamento: o magistrado.** Imóvel, solene, olha tudo de cima. Não reage, registra.
- **O ritmo:** lento, cerimonial. O radar girando a 8 rpm é o relógio da base.
- **O jogador bom** cerca o CC com as Gattling Cannons e põe o reator longe dele (o reator que derrete leva o CC junto: 400 até
  13 m da borda do lote do reator).
- **A IA** começa com ele; usa o Dozer pra seguir a lista (usina, centro, fábrica, defesa).
- **Sozinho e ferido:** o telhado pega fogo abaixo de 35%; segue de pé até o fim, solene.

## 5. A vida

- **No jogo:** o radar de treliça gira (peça `_Radar`, 8 rpm no `Units.json`); o andaime de aço sobe na obra (peça `_Scaffold`);
  abaixo de 35%, o fogo nos 2 pontos do telhado (`fire_points_m`) e os buracos na estrutura.
- **Ideia (parado):** as lanternas vermelhas do beiral balançam no vento; a baliza do mastro de rádio pisca devagar (1 a cada
  2 s); o estandarte bate; à noite, a luz amarela das janelas de treliça e o refletor varrendo o pátio.
- **Ideia (produzindo):** o portão de aço sobe (2 s), a luz giratória âmbar acende no lintel, o Dozer sai sobre a estrela do piso.
- **Ideia (sem energia):** a China sem energia só produz mais devagar (50–80%); o CC não para, mas as janelas apagam e o radar
  gira a meia velocidade, pra o jogador ler "falta energia" de cima.
- **Ideia (ferido):** acima de 35%, fumaça cinza saindo do salão; a telha que cai do beiral.

## 6. O visual

- **De cima:** o retângulo grande com o salão escuro emoldurado pelo terraço claro; os três telhados em camadas no meio; o círculo
  do radar à esquerda; a estrela vermelha no pátio da frente.
- **Assinatura:** os três telhados de pagode, o maior conjunto de telhados da base.
- **Cores:** concreto escuro oliva; telha verde vidrada; o vermelho do jogador no pórtico (paifang), nas colunas do salão, na
  borda dos telhados, nos estandartes (3: 2 no terraço, 1 no mastro) e na estrela do piso.
- **Nunca:** trem ou trilho; mais de 4 estandartes; cruz vermelha; concreto claro (confunde com a USA).

## 7. Tiro, projétil, impacto, destruição

- **Impacto (ideia, por tipo):** o canhão arranca lascas de concreto e poeira cinza; o fuzil só risca a parede (faísca pequena,
  50%); o míssil abre o buraco na parede inclinada e a telha voa.
- **A morte (no jogo):** a explosão grande no meio e 3 menores espalhadas em ~1,5 s, o chão queimado na pegada, o entulho que fica
  (cobre soldado e o Dozer desmancha: 10% do custo, $200 em 14 s).
- **A morte (ideia):** os três telhados caem um dentro do outro, de cima pra baixo (o de cima afunda primeiro); o radar tomba pro
  lado; o estandarte queima por último.

## 8. O som

- **Seleção (variantes):** o gongo de bronze curto e grave; o chiado do alto-falante abrindo + 2 notas de corneta; o motor do
  radar rodando com o rangido do mancal; o eco de botas marchando no pátio.
- **As falas** (inglês; o locutor do Estado, voz grave e firme, com o eco do alto-falante):
  - selecionado: "Command, awaiting directive." / "The State is listening." / "Speak, Commander." / "All sections report." /
    "Command center online."
  - produzindo: "Engineers dispatched." / "A Dozer for the people." / "Construction unit ready."
  - sob ataque: "The command center is under attack!" / "Defend the heart of the base!" / "Hold the walls!" / "Enemy at the
    gates!"
  - pronto: "Command center established." / "The foundation is laid."
- **O corpo:** o zumbido grave do transformador; o motor do radar (um "uuum" a cada volta, 8 rpm = a cada 7,5 s); de vez em quando,
  o alto-falante lendo um aviso abafado ao longe e o coro distante de soldados.
- **A construção:** do andaime ao pronto: o martelo pneumático, o guincho, o concreto sendo despejado, as chapas de aço batendo;
  no fim, o gongo e o hino curto (3 notas).
- **A morte:** os estouros em série; o estrondo do salão caindo (madeira e telha); o radar raspando o concreto; o alto-falante
  chiando e morrendo no meio de uma palavra.
- **A cara da China:** gongo, alto-falante, coro ao longe.

## 9. Ideias pra depois

- **O discurso:** com o Propaganda Center pronto, o CC transmite o discurso do general por toda a base: as unidades a até 40 m dele
  recarregam 10% mais rápido enquanto ele estiver acima de 50% de vida (cada pancada no CC "desafina" o discurso).
- **O bunker de comando:** abaixo de 25%, as portas de aço descem e o CC fica 30 s com o dano recebido pela metade, uma vez por
  partida (a China não cai no primeiro golpe).
- **Os leões de pedra:** os dois leões do pórtico ficam de pé até o fim; se o CC cair, um dos leões fica no entulho, inteiro (o
  monumento sobrevive à ruína: a história do mapa).

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#china_command_center` (feito no fim, com o modelo provisório como anexo de forma).
