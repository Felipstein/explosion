# Otimização na GTX 1660 (estudo de 14/10/2026, madrugada)

**O pedido do Felipe (13/10 à noite):** "foco na otimização, otimização perfeita, como os grandes otimizam, nas pegadinhas, nos truques; eu quero a lindeza e otimização". O ultra é a beleza máxima. Os outros presets seguram o quadro.

**O estado:** estudo com fontes (o fonte da 5.8 e a documentação da Epic). **Nada medido no jogo ainda.** Os números marcados "est." são estimativas a partir da maquete e só valem depois de A/B no jogo, na mesma cena e na mesma semente, com o vídeo pra beleza.

## A base medida (a maquete, 1080p, Scalability 3, CSV de 600 quadros)

O quadro tem mediana de 26,9 ms. Os passes mais caros:

| passe | ms |
|---|---|
| LumenScreenProbeGather | 6,98 |
| TemporalSuperResolution | 5,02 |
| NaniteBasePass | 3,13 |
| VolumetricFog | 1,96 |
| ShadowProjection | 1,69 |
| RenderDeferredLighting | 1,52 |
| NaniteVisBuffer | 1,27 |
| ShadowDepths | 1,01 |
| LumenSceneLighting | 0,98 |

O jogo mediu 31 ms na alta, no cenário `chao`, com 12 tanques e a poeira (a sessão do jogo, 13/10).

## As pegadinhas que mandam

- **Na 1660 a Unreal desliga o async compute.** No `D3D12Adapter.cpp`, na NVIDIA ele só liga da Ampere em diante. Tudo roda em série: só cortar trabalho funciona.
- **Os presets do jogo usam Médio/Alto/Épico/Cinematic da Unreal,** a 50/58/67/100% de resolução (`RTSPlayerController.cpp:150`). Conferir em que resolução cada medida foi feita.
- **A alta usa o viés do VSM do Épico (−1,5) com metade do pool,** e o aviso de estouro está desligado (`r.Shadow.Virtual.AllowScreenOverflowMessages=0`). Conferir com `r.Shadow.Virtual.Stats.Visible 1`.
- **A névoa volumétrica começa na distância de foco da câmera.** As 128 fatias do Épico (pensadas pra primeira pessoa) cobrem só ~75–250 m aqui: 64 bastam.
- **A areia com rugosidade 0,9 nunca traça reflexo do Lumen** (`MaxRoughnessToTrace` 0,4). Abaixo do ultra, o SSR basta.
- **Unidade que anda invalida o cache do Global SDF.** Tirar as unidades do campo de distância; o AO de contato e os screen traces continuam pegando o tanque.

## As cinco de maior valor (ms ganho por perda visual)

| # | Troca | Est. | Dono |
|---|---|---|---|
| 1 | Névoa volumétrica de 8 px/128 pra 16 px/64 fatias na alta | −1,5 ms | Look (a receita) + jogo (a escalabilidade) |
| 2 | Viés do VSM de −1,5 pra −0,5 na alta, conferindo o estouro do pool | −0,5 a −0,8 ms | jogo |
| 3 | Lumen de Épico pra Alto na alta (o ajuste de 60 qps da Epic) | −3 a −3,5 ms | Look + jogo |
| 4 | O chão em RVT, com a memória do chão por cima, fora do cache | −1,5 a −2 ms | WorldForge (o Look revisa o CODE) |
| 5 | TSR com AA Alto + FP16 (`r.TSR.16BitVALU.Nvidia`) | −0,5 a −1,5 ms | jogo |

A resolução vem depois: é o maior ganho (−4 a −5 ms contra 100%), mas a perda mais visível.

## A tabela proposta (a confirmar medindo)

| | baixa | média | alta | ultra |
|---|---|---|---|---|
| Resolução / AA | 58%, TSR AA 1 | 67%, TSR AA 1 | 75%, TSR AA 2 | 100%, TSR AA 3 |
| GI | sem Lumen, com skylight + DFAO | Lumen Lite (Médio da 5.8, beta) | Lumen Alto | Lumen Épico |
| Reflexos | SSR | SSR | SSR | Lumen, downsample 2 |
| VSM viés / SMRT | +1 / 4×2 | +0,5 / 4×2 | −0,5 / 8×4 | −1,5 / 8×4 |
| Névoa volumétrica | desligada (height fog + aerial perspective) | 16 px / 64 | 16 px / 64 | 8 px / 128 |
| Terreno | RVT, camadas Low | RVT | RVT | RVT + tesselação |
| GPU est. | ~7–8 ms | ~10–11 ms | ~14 ms | ~27–30 ms |

**Outros pontos:**
- `LumenSceneViewDistance` e `LumenMaxTraceDistance` a ~300 m: o topo da tela no zoom de 118 m passa dos 200 m do padrão.
- A sombra no cache: sol parado na partida; tanque com `Shadow Cache Invalidation Behavior = Rigid`; nada de WPO em unidade; árvores com WPO Disable Distance.
- `r.TSR.Velocity.WeightClampingSampleCount 2` (o do Fortnite), porque a câmera de RTS anda o tempo todo.
- Tanque como static mesh Nanite por peça (casco, torre, canhão), nunca skeletal.

## Como medir (a regra)

- Mesmo cenário, mesma semente, câmera fixa e 100+ quadros de aquecimento.
- `-csvGpuStats` com 600 quadros; mediana e p95 do tempo de GPU, nunca qps. ProfileGPU pra ver por passe; Unreal Insights com os 200 tanques.
- A beleza no mesmo quadro, contra a versão aprovada, mais um vídeo com a câmera andando (o TSR e a névoa só mostram defeito em movimento).

## Fontes

- O código da Unreal 5.8:
  - `Config/BaseScalability.ini`
  - `Lumen/Lumen.cpp`, `VolumetricFog.cpp`, `VirtualShadowMapClipmap.cpp`, `TemporalSuperResolution.cpp`
  - `D3D12RHI/Private/D3D12Adapter.cpp`
- A Epic:
  - Lumen Performance Guide: https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-performance-guide-for-unreal-engine
  - TSR: https://dev.epicgames.com/documentation/en-us/unreal-engine/temporal-super-resolution-in-unreal-engine
  - VSM: https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine
  - Volumetric Fog: https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-fog-in-unreal-engine
  - RVT: https://dev.epicgames.com/documentation/en-us/unreal-engine/runtime-virtual-texturing-in-unreal-engine
  - Fortnite na 5.1: https://www.unrealengine.com/en-US/blog/battle-testing-unreal-engine-5-1-s-new-features-on-fortnite-battle-royale-chapter-4
- O terreno virtual:
  - Far Cry 4, Adaptive Virtual Texture (GDC 2015): https://gdcvault.com/play/1021760/Adaptive-Virtual-Texture-Rendering-in
  - Terrain in Battlefield 3 (GDC 2012): https://gdcvault.com/play/1015414/Terrain-in-Battlefield-3-A
- Manor Lords, o dev sobre o Lumen: https://steamcommunity.com/app/1363080/discussions/0/4522262793488093900
