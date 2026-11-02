# Pantsir-S1 (Rússia, o antiaéreo de canhão e míssil)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o antiaéreo que anda com a coluna; o canhão também atira no chão |
| Nome na tela | Pantsir-S1 (a chave é `russia_pantsir`) |
| Custo e tempo | $1 000, 10 s |
| Vida e blindagem | 320, tanque |
| Velocidade | 9,2 m/s (rodas, 8 × 8), giro do casco e da torre 180°/s, acelera e freia forte (63 m/s²) |
| Visão | **79 m** (a maior dos veículos da Rússia) |
| Arma 1 (canhão) | os canhões duplos de 30 mm: rajada de 10 tiros de gattling 6 (0,15 s entre eles), 1,5 s de recarga, 50 m, **no chão e no ar** |
| Arma 2 (mísseis) | 4 mísseis: 45 explosivo (área de 1,3/2,6 m), um a cada 0,6 s, 12 s pra recarregar os 4, **110 m**, só no ar |
| Esmagador | nível 1 |
| Requer | Tank Plant |
| Estado | nos dados (`Units.json`) e no modelo provisório; fora do menu até os modelos da Rússia serem importados |

## 2. A ideia

**O prato redondo girando e o rosto de canos.** O caminhão blindado de 8 rodas com a cabine pequena, a caixa do gerador e, atrás, o
módulo que gira: o radar de tiro redondo na frente (o "rosto"), os 2 blocos de 6 mísseis nos lados, os canhões gêmeos por baixo, a
gaiola com a rede e, por cima de tudo, o radar de busca redondo girando sem parar.

- **A palavra de cima:** o PRATO girando sobre o caminhão comprido, com os canos finos apontando pra frente.
- **Inspiração real:** o Pantsir-S1 (o módulo 96K6 sobre o KamAZ-6560 8×8: os 2 canhões duplos 2A38M de 30 mm, os mísseis 57E6, o
  radar de busca em cima e o de tiro na frente), na escala dos caminhões do jogo (~0,8 do real: 7,9 m).
- **Por que existe:** a defesa aérea em camadas (estudo `docs/FACCAO_RUSSIA.md` §4.2): a resposta da Rússia ao ar da USA, que anda
  junto do tanque. Contra a China e a GLA (sem ar), vale pelo canhão.

## 3. Como funciona

- **No jogo, o canhão:** 60 por rajada a cada ~2,85 s = **~21/s** em soldado, em helicóptero e em avião (gattling 100%), 2/s em tanque
  e em prédio (10%). Mata um Red Guard (120) em ~4 s.
- **No jogo, os mísseis:** a salva de 4 em 1,8 s: **180 no avião, 234 no helicóptero** (explosivo 130%), 90 no Chinook (50%);
  sustentado, ~13/s no avião. O míssil sai a 60° pra cima a 20 m/s, acelera (260 m/s²) até 180 m/s e curva 300°/s.
- **Contra o ar:** o Raptor (160) e o Apache (220) caem com uma salva só (~2 s); o Chinook (300) precisa de 14 mísseis (~40 s só de
  míssil; o canhão ajuda a 50 m). A 110 m ele alcança além de onde o Raptor atira (84 m) e muito além do Apache (53 m).
- **O par:** o Linebacker da USA ($1 000): o míssil mais longe (110 contra 79 m), mais forte (45 contra 30) e mais lento de
  recarregar (12 contra 9 s); o canhão atira também no chão (o do Linebacker só no ar) (`_balance` no `elenco_units.py`).
- **Forte contra:** o Apache, o Raptor, o Chinook; a infantaria a 50 m (o canhão).
- **Fraco contra:** tanque (2/s: o T-90M e o tanque USA o matam em 6 tiros, 10 s), soldado antitanque (3 juntos o matam em ~2–3 s),
  artilharia.
- **O contra dele:** o tanque e o grupo antitanque. **Ele é o contra de:** o Apache, o Raptor, o Chinook.

## 4. Em batalha

- **Temperamento: o vigia do céu.** Calmo, metódico, olhos pra cima. Conta antes de atirar; quando atira, gasta só o que precisa.
- **O ritmo:** o radar dá uma volta a cada 1,5 s; o canhão em rajadas curtas (DUM-DUM-DUM ... pausa); quando o ar chega, a salva
  "fuum-fuum-fuum-fuum" e 12 s de espera.
- **O jogador bom** leva 1 por grupo contra a USA, 10–15 m atrás dos tanques (nunca na frente: no chão ele é casca fina), e usa os 79 m
  de visão pra dar alvo ao TOS-1A e ao Kornet.
