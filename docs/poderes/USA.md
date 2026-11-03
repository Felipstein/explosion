# Os poderes do general da USA (aprovada em 03/11/2026)

**O estado final:** a árvore da §2 com as trocas aplicadas da §11 (a artilharia naval no lugar do Cruise e do Precision Strike, o
Paradrop III no salto HALO, o Emergency Repair corrigido). As tabelas da §3 e da §4 ainda trazem as linhas antigas do Cruise e do
Precision: valem os números da §11. **Falta: um ícone de verdade pra cada poder** (pedido dele, antes da próxima facção).

O pedido: começar o debate pela USA, com o conteúdo na tela de poderes que já existe, e sem ficar preso ao ZH ("o Contra tem muita
criatividade, dá pra olhar lá"). O estudo de base é `docs/PODERES_DO_GENERAL.md` (aprovado em 03/11). A pesquisa só da USA (o Contra
com os 4 generais americanos, o ShockWave, o Rise of the Reds, outros RTS e a USA real) está em `docs/poderes/ESTUDO_USA.md`. A conta
de cada golpe está em `docs/poderes/poderes_conta.py`, na mesma base de referência das superarmas.

**Proposta pra debate. Nada aqui foi aprovado.** A tela com a árvore: `docs/poderes/tela/poderes_usa.html` (abre direto no navegador;
a foto é `docs/poderes/tela/poderes_usa.jpg`). Os ícones dos poderes novos são provisórios. Como refazer tudo: §8.

---

## 1. A ideia da árvore

**A USA vê tudo e acerta primeiro.** Tudo que ela faz começa por saber: o drone vê, o satélite marca, o AWACS avisa. Depois vem o
golpe exato, que chega de avião e pode ser derrubado. As três coisas que só ela tem:
- **o alvo marcado** (Target Designation): o que está na marca leva mais dano, e o inimigo vê a marca e sabe que vai apanhar;
- **o aviso antecipado** (AWACS): os poderes do inimigo aparecem pra você com o tipo, a rota e o alvo assim que ele chama;
- **o escudo** (Guardian Angel): por 20 s, os mísseis, os foguetes e as granadas que caem numa área são derrubados no ar.

**A régua de força** (a conta, §4): 1 ponto ≈ um tanque por uso; 2 pontos ≈ um prédio; 3 pontos ≈ metade de uma superarma. A MOAB e o
A-10 do ZH ficaram fora da régua (o A-10 apagava um exército de 18 unidades com um avião só) e foram recalibrados.

---

## 2. A árvore na tela (4 linhas × 5 patentes, um poder por casa)

| | ★ 1 (+1 ponto) | ★★ 2 (+1) | ★★★ 3 (+2) | ★★★★ 4 (+2) | ★★★★★ 5 (+3) |
|---|---|---|---|---|---|
| **AIR** (olhos e ar) | Spy Drone · 1 | Target Designation · 1 | A-10 Strike I · 2 | A-10 Strike II · +1 | AC-130 Gunship · 3 |
| **GROUND** (tropas) | Paradrop I · 1 | Paradrop II · +1 | Paradrop III · +1 | SEAL Insertion · 2 | — |
| **FIRE** (golpe) | — | Naval Gunfire I · 1 | Naval Gunfire II · +1 | Naval Gunfire III · +1 | MOAB · 3 |
| **SUPPORT** (manter e negar) | Emergency Repair I · 1 | Emergency Repair II · +1 | AWACS Early Warning · 1 | Guardian Angel · 2 | Forward Operating Base · 3 |

- **17 poderes e níveis, 28 pontos.** O jogador leva 9, um terço da árvore.
- **Cada patente tem 3 ou 4 escolhas de tipos diferentes:**
  - na ★1: ver, chamar tropa ou consertar;
  - na ★2: marcar o alvo, o golpe leve ou subir um nível;
  - na ★3: o A-10, o aviso ou subir um nível;
  - na ★4: o golpe de prédio, os SEALs ou o escudo;
  - na ★5: o golpe de habilidade (AC-130), a bomba grande (MOAB) ou tomar o mapa (o posto avançado).
- **Os níveis de um poder ficam em patentes seguidas.** Na primeira montagem a linha que liga os níveis passava por trás de outro
  poder (o Paradrop atrás do SEAL, o Cruise atrás do Precision) e não se lia. Por isso o Cruise II vem na ★3 (no jogo hoje é na ★4).
