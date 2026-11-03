# Segunda opinião: as superarmas (o advogado do diabo, 01–02/11/2026)

**O que eu li:** `docs/SUPERARMAS.md` inteiro, as duas pranchas (`predios_4_faccoes.jpg`, `momentos_4_faccoes.jpg`), `VISAO_DO_JOGO.md`,
`ELENCO.md` §1, §2, §7, §8, `FACCAO_RUSSIA.md` §1, `ECONOMIA.md` (renda, fila, energia, vender), `concept/hud/DECISOES.md` (topo e coluna
de poderes), o `Units.json` (as antiaéreas de verdade) e a pesquisa de efeito que chegou (`scratchpad/estudo_superarmas_real_fx_som.md`).
A pesquisa de RTS (`ESTUDO_RTS.md`) ainda não existia; conferi o ZH direto nos INI e no fonte que a pesquisa baixou (`scratchpad/zhini/`,
`scratchpad/zhsrc/`).

**O que eu rodei:** a conta do estudo e variações numa cópia (`scratchpad/segunda/conta.py`, `variacoes.py`, `variacoes2.py`,
`variacoes3.py`, `tabela.py`). Nada no repositório foi mexido.

**Em uma frase:** a ideia que amarra (a cicatriz de cada facção) e o relógio pago e público são bons; mas a conta da §8 não sustenta o
"as quatro ficam na mesma faixa", a USA sai a mais forte e a única sem resposta, a GLA sai a mais fraca e a mais anulada pela defesa,
o custo tem quatro buracos de regra, e vários números "da física" misturam potências diferentes.

---

## 1. Diversão e frustração

| | Usar | Levar | Onde vira frustração | O conserto |
|---|---|---|---|---|
| **USA** | O melhor de usar: a mira de 6 pontos é um quebra-cabeça e premia quem conhece a base inimiga. | O pior de levar: 3 s de aviso, nada derruba, e ela escolhe exatamente o que dói (usinas, tecnologia, a sua superarma). | **"Perdi sem poder fazer nada"**: a regra 3 do próprio estudo ("toda superarma tem resposta") não vale pra ela; a tabela da §0 escreve "quem para: ninguém". | A precisão custa visão (§4): mira ponto a ponto só onde o jogador vê agora; fora da visão, cada ponto desvia. A área aparece pra vítima no lançamento, os pontos nos 3 s finais. A resposta vira matar os batedores, o radar e a energia: a cara da USA ("vê tudo, acerta primeiro") ao pé da letra. |
| **Rússia** | O clímax do jogo; tudo ou nada é divertido. | Agência só pras unidades (14 s com o anel exato desde o lançamento: todo mundo foge sempre). | Os prédios do miolo somem sem defesa possível; o "31% do valor" esconde que caem a fábrica, o centro de suprimento e as usinas (a base para de produzir e de receber, §3). | Aviso que **encolhe**: no lançamento, a elipse grande da previsão; o anel exato só nos últimos ~5 s (o radar real afina a previsão com o tempo). Fugir cedo custa abandonar mais área; esperar arrisca. E o contra-jogo de sempre do ZH, escrito: base espalhada. |
| **China** | Bonita de ver, mas **pouca escolha**: depois do clique, os drones decidem tudo. | A melhor de levar: a antiaérea vira herói, dá pra lutar. | Contra quem não tem antiaérea perto do exército, ela apaga a batalha decisiva (70% de um exército de campo de 20 tanques, §3). | Dar uma escolha que já existe na linguagem do jogo: **Ctrl na mira = só prédios** (o "Ctrl força o tiro" do C&C, já decidido), sem Ctrl = o que valer mais. Mais simples que botão esquerdo/direito. |
| **GLA** | Nostalgia máxima, mas **a mais frustrante de usar**: com 2 Patriot por perto, 4 dos 9 somem no céu e a "tempestade" vira chuvisco (17–20% em média, 7% nos 10% piores). | O gás é chato e justo. | "Superarma que não faz nada" contra a USA e a Rússia, que são as duas facções que mais constroem antiaérea fixa. | §3.4: só o Patriot intercepta balístico, e o SCUD derrubado cai torto em vez de sumir. |

**O 1×1 competitivo.** O prédio custa $5 000 e 60 s, a primeira ogiva mais $1 500–3 500 e 4:30–6:00: o primeiro golpe chega depois da
decisão (60 s de obra + a carga: 5,5–7 min), com ~$7 500–8 500 parados (≈ 9 tanques). Nas partidas IA × IA de 01/11 as vitórias saem entre 6 e 14 min. No 1×1 a superarma vai
ser **uma jogada de fim de jogo ou de quem está na frente**, e isso está certo **se** ela funcionar como lance forçado: o relógio público diz
"você tem 4:30 pra me atacar". Pra isso o estudo precisa de uma regra que não está lá: **o prédio é revelado ao inimigo quando fica pronto**
(o aviso "detected" com a posição). Sem isso, o contra-jogo de todas (matar o prédio enquanto carrega) não existe.
O risco real do 1×1 é a **corrida de superarmas** (§3.3): hoje quem dispara primeiro mata a superarma do outro na mesma salva.

