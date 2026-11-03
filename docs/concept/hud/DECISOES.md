# HUD: o que o Felipe aprovou (10/10/2026)

O resumo de tudo o que ficou decidido sobre o HUD do jogo, peça por peça, com a imagem que vale
como referência. Os nomes das peças estão em `NOMES.md`. O histórico das rodadas está em
`captures/concept/hud/PEDIDO.md`.

**Ainda não existe uma imagem final aprovada.** O HUD aprovado é a soma das peças abaixo. A v9
(`refs/mix_v9_*`) é a que chegou mais perto do conjunto, mas tem os erros da seção 5.

## 1. As referências

| Imagem | É a referência de |
|---|---|
| `refs/clean_tanks.webp` | **O visual geral** (painéis, molduras, cores, fonte); o topo 1x1; o aviso; a seta de evento; o painel do comandante |
| `refs/clean_war_factory.webp` | A dica |
| `refs/clean_upgrades.webp` | A coluna de poderes (READY / tempo) |
| `refs/classic_tanks.webp` | Os ícones (Attack Move, Guard, insígnia de blindagem, moldura da patente, botões do minimapa); o conteúdo do minimapa; o retrato |
| `refs/classic_war_factory.webp` | O tamanho da paleta e as fotos das unidades; o cadeado; a fila no cartão |
| `refs/classic_dozer.webp` | O painel do meio quase encostando no minimapa e no cartão; o retrato e a composição do cartão |
| `refs/classic_nothing_8p.webp` | O emblema da placa da facção |
| `wireframe/hud_wireframe.html` | **A planta:** posição e tamanho de cada peça nos 5 estados (abrir no navegador; botões "Notas" e "Nomes") |

### As peças recortadas (`pieces/`)

Cada peça aprovada também está recortada sozinha em `pieces/`. A folha com todas é
`pieces_overview.webp`. O recorte sai de `art/tools/crop_hud_pieces.py` (as caixas ficam no script).
Algumas trazem junto algo que **não** foi aprovado; a coluna da direita avisa.

| Recorte | De onde | O que vale | Atenção |
|---|---|---|---|
| `top_bar_1v1` | limpo | o topo 1x1 inteiro | — |
| `alert` | limpo | o aviso | — |
| `commander_panel` | limpo | dinheiro, energia, patente, Promote | — |
| `offscreen_arrow` | limpo | a seta de evento | — |
| `powers_column` | limpo | READY / tempo, o brilho dourado, só ícone | o anel azul **não**: o carregamento é o fundo escuro do C&C |
| `tooltip` | limpo | a dica | — |
| `minimap_and_buttons` | clássico | o conteúdo do minimapa, a moldura e os ícones dos botões | — |
| `action_icons` | clássico | os ícones de Attack Move e Guard | o Stop vermelho **não**: é a mão branco-acinzentada |
| `unit_card` | clássico | o retrato, a insígnia, a moldura da patente | — |
| `main_grid_palette` | clássico | o tamanho, as fotos, o cadeado, a moldura dourada da melhoria | o girinho do 64% **não**: carregamento C&C |
| `unit_card_queue` | clássico | o cartão da fábrica com a fila | — |
| `command_panel_wide` | clássico | o painel largo encostando no cartão (e devia encostar no minimapa) | a placa STOP **não**: é a mão |
| `unit_card_dozer` | clássico | o retrato do dozer | — |
| `faction_plate` | clássico | o emblema grande | o nome do jogador **não**: vai "UNITED STATES" e as baixas |
| `top_bar_8_players` | v9 | o topo de 8 que ficou (o de hoje) | — |
| `group_tabs_health` | v9 | as abas com a vida do grupo | — |
| `multi_select_captain` | v9 | uma casa por unidade, o capitão dourado, a grade de ações | o painel não encosta no minimapa |

## 2. O visual

- **Base: o design limpo** (`clean_*`). É o da USA. China e GLA terão o seu, inspirado nele, depois.
- **Do clássico vêm:**
  - os ícones;
  - as fotos da paleta e do retrato;
  - o cadeado;
  - o conteúdo do minimapa.
  - As fotos da paleta do clássico são a inspiração pros retratos das unidades.
- **Fundo escuro opaco** em todo painel, sem vidro e sem neon.
- **Texto em inglês,** numa fonte militar condensada, nítida em 1080p.

## 3. Peça por peça

