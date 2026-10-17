# O tiro e o acerto no tanque

O pedido do Felipe, 14/10, madrugada (v3; frente da sessão Fluxo):
- o acerto no tanque fiel à arte 4.2;
- a areia que a onda de choque levanta no tanque atingido;
- a areia que o sopro do disparo levanta em volta da torre;
- o projétil e a luz dele.
"De arrepiar, AAA, RTS". E: "TEM QUE ESTAR IGUAL NA JOGATINA".

**Alvos:**
- **acerto:** `docs/concept/explosions/type_4.2_armor_sparks.webp` (Gemini, a escolhida);
- **tiro no chão:** `type_4.1_sand_impact.webp`;
- **clarão de boca:** `docs/concept/scenes/desert_golden_hour/best/close_game_cam_gpt.webp`;
- **traçantes:** laranja e curtos, em `road_battle_gemini.webp`, `town_battle_top_gemini.webp` e `desert_night/night_game_cam.webp`.

**Como conferir:** sempre no jogo. Use as cenas `fx_acerto` (câmera perto do atingido, no ângulo da arte) e `fx_disparo` (câmera no atirador), a `vila` na câmera de jogo, sempre com `-Record`. A comparação vai lado a lado e sobreposta com as artes.

## O estudo (14/10, duas pesquisas com fontes; os números abaixo vêm delas)

**A escala:** L = o casco, 7,9 m. Na câmera padrão, 1 m dá ~17–20 px e o tanque ~135–160 px. A física dá a cor e a
direção; o tempo e o tamanho vêm dos jogos. O Zero Hour estica tudo: o clarão real dura menos de um quadro.

**O que o Zero Hour fazia** (`FXList.ini`/`ParticleSystem.ini`/`GameData.ini` publicados; 1 unidade = 0,2633 m, 30 qps):
- **Disparo:**
  - luz amarelo-pálida com raio de 3,9 m, que cai em 0,5 s;
  - clarão de 10,5 a 24 m, com pico em 0,17 s, que some em 0,83 s;
  - **onda no chão**: deitada, cor de areia, de 0,8 a 8–9,4 m em 0,33 s, com prioridade máxima;
  - fumaça com a prioridade mais baixa;
  - projétil a **105 m/s** (o nosso jogo usa o mesmo);
  - traçante de 5,3 m × 13 cm.
- **Acerto** (`FX_DamageTankStruck`):
  - luz (255, 255, 128), com raio de 1 L, que cai em 500 ms;
  - faíscas de 167 ms;
  - brilho de 667 ms;
  - detritos de 500 ms;
  - fumaça de 5 s com alfa 0,1–0,5;
  - tudo orientado pra longe do atirador (`Ricochet = Yes`);
  - no máximo 1 efeito por vítima a cada 100 ms.
- **Dano:** "danificado" a 70% e "muito danificado" a 35% de vida (o nosso tanque morre com 8 tiros: danificado no 3º, muito danificado no 6º).

**O disparo real (120 mm):**
- **Clarão primário:** 4–7,5 ms.
- **Disco do choque:** a 2,4–3 m da boca.
- **Bola secundária:** começa em ~7,5 ms, tem outro pico em ~55 ms e dura ~170 ms; tem 7–13 m no infravermelho, menor no visível, a 1200–1600 K. O sódio deixa o miolo amarelo-branco.
- **Abrams:** sem freio de boca. Com freio, a poeira sai mais larga, pros lados.
- **No deserto, o sopro levanta poeira.** Com recarga de 2 s, uma poeira de 2–3 s esconderia o tanque o tempo todo: defeito.

**Os AAA:**
- **World of Tanks:** luz de 0,09 s sem sombra, laranja → amarelo → vermelho; onda no chão por tipo de chão.
- **War Thunder:** clarão por calibre; poeira do tiro por superfície; marca no casco no ângulo do impacto; fumaça no ponto do acerto.
- **Battlefield:** partícula não pode tapar a visão.
- **Star Wars Battlefront:** malhas de faísca e um orçamento total de ~1 ms.
- **Tutorial de faíscas da Epic:** vida de 0,2–0,7 s; arrasto 1,7; quique 0,4.

## A receita (de partida; calibrar no jogo contra as artes)

**Disparo** (`UBlastFxDirector::MuzzleBlast`, `ABlastDebris::StartMuzzle`):
- miolo amarelo-branco de ~1,4 m, por 60 ms;
- línguas laranja a 1,0, 1,7 e 2,4 m à frente;
- jatos laterais;
- bola secundária laranja crescendo até ~5 m em 150 ms (em 1 de 4 tiros, fraca);
- 12 riscos rápidos à frente;
- luz de 1,5·10⁶ cd, τ 35 ms, raio 12 m;
- a areia do sopro: a poeira gravada (`jg_dust_impact_96`) a 1,5 m à frente da boca, escala 0,15, relógio 0,5.

**Projétil** (`Tracer`, `StartTracer`):
- risco laranja esticado pelo borrão de 1/30 s (≈ 3,5 m a 105 m/s), com **≥ 3 px** de espessura a qualquer distância;
- cabeça de 0,35 m;
- rastro curto (0,12 s) e fraco;
- luz de 2·10⁵ cd que anda com ele e apaga em 30 ms ao chegar;
- um rastro longo seria um "raio laser" de 30 m (reprovado).

