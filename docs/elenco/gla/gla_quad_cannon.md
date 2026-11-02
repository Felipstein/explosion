# Quad Cannon (GLA, os quatro canos)

## 1. Ficha rápida

| | |
|---|---|
| Papel | antiaéreo e anti-infantaria da GLA |
| Custo e tempo | $700, 6 s |
| Vida e blindagem | 300, tanque |
| Velocidade | 10,5 m/s, giro 180°/s, o reparo gira 240°/s |
| Visão | 52,7 m |
| Arma (chão) | 4 metralhadoras pesadas: 10 de arma leve a cada 0,1 s, alcance 39,5 m |
| Arma (ar) | 5 de arma leve a cada 0,1 s, alcance 92,2 m |
| Esmagador | nível 1 |
| Requer | Arms Dealer |

## 2. A ideia

**Os 4 canos levantados.** Um caminhão 4×4 de capô dos anos 50 com um reparo quádruplo de metralhadoras pesadas soldado na
carroceria, a muralha de sacos de areia em U em volta, o escudo de chapas e a bandeira que gira com ele. A GLA não tem avião; ela
enche o céu de chumbo.

- **A palavra de cima:** os QUATRO CANOS lado a lado, levantados a 22° (a pose que lê "antiaéreo").
- **Inspiração real:** o ZPU-4 soviético (4 metralhadoras KPV de 14,5 mm num reparo rebocado; no real, os canos ficam 2 em cima de
  2; aqui lado a lado pra ler de cima, como a folha da paleta amarela aprovada) montado num caminhão soviético de capô dos anos 50 (o 4×4
  GAZ-63 é o mais perto; o ZIL-157 da mesma época é 6×6), como as guerrilhas fazem.
- **Por que existe:** a GLA precisa de uma resposta móvel ao helicóptero e à infantaria em massa que acompanhe os tanques.

## 3. Como funciona

- **No jogo, no chão:** 10 a cada 0,1 s = **100/s em soldado**; **25/s em tanque** (arma leve 25%), **50/s em prédio** (50%). Mata
  um Red Guard (120) em ~1,2 s; 4 Rebels em ~5 s; uma Technical (180) em ~7 s.
- **No jogo, no ar:** 5 a cada 0,1 s × 120% (a arma leve faz 120% em avião e helicóptero) = **60/s**: o Raptor (160) cai em ~2,7 s, o
  Apache (220) em ~3,7 s; o Chinook (50%) leva 25/s.
- **Atenção (a conferir):** no `Units.json` a arma dela é **arma leve** (`small_arms`), não `gattling` como a do Gattling Tank. Por
  isso ela faz 25/s no tanque (quase o canhão de um tanque, 30/s) e 50/s no prédio, 6,7× o Gattling Tank no tanque. No ZH a
  metralhadora dos veículos da GLA era de outro tipo (a nota do `_source` da Technical diz "Gattling no ZH"). Decidir: ou fica assim
  (a Quad vira também caça-veículo) ou passa pra `gattling` (10%: 10/s no tanque), como pede o motivo do `docs/ELENCO.md` §7.1.
- **Forte contra:** infantaria (a melhor do jogo), aeronave (a melhor do jogo), veículo leve.
- **Fraca contra:** tanque com canhão (o tanque USA a mata em 8 s), artilharia, o RPG e o Tank Hunter (8 foguetes).
- **O contra dela:** o tanque, a artilharia. **Ela é o contra de:** o Red Guard, o Ranger, o Apache, o Raptor, a Technical inimiga.

## 4. Em batalha

- **Temperamento: o fanático barulhento.** Exagerado, explosivo, eufórico. Atira em tudo, o tempo todo, e grita junto.
- **O ritmo:** a rajada longa e grave das 4 metralhadoras (um "DUDUDUDUDU" contínuo e pesado); quando acerta o helicóptero, o grito.
- **O jogador bom** põe 1 Quad pra cada 3 Scorpions, no meio do bando; contra a horda de infantaria da China, 2 Quads na frente
  apagam os pares de Red Guard.
- **A IA** a põe na mistura da fábrica pelo que falta (ela atira no chão também; não é o "antiaéreo puro" da regra da IA).
- **Sozinha e ferida:** segue atirando pro céu até cair.

## 5. A vida

