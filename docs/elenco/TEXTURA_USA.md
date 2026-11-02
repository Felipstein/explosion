# A tinta de campanha da USA (item 40 do `TAREFAS.md`, 01/11/2026)

O pedido (01/11, 13h): "as texturas da USA, eu acho que estão ruins ainda, olhe os game shots e tal, as outras facções você fez
muito bem as texturas, acho que a USA também merece, além de remodelar os modelos, uma texturização".

## 1. O que o alvo tem e o nosso não (medido)

Alvo: os 3 tanques da USA e a War Factory na `r1_raid_slabs` (`docs/aprovados/alvos/r1_raid_slabs_gemini.webp`). Nosso: o tour
`captures/game/usa_b_tour_A` (antes). A luz da tinta medida contra a areia da mesma imagem, pra não depender da exposição:

| | tinta/areia (mediana) | p10 | p90 | R/B da tinta |
|---|---|---|---|---|
| alvo, os 3 tanques | 0,75–1,09 | 0,50–0,76 | 1,12–1,44 | 1,18–1,21 |
| nosso tanque B | 0,68 | 0,50 | 1,10 | 1,11 |
| nosso Stryker / Linebacker / Paladin | 0,73–0,79 | 0,52–0,54 | 0,94–1,25 | 1,11 |

- **O valor da tinta já está certo** (a calibração de 29/10). Falta o **padrão**: no alvo a junta das chapas é **escura**, o baixo
  do casco (rodagem, esteira, saia) é **cor de areia**, e o deck tem véu de pó. O nosso é a tinta lisa com a sujeira de fresta
  **cor de areia** e a quina **clareada**: em tinta clara, as duas têm o valor da tinta e somem. Por isso a China e a GLA leem bem
  (a areia e a quina clara contrastam com a tinta escura) e a USA não.
- **A tinta é mais quente** no alvo (R/B 1,2 contra 1,1): é o pó por cima, não a cor da tinta.
- **O azul do jogador fica forte.** No alvo ele é ardósia apagado, mas o Felipe já disse (25/10, ficha do Stryker) que "até a do
  game shot é fraca". A faixa e a lona da cor do jogador não levam poeira.

## 2. Como se faz (fontes)

- **Modelismo de blindado no deserto** (a referência de quem pinta blindado): a poeira primeiro, grossa no baixo (rodas e casco de
  baixo os mais empoeirados), depois a lavagem **escura** só nas linhas de painel fundas ("senão o modelo fica contornado de caneta
  preta"); lasca com moderação, que muitos nem usam, e o sol desbotando a tinta.
  [FineScale, "Desert weathering"](https://forum.finescale.com/t/desert-weathering/195296);
  [Armorama, pintura e envelhecimento](https://archive.armorama.com/forums/233274/index.htm);
  [AK, a lavagem pra tinta areia clara](https://www.scalemodelshop.co.uk/ak-interactive-dak-wash-ak066).
- **Nos jogos**, a mesma pilha em camadas: a sujeira por altura com ruído pra quebrar o degradê (no espaço da peça, pra andar junto
  com ela), o pó pela normal pra cima, e a quina e a fresta pelas máscaras cozidas na cor de vértice.
  [Smart Height Dirt (UE)](https://forums.unrealengine.com/t/paulareb-smart-height-dirt-procedural-mask-educational-breakdown/2708321);
  [máscaras de posição e normal](https://weisl.github.io/simple_pbr_layering/material_masks/);
  [Studio 397: máscara de cor de vértice com pó, fresta e desgaste](https://docs.studio-397.com/x/NQBDAg);
  [80.lv: camadas da UE5 com máscaras cozidas](https://80.lv/articles/combining-unreal-engine-5-s-material-layers-with-substance-3d-s-baked-masks).
  O CoH3 e o WoT, no `docs/ESTUDO_DETALHE_AAA.md` §1 item 3.

## 3. O que mudou (`Game/Tools/import_provisional.py`)

Três parâmetros novos no material mestre, só no modelo do detalhe (o UV1 marcado), dentro do bloco das máscaras:

- `DustCoat`: a poeira cor da duna (0,40; 0,31; 0,21) grossa até ~0,9–1,7 m de altura (o limite varia por ruído de ~0,7 m), fina
  no que olha pra cima (em manchas) e um véu de 12% no resto; o grão de ~11 cm varia o tom. A rugosidade sobe, o metal some.
- `Wash`: a lavagem escura na fresta funda (a máscara de fresta acima de 0,25), até −60%.
- `Chip`: poucas lascas escuras de ~5 cm nas quinas (o primer e o aço por baixo da tinta clara).

Instâncias só da USA de terra (a China, a GLA e a Rússia usam o `TankMetal`, a esteira e a borracha compartilhados): `UsPaint`
(DustCoat 0,7, Wash 0,8, Chip 0,6, a areia na fresta de 0,6 pra 0,15, o tom por chapa 0,8), `UsMetal`, `UsTrackRun` e `UsRubber`
(a rodagem empoeirada). O mapa `USA_VEHICLE_SLOTS` (a chave `'usa'` no `FACTION_MODELS`): o tanque B, a Paladin e as 5 unidades B
de terra. Os aéreos ficam na `TankPaint` sem poeira de baixo.

## 4. A prova

A/B na mesma corrente: `captures/game/usa_b_tour_A` × `usa_b_tour_B` (de perto) e `elenco_prova_tanque_usa_glacis` ×
`elenco_prova_tanque_usa_B` (a câmera do jogo), medido contra o alvo pela mesma conta da §1.

## 5. As duas voltas (01/11, no jogo; `captures/game/usa_b_tour_A`, `_B`, `_B2`)

| | R/B da tinta | tinta/areia (mediana) |
|---|---|---|
| alvo (os 3 tanques) | 1,20–1,25 | 0,82–1,05 |
| antes | 1,11–1,15 | 0,69–0,77 |
| 1ª volta (véu 12%, lavagem só na fresta funda) | 1,12–1,18 | 0,68–0,78 |
| 2ª volta (véu 22%, lavagem na fresta rasa, escorrido fino nas paredes, mais pó em cima) | 1,15–1,20 | 0,67–0,78 |

A 1ª volta acertou o baixo (a rodagem, a esteira, os pneus e o pé das saias cor de areia, como no alvo) e deixou o de cima limpo.
A 2ª esquenta a tinta perto do alvo e mostra a junta e o escorrido de leve. O que ainda separa do alvo é o brilho do topo ao sol
(p90 1,15–1,24 contra 1,40–1,62), que é a luz da cena e não a tinta. Parado aqui pra ele ver (uma coisa de cada vez).
