# O chão da v3: a areia, a trilha e a cratera que leem na câmera do jogo

Estudo de 14/10/2026. Foi feito só lendo: nada aberto na Unreal, nada rodado na placa, nada mudado fora deste
arquivo. Teve pesquisa com 4 agentes e uma 2ª opinião crítica; o que a 2ª opinião mudou está na seção 9.

**O pedido do Felipe.** Na maquete, a areia, a trilha das lagartas e a cratera eram lindas. No jogo viraram chão liso,
risco chapado e mancha. Ele mandou isso pra v3 e quer "100% como os game shots", comparado quadro a quadro.

**O que não muda: o zoom.** "nosso RTS precisa ter um zoom bom pra entender o jogo, precisa trabalhar em cima disso".

Como ler os números:
- **(est.)** é estimativa e só vale depois do A/B no jogo.
- As medidas de imagem saíram de scripts no rascunho. O essencial deles está no apêndice.
- "Linear" é o brilho em luz linear, não o valor do pixel na tela.

Os alvos:
- o game shot `docs/aprovados/alvos/r1_raid_slabs_gemini.webp` e o recorte `docs/concept/ground/target_tracks_dust_r1_raid_slabs.jpg`;
- a folha da trilha `docs/concept/ground/tracks_sand_C_approved.jpg`;
- a cratera `docs/aprovados/look/2026-10-13_cratera_v2.jpg`.

---

## Em resumo (o que muda pra ele)

1. **O game shot cabe no pixel do jogo.**
   - O shot não tem detalhe menor que ~5 px, ou seja, ~22 cm de chão.
   - Reduzido ao pixel do nosso zoom normal, ele ainda mostra as trilhas em arco e as ondulações.
   - Resolução não falta. Falta desenhar o que lê nessa escala e filtrar direito o que não lê.
2. **O risco escuro do sulco, no shot, é uma sombra de sol.**
   - Medido em linear, o risco fica em 0,31 da areia em volta: a razão sombra/sol dos alvos.
   - Ele tem 1 a 2 px de largura, com uma borda clara de +13% e o fundo da faixa a −23%.
   - Com o sol a 8° do jogo, a física dá isso sozinha: um sulco de 4 cm faz 28 cm de sombra.
3. **Hoje o jogo apaga o que não cabe no pixel e escurece a cor pela sombra borrada.**
   - Some a ondulação, some a garra.
   - Sai uma faixa escura uniforme de ~20 cm, sem borda clara e sem fundo: o "risco chapado".
4. **A ideia central: o sol não anda na partida.**
   - A luz do relevo fino (quanto do sol chega em cada ponto, já com a sombra) pode ser calculada uma vez, em
     resolução alta, e guardada numa textura com mips.
   - Média de luz é média certa. De perto, a textura desenha o sulco. De longe, vira o tom certo (a areia ondulada
     fica ~10% mais escura).
   - Não pisca e não some.
   - No material, essa luz vira um leve giro da normal pro sol ou contra ele (a "normal de iluminação"). Assim a
     sombra das unidades continua certa por cima, em todos os presets, sem passada extra.
5. **Areia:**
   - o mapa do jogo passa pra areia v2 (hoje usa a antiga);
   - a ondulação ganha duas escalas: a física (22 cm), que vira tom, e uma ondulação de jogo de 0,7 a 1,4 m, que
     lê de 6 a 21 px. Ondulações desse tamanho existem na natureza: no deserto do Negev elas têm ~70 cm de onda e
     ~5 cm de altura.
6. **Trilha:**
   - largura real (63,5 cm). No nosso zoom ela ocupa os mesmos ~8 px da trilha do shot;
   - sulco de 4 cm. Na curva, 6 a 8 cm, mais largo e com a areia empurrada pra fora da curva, como medido em
     tanques reais;
   - bordinha de 3 × 20 cm;
   - a garra vira tom.
7. **Cratera:**
   - a bacia aprovada, com paralaxe (POM) só dentro dela. A 28°, o fundo de 50 cm desloca 0,94 m na tela (9 px);
     sem paralaxe ela é mancha;
   - a sombra de 8° assada: a bacia inteira na sombra e a borda acesa.
8. **Custo: −0,2 a +0,3 ms no Alta (est.).** Pode empatar ou ficar mais barato que hoje: o material atual lê ~32
   amostras por pixel de chão, e o novo leria ~8 a 10.
9. **A prova é no jogo, nas câmeras do jogo, nos 4 giros da câmera.**
   - Contra o game shot e as folhas aprovadas, reduzidos ao mesmo pixel.
   - Com números: o perfil do sulco, o espectro da ondulação, os tons da cratera, a cintilação e os ms.

---

## 1. O que está errado hoje (o diagnóstico, medido)

### 1.1 A câmera e o pixel no chão

O código: `Game/Source/RTSGame/Private/RTSCameraPawn.cpp` (`CurvePitch`, `UpdateCamera`) e `Game/Config/DefaultGame.ini`.
As resoluções por preset: `RTSPlayerController.cpp`, `ResolutionPct` = 50/58/67/100%. O pixel no chão sai da projeção
da câmera, em cm, na horizontal × na profundidade. Topo e pé são as bordas da tela.

| Câmera | Preset | Topo (borda) | Centro | Pé (borda) |
|---|---|---|---|---|
| Perto: 67 m, 28°, 50° (o código) | Alta (67%) | 9,6 × 40,4 | 4,9 × 10,4 | 3,3 × 4,6 |
| | Ultra (100%) | 6,4 × 27,0 | 3,3 × 6,9 | 2,2 × 3,1 |
| Perto deitado (< 25°), campo de 58° | Alta | — | 5,8 × 12,3 | — |
| **Normal: 100,4 m, 37,5°, 50°** | **Alta** | **11,1 × 27,6** | **7,3 × 12,0** | **5,4 × 6,6** |
| | Média (58%) | 12,8 × 31,9 | 8,4 × 13,8 | 6,3 × 7,7 |
| | Baixa (50%) | 14,8 × 37,0 | 9,8 × 16,0 | 7,3 × 8,9 |
| | Ultra | 7,4 × 18,5 | 4,9 × 8,0 | 3,6 × 4,5 |
| Longe: 118 m, 44°, 46° | Alta | 10,3 × 19,8 | 7,8 × 11,2 | 6,2 × 7,2 |

**Achado: o zoom perto tem 50° de campo, não 58°.**
- O campo só abre pra 58° quando o jogador deita a câmera abaixo de 25° com o botão do meio: `Low = (25 − Pitch)/20`
  em `UpdateCamera`. No zoom perto padrão (28°), `Low = 0`.
- Os "67 m a 58°" do `RECEITAS_DO_LOOK.md` (74 m de largura) são o caso deitado. No padrão, a largura no centro é
  62,5 m e o pixel é 4,9 × 10,4 cm no Alta.
- Conferir numa foto do `look_zoom`, com o tanque como régua.

**Achado: o giro da câmera é livre** (as setas, `RTSPlayerController.cpp` ~812).
- O sol fica preso no mundo. Com a câmera girada a 180°, a contraluz vira luz de frente e o par risco/claro do
  sulco se inverte.
- Toda prova é nos 4 giros.

**Achado: a Unreal afia as texturas pro TSR.**
- `SceneVisibility.cpp:5338` põe um viés de mip de −log2(1/fração) − 0,3: Baixa −1,30, Média −1,09, **Alta −0,88**,
  Ultra −0,30.
- As texturas do material são lidas mais finas que a grade interna, contando que o TSR resolva o resto.
- Isso ajuda o grão, mas piora a cintilação de padrão perto de Nyquist (seção 4.4).

### 1.2 O game shot, medido

A r1_raid_slabs tem 1024 × 572 px. O casco de 7,9 m ocupa ~180 px, então são **4,4 cm de chão por pixel**.

**O "2× mais perto" é enquadramento, não detalhe.**
- O nosso zoom normal tem 4,9 cm por pixel de saída a 1920 px, quase o mesmo do shot.
- Na grade interna do Alta são 7,3 cm.
- O shot "parece" mais detalhado porque a tela dele mostra só 45 m e é ampliado ~1,9× no monitor.

| O quê | Medido no shot | Em linear |
|---|---|---|
| Energia do espectro abaixo de 5 px (~22 cm) | quase zero | — |
| Ondulação (recorte 790–890 × 500–560) | pico de 14–20 px = **0,6–0,9 m** de onda | — |
| Contraste da areia (desvio/média, valor de tela) | 0,18 na ondulada, 0,10 na calma | — |
| Sulco reto: o risco | mínimo 0,41 contra 0,69 da areia, largura de 1–2 px | **0,31** |
| Sulco reto: a borda clara (1 px antes do risco) | 0,73 contra 0,69 | **1,13** |
| Sulco reto: o fundo da faixa | 0,63 contra 0,71 | **0,77** |
| Faixa de uma lagarta | 6–8 px (~30 cm no shot) | — |
| Os arcos | risco 0,45–0,55 linear, fundo ~−20% | — |

**Reduzido ao pixel do Alta normal** (a caixa de ×0,6), o shot ainda mostra:
- as trilhas em arco, como dois riscos com a faixa no meio;
- as ondulações.

A folha C aprovada (~1,45 cm/px), reduzida ×5 até 7,3 cm, também mostra a faixa e a ondulação, como textura. **Uma imagem bem filtrada no nosso pixel ainda lê.**

### 1.3 O jogo hoje

1. **O mapa do jogo usa a areia antiga.**
   - Fonte: `docs/LEVANTAMENTO_14-10.md`, achado 1. O `World_TestSalt2` é montado com `SAND=classic`.
   - A areia v2 só existe nos mapas de teste.
2. **A ondulação nunca aparece.**
   - Em `WorldForge/Tools/build_slice.py` ~580: `kT = zr·…·(1 − smoothstep(0.03, 0.06, fw))`, com
     `fw = length(fwidth(p))`.
   - Com a câmera girada a 45°, esse `fw` é ~1,6× o pixel real (o `fwidth` soma |ddx| + |ddy| por eixo; a MF já
     corrigiu isso em 13/10, a areia não).
   - Então o corte cai num pixel real de ~2 a 4 cm: nem o pé da tela no zoom perto do Ultra (2,2 × 3,1 cm) mostra
     a onda inteira.
   - O detalhe some sem deixar o tom: o "chão liso".
3. **A MF da memória do chão** (o CODE de `Game/Tools/look_mf.py`):
   - **~32 leituras de textura por pixel de chão.** Os cadernos não têm mip, e a média é feita à mão: relevo 4 + 4,
     gradientes 8, garra 4, cratera 10, marcas 2. Tudo fora de ramo, na janela de 384 m.
   - **A sombra escurece a cor:** `bc *= lerp(1, vis, 0.85·k·(1 − wall))`. Isso tira o sol e o céu juntos (a sombra
     desce a 0,15, mais escura que a real de 0,24–0,36). Na parede, a sombra é tirada.
   - **A garra só aparece até Nyquist** (pixel ≤ 5,7 cm). Depois some, sem tom.
   - **O cálculo do sol** (`Game/Shaders/Private/GroundRelief.usf`):
     - relevo a 9,4 cm por texel;
     - horizonte de 7 meios passos (até 33 cm); na bacia, no máximo 40 texels;
     - média de 5 pontos.
     - O sulco é 2,2 × 0,75 × (0,65 a 1,35) = **1,1 a 2,2 cm**.
     - A altura cabe em ±4 cm (`ReliefRangeCm = 4`, com `saturate`).