### Barra do topo (`TopBar`)
- **1x1 (2 jogadores):** o desenho do limpo, exatamente como em `clean_tanks`: centralizado, grande e bonito. O relógio fica no centro, você à esquerda num painel azul e o inimigo à direita. A superarma do inimigo fica numa caixinha embaixo dele.
- **Mais jogadores:**
  - **O relógio fica sempre no centro da tela,** com metade dos jogadores de cada lado.
  - **Você e os aliados** ficam colados à esquerda do relógio, **num bloco azul só.** É assim que se vê quem é aliado.
  - **Os inimigos** ficam na caixa escura padrão. **Não se separa os inimigos por time:** não precisa saber os grupos deles.
  - **Todos os cartões têm o mesmo tamanho:** você, aliado, inimigo, espionado e derrotado.
  - **O nome vem na cor do jogador,** sem barra embaixo do nome (parecia vida).
  - **Aliado mostra dinheiro e energia.** A energia é uma barra, igual à sua, com os números.
  - **Inimigo comum não mostra nada:** sem "?" nem rótulo de facção.
  - **Espionado:** a moldura fica laranja e ele mostra dinheiro e energia como um aliado.
    - **13/10, no HUDLab:** o olho é um selo redondo no canto do emblema (aro âmbar e olho âmbar, liso). O tempo que resta da espionagem é o aro escurecendo no sentido do relógio, igual ao carregamento do C&C. O quadradinho com um ponto foi reprovado ("parece uma criança").
  - **13/10, cartão do topo de 8:** nada passa da borda do cartão. O emblema fica grande à esquerda e o nome e o dinheiro numa coluna ao lado. A energia aparece em número à direita e numa barra fina embaixo, na largura do texto. Todas as caixas de superarma usam a mesma letra.
  - **Derrotado:** fica cinza, com um X vermelho (liso, por cima do emblema).
  - **Se não couber,** uma caixinha "+X" resume os que sobram.
- **Superarmas** (`SuperweaponList`):
  - ficam numa caixa separada, pendurada embaixo do dono;
  - uma linha por superarma, com o nome "de dar medo" e o tempo de cada uma;
  - a mais próxima fica em cima;
  - as iguais **não** se juntam (cada uma tem o seu tempo);
  - depois de 3, o resto vira "+2 more";
  - com menos de 1 minuto, a linha fica vermelha e brilha.
- **13/10, caixa das superarmas (no HUDLab, contra a peça `top_bar_8_players`):**
  - uma mini tecla igual ao cartão de cima: azul no seu time, escura no inimigo, vermelha com brilho quando falta menos de 1 minuto;
  - o trevo radioativo de verdade, liso;
  - respiro nas pontas;
  - a linha que vai sair fica vermelha, sem faixa por trás;
  - no 1x1, cada lado mostra até 3 e o resto vira "+ n more".
- **Decidido em 10/10:** fica o topo de hoje, mesmo ainda "carregado". As ideias A (nomes brancos com
  quadradinho de cor e uma linha só de superarma) e B (só ícone e tempo) foram vistas e não entraram.

### Painel do comandante (`CommanderPanel`)
- Fica no canto de cima, à direita, com o design do limpo.
- Mostra:
  - o dinheiro;
  - a energia em barra com os números (vermelha quando falta);
  - a patente em estrelas com a barra de experiência;
  - o botão "Promote", que abre os poderes do general.

### Avisos (`AlertFeed`) e Seta de evento (`OffscreenArrow`)
- O aviso do limpo ("Unit under attack" em vermelho).
- **13/10, no HUDLab:**
  - o triângulo é uma imagem lisa, vermelha com o "!" preto grosso (o feito no Canvas ficou "porco");
  - ele fica dentro da parte escura da caixa, não em cima da ponta de metal;
  - os parafusos da ponta são cor de aço (vieram rosa da folha do GPT).
- "Low power" em amarelo, com o mesmo desenho.
- A seta vermelha na borda da tela, do limpo, aponta o evento fora da vista.

### Coluna de poderes (`PowersColumn`)
- O desenho do limpo: **READY** quando está pronto, o tempo quando não está.
- **Carregando no padrão do C&C:**
  - um fundo escuro por cima do ícone, sumindo no sentido do relógio;
  - o tempo caindo em cima.
- Só o ícone, sem o nome; o nome fica na dica.
- Cada superarma sua tem o seu botão.
- **Decidido em 13/10 (no HUDLab):**
  - o botão é quadrado (1:1) e simples: a tecla escura da pele, sem a moldura alta e enfeitada;
  - a coluna nasce logo acima do minimapa e cresce de baixo pra cima, não lá de cima, embaixo dos avisos.

