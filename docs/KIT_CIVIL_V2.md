# Kit civil v2: carros, casas, ruas e miudezas (a lista viva)

O pedido (01/11/2026): "coloca grandes recursos de civilidade, carrinhos, dos gameshots das cidades, prédios das cidades, texturas das
estradas, ruas, detalhes e objetos dos mapas [...] nada de jogo de lego, tudo bem detalhado, nada de cantos retos feios".

Este documento é o levantamento (o que os game shots mostram de civil, por peso na leitura de cima) e a lista viva (feito, falta,
triângulos, de onde veio cada peça). Quem mexer numa peça do kit marca aqui.

- **Scripts:** `Game/Tools/model_civ_<tipo>.py`; as bibliotecas `model_civ_parts.py` (os carros e as cores de conferência) e
  `model_civ_house_parts.py` (as casas e as miudezas).
- **Saída:** `captures/game/models/civ_v2/<tipo>/` (os GLB, `preview/` e `preview_burnt/` com as prévias em Cycles na CPU).
- **Rodar:** `blender -b --factory-startup --python Game/Tools/model_run_detail.py -- <vehicle|building> Game/Tools/model_civ_<tipo>.py <saída absoluta> [burnt]`.
  As prévias: `PREVIEW_MASKS=game blender -b --factory-startup --python Game/Tools/model_preview.py -- <saída> <saída>/preview civ_<tipo> <repo>/captures/look/tank_glb`
  (o `preview.json` da pasta; `PREVIEW_SPEC=preview_burnt.json` pra carcaça).
- **O método:** o do elenco (`docs/ESTUDO_DETALHE_AAA.md`): a forma no perfil (seções, anéis, perfil girado, varredura), nunca caixa
  crua nem remesh; o executor do detalhe passa a grade, o UV e as máscaras (quina, contato, fresta) na cor de vértice.

## 1. O levantamento: o que os game shots mostram

As fontes: os recortes de `docs/concept/towns/` (149 casas, 77 partes, 25 cidades, `_variacoes_novas.jpg`), os game shots
`docs/concept/scenes/desert_golden_hour/best/` (`road_town_battle_gemini`, `town_battle_top_gemini`, `street_battle_usa_china_gemini`),
`captures/concept/16_beauty_pass/b_gpt.webp` (a vila mais detalhada de todas), `captures/concept/15_three_factions/b_gpt.webp`,
`captures/concept/20_beauty_new_tanks/usa_china_gpt.webp` e `china_gla_gpt.webp`, `docs/concept/scenes/city_*/best/` e o alvo
`docs/aprovados/alvos/2026-10-25_cara_do_jogo_ataque_cirurgico_gemini.jpg`. O `r1_raid_slabs` (o alvo principal) é a base no deserto
aberto: não tem civil.

A ordem é pelo que **mais aparece e mais pesa de cima** (na câmera do jogo, 1 px = 3–5 cm: o que tem menos de ~10 cm vira tom).

### 1.1 Casas e prédios

