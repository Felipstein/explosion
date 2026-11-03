# Estudo: poderes do general da USA fora do ZH (Contra, mods, RTS e a USA real)

Levantamento de 03/11/2026 para desenhar a árvore de poderes da nossa USA (5 patentes dão 1, 1, 2, 2, 3 pontos; poder de 1 a 3 pontos; níveis I–III; recarga por uso; o golpe chega pelo mapa).
O ZH base está em `docs/poderes/ESTUDO_ZH.md` (§2.1, §3 USA, §6) e o mercado em `docs/poderes/ESTUDO_RTS.md`. Aqui só entra o que é novo.

**Convenções**
- **n/c** = não confirmado. **estimativa** = conta minha.
- Pat. = patente (estrela) em que o poder aparece. Recarga em min:s.
- A wiki do Contra bloqueia leitura direta; li o texto pela API da própria fandom (`api.php?action=parse`). Os links abaixo são as páginas normais.
- O changelog oficial do Contra (Revora) tem 39 páginas. As do 009 Final e dos patches são a [página 38](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-38) e a [página 39](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-39). Cito como "Revora p38" e "Revora p39".
- Unidades de distância do Contra são as do ZH (1 u = 0,2633 m).

---

## Resumo (10 linhas)

1. **No Contra 009 Final não existe USA "pura".** São 4 generais jogáveis: Granger (Air Force), Townes (Laser), Alexander (Superweapon) e Ironhand (Cybernetic). Ironside é o chefe da IA. Cada um tem de 9 a 13 opções, quase todas de 1 ponto.
2. **O Contra separa os generais pelo jeito de o golpe chegar.** Granger usa aviões e helicópteros. Townes usa satélites (marcar, derrubar avião, micro-ondas) e artilharia de plasma de fora do mapa. Alexander faz coisas caírem e virarem base (Dozer Drop, Defense Core Drop). Ironhand usa cápsulas orbitais e drones.
3. **A "artilharia de laser" do Townes é o Plasma Strike:** níveis I–III, patente 3, com 10 / 20 / 30 projéteis de plasma vindos de fora do mapa. Na tropa, a artilharia dele é a Libra ($950, alcance 325). A recarga e o dano do poder são n/c.
4. **As ideias mais fortes do Contra:**
   - o nível muda o papel (Nano Swarm: no I cura e reforça, no III só causa dano);
   - o nível troca o avião (Carpet Bomb: B-52 → B-1 → B-2 furtivo);
   - poder-isca (Illusion: avião falso por 30 s que puxa o fogo antiaéreo);
   - satélite que marca o alvo (+30% de dano);
   - defesa que cai do céu e monta em 15 s.
5. **O Contra dá respostas claras aos poderes:** o Saboteur atrasa todos os poderes do inimigo; o F-22 nega poderes aéreos; o SDI Cannon e o SLID interceptam o que é balístico; o AA Laser derruba 3 / 6 / 9 aviões.
6. **Erros que o Contra corrigiu:**
   - as Drop Pods III "valiam a vitória" (jogador);
   - MOAB e Shockwave Bomb foram cortados;
   - o Microwave Beam mudou 4 vezes;
   - o avião de reforço dava XP fácil a quem o derrubava e virou inatacável.
7. **ShockWave:** cada general ganhou golpes de assinatura (HALO Drop, Rods From God, Hypersonic Cruise Missile, B-52 com mísseis de cruzeiro que o relevo bloqueia, ADMG). Os números quase não estão publicados.
8. **Rise of the Reds 1.87 é o mais organizado:**
   - 1 estrela escolhe a doutrina (5 opções passivas);
   - 3 estrelas são golpes de 4:00–4:30 em 3 níveis;
   - 5 estrelas são o Spectre (5:00) e o Guardian Angel (escudo antimíssil que voa por 20 s, 4:00).
9. **Fora dos mods, as melhores ideias são de informação:** radar que mostra a rota e o alvo do golpe que vem (Act of War); guerra eletrônica que apaga o minimapa do inimigo (EndWar); Command Center que impede poderes no raio dele (C&C 2013).
10. **A USA real dá nomes prontos e mecânicas novas:**
    - Growler: interferência;
    - HIMARS: atira e foge;
    - Switchblade: cancela e redireciona;
    - Tomahawk Block IV: espera no ar e troca de alvo;
    - DE M-SHORAD e Golden Dome: escudo;
    - Dark Eagle: hipersônico.

---

## 1. Contra 009 Final: os generais americanos

### 1.0 O que vale pra todos os generais USA do Contra

