# KamAZ (Rússia, o caminhão pesado)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o coletor: leva as caixas do depósito ao Supply Depot |
| Nome na tela | KamAZ (a chave é `russia_supply_truck`) |
| Custo e tempo | $700, 10 s (o 1º vem de graça com o Supply Depot) |
| Vida e blindagem | **350**, tanque (o caminhão mais duro do jogo: o da USA e o da China têm 300, a picape da GLA 200) |
| Velocidade | **8,9 m/s** (0,85 do caminhão da China), giro 90°/s, acelera e freia forte (63 m/s²) |
| Visão | 39,5 m |
| Carga | **5 caixas** de $75 = $375 por viagem (a USA e a China levam 4, a GLA 3) |
| Arma | nenhuma |
| Esmagador | nível 1: o soldado na frente dele morre |
| Requer | Supply Depot |
| Estado | nos dados (`Units.json`) e no modelo provisório; fora do menu até os modelos da Rússia serem importados |

## 2. A ideia

**A cabine alta e a carroceria aberta com os arcos.** O caminhão militar 6×6 pesado: a cabine quadrada e alta na frente, o estepe em
pé e o snorkel atrás dela, a carroceria comprida com os 3 arcos da lona e a lona da cor do jogador enrolada (aberta: a carga aparece
de cima). Carrega mais e anda mais devagar que todos.

- **A palavra de cima:** a CABINE ALTA + a carroceria comprida com os arcos e as 5 caixas (2 + 2 + 1).
- **Inspiração real:** o KamAZ-5350 (o 6×6 do exército russo), fictício, não cópia; na escala dos caminhões do jogo (~0,75: 6,0 ×
  2,4 m), maior que o da China (5,4 m) porque leva 5 caixas.
- **Por que existe:** a economia da Rússia é pesada e lenta como ela: menos viagens, mais caixa por viagem, mais vida pro raide.

## 3. Como funciona

- **No jogo:** carrega 5 caixas no depósito (1 s cada) e descarrega no Supply Depot (0,4 s cada): $375 por viagem.
- **A conta:** com o depósito a ~60 m, ida e volta a 8,9 m/s (~13,5 s) + 5 s carregando + 2 s descarregando: **~$1 100 por minuto**,
  ~4% mais que o caminhão da China na mesma rota, por $100 a mais (o comentário do código, contando só a carga e a velocidade, dá
  +6% por +17% do preço).
- **A carga que cai (no jogo):** se ele morre carregado, as caixas ficam no chão numa pilha pequena que **qualquer** caminhão pega,
  o dele ou o do inimigo.
- **Esmaga:** nível 1, o soldado que fica no caminho morre (200%).
- **Forte contra:** nada; é economia (e o soldado distraído na estrada).
- **Fraco contra:** raide: a Technical (12,5/s no tanque) mata em ~28 s; o RPG em ~8 s; 3 Rocket Buggies numa salva só.
- **O contra dele:** a Technical e o Rocket Buggy. **Ele é o contra de:** a pobreza.

## 4. Em batalha

- **Temperamento: o motorista de comboio.** Calmo, resistente, teimoso. Não corre, mas chega.
- **O ritmo:** a ida longa, 5 baques de caixa, a volta, 5 baques. O jogador sente a economia mais lenta e mais pesada no som.
- **O jogador bom** escolhe o depósito mais perto, põe o Pantsir fixo cobrindo a rota contra o Apache e mantém 2 ou 3 KamAZ por
  depósito; quando a pilha de um caminhão morto cai no meio do mapa, manda o próprio buscar antes do inimigo.
- **A IA** mantém os caminhões (o dinheiro do que falta fica guardado).
- **Sozinho e ferido:** segue a rota; abaixo de 70%, a fumaça clara; abaixo de 35%, a chama.

## 5. A vida

- **No jogo:** as 5 caixas aparecem e somem da carroceria conforme a carga (`cargo.slots_m`, 2 + 2 + 1; na folha dos modelos a
  carroceria aparece vazia: as caixas são do jogo); as 6 rodas giram cada uma pelo centro; a poeira e a marca dos pneus largos
  (0,36 m); os estados de dano do veículo.
