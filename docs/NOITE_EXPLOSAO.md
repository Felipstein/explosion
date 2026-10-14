# A noite de 13/10: a morte de tanque pelo combustível, do zero (sessão noturna)

Pedido do Felipe (13/10, 01:00, antes de dormir): uma explosão "de arrepiar" pro RTS, realista e nos gameshots
aprovados, feita em silêncio e sem copiar a sessão do BlastLab. Alvos: `road_town_battle`, `usa_vs_china` e
`r1_raid_slabs` (as 3 da morte de tanque pelo combustível).

Tudo foi feito numa **cópia isolada** do repositório (`git worktree` em `C:\Users\luisf\explosion-night`, branch
`night-explosion`, a partir do commit `fde1478`), pra não encostar nos arquivos que a sessão do BlastLab edita.
Nada foi commitado. O que ficou bom entra no repositório principal só depois do Felipe ver.

## 1. O método (o que os grandes fazem, aplicado aqui)

Estudo em 5 lentes + um diretor (workflow de agentes, 01:15–01:50; ficha em `BlastLab/Tools/night/FICHA_NOITE.md`):
anatomia real, técnica AAA (Battlefront II, Frostbite, Nubis, CoD; Realtime VFX), receita no EmberGen, o nosso
renderizador linha a linha, e a **medição dos 3 alvos** (em comprimentos de tanque e cores sRGB).

O achado que organizou a noite: **os 3 gameshots são 3 instantes de uma explosão só**, não 3 estilos:
road_town ≈ 0,5 s (bola branca-amarela de 1,2 tanque encostando no chão), r1 ≈ 1,0 s (laranja sem branco, 1,25 T,
coluna de areia 2,1 T), usa_vs_china ≈ 1,5 s de uma carga ~10× maior (a referência de COR da poeira em contraluz).
E o que mais faz "arrepiar", medido: (1) miolo clipado em branco com bolhas nítidas, (2) **poeira de areia acesa
pelo sol por trás** (borda 1,2–1,7× a areia, sombra 0,65–0,7×, R/B 2,2–3,6), (3) fuligem escura MISTURADA
(≥ 25 % da silhueta, marrom quente, nunca cinza), (4) 25–40 destroços + 10–20 riscos de faísca + torre,
(5) ritmo: branco a 0,5 s, laranja a 1 s, nuvem de 2 T.

Diagnóstico do renderizador (lente do render): a poeira de hoje recebe o sol × sen(elevação) (o "erro aprovado" da
fuligem preta do WebGL), albedo 0,78 cinza, céu em 1/4π e nenhuma rebatida do chão: a 20° ela sai 0,4–0,66× a
areia ao lado, e os alvos pedem 0,9–2,6×. **Não era o EmberGen, era a luz da poeira no shader.**

## 2. O que foi feito

### 2.1 Simulações no EmberGen (do zero, template Default Explosion, domínio 256³ a 0,16 m = 41 m)

Operado por script (mensagens de janela; `BlastLab/Tools/night/egn.py`). Duas gravações do MESMO projeto e das
mesmas forças, como os AAA fazem em camadas:

| rodada | o que mudou | medido no VDB (`night_vdbstats.py`) |
|---|---|---|
| A1 | template como vem | bola 13 m a 0,3 s, 40 m de fumaça rala enchendo a caixa; fogo morre a 0,6 s |
| A2 | fuel loss 6 %/quadro, combustão 4800 K, fuligem da queima 40 % | bola gigante (40 m), fogo até 1,8 s: grande demais |
| A3 | fuel 120, pressão 60, gás 30 %, fuel loss 12 | bola 10 m a 0,5 s mas fogo morre a 0,7 s |
| **A4** | pressão 40, gás 10 %, empuxo 150 %, calor 3600 K, dissipação 12 %/s | **bola 9,4 m a 0,5 s; nuvem 14 × 14 m a 1 s; chapéu 29 m a 3 s** (alvo: 9–11 / 13–17 / 24–32) |
| A5–A7 | fogo mais longo (fuel loss 7 → 4, extinção 2160 → 1400 K) | ver §3 |
| B1 | a areia: cilindro r 6 m no chão, só fumaça (400 %/s por 0,35 s), peso 8 %, dissipação −0,2/s | some em 1,5 s (dissipação demais) |
| **B2** | peso 3 %, dissipação −0,03/s, r 4,5 m | saia 20 m a 1 s, assenta em ~2,5 s |

**A receita que ficou (projeto `captures/embergen_night/night_tank_A4.ember`, salvo com o estado final A7 + B3):**
- Domínio 256³ a 0,16 m (41 m), chão sólido, Default Explosion como base; Timestep 60 Hz; export 105 quadros com stride 2 (3,5 s).
- Fogo (Emitter: Volume + Shape Sphere r 2,0 m a z 2,0 m): fuel 90 %/s, fumaça 0, temperatura 2900 K (Replace), pressão
  40 %/s, ligado nos 15 primeiros quadros (0,25 s, as keyframes do template).
- Combustão (avançada): O₂ no combustível 40 %, O₂ nas chamas 100 %/s, ignição 1800 K, ganho 3600 K/unidade, perda de
  combustível **4 %/quadro** (o fogo até ~1 s; é por QUADRO, não por segundo), gás 10 %, extinção a 1400 K, fuligem da
  extinção 50 %, progressivas 50 %. Dissipação: temperatura 12 %/s, fumaça 8 %/s e −0,03/s. Força: empuxo 150 %/1000 K,
  peso da fumaça 4 % (a areia assenta), gravidade 100 %.
- Areia (2º Emitter: Volume + Shape Cylinder r 7 m, altura 1,5 m, z 0,3 m): só fumaça, 300 %/s por 0,35 s (uma vez),
  sem temperatura nem combustível (areia fria não sobe sozinha: o térmico do fogo é que a arrasta — a couve-flor de
  areia da usa_vs_china/r1). Exportada como passada B (o fogo da A sem fuligem: extinção 0 %).
- A e B saem do MESMO campo de velocidade (emissores de volume são determinísticos): só o que emite fumaça muda.

### 2.2 O renderizador (cópia): a poeira acesa pelo sol, atrás de uma chave

`-RmDust=1` liga SÓ no ramo da poeira (a fuligem fica exatamente como aprovada): sol inteiro (sem o × sen),
céu do hemisfério (`-RmDustSky`, padrão 6; usado 4), rebatida do chão de areia (`-RmDustBounce`, albedo da areia do
look 0,49/0,40/0,27), albedo 0,92–0,95 (poeira mineral, AERONET/Dubovik 2002) e a cor por ordem de espalhamento
(cada oitava leva mais uma vez o espectro 1/0,974/0,944: a borda fica da cor do sol, o interior laranja).
Arquivos: `BlastLab/Shaders/Private/BlastRaymarch.usf`, `Source/BlastLabShaders/Private/BlastRaymarch.cpp`.

### 2.3 As camadas (cópia)

