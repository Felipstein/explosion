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
- **Melhoria em 2 casas:** o Felipe achou "uma possibilidade". A recomendação da arte é **1 casa**, pra manter as teclas e a grade alinhadas. Ainda falta a decisão dele.

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
