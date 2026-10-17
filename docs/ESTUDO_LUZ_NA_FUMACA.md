# Estudo: a sombra do mapa e a luz das explosões em toda fumaça

Estudo da sessão Fluxo (16/10 à noite), com fontes. É o pedido do Felipe: "a sombra do mapa projeta na fumaça… até a luz de explosões projetava na fumaça; o mundo todo tem que ser assim, sem gambiarra".
- `UE\` = `C:\Program Files\Epic Games\UE_5.8\Engine\`; `G\` = `C:\Users\luisf\explosion\Game\`.
- **[E]** = estimativa.

## O estado hoje (16/10)
- **O raymarch das explosões e poeiras** (`G\Shaders\Private\BlastRaymarch.usf`): só auto-sombra (a transmitância até o topo e o cache de luz). Projeta sombra no chão, mas não recebe a sombra do cenário nem as luzes das explosões.
- **O rastro do míssil** (`RTSMissileSmoke`, sessão do jogo): auto-sombra calculada no processador e sombra no chão. Também não recebe.
- **No BlastLab:** os volumes da Unreal recebiam a luz da cena. Na troca pro raymarch isso se perdeu, e o Felipe não foi avisado.

## Como os AAA fazem
- **Frostbite** (Hillaire 2015/2016): froxels; o sol vem das cascatas, as luzes locais da lista por bloco. A sombra das nuvens é uma textura 2D que as partículas também leem.
- **AC IV** (Wronski 2014): grade 160×90×64, sol por ESM, ~1,1 ms.
- **Volumetric Fog da Unreal:** froxels de 8–16 px; CSM/VSM e as luzes clusterizadas.
- **Killzone SF:** marcha no passe de cada luz, reusando o mapa de sombra dela.
- **Horizon (Nubis 2022):** as nuvens leem um mapa de sombra de longe.
- **TLOU2:** a névoa já iluminada ilumina as partículas, e as luzes de runtime ficam fora do filtro temporal.
- **O padrão:** amostrar o mapa de sombra do sol no ponto do meio e somar as luzes de uma lista. O froxel só barateia com a câmera perto; a nossa fica a 67–118 m.

## O que a Unreal 5.8 tem, e por que não serve de um módulo de jogo

| Opção | Um módulo de jogo usa? | Resolução a 70–180 m | Fica de fora | Custo [E] |
|---|---|---|---|---|
| Translucency Lighting Volume | não (privado; cascatas até 15 m e 50 m da câmera) | fora do volume | tudo | — |
| Volumetric Fog | não (privado; guarda a luz espalhada pela névoa, não o campo de luz) | 0,4–0,7 m × 2–3 m | — | — |
| Virtual Shadow Maps (como os Heterogeneous Volumes) | não (FVirtualShadowMapArray e FViewInfo privados, engine instalada) | exata | — | — |
| Global SDF (público, no View UB) | sim | 0,8–1,6 m, inflado | malha sem distance field | 0,3–0,8 ms |
| **Mapa de profundidade do sol próprio** | **sim** | **0,25–0,5 m** | nada, com a camada dinâmica | **0,1–0,3 ms + captura** |
| Grade 2,5D do jogo (a do raio-X) | sim (processador e placa) | 4 m hoje | veículos, forma real, copa | ~0,05 ms |

- **A grade 2,5D** (um mapa de altura da sombra, H(xy)) é técnica estabelecida (horizon map), exata pra relevo e bloco com o sol fixo.
  - Na nossa grade, a célula de 4 m dá degraus. A copa de palmeira é uma faixa no ar e um H só não a representa. Veículos e destruição ficam de fora.
  - Fica pro preset Baixa, ou como cópia no processador tirada do mapa capturado.
- **O Global SDF** serve pra tapar a luz das explosões (as explosões já usam sombra por distance field nos presets menores). É fraco pros feixes de sol, que precisam de borda nítida.

## A recomendação: uma arquitetura só pra todas as fumaças
1. **O mapa do sol** (`RTSShaders`):
   - captura ortográfica alinhada ao sol, só profundidade (`bRenderInMainRenderer`, Unreal 5.8);
   - **camada estática** (relevo, vila, palmeiras), feita uma vez por partida porque o sol é fixo: 2048²–4096² sobre o mapa de 1 km, 0,25–0,5 m por texel. É refeita por bloco quando a simulação avisa que algo foi destruído;
   - **camada dinâmica** só dos veículos, 1024² em volta do foco;
   - a fumaça, as malhas procedurais e os decalques ficam escondidos da captura.
2. **No raymarch:**
   - a luz do sol em cada amostra multiplicada pela visibilidade V(p) do mapa (PCF por hardware);
   - a luz que volta do chão multiplicada pela V do chão embaixo;
   - um filtro mais largo nas oitavas do espalhamento múltiplo.
3. **As luzes das explosões:**
   - um registro no jogo vira um buffer de até ~32 luzes, avaliadas com a queda da própria Unreal;
   - a soma entra no cache de luz (48³), com a transmitância pela fumaça;
   - fica fora do acúmulo temporal.
4. **O rastro do míssil:** continua Unlit (a fase HG e o espalhamento múltiplo dele ficam).
   - O mapa do sol entra como textura com a matriz por parâmetro, lido por pixel.
   - As luzes locais são somadas por puff no processador.
   - A alternativa "Surface ForwardShading" lê a VSM nativa, mas perde a fase e o espalhamento e custa 0,3–0,8 ms com a sobreposição dos puffs.
5. **Fase 2:** um canal de profundidade óptica da fumaça no mesmo mapa (o Beer Shadow Map das nuvens da Unreal), pra uma fumaça fazer sombra na outra.

**Custo [E] na 1660 a 1080p, por quadro:**

| Parte | Custo |
|---|---|
| Sol por amostra | 0,1–0,3 ms |
| Camada dinâmica | 0,1–0,3 ms |
| Luzes locais | 0,1–0,3 ms |
| Rastro do míssil | < 0,05 ms |
| **Total** | **~0,3–0,9 ms** |

A camada estática custa uma vez por partida.

**Riscos:**
- acne e vazamento no volume (viés de 1–2 texels, PCF);
- ~1 texel de diferença contra a VSM do chão (pede A/B na luz do jogo);
- o vento das palmeiras não aparece na captura;
- sombra fantasma de prédio destruído, se faltar o aviso da simulação;
- confirmar que `bRenderInMainRenderer` funciona com ortográfica e profundidade;
- a 1660 não tem RT por hardware (MegaLights fica de fora).

**Arquivos da Unreal a imitar:**
- `HeterogeneousVolumesRayMarchingUtils.ush:71-114` (onde entra a sombra);
- `VolumetricFog.usf:940-980,1075` (o sol e as luzes);
- `DeferredLightingCommon.ush:246-260` (a queda da luz);
- `GlobalDistanceFieldUtils.ush:90`;
- `VolumetricCloud.usf:2265` (o Beer Shadow Map).

## Fontes
- [Hillaire 2015](https://www.slideshare.net/DICEStudio/physically-based-and-unified-volumetric-rendering-in-frostbite), [Hillaire 2016](https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/s2016_pbs_frostbite_sky_clouds.pdf), [Wronski 2014](https://bartwronski.com/wp-content/uploads/2014/08/bwronski_volumetric_fog_siggraph2014.pdf)
- Unreal: [Volumetric Fog](https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-fog-in-unreal-engine), [Heterogeneous Volumes](https://dev.epicgames.com/documentation/en-us/unreal-engine/heterogeneous-volumes-in-unreal-engine), [Lit translucency](https://dev.epicgames.com/documentation/en-us/unreal-engine/lit-translucency-in-unreal-engine), [Distance field shadows](https://dev.epicgames.com/documentation/en-us/unreal-engine/distance-field-soft-shadows-in-unreal-engine)
- [Killzone SF](https://www.guerrilla-games.com/media/News/Files/GDC2014_Valient_Killzone_Graphics.pdf), [Nubis 2022](https://advances.realtimerendering.com/s2022/SIGGRAPH2022-Advances-NubisEvolved-NoVideos.pdf), [TLOU2](https://history.siggraph.org/wp-content/uploads/2022/08/2020-Talks-Kovalovs_Volumetric-Effects-of-The-Last-of-Us-Part-Two.pdf)
- [Deep Shadow Maps](https://graphics.stanford.edu/papers/deepshadows/), [Opacity Shadow Maps](https://diglib.eg.org/items/86a53d3f-cb12-4e56-8fd6-391ae1b45cc5), [Fourier Opacity Mapping](https://doi.org/10.1145/1730804.1730831), [AVSM](https://vidimce.org/publications/avsm/), [Timonen e Westerholm 2010](https://diglib.eg.org/handle/10.2312/CGF.v29i2pp723-731)
- A fonte da Unreal 5.8: os caminhos citados.

## A segunda opinião e o que ficou combinado (16/10, noite)

**A revisão aprovou com mudanças:**
- **A VSM** até dá pra alcançar pelo `PostCreateSceneRenderer` (público). Mas os parâmetros dela não são exportados pelo Renderer, então só ligando o buffer cru por fora do RDG, e isso é gambiarra. E a página fina só existe onde a tela marcou chão. Fica descartada.
- **O mapa estático do sol fica.** Os veículos entram como caixas (casco e torre), não como uma segunda captura, que custaria 0,3–1 ms de render thread.
- **A sombra do cenário fica fora das oitavas do espalhamento múltiplo.** Hoje é `TSun = lerp(1, SunT, Ap)`. Se ela entrasse no SunT, a fumaça na sombra de um prédio ainda pegaria sol pelas oitavas. Ela multiplica o termo do sol em todas.
- **As luzes:** no máximo 4 por explosão, escolhidas no processador. A transmitância marchada fica só pro próprio fogo; as outras usam a transmitância do céu.
- **O custo pelos piores 5% da janela das mortes:**
  - a marcha hoje custa 3,46 ms de média e 10,36 ms nos piores 5%;
  - o alvo com a luz nova é ~0,3 ms de média e 0,6–1 ms nos piores 5%.

**A divisão combinada (um sistema só):**
- **O mapa do sol é o da sessão de arte.** Plugin `RTSSunShadow`, já medido no FXLab: grade de 0,5 m girada pro sol, horizon map exato pra relevo e prédio, a faixa da copa da palmeira, os veículos como caixas e a recaptura por área quando algo é destruído; +0,10 ms de placa com 40 tanques.
  - Shader: `RTSSunVisibilityM(PosCamRelM, SoftM)` em `/Plugin/RTSSunShadow/Private/SunShadow.ush`.
  - C++: `FRTSSunShadowParameters` e `RTSSunShadow::SetShaderParameters`.
- **As luzes da cena são da sessão Fluxo:** `RTSSceneLights` no `RTSShaders`.
  - O diretor do BlastFx junta a cada quadro as luzes pontuais acesas (até 32, em candelas).
  - O raymarch soma as 4 que mais iluminam cada explosão.
  - `IlluminanceAt(posM)` dá lux pra quem soma por puff no processador (o rastro do míssil, o diesel).
- **Os materiais da sessão do jogo** (o rastro, o diesel): Unlit; leem o mapa do sol por pixel e as luzes por puff.

## Fase 2: uma fumaça faz sombra na outra (16/10, noite)

**O defeito.** Cada explosão só tinha a própria sombra. Com o sol a 8°, uma coluna de 20 m põe ~140 m de chão na sombra (o chão já recebe, pela light function). A poeira e as outras fumaças dentro dessa faixa continuavam ao sol: poeira clara em cima de chão escuro.

**A primeira ideia, recusada pela 2ª opinião:** continuar a marcha do cache de luz pelas caixas das outras.
- ~48 células na mesma reta do sol repetem a mesma marcha.
- Nos presets baixos só 1–2 explosões têm cache.
- A poeira ficaria de fora.

**O que foi feito** (`BlastRaymarch.usf` `MainSunTiles`, `TauTileAt`, `SunTilesT`):
- **Um ladrilho de 64² por explosão no espaço do sol**, num atlas 4 × 4. Por texel guarda a frente, o fundo, τ no meio e τ total. É o Beer Shadow Map das nuvens da Unreal (`VolumetricCloud.usf`) em dois trechos, como nos Heterogeneous Volumes.
  - Marcha de 32 passos com a mesma extinção do cache (`ExtAt`).
  - Na leitura, o modelo é avaliado em 4 texels e misturado na transmitância: a bilinear da distância da frente erra na silhueta.
- **O cache de luz passa a RGBA16F**, com dois canais novos:
  - z: o sol pelas outras explosões (a própria sai pelo índice);
  - w: o chão embaixo à sombra de todas, incluindo a própria.
  - Os dois entram fora do lerp das oitavas, como a sombra do mapa.
  - Sem cache (presets baixos), a amostra lê os ladrilhos direto.
- **A luz que volta do chão** passa a usar a sombra do chão embaixo. Antes, a barriga da fumaça ficava clara dentro da sombra.
- **A poeira das lagartas** lê os ladrilhos no ponto e no chão embaixo dela.
- **Depuração e A/B:** `-RmDebug=5` mostra o sol pelas outras explosões; `-RmSmokeShadow=0` desliga a fase 2.

**Falta (fase 2b):** publicar um mapa composto (os ladrilhos somados, em ~512 × 140 no plano do sol) com a matriz, pros materiais das outras sessões (o rastro do míssil, o diesel). Depois, ler o chão por ele no lugar do `MainShadowMap`. Isso economiza a marcha atual de 256² (0,3–1 ms) e tira a falha da projeção oblíqua acima de ~13 m.

**No jogo desde 16/10 23:59** (a troca segura). **A prova direta (17/10):** duas mortes de tanque na mesma reta do sol, a 8 m uma da outra (`-ShadowFxTest=2`/`3`, cena `luz_fase2_67`):
- **Vista de depuração** (`-RmDebug=5`, o sol pelas outras explosões): a de baixo do sol sai preta e a de cima, branca.
- **A/B na vista normal:** com a fase 2, a de baixo fica cinza-escura na sombra da outra; sem ela, as duas saem acesas igual.
- As imagens: `captures/fluxo_noite_17/f2_direto_lado.jpg` e `f2d_dbg.jpg`.
- **A primeira prova não serviu:** os tiros na areia caíam onde a nuvem da morte já tinha se desfeito (~1,2 s), sem cruzar.

**Custo medido** (Alta, 1600×900; média / piores 5%):
- ladrilhos: 0,03–0,07 / 0,11–0,16 ms;
- o cache de luz ficou igual dentro do ruído: 0,17–0,29 / 0,72–0,82.
Na cena da vila com as duas mortes e a poeira (luz_prova_67), a fase 2 inteira fica abaixo de 0,2 ms.

**Riscos anotados:**
- dois emissores na mesma reta erram para o escuro no composto (nos ladrilhos não, cada um é separado);
- o ladrilho de uma caixa grande (30 m) fica com texel de ~0,6 m.

## A luz das explosões nas fumaças à noite (17/10, madrugada; a pergunta da Arte)

**O sintoma:** de noite (desert_night), a poeira e a fumaça da própria morte de tanque ficavam azul-cinza de lua, sem o laranja do fogo. O chão em volta ficava laranja.

**O que foi medido** (cena `poeira_noite_morte2`, Path 1, `-SceneLightsLog=1 -RmDustLog=1`):
- A luz chega às fumaças: 1–3 luzes de 1,3–2,7 unidades (cd ÷ 128000).
- Essa noite é estilizada, uma "lua clara". A exposição é 8,05e-5, quase a do dia (6,95e-5), e a compensação do fogo sai 1,69, sem cortar nada. A lua forte apaga o laranja.
- A luz gravada da bola da morte (`eg_tq_v2`) tem pico aos 0,36 s da gravação e acaba em ~1 s: é o clarão. Depois disso a fumaça só tem a lua até a carcaça acender, aos 4 s. O fogo da própria morte na fumaça ×4 (`-RmFireGain=4`) não mudou nada visível.

**O que mudou:**
- **O alcance das luzes nas fumaças × 3** (`RTSSceneLights`). O raio da Unreal (7–9 m) é corte de desempenho; a luz física não acaba. `-SmokeLightReach=1` volta.
- **A luz do fogo da carcaça acende 0,8 s depois da morte**, e a fumaça e as chamas seguem aos 4 s. Na vida real o diesel queima embaixo logo depois da bola. `-WreckLightDelay=4` volta. O casco e o chão ficam laranja desde 0,8 s; na fumaça quase não aparece, porque a luz é pequena e baixa.

**Descartado** (a Arte apontou): dar à luz pontual o ganho do céu. O céu é radiância de um hemisfério, e o 2π sai de integrar a fase. A pontual é irradiância de uma direção, e o espalhamento múltiplo dela já vem pelas oitavas.

**Falta:** o fogo da própria coluna da carcaça em volume (a emissão e a luz gravada) é o que vai pintar a fumaça por baixo. Ou uma noite mais escura, que é decisão do Look e do Felipe.

**Interruptores pra A/B:** `-RmFireGain=`, `-RmFireLightExpo=`, `-SmokeLightUncomp=1`.

## A poeira das lagartas em resolução de tela (17/10, manhã)

**O defeito:** a marcha conjunta roda a 0,42 da tela no Alta. A poeira saía "fora de foco" perto dos tanques nítidos, e os cartões tinham a borda dos bolos definida (a Arte, na poeira_sombra_wf).

**O A/B:** com tudo em resolução inteira (`-RmRes=1`) os gomos aparecem, mas custa +2,0 ms só com poeira e +3,5 ms com as explosões grandes na tela. Com 11 mortes juntas, o pior quadro passaria de ~40 ms.

**O que entrou:** a poeira sai da marcha conjunta e marcha dentro da composição, pixel a pixel, só onde o raio cruza a caixa justa dela (`MarchDustFull`).
- É a mesma luz, o mesmo passo e o mesmo salto dos vazios.
- O TSR acumula o ruído, porque a composição vem antes dele, com a mesma tremida.
- A ordem com a fumaça das explosões vai pela profundidade média de cada uma.
- `-RmDustFull=0` volta.

**O resultado:**
- A nitidez fica igual à da resolução inteira (`captures/game/df_wf`).
- Na WF, só com poeira: 1,04 ms, contra 0,93 na meia resolução e ~3,0 em tudo inteiro.
- Com as mortes (`luz_prova_67`): o total fica igual dentro do ruído, e a ordem com as explosões não mostra defeito.
