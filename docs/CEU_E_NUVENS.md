# Céu e nuvens volumétricas (estudo de 14/10/2026, pra v3)

**O pedido do Felipe:** "o céu azul LINDO, NUVENS VOLUMÉTRICAS PODEROSAS, o céu bonito, melhoria na câmera deitada".

**O estado:** estudo com fontes. Nada foi aberto na Unreal nem medido na placa. Tudo marcado **est.** é estimativa (minha conta ou extrapolação) e só vale depois de A/B no jogo, mesma cena, mesma semente, com vídeo.

---

## 1. O resumo (o que muda pro Felipe)

1. **O céu que a câmera deitada mostra é a faixa de 0° a ~14° acima do horizonte** (§2). As nuvens "poderosas" ali são **torres no horizonte, a 20–80 km**. Nada fica acima da batalha nem entre a câmera e as unidades.
2. **Hoje uma nuvem nem apareceria.** A névoa de altura aprovada cobre metade ou mais do céu deitado (§8.2). A perspectiva aérea ×40 da receita apaga a nuvem inteira (§8.3). Primeiro vem **destravar o céu**: dois ajustes que só valem com a câmera deitada, e a câmera normal fica idêntica (§7.1).
3. **O alvo `day_tilt_cam` não tem azul nem nuvem.** É céu de poeira, pastel, com um malva do lado oposto ao sol (medido, §2). "Céu azul lindo" é outro céu. O A/B leva os dois.
4. **Quatro estilos pro A/B** (§7.2):
   - **A**, poeira dourada (o alvo);
   - **B**, azul limpo;
   - **C**, altocúmulo dourado;
   - **D**, torres no horizonte.
5. **Sombra de nuvem no chão: não** (§7.4). Com o sol a 8° ela não passa como mancha: escurece a tela inteira devagar. Isso troca a luz aprovada e atrapalha a leitura (jogador de Civ VI: "deixa o mapa mais difícil de ler").
6. **O custo:**
   - a nuvem só liga com a câmera deitada; na normal custa 0;
   - deitada, na 1660: **est.** 0,2–0,4 ms na média, 0,5–1 ms na alta, 1,5–3 ms no ultra; na baixa, sem nuvem;
   - ninguém publicou medida nessa classe de placa, então é a medir (§7.3).
7. **O "uau" que a física e os AAA ensinam** (§3.7, §5.4):
   - ar limpo com nuvem média e alta;
   - borda prateada olhando pro sol;
   - dispersão múltipla (sem ela a nuvem vira fumaça escura);
   - topo dourado e base fria;
   - a luz da areia por baixo da nuvem;
   - raios de luz pelos buracos;
   - torre desbotando na distância.
8. **O "céu preto" era a névoa de guerra** e já foi corrigido em 14/10. Sobrou uma faixa escura das dunas fora do mapa (§8.1).

---

## 2. O que a câmera do jogo vê do céu (a geometria manda em tudo)

Contas a partir de `RTSCameraPawn.cpp:222-226` e `Game/Config/DefaultGame.ini` (campo de visão horizontal, tela 16:9). O campo abre de 50° pra 58° só quando a câmera deita abaixo de 25° (curva suave até 5°). **No zoom perto (67 m, 28°) o código dá 50°, não 58°.**

| câmera | campo (vertical) | inclinação | topo da tela em relação ao horizonte |
|---|---|---|---|
| normal (100 m) | 50° (29,4°) | 37,5° | −22,8° (só chão) |
| zoom perto (67 m) | 50° (29,4°) | 28° | −13,3° (só chão) |
| deitando | 51,2° (30,2°) | 20° | −4,9° (ainda só chão) |
| **o horizonte entra** | 53,3° (31,6°) | **15,8°** | 0° |
| deitada | 56,8° (33,8°) | 10° | +6,9° (20% da tela é céu) |
| deitada no limite (`MinFreePitchDeg` 3°) | 58° (34,6°) | 3° | **+14,3°** (41% da tela é céu) |

**O que isso quer dizer:**
- **O céu que o jogo mostra é só a faixa de 0° a ~14° acima do horizonte.** O zênite nunca aparece. O "azul" e as nuvens têm de morar nessa faixa.
- **Nuvem nessa faixa está longe.** A base de uma nuvem a 3 km de altura aparece a 14° quando está a 12 km, a 5° a 34 km, a 3° a 57 km. Um cúmulo-nimbo a 40 km (base 2–3 km, topo 12 km) ocupa de ~3° a ~16°: a faixa inteira. Por isso "nuvens poderosas" aqui quer dizer **torres no horizonte, a 20–80 km**, nunca nuvem baixa perto.
- **Nuvem nunca fica entre a câmera e as unidades** se a camada começar acima de ~1,5 km: a câmera deitada fica a ~5 m do chão (100 m × sen 3°) e a normal a ~60 m. A regra de jogabilidade se cumpre sozinha pela altitude.
- **O sol fica fora do quadro na câmera deitada inicial.** Ele está a −42° da câmera inicial e o meio campo horizontal é 29°. Só aparece girando a câmera pra ele (o alvo mostra o sol no quadro).

**O alvo da câmera deitada** (`docs/concept/scenes/desert_golden_hour/best/day_tilt_cam.webp`), medido em 16 pontos do céu (média de blocos):
- **Não tem azul.** O céu inteiro tem saturação 0,06–0,35 e matiz 20–40° (pêssego e dourado).
- **O "lilás" é um malva-rosado pálido** longe do sol: topo esquerdo `#B8A9AD` (matiz 344°, sat 0,08), horizonte esquerdo `#A7898D` (matiz 352°, sat 0,18).
- **Perto do sol:** dourado claro, `#FFECBA` (sat 0,27, quase branco).
- **Topo à direita:** pêssego claro, `#ECDBCF`.
- **Dunas ao longe:** `#A17A59`–`#AF8357`, mais escuras que o céu, sem contorno duro.
- **Nuvens:** só uns fiapos de névoa. O alvo é céu sem nuvem e com muita poeira.

**A tensão a mostrar ao Felipe:** "céu azul LINDO" e o alvo (sem azul) são dois céus diferentes. Com o sol a 8°, um céu azul de verdade nessa faixa baixa pede **pouca poeira** (ver §5) e o lado oposto ao sol. Por isso os estilos do A/B (§7) incluem os dois.

---

## 3. Como os AAA fazem o céu e as nuvens

**O limite das fontes:** cinco PDFs passam de 10 MB e não abriram na ferramenta (Frostbite 2016, Nubis 2017, 2022 e 2023, o PDF do autor do EGSR 2020). Usei o SlideShare, um espelho do texto do artigo, a tese do Frostbite e resumos. O RDR2 só existe como PPTX de 232 MB. Onde o número é de segunda mão, está marcado.

