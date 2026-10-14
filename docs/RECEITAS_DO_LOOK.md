# Receitas do Look: como cada coisa aprovada foi feita

O pedido do Felipe (13/10/2026): "não esqueça de anotar cada coisa que deu certo, a forma, como chegou lá; eu não sei
como foi, as outras sessões não sabem". Este é o livro de receitas da sessão do Look (gráficos).

**A regra deste documento:** toda coisa aprovada ou que "ficou boa" entra aqui com quatro partes:
- o que é;
- como chegou lá (o caminho, inclusive o que falhou);
- os números e os arquivos;
- como reproduzir ou levar pra outro lugar.

O histórico dia a dia fica em `docs/GRAFICOS.md`; aqui fica só o que funciona.

---

## 0. O método que deu certo (vale pra tudo)

> **A regra do Felipe (13/10 à noite): "bonito = jogo bonito otimizado".**
> - A maquete é laboratório. Uma receita só está pronta quando roda no jogo (cenário rápido, câmera do jogo e zoom perto), ao lado da folha aprovada e medida.
> - Todo visual nasce com o plano do jogo junto: o dado em tempo real (caderno, textura, MF), a resolução, a distância da câmera e o custo.
> - Exemplo do que não pode repetir: a trilha da areia "C" (12/10) foi aprovada na maquete e chegou no jogo como risco chapado (os cadernos de 9,4 cm, a câmera a 100 m, a ondulação desligada na distância).


Ele aprovou o resultado da cratera assim: "viu só como conseguimos com estudos? é esse tipo de resultado e investimento
que quero pra tudo". O caminho:

1. **O estudo com fontes, antes do código:**
   - jogos lançados (palestras GDC/SIGGRAPH, blogs, docs de modding);
   - a doc da Unreal e o código da engine instalada;
   - a física real (papers, manuais técnicos).
   - Um agente pesquisa e cita a fonte de cada coisa.
2. **A segunda opinião** (um agente cético) antes de fechar o plano. Na cratera, foi ela que achou o problema de verdade: nitidez e valor, não falta de textura.
3. **As referências certas, por caso:**
   - a sessão de arte recorta as imagens aprovadas por tema;
   - o que falta, o Felipe gera (Gemini/GPT), com o sol e a escala do jogo no prompt.
4. **A prova na maquete** (LookLab), com a mesma forma que o jogo usa.
5. **A comparação rígida NA MESMA ESCALA:**
   - o tanque de 7,9 m como régua em todas as imagens;
   - recortes de 2,5 m por região, lado a lado;
   - brilho, contraste e grão medidos.
   - A folha do quadro inteiro engana: o Felipe cobrou "compare direito as texturas".
6. **Iterar volta a volta:**
   - uma mudança medida por vez, olhando ampliado;
   - a crítica dura da sessão de arte em rodadas curtas (imagem primeiro).

**O que não fazer:**
- julgar pela média da cor (ΔE) e perder o conjunto;
- dizer "sumiu" sem ampliar;
- calibrar na luz da bancada e mandar pro jogo com outra luz (ver §5).

---

## 1. A areia do deserto v2 (aprovada em 13/10: "vale ir com a C mais marcada")

> **Estado em 14/10: a ondulação desta areia nunca aparece no jogo.**
> - No `WorldForge/Tools/build_slice.py` (o kT das ondulações), `1 - smoothstep(0.03, 0.06, fw)` desliga a onda quando o pixel cobre mais de 6 cm de chão.
> - Na câmera do jogo, o pixel cobre de 7 a 12 cm (fw ~9–14 cm), no zoom normal (100 m, 50°) e no mais perto (67 m, 58°), em qualquer preset. Na folha aprovada, a câmera estava a 45 m com 21° de campo: ~1 cm por pixel.
> - A onda de 22 cm não cabe: daria 2 pixels por onda e tremeria.
> - No jogo sobram o grão e as zonas: o "chão liso" que o Felipe vê.
> - **A lição:** o game shot desenha o detalhe maior que o real, pra ler de cima; nós fizemos no tamanho físico, que some a 100 m.
> - **A medida do game shot (14/10):** na r1_raid_slabs, o casco do tanque (7,9 m) ocupa ~180 de 1024 px, então o quadro tem ~45 m de largura. O nosso zoom normal mostra ~94 m e o mais perto ~74 m: o game shot está ~2× mais perto. O tanque ocupa ~1/5 da largura lá e ~1/10 no nosso.
> - **O zoom fica.** O Felipe: "nosso RTS precisa ter um zoom bom pra entender o jogo, precisa trabalhar em cima disso". Quem muda é o detalhe, desenhado pro zoom do jogo: nunca desligar, trocar de escala com a distância (sulco largo, ondulação maior nas zonas que comportam), e o padrão fino pré-filtrado pra virar tom em vez de sumir.
> - **Na v3:** a ondulação, o sulco e a borda redesenhados na escala que a câmera do jogo lê, e o detalhe fino só onde couber. A v2 está congelada.

