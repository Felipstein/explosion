# Gráficos: o look do jogo (começo da sessão de gráficos, 09/10/2026)

Documento de entrada da sessão de **gráficos**, escrito pela sessão de arte a pedido do Felipe.
O Felipe é o juiz de cada rodada.

## 1. O papel

**É seu:** o look do quadro inteiro.
- Luz: sol, céu, luz do céu, névoa e atmosfera.
- Exposição, curva de cor (tonemapper) e pós-processamento: bloom, exposição local, cor.
- O padrão de material do terreno e das unidades, pra combinar com a luz.

**A meta:** o jogo idêntico aos game shots aprovados, em cada hora do dia e em cada tipo de mapa.

**Não é seu:**
- explosões, que as sessões de explosão ainda estão fazendo;
- geração de mapa (WorldForge);
- código de jogabilidade (sessão do jogo);
- modelos 3D (o Felipe).

**O fim do trabalho, um padrão só reutilizado sem dor de cabeça:**
1. Montar o look uma vez, num cenário de referência.
2. Empacotar como um **plugin comum** usado por três:
   - o **jogo** (`Game/`);
   - o botão **"ver na versão final"** do WorldForge, que mostra o mapa com os gráficos do jogo sem abrir o jogo;
   - o **simulador de explosões** (BlastLab).

O **painel de montar do WorldForge continua com a luz leve de hoje**, nada realista. Não mexer nele.

## 2. As referências (`docs/concept/scenes/`)

