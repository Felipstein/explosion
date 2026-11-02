# O elenco jogável das 3 facções (31/10/2026)

O pedido dele (31/10, à noite): "faça todo o elenco mínimo pra ficar jogável de cada facção, as construções, as unidades e a
infantaria [...] centro de tecnologia e mais, que o mod Contra tem [...] sair fora da casinha do C&C ZH [...] poder e fogo
BALANCEADOS [...] cada construção muito bem pensada, muito bonita, muito detalhada [...] só a superarma fica de fora".

Este é o plano e a ficha de cada peça. A lista de quem foi feito, testado e onde está fica no fim (§9); a avaliação é dele, um
por um.

**As fontes dos números:** os INI originais do ZH 1.04 (TheSuperHackers/GeneralsGamePatch, `Patch104pZH/GameFilesOriginalZH`:
`FactionBuilding.ini`, `ChinaVehicle.ini`, `GLAVehicle.ini`, `ChinaInfantry.ini`, `GLAInfantry.ini`, `AmericaVehicle.ini`,
`AmericaInfantry.ini`, `Weapon.ini`, `Armor.ini`, `Locomotor.ini`), lidos em 31/10. 1 unidade do ZH = 0,2633 m (a régua do
`Units.json`). O ZH é o equilíbrio provado em 20 anos de partida; partimos dele e mexemos só onde o nosso jogo é diferente, com o
motivo escrito.

---

## 1. A identidade de cada facção

A proposta do rascunho (`docs/VISAO_DO_JOGO_RASCUNHO.md` §3), afinada: **cada facção tem a sua relação com o campo que muda.**

| | USA | China | GLA |
|---|---|---|---|
| **Uma frase** | Vê tudo, acerta primeiro | Arrasa em massa | Vive da ruína |
| **Como joga** | Pouco, caro e preciso; o ar; a informação | Muito, barato e pesado; a horda; o fogo | Rápido, barato e esperto; a emboscada; recicla o que sobra |
| **A economia** | Caminhão, Chinook, Drop Zone | Caminhão, reator que dá muita energia (e explode) | Picape da carga e o Worker que **transforma carcaça em dinheiro** |
| **A energia** | Reator de fusão (+5) | Reator nuclear (+10, **e derrete quando cai**) | **Nenhuma**: nada da GLA precisa de energia |
| **A tecnologia** | Strategy Center (o prato de satélite) | Propaganda Center (a torre de alto-falantes) | Palace (o palácio de cúpula) |
| **O pesado** | Paladin (o tanque com defesa a laser) | Overlord (o tanque gigante de dois canos) | Rocket Buggy (o enxame de foguetes) |
| **A cor** | cinza-claro, azul | oliva escuro, vermelho | areia, ferrugem, verde |
| **De cima** | prédios claros, quinas retas, pratos e domos | blocos escuros, telhados em camadas, torres | pátios de adobe, lonas, redes de camuflagem |

---

## 2. A regra do equilíbrio

1. **Os números do ZH, convertidos.** Custo, tempo, vida, visão, alcance, cadência e dano saem do INI. Velocidade e alcance ×
   0,2633 m.
2. **O triângulo de dano do ZH** (`Armor.ini`), que a simulação já tem:
   - o perfurante (canhão) faz 10% no soldado;
   - o fuzil faz 25% no tanque e 50% no prédio;
   - o esmagamento faz 200% no soldado;
   - **novos tipos (§7.1):**
     - a **metralhadora rotativa** faz 10% no tanque e no prédio e 100% no soldado;
     - o **fogo** faz 150% no soldado e 25% no tanque;
     - o **veneno** faz 100% no soldado, 25% no tanque e quase nada no prédio.
3. **A conta de cada duelo** (§8): a força de um exército é a vida × o dano por segundo dividido pelo custo² (a lei de
   Lanchester). Unidades do mesmo papel em facções diferentes ficam dentro de ±15% nessa conta; a diferença vai pra velocidade,
   alcance e jeito.
4. **A prova é a partida IA × IA** de cada par de facções (§8.3), no jogo de verdade: quem ganha, em quanto tempo, com que
   exército. Nenhuma facção pode ganhar sempre.

---

## 3. USA (o que já existe e o que entra)

O que já existe fica como está (aprovado na v4 e na v5). Entram:

