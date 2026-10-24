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

# Redesenho (14/10, depois da reprovação no jogo)

O Felipe viu a v12 no jogo (14/10, ~1h) e reprovou:
- "exagerada, fora da realidade, parece jogo de 2012";
- "entra dentro dos outros tanques e do próprio tanque";
- "tem que ficar a cor exata do chão; areia mais clara, poeira mais clara";
- "testar teste por teste, posição por posição".

A poeira fica no pacote da v2, por decisão dele. O conserto vai no próximo.

**Estudo** (9 agentes: 3 diagnósticos medidos, 3 pesquisas, 2 propostas e 1 juiz adversário; o resultado inteiro está em `captures/fxlab/tread_study/redesenho/poeira_redesenho.json`).
Folha jogo × alvo: `captures/fxlab/tread_study/redesenho/` e o scratchpad da sessão.

**Medido no jogo contra o alvo r1:**

| | Jogo | Alvo |
|---|---|---|
| Casco do próprio tanque coberto | 19–46% (dois tanques 100%) | ≤ 5% |
| Parte densa | 1,8–2,1 cascos | 0,65 |
| Largura | 1,9–2,2× o veículo | 1,3–1,7× |
| Brilho | 1,04–1,53× a areia | 0,78–0,9× |
| Cor | fixa, laranja, mais saturada que o chão | a do chão |
| Ritmo dos discos | autocorrelação 0,23–0,33 a cada ~1,2 m | ≤ 0,1 |
| Na sombra | 5,7× o chão sombreado (consertado em 14/10) | — |

**A causa:**
- cartão de frente pra câmera, puxado 0,4 m na direção dela (camOffset) e crescendo até 8–14 m;
- tint fixo (1; 0,64; 0,38);
- espaçamento regular;
- calibrado no laboratório, de perto e na luz da r1; o jogo tem o sol a 8°.

**O plano** (a proposta A corrigida: os cartões do EmberGen, que ele aprovou na v11). Cada passo tem um A/B, primeiro no laboratório na câmera do jogo e depois no jogo:
- **0. Bancada de medida (CPU primeiro, depois o jogo)**
- **1. Tamanho e lugar, só no perfil (tread_v13.json)**
- **2. A máscara de unidade (o defeito obrigatório)**
- **3. A cor do chão e o brilho na luz do jogo**
- **4. Sem rosário (só código, sem gravar de novo)**
- **5. Luz e sombra**
- **6. Outros veículos, custo e fechamento**

**A especificação do visual** (os números com fonte, inteiros no JSON):
- **Lugar:** atrás e ao lado; nunca sobre o convés nem sobre a torre.
- **Altura:** ≤ 1,2 m ao lado das lagartas; ≤ 2,3 m até 0,3 casco atrás.
- **Largura:** 1,3–1,7× o veículo.
- **Comprimento:** densa até 0,65 casco; véu até 1,0–1,6 casco. O "longo" (3 cascos) vai em A/B pra ele decidir.
- **Cobertura:** torre 0%; acima da roda motriz ≤ 2%; contraste do casco ≥ 0,9; telhado 0%.
- **Cor:** o matiz do chão onde nasceu, ±2°.
  - Nunca mais vermelha que o chão. O topo um pouco mais pálido que o chão.
  - Proporcional ao chão: Y_poeira(A)/Y_poeira(B) = Y_chão(A)/Y_chão(B), ±10%.
- **Brilho na luz do jogo:** p50 0,78–0,9 da areia ao sol.
- **Na sombra:** 0,5–0,6× a poeira ao sol e 1–2× o chão sombreado.
- **Forma:** autocorrelação ≤ 0,1.

**A cor do chão** (o jeito do Dagor/War Thunder: a cor do terreno lida onde o efeito nasce):
- uma captura ortográfica da BaseColor do Landscape, de cima, no carregamento;
- cada cartão guarda a cor de onde nasceu (média de ~1,5 m);
- no material, a regra de Hapke: a nuvem fina fica com o albedo de espalhamento simples, w = 1 − ((1 − A)/(1 + A))², mais claro e pálido; a densa fica com a cor do chão;
- um ganho só (G), calibrado na luz do jogo;
- a prova: a captura bate com a BaseColor vista pela câmera do jogo (≤ 2/255), e uma reta areia clara → escura → terra → sal → rocha → asfalto.

