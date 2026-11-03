# Estudo: superarmas em RTS (para o nosso RTS de 4 facções: USA, China, GLA, Rússia)

Pesquisa feita em 01–02/11/2026. Sem nenhuma mudança no repositório.

**Convenções**
- 1 unidade do ZH = 0,2633 m. Os raios aparecem nas duas medidas.
- "n/c" = não confirmado. "estimativa" = conta minha a partir dos dados, não número do jogo.

**Método**
- **ZH:** lido direto dos INI originais do Zero Hour e conferido no código do motor (links na §1.0).
- **Outros jogos:** sempre que deu, pelos arquivos de dados (rulesmd.ini, XML do SAGE, YAML do OpenRA, blueprints do FAF, unitdefs do BAR, JSON do CoH3). Fora isso, por wikis de dados. Dois agentes de pesquisa levantaram as tabelas de outros jogos; as fontes de cada linha estão nas listas de links.

---

## 0. Resumo

1. **No ZH as três superarmas são o mesmo pacote com efeitos diferentes:**
   - custam $5000 (metade do dinheiro inicial padrão de $10.000), levam 60 s pra construir, têm 4000 de vida e blindagem "dura";
   - recarregam em 4 min (Particle Cannon), 5 min (SCUD Storm) ou 6 min (Nuclear Missile);
   - o timer só começa quando o prédio fica pronto, e é **público pra todos**;
   - disparar é grátis.
2. **Uma salva do ZH destrói a "camada de produção" num raio de 30 a 50 m e não mata o Command Center.** A nuke dá no máximo 2800 num CC de 5000. O Particle Cannon parado 10 s em cima de um CC tira só 20% da vida dele.
3. **O ZH já tem o "LIMITED" pronto.** A caixa "Limit Superweapons" da sala põe o limite em **1 superarma por jogador, somando todos os tipos** (e o buraco de reconstrução do SCUD). Confirmado no código.
4. **Fora da série C&C, quase todo RTS cobra por disparo:** munição fabricada (SupCom, BAR, SC2, Act of War), pontos ou recurso por uso (WiC, CoH3, Halo Wars, DoW II), custo que cresce (Rise of Nations, Ashes) ou custo político (Dune, Alpha Centauri). O "construiu e é grátis pra sempre" é marca da série C&C.
5. **O que separa a superarma amada da odiada:**
   - **Amada:** aviso público em três tempos, contra-jogo durante a carga, não acabar a partida sozinha, custo de lançar que é uma decisão.
   - **Odiada:** disparo sem aviso, várias acumuladas de uma vez, "grátis pra sempre" que premia quem se esconde, efeito que cobre a batalha.

---

## 1. Zero Hour, pelos arquivos originais

### 1.0 Fontes (primárias)

**INI originais do ZH 1.04**, no repositório público TheSuperHackers/GeneralsGamePatch, pasta `Patch104pZH/GameFilesOriginalZH/Data/INI`. Último commit do SpecialPower.ini: `ecac486`, de 23/07/2022.
- https://github.com/TheSuperHackers/GeneralsGamePatch/tree/main/Patch104pZH/GameFilesOriginalZH/Data/INI
- `SpecialPower.ini`: recarga, timer público, sons.
- `Object/FactionBuilding.ini`: prédio, custo, vida, energia, módulo do Particle Cannon.
- `Object/WeaponObjects.ini`: o objeto NeutronMissile (as 9 ondas da nuke), o ScudStormMissile e o feixe.
- `Weapon.ini`: o dano de cada SCUD, os campos de veneno e de radiação.
- `ObjectCreationList.ini`: os 9 tiros do SCUD.
- `Object/System.ini`: a duração dos campos.
- `Armor.ini`, `Eva.ini`, `GameData.ini` e `multiplayer.ini` (dinheiro inicial).
- Os generais vêm de `Object/NukeGeneral.ini`, `SuperWeaponGeneral.ini`, `ChemicalGeneral.ini`, `DemoGeneral.ini`, `LaserGeneral.ini`, mais `CommandSet.ini` e `CommandButton.ini`.

