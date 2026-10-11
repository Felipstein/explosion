# Estado atual — 08/10/2026: o renderizador do WebGL dentro da Unreal

**O raymarch do WebGL portado (08/10, madrugada; o Felipe: "volta a ser o meu
velho gerador").** O Heterogeneous Volume da Unreal saía borrado (buffer de
536×304 no alta), apagava o fogo cedo, deixava a fumaça cinza-azulada e custava
20–30 ms. Agora a Carga Pesada e todas as armas feitas dela (granada a nuclear)
são desenhadas pelo MESMO renderizador do WebGL (`js/instances.js`), portado
como passo próprio depois do TSR e antes do bloom:
- `Shaders/Private/BlastRaymarch.usf` + `Source/BlastLabShaders/.../BlastRaymarch.cpp`:
  cache de luz 48³ por explosão (sol e fogo), marcha em meia resolução com salto
  pelo macro, quadros misturados com vetores de movimento, ruído que anda com a
  fumaça, oitavas de espalhamento múltiplo, corpo negro (Planck × CIE), upsample
  pela profundidade e acúmulo temporal (Schneider 2015 / Hillaire 2015). Mesmas
  constantes do WebGL.
- `Tools/build_raymarch_bake.py` lê o arquivo ORIGINAL do WebGL (com a
  transmitância do céu) → `Intermediate/Raymarch/r96_v<n>.bin` (213 MB cada, na
  RAM; os quadros em uso sobem pra um pool de 20 na placa, ~80 MB).
- `UBlastRaymarchSubsystem` junta as explosões vivas; `AExplosionVolume` esconde o
  volume dela e continua com luzes, clarão, sombra no chão e a **deformação pelo
  que passa** (o túnel do B-2 e do avião), que o raymarch lê igual ao material.
- Ligado por padrão; `-Raymarch=0` volta pro volume antigo. Sem o arquivo da
  gravação, a explosão fica no volume antigo (não some). Os corpos da EmberGen
  (morte de tanque) seguem no volume da Unreal.
- **O "erro" aprovado, reproduzido:** a cor da luz-chave do WebGL é a irradiância
  num chão horizontal (sol × sen da altura, `atmosphere.js` texel 0) usada como luz
  de frente. Às 15h a fumaça recebe 0.44 do sol: é a fuligem preta aprovada. Com a
  luz inteira ela saía bege (`captures/cmp_250.jpg` → `cmp_250b.jpg`).
- **Medido** (alta, 1600×900, câmera de perto da Carga Pesada): raymarch 1.1–1.7 ms
  de GPU (marcha cheia: 3.2–4.8 ms, sem ganho visível: a maciez de perto é o voxel
  de 40 cm da gravação). Quadro por arma, antes → agora: granada 17.2 → 11.6 ms,
  morteiro 22.8 → 11.9, Carga Pesada 25.1 → 12.1, paiol 27.2 → 12.2, ataque aéreo
  23.7 → 12.3. Seis Cargas de cima (RTS): raymarch 2–5 ms, 66 qps.
- **Nos mapas** (08/10, manhã; câmera de RTS de cima, alta, placa sozinha), quadro
  mediano sem explosão / volume antigo / raymarch: deserto 14.7 / 26.6 / **16.4 ms**
  (61 qps), neve 15 / 27.9 / **16.3 ms** (61 qps), floresta 17.8 / 29.1 / **19.1 ms**
  (52 qps); o raymarch 0.6–1.2 ms de GPU. As árvores na frente tapam a bola certo
  (`captures/rm_mapas_antes_agora.jpg`). Os engasgos de 0.5–6 s da 1ª rodada eram
  outra Unreal (o RTSGame) aberta junto, sem a trava: não valem.
- **A nuclear** muda de cor como no WebGL (`instances.js _nukeLook`): marrom-
  avermelhada logo depois da bola, branca quando esfria (albedo 0.48 → 0.70, a
  emissão compensada); `AExplosionVolume::bNuclear`, `captures/rm_nuke.jpg`.
- Até **16 explosões** ao mesmo tempo (32 quadros na placa, 113 MB); acima disso
  as mais longe não aparecem. **Barragem de 12** (de cima, alta): deserto 20.8 ms
  (48 qps, p95 33), campo de teste 19.0 ms (53 qps); o volume antigo com as mesmas
  12 caía pra ~4 qps com travadas de 1–10 s. Abaixo do ultra só as 3 maiores na tela
  fazem sombra com a luz do fogo (as 12 luzes com sombra custavam ~8 + 8 ms). A
  travada de 2.5 s na detonação dos testes com `-ProfileGpu` é o próprio profiler
  começando a gravar (log do CsvProfiler), não a explosão. Aberto: um tranco de
  ~250 ms aos 0.5 s no deserto com 12 (não aparece no campo de teste).
  Vídeo com a câmera girando: `captures/raymarch_barragem12_deserto.mp4` (o
  acúmulo temporal não deixa rastro).
- Folhas e vídeos: `captures/rm_arsenal.jpg` (o arsenal antes/agora),
  `captures/rm_live2_sheet.jpg`/`cmp_250b.jpg` (contra o WebGL),
  `captures/antes_agora_b2.mp4`, `raymarch_b2.mp4`, `raymarch_rts_barragem.mp4`.
- **Aberto:** a "saia de poeira" branca do volume antigo não existe no WebGL e
  não aparece no novo; o "pente" na borda do corte do B-2 já existia no volume
  antigo (é o desenho da deformação de 06/10); `-RmDetail2=` (2ª oitava do
  detalhe, a do ultra do WebGL) não mudou nada visível e fica desligada.
- **A fumaça macia de perto (08–09/10).** Ficou: as **bolhas de perto** (Worley
  que anda com a fumaça, só na fase quente e só com o voxel ≥ ~3 px na tela;
  Nubis Cubed, Schneider 2023), sutis, +0.1 ms; `-RmDetail=0` desliga. Ficou
  também a **comparação lado a lado no app** (`-RmCompare=1`: cada detonação
  ganha uma gêmea 40 m à direita tocando a gravação 128³, `r128_v0.bin`) e as
  explosões com gravações de tamanhos diferentes no mesmo quadro. Testado e
  **sem ganho visível**: a gravação 128³ (o Felipe: "não senti diferença"),
  afiar a borda (`-RmSharp`, tirava o contorno escuro da fuligem), e a sombra
  fina de 0.4 m entre os rolos (um 2º cache de luz, Nubis 2015; mudava a sombra
  em ~5% dos pixels, a fuligem escura escondia) com as oitavas de sombra de
  verdade (só escureciam): retirados. Conclusão medida: a maciez vem da FORMA
  gravada (a simulação do WebGL a 0.4 m espalha os redemoinhos pequenos), não da
  luz nem da tela. Próximo (o Felipe, 09/10): **mudar quem grava**.
- **"Mudar quem grava" (09–10/10, pedido do Felipe):** a mesma explosão gravada em
  5 fontes (WebGL, Unreal, Blender, EmberGen, Houdini), todas tocadas no raymarch
  dentro do jogo, lado a lado.
  - `Tools/build_raymarch_vdb.py` (no Python do Blender): uma sequência OpenVDB
    (EmberGen, Mantaflow) → o formato do raymarch, com a luz do céu como o WebGL
    (reto pra cima), a velocidade quando existe e um fator de calor por fonte
    (cada programa tem a sua unidade; iguala o calor mediano da fumaça aos 0,6 s).
  - No app, `-RmCompare=a,b`: cada clique detona a de sempre e uma gêmea por
    gravação (40 m à direita, 40 m à esquerda...); `-NoPanel` esconde o painel nas fotos.
    Cada gravação tem o próprio domínio e resolução; o ruído de detalhe encolhe
    com o voxel (`NoiseK`), pra não borrar o detalhe de uma gravação fina.
  - 1º resultado (`captures/fonte3_perto.jpg`, `fonte_eg_perto.jpg`, `fonte3_*.mp4`;
    detalhe < 1 m medido na silhueta): WebGL 96 0,22–0,34, WebGL 128 0,25–0,43,
    Blender 19 cm 0,38–0,48, EmberGen da JangaFX 16 cm 0,44–0,60. A gravação fina
    traz a borda nítida pro jogo. O Blender (Mantaflow, `Tools/blender_pyro_charge.py`,
    13 ajustes) não faz a coluna nem o cogumelo e fica felpudo: descartado. A
    EmberGen do teste é outra explosão (gasolina); a nossa Carga Pesada está sendo
    simulada no EmberGen pela sessão Projeto complexo (que opera a janela).
  - O porte do simulador pra Unreal é de 3–4 semanas (segunda opinião de 09/10):
    a grade colocada com diferença central corta a resolução efetiva pela metade
    (por isso a 128 quase não ganhou da 96); pressão convergida, grade MAC,
    "disturbance" e guia pela 128. Decisão depois de ver a do EmberGen.
- **O clarão refeito (10/10, item 2 da lista do Felipe: "gambiarra").** Saíram o disco
  M_FireGlare de 45 m (no teste ele aparecia como um quadrado bege de borda reta e
  clareava a tela inteira ~⅓ s) e o núcleo de 14 m da morte de tanque. O clarão sai
  do próprio fogo no raymarch (Jimenez, SIGGRAPH 2014: o halo é o bloom do HDR):
  o miolo quente esquenta +500 K e a emissão sobe, caindo com τ 100 ms no relógio da
  gravação, força 2× (o Felipe testou ao vivo e escolheu; o brilho curto separado
  saiu porque cortava no chão nas armas pequenas). O jogo manda a média do clarão no
  intervalo do quadro (não some a 30 qps) e o acúmulo temporal pesa menos durante ele.
  A luz curta do chão segue em 35 ms sem sombra (a sombra dela é o próximo item).
  Depuração no app: G liga/desliga, J a duração, K a força (`blast.Flash*`). Folha e
  vídeo: `captures/clarao_cima_ab.jpg`, `clarao_ab_cima.mp4`.
- **Atlas sem quadros vazios:** com uma gravação grande (a do EmberGen, até 7 592 blocos
  por quadro) o atlas enchia de quadros velhos e a explosão sumia por instantes; o jogo
  agora tira das vagas os quadros menos usados quando os blocos não cabem (teto do
  atlas 500 MB). A Carga Pesada simulada no EmberGen pela sessão Projeto complexo
  (192³, 19,8 cm) entrou na comparação (`eg_charge`, `captures/fonte_egc_*`): couve-flor
  nítida, mas mais larga e com o fogo apagando antes de 1 s. O Blender foi descartado;
  o Houdini foi pro backlog; o porte do simulador pra Unreal não por enquanto.
- **Gravações em blocos esparsos (BRM2, 10/10).** Uma gravação fina densa não cabe
  no jogo (192³: 2,7 GB). `Tools/brm2.py`: blocos de 8³ só onde há fumaça (+1 voxel
  de borda, como o Sparse Volume Texture da Unreal e o NanoVDB), uma tabela por
  quadro; os conversores já gravam assim. 96³ 213 → 95 MB, 192³ 2,7 GB → 586 MB.
  Na placa, um atlas só pra todas as explosões (o pior caso: o quadro mais cheio × 32
  vagas; ~107 MB só com as de 96) e a tabela por vaga; os blocos sobem num passe de
  compute (`MainScatterA/F`). O renderizador continua lendo uma textura só.
- Depuração: `-RmTerms=sol,céu,fogo,emissão`, `-RmDebug=1|2|3` (transmitância
  até o sol / fogo / céu), `-RmRes=` (resolução da marcha), `-RmTemporal=` (peso
  do quadro novo; 1 = sem acúmulo), `-RmLightCaches=`, `-RmSteps=`. E no teste
  automático `-AutoVideo=de,até` com `-UseFixedTimeStep -FPS=30`: uma foto por
  quadro no relógio fixo (vídeo sem os pulos das fotos).