| # | O quê | Onde aparece (a imagem de origem) | O que lê de cima |
|---|---|---|---|
| 1 | **Casa térrea de teto plano** (o bloco rebocado com o parapeito, a capa e os dentes, os pilares de quina) | ~60% dos 149 recortes de `towns/casas` (as fileiras 2–5 da `_folha_casas.jpg`); `16_beauty_pass/a_gemini.webp`; `road_town_battle_gemini` (o fundo) | o quadrado claro do teto com a borda de sombra do parapeito |
| 2 | **O teto carregado**: a caixa d'água de plástico (em pé, no cavalete), a de aço deitada, a parabólica, o varal, o tapete no parapeito, a casinha da escada, a condensadora, a antena, o ferro de espera | `16_beauty_pass/b_gpt.webp` (embaixo: 8 caixas d'água), `15_three_factions/b_gpt.webp` (a parabólica e a caixa deitada), `_variacoes_novas.jpg` fileiras 1 e 6 | os círculos escuros das caixas, o disco claro da parabólica, as manchas de cor do tapete e da roupa |
| 3 | **Sobrado de 2 andares** (a cinta, a sacada com o guarda-corpo, as janelas dos 2 andares) | `16_beauty_pass/b_gpt.webp` (a esquerda, com o tapete e a escada), `15_three_factions/b_gpt.webp` (embaixo) | o bloco mais alto com a sombra comprida |
| 4 | **Casa em ruína** (o canto caído, a laje partida, o ferro, o monte de entulho) | `_variacoes_novas.jpg` fileira 3; `16_beauty_pass/b_gpt.webp` (em cima à direita); `20_beauty_new_tanks/usa_china_gpt.webp` (à direita); o alvo "a cara do jogo" | a casa aberta com o escuro de dentro e o leque claro do entulho |
| 5 | **Muro de quintal** com os pilaretes e o **portão de ferro** | todas as `towns/partes`; `16_beauty_pass/b_gpt.webp` (o portão de chapa cinza) | as linhas finas que desenham os lotes |
| 6 | **Loja com toldo de pano e portas de enrolar** (o mercado) | `_variacoes_novas.jpg` fileira 6; `16_beauty_pass/b_gpt.webp` (o toldo cor de terra) | o retângulo de pano colorido na frente |
| 7 | Casa com varanda de chapa | `docs/concept/buildings/civil/civ_house_porch_*.webp` (as fichas aprovadas em 08/10); `towns/casas` | a chapa ondulada e a sombra da varanda |
| 8 | Casa de pátio com palmeira | `_variacoes_novas.jpg` fileira 2 | (o gerador monta: a casa + os muros + a palmeira do WorldForge) |
| 9 | Casa fortificada (sacos de areia no teto) | `_variacoes_novas.jpg` fileira 4 | (a peça de sacos no teto) |
| 10 | Mesquita com minarete (o marco do mapa) | `_variacoes_novas.jpg` fileira 5; `towns/casas/01_keyframe_gemini_05.jpg` | a cúpula e a torre |
| 11 | Prédios de cidade (3–6 andares, lojas com toldos, vidro) | `city_modern_noon_gemini.webp`, `city_old_europe_gemini.webp`, `city_port_dusk_gemini.webp`, `city_neon_night_gemini.webp` | (outro bioma) |

### 1.2 Carros

| # | O quê | Onde aparece | Observação |
|---|---|---|---|
| 1 | **Picape de cabine simples** dos anos 90 (a Hilux) | `road_town_battle_gemini` (6, branca, vermelha desbotada, oliva), `town_battle_top_gemini` (8+), `16_beauty_pass/b_gpt.webp` (as technicals) | o veículo civil mais visto de longe |
| 2 | **Carcaças queimadas** (sem vidro, sentadas nos aros, ferrugem e fuligem) | `street_battle_usa_china_gemini` (2), `16_beauty_pass/b_gpt.webp` (2, no meio), `15_three_factions/b_gpt.webp` (2), `20_beauty_new_tanks/usa_china_gpt.webp` (1) | a segunda coisa civil mais vista nas cenas de batalha |
| 3 | **Sedã de 3 volumes** dos anos 90 | `16_beauty_pass/b_gpt.webp` (o prata parado), `city_modern_noon_gemini.webp` (muitos) | |
| 4 | **Van de passageiros** (o "micro" de teto alto) | `city_modern_noon_gemini.webp` (as brancas) | o bagageiro carregado é a assinatura da região |
| 5 | **Caminhão leve de carroceria** | (pedido; não há um nítido nos game shots) | o de entrega das feiras e das obras da região |
| 6 | Hatch | as cidades | |
| 7 | Ônibus | `city_modern_noon_gemini.webp` | (cidade) |
| 8 | Carro capotado | `road_town_battle_gemini` (os 2 virados pela explosão) | o jogo pode girar a carcaça (é a mesma malha de cabeça pra baixo) |

### 1.3 Rua, quintal e telhado

