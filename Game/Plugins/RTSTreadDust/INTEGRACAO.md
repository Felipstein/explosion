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
- o perfil (`Resources/tread.json`, a cópia do perfil escolhido no FXLab);
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
- **A conferência** (refeita em 16/10):
  - a orientação é a convenção da câmera ortográfica da Unreal, (+1, −1);
  - a constante e a folga saem dos pontos do chão iluminados;
  - o log traz as 4 orientações;
  - se nada bater, a sombra do cenário se desliga e a poeira segue igual.
- **No jogo** (a `chao`, 16/10): a mediana fica em 32 cm, com espalhamento de 181 cm (o relevo em Nanite e o mapa a 28 cm por pixel), e a folga no teto de 400 cm. Com o sol a 8°, ~0,5 m de relevo desenhado fora da colisão vira ~4 m ao longo do raio.
- **Terreno à parte (16/10, tarde):** o plugin tira duas fotos do sol, com a mesma câmera: o terreno sozinho e depois tudo.
  - Um pixel em que a foto de tudo bate mais de 20 cm antes do terreno é casa, pedra ou árvore. Ele guarda a profundidade do obstáculo, comparada com uma folga fixa de 50 cm.
  - Os outros guardam a do terreno (com o sinal trocado), comparada com a calibração e a folga grande.
  - Antes, a folga de 400 cm valia pra tudo e apagava a sombra da casa nos primeiros ~4 m atrás da parede. Agora a poeira encostada na parede, do lado da sombra, escurece.
  - Custa mais uma foto no carregamento (~0,7 s no total no FXLab) e nada por quadro. A memória é a mesma: um mapa só, com o sinal separando terreno de obstáculo.
  - `-TreadShadowOneMap` volta ao mapa único, pro A/B.
- **Depuração:** `-TreadShadowDump` grava o mapa em `Saved/`.

## A sombra por pixel (16/10 à tarde, EM TESTE, o modo 2)

Pedido do Felipe: "as sombras do mapa não projetam na poeira, ela sempre brilha". Liga pelo bloco `"shadow": {"mode": 2}` do perfil, ou por `-TreadShadowMode=2` (`0` = a de antes, igual bit a bit; `1` = por cartão com o céu de volume). O desenho, a revisão adversária do desenho e a do código estão em `docs/POEIRA_DAS_LAGARTAS.md`.

**O que muda na tela:**
- A borda da sombra da casa, do muro, da palmeira, da duna e do tanque atravessa a nuvem na mesma linha da sombra no chão: o pé escurece e o topo fica aceso.
- Na sombra a poeira recebe o céu de um volume, mais o rebote da areia, e nunca fica mais clara que ao sol. Ao sol, a cor é a de antes.

**Como funciona:**
- **O mapa:** quatro fotos ortográficas em blocos de 256 m, numa grade de 0,5 m girada pro azimute do sol: o chão desenhado, o topo dos obstáculos presos ao chão, e o topo e o fundo das copas (este visto de baixo).
  - Uma varredura ao longo de cada linha dá, em cada ponto do chão, até que altura ele está na sombra, mais a faixa solta da sombra da copa (o sol passa por baixo).
  - As fotos não travam: a placa avisa quando a leitura chegou, e a varredura (~0,3 s) roda numa thread de fundo.
  - O mapa chega ~1 s depois do começo da partida. Até lá a poeira fica no modo 1.
  - No FXLab, o chão da foto bate com os raios no terreno: mediana 0, p75 1,1–1,3 cm.
- **A grade dos tanques:** o topo da sombra das duas caixas de cada veículo, acima do chão desenhado, no mesmo texel do mapa.
  - É refeita só quando algum veículo anda e só sobe o retângulo que mudou.
  - O veículo sob a névoa de guerra não faz sombra (não entrega a posição).
- **O material:** duas leituras de textura por pixel de poeira. A corda que o raio de vista atravessa na nuvem vira uma faixa de altura cortada no chão, e o sol é a fração dela acima da sombra.
- **Presets:** na baixa fica o modo 1 (nada por quadro na placa); da média pra cima, o 2.
- **Proteção:** se o `M_TreadDust` copiado for o de antes (sem o parâmetro `ShadowMode`), o plugin fica no modo 0 e avisa no log.

