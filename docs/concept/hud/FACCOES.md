# O HUD de cada facção (01/11/2026)

O pedido dele: "tem que mudar tudo, os detalhes das texturas, cores, tudo, pra encaixar bem e ficar bonito e bem feito; isso aqui é um RTS AAA".
A proposta de 13/10 (o HUD da USA com o neon girado de cor, `captures/hudlab/faccoes.jpg`) foi reprovada: "que feio que ficou".

## 1. Como o Zero Hour faz (conferido nos dados do jogo instalado)

- `Data\INI\ControlBarScheme.ini` (do `INIZH.big`): um esquema por facção (`America8x6`, `China8x6`, `GLA8x6`).
  - **A posição é a mesma nas três** (até a barra de energia: `PowerBarUL X:260 Y:470` na USA, `X:259` na GLA, `Y:469` na China).
  - **As cores de significado são as mesmas nas três:** construir azul (67,108,190), ação verde (1,175,2), melhoria laranja (208,108,0),
    sistema amarelo (207,195,2).
  - **O que muda é o conjunto inteiro de texturas:** cada peça existe em três versões com prefixo (SA = USA, SN = China, SU = GLA):
    `SAControlBar512_001.tga`, `SNControlBar512_001.tga`, `SUControlBar512_001.tga` (no `Textures.big` do Generals), a barra de fundo
    (`COB2_`, `China2_`, `GLA2_Background`), os botões do general, a barra de XP e a janela dos poderes (`GeneralsPowerWindow_American/_GLA`).
- O que as texturas mostram (a folha `captures/concept/hud/faccoes/estudo_zh_pecas_por_faccao.jpg`):
  - USA: aço azul polido, bisel limpo, o emblema da águia com as asas;
  - China: botões de laca vermelha, frisos dourados, o emblema do dragão dourado com a estrela vermelha;
  - GLA: latão e ferro gastos, oliva, peças de sucata, a lâmpada vermelha de alerta;
  - a fita da patente: azul → prata (USA), vermelho → dourado (China), verde → claro (GLA).

## 2. A regra do nosso (o ZH modernizado)

- **Fica igual nas quatro facções:** o lugar e o tamanho de cada peça; as cores de significado (verde = ação, dourado = melhoria, capitão e
  ponto, vermelho = alerta); os ícones das ordens; o comportamento (hover, plaquetas, tela do general).
- **Muda tudo do material:** o metal, a textura, o ornamento, a cor da luz, o emblema, os ícones e os nomes das patentes, as medalhas.
- **Legível acima de tudo:** o desgaste e o ornamento ficam nas molduras; as faces dos botões e dos painéis ficam limpas (a plaqueta "suja"
  da USA foi reprovada em 01/11).