**O teste antes de mostrar a ele:**
- a matriz no jogo: rumos de 45 em 45°, 5 posições na tela, 3 zooms, sozinho, coluna, par, prédio, sombra, cada chão, as velocidades, os veículos, os níveis Épico e Ultra;
- poeira ligada × desligada na mesma semente;
- recortes 4× de cada veículo, folha ao lado da r1, vídeo;
- uma tabela caso × portão com o número medido.

**O custo da v12 hoje** (A/B no mesmo processo, 40 tanques na câmera do jogo, sol a 8°), na placa:

| Preset | Custo |
|---|---|
| Baixa | +2,3 ms |
| Média | +4,3 ms |
| Alta | +4,4 ms |
| Ultra | +6,3 ms |

- No Ultra, a sombra no chão custa ~1,9 ms sozinha: com o sol baixo, cada sombra estica 6×.
- A CPU da poeira fica em 3,2–4,9 ms.
- O passo 1 deve levar o K2 a ~0,27× disso. Tudo é remedido no passo 6.

**Pausa:** às ~02:55 de 14/10 o Felipe parou todos os testes na placa até jogar a v2 com o pai. O redesenho continua só em código, sem compilar, até ele liberar.

**16/10, madrugada: o redesenho contra o game shot, imagem sobre imagem** (o pedido dele: "coloque nas mesmas posições, na mesma direção, compare imagem por imagem, sobreponha").
- **A régua:** o FXLab põe o tanque na pose do tanque GLA da r1.
  - Na mesma tela: 1024×572, `-CamScreen=0.282,0.649`.
  - Na mesma direção: `-CamRel=55`, medido pela inclinação do casco na tela (18°) com a câmera a 28°.
  - No mesmo tamanho: `-CamFrac=0.125`.
  - Na mesma luz do alvo, numa reta de areia aberta, sem sombra (`-TreadPath=open`).
  - `FXLab/Tools/tread_overlay.py` faz a folha (alvo, nosso, sobrepostos e diferença), o GIF que pisca e as medidas nas mesmas regiões: brilho ÷ areia ao sol, barulho = o detalhe miúdo dentro da poeira ÷ o do chão, e saturação.
- **Os consertos que a régua mostrou:**
  - a cor do chão vinha preta. Com o Landscape em Nanite, quem desenha é o LandscapeNaniteComponent; e no meio da compilação dos shaders o terreno sai cinza, com o material padrão;
  - a direção do sol vai no normal do vértice, e o teste de identidade bateu (luma p50 0,131 contra 0,130);
  - o expoente e a saturação foram por parâmetros sem uso na poeira, porque um quarto canal no vetor quebrou o material;
  - a sombra no chão com o sol baixo do jogo (8°) virava mancha de óleo: agora a força cai na proporção do esticamento, calibrada no sol da r1.
- **As voltas** (o miolo ÷ areia; o alvo dá 0,90, barulho 0,32, saturação 0,46 com a areia a 0,48):

| Versão | O que entrou | Miolo | Barulho | Saturação |
|---|---|---|---|---|
| v15b | a cor do chão + G 3,2 | 0,85 | 0,52 | 0,57 |
| v16b | o pálido no miolo, a folha 2,5 mips acima, sem o jato K1, cartões a cada 0,8 m (uma massa só) | 0,79 | 0,64 | 0,58 |
| v17b | sem erosão, a nuvem 1,7, G 4,2 | 0,89 | 0,65 | 0,59 |
| v18a | a 2ª opinião: contraste das 6 luzes 45 % pro neutro, alfa 0,6, sombra 0,15, névoa H | 0,75 | 0,48 | 0,57 |
| v19b | G 5,2, saturação 0,6×, alfa 0,7 | 0,92 | 0,54 | 0,53 |
| **v20b** | a luz no volume inteiro (o lado ao sol mais claro, sunSide 0,6), G 6,0 | **0,90** | **0,49** | **0,53** (0,95× a do chão; o alvo, 0,96×) |

- **Na câmera e na luz do jogo** (sol a 8°): pálida e macia nos três rumos, sem cobrir o tanque. Na coluna de 3, a máscara deixa os cascos de trás limpos (`captures/fxlab/tread_base/gj_v19b_*`).
- **Falta, contra o alvo:** os montes do alvo têm topos creme mais definidos, e o nosso véu fino não brilha acima da areia (véu 0,81 contra 1,16).
- **O pacote:** `FXLAB_TREAD_PROFILE=tread_v20b.json` em `pack_tread_dust.py` gera `Resources/tread.json` e as folhas BC7 como padrão.
- **A v21:** a v20b sem a névoa H, que na câmera do jogo aparecia como discos pálidos. Está no jogo desde as 06:49 de 16/10, com `Resources/tread_longo.json` pro A/B do comprimento.
- **A sombra do cenário no jogo:** a calibração foi refeita em três voltas.
  - A 1ª deixava a tolerância crescer junto com o espalhamento, e uma orientação errada "batia" 80 de 81.
  - A 2ª cortava em 4 m, e nada passava.
  - A de agora usa a orientação fixa da Unreal, (+1, −1). No jogo dá mediana 32 cm, espalhamento 181 cm e folga 400 cm.