- **As casas vazias** (FIRE ★1 e GROUND ★5) são de propósito: nenhum golpe na patente 1, e a ★5 já tem três poderes de 3 pontos.
- **A foto da tela:** `docs/poderes/tela/poderes_usa.jpg` (na patente 3, com o Spy Drone e o Paradrop I e II comprados: sobra 1 ponto
  e o A-10 pede 2).

---

## 3. Cada poder

Os golpes chegam por avião, que entra pela borda do mapa do lado do dono e pode ser derrubado (derrubar dá XP, decidido em 03/11). A
marca no chão aparece pros dois lados uns segundos antes. As tropas chamadas custam metade do preço das unidades em dinheiro (o custo
de usar misto, decidido em 03/11).

| Poder | Pts | Recarga | O que faz | Como chega e a resposta | A marca no campo | De onde veio |
|---|---|---|---|---|---|---|
| **Spy Drone** | 1 | 1:30 | um drone camuflado circula 40 s no ponto, vê 66 m e revela o camuflado | o drone tem 200 de vida; a antiaérea que detecta derruba | — | ZH |
| **Target Designation** | 1 | 1:30 | o satélite pinta um círculo de 30 m por 20 s: revela o camuflado, e o inimigo dentro leva +25% de dano | os pontos de laser no chão: o inimigo vê e pode sair | — | Contra (Townes), novo pra base |
| **A-10 Strike** I / II | 2 / +1 | 3:00 | 1 / 2 A-10 numa passada: 6 Mavericks de 200 cada (cheio a 3 m, metade a 6 m) e o canhão GAU-8 numa faixa de 80 m | o A-10 tem 600 de vida; a marca 4 s antes; o som do canhão chega antes do avião | a fileira de crateras do canhão | ZH recalibrado |
| **AC-130 Gunship** | 3 | 6:00 | gira 12 s sobre um círculo de 26 m, e **você clica nos alvos**: o obus de 105 mm (80, raio 6,6 m) e o canhão de 25 mm | 600 de vida, girando devagar: a antiaérea tem tempo | — | ZH (Spectre) |
| **Paradrop** I / II / III | 1 / +1 / +1 | 3:00 | 3 Rangers e 2 Javelins / 6 e 4 / 12, 8 e um Humvee; custa $650 / $1 300 / $2 900 por uso | o cargueiro tem 1 000 de vida e cai com a tropa dentro | — | ZH, com a mistura do patch da comunidade |
| **SEAL Insertion** | 2 | 4:00 | o Osprey põe 4 SEALs camuflados (parados ou andando, aparecem ao atirar); eles **plantam carga num prédio**, que cai a partir do ponto da carga 10 s depois; custa $600 por uso | o Osprey tem 800 de vida e pousa; a carga plantada aparece pro dono do prédio | o prédio caído do ponto da carga | Contra (SEAL Drop), ShockWave (HALO) |
| **Cruise Missile Strike** I / II | 1 / +1 | 3:00 | 6 / 9 Tomahawks num girassol de 12 / 14 m (o que já está no jogo, aprovado em 23/10) | os mísseis vêm baixos do lado do dono | as crateras | nosso |
| **Precision Strike** | 2 | 4:00 | uma bomba guiada de 2 000 lb num prédio: mata um prédio de 2 000 de vida e arranha o vizinho | o ponto de laser no prédio 6 s antes; o B-2 não aparece | **o prédio cai a partir do ponto do impacto** | novo (o campo que muda) |
| **MOAB** | 3 | 7:00 | a bomba grande: 2 400 até 14 m, 700 até 30 m; o maior golpe da USA | sai da rampa de um MC-130 (como a de verdade), de paraquedas: o avião tem 1 000 de vida e a marca fica 12 s no chão | a cratera rasa, as palmeiras deitadas pra fora | ZH recalibrado |
| **Emergency Repair** I / II | 1 / +1 | 2:00 | os veículos numa área de 26 m voltam 30% da vida na hora e levam 25% menos dano por 15 s; no II, 50% e também os prédios | o Chinook solta as caixas de conserto | — | ZH corrigido (no ZH ninguém pegava) |
| **AWACS Early Warning** | 1 | passivo | todo poder do inimigo aparece pra você e pros aliados com o tipo, a rota e o alvo assim que ele chama | é passivo, não tem resposta | — | Act of War |
| **Guardian Angel** | 2 | 3:00 | por 20 s num círculo de 40 m, os lasers e os interceptores derrubam os mísseis, os foguetes, as granadas e as bombas que chegam (**menos as superarmas**) | o escudo aparece pros dois lados: o inimigo espera ele acabar | — | Rise of the Reds, o laser DE M-SHORAD real |
| **Forward Operating Base** | 3 | 6:00 | o C-17 pousa e monta em 20 s um posto avançado de 1 500 de vida: os reforços das fábricas podem sair dali, ele vê 60 m e conserta os veículos em volta; um por vez | o C-17 tem 1 000 de vida; montando, é alvo | o posto no mapa | Contra (Deploy War Factory), EndWar |