| # | O quê | Onde aparece |
|---|---|---|
| 1 | Caixa d'água de plástico (em pé, na laje ou no cavalete) | `16_beauty_pass/b_gpt.webp`, `15_three_factions/b_gpt.webp`, `20_beauty_new_tanks/china_gla_gpt.webp` |
| 2 | Poste de madeira com cruzeta e fios | `road_town_battle_gemini`, `20_beauty_new_tanks/usa_china_gpt.webp`, `16_beauty_pass/b_gpt.webp` |
| 3 | Tambores de aço e bombonas de plástico | `16_beauty_pass/b_gpt.webp` (a esquina), `20_beauty_new_tanks/usa_china_gpt.webp` |
| 4 | Tapete (pendurado no parapeito, no chão do quintal) | `15_three_factions/b_gpt.webp` (3), `16_beauty_pass/b_gpt.webp` |
| 5 | Parabólica | `15_three_factions/b_gpt.webp`, `20_beauty_new_tanks/china_gla_gpt.webp` |
| 6 | Varal com roupa | `16_beauty_pass/b_gpt.webp` (à esquerda) |
| 7 | Toldo de pano | `16_beauty_pass/b_gpt.webp`, `_variacoes_novas.jpg` |
| 8 | Escada de mão e escada de fora | `16_beauty_pass/b_gpt.webp` (3), `15_three_factions/b_gpt.webp` |
| 9 | Caixotes e paletes | `16_beauty_pass/b_gpt.webp` |
| 10 | Tapume / barricada de chapa ondulada | `town_battle_top_gemini` (muitos) |
| 11 | Barreira e blocos de concreto | `street_battle_usa_china_gemini` |
| 12 | Sacos de areia | `_variacoes_novas.jpg` fileira 4, `20_beauty_new_tanks/china_gla_gpt.webp` |
| 13 | Entulho solto | todas as cenas de batalha |
| 14 | Ar-condicionado de parede e condensadora | `16_beauty_pass/b_gpt.webp` |
| 15 | Ferro de espera nos pilares | `15_three_factions/b_gpt.webp` |
| 16 | Pneus velhos | `20_beauty_new_tanks/china_gla_gpt.webp` |
| 17 | Poste de luz de rua | `town_battle_top_gemini` (embaixo), as cidades |
| 18 | Caçamba de lixo, banca de feira | as cidades, o mercado |
| 19 | Meio-fio, calçada, bueiro, faixa de pedestre | `city_modern_noon_gemini.webp`; a calçada quebrada da vila em `16_beauty_pass/b_gpt.webp` |

## 2. A lista viva

Status: **feito** = o script, o GLB com o detalhe (grade, UV, máscaras) e a prévia; nada disso foi pro jogo nem teve o OK do Felipe.
Triângulos = os do GLB exportado (depois do chanfro de 3 segmentos e da grade do executor). A pegada = a caixa medida no GLB
(X largura × Y comprimento × Z altura, a frente em +Y, a origem no centro do pé). **SUBSTITUI** = o mesmo nome de tipo do kit de hoje
(`WorldVillage.cpp`, a tabela `Kits`): o gerador troca sem mudar nada.

### 2.1 Carros (`vehicle`: chanfro de 4 cm, grade de 14 cm)

| GLB | Papel | Pegada medida (m) | Triângulos | Script | De onde veio |
|---|---|---|---|---|---|
| `SM_civ_sedan` | novo | 2,08 × 4,62 × 1,40 (antena 1,62) | 41 260 | `model_civ_sedan.py` | `16_beauty_pass/b_gpt` (o sedã prata), as cidades |
| `SM_civ_car_burnt` | **SUBSTITUI** `civ_car_burnt` (2,0 × 4,6 × 1,5) | 1,9 × 4,6 × 1,28 (os cacos no chão até ~2 m do centro) | 40 182 | `model_civ_sedan.py burnt` | `16_beauty_pass/b_gpt`, `street_battle_usa_china_gemini`, `15_three_factions/b_gpt` |
| `SM_civ_hatch` | novo | 1,78 × 4,11 × 1,60 (com a carga) | 45 940 | `model_civ_hatch.py` | as cidades; a carga de teto do `16_beauty_pass/b_gpt` |
| `SM_civ_hatch_burnt` | novo | | 41 113 | `model_civ_hatch.py burnt` | |
| `SM_civ_pickup` | **SUBSTITUI** `civ_pickup` (2,0 × 5,2 × 1,9) | 1,82 × 5,01 × 1,74 | 55 282 | `model_civ_pickup.py` | `road_town_battle_gemini` (as 6), `town_battle_top_gemini` |
| `SM_civ_pickup_burnt` | novo | (a tampa caída sai 0,55 m atrás) | 44 519 | `model_civ_pickup.py burnt` | |
| `SM_civ_van` | novo | 2,0 × 5,03 × 2,64 (com a carga) | 63 672 | `model_civ_van.py` | `city_modern_noon_gemini` (as vans brancas) |
| `SM_civ_van_burnt` | novo | | 50 329 | `model_civ_van.py burnt` | |
| `SM_civ_truck` | novo | 2,0 × 6,0 × 2,34 | 69 544 | `model_civ_truck.py` | o pedido (o caminhão pequeno); nenhum game shot mostra um nítido: o tipo é o da região (Isuzu Elf / Mitsubishi Canter) |
| `SM_civ_truck_burnt` | novo | | 45 745 | `model_civ_truck.py burnt` | |

**As variantes de cor** (`Game/Tools/civ_paint_variants.py`: a mesma malha, só o slot da lataria trocado no GLB):
`SM_civ_sedan_beige/_olive`, `SM_civ_car_burnt_beige`, `SM_civ_hatch_white/_olive`, `SM_civ_pickup_beige/_olive`, `SM_civ_van_beige`,
`SM_civ_truck_beige/_olive`. Branco = `Paint` (TankPaint), bege = `GlaPaint`, oliva = `Crate`.

