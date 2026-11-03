# Estudo: poderes do general da China fora do ZH (Contra, mods, RTS e a China real)

Levantamento de 03/11/2026 para desenhar a árvore de poderes da nossa China (5 patentes dão 1, 1, 2, 2, 3 pontos; poder de 1 a 3
pontos; níveis I–III em patentes seguidas; recarga por uso; o golpe chega pelo mapa; só as tropas chamadas custam dinheiro).
O ZH base (China, Tank, Infantry, Nuke) está em `docs/poderes/ESTUDO_ZH.md` §2.2 e §3; o mercado em `docs/poderes/ESTUDO_RTS.md`;
a USA fechada em `docs/poderes/USA.md`. Aqui só entra o que é novo.

**Convenções**
- **n/c** = não confirmado. **estimativa** = conta minha.
- Pat. = patente (estrela) em que o poder aparece. Recarga em min:s.
- Distância do Contra, ShockWave e RotR: são mods do ZH, então 1 u = 0,2633 m.
- A wiki do Contra foi lida pela API da fandom (`api.php?action=parse`). Os links abaixo são as páginas normais.
- O changelog oficial do Contra (Revora) tem 39 páginas. Cito como "Revora p38" ([009 Final, Patches 1 e 2](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-38)) e "Revora p39" ([Patch 3 e hotfixes](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-39)).
- **Restrições da nossa China:** nada de drone (a superarma é o enxame de drones), nada nuclear (é da Rússia).

---

## Resumo (10 linhas)