**Código do motor do ZH**, em TheSuperHackers/GeneralsGameCode (https://github.com/TheSuperHackers/GeneralsGameCode):
- `GeneralsMD/.../Update/ParticleUplinkCannonUpdate.cpp`: raio e dano por pulso.
- `.../Update/NeutronMissileSlowDeathUpdate.cpp`: tipo de dano da nuke e queda com a distância.
- `.../SpecialPower/SpecialPowerModule.cpp` e `.../Create/SpecialPowerCreate.cpp`: quando o timer começa e quando pausa.
- `.../Object/Object.cpp`: qualquer desligamento pausa o timer.
- `Core/.../Common/Thing/ThingTemplate.cpp` e `GeneralsMD/.../Menus/LanGameOptionsMenu.cpp`: o "Limit Superweapons".
- `.../GameClient/InGameUI.cpp`: timer público e EVA de "pronta".

### 1.1 As três superarmas (versão base)

| | **Particle Uplink Cannon (USA)** | **Nuclear Missile (China)** | **SCUD Storm (GLA)** |
|---|---|---|---|
| Pré-requisito | Strategy Center | Propaganda Center | Palace |
| Custo | **$5000** | **$5000** | **$5000** |
| Tempo de construção | 60 s | 60 s | 60 s |
| Vida | 4000 | 4000 | 4000; ao morrer deixa um **buraco de reconstrução** com 500 de vida |
| Blindagem | StructureArmorTough | idem | idem |
| Energia | −10 | −10 | 0 (a GLA não usa energia) |
| Recarga (`ReloadTime`) | **240 s = 4 min** | **360 s = 6 min** | **300 s = 5 min** |
| 1º disparo | só depois da recarga inteira: o timer começa quando o prédio fica pronto (`SpecialPowerCreate::onBuildComplete`) | idem | idem |
| Timer público (`PublicTimer`) | **Sim** | **Sim** | **Sim** |
| Cursor de área | sem círculo: o jogador "dirige" o feixe | 210 u = **55,3 m** | 200 u = **52,7 m** |
| Som | carga e disparo do canhão | **sirene antiaérea no lançador e no alvo** (`AirRaidSiren`, `InitiateAtLocationSound`) | `ScudStormInitiated` |
| Visão dada ao atirador no alvo | 250 u = 65,8 m, por 30 s | 250 u, por 40 s | 250 u, por 40 s |
| O prédio aparece pra todos | **Sim**: `ShroudRevealToAllRange = 60` (15,8 m em volta) | Sim | Sim |
| Custo pra disparar | **nenhum** | nenhum | nenhum |

Dinheiro inicial padrão do ZH: **$10.000**. A sala oferece 5, 10, 20 ou 50 mil (`GameData.ini: DefaultStartingCash = 10000`; `multiplayer.ini`). **Então a superarma custa 50% do dinheiro inicial padrão.**

### 1.2 O dano, peça por peça

**Particle Uplink Cannon** (`ParticleUplinkCannonUpdate` no FactionBuilding.ini, mais o código)

| Parâmetro | INI | Em metros e no efeito |
|---|---|---|
| Sequência de carga, visível no prédio | 5 s com os nós externos acendendo, depois 4,67 s com a antena subindo, depois 2 s até ficar pronto | Começa **11,7 s antes** do timer zerar. É um aviso visual pra quem está vendo o prédio |
| Tempo do feixe até o chão | `BeamTravelTime` 2,5 s | **2,5 s de reação** depois do clique |
| Tempo no chão | `TotalFiringTime` 10 s, com 2 s de feixe crescendo e 2 s diminuindo | — |
| Dano | `DamagePerSecond` 400 em **40 pulsos**, ou seja, **100 por pulso a cada 0,25 s** | Tipo PARTICLE_BEAM |
| Raio do dano | raio lógico do feixe 13 u × `DamageRadiusScalar` 3,4 = **44,2 u** | **11,6 m** (23 m de diâmetro). Largura visual do feixe: 26 u = 6,8 m |
| Movimento | o jogador **dirige o feixe**: 20 u/s (5,3 m/s), ou 40 u/s (10,5 m/s) com duplo clique. A IA faz um zigue-zague de 200 u (52,7 m) de comprimento por 50 u (13,2 m) de largura | — |
| Rastro | 40 "remanescentes": 15 de dano a cada 0,25 s no raio 10 u (2,6 m), por 4 s | O chão queima atrás do feixe |
| Atinge | todos os vivos no raio, **inclusive os do próprio dono** (só filtra "vivo") | — |

**Nuclear Missile** (`NeutronMissileSlowDeathBehavior` no objeto NeutronMissile)
- Nove ondas. As ondas 1 a 5 (raios de 60 a 180 u) só derrubam e empurram, com dano 0.
- **A onda 6 (1,18 s após a explosão) faz todo o dano:**
  - 3500 até 60 u (**15,8 m**);
  - cai em linha reta até 210 u (**55,3 m**), com mínimo de 300 até a borda;
  - tipo **EXPLOSION**, fixo no código (`DAMAGE_EXPLOSION`), e atinge aliados também.
- **Depois deixa radiação:** 25 de dano a cada 0,75 s no raio de 200 u (**52,7 m**), por **30 s** (`NukeRadiationFieldWeapon`).

**SCUD Storm** (`OCL SUPERWEAPON_ScudStorm` + `ScudStormWeapon` + `ScudStormDamageWeapon`)
- **9 mísseis.**
  - 3 s de espera inicial (`PreAttackDelay`), depois 0,1 a 1 s entre eles.
  - Os mísseis espalham numa tabela fixa, escala 120 u. Dois caem a até ~8 m do ponto mirado, três a até ~13 m e o resto entre 19 e 27 m.
- **Cada míssil:**
  - **500 de dano até 50 u (13,2 m)** e **150 até 200 u (52,7 m)**;
  - tipo EXPLOSION;
  - cria um **campo de veneno grande**: 15 de dano a cada 0,5 s no raio de 140 u (**36,9 m**), por **45 s**.
- **Com Anthrax Beta:** o secundário vira 200 e o veneno 25 a cada 0,5 s.
- Os mísseis têm 10.000 de vida e não são interceptáveis na prática (é tipo PROJECTILE, sem SMALL_MISSILE). O míssil da nuke é `UNATTACKABLE`.

### 1.3 O que uma salva mata no ZH (armadura × vida)

Armaduras (`Armor.ini`):
- **Prédio comum (StructureArmor):** EXPLOSION 100%, PARTICLE_BEAM **200%**, RADIATION **0%**, POISON **1%**.
- **Command Center e as superarmas (StructureArmorTough):** EXPLOSION **80%**, PARTICLE_BEAM **25%**.
- **Tanque (TankArmor):** PARTICLE 100%, RADIATION 50%, POISON 25%.
- **Infantaria (HumanArmor):** PARTICLE 150%.

Vida dos prédios no ZH (FactionBuilding.ini):

| Prédio | Vida | Custo |
|---|---|---|
| Command Center | 5000 | $2000 |
| War Factory / Supply Center | 2000 | $2000 |
| Palace | 3000 | — |
| Strategy Center | 1500 | — |
| Reator da China | 1500 | — |
| Barracks | 1000 | — |
| Usina dos EUA | 800 | $800 |
| Tanque | ~400–480 | — |

| Alvo | **Nuke** (raio onde morre) | **Particle Cannon** (tempo sob o feixe até morrer) | **SCUD Storm** |
|---|---|---|---|
| Usina dos EUA (800) | até **46 m** | 1 s | morre com o secundário de 6 mísseis (900) |
| Barracks (1000) | até **44 m** | 1,25 s | 2 primários, ou 7 secundários |
| Reator da China (1500) | até **38 m** | 2 s | 3 primários |
| War Factory (2000) | até **33 m** | 2,5 s | 4 primários. Estimativa: quem está no ponto mirado leva ~3 primários + 6 secundários ≈ 2400, então morre |
| Palace (3000) | até **21 m** | 3,75 s | 6 primários |
| **Command Center (5000, Tough)** | **sobrevive**: no máximo 2800 (56%) | 10 s parado tira **1000 (20%)** | o primário vale 400 contra ele (Tough 80%). Estimativa: ~1500–2500, ou seja, 30–50% |
| Outra superarma (4000, Tough) | sobrevive com 30% | 10 s tiram 25% | idem |
| Tanque (~480) | até **~50 m**; tudo com até 300 de vida morre nos 55 m inteiros | ~1,2 s | morre a até 13 m; fora disso leva 150 por míssil |
| Infantaria | morre na área toda; e a radiação dá 1000 em 30 s | instantâneo | morre a até 13 m; o veneno (30 de dano/s) termina o serviço |

As distâncias da nuke saem da conta com o dano em linha reta entre 60 u e 210 u (contas minhas).

**Leitura:**
- A nuke apaga um raio de ~35–45 m de prédios comuns e de tudo o que é unidade, e deixa o terreno proibido por 30 s.
- O Particle Cannon é cirúrgico. Cerca de 10 m de largura, mas quem o dirige consegue desenhar um corredor de ~50–100 m de prédios mortos.
- O SCUD é área e negação: mata os prédios pequenos e a infantaria em ~50 m, aleija a War Factory e deixa veneno por 45 s.
- **Nenhuma das três mata o Command Center numa salva só.**

### 1.4 Aviso no ZH: quem vê o quê

- **Timer público:** todos os jogadores veem o contador de cada superarma no canto da tela, com nome e cor do dono (`PublicTimer = Yes`; `InGameUI::addSuperweapon`).
- **EVA** (`Eva.ini`, `SpecialPowerModule.cpp`, `InGameUI.cpp`):
  - eventos *Detected*, *Ready* e *Launched*, em versões pra própria, aliada e inimiga, nas três armas;
  - "Ready" e "Launched" confirmados no código;
  - o momento exato do "Detected" não foi localizado no código (n/c). No jogo, toca quando a superarma inimiga aparece.
- **Prédio revelado a todos** num raio de 15,8 m (`ShroudRevealToAllRange`). O inimigo sempre sabe onde mirar.
- **Avisos que se veem:**
  - o Particle Cannon acende os nós 11,7 s antes de ficar pronto;
  - o silo da nuke abre as portas em 8 s (`DoorOpenTime`) e a sirene toca no lançador **e no alvo**.
- **O círculo do alvo só o dono vê** (`OnlyVisibleToOwningPlayer = Yes`). O alvo fica sabendo pelo EVA, pela sirene no local (nuke) e pelo próprio míssil ou feixe.

### 1.5 Contra-jogo no ZH

- **Cortar energia ou desligar o prédio pausa o timer.** O timer congela, não zera.
  - Qualquer desligamento conta: falta de energia, EMP, hack ou dano de subjugação (Microwave). É `Object::pauseAllSpecialPowers`.
  - Só o estado "HELD" não pausa.
  - O SubdualDamageCap de 4200 é maior que a vida de 4000, então o prédio pode ser subjugado.
- **Destruir o prédio.** São 4000 de vida e 80% contra explosão.
  - Contra o SCUD, o buraco da GLA reconstrói de graça se não for destruído (500 de vida).
  - O timer recomeça do zero, porque a construção nova chama `onBuildComplete`.
- **Capturar.** As três têm `CAPTURABLE`. O que acontece com o timer depois da captura: n/c.
- **Interceptar:** não existe no ZH.
- **Dispersar** as unidades quando o EVA avisar. O Particle Cannon dá 2,5 s de reação.

### 1.6 Os generais do ZH que mexem nisso

| General | O que muda (INI) |
|---|---|
| **Superweapon General** (Alexander, USA) | Particle Cannon custa **$2500** com a mesma recarga de 4 min. Pode construir **também a Nuclear Missile** ($5000) com **recarga de 4 min**, contra 6 do padrão (`SupW_SuperweaponNeutronMissile`, mesmo dano). Existe um modelo de 3 min pro canhão (`SupW_SuperweaponParticleUplinkCannon`), mas os botões usam o padrão de 4 min |
| **Nuke General** (Tao, China) | Nuclear Missile a **$4500**. Mesma nuke e mesma recarga de 6 min. Existe um modelo de 5 min (`Nuke_SuperweaponNeutronMissile`) ligado a um botão que nenhum CommandSet usa, ou seja, não entra no jogo pelos INI |
| **Toxin General** (Thrax, GLA) | SCUD já começa com Anthrax Beta (secundário 200, veneno 25 a cada 0,5 s). Com Anthrax Gamma, primário 550 (`Chem_ScudStormDamageWeaponGamma`) e veneno gama de 25 a cada 0,5 s, 140 u, 45 s |
| **Demolition General** (Juhziz, GLA) | SCUD **sem veneno**, 600 de primário em 75 u (19,7 m) e 150 em 200 u. Míssil mais rápido (700 contra 600) |
| **Laser General** (USA) | Tem um "Laser Cannon" cortado: recarga de 2 min, botão de construir comentado no CommandSet. Fica com o Particle Cannon padrão |
| Air Force, Tank, Infantry, Stealth | Iguais ao padrão |

### 1.7 A opção da sala: o "LIMITED" do ZH

- Todas as superarmas têm `MaxSimultaneousOfType = DeterminedBySuperweaponRestriction` e `MaxSimultaneousLinkKey = Superweapon`. O comentário no INI diz que elas contam **todas juntas, inclusive o buraco do SCUD**, como um tipo só.
- Na sala, a caixa **"Limit Superweapons" põe a restrição em 1** (`LanGameOptionsMenu.cpp: setSuperweaponRestriction(1)`); desmarcada fica 0, que é ilimitado.
- **Então o LIMITED do ZH = no máximo 1 superarma por jogador, de qualquer tipo.**
- Sem o limite, o jogador pode ter várias, cada uma com o próprio timer. É isso que deixa disparar 3 nukes de uma vez.

### 1.8 As "secundárias" do ZH: poderes de general

- **São grátis e pagas com pontos de patente ganhos em combate.**
- **Já começam prontas pra disparar.** Está no código: "Dustin wants these special powers to start ready to fire" (`SpecialPowerModule.cpp`).
- O timer delas é privado (`PublicTimer = No`).
- Recargas (SpecialPower.ini):

| Poder | Recarga |
|---|---|
| Carpet Bomb | 2,5 min |
| Sneak Attack | 2,5 min |
| A-10 | 4 min |
| Spectre | 4 min |
| Cluster Mines | 4 min |
| Cash Hack | 4 min |
| Emergency Repair | 4 min |
| GPS Scrambler | 4 min |
| Artillery Barrage | 5 min |
| Daisy Cutter / MOAB | 6 min |
| EMP | 6 min |
| Anthrax Bomb | 6 min |

### 1.9 Competitivo do ZH

- As regras-padrão de torneio do GameReplays **não proíbem superarmas**. Proíbem bugs, como o "Scud bug" de disparar antes do 0:00, e exigem GenTool (https://www.gamereplays.org/community/Standard_Tournament_Rules-t1051394.html).
- Quanto a superarma realmente aparece nas partidas 1v1 de alto nível: n/c. O que se sabe é que $5000 + 60 s + 4 a 6 min de espera é muito exército; inferência minha, sem fonte.

---

## 2. Os outros jogos

### 2.1 Série C&C

Fontes: rules/rulesmd.ini, XML do SAGE (https://github.com/MetaIdea/sage-ini-xml-source-code), código do Remastered (https://github.com/electronicarts/CnC_Remastered_Collection), YAML do OpenRA, cnc.fandom (links na §2.4).

**Tiberian Sun / Firestorm** (Rules.ini/Firestrm.ini do pacote do CnCNet). Referência: o Construction Yard tem 1000 de vida e custa $2500.

| Arma | Prédio | Carga | Efeito | Contra-jogo |
|---|---|---|---|---|
| Ion Cannon | $1500 (+ Upgrade Center $1000), −100 de energia | 8,5 min | 751 de dano num ponto: mata 1 prédio | energia (`IsPowered`) |
| Multi-Missile (Nod) | Silo $1300, −50 | 10 min | 10 × 130 | energia; destruir o silo |
| Firestorm Defense | $2000, −200 | carrega 3 min, dura ~1 min | muro que destrói tudo, inclusive mísseis | sobrecarregar com dano (o dano drena a carga) |
| Hunter-Seeker | $1000–2000 | 12 min | alvo aleatório, sem controle | só a Firestorm para |
| EMP | $1000, −150 | 4,5 min | desliga veículos | energia |

O multiplayer do TS não tem nuke.

**Red Alert 2 / Yuri's Revenge** (rulesmd.ini). Referência: o ConYard custa $3000 e tem 1000 de vida em concreto.

| Arma | Prédio | Recarga | Efeito | Aviso |
|---|---|---|---|---|
| Nuclear Missile | $5000, −200 de energia, limite de 1 | **10 min** | 600 de dano, raio de 10 células, só 8% em concreto, mais radiação | timer público + EVA "detected" e "launched" |
| Weather Control | $5000, −200 | 10 min | raios de 250 (2% em concreto); corta o radar de todos | timer + EVA |
| Iron Curtain | $2500 | 5 min | invulnerabilidade 3×3 por ~50 s | timer + EVA |
| Chronosphere | $2500 | 7 min | teleporta 9 unidades | timer + EVA |
| Psychic Dominator | $5000 | 10 min | domina unidades + 1000 em prédios (6% em concreto) | timer, mas **sem EVA no disparo** (reclamação recorrente) |
| Force Shield (YR) | Battle Lab | 5 min | prédios invulneráveis ~25 s, mas **apaga a própria energia por ~60 s** | sem timer |
| Paradrop / Spy Plane | $1000 | 4 min | não precisa de energia | — |

- Sem energia, o timer **pausa e não zera**. **Um espião infiltrado zera o timer.**
- Concreto aguenta tudo: o ConYard sobrevive.
- **Competitivo:** o Quick Match do Westwood Online tinha superarmas sempre ligadas (n/c). No CnCNet, as superarmas vêm decididas por mapa; não achei regra oficial desligando (n/c).

**C&C3 Tiberium Wars / Kane's Wrath** (XML). Referência: ConYard $3500, 20.000 de vida.

| Arma | Prédio | Carga | Dano |
|---|---|---|---|
| Ion Cannon | $5000, −20 | **7 min** | 20.000 no raio 250 |
| Temple of Nod (nuke) | $5000 | 7 min | 20.000 no raio 300 |
| Rift Generator | $5000 | 7 min | 18.000 no total em 9 s |

- **Uma salva mata usinas e fábricas; o ConYard e as outras superarmas sobrevivem, muito danificados.**
- O prédio e o contador ficam visíveis desde a construção, e o EVA avisa.
- Limite de 1 por jogador.
- Patch 1.05: desligar o Temple não aborta mais um míssil já lançado.
- **Support powers PAGOS por uso** (custo / recarga):

| Facção | Poderes |
|---|---|
| GDI | Radar Scan $300 / 2 min · Orca Strike $500 / 1,5 min · Shockwave Artillery $2000 / 3 min · Orbital Strike $3000 / 7 min |
| Nod | Decoy Army $500 / 1 min · Catalyst Missile $2000 / 5 min · Cloaking Field $3000 / 7 min · Tiberium Vapor Bomb $3500 / 5 min |
| Scrin | Ichor Seed $1000 / 3,3 min · Overlord's Wrath $4000 / 7 min |

- **Os patches subiram os preços por uso:** Radar Scan de 250 pra 300, Catalyst e Shockwave +33%, Cloaking +100%.
- O jogo não tem opção pra desligar superarmas.
- Uma entrevista da EA LA explicando por que o poder custa dinheiro: **não encontrada (n/c)**.

**Red Alert 3 / Uprising** (XML 1.12). Referência: ConYard $5000, 4000 de vida.

| Arma | Custo | Recarga | Efeito |
|---|---|---|---|
| Proton Collider | $2500 | 6 min | ~50% da vida de um ConYard |
| Vacuum Imploder | $2500 | 6 min | puxa e destrói unidades |
| Psionic Decimator (Uprising) | $4000 no XML (n/c) | 6 min | — |
| Iron Curtain, Chronosphere, Nanoswarm (secundárias) | $1500 | 3 min | — |

- **O patch 1.11 cortou o preço de todas pela metade** ($5000 → $2500, $3000 → $1500).
- Timer público e aviso no disparo.
- **Top Secret Protocols:**
  - pontos que chegam com o tempo, mais rápido em combate (Threat Meter);
  - 1 ponto por protocolo;
  - disparo grátis com recarga (por exemplo, 120/240/360 s por nível).
- **Amer Ajami (produtor):** a equipe não via as superarmas como as viradas de jogo que eram no RA2; seriam uma camada a mais de nuance no online (https://digitalchumps.com/2008/07/19/exclusive-interview-command-and-conquer-red-alert-3/3, via trecho da busca; a página não abriu, n/c palavra por palavra).
- Comunidade: "rarely used in competitive", mesmo depois de cortar o preço.

**C&C Rivals:**
- **Nuke no centro do mapa:** é de quem controla mais pads (de 3); a carga sobe sempre; **a base cai com 2 nukes**.
- **Ion Cannon do Solomon:** 130 de Tiberium por uso, recarga de 30 s, 3,3 s de aviso. Dá 6000 em unidades e 4000 na base (era 6000 até dez/2018).
- A superarma vira **objetivo disputado**, não prédio (https://cnc.fandom.com/wiki/Command_%26_Conquer:_Rivals).

**Tempest Rising (2025):**
- **Lançou sem superarmas.** Houve reclamações ("C&C clone without superweapons"); o contra-argumento dos jogadores foi que elas "lowered the skill ceiling".
- **O update de 02/12/2025 adicionou uma por facção:**

| Arma | Custo | Energia / vida | Carga | Efeito |
|---|---|---|---|---|
| GDF Drone Strike Station | 4000 + 50 de Intel | −60 / 4000 | 6 min | 9 drones de 1000 |
| Dynasty Excavator | 5500 no changelog (4600 na wiki) | — | 5 min | terremoto de 3 × 1100; **Drill Pod extra a 500 por uso / 40 s** |
| Veti Sanctifier Portal | n/c | — | n/c | — |

- **Na sala, o padrão é "máximo de 1 superarma por jogador"**, com opção de desligar ou deixar ilimitado.
- Support power pago: Tempest Air Strike 3000 / 3 min.

**Remasters e OpenRA:**
- **TD Remastered (código da EA):**
  - Ion 10 min, Nuke 14 min;
  - **a nuke do multiplayer dá 200 de dano em 3 células, contra 1000 em 4 na campanha** (enfraquecida de propósito);
  - sem energia, pausa;
  - a sala tem `AllowSuperWeapons`.
- **OpenRA (RA):**
  - Nuke $2500, 9 min; Iron Curtain e Chronosphere 2 min;
  - espião zera; energia baixa pausa; timer visível;
  - a sala tem "No Superweapons", mas **o ladder oficial trava "Unrestricted", ou seja, superarmas ligadas**.

### 2.2 Fora da série C&C

**Supreme Commander: Forged Alliance (FAF, blueprints atuais)**

| Item | Prédio | **Cada disparo** | Tempo | Efeito |
|---|---|---|---|---|
| Silo nuclear T3 | 16.500 de massa / 412.500 de energia | **míssil de 12.000 M + 1.500.000 E** | 300 s por míssil | 70.000 no raio 30; 500 até 40; guarda 5 |
| Antinuke SMD | 7.500 M | **interceptor de 3.600 M + 360.000 E** | 240 s | cobre raio 90; guarda 7 |
| Yolona Oss | 187.650 M | 600 M (o custo está no prédio) | 60 s | 1.000.001 no raio 45 |
| Mavor / Scathis / Salvation | ~200–225 mil M | **energia por tiro** (15.000 a 30.000 E) | — | artilharia de mapa inteiro |

- O interceptor custa **~1/3 da massa** do míssil: **defender sai mais barato**. Por isso o atacante sobrecarrega ou mira fora da cobertura.
- O lobby do FAF tem atalho pra proibir os silos.
- Chris Taylor, na visão da imprensa: arma muito forte **junto com** as ferramentas pra pará-la (GameSpot/PC Gamer; https://www.gamespot.com/articles/supreme-commander-updated-qanda-nukes-scale-and-the-supreme-commander-itself/1100-6162843/).

**Beyond All Reason** (unitdefs)

| Item | Prédio (metal / energia) | **Cada disparo** | Carga | Efeito |
|---|---|---|---|---|
| Armageddon (Armada) | 8100 / 90.000 | **1000 metal + 125.000 E** | 120 s | 12.500 de dano (só **2.500 no comandante**) |
| Apocalypse (Cortex) | 7700 / 82.000 | **1500 metal + 187.500 E** | 180 s | 15.000 de dano |
| Citadel (antinuke) | 1500 / 38.000 | interceptor de **150 metal + 7.500 E** | 90 s | — |
| Ragnarok / Calamity | ~70.000 de metal | **15.000–18.000 E por tiro** | — | — |

- O interceptor custa **~1/7 do metal** do míssil.
- Opções do lobby desligam nukes, antinukes e os "lolcannons" (o nome está no próprio código).

**StarCraft II** (Liquipedia, Blizzard)
- **Custo:**
  - **cada bomba custa 100 minerais / 100 de gás**;
  - pra chegar nela, Ghost Academy (150/50) + Ghost (150/125);
  - 43 s pra armar; **1 bomba por Academy**.
- **Disparo:**
  - o Ghost marca o alvo e a bomba cai **14 s** depois (eram 20);
  - 300 de dano (+200 em prédio), raio 8, atinge aliados.
- **Aviso:** **"Nuclear launch detected"** pra todos e **ponto vermelho no alvo, visível até na névoa**.
- **Contra-jogo:** matar ou empurrar o Ghost **aborta e a bomba se perde**.
- **No alto nível quase não se usa.** Serve como **negação de área** (força o adversário a sair do cerco). O custo real é o lançador exposto.

**World in Conflict** (Massive)
- **Custo em Tactical Aid points**, ganhos matando, tomando pontos de comando, consertando e fortificando. **Dá pra doar pontos ao colega.**

| Ajuda tática | Pontos | Chegada | Recarga |
|---|---|---|---|
| Nuke tática | 80 (a wiki lista 80/140/180; n/c) | 16 s | 450 s |
| Carpet bomb | 45 | 15 s | 90 s |

- **A nuke:** deixa radiação de 20/s e **neutraliza os pontos de comando**. Serve pra virar o placar.
- **Intenção da Massive:** "muito difícil de obter mais de uma vez" por partida (prévia de 2007). Martin Walfisz diz que criaram mais ajudas táticas do que o planejado porque eram "so much fun" (https://spong.com/feature/10109558/Interview-World-In-Conflict-Developer-Martin-Walfisz/4).

**Company of Heroes 3** (dados do jogo, cohstats)
- **Os pontos de comando desbloqueiam; cada uso custa munição**, um recurso que vem do território, e tem recarga.

| Habilidade | Munição por uso | Recarga |
|---|---|---|
| Carpet Bombing (US) | 200 | 180 s |
| V1 | 225 | 180 s |
| Barragem de 240 mm | 200 | 180 s |
| Bombardeio naval (UK) | 250 | 180 s |

**Halo Wars 1 e 2** (Halopedia)

| Jogo | Como paga | Exemplos | Recarga |
|---|---|---|---|
| HW1 | **600 por uso** (MAC Blast, Carpet Bomb, Cryo); as melhorias aumentam dano, custo e recarga | Disruption Bomb (450): **bloqueia poderes** numa área por 1 min | — |
| HW2 | pontos de líder liberam; **cada uso custa supply e power** | Eradication 1000/1000; Glassing Beam de 350/150 até 550/350; ODST 1650/625 | 360 s (Eradication), 228 s (Glassing) |

No HW2 não achei custo que suba por uso (n/c).

**Act of War (2005) e Act of Aggression (2015)**, os dois da Eugen

| Jogo / arma | Prédio | **Cada disparo** | Guarda | Defesa |
|---|---|---|---|---|
| AoW, Wolverine | $3000 | **$3000 por míssil** (o preço do prédio) | 1 | Patriot; a melhoria "Tactical warning system" **é o aviso comprado** |
| AoA, Peacekeeper | 1000 $ + 1000 de alumínio + 750 de terras raras | **4000 $ + 250 de terras raras** | 1 | antimíssil que gasta munição |
| AoA, THOR | 3000 $ + 500 de terras raras | **750 de terras raras** | 4 | idem |

- **Regras do Act of Aggression:**
  - **um tiro tira 1/4 da vida do QG**;
  - mata por superarma rende 0% de prisioneiros;
  - as terras raras são o recurso escasso que limita o uso.

**Ashes of the Singularity: Escalation**
- Orbitais pagos em Quanta (1 por segundo por prédio).
- **"Toda vez que você chama um orbital, o custo daquela ação aumenta"**, e ainda tem recarga.
- Orbital Strike: o custo caiu de 500 pra 400 e o dano de 1000 pra 800. O valor exato do aumento por uso: n/c.

**Dune: Spice Wars** (wiki)
- **Ogiva:** 5000 Solari e 10 dias de fabricação.
- **Lançar custa −500 de prestígio político** no Landsraad.
- **Efeito:** 4000 de dano, vilas irradiadas por 20 dias e **base principal com −50% de armadura por 20 dias**.
- **O alvo aparece pra todas as facções.**

**Rise of Nations** (wiki)
- **Custo que sobe a cada bomba**, contando as já detonadas, até +125% da base:
  - Nuclear Missile: 500 Knowledge + 600 Oil, +50/+60 por bomba;
  - ICBM: 750 + 900, +75/+75.
- **Armageddon Clock:** a contagem começa em 4 + 1 por nação + 2 por time. **Chegou a zero, todos perdem.**
- **Embargo nuclear:** ~2 min sem mercado por bomba, crescendo a cada bomba seguinte, e vale pro time inteiro.
- **Missile Shield:** tecnologia cara que bloqueia mísseis no seu território, inclusive um já em voo.
- É o exemplo mais forte de **custo compartilhado**: o custo é o mundo.

**Dawn of War II: Retribution:** Orbital Bombardment custa **500 de recurso global**, exige nível 3 e recarrega em 120 s. O recurso vem de matar, e **quem perde unidades ganha 75% do valor delas**: um elástico pro lado que está perdendo.

**4X de referência (custo político e global):**
- **Civilization IV:**
  - o Manhattan Project, construído por **qualquer** civilização, libera nukes **pra todos** que tiverem a tecnologia;
  - o SDI intercepta ~75%: número de fórum e prévia, n/c no arquivo (https://civfanatics.com/?p=232).
- **Alpha Centauri:** usar o Planet Buster é "Major Atrocity". **Todas as facções declaram Vendetta e o jogador é expulso do Conselho Planetário** (https://alphacentauri2.info/wiki/Atrocity).

**Não levantados (n/c):** Broken Arrow (nenhuma nuke confirmada; só especulação de fã), Rise of Legends (custos dos superpoderes), C&C4, Empire Earth, WARNO, Stormgate, Petroglyph (Universe at War, Grey Goo).

### 2.3 Tabela-síntese

| Jogo | Prédio | Carga | **Custo por disparo** | Contra a base | Aviso | Contra-jogo |
|---|---|---|---|---|---|---|
| **ZH** | $5000 (50% do inicial) | 4–6 min | **0** | apaga 30–45 m de prédios comuns; o CC sobrevive | timer público + EVA + sirene | energia/EMP pausa; destruir; capturar |
| RA2/YR | $5000 | 10 min | 0 | concreto aguenta | timer + EVA (o Dominator não avisa) | energia pausa; espião zera |
| C&C3/KW | $5000 | 7 min | 0 (support powers: $300–4000) | mata tudo menos o ConYard | timer + EVA | energia; destruir |
| RA3 | $2500 (pós-patch) | 6 min | 0 (protocolos: pontos de combate) | ~50% do ConYard | timer + aviso | energia; destruir |
| Tempest Rising | 4000–5500 | 5–6 min | 0 (Drill Pod: 500) | — | — | máx. 1 por padrão |
| SupCom/FAF | 16,5 mil M | 300 s por míssil | **12 mil M por míssil** | apaga tudo no raio 30 | n/c | antinuke barato (1/3) |
| BAR | ~8 mil metal | 120–180 s | **1000–1500 metal** | comandante leva só 2500 | n/c | antinuke barato (1/7) |
| SC2 | Academy 150/50 | 43 s | **100/100** | 300/500 no raio 8 | voz + ponto vermelho | matar o Ghost aborta e perde a bomba |
| WiC | — | 450 s | **80 pontos de combate** | neutraliza pontos de comando | n/c | dispersar |
| CoH3 | — | 180 s | **200–250 de munição** | — | — | — |
| HW2 | — | 228–360 s | **até 1000 supply + 1000 power** | — | — | — |
| Act of War | $3000 | — | **$3000** (= prédio) | — | aviso é melhoria comprada | Patriot |
| AoA | ~3000 + raros | — | **$4000 + raros** | QG aguenta 3 | — | antimíssil com munição |
| Rise of Nations | — | 40 s | **custo crescente** | ICBM "destrói uma cidade" | — | escudo; relógio do Armagedom |
| Dune | — | 10 dias | **−500 de prestígio** | base −50% de armadura | alvo visível a todos | — |

### 2.4 Links (outros jogos)

**Série C&C**
- **RA2/YR:** https://github.com/moshowgame/ra2yuri · https://modenc2.markjfox.net/IsPowered · https://modenc2.markjfox.net/ShowTimer · https://github.com/Ares-Developers/Ares/blob/master/src/Misc/SWTypes/Nuke.cpp · https://cnc.fandom.com/wiki/Psychic_Dominator · https://cnc.fandom.com/wiki/Force_Shield
- **CnCNet:** https://forums.cncnet.org/topic/7420-question-yr-quick-match-map-pool/ · https://forums.cncnet.org/topic/9440-yuri-in-competitive-games/
- **TS:** https://github.com/CnCNet/cncnet-ts-client-package · https://cnc.fandom.com/wiki/Firestorm_generator
- **C&C3:** https://cnc.fandom.com/wiki/Ion_cannon_control_center · https://cnc.fandom.com/wiki/Temple_of_Nod_(Tiberium_Wars) · https://cnc.fandom.com/wiki/Rift_generator · https://cnc.fandom.com/wiki/Orca_strike · https://cnc.fandom.com/wiki/Cloaking_field · https://cnc.fandom.com/wiki/Catalyst_missile_(Tiberium_Wars) · https://forums.anandtech.com/threads/c-c-3-questions.2085034/
- **RA3:** https://cncnz.com/features/technical-support-help-guides/red-alert-3-v101-v112-patch-history/ · https://cnc.fandom.com/wiki/Proton_collider · https://cnc.fandom.com/wiki/Transcript:Support_Powers · https://www.bit-tech.net/reviews/gaming/command-and-conquer-red-alert-3/4/
- **Rivals:** https://cnc.fandom.com/wiki/Ion_cannon_(Rivals)
- **Tempest Rising:** https://support.saber.games/hc/en/tempest-rising/articles/december-2-2025-patch-notes-17 · https://tempestrising.wiki.gg/wiki/Drone_Strike_Station · https://tempestrising.wiki.gg/wiki/Excavator_Platform · https://steamcommunity.com/app/1486920/discussions/0/603029213292791958
- **TD Remastered:** https://steamcommunity.com/app/1213210/discussions/0/3819659084191925925
- **OpenRA:** https://github.com/OpenRA/OpenRA/blob/bleed/mods/ra/rules/structures.yaml · https://github.com/suhrm1/oraladder/blob/master/laddertools/srvwrap.py

**Outros jogos**
- **FAF:** https://github.com/FAForever/fa/tree/develop/units · https://github.com/FAForever/fa/blob/develop/lua/ui/lobby/UnitsRestrictions.lua · https://unitdb.faforever.com · https://forum.faforever.xyz/post/37817
- **BAR:** https://www.beyondallreason.info/unit/armsilo · https://www.beyondallreason.info/unit/corsilo · https://www.beyondallreason.info/unit/armamd · https://github.com/beyond-all-reason/Beyond-All-Reason/blob/master/modoptions.lua
- **SC2:** https://liquipedia.net/starcraft2/Tactical_Nuke · https://news.blizzard.com/en-us/article/5838580/game-guide-terran-nukes
- **WiC:** https://worldinconflict.fandom.com/wiki/Tactical_aid · https://worldinconflict.fandom.com/wiki/Tactical_nuke
- **CoH3:** https://github.com/cohstats/coh3-data
- **Halo Wars:** https://www.halopedia.org/Leader_power · https://www.halopedia.org/Atriox/Gameplay
- **Act of War / Act of Aggression:** https://actofwar.fandom.com/wiki/Wolverine_missile_silo · https://actofaggression.fandom.com/wiki/Superweapon · https://actofaggression.fandom.com/wiki/Peacekeeper
- **Ashes:** https://ashesofthesingularity.fandom.com/wiki/Post-Human_Coalition_global_abilities
- **Dune:** https://dunespicewars.fandom.com/wiki/Nuclear_Warheads
- **Rise of Nations:** https://riseofnations.fandom.com/wiki/Armageddon_Clock · https://riseofnations.fandom.com/wiki/Nuclear_Missile · https://riseofnations.fandom.com/wiki/Missile_Shield
- **Dawn of War II:** https://dawnofwar2.wiki.gg/wiki/Retribution/Orbital_Bombardment

---

## 3. Padrões de custo de lançamento

| Padrão | Quem usa | Prós | Contras |
|---|---|---|---|
| **(a) Só timer** (constrói e dispara de graça) | ZH, RA2, C&C3, RA3, TS, TD, Tempest Rising | Simples. O timer público cria uma contagem regressiva dramática pra todos. Combina com o tom "momento EVA" da série | Depois de construída é **renda grátis** pra sempre: reforça quem já está ganhando e premia quem se esconde. Várias juntas viram uma salva tripla. Disparar não exige decisão nenhuma |
| **(b) Munição fabricada e estocada** | SupCom, BAR, SC2, Act of War, AoA, Dune, RoN | Cada disparo vira decisão econômica. O estoque se vê e se pode explorar. Casa naturalmente com o **antimíssil que também gasta munição** e é mais barato por tiro (1/3 no SupCom, 1/7 no BAR): o atacante precisa acumular | Corrida de estoque vira contabilidade. A disputa míssil contra antimíssil pode congelar o fim da partida. Os lobbies de FAF e BAR têm opção pra desligar |
| **(c) Pontos ou recurso por uso**, mais recarga | WiC (pontos de combate), CoH3 (munição do território), HW1/HW2, DoW II, Ashes, support powers do C&C3 | Liga o uso ao **controle do mapa e ao combate**, não ao esconderijo. É fácil de balancear pelo preço (os patches do C&C3 subiram os preços) | Se o pagamento vem de abates, ajuda quem já está na frente. O DoW II compensa dando 75% a quem perdeu |
| **(d) Híbridos**: desbloqueio + custo por uso + recarga | CoH3, HW2, WiC, DoW II, C&C3 | **O padrão dos jogos recentes.** A recarga limita a frequência e o custo cobra cada uso | Mais uma regra pra ler na tela |
| **(e1) Custo que cresce** | Rise of Nations, Ashes | A primeira é acessível e a quinta dói. Freia o abuso sem proibir | Precisa mostrar o preço atual |
| **(e2) Custo político ou do mundo** | RoN (relógio do Armagedom, embargo), Dune (prestígio), Alpha Centauri (vendetta), Civ IV (o Manhattan libera pra todos) | **O mais memorável e o que tem mais identidade:** o custo é o mundo, não o bolso | Exige sistemas fora do combate (diplomacia, placar) |
| **(e3) Lançador exposto** | SC2 (o Ghost tem que sobreviver 14 s), Act of War (o aviso é comprado), Force Shield do YR (apaga a própria energia por 60 s) | O contra-jogo vira **ação no campo**: caçar o lançador, sair da área | Exige microgerenciamento no momento errado |
| **(e4) Energia por disparo ou por carga** | Mavor/Ragnarok (SupCom/BAR), Firestorm do TS, Force Shield do YR; o ZH pausa sem energia, mas não cobra | Usa um recurso que já existe nas facções com energia (USA e China no ZH) e cria uma janela de vulnerabilidade (falta de energia na base) | A GLA do ZH não tem energia; teria de ser outro custo |

---

## 4. Literatura de design

Pouca literatura trata superarma diretamente. **Não achei** palestra do GDC nem entrevista da EA LA ou da Westwood explicando o "grátis depois de construída" ou o preço dos support powers do C&C3 (n/c). O que existe, e serve:

- **Marc LeBlanc, "Feedback Systems and the Dramatic Structure of Competition" (GDC 1999).**
  - Realimentação positiva leva o jogo ao fim e **amplia a vantagem inicial**; a negativa estabiliza (resumo pelo artigo da DiGRA: https://dl.digra.org/index.php/dl/article/download/593/593).
  - **Uma superarma grátis depois de construída é realimentação positiva pura.** Custo por uso, custo crescente ou pagamento com território mexem no ganho desse laço.
- **Brandon Casteel, "The Importance of Control over Tactical Outcomes in RTS Design"** (Game Developer, 18/02/2016; https://www.gamedeveloper.com/design/the-importance-of-control-over-tactical-outcomes-in-rts-design).
  - O jogador precisa de liberdade "to succeed or fail" pelos próprios méritos e de jeito de **recuar e limitar perdas**.
  - Perdas binárias ("a single bad combat engagement can lose one player the game") frustram.
  - Pra superarma, isso significa: aviso e tempo de reação, e nenhuma salva que decida sozinha.
- **Callum McCole, "What Makes RTS Games Fun: Super Units"** (Wayward Strategy, 06/05/2019; https://waywardstrategy.com/2019/05/06/what-makes-rts-games-fun-super-units/).
  - Prefere supertropas a superarmas, porque as tropas têm contra e exigem investimento; a fala de Brandon Casteel citada vai no mesmo sentido (cap de unidades, risco).
  - O "trunfo" tem de ser **descobrível pelo prédio** que o produz.
  - **Anúncio global demais (C&C3) tira a necessidade de explorar.** Ele prefere a pista sonora do CoH.
  - "Super units should not be a no-brainer"; reconstruir a cada morte "is not interesting".
- **Blizzard, "Overwatch: The Elusive Goal: Play by Sound"** (GDC 2016, Scott Lawlor e Tomas Neumann; https://gdcvault.com/play/1023010/Overwatch-The-Elusive-Goal-Play; resumo em https://www.pcgamer.com/how-overwatch-uses-sound-to-pinpoint-threats-you-cant-see).
  - Os sons de habilidade do inimigo são distintos dos do aliado.
  - A mixagem destaca a maior ameaça.
  - **Legibilidade por som**: é o mesmo princípio do "Nuclear launch detected" e da sirene no alvo do ZH.
- **Amer Ajami (RA3), entrevista de 2008:** as superarmas **deliberadamente menos decisivas** que no RA2, uma camada de nuance no online (https://digitalchumps.com/2008/07/19/exclusive-interview-command-and-conquer-red-alert-3/3, via trecho da busca, n/c palavra por palavra). O patch 1.11 depois cortou o preço à metade por falta de uso.
- **Massive (WiC):**
  - a nuke pensada pra ser "muito difícil de obter mais de uma vez" por partida;
  - os pontos vêm do jogo em equipe e do combate;
  - Walfisz: criaram mais ajudas táticas porque eram "so much fun" (https://spong.com/feature/10109558/Interview-World-In-Conflict-Developer-Martin-Walfisz/4).
- **Chris Taylor (SupCom):** arma estratégica muito forte, **junto com** as ferramentas pra pará-la (antinuke). Visão de imprensa, não citação direta (https://www.gamespot.com/articles/supreme-commander-updated-qanda-nukes-scale-and-the-supreme-commander-itself/1100-6162843/).
- **Dustin Browder (lead designer do Generals), no código do ZH:** "Dustin wants these special powers to start ready to fire".
  - Os poderes de general (os menores) **nascem prontos**; as superarmas **não**: esperam a carga inteira depois de construídas.
  - Ou seja, o ZH separa o poder tático imediato do poder estratégico que se anuncia.
- **Elástico × ladeira escorregadia** (David Sirlin, "perpetual comeback", via fontes secundárias; https://critpoints.net/2017/04/01/comeback-mechanics-2/).
  - A compensação deve vir de **oportunidade e habilidade**, não de esmola.
  - Pra superarma: ela pode ser a **virada de quem está perdendo** se o custo dela for pago com algo que o perdedor ainda tem (território, risco, prestígio), não com mais renda.
- **Civ IV e Alpha Centauri (Firaxis):** o custo da arma total é **global e político**. O Manhattan libera pra todos; o Planet Buster vira vendetta de todos.

---

## 5. As lições

### 5.1 O que faz a superarma ser amada

1. **Aviso em três tempos, público.**
   - Construção revelada, timer na tela de todos, alerta no disparo com tempo de reação (ZH: o feixe leva 2,5 s até o chão; SC2: 14 s; WiC: 16 s; a sirene no alvo do ZH).
   - **É o "momento EVA" da série:** a contagem regressiva é o drama.
2. **O contra-jogo existe durante a carga, não só depois.**
   - Cortar energia ou EMP pausa (ZH, RA2, OpenRA, TD); espião zera (RA2, OpenRA); destruir ou capturar o prédio, que é revelado a todos.
   - Antimíssil que gasta munição e é mais barato por tiro (SupCom, BAR, Act of War, AoA).
3. **Destrói uma "camada" da base, não a partida.**
   - O coração sobrevive: CC do ZH com 44%; ConYard do RA3 com ~50%; QG do AoA aguenta 3; comandante do BAR leva 2.500; a base do Rivals cai com 2 nukes.
4. **Mirar é uma decisão.**
   - Escolher entre a camada econômica, a produção ou o exército parado. O feixe dirigido do ZH é o melhor exemplo: habilidade no disparo.
   - O resíduo (radiação 30 s, veneno 45 s) **nega terreno** de forma legível.
5. **Tem identidade de facção** (ZH: feixe cirúrgico, nuke em área, SCUD que espalha e envenena). No custo também: terras raras no AoA, prestígio no Dune, relógio no RoN.
6. **Custo de lançar que é escolha, não multa:** cada disparo compete com o exército (C&C3, Act of War, SupCom, CoH3).

### 5.2 O que faz a superarma ser odiada

1. **Disparo sem aviso** (o Psychic Dominator sem EVA é a mais criticada da série).
2. **Várias acumuladas disparando juntas** (ZH sem limite, RA2 antes do BuildLimit). É o motivo de existir o "Limit Superweapons" e o máximo de 1 do Tempest Rising.
3. **Grátis pra sempre:** renda infinita de destruição que premia quem se esconde e prolonga a partida perdida do outro (realimentação positiva, LeBlanc).
4. **Decidir a partida numa salva:** perda binária, sem recuo (Casteel). Por isso as versões de multiplayer são enfraquecidas (nuke do TD de 1000 pra 200; RA3 com preço à metade).
5. **Contra-jogo "só construir a defesa"** sem decisão nenhuma, ou impasse eterno de nuke contra antinuke.
6. **Efeito que cobre a leitura da batalha:** a tempestade do RA2 corta o radar de todos; fumaça ou clarão que esconde as unidades. **Pra nós isso é defeito (CLAUDE.md, jogabilidade antes).**
7. **No competitivo, "baixa o teto de habilidade"** (crítica ao Tempest Rising, RA3 "rarely used"). Mesmo assim, poucos ladders desligam por regra: o do OpenRA trava ligado e o GameReplays permite. Quem quer sem superarmas faz por acordo ou por opção da sala.

### 5.3 Faixas de números que funcionam

| Grandeza | Faixa observada | Referências |
|---|---|---|
| **Carga da superarma principal** | **4 a 7 min** (10 min é o teto, RA2/TD, sentido como lento) | ZH 4/5/6 · C&C3 7 · RA3 6 · Tempest 5–6 · RA2 10 |
| **Carga das "secundárias"** | **2 a 4 min** | ZH 2,5–6 (poderes de general) · RA3 3 · RA2 4–5 · C&C3 1–3,5 |
| **1º disparo** | só depois da carga inteira, que começa ao terminar o prédio | ZH (código) |
| **Custo de construir** | **25 a 50% do dinheiro inicial** | ZH, RA2 e C&C3: $5000 · RA3 cortou pra $2500 por desuso |
| **Custo por disparo** (quando existe) | entre **~10% e 100% do custo do prédio**; o meio comum fica em **30–60%**, ou **15–35% do dinheiro inicial** | Act of War: 100% · SupCom: ~73% · AoA Peacekeeper: o disparo ($4000 + 250 de raros) custa mais que o prédio ($1000 + 1000 de alumínio + 750 de raros), exceção cara · BAR: 12–20% · maiores support powers do C&C3: $3000–4000 com 7 min · CoH3: 200–250 de munição a cada 3 min |
| **Antimíssil** (quando existe) | interceptor a **1/3–1/7** do custo do míssil | SupCom 1/3 · BAR 1/7 |
| **Tempo entre o alerta de disparo e o impacto** | **2,5 a 16 s** | ZH feixe 2,5 s + sirene · SC2 14 s · WiC 16 s · Rivals Ion 3,3 s |
| **Raio** (escala do ZH) | **50–55 m** de área marcada; dano máximo em **12–16 m** | ZH: nuke 15,8/55,3 m · SCUD 13,2/52,7 m · feixe 11,6 m |
| **O que uma salva destrói** | **3 a 6 prédios comuns + tudo o que for unidade na área**; o coração (CC/ConYard) fica com 40–60%. Estimativa minha pela escala do ZH: **~15–30% do valor de uma base madura** (n/c) | ZH (§1.3) · C&C3 · RA3 · AoA (1/4 do QG) · BAR |
| **Resíduo / negação de área** | **30–45 s** | ZH: radiação 30 s, veneno 45 s · WiC: radiação |
| **Limite (o "LIMITED")** | **1 por jogador, somando todos os tipos** | ZH ("Limit Superweapons" = 1) · C&C3 · Tempest Rising (padrão) |

### 5.4 O que os dados sustentam pro nosso jogo (opções, a decisão é dele)

- **LIMITED = o do ZH:** 1 superarma por jogador, contando todos os tipos.
  - É o padrão do ZH, do C&C3 e do Tempest Rising, e a sala do jogo já tem a opção.
  - Variante possível: 1 por jogador **+ custo de lançar dobrado**. Não achei precedente; é ideia.
- **Custo de lançar por facção**, cada um com precedente real:
  - **USA (energia/satélite):** o disparo drena energia da base por um tempo (Force Shield do YR, Mavor/Ragnarok). A janela de vulnerabilidade é o custo, e cortar a energia continua sendo contra-jogo.
  - **China (míssil fabricado):** cada ogiva custa dinheiro e tempo pra fabricar e fica visível no silo (SupCom, BAR, Act of War, Dune). Abre espaço pro antimíssil de outra facção.
  - **GLA (sem energia):** dinheiro + material raro ou um custo crescente por salva (AoA com as terras raras do Cartel; RoN). Ou um lançador exposto que alguém tem de proteger (SC2).
  - **Rússia:** custo político ou de território (Dune, Alpha Centauri, WiC). Por exemplo, o disparo exige ou consome pontos de comando ou território.
- **Manter do ZH:** timer público, EVA nos três tempos, prédio revelado, energia/EMP pausando, coração da base sobrevivendo a uma salva e área legível vista de cima.
- **Configurável no futuro:** o custo de lançar como opção da sala (grátis / padrão / dobro), como a sala do ZH faz com o dinheiro inicial (5/10/20/50 mil).