**Como o carro é feito** (`model_civ_parts.py`, `Car` e `passenger_car`):
- a lataria de baixo é um loft de **seções** ao longo do X (o pé redondo, a lateral com a barriga e a parede que entra, o ombro
  redondo, o topo com a coroa), cortado pelo **arco** de cada roda; o nariz e a traseira redondos pelas estações das pontas;
- a estufa é um loft de **anéis em planta** (o retângulo de cantos redondos subindo e encolhendo: o para-brisa inclinado, as colunas,
  o teto com a coroa); os vidros são chapas que seguem a estufa, 5 mm pra fora, com a borracha escura em volta;
- o para-choque é **varrido** (o perfil arredondado dobrando nos cantos); os faróis e as lanternas embutidos no aro; a grade com as
  lâminas; as juntas de porta, capô e tampa (o friso de 9 mm que segue a lataria); as maçanetas, os retrovisores no pé, os
  limpadores, a antena, a tampa do tanque, a caixa de roda escura por dentro, o chassi;
- o pneu de passeio é o `tyre` do elenco sem os blocos (a banda lisa não lê a 3 cm/px) e com a **calota**; o de carga, com os blocos;
- a **carcaça** é o mesmo carro: sem vidro (a estufa vira o quadro: as colunas, as cintas e o teto **afundado**); as manchas
  recortadas pela isolinha do ruído (`loft_layers`: a borda corta a face, não segue a grade, nada de pixel) com a física do fogo: a
  **ferrugem em cima** (teto, capô, alto das portas: a tinta queimou toda), a **fuligem embaixo** e em manchas no alto (o fogo que
  saiu pelas janelas), o aço cinza oxidado em placas pequenas no teto, a tinta que sobrou só na saia; os bancos em armação e mola, o
  volante, o painel em carvão; sentada nos **aros** (0,11 m mais baixa, inclinada); o para-choque pendurado de um lado; os cacos no chão.
  (A 1ª versão pintava cada face de uma cor ao acaso: lia camuflagem em pixel. A 2ª, as manchas grandes ao acaso: ainda camuflagem.)

### 2.2 Casas e muros (`building`: chanfro de 12 cm na parede, grade de 40 cm)

| GLB | Papel | Pegada medida (m) | Triângulos | Script | De onde veio |
|---|---|---|---|---|---|
| `SM_civ_house_1story` | **SUBSTITUI** `civ_house_1story` (8 × 8, H 4,5) | corpo 7,4 × 7,4; caixa 8,4 × 8,45 × 5,7 (a laje a 3,35, o parapeito a 4,05, a casinha da escada a 5,7) | 82 582 | `model_civ_house_1story.py` | `towns/casas` (~60% delas), `16_beauty_pass/b_gpt` |
| `SM_civ_house_2story` | **SUBSTITUI** `civ_house_2story` (8 × 8, H 7) | corpo 7,4 × 7,4 × 7,05; a **sacada sai 1,05 m** na frente (Y até 4,77) | 126 424 | `model_civ_house_2story.py` | `16_beauty_pass/b_gpt`, `15_three_factions/b_gpt` |
| `SM_civ_house_long` | **SUBSTITUI** `civ_house_long` (16 × 8) | corpo 15,4 × 7,4; o **toldo com os paus sai 2,3 m** na frente (Y até 6,13) | 131 201 | `model_civ_house_long.py` | `_variacoes_novas.jpg` fileira 6, `16_beauty_pass/b_gpt` |
| `SM_civ_house_porch` | **SUBSTITUI** `civ_house_porch` (8 × 12) | 8,3 × 12,35 × 5,7 | 107 954 | `model_civ_house_porch.py` | as fichas `civ_house_porch_*` (aprovadas em 08/10), `towns/casas` |
| `SM_civ_house_ruin` | **NOVO** (8 × 8: o estado destruído da térrea) | 8,1 × 7,9 × 4,35 | 47 437 | `model_civ_house_ruin.py` | `_variacoes_novas.jpg` fileira 3, `20_beauty_new_tanks/usa_china_gpt`, `16_beauty_pass/b_gpt` |
| `SM_civ_wall` | **SUBSTITUI** `civ_wall` (4 × 0,5, H 2,3) | 4,04 × 0,5 × 2,66 (o pilarete na ponta −X) | 2 928 | `model_civ_wall.py` | a ficha `civ_wall_gate_gpt`, `towns/partes` |
| `SM_civ_wall_gate` | **SUBSTITUI** `civ_wall_gate` (4 × 0,5, H 2,6) | 4,0 × 0,6 × 2,71 | 8 050 | `model_civ_wall.py` | `16_beauty_pass/b_gpt` (o portão de chapa) |

