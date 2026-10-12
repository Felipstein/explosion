# Kit do HUD pro jogo (USA)

As peças prontas pra montagem em Slate, recortadas pela sessão de arte. A regra de montagem está em `../DECISOES.md`, os nomes em `../NOMES.md`.

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
