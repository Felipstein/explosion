# Créditos obrigatórios

Lista só o que as licenças **obrigam** a creditar ou avisar no jogo. Atualizar
a cada asset ou trecho de código de terceiros que entrar no projeto.
Levantado em 07/10/2026, com as páginas oficiais das licenças. Não é parecer
jurídico.

## Na tela de créditos do jogo

| Asset | Onde no projeto | Licença | Linha pra colar nos créditos |
|---|---|---|---|
| Unreal Engine 5.8, com o conteúdo da Epic que vem com ela (árvores de amostra do PVE, `/Engine`, material glTF) | BlastLab e WorldForge inteiros | [EULA da Unreal](https://www.unrealengine.com/eula/unreal), seção 6a. Vale se o jogo tiver tela de créditos (um RTS vai ter) | `[NOME DO JOGO] uses Unreal® Engine. Unreal® is a trademark or registered trademark of Epic Games, Inc. in the United States of America and elsewhere.`<br>`Unreal® Engine, Copyright 1998 – 2026, Epic Games, Inc. All rights reserved.` |
| Palmeiras da PlantCatalog (e-on software) no Sketchfab: 5 "Realistic HD Date palm" (6, 16, 36, 53 e 73 de 78) e 1 "California fan palm" (20/25), baixadas em 07/10/2026 | oásis do WorldForge (`WorldForge/Intermediate/Assets/date_palm/`) | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) | Uma linha por modelo:<br>`Based on "Realistic HD Date palm (6/78)" (https://sketchfab.com/3d-models/none-a021bc39f3d64995888ec9457afec1c1) by PlantCatalog (https://sketchfab.com/PlantCatalog), licensed under CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/). Modified: mesh simplified, leaflets rebuilt, materials merged.`<br>
`Based on "Realistic HD Date palm (16/78)" (https://sketchfab.com/3d-models/none-2f34e3f54bef40eb8d94ca6507c2f822) by PlantCatalog (https://sketchfab.com/PlantCatalog), licensed under CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/). Modified: mesh simplified, leaflets rebuilt, materials merged.`<br>
`Based on "Realistic HD Date palm (36/78)" (https://sketchfab.com/3d-models/none-8805967d4c88457b98195c2f944d5ae4) by PlantCatalog (https://sketchfab.com/PlantCatalog), licensed under CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/). Modified: mesh simplified, leaflets rebuilt, materials merged.`<br>
`Based on "Realistic HD Date palm (73/78)" (https://sketchfab.com/3d-models/none-5ab22f161e4547d5aa2d602cbcf8e829) by PlantCatalog (https://sketchfab.com/PlantCatalog), licensed under CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/). Modified: mesh simplified, leaflets rebuilt, materials merged.`<br>
`Based on "Realistic HD Date palm (53/78)" (https://sketchfab.com/3d-models/none-a8706e6ce8f94f26a97329c2f7610384) by PlantCatalog (https://sketchfab.com/PlantCatalog), licensed under CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/). Modified: mesh simplified, leaflets rebuilt, materials merged.`<br>
`Based on "Realistic HD California fan palm (20/25)" (https://sketchfab.com/3d-models/none-17d6502be5df4fe5aa074e071f4a25e0) by PlantCatalog (https://sketchfab.com/PlantCatalog), licensed under CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/). Modified: mesh simplified, leaflets rebuilt, materials merged.`<br> |

- Unreal: o texto é o do PDF oficial da EULA. A página atual não abriu daqui,
  mas o monitor ConductAtlas mostra a mesma cláusula em 05/10/2026. Fica em
  inglês, igual à EULA, e o ano é o do lançamento.
- PlantCatalog: a CC BY exige autor, título, link, licença com link e **dizer
  que modificamos** (reduzir polígonos conta). A confirmar ao baixar: comparar
  com o botão "Copy Credits" do Sketchfab e ajustar o "Modified:" ao que for
  feito de fato.

## No arquivo de avisos que vai junto do jogo (não na tela)

Arquivo tipo `ThirdPartyNotices.txt`. Os três MIT só valem se o céu e o
tonemap do BlastLab forem pro jogo. O texto da MIT vai uma vez, com as linhas
de copyright em cima.

