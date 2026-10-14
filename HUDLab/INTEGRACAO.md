# Integração do HUD no jogo (pra quando o Felipe aprovar)

O HUD mora no plugin `HUDLab/Plugins/RTSHud`, que só desenha um `FRTSHudState`. Pra ir pro jogo:
1. O plugin vai pro `Game/Plugins/`.
2. O `ARTSHUD` do jogo preenche o `FRTSHudState` a partir da simulação e chama `FRTSHudPainter::Draw`.
3. O jogo liga os cliques pelos ids dos botões.

## O que a sessão do jogo passou (12/10/2026)

**De onde ler** (as mesmas chamadas do `Game/Source/RTSGame/Private/RTSHUDEconomy.cpp`):
- **Seleção** (`ARTSPlayerController`):
  - `GetSelection()`;
  - `SelectedStructure()`;
  - `SelectedWith(true = Dozers / false = caminhões, Out)`;
  - `GetPlacingDef()`;
  - `IsAttackMoveArmed()`.
- **Grupos:** o `TMap Groups` do controle é privado hoje. A sessão do jogo faz um getter público; é só pedir o nome.
- **Unidade:** `Match->GetUnit(Id, FRTSUnitState)` traz vida, dono, tipo, `BuildFraction` (obra), `QueueFraction` (fila) e carga. `GetUnits(Out)` devolve todas.
- **Tipo:** `Match->GetDef(Def, FRTSDefInfo)` traz nome, custo, `BuildSeconds`, `Produces` (o que o prédio treina), `Constructs` (o que o Dozer constrói), `Energy` e `bArmed`.
- **Economia:**
  - `Money(Player)`, `PowerMade`, `PowerUsed`, `ProductionSpeed`;
  - `GetQueue(Structure, Out)`;
  - `HasPrerequisites(Def)`;
  - `MatchSeconds()`;
  - `LocalPlayer()`.
- **Patente, poderes e superarmas:** não existem na simulação. A coluna de poderes, o Promote e as listas de superarmas ficam vazios até existirem.

**Regras da sessão do jogo:**
1. **Os ids dos botões não mudam**, porque o `OnButton` do controle e o teste automático acham os botões por eles:
   - `eco_build_<def>`, `eco_produce_<def>`, `eco_cancelq_<k>`, `eco_cancelbuild_0`;
   - os `lan_*`, `lobby`, `skirmish` e `fogsetting`.
   - Id novo: avisar a sessão do jogo, que liga no `OnButton`.
2. **Imagens:** o `build_content.py` é da sessão do jogo. O kit entra por um `Tools/import_hud_kit.py` nosso, rodado com `game.ps1 py Tools\import_hud_kit.py` (Unreal sem janela, sem placa). Os assets ficam em `/Game/HUD/`.
3. **Compilar:** `game.ps1 compile`, com nenhuma Unreal aberta no `RTSGame.uproject`. Mandar "vou compilar" antes e "compilado" depois.
4. **Abrir:** `game.ps1 view` já entra na fila da placa.
5. **Testes:**
   - `game.ps1 view -Scenario chao -Folder X` abre uma partida de 6 contra 6 em ~40 s (tanques selecionados);
   - `-Script base` faz a base e a fábrica (~6 min).

## Passo a passo pro primeiro teste jogando (13/10/2026)

O Felipe quer testar o HUD novo jogando. O plugin já traz tudo pro teste sem importar asset nenhum.

1. **O plugin:** copiar `HUDLab/Plugins/RTSHud` pra `Game/Plugins/RTSHud`, ligar no `RTSGame.uproject` e pôr `"RTSHud"` nas dependências do `RTSGame.Build.cs`.
2. **As imagens:** `FRTSHudKit Kit(RepoRoot / "docs/concept/hud/kit", RepoRoot / "HUDLab/LabAssets")`.
   - O lookup do painter pode responder o "minimap" com a textura do minimapa do jogo, que pode ser um render target: o lookup devolve `UTexture*`. O resto vai pro `Kit.Get(Name)`.
   - O mapa vai desenhado girado 45° dentro do losango; o painter já faz isso.
