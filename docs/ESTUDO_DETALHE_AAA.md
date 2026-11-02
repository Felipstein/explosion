# Estudo: o detalhe AAA que lê na câmera do RTS (31/10/2026)

Pedido do Felipe sobre os provisórios por script: "isso não é um jogo de lego [...] TEXTURIZADO, DETALHADO [...] nada de quinas
quadradas". Este estudo diz **o que faz um modelo parecer AAA na nossa câmera**, o que é desperdício, e como pôr isso no pipeline
por script (Blender sem janela → GLB → import por Python → o material mestre do `import_provisional.py`). Nada aqui foi
implementado nem medido na placa; os custos são estimativas. † = só pelo trecho do buscador (não abri a página).

**Resumo:**
- Na câmera padrão, **1 px vale 3 a 5 cm** (e ~2–3 cm deitada). Forma menor que ~6–10 cm não existe como forma: vira tom, brilho
  ou sombra. Por isso o que tira a cara de "lego" **não é parafuso**: é **sombra de contato entre as peças (AO), sujeira nas
  frestas, quina arredondada que pega luz e desgaste nas quinas**. Hoje o modelo não tem nenhuma das quatro.
- O caminho barato e compatível: **cozer quina, cavidade e AO na cor de vértice do GLB** (branco = neutro, os modelos antigos não
  mudam) e ler isso no HLSL do material mestre. Depois, a esteira que corre, a pintura no material (não em blocos de geometria) e
  um atlas pequeno de decais.

## 0. A régua: quanto vale um pixel na nossa câmera

- A câmera padrão (37,5°, 50° de campo, 61 m de altura: `docs/concept/scenes/README.md`) fica a ~67–100 m do alvo: em 1080p dá
  **3,3 cm/px** no centro (a conta do comentário do `import_provisional.py`, a 67 m) a ~5 cm/px longe. O tanque tem ~150 px na foto
  do jogo e ~250 px deitado (`docs/CONCEITO_UNIDADES.md` §2 item 4: um elo ~2 px, uma roda ~15 px).
- Pela amostragem (2 px no mínimo pra uma forma existir), **detalhe abaixo de ~6–10 cm some** na câmera padrão e abaixo de ~5 cm
  deitada. Ele só aparece somado: uma fileira de parafusos vira um "ritmo" escuro; mil arranhões viram uma rugosidade mais clara.