### Minimapa (`Minimap`) e Botões do minimapa (`MinimapButtons`)
- Um losango com névoa em três níveis: visto, explorado escurecido e nunca visto preto total.
- **O conteúdo é o do clássico:** a prévia do mapa inteiro.
- **Os ícones dos botões ao lado são os do clássico:** opções, unidades, sinalizador e chat.
- **Os dois têm fundo e moldura;** não podem ficar "voando" sobre o mundo (erro da v9).

### Abas de grupo (`GroupTabs`)
- **O número grande** é a tecla. **A quantidade** vai num selo pequeno ao lado ("×4").
- **A vida do grupo:** uma barrinha com a vida somada de todos, pra saber se o grupo está bem.
- **Com algo selecionado,** as abas ficam em cima do painel central.
- **Sem nada selecionado,** descem pra borda de baixo, centralizadas. Não esticam, porque são pequenas.

### Painel central (`CommandPanel`)
- **Vai do minimapa até o cartão da direita, encostando nos dois.** É o pedido mais importante do painel; a referência é `classic_dozer`, que só faltou encostar no minimapa.
- **13/10: colunas da grade.**
  - O Felipe achou o painel apertado comparado à imagem final.
  - A `classic_dozer` tem 8 colunas de casas grandes; a planta tinha 9 + 3 no mesmo espaço, com casas uns 30% menores.
  - Decidido pela arte, com o Felipe dormindo e pedindo pra decidir: **7 + 3**, casas do tamanho do clássico, painel 24 px mais alto.
  - As teclas seguem as fileiras: QWERTYU em cima, ZXCVBNM embaixo.
  - A de 9 + 3 continua pronta pra comparar (`hudlab.ps1 shot -Cols 9`).
  - As casas vazias são lisas (fundo escuro e um fio), como na peça `multi_select_captain`.
- **Grade principal (`MainGrid`),** à esquerda:
  - mostra o que se constrói, ou as unidades selecionadas;
  - as fotos e o tamanho são os do clássico.
- **Grade de ações (`ActionGrid`),** à direita, com moldura verde:
  - é onde ficam as ordens (Attack Move, Stop, Guard, Rally Point, Sell...);
  - a moldura verde marca "ação, não construção", e esse padrão vale em todos os estados.
- **Várias unidades selecionadas:**
  - não se usa a bandeja pequena, que ficou feia e sem hierarquia;
  - cada casa da grade grande é uma unidade selecionada, com a vida;
  - **o capitão** (a unidade principal, que responde com a voz e aparece no retrato) fica em destaque.
- **Ícones das ordens:**
  - Attack Move (mira) e Guard (escudo) são os do clássico;
  - **Stop é a mão levantada, branco-acinzentada,** o mesmo em todo lugar (tanque, dozer).
- **Botão bloqueado:** mais escuro, com o cadeado do clássico.
- **Sem dinheiro:** o preço fica vermelho.
- **Melhoria:** moldura dourada tracejada.
  - Comprada fica apagada, com um check verde.
  - Pesquisando mostra o carregamento do C&C.
- **Carregando (unidade, melhoria):** o fundo escuro do C&C sumindo no sentido do relógio, **com a % em número.**
- **Vender (`Sell`):** um ícone novo, nem as moedas da v7 nem as da v8. O da v9 (um prédio com cifrão) ainda não foi julgado.
- **Melhoria em 1 casa (decidido 01/11):** ele tinha achado 2 casas "uma possibilidade"; ficou a recomendação da arte, **1 casa**, pra manter as teclas e a grade alinhadas ("e o botão de melhoria 1 casa mesmo").

### Dica (`Tooltip`)
- **Aparece no mouse,** em cima do botão apontado. Não fica fixa em cima das abas de grupo.
- O desenho é o do limpo (`clean_war_factory`). Mostra o nome, a tecla em amarelo, o preço e o tempo, e:
  - os cliques ("Left-click +1 · Shift-click +5 · Right-click −1");
  - ou o que falta, em vermelho ("Requires: War Factory").

### Cartão da seleção (`UnitCard`)
- **O retrato e a composição** são os do clássico:
  - foto da unidade;
  - insígnia de melhoria no canto de cima, à esquerda;
  - a moldura quadrada da patente, com as divisas, no canto de cima, à direita.
- O nome e a vida em barra e em número.
- **A fila (`ProductionQueue`)** fica embaixo do cartão, colada nele, como no clássico. Em 13/10, no HUDLab, ela tinha
  ido pra cima do cartão e o Felipe mandou voltar pra baixo:
  - usa o mesmo carregamento do C&C, com a %;
  - clicar cancela (um X vermelho no mouse);
  - arrastar muda a ordem.