- arma nova "Morte no Deserto" (`night`, índice 8): a gravação `night_kill` (trocada por `-RmSwap=night_kill:<nome>`),
  torre e destroços da morte de tanque, sem a coluna antiga nem as bolas da carcaça (`-Plume=0 -WreckBalls=0`);
- torre: 12,5 → 18 m/s (a arte 4.3: torre a 11 m aos 0,8 s), 4 m/s de lado;
- faíscas: 60 espinhos a 40–90 m/s → 22 a 25–55 m/s (os alvos têm 10–20 riscos de 0,2–1 m; os 60 viravam fogos de artifício);
- a vítima na bancada do Look: o tanque pelo nome da malha (`SM_LL_USATank_*`), `-AutoAt=x,y` e `-CamAt=x,y` nos testes.

### 2.4 A conversão (VDB → gravação do jogo)

`night_bake.ps1 convert` (Blender 5.2, só CPU): 128³ em 40 m (31 cm/voxel) e a versão 96³ pro jogo; fogo com
TEMP_K pela temperatura em Kelvin (0,0006: 3000 K → 0,72 da gravação, miolo satura em branco a ≥ 3600 K), fuligem
× 1,5; areia como camada `poeira` × 1,2 e fundida com `brm2_merge_dust.py` (mesmo domínio, atraso 0, ganho 1).

## 3. Resultados (medidos na captura, câmera do jogo, `night_measure.py`)

Metas (dos alvos): miolo branco Ø 0,35 T a 0,3–0,5 s; bola 1,2 T a 0,5 s, laranja sem branco a 1 s; nuvem 1,7–2,1 T;
escuro 25–35 % da silhueta e fogo 20–35 % a 0,5–1 s; poeira ao sol 1,2–1,7× a areia (R/B 2,2–2,6), sombra 0,65–0,7×;
fumaça escura marrom R/B 2,3–2,9 lum 80–100; silhueta ≤ 8–10 % do quadro.

| teste | 0,4 s | 0,9 s | 1,3 s | o que diz |
|---|---|---|---|---|
| A4 só fogo (`night_A4`) | bola amarela-branca 1,2 T, miolo clipado; sil 5 % | fogo 1 %, escuro 77 % (bolo preto); fumaça lum 50 R/B 1,9 | escuro 59 % | o fogo morre cedo (0,7 s) e a fumaça é preta e opaca demais |
| A4 + areia B2 (`night_A4B2`) | poeira ao sol 1,37× a areia, sombra 0,64× (meta!), mas R/B 1,24 (branca) | escuro 47 %, poeira 52 % | sil 7,5 %; fumaça lum 61 R/B 1,9 | a LUZ da poeira já bate (claro/escuro); falta a COR da areia e a fuligem marrom; o fogo ficou deslocado do tanque (a conversão centrava pela caixa ativa) |

| hoje (arma 6 do repositório, `base_hoje`) | bola dourada pequena "pipoca" (volume antigo), sem poeira | bolo preto pequeno + estrela de faíscas | idem | a linha de base pra comparar (mesmo tanque, câmera e semente) |
| **A7 + B2 centrados, poeira cor de areia, fuligem marrom (`night_A7B2c`)** | (a 1ª foto saiu a 0,73 s por um engasgo) fumaça escura R/B 2,5 lum 73, poeira ao sol 1,24× / sombra 0,59× | escuro 46 %, poeira 52 %; fumaça R/B 2,3 (meta 2,3–2,9) | sil 6 %; coluna marrom sobe, a saia assenta | **a cor e a luz batem com os alvos; o fogo é curto e as faíscas ainda viram estrela** |
| A7 + saia larga B3, 96³ (`night_A7B3c96`) | a areia (× 1,0) esconde a bola: silhueta 90 % bege a 0,43 s | fogo 12–17 % (meta 20–35), escuro 9–15 % (meta ≥ 25): pouca fuligem à vista, muita areia | poeira ao sol 1,2×, sombra 0,65×, R/B 1,5 / 2,2 | a areia tem que ser mais rala no 1º segundo (× 0,4, atraso 0,1 s): a bola de fogo é a estrela |
| A7 fuligem × 0,5 + B3 × 0,4 atrasada 0,1 s (`night_A7dB3g96`) | igual: clarão branco + areia, sem a couve-flor | fogo 12 %, areia 75 % | areia 89 % | a areia ainda cobre a bola; e com a fuligem pela metade o fogo fica transparente (a emissão é ∝ fuligem) — voltar à fuligem × 1,0 e tirar a areia do miolo (`oco_m` no merge) |
| A7 + B3 oca (6 m), × 0,35, 96³ com a temperatura pesada pela fuligem (`night_A7cB3h96t`) | clarão + areia rala em volta; o fogo mal aparece | escuro 68 % (a fuligem da A7, τ 23, embrulha a bola) | escuro 52 %, poeira 46 % | a casca de fuligem da A7 é densa demais no 1º segundo: a A8 (fuligem da queima 15 %, × 4 na conversão) deixa o fogo à vista |
| A8 (fuligem da queima 15 %) × 4 + B3 oca (`night_A8dB3h96`) | clarão sem bola | fumaça pequena, fogo 1–2 % | — | fuligem de menos: a emissão é ∝ fuligem, e sem ela o fogo não aparece. **A A4 (emissor 4100 K, fuligem da queima 40 %, fuligem × 1,5) tinha a melhor bola (couve-flor amarela de 1,2 T a 0,43 s): é ela o fogo final**, centrada, + B3 oca |
| **A4 centrada + B3 oca × 0,35 (`night_A4cB3h96`, 96³, A ESCOLHIDA)** | couve-flor amarela de 1,2 T EM CIMA do tanque, miolo branco; sil 4 % | fumaça marrom quente R/B 2,4 lum 52 (meta 2,3–2,9 / 80–100: um pouco escura); escuro 67 % (meta 25–35: fuligem demais); poeira ao sol 1,1–1,5× a areia (meta 1,2–1,7) | escuro 48 %, poeira 49 %; sil 4 %; torre no ar; a coluna sobe e sai das unidades | bate em tamanho, ritmo, luz da poeira e cor da fumaça; falta: fogo visível de 0,5 a 1 s (o da A4 morre a 0,7 s) e menos fuligem no 1º segundo |
| variante de cor (`night_A4cB3h96_v2`: poeira albedo 0,88, céu 3, cor 0,95/0,62/0,30) | igual | poeira ao sol lum 200 → 170 (não estoura), mas R/B 1,6 (alvo 2,2–2,6): o laranja-areia dos gameshots ainda não vem; `sheet_v2.jpg` tem as duas lado a lado | | o tom da poeira é decisão do Felipe (mais saturada = mais "fantasia") |

Fotos: `captures/night/night_A4cB3h96/sim_*.png`; folha contra os alvos: `captures/night/sheet_A4cB3h96.jpg`; hoje × noite:
`captures/night/sheet_hoje_vs_noite.jpg`. Vídeos (30 qps, 6 s, mesmo tanque e semente): `captures/night/video_game.mp4`
(câmera do jogo), `video_close.mp4` (perto), `video_tilt.mp4` (deitada) e `video_hoje.mp4` (a morte de tanque de hoje, A/B).

