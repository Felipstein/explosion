# Pantsir Site (Rússia, o Pantsir fixo)

## 1. Ficha rápida

| | |
|---|---|
| Papel | a defesa de base: antiaérea de longe e anti-infantaria de perto |
| Nome na tela | Pantsir Site (a chave é `russia_pantsir_site`) |
| Custo e tempo | $1 200, 25 s |
| Vida e blindagem | 1 000, prédio |
| Energia | −3 (e **para sem energia**: `needs_power`) |
| Visão | 94,8 m |
| Lote | 8 × 8 m (a menor peça da base) |
| Arma (chão) | os canhões de 30 mm: gattling 6 a cada 0,25 s, alcance 55 m |
| Arma (ar) | míssil: 45 explosivo a cada 1,5 s, alcance **110 m** (a defesa de base de mais longe do jogo; empata com o Pantsir-S1), a 160 m/s |
| Torre | gira 180°/s; volta pra frente 2 s depois de parar; o radar de busca gira a 30 rpm |
| Requer | Gas Power Plant |
| Conta pra vitória | não |
| Estado | nos dados (`Units.json`) e no modelo provisório; fora do menu até os modelos da Rússia serem importados |

## 2. A ideia

**O módulo do Pantsir no reparo fixo, dentro do anel de sacos.** O módulo de combate tirado do caminhão e montado num pedestal de aço
sobre a laje octogonal: o radar de tiro redondo na frente (o "rosto"), os 2 canhões duplos de 30 mm, os 2 blocos de 6 tubos de
míssil por fora e, em cima, no eixo do giro, o radar de busca redondo que gira sozinho. Em volta, 3 fiadas de sacos de areia, as
caixas de munição verdes e o gerador.

- **A palavra de cima:** o PRATO REDONDO girando em cima dos canos e dos tubos, no CÍRCULO de sacos (duas assinaturas redondas).
- **Inspiração real:** o Pantsir-S1 (o módulo 96K6: os 2A38M de 30 mm, os mísseis 57E6, o radar de busca em cima e o de tiro na
  frente) e as posições fixas de defesa de ponto das bases russas, com os sacos em volta.
- **Por que existe:** a defesa aérea em camadas (estudo `docs/FACCAO_RUSSIA.md` §4.2): a resposta da Rússia ao ar da USA, e o
  canhão pra infantaria que chega na base.

## 3. Como funciona

- **No jogo, no ar:** 45 a cada 1,5 s a 110 m: **30/s no avião**, **39/s no helicóptero** (explosivo 130%), 15/s no Chinook (50%). O
  Raptor (160) e o Apache (220) caem com 4 mísseis (~4,5 s); o Chinook (300) com 14 (~20 s). A 110 m ele alcança além de onde o
  Raptor atira (84 m) e do Apache (53 m): a aeronave que vem atacar a base apanha antes.
- **No jogo, no chão:** 24/s em soldado (gattling 100%), **2,4/s em tanque e em prédio** (gattling 10%). Mata um Red Guard ou um
  Rebel (120) em ~5 s, um Ranger (180) em ~7,5 s, um soldado antitanque (100) em ~4 s. Alcança 55 m: mais que o tanque (39,5 m) e o
  soldado (26 m), menos que a Gattling Cannon e o Patriot (59 m).
- **O míssil, nos dados:** um projétil reto a 160 m/s, sem a guiagem (`missile`) e sem o míssil modelado (`missile_model`); o
  Pantsir-S1 móvel tem os dois, o fixo ainda não (ideia, §7).
- **O par:** a Gattling Cannon da China ($1 200): menos contra a infantaria (24/s contra 40/s), mais contra o ar (30/s a 110 m contra
  20/s a 105 m) (`_balance` no `elenco_units.py`).
- **Sem energia:** para de atirar (a usina caiu = a defesa morreu junto).
- **Forte contra:** o Apache, o Raptor, a infantaria em grupo.
- **Fraco contra:** tanque (um tanque USA o derruba em ~33 s e levaria ~200 s pra morrer dele), veículo leve (a Technical leva ~75 s),
  artilharia (o TOS-1A a 70 m, o Inferno e o Rocket Buggy a 79 m, o Tomahawk a 85 m), perder a energia.
- **O contra dele:** o tanque e a artilharia; o raide na usina. **Ele é o contra de:** o Apache, o Raptor, os Rangers, os Rebels e
  os Red Guards.

## 4. Em batalha

- **Temperamento: a sentinela de radar.** Fria, vigilante, numérica. Vê tudo antes de atirar; quando atira, a conta já está feita.
- **O ritmo:** o radar dá uma volta a cada 2 s; no ar, o "fuum" do míssil a cada 1,5 s; no chão, o rasgo curto e grave dos canhões.
- **O jogador bom** põe 2 cobrindo a usina e o depósito, perto (não colado) da usina: fora dos 18 m do raio dela, dentro da rede que
  ela alimenta. Contra a USA, um na rota do Apache, longe da base.
- **A IA** o constrói na lista da base (depois da fábrica), quando tem a usina.
- **Sozinho e ferido:** atira até o fim; abaixo de 35%, o fogo nos 3 pontos.

## 5. A vida

