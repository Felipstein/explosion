# Cavar o chão: o prédio pronto abre o buraco de verdade

Estudo de 02/11/2026, sessão Elenco, só de leitura: nada foi mudado, compilado ou aberto na placa. O pedido é o item 22 de
`docs/elenco/TAREFAS.md`. O Felipe: "eu pedi pra cavar o chão, qual o problema pra cavar o chão?". O caso concreto é o Arms Dealer da GLA:
a rampa desce até a boca do túnel e o veículo novo sobe dela.

## 0. Resumo

- **O que se vê no buraco é sempre o modelo do prédio**: a rampa, os muros de arrimo, o fundo e o túnel. O chão do mapa só precisa
  **sair da frente**, em qualquer caminho. Mesmo no rebaixo pelo deslocamento, a areia tem de descer pra baixo da laje da rampa, senão as
  duas superfícies brigam. Então o que falta é um **buraco no chão**. A cratera é outra coisa: nela a própria areia forma a bacia.
- **Recomendado: o buraco pela máscara, num mecanismo só, em todos os presets.**
  - No Nanite (Alta e Ultra, e Baixa e Média se o A/B mandar), a máscara fica ligada porque o material do chão ganha uma saída de
    Pixel Depth Offset zerada. A Unreal só tira a máscara do Landscape Nanite quando ela é a ÚNICA razão do raster programável
    (`LandscapeRender.cpp:4900–4917`). Por isso o conserto de 01/11 compilou e não abriu.
  - No Baixa e no Média, a alternativa é o Landscape comum (`landscape.RenderNanite 0` no preset), onde a máscara sempre funcionou na
    Unreal. Fica a mais barata no A/B.
  - O corte sai no pixel, sem limite de fundura. A cratera aprovada, o caderno do chão e a magnitude ficam como estão.
- **O plano B é o rebaixo no caderno** (o que estava no item 22), se a máscara com o PDO não abrir ou custar mais. Ele exige subir a
  magnitude do mapa inteiro (de ±1 m pra −3,6/+0,9 m) e um canal novo no caderno. Mexe em três sessões, precisa de uma berma no modelo
  pra esconder a borda e **não resolve o Baixa e o Média sozinho**: lá não há tesselação.
- **Antes de tudo, um defeito a achar.** Em 01/11 o buraco também não abriu com o Nanite desligado (a cena `gc_debug` usa `r.Nanite 0`,
  que troca o Landscape pro comum: `RenderUtils.cpp:1478` e `LandscapeSubsystem.cpp:973`). A máscara tem um 2º erro, fora do Nanite.
  Ele bloqueia o caminho recomendado e também o Baixa e o Média do plano B.

## 1. O caso (o Arms Dealer)

Medidas do `Game/Tools/model_gla_arms_dealer.py` (escala `S = 0.79`) e do `Units.json` (`visual.ground_cut_m`, `visual.tunnel_exit`):

| | no modelo | no mundo |
|---|---|---|
| fundo do túnel (`DEEP`) | −3,8 | **−3,00 m** (o mais fundo à vista); a caixa do fundo vai a −3,24 m |
| rampa: da boca (`MOUTH_X`) ao topo (`RAMP_TOP`) | −8 → 4 | 9,48 m subindo 3,16 m: **~18°** |
| muros de arrimo (meio, face de dentro, face de fora) | 5,75 / 5,5 / 6,0 | ±4,54 / ±4,35 / ±4,74 m: **40 cm de muro** |
| parede do fundo do túnel | −11,6 a −11 | −9,16 a −8,69 m |
| recorte de hoje (`ground_cut_m`) | | [−8,93, −4,54, 3,16, 4,54] m = 12,1 × 9,1 m, as bordas no MEIO dos muros |

