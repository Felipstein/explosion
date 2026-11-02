# O estado visual de cada peça (pra quem faz o som e o retrato do HUD)

Atualizado em 01/11/2026, 17h30, pelos vereditos do Felipe: a galeria do elenco (https://claude.ai/artifact/GGMzyq4iRM1EfMDMt82N9D,
avaliada às 15h20) e o índice `docs/APROVADOS.md`. A sessão do elenco atualiza este arquivo a cada veredito novo.

**A regra pra quem usa:**
- **APROVADO:** pode fazer o retrato do HUD e o som. O retrato parte da prévia/foto do modelo aprovado (`captures/game/models/<pasta>/preview/`).
- **NO RUMO:** o modelo vai mudar pouco (o ajuste está anotado). O som pode; o retrato espera o ajuste.
- **REFAZER / EM REFAÇÃO / NÃO AVALIADO:** não fazer o retrato ainda (o visual vai mudar ou ele não viu). O som pode, se a função não muda;
  quando a ideia muda (está escrito na linha), o som segue a ideia nova.
- **A Rússia inteira:** a paleta A (a de hoje) foi ESCOLHIDA em 01/11 19h: os retratos podem seguir ela. O emblema nos prédios virou o
  escudo tricolor (a estrela ficou com a China: `docs/elenco/EMBLEMAS.md`); a cor do time da infantaria russa ainda é a da China (a decidir).
- A ficha de cada peça (o som na §8, o prompt do HUD na §10) está em `docs/elenco/<facção>/<chave>.md`.

## USA

| Peça | Estado | Observação |
|---|---|---|
| Battle Tank (`usa_mbt`) | **APROVADO** | O tanque B (01/11: "ficou super bom"), com a frente que desce (o glacis, 01/11) |
| Dozer | **APROVADO** | A versão B |
| Supply Truck | **APROVADO** | A versão B |
| Stryker | **APROVADO** | A versão B (a ficha do Gemini aprovada em 25/10) |
| Linebacker (`usa_avenger`) | **APROVADO** | A versão B; ele notou um "probleminha na parte de baixo" (o casco parecido com o do tanque, item 41): pode mudar um pouco |
| Tomahawk Launcher | **APROVADO** | A versão B; os mísseis do Tomahawk aprovados ("perfeitos") |
| Javelin | **APROVADO** | |
| F-22 Raptor | **APROVADO** | A versão B |
| AH-64 Apache (`usa_comanche`) | **APROVADO** | A versão B |
| CH-47 Chinook | **APROVADO** | A versão B |
| Avião de carga e a caixa do lançamento | **APROVADO** | As versões B |
| Patriot Missile System | **APROVADO** | A versão B |
| Supply Drop Zone | **APROVADO** | A versão B |
| Airfield | **APROVADO** | A versão B |
| War Factory | **A ficha APROVADA; o modelo B NÃO AVALIADO** | A arte aprovada é a War Factory do `r1_raid_slabs` e a ficha `docs/concept/buildings/usa_war_factory_sheet.webp`; o modelo B entrou no jogo em 01/11 e ele ainda não viu |
| Supply Center | **NÃO AVALIADO** | O modelo B (o heliponto no teto) entrou no jogo em 01/11 |
| Barracks | **NÃO AVALIADO** | O modelo B (o hangar arredondado com os sacos de areia) entrou no jogo em 01/11 |
| Command Center | **EM REFAÇÃO** | A e B reprovados ("muito básico"). O novo é **o Pentágono** (`docs/elenco/USA_CC_E_USINA.md`), ainda sem a palavra dele |
| Cold Fusion Reactor (a usina) | **EM REFAÇÃO** | A e B reprovados ("não tem cara de power plant"). A nova é **a subestação com a torre de alta tensão e o microrreator** (o som muda: o zumbido do transformador e o estalo elétrico, não o reator) |
| Strategy Center | **EM REFAÇÃO** | A e B reprovados ("sem criatividade"). O novo é **o campo de radomos** (`docs/elenco/USA_STRATEGY_CENTER.md`) |
| Paladin | **EM REFAÇÃO** | Reprovado ("igual ao Abrams"). O novo pela regra de `docs/elenco/SILHUETAS_USA.md` (o maior de lagarta, o laser grande) |
| Ranger (a infantaria) | **REPROVADO** (30/10) | O soldado provisório ("quadrado e sem detalhe"); a infantaria é da sessão da infantaria |
| Cruise Missile (o poder) | Efeito, não modelo | Os mísseis e a chegada aprovados em 31/10 |

## China

| Peça | Estado | Observação |
|---|---|---|
| Command Center, Supply Center, Barracks, War Factory | **APROVADO** | |
| Dozer, Supply Truck, Battlemaster (`china_type99`), Gattling Tank, Inferno Cannon, Overlord | **APROVADO** | |
| Red Guard (`china_rifleman`), Tank Hunter | **APROVADO** | |
| Nuclear Reactor (a usina) | **NO RUMO** | "no topo das usinas, por dentro, ficou vazando" (a boca das torres de resfriamento) |
| Propaganda Center | **REFAZER** | |
| Gattling Cannon | **REFAZER** | "parece um bunker [...] o bagulho é redondo" (sem a laje quadrada; a ideia muda) |

## GLA

| Peça | Estado | Observação |
|---|---|---|
| Command Center, Arms Dealer | **APROVADO** | |
| Supply Pickup, Quad Cannon, Toxin Tractor, Rocket Buggy | **APROVADO** | |
| Rebel, RPG Trooper | **APROVADO** | |
| Scorpion (`gla_tank`) | **APROVADO** (13/10) | O tanque da GLA ("top, salva, commita") |
| Technical | **A ficha e a picape 1 APROVADAS** | O modelo final é o que ele faz no Blender; o do jogo é provisório |
| Worker | **NO RUMO** | "parece que ele tá segurando uma arma" |
| Supply Stash, Barracks, Palace, Stinger Site | **REFAZER** | O Palace: "sai um pouco dessa de tudo ser murado assim" |

## Rússia (a paleta A escolhida em 01/11 19h)

| Peça | Estado | Observação |
|---|---|---|
| Gas Power Plant, Supply Depot, Tank Plant, Research Institute, Pantsir Site | **APROVADO** (a forma) | O emblema muda (a estrela sai, entra a águia) |
| IMR-3M, KamAZ, T-90M, Pantsir-S1, TOS-1A | **APROVADO** (a forma) | |
| Motostrelok, Kornet Team | **APROVADO** (a forma) | A farda própria é da sessão da infantaria |
| Barracks (a Kazarma) | **NO RUMO** | "mude algumas coisas, parece um prédio normal" |
| Command Center (o Shtab) | **REFAZER** | "parece uma prisão" |
| BMPT Terminator, T-14 Armata | **REFAZER** | "muito parecido com o outro tanque" (o T-90M) |