---

## 4. A conta (o que um uso faz)

Na mesma base de referência das superarmas (as fábricas, as usinas, 8 tanques e 10 soldados) e num exército de 18 unidades no campo.
Rodar: `python -I docs/poderes/poderes_conta.py`.

| Golpe | Na base, bem mirado | No exército do campo | Leitura |
|---|---|---|---|
| Cruise I (1 pt) | estrago de $900 a $1 600 (uma usina) | $2 000 | o golpe leve: certo pra 1 ponto |
| A-10 I, o do ZH | $2 400 | **mata 13 de 18** | o motivo de ser obrigatório no ZH |
| A-10 I, a proposta (2 pts) | $1 600 | $2 800, sem matar | um golpe médio |
| A-10 II, a proposta (+1) | mata a War Factory | $6 100 | os dois aviões juntos pesam |
| Precision Strike (2 pts) | mata a War Factory ($2 000) ou a tecnologia ($2 500) | — | o golpe de prédio |
| AC-130, 12 s (3 pts) | $6 400 nos tanques (mata 5) | $7 000 (mata 6) | depende de quem clica; o do ZH (15 s, a rotativa de 90) matava 17 |
| MOAB do ZH | **mata $8 900**, como uma superarma | apaga tudo | forte demais pra um poder |
| MOAB, a proposta (3 pts) | de $4 400 a $5 800 | apaga o exército se ele ficar junto | metade de uma superarma; os 12 s da marca dão tempo de espalhar |
| *A superarma da USA, pra comparar* | *de $5 600 a $9 200* | | |

---

## 5. O que ficou de fora (e por quê)

| Ideia | Por que não agora | Onde pode entrar |
|---|---|---|
| **Dark Eagle** (o hipersônico em 3 alvos) | repete a superarma da USA (o míssil que se parte em 5) | — |
| **Switchblade** (drones-kamikaze guiados) | os drones são da China (a superarma) | — |
| **HIMARS Raid** (o lançador entra no mapa, atira e foge) | ótima resposta (dá pra caçar), mas faz o mesmo papel do Cruise Missile, que já está no jogo | se o Cruise sair, ou num general |
| **Strategic Bomber** (o nível troca o avião: B-52, B-1B, B-21) | é o Carpet Bomb do Air Force no ZH e no Contra | o general Air Force, no lugar da MOAB |
| **Reaper Overwatch** (o MQ-9 atira em quem entra na área por 60 s) | bom, mas a linha do ar já está cheia | o general Air Force |
| **MALD Decoys** e **Growler Jamming** (iscas e interferência) | poderes de combo, que pedem jogador experiente | o general Air Force |
| **Leaflet Drop** (desliga a tropa 20 s) | pouco falado no ZH e forte demais cedo; a Rússia já tem a interferência | — |
| **Fighter Sweep** (Raptors caçam os aviões dos poderes) | o Guardian Angel e a antiaérea fazem o papel | o general Air Force |

---

## 6. As vagas pros generais (depois)

- **Air Force:** o A-10 ganha o nível III; a MOAB vira o Strategic Bomber; entram o Reaper, as iscas, o Growler e o Fighter Sweep.
- **Laser:** o Precision Strike vira o laser do satélite; o Guardian Angel vira só laser (mais forte, mais curto); a artilharia de laser
  (o Plasma Strike do Townes no Contra, 10 / 20 / 30 tiros de fora do mapa) no lugar do Cruise.
- **Superweapons:** a superarma vira o canhão eletromagnético (a visão dele); nos poderes, a definir quando chegar a vez.

---

## 7. O que é seu decidir

1. **A árvore (§2):** o que fica, o que sai, o que falta.
2. **A ★5:** a MOAB na USA base e o Strategic Bomber no Air Force (recomendo), ou o contrário.
3. **O Forward Operating Base:** fica (o poder mais diferente da árvore, e o mais trabalhoso de fazer) ou vira o Reaper.

---

## 8. Como refazer (a receita)

