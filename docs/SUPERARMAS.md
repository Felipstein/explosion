# As superarmas (estudo, 01/11/2026)

O pedido do Felipe (01/11, à noite): a primeira tarefa é a superarma de cada facção, "a melhor arma do jogo, o triunfo do RTS
clássico". Estudar muito; pensar o que cada facção tem, o custo de construir, o tempo, o poder de destruição e **o custo pra
usar** (no ZH ela é grátis: só espera o relógio; ele quer um custo de lançamento, talvez configurável depois, que pode variar
por facção). Tudo pensado, "desde a ideia que arrepia até o efeito sonoro final da explosão e o efeito visual". As ideias dele
de partida, sem me prender a elas: a GLA com os SCUDs do ZH; a USA com um míssil que estoura alto sobre a base inimiga, com
luz forte e um som tenebroso, e solta 5 ou 6 mísseis menores; a Rússia com a ogiva nuclear; a China em aberto. Ele quer as
discussões na mesa, inclusive como devem ser as construções. Depois: discutir, modelar, preparar os efeitos e os sons (o som
por último).

**Isto é estudo e proposta.** Nada foi aprovado. Os números são a faixa inicial, saídos da conta (§8) e do ZH; a prova é a
partida IA × IA e a cena no laboratório.

**As imagens** (referência de estudo do Atelier, não são o jogo):
- **2ª rodada (02/11), a atual:**
  - os prédios icônicos, 4 desenhos por facção: `docs/concept/superarmas/predios_v2_iconicos.jpg` (`captures/concept/111_` a `114_`);
  - a chegada fiel à câmera do jogo, GPT × Gemini Pro: `docs/concept/superarmas/momentos_v2_gpt_x_gemini.jpg` (`115_` a `117_`);
  - as 3 ideias novas da China, o momento: `docs/concept/superarmas/china_ideias_momento_gpt.jpg` (`108_` a `110_`).
- 1ª rodada (01/11), **reprovada** (genérica; os momentos "muito artísticos"): `predios_4_faccoes.jpg` e `momentos_4_faccoes.jpg`.

**A minha leitura dos momentos da 2ª rodada:** o Gemini Pro fica fiel à cena do jogo (ele parte da imagem aprovada e mexe só no
efeito), mas erra coisas: na USA, as ondas de choque saíram como bolhas de vidro; na GLA, os SCUDs parecem foguetes de brinquedo
(o gás em camada baixa, com os tanques por cima e os soldados dentro, ficou certo). O GPT refaz a cena e põe mais volume e mais
drama: a poeira da USA e o anel da nuke são os melhores, mas a cor e o exagero ainda são de cartaz.

**As pesquisas completas** (com as fontes): `docs/superarmas/ESTUDO_RTS.md` (os jogos e os números do ZH) e
`docs/superarmas/ESTUDO_REAL_FX_SOM.md` (as armas reais, a física de cada explosão, as referências de efeito e de som).

---

## O estado final (03/11): leia isto primeiro

**O que vale hoje está aqui.** O resto do documento é o caminho até chegar aqui: as rodadas, as reprovações e o raciocínio de cada
uma. Algumas tabelas lá embaixo são da proposta antiga (a §0 ainda mostra a China "em aberto"). Os aprovados com a imagem e a frase dele
estão em `docs/APROVADOS.md` §Superarmas. O prompt inteiro de cada imagem, com os anexos e o modelo, está em
`docs/superarmas/RECEITAS_DAS_IMAGENS.md`.

**As regras (as quatro):**
- **O custo de lançar: armar a ogiva.** O prédio dá o direito; cada carga é paga, e o relógio só corre pago.
  - O relógio é público.
  - Uma ogiva por prédio.
  - Cancelar devolve só a parte que falta.
  - Vender ou perder o prédio não devolve nada.
  - Sem energia, o relógio para.
- **O prédio:** $5 000, 60 s, 4 000 de vida (o ZH). Aparece pro inimigo quando fica pronto.
- **Na sala:** UNLIMITED / LIMITED / OFF. O LIMITED é 1 superarma por jogador, de qualquer tipo (o "Limit Superweapons" do ZH).
- **Cada superarma é um tiro só**, menos a da GLA.
- **Só o Patriot intercepta SCUD.**

| | USA | Rússia | China | GLA |
|---|---|---|---|---|
| **A arma** | **Global Strike**: um míssil só, que volta do espaço e se parte em 5 pedaços; cada pedaço (2 000) derruba uma fábrica | **Nuclear Missile**: uma ogiva nuclear tática | **O enxame de drones**: ~70 drones que chegam disparando mísseis meio pesados e no fim se jogam como kamikaze | **SCUD Storm**: 9 SCUDs |
| **Armar** | $3 000, 5:00 | $3 500, 6:00 | $2 000, 5:00 | $200 por SCUD (9 vagas de 30 s) |
| **O prédio aprovado** | a arena afundada com o míssil de pé correndo no rasgo e a torre do satélite grande; a casa de montagem vira o bunker baixo da 6ª rodada (paredes inclinadas, porta redonda, faixa azul, sem laje embaixo); fechada e aberta | o salão de painel pré-moldado com o telhado-serra que corre; aberto, o míssil sobe **na diagonal** no braço amarelo; fechado e aberto são o mesmo modelo | a colmeia redonda de favo (a base do GPT com o topo do Gemini: as torrezinhas e o brilho) | **a coroa**: 9 trilhos presos no núcleo; cada SCUD montado é levado pro centro e sobe até ficar de pé; o último fecha a coroa |
| **O disparo aprovado** | **a frio**: catapultado pra cima, quase para no ar e aí o foguete acende, a aceleração começando do zero (a imagem do Gemini é o segundo seguinte) | **a quente dentro do salão**: o fogo desce no fosso e sai pelos dois lados (o Gemini: "quero exatamente assim no jogo") | a colmeia esvaziando em cascata, os drones subindo em grade (o GPT) | a cascata dos 9 (o GPT e o Gemini) |
| **A chegada** | aceita (a 3ª rodada) | falta pelo método novo | aprovada (o GPT), mais a ficha do drone | falta pelo método novo |

As imagens aprovadas estão em `docs/aprovados/superarmas/`, e as folhas de cada rodada em `docs/concept/superarmas/`.

**O método que deu certo** (e o que deu errado), pra quem for refazer ou continuar:
1. **A ideia combinada em palavras antes de gastar imagem.** Uma facção por vez, uma imagem por vez.
2. **O prédio nasce na família dos aprovados.**
   - Anexar a folha dos prédios daquela facção já aprovados no jogo (`captures/concept/_attach/superarma_<facção>/`; a cópia está em
     `docs/superarmas/RECEITAS_DAS_IMAGENS_anexos/`) e pedir o prédio novo no meio deles.
   - **Deu errado:** 4 folhas em paralelo, "icônico" levado ao absurdo (o navio da GLA, a montanha de sucata) e o morro de areia da
     Rússia.
3. **Ajuste é edição da escolhida, mudando só o pedido.**
   - O outro estado (fechado ou aberto) sai da mesma imagem editada.
   - Conferir os dois lado a lado antes de mostrar (o mesmo número de portões, os mesmos contrafortes).
   - **Deu errado:** refazer do zero, que mudou o prédio inteiro; anexar a imagem B pra pegar um detalhe, que copiou a B inteira.
4. **O momento de gameplay é um quadro novo.**
   - 2 ou 3 game shots aprovados anexados só como referência do jogo, e o pedido descreve uma cena nova.
   - **Deu errado:** anexar só a cena r1, que deixou a imagem "enviesada" e "muito artística".
5. **Os modelos:**
   - o Gemini Pro é mais fiel ao jogo e à imagem anexada;
   - o GPT põe mais volume e drama, mas recusou a GLA duas vezes por "violência" (resolvido no Gemini, com "desert scavenger faction").
   - Pedido com texto longo em lote dá ENOENT no Windows: um pedido por vez.
