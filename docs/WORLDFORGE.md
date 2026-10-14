# WorldForge — criador de mundos do RTS

Projeto Unreal 5.8 em `WorldForge/`, ao lado do BlastLab. É um app: você
mexe nos controles e ele gera, em menos de um segundo, um mapa jogável de
RTS — terreno com platôs, penhascos e rampas, lagos, bases, campos de recurso,
florestas e rochas. A mesma semente sempre dá o mesmo mapa.

```
WorldForge\WorldForge.cmd                                        # abre o gerador (duplo clique)
powershell -File WorldForge\Tools\forge.ps1 setup                # compila e monta tudo do zero (1ª vez)
powershell -File WorldForge\Tools\forge.ps1 view [epic]          # abre (padrão: cine, o máximo)
powershell -File WorldForge\Tools\forge.ps1 view -Folder t -Seeds 1,2,3   # teste: fotos + dados
powershell -File WorldForge\Tools\forge.ps1 view -Folder ui -UITest       # teste da interface
```

**No app:**
- **Andar:** WASD, ou o mouse na borda da tela.
- **Botão direito arrastado:** rola o mapa como no C&C Generals. A câmera
  anda na direção do mouse, mais rápido quanto mais longe do ponto onde você
  apertou. É o mesmo do WebGL e do BlastLab.
- **Girar e inclinar:** botão do meio arrastado (até a horizontal) ou Q/E.
  Dois cliques na roda voltam ângulo e rotação pro início.
- **Zoom:** roda do mouse.
- **Atalhos:** Home volta pro mapa inteiro; um clique no minimapa leva a câmera até lá; F1 esconde o painel.
- **Mostrar:**
  - Passagem: penhasco em vermelho, água em azul.
  - Áreas: o território de cada jogador pelo caminho.
- **Salvar mapa:** grava em `WorldForge/Saved/Maps/` as alturas em 16 bits (o
  formato que o Landscape da Unreal importa), as camadas do chão, o minimapa e
  o `map.json` com bases, recursos e rush.

## 1. O que a pesquisa mostrou

### C&C Generals / Zero Hour (código da EA, GPL, fev/2025)

- **Sem gerador aleatório.** Os mapas eram feitos à mão no WorldBuilder. O
  RA2 e o TS tinham um gerador, mas o código dele não foi liberado.
- **Terreno:** grade de altura de 8 bits.
  - Célula de 10 unidades (~3 m); cada degrau de altura vale 0,625 unidade
    (`MapObject.h`).
  - Mapas 1v1 de ~220 células (~670 m); de 4 jogadores, 270–430 células.
- **Penhasco é calculado** (a partir de ~44°, em `AIPathfind`). Nos mapas
  oficiais ele ocupa no máximo ~6% da área.
- **Por jogador** (mapa de torneio): 2 supply docks + 6 pilhas + 1 oil derrick.
- **A lição que fica:** a lógica (altura, passabilidade, objetos) separada do
  visual.

### Geradores modernos

- **A ordem que todos seguem** (World Machine, Gaea, Houdini; Horizon Zero
  Dawn e Far Cry 5 na GDC; o PCG da Unreal): forma grande → detalhe →
  erosão → máscaras → materiais → espalhamento.
- **Mapas de RTS** (Age of Empires RMS, OpenRA 2026, StarCraft II):
  - etapas com semente própria;
  - simetria entre jogadores;
  - distância medida pelo caminho;
  - passagens abertas quando falta conexão;
  - validação automática.
- **Erosão rápida:** o filtro analítico do Rune Johansen (2025). Cava sulcos
  sem simular nada e cada ponto é calculado sozinho.

### O mercado de ferramentas de mundo (2024–2026)

- **Forma e beleza de terreno já estão resolvidas:**
  - **Gaea 2/3:** grafo de nós com "macros"; o Gaea 3 traz rios e estradas
    vetoriais.
  - **World Creator:** pilha de camadas na GPU, em tempo real.
  - **Houdini:** o padrão AAA pra ferramenta sob medida (Far Cry 5,
    Wildlands).
  - **Unreal 5.8:**
    - Landscape com edit layers e Landscape Patch;
    - PCG pronto pra produção, aceitando edição manual por cima do gerado;
    - Biome Core;
    - Mesh Terrain experimental;
    - servidor MCP nativo, por onde uma IA opera o editor.
- **O que os AAA fazem pra quem cria:** o artista desenha a **estrutura**
  (linhas, manchas, pontos) e o procedural faz o resto. Vale pra Horizon,
  Far Cry 5 e Wildlands. Edição local que sobrevive à regeneração, presa a
  algo com significado (esta rampa, esta base).
- **O que ninguém resolve: o mapa de RTS bom.**
  - No StarCraft II, a simetria é copiar e espelhar à mão e o rush é medido
    com cronômetro (o concurso do Team Liquid usa ~35 s agressivo, 40–50 s
    padrão).
  - O AoE IV tem "recursos balanceados" no gerador, mas nenhum editor mostra
    um relatório de balanceamento.
  - **É aí que dá pra ser melhor que o mercado.**
- **IA:** terreno por difusão (Terrain Diffusion, Earthbender) ainda é
  pesquisa: sem controle e sem garantia de jogabilidade. A IA útil hoje é a
  que **opera a ferramenta**: "2v2 no deserto, rio no meio com 3 travessias,
  rush de ~45 s" vira parâmetros e planta, e a análise confere.

## 2. O que eu faria (proposta, não combinada)

**A ideia central:** a fonte da verdade é a **planta de jogabilidade** (bases,
expansões, recursos, rampas, passagens, rotas, simetria). O terreno sai
dela, a análise roda sozinha, e o mundo final vai pro pipeline nativo da
Unreal, onde está a beleza AAA.

Fases, pelo tamanho do salto:

1. **Análise na tela e variações.** Custo baixo, maior diferencial.
   - Sobreposição de rush por par de bases, largura dos estrangulamentos,
     área de construção, vantagem do terreno alto e recursos por distância.
   - Uma nota por mapa.
   - Grade "9 sementes", ordenada pela nota.
   - O rush já aparece no painel (v0).
2. **Salto visual: pipeline nativo da Unreal.**
   - Landscape (testar Nanite), texturas virtuais (RVT) e PCG pra vegetação.
   - Megascans/Fab no lugar das malhas e texturas geradas.
   - Plugin Water pros lagos.
   - A beleza vem mais de material e vegetação do que do algoritmo de altura.
   - A prévia rápida atual continua como "rascunho"; um botão "gerar no
     nível" cria o mundo de verdade.
3. **Pintar intenção, com edição que sobrevive.**
   - Pincéis: nível, "rampa aqui", "bloquear", floresta, linha de rio.
   - Espelhados ao vivo pela simetria.
   - Cada edição fica presa a um objeto da planta, e regerar mantém tudo.
4. **Estilo:** rios com meandro e estradas (splines que escavam), vilas
   como "carimbos". Gaea como acabamento opcional.
5. **IA operando a ferramenta** pelo MCP da Unreal 5.8.

**Não vale fazer:**
- renderizador ou LOD de terreno próprio (o Landscape já faz);
- espalhamento de vegetação próprio (o PCG já faz);
- água própria (o plugin Water já faz);
- editor de nós próprio;
- erosão "de filme" (em RTS borra a leitura dos níveis);
- difusão como núcleo.

## 3. Como a v0 gera um mapa

Código em `WorldForge/Source/WorldForge/`. O núcleo (`Generation/`) só depende
de Core, então é o mesmo código que o jogo pode usar.

1. **Bases.** Nos cantos ou nos lados, na fatia do jogador 1, copiadas por
   rotação: 180° com 2 jogadores, 90° com 4.
2. **Níveis** (grade lógica de 4 m): água, baixada, platô, planalto,
   montanha.
   - Um ruído com domain warp cortado em frações-alvo de área: Água vira
     "% de água", Platôs vira "% de platô".
   - O planalto usa um ruído próprio e fica a 16 m da borda do platô. Com o
     mesmo ruído, saíam dois penhascos colados.
   - Moldura de montanhas na borda e bases forçadas planas.
3. **Limpeza.** Filtro de maioria e abertura/fechamento morfológico: faixas
   de platô e vales com menos de ~20 m somem. Ilhas pequenas são absorvidas.
4. **Caminhos.** Um Dijkstra que "pode construir" liga cada base ao centro,
   mais um flanco.
   - Desnível vira rampa de 18 m; água vira passagem rasa.
   - Platôs grandes isolados ganham rampa.
   - Os caminhos viram trilhas de terra.
5. **Distância pelo caminho** a partir das bases: territórios, rush e
   recursos (principal, expansão e disputados, à mesma distância pra todos).
6. **Relevo visível** (grade de 2 m):
   - penhascos de 7 m com contorno curvo e irregular;
   - margens rasas e montanhas com cristas;
   - rampas por cima;
   - o filtro de erosão só nas encostas e montanhas.
7. **Camadas do chão:**
   - terra: bases, trilhas, rampas, recursos;
   - vegetação: por onde a água escorre (fluxo D8) e em manchas;
   - areia: na margem;
   - rocha: nas encostas.
8. **Árvores e rochas** sorteadas por célula, fora das bases, rampas,
   trilhas e recursos.

- **Simetria exata:** os ruídos da fatia do jogador 1 são copiados girados,
  com emenda suave nas divisas. No fim, cada célula recebe o valor da sua
  canônica: o mapa é idêntico bit a bit pra cada jogador.
- **Determinismo:** cada etapa tem uma sub-semente. Pro multiplayer entre
  máquinas diferentes, a parte lógica pode passar pra inteiros, como fez o
  OpenRA.

**Desenho:**
- Malha gerada em pedaços de 64×64, com paredes de maquete nas bordas.
- Material num nó de código:
  - grama, terra e areia projetadas de cima em duas escalas;
  - rocha triplanar;
  - mistura pela altura da textura;
  - cores do bioma em `WorldPalette.h`.
- Texturas geradas por script (`Tools/make_textures.py`). Nada é baixado.
- Água translúcida com cor e transparência pela profundidade.
- Objetos low-poly gerados em código e instanciados.
- Luz:
  - sol de 120 mil lux, Sky Atmosphere, Lumen e sombras virtuais;
  - exposição física travada (EV100 15) mais exposição local, pra sombra
    legível.

## 4. Medições e testes (05/10/2026, GTX 1660, 1600×900)

- **Geração:** 0,4–0,6 s num mapa de 1 km e ~1,2 s num de 1,5 km. Roda fora
  do quadro, então o painel não trava.
- **Desempenho no app:**

  | Qualidade | Tempo de quadro | qps |
  |---|---|---|
  | Cine | 53 ms | 19 |
  | Épico | 31 ms | 32 |
  | Alto | 29 ms | 35 |

- **Teste da interface (`-UITest`): 15 de 15 passos ok.** Os cliques,
  arrastes, teclas e roda sintéticos passam pelo Slate:
  - sortear, 4 jogadores, deserto;
  - arrastar a água a 90% e clicar solto na faixa das florestas;
  - as três camadas e o clique no minimapa;
  - W anda; a roda dá zoom, mas não em cima do painel;
  - rolagem com o botão direito;
  - botão do meio até a horizontal e duplo clique na roda.

  O mouse de verdade em cima da janela pode atrapalhar o teste. Antes de
  cada clique ele solta a captura do mouse e avisa no log quando isso
  acontece.
- **Sementes testadas:** 1, 3, 5, 7, 42 e aleatórias; 2 e 4 jogadores; os
  três biomas; 768, 1024 e 1536 m. Todas com as bases ligadas por terra.

**Achados que valem pro BlastLab também:**
- Com exposição física de dia (EV ~15), o padrão de
  `r.EyeAdaptation.CachedLightingPreExposure` (4) corta a luz guardada do
  Lumen e do céu: sombras pretas e aviso vermelho. Com 8 cobre até EV 16.
- O comando `Scalability` só aceita número (0–3) ou `cine`. `epic`, `high`
  etc. dão "unknown parameter".
- O Single Layer Water da 5.8 aqui calculou a luz dentro d'água como ~0: o
  espalhamento não mudava nada (A/B). A água ficou translúcida por
  profundidade.

**Pendências conhecidas:**
- **Deserto:** algumas faixas estreitas ainda parecem valetas.
- **Sombras virtuais:** a fila de trabalho (tamanho fixo no shader) estoura
  com terreno e árvores sem Nanite. Custa desempenho, mas a sombra sai certa.
  O aviso está escondido (`r.Shadow.Virtual.AllowScreenOverflowMessages=0`);
  some de vez com Landscape e Nanite (fase 2).
- **Rampas:** saem sempre alinhadas aos eixos.
- **Jogadores:** só 2 e 4 por enquanto. 3, 6 e 8 pedem mapa circular ou
  espelho.

## 5. Fatia de qualidade: deserto (05/10/2026, em andamento)

Prova de que o WorldForge chega a imagem AAA: um recorte de 252 m do mapa
gerado vira um nível de verdade na Unreal (`/Game/Maps/Slice_Desert`).

```
python WorldForge\Tools\fetch_assets.py              # baixa os assets CC0 (~630 MB, fora do git)
powershell -File WorldForge\Tools\forge.ps1 slice     # monta o nível (Tools/build_slice.py)
powershell -File WorldForge\Tools\forge.ps1 view -Map Slice_Desert [-Folder f]   # abre (ou fotografa + passeio)
python WorldForge\Tools\make_motion.py captures\worldforge\f      # passeio em WebP animado
```

**O que entra:**
- **Assets:** CC0 do Poly Haven, a coleção Namaqualand (deserto real escaneado
  na África do Sul: superfícies, rochas, paredões, arbustos e capim da mesma
  região), mais o decalque de marca de pneu do ambientCG. Os Megascans no Fab
  são pagos desde 2025 (fora uma seleção grátis) e a licença não é livre.
- **Terreno:** Landscape da Unreal criado do gerador pela função de editor
  `WorldForgeEditor/WorldForgeEditorLib`. Tem 0,5 m por quad e 5 camadas
  (areia, cascalho, terra, rocha, terra rachada) pintadas pelas máscaras.
  - A areia usa duas fotos: a aérea de 45 m pra longe e a de 2,5 m pra perto.
  - Por cima, uma variação larga de tom contra a repetição.
  - A rocha é triplanar.
  - Cada canal (cor, normal, rugosidade, oclusão) tem a própria mistura pela
    altura.
- **Espalhamento:** rochas, pedras, pedregulhos, capim, arbustos, galhos e
  paredões escaneados (Nanite nas rochas), pelas posições e máscaras do
  gerador. Os paredões vão nos cortes de platô, virados pra baixo da encosta.
