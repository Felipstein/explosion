# Kit do HUD pro jogo (USA)

As peças prontas pra montagem em Slate, recortadas pela sessão de arte. A regra de montagem está em `../DECISOES.md`, os nomes em `../NOMES.md`.

## Pele (`skin/`, PNG com transparência, 12/10)

As molduras vazias do HUD, no acabamento das peças aprovadas: metal escuro com rebites, vidro azul.
São a textura de verdade de cada painel. O conteúdo (texto, foto, ícone, barra) vai por cima.

- **De onde vêm:** duas folhas do GPT sobre fundo verde (`captures/concept/hud/kit/skin_sheet_1_gpt.webp` e `skin_sheet_2_gpt.webp`).
- **Como recortar:** `python art/tools/cut_hud_skin.py <folha_1> <folha_2>`. O script tira o verde, aperta o recorte em cada peça e vaza o buraco do minimapa e a janela do retrato.

| Arquivo | Tamanho | Onde vai | Como estica |
|---|---|---|---|
| `top_bar_1v1.png` | 1087×140 | O topo 1x1 (o emblema vai no anel; os nomes e o relógio, nos painéis) | Inteira |
| `superweapon_box.png` | 427×69 | A caixa das superarmas | 9 fatias, margem 40×16 |
| `minimap_pod.png` | 706×620 | O minimapa e a coluna dos 4 botões; o losango é vazado e o mapa vai por baixo | Inteira |
| `unit_card.png` | 786×480 | O cartão da seleção; a janela é vazada e o retrato vai por baixo | Inteira |
| `slot_normal.png`, `slot_action.png` (verde), `slot_captain.png` (dourado), `slot_locked.png` (cinza) | 198×189 | Os botões das grades e da fila | 9 fatias, margem 40 |
| `group_on.png`, `group_off.png` | 231×102 | As abas de grupo, os cartões do topo de 8 e o relógio | 9 fatias, margem 34×30 |
| `alert.png`, `alert_amber.png` | 932×134 | O aviso; o âmbar é o vermelho com a cor girada | 3 fatias: a ponta fica, o fim some |
| `power.png` | 201×274 | A coluna de poderes (ícone em cima, tempo embaixo) | Inteira |
| `panel_wide.png` | 928×223 | O painel do comandante, a dica, a tira da fila, a placa da facção | 9 fatias, margem 50×40 |
| `panel_long.png` | 1626×214 | O painel central | 9 fatias, margem 100×36 |
| `commander_panel.png` | 502×201 | Sobra: tem um quadrado à esquerda que o painel aprovado não tem | — |
| `group_soon.png` | 231×102 | A caixa da superarma que vai sair (a azul com o neon vermelho) | 9 fatias, margem 34×30 |
| `alert_icon_red.png`, `alert_icon_amber.png` | 128×112 | O triângulo do aviso, liso (`art/tools/make_alert_icons.py`) | Inteira |
| `spy_badge_ring.png`, `spy_badge_eye.png` | 64×64 | O selo da espionagem no canto do emblema: o aro (o tempo escurece) e o olho (`art/tools/make_hud_glyphs.py`) | Inteira |
| `defeated_x.png` | 64×64 | O X do derrotado | Inteira |
| `radiation.png`, `radiation_red.png` | 64×64 | O trevo das superarmas (vermelho quando falta pouco) | Inteira |

## Ícones (`icons/`, 256×256, 12/10)

Cada ícone já vem com o próprio quadrado do botão (metal escuro com rebites). A moldura colorida (verde = ação, dourada = melhoria ou capitão) o jogo desenha por cima. Recortados da folha `captures/concept/hud/kit/icons_sheet_gpt.webp` (GPT).

| Arquivo | Uso |
|---|---|
| `attack_move.png` | Attack Move (A), na grade de ações |
| `guard.png` | Guard (G) |
| `stop.png` | Stop (S), a mão branco-acinzentada |
| `rally_point.png` | Rally Point, na grade de ações da fábrica |
| `sell.png` | Sell, o prédio com o cifrão |
| `repair.png` | Repair |
| `armor_upgrade.png` | A insígnia de blindagem (retrato) e o botão de melhoria |
| `locked.png` | O cadeado, sobre o botão bloqueado |
| `settings.png`, `idle_units.png`, `beacon.png`, `chat.png` | Os botões do minimapa |
| `orbital_lance.png`, `artillery_strike.png`, `paradrop.png`, `spy_drone.png` | A coluna de poderes (só o ícone; o nome vai na dica) |

## Fotos das unidades (`photos/`)

- **`photos/buttons/`** (256×256, 12/10): o veículo limpo sobre fundo escuro, no estilo das fotos da paleta aprovada (`../pieces/main_grid_palette.webp`). Arquivos: `usa_battle_tank`, `usa_dozer`, `usa_chinook`, `usa_war_factory`. Recortados de `captures/concept/hud/kit/photos_buttons_gpt.webp`.
- **`photos/card/`** (728×300, 12/10): o retrato largo do cartão, no estilo de `../pieces/unit_card.webp` (foto, deserto desfocado, luz neutra). A foto quadrada do GPT foi estendida pros lados pelo Gemini (originais em `captures/concept/hud/kit/card_gemini/`). Arquivos: os mesmos quatro nomes.

## Fonte

Proposta: **Barlow** (grátis, licença livre SIL OFL), parecida com a do HUD aprovado. A Bahnschrift das imagens é do Windows e não pode ir no jogo. Baixar quando formos montar.