**O que entra no mapa:** o mesmo conjunto que faz sombra no chão (VSM): malhas com sombra dinâmica, o ator visível.
- **Ficam fora:**
  - o poste (`ShadowSkipMeshes`: a caixa dele é a da cruzeta);
  - o fio;
  - o que tem o lado maior < 1 m ou o menor < 25 cm.
  - O muro da vila (0,36–0,48 m) fica.
- **Copa:** as árvores saem pelo nome da malha (`ShadowTreeMeshes`: `Palm`, `Tree`...).
- **Chão:** o que tem menos de 50 cm de altura (laje, calçada) conta como chão.

**O que a ponte do jogo precisa chamar** (a poeira não sabe da simulação; nada disso trava a partida: cada área é fotografada e lida sem esperar a placa):

| Quando | Chamada |
|---|---|
| prédio que já existe no começo, e prédio novo quando fica pronto (`BuildFraction` ≥ 1; guardar os ids já mandados) | `AddShadowCaster(Ator)` |
| prédio revelado da névoa | `RefreshShadowArea(centro m, raio m)` |
| prédio destruído ou vendido, ANTES do `Destroy()` (a carcaça que fica com o formato não precisa) | `RemoveShadowCaster(Ator)` |
| palmeira e carro derrubados (`F.bDone`), a troca pelo carro amassado, `TopplePole`, as caixas da doca | `RefreshShadowArea(centro m, raio m)` |

Os prédios levam a etiqueta `RTSUnit` e só entram por `AddShadowCaster`. As áreas da fila que cabem em 240 m viram uma foto só.

**Campos novos no `FRTSTreadSource`** (ABI: compilar o RTSGame junto com o plugin):

| Campo | O que é |
|---|---|
| `HalfWidthM`, `HullTopM`, `HeightM`, `HalfLenM` | as caixas da sombra de cada tipo (o padrão é o tanque USA) |
| `TurretFwdM` | a frente da torre (zero = a do casco) |
| `bEmitsDust` | falso: só faz sombra e empurra o ar (o veículo de roda, a carcaça) |

A ponte pode mandar os veículos de roda e as carcaças como fontes só de sombra.

**Depuração:** `-TreadHMapDump` grava em `Saved/`:
- `hmap.raw`: os 4 canais;
- a foto de cima com luz, a cor do chão e o que é telhado ou copa, num bloco de 256 m em volta da câmera.

Com isso se confere a sombra prevista contra a desenhada.

## A poeira em VOLUME (16/10 à noite, EM CONSTRUÇÃO; substitui os cartões quando o Felipe aprovar)

A regra do CLAUDE.md (toda fumaça e poeira com volume, sombra do mapa, sombra própria no chão e nos tanques, acesa pelo
fogo e pelas explosões; um sistema só) vale pra poeira. Ela vira mais um meio no raymarch das explosões (o
`BlastRaymarch` do Fluxo, no `RTSShaders`); a simulação da v26 não muda.
- **O módulo:** `RTSDustField` (neste plugin; Runtime, PostConfigInit, sem UObject). Interface em
  `Source/RTSDustField/Public/RTSDustField.h` e `Shaders/Private/DustField.ush` (`RTS_DUST_FIELD_GLOBALS 1`).
  - O ator manda os puffs a cada quadro (`SubmitFrame`).
  - O raymarch chama `RTSDustField::AddPasses` no passe dele e soma `RTSDustAt` / `RTSDustLightAt` / `RTSDustSkipM` no
    `MarchJoint`.
- **Os arquivos** (17/10; a gravação do EmberGen das folhas da v26, em volume, comprimida com zlib). Sem eles a poeira
  fica nos cartões.
  - `Resources/dust_kick_v2_05.rtsdust`: a fina, 0,05 m, 1,8 MB no disco e 42 MB na placa, pro Ultra.
  - `Resources/dust_kick_v2_r2.rtsdust`: a grossa, 0,1 m com a média de ρ² (a massa certa), 0,3 MB e 5 MB na placa, pra
    Alta. Sai da fina por `FXLab/Tools/build_dust_coarse.py`.
  - `dust_kick_v2.rtsdust` (0,1 m com a média de ρ, a de 16/10) não é mais usado.
- **A chave:** `rts.TreadDust.Path` 0 = os cartões (padrão), 1 = o volume. No 1 os cartões e a sombra deitada somem.
  O ajuste fica no bloco `"volume"` do perfil: `density` 1,0, `kick` (a fina), `kickCoarse` (a grossa), `erode`,
  `detail`, `albedoMax`.
  - σ = 6·ρ²·density: as folhas aprovadas foram cozidas com ρ² (o Blender multiplica a grade de novo).
  - Com a gravação de 0,05 m, o volume bate com o alfa das folhas sem fator nenhum (τ 0,94–1,01).
  - O 0,62 de 16/10 remendava o ρ linear com a gravação de 0,1 m e não vale mais com este shader.
