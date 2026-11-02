# Gattling Cannon (China, a defesa de canos rotativos)

## 1. Ficha rápida

| | |
|---|---|
| Papel | a defesa de base: anti-infantaria e antiaérea |
| Custo e tempo | $1 200, 25 s |
| Vida e blindagem | 1 000, prédio |
| Energia | −3 (e **para sem energia**: `needs_power`) |
| Visão | 94,8 m |
| Lote | 8 × 8 m (a menor peça da base) |
| Arma (chão) | gattling 10 a cada 0,25 s, alcance 59,2 m |
| Arma (ar) | gattling 5 a cada 0,25 s, alcance 105 m |
| Torre | gira 180°/s; volta pra frente 2 s depois de parar |
| Requer | Nuclear Reactor |
| Conta pra vitória | não |

## 2. A ideia

**A metralhadora de 6 canos numa torre baixa blindada sobre o bunker redondo.** Uma serra de chumbo: quem vem a pé pela frente
dela não chega, e o helicóptero que passa por cima volta com buracos.

- **A palavra de cima:** o CÍRCULO do bunker com a torre em ponta de escudo e o feixe de canos pra fora (a assinatura redonda
  num mundo de retângulos).
- **Inspiração real:** os canhões rotativos navais chineses (o Type 730, de 7 canos de 30 mm, e o Type 1130, de 11 canos) levados
  pra terra, num tambor de 6 canos; os bunkers redondos de concreto com a terra empurrada contra a parede.
- **Por que existe:** a China não tem o míssil antiaéreo da USA; a sua resposta ao ar e à infantaria é o volume de fogo.

## 3. Como funciona

- **No jogo, no chão:** 40 por segundo em soldado (gattling 100%), **4/s em tanque e em prédio** (gattling 10%). Mata um Red Guard
  ou um Rebel (120) em 3 s, um Ranger (180) em 4,5 s. Alcança 59 m: mais longe que o tanque (39,5 m) e o soldado (26 m).
- **No jogo, no ar:** 20/s (avião e helicóptero 100%, o Chinook 50%): o Raptor (160) cai em 8 s, o Apache (220) em 11 s, o Chinook
  (300) em 30 s. Alcança 105 m.
- **Sem energia:** para de atirar (o reator caiu = a defesa morreu junto). É a fraqueza dela.
- **Forte contra:** infantaria em grupo, helicóptero, avião que circula.
- **Fraco contra:** tanque (um tanque USA a derruba em 33 s e leva 120 s pra morrer dela), artilharia (Tomahawk a 85 m, Rocket Buggy
  e Inferno a 79 m), perder a energia.
- **O contra dela:** o tanque e a artilharia; o raide no reator. **Ela é o contra de:** Rangers, Rebels, Red Guards, o Apache e o
  Raptor que vem sozinho.

## 4. Em batalha

- **Temperamento: o cão de guarda.** Paciente até o primeiro passo no alcance; aí não para. Nervosa, contínua, implacável.
- **O ritmo:** silêncio, o giro dos canos acelerando (o "uiiin"), e o BRRRRT contínuo; para, os canos ainda rodando no vazio.
- **O jogador bom** põe duas cruzando fogo na entrada da base, perto (não colada) do reator: longe do raio de 26 m dele, mas
  dentro da rede elétrica que ele protege.
- **A IA** a constrói na lista da base (depois da fábrica), quando tem o reator.
- **Sozinha e ferida:** atira até o fim; abaixo de 35%, fogo no teto do bunker.

## 5. A vida

- **No jogo:** a torre gira 180°/s pro alvo e volta pra frente 2 s depois; o traçante e o clarão de boca pequeno (`light_gun`: o
  clarão de 0,4 m, o risco de 3,5 m, o espalhado de 2 m); o andaime na obra; abaixo de 35%, o fogo e os buracos.
- **Ideia (parado):** a torre faz uma varredura curta de 20° a cada ~10 s; a lente do sensor eletro-óptico brilha um reflexo; a
  antena treme.
