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

## Poeira do andar (13/10/2026, a arte, com o Felipe fora)

O rastro de poeira atrás das lagartas, item do `docs/BASE_JOGAVEL.md` (§2). O jogo definiu que é da arte com o FXLab.

**Como os jogos fazem (estudo):**
- **Zero Hour** (`W3DTankDraw.cpp`, o código liberado pela EA):
  - um emissor por lagarta (`TreadDebrisLeft/Right`, os sistemas `TrackDebrisDirtLeft/Right`), preso no tanque;
  - liga só com o tanque andando (velocidade² > 0,00001) e fora da névoa;
  - a velocidade escala a quantidade (velocidade + 0,1, até 1) e a velocidade das partículas (0,5 × velocidade + 0,1, até 1);
  - prioridade baixa: é o primeiro efeito a sair quando falta orçamento.
- **Jogos de hoje** (o WARNO, `TrailSpawner`, e o Niagara, `SpawnPerUnit`; o CoH3 não tem fonte pública sobre isso):
  - partícula por distância andada, e não por tempo, pra o rastro sair igual em qualquer quadro;
  - a cor pelo tipo de chão;
  - cartão de fumaça gravado com luz 6-way.

**O alvo:** as cenas aprovadas (`steppe_gpt`, `town_battle_top`, `road_battle`, a neve em `captures/concept/fx_refs/snow_walk/`).
- O rastro é baixo, nasce no pé da metade de trás da lagarta e fica abaixo do casco perto do tanque.
- É claro e quente (contraluz), ralo, junta as duas lagartas e tem 1 a 2 tanques de comprimento andando rápido.
- Na neve: branco, grosso e curto.

**Como está feito** (`FXLab/web/fxtread.js` é a especificação; `FXTread.cpp` repete linha a linha, mesma semente):
- cartões da Dust Impact (`dust_impact4`, JangaFX CC0), quadros 16 a 50 (a nuvem já aberta, que se desfaz sozinha);
- 1 cartão a cada 0,4 m andados por lagarta, num ponto sorteado da metade de trás dela;
- jogado pra trás (1,5 m/s × a regra do ZH) e pros lados (1,8 m/s), com arrasto, vento e um pouco de queda;
- cresce de ~2,9 m a ~9 m em 2,6 s; denso junto da lagarta e ralo na cauda;
- por chão (areia, neve, lama, asfalto): o tom, a claridade, a densidade, a vida, a quantidade, a opacidade e o tamanho.

**Pro jogo:**
- um ator só (`AFXTreadDust`) pra todas as unidades;
- a ponte chama `SetSources()` a cada quadro com os veículos que andam (id, posição, frente, velocidade, chão);
- ele desenha uma malha por tipo de chão, com o mesmo material `M_FXSixWay` e o cozimento `dust_impact4` (o mesmo do tiro no chão).

**Laboratório:**
- navegador: `?fx=tread[&ground=snow|mud|asphalt]` (o tanque dá voltas, a câmera segue);
- Unreal: `fxlab.ps1 shot -Extra "-Fx=tread -SunElev=22 [-Ground=snow]"`.
- O chão do navegador é a areia procedural `captures/fxlab/sand_lab.jpg` (a foto do WorldForge saiu do Intermediate).

**Vídeos:** `captures/fxlab/tread_sand_v1.mp4` e `tread_sand_v2.mp4` (6 s). Navegador e Unreal dão a mesma história (`captures/fxlab/shots*/tread_*`).

**Decisão do Felipe (13/10):**
- A poeira das 3 gameplays em vídeo (`captures/concept/33_video/`; recortes em `captures/concept/fx_refs/tread_dust_video/`) é melhor que a feita à mão. Ela é densa, granulada e rola em bolos baixos.
- **Gerar no EmberGen** um efeito bem parecido. A versão feita à mão **fica guardada como plano B**.
- O Felipe não quer a sessão do BlastLab nisso ("deixa o cara focar na explosão"): a arte operou o EmberGen sozinha.

