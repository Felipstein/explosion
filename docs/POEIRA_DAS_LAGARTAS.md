<!-- Estudo de 13/10/2026 (sessão da arte): 4 frentes de pesquisa (jogos AAA lidos nos arquivos, física real,
os vídeos quadro a quadro, a técnica), 3 planos, síntese, 2 críticos e o plano final calibrado pela r1_raid_slabs.
O estudo inteiro (folhas, medidas, scripts, cópias das fontes) está em captures/fxlab/tread_study/ (fora do git);
o resumo dos estudos é captures/fxlab/tread_study/study_dump.md e a síntese com as críticas, synth_crit.md. -->

# Estado (13/10, noite)

**Feito:**
- **A base de perto** (o "grava pertinho"): as duas versões de 13/10 no enquadramento da r1 e no v2 de perto, com o asfalto e o sol certos (`captures/fxlab/tread_base/base_*`).
- **O asfalto:** a leitura da camada falhava (0 componentes) e foi consertada (camadas de edição do terreno). Teste da areia pro asfalto com registro: no asfalto, zero nuvem (K2) e zero cortina (C); a areia presa pinga e some em 0,37-0,63 s.
- **O código das camadas** (B3-B5 sem os torrões G): K1, K2 com máscara em cascos e esteira, C, a carga de areia por lagarta, a sombra do tanque na poeira (UV3.y) e a sombra da nuvem no chão (`M_FXDustShadow`).
- **O jato no EmberGen** (D1), medido no VDB antes de cozinhar (`FXLab/Tools/vdb_measure.py`):
  - `tread_kick_v1`: caixa de 0,36 × 0,64 × 0,6 m, pressão 35 %/s, ruído de 1,5 m, empuxo 300 % com 600 K, peso 100 %. Bolos miúdos e granulados.
  - `tread_kick_v2`: o v1 com fumaça 3000 %/s, dissipação 8 %/s, vorticidade 15 %, vorticidade grande 40 %, ruído de 3 m com 1 oitava: bolos grandes e macios até os 2 s, como na r1.
  - Aprendido: o peso da fumaça de 280 % (herdado da poeira de chão do BlastLab) faz a nuvem assentar; sem calor não há empuxo; o ruído ligado ao emissor só age perto dele; a dissipação de 25 %/s afina a nuvem em grão depois de 1 s.
- **Os perfis:** `tread_v2_ember` → `v3` (nuvem 2,0, leque 1,6, um cartão a cada 1,6 m, sombra 0,45) → `v4` (o `tread_kick_v2`, nuvem 1,8, sombra 0,3); o plano B em camadas, `tread_v2_handmade`.

- **A cor e a luz, medidas contra a r1** (poeira ÷ areia ao sol, linear; na r1: p10 0,44-0,61, p50 0,78-1,03, p90 1,14-1,23, contraste p90/p10 ≥ 2,0):
  - v4 (`tread_kick_v2`, albedo 0,6 no cozimento, 1 rebote; albedo do perfil 2,6): 0,94 / 1,40 / 1,83, clara demais;
  - v5 (albedo do perfil 1,8): 0,61 / 0,95 / 1,25, contraste 2,04, mas os bolos parecem bolinhas com borda escura;
  - v6 (recozido com albedo 0,93 e 8 rebotes, `tread_kick_v2b`; perfil 0,76): macia, mas 0,93 / 1,13 / 1,34 e contraste 1,45;
  - v7 (albedo 0,85 e 3 rebotes, `tread_kick_v2c`; perfil 0,9): 0,69 / 0,94 / 1,16, contraste 1,68: o meio-termo;
  - v8/v9 (o pé da nuvem mais escuro, `baseShade` 0,45/0,3, o mesmo canal da sombra do tanque): mais volume; v9 ficou clara (1,06 / 1,35), a v10 corrige o albedo (1,2).
  - Aprendido: a resposta média do atlas (soma das 6 luzes ÷ alfa) prevê o albedo do perfil, mas a luz do mapa pesa mais o contraluz (na v6 errou 19 %); medir sempre no quadro.

- **A nuvem pela velocidade** (v11, `scaleSlow` 0,6): a 7,9 m/s (jogo, r1) igual à v10; a 2,5 m/s (v2) menor.
- **Na câmera do jogo** (37,5°, 100 m, `-Cam=game -TreadPath=line`): o rastro lê de longe e não cobre o tanque (`captures/fxlab/tread_base/jogo_antes_v11.mp4`).
- **O custo** (`FXLab/Tools/tread_measure.ps1`: 1920×1080, câmera do jogo, a luz da receita, formação de 5 por fila; GTX 1660):
  - sem poeira 19,0 ms de placa; 20 tanques +5,6 ms; 40 tanques igual a 20 (o peso é o pixel dos tanques da frente, não o número);
  - com o recorte pela caixa do alfa de cada quadro (`FXLab/Tools/atlas_bbox.py`: o quadro tem só 28 % de poeira em média), 20 tanques +2,3 ms, o mesmo visual (diferença média 1,9/255).
  - Ainda acima do teto proposto (≤1 ms com 40 tanques no Ultra). Próximos cortes: menos cartões de nuvem, distância (LOD) e translucidez em meia resolução (é global: muda as explosões das outras sessões, perguntar).

- **Decisões do Felipe (13/10 noite):** "v11 é o caminho"; o comprimento do rastro do tanque pesado é o LONGO (denso até 1,2 tanque, some em 3: `tread_v12_ember.json`, a principal), escolhido lado a lado com o curto na câmera do jogo e na da r1 (`captures/fxlab/tread_base/comprimento_*.mp4`). O comprimento vira um ajuste por tipo de veículo na ponte com o jogo.