- **O custo da v20b** (40 tanques, câmera do jogo, A/B no mesmo processo), na placa:

| Preset | v12 | v20b |
|---|---|---|
| Baixa | +2,3 ms | +1,0 ms |
| Média | +4,3 ms | +1,2 ms |
| Alta | +4,4 ms | +1,1 ms |
| Ultra | +6,3 ms | +1,7 ms |

  A CPU fica em 2,4–3,2 ms. Na `chao` do jogo, o quadro deu 32,7–32,9 ms, contra 33,2–33,4 com a v12.

**16/10, manhã (depois do "gostei do rastro curto!"):**
- **A cor:** o Felipe disse que a poeira está "sem cor porque eu pedi pra ser a cor sempre do chão; a nossa areia que está escura". O teste da cor passou pra areia nova, `World_TestSandV2`, na luz do jogo (o Look confirmou: matiz 29° contra 30° do r1). Os cenários são `poeira_{estrela,formacao,nuvem}_perto_{sandv2,sandv2dune,groundv3dune}`, em (30, −340) m.
  - Na areia clara a poeira sai creme. Onde o tanque passou pela terra do oásis, sai marrom e leva essa cor por uns metros pra cima da areia (`captures/game/pz_sandv2_estrela`).
- **A quina reta** (vista ampliada na estrela, aos 3 s): o alfa borrado vazava do octógono.
  - O bias somado ao nível da distância dava, de longe, um nível alto, e o octógono tinha sido medido na folha nítida.
  - Corrigido com o `mipMin` (o nível mínimo, igual em todo zoom) e o octógono medido na folha borrada (`atlas_cut8.py`, nível 4, +16% de área). É a v24.
  - Na pose do r1 a v24 sai igual à v21: o miolo 0,91, o barulho 0,55 contra 0,60.
- **A sombra do cenário com o terreno à parte:** a casa, a pedra e a árvore têm folga de 50 cm; só o terreno fica com a folga grande. Detalhes no `INTEGRACAO.md`.

**16/10, tarde: o Felipe escolheu a v26** ("fico com a v26, tá ótima pra início de jogo"; a v27, a gravação que se desfaz dentro, fica guardada). **O pedido seguinte: "as sombras do mapa não projetam na poeira, ela sempre brilha".**
- **O diagnóstico** (o estudo, a revisão adversária do desenho e a do código; tudo no scratchpad da sessão da Arte, resumido aqui):
  - os prédios ficavam fora do mapa: eles levam a etiqueta `RTSUnit`;
  - a sombra do tanque era por cartão, com 2 pontos de altura (T ≈ 0,5 no cartão inteiro, sem forma);
  - a do terreno tinha a folga de 4 m ao longo do raio, por causa do Nanite contra a colisão.
- **O feito: a sombra por pixel** (o modo 2, `RTSTreadDustShadow.cpp`; detalhes e os ganchos do jogo no `INTEGRACAO.md`).
  - Um mapa de altura da sombra numa grade girada pro sol, a partir de 4 fotos ortográficas.
  - Uma grade dos tanques, refeita só quando alguém anda.
  - O material compara a altura do pixel com elas, filtrando a corda da nuvem.
- **A 2ª revisão do código** (8 agentes, 4 frentes, cada achado conferido por outro): 30 achados reais, todos corrigidos. Os maiores:
  - a sombra podia clarear o pé da nuvem;
  - o filtro de finos tirava os muros da vila;
  - a grade dos tanques custava ~1 ms;
  - refazer a área de um prédio travava a partida (agora a leitura da placa é sem esperar).
- **As medidas, no FXLab** (`World_TestSalt2`, luz `desert_golden_dust`, sol a 8°):
  - o chão das fotos bate com os raios no terreno: mediana 0, p75 1,1–1,4 cm;
  - o mapa chega em ~1,2 s (36 blocos, sem travar);
  - na câmera do jogo, quando o tanque entra na sombra do paredão, ele perde a sombra própria e o mapa diz 3–5 m de sombra: os dois batem;
  - o custo, com 40 tanques na câmera do jogo: **+0,10 ms na placa e +0,16 ms na CPU**.