**Partidas em time** (não estão no estudo): duas ou três superarmas do mesmo time na mesma base em 30 s apagam um jogador. O LIMITED "um
prédio por jogador" não resolve, é por jogador. É o ZH (lá era igual); só precisa entrar no teste (IA × IA 2×2) e ser uma decisão escrita.

---

## 2. O custo de lançar

**O que fica:** o relógio só corre pago e é público; o pagamento vira informação. É a melhor parte da proposta e é original no gênero.

**Os buracos:**

1. **A fila do jogo devolve tudo.** O estudo diz "a ogiva é comprada como uma unidade na fila" e "destruíram o prédio no meio: perdeu o que
   pagou". Mas a fila que existe hoje (`ECONOMIA.md`, Parte 5 e 6) **devolve 100% ao cancelar, devolve a fila quando a fábrica cai, e vender
   devolve a fila inteira** (o `cancelAndRefundAllProduction`). Sem uma exceção escrita, o risco de armar cedo some: o prédio vai cair, o
   jogador cancela (ou vende) e recebe tudo de volta. **Regra:** cancelar devolve só a parte do relógio que falta (ou nada); vender ou
   perder o prédio não devolve a ogiva.
2. **O estoque.** "Como uma unidade na fila" = até 9 na fila. Precisa estar escrito: **1 ogiva por prédio, pronta ou montando**; com uma
   pronta, não começa outra. Aí guardar pronta tem custo natural (o relógio da próxima não corre e o prédio pode cair com ela dentro).
   Com isso, "armar e guardar pra sempre" deixa de ser buraco: é escolha com risco, como no ZH.
3. **O custo é imposto, não decisão.** Com a renda do próprio estudo (~$3 000/min no meio da partida), o lançamento é **10–19% do que
   entra durante a carga** (USA 18%, Rússia 19%, China 10%, GLA 13%) e devolve **3 a 6 vezes** o que custa (§3). Ninguém deixa de armar,
   a não ser quem está quebrado. Na prática o "relógio parado" vai significar só "acabou de construir" ou "está sem dinheiro". Não é
   motivo pra descartar (o atraso do primeiro golpe e o risco do prédio já valem), mas o estudo não pode vender o custo como "a decisão":
   a decisão real é **quando** armar (anunciar), não **se**.
4. **Dinheiro sobrando no fim:** os depósitos do mapa acabam (400 caixas, $30 000 cada no ZH), então o fim de jogo se limita sozinho;
   só com o dinheiro inicial de 50 000 o custo some, e aí é escolha da sala. O que o estudo não diz: **o custo premia quem está na frente**
   (o rico arma, o pobre não). É o oposto de mecânica de virada. Aceitável, mas tem de ser decisão consciente.
5. **A China pagando em energia é confusa e se sabota.** Três problemas:
   - **o ZH já cobra −10 de energia de toda superarma** (o Particle Cannon e o silo da China: `EnergyProduction = -10` no
     `Obj_FactionBuilding.ini`): o "custo escondido" da China é o padrão do ZH, não uma moeda nova;
   - **"presos enquanto carrega"** quer dizer que armar derruba a China pra energia baixa se ela não tiver um reator sobrando, e aí as
     Gattling Cannon dela desligam (`needs_power: true` no `Units.json`): **armar o enxame apaga a antiaérea da própria base**;
   - o estudo também diz "sem energia, o relógio para": se a colmeia é quem causa a falta, **ela para o próprio relógio**. Laço sem saída.
   **Regra:** as três facções com energia pagam como o ZH (o prédio consome −10 sempre), a China paga em dinheiro como as outras. "Matar os
   reatores atrasa o enxame" continua valendo, pra USA e Rússia também.
6. **A GLA está ambígua.** "$200 por SCUD, 25 s cada, até 9" dá 3:45, mas a carga da tabela é 4:30. Se ela pode lançar a qualquer momento
   com o que montou, a GLA solta 3–4 SCUDs a cada 90 s: a superarma vira uma artilharia periódica (o despertador que o Felipe não quer, só
   que mais frequente), e 3 SCUDs contra 2 Patriot não chegam. **Regra:** o relógio é a montagem (9 × 30 s = 4:30), cada vaga só corre
   paga; lançar antes com o que tem (mínimo 4) **zera o ciclo inteiro**. A decisão "espero os 9 ou solto 5 agora?" fica, sem virar spam.
7. **"Cada carcaça desmanchada monta 1 SCUD de graça": cortar.** O Worker já transforma carcaça em dinheiro, e o dinheiro já compra
   SCUD: é a mesma coisa contada duas vezes, e bola de neve (quem ganha a luta ganha a superarma de graça).

**A regra proposta, inteira:** o prédio dá o direito; a ogiva se arma no prédio (1 por prédio); pagar começa o relógio, que é público;
pronta, lança quando quiser; cancelar devolve só o que falta, vender ou perder não devolve; sem energia o relógio para (USA, Rússia,
China: o prédio consome −10, como no ZH); a GLA monta 9 vagas de 30 s e pode lançar cedo zerando o ciclo; o prédio é revelado ao inimigo
quando fica pronto.