**O que é:** a areia física com zonas. Os arquivos congelados são `Game/Tools/look_materials/sand_desert_v2.json` (todos os números) e `sand_desert_v2_ground.hlsl` (o shader). **Atenção (14/10): o mapa do jogo NÃO usa esta areia.** Desde 13/10 às 03:31 o WorldForge monta o `World_TestSalt2` com `SAND=classic` (o log diz `FORGE desert sand: classic`), porque a v2 estourava na luz antiga do jogo. A luz mudou na noite de 13/10 (`desert_golden_dust`) e o A/B pra trocar, que era do Look, não foi feito. A areia v2 só existe nos mapas de teste.

**Como chegou lá:**
1. **v1 (12/10, "uma das melhores até agora"):**
   - o albedo físico em linear (0,49/0,40/0,27): a cor vem da luz, não da textura;
   - a textura só dá o desenho: o grão do `gravelly_sand` a 1,3 m e 4,1 m (girada 37°), mais os montinhos da foto aérea de praia (`aerial_beach_01`) a 6 m e 2,3 m, em manchas de 25 m;
   - a variação em 8/40/200 m (±10%) contra a repetição.
2. **O que falhou no caminho:**
   - a ondulação de seno no shader virou listras;
   - a ondulação no chão todo foi regressão (ele quer a v1 como base, com a ondulação rara e em lugares certos).
3. **v2:**
   - **As zonas:** um mapa de formas irregulares de 3–15 m (ruído com deformação), em quatro tipos: liso, ondulado, pedregoso, revirado. Cada zona muda o material (grão, fresta, tom) e o espalhamento.
   - **As ondulações:** simuladas (o modelo de Nishimori & Ouchi, 1993; `look_ripples.py`, ladrilho de 10 m, onda de ~22 cm), só na zona delas, com o sol a ~155° do vento (as cristas riscam) e o hex tiling (Mikkelsen 2022) contra a repetição.
   - **A trilha da esteira:** relevo de verdade (sulco de 2,2 cm, garras a cada 17 cm, a bordinha), a idade, e a mais nova por cima. A sombra é assada com passo de meio pixel (o passo inteiro dava "pontinhos").
   - **O espalhamento pelas zonas:** capim no pisado, pedrinhas no pedregoso, gravetos no revirado.
4. **A guarda:** a areia nova só vai pro jogo com a luz em que foi medida. Com a luz do jogo ela saiu clara demais. O critério de estouro: p95 de 0,65 a 0,75, e menos de ~2,5% estourado.

**Como reproduzir:** os números estão no JSON. No jogo e no WorldForge, as zonas viram ruído no shader (a mesma estatística), e o vento do mapa sai da semente (87° − 155° ± 20).

---

## 2. A cratera de explosão v2 (13/10: "já ficou bom pra kct")

**O que é:** a cratera de tiro na areia, no nível da `sand_gpt` e da `street_battle`. Ela foi provada na maquete (`look_layout_crater.py`, `look_ground.py`: `crater_v2_values`, `crater_v3_relief`) por cima do `crater()` do jogo (`bake_craters.py`). Folhas: `captures/look/cratera_texturas_v6.jpg` e `cratera_prova_v5.jpg`.

**Como chegou lá:**
1. **O estudo:**
   - o Battlefield 3 deforma a altura e monta a cor a ~3 cm/texel;
   - o CoH pinta a altura e põe decals;
   - o Arma escolhe o efeito pelo material do chão;
   - a física: na areia seca a bacia é mole e com poucos torrões; no asfalto, a placa levanta em volta.