1. **O Contra 009 tem 4 chineses jogáveis e 1 chefe:** Kwai (tanques), Fai (infantaria), Tao (nuclear), Bao (fogo, novo do mod) e Leang (chefe do Challenge). Cada um troca quase toda a árvore.
2. **A "artilharia de fogo" do Contra é o Bao, e ela é um combo:** o An-22 encharca a área de combustível (Spill Fuel I–III: 3, 6 e 9 poças) e qualquer arma de fogo acende tudo. A Artillery Barrage dele (12, 24 e 36 granadas incendiárias) perdeu o fogo que ficava no chão e virou o fósforo que acende o combustível.
3. **A entrega do Contra tem cara:** o An-22 traz minas, tropas, tanques e combustível; o bombardeiro Hong-6 traz as bombas. Os dois se derrubam.
4. **A massa no Contra é poder de verdade:** Cloning (dobra a infantaria numa área), Deploy Barracks (quartel em 5 s em qualquer lugar), Tank Drop → Emperor Delivery (tanques gigantes de helicóptero, numa cadeia de pré-requisito).
5. **Os números do Frenzy do Contra servem de régua:** +25% por 30 s, +50% por 45 s, +100% por 1 min. No ZH, +10% não servia pra nada.
6. **O Contra tirou da China o que virou chato:** Cluster Mines (Patch 3), Cash Hack do Fai (Patch 2), Neutron Bomb do Fai (trocada pela ECM Bomb no Patch 3). E deu olhos baratos: as Signal Mines (1:00), minas que soltam sinalizador quando o inimigo passa.
7. **O Rise of the Reds é o mais organizado:** a 1 estrela escolhe a doutrina (Mass Mobilization faz tropa em par; War Propaganda dá o bônus de horda constante); na 3, um satélite de interferência controlável (ECM Burst, 15/25/35 s); na 5, o Napalm Strike de 3 H-6 (tempestade de fogo de 26 m por 6 s).
8. **Fora dos mods, o criativo pra massa é chamar gente sem avião:** Tempest Rising faz os seus prédios cuspirem milícia (Call the Guard); o CoH2 repõe de graça quem morre enquanto o poder dura (Rapid Conscription); o RA3 troca velocidade por força (Emperor's Rage).
9. **Pro hacker, o mercado dá três ideias:** apagar o minimapa inimigo (C&C3), criar exército falso (C&C3, Generals 2), e dinheiro por morte marcada numa área (RA3).
10. **A China real dá nomes e mecânicas prontas:** o PHL-16 troca o módulo (300, 370 ou 750 mm); o Y-20 leva 2 tanques leves; o Y-9XZ é um avião de propaganda de verdade; o balão de 2023; a Volt Typhoon (implantar agora, sabotar depois); a lei de mobilização; o micro-ondas Hurricane 3000 contra drones. O nome "DF" fica de fora: a família é a principal entrega nuclear da China.

---

## 1. Contra 009 Final: os generais chineses

### 1.0 O que vale pra todos os chineses do Contra

**Versão.** A última do 009 é o Final Patch 3 Hotfix 4. A wiki mostra quase sempre o estado do Patch 2; onde o Patch 3 mudou, eu aviso.

**Pontos e camadas** (já em ESTUDO_ZH §6.1)
- Começa com 1 ponto e ganha 1, 1, 2, 3. Quase tudo custa 1 ([wiki](https://zerohourcontra.fandom.com/wiki/About_General%27s_Promotion)).
- A patente 3 abre sozinha o Propaganda Center e o aeródromo chinês; a 5 abre superunidade e superarma (mesma fonte).

**A entrega** ([wiki Support Aircraft](https://zerohourcontra.fandom.com/wiki/Support_Aircraft))
- **An-22 "Antei"** (troca o Y-8 do ZH): solta minas, paraquedistas do Fai, tanques do Kwai e o combustível do Bao. A wiki diz que ele é rápido e resistente, mas não aguenta antiaérea por muito tempo, e cai com a carga.
- **Hong-6** (o H-6 real): faz o papel do B-52. Lento e resistente, com 2 canhões de 23 mm na cauda que metralham ao passar. Traz o Carpet Bomb, a Neutron Bomb, a Magnetic Bomb e a bomba termobárica.

**A massa, a propaganda e o hacker (o que não é poder, mas ensina)**

| Peça | O que faz | Números | Fonte |
|---|---|---|---|
| Horde bonus | grupos de 5 ou mais ganham bônus de combate | Nationalism +25%; Patriotism (Fai) +25% a mais | [Kwai](https://zerohourcontra.fandom.com/wiki/General_Ta_Hun_Kwai), [Fai](https://zerohourcontra.fandom.com/wiki/General_Shin_Fai) |
| Speaker Tower | cura e dá bônus às unidades perto | $500; Subliminal Messaging +65% | [wiki](https://zerohourcontra.fandom.com/wiki/Speaker_Tower) |
| Propaganda Officer (Fai, Patch 3) | soldado sem arma com alto-falante e bandeira: cura e dá moral | $300, 3 s; +25% de dano | [wiki](https://zerohourcontra.fandom.com/wiki/Propaganda_Officer), Revora p39 |
| Hacker / Internet Center | faz dinheiro; o prédio pesquisa o Satellite Hack | Patch 2: 1 saque a cada 1,9 s dentro do prédio, 2,1 s fora | Revora p38 |
| Satellite Hack I / II | I: mostra o CC inimigo; II: mostra toda a atividade inimiga por um tempo, de tempos em tempos | duração n/c | [Kwai](https://zerohourcontra.fandom.com/wiki/General_Ta_Hun_Kwai) |
| Black Lotus | captura prédio, rouba dinheiro, desliga veículo, cria holograma de si mesma | $2 000; o Patch 2 tirou a XP dos hacks dela | [wiki](https://zerohourcontra.fandom.com/wiki/Black_Lotus), Revora p38 |
| ECM Tank | desliga veículos e desvia mísseis | $900 ($2 300 o do Kwai); alcance 150 u (40 m) | [wiki](https://zerohourcontra.fandom.com/wiki/ECM_Tank) |

**O que o Patch 3 mudou na China** (Revora p39)
- Cluster Mines saiu de todos os chineses (e o upgrade de minas de nêutrons junto).
- Todas as Artillery Barrages ficaram mais fortes: o nível II derruba uma War Factory ou um aeródromo.
- Signal Mines: recarga de 1:15 → 1:00, e o avião ganhou visão.
- Fai: a Neutron Bomb virou ECM Bomb.

### 1.1 Tank: General Ta Hun Kwai

"Build more tanks and attack". Os tanques do Kwai ganham bônus de horda em grupos de 5 ([wiki](https://zerohourcontra.fandom.com/wiki/General_Ta_Hun_Kwai)).

| Poder | Pat. | Níveis | Recarga | O que faz | Como chega / contra-jogo | O que é criativo |
|---|---|---|---|---|---|---|
| **Signal Mines** | 1 | — | 1:00 (Patch 3; era 1:15) | minas que soltam um sinalizador no céu quando o inimigo passa por cima | o An-22 solta; o avião pode ser derrubado; **é de graça** pro Kwai | olho barato, sem drone: o campo minado vira alarme |
| Hacker School | 1 | — | passivo | hackers e Black Lotus nascem veteranos | — | — |
| Emergency Repair | 1 (exige pat. 2) | I–III | n/c | conserta veículos e prédios numa área | **os níveis II e III são comprados com dinheiro** no Propaganda Center ($2 000, 60 s, Patch 2) | o nível que se paga em dinheiro, não em ponto |
| Tank Squadron Training | 3 | — | passivo | War Master, ECM, Gauss e Dragon nascem veteranos | — | — |
| Heavy Tank Training | 3 | — | passivo | Emperor, Banshee, Manticore e Earth Shaker nascem veteranos | — | — |
| **Deploy War Factory** | 3 | — | n/c | monta uma War Factory em qualquer lugar do mapa | — | a fábrica na linha de frente (a USA já tem o FOB) |
| Frenzy | 3 | I–III | n/c | +25% de dano por 30 s / +50% por 45 s / +100% por 1 min | — | o nível que dobra de verdade |
| Artillery Barrage | 3 | I–III | n/c | 12 / 24 / 36 granadas **sísmicas** | de fora do mapa; o Hotfix 2 corrigiu um dano em dobro | a granada muda pelo general |
| **Tank Drop** | 3 | I–III | 5:45 (Patch 2; era 4:30) | 3 War Masters / 4 + 2 Gauss / 6 War Masters + 3 Gauss | o An-22 solta de paraquedas; o Patch 2 reduziu a quantidade | o tanque de paraquedas |
| **Magnetic Bomb** | 5 | — | n/c | desliga os veículos inimigos e desvia os mísseis numa área | o Hong-6 solta | desligar + escudo antimíssil num golpe só; libera o campo magnético da Black Lotus |
| **Emperor Delivery** | 5 | — | n/c | 3 helicópteros Bixi trazem Emperors (o super-Overlord) | exige Tank Drop III; o Bixi pode ser derrubado | **a cadeia de pré-requisito**: o máximo da linha só abre com o III completo |

Fontes: [wiki Kwai](https://zerohourcontra.fandom.com/wiki/General_Ta_Hun_Kwai); recargas e números do patch: Revora p38 e p39.

### 1.2 Infantry: General Shin Fai

"A maior riqueza da China são os milhões de uniformes" (a ficha dele). 10 tipos de infantaria e as Troop Crawlers ([wiki](https://zerohourcontra.fandom.com/wiki/General_Shin_Fai)).

| Poder | Pat. | Níveis | Recarga | O que faz | Como chega / contra-jogo | O que é criativo |
|---|---|---|---|---|---|---|
| Artillery Training | 1 | — | passivo | artilharia nasce veterana: +10% de cadência, dano e vida | — | — |
| Infantry Training | 1 | — | passivo | infantaria, hackers e Black Lotus nascem veteranos; **+20% de dinheiro do hacker** | — | o treino que também mexe na economia |
| Emergency Repair | 1 (exige pat. 2) | I–III | n/c | conserta veículos e prédios e **cura a infantaria** | — | — |
| **Deploy Bunker** | 3 | — | n/c | um bunker em qualquer lugar do mapa; 20 s pra montar | montando, é alvo | a trincheira que cai do céu |
| **Deploy Barracks** | 3 | — | n/c | um quartel em qualquer lugar; 5 s pra montar | — | a horda nasce na frente |
| Artillery Barrage | 3 | I–III | n/c | 12 / 24 / 36 granadas comuns | de fora do mapa | — |
| **Infantry Paradrop** | 3 | I–III | n/c | 4 Minigunners + 4 Tank Hunters / 4 + 5 + 4 Grenadiers / 4 MG + 4 Red Arrows + 4 ATGM + 4 Grenadiers | o An-22; o Patch 3 tirou 2 MG do III e pôs 2 Red Arrows; caem mais juntos | o nível troca a mistura, não só a quantidade |
| Cash Hack | 3 | I–III | n/c | $2 000 / $4 000 / $6 000 | **removido do Fai no Patch 2** (a wiki está atrasada) | — |
| Frenzy | 3 | I–III | n/c | igual ao do Kwai | — | — |
| Neutron Bomb → **ECM Bomb** | 5 | — | n/c | a Neutron matava infantaria e tripulação sem ferir prédio; o Patch 3 trocou por uma bomba de ECM | o Hong-6 | — |
| Carpet Bomb | 5 | — | n/c | o Hong-6 com bombas comuns | — | — |
| **Cloning** | 5 | — | n/c | **dobra a infantaria** dentro da área (menos a Black Lotus) | a IA do Patch 3 usa quando a horda dela é atacada | a horda que se multiplica no meio da luta |

**A artilharia que anda com a tropa:** o Mobile Barracks ($2 000) treina infantaria no campo e **chama uma barragem de fora do mapa** do tamanho de uma Artillery Barrage I, sem precisar se desdobrar. Recarga de 25 s no Patch 3 (era 40 s) ([wiki](https://zerohourcontra.fandom.com/wiki/Mobile_Barracks), Revora p39).

### 1.3 Nuke: General Tsing Shi Tao (só pra referência: nuclear é da Rússia)

- Árvore: Infantry Training, Emergency Repair I–III, Cluster Mines (saiu no Patch 3), **Nuclear Storm destravado na patente 3**, Artillery Barrage radioativa I–III, Cash Hack $2 000 / $4 000 / $6 000, Frenzy I–III; na 5, Neutron Bomb, Carpet Bomb nuclear e MIRV ([wiki](https://zerohourcontra.fandom.com/wiki/General_Tsing_Shi_Tao)).
- O Carpet Bomb nuclear perdeu 35% do dano no 009 (Revora p38).
- **O que serve pra nós:** o Cash Hack do Contra paga o dobro do ZH ($2 000 no nível I), e a superarma pode abrir antes por ponto (lição de pré-requisito, não de nuclear).

### 1.4 Flame: General Huang Lin Bao (o general novo do Contra)

Três fogos com papéis diferentes: **napalm** contra infantaria, **termite** contra veículo e **termobárico** contra prédio ([wiki](https://zerohourcontra.fandom.com/wiki/General_Huang_Lin_Bao)).

| Poder | Pat. | Níveis | Recarga | O que faz | Como chega / contra-jogo | O que é criativo |
|---|---|---|---|---|---|---|
| Flame Unit Training | 1 | — | passivo | lança-chamas, Dragon, Vortex, A-100, Immolator, Howitzer nascem veteranos (o MiG saiu no Patch 3) | — | — |
| Hacker School | 1 | — | passivo | igual ao Kwai | — | — |
| Emergency Repair | 1 / 3 | I–III | n/c | cura veículos | — | — |
| **Spill Fuel** | 3 | I–III | n/c | o avião encharca a área: **3 / 6 / 9 poças**; qualquer arma de fogo acende e vira tempestade de fogo | o An-22; a poça fica no chão até acender | **o fogo em dois tempos:** prepara o chão, depois acende |
| Frenzy | 3 | I–III | n/c | igual | — | — |
| **Artillery Barrage** | 3 | I–III | n/c | 12 / 24 / 36 granadas **incendiárias** | de fora do mapa | ver a história abaixo |
| **Fire Shaft** | 5 | — | — | destrava o tornado de fogo do Zhu Rong: 100 de dano em raio 50 u (13 m), alcance 50 u; queima amigo e inimigo | — | o poder que destrava a habilidade da superunidade |
| **Volume Detonation Bomb** | 5 | — | n/c | o Hong-6 solta a bomba termobárica; **acende o combustível** (Patch 2) | o Hong-6 | o grande final do combo |
| Magnetic Bomb | 5 | — | n/c | igual ao Kwai | o Hong-6 | — |

**A história da artilharia de fogo (o pedido especial)**
- No Contra 008, a barragem do Bao deixava fogo no chão. O time tirou: "does not leave fire anymore" ([Revora p37](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-37)).
- No 009, ela voltou com "um pouco de chama, não pra destruir, mas pra acender combustível" (Revora p38).
- No Patch 2, o Spill Fuel ganhou 3 níveis (antes era 1 nível e o Bao tinha um Tank Drop); o Flame Paradrop saiu (Revora p38; [wiki](https://zerohourcontra.fandom.com/wiki/General_Huang_Lin_Bao), Trivia).
- **Leitura:** o fogo que fica no chão em toda granada era forte demais. O Contra separou as funções: **o combustível nega a área, a artilharia só acende.** Quem joga escolhe a hora da explosão.

**As outras peças de fogo do Bao** (unidades, não poderes)

| Peça | O que faz | Números | Fonte |
|---|---|---|---|
| Fuel Truck | caminhão-tanque camuflado que derrama uma poça ao comando; imune a fogo | $500; derrama a cada 1:00 | [wiki](https://zerohourcontra.fandom.com/wiki/Fuel_Truck) |
| Fuel Tube | defesa camuflada que derrama combustível em volta | recarga 4:00 → 2:00 (Patch 3) | Revora p39 |
| Howitzer do Bao (o PLZ-05) | granada incendiária; **em grupos de 5 ou mais cria tempestade de fogo** (com Flame Combustion) | $1 000; 38 de dano a cada 4 s; alcance 350 u (92 m) | [wiki](https://zerohourcontra.fandom.com/wiki/Howitzer) |
| A-100 MLRS | foguetes de napalm de 300 mm; rajada longa de 10 ou curta de 2 | $2 750; 10 foguetes a cada 12,5 s; 20 de dano + fogo por 8 s; raio 25 u (6,6 m); alcance 420 u (110 m) | [wiki](https://zerohourcontra.fandom.com/wiki/A-100_MLRS) |
| Dong Feng 9000 | míssil de fogo que estoura ~100 m acima do alvo e chove bolas de fogo; alcance o mapa inteiro, **sem revelar o lançador** | $3 000, patente 5 | [wiki](https://zerohourcontra.fandom.com/wiki/Dong_Feng) |
| Zhu Rong (superunidade) | morteiro de magma: 600 + 200, raio 30/50 u (8/13 m), alcance 250 u (66 m); o chão fica queimando | $7 000, 2 750 de vida | [wiki](https://zerohourcontra.fandom.com/wiki/Zhu_Rong) |

### 1.5 Leang (chefe do Challenge, não jogável)

- Árvore: Signal Mines (custa 1 ponto pra ela), os 3 treinos, Cluster Mines, Artillery, Emergency Repair, Frenzy, Cash Hack; na 5, Magnetic, Neutron e Carpet Bomb ([wiki](https://zerohourcontra.fandom.com/wiki/General_%22Tigress%22_Leiong_Leang)).
- **O criativo é o mapa dela** (mesma fonte):
  - uma **estação de rádio** indestrutível chama reforços sem parar; o único jeito de cortar é **capturar** com infantaria;
  - derrubar a **torre de TV** civil apaga o minimapa do jogador por cerca de 10 min.
- **Leitura:** dá pra prender poder a um objeto do mapa (a rádio que chama tropa, a antena que cega). Combina com o nosso campo que muda.

### 1.6 Contra X (beta de 2025)

- Da China, só foi mostrada uma defesa nova do general Nuke ([ModDB, update 4](https://www.moddb.com/news/contra-x-work-in-progress-news-update-4)). Os poderes chineses do Contra X: n/c.

### 1.7 O que o changelog do Contra ensina (curto)

- **Poder que vira chato sai.** Cluster Mines, Cash Hack do Fai, Neutron do Fai, Ionization (Tank e Infantry) e o Flame Paradrop saíram (Revora p38, p39).
- **O nível pode ser pago em dinheiro.** Os Emergency Repair II e III do Kwai são pesquisa no Propaganda Center (Revora p38).
- **A cadeia de pré-requisito dá direção à árvore.** Emperor Delivery exige Tank Drop III.
- **O combo é mais interessante que o número.** O Spill Fuel sozinho não mata; a graça é a ordem: derramar, esperar o inimigo, acender.
- **A IA precisa saber usar.** O Patch 3 ensinou a IA a soltar minas no inimigo, não na própria base, e a não chamar paraquedista dentro da base inimiga (Revora p38, p39).
- **Recargas que aparecem no changelog:** Tank Drop 5:45; Signal Mines 1:00; Mobile Barracks 0:25; Fuel Tube 2:00; Fuel Truck 1:00. Um Carpet Bomb de 7:00 aparece no 008, sem dizer de qual general ([Revora p37](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-37)). O resto: n/c.

---

## 2. ShockWave e Rise of the Reds

### 2.1 ShockWave (1.2; a wiki cita a 1.25)

Números quase não publicados (n/c). O que importa é o que cada general troca.

| General | Patente 1 | Patente 3 | Patente 5 | Fonte |
|---|---|---|---|---|
| **Kwai** (tanques) | Battlemaster Elite Training; destrava o Reaper Tank | Cluster Mines, Artillery Barrage, **Tank Drop** (Battlemasters), Frenzy, Emergency Aid | EMP Pulse, Carpet Bomb | [wiki](https://shockwave.fandom.com/wiki/Kwai) |
| **Fai** (infantaria) | Marksman Training (infantaria elite); destrava o Berzerker MLRS; **o 1º nível do Frenzy já na 1** | **Napalm Strike** (esquadrilha Iron Dragon), Cluster Mines, Artillery, Cash Hack, Frenzy II e III, Infantry Paradrop (Mini-Gunners e Tank Hunters) | EMP Pulse, Carpet Bomb | [wiki](https://shockwave.fandom.com/wiki/Fai) |
| **Tao** (nuclear; só referência) | Red Army Veteran Training, Artillery Training; destrava o Devastator | Cluster Nuke, Cluster Mines, Artillery nuclear, Cash Hack, Frenzy, Emergency Aid | EMP, Nuke Bomber | [wiki](https://shockwave.fandom.com/wiki/Tao) |
| **Leang** (fogo, terremoto e artilharia; jogável no SW) | Red Army Veteran Training; destrava o bombardeiro Phoenix; Artillery Training | **Napalm Strike**, Cluster Mines, Artillery, Cash Hack; **destrava o Hammer Cannon** (terremoto); Emergency Aid | EMP, **Concussion Bombs** (desliga veículos e destrói aviões) | [wiki](https://shockwave.fandom.com/wiki/Leang) |

**O que é criativo no ShockWave**
- **A Leang troca o reator nuclear por usina a carvão** (+8 de energia) e a superarma nuclear pelo Temple of Gaia: o tiro de terremoto desliga os prédios da área e drena a vida deles ([wiki](https://shockwave.fandom.com/wiki/Leang)). É uma China sem nuclear que funciona.
- **Buratino** (o TOS-1 da Leang): 30 foguetes de napalm em 15 s, com a salva redirecionável no meio ([wiki](https://shockwave.fandom.com/wiki/TOS-1_Buratino)).
- **Concussion Bombs:** um golpe que pega o chão (veículos param) e o ar (aviões caem) ao mesmo tempo.
- A doutrina da China no SW: "juggernaut lento", "números" e "propaganda" ([wiki](https://shockwave.fandom.com/wiki/People%27s_Republic_of_China)).

### 2.2 Rise of the Reds (1.87): a promoção da China

Todos os poderes saem do Command Center ([wiki](https://generalsrotr.fandom.com/wiki/Chinese_Arsenal/General%27s_Promotion)).

| Poder | Pat. | Níveis | Recarga | O que faz | Fonte |
|---|---|---|---|---|---|
| Radio Intercepts | 1 | — | passivo | antena no CC que revela de tempos em tempos os veículos inimigos. Antes da 1.87 era um poder: revelava tudo por 2,5 s, recarga 2:00 | [wiki](https://generalsrotr.fandom.com/wiki/Radio_Intercepts) |
| **Mass Mobilization** | 1 | — | passivo | quartel e fábrica treinam **em par**, mais rápido que um por um | [wiki](https://generalsrotr.fandom.com/wiki/Mass_Mobilization) |
| Tactical Nuclear Weapons | 1 | — | passivo | destrava as armas nucleares (fora do nosso escopo) | [wiki](https://generalsrotr.fandom.com/wiki/Tactical_Nuclear_Weapons) |
| Breakthrough Strategy | 1 | — | passivo | destrava os superpesados (Overlord, Shenlong) | [wiki](https://generalsrotr.fandom.com/wiki/Breakthrough_Strategy) |
| **War Propaganda** | 1 | — | passivo | o veículo "herói" ganha holofote e dá **bônus de horda constante**; o Propaganda Center ganha a antena que fala pro **mapa inteiro** | [wiki](https://generalsrotr.fandom.com/wiki/War_Propaganda) |
| Cluster Mines | 3 | — | 4:00 | o cargueiro Y-7 solta o campo | [wiki](https://generalsrotr.fandom.com/wiki/Chinese_Arsenal/General%27s_Promotion) |
| Cash Hack | 3 | — | 3:00 | rouba até $2 000 | idem |
| **Parabombs** | 3 | I–III | 5:00 | o H-8 solta 6 bombas de paraquedas. I: 350→100 de dano, raio 50→80 u (13→21 m). II: 500→200, raio 50→100 u (13→26 m). III: nuclear | [wiki](https://generalsrotr.fandom.com/wiki/Light_Parabombs) |
| **ECM Burst** | 3 | I–III | 4:00 | um satélite de interferência **que você move**: os mísseis inimigos erram, ele atrai o tiro antiaéreo e desliga veículos aos poucos. 15 / 25 / 35 s; raio 80 / 120 / 160 u (21 / 32 / 42 m). Não pode ser atacado | [wiki](https://generalsrotr.fandom.com/wiki/ECM_Burst) |
| Frenzy | 3 | I–III | 3:00 | +40% de dano por 25 / 50 / 100 s | [wiki](https://generalsrotr.fandom.com/wiki/Chinese_Arsenal/General%27s_Promotion) |
| EMP Pulse | 5 | — | 4:00 | o H-8 solta o EMP: veículos e prédios desligados 30 s | idem |
| **Napalm Strike** | 5 | — | 6:00 | 3 H-6 em fila: **tempestade de fogo de raio 100 u (26 m) por 6 s, 200 de fogo por segundo** (300 com Black Napalm) | [wiki](https://generalsrotr.fandom.com/wiki/Napalm_Strike) |

**A massa no RotR, fora dos poderes**
- **Horde Bonus:** 5 ou mais unidades a até 30 u (7,9 m) ganham **+50% de cadência** ([wiki](https://generalsrotr.fandom.com/wiki/Horde_Bonus)). É o mesmo número que o nosso estudo de infantaria tirou do INI do ZH (`docs/ESTUDO_INFANTARIA_DESIGN.md`).
- **Propaganda Center:** raio 200 u (53 m), cura 1% da vida por segundo, +25% de cadência; infantaria 20% mais barata ([wiki](https://generalsrotr.fandom.com/wiki/Propaganda_Center)).
- **Propaganda Airship:** o dirigível de propaganda ($1 200, 640 de vida, até 4) leva o alto-falante pelo ar ([wiki](https://generalsrotr.fandom.com/wiki/Propaganda_Airship)).
- **Volunteer Squad:** 10 voluntários por $1 000, a "bucha de canhão" oficial ([wiki](https://generalsrotr.fandom.com/wiki/Volunteer_Squad)).
- **Na 2.0 (futuro):** o General Jin, da polícia secreta, com guerra eletrônica e propaganda; o Intelligence Bureau atualiza sozinho a posição de todo o inimigo ([wiki Jin](https://generalsrotr.fandom.com/wiki/Secret_Police_General_Jin), [Bureau](https://generalsrotr.fandom.com/wiki/Intelligence_Bureau)).

**Leitura do RotR:** a 1 estrela é doutrina (passiva), a 3 é golpe em 3 níveis, a 5 é o máximo. A propaganda vira identidade: holofote, dirigível, antena pro mapa inteiro.

---

## 3. Outros RTS (só o criativo pra uma China de massa, fogo, propaganda e hacker)

O que já está em `ESTUDO_RTS.md` (Final Squadron, Balloon Bombs, Mass Production e Orbital Drop do RA3; os TA do World in Conflict; os poderes da GDF no Tempest; o radar do Act of War) não se repete.

| Jogo | Poder | O que faz | Números | Fonte | O que serve pra nós |
|---|---|---|---|---|---|
| Red Alert 3 (Império) | **Emperor's Rage** I–III | as unidades ficam mais lentas e mais fortes; no III quase param e quase não morrem | ~30 s; 1 ponto | [wiki](https://cnc.fandom.com/wiki/Emperor%27s_Rage) | a propaganda com preço: a horda vira muralha |
| Red Alert 3 (Império) | Honorable Discharge | todo veículo explode ao morrer | 200 de dano, raio 50 | [wiki](https://cnc.fandom.com/wiki/Honorable_Discharge) | perder também machuca o inimigo |
| Red Alert 3 (Império) | Sleeper Ambush | 5 Tankbusters saem do chão em 10 s, indefesos enquanto saem | só em área explorada | [wiki](https://cnc.fandom.com/wiki/Sleeper_Ambush) | a emboscada com janela de resposta |
| Red Alert 3 (URSS) | **Grinder Treads** | o veículo que esmaga recupera 50% da vida máxima do esmagado | passivo, 1 ponto | [wiki](https://cnc.fandom.com/wiki/Grinder_Treads) | o nosso Overlord é esmagador nível 3 |
| Red Alert 3 (URSS) | Toxic Corrosion | sacrifica um veículo: ele solta veneno por onde passa e explode ao morrer | 5 000 em raio 80 + 40/s por 30 s | [wiki](https://cnc.fandom.com/wiki/Toxic_Corrosion) | o veículo-bomba que o inimigo precisa caçar |
| Red Alert 3 (URSS) | **Cash Bounty** (de área) | marca uma área: o que morrer ali paga 25% do custo a quem marcou; **o alvo marcado fica visível** mesmo fugindo | raio 100 por 30 s; recarga 2:00 | [wiki](https://cnc.fandom.com/wiki/Cash_Bounty_(Red_Alert_3)) | dinheiro + informação num poder só |
| C&C 3 (Nod) | **Radar Jamming Missile** | apaga o minimapa do inimigo por um tempo, sem dano | $750 (TW 1.09), $300 (KW); 1:00 | [wiki](https://cnc.fandom.com/wiki/Radar_jamming_missile) | o hacker que cega |
| C&C 3 (Nod) | **Decoy Army** | cria cópias frágeis das suas unidades; elas não dão XP a quem mata | $500; 1:00 | [wiki](https://cnc.fandom.com/wiki/Decoy_army) | a massa falsa |
| Generals 2 (cancelado, APA) | Misdirection | cópias holográficas das tropas que o inimigo vê como reais | n/c | [wiki](https://cnc.fandom.com/wiki/Unreleased_content_(Generals_2)) | idem |
| Generals 2 (APA) | Teargas, Thunderclap | gás que esconde os seus e piora os do inimigo; onda de choque que trava o movimento | n/c | idem | — (a Rússia já tem a fumaça) |
| Generals 2 (APA) | Offshore Bombardment, Inferno Artillery, Power to the People | artilharia de fora do mapa; artilharia de fogo; "o poder ao povo" (descrição n/c) | n/c | idem | a APA também tinha a artilharia de fogo |
| Generals 2 (APA) | Propaganda Tower | cura, conserta e acelera o tiro; os upgrades aumentam o raio | n/c | [wiki](https://cnc.fandom.com/wiki/Propaganda_tower) | — |
| Tempest Rising (Dynasty) | **Call the Guard** | até 4 prédios seus na área soltam, cada um, 3 Dynasty Guards e 1 Missile Trooper (+1 Assault Gunner se o prédio é melhorado) | $3 000 + 7 de população; 3:00; raio 1 500 (unidade do jogo). O 1.8.0 cortou o Missile Trooper de 2 pra 1 | [wiki](https://tempestrising.wiki.gg/wiki/Call_the_Guard) | **a milícia que sai das casas**, sem avião |
| Tempest Rising (Dynasty) | Lockdown | os prédios da área levam menos dano e não podem ser tomados, mas desligam | n/c | [wiki](https://tempestrising.wiki.gg/wiki/Support_power) | — |
| Tempest Rising (Dynasty) | Planos (Logistics, Martial, Security) | modos de guerra que o Construction Yard roda gastando energia: +15% na coleta, +20% na construção / +10% de cadência e +10% de resistência a foguete e explosivo / −10% de custo, +20% de conserto e radar +40% | energia e duração n/c | [wiki](https://tempestrising.wiki.gg/wiki/Plans) | a "economia de guerra" ligada e desligada |
| Tempest Rising (Dynasty) | Doutrina Manpower | enquanto um Plano roda, cada veículo seu que morre solta um soldado | 3 000 créditos, 1:20 de pesquisa | [wiki](https://tempestrising.wiki.gg/wiki/Doctrine) | a horda que não acaba |
| Company of Heroes 2 (URSS) | **Rapid Conscription** | enquanto dura, o comando repõe de graça os esquadrões de Conscripts perdidos | 6 pontos de comando; números n/c | [wiki](https://companyofheroes.fandom.com/wiki/Soviet_Reserve_Army) | a horda que não encolhe |
| Company of Heroes 2 (URSS) | Incendiary Artillery Barrage | granadas incendiárias que queimam infantaria e **negam a área** | 7 pontos de comando; n/c | [wiki](https://companyofheroes.fandom.com/wiki/Conscripts_Support_Tactics) | outra artilharia de fogo |
| Act of War | economia de prisioneiros | soldados inimigos capturados geram dinheiro num prédio especial | n/c | [Wikipedia](https://en.wikipedia.org/wiki/Act_of_War:_Direct_Action) | — (pesado pra simular) |

**Leitura:** pra massa, o mercado chama gente **perto do que você já tem** (prédios, perdas, o veículo que morre), não de avião. Pro hacker, o mercado **engana e cega** em vez de dar dano. O World in Conflict e o Broken Arrow não trazem nada novo pra China (os poderes são iguais pros dois lados).

---

## 4. A China real de hoje: o que vira poder (nome perto do real)

**Cuidado com o nuclear:** a família DF é "a principal entrega das armas nucleares da China" ([Wikipedia](https://en.wikipedia.org/wiki/Dongfeng_(missile))). O DF-17, o DF-21, o DF-26 e o H-6N têm versão ou suspeita nuclear; o CJ-10 também, segundo os EUA. Recomendo não usar esses nomes.

| Item | O que é (com número) | Que poder viraria | Fonte |
|---|---|---|---|
| **PHL-16** (PCL-191 / PCH-191) | lançador 8×8 com 2 módulos; cada módulo leva 1 míssil de 750 mm (Fire Dragon 480, 500 km) **ou** 4 foguetes de 370 mm **ou** 5 de 300 mm | uma salva cujo **nível troca o módulo** (300 → 370 → 750 mm) | [Wikipedia](https://en.wikipedia.org/wiki/PHL-16) |
| PHL-03 | 12 tubos de 300 mm; 70–130 km; foguete de 800 kg com ogiva de 280 kg | a saraivada de 12 rastros no céu | [Wikipedia](https://en.wikipedia.org/wiki/PHL-03) |
| PCL-181 | obuseiro de 155 mm/52 sobre caminhão; 40 km (72 km com munição estendida) | a bateria que chega de caminhão, atira e vai embora (a artilharia de fogo de tubo) | [Wikipedia](https://en.wikipedia.org/wiki/PCL-181) |
| PLZ-05 | obuseiro de 155 mm de lagarta; 40 km (mais de 50 com munição assistida) | já é o nosso Inferno Cannon | [Wikipedia](https://en.wikipedia.org/wiki/PLZ-05) |
| Type 63 | lançador rebocado de 12 × 107 mm; ~8 km | a saraivada barata, de milícia | [Wikipedia](https://en.wikipedia.org/wiki/Type_63_multiple_rocket_launcher) |
| **H-6K** | o bombardeiro (versão convencional) com mísseis YJ-12, YJ-21 e CJ-10; 231+ construídos (todas as versões) | o Carpet Bomb e o Napalm Strike em fila (usar "H-6K", nunca "H-6N") | [Wikipedia](https://en.wikipedia.org/wiki/Xian_H-6) |
| J-10C | caça com radar AESA e mísseis PL-15 | a escolta ou o caça do Napalm | [Wikipedia](https://en.wikipedia.org/wiki/Chengdu_J-10) |
| J-16D | J-16 de guerra eletrônica, pra suprimir antiaérea: interferidores internos e pods | "abrir a antiaérea" (choca com a Krasukha da Rússia) | [Wikipedia](https://en.wikipedia.org/wiki/Shenyang_J-16) |
| Z-10 | helicóptero de ataque anticarro, mísseis HJ-10 | um varredor de helicóptero; carga n/c | [Wikipedia](https://en.wikipedia.org/wiki/CAIC_Z-10) |
| **Y-20** | cargueiro de 66 t: **2 tanques leves Type 15 ou 1 Type 99A** a 7 800 km | o pouso de blindados (Armor Lift) | [Wikipedia](https://en.wikipedia.org/wiki/Xian_Y-20) |
| **Y-9XZ / Y-8XZ** | **avião de guerra psicológica** (rádio e TV no ar); o Y-9XZ em serviço desde ~2014 | a propaganda chega de avião, e dá pra derrubar | [GlobalSecurity](https://www.globalsecurity.org/military/world/china/y-9gx.htm), [Wikipedia](https://en.wikipedia.org/wiki/Shaanxi_Y-9) |
| KJ-500 | radar aéreo com 3 antenas AESA, cobertura de 360° | olhos (mas é o papel do AWACS da USA) | [Wikipedia](https://en.wikipedia.org/wiki/Shaanxi_KJ-500) |
| **Milícia marítima** | barcos "de pesca" em zona cinzenta; em Whitsun Reef (2021), ~220 barcos ancorados em fila | um enxame civil que bloqueia passagem (em mapa com água) | [Wikipedia](https://en.wikipedia.org/wiki/Maritime_Militia), [Whitsun Reef](https://en.wikipedia.org/wiki/Whitsun_Reef_incident) |
| **Balsas ro-ro civis** | ferries civis feitos sob padrão de defesa; em 2022 lançaram veículos de assalto no mar | o desembarque por balsa civil (mapa com costa ou rio) | [USNI 2022](https://news.usni.org/2022/09/28/chinese-launch-assault-craft-from-civilian-car-ferries-in-mass-amphibious-invasion-drill-satellite-photos-show), [CIMSEC](https://cimsec.org/civilian-shipping-ferrying-the-peoples-liberation-army-ashore/) |
| **Lei de Mobilização de Defesa** | 2010, emendada em 2026: põe a economia e os civis em pé de guerra | a produção de guerra por um tempo | [Wikipedia](https://en.wikipedia.org/wiki/National_Defense_Mobilization_Law) |
| Guerra popular | a estratégia de Mao: a população sangra o invasor | a milícia que sai das casas e prédios | [Wikipedia](https://en.wikipedia.org/wiki/People%27s_war) |
| **Three Warfares** | doutrina de 2003: guerra de opinião, psicológica e jurídica | a propaganda que derruba o moral inimigo | [Wikipedia](https://en.wikipedia.org/wiki/Three_warfares) |
| **APT41** ("Double Dragon") | espionagem + crime financeiro, muito na indústria de videogame | o hacker que sequestra (ransomware) | [Wikipedia](https://en.wikipedia.org/wiki/APT41) |
| **Volt Typhoon** | ativo desde 2021; entra na infraestrutura crítica dos EUA e **fica escondido**, pra sabotar numa crise | implantar agora, apagar depois | [Wikipedia](https://en.wikipedia.org/wiki/Volt_Typhoon) |
| **Salt Typhoon** | invadiu as teles (Verizon, AT&T e outras) e os sistemas de grampo legal; 200+ alvos em 80+ países | o grampo: ver as ordens do inimigo | [Wikipedia](https://en.wikipedia.org/wiki/Salt_Typhoon) |
| Unit 61398 (APT1) | unidade hacker do ELP em Xangai, exposta pela Mandiant em 2013 | nome pro prédio do hacker | [Wikipedia](https://en.wikipedia.org/wiki/PLA_Unit_61398) |
| Silent Hunter | laser antidrone de ~30 kW; o fabricante diz que fura 5 mm de aço a 1 000 m | o contra-poder antidrone | [Wikipedia](https://en.wikipedia.org/wiki/Silent_Hunter_(laser_weapon)) |
| **Hurricane 3000** | micro-ondas de alta potência contra enxame de drones; ~3 km segundo a Norinco; mostrado em Zhuhai, 2024 | o "guarda-chuva" que derruba drone e frita eletrônica | [Wikipedia](https://en.wikipedia.org/wiki/Hurricane_(weapon)) |
| ZTZ-99 / 99A | tanque de 125 mm; **todos têm o JD-3 (ZM-87), laser de autodefesa** que ofusca | o ofuscamento: a mira inimiga perde o alvo | [Wikipedia](https://en.wikipedia.org/wiki/Type_99_tank) |
| ZTQ-15 "Black Panther" | tanque leve; o Y-20 leva 2 | o tanque do pouso de blindados | [Wikipedia](https://en.wikipedia.org/wiki/Type_15_tank) |
| PF-97 | lança-foguete termobárico de 93 mm (cópia licenciada do RPO-A Shmel) | o pelotão termobárico (o fogo da infantaria) | [Wikipedia](https://en.wikipedia.org/wiki/PF-97) |
| Balão de vigilância | jan–fev de 2023: atravessou a América do Norte e foi derrubado por um caça | olhos que andam com o vento | [Wikipedia](https://en.wikipedia.org/wiki/2023_Chinese_balloon_incident) |
| Type 076 | navio de assalto com catapulta eletromagnética, porta-drones | fora (drone) | [Wikipedia](https://en.wikipedia.org/wiki/Type_076_landing_helicopter_dock) |
| DF-21D, DF-17 (DF-ZF), DF-26 | o 1º míssil balístico antinavio; o planador hipersônico (1 600–2 500 km); o "Guam killer" de dupla capacidade | fora (família nuclear) | [DF-21](https://en.wikipedia.org/wiki/DF-21), [DF-17](https://en.wikipedia.org/wiki/DF-17), [DF-26](https://en.wikipedia.org/wiki/DF-26) |

---

## 5. As 15 ideias mais criativas pra nossa China

Já descontei o que a USA tem (drone espião, marca de alvo, A-10, AC-130, Paradrop, SEAL, artilharia naval, MOAB, AWACS, Guardian
Angel, posto avançado) e o que o rumo da Rússia já pegou (Krasukha, fumaça, barragem que anda, Tornado-S, VDV). As do rumo da China
(§8.2 de `PODERES_DO_GENERAL.md`) entram só quando o estudo traz uma mudança.

**1. Fuel Drop + a artilharia de fogo (o combo do Bao)**
- **O que é:** o cargueiro risca uma faixa de diesel no chão (I: 3 poças, II: 6, III: 9). A poça fica lá, brilhando escura. Qualquer fogo acende: o Inferno Cannon, o napalm, um tanque que explode em cima.
- **De onde veio:** Spill Fuel e Volume Detonation do Bao (Contra); a lição do changelog: o combustível nega, a artilharia acende.
- **Por que é bom de cima:** a faixa se lê de longe, o inimigo vê e decide (atravessar rápido, desviar, ou atirar no avião). Quem joga escolhe a hora da explosão. O chão fica queimado (o campo que muda).
- **Arrasa em massa?** Sim: o nosso Inferno Cannon já existe e vira o fósforo.

**2. Incendiary Rocket Salvo (PHL-16)**
- **O que é:** o lançador entra pela borda do seu lado, abre os módulos e dispara. O nível troca o módulo: I, 10 foguetes de 300 mm; II, 8 de 370 mm; III, 2 mísseis de 750 mm que se abrem em chuva de fogo no ar.
- **De onde veio:** a Artillery Barrage incendiária do Bao, o A-100, o Dong Feng 9000 (chuva de fogo), o Buratino (ShockWave); o PHL-16 real.
- **Por que é bom de cima:** os rastros no céu avisam e mostram de onde vem; o Guardian Angel da USA pode derrubar os foguetes (uma resposta entre facções).
- **Arrasa em massa?** Sim. **Atenção:** a Rússia tem o Tornado-S na patente 5. A diferença precisa ser o que fica no chão: fogo (China) contra cratera e submunição (Rússia).

**3. Firestorm (a massa de fogo vira tempestade)**
- **O que é:** um treino passivo. Quando 5 ou mais fogos caem juntos e perto, eles se fundem numa tempestade de fogo maior, que dura mais e puxa a coluna de fumaça.
- **De onde veio:** o Flame Combustion do Contra (5 Howitzers fazem a tempestade); o Napalm Strike do RotR (raio 26 m, 6 s). A física real: as tempestades de fogo nascem de muitos focos juntos.
- **Por que é bom de cima:** premia a horda de artilharia junta, e a coluna de fumaça na luz do mundo é o "uau" da China.
- **Arrasa em massa?** É a frase da facção, ao pé da letra.

**4. Wartime Mobilization (o reator no talo)**
- **O que é:** por 60 s, as fábricas produzem 2× mais rápido. O preço: os reatores entram em sobrecarga, soltam vapor e brilham. Um reator atingido nesse tempo derrete com mais força.
- **De onde veio:** a lei de mobilização (2010), a sobrecarga do reator do Contra (+50% de energia), os Planos da Dynasty, o Mass Mobilization do RotR. A nota da USA (§9) já dizia que a produção acelerada cabe na China.
- **Por que é bom de cima:** o vapor e o brilho mostram ao inimigo onde bater. Risco e prêmio, legíveis.
- **Arrasa em massa?** Sim, e liga a mecânica-assinatura da China (o reator que derrete) aos poderes.

**5. Replacement Draft (a horda não encolhe)**
- **O que é:** por 30 s numa área, cada soldado chinês que morre é reposto por um recruta que chega correndo da borda ou do quartel mais perto. Paga metade do preço (tropa chamada custa).
- **De onde veio:** Rapid Conscription (CoH2), Cloning (Contra, mais realista aqui), Manpower (Tempest).
- **Por que é bom de cima:** a linha não abre; quem ataca vê a fila de reforço chegando e pode cortar o caminho.
- **Arrasa em massa?** É a horda em forma de poder.

**6. Militia Call-up (a guerra popular)**
- **O que é:** escolha uma área; até 4 prédios seus ali soltam milícia cada um (3 fuzileiros e 1 caçador de tanque). Paga por uso.
- **De onde veio:** Call the Guard (Tempest Rising), a guerra popular de Mao, o Volunteer Squad (RotR).
- **Por que é bom de cima:** não tem avião pra derrubar; a resposta é não deixar a briga chegar na base, ou bater nos prédios antes. Dá à China uma defesa que nasce da própria base.
- **Arrasa em massa?** Sim, na defesa. Não repete o Ambush da GLA (que surge em qualquer ponto).

**7. Y-20 Armor Lift (os tanques que pousam)**
- **O que é:** o Y-20 pousa ou solta blindados. I: 2 tanques leves ZTQ-15. II: 2 Y-20. III: 1 Type 99A por avião. A versão do general Tank leva o Overlord no fim da cadeia.
- **De onde veio:** Tank Drop → Emperor Delivery (Contra; recarga 5:45); o Y-20 real (2 Type 15 ou 1 Type 99A).
- **Por que é bom de cima:** o avião gordo e lento é um alvo enorme; cair com 2 tanques dentro dói. A USA solta gente, a China solta ferro.
- **Arrasa em massa?** É o "pesado". **Atenção:** o VDV III da Rússia traz um BMD de paraquedas; separar pelo peso (tanque contra blindado leve), ou deixar o Armor Lift pro general Tank.

**8. Ro-Ro Landing (a balsa civil)**
- **O que é:** em mapa com costa ou rio, uma balsa de carros civil encosta e despeja uma coluna de veículos. Pode ser o nível III do Reinforcement Convoy: por estrada ou, se o mapa deixar, por água.
- **De onde veio:** os ferries ro-ro da fusão civil-militar (exercícios de 2020 a 2023), o Offshore Bombardment da APA.
- **Por que é bom de cima:** a balsa é um prédio flutuante que dá pra afundar antes de abrir a rampa. Muda o jogo por tipo de mapa (a regra dos tipos de mapa).
- **Arrasa em massa?** Sim: uma coluna inteira de uma vez.

**9. Psyops Broadcast (o Frenzy refeito, de avião)**
- **O que é:** o Y-9XZ circula a área com os alto-falantes. Os seus ali ganham dano (+25 / +50 / +100% por 30 / 45 / 60 s, a régua do Contra). Os soldados inimigos perdem cadência e andam mais devagar.
- **De onde veio:** o Frenzy do Contra e do RotR, o Emperor's Rage (RA3), as Three Warfares, o Y-9XZ real de guerra psicológica.
- **Por que é bom de cima:** o avião girando lê como "aqui tem propaganda"; derrubar o avião corta o efeito. O efeito vale pros dois lados da briga.
- **Arrasa em massa?** Sim: o bônus vale mais quanto mais gente estiver embaixo.

**10. Signal Flares (o campo-alarme)**
- **O que é:** o cargueiro semeia uma linha de minas-sinal num caminho. Quando o inimigo passa, sobe um sinalizador que revela o grupo por 10 s e acende a área. À noite, a luz acende a fumaça.
- **De onde veio:** Signal Mines do Kwai (Contra, 1:00; de graça pro Kwai).
- **Por que é bom de cima:** olho barato de 1 ponto, sem drone, que vale pela posição. O sinalizador é visível pros dois lados.
- **Arrasa em massa?** Não diretamente; resolve o ponto fraco da China (pouca informação, segundo a doutrina do ShockWave).

**11. Surveillance Balloon (o balão que anda com o vento)**
- **O que é:** um balão de grande altitude atravessa o mapa na direção do vento e revela uma faixa larga embaixo dele. Só a antiaérea de longo alcance ou um caça derruba.
- **De onde veio:** o incidente do balão de 2023.
- **Por que é bom de cima:** lento, visível, meio imprevisível, e engraçado. A faixa revelada é fácil de ler.
- **Arrasa em massa?** Não; é o "olhos" da China com a cara dela, sem drone.

**12. Ransomware (o hacker que sequestra)**
- **O que é:** o hacker trava um prédio inimigo (a fábrica para, a defesa desliga). O dono escolhe: paga o resgate (que vai pra China) e o prédio volta na hora, ou espera 40 s.
- **De onde veio:** a APT41 (espionagem + dinheiro), o Cash Hack do ZH e do Contra, o Temporary Blackout do Contra (GLA, 15 s).
- **Por que é bom de cima:** a vítima decide (o contra-jogo é a escolha). Substitui o Cash Hack puro, cujos níveis II e III dividiam a comunidade do ZH (`ESTUDO_ZH.md` §7).
- **Arrasa em massa?** Indireto: dinheiro pra horda, e a fábrica inimiga parada.

**13. Grid Implant (implantar agora, apagar depois)**
- **O que é:** o hacker implanta um backdoor numa usina inimiga. O implante aparece pro dono como um chiado no prédio, e o Dozer pode limpar. Se sobreviver, a China dispara quando quiser: a base inimiga fica sem energia por 20 s (defesas lentas, radar desligado).
- **De onde veio:** a Volt Typhoon (pré-posicionamento pra sabotar numa crise); o Radar Jamming Missile (C&C3).
- **Por que é bom de cima:** dois tempos legíveis. O inimigo tem tempo de achar e limpar; o atacante guarda o apagão pra hora do ataque.
- **Arrasa em massa?** Prepara a onda: apaga as defesas na hora em que a horda chega.

**14. Phantom Column (o exército falso)**
- **O que é:** o minimapa e a névoa do inimigo mostram colunas chinesas que não existem, numa rota que você risca, por 40 s.
- **De onde veio:** Decoy Army (C&C3), Misdirection (Generals 2), o holograma da Black Lotus (Contra).
- **Por que é bom de cima:** custa só ícones e fantasmas na névoa. A visão de verdade (o drone da USA, o Orlan russo) desmascara.
- **Arrasa em massa?** Sim, na percepção: a China parece mais numerosa do que é.

**15. Microwave Umbrella (o contra-poder da China)**
- **O que é:** um caminhão de micro-ondas entra e abre uma cúpula por 30 s. Drones caem; aviões de poder que atravessam levam dano pesado; mísseis guiados ali dentro erram.
- **De onde veio:** o Hurricane 3000 (~3 km contra enxame) e o Silent Hunter; o ECM Burst do RotR; a Magnetic Bomb do Contra.
- **Por que é bom de cima:** a China não tem contra-poder (o buraco de §8.5). O caminhão está no mapa e dá pra caçar.
- **Arrasa em massa?** Protege a massa do golpe de precisão da USA. **Atenção:** separar da Krasukha russa (a Krasukha cega à distância; o micro-ondas queima de perto).

**Também valem (guardados)**
- **Wiretap** (Salt Typhoon + Radio Intercepts do RotR): por 30 s, você vê as setas de ordem de movimento do inimigo numa área.
- **Cloning** (Contra): mais forte que o Replacement Draft, mas é ficção científica.
- **Grinder Treads** (RA3) pro Overlord: esmagar conserta.
- **Dazzle** (o JD-3 do Type 99): ofusca a mira inimiga numa área.
- **Magnetic Bomb** (Contra): desliga veículos + escudo antimíssil; choca com o EMP Pulse que já está na patente 5.
- **Deploy Barracks** (Contra, 5 s): repete o posto avançado da USA.

**O que o estudo muda no rumo da §8.2**
- **O Frenzy refeito** ganha números de régua (Contra) e chega de avião (Y-9XZ).
- **O Cash Hack** vira o Ransomware.
- **As Cluster Mines** podem virar as Signal Flares (o Contra tirou as Cluster Mines da China no Patch 3).
- **O Napalm Strike** casa com o Fuel Drop e o Firestorm.
- **A China ganha o contra-poder** que faltava (o Microwave Umbrella).

---

## Fontes principais

- **Contra:** wiki pela API ([Kwai](https://zerohourcontra.fandom.com/wiki/General_Ta_Hun_Kwai), [Fai](https://zerohourcontra.fandom.com/wiki/General_Shin_Fai), [Tao](https://zerohourcontra.fandom.com/wiki/General_Tsing_Shi_Tao), [Bao](https://zerohourcontra.fandom.com/wiki/General_Huang_Lin_Bao), [Leang](https://zerohourcontra.fandom.com/wiki/General_%22Tigress%22_Leiong_Leang), [Support Aircraft](https://zerohourcontra.fandom.com/wiki/Support_Aircraft), [Promoção](https://zerohourcontra.fandom.com/wiki/About_General%27s_Promotion)); changelog no Revora ([p37](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-37), [p38](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-38), [p39](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-39)).
- **ShockWave:** [Kwai](https://shockwave.fandom.com/wiki/Kwai), [Fai](https://shockwave.fandom.com/wiki/Fai), [Tao](https://shockwave.fandom.com/wiki/Tao), [Leang](https://shockwave.fandom.com/wiki/Leang).
- **Rise of the Reds:** [promoção da China](https://generalsrotr.fandom.com/wiki/Chinese_Arsenal/General%27s_Promotion) e as páginas de cada poder citadas no texto.
- **Outros RTS:** [Top Secret Protocols do RA3](https://cnc.fandom.com/wiki/Top_Secret_Protocol), [conteúdo cancelado do Generals 2](https://cnc.fandom.com/wiki/Unreleased_content_(Generals_2)), [APA](https://cnc.fandom.com/wiki/Asia-Pacific_Alliance), [support powers do Tempest Rising](https://tempestrising.wiki.gg/wiki/Support_power), [comandantes soviéticos do CoH2](https://companyofheroes.fandom.com/wiki/Soviet_Reserve_Army).
- **China real:** as páginas da Wikipedia citadas na §4; [USNI](https://news.usni.org/2022/09/28/chinese-launch-assault-craft-from-civilian-car-ferries-in-mass-amphibious-invasion-drill-satellite-photos-show) e [CIMSEC](https://cimsec.org/civilian-shipping-ferrying-the-peoples-liberation-army-ashore/) (balsas); [GlobalSecurity](https://www.globalsecurity.org/military/world/china/y-9gx.htm) (Y-9XZ).
- **Bloqueados ou fracos:** a ModDB (Cloudflare), o site do ShockWave (certificado quebrado) e as notícias do Contra X sobre a China (nada publicado).
