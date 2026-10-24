# Otimização das explosões no jogo

O pedido do Felipe (14/10, madrugada, ao sair): "quero quando voltar a v2 já montada e testada, e após os testes, FOCO
NA OTIMIZAÇÃO, otimização perfeita, como os grandes otimizam, nas pegadinhas, nos truques; eu quero a lindeza e
otimização". Aqui: a morte de tanque e as explosões, medidas em ms no pior caso (várias no mesmo quadro).

## Como medir

- O pior caso: `Game/Tools/game.ps1 view alta -Folder <nome> -Scenario carcaca -Extra '-csvGpuStats -csvCaptureFrames=1300'`.
  - São 11 mortes no mesmo segundo (~24 s de partida), com a câmera a 50 m.
  - 1300 quadros fecham a captura antes do fim do teste. Se o teste fechar antes, a Unreal não grava o arquivo.
  - O CSV fica em `%LOCALAPPDATA%\UnrealEngine\5.8\Saved\Profiling\CSV\`.
- Ler: `python BlastLab/Tools/game_perf.py [arquivo.csv] [--top N]`.
  - Compara a janela das mortes (150 quadros depois do 1º evento `TankDeath`) com os 300 quadros antes.
  - Mostra o quadro, as threads do jogo e de desenho, a placa e cada passe dela.
  - Marcadores próprios: `BlastFxDebris/GameThread/Tick`, `BlastFxRaymarch/GameThread/Tick`, `BlastFxDirector/GameThread/TankDeath` e `GPU/BlastRaymarch`.
- Os picos de 150–350 ms no quadro dos 25 s são a foto do teste (`ViewportMisc`, `Slate`), não o jogo.

## O que já foi feito (14/10)

| O quê | Antes | Depois | Muda a imagem? |
|---|---|---|---|
| Destroços: a altura do chão vem direto do heightfield (`GetHeightAtLocation`). Antes, cada célula nova de 1 m traçava contra os 256 pedaços do Landscape. | 17,4 ms de processador (média), p95 63, pico 87 | 6,9 ms, p95 13, pico 15 | não |
| Destroços: cada parte (chapas, faíscas, rastros) para de ir pra placa quando acaba, e o ator para de vez. Antes, as chapas deitadas eram reenviadas por 2 min e redesenhavam a sombra virtual delas. | (junto acima) | (junto acima) | não (A/B `captures/game/ab_parada_destrocos.jpg`) |
| Rastro: criar a seção já a limpa. Limpar antes fazia a colisão síncrona e o desenho novo duas vezes por quadro. | (junto acima) | (junto acima) | não |
| Processador do jogo na janela das mortes | 35,5 ms (p95 88,7) | 22,7 ms (p95 27,1) | |
| Quadro na janela das mortes | 47,3 ms (p95 87,4) | 39,6 ms (p95 55,0) | |
| Destroços por preset: no Média (`sg.EffectsQuality` 2) metade das chapas, cometas, espinhos e fagulhas; no Baixa (≤ 1) um quarto; Alta e Ultra inteiros. Decidido no nascimento. O custo é de processador, então a placa não o corta (o Média roda num notebook de 2014). | ~0,6 ms por morte | ~0,3 no Média, ~0,15 no Baixa (proporcional; falta medir) | só no Média e no Baixa, com a mesma leitura (`captures/game/ab_media.jpg`) |

## O raymarch por passe e os A/B (14/10, madrugada)

Perfil com um marcador por passe, no pior caso (11 mortes, câmera a 50 m, Alta). Os quatro testes em gravação de passo
fixo (`-Record`) são comparáveis entre si: `captures/game/fx_ab_*/perfil.csv` e `batalha.mp4`.

| Passe (média / p95 na janela das mortes, ms) | Atual | 42 passos (`-RmSteps=42`) | Marcha em 0,42 (`-RmRes=0.42`) |
|---|---|---|---|
| Cache de luz | 1,40 / 4,29 | 1,40 / 4,25 | 1,38 / 4,21 |
| Sombra no chão | 0,37 / 0,96 | 0,38 / 0,96 | 0,37 / 0,97 |
| **Marcha** | **5,30 / 16,33** | **4,29 / 12,87** | **4,25 / 12,92** |
| Acúmulo + composição | 0,34 / 0,58 | 0,34 / 0,74 | 0,32 / 0,49 |
| **Raymarch inteiro** | **7,39 / 21,86** | **6,41 / 18,46** | **6,31 / 18,50** |

- **A imagem:** recortes em tamanho real no pico (`captures/game/ab_passos_zoom.jpg`, `ab_res_zoom.jpg`). As três dão
  a mesma nuvem, com diferença média de 3–4 níveis em 255. A cintilação quadro a quadro dentro das nuvens também é a
  mesma: 4,77 atual, 4,82 com 42 passos, 4,69 em 0,42.
- **Os dois juntos e os 4 caches (03:50):**

| Passe (média / p95, ms) | Atual | 42 passos + 0,42 | 4 caches |
|---|---|---|---|
| Cache de luz | 1,40 / 4,29 | 1,39 / 4,35 | **0,54 / 1,70** |
| Marcha | 5,30 / 16,33 | **3,46 / 10,36** | 5,23 / 15,94 |
| Raymarch inteiro | 7,39 / 21,86 | 5,55 / 16,04 | 6,47 / 18,89 |

  A mesma nuvem, com cintilação 4,77 / 4,75 / 4,79 e diferença de 3–4 níveis em 255 (`captures/game/ab_combo_lc4.jpg`).
- **Os presets da fumaça** (`RTSShaders/BlastRaymarch.cpp`, por `sg.EffectsQuality`; as opções de linha de comando passam por cima):

| Preset | Passos | Marcha | Caches de luz |
|---|---|---|---|
| Ultra (Cinematic) | 56 | 0,5 | 12 |
| Alta | 42 | 0,42 | 4 |
| Média | 32 | 0,38 | 2 |
| Baixa | 24 | 0,33 | 1 |

  No Alta, o pior quadro do raymarch cai de ~22 pra ~13 ms (estimativa somando os dois A/B). O Média e o Baixa ainda vão ser conferidos no próprio preset.
- **A 1ª morte da partida travava ~2 s.** Os aquecimentos do diretor tinham opacidade 0, e o raymarch pula opacidade 0.
  Por isso o atlas (198 MB), a subida da gravação, a sombra no sol e o 1º uso de cada passe caíam na 1ª morte.
  - Conserto (compilado às 02:55, sem teste na placa por causa da pausa): uma morte nova invisível (opacidade 0,001) no aquecimento.
  - A sombra no sol, depois de ligada, custa ~0,04 ms por quadro.

## O que falta, em ordem

**O pior caso na placa é o raymarch.** Ele custa 11–21 ms por quadro entre 1,1 e 2,4 s depois das mortes, quando as 11 nuvens estão no tamanho máximo (28 quadros acima de 10 ms). Na média da janela, os outros passes sobem menos de 0,6 ms cada; o Lumen tem picos isolados de 15–25 ms.

Da pesquisa de 14/10. As fontes estão na lista do fim; o que vem do código nosso foi conferido nele.

1. **Medir cada passe do raymarch** antes de mexer: o cache de luz, o mapa de sombra, a marcha, o temporal e a composição. Os marcadores estão no jogo e no BlastLab desde 14/10 (`GPU/BlastRmLight`, `BlastRmShadow`, `BlastRmMarch`, `BlastRmTemporal`, `BlastRmComposite`); falta rodar o perfil.
2. **Cache de luz persistente.** Hoje o 48³ é refeito do zero a cada quadro pra até 12 explosões. A ideia é atualizar metade por quadro, 32³ fora das 3 maiores e menos caches sob carga (`-RmLightCaches=`).
   - A Frostbite (Hillaire 2015), o AC IV (Wronski 2014), o Lumen e o Doom 2016 amortizam a luz entre quadros.
   - Risco pequeno; precisa A/B contra a aprovada.
3. **Luz barata depois que o raio fica opaco** (opacidade > 0,3). No Horizon Zero Dawn (Schneider 2015) isso deixou o shader 2× mais rápido. Precisa A/B.
4. **Salto conservador no espaço vazio** quando a fumaça está deformada. Hoje vira um passo só (`BlastRaymarch.usf`). Não muda a imagem.
5. **Orçamento único no diretor, pela fração da tela e pelo tempo da placa.** Cobre faíscas, fagulhas e espinhos, os passos da marcha e as sombras.
   - É decidido quando a explosão nasce, nunca no meio da vida, e com histerese.
   - Corta primeiro o que é numeroso, pequeno e curto; mantém a silhueta (a bola, a torre e o rastro dela, as chapas grandes).
   - Precisa A/B no arsenal contra a aprovada.
6. **Luzes do fogo.** Pôr histerese no ranking das 3 que têm sombra: hoje a 3ª e a 4ª trocam de lugar e `SetCastShadows` recria a luz. No ultra, não mover a luz que tem sombra virtual. Não muda a imagem (até pisca menos).
7. **Mais tarde (grande):** os destroços na placa, num sistema só pra todas as mortes, montados no shader de vértice ou na Niagara.

**Não serve pro nosso caso:**
- MegaLights: a 1660 não tem ray tracing.
- Heterogeneous Volumes: 111 ms medidos em 07/10.
- Reprojeção de 1/16 na explosão: deixa rastro.
- Flipbook ou froxel na bola: mistura de sistemas, o defeito nº 1 que ele já apontou.

## Fontes

- Horizon Zero Dawn (Schneider, 2015), Advances in Real-Time Rendering: https://www.advances.realtimerendering.com/s2015/
- Frostbite, volumetria unificada (Hillaire, 2015): https://www.ea.com/news/physically-based-unified-volumetric-rendering-in-frostbite
- Névoa volumétrica do AC IV (Wronski, 2014): https://bartwronski.com/wp-content/uploads/2014/08/bwronski_volumetric_fog_siggraph2014.pdf
- Doom 2016, estudo gráfico: https://www.adriancourreges.com/blog/2016/09/09/doom-2016-graphics-study/
- Volumetria em tempo real com poucos passos (Toft et al., 2016): https://arxiv.org/abs/1609.05344
- StarCraft II (SIGGRAPH 2008), sombra da fumaça num 2º mapa: https://www.advances.realtimerendering.com/s2008/
- Niagara, escalabilidade e boas práticas: https://dev.epicgames.com/documentation/unreal-engine/scalability-and-best-practices-for-niagara
- Significance Manager: https://dev.epicgames.com/documentation/en-us/unreal-engine/significance-manager-in-unreal-engine
- Sombra virtual (invalidação por caixa e por luz que se move): https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine
- PSO precaching: https://dev.epicgames.com/documentation/en-us/unreal-engine/pso-precaching-for-unreal-engine
- Fonte da Unreal 5.8:
  - `ProceduralMeshComponent.cpp`: criar ou limpar a seção refaz a caixa, a colisão síncrona e o desenho;
  - `LightComponent.cpp`: `SetCastShadows` recria a luz;
  - `LandscapeProxy.h`: `GetHeightAtLocation`.