O limite de hoje no afundar: o carimbo soma com limite de **−90 a +40 cm** (`GroundCrater.usf:159`, `CraterHeightCmAt:805`). O material
desloca no máximo **±100 cm** (`DISP_CODE`: `saturate(0.5 + h / 200.0)`, `look_mf.py:326`, com `DISPMAG` 2 locais = 2 m,
`build_slice.py:1728`). O Arms Dealer pede −3,3 m: **3,7× além** do que o chão desce hoje.

## 2. Como os jogos e a Unreal fazem

### 2.1 Os jogos

- **Zero Hour** (o código, `newcnc/GeneralsGameCode/GeneralsMD`): o prédio CAVA o mapa.
  - `TerrainLogic::flattenTerrain` (`TerrainLogic.cpp:2638`) baixa o mapa de alturas sob a estrutura até a média do lote. Roda quando o
    Dozer ou o Worker começa a obra (`DozerAIUpdate.cpp:1715`, `WorkerAIUpdate.cpp:420`).
  - `createCraterInTerrain` (`:2856`) abre um cone.
  - A regra é **só abaixar, nunca subir** (`W3DTerrainVisual::setRawMapHeight`, `W3DTerrainVisual.cpp:886`).
  - Ele pode porque o terreno é uma malha própria, refeita da grade de alturas na CPU. O túnel da GLA, esse, é um morro.
- **Spring / Beyond All Reason**: o mapa de alturas é da simulação. A parte vista é atualizada quando entra na visão
  (`HeightMapTexture::UnsyncedHeightMapUpdate` sobe o retângulo pra placa). O custo que pesa é refazer o caminho das unidades. Também
  é motor próprio.
- **Battlefield 3** (GDC 2012, Widmark): a malha do terreno é gerada em tempo de jogo da grade de alturas, e a cratera muda a altura. Um
  "destruction depth map" LIMITA a fundura da cratera em volta dos modelos fixos, pra não abrir buraco embaixo deles: o contrário do
  nosso caso.
- **Batman: Arkham Origins e Rise of the Tomb Raider** (já em `docs/CHAO_V3_ESTUDO.md`): a altura numa textura e a tesselação, sempre
  rasa (neve, rastro).
- **Unreal, a comunidade**: cavar raso é uma textura de altura mais o deslocamento (a pegada na areia). Cavar fundo é trocar o Landscape
  por voxels (o Voxel Plugin). A Epic deixa refazer o Nanite do Landscape só no editor, e chama isso de experimental.

A conclusão: quem cava fundo em tempo de jogo é dono do próprio terreno (grade na CPU ou voxels). Com o Landscape Nanite da Unreal sobram
o deslocamento (limitado pela magnitude, com custo no culling) e a máscara (exata, com custo de raster programável).

### 2.2 A Unreal 5.8 (lido no fonte, `C:\Program Files\Epic Games\UE_5.8\Engine`)

**a. O deslocamento Nanite (a tesselação), o que a cratera usa:**
- **A conta.** O vértice tesselado anda `(valor − Center) × Magnitude` ao longo da normal, no espaço LOCAL
  (`NaniteRasterizationCommon.ush`, `EvaluateDomain`, ~566). Com a escala Z 100 do Landscape do WorldForge, 1 local = 1 m de altura e
  0,504 m na horizontal. O padrão da Unreal é magnitude 4 e centro 0,5 (`EngineTypes.h:3444`).
- **O limite e o custo.** A caixa de culling de CADA cluster e de cada pedaço tesselado cresce em `max(Center·M, (1−Center)·M)` nos
  três eixos (`NaniteResources.cpp:791`, `NaniteSceneCommon.ush:40`, `NaniteClusterCulling.usf:169`, `NaniteSplit.usf:146–186`).
  - Hoje: +0,5 m na horizontal e +1 m na altura.
  - Pra −3,6 m: +1,8 m e +3,6 m, no mapa inteiro.
  - A Epic (Matt Oztalay, no fórum): os limites dos clusters inflam pela magnitude, o que dá culling menos eficiente e mais overdraw.