- **Ideia (parado):** o diesel em marcha lenta, o fio de fumaça do escape; a lona enrolada da lateral balança no vento.
- **Ideia (carregando):** a carroceria abaixa um pouco a cada caixa (a mola sente o peso); a poeira sobe quando ele arranca cheio.
- **Ideia (descarregando):** a ponte rolante amarela do depósito corre até ele e levanta as caixas; a carroceria sobe leve.

## 6. O visual

- **De cima:** o retângulo de 6,0 × 2,4 m, a cabine quadrada na frente com a lona vermelha dobrada no teto, a carroceria comprida
  aberta com os 3 arcos, o rolo vermelho na frente e ao longo da lateral, as caixas no piso.
- **Assinatura:** a cabine alta + os arcos (a carga se lê de cima).
- **Cores:** o aço frio azulado com as manchas grandes pretas e cinza-claras; a cor do jogador na lona enrolada da frente da
  carroceria, no rolo da lateral e na lona dobrada no teto da cabine.
- **Nunca:** arma; a lona fechada cobrindo a carga (a carga tem de aparecer de cima); Z ou V; a faixa branca.

## 7. Tiro, projétil, impacto, destruição

- **Impacto (ideia):** a bala fura a lona e a lataria (faísca e "tunc"); o tiro na carroceria faz uma caixa pular e cair (só
  desenho).
- **A morte (no jogo):** a morte de veículo; a carcaça fica 48 s e afunda em 7 s; a pilha da carga que cai fica no chão; a carcaça
  é sucata ($105 em ~8 s pro construtor).
- **A morte (ideia):** o tanque de diesel estoura embaixo da cabine (a bola suja e a fumaça preta), a cabine pega fogo; as caixas
  voam da carroceria e caem em volta, formando a pilha (o jogador vê a carga cair); os arcos ficam de pé, pretos.

## 8. O som

- **Seleção (variantes):** o diesel de caminhão dando a partida e roncando grave; a buzina grave de uma nota; o freio a ar
  ("pssh"); a porta de aço da cabine batendo.
- **As falas** (inglês, com o sotaque russo leve; o motorista de comboio, voz grave, tranquila, de estrada longa):
  - selecionado: "KamAZ." / "Ready to haul." / "Five crates, no problem." / "Long road, heavy load." / "Where to, Commander?"
  - andando: "On the road." / "Driving." / "Slow and sure." / "Heading out."
  - carregando: "Loading." / "Full load." / "Heading back."
  - sob fogo: "They are shooting at the truck!" / "I need an escort!" / "I am unarmed!"
  - pronto: "KamAZ ready."
- **O corpo:** o diesel V-8 turbo (mais grave e mais lento que o caminhão da China, roncando nas trocas de marcha); os pneus grandes
  na areia; a carroceria e os arcos batendo nos buracos; o bip de ré na doca.
- **A carga:** cada caixa de madeira entrando na carroceria é um baque seco (1 por segundo, o ritmo do jogo); na doca, 5 baques
  rápidos e o "clang" da tampa (nada de moeda tilintando: o tom é sério).
- **A morte:** o estouro do diesel, os pneus queimando, as caixas despencando.
- **A cara da Rússia:** o diesel grave, o metal pesado, a voz baixa e seca.

## 9. Ideias pra depois

- **O reboque:** um upgrade no depósito ($600): o KamAZ puxa um reboque com mais 2 caixas ($525 por viagem) e anda a 80%; de cima, o
  comboio comprido. A Rússia aposta no peso.
- **O trem** (estudo `docs/FACCAO_RUSSIA.md` §5.4): nos mapas com trilho, o trem de suprimento substitui o caminhão na rota longa.
- **A cabine blindada:** a cabine com as chapas soldadas e a gaiola por cima (como os blindados), +100 de vida; de cima, a cabine
  ganha a grade e a rede da cor do jogador (a assinatura chega na economia).

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#russia_supply_truck` (feito no fim, com o modelo provisório como anexo de forma).
