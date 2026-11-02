# Estudo: o veículo ferido (danificado e destruído)

01/11/2026. Estudo + protótipo (`Game/Tools/model_damage_vehicle.py`). **Nada ligado no jogo ainda.**

O pedido (avaliação de 01/11, item 12): "O veículo ferido parece intacto, só pegando fogo. O C&C Zero Hour tem uma forma clássica
de deixar ele bem danificado, danificado [...] olha como os RTS modernos lindos fazem, aplica essa melhoria."

O que manda no desenho:
- **14/10, reprovado:** as placas de tinta queimada pintadas no material do veículo "liam camuflagem pintada por cima, um negócio
  porco" (`docs/BASE_JOGAVEL.md`, Estados de dano). Então nada de mancha solta: o dano é geometria, e o escuro só sai de uma causa.
- **A câmera:** 88 m, 40°. O casco de 7,9 m fica com ~150 px em 1024, ou seja **~5 cm por pixel**. Lê a silhueta que muda e as
  zonas escuras de 0,5 m ou mais (≥ 10 px). Risco de 5 cm e o furo real de 3–5 cm somem (1 px).

## 1. Como os outros fazem

**C&C Generals / Zero Hour** (os INI do 1.04 e o código do motor, conferidos linha por linha):
- **Os limiares:** `UnitDamagedThreshold = 0.7` e `UnitReallyDamagedThreshold = 0.35`; `MovementPenaltyDamageState = REALLYDAMAGED`
  (o veículo fica mais lento a partir dos 35%)
  ([GameData.ini](https://raw.githubusercontent.com/TheSuperHackers/GeneralsGamePatch/main/Patch104pZH/GameFilesOriginalZH/Data/INI/GameData.ini),
  [ActiveBody.cpp](https://raw.githubusercontent.com/electronicarts/CnC_Generals_Zero_Hour/main/GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Body/ActiveBody.cpp)).
- **O veículo NÃO muda aos 70%.** Crusader (modelo AVLeopard), Paladin, Battlemaster, Scorpion e Humvee só têm `REALLYDAMAGED` e
  `RUBBLE` no Draw, e nos dois **o modelo inteiro troca** pelo `_D` (`AVLeopard_D`, `AVPaladin_D`, `NVBtMstr_D`, `UVLiteTank_d`,
  `AVHUMMER_d`). É diferente do prédio, que tem os três modelos (`docs/ESTUDO_DEGRADACAO_PREDIOS.md` §1).
  Fonte: `AmericaVehicle.ini`, `ChinaVehicle.ini`, `GLAVehicle.ini` (mesmo caminho do GameData).
- **A fumaça não mora no Draw:** é o módulo `TransitionDamageFX`, `ReallyDamagedParticleSystem1 = Bone:Smoke RandomBone:Yes
  PSys:SmallLightSmokeColumn`. Com `RandomBone:Yes` o motor sorteia um dos ossos `Smoke01…Smoke99` do modelo. **Não existe osso
  FIREPOINT nesses veículos** (o nome que se lembrava): o fogo do veículo ferido do ZH é só fumaça clara
  ([TransitionDamageFX.cpp](https://raw.githubusercontent.com/electronicarts/CnC_Generals_Zero_Hour/main/GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Damage/TransitionDamageFX.cpp),
  [W3DModelDraw.cpp](https://raw.githubusercontent.com/electronicarts/CnC_Generals_Zero_Hour/main/GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.cpp)).
- **A troca vem coberta:** na passagem pros 35% toca o `FX_BattleMasterDamageTransition` (estouro pequeno, brilho, destroços e som;
  `FXList.ini`).
- **A morte do Crusader:** 50/50 entre a torre que voa (`OCL_CrusaderTurret` lança `AVLeopard_D2`/`D3` a 85–90°) e o fogo
  (`TankFire` ~2 s, depois explode). Os dois deixam a carcaça `DeadCrusaderHulk` (modelo `AVLeopard_D1`), que afunda e some em ~10 s
  (`Hulk.ini`, `ObjectCreationList.ini`). O Humvee não tem carcaça: voam 2 pneus e 2 portas.
- **O que lê de cima:** um salto de silhueta aos 35% + a coluna de fumaça saindo de um ponto do modelo.

**Company of Heroes.**
- CoH1: o veículo morto vira um *wreck*, obstáculo que dá cobertura pesada e pode ser destruído ou esmagado; o motor danificado
  deixa lento ([wiki Vehicle](https://companyofheroes.fandom.com/wiki/Vehicle), [wiki Wrecks](https://companyofheroes.fandom.com/wiki/Wrecks)).
- CoH2: os críticos (motor, imobilizado, torre, canhão destruído…) são de jogo
  ([coh2.org](https://www.coh2.org/topic/30579/title/post/282561)); **não confirmei** o que muda no modelo.
- **CoH3:** o artista-chefe diz que nos anteriores o dano era "baked down" (fixo no modelo); no 3, cada tanque tem **regiões** que
  mostram o dano conforme o lugar e o tipo do impacto, num shader de veículo próprio
  ([NME](https://www.nme.com/news/company-of-heroes-3-tanks-will-show-accurate-dirt-and-damage-3210943)).

**War Thunder** (os blogs oficiais do Gaijin):
- a marca na blindagem tem a forma do ângulo do tiro, o furo tem o diâmetro do calibre e **o fogo sai pelos furos**; a carcaça fica
  até o fim da partida ([Dagor 6.0](https://warthunder.com/en/news/6946-development-dagor-engine-6-0-visual-improvements-effects-and-dlss-en));
- **saem voando** galões, ferramentas, a esteira reserva, rodas e rodas-motrizes, com física e às vezes rastro de fogo
  ([efeitos novos](https://warthunder.com/en/news/7843-development-new-visual-effects-in-the-drone-age-update-en));
- todo projétil cinético gera um cone de estilhaços lá dentro
  ([diário do dano](https://warthunder.com/en/news/387-Developers-Diaries-Ground-Forces-Damage-Model-Part-2-en)).

**Men of War (GEM):** dano por componente em vez de barra de vida; destroços sólidos
([Wikipedia](https://en.wikipedia.org/wiki/Men_of_War_(video_game))). **Não confirmei** o que aparece no modelo.
**Armored Warfare:** a explosão do paiol arranca a torre ([blog](https://armoredwarfare.com/en/node/6752)).
**Steel Division 2 / WARNO:** nada confirmado.

## 2. O fenômeno real

- **A carga oca** (RPG, ATGM): penetra ~6× o próprio diâmetro (8–10 bem feita), o jato não derrete o aço (efeito cinético, ~1100–1200 K)
  e **o furo é pequeno e afina** ("fist to finger") ([Wikipedia](https://en.wikipedia.org/wiki/Shaped_charge)). O que se vê de
  fora é a marca preta do explosivo em volta, não o furo.
- **O estilhaço (spall)** sai da face de dentro mesmo quando o jato não atravessa
  ([HEAT](https://en.wikipedia.org/wiki/High-explosive_anti-tank_warhead)).
- **A torre arrancada** ("jack-in-the-box"): a munição detona e a pressão escapa pelo anel da torre (T-72, Challenger 2 em Robotyne
  2023, Leopard 2A4 na Síria 2017) ([Wikipedia](https://en.wikipedia.org/wiki/Jack-in-the-box_effect)).
- **A saia é o que mais apanha:** num relatório do Abrams no Iraque (Janes, citado no fórum do army.ca) as saias não balísticas foram
  "repetidamente penetradas" pelo RPG anticarro ([army.ca](https://army.ca/forums/threads/janes-defence-abrams-tank.1678/)).
- **A tinta e o calor:** a 750 °C a tinta é toda destruída; casca de óxido soltando no aço indica mais de 500 °C (aço estrutural em
  incêndio, não tinta de tanque) ([NRC NUREG/CR-6987](https://www.nrc.gov/regulations-legislation/nureg-series-publications/publications-prepared-by-nrc-contractors/cr6987)).
- **A carcaça queimada** (só fórum de modelismo, **não confirmado**): a borracha das rodas vira cinza clara e sobra o miolo de aço;
  a tinta perto do fogo preteja e vira cinza, a mais longe muda de cor com o calor; o aço sem tinta enferruja rápido; a suspensão
  cede e a carcaça senta ([kitmaker](https://forums.kitmaker.net/t/tank-wheels-with-rubber-melted-off/33120),
  [armorama](https://archive.armorama.com/forums/242640/index.htm)).

**O que tiro disso pra câmera alta:** (1) a silhueta muda em saltos, como o ZH; (2) o escuro só nasce de uma causa que se vê: o
furo, a caixa que estourou, a grelha do motor; (3) o escuro é um degradê que sai da causa (preto → tinta queimada → tinta), nunca
placa; (4) o furo de verdade some, então o furo do modelo é exagerado de propósito (Ø 20–36 cm), como a ferragem de 4–6 cm dos prédios.

## 3. O que entra em cada estado

| | Danificado (< 70%) | Destruído (< 35%) | Por quê |
|---|---|---|---|
| **Saia / chapa do lado** | 1 arrancada (sobra a tira de cima rasgada, com a espessura de aço) e a vizinha **pendurada** por um parafuso (26–36°, aberta 12–18° pra fora) | a pendurada caiu, +1 arrancada no mesmo lado; do outro lado +1 arrancada e a da frente **torta pra fora** | a saia é o que mais apanha (Janes); o buraco mostra a rodagem escura (0,8 × 0,8 m ≈ 16 px): é a silhueta que mais lê |
| **Paralama** | amassado 8 cm na quina da frente | 14 cm | lê pelo sol rasante na quina |
| **Furo** | 1 na torre e 1 no flanco, com a fuligem do estouro em raios (preta até ~0,4 m, some até ~0,7 m) | +1 grande na torre (ATGM, fuligem até 0,85 m), +1 na frente da torre, +1 na frente do casco | a marca de verdade é o preto do explosivo; o furo sempre com a espessura e a cavidade escura atrás |
| **Antena** | a maior dobrada (70–105°) | quebrada; as outras dobradas ou quebradas | linha fina que lê na câmera de 40° |
| **Caixa de fora** (defesa ativa, ferramenta) | 1 arrancada, com a marca do estouro embaixo | a pilha inteira e mais 2 | furo na silhueta da torre |
| **Motor** | só o fio de fumaça clara (o jogo já faz) | **a fuligem sai de cada grelha** (preta até ~meia grelha além da borda, some em ~2×) e o calor escurece a tinta do compartimento (até 0,32: tinta queimada, não preta); a pluma suja a traseira da torre que fica em cima | o jogo acende o fogo do motor aos 35% (`VehicleDamage`): a fuligem fica onde o fogo está |
| **Lona e carga** | — | a lona do deck queima (sobra pedaço longe do fogo, com a borda carbonizada); 60% da carga do cesto some, o resto carbonizado | é o que pega fogo primeiro, e de cima é uma mancha grande (a lona azul vira o deck escuro) |
| **Fogo / fumaça (json)** | fumaça clara no furo do casco e no da torre | fogo nas grelhas e nos furos; fumaça grossa no motor | o ZH: fumaça num osso sorteado; aqui os pontos saem do lugar que quebrou |
| **Carcaça (morte)** | — | (fora deste protótipo) | a carcaça do Look continua; o que falta lá é a roda sem borracha e a carcaça sentada |

**Por que dois estados e não só o do ZH:** o ZH não muda o veículo aos 70% (só aos 35%). O pedido é "bem danificado, danificado", e
o CoH3 mostra o dano crescendo. Então o 70% existe, mas moderado (a chapa, um furo de cada lado, a antena); o salto grande (o motor,
a lona, as chapas dos dois lados) fica nos 35%, como no ZH.

## 4. O protótipo: `Game/Tools/model_damage_vehicle.py`

```
"C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_damage_vehicle.py -- ^
    captures/game/models/usa_mbt SM_LL_USATank 7 captures/game/models/usa_mbt_damage_proto --render captures/dano_veiculos
```

- **Entra:** `<PREFIXO>_Hull.glb` e `_Turret.glb` (se houver). **Sai numa pasta separada** (padrão `<pasta>_damage`; recusa a pasta
  do modelo): `_Hull_Damaged/_Wrecked.glb`, `_Turret_Damaged/_Wrecked.glb` e `<PREFIXO>_damage.json`. Com `--render`, a prova
  em Cycles na CPU.
- **Sem código por veículo.** Lê a geometria:
  - as peças soltas (as ilhas, soldando os vértices que o GLB parte nas quinas duras) com caixa, área e material;
  - **chapa do lado:** fina no Y, 0,35–2,2 m, na borda do casco (a saia, a porta, o paralama), com as peças presas nela (faixa da
    cor do jogador, venezianas);
  - **antena:** pegada < 7 cm e > 0,6 m acima do teto;
  - **caixa de fora:** pequena e cheia na borda do lado; **carga:** pequena no terço de trás, perto do meio;
  - **o motor:** a ponta com mais área escura/metálica no alto (as grelhas); sem grelha, atrás no de esteira e na frente no de
    rodas. `<PREFIXO>_damage_hints.json` força `engine` e `side`;
  - **o furo:** numa face das peças grandes que "vê fora" (o raio pela normal e pela diagonal de cima não bate no modelo: nada atrás
    da saia).
- **A malha não tocada fica idêntica** (vértice, normal endurecida, UV e as máscaras do `model_detail.py`): a troca só mostra o que
  quebrou.
- **A queima vai no alfa da cor de vértice** (`A = 1 − queima`; o `model_detail.py` grava R/G/B e deixa o A = 1, o canal livre).
  É um campo liso que sai da causa (o furo, a caixa, a grelha, a pluma), com o ruído só na borda: sem placa, sem ilha.
- **Os slots são os do modelo:** a cavidade e a borda do furo no `Dark`, a borda da chapa rasgada no `Metal`. O jogo importa sem
  tabela nova.
- **O M1 (semente 7):** 9 chapas por lado, motor atrás, 4 grelhas, lado do tiro = direita. Casco 104 mil → 106 mil triângulos
  (danificado) e 103 mil (destruído: a lona e as chapas saem, os furos entram); torre 48 mil → 48,6 mil / 50 mil. ~1 min sem a prova; ~6 min com.

**Limitações:**
- A rodagem e as esteiras **não mudam** (são peças que o jogo anima). A oclusão delas foi assada com a saia no lugar, então a roda
  atrás da chapa que saiu fica um pouco escura. Rodas sem borracha e esteira solta ficam pra carcaça.
- A borda rasgada da chapa sai em dentes (a grade de 4,7 cm do refino): perto parece serrilhada, no jogo é menos de 1 px.
- A fuligem da pluma na torre foi calculada com a torre pra frente; girada, ela fica onde estava (o fogo sujou naquela posição).
- As heurísticas foram provadas só no M1. Picape, caminhão e Stryker devem achar portas, paralamas e caixas, mas sem prova; o
  `_damage_hints.json` é a saída se errar o motor ou o lado.
- O furo no 70% e no 35% é o mesmo (mesma semente): o dano só cresce, nunca muda de lugar.

## 5. O que falta pra ligar no jogo (não implementado)

1. **`ARTSUnitActor`:** hoje o `UpdateStructureState` só roda com `bStructure`. Pro veículo, a mesma troca: `HullParts[0]` vira o
   `_Damaged`/`_Wrecked` e `TurretParts[0]` o da torre, nos 70/35% com a volta acima de 85% (a mesma histerese), em silêncio na névoa.
   A troca vem coberta como no ZH (`FX_BattleMasterDamageTransition`): o estouro pequeno + **as chapas do `torn_off_*` saindo
   voando** (do tamanho delas, pelo arco do `ARTSDebrisField`), no quadro da troca.
2. **`Units.json` (`usa_mbt`, pelo `units_json_io.save_units`) e `RTSGameData`:** o bloco `damage_states` que o prédio já lê
   (`damaged`, `wrecked`, `fire_damaged_m`, `fire_wrecked_m`) mais as chaves novas: `turret_damaged`, `turret_wrecked`,
   `smoke_damaged_m`, `smoke_wrecked_m`, `turret_smoke_damaged_m`, `turret_fire_wrecked_m` (no espaço da torre, que gira) e
   `engine_m`. Os valores estão em `captures/game/models/usa_mbt_damage_proto/SM_LL_USATank_damage.json`.
3. **`UBlastFxDirector::VehicleDamage`:** o fogo do motor sai do `engine_m`/`fire_wrecked_m` do veículo, no lugar das constantes
   `DeckBackCm`/`DeckUpCm` (iguais pra todos); a fumaça clara do danificado nos `smoke_damaged_m`. Tudo no Fluxo, em volume.
4. **O material das unidades (o CODE do RTSLook):** ler a queima no `VertexColor.A`, uma rampa só: tinta → tinta queimada
   (0,13/0,105/0,085, fosca) → fuligem (0,016), a rugosidade subindo até 0,95. É o que a prova usa (`render_mat`). **Sem isso, no
   jogo aparece a geometria (chapas, furos, antenas, caixas, lona) mas não a fuligem.** Conferir no import que o alfa do `COLOR_0`
   chega na Unreal.
5. **O import:** com o aprovado, os GLB vão pra `captures/game/models/usa_mbt/` (o `import_provisional.py` já pega
   `SM_LL_USATank_*.glb` daquela pasta pra `/Game/Units/usa_mbt_v2`; os caminhos do `Units.json` são explícitos, não colidem).
6. **A prova no jogo** (a regra de sempre): o tanque levando tiro até os 35%, em vídeo, ampliado, na sombra de um prédio e ao lado
   de uma explosão, A/B com o de hoje; o custo medido (+2 malhas por tipo de veículo, Nanite).

## 6. A prova (Cycles na CPU, a luz rasante quente)

`captures/dano_veiculos/`: `dano_jogo.jpg` (a câmera do jogo, 1024, os 3 lado a lado: intacto, danificado, destruído),
`dano_jogo_ampliado.jpg` (o mesmo quadro 2× no pixel), `dano_perto.jpg` e `dano_perto_cima.jpg` (cada um no mesmo enquadramento,
colados). As cores são as de conferência; a tinta e as máscaras imitam o material do jogo.