- **Marcas de pneu:** decalques em dupla ao longo das trilhas que o gerador
  abriu.
- **Luz:**
  - sol a 20° (sombra legível pro RTS);
  - poeira na atmosfera (Mie);
  - exposição travada, exposição local, sombra dura no contato.

**Armadilhas que custaram tempo:**
- O `LandscapeLayerBlend` em "atributos de material" falha em combinações de
  camadas na 5.8. A causa real era o `ComponentMask`, que nasce com R e G
  ligados: um "só R" virava 2 canais.
- A normal da foto do decalque deixava a marca preta (A/B), então ela ficou
  de fora.
- No decalque, a caixa fica ao longo da trilha com o Y local, mas a foto entra
  girada 90°. Medido com um decalque de teste colorido pelas coordenadas.
- A primeira abertura compila shaders e mostra o quadriculado: o teste espera
  terminar antes de fotografar.

**Medido:** 50 ms (20 qps) na qualidade máxima a 1600×900 na GTX 1660.

## 6. Mais fatias: floresta, neve e estrada (05/10/2026, sem baixar nada novo)

Pedido da noite: fatias exemplares de floresta e neve, e chão de rua igual à
arte conceitual (asfalto rachado, faixa pintada, acostamento de terra). Sem
downloads novos (só os assets CC0 já aprovados + o que vem com a Unreal +
texturas geradas por código).

```
python WorldForge\Tools\make_slice_textures.py                 # rachaduras, neve, gelo, folhiço, grama
powershell -File WorldForge\Tools\forge.ps1 slice [desert,snow,forest]   # padrão: as três (~15 min)
powershell -File WorldForge\Tools\forge.ps1 view -Map Slice_Forest [-Folder f]   # Slice_Desert, Slice_Snow, Slice_Forest
```

**Referências olhadas (capturas da Steam):**
- Company of Heroes 3, deserto: areia lisa com sombra larga das dunas, rocha
  vermelha, manchas escuras de mato. A areia não é granulada vista do alto.
- Company of Heroes 2, inverno: neve muito clara com sombra azulada, estrada
  de neve pisada virando lama cinza-marrom, pinheiros escuros com neve em
  cima, rio congelado cinza com rachaduras.
- Manor Lords (Unreal): campo de grama densa, trilha de terra, a borda da
  mata como um bloco escuro, névoa de distância.

**O que mudou no pipeline:**
- `build_slice.py` monta qualquer fatia (`SLICE=desert,snow,forest`). Cada
  fatia tem materiais, camadas e mestres próprios. A mesma malha serve às
  três, porque o material entra só no componente instanciado. Refazer uma não
  estraga as outras.
- **Árvores:** as amostras do Procedural Vegetation Editor que vêm com a 5.8
  (um pinheiro de ~22 m, uma árvore de folha larga de ~11 m e um arbusto).
  - Elas vêm como malha esquelética Nanite. O tronco é uma malha, e os
    raminhos e folhas são uma "assembly" (centenas de peças instanciadas nos
    ossos).
  - `VegetationToStaticMesh` converte isso pra malha estática na pose de
    referência e mantém a assembly. Exemplo: a de folha larga tem 206 mil
    triângulos de tronco e 801 raminhos instanciados.
  - Precisa de `r.Nanite.Foliage=1` (Nanite Foliage, experimental na 5.7+).
    Sem isso a Unreal desenha só o tronco. Ligar recompila todos os shaders
    uma vez.
  - As árvores da amostra de PCG (PCG_Pine etc.) foram descartadas: são
    bonecos de 200 triângulos sem textura.
- **Neve nas árvores:** cópia do mestre de folhagem do PVE
  (`AddSnowToMaterial`). A cor e a rugosidade viram neve no que olha pra cima,
  pela normal do vértice, com falhas em manchas de ~3 m. É a técnica de
  "neve por cima" do `WorldAlignedBlend` da própria Unreal.
  - Nas rochas, o mestre `M_ScanRock` ganhou o mesmo parâmetro `Snow`.
  - A camada de neve do Nanite não serve: a "overlay material" não roda em
    malha Nanite (`NaniteResources.cpp` nem lê).
- **Bosques:** o espalhamento `Forest` usa a vegetação do gerador mais uma
  mancha larga de ~60 m. Sai mata com borda e clareira, fora de trilha, rocha,
  água e encosta.
- **Estrada (deserto):** a trilha mais longa vira pista aplainada. Tem:
  - borda gasta, buracos e acostamento de terra e cascalho;
  - a textura `asphalt_04` com uma máscara de rachaduras gerada: couro de
    jacaré e trincas longitudinais e transversais (os defeitos do manual de
    pavimento da FHWA), terra dentro da trinca, remendos e areia soprada;
  - faixas em decalque: tracejado central de 3 m a cada 12 m (a proporção
    10 ft/30 ft do MUTCD) e bordas contínuas. A tinta falha onde a pista
    rachou.
- **Texturas geradas** (`make_slice_textures.py`, periódicas):
  - neve com ondulação de vento e brilho de cristal;
  - gelo de lago;
  - folhiço de folhas secas sobrepostas com gravetos;
  - grama baixa em traços.
  São o melhor sem download; scan de verdade é melhor (lista no fim).

**Resultado (fotos e passeio em `captures/worldforge/<fatia>_final/`, `sheet.png` e `motion.webp`):**
- **Neve:** a melhor das três. Pinheiros com neve nos galhos, neve lisa com
  ondulação fina de vento, sombra longa azulada do sol de 18°, barro cinza nos
  trechos pisados. Perto do Company of Heroes 2. Falta lago congelado nesta
  fatia (o recorte escolhido não tem água) e trilha pisada contínua.
- **Floresta:** as árvores são o ponto alto: copa cheia de folhas de verdade,
  sombra salpicada no chão e folhiço marrom embaixo da mata.
  - O chão aberto ainda é o fraco: grama gerada por código, que de cima lê
    como gramado.
  - A copa é escura perto do Manor Lords.
  - Contra o sol, o chão rasante clareia demais.
- **Deserto com estrada:** asfalto escuro com rachadura fina, faixa de borda
  contínua, tracejado central e acostamento de areia, ao longo da trilha do
  gerador. Lê como estrada de verdade da câmera de RTS.
  - Está mais limpo que a arte conceitual. Falta buraco grande, areia
    cobrindo trechos e couro de jacaré largo. Pra isso servem os scans de
    asfalto quebrado da lista.

**Medido** (qualidade máxima, 1600×900, GTX 1660; com um editor do BlastLab
aberto em paralelo, então é o pior caso):

| Fatia | Tempo de quadro |
|---|---|
| Deserto | 49 ms (20 qps) |
| Neve | 62 ms (16 qps) |
| Floresta | 70 ms (14 qps) |

Uma rodada da floresta deu 124 ms com o BlastLab rodando junto. O custo novo
é a folhagem Nanite mascarada: centenas de árvores com milhares de raminhos
cada.

**Armadilhas desta rodada:**
- `unreal.Rotator(a, b, c)` no Python é (roll, pitch, yaw), não (pitch, yaw,
  roll). Os decalques saíam atravessados: faixa da estrada virou "faixa de
  estacionamento" e as marcas de pneu viraram tracinhos. Agora vai por nome
  (`pitch=-90, yaw=...`). O Z local fica ao longo da linha, e o shader tira o
  U do Z (`DeferredDecal.usf`, `DecalVector.zyx`).
- O anel em volta da câmera no deserto vinha da troca da areia de perto pela
  aérea, pela distância. Em movimento ele andava junto. Agora as duas fotos
  vão misturadas sempre.
- A ondulação de duna (6 m) na neve, com sol baixo, virava "caixa de ovo".
  Agora as dunas são só do deserto.
- Fotos automáticas com alguém mexendo no PC: o mouse mexia a câmera. Agora
  a câmera fica presa durante a captura (`bLocked`).
- Passeio: o 1º quadro saía borrado pelo salto da vista anterior. Agora a
  câmera para no começo e espera.

**Downloads (CC0, Poly Haven; aprovados e baixados em 06/10/2026, 686 MB):**

| Pra quê | Asset | Tamanho |
|---|---|---|
| floresta: folhiço | `forest_leaves_02` (textura 4k) | 19 MB |
| floresta: chão de mata | `forest_ground_04` | 50 MB |
| floresta: folhas e terra | `leaves_forest_ground` | 54 MB |
| floresta: barro de mata | `mud_forest` | 53 MB |
| floresta: grama com folhas | `leafy_grass` | 65 MB |
| floresta: trilha | `grass_path_2` | 22 MB |
| floresta: samambaia | `fern_02` (modelo 2k) | 3 MB |
| floresta: arbustos | `shrub_01`, `shrub_02` | 15 MB |
| floresta: tocos, tronco caído | `tree_stump_01`, `dead_tree_trunk` | 30 MB |
| floresta: pedra com musgo | `rock_moss_set_01`, `moss_01` | 8 MB |
| neve: neve de verdade | `snow_02`, `snow_04` | 61 MB |
| neve: vista aérea | `snow_field_aerial` | 20 MB |
| neve: estrada com neve | `asphalt_snow` | 64 MB |
| rua: asfalto quebrado | `road_damaged`, `road_damaged_2` | 91 MB |
| rua: asfalto gasto | `worn_asphalt` | 56 MB |
| rua: asfalto visto do alto | `aerial_asphalt_01` | 26 MB |
| rua: mureta, poste de luz, poste | `concrete_road_barrier`, `street_lamp_01`, `modular_electricity_poles` | 34 MB |

Os pinheiros escaneados (`fir_tree_01` 249 MB e `pine_tree_01` 649 MB)
ficaram de fora: o pinheiro do PVE já serve. Tamanhos lidos da API do Poly
Haven em 05/10/2026.

Ficaram de fora depois de importar:
- `moss_01`: tufos de 1–3 cm, pequenos demais pra câmera de RTS.
- `modular_electricity_poles`: é um kit de peças (parafuso, conector,
  fusível) sem o poste.
- `street_lamp_01`: poste de luz na estrada do deserto não combina.

**Assets novos nas fatias (06/10), na escala real de cada foto (API `/info`
do Poly Haven):**
- **Floresta:**
  - chão: `leafy_grass` (2 m), `forest_leaves_02` (3 m), `forest_ground_04`
    (3,15 m), `mud_forest` (2,35 m);
  - plantas e madeira: samambaias, arbustos de 1–2 m, mudas, tocos e
    troncos caídos (`Undergrowth` e `Deadwood`, dentro da mata);
  - rochas: com musgo (`rock_moss_set_01`).
- **Neve:**
  - chão: `snow_02` (2 m) e uma estrada de `asphalt_snow` com neve soprada;
  - rochas com musgo e madeira morta, com neve por cima;
  - a escolha do recorte agora prefere ter um lago (o rio congelado).
- **Deserto:** asfalto `road_damaged_2` com a foto aérea `aerial_asphalt_01`
  por cima contra a repetição, e muretas de concreto na beira da estrada (uma
  tombada).

**Cor medida, não chutada** (média linear da foto, `PIL`):
- A neve do Poly Haven tem albedo 0,39. Neve fresca reflete 0,8–0,9, então a
  foto entra ×2,1, com a neve gerada (0,82) por cima contra a repetição.
- A aérea `snow_field_aerial` é neve com barro (0,26) e deixava tudo bege.
  Saiu.
- A grama `leafy_grass` é palha na média (0,32/0,24/0,10). Entra puxada pro
  verde, com a grama gerada por cima.

**Armadilhas:**
- `snow_04` é foto de sulcos de pneu paralelos. Colada no terreno, alinhada ao
  mundo e não à estrada, as listras cruzavam o acostamento na diagonal e
  pareciam dentes. Saiu.
- Montando várias fatias seguidas, a escolha do recorte de uma usava o bioma e
  a estrada da anterior (variáveis globais do C++). O deserto caiu no lugar da
  neve. `SuggestSliceCenter` agora zera tudo.
- A borda do asfalto tinha 0,45 m de transição, menos que o espaçamento do
  terreno (0,5 m). Agora são 1,2 m.
- As pontas da estrada terminavam num "toco" redondo. Agora o asfalto se
  desfaz em buracos e terra nos últimos 45 m, sem tinta nos últimos 40 m.

## 7. "Ver em qualidade real": o mapa inteiro no acabamento (06/10/2026)

O botão no painel do gerador monta **o mapa inteiro** (não só um pedaço) com
o acabamento das fatias, usando exatamente os controles do painel, e abre
quando termina.

- O app grava os controles em `Saved/WorldForge/world_settings.json` e
  chama o editor sem janela (`UnrealEditor-Cmd`, `build_slice.py` com
  `SLICE=world`). O app continua usável enquanto monta.
- `LoadWorldSettings` faz o `BuildSliceLandscape` gerar com esses controles.
  O terreno tem 16×16 componentes de 127 quads (2033² vértices, ~0,5 m no
  mapa de 1 km). Espalhamento, decalques e estrada são os da fatia do bioma.
- O resultado é `/Game/Maps/World_Full`, com "← Voltar ao gerador" no canto.
  Ele volta com os mesmos controles (`?resume`). Na 1ª abertura de materiais
  novos aparece "preparando os materiais".
- **Medido:**
  - Monta em ~2 minutos (deserto 107 s, neve 117 s).
  - Roda como a fatia, porque a Unreal só desenha o que está perto da câmera:
    deserto 20 qps (51 ms), neve 16 qps (63 ms).
  - O deserto tem 840 rochas, 200 mil pedras, 145 mil seixos, 212 mil
    touceiras, 2.400 paredões e 1.900 marcas de pneu.
- Pra montar o mapa inteiro rápido, a distância até a estrada usa uma grade
  de 8 m com os trechos próximos. Sem ela: 4 milhões de vértices × mil trechos
  de estrada.
- Teste de ponta a ponta sem clicar: `-ForgeRealQuality` dispara o mesmo
  caminho do botão quando o 1º mapa aparece. Com `-ForgeCapture`, o mapa
  montado tira as fotos, e `-ForgeBack` volta pro gerador.

```
powershell -File WorldForge\Tools\forge.ps1 view -Folder rq -Biome 2 -Extra '-Seed=7 -ForgeRealQuality -ForgeBack -ForgeNoMotion'
```

**Falta:**
- As áreas de recurso do gerador aparecem como discos de terra no deserto.
  Precisam de objeto de recurso ou de borda que conte história.
- A estrada é uma só, na trilha mais longa. Faltam a rede (base a base) e os
  cruzamentos.
- As medições com outro editor da Unreal aberto (o BlastLab) saem 2–3× piores.
  Medir sempre com a máquina livre.



## 8. Desempenho e árvores iguais no rascunho e na qualidade real (06/10/2026)