- **No jogo:** o reparo inteiro gira 240°/s pro alvo (peça `_Turret`, com a bandeira do escudo girando junto); o traçante e o clarão
  (`light_gun`: 0,4 m, risco de 3,9 m, espalhado de 3 m); as 4 rodas, a poeira, a marca; a morte de gasolina (escala 0,5).
- **Ideia (parado):** os canos a 22° varrem o céu devagar; o motor velho tremendo; as bandeirinhas do para-choque.
- **Ideia (atirando):** os 4 canos recuam alternados (a "costura" dos 4); o caminhão balança inteiro; as cápsulas caindo em cascata
  pela carroceria e se acumulando no chão; a fumaça cinza que cobre o reparo depois de 3 s de fogo.
- **Ideia (no ar):** os canos sobem a 60°; os traçantes viram uma parede de linhas laranja subindo.

## 6. O visual

- **De cima:** o caminhão de ~6,4 × 2,3 m (79% do tanque USA), o capô estreito, a cabine, a carroceria de tábua com a muralha de sacos
  em U, o reparo com os 4 canos lado a lado.
- **Assinatura:** os 4 canos.
- **Cores:** o amarelo-areia fosco com as listras verde-escuras gastas, a porta e o painel de ferrugem, a carroceria de madeira; o
  verde do jogador nas 2 bandeirinhas do para-choque, na bandeira do escudo, nos rabiscos e no pano amarrado num cano.
- **Nunca:** torre de tanque; pintura limpa; radar moderno (a GLA mira no olho, na alça de anel).

## 7. Tiro, projétil, impacto, destruição

- **A boca:** 4 clarões alternados, grandes e piscando, a fumaça cinza grossa que se acumula (volume, acesa pelos clarões).
- **O projétil:** os traçantes laranja em feixe largo, 4 linhas tremendo (o caminhão balança).
- **O impacto:**
  - no soldado: a área inteira em volta dele levanta poeira; ele é apagado;
  - no veículo: as faíscas e os furos, a lataria rasgando;
  - no ar: as faíscas em volta da aeronave, a fumaça, as peças soltando;
  - no prédio: o reboco saindo em placas.
- **A morte (no jogo):** a morte de gasolina (escala 0,5); a carcaça 48 s; sucata ($105 em ~8 s).
- **A morte (ideia):** a munição das caixas de fora estoura em sequência (os traçantes saindo pra todo lado por 2 s); o reparo cai da
  carroceria.

## 8. O som

- **Seleção (variantes):** a manivela do reparo girando (a catraca); o engatilhar das 4 metralhadoras em sequência (clack-clack-clack-
  clack); o motor velho a gasolina acelerando; o grito curto do atirador.
- **As falas** (inglês; o atirador, voz alta, eufórica, sem fôlego, rindo no meio da rajada):
  - selecionada: "Quad cannon!" / "Four barrels, no mercy!" / "Show me the sky!" / "Ready to shred!" / "Let them fly!"
  - andando: "Moving the guns!" / "Rolling!" / "Keep up, keep up!" / "To the front!"
  - atacando: "Fill the sky!" / "All four, fire!" / "Shred them!" / "Bird going down!"
  - sob fogo: "Tank! Tank!" / "We can't take that!" / "Get us out of here!"
  - pronta: "Quad cannon, ready."
- **O corpo:** o motor a gasolina velho do caminhão (rouco, com o escape furado); a carroceria de madeira rangendo; os sacos de areia.
- **A arma:** as 4 metralhadoras pesadas (o rosnado grave e rápido, mais grave e "socado" que o rotativo da China); as cápsulas
  pesadas caindo em cascata; a manivela de giro.
- **A morte:** a gasolina estourando, a munição pipocando, o metal do reparo caindo.

## 9. Ideias pra depois

- **A munição de sucata:** upgrade ($500): a Quad carrega munição incendiária feita de restos; 20% do dano vira fogo (150% no soldado).
- **A parede de chumbo:** uma habilidade de 4 s: os 4 canos em cortina pro alto; toda aeronave que passa a até 30 m perde a mira (o
  piloto se assusta) e o míssil que passa tem chance de cair. Recarga de 45 s.
- **A Quad na ruína:** parada em cima do entulho de um prédio, ganha +20% de alcance no ar (a altura do monte), a GLA usando a cidade
  destruída como torre.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#gla_quad_cannon` (feito no fim, com o modelo provisório como anexo de forma).
