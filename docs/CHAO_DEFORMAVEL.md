# O chão que afunda onde o tanque passa

Estudo de 12/10/2026, sessão do jogo, com pesquisa e revisão por agentes. Pedido do Felipe, com a
imagem do Gemini (`captures/game/esteira_v1_salva/alvo_gemini_chao_mexido.webp`): "não parece só uma
textura em cima, parece que o próprio chão foi manipulado". **Dono: a sessão do Look**, por decisão do
Felipe em 12/10. O jogo entrega o caderno e a interface.

## DECISÃO DE 14/10, MADRUGADA (leia primeiro): lagarta e cratera no chão vão pra v3

**O Felipe:** "pra v2 não vai ter mais nenhum ajuste, isso aí tudo vai ser pra v3, estou bem decepcionado". O motivo dele: na maquete a areia, a trilha e a cratera eram lindas; no jogo viraram risco e mancha.

**O que vai na v2:** o chão do mapa com o CODE do `look_mf.py` de 14/10 00:38 (o ajuste da seção "14/10, madrugada" abaixo). A prova ficou pela metade: a foto das lagartas rodou (`captures/game/lk_esteira_depois`); a da cratera e a do tiro não, paradas pra liberar a placa pra v2. Não mexer mais nisso pra v2.

**Por que caiu da maquete pro jogo (a conversa com ele):**
- A maquete é foto de estúdio: câmera perto, render sem pressa, relevo 3D de verdade com sombra dentro.
- O jogo desenha o mapa inteiro 60 vezes por segundo a ~100 m, em 67% da resolução com TSR: cada pixel cobre ~10 cm de chão, e a garra tem 17 cm de período.
- No jogo, o relevo virou pintura com truque de luz (a tesselação furava no zoom perto e foi pra v3). Com sol baixo, sulco pintado vira risco; cratera pintada vira mancha.
- O material do chão foi traduzido por várias mãos (Look → WorldForge → jogo), e numa luz diferente.
- A falha nossa: aprovar na maquete sem a foto do jogo ao lado.

**O caminho pra v3** (a estudar no método de sempre: fontes, conta, 2ª opinião, discussão com ele, prova no jogo):
1. **Relevo por pixel** no chão perto da câmera (parallax occlusion mapping, com sombra dentro do sulco e da bacia), e a tesselação de verdade só onde couber.
2. **A sombra do sol calculada uma vez, na gravação.** Na partida o sol não anda e a câmera tem ângulo fixo: dá pra calcular a luz de dentro de cada sulco e cratera com qualidade de cinema no momento em que o tanque passa ou o tiro cai, e o quadro só lê. Hoje são 3 passos com pixel de 9,4 cm.
3. **A forma desenhada pra câmera do RTS:** a 100 m tem de ler a valeta, a borda empurrada e a sombra; a garra e a ondinha só no zoom perto.
4. **O processo:** nada se aprova na maquete sem a foto do jogo ao lado, na câmera do jogo.

**Referências pra começar o estudo:**
- Red Dead Redemption 2: tesselação e parallax occlusion no menu do PC ("Tessellation Quality", "Parallax Occlusion Mapping Quality"); lama e neve que afundam.
- Company of Heroes: cratera e marca no terreno, legíveis da câmera de RTS.
- Rise of the Tomb Raider: a deformação da neve em volta do jogador (GPU Pro 7).

## ESTADO EM 13/10 À NOITE: as lagartas no jogo ainda não ficam como o alvo: as lagartas no jogo ainda não ficam como o alvo

**O alvo aprovado:** a areia v2 "C" da maquete (12/10), na folha `captures/look/areia_C_final.jpg`. Lá a trilha tem:
- relevo de verdade: sulco de 2,2 cm, garras a cada 17 cm e a bordinha com sombra;
- a idade;
- uma trilha velha cruzando, já meio coberta pelo vento, com a nova passando por cima.

O mapeamento das trilhas está no `Game/Tools/look_layout_ground.py` ("B = uma trilha fresca; C = + uma velha cruzando, já meio coberta pelo vento"). As imagens de partida são `captures/game/esteira_v1_salva/alvo_gemini_chao_mexido.webp` ("não parece só uma textura em cima, parece que o próprio chão foi manipulado") e o recorte da r1_raid_slabs (`captures/look/alvo_poeira_dos_veiculos_r1_raid_slabs.jpg`).

**O que o Felipe viu no jogo (13/10 à noite):** a marca da lagarta sai como risco chapado ou nem aparece. "Não modela." É um problema antigo, não é da luz.

**Os donos** (a decisão do Felipe de 12/10 está acima):
- **Look:** o visual inteiro. Isso inclui a MF `Game/Tools/look_mf.py` (o CODE que o WorldForge põe dentro do chão do mapa) e a medida contra a folha C.
- **Jogo:** grava as fitas e calcula o relevo (`RTSGroundMemory`, `GroundRelief.usf`): a altura, a idade, o sol e a areia revolvida nos cadernos.
- **WorldForge:** monta o chão do mapa com o CODE.
- **O relevo de geometria** (DISP_CODE, deslocamento Nanite) está desligado no mapa por causa dos buracos. A proposta é ligar só no ultra (DicingRate 4 + 8M de candidatos), com a decisão do Felipe pendente.

**O que falta, do Look, pra v2:** o chão do jogo tem de ler os cadernos do relevo como a maquete:
- o sulco com lado claro e lado escuro pelo sol;
- a bordinha;
- a areia revolvida;
- a trilha velha sumindo com o vento e a nova por cima.