Hitches de 400–600 ms nas fotos e "video memory exhausted" (700–1000 MB acima): a placa estava com EmberGen + 2–3 Unreal de
outras sessões. **Nenhuma medida de ms desta noite vale**; a versão 96³ (`*_96.brm2`) é a do jogo.

## 4. O que falta / o que o Felipe decide

**Defeito visto no vídeo e corrigido na cópia:** de 2,5 s em diante uma bola escura redonda em cima do casco (a fumaça do
"fogo da carcaça" do fluido do campo antigo, desenhada pelo volume antigo). `-WreckFire=0` desliga; o fogo da carcaça do
jogo é o do FXLab (cartões), que a bancada não tem.

**O que o vídeo final mostra** (`captures/night/ab_hoje_vs_noite.mp4`, hoje à esquerda × noite à direita; `video2_sheet.jpg`):
clarão e couve-flor amarela em cima do casco (0,2–0,4 s), a casca vira fuligem marrom com a areia acesa em volta
(0,7–1,0 s), a torre voa, a coluna sobe e sai das unidades (1,5 s) e aos 2,5 s só ficam os destroços e a carcaça: a
fumaça some cedo demais (dissipação 8 %/s + −0,03/s na simulação; nos alvos a nuvem está inteira a 1,5 s e a coluna
dura 3–5 s). Hoje: bola dourada de "pipoca" e uma coluna preta opaca até 5 s.

**Falta (por ordem do que mais muda a imagem):**
1. O fogo ainda é menor e mais curto que o dos alvos (bola laranja grande até 1 s): a A7 chega perto no tempo; falta
   volume de chama de 0,5 a 1 s (mais combustível por mais tempo: emissor 0,25 → 0,4 s) e a casca de fuligem mais rala
   no 1º segundo (a A7d testa fuligem × 0,5).
2. A nuvem some rápido demais depois de 2 s (dissipação da areia): nos alvos a 1,5 s ela ainda está inteira. Dissipação
   −0,03 → −0,015/s e 8 → 5 %/s.
3. A marca no chão (queimado + cratera) e a carcaça: a bancada do Look não tem o fogo da carcaça do FXLab; o jogo já tem a
   memória do chão (12/10). Juntar.
4. Sombra da fumaça no chão (mapa projetado, como as nuvens da Unreal; ≤ 0,2 ms) e a luz do fogo pintando a base da
   coluna (R/B ~3,5 nos alvos).
5. A luz da poeira (`-RmDust=1`) vale pra todas as explosões: re-render do arsenal contra a aprovada antes de ligar por
   padrão; se a Carga Pesada (poeira e fuligem no mesmo voxel) sair do piso, o modelo novo fica só na morte de tanque.
6. Medir o custo com a placa livre (nenhuma medida desta noite vale) e a versão 96³ no jogo (`Game/Source/BlastFx`).

**Arquivos tocados na cópia** (`git status` em `explosion-night`): `BlastLab/Shaders/Private/BlastRaymarch.usf`,
`Source/BlastLabShaders/Private/BlastRaymarch.cpp` (chaves `-RmDust*`, `-RmSootColor`, `-RmSootAlbedo`),
`Source/BlastLab/Private/BlastLabSimulation.cpp` (arma `night`, `-Plume=0`, torre 18 m/s, a vítima pelo nome da malha),
`BlastLabController.cpp` (`-AutoAt`, `-CamAt`), `Public/BlastLabCamera.h`, `BlastDebris.cpp` (22 espinhos, 36 fagulhas),
`Tools/build_raymarch_vdb.py` (`origem`), `Tools/brm2_merge_dust.py` (`oco_m`), `Tools/brm2_downsample.py` (temperatura
pesada pela fuligem), `BlastLab.uproject` (RTSLook) e a pasta `Tools/night/` (scripts da noite). Gravações finais:
`BlastLab/Intermediate/Raymarch/A4cB3h.brm2` (128³) e `A4cB3h_96.brm2` (o jogo).

**O Felipe decide:** (a) o tom da fuligem (marrom quente dos alvos × o preto neutro aprovado na Carga Pesada);
(b) se a poeira acesa pelo sol entra no padrão de todas as armas ou só na morte de tanque; (c) o tempo do fogo
(1 s "cinema" × 0,6 s físico); (d) levar a cópia (`explosion-night`) pro repositório: os arquivos tocados são
`BlastRaymarch.usf/.cpp` (chave `-RmDust`, `-RmSootColor`), `BlastLabSimulation.cpp` (arma, torre, tanque da bancada),
`BlastLabController.cpp` (`-AutoAt`, `-CamAt`), `BlastLabCamera.h`, `BlastDebris.cpp` (faíscas), `build_raymarch_vdb.py`
(centro na origem) e a pasta `Tools/night/`.

## 5. Manhã (13/10, 10:00): a explosão na maquete do Look

