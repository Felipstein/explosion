# As marcas que contam a batalha

Estudo de 16/10/2026 (pedido do Felipe: "faça então", `docs/BASE_JOGAVEL.md`, itens de 16/10). O alvo: no fim de uma
partida longa, olhando o mapa, ver onde foram as lutas. As marcas se acumulam durante a partida; o mundo não começa
marcado (o "mapa que já começa destruído" é outra coisa, opção do WorldForge).

Só leitura de código e pesquisa: nada foi mudado, nenhuma Unreal aberta. Os números marcados **(estimativa)** não
foram medidos; os outros saem do código ou das fontes.

**Resumo em uma linha:** o jogo já risca quase tudo o que precisa (crateras, queimado, avental do casco, trilhas,
destroços), mas guarda a batalha inteira num caderno que ninguém mostra, perde as marcas da névoa e do anel cheio, e
não tem nada que *acumule* (o caminho batido, o campo revirado, a vegetação queimada). O que mais conta a história
por menos custo é mostrar o caderno do mapa inteiro (no minimapa e no chão de longe) e somar um caderno novo de
"acúmulo" com quatro canais.

---

## Parte 1: o que existe hoje (o código de 16/10)

### 1.1 A simulação (`Game/Source/SimCore`)

**A carcaça** (só veículos; prédio não vira carcaça na simulação):

- Nasce no lugar da morte, com o giro do casco: `World.cpp:2194-2206`.
- **Teto de 256**, a mais velha sai (fila): `World.cpp:37` (`MaxWrecks`) e `World.cpp:2203-2206`
  (`Wrecks.erase(Wrecks.begin())`). Não há tempo de vida: fica até a 257ª morte.
- **Pode ser empurrada:** `World::PushWrecks`, `World.cpp:1800-1956`. O casco são dois círculos no eixo
  (±0,45 R, raio 0,6 R, `World.cpp:38-39`); a massa ∝ raio², a mesma do vivo do mesmo tipo (`World.cpp:1837-1846`);
  a batida torta de ±9° (`World.cpp:1857-1859`); anda até R/4 e gira até 2° por passo (`World.cpp:40`, `:1918-1928`).
- **Trava** contra penhasco e prédio: se a célula não é andável, fica travada 15 passos = 0,5 s
  (`WreckStuckTicks`, `World.cpp:41` e `:1930-1939`); travada, o vivo leva o empurrão inteiro (`:1846`).
- **Bloqueia?** Só pelo contato físico. O planejador de caminho **não conhece a carcaça** (não há leitura de `Wrecks`
  fora de `PushWrecks`, `RemoveDead` e do hash, `World.cpp:2523-2524`): o tanque traça a rota por cima e empurra.
- **O tiro não a destrói:** o projétil só faz dano no alvo vivo (`World.cpp:2007-2019`) e o dano só vai pra
  unidades (`World::ResolveDamage`, `World.cpp:2107-2123`). Não há dano em área. Ela também não é alvo nem
  cobertura.
- Entra no hash (`World.cpp:2523`): igual em todo PC.

**O prédio que cai:** a pegada volta a ser terreno andável na hora (`SetFootprint(..., false)`, `World.cpp:2179-2181`).
A simulação não guarda ruína nem entulho.

**A carga que cai** (o caminhão destruído): vira pilha-depósito até ser recolhida (`WorldEconomy.cpp:803-815`).

**Eventos que o desenho recebe** (`Events.h:13-28`): `WeaponFired`, `ProjectileHit`, `UnitDamaged`, `UnitDied`,
`CargoDropped`. Não existe evento de carcaça que sai, nem de carcaça empurrada.

### 1.2 O desenho (`Game/Source/RTSGame`, `BlastFx`)

**A memória do chão** (`RTSGroundMemory.cpp`):

- **Três cadernos:**
  - o **grosso**, o mapa inteiro, 2048² a 50 cm por pixel (`:80`, `:867-871`). RGBA8: R esteira, G escuro
    (fuligem, combustível, halo), B calor (a brasa), A claro (torrões, borda, raios); mais a altura das crateras
    (`CoarseCrater`, G16R16F);
  - o de **detalhe**, 192 m em volta da câmera, 4,7 cm por pixel (`:84-87`);
  - o do **meio**, 384 m em volta da câmera, ~19 cm por pixel (`:99-103`).
  Detalhe e meio têm também o perfil, o relevo e (com a v3) a luz assada.
- **O que desbota:** a cada 2 s o caderno é multiplicado por (R × 0,987, G × 1, B × 0,88, A × 1)
  (`:69-73`, `:2002-2008`):
  - a esteira (R) cai a 14% em ~5 min (`0,987^150`);
  - a brasa (B) cai a 15% em ~30 s;
  - **o escuro (G) e o claro (A) não desbotam nunca**; a altura da cratera também não.
- **O relevo da trilha** (sulco e bordinha, `GroundRelief.usf:11` e `:87`): a idade tira até 45% do sulco e 30% da
  bordinha, e para aí. A trilha velha fica a 55% enquanto a fita estiver guardada.
- **Os anéis do que fica guardado** (`:93-95`):
  - **262.144 pedaços de trilha** (`MaxQuads`): pelo comentário, "~5 min de 50 tanques andando à vista";
  - **16.384 cicatrizes** (`MaxScars`): crateras, manchas e aventais.
  Cheio, o mais velho é sobrescrito (`:1086-1095`, `:1164-1173`, `:1203-1212`). Detalhe e meio se **redesenham da
  lista** quando a câmera anda (`Redraw`, `:1821-1881`): o que saiu do anel **some deles** no próximo redesenho. O
  grosso nunca se redesenha, então guarda tudo.
- **Quem lê cada caderno:**
  - a MF do Look (`Game/Tools/look_mf.py:41-48`, `:96`) lê **só o detalhe e o do meio**; fora da janela de 384 m
    não há marca nenhuma (`if (wc <= 0.001) return`, `:48`);
  - o **grosso só alimenta o decal** do mapa (`:969`, `:973`), e o decal **é escondido** quando o chão já tem a MF
    lendo o meio (`:769-776`), que é o caso do jogo hoje. **O caderno do mapa inteiro é escrito e ninguém vê.**
- **O que risca cada coisa** (`RTSMatchSubsystem.cpp`):

| Evento | Marca | Onde |
|---|---|---|
| Tiro que acerta o tanque | mancha fraca de raio 1,3 m (escuro 0,35) | `:2019-2022` |
| 20% desses tiros (near miss) | cratera A (bacia ~1,7 m) 6–12 m além do alvo | `:2023-2045`; `Units.json:56-58` |
| Tiro no chão (Ctrl) | cratera da arma | `:1983-1997` |
| Morte de tanque | avental do casco (queimado + combustível, brasa ~30 s), atlas C | `:2190-2200`, `AddWreck :1177-1214` |
| Prédio que cai | mancha na pegada + 3 crateras B | `:2374-2378`, `:2529-2534` |
| Lagarta | fita com garra e relevo | `:2503-2511`, `TrackUnit :1021-1105` |
| Carcaça arrastada | risca como esteira de lado | `:2274-2279` |

- **A névoa:**
  - a morte de veículo dentro da névoa é guardada e o avental aparece quando a área é revista
    (`HiddenMarks`, `:2203-2207`, `RevealHiddenMarks :2285-2307`);
  - **o tiro e a cratera dentro da névoa não são guardados nunca** (todos passam por `IsPointShown`,
    `:1983`, `:2002`, `:2036`);
  - **a queda de prédio na névoa também não deixa nada** (`StructureDeath` só roda se `bSeen`, `:2180-2183`);
  - a lagarta só risca onde o jogador vê (`:2510`).