- **A conta:** `python -I docs/poderes/poderes_conta.py`. Reaproveita a base de referência e o dano em área do estudo das superarmas
  (`docs/superarmas/superarmas_conta.py`). Cada golpe tem o perfil do ZH (de `docs/poderes/ESTUDO_ZH.md`) e o da proposta, lado a lado.
- **A régua que saiu dela:**
  - 1 ponto ≈ um tanque por uso;
  - 2 pontos ≈ um prédio;
  - 3 pontos ≈ metade de uma superarma, que mata de $5 600 a $9 200 na mesma base.
- **A tela:** a árvore está em `docs/poderes/tela/usa.js`, no formato da tela de poderes do protótipo aprovado do HUD:
  - cada casa (linha × patente) aceita um poder;
  - os níveis do mesmo poder ficam em patentes seguidas.
  - `python docs/poderes/tela/make_tela.py usa --shot 3 drone,para1,para2 a10_1` monta a página a partir do
    `docs/concept/hud/prototipo/index.html` (sem tocar nele) e tira a foto. O "3" é a patente, depois vêm os comprados e o poder no
    detalhe.
- **Os ícones provisórios** (A-10, AC-130, Precision, MOAB, AWACS): `python docs/poderes/tela/make_icons.py
  docs/concept/hud/prototipo/a docs/poderes/tela/pw`. É o fundo de metal do kit com um pictograma claro. A arte final vem depois.
- **As pesquisas:**
  - `ESTUDO_USA.md`: o Contra (os 4 generais americanos), o ShockWave, o Rise of the Reds, os outros RTS e a USA real;
  - `ESTUDO_ZH.md` e `ESTUDO_RTS.md`: o estudo geral.
- **O que ele disse no caminho:**
  - "não fique tão enviesado com o C&C ZH, o Contra tem muita criatividade";
  - "lembre-se que já existe uma UI top pra escolher os poderes";
  - a pergunta sobre trocar o Cruise Missile (a resposta: o HIMARS Raid, §5).

---

## 9. A 1ª revisão dele (03/11) e a minha resposta

**O que ele disse:**
- Gostou da diferenciação do AWACS, do Target Designation e do Spy Drone ("muito criativo, parabéns") e do Forward Operating Base
  ("top, interessante"): "eu gostei muito das ideias da USA".
- **O Humvee do Paradrop III não existe no jogo.** Foi erro meu: copiei o exemplo do protótipo sem conferir o elenco.
- "Você não acha que tá tudo muito copiado do C&C ZH?"
- A artilharia era exemplo, não regra; e "você tá muito enviesado com o que eu falo [...] toma decisões sozinho por meio de discussões,
  estudos e inspiração".
- Perguntou o que é o SEAL.
- Trouxe a ideia da fábrica que produz 2 ao mesmo tempo, ou mais barato e mais rápido.

**A conta honesta da cópia:** dos 13 poderes, 6 vinham do ZH (Spy Drone, Paradrop, A-10, AC-130, MOAB, Emergency Repair). A linha
dos golpes é a mais ZH.

**A minha decisão (proposta, falta ele ver):**
- **Ficam do ZH só três ícones de nostalgia: o A-10, o AC-130 e a MOAB** (a visão do jogo, 24/10: "o peso maior é a nostalgia").
  O Spy Drone fica pelo papel novo dele, o de abrir a cadeia "ver, marcar, acertar".
- **Emergency Repair vira Combat Recovery I–II.** O Chinook vem buscar um veículo seu avariado no meio da luta, tira ele do combate e
  devolve consertado no ponto de encontro (no II, dois veículos).
  - É a cara da USA: pouco, caro e preciso, e salvar o tanque caro vale mais que remendar.
  - A resposta do inimigo: derrubar o Chinook com o tanque pendurado.
- **Cruise Missile vira HIMARS Raid I–II, pelo mérito.** O lançador fica exposto no mapa uns 20 s e dá pra caçar; o Cruise vinha de
  fora do mapa sem resposta e era fraco. O Tomahawk continua no jogo pelo caminhão lançador. A razão "cada facção tem a sua
  artilharia" saiu: era o exemplo dele virado regra.
- **Paradrop III sem veículo, com o salto HALO.** São 10 Rangers e 6 Javelins, e o avião voa alto: só a antiaérea de longo alcance
  (Patriot) alcança. É o nível que muda o como.

