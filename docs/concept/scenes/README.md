# Cenas do jogo: o alvo do look, por ambiente e hora

As imagens que mostram como o jogo tem de ficar, separadas por ambiente e hora do dia (10/10/2026).
O look de cada ambiente e hora é montado contra a pasta dele (`docs/GRAFICOS.md`).

**O que vale como alvo:**
- a câmera do jogo: 37,5° de inclinação, 50° de campo, 61 m de altura (`Game/Config/DefaultGame.ini`);
- as unidades na escala de RTS;
- a luz e a cor que deixam a batalha legível.

A câmera deitada (cinematográfica) e as cenas "fantasia" do GPT servem de apoio, nunca de alvo.

**Peças que se combinam, não mapas fechados:** no WorldForge qualquer pessoa monta o ambiente à mão (texturas,
hora do dia, atmosfera), e **qualquer combinação tem de ficar bonita**. Por isso cada cena aqui é referência de
três peças:
- o **kit do bioma**: chão, vegetação, prédios;
- a **hora**: a predefinição de luz;
- a **atmosfera e o clima**.

A "mesma cena" em vários biomas e horas serve pra provar que as peças combinam entre si.
A arquitetura inteira (peças, caminho de cada imagem, onde cada mundo encaixa no WorldForge, decisões) está em `docs/concept/AMBIENTES.md`.

## Deserto, golden hour (`desert_golden_hour/`)

### `best/`: as melhores, as que "têm que ser isso"

| Arquivo | Por que está aqui |
|---|---|
| `day_game_cam.webp` | **O alvo nº 1.** Câmera do jogo, sol baixo e quente, sombras longas, poeira no ar; a cidade de adobe, a USA clara contra o GLA. É a base da maquete do look (`Game/Tools/look_compare.py`). |
| `usa_vs_china_gemini.webp` | **A atmosfera mais forte:** a poeira acesa pelo sol, o chão rachado, a luz contra; a USA clara e a China escura se separam bem. |
| `china_vs_gla_gemini.webp` | **A leitura das facções:** China escura e GLA cor de areia no mesmo chão, cada uma lendo pelo claro e escuro. Luz limpa, sombra nítida. |
| `close_game_cam_gpt.webp` | **De perto, o escolhido pelo Felipe:** o detalhe de tanque, chão e muro. A cor bate com `day_game_cam` (medido). Ignorar o desfoque de miniatura em cima e embaixo; a câmera está mais baixa e mais perto que a do jogo. |
| `close_ground_light_gemini.webp` | **De perto, o alvo de luz e de chão (11/10):** o tanque da USA ao lado de um Dozer (o Dozer é só rascunho, não é alvo de modelo). O Felipe: "parece muito a nossa gameplay final". Pro look, **a luz**: sol baixo, sombra longa e macia, cor desbotada. Pro WorldForge, **o chão**: areia batida com marca de esteira e pneu, seixos e moitas secas espalhados, e a ondulação suave do vento. |

### `more/`: as outras da golden hour (apoio)

| Arquivo | O que é |
|---|---|
| `three_factions_gemini.webp` | As 3 facções juntas na câmera do jogo, com as cores de hoje. A luz é mais lavada que a das melhores. |
| `close_game_cam_gemini.webp` | De perto, a alternativa: a altura de câmera do jogo; o USA com pintura azul grande, fora da regra. |
| `day_tilt_cam.webp` | Câmera deitada (cinematográfica), com sol no horizonte e helicópteros. |
| `usa_vs_china_gpt.webp`, `china_vs_gla_gpt.webp` | As mesmas batalhas no GPT: mais detalhe, mais "fantasia" (saturado e dramático). |
| `game_shot_usa_vs_gla.webp` | A "foto do jogo" antiga (GPT), com fábricas, cor do jogador e explosão. **Não é alvo de luz:** fantasia demais. |

## Deserto, noite (`desert_night/`)

| Arquivo | O que é |
|---|---|
| `night_game_cam.webp` | **A noite aprovada:** luar azul forte com o campo todo legível, mais as poças de luz laranja do fogo. |
| `night_tilt_cam_gemini.webp` | Noite, câmera deitada. |
| `night_tilt_cam_gpt.webp` | Noite, câmera deitada, no GPT (com o tanque chinês favorito do Felipe). |

## Os mundos novos (aprovados em 10/10)