| Peça | Papel | Números | Requer | O que é novo |
|---|---|---|---|---|
| **Strategy Center** | a tecnologia | $2 500, 60 s, 1 500 de vida, −2 de energia, visão 105 m (ZH) | War Factory | Já tinha modelo (22/10), fora do jogo. Agora destrava o **Paladin** e o **Javelin**. A palavra: o prato de satélite girando sobre o bunker octogonal. |
| **Paladin** | o tanque pesado | $1 100, 12 s, **650 de vida** (o ZH tem 500 e o laser de defesa de ponto; sem o laser, 500 saía ~30% pior por dólar), 6,6 m/s, canhão 60 a cada 2 s a 39,5 m | War Factory + Strategy Center | A unidade reservada (a torre em cunha do design 3 de `usa_mbt_exploration.webp`). O tanque básico com mais casco, pro fim de jogo. |
| **Javelin** | o antitanque a pé | $300, 5 s, 100 de vida, míssil 40 a cada 1 s a 46 m, atira no ar (o Missile Defender do ZH) | Barracks + Strategy Center | O contra exato do tanque: 3 Javelins ($900) matam um tanque ($900) em 4 s. |

---

## 4. China: arrasa em massa

**A cara:** concreto escuro puxado pro oliva, telhados de pagode em camadas verdes com borda vermelha, estrela vermelha, torres de
alto-falantes, forno aceso, estandartes (3 ou 4 por prédio, nunca mais). A cor do jogador é o vermelho
(`docs/CONCEITO_CONSTRUCOES.md` §2).

### 4.1 Construções

| Prédio | A palavra (o que se lê de cima) | Números (ZH) | Requer | Produz / faz |
|---|---|---|---|---|
| **Command Center** | o salão de pagode de 3 telhados com a torre de alto-falante e a estrela no pátio | $2 000, 45 s, 5 000 de vida, visão 79 m | — | Dozer; liga os poderes |
| **Nuclear Reactor** | **duas torres de resfriamento** em ampulheta soltando vapor, com o domo do reator no meio | $1 000, 10 s, 1 500 de vida, **+10 de energia** | — | Energia. **Derrete quando cai** (§7.2) |
| **Supply Center** | o armazém comprido com a ponte rolante vermelha sobre a doca de descarga | $1 500, 10 s, 2 000 de vida, −1 | Reator | Caminhão |
| **Barracks** | o pátio de treino murado com o portão de pagode e o mastro da bandeira | $500, 10 s, 1 000 de vida | — | Fuzileiro (em par), Caçador de tanque |
| **War Factory** | a ficha aprovada: o hangar escuro com o telhado de pagode, o forno aceso e a estrela no piso | $2 000, 15 s, 2 000 de vida, −1 | Supply Center | Type 99, Gattling, Inferno, Overlord |
| **Propaganda Center** (tecnologia) | **a torre de alto-falantes gigante** com o telão vermelho e a estrela, sobre o bloco de concreto | $2 000, 45 s, 1 000 de vida, −2 | War Factory | Destrava Inferno e Overlord |
| **Gattling Cannon** (defesa) | o bunker redondo com a metralhadora de 6 canos | $1 200, 25 s, 1 000 de vida, −3; chão 10 a cada 0,25 s a 59 m, ar 5 a cada 0,25 s a 105 m | Reator | Anti-infantaria e antiaéreo |

### 4.2 Veículos

| Unidade | Real (perto do real) | Papel | Números (ZH) | Requer |
|---|---|---|---|---|
| **Dozer** | o trator de esteira chinês | constrói | $1 000, 5 s, 250 de vida, 7,9 m/s | — |
| **Supply Truck** | o caminhão 6×6 do ELP | coleta | $600, 10 s, 300 de vida, 10,5 m/s | Supply Center |
| **Type 99** (o Battlemaster da ficha) | ZTZ-99 | tanque básico | $800, 10 s, 400 de vida, 6,6 m/s, canhão 60 a cada 2 s a 39,5 m | War Factory |
| **Gattling Tank** | PGZ-09 (o antiaéreo de canos) | anti-infantaria e antiaéreo | $800, 10 s, 300 de vida, 10,5 m/s; chão 15 a cada 0,4 s a 39,5 m; ar 12 a cada 0,4 s a 92 m | War Factory |
| **Inferno Cannon** | PLZ-05 (o obuseiro de cano longo) | artilharia | $900, 15 s, 120 de vida, 7,9 m/s; granada incendiária: 30 de fogo em área (2,6 m cheio, 5,3 m na borda) a cada 4 s, de 13 a 79 m | War Factory + Propaganda |
| **Overlord** | o "tanque chinês gigante" (a escapada do mod Contra) | o pesado | $2 000, 20 s, 1 100 de vida, 5,3 m/s; 2 canos de 80 (rajada de 2) a 46 m; **esmagador nível 3**: passa por cima de carro, poste, árvore e soldado (esmagar veículo vivo fica pra depois: a simulação ainda não tem) | War Factory + Propaganda |