- **A densidade não muda.** A tesselação não depende da magnitude: o fator vem do tamanho da aresta na tela (`NaniteTessellation.ush`,
  `GetTessFactors`; `r.Nanite.DicingRate` 2 px, `NaniteCullRaster.cpp:148`).
  - No zoom do jogo, com 67% de resolução (~5–10 cm por pixel), os triângulos ficam com ~10–20 cm.
  - A malha de base do Landscape tem 50 cm.
- **A armadilha das sombras.** Na faixa do "displacement fade", as sombras vão pro deslocamento MÍNIMO (`ApplyFallbackDisplacement`).
  Com a magnitude funda, seriam −3,6 m. O `DISPFADE` tem de ficar desligado, como hoje.
- **O Lumen não vê o deslocamento**: usa o Landscape comum (`LandscapeRender.cpp`, `GDisableLandscapeNaniteGI`).
- **Medido no jogo (31/10)**: o `DispOn` ligado ou desligado fica dentro do ruído (54,0 e 51,3 contra 52,3 ms,
  `docs/PENDENCIAS_NOITE.md:454`). O custo da tesselação em si (Baixa e Média sem ela) nunca foi medido.

**b. A máscara no Landscape Nanite (o que falhou em 01/11):**
- **Quando a máscara sai.** O proxy Nanite do Landscape tira a máscara SÓ se ela é a única razão do raster programável por pixel
  (`LandscapeRender.cpp:4900–4917`: "Don't change bMasked if it is not the sole factor"). O raster programável por pixel é
  `PixelDepthOffset || bMasked` (`NaniteSceneProxy.h:273`). A tesselação conta como programável no vértice, não no pixel: por isso a
  máscara de 01/11 saiu mesmo com o deslocamento ligado.