**Como a casa é feita** (`model_civ_house_parts.py`):
- a parede é uma **malha contínua furada pelos vãos** (`wall_grid`): o chanfro de 12 cm só pega a quina de verdade (o batente, a
  ponta, o topo); as paredes param nos **pilares de canto** (`walls_with_posts`): a quina de fora é uma só, redonda. (As 2 primeiras
  tentativas abriam sulcos escuros: a parede em faixas arredondava a ponta de cada faixa, do chão ao teto do lado das janelas; as
  paredes que se cruzavam no canto abriam a fresta na quina.)
- o rodapé de reboco e a cinta entre os andares (a borda de 3 cm), a saia enterrada até −0,4 m (como o kit de hoje);
- a janela **recuada** na espessura, com a moldura de reboco, o peitoril, o caixilho, o vidro (`Glass`) e a grade de barras (as de
  baixo) ou as venezianas; a porta de ferro com as almofadas, a **marquise** e o degrau; a porta de enrolar da loja;
- os **remendos** de reboco e o bloco à mostra (`wall_patches`); na ruína, os **buracos de tiro** em rajada (`pockmarks`);
- o parapeito com a **capa**, os **dentes** (o ritmo de blocos em cima, o que lê de cima; com o chanfro de 9 cm viravam travesseiros,
  ficaram com 3 cm) e os pilares de quina mais altos com o **ferro de espera**;
- o teto: a casinha da escada, a **caixa d'água de plástico** com os frisos (na laje ou no cavalete), a de aço deitada, a
  **parabólica**, a condensadora, a antena de mastro, o **varal** com a roupa caída, o **tapete** jogado no parapeito, as bicas, a
  tralha (blocos, tijolos, balde, galão, a pilha de caixotes); a **escada de mão** encostada na parede até o teto (as 3 do
  `16_beauty_pass/b_gpt`); na varanda, os **pneus velhos** segurando a lona da cobertura;
- a ruína: as faces que caíram com o **topo serrilhado contínuo** (uma peça por trecho, a quina viva da quebra fresca, a lasca do
  miolo cru por cima), a laje partida na diagonal com o ferro saindo da quebra e a metade da frente caída em **2 pedaços dobrados**,
  o **monte de caliça** debaixo dos blocos (o entulho assenta num monte, não em caixas soltas), a caixa d'água tombada.

### 2.3 Miudezas de rua, quintal e telhado (`vehicle`: são peças pequenas)

| GLB | Papel | Pegada medida (m) | Triângulos | De onde veio |
|---|---|---|---|---|
| `SM_civ_barrels` | **SUBSTITUI** `civ_barrels` (2 × 2) | 1,65 × 1,83 × 0,94 | 5 560 | `16_beauty_pass/b_gpt` (a esquina) |
| `SM_civ_crates` | **SUBSTITUI** `civ_crates` (2,4 × 2,4) | 2,07 × 1,82 × 1,02 | 11 108 | `16_beauty_pass/b_gpt` |
| `SM_civ_sandbags` | **SUBSTITUI** `civ_sandbags` (4 × 1,6) | 3,67 × 0,86 × 0,65 | 12 220 | `_variacoes_novas.jpg` fileira 4 |
| `SM_civ_tires` | novo | 1,46 × 1,18 × 0,80 | 7 728 | `20_beauty_new_tanks/china_gla_gpt` |
| `SM_civ_water_tank` | novo (teto) | 1,32 × 1,14 × 1,38 | 2 528 | `16_beauty_pass/b_gpt` |
| `SM_civ_water_tank_stand` | novo (teto) | 1,44 × 1,44 × 2,57 | 5 056 | `16_beauty_pass/b_gpt`, `15_three_factions/b_gpt` |
| `SM_civ_tank_horizontal` | novo (teto) | 0,86 × 1,76 × 1,53 | 2 560 | `15_three_factions/b_gpt` |
| `SM_civ_dish` | novo (teto) | 0,92 × 0,64 × 1,09 | 662 | `15_three_factions/b_gpt` |
| `SM_civ_roof_ac` | novo (teto) | 0,86 × 0,56 × 0,66 | 1 412 | `16_beauty_pass/b_gpt` |
| `SM_civ_laundry` | novo (teto) | 2,6 × 0,16 × 1,65 | 1 608 | `16_beauty_pass/b_gpt` |
| `SM_civ_awning_wall` | novo (na parede, sai pra +Y, de 1,88 a 2,51 m) | 3,1 × 1,66 | 4 132 | `16_beauty_pass/b_gpt` |
| `SM_civ_fence_corr` | novo | 4,08 × 1,1 × 2,18 | 15 696 | `town_battle_top_gemini` (as barricadas de chapa) |
| `SM_civ_jersey` | novo | 3,0 × 0,62 × 0,81 | 2 810 | `street_battle_usa_china_gemini` |
| `SM_civ_dumpster` | novo | 2,75 × 1,82 × 1,44 (a tampa aberta de pé até 2,51) | 9 888 | as cidades |
| `SM_civ_streetlamp` | novo | 0,65 × 2,5 × 6,98 | 2 896 | `town_battle_top_gemini`, as cidades |
| `SM_civ_market_stall` | novo | 2,84 × 2,23 × 2,39 | 12 288 | `_variacoes_novas.jpg` fileira 6 (o mercado) |
| `SM_civ_rubble` | novo | 3,2 × 3,1 × 0,98 | 10 898 | todas as cenas de batalha |
| `SM_civ_pole` | **SUBSTITUI** `civ_pole` (0,4 × 0,4, H 8) | 2,26 × 0,56 × 8,0 | 6 302 | `road_town_battle_gemini`, `20_beauty_new_tanks/*` |
| `SM_civ_pole_transformer` | novo | 2,26 × 1,0 × 8,0 | 8 654 | `20_beauty_new_tanks/usa_china_gpt` |