- **Ideia (atirando):** o tambor de 6 canos gira (a peça `_Barrels` já está no script, ainda sem o giro nos dados); a chuva de
  cápsulas cai pela calha e se acumula no chão em volta (some depois de ~30 s); a boca fica vermelha depois de 5 s de fogo
  contínuo e esfria devagar.
- **Ideia (sem energia):** o tambor para, a lente apaga, a torre volta pra frente e baixa os canos: de cima se lê "desligada".

## 6. O visual

- **De cima:** o círculo do bunker com a saia de terra, a torre facetada em ponta de escudo, o feixe de canos saindo 4 m do pivô.
- **Assinatura:** o tambor de 6 canos.
- **Cores:** concreto escuro oliva no bunker, o oliva dos tanques na torre; o vermelho do jogador na lona dobrada do cesto de trás
  e na estrela da caixa de munição e do bunker.
- **Nunca:** cano duplo (é do Overlord); faixa pintada; cruz vermelha.

## 7. Tiro, projétil, impacto, destruição

- **A boca:** o clarão curto e contínuo, amarelo-branco, em estrela de 6 pontas (o tambor); a fumaça cinza fina que se acumula em
  volta da torre quando ela atira muito (volume, acesa pelo clarão).
- **O projétil:** o traçante vermelho-alaranjado a cada 3 a 4 tiros, rápido (instantâneo no jogo), em leque estreito; contra o ar,
  a linha de traçantes subindo em arco.
- **O impacto:**
  - no soldado: a poeira levantada em linha andando até ele (a rajada "caminha" no chão);
  - no tanque: faísca em cascata, quase sem dano (10%), o "ping-ping-ping";
  - no prédio: lasquinhas de reboco, pouco;
  - no ar: as faíscas no casco do helicóptero e a fumaça saindo do motor.
- **A morte (no jogo):** a morte de prédio (a grande no meio, 3 menores, o chão queimado, o entulho).
- **A morte (ideia):** a munição da caixa cozinha: uma série de estalos e traçantes saindo pra todo lado por ~2 s; a torre tomba do
  anel.

## 8. O som

- **Seleção (variantes):** o motor elétrico do tambor dando um giro curto no vazio ("uiiin-uuun"); o ferrolho/a culatra batendo; a
  torre girando com o rangido do anel; o alto-falante "Perimeter secure."
- **As falas** (inglês; o operador, voz seca, concentrada, curta):
  - selecionado: "Gattling ready." / "Perimeter covered." / "Barrels spinning." / "Let them come." / "Watching the sky."
  - atacando: "Engaging!" / "Cut them down." / "Target in the air!" / "Sweeping."
  - sob fogo: "Bunker taking hits!" / "Still firing!"
  - sem energia: "No power! Guns are down!"
  - pronta: "Gattling cannon online."
- **O corpo:** o zumbido do motor elétrico em espera; o ventilador do bunker.
- **A arma:** o "uiiin" do tambor acelerando (~0,3 s) e o BRRRRT contínuo (o som de rotativo é um rasgo, não tiros separados: o
  dano a cada 0,25 s é a conta do jogo, o som é contínuo); o "tilintar" das cápsulas caindo no concreto; ao parar, o tambor
  desacelerando no vazio.
- **A construção:** a escavadeira cavando o buraco, o concreto despejado, a torre assentada com o "clank"; no fim, o tambor testado
  com um giro curto.
- **A morte:** os estouros; a munição cozinhando (o pipocar de estalos secos); a torre caindo no concreto.

## 9. Ideias pra depois

- **O aquecimento:** o tambor esquenta: depois de 6 s de fogo contínuo a cadência cai 30% até esfriar 3 s. O jogador bom faz a
  defesa "respirar" escolhendo alvo; o atacante esperto manda isca.
- **A munição incendiária:** upgrade no Propaganda Center ($1 000): o tiro no chão vira fogo (150% no soldado), com o traçante
  mais alaranjado.
- **O radar da base:** a Gattling Cannon vê aeronave invisível a até 30 m (a resposta da China ao avião furtivo que vier no futuro).

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#china_gattling_cannon` (feito no fim, com o modelo provisório como anexo de forma).