- **No jogo não precisa de gancho novo:** os veículos, a névoa de guerra e as explosões continuam pelo
  `SetSources` / `AddBlast`. As luzes vêm do `RTSSceneLights`, pelo Fluxo.
- **Antes de copiar:** copiar só quando a Arte avisar, com o jogo compilando junto. O plugin inteiro vai junto: Source,
  Shaders e Resources (o `tread.json` com as gravações). O `.ush` e o `.cpp` andam juntos. Sem binário, o editor de
  todos não abre (aconteceu às 20:23 de 16/10).
- **Depuração (só o FXLab):** `rts.DustField.Debug` 1 = a luz com a conta do raymarch, 2 = a densidade, 3 = o SunT,
  4 = o SkyT, 5 = a luz local, 9 = só os passes do campo (a medida do custo).
- **Os presets:** pelo `sg.EffectsQuality`; `rts.DustField.Fine` 1/0 força.
  - 4 (o Ultra do jogo) = 0,125 m, metade dos tijolos por quadro e a gravação fina.
  - Abaixo = Alta: 0,25 m, um quarto dos tijolos e a gravação grossa.
  - Até 17/10 o corte era 3, e a Alta do jogo (nível 3 do motor) pagava o Ultra.
- **O custo dos passes do campo** (40 tanques a 1080p):
  - Ultra: +4,1 ms contra o campo desligado, com a gravação fina; +3,7 ms com a de 0,1 m. A fina custa +0,4 ms no pior
    caso, pelo cache.
  - Alta: −0,8 ms contra os cartões (16/10).
  - No jogo (luz_prova_67, 3 tanques e 2 mortes): +0,4 ms em média.
- **A luz, no raymarch** (os ajustes por instância das explosões, medidos na luz do jogo): céu × 3,4, sol 27 % inteiro,
  o sol × MapV fora do lerp das oitavas; as 8 luzes mais fortes do `RTSSceneLights`.
- **Ligado no jogo desde 16/10 à noite** (o Fluxo, no `MarchJoint`): `rts.TreadDust.Path=1`; `-RmTreadDust=0` tira a
  poeira só do raymarch.

## A cor do chão (16/10)

Cada nuvem nasce com a cor do chão onde o veículo passa: areia clara dá poeira clara; sal, terra e asfalto dão a cor deles. É o pedido do Felipe de 14/10.
- **A captura:** a BaseColor de todo componente do ator do terreno (inclusive o LandscapeNaniteComponent), de cima, em 2048².
  - Sai no 1º Tick depois dos shaders prontos (~0,2–0,8 s, ainda no carregamento) e fica na CPU (16 MB).
  - As frestas pretas entre pedaços do terreno ficam fora da média.
- **No material:** a regra de Hapke. O pó fino fica mais claro e mais pálido que o chão, e um ganho só (G) vale pra todo chão, calibrado contra o game shot r1 na mesma pose.
- **Sem captura** (o log avisa): a cor da reserva do perfil.
- **Depuração:** `-TreadGroundDump` grava `Saved/ground_color.bmp`.

## A máscara de unidade (16/10)

O material lê o bit 128 do estêncil (o `ARTSUnitActor` do jogo já põe), mais a profundidade da cena. Onde o ponto atrás é unidade ou prédio, acima da roda motriz (0,6–0,96 m), o alfa da poeira fica em no máximo 0,1. A lagarta de baixo pode ficar empoeirada, como no alvo; casco e torre, não.

## A interação: os outros veículos e as explosões empurram a poeira (16/10, em teste)

Pedido do Felipe ("igual o BlastLab fez com a Carga Pesada: o míssil e o avião empurravam a fumaça"). O estudo com fontes e a revisão adversária estão em `docs/POEIRA_DAS_LAGARTAS.md`. Liga pelo bloco `"interact"` do perfil (a v25) ou por `-TreadDustInteract=1`.
- **Os veículos** já vêm todos em `SetSources`. Cada um empurra a poeira dos outros; a do próprio fica como foi aprovada.
  - **A proa:** o corpo de Rankine no chão. Fonte e sumidouro suavizados pela área frontal; o ar abre em volta do casco e fecha atrás.
  - **A esteira:** as migalhas da traseira. A força é ancorada no `inherit` do perfil, então a poeira do outro na esteira anda igual à do próprio.
  - Campos novos no `FRTSTreadSource`: `HalfLenM`, `HeightM` e `FrontalM2`. O padrão é o tanque USA; o caminhão e o Dozer mandam os deles.
  - Veículo parado não empurra.