**Medido antes de mexer** (`-ForgeProfile -csvGpuStats` + `Tools/gpu_profile.py`,
floresta, vista média, placa livre):
- Quadro de 73 ms (14 qps) no modo cine.
- Iluminação global (Lumen, *screen probe gather*): 46 ms. Era 62% do
  quadro.
- Sombras 8,5 ms, TSR 5,6 ms e as árvores e rochas (Nanite) só 4,3 ms. As
  árvores já simplificam de longe: o PVE as deixa com
  `ShapePreservation = Voxelize`, o Nanite Foliage da 5.7+ que troca a copa
  distante por voxels.

**Por quê:** o cine da Unreal é modo de filme (`BaseScalability.ini`). Ele faz
4× mais pontos de amostra (`DownsampleFactor` 8 contra 16) e 4× mais raios por
ponto (`TracingOctahedronResolution` 16 contra 8) que o épico, ou seja 16× mais
raios. Da câmera de RTS a imagem é a mesma (A/B em
`captures/worldforge/forest_cine_vs_epic.png`).

**O que mudou:**
- **Ultra = épico.** Era o padrão do app (`forge.ps1 view`) e passou a ser o
  "Ultra" do painel (Baixa/Média/Alta/Ultra = níveis 0–3 da Unreal). O cine
  fica pra foto e vídeo.
- **O miúdo sai da iluminação global e do campo de distância.** Capim, seixo,
  galho, muda e tudo que tem distância de corte ficam com
  `bAffectDistanceFieldLighting` e `bAffectDynamicIndirectLighting` desligados.
  O campo de distância custava ~5 ms por quadro.
- **Resultado, floresta:** de 73 para 29–34 ms (30–34 qps). A variação vem de
  outro programa usando a placa. O mapa inteiro de floresta (2.100 árvores)
  roda a 43 qps.
- Medir sem o gravador: os tempos de GPU por passe fazem a CPU esperar a placa
  e somam ~8 ms. Ele serve pra ver a divisão, não o total.

**Árvores iguais nos dois lados (pedido de 06/10):** a qualidade real tinha
muito mais árvores que o painel. Eu espalhava as árvores por conta própria e
ignorava as do gerador e o controle "Florestas". Agora:
- Há uma regra só em `Generation/WorldTrees.h`:
  - o tipo vem do bioma e do sorteio do gerador (a floresta temperada mistura
    folha larga e pinheiro);
  - o tamanho vem da malha real medida × a escala base × a escala do gerador.
- **Qualidade real:** `TreeBroadleaf` / `TreeConifer` são exatamente as
  árvores do gerador (posição, giro, tamanho), simétricas como o gerador as
  pôs. O folhiço, o sub-bosque e a madeira morta seguem a cobertura das copas
  delas. O folhiço fica só onde a mata fecha, sem disco laranja em árvore
  solta.
- **Rascunho (o app):** desenha as árvores de verdade (`/Game/Slice/Trees`)
  com o Nanite mais grosso (`r.Nanite.MaxPixelsPerEdge` 4, só no gerador, de
  volta a 1 no mapa em qualidade real). Sem as árvores montadas, usa as
  simples em código esticadas pro mesmo tamanho.
- Comparação com a mesma semente e a mesma câmera:
  `captures/worldforge/gen_vs_real_f42.png`. São as mesmas árvores no mesmo
  lugar.
- As fatias de exemplo usam os controles do painel por JSON (como o botão): a
  floresta com "Florestas" 0,85 e a neve com 0,7.

**Teste de qualidade (F2), refeito em 06/10:**
- A 1ª versão não servia: a câmera de teste desenha o que **ela** vê, sempre
  completo. A Unreal decide o que descartar **por câmera, a cada quadro**, e
  o que a câmera principal descartou não aparecia nela.
- Agora o F2 testa cada bloco da cena contra a vista principal:
  - fora do campo de visão, ou além da distância de corte × `r.ViewDistanceScale`
    do preset, o bloco é escondido nos quadros (`HiddenComponents`);
  - de costas pro ponto, o chão, as árvores e as pedras somem do quadro;
  - o mapa pequeno pinta os blocos de terreno: verde se desenhados, cinza se não.
- Pra isso as instâncias (árvores, pedras, capim) ficam em **blocos de 64 m**
  (`AddInstances`), como a vegetação e o PCG da Unreal particionam. A câmera
  descarta o bloco inteiro de uma vez. A floresta tem 564 blocos e ficou nos
  mesmos 34 qps.
- O quadro amarelo mostra **quantos pixels a sua câmera gasta naquele ponto**,
  ampliados: 480×270 a 30 m, 60×33 a 350 m. É por isso que a Unreal pode
  simplificar de longe sem ninguém notar.
- As câmeras de teste guardavam a própria iluminação global e estouravam a
  memória de vídeo da 1660 (1,2 GB acima, 4 qps). Agora vão sem Lumen nem
  reflexos, e a câmera do mapa pequeno fotografa 2 s e se desliga: 23 qps com
  o F2 ligado.

**Mapas guardados e prévias guardadas:**
- O painel tem uma lista de mapas: um clique carrega os controles.
  - Ela começa com os 3 de exemplo (deserto semente 3, neve 5, floresta 42).
  - "Guardar este mapa na lista" acrescenta o atual.
  - Fica em `Saved/WorldForge/maps.json`.
- Cada prévia em qualidade real fica guardada em
  `/Game/Maps/Worlds/W_<chave>`.
  - A chave é a impressão digital (MD5) dos controles + do script de
    acabamento + `PipelineVersion`, o mesmo esquema do cache de shaders da
    Unreal.
  - Mesmos controles: abre na hora (testado: 1ª vez 110 s de montagem, 2ª vez
    direto).
  - Mudou qualquer controle (testado com a semente) ou o acabamento: a chave
    muda e monta de novo.
  - Guarda as 12 mais novas, ~100 MB cada.
  - Ao mudar o gerador ou as regras do C++, subir `PipelineVersion` em
    `WorldForgeGameMode.cpp`.

**Tela feia depois do F2 (06/10): era o atalho da Unreal.**
- Nas versões de desenvolvimento a Unreal liga F1–F5 a modos de depuração
  (`BaseInput.ini`, `DebugExecBindings`): F1 wireframe, F2 **unlit** (sem sol
  nem sombra; a exposição sobe e a água e os cristais estouram em branco), F3
  lit, F4 e F5.
- O log da sessão dele mostra `Set new viewmode: Unlit` no mesmo instante do
  F2.
- `Config/DefaultInput.ini` tira esses cinco atalhos. O teste do painel tem
  agora um passo `key_f2`: aperta F2 e confere que o modo de visão continua
  "lit" e que o teste abriu. 16 de 16 passos.
- Lição: tecla nova → conferir as `DebugExecBindings` da Unreal antes.

**E a memória (achado à parte, também real):**
- Com o mapa inteiro aberto, a memória de vídeo do WorldForge fica em ~2,2 GB
  dos ~4,7 GB que o Windows dá pra ele. O F2 soma ~200 MB.
- Com o BlastLab aberto junto (a 1660 tem 6 GB pros dois), o limite cai.
  O F2 com a câmera longe passou dele: 1,9 GB "estourados".
- Aí o Windows joga parte da memória de vídeo pra memória comum e ela não
  volta ao desligar o F2. O quadro foi de 36 ms a 68 ms e a imagem degradou.
  Reproduzido em `captures/worldforge/f2_repro2`. Sozinho, o mesmo caminho não
  estoura.
- O F2 passou a fotografar 5× por segundo, não a cada quadro.
- Regra: medir e testar com um editor da Unreal por vez.

**O quadro "pixelado" do F2 não é a redução da Unreal:**
- É a sua tela ampliada: de longe, aquele pedaço ocupa poucos pixels.
- A Unreal reduz outra coisa:
  - **triângulos:** o Nanite mantém ~1 triângulo por pixel; de perto um galho
    tem centenas, de longe uma árvore inteira tem poucos, e a folhagem vira
    voxels;
  - **resolução das texturas:** os mipmaps; de longe vem a versão pequena.
  Os dois somem dentro do mesmo pixel, por isso ninguém percebe.
- **F3** mostra isso na tela grande: os grupos de triângulos do Nanite
  (`r.Nanite.Visualize Clusters`, cada cor ~128 triângulos). O modo
  "Triangles" lia pior: os triângulos ficam do tamanho de um pixel em qualquer
  distância, e é justamente essa a ideia do Nanite.
- O chão (Landscape) aparecia preto no F3 porque não era Nanite; desde 06/10
  é (seção 9).

## 9. Árvore transparente, rede de estradas e o mapa inteiro com cara de fatia (06/10/2026)

**Árvore transparente perto do cursor (estilo Company of Heroes):**
- A copa entre a câmera e o ponto do chão sob o cursor some, num cilindro em
  volta dessa linha (raio 5–14 m, cresce com a distância da câmera). Some em
  pontilhado que muda a cada quadro (o TSR suaviza). A sombra fica no chão.
- Material: `AddOcclusionFadeToMaterial` (C++) na cópia do mestre do PVE
  (`MA_Foliage_Trees_Fade4`, neve `_Snow5`). A câmera e o cursor vêm da
  coleção `MPC_TreeFade`, que `ForgeCameraPawn::UpdateTreeFade` atualiza.
- **Dois achados:**
  - Os materiais das árvores do PVE são **opacos** (as folhas são geometria),
    e o opaco ignora o recorte. As cópias do WorldForge ligam "masked".
    Custo medido: ~0,05 ms.
  - De longe o Nanite desenha a árvore em **voxels**, e o voxel só compila
    com o material padrão (`NaniteCullRaster.cpp`), então o recorte não vale
    ali. Solução (`ForgeTreeFade`): só as árvores perto da linha saem do bloco
    delas e vão pra um componente "fantasma" com a cópia que nunca vira voxel
    (`SM_x_Fade`, forma "área"). São 5–25 árvores por vez, ~0,1–0,2 ms.
- Só no mapa em qualidade real. No gerador não: as árvores dele são
  componentes hierárquicos enormes.
- **Defeito antigo achado no caminho:** a neve das coníferas usava a normal do
  vértice, e metade das agulhas (o lado de trás) ficava sem neve. A árvore só
  era branca de longe, nos voxels; de perto ficava verde. Agora vale a normal
  do lado que se vê (`TwoSidedSign`). Verdor medido: voxel 0,83 × triângulo
  0,86 (antes 0,79 × 3,08).

**Rede de estradas (no lugar do trecho único da trilha mais longa):**
- Fontes: Galin et al. 2010 (rota no relevo, vizinhança de 16 direções),
  Galin et al. 2011 (hierarquia e trechos absorvidos), Emilien et al. 2012
  (ramal entra na estrada existente), Ghost Recon Wildlands GDC 2017 (mesmo
  método em produção).
- Tipos:
  - **principal** de cada jogador (base → centro): asfalto no deserto
    (7,5 m) e na neve (6 m), terra de 6 m na floresta;
  - **flancos e ligações**: terra de 4,4 m (pista simples de estrada de
    serviço, 3,5–4,5 m, Keller & Sherar 2003);
  - **ramais**: cada campo de recurso longe da rede ganha um. É o caminho mais
    barato na grade de 2 m (rampa acima de 12% fica cara, penhasco e água não
    passam) até a estrada mais perto.
- Trecho que repete uma estrada já feita é absorvido. Pontas que se encontram
  viram uma estrada só: as duas principais formam uma rodovia contínua de base
  a base pelo centro, e as de terra cruzam em X. Toda ponta acaba numa base
  (o asfalto afina e se desfaz em 45 m na terra do platô), num campo ou num
  cruzamento. No cruzamento, o asfalto manda.
- A rede é do mapa inteiro, não do recorte: as fatias mostram o pedaço dela.
- Cota: o asfalto suaviza o relevo em ±25 m (corta e aterra); a de terra
  acompanha o relevo (±8 m: com ±25 m ela afundava num corte nas rampas). O
  talude que passa de ~40° vira rocha (projetada dos três lados).
- Pintura e muretas só fora dos cruzamentos e das pontas. As marcas de pneu
  vão nas estradas de terra.
- Mapas de teste (semente, bioma): deserto 3 → 0,9 km de asfalto + 1,6 km de
  terra; floresta 42 → 3,2 km de terra; neve 5 → 1,0 + 1,8 km.

**Mapa inteiro com cara de fatia (o que estava feio e o que mudou):**
- **Campos de recurso e bases sem disco:** a borda é deformada por ruído
  (domain warp, Quilez) em lóbulos do tamanho do campo (~9 m) e finos (~6 m).
  Isso segue os campos do Age of Empires (grupos apertados de manchas) e a
  borda de ≥ 4 m. Bases com lóbulos de ~22 m. O ruído é o simétrico: os
  jogadores veem o mesmo.
- **Rampas:** saiu o quadrado de terra. A estrada passa por elas.
- **Paredões:** a listra laranja regular na parede era a textura de rocha
  (`cliff_side`, estratos a cada 3,6 m). Agora:
  - mistura duas escalas;
  - a rocha escaneada cobre a parede: cada célula de 9–13 m põe o paredão no
    ponto mais íngreme dela, e só onde há desnível de ≥ 1,8 m, com o tamanho
    pelo desnível;
  - antes, um ponto sorteado por célula errava a faixa do corte metade das
    vezes; agora são ~6,4 mil paredões contra ~1,7 mil.
