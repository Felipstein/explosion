# Os poderes do general (estudo por cima, 03/11/2026)

O pedido (03/11): estruturar os poderes que se compram no painel do general com os pontos das patentes, de cada facção: cada
poder, custo, descrição. "Estudar muito, mas muito", com o C&C ZH e outros jogos como referência, "pra ter coisas que realmente
deixam o jogo muito foda, divertido pra caralho, balanceado e lindo, com vários e variados tipos de poderes". E tudo o que ele não
listou e muda o resultado. **Esta primeira volta é só o estudo, por cima; depois o debate facção por facção.**

**Isto é estudo e proposta. Nada aqui foi aprovado.** Os nomes e números são ponto de partida; a prova é a conta (§7) e a partida
IA × IA.

**As pesquisas completas** (com as fontes): `docs/poderes/ESTUDO_ZH.md` (o ZH pelos arquivos do jogo, o patch da comunidade e os
mods Contra, ShockWave e Rise of the Reds) e `docs/poderes/ESTUDO_RTS.md` (os outros RTS).

---

## 0. Resumo

1. **O ZH acerta na base e erra no preço.**
   - Acerta: a patente vem da luta, os aviões dos poderes podem ser derrubados, e o relógio sobrevive à queda do Command Center.
   - Erra: tudo custa 1 ponto, então uns poucos poderes são obrigatórios e outros ninguém pega. É bola de neve, e o alvo quase
     não é avisado.
   - A nossa estrutura (os pontos 1, 1, 2, 2, 3 e o custo de 1 a 3 pela força) já conserta o preço. Foi a conclusão do patch 1.06
     e do ShockWave EX.
2. **A régua de um poder bom** (§4):
   - é decisão, não botão;
   - tem resposta (o aviso e o avião derrubável);
   - a entrega é física;
   - lê de cima;
   - deixa marca no campo;
   - conta a facção;
   - não decide a partida sozinho.
3. **8 tipos de poder** (§5): olhos, golpe, chamar tropas, manter, negar, dinheiro, passivo e contra-poder. Toda facção tem o
   esqueleto comum, mais 2 ou 3 assinaturas só dela.
4. **A XP:**
   - perder lutando passa a valer mais (de 1/3 pra metade ou 3/4 de matar);
   - derrubar o avião de um poder dá XP;
   - os feitos e os prédios neutros também dão;
   - **o primeiro passo é medir em que minuto se chega em cada patente** (§6).
5. **O custo de usar:** recomendo o misto (§6.4). Tudo é grátis com recarga, menos as tropas chamadas, que ficam no campo e
   custam metade do preço.
6. **As 4 árvores, por cima** (§8): de 10 a 11 poderes cada, de 22 a 26 pontos.
   - A USA vê e acerta: o drone, o A-10, o AC-130, a MOAB e a espionagem das fábricas.
   - A China arrasa em massa: o comboio de fuzileiros, o Carpet Bomb, o napalm que queima o mapa e o EMP.
   - A GLA vive da ruína: a emboscada, o Bounty, a ruína armada, o túnel e o Anthrax.
   - A Rússia cava e queima: a barragem que deixa cratera, a linha de dentes de dragão, a guerra eletrônica e o Tornado-S.
7. **20 coisas que você não listou e mudam o resultado** (§9). Entre elas:
   - a tecnologia abrindo pela patente (o jeito do Contra);
   - a mira com ou sem visão;
   - o fogo amigo;
   - os poderes em time;
   - a IA usando os poderes;
   - a noite;
   - o custo na placa;
   - as vagas pros generais.

---

## 1. O que já está decidido (não se mexe aqui)

Vem do HUD (`docs/concept/hud/DECISOES.md` §7, 01/11) e da patente de combate (`docs/IDEIAS_JOGABILIDADE.md` §4, 11/10):

- **A patente vem da luta.** "Pras pessoas precisarem ir pra luta." Construir e economizar em casa não dá nada.
  - Hoje na simulação (`Units.json`, `rank`): XP = o valor destruído ÷ 10 + o valor que você perdeu ÷ 30. As patentes em
    0 / 600 / 1 500 / 3 000 / 5 000.
- **A patente também libera tecnologia** (prédio **e** patente): o Tomahawk na patente 3 já está no jogo. A War Factory nível 2 e
  os superpesados ainda vão entrar.
- **A partida começa com 1 estrela e 1 ponto**, como no ZH. O primeiro poder só fica pronto depois de carregar uma vez. Ele
  depende do Command Center. Não há patente 0.
- **O cardápio do general, sem trava:**
  - cada patente dá pontos: 1, 1, 2, 2, 3 (9 no total);
  - cada poder custa de 1 a 3 pontos, pela força;
  - compra o que quiser, na ordem que quiser, e não dá pra ter todos;
  - alguns poderes têm níveis I a III, cada nível na coluna da patente que o libera.
  - A trava "1 de 2 por patente" foi descartada (01/11).