### 4.3 Infantaria

| Soldado | Papel | Números (ZH) |
|---|---|---|
| **Fuzileiro do ELP** (o Red Guard) | massa barata; **sai em par** (o QuantityModifier do ZH, a mecânica do Contra que ele pediu) | $300 o par, 10 s, 120 de vida cada, 6,6 m/s; 15 a cada 1 s a 26 m |
| **Caçador de tanque** | antitanque e antiaéreo | $300, 5 s, 100 de vida; foguete 40 a cada 1 s a 46 m |

---

## 5. GLA: vive da ruína

**A cara:** adobe cor de areia, chapa enferrujada, lona, rede de camuflagem, bandeiras verdes, rabiscos verdes pintados à mão, a
unidade saindo de rampa debaixo da terra. A cor do jogador é o verde (`docs/CONCEITO_CONSTRUCOES.md` §2).

**Sem energia:** nada da GLA consome nem gera. O preço disso é que os prédios dela não têm a vida extra de concreto: o pátio de
adobe cai mais fácil (§8).

### 5.1 Construções

| Prédio | A palavra | Números (ZH) | Requer | Produz / faz |
|---|---|---|---|---|
| **Command Center** | o pátio murado de adobe com o prédio de cúpula baixa e a antena de rádio torta | $2 000, 45 s, 5 000 de vida | — | Worker; liga os poderes |
| **Supply Stash** | **a pilha de caixas debaixo da lona e da rede de camuflagem**, com a rampa do túnel | $1 500, 10 s, 2 000 de vida | — | Picape da carga |
| **Barracks** | a casa de adobe com o pátio de treino de pneus e sacos de areia | $500, 10 s, 1 000 de vida | — | Rebel, RPG (o Worker sai do CC, como no ZH) |
| **Arms Dealer** (a War Factory) | a ficha aprovada: o armazém fechado, a gaiola de armas e a rampa sob a rede | $2 500, 15 s, 2 000 de vida | Supply Stash | Technical, Scorpion, Quad Cannon, Toxin Tractor, Rocket Buggy |
| **Palace** (tecnologia) | **o palácio de cúpula verde** com as colunas e o pátio de fonte | $2 500, 45 s, 3 000 de vida | Arms Dealer | Destrava o Rocket Buggy |
| **Stinger Site** (defesa) | o ninho de sacos de areia redondo com os lança-mísseis | $900, 15 s, 1 000 de vida; chão 20 a cada 2 s a 59 m, ar 30 a cada 2 s a 105 m | Barracks | Antiaéreo e anti-veículo leve |

### 5.2 Veículos

| Unidade | Real | Papel | Números (ZH) | Requer |
|---|---|---|---|---|
| **Supply Pickup** (nova, §7.4) | a picape velha carregada de caixas | coleta | $400, 6 s, 200 de vida, 11 m/s, **3 caixas** (o caminhão leva 4) | Supply Stash |
| **Technical** | (já existe) | o raide | já no jogo | Arms Dealer |
| **Scorpion** | (já existe: o T-55 com o kit) | tanque | já no jogo | Arms Dealer |
| **Quad Cannon** | ZPU-4 na caçamba do caminhão | anti-infantaria e antiaéreo | $700, 6 s, 300 de vida, 10,5 m/s; tiro rotativo (10% no tanque): chão 10 a cada 0,1 s a 39,5 m; ar 5 a cada 0,1 s a 92 m | Arms Dealer |
| **Toxin Tractor** | o trator agrícola com o tanque de veneno (a ideia guardada) | anti-infantaria | $600, 5 s, 240 de vida, 7,9 m/s; veneno contínuo: 10 em área (2,6 m) a cada 0,25 s, a 26 m (1% no prédio: não limpa prédio) | Arms Dealer |
| **Rocket Buggy** | o buggy de areia com os tubos de foguete | artilharia rápida | $900, 10 s, 120 de vida, 23,7 m/s; 6 foguetes de 20 a cada 6 s, de 13 a 79 m | Arms Dealer + Palace |

