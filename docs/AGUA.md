# A água do RTS (estudo e plano do Look, 13/10/2026)

**Estado:** estudo e plano. Nada foi aprovado pelo Felipe ainda. O que ele aprovar vai pro `docs/RECEITAS_DO_LOOK.md`.

**Por que existe:** o Felipe questionou o plano d'água global embaixo do mapa ("não é a solução mais moderna; não fique preso a decisões anteriores").

**A divisão** (WORLDFORGE.md §12.3):
- **O WorldForge** decide onde e qual forma: a malha por corpo, a cota, o leito e a textura `T_Water_<mapa>`.
- **O Look** faz o material.
- **O jogo** troca o material pelo preset e trata a água na simulação.

## O que o material recebe (combinado com o WorldForge)

- **A textura `T_Water_<mapa>`:** RGBA16F, 1 m por texel, na mesma grade UV do chão (LX/LY/LS).
  - R = distância com sinal até a margem em m (+ na água, cortada em ±8);
  - G = profundidade, 0–8 m;
  - BA = fluxo XY em m/s.
- **O Custom Primitive Data de cada corpo:**
  - 0 = cota em m;
  - 1 = tipo (0 lago, 1 oásis, 2 rio, 3 mar);
  - 2 = turbidez;
  - 3 = escala da espuma.
- **Os materiais:** `/RTSLook/Water/M_Water_SLW` (padrão, presets alta e ultra) e `/RTSLook/Water/M_Water_Trans` (irmão translúcido, presets baixa e média), com uma instância por tipo. O jogo troca na carga do mapa. Não dá num material só: o shading model e o blend mode não mudam pela chave de qualidade.

## O custo medido na 1660

A vila da maquete com uma lagoa no centro da câmera do jogo, `look.ps1 perf`: 600 quadros, 1920×1080, Scalability 3, TSR.

| caso | quadro (mediana) | diferença |
|---|---|---|
| sem água | 26,94 ms | — |
| oásis, 15% da tela, Single Layer Water | 27,57 / 27,53 ms | +0,6 ms |
| oásis 15%, translúcida | 27,12 ms | +0,2 ms |
| lago, 40% da tela, SLW | 29,66 ms (base da mesma leva: 28,33) | +1,3 ms (a etapa SingleLayerWater: 0,51 ms) |
| lago 40%, translúcida | 27,50 ms | +0,6 ms (a etapa Translucency: 0,49 ms) |

O ruído entre rodadas fica em ~±0,5 ms. Os números da mesma leva são os confiáveis.

## O estudo

A câmera: 61 m de altura, 37,5° de inclinação, 50° de campo. O centro da tela fica a ~100 m, cerca de 20 px por metro. Com o Fresnel da água (F0 0,02), o reflexo pesa 2% embaixo, 3% no centro e ~10% no alto da tela. **A cor da água vista de cima vem do corpo d'água e do fundo, quase nada do reflexo.**

### 1. Cor pela profundidade

**Os coeficientes do Single Layer Water são por centímetro.** A doc diz "metros recíprocos", mas o shader multiplica pela profundidade em cm (`SingleLayerWaterShading.ush`, l. 221). O exemplo da doc (R 0,0033) é a água pura a 650 nm por cm.

**O caminho da luz é mais longo que a profundidade.** O fundo é atenuado por `exp(−σt·(ΔZ_view + h))` (l. 160 e 206): com essa câmera, cerca de 2,6·h no centro da tela. Regra pra calibrar: pro fundo sumir (~5%) na profundidade D, use σt ≈ 1,1/D por metro.

Água pura (Pope & Fry), por metro: R 0,34, G 0,057, B 0,009.

Pontos de partida por metro (dividir por 100 no SLW). São derivados, não medidos:

| tipo | absorção R,G,B | espalhamento R,G,B | o fundo some (G) em |
|---|---|---|---|
| oásis | 0,40 · 0,07 · 0,04 | 0,03 · 0,06 · 0,06 | ~9 m |
| lago | 0,50 · 0,15 · 0,25 | 0,20 · 0,30 · 0,25 | ~2,5 m |
| rio turvo | 0,8 · 1,0 · 1,6 | 3,0 · 2,6 · 2,0 | ~0,3 m |
| mar | 0,45 · 0,07 · 0,03 | 0,01 · 0,03 · 0,09 | ~9 m |

