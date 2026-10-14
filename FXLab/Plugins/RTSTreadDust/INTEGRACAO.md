# Integração da poeira das lagartas no jogo

A poeira mora no plugin `FXLab/Plugins/RTSTreadDust` (a sessão da arte). É a v12 do FXLab, o caminho que o Felipe
escolheu em 13/10 ("v11 é o caminho", rastro longo pro tanque pesado). Tem:
- o jato na roda motriz;
- a nuvem que abre em leque até a altura da torre;
- a cortina ao longo da lagarta;
- a sombra do tanque na poeira;
- a sombra da nuvem no chão.

Um ator só desenha a poeira de todos os veículos. O plugin só desenha: não volta nada pra simulação (o lockstep não muda).

O plugin traz tudo, sem importar asset nenhum:
- o código;
- o perfil (`Resources/tread_v12.json`);
- o atlas da gravação do EmberGen (`Resources/tread_kick_v2c.atlas.json`);
- as duas folhas e os dois materiais (`Content/`, montados pelos mesmos scripts do FXLab).

Quando a poeira mudar no FXLab, a arte roda `FXLab/Tools/pack_tread_dust.py` de novo e avisa; o jogo recopia a pasta.

## Passo a passo

1. **Copiar** a pasta `FXLab/Plugins/RTSTreadDust` inteira pra `Game/Plugins/RTSTreadDust`:
   - não precisa das pastas `Binaries/` e `Intermediate/`;
   - ligar o plugin no `RTSGame.uproject`;
   - pôr `"RTSTreadDust"` nas dependências do `RTSGame.Build.cs`.
   O plugin já liga o `ProceduralMeshComponent`.
2. **Um ator por partida**, criado no cliente (é só desenho):
   ```cpp
   #include "RTSTreadDust.h"
   Dust = GetWorld()->SpawnActor<ARTSTreadDust>();
   Dust->SetQuality(Preset);   // 0 baixa, 1 média, 2 alta, 3 ultra
   ```
3. **A cada quadro**, `Dust->SetSources(Lista)` com **todos os veículos de lagarta perto da câmera**, os parados também. Pode ser em qualquer Tick do jogo: o ator desenha depois, no `TG_PostUpdateWork`, já com a câmera do quadro.
   - O parado vai com `Speed = 0`. Ele não solta poeira, mas continua fazendo sombra nela: se sair da lista, a poeira na sombra dele acende de repente.
   - Quem morreu sai da lista. A poeira dele no ar termina sozinha.

## O `FRTSTreadSource`, campo por campo

| Campo | Do jogo |
|---|---|
| `Id` | o id da unidade (estável de quadro em quadro: guarda os metros andados e a areia presa na lagarta) |
| `PosM` | a posição **desenhada** do casco (a interpolada, a mesma do modelo), em **metros** (cm ÷ 100), com o Z do chão |
| `FwdM` | a frente do casco no chão (X, Y normalizados, Z = 0) |
| `Speed` | m/s com sinal: negativa de ré |
| `YawRate` | a derivada do yaw da Unreal, em **rad/s**: positivo virando pra +Y. No giro no lugar, cada lagarta anda ±ω·1,425 m/s, e isso também levanta poeira |
| `Sand[0]`, `Sand[1]` | o peso da areia no **pé de trás** de cada lagarta, explicado abaixo |
| `SandFront[0]`, `SandFront[1]` | o mesmo na **frente** de cada lagarta |
| `bVisible` | falso sob a névoa de guerra: a poeira do inimigo escondido não aparece, como no Zero Hour |

**A areia vem do `FRTSBakedMap::SurfaceAt`.** Com `S = SurfaceAt(ponto em cm)`, o peso é `1 − max(S.X asfalto, S.W molhado)`.
- Sem a grade do mapa, `SurfaceAt` devolve zero, e o peso fica 1 (areia seca).
- Os pontos, com `F = FwdM` e `R = (F.Y, −F.X, 0)`:
  - `Sand[0]` = `Pos − 2,40·F − 1,425·R`;
  - `Sand[1]` = `Pos − 2,40·F + 1,425·R`;
  - `SandFront[0]` = `Pos + 2,55·F − 1,425·R`;
  - `SandFront[1]` = `Pos + 2,55·F + 1,425·R`.
- O índice 0 é a lagarta do lado −R, e o 1 a do lado +R.
- As medidas são do tanque USA (casco de 7,93 m). Com elas, a poeira:
  - para na hora no asfalto;
  - pinga menos de 1 s com a areia presa;
  - e a cortina para quando a frente da lagarta chega no asfalto.

## A sombra do cenário (14/10)

A poeira escurece na sombra de casas, árvores, pedras e do relevo, e continua recebendo a luz do céu, como o chão.
- **Como funciona:** no 1º Tick, o plugin tira um mapa de profundidade visto do sol, com tudo que projeta sombra.
  - Leva ~350 ms uma vez, ainda na tela de carregar.
  - Fica na CPU (64 MB); a placa devolve o alvo depois de ler. Por quadro não custa nada na placa.
  - Cada cartão consulta o mapa no pé, no meio e no topo.
- **Ficam fora do mapa:** os Pawns e os atores com a etiqueta `RTSUnit` ou `GameTank`. O `ARTSUnitActor` do jogo já leva `RTSUnit`.
- **`RefreshMapShadows()`** refaz o mapa, mas trava ~0,3–1 s. Não chamar no meio da partida: no lockstep a travada de um PC segura todos.
  - Os prédios novos entram quando a leitura for assíncrona.
- **A conferência:** o plugin confere o mapa contra pontos do chão. Se não bater, desliga a sombra do cenário e avisa no log; a poeira segue igual, só sem essa sombra.
- **Depuração:** `-TreadShadowDump` grava o mapa em `Saved/`.

## Os presets

| Nível | O que tem |
|---|---|
| **0 baixa** | só a nuvem: metade dos cartões, e cada um cobre o que os dois sobrepostos cobririam |
| **1 média** | + o jato na roda motriz |
| **2 alta** | + a cortina ao longo da lagarta |
| **3 ultra** | + a sombra da nuvem no chão |

A nuvem, que é o que esconde as unidades, cobre igual em todos os níveis. É a regra da justiça entre presets (`docs/BASE_JOGAVEL.md`): quem joga no baixo não vê mais.

Outras chamadas:
- `SetDustEnabled(false)` apaga tudo na hora;
- `NumCards()` e `LastMs` (a CPU da última conta, em ms) servem pra medir.

## O custo medido no FXLab

Medido no mapa do jogo, na câmera do jogo, em 1080p, na GTX 1660, com a v12 no ultra:

| Tanques andando | Placa | CPU |
|---|---|---|
| 20 | ~+4 ms | 2,2 a 2,8 ms |
| 40 | — | 5,2 a 8 ms |

O que vem a seguir (`captures/fxlab/tread_study/PLANO_OTIMIZACAO.md`):
- **Memória:** as duas folhas são RGBA16F de 3072 × 2304, cerca de 150 MB de placa com as mips. A compressão está na fila de otimização da arte.
- **CPU:**
  - cortar o que está fora da tela antes de ordenar;
  - uma grade pra sombra do tanque;
  - `ParallelFor`;
  - depois, o caminho na placa.

## O que ainda não tem

- **Rodas** (picape, jipe): cada tipo de veículo terá o seu rastro (o tanque pesado usa o longo). Hoje é só lagarta: não mande veículo de roda.
- **Torrões** e **sujeira acumulada no casco**.
- **Diretor de orçamento:** a poeira some primeiro quando o quadro aperta, pelas prioridades do ZH.