**Acerto na blindagem** (`ArmorHit`, `StartImpact`):
- o ponto é o raio do atirador entrando na caixa justa do próprio tanque, no espaço dele (o "box" que o Felipe pediu), na altura sorteada entre o casco e a torre;
- o clarão branco no metal;
- 70 riscos brancos-amarelos (a cor do urânio) no cone refletido pra longe do atirador, 48° de abertura;
- 30 fagulhas;
- 6 lascas;
- luz;
- a areia da onda de choque: a poeira gravada no chão do tanque, escala 0,18;
- no tiro que mata, só o clarão e as faíscas (a morte traz a dela).

**Tiro no chão** (`GroundHit`): a areia, maior (escala 0,25), e um clarão curto. A cratera A segue com o jogo.

**Orçamento da areia:**
- até 6 ao mesmo tempo no Alta/Ultra, metade no Média, 1 no Baixa;
- o raymarch desenha até 16 explosões por quadro, e as mortes de tanque vêm primeiro;
- cheio, o acerto tira de cena o sopro mais velho, e o sopro não nasce.

## O que já saiu no jogo (16/10, madrugada)

- **Os ganchos** (sessão do jogo, 14/10): `MuzzleBlast` no disparo, `Tracer` no traçante, `ArmorHit` no acerto, `GroundHit` no chão e no raspão. `-ShotFx=0` volta ao provisório.
- **As cenas** `fx_acerto` (câmera a 30 m, 42°, no ângulo da arte; a sessão do jogo tirou o limite do zoom só nas cenas, `bScenarioView`) e `fx_disparo`. A ferramenta `BlastLab/Tools/fx_vs_art.py` acha os clarões no vídeo e monta jogo | arte | sobrepostos.
- **Defeitos achados no jogo e corrigidos:**
  - **O retângulo branco** (achado pela sessão do jogo): o clarão e o brilho eram faíscas paradas, e o perfil do material só arredondava a largura. Agora são redondos (`bRound`).
  - **Tudo branco no disparo:** de dia o brilho do `M_DebrisGlow` estoura a cor. Agora o brilho vai por efeito: disparo e traçante a 0,3, acerto a 0,6 (o miolo do metal é branco-quente). A bola secundária fica a ~1/8 do miolo, as línguas a 1/4.
  - **Faíscas em barras:** com 8–14 cm de largura e todas iguais, saindo de um ponto, viravam uma estrela desenhada. Agora têm 1–2,5 cm (mais o mínimo de pixels), comprimento sorteado (1/30–1/10 s de borrão), cor esfriando do branco-amarelo pro laranja, saem de uma área de 15 cm, quicam na areia (0,4) e são 110.
- **A areia do sopro e a da onda de choque** já aparecem no jogo: macias e na cor do chão (`captures/game/fx_disparo_v2`, `fx_acerto_v2`).
- **O tanque ferido** (escrito; o gancho é da sessão do jogo, no `ARTSUnitActor::SetDamage`):
  - fio claro abaixo de 70% de vida;
  - abaixo de 35%, o jato de chama pelo anel da torre e a fumaça escura;
  - para na morte.

## Segunda rodada no jogo (16/10, 03–05 h)

Tudo medido nas cenas `fx_acerto` e `fx_disparo` (e nas `_67`, a câmera de jogo aproximada), quadro a quadro, contra as artes. A regra que apareceu em quase todo defeito: **brilho aditivo somado à areia de sol estoura pro branco ou pro creme.** Um efeito só sai com cor se ficar entre ~1,5× e ~3× a areia na tela, com a cor saturada. Acima disso, a curva de tom da Unreal leva tudo pro branco.

**A causa de fundo (achada às 04:20):** a exposição. A morte de tanque foi aprovada na maquete do deserto, com EV100 13,55 travado (a receita `desert_golden_hour`, exposição 6,95·10⁻⁵). No `World_TestSalt2` do jogo o raymarch registra 2,2·10⁻⁴: a câmera está 3,2× mais aberta. O fogo do raymarch já se compensa por isso desde 11/10 (`BlastRaymarch.cpp`, a "compensação do fogo" do registro). Os destroços não se compensavam: faíscas, chamas, clarão e luzes saíam 3,2× mais fortes que na aprovada. Agora `BlastRaymarch::LastViewExposure()` passa a exposição da vista pros destroços, e o brilho dos materiais e as luzes deles × (6,95·10⁻⁵ ÷ a de agora). Os números do código valem na exposição da aprovada (`-DebrisRefExpo=` troca a referência). Os remendos à mão da noite saíram (as fagulhas a 0,35), e os números afinados no jogo sem compensar passaram pra referência: o acerto 0,25 → 0,8, a luz dele 3·10⁵ → 6·10⁵.

**O ponto do acerto** (v12): o raio do atirador contra a caixa de cada malha visível do tanque, no espaço girado dela (o jogo gira o corpo, não o ator). Agora cai na lateral virada pro atirador. Achado no registro: os 8 tiros caíam no mesmo ponto e com o mesmo leque, porque o jogo manda a mesma variante pro mesmo atirador. Um contador de tiros entra na variante, e o ponto varia ±0,7 m de lado.

**O acerto** (contra a 4.2): o leque parecia laser branco, o brilho era uma bola de névoa branca de ~5 m e as faíscas que quicavam viravam grãos de arroz brancos na areia.
- brilho do acerto 0,6 → 0,25;
- riscos amarelo-brancos (1; 0,82; 0,5) esfriando pro laranja;
- o brilho que fica com 0,9 m crescendo 1,5 m/s (era 1,3 m crescendo 3 m/s);
- a faísca que quica perde 60% do brilho.