4. **A prova da trilha foi numa câmera que o jogador não tem.**
   - O `look_esteira` usa 45 m. Mesmo ali, o perfil (`captures/game/lk_esteira_depois/06_cena_013s.png`) é uma faixa
     escura uniforme: 0,55 linear, ~7 px (~20 cm), bordas macias, sem borda clara, sem fundo.
   - É o "risco chapado".
5. **A cratera só tem normal e cor.**
   - A 28–37,5°, o fundo de 50 cm devia deslocar 0,65 a 0,94 m na tela em relação à borda (5 a 9 px).
   - Sem paralaxe, a bacia parece pintada. O truque da parede escondida (`hideW`) só aproxima isso.
6. **A tesselação ainda está ligada no código.**
   - O `DISP_CODE` entra no material por padrão (`build_slice.py:641`, `DISP='1'`).
   - O jogo liga o `DispOn` no Alta e no Ultra (`RTSGroundMemory.cpp` ~574).
   - O `DefaultScalability.ini` põe `r.Nanite.Tessellation=1` no ShadingQuality 3.
   - O `CHAO_DEFORMAVEL.md` diz que o WorldForge reverteu depois dos buracos. **Conferir no mapa e desligar de vez na
     v3 base:** com o POM por cima, a cratera sairia dobrada.

### 1.4 O sol a 8°: a física ajuda e atrapalha

O Felipe escolheu o sol a 8° de altura, em contraluz pela direita (`RECEITAS_DO_LOOK.md` §8).

**As sombras ficam longas:** sombra = h ÷ tan 8° = **7,1 × a altura**.

| Altura | 1,1 cm | 2,2 cm | 4 cm | 6 cm | 15 cm (borda da cratera) | 50 cm (fundo da bacia) |
|---|---|---|---|---|---|---|
| Sombra | 7,8 cm | 15,7 cm | 28,5 cm | 42,7 cm | 1,07 m | 3,56 m |

**A luz fica nervosa.**
- O chão plano recebe sen 8° = 0,139 do sol.
- Uma rampa de ±5° recebe 0,225 ou 0,052: **±60%** (a 30° de sol seriam ±15%).
- Microrelevo do tamanho do pixel, desenhado por normal, cintila 4× mais forte que na luz da bancada: o
  "pixel espalhado".
- **A média tem de ser feita na luz, não na normal.**

**Na contraluz, as paredes em sombra são as viradas pra câmera, que aparecem esticadas na tela.** As paredes acesas
viram de costas e aparecem espremidas. É a estrutura do shot: risco escuro largo e borda clara fina. Com a câmera
girada pra luz de frente, isso se inverte.

**Simulação de um corte do sulco** (Lambert, sol a 8° em contraluz, sombra/sol 0,4, média no pixel do jogo). φ é o
ângulo entre a direção do sol e a perpendicular à trilha.

| Perfil | φ = 0° | φ = 45° | φ = 70° | φ = 90° (trilha paralela ao sol) |
|---|---|---|---|---|
| Hoje: 1,65 cm, bordinha 2 × 9 cm | risco 0,28, claro 1,38 | 0,29 / 1,26 | 0,43 (oscila 0,21) / 1,11 | 0,80 / 1,00 |
| **Proposta: 4 cm, bordinha 3 × 20 cm** | **0,25 / 1,44** | **0,25 / 1,30** | **0,26 (oscila 0,06) / 1,13** | 0,72 / 0,99 |
| 6 cm, bordinha 4 × 25 cm | 0,24 / 1,49 | 0,24 / 1,34 | 0,24 (oscila 0,01) / 1,15 | 0,68 / 0,99 |

O pixel é o do Alta normal (7,3 cm na tela). "Oscila" é quanto o risco muda conforme a trilha cai na grade de
pixels: é o quanto ela pisca quando a câmera anda.

O que a tabela diz:
- **O sulco de hoje já faria o risco com φ pequeno**, se a sombra chegasse inteira e nítida ao pixel.
- **Com 4 cm, o risco aguenta até ~70° sem piscar.** O de hoje oscila 0,21.
- **O lado aceso passa do shot** (1,3 a 1,5 contra 1,13): é preciso limitar o claro, como controle de arte medido
  contra o shot.
- **Com a trilha paralela ao sol, o risco some na física**: sobram só a cor da faixa e a oclusão do céu.

**Simulação da ondulação** (a mesma luz, sombra/sol 0,3):

| Onda | Tom (média) com as cristas ⟂ ao sol | Tom com as cristas ∥ ao sol | px por período, normal Alta (centro / topo) | Quanto varia dentro do pixel |
|---|---|---|---|---|
| 11 cm | 0,89 | 0,97 | 0,9 / 0,4 | 5% |
| **22 cm (a v2)** | 0,91 | 0,98 | **1,8 / 0,8** | **29%: cintila** |
| 35 cm | 0,89 | 0,97 | 2,9 / 1,3 | 51% |
| 70 cm × 5 cm | 0,87 | 0,97 | 5,8 / 2,5 | 62% (lê como desenho) |
| 1,2 m × 7 cm | 0,88 | 0,97 | 10 / 4,3 | 63% (lê como desenho) |

**A areia ondulada fica ~10% mais escura que a lisa nessa luz, mesmo quando a onda não cabe no pixel.** Esse é o
tom que se perde quando o detalhe é apagado.

---

## 2. As fontes e o que cada uma ensina

**A cota de buscas na web acabou.** Os 4 agentes de pesquisa gastaram o limite de buscas desta rodada. Ficaram sem
fonte técnica:
- Company of Heroes 3;
- Total War;
- Wargame e Steel Division (Eugen);
- Men of War e Gates of Hell (GEM);
- os detalhes da lama e da neve do RDR2;
- Snowrunner, além do Spintires.

Dá pra continuar numa próxima mensagem. **[lido]** = o texto ou código foi aberto. **[trecho]** = só o resumo da busca.

### 2.1 Detalhe que troca de escala em vez de sumir

| Fonte | O que ensina pra nós |
|---|---|
| Toksvig, *Mipmapping Normal Maps*, JGT 2005 (relatório NVIDIA 2004) [lido]: https://developer.download.nvidia.com/whitepapers/2006/Mipmapping_Normal_Maps.pdf | **A média das normais numa mip fica mais curta que 1, e o encurtamento mede a variação.** σ² ≈ (1 − \|Na\|)/\|Na\|; fator ft = 1/(1 + s(1/\|Na\| − 1)). O relevo que some vira brilho mais fosco. Só vale pro brilho especular. |
| Olano & Baker, *LEAN Mapping*, I3D 2010 [página]: https://userpages.cs.umbc.edu/olano/papers/lean/ ; post do Olano: https://gaim.umbc.edu/?p=509 | **Guarda a média e os segundos momentos da inclinação (5 valores em 2 texturas), filtrados pelas mips comuns.** A covariância vira o lobo especular. Pede 16 bits; DXT não serve. |
| Baker, *CLEAN*, GDC 2011 (citado pelo Hill) | O LEAN com 3 valores: perde a anisotropia, ocupa metade. |
| Dupuy et al., *LEADR Mapping*, SIGGRAPH Asia 2013 [resumo]: https://perso.liris.cnrs.fr/victor.ostromoukhov/publications/pdf/SAsia2013-LEADR.pdf | **O mais perto do nosso caso:** os momentos do *deslocamento* com mips, e o mascaramento e a sombra **também na difusa**. É a prova acadêmica de que, de longe, o relevo vira tom e não some. Caro pra nós; a seção 4 faz a versão barata com o sol fixo. |
| Han et al., *Frequency Domain Normal Map Filtering*, SIGGRAPH 2007 [lido]: https://www.cs.columbia.edu/cg/normalmap/normalmap.pdf | Filtrar normal é convolução da distribuição de normais com o BRDF. 3 a 4 lobos vMF bastam. |
| Kaplanyan et al., *Filtering Distributions of Normals*, HPG 2016 [lido]: https://research.nvidia.com/sites/default/files/pubs/2016-06_Filtering-Distributions-of/NDFFiltering.pdf | A variação da normal dentro do pixel (por ddx/ddy) somada à rugosidade. Custa 0,09–3,7 ms em 4K. |
| Tokuyoshi & Kaplanyan, JCGT 2021 [lido]: https://www.jcgt.org/published/0010/02/02/paper-lowres.pdf | A versão barata: α² += min(2σ²(\|ddx n\|² + \|ddy n\|²), 0,18), com σ² = 1/2π. ~0,1 ms em 8K. Só especular. |
| Neubelt & Pettineo, *The Order: 1886*, SIGGRAPH 2013 [lido]: https://blog.selfshadow.com/publications/s2013-shading-course/rad/s2013_pbs_rad_notes.pdf | **Assam a variação nas mips da rugosidade** (vMF: rugosidade' = √(r² + 1/κ)): custo zero no jogo. Preferiram mip trilinear a ver cintilação. |
| Hill, *Specular Showdown in the Wild West*, 2011 [lido]: https://blog.selfshadow.com/2011/07/22/specular-showdown/ | O "mapa de Toksvig" assado: uma leitura de textura. |
| Chan, *Material Advances in Call of Duty: WWII*, SIGGRAPH 2018 [lido]: http://www.realtimerendering.com/advances/s2018/MaterialAdvancesInWWII-course_notes.pdf | **A normal de detalhe que desce de mip vira "brilho de detalhe".** É o modelo industrial de "o detalhe vira tom". |
| McAuley, *Far Cry 3*, SIGGRAPH 2012 [slides]: https://blog.selfshadow.com/publications/s2012-shading-course/mcauley/s2012_pbs_farcry3_notes_v2.pdf | Toksvig assado num canal livre do normal map, no lugar do LEAN, por memória e desempenho. |
| Widmark, *Terrain in Battlefield 3*, GDC 2012 [lido]: https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/gdc12-terrain-in-battlefield3.pdf | **Recomenda pôr o detalhe, como a oclusão, no mapa de cor pra esconder altura e normal borradas.** A textura virtual vai a ~3,1 cm por texel; a cratera recompõe o bloco na hora (0,2–0,5 ms no PS3). |
| Bruneton & Neyret, *Survey of non-linear pre-filtering*, IEEE TVCG 2012, DOI 10.1109/TVCG.2011.81 [não aberto] | A referência geral: filtrar linearmente uma entrada não linear (normal, altura) é errado. O que se filtra certo é o resultado da luz. |
| Quilez, *filtering*, *bandlimiting* e *checkerfiltering* [lidos]: https://iquilezles.org/articles/filtering/ , https://iquilezles.org/articles/bandlimiting/ , https://iquilezles.org/articles/checkerfiltering/ | Nyquist pede ~2 amostras por ciclo, e os limiares são arbitrários. A onda filtrada analiticamente **tende à média** em vez de sumir. |
| Epic, *Temporal Super Resolution* [lido]: https://dev.epicgames.com/documentation/en-us/unreal-engine/temporal-super-resolution-in-unreal-engine | O TSR não recupera um detalhe de 1 px que anda meio pixel por quadro. Padrão repetido pequeno faz moiré. |