- **A tela do general já está desenhada:**
  - as colunas são as 5 patentes;
  - as linhas são o tipo de poder (ar, tropas, fogo, apoio);
  - comprar é clicar no poder;
  - o que você vê do inimigo vem só da espionagem.
  - Os poderes que aparecem lá hoje são exemplos. **O conteúdo e o ritmo da XP ficaram pra esta sessão** ("acho que tá bom";
    o conteúdo vai pra uma sessão própria de balanceamento e ideias).
- **O que já existe na simulação:** o Cruise Missile Strike da USA (patente 2, cresce na 4, 6 → 9 Tomahawks, recarga de 180 s).
  Também o comando `UsePower` (lockstep, no replay).
- **Os generais (a visão dele, pra depois):**
  - USA: Air Force, Superweapons (o canhão eletromagnético) e Laser;
  - China: Flame e Infantaria;
  - Rússia: Tank e Nuclear;
  - GLA: Demolition, Stealth e Toxin.
  - Visão do jogo (24/10): "nenhum agora, como o Generals; depois o jogo cresce como o Zero Hour".
- **As superarmas estão fechadas** (`docs/SUPERARMAS.md`). Um poder nunca pode repetir o papel delas.

---

## 2. O que o ZH faz (pelos arquivos do jogo)

Tudo com a fonte (o arquivo e o bloco, ou a linha do código) em `docs/poderes/ESTUDO_ZH.md`.

**A regra:**
- **5 patentes** a 0 / 800 / 1 500 / 2 500 / 5 000 de XP. Dão 1, 1, 1, 1 e 3 pontos: 7 no jogo todo. A partida começa na
  patente 1 com 1 ponto.
- **A XP só vem de matar.** O dano não conta, e o valor do morto não segue o preço: um Stinger Site de $900 vale o mesmo que uma
  War Factory de $2 000. Derrubar o avião de um poder não dá XP.
- **Todo poder custa 1 ponto.** As fileiras abrem nas patentes 1, 3 e 5; a 2 e a 4 só dão ponto.
- **Comprou, está pronto.** O relógio é do jogador, não do prédio: perder o Command Center some com os botões, mas o relógio
  continua, e vários CCs dividem o mesmo relógio.
- **A maioria dos golpes vem de avião, e o avião pode ser derrubado:**
  - o A-10 tem 600 de vida;
  - os cargueiros e os bombardeiros têm 1 000.
  - A exceção é a Artillery Barrage: vem de um canhão fora do mapa que não pode ser atacado.
- **O inimigo quase não é avisado.** A marca no chão só aparece pra quem lançou. Só o GPS Scrambler e o Sneak Attack têm aviso
  de voz pro inimigo.

**O que cada facção tem:**

| | Patente 1 | Patente 3 | Patente 5 |
|---|---|---|---|
| **USA** | Paladin e Stealth Fighter (destravam a unidade), Spy Drone | Pathfinder (destrava), Paradrop I–III (5 / 10 / 20 Rangers), A-10 Strike I–III (1 / 2 / 3 aviões), Emergency Repair I–III | Fuel Air Bomb (a MOAB vem de uma melhoria de $4 000), Leaflet Drop, Spectre Gunship |
| **China** | Red Guard Training e Artillery Training (já nascem veteranos), Nuke Cannon (destrava) | Cluster Mines, Carpet Bomb, Artillery Barrage I–III (12 / 24 / 36 granadas), Cash Hack I–III ($1 000 / 2 000 / 4 000), Emergency Repair, Frenzy I–III | EMP Pulse (30 s sem veículos nem prédios, os seus também) |
| **GLA** | SCUD Launcher e Marauder (destravam), Technical Training | Hijacker (destrava), Rebel Ambush I–III (4 / 8 / 16), Cash Bounty I–III (5 / 10 / 20% de cada morte), Emergency Repair | Anthrax Bomb, Sneak Attack (o túnel em qualquer ponto), GPS Scrambler |

- **Os 9 generais** trocam de 2 a 4 peças da árvore da facção e mexem em números. Exemplos: o Air Force ganha o Spectre I–III e
  o Carpet Bomb na patente 1; o Tank ganha o Tank Drop; o Stealth deixa os Rebels camuflados.

**O que a comunidade diz** (as discussões do patch com os jogadores competitivos, os guias e o patch 1.06):
- **Os que todo mundo pega:**
  - USA: Pathfinder, A-10 e Spy Drone;
  - China: a Artillery Barrage completa (o combo Carpet + Artillery arrasava bases);
  - GLA: Cash Bounty.
- **Os que ninguém pega:**
  - Emergency Repair e Frenzy ("não servem na hora em que recarregam");
  - Paradrop II e III, Stealth Fighter, Hijacker.
- **Os chatos:**
  - o Carpet Bomb do Air Force cedo (os torneios proíbem antes da patente 3);
  - os Ambushes do Demo e do Toxin, que apagam exércitos.
- **"Todos a 1 ponto não faz sentido"** é do patch 1.06 do H2 Clan (2008). Ele triplicou os pontos e cobrou de 1 a 4 por poder,
  pela força. Uma das metas era quebrar o combo de abertura Carpet + Artillery II na patente 3.
