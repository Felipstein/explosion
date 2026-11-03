# Poderes do general: como os outros RTS fazem (estudo de 03/11/2026)

Pesquisa do mercado fora o Zero Hour (o ZH está em `docs/poderes/ESTUDO_ZH.md`). Serve de base pra `docs/PODERES_DO_GENERAL.md`.
Nomes de jogo e de poder em inglês. **"n/c" = não confirmado.** Cada número tem o link da fonte. As wikis de dados
(fandom, halopedia, wiki.gg, os dados extraídos do CoH3) são a fonte principal. Opinião de jogador vem de fórum e é marcada assim.
Contas feitas por nós aparecem como "soma nossa".

---

## Resumo (10 linhas)

1. **Escolher e usar são coisas separadas** em quase todos: o ponto destrava o poder uma vez, e cada uso paga recarga (às vezes também dinheiro ou munição). É assim no RA3, no BFME, no CoH, no Halo Wars 2 e no Tempest Rising.
2. **Os pontos vêm do combate**, mas os sistemas que melhor seguram a bola de neve **também pagam quem perde**. No RA3, perder uma unidade dá 300% do preço em "ameaça", e matar dá 75%. No DoW2, o dono do modelo morto leva 75% do valor. No WiC, os pontos também vêm de capturar, fortificar e consertar.
3. **Escolha que exclui dá identidade.** No CoH, o jogador escolhe 1 de 3 doutrinas. No AoM, escolhe 1 de 2 deuses por era. Quando dá pra comprar tudo numa partida longa (Halo Wars 2, e o BFME2 com 150 pontos por livro), a escolha se perde. Os nossos 9 pontos, com poderes de 1 a 3, já obrigam a escolher.
4. **Os níveis I–III aumentam a força e também a recarga.** RA3: Time Bomb 120 → 240 → 300 s; Final Squadron 120 → 240 → 360 s. Halo Wars 2: o nível sobe o custo e a recarga fica igual.
5. **As recargas do mercado:**
   - reconhecimento e fumaça: 20–60 s;
   - ataque médio: 90–180 s;
   - ataque grande: 300–450 s;
   - o máximo: 360–600 s, e no SC2 ele já nasce em recarga (Death Fleet e Odin: 600 s).
6. **O inimigo precisa ver o golpe chegando e ter como reagir:**
   - atraso de 3 a 25 s;
   - sinalizador no chão (a fumaça vermelha do CoH);
   - som que todos ouvem (C&C3);
   - bomba ou avião que pode ser derrubado: a Time Bomb tem 500 HP, e quem derruba a Final Squadron ganha XP;
   - radar de alerta (Act of War).
7. **O poder máximo precisa de atraso, recarga longa e um efeito que permita a virada.** Exemplos: o BFME2 custa 25 pontos; no WiC a nuke custa 80 TA, recarrega em 450 s e neutraliza os pontos de comando; no DoW2 o Orbital custa 500.
8. **Os erros mais conhecidos:**
   - poder que todo mundo pega: Rallying Call "MUST" no BFME2; no CoH3, "pega o que funciona";
   - poder liberado fora da partida: o CoH2 virou "pay to win"; o DoW3 travava doutrinas atrás de Skulls; o C&C4, atrás da patente da conta;
   - poder confuso: o beta do HW2;
   - uso único que ninguém gasta: dito pela designer do AoM;
   - superpoder forte demais: o DoW3 subiu a recarga de 5 para 7,5 min.
9. **Em time funciona:**
   - doar pontos ao aliado (WiC);
   - bônus pequeno que respinga no aliado: no RA3, 25% pro dono e 5% pro time;
   - preço diferente por papel (WiC).
10. **Pra nós:**
    - manter a recarga por uso;
    - um poder máximo de 3 pontos por general, com atraso e aviso;
    - dar mais XP a quem perde e a quem defende do que hoje;
    - todo poder visível de cima sem cobrir a luta;
    - nada liberado fora da partida.

---

## Tabela-mãe: o mercado lado a lado