### 5.3 Infantaria

| Soldado | Papel | Números (ZH) |
|---|---|---|
| **Worker** | constrói, **desmancha carcaça em dinheiro** (§7.3) | $200, 3 s, 100 de vida, 7,9 m/s |
| **Rebel** | o barato; emboscada | $150, 5 s, 120 de vida, 5,3 m/s; rajada de 3 × 5 a 26 m; vê 39,5 m |
| **RPG** | o antitanque barato | $300, 5 s, 100 de vida; foguete 40 a cada 1 s a 46 m, atira no ar |

---

## 6. A árvore de cada facção (o que destrava o quê)

```
USA    CC ─ Dozer ─┬─ Reator ─ Supply Center ─ War Factory ─┬─ Strategy Center ─ Paladin, Javelin
                   │                                        ├─ Airfield ─ Raptor, Apache, Chinook
                   ├─ Barracks ─ Ranger                     ├─ Drop Zone
                   └─ Patriot (com o reator)                └─ (patente 3) Tomahawk
CHINA  CC ─ Dozer ─┬─ Reator nuclear ─ Supply Center ─ War Factory ─ Propaganda Center ─ Inferno, Overlord
                   ├─ Barracks ─ Fuzileiro (par), Caçador de tanque
                   └─ Gattling Cannon (com o reator)
GLA    CC ─ Worker ┬─ Supply Stash ─ Arms Dealer ─ Palace ─ Rocket Buggy
                   ├─ Barracks ─ Rebel, RPG
                   └─ Stinger Site (com o quartel)
```

---

## 7. As mecânicas novas (o que mexe na simulação)

### 7.1 Três tipos de dano novos

`gattling`, `flame` e `poison`, com as porcentagens do `Armor.ini` (§2). Sem eles, a metralhadora de 6 canos da China faria 2,5×
mais no tanque do que no ZH (25% do fuzil contra 10% dela) e o Gattling Tank viraria um caça-tanque.

### 7.2 O reator que derrete (China)

Quando o Nuclear Reactor cai, ele explode: **o dano de morte** (`death_blast` nos dados) em tudo em volta, de qualquer dono.
- **Os números do ZH:** o `ChinaPowerPlant` solta a `ChinaPowerPlantDeathWeapon`, com 400 explosivo até 13 m e 50 até 26 m, em
  aliado, inimigo e neutro.
- **É a escolha da China:** o reator dá o dobro da energia, mas onde ele fica vira perigo. Atrás da base? No meio dos prédios?
  Colado nas defesas?
- **De cima:** o domo racha, vem o clarão e sobe a coluna de vapor.

### 7.3 O Worker que vive da ruína (GLA)

O Worker é o construtor da GLA e, como o Dozer, **desmancha a carcaça em dinheiro** (o sucateio que a simulação já tem,
`TestSalvage`: decidido por ele em 30/10). Pra GLA é a identidade: a batalha deixa carcaças, o Worker transforma em caixas.

### 7.4 A picape da carga (GLA)

No ZH o Worker também coleta. Aqui a coleta fica com a **picape velha** (mais barata e mais frágil que o caminhão): o Worker fica
construindo e sucateando, e a economia da GLA é a mais barata e a que mais sofre com raide.

---

### 7.5 A IA das 3 facções (simulação 33)

A IA de regras (`SkirmishAI.cpp`) acha os papéis pelos dados, não pelo nome; o que precisou pra ela jogar com a China e a GLA:
- **A GLA não tem usina:** a lista fixa (usina → centro → fábrica → defesa) pula o papel que nenhum construtor dela sabe fazer, e a
  conta de energia só vale pra quem tem usina. Sem isso, a IA da GLA pedia a usina pra sempre e não construía nada.