- **O céu na sombra, medido** (brilho da poeira ÷ chão em volta, na câmera do jogo):

  | | Ao sol | Na sombra |
  |---|---|---|
  | Modo de antes | 1,11 | 1,19 |
  | Céu de volume da revisão física | 1,17 | **1,36** |
  | Céu do visual aprovado (só sai o sol) | 1,15 | 0,71 |
  | **Metade de cada (o padrão, `"sky": 0.5`)** | **1,16** | **1,03** |

  - O céu de volume fazia a poeira na sombra brilhar mais que o chão: era o defeito que ele apontou.
  - Só o céu aprovado virava mancha escura.
  - A metade deixa a poeira no tom do chão sombreado, dentro da faixa física da revisão (0,75–1,1).
  - No vídeo, a borda da sombra atravessa a nuvem: a parte que entrou escurece e a que está no sol fica clara (`captures/fxlab/tread_base/sf_game_m2.mp4` × `sd_game_m0.mp4`).
- **No contraluz (a deitada)** a poeira ficou mais clara e acesa, mais perto do game shot. Saiu o escurecimento falso por cartão perto do tanque. É mudança de visual: ele julga.

**16/10, noite: a sombra por pixel REPROVADA ("MUDOU NADA") e o redesenho como VOLUME.** O Felipe: "o BlastLab fazia
perfeito nas fumaças deles, inclusive luz de explosões rebatendo... não joga ajuste gambiarrento em cima de outra coisa
feita errada... se for pra quebrar o projeto e refazer do 0 que seja". A regra nova do CLAUDE.md (toda fumaça, fogo e
poeira tem volume, recebe a sombra do mapa, projeta sombra e é acesa por fogo e explosões; um sistema só) vale pra ela.
- **O desenho** (estudo + juízes; o plano inteiro no scratchpad da Arte, `dust_redesign_plan.md`): a poeira vira mais
  um meio no raymarch das explosões (o `BlastRaymarch` do Fluxo), com a MESMA luz (a fase HG dupla, as 3 oitavas, o
  passo de Hillaire, o sol e o céu da View, as luzes do `RTSSceneLights`). A simulação da v26 fica inteira (nascer,
  andar, crescer, sumir): cada cartão vira um puff da MESMA gravação do EmberGen das folhas aprovadas.
- **O campo** (módulo `RTSDustField` no plugin `RTSTreadDust`; `RTSDustField.h`, `DustField.ush/.usf`):
  - tijolos de 2 m (voxels de 0,25 m) presos ao mundo, numa janela de 192 × 192 × 32 m em volta de onde a câmera olha;
    tabela de páginas + uma grossa de 8 m (o salto dos vazios);
  - a densidade de cada tijolo é a soma dos puffs (a gravação `Resources/dust_kick_v2.rtsdust`, 48 quadros de
    48×48×24 a 0,1 m, feita por `FXLab/Tools/build_dust_volume.py`), com a erosão do BlastLab
    (ρ − E·(0,5 − 0,5n)/(1 + 9ρ), o Worley do mundo subindo 0,3 m/s);
  - a luz por célula de 0,5 m: a sombra da própria poeira até o sol (a de um tanque cobre o rastro do outro), a do
    mapa do sol comum (`RTSSunShadow`), o céu pela poeira acima, e as luzes do fogo e das explosões com a
    transmitância pela poeira;
  - metade dos tijolos refeita por quadro, em xadrez (os novos sempre).
- **A cor:** o albedo de espalhamento simples sai da cor do chão onde o puff nasceu pela inversão de Hapke
  (r0 = (1 − γ)/(1 + γ), γ = √(1 − w)), com a saturação relativa do perfil (0,6) e teto 0,97. Sem ganho de material.
- **A espessura, medida** (`calib_vol.py`: o volume projetado pela câmera do cozimento contra o alfa da folha, alinhados,
  erro de α 0,6–4 % nos quadros 4/12/24/40): o τ do volume dá 1,5–1,8× o da folha → `"volume": {"density": 0.62}`.