---

## 3. O balanceamento

### 3.1 O que a conta da §8 esconde

- **A tabela não sai do script publicado.** O `superarmas_conta.py` imprime uma semente só: a GLA "sem derrubar" dá 18,5% (semente 1), e
  com 2 derrubados dá 23,8% (mais que com 0). O 23% do estudo só aparece na **média de 200 sementes** (rodei: 23,4%), e a dispersão
  esconde o principal: **p10–p90 de 13% a 30%**. A GLA é uma moeda jogada; a tabela mostra um número.
- **Uma base só, compacta e desenhada à mão.** A base que o jogador ou a IA monta não é essa. Espalhar 1,5× derruba a nuke no meio de
  $9 300 pra $2 700. O contra-jogo do ZH contra superarma é exatamente esse, e a conta não mede.
- **A mira não foi buscada.** O estudo escolhe a mira à mão. Buscando a melhor (rodei uma busca gulosa com reinícios):
  - USA: **$15 100 (50%)**, contra os $8 100 (27%) "bem mirada";
  - Rússia com as unidades fugidas: **$13 300 (44%)** mirando em (−32, 12), contra os $9 300 (31%) no meio. **E as duas, na mira ótima,
    matam a superarma inimiga junto** (2 500 de vida por $5 000 é o melhor alvo por vida da base).
- **A reação não é a mesma pra todas.** A Rússia ganha o desconto "as unidades fogem nos 14 s"; a China não, mas o enxame é visto
  cruzando o mapa (a 40 m/s, 500–900 m = 12–22 s de aviso), então as unidades também fogem dela. A GLA tem 10 s com a elipse. Com a
  mesma regra, **a China sem unidades no raio faz $6 200 (20%)**, não 31%.
- **A antiaérea é inventada.** "2 SCUDs por Patriot por salva, 1 pelo resto" não sai da arma. A base de referência **tem 2 defesas**, e a
  manchete da China e da GLA é "sem antiaérea". Pela taxa das armas do `Units.json` (o Patriot solta 4 mísseis a cada 2,75 s no ar; a
  Gattling, 5 de dano a cada 0,25 s contra 15 de vida; o Pantsir Site, 1 a cada 1,5 s; o Stinger, 1 a cada 2 s), com ~10 s debaixo do
  fogo, **2 Patriot derrubam ~38 drones, 2 Gattling ~27, 2 Pantsir ~13, 2 Stinger ~10**. A mesma dupla de Patriot tira 44% da GLA (4 de 9).