2. **A segunda opinião:** o atlas de 4,4 cm carimbado num caderno de 4,7 cm e lido de novo dá ~9 cm efetivos, e o claro de +13% lê dois tons. Mais resolução não resolve (o jogo mediu 2,35 cm: ganho zero).
3. **As voltas da prova:**
   - **Volta 1:** relevo de verdade (malha de 2,5 cm). Saiu pequena e sem halo.
   - **Volta 2:** o tamanho da `sand_gpt` (ra 2,0) e o halo escuro até ~1,95 R.
   - **Volta 3:** o "fofinho" do Felipe. A parede em pé (o perfil 1 − (1 − t)^1,6) e os calombos viraram pedras 3D. Encheu demais a bacia.
   - **Volta 4:** nada no núcleo, torrões escuros.
   - **Volta 5:**
     - o fundo com textura;
     - o escuro só no núcleo torto (a parede do lado do sol acende sozinha);
     - sem o claro pintado onde há pedra (lia "pipoca");
     - os riscos de fuligem;
     - 170 pedrinhas em raios.
   - **Volta 6:** o manto de ejeção escuro e mosqueado até ~2,6 R, o lábio de pedras graúdas em ~60% da borda, o relevo fino da terra revirada (o fim da "massinha") e as pedras em lei de potência.

**Os números** (estão em `crater_v2_values` e `crater_v3_relief`):
- **O perfil:** h = −d·(1 − (1 − t)^1,6), com um blur leve.
- **O núcleo escuro:**
  - raio deformado ±65%, de 0,15 a 0,55 R;
  - manchas de carvão de ~0,4 m.
- **O manto:**
  - G de 0,35 a 0,7, até 2,6 R, mais comprido pro lado do tiro;
  - 14 riscos de 1,2 a 3,5 R.
- **O relevo fino:** ±1,5 cm (ruído de 12 cm), mais ±2 cm (30 cm), mais torrõezinhos de 9 cm (+2,5 cm), até 2,8 R.
- **As pedras:** ~320 de 3–7 cm, 170 de 10–22 cm, 26 de 25–45 cm no lábio, mais os picos do relevo.

**Pro jogo:** passado pra sessão do jogo como alvo do rebake do atlas, com rampas de distância pras bordas nítidas, a sombra da bacia na hora do carimbo e os debris 3D nos torrões.

**Cada chão** (a regra da sessão de arte):
- a areia escorre;
- a terra lasca;
- o asfalto quebra em placas;
- a grama rasga.

A MF tem a entrada `Surf` (o peso do asfalto): no asfalto o escuro é quase preto e o claro é cascalho cinza.

---

## 3. A vila contra as 7 favoritas (13/10, 15 voltas)

**O que é:** a maquete do deserto dourado batendo com as favoritas em saturação, calor, curva e névoa. Folhas: `captures/look/vila_antes_depois.jpg`. A receita é a bancada `desert_golden_hour.json`.

**O que fez a diferença:**
1. **Medir por faixa de brilho**, não a média: o excesso de saturação estava só nas sombras (0,69 contra 0,48).
2. **O lilás das sombras:** vinha do rebote quente (`lumen_diffuse_color_boost` 1,2) somado ao céu azul. Ele voltou a 1,0.
3. **Na Unreal, a areia ao sol cai no meio-tom:** o claro só começa no `ColorCorrectionHighlightsMin`, e a sombra vai até o `ColorCorrectionShadowsMax`. Mexer no meio-tom e baixar o `HighlightsMin` (0,15) pra o ouro pegar.
4. **O ouro na luz:** o sol a 4700 K, um pouco mais claro e com mais brilho.
5. **O vestir da vila:** o entulho em regras (telhado, pé de parede, beira de rua, terreno; a rua das unidades limpa), um tom por casa e a sujeira subindo da base.
6. **A explosão provisória sai do quadro:** ela estragava a leitura.

**O que falhou** (não repetir):
- o brilho direcional da névoa (`directional_inscattering`) vela o quadro inteiro;
- névoa demais levanta as sombras ("lavado").

---

## 4. A carcaça queimada (13/10, maquete e jogo)