- **Deserto:** as manchas largas de terra do gerador saíram do acabamento:
  vistas de cima liam como sujeira. A variação agora é o cascalho ("pavimento
  do deserto"): ~15% do chão em manchas de ~45 m com borda recortada em ~8 m,
  e a cor perto da areia.
- **Floresta:** manchas de ~70 m entre verde escuro e capim seco no gramado.
  Antes, da câmera de jogo (sem touceiras a 190 m), o campo era de um verde
  só.
- Desempenho (deserto inteiro, vista de jogo a 190 m, placa livre): 22–24 ms
  por quadro.

**Os 4 presets (Baixa/Média/Alta/Ultra), medidos em 06/10:**
- Condições:
  - GTX 1660, 1600×900;
  - mapa inteiro de floresta (2.100 árvores, 3,2 km de estrada), vista de jogo
    a 190 m; gerador com a semente 42;
  - duas rodadas intercaladas, vale a melhor;
  - outros programas (Chrome, o app do Claude) ocupavam ~40% da placa: numa
    placa vazia o número é menor; a ordem e as diferenças valem.

| Preset | Mapa inteiro | Gerador | O que pesa |
|---|---|---|---|
| Baixa | 11,4 ms (88 qps) | ~13 ms | sombra mínima, sem iluminação global |
| Média | 16,8 ms (60 qps) | 17,6 ms | sombras; Lumen leve |
| Alta | 23,6 ms (42 qps) | 25,2 ms | Lumen 2,5 ms |
| Ultra | 36,1 ms (28 qps) | 37,5 ms | Lumen (*screen probe gather*) 7,7–9 ms |

- Folha com a mesma vista nos 4: `captures/worldforge/presets_sheet.jpg`.
  - Alta e Ultra quase não se distinguem da câmera de RTS.
  - A Média fica perto delas.
- **A Baixa da Unreal desliga a sombra do sol**, e a mata e o relevo viravam
  recorte de papel. `Config/DefaultScalability.ini` dá ao nível 0 de sombra os
  valores do nível 1. Custo: +1,2 ms
  (`captures/worldforge/low_shadow_ab.jpg`). Continua sendo o nível 0
  inteiro: o painel e a volta da qualidade real leem o preset certo.
- O "Alta mais lento que Ultra" da medição anterior não se repetiu com a placa
  livre. Era o outro editor aberto disputando a placa.

## 10. Mundos vivos: o plano (proposta de 07/10/2026, antes do código)

Pedido do Felipe: `WorldForge/PEDIDO_MUNDOS_VIVOS.md` (vindo da sessão de arte),
mais as ideias de `WorldForge/IDEIAS_DIRECAO_DE_ARTE.md`.

**Como foi feito:**
- três pesquisas independentes: relevo e água; camada humana; crítico de
  jogabilidade de RTS;
- um crítico que confrontou as três;
- o estudo dos 195 mapas 1v1 do ranqueado do ZH;
- as imagens-alvo da arte (`captures/concept/32_world_targets/`).

**O que os dados dizem:**
- **Os mapas que o povo joga são abertos.** Medido nos .map do ranqueado do ZH
  (repositório GeneralsRankedMaps, leitor em `captures/reference/zh_maps.py`):

  | Medida | Mediana | p90 |
  |---|---|---|
  | Penhasco | 3,6% | 6,5% |
  | Água dentro do mapa | 1,8% | 10% |
  | Relevo (desvio padrão) | 5,7 m | — |
  | Árvores | 6,7 por hectare | — |
  | Prédios civis | 8 | — |
  | Poços de petróleo | 3 | — |

  - Ponte aparece em só 21 dos 195 mapas.
  - O mais jogado de 2023 (Snowy Drought, 5,6% das partidas) não tem água e
    tem 3,8% de penhasco.
  - O tamanho mediano do 1v1 é 671 × 792 m.
- **Lição:** o miolo jogável fica aberto como o dos mapas competitivos. A
  beleza AAA vem de três coisas:
  - a **moldura** (mar, serra, dunas), fora da área de jogo;
  - poucos **marcos** grandes;
  - um **chão com motivo** (água, gravidade, vento, gente).
- **Alvo de composição** (imagens do GPT): moldura → espinha → centro. Serra
  e costa emolduram, o rio ou o vale atravessa, e a vila e as pontes ficam
  onde a espinha cruza a estrada.

**Por que hoje sai "gerado":**
- `MakeLevels` faz o penhasco cortando um ruído por limiar, e cada parede é
  uma curva de nível (as "minhocas").
- As montanhas internas são os 10% mais altos de um ruído "ridged", que já
  nasce como uma rede de cristas finas.
- Todo degrau tem 7 m.
- A água é um nível só.
- A borda do mapa é um muro.

**Decisão do Felipe (07/10): tudo é tipo e variação de mapa, nunca regra pra
todo mapa.** "Minecraft não tem ravina em toda planície." O gerador sorteia um
**tipo** (ou o jogador escolhe), e o tipo decide quais elementos existem e em
que medida. Os eixos do sorteio:

- **Borda:**
  - nenhuma (planície até a borda);
  - serra;
  - dunas;
  - mar de um lado;
  - mar em dois lados;
  - metade do mapa de mar;
  - ilhas.
- **Mar:**
  - opcional e **jogável**, porque o jogo terá guerra naval;
  - no tipo "ilhas", cada base fica numa ilha e só se chega de avião ou
    barco;
  - as conferências de jogo dependem do tipo: num mapa de ilhas não se exige
    ligação por terra, e sim acesso por mar e ar igual pra todos.
- **Relevo:**
  - planície aberta;
  - colinas;
  - mesas e monólitos;
  - cânion;
  - serra interna;
  - escarpa que divide.
- **Água doce:** nenhuma; lago em bacia; rio; braço de mar (ria); delta.
- **Marco:** nenhum, monólito, oásis, ruína, vila.
- **Uso do solo, por bioma:**
  - leitos secos e salinas no deserto;
  - fazendas no temperado;
  - petróleo e carcaças.
- **Prédios:**
  - entram como dado (posição, tamanho, guarnição) e aparecem como caixa
    provisória;
  - os modelos do Felipe substituem as caixas depois, sem mudar o lugar.

Os números do ZH (penhasco ~4%, água ~2%) são o **ponto de partida dos tipos
competitivos**, não teto pra todo mapa. Um tipo "ilhas" ou "metade de mar"
tem metade de água por definição.

**Simetria.** Nos mapas de 2 jogadores, a metade do jogador 2 é a do jogador 1
**girada meia volta (180°)** em torno do centro: o mapa virado de cabeça pra
baixo. A alternativa é o **espelho**, a metade refletida como num espelho,
esquerda ↔ direita. A diferença importa por causa da câmera, que olha sempre do
mesmo lado. No giro, um monólito "na frente" da base de um jogador fica "atrás"
da base do outro. Decisão: manter o giro (casa com a luz, com os conceitos em
diagonal e com 4 jogadores) e conferir a oclusão e a sombra por jogador. O
espelho fica como opção de tipo (o rio no eixo).

**Decisões técnicas (com o motivo):**
- **Relevo por formas com corpo**, não por limiar.
  - Fontes: arquétipos como os templates do Azgaar; campos com nome como o
    Minecraft 1.18; formas com campo de distância.
  - Os níveis discretos passam a ser os degraus das formas.
  - Penhasco só existe no contorno de uma forma.
  - Regras de corpo: raio inscrito ≥ 20 m, abertura com r = 12 m, compacidade
    ≥ 0,25.
  - A **cuesta** (platô com um lado em penhasco e o outro em encosta larga)
    vira a rampa natural.
- **Drenagem** com Priority-Flood (Barnes 2014) e D8 aplicados na forma
  grande, antes dos degraus. Ela dá os wadis, a bacia central (oásis ou
  salina) e os lagos em bacia.
  - "Rio primeiro" puro (Génevaux 2013) falhou na prática (Red Blob 1724).
  - Simulação de erosão (Cordonnier 2016) não serve nesta escala.
- **Monólitos em três classes,** pela faixa de chão que a câmera real esconde
  (0,3–1,1 × a altura):
  - marco de 30–55 m só sobre chão não jogável;
  - médio de 12–20 m com silhueta da unidade;
  - pequeno abaixo de 6 m.
- **Estradas e assentamentos no núcleo, em C++** (`GRoads` sai do editor),
  pra o rascunho ficar igual à qualidade real.
  - PCG no máximo pra vestir: no commandlet sem placa de vídeo ele não é
    confiável.
- **Prédio só com modelo.** Caixa provisória à vista seria reprovada. A vila
  existe como dado (e camada de depuração) até haver modelos.

**Invenção: planta de composição com contrato de câmera.**
- O arquétipo do mapa diz: a borda, o marco, as formas grandes, as médias por
  jogador e o % de chão aberto.
- A ordem é marco → rotas (a curta pelo centro, a longa pelo flanco; Dormans
  2010) → relevo. O gargalo vira um vão desenhado entre massas, e não um acaso.
- Cada objeto alto tem pegada de oclusão (pela câmera real) e de sombra (pelo
  sol do bioma). As duas caem em chão não jogável ou entram no orçamento do
  jogador.
- Boletim com consertos locais: encolher a massa, mover o monólito na órbita,
  puxar o tom do chão. No máximo 2 passadas.
- Nenhum gerador de RTS confere oclusão de visão.

**Fases:**

| Fase | O que se vê | Esforço / risco |
|---|---|---|
| 1. Tipos de mapa, borda viva e chão com motivo | O sorteio de tipo; a borda conforme o tipo (planície aberta, serra, dunas, costa) no lugar do muro; fim das minhocas (filtro de forma nos níveis, fim das cristas "ridged"); wadis pela drenagem; bacia com oásis ou salina; pedras assentadas com tálus | 1–1,5 semana / baixo |
| 2. Relevo com corpo e marco | Arquétipos (AlUla primeiro): mesa, cuesta, monólito, serra; marco central icônico com ≥3 passagens de ≥2 tanques; contrato de câmera | ~2 semanas / médio |
| 3. Gente sem prédio | Estradas no núcleo; muros, cercas, postes, oleoduto; campos em faixa e pivôs; poços de petróleo; carcaças nos gargalos; salinas | ~1,5 semana / baixo-médio |
| 4. Água como tipo de mapa | Mar jogável (um lado, metade, ilhas), ria, rio no eixo, lago em bacia, com pontes e vaus | ~2 semanas / alto |
| 5. Vilas e cidade | Como dado com caixa provisória; os modelos do Felipe substituem depois | — |
| Depois | Mais formatos de mapa (ilha, península, vale entre serras, interior com lago, delta, cânion), pelo mesmo sistema de arquétipos | — |

**Conferências automáticas (aceite):**
- Área e relevo:
  - penhasco ≤ 6%;
  - água dentro da área jogável ≤ 10% nos tipos de terra (nos tipos de mar, pelo tipo);
  - ≥ 70% passável;
  - nenhum platô com raio inscrito < 20 m;
  - 1–2 formas grandes, cada uma ≥ 3× a mediana das outras.
- Caminhos:
  - gargalos ≥ 2 tanques (12 m);
  - 2ª rota com ≤ 50% de chão em comum e ≤ 1,3× o comprimento;
  - recursos à mesma distância pelo caminho (±3%).
- Câmera e cor:
  - oclusão ≤ 3% do mapa e igual entre jogadores (±5%);
  - cor das facções contra o chão: contraste 3:1 ou ΔE ≥ 20.
- Água (por tipo): com terra entre as bases, sem pontes elas continuam ligadas e o rush fica ≤ 1,5×; no tipo ilhas, o acesso por mar e ar é igual pra todos.
- Custo e tempo:
  - rascunho ≤ 0,7–1,0 s;
  - Ultra ≤ +1,5 ms por fase;
  - folha lado a lado com as imagens-alvo e um vídeo em cada entrega.

**Fontes principais:**
- Relevo e mapas:
  - [Minecraft, geração de mundo](https://minecraft.wiki/w/World_generation)
  - [Azgaar, templates de relevo](https://github.com/Azgaar/Fantasy-Map-Generator/wiki/Heightmap-template-editor)
  - [Red Blob, mapas poligonais](https://www.redblobgames.com/maps/polygon-map-generation/)
  - [Red Blob, "rio primeiro" que falhou](https://www.redblobgames.com/x/1724-elevation-from-rivers)
- Água:
  - [Barnes 2014, Priority-Flood](https://arxiv.org/pdf/1511.04463)
  - [Génevaux 2013](https://www.cs.purdue.edu/homes/bbenes/papers/Genevaux13ToG.pdf)
- Estradas e vilas:
  - [Galin 2010, estradas](https://perso.liris.cnrs.fr/egalin/Articles/2010-roads.pdf)
  - [Emilien 2012, vilas](https://perso.liris.cnrs.fr/egalin/Articles/2012-villages.pdf)
  - [Ghost Recon Wildlands no 80.lv](https://80.lv/articles/procedural-world-building-in-ghost-recon-wildlands/)
- Jogabilidade:
  - [OpenRA, gerador de mapas](https://github.com/OpenRA/OpenRA/blob/bleed/OpenRA.Mods.Common/Traits/World/ClassicMapGenerator.cs)
  - [Togelius 2010](https://pure.itu.dk/en/publications/multiobjective-exploration-of-the-starcraft-map-space/)
  - [Liapis 2013](https://mail.antoniosliapis.com/papers/generating_map_sketches_for_strategy_games.pdf)
  - [mapas ranqueados do ZH](https://github.com/TheSuperHackers/GeneralsRankedMaps)
  - [Locomotor.ini do ZH](https://github.com/TheSuperHackers/GeneralsGamePatch2/blob/main/GeneralsZH/Data/INI/Locomotor.ini)
- Unreal: [Water Body](https://dev.epicgames.com/documentation/en-us/unreal-engine/water-body-actors-in-unreal-engine)

### Fase 1, andamento (07/10/2026, madrugada)

- **Fim das "minhocas":**
  - As montanhas do miolo eram os 10% mais altos de um ruído "ridged" (rede de cristas). Agora são maciços, de ruído liso com domain warp.
  - O contorno dos platôs é suavizado em arcos (fluxo por curvatura, ~16 m).
  - **Regra de corpo** (`KeepBody`): cada platô ou maciço precisa caber um círculo de 28 m de raio, com compacidade 4πA/P² ≥ 0,25. Senão desce de nível.
  - Abertura com raio de 12 m e fechamento com raio de 8 m.
  - O chão aberto foi de ~70% para 70–73% nas 5 sementes de teste (`captures/worldforge/body_ab2.jpg`).
- **Cuesta:**
  - A borda de platô vira encosta larga (~30 m, ~13°, passável sem rampa) de um lado e paredão do outro. Some o contorno fechado e fino.
  - A direção é radial, girada até ±80° por ruído largo, e por isso a mesma em toda a órbita da simetria.
  - Não vale perto das bases.
- **Maciço com estratos e tálus:**
  - Camadas de ~5 m com patamar.
  - Saia de tálus de ~20 m: pedra e cascalho no chão, perfil côncavo, verde no pé (a água escorre ali).
- **Borda por tipo** (`FWorldGenSettings::Frame`, campo "Borda" no painel, sorteada pela semente):
  - os tipos: serra, colinas (dunas no deserto, mata no resto), costa (mar em dois lados opostos) e ilha;
  - a costa tem baías e pontas em ~140 m, falésia nas pontas e ilhotas;
  - o fundo desce até ~45 m na borda do mapa (plataforma e talude, com ruído).
- **Leitos secos:**
  - Drenagem (D8) sobre o relevo suavizado (~10 m): os canais se juntam, sem riscos paralelos.
  - Canal raso de 0,4–0,9 m onde a bacia acima passa de ~0,6 ha, nunca abaixo de 0,4 m sobre a água.
  - No deserto viram cascalho.
- **Bacias do deserto:**
  - O lago mais central vira **oásis**: água, anel denso de árvores de copa verde, capim verde fechado e terra úmida.
  - Os outros secam em **salina**: chão plano e jogável, crosta clara mas não branca (o branco apagaria a USA), borda suave.
- **Terra seca de verdade:** longe da água, o chão nunca desce a menos de 0,35 m da cota d'água. Antes saíam poças no pé dos penhascos.
- **Água (qualidade real):**
  - Single Layer Water (`ocean_material`), com absorção e espalhamento por canal por tipo de mar (Pope & Fry, Jerlov) e Specular 0,255.
  - Rugosidade de 0,12 a 0,35 com a distância (Cox–Munk).
  - Opacity = espuma. O teste "escuro" de 05/10 era o Opacity padrão 1, que apaga a água.
  - Textura da água por mapa (`MakeWaterInfoTexture`: distância com sinal até a linha d'água, e profundidade). Dela saem a espuma na linha d'água, a arrebentação rala e a areia molhada do chão.
  - O plano de água cobre o mapa inteiro (antes eram 260 m: o lago do mapa inteiro ficava sem água) e, com borda de mar, vai até o horizonte.
- **Montagem:**
  - A instância de material do chão entra na criação do Landscape, com a textura vazia que é preenchida depois.
  - Trocar o material com o Landscape pronto apagava as camadas, porque sem placa de vídeo o Landscape não as refaz.
  - O Nanite do chão é montado uma vez só, depois do material.
- **Custos:**
  - Gerador: ~0,6 s por mapa.
  - Deserto inteiro com costa: 20–25 ms por quadro na vista de jogo (a placa dividida com o BlastLab em parte das medições).

### Fase 1, revisões de arte (07/10/2026, noite)

Rodadas com a sessão de arte conceitual como diretora de arte, contra os alvos
(`captures/concept/32_world_targets/`). Folhas em `captures/worldforge/rev*_sheet.jpg`.

- **Oásis:** só no lago grande (≥ 2.400 m²; o pequeno dava 3–6 árvores sem leitura de marco).
  - Árvores até 0,25 m da água, densidade em degradê.
  - Margem orgânica: ruído de ~12 m, mais larga no lado baixo.
- **Salina:** só de lago grande, com textura gerada (placas poligonais com cristas brancas, creme e não branco).
  - Forma: distância com sinal até a borda (`GSaltDist`), mais lóbulos de ~40 m e recorte de ~9 m. Assim a borda deixa de seguir a grade.
  - Borda em "renda": placas de crosta com borda nítida por cima da lama, em vez de degradê.
  - Em volta, uma faixa de ~15 m de lama úmida. A umidade vai no canal A da textura da água, e o chão escurece ~40% e fica mais liso (Lekner & Dorf 1988; Lagarde 2013).
  - No miolo, manchas de crosta fina e úmida (~35 m).
  - Nada espalhado em cima (pedra na salina lia como sujeira).
- **Lagoa seca pequena:** vira barreiro (`M.Basin == 3`), com a mesma forma da salina e metade da crosta. Antes era um disco oliva.
- **Pedras e capim:**
  - As pedras ficam assentadas: base medida em 7 pontos, inclinação da encosta, 25% enterradas.
  - Pedra solta só onde a gravidade e a água levam (pé de escarpa, tálus, leitos). Fim da "pele de onça".
  - Capim seco nos leitos e no pé das pedras, quase nada no areal aberto.
- **Areal:** marolas de vento de ~35 m e até 0,7 m (cristas transversais ao vento) para dar escala média de relevo com o sol baixo.
- **Mar:** a espuma é fina e quebrada; a arrebentação só no mar (B da textura da água), no lago só um fio.
- **Inverno com mar:**
  - O mar não congela; o lago sim, com o gelo 5 cm acima da água.
  - Água polar clara (Jerlov I–II): azul-marinho no fundo e turquesa no raso, como nos alvos. O verde costeiro de antes virava um caldo.
  - O fundo e a faixa que a onda lava (até ~1 m acima da água) ficam cascalho e pedra, sem neve.
  - As pedras submersas perdem a neve: a cota d'água vai por instância (`AddInstances`, custom data) e o material corta a neve abaixo dela.
  - **Gelo na costa:** placas e cacos na faixa de ~10–30 m da costa, no material da água (nomenclatura da OMM: "brash" < 2 m, placas < 20 m). Voronoi em duas escalas, halo turquesa na borda (gelo submerso). É o elemento-assinatura dos dois alvos de neve.
  - **Lago congelado:** azul-claro com trechos limpos pelo vento e rachaduras quebradas. O gelo preto de antes virava buraco azul-escuro.
  - O cascalho só aparece onde o vento varre a neve: cristas e bordas convexas (Winstral 2002, índice Sx). A mancha de ruído no plano lia como sujeira.
  - A neve pisada das bases tem menos lama.
- **Drenagem de verdade (`BuildLayers`):**
  - Antes do D8, as depressões são enchidas (Priority-Flood+ε, Barnes, Lehman & Mulla 2014).
    - As saídas são a borda, a água e as bacias fechadas do deserto.
    - O ε varia com ruído pra água serpentear no trecho enchido.
    - Sem isso a água parava em cada buraquinho e os leitos saíam em pedaços.
  - **Leito** (`M.Bed`, o fundo marcado pelo gerador):
    - de rio efêmero, largo e raso, de fundo chato;
    - 4–18 m de largura pela área drenada (largura ∝ vazão^0,5, Leopold & Maddock 1953), 0,3–0,8 m de fundo, barranco de ~3,5 m (o tanque passa);
    - serpenteia ±6 m em volta da linha do D8.
  - **Por clima:**
    - deserto: leito de cascalho, mais claro que o chão (o leito ativo não tem verniz), com arbustos;
    - neve: riacho congelado até o lago;
    - floresta: córrego de 2–6 m.
  - O tom do chão vai no A da textura da água: 128 é neutro, abaixo é úmido, acima é lavado.
  - O fundo do leito do deserto é areia lavada com pouco cascalho, ~45% mais clara: com cascalho cheio o leito sumia na vista geral.
  - A borda do leito é lida bilinear; pelo vértice mais perto saía em dente de serra de 2 m.
- **Água no deserto:** o controle "Água" diz o quanto o deserto é úmido. Acima de ~55%, mais lagos grandes ficam oásis, do centro pra fora. Antes, mais água fazia lagos maiores, logo mais salina e menos água; o teste da interface pegou isso.
- **Salina:** textura com ladrilho de 16 m e placas de 1,3–3 m em duas grades. As manchas úmidas largas saíram da textura (repetiam em grade); a umidade larga vem do tom do chão.
- **Pátio de campo de recurso (deserto):** terra batida com cascalho e borda roída. O disco liso lia como poça.
- **Floresta:**
  - O tom do chão aqui é a umidade do capim (`GroundToneAt`): seco, cor de palha, nas clareiras abertas longe da mata e da água, em manchas de ~60 m; viçoso nos córregos e a até ~12 m da água.
    - "Perto d'água" é medido pela distância. Pela altura, o mapa plano inteiro contava como margem e o capim saía de um verde só (visto no `debug_maps`, imagem `tone.png`).
  - O folhiço fica só onde a copa fecha, em manchas de ~15 m, mais escuro e pouco saturado. A borda larga em volta de todo bosque virava um anel laranja igual, como contorno de seleção.
- **Salina:** sem o anel de cascalho das pedras (a pedra não vai na salina). Sozinho, ele virava manchinha marrom solta na crosta.
- **Fundo do mar:** só pedra grande; o miúdo virava confete.
- **Ferramenta de depuração:** `Tools/debug_maps.py` (o gerador sem montar nada, 2 s) grava em `captures/worldforge/debug_<mapa>/`:
  - relevo, bacias, leitos e pesos das camadas, 1 m por pixel;
  - `PROFILE=x0,y0,x1,y1` imprime a altura ao longo de uma linha.

  Foi ela que mostrou que o "degrau fino" das fotos era um platô de 7–8 m sem hierarquia (topo e pé da mesma cor). Isso fica pra fase 2.

## 11. Ferramenta de verdade: ver tudo e editar tudo (plano de 07/10/2026)

**Pedido do Felipe (07/10):** a tela de montar tem que mostrar o mundo de
verdade (estradas, chão, água, pedras, árvores), como o WorldBuilder do C&C.
O gerador faz o primeiro rascunho e a pessoa mexe peça por peça: apaga uma
árvore, pinta o chão, troca a textura e o traçado da estrada. Ordem combinada:
1. vista ao vivo real;
2. peças editáveis;
3. importar um mapa do Zero Hour pra comparar;
4. só então a fase 2.

**Pesquisa** (2 agentes; fontes no fim):
- **Os estúdios não guardam o gerado.** Guardam o que entra no gerador:
  - semente e controles;
  - máscaras pintadas e linhas desenhadas;
  - deltas de objeto presos a uma identificação estável.
- **Exemplos:**
  - Horizon Zero Dawn: mapas pintados e regras na GPU;
  - World Machine/Gaea: vetores e máscaras;
  - edit layers do Landscape: a mão numa camada própria;
  - PCG Data Override da 5.8: apagar, mover e inserir por hash de posição com tolerância de 10 cm;
  - Wildlands: estrada desenhada ou apagada à mão vira entrada.
- **A vista ao vivo vem de refazer só o pedaço que mudou, já com o visual final.**
  - Horizon gera por pedaço do mundo.
  - O PCG tem geração hierárquica.

**Decisão:** continua um app (não um modo do editor da Unreal), com **uma vista só**. O rascunho passa a usar o mesmo chão, a mesma água e as mesmas malhas Nanite do "qualidade real". O Landscape fica só como exportação (o acabamento final da luz e do Nanite do chão), porque não dá pra criar nem editar Landscape fora do editor (`#if WITH_EDITOR`).

**1. Vista ao vivo real:**
- **Lógica pura do módulo de editor passa pro módulo do app**, com um contexto por mapa no lugar das variáveis globais, em `ParallelFor` e numa tarefa fora da thread do jogo:
  - rede de estradas;
  - máscaras (salina, mar, oásis, copa, anel de pedras);
  - pesos do chão (`DesertWeights`) e `SliceHeight`;
  - espalhamento (`SliceScatter`/`SliceScatterMeshes`);
  - textura da água, como textura transitória.
- **Chão:** malha de 1–2 m com a altura de verdade (dunas, corte das estradas). Os pesos das 6 camadas vão em 2 texturas de 0,5–1 m e o material `M_LiveGround_<bioma>` é gerado pelo `build_slice.py` com o mesmo código das camadas do Landscape, mudando só a fonte dos pesos. O raio do mouse, os objetos e o minimapa passam a usar a mesma altura.
- **Água:** o `M_Ocean_<bioma>` com a textura da água feita em runtime (espuma, gelo, areia molhada).
- **Objetos:** em blocos de 64 m com distância de corte e criação espalhada em vários quadros:
  - pedras, árvores, palmeiras, capim (mais ralo na vista ao vivo), paredões;
  - decalques de pneu e pintura da pista.
  - A transparência das árvores volta a varrer os blocos.
- **Luz por bioma igual à do "qualidade real".**
- **Uma fonte de regra:** a tabela de espalhamento por bioma (malhas, espaçamento, corte) sai do Python pra um arquivo de dados lido pelos dois.
- **Meta:** mapa novo em poucos segundos; edição local refaz só os blocos tocados.

**2. Peças editáveis (o arquivo do mapa é a verdade):**
- O arquivo guarda:
  - semente, controles e versão do gerador;
  - camadas de intenção na grade de 2 m: desvio de altura, pintura por camada do chão, densidade de árvore, "proibido";
  - vetores: estradas com tipo e textura (depois rios e rampas);
  - deltas de objeto: apagar, mover, inserir.
- **Identificação estável:** hash da etapa, da célula do sorteio e do índice local, nunca a posição na lista.
  - Ao regerar, casa pela identificação. Se não achar, procura pela posição com tolerância.
  - O que sobrar aparece numa lista de "edições sem alvo", nunca some calado.
- **Estrada tocada vira do usuário:** o gerador para de traçar e passa a usá-la como entrada (escava, pinta, afasta árvore).
- **Edições dobradas pela simetria** (desligável por edição). Tem desfazer e refazer.
- **Ferramentas, primeira leva:**
  - selecionar, apagar e mover objeto;
  - pincel de camada do chão;
  - estrada: desenhar e editar pontos, trocar tipo e textura;
  - pincel de altura: subir, baixar, aplainar, suavizar;
  - pincel de densidade de árvore e pedra.

**3. Mapa do Zero Hour:**
- O leitor (`captures/reference/zh_maps.py`) vira um conversor `.map` → `FWorldMap`:
  - o relevo vira o terreno;
  - as texturas de tile viram as nossas camadas;
  - os objetos viram os nossos modelos;
  - os prédios viram caixa provisória;
  - a água vira a nossa;
  - o que não tem suporte sai.
- Depois passa pela mesma vista ao vivo e pelo "qualidade real".
- Serve pra estudo e teste: o mapa tem dono (EA ou o autor), então não vai pro jogo.

**Armadilhas conhecidas:**
- Trocar a semente deixa órfãs as edições de objeto (a pintura e os vetores sobrevivem), e isso aparece na tela.
- Mudar o gerador muda o chão debaixo das edições, por isso a versão vai gravada no mapa.
- Apagar 100 árvores à mão é intenção de sistema e vira máscara.
- Duas vistas (ao vivo e Landscape) podem divergir: comparação A/B automática a cada `PipelineVersion`.

**Fontes:**
- HZD: GDC 2017, van Muijden.
- Far Cry 5: GDC 2018.
- Ghost Recon Wildlands: 80.lv.
- Unreal 5.8: notas de versão, PCG Editor Mode, PCG Data Override (`Engine/Plugins/PCG/.../PCGDataOverride.h`) e Landscape Edit Layers.
- Houdini: atributo `id` e bake.
- Layout do World Machine e nó Mask do Gaea.
- WorldBuilder do ZH, editor de terreno do SC2 e editor do AoE2 DE.
- No Man's Sky Origins: versão do gerador congelada por mundo.

### 11.1 Vista ao vivo: andamento (07/10/2026, tarde)

- **`FWorldDetail`** (`Source/WorldForge/Public/Generation/WorldDetail.h`): a lógica do mundo de verdade saiu do módulo de editor pro do app, com uma instância por mapa no lugar das variáveis globais.
  - Entram: estradas, máscaras, pesos e tom do chão, altura final, espalhamento (`ScatterCandidates`/`PlaceMeshes`), textura da água e pesos em textura.
  - O editor usa uma instância (`GD`).
  - Conferido pixel por pixel com o `debug_maps` e pela contagem do espalhamento: igual ao de antes.
- **`FWorldLive`** (`WorldLive.cpp`): na mesma tarefa da geração, monta os pesos do chão (1 m), a textura da água (2 m), o espalhamento e a altura final, gravada no próprio mapa. Assim o mouse, as árvores e o minimapa usam a altura que se vê.
- **Chão e água de verdade na tela de montar:**
  - `M_LiveGround_<bioma>` é o mesmo código das camadas do Landscape, mudando só a fonte dos pesos (2 texturas).
  - A mistura pela altura é a conta da própria Unreal (`MaterialExpressionLandscapeLayerBlend.cpp`, LB_HeightBlend).
  - Mais o `M_Ocean_<bioma>` e as camadas de depuração por cima.
  - `forge.ps1 slice live` monta só esses materiais.
- **Espalhamento de verdade:**
  - O Python grava `Saved/WorldForge/live_<bioma>.json` (tipos, malhas, materiais, caixas) a cada mapa inteiro, ou só as tabelas com `forge.ps1 slice manifest`.
  - O app sorteia e assenta com as mesmas regras e cria as instâncias em blocos de 128 m, com distância de corte.
  - Capim e sub-bosque ficam 1,5× mais espaçados na vista ao vivo.
  - O rascunho das árvores e pedras sai quando há o de verdade.
- **Tempo:** os pesos do chão do espalhamento e da altura vêm de uma grade de 1 m (bilinear, ~50 ns contra ~3 µs da conta exata). O "qualidade real" usa a mesma grade, então as duas vistas sorteiam igual. O mapa de deserto ao vivo foi de 13 s pra ~6 s, e o objetivo é ~2 s.
- **Palmeiras:** Nanite com área preservada (a técnica de folhagem). Sem isso, a folha em fita sumia de longe e a palmeira virava palito na vista do app.

### 11.2 Peças editáveis: andamento (07/10/2026, tarde)

- **`FWorldEdits`** (`Generation/WorldEdits.h`): as edições à mão por cima do gerador, em JSON (`Saved/WorldForge/world_edits.json`, ao lado dos controles).
  - **Peça apagada:** tipo e ponto sorteado, com busca de tolerância de 10 cm (chave espacial, como o Data Override do PCG). O sorteio pula o que foi apagado, na vista ao vivo e no "qualidade real".
    - `Tools/test_edits.py` confere pelo caminho do "qualidade real": 234 palmeiras; apagando todas sobram 0, apagando metade sobram 117.
  - **Pintura do chão:** células de 1 m com camada e força. Entra por último nos pesos do chão, então vale pra vista, pro sorteio e pro Landscape.
- **App:**
  - Seção EDIÇÃO no painel, com as ferramentas Olhar, Apagar peça e Pintar chão.
  - Pintar chão tem a camada com os nomes do bioma e pincéis de 3, 6, 12 e 24 m.
  - Ctrl+Z ou o botão Desfazer; a pincelada desfaz inteira.
  - O apagar pega a peça de melhor acerto (distância dividida pelo tamanho): o clique na copa pega a árvore, não o capim.
  - A pintura refaz só o retângulo da textura de pesos (`UpdateTextureRegions`).
- **"Qualidade real" com edições:** o app grava as edições e o caminho delas vai no JSON dos controles. A prévia guardada ganha a marca das edições no nome, porque o mesmo mapa com edições diferentes é outra prévia.
- **Semente nova (ou bioma ou tamanho) zera as edições.** Os outros controles mantêm.
- **Pincel de altura:** Subir, Baixar e Aplainar, segurando no mapa. Soma um desvio por vértice do mapa (`FWorldEdits::Height`), aplicado no `Setup` antes das máscaras e das estradas; no app refaz só o pedaço da malha, as peças em cima e os pesos em volta.
- **Estradas à mão (ferramenta Estrada):**
  - Clique no chão põe pontos; Enter ou duplo clique termina; Backspace tira o último; Esc cancela. A prévia é o traçado suavizado de verdade (`FWorldDetail::UserRoadLine`).
  - Clique numa estrada escolhe (do gerador ou desenhada): os pontos brancos arrastam o caminho; Terra/Asfalto troca o tipo; Delete apaga.
  - A estrada do gerador vira desenhada na primeira mudança: os pontos de controle saem do traçado dela por Douglas-Peucker (1,5 m), e ela é tirada por um ponto dela longe das outras (`RemovedRoads`).
  - Entra na rede do gerador (`BuildRoads`, etapa 4b): escava, aterra, entronca a até 12 m de cada ponta; as pontas que entroncavam numa estrada tirada entroncam na que ficou ali.
  - Cada mudança refaz o mundo ao vivo (~4 s, fora do game thread); o desfazer de uma edição feita antes disso volta nos dados e refaz.
  - Na floresta só tem terra (o chão de lá não tem asfalto).
  - A ponta solta da desenhada termina em ~4 m; a da do gerador (que entra na base) continua se desfazendo em 45 m, mesmo depois de virar desenhada. Com os 45 m, uma estrada desenhada de ~40 m sumia inteira nos buracos.
- **Mover peça (ferramenta Mover):** arrasta pedra, árvore ou moita; R gira 15° (Shift+R pro outro lado). A edição guarda o ponto sorteado (a mesma chave do apagado), o lugar novo e o giro (`FWorldEdits::Moved`). O sorteio põe a peça no lugar novo e assenta de novo pelas regras dela; a movida não passa pelas regras de pista, salina, fundo d'água e chão desnivelado (fica onde o jogador pôs).
- **Pincel refaz o pedaço:** ao soltar o pincel de chão ou de altura (e no desfazer deles), a altura final e as peças daquele retângulo são sorteadas de novo com o chão novo (`FWorldLive::ScatterRegion` + `ReplaceLiveRegion`). Dá certo porque o sorteio é por célula (a semente é da célula): o pedaço sai igual ao que o mapa inteiro daria. Desfeita a pincelada, voltam exatamente as mesmas peças (o teste confere a contagem).
- **Mapas guardados levam as edições:** "Guardar na lista" grava as edições num arquivo do mapa (`Saved/WorldForge/edits/`); abrir o guardado volta com elas. "Exportar arquivos" põe um `edits.json` na pasta.
  - `Tools/test_edits.py` confere no "qualidade real": a estrada tirada some e a desenhada aparece onde foi desenhada.
- **Teste da interface:** 19 passos, incluindo apagar e desfazer; o pincel tem mais 2, a altura 2, a estrada 12, mover 5 e guardar com edições 2.

## 12. A vila do kit civil (11/10/2026, pedido do jogo, decisão do Felipe)

**O pedido:** a vila da maquete do Look no mapa jogável (World_TestSalt2), com as peças do kit
civil aprovado (`docs/concept/buildings/civil/README.md`): casa de 1 andar 8×8, sobrado 8×8,
casa com varanda 8×12, casa comprida 16×8, muro de 4 m com portão; mais a rua e as miudezas
do Look (carro queimado, picape civil, caixotes, tambores, sacos de areia, munição).
Sem tapar as 4 largadas (~88 m), os 6 depósitos e o caminho entre as bases. A vila passou na
frente das fases 2–4 (Felipe, 11/10). **Ele escolheu: uma vila só, no centro, compacta.**

**Estudo** (agente de pesquisa + revisor, 11/10):
- A rua primeiro, depois os lotes: Parish & Müller 2001 (CityEngine); Emilien et al. 2012,
  "Procedural Generation of Villages on Arbitrary Terrains" (o lote toma um trecho da rua e
  cresce pra trás; usado pela Ubisoft em Ghost Recon Wildlands); Vanegas et al. 2012 (lotes
  por cortes pelo lado comprido).
- Vila de estrada (povoamento linear) que engrossa no cruzamento; casa virada pra dentro,
  quintal murado com portão (Hakim, "Arabic-Islamic Cities: Building and Planning Principles").
- Jogo: passagem principal ≥ 16 m; nada de passagem de 8–12 m (engarrafa); beco só sem saída;
  a vila estreita o caminho mas não fecha, e sobra um desvio; mesmo número de prédios por lado.
- Revisão: a câmera do jogo nasce a 45° e gira (o sobrado escolhido depois do giro, olhando o
  mundo); um platô por quarteirão (muro dividido não fica em degrau); miudeza fora da rua; a
  vila toma a forma do lugar (o lote no lago ou no morro sai); o mapa não é simétrico exato.

**Como ficou** (`Source/WorldForge/Private/Generation/WorldVillage.cpp`, `FWorldDetail::BuildVillages`):
- Controle `Villages` (JSON dos controles: `"villages"`, `"keepout": [[x, y, raio]]`): 0 sem,
  1 **uma no centro** (o padrão pedido), 2 uma por lado (girada como o mapa).
- No centro: a rua reta pelo meio do mapa no eixo da estrada que passa ali (24 m de fachada a
  fachada), transversais de 16 m a ±56 m pros lados; cada lote tem o irmão girado de 180°
  (justo pros dois jogadores) e morre junto. Fileiras de lotes de 8 ou 16 m de frente e 20 m de
  fundo: casa na fachada (recuo de 4 m com pátio murado em ~25%), quintal murado atrás com
  portão, palmeira em 60% dos quintais, pomar murado sem casa em ~12%.
- O sobrado: na esquina e em ~15% das outras, só onde a frente olha pra câmera (yaw 45°):
  esconde o próprio quintal, não a rua. Mesma pegada da casa térrea (a grade não muda).
- Terreno: lote que cai na água, na encosta (> 1/3 acima de 31°), na base ou no depósito, ou
  torto demais (> 5 m), sai; um platô por quarteirão (lote a > 2,5 m dele sai).
- Caminho (grade de 4 m, tanque com uma célula de folga, as regras do jogo): base com base e
  cada depósito com a base mais perto crescem ≤ 10%; com a rua e as transversais fechadas ainda
  há desvio (≤ 60% mais longo); senão sai o lote mais longe da rua (em par) e confere de novo.
- No mundo: o chão do lote aplainado no platô (borda de 3 m) no `SliceHeight`; nada do
  espalhamento nasce no lote nem na rua; o chão do lote é terra batida com cascalho; a estrada
  endireitada pela vila e as transversais de asfalto que viram terra no fim.
- O jogo lê `Saved/WorldForge/exports/<mapa>/village.json` (o "qualidade real" grava): por
  peça `id`, `type`, `x`, `y`, `rot` (yaw = 90 × rot, a frente em +Y no 0), `cell`/`cells` (a
  pegada na grade de 4 m a partir de `origin_m`, já girada), `floor_cm`, `street`, `blocks`.
  O World_TestSalt2 não leva ator de casa (o jogo desenha); a prévia do WorldForge desenha.
- Vista ao vivo: o kit civil do Look (`captures/look/civil_glb/SM_<type>.glb`), importado por
  `forge.ps1 py Tools\import_village.py` em `/Game/Village/` com o mesmo material do jogo (roda as
  funções do `Game/Tools/import_provisional.py`, sem copiar). `AWorldForgeTerrain::BuildVillage` põe a
  malha no centro da peça, no piso, girada 90 × rot; o tipo sem malha cai na caixa provisória
  (12/10: as 250 peças com malha, `captures/worldforge/village_kit/`).
- Ferramentas: `Tools/village_preview.py` (a planta de cima, sem Unreal, com o motivo de cada
  lote que saiu); `forge.ps1 view -Extra '-ForgeSettings=<json> -ForgeShots=village'` (fotos da
  vila de cima e na câmera do jogo).

**Medido no mapa do jogo (World_TestSalt2 com as largadas e depósitos do jogo):** 24 casas (5
sobrados), 250 peças, 29 palmeiras; caminho ×1,11 no pior par, desvio ×1,46; ~0,4 s.

**Montado (12/10, madrugada):** o World_TestSalt2 com a vila (controles em
`Saved/WorldForge/world_test_salt2.json`, `"villages": 1` e o `keepout` das 4 largadas e 6
depósitos do jogo; os de antes em `world_test_salt2.before_village.json`). Hash d7acd936, o
mesmo da depuração (o gerador repete). O jogo religa e refaz o pacote da família.

**Água (pedido do Look, 11/10):** o M_Ocean_<bioma> ganhou `WaterBodyColor` (vetor),
`WaterBodyStrength` (0–1, padrão 0) e `SkyReflection` (0–1, padrão 1), com o padrão igual ao de
antes; a receita da luz ajusta (o lago virava cinza-marrom refletindo a névoa dourada).

**Falta:** as malhas do Look; casa por casa no `FWorldEdits` (apagar, mover, trocar o tipo);
a guarnição (infantaria na casa, como o ZH) e as ideias da revisão (muro que tanque derruba,
poço capturável no cruzamento); o poste no kit do Look.

### 12.1 A vila v3 no padrão da maquete do Look (13/10/2026)

**O pedido:** o Felipe comparou a vila do jogo com a maquete do Look (volta 15,
`captures/worldforge/vila_look_x_jogo.jpg`). Ele disse "o modelo das casas está muito bom, e as ruas" e pediu
pra trazer o que o Look acertou: a estrada um pouco maior, mas a rua da vila um pouco menor que a da maquete
(16–21 m), como nas favoritas. Lagos podiam mudar se precisasse. Depois pediu os postes na direção certa,
com fio bonito e físico, e nunca em cima de rua ou de peça.

**As medidas** (as favoritas contra o casco de 7,9 m do tanque; a 2ª opinião mede a rua pelo vão na grade
de 4 m, sem passagem de 8–12 m):

| | Antes | Agora | Fonte |
|---|---|---|---|
| Estrada fora da vila | 7,5 m | 10 m | a de duas faixas da `road_town_battle` tem ~9–10 m |
| Rua da vila | 7,5 m de asfalto num vão de 24 m | 13,5 m de asfalto num vão de 20 m; transição de 24 m nas entradas | a avenida da `street_battle` tem ~16 m e a maquete 16–21 m |
| Transversal do meio | 7 m (T a ±56 m) | 10,5 m num vão de 16 m, 4 braços | a maquete tem um cruzamento só |
| Travessas nas pontas | — | de terra, 12 m de vão, um T de cada lado (a ±66 m) | |

- **Chão** (`WorldDetail.h`):
  - a beira do asfalto quebrada em ±0,7 m;
  - as línguas de areia que o vento do mapa (`WindYaw`) sopra até ~2,4 m pra dentro da pista, em manchas de ~9 m esticadas no vento;
  - a esquina arredondada (união suave das duas pistas, raio ~4 m);
  - o acostamento curto de cascalho;
  - areia até o muro, com manchas de chão pisado nos lotes;
  - o capim e a moita vão até o pé do muro, enquanto árvore e pedra seguem fora do lote.
- **Pintura:** 3 m pintados a cada 9 m e a borda a 0,55 m da beira, acompanhando a largura de cada ponto.
- **Largura ponto a ponto:** `FRoad::W`.

**A planta** (`WorldVillage.cpp`):
- **Lotes:** ~30% de quintal murado (a esquina, metade das vezes), ~56% de casa solta na areia, ~5% de pomar murado e ~9% de lote aberto.
- **Casa solta:** às vezes ganha a casa dos fundos (8×8, porta de lado) ou um resto de muro de 1–3 peças. Esse resto bloqueia, então é sorteado no lote e espelhado junto com o irmão.
- **Carros:** carcaça e picape no acostamento, quase paralelas à rua (±12°), com uma carcaça na transversal. A pegada tem 2 células e sobram ≥ 12 m de passagem. Não tem mais sacos de areia.
- **Miudeza** (caixotes, tambores e munição do kit, em grupos de 1 grande + 1–2 pequenas): fica no pé do muro, do lado da casa, no acostamento, no quintal da solta e no lote aberto. Não bloqueia e tem sorteio próprio, sem espelho.

**Os lagos:** com a vila do centro, a água dentro de ~80 m do meio vira chão baixo (`MakeBasins`). A borda é
ondulada só com harmônicas pares do ângulo, então a simetria fica. O campo de recurso não cai a menos de
105 m do centro. O lote agora pede 0,3 m acima da água e 6 m de distância da água de verdade (antes pedia
1 m, e o chão baixo da margem recuada matava os cantos do cruzamento).

| | Antes | Depois |
|---|---|---|
| Lotes | 26 de 52 morriam na água | todos vivos |
| Casas | 24 | 50 |
| Peças | — | 330 |
| Miudezas | — | 29 grupos |
| Palmeiras | — | 17 |
| Caminho entre bases / desvio com a rua fechada | — | ×1,11 / ×1,11 |

**Os postes e fios** (`PlaceVillagePoles`, no fim do `Setup`, com a estrada já na cota):
- **Malha:** o `SM_civ_pole` do Look (8 m), com a cruzeta no X local e 4 isoladores a ±0,35 e ±0,95 m, a 7,57 m de altura.
- **Linha:** acompanha a estrada pelo lado −v da planta, a 1,5 m da beira, a cada ~32 m e até 56 m além da vila. A cruzeta fica perpendicular à linha (o Y local na tangente).
- **Onde o poste não entra:**
  - na pista, com folga de 1,2 m;
  - perto da outra pista, com 2,2 m;
  - a menos de 1 m de qualquer peça;
  - a menos de 2,5 m da frente de porta ou portão;
  - a menos de 3 m de palmeira ou árvore;
  - na água ou em encosta.
- **Fio sem obstáculo:** o fio não passa por cima de sobrado nem de copa.
- **Fios:** de isolador a isolador, com flecha de 3,5% do vão. A ligação vai até a parede da frente de metade das casas a até 22 m (3,4 m de altura, ou 6 m no sobrado), com flecha de 2,5%.
- **Na prévia:** o fio é desenhado em 10 pedaços de 4,5 cm, escuros e sem sombra.
- **Bloqueio:** o poste não bloqueia mais. O jogo vai derrubá-lo (`knockable`).

**O JSON v3** (lido pelo jogo, combinado em 13/10; o `pieces[]` de antes continua valendo):
- `yaw_deg` opcional nas peças, que vale no lugar do `rot`;
- `poles[]`: peça, x, y, z, `yaw_deg`, `sockets_local`;
- `wires[]`: `a`/`b` = [poste, isolador] ou `b_wall` + `b_piece`, mais `sag_m`.

**Falta:**
- as malhas do Look (sucata, entulho, cerca, pneus e, na laje, caixa d'água, antena e ar-condicionado), que vão no `z` da laje;
- o tom por casa no material do kit;
- as árvores com id e pegada no JSON (pro derrubar);
- a física do fio no jogo.

### 12.2 O chão da vila: asfalto v2, óleo, oásis verde e o relevo da cratera (13/10/2026, tarde)

- **Asfalto da volta 15 do Look:** rejeitado. O Felipe viu no mapa e disse "senti uma regressão". O tom do
  Look (AG) é da luz da bancada; na `v2_aprovada` ficou lilás e ruidoso. Fica desligado (`ASPHALT=1` liga).
- **Asfalto v2 (aprovado):**
  - É o `ASPHALT_V2_CODE` do Look (`Game/Tools/look_asphalt.py`) por cima do asfalto de antes.
  - Por cima entram os defeitos de pavimento do FHWA:
    - as zonas boa/cansada/abandonada (15–40 m);
    - as trincas em bloco, a longitudinal e as transversais;
    - a beira desagregada;
    - os remendos escuros e foscos de borda recortada;
    - a areia do lado do vento.
  - As trilhas de roda saíram a pedido do Felipe ("não se vê no dia a dia nem nos gameshots").
  - Óleo v4: carimbos raros do atlas `T_LL_Oil` (mancha fosca, grupinho, pingos, poça viscosa) a cada 14 m
    de rua; a poça brilha contra o sol, com `OilPuddle` 0,07 (com 1, todas viram poça, pra foto).
  - Padrão `ASPHALT2=1`.
  - A rua vem da textura `T_RoadDir_<mapa>` (`RoadDirPixels`, texel de 2 m):

    | Canal | O que guarda |
    |---|---|
    | RG | a direção, com o ângulo dobrado e girado de 45° |
    | B | a distância com sinal até o eixo |
    | A | a meia largura |

- **Aérea borrada contra os arcos de pneu repetidos:** rejeitada ("tudo borrado, simples"). O `AerBlur` fica 0.
  - Atenção: o parâmetro do Landscape em tempo de jogo (`SetLandscapeMaterialScalarParameterValue`, a chave
    `-ForgeLandscapeParam`) não pega no chão Nanite. A/B de material sai de duas montagens.
- **Oásis verde (aprovado, `OASIS2=1` padrão):**
  - O chão de 2 a 16 m da água fica verde-oliva escuro em manchas (no `wet` do material, pela distância da WaterInfo).
  - O tufo fino do oásis fica com metade da densidade e a moita compacta (shrub_02) fica mais junta e maior.
  - O Felipe aprovou; eu ainda acho fraco. Ele lê mais como terra escura do que verde, e de perto a moita fica
    espetada. Vale uma rodada com referência de oásis.
- **Relevo da cratera:** o `DISP_CODE` do Look (`look_mf.py`) no pino Displacement do chão.
  - O material leva `bEnableTessellation` e magnitude de 200 cm, com centro 0,5.
  - Usa as mesmas texturas da memória do chão, mais o FarmH (farm_soil_disp, Poly Haven).
  - O `DispOn` (padrão 0) é o jogo que liga.
  - O pino Displacement é escondido no Python, então quem liga é `UWorldForgeEditorLib::ConnectMaterialDisplacement`. `DISP=0` monta sem.
- **Mapa de teste:** `World_TestAsph2` (os controles do TestSalt2, `TAG_SUFFIX=_a2` pra ter materiais próprios),
  pra A/B sem mexer no mapa do jogo.

### 12.3 A água só onde existe água (plano, 13/10/2026; ainda sem código)

**O pedido:** o Felipe viu a água aparecendo pelos buracos do chão (o teste do deslocamento) e perguntou por
que tem água embaixo do mapa: "às vezes fazemos isso por causa do C&C, mas não é a solução mais moderna; não
fique preso a decisões anteriores".

**Hoje:** um plano d'água global numa altura só cobre o mapa inteiro. O lago é a bacia do terreno abaixo dela.

**O estudo** (com fontes; o relatório completo está na conversa de 13/10):
- **O plugin Water da 5.8:**
  - Continua `IsExperimentalVersion` (`Water.uplugin`).
  - Fora do editor só reconstrói o oceano: lago e rio são "baked at save-time" (`WaterBodyComponent.cpp:1578`, `IsBodyDynamic`). Então não serve pro app ao vivo.
  - A escavação do terreno é só no editor (`WaterBrushManager`), e o WorldForge já escava o leito.
  - A Water Info guarda uma altura por texel (sem dois níveis empilhados).
  - Não achei custo publicado na 1660.
- **Os jogos de estratégia:**
  - O ZH guarda um polígono por corpo (`IsWater`/`IsRiver`, `RiverStart`).
  - O AoE IV usa mapa de altura da água, de fluxo e de espuma.
  - O Cities 2 tem simulação, com ~1 ms de render numa 3080.
  - O SupCom usa um nível único e uma rampa de cor pela profundidade.
  - O CoH2 tem rio congelado que quebra.
  - O Croakwood faz a espuma da margem por campo de distância visto de cima (o teste de profundidade falha de cima).

**A recomendação: malha por corpo feita pelo gerador, sem o plugin.**
- **P0 (dados):** cada corpo sai com id, tipo, nível em mm inteiro, contorno e furos; rio com linha, largura, altura (nunca sobe rio abaixo), velocidade e leito; boca de rio ligada ao corpo onde deságua.
  - A grade de 4 m da simulação marca seco, margem, vau (raso) e fundo, com o id do corpo, em conta inteira e conferida no `simtest`.
  - As texturas de 0,5–1 m levam a distância com sinal até a margem, a profundidade, o fluxo XY e o id.
- **P1:** lagos e oásis como malha de grade de 2–4 m cobrindo a máscara, crescida 1–2 células pra a borda ficar embaixo do terreno; o mar recortado pelo que liga à borda do mapa. Sai o plano global.
- **P2:** o rio como fita ao longo da linha (U através, V ao longo), o fluxo no vértice pro material em duas fases (Vlachos, SIGGRAPH 2010); as bocas.
- **P3:** o lago congelado no inverno, com o mesmo polígono, material de gelo e as células passáveis.
- **A divisão:**
  - o WorldForge decide onde e qual a forma (contorno, nível, leito, máscara);
  - o Look faz o material (cor pela profundidade, espuma pela distância, reflexo, `FLookWater`);
  - o jogo trata a água na simulação.

**Os formatos fechados com o jogo e o Look** (13/10). Saem na montagem, em `Saved/WorldForge/exports/<mapa>/`; o `game.ps1 worlds` copia pra `Config/RTS/Grid/<mapa>`.
- **`grid.png` (o jogo):** RGBA 8 bits, uma célula de 4 m por pixel, o canto em −Half (o mesmo da grade da simulação), a linha 0 no −Y.
  - R = 0 seco, 1 margem, 2 vau, 3 fundo, 4 gelo passável;
  - G = o id do corpo;
  - B = 0 e A = 255, reservados (depois, o penhasco e o bloqueado).
  - **A regra da célula:**
    - é água quando metade dos texels de 1 m dela é;
    - é vau quando a profundidade média da parte molhada fica até 0,8 m (o Humvee passa 0,76 m sem kit; o Abrams e o T-72, 1,2 m);
    - é margem quando tem um pouco d'água, ou quando está seca mas encosta (4 vizinhos) numa célula d'água.
  - **No jogo:** o fundo bloqueia a terra, o vau passa a meia velocidade, a margem e o gelo passam.
- **`grid.json` (o jogo):**
  - min_m, cell_m, res e row0;
  - os corpos: id, kind (lake, oasis, river, sea, ice), level_mm e, só no rio, flow_mm_s; mais area_m2 e max_depth_mm, só pra informação;
  - o `hash`: FNV-1a 64 dos pixels em RGBA, mais uma linha ASCII por corpo (`id,kind,level_mm,fx,fy` e a quebra de linha). O jogo recalcula e confere; ele entra no aperto de mão do lockstep.
- **`water.exr` e `T_Water_<mapa>` (o Look):** RGBA16F a 1 m na mesma grade UV do chão.
  - R = a distância com sinal até a margem, em m, + na água, cortada em ±8;
  - G = a profundidade, de 0 a 8 m;
  - BA = a correnteza, em m/s (zero até a P2).
  - A linha d'água é interpolada entre os texels (quadrados marchantes), porque a espuma é mais fina que 1 m.
- **Na malha de cada corpo (P1, o Look):** um material só, com o Custom Primitive Data: 0 = nível em m, 1 = tipo, 2 = turbidez, 3 = escala da espuma.
  - A P1 está no código: `UWorldForgeEditorLib::SpawnWaterBodies` (`SM_Water_<mapa>_<id>`, grade de 2 m crescida 4 m; o mar a 4 m com a saia até o horizonte) e `AWorldForgeTerrain::BuildWaterBodies` (o app). `WATER_PLANE=1` volta o plano.
  - **O custo, medido pelo Look na 1660** (1080p, Scalability 3):
    - o Single Layer Water custa +0,6 ms com um oásis em 15% da tela e +1,3 ms com um lago em 40% (só a etapa SLW, 0,51 ms);
    - a translúcida custa +0,2 e +0,6 ms.
  - **A escolha por preset:** o SLW no alta e no ultra; a translúcida irmã no baixa e no média. O material é por mapa, e a troca é do jogo, na carga, porque o shading model não muda pela chave de qualidade.
  - **Os materiais do Look:** `/RTSLook/Water/M_Water_SLW` e `M_Water_Trans`, com as instâncias `MI_Water_Lake`, `_Oasis`, `_River` e `_Sea`. Até eles existirem, segue o MI_Ocean de hoje.
- **`surface.png`:** o A passou a ser a margem molhada: 255 embaixo d'água, a faixa até ~1,2 m acima da água a até 8 m da margem e a terra úmida do oásis.
- **O código:**
  - `FWorldDetail::BuildWaterField`, `WaterGridPixels` e `WetGroundAt` (WorldDetail.h);
  - `UWorldForgeEditorLib::MakeWaterExports`.
  - Os corpos usam o fundo sem a placa de gelo; o lago congelado vira `ice`.

### 12.4 Fase 3, gente sem prédio (estudo de 13/10/2026; fica pra v3)

**É da v3:** o Felipe, 13/10 à noite: "a v2 já tá muito atrasada, vamos vir com essas propostas na v3". O tanque derrubando poste e fio também passa pra v3. Ficam pendentes duas decisões dele:
- o derrick capturável entra na economia ou é cenário;
- o oleoduto vira barreira que se explode.

**O estudo** (agente, com fontes na conversa de 13/10):
- **O ZH:** o derrick capturável é o único sinal humano com função no jogo (paga na hora e de tempo em tempo). O resto é cenário com nível de esmagamento: `CrushableLevel` e `CrusherLevel`.
- **O CoH:** a cobertura funciona como semáforo; o mapa abre as sebes à mão.
- **O AoE IV, o Manor Lords e o C:S:** nenhum gera talhão por algoritmo.
- **Os papers:** Vanegas 2012 (lotes por caixa orientada) e a T-tessellation (INRAE): quase toda quina de talhão real é um "T".
- **O mundo real:**
  - o pivô padrão tem 400 m de raio, e os da Arábia e de Kufra ~1 km de diâmetro; muitos estão mortos desde 2016;
  - o cavalinho fica a 100–400 m do outro, e o Golfo tem (Wafra);
  - o oleoduto corre reto e vai elevado sobre a rocha, com faixa limpa de 15–23 m (Tapline);
  - os postes de distribuição ficam a 30–90 m; o mourão da cerca, a 3,6–6 m;
  - a faixa de cultivo tem 12–24 m de largura.

**A regra de jogo proposta** (o nível vai no dado da grade de 4 m; a simulação é do jogo):
- 0: só desenho (sulco, plantação, trilha, fio);
- 1: qualquer veículo amassa (cerca de arame ou madeira, sebe baixa);
- 2: só o tanque amassa (poste, muro baixo, carro);
- 3: bloqueia, e cai a tiro (muro alto, oleoduto elevado, cavalinho, carcaça de tanque).

**O que se lê de cima:**
- nada maciço acima de 1,5 m perto de rota;
- o desenho na escala da faixa (12–24 m), não do sulco;
- o verde do campo com ΔE ≥ 20 contra as facções;
- o gargalo nunca abaixo de 12 m.

**O pivô no tamanho do jogo:** de 60 a 150 m de raio, porque o de 400 m tomaria 80% do mapa. Ainda vou conferir em render.

**A ordem:**
1. a camada de uso do solo na grade de 4 m (rótulo, nível e custo);
2. a rede de postes ligada a alguma coisa (vila, bomba do pivô, poço);
3. os campos: faixas por caixa orientada com quinas em "T" e os pivôs;
4. as cercas e os muros no contorno do bloco, com porteira na rota;
5. o petróleo: o derrick e 3–6 cavalinhos com o cano até um tanque;
6. o oleoduto (elevado na rocha, com as rampas; enterrado na areia);
7. as carcaças nos flancos dos gargalos;
8. a salina humana: tanques de evaporação e montes de sal.

**As ideias novas:**
- uma rede coerente: poço → cano → tanque → oleoduto, e vila → poste → pivô;
- o oleoduto como barreira com travessias, que um trecho explodido abre;
- pivôs vivos perto das bases e mortos no meio;
- o campo guarda a batalha (cerca amassada, esteira na plantação).

### 12.5 Otimização do que o WorldForge desenha (plano, 14/10/2026)

**O pedido:** o Felipe, 13/10 à noite, depois da v2: "foco na otimização, otimização perfeita, como os grandes otimizam, nas pegadinhas, nos truques; eu quero a lindeza e otimização".

**O estudo** (agente, com fontes, 14/10):
- **A câmera muda as receitas:** ela vê o chão de 77 a 157 m, e um pixel cobre 3,5 a 7 cm. Receita de primeira pessoa não serve (o WPO Disable Distance e o HLOD rendem pouco).
- **O chão em RVT, o maior ganho:** no Battlefield 3 (GDC 2012), o chão pintado por camadas custava 10–20 ms e caiu pra 2,5–3 ms com textura virtual.
  - **Pra nós:** 32k texels em 1024 m (3,1 cm por texel, o pixel de perto), 30–60 MB.
  - **No RVT:** só o que não muda (areias, zonas, ondulação na normal, asfalto, foto aérea).
  - **Fora dele:** a memória do chão (lagarta, cratera) e a rocha íngreme.
  - **A pegadinha:** no jogo só 2 páginas por quadro (`r.VT.MaxUploadsPerFrame`), e no editor são 32. Sobe pra 16–32, assa os mips baixos e mede no modo jogo.
  - **O cuidado do Look:** o grão de 1,3 m e a ondulação de 22 cm precisam de ~2–3 cm por texel perto.
- **O material do Landscape:**
  - a camada alocada entra inteira no shader, mesmo com peso quase zero;
  - `MaxPaintedLayersPerComponent=4`;
  - a regressão da 5.8 (Edit Layers vaza camadas do vizinho e estoura as 64 texturas).
- **As sombras:**
  - a sombra virtual com cache é o certo (sol fixo, tudo Nanite);
  - `Shadow Cache Invalidation Behavior = Rigid` no que tem vento (a sombra fica guardada e a planta balança);
  - capim e moita baixa sem sombra (contact shadow);
  - o Ultra usa `ResolutionLodBiasDirectional −1,5` (8× páginas).
- **O Lumen:** reflexo só em superfície lisa (`MaxRoughnessToTrace` ~0,3), ~1 ms publicado no Series S.
- **A água:** refração e reflexo a meia resolução, e o corte de refração nos lagos fundos.
- **Pra medir:** `-ForgeProfile -csvGpuStats` (o CSV por passe e o `Tools/gpu_profile.py`), `r.Nanite.ShowStats`, `r.Shadow.Virtual.Stats.Visible`, `stat VirtualTexturing`.

**O estudo do Look** (o quadro inteiro do jogo) fica em `docs/OTIMIZACAO.md`: lá o RVT do chão é o item 4, com o NaniteBasePass estimado de 3,1 ms pra ~1,2–1,5 ms.

**A ordem** (medir antes; A/B na luz do jogo contra o aprovado em cada passo):
1. A medida por passe no World_TestSalt2, a 1080p, na luz do jogo: a vila, o lago e o aberto.
2. Os baratos do WorldForge: capim sem sombra, `Rigid` nas plantas com vento, o limite de camadas.
3. Os da receita, que são do Look e do jogo (o Lumen, a água, a sombra do Ultra): mando os números.
4. O RVT do chão, com o Look revisando o CODE.

**Alvo novo do chão do deserto** (sessão de arte, 11/10, pedido do Felipe):
`docs/concept/scenes/desert_golden_hour/best/close_ground_light_gemini.webp` — areia batida
desbotada sem duna forte, ondulação rasa do vento, marcas de esteira e pneu, seixos e pedras
pequenas espalhados com poucas maiores em grupo, moitas secas baixas e esparsas.

## O chão do deserto, a luz da versão final e os monólitos em malha (12–13/10/2026)

**A luz da versão final** ("ver na versão final"; o painel de montar continua com a luz leve dele, regra do
Felipe pelo Look):
- o plugin de luz do Look (`Game/Plugins/RTSLook`) entra por link: `WorldForge/Plugins/RTSLook` tem o
  `.uplugin` (hardlink) e `Source`/`Content` (junção) do jogo, com `Binaries` e `Intermediate` próprios
  (a mesma DLL nos dois projetos travava a compilação do jogo);
- `AForgeSliceGameMode::ApplyLook` põe um `ALookDirector` com a receita do bioma (deserto:
  `desert_golden_hour`, a bancada do Look; a `v2_aprovada` tem o sol a 30° e a exposição da areia antiga:
  a areia nova estourava). `-ForgeLook=<receita>|none` pro A/B. O pawn do WorldForge implementa
  `ILookCamera`, e a câmera leva a `LocalExposureBlurredLuminanceBlend = 0` do jogo.

**As areias** (o Felipe: "ambas são boas, não é substituível"): `SAND=v2` (padrão, a do Look de 13/10, a "C"),
`SAND=v1`, `SAND=classic` (a de antes) no `build_slice.py`; o material do chão é um só por bioma.
- v2: a v1 do Look (albedo físico 0,49/0,40/0,27, o desenho da textura a 25%) + as zonas (liso, ondulado,
  pedregoso, revirado; o resto é o pisado) + as ondulações simuladas (`T_LL_Ripples`) só na zona delas.
- As zonas saem do terreno, não de textura: o fbm de ruído de valor do Look (hash inteiro) com as regras do
  terreno (pedregoso na encosta e na pedra, liso no pisado, ondulado no aberto e longe do pisado: +0,25 a
  partir de ~45 m, pela grade `FWorldDetail::OpenAt`), a borda rasgada (ruído de 0,8 m antes de um limiar de
  0,03) e pouco tom. O material e o C++ (`FWorldDetail::ZonesAt`) fazem a mesma conta.
- A terra batida e o cascalho derivam da areia (terra = areia × 0,63/0,57/0,50, como na maquete).
- O espalhamento pelas zonas (as fórmulas do Look divididas pela média: o total fica, muda o lugar), todas
  as variações dos conjuntos (antes só a 1ª malha de cada FBX), os tamanhos e tons da maquete.
- Medido contra as 7 favoritas (`captures/worldforge/areias_x_gameshots.jpg`): areia nova + bancada dentro
  da faixa (130–168 contra 132–164 no vermelho); areia nova + a luz de hoje do jogo estoura.
- A vila tem chão pisado contínuo em volta (borda de ruído), sem as cunhas de areia pura.

**O vento do mapa**: a coleção do jogo (`/Game/Materials/MPC_Wind`, a cópia do arquivo com o mesmo GUID;
`Wind` = x, y pra onde sopra, m/s, rajada). Sorteado pela semente do lado certo do sol (a sombra das
ondulações vem assada pra um sol a 155° do vento: 87° − 155° ± 20°), na marca `wind_yaw:`/`wind_speed:` do
mapa e em `Saved/WorldForge/exports/<mapa>/map.json` (o jogo lê pelo `world_info` do Maps.json).

**A memória do chão do jogo** (a esteira afundada, a cratera): a conta do Look (o `CODE` de
`Game/Tools/look_mf.py`, lido na montagem) entra **direto** no material do Landscape do deserto, depois de tudo
misturado (cor linear, normal tangente, rugosidade; a brasa no emissivo), com os nomes de parâmetro que o jogo
põe. Não pela chamada da `MF_LookGroundMemory`: cada regravação da MF recriava as entradas e o material salvo
perdia as ligações ("Missing function input", o chão do jogo em xadrez na madrugada de 13/10). Sem os cadernos
(`ReliefRangeCm = 0`) a conta devolve a entrada; `GROUND_MEMORY=0` monta sem ela. Os vetores entram pela saída
`RGBA` (pela '' chega só o RGB e o `.zw` não compila no SM6).

**As palmeiras e o recorte perto do cursor** (só no WorldForge; o jogo tirou): as originais ficam opacas e a
cópia `SM_x_Fade` leva os materiais `_Fade` (masked); o fantasma usa os da cópia e a luz da original.

**O capim**: os tufos finos viravam riscos escuros e pixel espalhado de cima (o Felipe, 13/10). A luz do tufo
passa a seguir o chão (`NormalUp` no mestre das plantas: a normal da folha puxada pra cima, saindo no mundo e
com o lado de trás pelo `TwoSidedSign`), o tom dourado da maquete, as variações e menos tufos no oásis.

**Os monólitos em malha** (a 2ª opinião de 12/10: o heightfield não faz pescoço, lábio nem arco):
`Tools/blender_monoliths.py` (CPU) gera o kit provisório (6 pilares e o marco) em `captures/worldforge/monoliths`
e a pegada de cada peça em `Config/WorldForge/Monoliths.json`. O gerador sorteia a peça, a escala e o giro,
usa a pegada real (passagem, oásis do marco) e faz só o monte do pé; `FWorldMap::Monoliths` vai pra vista ao
vivo (`AWorldForgeTerrain::BuildMonoliths`) e pra versão final (`place_monoliths`, `M_MonolithRock`: a rocha nos
3 planos com as camadas pela altura do mundo e areia no que olha pra cima). Falta: a fachada esculpida do marco,
o arco, e comparar lado a lado com a arte do AlUla.
Feito depois (13/10, madrugada): a fachada esculpida dos dois lados do marco (painel, porta funda, pilastras,
friso, degraus), o arco natural (a lâmina com o túnel até o chão: duas pernas), os pilares do anel a 1,3–1,7×
(na arte são quase da altura do marco), 10 m de folga do caminho em volta deles (a estrada suavizada cortava a
curva por cima da pedra) e o **anel de estrada** em volta do oásis do marco, ligando as pontas das estradas
(como na arte). Folhas: `captures/worldforge/mono_kit_v*_x_alula.jpg`. O monte de pedra solta continua por
baixo da pedra (antes ficava um fosso em volta do pé: `pe_do_pilar_antes_depois.jpg`). O capim: sem a sombra
comprida e com a luz pelo chão os riscos sumiram; os tufos secos ficaram maiores e menos
(`capim_seco_aberto.jpg`, câmera do jogo a 45 m).

## Fila do WorldForge (12/10/2026)

Na ordem. Os pedidos novos do Felipe entram **no fim**, sem mudar o que já estava na frente.

1. Terminar e conferir a vila no jogo (a sessão do jogo testa) e as fotos dela na cor de barro.
2. Teste completo da interface (estrada à mão, mover peça, mapas guardados com edição, pincel que
   refaz o pedaço, Desert Fury) na placa.
3. As malhas do Look no lugar das caixas, quando chegarem (`captures/look/civil_glb/`), e a
   areia nova do deserto que o Felipe aprovou na cena do Look (12/10): o Custom node e os números
   de `Game/Tools/look_materials/sand_desert_v1*` no chão do deserto (Landscape e ao vivo) e o
   espalhamento (capim seco, gravetos, pedrinhas) no gerador; conferir ponto a ponto na
   desert_golden_hour. Depois o material vai pro plugin RTSLook e o chão passa a usar o de lá.
   Vila no jogo: portão no fundo do quintal é de propósito; os carros encostaram na fachada e a
   pegada cresceu uma célula pra rua (o tanque raspava, 12/10).
4. Fases 2 a 4 (relevo com corpo e marco, gente sem prédio, água como tipo de mapa).
5. Pedidos do Felipe de 12/10, por último (feitos na madrugada de 13/10, falta ele testar):
   1. Tirar o deslize da câmera quando o mouse encosta na borda da tela. A câmera anda só com
      o botão do mouse, como hoje.
   2. Painel da esquerda organizado como os editores de mapa (WorldBuilder do C&C e outros):
      hoje está sujo e cheio de novo.
   3. A lista de mapas guardados não fica sempre aberta (entra na reorganização do painel).
   4. Trazer bem mais mapas do Zero Hour pro WorldForge, variados (tipos diferentes), pra
      analisar e estudar.

   Feito: (1) a borda da tela não desliza mais (`AForgeCameraPawn::EdgePan = false`); (2–3) o painel em
   três abas, Mundo (tipo de mapa e terreno), Editar (só as opções da ferramenta) e Mapas (os guardados e
   os do Zero Hour), com o cabeçalho fixo (semente, gerar, qualidade real); sair de Editar volta a
   ferramenta pra "Olhar"; o teste da interface troca de aba e passou 56/56; (4) 28 mapas da liga
   (GeneralsRankedMaps, release 2024): neve, deserto e rio, floresta, água e ilha, cidade e trilho, 2v2 e
   4v4, em `Saved/WorldForge/imported/` pelo `Tools/zh_import.py` (29 com o Desert Fury).
