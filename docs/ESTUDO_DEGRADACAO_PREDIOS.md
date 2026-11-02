# Estudo: o prédio que degrada bonito (e não um jogo de Lego)

31/10/2026. Estudo e plano. Nada foi implementado ainda.

O pedido, palavra por palavra:
- "as construções têm que DEGRADAREM DE FORMA LINDA E BONITA E NÃO UM JOGO DE LEGO";
- antes (30/10): "quando tá com pouca vida, além de só pegar fogo, quero danificações, desabamentos também enquanto tá sendo
  destruído".

## 0. O que hoje lê como Lego

Vi nos quadros de `captures/game/v11_fx_predio_etapas/batalha.mp4`, `escala_v5/predio_dano_fogo_entulho.mp4` e
`escala_v5/predio_entulho_perto.jpg`.

- **O dano é pinta.** As placas de lasca e de fuligem do `M_Structure` (o bloco `Damage > 0.2` do CODE em
  `import_provisional.py`) espalham manchas pretas iguais pelo prédio inteiro. Leem como dálmata ou camuflagem, não como dano.
  Nada mostra de onde o dano veio.
- **O furo é um recorte de papel.** `OpenStructureHole` é uma máscara sem espessura (raio de 1,3 a 2,8 m). O lado de dentro é só
  o verso da casca escurecido a 0,22: sem laje, viga nem andar. A borda é rasgada pelo ruído, mas não tem a grossura da parede.
- **A morte é um elevador.** O prédio inteiro e rígido desce 80% em 3,5 s (`StructureSinkFraction`) dentro das bolas de fogo.
  No lugar fica um tapete uniforme de pedras cinza-azuladas iguais (`AddRubble`), que não têm a cor, a planta nem um osso do
  prédio. Ninguém lê "aqui era a War Factory". Esse é o Lego: blocos genéricos que surgem do nada.
- **O que fica:** o fogo do telhado e as baforadas de fumaça em volume (aprovados, "top demais") e o ritmo dos 70/50/35/20%.

## 1. Como os outros fazem

**C&C Generals / Zero Hour.** Confirmado nos INI do 1.04 e no código do motor:
- **Três modelos por prédio.** O padrão, o `_D` (DAMAGED, abaixo de 70%) e o `_E` (REALLYDAMAGED, abaixo de 35%; serve também
  de RUBBLE). Os limiares são `UnitDamagedThreshold = 0.7` e `UnitReallyDamagedThreshold = 0.35`, no `GameData.ini`. O prédio
  civil tem ainda o `_R`, o modelo de depois do colapso (`POST_COLLAPSE`). Na War Factory USA são `ABWarFact`, `ABWarFact_D` e
  `ABWarFact_E` (`FactionBuilding.ini`).
- **O fogo e a fumaça moram nos ossos do modelo danificado.** São os ossos `Fire01…`, `Smoke01…` e `Dust01` do `_D`/`_E`, com
  `SmolderingFire` e `SmolderingSmoke`. O fogo sai de onde o artista quebrou o modelo.
- **A troca de estado vem coberta.** O `TransitionDamageFX` solta uma baforada na passagem dos 70%; na dos 35%, a baforada, um
  estouro médio e uma onda de choque.
- **A morte do prédio de facção.** `FX_StructureMediumDeath` mais `OCL_LargeStructureDebris`: cerca de 45 pedaços jogados longe
  (`SEND_IT_FLYING`), e esses pedaços são modelos do próprio prédio (`ABPwrPlant_d01…d30`). Depois o prédio some (`DestroyDie`).
- **A morte do prédio civil.** O `StructureCollapseUpdate` faz o prédio tremer ±0,6 u (só no cliente, aleatório visual). Ele
  afunda com a gravidade, solta um estouro a cada 250–800 ms (1 em cada 4 é grande) e, quando passa do chão, troca pro `_R`.
  O entulho tem `DefaultStructureRubbleHeight = 10` u, uns 2,6 m.
- **O que lê de cima:** a silhueta muda em dois saltos, o fogo nasce no lugar quebrado e os destroços são pedaços do prédio.
- **Não confirmei** o que a geometria do `_D`/`_E` tem por dentro, porque não abri os W3D. Pela memória do jogo, é telhado aberto
  e estrutura à mostra.
