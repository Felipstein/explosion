# Pendências da noite (30/10/2026): a lista de tudo

O pedido dele ao dormir: "ler toda a conversa de novo, separar cada demanda e fazer do JEITO CERTO, nem que leve a madrugada
toda, nem que precise refazer algo do 0 [...] quando eu acordar quero tudo finalizado e resolvido, ZERO pendências [...] cada
item tem que ser bem feito". Ele deu permissão pra tudo, inclusive baixar assets.

**O jeito certo, em cada item** (as regras que ele repetiu: memórias "Sair da roda", "Leitura da batalha", "Aceito não é
aprovado", "Atmosfera, não exposição", "Fenômeno real primeiro", "Estudar os RTS antes"):
1. **Estudo antes:** o fenômeno real, como o ZH faz (pelo código), como os AAA fazem.
2. **Plano com números de origem dita:** medida, fonte ou proporção, nunca chute.
3. **Código.**
4. **Prova em vídeo no jogo,** no zoom do jogo e ampliado, A/B com a mesma história, lado a lado com o alvo.
5. **Eu comparo antes de mostrar.** Duas voltas sem chegar perto = outra abordagem.

**Os estados:** `[ ]` a fazer · `[~]` fazendo · `[x]` feito e provado (com o vídeo) · `[!]` não deu, com o motivo · `[?]` decisão sua (o trabalho que era meu está feito; falta você escolher ou aprovar).

**Depois da revisão dele (30/10 de manhã): os itens 20 a 39 da seção D estão abertos, todos meus**, menos o 39 (o EmberGen, que
ele opera e eu guio) e o commit (19), que espera o "aprovado".

Os detalhes de cada item estão nos estudos:
- `docs/ESTUDO_IMPACTOS.md` (seções G a L);
- `docs/ESTUDO_ACERTO_TANQUE.md`;
- `docs/COMPARACAO_GAMESHOTS.md` (a lista do que falta, itens 0 a 7).

---

## Pra avaliar de manhã: um vídeo por item, nessa ordem

Os vídeos estão em `captures/game/escala_v5/`. Nada foi commitado.

| # | O quê | O vídeo | O que mudou pra você ver |
|---|---|---|---|
| 1 | O desgaste das construções | `desgaste_ab.mp4` | Menos juntas e escorridos; a cor igual |
| 2 | A atmosfera | `atmosfera_ab.mp4` | O véu quente no fundo, a sombra azulada, o tanque menos amarelo |
| 4 | A metralhadora | `metralhadora_ab.mp4` (perto) e `metralhadora_zoom_jogo.mp4` | A rajada costura a areia, ~30% bate no casco, o traçante 1 em 4, o pulso de luz em quem atira |
| 5–6 | O tiro de canhão no casco e o ricochete | `tiro_tanque_ricochete_lento.mp4` (metade da velocidade) | Sem a bola bege; o estalo, o leque, a chama e a fumaça escura do furo; o ricochete com o jorro raso e o traçante indo embora |
| 7 | O tiro no prédio | `predio_ab.mp4` | O acerto nas paredes (não mais dentro do portão), o reboco e o pó |
| 8 | O recuo | `recuo_ab_lento.mp4` (1/4 da velocidade) | O cano entra na torre, a torre fica |
| 9–10 | As mortes e a carcaça queimando | `fogo_carcacas_ab2.mp4` | A fumaça preta e a chama em cada carcaça; a gasolina mais clara, o diesel mais preto, a munição estourando |
| 9 | Os pedaços das mortes | `mortes_pedacos_ab_lento.mp4` (metade da velocidade) e `mortes_pedacos_zoom.jpg` | Somem as placas pretas grandes; pedaços grossos e menores, cor de aço queimado. No zoom do jogo a diferença é pequena |
| 15 | A obra | `obra_solda_ab.mp4` | A solda trabalhando: rajadas de estalos num ponto, a pausa, outro ponto |
| 15 | O Dozer trabalhando | `dozer_terra_lento.mp4` (metade da velocidade) e `dozer_terra_zoom_jogo.mp4` | A terra escorrendo pela borda da lâmina a cada subida dela (o jeito do ZH) |
| 7, 12 | O tiro no prédio | `predio_tiro_ab.mp4` e `predio_marcas_perto.jpg` | O tiro bate na parede de verdade (antes, no ar na frente dela) e deixa a marca de fuligem com a lasca clara |
| 12 | O prédio danificado e caindo | `predio_dano_fogo_entulho.mp4` e `predio_entulho_perto.jpg` | Com 70%: a baforada de fumaça do telhado. Com 30%: o estouro e o fogo no telhado (o fogo das carcaças, volumétrico). Consertado, apaga. Morto: desaba e deixa o entulho de concreto |
| 2 | A sombra da palmeira | `palmeira_sombra.jpg` | Conferida: a sombra longa e fina do sol baixo, certa pela física |
| 0 | Tudo junto, a leitura da batalha | `batalha_antes_depois.mp4` | O alvo, ontem e agora, na câmera do game shot |
| 16 | A infantaria | `quartel_hud.jpg` (o quartel selecionado com o botão do Ranger e a fila), `infantaria_combate.mp4`, `infantaria_atropelamento_lento.mp4` (metade da velocidade), `quartel_produzindo.mp4` e `infantaria_perto.jpg` | O Ranger e o quartel: o tiroteio com as Technicals, o tanque que passa por cima, o quartel produzindo. O estudo: `docs/ESTUDO_INFANTARIA.md` |
| 11 | O chão (o teste do recorte) | `chao_pedras_ab.jpg` e `chao_pedras_ab_perto.jpg` | As pedras soltas pela regra física quase não mudam o gs02: a diferença é o tipo de mapa (o deserto pedregoso), do WorldForge |
| 0 | Tudo junto, no fim da noite | `batalha_noite3.mp4` | A mesma batalha do game shot com a 3ª volta inteira (a prova de que nada quebrou) |

---

## A. As que ele mandou fazer

1. `[x]` **O desgaste das construções amenizado.** Feito: `captures/game/escala_v5/desgaste_ab.mp4`, menos juntas e escorridos.
   - **O pedido:** "essa textura da construção toda fodida tá horrível, dá uma amenizada nisso".
   - **O plano:** o desgaste no meio do caminho entre o de antes da v5 e o de hoje (`Weather` 1,0 → 0,5, sujeira 0,35 → 0,25 em
     `import_provisional.py`; a edição já está escrita).
   - **A prova:** A/B em vídeo na cena `fx_predio`.
2. `[x]` **A atmosfera, a cor da sombra e o tanque amarelo** (feito, `captures/game/escala_v5/atmosfera_ab.mp4`; medidas e o que não saiu em `COMPARACAO_GAMESHOTS.md` item 6; a sombra da palmeira conferida: `palmeira_sombra.jpg`) (a crítica do profissional; `COMPARACAO_GAMESHOTS.md` item 6):
   - o degradê laranja da névoa do lado do sol, indo pro amarelo da areia;
   - os raios do sol na poeira, no canto da tela;
   - a sombra azulada (hoje cinza morto);
   - o tanque menos amarelo (cinza ou cinza-azulado);
   - a areia menos forte, que é gosto (medir contra o alvo);
   - a sombra da árvore errada. **Conferida** (cena `fx_palmeira`, a 40 m e no zoom do jogo a 88 m): o tronco e a copa fazem a
     sombra longa e fina que o sol de 18–24° dá (uma palmeira de ~10 m faz ~25 m de sombra). Está certa pela física. O "errado"
     do profissional deve ser isso: a sombra da copa cai longe do pé, parece solta da árvore. Não mexi.
   - Sem cobrir as unidades.
3. `[x]` **O estudo da leitura da batalha** (feito: `docs/LINGUAGEM_DO_COMBATE.md`, com o `Weapon.ini` e o `FXList.ini` do ZH) (o item 0, o mais grave):
   - **A frase:** "não dá pra entender porra nenhuma de quem tá atirando em quê, onde acertou [...] o próprio C&C ZH dá um pau
     na gente".
   - **O que estudar:** o ZH pelo código e pelos INI liberados (as FXList das armas), os AAA, e o nosso medido.
   - **O que sai:** a linguagem do combate, que guia os itens 4 a 7.
4. `[x]` **A metralhadora pelo fenômeno real** (feito: `metralhadora_ab.mp4` de perto e `metralhadora_zoom_jogo.mp4` no zoom do jogo; o que entrou em `ESTUDO_IMPACTOS.md` M) (`ESTUDO_IMPACTOS.md` G):
   - a rajada desenhada;
   - a maioria das balas erra e costura a areia até o alvo;
   - o esguicho claro e o véu no campo único de poeira;
   - o traçante fino, 1 em 4;
   - o casco com faíscas finas, sem estrela; na Technical, lascas e vidro;
   - um ator só pras balas (o custo).
5. `[x]` **O tiro de tanque no casco: a receita em camadas** (feito: `tiro_tanque_ricochete_lento.mp4`; sem a bola bege) (`ESTUDO_IMPACTOS.md` H, `ESTUDO_ACERTO_TANQUE.md` §6):
   - penetrou, travou, ricocheteou e acertou o chão perto, cada um com a sua cara;
   - a flecha sem bola de fogo;
   - a fumaça que sai do furo e a marca no casco.
6. `[x]` **O ricochete** (feito: simulação 25, o teste da simulação TUDO CERTO; o jorro raso e o traçante que sai) (decidido: 25% do dano, só entre tanques, a chance pelo ângulo na face). Na SimCore, em ponto fixo, e o
   efeito que se entende na tela.
7. `[x]` **O tiro de tanque no prédio** (feito: `predio_ab.mp4`; o acerto nas paredes e colunas, o reboco e o pó. Melhor, mas ainda não arrepia: falta a marca que fica na fachada e os estados de dano do prédio) (`ESTUDO_IMPACTOS.md` L; reprovado: "muito mal feito"):
   - o acerto na parede virada pro tanque, não dentro da porta;
   - o pó de concreto de 5–8 m que fica 8–15 s;
   - o reboco descendo pela fachada, os pedaços e a marca que fica.
8. `[x]` **O recuo** (feito: o cano entra 55 cm na torre e volta em ~0,5 s, a torre só estremece, o tranco do casco ×2; `captures/game/escala_v5/recuo_ab_lento.mp4`) (`ESTUDO_IMPACTOS.md` I):
   - o cano entra bem mais, dentro da torre;
   - a torre e o casco sentem a inércia, pouco mas visível.
9. `[x]` **As mortes com a assinatura de cada um** (feito: `fogo_carcacas_ab2.mp4`: a gasolina clara e a picape pulando, o diesel preto, a munição estourando e o míssil torto. Os pedaços refeitos na 2ª volta: `mortes_pedacos_ab_lento.mp4`; as marcas no chão variadas não) (`ESTUDO_IMPACTOS.md` J):
   - a Technical: a gasolina, a carroceria pulando e a chama;
   - o Stryker: a torre remota voando e a fumaça preta do diesel;
   - o Linebacker: a munição estourando e o míssil torto;
   - pedaços com forma, não confete;
   - marcas no chão variadas.
10. `[x]` **A carcaça queimando** (feito: `fogo_carcacas_ab2.mp4`; a fuligem 0,30 → 0,06, o fogo simulado padrão) (`ESTUDO_IMPACTOS.md` K):
    - a fumaça preta e grossa;
    - a chama saindo pelas aberturas e acendendo a fumaça;
    - a coluna que varia com o veículo e com o tempo.
    - Hoje nenhuma das duas versões presta.

## B. O que falta pro nível do game shot (`COMPARACAO_GAMESHOTS.md`)

11. `[x]` **O chão** (o 1º passo do plano feito, o teste do recorte, e a resposta dele: `chao_pedras_ab.jpg` e `chao_pedras_ab_perto.jpg`. As pedras escaneadas pela regra física, como B desligado (`-RTSTerrainStones`): a leira da beira do asfalto e o bloco no canal de rocha. No gs02 quase não muda, porque a diferença do alvo não é pedra solta no nosso mapa de duna: é o tipo de mapa, o deserto pedregoso (campo de rocha, cascalho, asfalto rachado). Pela sua regra "tipos de mapa, não regra", isso é um tipo de mapa novo do WorldForge, com as texturas CC0 do plano e essas pedras; anotado em `COMPARACAO_GAMESHOTS.md` item 1. E o estudo desse tipo de mapa, o 1º passo da regra "estudo antes": `docs/ESTUDO_DESERTO_PEDREGOSO.md` (o alvo medido: pedras de 0,4–0,8 m a ~1 por m², blocos de 1,6–4 m, asfalto rachado em placas de 1–2 m; duas texturas do plano de antes, a `aerial_rocks_02` e a `rocky_terrain_02`, são verdes e não servem; onde entra no WorldForge, o custo a medir e o que é seu decidir). Não mexi no shader do chão nem nas areias aprovadas):
    - pedra, cascalho, asfalto rachado, marca de pneu e mato, com material de verdade (texturas e pedras escaneadas, CC0);
    - primeiro o teste de um recorte só, lado a lado com o alvo.
12. `[x]` **Os prédios** (feito na 3ª volta: `predio_tiro_ab.mp4` e `predio_dano_fogo_entulho.mp4`. O tiro bate na parede de verdade, pelo relevo medido de cada prédio, e deixa a marca de fuligem; com 35% de vida o estouro da passagem e o fogo simulado nos pontos medidos do telhado, que apaga no conserto; a morte desce 80% e deixa o entulho de concreto. O kit modular e o reboco caído são da arte final): o desgaste com cara de lugar vivido (sem o "todo fodido" do item 1), o entulho em volta e os estados de
    dano.
13. `[?]` **As unidades** (não feito, de propósito: é o modelo, e o final é seu. Lado a lado na mesma câmera (`unidades_alvo_x_jogo.jpg`): no alvo, o blindado tem o desgaste, a poeira embaixo, as rodas e as escotilhas que fazem sombra própria, e a cor do time só numa faixa; no nosso M1 (o da maquete do Look), as caixas laterais inteiras são azuis e o casco é liso. A cor geral não é o problema (os USA do alvo também são cinza-claro). A poeira do mundo já existe (a receita do Look) e a sombra de contato também (conferida: o chão recebe o decalque). Isso fica de entrada pro seu modelo no Blender, junto com a ficha; mexer na cor do time do provisório é decisão sua):
14. `[x]` **As marcas da batalha que ficam** (feito: as carcaças queimam com fumaça preta, item 10; o prédio guarda a marca do tiro e deixa o entulho, item 12; as crateras já variam (o chão tem 3 atlas de cratera com 6 variantes cada, mais a mancha e o queimado da carcaça, conferido no registro). A mancha marrom ao lado do M1 não é efeito: é o mapa, um pedaço de "rocha" de ~5×1 m que o WorldForge pintou na areia do World_TestSalt2; anotada em `ESTUDO_IMPACTOS.md` M): as crateras, o chão queimado e as carcaças com fumaça (junto com o 10).
15. `[x]` **A obra com vida** (feito. A solda lê de dia a 75 m: `obra_solda_ab.mp4`. O Dozer: a poeira na lâmina não aparecia em duas voltas; na 3ª, o jeito do ZH (o `DozerDirtFall`): a terra escorre pela borda de cima da lâmina a cada subida dela, `dozer_terra_lento.mp4`. A fumaça do escapamento do ZH não entrou: o USA ficou sem diesel, decisão sua de 17/10): a poeira, a solda e o Dozer trabalhando, como no ZH.
16. `[x]` **A infantaria** (o 1º passo inteiro, pelo estudo `docs/ESTUDO_INFANTARIA.md` §5: a simulação 26 com a armadura de infantaria e o atropelamento do ZH; o Ranger e o quartel nos dados, com a fonte; o soldado e o quartel provisórios; a IA faz o quartel logo depois da fábrica e treina um soldado por tanque (+2), até 10 (o teste da simulação: 1 quartel e 10 soldados de cada lado em 8 min, os tanques iguais; no jogo, o teste da base: "barracks wanted" e 0 falhas); o botão e o cartão do Ranger e o cartão do quartel no kit do HUD (do próprio jogo, como a zona de lançamento). Testes da simulação: TUDO CERTO nas 3 compilações, determinismo OK. Vídeos: `infantaria_combate.mp4`, `infantaria_atropelamento_lento.mp4`, `quartel_produzindo.mp4`, `quartel_hud.jpg`. As escolhas que eram suas seguiram o ZH e trocam num número: a escala 1,25×, o soldado solto, sem sangue, sem a luz própria):

## C. A fechar no fim

17. `[x]` **O registro** (os estudos: `ESTUDO_IMPACTOS.md` M, `LINGUAGEM_DO_COMBATE.md`, `COMPARACAO_GAMESHOTS.md` item 6; nada novo foi aprovado, então o `APROVADOS.md` só ganhou o da revisão): `docs/APROVADOS.md`, `RECEITAS_DO_LOOK.md` e os estudos atualizados com o que entrou.
18. `[x]` **O relatório pra ele avaliar de manhã** (a tabela no topo): um vídeo por item, o antes e o depois.
19. `[?]` **O commit:** só com o "aprovado" dele. A pergunta fica no relatório; nada é commitado sem ele.

## D. Da revisão dele (30/10 de manhã)

20. `[x]` (feito e provado: `v4_fx_predio`, o raio nos triângulos do modelo) **O tiro no prédio bate no modelo de verdade** (item 1, aceito "por hora", mas: "isso aqui me deixa broxante [...] o
    impacto na construção não tá no hitbox dos models dela"). Hoje ainda clareia no meio do portão e levanta pó no chão na frente:
    o relevo de cima (`Heights/*.json`) não enxerga o portão recuado. O certo: o raio do tiro nos triângulos da malha (traço na
    colisão complexa das peças do prédio), com a mesma prova em vídeo. Fazer depois da revisão.
21. `[?]` (o A/B pronto (`v10_nevoa_A` × `v10_nevoa_B`); a escolha é dele) **A névoa só do lado do sol** (item 2, a atmosfera aceita "por enquanto", mas: "no game shot os tanques da China no
    topo não são pegos pela névoa [...] só se enxerga a névoa viva espessa no canto que o sol vem"; a linha vermelha dele: no
    alvo o véu começa a ~25% do topo à esquerda e quase no topo à direita; no nosso, no meio da tela). Medido na foto dele: o
    mais escuro do canto dos tanques 0,04 no alvo e 0,42 no nosso. O caminho (a geometria: o topo da tela fica a 1,5× o foco; a
    linha do alvo a ~1,2× à esquerda e ~1,47× à direita): o começo da névoa a ~1,2× o foco, pouca densidade e o espalhamento
    bem pra frente (o sol a 45° à esquerda da câmera faz o lado esquerdo brilhar e o direito ficar limpo), com rampa suave.
    A/B em vídeo contra o alvo. **Se não ficar bom, fica exatamente a de agora.** Fazer depois da revisão.
22. `[x]` (feito: o reflexo e a sombra pela média em volta (`DustField.usf`)) **A poeira escura em "quadrado" atrás do Stryker** (vídeo da metralhadora: "é a porra da poeira de areia saindo como
    um quadrado ali [...] eu quero saber o que causa isso"; eu tinha dito errado que era a rocha do mapa). **A causa, no
    código:** a poeira do campo único (`DustField.usf`, `MainLight`) recebe o reflexo da areia (`GroundD` no
    `BlastRaymarch.usf`) pelo sol no chão **num ponto só, bem embaixo dela** (`Vg = RTSSunVisibilityGroundM`). Onde o chão
    embaixo está na sombra do Stryker, a poeira perde de uma vez o reflexo da areia (e o sol, que esse é certo), fica marrom
    escura e com o contorno exato da sombra da caixa do Stryker: o quadrado. Na física, o reflexo vem de todo o chão que a
    partícula vê (um disco de alguns metros em volta), então a borda tem de ser macia e a poeira na sombra bem menos escura.
    **O conserto:** o reflexo pela média do sol no chão num disco de raio ~2× a altura da poeira (5–8 amostras), não no
    ponto. O shader é de todas as sessões: cópia, compilar na fila, A/B em vídeo na cena `fx_metralhadora`.
23. `[x]` (feito: `muzzle_forward_m` 1,56 medido na malha) **O tiro da Technical sai na frente da picape, não na boca da metralhadora** ("a bola de tiro da Technical tá saindo
    totalmente à frente deles, e não na boca do canhão"): o ponto da boca (`muzzle_forward_m` e a altura da arma no
    `Units.json`) está no para-choque. Medir a boca no modelo e acertar, com a prova em vídeo.
24. `[x]` (feito e provado: `chao_depois_v18`, `RoadZAt`) **O X de areia no meio do cruzamento** ("a estrada tem um X horroroso de areia no meio, parecendo textura mal feita,
    mal encaixada"): as faixas do meio da pista, perto do cruzamento do centro da vila, saem como manchas cor de areia de borda
    irregular, em X. Entra na comparação do chão de antes × agora (o ponto 05, o cruzamento): ver se é a versão do chão ou a
    pintura das faixas do WorldForge, e consertar na origem. **Conferido (30/10):** o X está igual no chão de antes e no de
    agora (`captures/game/escala_v5/chao_antes_agora/chao_05_centro_vila.jpg`): não é a versão do chão, é a estrada do
    WorldForge no cruzamento (as faixas do meio, perto do cruzamento, saem como buraco de areia em vez de tinta branca).
26. `[x]` (feito: a sombra pela média; "a outra coisa" atrás do Stryker, só com ele mostrando o quadro) **Um quadrado de sombra em todos os veículos** (o Felipe, 30/10, vendo o vídeo do tiro no casco: "tinha um quadrado de
    sombra nada a ver [...] no Stryker, na Technical e em todos os veículos"). Achar a causa (suspeitas a conferir, nessa ordem:
    o decalque da sombra de contato embaixo de cada unidade, quadrado; a poeira na sombra do item 22) e consertar, com o A/B em
    vídeo. E ainda falta achar "a outra coisa" que ele via atrás do Stryker, que não é essa sombra: perguntar mostrando o quadro.
27. `[x]` (feito: o repouso do cano depois do pivô) **O recuo: o cano solto da torre** (o recuo foi aprovado, menos isso: "o cano tá voando, ficou afastado da
    torreta"): a folga entre o cano e o mantelete no M1 (`docs/aprovados/explosoes/2026-10-30_recuo_cano_solto_PROBLEMA.jpg`).
    Medir no modelo onde o cano encosta e acertar o pivô/posição de repouso, com o quadro ampliado de prova.
28. `[x]` (feito: a mola da rolagem do M1) **O recuo: o tombo do casco** ("a inércia no tanque é imperceptível, ele quase nem sai do lugar, não tomba um pouco
    pro lado, dá um efeitinho assim bem macio só pra ver como fica"): o casco inclina um pouco no tiro (pro lado contrário do
    cano, como a mola da suspensão), macio, e volta. Mostrar em vídeo lento e no zoom do jogo. **A régua (30/10):** o tanque da
    GLA, aprovado 100% ("dá uma inércia boa pra trás, não entendi por que da USA não deu isso"). Achar por que o M1 não joga
    igual (massa, mola ou o tranco por unidade no `Units.json`) e igualar; o cano solto do item 27 vale pros dois.
29. `[x]` (feito: o esguicho simulado, a massa 0,5 no A/B `v12_bala`) **A areia das rajadas da metralhadora, refazer** (reprovada: "parecem um monte de pixelado saindo, sem qualidade, sem
    textura, sem vivência [...] mas é nesse tamanho mesmo, tá quase certo"). O tamanho fica; o que sai é a bolinha branca lisa.
    Estudo antes: como a areia sobe com a bala de verdade (o leque de grãos, o véu fino e a coluna que cai) e como os AAA
    fazem; a areia no tom do chão, com textura e volume, acesa pelo sol. Prova em vídeo de perto e no zoom do jogo.
30. `[?]` (provado (31/10, `v30_dozer_giro_dia`/`_tarde`, `v31_dozer_giro_tarde`): o giroflex de 80 000 cd com sombra acende a cabine e o chão perto, no dia e no fim de tarde; falta o aprovado dele) **O Dozer: gravar de novo, de frente, e a luz do giroflex** ("não consegui ver nada, ele tá virado pro lado
    oposto da câmera"; e "essa luz em cima amarela piscando tem que ter uma luz mais forte, refletir a luz nas coisas, não tão
    forte também, olha o tamanho dela"). (1) A cena com a lâmina virada pra câmera, no zoom do jogo e de perto, pra ele avaliar a
    terra caindo da lâmina. (2) O giroflex vira luz de verdade que gira e acende o chão, o Dozer e o que estiver perto, na força de
    um giroflex desse tamanho (medir a de verdade: candelas de um giroflex âmbar de teto), com sombra; vídeo de dia e no fim
    de tarde.
31. `[x]` (feito e provado: `v10_fx_predio_etapas`) **O prédio: a destruição de verdade e o dano por etapas** (o dano e o fogo foram aprovados, "top demais, um grande
    acerto"; mas: "na hora da explosão algo mais bem feito, não isso aí que ele desce e spawna quadradinhos do nada, eu quero
    desmanches, explosões, danificações, desabamento, coisas variadas, e quando tá com pouca vida, além de só pegar fogo,
    danificações, desabamentos também enquanto tá sendo destruído"). Estudo antes: como o ZH faz (os estados de dano dos
    prédios: o modelo danificado e o muito danificado, as peças que caem, o OCL de destruição) e os AAA (fratura pré-cortada,
    Chaos da Unreal, a destruição do CoH/Battlefield, o colapso por andares); variado por prédio; o entulho nascendo das peças
    que caem, não do nada.
32. `[x]` (feito e provado: `v10_fx_veiculo_ferido`) **As unidades com o fogo e o dano do prédio** ("inclusive quero fogo assim e danificação assim nas unidades"): com
    pouca vida, o veículo pega o mesmo fogo simulado nos pontos certos do modelo (motor, tanque de combustível) e mostra dano
    (fuligem, peças soltas, amassado), por etapas como o prédio. Estudo antes (o ZH: os estados de dano e o fogo dos veículos
    danificados) e a prova em vídeo.
33. `[x]` (feito: o clarão de 15 cm e a bala a 900 m/s) **O clarão do fuzil do soldado parece fogo no chão** ("tá saindo um fogo no chão no lugar das infantarias, o que é
    ele?"). É o clarão da boca com o pulso de luz laranja (`ARTSEffect::SpawnFlash` no `RTSMatchSubsystem`, as balas): o fuzil
    usa o mínimo da metralhadora (raio 45 cm, luz forte), que a 1 m do chão acende a areia em volta. O fuzil tem de ter o
    clarão e a luz do tamanho de um fuzil.
34. `[x]` (feito e provado: `v12_fx_tiro_chao_fogo`) **A ideia dele: um foguinho fraco por alguns segundos onde o tiro de tanque bate no chão** ("por conta do calor,
    daria pra deixar uns segundinhos um foguinho fraco assim"). Conferir antes na física e nas referências (o que fica
    queimando depois do impacto de HE e de flecha na areia, no capim e no asfalto) e propor com o motivo; no capim e no mato
    é real, na areia pura quase não.
35. `[?]` (o pano no vento feito; saía preto (31/10, consertado e provado no `v30_fx_bandeira`: a cor do time no pano); falta o aprovado dele) **A bandeira da Technical** ("tá pequena, e meio que mal feita, e não tem simulação do vento nela, o vento do mapa,
    não tem física"): maior, bem feita, com o pano simulado no vento do mapa (o `MPC_Wind` / `wind_yaw`).
36. `[x]` (feito: o estudo, o soldado vestido, as mortes, a cobertura, a supressão, as posturas, a sucata, as pegadas) **A infantaria, refazer** (reprovada: "horrível, a morte deles, o tiro numa velocidade extremamente lenta, muito
    quadradinhos, sem detalhes [...] de broxar e dar vontade de vomitar"). O soldado provisório com detalhe de verdade, a morte
    com animação de queda boa, a bala na velocidade real (o traçante de fuzil a ~900 m/s, que no zoom do jogo é um risco), o
    estudo da infantaria dos AAA vistos de cima (CoH 3, Company of Heroes, ZH) antes.
    **O pedido dele (30/10):** "estude muito bem como fazer, não é de qualquer jeito [...] não é uma simples unidade nova num
    jogo RTS AAA poderoso lindo, tem suas qualidades, mecânicas de jogo, tudo muda aqui nessa situação, tem que ser bem feito,
    bem analisado, ideias ótimas, ideias criativas". Primeiro o estudo de verdade: o design (as mecânicas do ZH a fundo, CoH,
    Men of War, WARNO, Broken Arrow, a infantaria real contra blindado, as ideias criativas pro nosso jogo, o elenco por
    facção) e o visual (modelo, animação, render de centenas na 1660, o tiro de fuzil, a morte). Mostrar as ideias pra ele
    escolher antes do código.
37. `[x]` (feito e provado: `v10_air_fx_hellfire`, `v8_patriot_67`) **O míssil que se vê** (Patriot, Apache, Linebacker, o Tomahawk do caminhão): um míssil pequeno modelado, com o
    fogo do motor atrás virando a fumaça, "realista, bonito, rápido, simples, visível porém imperceptível". O rastro branco:
    um pouco menor, menos borrado, com textura, sem as bolonas soltas.
38. `[x]` (feito e provado: `patriot_ar_A_B.jpg`, `hellfire_A_B.jpg`) **A chegada do míssil: uma explosão por arma, que se vê** ("quando chega a explosão nem aparece"): o Hellfire do
    Apache, o Patriot, o antiaéreo do Linebacker e o Tomahawk, cada um na sua potência, tamanho e cara, sem repetir. A régua de
    "visível" é o Cruise Missile (aprovado como um dos melhores). Estudo antes: as ogivas de verdade (kg de explosivo de cada
    uma, a fragmentação do Patriot, a carga oca do Hellfire) e como o ZH e os AAA mostram cada uma.
39. `[?]` (a 1ª (o jato da GLA) feita; as próximas com ele no EmberGen) **A explosão de cada unidade, no EmberGen, junto com ele** ("me ensine a usar o EmberGen, vamos construir uma bem
    bonita, onde pra cada unidade tem uma diferente"; inclui a explosão do vídeo das mortes, que ele pôs por último). Ele
    opera o EmberGen (não tem linha de comando) e eu guio; a gravação sai em VDB e entra no raymarch pelo
    `BlastLab/Tools/build_raymarch_vdb.py` (volume na luz do mundo, nunca flipbook chapado). Uma unidade por vez.
40. `[?]` (o carregar por facção feito e o teto de 128³ cumprido (o jato em 128³); o teto de RAM por preset espera o aprovado dele) **O orçamento das gravações de volume** (o Felipe, 30/10: "o mesmo M1 vai ter umas 3 variações de morte, o Overlord
    da China um diferentão, os tanques nucleares outro, tiro na areia depende do tanque, construção também"). Muitas gravações
    (70–500 MB cada no formato do jogo; o bruto do EmberGen, 230 MB por quadro, vai pra Lixeira depois de converter). O plano:
    (1) **peças, não cenas**: gravações curtas e reaproveitáveis (o jato, a bola, a saia de areia, a coluna, o fogo baixo)
    montadas por unidade com escala, relógio, giro, espelho e semente diferentes (as 3 mortes do M1 = 3 combinações);
    (2) **carregar por partida**: só as gravações das facções e unidades em jogo, descarregar o resto; (3) **por preset**:
    96³/128³ e menos quadros nos mais baixos; (4) medir o total (pacote e memória da placa) a cada gravação nova.
25. `[x]` (feito: a ondulação meio-termo, a areia só nas bases, o afundar (`chao_depois_v23`); falta o olho dele) **As duninhas do chão novo** ("esse chão parece que regrediu, tem muita duninha de areia"). Conferido no mapa todo
    (cena `chao_mapa_todo`, 12 pontos, mesma câmera; `captures/game/escala_v5/chao_antes_agora/`): a ondulação da v5 do chão
    (a "ondulação cheia" de 29/10, `GroundVersion` 1) está mais forte em 8 dos 12 pontos, +26% a +54% de detalhe médio na
    areia; iguais no cruzamento e em 2 trechos de areia aberta. **A decisão dele (30/10, registrada no `APROVADOS.md`):**
    - **a ondulação:** nem a de agora nem voltar à de antes: uma textura nova de ondulação, mais variada, que não canse o
      olho (estudo antes: como a ondulação de verdade varia, tamanho, direção, onde some; e as referências);
    - **a terra da beira do lago (salina):** fica a nova; a de antes guardada nos assets;
    - **a base:** fica a areia nova no pátio, mas as estradinhas de terra da base até a vila voltam pra terra escura de antes.
    Tudo natural. A/B no mapa todo de novo (a cena `chao_mapa_todo`).
- O chão pedregoso e a vila do gs02: **ele desistiu** ("é outro tipo de mapa, um deserto mais meio-dia, menos poeira no ar").

---

## E. A tarde e a noite de 30/10 (o pedido dele ao sair)

O pedido: "eu ainda prefiro o seu [o fogo da carcaça], pega esse que fizemos [o jato do EmberGen] e coloca em outro [...] você tem uma
lista de coisas pra fazer, muita coisa [...] cada coisa bem feita, muito bem feita, perfeita, estudado, planejado, as unidades também
[...] todos os ajustes e melhorias que listamos [...] no mesmo padrão de qualidade do serviço feito de madrugada [...] nos game shots tem
várias imagens lindas com unidades, infantaria, segue aqueles modelos". Decidido também: a torre continua voando ("é assim que tem que
ser"); o M1 morre com o fogo da carcaça (o simulado), sem o jato do EmberGen.

**A ordem** (os itens da seção D, cada um com estudo, prova em vídeo no jogo e registro; um lote de código por compilação):

- **Lote 1, consertos com causa achada:** o jato do EmberGen noutra unidade (39); a boca da Technical (23); o cano solto (27); o tombo
  do M1 (28); o clarão do fuzil (33); a poeira quadrada e o quadrado de sombra (22, 26).
- **Lote 2, os médios:** a areia das rajadas (29); a bandeira no vento (35); o Dozer e o giroflex (30); o tiro no prédio pelo modelo
  (20); o foguinho no chão (34).
- **Lote 3, os grandes:** a infantaria inteira pelos estudos e pelos game shots (36); o prédio desabando por etapas (31); o dano e o
  fogo nas unidades (32); os mísseis que se veem e a explosão de cada arma (37, 38); o chão (25); a névoa do lado do sol (21); o X da
  estrada (24); o orçamento das gravações (40).

**O andamento da seção E:**
- 30/10 ~17h–18h, o lote 1 (compilado e gravado; nada commitado):
  - **O jato do EmberGen no tanque da GLA** (não na Technical: a picape queima gasolina baixa; o T-55 não tem painel de alívio, a
    munição queima, a torre voa e o fogo sai em jato pelo anel dela): `visual.death_jet` no `Units.json`, a gravação
    `eg_jato_municao` (o recorte de 9 m em volta do jato, 5,6 cm por voxel, 361 MB) nasce no anel da torre e o fogo da carcaça
    espera 3 s. Vídeo: `captures/game/lote1_fx_morte_gla/batalha.mp4`. O M1 segue com o fogo da carcaça.
  - **A boca da Technical** (23): sem a peça "Barrel", a boca ficava a 5 m do pivô; `muzzle_forward_m` 1,56 medido na malha.
  - **O cano solto** (27): a causa era o repouso do cano guardado antes de encaixar as peças no pivô (o recuo devolvia o cano 10 cm à
    frente no M1 e ~40 cm no da GLA). Consertado no `RTSUnitActor`.
  - **O tombo do M1** (28): a mola da rolagem amortece 0,3 por passo e a do arfar 0,8; o tiro de lado leva 3,5× pra tombar igual.
  - **O clarão e a bala do fuzil** (33 e parte do 36): o clarão do tamanho do fuzil (15 cm, não a bola de 90 cm), a bala desenhada a
    900 m/s (era 10 m/s) e 1 bala por tiro.
  - **A poeira e a sombra quadradas** (22, 26): a sombra pela média de 5 pontos e o reflexo do chão pela média de 7 (`DustField.usf`).
- 30/10 ~18h–19h, **a infantaria nova** (36, o 1º passo do visual): o corpo humano e os clipes de fuzil do modelo da Unreal 5.8
  (copiados pra `Game/Content/Characters/Mannequins` e `Game/Content/Weapons/Rifle`), a animação em C++ (`RTSSoldierAnim`: parado,
  andando, correndo pela velocidade medida na raiz de cada clipe, atirando, 6 mortes, troca suave de 0,2 s), o fuzil na mão, a farda
  areia e o colete e o capacete na cor da facção. **A bandeira da Technical** (35): o pano de 1 × 0,65 m no vento aparente.
- 30/10 ~18h30–19h30, **o lote 2 e a infantaria** (compilado e gravado; nada commitado):
  - **O soldado** (36): o corpo sumia ao atirar parado (o clipe do tiro do modelo é aditivo e era tocado como pose inteira: o corpo
    encolhia num ponto e sobravam o capacete e o fuzil); agora o coice soma por cima da mira, e andando também. **O tranco do acerto**
    (os clipes de reação do modelo), um por vez. A bala no soldado deixou de fazer o acerto de casco (a faísca de metal e o pó da tinta,
    que de perto liam como uma bola de fogo ao lado dele). Vídeos: `captures/game/v4_fx_soldado_perto`, `v5_fx_soldado_perto`.
  - **O sangue** ("sim, com sangue"): o respingo no chão além de quem leva bala (o leque de gotas no rumo dela) e a poça que cresce
    embaixo do corpo, num decal aceso pela luz e pela sombra do mapa (`M_BloodDecal`, `URTSMatchSubsystem::AddBlood`); secam em ~25 s
    e somem. As gotas em 3D saíram (as peças de destroço ficavam de pé como palitos vermelhos).
  - **O tiro no prédio pelo modelo** (20): o raio do tiro bate nos triângulos do modelo (`Heights/<malha>.tris.bin`, do
    `roof_points.py`), não mais na caixa: o pó e os pedaços saem do pilar do portão da War Factory (`v4_fx_predio`).
  - **O foguinho do tiro de tanque** (34): o fogo em loop (volumétrico, com a luz da carcaça) bem pequeno no buraco, 2–3,5 s na areia e
    3–5,5 s na terra e no chão duro; nada na neve e no barro. 1ª prova: não acendia (a limpeza do diretor apagava o volume na emenda
    do loop); consertado, falta a prova.
  - **O soldado vestido** (36, os game shots): a farda pintada nas UVs do corpo do modelo (a camuflagem, a bota, a luva, a
    joelheira, a faixa do time no ombro) e as peças presas nos ossos (o colete com os porta-carregadores, o capacete com o suporte
    do visor, a mochila, as bolsas do cinto), feitas por script no Blender (`Tools/soldier_dress.py`), no mesmo esqueleto (as
    animações tocam sem retarget). Em andamento.
  - **O soldado vestido no jogo** (36; vídeo `captures/game/v8_fx_soldado_perto`): o capacete moderno moldado na cabeça (com o suporte do
    visor), o colete com as alças e os porta-carregadores, a mochila, a farda em camuflagem pintada nas UVs do corpo e a faixa do time no
    ombro (o material `M_Soldier`). As variantes da China (capacete, farda mais escura) e da GLA (o lenço, o colete de bolsos, sem mochila)
    ficam prontas no Blender (`Saved/SoldierDress/{china,gla}`) pra quando essas unidades existirem.
  - **As mortes do soldado** (36): o relógio da morte anda certo, mas os 6 clipes do modelo terminam de joelhos (no Lyra, de onde vieram, o
    boneco de pano assume no fim): no fim do clipe o corpo vira boneco de pano (o PA_Mannequin do modelo) e deita pela física. Em prova.
  - **Os mísseis que se veem** (37): o Hellfire (Comanche), o Patriot, o Stinger (Linebacker) e o Tomahawk modelados nas medidas reais
    (`Tools/missile_models.py`), a ponta no ponto do míssil e a chama, a luz e a fumaça saindo da tubeira; o rastro pelo motor de cada um
    (o Patriot 0,75 da largura aprovada; o Hellfire de pouca fumaça; o Stinger um fio; o Tomahawk só no reforçador da largada).
  - **O prédio por etapas** (31): a cada passagem de 70, 50, 35 e 20% da vida um pedaço cede (o furo no telhado ou na parede pelo material,
    o escuro de dentro, o estouro pequeno, os pedaços de concreto caindo e o pó do reboco); consertado acima de 85% fecha. Na morte, o
    entulho cai de dentro do prédio enquanto ele desaba (cada pedaço do alto até o lugar do monte; os de fora saem da parede em arco), no
    lugar do monte que aparecia pronto. Em prova (`fx_predio_etapas`).
  - **O veículo ferido** (32): abaixo de 35% o motor pega fogo (o fogo em loop volumétrico, com a luz, andando com o veículo) e cada passagem
    solta pedaços de chapa. Escrito, entra no próximo lote.
  - **Achado de outra área, consertado:** o material das unidades com textura (`M_UnitAtlas`: a Technical) não compilava desde as marcas de
    tiro do prédio da madrugada (o código é o mesmo e pedia entradas que o das unidades não tinha): a Technical saía com o material
    padrão. As entradas novas no `Tools/import_gla.py`.
  - **A areia da metralhadora** (29), A/B v7: o véu mais ralo pegou o azul do céu (bolinhas cinza-azuladas, pior que o de antes): voltou ao
    de antes; ficou só a troca dos torrões (cubos escuros) pelos riscos finos de areia. O véu de verdade é o caminho do estudo (a gravação
    própria no tamanho real e o detalhe no volume).
- 30/10 ~19h30–20h30, **as provas da noite** (compilado e gravado; nada commitado):
  - **As mortes do soldado deitam** (`v9_fx_soldado_morte`): no fim do clipe o corpo vira boneco de pano e cai no chão, com a mochila e a
    faixa do time.
  - **O prédio por etapas** (`v10_fx_predio_etapas`): o furo rasga o telhado (o material `M_StructureHoled`: o mesmo visual do pronto,
    mascarado e de dois lados, o lado de dentro escuro pela normal contra a câmera) e, na morte, o entulho cai de dentro do prédio entre
    as explosões. Dois defeitos achados no caminho: o material da obra escurecia o prédio inteiro (o sinal de dentro falha no Nanite
    mascarado) e o corte da obra (o dado 5) virava 0 quando os dados cresciam até as vagas dos furos (o telhado sumia): os dois
    consertados. Nos testes do editor o prédio fica cinza ~5 s no 1º furo da sessão (o shader do material novo compilando); no jogo
    empacotado não existe.
  - **Os mísseis** (`v10_air_fx_hellfire`, `v8_patriot_67`, `v10_v4_siege`): o Hellfire oliva pequeno com o trecho quente atrás virando
    um fio fino de fumaça; o Patriot branco com as aletas; o Tomahawk chega com a explosão do míssil de cruzeiro (a referência dele).
  - **O veículo ferido** (`v10_fx_veiculo_ferido`): o fogo do motor andando com ele e os pedaços nas passagens.
  - **A névoa do lado do sol** (21), A/B na cena do game shot das três facções (`v10_nevoa_A` × `v10_nevoa_B`, a chave nova
    `rts.Look.FogG`): a B (o começo a 1,2× o foco, 0,6 da densidade, o espalhamento 0,75 pra frente) deixa o topo limpo e só o véu
    quente no canto do sol, mais perto do alvo; com menos brilho que o alvo no canto. Pra ele escolher (a regra: se não ficar bom,
    fica a de hoje).
  - **A duna nova** (25): a textura `varied2` (`Game/Tools/look_ripples_v3.py`, `captures/look/textures_v3_varied`): as manchas de
    ondulação forte, média e quase lisa, as cristas mais curvas, a megaondulação rara. Pronta pra entrar na próxima remontagem do mapa
    (o WorldForge, `RIPLIT=varied2`), junto da trilha escura base–vila, que pede o material do chão do WorldForge (a areia nas bases e a
    terra nas trilhas hoje são a mesma camada "Dirt").
  - **A areia da metralhadora** (29), do jeito do estudo: o esguicho simulado no Mantaflow no tamanho do mundo
    (`Game/Tools/blender_bullet_dust.py`), juntado à gravação da poeira (`dust_kick_v4_bala.rtsdust`, os quadros das lagartas idênticos
    bit a bit; os da bala a partir de 100 s) e tocado numa camada própria (`impact_bullet`) na escala 1, virada pro rumo da bala. Em
    prova (`v11_bala_*`).
  - **A cobertura nascida da batalha** (36, simulação 27): o soldado com uma carcaça ainda sólida (os 48 s em que ela queima) a até 3,5 m
    dele, do lado de quem atira, leva metade do dano de bala e 70% do resto (`World::InCover`, no `ResolveDamage`; o esmagar não). O
    teste novo da simulação (`TestCover`: o mesmo Ranger atirando no mesmo Ranger a 25 m, 90 de dano no aberto e ~45 atrás da
    carcaça) e o determinismo nas 3 compilações: TUDO CERTO. Falta o que se vê (o soldado agachado atrás dela: o modelo não tem os
    clipes de agachar e deitar; o próximo passo é a pose procedural ou os clipes) e a cratera e o entulho como cobertura.
  - **Várias por vez (36)**: o mecanismo é o do ZH (o Red Guard sai em par, `QuantityModifier`); o que o mod Contra faz não está
    documentado na web (achei só outro mod treinando a infantaria em grupo). (30/10, simulação 32) o mecanismo pronto: o
    `"produce_count"` do Units.json (o `UnitDef::ProduceCount`, 1 a 8): um item da fila solta N soldados pelo preço de um, cada um na
    vaga dele da área de reunião (o teste `TestProduceCount`). O Ranger fica em 1 (o do ZH); o número de cada soldado da China e da
    GLA entra com o elenco deles.
  - **O orçamento das gravações** (40): medido e escrito em `docs/ORCAMENTO_GRAVACOES.md` (2,2 GB de RAM por partida hoje; o jato da GLA
    em 160³ subiu a vaga de todas as explosões; a regra proposta: peças em vez de cenas, teto de 128³, teto de RAM por preset, carregar
    só as peças das facções da partida). O código do carregamento por facção fica pra depois do aprovado dele na regra.
- 30/10 ~20h40–21h30, **v12 e v13**:
  - **A areia da metralhadora** (29), a massa do véu escolhida no A/B da rajada da Stryker (`captures/compare/v12_bala`: antes, 1, 0,5,
    0,3): a 1 vira algodão de ~3 m que cobre a área (o defeito do RTS); a 0,3 some; a **0,5** fica o véu com volume e textura, que se
    vê sem cobrir. Padrão 0,5 (`-BulletDustMass=` pro A/B).
  - **O foguinho do tiro no chão** (`v12_fx_tiro_chao_fogo`): a chama pequena na cratera com o fio de fumaça escura, sem o bloco cinza
    de bordas retas de antes.
  - **O soldado ferido acendia fogo** (achado no `v12_fx_soldado_perto`): o fogo do motor do veículo ferido (e a fumaça e o jato pelo
    anel da torre) nascia no chão do lado do soldado ferido; o soldado saiu desse caminho (o ferido dele é o tranco e o sangue).
  - **A cobertura** (36): o arredondamento do dano atrás da carcaça sorteado pelo passo (a bala de 5 levava 3 = 60%; agora 2 ou 3, na
    média a metade: 48 de 90 no teste).
  - **Construir em cima some com a carcaça** (36, simulação 28, o C&C, a frase dele: "desaparecer com a carcaça tanto de unidades quanto
    de construção se alguém botar pra construir em cima, chãos machucados também meio que voltariam pro padrão"): quando o Dozer toma o
    chão (`World::ClaimFootprint`), as carcaças do lote saem (o `WreckRemoved` com o motivo 2: o fogo apaga e ela desce) e o evento novo
    `StructureGroundClaimed` faz o desenho tirar o entulho, as pedras e os destroços do lote (`ARTSDebrisField::ClearArea`) e as marcas
    do chão com o centro ali, o queimado, a cratera, o avental e a esteira (`URTSGroundMemory::HealArea`, os cadernos redesenhados).
    O teste novo (`TestBuildOverWreck`) e o determinismo nas 3 compilações: TUDO CERTO. A cena `fx_obra_em_cima` (a ordem nova "build"
    dos cenários) provada (`v14_fx_obra_em_cima`: a carcaça do lote sai, 883 pedaços de entulho e 5 marcas do chão somem quando o
    Dozer toma o chão, em 3 ms; a carcaça de fora fica).
- 30/10 ~21h30–22h, **a supressão e a sucata** (36):
  - **A supressão** (simulação 29; o CoH: a metralhadora suprime um grupo em 1–2 s e prende em 3–4 s): o soldado acumula 0–1000 pelo
    dano bruto que chega nele (a bala 6 por ponto, a explosão e o canhão 4), os do mesmo dono a até 4 m levam metade da bala (a rajada
    varre o grupo), a cobertura corta pela metade; cai 300/s depois de 1 s sem fogo. Suprimido (≥ 350): o passo a 60% e a recarga a
    135%; preso pelo fogo (≥ 800, solta abaixo de 450): o passo a 25% e a recarga a 200%. O teste novo (`TestSuppression`: o alvo
    suprimido, o vizinho com metade, anda a 70% sob fogo, volta a zero) nas 3 compilações: TUDO CERTO.
  - **As posturas** que o modelo da Unreal não tem, montadas no Blender sobre a pose de mira dele (`Game/Tools/soldier_poses.py`, a IK
    de dois ossos nas pernas; `export_soldier_anims.py` e `import_soldier_anims.py`): o **ajoelhado de tiro** (o suprimido, e o parado
    atrás de uma carcaça sólida: a cobertura se vê) e o **encolhido** (o preso pelo fogo: o tronco e a cabeça pra baixo; não deitado, que
    de cima lê como o morto). A troca leva 0,35 s; o coice do tiro por cima. Prova: `v15_fx_supressao` (`captures/compare/v15_supressao_perto.jpg`).
  - **A sucata** (simulação 30; o reclaim do Supreme Commander, como um extra do dinheiro do ZH): o prédio que cai agora deixa o
    entulho na simulação (não bloqueia; sai com a obra em cima ou recolhido); o Dozer clicado na carcaça ou no entulho (o cursor da
    chave) vai até lá, desmancha (a carcaça 3 s + 1 s a cada $150; o entulho 4 s + 1 s a cada $200) e recebe 15% do custo da carcaça e
    10% do prédio (o M1: $135 em 9 s; a usina: $80), com o "+$" de sempre. O teste novo (`TestSalvage`) nas 3 compilações: TUDO CERTO.
    A cena `fx_sucata` (a ordem nova "salvage" dos cenários) provada (`v16_fx_sucata`: $15 000 → $15 135 com a carcaça do M1 →
    $15 215 com o entulho da usina, os dois somem; a casca queimada do prédio sai junto a partir do v17). A IA ainda não recolhe.
  - **O entulho como cobertura** (36, simulação 31): dentro da planta do entulho, coberto por todos os lados; a até 3 m dela (o
    espirrado do desabamento), contra quem atira do outro lado; o soldado parado ali ajoelha. O teste novo (`TestRuinCover`: 60 de dano
    no aberto, 24 ao lado do entulho) nas 3 compilações: TUDO CERTO.
- 30/10 ~21h45–22h30, **o chão (25 e 24)**, o World_TestSalt2 remontado (`chao_antes_v16` × `chao_depois_v16`, a cena `chao_mapa_todo`):
  - **A duna nova** (`RIPLIT=varied2`): as manchas de ondulação forte, média e quase lisa.
  - **A areia só nas bases** (`WorldForge/Tools/build_slice.py`, `base_sand_node`): a terra batida vira a areia nova só dentro das 4
    áreas de base do mapa (o "keepout" do painel de raio 88 m, a borda macia de 70 a 92 m); fora delas a terra escura de antes volta: as
    trilhas da base até a vila, e também os lotes da vila e a beira do oásis (a terra que não é trilha usa a mesma camada; o visual da vila
    aprovado em 13/10 era esse). `BASE_SAND=0` na montagem volta a areia em toda a terra (o chão de 29/10).
  - **O X de areia no cruzamento** (24): as manchas ficam nas diagonais do cruzamento, onde a "estrada mais perto" troca de uma rua
    pra outra. 1ª hipótese (o véu de areia da beira lido da textura RoadDir, consertado em `RoadDirPixels`): não era (`chao_depois_v17`
    igual). 2ª, pela leitura do código (um agente): cada estrada tem a cota dela e o chão e a laje davam um degrau na costura, cada
    malha na sua grade (a laje em células de 0,48 m, o chão em quadros de 0,5 m); com só 1,5 cm de folga debaixo da laje, a areia de
    baixo furava a laje em manchas ao longo das diagonais. Consertado na origem: `FWorldDetail::RoadZAt`, a cota das duas misturada
    na costura (meio a meio nela, só a da mais perto a 4 m), no chão (`SliceHeight`) e na laje (`BuildRoadSlabs`). **Provado**
    (`chao_depois_v18`, `captures/compare/cruzamento_v16_v18.jpg`): o cruzamento limpo, o resto da estrada igual.
  - **A IA recolhe a sucata** (simulação 30): o Dozer dela parado, sem obra mandada no pensar, recolhe a carcaça ou o entulho mais perto
    a até 90 m da casa, fora do alcance das defesas inimigas (+15 m) e longe do armado inimigo conhecido (40 m). No teste do Dozer no
    bolso entre duas Patriots a base subiu melhor (11 armados e 0 Dozers perdidos em 4 min; antes 9 e 2). O comando novo entrou no
    replay e na rede (`Commands.cpp`).
- 31/10 ~0h–1h, **a chegada de cada míssil** (38). O estudo: as ogivas de verdade (o Hellfire ~9 kg de carga oca, a PAC-2 da Patriot
  ~90 kg de estilhaço que estoura no ar por proximidade, o Stinger ~1 kg, o Tomahawk ~450 kg) e a bola de fogo crescendo com a raiz
  cúbica da carga (Hopkinson-Cranz, a mesma escala da ogiva v2: Hellfire ≈ 0,29, Patriot ≈ 0,60, Stinger ≈ 0,14 do Tomahawk). O A
  gravado (`chegada_A_*`, as cenas novas `fx_chegada_patriot_ar`, `fx_chegada_stinger`, `fx_chegada_patriot_chao` e a `air_fx_hellfire`):
  o acerto do míssil na aeronave não tinha bola nenhuma (a ogiva era zerada no ar, só o clarão e uma baforada de 0,55 m); o Hellfire
  saía com 2 kg (o clarão pequeno). O B:
  - **A ogiva que estoura no ar** (`UBlastFxDirector::AirBurst`): a mesma bola em volume da ogiva do chão na escala da carga, o pé meia
    bola abaixo do ponto, sem o anel de areia e com a saia baixa no gás da carga; os estilhaços e o clarão pelo K da carga; a baforada
    escura cresce junto (σ ∝ ∛kg). A Patriot no ar com 8 kg (`"air_warhead_kg"`: a de estilhaço estoura maior que no chão, 4 m de bola),
    o Stinger com 1 kg. `-NoAirBurst` volta a de antes.
  - **O Hellfire com 6 kg** (a carga de ~9 kg da ogiva de verdade): a bola de 3,7 m que se vê na câmera do jogo.
  - O Patriot no chão (4 kg, a receita aprovada) e o Tomahawk (a explosão do Cruise Missile, a régua) ficam.
  - Prova: `captures/compare/patriot_ar_A_B.jpg` (o mesmo enquadramento, à esquerda a de antes) e `hellfire_A_B.jpg`. A explosão de cada
    unidade no EmberGen (39) continua com ele.
  - **O carregamento por facção** (40, a parte que não muda o que se vê): o jato da morte da GLA (361 MB, e os 160³ dele sobem as vagas de
    todas as explosões) não carrega mais no começo; a partida carrega se uma unidade que usa ele pode aparecer (as da largada e o que elas
    e os prédios delas treinam e constroem, em cascata; no laboratório, sempre). `-AllBakes` carrega tudo como antes. O teto de 128³ e o
    teto de RAM por preset (que mudam o que se vê) seguem esperando o aprovado dele na regra. Provado: na partida normal
    (`faccao_usa_v20`) o jato fica de fora (as gravações dedicadas em 175 MB, antes ~536 MB); no laboratório ele carrega.
  - **A cratera que vira buraco** (36; "já tá mapeado isso, até a lagarta machuca o chão, mas não fizeram ainda: conferir por que
    parou"). Por que parou (29/10): com o afundar ligado, a janela do caderno em volta da câmera saía um retângulo liso, e o mapa do jogo
    passou a montar sem o deslocamento. Reproduzido num mapa de teste (`World_TestSalt2Disp`: o mesmo mapa, com material próprio, sem
    tocar no do jogo; `captures/compare/cratera_disp_0_1.jpg`): o chão em volta da câmera virava um plano marrom liso, sem a ondulação e
    sem sombra, e as unidades flutuavam. A causa: a magnitude do deslocamento é no espaço LOCAL do Landscape, e o do WorldForge tem a
    escala Z 100: os 200 cm da maquete do Look viravam até ±100 m no mundo. Consertado na montagem (`build_slice.py`, `DISPMAG` 2 = os
    2 m no mundo). **Provado no mapa de teste** (`cratera_disp1_v22`, `captures/compare/cratera_perto_0_v22.jpg`): o chão em volta da
    câmera normal, com a ondulação e a sombra, e a cratera vira a bacia funda com a parede em sombra e as pedras dentro (antes, a mancha
    chapada). O custo não deu pra medir na gravação (ela trava em 30 qps). O mapa do jogo (World_TestSalt2) só ganha o afundar com o
    aprovado dele: a montagem de sempre sem o `GROUND_DISP=0`.
  - **Achados no caminho:** o soldado sob a rajada da Technical fica curvado e não morre em 6 s (a conferir na cena de mortes
    `fx_soldado_morte`); os torrões do tiro de tanque na areia são cubos escuros de perto; a areia da metralhadora segue a de antes
    (o estudo do item 29 pronto, o próximo).

## O andamento (atualizado a cada item)

- 30/10 00h50: a lista montada. Nenhuma outra sessão rodando; a placa livre.
- 30/10 01h45: 1 e 2 feitos (o desgaste e a atmosfera). Começando o 3, o estudo da leitura da batalha.
- 30/10 03h30: o estudo da linguagem do combate feito (o ZH pelo FXList: quem atira se lê pelo pulso de luz e pela poeira no atirador). A metralhadora nova compilada e na 2ª volta (a rajada, erra e acerta, o traçante 1 em 4, as faíscas sem estrela; a areia mais escura que o chão pra ler). O ricochete na simulação (versão 25, decidido no disparo) e o acerto novo do canhão (sem a bola bege: a chama do furo e a fumaça escura) compilando.
- 30/10 05h50: o ricochete funciona (simulação 25, decidido no disparo; o teste da simulação: TUDO CERTO, determinismo nas 3 compilações): 4 ricochetes no tanque girado, o jorro raso de faíscas e o traçante indo embora. O acerto que entra sem a bola bege (o estalo, o leque, a chama pequena e a fumaça escura do furo): `tiro_tanque_ricochete_lento.mp4`. O recuo do cano feito. A metralhadora: achada a causa de os erros não aparecerem (caíam embaixo do próprio casco); refeita pela física (antes, do lado, atrás longe). O fogo da carcaça: a fumaça branca era o albedo 0,30 (mais densa só ficava mais branca, medido); a fuligem do incêndio vai a 0,06. As mortes: gasolina, diesel e munição (os estouros e o míssil torto). Compilando.
- 30/10 07h15: o prédio (as paredes), as mortes com a picape pulando e a obra com a solda provados. O custo da metralhadora medido e cortado (+11,3 → +5,4 ms com 12 armas). A sombra da palmeira não foi conferida (a malha do catálogo de plantas; fica a pista).
- 30/10 07h40: a cena do game shot gravada com a noite inteira junta (`batalha_antes_depois.mp4`). Nada commitado.
- 30/10 03h25 (a hora do relógio; as de cima foram anotadas de cabeça e estão adiantadas): a 2ª volta. Os pedaços das mortes refeitos (`mortes_pedacos_ab_lento.mp4`). A solda vira um trabalho que lê de dia (`obra_solda_ab.mp4`). O Dozer não saiu em duas voltas: anotado o caminho do ZH. A mancha do M1 é do mapa, não da morte. A sombra da palmeira conferida: certa. Nada commitado.
- 30/10 05h10: a 3ª volta. O Dozer pelo jeito do ZH (a terra caindo da lâmina). Os prédios: o tiro na parede de verdade pelo relevo medido (`Tools/roof_points.py` mede telhado e paredes de cada prédio no modelo) e a marca de fuligem no material; o fogo no telhado com 35% (o fogo simulado, volumétrico) com o estouro da passagem; o conserto apaga; a morte desce 80% e deixa o entulho de concreto. Achado de outra área: `-RTSGroundDebug` derrubava o jogo (RTSGroundMemory.cpp:1186, peça sem dono): consertado numa linha; com ele, conferido que o chão (o Landscape Nanite) recebe decalque, então a sombra de contato das unidades funciona. Nada commitado.
- 30/10 ~04h30: o estudo da infantaria pronto (`docs/ESTUDO_INFANTARIA.md`). A baforada dos 70% no prédio. Nada commitado.
- 30/10 05h40 (hora certa): a infantaria, o 1º passo inteiro (a simulação 26, o Ranger, o quartel, o desenho provisório, o atropelamento). O teste da base de sempre (o Dozer, a economia, a IA contra a IA): 0 falhas em 449 s. Nada commitado.
- 30/10 06h50: a IA com o quartel (o teste da simulação e o da base: 0 falhas), o botão e os cartões do Ranger e do quartel, o teste do recorte do chão (a resposta: é tipo de mapa). Um erro meu no caminho: o `enter` da fila não espera quando a placa está ocupada, e uma importação de malha (sem placa, `-nullrhi`) rodou junto com o teste da base; o teste passou e a importação foi refeita na vez certa. Nada commitado.
- 30/10 ~07h30: o estudo do deserto pedregoso (`docs/ESTUDO_DESERTO_PEDREGOSO.md`): o próximo passo dele é o tipo de mapa no WorldForge, com as decisões do §5. Nada commitado.
- 31/10 ~1h–2h30, **os pedidos dele antes de dormir** ("inclusive as lagartas, os pneus dos carros, tudo, e o passo da infantaria se
  der"; "faz a análise da infantaria [...] comparar, e prepara o jato da GLA"; "falei pra dar uma diminuída nessas dunas repetidas [...]
  tenta achar um meio-termo bonito"):
  - **O afundar de verdade no mapa do jogo**: o custo medido no mapa de teste, sem gravar, 4 rodadas de 55 s alternadas (o tiro de 3 M1 no
    chão na câmera do jogo): ligado 54,0 e 51,3 ms, desligado 52,3 ms (a 1ª, fria, 63,7): dentro do ruído. A cratera vira a bacia funda;
    a esteira e o pneu afundam pelo mesmo caderno (os poucos cm do sulco quase não mudam de cima: a cratera é o ganho). O World_TestSalt2
    remontado com ele (`v23`); só no Alta e no Ultra (`SetDisplacement`), `-RTSDisp=0` desliga.
  - **As pegadas da infantaria**: o soldado risca o caderno da esteira como um pneu estreito: os dois pés a ~22 cm, 12 cm cada, rasas
    (0,3 do fundo da lagarta) e claras, a passada de ~1,4 m de cada pé, trechos de 2 m (`RTSMatchSubsystem`, junto do pneu). Com o
    afundar, afundam junto. Em prova (`v23_fx_soldado_perto`).
  - **A ondulação no meio-termo** (`RIPLIT=meio`, `look_ripples_v3.py` `meio`): as cristas curvas da varied2 só em manchas (~55% da
    areia, a borda macia de ~1 m) e a areia lisa de verdade entre elas, mais baixa (a força 1,1 contra 1,5), sem a megaondulação. Medido no
    pixel do jogo (4,9 cm/px, o sol de 18°): a luz varia ±13%, entre a de 16/10 (±5%, a "anterior") e a varied2 (±24%).
    `captures/compare/ondulacao_patch_varied2_meio.jpg`; no mapa `chao_depois_v23`.
  - **A infantaria contra os game shots** (o r1_raid_slabs e o gs15), na câmera do jogo de 67 m: o tamanho está certo (o soldado/o casco
    do tanque 0,13 aqui, 0,10 no r1 e 0,12–0,14 no gs15). O defeito era o valor: no r1 o soldado fica a 0,12–0,16 do brilho da areia (a
    silhueta escura do oliva e do equipamento, com a sombra), o nosso ia de 0,15 a 0,87 (as partes acesas da cor da areia: sumia). A
    farda e o equipamento da USA ~45% mais escuros (`soldier_dress.py`: a farda ~0,15 linear, o colete ~0,12, o capacete ~0,13). Em prova
    (`v23_fx_supressao`, a mesma medida). O que fica pra frente: a cor do time maior (no gs15 o capacete e o colete inteiros na cor da
    GLA; o nosso tem a faixa), e o deitado do r1 (o encolhido segue, porque de cima o deitado lê como morto).
  - **O jato da GLA em 128³**: o mesmo recorte do export do EmberGen (`captures/vdb/m1_jato_v1`, só lido), a mesma linha do
    `build_raymarch_vdb.py` a 128 (`eg_jato_municao_128.brm2`); o A/B na morte do tanque da GLA (`fx_gla_jato`, 160³ × 128³ pelo
    `-HitBakeFile=`). Fica o arquivo de 160³; a troca é dele.
- 31/10 ~3h20–10h, **as provas da madrugada** (as gravações v24/v25; as imagens em `captures/relatorio_30_10_noite/` 14 a 17):
  - **A cratera no mapa do jogo afunda de verdade** (`v24_fx_tiro_chao_fogo` × `v12_`, a mesma cena): a mancha de antes vira o
    buraco com a borda; os tiros de tanque viram buracos. Os shaders compilaram, sem o xadrez. O que olhar: o buraco do tiro de tanque
    é a cratera aprovada encolhida (a A a 0,75: ~1,9 m de boca, 40 cm de fundo; fundura/boca 0,21, a de uma cratera de verdade), de
    fundo chato e parede firme; ampliado, lê meio "caixa". Se ler assim pra ele, o ajuste é a bacia mais redonda só nas pequenas.
  - **As pegadas** (`v24_fx_soldado_perto`, depois do conserto do quadro a quadro): cada soldado deixa a trilha dos passos na areia,
    só quando anda (parado e mirando não risca mais). Na câmera do jogo os dois pés juntam numa trilha só, de contas.
  - **O jato da GLA, 160³ × 128³**: a 1ª tentativa tocou o 160³ nas duas (o `-HitBakeFile` é `nome:arquivo`, com dois pontos; eu
    passei com `=`). Refeito (`v25_gla_jato_128`, o log: "128³, 223 MB"): a mesma tela (a diferença média 2 de 255, o mesmo detalhe
    nos 5 tempos). **O padrão do jogo passou pro 128³** (`game.ps1`, o link do `eg_jato_municao_128.brm2`): 138 MB a menos e as vagas
    do raymarch de todas as explosões voltam a 128³. O de 160³ fica no BlastLab; voltar = a linha do `game.ps1` ou
    `-HitBakeFile=eg_jato_municao:<caminho do 160³>`.
  - **A lagarta, o pneu e os passos no mapa do jogo** (a cena nova `fx_rastros_mapa`: o M1, a Technical e 2 Rangers cruzando a areia;
    `v25_rastros_disp1` × `v25_rastros_disp0`): os três riscam o chão com as trilhas v3 aprovadas em 16/10 (o mapa já liga a v3) e
    afundam junto com o chão. Com e sem o afundar a tela é a mesma: os 5 cm do sulco não mudam a silhueta de cima; o ganho do afundar
    é a cratera. O que olhar (não mexi, é o aprovado): na câmera do jogo a lagarta do M1 lê como duas linhas finas escuras; no alvo
    "areia C" é a faixa larga com a areia empurrada (a v3 já dizia "ainda não chegou no alvo").
- 31/10 ~10h30, **EM CURSO (parado pra ele fechar o Claude)**:
  - **O item 30 (o Dozer e o giroflex)** estava aberto de verdade: a prova `v3_fx_dozer_frente` não serve (o Dozer atrás dos andaimes
    e parado, sem obra mandada, então o giroflex nem acendia). Feito no código (`RTSUnitActor.cpp`, não compilado/provado ainda): o
    giroflex na força de um de verdade (o Federal Signal Sentry de 55 W, ~80 000 cd por clarão; era 25 000), o facho estreito (6°/12°,
    8° pra baixo, até 35 m), com sombra no Alta e no Ultra (`-RTSBeaconShadow=0` tira), a luz 20 cm fora da cúpula. As cenas novas
    `fx_dozer_giro_dia` e `fx_dozer_giro_tarde` (o Dozer constrói a usina, a câmera de 3/4 da frente dele). O roteiro
    `scratchpad/build_v26.ps1` (compilar na fila, gravar `v26_dozer_giro_*` e o custo `custo_giro_1a/0/1b`) foi disparado; se o
    Claude fechou no meio, rodar de novo (e conferir se ficou senha velha na fila da placa).
  - **A bandeira da Technical (35) sai PRETA**: o pano pega o material do slot PlayerColor antes da cor do jogador (que vai por
    `SetPlayerColor`, o dado 0 e o `TeamColor` do MID). Consertar: aplicar a cor do jogador também no `FlagMesh` no `SetPlayerColor`.
  - Depois: marcar o estado real de cada item da seção D (quase todos feitos e provados na seção E; com ele: 21, 39, 40 e o commit).
- 31/10 ~12h45–14h, **a volta depois da queda de energia** (o pedido: "cuidado com falhas idiotas [...] discuta com agents o que pode
  melhorar ainda, o que faltou, o que tá ruim, o que arrepia; por exemplo os destroços do tiro do canhão no chão voando e não ficando na
  física no buraco, e os tanques passando por cima e voando"). Tudo no lote `v30_*` (compilado na fila, em prova):
  - **A causa dos dois exemplos dele é a mesma: o afundar existe só no desenho** (a simulação e a física andam no terreno assado). Agora
    há o chão desenhado na CPU (`URTSGroundMemory::CraterHeightCmAt`: a mesma conta do carimbo da cratera, pela bacia sem os torrões, por
    células de 8 m) e quem pousa ou anda nele: as pedras da cratera, o entulho, os pedaços da carcaça e o fogo do chão
    (`VisualGroundZCm`); os torrões e as brasas do BlastFx (`BlastFxGround::SetSurfaceOffset`); o veículo pelas rodas (`SeatOnCraters`:
    5 pontos por lagarta, cada roda desce até o curso da mola de 25 cm, o casco na média e inclinado pela reta das rodas: na cratera
    pequena o tanque passa quase reto, como na vida, na grande mergulha e sobe) e o soldado pelo pé. A cena nova `fx_tanque_cratera`.
  - **Os torrões do tiro de tanque**: pelotas irregulares (eram chapas grossas que liam cubo), a velocidade pelo tamanho (o graúdo sai a
    3–6 m/s e cai na boca da cratera; antes os de 45 cm subiam ~15 m e voavam pra câmera), no máximo 32 cm, e na areia o tom da areia
    revirada e chamuscada (o albedo ~0,11; era 0,06, carvão). A terra da lâmina do Dozer também em pelotas.
  - **A revisão de vídeo de um agente** (16 vídeos, 15 defeitos; os recortes em `scratchpad/revisao/`) e o conserto dos que têm causa no
    código, por dois agentes (a causa em arquivo:linha e o patch) e por mim:
    - o entulho do prédio era um tapete retangular: agora um monte (a densidade e a altura pelo perfil, o contorno arredondado e
      irregular, os graúdos no miolo);
    - a carcaça aparecia 1–2 quadros antes do fogo cobrir: o queimado e a bandeira trocam aos 0,1 s, escondidos pela bola;
    - a sucata sumia de estalo: a carcaça desce durante o desmanche, o fogo sai pelo fim simulado (a fumaça já no ar segue subindo), o
      Dozer mostra a lâmina e a sirene;
    - a obra surgia inteira: o andaime sobe com a obra (antes do Dozer chegar, só a laje), a solda só com o Dozer trabalhando, e o
      material da obra preparado no carregar;
    - a torre arrancada parava de pé na borda: o chanfro de 45° na forma de contato (ela tomba pra face larga). **Mexe no pouso de
      todas as torres**, que é física aprovada: a olhar;
    - o soldado morto era arrastado 1,3–2,2 m pelo clipe (os corpos se encaixavam): a raiz encolhida pra 40 cm;
    - a fumaça preta do acerto virava uma rosquinha e sumia em ~2 s: o fim por diluição (depois da erosão da borda, sem limiar), a
      nuvem dura o dobro. O shader (`BlastRaymarch.usf`) com a codificação compatível: com o binário velho fica igual;
    - o rastro do míssil virava contas: o passo dos puffs pela largura do jato de cada míssil;
    - a fumaça do veículo ferido virava uma corda preta: a fuligem em volume do fogo do motor (sobe pelo empuxo, abre, vai com o vento).
  - **Achados que ficam pra ele** (não são de hoje ou mexem no aprovado): a trilha da lagarta no mapa do jogo lê como risco fino (já era
    assim no `v11` de 30/10; a v3 foi aprovada no mapa de teste; o `v30_rastros_surf0` testa se é o sulco por tipo de chão); o dano no
    casco do veículo segue fora (tirado em 14/10 até ter um bem feito); o buraco de parede de pedreira perto da largada é do mapa.
- 31/10 ~15h50–16h30, **a conferência da v30 quadro a quadro** (os recortes em `captures/rev_v30/`), e o que ela pegou:
  - **Os torrões novos saíram como estacas pretas em pé** (e azuis no fim de tarde, na lâmina do Dozer): o desenho das peças escalava só
    a largura (a altura fixa, pensada pra chapa de 3 cm); a pelota de 8–32 cm ficava com 70 cm de altura, e a normal torta refletia o
    céu. O mesmo erro já vinha no bloco grosso de 30/10 ("~0,3 do tamanho" saía 30 cm em qualquer tamanho: os cubos escuros). Agora o
    bloco e a pelota crescem nos três eixos; a chapa guarda os 3 cm. Lote `v31_*`.
  - **A sucata: o Dozer trabalhava parado a ~8 m, a lâmina longe do monte, e o entulho do prédio sumia num quadro no fim.** A simulação
    usava o alcance da obra (o raio + 2 células). Simulação 34: encostado (`World::SalvageReachOf`: da carcaça, o raio do Dozer + 0,5 m,
    que alcança de qualquer lado sem empurrar a carcaça; do entulho, 0,8 do raio, a lâmina na borda do monte). E o monte e o prédio caído
    descem no chão durante o desmanche, de fora pra dentro, no ritmo da simulação (`ARTSDebrisField::SinkArea`, `SetRuinSink`).
  - **A bandeira do prédio fica de pé no entulho** (a War Factory: o pano e o mastro queimados, sozinhos). ~~Da malha da ruína~~ (corrigido
    às 16h50: a War Factory não tem ruína; só o quartel da China tem): o prédio sem ruína afundava 80% da ALTURA DA MALHA, que é o topo do
    mastro; os 20% de fora eram o pano da bandeira, não a laje. Agora o prédio sem ruína desce inteiro, como o que tem ruína
    (`StructureSinkFraction` 1,05); o monte de entulho de 30/10 faz a panqueca. Lote `v32_*`.
  - **A lagarta fina no mapa do jogo não é o sulco por tipo de chão** (o `v30_rastros_surf0` igual ao normal). O que mudou desde a v3
    aprovada: a garra caiu de 2 cm pra 0,5 cm em 29/10 (o visual v5 contra o r1, "o sulco do alvo é liso") e a areia nova. O
    `v31_rastros_claw2` mostra a garra de 2 cm no mapa do jogo, pra ele escolher.
  - Conferidos e certos: a cratera com o tanque e a picape assentando (o A/B com `-RTSDisp=0`), a bandeira com a cor do time, o soldado
    sem arrasto, a fumaça do motor ferido igual à de antes, a sucata da carcaça descendo, a obra subindo com o andaime, o rastro da
    Patriot um pouco mais contínuo, o giroflex aceso na cabine e no chão perto. O prédio que nasce escuro nos primeiros 5 s do
    `fx_predio_etapas` é a compilação dos shaders do teste (igual no `v9`).
  - **A v31 conferida** (compilada na fila às 16h10; as folhas `captures/relatorio_30_10_noite/18–22`): os torrões são pelotas de 8–32 cm
    que caem dentro e em volta do buraco (sem estaca nem caco azul); na câmera baixa de lado (`fx_tanque_cratera_lado`, nova) o M1 desce de
    nariz na cratera do míssil de cruzeiro e sobe empinado, e a picape desce no buraco; a sucata com o Dozer encostado na carcaça e dentro
    do monte, os dois descendo até o +$80; os destroços da morte iguais. Detalhes vistos e deixados: a bandeirinha da traseira do tanque GLA
    é do modelo e fica queimada na carcaça; a solda da obra sai em volta do prédio inteiro, às vezes no canto longe do Dozer.
- 31/10, **o pedido dele pro FIM de tudo** ("faz esse teste só quando terminar tudo"): **o teste de leitura das unidades** ("ver se tá
  fácil você entender o jogo, se as unidades estão bem perceptíveis, principalmente o helicóptero, que achei meio escondido"). O método é
  o da sessão do conceito (`docs/CONCEITO_UNIDADES.md` §7): numa batalha na câmera do jogo com todas as unidades (o Comanche junto), a
  visão de longe (10× BOX), o ΔE CIELAB de cada unidade contra o chão em volta e entre as facções (> 20 distinto, < 10 some), o claro e
  escuro (L*), a cor do jogador vista de cima (0,7% some, 5% lê no zoom padrão); e o porquê de cada uma que some, com a proposta medida.
  - **Feito (31/10 ~16h40–17h20)**, `Game/Tools/readability_test.py` (com as caixas de cada unidade: os pedaços ligados juntavam vizinhos
    pela sombra; a cor do time pelo matiz, porque o azul do jogo sai ciano, a* ≈ 4, b* ≈ −37) na cena `fx_leitura_unidades` (+ `_vazio`),
    e o helicóptero também no `gs15_tres_faccoes` contra o alvo aprovado. As folhas: `captures/relatorio_30_10_noite/23–25`.
  - **As 24 unidades de chão leem bem**: a silhueta de longe (a unidade com a sombra, 10×) dá ΔE 32–53 contra a areia; a mais fraca é o
    soldado chinês (24, a farda verde-cinza). **A cor do time é que falha fora da USA**: USA 2–11% (o M1 10,7%), GLA 0–5,5%, China
    0,6–2,3%, os soldados 0,3–0,5% (abaixo dos 0,7% em que some). Numa batalha de duas cores, é ela que diz amigo ou inimigo.
  - **O helicóptero**: na areia aberta ele lê (ΔE 27, time 6,2%), mas **na vila ele some**: o casco é claro (L* 57) na mesma claridade do
    adobe e da areia (ΔL −0,5 a 67 m; −6 a 140 m), só a cor separa, e a cor sozinha não segura forma pequena (a visão de cor tem bem
    menos resolução que a de claro e escuro). Ele não tem os escuros (o mais escuro L* 38) e a sombra dele cai a ~30 m (14 m de altura
    com o sol a 24°), fora de onde ele está. **O alvo aprovado (o game shot das 3 facções)**: o helicóptero USA é cinza médio (L* 32,
    9,5 mais escuro que o fundo) com cabine, motores e rotor escuros (L* 17), e ocupa 19% da largura da tela (o nosso, 8%).
  - **As propostas, na ordem do efeito medido** (pra ele escolher; nada mudado): (1) o casco no cinza do alvo (L* ~32–40, com os escuros
    da cabine, dos motores e do rotor; as faixas azuis ficam), ΔL de 0 pra ~−20 contra o adobe; (2) a poeira do rotor na areia, em volume,
    embaixo dele (o vento do rotor a 14 m, ~1,4 diâmetro, levanta areia de verdade): marca onde ele está, o que a sombra a 30 m não faz
    (o StarCraft II põe uma mancha embaixo do aéreo pelo mesmo motivo); (3) as pás escuras e visíveis; (4) o tamanho (o alvo é ~2×; a
    escala 1,2 foi aprovada em 29/10). Pras outras facções e os soldados: a cor do time nas faces de cima (o que a câmera vê), ≥ 5%.
  - **Feito (31/10 ~17h, o pedido dele não estava cumprido: "o helicóptero meio escondido")**: (1) o Apache no cinza do alvo, só ele
    (`visual.paint_tint` [0,16; 0,165; 0,16] no Units.json, a cor base linear por cima do TankPaint 0,40 de todos os veículos USA;
    `ARTSUnitActor::ApplyPaintTint`): a faixa azul, o metal e o vidro ficam. (2) A poeira do rotor já existe (o vento na areia, o anel
    e a nuvem do brownout), mas a 17,6 m (1,75 diâmetro) ela sai quase nada (S 0,07, 2–3 cartões, o `-RTSAirLog`): ficou como está,
    porque um anel de ~13 m embaixo de cada helicóptero parado cobriria as unidades que ele ataca (jogabilidade antes do realismo).
    (3) O rotor fica: o disco já é a cobertura média das pás borradas (a pá parada do alvo é licença da imagem). (4) O tamanho, dele.
    A cor do time das outras facções e dos soldados é da sessão dos modelos (o elenco, trabalhando neles hoje): a recomendação fica
    aqui, com os números da folha `23`.
  - **A v32 conferida** (compilada às 16h52; a folha `captures/relatorio_30_10_noite/26_apache_cinza_do_alvo.jpg`): o Apache na vila
    a 67 m de L* 57 (ΔE 15, ΔL −0,5 contra o adobe) pra L* 31 (ΔE 32, ΔL −27); a 140 m, de longe (10×), ΔE 18 → 28; na areia aberta,
    ΔE de longe 28 → 36. De perto ele lê como o AH-64 do alvo, as faixas azuis mais fortes no cinza. A bandeira da War Factory desce
    com o prédio (aos 17 s só o monte); a sucata igual à v31. Nada commitado: o cinza do Apache é visual, espera o "aprovado" dele.
  - **A cor do time, conferida contra os alvos aprovados** (a mesma medida, pelo matiz): no `r1_raid_slabs` a GLA tem 0,4–0,9% (a
    bandeira verde), o M1 0%, os soldados 0%; no `15_three_factions` a picape GLA 5,4%, os soldados GLA 3,2%, o M1 1,2%, o helicóptero
    1,7%. O nosso: USA 2–11%, GLA 0–5,5%, China 0,6–2,3%, soldados 0,3–0,5%: igual ou acima dos alvos (os 5% do §7 do conceito são
    mais do que os próprios alvos têm). Nos alvos o dono se lê pela cara da facção e pela bandeira, como no nosso. **Não é defeito**;
    fica a nota pra uma partida espelho (USA × USA), onde só a cor separa: aí a faixa e a bandeira teriam de crescer.

- 31/10 ~22h40, **na avaliação, o pedido novo dele: o canteiro da obra não começada** ("o placeholder da construção não iniciada ainda tá ruim, tinha que ser muros de construção, entulhos, essas coisas"). Hoje, até o Dozer chegar, fica a laje com a caixa escura do andaime e a faixa zebrada, por cima do que estiver no lote (no `fx_obra_em_cima`, a ruína da usina ainda fumegando). O plano: estudo antes (como o ZH marca o prédio posto e não começado; o canteiro de verdade: o gabarito com estacas e fita, o tapume, o material empilhado), e um canteiro provisório por script: o tapume no contorno com a faixa na cor do time, as estacas e o material limpo dentro (paletes de bloco, feixes de ferro, sacos). **Sem entulho**: no jogo o entulho já quer dizer prédio destruído (a sucata, a cobertura do soldado), e a marca da obra não pode ler como ruína. Depois da rodada de avaliação.
  - **O que ele explicou em seguida** ("quando o Dozer chega e TOMA INICIATIVA, começa o progresso (>0%), aí o negócio realmente começa construção, começa tudo, deixa de ser só marcação, precisamos melhorar esse detalhe pra ficar interessante"): as três fases. (1) **Posto, 0%, o Dozer a caminho:** só a marcação: as estacas com a fita no contorno, o tapume com a faixa do time e os montes de material de obra (terra, brita, areia, paletes; o "entulho" dele, no tom de material novo, não o concreto quebrado da ruína). (2) **O Dozer chega e o progresso passa de 0%:** a obra começa de verdade: a fundação, o andaime subindo, a solda, o material sendo consumido. (3) **Pronto:** o canteiro sai.
  - **A régua dele pro canteiro:** "não sei na verdade se gostei do laranja e preto [a faixa zebrada], tô com medo de você fazer algo sem graça, sem qualidade, broxante [...] faça de arrepiar, bem feito, sem parecer jogo low poly, sem parecer jogo de lego". Então: o tapume, as estacas e o material com o detalhe e o desgaste dos prédios aprovados (chapa marcada, poeira, sombra de contato), não caixa lisa; a zebra laranja e preta só se o estudo mostrar que ajuda, e mostrada em A/B com ele.

- 31/10 ~23h20, **na avaliação do item 4 (o soldado), o que ele reprovou e o pedido grande**:
  - "tem uma das mortes que tá horrível, sem sentido nenhum" (os dois de bruços, a mochila pra cima, braços e pernas abertos): é a MM_Death_Back_01, a única que cai de bruços; no fim ela vira boneco de pano e abre os membros. Sai do sorteio (ficam 5). "De resto tá top."
  - "o andar dele tá bem mal feito [...] quando ele começa a andar e quando ele toma tiro as animações se juntam [...] ele DESLIZA o pé": no começo do passo a perna ia mais devagar e mais curta que o corpo (o ritmo × o tamanho do passo, os dois pelo Loco: o pé a Loco² da velocidade); o tranco do acerto (o clipe inteiro a 0,75) puxava as pernas. Agora o ritmo segue o chão percorrido e o tranco só da cintura pra cima.
  - "o rastro no chão tá horrível, parece um carrinho de 1 pneu [...] se não der pra fazer footstep então já era, não tem que fazer gambiarra": a fita contínua sai. Pegada de verdade (a bota carimbada no pé que pisa) fica como item próprio, se ele quiser.
  - "a luz do tiro não segue nunca a saída da arma, ele toma dano, agacha, continua no mesmo lugar [...] você SEMPRE erra nisso": o ponto do tiro do soldado era fixo na altura dele em pé (o pivô + o MuzzleHeight); agora sai da ponta do fuzil na mão.
  - **O pedido grande:** "não consigo entender nada ainda do jogo em relação aos tiros, nem consegui ver quem tava atirando em quem, quando a infantaria levava dano [...] em quem a Technical tá atirando, isso TÁ MUITO RUIM NO NOSSO JOGO, e eu tô falando em QUASE GERAL, tá ruim de LER O JOGO". O estudo de 30/10 (`docs/LINGUAGEM_DO_COMBATE.md`) cuidou do canhão e da metralhadora no veículo; o fuzil e o soldado ficaram de fora, e a metralhadora no soldado também. Próximo passo, depois das correções do item 4: a cena de leitura do combate (Rangers × Rebels × Technical, na câmera do jogo), a medida de cada pergunta da régua (quem atirou, em quem, onde pegou, o que aconteceu), o ZH pelos arquivos do fuzil, uma segunda opinião, o plano e o A/B em vídeo.

- 31/10 ~23h35, **na avaliação do item 5 (a névoa), o que ele decidiu**: "a B quase não tem nada no zoom próximo [...] eu iria de meio termo". A **névoa C**: mais visível que a B e mais fraca que a A no meio, começando mais pra cima; no topo da tela quente como a A. Ele sente "um leve desborramento no game shot" (corrigido por ele: "eu comentei sobre borrar, borramento"): medir se o alvo borra o que está longe (o topo da tela mais macio que o meio, a profundidade de campo da foto) e quanto, antes de pôr qualquer coisa. A régua dele: no game shot os tanques da China estão quase no topo porque a câmera é mais deitada; na nossa as unidades ficam no meio da tela e não podem sumir na névoa (a leitura). **O zoom:** "gostei do zoom próximo ali, podia ser até o default [...] permitir o zoom out máximo só scrollando o mouse". Hoje o padrão é 140 m a 37,5° (o mínimo 10, o máximo 170: `Config/DefaultGame.ini`). O zoom de perto do vídeo da névoa é a câmera do game shot das 3 facções, 88 m a 40° (`gs15_tres_faccoes`): o padrão passa a ser esse, com a rodinha até o máximo. A/B em vídeo depois da avaliação.

- 31/10 ~23h50, **na avaliação do item 6 (aprovado), as melhorias de fora que ele já viu, pra madrugada**:
  - **Os foguetes do Apache** saem errados: atrás da asinha (o casulo dos foguetes fica no pilone de dentro). E a bola branca da saída "mal feita, parecendo PNG": se ficar, menor, mais bonita, em volume e detalhada.
  - **O canhão do queixo do Apache** (30 mm): "um spawn de fogos que ficou até parecendo PNG", não sai na boca (sai muito pra frente). A rajada não chega no tanque ("só sai um monte de luzinha devagarzinho na frente da boca, anda um pouco e nada chega"), e o tanque não mostra acerto nenhum. Gravar de perto, comparar quadro a quadro e pensar o certo (a bala de 30 mm a ~800 m/s, o acerto de 30 mm no casco: o estudo do acerto de tanque).
  - **Todos os que disparam míssil, não só o Apache** ("repare em outros veículos que disparam mísseis, e o Patriot, às vezes eles estão ruins também"): a mesma conferência quadro a quadro na saída de cada lançador (a Patriot, o Avenger e o Linebacker com o Stinger, o Tomahawk, o Rocket Buggy da GLA): sai da boca certa do tubo, a chama e o clarão de saída em volume e no tamanho certo, sem a bola branca chapada.
  - **A cor do vídeo:** ele achou a foto "mais lavada" e o vídeo "mais quente". Mesma receita do look nas duas gravações, e o quadro do vídeo e a foto do mesmo instante batem (medido no `v31_fx_tanque_cratera`). A causa provável é o arquivo: o mp4 sai em `yuvj420p` (a faixa cheia) com a matriz BT.601, sem marcar; os players do Windows e do navegador leem como BT.709 em faixa limitada e esticam o contraste e a saturação (o quente). Conserto no `game.ps1` (o ffmpeg do vídeo): converter pra BT.709 em faixa limitada e marcar o arquivo. Depois do lote em curso, pra não misturar os dois jeitos no mesmo A/B.

- 31/10 ~23h55, **na avaliação do item 7 (aprovado), mais dois pra madrugada**:
  - **As rodas da Technical** (as duas fotos dele, na cena `fx_tanque_cratera`: a roda da frente fora do lugar, abaixo e atrás do para-choque, e um pneu meio enterrado na frente): "que merda é essa [...] bota na lista pra arrumar isso". Provável que seja meu: o casco assenta na cratera (`SeatOnCraters`) e as rodas seguem outra conta. Conferir quadro a quadro; se for do modelo, é da sessão do conceito (ele: "ver se ela já não está fazendo, ou pedir pra ela").
  - **A poeira atrás dos veículos não vê a cratera** ("parece que a cratera não existiu pra poeira saindo atrás"). O certo: (1) a poeira nasce onde a lagarta e o pneu estão, no fundo do buraco (hoje nasce na altura do chão de antes, flutuando em cima dele), e a parede da cratera tapa o começo dela até ela subir e sair; (2) a terra da cratera é a mais solta (pulverizada pela explosão): passar por dentro e subir a borda levanta mais poeira e mais escura (o tom da terra revirada), com a lagarta patinando na subida.
- 31/10 ~0h, **item 8: a garra de 2 cm escolhida por ele** ("óbvio que é o com dente"): o padrão do `rts.Ground.ClawCm` volta de 0,5 pra 2 cm (madrugada, com o resto).
- 01/11 ~0h, **item 9 aprovado, e o que ele lembrou do item 6, pra madrugada (as explosões e as fumaças)**:
  - **A cor:** "por que é amarelado? não faz sentido com fuligem [...] vamos pensar o que seria foda". A regra dele de 13/10 já diz: a fuligem é cinza-carvão neutra, o amarelo é só a areia que a onda levanta. Hoje a areia sobe junto até o alto e o sol quente + a luz do fogo pintam tudo de bege-laranja. No helicóptero caindo não há areia nenhuma no ar e ele sai bege.
  - **O congelar-e-andar:** "a explosão de qualquer coisa chega num estágio que ela CONGELA e simplesmente começa a andar pra um lado". É o fim da gravação: o último quadro parado, crescendo e indo com o vento (a "nuvem viva"). O meu conserto do fim do fogo da sucata é o mesmo jeito: desfeito (`BlastFireHold` volta a 0 de padrão). O certo é a fumaça continuar se mexendo depois da gravação (estudo antes: gravações mais longas até sumir, ou o último quadro carregado por um fluxo turbulento que enrola, estica e rasga).
  - **O helicóptero e o avião caindo:** "a fumaça segue o avião como se ele tivesse no chão [...] o avião tá girando, causando turbulência, a fumaça parece estática, não se deforma". No código: o fogo da queda é a gravação do fogo de chão presa na aeronave (anda junto, rígida). O certo: a chama presa no motor e a fumaça deixada no ar por onde ela passou (o rastro, como o do míssil, em volume e preto), torcida pelo giro e pela esteira do rotor.
- 01/11 ~0h10, **item 10: a bandeira e o tiro no prédio aprovados; pra madrugada**:
  - **As rodas da Technical** também no `v30_fx_bandeira` (fora da cratera): o defeito é geral, não do assentar na cratera.
  - **O tiro no prédio:** cada atirador escolhe o seu ponto no prédio e troca pra outro ponto sorteado a cada X tiros (não todos no centro), e o dano (os furos, a fuligem, as etapas) nasce onde os tiros caíram, não em lugar sorteado. "Pense além do que eu falei": o lado que apanha mais cede primeiro, a fumaça e o pó saem dos furos do lado dos atacantes, o desabamento pende pro lado mais ferido.
  - **O desabamento por etapas de verdade** (hoje ele desce inteiro): "se você conseguir essa melhoria FODA de realmente por etapas, PERFEITO" (ele acha difícil; é o bônus).
  - **Os pedaços que caem do alto depois que o prédio já está no chão** ("spawna pedra do nada lá de cima e cai"): o entulho que cai tem de cair junto com o prédio descendo, de onde a parede está naquela hora, e acabar quando ele acaba.
- 01/11 ~0h15, **item 11: o Apache cinza "pode ser, mas achei ele muito, muito queimado"**: a tinta de 0,16 (L* 31) pra 0,24 (~L* 42 pela medida, ~15 abaixo do adobe): o cinza médio. Já no `Units.json` (o jogo lê ao abrir a partida); provar em vídeo na vila.
- 01/11 ~0h20, **item 12: o recuo, o tombo e o Tomahawk aprovados; pra madrugada**:
  - **A fumaça e o fogo do motor do veículo ferido** "ficou bom, mas tá com o mesmo problema do Apache: ela é fixa ali": presa no veículo, rígida. O mesmo conserto da queda da aeronave: a chama no motor e a fumaça deixada no ar por onde ele passa, levada pelo vento e pela esteira.
  - **O veículo ferido parece intacto, só pegando fogo**: "o C&C ZH tem uma forma clássica de deixar ele bem danificado, danificado [...] olha como os RTS modernos lindos fazem, aplica essa melhoria". Estudo antes (o ZH: os estados DAMAGED e REALLYDAMAGED do modelo; os RTS de hoje); a marca de dano no casco saiu em 14/10 por estar "porca" (camuflagem pintada): tem de ser de verdade (chapa amassada e queimada, peça solta caindo, fuligem do furo).
  - **O tiro da Technical**: ele não viu ("cadê?"). Gravar a Technical atirando de perto e na câmera do jogo e mostrar.
  - **A sombra quadrada** no `v30_v4_siege` ("parece que ainda tem, pode ser impressão minha"): conferir quadro a quadro.
- 01/11 ~0h25, **a causa das rodas da Technical** (quadro a quadro no `v30_fx_bandeira`, 1,85–2,1 s): a roda não deita nem sai do lugar, ela AFUNDA na areia até o cubo quando a picape passa na ondulação. O veículo assenta no chão assado da simulação (e na cratera, desde ontem), mas o chão desenhado (o deslocamento Nanite com a ondulação da duna nova) fica mais alto ali. Conserto: o `VisualGroundZCm` e o `SeatOnCraters` leem também a altura da ondulação no ponto (a mesma conta do material, na CPU, como a da cratera).
- 01/11 manhã, **o que a madrugada e a manhã fizeram (o /goal: o transporte, a fila do Alt e "todos os ajustes"), pra ele avaliar**:
  - **A fila do Alt (simulação 35):** segurando o Alt, toda ordem de andar, atacar (inclusive outro tanque), o tiro forçado, a obra, o conserto e a sucata entra no fim da fila de cada unidade (até 16); o Dozer com Alt paga e marca as obras na hora e faz uma depois da outra; ordem sem Alt substitui e apaga a fila; S apaga; Alt + clique rápido do direito tira o último ponto. A linha da fila (verde andar, vermelho atacar, âmbar o trabalho do Dozer, os anéis nos pontos) só aparece com o Alt apertado (o pedido de 01/11: soltou, some). Teste da simulação: A → B → C, o 17º recusado, o ataque na vez dele, 3 obras pagas e feitas. Prova: `captures/game/v34_fila_alt` (os tanques) e `v34b_fila_alt` (o Dozer, com as usinas a 24 m: a 16 m elas não cabiam).
  - **O transporte (simulação 35):** o Stryker leva 4 (o soldado ocupa 1); clicar no próprio Stryker com soldado selecionado = entrar (sem vaga, para do lado); dentro, ninguém mira, empurra, esmaga nem acerta com dano de área; a tropa atira de dentro com a arma dela, o alcance contado do Stryker (o Javelin acerta tanque e helicóptero); o ataque dado ao Stryker passa pra quem acerta o alvo e ele chega até o menor alcance dos que acertam; D (ou o botão) desembarca um a cada 0,25 s pela traseira; ele morre, morrem todos (o Humvee do ZH). Os quadradinhos de lotação embaixo da barra. Teste da simulação (`TestTransport`): tudo passa. Prova: `v34_stryker_javelin`.
  - **O pedido de 01/11 (manhã), no transporte:** (1) o Stryker sozinho na seleção mostra as vagas no painel (quem está dentro com foto e vida: clicar tira só ele; as livres apagadas; D tira todos); vários selecionados, as unidades como sempre. (2) A tropa fica escondida: o tiro sai da escotilha da vaga, que abre (a tampa gira na dobradiça e aparece o vão escuro) e fecha 2 s depois do último tiro; o Javelin sai no lançamento suave de verdade (empinado 55° pela escotilha, a ignição pequena) e emenda no voo. Prova: `v34b_stryker_javelin`.
  - **O bazuqueiro** já existia (o `usa_javelin` com o lançador no ombro, do elenco); entra no Stryker e atira de dentro.
  - **A falha do teste da IA com o ar** (achada no simtest): não é bug; a sucata encostada de 31/10 muda um pouco a economia e essa semente acaba aos 12,3 min com 1 Raptor (antes empatava aos 20 min com 2+). Conferido passo a passo que o Dozer não trava; o teste agora pede 1 aeronave.
  - **O Apache:** o canhão do queixo vira a arma leve (o clarão e a rajada até o alvo, as faíscas no casco), na boca do modelo (3,65 m à frente, 0,43 m de altura; era a boca padrão de 5 m e o sopro grande do tiro de tanque: o "fogo de PNG"); o Hellfire nasce na PONTA do míssil guardado (a ponta do corpo desenhado vai no ponto do voo: ele nascia meio corpo pra trás, "atrás da asa"); a saída do trilho com a ignição pequena na tubeira, sem a bola de 1,2 m e a tampa voando da caixa da Patriot. Prova: `v34b_apache_ataque`.
  - **O tiro no prédio:** cada tanque mira o ponto dele na fachada virada pra ele (o lado e a altura sorteados) e fica nele 3–6 tiros; o traçante termina no ponto e o acerto (o pó, os pedaços, a marca) nasce ali; o 1º e o 3º furo da vida baixa abrem onde a marca mais nova está (os outros no telhado). O desabamento em 3 etapas (o andar cede acelerando, uma pausa com o pó, o próximo) e o entulho cai da altura que ainda está de pé na hora em que cai (antes despencava do alto com o prédio já no chão). Prova: `v34b_predio_tiros`.
  - **A névoa C** (`v34b_gs15_A/B/C/A2`, a mesma corrente): começo no foco (1,0×), 0,85 da densidade, a rampa até o topo com o véu da A (FogFade 1,5) e o espalhamento 0,45. **O "borramento" medido:** a nitidez do topo do game shot é ~85% da do pé da tela (o laplaciano sobre o contraste local); no nosso, 92%. Existe, mas é leve, e a nossa imagem inteira já é mais macia que o alvo: borrar mais pioraria; o que falta é o alvo ser mais nítido no meio e embaixo.
  - **O giroflex v3:** de dia a v2 não aparece nada (`v34_dozer_giro_dia`). A conta: pra se ler na areia ao sol o facho precisa somar ~15% da luz dela (18 000 lux) a ~8 m: ~1,2·10⁶ cd no pico; a cúpula 4·10⁴ cd. A/B no `v34b_giro_dia_v3` × `_v2`.
  - **Atenção, o disco:** o C: chegou a 2,3 GB livres de madrugada e o cache de shaders recusou gravar: as cenas do Dozer da v33 saíram com o chão xadrez (inválidas, refeitas na v34). Movi 3,8 GB de rascunho meu pro `E:\movido_do_C_01-11\scratch_pendencias` (nada apagado). Fica com ~6 GB.
- 01/11 ~13h, **mais achados da manhã** (pra próxima rodada):
  - **O "quadrado" do cerco** (`v30_v4_siege`, 17,5–18 s, a fumaça escura da explosão do Tomahawk perto da Patriot): ao se desfazer, a nuvem vira um anel com forma de quadrado arredondado; o miolo some primeiro e a borda fica. Suspeita: a fumaça acumulada nas paredes do domínio da gravação (o `BoxFade` do `BlastRaymarch.usf` só esmaece 5% da borda). Confirmar medindo o tamanho do anel contra a caixa da gravação antes de mexer no shader (que é de todos).
  - **O desabamento em etapas** acontece (`v34b_predio_tiros`), mas fica escondido pela bola de fogo da morte do prédio (as 3 etapas caem dentro dela). Próximo: começar o desabamento ~1 s depois e esticar as etapas pra ~5 s.
  - **O Apache "ruim de ver"** (a pergunta dele): no game shot alvo ele ocupa ~19% da largura da tela, no nosso ~8% (`captures/relatorio_01_11/i_apache_alvo_vs_nosso.jpg`). A tinta (cinza-claro com as faixas azuis) é a mesma do alvo; o que falta é o tamanho (o ZH voa a 26 m, o nosso a 14 m: a 88 m e 40° de câmera, 26 m deixa ele ~1,4× maior e a sombra mais longe, lendo "no ar"), as pás escuras visíveis (no nosso, um disco claro borrado) e faixas azuis maiores. Proposta em A/B pra ele escolher.
  - **O Elenco trocou o caminho do Apache** pra `/Game/Units/usa_comanche_v2` (12:41), ainda não importado: até importarem, o Apache não aparece no jogo.
  - **Pedidos de imagem pra ele** (01/11, anexos em `captures/concept/_attach/pend_0*`): o ícone de desembarcar, o da vaga vazia, o botão do Javelin, o cartão do Stryker, o botão do Apache e o conceito do canteiro.
- 01/11 ~14h30, **a tarde** (depois do reinício do PC):
  - **A v35** (tudo de hoje numa corrente: o transporte, o canteiro, a leitura do tiro, a fila do Dozer, o prédio com o desabamento 1 s
    depois e em 5 s, o Apache, o fim do fogo, a cratera, o giroflex v3 apontado pro chão em volta) compilou às 14h12; as gravações
    esperam a placa (o Elenco importando modelos).
  - **O Apache "ruim de ver", o A/B (v36)**, só por chave de teste: `-RTSApacheAltM=24` (a altura de cruzeiro; o ZH voa a 26,3 m, o
    nosso a 14: mais perto da câmera, ~1,3× maior na tela, e a sombra longe, lendo "no ar") e `-RTSRotorFreeze=1` (a pá de verdade à
    vista no giro cheio, "congelada" pela câmera onde está a pá fantasma do disco, escura e nítida como no game shot alvo; o efeito
    estroboscópico de foto e vídeo). Na câmera do jogo (`gs15_tres_faccoes`): B alto, C alto + rotor, D só o rotor, A2. As faixas azuis
    maiores e o tamanho da malha são do Elenco (o modelo `usa_comanche_v2`, já importado): proposta, não mexi.
  - **O "quadrado" do cerco, conferido quadro a quadro** (`scratchpad/ring/ring_sheet.jpg`, 15,5–18,6 s): não é impressão, as DUAS nuvens
    pretas viram um anel de quadrado arredondado ao se desfazer. A gravação tem as paredes abertas (só o chão fecha:
    `blender_pyro_tank.py`), então não é fumaça presa na parede. Suspeita agora: a saia de fuligem rente ao chão cortada pela caixa (o
    `BoxFade`: 5% nas laterais e o círculo de 72–98%) e a coluna do meio saindo pelo teto aberto, o miolo ralo. Cena isolada
    `fumaca_anel` (o Tomahawk e o ataque pesado na areia aberta, na câmera do jogo, até sumir) na v36 pra medir antes de mexer no shader.
  - **A fumaça "fixa" do veículo ferido e da aeronave caindo**: o conserto já estava no código (de manhã): andando, a fuligem da gravação
    do fogo dilui (a chama fica) e a fumaça sai em volume deixada no ar (o mesmo sistema de toda fumaça). Faltava a prova: na v36
    (`fx_veiculo_ferido`, `air_fx_crash`).
  - **Os estados de dano do veículo** (item 12): o estudo (o ZH, os RTS de hoje, o tanque atingido de verdade) e o protótipo do
    "destrutor de veículo" no Blender, como o dos prédios (`model_damage.py`), em andamento num agente, só na CPU (o doc
    `docs/ESTUDO_DANO_VEICULOS.md`, o script `Game/Tools/model_damage_vehicle.py`, as imagens em `captures/dano_veiculos/`).
- 01/11 ~15h, **dois pedidos novos dele** ("mais umas coisas pra colocar na lista"):
  - **O botão ao lado do minimapa vira o do trabalhador** ("pra clicar pra ir pra próxima unidade de construção liberada; se não tiver
    nenhum, desabilitado"). Feito: a foto do construtor da facção (a do painel; o ícone pintado no estilo dos outros 3 fica pra pedir
    no Atelier), apagada e sem clique sem nenhum parado; com algum, o número no canto (o "idle worker" do StarCraft II); cada clique
    seleciona o próximo parado (sem obra, conserto ou sucata, sem ordem e sem fila) e leva a câmera até ele; o Ctrl pega todos. Sem
    tecla por enquanto: o "." do Age of Empires já é a última casa da 2ª fila do painel (ZXCVBNM,.); a tecla é escolha dele.
  - **A lotação embaixo da vida** ("ficou só quadradinhos mal feitos, mal pensados"): os pips soltos do ZH pediam legenda. Agora a
    plaquinha presa embaixo da barra de vida (a mesma moldura do time: as duas leem como uma peça só) com um bonequinho por vaga (a
    cabeça e os ombros), aceso quem está dentro, apagado a vaga livre (o garrison dos RTS de hoje).
- 01/11 ~15h40, **a v35 conferida até onde a queda de energia deixou** (o PC reiniciou às ~15h25 no meio do Apache):
  - **O Javelin de dentro do Stryker** (`v35_stryker_javelin`, quadro a quadro): a escotilha abre, mas o míssil nascia ~4 m à frente
    (o voo da simulação desde o 1º quadro, só a direção empinada), o clarão da saída lavava o deck de branco e a nuvem da caixa da
    Patriot envolvia o carro. Refeito o lançamento suave de verdade: sai do tubo a 12 m/s sem chama por 0,22 s e o motor de voo acende
    no ar (a chama, a luz e o rastro nascem ali), sem a nuvem e sem a poeira do chão. Prova na v36.
  - **O canteiro** (`v35_canteiro`) saiu vazio: a War Factory pede a Supply Center (que pede a Power Plant) e a cena só tinha o Dozer.
    A cena ganhou as duas prontas longe da câmera. Na `v35_fila_alt` o canteiro aparece nas 3 obras da fila (o tapume, as estacas, o
    material) e a linha da fila do Alt liga as obras.
  - **O prédio** (`v35_predio_tiros`): os furos e a fuligem nascem do lado atingido (bom); as 3 etapas acontecem, mas as 3 explosões
    da morte (0,35–1,35 s) sobem por dentro da planta e tapam o desabamento de novo. Conserto pronto pra depois da v36: o prédio cede
    primeiro (0,3 s, o pó de cada andar, o 1º inclusive) e as explosões viram as detonações no entulho (3,4–4,7 s).
  - **O disco**: o atualizador do Visual Studio (BackgroundDownload, desde 14h28) encheu ~4 GB no Temp; o cache de shaders recusou
    gravar 11 vezes na fila_alt (a foto saiu certa). Não mexi no atualizador (é do sistema).
  - **Os estados de dano do veículo** (o agente): o estudo `docs/ESTUDO_DANO_VEICULOS.md`, o destrutor `Game/Tools/model_damage_vehicle.py`
    e as imagens do M1 (intacto, danificado, destruído) em `captures/dano_veiculos/`. O destruído lê bem na câmera do jogo; o danificado
    é moderado (o ZH nem muda aos 70%). Falta ligar no jogo (a troca do casco e da torre aos 70/35%, os pontos de fogo por veículo e a
    rampa da fuligem no material das unidades): esperar ele ver.
- 01/11 ~16h50, **a v36 conferida quadro a quadro** (`captures/relatorio_01_11/o_*`, `p_*`, `q_*`), e o que entrou na v37:
  - **O Javelin de dentro do Stryker, provado** (`p_javelin_suave_v36.jpg` × `p_javelin_antes_v35.jpg`): os 4 saem devagar das escotilhas
    abertas, empinados e sem chama, o motor acende no ar em cima da torre e o rastro nasce dali; o deck não lava mais de branco.
  - **A lotação** (a plaquinha com os bonequinhos) lê, mas no tamanho do jogo o boneco parecia peão de xadrez e a vaga livre (cinza
    médio) confundia com a ocupada: o pescoço maior, os ombros mais largos e a vaga livre vazada (contorno claro, miolo escuro). v37.
  - **O Apache, o A/B** (`o_apache_ABCD.jpg`, `o_apache_C_rotor_movimento.jpg`): A hoje (14 m, o disco claro), B 24 m, C 24 m + as pás,
    D 14 m + as pás. **O que faz ele ler são as pás** congeladas pela câmera (as 4 pás escuras com a ponta amarela, como no alvo; no
    vídeo giram devagar, ~40°/s, o estroboscópico das filmagens); a altura quase não muda o tamanho na tela e empurra ele pra borda de
    cima. A minha indicação: a D (as pás, a altura de hoje). A escolha é dele.
  - **A leitura do tiro** (`v35_leitura_tiro_leve`): a Technical ficava pálida e sumia numa nuvem branca. Duas causas: (1) o pulso do metal
    (o meu A5 de hoje) com 2,5·10⁴ cd a 60 cm dava ~69 mil lux, ~4× o sol: agora 9·10³ cd a 1 m (+50% do sol no ponto); (2) **uma
    regressão da areia da metralhadora aprovada em 31/10**: o "O.Layer = 0" ficou depois do véu gravado e o desligava, deixando o puff
    simples com a janela de 4,8 m da gravação (a nuvem branca de ~4 m). A ordem corrigida: volta o véu do EmberGen na massa 0,5.
  - **O míssil da Patriot "azul de brinquedo"** (`v36_lanca_ar`): era a silhueta de raio-X do Avenger (escondido atrás da caixa)
    pintada "através" do míssil que passava na frente. O corpo do míssil agora grava a profundidade própria (a marca 0): a silhueta
    não sai onde ele está.
  - **Os lançadores abertos** (`v36_lanca_chao`): o casulo do Rocket Buggy soltava as tampas e a nuvem da caixa da Patriot a cada
    salva (a nuvem envolvia a picape), com a bola branca em cada foguete. A saída de caixa fechada ficou só com a Patriot
    (`launch_canister` no Units.json); o casulo, o tubo do Stinger, do Avenger e do soldado acendem pequeno na tubeira, sem tampa,
    sem a nuvem da caixa e sem a poeira do chão.
  - **O fogo do motor andando** (`v36_fx_veiculo_ferido`): seguia um bolo preto preso no deck (a fuligem da gravação, gravada pequena e
    4× espessa, a 20% ainda é opaca). Andando agora ela vai a 3% (fica a chama) e a fumaça é a do rastro no ar; o mesmo na aeronave
    caindo. A queda do Apache (`v36_air_fx_crash`): a chama no motor e a fuligem deixada no ar, curta (14 m de queda).
  - **O canteiro** saiu vazio de novo: a War Factory não cabe em (−300, 100) (as pedras da encosta; a simulação recusa sem dizer). A cena
    foi pro ponto plano da cena do prédio, (−296, −313). A v37 também grava a `fila_alt` (o HUD com o botão do trabalhador).
  - **O "quadrado"** não saiu na cena isolada (o míssil solto do laboratório dá uma explosão pequena e a fumaça sai da tela): a v37
    refaz o cerco (`v4_siege`, semente 12) pra ver o anel no lugar onde ele apareceu.
- 01/11 ~19h30, **dois pedidos novos dele pra lista**:
  - **O andaime fica dentro, no centro da obra** ("precisa ficar em volta da construção"): o andaime de cada modelo abraça só o bloco alto
    (a War Factory: o anexo, as chaminés e os tanques ficam de fora). Feito no meu lado, pra todo prédio: o jogo monta o andaime em volta da
    caixa inteira do casco, vão a vão (a peça nova `SM_Site_ScaffoldBay` do `model_site_props.py`: 3 m de vão, 1 m de fundo, 2 m de lance,
    tubo, assoalho de tábua, guarda-corpo, rodapé zebrado, diagonal), 30 cm fora da parede, lance por lance com a obra, no lugar da malha de
    andaime do modelo (`ARTSUnitActor::UpdateScaffoldRing`; `-RTSScaffoldRing=0` volta o de antes). O tapume do canteiro foi pra 2,2 m do
    contorno (fora do andaime). Prova na v38 (`v38_canteiro`).
  - **A terraplanagem embaixo do prédio no terreno inclinado:** o lado que ficaria no ar tem a base sólida embaixo ("lindo, perfeito, pode
    continuar"); o lado que fica abaixo do chão some, tapado pelo terreno. O certo: ali o terreno é aplainado até a base, natural, como o
    trator deixa (o corte com o barranco em volta); o resto mantém o chão sólido. O plano: o material do chão dos mapas (o mesmo lugar do
    recorte do túnel, `Game/Tools/ground_cut.py`, a coleção `MPC_Ground`) ganha o rebaixo: em cada prédio, o terreno acima da base desce
    até ela dentro da planta + 0,5 m e sobe no barranco de ~35° até encontrar o terreno; o terreno abaixo da base não muda (a base sólida
    continua). Os dados por prédio: o centro, a meia-largura e o giro que o recorte já manda, mais a altura da base (os dois números livres
    do Rot0..7). Só desenho (a simulação anda no chão assado; dentro da planta ninguém anda). Risco: é o material do chão de todos os mapas
    (o "xadrez" de manhã): fazer na fila, com o jogo fechado, e A/B do custo e da forma num mapa com encosta.
- 01/11 ~20h10, **a terraplanagem, o que o código da Unreal permite** (antes de escrever qualquer coisa):
  - **Mexer o chão pelo material não dá:** o Landscape Nanite desliga o WPO de propósito (`LandscapeNaniteComponent.cpp`: "We don't
    want WPO evaluation enabled on landscape meshes", no construtor e de novo no PostLoad), e o deslocamento da tesselação que já existe
    vai só ±1 m e só nas janelas do caderno em volta da câmera, no Alta/Ultra. Uma encosta pede 2–4 m de corte e em todos os presets.
  - **O caminho que sobra, sem costura:** (1) o material do chão some, por pixel, onde o terreno passa da superfície do corte (a conta
    exata: a base do prédio dentro da planta + 0,5 m e o barranco de ~35° pra fora; a mesma máscara do recorte do túnel, `ground_cut.py`,
    com a altura da base num dos números livres do Rot); (2) uma malha do corte (o fundo plano e o barranco), gerada no jogo pela mesma
    conta, aparece só onde ela fica abaixo do terreno. As duas superfícies se encontram exatamente na linha do corte (a crista do
    barranco): não há degrau, só a troca de material, que no mundo real também existe (a terra cortada pelo trator é mais escura,
    compactada, com as marcas da lâmina). Embaixo da base nada muda (a base sólida continua).
  - **Os riscos:** é o material do chão de todos os mapas (o xadrez de manhã foi isso); o recorte hoje tem 8 vagas no mapa inteiro
    (precisa de mais: só o prédio na encosta ocupa uma); o custo da máscara por pixel. Fica depois dos itens da avaliação, na fila e com
    A/B num mapa com encosta.
- 01/11 ~20h30, **o "quadrado" do cerco, a causa achada** (`v37_v4_siege`, 15,8–17,4 s, `scratchpad/v37_ring.jpg`): a nuvem preta do
  Tomahawk esvazia por dentro e fica o anel com a borda da caixa. A fuligem afinada da ogiva (o SootKeep: 0,10 no Tomahawk) e o fim da
  nuvem da morte iam pela Opacity, que no raymarch multiplica a densidade ANTES da erosão da borda (`BlastRaymarch.usf`, SampleInst):
  baixar ela é um limiar subindo, e o miolo mais ralo da gravação some primeiro (o mesmo defeito que a nuvem viva já tinha resolvido
  em 31/10 só no fim dela). Conserto só no C++ (o shader de todos não muda): o afinar e o fim de todas as nuvens vão pela diluição
  (linear, depois da erosão: a mesma massa se espalha e rareia por igual), também no volume clássico. Compila (espelho); entra na
  compilação da v38 e a prova é a `v39_v4_siege`, a mesma semente.
- 01/11 ~20h30, **as infantarias das 4 facções** (o pedido dele: "no C&C você via uma infantaria independente da cor e sabia de qual
  facção era"): a USA no coyote com o capacete ACH e o suporte do visor noturno; a China no verde fechado com o gorro de pele (o topo
  redondo mais largo que a cabeça, a aba da frente com a estrela, as abas dos lados); a GLA no cáqui com o pano claro na cabeça e o
  lenço no pescoço, sem mochila; a Rússia com peças próprias (eram as da China): o cinza-esverdeado, o capacete grande 6B47 com a faixa
  e os óculos e a gola alta do colete. Importadas e ligadas no Units.json (as 9 unidades de infantaria). A cena nova
  `infantaria_4_faccoes`: as 4 lado a lado, todas do MESMO jogador (a facção tem de ler pela forma), de perto e na câmera do jogo.
- 01/11 ~21h, **o anel do cerco, 2ª volta** (a 1ª não mudou nada: `v39_v4_siege` igual à v37 quadro a quadro,
  `captures/relatorio_01_11/final/11_anel_v37_v39_igual.jpg`). Pelo log da partida a nuvem é o **fogo da Patriot destruída se
  apagando** (o fogo simulado 1 nasce com a carcaça dela e "apagou aos 10,1 s"), não a ogiva. O apagar do fogo (FireOut) e o fim da
  gravação dele iam pela Opacity (a Keep do raymarch, antes da erosão: o miolo ralo some primeiro). Agora vão por um esvaecer linear
  novo (`AExplosionVolume::RaymarchFade`, × a diluição no raymarch, no volume clássico e na luz do fogo). A 1ª volta foi desfeita (a
  ogiva e a morte voltam a ser as aprovadas). Compila (espelho); prova na `v40_v4_siege`.
- 01/11 ~21h, **o pedido dele: "só a cor não é o principal [...] muda o formato, a roupa [...] e os tipos de infantaria, cada um tem
  seu jeito"** (registrado como REPROVADA em `docs/APROVADOS.md`). Feito:
  - a folha de ideias (1 imagem do GPT, `captures/concept/90_infantaria_9_tipos_folha_de_ideias/raiz_r1_gpt.png`): os 9 tipos, cada
    um com a forma dele, todos com a mesma cor de time;
  - o kit por tipo no `soldier_dress.py` (`KITS`, o 4º argumento = o tipo): USA fuzileiro compacto (capacete com visor noturno,
    colete, mochila pequena) e o Javelin com o tubo reserva de 1,1 m na diagonal; China fuzileiro de **casacão até abaixo do
    joelho** com cinto e cartucheiras e o gorro de pele, e o Tank Hunter com o capacete de óculos e **3 foguetes de pé acima dos
    ombros**; GLA trabalhador de **túnica clara, turbante alto, saco no ombro e pá**, sem arma nem colete, o Rebel de túnica cáqui,
    lenço claro e colete de bolsos, e o RPG de pano escuro na cabeça e **3 ogivas verdes acima dos ombros**; Rússia fuzileiro
    volumoso (6B47 com óculos, gola alta, placa da virilha, mochila) e o Kornet com **o tubo do míssil e o tripé dobrado** nas
    costas. A barra do casaco e da túnica é presa em metades nas coxas: parado lê como uma peça, andando abre na frente e atrás;
  - a folha dos 9 no Blender: `captures/relatorio_01_11/final/16_infantaria_9_tipos_blender.jpg`. Importação dos 9 na fila e a
    cena `infantaria_4_faccoes` (agora GLA = Worker, Rebel, RPG) gravando em `v40_infantaria_tipos`.
- 01/11 ~21h15, **o pedido dele: "a fuligem preta de queimado [...] nas unidades e construções que levaram um impacto explosivo de
  mísseis, por alguns segundos"**. O que já existia: cada acerto de míssil grava no casco onde e quando bateu (`MarkHull`, o tipo 1:
  a ogiva e o vizinho da explosão) e o material das unidades desenhava só a mancha permanente de 90 cm (a 67 m, quase nada); o
  material dos prédios ignorava o tipo 1. Agora a bola de fogo deixa na face atingida a fuligem quase preta e fosca (~3 m nas
  unidades, `Tools/hull_marks.py`; ~4 m nos prédios, `Tools/import_provisional.py` M_Structure), manchada, cheia até 1,5 s e
  clareando até 6 s, quando fica a marca que já ficava (nos prédios, ~1,6 m). Na fila: os materiais e a cena nova `fuligem_missil`
  (o Apache ataca a Power Plant da China e depois o tanque da GLA, de perto), `v41_fuligem_missil`.
- 01/11 ~21h30, **mais dois pedidos dele sobre a infantaria** (os dois registrados como REPROVADOS em `docs/APROVADOS.md`):
  - **"sem qualidade, sem rosto ainda, mal feitas [...] nos GLA não tem que ter bagulho militar, os caras são rebeldes"**: o rosto em
    todos (o nariz e as orelhas de peça, os olhos, a sobrancelha e a boca pintados; os óculos do americano, a balaclava do russo, o
    lenço cobrindo do nariz pra baixo do rebelde e do RPG, a barba do trabalhador); a GLA de roupa civil (sem o colete de bolsos, as
    bolsas militares e a joelheira: a túnica, o colete civil aberto, a bandoleira de couro cruzada no peito, a faixa de pano na cintura
    e o sapato de couro); o acabamento (as dobras na barra do casaco e da túnica e a bainha irregular, a trama e a mancha de uso no
    pano, o pó da areia subindo do pé até o joelho).
  - **"as infantarias com armas pesadas tipo as bazucas estão na mesma posição de armas leves"**: o lança-foguetes vai no ombro (o tubo
    apoiado em cima do ombro direito, 40% dele atrás, apontado pra mira; andando, empinado ~25° no ombro) e o antitanque ajoelha no
    combate (o último tiro há menos de 2,5 s). Achado junto: na foto do jogo o tubo do Javelin aparece de pé, apontando pra baixo (o eixo
    do modelo do lançador na Unreal não é o do fuzil): medindo a caixa das 3 malhas na Unreal antes de fechar (`probe_launchers.py`).
- 01/11 ~21h45, **a 2ª volta dos 9 tipos no Blender** (`captures/relatorio_01_11/final/17_infantaria_9_tipos_v2_blender.jpg` e os rostos
  em `17b_rostos.jpg`): o rosto em todos, a GLA civil (a túnica do trabalhador num cinza-azulado desbotado: a areia clara lia como
  pele), a bandoleira na diagonal do ombro ao quadril. Importação dos 9 na fila (junto, a medida dos eixos dos lança-foguetes) e a
  gravação `v40_infantaria_tipos`.
- 01/11 ~22h10, **o lança-foguetes no ombro, medido**: as 3 malhas na Unreal têm o eixo do fuzil (a frente em +Y, o punho na origem, o
  tubo de −6 a +24 cm em Z: `probe_launchers.py`); o "tubo de pé" da foto era o tubo apontando pra câmera. O ombro vai como planejado
  (o pé do tubo no alto do ombro, ~10 cm acima da junta). Compila (espelho); entra na compilação da v40. **E ele reprovou a 2ª volta da
  infantaria** ("tá achando que é jogo de lego? [...] termina o que tá fazendo aí e depois dá uma atenção nisso"): terminar a fila e
  refazer do zero (o estudo `docs/ESTUDO_INFANTARIA_MODELO.md` em andamento num agente: como os RTS de hoje fazem o soldado e os
  caminhos possíveis pra cá, com fontes).
- 01/11 ~22h, **a terraplanagem fica com o Elenco** (o item 22 da lista dele, "o prédio recorta o chão", com a decisão dele de 19h20:
  CAVAR de verdade pelo caderno do chão, o mesmo da cratera). O meu caminho (a máscara do chão + a malha do corte) não funciona: o
  terreno Nanite tira a máscara de opacidade de propósito (`LandscapeRender.cpp`, achado do Elenco de manhã: "o buraco não abre"). A
  minha v42 religou por engano a máscara nos 9 mestres do chão (o `ground_cut.py` achou o nó tirado e pôs de novo): sem efeito na
  imagem nem no custo (o Nanite descarta), mas desfaz o estado que o Elenco deixou. Desfeito: o C++ da terraplanagem saiu, o
  `ground_cut.py` voltou ao dele e a máscara sai de novo na fila (`GROUND_CUT_UNDO=1`). O pedido dele (o lote aplainado no barranco,
  a base sólida embaixo do lado que fica no ar) vale pro caminho do Elenco.
- 01/11 ~22h, **a bandeira dos EUA nos prédios** (ele, com a foto: "que porra é essa kkk"): o pano da bandeira com física do Elenco
  (o item 23 dele, `visual.flags` na War Factory, o Barracks, o Command Center e o Strategy Center) sai no jogo como uma fita de ~6 m
  ondulando (no Units.json o pano é de 2,4 × 1,45 m); de longe as listras viram rosa. É do Elenco; pergunto a ele se eu conserto.
- 01/11 ~21h50, **a v40 (o cerco que provaria o anel) não gravou**: a lista de uma cena só no PowerShell achatou (a armadilha da "lista de
  uma") e ela rodou a pasta "v"; parada pelo PID, a fila liberada. O cerco vai na v44.
- 01/11 ~22h50, **dois tropeços meus e o disco**:
  - **O Units.json derrubou as partidas por ~40 min (22h16–22h55)**: a unidade de teste do soldado novo (`usa_ranger_v3`) entrou com um
    pré-requisito inventado, e o jogo recusou montar qualquer partida ("chave desconhecida _teste_v3"), inclusive nos testes das outras
    sessões. Tirada e recolocada com o pré-requisito do Ranger (o `usa_barracks`): nenhum prédio a lista em `produces`, então não
    aparece em quartel nenhum. A `v43_fuligem_missil` caiu nisso (o "nenhuma marca" dela não vale); refeita na v45.
  - **O disco** chegou a 2,9 GB (a v43 parou antes das infantarias). Com a palavra dele ("se tiver coisa muito antiga não sendo mais usada
    pra nada pode jogar fora"): as 176 pastas de teste de `captures/game` até 26/10 que nenhum documento cita (5,7 GB) foram pro
    `E:\movido_do_C_01-11\captures_game_ate_26-10` (movidas, não apagadas; as 52 citadas nos docs e a `models` ficaram). O C: com 10 GB.
- 01/11 ~22h, **o soldado novo (a 3ª volta, `Game/Tools/soldier_outfit.py`)**, pelo estudo `docs/ESTUDO_INFANTARIA_MODELO.md` (o caminho
  (a)): UMA malha no esqueleto do Manny, com a farda sendo o próprio corpo inflado (alisado sem o músculo e os sulcos do manequim, com
  as dobras), a **cabeça humana do MetaHuman** que vem com a Unreal 5.8 (`MetaHumanAnimator/Content/MeshFitting/Template/mean.obj`, com
  os contornos marcados: pálpebras, íris, sobrancelhas, lábios; o olho com globo, íris e pupila), o equipamento com chanfro encostado
  no pano (as placas curvas com o corte de ombro, a cinta, o MOLLE, os porta-carregadores com aba e o carregador saindo, o rádio, o
  cinto, as joelheiras, os bolsos; o capacete com trilhos, o suporte do visor e os óculos), a oclusão assada nos vértices. Feitos: o
  fuzileiro USA, o fuzileiro da China (o casacão até abaixo do joelho como saia única aberta na frente, pesada entre a bacia e as
  coxas; o gorro de pele com a aba e a estrela) e o rebelde da GLA (a túnica aberta dos lados, a calça larga, o colete civil aberto,
  a faixa, a bandoleira, o lenço enrolado só com os olhos de fora). O jogo pinta a malha pela cor nos vértices (os slots VC e
  VC_Team, `RTSUnitActor`) e o importador (`Game/Tools/import_outfit.py`) prende no `SK_Mannequin`. Prova no jogo: `v44_soldado_v3`.