**O disparo:**
- **A bola branca:** ela herdava o clarão da detonação da morte de tanque (o fogo esquenta e brilha ×7 no começo). A poeira e a bola do disparo agora nascem sem ele.
- **Sumia em meio segundo:** a bola é a morte de tanque minúscula tocada 5,5× mais rápido, então a fumaça ia embora junto com o fogo. Agora o relógio tem dois andamentos (`SlowAfterS`/`SlowTimeScale` no volume): o fogo nos primeiros 0,6 s da gravação (~0,11 s de verdade; o clarão secundário de um canhão grande dura dezenas de ms, Klingenberg & Heimerl, AIAA 1992) e o resto a 0,35, ~2 s de fumaça (as gravações têm ~6 s, medido no registro). A areia do sopro faz o mesmo: o golpe em 0,25 s e ~2 s pairando.
- **A cor:** A/B `fx_disparo_v6` (emissão 1) × `v6b` (0,55) × `v6c` (2, com a fumaça cinza 0,60; 0,57; 0,52). Com menos fogo ficou mais creme, então o creme é a fumaça clara espalhando sol, não o fogo. A `v6c` virou a padrão: o fogo aparece amarelo-laranja nos primeiros 0,15 s e a fumaça sai cinza-acastanhada.
- **Na câmera de jogo (67 m)** o clarão sumia: um pontinho branco e uma bolinha. O miolo foi de 0,7 m por 0,06 s pra 1 m por 2 quadros (a estrela da arte GPT tem ~a frente da torre).
- **O traçante:** duas linhas brancas de ~10 m. A cabeça (1; 0,55; 0,25) somada à areia saía um ponto branco, e o rastro aceso de 0,12 s a 105 m/s era a linha. Agora: cabeça laranja escura, risco laranja saturado (1; 0,4; 0,1) a 0,5, rastro sem fogo.
- Opções de A/B sem recompilar: `-ShotFxBallEmit=`, `-ShotFxBallSoot=r,g,b,a`, `-ShotFxBallSlow=`, `-ShotFxDustSlow=`.

**A fumaça do tanque ferido:** os pontos existiam, mas a fita não aparecia. Com o material sólido de teste (`-ShotFxTrailMat=1`) ela aparece no lugar certo, como um cone preto atrás da torre. O defeito era a cor: cinza-claro com albedo 0,45 tem a claridade da areia de sol. Agora:
- o fio do ferido é cinza-escuro (0,2);
- o muito ferido e a carcaça são fuligem (0,07 e 0,06);
- todos deitam com uma brisa fixa de ~0,8 m/s (o jogo ainda não tem vento).

**A morte de perto:** as fagulhas caindo saíam como riscos brancos (chuva); era a exposição (acima), e a compensação as devolve ao laranja da aprovada. A borda clara reta em volta da carcaça (de ~0,8 a ~2,6 s depois da morte; na câmera de jogo é o defeito mais visível) foi caçada em três testes:
- o esmaecer redondo da caixa não mudou nada;
- `-RTSNoDust` (sem a poeira das lagartas) também não;
- `-RmDebug=1` deixou a borda cinza junto com o raymarch, então ela é da explosão.

Primeira hipótese (errada): a morte acabava pelo relógio da Carga Pesada e congelava o último quadro. O registro `BlastFx vivas` (`-ShotFxLog=1`) mostrou que a gravação `eg_tq_v2` tem ~6 s (90 quadros a ~15 qps), e a morte vive ~3,5 s na partida. A moldura é dela mesma: nos quadros finais a gravação assenta a areia numa laje fina rente ao chão da simulação. No jogo, o chão de verdade em volta do casco fica abaixo dela (o avental da cratera afunda até 50–90 cm; na vila as molduras seguiam o contorno de cada casco). A/B na câmera de jogo (`fx_acerto_67_v2` × `fx67_drop70`): com o pé da explosão 70 cm abaixo do chão, a moldura some e a bola fica igual. Ficou o padrão (`-DeathFloorDrop=` troca). A saída no fim da gravação própria, com esmaecer no último 0,8 s, também ficou. No vídeo aprovado essa laje nunca aparece.

**Aviso pra comparações futuras (16/10, sessão do jogo):** o Alta passou a usar `r.Shadow.Virtual.ResolutionLodBiasDirectional=-0.5` (era −1,5). As fotos desta rodada são de antes: num A/B novo contra elas, a sombra do sol muda um pouco e o quadro fica ~0,8 ms mais barato.

## O tiro na areia (16/10, contra a arte 4.1; o Felipe: "falta o impacto na areia que faz a poeira sair pros lados, bem simulado")

