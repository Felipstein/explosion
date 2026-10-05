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

