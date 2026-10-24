# A v4: o arsenal do USA (a renda que amadurece, a patente, os mísseis e o Tomahawk)

As decisões de design da noite de 22–23/10/2026. É o registro permanente: o que entrou, os números,
por que esses números e o que ficou pra depois. Pra ler de manhã e discutir.

**Estado (23/10, manhã):** feito no código e testado na simulação (o simtest e 20 partidas robô × robô). Falta o
teu teste. **Nada foi commitado.** Os modelos novos (a zona, o avião, a caixa com paraquedas e o Tomahawk) são
provisórios, feitos por script.

**Fontes:** o spec do debate, os mapas de pesquisa (ZH 1.04, AAA, a economia medida) e o harness IA × IA, fora
do git. Os números finais estão no `Game/Config/RTS/Units.json` (`usa_drop_zone`, `usa_tomahawk`,
`usa_cruise_missile`, `rank`, `powers`), com o motivo de cada mudança num campo `_`.

## 1. Em um minuto

- **O problema medido.** A árvore de tecnologia acabava em 1:40, e o minuto 20 era igual ao minuto 2: só
  Crusader e a mesma renda. A partida robô × robô acabava em 5,4–6,5 min.
- **Supply Drop Zone** ($2.500, até 2 por jogador). Um C-130 pousa paletes no pátio a cada 70 s. A rota
  amadurece (3, depois 4, depois 5 paletes). Se a zona morre, a carga cai no chão e qualquer caminhão pega.
- **A patente do general.** O XP vem só da luta. A patente libera sozinha: R2 o ataque, R3 o Tomahawk,
  R4 o ataque maior.
- **Cruise Missile Strike** (R2). Seis mísseis caem num círculo de 12 m, com o rastro da Patriot. Quem é
  atacado vê o anel vermelho e tem uns 3 s pra sair.
- **Tomahawk Launcher** (R3). É artilharia de mísseis que atira de 85 m, e a Patriot só alcança 63 m.
  Quebra a parede de defesa.
- **A IA usa tudo:** compra a zona, mira o ataque onde há mais valor, faz Tomahawk e cerca as Patriots.
- **Medido.** A partida robô × robô agora dura 15,9 min na mediana: 19 de 20 passam de 12 min, sem empate.

## 2. O debate

Três propostas disputaram a v4:
- **economia:** o poço de petróleo no meio do mapa;
- **evolução:** a veterania das unidades e a patente;
- **viável:** o que cabe numa noite com qualidade.

**Onde as três concordaram:**
1. A renda nova é a Supply Drop Zone. Ela traz as próprias caixas e **nunca reabastece depósito**. Assim
   respeita a tua reprovação de 14/10 ("o depósito tem quantidade limitada").
2. A patente vem só da luta, e fundação vale 0.
3. Na patente 2 entra um poder de ataque. Ele precisa de dano em área, que não existia (todo tiro acertava
   um alvo só).
4. Sem consertar a IA, nada novo aparece na partida: cada fábrica dela só fazia o primeiro armado da lista.

**Onde brigaram, e quem ganhou:**

| Briga | Ganhou | Por quê |
|---|---|---|
| Poço de petróleo × Drop Zone | A Drop Zone, hoje | O poço é o melhor desenho, porque puxa a briga pro meio. Mas custa ~6 h e mexe no mapa, na regra de construção, no minimapa e no WorldForge. **É a 1ª coisa da próxima vez.** |
| "Renda segura demais" (o Black Market escondido na base) | Segurada de 4 jeitos | Limite de 2; a rota que amadurece e zera quando a zona morre; a carga do pátio que cai no chão; o ataque de mísseis que acha a zona sozinho. |
| Veterania das unidades | Fica pra depois | Mexe em dano, recarga e vida de todo o combate e alimenta a bola de neve. A patente basta hoje. |
| Escolher no Promote × liberar sozinho | Liberar sozinho | Com um poder por patente, a escolha seria de mentira. O Promote volta quando houver 2 opções por patente. |
| Granadas, foguetes com fumaça vermelha ou mísseis de cruzeiro | Mísseis | Usam o míssil que já existe, com a fumaça da Patriot aprovada. A fumaça vermelha seria fumaça nova sem prova, então o aviso virou um anel vermelho no chão. |
| Tomahawk na R2, na R3 ou depois | Na R3 | É a escada que você aprovou: 2 = 1º poder, 3 = unidades médias de topo. |
| Fogo amigo | Desligado hoje | A batalha lê fácil e a IA fica simples. |