- **A luz, medida** (poeira ÷ chão em volta, mediana do miolo; `captures/fxlab/tread_vol/`). A luz é a do raymarch
  (a fase HG dupla, 3 oitavas, Hillaire); os dois ajustes por instância que ele já tem (SunFull, SkyGain) saem de
  dois alvos: ao sol 0,90 (a r1) e na sombra do paredão 0,90 (um pouco abaixo do chão sombreado, pra sombra se ler;
  a faixa física é 0,75–1,1).

  | Luz do jogo (sol a 8°) | Ao sol | Na sombra do paredão |
  |---|---|---|
  | Cartões da v26, sombra por pixel | 1,15 | 1,06–1,14 (o "sempre brilha") |
  | Volume, céu 1,1 (o das explosões), sol inteiro | 0,97 | **0,19** (mancha escura) |
  | Volume, céu 2π (o físico) | 1,23–1,30 na r1 (pálida, cor de vapor) | ~1,1 |
  | Volume, céu 5,2 e o sol 27 % inteiro (a conta linear; não vale: as fotos passam pela curva de tela) | 1,37 | 1,58 |
  | Varredura (sw1), céu 2,6 / sol 0 e 0,27 | 0,64 / 0,85 | 0,60 |
  | Varredura (sw1), céu 3,4 / sol 0 e 0,27 | 0,85 / 1,03 | 0,92 / 0,91 |
  | Volume, céu 3,4 e o sol 8 % inteiro (interpolado) + a sombra própria fina | 0,89 | 0,91 (mas na r1 0,77–0,85: marrom, cara de fumaça) |
  | **Volume, céu 3,4 e o sol 27 % inteiro + a sombra própria fina (o padrão)** | **1,02** | **0,92** |

  - Na régua r1 o padrão dá miolo 0,83 / 0,84 / 0,90 (o alvo 0,90; os cartões 0,84 / 0,90 / 0,91) e a saturação da
    areia (0,55–0,56).
  - A sombra própria em duas escalas (as nuvens AAA): a longa nas células de 0,5 m e a curta em cada amostra (3
    amostras nos primeiros 0,5 m até o sol). Dá o topo aceso e a reentrância escura de cada bolo.
  - Reprovado por mim: a erosão fina na marcha (o ruído de 0,15–0,6 m a 2/m) deixou a poeira granulada e suja.
  - A luz do fogo, à noite (`-TreadFire`): nos cartões a poeira vira uma silhueta escura contra o clarão; no volume o
    lado virado pro fogo acende laranja e o de lá fica azul de lua.

  - Na régua r1 com céu 1,1: miolo 0,87 / 0,89 / 0,94 (o alvo 0,90; os cartões 0,84 / 0,90 / 0,91), saturação 0,57
    (areia 0,55; o alvo 0,96×). A saturação da v26 (0,6) era da regra do material dos cartões; no volume a inversão
    de Hapke já empalidece, e com 0,6 dava 0,73× a da areia: `"sat": 1.0`.
  - A erosão do cartão estava numa unidade errada no volume (÷ (1 + 9ρ), a do BlastLab pra densidade normalizada) e
    quase não comia: a cauda não sumia. Agora é a do cartão, em extinção (E ÷ a corda da nuvem).
  - O detalhe: o campo foi de 0,25 m pra 0,125 m (a gravação é de 0,1 m); a cor fica na grade de 0,5 m.
- **O custo, medido** (`tread_measure.ps1 -AB field,volume -Grid 8x5`: 40 tanques, 1920×1080, câmera do jogo, A/B no
  mesmo processo, 12 pares): 3996 tijolos de 2 m, 1249 puffs (cada ponto soma 10–20 puffs sobrepostos, como os cartões
  da v26 se empilham). Só os passes do campo: **+5,3 ms de placa a mais que os cartões**; com a marcha da depuração,
  +7,0 ms. O caro é o preenchimento a 0,125 m.
  - Ler a gravação só dentro da caixa da nuvem de cada quadro: a nuvem ocupa 16–37 % do domínio; a imagem é a mesma
    (diferença média de ~1/255). Os passes do campo foram de +5,3 pra **+3,2 ms** a mais que os cartões.
  - Os presets, pelo `sg.EffectsQuality` (a mesma régua do raymarch; `rts.DustField.Fine` força): **Ultra** (3 ou
    mais) a 0,125 m e metade dos tijolos por quadro, +3,2 ms a mais que os cartões; **Alta** a 0,25 m e um quarto dos
    tijolos por quadro, **−0,8 ms (mais barata que os cartões)**, um pouco mais mole (`captures/fxlab/tread_vol/alta_vs_ultra.jpg`).