- A produção só aparece com a fábrica selecionada. Pode haver 10 fábricas.

### Placa da facção (`FactionPlate`), sem nada selecionado
- **O emblema é o do clássico,** grande e nítido.
- **O nome da facção** ("UNITED STATES"), **sem o nome do jogador.**
- "Army 24 · Buildings 9", como está.
- **Mais embaixo, com menos destaque:** as baixas que você causou ("KILLS Units 31 · Buildings 6").

### No mundo
- Nenhum anel no chão.
- A unidade selecionada mostra só a barra de vida fina em cima.

## 4. Regras que valem sempre

- Nenhuma unidade pode ficar escondida atrás do HUD.
- Sem miniatura do alvo inimigo.
- A produção não fica sempre à vista; só aparece com a fábrica selecionada.
- O retrato é sempre em foto, no estilo clássico do ZH.
- Poucas opções pro jogador.

## 5. O que a v9 errou (corrigir na próxima imagem e no jogo)

**Apontado pelo Felipe:**
- **O minimapa ficou "voando",** sem fundo.
- **Os botões ao lado dele também** ficaram sem fundo.
- **O painel central não encosta no minimapa.**
- **O topo 1x1 saiu errado:** não é o desenho do limpo que ele escolheu (`refs/clean_tanks.webp`).

**Visto pela arte:**
- **O carregamento do C&C virou anéis e girinhos;** o certo é o fundo escuro que some, com o número.
- **No dozer:** uma casa a mais, a tecla O repetida, preços trocados e o Airfield sem cadeado.
- **Na fábrica:** sumiu o risco que separa a grade de ações.
- **No mapa de 8:** o relógio saiu um pouco à direita do centro, e a patente com 2 estrelas e meia.

## 5b. A imagem-mestre não é necessária (12/10)

O Felipe: "pra que ficar insistindo numa imagem coringa, a gente já tem tudo definido; os detalhes pequenos a gente corrige na hora da montagem e da criação".
- **A v10 (`captures/concept/hud/hud_v10_master_tanks_gpt.webp`) NÃO é referência.** O Felipe: a composição do topo e vários detalhes "não batem com o que eu aprovei".
- **A montagem se constrói em cima das imagens aprovadas,** peça por peça:
  - as referências inteiras em `refs/`;
  - as peças recortadas em `pieces/`;
  - as regras da seção 3;
  - a planta (`wireframe/`) só diz posição e tamanho;
  - nenhuma imagem gerada depois (v9, v10) entra como fonte.
- **Decidido por padrão:** melhoria em 1 casa; o ícone de vender é o da v9 (o prédio com o cifrão).
- **Próximo:** o kit pro jogo (os ícones e as fotos recortados), feito pela arte. A montagem em Slate fica com a sessão do jogo, quando ela tiver vez.

## 6. Pendente

- **A imagem final:** refazer os 5 estados com as correções da seção 5, e o Felipe aprova.
- **Melhoria em 1 ou 2 casas:** decisão do Felipe.
- **O ícone de vender.**
- **O topo de 8 jogadores:** fica o de hoje, mas ele ainda acha carregado. Volta a ser estudado se ele pedir.
- **Depois da aprovação:**
  - a imagem aprovada vai pra cá (`docs/concept/hud/`);
  - a sessão do jogo ("RTS game architecture e gráficos") recebe aviso por mensagem, pra montar o HUD em Slate com a planta, os nomes e este documento.

## 13/10: estados, animações e facções (feitos pela arte no HUDLab; o Felipe avalia jogando)

- **O mouse em cima:** a moldura acende na cor do neon da facção, com um véu claro. **Apertado:** o botão afunda e escurece.
- **Animações** (o ZH modernizado, discretas):
  - o READY respira (~1 vez por segundo);
  - a superarma perto pulsa em vermelho;
  - a barra de energia faltando pisca devagar;
  - o aviso entra deslizando da esquerda (0,3 s);
  - o painel central e as abas sobem da borda quando algo é selecionado (0,35 s).
- **Telas:**
  - em 16:10 nada se sobrepõe;
  - no ultrawide, o HUD fica numa faixa 16:9 no meio, porque o olho não deve atravessar 3440 px pra achar o minimapa e o cartão;
  - a seta de evento fica na borda de verdade.
- **A pele de cada facção** (proposta):
  - China com o neon vermelho sobre aço escuro quente;
  - GLA com o neon âmbar de latão sobre ferro marrom;
  - o verde continua sendo a ação em todas, por isso o GLA não é verde;
  - o topo 1x1 não muda, porque o azul ali é "você".
  - Folha: `captures/hudlab/faccoes.jpg`. Peças: `kit/skin/china/`, `kit/skin/gla/` (`art/tools/make_faction_skins.py`).