- **Revisão adversária do código** (13/10 noite, 3 revisores + 1 verificador por achado; 16 confirmados, 16 derrubados). Corrigido:
  - a cortina lê o chão onde nasce (o pé de trás e a frente de cada lagarta, interpolados), não só no pé de trás;
  - a sombra do tanque na poeira: um valor por cartão (no pé e no meio), suavizado em ~0,15 s, em vez de 0/1 por canto; e só os tanques perto entram na conta (o custo era N²);
  - a sombra da nuvem no chão não escurece de novo dentro da sombra do tanque;
  - o pivô (`ball_bottom`) interpolado entre os dois quadros (o cartão pulava na vertical);
  - o espaçamento sorteado guardado até sair o cartão (dependia do qps);
  - cada lagarta anda v ∓ ω·1,425 (`YawRate`): o giro no lugar levanta poeira, e de ré a fonte é a roda da frente de cada lagarta;
  - a poeira de 13/10 (sem camadas) volta a ler o asfalto pelo vértice mais próximo, igual à guardada;
  - malhas sem colisão, sem refazer a malha vazia, listas reservadas; a medida traz a CPU (Step + Draw).
- **O contrato da ponte com o jogo** (está no `FXTread.h`, `FFXTreadSource`):
  - a ponte manda TODOS os veículos perto da câmera a cada quadro, os parados com velocidade 0 (não soltam poeira, mas fazem sombra nela);
  - id estável, velocidade com sinal (ré), `YawRate` (rad/s, a derivada do yaw da Unreal: positivo virando pra +Y), e o peso da areia no pé de trás e na frente de cada lagarta (`Sand`, `SandFront`), da grade do asfalto que o WorldForge exporta;
  - **o chão no jogo** (WorldForge, 13/10): `WorldForge/Saved/WorldForge/exports/<mapa>/surface.png` + `surface.json` (1 m por pixel; R = asfalto, G = terra batida, B = rocha, A = molhado), que o jogo lê com `FRTSBakedMap::SurfaceAt`. A ponte manda `Sand = 1 − max(R, A)/255` no pé de trás e na frente de cada lagarta (asfalto e molhado não levantam areia); terra batida (G) e rocha (B) viram o tipo de chão (cor e quantidade) quando tiverem perfil;
  - ainda em aberto pra ponte: a altura do chão em rampa (hoje a poeira usa a altura do centro do tanque), a névoa de guerra, o tipo de veículo (o comprimento do rastro e a geometria das rodas), e trocar de perfil com cartões vivos.

- **O plugin do jogo** (13/10 noite, pra v2): `FXLab/Plugins/RTSTreadDust` (`ARTSTreadDust`).
  - É a conta das camadas do `FXTread.cpp` sem o laboratório, com a v12 e o cozimento `tread_kick_v2c` dentro.
  - Os presets 0–3: a nuvem em todos (no baixo, metade dos cartões com a cobertura de dois); o jato da média pra cima, a cortina da alta, a sombra no chão no ultra.
  - O que está fora da tela não entra na malha, e o passo e a montagem dividem os núcleos.
  - Empacotar: `FXLab/Tools/pack_tread_dust.py`, que usa os mesmos scripts dos materiais, então o visual é o mesmo.
  - Conferir contra o FXLab: `-TreadPlugin` (e `-TreadQuality=N`) no `tread_baseline.ps1`/`tread_measure.ps1` desenha pela cópia do jogo, com as mesmas fontes.
  - O passo a passo pra sessão do jogo está em `FXLab/Plugins/RTSTreadDust/INTEGRACAO.md`.
  - O `YawRate` passou pro sinal da Unreal (positivo virando pra +Y). A volta do laboratório não gira, então a imagem não muda.

- **A sombra do cenário na poeira** (14/10, a pergunta do Felipe: "testou se as sombras do mapa projetam em cima?"). Não projetavam: a poeira é translúcida e só tinha a sombra dos tanques. Na sombra de uma rocha ela brilhava como no sol (`captures/fxlab/tread_base/sombra_r1_v12_ember.mp4`, a reta `-TreadPath=shade`).
  - O conserto, o jeito dos jogos pra partícula receber sombra sem custo por quadro: um mapa de profundidade visto do sol, como no "Volumetric Particle Shadows" de Simon Green (NVIDIA, GDC 2009) e no conselho comum de renderizar o próprio mapa de sombra da luz.
  - O mapa é tirado uma vez (4096², 36 cm por pixel no mapa de teste) e lido na CPU. Cada cartão consulta pé, meio e topo, numa média de 3 × 3 pontos interpolados, suavizada em 0,15 s.
  - A sombra do cenário corta só o sol (vai no UV2.x do material). A do tanque corta sol e céu.
  - Prova: `sombrafix2_r1_v12_ember.mp4`.
  - O volume de translucidez da Unreal não serve: na câmera do RTS, a 100 m, ele cobre só até 50 m e tem só 64 células.
- **A compressão das folhas** (em teste): `FXLab/Tools/bake_norm8.py` normaliza a luz (ganho 7,8 e 5,5, satura 0,0016 % dos pixels) pra 8 bits em BC7, porque direto em 8 bits a luz típica teria ~20 degraus. Fica em `tread_kick_v2c_norm8`, o A/B é com `-TreadDustSheets=norm8`, e o material desfaz com `LightScale`.

**Falta:** os torrões (G, segunda folha), a esteira cozida (D8) se precisar, o v2 de perto com a versão nova, as rodas, a sessão do jogo ligar o plugin, e os cortes de custo (compressão das folhas, ~150 MB de placa hoje; o caminho na placa).

---

# Plano final: a poeira das lagartas e das rodas (versão EmberGen e plano B), calibrada pela r1_raid_slabs

**Fontes, abreviadas em cada número**
- **[r1]**: medi agora na `r1_raid_slabs_gemini.webp` (1024×572, imagem nativa). O recorte do Felipe é a mesma imagem ampliada 2,34× (x 0–513, y 257–572, correlação 0,98), então meço na nativa.
- **[v2] / [v3]**: os vídeos de perto, medidos no estudo anterior.
- **[geom]**: `look_tank.py`.
- **[física]**: o estudo real-physics.
- **[ZH] [WARNO] [WT] [WoT] [Reforger]**: os arquivos dos jogos.
- **[repo]**: conferido agora no código ou no log.
- **PP**: ponto de partida, com o jeito de medir ao lado.
- **L**: o comprimento do casco do veículo que solta a poeira.
- **H**: a altura da silhueta do veículo na tela.