- **No jogo** (o Fluxo integrou no `MarchJoint` em 16/10 à noite; `rts.TreadDust.Path=1`): a cena `poeira_sombra_wf2`
  (dois tanques entram e saem da sombra da War Factory, sol a 8°). Na sombra: **cartões 1,30× o chão sombreado
  (brilham), volume 1,04** (`captures/game/poeira_volume_jogo_sombra_wf.mp4`, lado a lado). Falta a nitidez: o raymarch
  marcha em meia resolução com acúmulo e a poeira fica mais mole que os tanques; o Fluxo testa a resolução inteira, menos
  acúmulo e a subida bilateral. À noite (`poeira_noite_tiro2`) a poeira em volume dá 0,65 do chão ao luar (a lua por trás
  da câmera; a poeira espalha pra frente); o clarão de um tiro no chão é curto demais pra acendê-la.
- **A luz das explosões, no jogo, à noite** (`poeira_noite_morte4`: a morte na hora em que a coluna passa colada, com
  `-SceneLightsLog=1 -RmDustLog=1`): as luzes chegam ao campo (3 no quadro da morte; com o alcance × 3 do Fluxo, raio de
  18–27 m e ~0,04–0,075 de irradiância a 6 m). O laranja não aparece porque a noite do `desert_night` é uma "lua clara"
  quase tão forte quanto o sol: a exposição é 8,05e-5, contra 6,95e-5 de dia. Não é a compensação de exposição. Nem a
  fumaça da própria explosão fica laranja, e a poeira acompanha.
  - No laboratório, com uma luz de fogo forte (raio de 30 m), a poeira em volume acende laranja do lado do fogo
    (`captures/fxlab/tread_vol/poeira_volume_noite_fogo.mp4`).
  - A saída é decisão de visual, e o Fluxo leva ao Felipe como A/B: fogo mais forte na fumaça só de noite, ou uma noite
    mais escura (a receita do Look).
  - **A prova no jogo** (`poeira_noite_morte5`, a coluna a ~1,8 m da borda da carcaça já em fogo, 2–2,75 s depois da
    morte; a luz da carcaça acende 0,8 s depois da morte desde 17/10): a razão vermelho ÷ azul da poeira em volume é
    0,539 perto da carcaça contra 0,444 longe, **+21 % mais quente** perto do fogo. Nos cartões é 0,544 contra 0,522
    (+4 %). A luz pega, mas fraca no olho com a lua clara.
- **A nitidez no jogo** (17/10 de manhã): o Fluxo passou a marcha da poeira pra resolução de tela, dentro da composição,
  só onde o raio cruza a caixa dela. Os gomos e a borda dos bolos aparecem como na resolução inteira, por +0,1 ms
  (`-RmDustFull=0` volta a meia). As provas regravadas com ela são as que foram ao Felipe:
  `captures/game/poeira_volume_jogo_sombra_wf_v2.mp4` (de dia, na sombra da War Factory) e
  `captures/game/poeira_volume_jogo_noite_fogo_v2.mp4` (de noite, colada na carcaça acesa: +20 % mais quente perto do
  fogo no volume, contra +4 % nos cartões).
  - De quebra, o teste achou um defeito do jogo: a carcaça só pegava fogo quando a morte vinha pelo acerto marcado
    (consertado pelo Fluxo e pelo Jogo, compilação 43). A luz da carcaça (1,5e5 cd, raio de 7 m, ~1 m acima do casco) é
    pequena de propósito: a 4,5 m chega ~1/20 do que chega no chão colado.