**A lição do grupo:**
- quando o relevo fica menor que o pixel, a variação dele vira rugosidade no brilho, ou cor/oclusão assada no difuso;
- nunca zero;
- o filtro tem de ser aplicado **ao resultado da luz** ou aos momentos, não à normal.

Pra areia, com rugosidade ~0,9, o brilho especular quase não conta. O que importa é o **difuso com sombra** na luz
de 8°: o caso LEADR. A seção 4 resolve isso de um jeito barato, porque o nosso sol é fixo.

### 2.2 Relevo sem geometria, e a sombra calculada uma vez

| Fonte | O que ensina pra nós |
|---|---|
| Tatarchuk, *Dynamic POM with Approximate Soft Shadows*, I3D 2006 [lido]: https://faculty.digipen.edu/~gherron/references/References/SurfaceDetail/Tatarchuk-POM-SI3D06.pdf ; notas SIGGRAPH 2006 [lido]: https://www.advances.realtimerendering.com/s2006/Tatarchuk-POM.pdf | **O que usar na cratera:** busca linear com interseção por reta, sem busca binária, porque cada passo binário espera o anterior. Passos variam de 8 a 50 conforme o ângulo, e de longe vira bump. Sombra suave por amostras na direção da luz. Escrever profundidade no pixel desliga o Hi-Z. |
| Policarpo, Oliveira & Comba, *Relief Mapping*, I3D 2005 [lido]: https://www.inf.ufrgs.br/~oliveira/pubs_files/Policarpo_Oliveira_Comba_RTRM_I3D_2005.pdf | Linear (até 32) + binária (5–8 passos). A sombra dura é uma segunda busca pra luz. |
| Dummer, *Cone Step Mapping*, 2006 [lido]: https://faculty.digipen.edu/~gherron/references/References/SurfaceDetail/ConeStepMapping.pdf ; Policarpo & Oliveira, *Relaxed Cone Stepping*, GPU Gems 3 cap. 18 [lido]: https://developer.nvidia.com/gpugems/gpugems3/part-iii-rendering/chapter-18-relaxed-cone-stepping-relief-mapping | Bem menos passos (15 de cone + 6 binários), mas o pré-cálculo é O(n²) e o mapa de cones não aceita mip comum. **Não serve pra um caderno que muda a cada tiro.** |
| Max, *Horizon Mapping*, The Visual Computer 1988, DOI https://doi.org/10.1007/BF01905562 [citado] | **A sombra do relevo guardada por texel como ângulo do horizonte, em algumas direções.** Com o sol parado, basta uma direção: é o nosso caso. |
| Sloan & Cohen, *Interactive Horizon Mapping*, EGWR 2000 [lido]: https://www.ppsloan.org/publications/bs.pdf | 8 direções em 2 RGBA, pra luz que se move. Nós não precisamos. |
| Oat & Sander, *Ambient Aperture Lighting*, I3D 2007 [lido]: https://cse.hkust.edu.hk/~psander/docs/aperture.pdf | Bent normal + cone de visibilidade (4 canais) pra luz de área e céu, feito pra terreno. Penumbra pelo tamanho do sol. |
| Snyder & Nowrouzezahrai, EGSR 2008 [relatório]: https://www.microsoft.com/en-us/research/wp-content/uploads/2016/02/tr-2008-77.docx | Horizonte multirresolução; pro sol, 3 azimutes bastam e a sombra fica mais nítida. |
| Timonen & Westerholm, *Scalable Height Field Self-Shadowing*, Eurographics 2010 [resumo]: https://diglib.eg.org/handle/10.2312/CGF.v29i2pp723-731 | **O horizonte exato por varredura de linha, em tempo linear:** 1024² com 64 direções a 23 qps numa GTX 280. Com uma direção só, a varredura no rumo do sol custa O(1) por texel (seção 5.2). |
| Mittring, *Finding Next Gen: CryEngine 2*, SIGGRAPH 2007 [lido]: https://www.advances.realtimerendering.com/s2007/Mittring-Finding_NextGen_CryEngine2(Siggraph07%20Course%20Notes).pdf | **O alerta:** tentaram assar o horizonte do terreno e desistiram, porque os objetos em cima precisam de sombra correta. Pra nós: o que assamos fica só no relevo fino do chão, e as unidades continuam com a sombra virtual. |
| Epic, nó `ParallaxOcclusionMapping` da 5.8 [lido no asset, `Engine/Content/Functions/Engine_MaterialFunctions01/Texturing/ParallaxOcclusionMapping.uasset`] | Laço `while (i < MaxSteps+2)` com `SampleGrad` e interseção por reta (o Tatarchuk). Min Steps olhando de cima, Max Steps rasante. Sombra desligada por padrão; a luz vem de uma Material Parameter Collection. O próprio nó avisa: a saída Pixel Depth Offset "will cause a problem with Dynamic Shadows". |
| Ryan Brucks (Epic), fórum [lido]: https://forums.unrealengine.com/t/pom-material/13050?page=21 | Palpite, não medida: "a 'cheap' POM with ~16 steps would cost at least 1-2ms" na tela cheia. **Por isso o POM só roda dentro da cratera.** |
| Epic, *Contact Shadows* [lido]: https://dev.epicgames.com/documentation/en-us/unreal-engine/contact-shadows-in-unreal-engine | Marcha no depth buffer, 8 passos na 5.8 (`DeferredLightingCommon.ush`). Grosso demais pra um sulco de 1–2 px e precisa de PDO: não serve. |
| RDR2 no PC [lido]: https://pcoptimizedsettings.com/red-dead-redemption-2-rdr2-optimized-settings-for-pc-2024/ | O POM em Ultra custa ~4%. O patch 1.19 corrigiu sombras em bloco e tremulação ligadas ao POM. |

### 2.3 Como os jogos fazem trilha e cratera legíveis de cima