**A fábrica que produz 2 ao mesmo tempo:** já está anotada e desenhada desde 11/10 (`docs/IDEIAS_JOGABILIDADE.md` §3, a linha de
montagem dupla). Ela é uma melhoria da própria fábrica, paga em dinheiro e aberta pela patente (a tecnologia que abre pela patente,
aprovada em 03/11). **Não entra nos poderes:** um multiplicador de produção comprado com ponto viraria a escolha obrigatória (como
o Cash Bounty no ZH) e uma bola de neve. A produção acelerada por um tempo cabe melhor como poder da China, a facção da massa.

---

## 10. A 2ª revisão dele (03/11)

**O que ele disse:**
- **Reprovou o Combat Recovery:** "vai fazer as pessoas detestarem mais ainda usar o Emergency Repair; foi um pensamento rápido sem
  base?"
  - Foi. Pensei só na frase da USA (pouco e caro, salvar o tanque) e não conferi contra o estudo.
  - O Emergency Repair do ZH morreu porque não serve na hora da luta (a fala dos competitivos, `ESTUDO_ZH.md` §7). O Combat Recovery
    tira o tanque da luta justo nessa hora, ou seja, piora o mesmo defeito.
- **Perguntou do HIMARS:** onde o lançador fica exposto, de onde sai e se anda.
- **O SEAL ficou bom.**
- **A ideia dele pro lugar do Cruise Missile:** a artilharia de um navio grande, como no ZH, com um som tenebroso que todos escutam.
  Ou na patente 5, ou em 3 níveis nas patentes 3 e 4, porque a MOAB já está na 5.
- **A fábrica:** "lá deveria ser melhoria das unidades". O Contra traz construções de melhoria novas, além do centro de tecnologia,
  cada facção com a sua especial. Ele quer expandir o jogo pra algo assim, "não exatamente, pense você mesmo".

**A minha decisão (proposta, falta ele ver):**
- **O Emergency Repair volta**, o corrigido de §3: o conserto na hora (30%) e o casco reforçado por 15 s, com recarga de 2:00 (era
  4:00 no ZH). Assim ele vira um reforço pra ganhar a troca de tiros, não um remendo.
- **Naval Gunfire I–III no lugar do Cruise Missile**, nas patentes 2, 3 e 4. O nível muda o navio:
  - I: um destróier, uma fileira de granadas de 127 mm que anda;
  - II: três destróieres;
  - III: **o encouraçado, a salva de 9 granadas de 406 mm**. O estrondo da salva chega pra todos os jogadores, depois o assobio de
    "trem de carga" das granadas e as marcas no chão. É a escapada pro real (o encouraçado voltando, como no ZH), em nome da nostalgia.
  - A resposta é o aviso: o som chega antes e a fileira anda, então dá pra sair.
  - A Rússia fica com a barragem de tubo que satura e deixa cratera; a USA, com poucas granadas enormes de uma vez.
- **O HIMARS sai.** A exposição dele era fraca: o avião pousaria o lançador do lado de quem chamou, longe do inimigo. E ele pedia
  pouso, caminho e resgate, muita regra pra pouco jogo.
- **O Precision Strike sai** da base: a linha do golpe fica com a artilharia naval nas patentes 2 a 4 e a MOAB na 5, e o SEAL já
  derruba prédio. A bomba guiada a laser fica guardada pro general Laser.
- **A fábrica:** a proposta de um prédio novo por facção, o da indústria, está em `docs/IDEIAS_JOGABILIDADE.md` §3 (a atualização de
  03/11). É fora dos poderes e pede um estudo próprio.

---

## 11. Aplicado e aprovado (03/11)

Ele: "tá ok, ótimo, perfeito, e pode aplicar". A tela está em `docs/poderes/tela/poderes_usa.jpg`, na patente 4 com o Spy Drone, o
Target Designation, o Paradrop I e a Naval I e II comprados.

- **Naval Gunfire I–III** (patentes 2, 3 e 4; 1 ponto mais 1 e 1; recarga de 3:00, 3:00 e 5:00):
  - I: um destróier, 12 granadas de 127 mm (130 de dano, cheio a 2,5 m, metade a 5 m) numa fileira de 40 m que anda;
  - II: três destróieres, 3 fileiras lado a lado;
  - III: o encouraçado, a salva de 9 granadas de 406 mm (800 até 4 m, 250 até 10 m) numa elipse de ~30 × 18 m.
  - Na conta (`poderes_conta.py`): o I faz de $1 000 a $2 200 de estrago; o II derruba a War Factory ou as 3 usinas (~$2 800); o III
    derruba de $3 300 a $5 200 na base. No exército parado e junto, o III apaga tudo, como a MOAB; a resposta é o aviso (o estrondo
    pra todos e as marcas no chão).