O Felipe, acordando: a bancada de caixas bege não serve (luz, atmosfera e texturas "horríveis"); usar a maquete realista
do Look. Feito: o conteúdo do jogo (`Game/Content/LookLab`, `Units`, `Slice`, `Materials`…) entrou na cópia do BlastLab por
links físicos (1 487 arquivos, sem gastar disco), a maquete `LL_DesertDay_Golden` abre no BlastLab com a luz dela; a vítima
é o tanque USA do meio do grupo (`look_scene.TANKS[1]`, −21,7 / −1,6 m) e a câmera é a do jogo da maquete (giro −45°,
61 m, 37,5°). Código: o chão em relevo da maquete (`SM_LL_Ground`) vale como terreno; as peças do tanque `SM_USA_MBT_*`.
Obstáculo: os shaders do cenário compilam por minutos na 1ª abertura (o "preparando" do teste esperava pra sempre e a
placa ficava presa; a sessão do BlastLab pediu pra fechar, fechei pelo PID). O cook sem placa não resolveu (chaves de
shader diferentes); entrou `-AutoShaderWait=240` (o teste segue depois de 4 min) e o preset alto.
Diagnóstico certo (11:30): não eram os shaders. O mapa da maquete traz o GameMode do jogo, que não existe no BlastLab, e a
Unreal caía no `GameModeBase`: sem o controlador do simulador, o teste nunca detonava (o log diz "Game class is
'GameModeBase'"). Correção: o mapa abre com `?game=/Script/BlastLab.BlastLabGameMode` (night.ps1).
1ª foto na maquete (11:53, `captures/night/sheet_look_v1.jpg`): abre em 7 s de shaders e detona em cima do tanque USA. Medido:
poeira ao sol 1,6–1,8× o chão com R/B 1,65–1,85 (alvo 1,2–1,7 e 2,2–2,6: perto); fumaça escura lum 40–54 = 0,4× o chão
(alvo 0,8–1,0×: preta demais) e 73–80 % da silhueta (alvo 25–35 %). Causa: a fuligem recebe o sol × sen(20°) = 0,34 (o
"erro aprovado" da Carga Pesada) e a conversão levou fuligem × 1,5. Rodada 2: `-RmSootSun=1` (sol inteiro na fuligem) e
fuligem × 0,8 (`A4eB3h_96`).
Rodada 2 (12:08, `sheet_look_v2.jpg`): fumaça marrom lum 74 = 0,76× o chão (alvo 0,8–1,0 ✓), R/B 2,0 (alvo 2,3–2,9), escuro
42–52 % da silhueta (alvo 25–35), poeira ao sol 1,6–1,8× o chão (alvo 1,2–1,7 ✓); mas a nuvem ficou rala e some em ~2 s.
Rodada 3 (nos vídeos): fuligem × 1,2 e areia × 0,6 (oca 5 m), `A4fB3i_96`.
Rodada 3 (12:20, nos vídeos `lookvid_*`, gravação `A4fB3i_96`): fogo 17–30 % da silhueta de 0,5 a 1,5 s (alvo 20–35 ✓), escuro
44–52 % (alvo 25–35), fumaça marrom lum 55–72 R/B 2,0, poeira ao sol 1,6–2,0× o chão (alvo 1,2–1,7) R/B 1,85–1,9; silhueta
4–6 % do quadro (✓ ≤ 8–10). Falta ainda: a nuvem some aos 3 s (nos alvos dura 3–5 s) e o tom da poeira (R/B 2,2–2,6).
**Esta é a versão a mostrar pro Felipe na maquete.** Chaves: `-RmDust=1 -RmDustSky=4 -RmDustColor=0.98,0.72,0.42
-RmDustAlbedo=0.95 -RmSootColor=0.62,0.50,0.38 -RmSootSun=1 -Plume=0 -WreckBalls=0 -WreckFire=0`.

## 6. Tarde (13/10): o casco voando e o estudo detalhe a detalhe

O Felipe viu o CASCO INTEIRO do tanque voar nos vídeos da maquete (eu tinha olhado o quadro e não vi: lição anotada,
conferir ampliado antes de dizer pronto). Causa: na maquete do Look o kit do tanque prende casco, esteiras e rodas como
FILHOS do primeiro ator do kit, que é o CANO (`look_lab.py tanks()`); ao arremessar torre + cano, o cano levava o tanque
inteiro. Correção (`BlastLabSimulation.cpp TankDeath`): o que não voa é solto do que voa (no mundo) antes do arremesso;
verificado em `captures/night/look_fix1` (casco no chão, torre voa). A torre saía de lado a 4 m/s e pousava no telhado
vizinho: agora 2 m/s. Em paralelo: estudo detalhe a detalhe (7 lentes + 3 revisores + diretor, workflow) sobre os meus
quadros × alvos × vídeo do Veo, e a simulação A9 (dissipação 8 → 4 %/s e −0,03 → −0,012/s, fuel loss 4 → 3 %/quadro) pra
nuvem durar 3–5 s e o fogo até ~1 s.

### 6.1 O que o estudo fechou (fatos conferidos no código, não de memória)

| Fato | Consequência |
|---|---|
| A luz do clarão nascia a 3 m, DENTRO da torre (2,9 m), com sombra de distance field; e nascia na CONSTRUÇÃO do ator, antes de a simulação ajustar τ/altura/fonte (o log `-FlashLog` mostrou 300 cm / 50 cm / 0,035 s) | chão 0,99× em todos os anéis e quadros: nada acendia. Agora: luz a 4,5 m, fonte de bola 1,5 m, τ 0,10 s, ganho pela meta dos alvos (`-FlashScale=`, 0,12 ≈ 1,1e7 cd), nascendo só depois do BeginPlay |
| `bLayersEnabled` nunca é verdadeiro e a arma `night` não tem receita no `BlastRecipes.json` | anel, queimado, faíscas-cubo e torrões do BlastEffects NÃO existem em quadro nenhum (nem na morte antiga); mexer na receita não muda pixel |
| O "_96" do jogo era 64³ (`brm2_downsample.py` faz res//2) e a gravação tocava a 0,875× (90 quadros a 26 qps) | conversão direta a 96 (jogo) e 128 (herói), a 30 qps (`A9k96`, `A9k128`, merges `A9kB3_96/_128`) |
| A erosão global do raymarch (0,042) apagava a coluna de 2 s em diante | erosão por gravação (cabeçalho `erode`, `InstTune` no shader; 0,008 na morte de tanque; o arsenal mantém 0,042) |
| Faíscas a 2e5 cd/m² atravessavam a fumaça preta (T 5 % sobra 1e4) | teto 4e4 cd/m² (`-DebrisGlow=`), faísca só acende ao sair da bola, espinhos só a < 35 m; cometas mais lentos (14–24 m/s) com rastro cinza curto em vez das linhas brancas de giz |
| A luz do fogo gravada tinha matiz fixa (3600 K) e pico 83 (4,2e7 cd) | cor por corpo negro por quadro (`kelvin=1 kscale=0.7`, 2000–2400 K efetivos) e pico 14 (~7e6 cd) |
| Casco pintado, nada de carcaça | casco/esteiras/rodas viram carvão quente (M_Base por dados primitivos) + luz stand-in da carcaça (0,9e6 cd, 2000 K, curva NIST 90 s); o fogo e as poças de verdade são da sessão do FXLab |
| A gravação acabava em corte seco | fade de 0,5 s no fim, só na morte de tanque (`EndFadeS`) |
| Chapas de 0,4–1,4 m ilegíveis; torre sem rastro | 6 chapas de 1,4–2,2 m com rastro + 18 pequenas; torre com brasa no rastro (0,6 s) |

Medição: `Tools/night/death_measure.py` (um script só; folha ao lado das células do Veo). Passo fixo 30 qps sempre
(as fotos "0,5 s" saíam a 0,76 s por engasgo de shader).

Gate de jogabilidade (medido em `look_fix2`): a saia de areia da B3 sobe à altura da bola e tapa os vizinhos
(correlação 0,10 aos 1,5 s, meta ≥ 0,5): a geometria certa é a passada B4 (torus baixo); A/B barato enquanto ela não
existe: saia achatada no merge (`brm2_merge_dust.py ... 0.5`, `A9kB3f_96`).

### 6.2 Passada EmberGen A5 (14:10, fora da fila, pedido do Felipe) e o clarão medido

`captures/embergen_night/night_tank_A5.ember` (o A4 com a receita única do diretor): fuel loss 3 %/quadro, extinção de
fuligem 15 → 30 %, dissipação da temperatura 10 %/s, fumaça × 5 %/s e − 0,005/s (era 8 e −0,03: a nuvem sumia aos 2 s),
vorticidade 25 → 50 % com máscara de temperatura 600–4000 K, pressão do emissor 40 → 25 %/s; export 150 VDB (5,0 s a
30 qps, 2 min de placa) em `captures/vdb/A5`. Não feitos nesta passada: cook-offs E2/E3 (emissores novos pelo painel) e o
emissor ON até 0,40 s (o arrasto do keyframe não pegou); a areia B4 em torus (fica a B3 + saia achatada no merge).
Conversão: `bake_A5.ps1` → `A5kB3_96`, `A5kB3f_96` (saia achatada), `A5kB3_128`; fuligem × 1,5.

O clarão, medido na maquete (`death_measure.py`, anel de areia ao sol 0,5–1 T / 1–2 T contra o quadro de antes):
- luz dentro da torre com sombra DF (manhã): 0,99 / 0,99;
- a 4,5 m, fonte 1,5 m, 1,1e7 cd, COM sombra DF (`look_fix3`): 0,996 / 0,998 — a sombra de distance field do relevo do
  chão e do casco come a luz;
- sem sombra, 5,5e6 cd (`look_fix3b`, `r.DistanceFieldShadowing 0`): 1,05 / 0,99;
- sem sombra, 3,7e7 cd, τ 0,07 s (`look_fix4`): 1,08 / 1,02 aos 0,03 s; a luz do fogo gravada (7e6 cd) dá 1,19–1,27 a
  0,5–1 T de 0,5 a 1,0 s; as sombras do sol a 1–2 T acendem 1,3–1,9×.
Por que não chega aos 1,4–1,6× do Veo: a receita do Look (`desert_golden_hour.json` › post) tem **exposição local**
(`local_exposure_highlight` 0,9, `detail_strength` 1,35) e ombro de filme 0,32: qualquer mancha clara no chão é
comprimida de volta ao tom da areia; só as sombras sobem. Os gameshots aprovados medem 1,11–1,13× a 0,5–1 T: a bancada
já está nesse nível. Pra ter o creme do Veo é preciso uma exceção no Look durante a explosão: **decisão do Felipe**.

### 6.3 Rodadas na maquete (tarde, 14:00–15:00), medidas pelo `death_measure.py` na câmera do jogo

| Render | O que muda | 0,33 s: fogo / escuro / miolo | 0,5 s: fogo / escuro | 1,0 s: fogo / escuro | vizinho a 1 T aos 1,5 / 2 s | coluna 3 s |
|---|---|---|---|---|---|---|
| meta (road/r1/Veo) | | 0,2–0,4 / 0,25–0,35 / Ø 0,25–0,45 T | 0,2–0,35 / 0,25–0,35 | 0,1–0,3 / 0,25–0,4 | ≥ 0,5 / ≥ 0,7 | escuro ≥ 15 % da silhueta |
| `look_fix3` (A9, 96³, código novo) | luz fora do casco, erosão 0,008, faíscas com teto | 0,44 / 0,16 / 0,01 | 0,32 / 0,46 | 0,28 / 0,42 | 0,67 / 0,87 | sil 1,5 %, escuro 54 % |
| `look_fix5` (A9 + saia achatada 0,5) | gate de jogabilidade | 0,35 / 0,16 / 0,01 | 0,20 / 0,52 | 0,13 / 0,51 | **0,87 / 0,94** | sil 1,4 % |
| `look_fix6` (A5, fuligem × 1,5) | receita única do EmberGen | 0,22 / 0,52 / 0,02 | 0,14 / 0,60 | 0,16 / 0,52 | 0,86 / 0,94 | sil 1,4 % |
| **`look_fix7` (A5m, fuligem × 0,6, TEMP_K 0,001)** | τ na bola 2,5 (meta 2–4), miolo quente | **0,39 / 0,28 / 0,23** | **0,26 / 0,39** | **0,29 / 0,22** | 0,88 / 0,95 | sil 1,4 %, escuro 56 % |

Lição: a conversão (fuligem × e TEMP_K) pesa tanto quanto a receita; a A5 com × 1,5 saiu preta (bola 60 % escura), com × 0,6
bate as metas de fogo/escuro de 0,3 a 1,25 s e o miolo branco chega a Ø 0,23 T. O que ainda falta na A5m: a coluna de
2–5 s (a fuligem some por DILUIÇÃO, a caixa cresce 8× e a densidade cai 8×, não é dissipação) → passada **A6** = A5 +
emissor do fogo da carcaça no próprio EmberGen (cópia do emissor, esfera do anel da torre, 30 %/s a 1800 K, pressão 0,
ligado de 0,3 s a 5 s): a fuligem continua nascendo e a coluna fica. Vídeos A/B: `captures/night/look_ab_hoje_vs_fix7.mp4`.

### 6.4 Passada A6 (15:00–15:30): o fogo da carcaça dentro da própria simulação

Por que: na A5 a fuligem some por diluição (a caixa cresce 8× entre 0,5 e 3 s); o Veo tem coluna até 6 s porque o fogo da
carcaça CONTINUA produzindo fuligem. Solução no EmberGen: um 2º emissor (cópia do do estouro, Ctrl+C/Ctrl+V no grafo,
mesma esfera do anel da torre) com combustível 8 %/s, temperatura 2000 K (acima da ignição de 1800 K), pressão 0,
desligado durante o estouro (keyframe OFF no quadro 0) e ligado do quadro 18 (0,3 s) até o fim. Passadas:
- A6 (30 %/s, 1800 K, ligado desde 0): fuligem p99 0,13 constante até 5 s (meta ≥ 0,08 aos 3 s ✓) mas o fogo virou uma
  tocha de 25–33 m de largura e o estouro saiu pela metade (o emissor novo sobrescrevia a temperatura do estouro);
- A6b (8 %/s, 1600 K): sem estouro nenhum (1600 K < ignição 1800 K sobrescrevendo a esfera);
- A6c: idem, porque o emissor do ESTOURO tinha sido alterado sem querer (ver lição abaixo) pra 90 %/s e 2900 K;
- **A6d** (estouro 140 %/s / 4100 K restaurado; carcaça 8 %/s / 2000 K, OFF no quadro 0, ON do 18): a que vale.
Lições da automação do EmberGen (13/10): (1) um duplo clique num campo-slider que NÃO entra em edição põe o valor no
MEIO da faixa (foi assim que o estouro virou 90 %/s e 2900 K): sempre ler o painel antes e depois (`setf.py` espera
1 s e confere a borda azul); (2) a seleção de nó por clique falha às vezes (estado de arrasto preso depois de ligar fios):
salvar e reabrir resolve; conferir a seleção pelo painel (a cor do botão "Export Now"); (3) a pasta de exportação às vezes
não grava: conferir o painel antes do "Export Now" (a A6d saiu na pasta A6c e foi movida).
Projetos: `night_tank_A5.ember` (A5), `night_tank_A6d.ember` (A6d). VDB: `captures/vdb/A5`, `A6`, `A6b`, `A6c`, `A6d`
(os A6/A6b/A6c podem ir pra Lixeira).
- Causa real das A6b/A6c/A6d sem estouro (achada na A6e, 15:20): ao ligar a forma no emissor novo, o arrasto MOVEU o fio
  da forma que alimentava o emissor do estouro (ele ficou sem forma e não emitia nada); religado, a A6e tem o estouro
  (228 m³ de chama a 0,5 s, 5472 K; 115 m³ a 1,0 s: chama(1,0)/chama(0,5) = 0,5, meta ≥ 0,4 ✓) e a fuligem segura
  (p99 0,16 a 0,5 s, 0,10 a 1,5 s, 0,037 a 3 s, 0,06 a 5 s; a caixa da fumaça 27×31×39 m aos 3 s). **A6e é a que vai
  pro render** (`A6emB3f_96`). Projeto: `night_tank_A6e.ember`.

### 6.5 `look_fix8` = A6e na maquete (15:35): a primeira com começo, meio e fim

| instante | fogo / escuro na silhueta | miolo branco | vizinho a 1 T | topo da coluna | observação |
|---|---|---|---|---|---|
| 0,33 s | 0,32 / 0,20 | Ø 0,86 T (meta 0,25–0,45: demais → A6e2 com TEMP_K 0,0008) | 0,90 | 1,25 T | bola amarela cheia, chão a 0,5–1 T 1,00× |
| 0,5 s | 0,29 / 0,28 | Ø 0,69 T | 0,79 | 1,05 T | coluna de chama saindo do casco, tampa preta |
| 1,0 s | 0,18 / 0,52 | 0 | 0,79 | 1,87 T | casca preta, fogo nas frestas |
| 1,5 s | 0,12 / 0,52 | 0 | 0,87 | 2,65 T | coluna preta subindo (meta T ≥ 0,5 ✓) |
| 2,0 s | 0,08 / 0,51 | 0 | 0,95 | 2,75 T | coluna 1,8 T de largura, vizinhos legíveis |
| 3,0 s | 0,19 / 0,45 | 0 | 0,93 | 2,55 T | **a coluna existe** (sil 2,0 %) e o casco queima (fogo 19 %) |
| 4,0 s | 0,24 / 0,45 | 0 | 0,94 | 2,3 T | fogo da carcaça + coluna rala (sil 1,6 %) |
| 6,0 s | 0,14 / 0,52 | 0 | 0,95 | — | a gravação acabou (fade aos 4,5–5 s); fica a luz da carcaça |
Torre pousa a ~8 m do casco (log); casco no chão em todos os quadros; 0 pontos claros dentro da fumaça de 0,5 s em diante.
Vídeo A/B: `captures/night/look_ab_hoje_vs_fix8.mp4`; folha: `sheet_fix8.jpg`; medidas: `medidas_fix8.txt`.

## 7. Resumo pro Felipe (13/10, 15:45): o que ver, o que mudou, o que decide

**Ver** (em `captures/night/`): `look_ab_hoje_vs_fix9.mp4` (esquerda a morte de hoje, direita a nova, mesma câmera e
semente), `sheet_fix9.jpg` (quadro a quadro ao lado do vídeo do Veo, com os números), `ab_hoje_vs_nova_sheet.jpg`.

**O que mudou desde o casco voando** (tudo medido na câmera do jogo, maquete do Look):
1. Casco, esteiras e rodas ficam no chão; só torre e cano voam; a torre pousa a 8 m (log), não no telhado.
2. A explosão tem começo, meio e fim: bola amarela com miolo branco (0,3–0,5 s), casca preta com fogo nas frestas (1 s),
   coluna preta subindo e saindo de cima das unidades (1,5–3 s), casco carbonizado queimando com luz própria (2–5 s).
   Antes a nuvem sumia aos 2,5 s e sobrava uma rua limpa.
3. As faíscas não atravessam mais a fumaça preta e não há mais linhas brancas de giz (rastros dos cometas).
4. Jogabilidade: os tanques vizinhos a 1 T ficam legíveis (correlação 0,8–0,95 aos 1,5–2 s; antes 0,1): a saia de areia
   ficou baixa; a silhueta da explosão ≤ 8 % da tela; nenhuma unidade coberta por mais de 1,5 s.
5. A gravação toca em tempo real (30 qps) e o jogo vê 96³ de verdade (antes 64³ a 0,875×).

**O que ainda não está no alvo** (e por quê):
- Miolo branco: com a conversão mais fria (`look_fix9`, TEMP_K 0,0008, gravação `A6enB3f_96`) o miolo fica Ø 0,54 T a
  0,33 s e 0,30 T a 0,5 s (meta 0,25–0,45): **é a versão a avaliar** (`look_ab_hoje_vs_fix9.mp4`, `sheet_fix9.jpg`,
  `ab_hoje_vs_nova_sheet.jpg`). Fogo 0,41 / 0,33 / 0,19 / 0,17 da silhueta a 0,33 / 0,5 / 0,83 / 1,0 s (metas 0,2–0,4,
  0,2–0,35, 0,15–0,35, 0,1–0,3); escuro 0,36 a 0,5 s e 0,51 a 1 s (metas 0,25–0,35 e 0,25–0,4: um pouco de fuligem a mais).
- O chão em volta não acende como no Veo (1,4–1,6×): a luz existe (medida), mas a exposição LOCAL da receita do Look
  comprime o clarão de volta ao tom da areia; os gameshots aprovados medem 1,1×, e a bancada está em 1,0–1,1×. Ter o
  creme do Veo = uma exceção no Look durante a explosão: decisão sua.
- A coluna acaba aos 5 s (fim da gravação, com fade); a coluna de 5–20 s é a próxima rodada (continuação no raymarch).
- O fogo da carcaça é um stand-in da própria simulação (uma língua de ~1 T): o fogo e as poças de verdade são do FXLab
  (sessão de arte), ainda não pedidos.
- Custo em ms não medido hoje (placa sempre ocupada por outras sessões); fica pra fechar com a placa livre.

**16:15, decisão do Felipe (via sessão do BlastLab):** a branch `night-explosion` fica guardada como está, sem merge em main
("eu falei pra manter salvo ali"). O "aprovado por enquanto" vale pro visual da morte de tanque; quando e como juntar com a
morte de tanque da sessão do BlastLab é decisão dele. Próxima rodada pedida: a coluna de 5–20 s e o custo em ms.

### 6.6 Custo medido (16:30, fila da placa, EmberGen fechado): a morte de tanque nova (A6e) na câmera do jogo, 1600×900

`profilegpu` aos 1,0 s (o pico da bola), passe `BlastRaymarch`:

| preset | resolução da marcha | raymarch (GPU) |
|---|---|---|
| alta | 536×300 (⅓ da tela) | **0,79 ms** |
| ultra | 800×447 (½ da tela) | **13,3 ms** |

Memória: atlas de blocos 108 + 22 MB (pool de 29³ blocos, 113 MB). O tempo de quadro do teste automático saiu travado
em 33,3 ms porque o teste roda com passo fixo (`-UseFixedTimeStep`): pra custo vale o `profilegpu`; `night.ps1 -NoFixed`
tira o passo fixo nas medições. O ultra a 13 ms está MUITO acima do teto de 2,5 ms da FICHA (meta 15): antes de qualquer
coluna longa, descobrir o que o ultra liga (cache de luz com sombra do fogo, passos, meia resolução) e onde cortar. A
"alta" cabe. O custo da fase de coluna (3–20 s) ainda não foi medido (o dump de 3,0 s coincidiu com o fim do teste).
Aos 3,0 s (fase de coluna, caixa da fumaça 27×31×39 m): **alta 1,20 ms, ultra 17,4 ms**: a coluna custa MAIS que a bola
(caixa maior na tela). A coluna de 5–20 s tem que ser mais barata por instância (menos passos na fase segurada, caixa
menor pelo fade do topo), senão 6 colunas vivas = 7 ms só em "alta".
Correção (cost_report.py, 16:50): nos dois dumps a memória de vídeo rastreada passava da placa (alta 6,2 GB, ultra
9,2 GB "Total tracked", pool de render targets 1,8 / 3,2 GB; a 1660 tem 6 GB): os outros editores Unreal abertos nas
outras sessões seguram VRAM, e o número do ultra (13–17 ms) é PAGINAÇÃO, não o shader. Medida válida só com a VRAM
sã (≤ 5,5 GB rastreados, cabeçalho de cada medida). Outro achado: o passe "Lights" custa 1,3 ms em alta (5,4 no ultra)
com uma morte: a luz do fogo com sombra de distance field + a do clarão + a da carcaça; a medir por luz.
Por que o ultra custa 15×: a marcha roda a ½ da tela (`-RmRes=`, padrão 0,5) em cima da porcentagem de tela do preset
(alta ≈ 67 % → 536×300; ultra 100 % → 800×447 = 2,2× os pixels) e só o ultra liga o cache de luz com a sombra do fogo
dentro do volume (`FullDetail`, `-RmLightCaches=`, "~6 ms na morte de tanque" em 07/10). A/B a fazer com a placa livre:
ultra com `-RmLightCaches=0` e com `-RmSteps=` menor, pra saber o que paga o quê; fora do escopo desta rodada.

## 8. Rodada da coluna de 5–20 s (13/10, 16:30–)

**Estudo** (3 lentes + 3 refutadores + diretor, `scratchpad/a9/coluna_plano.txt` da sessão): três fatos medidos mudaram
o COMO: (1) a gravação A6e não tem velocidade (os VDB só trazem density/temperature/flames: todos os bytes de velocidade
do brm2 são 128); (2) os últimos quadros são um caule estacionário (massa constante nos últimos 10 quadros) com a
textura subindo a 6–18 m/s; um ping-pong de 20–30 quadros mostraria a fumaça DESCENDO metade do tempo; (3) na tela a
coluna de 3–6 s sai 3–5× mais fraca que o VDB (τ 2 a 8–12 m no VDB, nada acima de 0,5 T na tela): causa a achar antes de
confiar no hold (passo offline, zero placa).

**O mecanismo (feito, compilado):** opt-in por instância (`HoldS`, 0 no arsenal aprovado: o caminho antigo fica bit a
bit igual). Com `-HoldS=15` a morte de tanque vive 20 s: depois do fim da gravação o raymarch segura o fim dela por um
dos três segmentos (`-RmHoldMode=loop|flow|pingpong`: loop pra frente dos últimos 10 quadros com emenda de 0,15 s;
um quadro + flow map no volume com fase por amostra; vai-e-vem curto sobre 6 quadros), o ruído de detalhe passa a andar
no tempo REAL (antes congelava com o quadro), a velocidade vira uma pluma analítica (Heskestad u ∝ z^(−1/3) de 12 → 5 m/s
em τ 3 s, abertura MTT 7°, entranhamento 0,1 u, vento `-RmWind=`), a densidade decai com τ 6 s (afina pelas bordas, pela
erosão absoluta) e apaga de vez nos últimos 3 s; a chama gravada apaga em τ 2 s; passos × 0,66 no hold; a luz gravada
apaga no hold (fica a da carcaça, teto de 8 FIFO). Política pra N mortes: `-HoldMax=` colunas em hold na tela (3 por
padrão); passando, a mais velha recebe o corte (fade contínuo de 1,5 s). Na seleção das 16 instâncias: na tela > em
curso > hold mais novo > mais perto (a composição continua por distância). Teste: `-AutoMultiEvery=<s>` escalona N
mortes em volta do herói. `ScreenFraction` corrigido (caixa toda atrás da câmera = 0, não a tela toda).
Arquivos: ExplosionVolume.h/.cpp, BlastRaymarch.h/.cpp, BlastRaymarchSubsystem.cpp, BlastRaymarch.usf,
BlastLabSimulation.cpp, BlastLabController.h/.cpp; `Tools/night/run_hold_look.ps1` (rodada 1), `death_measure.py`
(metas de 5–20 s, coluna por trecho contíguo, vizinhos atrás, IoU/fluxo), `cost_report.py`, `hold_offline.py`.

**Offline (hold_offline.py, zero placa, 17:00):**
- A perda de contraste da coluna (3–5×) tem causa: a **erosão absoluta** do raymarch (0,008 nesta gravação): a coluna
  tem fuligem S ≈ 0,02–0,04 e tirar 0,004–0,008 absolutos come 20–25 % no miolo e 50–80 % nas partes ralas (pé e topo);
  a interpolação entre quadros em espaço √ (sem velocidade gravada o BakeAtMV é um lerp puro) perde mais 6–25 %. A
  reamostragem 0,16 → 0,42 m não perde massa (1,000), o limiar tira só o véu, 8 bits < 1 %, BoxFade 0. O próprio VDB só dá
  τ 1,1–1,6 pelo miolo a 37,5° (transmitância 20–33 %: a coluna já nasce fina). Remédio em teste: erosão 0,008 → 0,002
  nesta gravação (`-RmErode=0.002`, rodada 1b), A/B contra o fix9 também no 0–5 s; lerp linear é mudança global (depois).
- Candidatos do hold (sequências sintéticas de 2 s): **B (flow map)** sobe sempre (4,7–5,5 m/s em todas as alturas),
  textura mais contínua (corr 0,87), mas silhueta parada (IoU 1,00: o risco "congelada"); **A (loop 110–119)** silhueta
  viva sem pulsar demais (IoU 0,85 a 1 s, pulso 24 %), fluxo sempre pra cima (4–7 m/s), mas um salto de velocidade na
  emenda a cada 0,52 s; **C (ping-pong)** rejeitado (metade do tempo a fumaça desce); **A20** rejeitado (respira 51 %).
  Os dois pro vídeo: B e A; o ping-pong fica como controle. Pluma analítica calibrada no quadro 119: meia-largura
  1,9 m (0–4 m) → 2,7 m (8–12) → 3,75 m (16–20) → b0 ≈ 2 m + 0,09 z (o shader usa 3 + 0,12 z: um pouco larga).

**Rodada 1 (17:30, 3 candidatos + mapa de depuração, `captures/night/hold_*`):** o hold funciona (a instância vive 20 s,
a torre pousa, a luz da carcaça fica), mas a coluna de 5 s em diante é um fiapo bege de 0,25–0,3 T de largura e 1,1–1,4 T de
altura, e some aos 8–10 s; vizinhos atrás 0,92–0,99 (legíveis demais: não há coluna). O render de depuração (`-RmDebug=1`,
`hold_tau`) mostra a coluna INTEIRA (0,5 T de largura, 2,5 T de altura, de 3 a 10 s): os dados existem; no render normal
ela some porque (1) é rala (τ ~1 a 37,5°) e (2) a fuligem rala acesa pelo sol inteiro (`-RmSootSun=1`, cor 0,62/0,50/0,38)
fica da cor da areia: contraste zero. **Rodada 1b** (erosão 0,002): a coluna de 3–4,5 s volta a aparecer (1,3–2 T de largura,
topo 2,5–2,8 T) e de 6 a 15 s fica um véu bege mais visível, mas ainda bege. Fila: **1c** (gravação `A6esB3f_96`: fuligem
× 2,5 de 2 → 4 s, erosão 0,002) e **1d** (a mesma com `-RmSootSun=0.35`: menos sol na fuligem, núcleo escuro como a arte
type_4_3: 0,12× o chão no núcleo, 0,85× no lado ao sol).

**Rodada 1c (17:45, `A6esB3f_96`: fuligem × 2,5 de 2 → 4 s, erosão 0,002, loop):** a coluna EXISTE e lê como fumaça
preta de carcaça da câmera do jogo: 3 s largura 0,9/0,7 T (0–1/1–2 T), topo 4 T; 4,5 s 0,54/0,62/0,73 T, escura 0,27/0,38 T;
6 s 0,51/0,48/0,59 T; 8 s 0,45/0,42/0,54 T; 10 s 0,40/0,40 T, topo 1,8 T; 12 s 0,31/0,27 T; 15 s 0,18 T (véu); 18–20 s nada.
Silhueta 5,9 % (3 s) → 2,0 % (6 s) → 0,6 % (15 s). O 0–2 s é idêntico ao fix9 (a rampa começa em 2 s). **Defeito:** cobre
demais as unidades atrás: correlação 0,75/0,47/0,57 (1/2/3 T atrás) aos 6 s, 0,78/0,83/0,44 aos 4,5 s (meta ≥ 0,5 em hold,
≥ 0,7 em curso). Próxima: `A6etB3f_96` (× 1,8 em vez de 2,5) e, se precisar, τ da densidade 6 → 4 s. Incidente: as 5 rodadas
de vídeo de 20 s (600 PNG cada) encheram o disco (0 GB, aviso da sessão do Look): apaguei 6,5 GB de quadros meus; os
scripts passam a apagar os quadros logo depois do mp4.

**Rodada 1e (18:10, `A6etB3f_96`: fuligem × 1,8 de 2 → 4 s, erosão 0,002):** o candidato **loop** passa no gate de
jogabilidade: vizinhos atrás (1/2/3 T) 0,82/0,90/0,71 aos 4,5 s, 0,82/0,73/0,78 aos 6 s, 0,89/0,88/0,90 aos 8 s; coluna
0,49/0,54/0,62 T (4,5 s), 0,45/0,42/0,56 T (6 s), 0,36 T (8 s), 0,32 T (10 s), 0,26 T (12 s), véu aos 15 s; topo 3,5 T (4,5 s) →
2,9 T (6 s) → 1,8 T (8 s) → 1,1 T (10 s); silhueta 3,5 % → 1,4 %. Os quadros mudam 6–10 % dos pixels a cada 0,5 s (a textura
vive). O candidato **flow map** saiu praticamente PARADO (0,5 % dos pixels por 0,5 s): o jitter de fase por amostra (0,5
do período) espalhava as duas cópias por todas as fases; corrigido pra 0,15 (Vlachos), a re-renderizar. O flow de ×2,5
(1c) foi perdido com o disco cheio (quadros cortados). **Escolha de hoje: loop × 1,8.** Entregas: `captures/night/
coluna_loop_16s.mp4`, `coluna_loop_densa_20s.mp4` (× 2,5, cobre demais), `coluna_sheet.jpg`.

**Rodada 2 (18:30, custo e N mortes; alta, 1600×900, `cost_report.py`; VRAM rastreada 5,9–6,0 GB: no limite da placa,
medir de novo com os outros editores fechados):**

| cena | raymarch 3 s | 6 s | 10 s | 15 s | 20 s | luzes | hitch |
|---|---|---|---|---|---|---|---|
| 1 morte (`cost_hold`, loop × 1,8) | 1,39 ms | 0,68 | 0,58 | 0,56 | — | 0,9–1,1 ms | 0 |
| 10 mortes em 10 s (`n10_hold`, 1/s, política teto 3) | — | — | 6,47 ms | 4,41 | 3,66 | 0,9–1,5 ms | 0 |

Uma coluna em hold custa **0,56–0,68 ms** (meta ≤ 1,0 ✓); a fase ativa da coluna (3 s) 1,39 ms. Com 10 mortes o pico
(6,5 ms aos 10 s) vem das explosões AINDA ATIVAS (as de 5–9 s, 1,0–1,4 ms cada): a política cortou as colunas em hold
além de 3 (7 cortes no log, fade de 1,5 s), mas o raymarch ainda marcha todas as ativas com os passos cheios (o LodFloor
da simulação não chega ao raymarch: 56/37/28 passos por classe é a próxima alavanca); aos 20 s (3 colunas + rabos) 3,7 ms.
Atlas 30³ blocos (125 MB), nunca cheio; 0 engasgos; os tempos de parede saem travados a 33,3 ms (passo fixo).
Legibilidade com 10 mortes (1/s, anel de 22 m em volta do herói, `n10_hold`, fotos): união das silhuetas 21,6 % da tela aos
5 s (meta ≤ 25 % ✓), 14,8 % aos 10 s, 5,6 % aos 15 s, 3,6 % aos 20 s; escuro na tela 17,7 % aos 5 s (meta ≤ 10 %: ✗ na
fase das bolas, que a política não governa), 15,2 % aos 10 s, 7,0 % aos 15 s ✓, 2,7 % aos 20 s ✓; aos 15 s todos os
vizinhos legíveis (0,9–1,0). A política cortou 7 colunas (teto 3 em hold na tela) sem pop visível nas fotos.

**Estado ao fim da rodada (18:40):** coluna de 5–20 s = `-HoldS=15 -RmHoldMode=loop` + gravação `A6etB3f_96`
(fuligem × 1,8 de 2 s em diante, erosão 0,002). Pendências: re-render do flow map com o jitter 0,15 (A/B pro Felipe
escolher em movimento); passos por classe no raymarch (56/37/28) pra 10 mortes caberem em 4 ms; custo do ultra com a VRAM
sã; arsenal aprovado conferido no `compare.py` (bit a bit igual por construção, HoldS = 0; render de confirmação pendente);
porte pro jogo (BlastFx) quando o Felipe decidir a integração; vento do mapa (RTSLook) e a fumaça fina de 15 s+ do FXLab.

**19:05, pausado pelo Felipe.** Estado: tudo compilado na branch `night-explosion`; aprovado "por enquanto" a morte de
tanque (`look_fix9`); a coluna de 5–20 s entregue em vídeo sem veredito; a rodada "fumaça × veículos" ficou no estudo
(interrompido antes do plano do diretor) com o flag de teste `-AutoDrive` já no código. Nada em main além deste relatório.