## 7. Dinheiro, energia e patente: o console que vive da energia da base (31/10)

O Felipe: o painel do canto de cima à direita fica longe da vista; ele mal percebe quase sem energia, sem energia e dinheiro baixo.
Mudar a caixa de lugar foi reprovado ("só pegou e tacou ali"). O rumo aprovado ("agora sim porra! tudo maravilhoso"):

- **A energia é a luz do console.** Uma fileira de células na borda de cima do painel do meio, do minimapa até o cartão,
  **uma célula por ponto de energia** (reator USA = 5, Patriot = −3). Acesa forte = consumo; acesa fraca = sobra; o comprimento
  aceso é a produção; o que falta aparece em vermelho depois do fim. Número pequeno na ponta esquerda (`4 / 10`).
  - Quase sem energia: sobra menor que 3 (o maior consumidor): âmbar, a sobra pisca.
  - Colocando um prédio: as células que ele vai comer aparecem riscadas; se estoura, a ponta pisca em vermelho e a mão diz OVERLOAD.
  - Sem energia: o HUD dá um tranco forte uma vez e fica escurecido (a batalha não escurece); um tranquinho de lembrete a cada ~12 s;
    o botão em produção ganha o raio de "mais lento".
  - O ZH (código): a tira de energia ficava em cima da grade de compra, amarela com menos de 5 de sobra (`W3DControlBar.cpp`).
- **O dinheiro é um contador mecânico** (tambores que giram, com o vai-um) num brasão no meio da fileira (o lugar do dinheiro no ZH).
  O +$ do avião de carga voa do mapa até o contador; a compra rola pra baixo. Pouco dinheiro: os tambores acendem em vermelho e os
  preços que não dá pra pagar ficam vermelhos.
- **As abas de grupo** ficam dos dois lados do brasão (1–5 e 6–0), na mesma fileira. Sem nada selecionado, a fileira inteira desce pra borda.
- **As plaquetas** sobem do brasão com o efeito de mola: LOW POWER (o que parou), INSUFFICIENT FUNDS (quanto falta e quando chega o
  avião), PROMOTE. O efeito e os tipos foram aprovados ("top").
- **01/11, aprovado (v3, "o resto tá aprovado"):**
  - o brasão menor (tambores 22×36, números de 28 px), **sem estrelas**;
  - **o hover de todos os botões:** sobe 3 px e cresce 5%, acende por dentro na cor do tipo (azul; verde na ação; dourado na melhoria e no
    poder pronto), a luz corre no metal uma vez (0,55 s), sombra embaixo; apertado afunda e pisca o aro branco; bloqueado não sobe, só treme;
    casa vazia só clareia. A dica é a do limpo; nos botões da grade ela sobe **acima da fileira**, pra nunca tampar o dinheiro e a energia na compra.
    O contador, a energia e a patente mostram o detalhe no mouse (renda por minuto, o que gasta energia, o XP). O de antes ("só uma corzinha em volta")
    era um véu branco de 7% e uma linha acesa (`RTSHudPainter::HoverPress`);
  - **o chat:** as linhas perto do minimapa, à direita da coluna de poderes, empilhando pra cima; cada uma numa placa escura com a cor do jogador
    na borda, letra de 17 px, o nome na cor do jogador, ALLIES num selo verde, sistema em cinza; entram com o tranco das plaquetas e somem em 12 s;
    o botão do chat no minimapa pisca quando chega mensagem; Enter abre a caixa com as abas ALL / ALLIES e a dica das teclas.
    O de antes: letras de 11 px sem fundo no canto de cima, onde ficam os avisos (`ARTSHUD::DrawChat`).
- **A patente: em aberto.** Reprovados: as estrelas no brasão e o selo + barrinha na ponta direita da fileira ("ali tá ruim, quero outra opinião").
- **01/11, a patente escolhida: a escada (B)** na emenda do painel do meio com o cartão: 5 degraus, um por patente, o atual enchendo com o XP.
  Pedidos junto: mais grossa e visível; dar pra entender **em que patente estou**; **um botão que abre uma tela** (o sistema de patentes e poderes
  vai evoluir muito além do ZH). Base do ZH conferida no código: a barra de XP dele era vertical e sempre à vista, a estrela piscava 1 s quando
  sobrava ponto e uma seta caía até ela (`ControlBar.cpp`, `W3DControlBar.cpp`).