**A versão EmberGen (13/10, a ativa):**
- **O projeto:** `captures/embergen/tread/tread_puff_v1.ember`, a `poeira_chao_v4` do BlastLab com:
  - o emissor em toro de 0,9 m a 0,35 m do chão;
  - voxel de 5 cm (192³, domínio de 9,6 m);
  - pressão 150 %/s.
- **A simulação:** VDB em `captures/vdb/tread_dust/puff_v3` (90 quadros, 30 qps, só densidade e temperatura).
- **Os cartões:** gravados com `bake_sixway.py` (`captures/fxlab/bake/tread_puff_v3`, 32 quadros de 256 px, lado 7,9 m). O pivô foi medido no 1º quadro.
- **O ajuste:** `FXLab/tread.json`. O plano B é o `FXLab/tread_handmade.json` (`?treadProfile=` no navegador, `-TreadProfile=` na Unreal).
- **Como operar o EmberGen por script**, sem mexer no mouse:
  - o `desk.py` e o `eg.py` do BlastLab, em `C:\Users\luisf\explosion-lab\tools\desk\`, com o mapa em `EMBERGEN_MAPA.md`;
  - o `art/tools/eg_set.py` (muda campos e exporta, rolando o painel pro topo antes);
  - o `art/tools/eg_back.py` (manda a janela pra trás);
  - regras: nunca com outro EmberGen aberto; fechar pelo PID.
- **Vídeos no navegador:** `captures/fxlab/tread_embergen_v1.mp4` (a nova) e `tread_sand_v2.mp4` (o plano B). A folha é `captures/fxlab/tread_compare.jpg`.
- **Na Unreal, no deserto do jogo** (o Felipe: "no webgl não vou saber, precisa ser no Unreal, com textura nossa"):
  - o mapa é o `World_Desert` (a fatia do WorldForge, com a luz, a exposição e a câmera do jogo);
  - o tanque do grupo 1 dá voltas de 14 m seguindo o chão (um raio de cima pra baixo), e a câmera segue.
  - Comando: `fxlab.ps1 shot -Map World_Desert -Cam game -Extra "-Fx=tread -Load=0 [-TreadProfile=tread_handmade.json]"`.
  - Vídeos: `captures/fxlab/tread_game_eg.mp4` (EmberGen) e `tread_game_hand.mp4` (plano B); a folha é `tread_game_compare.jpg`.
  - O cozimento entra na Unreal com `fxlab.ps1 import -Name tread_puff_v3`.

**No mapa do jogo de hoje (13/10, o Felipe: "pega o tanque que estamos usando hoje e a luz e as texturas"):**
- **Cópia do que o jogo usa**, só leitura do Game/:
  - o mapa `World_TestSalt2` e o conteúdo dele (Slice, Village, Structures, Materials, Textures);
  - o tanque `Units/USA_MBT_LL` (o da maquete do Look, o mesmo do `Units.json`);
  - o plugin `RTSLook` (`FXLab/Plugins/RTSLook`, cópia; o dono é o Look).
- **A luz:** a receita do mapa (`desert_golden_hour_v2_aprovada`) pelo `ALookDirector`, como o `RTSGameModes.cpp` faz.
- **O asfalto:** a camada "Asphalt" do Landscape, lida das texturas de peso no editor (`BuildAsphaltGrid`).
  - Em cima do asfalto a areia não levanta poeira (o Felipe: "poeira de areia não surja no asfalto").
  - No jogo, o tipo de chão tem de vir da simulação: a grade do mapa ainda não tem isso.
- **Comando:** `fxlab.ps1 shot -Map World_TestSalt2 -Cam game -Extra "-Fx=tread -Load=0 -GameLight -GameTank -LookRecipe=desert_golden_hour_v2_aprovada [-TreadProfile=tread_handmade.json]"`.
  - A volta é de 14 m no cruzamento do centro.
- **Os dois pontos que separavam do vídeo:**
  - a cor: a poeira saía com claridade 0,81 contra 0,60 da areia; agora o tom é o da areia (tint 1,0/0,64/0,38, albedo 2,6);
  - a altura: o cartão achatado na vertical (`squashUp` 0,65) e menor ao nascer.
  - Valem nos dois perfis.
- **Vídeos:** `captures/fxlab/tread_gamemap_lado_a_lado.mp4` (EmberGen × plano B); a folha é `tread_gamemap_compare.jpg`.

**O estudo de 13/10 à tarde e o plano novo** (o Felipe: "estudar como é de verdade, se inspirar, entender, planejar e discutir"):
- O plano está em `docs/POEIRA_DAS_LAGARTAS.md`; o estudo inteiro, em `captures/fxlab/tread_study/` (fora do git).
- O alvo principal é a `r1_raid_slabs` (escolha do Felipe, 13/10). O vídeo v2 de perto é a referência de onde a poeira nasce.
- As duas versões de antes estão guardadas em `FXLab/tread_versions/v1_13-10/`.

**A poeira em camadas (13/10 à noite, o plano `docs/POEIRA_DAS_LAGARTAS.md`):**
- **O código** (`FXTread.cpp`): um perfil com `layers` liga a poeira nova; sem `layers` é a de 13/10, igual (as duas guardadas em `FXLab/tread_versions/v1_13-10/`).
  - Camadas K1 (o jato atrás da roda motriz), K2 (a nuvem, com máscara de comprimento em cascos e a esteira), C (a cortina ao longo da lagarta); cada uma nasce na caixa dela, no referencial do tanque, com pivô no fundo da poeira (`ball_bottom`), o quadro pela idade (`times_s`), deslocada pra câmera (`camOffset`) e a curva de opacidade do WARNO.
  - O chão é lido no pé de trás de cada lagarta, com peso contínuo (o asfalto bilinear); a areia presa de cada lagarta esvazia com τ = 0,27 s (`loadTau`) e só o jato segue ela.
  - A sombra do tanque na poeira: cada canto do cartão vê se o casco ou a torre tapam o sol (UV3.y, o canal que o material já usa).
  - A sombra da nuvem no chão (`shadow` na camada): `M_FXDustShadow` (Modulate), um cartão deitado esticado pra longe do sol (`FXLab/Tools/make_dust_shadow.py` cria o material).
- **A câmera dos alvos** (`-Cam=r1`, `-Cam=v2`, `-Cam=v2close`): presa ao tanque numa reta só de areia (`FindPathLine`, longe de árvore, pedra e casa), com o sol na mesma relação do alvo; a distância sai do tamanho do casco na tela.
- **O jato novo do EmberGen:** `captures/embergen/tread/tread_kick_v1.ember` (caixa arredondada de 0,36 × 0,64 × 0,6 m no chão, pressão 35 %/s, ruído de 1,5 m com 2 oitavas, empuxo 300 % com 600 K, peso 100 %).
  - Medido no VDB, antes de cozinhar (`FXLab/Tools/vdb_measure.py` no Blender + `tread_vdb_sheet.py`): 1,0 × 1,3 m e 1,0 m de altura aos 0,2 s; um monte de 2,0 × 2,3 m com 1,1-1,2 m até os 2 s.
  - O cozimento `tread_kick_v1` (33°, lado 4,2 m, 48 quadros mais densos no começo: o argumento novo de potência do tempo no `bake_sixway.py`).
- **Os perfis:** `tread_v2_ember.json`, `tread_v2_ember_s.json` (com a sombra no chão), `tread_v2_handmade.json`.
- **A gravação de perto:** `FXLab/Tools/tread_baseline.ps1 -Profiles ... -Tag ...` (r1 e v2close, pela fila, vira mp4 em `captures/fxlab/tread_base/`); o alvo de perto do v2 é `FXLab/Tools/tread_v2crop.py` (o T1 seguido).