**O que o código mostrou antes de começar:** as faixas cinza da fumaça do míssil já estavam corrigidas; a IA já
tinha, da sessão do dinheiro inicial (22/10), o freio das fundações sob fogo e a 2ª fábrica (não foi refeito); a
versão da simulação foi de 18 pra 19.

## 3. As peças novas

### 3.1 Supply Drop Zone (`usa_drop_zone`)

**O que você vê:**
- Um pátio de concreto com o círculo de pouso e uma torre com radar.
- Antes de cada pouso vem o aviso "Airlift inbound", e um C-130 cruza baixo.
- Os paletes descem de paraquedas nas vagas. A empilhadeira leva um a cada 3 s: "+$300" em cima do pátio.
- No cartão da zona: "Next airlift 0:42" (ou "Low power: paused") e os paletes. A 3ª zona mostra
  "Limit reached (2)".

| | |
|---|---|
| Custo e obra | $2.500, 30 s |
| Vida | 1.200, armadura de prédio |
| Lote | 16 × 16 m |
| Energia | −3. Sem energia, a contagem e a empilhadeira param (o avião já avisado pousa) |
| Precisa de | War Factory. Não pede patente: a renda nunca depende de ganhar briga |
| Limite | 2 por jogador (fundação conta) |
| Pousos | O 1º vem 45 s depois de pronta, depois um a cada 70 s. O aviso vem 10 s antes |
| Carga | 3, 4, depois 5 paletes de 4 caixas de $75: $900, $1.200, $1.500... |
| Madura | 5 × $300 a cada 70 s = **$1.286/min**. Com duas, $2.571/min |

**Por que esses números:**
- **Quando se paga.** O custo real é $2.980 (os $2.500 mais 3 de energia × $160). Contando de quando a obra é
  posta, os pousos caem a 1:15, 2:25 e 3:35: a zona se paga em ~3,7 min, 2,6× o Supply Center (1,43 min).
  A regra da pesquisa: renda infinita paga 2–3× mais devagar que a finita (ZH Drop Zone 3,3–4,2 min, Black
  Market 4,2, SupCom 4,2). Por isso a zona nunca é a abertura: o caminhão se paga em menos de 1 min.
- **Quanto rende.** Posta aos 3:30, ela recebe 14 pousos até 20:00: $20.100. Tirando o custo, +$17.100,
  uns 19 Crusaders.
- **Por que o limite de 2.** A renda segura chega a ~$2.570/min, uns 70% de um depósito saturado (~3.600):
  os caminhões continuam sendo a renda maior e mais arriscada.
- **O risco de quem tem a zona.** Perder a zona custa a obra nova, a rampa (volta a 3 paletes) e até $1.500
  caídos no chão pro caminhão do inimigo ("a perda de um é o ganho do outro", ECONOMIA §2). 6 Crusaders matam
  a zona em ~7 s; o ataque de mísseis sozinho tira ~74% da vida dela.
- **As aprovações.** A zona nunca toca depósito do mapa (APROVADOS:102). O C-130 soltando caixas estava na
  lista de momentos de arrepiar da ECONOMIA.

**Como se joga contra:**
- atacar logo depois de um pouso: a carga do pátio cai pros teus caminhões;
- o ataque de mísseis;
- matar a usina, o que congela a contagem; matar a zona, o que zera a rota;
- depois: derrubar o C-130 com o antiaéreo da Patriot.

**A IA:**
- **A 1ª zona** sai com a War Factory pronta, depois de 3:00 (ou da 1ª onda), sobrando o dinheiro de um
  tanque. Pra juntar, a fábrica para no máximo 60 s.
- **Onde:** atrás do CC, do lado longe do inimigo, sob a cobertura da Patriot.
- **A 2ª zona** depois de 10:00 ou quando a base seca. Zona morta volta pela mesma regra, fora do mesmo lugar
  por 30 s.

### 3.2 Dano em área e alcance mínimo (a base do ataque e do Tomahawk)