**O estudo (com fontes):**
- **Régua da arte 4.1:** a picape tem ~5,2 m. Na arte, o anel de areia tem ~10 m de raio, o jato escuro sobe 12–15 m inclinado 25–30° pra frente, o clarão tem ~2,5–3 m e a cratera escura tem 4–6 m.
- **O real** (HE de 120 mm, 1–2,5 kg de TNT equivalente):
  - **Bola de fogo:** 1,8–2,7 m, em menos de 1 ms ([OSTI](https://www.osti.gov/pages/servlets/purl/1469550)).
  - **Anel de areia:** a onda de choque levanta a poeira da superfície até onde ainda passa de Mach ~1,1 ([Ugarte, Houim & Oran 2017](https://stacks.cdc.gov/view/cdc/207541)). Isso dá ~7–10 m com Kingery-Bulmash ([Swisdak](https://apps.dtic.mil/sti/pdfs/ADA526744.pdf)). O anel se forma em ~1 quadro e depois fica pairando.
  - **Jato de terra:** sai num cone a 45–60° do chão ([Housen & Holsapple 2011](https://doi.org/10.1016/j.icarus.2010.09.017); areia sob projétil, 56–63°: [PLOS ONE 2022](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0265546)), 3–5× mais pra frente num tiro inclinado ([Quillen & Doran 2024](https://arxiv.org/html/2404.16677v2)).
  - **Torrões:** caem em 0,5–1,5 s.
  - **Cor:** a terra de baixo é escura (úmida, [NASA/Whiting](https://ntrs.nasa.gov/api/citations/20050192452/downloads/20050192452.pdf)); o anel é a areia clara da superfície.
- **Os jogos:**
  - **Zero Hour:** o tiro de tanque não fazia terra nem anel. O jato e o anel eram do efeito de artilharia (`FX_ArtilleryBarrage`: terra com gravidade 3,6×, 6 discos de anel de 2 a 14 m em 0,5–0,7 s).
  - **World of Tanks:** HE na areia com luz de 0,22 s e raio de 2 → 10 m.
  - **Beyond All Reason:** a terra marrom-escura vive 0,6–0,8 s.

**A receita (`ABlastDebris::StartGroundImpact` + o anel no diretor):**
- **Clarão:** 1,25 m de meio tamanho por 2 quadros, mais um brilho laranja até ~0,12 s.
- **Faíscas:** 32 num cone de 60° em volta da vertical, puxado pra frente, por 0,15–0,35 s.
- **Luz:** laranja, 9 m, apagando em ~0,08 s.
- **Jato de terra escura:** 13 torrões com rastro de terra (albedo 0,18 × a cor (1; 0,82; 0,62)), 9–16 m/s num cone de 24° em volta de ~50° do chão, mais pra frente.
- **Torrões:** 32 de 8–30 cm, que caem e ficam.
- **O anel:** a saia de areia simétrica do EmberGen (`eg_poeira3_96`, 28,5 m, 13/10) a 0,6. O raio vai até ~8,5 m, com o relógio em dois andamentos: o golpe (3,6 s da gravação) em ~0,3 s, e o resto pairando ~1,4 s. A espessura é 0,7.
- **Cratera:** continua a da sessão do jogo.

**As provas no jogo e o passo atrás (16/10 à tarde).** A v5 e a v6 tinham os mesmos três defeitos (a v6 maiores), contra a 4.1 na câmera do jogo (`chao_v5`, `chao_v6`, o 1º tiro limpo):
- a nuvem nascia pronta aos 0,13–0,23 s. O relógio rápido do começo da gravação punha o fade-in em ~0,02 s;
- o anel virava um domo creme cheio, não um anel rasteiro de meio vazio;
- o jato de terra era uma bola marrom, não o leque alto de torrões e riscos escuros.

São defeitos do método (esticar gravações feitas pra outra coisa), não de ajuste. A v7 foi feita do zero:

- **O anel simulado no Mantaflow** (`BlastLab/Tools/blender_pyro_ground.py`, gravação `sim_chao_anel`, em metros e no tempo reais):
  - **O sopro:** 13 fontes rente ao chão sopram o gás do buraco na horizontal pra fora.
  - **A areia de perto:** 17 fontes num círculo de 1,5 m varrem a areia pra fora. Ela rola num anel com o meio limpo, até ~7 m em ~0,5 s (o estudo dá 7–8 m), baixo no começo e assentando (dissolve).
  - **A areia de longe:** três anéis ralos de 3 a 7,5 m, com a densidade caindo pra fora, levantados no lugar pelo choque.
  - **A quebra da simetria:** números primos e ângulo, raio e força sorteados; com 12 e 16 fontes iguais, a grade juntava tudo em 4 gomos.
  - **Calibração:** num domínio de 20–24 m o Mantaflow anda a ~1/3; a escala de tempo 3–3,6 é o tempo real (medido com um bloco a 10 m/s).
  - **Duas lições:** o peso da densidade (alpha) quase não muda nada; e a advecção dele ganha massa onde a velocidade varia muito (de 645 a 1520 em 1,5 s sem fonte nenhuma). O dissolve logarítmico segura as duas coisas.
- **A terra escura é balística, nos destroços.** Um gás não carrega os torrões: no Mantaflow o jato perdia força em 3–5 m. Por isso ela sai pelas partículas:
  - 30 riscos escuros, com rastro só enquanto sobem rápido (0,35 s), por isso retos e curtos, sem virar o "cabelo" arqueado;
  - 45 torrões escuros de 12–45 cm;
  - tudo num leque de ±20°, inclinado 15° pra longe do atirador, a 13–22 m/s.
- **A/B:** `-GroundHitV=6` volta o anel e o jato da v6.

**A cortina de ejeção, v8 (17/10, madrugada; por enquanto só com `-GroundHitV=8`).** O leque escuro da 4.1 é a terra fina que a cratera joga pra fora. Ela é simulada grão a grão (`BlastLab/Tools/ejecta_curtain.py`, gravação `sim_chao_cortina`):
- **A saída:** a velocidade cai com a distância do centro, v = 0,55·√(gR)·(x/R)^(−1/0,41) ([Housen & Holsapple 2011](https://doi.org/10.1016/j.icarus.2010.09.017)), a 45–65° do chão.
- **Os raios:** 65 % das parcelas se juntam em 9–13 rumos sorteados. São os "dedos" das cargas em areia.
- **O arrasto do ar:** pela esfera de Schiller e Naumann, com grãos de 40 µm a 5 mm.
- **O gás empoeirado:** onde a carga de grãos passa da massa do ar, o ar anda junto (a velocidade de equilíbrio). É o que leva os finos no jato denso; quando o jato abre, eles se soltam e descem pairando.
- **O resultado:** sobe a ~8–10 m, abre a ~8 m de raio e cai a ~30 % em 2,5 s.
- **Só a fração fina vira volume:** ela é a que escurece o ar. Os torrões seguem nos destroços.
- **A 1ª prova no jogo (`chao_v8`):** a base do leque ficou escura e densa como na 4.1, mas borrada, uma mancha marrom. A 2ª gravação tem menos mistura e os raios aparecem; falta a prova.

## O custo de processador dos destroços (16/10, item 3)

**Medido na vila** (12 tanques, câmera de jogo, Alta, sem gravar vídeo, `vila_cpu1`): os destroços custam 1,21 ms em média e 2,87 ms nos piores 5% dos quadros, com até 35 atores vivos. Por parte:

| Parte | Média | Piores 5% |
|---|---|---|
| Física | 0,28 ms | 1,11 ms |
| Rastros | 0,32 ms | 0,82 ms |
| Brilhos | 0,26 ms | 0,62 ms |
| Chapas | 0,21 ms | 0,50 ms |
| Luz | 0,02 ms | — |

Criar os atores de um disparo chega a 2,7 ms no pior caso. Os marcadores `BlastFxDebris/Integrate|Light|Plates|Glows|Trails` ficam.

**O que mudou:**
- **O chão consultado uma vez por efeito.** Cada faísca, torrão e chapa consultava 4 nós do mapa de alturas a cada quadro. Agora é uma consulta por efeito, usada num raio de 6 m do centro; fora disso, segue a consulta por peça.
- **A conversão de cor (causa raiz de "faíscas creme").** Na Unreal 5.8, `CreateMeshSection_LinearColor` não converte as cores, mas `UpdateMeshSection_LinearColor` converte pra sRGB por padrão (`ProceduralMeshComponent.h`). Os brilhos dos destroços são criados no 1º quadro e atualizados nos seguintes, então do 2º quadro em diante a cor saía clareada: o laranja 0,5 virava 0,73, faíscas creme. É também a causa do A/B de 07/10, quando o rastro escuro da torre virou um "tubo creme aceso" ao só atualizar. Agora as duas vão sem converter.
  - Os rastros passam a só atualizar (a seção não é mais recriada a cada quadro).
  - A morte de tanque aprovada tinha o mesmo defeito: as faíscas dela ficam mais quentes. Precisa de A/B com a aprovada.

**A medição depois das mudanças** (`vila_cpu3`, a mesma cena, 780 quadros):

| | Média | Piores 5% |
|---|---|---|
| Antes (`vila_cpu1`) | 1,21 ms | 2,87 ms |
| Depois (`vila_cpu3`) | 1,16 ms | 2,67 ms |

- **Por parte:** os rastros caíram 30% (0,32 → 0,22 ms) e a física 15%; os brilhos subiram um pouco (0,26 → 0,31 ms, mais faíscas visíveis com a cor certa).
- **O que falta:** o grosso continua sendo criar e atualizar um ator por efeito. O passo seguinte é juntar todos os efeitos num ator só (uma malha pros brilhos, uma pros rastros) e reaproveitar atores em vez de criar. É uma reforma maior, que fica pra quando a medição com 50 tanques pedir.

## O carro esmagado e o choque do poste (16/10 à tarde, itens 5 e 6)

O estudo com fontes está em `docs/ESTUDO_ESMAGAR_E_POSTE.md`. As duas chamadas ficam no `UBlastFxDirector`, e a sessão do jogo liga as duas.

**O carro: `CarCrushStart(Car, TankDirCm, PassSeconds, Variant, bAlarm)` e `CarCrushEnd(Car)`**
- **A chamada:** o jogo chama no 1º `PropCrushed`, passando o Proxy do carro. A passagem é (comprimento do carro + 4,5 m) ÷ a velocidade do tanque.
- **A linha do tempo, pela fração da passagem:**
  - 0: a poeira do contato e as lanternas (lascas vermelhas e âmbar);
  - 0,2T: os vidros das laterais estouram;
  - 0,35T e 0,8T: os pneus estouram (sopro de poeira e pedaços de borracha);
  - 0,5T: o teto cede, o vidro de trás estoura e a poeira sai pelas janelas;
  - 0,6T: as lanternas da outra ponta;
  - 0,65T: o casco raspa o teto (poucas faíscas, fracas de dia).
- **O vidro que pisca:** 120 cacos por janela, cada um com uma normal que gira. O caco só acende quando o reflexo do sol bate na câmera (lóbulo de ~7°). Os que caem no chão ficam ~2,5 s e piscam quando a câmera passa. Mais 24 blocos de cubos ainda grudados.
- **O carro queimado** (`SM_civ_car_burnt`, reconhecido pelo nome da malha): não tem vidro, lanterna, pneu nem alarme. No lugar sai a cinza escura pelas janelas e lascas de ferrugem e de tinta queimada.
- **O pisca-alerta** (o alarme do ZH em metade dos carros): luz âmbar a 1,5 Hz por 10–30 s, só na picape.
- **A frente da malha é o +Y:** o comprimento sai pelo maior lado da caixa.
- **Teste sem o jogo:** `-CrushFxTest` acha o carro que o jogo trocou pelo ator "Prop" ao esmagar. As cenas são `carro_picape_67` e `carro_queimado_67`.

**O poste: `WireArc(PointCm, Kind, Variant)`**, no lugar do `URTSMatchSubsystem::WireArc` provisório.
- **Os tipos:**
  - Kind 0, o fio arrebenta no ar: clarão de 3–4 quadros, arco de 0,1–0,6 m, 40–80 gotas de metal e as lascas brancas do isolador;
  - Kind 1, a ponta viva encosta na areia: o clarão rápido rente ao chão;
  - Kind 2, a religada: o arco de 0,6–1,0 s, 0,3–1,5 m, pulando pela areia a cada 0,1–0,2 s, com gotas o tempo todo e uma fumacinha branca-acinzentada.
- **O arco:** a fita em zigue-zague é refeita todo quadro (ponto médio deslocado, 4 níveis). Tem núcleo branco-azulado e halo azul-violeta, e às vezes um ramo. O brilho é sorteado por quadro, porque o piscar de 120 Hz da rede, a 30–60 quadros por segundo, vira isso; 12% dos quadros ficam apagados e 1 em 8 sai com o verde do cobre.
- **A luz:** azul-branca. No clarão, 0,5–1× o sol a 3 m; no arco temporizado, piscando.
- **Teste sem o jogo:** `-ArcFxTestX=190 -ArcFxTestY=-236` roda a sequência num ponto (o fio arrebenta aos 2,0 s, encosta aos 2,5 s, religa aos 4,5 e 7,0 s). A cena é `arco_67`.
- **As 3 decisões do Felipe**, com o que pus até ele decidir:
  - o religador no tempo de jogo (0,5 / 2 / 2 s, o padrão do OpenDSS), não o real de 10–30 s;
  - o fio na areia com arco visível. É defensável: os fios caem juntos com o neutro. O real na areia seca seria nada;
  - o transformador sem fogo (o real: a chave fusível abre).

**As primeiras provas no jogo (16/10, 15:50–17:00):**
- **A picape** (`carro_picape_v2`): a poeira das lagartas do próprio tanque (o RTSTreadDust, da sessão de arte) é uma nuvem grande que cobre o carro durante todo o esmagamento. O meu efeito não dá pra julgar assim: repetido com `-RTSNoDust` (`carro_picape_v2b`).
- **O carro queimado** (`carro_queimado_v2`): o tanque parou antes do carro. O destino da cena encostava numa casa e o caminho desistia (a sessão do jogo achou a causa); a cena foi corrigida.
- **O arco** (`arco_v1`): a religada lê como eletricidade quando ampliada (o zigue-zague com o halo violeta, o brilho azul-branco piscando no chão, as gotas). Na câmera normal fica um ponto azul-branco pequeno e piscante de ~20–30 px: discreto, mas certo pra escala. O vídeo começa ~3 s depois da partida e cortava o fio arrebentando e a última religada; os tempos do teste foram empurrados 2 s.
- **A Patriot** (`patriot_v1`, `patriot_v2`), dois defeitos do lado do jogo, avisados:
  - o míssil sai pela traseira da caixa (as bocas ficam do outro lado);
  - a fumaça simulada do rastro vira um banco de nuvem que cobre os tanques.
  - Do meu lado, a poeira de saída saía uma bola branca: mais rala (0,8 → 0,45) e menor.

## A fumaça na luz do mundo (16/10 à noite; a regra nova do CLAUDE.md)

O Felipe, à noite: "o mundo todo tem que ser assim, sem gambiarra". Toda fumaça, fogo e poeira tem volume, recebe a sombra do mapa, projeta sombra e é acesa pelas explosões. O estudo e a segunda opinião estão em `docs/ESTUDO_LUZ_NA_FUMACA.md`. É um sistema só, dividido entre três sessões.
- **A sombra do mapa: o mapa do sol da sessão de arte** (plugin `RTSSunShadow`, no jogo desde as 20:32). Grade de 0,5 m girada pro sol; o mapa do World_TestSalt2 tem 2268² (1134 m), com o sol a 8°.
  - No raymarch: `RTSSunVisibilityM` em cada amostra, multiplicando o sol em todas as oitavas (fora do lerp do espalhamento, pra fumaça na sombra de um prédio não pegar sol "por tabela") e a luz que volta do chão.
  - Entrou pela troca segura às 20:42. `-RmSunShadow=0` desliga, pro A/B; `-RmDebug=4` pinta a fumaça com a visibilidade do sol.
- **A luz das explosões e do fogo: `RTSSceneLights`** (sessão Fluxo, no `RTSShaders`).
  - O diretor junta a cada quadro as 32 luzes pontuais mais fortes (candela × cor).
  - O raymarch soma as 4 que mais iluminam cada explosão, fora a dela mesma.
  - `IlluminanceAt(posM)` dá lux pra quem soma por puff (o rastro do míssil, o diesel); a textura 32×2 serve aos materiais (a poeira).
  - Entrou às 19:38. `-RmLocalLights=0` desliga.
- **A fumaça dos destroços em fitas** (`M_TrailSmoke`): já é Lit translúcido por pixel ("Surface ForwardShading"), e recebe a sombra e as luzes da própria Unreal.
- **A poeira das lagartas:** vira volume dentro do raymarch, com a mesma luz. A Arte faz o campo (`RTSDustField`) e o Fluxo faz a integração na marcha conjunta.
- **A troca segura do shader** (`luz_troca.ps1` no scratchpad do Fluxo): segura a fila com o mesmo dono do teste, compila, troca o `.usf`, roda o teste e volta o `.usf` se o log acusar erro. Um `.usf` quebrado derruba o jogo de todas as sessões.

## O que falta (a fila combinada com o Felipe, 16/10 de manhã)

A ordem é a dele ("segue a tua ordem e deixa as novas no fim; cada uma bem feita"). Cada item: estudo com fontes, 2ª opinião, prova no jogo em vídeo, A/B; nada de commit sem o "aprovado".

1. **O traçante e o acerto no mesmo ponto e no mesmo instante** (o Felipe: "ele chega, some, depois aparece um pouco pra outros lados"). O ponto na blindagem sai no disparo, o traçante voa até ele e as faíscas nascem quando ele chega. E o risco do traçante todo atrás da cabeça ("a cabeça está no meio do trajeto").
2. **O tiro na areia contra a 4.1:** o jato de terra escura e o anel de areia correndo pelo chão.
3. **O custo de processador dos destroços** (1,0 ms em média e 2,5 ms nos piores quadros da vila): um ator só pra todos os tiros.
4. **As torres de defesa atacando:** o disparo, o míssil e o acerto (v3, pela sessão do Look; docs/BASE_JOGAVEL.md).
5. **O tanque esmagando os carros civis:** o vidro estourando, a poeira e a faísca do metal. A regra é do jogo e o carro amassado é do Look.
6. **O choque elétrico dos postes derrubados:** o arco, as faíscas e o clarão, com a Arte.
   - *(17/10, madrugada)* No jogo de verdade (a cena `derrubar_postes`, `captures/game/arco_postes2`). O `WireArc` do jogo chama o do BlastFx desde a compilação 40. Na 41, o Jogo pôs a falta à terra: quando o poste deita, cada fio com a cruzeta a menos de 1,5 m do chão faz o arco ali, e depois vêm as religadas. No vídeo se vê o clarão azul na cruzeta deitada e a 2ª tentativa ~1 s depois. **Ficou de fora** o meio do vão encostando no chão com as duas pontas no alto: depende do sistema de fios do WorldForge.
7. **A marca quente no casco** onde o tiro bate (com a sessão do jogo).
8. *(novo, 16/10)* **O disparo refeito pelo fenômeno real** (a referência que ele elogiou à noite: `Downloads/gemini_generated_video_a0c04d8e.mp4`, ~6,0 s, a bola branco-amarela à frente da boca por ~0,17 s, virando laranja e acabando em fumaça cinza aos ~0,4 s; "tem volume, tem qualidade, tem originalidade"): o clarão colado na boca, o jato cinza saindo de dentro do cano na direção dele e a areia baixa e sutil onde a onda bate no chão. A versão da noite (a morte de tanque em miniatura solta à frente, cor de areia) foi reprovada.
   - *(17/10, madrugada)* **Simulado e no jogo (v7, o padrão):**
     - **A simulação:** Mantaflow (`BlastLab/Tools/blender_pyro_muzzle.py`, gravação `sim_boca`). O jato sai pelo eixo, com sub-jatos abertos até 7° (com um só, o anel de vórtice saía limpo demais). O tempo vai em dois andamentos, câmera lenta no fogo e tempo real na fumaça.
     - **No jogo:** a boca fica no meio da gravação e ela gira pro rumo do cano (o giro das instâncias no raymarch, novo, 17/10).
     - **O que se vê** (`captures/fluxo_noite_17/disparo_simulado.jpg`, ao lado da referência `disparo_referencia.jpg`): o clarão na boca; a bola branco-quente de ~3–4 m em ~0,07 s; o miolo laranja dentro da fumaça; a couve-flor cinza de ~5 m que se desfaz em ~1,2 s.
     - **O ajuste, medido no jogo:**
       - a densidade do Mantaflow é ~0,3 no máximo, e o conversor multiplicava por 0,35: a bola saía rala;
       - ×8 saiu preta e opaca, então ficou ×3 com a curva de densidade 0,6 (nova no `build_raymarch_vdb.py`, argumento 19);
       - a fumaça cinza-clara (albedo 0,75, a da pólvora), o fogo a 4 e a fumaça segurada a 1,6.
     - **A/B:** `-MuzzleV=6` volta a bola de antes.
   - *(17/10, tarde)* **Menor, mais rápido e sem inflar** (o Felipe: "grande, lento e formato errado"; depois, no vídeo de perto: "na hora que ele vai desfazer ele infla do nada").
     - **O tamanho e o tempo:** a gravação a 0,4: a bola de ~1,5 m dura ~0,2 s.
     - **O inflar:** o 2º andamento a 0,15 atravessava o resto da simulação a 6,7×. A fumaça se espalhando de 2 s de gravação passava em 0,25 s, dobrava em 2 quadros e sumia no seguinte. Agora a gravação toca num andamento só, e a fumaça some pela opacidade entre 0,3 e 0,7 s, ficando rala aos poucos. `-ShotFxSimLife=0` volta o de antes.
     - **A prova** no Alta, gameplay: `captures/game/alta_perto` e `alta_varios`.
   - *(17/10, tarde)* **A falha crítica dos disparos sumindo** (o Felipe: "some os tiros quando mais de 3 atira").
     - **A causa:** cada tiro gasta 3–4 volumes no raymarch: o disparo, a areia do sopro, o anel na areia e, no mesmo buraco, a cortina. O diretor tinha 12 vagas e o raymarch desenhava 16 por quadro, então o 4º tanque já estourava.
     - **O conserto:** o orçamento por importância, como o Effect Type do Niagara.
       - 24 vagas no Alta e no Ultra (12 na Média).
       - O raymarch desenha 32 por quadro e tem 64 vagas de quadro, com o atlas no tamanho de antes.
       - Cheio, sai primeiro o volume fora da tela, depois o mais perto do fim. Um que nasceu há menos de 0,25 s só sai se não houver outro.
       - No raymarch, os da tela vão na frente da fila.
     - **A prova:** `tiro_chao` no Alta com `-ShotFxLog=1`. No pico ficam 24 vivos, todos desenhados, e nenhum dos 46 disparos sai antes da hora (`scratchpad/vivas_boca.py`).
     - **O custo:** com os 6 tanques, o raymarch vai de ~6,5 a ~7,7 ms na média, porque desenha o que antes sumia. O p95 fica igual (~11 ms).
   - *(17/10, tarde, APROVADO; melhoria futura)* **A areia que o sopro levanta** (o Felipe, no quadro a quadro de perto: "ela não tem muita vida, ela só sai pra cima"; depois, "pode deixar assim, pode ser melhoria futura").
     - **Hoje:** a `jg_dust_impact_96` a 0,15, 1,5 m à frente da boca. É uma poeira de impacto de cima, encolhida: um bolo que só sobe e, embaixo do cano, cai na sombra do tanque e sai escuro.
     - **O fenômeno:** o gás do tiro bate no chão na frente da boca, e a ventania logo atrás do choque arranca a areia (o cisalhamento atrás do choque; APS DFD 2016, "The role of granular shocks in dust-layer dispersal"). A areia sai rente ao chão, pra frente e pros lados, mais forte na frente: o sopro na frente da boca é bem maior que atrás (ARBRL). Depois ela enrola e fica pairando como um véu claro de 1–3 s, com grãos riscando pra fora.
     - **Fontes:** a patente US 6308608 (os dois choques da boca levantam a poeira, mais com o cano baixo); a carta de 1942 do arquivo da UEA (os gases batendo no chão na frente da boca); o War Thunder (nuvem de gás, fumaça e poeira por tipo de chão); o mod Blastcore do Arma 3 (onda de choque e poeira pairando).
     - **O plano:** a "saia" simulada como a cortina do tiro na areia (`ejecta_curtain.py`): parcelas arrancadas onde o vento do choque passa do limite, a velocidade radial que cai com a distância e com o rumo do cano, o enrolar e a mistura que cresce com a idade. Gravação nova, girada pro cano.
9. *(novo, 16/10)* **O fogo e a fumaça da carcaça contra os game shots** ("mixuruca").
   - *(17/10, madrugada)* Estudo em `docs/ESTUDO_CARCACA.md`. A carcaça agora é uma coluna simulada (`BlastLab/Tools/blender_pyro_wreck.py`, gravação `sim_carcaca`, loop de 10 s, inclinada 15°, girada pro vento do mapa; o Jogo passa o vento por `SetWind`). Ela troca as chamas de sprite e a fumaça em fita.
   - **REPROVADA pelo Felipe em 17/10, vai pra v4** ("é só um borrão, não tem fluido, não tem física, não tem volume"). Fica desligada (`-WreckV=2` liga). A 1ª simulação saiu um "bastão" escuro e fino de 24 m: o jato subia rápido e estreito, e o miolo opaco ficava sem a fumaça rala. A 2ª (menos empuxo, mais turbulência, se desfazendo até ~12 m) está na bancada (`carcaca_bancada_67`, `-ShadowFxTest=4`).
   - **Corrigido também:** a carcaça só acendia quando o tiro que mata passava pelo `ArmorHit`. Agora a própria morte acende (`TankDeath(..., WreckOf)`; o Jogo passa a unidade, compilação 43).
10. *(novo, 16/10)* **A onda de choque da explosão do tanque levantando a areia em volta**, "bem estruturada, bem calculada". O anel de distorção no chão entra aqui.

## Fontes (das duas pesquisas)

- ZH: [FXList.ini e ParticleSystem.ini](https://github.com/TheSuperHackers/GeneralsGamePatch/tree/main/Patch104pZH/GameFilesOriginalZH/Data/INI), [GeneralsGameCode](https://github.com/TheSuperHackers/GeneralsGameCode)
- World of Tanks: [gun_effects.xml](https://raw.githubusercontent.com/izeberg/wot-src/EU/sources/res/scripts/item_defs/vehicles/common/gun_effects.xml), [2.4 VFX](https://worldoftanks.eu/en/news/general-news/2-4-upgraded-vfx-sfx/)
- War Thunder: [datamine](https://github.com/gszabi99/War-Thunder-Datamine), [Drone Age](https://warthunder.com/en/news/7843-development-new-visual-effects-in-the-drone-age-update-en), [Dagor 6.5](https://warthunder.com/en/news/7612-development-dagor-engine-6-5-new-graphic-features-en)
- O clarão real: [NSWC Crane, Dye 2016](https://ndia.dtic.mil/wp-content/uploads/2016/armament/18357_Dye.pdf), [ARL-TR-909](https://apps.dtic.mil/sti/pdfs/ADA396882.pdf), [Telops](http://info.telops.com/rs/telops/images/FAST%20Thermal%20Imaging%20-%20Tank%20Muzzle%20Flash%20Analysis.pdf), [navweaps](https://navweaps.com/index_tech/tech-090.php)
- Faíscas: [tutorial da Epic](https://dev.epicgames.com/documentation/en-us/unreal-engine/how-to-create-a-sparks-effect-in-niagara-for-unreal-engine), [IspatGuru](https://ispatguru.com/?p=10374)
- Estilhaços atrás da blindagem: [Tank Archives](https://www.tankarchives.com/2015/03/beyond-armour-effects.html)
- Leitura: [Riot, Clarity](https://www.leagueoflegends.com/en-gb/news/dev/clarity-in-league/)