- **A defesa que pede pré-requisito** (o Stinger Site pede o quartel) espera o pré-requisito em vez de travar a lista.
- **O prédio de tecnologia** (papel novo: o prédio que não treina nem atira, mas é exigido por uma unidade armada): um só, depois da
  fábrica, com a 1ª onda fora ou a partir de 5 min, e o preço dele + um tanque sobrando.
- **A mistura do exército:** antes a fábrica treinava só o 1º armado da lista (a China seria só Type 99). Agora treina o que ela
  tem menos, com o 1º da lista (o tanque) valendo por 2; a artilharia e o antiaéreo seguem as regras deles. No quartel, o mesmo
  (o fuzil vale por 2; o Red Guard conta por par).
- **O contra (01/11, depois do 1º torneio):** a IA guarda o exército de terra inimigo visto por último (o custo por blindagem) e, na
  fábrica e no quartel, a mistura só leva o armado que faz ao menos metade do % de dano do melhor que ela pode comprar ali contra esse
  exército (sem nada visto, contra tanque). Contra tanque, a GLA para de gastar metade em Technical e Toxin Tractor (25%), a Rússia
  no BMPT e a China no Gattling Tank (10%); contra soldado, o contrário. Só o multiplicador de dano, não o dano por segundo (com o
  dano por segundo o Overlord, contado sem o pente, tirava até o Type 99). `SkirmishAI.cpp` (`Versus`, `BestVersus`).

---

## 8. O equilíbrio: as contas

### 8.1 Os tanques (força de Lanchester = vida × dano/s ÷ custo²)

| Tanque | Custo | Vida | Dano/s no tanque | Força | Velocidade |
|---|---|---|---|---|---|
| USA Battle Tank | 900 | 480 | 30 | 0,0178 | 7,9 m/s, ricochete |
| China Type 99 | 800 | 400 | 30 | 0,0188 | 6,6 m/s |
| GLA Scorpion | 600 | 370 | 20 | 0,0206 | 10,5 m/s |
| USA Paladin | 1 100 | 650 | 30 | 0,0161 (mais casco por vaga de exército) | 6,6 m/s, ricochete |
| China Overlord | 2 000 | 1 100 | 69,6 | 0,0191 | 5,3 m/s, esmagador nível 3 |

**A leitura:** dentro de ±15%, como pede a regra. O Scorpion é o mais eficiente por dólar, mas morre mais rápido sozinho e
precisa do grupo. O USA tem o ricochete (25% do dano de raspão) e a melhor velocidade de giro, e o Overlord é lento.

### 8.2 Os contras (quem mata quem em quanto tempo, pela conta)

| Ataque | Alvo | Tempo |
|---|---|---|
| 3 Javelin / 3 Caçador / 3 RPG ($900) | 1 tanque USA ($900) | 4,0 s |
| 1 Gattling Tank | 4 Rangers (180 de vida cada) | ~19 s (15 ÷ 0,4 s = 37,5/s × 100%) |
| 1 Quad Cannon | 4 Rebels | 4,8 s (10 ÷ 0,1 s = 100/s) |
| 1 Gattling Tank | 1 tanque USA | 128 s (3,75/s: o canhão rotativo não é caça-tanque) |
| 1 Overlord | 2 tanques USA | ~14 s (e leva ~16 s de dano dos dois) |
| 1 Gattling Cannon | 1 Raptor (160 de vida; o rotativo faz 100% no avião) | 8 s |

### 8.3 A prova na partida

A partida IA × IA (`game.ps1 view ... -Script aivsai -Extra "-RTSSelfAI -RTSFaction=china -RTSAIFaction=gla"`) dos 6 pares de
facções. Os resultados ficam em §9.

**01/11, sem a placa** (`-nullrhi`; o teste agora começa sem o botão do menu e grava o placar a cada minuto; a memória
`reference-ia-x-ia-sem-placa`): 10 partidas de 15 min, cada par dos dois lados (`captures/game/aivsai_r1/`):

| Par | Resultado |
|---|---|
| Rússia × China | a Rússia vence as duas (12,5 e 13,6 min) |
| Rússia × GLA | a Rússia vence as duas (10 min) |
| Rússia × USA | a USA vence as duas (~12,7 min; numa, virada no fim) |
| China × GLA | a China vence as duas (8–10 min) |
| USA × China | ninguém aos 15 min (a USA na frente, $15,7 mil × $7,2 mil de exército) |
| USA × GLA | a USA vence (14 min) |