Todos em `model_civ_props.py` (um GLB por peça; `-- <saída> nome1,nome2` refaz só alguns), menos os postes (`model_civ_pole.py`).
**O poste guarda as medidas do aprovado** (`look_houses.py pole()`, 17/10): a cruzeta ao longo de X a 7,25–7,40 m, os 4 isoladores
em x = ±0,35 e ±0,95 com o topo a 7,60 m, nenhuma ponta de fio modelada: o `RTSWireSystem` liga igual. Regra do gerador: **poste
nunca em cima de rua nem de outra peça** (vale pro poste de luz também).

### 2.4 Rua (`model_civ_street.py`, `vehicle`)

| GLB | Pegada medida (m) | Triângulos | O quê |
|---|---|---|---|
| `SM_civ_curb` | 2,0 × 0,47 × 0,40 (15 cm acima do asfalto, enterrado 25 cm) | 848 | o meio-fio reto (2 blocos com a junta), a face da rua inclinada, a quina de cima redonda, a sarjeta |
| `SM_civ_curb_corner` | 1,82 × 1,82 | 1 544 | a curva de 90° (raio 1,5 m) com a sarjeta |
| `SM_civ_sidewalk` | 2,0 × 2,17 | 12 544 | a calçada de placas de 40 cm com o chanfro, umas afundadas, uma quebrada, uma faltando, o meio-fio do lado da rua |
| `SM_civ_drain` | 2,0 × 0,65 | 1 716 | a boca de lobo no meio-fio e a grelha de ferro na sarjeta |
| `SM_civ_crosswalk` | 3,0 × 5,5 × 0,004 | 4 730 | 6 listras gastas (a borda comida pela isolinha, mais gasta onde o pneu passa) |

Convenção: o comprido ao longo de X, a rua do lado +Y, a origem no nível do asfalto.

## 3. O que o jogo precisa pra usar

1. **Os GLB**: `captures/game/models/civ_v2/<tipo>/SM_civ_*.glb`. Hoje o kit vem de `captures/look/civil_glb/`
   (`import_provisional.py` com `PROV_CIVIL_ONLY`, e `WorldForge/Tools/import_village.py`). As peças que SUBSTITUEM têm o mesmo nome:
   depois do OK, basta copiar os GLB pra lá (guardando os de hoje) ou apontar o `CIVIL_SRC` pra uma pasta com as do v2. Os GLB novos
   trazem a cor de vértice (as máscaras) e o UV: o `M_Structure` do elenco já lê (o `MASK_PARAMS` tem os slots `Civ*`).
2. **Os slots**: todos os materiais usados já existem no `SLOTS`/`CIVIL_SLOTS` (Plaster, Wood, Metal, Cloth, Dark, Rust, Char,
   Paint→TankPaint, Lens→Glass, Glass, Rubber, GlaPaint, Crate, Yellow, Concrete, BrokenAdobe): o import não precisa de mudança.
