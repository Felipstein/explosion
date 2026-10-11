# HUD: o nome de cada peça

O mapa de nomes do HUD, pra todo mundo (Felipe, sessão de arte, sessão do jogo) falar da mesma
coisa. O nome em português é o da conversa; o nome em inglês é o do código (Slate) e dos prompts.

Imagens com os nomes em cima do layout:
- `hud_names_selected.webp`: com algo selecionado (a War Factory);
- `hud_names_nothing.webp`: sem nada selecionado, partida de 8.

Ao vivo, com os 5 estados: `wireframe/hud_wireframe.html` (nesta pasta), botão **Nomes**. O que foi aprovado: `DECISOES.md`.

## Em cima

| Peça | Código | O que é |
|---|---|---|
| **Barra do topo** | `TopBar` | A faixa de cima inteira: os jogadores e o relógio. |
| **Relógio da partida** | `MatchClock` | O tempo de partida, sempre no centro da tela. |
| **Cartão de jogador** | `PlayerCard` | Um por jogador: emblema e nome. Aliado e espionado mostram também dinheiro e energia. |
| **Bloco do seu time** | `TeamBlock` | Você e os aliados num painel azul só, colado à esquerda do relógio. |
| **Lista de superarmas** | `SuperweaponList` | A caixinha embaixo de cada cartão, com as superarmas do dono e o tempo de cada uma. |
| **Painel do comandante** | `CommanderPanel` | Canto de cima à direita: o seu dinheiro, a energia, a patente (estrelas e experiência) e o botão Promote. |
| **Avisos** | `AlertFeed` | Canto de cima à esquerda: "Unit under attack", "Low power"... |
| **Seta de evento** | `OffscreenArrow` | A seta na borda da tela que aponta pra um evento fora da vista. |

## Na esquerda

| Peça | Código | O que é |
|---|---|---|
| **Coluna de poderes** | `PowersColumn` | Os poderes do general e as suas superarmas, um botão cada, com a recarga. |

## Embaixo

| Peça | Código | O que é |
|---|---|---|
| **Minimapa** | `Minimap` | O losango com a névoa. |
| **Botões do minimapa** | `MinimapButtons` | Os botões redondos ao lado: opções, unidades paradas, sinalizador, chat. |
| **Abas de grupo** | `GroupTabs` | As teclas 1 a 0 dos grupos, com a quantidade e a vida somada do grupo. |
| **Painel central** | `CommandPanel` | O painel grande do meio, do minimapa até o cartão da direita. |
| **Grade principal** | `MainGrid` | O lado esquerdo do painel central: o que se constrói (unidades, prédios, melhorias) ou, com várias unidades, uma casa por unidade selecionada. |
| **Grade de ações** | `ActionGrid` | O lado direito do painel central, com moldura verde: as ordens (Attack Move, Stop, Guard, Rally Point, Sell...). |
| **Dica** | `Tooltip` | A caixinha que aparece no mouse, em cima do botão apontado. |
| **Cartão da seleção** | `UnitCard` | Canto de baixo à direita: o que está selecionado. |
| **Retrato** | `Portrait` | A foto dentro do cartão, com as insígnias (melhorias) e a patente da unidade. |
| **Vida** | `HealthBar` | A barra de vida do cartão. |
| **Fila** | `ProductionQueue` | A fila da fábrica ou da pesquisa, dentro do cartão. |
| **Placa da facção** | `FactionPlate` | O que o cartão mostra quando nada está selecionado. |

## No mundo

| Peça | Código | O que é |
|---|---|---|
| **Barra de vida no mundo** | `WorldHealthBar` | A barrinha em cima de cada unidade selecionada. |

## Palavras do dia a dia

- **Capitão:** a unidade principal de uma seleção de várias. É a voz que responde e a foto do retrato.
- **Carregando:** o padrão de recarga do C&C. Um fundo escuro some no sentido do relógio, com o número em cima (o tempo nos poderes, a % na construção).
- **Moldura dourada:** melhoria. **Moldura verde:** ação. **Cadeado:** falta um prédio. **Preço vermelho:** falta dinheiro.