- **A IA** o põe na mistura da fábrica como armado comum (o canhão atira no chão, então a regra do antiaéreo puro, "um por 2
  helicópteros inimigos vistos", não vale pra ele).
- **Sozinho e ferido:** contra o chão, só o canhão; foge a 9,2 m/s.

## 5. A vida

- **No jogo:** o radar de busca gira em cima do módulo (peça `_Radar`, 40 rpm, `on_turret`: vira junto com a torre); a torre gira
  180°/s; as 8 rodas giram cada uma pelo centro; o traçante e o clarão do canhão (`light_gun`: 0,4 m, o risco de 3,5 m, o espalhado
  de 2 m); o míssil modelado em voo com o rastro (`missile_model`); a poeira das rodas.
- **Ideia (parado):** os macacos de apoio descem do pedestal (o de verdade se apoia pra atirar parado); o gerador ronrona; o rosto
  faz uma varredura curta.
- **Ideia (alvo no ar):** o radar de busca **para** apontado, o rosto acende e segue, os blocos de mísseis sobem; a tampa do tubo
  salta no disparo.
- **Ideia (ferido):** o radar gira aos trancos; uma tampa de tubo fica aberta, torta.

## 6. O visual

- **De cima:** o caminhão comprido de 8 rodas (7,9 × 2,4 m), a cabine quadrada na frente com o rolo vermelho no teto, a caixa do
  gerador com as venezianas, o módulo largo atrás com a gaiola e a rede vermelha, o prato redondo por cima, os canos finos pra
  frente.
- **Assinatura:** o prato redondo + o rosto de canos e tubos.
- **Cores:** o aço frio azulado com as manchas grandes pretas e cinza-claras; a cor do jogador na rede da gaiola, na lona dobrada e
  no rolo da cabine.
- **Nunca:** os 2 canos grossos juntos de frente com o prato no mastro (é o Linebacker da USA); o tambor rotativo (China); a caixa de
  mísseis deitada (é o Tomahawk); Z ou V; a faixa branca.

## 7. Tiro, projétil, impacto, destruição

- **A boca:** o canhão: os clarões curtos alternando entre os canos (o tiro de arma leve: sem sopro de canhão de tanque, sem recuo
  da torre), as cápsulas caindo do lado; o míssil: o "pop" do tubo a 60°, a nuvem branca curta e o motor acendendo.
- **O projétil:** o traçante curto subindo em rajada; o míssil fino e muito rápido com o rastro branco, subindo e virando forte pro
  alvo.
- **O impacto no ar:** o míssil estoura ao lado da aeronave (a área de 1,3/2,6 m: o estilhaço), a fumaça preta do motor; o canhão, as
  faíscas no casco e o helicóptero balançando.
- **O impacto no chão:** no soldado, a linha de poeira; no tanque, a faísca, quase nada (10%).
- **A morte (no jogo):** a morte de veículo; a carcaça fica 48 s queimando e afunda em 7 s; sucata ($150 em ~10 s) pro construtor.
- **A morte (ideia):** os mísseis dos blocos cozinham e um sai torto assobiando (sem dano); o prato voa pro lado; a cabine fica.

## 8. O som

- **Seleção (variantes):** o motor do radar (o "uuun" ritmado a cada volta); o bip de busca, agudo e repetido; o servo do módulo; o
  diesel do caminhão.
- **As falas** (inglês, com o sotaque russo leve; o comandante da bateria móvel, voz calma e numérica):
  - selecionado: "Pantsir ready." / "Radar is up." / "Sky under control." / "Searching." / "Air defense, on the move."
  - andando: "Moving with the column." / "Keeping the umbrella up." / "Repositioning." / "Following the armor."
  - atacando: "Target in the air!" / "Launch!" / "Guns, guns!" / "Target destroyed."
  - sob fogo: "Ground fire, I need armor!" / "Taking hits, still tracking!" / "Get them off me!"
  - pronto: "Pantsir-S1 ready."
- **O corpo:** o diesel do caminhão de 8 rodas; os pneus grandes; o zumbido agudo do gerador; o radar girando (o pulso a cada 1,5 s).
- **A arma:** os canhões duplos de 30 mm: o rasgo grave e muito rápido de cada rajada curta, alternado no estéreo; o míssil: o "FUMP"
  do tubo, o "SHHHH" subindo rápido e o estouro seco lá em cima.
- **A morte:** o estouro, a munição pipocando, o míssil solto assobiando, o metal do prato caindo.
- **A cara da Rússia:** o metal grave, o radar, a voz baixa e seca.

## 9. Ideias pra depois

- **O míssil contra o míssil:** o Pantsir derruba o Tomahawk em voo (o de verdade faz defesa contra míssil de cruzeiro): cada míssil
  dele contra um Tomahawk que passa a 110 m, com 50% de acerto e a salva de 4 gasta.
- **O radar que avisa:** o Pantsir-S1 revela as aeronaves inimigas a ~150 m (o radar vê antes do olho) e passa o alvo pro Pantsir fixo
  perto (a mira assistida que a Patriot já tem).
- **O enxame:** quando entrar o drone (o FPV da GLA), o canhão é o contra natural; o radar dá prioridade ao drone mais perto do tanque
  aliado.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#russia_pantsir` (feito no fim, com o modelo provisório como anexo de forma).