- **As explosões:** `AddBlast(FRTSDustBlast)` onde o jogo chama o BlastFx, com a **escala que ele desenha** (`Scale`: 1 = a Carga Pesada).
  - O ar sai em r/340 s e anda por 0,15 s. Dentro da bola desenhada (9 m × escala) não empurra.
  - A boca do canhão leva `DirM` e `HalfConeDeg` (60).
  - `bVisible` falso sob a névoa: não empurra.
  - Dois modelos, pro A/B do Felipe (`"blastModel"` ou `-TreadDustBlast=`):
    - `aprovado`: o pulso da fumaça do WebGL e do BlastLab, 18 m/s na borda por 0,15 s;
    - `kg`: Kinney–Graham, a física; empurra ~9× menos.
- **As regras de leitura:** nada sobe, o alfa nunca aumenta e a máscara de unidade continua valendo. A rampa de 0,15 s entra quando um veículo sai da névoa.
- **A pausa:** `SetPaused(true)` congela a poeira junto com a partida, valendo também pra pausa da rede.
- **A revisão do código (16/10):** 4 revisores e 1 verificador por achado; 13 achados confirmados. Corrigidos:
  - o relógio da `-TreadLabBlast`: o laboratório disparava a explosão no aquecimento, antes da 1ª foto;
  - a esteira com ré e meia-volta: cada trecho contínuo conta à parte, e os trechos somam;
  - a rampa sai junto com quem sai da lista;
  - a explosão sai da lista só depois do passo que fecha o empurrão;
  - a sombra do cenário amostra só na horizontal: com o sol baixo, a amostra de baixo sombreava o pé da poeira;
  - o mapa refeito é lido no passo seguinte, sem acender por 1–2 quadros;
  - os comentários de lado das lagartas (+Y = a direita, na Unreal).
  - **Fica pra depois:** o alvo retangular da foto do sol (hoje 4096², ~80% vazio com o sol baixo; ganho de 2–5× no tempo de carregamento) e medir o custo da interação com duas passadas (`-TreadDustInteract=0/1`), porque o `-TreadAB` congela a cena.
- **Os ganchos no jogo** (dono: o jogo):
  - o sopro da boca no `WeaponFired`, com escala 0,15 e cone de 60°;
  - o acerto no chão e o raspando (`GroundHit`, 0,25);
  - o acerto no casco (`ArmorHit`, 0,28);
  - a morte de tanque (0,55) e a de prédio, com a escala de cada uma.

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
- **O mip mínimo (16/10, tarde):** a folha é lida no mínimo no nível `mipMin` do perfil (a v24: 3,5), com `max` sobre o nível da distância.
  - O borrado tem o mesmo tamanho no mundo em todo zoom. Antes era um bias somado ao nível da distância (o `mipBias`): de longe virava mingau, e o alfa espalhado vazava do octógono, que dava quinas retas no jogo.
  - O octógono agora é medido na folha borrada (`Tools/atlas_cut8.py <bake> 0.002 1 4.0`: o nível 4, meio acima do mínimo, com limiar 0,002). Ficou 16% maior.
  - Os perfis antigos com `mipBias` viram `mipMin = mipBias + 1`.
- **Memória:** as folhas padrão são as normalizadas em BC7, cerca de 19 MB de placa (eram 150 MB em RGBA16F). No A/B de 14/10 elas saíram iguais no olho. As HDR (`Content/tread_kick_v2c`) ficam só pra comparar (`-TreadDustSheets=hdr`) e podem sair do pacote.
- **CPU:**
  - cortar o que está fora da tela antes de ordenar;
  - uma grade pra sombra do tanque;
  - `ParallelFor`;
  - depois, o caminho na placa.

## O que ainda não tem

- **Rodas** (picape, jipe): cada tipo de veículo terá o seu rastro (o tanque pesado usa o longo). Hoje é só lagarta: não mande veículo de roda.
- **Torrões** e **sujeira acumulada no casco**.
- **Diretor de orçamento:** a poeira some primeiro quando o quadro aperta, pelas prioridades do ZH.