Antes, todo tiro acertava um alvo só. Agora:
- O dano cai em linha reta, medido da borda do alvo: cheio até a 1ª distância, metade na 2ª, zero depois.
- **Tomahawk:** cheio até 2 m, 50% em 5 m (medido: 110 / 110 / 82 / 55 / 0 a 0 / 2 / 3,5 / 5 / 5,1 m da borda).
  **Míssil do ataque:** cheio até 3 m, 50% em 6 m.
- Só o inimigo leva. O aliado e você mesmo levam 0.
- **Alcance mínimo:** o Tomahawk não mira nem atira a menos de 20 m.
- **A artilharia prefere prédio.** Com um tanque a 40 m e uma Patriot a 66 m, o Tomahawk mira a Patriot.
  Assim o cerco sai quase sozinho.

Pra quê: o bolo de tanques vira decisão. O ataque pune o bolo, o Tomahawk pune Patriots juntas, e o tiro
forçado (Ctrl) no chão passa a valer.

### 3.3 Cruise Missile Strike (o poder da patente 2)

**O que você vê:**
- Na coluna de poderes acende "READY".
- F1 (ou o clique no botão) põe um círculo azul de 12 m no cursor. Clique esquerdo dispara; o direito ou Esc
  cancela. Se não pode, o motivo aparece no mouse.
- Seis mísseis entram pelo lado da tua base, com o rastro branco da Patriot, e caem em leque. Cada impacto é
  a explosão clássica em volume dos prédios, com a cratera B.
- Quem é atacado vê o anel vermelho e o aviso "Missile strike incoming!".

| | |
|---|---|
| Libera | Na patente 2. Precisa do CC vivo e pronto |
| Custo | Grátis. Recarga de 180 s depois de cada uso; fica pronto assim que libera |
| Mira | Qualquer ponto do mapa que o teu time já explorou |
| Na R2 / na R4 | 6 mísseis num círculo de 12 m / 9 mísseis em 14 m (cresce sozinho) |
| Cada míssil | 150 de explosão em área: cheio até 3 m, 50% em 6 m |
| Tempo | As explosões caem 2,8 a 3,8 s depois do clique (até 4,4 s com 9) |
| Padrão | Um girassol fixo, sem sorteio. O dano vale XP pra quem lançou |

**Dano:** medido, a Patriot leva ~665 de 1.000 e a War Factory ~900 de 2.000. Na conta: a Drop Zone leva ~890
de 1.200 (74%); a usina, ~800 de 800 (morre, e as Patriots dela apagam); 6 tanques parados, ~1 morto e o resto
com ~45% de vida.

**Por que esses números:**
- **Quanto vale.** Um uso tira uns $2.000–3.000 do inimigo a cada 180 s, ou $700–1.000/min. O alvo era valer
  uma zona madura: um poder vale um prédio de renda, não mais. Com o avião de 90 s a zona dava $1.000/min; com
  o de 70 s, ela passou um pouco à frente ($1.286/min).
- **Ajuda quem está atrás.** O valor é fixo e não depende da tua economia, então pesa mais pro lado mais
  pobre.
- **O que acerta.** O que não foge: Patriot, fábrica, zona, usina. Matar a usina apaga as Patriots, e aí a
  onda entra: ZH puro. Um tanque a 7,9 m/s sai de 12 m em menos de 2,5 s, então o ataque premia a atenção e
  pune o bolo; não é sorte.
- **A arte.** Não nasce fumaça nova: o rastro é o da Patriot aprovada, e a regra da fumaça fica de pé.

**Como se joga contra:**
- mexer ou espalhar quando o anel vermelho aparece (~3 s);
- matar o CC dele, o que desliga o poder;
- não estacionar o exército em bolo na porta da fábrica;
- depois: o laser do Paladin derruba os mísseis.

**A IA.** A cada 3 s com o poder pronto, ela dá nota a cada ponto inimigo que já viu: o valor em volta (o
custo vezes a vida que sobra), com peso 3× pra zona, 1,5× pra defesa, usina e artilharia, 1× pra outros
prédios e unidades paradas e 0,4× pra unidades andando. Dispara se a nota passar de 2.000.
- Uma zona sozinha dá 7.500; 6 tanques parados, 5.400; uma Patriot sozinha, 1.500 (não dispara); com a usina, 2.700.
- Prédio já quebrado vale menos, então quem termina o serviço são os tanques.

### 3.4 Tomahawk Launcher (`usa_tomahawk`, patente 3)