- A turbidez interpola em escala log entre o limpo e o turvo de cada tipo.
- **Specular 0,25.** O SLW tira o IOR do Specular: 0,25 dá F0 0,02 e IOR 1,33. O padrão 0,5 dá IOR 1,5, errado pra água.
- PhaseG 0,1: o `Water_Material` da Epic tem `Anisotropy` 0,1.
- **Os RTS com fonte pública:**
  - A Supreme Commander (FAF) tinge o terreno submerso por uma rampa 1D indexada pela profundidade. A superfície só soma reflexo e refração.
  - O AoE IV pinta tiles de água com cor, profundidade, tipo de espuma e velocidade, mais um flow map.

### 2. Margem, espuma e areia molhada

**A espuma por diferença de profundidade falha na câmera alta:**
- mede ao longo do raio, então a faixa varia 1,2–2,5× pela tela e muda com o zoom;
- gruda em unidade, ponte e destroço, e não anima a onda chegando (Croakwood).

O campo R do gerador resolve: está em metros, é estável e igual em todos os presets.

- **A espuma da Epic (`BeachFoam`):** distância 2,56 m, escala macro 15,4 m, rugosidade 0,4, limiar e mistura 0,4/0,4, máscara da borda em −24 cm.
- **A receita:** cobertura = 1 em R=0, caindo a 0 em ~2 m; espuma = `saturate((tex·macro − (1−cobertura))/0,4)`. No SLW ela entra em Opacity, com BaseColor 0,6–0,7. A espuma velha reflete ~0,22 (Koepke).
- **A marola:** bandas `frac(R/λ + t/T)`. No lago e no oásis, λ 2–3 m e período 3–4 s, fraca; no mar, λ 6–10 m e 6–9 s. Pra água subir na areia, a malha avança 2–3 m na terra (R<0) e a máscara é R + a subida(t).
- **A areia molhada fica no material do chão, pela altura acima da água** (a função `Wetness` da Epic usa WaterZ − TerrainZ):
  - a capilaridade sobe 12–35 cm em areia média;
  - a largura = subida ÷ declive (1:10 dá ~2 m), com mínimo de 1–2 m por R pra leitura;
  - **os números:** albedo ×0,5 (Epic; Lekner & Dorf), rugosidade ×0,6 no úmido (Lagarde) e 0,03 na película junto da água (Epic), Specular 0,225, saturação um pouco maior;
  - o terreno submerso fica com rugosidade 0,75, pra não dar brilho duplo.
  - **O chão precisa do nível da água numa textura.**

### 3. Fluxo do rio (Vlachos, Portal 2, SIGGRAPH 2010)

- **A técnica:**
  - duas fases defasadas em meio ciclo, com peso triangular;
  - a fase B com UV deslocado em 0,5;
  - ruído de baixa frequência no tempo, pra esconder a pulsação.
- **Os valores:**
  - com offset −0,5 (Catlike Coding), T ≤ 2·tile/(3·|v|);
  - início: tile 6 m e T 1,5 s, que vale até 2,7 m/s;
  - ruído de fase de ~27 m (o rio da Epic usa 27,5 m);
  - força da normal 0,25 + 0,75·|v|/vmax;
  - as normais das duas fases misturadas por whiteout ("Blending in Detail").
- **A espuma do rio:** `smoothstep(0,8; 2,0 m/s; |v|)` × ruído, mais espuma perto de obstáculo via R. Anda com as mesmas fases. O tile da Epic é 5,12 m.

### 4. Normais e ondas

- A 100 m, onda de menos de ~0,4 m some. O que se lê são ondas de 1–10 m, pelo desenho do brilho.
- A Epic usa tile de 5,12 m perto e 35 m longe, com transição de 50 a 150 m. A tela inteira (77–157 m) cai nessa faixa, então as duas escalas entram.
- **Contra a repetição:**
  - escalas que não são múltiplas uma da outra (6,3 m e 23,7 m), em direções 30–60° diferentes;
  - macro-ruído de 80–150 m variando força e rugosidade em ±30%;
  - hex tiling (Mikkelsen 2022) só se ainda repetir.
- **A rugosidade:** 0,05 perto subindo até ~0,15 no alto da tela (Cox–Munk: vento de 2 m/s daria ~0,34 sem as normais resolvendo a onda).
- **Onda na malha não vale a pena:** 0,1–0,5 m viram 2–10 px. Malha plana, e a marola no shader.

### 5. Reflexo