A ordem com a IA daquele dia: USA > Rússia > China > GLA; o lado não muda o resultado. A Rússia fica no meio. A GLA perdia todas:
não pelos números (por dólar ela está na régua), mas pela mistura da IA (metade da fábrica em Technical e Toxin Tractor, 25% contra
a blindagem de tanque). O conserto (o contra, §7.5) e a 2ª rodada abaixo.

**A 2ª rodada, com o contra** (`captures/game/aivsai_r2/`; entre parênteses, quem estava na frente no empate aos 15 min):

| Par | 1ª rodada | 2ª rodada |
|---|---|---|
| Rússia × China | Rússia 2 × 0 | China vence uma; empate (Rússia na frente, $10,1 mil × $3,1 mil) |
| Rússia × GLA | Rússia 2 × 0 | GLA vence uma; empate (Rússia na frente, $7,7 mil × $5,4 mil) |
| Rússia × USA | USA 2 × 0 | Rússia vence uma; empate (Rússia na frente, a USA com 1 prédio) |
| China × GLA | China 2 × 0 | China vence uma (6 min); empate parelho |
| USA × China | empate (USA na frente) | empate (USA na frente) |
| USA × GLA | USA | USA (11,7 min) |

Ninguém ganha tudo e a GLA sai do zero (1 vitória e 2 empates). A Rússia fica um pouco acima (nenhuma derrota sem troco, 3 empates na
frente); com uma partida por lado (o mapa e a semente iguais, a mesma partida sai igual), não muda número por isso. A suspeita, se
repetir: o alcance de 52 m do T-14, que a conta de Lanchester não vê.

---

## 9. O que foi feito (a lista viva)

Preenchida conforme sai. Cada linha: o script, o modelo, a prévia e o que conferir. A lista inteira do que o Felipe pediu (o elenco, o
AAA, o túnel, o recorte do chão, as bandeiras, a USA, o kit civil) está em `docs/elenco/TAREFAS.md`. A galeria de avaliação, com o
veredito dele por peça: `Game/Tools/elenco_gallery.py` → `captures/game/elenco_galeria/`.

**Como cada modelo é feito (todos):**
1. O script `Game/Tools/model_<chave>.py` monta o modelo.
2. O executor do detalhe, `model_run_detail.py`, aplica:
   - a grade e o UV em metros;
   - as máscaras de quina, sombra de contato e fresta na cor de vértice;
   - as rodas da lagarta, uma malha por roda;
   - a tangente do elo da esteira;
   - o pano das bandeiras tirado da malha.
3. O destrutor dá os estados de dano dos prédios (`model_damage.py`).
4. A importação, pelo `import_provisional.py` (a família de cada facção).
5. O `Units.json`, pelo `elenco_units.py`.