3a. **(01/11) O passo do kit, até o gerador mudar:** `python Game/Tools/village_kit_v2.py [mapa]` troca no arquivo da vila só a malha
    das miudezas que não bloqueiam (o `type` fica: o amasso do `SquashByType` segue valendo); é idempotente (`"kit": "v2"`), escolhe pelo
    hash do id (a LAN vê igual) e confere a mureta e as vizinhas no teto. **Rodar de novo depois de gerar a vila no WorldForge.** Sai
    quando a tabela abaixo entrar no `WorldVillage.cpp`.
3. **As peças novas no `WorldVillage.cpp`** (a tabela `Kits`: tipo, W ao longo de X, D ao longo de Y, H, bloqueia). A proposta:

| Tipo | W | D | H | Bloqueia | Onde o gerador põe |
|---|---|---|---|---|---|
| `civ_house_ruin` | 8 | 8 | 4,4 | sim | no lugar de uma térrea (o estado destruído; a planta e os vãos são os mesmos) |
| `civ_sedan`, `civ_hatch` | 2 | 4,6 / 4,2 | 1,5 | sim | na beira da rua, como o `civ_pickup` (o `Car()` do `WorldVillage.cpp`) |
| `civ_van` | 2 | 5,2 | 2,6 | sim | idem |
| `civ_truck` | 2 | 6,0 | 2,4 | sim | na rua larga e nos depósitos |
| `civ_hatch_burnt`, `civ_pickup_burnt`, `civ_van_burnt`, `civ_truck_burnt` | o do inteiro | | | sim | idem (o `civ_car_burnt` continua sendo o sedã) |
| `civ_pole_transformer` | 0,4 | 0,4 | 8 | sim | 1 a cada ~5 postes (os mesmos isoladores) |
| `civ_water_tank`, `civ_water_tank_stand`, `civ_tank_horizontal`, `civ_dish`, `civ_roof_ac`, `civ_laundry` | (a pegada medida acima) | | | não | nas lajes (como os `LL_Prop_*` de hoje) |
| `civ_awning_wall` | 3,1 | 1,7 | 2,5 | não | na parede da frente, sai pra +Y |
| `civ_fence_corr`, `civ_jersey`, `civ_dumpster`, `civ_market_stall`, `civ_streetlamp` | (idem) | | | sim | quintal, beira de rua, praça |
| `civ_tires`, `civ_rubble` | (idem) | | | não | quintal, pé de muro, ruína |
| `civ_curb`, `civ_curb_corner`, `civ_sidewalk`, `civ_drain`, `civ_crosswalk` | | | | não | pelo traçado da rua da cidade (o `BuildRoadSlabs`), não pelos lotes |

4. **Duas peças passam da pegada** (decisão do dono do WorldForge): a sacada do sobrado sai 0,77 m da célula de 8 m (a 3,2 m de
   altura: não bloqueia o chão); o toldo da loja (`civ_house_long`) põe os paus 2,3 m na frente da fachada, na calçada. Sem o recuo,
   o toldo vira o sem pau (`awning(..., posts=False)` no script, como o `SM_civ_awning_wall`).
5. **O carro amassado** (`<malha>_Crushed`, `docs/CARRO_AMASSADO_ESTUDO.md`) não foi feito: a lataria do v2 já está em seções de ~7 cm,
   a base que o estudo pede.

## 4. Slots que faltam (a proposta; não mexi no `import_provisional.py`)

As cores que os game shots têm e o kit não consegue:
- **o vermelho desbotado** das picapes (`road_town_battle_gemini`, 3 das 6) e o azul dos carros de cidade: um slot `CivCarRed`
  (~0,30, 0,09, 0,06 linear) e, melhor, a cor por instância no `M_Structure` (dado de primitiva), pra variar sem GLB;
- **o azul das bombonas** de plástico (`16_beauty_pass/b_gpt`, a esquina; hoje no `Dark`);
- **o vermelho dos tapetes** e do toldo de feira (`15_three_factions/b_gpt`; hoje no `Rust`, escuro demais);
- **o laranja-terra da lona** (`16_beauty_pass/b_gpt`, o toldo).

## 5. O asfalto: o que eu mudaria (só nota: o asfalto é de outra sessão e está aprovado, não mexi)

O que os game shots têm e o asfalto de hoje (`RECEITAS_DO_LOOK.md` §5–§7, `docs/APROVADOS.md` Mundos) ainda não tem:
- **a faixa do meio tracejada** (branca, gasta, com falhas) nas estradas entre vilas: `road_town_battle_gemini` (a pista inteira);
  a faixa da beira já está estudada no §7;