**Versão**
- A última do 009 é o Final Patch 3 Hotfix 4 ([ModDB](https://www.moddb.com/mods/contra/downloads)).
- Já existe o Contra X Beta 2 Patch 1, de 05/10/2025 ([wiki](https://zerohourcontra.fandom.com/wiki/Contra_Wiki)). Ver §1.9.

**Generais**
- Jogáveis: Granger, Townes, Alexander e Ironhand.
- Ironside não é jogável: é o chefe do Challenge ([wiki](https://zerohourcontra.fandom.com/wiki/Contra_Wiki)).

**Pontos e camadas** (já em ESTUDO_ZH §6.1)
- Começa com 1 ponto e ganha 1, 1, 2, 3.
- Cada opção custa 1 ponto. Exceção: os Lasers Training do Townes ([wiki](https://zerohourcontra.fandom.com/wiki/General%27s_Promotion)).
- **Emergency Repair I** é de todos desde o 009 Final, mas exige patente 2 ([Revora p38](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-38)).

**Contra-jogo comum a todos**

| Resposta | O que faz | Fonte |
|---|---|---|
| Saboteur (GLA) | Entra no Command Center e aumenta a contagem de todos os poderes do dono | [wiki Saboteur](https://zerohourcontra.fandom.com/wiki/Saboteur), [wiki CC](https://zerohourcontra.fandom.com/wiki/American_Command_Center) |
| F-22A King Raptor | Nega os poderes aéreos do inimigo: caça os aviões dos poderes | [wiki](https://zerohourcontra.fandom.com/wiki/F-22A_King_Raptor) |
| SDI Cannon (Alexander) | Patente 5, $3000, alcance 600 (o maior entre as defesas do jogo). Intercepta Artillery Barrage, mísseis e nukes. Não pega avião de poder, mas pega a carga solta do alto | [wiki](https://zerohourcontra.fandom.com/wiki/SDI_Cannon) |
| SLID | Unidade que intercepta poderes balísticos, mais fraca que o SDI | [wiki](https://zerohourcontra.fandom.com/wiki/SLID) |
| AA Laser (poder) | Laser em órbita que derruba 3 / 6 / 9 aviões. No Patch 2, deixou de ferir avião pousado | wikis dos generais; [Revora p38](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-38) |

**Regras de entrega que importam pra nós**
- **Avião de poder dá XP a quem derruba.** Um jogador reclamou que os aviões de poder e de reforço davam nível fácil. No Patch 2, os cargueiros dos Reinforcement Pads e de transferência de dinheiro ficaram inatacáveis. Os aviões dos poderes continuam atacáveis ([Revora p38](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-38)).
- **A GLA passou a receber minas e túnel por um avião próprio (Corvus).** A wiki diz que é para o inimigo poder interceptar o poder. Mas o Corvus nasce do nada perto do alvo, então a interceptação é difícil ([wiki](https://zerohourcontra.fandom.com/wiki/Support_Aircraft)).
- **O Command Center americano não pode ser capturado.** A causa é um bug: quem capturava não usava mais os poderes ([wiki](https://zerohourcontra.fandom.com/wiki/American_Command_Center)).

### 1.1 Air Force: General Malcolm "Ace" Granger

Fonte da lista: [wiki do Granger](https://zerohourcontra.fandom.com/wiki/General_Malcolm_%22Ace%22_Granger).

| Poder | Pat. | Níveis e números | Chega como | Recarga | Contra-jogo |
|---|---|---|---|---|---|
| Fighter Training | 1 | F-18, F-35, F-22 e Comanche nascem veteranos: +10% de cadência, dano e vida ([Revora p38](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-38)) | passivo | — | — |
| Bomber Training | 1 | F-16XL, B-4 Chimera, Pave Low e Aurora Alpha nascem veteranos, com os mesmos +10% | passivo | — | — |
| Spy Drone | 1 | Drone camuflado que revela a área. Não detecta camuflados | drone | n/c | antiaéreo que detecte camuflado |
| Emergency Repair I | 1 (exige pat. 2) | Conserta veículos e prédios na área e cura infantaria | instantâneo | n/c | — |
| Reinforcements I–III (Comanche Drop) | 3 | 2 / 4 / 6 Comanches de graça | voam até o ponto | n/c | antiaéreo |
| A-10 Strike I–III | 3 | 1 / 2 / 3 A-10. No Patch 2: menos dano do canhão, mais dos mísseis ([Revora p38](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-38)) | da borda | n/c | antiaéreo; sair da mira |
| Improved Spy Satellite | 3 | O satélite fica controlável e detecta camuflados (infantaria, veículos, prédios escondidos, aviões furtivos). Não detecta minas, Demo Traps nem Chemical Pumps | satélite | n/c | nenhum |
| SEAL Drop | 5 | 3 Black Hawks entregam 9 SEALs: 200 de vida, lançador de granadas (40 + 20 de respingo), camuflados quando parados ([wiki SEAL](https://zerohourcontra.fandom.com/wiki/Navy_SEAL)). Desde o Patch 2, caem espalhados | helicópteros | n/c | derrubar os Black Hawks; detector |
| Illusion | 5 | Cria a ilusão de um ataque aéreo: o antiaéreo inimigo atira no ar. O avião falso dura 30 s (era 60 s; [Revora p39](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-39)) | aviões-isca | n/c | ignorar a isca (exige leitura) |
| Carpet Bombing | 5 | Um B-1B Lancer bombardeia a área. Dano aumentado no Patch 3 ([Revora p39](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-39)) | da borda | n/c | antiaéreo pesado |

**Superarma:** o Strategic Command Uplink. Custa $5000 e chama 3 B-2 furtivos com bombas. A contagem caiu de 8 para 6 min no Patch 2. A wiki registra que ele pode ser interceptado normalmente ([wiki](https://zerohourcontra.fandom.com/wiki/Strategic_Command_Uplink), [Revora p38](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-38)).

**O que saiu:**
- Pilot Training, Helicopter Training e Shockwave Bomb (009 Beta 2);
- a melhoria MOAB (Final Patch 1);
- Mine Clearing (Patch 2);
- o AC-150, gunship controlável (desde o 005).
- Fontes: [Revora p38](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-38), [Support Aircraft](https://zerohourcontra.fandom.com/wiki/Support_Aircraft), [wiki](https://zerohourcontra.fandom.com/wiki/General_Malcolm_%22Ace%22_Granger).

**O que é criativo**
- **Illusion** é o único poder-isca do gênero que achei. Combina com o golpe real: a isca primeiro, depois o A-10.
- **O "Training" vem dividido por papel** (caças × bombardeiros). O jogador escolhe que parte da força aérea fica de elite.
- **Comanche Drop é reforço que chega voando, não paraquedas.** O exército aparece já pronto pra lutar.

### 1.2 Laser: General "Pinpoint" Townes

Fonte da lista: [wiki do Townes](https://zerohourcontra.fandom.com/wiki/General_%22Pinpoint%22_Townes).

| Poder | Pat. | Níveis e números | Chega como | Recarga | Contra-jogo |
|---|---|---|---|---|---|
| Spy Drone | 1 | Igual ao do Granger | drone | n/c | — |
| Target Designation | 1 | Satélite marca alvos em qualquer lugar do mapa. Revela escondidos. As suas unidades dão +30% de dano no marcado. No Patch 3, deixou de valer para os aliados ([Revora p39](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-39)) | satélite | n/c | sair da marca; matar o alvo antes |
| Emergency Repair I / II–III | 1 (pat. 2) / 3 | Reparo leve / médio / sério | instantâneo | n/c | — |
| Electricity Net Connection | 3 | +24 de energia por 1 min. No 008 eram 2 min ([Revora p10](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-10), [p38](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-38)). A IA usa quando fica sem energia | instantâneo | n/c | derrubar as usinas mesmo assim |
| Lasers Training 1 | 3 | Tudo que é laser nasce veterano, com laser verde. **Custa 2 pontos** desde o Patch 3 (era 1). Não exige mais o Electricity Net ([Revora p39](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-39)) | passivo | — | — |
| AA Laser I–III | 3 | Laser em órbita derruba 3 / 6 / 9 aviões. Avião blindado leva mais de um tiro | satélite | n/c | aviões espalhados |
| **Plasma Strike I–III** | 3 | **Artilharia de plasma de fora do mapa: 10 / 20 / 30 projéteis.** Dano e raio n/c | de fora do mapa, sem avião | n/c | sair da área; o SDI Cannon e o SLID interceptam o que é balístico (n/c se o plasma conta) |
| Infantry Paradrop I–III | 3 | 4 / 6 / 9 Rangers + 4 / 6 / 9 Laser Defenders. Desde o 009 Final, não caem num ponto só ([Revora p38](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-38)) | paraquedas | n/c | antiaéreo; matar no ar |
| Lasers Training 2 | 5 | Tudo que é laser nasce elite, com laser laranja. Exige o LT1. Custa 2 pontos (era 3) | passivo | — | — |
| High Explosive Bomb | 5 | Um B-52 solta uma fuel air bomb | da borda | n/c | antiaéreo |
| Microwave Beam | 5 | Feixe de satélite queima o que está na área e desliga prédios | satélite | n/c | sair da área |

**Histórico do Microwave Beam** (o poder que mais mudou)
- 008 Beta: só desligava prédios e matava infantaria, sem dano em veículo e prédio.
- 008 Final: voltou a dar dano.
- 009 Final Patch 3: menos dano, mas desliga os prédios mais rápido.
- Fontes: [Revora p10](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-10), [p38](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-38), [p39](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-39).

**Plasma Strike × Laser Strike**
- No 008 Beta havia um "Laser Strike", enfraquecido em 25%. Um jogador reclamou da recarga muito longa dele ([Revora p10–11](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-11)).
- Se o Plasma Strike é o mesmo poder renomeado: n/c.

**Fora dos poderes:** Particle Cannon na patente 5 ($4000), Annihilator e Omega Cannon (§1.6).

**O que é criativo**
- **O general dos satélites:** marcar, derrubar avião, micro-ondas e reconhecimento. Nenhum golpe dele pode ser abatido no caminho. Bom pra identidade, ruim pra contra-jogo (leitura minha).
- **Target Designation já na patente 1** é um poder de equipe: não mata, faz o exército bater mais. Lê bem de cima.
- **O Electricity Net é um poder de economia de emergência.** Resolve a fraqueza do próprio general (a energia).
- **O preço segue a força:** o Training passou a custar 2 pontos.

### 1.3 Superweapon: General Alexis Alexander

Fonte da lista: [wiki da Alexander](https://zerohourcontra.fandom.com/wiki/General_Alexis_Alexander).

| Poder | Pat. | Níveis e números | Chega como | Recarga | Contra-jogo |
|---|---|---|---|---|---|
| Spy Drone | 1 | Drone camuflado que detecta camuflados (segundo a wiki) | drone | n/c | — |
| Spy Satellite | 1 | Revela a área. Já vem liberado | satélite | n/c | — |
| Defenders Training | 1 | Defesas nascem veteranas: +10% de cadência, dano e vida. Desde o Patch 1, vale também pras Defense Cores ([Revora p38](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-38)) | passivo | — | — |
| Centurion Tank | 1 | Destrava o Centurion (exige War Factory e patente 2). Até o Patch 2 custava 1 ponto ([wiki](https://zerohourcontra.fandom.com/wiki/Centurion)) | destrava | — | — |
| Emergency Repair I / II–III | 1 (pat. 2) / 3 | Reparo leve / médio / sério | instantâneo | n/c | — |
| **Dozer Drop** | 3 | Solta 2 Heavy Construction Dozers na zona | paraquedas | n/c | matar os dozers |
| **Defense Core Drop I–III** | 3 | I: 1 Patriot Core. II: 1 Patriot + 1 Grendel. III: 2 + 2. A defesa leva 15 s para montar ([Revora p38](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-38)). Desde o Patch 2, cai como drone que se monta, não como defesa pronta | paraquedas | n/c | atacar nos 15 s de montagem |
| Carpet Bomb I–III | 3 | I: 1 B-52. II: 2 B-1. III: 3 B-3/B-2 furtivos. Recarga de 7:00 (era 8:00; [Revora p39](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-39)) | da borda | 7:00 | antiaéreo; o B-2 só aparece ao soltar a carga |
| AA Laser I–III | 3 | 3 / 6 / 9 aviões | satélite | n/c | — |
| EMP Bomb | 5 | Um B-1 Lancer solta a bomba EMP. Desliga veículos e prédios | da borda | n/c | antiaéreo |
| Defense Core Drop (pat. 5) | 5 | 2 Patriot + 2 Grendel + 1 Ion Core | paraquedas | n/c | idem |
| High Explosive Bomb | 5 | Um B-52 solta uma fuel air bomb | da borda | n/c | antiaéreo |

**Superarmas (fora dos pontos)**

| Superarma | Patente | Custo | O que faz | Fonte |
|---|---|---|---|---|
| Tomahawk Storm | 1 (uma das 2 superarmas de patente 1) | $2500 | 9 Tomahawks lançados um a um | [wiki](https://zerohourcontra.fandom.com/wiki/Tomahawk_Storm) |
| Particle Cannon | 3 | $3500 | o feixe padrão | [wiki](https://zerohourcontra.fandom.com/wiki/Particle_Cannon) |
| Weather Manipulation Device | 5 | $5000 | tempestade elétrica (estilo HAARP) | [wiki](https://zerohourcontra.fandom.com/wiki/Weather_Manipulation_Device) |
| SDI Cannon | 5 | $3000 | defesa antibalística, alcance 600 | [wiki](https://zerohourcontra.fandom.com/wiki/SDI_Cannon) |

**Histórico**
- O "Zone Capture" (o Defense Drop) foi dividido em 4 poderes no 009 Beta 2.
- O A-10 Strike saiu da Alexander ([Revora p38](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-38)).
- A IA do Challenge dela só usa o Zone Capture quando está perdendo (mesma fonte).

**O que é criativo**
- **Poder que vira território.** O Defense Core Drop é "capturar a zona": a defesa cai, leva 15 s para montar e segura o ponto. A janela de 15 s é o contra-jogo.
- **Dozer Drop** abre uma base nova longe de casa. É o "Deploy War Factory" da USA.
- **Os níveis do Carpet Bomb trocam o avião, não só a quantidade.** O B-52 é lento e duro, o B-1 é rápido, o B-2 é furtivo. O contra-jogo muda junto.

### 1.4 Cybernetic: General Algrin "A.I." Ironhand (novo do Contra)

Fonte da lista: [wiki do Ironhand](https://zerohourcontra.fandom.com/wiki/General_Algrin_%22A.I.%22_Ironhand).

| Poder | Pat. | Níveis e números | Chega como | Recarga | Contra-jogo |
|---|---|---|---|---|---|
| Spy Satellite | 1 | Reconhece a área | satélite | n/c | — |
| Spy Drone | 1 | Já vem liberado | drone | n/c | — |
| Emergency Repair I | 1 (pat. 2) | Igual aos outros | instantâneo | n/c | — |
| Improved Spy Satellite | 3 | Igual ao do Granger | satélite | n/c | — |
| Stinger Drop | 3 | 4 Stinger Drones (eram 6) guardam a área até morrer ([Revora p38](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-38)). Recarga caiu de 5:00 para 3:30 ([Revora p39](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-39)) | cápsula | 3:30 | matar os drones |
| **Nano Swarm I–III** | 3 | O nível muda o papel. I: +50% de poder de fogo e a maior cura. II: +25%, cura normal e pouco dano. III: só dano alto em área. Desde o Patch 2, chega em cápsulas pequenas, não na hora ([Revora p38](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-38)). No Patch 3, mais dano, e o III dura mais | cápsulas | n/c | sair da nuvem |
| Infantry Drop I–III (Drop Pods) | 3 | I: 4 Cyborgs + 2 Missile Cyborgs + 2 Medic Droids. II: o dobro. III: o triplo. No Patch 3, +1 Medic Droid por cápsula. Recarga caiu de 7:00 para 5:00 no Patch 2 | cápsulas orbitais | 5:00 | matar ao abrir; a cápsula não é avião |
| Mosquito Swarm | 5 | 4 drones antipessoal, com 3 min de vida. Substituiu o A-10 ([Revora p38](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-38)) | cápsula | n/c | antiaéreo |
| Commando Drop | 5 | O Cyborg Commando cai numa cápsula verde. Voa (jetpack), detecta camuflados, minas e armadilhas. **Exige Drop Pods III** ([wiki](https://zerohourcontra.fandom.com/wiki/Cybernetic_Commando)) | cápsula | n/c | concentrar fogo; ele não ataca voando |

**Histórico das Drop Pods** (lição de balanço)
- No 008, um jogador escreveu que um golpe de Drop Pods III equivalia à vitória. Elas tinham recarga de 7 min ([Revora p3](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-3)).
- Depois:
  - o dano do pouso caiu 3 vezes, e pousar em prédio foi proibido ([Revora p37](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-37));
  - no Patch 2, a cápsula deixou de ferir o dono e os aliados.

**A IA do Ironhand no Challenge**
- Pausa na patente 3 para o jogador humano se recuperar ([Revora p39](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-39)).

**O que é criativo**
- **O nível inverte o poder.** O Nano Swarm I é suporte, o III é ataque. Comprar o nível alto é uma escolha, não só "mais".
- **Cadeia de pré-requisito com recompensa:** quem investe nas 3 Drop Pods ganha o comando de elite na patente 5.
- **A cápsula orbital é um meio de entrega diferente do avião.** Ela não é derrubável, então o contra-jogo é no pouso.

### 1.5 Ironside (chefe, não jogável) e a "USA de base"

Fonte: [wiki do Ironside](https://zerohourcontra.fandom.com/wiki/General_Ironside). É o mais parecido com a USA do ZH.

| Poder | Pat. | O que faz |
|---|---|---|
| Spy Satellite / Spy Drone | 1 | Os dois custam ponto, ao contrário dos outros generais |
| Clustered Tomahawk I–III | 3 | Um Tomahawk que espalha bombas na área. **Só a IA usa** |
| A-10 Strike I–III | 3 | 1 / 2 / 3 A-10 |
| Paratrooper Drop I–III | 3 | Rangers e Missile Defenders |
| High Explosive Bomb / Carpet Bombing | 5 | B-52 |
| EMP Bomb | 5 | B-1 Lancer |

- **Target Designator** (o prédio que ele usa) ([wiki](https://zerohourcontra.fandom.com/wiki/Target_Designator)):
  - $2500;
  - chama um A-10 a cada 25 s, com alcance de 600;
  - **o ponto do golpe fica marcado para todos**, e o A-10 leva alguns segundos para chegar.
- **O Ironhand já teve Cluster Tomahawks:** a recarga caiu de 7 para 6 min no 008 ([Revora p1](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/)).

### 1.6 A artilharia de cada general (o pedido especial)

A wiki explica por que cada general USA ganhou uma artilharia de patente 1: os jogadores acham que "having a weak or flawed artillery is better than having none" ([wiki Libra](https://zerohourcontra.fandom.com/wiki/Libra)).

| General | Artilharia (unidade) | Números | Poder de artilharia | Fonte |
|---|---|---|---|---|
| Granger | F-16XL, a "artilharia voadora" | $900; 180 de dano em raio 15; alcance 550; recarga 9 s | A-10, Carpet, Strategic Bombing | [wiki](https://zerohourcontra.fandom.com/wiki/F-16XL) |
| Granger | Pave Low (helicóptero com mísseis) | $1800; 180 + 40; alcance 210; recarga 20 s | — | [wiki](https://zerohourcontra.fandom.com/wiki/Pave_Low) |
| Granger | B-4 Chimera (6 mísseis de cruzeiro) | $3500; 150 por míssil; alcance 800 | — | [wiki](https://zerohourcontra.fandom.com/wiki/Chimera) |
| Granger | Eye Target Designator (Stryker que marca) | $950; +30% de dano no alvo; dá alcance a F-16XL e Pave Low | — | [wiki](https://zerohourcontra.fandom.com/wiki/Eye_Target_Designator) |
| **Townes** | **Libra** (Paladin com 2 canhões de plasma) | $950; 78 de dano em raio 6; alcance 325; recarga 8 s; não precisa desdobrar; gasta 1 de energia | **Plasma Strike I–III** (10 / 20 / 30 projéteis de fora do mapa) | [wiki](https://zerohourcontra.fandom.com/wiki/Libra) |
| Townes | Annihilator (defesa, patente 3) | $2500; 600 + 100; alcance 440 | — | [wiki](https://zerohourcontra.fandom.com/wiki/Annihilator) |
| Townes | Omega Cannon (superunidade, patente 5) | $5500; feixe de 15 contínuo (alcance 330) ou tiro de 600 + 100 (alcance 440) | — | [wiki](https://zerohourcontra.fandom.com/wiki/Omega_Cannon) |
| Alexander | AMOS (Stryker de morteiro) | $950; 80 de dano; alcance 275; recarga 8 s | Carpet Bomb, Tomahawk Storm | [wiki](https://zerohourcontra.fandom.com/wiki/AMOS) |
| Alexander | Atlas (defesa de longo alcance) | $2200; 200 de dano; alcance 500 (mínimo 200) | — | [wiki](https://zerohourcontra.fandom.com/wiki/Atlas) |
| Ironhand | Phobos (morteiros) | $950; 80 de dano; alcance 275 | Nano Swarm III | [wiki](https://zerohourcontra.fandom.com/wiki/Phobos) |
| Ironhand | Dominator (aranha de mísseis guiados) | $1800; 200 + 100; alcance 360 | — | [wiki](https://zerohourcontra.fandom.com/wiki/Cyber_Spiders) |
| Ironside | M270 MLRS | $1000; alcance 375 | Clustered Tomahawk (só IA) | [wiki](https://zerohourcontra.fandom.com/wiki/M270_MLRS) |

**Leitura**
- Só o Townes tem artilharia como **poder** de verdade (o Plasma Strike).
- Os outros tratam a artilharia como unidade e deixam o poder pro avião.
- O Plasma Strike repete o defeito do Artillery Barrage do ZH: vem de fora do mapa e não pode ser parado no caminho. O único contra é o SDI/SLID da Alexander, que nem é certo que pegue plasma (ESTUDO_ZH resumo 8).

### 1.7 Drops, destravar e "Training": o quadro transversal

| Tipo | Exemplos no Contra (USA) | Observação |
|---|---|---|
| Chamar unidade (drop) | Comanche Drop 2/4/6; SEAL Drop 9; Infantry Paradrop 4+4 até 9+9; Drop Pods; Stinger Drop 4; Mosquito Swarm 4; Commando Drop; Dozer Drop 2; Defense Core Drop | Cada general entrega de um jeito: helicóptero, paraquedas, cápsula, drone |
| Destravar | Centurion (deixou de custar ponto no Patch 2) | Destravar unidade com ponto quase sumiu da USA do Contra |
| Training | Fighter, Bomber, Defenders, Lasers 1 e 2 | +10% de cadência, dano e vida. Só o do Townes custa 2. Lasers sobem de cor (verde, laranja) |
| Economia | Electricity Net (+24 de energia por 1 min) | Único poder de economia da USA no Contra |
| Informação | Spy Drone, Spy Satellite, Improved Spy Satellite, Target Designation | Algumas já vêm de graça |

### 1.8 O que o changelog ensina (curto)

- **Poder fraco ninguém compra, poder forte vira obrigatório.** O Contra mexeu no Microwave Beam 4 vezes e no Laser Strike, cortou a MOAB e a Shockwave Bomb e removeu Mine Clearing, Data Upload e Fake Units ([Revora p38](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-38)).
- **Preço por força.** Os Lasers Training passaram para 2 pontos ([Revora p39](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-39)).
- **Drop que causa dano no pouso é perigoso.** As Drop Pods foram enfraquecidas 3 vezes (§1.4).
- **Avião abatível dá XP:** decidir de propósito quanto vale derrubar o avião do poder (§1.0).
- **Recargas que apareceram no changelog:**
  - Drop Pods 5:00;
  - Stinger Drop 3:30;
  - Carpet Bomb da Alexander 7:00;
  - Strategic Bombing 6:00.
  - O resto das recargas: n/c.

### 1.9 Contra X (beta de 2025): o que já se sabe da USA

- **Superweapon General:** escolhe um caminho ([PPM, 18/12/2022](https://ppmforums.com/post-614327/contra-x-work-in-progress-news-update-5/)):
  - **Conventional:** Tomahawk Storm na patente 3 e ICBM na 5;
  - **Futuristic:** Particle Cannon na 3 e Weather Device na 5.
- **Laser General, Laser Control Station** ([PPM, 28/10/2024](https://ppmforums.org/topic-71654/contra-x-work-in-progress-news-update-14-usa-laser-control-station)):
  - patente 1: o satélite Target Designator, com **+33% de dano** no marcado;
  - patente 3: o prédio sobe o laser das unidades vivas, **1 nível a cada 2 min** (vermelho → verde → azul);
  - patente 5: o Laser Wavelength Intensifier dá mais um nível pra todos (até o violeta).
- **Leitura:** o "Training" do Contra X vem de um prédio e cresce com o tempo. Matar o prédio corta o fluxo. É veterania com contra-jogo.

---

## 2. ShockWave (1.2): os generais USA

**Números:** a wiki quase não tem; o resto é n/c. O Particle Cannon da USA base e do Granger recarrega em 4 min ([wiki](https://shockwave.fandom.com/wiki/Super_Weapons)).

**Os 3 novos do mod:** Ironside (blindados), Leang e Deathstrike ([site oficial](http://shockwave.swr-productions.com/features.php)).

| General | Patente 1 | Patente 3 | Patente 5 | Fonte |
|---|---|---|---|---|
| USA (base) | Paladin, Stealth Fighter, Spy Drone | Pathfinder, **F18 Hornet Carrier Strike**, Paradrop 1–3, Spectre 1–2, A-10 1–3, **Emergency Aid 1–3** | FAB, Leaflet, Spectre 3 | [wiki](https://shockwave.fandom.com/wiki/United_States_of_America) |
| Granger | Pilot Training, Spy Drone, Tornado Bomber (destrava) | Pathfinder, Carpet Bombing, **HALO Drop**, A-10, Spectre, Emergency Repair | FAB, Leaflet | [wiki](https://shockwave.fandom.com/wiki/Granger) |
| Townes | Laser Paladin, Spy Drone, **Railgun Artillery** (destrava) | Pathfinder, Carrier Airstrike, Paradrop, Spectre 1–2, **UCAV Air Strike**, Emergency Repair | FAB, Leaflet, Spectre 3 | [wiki](https://shockwave.fandom.com/wiki/Townes) |
| Alexander | Enforcer Drone, Spy Drone, Shatterer (destravam) | Pathfinder, Leaflet, **Turret Paradrop**, **Rods From God**, Spectre, Emergency Aid | FAB, **Hypersonic Cruise Missile Strike** | [wiki](https://shockwave.fandom.com/wiki/Alexander) |
| Ironside | Howitzer, Spy Drone, Mammoth (destravam) | Tank Destroyer, **B52 Cruise Missile Strike**, **Bradley Paradrop**, A-10, **ADMG Grenade Barrage**, Emergency Aid | FAB, Leaflet | [wiki](https://shockwave.fandom.com/wiki/Ironside) |

**O que cada poder novo faz** (pela wiki)

| Poder | O que faz | Criativo |
|---|---|---|
| HALO Drop (Granger) | Paraquedistas que não podem ser atacados antes de pousar | O salto alto tira o abate no ar e troca o contra-jogo pro pouso |
| UCAV Air Strike (Townes) | Drones de combate soltam carga na zona | Ataque aéreo sem piloto |
| Railgun Artillery (Townes) | Destrava a artilharia de railgun; substitui o Tomahawk | A artilharia "laser" do SW é railgun |
| Laser Designator Base (Townes, prédio) | Funciona como SOFLAM (designador laser) para as unidades de míssil | O prédio marca, os mísseis acertam |
| Turret Paradrop (Alexander) | Paradrop de Patriots que saltam do chão | O mesmo "território do céu" do Contra |
| Rods From God (Alexander) | Golpe orbital com varas de tungstênio | Golpe cinético, sem explosivo |
| Hypersonic Cruise Missile (Alexander) | Um míssil hipersônico na área | Golpe rápido de patente 5 |
| B52 Cruise Missile Strike (Ironside) | 6 Tomahawks lançados de um B-52, em sequência. A rota sai da borda mais perto do seu Command Center. **Arranha-céu e relevo param o míssil** ([wiki](https://shockwave.fandom.com/wiki/B52_Cruise_Missile_Strike)) | O mapa é o contra-jogo; o dono escolhe o ângulo pela posição da base |
| Bradley Paradrop (Ironside) | Armor Rangers e Bradleys de paraquedas | Drop misto de infantaria e veículo |
| ADMG Grenade Barrage (Ironside) | Solta metralhadoras de granada automáticas | Defesa temporária que cai do céu |
| A-10 Strike | 1 / 2 / 3 A-10. O nível 3 só abre na patente 5 ([wiki](https://shockwave.fandom.com/wiki/A10_Strike)) | Nível preso à patente |

**Leitura**
- O padrão do SW na patente 1 é destravar unidade (que ainda exige o Strategy Center).
- O que vale copiar são os golpes de assinatura e a regra do relevo bloqueando o míssil.

---

## 3. Rise of the Reds (1.87): a promoção da USA

Fonte: [USA General's Promotion](https://generalsrotr.fandom.com/wiki/USA_Arsenal/General%27s_Promotion). Recargas da própria wiki.

| Pat. | Poder | Recarga | O que faz |
|---|---|---|---|
| 1 | Veteran Company | — | Ranger, Missile Defender e Vanguard nascem veteranos; destrava Veteran Specialists |
| 1 | Armored Company | — | Destrava Paladin, Patriot SAM e Composite Armor |
| 1 | Covert Forces | — | Destrava Pathfinder, SEAL e Little Bird |
| 1 | **Air Mobility** | — | Infantaria chama um Drone Airlifter e veículo chama um Starlifter pra atravessar o mapa ([wiki](https://generalsrotr.fandom.com/wiki/Air_Mobility)) |
| 1 | Logistics Efficiency | — | Supply Drone, Combat Chinook, Chinook Supply Point |
| 3 | Leaflet Drop | 4:00 | 3 cargueiros C-3 Trinity; desliga as unidades de chão por 20 s ([wiki](https://generalsrotr.fandom.com/wiki/Leaflet_Drop)) |
| 3 | Acolyte Tank Drop | 4:00 | Um C-17 Globemaster solta 3 Acolyte Tanks ([wiki](https://generalsrotr.fandom.com/wiki/Acolyte_Tank_Drop)) |
| 3 | Airborne Fire Team / Squad / Raid | 4:00 | C-130 + C-17. Nível 1: 3 Green Berets, 2 Delta e 1 ATV Humvee. Nível 2: 6 + 4 + 2. Nível 3: 9 + 6 + 3 |
| 3 | A-10 Strafing / Rocket / Bombing Run | 4:00 | 1 / 2 / 3 A-10, cada nível com nome próprio |
| 3 | Carpet / High Speed / Stealth Carpet Bombing | 4:30 | B-52 → B-1B (mais rápido) → B-2 (furtivo) |
| 5 | Spectre Gunship | 5:00 | AC-17 Spectre circula 20 s; você escolhe o alvo do obus de 155 mm e da Gattling; os mísseis atiram sozinhos ([wiki](https://generalsrotr.fandom.com/wiki/Spectre_Gunship)) |
| 5 | **Guardian Angel TMD** | 4:00 | O EAL-3 circula 20 s: laser de defesa derruba mísseis e foguetes num raio de 180, a cada 0,1 s, e solta flares que puxam o antiaéreo inimigo ([wiki](https://generalsrotr.fandom.com/wiki/Guardian_Angel_TMD)) |

**Notas**
- **Os pontos estão n/c nesta página.** O ESTUDO_ZH §6.3 diz que o ponto de 1 estrela escolhe um ramo.
- **A versão 1.9 está anunciada** ([stream interna de 08/10/2024](https://youtu.be/bnxP_dVB6q0?t=9546), citada nas páginas [Armored Company](https://generalsrotr.fandom.com/wiki/Armored_Company) e [Covert Forces](https://generalsrotr.fandom.com/wiki/Covert_Forces)):
  - a Armored Company troca o Patriot pelo Bradley;
  - a Covert Forces ganha Black Hawk e Fire Base.

**O que é criativo**
- **Guardian Angel:** um poder defensivo de patente 5 que nega o poder do outro (míssil, SCUD, artilharia de foguete) por 20 s. É a resposta americana à artilharia da China e ao SCUD da GLA.
- **Air Mobility:** em vez de um golpe, o ponto dá mobilidade a todo o exército (chamar transporte).
- **O drop misto do Airborne Raid** traz infantaria e veículos leves juntos.
- **Os nomes por nível** (Strafing, Rocket, Bombing Run) explicam o que muda sem tooltip.

---

## 4. Outros RTS com USA moderna (só o que é criativo)

### 4.1 Act of War: Direct Action / High Treason (US Army)

| Mecânica | Números | O que é criativo | Fonte |
|---|---|---|---|
| Early Warning Radar | Melhoria de $1200 na torre aérea. Mostra o destino, o alvo e o tipo de cada ataque aéreo inimigo. O ícone diz o tipo: tanque = ataque ao chão, avião = caça, prédio = bombardeiro | O defensor vê a rota antes do golpe | [wiki](https://actofwar.fandom.com/wiki/Early_warning_radar) |
| Tactical Warning System | Melhoria de $1500 na defesa antibalística. Mostra todos os golpes de superarma e quantas o inimigo tem prontas ou em produção | Aviso da superarma vira algo que se compra | [wiki](https://actofwar.fandom.com/wiki/Tactical_warning_system) |
| Patriot Ballistic Defense System | Cada facção tem um prédio que intercepta a superarma do inimigo | Contra específico pra superarma | [wiki](https://actofwar.fandom.com/wiki/Tactical_weapon) |
| Prisioneiro de guerra | Capturar dá $500 na hora. Num centro de interrogatório, rende crédito contínuo (a wiki diz $50 por segundo; n/c). Trocar o prisioneiro revela uma área pequena do mapa por um tempo | A baixa do inimigo vira dinheiro e informação | [wiki](https://actofwar.fandom.com/wiki/Prisoner_of_war) |
| CIA Bank Account Freeze | $2000. O inimigo perde $250 a cada soldado dele capturado | Poder de economia com tema real | [wiki](https://actofwar.fandom.com/wiki/CIA_bank_account_freeze) |
| B-2 Spirit | $4000; exige DEFCON 1; 8 bombas Mk 84 | O avião mais forte só depois da escalada | [wiki](https://actofwar.fandom.com/wiki/B-2_Spirit) |

### 4.2 World in Conflict (o que é dos EUA e não está no ESTUDO_RTS §4)

Fonte: o [manual oficial](https://cdn.akamai.steamstatic.com/steam/apps/21760/manuals/manual_english.pdf), págs. 27–30.

- **Daisy Cutter só da USA/NATO:** um cargueiro solta uma bomba de 17.000 lb. Limpa a mata e mata infantaria, mas é fraca contra prédio grande e blindado. A soviética equivalente é a Fuel Air Bomb, num Su-25.
- **Tank Buster:** canhão de 30 mm num corredor que acerta amigo e inimigo.
- **Heavy Air Support:** ondas com mísseis guiados que poupam os amigos. O fogo amigo vira uma escolha de preço.
- **Air-to-Air Strike:** um caça varre a área e derruba helicópteros. É um poder antiaéreo.
- **Airborne Infantry:** o líder do esquadrão que cai pode pedir artilharia. A tropa traz um poder junto.
- **Atraso pela distância até o QG:** o jogador acerta a hora do impacto com o movimento do inimigo.

### 4.3 Broken Arrow (2025), EUA

Fonte: [Slitherine, Special Operations](https://www6.slitherine.com/news/broken-arrow-special-operations-forces-army-specialization). Especializações: Armored, Marines, Special Operations, Stryker Cav e Airborne ([Steam](https://steamcommunity.com/app/1604270/discussions/0/715612499312950765)).

- **AC-130U/J:** atira em quem está no círculo da curva. O J tem 30 mm + 105 mm e mísseis Griffin pela rampa, em alvos que ele mesmo marca.
- **MQ-9 Reaper:** até 2 bombas de 500 lb e 8 Hellfire, mais Stinger ou AIM-9. É um drone que se defende no ar.
- **MC-130H:** até 64 paraquedistas ou 20 t de carga, em baixa altitude. O transporte é o alvo.
- **Laser designator:** Ranger Snipers e a equipe STT marcam alvos para a munição guiada. Matar quem marca cancela o golpe.
- **Lição já anotada** (ESTUDO_RTS §10): o míssil derrubado com facilidade parece inútil.

### 4.4 WARNO (EUA)

- Pouco aproveitável pra poderes.
- As divisões têm caráter: a 3rd Armored é a ponta de lança, com M1A1HA, Apache e Bradley ([wiki, espelho](https://apis.emri.workers.dev/https-warno.fandom.com/wiki/3rd_Armored_Division)).
- O apoio fora do mapa: n/c.

### 4.5 C&C "Generals 2" / Command & Conquer (2013, alfa cancelado)

| Mecânica | O que faz | Criativo | Fonte |
|---|---|---|---|
| Baffle do Command Center | Passivo que impede poderes inimigos no raio do CC. A pesquisa "Pierce Baffle" fura o baffle do outro | A base tem escudo contra poderes, e furar vira tecnologia | [wiki](https://cnc.fandom.com/wiki/Command_center_(Generals_2)) |
| Orbital Collider (superarma da EU) | O jogador escolhe um raio grande e 3 alvos prioritários dentro dele. Caem 4 feixes | Mira com vários alvos: o jogador desenha o golpe | [wiki](https://cnc.fandom.com/wiki/Orbital_collider) |
| Suppressing Fire (Salvo) | Artilharia ao longo de um caminho desenhado pelo jogador | Golpe em linha, que se lê bem de cima | [wiki](https://cnc.fandom.com/wiki/Unreleased_content_(Generals_2)) |
| Reaper UAV (Ghost) | Chama um Reaper que dispara mísseis nos inimigos perto | Drone de vigia temporário | idem |
| Supply Drop | Caixa que cura, dá veterania e reforça o ataque por um tempo | Drop de apoio, não de tropa | idem |
| GPS Scrambler (versão 2) | Revela a névoa e cega as unidades inimigas | O mesmo nome com o efeito invertido | idem |
| Air Raid (Daedalus) | Ataque aéreo que sobe de dano com melhoria | — | idem |

Os números do alfa: n/c.

### 4.6 Tom Clancy's EndWar (JSF americano)

Fonte: [Mission Supports](https://tomclancy.wiki.gg/wiki/Endwar:Mission_Supports).

| Apoio | Números | Criativo |
|---|---|---|
| Air Strike | 4 / 5 / 6 CP por nível. N1 tira 25% da vida; N2 cerca de 50%, em vários alvos; N3 mata ou incapacita. Exige o alvo à vista e uma base aérea ao alcance | O nível exige uplinks melhorados: território |
| Electronic Warfare | 3 / 4 / 5 CP. O EMP tira o escudo do veículo, trava tudo e apaga as ordens; **desliga o mapa do inimigo**; destrói minas; não pega infantaria | Poder de informação |
| Force Recon | 3 / 4 / 5 CP. Reforço que entra pela borda, devagar | Reforço por terra, que pode ser interceptado |
| Forward Command | 6 CP. Leva o ponto de chegada pra um uplink amigo | Ponto de reforço móvel |
| WMD (USA: Rods from God) | Mata o que não tem escudo e destrói prédios e uplinks. Só o lado que está perdendo ganha a arma | A superarma serve à virada |

---

## 5. A USA real de hoje: o que vira poder (nome perto do real)

Uma linha por item: o que é → o poder que viraria.

| Sistema real | O que é (fonte) | Poder que viraria |
|---|---|---|
| **M142 HIMARS** | Lançador sobre rodas: 6 foguetes GMLRS ou 1 míssil ATACMS. Atira e sai ([Wikipedia](https://en.wikipedia.org/wiki/M142_HIMARS)) | **HIMARS Raid:** o caminhão entra pela borda, dispara a salva guiada e foge. Pode ser caçado. Esta é a nossa artilharia |
| **ATACMS / PrSM** | Míssil balístico tático; o ATACMS alcança cerca de 300 km; o PrSM o substitui ([ATACMS](https://en.wikipedia.org/wiki/MGM-140_ATACMS), [PrSM](https://en.wikipedia.org/wiki/Precision_Strike_Missile)) | **ATACMS Cluster:** balístico, quase sem aviso, espalha submunições. Interceptável só por defesa antimíssil |
| **MQ-9 Reaper** | Drone armado de longa espera, com Hellfire e bombas guiadas ([Wikipedia](https://en.wikipedia.org/wiki/General_Atomics_MQ-9_Reaper)) | **Reaper Overwatch:** drone circula X s, vê e atira sozinho nos alvos marcados; derrubável |
| **Switchblade 300/600** | Munição que vagueia. O 600 voa 40 km e espera cerca de 20 min; tem ogiva do Javelin; **pode abortar e ir pra outro alvo ("wave-off")** ([Wikipedia](https://en.wikipedia.org/wiki/AeroVironment_Switchblade)) | **Switchblade Swarm:** N drones-kamikaze que o jogador guia e pode desviar até o último segundo |
| **EA-18G Growler** | Avião de guerra eletrônica: pods de interferência e o míssil HARM contra radar ([Wikipedia](https://en.wikipedia.org/wiki/Boeing_EA-18G_Growler)) | **Growler Jamming:** faixa onde o antiaéreo e o radar inimigos ficam cegos por X s; abre caminho pros outros golpes |
| **ADM-160 MALD** | Míssil-isca que imita avião no radar ([Wikipedia](https://en.wikipedia.org/wiki/ADM-160_MALD)) | **MALD Decoys:** o Illusion do Contra com nome real |
| **AC-130J Ghostrider** | Canhoneira com 30 mm, 105 mm e munição guiada ([Wikipedia](https://en.wikipedia.org/wiki/Lockheed_AC-130)) | O nosso Spectre com nome atual |
| **B-21 Raider / B-2 Spirit** | Bombardeiros furtivos; o B-2 leva a GBU-57 MOP, de 30.000 lb ([B-21](https://en.wikipedia.org/wiki/Northrop_Grumman_B-21_Raider), [MOP](https://en.wikipedia.org/wiki/GBU-57A/B_MOP)) | **Bunker Buster:** dano só em prédio, atravessa abrigo; furtivo até soltar |
| **JDAM / GBU-39 SDB** | Kit GPS que torna a bomba comum guiada ([Wikipedia](https://en.wikipedia.org/wiki/Joint_Direct_Attack_Munition)) | **JDAM Strike:** precisão que depende do GPS; um jammer inimigo espalha o golpe |
| **Tomahawk Block IV** | Subsônico (Mach 0,74), cerca de 1.600 km; **pode esperar no ar e trocar pra 1 de 15 alvos ou um novo** ([Wikipedia](https://en.wikipedia.org/wiki/Tomahawk_(missile))) | **Tomahawk Loiter:** o míssil cruza o mapa devagar (interceptável), espera e o jogador escolhe o alvo depois |
| **F-35** | Furtivo, com fusão de sensores e data link que compartilha alvos ([Wikipedia](https://en.wikipedia.org/wiki/Lockheed_Martin_F-35_Lightning_II)) | **F-35 Sensor Sweep:** passa, revela e marca alvos pro exército todo por X s |
| **C-17 Globemaster III** | Cargueiro estratégico; leva 1 M1 Abrams; lança carga e tropa de paraquedas ([Wikipedia](https://en.wikipedia.org/wiki/Boeing_C-17_Globemaster_III)) | **Heavy Drop:** veículos de paraquedas (Stryker, JLTV); o cargueiro é o alvo |
| **JLTV** | Sucessor do Humvee ([Wikipedia](https://en.wikipedia.org/wiki/Joint_Light_Tactical_Vehicle)) | Unidade do drop rápido (com TOW ou Javelin) |
| **Stryker** | Blindado 8×8; tem versões M-SHORAD e de laser ([Wikipedia](https://en.wikipedia.org/wiki/Stryker)) | **Stryker Brigade Drop** |
| **CV-22 Osprey** | Tiltrotor: decola como helicóptero e voa como avião ([Wikipedia](https://en.wikipedia.org/wiki/Bell_Boeing_V-22_Osprey)) | **Osprey Insertion:** forças especiais em qualquer ponto; vulnerável no pouso |
| **Satélites (GPS, SBIRS)** | O SBIRS detecta lançamentos de míssil por infravermelho ([Wikipedia](https://en.wikipedia.org/wiki/Space-Based_Infrared_System)) | **Satellite Pass:** recon numa faixa, com hora prevista; **SBIRS Early Warning:** mostra o golpe inimigo que vem |
| **DE M-SHORAD** | Stryker com laser de 50 kW contra drones de grupo 1 a 3; teste de tiro real em Yuma em 2024 ([ExecutiveGov](https://www.executivegov.com/articles/army-tests-laser-variant-of-stryker-based-shorad-system)) | **Laser Shield:** derruba drones, foguetes e morteiros perto; gasta energia |
| **LaWS / HELIOS (Marinha)** | Laser de 30 kW no USS Ponce; o HELIOS tem cerca de 60 kW ([LaWS](https://en.wikipedia.org/wiki/AN/SEQ-3_Laser_Weapon_System), [HELIOS](https://en.wikipedia.org/wiki/HELIOS_(laser_weapon))) | Mesmo poder em versão de navio ou de base |
| **Golden Dome** | Escudo antimíssil anunciado em 20/05/2025, com sensores e interceptores no espaço ([Wikipedia](https://en.wikipedia.org/wiki/Golden_Dome_(missile_defense_system))) | **Golden Dome:** cúpula que intercepta mísseis, artilharia e bombas na área por X s |
| **Dark Eagle (LRHW)** | Hipersônico do Exército, Mach 5 ou mais, cerca de 3.500 km, ogiva cinética; bateria de 8 mísseis ([Wikipedia](https://en.wikipedia.org/wiki/Long-Range_Hypersonic_Weapon)) | **Dark Eagle:** golpe de 3 pontos quase sem aviso, raio pequeno, não interceptável; mira em 1 a 3 alvos |
| **E-7 Wedgetail / E-3 AWACS** | Radar voador ([Wikipedia](https://en.wikipedia.org/wiki/Boeing_E-7_Wedgetail)) | **AWACS:** revela todos os aviões e mostra a rota dos poderes aéreos inimigos |

---

## 6. As 15 ideias mais criativas pra nossa USA

Critério: chega pelo mapa, tem resposta e se lê de cima sem cobrir a batalha. Os custos e as patentes são sugestão.

| # | Ideia | O que é | De onde veio | Por que é bom num RTS visto de cima |
|---|---|---|---|---|
| 1 | **HIMARS Raid** (a nossa artilharia, 2 pts, I–III) | Um HIMARS de verdade entra pela borda (ou por um ponto de lançamento), dispara 6 / 12 / 18 foguetes guiados e foge | Plasma Strike do Townes + HIMARS real + o defeito da Artillery Barrage do ZH | A artilharia deixa de ser "do céu sem resposta": o inimigo vê o lançador e pode caçá-lo. As salvas são pontos marcados no chão, fáceis de ler |
| 2 | **MALD Decoys** (1 pt) | Uma onda de aviões-isca puxa o antiaéreo por cerca de 30 s | Illusion do Granger (Contra) + o MALD real | Recompensa a combinação (isca e depois o golpe). O contra-jogo é saber ler; de cima, aparecem traçantes subindo pro nada |
| 3 | **Growler Jamming** (2 pts) | Um Growler cruza o mapa e deixa uma faixa onde antiaéreo e radar ficam cegos por X s | Electronic Warfare do EndWar + o EA-18G real | Poder de "abrir porta" (SEAD), sem dano. Leitura: a faixa com interferência no minimapa e no chão. O Growler pode ser derrubado |
| 4 | **Target Designation** (1 pt) | Um satélite ou o F-35 marca um alvo ou uma área: revela camuflados e dá +30% de dano | Target Designation do Townes; +33% no Contra X; designador laser do Broken Arrow | Não mata nada sozinho, mas faz o exército focar. A marca vermelha é a leitura mais clara possível. Barato e útil sempre |
| 5 | **Guardian Angel / Golden Dome** (2 pts) | Um avião de defesa (ou cúpula) circula por 20 s e derruba mísseis, foguetes e artilharia na área | Guardian Angel TMD do RotR + SDI Cannon do Contra + Golden Dome e DE M-SHORAD reais | É a resposta da USA à artilharia da China, ao SCUD da GLA e à Rússia. Um poder que nega poder dá jogo de reação. Cúpula visível e de curta duração |
| 6 | **Forward Air Defense Drop** (1 pt, I–III) | Um C-17 solta 1 / 2 / 3 Patriot ou M-SHORAD que levam 15 s para montar | Defense Core Drop / Zone Capture da Alexander (Contra) + Turret Paradrop (SW) | Território na hora, com janela de 15 s pro inimigo reagir. Lê-se como "isto agora é meu" |
| 7 | **Forward Operating Base** (2 pts) | Cai um dozer (ou um posto) que vira o novo ponto de chegada dos reforços e dos drops | Dozer Drop (Contra) + Forward Command (EndWar) + Deploy War Factory (Contra) | Muda a geografia da partida. O inimigo sabe onde atacar (o posto) |
| 8 | **Strategic Bomber, o nível troca o avião** (3 pts, I–III) | I: B-52 lento e duro. II: B-1B rápido. III: B-21 furtivo até soltar | Carpet Bomb da Alexander (Contra) + Carpet Bombing do RotR | Cada nível pede outra resposta (muito antiaéreo, antiaéreo rápido, detector). Progressão que se vê no ar |
| 9 | **Tomahawk Loiter** (2 pts) | O míssil cruza o mapa em velocidade de avião, espera no ar e o jogador escolhe o alvo no fim; prédio alto e relevo bloqueiam | Tomahawk Block IV real (espera e troca de alvo) + B52 Cruise Missile do SW (o relevo bloqueia) | O mapa vira o contra-jogo, e o inimigo pode interceptar no caminho. De cima, um rastro lento e visível |
| 10 | **Switchblade Swarm** (1 pt, I–III) | 3 / 6 / 9 drones-kamikaze que o jogador guia; pode cancelar e escolher outro alvo até o fim | Switchblade 300/600 real ("wave-off") | Poder de habilidade, não de clique. Contra blindado leve e artilharia. O antiaéreo de curto alcance responde. Cada drone é um ponto pequeno, sem poluir a tela |
| 11 | **Reaper Overwatch** (2 pts) | O MQ-9 circula 60 s numa área e atira Hellfire sozinho em quem entra | Reaper UAV do C&C 2013 + MQ-9 do Broken Arrow + o real | Negação de área com tempo e com resposta (derrubar o drone). O círculo da órbita se lê bem |
| 12 | **SEAL Team / Osprey Insertion** (2 pts) | 2 Black Hawks ou 1 CV-22 entregam uma equipe de elite camuflada (ex.: 6 SEALs mais um JLTV no nível III) | SEAL Drop do Granger (Contra) + HALO Drop (SW) + Airborne Raid do RotR (com veículo) | Missão de infiltração com contra-jogo no pouso. O drop misto (tropa e veículo) é mais interessante que "N soldados" |
| 13 | **Air Mobility** (1 pt, passivo) | Destrava "chamar transporte": um grupo selecionado chama um C-17 ou um Osprey que o leva pelo mapa | Air Mobility do RotR | O ponto dá mobilidade ao exército inteiro, não um golpe. É a assinatura da USA (desdobrar rápido) e cria rotas aéreas que o inimigo pode emboscar |
| 14 | **AWACS / Early Warning** (1 pt, passivo) | Todo poder aéreo ou balístico inimigo mostra o tipo, a rota e o alvo assim que é chamado | Early Warning Radar e Tactical Warning System do Act of War + SBIRS e E-7 reais | Contra-jogo puro: dá ao defensor tempo pra dispersar. Barato, mas muda a partida. Combina com a regra do aviso (ESTUDO_RTS resumo 6) |
| 15 | **Dark Eagle** (3 pts, o golpe de 5 estrelas) | Hipersônico cinético: o jogador escolhe um círculo e até 3 alvos dentro; cai em segundos, sem fogo, e não pode ser interceptado | Orbital Collider do C&C 2013 (vários alvos) + Rods From God do SW e do EndWar + o LRHW real | Golpe máximo preciso e "limpo" (contra prédio e superarma). Diferente do cogumelo da China. Pede marca no chão por 5 s e recarga longa |

**Ideias boas que ficaram de fora do top 15**
- **Nano Swarm ao contrário:** o nível muda o papel (cura → dano). Serve de padrão de design pra qualquer poder nosso (Contra, Ironhand).
- **Training que vem de prédio e cresce com o tempo:** +1 nível de veterania a cada 2 min enquanto o prédio vive (Laser Control Station, Contra X).
- **CSAR / prisioneiro:** resgatar o piloto abatido (o nosso, pra manter a veterania) e capturar o inimigo, que vira dinheiro e informação (Act of War).
- **Electricity Net:** energia de emergência por 1 min (Townes, Contra).
- **Baffle do CC:** a base bloqueia poderes no raio dela, e furar vira pesquisa (C&C 2013).
- **Saboteur atrasa os poderes e F-22 nega os poderes aéreos:** contra-jogos baratos que já cabem nas unidades (Contra).

---

## Fontes principais

- **Contra (wiki, lida pela API da fandom):**
  - [Contra Wiki](https://zerohourcontra.fandom.com/wiki/Contra_Wiki)
  - [General's Promotion](https://zerohourcontra.fandom.com/wiki/General%27s_Promotion)
  - [Granger](https://zerohourcontra.fandom.com/wiki/General_Malcolm_%22Ace%22_Granger)
  - [Townes](https://zerohourcontra.fandom.com/wiki/General_%22Pinpoint%22_Townes)
  - [Alexander](https://zerohourcontra.fandom.com/wiki/General_Alexis_Alexander)
  - [Ironhand](https://zerohourcontra.fandom.com/wiki/General_Algrin_%22A.I.%22_Ironhand)
  - [Ironside](https://zerohourcontra.fandom.com/wiki/General_Ironside)
  - [Support Aircraft](https://zerohourcontra.fandom.com/wiki/Support_Aircraft)
  - [SDI Cannon](https://zerohourcontra.fandom.com/wiki/SDI_Cannon)
  - [Saboteur](https://zerohourcontra.fandom.com/wiki/Saboteur)
  - [Libra](https://zerohourcontra.fandom.com/wiki/Libra)
  - [Tech Reinforcement Pad](https://zerohourcontra.fandom.com/wiki/Tech_Reinforcement_Pad)
- **Contra (changelog oficial):** [Revora, tópico 74379](https://forums.revora.net/topic/74379-contra-008-009-changelog-topic/page-38). As páginas 38 e 39 trazem o 009 Final, os Patches 1–3 e os Hotfixes.
- **Contra X:**
  - [PPM, update 5](https://ppmforums.com/post-614327/contra-x-work-in-progress-news-update-5/)
  - [PPM, update 14](https://ppmforums.org/topic-71654/contra-x-work-in-progress-news-update-14-usa-laser-control-station)
- **ShockWave:**
  - [wiki](https://shockwave.fandom.com/wiki/United_States_of_America): Granger, Townes, Alexander, Ironside
  - [site oficial](http://shockwave.swr-productions.com/features.php)
- **Rise of the Reds:** [USA General's Promotion](https://generalsrotr.fandom.com/wiki/USA_Arsenal/General%27s_Promotion), com as páginas de cada poder.
- **Act of War:** [Early warning radar](https://actofwar.fandom.com/wiki/Early_warning_radar), [Prisoner of war](https://actofwar.fandom.com/wiki/Prisoner_of_war), [Tactical weapon](https://actofwar.fandom.com/wiki/Tactical_weapon).
- **World in Conflict:** [manual oficial](https://cdn.akamai.steamstatic.com/steam/apps/21760/manuals/manual_english.pdf).
- **Broken Arrow:** [Slitherine](https://www6.slitherine.com/news/broken-arrow-special-operations-forces-army-specialization).
- **EndWar:** [wiki.gg](https://tomclancy.wiki.gg/wiki/Endwar:Mission_Supports).
- **C&C 2013:** [Command center](https://cnc.fandom.com/wiki/Command_center_(Generals_2)), [Orbital collider](https://cnc.fandom.com/wiki/Orbital_collider), [Unreleased content](https://cnc.fandom.com/wiki/Unreleased_content_(Generals_2)).
- **USA real:** os links da Wikipedia e da ExecutiveGov citados na §5.