| Jogo | De onde vêm os pontos | O que o ponto compra | Custo por uso | Recarga | Níveis | Dá pra ter tudo? | Excludente? |
|---|---|---|---|---|---|---|---|
| BFME1 | matar ([review](https://scifibloggers.com/lord-of-the-rings-battle-for-middle-earth-ii-review/)) | árvore com pré-requisitos | nenhum ([gamicus](https://gamicus.fandom.com/wiki/The_Lord_of_the_Rings:_The_Battle_for_Middle-earth)) | sim | não | sim, 32–44 PP ([wiki](https://bfme.fandom.com/wiki/Powers)) | não |
| BFME2 / RotWK | matar | livro de 4 fileiras (5/10/15/25 PP) | nenhum | sim | não | 150 PP por facção (soma nossa) | não |
| RA3 / Uprising | combate, acelerado pela ameaça ([wiki](https://cnc.fandom.com/wiki/Threat_Meter)) | 15 protocolos de 1 ponto, em 3 colunas ([wiki](https://cnc.fandom.com/wiki/Top_Secret_Protocol)) | nenhum | 120–360 s | sim, I–III | n/c | não; ordem dentro da coluna |
| C&C3 / KW | (não há pontos) | prédio libera | crédito: $300–$4000 | 3–7 min | não | sim | pela subfacção (KW) |
| C&C4 | Support Points: tempo, buff/debuff, dano ([wiki](https://cnc.fandom.com/wiki/Resources_(Tiberian_Twilight))) | patente da conta (fora da partida) | 40–50 SP | n/c | não | só a classe Support | pela classe |
| WiC | matar, capturar (+5), fortificar (+5), consertar ([wiki](https://worldinconflict.fandom.com/wiki/Command_point)) | (não compra: paga cada uso) | 5–180 TA | 30–450 s | 1×/2×/3× | — | não |
| CoH1 | XP de combate | nó da doutrina (1–5 CP) | munição/mão de obra | 20–180 s | não | 12–16 CP a árvore | 1 de 3 doutrinas |
| CoH2 | XP de combate | o limiar de CP libera sozinho | munição/mão de obra | n/c | não | sim, no comandante escolhido | 1 de 3 comandantes |
| CoH3 | XP de combate | nó do battlegroup (0–10 CP) | munição/mão de obra | 60–180 s | não | não (pares a/b) | 1 battlegroup; nós a/b |
| Halo Wars 1 | (não há pontos) | Field Armory com recursos | 350–1400 recursos | n/c | I–IV | sim | não |
| Halo Wars 2 | XP de tudo, mais em combate | 1 ponto = destrava ou sobe | supply + power | 105–360 s | I–III | sim, em partida longa ([halopedia](https://halopedia.org/Leader_power)) | não (5 faixas de 2) |
| SC2 co-op | nível da conta | já vem | energia/recurso ou nada | 30–600 s | não | sim | só o Zeratul |
| Tempest Rising | crédito + tempo; Intel (GDF) | doutrina (18, até 11 ativas) | crédito (+Intel) | 1:30–3:00 | 6 tiers | não (11 de 18) | ordem na árvore |
| DoW2 | "red": matar (75% pro dono do morto) | (não compra: paga cada uso) | 25–500 | 50–420 s | por tier | — | não |
| DoW3 | Elite Points: tempo e captura | escolha antes da partida | Elite Points | 5 → 7,5 min | não | 1 super por raça | 6 doutrinas antes |
| AoM / Retold | (não há pontos) | o deus menor de cada era | Retold: 1º uso grátis, depois favor crescente | 90–300 s | não | não | 1 de 2 por era |

As fontes de cada linha estão nas seções abaixo.

---

## 1. Battle for Middle-earth 1, 2 e Rise of the Witch-king

### Como funciona

- **Os pontos (Power Points, PP) vêm de matar.** "As you kill enemy units you receive power points" ([review](https://scifibloggers.com/lord-of-the-rings-battle-for-middle-earth-ii-review/)).
- **No BFME1:**
  - cada facção tem a própria árvore, com pré-requisitos ("fulfill any one");
  - o poder comprado se usa à vontade, só com recarga e sem custo ([gamicus](https://gamicus.fandom.com/wiki/The_Lord_of_the_Rings:_The_Battle_for_Middle-earth));
  - as duas facções de um lado compartilham os mesmos poderes (mesma fonte).
- **No BFME2 o livro tem dois lados:**
  - Forces of Light ([heavengames](https://bfme2.heavengames.com/factions/powers-fol/));
  - Forces of Darkness ([heavengames](https://bfme2.heavengames.com/factions/powers-fod/));
  - cada facção pega 12 poderes do seu lado, alguns exclusivos dela;
  - são 4 fileiras, de 5, 10, 15 e 25 PP.
- **A conta (soma nossa, pelas listas):** 3 poderes de 5 + 4 de 10 + 3 de 15 + 2 de 25 = **150 PP** pra ter o livro inteiro. Vale para Goblins e Men of the West.
- **RotWK:** o Angmar recebe o seu próprio livro, também em 4 tiers ([wiki](https://bfme.fandom.com/wiki/Angmar_(faction))). Os custos por tier são n/c.
- **Teto de pontos e ritmo:** n/c. Os pré-requisitos do livro do BFME2 também são n/c.

### Exemplos

| Poder | Jogo | Custo | Efeito | Fonte |
|---|---|---|---|---|
| Heal / Elven Wood | BFME1 Gondor | 1 PP | cura; terreno que dá +40% de armadura | [wiki](https://bfme.fandom.com/wiki/Powers) |
| Summon Army of the Dead | BFME1 Gondor | 10 PP | invocação | idem |
| Summon Balrog | BFME1 Mordor | 20 PP | invocação controlável | idem |
| Total da árvore | BFME1 | 32 (Rohan) a 44 (Mordor) PP | — | idem |
| Rallying Call, Heal, Rebuild | BFME2 Light | 5 PP | buff de 50%; cura; conserta prédios | [heavengames](https://bfme2.heavengames.com/factions/powers-fol/) |
| Arrow Volley, Lone Tower, Tom Bombadil | BFME2 Light | 10 PP | ataque; torre; herói temporário | idem |
| Cloud Break, Summon Eagles | BFME2 Light | 15 PP | atordoa e cancela Darkness | idem |
| Army of the Dead, Earthquake, Flood, Sun Flare, Citadel | BFME2 Light | 25 PP | os poderes máximos | idem |
| Darkness, Awaken Wyrm | BFME2 Dark | 15 PP | o mapa escurece e o time fica +50% | idem, [wiki](https://bfme.fandom.com/wiki/Powers) |
| Summon Balrog, Dragon Strike, Rain of Fire | BFME2 Dark | 25 PP | os poderes máximos | [heavengames](https://bfme2.heavengames.com/factions/powers-fod/) |

### Poderes que mudam o mapa
- **Elven Wood e Tainted Land** pintam o chão:
  - os aliados ganham +40% de armadura;
  - o inimigo perde a liderança ([wiki](https://bfme.fandom.com/wiki/Powers)).
- **Devastation:** as árvores viram recurso.
- **Darkness:** escurece o mapa inteiro.
- **Flood:** apaga o fogo ([heavengames](https://bfme2.heavengames.com/factions/powers-fol/)).

### Contra-jogo
- Cloud Break cancela Darkness e Freezing Rain ([wiki](https://bfme.fandom.com/wiki/Powers)). É um poder que responde a outro.
- Aviso antes dos 25 PP: n/c.

### O que acharam (jogadores)
- Guia do RotWK 2.01:
  - "Rally Call is a MUST!";
  - Flood é "the better of the 25 cost PP" ([fórum](https://bfme2.heavengames.com/cgi-bin/forums/display.cgi?action=st&fn=6&tn=622)).
- Uma resposta no mesmo tópico: começar "ALWAYS" com Rallying Call (mesmo link).
- Isso é escolha obrigatória: um poder de 1ª fileira dominou a abertura.

---

## 2. Red Alert 3 e Uprising (Top Secret Protocols)

### Como funciona

- **Os pontos (Security Points) vêm com o tempo de batalha.** O ritmo sobe com a ameaça ([Threat Meter](https://cnc.fandom.com/wiki/Threat_Meter)).
- **A ameaça:**
  - matar dá 75% do preço da unidade morta;
  - **perder dá 300% do preço da unidade perdida**;
  - a ameaça cai sozinha: 50, 150 ou 300 pontos por segundo, conforme o nível.

| Nível de ameaça | Pontos de ameaça | Bônus no ganho de Security Points | Bônus de veterania | Fonte |
|---|---|---|---|---|
| Amarelo (1×) | 0–12 499 | nenhum | nenhum | [wiki](https://cnc.fandom.com/wiki/Threat_Meter) |
| Laranja (2×) | 12 500–37 499 | +50% | +50% | idem |
| Vermelho (3×) | 37 500–50 000 | +100% | +100% | idem |

- **A compra:**
  - cada protocolo custa 1 ponto;
  - o tutorial diz que há "like 15" por facção e que, pra pegar um mais abaixo, é preciso ter os de cima ([tutorial](https://cnc.fandom.com/wiki/Transcript:Support_Powers));
  - as 3 colunas têm 5 casas cada, e um poder com níveis I–III ocupa 3 casas ([lista](https://cnc.fandom.com/wiki/Top_Secret_Protocol)).
- **O Construction Yard é obrigatório** pra usar os protocolos (mesma fonte). Perder a base tira os poderes.
- **Três tipos de protocolo** (tutorial):
  - ataque: Surgical Strike;
  - tático: Chrono Rift;
  - melhoria passiva: Advanced Aeronautics, que vale na hora.
- **Teto de pontos na partida:** n/c. Um mod diz que no original dá pra pegar só 10 dos 15 ([moddb](https://www.moddb.com/mods/red-alert-3-revolution/features/red-alert-3-revolution-change-list)), fonte fraca.

### Exemplos com números

| Protocolo (facção) | Nível I → II → III | Recarga I → II → III | Atraso | Fonte |
|---|---|---|---|---|
| Time Bomb (Allies) | 750 → 1000 → 1500 de dano; a bomba tem 500 → 1000 → 2000 HP | 120 → 240 → 300 s | pavio de 5 → 10 → 20 s | [wiki](https://cnc.fandom.com/wiki/Time_Bomb) |
| Orbital Drop (Soviet) | 3 sondas de 300 → 2 satélites de 500 → estação de 1500; raio 75 → 125 → 175 | 180 → 240 → 300 s | 3 → 6 → 9 s | [wiki](https://cnc.fandom.com/wiki/Orbital_Drop) |
| Magnetic Satellite (Soviet) | raio 30 → 40 → 50; puxa veículos leves → médios → todos | 120 → 150 → 180 s | 3 s | [wiki](https://cnc.fandom.com/wiki/Magnetic_Satellite) |
| Final Squadron (Empire) | 5 → 10 → 15 drones; raio 100 → 150 → 200 | 120 → 240 → 360 s | — | [wiki](https://cnc.fandom.com/wiki/Final_Squadron) |
| Balloon Bombs (Empire) | 5 → 10 → 15 bombas | ~3:15 → ~4:40 → ~5:35 | — | [wiki](https://cnc.fandom.com/wiki/Balloon_Bombs) |
| Desolator Airstrike (Soviet) | 1 → 2 → 3 Badgers; 300 de dano em raio 150 por 30 s; esvazia prédio ocupado | 180 s | 7 s | [wiki](https://cnc.fandom.com/wiki/Desolator_Airstrike) |
| Magnetic Singularity (Soviet) | raio 100 por 10 s; mata infantaria | 120 s | 3 s | [wiki](https://cnc.fandom.com/wiki/Magnetic_Singularity) |
| Free Trade (Allies) | +25% de minério pro dono e +5% pros aliados; acumula até 35% | passivo | — | [wiki](https://cnc.fandom.com/wiki/Free_Trade) |
| Mass Production (Soviet) | −25% no preço pro dono e −5% pros aliados | passivo | — | [wiki](https://cnc.fandom.com/wiki/Mass_Production) |

### Contra-jogo
- **A Time Bomb pode ser destruída antes de explodir.** Destruída, ela causa menos dano ([lista](https://cnc.fandom.com/wiki/Top_Secret_Protocol)).
- **Os paraquedistas** (Paratroop, campanha) ficam vulneráveis ao antiaéreo enquanto descem (mesma fonte).
- **Balloon Bombs:** o antiaéreo estoura o paraquedas. A bomba ainda cai, mas desgovernada ([wiki](https://cnc.fandom.com/wiki/Balloon_Bombs)).
- **Quem derruba os drones da Final Squadron ganha 2000 de XP** ([wiki](https://cnc.fandom.com/wiki/Final_Squadron)). O contra-jogo paga.

### O que acharam
- Um verbete de fãs lista a Time Bomb, a Magnetic Satellite, o Orbital Drop e a Final Squadron como "devastating, difficult to counter" ([TV Tropes](https://tvtropes.org/pmwiki/pmwiki.php/YMMV/CommandAndConquerRedAlert3)).
- Bastidores: o botão "Red Alert" era pra estar no RA3 original. Ele seria um último recurso: parava a construção, danificava os prédios devagar e barateava as unidades ([wiki](https://cnc.fandom.com/wiki/Threat_Meter)). Acabou só no Commander's Challenge.
- Entrevista de designer sobre os protocolos: n/c.

---

## 3. C&C 3, Kane's Wrath e C&C 4

### C&C3 / Kane's Wrath: como funciona
- **Não há pontos.** O poder vem do prédio e é pago em crédito a cada uso, com recarga longa ([Support power](https://cnc.fandom.com/wiki/Support_power)).
- **Quem captura o prédio ganha o poder.** Exemplo: capturar o Command Post da GDI dá o Radar Scan ([Radar scan](https://cnc.fandom.com/wiki/Radar_scan)).
- **No KW**, cada subfacção tem um conjunto próprio de poderes (as páginas listam a subfacção). Os detalhes por subfacção são n/c.

| Poder (facção) | Prédio | Custo por uso | Recarga | Fonte |
|---|---|---|---|---|
| Radar Scan (GDI) | Command Post | $300 (patch 1.05: +20%) | n/c | [wiki](https://cnc.fandom.com/wiki/Radar_scan) |
| Orca Strike (GDI) | Airfield | $500 | n/c | [wiki](https://cnc.fandom.com/wiki/Orca_strike) |
| Shockwave Artillery (GDI) | Space Command Uplink | $2000 | 3:00 | [wiki](https://cnc.fandom.com/wiki/Shockwave_artillery) |
| Orbital Strike (KW) | Space Command Uplink | $3000 (KW 1.02) | n/c | [wiki](https://cnc.fandom.com/wiki/Orbital_strike) |
| Decoy Army (Nod) | Operations Center | $500 | n/c | [wiki](https://cnc.fandom.com/wiki/Decoy_army) |
| Radar Jamming Missile (Nod) | Operations Center | $750 (TW 1.09) → $300 (KW 1.02) | n/c | [wiki](https://cnc.fandom.com/wiki/Radar_jamming_missile) |
| Cloaking Field (Nod) | Operations Center | $3000 (subiu 100%) | 7:00 | [wiki](https://cnc.fandom.com/wiki/Cloaking_field) |
| Catalyst Missile (Nod) | Tiberium Chemical Plant | $2000 (1.09: +33%) | 5:00 | [wiki](https://cnc.fandom.com/wiki/Catalyst_missile_(Tiberium_Wars)) |
| Tiberium Vapor Bomb (Nod) | Tiberium Chemical Plant | $3500 | 300 s (subiu 25%) | [wiki](https://cnc.fandom.com/wiki/Tiberium_vapor_bomb) |
| Ichor Seed (Scrin, KW) | Nerve Center | $1000 | n/c | [wiki](https://cnc.fandom.com/wiki/Ichor_seed) |
| Overlord's Wrath (Scrin) | Signal Transmitter | $4000 | 7:00 | [wiki](https://cnc.fandom.com/wiki/Overlord%27s_Wrath) |

- **O dinheiro é a alavanca de balanço.** Os patches mexeram quase só no preço: Catalyst +33%, Cloaking +100%, Jamming de $750 pra $300 (tabela acima).
- **Contra-jogo:** o lançamento da Shockwave Artillery "can be heard by all commanders" ([wiki](https://cnc.fandom.com/wiki/Shockwave_artillery)). É um aviso sonoro pra todos.
- **Poder que muda o mapa:** o asteroide do Overlord's Wrath deixa uma mancha de Tiberium que vale $4645 se colhida logo. Qualquer lado pode colher, e o lugar fica marcado no chão ([wiki](https://cnc.fandom.com/wiki/Overlord%27s_Wrath)).
- **Leitura de design (nossa):** pagar em crédito liga o poder à economia. Quem está ganhando paga mais fácil, e isso puxa a bola de neve.

### C&C4 (Tiberian Twilight)
- **Os Support Points enchem de três jeitos** ([wiki](https://cnc.fandom.com/wiki/Resources_(Tiberian_Twilight))):
  - com o tempo;
  - dando buff no aliado ou debuff no inimigo;
  - causando dano.
- **Só a classe Support tem poderes**, 15 por lado ([wiki](https://cnc.fandom.com/wiki/Support_Class)).
- **Os poderes destravam pela patente da conta, fora da partida.** Exemplos:
  - Sonic Artillery: 40 SP, rank 17 ([wiki](https://cnc.fandom.com/wiki/Sonic_artillery));
  - Seismic Disturbance: 50 SP, rank 20 ([wiki](https://cnc.fandom.com/wiki/Seismic_disturbance)).
- **Opinião específica sobre os poderes:** n/c. O jogo inteiro foi mal recebido.
- Fica como exemplo do que evitar: poder liberado fora da partida.

---

## 4. World in Conflict (Tactical Aid)

### Como funciona
- **Os pontos de Tactical Aid (TA) vêm de** ([TA](https://worldinconflict.fandom.com/wiki/Tactical_Aid), [Command point](https://worldinconflict.fandom.com/wiki/Command_point)):
  - matar;
  - capturar ponto de comando: +5 pra cada jogador que participou;
  - cada fortificação construída: +5 pro time;
  - consertar veículos.
- **Não se compra o poder: paga-se cada uso.** Cada poder sai em 1×, 2× ou 3× de uma vez, por um preço crescente ([TA](https://worldinconflict.fandom.com/wiki/Tactical_Aid)).
- **O jogador pode doar TA pro aliado** ([review](https://www.gamespot.com/world-in-conflict/user-reviews/2200-501833/)).
- **O preço muda pelo papel** (Infantry, Support, Armor, Air). O Support paga menos pra consertar ponte e pra bomba guiada (tabela abaixo).
- **A unidade perdida devolve o ponto de reforço aos poucos** ([Wikipedia](https://en.wikipedia.org/wiki/World_in_Conflict)). Perder o exército não acaba com o jogo.

### Os preços (1× / 2× / 3×, em TA)

| Poder | Tipo | Custo | Recarga | Chegada | Fonte |
|---|---|---|---|---|---|
| Light Artillery Barrage | indiscriminado | 5/10/15 | 30 s | 10 s | [wiki](https://worldinconflict.fandom.com/wiki/Light_artillery_barrage) |
| Aerial Recon | não destrutivo | 5–8 (pelo papel) | 30 s | 15 s | [wiki](https://worldinconflict.fandom.com/wiki/Aerial_recon) |
| Airborne Infantry | invocação | 5–10 | 30 s | 25 s | [wiki](https://worldinconflict.fandom.com/wiki/Airborne_infantry) |
| Airdropped Light Tank | invocação | 6–10 | 30 s | 25 s | [wiki](https://worldinconflict.fandom.com/wiki/Airdropped_light_tank) |
| Tank Buster | seletivo, faixa | 6/12/18 | 90 s | 12 s | [wiki](https://worldinconflict.fandom.com/wiki/Tank_buster) |
| Napalm Strike | seletivo, faixa; queima mata | 8/16/24 | 90 s | 20 s | [wiki](https://worldinconflict.fandom.com/wiki/Napalm_strike) |
| Laser-Guided Bomb | contra prédio e ponte | 8 (Support) a 10 | 30 s | 11 s | [wiki](https://worldinconflict.fandom.com/wiki/Laser-guided_bomb) |
| Repair Bridge | utilidade | 10 (Support) a 15 | 30 s | 15 s | [wiki](https://worldinconflict.fandom.com/wiki/Repair_bridge) |
| Air Strike | indiscriminado | 10/18/26 | 30 s | 20 s | [wiki](https://worldinconflict.fandom.com/wiki/Air_strike) |
| Chemical Strike | contra infantaria em prédio | 12/22/32 | 30 s | 15 s | [wiki](https://worldinconflict.fandom.com/wiki/Chemical_strike) |
| Heavy Artillery | indiscriminado | 14/26/38 | 30 s | 12 s | [wiki](https://worldinconflict.fandom.com/wiki/Heavy_artillery_barrage) |
| Precision Artillery | indiscriminado, área pequena | 15/30/45 | 30 s | 10 s | [wiki](https://worldinconflict.fandom.com/wiki/Precision_artillery) |
| Heavy Air Support | seletivo, 3 ondas | 25/45/65 | 30 s | 15 s | [wiki](https://worldinconflict.fandom.com/wiki/Heavy_air_support) |
| Daisy Cutter / Fuel Air Bomb | indiscriminado; derruba mata | 30/50/70 | 30 s | 18 s | [wiki](https://worldinconflict.fandom.com/wiki/Daisy_Cutter), [wiki](https://worldinconflict.fandom.com/wiki/Fuel_air_bomb) |
| Carpet Bombing | indiscriminado, faixa longa | 45/75/95 | 90 s | 15 s | [wiki](https://worldinconflict.fandom.com/wiki/Carpet_bombing) |
| Tactical Nuke | tudo; deixa radiação de 20 HP/s | 80/140/180 | 450 s | 16 s | [wiki](https://worldinconflict.fandom.com/wiki/Tactical_nuke) |

### Mapa e virada
- **Napalm e Daisy Cutter limpam a mata** e expõem a infantaria escondida ([TA](https://worldinconflict.fandom.com/wiki/Tactical_Aid)).
- **A ponte:** a LGB derruba com 2 acertos, e o Repair Bridge reconstrói (mesma fonte).
- **A nuke deixa neutros os pontos de comando da área.** A wiki diz que ela serve pra frear o avanço e "enable a turnaround" (mesma fonte).

### Contra-jogo
- Todo TA chega com atraso, de 10 a 25 s. Dá tempo de o inimigo sair da área. O napalm, por exemplo, "takes rather long to arrive" (mesma fonte).
- **Heavy Air Support:** a 3ª onda não ataca o ar, e prédio e relevo bloqueiam os mísseis (mesma fonte).
- **O Tank Buster acerta aliado também** (mesma fonte).
- Se o avião do TA pode ser derrubado, e se o inimigo vê a marca antes do golpe: n/c.

### O que acharam
- Imprensa e jogadores falam bem de juntar pontos com o time pra nuke e de o inimigo ficar exposto depois de gastar tudo nela ([review](https://www.gamespot.com/world-in-conflict/user-reviews/2200-501833/)).
- Entrevista de designer sobre o TA: n/c.

---

## 5. Company of Heroes 1, 2 e 3

### CoH1 (doutrinas)
- **A escolha:** no começo, 1 de 3 doutrinas, sem troca ([wiki](https://companyofheroes.fandom.com/wiki/Company_Commander)).
- **Os pontos:** a XP de combate vira Command Points (CP).
- **A árvore:** cada doutrina tem 2 ramos de 3 nós. O CP destrava o nó uma vez. A árvore inteira custa de 12 a 16 CP (soma do levantamento).
- **O uso:** paga munição ou mão de obra (MP), mais recarga.

| Poder (doutrina) | CP | Por uso | Recarga | Fonte |
|---|---|---|---|---|
| P-47 Recon (Airborne) | 1 | 50 mun | 20 s | [wiki](https://companyofheroes.fandom.com/wiki/Airborne_Company) |
| Paratroopers (Airborne) | 2 | 375 MP | 40 s | idem |
| Strafing Run, faixa de 40×10 m (Airborne) | 2 | 150 mun | 45 s | idem |
| Supply Drop (Airborne) | 2 | 100 MP | 180 s | idem |
| P-47 Rocket Strike (Airborne) | 3 | 250 mun | 45 s | idem |
| Off-map Artillery, 6 tiros (Infantry) | 2 | 150 mun | n/c | [wiki](https://companyofheroes.fandom.com/wiki/Infantry_Company) |
| Registered Artillery (Defensive) | 2 | 125 mun | 35 s | [wiki](https://companyofheroes.fandom.com/wiki/Defensive_Doctrine) |
| Pershing, 1 em campo (Armor) | 5 | 800 MP | até ele morrer | [wiki](https://companyofheroes.fandom.com/wiki/Armor_Company) |
| Tiger (Blitzkrieg) | 5 | 800 MP | n/c | [wiki](https://companyofheroes.fandom.com/wiki/Blitzkrieg_Doctrine) |

**Contra-jogo do CoH1**
- **Aviso antes do golpe:**
  - a artilharia joga 3 sinalizadores de fumaça vermelha e começa 3 s depois;
  - os paraquedistas avisam com fumaça verde e levam cerca de 13 s até ficar controláveis;
  - a artilharia registrada tem 3 s + 5 s de atraso e só cai em ponto já capturado.
- **Só onde há visão:** quase tudo exige visão do alvo.
- **O inimigo pode pegar as caixas do Supply Drop.**

As fontes são as páginas da tabela.

### CoH2 (comandantes)
- **A escolha:** no 1º CP, o jogador escolhe 1 dos 3 comandantes que levou pra partida ([gamepressure](https://www.gamepressure.com/companyofheroes2/guard-motor-coordination-tactics/z55178)).
- **O comandante:** tem 5 poderes, e cada um **libera sozinho** ao atingir o seu limiar de CP. O CP não é gasto ([Steam](https://steamcommunity.com/app/231430/discussions/0/458605613394052064)).
  - Exemplo, Guard Motor: limiares de 1, 1, 3, 4 e 4 CP.
- **O uso:** paga munição. Exemplos ([coh2.org](https://www.coh2.org/guides/88945/the-coh2-ability-guide-mark-2-0)):
  - Mark Vehicle: 80 mun;
  - 105 mm (OKW): 180 mun;
  - Hold the Line: 250 mun.
- **O que acharam:**
  - os comandantes vendidos à parte viraram "pay to win" ([coh2.org](https://www.coh2.org/topic/14150/title/post/122690));
  - a Relic respondeu com rotação de comandantes grátis e com as War Spoils ([coh2.org](https://Coh2.org/news/20877/war-spoiled-commentary-on-war-spoils));
  - antes do lançamento, a própria Relic já tinha dito: "We learned people don't like pay-to-win" ([PC Gamer](https://pcgamer.com/company-of-heroes-2-hands-on-we-feel-the-chill-in-relics-eastern-front-epic/3)).
- **Contra-jogo:** o IL-2 do Mark Vehicle pode ser derrubado. Isso corta a visão do alvo, mas o bônus de dano continua (coh2.org, acima).

### CoH3 (battlegroups; números dos dados extraídos do jogo, [cohstats/coh3-data](https://github.com/cohstats/coh3-data))
- **Os pontos:** a XP de todos os esquadrões enche um medidor, e cada medidor cheio vale 1 CP. O ganho começa antes da escolha ([gamerjournalist](https://gamerjournalist.com/how-to-earn-command-points-in-company-of-heroes-3/)).
- **A escolha:** 1 battlegroup trava a partida ([fórum oficial](https://community.companyofheroes.com/coh-franchise-home/company-of-heroes-3/forums/1-general-discussion/threads/1602-company-of-heroes-3-battlegroups)).
- **A árvore:** cada battlegroup tem 2 ramos, com pares a/b excludentes. A opção "a" da 1ª faixa custa 0 CP.

| Poder (battlegroup) | CP | Por uso | Recarga |
|---|---|---|---|
| Smoke Bombardment (Airborne, 1a) | 0 | 40 mun | 60 s |
| Recon Loiter (Airborne, 1b) | 1 | 50 mun | 60 s |
| Paradrop de esquadrão (Airborne) | 1 | 320 MP | 60 s |
| P-47 Rocket Strike duplo (Airborne) | 3 | 120 mun | 120 s |
| Carpet Bombing (Airborne) | 5 | 200 mun | 180 s |
| 155 mm Barrage (Advanced Infantry) | 2 | 150 mun | 180 s |
| Incendiary Bombardment (Breakthrough) | 2 | 100 mun | 180 s |
| Tiger chamado (Breakthrough) | 10 (era 9 no lançamento, [Prima](https://primagames.com/gaming/company-of-heroes-3-wehrmacht-battlegroups-guide)) | 910 MP + 250 fuel | 180 s |

- **O que acharam** ([Steam](https://steamcommunity.com/app/1677280/discussions/0/598516772690304156), [Steam](https://steamcommunity.com/app/1677280/discussions/0/598513524566712880), [coh2.org](https://www.coh2.org/topic/62130/title/post/619949)):
  - reclamam de poucos battlegroups;
  - as opções a/b raramente se equivalem, então todo mundo pega "o que funciona";
  - muitos dizem que a doutrina do CoH1 era a melhor da série.
- **Aviso visual e abate no CoH3:** n/c.

---

## 6. Halo Wars 1 e 2

### Halo Wars 1
- **Os leader powers são pagos em recursos.** O ataque do líder sobe até o nível IV no Field Armory ([halopedia](https://halopedia.org/Leader_power)).
- **O que muda com o nível:** cada nível aumenta o dano, o custo e a recarga.
- **Os poderes comuns:** todo líder da UNSC tem Transport (200), Heal and Repair (350) e Disruption Bomb (450). Fora isso, cada um tem 1 poder próprio (o Cutter tem 2).
- **Cutter, MAC Blast:**
  - custa 600 → 700 → 1000 → 1400;
  - dá 1 → 4 tiros ([halopedia](https://www.halopedia.org/James_Cutter/Gameplay));
  - a recarga é n/c.

### Halo Wars 2
- **Os pontos de líder vêm da XP** de coletar, produzir, pesquisar e construir, e muito mais rápido em combate ([Prima](https://primagames.com/eguides/halo-wars-2-eguide/multiplayer-basics)).
- **A partida começa com 1 ponto e 2 poderes à venda** ([halopedia](https://halopedia.org/Leader_power)).
  - Cada ponto gasto abre mais 2 poderes.
  - São 10 poderes em 5 faixas de 2.
  - Em partida longa dá pra ter tudo.
- **Cada uso custa supply e power**, e cada poder tem recarga. O nível sobe o custo e mantém a recarga.

| Poder (Cutter) | Faixa | Supply + power | Recarga | Fonte |
|---|---|---|---|---|
| Lotus Mine I | 2 | 25 + 100 | 105 s | [halopedia](https://www.halopedia.org/James_Cutter/Gameplay) |
| Restoration Drones I → III | 2 | 200+50 → 400+150 | 160 s | idem |
| Archer Missiles I → III | 3 | 300+200 → 500+400 | 228 s | idem |
| ODST Drop | 3 | 800 | 180 s | idem |
| Turret Drop I → II | 4 | 400+25 → 550+50 | 203 s | idem |
| ODST Assault Group | 5 | 1650 + 625 | 360 s | idem |
| Close Air Support | 5 | 1500 | 360 s | idem |

**O que acharam**
- **Confusão:** após o beta, os devs disseram que muita gente achou o uso dos leader powers "unclear". As árvores de evolução eram "confusing, or not distinct enough" ([Xbox Wire](https://news.xbox.com/en-us/2016/08/11/halo-wars-2-beta-thanks-game-changing/)).
- **Ajuste:** prometeram afinar "cost versus impact" (mesmo post).
- **Bola de neve:** o patch de jan/2018 corrigiu um exploit que dava pontos de líder rápido demais e tornava "too easy to dominate your opponents" ([TheSixthAxis](https://www.thesixthaxis.com/?p=308413)).

---

## 7. StarCraft II co-op (a barra de cima)

- **Não há pontos.** Os poderes destravam pelo nível da conta do comandante e, às vezes, por um prédio na partida.
- **Cada comandante tem de 2 a 4 poderes.**
- **O custo:**
  - o freio quase sempre é só a recarga;
  - Artanis, Karax e Vorazun pagam com a energia da nave: começa em 0, máximo de 200, enche 16,9 por minuto;
  - Nova, Zeratul e Han & Horner pagam com recursos.
- **"Coolup":** os mais fortes já nascem em recarga.

Fonte de todos os números: a wiki por comandante, como [Raynor](https://starcraft.fandom.com/wiki/Jim_Raynor_(Co-op_Missions)) (padrão `<Nome>_(Co-op_Missions)`).

| Comandante | Poder | Custo | Recarga |
|---|---|---|---|
| Raynor | Hyperion (dura 60 s) | — | 360 s (já começa em 300) |
| Raynor | Banshee Airstrike | — | 240 s |
| Artanis | Orbital Strike | 25 de energia | 30 s |
| Artanis | Solar Bombardment | — | 300 s |
| Karax | Solar Lance | 50 de energia | 120 s |
| Karax | Purifier Beam | — | 360 s (já começa em 450) |
| Vorazun | Time Stop (congela o mapa por 20 s) | — | 300 s |
| Swann | Concentrated Beam (400 de dano numa linha do mapa) | — | 180 s |
| Nova | Griffin Airstrike | 1000 | 30 s (já começa em 120) |
| Alarak | Death Fleet (dura 60 s) | — | 360 s (já começa em 600) |
| Tychus | Odin | — | 360 s (já começa em 600) |
| Stukov | Apocalisk | — | 300 s |
| Abathur | Toxic Nest | — | 20 s por carga, até 5 cargas |
| Han & Horner | Precision Strike | — | 60 s (120 s se o caça cai) |

- **O que acharam:**
  - a wiki diz que o Raynor depende das chamadas e do tempo certo;
  - também diz que a falta de chamadas fortes é o ponto fraco do Stetmann;
  - a barra do Zeratul, com escolhas travadas até o fim da missão, é a mais customizável ([TL](https://tl.net/forum/starcraft-2/551722-co-op-commander-guide-zeratul));
  - um designer da Blizzard: jogador experiente deve ter opções mais fortes e mais difíceis de dominar ([Blizzard](https://starcraft2.com/news/21574696)).
- **Contra-jogo:** não há (é PvE). A única régua é a recarga.

---

## 8. Tempest Rising (2025; os Veti saíram em 29/09/2026)

- **Três sistemas separados** ([Doctrine](https://tempestrising.wiki.gg/wiki/Doctrine), [Plans](https://tempestrising.wiki.gg/wiki/Plans), [Support power](https://tempestrising.wiki.gg/wiki/Support_power)):
  - **Doutrinas:** 18 por facção, em 3 árvores de 6 tiers.
    - São compradas com crédito e tempo de pesquisa: 800, 1500 ou 3000 cr; 40, 60 ou 80 s.
    - Cabem até 11 ativas.
    - Só se vende a última doutrina da árvore, pela metade do preço.
  - **Planos da Dynasty:** Logistics, Martial e Security.
    - Cada Construction Yard roda 1 plano, que gasta energia.
    - Cada plano a mais custa mais energia.
  - **Support powers:** 5 por facção, cada um preso a um prédio, pago a cada uso e com recarga longa.
- **Intel da GDF** ([Intel](https://tempestrising.wiki.gg/wiki/Intel)):
  - vem dos prédios (1 a cada 6–8,5 s);
  - vem de matar inimigos marcados e de capturar com engenheiro;
  - fica guardado nos prédios, e o dono perde o guardado quando o prédio cai.

| Poder | Custo por uso | Recarga | Prédio | Fonte |
|---|---|---|---|---|
| Spy Drones (GDF) | 1500 cr | 1:30 (era 60 s antes do 1.2.0) | Air Control Tower | [wiki](https://tempestrising.wiki.gg/wiki/Spy_Drones) |
| Support Beacon (GDF) | 1000 cr + 20 Intel (era 30) | 1:40 | Satellite Uplink | [wiki](https://tempestrising.wiki.gg/wiki/Support_Beacon) |
| Communications Jammer (GDF) | 2000 cr + 40 Intel | 2:40 | Data Core | [wiki](https://tempestrising.wiki.gg/wiki/Communications_Jammer) |
| Tempest Air Strike (GDF) | 3000 cr | 3:00 | Tempest Pod | [wiki](https://tempestrising.wiki.gg/wiki/Airstrike) |
| Tempest Instability Missile (Dynasty) | 3500 cr | 3:00 | Radar Station Jammer | [wiki](https://tempestrising.wiki.gg/wiki/Tempest_Instability_Missile) |

- **Contra-jogo:**
  - os 3 bombardeiros do Air Strike podem ser derrubados ([wiki](https://tempestrising.wiki.gg/wiki/Airstrike));
  - o Beacon exige visão da área;
  - derrubar o prédio tira o poder.
- **O que acharam:**
  - no demo, quase ninguém usava doutrina, e os devs baixaram o preço (de 1000/2000/4000) pra mais partidas terem doutrina ([patch](https://tempestrising.wiki.gg/wiki/Multiplayer_demo_patch_28_January_2025));
  - opinião de jogador sobre spam: n/c.

---

## 9. Dawn of War 2 / Retribution e Dawn of War 3

### DoW2
- **O recurso global ("red") vem de matar.**
  - Quem mata leva o valor inteiro, e **o dono do modelo morto leva 75%**. Exemplo: o Land Raider vale 120 pra quem mata e 90 pra quem perde ([wiki](https://dawnofwar2.wiki.gg/wiki/Dawn_of_War_II_economy)).
  - Os Orks ganham +15 por minuto.
- **Cada habilidade global** abre por tier e tem recarga própria.

| Poder | Tier | Custo | Recarga | Fonte |
|---|---|---|---|---|
| For the Emperor (SM) | 1 | 50 | 50 s | [wiki](https://dawnofwar2.wiki.gg/wiki/Retribution/Space_Marines/Globals) |
| Drop Pod (SM) | 2 | 200 + 100 req | 300 s | idem |
| Angels of Death (SM) | 2 | 250 | 180 s | idem |
| Deep Strike Terminators (SM) | 3 | 350 + 650 req + 100 pow | 420 s | idem |
| Orbital Bombardment (SM) | 3 | 500 | 120 s | idem |
| Basilisk Creeping Barrage (IG) | 2 | 200 | 90 s | [wiki](https://dawnofwar2.wiki.gg/wiki/Retribution/Imperial_Guard/Globals) |

- **Contra-jogo:** o Orbital só prende a infantaria 4,5 s depois de chamado. Os feixes seguintes caem aos 6,5 e 8,5 s, o que dá tempo de dispersar ([wiki](https://dawnofwar2.wiki.gg/wiki/Retribution/Orbital_Bombardment)).

### DoW3
- **Os Elite Points sobem com o tempo**, e mais rápido capturando pontos de elite ([manual](https://segaretro.org/images/3/39/DOW3_Steam_manual.pdf)).
  - Esses pontos pagam tanto os heróis de elite quanto a super-habilidade.
- **Antes da partida** o jogador escolhe 3 elites e 6 doutrinas. Elas destravam fora da partida, com Skulls (mesma fonte).
- **Uma super-habilidade por raça:** Orbital Bombardment, Eldritch Storm e Rokks.
- **O que acharam:**
  - o Orbital foi chamado de quebrado: matava um Wraithknight de vida cheia ([CGMagazine](https://www.cgmagonline.com/review/game/warhammer-40000-dawn-war-3-review-step-away-security/));
  - a Relic subiu a recarga de 5 para 7,5 min no multiplayer (patch de 31/05/2017, número n/c na página oficial);
  - outra crítica: as doutrinas presas atrás de uma moeda lenta ([GameCritics](https://gamecritics.com/steven-brown/warhammer-40000-dawn-of-war-iii-review/)).
- **Contra-jogo:** o feixe do Orbital segue um farol. Se o inimigo captura o farol, o dono perde o controle ([GamePressure](https://www.gamepressure.com/warhammer-40k-dawn-of-war-3/space-marines-faction-characteristic/z19acd)).

---

## 10. Act of War, Iron Harvest, Broken Arrow e WARNO

### Act of War: Direct Action / High Treason
- **Não há poder.** O ataque aéreo é um avião comprado numa torre (US$ 2400–4000), até 3 por torre ([wiki](https://actofwar.fandom.com/wiki/Aircraft_control_tower), [B-2](https://actofwar.fandom.com/wiki/B-2_Spirit)).
- **O DEFCON libera os mais fortes:** o B-2 pede DEFCON 1 ([DEFCON 2](https://actofwar.fandom.com/wiki/Alert_Status_DEFCON_2)).
- **O melhor contra-jogo da lista:** o Early Warning Radar mostra o tipo do avião, a rota e a hora da chegada ([wiki](https://actofwar.fandom.com/wiki/Early_warning_radar)). O antiaéreo derruba.
- Cada facção tem também um prédio que intercepta a "tactical weapon" (a superarma) do inimigo ([wiki](https://actofwar.fandom.com/wiki/Tactical_weapon)).

### Iron Harvest
- Não tem poder de comandante nem apoio fora do mapa. O "especial" são os heróis, 3 por facção ([devblog](https://kingart-games.com/article/77)).

### Broken Arrow (2025)
- **O battlegroup é montado antes da partida** ([guia oficial](https://www.matrixgames.com/amazon/BrokenArrow/BrokenArrow_Basics.pdf)).
- **A renda:** cai por minuto e diminui com mais unidades em campo.
- **Não há poder fora do mapa:** artilharia, míssil e bombardeiro são unidades.
- **Opiniões de jogadores no Steam:**
  - um reclama do míssil de cruzeiro lançado a 5 km e derrubado logo ([Steam](https://steamcommunity.com/app/1604270/discussions/0/4635986990492307001));
  - outro diz que um antiaéreo leve basta pra derrubar os mísseis ([Steam](https://steamcommunity.com/app/1604270/discussions/0/603018544972597895)).
- **A lição:** quando o abate é quase garantido, o poder parece inútil.

### WARNO
- **O baralho tem 50 pontos de ativação.** Cada vaga a mais da mesma categoria custa mais: 1, 1, 2, 2, 3 ([guia](https://www.magicgameworld.com/warno-ultimate-beginners-guide-tips-tricks-for-new-players/)).
- Apoio fora do mapa: n/c.

---

## 11. Extra: Age of Mythology (original e Retold)

- **A escolha:** a cada era, o jogador escolhe 1 de 2 deuses menores, e cada um traz 1 poder ([wiki](https://ageofempires.fandom.com/wiki/Minor_god)). É a escolha excludente mais parecida com um poder comprado na partida.
- **No original**, os poderes de gregos, egípcios e nórdicos eram de uso único.
- **No Retold** todos se reusam ([wiki](https://ageofempires.fandom.com/wiki/God_power)):
  - o 1º uso é grátis;
  - o 2º custa um valor fixo de favor;
  - cada uso seguinte soma um acréscimo;
  - há recarga entre usos.

| Poder | Recarga | 2º uso | Acréscimo por uso | Fonte |
|---|---|---|---|---|
| Bolt | 90 s | 55 favor | +5 | [wiki](https://ageofempires.fandom.com/wiki/God_power) |
| Restoration | 180 s | 125 | +25 | idem |
| Meteor / Lightning Storm | 240 s | 350 | +5 | idem |
| Ragnarok | 300 s | 250 | +50 | idem |

- **O que os devs disseram:**
  - Kristen Pirillo: com uso único, o jogador guardava o poder pro momento perfeito, que nunca chegava ([Windows Central](https://www.windowscentral.com/gaming/xbox/age-of-mythology-retold-interview-aug-2024));
  - Earnest Yuen: o poder que volta, como num MOBA, é melhor pro esports e pra quem assiste ([PC Gamer](https://www.pcgamer.com/age-of-mythology-retold-interview-preview/)).
- **Poder que muda o mapa:** o Forest/Aspen Grove planta árvores que servem de barreira ([wiki](https://ageofempires.fandom.com/wiki/God_power)).

---

## 12. Padrões e lições pra nós

A nossa estrutura já decidida (`docs/PODERES_DO_GENERAL.md` §1):
- 5 patentes que dão 1, 1, 2, 2, 3 pontos (9 no total);
- poderes de 1 a 3 pontos, sem trava;
- alguns poderes com níveis I–III;
- recarga por uso;
- a XP de hoje: valor destruído ÷ 10 + valor perdido ÷ 30.

As lições abaixo são o que o mercado mostra. **As propostas são nossas**, pra conta e pro teste IA × IA, e ainda não foram aprovadas.

### 12.1 O que deixa um poder divertido de usar e de levar

| Padrão | Quem faz | Por que funciona |
|---|---|---|
| Destravar uma vez, usar com recarga | todos | a escolha pesa uma vez e o uso vira decisão tática |
| Efeito grande, claro e que se lê na hora | Orbital Drop, Time Bomb, Napalm | o jogador vê o resultado e o inimigo entende o que levou |
| Nível que muda o "como", não só o número | Magnetic Satellite (o III puxa até o pesado), Time Bomb (pavio maior) | o nível vira uma nova decisão, não um +10% |
| Combinação entre poderes | Magnetic Satellite + Orbital Drop (o Drop joga de volta o que o satélite puxou, [wiki](https://cnc.fandom.com/wiki/Orbital_Drop)) | dá o "momento" de quem planejou |
| Poder que responde a outro | Cloud Break × Darkness (BFME2) | cria contra-jogo dentro do livro |
| O último ponto vale o grande | BFME2 (25 PP), poderes de 3 pontos | dá um alvo pra partida inteira |

**O que evitar:**
- **Uso único:** vira poupança e o poder nunca é usado (AoM, a designer acima).
- **Poder óbvio demais:** todo mundo pega (Rallying Call no BFME2; a/b desiguais no CoH3).
- **Poder que não se entende:** o beta do HW2.

**Proposta nossa:** toda patente deve ter pelo menos 2 boas opções de 1 ponto, de tipos diferentes. Pra cada facção, testar na IA × IA se algum poder aparece em mais de ~80% das partidas.

### 12.2 Bola de neve e chance de virada

O risco: se a patente vem só de matar e cada patente dá poder, quem ganha a primeira luta ganha o poder que vence a segunda. O mercado tem cinco freios:

| Freio | Quem faz | Número |
|---|---|---|
| Quem perde também ganha ponto | RA3: perder vale 300% e matar 75% em ameaça; DoW2: o dono do morto leva 75% | [Threat Meter](https://cnc.fandom.com/wiki/Threat_Meter), [DoW2](https://dawnofwar2.wiki.gg/wiki/Dawn_of_War_II_economy) |
| Ponto que não vem só de matar | WiC: capturar +5, fortificar +5, consertar; C&C4: tempo, buff, dano | [WiC](https://worldinconflict.fandom.com/wiki/Command_point), [C&C4](https://cnc.fandom.com/wiki/Resources_(Tiberian_Twilight)) |
| A perda volta aos poucos | WiC devolve o ponto de reforço | [Wikipedia](https://en.wikipedia.org/wiki/World_in_Conflict) |
| O poder máximo nasce em recarga | SC2: 450–600 s | wiki do SC2 (seção 7) |
| O poder grande zera o placar no lugar | WiC: a nuke deixa os pontos neutros | [wiki](https://worldinconflict.fandom.com/wiki/Tactical_Aid) |

- **A comparação com o nosso:** hoje perder vale 1/3 de matar (÷30 contra ÷10). No RA3 vale 4×, e no DoW2 vale 0,75×.
- **Proposta nossa:** subir o peso da perda pra algo entre 0,5× e 0,75× do peso de matar (÷15 a ÷13), sem passar de matar. Isso segue a regra "pras pessoas precisarem ir pra luta": quem se esconde não ganha nada, e quem luta e perde ainda avança.
- **Outra proposta:** dar XP por defender (dano evitado perto dos seus prédios). Isso não está no mercado como regra direta. Testar na IA × IA (`reference-ia-x-ia-sem-placa`): medir o minuto de cada patente do lado que ganha e do que perde. O alvo é a diferença ficar abaixo de ~1 patente aos 15 min.
- **Evitar:** poder pago com crédito. No C&C3 quem tem mais dinheiro tem mais poder (leitura nossa).

### 12.3 Poderes que mudam o mapa

| Efeito | Quem faz | Fonte |
|---|---|---|
| Queimar a mata e expor a infantaria | WiC Napalm, Daisy Cutter | [WiC](https://worldinconflict.fandom.com/wiki/Tactical_Aid) |
| Derrubar e reconstruir ponte | WiC LGB (2 acertos) e Repair Bridge | idem |
| Pintar o chão com bônus | BFME Elven Wood, Tainted Land | [BFME](https://bfme.fandom.com/wiki/Powers) |
| Escurecer o mapa ou trocar o clima | BFME Darkness, Freezing Rain; Cloud Break desfaz | idem |
| Criar recurso no mapa | C&C3 Overlord's Wrath (Tiberium de $4645) | [C&C](https://cnc.fandom.com/wiki/Overlord%27s_Wrath) |
| Plantar barreira | AoM Forest/Aspen Grove | [AoM](https://ageofempires.fandom.com/wiki/God_power) |
| Zona que fica | WiC: radiação da nuke (20 HP/s); RA3: Desolator (30 s) | [WiC](https://worldinconflict.fandom.com/wiki/Tactical_nuke), [RA3](https://cnc.fandom.com/wiki/Desolator_Airstrike) |
| Fumaça que tapa a visão | CoH Smoke Bombardment | [coh3-data](https://github.com/cohstats/coh3-data) |

**Proposta nossa:**
- **Napalm da China (Flame):** queima palmeira e capim, e a fuligem segue o Fluxo (`docs/ESTUDO_LUZ_NA_FUMACA.md`).
- **Minas da GLA:** a ideia já está no doc mãe.
- **Ponte do engenheiro:** como utilidade.
- **Fumaça de cobertura:** na simulação, tapa a visão das unidades inimigas. No visual, segue a regra da fumaça volumétrica e não pode esconder a batalha de quem joga (§12.7).

### 12.4 Variedade de tipos (o mercado por tipo)

| Tipo | Exemplos do mercado | Custo/recarga típicos |
|---|---|---|
| Ataque de área | WiC Air Strike, Napalm, Carpet; RA3 Desolator | WiC 10–95 TA; 30–90 s |
| Ataque pontual (prédio, ponte) | WiC LGB; RA3 Surgical Strike | WiC 8–10 TA |
| Invocação de tropas | CoH Paratroopers; HW2 ODST; BFME Army of the Dead; RA3 Sleeper Ambush | CoH 320–375 MP; HW2 180 s |
| Unidade de elite chamada | CoH Pershing/Tiger (1 por vez); SC2 Odin | CoH 5–10 CP |
| Reconhecimento | WiC Aerial Recon; CoH P-47 Recon; C&C3 Radar Scan; RA3 Surveillance Sweep | WiC 5–8 TA / 30 s; CoH 20–60 s |
| Economia | RA3 Free Trade (+25%) e Mass Production (−25%); BFME Industry, Devastation; CoH Supply Drop | passivo ou 180 s |
| Defesa e controle | RA3 Chrono Rift, Magnetic Singularity; HW2 Turret Drop; BFME Lone Tower | 120–203 s |
| Buff do exército | BFME Rallying Call; DoW2 For the Emperor; RA3 Emperor's Rage | DoW2 50 / 50 s |
| Logística e utilidade | WiC Repair Bridge; HW2 Restoration Drones; C&C3 Cloaking Field; Nod Decoy Army | 30–420 s |
| O máximo | WiC Nuke; DoW2/3 Orbital; BFME 25 PP; SC2 Purifier Beam | 300–600 s |

**Proposta nossa pras 4 linhas da tela** (ar, tropas, fogo, apoio):
- cada facção tem pelo menos 1 poder de reconhecimento;
- cada facção tem 1 de economia ou logística;
- cada facção tem 1 de controle, que trava ou desvia sem matar.

Só ataque deixa a escolha rasa.

### 12.5 Em time (2v2 e aliados)

| Padrão | Quem faz |
|---|---|
| Doar pontos ao aliado (juntar pra nuke) | WiC ([review](https://www.gamespot.com/world-in-conflict/user-reviews/2200-501833/)) |
| Bônus que respinga no aliado (25% dono / 5% time, teto de 35%) | RA3 Free Trade, Mass Production |
| Preço diferente por papel, pra cada um fazer a sua parte | WiC (o Support paga menos pela ponte e pela LGB) |
| Combos entre parceiros | SC2 co-op (a barra de um prepara o outro) |
| Mesmo livro pro lado inteiro | BFME1 ([gamicus](https://gamicus.fandom.com/wiki/The_Lord_of_the_Rings:_The_Battle_for_Middle-earth)) |

**Proposta nossa:**
- alguns poderes de apoio também servem o aliado (reparo, reconhecimento, fumaça);
- o alvo do poder aparece pro aliado antes do golpe, pra ele tirar as unidades de baixo;
- doar pontos de patente fica fora: tira o peso da escolha de cada um (a conferir com ele).

### 12.6 Como a IA usa
- O mercado não documenta como a IA escolhe poder (n/c).
- **Proposta nossa, pela SimCore:** a IA escolhe o poder por personalidade do general e mira onde o valor inimigo na área supera o próprio. Os critérios:
  - guarda o poder máximo pra base ou pra grupo grande;
  - nunca usa onde há unidade sua;
  - respeita o atraso: mira à frente do grupo que anda.
- O teste IA × IA dá a taxa de compra de cada poder (§12.1) e o minuto de cada patente (§12.2).

### 12.7 Legibilidade vista de cima
- **O aviso antes do golpe precisa ler de cima.** O mercado usa:
  - fumaça vermelha no chão e 3 s de atraso (CoH);
  - som pra todos (C&C3);
  - pavio visível (Time Bomb);
  - radar com a rota (Act of War);
  - de 4,5 a 25 s até o impacto (DoW2, WiC).
- **Proposta nossa:** a marca do alvo é um decalque no chão que os dois lados veem, mais o som. O nosso aviso de superarma já vai nessa linha (`docs/SUPERARMAS.md`).
- **O efeito não pode cobrir a batalha:**
  - zona que fica (fogo, gás, radiação) com borda clara e nível baixo;
  - a fumaça do poder vive na luz do mundo e mantém a unidade legível;
  - as invocações chegam por avião ou paraquedas, que dá pra derrubar (CoH, RA3) e não cobre a tela.
- **Contra-jogo que paga:** derrubar o avião ou a bomba dá XP a quem derruba (RA3 Final Squadron: 2000). Mas não pode ser abate garantido (Broken Arrow).
- **Confusão mata o poder** (HW2 beta). O ícone e o alvo devem dizer o que o poder faz sem legenda (regra da UI dele).

### 12.8 Ritmo: números de referência pro nosso cardápio

| Medida | O mercado | Proposta nossa (pra conta) |
|---|---|---|
| 1º poder | CoH3: a 1ª opção custa 0 CP; HW2: começa com 1 ponto; o nosso começa com 1 ponto | igual (já decidido) |
| Recarga de 1 ponto | 20–90 s (WiC recon 30, CoH recon 20–60, DoW2 buff 50) | 60–120 s |
| Recarga de 2 pontos | 90–240 s (WiC napalm 90, RA3 120–240, HW2 160–228) | 150–240 s |
| Recarga de 3 pontos / máximo | 300–600 s (WiC nuke 450, RA3 III 300–360, SC2 360–600, DoW3 7,5 min) | 360–480 s, nascendo em recarga |
| Atraso até o impacto | 3–25 s | 3–6 s nos pequenos; 10–16 s no máximo |
| Nível I → III | a recarga sobe 1,5–3× (RA3); o custo sobe e a recarga fica (HW2) | a recarga sobe ~1,5× por nível |
| Patente 5 | não há equivalente direto; o máximo do SC2 nasce em 600 s | perto de 15–20 min numa 1×1 normal, medido na IA × IA |

---

## Fontes principais (as outras estão no texto)

- **RA3:**
  - [Top Secret Protocol](https://cnc.fandom.com/wiki/Top_Secret_Protocol)
  - [Threat Meter](https://cnc.fandom.com/wiki/Threat_Meter)
  - [Tutorial](https://cnc.fandom.com/wiki/Transcript:Support_Powers)
- **C&C3/KW:** [Support power](https://cnc.fandom.com/wiki/Support_power)
- **C&C4:** [Resources](https://cnc.fandom.com/wiki/Resources_(Tiberian_Twilight))
- **BFME:**
  - [Powers](https://bfme.fandom.com/wiki/Powers)
  - [Forces of Light](https://bfme2.heavengames.com/factions/powers-fol/)
  - [Forces of Darkness](https://bfme2.heavengames.com/factions/powers-fod/)
- **WiC:** [Tactical Aid](https://worldinconflict.fandom.com/wiki/Tactical_Aid), mais as páginas de cada poder
- **CoH3:** [cohstats/coh3-data](https://github.com/cohstats/coh3-data)
- **CoH1:** [fandom](https://companyofheroes.fandom.com/wiki/Company_Commander)
- **Halo Wars:**
  - [Leader power](https://halopedia.org/Leader_power)
  - [Cutter](https://www.halopedia.org/James_Cutter/Gameplay)
- **SC2 co-op:** [starcraft.fandom](https://starcraft.fandom.com/wiki/Jim_Raynor_(Co-op_Missions))
- **Tempest Rising:** [wiki.gg](https://tempestrising.wiki.gg/wiki/Doctrine)
- **DoW2:** [wiki.gg](https://dawnofwar2.wiki.gg/wiki/Dawn_of_War_II_economy)
- **AoM:** [God power](https://ageofempires.fandom.com/wiki/God_power)

Nota técnica: as wikis do fandom bloqueiam leitura direta. Os números vieram do texto-fonte das páginas, pela API do MediaWiki (`api.php?action=parse&prop=wikitext`).
