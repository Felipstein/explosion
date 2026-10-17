# Estudo: o fogo e a fumaça da carcaça em volume (item 9)

Estudo da sessão Fluxo (17/10, madrugada), com fontes. É o item 9 da fila em `docs/TIRO_E_ACERTO.md`; o Felipe chamou a versão atual de "mixuruca". A regra do CLAUDE.md vale inteira: volume, sombra do mapa, sombra no chão, luz das explosões.
- **[E]** = estimativa.

## O que tem hoje e por que não serve
- **O fogo** (`BlastDebris.cpp`, `StartWreckFire`): até 14 bolinhas-sprite de 0,22–0,38 m, que vivem 0,5–0,8 s.
- **A fumaça:** uma fita `M_TrailSmoke` de 5 s.
- **A luz:** duas senoides, 2,1 e 4,9 Hz.
- Nada disso tem volume nem projeta sombra.
- A tentativa em raymarch de 13/10 foi reprovada (`APROVADOS.md`): "2 efeitos", "retângulo", rápida demais, fumaça creme e coluna tapando unidades.

## O alvo
**`street_battle_usa_china_gemini.webp`** (os game shots escolhidos mostram a explosão, não a carcaça):
- o fogo é baixo, até 1/3 do comprimento do veículo, saindo das aberturas; de cima é uma mancha quente com brilho laranja no casco e no chão;
- a fumaça é cinza-carvão neutra, de base estreita, densa só nos primeiros metros e translúcida depois;
- some a 2–3 comprimentos de veículo de altura, inclinada 10–25°.

**O vídeo de referência do Felipe:** chama da altura do carro, couve-flor preta com o topo aceso pelo sol, ~1 pulso por segundo.