6. **O detalhe é de jogo, não de filme:** superfícies grandes e limpas, poucos painéis e cabos. Nada de laje embaixo de tudo ("a base
   acompanha a forma").

**O que falta** (a ordem da §12): a chegada da Rússia e da GLA pelo método novo; as regras na simulação; os modelos 3D com os estados;
os efeitos no laboratório; o HUD; o som por último.

---

## 0. A proposta numa página

**Revisada duas vezes:** pela segunda opinião (§10; o texto inteiro em `docs/superarmas/SEGUNDA_OPINIAO.md`) e pelas decisões dele
de 02/11 (a USA com 5 ogivas; a precisão pela visão reprovada; o enxame da China reprovado: §6 tem as ideias novas). As seções 4 a 7
contam o raciocínio; onde algo mudou, está escrito ali.

| | USA | Rússia | China | GLA |
|---|---|---|---|---|
| **A arma** | **Global Strike**: um tiro só: o míssil que sai da casa pelos trilhos, volta do espaço e **se parte em 5 pedaços**, cada um derruba uma fábrica (decisões dele, 02/11) | **Nuclear Missile**: a ogiva nuclear tática, uma só (~5 t) | **em aberto** (o enxame foi reprovado): **Fire Dragon** (recomendado), o canhão eletromagnético ou a barragem rolante (§6) | **SCUD Storm**: **9 SCUDs** com ogiva tóxica, o do ZH |
| **A frase da facção** | vê tudo, acerta primeiro | cava e queima | arrasa em massa, queima a área | vive da ruína |
| **O jeito** | **a única que se mira ponto a ponto**, exata em qualquer lugar do mapa | a maior e mais lenta: tudo ou nada | (§6) | a suja: imprecisa, se parte, e o veneno fica |
| **A resposta da vítima** | ver a área no lançamento e os 5 pontos nos 3 s finais (as unidades desviam); espalhar a base; cortar a energia que arma | fugir: a elipse da previsão desde o lançamento, o anel exato nos ~5 s finais; espalhar a base | (§6) | **só o Patriot** derruba SCUD (o papel real dele), e o SCUD derrubado cai torto, com o gás |
| **O custo de lançar** (armar a ogiva) | **$3 000** | **$3 500** | **$2 000** | **$200 por SCUD** (9 vagas de 30 s) |
| **A carga** | 5:00 | 6:00 | 5:00 | 4:30 |
| **O prédio** | $5 000, 60 s, **4 000 de vida**, −10 de energia | idem | idem | $5 000, 60 s, 4 000, sem energia |
| **A cicatriz no mapa** | 5 crateras no padrão escolhido, prédios caindo do ponto do impacto | o chão queimado, a ruína, a zona de radiação (e a cratera grande, se for "a que cava", §5.5) | (§6) | a mancha tóxica e o campo de carcaças pro Worker |
| **O prédio** (a 1ª folha foi reprovada: genérica) | **a casa, os trilhos e o satélite grande** (a direção dele): o míssil sai da casa pelos trilhos quando fica pronto, e o satélite gigante mostra que ele acha o alvo | a 2ª folha: a Tulipa, o Monumento, o Pica-pau, a Concha (§5.3) | a 2ª folha, uma por arma (§6) | a 2ª folha: o Navio, a Montanha de Sucata, a Cúpula do Bazar, a Torre de Petróleo (§7.3) |

**A ideia que amarra tudo: cada superarma deixa no mapa a cicatriz da sua facção.** É o pilar do jogo (`docs/VISAO_DO_JOGO.md`,
"o campo que muda") no seu ponto máximo. A USA deixa marcas precisas; a Rússia muda o relevo; a China queima a área; a GLA deixa
veneno e sucata. Quem olha o mapa no fim da partida sabe quem bateu onde.

**A regra do custo, igual pras quatro: o relógio só corre pago.** Construir o prédio dá o direito; cada carga tem de ser paga
(armar a ogiva). Sem pagar, o relógio fica parado e o inimigo vê que não há ameaça. Pago, o relógio corre aos olhos de todos,
como no ZH. Cada facção paga numa moeda que conta a história dela (§3.2).

---

## 1. A régua (o que tem de ser verdade no fim)

1. **Arrepia nas três escalas:** a do dono (o lançamento na base dele), a da vítima (o aviso, a espera, a chegada) e a do mapa
   inteiro (todo mundo ouve que "deu merda" em algum lugar, abafado e sem posição: decisão de 07/10, `docs/CONCEITO_SOM.md`).
2. **Vira a partida, mas não acaba sozinha.** Uma salva bem mirada numa base sem defesa tira **20–35% do valor da base** (ou os
   prédios críticos equivalentes); a nuke, a mais lenta e cara, mais. Uma salva sozinha não mata o Command Center nem outra
   superarma (o prédio tem 4 000 de vida, como no ZH).
3. **Toda superarma tem resposta**, e a resposta também é espetáculo: fugir do anel, derrubar o SCUD no céu, a antiaérea
   rasgando o enxame, cortar a energia que arma o silo, destruir o prédio enquanto ele carrega.
4. **Lê de cima** (jogabilidade antes do realismo): a área do dano se vê no chão; a fumaça sai do nível das unidades em
   segundos; o gás fica baixo e ralo o bastante pra ver as unidades; nada cobre a batalha por mais que o necessário.
5. **Toda fumaça, fogo e poeira é volumétrica e vive na luz do mundo** (as quatro regras do `CLAUDE.md`): o cogumelo projeta a
   sombra dele na base; o clarão acende a fumaça por dentro.
6. **Física decidindo:** a explosão nasce do fenômeno real na escala certa (`docs/superarmas/ESTUDO_REAL_FX_SOM.md`), não de
   uma constante chutada.

---

## 2. O que os RTS ensinam

O estudo inteiro, com as fontes: `docs/superarmas/ESTUDO_RTS.md`. O resumo:

**O ZH 1.04, nos arquivos de dados** (`Obj_FactionBuilding.ini`, `Weapon.ini`, `SpecialPower.ini`, e o código-fonte publicado;
conferido pela segunda opinião; 1 unidade do ZH = 0,2633 m):

| | Particle Uplink Cannon (USA) | Nuclear Missile (China) | SCUD Storm (GLA) |
|---|---|---|---|
| Prédio | $5 000 (metade do dinheiro inicial), 60 s, 4 000 de vida, blindagem dura (80% contra explosão), −10 de energia | idem, −10 | idem, sem energia |
| Relógio | 4:00 | 6:00 | 5:00 |
| Disparar | grátis; o relógio começa quando o prédio fica pronto | | |
| O golpe | **o feixe que o jogador humano dirige** (`PUC.cpp`): 100 de dano a cada 0,25 s por 10 s (4 000), num raio de 11,6 m; prédio comum leva 200% (a fábrica morre em 2,5 s), o CC 25% | 3 500 até 16 m, caindo a 300 aos 55 m; radiação por 30 s em 53 m; mata a fábrica até ~33 m; **o CC fica com 44%** | 9 SCUDs com ~25 m de dispersão; cada um 500 até 13 m e **150 até 53 m**, e o veneno por 45 s em 37 m |
| Fogo amigo | — | sim (aliado, inimigo e neutro) | sim |
| O aviso | a voz de "detectada", "pronta" e "lançada" pra todos; o prédio visível pra todos; a sirene no alvo da nuke; **o círculo da mira só o dono vê** | | |
| A resposta | sem energia, EMP ou subjugado: o relógio pausa sem zerar; destruir ou capturar o prédio; **não existe interceptação** | | |
| Na sala | a opção "Limit Superweapons" (`GameInfo.cpp`): **1 por jogador**, somando os tipos | | |

Os generais do ZH mexem nisso: o Superweapon General tem o canhão a $2 500 e a nuke com 4 min; o Nuke General, a nuke a $4 500; o
Toxin General, o SCUD com veneno mais forte (o Anthrax Gamma dá 550).

**Os outros jogos** (C&C clássicos, C&C3, RA3, Supreme Commander, Beyond All Reason, StarCraft II, World in Conflict, Company of
Heroes 3, Halo Wars 2, Act of War e Act of Aggression, Tempest Rising, Rise of Nations, Dune: Spice Wars e mais; a tabela no
`ESTUDO_RTS.md` §2.3). **Os padrões de custo de lançar:** (a) só o relógio (a série C&C); (b) munição fabricada e estocada (SupCom,
BAR, SC2); (c) pontos ou recurso por uso (WiC, CoH3, Halo Wars); (d) os híbridos, desbloqueio + custo por uso + recarga, **o padrão
dos jogos recentes**; e os diferentes: o custo que sobe a cada uso (Rise of Nations), o custo político (o prestígio do Dune, o
relógio do Armagedom do RoN), o lançador exposto que tem de sobreviver (o Ghost do SC2), a energia por disparo.

**O que faz a superarma ser amada:**
1. **O aviso em três tempos, público:** o prédio revelado, o relógio de todos, o alerta no disparo com tempo de reação (2,5 a 16 s).
   É o "momento EVA" da série. A superarma sem aviso (o Psychic Dominator do RA2) é a mais odiada.
2. **A resposta existe durante a carga**, não só depois: cortar a energia pausa, destruir o prédio revelado, o antimíssil.
3. **Destrói uma camada da base, não a partida:** 3 a 6 prédios comuns e o que estiver na área; **o coração da base fica com 40–60%**
   (o CC do ZH, 44%; o do RA3, ~50%).
4. **Mirar é uma decisão** (a economia, a produção ou o exército parado); o feixe dirigido do ZH é a habilidade no disparo.
5. **O resíduo nega terreno de forma legível** (a radiação de 30 s, o veneno de 45 s).
6. **A identidade da facção**, inclusive no jeito de pagar.

**O que faz ser odiada:** várias juntas disparando (o motivo do "Limit Superweapons"); **grátis pra sempre** (renda de destruição
infinita que premia quem se esconde: a realimentação positiva de Marc LeBlanc, GDC 1999); decidir a partida numa salva (as versões
de multiplayer são enfraquecidas: a nuke do C&C original de 1 000 pra 200, o RA3 cortou o preço à metade); a resposta "só construir
defesa"; e **o efeito que cobre a leitura da batalha** (pra nós, defeito).

**As faixas que funcionam** (e onde a proposta cai): carga de 4 a 7 min (a nossa: 4:30 a 6:00); construir por 25–50% do dinheiro
inicial (a nossa: $5 000 de 10 000); **custo por disparo de 30–60% do prédio, ou 15–35% do dinheiro inicial** (a nossa: $2 000 a
$3 500, de 40 a 70% do prédio e 20 a 35% do dinheiro inicial: dentro); aviso de 2,5 a 16 s (a nossa: 3 a 14 s); resíduo de 30–45 s
(a nossa: 40–60 s); 1 por jogador no LIMITED (a nossa: igual).

**O que o ZH ensina e o nosso muda:** o ZH é aditivo e verde-branco onde o nosso é volume na luz do mundo; o ZH não intercepta nada
e o nosso deixa o Patriot derrubar SCUD; no ZH o círculo da mira só o dono vê e no nosso a vítima vê a área (a elipse da nuke que
encolhe, os pontos da USA nos 3 s finais), porque o "toda superarma tem resposta" pede.

---

## 3. As regras do nosso

### 3.1 O caminho de toda superarma (a cerimônia)

| Passo | O dono | A vítima | O mapa inteiro |
|---|---|---|---|
| **Construída** | o botão aparece na coluna de poderes (já aprovado: quadrado, cresce de baixo pra cima) | aviso: "*[nome]* detected" | a caixa da superarma aparece embaixo do dono no topo (já aprovada), com o relógio parado |
| **Armada** (pagou) | o prédio começa a "trabalhar" (vapor, luzes, a colmeia acendendo) | o relógio dele começa a correr no topo | idem |
| **Pronta** | READY respirando no botão (aprovado); a voz: "*[nome]* ready" | a linha fica vermelha e brilha com menos de 1 min (aprovado) | idem |
| **Mira** | o cursor da área no chão (a marcação do poder aprovada, a elipse) com a prévia própria de cada arma (§4–7) | — | — |
| **Lançamento** | o espetáculo na base dele; a voz: "launch confirmed" | a voz: "*[nome]* launched!"; o alarme da base | o ronco distante, abafado |
| **Voo** | — | **o aviso no chão** (o anel ou os pontos), o tempo de cada arma | a seta de evento fora da tela (aprovada) pra quem é atingido |
| **Chegada** | — | o momento (§4–7) | o estrondo atrasado pela distância (d/343 s), abafado |
| **Cicatriz** | — | o mapa mudou | o mapa mudou |

### 3.2 O custo de lançar (o que o Felipe pediu)

**O modelo proposto: armar a ogiva.** O prédio é a plataforma; a ogiva é comprada no próprio prédio, como uma unidade na fila
(o botão na grade, o preço, o carregamento do C&C). Pagar começa o relógio. O relógio É a montagem. Depois de pronta, lança na
hora que quiser. Destruíram o prédio no meio: perdeu o que pagou (o risco de armar cedo).

Por que este e não os outros (o estudo dos padrões em `ESTUDO_RTS.md`):
- **Só o relógio (ZH):** é o que ele não quer: a superarma vira despertador, dispara de graça a cada 5 min.
- **Pagar na hora de lançar:** simples, mas tira a tensão do relógio: o inimigo não sabe se você vai ter o dinheiro.
- **Munição fabricada (Supreme Commander, Beyond All Reason, StarCraft II):** é o nosso, com uma diferença: o relógio que todo
  mundo vê continua (a nostalgia do ZH e o topo aprovado). O pagamento vira informação: relógio parado = sem ameaça.

**A regra inteira** (revisada com a segunda opinião, que achou quatro buracos):
1. **Uma ogiva por prédio**, pronta ou montando. Com uma pronta, a próxima não começa: guardar pronta tem custo natural (o relógio
   da próxima não corre e o prédio pode cair com ela dentro).
2. **Cancelar devolve só a parte do relógio que falta; vender ou perder o prédio não devolve nada.** Isto é uma exceção à fila do
   jogo, que hoje devolve tudo (`docs/ECONOMIA.md`): sem a exceção, o risco de armar cedo some.
3. **O prédio é revelado ao inimigo quando fica pronto** (o aviso "detected" com a posição no minimapa): todo contra-jogo depende
   de saber onde ele está.
4. **Sem energia o relógio para** (USA, Rússia, China: o prédio consome −10 sempre, como no ZH).
5. **A GLA monta 9 vagas de 30 s** (cada uma só corre paga, $200); pode lançar cedo com o que tem (no mínimo 4), e isso **zera o
   ciclo inteiro**. A decisão "espero os 9 ou solto 5 agora?" fica, sem virar uma artilharia a cada 90 s.

**O preço de cada facção:** USA $3 000, Rússia $3 500 (e a carga mais longa), China $2 000 (a arma ainda em aberto), GLA 9 × $200. Pelo estudo dos RTS
(§2), cai na faixa que funciona (30–60% do prédio, 15–35% do dinheiro inicial).

**As outras moedas que o estudo achou** (na mesa, pra ele): **o custo que sobe a cada uso** (Rise of Nations: a primeira é
acessível, a quinta dói; freia o abuso sem proibir); **o lançador exposto** (o Ghost do StarCraft II tem de sobreviver: a GLA com os
caminhões que saem do túnel caberia aqui); **o custo político ou do território** (o prestígio do Dune; pra Rússia, consumir pontos de
comando). Eu fiquei com o dinheiro pras quatro porque o jogo ainda não tem esses sistemas e o relógio pago já dá o anúncio e o
risco; **o custo que sobe** é o melhor candidato a somar, se o fim de partida com dinheiro sobrando incomodar no teste.

**O que mudou da 1ª versão e por quê:**
- **A China pagava em energia** (10 presos enquanto carrega). A segunda opinião mostrou o laço: armar derruba a energia, desliga
  as Gattling da própria base e para o próprio relógio. E o ZH já cobra −10 de energia de toda superarma. Agora ela paga em
  dinheiro como as outras; "matar os reatores atrasa a superarma" continua valendo pras três facções com energia.
- **"Cada carcaça monta 1 SCUD de graça" saiu:** é o mesmo dinheiro do Worker contado duas vezes, e bola de neve.

**O custo, com honestidade:** com a renda do meio da partida (~$3 000/min), armar é 10–20% do que entra durante a carga, e uma
salva devolve 3 a 6 vezes o que custa. Ninguém deixa de armar, a não ser quem está quebrado. **A decisão real é quando armar
(anunciar ao inimigo), não se.** O relógio parado vira informação ("acabou de construir" ou "está sem dinheiro"). E o custo premia
quem está na frente (o rico arma, o pobre não): é o contrário de mecânica de virada. Aceitável, mas é escolha consciente.

**Na sala:** SUPERWEAPONS: UNLIMITED / LIMITED / OFF (já aprovado na tela do skirmish). **O LIMITED é o "Limit Superweapons" do
próprio ZH: um prédio de superarma por jogador** (nostalgia). Depois, se ele quiser: LAUNCH COST: STANDARD / CLASSIC (o clássico é
o ZH, grátis, só o relógio).

### 3.3 O que destrava

O prédio de tecnologia de cada facção (Strategy Center, Instituto, Propaganda Center, Palace), como no ZH. Sem patente: a
superarma é do prédio; os poderes do general são outra coisa (o Cruise Missile Strike da USA é poder do general, §4.6).
Construir custa **$5 000 e 60 s** nas quatro, com **4 000 de vida** (os números do ZH: com 4 000, nenhuma superarma mata outra
numa salva só; com os 2 500 da 1ª versão, a USA ganhava toda corrida contra a Rússia: disparava primeiro e matava o silo armado).

---

## 4. USA: Global Strike

### 4.1 A ideia

A dele, levada pro real: **o Prompt Global Strike**, o programa americano de atacar qualquer ponto do planeta em menos de uma
hora com arma convencional. Um míssil balístico sobe da base, sai da tela, volta do espaço sobre a base inimiga, o veículo de
reentrada estoura alto com um clarão e um estrondo de cima, e **as ogivas incandescentes descem como meteoros**, cada uma com o
risco de plasma laranja e a linha branca de fumaça (as fotos de Kwajalein, os testes de reentrada). Cada uma explode grande, em
volume.

**O que a faz da USA: é a única que se mira ponto a ponto.** O jogador põe as 5 ogivas (padrões prontos: linha, anel, leque; e
arrastar pra girar e esticar) dentro de um círculo de 60 m, no mínimo 12 m uma da outra, e cada uma cai exatamente onde ele pôs.
A vítima vê a área no lançamento e os 5 pontos exatos nos 3 s finais (as unidades desviam; os prédios, não).

**Um tiro só, que se parte em 5** (decisões dele, 02/11). A primeira: "a USA podia ser umas 5 ogivas, reduzindo o dano de cada um
pra balancear". Fiz 5 de 1 000 e ele reprovou: "acho fraco, pq a ogiva é uma só, e ela que se quebra, então ela é 1 único tiro; o
único aqui por enquanto que é mais de 1 é do GLA". **A precisão pela visão também foi reprovada** ("nerfa muito as superweapons"):
a mira é exata no mapa inteiro. Então **cada pedaço tem força de superarma: um pedaço derruba uma fábrica sozinho** (§4.5), e o
equilíbrio vem do preço e da carga, como a nuke. A USA e a Rússia ficam no mesmo patamar: a Rússia arrasa uma área, a USA escolhe
5 alvos.

### 4.2 As alternativas (na mesa)

| Ideia | A favor | Contra | Veredito |
|---|---|---|---|
| **A: Global Strike, 4 a 6 ogivas de reentrada** (a dele, no real) | vertical, do espaço, o risco de luz é inesquecível; real (o programa existiu); precisão = a cara da USA | parece o Cruise Missile Strike se mal feita (§4.6) | **recomendada** |
| B: Tomahawk gigante com submunições (TLAM-D) | real (soltava 166 bombinhas) | é o Tomahawk que já temos, maior; a chuva de bombinhas lê como fogo de artifício | não |
| C: Orbital Lance / "Rods from God" (varas de tungstênio do satélite) | o Particle Cannon do ZH de hoje; o prato da folha 4 é lindo | ficção (nunca existiu); o visual da chegada é o mesmo da A | guardar o prato: pode ser o poder máximo do general |
| D: B-2 com a bomba gigante (MOAB/MOP) | a sombra do avião passando, o paraquedas da bomba | é poder de general no ZH (o bombardeiro); pode ser derrubado = frágil | não |
| E: Particle Cannon do ZH | nostalgia pura | ficção científica: fora do tom (visão do jogo) | não |

### 4.3 O prédio (Global Strike Facility)

**3ª rodada (02/11), a atual, pelo caminho certo** (a regra de 11/10, `memory/feedback-construcoes-pela-cena`: o prédio nasce dentro
da cena da base da facção, com os aprovados anexados, uma facção de cada vez): `docs/concept/superarmas/usa_v3_base_e_chegada.jpg`
(`captures/concept/118_` e `119_`). O prédio de superarma na mesma família da War Factory aprovada (os mesmos painéis, as faixas
azuis, o guindaste e o pátio zebrado): um prédio só, compacto, com o prato grande no teto, as portas abrindo pros trilhos e o
míssil rolando até o pátio redondo. A chegada saiu num quadro novo (os game shots só como referência de câmera e luz), numa base
chinesa.

**4ª rodada (02/11)**, depois do que ele disse da 3ª ("quase; o GPT melhor"; o míssil não sai deitado: vai pro centro já de pé,
com só a metade de cima aparecendo, correndo num trilho dentro de um rasgo no chão; e ainda faltava a cara de superarma):
`docs/concept/superarmas/usa_v4_missil_no_rasgo.jpg` (`captures/concept/120_`, GPT, 2 variações). A arena redonda de lançamento
afundada no chão, com os muros de proteção inclinados, as faixas zebradas e as luzes vermelhas; o prédio de montagem com as portas
blindadas; o rasgo no chão do prédio até o centro; o prato numa torre alta. **Minha leitura:** a arena dá a cara de superarma que
faltava; a variação 1 mostra o míssil de pé no rasgo saindo do prédio (a ideia dele), mas pequeno; o prato ainda não é o maior
objeto da base.

**O rumo da arena foi aprovado** ("agora sim, tá ficando com cara de bagulho FODA mesmo, de um SUPERWEAPON"), com dois pedidos:
mais alternativas, e menos detalhe ("às vezes tem muito detalhe, que parece coisa de filme e não de um jogo").

**5ª rodada (02/11):** `docs/concept/superarmas/usa_v5_arena_alternativas.jpg` (`captures/concept/121_`, as cópias curtas `alt_A/B/C.jpg`),
3 arranjos com o pedido de detalhe de jogo (superfícies grandes e limpas, poucos painéis, quase nada de cabo, cano e caixote):
- **A:** o prato na beira da arena, num pilar inclinado; o rasgo corta a arena ao meio e o míssil sai de pé das portas do prédio;
- **B:** o prato no teto do prédio de montagem (o prato mais largo das três); o rasgo leva o míssil até o centro;
- **C:** a arena com o muro alto e uma brecha só, o rasgo entrando por ela, o prato atrás (a mais limpa).

**Minha leitura:** o detalhe caiu pro nível de jogo nas três. A **A** é a que mais mostra a ideia dele (o míssil de pé saindo do prédio
pelo rasgo); a **B** é a que mais se aproxima do "satelitezão". Juntar as duas: o arranjo da A com o prato do tamanho da B.

**Ele escolheu a 4ª rodada (a variação 1)** e reprovou as 3 da 5ª ("simples, perdeu a cara de superweapon"); pediu pra simplificar a
casa de montagem ("muito detalhado, e muito parecido com a War Factory"). A escolhida: `docs/aprovados/superarmas/2026-11-02_usa_arena_4a_rodada_v1_ESCOLHIDA.jpg`.

**6ª rodada (02/11):** `docs/concept/superarmas/usa_v6_casa_simplificada.jpg` (`captures/concept/123_`, Gemini Pro: o GPT do Atelier
respondeu "sem créditos" na OpenAI). A casa virou um bunker baixo de paredes inclinadas com uma porta redonda e uma faixa azul: bem
diferente da War Factory e simples. **Mas a edição passou do ponto:** o conjunto inteiro ficou liso e limpo demais (perdeu o desgaste
e as faixas zebradas da arena) e apareceu **uma laje clara embaixo de tudo**, o que ele já reprovou ("base acompanha a forma"). 

**A versão fechada (02/11, pedido dele depois da Rússia):** `docs/concept/superarmas/usa_v7_fechada_e_aberta.jpg`
(`captures/concept/138_`, Gemini Pro, editada da 4ª escolhida): o poço tampado por uma tampa redonda blindada em gomos com faixas
zebradas, o rasgo coberto por escotilhas, as portas da casa fechadas, sem vapor, as luzes fracas. Conferida contra a aberta: o mesmo
prédio, a mesma arena, a mesma torre e os mesmos objetos. O prato não foi pra posição de descanso (ficou como na aberta): no modelo ele
gira.

**DECIDIDO (02/11): a referência visual é a 4ª rodada, variação 1; acabou a geração de imagem pra este prédio** ("fica com o que a
gente tem visualmente ali mesmo"). No modelo: a arena, o rasgo com o míssil de pé, a torre do prato (maior que nas imagens: o
"satelitezão") como na 4ª; a casa de montagem como o bunker baixo da 6ª (paredes inclinadas, porta redonda, uma faixa azul), com o
desgaste e as faixas zebradas da 4ª e sem laje embaixo de tudo.

As rodadas anteriores (reprovadas) ficam abaixo pra ninguém repetir.

**A 1ª folha foi reprovada (genérica). A direção dele (02/11): "a ogiva sair de dentro de uma casa, andando pelos trilhos pelo chão
quando tá pronta", numa ideia menos padronizada, e "um satelitezão pra dar a entender que ele vai localizar onde cair".**

A 2ª folha (`predios_v2_iconicos.jpg`, em cima à esquerda; `captures/concept/111_`), os mesmos três elementos em 4 arranjos:
1. o hangar arqueado, o míssil saindo pelos trilhos até o pátio redondo, o prato ao lado;
2. **o pátio redondo de anéis concêntricos** com o míssil de pé, os trilhos chegando em curva, o prato numa torre de treliça;
3. a casa enterrada no morro, o prato no alto do morro, o míssil subindo no fosso queimado;
4. as duas torres de serviço no pátio e o prato grande no meio de um anel de pratos menores.

**Minha leitura:** o 2 é o mais forte de cima (os anéis do pátio, o círculo dentro do círculo) e o 1 tem o ritual dele (o míssil
saindo da casa pelos trilhos). Juntar os dois. O que ainda falta: **o prato tem de ser o maior objeto da base** (aqui ele tem o
tamanho do hangar; ele pediu um "satelitezão"), e **o míssil lê como foguete espacial** (branco, de aletas): tem de ler como arma.

O que estava antes (a 1ª folha, reprovada):
- **Recomendado: o 3** (revisão; eu tinha posto o 1): o caminhão lançador com os dois tubos brancos erguidos sobre o pátio redondo,
  saindo do abrigo de concreto na encosta de terra (o Dark Eagle de verdade). De cima, **o retângulo comprido que vira um ponto
  quando os tubos ficam de pé**: o movimento mais legível das quatro folhas. O pátio é redondo. (Os 2 tubos e as 5 ogivas: um míssil
  só, o segundo tubo é a reserva da próxima carga; ou 1 tubo grande. Fica pra versão final.)
- **Por que não o 1** (o convés com a grade de 8 tampas e o radar de faces): tem **a laje retangular com faixa zebrada embaixo de
  tudo**, o que ele reprovou na galeria de 01/11 ("nada de laje quadrada embaixo de tudo"); 8 tampas pra um míssil leem "8
  mísseis"; de cima a grade lê como estacionamento; e o Aegis Ashore de verdade é defesa antimíssil. Se ele gostar do 1: uma tampa
  grande só, redonda, sem a laje.
- O 2 virou um hangar com porta (de cima não se vê o míssil); o 4 é o Orbital Lance (alternativa C).
- A vida: o caminhão entra e sai do abrigo; armando, os tubos sobem devagar; pronta, a faixa azul acende no pátio; o vapor e as
  luzes de serviço.

### 4.4 A sequência (o roteiro do arrepio)

**A referência do disparo (03/11, pedido dele: "o momento exato em que a superarma foi disparada, só ela saindo já arrepiando"):**
`docs/concept/superarmas/usa_disparo.jpg` (`captures/concept/151_`, GPT e Gemini, em cima da arena aprovada). O raciocínio, pela física
de um lançamento a quente de silo: no 1º segundo o míssil sai devagar do poço, numa coluna de fogo branco que ofusca; o escape não tem
por onde ir, explode pra fora do poço e escorre por cima dos muros de proteção num anel grosso de fumaça e areia que rola pelo chão
(como nas decolagens de foguete); a chama acende tudo de baixo; o prato travado no alvo; as luzes vermelhas piscando. **Minha leitura:**
o GPT está mais no visual do jogo (o anel de fumaça rolando por cima dos muros e envolvendo os tanques); o Gemini tem a chama mais forte
e as luzes vermelhas acesas dentro da fumaça, mas lavou as cores.

**APROVADO (03/11): o do Gemini** (`docs/aprovados/superarmas/2026-11-03_usa_disparo_GEMINI_APROVADO.jpg`). **O jeito de lançar,
regra dele:** lançamento a frio. O míssil é **catapultado pra cima** pelo gás, sobe sem chama perdendo velocidade e, **quando a gravidade
quase vence e ele quase para no ar, o foguete acende**: o clarão, o rugido, a fumaça em volume (simulada, na luz do mundo) e a subida com
a aceleração começando do zero, devagar e crescendo. Esse instante parado no ar antes de acender é o arrepio (o som some e volta com o
rugido). A imagem aprovada é o segundo seguinte, a coluna de fogo já acesa.

| Tempo | O que se vê | O que se ouve |
|---|---|---|
| L+0 | na base do dono, os tubos já erguidos; a tampa do tubo salta | o clac pesado da tampa, a sirene curta do pátio |
| L+0,8 | o lançamento: o míssil sai do tubo e acende; coluna de fogo branco e fumaça branca que sobe e fica | o estalo, depois o rugido rasgado que sobe e some |
| L+3 | some no alto da tela; a coluna de fumaça fica uns 40 s, entortando com o vento, com sombra | o trovão rolando longe |
| voo, 9 s | **a vítima vê os 6 pontos no chão nos últimos 3 s** (anéis vermelhos finos) | alarme na base atingida; a voz: "Incoming strike!" |
| T−2,5 | **o estouro alto, contado pela luz** (a câmera do RTS não vê o céu: o alto da imagem ainda é chão): um clarão só, branco-azulado, 3–6 quadros, o chão inteiro acende; **a luz se abre em cinco** | — (a luz chega antes) |
| T−2,5 a 0 | **as 5 ogivas entram pelo alto da tela**, pontos brancos com o risco de plasma laranja que fica 0,5–1 s na imagem (a foto de exposição longa de Kwajalein); **são 5 luzes que se movem: as sombras da base giram e encurtam apontando onde cada uma vai cair** (aviso e espetáculo na mesma coisa) | **chegam mudas** (o balístico é mais rápido que o som: é o terror) |
| T−1,6 | — | **o som tenebroso**: o trovão do estouro alto chegando de cima (~0,9 s depois do clarão; licença: o estouro a ~300 m virtuais pra o som chegar antes dos impactos) |
| T+0 a 1,5 | os 5 impactos no padrão escolhido, **no máximo 3 clarões por segundo** (o limite de fotossensibilidade das publicadoras: o primeiro clarão é o grande, os outros menores): a bola de fogo em volume, o cone de terra lançado pra cima, o anel de poeira do choque na velocidade real | cada um: estalo, corpo grave, a pressão do sub com o tranco da câmera |
| T+0,5 a 5 | — | **o rasgo invertido**: o estrondo supersônico de cada ogiva chegando depois do impacto, subindo (a física do balístico; nenhum RTS faz) |
| T+2 a 30 | as colunas de poeira e fumaça sobem, saem do nível das unidades em 3–5 s e derivam com o vento; os prédios atingidos caem a partir do ponto do impacto | a cauda rolando; os destroços; o silêncio voltando |

### 4.5 Os números (faixa inicial)

- Cada pedaço: **2 000 até 7 m, caindo a 560 aos 18 m, zero aos 26 m**; explosivo (na física, ~200 kg de TNT por pedaço: o anel
  forte da pesquisa, 30 m, era pra 150–250 kg). Total da salva: 10 000, o dobro da nuke no centro, mas em 5 pontos.
- **Um pedaço derruba uma fábrica** (2 000), uma usina de qualquer facção, o quartel, o prédio de tecnologia; o CC (5 000) só cai
  com 3 no miolo dele (com a blindagem dura do ZH, 80% contra explosão, a proposta pro CC e pra superarma, §5.5: 4).
- Pontos a no mínimo 12 m um do outro (não empilha num ponto).
- A conta (§8): os 5 na fábrica, na tecnologia, no centro de suprimento e nas usinas: **6 prédios, 42% do valor em prédios** (a
  nuke no meio, 54%).
- **O preço sobe junto:** $3 000 e 5:00 de carga (era $2 500 e 4:30), no patamar da Rússia ($3 500, 6:00).

### 4.6 O Cruise Missile Strike do general (o cuidado)

O general da USA já tem 6 Tomahawks (180 s, patente 2, aprovado em 23/10). A superarma precisa ser outra coisa aos olhos:

| | Cruise Missile Strike (poder) | Global Strike (superarma) |
|---|---|---|
| De onde vem | de lado, baixo, horizontal | de cima, vertical, do espaço |
| Como se vê | o míssil com asa e o rastro do Patriot | pontos brancos incandescentes com o risco de plasma |
| Força | ogiva de míssil (escala do Tomahawk) | ~5× cada uma, com a cratera grande |
| Mira | o círculo | ponto a ponto |

### 4.7 A cicatriz

4 crateras de ~9 m, no padrão que o jogador escolheu; os prédios atingidos viram ruína a partir do ponto do impacto (o que já existe
no `model_damage.py`, com a direção do golpe).

---

## 5. Rússia: Nuclear Missile

### 5.1 A ideia

A dele: a ogiva nuclear, porque é o foco deles. Uma só, **tática** (baixa potência na escala do jogo), a superarma mais forte, mais
cara e mais lenta: **tudo ou nada**.

**O nome** (revisão): eu tinha posto "Sarmat". Saiu: o RS-28 Sarmat é o míssil estratégico de 208 t com várias ogivas, o centro da
ameaça nuclear de hoje na TV russa; pôr uma ogiva tática de ~5 t nele é o contrário do "perto do real", e o `docs/FACCAO_RUSSIA.md`
proíbe os símbolos da guerra de hoje. **NUCLEAR MISSILE**, o nome do ZH, resolve com nostalgia e tom seguro.

**O que a faz da Rússia: ela molda o terreno** (a identidade "cava e queima" de `docs/FACCAO_RUSSIA.md`). É a única superarma
que muda o relevo de verdade: a cratera atrasa quem passa, o chão queimado fica a partida inteira, a zona de radiação nega a área
por 60 s. "A bomba nuclear molda o chão" é um dos pilares da `VISAO_DO_JOGO.md`: aqui ele vira arma.

### 5.2 As alternativas (na mesa)

| Ideia | A favor | Contra | Veredito |
|---|---|---|---|
| **A: a ogiva tática do silo** | o lançamento a frio do silo (o míssil salta da tampa, para no ar e acende) é único; a nuke é a nostalgia mais forte do ZH | a China do ZH tinha a nuke: aqui ela muda de dono | **recomendada** |
| B: Iskander-M com ogiva nuclear (2 caminhões) | real, móvel, a cara da Rússia de hoje | lança de caminhão = menos "superarma" de cima | a folha 4 mostra; perde pro silo |
| C: o trem de mísseis saindo do túnel (Barguzin) | o trem casa com o depósito ferroviário da Rússia (`FACCAO_RUSSIA.md`); lindo | o míssil furando o túnel ficou estranho; prédio grande | guardar pra variação (ou o general nuclear) |
| D: ATBIP, a "Pai de todas as bombas" (termobárica) | "queima"; sem radiação, mais "limpa" de ler | o TOS-1A já é a termobárica da Rússia; perde o "nuclear" que ele quer | não |
| E: Poseidon (o torpedo nuclear que faz tsunami) | **o pilar "a represa estoura e a água sobe"** | só em mapa com mar | **ideia pra mapa naval, depois** |

### 5.3 O prédio (Strategic Missile Silo)

**A direção dele (02/11): a Rússia é uma ogiva nuclear gigante, como a da USA, mas não sai do chão** ("o da USA já sai do chão, pra
que repetir?"); **o exemplo é a nuke da China do ZH: "uma casinha com a ogiva lá dentro"**. O bombardeiro e o míssil de cruzeiro
abaixo foram descartados por ele ("é a porra de uma ogiva gigante").

**4ª rodada (02/11):** `docs/concept/superarmas/russia_v4_ogiva_dentro.jpg` (`captures/concept/126_` e `127_`, Gemini Pro), a ogiva
dentro do prédio russo:
- **A, a torre:** o salão alto de painel pré-moldado, o telhado-serra verde que corre nos trilhos, o nariz verde do míssil saindo
  pelo teto aberto, o pórtico amarelo em cima, o trevo radioativo pintado na porta do teto;
- **B, as asas:** o galpão comprido cujo telhado-serra se parte e abre em duas asas, o míssil sendo erguido de deitado pra de pé pelo
  braço amarelo, o trevo pintado no chão da porta.

**Minha leitura:** as duas estão na família dos prédios russos do jogo e leem como superarma. A **B** tem o ritual mais forte (o teto
abrindo e o míssil subindo). Dois consertos na versão final: o míssil da B saiu com estrelas vermelhas (a estrela ficou com a China;
a Rússia usa o escudo tricolor) e há um letreiro na fachada.

**O rumo das duas foi aprovado** ("ambas podem funcionar, é o caminho, mas falta detalhes, não tem cara de superweapon ainda").

**5ª rodada (02/11):** `docs/concept/superarmas/russia_v5_mais_superarma.jpg` (`captures/concept/128_` e `129_`, Gemini Pro), o mesmo
desenho de cada uma com a superarma aprovada da USA anexada só como régua de detalhe e de presença: os muros de proteção inclinados
com faixas zebradas, as portas do teto grossas como porta de bunker, o míssil maior com o escudo tricolor (sem estrela), o vapor
saindo do teto aberto, as luzes vermelhas e os holofotes, os cabos e os canos de combustível entrando no prédio, o concreto manchado.
**Minha leitura:** as duas ganharam presença; a **B** é a mais superarma (o muro comprido de contenção, os trevos na asa do teto, o
míssil subindo no vapor). O acabamento segue o dos prédios russos provisórios do jogo (o anexo de família); o GPT, quando voltar,
pode subir o acabamento.

**Ele escolheu a A (a torre)** com pedidos: tirar o pátio quadrado grande; o desenho do chão da B (o trevo com as faixas na frente
da porta); a estrela no míssil (não a bandeira); o míssil da B de antes; e a versão fechada. A escolhida:
`docs/aprovados/superarmas/2026-11-02_russia_A_torre_ESCOLHIDA.jpg`.

**6ª rodada (02/11):** `docs/concept/superarmas/russia_v6_A_aberta_e_fechada.jpg` (`captures/concept/131_` fechada, `132_` aberta;
Gemini Pro). **Fechada:** o teto-serra fechado com os trevos, o pórtico parado numa ponta, sem vapor, sem o pátio grande, o trevo
pintado na frente da porta. **Aberta:** o teto aberto, o míssil de pé (verde com as faixas brancas, a ponta branca e as estrelas), o
vapor, o pórtico em cima. A 1ª tentativa da aberta trouxe o braço inclinado da B e perdeu o pórtico (`130_`); foi refeita a partir
da fechada. **O que ainda difere entre as duas:** a aberta perdeu os contrafortes de concreto dos lados; no modelo é um prédio só, com
os contrafortes da fechada nos dois estados.

**7ª rodada (02/11, depois da recarga), pelo método certo:** `docs/concept/superarmas/russia_v7_A_fechada_e_aberta.jpg`
(`captures/concept/134_` → `136_` aberta, `137_` fechada; Gemini Pro). A aberta é a A escolhida mudando só os pedidos (a laje vira
areia com a faixa da fundação, o avental zebrado com o trevo na frente do portão, a pintura do míssil com as estrelas); a fechada sai
da aberta, só fechando o telhado-serra e tirando o vapor. Conferidas lado a lado antes de mostrar: o mesmo prédio, os mesmos
portões, contrafortes, pórtico, chaminé, torre e contêiner nos dois estados.

**Correção (03/11): a aberta é com o míssil subindo NA DIAGONAL** no braço amarelo (a pose da B de antes, que ele tinha aprovado;
eu tinha deixado de pé): `docs/aprovados/superarmas/2026-11-03_russia_A_aberta_diagonal_APROVADA.jpg` (`captures/concept/147_`), ao
lado da fechada em `docs/concept/superarmas/russia_v8_A_diagonal.jpg`. O mesmo prédio nos dois estados (conferido). No jogo, o
lançamento é o braço erguendo o míssil da diagonal até ficar de pé, e aí o disparo. Uma 1ª tentativa anexando a B copiou o prédio
inteiro dela (`146_`, descartada).

**A referência da Rússia (02/11, depois que os créditos acabaram):** a A escolhida (`docs/aprovados/superarmas/2026-11-02_russia_A_torre_ESCOLHIDA.jpg`)
com os pedidos dele em texto, sem mais imagem: sem o pátio quadrado grande (a areia até o prédio, só a faixa da fundação); na frente
do portão principal, o avental zebrado com o trevo redondo pintado; o míssil de pé no mesmo lugar, verde com faixas brancas, ponta
branca e as estrelas vermelhas (sem a bandeira). **Fechado e aberto são o mesmo modelo** (o telhado-serra corre nos trilhos; o
pórtico, os portões, os contrafortes e as paredes iguais nos dois estados): o modelo 3D garante o que as imagens não garantiram.

Ideias descartadas (não eram o que ele pediu):
- **A, o bombardeiro da Tsar Bomba:** o hangar gigante na base; pronta, as portas abrem, o bombardeiro branco de asa em flecha (o
  Tu-160 de hoje) taxia e decola da própria base, cruza o mapa à vista de todos, solta a bomba com o paraquedas enorme (a Tsar Bomba
  de 1961 caiu de paraquedas, pra dar tempo de o avião fugir), a bomba desce devagar e vem o clarão. A antiaérea pode derrubar o
  avião no caminho (o drama vira a escolta).
- **B, o míssil de cruzeiro nuclear (o Burevestnik):** sai de uma rampa inclinada, voa baixo e serpenteando pelo mapa, visível pra
  todos (o terror chegando), e explode no alvo.

**REPROVADA a 3ª rodada** ("bagulho de areia? essa porra tá com a cara das construções russas que temos?"): o morro de terra saiu
da frase "cava" da facção, não dos prédios aprovados. A próxima nasce só da língua dos prédios russos do jogo (o painel pré-moldado,
o telhado-serra verde, a chaminé listrada, a treliça, o pórtico amarelo), sem morro.

**3ª rodada (02/11), reprovada:** `docs/concept/superarmas/russia_v3_silo_na_familia.jpg`
(`captures/concept/124_`, Gemini Pro; o GPT está sem créditos). Anexo: os prédios russos aprovados como estão no jogo
(`captures/concept/_attach/superarma_russia/`). O silo nasce na família deles: o morro de terra redondo com o muro de contenção e a
vala em volta, a tampa redonda de aço com o trevo radioativo grande correndo nos trilhos, o nariz verde do míssil aparecendo, o
pórtico amarelo por cima do silo (o mesmo do depósito russo), a torre de treliça, a chaminé listrada soltando vapor, o salão de
controle com o telhado-serra verde, as luzes vermelhas. **Minha leitura:** está na família e lê como superarma nuclear (o trevo, a
tampa, o pórtico); a variação 1 é mais aberta e legível (mas tem um letreiro escrito, que sai), a 2 é uma fortaleza murada. O
acabamento do Gemini é mais simples que o do GPT da USA.

**A 1ª folha foi reprovada ("muito genéricas").** A 2ª folha (`predios_v2_iconicos.jpg`, em cima à direita; `captures/concept/112_`)
foi pensada como as superarmas clássicas do C&C (um objeto só, inesquecível, que se transforma ao disparar):
1. **a Tulipa**: o míssil de quatro propulsores cônicos (o desenho do primeiro míssil intercontinental soviético) preso por quatro
   braços de aço fechados como pétalas sobre um fosso de chama cavado na terra; os braços se abrem como uma flor no lançamento;
2. **o Monumento**: um monumento brutalista colossal em forma de rastro de foguete, com os relevos heroicos na base; o míssil sai
   do alto dele;
3. **o Pica-pau**: a parede gigante de radar de treliça (a de Chernobyl) atrás de uma tampa de silo redonda;
4. **a Concha**: o morro fortificado de degraus com uma cúpula de aço rebitada que se parte em duas e se abre como uma concha.

**Minha leitura:** a Concha é a mais "superarma clássica" e a mais legível de cima (o círculo que se parte em dois); a Tulipa tem a
silhueta mais soviética e inesquecível. Dá pra juntar: a Concha com o míssil da Tulipa dentro. O Pica-pau é lindo e enorme (lê como
parede); o Monumento é o mais estranho.

O que estava antes (a 1ª folha, reprovada):
- **Recomendado: o 1, o silo no morro de terra**: a tampa redonda de concreto com a estrela, o anel de muros de proteção, o morro
  de terra em volta (a Rússia cava), a torre de treliça, o cano de gás amarelo. De cima é **o círculo da tampa no meio do morro**:
  a forma mais forte das quatro folhas.
- O movimento: armando, o vapor escapa em golfadas pela fresta; pronta, a tampa destrava (a faixa vermelha acende); lançando, a
  tampa corre nos trilhos e o vapor branco enche o poço.
- O 2 (o abrigo Krona com o caminhão gigante) é lindo, mas lê como um galpão comprido; o 3 é o trem (alternativa C); o 4 o
  Iskander (alternativa B).
- **Os cuidados da versão final** (segunda opinião): o morro cor de terra some no deserto (precisa da borda de concreto dura e da
  cor do jogador nas lonas e redes, a regra da facção); falta a língua da Rússia (o telhado-serra e a chaminé listrada, como no 2); e
  **a tampa abrindo é o aviso mais forte que a vítima com visão pode ter**: desenhar pra ler de cima.
- **Risco pra discutir:** o silo destruído com a ogiva armada vaza radiação na própria base (um anel pequeno). Paralelo ao reator
  chinês que derrete; pode ser demais (duas facções com "explode na sua cara").

### 5.4 A sequência

**A referência do disparo (03/11):** `docs/concept/superarmas/russia_disparo.jpg` (`captures/concept/152_`, GPT e Gemini, em cima da
aberta aprovada). Pra não repetir a USA (o lançamento a frio, catapultado e acendendo no ar), a Rússia é o oposto: **o lançamento a
quente dentro do prédio**. O braço amarelo termina de pôr o míssil de pé e tomba pra trás; o foguete acende ainda dentro do salão; o fogo
desce num fosso por baixo do prédio (a Rússia cava) e sai pelas bocas do fosso dos dois lados em **dois jatos enormes de fogo e fumaça
rasgando a areia**; o salão brilha por dentro pelo teto aberto; o míssil sobe devagar numa coluna laranja. **Minha leitura:** o GPT mostra
o míssil inteiro saindo do teto e os jatos mais fortes; o Gemini tem a fumaça dos jatos com mais volume, mas o míssil já saiu quase todo
do quadro. **APROVADO (03/11): o do Gemini** ("totalmente aprovado, quero exatamente assim no jogo"):
`docs/aprovados/superarmas/2026-11-03_russia_disparo_GEMINI_APROVADO.jpg`.

| Tempo | O que se vê | O que se ouve |
|---|---|---|
| L+0 | a tampa corre nos trilhos; vapor branco | o rangido do aço, o clangor no fim do curso |
| L+1,5 | **o lançamento a frio**: um tranco de gás, o míssil salta ~30 m do poço envolto em nuvem branca, **para no ar um instante** | o "tump" surdo, depois **0,4 s de silêncio** |
| L+2 | acende: a chama ofuscante, o míssil sobe | o rugido que faz o chão tremer, ouvido no mapa inteiro, abafado |
| L+2 a 60 | a coluna de fumaça fica sobre o silo, deriva com o vento, com sombra | — |
| voo, 14 s | **a vítima vê a elipse grande da previsão desde o lançamento, que encolhe até o anel exato nos ~5 s finais** (o radar real afina a previsão com o tempo): fugir cedo custa abandonar mais área, esperar arrisca | a sirene antiaérea da base atingida; a voz: "Nuclear missile launched!" |
| T−0,8 | um risco fino descendo rápido | **a mixagem cai pra quase muda** (o vácuo antes) |
| **T+0** | **o clarão**: 1–3 quadros (abaixo de ~5 kt não há o clarão duplo); a exposição sobe ~2–3 pontos e volta em ~0,2 s (sem tela branca chapada); toda sombra do mapa aponta pra longe do ponto | **a mixagem afunda −15 a −25 dB em 50–100 ms**, música junto; fica só um sopro (a "respiração" do Oppenheimer); as vozes nunca |
| T+0 a 1,5 | a bola de fogo (hemisfério de ~10 m de raio, encostada no chão) branca → amarela → laranja; brilho real de 0,5–1 s, no jogo 1,5–2 s pra ler (licença); o anel de condensação (Wilson) só nos mapas úmidos, nunca no deserto seco | — |
| T+0,05 a 0,5 | **o choque na velocidade real**: a areia sobe no lugar em anel e varre a base em ~0,3 s, derrubando paredes de lado (o prédio cai do lado de onde a onda veio) e jogando veículos | **o estrondo** na câmera 0,3–0,6 s depois do clarão (esse meio segundo é o silêncio físico): estalo, soco grave, a pressão do sub com o tranco |
| T+1 a 10 | a bola sobe e vira o toroide; **o vento de volta puxa a poeira pro centro** (10–30 m/s) e alimenta o caule; **a nuvem é castanho-avermelhada** (o NOx, a cor real), o caule e o anel da cor do chão; o caule sai do nível das unidades em 3–5 s | o sopro pro centro e o rugido baixo da coluna subindo; o trovão rolando com eco no relevo (10–30 s) |
| T+8 a 60 | a cabeça sai do quadro aos 8–12 s e para a 200–300 m (a nuvem real de 2 km comprimida, o tempo junto pela semelhança de Froude); **o volume some perto da câmera** (a tela nunca vira fumaça: cobriria a batalha e é o pior caso do desenho na 1660); **a sombra do cogumelo passa sobre a base**; a poeira fina cai no sol; a poeira acima de 3 m sai do nível das unidades em 3–5 s (licença: a física daria minutos) | o estalo do contador Geiger quando a câmera está sobre a zona |
| depois | a cratera, o chão queimado, o capim e as palmeiras queimados, as ruínas fechando ruas | — |

### 5.5 Os números (faixa inicial)

- **A potência: ~5 t** (a nuclear mais baixa que existiu, a Davy Crockett, ia de 10 a 20 t). Os anéis saem da física, na
  proporção 1 : 2 : 3,8 (20 : 5 : 2 psi): **total até 39 m, forte até 78 m, leve até 148 m**. O dano: **4 000 até 39 m, 1 300 aos
  78 m, zero aos 148 m** (explosivo): todo prédio no anel total cai, menos o CC (fica com ~18%).
- **Por que 5 t e não 20 t** (a pesquisa propôs 20 t, anel forte de 123 m e leve de 228 m): na base de referência, 10 t já
  derruba 11 de 12 prédios (62% só em prédios): acaba a partida numa salva. Com 5 t, 37%. A bola de fogo fica com ~10 m de raio
  (2,5 tanques de diâmetro): lê nuclear.
- **A radiação:** 40 m por 60 s, 15/s no soldado, 40% disso no veículo, 10% no prédio. **Licença escrita:** nas potências baixas a
  radiação real mata mais longe que o choque (150–200 m) e por minutos; a física cobriria a base inteira. A chuva radioativa real vai
  **a favor do vento** (uma elipse, não um círculo): bonito, físico, e pode cair no caminho do próprio russo; depende do vento na
  simulação (§9). Lê-se pelo chão (a poeira fina caindo e um tom leve), pelo Geiger e pelo anel no HUD; **nada
  de brilho verde** (o erro aditivo do ZH, fora do tom).
- **A cratera, o conflito:** na física, 5 t encostada no chão faz só ~7 m (20 t, 11 m × 3 m). A cratera "que molda o mapa" pede
  uma bomba que acaba a base. A saída está na alternativa F abaixo (a que cava) ou em aceitar que a cicatriz da nuke é o chão
  queimado, a ruína e a zona, com a cratera pequena.
- **F: "a que cava"** (a Rússia cava): a ogiva entra no chão antes de explodir (a ogiva de penetração, como a B61-11). Pela física
  do enterrado (o teste Sedan, 1962, escalado), a mesma 5 t faz uma **cratera de ~20 m**, a onda no ar fica menor (menos dano em
  volta: equilíbrio melhor) e o visual muda: **o domo de terra que estufa e estoura, a bola vazando pelo meio, a coluna de terra e
  o anel de poeira rolando rente ao chão** (a "base surge" das fotos dos testes). Muda o relevo de verdade e é a frase da facção ao
  pé da letra; perde o cogumelo clássico do ZH (a nostalgia). **Decisão dele.**
- A conta (§8): no meio da base de referência, **37% do valor só em prédios** (as unidades fogem nos 14 s; paradas, 69%).
- **O CC fica com ~18%; o gênero deixa o coração da base com 40–60%** (o ZH, 44%). Ajuste a fazer na conta nova: dar ao CC e ao
  prédio da superarma a blindagem dura do ZH (80% contra explosão), ou baixar o centro pra ~3 000.

---

## 6. China: o enxame de drones (decidido em 03/11)

**DECIDIDO (03/11): a superarma da China é o enxame de drones** ("vamos criar a superarma de drones pra China então, fechou, vai ser
assim"). Veio da conversa das facções: a China tem dois lados, a massa (a horda, o fogo) e a potência industrial (o reator, o monumental);
o canhão eletromagnético fica pro general de superarmas da USA, quando vierem os generais. A visão dele dos generais: USA Air Force,
Superweapons, Laser; China Flame, Infantaria; Rússia Tank, Nuclear; GLA Demolition, Stealth, Toxin (eu sugeri um 3º pra China, Drones,
e pra Rússia, Artilharia).

**O prédio, 3ª rodada (03/11):** `docs/concept/superarmas/china_v3_colmeia.jpg` (`captures/concept/148_`), pelo caminho que deu certo
nas outras: a colmeia na família dos prédios chineses do jogo (`captures/concept/_attach/superarma_china/`). A fortaleza redonda de
concreto escuro com ameias e portão, o anel de telhados de pagode verdes em volta, e **o topo inteiro é um favo de centenas de tubos
hexagonais**. Sem nada que abra (a USA sai do chão, a Rússia abre o teto, a GLA levanta os trilhos): **os tubos acendem em onda conforme a
carga enche** (o prédio é o relógio, como na GLA) e, no lançamento, os drones saem em cascata e sobem em coluna. **Minha leitura:** as duas
estão na família e leem como superarma; o GPT encaixa melhor na base (o portão com a estrela, os cabos); o Gemini é mais limpo, com os
pavilhões de pagode na borda, e a parte acesa saiu desenhando um yin-yang por acaso.

**O rumo foi aprovado** ("acertou de primeira"), com a mistura: a base do GPT sem o corredor do portão e o topo do Gemini (os
pavilhões e o brilho). **A mistura:** `docs/concept/superarmas/china_v4_colmeia_misturada.jpg` (`captures/concept/150_`, Gemini Pro
editando a do GPT): o portão abrindo direto na areia com a estrela pintada, os pavilhões de pagoda na borda do favo, as lamparinas.

**A chegada (03/11):** `docs/concept/superarmas/china_chegada_do_enxame.jpg` (`captures/concept/149_`), num quadro novo (os game shots
só como referência): o anel perfeito de drones em formação de máquina sobre uma base americana, as luzes vermelhas sincronizadas, os
fios mergulhando como raios de roda, as explosões, a antiaérea. **Minha leitura:** o GPT tem a câmera de jogo e o enxame lê como enxame,
mas as explosões ficaram pequenas e espalhadas (o defeito que derrubou a 1ª versão); o Gemini tem as explosões grandes e em volume e a
sombra do anel no chão (lindo), mas os drones saíram do tamanho de caças.

**APROVADOS (03/11): a colmeia misturada** (`docs/aprovados/superarmas/2026-11-03_china_colmeia_APROVADA.jpg`) **e a chegada do
GPT** (`docs/aprovados/superarmas/2026-11-03_china_chegada_do_enxame_GPT_APROVADA.jpg`). **O comportamento dos drones (a regra dele):**
não precisam chegar em formação certinha; vão chegando, **disparam mísseis meio pesados** no caminho (já causam dano) e, no fim, **são
kamikaze**: atiram, atiram e, quando chegam, se jogam e explodem no impacto, causando mais dano. Isso resolve o defeito da 1ª versão
(muitas explosões pequenas sem um momento): o ataque vem em duas ondas, os mísseis e depois os próprios drones. Sem imagem nova pra isso.

**A referência do disparo (03/11):** `docs/concept/superarmas/china_disparo.jpg` (`captures/concept/154_`, GPT e Gemini, em cima da
colmeia aprovada). As outras três são fogo de foguete; a China é **a colmeia esvaziando**: os tubos cospem um drone cada, em cascata,
com um sopro de gás e um estalo; os drones abrem as asas e sobem numa coluna enorme que se espalha no alto como um gêiser; o favo apaga em
onda conforme se esvazia (o contrário da carga); as luzes vermelhas dos drones piscam juntas. **Minha leitura:** o GPT sai em grade
ordenada (formação de máquina, os rastros de vapor de cada tubo); o Gemini explode pra fora em leque, com o favo ainda aceso nos tubos que
faltam (a onda de apagar se lê melhor).

**APROVADO (03/11): o disparo do GPT** (`docs/aprovados/superarmas/2026-11-03_china_disparo_GPT_APROVADO.jpg`).

**A ficha do drone (03/11):** `docs/concept/superarmas/china_drone_ficha.jpg` (`captures/concept/155_`, GPT; o Gemini bateu o teto de
gasto do mês no AI Studio). Perto dos drones de ataque chineses de verdade (a família em asa delta com hélice atrás, como o ASN-301): uns
3 m de asa, o nariz com o sensor e a ogiva (no fim ele se joga), a hélice atrás, as aletas nas pontas, **dois mísseis pequenos embaixo das
asas** (os que ele dispara no caminho), a luz vermelha em cima; o oliva escuro com a camuflagem em blocos da China, as estrelas vermelhas e
a cor do jogador numa faixa fina na ponta das asas. As vistas de cima, de três quartos, de lado e de frente, e um grupo voando ao lado do
tanque chinês pra escala.

**O que ainda falta pensar pro enxame arrepiar** (o motivo da 1ª reprovação: muitas explosões pequenas, sem um momento só, lendo como
bando de pássaros): o voo em formação de máquina, o zumbido crescendo, e um momento único na chegada (proposta pra discutir: o enxame se
junta numa forma só sobre a base antes de mergulhar, como nos shows de drones chineses).

## 6-antigo. China: em aberto (o histórico)

### 6.0 A conversa do canhão eletromagnético (02/11)

1. Ele perguntou se o canhão faz sentido, "mesmo que tinha na USA do C&C ZH". Eu disse que não (a superarma de energia do ZH é da
   USA) e propus a ogiva incendiária gigante.
2. Ele: **o fogo fica repetido** (a USA já é explosões de cima, a GLA uma chuva de explosões, a Rússia a explosão nuclear); quer uma
   diferente. E perguntou: então quem teria o canhão?
3. **A resposta, revista:** pelo mundo de hoje, **o canhão eletromagnético é da China**. A Marinha americana encerrou o programa dela
   em 2021, depois de ~15 anos e mais de US$ 500 milhões, sem levar nenhum ao mar; a China montou o dela num navio (o Haiyangshan) em
   2018 e fez o teste em mar aberto. E é **a única superarma que não é uma explosão vinda de cima**: o tiro vem de lado, cruza o mapa
   como um raio e o impacto é de meteoro (terra jogada pra cima, o prédio atravessado, a cratera), sem bola de fogo. O Particle
   Cannon do ZH era um feixe do espaço; o canhão é outra coisa. Fontes: [The Navy's Railgun Is Finally Dead](https://money.yahoo.com/navys-railgun-finally-dead-130000535.html),
   [SCMP: US holding fire on rail gun, China takes aim](https://www.scmp.com/news/china/military/article/3166582/us-holding-fire-rail-gun-development-china-takes-aim-planned),
   [Gizmodo: China may be testing a ship-mounted railgun](https://gizmodo.com/china-may-be-testing-a-ship-mounted-railgun-1822698248).
4. **Um esclarecimento:** no nosso jogo o reator nuclear que derrete é a usina da China (+10); a da Rússia é a usina a gás. A bomba
   nuclear é da Rússia. O canhão bebe dos reatores chineses: as luzes da base piscam no disparo.

A China só volta a ser desenhada depois da USA aprovada (uma facção de cada vez).

### 6.1 O que já se sabe

- **Reprovado (02/11): o enxame de drones** ("esse negócio de drone aí, cara, ideia péssima, não me arrepiou"). Ele era a colmeia de
  favo e 70–100 drones suicidas em formação; o registro está em `docs/APROVADOS.md` e o raciocínio antigo no histórico do git deste
  arquivo.
- **A regra dele pra todas:** uma superarma é **um tiro só**; por enquanto, só a GLA é de vários mísseis.
- **O que tem de ter, pela régua e pelas reprovações:** arrepiar na hora (luz, som, volume, um momento de cinema), ficar na memória
  (uma forma que não existe em mais lugar nenhum do jogo, como o prato do Particle Cannon e o silo da nuke do ZH), e ser China
  (massiva, monumental, o reator que dá energia demais, o fogo).

### 6.2 As ideias novas (na mesa)

As imagens do momento estão em `captures/concept/108_` a `110_` (GPT; o Gemini sai quando ele escolher a direção) e a folha dos
prédios em `captures/concept/113_`.

| Ideia | O tiro | Por que é China | Contra | Veredito |
|---|---|---|---|---|
| **A: Heavenly Thunder, o canhão eletromagnético** | **um projétil só**, disparado de um canhão colossal alimentado pelos reatores: cruza o mapa em ~1 s como um raio incandescente, rasgando um sulco de poeira por baixo, e bate como um meteoro: sem bola de fogo, um cone de terra e destroços jogado pra cima, o anel do choque, a cratera grande e o prédio atravessado de lado a lado | **o reator que dá energia demais vira arma**: no disparo as luzes da base chinesa inteira piscam e apagam por um instante (a energia indo pro canhão); monumental; e é real o bastante (a China mostrou o canhão eletromagnético no navio Haiyangshan em 2018; a escala é a "escapada" no estilo do Contra) | o canhão eletromagnético lê como ficção científica se o tiro virar laser (a imagem do GPT errou nisso: o projétil é um ponto incandescente com rastro, não um feixe) | **recomendada**: o único tiro do jogo que vem de lado e a única forma de canhão gigante; o "trovão" é um som que ninguém tem |
| B: Fire Dragon, a salva termobárica | 8 foguetes pesados que estouram em linha, um atrás do outro, como um dragão de fogo andando pela base; o chão queima | **o nome é real** (o lançador chinês Fire Dragon, da Norinco); o fogo é a China do `CONCEITO_EXPLOSOES` | **são vários tiros** (a regra dele: só a GLA); a termobárica é da Rússia (o TOS-1A) | não, pela regra |
| C: a barragem rolante | centenas de projéteis de artilharia caminhando pela base | **"arrasa em massa"** ao pé da letra | vários tiros; a fumaça cobre a batalha; lê como artilharia comum, não como superarma | não |
| D: a torre-pagode que se abre | um míssil único "Dongfeng" (Vento do Leste) sai de dentro de uma pagoda cujos telhados se abrem como uma flor de lótus; a ogiva seria termobárica gigante (o maior fogo do jogo) | a pagoda que vira lançador é inesquecível de cima | a explosão única e grande se confunde com a nuke russa | **guardar o prédio**: a pagoda que se abre pode ser o prédio do canhão (A) ou de outra arma |

### 6.3 O prédio

Na folha nova (`predios_v2_iconicos.jpg`, embaixo à esquerda; `captures/concept/113_`), cada desenho é uma arma: o canhão com os
anéis de cobre e as aletas em camadas como os telhados de um pagode (A), a torre-pagode que se abre como lótus (D), o lançador em
forma de dragão (B) e o trecho da Grande Muralha com os canos de artilharia (C). O prédio se decide junto com a arma.

**Minha leitura:** o canhão (1) é o mais forte: o cano comprido sobre a mesa redonda, com os andares de pagode no próprio canhão, lê
de cima numa olhada e é China. A pagoda-lótus (2) é inesquecível. O dragão (3) puxa pro desenho animado; a Muralha (4) lê como um
forte de artilharia.

### 6.4 A, o canhão: o roteiro proposto

| Tempo | O que se vê | O que se ouve |
|---|---|---|
| armando (5:00) | os anéis do cano acendem um a um, do fundo pra boca (o progresso da carga, sem legenda) | o zumbido elétrico subindo |
| L−3 | a mesa gira e o cano sobe até o ângulo do alvo | o rangido do aço, o motor da mesa |
| L−1 | **as luzes da base chinesa inteira piscam e apagam**; arcos violeta correm pelos anéis | o zumbido sobe até um guincho e corta |
| L+0 | o clarão branco-violeta na boca; o anel de poeira que o tiro levanta em volta do canhão; o recuo do cano | **o estalo seco do disparo**, depois o trovão |
| voo, ~1 s | o projétil cruza o mapa como um ponto incandescente com rastro, rasgando um sulco de poeira no chão por baixo do caminho | **chega mudo** (mais rápido que o som) |
| T+0 | o impacto de meteoro: o cone de terra e destroços jogado pra cima, o anel do choque correndo, o prédio atingido atravessado e caindo a partir do furo, **sem bola de fogo** | o soco do impacto; **o estrondo sônico do caminho chegando depois, rolando pelo mapa** |
| depois | a cratera grande (muda o caminho), o sulco do voo marcado no chão | — |

Os números esperam a escolha: a régua é "um tiro só com força de superarma" (o centro na faixa da nuke, a área menor).

## 7. GLA: SCUD Storm

### 7.1 A ideia

A dele: a tempestade de SCUDs do ZH, a superarma mais lembrada do Generals. Nove mísseis velhos, sujos, que balançam ao subir,
imprecisos, com a ogiva de veneno. **Mantém o nome do ZH** (nostalgia, decisão 3 da visão). O que muda:
- **é real e tem contra-jogo histórico:** a caça aos SCUDs e o Patriot contra o SCUD em 1991. **Só o Patriot derruba SCUD** (é o
  papel real do PAC-3; o Pantsir, a Gattling e o Stinger são defesa contra avião, helicóptero, drone e míssil de cruzeiro). E o
  derrubado **não some: cai torto** (~35 m fora, metade do estouro, a nuvem inteira), como em 1991, quando os pedaços caíam igual
  (o "Patriot vencendo o SCUD" foi número de propaganda). A defesa protege o alvo escolhido; não apaga a arma;
- **o veneno é gás de verdade:** pesado, baixo, escorre pro chão baixo e anda com o vento (o campo de vento único);
- **"vive da ruína":** a tempestade enche o campo de carcaças, e o Worker transforma carcaça em dinheiro (o sucateio aprovado em
  30/10): a GLA colhe a própria destruição.

### 7.2 As alternativas (na mesa)

| Ideia | A favor | Contra | Veredito |
|---|---|---|---|
| **A: SCUD Storm com veneno, interceptável** | nostalgia máxima; o contra-jogo de 1991 | — | **recomendada** |
| B: os lançadores móveis (os caminhões saem do túnel e se escondem) | "atira e foge"; a GLA do túnel | superarma que anda é difícil de achar e de contra-atacar; precisa do prédio pro relógio | um meio-termo: os caminhões saem do túnel do prédio pra lançar e voltam |
| C: a bomba suja (um caminhão-bomba nuclear improvisado) | sujo, GLA | perto demais da nuke russa; tom pesado | não |
| D: o comboio suicida | GLA clássico | é unidade, não superarma | não |

### 7.3 O prédio (SCUD Storm)

**3ª rodada, em palavras primeiro (02/11):** a GLA não repete as outras duas (a USA sai do chão; a Rússia é o prédio que abre o
teto). Na família dos prédios da GLA do jogo (o pátio murado de adobe com as torres de ameia, a cúpula, os arcos, a rede de
camuflagem, as faixas da cor do jogador; `captures/concept/_attach/superarma_gla/`):
1. **O pátio dos caminhões (recomendado):** um pátio de adobe murado como o do Command Center, coberto por uma rede de camuflagem
   enorme; embaixo, 3 caminhões lançadores velhos (os MAZ soviéticos reaproveitados), cada um com um suporte soldado de 3 SCUDs. Pronta:
   os Workers puxam a rede, os 3 caminhões erguem os suportes e os 9 SCUDs apontam pro céu. "Atira e foge", improvisado, sucata.
2. **A cúpula de adobe que abre:** a cúpula da GLA, gigante, abre em gomos e mostra os 9 SCUDs em anel (fica perto da Rússia, que
   também abre o teto). **Reprovada por ele** ("não achei uma boa ideia").

Ele escolheu experimentar **os caminhões**, mas **sem a cara de sucata improvisada** ("pra que ser de sucata improvisado, as
construções não são todas assim"): o pátio de adobe bem construído, como os prédios da GLA do jogo.
`docs/concept/superarmas/gla_v3_patio_dos_caminhoes.jpg` (`captures/concept/139_`, GPT e Gemini Pro). **Minha leitura:** as duas estão
na família. O GPT parece o Command Center da GLA com os caminhões estacionados no pátio (o prédio de cúpula atrás); o Gemini é uma
fortaleza com as torres de ameia, a rede em tenda e o alarme vermelho no portão. Nas duas, os foguetes ainda são pequenos pra cara de
superarma. **Ele concordou** ("não estão com muita cara de superweapon, e esses SCUD estão muito pequenos").

**4ª rodada (02/11):** `docs/concept/superarmas/gla_v4_feixe_de_9.jpg` (`captures/concept/140_`), a fortaleza do Gemini com o centro
que é a arma: a plataforma redonda de lançamento no meio do pátio, os 3 caminhões de ré contra ela em triângulo, os SCUDs grandes (do
tamanho do caminhão) erguidos juntos num feixe, a rede puxada pros muros, o vapor e os alarmes vermelhos nas torres. **Minha leitura:**
o GPT acertou (o feixe enorme no centro é a cara de superarma, e lembra a SCUD Storm do ZH); o Gemini deixou a plataforma vazia e os
mísseis nos caminhões. No GPT há mais de 9 mísseis no feixe: no modelo são 9.
**REPROVADA** ("tá fugindo muito de construção, da autenticidade do nosso jogo [...] dê ideias parecidas com as iniciais"): os
caminhões num pátio não são um prédio. A GLA volta pro caminho da USA e da Rússia: **um prédio da família da GLA com a arma dentro**,
com um jeito de revelar que não repita os outros dois (a USA sai do chão; a Rússia abre o teto). Ideias em palavras:
1. **A torre de ameias:** a torre redonda de ameias da GLA (a do canto do Command Center), gigante, no meio de um pátio de adobe com
   arcos e a faixa da cor do jogador; pronta, a coroa de ameias se abre em gomos e os 9 SCUDs sobem juntos de dentro dela. De cima, o
   círculo grande da torre.
2. **O pátio coberto:** o bloco de adobe grande como o Command Center, com o pátio coberto por um teto de tábuas e lona que corre
   pro lado; dentro, o suporte com os 9 SCUDs deitados levanta até ficar de pé, aparecendo acima dos muros.

**5ª rodada (03/11):** ele pediu pra testar as duas e mais uma diferente, olhando o ZH e os outros RTS.
`docs/concept/superarmas/gla_v5_tres_predios.jpg` (`captures/concept/141_`, `142_`, `143_`). A terceira veio do **SCUD Storm do ZH**
(o original: um pátio murado com **9 trilhos saindo do núcleo como as pontas de uma estrela**, os SCUDs deitados neles; a referência em
`captures/concept/_attach/superarma_gla/2_zh_scud_storm_referencia.jpg`):
3. **A estrela do ZH modernizada:** o pátio de adobe com as torres de ameia; no centro, o núcleo redondo blindado com o alarme e o
   símbolo tóxico; em volta, os 9 trilhos em estrela com um SCUD grande em cada, que se levantam pro céu um a um.

**O que os RTS ensinam pro prédio** (o ZH e os outros): a superarma que fica na memória é **uma forma só, que se reconhece de cima**
(o prato do Particle Cannon, o silo de portas zebradas da nuke, a estrela de trilhos do SCUD Storm; no Red Alert 2, a cúpula da Iron
Curtain e a torre da Weather Control; no C&C3, a pirâmide do Temple of Nod), **o ritual de se abrir antes de disparar** e a língua da
facção. **As quatro do nosso, sem repetir:** a USA sai do chão (a arena), a Rússia abre o teto (o salão), a GLA levanta de lado (a estrela)
ou sobe de dentro de uma torre (as ameias).

**Minha leitura:** o **pátio coberto** sai (o teto que corre repete a Rússia, e o suporte tem 12 mísseis). Entre as outras duas, eu fico
com **a estrela**: a nostalgia do ZH (quem jogou reconhece na hora), a forma mais clara de cima das quatro facções, os 9 trilhos contam
os mísseis sem legenda, e o ritual (os trilhos se levantando um a um) não repete ninguém. A **torre de ameias** é a segunda: a mais
dramática, redonda e nova. Na estrela, o GPT tem o núcleo mais forte; o Gemini, a cara da GLA (as redes, a ameia do portão).

**Ele (03/11):** a estrela está "muito melhor", mas igual ao ZH é cópia: pensar algo nosso. **6ª rodada:**
`docs/concept/superarmas/gla_v6_redemoinho_e_coroa.jpg` (`captures/concept/144_`, `145_`). Duas formas de tirar a estrela da cópia, com
a mesma ideia nova nas duas: **o prédio é o relógio** (cada trilho se levanta quando o SCUD dele fica montado, as 9 vagas de 30 s da
§3.2: de cima se conta quanto falta, sem legenda):
- **O redemoinho:** o pátio redondo de adobe (o muro em anel com as torres de ameia); os 9 trilhos **curvos, em espiral**, como os
  braços de uma tempestade de areia vista de cima; os montados de pé, os outros deitados na espiral; a fuligem seguindo a espiral.
- **A coroa:** os 9 trilhos presos na borda do núcleo, abertos no chão como pétalas; montado, cada um sobe pra dentro e o SCUD fica de
  pé encostado no núcleo: os de pé fazem uma coroa.

**Minha leitura:** **o redemoinho do GPT** é o mais nosso: a espiral se lê de cima numa olhada, é a "tempestade" do nome ao pé da
letra, e guarda a nostalgia (os mísseis em volta do núcleo) sem ser a estrela do ZH. O do Gemini voltou pra estrela (trilhos retos, pátio
quadrado): não serve. A coroa é a segunda: bonita e diferente, mas de cima lê como um castelo com torres.

**APROVADA (03/11): a coroa** ("pode ser a coroa, já gastamos tempo demais, eu aprovo a coroa"):
`docs/aprovados/superarmas/2026-11-03_gla_a_coroa_APROVADA.jpg`. **O pedido do movimento:** os SCUDs são **levados pelos trilhos pra
perto do centro e vão subindo até ficar de pé** encostados no núcleo, um movimento bonito (o trilho corre pra dentro e ergue o míssil
ao mesmo tempo, com o vapor, o tranco no fim e o alarme). Com a regra do relógio: cada um faz isso quando o SCUD dele fica montado; o
último a subir fecha a coroa e a superarma está pronta.

**A 1ª folha foi reprovada ("tudo muito genérico").** A 2ª folha (`predios_v2_iconicos.jpg`, embaixo à direita;
`captures/concept/114_`, Gemini Pro, porque o GPT recusa a GLA):
1. **o Navio**: um cargueiro enferrujado encalhado no deserto, meio enterrado na areia com a proa levantada, as escotilhas do convés;
   uma abre e o SCUD sobe do porão no braço;
2. **a Montanha de Sucata**: um morro de carros, ônibus e tanques velhos empilhados que se abre no meio como uma cratera e o SCUD sobe;
3. **a Cúpula do Bazar**: a cúpula de chapa e lona que se abre em pétalas e mostra o anel de SCUDs de pé (a mais perto do SCUD
   Storm do ZH);
4. **a Torre de Petróleo**: a torre de perfuração virada torre de lançamento, as bombas de petróleo balançando em volta.

**Minha leitura: o Navio.** Nenhum RTS tem; conta a GLA inteira numa imagem ("vive da ruína"); de cima, o casco de navio no meio do
deserto não se confunde com nada; e as 9 escotilhas do convés são os 9 SCUDs (o contador sem legenda). É comprido (maior que o lote
dos outros): pra superarma, a hierarquia de tamanho ajuda. A Cúpula é a segunda (a nostalgia).

O que estava antes (a 1ª folha, reprovada; esta saiu do Gemini Flash, mais "brinquedo"; o GPT recusou duas vezes pelo filtro de
violência):
- **Recomendado: o 3, o tanque de petróleo enferrujado cortado no alto e virado silo**, com andaimes, a faísca da solda e a rampa
  pro túnel do lado. De cima é **o círculo enferrujado**: a assinatura redonda, e conta a GLA inteira numa palavra (a ruína
  reaproveitada). Os SCUDs sobem de dentro do tanque um atrás do outro.
- **O 1, o fosso camuflado com os 3 caminhões**, é o segundo: a rede pra fora e o caminhão erguendo o míssil é o "atira e foge"
  de verdade; de cima lê como a rede (retângulo).
- O 2 (o pátio com o teto que abre) é bom e lê pouco de cima; o 4 são os trilhos do ZH refeitos (nostalgia; de cima lê como
  riscos).
- A vida: a solda faiscando, o Worker subindo no andaime, a lâmpada de alerta vermelha (a assinatura do HUD da GLA), **as pontas
  dos SCUDs montados visíveis de cima numa grade 3 × 3 dentro do tanque**: é o contador da montagem sem legenda, e o inimigo com visão
  sabe quantos vêm.
- A imagem é do Gemini Flash ("brinquedo"): a versão final refaz ancorada na raiz, com o pedido escrito como instalação industrial
  (o filtro do GPT reagiu às palavras).

### 7.4 A sequência

**A referência do disparo (03/11):** `docs/concept/superarmas/gla_disparo.jpg` (`captures/concept/153_`, GPT e Gemini, em cima da coroa
aprovada). Sem repetir a USA (a frio, acende no ar) nem a Rússia (a quente, o fogo pelo fosso): **a tempestade em cascata**. Os 9 SCUDs da
coroa disparam um atrás do outro, meio segundo entre eles, e a coroa vai se esvaziando; cada um acende com a nuvem laranja-marrom suja do
combustível do SCUD de verdade e sobe balançando com um rastro grosso e escuro; a fumaça dos 9 enche o pátio redondo e transborda por cima
do muro. **Minha leitura:** o GPT tem o pátio cheio de fumaça e a cascata mais legível (três no ar em alturas diferentes); o Gemini tem a
coluna de fumaça escura do que já subiu e a ignição mais bonita, mas a coroa ficou com mais de 9 mísseis. **APROVADOS os dois** ("pode
ser qualquer um"): `docs/aprovados/superarmas/2026-11-03_gla_disparo_GPT_APROVADO.jpg` e `..._GEMINI_APROVADO.jpg` (no jogo, 9).

| Tempo | O que se vê | O que se ouve |
|---|---|---|
| L+0 | a lâmpada vermelha gira; os Workers correm; a tampa de chapa do tanque é arrastada | a sirene de manivela; o rádio chiando |
| L+1 a 6 | os SCUDs acendem um a um (0,6 s entre eles): chama laranja suja, fumaça preta-marrom grossa; sobem **balançando** | cada um: o ronco rasgado, áspero, o metal sacudindo |
| voo, 10 s | a vítima vê a elipse grande da dispersão no chão; **o Patriot dispara**: o rastro dele sobe e sai do quadro (o encontro é no céu, que a câmera não vê), o estalo, e **os pedaços caem em chamas dentro da base** com o gás do SCUD derrubado | a voz da GLA no rádio chiado: "SCUD Storm launched!"; o uivo dos que chegam (Doppler) |
| chegada, 0 a 4 | os que passaram caem quase na vertical em cascata, **espalhados** (um se parte na descida e cai em pedaços; às vezes um falha); cada um: a explosão suja com **a névoa laranja-marrom** (o ácido do próprio SCUD, a assinatura suja) e **a nuvem amarelo-esverdeada pálida e pesada** (a cor real do cloro e da mostarda, acesa pelo mundo e com sombra, nunca brilhando como no ZH) | o baque surdo, o chiado do gás saindo; **o bipe do alarme químico** da base atingida |
| 4 a 40 | o gás **escorre** de 20–30 m até 40–60 m, procura o chão baixo, é **uma camada de 1–2 m** que anda com o vento a 1–3 m/s entre os prédios: **o soldado (1,8 m) some dentro, o tanque (2,4 m) fica por cima** (o desenho conta a tabela de dano: 100% no soldado, 25% no veículo); soldados saindo correndo | a tosse, o chiado sumindo |
| depois | o chão manchado de ocre-esverdeado (some em 1–2 min); as carcaças pro Worker | — |

### 7.5 Os números (faixa inicial)

- 9 SCUDs, espalhados com desvio de ~11 m em volta de uma grade 3 × 3 de 20 m (a elipse da mira).
- Cada SCUD: **1 000 até 10 m, 350 aos 21 m, zero aos 40 m** (explosivo; na física, ~100 kg de TNT, os anéis 10 : 21 : 40 m; o
  SCUD real tem ~800 kg, que daria 53 m de anel forte por míssil) **+ a nuvem**: nasce com 20 m, escorre até ~40 m, dura 40 s, 15/s (o soldado leva tudo, o
  veículo 25%, o prédio 2%).
- **A interceptação:** só o Patriot, pela taxa da arma dele no `Units.json` (4 mísseis a cada 2,75 s); o derrubado cai torto com o
  gás. Determinístico (a simulação é lockstep): os que chegam primeiro primeiro. Com isso, 2 derrubados ainda tiram 23% e 6 tiram 16%
  (a conta da segunda opinião).
- **A montagem:** 9 vagas de 30 s, $200 cada, só corre paga; lançar cedo (no mínimo 4) zera o ciclo.
- **Perto do ZH:** o SCUD do ZH faz 500 até 13 m e **150 até 53 m** (o anel largo é o que faz a tempestade "limpar"); na mesma conta,
  ele tira 46% na fábrica. O nosso, com a nuvem que escorre e o derrubado que cai torto, fica perto disso; medir.
- A conta (§8): no miolo da base, **32% dos prédios sem nenhum derrubado, 26% com 3, 12% com 5**; com o exército parado no gás,
  bem mais. É a mais barata e a única que a defesa enfraquece de verdade.

---

## 8. O balanceamento (a conta)

**A base de referência:** o desenho típico de uma base no meio da partida (CC, fábrica, centro de suprimento, quartel, prédio de
tecnologia, aeroporto, 2 defesas, a superarma, 3 usinas, 8 tanques perto da fábrica, 10 soldados perto do quartel): **$30 250**
de valor ($20 800 em prédios). Os prédios, os tamanhos e a vida são os do `Units.json`. A conta é um modelo simples (o dano cai
pela distância até a borda do prédio, como no ZH); o script: `docs/superarmas/superarmas_conta.py`.

Duas medidas: **só os prédios** (as unidades fugiram ou não estavam lá; $20 800 de valor) e **com as unidades paradas** ($30 250).

| Salva | Só os prédios | Com as unidades paradas | O que cai (só prédios) | Custo da salva | Carga | Prédios por minuto de carga |
|---|---|---|---|---|---|---|
| **USA, o tiro único que se parte em 5 (2 000 cada)**, na fábrica, tecnologia, suprimento e usinas | **$8 700 (42%)** | $9 600 (32%) | fábrica, suprimento, tecnologia, 3 usinas; CC intacto | $3 000 | 5:00 | ~$1 750 |
| (revisão de 01/11, reprovada: 4 ogivas de 1 300 / 5 de 1 000) | $4 700 (23%) | $5 600 (19%) | fábrica e as 3 usinas | $2 500 | 4:30 | |
| (1ª versão: 6 ogivas, + a tecnologia) | $7 200 (35%) | $8 100 (27%) | | | | |
| Rússia, no meio (5 t) | **$11 300 (54%)** | $20 750 (69%) | 9 prédios; o CC fica com 18% | $3 500 | 6:00 | ~$1 900 |
| China, 70 drones, sem antiaérea | **$6 200 (30%)** | $9 450 (31%) | tecnologia, 1 defesa, 3 usinas (com unidades, ela mata o exército inteiro em vez disso) | $2 000 | 5:00 | ~$1 250 + o exército |
| China, 20 / 35 derrubados | $5 200 (25%) / $3 400 (16%) | $8 800 (29%) / $6 300 (21%) | | | | |
| GLA, sem derrubar (média de 12 sorteios) | **$6 600 (32%)** | $12 600 (42%) | o miolo; com o exército parado na fábrica, o gás faz o resto | $1 800 (9 × 200) | 4:30 | ~$1 450 |
| GLA, 3 derrubados | $5 400 (26%) | $11 100 (37%) | | | | |
| GLA, 5 derrubados | $2 600 (12%) | $10 800 (36%) | | | | |

**A leitura:**
- **Em dólares, ficam na mesma faixa** (~$1 000 a $1 900 em prédios por minuto de carga). Mas o dólar engana (a segunda opinião,
  §10): **a USA com 4 ogivas tira "só" 23%, e são a fábrica e as 3 usinas**: a base para de produzir e anda a meia energia. Por isso
  a próxima conta mede o caminho crítico. A Rússia é a mais forte por salva e a mais lenta e cara; a USA tira prédios-chave, a
  Rússia tira o miolo, a China caça o exército (e os prédios leves quando não há exército), a GLA bate em área e o gás pune quem fica.
- **Duas anti-base (USA, Rússia) e duas que também servem contra o exército em campo (China, GLA):** soltar o enxame ou os SCUDs no
  exército inimigo no meio da batalha é um uso de verdade.
- **Quem não pode ser parado custa mais:** a USA e a Rússia pagam $3 000 e $3 500; a China e a GLA, que a antiaérea derruba,
  pagam menos.
- **A renda do jogo** (`docs/ECONOMIA.md`): um caminhão rende ~$950/min a 50 m; uma base no meio da partida tira ~$3 000/min.
  O custo de uma salva (~1 minuto de renda) pesa sem impedir.
- **O que a conta não vê:** a precisão do jogador da USA (pode ser bem pior), a fuga real das unidades, a cratera e a radiação
  atrasando o contra-ataque, o veneno negando a área, o fogo da China. **A prova é a partida IA × IA** com superarmas ligadas
  (o placar por minuto, `reference-ia-x-ia-sem-placa`), e o ajuste vem dela.

**A conta revista pela segunda opinião** (§10; as variações dela em `docs/superarmas/SEGUNDA_OPINIAO.md` §3.2):

| Cenário | % da base (com unidades) | Prédios críticos que caem (de 5: fábrica, suprimento, 3 usinas) |
|---|---|---|
| USA, 6 ogivas, mira buscada | 50% | 3 |
| USA, 4 ogivas, com visão / no escuro | 39% / 23% | — |
| Rússia na melhor mira, unidades fugidas | 44% | 4 (e a superarma inimiga) |
| Rússia no meio, base espalhada 1,5× | 9% | 3 |
| China, unidades fora do raio | 20% | 3 |
| China contra 2 Patriot (pela taxa da arma) | 14% | 2 |
| China num exército de campo de 20 tanques | 70% do exército | — |
| GLA na fábrica, 0 / 2 / 4 derrubados (200 sementes) | 23 / 20 / 17% (p10–p90 de 13–30%) | — |
| GLA com o SCUD do ZH | 46% | 2 |

Estes números são da 1ª versão (antes da nuke de 5 t e do SCUD novo). **A próxima conta**, antes de qualquer número final: o caminho
crítico e o tempo pra voltar a produzir e a receber (não o % do valor); média de sementes com a dispersão; 3 bases (compacta,
espalhada e a que a IA monta de verdade); a mesma reação pra todas; a antiaérea pela arma do `Units.json`; a melhor mira buscada; o
reator chinês derretendo em cadeia.

---

## 9. Os sistemas que cada uma pede (pra quando for fazer)

| | Simulação (determinística) | Efeito (o Fluxo, o BlastLab, o FXLab) | Som (por último) |
|---|---|---|---|
| **Todas** | o prédio, a ogiva armada no prédio (com a exceção da fila: cancelar devolve só o que falta, vender e perder não devolvem), o relógio pago e público, o prédio revelado, a mira, o voo, o dano pela distância à borda, o aviso, **a IA que usa e a que reage** | o lançamento (coluna de fogo e fumaça que fica, com sombra), o rastro de cada míssil (`RTSMissileSmoke`) | o anunciador de cada facção, a mixagem que cala o resto, o estrondo do mapa inteiro abafado |
| **USA** | 4 pontos com o mínimo de 12 m; **a precisão pela visão** (o desvio no escuro); a ogiva sem interceptação | a luz das 4 ogivas descendo (as sombras girando até o ponto), o risco de plasma, 4 bolas grandes em volume, o cone de terra; no máximo 3 clarões por segundo | o trovão de cima, a chegada muda, o rasgo invertido |
| **Rússia** | a zona de radiação; a cratera que muda o caminho (novo: o relevo na simulação; essencial se for "a que cava") | **o cogumelo** (novo: a bola que sobe e vira toroide, o caule puxando a poeira pro centro, a nuvem castanha, o anel de choque na velocidade real, a sombra do chapéu, o volume sumindo perto da câmera), o clarão de exposição | o vácuo antes, o estrondo, o ronco longo, o Geiger |
| **China** | 70 drones na simulação (escolha de alvo e acerto), **a antiaérea mirando drone** o voo inteiro | a formação de máquina com a sombra e as luzes, as explosões pequenas **injetadas no volume compartilhado do Fluxo** (um campo só, não 70 volumes), os fogos vizinhos juntando em ~10–15 colunas | o zumbido crescendo, os mergulhos |
| **GLA** | a interceptação do SCUD pelo Patriot (o derrubado cai torto), **o gás como camada rasa numa grade 2D da simulação** (a espessura do gás, o dano por onde ela passa) | o gás pesado como uma fatia fina em volume que escorre e recebe sombra e luz (a técnica da indústria pra gás denso, a camada rasa), a mancha no chão, a névoa laranja-marrom do ácido | o ronco áspero, o uivo, o chiado do gás, o bipe do alarme químico |
| **Sistema novo que falta** | **o vento na simulação** (um vento por mapa em ponto fixo; o gás e a chuva radioativa mudam a partida, então moram na simulação, e o desenho lê o vento dela); o fogo que queima a vegetação e fica | — | — |

---

## 10. A discussão (a segunda opinião e a minha posição)

Um agente revisor fez o papel do advogado do diabo: leu o estudo, as pranchas e a pesquisa, conferiu o ZH direto nos arquivos de
dados e no código-fonte, e rodou variações da conta. O texto inteiro: `docs/superarmas/SEGUNDA_OPINIAO.md`. Em uma frase dele: "a ideia
que amarra (a cicatriz de cada facção) e o relógio pago e público são bons; mas a conta não sustenta 'as quatro na mesma faixa', a USA
sai a mais forte e a única sem resposta, a GLA a mais fraca, o custo tem quatro buracos de regra, e vários números 'da física'
misturam potências diferentes".

| O que ele apontou | Minha posição | O que mudou |
|---|---|---|
| **A USA forte demais e sem resposta**: na mira buscada, 6 ogivas tiram 50%; o Particle Cannon do ZH punha 4 000 de dano dirigido e à vista, a proposta 7 800 de uma vez | **Concordo.** A regra 3 da régua ("toda superarma tem resposta") não valia pra ela | 4 ogivas, a precisão só onde vê, a área revelada no lançamento (§4.1). A ideia dele (5–6) fica na mesa, mais fracas cada |
| **Os buracos do custo**: a fila devolve tudo; sem limite de estoque; a China em energia se sabota; a GLA vira artilharia a cada 90 s; a carcaça-SCUD conta o dinheiro duas vezes | **Concordo com todos** | a regra inteira (§3.2) |
| **O custo é imposto, não decisão** (a salva devolve 3–6× o que custa) | **Concordo, e não muda a proposta:** o valor está no anúncio e no risco, não em fazer o jogador desistir | escrito com honestidade (§3.2) |
| **O prédio com 2 500 de vida faz a USA ganhar toda corrida contra a Rússia** | **Concordo** | 4 000, o do ZH (§3.3) |
| **A régua errada**: o % do valor esconde que a nuke derruba os 5 prédios críticos; uma base só, compacta; a mira não buscada; reações diferentes; antiaérea inventada | **Concordo.** A conta foi um primeiro corte | a próxima conta (§8, o fim): o caminho crítico (fábrica, suprimento, usinas) e o tempo pra voltar a produzir, 3 bases, a mira buscada, a antiaérea pela arma |
| **A nuke de duas potências** (o dano de 20 t com a cratera e a bola de ~0,3 kt) | **Concordo; já corrigido** quando a pesquisa chegou | uma potência só, 5 t, encostada no chão, com as licenças escritas (§5.5) |
| **"Sarmat" não**; "Nine Heavens" é o drone-mãe | **Concordo** | NUCLEAR MISSILE; DRONE SWARM |
| **Só o Patriot intercepta balístico**, e o SCUD derrubado cai torto | **Concordo.** O Pantsir não é antimíssil balístico; o 1991 dos pedaços caindo é o real | §7.1 e §7.5 |
| **O SCUD e a ogiva da USA são a mesma coisa pra quem intercepta** | **Em parte.** O SCUD (alcance curto) reentra a ~1,5–2 km/s, que o Patriot alcança; a ogiva de alcance intercontinental entra a ~7 km/s, fora do alcance dele. A diferença é física | a USA continua não interceptável; a resposta dela é a visão |
| **A câmera não vê o céu**: o estouro da USA, a interceptação e o cogumelo só existem pela luz e pela sombra no chão | **Concordo** (a pesquisa disse o mesmo) | as sequências refeitas (§4.4, §5.4, §7.4) |
| **Fotossensibilidade**: 6 clarões em 1,2 s passam do limite de 3 por segundo das publicadoras | **Concordo** | no máximo 3 por segundo; o primeiro é o grande; e uma opção "reduzir clarões" |
| **O enxame lê como pássaro** | **Concordo** (a própria imagem mostra) | a formação de máquina (§6.1) |
| **O gás cobrindo os tanques** | **Concordo**, e a física resolve: camada de 1–2 m | §7.4 |
| **O prédio da USA é o 3, não o 1** (o 1 tem a laje quadrada que ele reprovou em 01/11) | **Concordo**, eu tinha esquecido a regra dele | §4.3 |
| **A simulação não tem vento**, e o gás e a chuva radioativa mudam a partida | **Concordo**: é sistema novo e está no caminho crítico | §9 |
| **Decidir a regra antes do prédio** (o número de ogivas muda os tubos; a montagem da GLA muda o topo do tanque) | **Concordo** | a ordem (§12) |

**O que ele disse que falta** (e passa a fazer parte do trabalho):
- **a IA dos dois lados**: a que usa (quando armar, onde mirar só com o que ela vê) e a que reage (sair do anel, espalhar a base,
  construir antiaérea contra a China, atacar o prédio enquanto o relógio corre). Sem a reação, a IA × IA mede a IA, não a arma;
- **partidas em time**: duas ou três superarmas do mesmo time na mesma base em 30 s apagam um jogador (o ZH era igual; testar no
  2 × 2 e decidir);
- **fogo amigo**: no ZH a nuke e o SCUD batem em aliado e neutro; e o gás que o vento devolve pra quem lançou;
- **o prédio ocupado e os civis**: o gás limpa o prédio guarnecido (o Anthrax do ZH limpava)? A vila no raio da nuke (o tom);
- **o estado "não armada" no HUD** (forma, sem legenda) e o botão de armar com o preço na grade do prédio;
- **o tempo de voo fixo ou pela distância** (o enxame a 40 m/s muda com o mapa): escolher e escrever;
- **o multiplayer não toma a câmera**; a tecla de pular pro evento (o espaço do ZH); o replay (a mira ponto a ponto vira comando);
- **o daltonismo**: os avisos têm forma e movimento, não só vermelho;
- **os poderes do general como resposta**: o EMP pausando o relógio de uma superarma inimiga, o satélite espião dando a visão que a
  mira da USA pede (a árvore de patentes está em discussão: é a hora de cruzar);
- **a régua medida**: na IA × IA, a duração da partida com e sem superarma, quem ganha depois do primeiro lançamento, quantas vezes a
  superarma decide sozinha; e o teste dele com o pai na LAN, onde a frustração se mede.

---

## 11. O que é dele decidir

1. **A China:** o canhão eletromagnético (recomendado), a torre-pagode que se abre, ou outra ideia (§6; o enxame foi reprovado).
2. **O prédio da USA:** o arranjo da casa, dos trilhos e do satélite grande (§4.3; a 2ª folha).
3. **A Rússia:** a nuke clássica de cogumelo (5 t, cratera pequena) ou **"a que cava"** (a mesma potência enterrada: a cratera de
   ~20 m que muda o relevo, o domo de terra e o anel de poeira, sem o cogumelo clássico) (§5.5).
4. **O prédio da Rússia e o da GLA** (a 2ª folha de cada uma; as minhas leituras em §5.3 e §7.3: a Concha com o míssil da Tulipa; o
   Navio). Daí sai a versão final e a ficha (o caminho do `docs/CONCEITO_CONSTRUCOES.md` §4).
5. **Os momentos:** o rumo de cada efeito pela 2ª rodada (o GPT tem mais volume, o Gemini é mais fiel ao jogo).
6. **O modelo do custo:** armar a ogiva, 1 por prédio, o relógio pago e público (§3.2).
7. **Os nomes** no topo: GLOBAL STRIKE, NUCLEAR MISSILE, (a China), SCUD STORM.
8. **Fogo amigo e partidas em time** (§10): como no ZH (bate em aliado; times empilham) ou com limite.

## 12. A ordem de fazer (depois das decisões)

1. **A regra primeiro** (a segunda opinião: o número de ogivas muda os tubos do prédio; a montagem da GLA muda o topo do tanque; a
   arma da China muda o prédio inteiro): a conta nova (§8, o fim) e as decisões da §11.
2. O prédio de cada facção: a versão final, a ficha, o modelo por script com os estados de dano.
3. A regra na simulação (o prédio, a ogiva armada, o relógio pago, a mira, o voo, o dano, o aviso, o vento, a camada do gás, os
   drones como alvos aéreos, a IA que usa e a que reage) e a partida IA × IA com as superarmas ligadas, 1 × 1 e 2 × 2: o
   balanceamento medido.
4. O efeito de cada uma no laboratório, uma de cada vez, em vídeo, com a prova no jogo ampliada (a regra da fumaça), e o custo
   medido na 1660.
5. O HUD (o botão, a ogiva na grade, o estado "não armada", a mira de cada uma, os avisos com forma) e os ícones de cada facção.
6. O som e as vozes, por último (pedido dele).