**A carcaça desenhada** (`RTSUnitActor.cpp`):

- Na morte, o veículo é carbonizado (cor 0,07, rugosidade 0,95, sem cor de jogador: `Char`, `:788-823`); a v2
  tirou o fogo, a luz e a brasa do veículo (`:913-921`).
- A torre voa em toda morte, cai no lado livre e fica lá (`:714-786`, `:878-912`).
- O prédio: afunda 42% da altura em 3,5 s e se inclina (`:87-89`, `:867-877`); a luz do fogo apaga em 30 s
  (`WreckFireSeconds`, `:73`, `:923-933`).
- **O ator da carcaça nunca sai** (`RTSMatchSubsystem.cpp:2215`; só limpa no fim, `:3126`). Quando a simulação
  tira a carcaça mais velha (a 257ª morte), o ator fica no mapa **parado e sem colisão**: `UpdateWreckPoses` só
  pula (`:2236-2239`). Os tanques passam por dentro dela.
- O mesmo com o prédio: a ruína afundada fica desenhada, e a simulação já liberou a pegada. **Os tanques passam por
  dentro da ruína.**

**Os destroços e as pedras** (`RTSDebris.cpp`):

- 5–8 peças por morte de tanque (roda, lagarta, chapa, escotilha, caixa, lascas), a 4,5–9 m, 70% pra longe do
  atirador (`:210-250`).
- Instâncias por célula de 64 m, sem colisão, queimadas (`:254-289`).
- As pedras da cratera: graúdas (≥ 20 cm) e miúdas; as miúdas somem a 70–80 m da câmera; nada abaixo de 9 cm
  (`:39-47`, `:175-178`).
- **Ficam a partida toda.** Tetos:
  - 1.500 peças, ~250 mortes (`:35`);
  - 8.000 pedras graúdas e 60.000 miúdas (`:42-43`).
  Passou do teto, **as novas ficam sem nada** (`:168-171`, `:198-201`): a luta mais recente é a que perde os
  destroços.

**A fumaça que fica** (`BlastFxDirector.cpp`):

- A carcaça: fumaça escura e fogo pequeno começam 4 s depois da morte e duram 30 s, mais 4 s pra apagar
  (`:114-116`, `:806-826`, `StartWreck :855-874`). Só com o tiro novo ligado (`ArmorHit` com `bKill`, `:673`).
- A explosão sai quando a gravação acaba (a `eg_tq_v2` tem ~3 s, `:411-430`).
- O tanque ferido solta fumaça abaixo de 70% e de 35% da vida, e para na morte (`BlastFxDirector.h:66-72`).
- `RTSEffect.cpp`: clarões e traçantes de décimos de segundo (`:52-64`).
- **Nada de fumaça passa de ~34 s.** A fumaça mostra a luta de agora; a história fica no chão.

**O minimapa** (`RTSHUD.cpp:924-938`): é a foto de cima tirada na tela de carregar (`Fog->MinimapTexture()`). **Não
mostra marca nenhuma.** Num mapa de 1024 m, o losango tem ~222 px a 1080p: ~4,6 m por pixel.

### 1.3 A tabela: o que some e quando

| Marca | Onde mora | Dura quanto | Limite | Arquivo:linha |
|---|---|---|---|---|
| Carcaça (física) | simulação, `Wrecks` | até a 257ª morte | 256, sai a mais velha | `World.cpp:37`, `:2203-2206` |
| Carcaça (modelo) | ator `ARTSUnitActor` | a partida toda; depois que a simulação a tira, fica parada e **sem colisão** | nenhum | `RTSMatchSubsystem.cpp:2215`, `:2236-2239` |
| Torre arremessada | componente do ator | a partida toda | nenhum | `RTSUnitActor.cpp:714-786` |
| Ruína do prédio | ator afundado 42% | a partida toda; a simulação já liberou a pegada | nenhum | `RTSUnitActor.cpp:87-89`; `World.cpp:2179-2181` |
| Fogo do prédio (luz) | ator | 30 s | — | `RTSUnitActor.cpp:73`, `:923-933` |
| Fumaça e fogo da carcaça | `BlastFxDirector` | 4 s de espera + 30 s + 4 s | uma por carcaça | `BlastFxDirector.cpp:116`, `:806-826` |
| Explosão da morte | `AExplosionVolume` | ~3 s (a gravação) | orçamento do raymarch | `BlastFxDirector.cpp:411-430` |
| Esteira (escuro e garra, R) | cadernos | 14% em ~5 min | anel de 262.144 pedaços | `RTSGroundMemory.cpp:69-71`, `:94` |
| Sulco e bordinha (relevo) | detalhe e meio | ficam a 55% e 70% até sair do anel | anel de 262.144 | `GroundRelief.usf:11`, `:87` |
| Brasa do avental (B) | cadernos | 15% em ~30 s | — | `RTSGroundMemory.cpp:72-73` |
| Escuro (G: queimado, combustível, halo) | cadernos | a partida toda | anel de 16.384 (detalhe e meio); o grosso, sem limite | `:93-95`, `:2004` |
| Claro (A: torrões, borda, raios) | cadernos | a partida toda | idem | idem |
| Altura da cratera | `*Crater` (RG16F) | a partida toda | idem; a soma trava em −90 / +40 cm | `:1216-1306`; `GroundCrater.usf:158-159` |
| Cratera e tiro na névoa | — | **nunca riscada** | — | `RTSMatchSubsystem.cpp:1983`, `:2002`, `:2036` |
| Prédio que cai na névoa | — | **nunca riscado** | — | `RTSMatchSubsystem.cpp:2180-2183` |
| Avental de morte na névoa | `HiddenMarks` | aparece quando a área é revista | sem teto | `RTSMatchSubsystem.cpp:2203-2207`, `:2285-2307` |
| Destroços do tanque | ISM por célula de 64 m | a partida toda | 1.500 peças; as novas ficam sem | `RTSDebris.cpp:35`, `:198-201` |
| Pedras da cratera | ISM | a partida toda | 8.000 graúdas, 60.000 miúdas; as miúdas somem a 70–80 m | `RTSDebris.cpp:42-47`, `:168-171` |
| Caderno do mapa inteiro (50 cm) | `CoarseTarget` | a partida toda | — | `RTSGroundMemory.cpp:80`, `:870` |
| …e quem o mostra | o decal | **escondido no jogo** (a MF lê o meio) | — | `RTSGroundMemory.cpp:769-776` |
| Pilha de carga | simulação, `Docks` | até ser recolhida | — | `WorldEconomy.cpp:803-815` |

### 1.4 Os furos que impedem "ver onde foram as lutas"

1. **O caderno do mapa inteiro não aparece.** Ele guarda todo o escuro e o claro da partida a 50 cm, mas só o decal
   o lê, e o decal está escondido. Fora da janela de 384 m e no minimapa não há marca.
2. **O anel cheio apaga a luta antiga** do detalhe e do meio. Numa partida grande, depois de 16.384 cicatrizes, as
   primeiras somem quando a câmera volta lá.
3. **A névoa engole crateras e prédios.** Só a morte de veículo é guardada pra depois.
4. **A simulação e o desenho discordam:** a 257ª morte deixa uma carcaça-fantasma (vista, sem colisão), e toda
   ruína de prédio é atravessável.