**O que é:** marrom-carvão com variação, não bloco preto. Medida na carcaça da `street_battle`.

**As cores e a cobertura** (sRGB):

| Camada | Cor | Onde |
|---|---|---|
| Tinta queimada | 92,74,60 | ~55% |
| Fuligem | 34,30,27 | embaixo e em manchas |
| Ferrugem de calor | 118,70,42 | manchas de ~1,5 m |
| Cinza | 165,155,142 | em placas no teto |

**Como fica na tela:** p10/p50/p90 ≈ 0,03/0,33/0,60.

**O miolo quente:** corpo negro de 1300 → 900 K, pico de ~3000 nits, apagando em (1 − t/30)², mais 20 s de brasas.

**Onde está:**
- na maquete, o `BURNT_CODE` (`look_lab.py`);
- no jogo, o `M_Structure`, com dado de primitiva pro calor.

**A lição:** o ruído limiarizado da foto de praia vira chuvisco; o jogo trocou por ruído de valor 3D procedural.

---

## 5. O asfalto (13/10)

**Na bancada (volta 15, aprovada na maquete):**
- 1,2× mais claro;
- uma película de areia em manchas (14 m);
- as rachaduras engrossadas pra ler a 61 m;
- tudo em `look_asphalt.py` (`ASPHALT_CODE`).

**No jogo foi regressão:** na luz do jogo (`v2_aprovada`) leu lilás e ruidoso. O tom AG tinha azul pra compensar a luz quente da bancada. O WorldForge voltou pro asfalto de antes.

**A regra:** o que é calibrado na bancada só vai pro jogo junto com a luz da bancada, no A/B com o Felipe.

**O asfalto v2** (13/10, o Felipe no A/B na luz do jogo: "a qualidade do novo asfalto tá surreal de bom"), no espírito da areia v2.
- **A base:** o asfalto de antes do WorldForge, que o Felipe gosta.
- **Por cima:** os defeitos do catálogo de pavimento do FHWA (Distress Identification Manual), cada um no lugar dele:
  - a trilha de roda com o jacaré;
  - o óleo no meio da pista;
  - as trincas em bloco, as longitudinais e as transversais;
  - a desagregação na beira;
  - os remendos e a areia;
  - as zonas grandes (bom, cansado, abandonado).
- **O teste:** A/B na luz do jogo.
- **Os arquivos:** `look_asphalt.py`, `ASPHALT_V2_CODE`. O WorldForge liga no mapa de teste `World_TestAsph2`, com os campos assados na textura do RoadDir: `Across`, `Width`, `Edge`.
- **O que saiu e por quê:**
  - **As trilhas de roda polidas com o jacaré.** Ele: "você não vê isso no dia a dia, nem no game shot". A 61–100 m viravam 4 listras claras paralelas.
- **O v2d (13/10, aprovado pelo Felipe: "top, melhorou; de resto aprovado"):**
  - os remendos escuros (×0,72–0,80), foscos e com a borda recortada por ruído de ~0,9 m (antes eram claros, lisos e retos, e liam "quadrados do nada");
  - o óleo numa faixa de ~1 m.
  - **O que falta:** o óleo ainda "não parece óleo". É o próximo, com estudo de referência.
- **O óleo v4 (13/10, em teste):** o Felipe rejeitou a faixa e os pingos em fila ("ninguém vai ver isso como óleo"). O caminho:
  - **As referências da internet**, que ele escolheu (salvas em `captures/look/oleo_refs/`): o estacionamento visto de cima (Unsplash, Kotkowicz) e os pingos no concreto, mais a mancha acumulada com reflexo.
  - **O atlas de carimbos gerado:** não existe mancha escaneada gratuita (ambientCG e Poly Haven: nada). O `look_oil.py` gera 4 tipos: a fosca grande, o grupinho separado, os pingos espalhados e a poça viscosa. É ruído suave com a borda deformada, miolo manchado com poros, a beirada mais escura e os respingos em volta.
  - **Raro e aleatório:** células de 14 m, ~28% com um carimbo, o tipo, o lugar, o giro e o tamanho sorteados. "Bem aleatório pra fugir de padrão."
  - **O estudo do brilho** (GGX com o sol de três-quartos): óleo liso demais vira um buraco preto em vez de brilhar. O reflexo da poça só na beira e em faixas, com rugosidade 0,25; o resto fosco.