- **Texel:** com 3 cm/px, 64–128 px/m de textura já sobram (2× a tela, pela regra do mip). Isso é **um atlas de 1024² por tanque**
  (~100 m² de superfície a 100 px/m). 2k–4k por unidade é desperdício: o mip mais alto nunca carrega
  ([Groenewoud](https://stefangroenewoud.artstation.com/blog?page=2)†; a faixa "top-down ≤ 2,56 px/cm" da
  [Maxon](https://help.maxon.net/c4d/2025/en-us/Content/html/Texel_Density.html)† é genérica; pra RTS não achei número publicado).

## 1. As técnicas, por impacto na NOSSA câmera ÷ custo

| # | Técnica | O que dá na nossa câmera | Custo | Fonte | Já temos? |
|---|---|---|---|---|---|
| 1 | **AO e cavidade cozidos** (contato entre peças, frestas, caixa de roda, anel da torre) | As peças "sentam" umas nas outras; o volume lê de cima. O maior ganho por real gasto | Cozer: segundos/modelo. Jogo: 1 leitura de cor de vértice | [Nanite: "supports multiple UVs and vertex colors"](https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-virtualized-geometry-in-unreal-engine); [Dirty Vertex Colors](https://docs.blender.org/manual/nb/dev/sculpt_paint/vertex_paint/editing.html)†; [Curvature-Cache (curvatura na cor de vértice, hard surface com bevel)](https://github.com/hubertekvall/Curvature-Cache)† | Não |
| 2 | **Chanfro arredondado de verdade** (segmentos + normal ponderada pela área) | A quina pega o sol numa linha de 1–2 px: some o "lego" sem textura nenhuma | +30–80% de triângulos; Nanite absorve | [Weighted Normal (Face Area)](https://docs.blender.org/manual/en/5.1/modeling/modifiers/normals/weighted_normal.html); chanfro pequeno + FWN sem normal map ([Polycount](https://polycount.com/discussion/comment/2425349)†) | Parcial: 3 cm, 1–2 segmentos; cilindros e peças pequenas sem chanfro |
| 3 | **Shader de camadas: desgaste de quina + pó + sujeira de cavidade** pelas máscaras | A pintura gasta clareia a quina; pó nas frestas e no que olha pra cima: o "veículo de campanha" das fichas | ~150–250 instruções nos pixels das unidades | CoH3 trocou o desgaste "cozido no veículo" por um "complex vehicle shader" com sujeira do ambiente (Bourdage, [NME](https://www.nme.com/news/company-of-heroes-3-tanks-will-show-accurate-dirt-and-damage-3210943)); WoT: um material-base pra 99% dos casos, com sujeira e molhado dinâmicos ([80.lv](https://80.lv/articles/world-of-tanks-graphical-update-technical-overview/)); quina pela máscara + ruído no espaço do mundo, ~30 instruções, serrilha de longe ([Polycount](https://polycount.com/discussion/comment/2641624)†) | Parcial: pó em cima, terra no pé, manchas, lascas de dano. Falta quina e cavidade |
| 4 | **Pintura no material, não em geometria** (camuflagem digital, faixas, rabisco do GLA) | A camuflagem da China hoje é **milhares de blocos de 0,16 m colados** (`camo_poly`): é literalmente lego. No material ela desbota, leva pó e desgaste por cima | Tira triângulos; +1 ruído quantizado no shader | Total War: cor da facção por máscara de textura, não por malha ([TW wiki](https://wiki.totalwar.com/w/Unit_Part_folder_structure_documentation.html) só confirma diffuse/normal/gloss por peça; a máscara vem de modders†) | Não |
| 5 | **Esteira que corre + roda/pneu legíveis** | Hoje as rodas giram (`RTSUnitActor.cpp:264`) e **a esteira fica parada**: o tanque desliza. Pneu preto liso = brinquedo | Grátis (UV andando) | O padrão: textura do elo correndo na faixa, sincronizada com a roda e a velocidade ([three.js](https://discourse.threejs.org/t/tank-controller-track-system-in-three-js-any-implementations/91233)†; o BF1942 já ligava a velocidade da textura ao movimento: [doc](https://realtimerendering.com/erich/bf1942/mdt/MDTDOC/Confiles/ObjectTemplate/Properties/setAnimatedTextureSpeed%20.htm)†); o WoT 1.0 passou a mover elo a elo ([PC Gamer](https://www.pcgamer.com/world-of-tanks-is-getting-serious-about-graphics-and-its-great)†) | Não (elos de geometria parados) |
| 6 | **Quebra de material**: tom por placa, rugosidade variada, desbotado no teto | Placa a placa a cor muda 3–8%: lê como "chapa de verdade", não plástico | Barato (semente por peça no UV1) | Fórum do AoE IV: "por que as unidades parecem de plástico ou massinha" (cor do time grande e sem textura†: [link](https://forums.ageofempires.com/t/why-do-units-look-like-they-are-made-of-plastic-or-clay/135717?page=2)) | Só no prédio grande (>9 m, `Weather`) |
| 7 | **Decais de malha + atlas pequeno** (estêncil, número, fileira de rebite, solda, grade, perigo) | O "texto" da superfície que a ficha tem; lê na câmera deitada | Médio: UV + atlas 1k gerado por script; 1 slot a mais | Trim de faixas com chanfro de 45° no normal (Sunset Overdrive, [GDC 2015](https://gdcvault.com/play/1022323/The-Ultimate-Trim-Texturing-Techniques)†); decais de malha no Blender exportando atlas/trim ([DECALmachine](https://machin3.io/DECALmachine/docs)†) | Não (só estrela e faixa de perigo procedural) |
| 8 | **Peças de detalhe na escala do RTS** (alça, cabo, dobradiça, ferramenta, galão, saco arredondado, lona com dobra) | Silhueta e sombra miúda; exageradas pra ter ≥ 2 px | Triângulos, Nanite absorve | SC2: personagens "bigger and over-proportioned" (Samwise, em `ESTUDO_INFANTARIA_VISUAL.md`) | Parcial (o kit da China tem `pillow`, `tarp_fold`, `hatch`) |
| 9 | **Antisserrilhado do brilho** (Normal Curvature to Roughness) | A quina fina não pisca com a TSR quando o tanque anda | Quase grátis | [Kaplanyan, GDC 2016](https://developer.download.nvidia.com/gameworks/events/GDC2016/akaplanyan_specular_aa.pdf)† | Não |
| 10 | Normal fina triplanar (fundido, casca de laranja, reboco) | Só deitada e de perto; no padrão vira pisca-pisca | 3 amostras | — | Só no concreto (`ConcN`) |

**Desperdício na nossa câmera:** parafuso isolado < 4–5 cm (só em anel/fileira, onde vira ritmo); grade de malha fina de verdade
(barra < 4 cm vira moiré com a TSR: barra ≥ 4 cm com vão ≥ 15 cm, ou chapa com o padrão escuro no material); textura de 2k–4k por
unidade; interior modelado (basta o gradiente escuro do vão); normal map de micro detalhe em veículo (troque por rugosidade);
curvatura em tela pela derivada da normal ([Evan Wallace](https://madebyevan.com/shaders/curvature)†: depende da malha e some de
perto); material **mascarado** pra vazado no Nanite (a Epic recomenda evitar:
[fórum](https://forums.unrealengine.com/t/nanite-material-level-of-detail-performance-masked-materials/787465)†); e, regra de
08/10, **remesh/suavizar/deformar com ruído** ("massa de modelar").

## 2. O plano técnico

### 2.1 `model_lib.py`: tudo opcional (outros agentes usam a biblioteca; o modelo antigo exporta igual)

`Part(name, detail=True)` e `export(..., masks=True)` ligam o novo; sem isso, nada muda.

1. **Chanfro** (`harden`): placa de blindagem 4–6 cm (1–2 px), prédio 8–15 cm, peça pequena 1–2 cm; **3 segmentos com perfil
   0,5 (redondo)** onde hoje são 1–2; cilindros com chanfro na tampa (hoje 0,0–0,01); `harden_normals` + Weighted Normal
   (Face Area, `keep_sharp`) como já está. A quina que é viva na ficha continua viva: o chanfro sai do contorno medido.
2. **Grade plana nas faces grandes** pra cor de vértice ter resolução: subdividir só faces **planas** > 0,25 m² em células de
   ~12 cm (veículo) / 30–40 cm (prédio). A forma fica idêntica (sem remesh, sem suavizar). Tanque: ~100 m² → +15–25 mil
   triângulos; War Factory: +60–120 mil.
3. **As máscaras na cor de vértice** (atributo `Col`, FLOAT_COLOR, por canto; exportar com `export_vertex_color='ACTIVE'`,
   `export_all_vertex_colors=False`, como o `missile_models.py:140`). **Branco = neutro**, então malha sem cor (os antigos) não muda:
   - **R = 1 − quina convexa**: ângulo assinado das arestas no bmesh (`edge.calc_face_angle_signed()` > ~20°), por vértice,
     1–2 passadas de suavização. Com o chanfro de 3 segmentos a máscara cobre exatamente a faixa do chanfro.
   - **G = AO**: `mathutils.bvhtree` com **todas as peças juntas** (casco, torre, rodas, esteira) **mais um plano de chão em z=0**
     (a barriga e a caixa de roda escurecem: a sombra de contato), 48–64 raios cosseno, alcance 1 m no veículo e 3 m no prédio.
     Sem Cycles e determinístico. (O bake do Cycles pra cor de vértice existe mas pede UV:
     [manual](https://docs.blender.org/manual/vi/5.2/render/cycles/baking.html)†.)
   - **B = 1 − cavidade**: aresta côncava (< −20°) somada a um AO curto (0,15 m): as frestas onde o pó assenta.
   - **Alfa: não usar.** "O alfa da cor do vértice não chega no material" (`soldier_dress.py:277`).
4. **UV0 em metros** (projeção plana por face, a caixa de cada primitiva) e **UV1.x = semente aleatória da peça**;
   `export_texcoords=True`. Serve pro tom por placa, a semente da camuflagem e os decais. A Nanite aceita UVs múltiplos.
5. **Peças novas** (funções do lib; os tamanhos já na escala de 2 px):
   - `bolt_ring` (porca 5–6 cm, só em aro/flange/cubo, nunca solta);
   - `weld_bead` (cordão achatado de 3–4 cm só nas juntas grandes);
   - `panel_seam` (sulco de 3 × 2 cm cortado na chapa: a cavidade escurece sozinha);
   - `hinge`, `handle`, `tool` (pá, picareta, macaco presos no para-lama);
   - `cable` (catenária, tubo de 8 lados, raio ≥ 2 cm);
   - `grille_open` (barra ≥ 4 cm com o fundo escuro);
   - `tarp_draped` (o `sheet` com dobras ancoradas nas cordas, não ruído);
   - `sandbag` (o `pillow` do kit da China promovido pro lib);
   - `tyre` (perfil girado com o flanco abaulado, banda com blocos de 5–8 cm, aro rebaixado, `bolt_ring` no cubo);
   - `track_band` (§2.3).
6. **Saída:** cada modelo imprime triângulos antes/depois e o tempo do cozimento; `model_preview.py` ganha a vista "máscaras"
   (R, G, B) pra conferir sem a Unreal.

### 2.2 O HLSL do material mestre (`import_provisional.py`, o CODE)

O CODE é lido também pelo `M_UnitAtlas` (`import_gla.py:47`), que **exige a linha `float foot = ...` intacta**: não mexer nela.
Entradas novas: `VC` (VertexColor), `UV0`, `UV1`; saída nova `AO` → `MP_AMBIENT_OCCLUSION` (o `look_lab.py:588` já faz isso).
Parâmetros novos por instância: `Wear`, `CavityDirt`, `PanelTone`, `CamoA/CamoB` (cores), `CamoPix`; todos com padrão 0 (neutro).
Inserir depois do pó/terra no pé e antes do dano:

```hlsl
float edge = 1.0 - VC.r;  float ao = VC.g;  float cav = 1.0 - VC.b;     // branco = neutro (malha sem cor de vértice)
// tom por placa (a semente da peça no UV1): chapa de verdade, não plástico
base *= 1.0 + PanelTone * (frac(UV1.x * 91.7) - 0.5) * 0.12;
// pó e areia nas frestas (a cor da duna do mapa, a mesma do pé)
base = lerp(base, float3(0.40, 0.31, 0.21), saturate(cav * CavityDirt * (0.6 + 0.6 * stain)));
// desgaste de quina: a tinta gasta clareia e o aço aparece; recortado por ruído de ~6 cm pra não virar contorno de desenho
float wear = smoothstep(0.35, 0.65, edge * (0.5 + Nz.N(q / 0.06 + 3.1))) * Wear;
base = lerp(base, min(base * 1.6 + 0.04, 0.6), wear * 0.7);
Rough = lerp(Rough, 0.42, wear * 0.6);
AOut  = lerp(1.0, ao, 0.9);                       // o pino AO: só a luz indireta e do céu (Lumen), a direta fica pra sombra
```

- **O AO vai no pino de AO, não na cor** (regra "causa física, não cor"): o sol nas frestas já é sombra real (VSM); o pino escurece
  só a luz do céu e a indireta, que o Lumen por software não resolve abaixo de ~10 cm. Se na câmera não ler, aí sim 20–30% na cor.
- **Camuflagem no material:** `floor(UV0 / CamoPix)` com o `Nz.N` no espaço da peça (as manchas continuam de face pra face, como no
  `camo_value` do kit), desenhada **antes** do pó e do desgaste. Tira o `camo_poly` dos modelos.
- **Antisserrilhado:** ligar Normal Curvature to Roughness no mestre (conferir o nome da propriedade na API Python).
- **Normal fina** (fundido/reboco): opcional pelo parâmetro, triplanar como o `ConcN`; só vale deitada.
- **Calibração antes de tudo:** um GLB com uma rampa 0→1 em cada canal, lido no material. Falta confirmar se o importador do glTF
  converte o COLOR_0 (linear) em sRGB, e se a malha **sem** cor de vértice devolve branco na Nanite (deve; conferir).

### 2.3 Esteira e roda

- **Esteira:** o `track_band` é um loft do `track_path` com **u = comprimento ÷ passo do elo**. Um elo de verdade (pino, garra,
  dente-guia, furos) é modelado uma vez e **cozido num ladrilho 256×128** (BC, normal, ORM; Cycles na CPU), repetido no u. O jogo
  escreve o deslocamento (distância ÷ passo, frac) num dado de primitiva livre (os 10–11 parecem livres; conferir) e o material
  desloca o u: a esteira anda junto com as rodas, que já giram. O dente-guia de cima e a borda de fora ficam em relevo baixo na
  própria faixa.
  - Alternativa sem textura: exportar 4 fases da esteira de elos (deslocadas de ¼ de passo) e o ator troca a visível. Quatro
    malhas na memória, zero shader.
- **Roda de apoio:** banda de borracha com o vão escuro, disco rebaixado (a cavidade escurece), anel de porcas, cubo saliente.
- **Pneu:** flanco abaulado, blocos de 5–8 cm na banda, **pó na borracha** (o `Rubber` hoje é 28,28,28 puro: pneu novo é a peça
  mais "brinquedo" de todas; a ficha da Stryker tem o pneu cinza de pó).

### 2.4 Custo esperado na GTX 1660 (estimativas; medir com `stat GPU`, Nanite VisBuffer e BasePass, A/B na mesma cena)

| Item | Custo |
|---|---|
| Tanque de ~50 mil → ~100–150 mil triângulos (chanfro de 3 segmentos + grade plana); prédio grande de ~50 mil → ~150–250 mil | A Nanite trabalha pelo que aparece na tela, não pelo total: o tanque de 150 px rasteriza só alguns mil triângulos. O custo real é memória/disco (medir); ms de raster: ≲ 0,2 ms com 60 tanques |
| Cor de vértice + UV0/UV1 | +4–16 bytes por vértice; uma interpolação |
| Shader (quina, cavidade, tom, camuflagem) | ~150–250 instruções só nos pixels das unidades e prédios: ≲ 0,3 ms em 1080p |
| Esteira correndo | ~0 |
| Decais de malha (opacos, nunca mascarados) | +1 material por unidade; triângulos irrisórios |
| Cozimento no Blender (BVH em Python) | ~5–60 s por modelo, fora do jogo |

## 3. Por classe: as 5 coisas que mais faltam hoje

Olhando `captures/game/models/*/preview` (Blender) e a folha `v5_linebacker_stryker_folha.jpg` (no jogo, com o material) contra as
fichas de `docs/concept/units`, `docs/concept/buildings` e a `town_battle_top_gemini.webp`.

**Tanque de lagarta** (Type 99 × `china_mbt_sheet`)
1. Nenhuma sombra de contato: torre, saias, caixas e rodas com a mesma luz chapada; de cima o volume não lê.
2. Quina de 3 cm e caixas retas; a ficha tem chapas de borda arredondada, mantelete curvo e saias em painéis com fresta.
3. A camuflagem digital em blocos de geometria: o "lego" literal. Na ficha é pintura desbotada com pó por cima.
4. Rodagem sem leitura (borracha, disco, cubo) e **esteira parada** com o tanque andando; sem pó nas sapatas nem o brilho do uso.
5. Sem desgaste de quina e sem quebra de material: a lona da cor do jogador lê como plástico vermelho (sem dobra, sem tecido).

**Veículo de roda** (Stryker, picapes)
1. Pneu preto novo, sem pó e sem flanco abaulado.
2. Caixa de roda e vão do chassi sem escurecer: a roda parece colada por fora.
3. Casco branco liso: sem fresta de painel, sem sombra em alça e escotilha, sem tom por placa (a ficha é toda manchada).
4. A faixa da cor do jogador como fita chapada; na ficha é fina, desbotada e lascada.
5. Vidro e farol em cor chapada (sem reflexo, sem sujeira, sem moldura escura); embaixo do carro, nada pra quem deita a câmera.

**Caminhão** (supply trucks)
1. Carga em caixas lisas: falta tábua, cinta, lona com dobra presa por corda.
2. Sem AO entre a cabine, a carroceria e o chassi (as três partes se fundem de cima).
3. Para-lama, para-choque e degraus sem desgaste de quina nem lama no pé das rodas.
4. Pneu (como acima) e cubo sem porcas.
5. Detalhe de uso: galão, estepe, escada, faróis com grade, na escala de 2 px.

**Avião e helicóptero** (Chinook, Comanche)
1. Fuselagem sem linhas de painel e rebites em fileira: é o detalhe principal de aeronave (trim/decal, §1 item 7).
2. Pás retas de tábua, sem afinar nem torcer; girando, sem o disco borrado (malha à parte, fora da Nanite).
3. Sem fuligem de escape atrás dos motores e sem sujeira de óleo na barriga.
4. Entrada de ar, carenagem e trem de pouso sem AO: as peças boiam.
5. Janelas sem moldura escura e sem reflexo; a cor do jogador em faixas limpas demais.

**Prédio de concreto** (China War Factory × a ficha)
1. Paredes chapadas sem relevo de painel: a ficha tem placas com chanfro largo, juntas fundas e contrafortes chanfrados.
2. Sem AO de contato (prédio no chão, peças no teto, pórtico na parede, beiral do telhado).
3. Sem escorrido e fuligem da chaminé e da forja; concreto sem quina lascada.
4. As luminárias são pontos brancos; na ficha cada uma acende um leque quente na parede.
5. O vão do hangar é raso e claro; na ficha é fundo, escuro, com a luz de dentro.

**Prédio de adobe e sucata** (GLA Command Center × `gla_war_factory`)
1. Reboco com quina viva e parede perfeita: o adobe pede chanfro largo (8–15 cm, 3 segmentos) + o relevo de reboco no material
   (nunca remesh).
2. Sem sujeira no pé, nas ameias e nas janelas (cavidade e AO).
3. O verde da facção em faixa limpa; na ficha é pintado à mão, escorrido e lascado.
4. Chapa corrugada, lona e rede sem ferrugem, dobra nem furo.
5. Sem "vida" na parede: conduíte, fio, ar-condicionado, luminária com a luz no reboco.

**Infantaria** (o estudo próprio é o `docs/ESTUDO_INFANTARIA_VISUAL.md`; aqui só o que é detalhe)
1. Corpo de caixas (`blender_provisional.py:973`): falta o corpo humano de verdade (MPFB2) com o equipamento por cima.
2. A silhueta de cima: capacete com capa, ombros, mochila e **a arma saindo do corpo, exagerada ~1,2×**.
3. A cor do time onde se vê de cima (capacete, colete), pequena e saturada, sobre a farda desbotada.
4. AO entre os membros e o corpo e dobra no pano (cozidos na textura da farda, que o `soldier_dress.py` já gera).
5. Camuflagem em manchas largas que leiam a 3 cm/px, não o padrão fino de verdade (vira cinza).

## 4. O que não consegui confirmar

- **Nenhum dos RTS pedidos publicou a técnica** de textura das unidades: CoH3, AoE IV, Iron Harvest, Tempest Rising, Men of War II,
  Broken Arrow, WARNO e DoW III só têm direção de arte na imprensa. O diário 7 do Broken Arrow ("A sense of detail") devolveu 401.
  O que há:
  - o CoH3 trocou o desgaste cozido por um shader de veículo complexo com sujeira do ambiente (Bourdage, NME);
  - o AoE IV tirou detalhe dos soldados pra leitura e deixou o pintado nos prédios
    ([MobileSyrup](https://mobilesyrup.com/2021/04/10/age-of-empires-iv-relic-developer-interview/)†);
  - o Tempest Rising usa Nanite e Lumen, segundo um site de teste de placa, não o estúdio
    ([GameGPU](https://en.gamegpu.com/test-gpu/rts-strategii/tempest-rising-test-gpu-cpu)†);
  - do Total War, a wiki oficial só confirma diffuse/normal/gloss por peça; os mapas de sujeira e de máscara não confirmei.
- **Triângulos e texturas de tanque AAA de RTS:** só o Churchill do CoH2 (~10 mil, Sketchfab oficial, citado no estudo da
  infantaria) e números de fórum sem jogo certo (50 mil triângulos, casco 2k e torre 1k: Polycount†).
- **A esteira do World of Tanks:** só a manchete de que move elo a elo; como ele texturiza, não achei. A "textura correndo" é
  prática de fórum, não de palestra.
- **No motor:** a gama do COLOR_0 no importador, o branco da malha sem cor de vértice na Nanite, os dados de primitiva 10–11
  livres e o nome Python do Normal Curvature to Roughness. Tudo se confere num teste antes de usar.
- **Custos:** todos estimados; nada medido na 1660.

## 5. O que é decisão dele

- **Quanto desgaste.** Em 13/10 ele disse "não quero desgaste"; as fichas que aprovou depois são gastas (a da Stryker e a da Technical
  muito). Proposta: `Wear` por facção (USA ~0,3, China ~0,5, GLA ~0,9), mostrada em A/B no jogo, em vídeo.
- **A camuflagem digital** passar de geometria pro material (A/B lado a lado na câmera do game shot).
- **Ordem sugerida:** máscaras e chanfro no `model_lib` (1 tanque + 1 prédio de prova) → HLSL → esteira → decais.