**O que você vê:** o chassi do caminhão de suprimento com dois tubos num berço que gira. Ele dispara rajadas
de 2 mísseis em arco longo, com o rastro branco, que caem nas Patriots de fora do alcance delas.

| | |
|---|---|
| Custo e obra | $1.200, 15 s, na War Factory, com a patente 3 |
| Vida e velocidade | 200, armadura de tanque; 7,0 m/s |
| Visão | 70 m: o radar próprio, maior que o alcance da Patriot |
| Arma | Alcance de 85 m, mínimo de 20 m. Rajada de 2 mísseis de 110, a 0,6 s um do outro. Recarga de 9 s (22,9 de dano por segundo) |
| Área | Cheio até 2 m, 50% em 5 m |

**Por que esses números (a lei de Lanchester):**
- A força de um exército cresce com N² × dano por segundo × vida. Por dólar, o índice é
  E = dano por segundo × vida ÷ custo².
- A Patriot tem E = 0,0436, 2,45× o Crusader (0,0178). Por isso as ondas de tanque morriam nela.
- A Patriot alcança o Tomahawk a 63,2 m. Ele vê a 70 e atira a 85, então fica de fora sem perder vida.
- Em campo aberto ele é inútil de propósito: E = 0,0032, 18% do Crusader.
- **Medido:** 3 Tomahawks parados a 72,6 m matam uma Patriot em ~12 s depois do 1º tiro, sem perder vida.
  2 Crusaders matam um Tomahawk em 2,6 s.
- **O ciclo:** a Patriot ganha da onda de tanques; o Tomahawk ganha da Patriot; o tanque ganha do Tomahawk;
  o ataque de mísseis pune o cerco parado. O defensor tem de sair da cobertura e brigar em campo aberto, e a
  partida muda de cara perto do minuto 9.

**Como se joga contra:**
- ir pra cima com tanques: a menos de 20 m ele não atira, e morre em ~2 s;
- o ataque de mísseis no grupo do cerco;
- atacar em outro lugar enquanto o exército dele está parado no cerco;
- Patriots espalhadas, pra uma rajada não pegar duas.

**A IA:** quando a patente deixa, ela quer 1 Tomahawk + 1 por defesa inimiga que conhece, até 4, sempre com
2 tanques de escolta por lançador (mais 2). **O cerco:** com 2 ou mais Tomahawks perto de uma defesa, eles
atiram nela e os tanques param a 72 m, fora dos 63,2 m da Patriot.

### 3.5 A IA v4 (`SkirmishAI`)

**Como ela era (medido):** cada fábrica só fazia o primeiro armado da lista; conhecia 4 papéis e nenhum
poder; atacava o mais perto (caminhão e Patriot valiam igual); nunca defendia; a economia quebrava aos ~4 min.

**O que mudou:**
- **Os papéis saem dos dados:** renda = prédio com avião; artilharia = arma com alcance mínimo. Unidade nova
  entra sem código.
- **3 caminhões por centro.** O 3º se paga em 40 s. Enquanto falta caminhão, a fábrica guarda o dinheiro
  dele.
- **A energia olha a obra que vem.** A zona, de −3, não sobe sem energia.
- **A expansão vem cedo.** Com 25% das caixas de casa, ela põe um centro novo no depósito do meio, uns 2 min
  antes de o de casa secar.
- **A fábrica escolhe pelo que falta.** O Tomahawk entra quando a patente deixa.
- **A casa se defende.** Inimigo armado a 90 m de um prédio chama quem está a até 200 m de casa.
- **A onda cresce com a patente**, e tem **o cerco** (ver o 3.4).
- **O alvo é escolhido pelo valor:** a zona primeiro, depois o caminhão, depois o centro e a usina, depois o
  resto. A Patriot pesa pouco, a não ser que haja artilharia junto (com tanque sozinho a troca é ruim).
- **Tudo inteiro e repetível,** com desempate pelo menor Id. Cada decisão vira uma linha no log:
  `RTS AI p1: drop zone at x,y`, `strike at x,y score s`, `siege on id`, `usa_tomahawk queued`,
  `expand to dock k`, `defend at x,y`.

**Ela continua batível:** nunca recua nem conserta; dispara o ataque mesmo quando o teu exército pode desviar;
e o cerco avisa (tanques parados a 72 m da tua Patriot).

## 4. A patente do general

**As regras do XP:**
- Cada acerto vale o dano que entrou, até a vida que o alvo tinha, convertido em dinheiro: dano × custo ÷
  vida cheia.