# 06/10/2026: a explosão reage ao que passa, sem trocar de fumaça

**Uma fumaça só na morte de tanque (07/10, noite; P0 do Felipe).** Eram três fumaças com três caras no mesmo quadro: a cortina escura do fogo da carcaça (Mantaflow), a coluna da JangaFX (aprovada: "tá boa") e a bola cinza-clara do campo em cima dela, que cresce com o tempo. Agora:
- o fogo da carcaça mostra só a chama: a fuligem abaixo de um calor some (`SootTempCut` no material, `build_explosion.py`; zerar a fuligem não servia, a emissão é proporcional a ela). A fumaça dele fica ≤ 0.3 de calor, a chama ~1.3: corte em 0.3;
- com a coluna, o campo só recebe o calor da carcaça, sem fuligem: a bola some (conferido aos 15 e 30 s, `captures/ue/_p0_longo.jpg`);
- sem a bola, o topo da coluna virava uma tampa reta (o teto do domínio do EmberGen): ela se dissolve de 7 a 11 m locais (`TopCutM`/`TopCutFadeM`; A/B 14.5/13/12/11 na câmera baixa). A coluna ficou ~3 m mais baixa e mais estreita (a cortina do fogo a alargava).
- Folha e vídeo de 6 s antes/depois, mesma história, câmera de RTS e baixa: `captures/ue/_p0_uma_fumaca.jpg` e `.mp4`.
- Medido no miolo opaco da fumaça: o R/B varia 0.02–0.25 dentro da própria coluna (o topo ao sol ~0.94, o lado na sombra do céu ~0.73) — é a luz, não outra fumaça; o critério Δ ≤ 0.1 não fecha assim. E a fuligem sai neutra-fria (R/B 0.7–0.95) contra o marrom-preto quente do alvo (1.3–2.9): decisão de luz/ar ou da cor da fuligem, fora do P0.

**Desempenho (07/10, tarde; o Felipe: "tá travando pra krl").** Medido ao vivo, sem `-benchmark` (o passo fixo das gravações A/B escondia o tempo real: os "33.3 ms" do log nunca mediram nada). Preset alta, Range_Desert_Units, câmera de RTS, perfil de GPU por passe + memória de vídeo:

| | antes | depois |
|---|---|---|
| Morte de Tanque, quadro mediano / p95 | 89 / 145 ms (11 qps) | 32 / 41 ms (31 qps) |
| Morte de Tanque, quadros > 50 ms | quase todos | 0 |
| Carga Pesada durante a explosão | ~48 ms (21 qps) | ~30 ms (33 qps), 0 > 50 ms |
| Morte de Tanque, câmera de perto | — | 35 / 45 ms (29 qps) |
| memória de vídeo, pico | 5.7 GB (~800 MB jogados pra RAM) | 5.1 GB |

As causas, todas medidas (árvore do `profilegpu` por volume: `-AutoGpuDump=1.5,7`):
- **Volumes 111 → 13 ms.** O cache de luz de cada volume custava 10–15 ms por luz: a bola, a coluna e o fogo da carcaça têm voxel de ~9 cm (192³ em 17 m; a Carga Pesada tem 40 cm) e as duas luzes de dentro (bola e carcaça) acendiam os três. Abaixo do ultra (`ExplosionVolume.cpp`, `FullDetail`): célula do cache ≥ 45 cm (ainda mais fina que os 80 cm da Carga Pesada aprovada), a luz de dentro só pelo material (sem a luz com sombra) e 96 passos de marcha no alta (`DefaultScalability.ini`, como o Alto da Epic).
- **CPU: streaming bloqueante das texturas de volume, 17–34 ms → 1 ms por quadro** na thread de render. Ao vivo os pedidos agora são assíncronos com busca adiantada; bloqueantes só nas gravações (`-benchmark` e Movie Render Queue).
- **CPU: ~250 consultas do chão por quadro** (cada pedaço, contra cada pedaço de Landscape): 16 → 2 ms. Grade de alturas de 1 m guardada (`BlastLabGround::HeightAtCached`). Os rastros reamostrados em 48 pontos e as cabeças com malha fixa.
- **Memória de vídeo:** no alta, o pool de sombra virtual, o histórico do TSR e o atlas do Lumen nos tamanhos do Alto da Epic (o Épico dobrava os dois primeiros).
- **Travada da 1ª detonação:** os materiais dos pedaços estreavam no 1º quadro (PSO); agora aquecem na abertura (140 → 0 ms).
- O fogo da carcaça (queima 2 min) para de fazer sombra no cenário depois de 6 s abaixo do ultra (~2–3 ms a menos o resto do tempo); nos primeiros 6 s ela fica: a luz mora no casco, e sem a sombra dele o rastro escuro da torre acendia de creme.
- **O que muda na imagem** (`captures/ue/_ab_desempenho.jpg`, mesma história): de cima, nada que se veja; de perto, o topo da coluna um pouco mais cinza-claro aos 8 s (o cache de luz mais grosso). O ultra fica como era (visual máximo, mesmo travando).
- Ainda aberto: a fase de 0.5–3 s da morte de tanque fica em ~39 ms (25 qps) na câmera de RTS (a placa: volumes ~12 ms, sombras e luzes da bola ~7 ms); a grade de alturas guardada não acompanha terreno que mude (hoje ele é fixo); a sombra volumétrica do fogo de dentro só existe no ultra.

**Teste do flipbook (07/10, tarde; o Felipe: "parece jogo de 2003").** A prévia no Cycles da mesma simulação é muito melhor que o volume ao vivo, então testamos o que os AAA fazem: a bola pré-renderizada num cartão.
- `-BallFlipbook=1`: `ABlastFlipbook` e `Tools/build_flipbook.py` (atlas EXR da sessão de arte em `captures/concept/38_flipbook/`, fumaça e fogo em atlas separados com ganhos próprios). O volume da EG fica invisível, mas segue com as luzes e o choque.
- **Resultado até agora, sem enfeite:** tamanho e tempo batem. Mas o atlas foi renderizado em contraluz: a fumaça vira silhueta preta com borda dura e a bola fica bege-marrom aos 0.7–1.0 s, longe da prévia, que tem o sol de lado/frente. Próximo: o mesmo render com a luz da prévia, borda do alfa mais macia e domo menos afundado (`captures/ue/_ab_flipbook_fixed.jpg`).
- **Atlas AgX em cor de tela** (`-FlipbookSet=gas_hero_agx`, `M_FlipbookCardDisplay`): o material inverte o nosso tonemapper em forma fechada. A 1ª versão iterava e fazia pontos coloridos; a ida e volta agora erra poucos %. Tirou o contorno preto. **Mas na cena o cartão sai ~3 EV mais claro que o atlas** (bola creme com pontos vermelhos aos 1 s; `captures/ue/_flipbook_agx_vs_cycles.jpg`, com o volume na mesma luz e história). Dividir pela pré-exposição piorou (tela branca, rodada agx3; desfeito no código, o asset ficou com a versão errada até remontar). Fato da engine: com `BL_ReplacingTonemapper` a cena chega ao nosso tonemapper SEM dividir pela pré-exposição (`PostProcessMaterial.cpp:263`, `MaterialTemplate.ush:3473`). Medir com `TONEMAP_DEBUG` antes de mexer de novo.
- O teste agora carrega só o conjunto de atlas em uso: os sete juntos, ~600 MB, estouravam a memória de vídeo. O aviso de memória aos 6–8 s na vista aberta já existia só com o volume.
- **Parado por ordem do Felipe (07/10):** chega de tentativa e erro. A sessão de arte fecha um plano com estudo (como os estúdios fazem no UE5, o que foge do padrão da Unreal aqui, critérios medidos) antes da próxima mudança.

**Recomeço da morte de tanque (07/10, madrugada).** O Felipe achou "gigante" e pediu um passo atrás, baseado nos vídeos, nas artes e em segundas opiniões. Coordenação da sessão de arte; os estudos ficam em `captures/concept/37_restart_study/`.

- **Escala medida, não chutada:** no vídeo do Veo (`gameplay_v3`, tanque de 7.9 m), contra a largura opaca da gravação (`Intermediate/BakeRaw`).
  - Bola no vídeo: 6–8 m aos 0.3 s, 6.7 × 9.4 m a 1.06 s, 10.5 m a 2.8 s.
  - A Carga Pesada a 0.55, no relógio da gravação, bate nos quatro instantes. A de antes (2.2) era 4× grande.
  - O choque e o tremor seguem o rendimento; a bola segue o alvo (`FBlastWeapon::BallScale`).
- **Corpo novo:** a Gasoline Explosion da JangaFX (EmberGen, CC0), convertida pela sessão de arte pra `BakeRaw/eg_gasoline`.
  - Formato: 192³, 17.1 m, em metros reais.
  - Montagem: `BLAST_BAKE=eg_gasoline` no `build_explosion.py` gera `SVT_/DA_/M_Explosion_EgGasoline`, sem os remendos do corpo do WebGL; o detalhe vem da simulação.
  - A arma aponta pra ela (`FBlastWeapon::Body`); faltando o corpo, cai na Carga Pesada em 0.55.
  - O ganho de fuligem por quadro (`UExplosionBake::SootGain`) segura a fumaça que o EmberGen dissolve.
- **Saiu:**
  - a saia de poeira analítica dentro do volume, na morte de tanque (vira camada própria);
  - a estrela de lente (`-FlashStar` traz de volta pra comparar).
- **Destroços** (`ABlastDebris`, `Tools/build_debris.py`):
  - chapas pretas recortadas, girando e com sombra;
  - cometas com cabeça branca e rastro de fumaça em fita (borda comida por Worley);
  - rastro de fuligem com fogo atrás da torre arremessada.
- **Luz da fumaça:** o espalhamento múltiplo (oitavas de Wrenninge) passa a pesar o albedo (ω^n; `MsAlbedo`). Antes ele supunha nuvem branca e enchia a sombra da fuligem de sol quente.
  - A/B do céu nos volumes (0.175 × 1.0) em `captures/ue/_ab_sky_0175_vs_1.jpg`.
- **Câmera em tripé no `-AutoTest`**, sem câmera na mão e sem tremor (`-LiveCam` devolve os dois). Os shaders da torre móvel compilam no aquecimento.
- **A morte de tanque em camadas, todas simuladas** (sessão de arte converte; aqui `BLAST_BAKE=<nome>` monta SVT, DA e material):
  - bola: `eg_gasoline_b` (EmberGen), TempScale 1.8 (A/B `captures/ue/_ab_eg_temp.jpg`);
  - fogo da carcaça: `wreck_fire_loop` (Mantaflow, casco como obstáculo), loop de 4 s no pé do tanque, entrando de 0.8 a 1.3 s, TempScale 0.55 (o calor do Mantaflow não esfria), luz a 2% da curva;
  - coluna: `eg_plume_loop` (Smoke Plume), loop de 3 s a 0.85, base rala até 4 m e topo esvaecendo; a fonte dela está a 8.5 m do meio do cubo e tomba ~55°, então o volume é deslocado pra fonte cair no casco e girado pra tombar pra longe da câmera;
  - anel de poeira: `eg_ring` (Dust Shockwave) como segundo volume, a 35% da densidade, cor do chão.
  - `AExplosionVolume`: loop (`LoopPeriod`, emenda do último quadro no primeiro), parâmetros de material por explosão, luz por escala, clarão e halo desligáveis. O halo de ofuscamento (M_FireGlare) ficou só no corpo antigo: na EG ele lavava a bola de creme por ~1 s.
  - Os quadros empilhados das texturas auxiliares cabem em 2048 de profundidade (a EG tem 74 quadros: 24³ por quadro).
  - Pool de streaming dos corpos novos em 4× o maior quadro: com 12× a 1660 passou 543 MB do orçamento.