- Fontes: [FactionBuilding.ini](https://github.com/TheSuperHackers/GeneralsGamePatch/tree/main/Patch104pZH/GameFilesOriginalZH/Data/INI),
  [StructureCollapseUpdate.cpp](https://github.com/electronicarts/CnC_Generals_Zero_Hour/blob/main/GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/StructureCollapseUpdate.cpp)
  e o [tutorial das pontes no ModDB](https://www.moddb.com/mods/command-conquer-generals-version-20/tutorials/gen-make-bridges-destroyable)
  (os estados damaged, really damaged e rubble).

**Company of Heroes 3** (o diário da Relic, lido pela imprensa: a página original do fórum da Relic não abriu, erro de
certificado):
- O prédio inteiro é fraturado em geometria de verdade (o "fracture the entire building" do artista de ambiente).
- Os pedaços mantêm a textura da malha inteira.
- O prédio é feito em camadas (reboco, tijolo…), que aparecem com o dano, e cada camada solta o próprio entulho.
- A simulação calcula quais células a área do tiro atinge.
- Os pedaços escorregam pelo telhado.
- No fim fica uma "husk", uma casca que serve de cobertura.
- Fontes: [NME](https://www.nme.com/news/gaming-news/company-of-heroes-3-destruction-overview-details-collapsing-buildings-3137061),
  [PCGamesN](https://www.pcgamesn.com/company-of-heroes-3/building-destruction-physics) e
  [GameSpot](https://www.gamespot.com/articles/company-of-heroes-3-has-some-impressive-building-destruction-features/1100-6499603/).

**Company of Heroes 1 e 2.** Usavam Havok, e o entulho era um efeito pronto "que às vezes não combinava com o prédio" (a Relic,
na NME acima). Um patch do 2 fala em "panel buildings", prédios por painéis
([changelog no Steam](https://steamcommunity.com/sharedfiles/filedetails/changelog/341279469?p=10)). O detalhe dos painéis
(cada trecho de parede com versões) **é memória minha, não confirmei.**

**Age of Empires IV.** Primeiro o dano aparece no modelo, depois o fogo. Abaixo de ~25% o prédio pega fogo e queima até o chão se
ninguém consertar ([TV Tropes](https://tvtropes.org/pmwiki/pmwiki.php/VideoGame/AgeOfEmpiresIV)). **Os 25% não estão
confirmados** em fonte oficial. O colapso foi criticado como inacabado
([fórum](https://forums.ageofempires.com/t/plans-for-building-destruction-improvement/38957)). Lê bem porque o fogo é a vida.

**Iron Harvest, Tempest Rising, Men of War.** Só achei o geral:
- Iron Harvest: tudo é destrutível e a cobertura muda ([PC Gamer](https://www.pcgamer.com/alternate-history-mech-game-iron-harvest-details-destructible-environments-in-new-video)).
- Tempest Rising (UE5): uma resenha diz que o prédio cai por etapas, conforme o lado do dano. **Não confirmei com o estúdio.**
- Men of War (GEM): destruição quase total.
- **Nenhum dos três tem detalhe técnico publicado que eu tenha achado.**

**Battlefield e Frostbite:**
- No Bad Company, o prédio desaba quando caem paredes suficientes.
- O Frostbite 2 usa uma máscara por campo de distância em volume, feita de esferas (Kihl, SIGGRAPH 2010,
  [PDF](https://www.advances.realtimerendering.com/s2010/Kihl-Destruction%20in%20Frostbite(SIGGRAPH%202010%20Advanced%20RealTime%20Rendering%20Course).pdf)).
  É o que o nosso `_Holed` já faz. Lá, a máscara corta uma malha que tem espessura e o "miolo" por trás.
- No [Battlefield 6](https://www.ea.com/en/news/how-battlefield-6-redefined-destruction) a destruição é por partes e os
  destroços nascem dessas partes.
- O Teardown (voxel) serve só de ideia: a borda do furo mostra o material por dentro.

**Deterrence**, um RTS indie
([devlog](https://www.gamedeveloper.com/blogs/rts-building-damage-and-destruction-deterrence---video-devlog-15)):
- duas fases de malha, cortadas com a faca no Blender e com o furo preenchido em cratera;
- na morte, a animação de queda e o entulho, com a poeira escondendo a troca.

É o truque mais barato do gênero.

**O que lê bem na câmera alta** (61 m, 37,5°; a 67 m, 1 px ≈ 3,3 cm, então 1 m ≈ 30 px):
1. **A silhueta que muda.** A platibanda mordida, uma quina que falta, a laje pendurada valem mais que qualquer textura.
2. **A profundidade no furo.** O escuro com uma laje e uma viga dentro, e a borda com a grossura da parede, mais clara (o miolo
   novo) que a pele velha.
3. **O fogo e a fumaça saindo do furo e da janela**, não do meio do telhado.
4. **O entulho na cor do prédio e amontoado no pé da parede** (o talude), nunca espalhado por igual.
5. **A ruína com a planta e um osso do prédio:** dá pra ler o que ele era.
6. **A poeira esconde cada troca.**

## 2. O plano

**As regras do plano:**
- **A simulação decide só a vida e a hora da morte.** Todo o resto é uma função visual da vida mostrada, do `CosmeticHash(SimId)`
  e das marcas de tiro que já são cosméticas. Nada volta pra `SimCore`.
- **A ruína fica dentro da pegada mais 3 m**, a `RuinCoverMargin` da simulação 31. Assim o que o jogador vê bate com a cobertura
  que a simulação dá. Hoje o `AddRubble` espirra até 0,5 H (4,5 m num prédio de 9 m): passa da regra.
- **Uma ferramenta só, "o destrutor"** (`Game/Tools/model_damage.py`). Ela roda sobre qualquer GLB de prédio, o provisório por
  script e o modelo final que o Felipe vai fazer à mão, com um JSON pequeno de dicas por prédio:
  - a peça-assinatura;
  - o que é osso (pórtico, coluna);
  - a facção (o material que quebra).

  Não é código à mão por prédio.
- **Este plano muda o `docs/PRODUCAO_3D.md` §6 (08/10).** Lá: "danificado: a mesma malha, não precisa de outra". Esse caminho é
  o que deu Lego hoje. O ZH e o CoH trocam geometria por estado.
- **Mantém de lá:**
  - a fratura em quebra-cabeça que atravessa as paredes ("separar a casa por parede NÃO serve", o Felipe em 08/10);
  - a ruína baixa (até ~1,5 m de massa).
- **Decisão dele:** os ossos vazados (coluna, viga torta, o pórtico da War Factory) podem ficar em pé até ~4 m? Eles não tapam a
  unidade e são o que faz a ruína ser "a War Factory".

### (a) O que o Blender gera por prédio

**Os arquivos que o script de cada prédio solta,** ao lado do `SM_X_*.glb` (o `import_provisional.py` já pega `SM_*_*.glb`
por pasta):
1. **`SM_X_Damaged.glb` (70%).** O modelo com 2–4 mordidas na platibanda, nas quinas e na borda do telhado:
   - **O corte** é um booleano EXACT com um cortador de ruído, nunca um círculo. A face de corte ganha o slot novo `Broken` (o
     miolo: adobe ou concreto mais claro e áspero) e, onde o corte passa, a grossura de verdade (0,25–0,4 m).
   - **Onde a mordida tira um pedaço do telhado,** aparece o escuro de dentro com a laje.
   - **As janelas quebram:** sai o vidro e fica o caixilho.
   - **Os pontos de fogo `Fire01…`** (empties) ficam nas mordidas e saem num JSON.
2. **`SM_X_Wrecked.glb` (35%).**
   - **O telhado cede em 1–2 áreas,** com a laje pendurada a 20–40° pela borda que ainda segura: a "dobradiça" da ferragem. É a
     silhueta que mais lê de cima.
   - **Uma quina cai inteira:** duas paredes e o canto da laje, uma célula só, nunca por parede.
   - **O interior aparece:** a laje do andar, 2–4 colunas e as vigas.
   - **A peça-assinatura fica em pé:** o pórtico da War Factory, o telhado de pagode torto, a cúpula do CC da GLA rachada. Assim
     ele ainda é quem é.
   - **No pé da quina caída,** o talude de entulho na cor do prédio.
3. **`SM_X_Ruin.glb`.**
   - **A planta inteira em muretas de 0,5–1,5 m.**
   - **As lajes tortas.**
   - **Os ossos:** 1–3 colunas e uma viga torta.
   - **A peça-assinatura quebrada em cima do monte,** com o monte a ~1/5 da altura.
   - **As cores são os mesmos slots do prédio** (Plaster, Player, Metal…), com a fuligem por cima.
4. **`SM_X_Chunks.glb` e `X_collapse.json`.**
   - **Os pedaços:** o `_Wrecked` fraturado em 40–120 células de Voronoi que atravessam as paredes, na lei de potência: poucas
     lajes de 2–4 m e muitas de 0,5–1,5 m.
   - **No JSON, por pedaço:** a pose de saída (a do `_Wrecked`), a pose de chegada e o atraso.
   - **A chegada sai de um rigid body do Blender, rodado uma vez no script,** com uma parede invisível na pegada mais 3 m. A
     ruína é a pose final dos próprios pedaços, então não há troca visível no fim.
   - **2–3 variantes por prédio,** caindo pra lados diferentes (o `PRODUCAO_3D.md` já pedia "2–3 versões por casa").
5. **O interior mínimo:** lajes a cada ~3,5–4 m, colunas e um fundo escuro. Só onde a parede abre: o resto continua oco.
6. **O material que quebra, por facção:**
   - **GLA (adobe):** quebra em torrões redondos e mostra caibros e troncos de palmeira, sem ferragem.
   - **USA (chapa e concreto):** a chapa dobra, a viga I aparece e a ferragem sai do concreto.
   - **China (concreto e telha):** a ferragem e a telha que escorrega.
   - **A ferragem tem 4–6 cm, de propósito.** A de verdade, de 1,2–2,5 cm, some (fica abaixo de 1 px). São tufos de 0,3–0,8 m,
     tortos, só nas bordas de concreto.

**Como, no Blender:**
- **O Voronoi é nosso:** ~150 linhas com `bmesh.ops.bisect_plane`, sem add-on.
- **Por que não o Cell Fracture:** virou extensão do Blender 4.2+
  ([extensions.blender.org](https://extensions.blender.org/add-ons/cell-fracture/)), e as resenhas dizem que o material das faces
  de dentro falha no 4.3. **Não confirmei** o nome do operador na extensão.

**O custo:**
- a ferramenta genérica: 3 dias;
- cada prédio: ~0,5 dia (as dicas e a prova);
- as chaves novas no `Units.json` (`damaged`, `wrecked`, `ruin`, `collapse`, `fire_points_m` por estado): pelo
  `units_json_io.save_units`, nunca `json.dump`;
- a malha: Nanite, 3 malhas a mais por tipo de prédio.

### (b) O que muda no `RTSUnitActor`

- **A troca de estado,** `UpdateStructureState(Health01)`, entra junto do `UpdateStructureHoles`:
  - abaixo de 70%, o casco vira `_Damaged`;
  - abaixo de 35%, `_Wrecked`;
  - consertado acima de 85%, volta (a mesma histerese dos furos).
- **A troca vem coberta, como no ZH.** No mesmo quadro saem a baforada do `StructureDamage` e os pedaços daquela mordida, que
  caem e ficam no pé da parede. O entulho nasce do que saiu do modelo, como o item 31 pedia.
- **O ritmo continua:** 70 (troca D), 50 (furo), 35 (troca W), 20 (furo). Os furos de máscara de hoje continuam, menores, em
  cima da malha nova.
- **O fogo do telhado** passa a ler os `fire_points_m` do estado: o fogo sai do lugar quebrado.
- **A morte:**
  - no quadro da morte, o `_Wrecked` vira `_Chunks`, que estão na mesma pose, então a troca não aparece;
  - nos 0,5 s seguintes, o tremor do ZH (±10–15 cm);
  - os pedaços caem cada um no seu atraso, pelo arco balístico com giro (o `FFallPiece` do `ARTSDebrisField` já faz o arco
    From → To);
  - no fim, os pedaços viram o `_Ruin`, que é a mesma pose.
  - O afundar em elevador sai. O `AddRubble` fica só de enchimento miúdo, na cor do prédio.
- **A variante** sai do lado da última marca de tiro (cosmético) ou do hash.
- **Na névoa:** a troca acontece em silêncio, sem efeito (a mesma regra do fogo de hoje).
- **O custo:** 1–2 dias. O risco é médio, porque o ator é de várias sessões.

### (c) O que muda no material (o CODE do `M_Structure`)

- **As pintas saem do prédio.** O `chip` e o `sootD` do bloco `Damage` continuam só no veículo, que já foi pra outro caminho.
- **A rachadura cresce com o dano.**
  - É procedural, no espaço da peça (o GLB não tem UV): borda de Voronoi (F2 − F1), larga o bastante pra ler (5–8 cm ≈ 2 px).
  - Nasce de três lugares:
    1. das quinas das aberturas, em diagonal de 45° (onde a tensão concentra);
    2. das marcas e furos (os dados 12–35);
    3. das bordas das mordidas, por uma "distância até a quebra" que o Blender grava na cor do vértice.
  - O limiar anda com o `Damage`.
- **A fuligem acima das aberturas.** É a pluma em V, que alarga subindo até 2–3 m acima da janela ou do furo por onde o fogo
  respira. É o "V pattern" da perícia de incêndio, NFPA 921
  ([NIST/OSAC](https://www.nist.gov/glossary-term/34271)). Liga com o fogo dos 35%.
- **O vidro quebrado.** O slot de vidro vira buraco escuro com cacos na borda. À noite, com fogo, sai o brilho de dentro, que é
  a luz do próprio fogo simulado. Mais tarde dá pra fingir os cômodos com interior mapping (van Dongen, CGI 2008;
  [80.lv](https://80.lv/articles/interior-mapping-rendering-real-rooms-without-geometry); o `InteriorCubemap` da Unreal).
- **O slot `Broken`:** o miolo claro com ruído, que escurece pra dentro e pega fuligem na borda. A ferragem é ferrugem escura.
- **O custo:** 1–2 dias. O risco é médio: o material é vivo pras outras sessões, então só compila na fila.

### (d) O que muda no efeito (tudo em volume, no Fluxo, na luz do mundo)

- **A poeira do desabamento,** pelo `SpawnDust` e `PushDust` do campo único:
  - **O tamanho:** a nuvem nasce com ~2× a largura do prédio e ~1,25× a altura. É a regra de demolição do modelo de pó da ASI
    ([fonte](https://www.appliedscienceint.com/dust-analysis-service-featured-in-demolition-recycling-international/)): uma
    regra de modelagem, **não uma medida**.
  - **Como anda:** sai como um anel rente ao chão, que corre pra fora (o ar expulso pelo colapso, Bažant 2008,
    [arXiv](https://arxiv.org/pdf/1712.06207)), depois sobe e fica.
  - **A cor:** a do material (adobe ocre, concreto cinza-bege).
  - **A leitura da batalha:** a parte densa fica no máximo ~5 s; depois fica rala por 15–30 s.
- **O aviso:** 0,3–0,6 s antes da queda, sai pó pelas janelas e pelos furos (o ar empurrado pelos andares). É o "aviso" do
  `ESTUDO_INFANTARIA_DESIGN.md`.
- **O pó no impacto:** um puff onde cada pedaço maior que 1 m toca o chão; o atraso e a pose de chegada vêm do JSON.
- **Os pedaços do `StructureBreak`** pegam a cor do prédio (hoje são fixos em 0,40/0,38/0,35).
- **O pó assentado:** o `GroundMemory` ganha uma mancha de pó claro em volta da ruína, além da queimada.
- **O custo:** 1–2 dias. O risco é médio (o orçamento de volume com várias mortes juntas).

### (e) Depois (se ele quiser mais)

- **O pedaço quebra onde o tiro bate, como as células do CoH3:**
  - o `_Damaged` é dividido em 8–12 regiões, cada uma com a versão inteira e a quebrada;
  - quebra a região mais perto da última marca.
- **A queda gravada inteira:** a trajetória do rigid body por pedaço, no lugar do arco, com os choques entre pedaços.

## 3. Os 3 primeiros passos (o maior salto, o menor risco)

1. **O material sem pintas, com a rachadura e a fuligem em V.** Mexe só no shader, atrás de uma chave pro A/B. Muda o prédio
   ferido a partida inteira. 1 dia, risco baixo.
2. **A ruína com a cara do prédio.**
   - O destrutor gera só o `_Ruin` e a tinta dos slots no entulho.
   - O `AddRubble` passa a amontoar no pé das paredes, na cor do prédio e dentro da margem de 3 m.
   - O afundar continua por enquanto, mas desce pra dentro da ruína, coberto pela poeira do desabamento (o truque do Deterrence
     e do ZH).
   - Some o tapete de pedras cinza, que é o que fica na tela pelo resto da partida.
   - 2 dias, risco baixo (só o prédio morto, nada na simulação).
3. **O `_Damaged` e o `_Wrecked` num prédio só** (a War Factory da China, a mais rica), com a troca coberta aos 70% e aos 35%.
   É o maior salto na silhueta e prova a ferramenta antes de passar pros outros. 3–4 dias com a ferramenta, risco médio.

Em seguida vem a queda pelos pedaços (`_Chunks`), que tira o elevador.

**A prova de cada passo:**
- no jogo, em vídeo, ampliado e no zoom do jogo, com a mesma história nos dois lados (a cena `fx_predio_etapas`, A, B, A);
- um prédio na sombra de outro e uma explosão ao lado (a regra da fumaça);
- o custo medido na 1660.

## 4. O que não consegui confirmar

- **ZH:** a geometria de dentro do `_D`/`_E`, porque não abri os W3D.
- **CoH3:** a página original do diário da Relic (erro de certificado; li pela imprensa).
- **CoH 1 e 2:** os painéis por trecho de parede.
- **AoE IV:** os 25% do fogo.
- **Tempest Rising, Iron Harvest e Men of War:** os detalhes técnicos.
- **Cell Fracture:** o operador da extensão nova.
- **A nuvem de pó:** os números são regra de modelo, não medida. Um vídeo de demolição de 2–3 andares, medido quadro a quadro,
  deve confirmar antes de fechar.