| | USA (feito) | China | GLA | Rússia (depois da paleta) |
|---|---|---|---|---|
| Emblema (`docs/concept/factions/`) | águia de aço, anel azul | dragão dourado numa engrenagem vermelha, estrela | guerrilheiro de pano, anel de bronze rebitado e verde, facas | (a fazer) |
| Ideia | precisão, tecnologia | império militar: pesado, disciplinado | sucata viva: soldado, remendado | indústria pesada soviética de hoje |
| Metal | aço escuro escovado, frio | aço escuro quente, quinas cortadas em 45°, mais grosso | ferro e latão de sucata, solda à mostra, parafusos desencontrados | aço frio azulado, painel de instrumento soviético |
| Faces | vidro azul-marinho | esmalte vermelho (mais escuro que o do emblema) | chapa oliva fosca, faixa de esmalte verde no bronze | esmalte cinza-esverdeado de painel |
| Friso / ornamento | rebites de aço | friso de ouro; **o cubo vira a engrenagem do emblema** (dentes de ouro); a grega "huiwen" em baixo relevo, só na moldura | **bronze com a fileira de rebites do anel do emblema**; **o pano oliva amarrado** no pescoço do cubo e na ponta do painel (assinatura); solda, poucas listras de perigo | parafusos grandes, chaves de alavanca, placas estampadas |
| Luz | neon azul (#49AAFF) | neon vermelho de lanterna (#E8402A) | lâmpadas incandescentes brancas quentes (#FFE2A0) + a lâmpada vermelha de alerta (assinatura) | luz branca fria (#E6F0FF) |

Por que a GLA não tem neon colorido: o verde é a ação e o dourado é a melhoria; um âmbar forte confundiria com o dourado. A luz dela é a
lâmpada branca quente (sem saturação) e a lâmpada vermelha de alerta, que é a assinatura do ZH.

**Os materiais saem dos emblemas** (pergunta dele, 01/11: "você já viu os emblemas de cada facção?"): o prompt anexa o emblema e pede o
material dele, nunca o desenho (nada de dragão, estrela ou rosto nas peças).

## 3. A ordem

1. **A folha do visual de cada facção** (os botões nos 4 estados, as abas, o aviso, o botão de poder, os painéis, o brasão e o cubo):
   define o material. Ele aprova uma por facção.
2. Com a aprovada como raiz: a folha das molduras grandes (topo 1x1, superarma, minimapa, cartão), a folha da economia (tubo, fileira),
   as fichas, as medalhas e os ícones das patentes (os nomes de cada exército) de cada facção.
3. Os ícones das ordens com o quadrado do botão de cada facção.
4. A Rússia depois da paleta dela (`docs/FACCAO_RUSSIA.md` §2, decisão dele).

Os prompts: `captures/concept/hud/recursos2/PROMPTS_ARTE.md` (folhas E e F).

## 4. A Rússia: o que falta decidir (dele) pra montar o HUD

1. **A paleta** (`docs/FACCAO_RUSSIA.md` §2): recomendada a **A média** (o aço frio azulado com as manchas cinza, a gaiola na cor do
   tanque): o HUD sai de aço frio azulado, o único frio do jogo.
2. **O emblema: o urso de aço, sem a estrela (escolhido em 01/11)** → `docs/concept/factions/russia_emblem.webp`.
3. **A luz do HUD:** proposta a **branca fria** (#E6F0FF): a USA é o azul, a China o vermelho, a GLA a lâmpada quente.
4. **Os nomes das patentes** (proposta, os reais do exército russo): Major General ★, Lieutenant General ★★, Colonel General ★★★,
   General of the Army (a estrela grande) e Marshal (a estrela grande com a coroa de louros).

## 5. O feito em 01/11 (com ele fora: "vai melhorando, vai gerando no app")

- **As folhas do visual:** China v2 e GLA v2 (com o emblema anexado; as antigas, sem o emblema, perderam: menos assinatura) e a Rússia (a
  1ª das duas do Atelier). Recortadas por `art/tools/cut_faction_skin.py` em `docs/concept/hud/kit/skin/<facção>/` (a pele de 13/10 guardada em
  `captures/concept/hud/kit/skin_1310_hue/`).
  - O recorte agora tira só o verde do fundo (o aro verde do botão de ação se apagava junto).
  - A face dos painéis grandes da China saiu mais escura e menos saturada ("cores muito fortes"): `FACE_TONE` no script.
- **As molduras grandes** (minimapa com os 4 botões, cartão, caixinha da superarma) e **os 16 ícones** de cada facção, gerados no Atelier
  ancorados na folha aprovada de cada uma: `art/tools/cut_faction_frames.py` → `skin/<facção>/` e `icons/<facção>/`.
- **O HUD** (`RTSHudPainter.cpp`): a luz da Rússia branca fria e a da GLA a lâmpada quente pálida (era âmbar, que confunde com o dourado da
  melhoria); os ícones procuram `icon:<facção>/<nome>` antes do da USA.
- **O emblema da Rússia sem a estrela:** urso e águia (`docs/concept/factions/README.md`).
- **Falta:** o topo 1x1 e o painel do comandante por facção (o topo é o mesmo nas 4: o azul ali é "você"); o brasão, o cubo e as fichas no
  protótipo por facção; os ícones das patentes e as medalhas de cada exército.
- **A revisão (01/11 à tarde, ele: "detalhes sobrepostos, cores muito fortes"):**
  - o esmalte vermelho da China mais escuro e menos saturado também no minimapa, no cartão e na caixinha (`calm_red` em `cut_faction_frames.py`);
  - as letras da bússola (N, S, E, W) entram 26 px nas outras facções: sentavam na engrenagem, no pano e nas ripas;
  - a moldura das outras facções é mais grossa: o painel do meio desce até a beira da tela, pra borda de baixo não passar por trás das casas;
  - a placa sem seleção mostrava "UNITED STATES" e a águia em todas: o laboratório agora põe o nome e o emblema da facção da foto
    (`HudLabHUD.cpp`; o emblema da Rússia é o urso sem estrela, que ele escolheu).
- **O topo 1x1 por facção (01/11, noite):** a folha das molduras deixou o topo de fora. Gerado no Gemini Flash pela chave (ele carregou
  R$ 30 no pré-pago do AI Studio; 6 imagens, ~US$ 0,40). Prompts na Folha I do `PROMPTS_ARTE.md`, anexos em
  `captures/concept/_attach/hud_topo_<facção>/`. **1ª volta:** GLA aprovada (com o pano saindo pra fora); China apagada e com engrenagens
  atrás do anel, Rússia igual à USA e os emblemas fora do centro, reprovados (`APROVADOS.md`). **2ª volta:** China `68_…`/r4 (azul e
  vermelho fortes, engrenagem dentro do aro), GLA `70_…`/r1, Rússia `71_…`/r3 (aço grafite, grade de ripas nas pontas).
  - O recorte (`cut_faction_frames.py --top <folha> <facção>`): pega a peça larga de cima, separa a vizinha encostada, acha a face
    redonda dos dois anéis, escala igual nos dois eixos (o anel fica redondo) com os anéis em x 83/1006 e y 68, tira o verde preso e o
    respingo da borda, e grava com margem (110 dos lados, 60 embaixo) pra peça sair da caixa (o pano, as ripas). Imprime o fim do corpo.
  - O HUD (`DrawTop1v1`): lê a margem pelo tamanho da textura; o emblema no centro medido (y 68, raio 37); a caixinha da superarma sai
    de baixo do fim do corpo de cada facção (tabela `Bottom`: USA 128, China 130, GLA 138, Rússia 147; atualizar se regerar).
  - Foto: `captures/concept/hud/faccoes/topo_1x1_4_faccoes_v2.jpg`. Falta ele ver a 2ª volta.

## 6. A China nova: a laca (01/11, noite)

- A v2 vermelha ("muito avermelhado") e a v3 dourada ("parecendo o GLA") foram reprovadas. Três direções no Atelier (A laca preta e
  vermelhão, B tecnologia, C cloisonné); vistas no HUD inteiro, **ele escolheu a A**.
- O kit da China agora é a laca: `cut_faction_skin.py` na folha `84_china_a_laca/raiz_r1_flash.jpg` (as casas reordenadas pelo aro;
  o medalhão e o poder destrocados, e o recorte agora pega o medalhão pela forma redonda); as molduras e o topo de `91_china_laca_molduras`,
  o tubo e a energia de `92_china_laca_tubo_e_energia`. As peças antigas guardadas em `captures/concept/hud/kit/skin_china_v2_0111/` e `_v3_0111/`.
- **A energia não segue a facção:** verde, amarelo e vermelho em todas (DECISOES §9). O miolo do tubo e da energia da laca veio neutro.
- Falta: os 16 ícones da China ainda são os da v2 (vermelhos); a tela do general da China com a face da laca; o topo de 8 jogadores.

## 7. Os consertos da noite de 01/11 (ele viu no navegador)

- **A tela de poderes: a moldura de cada facção no tamanho da janela** (`art/tools/make_gen_frame.py`): os cantos inteiros no tamanho natural
  (as cantoneiras da China, os parafusos da GLA, as placas da Rússia), a borda lisa entre eles (a da GLA repete os rebites em vez de esticar),
  os lados de uma faixa lisa do meio, o miolo vazado; o fundo é metal escovado gerado, sem emenda e sem espelho. No protótipo a moldura é
  desenhada por cima (`#gen::before`) e a borda de verdade empurra o conteúdo pra dentro (`--bt`/`--bl` por facção): nada passa por cima dela.
- **As abas dos grupos em vetor** (`art/tools/make_group_tabs.py`): a arte larga encolhida numa casa de 48×46 virava borrão (a GLA pior);
  cada aba é um SVG no material da facção, nítido em qualquer escala.
- **O brasão do dinheiro da China refeito** (`93_china_laca_brasao`): a laca era mais alta (a faixa de nuvens em cima) e subia acima dos outros;
  agora tem a proporção dos outros e a janela única do mostrador. E o mostrador segue a janela de cada facção (uma regra antiga prendia a da USA).
- **As patentes por facção (APROVADAS 02/11):** USA como hoje; China do Exército Popular em pinyin (Shao Jiang, Zhong Jiang,
  Shang Jiang, Da Jiang, Yuan Shuai); Rússia transliterada (General-Mayor, General-Leytenant, General-Polkovnik, General Armii, Marshal);
  GLA com títulos de milícia, sem título religioso (Cell Leader, Field Commander, Warlord, High Warlord, Supreme Warlord). Os ícones das
  patentes ainda são os da USA nas quatro.
- **O emblema do topo: a silhueta na cor do jogador (ESCOLHIDA 02/11)** (a outra sessão gerou em `80_emblemas_pequenos_ficha_e_silhueta`, versão 4;
  guardadas em `docs/concept/hud/kit/emblems/glyph_<facção>.png`): o padrão do `DrawTop1v1` (`TopEmblem` 1); o resto do HUD fica com o emblema inteiro.
  `-TopEmblem=0` mostra o inteiro e `=2` a ficha nas cores da facção, pra comparar.

## 8. As peças que faltavam (02/11)

- **O topo de 3 a 8 jogadores v1 (REPROVADO 02/11: "bem sujo, bem ruim de ver, bem mal feito")** (`RTSHudPainter::DrawTopMany`): montado com as peças do topo 1x1 aprovado da facção (cortes da mesma
  textura por UV: a ponta, o anel, o começo e a ponta do painel, o relógio). Cada jogador ganha o anel com a silhueta na cor dele e um painel
  curto na cor dele (a camada do painel pintada pela cor); o relógio no meio; as pontas da facção só nas extremidades. A metade da esquerda usa
  o lado "você" da arte, a da direita o do adversário. Escala 0,46 (0,8 da do 1x1): com 8 a barra tem ~1150 px e não chega nos avisos.
  Aliado ou espionado: o dinheiro e a energia embaixo do nome; o derrotado cinza com o X; as superarmas embaixo de cada jogador.
- **Os 16 ícones da China na laca** (`104_china_laca_icones`, recortados por `cut_faction_frames.icons`).
- **As patentes e as medalhas de cada exército (APROVADAS 02/11)** (`art/tools/cut_faction_ranks.py`, `rank_<facção>_1..5.png` e `medal_*.png` em
  `kit/skin/<facção>/`): China (as estrelas douradas com o louro do Exército Popular; a de marechal com o disco vermelho; fitas vermelhas com
  listras amarelas), Rússia (o ziguezague dos generais, 1 a 3 estrelas, a estrela grande e a de marechal com louro; a fita tricolor), GLA
  (do punhal ao escorpião dourado; medalhas de bronze batido com fita de pano). No protótipo, o cubo, a tela de poderes e as medalhas usam as
  da facção (`rankIcon`, `medalsSet`).