**Só as do Gemini** (decisão do Felipe, 09/10): as do GPT são "fantasia" demais, saturadas e
dramáticas. Exceção: o game shot de perto, em que ele escolheu o do GPT ("gosto muito dos gráficos
do GPT, mas é bem fantasia; vamos tentar GPT"), com o do Gemini guardado. A "foto do jogo" antiga
(`game_shot_usa_vs_gla.webp`, GPT) **não** é alvo de luz. **Mapas-mundo e a gameplay de hoje ficam
fora**, pra não confundir: o foco é o look.

**As cenas ficam em pastas por ambiente e hora (10/10):**
- `desert_golden_hour/best/`: as melhores, as que "têm que ser isso";
- `desert_golden_hour/more/`: as outras da golden hour;
- `desert_night/`;
- as pastas das horas e ambientes que ainda não têm imagem.

O guia está em `docs/concept/scenes/README.md`.

| Arquivo | O que é |
|---|---|
| `desert_golden_hour/best/` (as sete escolhidas pelo Felipe em 11/10, ver `scenes/README.md`) | **Dia, golden hour: o alvo de beleza e de gameplay.** A `day_game_cam` fica como base medida da maquete, não como a favorita |
| `desert_golden_hour/best/day_tilt_cam.webp` | Dia, câmera deitada (cinematográfica) |
| `desert_golden_hour/best/close_game_cam_gpt.webp` | **De perto, o escolhido:** tanque, chão e luz em detalhe. A cor bate com o dia aprovado (saturação 0,49 × 0,50, brilho 0,38 × 0,38, contraste 0,18 × 0,18). **Ignorar o desfoque em cima e embaixo** (efeito de miniatura, não existe na câmera de jogo); a câmera está um pouco mais baixa e mais perto que a do jogo |
| `desert_golden_hour/best/close_ground_light_gemini.webp` | **De perto, a luz do chão (11/10):** o Felipe disse que "parece muito a nossa gameplay final objetiva". Vale a **luz**: sol baixo e macio, sombra longa, cor desbotada e quente sem saturar, o tanque claro lendo na areia com a lona azul, nada de névoa nas unidades. O Dozer é rascunho, não é alvo de modelo. Aqui o sol vem de trás e da direita da câmera; o lado do sol do mapa continua o da `day_game_cam` |
| `desert_golden_hour/more/close_game_cam_gemini.webp` | De perto, a alternativa guardada: a altura de câmera do jogo; o USA saiu com pintura azul grande (fora da regra) e menos detalhe |
| `desert_night/night_game_cam.webp` | **Noite aprovada:** luar azul forte deixando o campo todo legível, mais as poças de luz laranja do fogo |
| `desert_night/night_tilt_cam_gemini.webp` | Noite, câmera deitada |
| `desert_golden_hour/best/usa_vs_china_gemini.webp`, `desert_golden_hour/best/china_vs_gla_gemini.webp` | **Leitura das facções sob a luz** |

Também servem de referência:
- `docs/concept/towns/`: todas as casas e cidades recortadas das cenas;
- `docs/concept/buildings/`: War Factory e construções civis;
- `docs/concept/units/`: fichas dos tanques com a cor do jogador.

## 3. O que já está decidido sobre a luz (não reabrir sem o Felipe)

- **O dia é golden hour:** sol baixo e quente, sombra longa, poeira no ar, "luz cinematográfica,
  look de Unreal Engine 5". Tirar isso por "cores naturais" achatou a luz (contraste −20%, medido).
- **A noite é legível:** luar azul forte com o campo todo visível, mais as poças de luz do fogo.
  A noite "realista" escura engole as unidades e foi rejeitada.
- **Cores desbotadas e empoeiradas, nunca saturadas.** O chão e os prédios ficam mais calmos que
  as unidades.
- **As facções se leem pelo claro e escuro:**
  - USA cinza-claro com azul; China oliva escuro; GLA amarelo-areia;
  - medido: cidade cinza apaga a USA e deserto apaga o GLA (ponto fraco aceito; a interface ajuda);
  - a luz e a cor não podem destruir esse contraste.
- **Jogabilidade antes do realismo:**
  - **Câmera do jogo** (`Game/Config/DefaultGame.ini`): 37,5° de inclinação, 45° de giro, 50° de campo e 61,1 m de altura (de 31,6 a 81,6). O modo cinematográfico deita até 12°.
  - **Sombra:** o sol do WorldForge está a 20°, com sombra de ~2,7× a altura. Ela desenha o relevo sem cobrir unidades.
  - **Névoa, atmosfera e bloom:** não podem cobrir nem lavar as unidades.
  - Na câmera alta, o halo de ofuscamento atravessa o chão; o jogo já o tirou.
- **Desempenho:** o Ultra é o máximo, mesmo travando na 1660. Cada preset (baixa/média/alta/ultra)
  segura ~60 qps na placa que o escolhe. Todo número sai de medida.

## 4. O estado de hoje: 3 luzes diferentes

| Projeto | A luz de hoje |
|---|---|
| **BlastLab** | O porte do WebGL (`BlastLabEnvironment`): sol de 128 000 lux por hora, céu e névoa por tabela, exposição e curva de cor do WebGL (`M_TonemapWebGL`). O céu e a névoa aprovados vêm de um **bug aprovado** do WebGL, reproduzido por tabela; não "corrigir" sem o Felipe. |
| **WorldForge** (botão de qualidade real) | Luz física por bioma (`WorldForge/Tools/build_slice.py`): sol de 120 000 lux a 20°, `SkyAtmosphere`, luz do céu em captura ao vivo, névoa, exposição travada no EV100 físico (~13,6 no deserto), curva Filmic, bloom 0,25 e exposição local 0,6/0,9. |
| **Jogo** | A luz do mapa do WorldForge, com a curva padrão da Unreal. |

**A consequência:** a explosão aprovada no BlastLab sai no jogo como uma **nuvem branca estourada**,
porque a curva de cor do jogo é outra (`docs/JOGO.md`, teste t4).

Quando o plugin for pro BlastLab, as explosões aprovadas têm de ser comparadas de novo com a
versão aprovada. O que já foi aprovado não pode piorar.

O efeito do FXLab (o fogo da carcaça) já lê a luz do mapa (`FXLab/Source/FXLab/Public/FXSceneLight.h`):
- o sol depois da atmosfera e o céu da luz do céu, direto no material;
- o fogo multiplicado por 1/exposição.

É um exemplo de efeito que se adapta a qualquer luz.

## 5. O que falta de referência (o Felipe decide se gera)

Os mundos além do deserto (neve, floresta, selva, estepe, naval, cidades), as horas e os climas estão em
`docs/concept/AMBIENTES.md`: cada pasta de cena vira uma receita de luz com o mesmo nome.

- **Outras horas:** só existem golden hour e noite. Faltam meio-dia, amanhecer, nublado,
  tempestade de areia e chuva, se o jogo for tê-las.
- **Neve e temperado de perto,** na câmera do jogo, com unidades.
- **Interface sobre a luz** (HUD, minimapa, névoa de guerra): a sessão de arte vai gerar.

Quando precisar de referência nova, o Felipe chama a sessão de arte.

## 6. Como trabalhar

- **A placa é uma só (GTX 1660):** toda Unreal com janela ou render entra na fila (`docs/GPU.md`).
- **O WorldForge é só leitura:** pedidos à sessão dele por mensagem.
- **O BlastLab é da sessão "Fluxo de explosão realista AAA".** Combine antes de mexer.
- **O `Game/` é dividido com a sessão do jogo:** combine quais arquivos de luz e pós são seus antes
  de editar.
- **A prova:**
  - a mesma câmera de cada referência, lado a lado com ela, em imagem;
  - visto em movimento (vídeo), não só em foto;
  - o Felipe julga.
- **Pesquise antes de mudar o visual:** como os jogos AAA e a própria Unreal fazem, um plano curto
  com as fontes e só então código.
- **Commit só com o "aprovado" do Felipe.**
- **A lista viva da base jogável** é `docs/BASE_JOGAVEL.md`: marque lá o que tocar.

## 7. A maquete do look (LookLab, desde 10/10/2026)

Cada cena-alvo vira uma **maquete na Unreal, na mesma câmera da arte**, pra ajustar a luz contra o
alvo sem chute. Tudo é gerado por script (`Game/Content/LookLab/` fica fora do git):

| Comando (`Game/Tools/look.ps1 … [-Scene <cena>]`) | O que faz |
|---|---|
| `houses` | casas provisórias de adobe no Blender (quina dura): `look_houses.py` |
| `tank` | os provisórios no Blender (`look_tank.py`): o tanque USA detalhado, o carro queimado, a picape técnica do GLA, o paraquedas do sinalizador e as miudezas (caixotes, tambores, sacos de areia, munição); `LOOK_TANK_ONLY=props` refaz só uma parte |
| `ground` | o asfalto rachado (`look_textures.py`), as texturas do fogo e da fumaça (`look_fx_textures.py`) e o chão em relevo + planta pintada + marcas de esteira + espalhamento (`look_ground.py`) |
| `build` | a cena na Unreal (`look_lab.py`): chão, casas, veículos, luz da receita, câmeras, sequências |
| `iterate <variante>` | a volta de ajuste: monta só essa receita, render rápido na câmera da arte e os números |
| `render <variante>` | as três câmeras (a da arte, a do jogo a 61 m e o zoom perto a 32 m) |
| `perf <variante> [-Preset alta\|ultra]` | o tempo de GPU na câmera do jogo (`look_perf.py`) |

- **A planta de cada cena** fica num arquivo só: `look_layout.py` (deserto dourado) e
  `look_layout_night.py` (noite), escolhidos por `LOOK_SCENE` (`look_scene.py`). A câmera de cada
  arte foi **medida pela perspectiva** (cantos de telhado em ângulo reto, pontos de fuga) e a escala
  pelo casco do tanque (7,9 m):
  - dia: 29° de inclinação, 65° de campo, 26,25 m;
  - noite: 38°, 66°, 31 m (a câmera do jogo no zoom perto, com a lente mais aberta).
- **A luz é uma receita por ambiente e hora** (`Game/Tools/look_recipes/<pasta das cenas>.json`):
  sol (altura e lado em relação à câmera inicial), atmosfera e poeira, luz do céu, névoa, exposição
  e cor, e o **pó do mundo** (a cor e a quantidade de pó/terra nas unidades). A mesma receita vai
  pro plugin comum.
- **A regra de RTS da poeira:** a névoa e a perspectiva aérea começam numa proporção da distância
  até o ponto que a câmera olha; o foco fica limpo em qualquer zoom.
- **A prova é o ponto a ponto** (`look_match.py`): a planta projetada na câmera diz, na arte e no
  render, onde está cada material (asfalto e areia no sol e na sombra, telhado, as duas paredes,
  teto e lado do tanque, a névoa do fundo). Compara a cor (ΔE no L*a*b*: < 5 é igual a olho), a
  curva de tom (percentis 1–99) e a cor das sombras, dos meios e das luzes.
- **Modelos provisórios** (casas, tanque, carcaça) são da sessão de gráficos, só pra a luz ter onde
  bater; os finais são do Felipe e passam pelo mesmo ponto a ponto.
- **Os efeitos da noite são de look-dev** (só pra a maquete ter a luz e a cor da arte; o efeito final é
  das sessões de explosão), feitos do jeito dos jogos (11/10):
  - a fumaça é um cartão virado pra câmera com um tufo desenhado e o relevo dele na textura
    (`T_LL_Puff`, o "normal-mapped smoke sprite" de Killzone/Battlefield; o 6-way lightmap da
    Unity/CoD é o mesmo princípio), translúcida e acesa por pixel pela luz da cena: o fogo embaixo
    acende o lado de baixo, a lua o de cima;
  - o fogo é a temperatura em cinza (`T_LL_Flame`: línguas de fogo e bola de fogo) pintada pela rampa
    do corpo negro, aditiva (o "temperature map" que o EmberGen exporta);
  - a conta da textura usa a direita e o alto da tela, não o UV da malha: o desenho e a luz não
    dependem de como o cartão foi girado.
- **A comparação de perto** (`Close`, receita `desert_golden_close`): o mesmo deserto dourado com o
  sol na posição da arte `close_game_cam_gpt` (na frente e à esquerda, baixo) e a câmera na própria
  receita (`camera`), pra ver os modelos de perto na luz da arte. O mapa do jogo continua com o sol
  do `desert_golden_hour`.

### O estado em 11/10/2026

| Cena | Ponto a ponto (ΔE por material, < 5 = igual a olho) | Curva p5/p50/p95 (arte → maquete) | GPU no jogo, 1080p, preset alta |
|---|---|---|---|
| Deserto dourado | 0,7 a 6,2 (pior: a parede de lado) | 0,09/0,34/0,69 → 0,07/0,37/0,67 | 29,6 ms (Lumen 6,8, TSR 4,9, luzes 3,9, névoa volumétrica 2,0) |
| Noite | 3,6 a 6,5 nas superfícies ao luar; o "chão no sol" da arte é o chão aceso pelo fogo e pela explosão (efeito, fora da maquete) | 0,06/0,23/0,49 → 0,06/0,22/0,42 | 28,3 ms (luzes ~6: as luzes de fogo com sombra) |

- **A crítica da sessão de arte (11/10) mudou três coisas grandes:** o sol do dourado estava espelhado
  (agora é contraluz de três-quartos, na frente e à direita da câmera da arte, sombras caindo pra baixo e pra
  esquerda); a areia da noite estava clara como neve (agora escura, e a USA areia é a coisa clara); o pó do
  mundo virou gradiente de baixo pra cima, sem apagar a cor da facção.
- **A noite:** lua fraca e quase branca (28 klux, 245/240/232) e céu azul forte (a luz do céu 12×, 150/176/235):
  o azul vem da sombra; o chão à noite é cinza-azulado no material (a areia laranja virava roxo ou neve);
  a USA areia fica bege e clara, a China oliva escura; o fogo é poça pequena laranja.
- **O desempenho ainda não está nos 60 qps** (16,7 ms) no preset alta: é o próximo trabalho do look
  (Lumen e TSR por preset, sombra só nas luzes que importam), sempre medido com `look.ps1 perf`.

## 8. O plugin RTSLook (proposta de 11/10, combinada com a sessão do jogo; código depois da noite)

O look sai da maquete e vai pro jogo, pro "ver na versão final" do WorldForge e pro BlastLab por um
plugin de runtime. A sessão de gráficos escreve; a do jogo revisa antes de entrar.

**Arquivos novos (meus):** `Game/Plugins/RTSLook/`
- `RTSLook.uplugin` (módulo de runtime `RTSLook`);
- `Public/LookRecipe.h`: `ULookRecipe` (DataAsset) com as estruturas da receita, espelho do JSON:
  sol (altura, lado em relação à câmera inicial, lux, cor ou temperatura, ângulo), atmosfera, luz do
  céu, névoa (densidade, queda, começo por proporção, volumétrica), pós (EV100 travado, curva, ganhos,
  vinheta) e o pó do mundo;
- `Public/LookDirector.h` + `Private/LookDirector.cpp`: `ALookDirector`, que monta e mantém sol,
  `SkyAtmosphere`, `SkyLight`, névoa e pós ilimitado a partir da receita (o mesmo que o `environment()`
  do `look_lab.py` faz em Python). A cada quadro ele põe o começo da névoa, da névoa volumétrica e da
  perspectiva aérea na proporção da distância da câmera ao foco (a regra de RTS);
- `Public/LookCamera.h`: a interface `ILookCamera` (`GetLookFocusDistance()`, `GetLookYaw()`): a
  câmera do jogo implementa e o plugin não depende do módulo do jogo;
- `Content/`: o material de pó das unidades (`M_UnitDust`, com o slot Team recebendo a cor do
  jogador por Custom Primitive Data) e as receitas convertidas (`DA_Look_<cena>`);
- `Game/Tools/look_recipe_import.py`: o JSON (`Game/Tools/look_recipes/`, que continua sendo a fonte)
  vira o DataAsset.

**Do lado do jogo (da sessão do jogo):** ligar o plugin no `RTSGame.uproject`; o GameMode cria o
diretor com o nome da receita do mapa; o `ARTSCameraPawn` implementa `ILookCamera`. A passada da névoa
de guerra continua antes das explosões e do pós; a foto do minimapa é com luz chapada, sem pós.

**Presets:** cada receita pode trazer `presets.{baixa,media,alta,ultra}` com o que muda (Lumen ou
não, névoa volumétrica, sombras das luzes locais, resolução do TSR). Meta combinada: o look em ~8 ms
no alta, na 1660 a 1080p, medido no mapa do jogo com 50 tanques.

**Revisão da sessão do jogo (11/10), já valendo pro código:**
- Custom Primitive Data das unidades e prédios: 0..3 = cor do time (RGBA, A = 1, `ARTSUnitActor::SetPlayerColor`),
  4 = energia (luzes dos prédios), 5 = altura da obra (m), **6 = queimado** (0..1: o `M_UnitDust` escurece e
  tira o brilho; o jogo põe 1 quando a unidade morre, no lugar das instâncias dinâmicas de hoje).
- O mapa do WorldForge já vem com sol, céu e névoa: o diretor **assume esses atores** (aplica a receita
  neles) em vez de somar outros, e expõe `GetSun()` (a sombra da fumaça das explosões entra como função de
  luz no sol; a foto do minimapa usa a luz do mapa).
- `ALookDirector::ApplyPreset(int32 Level)` (1..4, o mesmo do Scalability), chamado pelo
  `ARTSPlayerController::ApplyQuality` (F5..F8): a receita e o Scalability andam juntos.
- `ILookCamera` também com `GetLookFocusPoint()`.
- A névoa de altura nunca com começo zero (tinge o foco de perto); o pós ilimitado vem depois da passada
  da névoa de guerra (`RTSFogPass`).
- A exposição travada muda o brilho do clarão das explosões: a sessão das explosões já compensa a emissão
  pela exposição (referência EV100 12,58 do BlastLab); o valor de cada receita é combinado com ela.

### A fila do look (11/10, a ordem do Felipe: comentário novo entra na fila, não muda a ordem)

1. A noite: avaliar junto com o Felipe o A/B (`captures/look/noite_A_B.jpg`: o fogo pintando o tanque inteiro × lado laranja e teto na cor da facção).
2. A cena de perto contra a `close_ground_light_gemini` (cena própria `desert_ground`) e o deserto contra as sete "perfeitas" na câmera do jogo, em A/B com a receita de antes (`desert_golden_hour_v1_before_seven.json`).
3. O desempenho: os presets baixa/média/alta/ultra, o look em ~8 ms no alta no mapa do jogo.
4. Os materiais no plugin RTSLook: o pó das unidades (Team e queimado por CPD), o reboco e **o chão em qualidade AAA**. O Felipe: "o pior de todos é a qualidade da textura do chão, é o que tá piorando tudo". Primeiro o estudo (como os jogos AAA resolvem o chão visto do alto a 60 m: conjunto de texturas de alta resolução com variação grande, quebra do ladrilho, normal de detalhe, textura virtual, decalques, relevo), uma segunda opinião e só então o código, medido.
   - A areia do deserto v1 está **aprovada** pelo Felipe (12/10: "uma das melhores até agora") e congelada pro WorldForge
     em `Game/Tools/look_materials/sand_desert_v1.*` (o material e o espalhamento: capim seco, gravetos, pedrinhas).
     Próximo nela: o relevo de ondulação simulado (o modelo de Nishimori & Ouchi, 1993, num mapa de altura que emenda sem
     costura, em manchas) no lugar do seno do shader, e o hex tiling (Heitz & Neyret 2018, Mikkelsen 2022) contra a
     repetição; materiais escaneados (Megascans/Fab) só se forem gratuitos, pra comparar.
5. O kit civil pro WorldForge (as 6 peças e as 6 miudezas, `civ_*`, mais o `civ_pole`) e os parâmetros de água (WaterBodyColor/Strength, SkyReflection) na receita.

### O desempenho medido (12/10, o mapa do deserto em -game, 1080p, `look.ps1 perf Golden -Preset …`)

| Preset | GPU (mediana) | Os maiores |
|---|---|---|
| alta | 28,3 ms | Lumen (coleta de sondas) 6,8 · TSR 4,9 · sombras por campo de distância 2,0 · névoa volumétrica 2,0 · Nanite 1,9 |
| média | 20,5 ms | TSR 4,5 · Lumen 2,9 · sombras por campo de distância 1,9 |
| baixa | 9,9 ms | TSR 2,2 · Lumen (campo de irradiância) 1,0 |

Plano dos presets (cada corte em A/B contra o ultra na mesma câmera): Lumen em resolução menor no alta, TSR a ~70% da tela,
névoa volumétrica com grade menor, sombras por campo de distância só no ultra. Meta: alta ~16,7 ms (60 qps).

### O que o Felipe escolheu até 13/10/2026 (o 1º commit do look)

- **O jogo:** a receita `desert_golden_hour_v2_aprovada` (congelada; o jogo aponta pra ela no Maps.json). A bancada
  `desert_golden_hour` só a substitui depois de um A/B com ele. A areia nova só vai junto com a bancada (exposição do jogo
  14,3: o sol do jogo é a 30°); critério de estouro: o claro p95 0,65–0,75 e o estourado < ~2,5%, como nas favoritas.
- **A areia:** a v2 (a versão C): a v1 + zonas + ondulações simuladas só nas zonas delas + a trilha da esteira (fresca e
  velha mais marcada), congelada em `Game/Tools/look_materials/sand_desert_v2.*` pro WorldForge; a trilha do jogo vem da
  `MF_LookGroundMemory` (o plugin), que também pinta a cratera, o ejetado e a fuligem.
- **A noite:** a A (o fogo pinta as unidades inteiras; a separação de luz ficou desligada pela receita).
- **O kit civil da vila** (`SM_civ_*`, `look.ps1 civil`) no jogo; os modelos finais continuam dele.

### A vila contra as favoritas (13/10/2026, à noite: "horrível perto dos game shots")

O Felipe comparou a vila da maquete com os game shots e achou feia. O defeito era de conjunto, não de média de cor:
cena vazia, asfalto liso, ar parado, sombra roxa, casas em caixa. As voltas (cada uma medida contra as sete com
`look_stats.py` e na folha lado a lado):
- **Volta 1–2:** o vestir da vila (`DRESSING` no `look_layout.py`, o kit de entulho do `look_tank.py`: telhados, pé de
  parede, beira de rua, terrenos; a rua das unidades limpa), asfalto rachado e mais escuro, três carcaças de carro,
  sombra quente, contraste de volta (p95 0,69).
- **Volta 3–4:** saturação e calor pra baixo (0,56 → 0,52 e 0,26 → 0,24; o alvo é 0,44 e 0,21). Também:
  - poeira no ar ao fundo (a névoa 0,11 → 0,15: a razão topo/base 1,19 → 1,27, faixa das sete 1,28–1,82);
  - asfalto menos laranja;
  - o tanque da USA cor de areia com o painel do time azul forte (as sete; o cinza-claro antigo saía branco).
- **Volta 5:** a medida por faixa de brilho achou o excesso de saturação nas **sombras** (0,69 contra 0,48 no escuro,
  0,57 contra 0,42 na meia-sombra); o claro já batia. A saturação das sombras foi pra 0,65 e a do meio-tom pra 0,92.
  O tanque também foi pelas razões medidas no teto ao sol (alvo 0,67/0,53/0,40 na road_battle), e o asfalto ficou 20% mais cinza no mesmo matiz.
- **A carcaça queimada** (maquete e jogo, a mesma receita, medida na carcaça da street_battle): marrom-carvão com
  variação, não bloco preto.
  - Cores sRGB: tinta queimada 92,74,60 (~55%), fuligem 34,30,27 embaixo e em manchas, ferrugem de calor 118,70,42 em
    manchas de ~1,5 m, cinza 165,155,142 em placas no teto.
  - Na tela o alvo dá p10/p50/p90 ≈ 0,03/0,33/0,60. Na maquete é o `BURNT_CODE` do `look_lab.py`.
  - O miolo quente no jogo: corpo negro de 1300 → 900 K, pico ~3000 nits, (1 − t/30)², brasas mais 20 s.
- **Voltas 6–14 (13/10, de madrugada), pelas três críticas da sessão de arte (a imagem primeiro):**
  - **O chão:**
    - asfalto mais escuro e cinza que a areia (1 : 0,76 : 0,58, perto da road_town_battle);
    - areia por cima só na borda da pista, em línguas na direção do vento (o relevo ondulado dentro do asfalto fazia bolhas no meio da pista);
    - mato mais denso, com o capim seco dourado.
  - **A luz contra a sombra** (medida por faixa de brilho contra as sete):
    - o malva vinha do rebote quente (`lumen_diffuse_color_boost` 1,2) somado ao céu azul: o rebote voltou a 1,0;
    - na Unreal, a areia ao sol cai no meio-tom (o claro só começa em `ColorCorrectionHighlightsMin`): o meio-tom em 0,86 desbotava a luz;
    - a sombra da Unreal só vai até `ColorCorrectionShadowsMax` 0,09: subiu pra 0,2;
    - o sol foi pra 4700 K (o ouro), e a exposição local deixou de levantar tanto as sombras (0,8).
  - **A explosão provisória saiu** da vila até chegar a nova (a crítica: "figurinha, estraga a leitura do quadro").
  - **O brilho direcional da névoa** (luminância 2400) velou o quadro inteiro: fora, e não entra de novo sem medir a escala.
  - **Unidades:**
    - USA cor de areia, com o azul do time 40,70,200 (lê de cima sem virar violeta);
    - técnicas do GLA brancas, vermelhas e ferrugem;
    - a tinta com brilho no topo (rugosidade 0,45) e mais poeira embaixo.
  - **Casas:** um tom por casa (caiada creme, ocre), tetos claros, sujeira subindo ~1,5 m da base, reboco caído mostrando o barro.
  - **Resultado (câmera de 61 m):** saturação 0,48, calor 0,23, p5/p50/p95 0,09/0,35/0,69, névoa 1,39. Tudo dentro da faixa das sete.
    - Folhas: `captures/look/vila_antes_depois.jpg` e `vila_perto_antes_depois.jpg`.
    - O que sobra é conteúdo (densidade de unidades e infantaria, a explosão nova) e o detalhe das casas.
  - **Tudo isso está na bancada** (`desert_golden_hour.json`). O jogo segue na `v2_aprovada` até o A/B com o Felipe.
- **A cena dos tanques** (`LL_DesertTanks_Tanks`, `look_layout_tanks.py` + `desert_tanks.json`), pedida pelo Felipe pra sessão das explosões:
  - 4 tanques oliva em linha diagonal e a carcaça queimada no meio;
  - ondulação no chão todo, trilhas cruzando, sol de lado a 18°, o pós da cena de perto;
  - foto em `captures/look/tanques_cena.jpg`. A sessão das explosões já testou a morte de tanque nela.
- **As texturas de efeito (chama e fumaça) não entram no streaming.** No -game com o pool estourado, a chama caía no último mip e o cartão inteiro brilhava, um quadrado branco no BlastLab.
- **A noite A refeita sem cair** (o shader do jogo estável): `captures/look/noite_A_final.jpg`.
