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
- O chão (Landscape) aparece preto no F3 porque ainda não é Nanite.