- **Cada nível tem de dobrar o efeito** (o ZH faz: 4 / 8 / 16).
- **Todo golpe precisa de uma reação possível do outro lado.**

**Os mods:**
- **Contra 009:** a patente abre a tecnologia em camadas, sem gastar ponto. A 3 abre o meio do jogo (os prédios de tecnologia); a
  5 abre as superunidades e as superarmas. O ponto compra poder, unidade e habilidade.
  - O próprio time admite a bola de neve: quem chega primeiro na 3 leva grande vantagem. Eles deram XP extra à IA que fica pra
    trás.
- **Rise of the Reds:**
  - o ponto da patente 1 escolhe um ramo da tecnologia (a Rússia: aviação, armas avançadas ou nuclear);
  - a patente 3 tem os poderes em 3 níveis;
  - o Field Promotion sobe a veterania de tudo numa área (o ZH não tem).
- **ShockWave EX (2023):** 2 pontos por patente e os golpes a 1, 2 e 3 pontos por nível. É a mesma conclusão do 1.06.

---

## 3. O que os outros RTS fazem

Levantados 15 jogos (BFME 1 e 2, RA3, C&C 3 e 4, World in Conflict, CoH 1 a 3, Halo Wars 1 e 2, SC2 co-op, Tempest Rising,
DoW 2 e 3, Act of War, Broken Arrow, WARNO, AoM). A tabela de todos lado a lado e as fontes estão em `docs/poderes/ESTUDO_RTS.md`.

**O que o mercado ensina:**
1. **Escolher e usar são coisas separadas.** O ponto destrava o poder uma vez; cada uso paga a recarga, às vezes também dinheiro
   ou munição (RA3, BFME, CoH, Halo Wars 2, Tempest Rising).
2. **Os melhores também pagam quem perde:**
   - no RA3, perder uma unidade enche a barra 4× mais que matar;
   - no DoW2, o dono do morto leva 75% do valor;
   - no World in Conflict, capturar, fortificar e consertar também dão ponto.
   - O nosso hoje: perder vale 1/3 de matar.
3. **Níveis que mudam o "como"** valem mais que +10%. Os níveis também sobem a recarga (RA3: a Time Bomb vai de 120 a 300 s).
4. **As recargas do mercado:**
   - olhos e fumaça: 20–60 s;
   - golpe médio: 90–180 s;
   - golpe grande: 300–450 s;
   - o máximo: 360–600 s (no SC2 ele já nasce em recarga).
5. **O alvo vê chegar e tem resposta:**
   - 3 a 25 s de atraso;
   - a fumaça vermelha no chão (CoH);
   - o som que todos ouvem (C&C 3);
   - a bomba ou o avião que dá pra derrubar: a Time Bomb do RA3 tem 500 de vida, e derrubar a Final Squadron dá ponto.
6. **O poder máximo tem atraso, recarga longa e deixa chance de virada.** No World in Conflict a nuke custa caro, recarrega em
   450 s e deixa neutros os pontos da área.
7. **Os erros conhecidos:**
   - o poder que todo mundo pega (Rallying Call no BFME 2);
   - o poder destravado fora da partida (CoH 2 virou "pay to win");
   - a árvore confusa (o beta do Halo Wars 2);
   - o poder de uso único, que ninguém gasta (AoM);
   - o superpoder forte demais (o DoW 3 subiu a recarga de 5 pra 7,5 min).
8. **Em time:**
   - doar pontos ao aliado (World in Conflict);
   - o bônus que respinga no aliado (RA3: 25% pro dono, 5% pro time);
   - o aliado vê o alvo antes do golpe.
9. **Pagar o poder em dinheiro** (C&C 3) faz quem tem mais dinheiro ter mais poder. Isso pesa contra o caminho B (§6.4).

---

## 4. A régua: o que faz um poder ser bom

Sete regras, tiradas das duas pesquisas e das regras do projeto:

1. **Cada poder é uma decisão, não um botão de dano.** Tem um onde e um quando que pesam. O mesmo poder usado bem ou mal dá
   resultados muito diferentes.
2. **Todo poder tem resposta.**
   - O alvo vê chegar: a sombra do avião, o som, a marca no chão.
   - O avião que entrega pode ser derrubado. A área dá pra largar a tempo.
   - Quem acerta a resposta ganha alguma coisa (§6.2).
3. **O poder vive no mundo.**
   - **A entrega é física:** o avião entra pela borda do mapa do lado do dono, a granada voa de fora do mapa, o paraquedista
     desce.
   - A defesa antiaérea passa a valer contra os poderes. O efeito segue as regras de sempre: a fumaça e o fogo em volume, na luz
     do mundo.
4. **Lê de cima e não tapa a batalha.** Câmera de RTS. Efeito que cobre as unidades é defeito, mesmo sendo bonito.
5. **Deixa marca no campo**, quando faz sentido (o pilar do jogo):
   - cratera;
   - a vegetação queimada pro resto da partida;
   - o prédio caindo a partir do ponto do impacto;
   - a trincheira, o campo de minas, a carcaça.