- **No jogo:** a torre gira 180°/s pro alvo e volta pra frente 2 s depois (peça `_Turret`); o radar de busca gira em cima, no eixo
  do giro, a 30 rpm (peça `_Radar`: não sai do lugar quando a torre vira); o traçante e o clarão pequeno dos canhões (`light_gun`: o
  clarão de 0,4 m, o risco de 3,5 m, o espalhado de 2 m); o andaime de aço na obra; abaixo de 35%, o fogo nos 3 pontos.
- **Ideia (parado):** o radar de tiro da frente (o "rosto") faz uma varredura curta; a luz do gerador; as tampas vermelhas dos tubos
  fechadas.
- **Ideia (alvo no ar):** o radar de busca **para** apontado pro alvo, o rosto vira, os blocos sobem; a tampa do tubo salta no
  disparo e fica aberta até a recarga.
- **Ideia (sem energia):** o radar para, a torre volta pra frente e baixa os canos: de cima se lê "desligado".

## 6. O visual

- **De cima:** o octógono de concreto com o anel de sacos de juta, a torre no meio com o prato redondo de grade por cima, os 2 blocos
  de 6 tubos com as tampas vermelhas dos lados, os canos finos pra frente, as caixas de munição verdes e a lona dobrada atrás.
- **Assinatura:** o prato redondo + o rosto de canos e tubos.
- **Cores:** o concreto, os sacos de juta; o aço frio azulado dos tanques na torre; a cor do jogador na lona dobrada atrás da torre.
- **Nunca:** o tambor rotativo (é da Gattling Cannon da China); os 3 tubos no suporte improvisado (é do Stinger Site da GLA); Z ou V.

## 7. Tiro, projétil, impacto, destruição

- **A boca:** os canhões: os clarões curtos alternando esquerda-direita, sem sopro de canhão de tanque; o míssil (ideia): o "pop" da
  ejeção, a nuvem branca curta na boca do tubo, o motor acendendo a 1 m e o rastro branco reto subindo.
- **O projétil:** o traçante dos canhões; o míssil quase reto e muito rápido (160 m/s nos dados), o rastro fino que fica ~1 s no ar
  (ideia: o míssil modelado e a guiagem, como no Pantsir-S1).
- **O impacto:**
  - no ar: o estouro de estilhaço ao lado da aeronave (o míssil de verdade explode por proximidade), a fumaça preta do motor;
  - no soldado: a linha de poeira andando até ele;
  - no tanque: a cascata de faíscas, quase nada (10%);
  - no prédio: lascas pequenas.
- **A morte (no jogo):** a morte de prédio (a grande no meio, 3 menores, o chão queimado, o entulho: $120 em 10 s pro construtor).
- **A morte (ideia):** os mísseis dos blocos cozinham e saem tortos girando no chão (uma cena de 2 s, sem dano); a munição dos
  canhões pipoca; a torre tomba do pedestal; os sacos rasgam e a areia escorre.

## 8. O som

- **Seleção (variantes):** o bip do radar de busca (um a cada volta: 2 s); o servo da torre; o "clack" duplo da culatra dos 30 mm; o
  rádio de onda curta.
- **As falas** (inglês, com o sotaque russo leve; o operador de radar, voz calma, monótona, numérica):
  - selecionado: "Pantsir ready." / "Radar sweeping." / "Sky is clean." / "Tracking." / "Air defense on watch."
  - atacando: "Target locked." / "Launch!" / "Guns on the infantry." / "Target destroyed."
  - sob fogo: "Position under fire!" / "Still tracking!"
  - sem energia: "No power, radar is down!"
  - pronto: "Pantsir site operational."
- **O corpo:** o gerador ronronando; o motor do radar (o "uuun" a cada 2 s); o ventilador do módulo.
- **A arma:** os canhões de 30 mm: o "DUM-DUM-DUM" grave e rápido, tiros que ainda se separam (o contrário do rasgo contínuo do
  rotativo da China); a cadência do dano (0,25 s) é do jogo; o míssil: o "pop" da ejeção, o "SHHHH" muito rápido subindo, o estouro
  seco no alto.
- **A construção:** a laje, os sacos jogados (o "fluff" grave), o módulo assentado no pedestal com o "clank"; no fim, o radar
  começando a girar e o primeiro bip.
- **A morte:** os estouros; os mísseis chiando no chão; a torre caindo no concreto; a areia escorrendo.
- **A cara da Rússia:** o metal grave, o radar, a voz baixa e seca.

## 9. Ideias pra depois

- **O míssil contra o míssil:** o Pantsir derruba o Tomahawk em voo (o de verdade faz defesa de ponto contra míssil de cruzeiro): um
  míssil dele contra cada míssil que entra a 110 m, com 50% de acerto. A base russa que a precisão da USA não abre de graça.
- **A rede de radar:** dois Pantsir fixos a menos de 60 m dividem os alvos sem gastar dois mísseis no mesmo (a mira assistida que a
  Patriot já tem), e o Pantsir-S1 móvel perto entra na rede.
- **A bateria S-400** (estudo §4.2): no fim da árvore, a camada de cima da defesa aérea: o alcance mais longo do jogo contra avião,
  caro e lento de recarregar. Não é superarma: não atira no chão.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#russia_pantsir_site` (feito no fim, com o modelo provisório como anexo de forma).