5. **Nada acumula.** A trilha satura em 2 passadas (`TrackPass` 0,55, `:91-92`) e desbota em 5 min. Não há "muitos
   passaram aqui", "muitas bombas caíram aqui", "o mato queimou".
6. **Os destroços acabam na luta errada:** passado o teto, a mais recente fica sem nada.

---

## Parte 2: como os jogos fazem (as fontes)

Pesquisa de 16/10 com dois agentes. As fontes mais fortes são **código de verdade**: o fonte do ZH que a EA liberou, o
Lua do Forged Alliance (FAF) e o motor do Beyond All Reason (Spring/Recoil). Depois vêm as palestras da DICE. Da
Relic, da Eugen e da Creative Assembly não há palestra técnica pública sobre isso: o que há são diários, wikis e
fóruns, e está marcado assim.

Os links curtos:
- **[EA]** = `github.com/electronicarts/CnC_Generals_Zero_Hour/blob/main/GeneralsMD/Code/`;
- **[INI]** = `github.com/TheSuperHackers/GeneralsGamePatch/blob/main/Patch104pZH/GameFilesOriginalZH/Data/INI/`
  (os INI originais do ZH 1.04).

### 2.1 C&C Generals Zero Hour (o fonte aberto)

| Marca | Como o ZH faz | Quanto dura | Fonte |
|---|---|---|---|
| Lagarta | até 100 veículos riscam; fila de segmentos, os últimos opacos, desbota pelo tempo | o padrão do código é 300 s, mas o `GameLOD.ini` sobrescreve: **Baixa 5 s, Média 30 s, Alta 60 s** | [INI]`GameData.ini` (`MaxTerrainTracks = 100`), [INI]`GameLOD.ini`, [EA]`W3DTerrainTracks.cpp` |
| Queimado | `MAX_SCORCH_MARKS = 500`, atlas de 9 tipos; o novo a menos de raio/4 de um igual **não é criado** | **a partida toda**; passou de 500, sai o mais velho; só no cliente | [EA]`BaseHeightMap.h` ~l.245, `BaseHeightMap.cpp:2051` |
| Quem queima | a peça `TerrainScorch` em ~47 efeitos: a morte de tanque com raio 15, a artilharia 15–50, a MOAB 250; **a morte de prédio não queima** | — | [INI]`FXList.ini`, [EA]`FXList.cpp:458` |
| Cratera de verdade | **só por script do mapa** (`BlastCraterGenerator`, `createCraterInTerrain` só abaixa); arma normal nunca afunda o chão | permanente, muda o caminho | [EA]`ScriptActions.cpp:1025`, `KindOf.h:156`, `AIPathfind.cpp:4144` |
| Carcaça | objeto próprio (`DeadCrusaderHulk`, `NO_COLLIDE HULK`, sem vida): espera ~1,5 s, afunda e some | **10 s** (Crusader), 12 s (Scorpion), **20 s** (Battle Master, com fumaça); nunca bloqueia | [INI]`Object/Hulk.ini:1350`, `:536`; [EA]`SlowDeathBehavior.cpp:251` |
| Destroço | `GenericDebris`, some ao repousar | **~2 s** depois de cair | [INI]`Object/System.ini:872` |
| Ruína civil | `KeepObjectDie` ("IWantRubble"): achata pra `StructureRubbleHeight` e vira `CELL_RUBBLE` | **a partida toda**; **bloqueia tanque**, a infantaria e a moto passam | [EA]`ActiveBody.cpp:214-217`, `AIPathfind.cpp:1457`; [INI]`Object/CivilianBuilding.ini` |
| Prédio de facção | `DestroyDie`: some inteiro, com destroço e efeito | nada fica (fora o buraco da GLA) | [EA]`DestroyDie.cpp` ~l.63 |
| Caixa de sucata (GLA) | cai da carcaça se quem matou é da GLA; só a GLA vê | 30–35 s | [INI]`Crate.ini:181`; [EA]`CreateCrateDie.cpp` |
| Buraco da GLA | o prédio vira buraco que se reconstrói (um operário a cada 20 s) | até reconstruir | [INI]`Object/StealthGeneral.ini`; [EA]`RebuildHoleBehavior.cpp` |

**O que ensina:**
- No fim de uma partida do ZH, a luta só se lê pelos 500 queimados e pelas ruínas civis. A carcaça e o destroço são
  espetáculo de segundos.
- O nosso jogo já guarda mais que o ZH (as crateras, o avental e a carcaça física ficam), mas mostra menos (o caderno
  do mapa inteiro escondido).
- **A regra do raio/4** impede o bombardeio no mesmo ponto de virar borrão: é a irmã da P5.
- **A ruína civil do ZH muda o caminho** (tanque não passa, infantaria passa). A ruína que vale pro jogo mora na
  lógica, não no desenho.
- A nossa esteira de 5 min veio do padrão do código (300 s); o ZH que se joga usa 60 s no Alta.

### 2.2 Company of Heroes 1, 2 e 3 (Relic, Essence)