6. **Conta a história da facção.** Cada facção resolve os mesmos problemas do seu jeito, e cada uma tem um poder-assinatura que
   só ela tem.
7. **Não decide a partida sozinho e não premia só quem já está ganhando.** O poder máximo é menor que a superarma. A virada tem
   que existir (§6.3).

---

## 5. Os tipos de poder (o leque)

"Vários e variados tipos." Cada facção tem de todos os tipos, mas cada uma do seu jeito:

| Tipo | O que faz | Exemplo do ZH | O que o nosso jogo acrescenta |
|---|---|---|---|
| **Olhos** | revela uma área, mostra o que se esconde | Spy Drone, Satellite Hack | **ver o que a fábrica inimiga produz** por um tempo; a contra-bateria: de onde veio o tiro |
| **Golpe** | dano numa área ou num alvo | A-10, Carpet Bomb, Artillery Barrage, Anthrax Bomb | o golpe que **derruba o prédio do ponto do impacto**; o napalm que deixa a vegetação queimada |
| **Chamar tropas** | unidades chegam onde você quer | Paradrop, Rebel Ambush | **a chegada pelo jeito da facção**: o comboio da China pela estrada, o trem da Rússia, o túnel da GLA |
| **Manter** | conserto, cura, reforço | Emergency Repair | o conserto que só vale em quem está lutando (não em casa) |
| **Negar** | trava, atrapalha, esconde | EMP Pulse, Cluster Mines, GPS Scrambler, Leaflet Drop | a **guerra eletrônica** (os mísseis guiados erram), a cortina de fumaça, a **linha fortificada** que o poder levanta |
| **Dinheiro** | renda por combate ou roubo | Cash Bounty, Cash Hack | dinheiro **só vindo da luta** (abater, sucatear), nunca de ficar em casa |
| **Passivo** | destrava unidade, veterania, melhoria | Pathfinder, Red Guard Training, Technical Training | o jogador que não quer clicar tem escolha; o destravar de unidade passa pra patente sozinha (§8) |
| **Contra-poder** | responde ao poder do inimigo | — | o guarda-chuva antiaéreo, o alerta antecipado, o EMP que pausa a carga de uma superarma |

**O esqueleto comum e a assinatura:**
- Toda facção tem uma versão de olhos, golpe, chamar tropas e manter. Isso facilita o equilíbrio e o jogador sabe o que esperar.
- Por cima disso, cada facção tem 2 ou 3 poderes que só ela tem.

---

## 6. A economia dos pontos e da XP

### 6.1 O ritmo

- 9 pontos na partida inteira (o ZH dá 7). A árvore de cada facção custa bem mais que isso (proposta: de 22 a 26 pontos, §8).
  Ninguém tem tudo, e duas partidas com a mesma facção podem jogar diferente.
- **Alvo de ritmo a medir** (proposta, numa partida 1×1 de ~25 min):
  - a patente 2 na primeira luta de verdade (~5–7 min);
  - a 3 no meio do jogo (~10–12 min);
  - a 5 perto do fim, junto com a primeira superarma (~20–25 min).
  - Hoje ninguém sabe em que minuto se chega em cada patente: **o primeiro passo do balanceamento é medir** na partida IA × IA
    (pôr a XP no placar por minuto do teste).

### 6.2 De onde vem a XP (além de destruir)

- **Perder lutando.** Já existe, mas fraco: hoje perder vale 1/3 de matar. No RA3 vale 4×, e no DoW 2 vale 0,75×.
  - **Proposta:** perder passa a valer de metade a 3/4 de matar (÷15 a ÷13 no lugar de ÷30), sem nunca passar de matar.
  - Quem se esconde continua sem ganhar nada.
- **O valor certo de cada coisa:** o nosso já segue o preço, o que conserta o erro do ZH (o Stinger Site de $900 valendo o
  mesmo que a War Factory).
- **Feitos** (as comendas da tela do general):
  - derrubar o avião de um poder inimigo (premia a resposta, regra 2);
  - interceptar um SCUD;
  - destruir uma superarma;
  - ganhar uma luta em desvantagem;
  - expulsar o ataque de dentro da base.
- **Segurar os prédios neutros do mapa** (decidido em 11/10, ainda não existe).
- **Nada da economia** dá XP.

### 6.3 Contra a bola de neve

- O poder de patente 1 de cada facção inclui um de defesa (manter ou negar). Quem perdeu a primeira luta não fica sem resposta.
- Os golpes fortes ficam nas patentes 3 a 5. Quem chega lá primeiro venceu lutas, e o inimigo tem a defesa antiaérea pra
  responder.
- **A medir:** o perdedor de uma luta ganha XP suficiente pra não ficar uma patente inteira atrás?

### 6.4 O custo de usar (decisão dele)

No ZH, comprado o poder, usar é de graça: só a recarga. Nas superarmas ele quis custo de lançar. Três caminhos:

| | Como é | A favor | Contra |
|---|---|---|---|
| **A. O do ZH** | o ponto paga, usar é grátis, só a recarga | a nostalgia; simples | o poder é dinheiro de graça (o Paradrop dá soldados sem custo) |
| **B. Tudo pago** | cada uso custa dinheiro, como o C&C 3 | cada uso disputa com o tanque: decisão | pune quem está sem dinheiro, justo quem está perdendo |
| **C. O misto (recomendo)** | tudo é grátis com recarga, **menos as tropas chamadas**: elas custam metade do preço das unidades, mostrado no botão | as tropas ficam no campo depois do poder (o Paradrop III do ZH dava 20 Rangers de graça a cada 4 min, ~$4 500); o resto continua leve | duas regras pra explicar (o preço no botão resolve) |

O mercado avisa contra o B: no C&C 3, quem tem mais dinheiro tem mais poder (`docs/poderes/ESTUDO_RTS.md` §12.2).

---

## 7. O balanceamento (o método)

- **A conta:** o dano esperado de cada uso, sobre a base de referência do estudo das superarmas
  (`docs/superarmas/superarmas_conta.py`, o mesmo modelo), em dólares destruídos. Ela se compara:
  - com o que o jogador ganha no tempo da recarga (um caminhão rende ~$1 050/min, `docs/ECONOMIA.md`);
  - com o custo de fazer o mesmo estrago com unidades.
- **A ordem de grandeza** (estimativa, a confirmar pela conta): o poder de 1 ponto rende por uso menos que um tanque; o de 3
  pontos, alguns tanques; o poder máximo fica bem abaixo da superarma.
- **A prova:**
  - a partida IA × IA com os poderes;
  - se um poder é escolhido em quase toda partida, ele está forte demais;
  - se ninguém escolhe, está fraco ou chato.
- **A recarga** (o ZH usa 4:00 em quase tudo; a faixa do mercado na coluna da direita):

  | Custo | A proposta | O mercado |
  |---|---|---|
  | 1 ponto | 1:00 a 2:00 | 20 a 90 s |
  | 2 pontos | 2:30 a 4:00 | 90 a 240 s |
  | 3 pontos (o máximo) | 6:00 a 8:00 | 300 a 600 s |

  - O nível novo dobra o efeito (a regra do ZH: 4 / 8 / 16) e pode subir a recarga.
- **O atraso até o golpe:**
  - de 3 a 6 s nos pequenos;
  - de 10 a 16 s no máximo.
  - É o tempo que o alvo tem pra reagir.
- **O poder máximo** nasce carregando: comprou, ainda espera a primeira carga.
- **Qualquer combo de abertura** (o Carpet + Artillery II do ZH na patente 3) se testa de propósito na partida IA × IA.

---

## 8. As 4 facções, por cima (o rumo de cada árvore, pra debater)

**É o rumo, não a lista final.** Cada árvore tem de 10 a 11 poderes e custa de 22 a 26 pontos. O jogador leva 9.

- **O que é cada coluna:**
  - **Pat.** é a patente que libera o poder;
  - **Pts** é o custo em pontos, com o "+1" de cada nível seguinte;
  - **Origem** diz se o poder é do ZH, se é do ZH com uma correção ou se é novo.
- **As duas regras que valem pra todas:**
  - toda facção tem olhos, golpe, chamar tropas e manter;
  - toda patente tem pelo menos 2 boas opções de 1 ponto, de tipos diferentes.
- **A tecnologia abre pela patente sozinha** (proposta, o jeito do Contra), como o Tomahawk na patente 3 já faz. O ponto não
  compra unidade, como comprava no ZH. Com isso, o ponto só compra poder e treino.

### 8.1 USA: vê tudo, acerta primeiro

O ar, a informação e o tiro exato. Os golpes vêm de avião, que se derruba. O poder-assinatura é saber o que o inimigo faz.

| Pat. | Poder | Tipo | Pts | A ideia | Origem |
|---|---|---|---|---|---|
| 1 | **Spy Drone** | olhos | 1 | o drone camuflado circula e revela a área; dá pra derrubar | ZH |
| 1 | **Emergency Repair** I–III | manter | 1 +1 +1 | o conserto na hora **e** o casco reforçado por 10 s, pra servir no meio da luta | ZH corrigido |
| 2 | **Cruise Missile Strike** I–II | golpe | 2 +1 | os 6 → 9 Tomahawks, já no jogo | nosso (23/10) |
| 2 | **Paradrop** I–III | tropas | 1 +1 +1 | Rangers e Javelins juntos, de paraquedas (o patch da comunidade misturou os dois) | ZH corrigido |
| 3 | **A-10 Strike** I–III | golpe | 2 +1 +1 | 1 / 2 / 3 A-10 com mísseis e o canhão numa faixa; obrigatório no ZH, por isso custa 2 | ZH |
| 3 | **Satellite Intel** | olhos | 1 | por 60 s, a fila das fábricas inimigas e os poderes que o general inimigo tem | novo (assinatura) |
| 3 | **Precision Strike** | golpe | 2 | uma bomba guiada num prédio, que cai a partir do ponto do impacto | novo (o campo que muda) |
| 4 | **Fighter Sweep** | contra-poder | 1 | 2 Raptors guardam uma área por 30 s e derrubam os aviões dos poderes inimigos | novo |
| 4 | **Leaflet Drop** | negar | 2 | desliga a infantaria e os veículos numa área por 20 s | ZH (era da patente 5) |
| 5 | **AC-130 Gunship** | golpe | 3 | gira 15 s sobre a área, e você escolhe os alvos (o Spectre) | ZH |
| 5 | **MOAB** | o máximo | 3 | o B-2 com a bomba grande; o maior golpe da USA | ZH |