- **as rachaduras seladas com piche** (as linhas pretas e brilhantes que seguem a trinca em rede): `street_battle_usa_china_gemini`,
  o asfalto da direita; hoje a trinca é só a fresta;
- **na cidade**: a faixa amarela dupla, a linha de parada antes da faixa de pedestre, as setas no chão e a tampa redonda de bueiro no
  meio da pista (`city_modern_noon_gemini`); a faixa de pedestre e o bueiro de meio-fio estão aqui como malha;
- **a sarjeta de concreto** entre o asfalto e o meio-fio, com a areia juntada no canto (o `SM_civ_curb` já traz a sarjeta: o asfalto
  para 30 cm antes, ou a sarjeta fica 1 cm por cima);
- **as trilhas de roda polidas: NÃO** (reprovadas em 13/10).

## 6. O que está fraco (a crítica de diretor de arte, sem passar pano)

- **Carros:**
  - o vidro do inteiro é opaco (sem o banco atrás): de cima o para-brisa é uma placa escura; o `Glass` reflete o céu no jogo, mas o
    interior não existe;
  - o sedã e o hatch têm a mesma cara (o mesmo farol e a mesma grade); falta uma frente de cada;
  - a picape, a van e o caminhão passam dos ~40 mil (55, 64 e 70 mil): a carga (os sacos, as cordas) e a grade de 14 cm do executor nos
    tubos compridos (corda, bagageiro, friso). Dá pra cortar ~25% baixando os lados dos sacos e dos tubos, sem mudar o que lê;
  - a carcaça: as manchas vão de 25 cm a 1 m; o miúdo (a bolha da tinta, o pó de cinza) é do material de queimado do jogo;
    (01/11, a sessão do Elenco) ainda lia como camuflagem: agora a lateral inteira é o carvão e a ferrugem fica só no alto (do friso
    pra cima: o teto, o capô, a tampa), com o cinza oxidado miúdo no teto e a fuligem de ~20 cm (`Car.burn_field`, `burn_layers`). A
    estria que escorre não se resolve na malha (a face tem 20–30 cm): tentada e tirada;
  - não há o amassado do tanque (`_Crushed`).
- **Casas:**
  - a parede limpa demais no Cycles: o reboco, a sujeira escorrida e o tom por casa são do material (`CivPlaster`, `ToneVariation`);
    a malha só traz o remendo, a lasca e a quina gasta;
  - o sobrado (126 mil) e a casa comprida (131 mil, a área de 2 casas) passam um pouco do teto do orçamento (60–120 mil);
  - só a térrea tem a ruína; o sobrado em ruína, a mesquita (o marco) e os prédios de cidade ficam pra próxima;
  - a casinha da escada é a mesma em todas; falta a escada de fora no lugar dela, como no `16_beauty_pass/b_gpt` (a função `ext_stair`
    existe e não está em uso). (01/11) A casa comprida já tem a escada de fora (de alvenaria, na parede de trás) no lugar da casinha.
- **Miudezas:** as cores (o azul, o vermelho) presas nos slots de hoje (§4); a calçada e o meio-fio não estão ligados ao traçado das ruas.
- **Nada foi visto no jogo** (o pedido: sem Unreal e sem a placa). As prévias são Cycles na CPU com o material aproximado
  (`PREVIEW_MASKS=game`); a prova de verdade é no jogo, na câmera do game shot, ampliada.

## 7. As imagens

- As prévias de cada peça: `captures/game/models/civ_v2/<tipo>/preview/` (e `preview_burnt/`): `three_quarter_masks_game.png` (três
  quartos), `game_cam_masks_game.png` e `game_cam_close_masks_game.png` (a câmera do jogo, 37,5°, com o tanque USA de régua). As
  miudezas, a rua e os postes com os muros saem numa folha só por pasta (`props/`, `street/`, `pole/`). Os `left_` e `front_` que
  sobraram em `sedan/` e `hatch/` são de uma rodada anterior (não valem).
- Como refazer tudo: `captures/game/models/civ_v2/_run_all.sh` (o detalhe de todas as peças e as prévias, 2 Blender por vez) e
  `python Game/Tools/civ_paint_variants.py` (as variantes de cor, depois dos carros).
- As folhas, em `captures/game/models/civ_v2/_sheets/`:
  - `folha_gameshot_carros.jpg`, `folha_gameshot_casas.jpg`, `folha_gameshot_miudezas.jpg`: o recorte do game shot × o nosso;
  - `folha_kit_atual_x_v2_1.jpg` a `_3.jpg`: a peça de hoje do kit × a nova, mesma câmera e mesma luz.
