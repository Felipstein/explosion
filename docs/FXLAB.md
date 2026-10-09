# FXLab: as explosões do jeito dos jogos

Projeto de teste das explosões, separado do BlastLab (pedido do Felipe, 07/10/2026).
Usa o método do mercado: a simulação é feita uma vez (EmberGen/JangaFX, CC0), gravada
em cartões com luz de 6 direções ("six-way lighting": Battlefront II, Unity VFX,
exportação do EmberGen), e o jogo só toca os cartões. Fogo e fumaça são a mesma gravação.

## Onde está cada coisa

- `FXLab/web/`: o laboratório no navegador (ajuste rápido, segundos por mudança).
  `fxfire.js` é a especificação da lógica; `fxlab.js` desenha; `uefilmic.js` é a curva de
  tela da Unreal 5.8 (a do jogo), portada do fonte da engine.
  Rodar: `python FXLab/web/devserver.py 8131` e abrir http://localhost:8131/FXLab/web/
- `FXLab/` (Unreal 5.8): a mesma lógica em C++ (`Source/FXLab/Private/FXWreckFire.cpp`, linha a
  linha igual ao `fxfire.js`, mesma semente) e o mesmo cálculo de luz no material
  (`Tools/setup_fxlab.py`, `SIXWAY_HLSL`). `Tools/fxlab.ps1 compile | setup | view | shot`.
- `FXLab/Tools/bake_sixway.py`: grava a simulação (VDB) em cartões 6-way no Blender (CPU).
  `FXLab/Tools/atlas_meta.py`: mede o cozimento (onde a bola nasce, fundo da bola por quadro,
  quadro em que o fogo apaga); os dois lados leem essas medidas.
- `FXLab/preset.json`: o ajuste ativo (o laboratório salva, a Unreal recarrega sozinha).
  `FXLab/presets/`: os aprovados.
- Cozimentos em `captures/fxlab/bake/` (fora do git).

## Aprovado

**Fogo da carcaça** (`presets/wreck_fire.json`, cozimento `gas_puff3`), 08/10/2026: "é o
melhor até agora! deixa salvo pra gente usar no modelo do jogo final quando o tanque estiver
explodido". Bolotas de fogo nascem no anel da torre (uma a cada ~0,7 s, mais as lambidas
curtas no anel), ganham casca com rachadura acesa e viram a fumaça preta da coluna, que se
desfaz tocando a própria simulação. Sombra entre cartões (a coluna se sombreia).

Conferência navegador × Unreal (mesma semente, passo de 1/30 s): mesma história quadro a
quadro nas câmeras baixa e de RTS. A cor é a mesma porque o laboratório usa a curva da
Unreal. Sobra o halo do brilho (o do navegador é mais avermelhado; Unreal `ueBloom` 1.2)
e o chão longe um pouco mais escuro na Unreal (luz do céu diferente).

Pra levar pro jogo falta: emissor em Niagara (hoje é C++ com malha procedural, que dá o
mesmo resultado), atlas em BC7/BC6H em vez de ponto flutuante cheio, e a escala de
quantidade (várias carcaças ao mesmo tempo).

## Em teste: tiro de tanque no chão (arte 4.1), 08/10/2026

`FXLab/impact.json` + `FXLab/web/fximpact.js` (especificação) = `FXImpact.cpp` na Unreal.
Camadas, cada uma com a técnica de mercado:
- nuvem de poeira: cozimento 6-way da Dust Impact (JangaFX, CC0), `dust_impact4`;
- anel de choque: Dust Shockwave (JangaFX, CC0) gravado de cima (`shockwave_top`) num cartão
  DEITADO no chão. Em pé, a metade de baixo afundava no chão e sobrava um arco; de câmera baixa
  (< ~25°) ele some e a nuvem em pé faz o papel;
- jato de areia: 18 mil grãos balísticos com arrasto e rastro de movimento, agrupados em 14 raios
  (os raios escuros da arte), mais 130 pedras em arco, gravados no Blender
  (`FXLab/Tools/bake_debris.py`, `debris4`);
- o anel esticado 1,3× e deslocado 1,2 m pra frente do tiro: a poeira tem direção (o crescente
  da arte), não é uma rosca simétrica;
- clarão com luz, faíscas esticadas pela velocidade, pedrinhas que ficam no chão e cratera.
- partícula macia (soft particles): o cartão some perto de onde encosta (Unreal: DepthFade).

Conferência navegador × Unreal: mesma história e mesma cor (o navegador convertia a cor do sol
de sRGB duas vezes e a poeira saía laranja; corrigido em 08/10).

Contra a arte (08/10, manhã): raios e pedras do jato e a direção da poeira já entram. A cena de teste com o
sol a 10° (fim de tarde, a do fogo) deixa o chão escuro na câmera de RTS; com o sol a 30° fica
perto da luz da arte. Teste com outra luz sem mexer no ajuste: Unreal `-SunElev=30 -Sun=5`,
laboratório `&sunElev=30&sun=5`.

## No mapa do jogo: o deserto do WorldForge (08/10/2026)

`fxlab.ps1 worlds` copia a fatia do deserto do WorldForge (`Content/Slice` e o mapa, como
`/Game/Maps/World_Desert`, fora do git; o WorldForge só é lido) e o `Tools/setup_world.py`
põe três grupos (tanque, fogo no anel da torre, tiro 9 m à frente) em chão plano de areia,
achado com raios de cima (só o Landscape conta, sem pedra nem planta no raio de 5 m).
Diferente do BlastLab, a luz da fatia **fica**: é a do jogo (sol de 120 mil lux a 20°,
atmosfera, céu da SkyLight, névoa, exposição travada em EV100 13,6).

Como os cartões acendem com a luz do mapa (`FXSceneLight.h`, sol marcado "GameSun"):
- o sol: no material, `SkyAtmosphereLightIlluminance` (o que chega no ponto depois da atmosfera,
  em lux, o mesmo que acende o chão); a direção do sol, a do sol do mapa;
- o céu: `SkyLightEnvMapSample` pra cima com rugosidade 1 (a radiância média do céu da SkyLight)
  × π = a iluminância num plano deitado, a mesma unidade do `SkyCol` do laboratório;
- o fogo, o clarão e as luzes deles têm luz própria: × 1/exposição do mapa
  (`2^(EV100 − bias)`, a conta da `PostProcessEyeAdaptation.cpp`), então saem na tela com o
  valor aprovado no laboratório (exposição 1). No laboratório nada muda (SceneLight = 0).

A câmera é a do jogo (`Game/Config/DefaultGame.ini`, valores do Zero Hour): 37,5° de
inclinação, 45° de giro, 50° de campo, 61,1 m de altura (31,6 perto, 81,6 longe).
`fxlab.ps1 view -Map World_Desert` (painel: câmeras, carga 1+1 / 3+3 / sem efeito, ms do
quadro), `shot -Map World_Desert -Cam game -Extra "-Load=3"` (fotos com passo fixo; o relógio
só anda depois dos shaders compilarem) e `measure -Map World_Desert` (sem efeito, 1+1 e 3+3:
mediana e p95 do quadro, da placa, do jogo e do render em `captures/fxlab/measure_<nome>.txt`).

Trava da placa entre as sessões: a fila de `docs/GPU.md` (o `fxlab.ps1 shot` e o `measure` entram nela).