As vagas pros generais: o Air Force mexe no A-10, no AC-130 e no Fighter Sweep; o Laser troca o Precision Strike pelo laser do
satélite; o Superweapons troca a MOAB.

### 8.2 China: arrasa em massa

A horda, o fogo, a propaganda e o hacker. Sem nuclear (é da Rússia) e sem drones (a superarma).

| Pat. | Poder | Tipo | Pts | A ideia | Origem |
|---|---|---|---|---|---|
| 1 | **Satellite Hack** | olhos | 1 | o hacker revela uma área pelo satélite inimigo | ZH (era do prédio) |
| 1 | **Red Guard Training** | passivo | 1 | os fuzileiros já nascem veteranos | ZH |
| 1 | **Emergency Repair** I–III | manter | 1 +1 +1 | como o da USA, com a cara da China | ZH corrigido |
| 2 | **Propaganda Broadcast** I–III | reforço | 1 +1 +1 | o Frenzy refeito: +25 / 50 / 100% de dano e +velocidade numa área (no ZH, +10% não servia pra nada) | ZH corrigido |
| 2 | **Cluster Mines** | negar | 2 | o cargueiro solta um campo de minas | ZH |
| 3 | **Carpet Bomb** | golpe | 2 | os bombardeiros H-6 em fila, 10 bombas pesadas | ZH |
| 3 | **Cash Hack** I–III | dinheiro | 1 +1 +1 | rouba $1 000 / 2 000 / 4 000 do prédio de suprimento inimigo | ZH |
| 3 | **Reinforcement Convoy** I–III | tropas | 2 +1 +1 | os caminhões entram pela borda do seu lado com 6 / 12 / 18 fuzileiros; a estrada importa, e dá pra emboscar | novo (assinatura: a horda) |
| 4 | **Napalm Strike** | golpe | 2 | os jatos riscam uma faixa de fogo; a vegetação fica queimada pro resto da partida | novo (assinatura: o campo que muda) |
| 5 | **EMP Pulse** | o máximo | 3 | desliga os veículos e os prédios numa área por 30 s, inclusive os seus | ZH |

**O conflito a decidir:** a Artillery Barrage é a mais escolhida da China no ZH, e a artilharia é a identidade da Rússia. Dá pra
deixar com a Rússia e dar à China o Carpet Bomb e o napalm. Também dá pra dar à China uma saraivada de foguetes (a massa) e à
Rússia a barragem de tubo que anda (a precisão pesada).

As vagas pros generais: o Flame mexe no napalm e no Carpet Bomb; o Infantaria mexe no comboio e no treino.

### 8.3 GLA: vive da ruína

A emboscada, o esconder, o veneno e o sucatear. Sem força aérea: os poderes chegam por terra, por túnel ou num cargueiro velho
(o Contra fez o mesmo, com o avião próprio da GLA).

| Pat. | Poder | Tipo | Pts | A ideia | Origem |
|---|---|---|---|---|---|
| 1 | **Technical Training** | passivo | 1 | as Technicals já nascem veteranas | ZH |
| 1 | **Emergency Repair** I–III | manter | 1 +1 +1 | o conserto do ferro-velho | ZH corrigido |
| 1 | **Informants** | olhos | 1 | os civis do mapa contam: revela o inimigo perto das casas civis por 30 s | novo |
| 2 | **Rebel Ambush** I–III | tropas | 1 +1 +1 | 4 / 8 / 16 Rebels surgem no ponto | ZH |
| 2 | **Cash Bounty** I–III | dinheiro | 2 +1 +1 | 5 / 10 / 20% do custo de cada morte inimiga; obrigatório no ZH, por isso custa mais | ZH |
| 3 | **Booby-Trapped Ruins** | negar | 2 | arma as carcaças e as ruínas de uma área, e elas explodem quando o inimigo passa | novo (assinatura: a ruína) |
| 3 | **GPS Scrambler** | negar | 2 | camufla as suas unidades numa área | ZH (era da patente 5) |
| 4 | **Black Market** | dinheiro | 1 | por 60 s, o Worker desmancha carcaça 2× mais rápido e ela rende o dobro | novo |
| 4 | **Sneak Attack** | tropas | 2 | o túnel brota em qualquer ponto do mapa | ZH (era da patente 5) |
| 5 | **Anthrax Bomb** | o máximo | 3 | o cargueiro velho solta o gás; o campo de veneno fica 60 s | ZH |

As vagas pros generais: o Demolition mexe no Booby-Trapped Ruins e no Ambush; o Stealth mexe no GPS e no Sneak Attack; o Toxin
mexe no Anthrax.

### 8.4 Rússia: cava e queima