- **O aro do XP em volta do emblema** fica **só nos aliados** (os cartões do topo); no nosso emblema, não.
- **Depois de aprovado:** as peças novas vão pra arte no gerador de imagem, como o HUD de hoje foi feito.
- Protótipo animado: `captures/concept/hud/recursos2/console_vivo.html` (servidor local `hud-concept` na porta 8133).
- **01/11, a escada v2 (no protótipo, falta ele ver):** tubo de 22 px com o número de cada degrau (1 a 5), a seta "você está aqui", e em cima a
  placa-botão com as estrelas e a sigla da patente real dos generais dos EUA (BG ★, MG ★★, LTG ★★★, GEN ★★★★, GA ★★★★★) e a faixa GENERAL ▸,
  que abre a tela do general. O aro do aliado: em volta do emblema dele no topo de 8, com o nível num selo dourado.
- **01/11, o sistema de patentes e poderes (proposta, em discussão):** a "escada de escolhas". A cada patente, 1 de 2 doutrinas (cada uma = 1 poder
  + 1 bônus); a outra fica trancada na partida. A XP vem do combate e de **comendas** (feitos medidos pela simulação: abater, segurar a base, entregar
  carga). A tela do general (a placa GENERAL abre, a batalha não para): a escada grande à esquerda, as duas doutrinas do degrau no meio, as comendas e
  o que o inimigo já revelou à direita. Generais depois trocam o par de 1 ou 2 degraus (o jeito do ZH: cada general é a mesma árvore com poderes
  trocados). O estudo: ZH conferido no código e no `Rank.ini`/`Science.ini` do jogo instalado (XP 0/800/1500/2500/5000 só por abate, pontos
  1/1/1/1/3, todo poder custa 1, pré-requisito patente 1, 3 ou 5); comparados CoH3 (battlegroups), RA3 (protocols), AoE IV (2 landmarks), Halo Wars 2,
  Tempest Rising (doutrinas) e SC2 co-op. O nosso de hoje (`Units.json`, `rank`): XP pelo dano dado e recebido, 0/600/1500/3000/5000, sem pontos.
- **01/11, v6 (falta ele ver):** o tubo sem os números. O botão virou **o cubo do general**: um medalhão de metal no encontro da fileira de energia
  com o cartão, que junta as três peças (sem seleção ele fica embaixo, onde a fileira desce); as estrelas da patente dentro; o aro acende dourado
  e o PROMOTE sobe dele quando há ponto. O aliado: **o próprio aro de metal do emblema dele** com o XP correndo no sulco (um brilho passa só na parte
  cheia) e as estrelas da patente ao lado do nome; subiu, o aro estala e ganha a estrela. A tela do general ganhou a visão **ALL RANKS** (as 5
  patentes lado a lado, as duas doutrinas de cada uma, a linha dourada ligando as escolhidas, as comendas embaixo); a ONE RANK continua.
  "ENEMY INTEL" virou **ENEMY DOCTRINES SPOTTED**: as doutrinas do general inimigo que você já viu ele usar (nunca o que ele não mostrou).
- **A doutrina trancada:** em cada patente você escolhe uma das duas; a outra daquela patente não pode mais ser escolhida naquela partida (não há
  troca nem reembolso). Na partida seguinte, escolhe de novo do zero.
- **01/11, aprovados:** as estrelas do aliado no fundo do aro do emblema (5 casas; o XP por cima, 210°) e o cubo do general no topo também
  sem seleção. **O que se viu do inimigo:** só aparece quando há algo descoberto (nada visto = nem o título); não mostra "outros X inimigos".
- **01/11, o sistema repensado (proposta v8, falta ele ver): o cardápio do general, sem trava.** Ele ficou com o pé atrás da trava 1 de 2. O estudo
  (fontes conferidas): quase nenhum RTS grande tranca 1 de 2 por degrau; onde trava (AoE IV, CoH3) os jogadores reclamam de "sempre o mesmo";
  a árvore do Halo Wars 2 confundiu no beta e foi simplificada; o patch comunitário 1.06 do ZH passou a cobrar 1 a 4 pontos por poder porque
  "todos a 1 ponto não fazia sentido". A proposta: cada patente dá pontos (1, 1, 2, 2, 3); cada poder custa de 1 a 3 pela força; compra o que
  quiser, na ordem que quiser; não dá pra ter todos; alguns poderes têm níveis I a III (como o Paradrop do ZH). As faixas abrem nas patentes 1, 3
  e 5. A tela: os poderes em 3 faixas, 4 estados só (comprado, dá pra comprar, falta ponto, falta patente), o detalhe do apontado com o botão de
  comprar, as comendas embaixo e os poderes que você já viu o inimigo usar. Protótipo: `console_vivo.html` (as versões com trava: `console_vivo_v7.html`).
