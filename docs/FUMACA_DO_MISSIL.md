# A fumaça do míssil (16/10, sessão do jogo)

O pedido do Felipe (16/10 à tarde): "não esqueça que o Patriot solta mísseis com fumaça e não borrões brancos igual tiro de
tanque ... não tô falando pra pegar nada de lá [do BlastLab] nem enviesar, tô falando pra ser inteligente, fazer algo bem
pensado, bem estruturado, bem consultado, bem inspirado, algoritmos, matemática, bem simulado, bem gerado". No BlastLab a
fumaça era empurrada por quem passava (os aviões); aqui são os tanques, os tiros e as explosões.

**Dono:** o jogo (o rastro inteiro, `Game/Source/RTSGame/*/RTSMissileSmoke.*`, e os materiais em `Game/Tools/build_content.py`
`missile_smoke`). O Fluxo fica com a chama, a luz, o lançamento, o acerto, a ogiva e o laser; com a fumaça do jogo pronta, o
BlastFx não cria mais as duas fitas do míssil (`UBlastFxDirector::SetExternalMissileSmoke`, do Fluxo).
**[M]** = medido ou publicado (fonte junto); **[E]** = estimativa, com a conta.

## 1. O real

- **Do que é feita** (propelente composto perclorato de amônio + alumínio, como o do Patriot):
  - a fumaça primária é Al2O3 sólido [M] (AGARD, em [Woźniak et al.](https://yadda.icm.edu.pl/baztech/element/bwmeta1.element.baztech-62b3b1a5-9d43-4930-8cc3-c62f2763270f/c/6_Nr-14_3_s.41-58_Wo_niak-i-inni_v3.pdf));
  - a secundária é a névoa de HCl e água, mais forte no ar frio e úmido [M] ([DTIC ADA359162](https://apps.dtic.mil/sti/html/tr/ADA359162/index.html)).
  - Com ~18% de Al, o Al2O3 é ~34% da massa do escape [E, estequiometria].
- **A cor:** branca. A alumina quase não absorve luz visível [M] ([arXiv 1106.2388](https://arxiv.org/pdf/1106.2388)): o albedo fica perto de 1 e o espalhamento é pra frente (partículas de mícrons).
- **A fumaça por metro é a vazão dividida pela velocidade** (o estudo do Fluxo, `docs/ESTUDO_MISSIL_PATRIOT.md`). O nosso míssil vai de 20 a 110 m/s, então o rastro sai ~5× mais denso perto da caixa.
- **O raio inicial do rastro** ∝ √(J/ρ)/V [E, pela esteira do jato]: grosso na largada e fino no alto.
- **A subida:** o rastro é uma térmica em linha. Briggs [M] ([NOAA ARL](https://ready2.arl.noaa.gov/documents/TutorialX/files/Rise_equations.pdf)): z(t) = 1,6·(B·t²)^⅓, com B o fluxo de empuxo por metro.
- **O espalhamento:**
  - a forma do puff é a dispersão relativa: os redemoinhos menores que ele o alargam e os maiores só o carregam;
  - **Richardson–Obukhov** [M]: dσ²/dt = 1,28·ε^⅓·σ^(4/3), com g ≈ 0,47 ([Biferale et al.](https://arxiv.org/pdf/nlin/0501054));
  - ε = u*³/(κz) na camada junto do chão, em ar neutro.
- **Quanto fica visível:** fisicamente, minutos [E] (o limiar de contraste de 2–5%, [Koschmieder](https://oceanopticsbook.info/view/photometry-and-visibility/level-2/contrast)). O jogo comprime o tempo (§3).
- **O que mexe na fumaça:**
  - **O tanque:** o ar em volta do casco e a esteira atrás, com o déficit ∝ x^(−2/3) [M] ([JFM](https://resolve.cambridge.org/core/journals/journal-of-fluid-mechanics/article/highreynoldsnumber-wake-of-a-slender-body/5D63461CB9AA0B3C365677D8004D9EBA)).
  - **A explosão:** a onda de choque desloca o ar só uns centímetros (a fase positiva dura ~10 ms). Quem empurra é a bola de fogo, que abre o ar em volta e sobe como um anel [M] ([Frontiers](https://pmc.ncbi.nlm.nih.gov/articles/PMC7047208)).
- **As fotos** [O]:
  - PAC-2 em Palau ([DVIDS 7275175](https://www.dvidshub.net/image/7275175/patriot-missile-launches-during-palau-live-fire-exercise)): rastro branco opaco, borda em "couve-flor", lado de sombra cinza;
  - Tenacious Archer 2025 ([9277911](https://www.dvidshub.net/image/9277911/tenacious-archer-25-patriot-live-fire-exercise-day-two)): a nuvem da largada com ~2× o lançador, cor de pêssego por dentro.

## 2. Como os AAA e a literatura fazem

- **Puffs lagrangianos:**
  - Arma 3: partícula com peso, volume e vento, inclusive o do rotor ([BI wiki](https://community.bistudio.com/wiki/ParticleArray));
  - CALPUFF, na dispersão atmosférica;
  - a fumaça como soma de gaussianas (Zhou et al. 2008, [MSR](https://www.microsoft.com/en-us/research/publication/real-time-smoke-rendering-using-compensated-ray-marching/)).
- **Vento que não cria ralos:**
  - curl noise (Bridson 2007, [PDF](https://www.cs.ubc.ca/~rbridson/docs/bridson-siggraph2007-curlnoise.pdf)), com a rampa junto da parede;
  - aqui, os modos de Fourier com a amplitude perpendicular ao número de onda, que é a mesma propriedade (Kraichnan 1970).
- **Quem passa empurra:** os vorticles de Ghost of Tsushima, esferas de vento numa lista percorrida por cada partícula ([GDC](https://gdcvault.com/play/1027124/Blowing-from-the-West-Simulating)).
- **A luz:**
  - Henyey–Greenstein, Beer e o "powder" (Schneider 2015, Horizon, [PDF](https://www.advances.realtimerendering.com/s2015/The%20Real-time%20Volumetric%20Cloudscapes%20of%20Horizon%20-%20Zero%20Dawn%20-%20ARTR.pdf));
  - o espalhamento múltiplo por oitavas (Wrenninge, Kulla e Lundqvist 2013; Hillaire 2016, Frostbite).
- **A leitura de cima:**
  - a sombra no chão é a pista da altura (Kersten 1997, [demo](https://kerstenlab.psych.umn.edu/demos/shadows));
  - sobre unidade, no máximo ~τ 0,5 (a legibilidade da Riot, [VFX](https://www.leagueoflegends.com/en-pl/news/dev/dev-behind-the-scenes-of-vfx-updates/)).
- **Não usamos Niagara Fluids 3D:** o rastro é longo e fino, e a grade gastaria células com ar vazio. O puff é a resolução adaptativa.

## 3. O modelo (o que o código faz)

Cada míssil solta um puff gaussiano a cada **0,45 m** do caminho da simulação, numa curva de Hermite com a direção do voo. O puff solto antes no trecho do quadro já nasce mais velho.

| Parte | A regra | Número |
|---|---|---|
| A massa do puff (extinção integrada) | τ de lado a lado do rastro logo atrás, a 110 m/s, × 110/V | τ 1,6 [E; o 1º vídeo com 3 fez cortina]; até 3× perto da caixa |
| O jato | σ abre de 0,12 m (o bocal) até σ_jato em ~0,06 s | σ_jato = 0,45 m × 110/V, até 1 m; sorteio de ±15% e de lado |
| A visibilidade | a parte sólida fica; a névoa úmida seca | 50% sólida [E]; a úmida some em ~2,5 s [E] |
| A subida | Briggs: w = ⅔·1,6·B^⅓·t^−⅓, quebrada depois de ~8 s | B = 1,5 m³/s² × 110/V [E] |
| O vento | o do mapa (o mesmo do MPC_Wind), perfil logarítmico | z0 = 0,01 m (areia com moita rala), U a 10 m |
| Os redemoinhos grandes | 7 oitavas de 30 a 1,7 m, amplitude ∝ λ^⅓ (Kolmogorov), só as maiores que 2σ | σu = 2,4 u*, σw = 1,25 u*; o vertical some junto do chão |
| O espalhamento | Richardson–Obukhov, com teto 2·σw²·tL | 2·0,64·ε^⅓·σ^(4/3); tL = 60 s |
| A compressão do jogo | o ε cresce até 8× entre 2 e 5 s | a vida pelo preset: baixa 3 s, média 4,5, alta 6, ultra 8 |
| A densidade que cai sozinha | a massa se conserva: τ do centro = E/(2πσ²) | sem curva de transparência inventada |
| O veículo | o dipolo da esfera que anda + a esteira que arrasta e mistura | raio ~2,6 m; ν_t ≈ 0,1·U·a [E] |
| O tiro e o outro míssil | um túnel pequeno no trecho que o projétil andou | [E]; de verdade são centímetros |
| A explosão | a bola de fogo abre o ar, Δr = ∛(r³+R³) − r; o anel sobe e suga | a ogiva R 1,6 m, sobe 4 m/s; a morte R 3,5 m; o tiro R 0,8 m |
| A boca do canhão | o mesmo deslocamento, num cone pra frente | R 2,5 m |
| A união | dois vizinhos a menos de 0,45 σ viram um (a massa e o 2º momento se conservam) | a sobreposição fica ~2–3 |
| A leitura | a fumaça entre a câmera e uma unidade (ou um prédio) fica com no máximo τ 0,25 | ~22% de cobertura |

**A luz.** Toda no material, na unidade da cena: a luz direcional principal (`MainDirectionalLight`, lux) e o céu (`SkyLightEnvMapSample`). A exposição é a mesma do resto, sem fator chutado.
- **Na CPU:** uma grade de extinção (célula ≥ 1 m). De cada puff saem duas marchas, uma até o sol (24 m) e outra pra cima (12 m); a transmitância é T = e^(−τ).
- **No material:**
  - a esfera do puff: a normal pelo canto do quadrado, e a sombra dentro dela (0 do lado do sol, ~τ do outro);
  - HG de dois lobos (g 0,7 a 85%, −0,2 a 15%), espalhamento múltiplo em 3 oitavas e o "powder" do lado de costas pro sol;
  - o céu vezes a transmitância pra cima, e o brilho laranja do gás quente logo atrás da chama e perto de uma explosão;
  - o ruído (Perlin-Worley e fBm, `Game/Tools/make_smoke_noise.py`) come a borda.
- **A sombra no chão:** cada puff ganha um quadrado na pegada dele no chão (pela direção do sol, esticado 1/sen(elevação)), sem teste de profundidade.
- Cada pixel acha o ponto da cena atrás dele pela profundidade (o chão de verdade ou um tanque) e mede a distância ao raio do sol que passa pelo centro do puff; a gaussiana dá a transmitância.
- O material multiplica o ponto por (céu + T·sol)/(céu + sol), então a sombra fica tão escura quanto a dos prédios na mesma luz, cai certa nas dunas e escurece o tanque que passa por baixo.
- A 1ª versão (o quadrado a 12 cm do chão assado) sumia debaixo das dunas.

**O sol vem da CPU** (v4): a direção e a iluminância da luz do mapa (a cor × a intensidade × a transmitância da atmosfera) vão por parâmetro pros dois materiais, e o log escreve os números. No fumaca3 não apareceu sombra no chão, e a fumaça saiu sem lado escuro; o nó da luz principal pode estar vindo zero no translúcido. `rts.MissileSmoke.Debug 1` pinta onde os quadrados da sombra caem, e `2` escurece a sombra 4×, pra achar.

**Na largada,** 12 puffs grossos saem da ré da caixa, rentes ao chão: a nuvem branca que envolve o lançador.

## 4. O custo e a prova

**Medido** (o log; 1.300 puffs no ultra, 8 s):
- **v1:** 4,5 ms de CPU com 1.635 puffs.
- **v2:** 2,2–3,6 ms (a simulação 0,9–1,1; a luz 0,3–0,5; a leitura 0,2–0,3; a malha 0,7–1,7).
- **v3:** a malha cresce de 256 em 256; a sombra só nos puffs que sombreiam; os redemoinhos em meio quadro. A medir.
- **O passo seguinte, se ainda pesar:** a simulação na GPU (Niagara).

- **A CPU:** a simulação, a luz e a malha medem no log a cada 5 s ("RTS fumaça do míssil: N puffs ... ms").
- **A GPU:** quadrados translúcidos de 2,2 σ, com a sobreposição contida pela união. A pesquisa estima 0,3–0,6 ms no típico [E]; a medir.
- **As cenas:**
  - `patriot_missil -Record`: o voo, o rastro, os tanques passando pela fumaça e a ogiva;
  - `patriot_fim`: o fim da partida.
- **O A/B:** `-RTSNoMissileSmoke` volta pras fitas do BlastFx.

## A v6 do visual: o detalhe (17/10)

**O problema:** o vídeo de lado mostrou uma parede branca larga e lisa.

**As causas:**
- **O ruído virava média:** cada quadrado tinha o seu ruído girando, e com ~5,5 sobrepostos por ponto o contraste caía pra ~0,43.
- **A luz não via bolotas:** a normal era a de uma esfera lisa.
- **O contraste ficava achatado:** metade do céu entrava igual em cima e embaixo, e o "pó" estava do lado errado.

**O que entrou** (em `build_content.py`, `missile_smoke`, atrás do `LookV6`; e em `RTSMissileSmoke.cpp`, `rts.MissileSmoke.Look`):

1. **O ruído preso ao mundo, no plano da vista** (os eixos direita e cima da câmera), levado pelo vento: os quadrados sobrepostos leem o mesmo detalhe.
   - As escalas: as bolotas de ~5 m, a cada ~2 diâmetros do rastro (o trem de vórtices de [Crow e Champagne 1971](https://www.doi.org/10.1017/S0022112071001745)); o fBm em 14 m; as bolhas do Worley em 8 m.
   - A leitura é com mip.
2. **A normal de "billow"** pelo gradiente do ruído das bolotas ([Quilez](https://iquilezles.org/articles/gradientnoise/); [Gustavson e McEwan, JCGT 2022](https://jcgt.org/published/0011/01/02/)).
3. **A erosão de borda por limiar**, o remap do [Schneider 2015, Horizon](https://www.advances.realtimerendering.com/s2015/The%20Real-time%20Volumetric%20Cloudscapes%20of%20Horizon%20-%20Zero%20Dawn%20-%20ARTR.pdf), mais forte com a idade.
   - O perfil de chapéu leve no puff novo: a pluma instantânea é um chapéu, e a gaussiana é a média ([Gifford 1970](https://repository.library.noaa.gov/view/noaa/33601/noaa_33601_DS1.pdf)).
4. **A luz:**
   - o céu pelo hemisfério da normal, com o chão de areia embaixo (albedo ~0,42 [E]);
   - o miolo grosso aceso como superfície (ρ E cos/π);
   - o "pó" do lado aceso, com a vista de costas pro sol (Schneider 2015).
5. **O rastro do RTS:** 3/4 da vida do preset e menos engorda.
   - Os RTS fazem o rastro fino e curto: no Zero Hour a Patriot é um risco de 1 s; no BAR o interceptor dura 3,7 s ([ParticleSystem.ini](https://github.com/TheSuperHackers/GeneralsGamePatch/blob/main/Patch104pZH/GameFilesOriginalZH/Data/INI/ParticleSystem.ini#L44405), [armscab.lua](https://github.com/beyond-all-reason/Beyond-All-Reason/blob/master/units/ArmBots/T2/armscab.lua)).

**As voltas até acertar:**
- **6a:** o ruído no chão esticava em riscos nos quadrados em pé.
- **6b e 6c:** fiapos horizontais, do ruído fino lido sem mip a ~2 cm por texel, abaixo de um pixel a 67 m.
- **6d:** com mip e escalas maiores, ficou boa.

**A prova:** `captures/game/entrega_v3/fumaca_missil_v5_x_v6.jpg`, e os vídeos `_v5_antes.mp4` e `_v6_nova.mp4`.

**Falta:** ela ainda é mais macia que a couve-flor dos vídeos de referência. Os próximos passos do estudo são:
- a densidade por bolota sorteada por uma distribuição gama;
- o detalhe 3D, hoje 2D na vista.

## A v7: o contorno visível (17/10)

**A v5 e a v6 foram REPROVADAS** (`docs/APROVADOS.md`, 17/10). O Felipe gostou da largada com volume e do vento. O resto: "espessa por muito tempo, sujando toda a tela", sem formato ("esticada", "um borrão"), acumulando de um tiro pro outro.

**O estudo** (a 2ª opinião mediu numa réplica do `Tick` em Python; ±20%): uma rajada ainda tinha 92% da opacidade máxima quando saía a seguinte (2,75 s depois), e a largura visível crescia de 1,2 pra 1,9 m.

**A causa:** desenhava-se o campo **médio** e o rastro sumia por engorda.
- Cada puff era uma gaussiana, e a união fazia de propósito um cilindro liso.
- A erosão da v6 era relativa ao perfil de cada quadrado: o contorno ficava no mesmo múltiplo de σ, então o rastro desbotava sem encolher nem se partir.
- Sumia por engorda: a compressão do ε, mais um apagamento tardio e uniforme nos últimos 3 s.

**O real:**
- **A forma:** a pluma instantânea é mais estreita que a média, com borda nítida.
  - Os redemoinhos grandes carregam a pluma e os pequenos misturam ([Gifford 1959](https://www.cmar.csiro.au/airquality/meander/index.html)).
  - A interface turbulenta é um salto ([Westerweel 2005](https://discovery.ucl.ac.uk/id/eprint/40297/), [2009](https://www.cambridge.org/core/journals/journal-of-fluid-mechanics/article/momentum-and-scalar-transport-at-the-turbulentnonturbulent-interface-of-a-jet/54D72600386F011AC9B3BEF0C5A9A9A0)).
  - As flutuações de concentração nascem perto da fonte ([Fackrell e Robins 1982](https://resolve.cambridge.org/core/journals/journal-of-fluid-mechanics/article/concentration-fluctuations-and-fluxes-in-plumes-from-point-sources-in-a-turbulent-boundary-layer/08268C38EC897F177BB6DF2FCB3B8104)).
- **O sumiço:** pelo teorema da opacidade de Roberts (1923), a borda visível é onde a coluna atinge um valor fixo τ* ([Gifford](https://ams.confex.com/ams/pdfpapers/128980.pdf); [Frenkiel e Katz](https://www.nature.com/articles/187586a0)).
  - Numa linha gaussiana, a meia-largura visível é w = σ √(2 ln(τ0/τ*)): cresce, chega ao máximo e **encolhe até zero** num tempo finito.
  - As bolotas fracas somem primeiro.
  - O condensado (a água e o HCl) seca rápido no ar seco; o Al2O3 fica ([KIT, o rastro curto em ar subsaturado](https://publikationen.bibliothek.kit.edu/1000148349/148994785)).

**O que entrou** (`rts.MissileSmoke.Look 7`, o padrão; a 6 e a 5 ficam pro A/B; no material, atrás do `LookV7`):
1. **O contorno:**
   - a CPU soma, no centro de cada puff, a coluna dos vizinhos do mesmo rastro projetada na vista (`Cuts()`), e manda o corte relativo τ*/τ da linha em UV1.y;
   - o material corta cada quadrado no mesmo contorno absoluto, com uma rampa até 1,5 τ*, sem estalo;
   - τ*(t) = 0,05 + 0,40 t, o único botão de arte [E].
2. **Nasce denso e fino:**
   - τ de lado a lado 3,2 (era 1,6), com σ do jato 0,40 m;
   - perto da caixa, σ até 0,6 m (era 1,0) e a fumaça por metro até 1,6× (era 3×);
   - sem a compressão do ε.
3. **A forma:**
   - as bolotas a cada ~3 m ±40% (~2 diâmetros, [Crow e Champagne 1971](https://www.doi.org/10.1017/S0022112071001745)), com a massa por uma gama k = 2 de média 1 e o pescoço entre elas;
   - a união não atravessa o pescoço;
   - a interface turbulenta pelo ruído preso ao mundo, que mexe o corte depois de ~0,8 s.
4. **A névoa seca:** 70% condensado (τ 1 s) e 30% sólido (eram 50% e 2,5 s).
5. **O ruído anda com a fumaça:** o deslocamento médio dos puffs visíveis, somado na CPU (`NoiseOffCpu`); antes era o vento de 10 m.
6. **A vida virou só a trava** (4 s; o apagamento só no último segundo).
   - De passagem, consertei o preset: a tabela lia o nível de efeitos do motor (1–4) como 0–3, e a Alta ganhava a vida do Ultra.

**A medida:** `rts.MissileSmoke.Log 1` registra a cada 0,25 s a massa visível. O alvo do estudo: no tiro seguinte (2,75 s), ≤ 10% do pico da rajada; hoje, ~92%.

**O que não fazer:**
- só encurtar a vida (vira um borrão que some mais rápido, o risco de 1 s do ZH);
- subir a erosão relativa (os fiapos das voltas 6b e 6c);
- sumir por engorda (o "abre pros lados");
- fita ou flipbook (quebra a regra do volume).

**O passo seguinte, o certo pro AAA:** somar antes de iluminar, numa grade 3D pequena em volta dos rastros, marchada com o BlastRaymarch e com o remap do Schneider na soma. Estimativa de 0,5–1 ms na 1660 [E].

### A v7 medida no jogo (17/10, compilação 48)

| Cena | O que sobra quando sai a rajada seguinte (R) | A massa visível entre as rajadas |
|---|---|---|
| `fumaca_predio2`, a v6 | 0,56–0,75 | ~300 e não cai (acumula) |
| `fumaca_predio2`, a v7 | 0,18–0,29 | cai a 22–50 |
| `fumaca_25_patriots`, a v6 | (tiro contínuo) | ~6000–7500 |
| `fumaca_25_patriots`, a v7 | (tiro contínuo) | ~2000–4000 |

**Os defeitos que sobraram:**
- **As bolotas soltas** saem redondas e iguais, um colar de algodão: o corte de um puff sozinho é um círculo, e o ruído da borda (24/14/8 m) é grande demais pro puff de ~1 m.
- **A nuvem em cima da bateria** é realimentada a cada rajada.
- **O R ainda está acima do alvo de 0,10.**
- **A carga:** com 25 Patriots, a CPU ficava em 14–16 ms (a v6, 22–24 ms), a ~3000 puffs. Cada puff testava todos os veículos, os ~100 trechos de míssil no ar e todas as explosões, na thread do jogo.
- **A sombra no chão:** preto e magenta, de borda reta, na sombra dos prédios (a faixa rosada de todas as cenas da Patriot; com as 25 explode).
  - O material da sombra multiplica a cena e não recebe o céu do motor (o SkyUp chega ~0).
  - Dentro da sombra de uma casa (Vg 0), a conta virava 0/1e-3: preto. Com o céu quase zero por canal, o verde é o menor e sobra o magenta.
  - Os quadrados empilhados multiplicam o defeito.

**A volta b:**
- **O sumiço:** τ* sobe 0,50/s (era 0,40); perto da caixa, a fumaça por metro vai até 1,3× (era 1,6).
- **As bolotas:** ±60% no comprimento (era ±40%), a gama até 2,2 (era 3) e o pescoço mais suave (0,7–1,3).
- **A borda:** o Worley de ~0,5 m entra no corte, lido com mip.
- **A sombra:** o céu vem da CPU, do SH da SkyLight convoluído com o cosseno (o mesmo que o motor monta: `CalcDiffuseTransfer` × a cor × a intensidade). A conta só tira a parte do sol que chegava ao chão, então fica sempre entre Tsh e 1.
- **A carga:**
  - cada rastro numa thread (a simulação, o contorno e a luz);
  - só com os veículos, os tiros e as explosões que encostam na caixa dele (o alcance da explosão: 5 R pro sopro e 4,5 R pro anel, onde o efeito já é ~1%);
  - a leitura das unidades olha só os rastros no caminho de cada raio.

### As voltas c a f: o colar de bolas (17/10)

De perto, perto da caixa, o rastro saía um colar de bolas redondas. Testei uma causa por volta, cada uma num A/B no jogo:

| Volta | Hipótese | O que entrou | O colar |
|---|---|---|---|
| c | o pescoço entre as bolotas | a massa lisa entre os nós da gama | continuou |
| d | a luz: cada quadrado sombreado como esfera | o tubo: a tangente leva o rumo do rastro e a normal perde o componente ao longo dele | mais suave, continuou |
| e | a massa variando até 5× em 3 m | a força do trecho (a gama) no corte, não na massa | continuou |
| f | **o corte por quadrado** | o corte pelo perfil de través | **sumiu** |

**A causa:**
- Cada quadrado cortava o contorno pelo perfil dele nas duas direções.
- Ao longo do rastro a soma é plana, mas o perfil de cada um cai. Então o corte comia o meio entre dois vizinhos, e a união, que afasta os puffs, deixava isso mais nítido.

**O conserto** (`Perp2`, a distância até o eixo, sai da base pra fumaça):
- τ = τ0 · perfil · (1 − τ*/τ_linha / perfil_través).
- Somado ao longo do rastro, dá τ_linha(través) − τ*: o contorno de Roberts da linha inteira, sem pescoço.
- Sem rumo (a nuvem da largada), fica o corte de antes.

**Também nessas voltas:**
- **O orçamento da tela:** acima de 600 m² de massa visível a 120 m do ponto que a câmera olha (~2,5 baterias no pico), o τ* cresce na mesma proporção, até 3×.
- **A carga:** a montagem da malha também roda em paralelo.

**Medido na v7f:**
- **O que sobra quando sai a rajada seguinte:** 16–19% na fumaca_predio2 (a v6 deixava 56–75%).
- **As 25 Patriots:** ~1500–3000 m² de massa visível (a v6, ~6000–7500) e 3–5 ms na CPU (a v6, 22–24 ms).
- **A comparação:** `captures/game/entrega_v3/fumaca_missil_de_lado_v6_x_v7f.jpg` e `fumaca_25_patriots_v6_x_v7f.jpg`.

### A v7f aprovada; a volumétrica guardada (17/10)

O Felipe aprovou a v7f como está ("aprovo o jeito que o Patriot tá") e cancelou a versão volumétrica.

**A proposta, guardada caso ele volte** (o Fluxo e a Arte, de acordo):
- **O rastro:** vai pro campo da poeira (RTSDustField), numa 2ª camada só pra fumaça (voxel de 0,5 m, tijolos de 4 m, atlas próprio). Cada puff vira uma pequena gravação do Mantaflow, com o quadro andando pela idade, no lugar do gaussiano. É ela que tira as bolinhas.
- **A nuvem da largada:** uma gravação própria, desenhada como o disparo do tanque no raymarch.
- **A luz:** a mesma da poeira e das explosões (a sombra do mapa, a sombra no chão e nas unidades pela M_SmokeShadowLF, o céu e as luzes perto). Um sistema só.
- **A divisão:** a Arte faz o campo; o Fluxo, as gravações e a marcha; o Jogo, a simulação e a conversão de cada puff.
- **O custo estimado:** 0,5–1,5 ms com 25 Patriots, a medir.
- **Pra dimensionar:** `rts.MissileSmoke.Dump N` grava todos os puffs aos N s em `captures/game/missile_puffs_<N>s.csv`.

**O caminho certo da sombra** (`docs/ESTUDO_LUZ_NA_FUMACA.md`, um sistema só): a sombra da fumaça do míssil deve entrar no mapa visto de cima que a light function do sol lê (o `M_SmokeShadowLF` do Fluxo), somando a espessura óptica. Ela tira só o sol direto, sombreia as unidades e não empilha quadrados. A combinar com o Fluxo.