- Pro dono de quem acertou: 1 XP por $10 destruídos. Pro dono do alvo: 1 XP por $30 perdidos.
- Conta por acerto, não só por morte, então a barra anda a cada tiro.
- Não vale XP: obra inacabada, alvo próprio ou aliado, coisa de custo 0. O míssil no ar ainda dá XP ao dono,
  mesmo se o lançador já morreu.

**XP por morte (quem matou / quem perdeu):** Crusader 90/30, caminhão 60/20, Dozer 100/33, Patriot 100/33,
usina 80/27, War Factory 200/67, CC 200/67, Drop Zone 250/83, Tomahawk 120/40.

**A escada** (a tua, IDEIAS:178-186):

| Patente | XP | Libera |
|---|---|---|
| 1 | 0 | o de sempre |
| 2 | 600 | Cruise Missile Strike (6 mísseis) |
| 3 | 1.500 | Tomahawk Launcher |
| 4 | 3.000 | o ataque cresce pra 9 mísseis em 14 m |
| 5 | 5.000 | reservada pra superarma (depois) |

A tua escada pedia um 2º poder na R4. Como hoje só existe um poder, a R4 faz o ataque crescer.

**Contra a bola de neve:**
- Quem perde de 2 × 1 ainda sobe a 72% da velocidade do vencedor: chega a cada patente ~1,5 min depois.
- Quem só se tranca em casa fica na R1.
- Fundação vale 0, então não dá pra fazer XP de obra. Construir e juntar dinheiro não dá XP.

**No HUD:** as estrelas e a barra de XP no painel do comandante; o letreiro dourado
"PROMOTED - RANK 2: Cruise Missile Strike"; o botão bloqueado com cadeado e "Requires: Rank 3".

## 5. A partida-alvo e o que o harness mediu

**O alvo** (1 × 1 contra a IA no World_TestSalt2, com $10.000):

| Quando | O que acontece |
|---|---|
| 0:00–1:10 | A abertura do ZH de sempre: usina, centro, fábrica, Patriot, 1º tanque |
| ~1:20 | O 3º caminhão. A renda vai de ~2.700 pra ~3.500 $/min |
| 2:00–3:00 | Sai a 1ª onda (6 tanques). Aos ~3:00, o 1º contato, e o XP começa a correr |
| 3:00–4:10 | A 1ª zona, atrás do CC |
| 4:00–5:30 | A patente 2 (a IA dispara o ataque em até 3 s se achar alvo). Aos ~4:30, o 1º C-130, com 3 paletes |
| ~7:30 | A zona madura, o 2º ataque e o centro novo no depósito do meio antes de a casa secar |
| 8:00–10:00 | A patente 3 e os Tomahawks |
| ~9:30–10:00 | O depósito de casa seca. A 2ª zona |
| 10:00–13:00 | O cerco. O defensor sai da cobertura |
| 13:00–16:00 | A patente 4 e o ataque de 9 mísseis |
| 16:00–25:00 | O ataque e o cerco abrem as bases. Alvo: fim na mediana dos 18 min |

**O que faz o minuto 15 diferente do minuto 5:**

| | No minuto 5 | No minuto 15 |
|---|---|---|
| Renda | O depósito de casa, que acaba | Duas zonas maduras e o depósito do meio, disputado |
| Exército | Só Crusaders | Tomahawks com escolta |
| Alvos | Caminhões | Zonas, usinas e Patriots |
| Ameaça | No máximo o 1º ataque, de 6 mísseis | 9 mísseis a cada 3 min |
| Onde é a briga | Na estrada de casa | No meio do mapa |

**O que o harness mediu.** Foram 20 partidas robô × robô, só na simulação, sem a Unreal: 10 sementes, cada
uma jogada duas vezes com os lados trocados, com teto de 30 min.