- **CoH1:**
  - "as crateras deformam o chão e viram cobertura pra infantaria" (análise de design de Brendon Chung,
    [Game Developer](https://gamedeveloper.com/design/on-company-of-heroes));
  - o prédio é desgastado aos pedaços até ruir, e a barricada e a ponte quebradas abrem caminho;
  - a carcaça de veículo dá **cobertura pesada**, pode ser destruída por mais dano ou esmagada por um veículo mais
    pesado, e algumas facções a recuperam ([wiki](https://companyofheroes.fandom.com/wiki/Light_tanks)).
  - A opção "Physics" controla o lixo e os destroços ([bit-tech](https://www.bit-tech.net/reviews/gaming/pc/Company_of_Heroes_Review/3/)).
  - **Sem fonte:** o limite de decais e o tempo de vida.
- **CoH2:**
  - pegadas e lagartas na neve são deformação 3D que **dura a partida inteira**, e a Relic admitiu o preço: o quadro
    cai quanto mais a partida dura ([SemiAccurate, conversa com a Relic](https://www.semiaccurate.com/2013/07/15/company-of-heroes-2-relic-performance/));
  - o gelo racha sob fogo e peso;
  - a cratera é cobertura leve ([Prima](https://primagames.com/eguides/company-of-heroes-2-eguide/essentials));
  - no editor, a neve é pintada em camadas (sem neve, só lagarta, lagarta e pegada, neve funda que deixa lento) com
    a borda clara pra se ler a rota ([guia do WorldBuilder](https://www.coh2.org/guides/16637/worldbuilder-totw-11-tactical-snow)).
- **CoH3:**
  - o diário de destruição: marcas de flak, furo de calibre grande, prédio que desaba e vira cobertura nova,
    destroço 3D com física ([SEGA](https://sega.prezly.com/company-of-heroes-3-destruction-dev-diary));
  - o "bombardeio antes da partida" deixa partes do mapa já destruídas ([GameSpot](https://gamespot.com/articles/company-of-heroes-3-video-details-new-ways-to-blow-up-everything-on-the-map/1100-6504674/),
    pelo resumo da busca: a página bloqueou).
- **O que ensina:**
  - no CoH a marca é **regra de jogo** (cratera = cobertura leve, carcaça = pesada, ruína = cobertura nova). Pra nós
    isso mora no SimCore ou não existe;
  - **marca persistente sem orçamento fixo derruba o quadro com o tempo** (CoH2). O caderno de custo fixo que o
    jogo já usa (`RTSGroundMemory.h:11-13`, que cita justamente o CoH2) é a resposta certa, e o `GroundBattle`
    segue a mesma regra.

### 2.3 Supreme Commander: Forged Alliance (FAF), o melhor exemplo de carcaça

Fonte: [FAForever/fa](https://github.com/FAForever/fa).

- **A carcaça:** `lua/sim/Unit.lua`, `lua/wreckage.lua`.
  - Tem vida (~0,9 da unidade) e **não tem cronômetro**: só sai quando é destruída ou recolhida.
  - Guarda a massa (~81% do custo no T1, menos nos tiers altos) e o engenheiro a recolhe.
  - Com dano demais (overkill acima de 1×), **não sobra carcaça**.
  - O projétil passa por ela.
- **A malha da carcaça sai sozinha** (`lua/system/Blueprints.lua`, `ExtractWreckageBlueprint`): a mesma malha da
  unidade com o shader de carcaça e um ruído. O custo de arte por unidade é zero.
- **O queimado** (`lua/defaultexplosions.lua`): a morte da unidade deixa 200–350 s; o genérico, 60–120 s. **Some.**
- **O que ensina:** no FA, o mapa de onde foi a luta são as **carcaças**, que ficam até alguém tirar. A carcaça como
  recurso cria jogo (a briga pra recolher depois da luta). O "dano demais não deixa carcaça" é uma regra simples pra
  arma pesada.

### 2.4 Beyond All Reason / Spring (o ouro técnico)

Fonte: [RecoilEngine](https://github.com/beyond-all-reason/RecoilEngine) (`GroundDecalHandler.cpp`) e a unidade
[armstump.lua](https://raw.githubusercontent.com/beyond-all-reason/Beyond-All-Reason/master/units/ArmVehicles/armstump.lua).

- **A cicatriz:** vive pelo dano, de 0,5 s a **3 min**, linear. O raio é limitado a dano × 0,25, e a brasa dura
  ~2 s.
- **O motor antigo** ([spring 104](https://github.com/spring/spring/blob/104.0/rts/Rendering/Env/Decals/GroundDecalHandler.cpp))
  tinha dois limites:
  - o teto `MAX_SCAR_COUNT 4096`;
  - o **orçamento de sobreposição**: a cicatriz coberta por novas além de N vezes a própria área é removida.
- **A lagarta:** dura 3 × a força em segundos (o tanque comum, ~18 s). As fitas quase retas se fundem numa só.
- **A carcaça em dois estágios** (o tanque de 225 de metal):

| Estágio | Metal | Vida | Bloqueia |
|---|---|---|---|
| carcaça | 152 (~67%) | 1056 | sim |
| monte, quando a carcaça é destruída | 61 | 528 | não |

- **O que ensina:**
  - **a carcaça em dois estágios** (a que bloqueia, depois o monte que não bloqueia) é exatamente a P8.2;
  - **o orçamento de sobreposição** é o caminho da P5: no lugar muito bombardeado, troca de estado em vez de empilhar
    círculos.

### 2.5 Battlefield (DICE, Frostbite)

- **BF3, GDC 2012** ([PDF](https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/gdc12-terrain-in-battlefield3.pdf)):
  - a cratera é escrita na **textura virtual procedural** do terreno, num canal "Destruction";
  - um **mapa de profundidade de destruição** limita a cratera perto dos modelos estáticos;
  - o mapa de material físico define a profundidade e a largura por chão;
  - os dois são compactados.
- **Frostbite 2, SIGGRAPH 2010** ([Kihl](https://www.advances.realtimerendering.com/s2010/Kihl-Destruction%20in%20Frostbite(SIGGRAPH%202010%20Advanced%20RealTime%20Rendering%20Course).pdf)):
  o dano na parede é um **decal diferido** guiado por um campo de distância em volume (~2 m por texel).
- **BF1:** a cratera quis ser cobertura de verdade, buracos pra atravessar o campo aberto (Gustavsson,
  [Stevivor](https://stevivor.com/features/interviews/dices-lars-gustavsson-levolution-is-the-most-depreciated-word-in-the-industry/)),
  e a profundidade muda pelo chão ([mein-mmo](https://mein-mmo.de/battlefield-1-massive-zerstoerung/)).
- **O que ensina:**
  - a cratera pelo tipo de chão, e **nunca cavando debaixo do prédio nem da estrada** (a máscara de profundidade). A
    MF já separa a areia do asfalto (`Surf`), mas o carimbo não tem a máscara;
  - a fuligem na parede pode ser "decal mascarado" ou, mais barato no nosso caso, o material da parede lendo o
    caderno do chão (P7).

### 2.6 Os outros

- **Men of War / Assault Squad:**
  - a cratera recém-aberta serve de abrigo ([Giant Bomb](https://www.giantbomb.com/games/3030-22156/));
  - o tanque abandonado é consertado e volta à luta ([Wikipedia](https://en.wikipedia.org/wiki/Men_of_War_(video_game))).
  - Ensina que a carcaça recuperável é um estado entre vivo e destruído.
- **Steel Division / Wargame / WARNO (Eugen):**
  - existem mods de "corpos e carcaças permanentes" ([Steam](https://steamcommunity.com/sharedfiles/filedetails/changelog/2377969920)),
    sinal de que o jogo base limpa (**impressão, não fato**);
  - um mod do WARNO faz a carcaça tapar a visão e tapar bem menos quando o fogo apaga ([changelog](https://steamcommunity.com/sharedfiles/filedetails/changelog/3039144581?p=7)).
- **Total War:** o "corpse timer" nas opções avançadas, 30 s no padrão segundo o fórum, e o `hide_dead_corpses` por
  preset ([Steam](https://steamcommunity.com/app/594570/discussions/0/1640927858842361495)). Os restos com tempo por
  preset são o padrão do gênero.
- **Iron Harvest:** setores inteiros marcados de cratera e prédios em escombro ([PC Gamer](https://www.pcgamer.com/iron-harvest-review)).
- **Gates of Hell:** os mapas já nascem cheios de cratera; o rastro na neve mostra atividade recente ([GameStar](https://www.gamestar.de/artikel/gates-of-hell-gameplay-preview,3370349.html)).
- **Age of Empires IV:** a ruína fica e a unidade morta some em ~30 s (relato de jogador, [fórum](https://forums.ageofempires.com/t/why-dont-the-destroyed-buildings-disappear/174630)).
- **MudRunner:** a lama em blocos de 16 m com texturas pequenas por bloco; a lagarta é malha, não decal ([Game Developer](https://www.gamedeveloper.com/programming/mud-and-water-of-spintires-mudrunner)).
- **Far Cry 2:** o fogo na vegetação é limitado de propósito, porque espalhar demais "não é divertido" ([Game Developer](https://gamedeveloper.com/game-platforms/interview-how-i-far-cry-2-i-s-fire-fuels-spreads)).
  O queimado é história, não um incêndio que toma o mapa.
- **Sem fonte:** a mancha de óleo em volta da carcaça e a fuligem de explosão na parede **em jogo**. Nenhum dos
  jogos pesquisados documenta isso: é diferencial nosso.

### 2.7 A realidade (pra as marcas novas não serem chute)

- **O caminho batido:**
  - Prose 1985, *Persisting effects of armored military maneuvers on some soils of the Mojave Desert*
    ([USGS](https://pubs.usgs.gov/publication/70013037)): 40 anos depois dos treinos do Patton, o chão de **uma
    passada** de tanque M3 seguia ~50% mais duro no penetrômetro; as estradas dos acampamentos não deixavam entrar
    5–10 cm; a mexida vai até 25 cm de fundo e 50 cm pro lado da lagarta (o "halo" que o jogo já usa, `HaloCmNow`).
  - Belnap e Warren 2002, *Patton's tracks in the Mojave Desert: an ecological legacy* ([USGS](https://www.usgs.gov/publications/pattons-tracks-mojave-desert-usa-ecological-legacy)):
    55 anos depois, as trilhas **ainda se veem**. Na trilha, o grão fino e solto troca a crosta e o pedregulho de
    fora.
  - Prose e Wilshire 2000 ([USGS OFR 00-512](https://pubs.usgs.gov/publication/ofr00512)): na trilha de uma
    passada, o pedregulho da superfície fica 15–50% menor.
- **O combustível:** no Kuwait de 1991, o óleo e a fuligem se misturaram à areia e viraram "tarcrete", uma crosta
  dura de ~1 cm sobre ~5% do país, que segue lá décadas depois ([Nottingham](https://eprints.nottingham.ac.uk/33258),
  [NASA](https://visibleearth.nasa.gov/images/78594/kuwait-oil-fires/78595l)). A mancha de combustível fica, escura.
  A crosta seca é dura e quebradiça (a tese de Nottingham); daí a nossa leitura de que o brilho de molhado é só do
  fresco (inferência, não da fonte).
- **O lugar muito bombardeado:** o LiDAR de Verdun mostra o campo de 1916 como um tapete contínuo de crateras e
  trincheiras ("polemoforms") sob a floresta (De Matos-Machado et al., [HAL](https://hal.archives-ouvertes.fr/hal-01231549));
  a Bélgica mediu a densidade de buracos por foto aérea e a classificou em graus de destruição ([RUC](https://forskning.ruc.dk/en/publications/a-new-evaluation-approach-of-world-war-ones-devastated-front-zone/)).
  A partir de certa densidade, **o chão deixa de ter crateras e passa a ser cratera**.
- **A fuligem na parede:** o padrão "V" da investigação de incêndio (o manual é a NFPA 921). O fogo encostado
  deposita um cone invertido de fuligem; quanto mais longe da parede, mais alta e redonda a base do V
  ([Fire Engineering](https://emberly.fireengineering.com/?p=318408)). O vento deforma o V.

### 2.8 O custo na Unreal 5

- **Decal diferido** ([Epic](https://dev.epicgames.com/documentation/en-us/unreal-engine/decal-actors-in-unreal-engine)):
  o custo é o shader × a área na tela e **soma onde os decais se sobrepõem**. Com câmera de cima e centenas de
  marcas, vira sobreposição na tela inteira. O caderno de custo fixo (+0,2 ms na mediana, medido em 12/10,
  `BASE_JOGAVEL.md`) é o caminho do BF3, do Spring e do MudRunner.
- **RVT:** a Epic recomenda pra marca no Landscape, mas **foi reprovado aqui em 14/10** (borrado, AO perdido; memória
  "Chão em RVT reprovado"). Não repropor.
- **Destroços:** ISM com dados por instância; o jogo já junta por célula de 64 m e por quadro, pra não invalidar a
  sombra virtual a cada peça (`RTSDebris.cpp:303-309`).

### 2.9 A leitura de cima

- A Riot separa o mapa em camadas: o **fundo** fica atrás de tudo e "emoldura" os personagens, com faixas de valor e
  saturação por camada ([resumo](https://www.surrenderat20.net/2014/11/red-post-collection-defining-rifts.html?m=1));
  e o secundário tem faixa de valor menor e forma simples ([guia de VFX](https://nexus.leagueoflegends.com/en-us/2017/10/dev-leagues-vfx-style-guide/)).
- O StarCraft II: silhueta forte e pouco detalhe miúdo pra vista de cima em três-quartos ([GameSpot](https://www.gamespot.com/articles/blizzard-talks-starcraft-ii-art-design/1100-6171176/),
  pelo resumo da busca).
- **A regra pra nós:** a marca é da camada do chão. Escurece e tira saturação, mas **nunca chega ao valor do
  tanque**: queimado escuro embaixo de carcaça escura esconde a unidade, e isso é defeito.

### 2.10 A tabela do que ensina

| Camada | As referências | O nosso alvo |
|---|---|---|
| Lagarta | ZH 5–60 s (o padrão do código, 300 s); Spring ~18 s; CoH2 a partida toda, e o quadro cai | ~5 min da esteira (fica) + **o caminho batido que acumula** |
| Queimado | ZH 500 sem desbotar e sem duplicata; FAF 1–6 min; Spring até 3 min | a partida toda, custo fixo; troca de estado no lugar muito bombardeado |
| Carcaça | ZH 10–20 s, nunca bloqueia; FAF e BAR até tirar, com vida, recurso e 2 estágios; CoH cobertura pesada | a partida toda, física (já é); destruível e limpável pela decisão do Felipe |
| Ruína | ZH civil a partida toda, bloqueia tanque; facção some; CoH3 vira cobertura | tem de valer na simulação (P8.1) |
| Destroço | ZH ~2 s; o nosso, a partida toda | a partida toda, com teto por preset que afina o mais velho |

---

## Parte 3: o que faz a marca ler de cima

A câmera do jogo (`RTSCameraPawn.h:87-100`): 67–118 m do foco, inclinação 28–44° (37,5° no padrão), campo de visão
46–58°, TSR a 67%. O pixel no chão tem 7–12 cm no centro. Pelo campo de visão, a borda de cima da tela fica a
~55–65 m além do foco: a janela do meio (384 m, 60 m à frente) cobre a vista normal. No minimapa, ~4,6 m por pixel.

As regras que saem daí (as fontes da Parte 2 e as medidas já feitas no projeto):

- **O tamanho.** Um detalhe precisa de 30–50 cm pra existir sem cintilar (o jogo já corta pedra abaixo de 9 cm,
  `RTSDebris.cpp:175-178`). Uma marca precisa de 2–3 m pra ser lida como "uma coisa" (a cratera A tem ~1,7 m de
  bacia, `RTSGroundMemory.cpp:1134-1136`, e lê pelo halo escuro em volta, bem maior que ela). Um *lugar* de luta precisa de 10 m ou mais, e no minimapa de 20–40 m.
  **A história se conta pela densidade, não pela marca solta.**
- **O valor, não a cor.** Na areia, cor sem diferença de claro e escuro some. Três tons fazem a leitura:
  - o escuro: carvão e combustível;
  - o meio: terra revirada;
  - o claro: o compactado, a areia jogada, o pó de concreto.
  A cratera v2 já vive disso: o miolo a 0,09 de brilho contra 0,46 do chão (`look_mf.py:121-124`).
- **A unidade vem primeiro.** Área grande escura não pode chegar ao tom do tanque queimado nem do tanque vivo: o
  campo revirado e o queimado largo param num albedo de ~0,08–0,10 (o casco carbonizado é 0,07, `RTSUnitActor.cpp:802`).
  O preto chapado fica só no miolo pequeno. É a regra do Felipe: efeito que polui a leitura da batalha é defeito.
- **A borda orgânica.** Círculo e carimbo repetido leem como textura. O atlas já sorteia variante, espelho, giro e
  escala (`RTSGroundMemory.cpp:1128-1144`).
- **O contraluz.** O sol dos game shots vem de trás, em três-quartos (memória "Sol dos game shots"). Nessa luz o que
  é liso *brilha* na câmera: o combustível e o caminho compactado leem pelo reflexo, não pela cor; a sombra fica do
  lado da câmera na parede do sulco e da bacia.
- **A idade.** Fresco e velho diferentes contam a ordem da luta: a brasa (30 s), a terra revolvida fresca mais
  escura (`look_mf.py:150-151`) e, proposto abaixo, a carcaça que pega poeira com o tempo.

---

## Parte 4: a proposta, por prioridade

O critério é **o quanto conta a história ÷ o custo**. Os custos de placa são **(estimativa)** até medir na cena
`trilha_arcos_groundv3dune` ou num cenário de batalha longo. A memória de vídeo sai da conta (pixels × bytes).

### A peça nova: o caderno de acúmulo (`GroundBattle`)

É uma textura do **mapa inteiro**, nos mesmos limites do caderno grosso (fixos a partida toda: o material lê sem
parâmetro que anda): 2048² RGBA8, 50 cm por pixel, **16 MB**. Só o G desbota (o brilho do combustível fresco); o
resto fica a partida toda. O jogo escreve; o Look, o WorldForge e o HUD leem.

| Canal | O que guarda | Quem escreve | Regra |
|---|---|---|---|
| R | **tráfego** (o caminho batido) | cada pedaço de trilha (`TrackUnit`) | soma ~1/32 por passada, borrado na largura da faixa (~1,5× a lagarta); satura em ~32 passadas; não desbota |
| G | **combustível fresco** (o molhado que brilha) | morte de veículo (`AddWreck`) | o desenho do avental, mais apertado; o máximo; desbota em ~5 min (× 0,985 a cada 2 s, como a esteira). A mancha escura que fica já existe: é o G do avental nos cadernos de hoje, que não desbota |
| B | **queimado largo** (o que a vegetação e a parede leem) | cratera, avental, prédio que cai | disco macio de ~1,5× a marca; o máximo |
| A | **densidade de crateras** | cada cratera | soma ~1/6 por cratera na pegada dela; satura em ~6 sobrepostas |

A escrita: o R e o A são soma (o Canvas aditivo, como o `DrawTracks`, saturando em 1); o G e o B são máximo, que o
Canvas não tem: vão pelo compute das marcas (`RTSGroundRelief::StampMarks`, que já faz o máximo nos cadernos de hoje).
Tudo sai da posição do evento da simulação: é só desenho, não entra no hash, e o que dois jogadores viram sai igual
nos dois PCs. O desbotar dos cadernos atuais (`:2002-2008`)
não toca nele. As trilhas do Patton no Mojave seguem visíveis 55 anos
depois (Belnap e Warren 2002), e o óleo do Kuwait virou crosta de décadas ("tarcrete"): ficar a partida toda é o
certo físico e o certo de jogo.

---

### P1. A batalha no minimapa e na tela do fim (jogo, HUD) — o maior retorno

- **O quê:** o minimapa passa a mostrar, por cima da foto, o escuro e o claro do caderno grosso e o tráfego e as
  crateras do `GroundBattle`. Num mapa de 1 km, o tiroteio de 30 m vira uma mancha de 6–7 px, a estrada batida uma
  linha de 1–2 px clara: é exatamente "no fim, olhando o mapa, ver onde foram as lutas".
- **A tela do fim** (o placar do ZH): o mesmo mapa grande, mais um mapa de calor das mortes, sem névoa. Os eventos
  da simulação chegam a todos os PCs inteiros, inclusive os da névoa, então o desenho monta essa grade sozinho, sem
  mexer na simulação (uma grade de 8 m com mortes, tiros e passagens). O replay já existe (`SaveReplay`) pra reabrir
  depois.
- **Como:** um material de UI que lê as duas texturas e decodifica as rampas (o `MarkRamps`, `look_mf.py:100-104`),
  desenhado no `DrawMinimap` (`RTSHUD.cpp:924`) e no HUD novo (`RTSHud`).
- **O custo:** ~0 ms (um desenho de 222² px com 2 leituras); 0 MB novos além do `GroundBattle`; ~1 dia de sessão
  **(estimativa)**.
- **A névoa:** no minimapa só aparece onde o jogador viu (o caderno só recebe o que foi visto); na tela do fim, tudo.

### P2. Os furos da memória (jogo) — barato e sem isso nada fecha

1. **A cratera e o prédio na névoa guardados pra depois:** uma lista `HiddenScars` igual à `HiddenMarks`
   (`RTSMatchSubsystem.cpp:2203-2207`), soltada pelo `RevealHiddenMarks`. O custo: ~0. É o que está combinado em
   `BASE_JOGAVEL.md` ("Marcas ... feitas na névoa só aparecem quando a área é revista").
2. **O grosso como memória longa do chão:** o jogo passa `GroundCoarse`, `GroundCraterCoarse` e `CoarseBounds` pro
   Landscape (o `PushToLandscape` já faz isso com o detalhe e o meio, `:786-791`). A MF usa o grosso **onde o meio
   não tem nada** (longe, na câmera baixa e nas fotos) e toma o **máximo** do escuro e do claro entre o meio e o
   grosso, pra cicatriz que saiu do anel não sumir (perde nitidez, nunca some). O custo: +2 leituras no chão
   (+0,03–0,08 ms **estimativa**), 0 MB novos. A MF é do Look (P4); a parte do jogo é passar a textura.
3. **A carcaça-fantasma:** quando a fila de 256 tira a mais velha, a simulação solta um evento novo
   (`WreckRemoved`) e o desenho a "assenta": afunda ~40 cm, vira chapa queimada sem colisão, e o avental fica. O
   mesmo vale pra carcaça destruída (P8). Simulação nova = versão nova, `simtest` e hash nos 3 builds.
4. **Os destroços na luta certa:** o teto de 1.500 passa a ser por preset (Baixa 800, Média 1.500, Alta 3.000,
   Ultra 5.000 **estimativa**), e cheio **afina a mais velha** em vez de negar a nova (tira uma lasca de cada morte
   antiga). O custo: instâncias (ISM por 64 m, sombra só das grandes): +0,05–0,15 ms no Ultra **(estimativa)**.
5. **A regra do raio/4 do ZH** (2.1) na mancha fraca do acerto (`RTSMatchSubsystem.cpp:2022`): cada tiro que acerta
   ocupa uma vaga no anel de 16.384. A mancha nova a menos de raio/4 de uma igual reforça a velha em vez de ocupar
   outra vaga. O anel dura muito mais numa partida longa. O custo: ~0.
6. **(Pra registrar, não mexer agora)** O laço carcaça × carcaça é O(N²) (`World.cpp:1870-1904`): com 256 são
   ~32 mil pares por passo. Se o teto subir, usar a grade de vizinhança (`BuildBins`) que a simulação já tem.

### P3. O caminho batido (jogo + Look + WorldForge) — o "uau" mais barato

- **A realidade:** a lagarta compacta o chão. No Mojave, uma passada de tanque M3 deixou o solo ~50% mais duro
  40 anos depois, e a estrada do acampamento não deixa o penetrômetro passar de 5–10 cm (Prose 1985). Na trilha, a
  superfície troca a crosta e o pedregulho por grão fino e solto (Belnap e Warren 2002; Prose e Wilshire 2000). Na
  areia, a trilha muito usada vira pó fino, mais claro e mais liso, sem a ondulação do vento.
- **O jogo:** cada pedaço de trilha também soma no R do `GroundBattle` (o mesmo desenho do `DrawTracks`, aditivo,
  largura de ~1,5× a lagarta). Custo de escrita ~0; 16 MB do caderno novo.
- **O Look (MF `code_v3()`):** `traffic = smoothstep(0.15, 0.7, R)`:
  - a cor um pouco mais clara e menos saturada (+8–12% no brilho, o pó fino);
  - a rugosidade mais baixa (−0,1: o liso que pega o contraluz);
  - a ondulação desligada (a saída `Mask` / `RippleMask`);
  - a bordinha amaciada.
  Os números são o ponto de partida do estudo do Look, não constantes fechadas.
- **O WorldForge:** o capim e a moita leem o R na raiz: deitam e encolhem (WPO) e ficam cor de palha onde o tráfego
  é alto. Árvore não some (regra do Felipe).
- **O que ele vê:** no fim, as rotas dos ataques aparecem como estradas claras que ninguém desenhou, ligando as bases
  aos lugares das lutas.

### P4. O chão que conta (Look: a MF `code_v3()` em `Game/Tools/look_mf.py`)

Uma leitura a mais do `GroundBattle` (e o grosso, de P2.2) dentro do CODE, depois da cratera (`:116-149`):

- **Combustível fresco (G):** por cima do escuro do avental, o molhado: mais escuro e bem mais liso (rugosidade
  ~0,3–0,45 contra ~0,9 do carvão). O queimado é fosco e o combustível fresco brilha no contraluz. Seco (o G
  desbotou), fica a crosta escura e fosca, como o "tarcrete" do Kuwait (2.7). Lê como "aqui morreu um veículo" mesmo
  com a carcaça empurrada pra longe, porque o avental fica onde ela morreu (a física da carcaça já faz isso de graça).
- **Campo revirado (A):** com 3 ou mais crateras sobrepostas, a areia vira terra revirada:
  - tom médio-escuro, que nunca passa do albedo ~0,1;
  - a normal grumosa (a Farm Soil que o `DISP_CODE` já usa);
  - sem ondulação.
  As bacias soltas viram um campo. É a "zona vermelha" de Verdun em escala de RTS (De Matos-Machado et al., LiDAR de
  Verdun).
- **Queimado largo (B):** no chão, o anel de cinza clara na borda do queimado (a cinza é cinza-clara, o carvão é
  preto): o terceiro tom que separa "queimou" de "explodiu".
- **O custo:** +1 leitura por pixel de chão. O CODE é inline no WorldForge (memória "Chão do jogo: MF inline"):
  mudar o CODE = avisar o WorldForge. O total da P2.2 com a P4 fica em +0,05–0,12 ms **(estimativa)**.

### P5. As crateras que se sobrepõem (jogo + Look)

- **Hoje:** a altura soma e trava em −90 cm e +40 cm (`GroundCrater.usf:158-159`; `RTSGroundMemory.cpp:1273`). O
  lugar muito bombardeado vira um **fundo chato a −90 cm** com as bordas velhas de pé dentro das bacias novas: lê
  como carimbos empilhados, não como chão revirado.
- **A restrição:** a composição tem de **não depender da ordem**, porque o caderno se redesenha da lista
  (`GroundCrater.usf:8`). "A bacia nova apaga a borda velha" depende da ordem, e as marcas do mesmo despacho
  correriam juntas na placa. Por isso a regra fica em duas partes, as duas sem ordem:
  - **no carimbo:** a soma continua, mas sem a trava dura (o RG16F guarda a soma crua);
  - **na leitura (MF e relevo):** o teto macio `−D·tanh(−h/D)` (D ≈ 60 cm, a medir), e a borda amortecida pela
    densidade: `borda × (1 − 0,7·A)`, com o A do `GroundBattle`. Onde caíram muitas, as bordas abaixam e o chão
    vira o campo revirado da P4, em vez do fundo chato.
- **O claro dos torrões:** no campo revirado (A alto), o claro (A dos cadernos) também é amortecido na MF, pra o lugar
  muito bombardeado ficar escuro-revirado e não branco de torrão.
- **A máscara de profundidade do BF3** (2.5): a bacia não cava debaixo do prédio, da laje da estrada nem encostada
  neles. O jogo já sabe a pegada dos prédios (a grade da simulação) e o WorldForge sabe as lajes; o carimbo multiplica
  a altura por essa máscara. No asfalto fica a placa quebrada e queimada que a MF já faz pelo `Surf`.
- **O custo:** ~0 (a mesma passada; duas contas no material).

### P6. A carcaça que conta a idade (Look + jogo)

- **O jogo:** um dado de primitiva novo na carcaça, a idade (0 → 1 em ~10 min). Os slots 6–9 já estão em uso
  (`RTSUnitActor.cpp:79-83`).
- **O Look** (o `M_UnitDust` do RTSLook): a carcaça nova é preta de fuligem; a velha pega **poeira da cor do chão**
  nas faces de cima (a normal pra cima, o que a câmera de RTS mais vê) e um pouco de ferrugem nas bordas.
- **O que ele vê:** no fim, as carcaças pretas mostram a última luta e as empoeiradas, a primeira. A ordem da
  batalha fica escrita sem número nenhum.
- **O custo:** 0 ms (um dado e uma conta no material); ~meio dia **(estimativa)**.

### P7. A vegetação queimada e a fuligem na parede (WorldForge + Look)

- **A vegetação** (o material das palmeiras, do capim e das moitas, que são instâncias em blocos de 64 m do
  WorldForge, `docs/WORLDFORGE.md:568`) lê o B do `GroundBattle` na posição da raiz:
  - o capim vira toco preto ou some encolhido;
  - a moita fica carvão;
  - a palmeira fica com o tronco preto e as folhas marrons e caídas (WPO).
  **Não sai nada do mapa:** árvore não some, e o tanque escondido nas árvores segue escondido (memória "Árvore não
  some").
- **Por que pela textura e não por instância:** escrever dado por instância invalida o cache da sombra virtual do
  bloco a cada explosão (o mesmo problema que o `RTSDebris` resolveu juntando por quadro, `RTSDebris.cpp:303-309`).
  A leitura da textura é de graça na escrita e custa uma leitura no vértice.
- **A parede:** o material das casas e dos prédios lê o B no XY do pixel da parede e escurece de baixo pra cima,
  sumindo em ~2–4 m de altura. O fogo encostado na parede deixa o "V" de fuligem (o padrão de investigação de
  incêndio, NFPA 921): o vértice do V no fogo, e quanto mais longe da parede, mais alta e redonda a base do V.
- **O custo:** +1 leitura no vértice da vegetação e +1 no pixel das paredes; +0,02–0,06 ms **(estimativa)**.
  O dono do material da vegetação e das casas é o WorldForge; a receita da cor é do Look.

### P8. O prédio em ruína e a carcaça destruível (jogo: simulação; Look e WorldForge: as peças)

São decisões de jogabilidade: **vão como pergunta pro Felipe**, com a recomendação.

1. **A ruína tem que valer na simulação** (hoje o tanque passa por dentro do prédio afundado 42%). O ZH faz duas
   coisas diferentes (2.1): o prédio de facção **some inteiro**, e o civil vira ruína baixa e permanente que **bloqueia
   o tanque** e deixa a infantaria passar. As opções:
   - **(a, recomendada pra prédio de facção)** o prédio desaba até um **monte baixo** (≤ 1 m, como o
     `StructureRubbleHeight` do ZH), e a pegada vira **entulho andável a ~50% da velocidade**. Ninguém atravessa
     parede em pé, e o lugar da base destruída fica marcado pra sempre;
   - **(b)** o monte fica bloqueado até o Dozer limpar;
   - **(c, a do ZH pra casa civil, quando ela puder cair)** a ruína bloqueia o tanque, e a infantaria (quando
     existir) passa.
   No desenho, o entulho derrama ~1,3× a pegada:
   - peças de concreto, viga e tijolo de 20–80 cm, o kit novo do WorldForge ou do Look, em ISM;
   - um halo de **pó de concreto cinza-claro** em volta (o claro A, com o tom de concreto): é o que mais lê de
     cima numa cidade bombardeada.
2. **A carcaça pode ser destruída:** é a carcaça em dois estágios do BAR (2.4) e a vida da carcaça do FAF (2.3).
   Ela ganha uma vida (~50% da do veículo). O tiro forçado (Ctrl) e a explosão grande (prédio que cai, arma pesada)
   a destroem, e ela vira **casco achatado** sem colisão, mais uma cratera. Isso abre a rua entupida. Do FAF vem
   também a regra "dano demais não deixa carcaça" pra arma muito pesada, no futuro.
3. **O Dozer limpa carcaça e entulho** (no FAF e no BAR, o engenheiro recolhe a carcaça como recurso; a GLA do ZH
   tira dinheiro da caixa de sucata). Isso mexe na economia: decisão dele, ligada ao `docs/ECONOMIA.md`.
4. **A carcaça como cobertura** (a do CoH) fica de fora: o nosso jogo não tem cobertura nem infantaria. Só registrar.
5. **O custo:** CPU de simulação ~0 (a carcaça é uma lista); a versão da simulação sobe; ~2–3 dias com teste e
   cenário **(estimativa)**. As peças do entulho em ISM: +0,05 ms **(estimativa)**.

### P9. A posição de tiro (jogo + Look) e as cápsulas (não)

- **As cápsulas, com franqueza: não leem.** O toco de cartucho de 120 mm tem ~15 cm, 1–2 px a 7–12 cm, e cintila;
  o jogo já corta pedra abaixo de 9 cm pelo mesmo motivo.
- **No lugar, o leque do sopro.** O tiro do canhão varre a areia num leque de 2–4 m à frente da boca. A cada
  `WeaponFired`, o jogo carimba um leque fraco no claro e no escuro, apontado pro tiro. Uns 3–5 tiros do mesmo
  lugar já marcam a **posição de tiro**: a Patriot e o tanque que segurou a linha ficam marcados. O brilho de metal
  espalhado, se o Look quiser, vai dentro desse leque, como pontinho no material, nunca malha.
- **O custo:** um carimbo por tiro, ~0.

### P10. A fumaça que fica (Fluxo e Arte)

- **A regra:** a fumaça mostra **onde a luta está agora**; a história fica no chão. Por isso a fumaça que fica é
  fina e alta, nunca um véu na altura das unidades (memória "Jogabilidade antes do realismo").
- **A proposta:** as N carcaças e ruínas mais novas (N por preset, ~4–8) seguem soltando um **fio de fumaça fina**
  por 2–3 min depois dos 30 s de fogo de hoje (`BlastFxDirector.cpp:116`), dobrado pelo vento do mapa, esmaecendo
  onde tapar unidade. O prédio queima mais que o veículo.
- **A coerência:** o fogo da poça de combustível (curto, baixo, sobre o avental) termina na mancha G do jogo, no
  mesmo lugar e no mesmo tom. A poeira do desabamento que assenta termina no halo de concreto da P8. Efeito e marca
  são uma coisa só (a regra "Coerência antes de medir").
- **O custo:** 0,1–0,3 ms por fio ativo na 1660 **(estimativa; o Fluxo mede)**; com orçamento fixo, o custo não
  cresce com a partida.
- **Pendente da v3:** o fogo da carcaça refeito (o reprovado de 13/10 em `docs/APROVADOS.md`).

---

## Parte 5: por dono, em ordem

| Ordem | Dono | Item | Custo (placa / memória / trabalho) |
|---|---|---|---|
| 1 | Jogo (HUD) | P1: a batalha no minimapa e na tela do fim | ~0 ms / 0 MB além do caderno / ~1 dia (est.) |
| 2 | Jogo | P2.1: crateras e prédios na névoa guardados | ~0 / ~0 / horas |
| 3 | Jogo | `GroundBattle` (o caderno de acúmulo) e a escrita dos 4 canais | escrita ~0 / 16 MB / ~1 dia (est.) |
| 4 | Jogo + Look | P2.2: o grosso no chão (longe e memória longa) | +0,03–0,08 ms (est.) / 0 MB |
| 5 | Look | P4: tráfego, combustível, campo revirado e cinza na MF `code_v3()` | +0,02–0,05 ms (est.) |
| 6 | Look + jogo | P6: a carcaça que envelhece (poeira e ferrugem) | 0 ms / ~meio dia (est.) |
| 7 | WorldForge | P3 e P7: o capim que deita no caminho; a vegetação queimada; a fuligem na parede | +0,02–0,06 ms (est.) |
| 8 | Jogo | P5: a regra das crateras sobrepostas no carimbo | ~0 |
| 9 | Jogo | P2.3 a P2.5: a carcaça-fantasma; os destroços por preset que afinam a mais velha; o raio/4 do ZH na mancha do acerto | +0,05–0,15 ms no Ultra (est.) |
| 10 | Jogo (simulação) | P8: ruína que vale na simulação; carcaça destruível; Dozer que limpa | **decisão do Felipe**; ~2–3 dias (est.) |
| 11 | Jogo + Look | P9: o leque da posição de tiro | ~0 |
| 12 | Fluxo e Arte | P10: o fio de fumaça das mais novas; a poça que queima; o fogo da carcaça da v3 | 0,1–0,3 ms por fio (est.), com teto |

**Por dono:**

- **O jogo:**
  - **na simulação:** o evento `WreckRemoved` (P2.3) e, se ele aprovar, a ruína andável e lenta, a carcaça
    destruível e o Dozer que limpa (P8). Tudo em ponto fixo, com versão nova e `simtest`;
  - **nos cadernos:** o `GroundBattle` e os 4 canais, a `HiddenScars`, o grosso passado ao chão, o raio/4 na mancha
    do acerto, a regra das crateras sobrepostas com a máscara de profundidade, o leque do tiro, o dado de idade da
    carcaça;
  - **no HUD:** o minimapa e a tela do fim.
- **O Look:**
  - a MF `code_v3()`: o tráfego, o combustível, o campo revirado, a cinza, o grosso como fundo;
  - a carcaça que envelhece (`M_UnitDust`);
  - a receita de cor da vegetação queimada e da fuligem;
  - a prova no jogo, na câmera do jogo, ao lado do alvo.
- **O WorldForge:**
  - os materiais da vegetação e das casas lendo o `GroundBattle` (os nomes e os limites que o jogo passa, como já
    é com `DetailBounds`);
  - o kit de entulho das casas;
  - o tipo de chão (`surface.png`) pra marca mudar por chão (o `Surf` da MF já separa areia e asfalto).
- **O Fluxo e a Arte:**
  - o fio de fumaça das mais novas com orçamento;
  - o fogo da poça de combustível que termina na mancha;
  - a poeira do desabamento que termina no halo de concreto;
  - o fogo da carcaça da v3.

**Como provar** (o método combinado):

- Um cenário longo e fixo (`Scenarios.json`), de uns 15 min de batalha com semente fixa, rodado em A/B.
- A foto do minimapa e da vista de cima no fim, lado a lado com a mesma foto sem as marcas novas.
- O vídeo em movimento.
- A medida do quadro na mediana e no p95 contra a cena de hoje.
- Nada se aprova na maquete sem a foto do jogo ao lado.
