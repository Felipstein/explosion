# Estudo: a marca quente no casco

Estudo da sessão Fluxo (16/10), com fontes, para o item 7 da fila em `docs/TIRO_E_ACERTO.md`. **[E]** = estimativa ou conta, sem fonte direta.

**Escala da câmera do jogo:** 1 px ≈ 3,3 cm a 67 m. O tanque ocupa ~210 × 110 px.

## O real

**APFSDS que não penetra.** A vareta (Ø ~25 mm [E]) se gasta no choque.
- Na blindagem composta, ela se consome dentro do módulo.
- No aço inclinado ou no ricochete, deixa um sulco. Caso documentado: um T-72 a 400 m acertou a frente de um M1A1, deixou "a groove" e ricocheteou [5].
- A cratera tem 5–8 cm e o sulco 20–50 cm [E].
- Sai uma chuva de faíscas. O urânio empobrecido pega fogo sozinho no ar, o tungstênio não [6].

**HEAT.** O furo é pequeno: 10–13 mm em alvo de polígono [7], ~1,5–3 cm no 120 mm [E]. Em volta fica fuligem e tinta queimada, Ø 40–80 cm [E].

**HE.** Não fura. Deixa fuligem e tinta descascada em 0,6–1,2 m e picadas de estilhaço [E].

**A temperatura**
- O fundo da cratera e o respingo ficam a ~1100–1400 K [E].
- Cores do aço quente [1][2]: 525 °C (Draper) só no escuro, 700 vermelho opaco, 900 cereja, 1000–1100 laranja, 1200 amarelo, ≥1300 branco.
- **O esfriamento** [E: esfera quente no aço, difusividade 6–12 mm²/s [3]]:
  - zona quente de raio 1 cm: de 1250 a 750 °C em 1,5–3 s;
  - zona de raio 2 cm: em 6–12 s.

**De dia o brilho real some em menos de 1 s** [E, conta]. A tinta bege ao sol tem ~11 000 cd/m². O metal quente, comparado a ela:

| Temperatura | Luminância | Em relação à tinta ao sol |
|---|---|---|
| 1300 K | ~500 cd/m² | 4% |
| 1500 K | ~6 000 cd/m² | 55% |
| 1700 K | ~40 000 cd/m² | 3,7× |

Ao sol, só aparece a fração de segundo do clarão e das faíscas. **Os segundos de laranja do jogo são escolha de leitura, não física.**

**A cicatriz** [E]: aço nu cinza-claro no fundo, um anel de tinta queimada marrom-preta e o respingo do penetrador cinza-escuro. Sai uma baforada cinza de tinta queimada por 1–3 s.

## Os jogos
- **World of Tanks** (código do cliente [12]): cada marca guarda o trecho do tiro, o calibre e o tipo; viram decals de GPU na peça. Ficam a partida inteira.
- **War Thunder** [10]: a cicatriz segue o ângulo e o furo segue o calibre; o projétil pode ficar cravado, com chama pelo furo.
- **IL-2** [11]: marca por calibre e material, inclusive de ricochete e de quase-penetração. Há um limite por veículo, e só aparece perto da câmera.
- **Company of Heroes 3** [14]: sem decal por tiro. Regiões do modelo mostram o dano no shader do veículo. É o precedente RTS.
- **Arma 3** [15]: os jogadores reclamam de uma mancha igual para tudo.
- **Nenhum jogo documentado põe brilho na marca:** o brilho fica nas faíscas.

## A técnica no Unreal 5.8

As peças do tanque são malhas soltas, sem receber decal (`SetReceivesDecals(false)`) e sem colisão (`RTSUnitActor.cpp` 175 e 190).

| Método | A favor | Contra |
|---|---|---|
| Decal preso à peça | simples; segue a peça | a caixa projeta em tudo que cruza (casco e torre ao girar, vizinhos); ligar o recebimento faz o chão pintar o tanque; 1000 componentes; emissivo num passe extra |
| Mesh decal | abraça as quinas | não funciona em Nanite [17] |
| Máscara pintada numa textura por UV | marcas ilimitadas | exige UV única; 12,5 MB; picos de ms por acerto [22] |
| **Lista de acertos no material, no espaço da peça** | não vaza; segue a torre de graça; Nanite ok; brilho e cicatriz no mesmo shader; é o método de Looman [21] | até 36 números de dados de primitiva por peça (`SceneTypes.h:38`) |
| Sprite preso à peça | bom pra clarão e faísca | não se molda à chapa |