- **01/11, v9 (falta ele ver):**
  - **O espionado:** o que se vê do inimigo vem só da espionagem (o estado "espionado" do topo, aprovado em 13/10: moldura âmbar, selo do olho com
    o aro escurecendo). Enquanto dura, o card mostra o general espionado (nome na cor dele, estrelas, os poderes que ele tem); acabou, some. Nada fica
    anotado (é da cabeça do jogador). A vaga do card fica sempre no lugar, vazia quando não há espionagem.
  - **A ficha de ponto:** um hexágono dourado com o número, o mesmo no custo de cada poder, no botão BUY e na carteira (POINTS to spend, em cima à
    direita, logo acima do detalhe: o olho vai da carteira ao botão de comprar). Dourada = dá pra pagar; escura = falta ponto; cadeado = falta patente;
    ✓ = comprado. Saíram as bolinhas. Em cima à esquerda, o comandante: o cubo com as estrelas, a patente, a barra de XP e quantos pontos a próxima dá.
  - **A trilha das patentes** (proposta no lugar das 3 faixas do ZH): colunas = as 5 patentes (cada uma diz quantos pontos dá; a atual marcada
    "you are here"), linhas = o tipo de poder (ar, tropas, fogo, apoio). Toda patente libera algo (no ZH as patentes 2 e 4 só davam ponto); os níveis
    de um poder são nós ligados na mesma linha (Paradrop I → II → III). As 3 faixas continuam numa aba pra comparar.
- **01/11, a tela do general (pedidos dele):** o quadro da direita é só o detalhe do poder no mouse, **sem botão BUY**; compra clicando no próprio
  poder; sem mouse em cima, o quadro fica vazio (o espaço segura o layout). O card do inimigo é a lista de antes, só enquanto espiona, com a dica
  em cada poder; sem espionagem não aparece nada. A fileira de baixo alinha com as colunas de cima. As dicas ficam por cima da tela.
- **01/11, o aviso do PROMOTE (pedido dele):** a plaqueta deixou de "voar": é uma placa escura com aro dourado presa ao cubo por uma ponta,
  com a ficha da estrela, PROMOTED e "General ★★★★ · 2 points to spend"; nasce do cubo com o mesmo tranco das plaquetas aprovadas e respira.
  **Some quando você abre a tela do general** (já viu); **o brilho do cubo e o número de pontos continuam enquanto sobrar ponto.**
- **01/11, protótipo:** a barra de cima escolhe a patente (0 a 5) e a tela (trilha ou faixas); a tela do general ficou 25% maior e pode ficar
  por cima do HUD de baixo (pedido dele). **Decidido (01/11): a partida começa com 1 estrela e 1 ponto, como o ZH**; o primeiro poder só fica pronto depois de
  carregar uma vez desde o início (e depende do Command Center); no começo a plaqueta diz FIRST POWER (não é promoção). Sem a patente 0.
- **01/11, a trilha escolhida (v14, o que confundia):** só parece clicável o que dá pra comprar (aro dourado parado; nada pisca na grade, só a
  carteira); o que não dá fica apagado, sem subir no mouse e sem a mão (ficha escura = falta ponto; ⛓ = falta o nível antes; 🔒 = falta patente);
  clicar nele só treme, e o detalhe diz o porquê em texto. **As melhorias (II, III) são nós pequenos e redondos presos ao poder pela linha** (não
  parecem outro poder), com o que muda ("8 Rangers"). A frase longa virou uma legenda curta com as cores.
- **01/11, v15 (pedidos dele):** **sem legenda nenhuma** (nem a de cima, nem as duas de rodapé, nem "you are here"): a tela se explica pela forma,
  pela cor e pelo que reage ao mouse. **A melhoria mora no próprio card do poder:** um card por poder; os níveis numa barrinha de segmentos embaixo
  do nome (verde = seu; dourado = o próximo que dá pra comprar); o card fica no lugar do nível I; com o nível I comprado ele ganha o ✓ e o aro verde,
  e se o próximo nível dá pra comprar o aro é dourado com a ficha do custo; clicar compra o próximo nível. O detalhe no mouse lista os níveis.