## A física
| Grandeza | Valor | Fonte |
|---|---|---|
| Potência de um tanque queimando (diesel/JP-8, hidráulico, borracha) | [E] 5–15 MW; carro: 3–9 MW de pico | relatórios de túnel ([KPT](https://www.kennisplatformtunnelveiligheid.nl/wp-content/uploads/2025/01/Design-Fires-for-Vehicles-in-Road-Tunnels.pdf)) |
| Altura da chama, anel da torre (D ≈ 2,3 m) | ~4–5 m | Heskestad, L = 0,235·Q^0,4 − 1,02·D ([AIDIC](https://www.aidic.it/cet/12/26/061.pdf)) |
| Altura da chama, escotilha (D 0,8 m) | ~3 m | Heskestad |
| Altura da chama nos alvos (depois do pico) | 1,5–3 m | alvos |
| Pulsação | f ≈ 1,5/√D: ~1,0 Hz no anel, ~1,7 Hz na escotilha | [revisão de Loughborough](https://repository.lboro.ac.uk/articles/journal_contribution/A_review_of_research_and_experimental_study_on_the_pulsation_of_buoyant_diffusion_flames_and_pool_fires/9547682) |
| Subida da coluna | [E] ~6–7 m/s a 10 m | — |
| Fumaça no vento | a 4 m/s ela deita (sobe ~9 m em 20 m de deriva) | Briggs ([PNNL](https://mepas.pnnl.gov/mepas/formulations/air/4.0/4_11.html)) |
| Vento nos alvos | quase parado, ≤ 1–1,5 m/s | alvos |
| Fuligem produzida | 0,04–0,06 kg/kg no líquido; 0,1–0,2 na borracha | [BRANZ](https://www.branz.co.nz/documents/737/SR185_Soot_yield_values_for_modelling_purposes_-_residential_occupancies.pdf) |
| Albedo da fuligem | ~0,2–0,3: preta, o sol a deixa cinza | [Bond & Bergstrom](https://www.cfa.harvard.edu/hitran/HITRAN2012/Aerosols/papers/bond_aerscitech_2006.pdf) |

**Os 30 s de jogo** (na vida real queima horas): cresce em 3–5 s, fica estável, e nos últimos ~5 s a chama vira brasa e a fumaça clareia e descola do casco.

## Como os AAA fazem
- **O comum do mercado:** flipbook do EmberGen ou Houdini com six-way lighting. É chapado, e a nossa regra proíbe.
- **O topo de hoje é volume de verdade**, a mesma família do nosso raymarch:
  - "Smolder" da IOI (007 First Light, [SIGGRAPH 2026](https://advances.realtimerendering.com/s2026/content/AlexanderMueller_Smolder_Siggraph26.pdf)): raymarch de simulações gravadas, com sombra de todas as luzes;
  - a fumaça em voxel do CS2;
  - o [Frostbite](https://www.ea.com/news/physically-based-unified-volumetric-rendering-in-frostbite).
- **Loop sem costura** ([JangaFX](https://forums.jangafx.com/t/high-scale-looping-simulations/1633)): fonte e vento constantes, janela maior que o tempo de travessia da coluna, emenda curta e a ignição como trecho à parte.

## O plano
1. **Uma simulação no Mantaflow** (`BlastLab/Tools/blender_pyro_wreck.py`):
   - o casco como obstáculo (8,1 × 3,8 × 1,8 m);
   - combustível no anel da torre (70%) e na grade do motor (30%), com a emissão modulada por ruído;
   - vento de 1–1,5 m/s;
   - domínio ~16 × 10 × 24 m a 12–15 cm por voxel;
   - a escala de tempo calibrada pela pulsação (~1 Hz) e pela subida, não a olho.
2. **A linha do tempo:**
   - nasce ~1,5 s depois da morte, dentro da fumaça dela, pra não virar "2 efeitos";
   - toca crua até a janela do loop (8–10 s, emenda de 1–1,5 s);
   - pra apagar, sai do loop na passagem seguinte e toca o fim (o combustível caindo a zero, ~6 s).
   - Pede uma extensão no `BakeTime()`: começo, loop e fim.
3. **Memória:** ≤ 300 MB, a 10 qps (o raymarch interpola pela velocidade), 128³–160³, o topo se desfazendo.
4. **Custo:**
   - meta [E] ≤ 0,35 ms por carcaça no Alta;
   - carcaças em volume cheio por preset: Ultra 6, Alta 4, Média 2.
5. **Luz:** a emissão e o centro da chama de cada quadro viram uma luz da `RTSSceneLights` (~2000 K, raio ~15 m), piscando com a gravação, sem senoide. O fogo acende a própria fumaça a no máximo 2%: mais deixou a coluna creme em 13/10.

## Jogabilidade (a regra do CLAUDE.md: efeito que tapa unidade é defeito)
- **Densidade:** densa (τ > 1) só até ~6 m de altura e ~4 m de raio. A transmitância fica ≥ 0,5 acima de 8 m e ≥ 0,75 acima de 15 m, e a coluna some até 20–25 m.
- **A inclinação pesa:** na câmera do jogo, uma coluna de 20 m tapa o chão atrás da carcaça assim:

  | Coluna de 20 m | Chão tapado |
  |---|---|
  | vertical | 26 m |
  | inclinada 25° pra longe da câmera (como nos alvos) | 32 m |
  | inclinada 25° pra câmera | 15 m |

  **Decisão do Felipe:** pra que lado a fumaça das carcaças deita.
- **A sombra da coluna no chão** tira no máximo ~40% do sol fora do pé.

## Riscos
- memória e tempo de gravação;
- fantasma na emenda do topo;
- custo com 6 carcaças;
- a física cortada pode parecer rala (A/B com ele);
- a emenda com a explosão;
- fumaça creme;
- o Blender 5.2 não grava o combustível no cache (a chama vem da temperatura);
- colunas repetidas (2 variantes, relógio defasado, escala ±10%).

## Fontes
Pulsação: [arXiv 1803.10400](https://arxiv.org/pdf/1803.10400). Inclinação da chama: [Welker–Sliepcevich](https://apps.dtic.mil/sti/html/tr/AD0474283/index.html). Fuligem: [NIST](https://www.nist.gov/publications/measurement-visible-and-near-infrared-optical-properties-soot-produced-laminar-flames). As colunas tapando unidades no WARNO: [Steam](https://steamcommunity.com/app/1611600/discussions/0/3415432674404556793). O resto está nas tabelas acima.