A artilharia, a guerra eletrônica, a fortificação e o terreno. É a facção que mais mexe no chão de propósito.

| Pat. | Poder | Tipo | Pts | A ideia | Origem |
|---|---|---|---|---|---|
| 1 | **Orlan Recon** | olhos | 1 | o drone revela a área por 40 s, e a sua artilharia acerta melhor ali | novo |
| 1 | **Field Workshop** I–III | manter | 1 +1 +1 | o conserto, com a cara russa | ZH corrigido |
| 1 | **Fortified Line** | negar | 1 | a engenharia levanta uma linha de dentes de dragão e trincheira onde você riscar; fica na partida | novo (assinatura: o chão) |
| 2 | **Artillery Barrage** I–III | golpe | 2 +1 +1 | "a deusa da guerra": a barragem que anda e deixa crateras que atrasam quem passa; o aviso dá tempo de sair | ZH refeito |
| 2 | **VDV Drop** I–III | tropas | 1 +1 +1 | os paraquedistas; no III, com um blindado BMD de paraquedas | novo |
| 3 | **Krasukha Jamming** | negar | 2 | por 30 s numa área, os mísseis guiados inimigos erram, os drones e os aviões de poder perdem o alvo, e o minimapa inimigo apaga | novo (assinatura: a resposta à precisão da USA) |
| 3 | **Smoke Screen** | negar | 1 | as granadas de fumaça tapam a visão inimiga de uma área por 40 s; a fumaça é em volume, e quem joga continua lendo as unidades | novo |
| 4 | **Remote Mining** | negar | 2 | os foguetes espalham minas antitanque numa faixa | novo (próximo das minas da China: decidir) |
| 4 | **Glide Bomb Strike** | golpe | 2 | as bombas planadoras caem de longe, sem o avião passar por cima; a resposta é o aviso de 8 s | novo |
| 5 | **Tornado-S Salvo** | o máximo | 3 | 12 foguetes pesados com submunições; o chão fica crivado | novo |

**Guardada:** o trem de reforço (o depósito ferroviário traz tanques). Pede trilho no mapa (um tipo de mapa do WorldForge).

As vagas pros generais: o Tank mexe no VDV e na oficina; o Nuclear troca o Tornado-S; a Artilharia, se vier, mexe na barragem.

### 8.5 Os quatro juntos

| Tipo | USA | China | GLA | Rússia |
|---|---|---|---|---|
| Olhos | Spy Drone, Satellite Intel | Satellite Hack | Informants | Orlan |
| Golpe | Cruise, A-10, Precision, AC-130 | Carpet, Napalm | — | Barrage, Glide Bomb |
| Tropas | Paradrop | Convoy | Ambush, Sneak Attack | VDV |
| Manter | Emergency Repair | Emergency Repair, Broadcast | Emergency Repair | Field Workshop |
| Negar | Leaflet | Cluster Mines | Booby Traps, GPS | Fortified Line, Krasukha, Smoke, Mining |
| Dinheiro | — | Cash Hack | Bounty, Black Market | — |
| Passivo | — | Red Guard Training | Technical Training | — |
| Contra-poder | Fighter Sweep | — | — | Krasukha |
| O máximo | MOAB | EMP | Anthrax | Tornado-S |

**Os buracos que a tabela mostra:**
- A GLA não tem golpe médio (no ZH também não tem).
- A USA e a Rússia não têm dinheiro, nem passivo.
- A China e a GLA não têm contra-poder.

**Fica pro debate de cada facção:** completar esses buracos de propósito, ou deixar como identidade.

---

## 9. O que você não listou e muda o resultado

**A regra do jogo**
1. **O ritmo da XP** (§6.1): em que minuto cada patente chega. É o que mais muda a sensação da partida, e hoje não está medido.
2. **O que cada patente libera além dos pontos:** a tecnologia (War Factory nível 2, superpesados, a superarma?). Patente e poder
   são a mesma escada; precisam ser desenhados juntos.
3. **A mira precisa de visão?** Na superarma você reprovou ("nerfa muito"). Pro poder de chamar tropas, o ZH e o protótipo dizem
   "onde você vê". Decidir poder a poder.
4. **Sem Command Center, sem poderes.** O ZH resolve bem, e a proposta é copiar:
   - o relógio é do jogador e continua correndo;
   - os botões somem;
   - o CC reconstruído devolve o poder com o tempo que sobrou;
   - dois CCs não dão dois disparos.
5. **Vários poderes prontos de uma vez:** sem limite, um jogador guarda tudo e solta junto (o "alfa" que ninguém responde).
   Proposta: nada que trave; a resposta é o aviso e a defesa.

**O contra-jogo**
6. **A entrega física pode ser derrubada:**
   - o avião do poder entra pela borda do mapa do lado do dono, e a antiaérea derruba;
   - o míssil e a granada não se derrubam, com uma exceção: o Patriot contra o SCUD, já decidido;
   - derrubar dá XP (§6.2).
7. **O aviso pra vítima:** a sombra, o som de longe, a marca no chão nos segundos finais. Igual pras quatro facções, cada uma
   com a sua cara.