| Peça | Modelo (triângulos depois do detalhe) | No jogo | Testado | Onde ver |
|---|---|---|---|---|
| China: Command Center, Reator, Supply Center, Quartel, War Factory, Propaganda, Gattling Cannon | 417 mil, 111 mil, 257 mil, 215 mil, 486 mil, 279 mil, 43 mil; ocos de placas, telha redonda, estados de dano | importados (31/10) | a vitrine `elenco_china` (31/10): o quartel caindo até a ruína; a ruína nova (lajes quebradas) falta ver | `captures/game/elenco_china_v1/` |
| China: Dozer, caminhão, Type 99, Gattling Tank, Inferno, Overlord | 147 mil, 93 mil, 237 mil, 190 mil, 216 mil, 399 mil; esteira de elos que anda, rodas separadas | importados (31/10) | vitrine `elenco_china` (parados) | `captures/game/models/<chave>/preview/` |
| GLA: Command Center, Supply Stash, Quartel, Arms Dealer, Palácio, Stinger Site | 403 mil, 119 mil, 205 mil, 322 mil, 350 mil, 33 mil; adobe oco, bandeiras no vento; o Arms Dealer com a rampa do túnel abaixo do chão (recorta o chão) | importados (31/10; refazer com o túnel e as bandeiras) | a vitrine `elenco_gla` (com o Quad Cannon e a Technical saindo do túnel) | — |
| GLA: picape, Quad Cannon, Toxin, Rocket Buggy | 168 mil, 200 mil, 156 mil, 80 mil; pneus com blocos, bandeiras no vento | importados (31/10) | — | — |
| USA: Strategy Center, Paladin, Javelin (lança-foguetes) | Strategy Center (o script da v4), Paladin 205 mil (esteira que anda) | importados | — | — |
| Infantaria China/GLA | o soldado da sessão da infantaria (as peças por facção) + os lança-foguetes novos (RPG-7, Javelin, PF-98) | importados | — | — |
| USA, versões B (01/11): tanque, Tomahawk, Dozer + caminhão, Stryker, Linebacker, Raptor, Apache, Chinook, avião de carga, caixa, Patriot, Drop Zone, Strategy Center, Airfield | `model_*_v2.py` (e `model_usa_mbt.py`); a A (a de hoje) fica até ele escolher | não importados (ele escolhe A ou B) | — | `captures/game/models/<chave>_v2/preview/folha_*.jpg` e a galeria |
| Rússia: Shtab, usina a gás, depósito, Kazarma, fábrica, Instituto, posto Pantsir (01/11) | 368 mil, 177 mil, 223 mil, 149 mil, 374 mil, 203 mil, 29 mil; painel pré-moldado, telhado-serra, chaminé listrada, torre de Shukhov | importação na corrente de 01/11 de manhã (com os estados de dano) | a vitrine `elenco_russia` e o tour | `captures/game/models/russia_predios_folha.jpg` |
| Rússia: T-90M, T-14, BMPT, TOS-1A, Pantsir-S1, IMR-3M, KamAZ (01/11) | 246 mil, 214 mil, 225 mil, 252 mil, 136 mil, 196 mil, 104 mil; a gaiola com a rede na cor do jogador, o Relikt em V, a camuflagem de 3 tons; o IMR-3M na escala de jogo 0,78 | idem | idem | `captures/game/models/russia_sheet/` |
| Infantaria da Rússia | o soldado da China na farda cinza-esverdeada (provisório) | — | — | — |

**Corrigido em 01/11 de manhã (os pivôs chutados):** 7 peças que giram estavam fora do eixo do modelo no `elenco_units.py` (a
lâmina do Dozer da China 2,8 m à frente, que se via solta no vídeo; o radar do Command Center da China 3 m acima; os alto-falantes
da Propaganda do outro lado; o prato da GLA 1,7 m fora do mastro; o radar do Gattling Tank; o laser da Paladin; o prato do Strategy
Center 3,2 m no ar). Agora todos saem das constantes dos scripts (conferidos contra o `preview.json` de cada modelo).
**Desfeito:** o recorte do chão (ver `docs/BASE_JOGAVEL.md`: a 3ª coleção de parâmetros passou do limite e o chão virou xadrez).
Depois consertado pela `MPC_Ground` e provado que não abre: o terreno Nanite da Unreal tira a máscara de propósito (o fonte do
`LandscapeRender.cpp`). Os 3 caminhos e a decisão dele em `docs/BASE_JOGAVEL.md`.

**Corrigido em 01/11 de dia (a prova de perto e os tours):**
- os tetos planos e os rufos dos prédios russos saíam com a camuflagem dos tanques (lia como neve): o `RuSteel`, o mesmo aço liso;
- o trator, o caminhão e o operário saíam andando no começo do tour e a foto ficava vazia: nascem de novo na hora da foto;
- as pedras, as moitas e os tufos do mapa ficavam dentro do lote dos prédios: o prédio que nasce limpa o lote (`ClearScatterUnder`);
- o veículo subindo do túnel do Arms Dealer aparecia como a mancha azul da silhueta: sem a silhueta no túnel;
- os pedaços que caem do prédio eram cinza-claros e liam como papel branco: na cor da parede (`StructureBreak` com a cor);
- a rachadura cobria a fachada aos 60% e lia como mosaico: nasce nas quinas e nos tiros e só se espalha com o prédio quase caindo;
- o quartel russo liso de cima: as chaminés, os exaustores, a antena e a parabólica;
- o TOS-1A: o cilindro de elevação ficava deitado na mesa e a haste subia com o bloco (soltavam na salva): 3 estágios telescópicos que
  o jogo mira a cada quadro (`visual.launcher.ram`, o mesmo sistema serve a qualquer lançador que levanta);
- a IA escolhendo o contra (§7.5) e o torneio de novo (§8.3).
