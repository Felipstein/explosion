# A mira dos poderes do general: o cursor e a prévia no chão (estudo, 05/11/2026)

## Estado (06/11): a mira projetada no chão, APROVADA ("agora sim, aprovado"; o vídeo `captures/game/mira_poderes_v6`)

**Por quê:** a v1 (05/11) era desenhada no Canvas do HUD: linhas finas por cima de tudo, por cima dos tanques e da usina, apagadas
na areia e sem a luz do chão. O Felipe, 06/11: "os cursores dos poderes parece que é um png em cima do chão [...] se vc ver que
realmente da pra melhorar, melhore". O vídeo `captures/game/mira_poderes_v2` confirmou.

**Como os RTS grandes fazem (06/11):** a área do poder é uma marca projetada no chão, do tamanho de verdade do poder: o "splat" do
StarCraft II ([SC2Mapster](https://sc2mapster.gamepedia.com/Data/Actors/Splat)), o decalque do 0 A.D. projetado de cima pra baixo
no relevo ([Pyrogenesis](https://docs.wildfiregames.com/pyrogenesis/structSDecal.html)), o `RadiusDecal` do ZH (só no terreno).
O jeito de hoje é desenhar no shader, pela profundidade da cena
([jMonkey](https://hub.jmonkeyengine.org/t/drwaing-on-terrain/15172); a varredura do Death Stranding pelo ponto do mundo da
profundidade, [realtimevfx](https://realtimevfx.com/t/found-an-easy-way-to-create-a-terrain-scanner-in-unity/23780)).

**Como está feito:**
- Um pós-processo **antes do TSR** (`Scene Color After DOF`, na resolução de desenho), `/Game/UI/M_PowerPreview`, gerado por
  `Game/Tools/make_power_preview.py` (o encanamento do clarão da seleção). Antes do bloom (depois do TSR, o v5) a linha tremia: a
  profundidade lida lá é a do quadro com o deslocamento de subpixel do TSR, e a linha pulava 1 px pra lá e pra cá com a câmera
  parada (medido no vídeo `mira_poderes_v5`; o Felipe: "pq ta tremelicando?"). Antes do TSR, ele trata a mira como o resto do chão:
  no v6 a linha mexe 0,02 px por quadro (era 0,75 px), medida pelo centro da linha numa fileira de pixels, quadro a quadro. Ele reconstrói o ponto do mundo de cada pixel pela profundidade e desenha as 6 formas por distância (SDF).
  - **Colada no chão:** no relevo, nas pedras e nos destroços.
  - **Por baixo dos vivos:** as unidades e os prédios marcam o bit 128 do stencil (a máscara da poeira), e ali a mira não pinta;
    eles ficam em pé na marca.
  - **A área: o miolo limpo e a cor na borda** (o jeito do StarCraft II e do AoE4): 3,5% no meio e, por dentro da linha, a
    faixa da cor do tipo acesa (a luz do chão × 1,15) com **12 px de largura na tela** (a 900 de altura; a curva ^2, até 64%),
    pela distância com sinal de cada forma: lê como o brilho da própria linha, igual em qualquer tamanho.
    - Reprovado por mim: a faixa como fração do raio (v4: os últimos 18%, ^2,5; v5: 12%, ^3). Num círculo grande ela tinha 30 px
      de mistura de cor: a do golpe lia como uma tira de tinta salmão e a do conserto deixava uma franja rosada por dentro. A cor da areia sai antes
    (fica cinza na mesma luz) e só então entra a do tipo. A linha clara é a luz do chão × 2,4 (+ um piso pra sombra), com o
    contorno escuro (−62%) dos dois lados.
    - Reprovado por mim (v3, 06/11): a área inteira com o matiz do tipo na MESMA luminância (10% no meio, 60% na borda): o azul do
      conserto por cima da areia laranja virou um lilás acinzentado (cor oposta misturada pela metade dá cinza).
    - Reprovado por mim (v4, 06/11): a faixa da borda direto da areia pro azul: no meio da faixa passava por rosa-lilás (o vermelho
      da areia + o azul); com o cinza no meio do caminho, vai de areia a cinza-aço a azul.
  - **Só no chão:** a face em pé (a normal com z < 0,3) e o que está 4 a 7,5 m acima do plano do chão em volta (a copa da palmeira,
    o telhado) não recebem a área; a linha sobe a pedra a 40%.
  - **A linha:** 2,6 px de largura (a 900 de altura) e o contorno 5,2 px, pela derivada do ponto do mundo, presa a 0,4–2,5× a escala
    do centro (na quina de uma pedra a derivada explode); a borda suave.
  - **Viva:** o golpe com o pulso FECHANDO no alvo (1,4 s) e os 4 dentes girando por fora; o apoio com a onda ABRINDO (1,8 s); os
    olhos com a varredura do radar (o braço e o rastro leve, 20%; a v3 era um setor branco chapado); as marcas de grau a cada 15°
    por dentro da borda do golpe; o espalhamento tracejado girando; as setas da faixa correndo no sentido do voo (9 m/s) e a
    fileira de impactos; a queda com o anel fechando em cada ponto (1,2 s, cada um defasado); o alvo único com a trava fechando em
    0,25 s; a órbita girando; quem o poder pega com o anel debaixo e o chão aceso por dentro. A área respira ±12% em 1,6 s.
  - **O inválido:** cinza, tracejado e hachurado (listras de 1,4 m); o motivo e o risco no mouse continuam no Canvas.
- O C++ (`RTSPowerPreview.cpp`, `Upload`) passa os números da forma a cada quadro (o centro, o raio, a direção, os 24 pontos do
  traçado, os 10 da queda, os 16 afetados, o plano do chão, a escala). Sem mira, o peso do pós-processo vai a 0 e ele não roda.
- A interface (`FSpec`, `Show`, `Hide`) ficou igual: a sessão dos poderes e a das superarmas não mudam nada.
- **O custo na placa (GTX 1660, 1600×900, Ultra; o ProfileGPU pela cena `mira_custo`):** só enquanto mira (sem mira o peso do
  pós-processo é 0 e ele não roda). A 1ª medida (`captures/game/mira_custo_v1`): 0,15 ms na maioria das formas e 1,69 ms numa delas
  (4% do quadro de ~39 ms): os laços do traçado, da queda e dos afetados liam as listas com o índice variável (a placa joga isso pra
  memória lenta). Desenrolados (`[unroll]`, o índice fixo em cada volta), a 2ª medida, forma por forma (`captures/game/mira_custo_v2`,
  o ProfileGPU a cada 2,5 s com a foto de cada um): **0,15 ms** em todas as formas (o círculo, a faixa, a queda, o alvo, a órbita, os
  olhos) e **0,32 a 0,47 ms** no traçado livre da muralha (a distância aos 23 trechos em cada pixel), menos de 1,2% do quadro.
- **Teste:** `game.ps1 view -Folder <pasta> -Scenario mira_poderes -Extra '-RTSPowerPreviewAt=-262,-326' -Record` (as 6 formas,
  2,5 s cada, em 3 voltas: a válida; a válida com o apoio acendendo os 3 tanques dentro e os olhos no lugar da órbita; a inválida).
  **O fluxo de verdade, provado (06/11, `captures/game/mira_conserto_v3`):** a cena `mira_conserto_cursores` mira o Emergency Repair
  pelo botão (o op `aim_power`, o `BeginTargeting` de verdade) no grupo (o círculo de 26 m e o anel debaixo dos 4 tanques; o de fora
  sem anel), depois 13 m além do tanque de fora (só ele com o anel) e usa. A imagem:
  `docs/aprovados/ui/2026-11-06_mira_conserto_pelo_fluxo_de_verdade.jpg`. A `mira_conserto` da sessão dos poderes caía na recarga
  ("Ready in 0:03") e a 2ª mira ficava fora do quadro; a minha mira aos 126 s e 130 s, com a câmera a 95 m pegando os dois.
- A v1 no Canvas (reprovada por mim em 06/11, pelo pedido dele): o código inteiro em `docs/poderes/mira_v1_canvas.cpp` (as linhas
  amostradas a cada 1 m e projetadas na tela, o véu em faixas, as setas simétricas na tela).

## O estudo (05/11)

Pedido da sessão dos poderes (04/11, a pedido dele): o cursor e a prévia no chão de cada forma de mira; a sessão dos poderes liga
na mecânica ("o que a prévia mostra = o que a simulação faz"). Os ~65 poderes das 4 árvores cabem em 6 formas
(`docs/poderes/IMPLEMENTACAO.md` §2.3). Ele delegou a aprovação (04/11, "aprove tudo sem mim, depois eu revejo").

## 1. Como os RTS fazem

- **Zero Hour** (pelo código: `InGameUI.cpp`, `RadiusDecal.cpp`):
  - cada poder tem o seu **desenho no chão** (`RadiusCursor`: `A10StrikeRadiusCursor`, `DaisyCutterRadiusCursor`,
    `ParadropRadiusCursor`, `SpectreGunshipRadiusCursor`, `EmergencyRepairRadiusCursor`, ~30 no total);
  - é uma textura de decalque que deita no relevo, do **raio de verdade** do poder, com a **opacidade pulsando** (`OpacityThrobTime`,
    a guarda de 50 a 100% em 1 s) e a cor do dono; só o dono vê;
  - o cursor do mouse só marca o centro (a mira de golpe ou a de apoio). Todos os golpes do ZH são círculos, até o A-10 e o
    Carpet Bomb: a direção vem de onde o avião entra, e o jogador não escolhe.
- **Company of Heroes 2 e 3:**
  - o golpe de área mostra o **círculo e o espalhamento** (o raio onde cai cada granada);
  - os rasantes (P-47, IL-2, o bombardeio em linha) são **clicar e arrastar a direção**: a faixa no chão do tamanho da passada,
    com a seta de onde o avião vem;
  - as linhas de defesa (arame, sacos de areia, as armadilhas de tanque) são **desenhadas arrastando**, com o custo crescendo.
- **World in Conflict:** os apoios táticos mostram a área no chão e o carpet bomb gira arrastando. A prévia é grande, limpa e
  some no clique; o aviso do golpe que vem fica pros dois lados.
- **StarCraft II:** o círculo exato do efeito; verde vale, vermelho não vale (fora do alcance); a borda nítida e o miolo quase
  transparente, pra não esconder as unidades embaixo.
- **Age of Empires IV, Tempest Rising:** o anel fino no chão, que acompanha o relevo; o miolo leve.

**O que todos têm em comum:**
1. A área vai no chão, do tamanho exato; o cursor só marca o centro.
2. A borda é nítida e o miolo quase não pinta (as unidades embaixo continuam legíveis).
3. A direção, quando o jogador escolhe, é clicar e arrastar.
4. O inválido diz que não vale (a cor e a forma) e por quê.

## 2. A nossa regra (a língua dos cursores, `docs/CURSORES.md` §3)

- A borda escura por fora e a linha clara por dentro (o contraste no deserto e na sombra), os traços lisos do `RTSSoftDraw`.
- **A cor pelo tipo do ícone** (`ICONES.md`): golpe quente (o vermelho do atacar), apoio frio (o azul), tropas o céu (o ciano claro),
  olhos a térmica (o branco).
- **O miolo:** um véu de 10 a 15% com as bordas mais fortes, nunca chapado (o que ele reprovou no raio-X).
- **O movimento:** a área respira devagar (o pulso do ZH, mais calmo: ±12% em 1,6 s); a borda gira devagar nos golpes; nada pisca.
- **O inválido:** a área em cinza com o traço cortado e o motivo em uma palavra no cursor ("out of range", "fog", "no Command Center").
- **O que a simulação faz aparece:** a fileira do Naval que anda, os pontos de queda das tropas, o eixo da passada do avião, a
  elipse do espalhamento.

## 3. As 6 formas

1. **Círculo num ponto:** o anel no raio do poder, o véu, o centro marcado. O espalhamento (onde as granadas podem cair) é um 2º anel
   tracejado, e a elipse do Naval III é o mesmo anel achatado.
2. **Faixa com direção:** clicar o início e arrastar. Desenha o retângulo da passada (o comprimento e a largura de verdade), as setas
   de onde o avião vem, e as fileiras do Naval I–II (o II com 3 lado a lado).
3. **Traçado livre:** a linha da Fortified Line seguindo o mouse, com a régua do que falta dos 120 m.
4. **Ponto de queda:** o anel do espalhamento e um ponto pra cada tropa que cai (Paradrop I–III, VDV, SEAL); o FOB mostra a planta
   da base como o fantasma de construir.
5. **Alvo único:** os cantos do C&C fecham no prédio ou na unidade (os mesmos do cursor de selecionar) na cor do tipo; fora de um
   alvo válido, o cursor de proibido.
6. **A órbita (AC-130):** o anel grande da órbita, fixo enquanto o poder dura, e a retícula de tiro no mouse dentro dela.

## 4. Como entra no jogo

- **Desenho:** pelo Canvas do HUD, projetado no chão e amostrado na altura do relevo (como o ponto de encontro e a marca da ordem,
  já aprovados). Custa quase nada e não depende de Nanite nem dos presets.
- **A chamada pra sessão dos poderes:** `RTSPowerPreview::Show(Shape, Center, Size, Dir, Kind, bValid, Reason)` a cada quadro em
  que a mira está ativa, e `Hide()`. Os números vêm da mecânica do poder, nunca da prévia.