### 3.1 Hillaire 2020: a SkyAtmosphere da Unreal
**"A Scalable and Production Ready Sky and Atmosphere Rendering Technique", EGSR 2020.** Fontes: [página do artigo](https://diglib.eg.org/handle/10.1111/cgf14050), [texto num espelho](https://www.readkong.com/page/a-scalable-and-production-ready-sky-and-atmosphere-3211109), [slides do curso SIGGRAPH 2020](https://blog.selfshadow.com/publications/s2020-shading-course/hillaire/s2020_pbs_hillaire_slides.pdf), [código](https://github.com/sebh/UnrealEngineSkyAtmosphere).

**O que ensina:**
- O céu distante é de baixa frequência: tabelas pequenas bastam e o custo não depende da resolução.
- A tabela do céu concentra resolução no horizonte (latitude não linear). Pro RTS é perfeito: o jogo só mostra o horizonte.
- A perspectiva aérea mora numa grade 3D presa à câmera e vale pra opacos, translúcidos **e nuvens**.
- A dispersão múltipla de ordem infinita vira uma tabela 32×32.

**As tabelas e o custo** (Tabela 2 do artigo, GTX 1080 a 720p):

| tabela | tamanho | ms |
|---|---|---|
| transmitância | 256×64 | 0,01 |
| céu (sky-view) | 200×100 (192×104 na 5.8) | 0,05 |
| perspectiva aérea | 32×32×32 sobre 32 km (32×32×16 sobre **96 km** na 5.8) | 0,04 |
| dispersão múltipla | 32×32 | 0,07 |
| **total com o céu desenhado** | | **0,31** |

A GTX 1080 tem ~1,8× o poder de cálculo da 1660 (8,9 contra 5,0 TFLOPS, especificações da NVIDIA de memória, não reabertas nesta sessão): o céu da atmosfera deve ficar em **~0,5 ms na 1660** (**est.**; o jogo já paga isso hoje).

**A Terra do artigo** (igual à da Unreal, §6): Rayleigh 5,802/13,558/33,1 ×10⁻⁶ /m com escala de 8 km; Mie 3,996 ×10⁻⁶ /m com escala de 1,2 km e g 0,8; ozônio 0,650/1,881/0,085 ×10⁻⁶ /m numa faixa de 30 km centrada em 25 km.

**Dos slides de 2020 sobre nuvens na Unreal:**
- sem dispersão múltipla a nuvem parece "fumaça escura" (o albedo da nuvem é ~1);
- fase isotrópica fica clara demais; um lobo forte só fica escuro demais;
- pras bordas escuras ele prefere baixar o albedo na borda em vez do Beer-Powder ("algodão sujo" é o risco);
- a luz do chão vista de baixo é "muito importante pra perceber a forma";
- a transmitância da atmosfera **a cada passo** dá o pôr do sol mais real.

### 3.2 Hillaire 2016: céu e nuvens no Frostbite
**"Physically Based Sky, Atmosphere and Cloud Rendering in Frostbite", SIGGRAPH 2016.** Fontes: [PDF da EA](https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/s2016_pbs_frostbite_sky_clouds.pdf) (lido pelo [SlideShare](https://www.slideshare.net/DICEStudio/physically-based-sky-atmosphere-and-cloud-rendering-in-frostbite)), [tese de mestrado do Frostbite (Högfeldt)](http://www.cse.chalmers.se/~uffe/xjobb/RurikH%C3%B6gfeldt.pdf).

**O que ensina:**
- **Fase de dois lobos:** g₀ = 0,8 (frente, a borda prateada), g₁ = −0,5 (trás), mistura 0,5.
- **Integração analítica por passo:** conserva energia (a soma ingênua escurece meio denso demais). A Unreal faz isso.
- Dispersão múltipla em oitavas (Wrenninge); luz ambiente = o céu sem o disco do sol; perspectiva aérea aplicada uma vez, na profundidade média pesada pela transmitância (a Unreal faz igual).

**Os números:**
- Xbox One: nuvem da vista em 640×360 com reprojeção, **0,72 ms a 720p e 1,09 ms a 900p**; total das nuvens 0,84 e 1,23 ms.
- Tabelas do céu: 3,5 ms de uma vez ou 0,22 ms por quadro fatiadas em 19 quadros.
- A tese: forma 128×32×128 RGBA8, detalhe 32³, mapa de tempo 1024², meia resolução, mistura temporal de 5%, 4 passos rumo ao sol, para com transmitância < 0,01.

### 3.3 Schneider 2015: as nuvens de Horizon Zero Dawn
**"The Real-time Volumetric Cloudscapes of Horizon Zero Dawn", SIGGRAPH 2015 Advances** ([PDF](https://www.advances.realtimerendering.com/s2015/The%20Real-time%20Volumetric%20Cloudscapes%20of%20Horizon%20-%20Zero%20Dawn%20-%20ARTR.pdf), lido inteiro).

**As camadas:**
- **Nuvem volumétrica baixa entre 1.500 e 4.000 m.**
- **Acima de 4 km, altocúmulo e cirro são texturas 2D** rolando: finas demais pra valer a marcha.
- Desenho num raio de 35 km. **A partir de 15 km a nuvem é forçada a cúmulo com ~50% de cobertura**, pra o horizonte ser sempre "épico". É exatamente a faixa que o nosso jogo mostra.

**O ruído e a forma:**
- 3D 128³ Perlin-Worley + Worley (a forma); 3D 32³ Worley (o detalhe); 2D 128² de curl (turbulência).
- Mapa de tempo: R = cobertura, G = precipitação, B = tipo.
- A forma é a base × um gradiente de altura por tipo (estrato, estratocúmulo, cúmulo). A erosão de borda só roda onde já tem nuvem; Worley invertido na base dá os fiapos.

**A marcha e o custo:**
- 64 passos, até 128 no horizonte; 6 amostras de luz num cone (5 perto, 1 longe pra pegar a sombra de nuvem distante).
- Sem truque: ~20 ms. Com 1/4 da resolução, atualizando 1 de 16 pixels por quadro com reprojeção: **~2 ms e 20 MB no PS4**.

**A luz:** Beer + Henyey-Greenstein (a borda prateada) + o "powder" (bordas escuras quando se olha pra longe do sol). Luz ambiente subindo com a altura. A atmosfera cobre a nuvem pela profundidade. (O g e a fórmula do powder estão nas imagens dos slides; a fórmula 1 − e^(−2d) é atribuída ao GPU Pro 7 e não foi conferida.)

### 3.4 Nubis 2017, 2022 e 2023 (Guerrilla)
- **2017** ([página](https://www.guerrilla-games.com/read/nubis-authoring-real-time-volumetric-cloudscapes-with-the-decima-engine), [gerador do ruído](https://www.guerrilla-games.com/media/News/Files/nubis_noise_generator.zip)): autoria regional, transições, mudanças no modelo de luz; a versão lançada só modela as nuvens baixas e passou do orçamento de 2 ms (trechos de busca).
- **2022, "Nubis, Evolved"** ([página](https://www.guerrilla-games.com/read/nubis-evolved)): nuvens que se atravessa voando, supertempestades, boa imagem a 1080p **sem** upscaling temporal; dois métodos, **perfil vertical** pras nuvens altas inalcançáveis e envelope pras que abraçam montanhas (trechos de busca).
- **2023, "Nubis³"** ([página](https://www.guerrilla-games.com/read/nubis-cubed)): voxels em vez de 2,5D, distância com sinal pra pular vazio, forma por simulação de fluido; "bordas escuras e brilho interno" aproximados.
- **Números de segunda mão** (resumos em chinês, [gcores 192613](https://www.gcores.com/articles/192613)): camada de ~500 m em 4 km², voxels de 8 m com detalhe de ~0,5 m, grade de luz 256×256×32 a 0,1–0,2 ms amortizada em 8 quadros, protótipo ~4 ms no PS5 a 960×540, nuvem de perfil amortizada em 16 quadros (~20 ms → ~2 ms).
- **A lição pra nós:** a forma "poderosa" vem de forma com intenção (perfil por tipo, envelope, modelado), não de ruído genérico.

### 3.5 Red Dead Redemption 2 (Bauer, SIGGRAPH 2019)
**"Creating the Atmospheric World of Red Dead Redemption 2"** ([Advances 2019](https://advances.realtimerendering.com/s2019/index.htm); só o PPTX de 232 MB, não aberto).
- **Resumo oficial:** uma solução só pra nuvem, névoa e luz ambiente, por voxelização e marcha de raio, usada na tela, nos reflexos e numa grade de sondas de céu.
- **Como o Hillaire 2020 descreve o RDR2:** nuvem e névoa na mesma marcha; traçado a 1/4 da resolução com upsampling pelo TAA (resposta instantânea, mais borrado; o oposto do Horizon, que tem 16 quadros de atraso mas parece resolução cheia); sombra de nuvem por mapa de sombra exponencial.
- Alturas, cirros, custo e hora dourada: **não achados**.

### 3.6 Microsoft Flight Simulator e outros
- **MSFS 2020/2024:** nenhuma palestra técnica achada. Só o formato do tempo ([doc do MSFS 2024](https://docs.flightsimulator.com/msfs2024/retail/content-configuration/mission-xml-files/weather-xml-wpr-file-properties/index.md)): várias camadas, cada uma com base e topo em metros, densidade 0–1 e "scattering" 0–1.
- **Ghost of Tsushima** (Patry, [SIGGRAPH 2021](https://advances.realtimerendering.com/s2021/jpatry_advances2021/index.html)): o g varia com a profundidade dentro da nuvem, de ≈ −0,15 a ≈ 0,85; o retroespalhamento × ~2,16 imita a dispersão múltipla (calibrado com albedo 0,9); nuvens num paraboloide 768² atualizado aos poucos em 60 quadros.
- **Toft et al., "Optimisations for Real-Time Volumetric Cloudscapes"** ([arXiv 1609.05344](https://ar5iv.labs.arxiv.org/html/1609.05344)), GTX 1080 a 1080p: 128 passos em resolução cheia **297,7 ms**; 1/4 da resolução, 8 passos com jitter e TAA, **2,4 ms**. A reconstrução temporal é o que torna a nuvem possível.
- **Wrenninge, "Oz: The Great and Volumetric"** (SIGGRAPH 2013 talk, [registro](https://history.siggraph.org/?p=139835)): a aproximação de dispersão múltipla por oitavas que a Unreal usa (`VolumetricCloud.usf:337`). O [Skybolt](https://prograda.com/2021/07/28/rendering-planetwide-volumetric-clouds-in-skybolt/) usa 3 oitavas com fator 0,5ⁿ pra albedo, extinção e fase.

### 3.7 O que dá a nuvem "poderosa" (a síntese)
1. **Forma com intenção.** Base de baixa frequência × perfil de altura por tipo, erosão só na casca, topo arredondado, base em fiapo, distorção por curl. **Cúmulo forçado no horizonte** (HZD).
2. **Dispersão múltipla.** Sem ela vira fumaça escura. 1–2 oitavas com a = b = c ≈ 0,5 (Unreal, Skybolt).
3. **Borda prateada.** Lobo da frente forte (g 0,8–0,85) + lobo de trás (−0,15 a −0,5), mistura ~0,5 (Frostbite, GoT).
4. **Bordas escuras e reentrâncias claras.** Powder (HZD) ou albedo menor na borda (Unreal), dosado.
5. **A cor da hora dourada.** O sol depois da atmosfera, **a cada passo** (topo quente, base fria).
6. **Luz do céu por altura.** Topo azulado, base escura, mais a luz do chão por baixo (no deserto, quente).
7. **Perspectiva aérea na nuvem.** A torre a 50 km desbota pra cor do horizonte. É ela que dá escala.
8. **Sombra longa.** Nuvem sombreando nuvem e os raios de luz no ar.

---

## 4. RTS e jogos de câmera alta

**O resumo:** nenhum RTS publica tamanho, velocidade ou força da sombra de nuvem. Esses números vão sair da nossa medida. O que existe são escolhas de produto e reclamações de jogador. Legenda: **[aberto]** = página lida; **[trecho]** = só o resumo do buscador.

| jogo | céu e nuvem | sombra de nuvem / leitura | fonte |
|---|---|---|---|
| **Total War: Warhammer III** | no WH2 o skybox é estático; no WH3 "as nuvens se mexem" (jogador, 2023). A câmera de batalha vê o horizonte | nenhuma opção de sombra de nuvem nos guias | [Steam](https://steamcommunity.com/app/1142710/discussions/0/6792046983686822785) [aberto] |
| **Total War: Pharaoh** | clima dinâmico por região (chuva vira tempestade e lama) | nada técnico | [Game Rant](https://gamerant.com/total-war-pharaoh-interview-dynamic-weather-system-development-explained/) [trecho] |
| **Age of Empires IV** | nada sobre o céu | a câmera "Panoramic" (2022) parou onde parou porque afastar mais "causou problemas de seleção de unidades e leitura" | [patch 17718](https://ageofempires.com/news/age-of-empires-iv-update-17718) [aberto] |
| **Company of Heroes 3** | sem opção de nuvem (Physics, Shadows, Textures, Geometry, AA) | — | [Overclock3D](https://overclock3d.net/reviews/software/company_of_heroes_3_pc_performance_review_and_optimisation_guide/) [trecho] |
| **Manor Lords** (Unreal 5) | opção "Volumetric Clouds"; no Steam Deck custou ~2 qps (a ~30 qps, ~2,3 ms, **est.**) | a sombra de nuvem parece vir de **light function** ("light function atlas habilitado pra lagoas receberem sombra de nuvem", patch 0.8.002), não do mapa da nuvem (**inferência**) | [SteamDeckHQ](https://steamdeckhq.com/game-reviews/manor-lords/) [aberto]; [wiki 0.8.002](https://wiki.hoodedhorse.com/Manor_Lords/0.8.002_-_Beta_patch) [trecho] |
| **Anno 1800** | — | sombra de nuvem por amostras de textura de ruído, cortada nos ajustes baixos (artigo Intel/Ubisoft, só o trecho) | [Intel](https://www.intel.com/content/dam/develop/external/us/en/documents/bringing-anno-1800-to-laptop-gamers-around-the-world-the-long-road-from-10-to-30-fps.pdf) [não legível] |
| **Cities: Skylines II** (Unity) | nuvem Off/Low/High = 88/83/80 qps numa RTX 4090 em 4K (~1,1 ms no High, **est.**); o guia recomenda Off | — | [guia Steam](https://steamcommunity.com/sharedfiles/filedetails/?id=3060018316) [aberto] |
| **Civilization VI** | — | tem "cloud shadows"; jogadores: "deixa o mapa mais difícil de ler", "bem irritante" | [CivFanatics](https://forums.civfanatics.com/threads/i-dont-see-cloud-shadows.643382/) [aberto] |
| **Supreme Commander** | o skybox só aparece no reflexo da água | — | [Courrèges](https://www.adriancourreges.com/blog/2015/06/23/supreme-commander-graphics-study/) [aberto] |
| **Dune: Spice Wars, Tempest Rising, Frostpunk 2, Anno 117** | nada sobre nuvem nas opções; no Frostpunk 2 a "Fog" está entre as mais caras | — | [GameGPU](https://en.gamegpu.com/test-gpu/rts-strategii/tempest-rising-test-gpu-cpu), [DSOGaming](https://www.dsogaming.com/pc-performance-analyses/frostpunk-2-pc-performance-analysis/) [trecho] |
| Homeworld 3, Planetary Annihilation, C&C Remastered, Stormgate, BAR | não achado (o limite de buscas acabou) | | |

**O que tirar disso:**
- **A sombra de nuvem é opcional e divide jogador.** Civ VI e Cities II tratam como enfeite que se desliga. Não achei RTS que trate a sombra de nuvem como parte da leitura.
- **Quem faz sombra de nuvem barata usa textura projetada** (light function ou ruído no material), não a sombra física da nuvem: Manor Lords (inferência), Anno 1800 (trecho). A própria Epic dá de exemplo a "Directional Light com light function de sombra de nuvem".
- **Céu animado se nota:** o jogador do Steam compara o WH2 (céu parado) com o WH3 (nuvens se mexendo). É um relato só.
- **A leitura manda na câmera:** a AoE IV recuou na distância por leitura. É a nossa regra.

### 4.1 O céu ao inclinar a câmera
- **Total War** deixa ver o horizonte e o céu ali é parte do espetáculo (as nuvens animadas do WH3).
- **Horizon Zero Dawn** força cúmulos a partir de 15 km com ~50% de cobertura "pra o horizonte ser sempre épico" (§3.3). É a receita direta pra nossa faixa de 0–14°.
- **O nosso caso:** a câmera deitada é a cinematográfica (a do alvo). A nuvem mora no horizonte e nunca acima do campo de batalha (§2), então a regra de jogabilidade fica cumprida por construção.

---

## 5. O céu da hora dourada no deserto (a física)

### 5.1 A cor do céu com o sol a 8°
- **Massa de ar a 8°: 6,86** (Kasten & Young 1989, via [Wikipedia, Air mass](https://en.wikipedia.org/wiki/Air_mass_(astronomy))). A luz do sol atravessa ~7 atmosferas.
- **O azul do zênite a 8° ainda é quase todo Rayleigh.** O ozônio (banda de Chappuis) dá 8% do azul do zênite com o sol a 20°, 15% a 10° e 66% no horizonte ([Lange, Rozanov & von Savigny, ACP 2023](https://acp.copernicus.org/articles/23/14829/2023/acp-23-14829-2023.html), que confirma Hulburt 1953). O ozônio é o que segura o azul-lilás **no crepúsculo**, não a 8°. A Unreal já tem o ozônio de Bruneton (tenda em 25 km, ~300 DU).
- **O brilho pelo céu** (céu padrão CIE 2003 / ISO 15469, sol a 8°, conta do levantamento com os parâmetros do [colour-science](https://colour.readthedocs.io/en/latest/_modules/colour/phenomena/sky/cie2003.html); múltiplos do zênite, **est.**):

| ponto do céu | limpo (tipo 12) | turvo (tipo 14) |
|---|---|---|
| 10° ao lado do sol | 23× | 47× |
| horizonte (5°) a 90° do sol | 3,4× | 5,5× |
| horizonte oposto ao sol | 4,5× | 6,2× |
| 30° de altura a 90° do sol | 1,7× | 1,8× |

  A faixa que o jogo mostra (0–14°) é a parte **clara** do céu: 3–6× o zênite, e 20–50× perto do sol. O azul fundo mora alto e longe do sol, fora do quadro.
- **Gurney** ([Sky Blue, 2008](https://gurneyjourney.blogspot.com/2008/04/sky-blue.html)): dois degradês sobrepostos, o brilho em volta do sol e o clarão do horizonte. Perto do sol o céu fica mais claro, mais quente e menos saturado; longe dele, azul mais saturado puxando pro violeta.
- **O lado oposto ao sol:** o cinturão de Vênus (rosa, 10–20° acima do horizonte) só existe com o sol **abaixo** do horizonte ([Wikipedia](https://en.wikipedia.org/wiki/Belt_of_Venus)). A +8° o lado oposto é um horizonte claro. O malva-rosado do alvo é licença artística ou poeira (abaixo).
- **A cor do sol a 8°** (conta espectral do levantamento, **est.**): ~3.900 K sem aerossol, ~3.600–3.700 K com poeira. A Wikipedia dá ~3.500 K pra hora dourada ([Golden hour](https://en.wikipedia.org/wiki/Golden_hour_(photography))). A receita aprovada usa 4.700 K (escolha de arte depois de 11 voltas; não mexer sem A/B).

### 5.2 A poeira do deserto (dados AERONET)

| lugar (fonte) | AOD | Ångström | g (fase) | albedo de espalhamento |
|---|---|---|---|---|
| Solar Village, Arábia ([Dubovik 2002](https://lweb.cfa.harvard.edu/hitran/HITRAN2012/Aerosols/papers/dubovik_et_al._(2002).pdf)) | ~0,20 a 550 nm (est. por Ångström) | 0,1–0,9 | 0,65–0,69 | 0,92–0,97 |
| Bahrein (idem) | ~0,25 | 0–1,6 | 0,66–0,68 | 0,92–0,97 |
| Cabo Verde, poeira do Saara (idem) | ~0,45 | 0,1–0,7 | 0,71–0,73 | 0,93–0,99 |
| 14 estações N. da África/Arábia, só dias de poeira ([Kim 2011](https://acp.copernicus.org/articles/11/10733/2011/)) | ≥ 0,4 (440 nm) | ≤ 0,2 | 0,752 | 0,944 |
| Tamanrasset, seca ([Guirado 2014](https://acp.copernicus.org/articles/14/11753/2014/acp-14-11753-2014.pdf)) | mediana 0,06 (440 nm) | 0,62 | — | 0,93–0,96 |
| Tamanrasset, estação de poeira (idem) | mediana 0,29 | 0,28 | 0,74–0,76 | 0,90–0,98 |
| Kuwait 2008–2017 ([Kokkalis 2018](https://mdpi-res.com/d_attachment/remotesensing/remotesensing-10-01807/article_deploy/remotesensing-10-01807.pdf)) | média 0,45 (500 nm); tempestade 0,75–2,9 | 0,61 | — | — |

- **A Unreal vem limpa demais pro deserto:** o Mie padrão dá AOD 0,0053 (0,00444/km × 1,2 km), herdado de Bruneton. A receita aprovada dá ~0,03–0,04. O deserto real fica em **0,06–0,45**.
- **A altura da poeira:** a camada de mistura em Tamanrasset sobe a 4–6 km na estação de poeira (1–2 km na seca); a Camada de Ar do Saara tem 3–4 km de espessura com topo em 5–6 km ([ACP 2018](https://acp.copernicus.org/articles/18/17655/2018/acp-18-17655-2018.html)). A receita usa 0,8 km.
- **O que a poeira faz:**
  - absorve mais azul (albedo menor em 440 nm): a luz espalhada fica bege;
  - quase não muda a cor do sol (Ångström ~0,2, poeira grossa), só tira a luz dele: a 8°, o aerossol sozinho deixa passar 49% com AOD 0,1, 12% com 0,3 e 3% com 0,5;
  - lava as cores: poeira e névoa atenuam as cores em vez de reforçar, o céu de dia fica acinzentado ou branco e os laranjas do pôr do sol viram amarelos e rosas pálidos; **ar limpo é o ingrediente principal de um pôr do sol colorido** ([Corfidi, NOAA SPC](https://www.spc.noaa.gov/publications/corfidi/sunset/), em paráfrase).
- **Da AOD pros parâmetros da Unreal:** AOD = β·H, então **β = AOD/H**. `Mie Scattering Scale` = albedo × β, `Mie Absorption Scale` = (1 − albedo) × β. Exemplo: AOD 0,15 com H 2 km e albedo 0,95 → β 0,075/km, espalhamento 0,071, absorção 0,004.

| AOD | H = 1,2 km | H = 2 km | H = 3 km |
|---|---|---|---|
| 0,10 | 0,083/km | 0,050/km | 0,033/km |
| 0,15 | 0,125 | 0,075 | 0,050 |
| 0,30 | 0,250 | 0,150 | 0,100 |
| 0,50 | 0,417 | 0,250 | 0,167 |

- **A fase:** g real da poeira 0,65–0,76, mas a Henyey-Greenstein subestima a auréola (Bruneton 2017, [ar5iv 1612.04336](https://ar5iv.labs.arxiv.org/html/1612.04336): todos os modelos dão o céu escuro demais perto do sol). Manter 0,75–0,8.
- **Lembrar a §8.6:** subir a AOD do céu corta a luz do sol no chão. Todo estilo com mais poeira compensa o sol.

### 5.3 As nuvens do deserto

**As alturas** (OMM, [International Cloud Atlas, níveis](https://cloudatlas.wmo.int/en/some-useful-concepts-levels.html)):

| nível | nuvens | trópicos | temperado |
|---|---|---|---|
| alto | cirro, cirrocúmulo, cirrostrato | 6–18 km | 5–13 km |
| médio | altocúmulo, altostrato | 2–8 km | 2–7 km |
| baixo | estratocúmulo, cúmulo, cumulonimbo (base) | 0–2 km | 0–2 km |

- **Cúmulo de deserto tem base alta:** em regiões áridas a base pode passar de 6 km ([Cumulus, Wikipedia](https://en.wikipedia.org/wiki/Cumulus_cloud)). Regra de Espy: base ≈ 125 m × (T − Td) ([LCL](https://en.wikipedia.org/wiki/Lifting_condensation_level)); 40 °C com orvalho a 5 °C dá ~4,4 km (**est.**).
- **Saara:** uma camada de altocúmulo aparece com frequência no topo da camada de poeira, perto de 500 hPa (~5,6 km), quase toda com água super-resfriada no topo ([Stein et al. 2011](https://www.met.reading.ac.uk/~swrhgnrj/publications/wafrica_monsoon_cloudsat.pdf)).
- **Altocúmulo:** cada elemento tem 1–5° de largura aparente ([Cloud Atlas, Ac](https://cloudatlas.wmo.int/en/altocumulus-ac.html)).
- **Haboob:** a parede de poeira que sai da descida fria de um cumulonimbo; até 2 km de altura e 100 km de largura ([Haboob](https://en.wikipedia.org/wiki/Haboob)). Fica de ideia pra mapa de tempestade, não pro céu padrão.

### 5.4 O que dá o "uau" sem perder a física
- **Nuvem média e alta, ar limpo embaixo.** Os vermelhos e laranjas fortes aparecem em cirro e altocúmulo, raramente em nuvem baixa: a nuvem tem de estar alta o bastante pra receber sol que não passou pela névoa de baixo (Corfidi).
- **Com o sol a +8° não existe nuvem iluminada por baixo** (isso é depois do pôr: uma nuvem a 5 km fica iluminada até o sol descer ~2,3° abaixo do horizonte, conta geométrica). A 8° o "uau" vem de:
  - cirro ou altocúmulo perto do sol com a **borda prateada** (espalhamento pra frente);
  - **torres ao longe com topo quente e base azul-acinzentada** (a luz do céu na base, o sol no topo);
  - **raios de luz** saindo dos buracos de um altocúmulo quebrado (a sombra da nuvem no ar, §6.7).
- **Contraste quente e frio:** sol 3.500–4.700 K contra a sombra azul do céu, em contraluz de três-quartos (o padrão dos game shots).
- **Faixas de poeira sugeridas pros estilos** (**est.**): dourado limpo AOD 0,10–0,15; poeira leve ~0,3; evitar ≥ 0,5 (sol a ~3%, cores lavadas).

---

## 6. A Unreal 5.8: o que o fonte instalado diz

Tudo lido em `C:\Program Files\Epic Games\UE_5.8\Engine\` (Source/Runtime/Renderer/Private, Source/Runtime/Engine, Shaders/Private, Content/EngineSky).

### 6.1 O componente `VolumetricCloud` (padrões em `VolumetricCloudComponent.cpp`)

| propriedade | padrão | o que é |
|---|---|---|
| `LayerBottomAltitude` | 5 km | base da camada |
| `LayerHeight` | 10 km | espessura da camada |
| `TracingStartMaxDistance` | 350 km | não começa a traçar se a camada estiver mais longe que isso |
| `TracingMaxDistance` | 50 km | quanto traça dentro da camada (a partir da entrada, `TracingMaxDistanceMode`) |
| `PlanetRadius` | 6360 km | só se não houver SkyAtmosphere |
| `GroundAlbedo` | 0,4 cinza | luz que sobe do chão pra base da nuvem (só com `GroundContribution` no material) |
| `bUsePerSampleAtmosphericLightTransmittance` | falso | ver §6.6 |
| `SkyLightCloudBottomOcclusion` | 0,5 | escurece a luz do céu na base |
| `ViewSampleCountScale` | 1,0 | × 96 amostras de base (`BaseViewRaySampleCount`) |
| `ShadowViewSampleCountScale` | 1,0 | × 10 amostras de sombra (`BaseShadowRaySampleCount`) |
| `ShadowTracingDistance` | 15 km | alcance do raio de sombra até o sol |
| `StopTracingTransmittanceThreshold` | 0,005 | para quando a nuvem já ficou opaca |
| `AerialPespective{Rayleigh,Mie}Scattering{Start,Fade}Distance` | 0 | controle artístico da perspectiva aérea na nuvem |
| `Material` | `m_SimpleVolumetricCloud_Inst` | |

**As amostras:** o número de passos vai de `SampleMinCount` (2) até 96 × escala, crescendo com o comprimento do raio até `r.VolumetricCloud.DistanceToSampleMaxCount` (15 km). Perto do horizonte todo raio é longo, então **todo pixel do céu da câmera deitada paga o máximo**. Com 50 km de traçado e 96 passos, cada passo tem ~520 m (`VolumetricCloud.usf:710-713`): um cúmulo pequeno fica sub-amostrado e vira ruído que a reconstrução temporal esconde. O raio de sombra: 10 passos em 15 km = 1,5 km por passo, grosso pra auto-sombra com o sol a 8°.

### 6.2 As cvars de custo (`VolumetricCloudRendering.cpp`, `VolumetricRenderTarget.cpp`)

| cvar | padrão | efeito |
|---|---|---|
| `r.VolumetricCloud` | 1 | liga/desliga (escalável) |
| `r.VolumetricCloud.ViewRaySampleMaxCount` | 768 | teto das amostras da vista |
| `r.VolumetricCloud.ReflectionRaySampleMaxCount` | 80 | teto nos reflexos (captura da SkyLight) |
| `r.VolumetricCloud.Shadow.ViewRaySampleMaxCount` | 80 | teto da sombra secundária |
| `r.VolumetricCloud.SampleMinCount` | 2 | |
| `r.VolumetricCloud.StepSizeOnZeroConservativeDensity` | 1 | pulo quando a densidade conservadora é zero (>1 acelera, pode dar faixas) |
| `r.VolumetricCloud.EmptySpaceSkipping` | 0 | experimental |
| `r.VolumetricCloud.ShadowMap` | 1 | o mapa de sombra da nuvem (só se o sol tiver `Cast Cloud Shadows`) |
| `r.VolumetricCloud.ShadowMap.MaxResolution` | 2048 | (base 512 × `CloudShadowMapResolutionScale`) |
| `r.VolumetricCloud.ShadowMap.RaySampleMaxCount` | 128 | |
| `r.VolumetricCloud.ShadowMap.RaySampleHorizonMultiplier` | 2,0 | mais amostras com o sol no horizonte (o nosso caso) |
| `r.VolumetricCloud.ShadowMap.SnapLength` | 20 km | |
| `r.VolumetricCloud.ShadowMap.LightDistanceOverride` | 0 | ver §8 (sol baixo) |
| `r.VolumetricCloud.SkyAO` | 1 | AO da nuvem na luz do céu (só se a SkyLight tiver `Cloud Ambient Occlusion`) |
| `r.VolumetricCloud.EnableAerialPerspectiveSampling` | 1 | perspectiva aérea na nuvem |
| `r.VolumetricCloud.HighQualityAerialPerspective` | 0 | 2ª passada por pixel |
| `r.VolumetricCloud.ApplyFogLate` | 1 | a névoa de altura entra na nuvem depois da reconstrução |
| `r.VolumetricCloud.EnableLocalLightsSampling` | 0 | luzes locais na nuvem, "Expenssive!" |
| `r.VolumetricRenderTarget` | 1 | traçado em resolução reduzida + reconstrução |
| `r.VolumetricRenderTarget.Mode` | 0 | ver tabela abaixo |
| `r.VolumetricRenderTarget.UpsamplingMode` | 4 | bilateral |
| `r.VolumetricRenderTarget.PreferAsyncCompute` | 0 | **inútil na 1660** (a Unreal não liga async compute em NVIDIA antes da Ampere, `docs/OTIMIZACAO.md`) |

**Os modos do `r.VolumetricRenderTarget.Mode`** (`VolumetricRenderTarget.cpp:121-151`):

| modo | traça em | reconstrói em | raios a 1080p com 100% | observação |
|---|---|---|---|---|
| 0 (padrão) | 1/4 × 1/4 da tela | metade | 480×270 = 130 mil | o normal |
| 1 | metade | tela cheia | 960×540 = 518 mil | 4× os raios do 0 |
| 2 | 1/4 | tela cheia | 130 mil | "não cruza com malha opaca": traça 1 de cada 4×4 e reaproveita 15, nítido mas com atraso, e a nuvem vaza por cima das malhas (Hillaire, [fórum da Epic](https://forums.unrealengine.com/t/volumetriccloud/688516)); arriscado com duna e prédio no horizonte |
| 3 | tela cheia | — | 2,07 milhões | cinemática |

A resolução que conta é a interna (antes do TSR): nos presets do jogo, 58/67/75/100% (`docs/OTIMIZACAO.md`).

**`BaseScalability.ini` da 5.8 não mexe em nenhuma cvar de nuvem.** A escala por preset fica por nossa conta (no `DefaultScalability.ini` do jogo e nas propriedades do componente).

### 6.3 Na câmera normal a nuvem quase não custa

`VolumetricCloud.usf:573-640`: antes de marchar, o raio lê a profundidade da cena e **pula o pixel se o chão está na frente da camada** ("Skip tracing if the depth buffer is in front of the cloud layer"). Na câmera normal e no zoom, 100% dos pixels são chão a 60–250 m e a camada começa a quilômetros: o traçado da vista é ~zero. O que continua pagando:
- o mapa de sombra da nuvem, se ligado (512² por padrão, com 32 amostras por texel com o sol a 8°, §7.3);
- o mapa de AO da luz do céu, se ligado;
- a reconstrução e a composição (passes de tela em resolução reduzida);
- a captura da SkyLight, se for em tempo real (§6.5).

### 6.4 O material

`Engine/Content/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud` (lido pelos nomes dentro do `.uasset`; os valores padrão só no editor):
- **Grupos:** `00 - Cloud Layout`, `01 - Cloud Shape`, `02 - Storm`, `03 - Multiscatter`, `Phase_Controls`.
- **Tipos de nuvem por máscara de layout:** `Altostratus`, `Cirrostratus`, `Stratocumulus`, `Nimbostratus (Storm)`. Não tem tipo "cúmulo de torre" nem bigorna: pra "poderosa" a gente faz o perfil de altura.
- **Parâmetros:** `Cloud_GlobalCoverage`, `Cloud_GlobalDensity`, `Cloud_AlbedoColor`, `WindMultiplier`, `Noise_Strength`, `Noise_Bias`, `PhaseG`, `PhaseG2`, `PhaseBlend`, `MultiScatteringContribution/Occlusion/Eccentricity`, `Fade Near Camera`, `Prevent Shadowing from Above Clouds`, os raios (`Storm_Lightning*`).
- **Texturas:** `VT_PerlinWorley_Balanced` (volume Perlin-Worley, o ruído do Schneider), `T_CloudMask`, `T_CloudPattern`, `T_Profile_08` (perfil de altura), `T_VolumeNoiseShape128/64`, `T_VolumeNoiseErosion32`.
- Um comentário no grafo: "scaled for clouds at a max height of 8km maxVisDist = sqrt(h^2+2(6371*h))" (a 8 km o horizonte da nuvem está a ~320 km).

**O nó `Volumetric Advanced Output`** (`MaterialExpressionVolumetricAdvancedMaterialOutput.h`):
- `MultiScatteringApproximationOctaveCount`: **0 a 2, fixo na compilação** ("0 means single scattering only. The maximum value is 2 (expenssive)"). Mudar por preset exige materiais diferentes.
- `ConstMultiScatteringContribution/Occlusion/Eccentricity`: 0,5 / 0,5 / 0,5 (as oitavas do Wrenninge, ver §3).
- `PhaseG`, `PhaseG2`, `PhaseBlend`: as duas Henyey-Greenstein misturadas.
- `bRayMarchVolumeShadow` (verdadeiro): sombra marchada até o sol. Falso usa o mapa de sombra da nuvem: "usually enough for clouds viewed from the ground and it result in a performance boost", mas a sombra fica cinza e menos precisa.
- `bGroundContribution`: luz do chão na base da nuvem (custo extra). No deserto, com a areia clara, é o que deixa a base quente.
- `ConservativeDensity`: a densidade grosseira (o mapa de cobertura 2D) que pula o material caro onde não tem nuvem.
- **Sinal do g:** o tooltip diz "forward(g<0)", mas o shader (`SamplePhaseFunction`, `VolumetricCloud.usf:327`, com `HenyeyGreensteinPhase(G, -cos)` e a direção do sol apontando pra luz) dá o **brilho pra frente com g positivo**. Na prática: g ≈ +0,6 a +0,8 = a borda prateada olhando pro sol.

### 6.5 A luz do céu (SkyLight)

- **Captura em tempo real** (`bRealTimeCapture`, padrão falso): pega SkyAtmosphere, nuvem e malhas de céu. É a única forma de a luz do céu acompanhar nuvem que anda.
- O custo é fatiado em quadros: `r.SkyLight.RealTimeReflectionCapture.TimeSlice` 1, `...TimeSlice.SkyCloudCubeFacePerFrame` 2 (a nuvem em 2 faces do cubo por quadro, "Default preferred for 60hz"), `...VolumetricCloudResolutionDivider` 2 (nuvem na captura em meia resolução).
- O editor avisa de captura cara com resolução ≥ 512 e nuvem (`SceneRendering.cpp:4570`).
- **O custo pela Epic** ([Sky Lights](https://dev.epicgames.com/documentation/en-us/unreal-engine/sky-lights-in-unreal-engine)): cubo de 128 no PS4, 1,465 ms o processo inteiro; fatiado em 9 quadros, no máximo 0,20 ms por quadro. O `RecaptureSky` custa "pelo menos 120 ms" (um engasgo só, na aplicação da receita).
- **Hoje o jogo usa captura estática** (`LookDirector.cpp:178-181` chama `RecaptureSky` quando não é tempo real). Com nuvem parada no céu basta. Com nuvem andando, a luz do céu fica com a foto do começo.
- `Lower Hemisphere Is Solid Color` (padrão verdadeiro, cor preta): a metade de baixo da captura é preta. Com Lumen o rebote da areia vem do GI; sem Lumen (preset baixa) vale pôr a cor da areia lá.
- `Cloud Ambient Occlusion` na SkyLight (padrão desligado): escurece a luz do céu debaixo da nuvem (mapa de 512 × escala, raio 150 km).

### 6.6 A cor da nuvem na hora dourada

- Sem `bUsePerSampleAtmosphericLightTransmittance`, a nuvem inteira recebe **a luz do sol do jeito que chega no chão** (`AtmosphereLightIlluminanceOnGroundPostTransmittance`, `VolumetricCloud.usf:650-655`). A 8° com a nossa poeira, isso é laranja forte.
- Com ela ligada, cada amostra pega a transmitância da sua altitude: a nuvem alta recebe luz mais branca que o chão, e o topo de um cúmulo alto fica mais claro que a base. É o que a física manda e o que dá o topo dourado com base fria nas torres ao longe.
- `CloudScatteredLuminanceScale` (no sol, padrão branco): compensa a dispersão múltipla aproximada ("can help counter balance the fact that our multiple scattering solution is only an approximation").

### 6.7 As sombras de nuvem (o sol, `DirectionalLightComponent`)

| propriedade | padrão | |
|---|---|---|
| `bCastCloudShadows` | falso | cria o mapa de sombra da nuvem |
| `CloudShadowStrength` | 1,0 | força geral |
| `CloudShadowOnAtmosphereStrength` | 1,0 | sombra no ar: os **raios crepusculares** no céu |
| `CloudShadowOnSurfaceStrength` | 1,0 | sombra no chão e nas unidades |
| `CloudShadowExtent` | 150 km | **raio** do mapa em volta da câmera |
| `CloudShadowMapResolutionScale` | 1,0 | × 512 |
| `CloudShadowRaySampleCountScale` | 1,0 | |
| `CloudShadowDepthBias` | 0 | |
| `bCastShadowsOnClouds` | falso | malha opaca sombreando a nuvem |
| `bCastShadowsOnAtmosphere` | falso | |

A sombra no ar entra na tabela do céu e na da perspectiva aérea como permutação de shader (`SkyAtmosphereRendering.cpp:1660-1675`): custo de tabela, não de tela.

### 6.8 A ordem de composição (`DeferredShadingRenderer.cpp:3740-3924`)

Sem a composição separada da névoa (o nosso caso):
1. céu (SkyAtmosphere) nos pixels sem nada;
2. névoa de altura (com a volumétrica) em todos os pixels, **inclusive os do céu**;
3. volumes de névoa locais;
4. a nuvem composta por cima, com a névoa aplicada nela pela distância dela (`ApplyFogLate`);
5. **depois** as extensões pós-opacas: é aqui que roda a passada da névoa de guerra do jogo (`RTSFogPass.cpp`, `FPostOpaqueRenderParameters`);
6. translúcidos.

---

## 7. A recomendação pro nosso caso

### 7.1 Antes de qualquer nuvem: destravar o céu

Sem isso nenhum estilo aparece (§8.2 e §8.3). As duas mudanças valem só com a câmera deitada, e a câmera normal aprovada não muda:
1. **A névoa de altura sai do céu:** `FogCutoffDistance` ~2 km (A/B 1,5 / 3 / 6 km). A câmera normal só vê chão a 60–250 m e fica idêntica.
2. **A perspectiva aérea ×40 vira ~×4 abaixo de 20° de inclinação** (A/B 2 / 4 / 8). O `LookDirector` mistura pela inclinação, do mesmo jeito que já ajusta o começo da névoa pela distância (`UpdateFocus`).

**Um passo de cada vez:** primeiro esse céu "destravado" com a atmosfera aprovada, sem nuvem nenhuma. Só a névoa sai do céu, então o chão não muda. A AOD da receita (~0,03–0,04) é de ar limpo: o céu deve sair mais azul que o alvo. Esse é o estilo B0 abaixo.

### 7.2 Os quatro estilos pro A/B

Todos com o sol aprovado (8°, contraluz pela direita, 4.700 K) e a câmera normal idêntica à aprovada. Os números são **pontos de partida com fonte** pra medir, não o final.

**A. Poeira dourada (o alvo, `day_tilt_cam`)**
- **Céu:** o de hoje, com a névoa de altura no céu (sem o corte da §7.1). A faixa de poeira vem da névoa. Calibrar a cor e o brilho da névoa contra os 16 pontos medidos do alvo (§2): pêssego `#ECDBCF` em cima, dourado `#FFECBA` no sol, malva `#A7898D` no horizonte oposto.
- **Nuvem:** nenhuma volumétrica. No máximo fiapos de cirro numa cúpula 2D de material de céu (o alvo tem uns fiapos).
- **A física:** é o céu de poeira do Corfidi (leitoso, pastel). O malva do lado oposto é licença: a 8° o cinturão de Vênus não existe (§5.1).
- **Custo:** ~0 (a cúpula, se houver, **est.** < 0,05 ms).

**B. Azul limpo (o "céu azul LINDO")**
- **B0:** a atmosfera aprovada + a §7.1. Zero mudança na luz do chão.
- **B1, se o B0 sair limpo demais:** AOD 0,12 com H 2 km (entre a seca de Tamanrasset e Solar Village, §5.2):
  - `MieScatteringScale` 0,057, cor branca (poeira grossa, Ångström ~0,2);
  - `MieAbsorptionScale` 0,003, cor (0,5; 0,7; 1);
  - `MieAnisotropy` 0,8; `MieExponentialDistribution` 2,0.
  - **Compensar o sol** (§8.6): a transmitância a 8° cai de 0,51/0,31/0,13 pra 0,31/0,17/0,07; o sol sobe ×1,7 / ×1,8 / ×1,9 (R/G/B, **est.**). O céu sobe junto e volta pelo `SkyAndAerialPerspectiveLuminanceFactor`, medido contra o alvo.
- **Nuvem:** fiapos de cirro perto do sol (a borda prateada) na cúpula 2D. É o que o HZD fazia acima de 4 km (§3.3).
- **O que esperar** (§5.1): azul claro a 90° do sol entre 8° e 14°, branco-dourado em volta do sol, horizonte claro.
- **Custo:** ~0.

**C. Altocúmulo dourado (o céu "escamado")**
- **Atmosfera:** a do B que ganhar.
- **Camada:** `LayerBottomAltitude` 4,5 km, `LayerHeight` 1,5 km. Altocúmulo nos trópicos fica entre 2 e 8 km; no Saara aparece no topo da camada de poeira, perto de 5,6 km (§5.3). A base a 4,5 km aparece a 8,4° quando está a 30 km: a camada cobre a metade de cima da faixa visível.
- **Forma:**
  - cobertura 0,4–0,5 (quebrado);
  - células de 0,5–1,5 km: um elemento de altocúmulo tem 1–5° aparentes (OMM), e a 20–40 km isso dá 0,35–3,5 km.
- **Luz:**
  - fase G 0,8 / G2 −0,5 / mistura 0,5 (o Frostbite; g positivo = pra frente na Unreal);
  - 1 oitava de dispersão múltipla (2 no ultra) com 0,5/0,5/0,5;
  - transmitância da atmosfera por amostra ligada (topo mais claro que a base);
  - sombra no ar 1,0 (os raios pelos buracos).
- **Por que dá "uau":** os laranjas fortes aparecem em altocúmulo e cirro, não em nuvem baixa (Corfidi). Com o sol a 8°, a borda prateada aparece no lado do sol e os raios pelos buracos.

**D. Torres no horizonte (as "nuvens volumétricas poderosas")**
- **Atmosfera:** a do B (ar limpo: as torres mantêm a cor).
- **Camada:** `LayerBottomAltitude` 3,5 km, `LayerHeight` 8,5 km (topo a 12 km).
  - Cúmulo de deserto tem base alta: a regra de Espy dá ~4,4 km com 40 °C e orvalho a 5 °C (§5.3).
  - Nos trópicos a nuvem alta vai até 18 km.
- **Onde ficam:**
  - mapa de cobertura com uma **máscara radial**: zero até ~15 km do centro do mapa, subindo pra 0,3–0,5 entre 25 e 60 km. É a regra do Horizon ("a partir de 15 km, cúmulo com ~50% de cobertura, pra o horizonte ser sempre épico", §3.3);
  - uma torre a 40 km vai de 4,6° (base) a 16,5° (topo): enche a faixa visível;
  - nada acima do campo de batalha: sem sombra na partida e nada entre a câmera e as unidades.
- **Forma:**
  - perfil de cúmulo (base chata, topo redondo) e, em 1 ou 2 torres, cumulonimbo com bigorna (o perfil abre perto do topo);
  - erosão só na casca e fiapos na base (HZD);
  - o `m_SimpleVolumetricCloud` não tem tipo de torre nem bigorna: precisa do nosso material (§7.3).
- **Luz:**
  - fase 0,8 / −0,5 / 0,5;
  - 2 oitavas no ultra (1 na alta);
  - transmitância por amostra (topo dourado, base azul-acinzentada);
  - **contribuição do chão** ligada (a areia quente na base; "muito importante pra perceber a forma", Hillaire 2020);
  - sombra no ar 1,0.
- **Traçado:** `TracingMaxDistance` 50 km (A/B com 80). `ViewSampleCountScale` ≥ 1: com 96 passos em 50 km cada passo tem ~520 m, grosso pra torre (§6.1).
- **Densidade:** o núcleo do cúmulo físico fica em ~0,03–0,15 /m (**est.**, pela fórmula de Stephens 1978, σ = 3·LWC / (2·ρ_água·r_ef), com LWC 0,2–1 g/m³ e r_ef ~10 µm; valores de livro, não reabertos nesta sessão). A 40 km a torre é opaca de qualquer jeito: a densidade controla mesmo a maciez da borda.

### 7.3 O material e as configurações por preset

**O material (`M_RTSCloud`, proposta):** a mesma estrutura do `m_SimpleVolumetricCloud`, com o que falta:
- **Mapa de tempo 2D:** cobertura no R, tipo no G (altocúmulo, cúmulo, cumulonimbo), máscara radial no B.
- **Perfil de altura por tipo:** laje fina pro altocúmulo, torre com bigorna pro cumulonimbo.
- **Ruído:** `VT_PerlinWorley_Balanced` pra forma, `T_VolumeNoiseErosion32` pra erosão.
- **`ConservativeDensity`** = cobertura × perfil, pra pular o vazio.
- **Albedo** ~1 (Hillaire 2020: a nuvem quase não absorve), um pouco menor na borda pra ter a borda escura.
- **A nuvem fica parada no lugar;** o vento só mexe no ruído de detalhe. Assim a captura estática da luz do céu continua valendo (§6.5).
- **Oitavas:** 1 ou 2 são fixas na compilação. A alta e o ultra usam instâncias de materiais diferentes (§8.7).

**As configurações** (resolução e GI de cada preset pela tabela de `docs/OTIMIZACAO.md`; a nuvem é o estilo D, o pior caso):

| | baixa | média | alta | ultra |
|---|---|---|---|---|
| resolução interna | 58% | 67% | 75% | 100% |
| nuvem volumétrica | desligada | modo 0, amostras ×0,5 | modo 0, amostras ×1 | modo 0 (A/B com o 1), amostras ×2 |
| oitavas | — | 1 | 1 | 2 |
| auto-sombra | — | pelo mapa de sombra (`bRayMarchVolumeShadow` falso) | marchada, 10 amostras | marchada, ×1,5 (15) em 8 km |
| transmitância por amostra / luz do chão | — | não / não | sim / não | sim / sim |
| mapa de sombra da nuvem | — | 256² (×0,5) | 512² | 512² |
| sombra no ar (raios) / no chão | — | 0 / 0 | 1 / 0 | 1 / 0 |
| luz do céu | estática | estática | estática | captura em tempo real (fatiada) |
| o céu da baixa | estilo A ou B sem nuvem; A/B com uma "nuvem assada" (a do ultra fotografada numa cúpula, proposta) | | | |

**O custo, câmera deitada no limite (3°, ~41% da tela é céu)** (**est.**, método abaixo):

| | baixa | média | alta | ultra |
|---|---|---|---|---|
| raios de céu por quadro (modo 0 = 1/16 dos pixels) | — | ~24 mil | ~30 mil | ~53 mil |
| amostras por quadro (vista + sombra + mapa) | — | ~3 mi | ~20 mi | ~65 mi |
| **nuvem, est.** | 0 | **0,2–0,4 ms** | **0,5–1,0 ms** | **1,5–3 ms** |
| captura em tempo real da luz do céu, est. | — | — | — | ≤ 0,1–0,15 ms por quadro |
| **câmera normal** (nuvem desligada pela inclinação, §7.4) | 0 | 0 | 0 | 0 |

**Como saiu a estimativa** (não há medida publicada de VolumetricCloud em placa da classe da 1660):
- **Âncora 1, HZD:** ~2 ms no PS4 pra 130 mil raios por quadro (1/16 de 1080p), 64–128 passos e 6 amostras de luz (§3.3).
- **A placa:** a 1660 tem ~2,7× o cálculo do PS4 (5,0 contra 1,84 TFLOPS) mas só ~1,1× a banda (192 contra 176 GB/s). São especificações dos fabricantes, de memória, não reabertas nesta sessão. Nuvem lê muita textura 3D, então tomei um ganho de 1,5–2,7×.
- **A conta:** escalei pelas amostras (raios × passos × fração de céu + sombra). Supus ~30% dos passos dentro da nuvem (a sombra marchada só roda lá) e ~0,1–0,2 ms fixos de reconstrução e composição.
- **Âncora 2, Manor Lords** (Unreal 5, VolumetricCloud): ~2,3 ms no Steam Deck (§4). O Deck tem 1,6 TFLOPS e 88 GB/s; a 1660 faria isso em ~0,7–1,0 ms. Bate com a alta.
- **Pegadinha do custo, o mapa de sombra com o sol baixo:** com o sol a 8° o motor dobra as amostras do mapa (`RaySampleHorizonMultiplier` 2): 512² × 32 = **8,4 milhões de amostras por quadro**, tanto quanto a vista (`VolumetricCloudRendering.cpp:1889-1895`). Na média ele cai pra 256².
- **O céu da atmosfera já é pago hoje:** ~0,5 ms (**est.**, §3.1).
- **Toft et al.** (GTX 1080): 1/4 da resolução com 8 passos e TAA custa 2,4 ms. O teto é alto se as amostras subirem.
- **A medida de verdade:** `ProfileGPU` e CSV na 1660, pela trava da placa (§9).

### 7.4 A câmera normal (37,5°, sem céu no quadro)

**Sombra de nuvem no chão: não, com a luz aprovada (sol a 8°). Força na superfície 0.**
- **A física:** com o sol a 8° a sombra de nuvem não é mancha passando; é a tela inteira escurecendo e clareando devagar (§8.4). Isso troca a luz que o Felipe aprovou no meio da partida.
- **A leitura:** debaixo de uma nuvem grossa a areia perde o sol direto e cai pra ~27% do brilho (a razão sombra/sol medida na receita, volta 35). Uma nuvem de 1 km faz sombra de ~7 km na direção do sol: com vento de 10 m/s, de ~2 a ~12 minutos de batalha escura, conforme o vento (**est.**). Isso atrapalha a leitura e não diz nada ao jogador. É o "efeito que polui a leitura" da regra.
- **Com a força na superfície em 0, a névoa volumétrica também não pega a sombra da nuvem.** A Unreal só liga a sombra de nuvem na névoa quando essa força é > 0 (`VolumetricFog.cpp:1667`). A poeira dourada aprovada fica intocada.
- **Os RTS (§4):** nenhum publica força nem tamanho. Civ VI e Cities II tratam como opção que se desliga, e jogador de Civ diz que piora a leitura.
- **Se um dia houver mapa com sol alto (≥ 30°):** aí a sombra vira mancha de 1–3 km, ainda maior que o quadro (100–250 m). Pra ler como "nuvem passando" no RTS precisaria de manchas menores que a física dá; isso é efeito artístico (projeção de textura no sol), não a sombra da Unreal. Fica pra esse mapa, com A/B.

**O sol continua com `Cast Cloud Shadows` ligado nos presets que têm nuvem,** por três motivos:
- a sombra no ar faz os raios crepusculares no céu da câmera deitada (`CloudShadowOnAtmosphereStrength` 1,0);
- é a auto-sombra barata da nuvem quando o material usa `bRayMarchVolumeShadow` falso (média);
- custa uma passada fora da tela, mas não é de graça com o sol baixo: 512² × 32 amostras = 8,4 milhões por quadro (§7.3).

**A nuvem só existe quando o céu pode aparecer.** Na câmera normal o raio já para no chão (§6.3), mas a reconstrução, a composição e o mapa de sombra continuam pagando (o mapa sozinho tem tantas amostras quanto a vista na alta). O `LookDirector` (que já lê a câmera todo quadro) desliga o componente acima de ~20° de inclinação e liga abaixo, antes do horizonte entrar (15,8°). Os quadros entre ligar e o horizonte aparecer dão tempo à reconstrução temporal convergir (**est.** 4–8 quadros; conferir em vídeo que não pisca).

---

## 8. As pegadinhas (lidas no fonte e na receita aprovada)

Em ordem de quanto estragam o céu. As contas são **est.** pelas fórmulas do shader; conferir com captura antes de mexer.

### 8.1 O céu preto acima do horizonte: era a névoa de guerra (já corrigido)
- A passada da névoa de guerra multiplica a cor da tela. Ela roda depois do céu e da névoa (passo 5 da §6.8) e multiplicava o céu por zero.
- A sessão do Look corrigiu em 14/10: pixel sem profundidade (`DeviceZ <= 0`, o céu) leva 1,0 (`Game/Shaders/Private/FogOfWar.usf:67-73`).
- **A nuvem passa livre:** ela não escreve profundidade, então o pixel continua "céu".
- **O que sobrou:** fora do mapa, a névoa de guerra multiplica o chão por 0,212 (o "já visto" do ZH) depois da névoa e da perspectiva aérea. As dunas além do mapa saem a ~21% do brilho, com a poeira dourada por cima escurecida junto. No alvo as dunas ao longe são claras e esfumaçadas. Contra o céu inteiro (1,0), isso vira uma faixa escura no horizonte. Sugestão pro dono da névoa de guerra: além da borda, voltar a 1,0 com a distância (nenhuma unidade anda lá).
- **As outras causas clássicas de céu preto na Unreal** (conferir se voltar):
  - falta a SkyAtmosphere, ou o sol está sem "Atmosphere Sun Light" (o `LookDirector` liga: `LookDirector.cpp:143`);
  - "Support Sky Atmosphere" desligado no projeto; um bug na troca da 5.0.3 pra 5.1 (UE-174156) ([fórum da Epic](https://forums.unrealengine.com/t/sky-atmosphere-is-straight-up-black-and-wont-change/541255));
  - abaixo do horizonte, a atmosfera "não tem névoa, então não tem cor": a Epic manda pôr terreno ou névoa ([SkyAtmosphere](https://dev.epicgames.com/documentation/en-us/unreal-engine/sky-atmosphere-component-in-unreal-engine));
  - a SkyLight vem com a metade de baixo preta (`Lower Hemisphere Is Solid Color`, cor preta);
  - exposição manual baixa demais pro céu.

### 8.2 A névoa de altura cobre o céu da câmera deitada
- A névoa de altura entra em **todo** pixel, inclusive o do céu (§6.8). A densidade some com a altura, mas um raio quase horizontal atravessa muita névoa.
- A conta (`HeightFogCommon.ush`: densidade/1000 e queda/1000 por cm, base 2, `SceneCore.cpp:404`), com a receita aprovada (densidade 0,06, queda 0,25, a névoa caindo pela metade a cada 40 m), supondo a base da névoa no chão (a altura do ator da névoa; conferir no mapa) e ignorando o começo a 100 m, a névoa no pixel de céu (**est.**):

| câmera a | 3° | 5° | 10° | 14° |
|---|---|---|---|---|
| 5 m (deitada) | 95% | 83% | 58% | 47% |
| 30 m | 85% | 68% | 43% | 34% |
| 60 m | 67% | 49% | 29% | 22% |

- **O que isso quer dizer:** na câmera deitada, a faixa de céu que aparece é metade névoa (a cor da névoa, não o céu da atmosfera). A nuvem é composta com a névoa da distância dela (`ApplyFogLate`): **uma torre a 5° some 83% na névoa.**
- **A alavanca da Unreal:** `FogCutoffDistance` ("Scene elements past this distance will not have fog applied"; no shader, `HeightFogCommon.ush:397`, a névoa vira zero além dela). Com um corte em ~2 km (**est.**), a câmera normal (só chão a 60–250 m) não muda nada e o céu e as nuvens saem limpos da névoa de altura. A faixa de poeira no horizonte passa a vir da atmosfera (o Mie, §5), igual pro céu, pro chão longe e pras nuvens. Testar 1,5 / 3 / 6 km, conferindo se aparece degrau na névoa do chão na distância do corte.
- **Outra saída:** o `LookDirector` já ajusta a névoa pela distância da câmera (`UpdateFocus`); ele pode também ajustar pela inclinação, só abaixo de ~20° (o horizonte entra a 15,8°). A câmera normal aprovada fica intocada por construção.

### 8.3 A perspectiva aérea ×40 apaga as nuvens
- A receita aprovada usa `aerial_distance_scale` 40 (`desert_golden_dust.json`), pra a atmosfera dar véu nas distâncias curtas do RTS.
- O céu não sente isso: o pixel de céu usa escala 1 (`SkyAtmosphere.usf:1111`).
- **A nuvem sente.** Ela pega a perspectiva aérea do volume da câmera (`VolumetricCloud.usf:1535`), e esse volume é calculado **com a escala** (`SkyAtmosphere.usf:1681-1690`).
- A conta (Rayleigh padrão + o Mie da receita, 0,05+0,005/km, altura 0,8 km): uma nuvem a 3 km de altura vista a 10° deixa passar 72/64/49% (R/G/B) com escala 1 e **0,000** com escala 40. Com 40 a nuvem inteira vira uma mancha lisa da cor da névoa da atmosfera: some a forma, a borda prateada, tudo.
- **Saídas:** (a) o `LookDirector` baixa a escala com a câmera deitada (o chão longe também ganha: a escala 40 a 2 km equivale a 80 km de ar e apaga a forma das dunas, que o alvo mostra); (b) `r.VolumetricCloud.EnableAerialPerspectiveSampling 0` e a perspectiva da nuvem vem só da névoa. A (a) é a certa: a escala é um truque pra câmera de cima.

### 8.4 A sombra de nuvem com o sol a 8°
- **A geometria:** a sombra de uma nuvem a altura h cai a h/tan 8° = **7,1·h** de distância, esticada **7,2×** na direção do sol. A penumbra (o disco do sol, 0,53°, a 7,2·h de distância) tem ~200 m pra nuvem a 3 km.
- **No quadro do RTS** (100–250 m de chão): uma sombra de nuvem não passa como mancha; ela escurece **a tela inteira devagar**, a borda levando ~20 s pra atravessar com vento de 10 m/s (**est.**).
- **No mapa da Unreal:** o padrão (raio 150 km, 512 texels) dá ~586 m por texel; no chão, com o sol a 8°, ~4 km na direção do sol. Mesma conclusão: borrado na escala da batalha.
- **Pegadinha de quem reduzir o raio pra ganhar nitidez:** o shader pula a sombra inteira se a origem do raio (2× o raio do mapa pra trás, na direção do sol) ficar abaixo da camada ("We are inside under the cloud layer, we simply skip shadowing evaluation", `VolumetricCloud.usf:2139`). Com o sol a 8°, a origem fica a 2·raio·sen 8°: raio de 10 km põe a origem a 2,8 km, abaixo de uma camada a 3 km, e **a sombra some sem aviso**. Aí precisa de `r.VolumetricCloud.ShadowMap.LightDistanceOverride` maior que a altura do topo ÷ sen 8° (topo a 12 km → ~86 km).
- **A máscara radial do estilo D não protege o chão da sombra:** com o sol a 8°, uma torre com topo a 12 km joga sombra até 85 km longe dela, pro lado oposto ao sol. As torres do lado do sol cobririam a batalha. Com a força na superfície em 0 isso não aparece. Se um dia subir, a máscara precisa de um corredor limpo na direção do sol.
- **Uma correção do estudo de 07/10:** o `docs/JOGO.md` §8 diz que "o que você vê de cima é a sombra das nuvens correndo pelo campo". Isso vale com o sol alto. Com a luz aprovada (8°), não.

### 8.5 Uma camada de nuvem só
- A Unreal desenha **um** `VolumetricCloud` por cena. Cirro a 9 km e cúmulo a 3 km na mesma camada = camada de 2 a 12 km e todo raio atravessa os 10 km (mais passos, mais custo).
- O jeito barato: o cirro numa cúpula com material de céu (`Is Sky`, entra na captura da SkyLight) e a camada volumétrica só pras nuvens com volume.

### 8.6 Mexer na poeira da atmosfera muda a luz do chão aprovada
- Com a SkyAtmosphere, o sol que chega no chão é a intensidade do sol × a transmitância da atmosfera (`AtmosphereLightIlluminanceOnGroundPostTransmittance`). Mais poeira no céu = menos sol e mais vermelho no chão.
- A conta a 8° (massa de ar 6,86, Kasten-Young 1989; Rayleigh e ozônio padrão da Unreal; poeira com Ångström baixo):

| AOD 550 nm | sol que chega (R / G / B) |
|---|---|
| 0,005 (o padrão da Unreal) | 0,66 / 0,38 / 0,15 |
| ~0,03–0,04 (a receita aprovada: Mie 0,05/km com cor (1; 0,8; 0,58), absorção 0,005/km com cor (0,35; 0,6; 1), altura 0,8 km) | 0,51 / 0,31 / 0,13 |
| 0,10 | 0,37 / 0,20 / 0,07 |
| 0,30 | 0,10 / 0,05 / 0,02 |
| 0,60 | 0,01 / 0,01 / 0,00 |

- **Ou seja:** subir a poeira do céu pra AOD 0,3 corta o sol no chão ~5× e a batalha escurece. Todo estilo que mexer no Mie tem de compensar a cor e a intensidade do sol pra manter a luz do chão igual à aprovada (cor nova = cor velha × transmitância velha ÷ nova, por canal; **est.**, conferir medindo a areia ao sol). Ou mexer só no que não muda a transmitância: a fase do Mie, o `MultiScatteringFactor`, o `SkyLuminanceFactor`, a névoa de altura.

### 8.7 O resto
- **Nuvem que anda e luz do céu estática:** a captura atual é tirada uma vez (§6.5). Ou a nuvem fica parada (o vento só mexe no detalhe), ou captura em tempo real.
- **Exposição:** a receita trava a exposição (EV 13,55, mín = máx). Bom pra nuvem: céu claro não escurece a batalha. Mas o céu perto do sol estoura; conferir o branco no topo (a lição da volta 36 do Look: >0,8 em no máx. ~9% da tela).
- **`bRayMarchVolumeShadow` falso sem `Cast Cloud Shadows` no sol:** a nuvem fica sem auto-sombra nenhuma e vira algodão chapado.
- **Oitavas de dispersão múltipla:** compiladas no material (0, 1 ou 2). Preset de qualidade diferente = instância de outro material.
- **Async compute:** os textos da Epic recomendam `PreferAsyncCompute` + `ApplyFogLate`. Na 1660 não existe (§6.2): tudo soma em série.
- **Sinal do g da fase:** positivo = brilho pra frente (§6.4), ao contrário do tooltip.
- **A foto do minimapa** (`RTSFogOfWar.cpp:224-238`) é uma captura de cena ortográfica tirada uma vez. Captura com nuvem custa caro quando é por quadro ([fórum da Epic](https://forums.unrealengine.com/t/render-target-cause-fps-loss-because-of-volumetric-clouds/475549)), e a sombra de nuvem no chão sairia "assada" no minimapa. Com a sombra no chão em 0 não aparece; por garantia, desligar a nuvem na captura (`ShowFlags` de nuvem).
- **`Cloud Ambient Occlusion` da SkyLight:** um usuário da 5.6 recuperou 33 qps desligando (mesmo tópico). Não usar sem medir.
- **A reprojeção do modo 0 deixa rastro em nuvem perto da câmera:** a cvar `r.VolumetricRenderTarget.MinimumDistanceKmToEnableReprojection` (o código sugere 4 km) troca o rastro por ruído. As nossas nuvens ficam a 15+ km.

---

## 9. Plano de prova (antes de mostrar)
1. **Captura de base**, sem nuvem: a câmera deitada nos cenários `look_inicial` e `look_zoom` girados pro sol e pro lado oposto, a 3° e 10°.
2. **As pegadinhas antes da nuvem:** 8.2 (corte da névoa) e 8.3 (escala da perspectiva) em A/B. A câmera normal tem de sair **idêntica** à aprovada (diferença pixel a pixel ~0).
3. **Os estilos da §7**, cada um com:
   - foto deitada pro sol e oposto;
   - um vídeo girando a câmera;
   - `ProfileGPU` nas duas câmeras;
   - CSV de 600 quadros (`docs/OTIMIZACAO.md`, "Como medir").
4. **A folha lado a lado com o alvo** (`day_tilt_cam.webp`) e a medida das cores do céu pelos mesmos 16 pontos da §2.
5. **A trava da placa** (`docs/GPU.md`) em todo render.
6. **Uma coisa de cada vez:** primeiro o céu destravado (B0), depois um estilo de cada vez, ele escolhe.

---

## 10. As fontes (o que cada uma ensina)

**Artigos e palestras:**
- [Hillaire 2020, EGSR](https://diglib.eg.org/handle/10.1111/cgf14050) ([texto](https://www.readkong.com/page/a-scalable-and-production-ready-sky-and-atmosphere-3211109), [código](https://github.com/sebh/UnrealEngineSkyAtmosphere)): a SkyAtmosphere da Unreal. Tabelas pequenas, horizonte com mais resolução, perspectiva aérea em grade, dispersão múltipla em tabela. 0,31 ms numa GTX 1080 a 720p.
- [Hillaire 2020, slides do curso SIGGRAPH](https://blog.selfshadow.com/publications/s2020-shading-course/hillaire/s2020_pbs_hillaire_slides.pdf): nuvem na Unreal. Dispersão múltipla obrigatória, fase de dois lobos, luz do chão, transmitância por passo.
- [Hillaire 2016, Frostbite](https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/s2016_pbs_frostbite_sky_clouds.pdf) ([SlideShare](https://www.slideshare.net/DICEStudio/physically-based-sky-atmosphere-and-cloud-rendering-in-frostbite)): fase 0,8/−0,5/0,5; integração que conserva energia; 0,72–1,09 ms no Xbox One.
- [Högfeldt, tese do Frostbite](http://www.cse.chalmers.se/~uffe/xjobb/RurikH%C3%B6gfeldt.pdf): tamanhos de ruído, meia resolução, mistura temporal de 5%.
- [Schneider 2015, Horizon Zero Dawn](https://www.advances.realtimerendering.com/s2015/The%20Real-time%20Volumetric%20Cloudscapes%20of%20Horizon%20-%20Zero%20Dawn%20-%20ARTR.pdf):
  - nuvem baixa em 1,5–4 km e alto/cirro em 2D;
  - cúmulo forçado no horizonte;
  - Perlin-Worley, perfil por tipo, powder;
  - ~2 ms no PS4 com 1/16 dos pixels por quadro.
- Schneider, Nubis [2017](https://www.guerrilla-games.com/read/nubis-authoring-real-time-volumetric-cloudscapes-with-the-decima-engine), [2022](https://www.guerrilla-games.com/read/nubis-evolved), [2023](https://www.guerrilla-games.com/read/nubis-cubed): forma com intenção (perfil, envelope, voxel); custos de segunda mão ([gcores](https://www.gcores.com/articles/192613)).
- [Bauer 2019, RDR2](https://advances.realtimerendering.com/s2019/index.htm): solução única pra nuvem, névoa e luz ambiente; 1/4 da resolução com TAA (pela descrição do Hillaire 2020).
- [Patry 2021, Ghost of Tsushima](https://advances.realtimerendering.com/s2021/jpatry_advances2021/index.html): g variando com a profundidade (−0,15 a 0,85); retroespalhamento ×2,16.
- [Toft et al. 2016](https://ar5iv.labs.arxiv.org/html/1609.05344): GTX 1080 a 1080p, 297,7 ms em resolução cheia contra 2,4 ms com 1/4 + TAA.
- [Wrenninge 2013, Oz](https://history.siggraph.org/?p=139835) e [Skybolt](https://prograda.com/2021/07/28/rendering-planetwide-volumetric-clouds-in-skybolt/): as oitavas de dispersão múltipla.
- [MSFS 2024, formato do tempo](https://docs.flightsimulator.com/msfs2024/retail/content-configuration/mission-xml-files/weather-xml-wpr-file-properties/index.md): camadas com base, topo, densidade e espalhamento.

**Física do céu e da poeira:**
- [Lange, Rozanov & von Savigny 2023](https://acp.copernicus.org/articles/23/14829/2023/acp-23-14829-2023.html): a fatia do ozônio no azul do zênite (15% com o sol a 10°, 66% no horizonte).
- [Bruneton 2017](https://ar5iv.labs.arxiv.org/html/1612.04336): os modelos de céu ficam escuros demais perto do sol (a fase).
- [Dubovik 2002](https://lweb.cfa.harvard.edu/hitran/HITRAN2012/Aerosols/papers/dubovik_et_al._(2002).pdf), [Kim 2011](https://acp.copernicus.org/articles/11/10733/2011/), [Guirado 2014](https://acp.copernicus.org/articles/14/11753/2014/acp-14-11753-2014.pdf), [Kokkalis 2018](https://mdpi-res.com/d_attachment/remotesensing/remotesensing-10-01807/article_deploy/remotesensing-10-01807.pdf), [ACP 2018 (SAL)](https://acp.copernicus.org/articles/18/17655/2018/acp-18-17655-2018.html): AOD, Ångström, g, albedo e altura da poeira no deserto.
- [Corfidi, NOAA SPC](https://www.spc.noaa.gov/publications/corfidi/sunset/): poeira lava as cores; o laranja forte mora em altocúmulo e cirro; ar limpo faz o pôr do sol.
- [OMM, Cloud Atlas](https://cloudatlas.wmo.int/en/some-useful-concepts-levels.html) e [altocúmulo](https://cloudatlas.wmo.int/en/altocumulus-ac.html): alturas por nível; tamanho aparente do altocúmulo.
- [Stein et al. 2011](https://www.met.reading.ac.uk/~swrhgnrj/publications/wafrica_monsoon_cloudsat.pdf): altocúmulo no topo da camada de poeira do Saara.
- Wikipedia: [Air mass](https://en.wikipedia.org/wiki/Air_mass_(astronomy)), [Golden hour](https://en.wikipedia.org/wiki/Golden_hour_(photography)), [Belt of Venus](https://en.wikipedia.org/wiki/Belt_of_Venus), [Cumulus](https://en.wikipedia.org/wiki/Cumulus_cloud), [LCL](https://en.wikipedia.org/wiki/Lifting_condensation_level), [Haboob](https://en.wikipedia.org/wiki/Haboob).
- [colour-science, céu CIE 2003](https://colour.readthedocs.io/en/latest/_modules/colour/phenomena/sky/cie2003.html): a distribuição do brilho pelo céu.
- [Gurney, Sky Blue](https://gurneyjourney.blogspot.com/2008/04/sky-blue.html): os dois degradês e o azul longe do sol.

**Unreal (documentação e fórum):**
- Componentes: [Volumetric Cloud](https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-cloud-component-in-unreal-engine), [as propriedades](https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-cloud-component-properties-in-unreal-engine), [Sky Atmosphere](https://dev.epicgames.com/documentation/en-us/unreal-engine/sky-atmosphere-component-in-unreal-engine).
- Luz e névoa: [Sky Lights](https://dev.epicgames.com/documentation/en-us/unreal-engine/sky-lights-in-unreal-engine), [Environmental Light](https://dev.epicgames.com/documentation/en-us/unreal-engine/environmental-light-with-fog-clouds-sky-and-atmosphere-in-unreal-engine), [Light Functions](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-light-functions-in-unreal-engine).
- [Fórum: Hillaire sobre os modos do render target](https://forums.unrealengine.com/t/volumetriccloud/688516) (o modo 2 traça 1 de cada 4×4 e vaza sobre malhas; o modo 0 para numa profundidade).
- **O fonte da 5.8 instalado** (a base da §6 e da §8):
  - `Renderer/Private/VolumetricCloudRendering.cpp`, `VolumetricRenderTarget.cpp`, `SkyAtmosphereRendering.cpp`, `DeferredShadingRenderer.cpp`, `ReflectionEnvironmentRealTimeCapture.cpp`, `SceneCore.cpp`;
  - `Engine/Private/Components/VolumetricCloudComponent.cpp`, `DirectionalLightComponent.cpp`, `SkyLightComponent.cpp`, `SkyAtmosphereComponent.cpp`, `ExponentialHeightFogComponent.cpp`;
  - `Engine/Classes/Materials/MaterialExpressionVolumetricAdvancedMaterialOutput.h`;
  - `Shaders/Private/VolumetricCloud.usf`, `SkyAtmosphere.usf`, `HeightFogCommon.ush`, `ParticipatingMediaCommon.ush`;
  - `Config/BaseScalability.ini`;
  - `Content/EngineSky/VolumetricClouds/`.

**RTS:** as fontes estão na tabela da §4.

**Do projeto:**
- `Game/Tools/look_recipes/desert_golden_dust.json` (a receita aprovada);
- `Game/Plugins/RTSLook/Source/RTSLook/Private/LookDirector.cpp`;
- `Game/Source/RTSGame/Public/RTSCameraPawn.h`;
- `Game/Shaders/Private/FogOfWar.usf`, `Game/Source/RTSShaders/Private/RTSFogPass.cpp`, `Game/Source/RTSGame/Private/RTSFogOfWar.cpp`;
- `docs/RECEITAS_DO_LOOK.md` §8, `docs/OTIMIZACAO.md`, `docs/concept/scenes/desert_golden_hour/best/day_tilt_cam.webp`.

**O que não deu pra abrir** (limite de tamanho da ferramenta ou de buscas da sessão):
- os PDFs grandes do Frostbite 2016, Nubis 2017/2022/2023 e EGSR 2020, e o PPTX do RDR2;
- o artigo Intel/Ubisoft do Anno 1800;
- Minnaert e Les Cowley;
- os RTS sem resultado (Homeworld 3, Planetary Annihilation, C&C Remastered, Stormgate, BAR).

---

## 11. O que a 2ª opinião mudou e o que já está no código (16/10, o Look)

**A 2ª opinião (crítica do plano da §7)** trocou três decisões:
1. **Nada de corte duro da névoa** (`FogCutoffDistance` é um `if` sem transição: risca escura logo abaixo do horizonte e costura no céu). No lugar, a névoa de altura fica **mais rasa** com a câmera deitada (a queda × 4, A/B × 2,5): o céu baixo e as nuvens saem dela, o chão longe continua enevoado, sem degrau.
2. **A perspectiva aérea deitada vai a ×1,5** (A/B ×1), não ×4: com ×4 a base de uma torre a 40 km ainda passa só 15/7/0,6% (R/G/B).
3. **A luz do céu só é recapturada com a câmera normal** (a captura é um render da cena: deitada, gravaria a névoa rasa e as nuvens na luz da partida inteira).

E mais: tudo pela **elevação do topo da tela** (o campo de visão abre ao deitar), não pela inclinação; as nuvens ligam com o topo a −9° (o zoom perto fica a −13°) e desligam 2° abaixo; o céu e a névoa ficam Movable (os setters ignoram sem aviso); a nuvem não traça antes de 15 km nem começa além de 100 km; **um corredor sem torre de ±18° na direção do sol** (torre ali tapa o disco, sombreia o ar da batalha e estoura o clarão); extinção ≤ 0,02/m (passos de ~500 m: 0,05/m dá nuvem binária); sem bigorna (lê como o cogumelo da nuke); sombra no ar começando em 0; `bUsedWithVolumetricCloud` no material (sem ela não desenha).

**No código** (compilado pela sessão do jogo em 16/10, 00:25):
- `LookRecipe.h`: `FLookSky` (aérea e névoa pela elevação do topo da tela), `FLookClouds` (camada, traçado, sombra, material e parâmetros), e no preset `bClouds`, `CloudSampleScale`, `CloudMaterial`.
- `LookDirector.cpp`: `UpdateTilt` (no Tick, pelo `PlayerCameraManager`), `ApplyClouds` (pelo `ApplyPreset`), `UpdateCloudSun` (o corredor), o `RecaptureSky` adiado.
- `Game/Tools/look_clouds.py`: `/RTSLook/Clouds/M_RTSCloud_Hi` (2 oitavas, sombra marchada, luz do chão) e `_Med` (1 oitava, sombra pelo mapa). O modelo do Horizon: cobertura em km com a máscara radial e o corredor, o perfil laje/torre, Perlin-Worley da Unreal e a erosão só na casca.
- As receitas de teste (`Game/Tools/look_recipes/`): `sky_b0` (o céu destravado), `sky_b0_air1`, `sky_b0_fog25`, `sky_epic` (o encanamento com o material da Epic), `sky_d_towers`, `sky_c_altocumulus`. Sem os blocos `sky` e `clouds`, nada muda (as aprovadas não têm).

**As provas** rodam nas cenas `ceu_deitado_1..5` (a sessão do jogo), contra as fotos de antes (`captures/game/ceu_antes_*`).

### 11.1 O que a prova no jogo mostrou (16/10, madrugada)

**O céu destravado** (as cenas `ceu_deitado_1..5`, alta, contra `captures/game/ceu_antes_*`):
- **A câmera normal ficou idêntica** à aprovada: a diferença pixel a pixel está só nas folhas das palmeiras (vento) e num ruído uniforme do TSR. Custo: 0 (31,6 ms contra 32,0).
- **Deitada:** o véu de poeira chapado deu lugar ao céu com degradê (azul acinzentado no alto, pêssego no horizonte) e aos paredões de pedra atrás da vila. A vista a 20° (o horizonte ainda fora) não muda: sem pulo.
- **A névoa 2,5× mais rasa ganhou da 4×:** os paredões ficam esfumaçados ao longe (como as dunas do alvo) e a faixa clara das palmeiras do fundo se integra. A aérea ×1 quase não difere da ×1,5. Base escolhida: `sky_b1` (névoa 2,5×, aérea ×1,5).

**As nuvens** (deitada, vista 2; o quadro sem nuvem 31,6 ms):
| | mediana | custo |
|---|---|---|
| a Epic como vem (`m_SimpleVolumetricCloud`, 5–15 km) | 39,2 ms | +7,6 ms |
| a Epic sem o mapa de sombra das nuvens | 36,3 ms | +4,7 ms (o mapa: **~2,9 ms**, com as forças de sombra em 0!) |
| + amostras da vista ≤ 48 | 34,9 ms | +3,3 ms |
| a câmera normal com nuvem na receita | 31,1 ms | 0 (desligada pela elevação do topo da tela) |
- O mapa de sombra agora só existe quando alguma força de sombra passa de 0 ou o preset pede (`bCloudShadowMap`, compilado 16/10 02:05).
- **A Epic, como vem, dá um céu azul com cúmulos brancos bonito** (vista 3). Ela vira a base dos estilos (`sky_e1_cumulus`, `sky_e2_poderosas`, `sky_e3_esparsas`).
- **O nosso `M_RTSCloud` falhou:** as torres (D) viraram um chuvisco branco de estática no céu e vazam por cima da borda do paredão; o altocúmulo (C) nem aparece, custando ~2,7 ms. Causa provável: filamentos verticais finos (o perfil de torre afinando com a cobertura baixa) bem menores que o passo de ~500 m da marcha, e a densidade de 0,012–0,015/m baixa demais pra ler. Fica de lado até ter tempo de depurar no editor; não usar.

### 11.2 A sombra das nuvens e o sol na tela (16/10, manhã)

**A sombra das nuvens "não aparecia" (o Felipe: "n vi nada"): era o tamanho, não a ligação.**
- **O caminho da Unreal confere:**
  - o termo de nuvem da `M_SmokeShadowLF` (Fluxo, `BlastLab/Tools/build_scene.py`) compila sem erro;
  - o BlastFx põe a função de luz no sol (`DirectionalLight_0`);
  - ela usa a posição no mundo, então não vai pro atlas de 128² das funções de luz (`HLSLMaterialTranslator.cpp:1769`) e passa pelo passe de tela normal (`LightRendering.cpp:2375`).
- **A medida:**
  - a 0,55 com manchas de 220 m, só 0,6% da tela escureceu mais de 10%;
  - na força 1 (a chave do Fluxo), a razão com/sem ficou em 0,93–1,16, ou seja, só a diferença da cena, sem mancha nenhuma.
- **A causa:** a tela do jogo tem ~95 m de largura (100,4 m, 50° de abertura). Com a mancha de 150–220 m a tela inteira fica na luz ou na sombra, e no vídeo de 13 s a borda nunca passa.
- **O ZH, no código liberado pela EA** (`GeneralsMD/.../TerrainTex.cpp` e `W3DShaderManager.cpp`):
  - a textura `TSCloudMed.tga` cobre 31,5 células de 10 unidades (`STRETCH_FACTOR`), ou seja 315 unidades = **83 m** (1 unidade = 0,2633 m, a conversão do projeto, `docs/JOGO.md`);
  - ela anda 0,02 e 0,03 da textura por segundo = **3 m/s**;
  - ela multiplica só o terreno e as estradas, não as unidades (limite do hardware de 2003; a nossa, na luz do sol, pega tudo, o que é o certo).
- **A escolha, medida no próprio ruído da função** (1 km simulado, janelas de 95×120 m):

| mancha | cobertura | área na sombra | telas com borda |
|---|---|---|---|
| 40 m | 0,6 / 0,12 | 29% | 92% |
| **60 m** | **0,6 / 0,12** | **30%** | **78%** |
| 90 m | 0,6 / 0,12 | 32% | 58% |
| 220 m (a de antes) | 0,5 / 0,18 | ~40% | < 40% |

- **As receitas de teste:**
  - `sky_e4_sombra3`: 60 m, o tamanho do ZH;
  - `sky_e4_sombra4`: 40 m.
  - As duas usam força 0,55, os 30% de cúmulos de tempo bom e vento de 5 m/s (o ZH 3 m/s; a nuvem real 5–15 m/s).
  - Cenário `sombra_nuvem`: a vila do `look_inicial` parada por 15 s, gravando.

**O sol na tela (`ceu_sol`, deitada olhando pro sol a 8°).**

| receita | estourado (canal > 0,97) olhando pro sol | de costas: p50 / p95 |
|---|---|---|
| E4 (sem ajuste) | 65% | 0,76 / 0,98 |
| `sky_e4_sol` (−1 EV, lago ×0,4) | 31% | 0,53 / 0,93 |
| `sky_e4_sol2` (−1,6 EV, lago ×0,3) | 15% | 0,44 / 0,88 |

- O alvo de conforto é estourado < 2,5% e p95 0,65–0,75.
- **Nenhuma resolve, e o disco do sol não aparece em nenhuma** (no lugar dele fica um clarão chapado atrás de um véu bege).
- **O defeito é o céu muito mais claro que o chão:** baixar a exposição inteira escurece o chão de costas pro sol antes de salvar o céu.
- **O próximo A/B:** a exposição local nas luzes altas com a câmera deitada (`TiltLocalExposureHighlight`, compilado 16/10). Ela comprime a área grande e clara do céu sem mexer no meio-tom do chão.
  - `sky_e4_sol3`: contraste 0,6 e −0,8 EV;
  - `sky_e4_sol4`: contraste 0,4 e −0,8 EV.
- **O disco:** dois testes isolam quem o tapa, um sem a nuvem (`r.VolumetricCloud 0`) e um sem a névoa (`r.Fog 0`).

### 11.3 O céu contra o gameplay_v3 e o "uma luz só" (16/10, tarde e noite)

- **O pedido** (o Felipe, com a folha do laboratório): "nosso céu tá podre", "falta comparação com game shots".
- **Medido no gameplay_v3:**
  - céu p50 0,59, saturação 0,38, estourado 3%;
  - chão p50 0,29.
- **O de hoje na mesma pose (cena `alvo_v3_ceu`):**
  - céu 0,97 e 76% estourado;
  - chão 0,84 e 57% estourado.
- **8 rodadas** (receitas `alvo_a` … `alvo_ab`, trocadas na mesma partida pelo `-RTSLookCycle`), chegando à **Y**:
  - nuvem a partir de 1 km da câmera, base a 3 km, cobertura −0,05 (no material da Epic, perto de 0 já é céu fechado);
  - a sombra da nuvem nela e no ar (`shadow_on_atmosphere` 1 + o mapa de sombra das nuvens), que dá a base escura e a borda clara;
  - o sol no 8° de hoje;
  - na câmera deitada: −1,4 EV, luzes 0,85, saturação ×0,85, balanço de branco 6500 K, bloom 1,0.
- **O custo da Y:**
  - +3 ms deitada;
  - na câmera do RTS, idêntica (0,66/255) e com o mesmo custo.
- **A Y foi salva como PISO** ("pelo menos o jeito que tá tá bom").
- **O defeito, medido contra os vídeos do Gemini:**

  | Areia | De cima | Deitada |
  |---|---|---|
  | Gemini | 0,48 | 0,68 (mais clara e mais dourada) |
  | Gemini, vídeo da estrada | 0,23–0,25 o vídeo inteiro | igual, com o céu 0% estourado |
  | Nosso | 0,67 | 0,47 (matiz 28 → 22, saturação 0,47 → 0,60) |

  - **A causa:** a curva fílmica trabalha por canal, e o −1,4 EV joga o verde e o azul no pé da curva. A saturação, o branco e o bloom do tilt eram remendos desse remendo.
- **O plano "uma luz só"** (o estudo da tarde; o modelo do Ghost of Tsushima, Patry, SIGGRAPH 2021): o mundo não sabe onde está a câmera, e a câmera é uma só, sem nada do tilt.
  - **Exposição:** a de hoje, pela luz que chega.
  - **O céu:** segurado pela exposição local nas luzes altas com limiar (+2 EV, contraste 0,4), sempre ligada, como o filtro graduado do fotógrafo; a areia (≤ +1,5 EV) não é tocada.
  - **A perspectiva aérea:** ×1 sempre (a Unreal limita o controle a 0–3; o ×40 era remendo do RTS).
  - **A névoa:** uma só, com teto de opacidade (`FogMaxOpacity` ~0,35) e a volumétrica a 80 m (hoje 250: a poeira acesa de frente pro sol era a suspeita nº 1 do estouro).
  - **As nuvens:** parte do mundo, com a luz do céu estática capturada uma vez. Conferir se o WorldForge liga a captura em tempo real (`build_slice.py:2367`); se ligar, a luz ambiente muda quando a câmera deita.
- **A ordem:**
  1. **Passo 0:** medir quem causa o estouro (névoa, volumétrica, aérea, contraste), um de cada vez.
  2. **Passo 1:** a receita única, uma mudança por vez, A/B de cima contra a aprovada e deitada contra os vídeos.
  3. **Passo 2:** as dívidas do mundo (sol branco + poeira física, luz do céu ×1).
- **Critérios de aceite:**
  - areia de cima × deitada: brilho ±0,04, matiz ±2°, saturação ±0,05;
  - olhando pro sol: estourado ≤ 3%;
  - no vídeo da descida, o brilho da areia reto;
  - de cima, só o terço de cima pode mudar, dentro das faixas da aprovada.

### 11.4 O que a madrugada de 16/10 mediu (o "uma luz só", cena `uma_luz`: de cima e deitada em cada receita)

- **A luz ambiente:** a luz do céu do `World_TestSalt2` está em captura em tempo real (`real_time_capture=True`, posta pelo WorldForge).
  - A nuvem passou a ficar fora da captura (`bInSkyCapture`, padrão false no plugin).
  - O brilho do céu (`SkyLuminanceFactor`) e a contribuição na névoa (`HeightFogContribution`) entraram como campos da atmosfera.
- **O passo 0, deitada olhando pro sol** (a aprovada + nuvem, nada do tilt):

  | Variante | Estourado |
  |---|---|
  | Base | 32% |
  | Volumétrica a 80 m | 1% (a poeira acesa de frente pro sol a 250 m era o estouro) |
  | Contraste 1,0 | 0% |
  | Sem a névoa de altura | 0%, mas o céu marrom-escuro |
  | Aérea ×1 | 66% |

- **Sem nada do tilt** (a luz_1), a areia é uma só:

  | | De cima | Deitada |
  |---|---|---|
  | Areia | 0,68 / matiz 28 / sat 0,45 (= a aprovada) | 0,81 / 32 / 0,34 |
  | Céu | — | 1% estourado, mas um véu bege sem nuvem |

- **O que apaga a nuvem e o horizonte:** a perspectiva aérea ×40 da aprovada (a nuvem a 3–7 km vira 120–280 km de ar). Com ×1 a nuvem aparece, mas o céu fica claro demais (0,86–0,90, 27–60% estourado; o alvo é 0,58–0,66).
- **A ARMADILHA DE MEDIDA:** na troca de receitas a vista de cima escurecia a cada vez que a câmera deitava (0,68 → 0,57), seja qual fosse a receita.
  - Os controles: só de cima fica 0,68 nas 10 fotos; sem nuvem, 0,68 nas 5 voltas; com a nuvem mas sem a sombra dela no ar, 0,68.
  - **A causa é a sombra da nuvem no ar** (`shadow_on_atmosphere` + o mapa de sombra das nuvens). A nuvem some quando a câmera sobe e o mapa velho continua escurecendo o ar sobre o chão, ampliado pelo ×40.
  - É também um defeito de jogo: o jogador que deita e volta veria o mundo mais escuro.
  - As conclusões de "a vista de cima mudou" nas rodadas 1–3 (luz_2 a luz_15) foram contaminadas por isso e foram refeitas sem a sombra no ar (luz_a..e).

### 11.5 Onde a noite de 16/10 parou (13 rodadas do "uma luz só")

- **O que funcionou e fica:**
  - nada de ajuste só da câmera deitada;
  - a aérea ×1;
  - a volumétrica a 250 m com espalhamento neutro (g 0);
  - a névoa 0,05;
  - a nuvem sempre ligada, com a sombra no ar e fora da captura;
  - a luz do céu ×1,6 (6,19 → 9,9), porque a nuvem sempre lá escurece o céu que ilumina o chão.
- **O resultado:** a vista de cima fica na aprovada (areia 0,66–0,68/27/0,40–0,41, terço de cima 0,68–0,70 contra 0,72) e a areia é a mesma nas duas vistas.
- **A nuvem:**
  - na 3–7 km, quebrada (X1), sai pálida e com 13% estourado;
  - na 1,8 km, cobertura 0 e densa (Y3/Y4), dá 2% estourado, mas o céu vira um teto uniforme e sem drama.
- **O que não se consegue com os escalares do material da Epic:** as massas escuras com borda dourada e os buracos de céu azul-acinzentado do alvo.
- **O rumo a decidir com o Felipe:**
  - (a) fazer a nossa nuvem, a forma e o mapa de cobertura, pra essa faixa baixa do céu;
  - (b) céu fotográfico de hora dourada (HDRI CC0) pra mapa de sol parado. A luz do céu sai da mesma foto, e é o caminho comum nos AAA de hora fixa.
- **As receitas:** `luz_w3`, `luz_x1`, `luz_y3` em `Game/Tools/look_recipes/`; a cena `uma_luz`.

### 11.6 O céu deitado claro e bege (17/10, madrugada)

O Felipe, sobre a X1: "o céu tá muito claro, só deixa um céu mais azul normal". E antes: "nosso mapa é golden hour com névoa".

- **O que não move o céu:**
  - o teto da névoa (`FogMaxOpacity` 10–30%): sem efeito visível, porque o véu do céu é a névoa volumétrica (250 m), que ignora o teto;
  - o brilho do céu azulado (`SkyLuminanceFactor`) e a névoa menos tingida pelo céu (`HeightFogContribution`): mudam a vista de cima pela captura em tempo real da luz do céu (a areia fica mais vermelha).
- **A camada de poeira rasa e densa, com a MESMA coluna** (densidade × k, queda × k):
  - de cima a câmera atravessa a coluna inteira e o véu aprovado fica;
  - deitada, o olho a ~4 m sai da camada logo e o céu limpa.

  | Variante | De cima: areia | Terço de cima | Deitada: céu p50 (p5–p95), estourado |
  |---|---|---|---|
  | X1 | 0,65/26/0,41 | 0,69 | 0,80 (0,59–0,96), 13% |
  | R6 (k 6) | 0,65/26/0,41 | 0,69 | 0,70 (0,43–0,97), 14% |
  | R6M (R6 + Mie 0,012) | 0,67/27/0,42 | 0,73 | 0,67 (0,34–0,98), 16% |
  | R10 (k 10) | 0,64/26/0,41 | 0,67 | 0,65 (0,27–0,98), 17% |
  | Gemini (road, r1) | — | — | 0,61–0,66 (0,34–0,86), 0–2% |

  O brilho e o contraste do céu chegam no alvo. O véu "fica lá" no horizonte, como o Felipe descreveu no game shot.
- **O que falta é a cor.** O topo do céu (as 120 linhas de cima, mais nuvem que céu) mede:

  | | RGB | Pixels azulados |
  |---|---|---|
  | Gemini | [158,158,155] a [192,183,174] | 15–38% |
  | Nosso | [208,180,157] | 2% |

  O bege vem de três fontes:
  - o ganho quente do sol na nuvem (`scattered_luminance_scale` 1,6/1,4/1,2, de uma volta antiga);
  - a poeira do alto colorida (a cor do Mie 1/0,8/0,58);
  - o Mie 3× acima do padrão.

  As rodadas C e D (`rasa_c*`, `rasa_d*`) testam:
  - a nuvem na cor física;
  - o Mie neutro e no padrão;
  - a base da nuvem escura (oclusão da luz do céu 0,85);
  - menos cobertura de nuvem.
- **O que as rodadas C e D mediram:**

  | Variante | De cima: areia | Terço de cima | Deitada: céu p50, estourado | Topo do céu (RGB, pixels azulados) |
  |---|---|---|---|---|
  | R6M | 0,66/27/0,41 | 0,73 | 0,73, 14% | [202,173,152], 1% |
  | C1: nuvem na cor física | 0,67/26/0,41 | 0,72 | 0,69, 10% | [183,160,145], 2% |
  | C3: + Mie neutro 0,004 | 0,68/27/0,42 | 0,73 | 0,67, 11% | [189,167,152], 3% |
  | D1: base escura 0,85 | 0,67/27/0,42 | 0,73 | 0,70, 10% | [183,161,146], 1% |
  | D4: D1 + menos nuvem | 0,68/27/0,40 | 0,74 | 0,76, 15% | [199,188,177], 5% |

  - A vista de cima fica na aprovada em todas.
  - O ganho quente da nuvem era o maior pedaço do bege.
  - A base escura não muda nada.
  - Menos nuvem dá o "céu azul normal" a olho, mas fica mais claro.
  - O mesmo controle (C3) mede o céu 0,67 numa rodada e 0,71 na outra: a nuvem anda com o tempo. Diferença de céu abaixo de ~0,04 é ruído.
- **O clarão em volta do sol** (a faixa do meio, 10–20° de altura) é o que estoura:

  | | Clarão (centro) | Areia deitada |
  |---|---|---|
  | Nosso | 0,93 [248,234,220] | 0,83, desbotada pelo brilho (o "piso liso") |
  | Gemini | 0,67 | 0,68 [215,168,116] |

  No Gemini, a nuvem grossa na frente do sol fica escura e só a borda brilha. A nossa é fina e acende pra frente: a fase do material é g 0,8.
- **O sol do mapa a 8° chega na areia como (72 323, 31 702, 8 516) lux:** a transmitância de 8° vezes o 4 700 K. A câmera (5 600 K e os ganhos) é que devolve a areia dourada.
- **Os padrões do material de nuvem da Epic** (`m_SimpleVolumetricCloud_Inst`, lidos no editor):
  - cobertura −0,2;
  - densidade 0,008;
  - albedo 0,98;
  - `Phase_Controls` (0,8; 0,167; 0,575);
  - `Multiscatter_Controls` (0,667; 0,25; 0,18).

  Contra os padrões, a nossa nuvem é 6× mais densa e cinza (albedo 0,55). E `MultiScatteringContribution` **não existe** no material: a chave da receita não fazia nada.
- **A rodada E:** o céu pintado pelo `SkyLuminanceFactor`, com a cor e a força da luz do céu divididas pelo mesmo fator. A captura da luz do céu entrega ao chão a mesma luz ambiente; só os pixels de céu mudam.
- **O que a rodada E mostrou:** o céu pintado (E1, E2) quase não muda a medida, porque a tela deitada é quase toda nuvem e a nuvem não recebe o fator. Com menos nuvem (E3), o céu limpo sai quase branco (0,86, 29% estourado). A nuvem no padrão da Epic (E4) muda a vista de cima (0,73/29/0,37): fora.
- **O diagnóstico da areia "piso liso"** (17/10, a C3 com `ShowFlag.Specular 0`, com e sem a névoa de altura):

  | | De cima | Deitada: areia | Céu p50 | Centro do céu | Estourado |
  |---|---|---|---|---|---|
  | C3 normal | 0,67/27/0,41 | 0,83/32/0,31 [242,206,166] | 0,71 | 0,93 | 10% |
  | C3 sem especular | 0,63/26/0,46 | 0,61/25/0,50 [205,146,103] | 0,70 | 0,93 | 10% |
  | C3 sem especular e sem névoa | 0,63/26/0,47 | 0,61/26/0,50 | 0,55 | 0,94 | 10% |
  | Gemini r1 | — | 0,68/32/0,46 [215,168,116] | 0,66 | 0,67 | 2% |

  - **O "piso liso" é todo do especular:** o reflexo rasante do sol baixo na areia (+0,22 de brilho, −0,19 de saturação). Sem ele, a areia deitada é dourada como no Gemini. A névoa não mexe na areia.
  - **A névoa é o véu do céu** (0,55 → 0,70). A camada 10× rasa (C4) deixa o céu em 0,62, a medida do alvo.
  - **O clarão do centro e o estouro são as nuvens finas acesas pelo sol**, com ou sem névoa.
- **A rodada F (no Salt2 remontado às 04:07, com a areia áspera 0,95 do code_v3):**

  | Variante | De cima: areia | Terço de cima | Deitada: areia | Céu p50, estourado |
  |---|---|---|---|---|
  | C4 (camada 10× rasa) | 0,66/27/0,42 | 0,69 | 0,76/30/0,38 | 0,59, 10% |
  | F2: luz do sol na nuvem 0,5 | 0,66/27/0,42 | 0,69 | 0,76 | 0,59, 4% |
  | F3: fase da nuvem g 0,8 → 0,6 | 0,66/27/0,42 | 0,69 | 0,76 | 0,60, 4% |

  - A fase menor é a saída mais física pro clarão: o espalhamento múltiplo dentro da nuvem baixa a anisotropia efetiva (as oitavas do Wrenninge). A F2 só tira energia.
  - Na 1ª tentativa a nuvem começou sem aparecer: o material ainda compilando depois da troca de binários. Daí pra frente, toda rodada leva uma receita de aquecimento antes.
  - O 1º teste no mapa recém-montado caiu no renderizador (a 1ª abertura monta os campos de distância das lajes). A 2ª abertura rodou limpa.
- **A areia "piso liso":**
  - Só a aspereza 0,95 tira metade do brilho: 0,83 → 0,76 no mapa, e 0,74 com `r.Roughness.Min 0.95`; o alvo é o Gemini, 0,68.
  - O resto vem do Specular da areia ≈ 0,12. A mistura medida entre o normal e o sem-especular dá a fração; a F_Schlick dá o valor.
  - Feito do lado do Look: o 4º dado do MF (`look_mf.py`, chave `SPEC_READY`, saída `Sout`, entrada `SandSpec`) e o `World.ground_sand_specular` da receita, que põe o 'SandSpecular' no Landscape em jogo.
  - Pedido ao WorldForge em 17/10 04:3x.
- **A rodada G (o azul):**

  | Variante | Deitada: céu p50, estourado | Topo do céu | Pixels azulados | De cima |
  |---|---|---|---|---|
  | G1: F3 + menos nuvem (−0,12) | 0,64, 6% | [175,168,161] | 18% | 0,67/27/0,42, terço 0,71 |
  | Gemini r1 | 0,66, 2% | [192,183,174] | 15% | — |

  - A G1 é a candidata (`rasa_g1`).
  - A nuvem anda com o vento: no ciclo, cada receita pega um céu diferente (a G2 e a G3 caíram num céu fechado). Comparação fina de nuvem precisa de rodadas separadas ou do vídeo.
  - As variantes do especular da areia sobre a G1: `uma_g1_s50/s20/s12/s06`.
- **A areia "piso liso", resolvida no jogo** (`uma_g1_s*`, World_TestSalt2 remontado com o SandSpecular às 04:49):

  | Specular da areia | Deitada | De cima |
  |---|---|---|
  | 0,5 | 0,76/31/0,37 | 0,67/27/0,42 |
  | 0,2 | 0,73 | — |
  | 0,12 | 0,69/28/0,44 | 0,65/27/0,45 |
  | 0,06 | 0,67/28/0,46 | — |
  | Alvo | Gemini 0,68/32/0,46 | aprovada 0,68/28/0,45 |

  - **Fica 0,12**, sem compensar a exposição. Com +0,15 EV, a vista de cima volta a 0,68, mas a curva do filme tira a saturação: 0,42, e a deitada sobe pra 0,73.
  - **O "quadro mais liso de perto"** (o alarme do WorldForge e do Fluxo, vila_rua_perto) não é regressão. Na mesma cena e no mesmo mapa a areia mede 5,6 às 02:28 e 5,4–5,5 depois; a rodada das 03:05 (9,1) foi a que fugiu do padrão. O especular (0,5 ou 0,12) e a aspereza (`r.Roughness.Max 0.7`) não mudam a areia de perto.
- **A faixa branca do horizonte, olhando pro sol:**

  | | Faixa do horizonte |
  |---|---|
  | Nossa | 0,81, sat 0,26 |
  | Gemini | 0,60–0,63, sat 0,43 |

  - **O que não é:** a névoa de longe. A cor dela (a contribuição do céu, `Fog.SkyAmbientScale`, campo novo do Look) quase não muda a faixa. Nem a espessura da camada: a 6× até clareia, 0,86.
  - **O que é:** a volumétrica do foco (51 m) até 250 m, na altura do olho, dentro da camada rasa e densa, saturada e acesa de frente pelo sol.
  - **A volumétrica até 100 m (`uma_i1`):** a faixa vai a 0,76 e a vista de cima não muda (no RTS a volumétrica começa no foco, ~165 m). O "muro branco" na descida da câmera (a partida aos 5 s, terço de cima 0,76) some: 0,48.
  - **O resto da diferença pro Gemini:** lá o sol está atrás da nuvem e a poeira fica na sombra; aqui o sol aparece.
- **A candidata:** `uma_i1`, que soma:
  - a camada de poeira 10× rasa;
  - a fase da nuvem g 0,6;
  - a cobertura de nuvem −0,12;
  - o Specular da areia 0,12;
  - a volumétrica até 100 m.

  Vídeos em `captures/game/v4i_*`.