- **Rejeitado (13/10): borrar a foto aérea** pra tirar os arcos de pneu repetidos. O Felipe viu no mapa do jogo: "regressão, tudo borrado, simples; deixa como antes, mesmo com a marca de pneu". Borrar tira a variação de mancha que a foto dá a 61–100 m. Se voltar ao assunto: quebrar a repetição sem tirar o detalhe (girar ou deslocar a amostra por zona), nunca borrar.
- **A lição da linha de óleo:** com a rugosidade menor (o óleo brilha), o reflexo do sol baixo anulava o escuro e ela sumia. Agora é fosca e escura ×0,62. A mesma coisa deixou os remendos um pouco mais claros em vez de escuros: um defeito "mais liso" clareia em contraluz.

---

## 6. A estrada com volume (13/10, protótipo)

> **Estado em 14/10: só na maquete, nunca foi pro jogo.** O Felipe procurou na vila do jogo e viu a estrada pintada no chão. Fica pra v3 (a v2 não tem mais ajuste), com a prova na câmera do jogo. Os 3 tipos que ele viu: `docs/aprovados/look/2026-10-13_estrada_volume_3tipos.jpg`.

**O pedido:** "a nossa é textura pintada no chão; a do game shot é uma estrada em cima do chão, uma camadinha bonita".

**O estudo:**
- BF3, Ghost Recon, Arma, Cities, Anno, a doc da Unreal;
- a melhor relação visual/ms é a malha da laje gerada a partir do traçado, opaca, com a borda quebrada na geometria;
- a areia encostada é o próprio chão levantado.

**O protótipo** (`look_ground.py`, `road_slab`):
- a laje de 12 cm, malha de 4 cm, o abaulado de 2,5 cm;
- a borda em placas Voronoi de 50 cm, das quais 38% faltam e 55% afundam e inclinam perto da beira, com a fresta entre elas;
- a face da quebra escura.

**A divisão:**
- **WorldForge:** o traçado e o leito rebaixado.
- **Look:** o kit (perfil, borda, material).
- **Jogo:** a altura na simulação e a inclinação visual do tanque.

---

## 7. A faixa pintada da estrada (13/10, em teste)

**O estudo**, nas favoritas e nas outras do deserto: o recorte ampliado de 8 imagens, em `captures/look/faixa_branca_estudo.jpg`.
- **A faixa da beira:**
  - fica a ~0,5 m da beira do asfalto, com 12–15 cm, contínua;
  - **acompanha o contorno da rua**: na esquina ela faz a curva (a close_game_cam).
- **A cor:** creme empoeirada, nunca branco puro. Brilho ~0,55–0,6 contra ~0,3 do asfalto.
- **A idade muda pela estrada, em trechos:**
  - quase nova (a road_town_battle de baixo);
  - gasta, com falhas (a road_battle, a close_game_cam);
  - quase sumida, só o "fantasma" (a three_factions, a street_battle);
  - ausente (a usa_vs_china_gpt).
- **A trinca passa por cima e corta a faixa**, e faltam lasquinhas ao longo.
- **A tinta velha descasca em placas de 10–50 cm.** Onde saiu, fica o fantasma: o asfalto um pouco mais claro.
- **A areia que invade a pista cobre a faixa:** a areia vem por cima dela, nunca o contrário.

**Como foi feito** (a maquete):
- `look_ground.paint` com `paint_v2`: a faixa da beira é a distância até a beira da rua (−d_sand − 0,55 m), então a curva da esquina sai sozinha.
- **Gravada como rampa:** 0,5 é a borda da faixa.
- **No material** (`look_lab`, `PAINTV2`):
  - o corte nítido com antisserrilhado (fwidth);
  - a idade em trechos de 30 m;
  - a descascada pelo ruído, com o limiar subindo com a idade;
  - o fantasma de 18%;
  - as lascas, o corte da trinca e o encardido pela idade.
- **A ordem:** a mistura da areia vem depois.

---

## 8. A luz da v2: o deserto na hora dourada com poeira (13/10, escolhida pelo Felipe)