A prova é no jogo, com tanque andando, uma trilha cruzando outra e uma velha, ao lado da folha C, na câmera do jogo e no zoom perto.

### 14/10, madrugada: o que foi mudado (o estudo, a 2ª opinião, a medida); ainda falta a prova no jogo

**As lagartas** (`Game/Tools/look_mf.py`, o CODE).

O diagnóstico, do estudo com o código do Zero Hour, do Dagor e do Tomb Raider, mais a 2ª opinião sobre o nosso código:
- a bordinha (9 cm) é menor que um texel do caderno e virava um degrau de 1–2 px;
- a parede contra o sol escurecia 3 vezes (N·L, B×0,85 e a diferença do R da esteira);
- o canal A (`max(Band, 0,6·Berm)`) engrossava o fio com a tinta.

Na folha C aprovada, reduzida 3× (a distância do jogo), a faixa é 12% mais clara que a areia ondulada em volta. Ela se lê pela textura das garras e pela bordinha macia, não por escurecer.

Mudado:
- o passo da diferença finita e a média de 4 amostras pelo tamanho do pixel (o que o mip fazia na maquete);
- a tinta e a máscara pelo miolo do sulco (h < 0), sem a bordinha;
- a sombra do caderno sem dobrar na parede;
- a garra só no miolo e em listra só até a Nyquist (pixel ≤ 5,7 cm).

Sai do plano: escurecer a faixa e a trilha tracejada pelo vento (a C não tem).

**As crateras de tiro.** Comparadas no jogo (`cratera_perto`, 67 m) com a v2 aprovada (`docs/aprovados/look/2026-10-13_cratera_v2.jpg`). Mudado:
- **O claro:** era um piso fixo cinza (B/R 0,6 contra 0,2 da areia) e virava mancha branca. Agora é a areia de entrada ×1,15, só nos torrões.
- **A parede:** sem relevo, a parede de dentro que a câmera não veria aparecia acesa, e a cratera lia como disco. Agora ela é achatada e ganha a cor do fundo da bacia (`Parameters.CameraVector`).
- **O manto:** mosqueado em 2 oitavas (0,6 e 1,2 m), no mundo.
- **As pedras** (o `RTSDebris` do jogo): a cor da prova aprovada, em `Game/Config/RTS/Ground/stones.json` (tint 0,7/0,6/0,5, desat 0,4, mínimo 9 cm).

**A prova:** os cenários `look_esteira`, `cratera_perto` e `tiro_chao`, contra o "antes" (`captures/game/lk_cratera_antes`, `lk_tiro_antes`) e contra as folhas aprovadas, na câmera do jogo e no zoom perto.

## O que já existe no jogo: a memória do chão, versão 1 (salva, é o piso)

- **Código:** `Game/Source/RTSGame/*/RTSGroundMemory.*`. A cópia da versão salva está em `captures/game/esteira_v1_salva/`.
- **Cadernos:** dois RGBA8.
  - **Detalhe:** 4096² numa janela de 192 m em volta da câmera, ~4,7 cm/texel. Ele se redesenha da lista de marcas quando a câmera anda 28 m.
  - **Grosso:** 2048² pro mapa inteiro (50 cm/texel).
- **Canais:**
  - R = esteira: fita por lagarta, garras de 17 cm em fase, some em ~5 min;
  - G = queimado;
  - B = cratera.
- **Desenho:** um decal DBuffer só, do tamanho do mapa (`M_GroundMemoryDecal`, `Game/Tools/build_content.py`), escreve cor e normal pelo gradiente.
- **Custo:** +0,2 ms de mediana e +0,9 ms de p95 (teste rápido "vila", com e sem `-RTSNoGround`).
- **Teste:** `game.ps1 view -Scenario vila -Folder X` (~40 s, semente fixa, a mesma cena a cada rodada).

## Como os jogos AAA fazem

Em todos, a deformação mora num render target **com sinal** (afunda e levanta) em volta da câmera, e
**quem lê é o material do chão**, nunca um decal.

| Jogo | Como | Onde guarda | Borda (berma) | Custo |
|---|---|---|---|---|
| Batman: Arkham Origins (GDC 2014) | relief mapping nos consoles, tesselação no PC | câmera ortográfica "no tornozelo" olhando de baixo, acumulada em textura dupla, FP16 ≤512² | 2 canais (altura mínima + deslocamento) | <1 ms (PS3/360) |
| Rise of the Tomb Raider (SIGGRAPH 2015) | tesselação + compute | 1024² 32 bits que segue a Lara e dá a volta nas bordas | elevação proporcional à profundidade | ~4 MB; enche o rastro com o tempo |
| Dagor (motor do War Thunder, código aberto) | grade densa deslocada + normal de 4 amostras | **R8 com 0,5 = zero**; 2048² em 90 m (4,4 cm/texel) no preset alto; segue a câmera | Laplaciano 3×3 + blur, do preset médio pra cima | ~0,5 ms (Xbox One S); só os blocos 16×16 sujos |
| God of War Ragnarök (GDC 2023) | trocou paralaxe por tesselação | carimbo de cápsulas na GPU | torrões em malha espalhados | **só os pedaços com rastro vão pro draw tesselado** |
| Spintires/MudRunner | grade deslocada | blocos de 16 m, 128² ARGB8 | implícita | simula só em volta do caminhão |