| Código | Onde no projeto | Licença | O que pôr |
|---|---|---|---|
| Simplex noise 3D, Ashima Arts e Stefan Gustavson | céu noturno `M_Sky` (`BlastLab/Tools/build_scene.py`, `snoise`) | [MIT](https://github.com/stegu/webgl-noise/blob/master/LICENSE) | `webgl-noise: Copyright (C) 2011 by Ashima Arts (Simplex noise); Copyright (C) 2011-2016 by Stefan Gustavson.` + texto da MIT |
| Hash without Sine, David Hoskins | `M_Sky` (`hash11`, `hash12`) | MIT, Shadertoy [4djSRW](https://www.shadertoy.com/view/4djSRW). A confirmar no Shadertoy, que bloqueia leitura daqui; conferi o cabeçalho numa cópia | `Hash without Sine: Copyright (c) 2014 David Hoskins.` + texto da MIT |
| Ajuste ACES de Stephen Hill, via BakingLab (MJP e David Neubelt) | tonemap `M_TonemapWebGL` (`build_scene.py`) | [MIT](https://github.com/TheRealMJP/BakingLab/blob/master/LICENSE) | `BakingLab ACES fit by Stephen Hill: Copyright (c) 2016 MJP.` + texto da MIT. São só 2 matrizes e 1 fórmula, talvez nem precise; o aviso custa uma linha |
| Advanced Terrain Erosion Filter / Phacelle Noise, Rune Skovbo Johansen | gerador do WorldForge (`ErosionFilter.h/.cpp`, módulo Runtime) | [MPL 2.0](https://www.mozilla.org/en-US/MPL/2.0/) | Só se o gerador for dentro do executável (mapa aleatório na partida): publicar o fonte desses 2 arquivos (§3.2) e avisar onde: `Advanced Terrain Erosion Filter, Copyright (c) 2025 Rune Skovbo Johansen, MPL 2.0. Source: <link>`. Se só os mapas prontos forem pro jogo, nada |
| Bibliotecas e fontes da engine (ex.: Roboto, Apache 2.0, nos painéis) | jogo empacotado | as de `Engine/Source/ThirdParty/Licenses` (EULA 6b) | Na hora de empacotar: os avisos só do que entrar no build |

## Outras obrigações

- **Cano do tanque USA** (`SM_USA_MBT_Barrel`): saiu do Tripo pelo AssetHub
  grátis, que não permite uso comercial
  ([AssetHub §4(1)](https://assethub.io/terms-of-service),
  [Tripo §5.2.1](https://www.tripo3d.ai/terms)). Crédito não resolve. Refazer
  a peça à mão ou por script (como a esteira), ou gerar de novo num plano
  pago; assinar depois não vale pro cano já gerado (AssetHub §6).
  **Refeito por script em 07/10/2026** (`BlastLab/Tools/blender_barrel.py`:
  torno pelo perfil da ficha, com textura nossa). A troca na Unreal acontece
  na próxima reimportação do tanque, com o app fechado. Os arquivos do Tripo
  não podem ir pro commit nem pro jogo.
- **Casco, torre e rodas** (Hunyuan3D, site grátis): não pedem crédito. Os
  [termos do site](https://docs.qq.com/doc/DSHRnRWp2YUVuQXJv) pedem avisar
  que é gerado por IA e proíbem dizer que foi feito à mão. A Steam também
  exige declarar o conteúdo de IA que vai no jogo, o que inclui a pintura
  projetada do Gemini. A confirmar qual site foi usado: a anotação "grátis,
  20/dia" bate com o Global (hy3d.tencent.ai). O modelo aberto do Hunyuan,
  se um dia rodar local, proíbe usar as malhas na UE, no Reino Unido e na
  Coreia do Sul.
- **Unreal**: a EULA do nosso jogo tem de isentar a Epic de garantias (EULA
  §3). Logo e "Powered by Unreal" só com licença de marca. Se vender: 5% de
  royalty acima de US$ 1 milhão e o formulário de lançamento.
- **PlantCatalog**: não dar a entender que a e-on apoia o jogo. A CC BY
  (§2(a)(5)(B)) veda DRM que impeça o uso livre das palmeiras; a confirmar
  com pak criptografado, na hora de empacotar.

CC0, sem crédito: os VDBs da JangaFX (EmberGen), as 46 texturas e modelos do
Poly Haven (com a coleção Namaqualand) e o TireTracks001 do ambientCG. Também
não pedem crédito os plugins da Epic, Gemini, GPT Image, Hunyuan3D (site) e
Sonniss (som, ainda não usado).

Feitos por nós, sem crédito: a textura do elo da esteira (`BlastLab/Tools/make_track_link.py`, procedural), a esteira (`blender_track.py`) e o fogo da carcaça (simulação no Mantaflow do Blender, `blender_pyro_wreck.py`).