| Jogo e fonte | O que ensina pra nós |
|---|---|
| **C&C Generals Zero Hour**, código liberado [lido]: [W3DTerrainTracks.cpp](https://github.com/TheSuperHackers/GeneralsGameCode/blob/02b5b56e22fa6a1ca06a3fc2f7648e646e9f56cb/Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DTerrainTracks.cpp), [W3DScorch.cpp](https://github.com/TheSuperHackers/GeneralsGameCode/blob/02b5b56e22fa6a1ca06a3fc2f7648e646e9f56cb/Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DScorch.cpp), [GameLOD.cpp](https://github.com/TheSuperHackers/GeneralsGameCode/blob/02b5b56e22fa6a1ca06a3fc2f7648e646e9f56cb/GeneralsMD/Code/GameEngine/Source/Common/GameLOD.cpp) | **Uma faixa só pro veículo inteiro**, da largura entre os bones TREADFX + 4 unidades. Uma aresta a cada 10 unidades, 100 arestas (25 opacas). Some em 60–300 s. Sem normal, sem sombra, sem relevo: tudo vem da textura desenhada. O chão não deforma; são até 500 queimados num atlas 3 × 3. |
| **Beyond All Reason / Recoil** [lido]: [BasicMapDamage.cpp](https://github.com/beyond-all-reason/RecoilEngine/blob/01efdf43121dfd40d105649ef7eb89a8804662d9/rts/Map/BasicMapDamage.cpp), [GroundDecalHandler.cpp](https://github.com/beyond-all-reason/RecoilEngine/blob/01efdf43121dfd40d105649ef7eb89a8804662d9/rts/Rendering/Env/Decals/GroundDecalHandler.cpp), [GroundDecalsFragProg.glsl](https://github.com/beyond-all-reason/RecoilEngine/blob/01efdf43121dfd40d105649ef7eb89a8804662d9/cont/base/springcontent/shaders/GLSL/GroundDecalsFragProg.glsl) | **A cratera deforma o heightmap** com o perfil "sombrero" (funda no centro, borda levantada), e a profundidade cai com a dureza do chão. O rastro é decal **com normal map, iluminado pelo sol e com a sombra do shadow map**: por isso o sulco acende e escurece. Os devs reclamam que a cratera pequena serrilha na resolução do heightmap. |
| **Supreme Commander / FAF** [lido]: [UEL0202_unit.bp](https://github.com/FAForever/fa/blob/0eb3d6150a6a4c6df377ab116e9dce1cd7ff5cd7/units/UEL0202/UEL0202_unit.bp) | Carimbo a cada 0,1–0,2 s, ~72% da largura do tanque, decals com `_normal`. Sem exagero de largura: lê pelo contraste. |
| **Company of Heroes 1/2** [lido]: https://www.semiaccurate.com/2013/07/15/company-of-heroes-2-relic-performance/ , https://www.pcgamesn.com/relic-wanted-make-eastern-front-expansion-company-heroes-couldn-t-do-snow , https://coh2.org/guides/11703/how-to-using-template-images-in-worldbuilder | CoH1: a cratera deforma o chão e vira cobertura. CoH2: pegada e lagarta são deformação persistente na neve, com custo admitido. Os "splats" do terreno têm cor, normal e especular. **CoH3: sem fonte.** |
| **Batman: Arkham Origins**, GDC 2014 [lido]: https://www.slideshare.net/colinbb/gdc2014-deformable-snow-rendering-in-batman-arkham-origins , https://gdcvault.com/play/1020379/Deformable-Snow-Rendering-in-Batman | Altura no máximo 512² por superfície, e **"fica melhor em resolução baixa"**: o rastro borrado lê melhor que o serrilhado. Relief mapping nos consoles, tesselação no PC; <1 ms no PS3/360. |
| **Rise of the Tomb Raider** (Michels & Sikachev, GPU Pro 7) [resumo]: https://gameenginegems.com/gemsdb/article.php?id=1314 ; https://history.siggraph.org/wp-content/uploads/2022/10/2015-Talks-Michels_Labs-R_D-Rendering-Techniques-in-Rise-of-the-Tomb-Raider.pdf | **Afunda no rastro e levanta nas laterais** (a berma), e o rastro se enche com o tempo. 4 MB. Tesselação, então a sombra é de geometria. |
| **Spintires / MudRunner** [lido]: https://www.gamedeveloper.com/programming/mud-and-water-of-spintires-mudrunner | **O sulco da roda é desenhado com paralaxe, não com deslocamento.** A lama fica a 128² por bloco de 16 m (**12,5 cm/texel**, quase o nosso pixel). |
| **Battlefield 3**: a fonte da seção 2.1 | A cratera muda a altura e a profundidade depende do material físico. O bloco da textura virtual é recomposto na hora. |
| **World in Conflict** [lido]: https://bit-tech.net/previews/gaming/pc/world_in_conflict_beta_impressions/3/ | Crateras proporcionais à arma, e **pequenas de propósito pela jogabilidade**, até a da bomba nuclear tática. |
| **War Thunder / Dagor** [lido]: https://warthunder.com/en/news/7585-development-dagor-engine-6-5-new-surface-rendering-en ; https://github.com/GaijinEntertainment/DagorEngine | "Microshadows" (cada pedrinha faz sombra) por cima do deslocamento, e crateras que viram cobertura. O código já estudado em `CHAO_DEFORMAVEL.md` usa R8 com 0,5 = zero e 4,4 cm/texel no preset alto. |
| **God of War Ragnarök** (`CHAO_DEFORMAVEL.md`): https://80.lv/articles/santa-monica-s-senior-programmer-on-how-god-of-war-ragnar-k-s-snow-system-was-made/ | Trocaram paralaxe por tesselação **só nos pedaços com rastro**. |
| **DOOM 2016** [lido]: https://www.adriancourreges.com/blog/2016/09/09/doom-2016-graphics-study/ | Decals no passe forward, em atlas; não serve pro chão do RTS. |

**A lição do grupo:**
1. **Os RTS clássicos** (Generals, Spring, SupCom) não exageram a largura da trilha. Lêem pelo contraste da textura,
   e os melhores pela normal mais a sombra do sol.
2. **Quem quer "sulco com relevo"** usa paralaxe (Spintires), deformação (CoH, BF3, Spring, Batman, RotTR) ou
   microssombra (Dagor).
3. **A cratera é sempre altura de verdade.** Ela é pequena ou rasa pela jogabilidade.
4. **Ninguém desenha o detalhe abaixo do pixel.** Ou ele é borrado (Batman: "fica melhor em resolução baixa"), ou
   vira cor (BF3).

### 2.4 A escala física

| O quê | Número | Fonte |
|---|---|---|
| Esteira do M1 Abrams (T-156/T-158) | **63,5 cm**; passo da sapata **19,4 cm** (7,625 in); centros a **2,845 m**; contato 4,58 m; 0,94–1,08 kg/cm² | afvdatabase [lido] https://afvdatabase.com/usa/m1abrams.html ; FAS [lido] https://man.fas.org/dod-101/sys/land/m1.htm |
| Leopard 2 / T-72 / M113 | 63,5 cm / 58 cm (bitola 2,79 m) / 38 cm (centros 2,16 m) | https://www.tankarchives.com/2017/05/leopard-tracks.html [lido]; https://cyclowiki.org/wiki/Т-72 [lido]; https://afvdatabase.com/usa/m113.html [lido] |
| Pneu do Humvee | 31,8 cm, bitola 1,79 m | Liu 2009 [lido] |
| **Sulco do MBT em areia seca** | **solo seco: "nenhum ou raso demais pra medir"** (Fort Riley); solo molhado: M1A1 até ~8–10 cm. **Na curva fechada (raio < 30 m) a largura passa de ~100 cm e a terra raspada vai pra fora da curva.** Passadas + curvas: +65 a +548% de profundidade. Na conta de Bekker, areia seca solta dá ~8–9 cm; areia firme, ~0,6 cm (os parâmetros do Wong não foram conferidos). | Liu, tese UTK 2009 [lido] https://trace.tennessee.edu/utk_graddiss/620 |
| Faixa proposta pelo agente (juízo) | reto em areia do deserto **1–5 cm**; curva, pivô ou duna solta **5–10 cm**; berma **1–3 cm** (a maior parte é compactação) | — |
| A perturbação em volta | até 25 cm de fundo e **50 cm pra fora da borda da esteira** | Prose 1985 [lido] https://pubs.usgs.gov/publication/70013037 |
| A cor da trilha | na crosta, o rastro fica **cinza-claro e liso, "muito visível"**; em duna e leito seco, o vento apaga | Prose & Wilshire 2000 [lido] https://pubs.usgs.gov/of/2000/0512/pdf/of00-512.pdf |
| Quanto dura | no pavimento desértico, visível após ~55 anos; em areia ativa, horas a dias | Belnap & Warren 2002 [lido] https://pubs.usgs.gov/publication/70024554 |
| **Ondulação comum** | onda de **5–20 cm (até 30)**; altura 0,5–1 cm; altura ≈ **4% da onda**; a onda cresce com o vento | Andreotti et al. 2006 [lido] https://arxiv.org/abs/cond-mat/0506758 ; Sharp 1963, Kelso [trecho] https://digital-desert.com/blog/ripples/ |
| **Megaondulação** | **30 cm a vários metros**, razão onda/altura < 15. Areia **bimodal**, com grão grosso na crista (crista mais áspera e clara), crista ondulada e interrompida, dias a anos pra formar. Em **superfície de deflação, corredor entre dunas, lençol de areia e cascalho residual**; na duna ativa, só a comum. Nahal Kasuy (Negev): **~70 cm de onda, ~5 cm de altura, nunca acima de 7 cm** | Tholen et al. 2023 [lido] https://arxiv.org/abs/2208.00888 ; Yizhaq, KITP 2013 [lido] https://online.kitp.ucsb.edu/online/geoflows_c13/yizhaq/pdf/Yizhaq_GeoFlows13Conf_KITP.pdf ; Ellwood et al. 1975 [trecho] |
| **Cratera de obus** | por Kinney & Graham: superfície D ≈ 0,8·W^⅓, apoiada 0,51·W^⅓; profundidade ≈ D/4. **155 mm de contato (6,86 kg): D ≈ 1,0–1,7 m, 0,25–0,65 m de fundo; 105 mm: 0,65–1,15 m.** Com retardo (est. do agente): 2,5–4,5 m e 1–1,5 m. **A marca visível de cima (o ejetado e o queimado) vai de 3 a 9 m.** | Ambrosini et al. 2004 [lido] https://amcaonline.org.ar/ojs/index.php/mc/article/download/271/260 ; 2007 [lido] https://amcaonline.org.ar/ojs/index.php/mc/article/download/1332/1280 ; ADA111994 (não aberto) https://apps.dtic.mil/sti/tr/pdf/ADA111994.pdf ; Magnini et al. 2017 [lido] https://opus.bibliothek.uni-augsburg.de/opus4/files/94457/94457.pdf |

**O que isso diz pra nós:**
- **A trilha de 63,5 cm é a real e fica real.** No nosso zoom normal ela ocupa ~8,7 px, o mesmo número de pixels da
  trilha do shot (6–8 px), porque o shot está 2× mais perto.
- **O sulco de 4 cm está no limite de cima da física** pra tanque andando reto em areia seca. Isso é defensável. O
  de 6–8 cm fica pra curva.
- **A curva larga, com a areia empurrada pra fora, é física medida.** É ela que faz os "arcos" do shot.
- **A ondulação de 22 cm da v2 é física, mas não cabe no pixel.**
  - A de 0,7–1,4 m também é física (a megaondulação) e é a que o shot mostra (0,6–0,9 m).
  - Ela pertence às zonas abertas e de deflação, não à duna ativa: casa com a zona "ondulado" da v2.
- **As crateras A/B/C (1,95 / 2,6 / 3,4 m, 50 cm) ficam entre o obus de contato e o de retardo.** É um exagero leve,
  coerente com a leitura do RTS (o WiC e o CoH fazem o contrário pela jogabilidade; a nossa não vira cobertura).

### 2.5 A Unreal 5.8 (o fonte instalado, conferido)

- **`ParallaxOcclusionMapping`**: o código está na seção 2.2. Ele aceita coordenadas do mundo ("Use World
  Coordinates"). Tudo o que ele faz dá pra escrever no nosso Custom, que é o que vamos usar.
- **`NormalCurvatureToRoughness`** (`MaterialTemplate.ush:4272`) usa só a normal da geometria, não o normal map.
  **Não pega a areia nem a esteira.**
- **A rugosidade pela variação do normal map** (Composite Texture, `TextureCompressorModule.cpp:3259–3294`) é Toksvig
  assado na hora de montar a textura: custo zero. Serve pro grão estático; não serve pro caderno, que muda.
- **A light function no sol foi estudada e descartada** (seção 4.3):
  - O resultado dela vai pra um canal de 8 bits da máscara da luz (`LightFunctionPixelShader.usf:78–81`,
    `EncodeLightAttenuation`): **não passa de 1**, então não faz a borda acesa.
  - O Lumen avalia a mesma light function no cache de superfície, num compute shader
    (`LumenSceneDirectLighting.cpp` ~1360).
  - A névoa volumétrica pede a light function do sol sempre que ela existe (`VolumetricFog.cpp:1584`).
  - Um material que lê a posição no mundo não entra no atlas (`HLSLMaterialTranslator.cpp:1769`). O atlas tem
    128 px por luz e não serviria mesmo.
- **O AO do material vai pro Lumen:** `r.Lumen.ScreenProbeGather.MaterialAO = 1` (`LumenScreenProbeGather.cpp:128`).
  - O pino AO do chão já é usado pela areia (`AO = sM.r`, `build_slice.py:586`): o SkyAO multiplica.
  - O Lumen aplica o multi-bounce no AO (`LumenScreenProbeGather.usf` ~1446): calibrar.
- **O pino Bent Normal não serve aqui:** ele só mexe na oclusão indireta (`LumenScreenProbeGather.usf` ~1331).
- **O PDO não chega na sombra:** o `ShadowDepthPixelShader.usf` não tem termo de PDO, e o nó POM avisa isso. No
  Landscape Nanite, ele empurra o chão pro raster programável.
- **Tesselação Nanite:** `r.Nanite.MaxCandidatePatches` e `MaxVisiblePatches` (`NaniteShared.cpp:64/72`). Os buracos
  do TestSalt2 estão em `CHAO_DEFORMAVEL.md`.

---

## 3. A conta de pixels: o que se vê em cada zoom

**A regra** (ppp = pixels por período, ou por largura, no eixo pior):
- **ppp ≥ 4: desenha.** Relevo, sombra e desenho, estável com o TSR.
- **2 ≤ ppp < 4: transição.** Do desenho pro tom, na textura com mip; nunca um corte.
- **ppp < 2: só tom.** A média certa da luz.

O limiar teórico é 2 (Nyquist). O 4 vem da prática: o TSR não segura padrão de 1–2 px em movimento, e a v2 da
maquete já sumia a onda abaixo de ~4 px. Com a luz em textura com mip (seção 4), a placa de vídeo faz essa transição
sozinha; o número só serve pra conferir.

Em cada célula: px na horizontal / px na profundidade, no centro da tela, exceto onde a coluna diz outra coisa.

| Detalhe | Perto, Alta | Perto, pé, Alta | Perto, Ultra | **Normal, Alta** | Normal, topo, Alta | Normal, Ultra | Normal, Baixa | Veredito |
|---|---|---|---|---|---|---|---|---|
| Onda 11 cm | 2,2 / 1,1 | 3,3 / 2,4 | 3,3 / 1,6 | 1,5 / 0,9 | 1,0 / 0,4 | 2,2 / 1,4 | 1,1 / 0,7 | **só tom** |
| Onda 22 cm (v2) | 4,5 / 2,1 | 6,7 / 4,8 | 6,7 / 3,2 | **3,0 / 1,8** | 2,0 / 0,8 | 4,5 / 2,8 | 2,2 / 1,4 | tom no normal; desenha só no pé do perto |
| Onda 35 cm | 7,1 / 3,4 | 11 / 7,6 | 11 / 5,1 | 4,8 / 2,9 | 3,2 / 1,3 | 7,1 / 4,4 | 3,6 / 2,2 | transição |
| Garra 17–19 cm | 3,7 / 1,7 | 5,5 / 3,9 | 5,5 / 2,6 | 2,5 / 1,5 | 1,6 / 0,7 | 3,7 / 2,2 | 1,8 / 1,1 | **só tom**; desenho só no Ultra perto |
| Bordinha 9 cm | 1,8 / 0,9 | 2,7 / 2,0 | 2,7 / 1,3 | 1,2 / 0,8 | 0,8 / 0,3 | 1,8 / 1,1 | 0,9 / 0,6 | sub-pixel: só a luz dela conta |
| Bordinha 20 cm | 4,1 / 1,9 | 6,1 / 4,3 | 6,1 / 2,9 | 2,7 / 1,7 | 1,8 / 0,7 | 4,1 / 2,5 | 2,0 / 1,2 | transição: o par claro/escuro macio |
| Sombra do sulco de 2,2 cm (15,7 cm) | 3,2 / 1,5 | 4,8 / 3,4 | 4,7 / 2,3 | 2,1 / 1,3 | 1,4 / 0,6 | 3,2 / 1,9 | 1,6 / 1,0 | risco fino, pisca |
| **Sombra do sulco de 4 cm (28,5 cm)** | 5,8 / 2,7 | 8,6 / 6,2 | 8,6 / 4,1 | **3,9 / 2,4** | 2,6 / 1,0 | 5,8 / 3,6 | 2,9 / 1,8 | **o risco de 1–2 px do shot** |
| Esteira 63,5 cm | 13 / 6,1 | 19 / 14 | 19 / 9,2 | **8,7 / 5,3** | 5,7 / 2,3 | 13 / 7,9 | 6,5 / 4,0 | desenha sempre |
| Onda de jogo 70 cm | 14 / 6,7 | 21 / 15 | 21 / 10 | **9,6 / 5,8** | 6,3 / 2,5 | 14 / 8,8 | 7,1 / 4,4 | desenha; no topo, transição |
| Onda de jogo 1 m | 20 / 9,6 | 30 / 22 | 30 / 15 | 14 / 8,3 | 9,0 / 3,6 | 20 / 13 | 10 / 6,2 | desenha |
| Bitola 2,85 m | 58 / 27 | — | 86 / 41 | 39 / 24 | 26 / 10 | 58 / 36 | 29 / 18 | — |
| Bacia A / B / C (1,95 / 2,6 / 3,4 m) | 40–69 / 19–33 | — | 59–103 / 28–49 | 27–47 / 16–28 | 18–31 / 7–12 | 40–69 / 24–43 | 20–35 / 12–21 | forma: geometria da tela |
| **Paralaxe do fundo de 50 cm** | 94 cm = 9 px | — | 14 px | 65 cm = 5 px | — | 8 px | 4 px | **sem POM, a bacia é pintada** |

**Em português claro:**
- No zoom do jogo **não dá pra ver** a ondulação de 22 cm, a garra nem a bordinha de 9 cm como desenho. As três têm de
  virar tom, não sumir.
- **Dá pra ver:**
  - a faixa da esteira (5 a 13 px);
  - o risco da sombra de um sulco de 4 cm (2 a 6 px);
  - uma ondulação de 0,7 a 1,4 m (6 a 21 px);
  - a cratera inteira, com paralaxe.

---

## 4. A ideia central: o sol parado vira textura

### 4.1 O que é

**O ponto de partida:**
- Na partida, o sol não se move.
- O relevo fino é parado até um tanque passar ou um tiro cair: a ondulação nunca muda, e a trilha e a cratera mudam
  só onde foram carimbadas.

**Então a luz do sol sobre esse relevo pode ser calculada uma vez,** no carimbo ou na montagem, em resolução alta, e
guardada como um número **linear**:

> **SunLit(x) = [ max(0, N(x)·L) × V(x) ] ÷ sen(elevação)**
>
> É quanto do sol chega naquele ponto em relação ao chão liso. 1 = igual ao plano; 0 = sombra; 1,3 = rampa virada
> pro sol. N é a normal do relevo fino inteiro, V a visibilidade do sol pelo horizonte, L a direção do sol.

**Por que isso resolve:**
- Um número linear tem média certa. A mip de SunLit é exatamente o que um pixel distante vê, a média da luz na área
  dele: é a "folha reduzida" da seção 1.2.
- Na prática:
  - **de perto**, a mip 0 desenha o sulco, a borda acesa e a sombra;
  - **na transição**, a placa de vídeo filtra entre as mips (trilinear e anisotrópico);
  - **de longe**, sobra o tom: a areia ondulada ~10% mais escura, a faixa da trilha com a garra virando um cinza
    mosqueado. Nunca o liso, nunca o pisca.
- Isso troca as duas coisas que falharam:
  - **apagar pelo fwidth** perde o tom (o "chão liso");
  - **a normal no tamanho do pixel** pisca na luz de 8° (o "pixel espalhado").

**De onde vem:**
- O horizon mapping (Max 1988) com uma direção só. O Sloan & Cohen guardam 8 porque a luz deles anda.
- O "detalhe no mapa de cor" do BF3.
- A normal de detalhe que vira brilho do CoD WWII.
- O difuso com sombra do LEADR.

O que é nosso: com o sol fixo do RTS, dá pra assar também o N·L, não só a oclusão. **O alerta da Crytek vale:** o que
se assa é só o relevo fino do chão; as unidades seguem com a sombra virtual normal, multiplicada por cima.

### 4.2 Onde isso mora

| Textura | O quê | Quando é calculada | Dono |
|---|---|---|---|
| **RippleLit** (o ladrilho da ondulação, 10 m, com mips) | a luz da ondulação, nas duas escalas, pro sol da receita e o vento do mapa | na montagem da receita (`look.ps1 recipes`), ~1 s por par sol/vento, fora do jogo | Look |
| **SunLit + SkyAO no caderno** (detalhe e meio, com mips) | a luz e a oclusão do céu da trilha, da garra, da bordinha e da cratera | no carimbo, pelo compute do relevo (`GroundRelief.usf`), com supersampling | jogo |

### 4.3 Como a luz chega só no sol: a normal de iluminação

**A ideia** (proposta pela 2ª opinião, conferida aqui):
- No material do chão, a normal final é girada no plano que contém a normal e o sol, só o bastante pra que o N·L da
  Unreal dê exatamente a luz assada.
- O resto da iluminação segue normal.

> **t = SunLit_total × (N·L)**, com SunLit_total = SunLit do caderno × RippleLit, limitado a [−0,05; 1]
>
> **N' = t·L + √(1 − t²) · normalize(N − (N·L)·L)**
>
> N é a normal de hoje: o terreno e o grão da areia. L aponta pro sol. N'·L = t, exato. Com SunLit = 1, N' = N.

**Por que é melhor que qualquer outro caminho:**
- **A sombra das unidades fica certa.** A sombra virtual multiplica o N·L da Unreal. Debaixo de um tanque (a 8°, a
  sombra dele tem ~17 m), a marca da trilha não escurece em dobro.
- **Funciona em todos os presets**, sem passada extra e sem corte por altura.
- **Passa de 1:** a borda acesa do shot sai.
- **A paralaxe da cratera roda uma vez só**, no material.
- **O céu quase não muda.** No chão plano, o giro vai de −8° (sombra) a +1,5° (claro de 1,2), e o céu muda menos de 1%.
  Numa encosta virada pro sol, numa microssombra, o giro chega a ~30° e o céu cai até ~7%.
- **O degrau é pequeno.** A normal na GBuffer tem ~10 bits por componente, então o SunLit anda em passos de ~1,4%. O
  TSR dilui.
- **O que muda junto:** o brilho especular (a areia tem rugosidade ~0,9; quase nada) e o AO de tela.

**O que foi descartado e por quê:**
- **A light function no sol:**
  - não passa de 1;
  - o Lumen a avalia em compute no cache de superfície, onde `Sample` e `ddx` num Custom podem não compilar e o
    material cai no padrão sem aviso;
  - a névoa volumétrica a avalia também;
  - precisaria do corte por altura pras unidades e da paralaxe repetida.
- **Escurecer a cor:** certo no sol, mas na sombra de uma unidade a marca fica ×0,48 (com f_sol 0,7) em vez de ~1. A
  8°, a sombra de um tanque tem ~17 m e a de um prédio de 6 m, ~43 m: o erro aparece muito.
- **O pino Bent Normal:** só mexe na oclusão indireta.

**A luz do céu:** pelo pino **AO** do material (SkyAO, multiplicando o AO que a areia já põe), que o Lumen usa.

**A normal do relevo fino não entra na normal do material.** Nem a do caderno nem a da ondulação. Senão o N·L seria
contado duas vezes, e a normal não tem a sombra do horizonte: o tom pularia na transição.

### 4.4 Os cuidados

- **O viés de mip do TSR:**
  - Nas texturas de luz, anular o viés da tela: amostrar com `-View.MaterialTextureMipBias` de viés (varia por
    preset: +1,30 a +0,30).
  - Medir a cintilação com e sem.
- **O lado aceso:**
  - A 8°, a rampa virada pro sol passa de 1,4 (seção 1.4), e o shot fica em 1,13.
  - Limitar o SunLit a ~1,15–1,2 no carimbo é um controle de arte, medido contra o shot.
- **A trilha paralela ao sol perde o risco (φ > 70°).** Duas saídas, em A/B, com a decisão do Look:
  - aceitar, porque na física é assim e a faixa ainda lê pela cor e pela oclusão do céu;
  - ou calcular a sombra com o sol "espalhado" ±15–20° no rumo. Isso é artístico, não físico.
- **O sol muda com a receita** (noite, outro mapa): refazer a RippleLit e o SunLit do caderno inteiro, distribuído
  em vários quadros. A RippleLit depende também do vento do mapa (`wind_yaw`): uma por par sol/vento.
- **O grão da areia** (o normal map do `gravelly_sand`, com mips) é o caso clássico do Toksvig. A 8° ele pode
  faiscar: entra na mesma medida de cintilação. Se faiscar, a rugosidade composta (Composite Texture) ou o fade do
  grão pela mip.

---

## 5. A proposta para a v3

### 5.1 A areia

**1. O mapa do jogo na areia v2** (WorldForge). Montar o `World_TestSalt2` com `SAND=v2` no lugar de `classic`.
- O motivo do `classic` era a luz antiga. A luz mudou pra de 8° (`desert_golden_dust`).
- Antes, um A/B na luz do jogo com as medidas de conforto (`RECEITAS_DO_LOOK.md` §1; memória "razão sombra/sol"):
  - p95 de 0,65 a 0,75 e < 2,5% estourado;
  - quase preto em 5–6%;
  - sombra/sol de 0,24 a 0,36.
- **Isso sozinho já tira boa parte do "chão liso":** voltam as zonas, o grão e a fresta.

**2. A ondulação em duas escalas, numa textura de luz** (Look: `look_ripples.py`, `sand_desert_v3.json`; a v2 fica
congelada).

**A física: 22 cm** (a de hoje, Nishimori & Ouchi).
- Altura ~0,9 cm (4% da onda, Andreotti).
- **Vira tom em quase todo o zoom.**

**A ondulação de jogo: 0,7 a 1,4 m em duas oitavas**, com altura de 1/14 a 1/20 da onda (5 a 7 cm, o teto do Negev).
- Crista ondulada e interrompida.
- Grão grosso na crista: um pouco mais clara e áspera, pela cor e pela rugosidade.
- Só na zona "ondulado" e no deserto aberto longe do pisado (a regra `open_desert_rule` da v2), nunca na vila.

**As duas entram no mesmo ladrilho de 10 m.**
- A `RippleLit` é a luz pro sol da receita, em R8 ou BC4, com mips, a ~0,5 cm por texel.
- No material, ela é lida com as 3 amostras do hex-tiling que já existem (só muda o canal lido).
- **O corte `1 − smoothstep(0.03, 0.06, fw)` sai.**
- **A normal da ondulação sai do N·L.** A RippleLit entra no `SunLit_total` da normal de iluminação (seção 4.3). De
  perto, a mip 0 a 0,5 cm já desenha a onda.

**3. A trilha apaga a ondulação** (já existe: `RippleMask`). No fundo da trilha, a RippleLit vai pra 1, e a
ondulação volta com a idade.

### 5.2 A trilha

| O quê | v2 (hoje) | **v3** | Por quê |
|---|---|---|---|
| Largura de uma lagarta | 64 cm | **64 cm** (fica) | M1 63,5 cm. No nosso zoom são ~8,7 px, como os 6–8 px do shot. |
| Bitola (entre centros) | 2,85 m | **2,85 m** (fica) | M1 2,845 m |
| Sulco, andando reto | 2,2 × 0,75 × (0,65–1,35) = 1,1–2,2 cm | **4 cm × (0,8–1,2)** | física 1–5 cm (seco) a 8–9 cm (solto); o risco firme até φ ≈ 70° (seção 1.4) |
| Sulco na curva e no pivô | igual ao reto | **6–8 cm, largura até ~1 m na curva fechada** | Liu 2009: raio < 30 m → > 100 cm de largura, várias passadas +65 a +548% |
| Bordinha | 2 cm × 9 cm | **3 cm × 20 cm; na curva, a de fora 2×, a de dentro ~0** | física 1–3 cm; 20 cm = 2,7 px (vira o par claro/escuro macio); a terra raspada vai pra fora da curva (Liu) |
| Faixa da altura no caderno | ±4 cm (`ReliefRangeCm = 4`, com `saturate`) | **±10 cm ou mais, ou R16F** | o sulco de 4,8 cm e o de 8 cm da curva seriam cortados |
| Halo revirado | — | **até 50 cm além da borda, só tom e rugosidade** | Prose 1985 |
| Garra | 17 cm, normal até Nyquist, depois nada | **19,4 cm (M1)**, ou 17 cm se o modelo do tanque mandar; **assada no SunLit** (supersampling 2 × 2, ~2,35 cm) | vira o tom mosqueado da faixa; desenho só no Ultra, no pé do zoom perto |
| A sombra | 9,4 cm, horizonte de 33 cm (40 texels na bacia), a média escurece a cor | **SunLit a 4,7 cm (Alta) com supersampling, pela varredura no rumo do sol (abaixo); SkyAO a 9,4 cm, 4 direções, raio ~30 cm; o claro limitado a ~1,15** | seção 4 |
| A cor da faixa | ×(0,84; 0,80; 0,76) fresca | **pela zona**: na areia solta ou ondulada, mais escura e quente (o shot: fundo −23% linear); no liso, pisado ou pedregoso, **mais clara e lisa** (crosta: "cinza-claro e liso", Prose & Wilshire; a folha C: +12%) | a física e os dois alvos |
| A idade | o vento enche, a garra some primeiro | fica; na areia solta mais rápido, no pisado mais devagar | Belnap & Warren; Prose & Wilshire |
| Paralaxe (POM) | — | **não** | 4 cm × cot(28–37°) = 5–7,5 cm, menos de 1 px |
| Tesselação | ligada no código (seção 1.3) | **desligada de vez** | não ganha da luz assada (`CHAO_DEFORMAVEL.md`, item 9 da revisão) |

**O horizonte pela varredura no rumo do sol** (a ideia do Timonen & Westerholm, com uma direção só).
- Cada fio da placa anda uma linha no rumo do sol, da borda de onde vem o sol até a outra, carregando o horizonte:

> **S(x) = max( h(x), S(x − Δ) − Δ·tan(sol) )**; o ponto está na sombra quando S(x) > h(x).

- É exata e custa O(1) por texel, sem teto de distância. A sombra de 3,6 m da bacia e a de 1,07 m da borda da
  cratera, que cai fora da bacia, saem sem caso especial. Hoje a marcha só roda com a altura da cratera < −1 cm.
- **O retângulo do carimbo é dilatado contra o sol** até a sombra mais longa possível.

### 5.3 A cratera

**O visual aprovado fica:** a v2 da maquete (`bake_craters.py`, `crater_proof`), as cores do Look e as pedras 3D. O
que muda é como ele chega ao pixel.

**1. Os tamanhos ficam:** A 1,95 m, B 2,6 m, C 3,4 m, com 50 cm de fundo e a borda +15 cm em 1,25 R. A física está na
seção 2.4: um pouco maior que o obus de contato, de propósito pela leitura.

**2. A paralaxe só dentro da cratera.** Pros números, usar o próprio fonte do nó da 5.8.
- **Onde ela roda:** antes das camadas, no material do mapa (WorldForge).
  - O deslocamento achado vai pra posição que **todas** as camadas leem (a areia, a terra, o asfalto e a MF).
  - Se rodasse só na MF, o grão da areia de baixo, que lê a posição antes dela, escorregaria sob a cratera.
- **Região:**
  - uma máscara assada no caderno do meio: a cratera, dilatada pela paralaxe máxima (0,94 m) pro raio que entra
    pela borda de perto;
  - o ramo pula todo o resto do chão.
- **Plano de partida:** a crista (+15 cm).
- **Passos:** busca linear com interseção por reta (o Tatarchuk): **12 no Alta, 16 no Ultra, 8 no Média, nenhum no
  Baixa.**
- **Textura:** `SampleLevel` na `GroundCrater` (R16F, cm), com o nível de mip calculado pela distância (não pelo ddx).
- **O resto é lido no ponto achado:** a cor, o AO, o SunLit e as marcas.
- **Sem PDO:**
  - o PDO não chega na sombra (seção 2.5);
  - ele desliga o early-Z;
  - empurra o Landscape Nanite pro raster programável.

**3. A sombra assada:**
- A varredura no rumo do sol (seção 5.2) escreve o SunLit com supersampling, mais o SkyAO.
- A 8°, o fundo de 50 cm faz 3,6 m de sombra: **a bacia inteira fica na sombra e só a parede de perto (virada pro sol)
  acende**. Essa parede, a paralaxe esconde atrás da borda.
- É o "miolo bem escuro" da cratera aprovada, sem pintar.
- Com a câmera girada pra luz de frente, a parede acesa fica à vista, como no mundo real.

**4. Unidade dentro da cratera:** afundar quem passa (o jogo, já pendente). Sem PDO, a sombra da unidade cai no plano.
A cratera tem ~1–2 m de sombra própria, e a diferença é pequena.

**5. A tesselação Nanite fica fora da v3 base.** Ela pode virar um experimento só do Ultra, só na cratera, depois de
entender os buracos do TestSalt2 e sempre com A/B.

### 5.4 O custo e os presets

O quadro de hoje no Alta tem ~31 ms (`OTIMIZACAO.md`). A memória do chão inteira custa ~0,5 ms
(`CHAO_DEFORMAVEL.md`). Tudo abaixo é estimativa, pra 1286 × 724 na GTX 1660.

| Peça | Custo (est.) | Base da conta |
|---|---|---|
| A MF lendo cadernos com mip (~8–10 leituras no lugar de ~32) | **−0,1 a −0,3 ms** | 22 leituras a menos × 0,9 Mpx |
| A normal de iluminação | ~0 | umas 15 instruções |
| As mips do retângulo que mudou | +0,02 a +0,1 ms, só com tanque andando | um compute de redução por mip, no retângulo sujo, ~6 níveis |
| O carimbo: a varredura do sol com supersampling 2 × 2 e o SkyAO | +0,01 a +0,05 ms por quadro carimbando; ~0,05 ms uma vez por cratera | ~35 mil leituras por fita; a cratera C com a sombra, ~0,5 milhão |
| O recentrar do caderno | hoje dá picos (até 262 mil fitas). **Com endereçamento toroidal**, só a faixa nova, distribuída em ~8 quadros: ≤ 0,3 ms por quadro | a faixa de 28 m em 192 m |
| A paralaxe da cratera | < 0,05 ms | ~1.500 px por cratera × 12 passos |
| A RippleLit (troca a leitura da normal da ondulação) | ~0 | o mesmo número de leituras |
| **Total no Alta** | **−0,2 a +0,3 ms** (pode empatar ou ficar mais barato) | medir p50, p95, p99 e máximo |
| Memória de vídeo | as mips dos cadernos que a MF lê: ~+43 MB. SunLit a 4,7 cm num 4096² novo: +85 MB | a sessão do jogo escolhe como empacotar; a 1660 está em 5,1–5,7 GB |

| | Baixa (50%) | Média (58%) | Alta (67%) | Ultra (100%) |
|---|---|---|---|---|
| Caderno de luz | só o do meio (18,75 cm), com mips | detalhe a 9,4 cm | detalhe a 4,7 cm, supersampling 2 × 2 | 4,7 cm, supersampling 4 × 4 |
| Normal de iluminação | sim | sim | sim | sim |
| SkyAO | sim (meio) | sim | sim | sim |
| RippleLit | sim | sim | sim | sim |
| POM da cratera | não | 8 passos | 12 passos | 16 passos |
| Tesselação | não | não | não | experimento, desligado |

### 5.5 O que não fazer (com o motivo)

| Não fazer | Por quê |
|---|---|
| Apagar o detalhe pelo fwidth | perde o tom: o "chão liso" |
| Normal de detalhe menor que o pixel na luz de 8° | ±60% de luz por ±5° de rampa: cintila |
| Escurecer a cor pela sombra | tira o sol e o céu juntos; na sombra das unidades, a marca fica em dobro |
| Light function no sol | não passa de 1, roda no Lumen em compute e na névoa (seção 4.3) |
| Normal da ondulação ou do caderno junto com a luz assada | conta o N·L duas vezes |
| POM na tela inteira | 1–2 ms (Brucks), pra trilha que desloca menos de 1 px |
| PDO | não chega na sombra, desliga o early-Z, raster programável no Nanite |
| Contact shadows | 8 passos na tela, grosso; precisa de PDO |
| RVT no chão | reprovado em 14/10 (borra, achata, estica a pedra) |
| Cone stepping | pré-cálculo O(n²) e sem mip comum: não serve pra caderno que muda |
| Aprovar na maquete | **nada se aprova sem a foto do jogo ao lado, na câmera do jogo** (a lição da v2) |

---

## 6. Os testes de prova

### 6.1 As cenas e as câmeras (no `Game/Config/RTS/Scenarios.json`)

**As câmeras são sempre as do jogo:**
- **normal:** `look_inicial`, 100,4 m, 37,5°;
- **perto:** `look_zoom`, 67 m, 28°.

Cada uma **nos 4 giros** (45°, 135°, 225° e 315°): o sol fica preso pela câmera inicial, e o giro troca contraluz por
luz de frente. **A câmera de 45 m do `look_esteira` não vale pra aprovar**; serve só pra depurar.

| Cena | O quê |
|---|---|
| **`trilha_arcos` (nova)** | 4 MBT em areia aberta na zona ondulada: S, um 8 e um pivô, com trilhas a φ = 0°, 45°, 70° e 90° do sol. Uma trilha velha (4 min) cruzando. Fotos aos 10 s, 60 s e 4 min. Vídeo com a câmera deslizando a 4 m/s. |
| **`areia_aberta` (nova)** | Sem unidades, zonas lisa, pisada e ondulada à vista. 60 quadros parados e o vídeo deslizando. |
| `cratera_perto`, `tiro_chao` (já existem) | As crateras A, B e C a 67 e a 100 m, nos 4 giros: a paralaxe e o lado do sol. |
| `vila` (já existe) | O custo, a mesma cena das medidas de hoje. |

### 6.2 A folha: como comparar quadro a quadro

**Cada prova é uma folha com três colunas, na mesma área de chão e no mesmo pixel:**
1. **O alvo reduzido ao pixel do jogo**, por média em caixa, que é o que um pixel perfeito veria:
   - o game shot ×0,60 pro normal (de 4,4 pra 7,3 cm) e ×0,90 pro perto (4,9 cm);
   - a folha C ×5 e ×3,4 (de ~1,45 cm);
   - a cratera v2 na mesma conta.
2. **O jogo hoje.**
3. **O jogo v3.**

**Por coluna:** 1:1 e ampliado 3× (vizinho mais próximo), rótulo em cima. Mais 3 quadros do vídeo, pra ver em
movimento.

### 6.3 Os números de cada prova

| Prova | Medida (luz linear, onde couber) | Alvo | De onde vem |
|---|---|---|---|
| **O risco do sulco** | perfil perpendicular à trilha, média de 16 px ao longo: mínimo ÷ areia | **0,30–0,45** com φ ≤ 45°, na contraluz; φ = 90° aceito ≥ 0,7 se a faixa ler; na luz de frente, o par invertido | shot: 0,31 |
| A largura do risco | largura a meia altura do risco | **1–2 px** no pixel do jogo | shot: 1–2 px a 4,4 cm |
| A borda clara | máximo ÷ areia | **1,05–1,15** | shot: 1,13 |
| O fundo da faixa | média da faixa ÷ areia | **0,75–0,90** na solta; 1,05–1,15 no pisado | shot: 0,77; folha C: +12% |
| A curva | a largura do sulco no pivô ÷ no reto; a bordinha de fora | ≥ 1,5×; bordinha de fora visível | Liu 2009 |
| **A trilha debaixo da sombra de um tanque** | o risco ÷ a areia, os dois na sombra | ≥ 0,85 (só o céu) | a normal de iluminação |
| **A escala da ondulação** | pico do espectro radial do recorte | **0,6–1,4 m** | shot: 14–20 px a 4,4 cm |
| O contraste da ondulação | desvio ÷ média (valor de tela), zona ondulada e lisa | **0,12–0,20 e ≤ 0,10** | shot: 0,18 e 0,10 |
| **O tom de longe** | média da zona ondulada ÷ lisa, no topo da tela (onde a onda é sub-pixel) | **0,87–0,95, nunca 1,0** | a simulação da seção 1.4 |
| **A cintilação** | desvio temporal por pixel em 60 quadros parados (o TSR mexe); no vídeo, a energia da diferença entre quadros na areia e na trilha | **≤ 1%** parado; no vídeo, ≤ a areia de hoje | TSR (seção 2.1) |
| **A cratera: tons** | brilho e contraste dos 5 recortes da folha aprovada (fundo, parede, borda, chão com pedras, areia), com a aprovada reduzida ao pixel do jogo | brilho ±0,03; contraste ≥ 70% da aprovada (antes de reduzir: 0,12/0,11 · 0,27/0,18 · 0,35/0,17 · 0,42/0,15 · 0,56/0,04) | `2026-10-13_cratera_v2.jpg`, volta 6 |
| A cratera contra a street_battle | p5, p25, p50; nitidez (laplaciano) | 0,09 / 0,27 / 0,36; ≥ 1,0 | `CHAO_DEFORMAVEL.md` |
| **A cratera: paralaxe** | nos 4 giros: a parede de perto escondida; o fundo deslocado ~d ÷ tan(ângulo); o lado aceso segue o sol, não a câmera; o grão da areia não escorrega | os 4 giros certos | geometria |
| O conforto do quadro | quase preto, estourado, sombra/sol | 5–6%; ~3%; 0,24–0,36 | memória "razão sombra/sol" |
| **O custo** | 600 quadros: p50, p95, p99 e máximo; com e sem (`-RTSNoGround`, e uma chave nova `-RTSGroundV3=0`); os passes: NaniteBasePass, "RTSGround Sun", as mips | **≤ +0,3 ms no p50 do Alta; p99 sem os picos do recentrar** | este estudo |

### 6.4 A ordem e quando está pronto

**A ordem**, uma coisa de cada vez, cada passo com a folha:
1. **WorldForge:** a areia v2 no mapa do jogo, com o A/B de conforto. É rápido e já ataca o "chão liso".
2. **Look:** a RippleLit nas duas escalas e a normal de iluminação; o corte por fwidth sai. Prova: `areia_aberta`.
3. **Jogo:**
   - as mips dos cadernos;
   - o SunLit pela varredura e o SkyAO no carimbo;
   - a faixa da altura maior;
   - o perfil novo da trilha (4 cm, a curva, a bordinha de 3 × 20);
   - o endereçamento toroidal;
   - a tesselação desligada de vez.
4. **Look:** a MF nova (sem a média à mão, sem escurecer a cor pela sombra, SunLit na normal de iluminação). Prova:
   `trilha_arcos`.
5. **Look + WorldForge:** o POM da cratera antes das camadas. Prova: `cratera_perto` nos 4 giros.
6. **Jogo:** os presets e a medida de custo.

**Pronto =**
- os números da 6.3 dentro do alvo, nas duas câmeras e nos 4 giros;
- a folha e o vídeo mostrados ao Felipe;
- a palavra dele.

O que ele aprovar vai pro `docs/APROVADOS.md` e pro `docs/RECEITAS_DO_LOOK.md` no mesmo turno.

---

## 7. Por dono

**Look** (o CODE do material e a receita):
- `look_ripples.py`: as duas escalas e a RippleLit por par sol/vento, com mips; o `sand_desert_v3.json` com os números.
- A MF nova (`look_mf.py`):
  - mips no lugar da média à mão;
  - **a normal de iluminação** (seção 4.3) com o SunLit do caderno × a RippleLit;
  - SkyAO no pino AO (multiplica o da areia);
  - sem escurecer a cor pela sombra;
  - a cor da faixa pela zona;
  - as texturas de luz amostradas com o viés de mip anulado.
- O POM da cratera (o código), que o WorldForge põe antes das camadas.
- O limite do claro (~1,15) e o "leque" de azimute, os dois em A/B.
- A medida da cintilação do grão a 8°.

**Jogo** (os cadernos e o C++ que grava):
- `RTSGroundMemory` e `GroundRelief.usf`:
  - mips por compute no retângulo sujo (com amostrador Wrap e fade de ≥ 2^mip texels na borda da janela, por causa
    do endereçamento toroidal);
  - guardar já prontos os produtos que a MF usa (por exemplo, o frescor = faixa × (1 − idade)), porque o produto das
    mips não é a mip do produto;
  - a cobertura das rampas (`MarkRamps`) calculada antes de reduzir;
  - SunLit (N·L × V ÷ sen, com supersampling, escala acima de 1 no formato) pela varredura no rumo do sol, e o SkyAO;
  - a faixa da altura ≥ ±10 cm ou R16F;
  - o perfil v3: sulco de 4 cm, 6–8 cm e mais largo na curva pelo raio da curva; bordinha de 3 × 20 com a de fora
    maior na curva; halo de 50 cm;
  - a garra no SunLit;
  - a máscara da região da cratera no caderno do meio;
  - o refazer distribuído quando o sol muda.
- O `DispOn` desligado de vez na v3 base; a tesselação só como experimento do Ultra.
- A chave `-RTSGroundV3`, os presets, as cenas `trilha_arcos` e `areia_aberta`, e a medida de custo.

**WorldForge** (o material do mapa):
- `SAND=v2` no mapa do jogo, depois do A/B de conforto.
- O CODE novo dentro do material.
- O POM da cratera **antes das camadas**, com o deslocamento passado a todas elas.
- O corte `1 − smoothstep(0.03, 0.06, fw)` sai do `SAND_V2_HLSL`; a normal da ondulação sai; a RippleLit entra.
- Montar sem o `DISP_CODE` (`DISP=0`) e sem PDO.

---

## 8. O que é decisão do Felipe (com a folha na frente)

1. **A ondulação de jogo de 0,7 a 1,4 m nas zonas abertas.** Muda o desenho da folha C (que tinha 22 cm, aprovada a
   ~1 cm por pixel). É física (a megaondulação) e é a escala do game shot, mas é gosto.
2. **A trilha mais funda (4 cm) e as curvas largas,** com a areia empurrada pra fora. Também é física, e muda a C.
3. **A trilha paralela ao sol:** aceitar que o risco some (física) ou "espalhar" o sol na sombra (arte).

---

## 9. A 2ª opinião (14/10): o que mudou no estudo

Uma revisão crítica leu este estudo contra o código do jogo e o fonte da 5.8. O que ela achou e foi conferido:

1. **A light function não passa de 1** (`LightFunctionPixelShader.usf:78–81`), roda também no Lumen e na névoa
   (`VolumetricFog.cpp:1584`). Saiu: a luz assada vai pela **normal de iluminação**, a proposta dela. Isso também
   tirou a reserva pela cor, o corte por altura e a paralaxe repetida.
2. **O carimbo de hoje não aguenta o perfil v3:**
   - a faixa da altura de ±4 cm;
   - o teto de 40 texels da marcha na bacia;
   - a sombra da borda fora da bacia.
   A varredura no rumo do sol resolve os três, e o custo do carimbo, que estava subestimado, cai.
3. **As mips dos cadernos** não saem sozinhas de um alvo escrito por compute (`TextureRenderTarget2D.cpp`: só alocam).
   Precisam de compute próprio, Wrap e cuidado com produtos e rampas.
4. **A areia de baixo escorregaria sob a cratera** se o POM ficasse só na MF: ele vai antes das camadas.
5. **A normal da ondulação junto com a RippleLit contaria a luz duas vezes:** saiu.
6. **A tesselação ainda está ligada no código** (seção 1.3).
7. **Os números:**
   - o topo da tela estava ~10% otimista (corrigido na 1.1 e na 3);
   - o giro da câmera é livre (os 4 giros nas provas);
   - o viés de mip varia por preset;
   - as mips custam ~43 MB.

---

## Apêndice: como medir

**O perfil do sulco:**
- O mesmo do item 1.2. Converter a imagem pra brilho linear: (0,2126 R^2,2 + 0,7152 G^2,2 + 0,0722 B^2,2).
- Achar a direção da trilha. Amostrar perpendicular a ela, de −12 a +12 px em passos de 0,5, cada ponto com a média
  bilinear de 17 amostras ao longo da trilha.
- Pontos medidos no shot (pra repetir):
  - reto: (622,5; 492,5), direção (0,789; −0,614);
  - arcos: (486; 382) e (510; 382), direção (0; 1).

**O espectro da ondulação:**
- O recorte sem média, com janela de Hann 2D e FFT. A potência por anel de frequência; o pico fora do anel mais baixo.
- O recorte do shot: x 790–890, y 500–560.

**A redução ao pixel do jogo:** PIL `resize(..., Image.BOX)` pelo fator da 6.2. Ampliar com `Image.NEAREST` pra ver.

**As simulações da seção 1.4** (um corte em 1D):
- Lambert, com sombra por horizonte e o céu pela visibilidade dos dois lados.
- A tela é a projeção s = x·sen(v) + h·cos(v), com o peso ds/dx.
- A caixa do pixel em várias fases.

Vale refazer na luz medida da receita (a sombra/sol de 8°) antes de fixar os alvos.

---

## 10. O Look em 16/10: o que mediu e o que fez

**A ondulação no game shot, medida na mesma escala** (o recorte `target_tracks_dust_r1_raid_slabs.jpg` tem 1,7 cm/px pelo casco de 7,9 m; folha em `captures/look/textures_v3/` e no rascunho `rip_ab2.jpg`):
- a ondulação do alvo tem **~25–35 cm** (a escala física), **em campos espalhados, irregulares e baixos**, com cristas curvas e muita areia lisa entre eles;
- a nossa contínua (física ou a de jogo) vira "veludo cotelê" em qualquer escala; **a megaondulação de 0,7–1,4 m (a proposta da §5.1) não é o que o alvo mostra**: fica no A/B só como alternativa;
- a proposta do Look: **a física em manchas** (`look_ripples_v3.py`, `patchy`): a altura varia de 0 a ~1,1 cm por campos de 2–5 m (~40% liso), as cristas entortadas ±25 cm em 1–3 m e quebradas.

**As texturas** (`Game/Tools/look_ripples_v3.py`, sol 87°/8°, vento −82,9°, o World_TestSalt2): `T_LL_RippleLit_patch` (a proposta), `_phys` e `_game`, 16 bits, a luz linear ÷ 1,25, 2048² no ladrilho de 10 m (0,49 cm/texel), com a prévia reduzida ao pixel do jogo.

**O material:** `code_v3()` no `Game/Tools/look_mf.py` devolve o CODE com a normal de iluminação (§4.3). O CODE de hoje fica intacto; o WorldForge liga com `GROUNDV3=1`. Entradas novas: `SunLitT`, `SunLitMidT`, `UseSunLit`, `RipLit`, `Lt` (o sol em espaço tangente), `V3On`; saída nova `AOout` (o SkyAO).

**A interface com o jogo** (combinada em 16/10): `GroundSunLit` RG16F 4096² na janela da `GroundTrack` (R = SunLit linear até 1,2; G = SkyAO) e `GroundSunLitMid` 2048²; `-RTSGroundV3` liga `UseSunLit` = 1; o `ReliefRangeCm` vai a 10.

**O mapa:** o WorldForge monta `World_TestSandV2` (o TestSalt2 com `SAND=v2`, nada mais) pro A/B da areia na luz do jogo.

## 11. O jogo em 16/10: a 1ª medida e duas correções

**A 1ª medida da luz assada no jogo** (`captures/game/v3_chao/`, `sol_crop.png`):
- A trilha mostra a faixa escura e a clara, a bacia fica na sombra e a borda acesa (o Look conferiu).
- O SunLit vai de 0 a 1,2 (média 0,98); o SkyAO de 0,67 a 1.
- **O custo foi +1,2 ms** (34,1 × 32,9 ms), acima da estimativa da §5.4.
  - A causa: a cada quadro o jogo refazia um retângulo só, a união de todas as marcas novas da batalha, com 7 m de contexto pra todo lado.
  - **A correção:** uma caixa por fita ou cicatriz nova, juntando as vizinhas (a unida custa até 1,5× as duas).
  - O contexto do lado do sol continua inteiro (`MaxDropCm` 100 cm ÷ tan): a sombra da cratera vizinha cai certa no que mudou.
  - A escrita pro lado da sombra cresce só o quanto a altura mudou (`WriteDropCm`): a trilha ~20 cm (~1,4 m a 8°), a cratera os 100 cm.
  - **Medido:** +0,2 ms de mediana em 2 pares na cena `chao` (33,4 × 33,2 e 33,6 × 33,4 ms); o p95 não piora. Dentro da meta de ≤ +0,3 ms.

**Os torrões fora da sombra do sol** (a decisão (b) do Look):
- **O problema:** a 8°, um torrão de 5 cm faz 35 cm de triângulo preto, 3–5 px na câmera: o "pontinho preto" já reprovado. As pedras ≥ 9 cm já são malha e fazem a sombra delas; a altura do caderno dava uma segunda sombra no mesmo lugar.
- **O que fica na varredura:** a bacia, a borda (+15 cm a 1,25 R, a sombra de ~1 m pra fora), o halo levantado, o perfil da trilha e, depois, a garra.
- **O que sai:** os torrões, os raios e a terra revirada. Ficam só no SkyAO (o escurecimento de contato) e na cor (o claro dos torrões).
- **Como foi feito:**
  - o `bake_craters.py` grava `crater_*_sun.png` (cinza de 16 bits, o arranjo e a faixa do R), com a forma antes dos torrões e o mesmo perfil e borrão da `crater_proof`;
  - o atlas aprovado saiu idêntico, byte a byte;
  - o caderno de altura das crateras passou a RG16F: R a cheia (a MF segue lendo o .r), G a do sol;
  - o `HeightCS` soma os dois; a carcaça não tem a do sol (a altura dela é só torrão);
  - a varredura (o SunLit e o N·L dele) lê a esteira + G; o SkyAO lê a cheia.
- **A prova a 8°**, a altura cheia × só a forma: `captures/game/v3_chao/sombra_torroes_prova.png`.

**Um defeito antigo das crateras que a luz assada mostrou:**
- O `bake_craters.py` borra o X com `np.roll`, que dá a volta: as ~8 colunas das bordas de cada variante trazem a cratera do outro lado (até 15 cm, escuro e claro).
- No caderno, girado, virava uma linha fina na borda do carimbo, desde a v2. Com o sol a 8°, pontilhados de ~2 m paralelos ao sol.
- **Corrigido no carimbo** (`GroundCrater.usf`): a borda vai a zero (zero até 17 cm do lado, inteira a 35 cm). A cratera nunca chega lá, e o atlas aprovado não mudou.
- Antes × depois: `captures/game/v3_chao3/pontilhado_antes_depois.png` e `_b.png`.
- Se o atlas for re-assado um dia, trocar o `np.roll` do `blur` por borda repetida.

**Feito depois, na mesma noite** (o detalhe e os números em `docs/BASE_JOGAVEL.md`, a noite de 16/10):
- **A trilha velha:** uma volta lenta refaz um ladrilho de 512 da luz a cada 0,5 s, com a fila vazia.
- **A curva:** pelo raio, de 30 m a 6 m (ou o pivô), a fita vai a ×1,55 e o sulco a ×1,75; a bordinha de fora dobra e a de dentro some; a curvatura é alisada por 3 m de distância andada. O Look julgou na câmera do jogo: lê bem.
- **O contexto curto:** sem cratera a até 7 m do lado do sol, a varredura da trilha começa só 1,4 m antes. Na cena de trilhas do Look: +0,3 ms (27,0 × 26,7).
- **A garra:** 1 cm de pico a pico, no passo de 19,39 cm, pelo andado da lagarta (um caderno G16 novo), só na luz; some com o frescor². A 8°, ela aparece como dente na borda da sombra do sulco.
- **O halo de 50 cm:** no A do relevo, de 0,5 a 0.
- **As mips:** do SunLit e do relevo, por compute no que mudou, até a mip 6.

**O que falta:** a máscara da cratera no caderno do meio (pro POM), as mips da cratera e o endereçamento toroidal.

### 10.1 16/10: o Felipe aprovou o caminho das trilhas v3 (e a areia 3)

- **"um baita avanço, tá aprovado, pode seguir melhorando a partir dele"**, e pediu pra salvar esta versão contra regressão (`docs/APROVADOS.md`, a imagem em `docs/aprovados/look/2026-10-16_trilhas_v3_aprovadas.jpg`). A prova: `captures/game/v3d_final` (cena `trilha_arcos_groundv3dune`, `-RTSGroundV3`, garra 2 cm, mips), 27,8 ms contra 25,0 hoje na cena das trilhas (~1,5 ms disso é o material da areia nova).
- **O próximo pedido dele: o sulco pelo tipo de chão.** "Cada textura do chão tem um tipo de criação de sulco óbvio":
  - **areia:** macia, empurrada e desabando (o que temos hoje);
  - **terra:** bem marcada, e a graminha fica morta, deitada em cima;
  - **barro:** bem moldado (borda nítida, garra funda e limpa, mais escuro e úmido).
  - O tipo de chão já existe no mapa (o `surface.png` do WorldForge, que a poeira usa). O perfil (a largura, o fundo, a bordinha, a garra, o tempo de enchimento) vira uma tabela por tipo no jogo, e a cor e a rugosidade da faixa por tipo na MF. A graminha deitada pede o espalhamento (o capim) saber onde a trilha passou.