**Escolha:** a lista de acertos no material. As faíscas e a baforada continuam nos destroços. Custo estimado: ~0,1 ms com 50 tanques [E].

## A receita (para a nossa câmera) [E]

Em tamanho real a cicatriz teria 2–3 px e não se veria.

| Tiro | Centro | Halo de fuligem |
|---|---|---|
| Canhão (APFSDS) | metal nu, Ø 12 cm (4 px) | Ø 40 cm (12 px), escurece a tinta até 0,25 |
| Míssil (HE) | nenhum | Ø 90 cm (27 px), manchado, até 0,2 |

- **O brilho:** cor de corpo negro numa mancha de Ø 10 cm, com bloom.
  - Temperatura: T(t) = 850 + 950·e^(−t/1,0 s) K.
  - Intensidade: I(t) = 8·e^(−t/0,7 s) × o brilho da tinta ao sol. Medida contra o branco da tela (o `EyeAdaptationInverse`), não depende da exposição do mapa.
  - Corta em 3,5 s: 1800 K e 8× em 0 s, 1280 K e 2,5× em 0,8 s, 930 K e 0,2× em 2,5 s.
- **A cicatriz fica a vida do tanque** (como WoT, War Thunder e CoH3) e passa pra carcaça.
- **Limite:** 6 por peça (casco, torre, cano). A mais velha sai primeiro. Um acerto a menos de 25 cm de uma marca reforça essa marca em vez de ocupar vaga.
- **Leitura:** 18 halos de 12 px cobrem ~9% do tanque. É o teto para não poluir a batalha.

## O que ficou combinado no código (16/10)
- **Os dados de primitiva 12–35 de cada peça:** 6 vagas de 4 números.
  - Os três primeiros: a posição do acerto no espaço da malha (cm).
  - O quarto: a hora do acerto (s, o relógio do jogo) + 100 000 × o tipo (0 canhão, 1 míssil).
  - Os slots 0–9 são da sessão do jogo (cor do time, energia, obra, queimado, calor, brasas, dano).
- **Quem grava:** o `UBlastFxDirector` (BlastFx), no instante em que as faíscas do acerto nascem.
- **Quem desenha:** a função de material `MF_HullMarks`, que o `M_UnitDust` do Look chama. O código está em `Game/Tools/hull_marks.py`.

## Fontes
1. [Red heat](https://en.wikipedia.org/wiki/Red_heat)
2. [Draper point](https://en.wikipedia.org/wiki/Draper_point)
3. [Thermal diffusivity](https://en.wikipedia.org/wiki/Thermal_diffusivity)
4. [Pai et al. 2020, temperatura do jato](https://mathnet.ru/eng/fgv686)
5. [GulfLink Tab F](https://gulflink.health.mil/du_ii/du_ii_tabf.htm)
6. [ORAU, penetradores de urânio](https://orau.org/health-physics-museum/collection/consumer/depleted-uranium/penetrators.html)
7. [Armourer's Bench, M3 Grant alvo de polígono](https://armourersbench.com/tag/t24193/)
10. [War Thunder, Dagor 6.0](https://warthunder.com/en/news/6946-development-dagor-engine-6-0-visual-improvements-effects-and-dlss-en)
11. [IL-2 Dev Blog #275](https://il2sturmovik.com/news/616/dev-blog-275/)
12. [wot-src: VehicleStickers.py](https://raw.githubusercontent.com/IzeBerg/wot-src/EU/sources/res/scripts/client/VehicleStickers.py)
14. [NME, CoH3](https://www.nme.com/news/company-of-heroes-3-tanks-will-show-accurate-dirt-and-damage-3210943)
15. [Arma 3, T126939](https://feedback.bistudio.com/T126939)
16. [Kihl, SIGGRAPH 2010](https://www.advances.realtimerendering.com/s2010/Kihl-Destruction%20in%20Frostbite(SIGGRAPH%202010%20Advanced%20RealTime%20Rendering%20Course).pdf)
17. [UE Nanite](https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-virtualized-geometry-in-unreal-engine)
21. [Tom Looman, ferimentos em personagens](https://tomlooman.com/unreal-engine-render-character-wounds/)
22. [80.lv, ferimentos no Unreal](https://80.lv/articles/tutorial-wounds-on-characters-in-unreal)