8. **Os contra-poderes:** a antiaérea por um tempo numa área, o alerta antecipado, a contra-bateria.

**O mundo**
9. **O campo que muda** (o pilar do jogo):
   - o napalm queima a vegetação pra sempre;
   - a granada deixa cratera que atrasa quem passa;
   - o golpe preciso derruba o prédio a partir do ponto do impacto;
   - o poder russo levanta a linha de dentes de dragão;
   - a GLA arma a ruína.
10. **A noite:** o sinalizador que acende uma área, e a fumaça que ele ilumina (na luz do mundo). Os mapas de noite já existem.
11. **O fogo amigo:** o golpe bate no seu e no aliado? No ZH bate. Com ele, o poder exige cuidado (decisão).

**Os jogadores**
12. **Partida em time (2×2, co-op):** os poderes de aliados somam no mesmo alvo? Um aliado pode dar ponto ao outro? A XP de quem
    ajudou a matar?
13. **A IA usando os poderes:** quando e onde, sem virar a "apelona" (a reclamação de 24/10). Ela precisa usar bem desde o
    começo, ou o jogo contra a IA fica sem graça.
14. **A opção da sala:** poderes ligados ou desligados e "começar na patente X" pra partida rápida, como o liga/desliga das
    superarmas.
15. **O teclado:** o ZH tem 11 atalhos de poder especial. A coluna de poderes do HUD já está aprovada.

**A produção**
16. **Os nomes perto do real** (a decisão de 24/10), por exemplo:
    - USA: o A-10 Warthog, o AC-130, o B-52;
    - China: o bombardeiro H-6;
    - Rússia: o Su-25, a Krasukha, o Iskander;
    - GLA: as técnicas e o túnel.
17. **O custo na placa:** um bombardeio de 30 bombas é 30 explosões em volume. Cada poder entra no orçamento do BlastFx (o
    desempenho junto com a beleza).
18. **O som e a voz** de cada poder: o seu ("Airstrike inbound") e o do inimigo ("Enemy aircraft detected"). Fica por último, como
    combinado.
19. **Os ícones e a tela do general:** o conteúdo final entra no protótipo aprovado; um ícone por poder (a arte).
20. **Os generais depois:** cada general troca 2 a 4 poderes da árvore da facção e mexe em números (o jeito do ZH). A árvore base
    precisa ter as vagas pensadas pra essa troca desde já.

---

## 10. O que é seu decidir (no debate)

Pra cada facção, no debate:
1. **O custo de usar** (§6.4): A, B ou C (recomendo o C).
2. **A mira com ou sem visão**, nos poderes de chamar tropas (§9, item 3).
3. **O fogo amigo** dos golpes (§9, item 11).
4. **O rumo de cada árvore** (§8): o que fica, o que sai, o que falta; a assinatura de cada facção.
5. **Quem fica com a artilharia:** a China tem a Artillery Barrage no ZH, e a artilharia é a identidade da Rússia (§8).
6. **O ritmo:** o alvo de minuto por patente (§6.1), que a medida confirma.
7. **A tecnologia pela patente sozinha** (o Contra, §8) ou o ponto comprando unidade (o ZH).
8. **O peso de perder lutando** na XP (§6.2): de 1/3 pra metade ou 3/4 de matar.
9. **Matar com poder dá XP?** No ZH, não (e o avião derrubado também não). A proposta: derrubar o avião dá XP; matar com o poder,
   metade.

---

## 11. A ordem

1. Este estudo. Depois o debate, uma facção por vez (proposta: a USA primeiro, que tem o Cruise Missile Strike e os exemplos da
   tela).
2. Pra cada facção fechada: os números pela conta, a lista na tela do general (o protótipo) e os ícones.
3. A simulação: o poder genérico (área, tropas, revelar, conserto) a partir do `UsePower` que já existe, e a XP no placar do teste
   pra medir o ritmo.
4. A IA usando os poderes.
5. Os efeitos no laboratório, um de cada vez, em vídeo.
6. O som e a voz, por último.

---

## 12. As decisões dele (03/11)

- **O estudo e as recomendações: aprovados** ("top blz, e sim eu concordo com tudo"). Valem:
  - o custo de usar misto (§6.4): tudo grátis com recarga, menos as tropas chamadas, que custam metade do preço;
  - a tecnologia abrindo pela patente sozinha (§8);
  - perder lutando valendo de metade a 3/4 de matar (§6.2);
  - derrubar o avião de um poder dá XP, e matar com o poder dá metade (§10, item 9);
  - o relógio do jogador que sobrevive ao Command Center (§9, item 4).
- **A artilharia não é de uma facção só.** O conflito de §8.2 acabou: como no Contra, cada facção (ou general) tem a sua artilharia,
  do seu jeito: a de fogo na China, a do Assault na GLA, a de laser na USA Laser. "Lá tem muita criatividade, dá pra olhar lá
  também, não fique tão enviesado assim com o C&C ZH."
- **O debate começa pela USA**, e o conteúdo entra na tela de poderes que já existe (o protótipo aprovado do HUD).