| Meta | Alvo | Avião de 90 s | **Avião de 70 s (o que ficou)** |
|---|---|---|---|
| Duração mediana | 15–25 min | 15,5 min | **15,9 min** |
| Partidas acima de 12 min | ≥ 70% | 17/20 | **19/20** |
| Empates no teto | ≤ 15% | 0/20 | **0/20** |
| 1ª zona até 5:00 | ≥ 90% | 40/40 (~2:53) | **40/40 (~2:53)** |
| 1º avião pago até 6:00 | — | 40/40 (~4:26) | **40/40 (~4:26)** |
| Zonas de antes de 12:00 que se pagaram | ≥ 70% | 40/44 | **47/51** |
| R2 até 6:30 / R3 até 12:00 | ≥ 80% / ≥ 60% | 40/40 / 40/40 | **40/40 / 40/40** |
| Lado na R2 que disparou o ataque | todos | 40/40 (nota média 2.942) | **40/40 (2.953)** |
| Partidas com R3 que tiveram Tomahawk | todas | 20/20 | **20/20** |
| Renda nunca zerada por mais de 60 s | todos | 18/40 | **36/40** |
| Banco acima de $6.000 por no máximo 90 s | todos | 40/40 | **40/40** |
| O gasto muda ≥ 20 pontos entre 0–5 e 10–15 min | — | 25/40 | **37/40** |

Antes da v4, a mesma partida acabava em 5,4–6,5 min. Na rodada de 70 s:
- nos lados ainda vivos aos 15 min, a mediana da renda é de ~1.500 $/min dos caminhões e ~1.050 $/min dos
  aviões;
- em 9 dos 14 lados em que Tomahawk e Patriot se encontraram, o Tomahawk matou pelo menos uma Patriot.

**Onde o medido foge do alvo: a patente chega mais cedo.** A R2 vem aos ~3:47 (alvo 4:00–5:30), a R3 aos
5–7 min (alvo 8–10), a R4 aos ~9 min (alvo 13–16) e a R5 aos ~14 min, em 30 dos 40 lados (alvo 21+).
- A IA briga mais do que a conta previa: aos 4 min cada lado já tem ~1.300 XP (a mediana), e a conta dava ~660
  pelo 1º choque.
- Limiares maiores (1.000 / 3.000 / 6.000 / 10.000) foram testados, e as partidas ficaram mais curtas. Por isso
  a recomendação foi manter os de hoje (ver o §8).

## 6. O que mudou contra o spec esta noite

1. **O avião vem a cada 70 s, não 90.** Medido nas 20 partidas: a renda que zerava por mais de 60 s quando os
   caminhões morriam caiu de 22 pra 4 lados em 40, e a mistura de gasto foi de 25 pra 37 em 40. A zona madura
   passa a render $1.286/min e se paga em ~3,7 min (antes, 4,25).
2. **O lote é de 16 × 16 m, não 20 × 20.** O modelo provisório fechou na grade de 4 m.
3. **O avião voa a 24 m, não 70**, pra caber na câmera do RTS.
4. **A carga do pátio é a caixa do lançamento** (1,2 × 1,0 m), a mesma que desce no paraquedas. A caixa do
   depósito é uma pilha de 4 m e saía pra fora do pátio.
5. **O impacto do míssil do ataque** é a explosão clássica em volume dos prédios (aprovada, sem destroços),
   com a cratera B, maior que a do tiro de tanque. Um Tomahawk de verdade (~450 kg de ogiva) faria uma bola
   de ~7,7 m, que não cabe na leitura do RTS. A explosão grande sai em 1 de cada 2 mísseis: com as 6 juntas a
   fuligem fechava ~35 m por 3 s e cobria o alvo (efeito que cobre unidade é defeito); os outros ficam com o
   clarão do míssil e a cratera. O míssil do lançador Tomahawk continua com a cratera A.
6. **O míssil do Tomahawk não acelera além da velocidade que ainda deixa fazer a curva até o ponto.** Foi uma
   correção fora do spec: sem ela, o míssil rodava em volta do ponto a 20–30 m e passava 7–25 m longe dele.
7. **A IA** (`SkirmishAI.h`):
   - pra juntar o dinheiro da zona, a fábrica para até 60 s. O spec dizia 40, e com 40 a zona nunca saía;
   - a onda cresce 1 por patente (o spec dizia 2) e 1 por onda (eram 2), e volta a juntar com 4 (eram 2).
     Com a patente chegando cedo, a onda de 10 aos 5 min acabava a partida na 2ª onda;
   - as defesas: 1, mais uma por fábrica querida, até 4.

## 7. O que ficou pra depois (em ordem)

1. **O poço de petróleo no mapa**, a 1ª coisa da próxima noite. É renda de quem controla o mapa: o pátio de
   barris derrama quando morre, e quem segura o poço ganha XP. O WorldForge gera os poços (nenhum, poucos ou
   muitos), o que pede um contrato no `Maps.json` com a sessão do WorldForge.