3. **A fonte:** `FRTSHudPainter Painter(Lookup, RepoRoot / "HUDLab/Fonts")`. No jogo final, as fontes vão pro pacote.
4. **Uma chave, sem trocar o HUD de hoje:** `-NewHud` na linha de comando (ou um console var). Com ela ligada, o `ARTSHUD::DrawHUD` monta um `FRTSHudState` e chama `Painter.Draw(Canvas, State, Buttons)`.
5. **O clique:** o `Draw` devolve os botões (`Id` + caixa na tela). O clique acha a caixa debaixo do mouse e chama o `OnButton(Id)` de hoje.
   - Os ids do painel são os que o jogo já usa (`eco_build_<def>`, `eco_produce_<def>`, `eco_cancelq_<k>`...).
   - Os ids novos são `group_<n>`, `power_<i>`, `mm_settings`, `mm_idle`, `mm_beacon`, `mm_chat`, `promote` e `queue_<i>`.
   - **`hud_bg`** (13/10): o fundo de cada peça, pra o clique não passar pro mundo. Abrange as barras do topo, as caixas de superarma, o comandante, os avisos, o minimapa inteiro, o painel central, o cartão e a tira da fila.
     - Esses itens vêm no fim da lista: pegue o primeiro botão que contém o mouse.
     - O losango do minimapa fica dentro do `hud_bg` do minimapa, então confira o clique no minimapa antes dos botões.
6. **O estado, campo por campo (de onde ler):**

| Campo do `FRTSHudState` | Do jogo |
|---|---|
| `Players` (nome, emblema `emblem:usa/china/gla`, cor, `bMe`, `bAlly`, `bDead`) | os jogadores da partida; com 2 jogadores sai o topo 1x1 |
| `Clock` | `MatchSeconds()` em mm:ss |
| `Money`, `PowerUsed`, `PowerMade` | `Money(LocalPlayer())` ("$ 4,250"), `PowerUsed`, `PowerMade` |
| `Stars`, `Xp`, `Promote`, `Powers`, `Superweapons` | não existem na simulação: vazios |
| `Alerts` | os avisos do jogo ("Unit under attack", "Low power" em âmbar) |
| `MinimapImage`, `MinimapCamera` | a textura do minimapa e o retângulo da câmera em UV (0..1) |
| `Groups` (10) | o getter dos grupos que a sessão do jogo ia fazer: quantidade e vida somada |
| `Main` (2 × `MainColumns`), `Actions` (2 × 3) | as unidades selecionadas (uma casa por unidade, `bCaptain` na principal) ou o que se constrói/produz; as ordens com `bAction` |
| `Card` | a seleção principal: nome, vida, retrato `card:<def>`, `Queue` da fábrica; sem seleção, `bFactionPlate` |
| `TooltipFor` e o resto da dica | o botão debaixo do mouse |

7. **Grade:** `MainColumns = 7` é o padrão (casas do tamanho do clássico; decidido pela arte em 13/10). A de 9 continua disponível pra comparar.

## O que muda do laboratório pro jogo

- As imagens:
  - **no laboratório e no primeiro teste** saem direto dos PNG do kit, com mipmaps feitos na hora (`FRTSHudKit`, no plugin);
  - **no jogo** viram assets em `/Game/HUD/`, com mipmaps e grupo UI, e o lookup do painter aponta pra eles;
  - os nomes que o painter pede são `skin:<peça>` (as molduras de `kit/skin/`), `icon:`, `btn:`, `card:`, `emblem:` e `minimap`.
- As fontes: no laboratório saem dos `.ttf` em `HUDLab/Fonts/`. No jogo, vão pra `Game/Content/` ou pra pasta de fontes do pacote (Barlow, SIL OFL; a licença vai junto e entra no `CREDITOS.md`).
