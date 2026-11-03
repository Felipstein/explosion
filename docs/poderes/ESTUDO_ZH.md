# Estudo: os poderes do general no Zero Hour (e nos mods)

Levantamento de 03/11/2026 para desenhar os poderes do general do nosso RTS.

**Convenções**
- **INI** = arquivos originais do ZH 1.04 em [GeneralsGamePatch/Patch104pZH/GameFilesOriginalZH/Data/INI](https://github.com/TheSuperHackers/GeneralsGamePatch/tree/main/Patch104pZH/GameFilesOriginalZH/Data/INI). Citação = `arquivo › bloco`.
- **Código** = `GeneralsMD/Code/GameEngine` do motor liberado (`C:\Users\luisf\newcnc\GeneralsGameCode`). Citação = `arquivo:linha`.
- **Textos do jogo** = `Data/English/generals.csf` do mesmo repositório (decodificado).
- Distância: 1 unidade do ZH = 0,2633 m. Tempo: o INI usa ms.
- **n/c** = não confirmado. **estimativa** = conta minha.

---

## Resumo (o que o ZH faz bem e mal)

1. **Bem:** a patente só vem de matar. A XP é o valor do que morreu, e prédio vale mais que tropa. O poder premia quem ataca.
2. **Bem:** são 3 fileiras (1, 3 e 5 estrelas) e 7 pontos no jogo todo. A escolha é visível e não volta atrás: cada partida vira uma build, e o rival lê o seu estilo.
3. **Bem:** quem compra já recebe o poder pronto. O relógio é do jogador, não do prédio: perder o Command Center não zera a recarga.
4. **Bem:** os níveis I/II/III dobram o efeito (Ambush 4/8/16, Cash Hack 1000/2000/4000, Bounty 5/10/20%). A progressão se lê de longe.
5. **Bem:** a maioria dos ataques chega de avião, e o avião pode ser derrubado (A-10 com 600 de vida, cargueiros e bombardeiros com 1000). O defensor tem resposta.
6. **Mal:** tudo custa 1 ponto, então o forte e o fraco empatam. Emergency Repair e Frenzy quase ninguém pega. Pathfinder, A-10, Artillery Barrage e Cash Bounty são obrigatórios. O patch 1.06 (H2 Clan) criou custos de 1 a 4 por isso.
7. **Mal:** é bola de neve. Quem ganha a primeira troca sobe primeiro e ganha poder para ganhar a próxima. Contra e RotR tiveram de remendar isso.
8. **Mal:** há poderes sem resposta e quase sem aviso. A Artillery Barrage vem de um canhão fora do mapa que não pode ser atacado. Os Ambush do Demo e do Toxin apagam exércitos. O Carpet Bomb do Air Force funciona na patente 1. O inimigo só recebe aviso do GPS Scrambler e do Sneak Attack.
9. **Mal:** muito resto e muito bug nos dados. Há patentes 6 a 8 fantasmas, poderes cortados ainda no INI (Nuke Drop, Overlord/Gattling Training), tooltips com tempo errado e o Cash Bounty que ativa só com o andaime do CC.
10. **Os mods apontam a saída:** no Contra, a patente libera tecnologia em camadas (3 = meio do jogo, 5 = superarma). No RotR, o ponto de 1 estrela escolhe um ramo da árvore. Os dois ainda lutam contra os poderes obrigatórios.

---

## 1. O sistema (regras do motor, conferidas no código)

### 1.1 Escada de patentes

| Patente | XP acumulada | Pontos ganhos | Pontos no total | Nome no jogo | Fonte |
|---|---|---|---|---|---|
| 1 | 0 | 1 | 1 | 1 Star General | `Rank.ini › Rank 1` |
| 2 | 800 | 1 | 2 | 2 Star General | `Rank.ini › Rank 2` |
| 3 | 1500 | 1 | 3 | 3 Star General | `Rank.ini › Rank 3` |
| 4 | 2500 | 1 | 4 | 4 Star General | `Rank.ini › Rank 4` |
| 5 | 5000 | 3 | **7** | 5 Star General | `Rank.ini › Rank 5` |

- **A partida começa na patente 1 com 1 ponto.** O `resetRank()` soma os pontos intrínsecos do PlayerTemplate (nenhum define, então 0) com o ponto da patente 1 (`Player.cpp:2700`).
- **Só há 5 patentes.** Os textos `SCIENCE:Rank6..8` (Major General, Lt. General, General) e as sciences `SCIENCE_Rank6..8` são restos de um desenho com 8 patentes (`Science.ini`, `generals.csf`). Por isso a 3ª fileira se chama `PurchaseScienceCommandSetRank8`, mas abre na patente 5 (`PlayerTemplate.ini`).
- **O ponto não expira.** Você pode guardar e gastar depois. Comprado não volta.
- **Perder patente zera tudo** (`setRankLevel`, `Player.cpp:2734`). Na prática só um script de mapa faz isso.
- **Teto:** o mapa pode limitar a patente por script (`doMapSetRankLevelLimit`). O padrão é 1000 (`GameLogic.cpp:273`).
- **Ritmo:** chegar na patente 5 exige 5000 XP, o equivalente a 50 tanques de 100 XP (estimativa).

### 1.2 De onde vem a XP do jogador

**Fonte principal: matar.**
- O motor soma a XP no `scoreTheKill`, a cada morte (`Object.cpp:2989`, que chama `addSkillPointsForKill`, `Player.cpp:2525`). **Dano sozinho não conta.**
- O valor é o `SkillPointValue` do morto no nível de veterania dele. Nenhum INI do ZH define `SkillPointValue`, então o motor usa o `ExperienceValue` (`ThingTemplate.cpp:1409`).
- **Matar veterano rende mais.** O `ExperienceValue` tem 4 colunas: regular, veterano, elite e herói.

**O que não rende XP**
- Matar aliado ou unidade própria (`Object.cpp:3015–3024`).
- Matar civil ou neutro (`isPlayableSide`, `Object.cpp:2997`).
- Matar o que ainda está em construção (`Player.cpp:2531`).
- Matar objetos `IGNORED_IN_GUI` (`Object.cpp:3003`). Isso inclui os aviões dos poderes (A-10, B-52, cargueiros): derrubá-los não dá XP.
- Dozer e Worker não têm `ExperienceValue`, então valem 0.

**Outras fontes**
- Habilidades com `AwardXPForTriggering`, que também viram XP do jogador (`SpecialAbilityUpdate.cpp:1262–1281`):
  - captura de prédio: Ranger 15, Red Guard 4, Rebel 12, Black Lotus 20;
  - roubo de dinheiro da Black Lotus: 20.
  - Fonte: `AmericaInfantry.ini`, `ChinaInfantry.ini`, `GLAInfantry.ini`.
- Scripts de mapa ou campanha: dar pontos (`ScriptActions.cpp:6031`) e multiplicar a XP (`setSkillPointsModifier`, `ScriptActions.cpp:6472`).
- No single player, a XP da campanha é somada no início da missão (`GameLogic.cpp:2217`).
- Caixas (crates) não dão XP de general. Só existem os chamadores acima (busca no código).

**Quanto vale cada coisa (XP regular → herói)**

| Alvo | XP | Custo $ | Fonte |
|---|---|---|---|
| Red Guard (cada soldado) | 5 / 5 / 10 / 20 | 300 o par | `ChinaInfantry.ini` |
| Rebel | 15 / 15 / 30 / 40 | 150 | `GLAInfantry.ini` |
| Ranger, Missile Defender, Tank Hunter | 20 / 20 / 40 / 60 | 225–300 | `*Infantry.ini` |
| Humvee, Rocket Buggy, Quad, Dragon | 50 / 50 / 100 / 150 | 700–900 | `*Vehicle.ini` |
| Crusader, Paladin, Battlemaster, Marauder | 100 / 100 / 200 / 400 (Marauder 300) | 800–1100 | `*Vehicle.ini` |
| Overlord | 200 / 200 / 400 / 600 | 2000 | `ChinaVehicle.ini` |
| Heróis (Burton, Black Lotus, Jarmen Kell) | 50 a 150, com Black Lotus e Hacker até 400 | 625–1500 | `*Infantry.ini` |
| Bunker chinês | 20 | 400 | `FactionBuilding.ini` |
| Barracks, Power Plant, Tunnel | 100 | 500–1000 | `FactionBuilding.ini` |
| War Factory, Supply Center, CC, Stinger Site, Gattling Cannon, Patriot | 200 | 900–2500 | `FactionBuilding.ini` |
| Strategy Center / Palace | 250 / 300 | 2500 | `FactionBuilding.ini` |
| Superarma (Particle Cannon, Nuke, Scud Storm) | 400 | 5000 | `FactionBuilding.ini` |

Leitura: a patente 2 (800 XP) sai com 8 tanques, 4 War Factories ou 160 Red Guards (estimativa). **O XP não segue o preço.** Um Stinger Site de $900 vale o mesmo que uma War Factory de $2000.

### 1.3 Como se compra

- **Todo poder custa 1 ponto** (`SciencePurchasePointCost = 1`, `Science.ini`, todos os blocos compráveis). Custo 0 significa "não comprável" (comentário do próprio INI).
- **Pré-requisitos.** Cada science exige a patente mínima e, a partir do nível 2, o nível anterior. Os níveis 1 a 3 de A-10, Artillery, Ambush etc. exigem todos a patente 3. Na patente 3, com 3 pontos, dá para comprar o A-10 III de uma vez (`Science.ini`).
- **A tela de compra** mostra 3 fileiras por general, uma por patente (1, 3 e 5): `PurchaseScienceCommandSetRank1/3/8` (`PlayerTemplate.ini`, `CommandSet.ini`).
- **Comprar deixa o poder pronto na hora.** O comentário no código: "Dustin wants these special powers to start ready to fire" (`SpecialPowerModule.cpp:202–220`; `Player.cpp:2566`).
- **Subir de nível não mexe no relógio.** O poder segue o science do nível 1 (`RequiredScience = ..._1`), e o nível novo só troca o que o poder cria (`UpgradeOCL`) (`SpecialPower.ini`, `FactionBuilding.ini`).

### 1.4 Regras de uso

**Depende do Command Center?** Sim. Os módulos dos poderes ficam no CC (`FactionBuilding.ini › AmericaCommandCenter`, `ChinaCommandCenter`, `GLACommandCenter` e os CCs de cada general). Exceções:
- O **Carpet Bomb do Air Force General** fica no **Strategy Center** (`AirforceGeneral.ini › AirF_AmericaStrategyCenter`, botão 12 em `CommandSet.ini › AirF_AmericaStrategyCenterCommandSet`). No CC ele está comentado.
- O **Cash Bounty** é passivo. Uma vez ligado, fica no jogador (abaixo).
- **Unidades destravadas e "Training"** são sciences e não dependem do CC.

**O que acontece se o CC cai**
- Os botões somem, e não dá para disparar.
- O relógio continua correndo, porque é do jogador: `SharedSyncedTimer = Yes` em todos os poderes de general (`SpecialPower.ini`; `Player.cpp:3324–3396`).
- Ao reconstruir o CC, o poder volta com o tempo que sobrou.
- Vários CCs dividem o mesmo relógio, então não dá para disparar em dobro.

**Quando o relógio começa**
- Poder de general: não corre desde o começo da partida.
  - Na compra fica pronto na hora; depois de usar, recarrega o `ReloadTime` (`SpecialPowerModule.cpp:96–117`, `:389–425`).
  - Um CC novo não reinicia o relógio ("only startPowerRecharges when first scienced or when executed").
- Superarma (prédio): o relógio começa quando o prédio fica pronto.

**CC desligado** por EMP, hacker ou Leaflet não dispara (`isDisabled`, `SpecialPowerModule.cpp:676–750`, `OCLSpecialPower.cpp:151`).

**É público?**
- Nenhum poder de general tem relógio público (`PublicTimer = No`). Só as superarmas e poderes de script (Napalm, Crate Drop, Black Market Nuke) têm.
- O círculo de mira no chão só aparece para o dono (`OnlyVisibleToOwningPlayer = Yes` em todas as `DeliveryDecal`, `ObjectCreationList.ini`).
- Só **GPS Scrambler** e **Sneak Attack** disparam aviso de EVA para o inimigo (`SpecialPowerModule.cpp:543–645`). Os outros só fazem o som no local.

**Os aviões podem ser derrubados?** Sim, todos são `AIRCRAFT` atacáveis.

| Avião | Vida | Usado em | Fonte |
|---|---|---|---|
| A-10 | 600 | A-10 Strike | `AmericaAir.ini › AmericaJetA10Thunderbolt` |
| Spectre | 600, selecionável | Spectre Gunship | `AmericaAir.ini`, `AirforceGeneral.ini` |
| B-52 | 1000 | Fuel Air Bomb, Leaflet | `AmericaAir.ini` |
| B-3 (B2) | 1000 | MOAB, Carpet Bomb do Air Force | `AmericaAir.ini`, `AirforceGeneral.ini` |
| Cargueiros dos EUA, China e GLA | 1000 | Paradrop, Cluster Mines, EMP, Anthrax, Tank/Infantry Drop | `AmericaAir.ini`, `ChinaAir.ini`, `GLAAir.ini` |
| Bombardeiro chinês | 1000 | Carpet Bomb da China | `ChinaAir.ini` |

- Todos têm `EMP_HARDENED`: o EMP não os derruba.
- A **Artillery Barrage** vem do `ChinaArtilleryCannon`, que é `UNATTACKABLE`. Não há defesa.
- O **Carpet Bomb do Air Force** tem laser de defesa (`PointDefenseLaserUpdate`, `AirforceGeneral.ini › AirF_AmericaJetB3`).

**Kill de poder dá XP?** Relatado que A-10 e Artillery Barrage não dão (issue [#291](https://github.com/TheSuperHackers/GeneralsGamePatch/issues/291), aberta). Não conferi no código (n/c).

---

## 2. Os poderes de cada facção base

Todos custam 1 ponto. Recarga em min:s.

### 2.1 USA (`SCIENCE_AMERICA_CommandSetRank1/3/8`)

| Poder | Patente | Níveis | Recarga | O que faz | Fonte |
|---|---|---|---|---|---|
| Paladin Tank | 1 | — | — | Destrava o Paladin na War Factory | `Science.ini › SCIENCE_PaladinTank`, `AmericaVehicle.ini` |
| Stealth Fighter | 1 | — | — | Destrava o Stealth Fighter no Airfield | `AmericaAir.ini` |
| Spy Drone | 1 | — | 1:30 | Drone camuflado, visão 250 u (66 m), detecta camuflados, 200 de vida, fica até ser morto (sem `Lifetime`) | `SpecialPower.ini › SpecialPowerSpyDrone`, `AmericaVehicle.ini › AmericaVehicleSpyDrone` |
| Pathfinder | 3 | — | — | Destrava o sniper camuflado no Barracks ($600) | `AmericaInfantry.ini` |
| Paradrop | 3 | I/II/III | 4:00 | 5 / 10 / 20 Rangers. O nível III usa 2 aviões de 10 | `ObjectCreationList.ini › SUPERWEAPON_Paradrop1..3` |
| A-10 Strike | 3 | I/II/III | 4:00 | 1 / 2 / 3 A-10. Cada um dispara 6 mísseis de 200 de dano em raio 50 u (13 m) e metralha com Vulcan (10 por tiro a cada 60 ms) numa faixa de 450 u (118 m) | `SUPERWEAPON_A10ThunderboltMissileStrike1..3`, `Weapon.ini › A10ThunderboltMissileWeapon`, `A10ThunderboltVulcan` |
| Emergency Repair | 3 | I/II/III | 4:00 | Cura de uma vez 100 / 200 / 300 de vida dos veículos (seus e aliados) num raio de 100 u (26 m) | `Object/System.ini › RepairVehiclesInArea_InvisibleMarker_Level1..3`; `AutoHealBehavior.cpp:234` |
| Fuel Air Bomb (Daisy Cutter) | 5 | — | 6:00 | O B-52 solta a bomba: 2000 de dano em raio 100 u (26 m), mais fogo. Cursor de 170 u | `SuperweaponDaisyCutter`; `WeaponObjects.ini › DaisyCutterGas`; `Weapon.ini › DaisyCutterDetonationWeapon` |
| ↳ MOAB (melhoria) | — | — | — | Melhoria do Strategy Center ($4000, 60 s; exige o Fuel Air Bomb). Troca para o B-3 com a MOAB: 2000 em raio 150 u (39 m) e onda de choque de 250 em raio 200 u | `Upgrade.ini › Upgrade_AmericaMOAB`; `Weapon.ini › MOABDetonationWeapon` |
| Leaflet Drop | 5 | — | 5:00 | Desliga infantaria e veículos inimigos por 20 s num raio de 110 u (29 m) | `SuperweaponLeafletDrop`; `WeaponObjects.ini › LeafletContainer`; `EMPUpdate.cpp:523–530` |
| Spectre Gunship | 5 | — | 4:00 | Gira 15 s sobre um círculo de 200 u (53 m). Obus de 80 de dano em raio 25 u a cada 0,3 s; Gattling de 90 por tiro a cada 100 ms, até 3× mais rápida no fogo contínuo | `SuperweaponSpectreGunship`; `AmericaAir.ini › AmericaJetSpectreGunship`; `Weapon.ini › SpectreHowitzerGun`, `SpectreGattlingGun` |

### 2.2 China (`SCIENCE_CHINA_CommandSetRank1/3/8`)

| Poder | Patente | Níveis | Recarga | O que faz | Fonte |
|---|---|---|---|---|---|
| Red Guard Training | 1 | — | — | Red Guards já nascem veteranos | `ChinaInfantry.ini` (`VeterancyGainCreate`) |
| Artillery Training | 1 | — | — | Nuke Cannon e Inferno Cannon já nascem veteranos | `ChinaVehicle.ini` |
| Nuke Cannon | 1 | — | — | Destrava o Nuke Cannon (exige War Factory e Propaganda Center) | `ChinaVehicle.ini › ChinaVehicleNukeLauncher` |
| Cluster Mines | 3 | — | 4:00 | O cargueiro solta um campo circular de minas com raio 80 u (21 m). Cada mina detona até 8 vezes, com 100 de dano. O número de minas segue a densidade padrão (n/c) | `SUPERWEAPON_ClusterMines`; `WeaponObjects.ini › ClusterMinesBomb`; `ChinaClusterMine`; `Weapon.ini › ClusterMineWeapon` |
| Carpet Bomb | 3 | — | 2:30 | O bombardeiro solta 10 bombas de 300 em raio 75 u (20 m) | `Early_SuperweaponChinaCarpetBomb`; `SUPERWEAPON_ChinaCarpetBomb`; `ChinaCarpetBombWeapon` |
| Artillery Barrage | 3 | I/II/III | 5:00 | 12 / 24 / 36 granadas de 105 em raio 50 u, espalhadas num raio de 100 u (26 m) em até 3 s. Vem de fora do mapa e não pode ser parada | `SUPERWEAPON_ArtilleryBarrage1..3`; `ArtilleryBarrageDamageWeapon` |
| Cash Hack | 3 | I/II/III | 4:00 | Rouba $1000 / $2000 / $4000 de um prédio de suprimento inimigo, limitado ao que o inimigo tem | `FactionBuilding.ini › ChinaCommandCenter` (`CashHackSpecialPower`); `CashHackSpecialPower.cpp` |
| Emergency Repair | 3 | I/II/III | 4:00 | Igual ao dos USA | idem |
| Frenzy | 3 | I/II/III | 4:00 | +10% / +20% / +30% de dano por 10 / 20 / 30 s, num raio de 200 u (53 m), para unidades que atacam (inclui aliados; não afeta prédios) | `System.ini › Frenzy_InvisibleMarker_Level1..3`; `GameData.ini › FRENZY_ONE..THREE`; `WeaponBonusUpdate.cpp:131` |
| EMP Pulse | 5 | — | 6:00 | O cargueiro solta o pulso, raio 200 u (53 m). Desliga por 30 s veículos e prédios de facção, inclusive os seus. Derruba aeronaves no ar que não têm `EMP_HARDENED`. Infantaria não sofre | `SUPERWEAPON_EMPPulse`; `System.ini › EMPPulseEffectSpheroid`; `EMPUpdate.cpp:245–282` |

### 2.3 GLA (`SCIENCE_GLA_CommandSetRank1/3/8`)

| Poder | Patente | Níveis | Recarga | O que faz | Fonte |
|---|---|---|---|---|---|
| SCUD Launcher | 1 | — | — | Destrava o lançador (exige Arms Dealer e Palace) | `GLAVehicle.ini` |
| Marauder Tank | 1 | — | — | Destrava o Marauder (Arms Dealer) | `GLAVehicle.ini` |
| Technical Training | 1 | — | — | Technicals já nascem veteranos | `GLAVehicle.ini` |
| Hijacker | 3 | — | — | Destrava o Hijacker (exige Barracks e Palace) | `GLAInfantry.ini` |
| Rebel Ambush | 3 | I/II/III | 4:00 | 4 / 8 / 16 Rebels aparecem no ponto, espalhados até 400 u, surgindo em 3 s | `SUPERWEAPON_RebelAmbush1..3` |
| Cash Bounty | 3 | I/II/III | passivo | Ganha 5% / 10% / 20% do custo de cada unidade ou prédio inimigo morto | `GLACommandCenter` (`CashBountyPower`); `Player.cpp:2458` |
| Emergency Repair | 3 | I/II/III | 4:00 | Igual ao dos USA | idem |
| Anthrax Bomb | 5 | — | 6:00 | O cargueiro solta a bomba: 200 de dano em raio 100 u. Depois, campo de veneno por 60 s, com 40 de dano a cada 0,5 s em raio 300 u (79 m) | `SUPERWEAPON_AnthraxBomb`; `AnthraxBombWeapon`; `PoisonFieldAnthraxBomb`; `AnthraxBombPoisonFieldWeapon` |
| Sneak Attack | 5 | — | 2:30 | Um túnel brota em qualquer ponto em 5 s, com 3 ondas de choque (10 / 50 / 50 de dano), e vira um Tunnel Network de 1000 de vida ligado à sua rede | `OCL_CreateSneakAttackTunnelStart`; `FactionBuilding.ini › GLASneakAttackTunnelNetworkStart` |
| GPS Scrambler | 5 | — | 4:00 | Camufla infantaria e veículos seus num raio que cresce de 20 a 100 u (26 m). A camuflagem é permanente, até a regra normal revelar (ex.: atirar) | `System.ini › GPSScrambler_InvisibleMarker`; `GrantStealthBehavior.cpp:165`; `StealthUpdate.cpp:192` |

**Como o Cash Bounty funciona por dentro**
- Liga quando você compra o poder com um CC vivo, ou quando um CC nasce depois da compra (`CashBountyPower.cpp:135–180`).
- O motor guarda o maior percentual no jogador e nunca zera. Perder o CC não desliga.
- O andaime do CC já liga o bônus. Foi corrigido no patch da comunidade (§4.1).

---

## 3. Os 9 generais: o que cada um troca

Mesmas regras e custo (1 ponto). Só listo o que muda em relação à facção base. Fonte da árvore: `CommandSet.ini › <prefixo>_SCIENCE_*_CommandSetRank1/3/8`. Fonte dos efeitos: o CC de cada general.

### USA

**Air Force (`AirF_`)**
- **Patente 1:** Spy Drone, **Carpet Bomb** e **Emergency Repair I**.
- **Patente 3:** Pathfinder, Paradrop I–III, **Spectre I–II**, Emergency Repair II–III e A-10 I–III.
- **Patente 5:** Fuel Air Bomb, Leaflet e **Spectre III**.
- **Diferenças:**
  - O **Carpet Bomb** usa o B-3 (B2) com 12 bombas de 300 em raio 50 u. Cursor de 180 u, recarga de 4:00, mora no Strategy Center (`AirF_SUPERWEAPON_CarpetBomb`, `AirF_SuperweaponCarpetBomb`).
  - O **Spectre vira 3 níveis**, com recarga de 3:00 e voo de 10 / 15 / 20 s. A vida fica em 600 em todos (`AirF_SuperweaponSpectreGunship`, `AirF_AmericaJetSpectreGunship1..3`).
  - O Emergency Repair desce para a patente 1.
  - **Perde:** o ponto do Paladin. O ponto do Stealth Fighter não existe porque ele é **livre**: só exige o Airfield (`AirF_AmericaJetStealthFighter`).

**Laser (`Lazr_`)**
- **Patente 1:** Spy Drone e Stealth Fighter. **Não tem o ponto do Paladin.**
- **Patentes 3 e 5:** iguais às da base.
- O Spectre é o da base (nível único, 4:00, 15 s).
- A diferença real dele fica nas unidades e superarmas, fora dos poderes.

**Superweapon (`SupW_`)**
- **Patente 1:** Spy Drone e Stealth Fighter, sem Paladin.
- **Patente 3:** Pathfinder, **Leaflet (antecipado para a 3)**, Paradrop, A-10, Emergency Repair e **Spectre I–II**.
- **Patente 5:** Fuel Air Bomb e **Spectre III**.
- Usa o Spectre de 3 níveis do Air Force.
- Fora dos poderes: o Particle Cannon dele recarrega em 3:00, contra 4:00 na base (`SpecialPower.ini › SupW_SuperweaponParticleUplinkCannon`).

### China

**Tank (`Tank_`)**
- **Patente 1:** Red Guard Training, **Battlemaster Training** (Battlemasters já nascem **elite**) e Emergency Repair I.
- **Patente 3:** Cluster Mines, Artillery I–III, **Tank Drop I–III**, Emergency Repair II–III e Frenzy I–III.
- **Patente 5:** EMP e **Carpet Bomb, que sobe para a 5**.
- **Tank Drop:** 1 / 2 / 4 Battlemasters de paraquedas, recarga de 4:00 (`Tank_SUPERWEAPON_TankParadrop1..3`).
- **Perde:** Cash Hack, Nuke Cannon e Artillery Training.

**Infantry (`Infa_`)**
- **Patente 1:** **Mini-Gunner Training** (já nascem elite), Artillery Training, Nuke Cannon e **Frenzy I (antecipado)**.
- **Patente 3:** Cluster Mines, Carpet Bomb, Artillery I–III, Emergency Repair I–III, Frenzy II–III e **Infantry Paradrop I–III**.
- **Patente 5:** EMP.
- **Infantry Paradrop:** 5 / 10 / 15 Mini-Gunners (o III usa 2 aviões, 8 + 7), recarga de 4:00 (`Infa_SUPERWEAPON_Paradrop1..3`). O tooltip diz "Red Guard", mas o poder solta Mini-Gunners.
- **Perde:** Cash Hack. O módulo existe no CC, mas não há botão de compra.

**Nuke (`Nuke_`)**
- **Patente 1:** Red Guard Training, Artillery Training e Emergency Repair I.
- **Patente 3:** Cluster Mines, **Nuke Bomber**, Artillery I–III, Cash Hack I–III, Emergency Repair II–III e Frenzy I–III.
- **Patente 5:** EMP.
- **Nuke Bomber:** 10 bombas de 300 em raio 75 u, com onda de choque (75) e campo de radiação (15 por pulso, raio 50 u, 30 s). Recarga de 3:00, contra 2:30 do Carpet Bomb comum (`Nuke_SuperweaponChinaCarpetBomb`, `Nuke_ChinaCarpetBombWeapon`, `RadiationFieldMedium`).
- O **Nuke Cannon é livre**, sem ponto (`Nuke_ChinaVehicleNukeLauncher`).
- **Cortado:** o "Nuke Drop" (bomba nuclear tática lançada do ar) existe no INI (`Nuke_SCIENCE_NukeDrop`, `Nuke_SuperweaponNukeDrop`), mas não está em nenhuma fileira.

### GLA

**Toxin (`Chem_`)**
- **Patente 1:** SCUD, Marauder e Technical Training.
- **Patente 3:** Ambush I–III com **Toxin Rebels** (4 / 8 / 16, arma de toxina), Cash Bounty e Emergency Repair.
- **Patente 5:** Anthrax Bomb e Sneak Attack.
- **Anthrax Bomb sempre na versão Gamma** (`SUPERWEAPON_AnthraxBombGamma`). Em 1.04 o dano é igual ao da Beta; muda só o tipo de morte (`Weapon.ini › AnthraxGammaBombPoisonFieldWeapon`).
- **Perde:** Hijacker e GPS Scrambler.

**Demolition (`Demo_`)**
- **Patente 1:** igual à base.
- **Patente 3:** Ambush I–III com **Demo Rebels** (4 / 6 / 10, com carga suicida: arma `TERTIARY TerroristSuicideNotARealWeapon`), Cash Bounty e Emergency Repair.
- **Patente 5:** Anthrax e Sneak Attack.
- **Perde:** Hijacker (existe no INI, sem botão) e GPS.
- Fonte: `Demo_SUPERWEAPON_RebelAmbush1..3`, `DemoGeneral.ini › Demo_GLAInfantryRebel`.

**Stealth (`Slth_`)**
- **Patente 1:** Technical Training e Emergency Repair I.
- **Patente 3:** **GPS Scrambler (antecipado para a 3, recarga de 3:00)**, Ambush I–III com **Rebels camuflados** (4 / 8 / 16, `InnateStealth = Yes`), Cash Bounty e Emergency Repair II–III.
- **Patente 5:** Anthrax e Sneak Attack.
- O **Hijacker é livre**, sem ponto (`Slth_GLAInfantryHijacker` só exige o Barracks).
- **Perde:** SCUD e Marauder.

### Resumo das trocas

| General | Ganha | Perde | Antecipa |
|---|---|---|---|
| Air Force | Spectre I–III (3:00), Carpet B-3 no rank 1 | Paladin | ER I → rank 1; Stealth Fighter livre |
| Laser | — | Paladin | — |
| Superweapon | Spectre I–III (3:00) | Paladin | Leaflet → rank 3 |
| Tank | Tank Drop I–III, Battlemaster elite | Cash Hack, Nuke Cannon, Artillery Training | ER I → rank 1; Carpet → rank 5 |
| Infantry | Infantry Paradrop I–III, Mini-Gunner elite | Cash Hack | Frenzy I → rank 1 |
| Nuke | Nuke Bomber (radiação) | Nuke Cannon como ponto (vira livre) | ER I → rank 1 |
| Toxin | Toxin Rebels, Anthrax Gamma | Hijacker, GPS | — |
| Demolition | Demo Rebels suicidas | Hijacker, GPS | — |
| Stealth | Rebels camuflados, GPS a 3:00 | SCUD, Marauder | GPS → rank 3; ER I → rank 1; Hijacker livre |

---

## 4. O que não é poder de usar, mas destrava algo

| Tipo | Itens | Efeito | Fonte |
|---|---|---|---|
| Destrava unidade | Paladin, Stealth Fighter, Pathfinder, Nuke Cannon, SCUD Launcher, Marauder, Hijacker | A science vira pré-requisito da unidade, junto do prédio | `Prerequisites › Science` nos INIs de unidade |
| Já nasce veterano | Red Guard Training, Artillery Training, Technical Training | Veterano: +20% de cadência, +10% de dano | `VeterancyGainCreate`; `GameData.ini › VETERAN` |
| Já nasce elite | Battlemaster Training (Tank), Mini-Gunner Training (Infantry) | Elite: +40% de cadência, +20% de dano | `GameData.ini › ELITE` |
| Destrava melhoria | Fuel Air Bomb abre a melhoria MOAB no Strategy Center | Muda o poder | `CommandButton.ini › Command_UpgradeAmericaMOAB` |
| Passivo | Cash Bounty I–III | % do custo de cada morte | §2.3 |
| Cortados (no INI, sem botão) | Overlord Training, Gattling Tank Training, Tank Hunter Training, Nuke Drop | Teriam feito as unidades nascerem elite ou soltado uma nuke do ar | `Science.ini`, `generals.csf` |

**Battlefield promotion:** o ZH não tem poder que promova unidades em campo. Esse tipo de poder aparece no RotR (Field Promotion da ECA, §6.3).

---

## 5. Patches da comunidade

### 5.1 TheSuperHackers: GeneralsGamePatch ("Community Patch", interno Patch104p)

- **Não mudou custos nem patentes.** `Rank.ini` igual, e tudo segue custando 1 ponto. Só entraram sciences técnicas (Ambush do Demo; a do SatHack 2, de custo 0). Comparação de `GameFilesOriginalZH` com `GameFilesEdited`.
- **Discussão sobre custos:** um jogador estranhou o custo 2 quando todo o resto custa 1 ([#1219](https://github.com/TheSuperHackers/GeneralsGamePatch/issues/1219)).

**Balanço fundido** (`Patch104pZH/Design/Changes/v1.0/`)

| Mudança | PR |
|---|---|
| Paradrop: 3 Rangers + 2 Missile Defenders / 6 + 4 / 12 + 8 | #2026, #2272 |
| Anthrax: campo −20% sem a melhoria; Gamma +20% sobre a Beta | #2044, #2045, #2012 |
| Campos grandes (Anthrax, nuke) não são limpos por um campo pequeno | #2023 |
| Cash Bounty passivo de verdade, sem exigir CC | #2263, #2298 |
| Carpet Bomb do Air Force sem o laser de defesa | #953 |
| Fuel Air Bomb com atraso fixo de 1 s | #1800 |
| Black Lotus rende 2× mais pontos de patente | #1134 |
| −30% de XP ao matar Battlemaster, Marauder, Crusader, Paladin e Microwave | #412 |

**Primeiro balanço deles (revertido)**
- Recargas encurtadas: ER 4:00 → 2:00, Cash Hack 4:00 → 3:00, Frenzy 4:00 → 2:30, Paradrop 4:00 → 2:30.
- Carpet do Air Force alongado: 4:00 → 6:00.
- Pathfinder: $600 → $1500.
- Tudo foi revertido pelo [PR #679](https://github.com/TheSuperHackers/GeneralsGamePatch/pull/679), em 22/07/2022 (`Design/Changes/Legacy/game_balance_legacy104p.md`).

**Em aberto**
- Frenzy: +30% fixo, 10 / 20 / 40 s, recarga de 3:00 (#2251).
- Emergency Repair: várias propostas (#707, #1063–#1067).
- Demo Ambush: atraso de 3 s antes do suicídio (#2303).
- Stealth Fighter sem ponto (#643).
- Carpet do Air Force na patente 3 ou com recarga de 5 a 6 min (#1219).

**Bugs de poder corrigidos ou abertos**
- Tooltips trocados: o Carpet do Air Force dizia 2:30 (é 4:00); o Spectre do Air Force e do Superweapon dizia 4:00 (é 3:00) (#841, #842).
- Cash Bounty ligava pelo andaime do CC (#590, #2263).
- Frenzy revelava o mapa, raio 250 por 30 s (#591, #593).
- Spectre selecionado por todos os jogadores ([GeneralsGameCode #19, #2402](https://github.com/TheSuperHackers/GeneralsGameCode)).
- Emergency Repair engolido pela aura da Ambulance ou da Propaganda (GameCode #240, #3602).
- Exploit de relógio começando em 0 (GameCode #1218).

### 5.2 Patch 1.06 do H2 Clan (2008): a fonte do "1 a 4 pontos"

Fonte: [CNCNZ, Community Patch Spotlight: Zero Hour v1.06](https://forums.cncnz.com/topic/21059-community-patch-spotlight-zero-hour-v106/). Patch do **H2 Clan**: código de INI do AngryHan, mapas do ReLaX, última versão de 22/08/2008. Não é o patch do TheSuperHackers.

**Pontos triplicados:** começa com 3, ganha +3 por patente e +9 na 5. Total de 21.

**O raciocínio:** havia poderes quase OP e outros inúteis, e todos com o mesmo custo de 1 ponto, "which made no sense" (H2 Clan). O objetivo:
- poder caro obriga a abrir mão de outros;
- poderes baratos podem ser pegos juntos;
- quebrar combos de abertura devastadores, como Carpet Bomb + Artillery II na patente 3.

**Custos no 1.06**

| Custo | USA | China | GLA |
|---|---|---|---|
| 1 | Paladin, Stealth Fighter, Pathfinder, ER I–III | Trainings, Nuke Cannon, ER I–III, Frenzy I–III | SCUD, Marauder, Technical Training, Hijacker, ER I–III |
| 2 | Spy Drone, Paradrop I–III | Cash Hack I–III, Tank Drop I–III, Infantry Paradrop I–III | — |
| 3 | Leaflet, A-10 I–III, Spectre I–III (Air Force) | Cluster Mines, Artillery I–III | GPS, Ambush I–III, Cash Bounty I–III |
| 4 | Carpet Bomb, Spectre (nível único), Fuel Air Bomb | Carpet Bomb, Nuke Bomber, EMP | Anthrax, Sneak Attack |

**Outras mudanças do 1.06**
- ER I desce para a patente 1, com recarga de 2:00.
- O Carpet do Air Force sobe para a patente 3. O texto confirma que antes bastava o Strategy Center.
- Spectre I com 400 de vida; Spectre III com 800.
- Paradrop: 3 Rangers + 2 Missile Defenders / 6 + 4 / 9 + 6.
- O texto do 1.06 diz que o Paradrop III do 1.04 soltava 15 Rangers. **O INI original mostra 20** (2 aviões de 10).

---

## 6. Mods

As fontes são o changelog oficial do Contra (fórum Revora), notícias na PPM e as wikis de fãs. ModDB e Reddit bloquearam a leitura automática, então reviews de lá ficam n/c.

### 6.1 Contra 009 Final (o principal)

**Generais e chefes**
- 12 generais jogáveis: os 9 do ZH e mais 3 novos ([wiki](https://zerohourcontra.fandom.com/wiki/Contra_Wiki)):
  - USA Cybernetic (Ironhand);
  - China Flame (Bao);
  - GLA Assault (Huchum).
- Ironside, Leang e Deathstrike são chefes do Challenge.

**Pontos**
- Começa com 1 ponto e ganha +1, +1, +2, +3 ao subir. Total de 8 (conta minha) ([About General's Promotion](https://zerohourcontra.fandom.com/wiki/About_General%27s_Promotion)).
- Quase tudo custa 1 ponto. Exceção: o treino de elite do Laser custava 3. No Patch 3, os Lasers Training 1 e 2 passaram a custar 2 cada ([Revora p39](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-39)).
- **Escada:** a Beta 1 reduziu a XP exigida por patente. A Beta 2 criou a patente 6 (+3 pontos); o Final a removeu ([Revora p38](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-38)).

**Como a patente libera tecnologia (o ponto mais útil pra nós)**
- **Três camadas que abrem sozinhas, sem gastar ponto:**
  - a básica já vem aberta;
  - a do meio abre na **patente 3**: prédios de tecnologia (Palace, Propaganda, Strategy Center) e o aeródromo chinês;
  - a final abre na **patente 5**: superunidades (1 por jogador), superarmas e mini-superarmas.
  - Fonte: [wiki](https://zerohourcontra.fandom.com/wiki/About_General%27s_Promotion).
- **O ponto também compra unidade, prédio e habilidade.** Exemplos:
  - Centurion do Superweapon, na patente 1;
  - Deploy War Factory e Deploy Barracks, que montam o prédio em qualquer lugar do mapa;
  - Fire Shaft do Bao, que libera habilidade.
- **Correntes de pré-requisito:**
  - Commando Drop exige Drop Pods III;
  - Emperor Delivery exige Tank Drop III;
  - Stealth Tunnel exige Sneak Attack.
- **"Training" é veterania de nascença:** Hacker School, Chemic Training, Lasers Training. GLA Training dá +10% de cadência, dano e vida.
- **A camada muda com o balanço.** No Patch 3, o Hijacker passou a exigir só a patente 3, sem o Palace, e o Nuke Detonator Site desceu da patente 5 para a 3.
- **Poderes que entraram e saíram (exemplos):**
  - entraram: SEAL Drop, AA Mine Deployment, Nano Swarm, Hijacker Ambush;
  - saíram: Cluster Mines (Patch 3), Ionization e Fake Units;
  - mudou: os poderes da GLA passaram a chegar num avião próprio, o Corvus.
- **Números:**
  - Frenzy do Bao: +25% por 30 s, +50% por 45 s, +100% por 1 min;
  - Cash Bounty: 5 / 10 / 20%;
  - AA Laser do Townes: derruba 3 / 6 / 9 aviões.
  - Fonte: páginas dos generais na wiki.

**A crítica que o próprio time admite: bola de neve na patente**
- A IA que fica para trás ganha XP extra.
- O Challenge tem trégua quando o jogador chega na patente 3.
- A IA do Cybernetic pausa na patente 3.
- A wiki diz que quem chega primeiro na 3 leva forte vantagem.
- Fontes: Revora p38 e p39.

### 6.2 ShockWave (v1.2, 2016; a wiki cita a 1.25)

- **3 generais novos:** Ironside (blindados), Leang (artilharia e chamas) e Deathstrike (sucata). Os 9 originais ganharam poderes novos ([site oficial](http://shockwave.swr-productions.com/features.php)).
- **Mesma estrutura de 3 fileiras.** O padrão é um poder de patente 1 que destrava uma unidade (que ainda exige o prédio). Exemplos: Howitzer e Mammoth do Ironside; Basilisk e Katyusha do Deathstrike ([wiki](https://shockwave.fandom.com/wiki/Ironside)).
- **Poderes de veterania:** Battlemaster Elite Training (Kwai) e Marksman Training (Fai).
- **ShockWave EX (2023, de fãs) mudou justamente o sistema** ([PPM](https://ppmforums.com/topic-66954/cc-shockwave-ex-has-been-released/)):
  - 2 pontos por promoção;
  - poderes de ataque custam 1, 2 e 3 pontos por nível;
  - o ponto de patente 1 passa a destravar mais de uma unidade.
  - Isso sugere as críticas: 1 ponto por patente é pouco, e destravar uma unidade só é fraco (leitura minha).
- Pontos por patente e XP: n/c.

### 6.3 Rise of the Reds (1.87; a 1.9 estava em produção em 2024)

- **Rússia e ECA**, além das 3 do ZH. Um general por facção ([wiki](https://generalsrotr.fandom.com/wiki/Rise_of_the_Reds)).
- **O ponto de 1 estrela escolhe um ramo da árvore de tecnologia**, de forma passiva. Exemplos ([Russian Promotion](https://generalsrotr.fandom.com/wiki/Russian_Arsenal/General%27s_Promotion), [USA](https://generalsrotr.fandom.com/wiki/USA_Arsenal/General%27s_Promotion)):
  - a Rússia escolhe VVS (Airfield e jatos), Advanced Weapons (Tesla) ou Nuclear Warfare (o silo, ou seja, a superarma vem por esse ponto);
  - os USA escolhem Armored Company (Paladin) ou Covert Forces (Pathfinder, SEAL, Little Bird).
- **3 estrelas:** poderes ativos em 3 níveis. Russian Missile Strike: 1 / 2 / 3 bombardeiros. China Frenzy: +40% de dano por 25 / 50 / 100 s. Algumas linhas destravam em degraus.
- **5 estrelas:** um golpe e um desbloqueio grande, como a Ursa Division (Blackbear, e 3 tanques nascem veteranos).
- **Field Promotion (ECA):** sobe um nível de veterania de tudo num raio de 40, recarga de 2:30 ([wiki](https://generalsrotr.fandom.com/wiki/Field_Promotion)). É a "battlefield promotion" que o ZH não tem.
- **Crítica admitida pelo time:**
  - alguns poderes todo mundo pega, outros ninguém; a meta da 1.9 é desafiar as "must picks";
  - o SCUD foi limitado a 4 para não ser obrigatório.
  - Fonte: [PPM](https://ppmforums.com/topic-70810/rise-of-the-reds-usa-update-armoured-company-part-1/).
- Na 1.9 o Smart Bomb virou um ataque de 3 pontos, mas o detalhe é n/c (só trecho da ModDB).

---

## 7. Como a comunidade competitiva julga

**Fontes**
- Discussões do GeneralsGamePatch com jogadores competitivos (ExiLe, Jundiyy, MTKing4, Stubbjax, xezon, commy2).
- Guias "Basic Strategy" do GameReplays, de 2005. Podem ser anteriores ao 1.04 (n/c).
- O changelog do 1.06.
- **Força da fonte:** C = consenso de várias; I = opinião isolada ou guia antigo.

| Facção | Obrigatórios / preferidos | Ninguém pega | Injustos ou chatos |
|---|---|---|---|
| USA | Spy Drone na patente 1 (I+); **Pathfinder**, "OP", pegar toda partida (C: #690, #1682, guia GR); **A-10** na 3, o nível II já mata a maioria dos prédios (C: #705, #707); Spectre, cujo nível I já basta (C) | **Stealth Fighter** (C: #643); **Paradrop** II e III (C); **Emergency Repair** (C: #707) | **Carpet do Air Force cedo**; torneios proíbem antes da patente 3 (C: #1219, 1.06) |
| China | **Artillery Barrage** completa (C: #708, #709); ordem comum Artillery ×3 + Carpet + Mines + EMP (I+: #2251); Carpet + Artillery arrasava bases (C) | **Frenzy** (C); Cash Hack II e III (dividido: #708); Emergency Repair (C) | Combo Carpet + Artillery na patente 3 (C: 1.06) |
| GLA | **Cash Bounty** essencial (C: #707, #1184); Technical Training e Marauder (I, guia GR); GPS forte (dividido: #1330) | **Hijacker** (C: #644, 1.06); SCUD do Toxin (I+: #784); Ambush comum, só distração (I: #1184) | **Demo Ambush**, banido em jogos com regras (C); **Toxin Ambush** OP para a maioria (C: #872, #1184); Anthrax limpável por veneno, um exploit (I+: #55) |

- Comparação direta **A-10 x Spectre**: n/c. As fontes elogiam os dois, e o A-10 vem antes por estar na patente 3.
- Sem opinião encontrada (n/c): Tank Drop, Nuke Bomber, Battlemaster Training e Leaflet (fora o uso para desligar aviões, I).

**Lições que se repetem nas discussões**
- **Poder bom serve na hora em que recarrega.** Artillery, GPS e Spy Drone servem; Emergency Repair e Frenzy não (xezon, #707).
- **Cada nível tem de dobrar o efeito** (commy2, #2251).
- **Poder destrutivo precisa de uma reação possível do outro lado**, senão é frustrante (Stubbjax, #1184).
- **Poder que dá XP alimenta a bola de neve** (Dominator, #291).

---

## 8. O que levar daqui (curto)

- **Patente vinda de matar:** manter. É o que faz o ZH premiar a agressão.
  - Corrigir o desequilíbrio entre preço e XP (o Stinger Site de $900 vale o mesmo que a War Factory).
  - Decidir de propósito se kill de poder conta.
- **Custo de 1 ponto para tudo:** não copiar.
  - Dar o custo pela força medida, como no 1.06 e no EX, ou fazer de cada fileira uma escolha exclusiva, como o RotR.
- **Relógio do jogador, pronto na compra, sobrevivendo ao CC:** bom. Copiar.
- **Resposta do defensor:** o avião derrubável é a melhor parte.
  - Evitar ataque inatacável, como a artilharia fora do mapa.
  - Dar aviso legível ao alvo, coisa que o ZH quase não faz.
- **A patente como portão de tecnologia (Contra) e o ponto como escolha de ramo (RotR):** são as duas ideias que os mods acrescentam e que a comunidade aceita.
  - As duas precisam de freio contra a bola de neve (bônus para quem está atrás, ou patente mais barata no início).

---

## Fontes principais

- INIs do ZH 1.04: https://github.com/TheSuperHackers/GeneralsGamePatch/tree/main/Patch104pZH/GameFilesOriginalZH/Data/INI (`Science.ini`, `Rank.ini`, `SpecialPower.ini`, `CommandButton.ini`, `CommandSet.ini`, `Upgrade.ini`, `Weapon.ini`, `ObjectCreationList.ini`, `GameData.ini`, `PlayerTemplate.ini`, `Object/*.ini`)
- Textos do jogo: `Patch104pZH/GameFilesOriginalZH/Data/English/generals.csf`
- Código do motor: https://github.com/TheSuperHackers/GeneralsGameCode (cópia local em `C:\Users\luisf\newcnc\GeneralsGameCode`, commit `ca9358fcc`)
- Patch da comunidade: https://github.com/TheSuperHackers/GeneralsGamePatch (issues e PRs citados por número)
- Patch 1.06 (H2 Clan): https://forums.cncnz.com/topic/21059-community-patch-spotlight-zero-hour-v106/
- Guias do GameReplays: [USA](https://www.gamereplays.org/cnczerohour/portals.php?show=page&name=USAbasicstrats), [China](https://www.gamereplays.org/cnczerohour/portals.php?show=page&name=Chinabasicstrats), [GLA](https://www.gamereplays.org/cnczerohour/portals.php?show=page&name=GLAbasicstrats)
- Contra: [changelog no Revora](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-38) e [wiki](https://zerohourcontra.fandom.com/wiki/About_General%27s_Promotion)
- ShockWave: [site oficial](http://shockwave.swr-productions.com/features.php), [wiki](https://shockwave.fandom.com/wiki/Ironside), [EX na PPM](https://ppmforums.com/topic-66954/cc-shockwave-ex-has-been-released/)
- RotR: [wiki](https://generalsrotr.fandom.com/wiki/Rise_of_the_Reds), [atualizações da 1.9 na PPM](https://ppmforums.com/topic-70810/rise-of-the-reds-usa-update-armoured-company-part-1/)