- **Paradrop III:** o salto HALO, 10 Rangers e 6 Javelins por $2 000. O avião voa alto: só a antiaérea de longo alcance alcança.
- **Emergency Repair I–II:** 30% de vida na hora e 25% menos dano por 15 s; recarga de 2:00.
- **Saíram:** o Cruise Missile (o míssil continua no caminhão Tomahawk), o Precision Strike (guardado pro general Laser), o Combat
  Recovery (reprovado) e o HIMARS.
- **18 poderes e níveis, 28 pontos;** o jogador leva 9.
- **O próximo passo** (quando ele der o sinal): um ícone de verdade pra cada poder, no estilo dos ícones do HUD (o kit), antes de ir pra
  próxima facção. Os de hoje (`docs/poderes/tela/pw/`) são provisórios, feitos por script.

---

## 12. Os ícones dos poderes (03/11, em teste)

- **Pedido dele:** gerar os ícones de cada poder, gastando o mínimo, só pra ver (sem pôr no jogo).
- **1ª tentativa, reprovada antes de ver:** a folha dos 9 ícones novos no estilo de pictograma do kit do HUD (`captures/concept/162_`).
  Ele: "eles são ícones muito básicos, eu quero propostas e ideias diferentes, tanto o design do ícone quanto o design proposto como
  padrão".
- **2ª, os estilos:** uma imagem só, com 3 poderes (A-10, AWACS, Naval) em 4 estilos. Ele pediu o Gemini também:
  `docs/concept/poderes/icones_4_estilos_gpt.jpg` e `..._gemini.jpg` (`captures/concept/163_`; o prompt está nas receitas). Os estilos:
  1. **o retrato pintado** (o botão do ZH);
  2. **a visão do sensor**: a câmera térmica, o radar, a visão noturna, com as marcas de mira;
  3. **o emblema bordado**, o patch militar;
  4. **a silhueta heroica** contra a luz.
- **A minha leitura:**
  - o GPT acertou mais as formas (o A-10 do Gemini saiu errado na silhueta);
  - a 64 px, a silhueta e o emblema leem melhor;
  - o pintado é o mais ZH, mas embola quando fica pequeno;
  - **a visão do sensor é a que conta quem a USA é** ("vê tudo") e casa com os poderes de olho (o drone, o satélite, o AWACS).
- **A ideia pro padrão:** cada facção com o estilo de ícone que conta quem ela é, como os HUDs de cada facção (aprovados) já são
  diferentes:
  - USA: a visão do sensor;
  - China: o cartaz de propaganda;
  - GLA: o estêncil pintado com spray na chapa;
  - Rússia: o distintivo de esmalte.

---

## 13. Os estilos na tela de verdade e o padrão por tipo (03/11)

- **Pedido dele:** pôr os estilos "na UI, bem feito, como se fosse realmente no jogo", pra comparar; e seguir um padrão de desenho por
  tipo de poder: os que atacam, os que dão suporte, os que desbloqueiam unidade ou construção, e os outros.
- **Como foi feito** (a receita):
  - `cut_styles.py` recorta as 12 casas de cada folha; o emblema sai com o fundo transparente;
  - `make_tela.py usa --style <modelo>_<estilo> --column a10_1,naval1,drone,para1,repair1 --shot 5 drone,a10_1,naval1,naval2 naval3`
    monta a tela com os ícones dos 3 poderes da folha (o A-10, o AWACS e o Naval) e a coluna de poderes do HUD com os ícones por cima
    de cada casa, e tira a foto.
  - Os outros poderes seguem com os ícones provisórios.
  - As fotos estão em `docs/concept/poderes/tela_estilo_<estilo>_gpt.jpg` e `tela_estilo_<estilo>_gemini.jpg` (uma tela por imagem: ele pediu, "tudo pequeno não consigo ver nada").
- **O padrão por tipo** (proposta). A forma vem da moldura, e a cor fica só no selo, pra não brigar com o verde de "comprado" e o
  dourado de "dá pra comprar". Também lê pra quem não distingue cor:

  | Tipo | A moldura | O selo |
  |---|---|---|
  | **Golpe** (A-10, AC-130, Naval, MOAB) | os cantos chanfrados | vermelho com a mira |
  | **Olhos** (Spy Drone, Target Designation, AWACS) | as 4 cantoneiras de mira em ciano | ciano com o olho |
  | **Tropas** (Paradrop, SEAL) | a ponta de escudo embaixo | oliva com as divisas |
  | **Apoio** (Emergency Repair, Guardian Angel) | redonda | azul com o escudo |
  | **Construção e desbloqueio** (Forward Operating Base; as unidades e os prédios que um poder destravar) | hexágono | bronze com a bandeira |

  - O poder passivo (o AWACS) ainda não tem marca própria.
  - No estilo do emblema, a moldura do tipo corta o próprio formato do bordado. Se o emblema for o escolhido, o tipo passa a ser o
    formato do emblema (o escudo, o círculo, o hexágono).