- `r.Water.SingleLayer.Reflection`: 1 = Lumen, 2 = captures + céu, 3 = SSR. O Lumen reflete o SLW como espelho. Os raios sobem 23–52°: quase só céu e o que é alto na margem.
- **O brilho do sol:** como espelho, só cai na tela com o sol entre 23° e 52° de elevação e até ±25° do azimute da câmera, e as ondas espalham ±15–30°. **Brilho cobrindo unidade é defeito:** pôr teto no especular do sol e medir.
- **O preset baixo:** translúcida em Surface ForwardShading, com skylight e captures, sem SSR (o caminho da SupCom: cubemap do céu + especular do sol + Fresnel estilizado).

### 6. Desempenho

- **As alavancas:** `RefractionDownsampleFactor` 2 no médio e alto; SSR no alto e Lumen só no ultra; `Refraction.Culling`.
- **O orçamento:** ~8 leituras de textura (o dado do gerador, 2×2 normais, 1–2 de espuma, o macro).
- **Baixa e média:** sem SceneDepth nem SceneColor. A opacidade = `1 − exp(−σt·2,6·G)` pelo G do gerador, e o fundo tingido no material do chão.

## O plano do material (a montar)

1. Specular 0,25; coeficientes por cm calibrados por σt ≈ 1,1/D; um conjunto por tipo; turbidez em log.
2. Margem, espuma, marola e máscara da borda só pelo R.
3. Areia molhada no material do chão (`look_mf.py`, avisando o WorldForge antes), pela altura acima da água, com mínimo por R.
4. Normais de 6,3 m e 23,7 m, macro de 120 m, rugosidade subindo com a distância.
5. Rio em duas fases: T 1,5 s, offset −0,5, ruído de fase de 27 m, espuma pela velocidade.
6. Malha plana, marola no shader.
7. Reflexo: Lumen no ultra, SSR no alto, céu + sol no resto, com teto no brilho do sol.
8. Provar na maquete (o oásis de teste da `look.ps1`, variantes WaterSLW e WaterTrans) e fazer o A/B na luz do jogo com o Felipe.

## Fontes

- **O fonte da Unreal 5.8 (local):**
  - `Engine/Shaders/Private/SingleLayerWaterShading.ush`
  - `Engine/Source/Runtime/Renderer/Private/SingleLayerWaterRendering.cpp`
  - `Engine/Shaders/Private/WaterInfoTextureMerge.usf`
  - os materiais do plugin Water: `Water_Material`, `WaterTextureSurface`, `Wetness`, `WaterRiverFlowmaps`, `BeachFoam`
- **A Epic:**
  - SLW: https://dev.epicgames.com/documentation/en-us/unreal-engine/single-layer-water-shading-model-in-unreal-engine
  - Lumen: https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-global-illumination-and-reflections-in-unreal-engine
- **A ótica da água e da espuma:**
  - Pope & Fry: https://omlc.org/spectra/water/data/pope97.txt
  - Cox–Munk: https://oceanopticsbook.info/view/surfaces/cox-munk-sea-surface-slope-statistics
  - Koepke (espuma): https://frouingroup.ucsd.edu/Aw_Chl/REFRENCES/Frouin_2000GL012657.pdf
  - Secchi: https://www.jlimnol.it/jlimnol/article/download/jlimnol.2009.83/160/319
- **O fluxo:**
  - Vlachos 2010: https://www.advances.realtimerendering.com/s2010/Vlachos-Waterflow(SIGGRAPH%202010%20Advanced%20RealTime%20Rendering%20Course).pdf
  - Catlike Coding: https://catlikecoding.com/unity/tutorials/flow/texture-distortion/ e https://catlikecoding.com/unity/tutorials/flow/directional-flow/
- **A superfície molhada:**
  - Lagarde: https://seblagarde.wordpress.com/2013/04/14/water-drop-3b-physically-based-wet-surfaces/
  - Lekner & Dorf: https://www.wgtn.ac.nz/scps/staff/pdf/darkerwhenwet.pdf
  - Zhang & Voss: https://web3.physics.miami.edu/~voss/ken/RefPapers/056_ZV_AO_2006.pdf
- **A textura:**
  - hex tiling (Mikkelsen 2022): https://jcgt.org/published/0011/03/05/paper.pdf
  - Blending in Detail: https://blog.selfshadow.com/publications/blending-in-detail/
- **Os jogos:**
  - Supreme Commander (FAF): https://raw.githubusercontent.com/FAForever/fa/develop/effects/water2.fx
  - AoE IV: https://support.ageofempires.com/hc/en-us/articles/6588308453396-Ultimate-Water-Guide
  - Croakwood: https://www.texelraptor.com/blog/croakwood-devlog-5