- **01/11, v16 (a v15 regrediu):** com os níveis dentro de um card só, sumiram as linhas e a regra de patente de cada nível ficou invisível
  (parecia que dava pra subir a qualquer hora). Voltou: **cada nível na coluna da patente que o libera, ligado ao anterior pela linha**
  (verde = comprado; dourada tracejada = o próximo dá pra comprar agora; cinza = ainda não), **no mesmo card do poder** (o número do nível ao lado
  do nome), sem o nó redondo e sem legenda. **Comprado:** a ficha verde com o ✓ no canto do custo (a mesma ficha, outra cor) e o card sólido,
  sem neon.
- **01/11, a estrutura está boa (ele: "acho que tá bom").** Os poderes da tela são exemplo: **o conteúdo (quais poderes, custos, recargas, a patente
  de cada nível) e o ritmo da XP ficam pra uma sessão própria de balanceamento e ideias**, depois.
- **01/11, pedidos dele:** o "Click to open General's Command" só aparece na dica do cubo (o botão); a dica do tubo mostra o resto igual
  (patente, XP, pontos). A plaqueta PROMOTED fica só com o essencial: PROMOTED + a patente nova ("General"); no começo da partida, FIRST POWER.
  Os pontos já estão na moeda ao lado do cubo e as estrelas no próprio cubo, então saíram da plaqueta.
- **01/11, os ícones das patentes (pedido dele; falta ele ver no protótipo):** um ícone por patente (folha do GPT; o rank 4 consertado pra 4 estrelas).
  Entram no miolo do cubo (no lugar das estrelas desenhadas), no cabeçalho da tela do general e na fileira das patentes (o ícone à esquerda do
  nome; as estrelas em texto saíram, o ícone já diz). A plaqueta PROMOTED aprovada ficou como está (a ficha com a estrela).

- **03/11, a tela do general sem as linhas por tipo (decisão dele, na sessão dos poderes):** "dá pra tirar essa distinção das linhas,
  C&C não tem isso". O tipo do poder passou a ser lido no próprio ícone (`docs/poderes/ICONES.md`). As colunas continuam sendo as
  patentes; as linhas ficam livres, sem rótulo, e uma patente pode ter quantos poderes precisar; os níveis seguem ligados na mesma linha.
  Como fica: `docs/poderes/tela/poderes_usa.jpg` (a tela montada a partir deste protótipo por `docs/poderes/tela/make_tela.py`, sem mexer
  nele). Quando o protótipo for atualizado, sai a coluna AIR/GROUND/FIRE/SUPPORT.

- **03/11, a tela do general calma e com direção (aprovada por ele, na sessão dos poderes; aplicada no protótipo):**
  - "Meus olhos ainda cansam" e "parece que não tem ordem, hierarquia, direção".
  - **A calma:** o estado aparece uma vez só (a ficha do custo ou o ✓), sem moldura acesa em todo cartão; o dourado só no mouse ou no
    selecionado; o nível sem a plaquinha colorida; as linhas entre os níveis finas e cinza.
  - **A linha do tempo:** a coluna da patente atual acende (o traço dourado no cabeçalho), e as que faltam apagam.
  - Vale pras 4 facções, cada uma com o fio neutro da sua cor (o bloco de CSS de 03/11 no `prototipo/index.html`). O exemplo de poderes
    do protótipo virou a árvore aprovada da USA, com os ícones aprovados.
  - Fotos: `docs/concept/poderes/prototipo_<facção>.jpg`. O diagnóstico: `docs/poderes/USA.md` §18 e §19.

## 8. O HUD das outras facções (01/11)

O plano e o estudo do ZH (pelos dados do jogo instalado) em `docs/concept/hud/FACCOES.md`. A proposta de 13/10 (o neon girado de cor) foi
reprovada: "tem que mudar tudo, texturas, cores, tudo". Fica igual: o lugar das peças e as cores de significado. Muda: todo o material.
Primeiro a folha do visual de cada facção (China e GLA); a Rússia depois da paleta dela.

## 9. A energia é significado, igual nas 4 facções (01/11, noite)

- **Verde = energia boa, amarelo = quase acabando, vermelho = faltando**, em todas as facções. A luz da facção fica na moldura e nunca
  pinta a energia (ele: "nenhum tem que ficar laranja, vermelho, por conta da cor da facção").
- **GLA sem usina: a barra de energia não existe.** A GLA não tem energia própria; só constrói usina roubando a tecnologia de outra facção.
  Sem nenhuma energia construída, a barra some. As outras facções começam em zero mas já mostram a barra (podem construir desde o início).
  No `RTSHudPainter::DrawCommander` hoje a energia some com `PowerMade == 0` em todas: no jogo, só a GLA some.