Saíram de prompt curto e livre (a receita da `day_game_cam`), só com as fichas das unidades anexadas.
O padrão: o **Gemini** é o alvo de jogo (`best/`, câmera alta, escala de RTS); o **GPT** é a beleza
e a vista de perto (`more/`, câmera mais baixa, mais "fantasia", "mas também são válidas").
A folha com todos: `captures/concept/worlds_atlas.jpg`.

| Pasta | O que é | O que o Felipe disse |
|---|---|---|
| `forest_golden_hour/` | Campo e vila europeus depois da chuva, igreja de pedra (USA × GLA) | "sensacional" |
| `jungle_golden_hour/` | Selva com rio, palafitas, briga pela ponte (USA × China) | "gostei gostei" |
| `steppe_golden_hour/` | Estepe dourada com campo de petróleo (China × GLA) | "sensacional", mas laranja demais; ver em outras horas. O GLA se mistura no capim. |
| `snow_golden_hour/` | Vila russa, sol baixo de inverno, sombra azul longa (China × GLA) | "sensacional": a neve voando no tiro, a poeira de neve onde o tanque anda |
| `snow_snowfall/` | Neve caindo, vila em chamas, tudo mais cinza (USA × China) | "mais foda ainda": os fogos nas casas |
| `naval_day/` | `naval_island`: assalto anfíbio à ilha (China × GLA); `naval_fleets`: frota contra frota, porta-aviões, encouraçado, submarino (USA × China) | "todas tops": os tiros caindo na água, os barcos atirando |
| `city_modern_noon/` | Cidade moderna viva, parque, ônibus, gente correndo (USA × GLA) | "a cidade viva" |
| `city_europe_after_rain/` | Praça da catedral gótica, pontes de pedra, chão molhado (China × GLA) | aprovada |
| `city_asia_night/` | Avenida de neon sob o metrô elevado, a explosão acendendo o quarteirão (USA × China) | **"que foda essa explosão, os efeitos, a luz, eu quero muito isso rodando!"** |
| `city_port_dusk/` | Porto de contêineres, guindastes, hora azul (USA × GLA) | aprovada |
| `forest_rain/` | Campo europeu sob chuva forte, lama e poças (USA × China) | aprovada; a chuva em risco fino não mata a leitura |
| `jungle_monsoon/` | Monção na selva, estrada vermelha alagada, rio cheio (China × GLA) | aprovada |
| `naval_storm/` | Tempestade no mar, farol, raio, ondas (USA × GLA) | aprovada; a naval mais "de jogo" |
| `special_chaos/` | O caos: refinaria em chamas, chaminé caindo, gás tóxico verde, sol vermelho, as três facções ao mesmo tempo | Gemini: "gameplay mais próxima e realista, até que gostei" |
| `special_base_assault/` | Ataque noturno dentro de uma base da USA (a China arromba o muro) | Só o GPT serve, e ainda não é final ("um pouco mais próximo do que dá pra ser"); o do Gemini virou maquete de brinquedo. A fábrica não é a nossa War Factory aprovada. |

## Ainda sem imagem aprovada (gerar)

| Pasta | O que vai ter |
|---|---|
| `desert_noon/`, `desert_dawn/`, `desert_overcast/`, `desert_sandstorm/` | O deserto em outras horas |

Na fila (`docs/concept/AMBIENTES.md` §6): costa mediterrânea, desfiladeiro, passo alpino com represa,
tundra com lago congelado, refinaria à noite; os climas; as outras horas de cada mundo aprovado.

**Como fazer:**
- Primeiro a câmera do jogo de cada mundo, em prompt curto e livre.
- Depois de aprovada, o resto do kit ancorado nela: a câmera deitada, as facções sob a luz, a vista de perto e a noite, se o mundo tiver noite.
- A versão "mesma cena" (o cruzamento do deserto em outro mundo) só vem depois, com a aprovada dando o estilo e a cena do deserto dando só a posição das coisas.
- As rodadas ficam em `captures/concept/42_environments/`. Só a aprovada vem pra cá.

## Fora daqui (de propósito)

- **As outras rodadas antigas** estão em `captures/concept/` (fora do git): `01_keyframe` a `16_beauty_pass`. Muitas têm a pintura velha das unidades (USA bege, picapes vermelhas).
- **Os biomas e os mapas-mundo** (`captures/concept/27_biomes`, `32_world_targets`) são do WorldForge, não do look.