O nosso caderno de detalhe já está na resolução do preset alto do Dagor.

**Fontes:**
- [Batman GDC](https://gdcvault.com/play/1020379/Deformable-Snow-Rendering-in-Batman)
- [Tomb Raider SIGGRAPH](https://history.siggraph.org/wp-content/uploads/2022/10/2015-Talks-Michels_Labs-R_D-Rendering-Techniques-in-Rise-of-the-Tomb-Raider.pdf)
- [DagorEngine](https://github.com/GaijinEntertainment/DagorEngine): `prog/daNetGame/render/world/deformHeightmap.cpp` e `shaders/deform_hmap.dshl`
- [War Thunder Ground Breaking](https://warthunder.com/en/news/7378-development-ground-breaking-self-entrenching-and-bomb-craters-en)
- [GoW Ragnarök](https://80.lv/articles/santa-monica-s-senior-programmer-on-how-god-of-war-ragnar-k-s-snow-system-was-made/)
- [Spintires](https://gamedeveloper.com/programming/mud-and-water-of-spintires-mudrunner)
- [Tatarchuk POM](https://www.advances.realtimerendering.com/s2006/Tatarchuk-POM.pdf)
- [Sumner 1999 (deformação de chão por massa)](https://doi.org/10.1111/1467-8659.00299)

## O achado que manda no plano: o que se vê da câmera do RTS

- Câmera a ~95 m, 37,5°, ~4,9 cm/px no foco na horizontal. Na profundidade é ~8 cm/px no foco e ~19 cm/px no alto da tela.
- Um sulco de 8 cm de geometria real mexe a imagem em **1–2 px**. O que se lê é:
  1. a **sombra**: com sol a 20–30°, uma berma de 8 cm faz 14–22 cm de sombra, ~3–5 px;
  2. a **areia revolvida**: muda de material, com torrão, grão grosso e outra rugosidade, não só escurece.
- O alvo do Gemini é uma vista de perto. Antes de fixar a profundidade, medir o sulco nele com o tanque (7,9 m) como régua; provavelmente exagera.

## Unreal 5.8 (conferido no código instalado)

- **Tesselação Nanite** é o único jeito de ter geometria real em tempo de execução.
  - A documentação ainda diz experimental; `r.Nanite.Tessellation` vem ligado na 5.8.
  - O material do chão liga Enable Tessellation e lê o RT no Displacement, com Center 0,5.
  - Riscos:
    - a subdivisão custa no chão inteiro onde o material estiver;
    - **a sombra virtual (VSM) não sabe quando o RT muda**: ou fica velha, ou é "Always" redesenhando tudo;
    - há relatos de sombra em blocos e de displacement quebrado em Landscape na 5.8;
    - o bug UE-370671: não usar Multiply no pino.
- **Decal não desloca vértice.** Virtual Heightfield Mesh e Mesh Terrain são experimentais. A RVT com displacement não ganha nada sobre o RT simples.

## A revisão crítica (aprovou com mudanças)

1. **A marca não deve ficar no decal.**
   - O DBuffer usa uma opacidade só pra cor, normal e rugosidade, e substitui a normal (N·(1−α) + α·N_decal): apaga o relevo fino da areia do Look embaixo da marca.
   - A sombra feita escurecendo o albedo sai errada: marrom quente em vez de fria, e "sombra dentro da sombra" debaixo de prédio ou fumaça.
   - O decal de AO da 5.8 exige SSAO.
   - **Conclusão: o material do chão lê o caderno**, como todos os AAA.
2. **Sem paralaxe (POM).** Com sulco de poucos cm, o deslocamento é ~1 texel e menos de 1 px, e a areia do Look por baixo escorrega quando a câmera anda.
3. **Sinal da berma.** Usar a profundidade D ≥ 0 e a berma só onde D = 0, fora da fita: `k·max(0, blur(D_contorno) − D_contorno)` sobre o **contorno** da fita, sem as garras (senão os vãos viram mini-bermas).
   - k pela conservação de massa (volume da berma = (1 − compactação) × volume do sulco).
   - O ângulo de repouso da areia seca (34°) só se garante com 2–4 iterações de erosão térmica (Musgrave 1989). A 1–2 px, opcional.
4. **A altura sai dos canais que já existem** (R esteira, B cratera) num passo de compute, e não de um RT novo riscado pelo Canvas. O Canvas só soma: as fitas que se cruzam dobrariam o sulco. Assim o desgaste e o redesenho ficam coerentes de graça.
5. **Falta um nível intermediário.** O decal hoje troca o detalhe pelo grosso quando um pixel cobre mais de 2,5 texels, e no alto da tela o detalhe some. O grosso (50 cm/texel) nem desenha uma fita de 60 cm. Usar mips no detalhe ou um 2º nível de ~15–20 cm/texel.
6. **Sombra:**
   - o certo é uma **light function** no sol lendo uma máscara de sombra do caderno, que tira só a luz direta, com um corte por altura pra não escurecer o tanque em cima da esteira;
   - o passo da marcha é h_max / (tan(elevação do sol)·N);
   - pré-filtrar, senão cintila com o TSR.
7. **Custo:**
   - a sombra e a normal saem no compute, só nos blocos sujos, dilatados pelo raio do blur e pelo comprimento da sombra; o material lê 1–2 amostras;
   - medir p99 e máximo, não só a mediana;
   - o reposicionamento do caderno (até 262 mil fitas redesenhadas) e o desbote a cada 2 s dão soluços: o endereçamento toroidal resolve.
8. **O sol** vem da DirectionalLight: a receita do jogo é 30°, a arte 20°. Recalcular a sombra do caderno quando o sol mudar.
9. **Nanite pra esteira quase não ganha** de normal + sombra. Vale pra **cratera**, depois, com A/B medido.

## Divisão proposta (o Look decide; ele é o dono)

- **Look:**
  - a areia revolvida (cor, normal de detalhe e rugosidade, calibradas contra as 7 cenas favoritas do deserto);
  - como o material do chão mistura;
  - a medida do alvo;
  - a escolha entre light function e outra saída pra sombra.
- **WorldForge:** o gerador do material do chão (`build_slice.py`, `ground_material`) recebe a entrada nova.
- **Jogo:** o caderno (os canais, a altura com sinal e a berma no compute, a máscara de sombra, o nível intermediário), exposto pro material como texturas + parâmetros:
  - `GroundDetail` e `GroundCoarse`;
  - `DetailBounds` e `CoarseBounds` em cm (mín. X, mín. Y, largura, altura);
  - `Texels`.
  O decal de hoje sai quando o material do chão assumir.

## Feito (12/10): a parte do jogo

A divisão está combinada com o Look: o jogo faz onde e quando, o Look faz como fica.

**O cálculo:** `Game/Shaders/Private/GroundRelief.usf` + `RTSShaders/RTSGroundRelief.*`. São três passadas de compute, só no retângulo que mudou, mais o caderno inteiro a cada 2 s (o desbote):
1. a faixa da fita sem as garras;
2. o sulco e a bordinha fora da fita, amolecendo com a idade, mais do lado de onde vem o vento;
3. o sol, por um horizonte de 3 passos.

**Os cadernos novos** (`RTSGroundMemory`):
- o frescor, R8: a fita nova sobrescreve a velha;
- o relevo, R8G8B8A8 com escrita da placa:
  - o de detalhe: 2048² na DetailBounds, 9,4 cm/texel;
  - o do meio: 2048² sobre 384 m, 18,75 cm/texel.
- Os dois se redesenham da lista de marcas quando a câmera anda.

**Os canais do relevo:**
- R = altura: h_cm = (R − 0,5)·2·ReliefRangeCm, com ReliefRangeCm = 4;
- G = idade: 0 fresca, 1 velha;
- B = sol;
- A = revolvido.

**O que o material do Landscape recebe** (por `SetLandscapeMaterial*ParameterValue`, com estes nomes):
- texturas: `GroundRelief`, `GroundReliefMid` e `GroundTrack` (a esteira 4096², com R = fita com garras e B = cratera);
- vetores: `DetailBounds` e `MidBounds`, em cm (mín X, mín Y, largura, altura);
- escalares: `ReliefRangeCm` e `ReliefTexel`.

**O vento:** `wind_yaw` no Maps.json, em graus, 0 = +X, a direção pra onde sopra.

**Os números provisórios** (o `track_relief` da maquete do Look): o sulco de 2,2 cm e a bordinha de 2 cm a ~25 cm do meio da fita. Os finais vêm do Look, depois da escolha do Felipe entre A, B e C.

**A depuração:** `game.ps1 view -Scenario vila -Folder X -Extra "-RTSGroundReliefCycle"` passa altura → idade → sol → normal a cada 4 s.

**Medido:** o teste "vila_relevo1" deu mediana de 21,7 ms com o relevo, contra 22,0 do vila4 sem ele, na mesma cena. Não pesa na mediana.

## A cratera e o queimado de morte (13/10, plano)

Pedido do Felipe: "vai ter que machucar o terreno, com os detalhes bem detalhados e lindos, bem estudado, igual o
que o Look fez com as trilhas". **Alvo:** a cratera da `docs/concept/scenes/desert_golden_hour/best/street_battle_usa_china_gemini.webp`
(o recorte ampliado: buraco escuro com sombra dentro, borda levantada de areia clara e áspera, torrões e lascas
escuras espalhados em raio até ~2–3 raios, espirro de areia clara por cima do chão; pouca fuligem, só perto da carcaça).
Hoje sai uma mancha escura macia: a bacia lisa `(1 − r²)²` sem borda, as cores todas escuras no decal, a
opacidade da cratera no máximo 0,35 e nada que clareie.

**Divisão (combinada com o Look em 13/10):** o jogo diz onde e quanto (o caderno e os destroços), o Look diz como
fica (`MF_LookGroundMemory`, que já lê o formato abaixo). O decal de hoje é o piso até a MF entrar no chão do jogo.

### Como os jogos fazem (pesquisa de 13/10, com o código conferido)
- **Recoil/BAR:**
  - a marca é textura pintada, sorteada entre várias, com rotação aleatória;
  - a cor é `2 × marca × cor do chão` (0,5 não muda nada, abaixo escurece, acima clareia);
  - o calor fica no alfa da normal;
  - a cratera de verdade mexe no relevo (`BasicMapDamage.cpp`: bacia até 0,56R, borda +15% da profundidade em 0,7R);
  - o raio sai do dano, ±15%.
  - Código: [GroundDecalHandler.cpp](https://github.com/beyond-all-reason/RecoilEngine/blob/01efdf4/rts/Rendering/Env/Decals/GroundDecalHandler.cpp#L562-L700) e [decals_gl4.frag.glsl](https://github.com/beyond-all-reason/Beyond-All-Reason/blob/d7a5dd3/luaui/Shaders/decals_gl4.frag.glsl).
- **C&C ZH** ([W3DScorch.cpp](https://github.com/TheSuperHackers/GeneralsGameCode/blob/2d6761c/Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DScorch.cpp)):
  - 4 variantes sorteadas de uma textura;
  - na morte de tanque, o queimado tem o diâmetro do comprimento do tanque (`TerrainScorch Radius=15`);
  - os destroços são modelos voadores temporários.
- **FAF:** 10 texturas, rotação aleatória, 200–350 s de vida.
- **CoH e War Thunder:** a cratera deforma o chão de verdade.
- **Conclusão:** ninguém faz a marca no procedimento; é atlas de variantes + rotação + escala ± 15%, com a cor multiplicada pela do chão.

### A cratera real (pra ter os números, não chutar)
- **Diâmetro:** D ≈ 0,5–0,8·W^(1/3) (Kinney & Graham; Ambrosini et al. 2004). A profundidade fica em ~D/4.
- **Perfil** (Glasstone & Dolan §6.70–6.75):
  - a bacia é ~parabólica;
  - a crista da borda fica em 1,25·Ra, a +0,25 × a profundidade;
  - o ejetado é contínuo até ~2,15·Ra;
  - a espessura do ejetado cai com (r/R)⁻³ (McGetchin et al. 1973).
- **No tanque:** a munição e o combustível queimam dentro do casco, a ~0,5 m do chão, então a cratera real é pequena. O RTS exagera pra leitura.

### Os números (a leitura na câmera do jogo, o tanque de 7,9 m como régua)
- **Bacia:** Ra = 1,4 m (2,8 m de diâmetro, ~60 px no foco), com 60 cm de fundo.
- **Borda:** crista a 1,25·Ra com +15 cm, caindo com r⁻³ até 2,15·Ra.
- **Raios:** 8–14 riscos ralos de ejetado até 3–4·Ra.
- **Torrões:** de 2–5 texels (10–25 cm), mais densos perto da borda, viram altura positiva + ejetado; as lascas escuras viram pontinhos de fuligem.
- **Fuligem:** pouca, com a borda de 1–3 texels, até ~2,5 m do meio, em riscos; a mancha preta grande sai.
- **Direção:** o leque do ejetado e da fuligem vai pra longe de quem atirou, com a bacia 1,25× alongada.
- **Sorteio:** 6 variantes, rotação sorteada (as do tiro dirigido ±15° em volta da direção) e escala ±15%.
- `CraterRangeCm` = 64 (a bacia de 60 cm cabe; passo de 0,5 cm no 8 bits, com pontilhado de meio passo na gravação pra não fazer curva de nível).

### O formato no `GroundTrack` (RGBA8, 4.096² a 4,7 cm; também no caderno do mapa e no do meio)
- R: a esteira, como hoje.
- G: a fuligem, 0..1, com borda definida.
- B: a altura da cratera com sinal. 0,5 = zero, h = (B − 0,5)·2·CraterRangeCm. O caderno limpa com B = 0,5.
- A: o ejetado, 0..1.
- O `GroundRelief.B` (o sol) soma a altura da cratera. A marcha longa, até a profundidade ÷ tan(sol), em meio texel, só roda dentro da bacia.

### Como sai
1. **O atlas de variantes**, assado por script (`Game/Tools/bake_craters.py`, numpy): 6 × 256² a ~4,4 cm por texel (~11 m).
   - Canais: R = a cobertura (pra mistura), G = a fuligem, B = a altura com sinal, A = o ejetado.
   - O script também desenha uma prévia sombreada com o sol do jogo e o tanque de régua, lado a lado com o recorte do alvo. Assim dá pra medir e mostrar as opções antes da Unreal.
2. **Um compute por cratera** (o Canvas somado não escreve o A nem o sinal), no retângulo dela, em cada caderno:
   - a altura e o ejetado novos cobrem os velhos pela cobertura, porque a cratera nova enche a velha de ejetado;
   - a fuligem fica com o máximo;
   - no redesenho do caderno, as crateras vão na ordem do tempo.
3. **Destroços de verdade:** 4–8 peças por morte (roda, pedaço de lagarta, chapa torta, lascas), em instâncias.
   - O lugar sai de um sorteio fixo, pra longe do atirador, meio enterradas.
   - Ficam a partida toda, com teto de ~1.500; sai a mais velha, como as 500 marcas do ZH.
   - As pequenas não fazem sombra.
4. **O decal:** ele lê o formato novo como piso (normal da altura somada, bacia escurecendo, ejetado clareando de leve). A aparência final é a da MF do Look.

### O teste
- **A cena:** cenário "vila" com 3 crateras de depuração, mais uma foto no enquadramento da `street_battle`.
- **A prova:** a folha lado a lado com o recorte do alvo, em vídeo.
- **O custo:** medido com e sem (`-RTSNoGround`), p95 e máximo.

### O que mudou depois da revisão e da crítica do Look (13/10)
- **A morte do tanque não deixa bacia.** A carcaça fica (é cobertura) e cobre o miolo. No alvo, as bacias não ficam embaixo de carcaça.
  - Ela vira o **avental** (`wreck`): terra queimada e combustível 1,5–3 m além do casco, com lóbulos e línguas de combustível e a borda definida.
  - Tem ainda a brasa nos primeiros ~30 s, a areia espirrada pra fora e os torrões e lascas.
  - Fica orientado pelo casco.
- **A bacia** (`crater`) fica pras explosões sem carcaça em cima (o tiro no chão, o prédio, a artilharia que vier). Ela vem em camadas:
  1. a bacia de ~2,6 m e 50 cm, com lóbulos e alongada pro lado do tiro;
  2. o halo de terra revirada **escura** até 1,65·R;
  3. a borda quebrada com os torrões claros;
  4. os raios finos com falhas até 3·R;
  5. os torrões em lei de potência, a partir de 20 cm (menor que isso cintila na câmera).
- **A altura vai numa folha própria, R16F em cm** (`GroundCrater` e `GroundCraterMid`, na resolução do relevo). Os 8 bits com sinal faziam curva de nível na luz rasante, e o pontilhado virava granulado.
- **O `GroundTrack`** é R8G8B8A8 com escrita da placa:
  - R = esteira;
  - G = o escuro;
  - B = o calor (×0,88 a cada 2 s);
  - A = o claro.
- **A composição não depende da ordem** (a altura somada com limite, o resto pelo máximo), então o redesenho dá o mesmo que a gravação.
- **O atlas** sai do `Game/Tools/bake_craters.py`: 6 variantes de cada tipo, PNG de 16 bits em `Config/RTS/Ground/`, que já entra no pacote. Cada marca recebe o espelho, ±12% e o giro sorteados pelo lugar. No caderno grosso, a média do atlas cobre o texel.
- **O sol do relevo** soma a altura da cratera e marcha até a profundidade ÷ tan(sol) dentro da bacia.
- **A MF do Look:**
  - pinta como fica;
  - faz uma paralaxe curta só dentro da bacia, com 8–12 passos onde a `GroundCrater` < −2 cm (sem ela a bacia parece pintada quando a câmera gira).
- **Pendente:**
  - os destroços de verdade: sem colisão, seguindo a névoa, no tempo do pouso e fora da bacia;
  - a variante no asfalto, com placas levantadas e rachaduras radiais (falta o tipo de chão no mapa);
  - a idade (o vento amolece a borda, some com os torrões pequenos e enche a bacia);
  - afundar a unidade que passa na cratera;
  - gravar a cicatriz na névoa e mostrar quando a área for revista.

### Feito na noite de 13/10 (sem a aprovação do Felipe)
- **No jogo:** o avental de morte, a cratera e os destroços, com o atlas B. Testes `vila_cr1` e `vila_cr2` (`game.ps1 view -Scenario vila`).
  - A cratera de depuração lê como buraco de verdade: tem sombra dentro, borda e torrões, mesmo só com o decal de piso.
  - A carcaça tem a receita do Look medida na street_battle.
  - Corrigidos depois de ver: o alfa do caderno limpo com o "preto" da Unreal (A = 1 no mapa inteiro); o retângulo afundado em volta do casco; os torrões claros em anel; a carcaça quadriculada (ruído do asfalto a 0,4 m repetindo); os destroços claros.
- **O chão do mapa:** em 13/10 o WorldForge ligou a `MF_LookGroundMemory` no material do World_TestSalt2, e a cor final passou a ser a do Look. Até funcionar foram cinco tropeços:
  1. O vetor dos limites entrava como RGB e o `.zw` não compilava no SM6: o chão ficou xadrez.
  2. Cada regravação da MF recria as entradas, e o material salvo perdia as ligações (`Missing function input 'BaseColor'`). Hoje o WorldForge embute o CODE do `look_mf.py` direto no material.
  3. `SetLandscapeMaterial*ParameterValue` só funciona com `bUseDynamicMaterialInstance`. O jogo liga isso na hora e cria as instâncias de cada pedaço, também na malha Nanite (256 pedaços, 256 casas).
  4. O escuro, o calor e o claro só vinham do caderno de detalhe com o pixel do chão abaixo de ~9 cm. Na câmera do RTS o queimado sumia. A MF agora lê também o caderno do meio (`GroundTrackMid`); quando ele existe, o decal de piso sai sozinho.
  5. A brasa a 40 000 nits estourava em manchas amarelo-claras. Ficou em 3000 nits, o pico do miolo da carcaça.
  6. A MF media o pixel por `length(fwidth(p))`, ~1,6× o tamanho real com a câmera girada a 45°. Na câmera do RTS ela nunca usava o caderno de detalhe, e a cratera virava borrão. Agora mede o maior eixo do pixel (~8 cm na câmera padrão) e usa o detalhe até 10–16 cm. O vídeo `cratera_pan1` (a câmera deslizando a 4 m/s) não cintila: o TSR resolve, sem mips.
  7. O meu atlas, no tom novo do Look (o escuro a 0,14× da areia), virou pele de onça. Os ajustes:
     - o escuro do avental e do halo ficou contínuo;
     - a bacia é carvão até perto da borda;
     - os torrões estão mais baixos e mais tortos;
     - a brasa virou pontos esparsos, porque em manchas largas lia como buraco de areia.
  8. A crítica do Look contra a street_battle, feita no gerador:
     - a cratera lia como poço redondo e ganhou contorno em 2–3 lobos (±25%) com borda levantada só em ~60% da volta;
     - ganhou 8–14 riscos de fuligem do centro até 2,5–4 raios, mais densos pro lado do tiro;
     - os torrões ficaram angulosos, fora do anel, com 3–5 grandes e claros (a areia de baixo);
     - o avental ganhou borda esfarrapada, ~30% de areia voltando em manchas largas e riscos curtos saindo do casco.
     A rodada é a `vila_mf13`.
  9. A 2ª crítica do Look (mf13 ampliado):
     - os riscos pareciam pente: ficaram 2× mais largos, com a borda esfumada e o comprimento variado (os curtos mais grossos), ~35% em toda a volta;
     - o miolo era uma cor só: ganhou núcleo quase preto e torto, manchas de carvão de ~0,5 m e 4–8 pedaços claros caídos dentro;
     - o avental: dentes de 1–3 m na borda, manchas de areia de 1–2 m e mais destroços escuros até ~1,5× o avental. A areia a ~90% (mf14) apagava o queimado na cena inteira; ficou em ~65%.
     A rodada final é a `vila_mf15`.
  10. **13/10 de manhã:** o Felipe aprovou a prova da cratera na maquete do Look ("já ficou bom pra kct"; `captures/look/cratera_texturas_v6.jpg`). Ela foi trazida pro `bake_craters.py` (`crater_proof`):
     - a bacia com parede em pé, h = −d·(1 − (1 − t)^1,6);
     - o escuro só num núcleo torto, com manchas de carvão;
     - o manto de ejeção mosqueado até ~2,6 R e 14 riscos finos de fuligem;
     - nenhum claro pintado onde vai pedra;
     - o relevo fino da terra revirada.
     - **Os torrões viram pedras escaneadas 3D** no mesmo lugar: a lista vai no JSON do atlas, e o campo de destroços põe as `namaqualand_stones_01` com o material escuro da cratera do Look, pelo mesmo giro, espelho e escala do carimbo, ~45 por cratera de 20 cm pra cima.
     - **A resolução:** a 2,35 cm (sem a prova) não deu ganho nenhum na tela (0,7/255 de diferença), porque o limite é o pixel da câmera e o caderno de 4,7 cm. A prova vai a 2 cm só pra bater com a da maquete e pras rampas.
     - **As rampas** (o G e o A em distância, a MF corta a borda com fwidth): ligadas pelo JSON do atlas (`"encoding": "ramp"`); o jogo manda `MarkRamps`. O queimado sem cratera (o chamuscado do tiro, a pegada do prédio) passou do Canvas pro mesmo carimbo, com o atlas "mancha".
     - **As três crateras por arma** (o Felipe, 13/10): A a 0,75× é o tiro de tanque, B a 1× é o prédio que cai, C a 1,3× é o tanque explodindo e o míssil pesado.
  - Rodadas: `vila_mf5` (o xadrez sumiu), `vila_mf6` (a bacia aparece), `vila_mf8` (o fundo da cratera escuro), `vila_mf15` (o estado final da noite). A folha contra a street_battle está em `captures/game/folha_noite_alvo_vs_jogo.png`.
- **A névoa:** a morte fora da vista guarda o avental e os destroços, que aparecem quando a área é vista.
- **O custo (medida limpa, a mesma cena da vila):** com tudo 23,9 ms de mediana, sem a memória do chão 23,4 ms, e com o decal de piso no lugar da MF 23,8 ms. O chão inteiro custa **~0,5 ms**.
- **Pendente:**
  - a escolha do Felipe entre as opções A/B/C (`captures/game/crateras/alvo_vs_*.png`);
  - a paralaxe curta na bacia (Look);
  - a variante no asfalto (falta o tipo de chão);
  - a idade das marcas;
  - afundar a unidade que passa na cratera.

## A cratera v2: cada chão com o seu impacto (13/10, plano do Look)

**O pedido do Felipe**, depois do mf15: "a cratera melhorou, mas está em baixa qualidade de detalhe". Ele quer
"bem estudado, bem inspirado, com a comparação final rígida", e o impacto diferente em cada chão: areia e estrada
agora, grama depois. Pra areia, o exemplo é a cratera da `street_battle_usa_china`.

**As referências:** a sessão de arte recortou as 48 imagens aprovadas por chão (`captures/concept/crater_refs/`).
Os prompts pra gerar o que falta (asfalto fresco, grama) estão no `PROMPTS.md` de lá, com o sol do jogo. A regra:
- a areia escorre (borda redonda);
- a terra lasca (torrões);
- o asfalto quebra em placas (bordas retas levantadas);
- a grama rasga e mostra a terra;
- a fuligem preta grande só existe com veículo queimando.

**O estudo, com fontes:**
- **Battlefield 3** (GDC 2012, "Terrain in Battlefield 3"):
  - a cratera muda a altura do chão;
  - a profundidade e a largura variam por material físico;
  - a cor é montada na textura virtual a ~3 cm/texel.
- **CoH2:** altura pintada mais decals de cratera, queimado e entulho.
- **Arma e DayZ:** o efeito do impacto vem do material do chão.
- **Real:**
  - na areia seca, a bacia é mole, com poucos torrões; no solo coeso, torrões grandes perto da borda (DTIC AD0472539, AD0639146);
  - no asfalto, o pavimento levanta em volta da cratera ("upheaval", TSPWG M 3-270-01), e a área quebrada é bem maior que o buraco (MDPI 2022, 12(6) 2888).
- **Unreal 5.8:**
  - a tessellation do Nanite pesa em placa fraca e tem defeitos abertos em Landscape (UE-370671);
  - o POM é caro;
  - o cache de sombra fica velho se o chão mudar só na textura.

**A segunda opinião** (o diagnóstico que manda): o "baixa qualidade" é mais nitidez e valor do que falta de textura.
- O atlas tem 4,4 cm, é carimbado girado num caderno de 4,7 cm e lido de novo em bilinear: ~9 cm efetivos.
- O claro só sobe 13%.

**Medido** (a folha `captures/game/folha_noite_alvo_vs_jogo.png`, brilho na tela, a mesma escala):

| | alvo | jogo (mf15) |
|---|---|---|
| p5 (o miolo) | 0,09 | 0,23 |
| p25 (o halo) | 0,27 | 0,45 |
| p50 | 0,36 | 0,56 |
| nitidez (laplaciano) | 1,08 | 0,80 |

No alvo, o anel da borda não é mais claro que o chão em volta (0,36 contra 0,46 do asfalto e 0,61 da areia). O que
lê é o miolo bem escuro, o halo meio-escuro e os torrões soltos claros por cima.

**O plano, na ordem:**
1. **Nitidez** (o jogo):
   - o atlas a ~2 cm/texel;
   - G e A como rampas de distância, cortadas no shader com antisserrilhado.
2. **Valor** (o Look, na MF):
   - o miolo e a bacia mais escuros (alvo p5 ~0,09);
   - o halo maior e meio-escuro (p25 ~0,27);
   - os torrões claros por lerp até um albedo-alvo.
3. **Cada chão** (o Look na MF, o WorldForge liga a entrada `Surf` com o peso da estrada):
   - a mesma cratera, interpretada pelo chão;
   - na areia: o claro é areia e a borda é redonda;
   - no asfalto: o claro é cascalho cinza, o escuro é placa preta, e a bacia é mais rasa.
4. **Sombra e AO da bacia na hora do carimbo** (o jogo; o sol é fixo por mapa): custo por quadro ~0.
5. **Debris 3D só nas peças grandes** (≥ 40 cm), nos torrões do JSON do atlas (o jogo).
6. **Textura escaneada de detalhe** só pro zoom perto (depois). **O Nanite fica fora**, no máximo como medição.

**O teste:** no jogo, na câmera do jogo (a maquete não tem TSR nem a reamostragem do caderno), contra a
street_battle na mesma escala. Medir os três tons, a nitidez da borda, a estabilidade numa panorâmica lenta
(diferença entre quadros) e o custo em ms.

### A tarde de 13/10: as pedras, o tamanho e a bacia de verdade (sem a aprovação do Felipe)
- **As pedras da cratera** (o pedido do Look):
  - no máximo 14 graúdas (≥ 20 cm) por cratera, num sorteio fixo do bake (`export_clods`);
  - deitadas pela face maior, com altura de 0,5–0,7 da maior dimensão, ±8° de inclinação e um quarto enterrado;
  - as miúdas (8–20 cm, ~150 por cratera) num componente à parte, sem sombra, que some de 70 a 80 m: só aparecem no zoom mais perto (a câmera fica a 67–118 m).
  - O teste: `cratera_perto3`.
- **O tamanho A/B/C não valia:** a linha do sorteio sobrescrevia a escala do tamanho. Agora valem A × 0,75 e C × 1,3.
- **A sombra da bacia:** o SunCS já roda no retângulo da marca na hora do carimbo (o `UpdateRelief` logo depois do `StampMarks`).
- **A bacia de verdade (o Felipe: "igual à v6"):** a v6 é uma malha a 2,5 cm, e o Landscape tem 50 cm por vértice.
  - **Descartada:** a malha pré-fabricada com buraco no chão. No fonte da 5.8:
    - o `ULandscapeNaniteComponent` força `bEvaluateWorldPositionOffset = false`, então não dá pra afundar o chão embaixo da malha;
    - o buraco por máscara ou PDO deixa o chão inteiro no raster programável.
  - **Escolhido com o Look:** o deslocamento Nanite do próprio Landscape (bEnableTessellation e o pino Displacement), lendo a cratera em cm, o sulco e a altura da Farm Soil (o `DISP_CODE` do look_mf.py).
    - O jogo liga o `DispOn` no Alta e no Ultra (`SetDisplacement`; `-RTSDisp=0/1` força, pro A/B).
    - Nos presets menores, o DefaultScalability.ini desliga a tesselação inteira (`r.Nanite.Tessellation 0` no ShadingQuality 0–2), porque ela custa mesmo com o deslocamento zerado.
  - **O tropeço:** com o deslocamento, o chão do TestSalt2 perdeu triângulos inteiros, mesmo com DispOn 0. O WorldForge reverteu.
    - Os suspeitos, do fonte: os limites de patch da tesselação (`r.Nanite.MaxCandidatePatches` e `MaxVisiblePatches`, 2M cada), a reserva de raster por cluster com o Displacement Fade (LandscapeRender.cpp:4926) e o Nanite do chão construído antes da troca do material.
    - O plano B, se for bug do motor: a bacia por sombra e normal, e a geometria só nas pedras.
- **O tipo de chão** (pra poeira da Arte e pro sulco):
  - o WorldForge exporta o `surface.png` (R asfalto, G terra batida, B rocha, a 1 m) e o `game.ps1 worlds` copia pra `Config/RTS/Surface/`;
  - o mapa assado lê (`FRTSBakedMap::SurfaceAt`).