**O que é:** a receita `Game/Tools/look_recipes/desert_golden_dust.json`, que vira `/RTSLook/Recipes/DA_Look_desert_golden_dust` pelo `look.ps1 recipes`. É a luz padrão do World_TestSalt2 (`Game/Config/RTS/Maps.json`, `look_recipe`).
- **Sol:** 8° de altura, em contraluz pela direita (from_camera −42 na câmera inicial de verdade).
- **Base:** a luz A da bancada.
- **Poeira dourada:** só no terço de cima do quadro.

**Como chegou lá:**
1. **A luz A no jogo:** as receitas da bancada no jogo, em A/B, com o cenário `vila_foto`. O Felipe achou a A bonita no jogo. A B, calibrada pela cartela com a gradação zerada, foi descartada: a 2ª opinião a chamou de "filtro sépia" (a sombra saturada demais e quase sem alta-luz).
2. **O lado do sol:** o `ALookDirector` fixa o sol pela câmera do começo da partida (o giro 45°, `DefaultYawDeg`). Fotos de cenário que giram a câmera mostram o sol deslocado. Por isso foram criados os cenários `look_inicial` (100,4 m, 37,5°, giro 45°) e `look_zoom` (67 m, 28°), no Scenarios.json. Ali o −42 dá a contraluz pela direita dos game shots de deserto.
3. **O horário:** 4 alturas na câmera real (8, 12, 18, 24°). O Felipe escolheu 8°. A EV de cada altura saiu da luz que chega no chão (sen da altura × transmitância), e a 8° fica em 12,1 no jogo.
4. **A poeira dourada,** estudada com 2 opiniões antes do código:
   - **A física e a Unreal:** a fonte da 5.8. Os pontos usados:
     - σ = 0,05·densidade/m na volumétrica;
     - o início é um plano de profundidade, escalado pela distância da câmera no `UpdateFocus`;
     - o brilho direcional da névoa não é físico (proibido);
     - o albedo laranja de antes absorvia 76% do azul e dava sépia.
   - **A medição dos 9 game shots:** o véu mora só nos 30% de cima, ~0,10–0,15. Ele quase não cresce com a câmera deitada, não clareia pro lado do sol, levanta a sombra do topo pra ~`#57422F` e não tira a cor.
   - **3 voltas no jogo, medidas com o mesmo script dos game shots** (`haze_measure.py --img`):
     - **1ª** (início 1,15×, densidade 0,10, g 0,6): forte no canto do sol.
     - **2ª:** perdida, porque o jogo quebrava na abertura enquanto o BlastLab trocava o shader.
     - **3ª** (o pedido do Felipe: "suavizar o degradê, um pouco menos densa, alongar"): início 1,0× a distância (o degradê ~1,4× mais longo), g 0,35, densidade 0,06.

**Os números da escolhida** (o bloco `game` da receita):
- **Névoa:** densidade 0,06, falloff 0,25, `start_ratio` 1,0. A volumétrica com o mesmo início, albedo (252,249,247), espalhamento 0,35, extinção 1,0, distância 250 m.
- **Atmosfera:** `aerial_start_ratio` 1,0.
- **Pós:** EV 12,1, saturação 0,82, exposição local da sombra 0,62.
- **Medido contra o alvo:**
  - véu no topo 0,10 (alvo 0,09–0,15);
  - sombra do topo `#5E4731` (alvo `#57422F`);
  - contraste topo/base 0,54 (alvo 0,5–0,6);
  - a metade de baixo limpa.
- **O que ficou acima do alvo:** o lado do sol 2,1× mais claro (os game shots ≤ 1,0). Boa parte disso é a cena em contraluz a 8°, que já dava 1,8 sem névoa.

**O que ficou pra v3:**
- o `r.SupportExpFogMatchesVolumetricFog` (a névoa analítica com fase física; ele recompila todos os shaders);
- o `VolumetricScatteringIntensity` da SkyLight;
- a névoa nos presets baixa e média, que ficam sem a volumétrica.

**Erros que custaram rodadas:**
- fotos com a câmera girada (o sol aparece no lugar errado);
- uma foto travada segurando a placa (agora toda cadeia de fotos usa um vigia que mata pelo PID se o log parar 120 s);
- o jogo quebrando na abertura enquanto outra sessão trocava o shader.