---

## 14. Os ícones reprovados (03/11) e o caminho certo

- **Reprovados:** os 4 estilos e o padrão por tipo da §13. "Mal feito, mal pensado, foi rápido demais, não teve embasamento em nada."
  - O erro foi o mesmo das superarmas no começo: fui pra imagem antes do estudo e da ideia combinada em palavras.
  - Também: uma imagem com várias telas pequenas não serve. A regra é uma tela por imagem.
- **O que continua:** cada tipo de poder com o seu padrão de desenho. Os tipos que ele citou: o desbloqueio de unidade (como o Paladin
  do ZH), o ataque, o suporte, o drop e o bônus. "É algo que dá pra testar", com decisões bem pensadas e discutidas.
- **O caminho:**
  1. o estudo de como os jogos AAA mostram o tipo no ícone (`docs/poderes/ESTUDO_ICONES.md`);
  2. a proposta em palavras, com o porquê de cada decisão;
  3. só depois o teste: uma imagem por tela.
- **Os critérios dele pro ícone (03/11):**
  - cada um **traduz bem o que faz**: "nem todos conseguem numa imagem, e tá ok, mas tem que ser algo que faz sentido";
  - **fácil de ler visualmente.**
  - Somam-se à regra do HUD: sem legenda (a forma, a cor e a reação ao mouse explicam).

---

## 15. O lugar do Forward Operating Base (03/11, proposta)

**Ele:** "o Forward Base não representa o que eu tinha entendido, e na verdade esse superpoder nem sei se eu gostei realmente dele,
acho que dá pra gente pensar num outro da USA que faz sentido".

**O que a casa pede** (SUPPORT, patente 5, 3 pontos):
- Na ★5 já há dois golpes, o AC-130 (de habilidade) e a MOAB (a grande).
- A terceira opção tem de ser **estratégica e sem dano**: quem não quer mais um golpe precisa de uma escolha forte.
- E tem de ser a cara da USA, que leva a guerra pra qualquer lugar.

**A proposta: Strategic Airlift.**
- **Como funciona:**
  - você escolhe até 4 veículos seus (ou 8 soldados) em qualquer lugar;
  - um C-17 pousa ao lado deles e embarca em 5 s;
  - cruza o mapa à vista de todos (~15 s);
  - pousa onde você mandou, em chão aberto que você já viu.
- **Por que é bom:**
  - abre as jogadas grandes: o ataque pelo flanco, o reforço de uma base longe, o prédio neutro tomado de surpresa;
  - é a força de verdade dos EUA (o transporte aéreo estratégico);
  - faz o que o posto avançado queria (levar a frente pra perto do inimigo), mas com as suas unidades e sem prédio novo.
- **A resposta do inimigo:**
  - derrubar o C-17 no ar, e a carga cai junto: 1 500 de vida, risco e prêmio;
  - o pouso aparece pra todos uns segundos antes.
- **O ícone:** o C-17 de rampa aberta, com o tanque descendo. Família comando, subtipo tropas (a chegada).
- **A 2ª opção, se ele preferir:** Global Recon. O satélite revela o mapa inteiro por 15 s, os camuflados também: o máximo do "vê
  tudo". É mais fraca de jogar (não muda nada sozinha), por isso fica atrás.

**O AWACS (a pergunta dele):**
- O E-3 Sentry é o avião de radar: um Boeing com o disco giratório em cima, que voa alto e vê aviões e mísseis a centenas de
  quilômetros.
- No jogo é passivo: depois de comprado, toda vez que o inimigo chama um poder (o A-10, a bomba, o paraquedas), você e os aliados veem na
  hora o que é, por onde o avião vem e onde vai cair. É o tempo pra tirar as unidades ou ligar o Guardian Angel.
- Não tem botão nem recarga.

---

## 16. As perguntas dele (03/11) e as regras que saem delas

**O AWACS vê tudo, até a superarma? E os poderes comprados?** A proposta:
- **Os poderes, na hora em que são chamados:** o tipo, a rota e o ponto do impacto, pra você e pros aliados (já era assim).
- **A superarma:** quando a superarma inimiga é lançada, quem tem o AWACS vê **a área do impacto na hora do lançamento**. Os outros só
  veem nos segundos finais (a regra das superarmas).