**Arquivos das medidas:** `captures/fxlab/tread_study\plano_final\` (folha `r1_medidas.jpg`; scripts `match3.py`, `maps.py`, `samp.py`, `grid.py`, `sheet.py`; recortes `g_*.jpg`).

**Resumo pro Felipe**
- A poeira passa a nascer pequena, escura e no chão, atrás da roda motriz.
- Logo atrás do tanque ela vira uma nuvem grande e cor de areia, que chega entre o convés e o teto da torre e junta as duas lagartas.
- Ela é densa até ~0,7 casco e vira véu depois disso.
- O lado do sol fica claro e o pé escuro, e a nuvem joga sombra macia no chão.
- Picape e caminhão ganham o mesmo sistema, saindo das rodas de trás.
- No asfalto a poeira para; a areia presa pinga menos de 1 s.
- Duas coisas que ele precisa saber:
  - no vídeo que ele viu, a regra do asfalto estava desligada;
  - no nosso laboratório o sol estava no lugar errado.
- Primeiro sai a gravação de perto do que existe hoje, no enquadramento da r1 e do v2.

---

## A) O alvo medido

A r1 manda no tamanho, no volume, no comprimento, na cor e na sombra. O v2/v3 manda em onde e como a poeira nasce.

### A1. Câmera e sol da r1 (pra gravar igual)
- **Inclinação da vista: 25 a 33°.**
  - Os ventiladores do teto da fábrica dão elipses de 0,51 a 0,54, ou seja, 31 a 33°.
  - A silhueta dos tanques dá 23 a 25°.
  - É imagem de IA e a perspectiva não fecha. **Uso 28°**, que é o "zoom perto" do jogo (`DefaultGame.ini` NearPitchDeg 28).
- **Escala.** O casco do tanque da GLA tem 165 px de 1024, ou seja, 16% da largura.
  - Pro nosso M1 (7,93 m) ocupar 16% de 1600 px, ele precisa de 258 px.
  - Com FOV 50 (foco 800/tan 25° = 1715 px), isso dá **~53 m**. É mais perto que o zoom mais perto do jogo (67 m).
- **Sol.** Medi pela sombra do cano (ponta em (400,323), sombra em (466,397)):
  - **~25° ± 5° de altura, ~41° à esquerda do eixo da câmera**, à frente dela. É contraluz de três-quartos à esquerda.
  - A receita aprovada tem 42° do outro lado. Por isso gira-se a câmera junto com o caminho; nunca se espelha a imagem.
- **Os veículos atravessam a tela** com rumo ~10° pra cima.

### A2. O que a r1 mostra (tanque GLA de baixo, picape de baixo e M1 do meio)

| Medida | r1 | v2 / v3 | Decisão |
|---|---|---|---|
| Altura da nuvem na tela ÷ H | Tanque: 1,22 (100/82 px) e 1,36 (o de cima). Picape: 0,79 (65/82) | — | Tanque 1,2–1,35; rodas ~0,8 |
| Topo logo atrás da traseira | Entre o convés e o teto da torre; a torre fica livre. No mundo, ~1,5–2,3 m (0,6–1,0× a altura do veículo) | Convés a 0,5 L; nunca cobre a torre | **r1**: o volume chega já em 0,1–0,3 L a 7,9 m/s |
| Largura (conta com 28°) | 1,3–1,7× a largura do veículo a 0,3–0,5 L | 5 m a 0,5 L = 1,37× | Concordam |
| Comprimento | Densa até **0,64 L** (tanque) e **0,73 L** (picape). Véu até ≥1,0–1,25 L, cortado pela borda da imagem | Densa até 1,25 L, some em 1,5–2,0 L | **r1** pro denso. Véu até 1,6 L (PP). O "2–3 L" não dá pra medir na r1 |
| Cor (Y linear ÷ mediana da areia ao sol, 0,51) | p10 0,44–0,61; **p50 0,78–1,03**; **p90 1,14–1,23** | Topo 1,15–1,20× | p90 bate. Mediana mais escura: **r1** |
| Saturação e matiz | Topo aceso 0,43; miolo e base 0,50–0,53; matiz 28–31° (areia: 0,46–0,47 e 31–32°) | −0,08 no topo | O pé denso é mais quente; o topo fica menos saturado |
| Contraste interno (p90/p10) | 1,9–2,8 | 2,2–2,4 | ≥2,0 |
| Poeira dentro da sombra do veículo | Y 0,10–0,25 (sombra dura no chão: 0,11–0,12) | Nasce escura, Y 0,15–0,29 | Precisa de visibilidade do sol por cartão |
| Sombra da nuvem no chão | Macia, pro lado contrário ao sol (baixo-direita na tela). O chão cai a **0,57×** junto da picape (0,25 contra 0,44) e ~0,8× na borda. A sombra do veículo, ao lado, é dura | Não medida | Camada S (B3). É provável, não certa: se mistura com a sombra dos veículos |
| Forma | Bolos de 15–25 px = 0,1–0,15 L, redondos | r25 0,12–0,14 L, anisotropia ~1 | Concordam |
| Opacidade (só diagnóstico) | Sob o miolo, o detalhe fino da areia cai 60–80% | 0,77 a 0,4 m | — |

**Rodas × lagartas**
- **Picape.** A poeira nasce no chão, na roda traseira, embaixo da carroceria. Os bolos ficam mais separados; a dianteira quase não solta.
  - Boa parte da nuvem fica dentro da sombra da própria picape, por isso parece mais escura.
  - Não é maior que a do tanque em proporção: 0,8× contra 1,2× de H. O "mais forte nas picapes" **não se confirma** na medida.
- **Tanque.** A corrida de baixo da lagarta some na poeira e a nuvem é contínua.

**Os M1 da r1** estão saindo da fábrica e soltam só um jato curto:
- 0,33 L, até o convés;
- topo aceso (Y 0,375, saturação 0,55).
- A nuvem grande é dos veículos da GLA em ataque.
- Decisão (avisar o Felipe numa linha): o nosso M1 a 7,9 m/s leva a nuvem do tanque da GLA, que é o pedido dele ("cada veículo andando"). O jato curto vira a arrancada.

**Outros detalhes da r1**
- Vários veículos somam os véus num véu só (toda a borda esquerda).
- No chão, além das marcas, há torrões escuros de 3–7 px (0,12–0,3 m).

### A3. O que continua vindo do v2/v3
- A poeira nasce no chão, no pé da lagarta, com a motriz mais forte e uma cortina ao longo dela.
- Vai de 0 a 0,6–0,8 m de altura em 0,2 s, com torrões caindo.
- Nasce escura, na sombra do tanque.
- Movimento: +0,09 v a 0,3 L; parada no mundo a partir de 0,45 L.

### A4. Erros de hoje (medidos)
- 100% dos cartões nascem embaixo do casco (`FXTread.cpp:592`, `Rnd(0, Along)`).
- Nascem largos: 2,35 m (EmberGen) e 1,94 m (plano B).
- O pico de opacidade só chega a 2,9–4,4 m de onde nasceram.
- Duas fitas que não se juntam.
- Riscos: anisotropia 4,0 (EmberGen) e 2,0 (plano B).
- Cor: o EmberGen fica 1,3× a areia e chapado; o plano B fica ≈ a areia, com contraste 1,6.
- Sem sombra própria e sem sombra no chão.
- O que está gravado não é um jato.

Dois erros novos, conferidos agora no repositório:
- **A regra do asfalto estava desligada no vídeo dele.**
  - O log das gravações das 14:26 e 14:35 diz `asfalto lido em 0 componentes` e `a volta em (0.0, 0.0)`.
  - A DLL é das 14:52 e a grade das 14:53.
  - Os "~2 s no cruzamento" ainda não têm causa provada.
- **O sol do FXLab fica no lugar errado.** O diretor do Look nasce em `FXLabGame.cpp:110`, antes do `SetCam` da linha 138.

---

## B) O desenho do efeito

### B1. Geometria [geom]
- Lagartas em y ±1,425 com 0,64 m de largura.
- Rodas no chão de x +2,55 a −2,40.
- Roda motriz em (−3,48; z 0,6; r 0,36): o ponto mais de trás fica em −3,84.
- Roda guia em +3,53.
- Saia de z 0,62 a 1,45.
- Convés a 1,55 m, teto da torre a 2,48 m.
- Nada nasce em cima da lagarta (WT `us_m1_abrams` `top:false`).

### B2. Princípios
- **O cozimento faz a forma; o cartão faz o transporte.** Perto do tanque, a **esteira do casco** muda o cartão (o campo de força do WoT; Gillies 2005: a mistura chega a 1,7 H).
- **O jogo só tem 0 ou 7,9 m/s** (aceleração de 263 m/s², `Units.json`). Por isso:
  - o volume e o comprimento se calibram a 7,9 m/s contra a r1;
  - 2,5 m/s serve só pro A/B do nascimento contra o v2.
- **O tempo do cozimento é sempre real** em K1, K2, C e G.
  - Sai a taxa r = v/2,5. Ela fazia borbulhar e dava salto na troca do K1 pro K2.
  - O comprimento do rastro passa a ser limitado **no espaço**: uma máscara de distância até a lagarta enquanto o tanque anda. É o teto do WARNO aplicado no espaço.
  - Com o tanque parado, a máscara congela e a nuvem assenta em 3–5 s, que é a cauda medida no v2.
- **Quantidade por metro andado e por lagarta**, com v_i = v ± ω·1,425.
  - Pra frente, a fonte é a motriz; de ré, a roda guia (x +3,86). A quantidade usa |v_i|.
  - Rampa de velocidade de K1 e G: zero em 1,88 m/s (√(g·0,36), quando a motriz chega a 1 g) e cheia em 2,5 m/s. A cortina usa a rampa do ZH.
  - Arrancada com rampa de 0,3–0,5 s (PP).
  - Giro no lugar: o casco tem v = 0, então K2 e a esteira ficam desligados. K1 e cortina ficam com teto de 0,3–0,5× (PP; medir que não cobre o casco).
- **Vento:** o do mapa × um perfil de altura de 0,1–0,3 perto do chão (PP; o WARNO usa ×0,1). Vento 0 nos A/B contra os alvos.

### B3. As camadas (iguais nas duas versões)

| | Nasce em | Espaçamento | Vida | Conteúdo e opacidade | Movimento |
|---|---|---|---|---|---|
| **K1, o jato** | x U(−3,84; −3,48), y ±1,425 ± 0,16, pivô no chão | 0,25 m (ZH ~30/s por lagarta) | 0,35 s | Atlas próprio de ~1,6 m de lado, de 0 a 0,4 s (5× mais texel). Opacidade AttackSustainDecay (0,05; 0,5) [WARNO] | Herança 0,3, arrasto 1,6/s [v2 a 24 qps] |
| **K2, a nuvem** | O mesmo ponto do K1 | 1,0 m (r25 ≈ 1 m) | Pela máscara: 0,8 até 0,7 L, depois cai a 0 até 1,6 L (PP) [r1] | Mesmo cozimento do K1, idade contínua com ele. Enquanto o K1 vive, alfa_K2 = 1 − (1 − α_K1)^(esp.K2/esp.K1), com soma constante | Igual ao K1, mais a esteira abaixo |
| **Esteira (no K2)** | Cartões a menos de 1 L do tanque, com o tanque andando | — | Decai em ~1 L | Subida, puxada pra linha do meio e crescimento, todos ∝ v. Calibrado até dar 1,2–1,35 H a 0,25 L e uma pluma só a 0,5 L [r1, v2] | A 2,5 m/s vale 1/3: a subida mais lenta do v2 sai sozinha |
| **C, a cortina** | x U(−2,40; +2,55), y ±1,745, no chão | 1,0 m | 0,36 s (queda de altura da saia) | O jato × 0,5. Peso ao longo da lagarta = o perfil medido no E0 | Igual ao K1 |
| **G, os torrões** | Arco de trás da motriz, z 0,24–0,6 | 0,25 m × U(1; 6) [WARNO] | Cozida com a trajetória (como o gêiser `debris4`); o cartão fica parado no ponto de saída | Torrões de 0,12–0,3 m [r1]. O tamanho final sai do v3. Translúcidos com Responsive AA; somem abaixo de 3 px | Dentro do cozimento |
| **S, a sombra no chão** (nova) | Um cartão deitado no chão (como o `m_isGroundAligned` do ZH) por cartão de K2 | — | A do K2 | Alfa = a pegada do quadro, borrada (mip +3) × k. k calibrado até o chão ficar 0,55–0,8× a areia ao sol [r1] | Deslocado a h_c·cot(sol) pro lado contrário ao sol (h_c = altura do centro do alfa, gravada no atlas). Pulado onde o chão já está na sombra do tanque (o mesmo raio contra as caixas do casco) |

**A sombra no chão**
- Material novo, `M_FXDustShadow`, em modo Modulate e sem luz. Custa pouco pixel.
- No jogo, depois da aprovação: uma textura de densidade vista do sol, com 0,25 m por texel. Ela entra:
  - no termo do sol do chão (a MF inline é do dono do Look: avisar e não mexer);
  - ou na função de luz do sol, e aí sombreia também as unidades dentro da nuvem.

**Rodas (caminhão de suprimento hoje, a 10,5 m/s; a Technical da GLA quando existir)**
- Uma fonte por roda, lida das malhas `wheel_*` no spawn.
- K1 em cada roda traseira, no quadrante de trás e de baixo do pneu. Na dianteira, ×0,25–0,5 (PP; na r1 ela quase não solta).
- Sem cortina. G sai das traseiras.
- Limiar de velocidade √(g·r_pneu): 2,1 m/s com r = 0,45 m [física].
- Calibração pela picape da r1: 0,8 H e densa até 0,73 L. A esteira fica no K2 (o mesmo campo, com o casco do caminhão).

### B4. O desenho do cartão
1. **Pivô** no ponto mais baixo do alfa de cada quadro (`ball_bottom` no `atlas.json`), nos dois cozimentos.
2. **Tamanho 1:1** = `side_m` × U(0,85; 1,15).
   - Giro de ±10°, espelho sorteado, defasagem de ±2 quadros e distorção de UV por ruído no mundo.
   - O crescimento no cartão só existe na esteira, e está declarado.
3. **O quadro vem da idade** pelo `times_s`.
4. **Deslocamento pra câmera ligado desde o começo:**
   - D ≥ s_abaixo/tanθ + SoftDist, ao longo do raio da câmera, com escala pra manter o tamanho na tela;
   - o SoftDist vem do F2, não da faixa 30–40 escolhida;
   - fade de ~60 cm contra o tanque pelo CustomDepth, se o tanque escrever CustomDepth.
5. **Plano B:** `orient` = 1 com espelho −1 (70–93% da poeira fica à esquerda do pivô).
6. **Recorte** por um contorno de 8 vértices. Ele cobre a união dos dois quadros misturados e tem borda vazia de 16 px no atlas.
7. **Uma malha por tanque**, com K1, K2 e C ordenados juntos, e prioridade de ordenação 0 (hoje é 1, `FXTread.cpp:125`). A malha das sombras S fica à parte.
8. **As 3 sementes num atlas só:** 12×12 quadros de 384 = 4608², ~21 MB em BC7.

### B5. Luz e cor
- **Material novo `M_FXSixWayDust`**, copiado do atual. O `M_FXSixWay` e o fogo ficam intocados. Ele traz:
  - o rebote da areia: sol × max(sol.z; 0) × ρ_areia × cor da areia, no canal de baixo e na metade dos lados;
  - um canal de céu visível;
  - a saturação seguindo a densidade (tinta^(1+k·α)), calibrada pela r1 (0,43 no topo, 0,50–0,53 no miolo);
  - vetores de movimento: fluxo por blocos em numpy entre quadros do atlas, que já existe (`blockflow2.py`), numa textura RG8, distorcendo ±½ quadro. Liga e desliga por parâmetro;
  - sem a textura do fogo.
- **Sol e céu visíveis, um valor por cartão** (no centro do alfa e no pé), suavizados no tempo.
  - No laboratório: raio contra o mundo (ECC_Visibility).
  - No jogo: a textura vista do sol, depois.
- **Cozimento** com albedo 0,93 [Dubovik: 0,92–0,97] e 8 a 16 rebotes de volume, na CPU. Cronometrar 1 quadro antes.
- **Albedo do material** = antigo × (resposta 6-way do cozimento velho ÷ do novo), medida no atlas. Os atlas de hoje são 0,6, não 0,93. Depois, iterar com o `samp.py` contra os percentis da r1.
- **Anisotropia 0,3** (a de hoje): a r1 e o v3 mostram só 1,1–1,2× em contraluz.
- **Tinta feita à mão por tipo de chão** (como WoT, WT, Arma e Reforger). A da areia sai da r1 (matiz 28–31°).

### B6. As duas versões (independentes)
- **EmberGen:**
  - K1 e K2 vêm do `tread_kick_v1` (D1);
  - C é o mesmo cozimento × 0,5;
  - a esteira fica no cartão.
- **Plano B (à mão):**
  - K2 = a `dust_impact4` a partir do quadro 16, escala 0,41, tocada **1,56× mais rápido** (Froude: tempo ∝ √escala) e um mip acima;
  - dos 2,13 s que sobram, ficam ~1,37 s de conteúdo, perto dos 1,6 s do K2 a 7,9 m/s. O último quadro segura o resto, e isso é dito ao Felipe;
  - K1 = a janela dos quadros 8→20 tirada pelo `measure_atlas.py`, **com crescimento no cartão só nele** (declarado);
  - BC7 só num asset novo, `/Game/FX/dust_impact4_bc7`, porque a original também é a poeira do tiro.
- **G e S são iguais nas duas**, pra isolar a nuvem no A/B.

---

## C) Asfalto e a areia presa (menos de 1 s)

1. **Antes de tudo:** regravar a linha de base com a grade funcionando (`-TreadAt` fixo) e medir se os "~2 s" ainda existem.
2. **O chão é lido por lagarta, no pé de trás** (x −2,40, y ±1,425), com o `AsphaltAt` bilinear e peso contínuo w = 1 − asfalto [WoT matkindL/R; Reforger por roda]. A cortina lê o chão onde nasce.
3. **A carga:** Λ = max(w, Λ·e^(−dt/τ)), com τ = 0,27 s. Fica abaixo de 5% em 0,8 s (PP; ele confirma a 0,25×).
4. **O que cada camada segue:**
   - K2, esteira e C seguem o w de agora e param na hora [Reforger StopEmission];
   - taxa do K1 = rampa(v)·[w + k·(Λ − w)⁺], tamanho = lerp(0,4–0,6; 1; w), corte em Λ − w < 0,025;
   - G = areia caindo da motriz (0,35 s até o chão), ∝ Λ·v;
   - nada que já está no ar é apagado.
5. **O relógio conta do pé de trás.** A 7,9 m/s, a fonte cheia ainda dura 0,63 s depois de a frente entrar no asfalto. Ele decide, vendo as duas opções a 0,25×.
6. **Do asfalto pra areia:** a poeira liga quando o pé de trás entra; a cortina liga antes, pela frente.
7. **A marca de areia no asfalto** (~15 m, uma volta da lagarta) vira pedido ao dono do `RTSGroundMemory`: um 2º canal × Λ. Os bolinhos no asfalto ficam com alfa ≤0,3, testados na câmera do jogo.
8. **Outros chãos:**
   - o log passa a imprimir os nomes das camadas, e as retas de teste ficam só na areia;
   - perguntar a ele se pátio coberto de areia conta como asfalto (o caso `roadSand` do WT);
   - neve e lama ficam fora: o caminho antigo continua funcionando e entra 1 quadro de conferência na neve.

---

## D) O que regravar e mudar

**Regra geral:** os scripts compartilhados (`bake_sixway`, `bake_debris`, `setup_fxlab`) só mudam por **argumentos novos**, e o padrão dá saída idêntica. O tiro e a carcaça são regravados antes e depois.

**D1. EmberGen `tread_kick_v1`** (fila `.ember_queue`; fechar pelo PID)
- **Ponto de partida:** o `tread_puff_v1.ember`, lido com `ver_no.py`.
- **Sai:** o toro, a pressão de 150%/s, a temperatura e o empuxo.
- **Emissor** de 0,64 × 0,36 m, de z 0 a 0,6.
  - Conferir se ele é `Emitter: Particles` e quem controla a rajada (o *Duration of burst* é do `Emitter: Volume`).
  - Se faltar, copiar o nó do Artillery Explosion 2 (Ctrl+C/V, §8 do `EMBERGEN_MAPA.md`).
- **Velocidade:** cone mais aberto e mais baixo.
- **Só a população de pó injeta fumaça.**
- **Force Noise** com curl, Scale ~1 m e amplitude que decai, desde o quadro 0 (quebra o anel e evita o cogumelo). Mais uma expansão fraca que também decai.
- **Domínio** de 6 × 5 × 2,5 m, voxel de 3 cm.
- **Calibração**, exportando poucos quadros antes:
  - 0,2 s: 1,1 m por lagarta, topo a 0,6–0,8 m [v3];
  - a 0,5 L, com a esteira: as duas lagartas juntas dão 1,3–1,7× a largura do veículo [r1].
- **Exportação:** 5 s simulados, ~50 VDB, passos escritos em quadros de 60 Hz. Uma semente de cada vez: conferir o disco (9,6 GB livres) e apagar os VDB depois de cozinhar.

**D2.** `bake_debris.py --variant tread_grit`.

**D3. `bake_sixway.py`**
- Tempos não uniformes e o atlas do K1 separado.
- No `atlas.json`: o pivô `ball_bottom`, o contorno de 8 vértices, a pegada e a altura do centro do alfa (pra S).
- Borda de 16 px, albedo 0,93, rebotes e vetores de movimento.
- **Elevação de 33°**: o meio de 28 (r1) e 37,5 (o jogo). Gravar o mesmo VDB a 28 e a 44 pra conferir; se o erro passar de 10%, um 2º cozimento.

**D4. `setup_fxlab.py`**
- Assets novos: BC7 quando o máximo é ≤1; `M_FXSixWayDust`; `M_FXDustShadow`.

**D5. `FXTread`**
- **Fonte:** `Sand[2]`, `YawRate`, `Kind` (lagarta ou rodas) e a lista de rodas.
- **Perfil = lista de camadas**, mais os campos da esteira e de S.
- **Estado** por lagarta e por camada, mais a Λ.
- **Sorteio** por camada.
- **Desenho:** malha por tanque e prioridade 0.
- **Laboratório:**
  - `-TreadPath=line`, `-TreadAt`, `-TreadHeading`, `-TreadSpeed`;
  - `-TreadDrive=sim` (a lei do SimCore: cantos de 90°, giro parado, parada);
  - `-TreadTanks=N` e `-TreadLog` (CSV com w e Λ);
  - `-StepHz`, junto com um `-Fps` no `fxlab.ps1`, tudo a 24.
- **Câmera e sol:**
  - `-Cam=r1/v2/v2close`, com o FOV aplicado por último e sem centralizar o tanque;
  - `-SunAz` e `-SunEl` absolutos, aplicados depois do 1º Tick do diretor. Conferir no log que o azimute do sol = giro da vista ± 41/42°;
  - botões no painel: "r1", "v2 de perto" e "reta areia→asfalto".

**D6. `fxtread.js`**
- Sincronizar depois da aprovação; o C++ é a referência.
- Corrigir o "CoH3 por distância" no `FXLAB.md`: a fonte real é o WARNO TrailSpawner e o Niagara SpawnPerUnit.

**D7. Ferramentas de CPU**
- `tread_metrics/`: os scripts do estudo mais os da r1 (`plano_final\`).
- `tread_ab.py`:
  - folha da r1 com o nosso quadro na **mesma escala de casco**;
  - vídeo do v2;
  - uma **passada limpa** (o mesmo quadro sem poeira), pra ter a opacidade verdadeira;
  - medidas sobre o nosso quadro reduzido igual ao v2.

**D8. Esteira cozida (depois, por A/B)**
- Só se a esteira no cartão falhar nos portões 2–3 ou parecer cartão crescendo.
- Cozimento no referencial do tanque: vento de 7,9 m/s, colisor no casco (vão de 0,43 m), voxel de 5 cm.
- Gravado em **fatias curtas de 0,8 m** ao longo do eixo, cada uma num cartão preso ao tanque. Uma fatia curta não depende do ângulo da câmera; a esteira inteira num cartão só dependeria.

---

## E) Como gravar

**Regras dos testes**
- Mesma semente por camada, mesmo caminho, mesmo sol; só muda o perfil. Uma mudança por A/B.
- Mostrar a 1× e a 0,25×, lado a lado e alternando.
- `-ExecCmds="DisableAllScreenMessages"`.
- PNG vira mp4/jpg e é apagado.

**E0, o gabarito (só CPU)**
- O T1 do v2 a 24 qps, sem o zoom da câmera.
- A distância entre os centros dos dois bolos ao longo do rastro.
- A sombra da poeira no chão do v2.
- A elevação do sol pela sombra do T1.

**E1, linha de base de hoje.**
- O código atual com a grade funcionando, no enquadramento da r1 e do v2 de perto, mais a reta pro asfalto.
- **É o "grava pertinho" que ele pediu.**

**E2, r1 (o principal)**
- `World_TestSalt2 -GameLight` com a receita aprovada, `-SunAz/-SunEl` em 41° à esquerda e 25° (escrito no nome do arquivo).
- Câmera a 28°, FOV 50, ~53 m, casco do M1 com 258 px em 1600.
- 1 M1 (mais o caminhão quando as rodas entrarem), em reta só de areia a 7,9 m/s, com 6 s de aquecimento.
- Saída: uma foto e 4 s de vídeo, mais a folha ao lado da r1.

**E3, v2 de perto (o nascimento)**
- A 2,5 e a 7,9 m/s, 24 qps.
- Quadro inteiro e close, com o recorte do v2 no mesmo tamanho.

**E4, asfalto a 0,25×**, com a marca do pé de trás e o log na tela.

**E5, a câmera do jogo no FXLab**
- 37,5° a 100 m e 28° a 67 m, com `-TreadDrive=sim` e uma coluna de 3 tanques.
- Três variantes do comprimento, ao lado dos game shots na mesma escala:
  - (a) o da r1;
  - (b) a vida do v2;
  - (c) o véu ralo do `usa_vs_china`.
- Ele escolhe.

**E6, no jogo, antes de pedir o "aprovado"**
- `game.ps1 view -Scenario` com semente fixa, 3 tanques e a ordem típica.
- O tipo de chão vem do mapa; as marcas da esteira estão lá.

---

## F) Aceite e custo

**F0, primeiro o olho.** De perto, quadro a quadro, ao lado da r1 e do v2:
- parece uma poeira só?
- nada quebrado ou voando?

**Portões que decidem (medidos na passada limpa):**
1. **Nasce na lagarta [v2]:** a 1ª poeira com alfa >0,3 aparece a ≤0,5 m da motriz ou embaixo da saia; o pé fica a ≤0,05 L das sapatas.
2. **Volume [r1]:** altura na tela ÷ H de 1,2–1,35 a 0,25 L (tanque) e ~0,8 (rodas). Nada acima do teto da torre até 0,3 L.
3. **Junção [r1, v2]:** uma pluma só a 0,5 L (o vale de luma ≤ o do T1), com 1,3–1,7× a largura do veículo.
4. **Cor e luz [r1]:**
   - Y ÷ areia ao sol: p10 0,45–0,6, p50 0,78–0,9, p90 1,15–1,23;
   - saturação: topo 0,43 ± 0,03, miolo 0,50 ± 0,03;
   - p90/p10 ≥2,0;
   - poeira na sombra do veículo: 0,2–0,55× a areia.
5. **Bolos, não riscos:** anisotropia ≤1,3; bolos de 0,1–0,15 L.
6. **Sombra no chão [r1]:** macia, chão a 0,55–0,8× a areia ao sol, do lado contrário ao sol; não escurece mais de 10% dentro da sombra do tanque.

**Diagnósticos (não decidem):**
- densa até 0,65–0,75 L;
- opacidade de 0,77 a 0,4 m;
- topo a 0,6–0,8 m aos 0,2 s;
- movimento: +0,09 v a 0,3 L, depois parada;
- asfalto: nenhum K2 ou C novo depois de o pé de trás cruzar; K1 e G abaixo de 5% em 0,8 s e zero em 1,0 s; no máximo 1 troca liga/desliga em 0,5 s;
- sem contas (pico de autocorrelação ≤0,1 em 0,25 e 1,0 m);
- sem pulso na troca das camadas;
- cobertura sobre o tanque de trás ≤ a do v2.

**Custo**
- ~35 cartões por lagarta a 7,9 m/s, mais S.
- 40 tanques dão ~3–4 mil cartões, 0,3–2 ms de GPU (estimativa).
- Memória: ~21 MB por atlas em BC7, contra 48 + 157 MB hoje em TC_HDR.
- **Medir** com a poeira ligada e desligada, com o mesmo N (20 e 40), mediana e p95, ProfileGPU e `stat game`. Sem o EmberGen de outra sessão aberto.
- **Teto pra ele decidir:** ≤1 ms com 40 tanques no Ultra.

**Presets**
- K2 e S iguais em todos, pela justiça entre presets (`BASE_JOGAVEL.md:241`).
- Só K1, C e G variam: Baixa sem K1, C e G; Média com K1; Alta com C; Ultra com G.
- Isso conflita com o `BASE_JOGAVEL.md:308` ("a primeira a sair no baixo"): decisão dele.

---

## G) Riscos e o que fica pra depois

**Riscos**
1. **Os alvos são IA.**
   - A r1 é uma foto parada: não dá tempo nem velocidade.
   - A sombra da nuvem é provável, não certa.
   - Os M1 da r1 têm só o jato curto.
2. **EmberGen:**
   - a rajada pode não ser editável por script;
   - a memória da placa;
   - o disco: 9,6 GB livres.
3. **A esteira no cartão** pode parecer cartão crescendo. A reserva é o D8.
4. **O cozimento a 33°** pode deformar quando visto a 28–44°.
5. **A cor da nuvem e dos torrões** pode não casar.
6. **O FXLab não tem marcas no chão** e a esteira do tanque não anda. Avisar o Felipe; o E6 cobre isso.
7. **O cozimento na CPU** pode ser lento.
8. **O FXLab está sendo usado por outra sessão** (logs de 16:20). Compilar só com ele fechado, avisando quem estiver usando.

**Depois da aprovação**
- A ponte com o Niagara na GPU (SpawnPerUnit, Landscape DI).
- A sombra vista do sol no jogo, com o dono do Look.
- Curva com a lagarta de fora até 2×; patinagem.
- Névoa de guerra e LOD.
- O escape da turbina.
- A sujeira no casco ligada à Λ (com o dono do tanque).
- A explosão empurrando a poeira que já está no ar (a r1 mostra; com a sessão das explosões).
- O chão já rodado levantando +569% (Meeks 2015).
- Anotar o aprovado em FXLAB, RECEITAS_DO_LOOK e BASE_JOGAVEL, trocando o "some em 3–6 s".

**Críticas que rejeitei (uma linha cada):**
- **vfx 1, a esteira inteira num flipbook preso ao tanque:** um cartão só de uma esteira comprida fica errado quando o rumo muda em relação à câmera. Entra a esteira no cartão (v1) e, depois, a esteira cozida em fatias curtas (D8).
- **vfx 4, a tinta 0,63× do WARNO como reserva:** a base medida na r1 (0,44–0,61×) é melhor.
- **vfx 7, fluxo óptico pelo OpenCV DIS:** o OpenCV não está instalado e instalar é baixar. O fluxo por blocos em numpy já existe.
- **vfx 9, 6 sementes:** pelo disco e pelo tempo. Ficam 3 num atlas mais as variações no cartão; a 4ª só se a autocorrelação mostrar repetição.
- **vfx 14, a sujeira do casco neste trabalho:** é do dono do tanque. Vai como proposta ligada à Λ.
- **vfx 16, translucidez em meia resolução agora:** o `r.SeparateTranslucencyScreenPercentage` é global e muda as explosões das outras sessões. Só se o custo medido passar do teto, e perguntando.
- **A tinta, o Albedo 2,34 e o r = v/2,5 do plano anterior:** substituídos pelos números da r1 e pelo tempo real com a máscara no espaço.

Todas as outras críticas, vfx e regras, foram aceitas e estão acima.

---

## ORDEM DE EXECUÇÃO (um passo por vez; cada A/B vai pro Felipe antes do próximo)

**CPU, sem placa:**
1. As medidas da r1 (feitas, em `plano_final\`). O E0 do v2. O `measure_atlas` da `dust_impact4`. O `tread_metrics` e o `tread_ab`, validados numa passada com alfa conhecido.
2. Só código de câmera, caminho e sol, sem tocar na poeira (D5: `-Cam`, `-TreadPath/At`, `-SunAz/El`, `-StepHz/-Fps`, botões). Compilar com o FXLab fechado.

**Fila da placa:**
3. Fotos de enquadramento: r1 a 28° e ~53 m com 3 rumos; v2 com 2 visadas × 3 rumos. A pose se escolhe na CPU.
4. **A linha de base de hoje** (EmberGen e plano B) no enquadramento da r1 e do v2 de perto, mais a reta pro asfalto. **Mostrar**, dizendo que a regra do asfalto e o sol estavam errados antes.
5. Plano curto pra discutir com ele: o relógio, o comprimento (a/b/c), o M1 com nuvem grande ou jato, o papel do plano B, o conflito dos presets e o teto de custo.

**Código mais fila da placa, uma mudança por A/B, cada uma mostrada:**

6. Uma coisa de cada vez, nesta ordem:
   - (a) onde nasce, pivô, deslocamento pra câmera, 1:1 e orientação (plano B);
   - (b) sol e céu por cartão mais o `M_FXSixWayDust`;
   - (c) a esteira no cartão, contra a r1;
   - (d) a sombra S;
   - (e) a cortina e a Λ no asfalto;
   - (f) as rodas no caminhão.

**Fila do EmberGen:**

7. Rajada sem exportar. Calibração com poucos quadros. Semente 1 (cozinhar e apagar os VDB). Sementes 2 e 3, uma de cada vez. O atlas do K1. O `tread_grit`, cronometrado.

**Fila da placa:**

8. A/B: r1 | EmberGen novo | plano B novo | linha de base. **Mostrar.**
9. E4, o asfalto a 0,25×. Ele decide o relógio e o τ.
10. E5, a câmera do jogo com a ordem típica. Ele decide o comprimento.
11. E6, no jogo, com a ponte mínima e o chão do mapa.
12. Custo com 20 e 40 tanques, poeira ligada e desligada.
13. Se os portões 2–3 falharem: a esteira cozida em fatias (D8).
14. Com o "aprovado": os docs e o commit só dos meus arquivos (`git commit -- <caminhos>`).

Nada no repositório foi mudado e nada usou a placa. Os arquivos desta etapa estão em `captures/fxlab/tread_study\plano_final\` (`r1_medidas.jpg` e os scripts).