2. **O Dozer conserta prédio**, como no ZH. Hoje nada cura; com as zonas em jogo, é a próxima defesa.
3. **O Promote com escolha:** 2 opções por patente. R2: Spy Drone ou Emergency Repair. R3: o A-10.
4. **A 2ª linha da War Factory na R3** (direção aprovada: 2 linhas, +15%).
5. **O Strategy Center:** os 3 planos de batalha do ZH e as melhorias. Não precisa de arte.
6. **Ar na simulação:** o antiaéreo da Patriot; o C-130 que pode ser derrubado (a carga cai como pilha); o
   Chinook e o Spy Drone.
7. **A veterania das unidades,** com uma recompensa por matar veterano como contrapeso.
8. **O Paladin (R3):** o laser que derruba míssil e fecha o ciclo do Tomahawk e do ataque.
9. **O Particle Uplink Cannon na R5,** com o relógio público aprovado, pra forçar o fim.
10. **Regras menores:** a 2ª zona mais cara; fogo amigo no dano em área; a IA desviando do ataque; a IA
    respeitando a névoa; níveis de dificuldade.
11. **A arte final (tua)** da zona, do C-130 e do Tomahawk.

## 8. Pra conversar

- **A patente chega cedo:** a R3 aos ~6 min, em vez de 9. Mantém assim ou sobe os limiares, aceitando
  partidas mais curtas?
- **A R4 faz o ataque crescer,** em vez do 2º poder da tua escada. Serve até o Promote voltar com escolhas?

## 9. Como testar

**A placa de vídeo:** o `game.ps1` entra sozinho na fila da placa (`docs/GPU.md`).

**As cenas** (`Game/Tools/game.ps1`; `-Record` grava o vídeo):

| Comando | O que conferir |
|---|---|
| `game.ps1 view alta -Scenario v4_airlift` | A zona pronta: o avião, 3 paraquedas nas vagas, 3 × "+$300" e a contagem no cartão |
| `game.ps1 view alta -Scenario v4_strike` | Na patente 2, contra 2 Patriots, usina e War Factory: o anel, os 6 rastros vindo de trás, as explosões e as crateras |
| `game.ps1 view alta -Scenario v4_strike_r4` | A mesma cena com 9 mísseis. Medir o tempo de quadro na Alta |
| `game.ps1 view alta -Scenario v4_siege` | A IA na R3, com 3 Tomahawks e 4 tanques, contra 2 Patriots: os arcos, os tanques parados a ~72 m e nenhuma faixa cinza |

**A partida longa, IA × IA** (a IA joga pelos dois lados; uma foto a cada 15 s e em cada ataque de mísseis):

```
game.ps1 view alta -Script aivsai -Folder X -Extra "-RTSSelfAI -RTSTestMinutes=20"
```

**Jogando você:** a skirmish normal contra a IA. Os poderes ficam no F1–F3.

**A simulação sem a Unreal:** `powershell -File Game\Tools\simtest.ps1`. Os grupos novos são `[patente]`,
`[area]`, `[alcance minimo]`, `[lancamento]`, `[ataque]`, `[cerco]` e `[ia v4]`. A última rodada (23/10) deu
tudo certo: o mesmo `FINAL_HASH` nas 3 compilações, com a simulação na versão 19. Os testes não leem o
`Units.json`, então a zona deles ainda usa o avião de 90 s.

**Onde está no código:**
- **Dados:** `Game/Config/RTS/Units.json`; as cenas em `Scenarios.json`.
- **Simulação e IA:** `World.cpp` (a patente, o dano em área, o alcance mínimo), `WorldEconomy.cpp` (o avião
  da zona), `WorldPowers.cpp` (o ataque), `SkirmishAI.h/.cpp` (a IA).
- **Desenho:** `RTSAirlift.cpp` (o avião e os paraquedas), `RTSHUDNew.cpp` (as estrelas, os poderes, o cartão
  da zona), `RTSPlayerController.cpp` (a mira do poder).
- **Modelos provisórios:** `Game/Tools/model_drop_zone.py`, `model_cargo_plane.py`, `model_supply_crate.py` e
  `model_tomahawk.py`, importados com `game.ps1 v4models`.