- **Correções do painel de 7 juízes sobre a rodada f** (`captures/concept/37_restart_study/05_painel_rodada_f.md`, nota 3.5/10; rodada j em `captures/ue/_hero_close_j_grid.jpg`):
  - poeira num canal de luz próprio (2), só com o céu: a luz do fogo de dentro da bola a acendia de creme;
  - anel rente (achatado a 0.3) e com o miolo livre (4 m: o casco nunca fica coberto);
  - escurecimento da autoexposição ≤ 0.25 EV de dia (às 16 h fechava −0.6 EV no clarão);
  - torre: sobe 12.5 m/s, sai 3 m/s pro lado da câmera, solta 2 quadros depois do clarão e pousa à vista a ~1 tanque; o rastro sem o "sabre" laranja;
  - fogo da carcaça com 1/3 do ganho e luz 0.1; ele cresce 1.35× de 1.3 a 3 s;
  - coluna mais opaca, decaindo pra 35% de 5 a 8 s;
  - 32 chapas de 0.4–1.4 m.
  - Falta: o núcleo HDR do clarão, faíscas em quantidade (40–80 espinhos + fagulhas), marca no chão e carcaça queimada, o topo da coluna.
- **Rodadas l–p (07/10, manhã):**
  - a torre sai de lado na tela (perpendicular à linha de visão), sem tapar a bola;
  - o creme da bola aos 0.8–1.0 s era a luz de DENTRO dela acendendo a própria fumaça rala (a temperatura mais baixa não mudava nada). Com a luz de dentro a 0.3 (`VolumeLightScale`; a do chão fica) e a temperatura 1.3× só de 0.6 a 1.2 s, a couve-flor fica amarelo-laranja;
  - a luz do fogo da carcaça no cenário fica em 0.1, mas na fumaça em 0.02: mais que isso acendia a coluna de marrom;
  - o fogo da carcaça usa a curva de emissão comprimida (0.55): com a física (1.0) o tonemapper clipava o miolo em branco antes da borda aparecer;
  - A/B sem recompilar: `-BallVolumeLight=`, `-BallTempDip=`, `-PlumeSootExt=`, `-WreckFireParams=Nome=valor;...`
  - Folha contra o vídeo: `captures/ue/_veo_vs_hero_close_p.jpg`.
- **O primeiro segundo e a luz do vídeo (07/10, manhã; decisão do Felipe: hora dourada com poeira no ar):**
  - núcleo do clarão (o halo de 9.5 m, 0–0.15 s), luz do clarão com τ 0.08 s a 1 m do chão (o "anel de luz" é o chão aceso), 60 espinhos e 150 fagulhas (`ABlastDebris`);
  - **"poeira no ar"** (`-Air=desert` ou o botão no painel; o padrão continua o céu atual): aerossol de poeira (Mie 40× o limpo da Unreal, AOD ~0.3, camada de 2 km, g 0.7, tom 1:0.85:0.65) e névoa 2.5× mais densa e quente;
  - **`-Backlight=<graus>`** (teste de comparação): o sol na frente da câmera, como no Veo, travado depois de 1.5 s;
  - capturas herói de comparação: `-Hour 16.75` e `-GameArgs "... -Air=desert -Backlight=12"`. Folha: `captures/ue/_veo_vs_hero_close_s2.jpg`;
  - a bola aos 0.6–1.0 s na luz dourada mede R/B 2.0–2.4 (alvo ≥ 2), menos aos 0.8 s (1.7);
  - núcleo do clarão de 14 m (~1.8 tanque), τ 0.1 s;
  - coluna em contraluz: saía cinza-marrom (L ~80 contra ≤ 50 no vídeo). Um arbusto escuro à mesma distância fica em 28–45, então não é a névoa: é a fuligem refletindo o céu. A fuligem da plume e da EG passa a albedo 0.15 × 0.55 = 0.08 (era 0.165);
  - **achado sobre a exposição:** com o sol dentro do quadro a exposição fica presa na borda do histograma e mudar o alvo por hora não a move (EV100 ~12.3 com o alvo de 10.9 a 12.1). O bias move (-AEBias=2.5: 12.5 → 11.45 EV100). Vai importar pra câmera de cinema; as comparações ficam na exposição do jogo (bias 1.5).

**Cena herói (07/10, madrugada; decisão da sessão de arte com o Felipe).** Em vez de espalhar por fases, uma tomada só de ponta a ponta: a câmera baixa do vídeo do Veo (a morte de tanque), no mapa `Range_Desert_Units`. O mapa tem 4 tanques USA da sessão de arte; a vítima fica na origem, com as tags HeroVictim + Hull/Turret/Barrel. O som ficou pra depois: foco 100% visual. Ordem:
1. anti-slime e TV velha;
2. morte de tanque (tamanho, torre e cano voando);
3. carcaça e coluna;
4. faíscas-cometa e chapas;
5. anel.

- **Arma nova "Morte de Tanque"** (`-AutoWeapon=6`): bola 2.2× a Carga Pesada (~25 m), no relógio da gravação (tamanho e tempo desacoplados: `AExplosionVolume::TimeScale`).
- **A torre e o cano da vítima voam juntos, como um corpo só:** 18 m/s, girando 300°/s, quicam uma vez e ficam.
- **A carcaça queima 2 min:** fonte de "fogo contínuo" (tipo 7) no fluido do campo, com fuligem e calor num cilindro de 2.2 m. A chama vai até 3 m e treme a 0.75 Hz; o empuxo do campo sobe a coluna preta.
- **Anti-slime:**
  - piroclástico em 4 oitavas (2.4 → 0.3 m), metade da amplitude por oitava;
  - fresta escura só na casca já fria;
  - línguas de chama no fogo quente;
  - separação quente/frio mais suave (o degrau virava grão colorido com a marcha sorteada).
- O `blast_metrics.py` mede também o "grão" (desvio da cor de cada pixel da média 5×5 nas partes claras): linha de base 3.0–3.3 aos 0.8–1 s.

**Fase A do arrepio (07/10; plano único em EXPLOSOES §6 + CONCEITO_EXPLOSOES §4d).**

O alvo virou o vídeo do Veo (morte de tanque) e as artes por tipo (§4e). A sessão de arte mede com a folha quadro a quadro; aqui, `Tools/blast_metrics.py` (janela de 24 m pela posição registrada no log).

- **A1, curva física:** a emissão vai com expoente 1.0 (o WebGL usava 0.44–0.6: a fuligem fria brilhava 24–90× demais, o "marrom"). Ganho 16 e temperatura 0.9.
- **A3, a bola é só fuligem e fogo:** sem a poeira da gravação dentro; fuligem 2.5× mais opaca; poeira da saia com albedo 0.95.
- **Brasas nas rachaduras:** os topos das bolhas do piroclástico esfriam e o quente vaza pelos vales.
- Medido às 16h (sol a 15°), comparando o `cmp_rts` (antes) com o `cmp_rts_a`:
  - saturação da parte escura: 1.0 → 0.41 aos 1.5 s, 0.31 aos 2 s (alvo ~0.4; antes era "bronze");
  - p5 da luminância: 0.11 a partir de 1.3 s (critério ≤ 0.18);
  - aos 0.8–1 s, casca preta com o fogo pelas rachaduras; aos 1.3–1.5 s, preto com brasas.
- **A4, clarão:**
  - luz própria (FlashLight: sem sombra, sem Lumen, sem névoa) e estrela de raios (M_FlashStar);
  - a curva da luz gravada levada à física (expoente 1/0.44);
  - a luz da bola no cenário com valores físicos (fator 50 → 4: o pico dava ~5e8 cd, a física ~4e7);
  - de dia a exposição fecha no máximo 0.3 EV;
  - estrela de 24 m (2–3× a bola), visível até ~0.13 s;
  - medido (`cmp_rts_d`): o chão volta ao normal já aos 0.10 s, com mediana 0.67 (antes, creme até 0.4 s).
  - O "limão" que sobra (1.7% aos 1.5 s) é o núcleo amarelo indo pro branco: rgb (0.97, 0.94, 0.62), matiz 52–60°, nenhum pixel ≥ 60°.
- **A5, evento único:** o choque chega ao ponto de mira em d/343 s e dispara o tremor junto:
  - trauma², só rotação, ruído a 18 Hz, decai em ~1 s;
  - teto global; botão "tremor" no painel e `-NoShake`;
  - gancho do som no mesmo instante. Os sons vêm da especificação da sessão de arte, e o download só com ok do Felipe.

**A explosão rumo à arte alvo (06/10, noite; fase 1 refeita, em andamento).**

O alvo agora é uma arte só: `docs/concept/explosions/target_ground_blast.webp` (a `29_game_shot/a_gemini`). Ela tem núcleo branco-amarelo estourado, fumaça preta subindo e terra indo pros lados, tudo em bolhas nítidas. A folha de comparação é `Tools/strip.py`: ela recorta os nossos quadros na mesma largura em metros que a arte (~30 m), pela posição que o teste automático registra.

Medido antes de mexer:
- Na câmera de RTS a explosão saía borrada. O ruído de detalhe (0.3–0.07 m) ficava abaixo do passo de 0.4 m e do pixel.
- A emissão com expoente 0.44 achatava a bola num creme uniforme.
- Dobrar a resolução do volume quase não mudou nada: o borrão estava no próprio volume.

O que entrou no material (`build_explosion.py`; tudo é parâmetro, com A/B por `-ExplosionParams=Nome=valor;...`):
- **Deslocamento piroclástico:** a superfície da fumaça anda ao longo da própria normal pelo ruído Worley de 2.4/1.2 m. A normal é pré-calculada da gravação de baixa resolução.
- **Erosão da casca rala** (Nubis), só onde a fumaça já esfriou.
- **Sombreado das bolhas no albedo:** fresta escura e lado oposto ao sol. O cache de luz do volume (0.8 m) não vê as bolhas.
- **Borda afiada:** a névoa rala some.
- **Cor:**
  - expoente 0.6 e ganho 6: o núcleo estoura em branco como na arte, que tem mais da metade da bola com saturação ≤ 0.11;
  - temperatura no mapa 0.75;
  - lóbulos frios pela temperatura;
  - a fuligem abaixo de ~1100 K não brilha.
- **Saia de poeira** analítica no mesmo volume, que compõe e faz sombra com o fogo:
  - frente de 15 m que freia, altura de 5 m e tronco no meio;
  - bolhas que crescem com a frente;
  - assentamento a partir de 0.8 s;
  - cor por campo (`DustTint`).

Custo, ainda alto, na câmera de RTS e qualidade alta: o quadro vai de 16 a 38 ms (~8 ms da bola e ~15 ms da saia, pela caixa marchada maior). A otimização fica pra depois do visual.

Falta pra chegar na arte:
- a nitidez das bolhas, porque a gravação de 96³ é lisa;
- a coluna preta subindo de lado;
- os destroços.

**Campos com chão de verdade (06/10, noite).**

Pedido do Felipe: testar as explosões num mundo do WorldForge, com deserto, neve, estrada e grama, e no futuro impacto em árvores.

- **O que já existe:** o WorldForge gera um bioma por mapa. As três fatias de qualidade dele viraram campos do simulador:
  - `Range_Desert`: deserto com estrada de asfalto;
  - `Range_Snow`: neve com pinheiros;
  - `Range_Forest`: grama com mata.
  - `blast.ps1 worlds` copia as fatias (1,3 GB, fora do git como lá) e monta os mapas (`build_worlds.py`).
  - A luz da fatia sai e entram o céu, o sol, a hora do dia e a exposição do simulador, com que as explosões foram calibradas.
  - Além da borda (~252 m) fica o chão liso do campo de teste.