- **O % do valor é a régua errada.** O que vira a partida é **o caminho crítico**: fábrica, centro de suprimento, usinas. A nuke no meio
  "tira 31%", mas tira 5 dos 5 prédios críticos: sem centro de suprimento o caminhão não entrega (`ECONOMIA.md`, "Entrega: no Supply
  Center do próprio jogador"), sem usina tudo anda à metade. A régua tem de ser **o tempo pra voltar a produzir e a receber**.
- **"Destruído por minuto de carga" esquece metade do custo:** o prédio ($5 000), o lançamento, e o risco de perder a ogiva.
- **A alocação dos drones não faz o que o texto diz.** "Com drones o bastante pra matar" vira `min(k, n)` no código: manda o resto num alvo
  que não morre. E os drones derrubados saem do fim da lista, quando no jogo saem de qualquer alvo (deixando vários "quase mortos"):
  o "21% com metade derrubada" é otimista.
- **O reator chinês que derrete não está na conta** (400 até 13 m, em cadeia). Rodei: a USA "bem mirada" na base da China ganha uma usina
  a mais pela cadeia (24% → 27%).
- **A regra 2 da §1 cai na própria conta:** "uma salva nunca mata o CC" e a USA mata (4 ogivas de 1 300 = 5 200). O estudo diz que é
  escolha; mas então a regra é "nunca, exceto a mais forte".

### 3.2 As variações (na régua do estudo: $30 250 = a base com as unidades)

| Cenário | Destruído | % | Críticos que caem (fábrica, suprimento, 3 usinas) |
|---|---|---|---|
| USA "bem mirada" do estudo | $8 100 | 27% | 4 |
| **USA ótima (busca), com unidades** | **$15 100** | **50%** | 3 |
| USA na superarma inimiga + tecnologia + usinas | $10 200 | 34% | 3 |
| Rússia no meio, unidades fugiram (a do estudo) | $9 300 | 31% | 5 |
| **Rússia na melhor mira, unidades fugiram** | **$13 300** | **44%** | 4 (e a superarma inimiga) |
| Rússia no meio, base ×1,5 | $2 700 | 9% | 3 |
| Rússia na melhor mira, base ×1,5 | $10 000 | 33% | 1 |
| China sem antiaérea, unidades paradas (a do estudo) | $9 450 | 31% | 0 |
| **China, unidades fora do raio** | **$6 200** | **20%** | 3 |
| China, só prédios, ~38 derrubados (2 Patriot pela arma) | $4 300 | 14% | 2 |
| China num exército de campo de 20 tanques ($18 000), sem antiaérea | $12 600 | 70% do exército | — |
| GLA na fábrica, 0 / 2 / 4 derrubados (média de 200 sementes) | $7 100 / $6 100 / $5 300 | 23 / 20 / 17% | p10–p90 de 13–30%, 10–30%, 7–27% |
| GLA com os números do ZH (500 até 13 m, 150 até 53 m), na fábrica | — | **46%** | 2 |

**Retorno por dólar de lançamento** (destruído ÷ custo da salva): USA 3,2× (ótima 6×), Rússia 2,7× (mira certa 3,8×), China 6,3× (só
prédios 4,1×), GLA 3,9×. Todas pagam com sobra: confirma o item 2.3.

### 3.3 A leitura

- **USA: forte demais na mão boa.** Imparável, precisa, a mais rápida (4:30) e mais barata que a Rússia, e na mira ótima empata com o
  melhor caso da nuke. O precedente do ZH é o Particle Cannon, que o jogador humano **dirige** (`PUC.cpp`: "All human players have manual
  control and must drive the beam"): 400 de dano/s por 10 s = **4 000 de dano**, andando a 5,3 m/s, visível pra vítima o tempo todo. A
  proposta é **7 800 de dano** (6 × 1 300) posto **de uma vez**, sem andar, com 3 s de aviso. É quase o dobro do pior pesadelo do ZH.
  Medido: com **4 ogivas** e a regra da visão, a ótima cai pra 39% com visão e 23% sem (desvio de 10 m).
- **Rússia: depende da mira e da base, não do número.** De 9% a 44% só mudando a mira e o espaçamento. Não está forte nem fraca: está
  **sem régua**. Com o aviso que encolhe (§1) as unidades voltam a ser alvo de quem lê bem o voo.
- **China: a mais eficiente por dólar e a mais robusta à defesa.** Perder 38% dos drones custa só ~12% do valor (os drones que sobram).
  E é a única que apaga um **exército de campo** (os drones perseguem; fugir a 8 m/s de drone a 40 m/s não funciona dentro do raio).
  Sugestão: menos drones (60–70) e mais fortes, pra cada abate pesar; e a defesa no caminho atirando o voo inteiro, não só na chegada.
- **GLA: a mais fraca e a mais anulada.** E é a facção que já perdia todas na 1ª rodada IA × IA (`ELENCO.md` §8.3).
- **A corrida de superarmas.** As duas na mira ótima matam a superarma do outro (2 500 de vida). Com a USA a 4:30 e a Rússia a 6:00,
  **a USA ganha toda corrida contra a Rússia**: dispara primeiro e mata o silo armado (e a Rússia perde a ogiva paga). O ZH resolveu isso
  com **4 000 de vida** em todo prédio de superarma (`MaxHealth = 4000.0`, os três): a nuke do ZH (3 500 no centro) não mata superarma
  numa salva. O estudo baixou pra 2 500–3 000 sem dizer por quê. **Voltar pros 4 000.**

### 3.4 A GLA contra a USA e a Rússia: a física diz outra coisa

- **O SCUD e a ogiva da USA são a mesma coisa pra quem intercepta:** dois corpos balísticos entrando a Mach 4–5. Se o Patriot derruba um,
  derruba o outro; "a USA vem do espaço, ninguém para" e "a GLA, todo mundo derruba" não é física, é escolha que bate na facção mais fraca.
- **O Pantsir não é antimíssil balístico** (é defesa de ponto contra avião, helicóptero, drone e míssil de cruzeiro), e a Gattling e o
  Stinger muito menos. **Quem intercepta balístico é o Patriot (o PAC-3 existe pra isso).**
- **1991 não foi o Patriot vencendo o SCUD:** a própria pesquisa (`estudo_superarmas_real_fx_som.md` §1.3 e §6, item 10) põe o "Patriot de
  1991" na lista dos números de propaganda; o que aconteceu de verdade é que **os pedaços caíam igual**, e o al-Hussein se partia sozinho
  na reentrada.

**A proposta:** só o Patriot intercepta míssil balístico (o SCUD e, no espelho USA × USA, a ogiva do Global Strike); o Pantsir, a Gattling,
o Stinger, o Quad e o Linebacker derrubam drone e míssil de cruzeiro. O SCUD interceptado **não some: cai torto** (desvio de ~35 m, metade
do estouro, a nuvem inteira). Rodei: com isso, 2 interceptados ainda dão 23% e 6 dão 16% (contra 20% e 14%), e o gás espalha pela base,
que é a cara da GLA. A defesa protege o alvo escolhido; não apaga a arma.

### 3.5 Os números do ZH que o estudo mexeu sem escrever o motivo

A regra do equilíbrio (`ELENCO.md` §2.1) é "partimos do ZH e mexemos só onde o nosso jogo é diferente, **com o motivo escrito**". Conferi
nos INI:

| | ZH 1.04 | O estudo | Motivo escrito? |
|---|---|---|---|
| Relógio | Particle Cannon 4:00, SCUD Storm 5:00, Nuke 6:00 | USA 4:30, GLA 4:30, China 5:00, Rússia 6:00 | não |
| Vida do prédio | 4 000 (os três) | 2 500–3 000 | não |
| Energia do prédio | −10 (USA e China), 0 (GLA) | só a China, "enquanto carrega" | não (§2.5) |
| SCUD | 500 até 13 m, **150 até 53 m**, 9 com dispersão de ~25 m; fogo amigo | 650 até 6 m, 220 aos 15 m, **zero aos 19 m** | não; é a metade do dano (46% × 23% na mesma conta) |
| Nuke | 3 500 até 16 m, caindo a 300 aos 55 m; fogo amigo | 3 600 até 22 m, 1 600 aos 45 m, zero aos 95 m | não; maior que o ZH |
| A arma precisa da USA | o feixe dirigido, 4 000 no total | 7 800 instantâneo | não |
| LIMITED | a opção "Limit Superweapons" do ZH (`GameInfo.cpp`, `m_superweaponRestriction`): 1 por jogador | "a proposta pro LIMITED" | o estudo apresenta como ideia nova; é a do ZH (melhor ainda: nostalgia) |

Mexer pode estar certo (a GLA interceptável pede **mais** dano, não menos). Mas cada linha precisa do porquê.

---

## 4. Originalidade e identidade

**USA, Global Strike.** A cara da facção, sim: precisão e informação. Dois problemas:
- **"6 e 6".** O general já tem o Cruise Missile Strike com **6** Tomahawks. Seis riscos caindo num círculo, de cima, com a câmera de RTS
  que não vê o céu (§5), lê parecido com seis Tomahawks em mergulho. A tabela §4.6 compara de onde vem e como se vê, mas de cima "de lado"
  e "de cima" viram dois riscos entrando pela tela. **4 ogivas** separa no número, é real (a modificação convencional do Trident, a proposta
  de 2006, levava 4 ogivas convencionais e foi barrada pelo Congresso porque o lançamento podia ser confundido com um nuclear: um detalhe de
  tom ótimo pro anúncio), cada uma maior e mais legível, e corta o excesso de força do §3.3.
- **A resposta.** A identidade "vê tudo, acerta primeiro" pede que a precisão dependa de ver: a mira ponto a ponto vale onde o jogador tem
  visão agora; no escuro, desvia. É a única superarma cuja resposta é **cegar** o inimigo, e é a cara dele.

**Rússia, a nuke.** O tom sério do ZH aguenta a nuke (o ZH tinha). O que não aguenta:
- **O nome "Sarmat".** O RS-28 é o míssil **estratégico** de 208 t com várias ogivas, e é o centro da ameaça nuclear de hoje na TV russa
  ("Satan II"); o próprio `FACCAO_RUSSIA.md` proíbe os símbolos dessa guerra, e a pesquisa põe o Sarmat na lista de propaganda. Pôr uma
  ogiva tática de ~20 t num míssil de 208 t é o contrário do "perto do real". Opções: **NUCLEAR MISSILE** (o nome do ZH: nostalgia e tom
  seguro) ou uma escapada no estilo do Contra; o silo fica.
- **A física é de duas potências.** O dano e os anéis são de uma nuke de ~20 t (a da pesquisa); a **cratera de 28 m** e a **bola de 40 m**
  são de ~0,3–0,4 kt (pela própria escala W^0,3 e W^0,4 da pesquisa: 20 t dá cratera de ~11 m e bola de ~18 m). E o script chama de
  "ar-explosão", que **não faz cratera nem chuva radioativa** (Glasstone, pela pesquisa §2a). Tem de ser **encostada no chão**, numa potência
  só, com as licenças escritas (ex.: "cratera 2,5× a física, pra ler de cima").
- **A radiação está ao contrário da física.** Nas potências baixas a radiação mata **mais longe** que o choque (a Davy Crockett, na
  pesquisa); o estudo põe 40 m, menor que o anel de dano. Pode ser licença (a física cobriria a base inteira por 2 min), mas escrita. A
  chuva radioativa real vai **a favor do vento** (uma elipse, não um círculo): bonito, físico e com consequência (pode cair no caminho do
  próprio russo). Depende do vento na simulação (§5).
- **O anel de Wilson no deserto:** só se forma em ar úmido (pesquisa §2a). No mapa de areia, não; nos mapas úmidos, sim.

**China, o enxame.** É a melhor das alternativas da tabela, concordo: nova, de hoje, com resposta. Ressalvas:
- **O nome está trocado.** O Jiu Tian ("Nine Heavens") é o **drone-mãe** (a alternativa B), não a colmeia. Ou o nome muda, ou a arma vira
  o B (a colmeia do prédio arma e o drone-mãe sai e solta o enxame sobre a base, com a sombra dele correndo antes, como a pesquisa sugere).
- **A assinatura que falta:** máquina não voa como estorninho. O bando orgânico da prancha lê como pássaro (o estudo admite). A China
  tem a cultura do **show de drones em formação** (os recordes mundiais são chineses) e os testes de enxame da CETC (o de 119 drones de
  2017; a conferir na pesquisa de RTS). **Voo em grade, em anel, viradas sincronizadas, as luzes de navegação piscando juntas**: lê como
  máquina, é chinês, e é icônico. Sobre a base, a formação "desenha" o anel antes de mergulhar.
- **Uma alternativa melhor?** Não achei. A barragem de foguetes pisa na Rússia; o DF-17 pisa na USA; o EMP não destrói; o bombardeiro com a
  bomba grande vira nuke. O enxame fica.

**GLA, SCUD Storm.** A cara dela, e o nome do ZH certo. O que falta pra ser GLA de verdade é a sujeira que a pesquisa trouxe e o estudo não
usou: **1 em 9 se parte na descida** (o al-Hussein), o **laranja-marrom do ácido nítrico** em cada impacto (a assinatura suja, real), a
dispersão grande (σ 25–40 m) e o interceptado caindo torto.

---

## 5. Leitura de cima e efeito

**A regra que muda tudo: o céu não está na tela.** A câmera de RTS olha pro chão até no alto do quadro (pesquisa §3.1 e §6.1); no zoom
mais perto a tela tem ~63 m de largura. Tudo que o estudo põe "no alto" (o estouro da USA, o Patriot derrubando SCUD, o enxame passando, o
chapéu do cogumelo) ou se conta **pela luz e pela sombra no chão**, ou acontece **na faixa que a câmera vê** (abaixo de ~2/3 da altura da
câmera), ou não existe pro jogador.

| | O risco | A alternativa |
|---|---|---|
| **USA** | O "clarão no alto" é licença (a pesquisa: a liberação real das ogivas **não tem clarão**). Diretamente em cima, as sombras encurtam e somem embaixo dos prédios: não "apontam numa direção". E 6 impactos em 1,2 s com clarão = **3 a 6 clarões por segundo**, acima do limite de fotossensibilidade (3 por segundo: o critério do WCAG 2.3.1 e do teste Harding que as publicadoras usam). | A luz que existe: **as ogivas incandescentes descendo são 4–6 luzes que se movem**, e as sombras da base giram e encurtam apontando onde cada uma vai cair. Isso é físico, é aviso e é espetáculo. O pedido dele ("estoura alto com luz forte") vira **uma luz que se abre em seis** quando entram no quadro, não uma bola de fogo no ar. O clarão grande fica num só, o primeiro; os outros, menores. |
| **Rússia** | "O chapéu passa por cima da câmera": a câmera entra no volume e **a tela inteira vira fumaça** (cobre a batalha, e é o pior caso do raymarch na 1660: a estimativa do `EXPLOSOES.md` citada pela pesquisa é 5–15 ms enquanto enche a tela). O anel de poeira da física fica minutos; a prancha mostra o quadro inteiro laranja. | A pesquisa já dá o caminho: altura comprimida (λ ≈ 0,12–0,15, a cabeça sai do quadro aos 8–12 s), **o volume some perto da câmera** (fade pela distância), meia resolução com reconstrução temporal, e a licença escrita: "a poeira acima de 3 m sai do nível das unidades em 3–5 s, a física daria minutos". |
| **China** | A prancha lê como pássaros. 100 explosões pequenas, se cada uma for um volume do BlastLab, não cabem; dezenas de fogos com coluna de fumaça que ficam são o custo de verdade. | A formação de máquina (§4). As explosões **injetam no volume compartilhado do Fluxo** (um campo só, não 100 volumes). Os fogos vizinhos se juntam em ~10–15 colunas. Os drones são malhas instanciadas: baratos. |
| **GLA** | A prancha mostra o gás amarelo cobrindo os tanques: viola a regra da silhueta com 90% tapado e o "efeito que cobre unidade é defeito". "Ralo o bastante pra ver as unidades" faz o gás não parecer perigoso. | **A física resolve a leitura:** gás pesado é uma **camada de 1–2 m** que escorre pro baixo e anda com o vento (pesquisa §2e). O soldado (1,8 m) **some dentro**, o tanque (2,4 m) **fica por cima**: o desenho conta a tabela de dano (100% no soldado, 25% no veículo). A técnica é a da indústria pra gás denso (o modelo de **camada rasa**, como o SLAB e o DEGADIS): uma grade 2D na simulação com a espessura do gás, e o desenho como uma fatia fina em volume, que recebe sombra e luz. É a mais barata das grandes, se for camada; a mais cara, se for um volume 3D genérico sobre a base. |

**O vento não existe na simulação.** O `SimCore` não tem vento (procurei: só aparece "window" em outro sentido). O gás que "anda com o
vento" e a chuva radioativa a favor do vento **mudam a partida** (dano), então pela regra da `VISAO_DO_JOGO.md` o vento tem de morar na
simulação determinística: um vento por mapa em ponto fixo, e o desenho (a poeira, as palmeiras, a fumaça) lendo o vento da simulação, não o
contrário. É um sistema novo que o estudo não lista.

**A interceptação no céu também não está na tela.** O SCUD derrubado a 2 km não aparece. Ou o encontro acontece na faixa visível (comprimido
como a nuvem), ou se conta pelo rastro do Patriot subindo e saindo do quadro, o estalo, e **os pedaços caindo em chamas** dentro da base.

**Daltonismo:** os avisos são todos vermelhos (anéis finos vermelhos, elipse, círculo). O aviso precisa de forma e movimento além da cor.

---

## 6. Os prédios

- **USA: discordo do 1; prefiro o 3.** O 1 é um bloco com uma **laje retangular com faixa zebrada embaixo de tudo**, o que ele reprovou na
  galeria de 01/11 ("base acompanha a forma": nada de laje quadrada embaixo de tudo). As 8 tampas pra **um** míssil confundem (lê "8
  mísseis"); de cima, a grade lê como estacionamento ou painel solar; e o Aegis Ashore é **defesa** antimíssil (a USA já tem o Patriot). O
  3 o próprio estudo chama de "o melhor desenho da folha": pátio redondo, o caminhão saindo do abrigo na encosta e **erguendo os tubos**
  (de cima, o retângulo comprido que vira um ponto quando fica de pé: o movimento mais legível das quatro folhas), e é real (o Dark Eagle).
  Os 2 tubos casam com a arma: 2 mísseis × 2 ogivas = 4 (a minha proposta) ou × 3 = 6. Se ele gostar do 1: uma tampa grande só, redonda,
  sem a laje.
- **Rússia: concordo com o 1**, a forma mais forte das quatro (o círculo no morro). Ressalvas: o morro cor de terra **some no deserto**
  (precisa da borda de concreto dura e da cor do jogador nas lonas e redes, a regra da facção); falta a língua da Rússia (o telhado-serra, a
  chaminé listrada do 2); e o estado "tampa abrindo" é o aviso mais forte que a vítima com visão pode ter: desenhar pra ler de cima.
- **China: concordo com o 1**, a colmeia. Ressalva: de cima o favo escuro lê como grelha ou poço. Precisa da **emissão em onda** nos tubos,
  que também mostra o progresso da carga sem legenda (a regra "UI sem legenda"). A China já tem muita coisa redonda (as torres do reator, o
  bunker da Gattling): o favo hexagonal tem de dominar.
- **GLA: concordo com o 3**, o tanque de petróleo. Com uma exigência: **as pontas dos SCUDs montados visíveis de cima numa grade 3×3**
  (é o contador da montagem sem legenda, e o inimigo com visão sabe quantos vêm). A imagem é do Gemini Flash, "brinquedo": refazer
  ancorado na raiz, com o pedido escrito como instalação industrial (o filtro do GPT provavelmente reagiu às palavras do pedido).
- **A ordem de fazer:** o estudo põe o prédio primeiro (§12). O número de ogivas muda as tampas e os tubos, a montagem da GLA muda o topo
  do tanque, a arma da China (colmeia ou drone-mãe) muda o prédio inteiro. **Decidir a regra antes da versão final do prédio.**

---

## 7. O que falta no estudo (o que um estúdio AAA poria)

1. **A IA dos dois lados.** A IA que usa (quando armar, onde mirar **só com o que ela vê**, não onisciente) e a IA que reage (sair do anel,
   construir antiaérea contra a China, espalhar a base contra a nuke, atacar o prédio enquanto o relógio corre). Sem a reação, a partida
   IA × IA vai inflar o valor de todas, e a "prova" do estudo mede a IA, não a arma.
2. **Partidas em time e todos contra todos:** o empilhamento (§1). O teste 2×2 na IA × IA.
3. **Fogo amigo.** O ZH bate em aliado e neutro nas duas (`RadiusDamageAffects = ALLIES ENEMIES NEUTRALS`, no SCUD e na nuke). Escrever;
   e o gás que o vento devolve pra quem lançou.
4. **Guarnição e civis.** O gás limpa prédio ocupado (o Anthrax do ZH limpava)? A vila do WorldForge no raio da nuke (tom).
5. **O estado "não armada" no HUD.** A caixa aprovada (`DECISOES.md`) tem o tempo e o vermelho do último minuto; o relógio parado é estado
   novo e precisa de forma (sem legenda). E o botão de armar na grade do prédio, com o preço.
6. **Revelar o prédio ao inimigo** quando fica pronto (todo contra-jogo depende disso) e a tecla de pular pro evento (o espaço do ZH).
7. **O tempo de voo fixo ou pela distância.** USA 9 s, Rússia 14 s e GLA 10 s são fixos; a China (40 m/s) muda com o mapa: 12 s num mapa,
   25 s noutro. Escolher e escrever.
8. **As dependências que não existem:** o vento na simulação, a cratera que muda o caminho, o fogo que queima a vegetação, a grade do gás,
   100 alvos aéreos na simulação com a antiaérea mirando drone, a exceção da fila (cancelar e vender). É o caminho crítico do trabalho,
   mais que o prédio.
9. **Fotossensibilidade e daltonismo** (§5): uma opção "reduzir clarões" e avisos com forma.
10. **A câmera no multiplayer não é tomada** (a pesquisa §6.11, `CONCEITO_EXPLOSOES` E17): escrever no estudo, porque o roteiro da §5.4
    ("o chapéu passa por cima da câmera") supõe a câmera olhando.
11. **A régua de sucesso medida:** na IA × IA (o placar por minuto já existe), a duração da partida com e sem superarma, quem ganha depois
    do primeiro lançamento, e quantas vezes a superarma decide sozinha. E o teste humano: ele com o pai na LAN, que é onde "frustração" se
    mede.
12. **Replay e espectador:** a mira da USA e a escolha dos drones têm de sair iguais no replay (lockstep: a mira ponto a ponto vira comando).
13. **Os poderes do general como contra:** o EMP que o estudo guardou pra China pode **pausar o relógio** de uma superarma inimiga; o "spy
    satellite" da USA vira a visão que a mira precisa. A árvore de patentes está em discussão (`DECISOES.md`, 01/11): é a hora de cruzar.

---

## 8. O veredito: as 5 mudanças, em ordem

1. **USA: a precisão custa visão, e são 4 ogivas, não 6.** Ponto a ponto só onde o jogador vê agora; fora da visão, cada ponto desvia
   ~10 m. 4 ogivas (o Trident convencional real levava 4, e não repete os 6 Tomahawks do general). A vítima vê a área no lançamento e os
   pontos nos 3 s finais. Hoje é a mais forte (a ótima tira 50%, o dobro do "bem mirada"), a mais rápida e a única sem resposta; com a
   regra, 39% com visão e 23% sem.
2. **Fechar os buracos do custo antes de qualquer número.** 1 ogiva por prédio (pronta ou montando); cancelar devolve só o que falta, vender
   ou perder o prédio não devolve (a fila do jogo hoje devolve tudo); a China paga em dinheiro e o −10 vira o consumo fixo do prédio, como o
   ZH faz com todas (sem a armadilha de armar e desligar as próprias Gattling); a GLA monta 9 vagas de 30 s e lançar cedo zera o ciclo;
   cortar "a carcaça vira SCUD"; o prédio com 4 000 de vida (o ZH), pra superarma nenhuma matar superarma numa salva.
3. **Refazer a conta com a régua certa e uma física só.** Medir o caminho crítico e o tempo de recuperação, não o % do valor; média de
   sementes com a dispersão; 3 bases (compacta, espalhada, a da IA de verdade); a mesma reação pra todas; a antiaérea pela arma do
   `Units.json`; a melhor mira buscada. A nuke numa potência só, encostada no chão (cratera, bola e radiação da mesma W), com cada licença
   escrita, e cada número tirado do ZH com o motivo da mudança.
4. **A GLA à altura da nostalgia.** Só o Patriot intercepta balístico (o papel real do PAC-3; o Pantsir, a Gattling e o Stinger não), e o
   SCUD derrubado cai torto com a nuvem em vez de sumir (o 1991 de verdade). O dano volta pra perto do ZH (na mesma conta, o SCUD do ZH tira
   46% na fábrica; a proposta, 23%), com a sujeira real: 1 em 9 se parte, o laranja do ácido nítrico, a dispersão grande.
5. **Desenhar pra câmera que não vê o céu.** USA: a luz das ogivas descendo e as sombras girando até o ponto, não um estouro no ar que a
   câmera não mostra, e no máximo 3 clarões por segundo; China: o enxame em formação de máquina (grade, anel, luzes sincronizadas), não bando
   de pássaro; GLA: o gás como camada pesada de 1–2 m numa grade da simulação, com o vento (que a simulação ainda não tem): o soldado some
   dentro, o tanque fica por cima; Rússia: o cogumelo sai do quadro e some perto da câmera, sem tela cheia de fumaça.

**Fora das 5, mas na mesa:** o nome "Sarmat" não (estratégico de 208 t e símbolo da ameaça de hoje; NUCLEAR MISSILE, o do ZH, resolve);
"Nine Heavens" é o nome do drone-mãe, não da colmeia; o prédio da USA é o 3, não o 1; o prédio revelado ao inimigo quando fica pronto; o
aviso da nuke que encolhe; Ctrl na mira do enxame = só prédios; o teste 2×2 e a IA que reage.

---

## Anexo: como reproduzir

Na pasta `scratchpad/segunda/` (cópia do `docs/superarmas/superarmas_conta.py` como `conta.py`):
- `variacoes.py`: o ruído das sementes da GLA, a GLA com os números do ZH, a busca da melhor mira da USA, a China sem unidades e num
  exército de campo, a base espalhada, o reator que derrete;
- `variacoes2.py`: a USA sem visão (desvio) e com 4 ogivas, o SCUD interceptado caindo torto, a antiaérea pela taxa da arma contra o enxame;
- `variacoes3.py`: 4 ogivas de 1 300 com e sem visão, a melhor mira da Rússia na base compacta e na espalhada;
- `tabela.py`: a tabela do §3.2 em dólares.

Rodar com `python -I -c "import sys; sys.path.insert(0,'.'); exec(open('tabela.py',encoding='utf-8').read())"` de dentro da pasta. A busca
da mira é gulosa com reinícios (não é ótimo garantido; o ótimo real é igual ou maior, o que só reforça o ponto).