- **A nitidez, o pacote (17/10 à tarde; o Felipe: "ficou macio de mais"; estudo e revisão em workflow, scripts em
  `scratchpad/nitidez`).** Cinco perdas, medidas na CPU e confirmadas no lab, na mesma história:
  - **A densidade era ρ, e a folha foi cozida com ρ².** O Principled Volume do Blender multiplica a grade de novo.
    - Com ρ² e a gravação inteira de 0,05 m, o volume bate com o alfa das folhas sem fator nenhum: τ 0,94–1,01,
      cobertura 1,00 (`calib_vol.py`).
    - O 0,62 de 16/10 remendava o ρ linear com a média 2×2×2. A média tirava ~33 % da banda de 1–2 px e o pico caía
      37–42 %.
  - **O céu curto:** cada amostra soma a poeira dos primeiros 0,5 m acima. As células de 0,5 m não enxergavam o gomo, e
    a reentrância embaixo do bolo não escurecia.
  - **A areia tapada pela poeira de baixo:** o rebote do chão passa pela poeira embaixo (`TauDown`).
  - **A tremida da marcha** se perdia depois de saltar um vazio: degraus de 0,125 m presos aos planos dos tijolos.
    Agora é a marcha estratificada, no jogo (Fluxo) e na vista do lab. Cada amostra cobre um trecho, sem viés, e o
    último trecho fica na frente do chão.
  - **As oitavas exponenciais** (T^(a^i), Wrenninge 2013 e a VolumetricCloud do UE) dão +15 % de detalhe a mais, mas a
    poeira teria uma conta de luz diferente da fumaça das explosões, no mesmo quadro. Ficam na chave
    (`-RmDustOctExp`, `rts.DustField.DebugOctExp`); o padrão é o lerp de sempre (a variante A). É decisão do Felipe.
  - **O resultado**, de perto (câmera a 35 m, 4 tempos, bandas DoG contra os cartões, `nit5_cmp.jpg`):
    - normalizado pela média: volume de antes 0,87× (1–2 px) e 0,73× (2–4 px); A 1,06× e 1,22×; B 1,15× e 1,42×;
    - em valor absoluto, com a mesma máscara pra todas: antes 0,31× e 0,45×; A 0,72× e 0,92×; B 0,83× e 1,06×.
    - Os cartões continuam mais claros (poeira÷chão p50 3,2 contra 2,2). O tom de A é o do volume aprovado (2,24 contra
      2,35).
  - **O movimento** (16 quadros seguidos, `nitidez/cint.py`): a fina pisca 1,2× a grossa, na mesma proporção do
    detalhe que ganhou (por detalhe, 4,7 contra 5,0). Não serrilha.
  - **O custo** (passes do campo, 40 tanques, 1080p): a gravação fina +4,1 ms, a de 0,1 m +3,7 ms. A fina custa
    +0,4 ms no pior caso, pelo cache.
  - **Os presets:** a Alta do jogo é o nível 3 do motor e o corte era ≥ 3, então a Alta pagava o Ultra. Agora o Ultra
    é o nível 4.
    - Cada preset tem a sua gravação: a fina de 0,05 m no Ultra; a grossa de 0,1 m com a média de ρ² na Alta
      (`build_dust_coarse.py`).
    - A média de ρ elevada depois perdia 18–31 % da massa com a idade (Jensen). A média de ρ² fecha em 1,000.
    - As duas vão comprimidas (zlib): 42 MB → 1,8 MB e 0,3 MB no disco.
- **A conferência do vídeo A × B e os consertos (17/10 à tarde; 5 revisores no jogo, `scratchpad/nitidez/verif`).** O Felipe
  escolheu a A ("ok eu gostei da A também, pode colocar no jogo"). A B tinha bolsões quase pretos no sol, com cara de
  fuligem. O custo das duas é o mesmo.
  - **O brilho, calibrado NO JOGO** (`poeira_sombra_wf2`, câmera fixa; a poeira ÷ o chão com o fundo pela mediana no
    tempo, `scratchpad/nitidez/measure_wf.py`). Com o céu curto e a areia tapada, a sombra tinha escurecido 12 % contra a
    de manhã (corpo 0,72 contra 0,82).
    - O céu da poeira foi de 3,4 pra 3,84, e o corpo voltou a 0,82.
    - No sol, a poeira fica em 0,68 da areia, igual à de manhã. O game shot r1 pede 0,90.
    - Subir o sol inteiro pra 0,6 clareia o sol (0,80), mas acende a poeira dentro da sombra do prédio (corpo 1,21). É o
      defeito dos cartões, então o sol ficou em 0,27.
    - **Pendente:** o sol mais claro pela física (o sol inteiro de 0,27 não é físico; ver a revisão do plano).
  - **O nascer suave:** no cartão, o fade de entrada escala a OPACIDADE; no volume ele escalava σ. Um puff de miolo τ ~8
    com 20 % do fade já saía ~80 % opaco, e o tufo aparecia inteiro de um quadro pro outro.
    - Agora o fade vira o σ que dá a mesma opacidade no miolo: −ln(1 − Att·(1 − e^(−τ)))/τ, com τ a coluna densa do
      quadro da gravação (`RTSDustField::KickColumn`, o p95 das colunas). O adulto não muda.
    - No lab, os maiores saltos entre quadros caíram 35 % (3,08 → 2,02). Ainda acima dos cartões (1,2).
  - **Pendentes da conferência:**
    - um disco claro e achatado no pé da poeira (2,8–3,2 s);
    - os tufos redondos e iguais (menos fiapo que os cartões);
    - o véu liso e "penteado" quando os tanques param.
  - **No jogo de fábrica:** `rts.TreadDust.Path` −1 (o volume na Alta e no Ultra, os cartões na Média e na Baixa).
    - Na Baixa o volume custaria +1,5 ms na 1660 com 8 tanques (11,0 → 12,5 ms; `captures/game/nitjogo/cu_baixa_*.csv`).
    - Num notebook de 2014, 4–6 ms. Por isso a Baixa fica nos cartões.