- **A Arena** (os quatro chãos juntos, com material físico por camada: PM_Sand, PM_Snow, PM_Grass, PM_Asphalt, PM_Dirt, PM_Rock) foi pedida à sessão do WorldForge. Ela entrou na fila depois dos itens que o Felipe já tinha pedido lá. `blast.ps1 worlds` já monta a `Range_Arena` quando ela existir.
- **No painel:** a seção CAMPO troca de campo (teste, deserto, neve, floresta) levando a hora junto.
- **Relevo:** o clique, a câmera, o avião e a sombra da fumaça seguiam o plano z = 0. Agora seguem o terreno (`BlastLabGround`: traço só contra o Landscape, sem rochas e árvores). A câmera não rola pra fora do terreno.
- **Árvores:** precisam do plugin Procedural Vegetation Editor e de `r.Nanite.Foliage=1` (ligado no BlastLab; recompila os shaders uma vez).

**Noite e hora dourada (06/10, tarde).**

O Felipe viu "dois sóis" e roxo. Medido (`captures/ue/_night_ab.jpg`, `_gold2.jpg`), eram três coisas:

1. **A lua virava uma bola branca de ~12°.**
   - A auréola herdada do WebGL era ~400× a física, e com a exposição da noite estourava em branco.
   - Agora a lua tem brilho de TELA fixo, como nos jogos (EyeAdaptationInverse):
     - disco 2× o real, com mares;
     - brilho em volta medido na noite aprovada (`captures/concept/11_night_tilt`);
     - extinção pela massa de ar (Kasten & Young).
2. **O céu da lua era o azul do dia, 1.7× mais claro que o chão.**
   - Nas noites aprovadas ele é 0.3–0.8× o chão.
   - A parte do céu iluminada pela lua escurece ×0.29 só na tela; a luz ambiente fica igual (`NightSky`, BlastLabEnvironment).
   - As estrelas passam a ter contagem real (N(<m) ≈ 10^(0.475m + 0.87)) e só aparecem acima da magnitude-limite do céu atrás delas (Crumey 2014): com lua cheia, poucas e fracas.
3. **O botão "dourada" (17:27, herdado do WebGL) caía com o sol 2° abaixo do horizonte.**
   - O chão saía magenta (matiz −31°, contra 63–72° nas artes).
   - Agora fica às 16:45 (sol a +6.4°): luz quente e sombras longas.

Em aberto: o crepúsculo civil (17:36) sai rosa-magenta forte no chão (croma 24 num chão cinza). Proposta: balanço de branco por hora com adaptação parcial (CIECAM02), a mostrar em A/B.

**Plano das explosões novas:** `docs/EXPLOSOES.md` (pesquisa de mercado, física e técnica + segunda opinião). As fases estão na memória (`project-fases-explosoes`).

**Fase 1, item 1: sombra da fumaça no chão (06/10).**

A sombra que o Heterogeneous Volume da 5.8 projeta (`r.HeterogeneousVolumes.Shadows`) não chegou ao chão aqui: com ou sem Virtual Shadow Maps, a diferença foi nula.

No lugar dela entrou um mapa de sombra próprio, a técnica das nuvens da Unreal, do Horizon e do Frostbite:

- **O mapa:** a espessura óptica da fumaça vista da luz principal, em 512² texels de 0.5 m numa janela que segue a câmera. É calculado por `BattleSmoke.usf MainShadow`.
- **A fonte:** a gravação em baixa resolução, que agora guarda a extinção (`export_bake_velocity.py`). Ela usa a mesma conta do material, com o desvanecimento do topo e a erosão das bordas finas.
- **Onde entra:** pela light function do sol e da lua (`M_SmokeShadowLF`), só na luz direta e só perto do chão, até ~10 m. Acima disso o volume já faz a própria sombra.

Custo: 0.06 ms (`GPU/BattleSmokeShadow`).

Medido: aos 2.5 s, uma sombra grande na direção da sombra dos prédios, enquanto a nuvem é densa (`captures/ue/_shadow4_ab.jpg`); aos 4.5 s ela se desfaz junto com a nuvem.

**Fase 1: REJEITADA pelo Felipe (06/10)** ("qualidade de jogo bosta, nada realista").

As camadas abaixo estão desligadas (`UBlastLabSimulation::bLayersEnabled = false`) e os retalhos de chão foram tirados do campo de teste. Só a sombra da fumaça ficou.

O erro foi de execução: primitivas de protótipo (bola aditiva, disco chapado, cubos, decal de ruído), validadas só por tempo e tamanho, sem pôr o quadro ao lado das artes. O que fica aproveitável é a infraestrutura: a receita em JSON relida sozinha, a física dos pedaços e o tipo de chão pelo material físico.

O que foi feito (`ABlastEffects`, vídeo `captures/ue/_fase1_carga.gif`):

Cada arma tem uma receita em `BlastLab/Config/BlastRecipes.json`, que o simulador relê quando o arquivo muda (testado com ele aberto). As camadas são:

- **Clarão:** bola branca aditiva de 60–240 ms.
- **Anel de poeira no chão até o raio do dano:**
  - o raio é o de 5 psi, R = 4.6·W^(1/3): 4.6 m na granada, 12.5 m na Carga Pesada, 112 m na nuke;
  - a frente sai rápido e freia; depois a poeira assenta em 2× o tempo;
  - a poeira é mais clara que o chão.
- **Destroços, torrões e faíscas:**
  - balísticos, com arrasto quadrático (velocidade terminal ∝ √tamanho) e quique no chão;
  - os pedaços ficam no chão e depois afundam;
  - tamanho mínimo de 3 pixels;
  - as faíscas são riscos de 1/30 s.
- **Marca queimada (decal):** com riscos radiais, do raio do dano, ficando 60–600 s.
- **Tipo de chão:** o material físico no ponto (PM_Terra, PM_Areia, PM_Asfalto, PM_Grama, com retalhos no campo de teste) muda a cor dos torrões, a poeira e a fuligem.

As escalas saem da física: quantidades ∝ λ, velocidades e durações ∝ √λ (Froude), com λ = (W/20 kg)^(1/3).

Problema achado e resolvido: na primeira explosão, a Unreal desenhava o decal padrão enquanto compilava o nosso, e isso pintava quadrados escuros no chão. Agora o aquecimento da abertura desenha uma camada minúscula, e os shaders compilam durante o "preparando".

Custo com 5 Cargas Pesadas: ~0.2 ms da sombra, mais os pedaços nas sombras das luzes do fogo, mais a translucidez do anel. O resto é o que já existia (volumes e luzes, ~22 ms no PC de hoje).

A luz do fogo no chão já existia (FireLight). À noite ela faz sombras radiais fortes dos objetos e dos destroços.

**Decisão (06/10, depois do teste do Felipe):** a "entrega" do WebGL (aos 3.3 s
a explosão gravada virava fumaça do fluido do campo) saiu. Ela se via: a
qualidade caía (grade de 1 m contra os ~0.4 m da gravação), a cor mudava, o
movimento ficava lento, a nuvem ficava ~15 s presa no ar, e antes dela o avião e
o míssil não mexiam na explosão. O `STATUS` já dizia "um sistema só, sem troca de
representação"; o porte de 05/10 tinha copiado o desenho do WebGL.