- **O arsenal do inimigo:** cada poder que ele usa fica anotado no card do general dele, na tela do general, enquanto o AWACS existir.
  Você vai conhecendo o arsenal dele pelo uso. A lista do que ele comprou e ainda não usou continua sendo só da espionagem (a regra do HUD).
- **O ícone:** a imagem do avião não dizia isso. Vira a placa com o AWACS visto de cima e o disco de radar dele como uma tela, com as
  setas vermelhas dos ataques chegando e o ponto de onde cada um cai (`captures/concept/167_`).

**O que era o Forward Operating Base:** o C-17 pousava num ponto que você escolhesse e montava em 20 s um posto avançado de 1 500 de
vida. As unidades novas das fábricas podiam sair dali em vez de atravessar o mapa; ele via 60 m e consertava os veículos em volta. Só
um por vez.

**O Strategic Airlift, em detalhe:**
- **Quem:** qualquer unidade de terra sua, menos os superpesados. Vêm **dois C-17**, cada um leva 2 veículos ou 6 soldados (até 4
  veículos ou 12 soldados, misturando).
- **Como:**
  1. aperta o poder;
  2. escolhe o grupo (ou usa a seleção);
  3. clica no destino, um chão aberto que você já viu.
  - Os C-17 pousam num trecho aberto perto do grupo (o pouso curto é licença do jogo), embarcam em 5 s, cruzam em linha reta e pousam no
    destino; as unidades descem prontas.
- **A rapidez:**
  - o C-17 a ~60 m/s cruza 800 m em ~13 s; com o embarque e o pouso, ~26 s no total;
  - um tanque leva ~100 s pra andar os mesmos 800 m: o poder é 4 vezes mais rápido.
- **A resposta do inimigo:**
  - a rota aparece pra todos;
  - os C-17 (1 500 de vida cada) podem ser derrubados com tudo dentro.
- **O custo:** 3 pontos, recarga de 6:00. Não cobra por uso: não cria unidade, só leva as suas.
- **03/11, a decisão dele:** fica o Forward Operating Base, não o Strategic Airlift ("entendi, mas acho que pode deixar o outro então,
  o posto avançado, mas precisamos de uma imagem mais bonita"). O ícone novo tem de dizer o principal do poder, que os reforços saem
  dali lá na frente. Duas ideias em teste: o portão com o tanque saindo, e o forte visto de cima com a sombra do C-17
  (`captures/concept/168_`, GPT e Gemini).

---

## 17. A tela sem as linhas por tipo (03/11, decidido)

- **A pergunta dele:** "e se tiver 2 AIR, ou SUPPORT, ou GROUND [...] no Major General, como encaixaria?" A grade tinha uma casa por
  tipo e patente; dois poderes do mesmo tipo na mesma patente não cabiam.
- **A 1ª resposta:** as faixas por tipo, onde uma faixa ganha uma 2ª linha quando precisa. Funcionou, mas ele cortou antes: "daria pra
  tirar essa distinção das linhas, e foda-se, C&C não tem isso mesmo".
- **Por que essa é a certa:**
  - o tipo já se lê no ícone (`ICONES.md`), então o rótulo da linha repetia a informação;
  - sem a coluna dos rótulos, os cartões ganham largura;
  - uma patente pode ter quantos poderes precisar;
  - o ZH também não tem linha por tipo.
- **As regras que ficam:**
  - as colunas são as patentes;
  - os níveis de um poder ficam na mesma linha, ligados pela linha;
  - as linhas seguem a ordem dos tipos (olhos, golpe, tropas, apoio), sem rótulo, só pra manter juntos os parecidos.
- **A árvore** (`docs/poderes/tela/usa.js`), em 5 linhas:
  1. olhos: Spy Drone, Target Designation, AWACS;
  2. golpe do ar: A-10 I e II, AC-130;
  3. golpe do mar: Naval I a III, MOAB;
  4. tropas: Paradrop I a III, SEAL, posto avançado;
  5. apoio: Emergency Repair I e II, Guardian Angel.
- **A tela:** `docs/poderes/tela/poderes_usa.jpg` e `docs/concept/poderes/tela_sem_rotulos.jpg`.
  - `make_tela.py`: quando todos os rótulos de `ROWS` são vazios, some a coluna dos rótulos e a grade fica com as 5 patentes.