- **A chave:** `rts.TreadDust.Path` −1 = pelo preset (o padrão desde 17/10), 0 = os cartões da v26, 1 = o volume (os
  cartões e a sombra deitada somem).
  O perfil `tread_v29.json` é a v26 + `"volume"` (densidade 0,62, a conta velha); o de hoje é o `tread_v31.json` e o
  `Resources/tread.json` (densidade 1,0). No FXLab, `rts.DustField.Debug` 1–5 marcha o campo sozinho (só pra
  conferir forma e luz; a entrega é no raymarch do Fluxo, no jogo).
- **A ordem:** o Fluxo integra no `MarchJoint` (a densidade da poeira entra também na marcha até o sol do mapa de sombra
  das explosões: a poeira faz sombra no chão e nos tanques) → provas no jogo, ampliadas (na sombra de prédio, ao lado
  de explosão, de noite, 40 tanques) → o Felipe aprova → só então os cartões e as gambiarras de sombra saem.

## v4 (22–23/10): fraquinha, lisa e sem a linha

O Felipe (22/10): "muito tenebrosa, nem parece que tá no jogo", "fps baixo" ("a poeira parece que está com um FPS
diferente do jogo, ela se movimenta travadinho, enquanto o jogo roda liso"), "dá pra ver uma linha no meio da partida
onde ela começa"; quer "bem fraco, bem menos denso", igual à areia do ataque do GLA (`r1_raid_slabs`).

- **O travadinho, três causas, as três consertadas:**
  - o campo refazia metade dos tijolos por quadro (xadrez): cada metade andava a 15 qps. Agora `rts.DustField.Refresh`
    1 (padrão) refaz todos a cada quadro (2 e 4 voltam ao xadrez, só pra PC fraco);
  - a gravação tinha 48 quadros com passo de 0,033 s no começo a 0,2 s no fim: a nuvem velha andava aos pulos de 5 qps.
    Agora `dust_kick_v3_30hz.rtsdust` (149 quadros a 30 qps, `build_dust_volume.py` com `DUSTVOL_FRAMES=all` e
    `DUSTVOL_RHO2=1`; média de ρ², 0,1 m, 1,04 MB) e o quadro pelo tempo dela (`RTSDustField::KickFrameAt`);
  - a tremida da marcha ficava parada na tela (o `Jitter·71,13` mexia ~0,005 por quadro): o degrau de 0,125 m não
    somava no TSR. Agora `IGN(p + 5,588238·n)` (`BlastRaymarch.usf`).
- **A linha:** a janela do campo era menor que a tela no zoom de jogo. Agora 128 × 128 × 32 tijolos (256 m), puxada
  40 m pra frente (a câmera inclinada vê mais chão acima do centro) e a borda some em 16 m (`DustField.ush`). Prova no
  zoom mais longe (170 m, cena `poeira_linha`, `captures/game/v4p_linha2`): a poeira chega fraquinha no fundo da tela,
  sem corte. (Achado de passagem: a faixa clara reta no lago do Salt2 nesse zoom é da água com a névoa, existe com a
  poeira desligada e com a receita velha; não é da poeira.)
- **"Não encaixa":** a névoa do mapa agora cobre a poeira também (a mesma conta das explosões, no `BlastRaymarch.usf`).
- **A densidade:** `"volume": {"density": 0.08, "erode": 0.08}` nos dois `tread.json` (a erosão escala junto). Varredura
  0,05 / 0,08 / 0,12 nas cenas `poeira_sombra_wf2` e `_perto` (`captures/game/v4d_*`), lado a lado com o recorte da r1:
  0,12 já vira bola de algodão; 0,05 some na câmera do jogo; 0,08 fica no véu de fiapos da r1.
  - A régua do brilho (`scratchpad/nitidez/measure_wf.py`, agora alinhada pelo fim do vídeo): 0,08 dá poeira ÷ chão
    0,76 na sombra e 0,63 no sol (a A de 17/10: 0,72 e 0,68). A 0,12 acende na sombra (1,54): mais um motivo pra
    ficar fina.
- **A prova da regra da fumaça** (`poeira_prova_v4`, `captures/game/v4p_prova`): os tanques cruzam a sombra da War
  Factory com um 3º atirando no chão 12 m ao lado; a poeira escurece na sombra, clareia no sol e fica ao lado da
  fumaça do tiro.

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