Agora a explosão gravada toca do começo ao fim (sobe e some, ~5 s na Carga
Pesada) e REAGE o tempo todo: o fluido do campo só calcula o vento do que
passa (avião, míssil, disparo, as outras explosões), e esse vento deforma a
própria gravação. Ela é lida no ponto y = x − D(x), o mapa característico
(Tessendorf & Pelfrey; o "rest field" do Houdini; Neyret 2003), com
∂D/∂t + ((u_gravada + u_vento)·∇)D = u_vento, numa grade de 64³ sobre o domínio
de cada explosão atravessada (até 4 de uma vez; `BattleSmoke.usf MainWarp`).
O túnel do corpo do avião e do projétil é um recorte à parte que fica aberto
~1.5 s e fecha das bordas pro meio no último segundo (ver "o B-2 passa e a
fumaça volta" abaixo), e a fumaça espalhada fica mais rala pelo det(I − ∇D). Sem
nada passando, D = 0 e a gravação sai exatamente como é. Segunda opinião (agente,
06/10): a técnica é a do cinema; nos jogos o mais perto é o recorte que fecha
da fumaça do CS2 (sem fluido). A velocidade gravada vai em 32³ por quadro
(`Tools/export_bake_velocity.py`), junto com a altura média da fuligem de cada
quadro (a mira do avião e do míssil, adiantada pelo tempo que eles levam pra
chegar). O rastro do míssil continua sendo fumaça própria do campo. No ar limpo
o campo não tem mais turbulência ambiente (ela mexeria nas explosões, que já
têm a delas).

O vento que deforma é calculado no próprio ponto da explosão, não na grade
de 2 m do campo (que borrava o giro dos vórtices em menos de 1 s e o efeito
quase não aparecia de cima): a esteira do avião é um par de linhas de vórtice
que fica no caminho, desce a w0 = Γ/(2π·b0) e quase não perde força até
~b0/w0 (Holzäpfel 2003, o modelo de duas fases da separação de aeroporto); o
jato dos motores, o arrasto do corpo e o pulso das outras explosões com os
mesmos perfis do fluido. O túnel recortado abre primeiro onde o ruído de detalhe
é ralo (borda rasgada, a erosão do Nubis).

Bloom: o do WebGL é na radiância antes da exposição (limiar 1.7 absoluto) e
soma 7 níveis de 0.052; o porte tinha o limiar relativo à exposição e 0.052 no
total. Agora o limiar segue a exposição a cada quadro (BlastLabEnvironment) e a
força/raios vêm da pirâmide do WebGL (`build_scene.py bloom_stages`). O fogo de
dia e de noite não mudou (quase não passa do limiar, como no WebGL). O traçante
ganhou um brilho próprio (`M_Glow`, calibrado pelo halo medido no WebGL), porque
o bloom da Unreal vem depois do TSR, que apagava a linha fina; "Has Pixel
Animation"/"Responsive AA" nos materiais dele. Projéteis ficam pra outra sessão
(06/10): o rastro do míssil "em contas" não é a deposição (corrigida, fatias que
se encaixam) e segue aberto.

Custo (06/10, com um jogo do WorldForge aberto ao mesmo tempo): 10.6 ms antes,
15.2 ms no meio da explosão com o avião passando (13.6 ms antes, sem passagem).

Bombardeiro pesado (botão B-2, tecla B): asa voadora classe B-2 Spirit, 52.4 m
de envergadura, ~150 t, passagem baixa a 130 m/s, quatro F118 (308 kN). A
esteira é Γ ≈ 225 m²/s (2.5× a do A-10), o par a 41 m; a asa varre uma placa
da envergadura inteira (disturbador tipo 3) e corta a nuvem como uma lâmina. Na
Carga Pesada o par passa por fora da nuvem (ela só leva a pancada do meio, ~3.5
m/s pra baixo); no Ataque Aéreo os vórtices passam por dentro e a puxam em duas
cortinas enroladas. O |D| máximo agora é em m locais (8 m na escala da Carga
Pesada, cresce com a explosão): em m do mundo ele limitava a nuvem de 80 m a
10% de deformação.

**"O B-2 passa e a fumaça volta ao lugar" (06/10, quadro a quadro).** O corte
da asa fechava em ~0.3 s. Decisão do Felipe: estilo jogo, o corte fica 2–3 s
(como os buracos na fumaça do CS2). Duas causas, medidas:
1. O recorte decaía como e^−t/τ: começava a fechar já no primeiro quadro. Agora
   cada ponto guarda quanto tempo falta pra fechar (o miolo nasce com 2.5 s, a
   borda com menos, pelo perfil do corte) e fica inteiro até faltar 1 s: o
   buraco encolhe das bordas pro meio (`CarveLife`/`CarveClose` em
   `BattleSmokeSim.cpp`). Sem a mistura (ν) de antes.
2. O transporte semi-lagrangiano com leitura linear borrava o campo a cada
   quadro (difusão numérica). Medido na GPU (o máximo e a contagem de células
   do recorte, lidos de volta): o pico caía 2.4× mais rápido que o prazo e o
   corte virava uma névoa rala em 30% da grade; aos 0.6 s só 284 células
   continuavam inteiras. Com a Catmull–Rom tricúbica presa entre os 8 vizinhos
   (Fedkiw, Stam & Jensen 2001, interpolação cúbica monotônica contra a
   dissipação numérica), o pico segue o prazo e ~7000 células continuam
   inteiras aos 0.6 s. Vale pra D também (a fumaça empurrada não volta aos poucos).

Quadro a quadro depois (lado, RTS e vídeo `captures/ue/_b2_fin_rts.gif`): o
corte do B-2 fica visível até a nuvem sumir (~4.5 s); o A-10 tira ~20% da
nuvem aos 3.4–3.7 s (antes ~10%, mesma história, câmera de RTS), consequência
de o corte durar mais; o míssil e o disparo não mudaram. Custo: a deformação
tem contagem própria de GPU agora (`GPU/BattleSmokeWarp`, e o fluido do campo
`GPU/BattleSmoke`): 0.17 ms por explosão deformada (mediana, p95 0.18), com a
leitura cúbica; o fluido do campo 1.4 ms enquanto roda. `Tools/gpu_profile.py`
lia o cabeçalho do começo do CSV e jogava fora as linhas com estatísticas que
aparecem no meio da captura (a Unreal grava o cabeçalho completo no fim): corrigido.

Como achar: `-WarpDebug=3` desenha só a fumaça DENTRO do corte. A velocidade
gravada leva o corte junto com a nuvem certo (verificado fora da Unreal:
85% de uma fatia de fumaça continua na fumaça 0.6 s depois). Na Carga
Pesada a nuvem já sai pelo topo do domínio gravado a partir de ~3.2 s (some
~75% da fuligem até 4.9 s): passando tarde, o corte tem pouca nuvem pra
aparecer.

# 05/10/2026 (tarde): avião/míssil/disparo, otimização e a luz dos volumes

Medido contra o WebGL na mesma câmera e instantes (`captures/webglpass/`,
folhas `captures/ue/cmp_pass60_*`, `_ab_*`, `cmp_fire_*3`). Teste automático
agora com a mesma história em todo A/B (`-AutoSeed`, padrão 1) e passo fixo
(`view -GameArgs '-benchmark -fps=60'`), passagens em instantes da gravação
(`-AutoPass=plane:4.5,missile:7,shell:9`).

- **Noite sem estourar**: emissão = a do WebGL (sem o 2.54× de antes), a luz
  do fogo de dentro com sombra no volume e o teto de sombra dele (`FireFill`).
  Dia e noite batem com os quadros do WebGL ao mesmo custo.
- **Otimização como nos jogos** (o papel do Significance Manager): fora da
  tela a explosão não é desenhada (o relógio dela segue); detalhe por quanto da
  tela ela ocupa, com histerese; só as 3 maiores na tela com detalhe máximo; a
  luz do fogo alcança até onde vale 1% do ambiente e só faz sombra quando se
  vê. Com 8 Cargas Pesadas: atrás da câmera 26.9 → 14.5 ms, longe 22.8 → 14.6,
  na frente 43 → 33.5 (sem travadas novas: o pedido de quadro da Sparse Volume
  Texture fica sempre no mip 0).
- **A fumaça que fica no campo** (substituída em 06/10 pela deformação, acima) (`BattleSmokeSubsystem`, `Shaders/Private/BattleSmoke.usf`):
  o fluido do `battlesmoke.js` em compute shaders (fumaça em 1 m, velocidade
  em 2 m, MAC, MUSCL, MacCormack, confinamento de vórtice, Jacobi), desenhado
  pelo mesmo Heterogeneous Volume. A explosão passa pra ele aos 3.3 s (2.8 s se
  algo a atravessa) e a fumaça paira ~15 s. Sem buraco na troca (o material
  compila na abertura, com a caixa na frente da câmera). Custo: 10.3 → 13.6 ms
  no meio da explosão.
- **Avião, míssil e disparo** (`ABlastLabMover`, porte do `movers.js`): botões
  do painel e teclas A/M/D; cruzam a tela da esquerda pra direita pela fumaça
  da última explosão, na altura média da fuligem (lida da GPU; o meio da caixa
  punha o avião por baixo da nuvem, por causa da poeira rente ao chão). O
  avião abre um túnel e o par de vórtices das asas puxa a fumaça em dois tubos;
  o míssil deixa o rastro de propelente e ilumina a fumaça perto do bocal; o
  disparo abre um túnel fino. Batem com o WebGL nas mesmas passagens.
- **Espalhamento múltiplo nos volumes** (oitavas de Wrenninge et al. 2013,
  como o WebGL): o volume da Unreal só faz a oitava 0, e o lado à sombra da
  fumaça saía 8× mais escuro que o WebGL (o rastro do míssil quase preto).
  Agora as oitavas 1–2 que não dependem da sombra entram como emissão nos dois
  materiais (explosão e campo, pra não ter salto na troca): o mais escuro da
  nuvem bate (0.015–0.019 contra 0.013–0.016). A luz do céu nos volumes vem
  do Lumen a 0.175 (a normalização do céu do WebGL): as sombras ganham o tom
  azulado dele. A bola de fogo de dia e de noite não mudou.
- **Sem desfoque de movimento** (o WebGL não tem; na câmera de cima ele borrava
  o mapa e o avião). **Traçante** com espessura mínima de 3 pixels do render e
  a radiância dividida na mesma proporção: com 0.14 m ele tinha 1.5 pixel e o
  TSR (que pesa por 1/(1 + brilho) contra vagalumes) o apagava a um pontilhado.

Diferenças conhecidas:
- O traçante tem menos halo que o do WebGL (o bloom pega menos dele).
- A fumaça tardia (> 8 s) sai mais rala que a do WebGL e, rala, deixa o azul do
  céu passar.
- O custo em tempo real das passagens ainda não foi medido: a medição de 05/10
  foi com dois jogos do WorldForge abertos dividindo a GPU.

# 05/10/2026: o simulador e a hora do dia como no WebGL

O simulador existe (`blast.ps1 view [baixa|media|alta|ultra]`): painel do
WebGL (arsenal de 6 armas, hora do dia com presets e ciclo, pausa/câmera
lenta, qualidade), clique no chão detona, câmera RTS do `camera.js`. Presets:
baixa = Médio@50%, média = Alto@58%, alta = Épico@67%, ultra = Cinematic@100%
(alta ~65 qps no meio de uma explosão na 1660, medido sem outro Unreal aberto).

Hora do dia, medida contra o WebGL na mesma câmera (`captures/webglrts_*`,
`webglcam_*`; folhas `captures/ue/cmp_*`):

- **Exposição = a do WebGL**, hora a hora: EV100 = log2(128 000 / expo)
  (`webgl_time_of_day.json`). A versão anterior usava a exposição "do
  fotômetro" (sunny 16) e escurecia tudo 1.8 stop; as cores do chão e dos
  objetos voltaram às do WebGL (a "refletância física" só compensava aquilo).
- **O céu aprovado do WebGL tem um desvio**: na LUT de multi-espalhamento os
  raios que descem atravessam a Terra (raySphere devolve a raiz de saída), o
  que dá ~4× o espalhamento múltiplo de Hillaire e um céu ~1 stop mais claro.
  A Sky Atmosphere da Unreal bate com o Hillaire correto; o céu do WebGL é
  reproduzido com `MultiScatteringFactor` por hora (`Tools/sky_ms_table.py`).
- **Névoa com a cor do WebGL** (`Tools/webgl_fog.json`, lida da GPU dele): o céu
  abaixo do horizonte, quase neutro à noite, magenta no poente, mais o lobo
  laranja do lado do sol (névoa direcional da Unreal). É daí o roxo/laranja.
- **Abóbada do céu** (`M_Sky`, material de céu): estrelas, Via Láctea, airglow
  e a lua com mares e halo, portados do `glsl.js`, fora da captura da luz
  ambiente (o halo virava luz pontual no Lumen e manchava o chão).
- **Adaptação às explosões**: o histograma da Unreal medindo a tela toda fechava
  a câmera no céu de poente e no halo da lua. Agora uma máscara (`M_ExposureMask`,
  desenhada pelo C++ a cada quadro) deixa só o chão até 700 m, como o medidor do
  WebGL; média dos 50–98% mais claros, limiar 0.18·2^2.5. Em repouso a exposição
  é exatamente a da hora; o tanque à noite fecha 3.4–5 stops (WebGL 4.7) e o
  quanto pode fechar segue o WebGL (~1 stop de dia, até 8 à noite).
- **Bug antigo do chão**: a máscara da rugosidade pegava o vermelho do albedo
  (a máscara nasce com r e g ligados): o chão era quase espelho e refletia o
  céu no rasante. Corrigido; o chão distante bate com o WebGL (±0.4 stop).
- `blast.ps1 compile` ignora o Live Coding de outro UnrealEditor aberto
  (`-NoHotReloadFromIDE`); `view -GameArgs '-AELow=.. -AEHigh=.. -AEBias=..'`
  calibra a medição sem remontar o mapa.

# 04/10/2026: BlastLab, o simulador no Unreal Engine 5.8

Repositório reorganizado: o simulador é o projeto `BlastLab/` (Unreal 5.8,
Windows 11, GTX 1660), o WebGL foi pra `reference/webgl/` (intacto) e os
documentos pra `docs/`. Mapa e comandos no `README.md`. Tudo do `Content/` é
gerado pelas ferramentas e testado por render de linha de comando:

```
powershell -File BlastLab\Tools\blast.ps1 setup                    # compila e monta tudo do zero
powershell -File BlastLab\Tools\blast.ps1 view                     # abre o simulador (gráfico máximo)
powershell -File BlastLab\Tools\blast.ps1 render Seq_Explosion     # quadros em captures\ue\
  ... -Folder <nome> -Cvars 'r.X=0'   (A/B)    ... -ProfileGpu   (GPU na qualidade de jogo → Tools\gpu_profile.py)
python BlastLab\Tools\compare.py <saida.jpg> <pasta> [<pasta>]      # folha WebGL × Unreal
```

O que já está no Unreal (fases 0–2 do `MIGRACAO.md`):

- **Laço de teste**: build C++ (módulos `BlastLab` e `BlastLabEditor`),
  scripts Python no commandlet, Movie Render Queue por linha de comando,
  janela do simulador com teste automático (`view -Folder`).
- **Cena**: layout do WebGL, chão rachado, sol de 128 000 lux, Sky
  Atmosphere, exposição física (EV100), tonemap do WebGL portado como
  material de pós-processo.
- **Explosão**: a gravação r96 do WebGL num Sparse Volume Texture tocado num
  Heterogeneous Volume (ator `ExplosionVolume`):
  - leitura trilinear como no WebGL (`r.HeterogeneousVolumes.StochasticFiltering=0`;
    o padrão cúbico estocástico borrava e granulava);
  - ruído de detalhe do WebGL (flow noise de duas fases, modulação, erosão
    das bordas finas), que devolveu a borda escura da bola de fogo;
  - transição contínua entre os quadros gravados com vetores de movimento
    (componente `FrameBlend`): o volume da Unreal mostrava um quadro
    por vez e a fumaça andava aos pulos (aspereza do movimento 1.33 → 0.07).
    Velocidade em meia precisão numa faixa de 8 voxels em volta da fumaça —
    em 8 bits o "zero" era −40 m/s e as bordas que avançam deixavam um fantasma;
  - caixa marchada justa por quadro: o volume marcha a caixa inteira voxel a
    voxel sem pular o vazio; encolher a caixa pra fumaça (união do quadro e do
    seguinte) cortou o custo de 15.5 ms pra 1.5–4.3 ms (720p, qualidade
    Épico de jogo, GTX 1660), imagem igual;
  - duas luzes do fogo animadas pela curva gravada (cenário com sombra +
    dentro do volume).

Medições que valem lembrar:
- O Movie Render Queue sobe tudo pra Cinematic (volume em resolução cheia,
  1024 passos): 55 ms por explosão. O preset de perfil mede na qualidade de
  jogo. Nas predefinições da Epic o volume é desligado em Baixo/Médio.
- O pipeline de pré-sombreamento (`r.HeterogeneousVolumes.Preshading=1`)
  derruba o motor na 5.8.
- Cena sem explosão: ~15 ms de GPU a 720p (TSR 3 ms, Lumen ~5 ms).

- Janela ao vivo (`view`): ~13 qps no gráfico máximo a 1600×900 na 1660.

Em aberto, nesta ordem:
- **O simulador de verdade** (pedido de 04/10): painel como o do WebGL
  (arsenal, hora do dia, tempo/pausa, qualidade), explosão onde clicar,
  câmera RTS. Levantamento do painel do WebGL feito (main.js/index.html).
- **Exposição ao vivo**: na janela ao vivo a exposição automática da Unreal
  fecha até o limite no clarão e demora a voltar (a cena fica escura); nos
  renders não aparece. Portar a exposição do WebGL (exposure.js).
- A nuvem some depois de ~5 s: a gravação acaba no teto da caixa de 38 m e
  no WebGL a fumaça passava pra grade do campo. Plano: um sistema só (gravação
  mais longa/alta, ou continuação), sem troca de representação.
- Bola de fogo de 0.4–0.8 s um pouco menos saturada que no WebGL (mediana de
  saturação 0.51 × 0.58); o miolo bate.
- Noite, interação (avião, míssil, disparo), presets e desempenho com várias
  explosões.

---

# Decisão — 03/10/2026

**DECISÃO (03/10/2026): migrar pra Unreal Engine.** Este commit (tag
`v6-fumaca-interacao`) é a referência final do WebGL. Os últimos problemas —
fumaça "dura", fogo que muda na troca gravação → grade, explosão que não reage
nos primeiros 3 s — vêm de limites do WebGL (sem compute shader, grade grossa,
sem composição de volumes que se interpenetram, explosão como gravação). Na
Unreal: Niagara Fluids (simulação por explosão, reage a objetos),
Heterogeneous Volumes (composição correta), Sparse Volume Textures (gravações
de volume deformáveis), TSR, MegaLights/VSM. O que leva daqui: os números
calibrados, as lições (ARCHITECTURE.md) e os vídeos/imagens como alvo visual
(em `captures/`, fora do git: `aviao_por_tras.mp4`, `aviao_missil_lado.mp4`,
`noite_troca*.jpg`; o app desta tag regenera qualquer um).
Próximos passos: documento de migração (no que cada sistema vira, quais
números, quais referências) e projeto Unreal com captura automática (Python
+ render por linha de comando) pra continuar testando quadro a quadro.

---

# Histórico — até 02/10/2026

Rodar: `python3 -m http.server 8129` na raiz e abrir http://localhost:8129
(ou o preview `explosion` do `.claude/launch.json`).

## O que já está funcionando

Pipeline completo de ponta a ponta, sem dependências, WebGL2 puro:

- **Solver de fluido 3D na GPU** (`js/fluid.js`) — advecção MacCormack com
  limitador, combustão por coordenada de reação, resfriamento radiativo T⁴,
  empuxo, vorticity confinement, turbulência curl-noise (textura 3D tileável
  pré-computada), anel de vórtice toroidal na detonação, surto de base com
  escala Taylor–Sedov (R ∝ t^0.4), projeção de pressão por Jacobi com warm
  start e fronteiras abertas (Dirichlet nas laterais/topo, Neumann no chão).
- **Emissão de corpo negro física** (`js/blackbody.js`) — LUT integrando
  Planck × CIE (fit gaussiano de Wyman/JCGT 2013) → sRGB linear. Verificada:
  1400K sai vermelho-profundo, 3500K branco-amarelado, intensidade caindo 5
  ordens de grandeza. Nenhum gradiente pintado à mão.
- **Raymarch volumétrico** (`js/volumeRender.js`) — integração
  energia-conservante por passo, espalhamento múltiplo em 3 octaves, phase
  Henyey–Greenstein dupla, meia resolução com jitter + upsample de
  profundidade-mais-próxima, distorção por calor.
- **Empty-space skipping com DDA exato** por blocos macro — salta até a
  fronteira do bloco, nunca atravessa detalhe fino.
- **A bola de fogo é uma luz real** — redução GPU do volume de emissão em
  cadeia 4×4 até 1×1, lida via PBO + fenceSync (assíncrono, sem stall).
- **Cena PBR** (`js/scene.js`, `js/layout.js`) — 376 props instanciados,
  materiais procedurais, shadow map do sol + shadow map dinâmico do fogo +
  sombra volumétrica marchada pelo volume de fumaça, céu procedural e névoa
  analítica de altura que usa a MESMA função do céu.
- **Post** (`js/post.js`) — bloom progressivo com média de Karis, streak
  anamórfico, ACES, aberração cromática, vinheta, grain, dither.
- **Hora do dia completa** (`js/atmosphere.js`, `js/celestial.js`) — modelo
  atmosférico de Hillaire 2020 em 4 LUTs, posição solar real por
  latitude/dia/hora, luz-chave única sol→lua, exposição analítica,
  estrelas/Via Láctea/lua. Slider + presets + ciclo automático na UI,
  teclas `,` `.` `t`. Validado de meia-noite a meio-dia.
- **Ferramentas**: `__app.prof.totals()` (timer queries de GPU, uma por
  seção por frame), `__app.bench()`, `__app.renderAt(t)`.

## Onde parei — os dois problemas abertos

### 1. Performance — 31 → 22 ms/frame (45 fps) no preset `alta`

Resolvido o suficiente. Detalhes e tabela em `ARCHITECTURE.md` §4b.

Principal armadilha, registrada pra não se repetir: **os timer queries de GPU
estavam mentindo** (apontavam `post` e `luzFogo` como gargalos; ambos custam
~0). A medição confiável é `App.ablate()`, que usa `readPixels` no
framebuffer **padrão** como barreira de sincronização.

Se precisar de mais: `sim` (7.8 ms) e `cena` (7.6 ms) são os maiores. Para
`sim`, o caminho é desacoplar a taxa da simulação da taxa de render (fumaça
não precisa de 60 Hz). Para `cena`, assar também o material dos props.

### 1b. Realismo da transição explosão → fumaça — reescrito em 01/10

A crítica era que fumaça e fogo estavam volumosos mas "a explosão em si
virando fumaça" não convencia. Diagnóstico feito com as ferramentas novas
(`__capture` e `__fields`, ver abaixo): os CAMPOS estavam lisos — não era o
renderizador escondendo estrutura.

Mudanças, em ordem de impacto:
1. Combustão limitada por mistura (antes: por temperatura). Ver ARCHITECTURE §1.
2. Fuligem com formação no lado rico e oxidação no lado pobre.
3. Detonação como fonte de pressão em vez de velocidade imposta.
4. Turbulência concentrada na interface de densidade.
5. Estrutura na condição inicial (borda nítida + mosqueado volumétrico):
   nos primeiros 200ms a bola tem ~12 células e o solver não gera
   turbulência sozinho nessa escala.
6. Empuxo rebalanceado — `sootWeight` 16 quase anulava `buoyancy` 52 e a
   coluna não subia. A massa de fuligem é ínfima; foi pra 3.4.
7. Resfriamento com auto-absorção (o fogo apagava em 0.5s).
8. Faixa dinâmica da emissão: a superfície estava ~10× acima do joelho do
   ACES, então toda variação de temperatura virava o mesmo branco.

Ainda imperfeito: os primeiros ~0.3 s continuam saturando em branco, e a
coluna de fumaça tardia é macia demais. Próximos passos seriam detalhe
sub-grid mais agressivo na fumaça e, se houver orçamento, resolução maior
só durante a fase quente.

### Ferramentas de inspeção

- `__capture('x.png', {times:[...]})` — folha de contato da evolução
  temporal com a câmera TRAVADA. Grava via POST pro `devserver.py`, então
  funciona mesmo com a janela do app minimizada.
- `__fields('x.png', {t:0.2})` — o mesmo instante em vários CAMPOS
  (temperatura, fuligem, combustível, frente de chama, emissão). Distingue
  "a simulação está lisa" de "o renderizador está escondendo a estrutura".
  Cuidado: o raymarch corta por opacidade, então nos modos de debug o
  early-out é desligado — senão só se vê a casca externa.

### 2. Escala física do evento — RESOLVIDO em 01/10

Aplicado: `blastSpeed` 36→19 com `drag` 0.06→0.55, `cooling` 1.75→0.72,
`burnRate` 2.6→1.45, `surgeSpeed` 16→6.5, `DOMAIN` 27→31 m, shot da câmera
reenquadrado pra ~40–52 m, `domainFade` estreitado. A explosão não transborda
mais o quadro e a bola de fogo sobrevive alguns segundos.

Ainda dá pra melhorar: a fase inicial ainda lê como um disco achatado por
causa do surto de base; vale separar melhor a bola de fogo (que sobe) da
poeira rasteira (que espalha).

## Qualidade de imagem / integração — rev. 3 (01/10)

Crítica: fogo e fumaça pararam de projetar sombras bonitas e a explosão
parecia "um PNG colado", não ambientada.

Causa raiz: a intensidade da LUZ da bola de fogo era derivada do mesmo
`emissionGain` que controla o RENDER. Ao baixar o ganho pra conter o estouro
de branco, a luz da cena caiu junto — `fireColor` tinha ido a 0.03 contra
~0.7 de um raio de sol. Desacoplado (a redução de momentos usa ganho fixo).

Também entrou:
- Sombra volumétrica mais profunda (`fireTauCap` 1.6 → 3.2).
- Luz de preenchimento do fogo com difusa envolvente (ver ARCHITECTURE §3).
- Erosão de borda modulada por ruído, ATENUADA pela densidade — a primeira
  versão subtraía valor absoluto e apagava a pluma tardia inteira.
- **Sistema de brasas** (`js/particles.js`), que faltava desde o começo.
- Grain reduzido e uniforme (pesar pras sombras destruía a cena noturna).

Performance mantida: ~70 fps no preset `alta`.

Ainda aberto:
- Os primeiros ~0.3 s continuam saturando em branco.
- Sem SSAO — o MRT de normal + fração-de-ambiente existe e segue sem
  consumidor; é o que daria sombra de contato nos props.
- Volume ainda em meia resolução; reprojeção temporal nunca feita.
- A fumaça tardia rarefaz bastante depois de ~4 s.

## Qualidade de imagem — rev. 4 (01/10)

Crítica: "parece 2012, parece um PNG, não faz parte do ambiente". Quatro bugs
distintos, nenhum deles no simulador:

1. **Tempestade de aliasing.** O chão assado não tinha mipmap, e o ruído
   procedural dos props não tinha LOD nenhum. Em ângulo rasante cada pixel
   cobria dezenas de texels → a cena inteira salpicada. Corrigido com
   `enableMipAniso()` (mipmap + anisotropia 16×) e `detailLOD()`, que atenua
   cada termo de alta frequência pelo tamanho do pixel em metros via
   `fwidth`.
2. **Normais aleatórias nos props** (o padrão "dálmata"/isopor). O bump por
   diferenças finitas divide a amplitude por `e = 0.03`, então o fator 0.30
   produzia perturbação de ~0.9 — da mesma ordem da própria normal. Além
   disso a atenuação por LOD entrava como `if (ld > 0.02)`, um limiar duro
   sobre valor contínuo: pixels vizinhos alternavam entre perturbado e liso.
   Agora amplitude 0.02 e LOD por multiplicação suave.
3. **Contorno escuro na bola de fogo** — o sinal real de "adesivo". O canal
   de poeira ABSORVE mas não emite, e a poeira colada na bola recebia
   transmitância ~0 em direção ao fogo (mesma quebra da aproximação de luz
   pontual já corrigida na cena, esquecida no volume de luz). Resolvido com
   `tauCap = 2.2` no canal de fogo do volume de luz.
   Também corrigido um bug no upsample: `abs(hd - min(dFull,1e8))` clampava
   só um lado, então céu-contra-céu dava 9e8 e o teste caía em point-sampling
   bloquiado exatamente na silhueta.
4. **Fuligem sendo consumida.** `sootOxid` 1.25 comia quase toda ela — a bola
   de fogo ficava sem o que emite e sobrava pouca fumaça. Baixado pra 0.30.

Também: perspectiva aérea aplicada ao volume no composite. A radiância do
volume era somada crua, sem atravessar ar nenhum, enquanto tudo atrás dela
atravessava 60 m — contribuía pro efeito de "colado na frente".

Performance: ~82 fps no preset `alta`.

Ainda aberto: sem SSAO (sem sombra de contato); volume em meia resolução;
primeiros ~0.3 s ainda saturam; a cena ainda é "limpa" demais (falta sujeira
e variação de material).

## rev. 5 (01/10) — pendências fechadas

- **SSAO** construído (`js/ssao.js`) e consumindo o MRT de normal +
  fração-de-ambiente que existia sem uso desde o início. Ver ARCHITECTURE §4d.
- **Resolução do volume** virou parâmetro de qualidade (`volScale`):
  alta = 0.72, ultra = 1.0. Era fixo em 0.5 e era o maior responsável pela
  maciez da fumaça.
- **Saturação dos primeiros 0.3 s** — ganho de emissão reduzido, brilho
  devolvido pelo bloom, e preservação de matiz no tonemap (ARCHITECTURE §4e).
- **Sujeira e variação de material** — manchas de óleo e areia no chão em
  escalas diferentes da do tile (evita repetição visível); pó assentado nas
  faces viradas pra cima, encardido acumulado na base dos props e escorrido
  vertical nas laterais.

Performance: ~47 fps no preset `alta` (era 82 antes destes quatro).

Pendências reais que restam: reprojeção temporal no volume; a cena ainda não
tem decalques nem variação de geometria; os embers não iluminam a cena.

## Bug corrigido — sombra fantasma (01/10)

Sintoma: depois da fumaça sumir, a sombra dela continuava no chão e ia
desaparecendo devagar.

Causa: inconsistência entre render e sombra. `sampleMedium()` aplica erosão
modulada por ruído na densidade antes de renderizar; `volumeShadow()`
marchava o campo CRU. Fumaça residual já apagada da imagem continuava
projetando sombra, e sumia junto com o decaimento lento da fuligem.

Correção em duas camadas:
1. `effExtinction()` em `glsl.js` — uma única definição de "densidade
   efetiva", usada pelo render, pela sombra E pelo volume macro. A sombra usa
   a MÉDIA da erosão (o ruído tem média zero), que basta: ela não precisa do
   detalhe de alta frequência, só precisa concordar em quanta densidade há.
2. Corte duro de resíduo no solver (`minDensity`). Decaimento exponencial tem
   cauda infinita — sobrava densidade baixa demais pra ver e alta o bastante
   pra sombrear. Num RTS isso acumularia a cada explosão.

Lição geral: qualquer efeito aplicado no render e não na simulação cria
divergência entre o que se vê e o que o resto do pipeline acredita existir.

## HUD + detonação por clique (01/10)

Sidebar no estilo C&C (`index.html`), com painéis em metal biselado e acentos
âmbar:
- **Armamento** — cartão com thumbnail. O ícone é um RENDER DO PRÓPRIO MOTOR
  (preset ultra, crepúsculo, enquadramento fechado, recorte 4:3 → 256×192),
  não arte externa: ele sempre reflete como a explosão realmente parece.
  Pra regerar depois de mudar o look, ver a receita no fim desta seção.
- **Ambiente** — slider de hora com gradiente dia/noite, readout em LED verde
  com hora + elevação solar + fase, presets e ciclo automático.
- **Sistema** — qualidade e telemetria ao vivo.

**Clique no terreno detona ali.** O que isso exigiu: o domínio da simulação
era cravado como `#define` na origem do mundo. Virou `uDomainOrigin`,
declarado uma única vez em `COMMON` (várias grades compartilham o domínio e
seriam declarações duplicadas no prelude sufixado) e propagado pros 8 shaders
que convertem mundo↔voxel. A caixa de simulação agora acompanha a detonação.

Arrasto continua orbitando: o clique só detona se o cursor andou menos de 5px.

**A câmera NÃO acompanha a detonação** — num RTS ela fica onde o jogador
deixou e a explosão acontece onde acontecer, inclusive fora de quadro.

Receita do thumbnail (console):
```js
const {heroShot, saveSheet} = await import('/js/contactsheet.js');
__app.setQuality('ultra'); __app.setTimeOfDay(18.1);
const big = heroShot(__app, {t:0.95, w:1024, seed:4242,
  view:{dist:26, azim:-0.55, elev:0.055, targetY:5.2}});
// recorta 4:3 em 256x192 e salva como assets/thumb_blast.png
```

## Múltiplas explosões simultâneas (01/10)

Havia UMA simulação e `detonate()` a resetava: clicar num segundo ponto
apagava o primeiro. `js/blastpool.js` resolve com um pool de slots, cada um
com sua simulação e suas partículas.

Todos os slots compartilham resolução e tamanho de domínio e diferem só na
ORIGEM — por isso os shaders são idênticos e basta trocar texturas e
`uDomainOrigin` entre as passagens.

**Política de passo.** Simular N fluidos por frame seria N× o custo. Só a
fase quente (primeiro ~1.2 s) precisa de 60 Hz; depois a fumaça é lenta e um
passo a cada K frames com dt K× maior é visualmente equivalente — advecção
semi-Lagrangiana é incondicionalmente estável, então dt maior não
desestabiliza. Resultado medido: 3 explosões custam 1.7× uma, não 3×
(13.6 ms → 23.4 ms; 73 → 43 fps).

**Composição volumétrica front-to-back.** Um alvo só, limpo em (0,0,0,1), e
cada volume desenhado do mais próximo ao mais distante com
`blendFuncSeparate(DST_ALPHA, ONE, ZERO, SRC_ALPHA)`:
RGB acumula radiância atenuada pelo que está à frente, A é o produto das
transmitâncias. No alvo auxiliar o alpha fica 1, então o mesmo blend vira
soma pura (profundidade ponderada por cobertura e calor somam).

**Cena.** Sol e cada bola de fogo são ocluídos por TODAS as explosões. O
teste de caixa rejeita na hora quem não está no caminho, então volume
inativo custa quase nada. O shadow map do fogo cobre só a explosão principal.

Armadilha: **array de sampler exige índice constante** neste driver — índice
de laço não conta, apesar do que a spec ES 3.0 sugere. A consulta por
explosão é desenrolada em JS na montagem do shader.

## Escalabilidade (01/10) — bake + instâncias

O pool de simulações resolvia "mais de uma explosão" mas nunca escalaria pra
RTS. Arquitetura correta implementada: o solver vira BAKER, o runtime
instancia. Detalhes em ARCHITECTURE §4f.

Medido: 96 instâncias simultâneas a 5,6 ms. O custo escala com área em tela.

O pool ao vivo continua existindo (tecla `espaço`) como caminho "hero" e como
fonte do bake. Clique no terreno usa o caminho instanciado.

Pendências desta camada:
- Instâncias não projetam sombra volumétrica no terreno (só luz pontual).
- Qualidade do bake é 64³ contra 128³ do ao vivo; dá pra subir se a memória
  permitir (96³ seriam ~177 MB).
- Falta variação: hoje todas instanciam a MESMA sequência, variando só
  escala, seed de ruído e velocidade. Assar 2-3 sequências diferentes
  removeria a repetição.
- Múltiplas magnitudes (granada/tanque/depósito) ainda não existem.

## Camada final entregue (01/10)

Tudo que estava listado como pendente desta fase foi fechado:

- **3 variantes** assadas no mesmo texture array (layer = variante×quadros +
  quadro), um sampler só. 56 quadros cada, 168 camadas, **168 MB**. Sem
  variantes uma barragem repete o mesmo desenho visivelmente.
- **5 magnitudes** (`MAGNITUDES` em `instances.js`) com leis de escala
  corretas: comprimentos E tempos vão com W^(1/3), então magnitude =
  {escala espacial, velocidade de reprodução} e a física sai certa de graça.
  Intensidade da luz com a área, W^(2/3).
- **Sombra volumétrica das instâncias** no terreno. Como todas compartilham o
  texture array, custa 2 samplers no total — independente de quantas houver.
- **HUD com 5 cartões**, cada thumbnail renderizado pelo próprio motor a
  partir da sequência assada (se o visual mudar, o ícone muda junto).

Armadilha que custou tempo: a luz de uma explosão estava sendo ocluída pelo
PRÓPRIO volume dela. A aproximação de luz pontual coloca a fonte no centro da
bola de fogo, então o raio saía atravessando ela inteira; com 4 oclusores
empilhando um teto de 4%, 0.04⁴ ≈ 2.6e-6 e o terreno ficava preto absoluto.
Luzes e oclusores agora carregam identidade e o sombreador pula a si mesmo.

## Bug "o chão parou de brilhar depois de algumas explosões" (02/10)

Não reproduzia com passos síncronos — só no loop real. Causa: várias portas
abertas que se somavam numa sessão de uso normal.

1. **Câmera em `cine` por padrão.** O shot é indexado por `this.time`, que o
   clique nunca reseta; em segundos o alvo sobe até 22 m e a câmera passa a
   olhar pro céu. Padrão agora é `free` (estática, RTS), elevação limitada
   entre 0.06 e 1.35 rad.
2. **Clique perto do horizonte**: raio quase paralelo ao chão, t = −y/dy
   explode e a explosão nascia a 1.4e12 m. Agora há `maxClickRange` (260 m).
3. **`realDt` sem piso** + shake multiplicado por exp(−dt·k): um único dt
   negativo faz o shake crescer exponencialmente (câmera chegou a 1e10 m).
   Piso em 0 no loop e no `update` da câmera, shake limitado a 1.5.
4. **`autoReplay` ligado** detonava a simulação ao vivo no centro a cada 15 s
   — resto da fase de explosão única. Desligado.
5. **Queimado global** cravado na origem e amarrado ao tempo global. Agora uma
   marca persistente por explosão (até 16).
6. **As 5 thumbnails deixavam 5 queimados na origem** no startup.
7. **`safeLoop` só protegia o 1º frame** (o loop re-agendava a si mesmo).
8. **HUD transbordando**: 5 cartões empurravam Ambiente/Sistema pra fora da
   tela. Grade de 2–3 colunas como a barra do C&C, `minmax(0,1fr)`.

Lição: teste síncrono não substitui sessão real. Três desses bugs só existem
com tempo global avançando, input do mouse e o rAF de verdade.

## Boot lento + "parou de emitir luz" (02/10, tarde)

- **Tela preta ~16 s no boot**: o bake rodava síncrono no construtor (3
  variantes × 720 passos de 128³). Virou asset em `assets/bake/<hash>.bin`
  (gzip, 4,4 MB) carregado em ~0,26 s; sem asset, bake incremental com o HUD
  vivo e o "relógio de construção" do C&C nos cartões. Detalhes em
  ARCHITECTURE.md §4f.
- **Explosões sem luz depois de trocar a qualidade**: `_buildSim` recriava o
  bake vazio e nunca rodava, e não liberava nada (~600 MB por troca). Agora o
  bake e as instâncias sobrevivem à troca, e a geração anterior é liberada por
  `trackGL`. A troca também não detona mais nada no centro.
- **Solver dependente do passo**: fade de borda (×0.955) e dissipação de
  velocidade (×0.999) eram por PASSO; agora são por segundo (ref. 1/120,
  idêntico nesse passo). Afetava explosões velhas do pool, que avançam com dt
  maior.
- Verificado: troca de qualidade 10× seguidas sem erro GL, instâncias e
  simulação ao vivo iluminando o chão à noite, asset carregando do zero.

## "Às vezes não dispara luz" / chão some (02/10, noite)

Não era a luz. Depois que uma explosão AO VIVO (tecla espaço, clique antes do
arsenal ficar pronto, ou a antiga troca de qualidade) expirava, os samplers
`uFieldsArr[i]`/`uMacroArr[i]` da cena continuavam apontando pra unidade de
textura que o bake (sampler2DArray) passa a usar. Dois tipos de sampler na
mesma unidade → `INVALID_OPERATION` → os 3 draws da cena (chão, prédios,
cilindros) descartados em todo frame, pra sempre. Só sobrava céu + explosão.

- Fix: slots sem explosão apontam pra uma textura 1×1 vazia (scene.js).
- O loop agora checa `gl.getError()` a cada 120 frames e loga no console —
  erro de GL não lança exceção e era invisível.
- Teste: sessão de ~2000 frames com instâncias de todos os tipos a cada
  0,4 s, explosões ao vivo a cada 3 s, troca de qualidade e de horário:
  0 erros de draw (antes: quebrava assim que a 1ª ao vivo expirava).

## Explosão do clique com a qualidade da tecla espaço (02/10, noite)

Comparação lado a lado no mesmo instante (variante 0 = mesma semente da ao
vivo). Diferenças achadas e resolvidas: passo do bake (1/120 → 1/60), média de
temperatura apagando a chama (agora conserva energia), sem combustível, sem
volume de luz direcional (cache de luz por instância), 1 oitava de
espalhamento (agora 3), sem faíscas (sparks.js, 12288 por explosão — a ao vivo
tem 12288, não 4096: o pool ajusta `count` depois da textura criada), luz na
cena pela metade, e o salto de espaço vazio das variantes 1 e 2 usando o macro
da variante 0. A diferença que sobra é resolução (64³ assado contra 128³ ao
vivo): detalhe fino um pouco mais suave.

Ferramenta: `app.hold = true` congela o loop real, pra capturar quadro a
quadro sem o rAF avançar a simulação no meio.

## Configuração gráfica (02/10, noite)

Presets + gaveta "⚙ personalizar" no painel Sistema, 18 opções em 4 grupos,
salvas no navegador. Detalhe das explosões vai de 48³ a 96³ com assets
prontos pras quatro resoluções. Fumaça-bloqueia-luz-de-outras saiu do padrão
(era ~20 ms numa barragem). Luz fraca das explosões pequenas no fim da vida é
física (luz ∝ área, vida ∝ tamanho), não bug: a Carga Pesada do clique tem a
mesma curva de luz da ao vivo; "Brilho das explosões" ajusta.

## Explosões sobrepostas (02/10, noite)

Várias pequenas + uma grande em cima: a fumaça das pequenas aparecia colada
por cima da bola de fogo. Agora volumes que se interpenetram são marchados
juntos (ARCHITECTURE.md §4f2). Caminho percorrido, pra não repetir: a luz
cruzada sem auto-sombra deixava a fumaça das pequenas "algodão branco"; a
aproximação pelo céu olhava pra cima com o fogo embaixo; e a linha reta era a
face dos blocos macro da grande (luz pulada junto com a densidade vazia).
Ferramentas: `inst.params.joint` e `inst.params.debug`.

Git: `v1-antes-composicao-conjunta` (estado anterior) e
`v2-composicao-conjunta`.

## Noite (Ghost of Tsushima) + sombras da luz do fogo (02/10, noite) — tag v4-noite-sombras

Aprovado pelo usuário. Detalhes em ARCHITECTURE.md §2c
e §3.

- Sombra "atrasada": o mapa de 145° pra baixo só cobria ~7 m em volta de uma
  bola de fogo baixa → 2 cubos de distância radial. Presente desde o quadro 1.
- Noite invisível: o céu não espalhava a Lua, a exposição da noite deixava o
  chão ~6.5 stops abaixo do dia e o "lift" azul era um Purkinje falso. Agora:
  céu de luar na mesma atmosfera, exposição por iluminância física
  (lua cheia alta ≈ −2.75 stops na tela), Purkinje shift do Ghost calibrado
  nos slides, luar fisicamente avermelhado.
- A noite visível expôs o fogo: o chão perto da explosão (~900× o luar)
  estourava a tela de branco por >1 s. O medidor da adaptação passou a ver as
  superfícies (não só o volume) e pode fechar até a exposição do dia.
- Granulado/malha nas sombras: IGN sem TAA + penumbra enorme. VSM testado e
  descartado (perde a sombra); ficou PCSS + PCF bilinear + filtro bilateral
  em tela (`js/shadowdenoise.js`).
- Sombra sumindo à noite quando o fogo fica vermelho (achado pelo usuário):
  o critério de quem ganha cubo era potência > 2, absoluto. Agora é o
  contraste da sombra contra a luz ambiente da hora (≥ 3%). À noite o cubo
  fica ~4.5 s em vez de ~1.9 s (o custo dura mais, junto).
- Asset r64 re-assado (o hash cobre glsl.js/volumeRender.js); r48/r80/r96
  re-assam ao trocar de qualidade.

Ferramentas: `captures/_harness.js` (importar no console) — `__runSeq`,
`__sheet`, `__still`, `__env4` (texel 4: exposição, lux, E_render, escala do
Purkinje), `__ae`, `__hdr`. `env.purkinje` liga/desliga o shift.

Em aberto, visto nos testes:
- Faíscas brancas na face dos pilares sob luz do fogo: relevo procedural do
  material (aparece com o cubo desligado) — aliasing de normal, não sombra.
- Crepúsculo (17.3–18h) bem saturado laranja → magenta: é o modelo
  atmosférico (ozônio) agora visível; falta adaptação cromática (white
  balance por hora, como o Ghost faz).
- Os 4 primeiros quadros de uma explosão perto são o clarão branco (ataque
  da adaptação 0.08 s).

## Item 3 — vetores de movimento no bake (03/10) — tag v5-vetores-movimento

Aprovado pelo usuário. Detalhes em
ARCHITECTURE.md §4f1. Velocidade gravada no bake, interpolação com movimento
em todas as instâncias (as pequenas não pulam mais de quadro), macro dos dois
quadros, cache de luz interpolado, ruído de detalhe advectado. +26% no cenário
de 12 explosões. Assets das 4 resoluções re-assados no formato novo.

Visto nos testes e NÃO tratado: uma linha horizontal cortando a fumaça na
altura do horizonte. Medido em 03/10: a transmitância da fumaça é contínua
na linha; o que muda é o fundo (céu claro × chão escuro) visto através da
fumaça semitransparente — é físico.

## Itens 4b + 4c — fumaça no campo e o que passa por ela (03/10)

Detalhes em ARCHITECTURE.md §4f3 e §4f4. O vento (4a) foi descartado pelo
usuário. Não commitado: aguardando aprovação.

- Fumaça no campo (`js/battlesmoke.js`, opção "Fumaça que reage",
  ligada em média/alta/ultra): grade de velocidade de 2 m + fumaça de 1 m,
  transporte conservativo, entrega das explosões em 3.2–3.5 s. Massa medida
  conservada; a nuvem para a ~36 m, se abre e some em ~15 s.
  ~5 ms por quadro na alta enquanto há fumaça.
- Passagem (`js/movers.js`, botões avião / míssil / disparo, teclas a/m/d):
  esteira de vórtices do avião, rastro branco e luz do míssil, túnel do
  projétil, explosão empurrando a fumaça. `scene.js` ganhou malhas
  dinâmicas, pintura (id 4) e emissivo (id 5).
- Nenhum arquivo do hash do bake mudou: os assets continuam valendo.

Rev. 2 (03/10, noite), depois do teste do usuário:
- Fumaça que ficava minutos atrapalhava a jogatina → agora some em ~15 s,
  desfazendo-se pelas bordas (dissipação + erosão), e o campo limpo custa
  zero.
- "A fumaça é dura": medido e corrigido (ARCHITECTURE.md §4f3, "Ar que não
  é duro"): pluma em canal próprio, MacCormack + vorticity confinement,
  freio do escoamento 6 → 15 s, perturbadores que somam em vez de
  sobrescrever, jato dos motores limitado pelo empuxo.
- Vídeos em `captures/aviao_por_tras.mp4` e `captures/aviao_missil_lado.mp4`.
- A fumaça preta dos primeiros ~3 s é a gravação: não reage. Algo que a
  atravessa depois de 2.8 s força a troca e ela reage.

Em aberto, visto nos testes:
- O brilho vermelho que resta em 3.2–3.4 s fica um pouco mais fraco na
  grade (ela fecha os vãos entre os lóbulos por onde ele aparece).
- O disparo de tanque é discreto (é o físico — ver §4f4).
- A fumaça a 1 m é mais macia que a da explosão (0.6 m) de perto; de
  distância de RTS não aparece. Na Unreal: grade esparsa mais fina.
- Pra explosão reagir desde o começo: deformar a gravação pelo vento da
  grade em vez de trocá-la (não feito).

## Fogo que "reacendia" na troca (03/10, noite)

Relatado pelo usuário com prints (à noite): na hora em que a fumaça passa pra
grade o vermelho voltava forte e depois apagava de vez. Reproduzido: a
janela de mistura (as duas versões no mesmo lugar, uma tapando a outra)
apagava o brilho de repente e ele voltava quando a gravação sumia; e a grade
esfriava 4× mais rápido que a gravação. Agora a troca é instantânea aos 3.3 s
e o calor esfria no ritmo da gravação (ARCHITECTURE.md §4f3, "Entrega").
Testado de noite e de dia, também com o avião antecipando a troca. Resta: na
troca a textura fica um pouco mais macia e os fiapos que pendem somem (grade
de 1 m).

**Veredito do usuário (03/10): não foi suficiente.** Com a troca instantânea
a mudança de estado ficou MAIS perceptível: o brilho muda e a subida fica
lenta de repente (a pluma herdada passa a desacelerar em 1.2 s no canal da
pluma, enquanto a gravação ainda subia). É o limite de trocar uma gravação
de 0.6 m por uma grade de 1–2 m no meio da animação. Decidido migrar pra
Unreal Engine (ver abaixo) em vez de insistir aqui.

## Câmera: rolagem pelo botão direito, estilo C&C Generals (03/10)

Pedido do usuário (navegar como no Zero Hour); o resto da câmera ficou igual.
Botão direito arrastado: a câmera anda na direção do cursor em relação ao
ponto onde o botão desceu, mais rápido quanto mais longe — a fórmula do
`LookAtXlat.cpp` do código liberado pela EA (velocidade = fator·distância +
mínimo; a âncora acompanha o cursor quando ele passa de meia tela). Em
alturas de tela por segundo, então vale igual em qualquer zoom: zona morta de
2% da meia altura, ~0.6 tela/s a um quarto da meia altura, ~2 telas/s a meia
tela. Não sofre a câmera lenta (`[`). Indicador: âncora + seta até o cursor.
Clique direito parado não detona mais nem abre menu. `Camera.scrollRMB`.

## Ainda não construído

- SSAO — o MRT de normal + fração-de-ambiente já existe em `sceneT.texs[1]`
  e está sendo escrito, mas ninguém consome. Era pra ancorar os props.
- Reprojeção temporal no volume (hoje só jitter espacial).
- Painel de parâmetros pra tuning ao vivo.
- `renderAt()` não atualiza a luz do fogo: as fences de PBO não sinalizam
  dentro de uma rajada síncrona de JS. Precisa de um caminho de leitura
  bloqueante só pra captura determinística.