- **O PDO segura a máscara.** Com um Pixel Depth Offset no material, a máscara fica. O PDO tem de sair de um nó Custom: o tradutor
  ignora o PDO constante 0 (`HLSLMaterialTranslator.cpp:1711`). Com a tesselação, o PDO nem é aplicado ("PDO with tessellation would
  screw up barycentrics", `EvaluatePixel`), mas a máscara é avaliada no pixel.
- **A borda com a tesselação.** O vértice tesselado mascarado também é descartado (`EvaluateDomain`, ~596): no Alta e no Ultra o buraco
  cresce até uma aresta tesselada (~10–20 cm) pra fora.
- **O preço.** A Epic recomenda evitar máscara no Nanite pelo custo: o sombreamento refaz as baricêntricas por pixel (o blog do Nanite
  no Fortnite). No chão inteiro, é o maior risco deste caminho, e por isso é o primeiro A/B.
- **O WPO não existe aqui**: o `ULandscapeNaniteComponent` força `bEvaluateWorldPositionOffset = false`
  (`LandscapeNaniteComponent.cpp:115,151`).

**c. A camada de visibilidade (o buraco pintado):**
- É um weightmap. A malha Nanite é montada SEM os quadrados furados (`LandscapeEdit.cpp:4280`, `VisThreshold` 170), no editor
  (`InitializeForLandscapeAsync`, `WITH_EDITOR`).
- O material de buraco do pedaço também é escolhido no editor (`GetCombinationMaterial`, `LandscapeEdit.cpp:583`).
- **Serve só pra prédio que já nasce no mapa** (uma missão). Pro prédio que o jogador põe, não.

**d. A malha do buraco desenhada por cima com profundidade e stencil:**
- Em motor próprio (ou Unity), um furo pelo stencil funciona. Na Unreal o material opaco do chão não lê o stencil no passe base. O
  Nanite desenha num buffer de visibilidade, e o custom depth vem depois.
- Uma "janela" translúcida sem teste de profundidade perde a sombra virtual, o Lumen e o vetor de movimento do TSR, e briga com as
  unidades na frente.
- **Não, sem mexer no renderer.**

**e. Editar o heightmap em tempo de jogo:**
- `LandscapeEditLayers.cpp` (do 535 ao 9607), `LandscapeEdit.cpp` e `LandscapeEditInterface.cpp` são todos `WITH_EDITOR`. O
  `LandscapePatch` aplica pelas camadas de edição. Refazer o Nanite depois é só no editor.
- No Landscape comum, o vértice lê a textura de altura (`LandscapeVertexFactory.ush:448,673`). Escrever nela à força mexeria o
  desenho, mas sem API, sem colisão e sem o Nanite.
- **Não, no jogo empacotado.**

**f. O Landscape comum como saída pro Baixa e o Média:**
- **A chave.** O `landscape.RenderNanite` é uma chave de escalabilidade (`Landscape.cpp:273`, `ECVF_Scalability`). Ao mudar, o
  subsistema refaz o jeito de desenhar de cada pedaço na hora (`LandscapeSubsystem.cpp:434`, `UpdateRenderingMethod`, `Landscape.cpp:7961`).
- **Os dois dados já estão lá.** A Epic: "both Nanite and non-Nanite data are necessary at runtime", então não há o que montar a mais.
- **A máscara funciona.** No comum ela vale sempre: o override de blend do material de combinação só mexe nos pedaços com buraco
  pintado (`LandscapeEdit.cpp:589`).
- **Os riscos.** O LOD do Landscape comum é por distância e a sombra virtual se refaz quando o LOD muda
  (`landscape.NonNaniteVirtualShadowMapInvalidation*`). Precisa de A/B de custo e de olho.

**g. Outras saídas, descartadas:**
- **Trocar o Landscape por voxels** (o Voxel Plugin) ou pelo Virtual Heightfield Mesh (experimental): refazer o chão inteiro, e o visual
  aprovado vai junto.
- **O prédio que levanta em vez de cavar** (o túnel do ZH): o Felipe recusou.
- **O Landscape comum em todos os presets**: perde a cratera aprovada (sem tesselação).
- **Um detalhe útil**: a grama do Landscape tem `ALandscapeProxy::AddExclusionBox` (`LandscapeProxy.h:1183`) pra tirar os tufos de um
  lote em tempo de jogo, se um mapa usar grama do Landscape. O `ClearScatterUnder` já limpa as pedras e moitas da montagem.

**h. Uma porta pra otimizar depois:**
- A malha Nanite do chão tem uma casa de material por pedaço (256; o `MakeLandscapeDynamic`, `RTSGroundMemory.cpp:624–670`, já troca
  as casas em tempo de jogo).
- A instância dinâmica (MID) NÃO muda o modo de blend nem a magnitude (`MaterialInstanceDynamic.h:207`: "MIDs cannot override these").
  Uma variante só nos pedaços com buraco pede **instâncias gêmeas** montadas pelo WorldForge.
- Isso põe o custo só ali (do PDO, ou da magnitude funda no plano B). Fica pra quando o A/B global pedir.

## 3. Os caminhos pro nosso caso

| | **C1: a máscara (PDO no Nanite)** — recomendado | **C2: o rebaixo no caderno** (o item 22) — plano B | C3: o rebaixo com tesselação em todo preset |
|---|---|---|---|
| Como | o chão some no retângulo do prédio pronto (o `ground_cut` que já existe); o PDO zerado mantém a máscara no Nanite | o prédio carimba o fundo do lote num canal novo do caderno (por mínimo, sem o limite de −90); a magnitude vai a −3,6/+0,9 m | o C2, com `r.Nanite.Tessellation 1` também no Baixa e no Média (só o lote afunda; a cratera segue desligada) |
| Alta/Ultra | o buraco no pixel (a borda cresce até ~20 cm pela tesselação: o retângulo vai na face de DENTRO dos muros) | o chão desce sob o modelo; a borda é uma rampa de alguns triângulos de 10–20 cm, que tem de caber nos 40 cm de muro | igual ao C2 |
| Baixa/Média | o mesmo material: no Nanite sem tesselação (máscara exata) ou no Landscape comum (`landscape.RenderNanite 0`), o mais barato no A/B | **não funciona**: não há tesselação; vai junto o C1 no Landscape comum | funciona, mas a 50–58% de resolução a aresta tesselada chega perto dos 50 cm da malha: a queda de 3,3 m escapa do muro sem uma berma de ~1 m |
| O que pesa na placa | o chão inteiro no raster programável por pixel (a Epic: evitar máscara no Nanite). **Não medido.** Se pesar, as gêmeas só nos pedaços do lote (§2.2h) | a caixa de culling de todos os clusters do chão +1,8 m/+3,6 m; +16 MB por caderno se a cratera virar RGBA16F (2048², 3 cadernos). **Não medido** | a tesselação do mapa inteiro nos presets fracos. **Não medido** (o ini diz "custa mesmo com o deslocamento zerado") |
| Fundura | qualquer uma | até a magnitude (−3,6 m); cada metro a mais pesa no mapa inteiro | igual ao C2 |
| A borda | exata no Baixa e no Média; ~20 cm no Alta | precisa de muro grosso ou berma (a terra tirada do buraco, amontoada em volta, como numa obra de verdade) | precisa de berma |
| Quem mexe | o material (`ground_cut.py`, a montagem do WorldForge) e uma linha no `VisualGroundZCm` | o Look (`DISP_CODE`), o WorldForge (magnitude e centro, remontar os mapas), o jogo/Look (o caderno, o compute, a CPU) e eu | igual ao C2 + o ini |
| Risco | o PDO depender de um detalhe do motor (comentado de propósito no fonte, mas pode mudar numa versão) | a borda vazando pra fora do muro; o custo no mapa inteiro | o custo nos presets fracos |

Em todos: o Lumen enxerga o chão inteiro sobre o buraco, então o lado do buraco na sombra pode sair mais escuro que o real. É o mesmo da
cratera aprovada, mas a 3 m de fundo é para conferir. Os traçados de colisão (`BlastFxGround`) também batem no chão de antes.

## 4. A recomendação: C1, com a ordem e os portões

**Por que o C1:**
- É o único que serve os quatro presets com o mesmo resultado e a mesma peça de código.
- Corta no pixel e em qualquer fundura (outros prédios virão: bunker, silo, a Rússia).
- Não toca no caderno da sessão do chão nem na cratera aprovada.
- E é o que já está quase pronto: a coleção, o `RTSGroundCut::Push`, o liga e desliga do prédio.

O C2 só vence se o PDO não segurar a máscara ou se ela custar mais que a magnitude funda no Alta.

**A ordem (cada passo com prova antes do próximo):**
1. **Achar o 2º erro da máscara** (sem ele nada da máscara anda, nem no plano B). Ver §6, T0.
2. **O PDO**: o buraco abre no Nanite? No Alta (com tesselação) e no Média (sem).
3. **O A/B do custo** (§6, T2):
   - Alta: hoje × máscara+PDO.
   - Média: hoje × máscara+PDO no Nanite × Landscape comum com a máscara.
   - O portão: se a máscara+PDO custar mais que o rebaixo, ou pesar no Alta, as gêmeas só nos pedaços do lote. Se nem assim, o C2 no
     Alta e no Ultra.
4. **A prova ampliada** nos quatro presets e o vídeo do veículo subindo (§6, T3–T4).
5. **O Felipe vê.** Commit só com o aprovado dele.

## 5. O passo a passo dos arquivos

### C1 (o recomendado)

**Meus (os prédios):**
1. `Game/Tools/ground_cut.py`: no `add_ground_cut`, um Custom `return 0.0;` (descrição `GroundCutPDO`) ligado ao
   `MP_PIXEL_DEPTH_OFFSET`, e o `GROUND_CUT_UNDO=1` tirando os dois. `MAX` de 8 pra 16 (vários GLA com mais de um Arms Dealer); o
   `CODE` fica.
2. `Game/Source/RTSGame/Private/RTSUnitActor.cpp` (`namespace RTSGroundCut`):
   - `MaxCuts` 16;
   - a consulta do fundo do lote, `GetGroundCutFloorCm(P)`, pelo perfil da rampa que já está no `tunnel_exit.path_m`, pros destroços e
     pedras que caem no buraco da ruína.
3. `Game/Tools/model_gla_arms_dealer.py` e `Game/Tools/elenco_units.py`:
   - o retângulo nas faces de DENTRO dos muros e da parede do túnel, calculado das constantes (`PIT_X·S`, `BAY·S`, `RAMP_TOP·S`), não à
     mão: ~[−8,69, −4,35, 3,16, 4,35];
   - o `Units.json` pelo `units_json_io.save_units`.
4. `Game/Tools/model_damage.py`: conferir que a ruína do Arms Dealer guarda a rampa e os muros (o recorte segue nela,
   `UpdateGroundCut`).

**Da sessão do chão (a interface, combinada com ela):**
5. `WorldForge/Tools/build_slice.py`:
   - montar com `GROUND_CUT=1` (o mestre recebe a máscara e o PDO do `ground_cut.py`; nada mais no material muda);
   - remontar o `World_TestSalt2` e os outros mapas do `Maps.json`.
   - A interface: "o mestre do chão é mascarado, lê `Cut0..15`, `Rot0..15` e `CutCount` da `MPC_Ground`, e tem o PDO zerado".
6. `Game/Config/DefaultScalability.ini`: só se o A/B do Média escolher o comum, `landscape.RenderNanite=0` em `[ShadingQuality@0..2]`
   (e `=1` no 3 e no Cine).
7. `Game/Source/RTSGame/Private/RTSMatchSubsystem.cpp`, `VisualGroundZCm` e o `VisH` do `SeatOnCraters`: `min(…, o fundo do lote)`
   dentro de um recorte. Uma linha cada.

Nada muda no `RTSGroundMemory`, no `GroundCrater.usf` nem no `look_mf.py`.

### C2 (o plano B, se o portão mandar)

**Da sessão do chão:**
- `Game/Source/RTSGame/Private/RTSGroundMemory.cpp/.h`:
  - a lista de lotes e a interface `SetLotCut(Id, Transform, RectCm, FloorCm)` / `ClearLotCut(Id)`;
  - o canal B da cratera (`PF_G16R16F` → RGBA16F, `MakeCraterTarget:369`) com o fundo do lote (+1000 = sem lote), composto por MÍNIMO e
    sem o limite;
  - carimbado depois das crateras no `Redraw`;
  - o `CraterHeightCmAt` com o `min` dos lotes (o −90 fica só pras crateras).
- `Game/Shaders/Private/GroundCrater.usf`: um `LotCS` (mínimo). O `HeightCS` escreve só R e G.
  - Sem isso, a cratera nova perto do lote prende o buraco de volta a −90 cm pelo `clamp`.
- `Game/Tools/look_mf.py` (`DISP_CODE`): `h = min(h, lot)` e `return saturate(C + h / M)`, com C e M vindos da montagem.
- `WorldForge/Tools/build_slice.py`: `DISPMAG` 4,5 e um `DISPCENTER` 0,8 (−3,6 a +0,9 m); remontar os mapas. Nunca o `DISPFADE` com
  essa magnitude (§2.2a).

**Meus:**
- o prédio chama o `SetLotCut` no pronto e o `ClearLotCut` na venda e no `EndPlay`;
- o fundo = o perfil do modelo menos 30 cm;
- a berma de ~1 m em volta do corte no modelo (a terra do buraco), com a borda do rebaixo por baixo dela.

O Baixa e o Média seguem o C1 no Landscape comum (os passos 1, 5 e 6 acima, sem o PDO).

## 6. Como testar o antes e o depois

Na fila da placa (`captures/.gpu_lock`, `docs/GPU.md`). As cenas existem: `gc_debug` (o pátio de perto) e `elenco_gla_v4` (o veículo
subindo).

- **T0, o 2º erro:**
  - No `gc_debug`, trocar o `r.Nanite 0` por `landscape.RenderNanite 0` (só o chão vira comum; o `r.Nanite 0` derruba também os prédios
    pro fallback).
  - Primeiro com o `CODE` do recorte trocado por `return 0.0;`: o chão INTEIRO tem de sumir.
  - Se não sumir, o modo mascarado não está chegando ao desenho. Os suspeitos:
    - as instâncias de combinação do mapa, com o blend sobrescrito;
    - as MIDs do `MakeLandscapeDynamic`;
    - o mestre que levou a máscara não ser o do mapa (a linha `GROUNDCUT pus em` do log);
    - `viewmode shadercomplexity` mostra se o chão está mascarado.
  - Depois, com o retângulo de verdade.
- **T1, o PDO:** a mesma cena, Nanite ligado, no Alta e no Média. As fotos alto, baixo e lado, como as de 01/11.
- **T2, o custo** (o método de 31/10):
  - 4 rodadas de 55 s alternadas (A, B, A, B), sem `-Record` (ele trava em 30 qps), na câmera do jogo sobre a base com o Arms Dealer;
  - a mediana e o p95 do quadro pelos logs;
  - no Alta: hoje × máscara+PDO;
  - no Média: hoje × máscara+PDO × Landscape comum com a máscara;
  - no C2: `DISPMAG` 2 × 4,5/0,8.
- **T3, a prova ampliada** (a regra dele), nos quatro presets, no zoom do jogo e ampliado:
  - a borda nos muros: sem fresta pra fora, sem a beirinha de areia por cima do buraco;
  - o sol entrando no buraco e a sombra dos muros na rampa;
  - o lado na sombra não preto (o Lumen);
  - ao lado de uma explosão: a luz no buraco;
  - a troca de preset no meio da partida (Baixa → Ultra): o buraco fica.
- **T4, em movimento:** o `elenco_gla_v4` com `-Record`: o veículo sobe do túnel à vista, sem a silhueta. Depois a venda (o chão
  fecha) e a destruição (a ruína guarda o buraco, os destroços caem dentro).
- **Antes e depois lado a lado**, como imagem na conversa: o pátio de hoje (a areia por cima da rampa) e o novo, na mesma câmera.

## 7. Armadilhas anotadas

- **O PDO** tem de vir de um nó Custom: constante 0 não conta (`HLSLMaterialTranslator.cpp:1711`).
- **A máscara do mundo:** `WorldSettings.NaniteSettings.bAllowMaskedMaterials` tem de ficar ligado. Desligado, o
  `AuditNaniteMaterials` (`Landscape.cpp:7888`) reprova o material e o chão cai pro comum em todos os presets, calado.
- **As instâncias dinâmicas:** não mudam blend nem magnitude. Variante local só por instância gêmea montada no editor.
- **A borda com a tesselação:** a máscara descarta o vértice tesselado inteiro. Por isso o retângulo na face de dentro do muro, não no
  meio.
- **O decal dos tipos de chão** (`M_GroundTypes`) pode projetar na rampa: conferir o `bReceivesDecals` do prédio no T3.
- **O limite de cortes:** com 8, o 9º não abre (o `Push` só manda os 8 primeiros). Por isso 16.
- **O Lumen e a colisão** veem o chão fechado sobre o buraco (§3). Fica anotado, não bloqueia.

## 8. O T0 (04/11): o C1 ABRE, com e sem o Nanite, por ~+0,5 ms

O teste no mestre do World_TestHills (um agente, na vez da placa; o mestre voltou ao original com o hash idêntico, conferido):
- **o recorte zerado com o PDO** (o Pixel Depth Offset ligado num MakeMaterialAttributes) tira o chão inteiro com o Nanite ligado,
  desligado e religado; **sem o PDO**, com o Nanite o chão fica (a Unreal tira a máscara, como o estudo leu no fonte) e sem o Nanite
  some: o PDO é o que segura a máscara no Nanite;
- **o "2º erro" de 01 e 02/11 (o buraco que não abria sem o Nanite) era a foto cedo demais**: o shader do raster mascarado leva de 2 a
  45 s pra compilar na 1ª vez (aos 2 s o chão ainda está fechado; aos 45 s, aberto; depois fica no cache);
- **o recorte de verdade do Arms Dealer**: a rampa desce até a boca do túnel, com o sol e a sombra dos muros (a prova:
  `docs/aprovados/modelos/2026-11-04_recorte_do_chao_T0_mascara_pdo_PROVA.jpg`; as folhas em `captures/game/t0p_corte/`);
- **o chão longe do corte não muda** (0,85/255 de diferença média, o mesmo ruído de duas rodadas do original: 0,96);
- **o custo**: +0,3 a +0,6 ms (~2%) no Alta 1600×900, a câmera do jogo a 80 m (a 2ª rodada, A B A B de 90 s: 26,2/26,4 → 26,5/27,0 ms;
  o p95 27,6/28,0 → 27,8/28,6).
Os scripts: `docs/elenco/cavar_t0/` (o `t0_pdo.py` muda e desfaz o mestre; as cenas `t0p_zero`, `t0p_corte`, `t0p_custo`,
`t0p_custo_longo` no Scenarios.json). **Falta:** o T1 num mestre com tesselação (o World_TestSalt2, o deslocamento das crateras: o
buraco e o custo), o T2 do Média e o T3 ampliado nos 4 presets; no jogo, o PDO junto da máscara no `ground_cut.py` e na montagem do
WorldForge (uma função de uma linha no `WorldForgeEditorLib`, igual à `ConnectMaterialDisplacement`; o WorldForge é de outra sessão).

## Fontes

- Fonte da Unreal 5.8, os arquivos e as linhas acima (`Runtime/Landscape`, `Runtime/Engine/Private/Rendering/NaniteResources.cpp`,
  `Shaders/Private/Nanite/*`, `Runtime/RenderCore/Private/RenderUtils.cpp`).
- Epic, "Using Nanite with Landscapes": https://dev.epicgames.com/documentation/en-us/unreal-engine/using-nanite-with-landscapes-in-unreal-engine
- Epic (fórum, Matt Oztalay), a magnitude inflando os limites dos clusters:
  https://forums.unrealengine.com/t/question-about-nanite-tessellation-being-core-to-asset-creation-workflow/2641356
- Epic, "Bringing Nanite to Fortnite Battle Royale in Chapter 4" (o custo da máscara no Nanite):
  https://www.unrealengine.com/tech-blog/bringing-nanite-to-fortnite-battle-royale-in-chapter-4
- Epic (fórum), cavar com textura de altura e deslocamento:
  https://forums.unrealengine.com/t/how-can-i-use-displacement-in-5-4-to-add-a-digging-effect-in-the-landscape/1946906/2
- Widmark, "Terrain in Battlefield 3", GDC 2012: https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/gdc12-terrain-in-battlefield3.pdf
- Spring, `HeightMapTexture::UnsyncedHeightMapUpdate`: https://sources.debian.org/src/spring/98.0%2Bdfsg-1/rts/Map/HeightMapTexture.cpp ;
  a deformação e a visão: https://springrts.com/mantis/view.php?id=3135
- Zero Hour, o código liberado (cópia local `C:\Users\luisf\newcnc\GeneralsGameCode`): `TerrainLogic.cpp`, `W3DTerrainVisual.cpp`,
  `DozerAIUpdate.cpp`.
- No projeto: `docs/CHAO_DEFORMAVEL.md` (a escolha do deslocamento, 13/10), `docs/PENDENCIAS_NOITE.md:423–457` (a magnitude local e o
  custo do `DispOn`), `docs/BASE_JOGAVEL.md:445–473` (o recorte de 01/11), `docs/APROVADOS.md` (a cratera de 31/10).
