# Os ícones dos poderes: a proposta (03/11/2026)

O pedido: cada tipo de poder com o seu padrão de desenho. Os tipos que ele citou: o desbloqueio de unidade (como o Paladin do ZH),
o ataque, o suporte, o drop e o bônus. Cada ícone **traduz o que o poder faz** e é **fácil de ler**, sem legenda. A 1ª tentativa
(4 estilos, e uma moldura com forma e um selo colorido por tipo) foi reprovada: "mal pensado, sem embasamento".

O estudo está em `docs/poderes/ESTUDO_ICONES.md`: a série C&C pelos arquivos do ZH, Blizzard, os MOBAs, os jogos de carta, a
simbologia militar da OTAN e as regras de acessibilidade. **Proposta, nada aprovado.**

---

## 1. A decisão em uma frase

**O tipo se lê em duas camadas.**
1. **O jeito de usar dá a família**, de relance, pelo formato do próprio botão: o que se aperta, o que fica ligado sozinho e o que
   vira unidade nova. São só 3 famílias.
2. **Dentro dos poderes que se apertam**, a própria imagem diz o subtipo: o assunto e a luz da cena. Não há moldura nem selo a mais.

## 2. Por que assim (o que o estudo mostrou)

- **Nenhum jogo grande separa 6 tipos pela moldura.** Os que funcionam mostram de 2 a 4 famílias de relance e deixam o resto pra
  imagem. Perto de 6 categorias, a leitura cai (o estudo de 2025, §2.3).
- **O corte mais usado é o jeito de usar.**
  - O que se aperta tem relevo de botão; o passivo é chapado e não tem tecla (Warcraft III, Dota 2).
  - O jogador já sabe ler isso sem aprender.
- **O desbloqueio é a própria imagem da unidade.**
  - No ZH, o Paladin que a patente libera usa o mesmo retrato do botão de construir.
  - O nosso HUD aprovado já faz essa divisão: **o que se constrói é foto**, **a ação é símbolo**.
  - Comprou o poder, a mesma imagem aparece na fábrica: o jogador entende na hora o que ganhou.
- **Duas diferenças juntas não saltam aos olhos; uma sozinha salta** (Treisman, §2.2).
  - A 1ª tentativa juntava 5 formas e 5 cores pra dizer a mesma coisa, e as formas não vinham de nenhuma convenção (chanfro = golpe,
    hexágono = desbloqueio): o jogador teria de decorar.
  - Aqui cada camada tem um sinal só.
- **A cor nunca sozinha** (Xbox XAG 103).
  - O verde de "comprado" e o dourado de "dá pra comprar" continuam só pro estado.
  - A luz da cena que separa os subtipos vem junto com o assunto, então não é o único sinal.
- **Pequeno:** uma marca de 12 px num canto vira ruído e disputa o canto com o nível e o estado (Gage, GDC 2018). Aqui não há marca
  nenhuma: o sinal é o botão inteiro ou a imagem inteira.

## 3. As regras

### 3.1 As 3 famílias (o botão)

| Família | Quem | O botão | Onde aparece |
|---|---|---|---|
| **Comando** | golpe, olhos, tropas e apoio: tudo o que você aperta e mira | **o botão em relevo do HUD da facção** (o mesmo metal, a borda clara em cima e a sombra embaixo), com a tecla no canto | a coluna de poderes (com o relógio), a tela do general, o detalhe |
| **Passivo** | o bônus que fica ligado (o AWACS; nos outros, o Cash Bounty, os treinos) | **uma placa chapada de metal, sem relevo e sem tecla**, com o desenho gravado e o esmalte da cor da facção | só a tela do general (não tem botão nem relógio) |
| **Desbloqueio** | o poder que libera uma unidade ou um prédio (o Paladin do ZH) | **a mesma foto do botão de construir** da unidade, a do HUD (`kit/photos/buttons`) | a tela do general e, depois de comprado, o botão novo na fábrica |

O passivo chapado não pode parecer bloqueado. O bloqueado continua com o cadeado e o cinza; a placa é colorida, com o esmalte.

### 3.2 Os 4 subtipos do comando (a imagem)

A regra de arte: **um assunto só, grande (uns 70% da casa), recortado contra um fundo simples**. No teste de 03/11, a silhueta e o
emblema foram os que leram melhor pequenos; o pintado com cenário embolava. O subtipo vem do **assunto** (o que se vê) e da **luz**
(a temperatura da cena):

| Subtipo | O assunto | A luz | Por quê |
|---|---|---|---|
| **Golpe** | a arma no instante do disparo ou do impacto | quente, de fogo | o que machuca o inimigo |
| **Olhos** | **a imagem do próprio sensor**: a câmera térmica (branco-quente), com as marcas de rastreio | o cinza da térmica | o poder te dá visão, então o ícone é a visão; o verde da visão noturna fica de fora pra não brigar com o verde de "comprado" |
| **Tropas** | quem chega: os paraquedas, o soldado, o avião que pousa | dia, céu aberto | a chegada |
| **Apoio** | o efeito nas suas unidades: o conserto, o escudo | fria, azul | o que ajuda o seu lado |

A cara da facção vem do metal do botão (o HUD aprovado de cada facção) e do assunto. **O jeito de desenhar é o mesmo nas 4 facções**:
o jogador aprende uma vez. Isso substitui a ideia de "um estilo por facção" (USA = sensor), que não passou pelo estudo.

## 4. Os ícones da USA (o que cada um mostra)

| Poder | Família / subtipo | O que o ícone mostra | Por que traduz | A 46 px |
|---|---|---|---|---|
| Spy Drone | comando / olhos | a térmica vista de cima: os tanques inimigos brancos-quentes, com os colchetes de rastreio | é o que o drone te mostra | manchas brancas grandes no escuro |
| Target Designation | comando / olhos | a térmica do satélite com o losango de laser travado num tanque | marcar o alvo | o losango e o tanque |
| A-10 Strike | comando / golpe | o A-10 de frente, baixo, com o clarão do canhão e a fileira de impactos | o rasante que metralha | o nariz, as asas retas e o clarão |
| AC-130 Gunship | comando / golpe | o AC-130 inclinado de lado, à noite, os traçantes descendo | a órbita que atira pra baixo | os 4 motores e o leque de traçantes |
| Naval Gunfire | comando / golpe | a torre tripla do encouraçado no instante da salva, a bola de fogo dos canos | o canhão do navio | os 3 canos e o clarão |
| MOAB | comando / golpe | a bomba enorme pendurada no paraquedas, o clarão laranja embaixo | a bomba grande que desce | o charuto e o paraquedas |
| Paradrop | comando / tropas | três paraquedas abertos descendo no céu | chamar tropa do ar | três cúpulas |
| SEAL Insertion | comando / tropas | o SEAL ajoelhado com o óculos de visão noturna de 4 tubos, a carga na mão | o comando que derruba prédio | o capacete com os 4 tubos |
| Forward Operating Base | comando / tropas | o C-17 pousando atrás do posto de sacos de areia com a bandeira | o posto que chega pelo ar | o avião e a bandeira |
| Emergency Repair | comando / apoio | o tanque com as faíscas da solda e as caixas caindo do Chinook | o conserto na hora | o tanque e as faíscas |
| Guardian Angel | comando / apoio | o feixe de laser subindo da base e acertando o míssil no céu | o escudo que derruba o que chega | a linha do laser e o estouro |
| AWACS Early Warning | passivo | a placa gravada: o AWACS com o disco e as ondas, no esmalte azul | o radar que sempre vigia | a placa chapada contra os botões |
| *(exemplo de desbloqueio)* | desbloqueio | a foto do botão de construir de uma unidade (no teste, o tanque da USA, que já tem foto) | a unidade que você ganha | igual ao botão da fábrica |

A USA não tem desbloqueio na árvore: a tecnologia abre pela patente, decisão de 03/11. A família existe pros generais e pras outras
facções. O exemplo só prova a família.

## 5. O teste (quando ele der o ok)

- **Uma folha** com os 12 ícones de comando e a placa do AWACS, pela regra acima: no GPT e no Gemini, 2 imagens.
- **Recortar e montar na tela de verdade** (`make_tela.py`): a tela do general, a coluna de poderes e o detalhe. **Uma tela por
  imagem:**
  - a grade inteira;
  - a coluna;
  - o detalhe de um poder de cada família.
- **O critério de passar:**
  - a 46 px, dá pra dizer o subtipo de cada um sem ler o nome;
  - o passivo não parece bloqueado;
  - o A-10 e o AC-130 não se confundem.

## 6. Os riscos

- **O subtipo depende da arte.** Se a imagem sair embolada, o subtipo some. Daí a regra: um assunto só, grande, fundo simples.
- **A luz como 2º sinal:** golpe quente e apoio frio. Se a arte não respeitar, golpe e apoio se confundem.
- **A placa do passivo** tem de ter cor (o esmalte), pra não ler como bloqueada.
- **O relevo do botão** tem de ser o do HUD de cada facção (a China laca, a GLA chapa, a Rússia aço), ou a família não se lê igual
  nas quatro.

---

## 7. O teste (03/11)

- **Ele:** "Gera o teste".
- **A folha:** 11 ícones de comando e a placa do AWACS, no GPT e no Gemini (`captures/concept/164_`; o prompt nas receitas). O
  exemplo de desbloqueio é a foto do tanque do kit do HUD.
- **A montagem:** `python docs/poderes/tela/cut_sheet.py <folha> <gpt|gemini> drone,desig,a10,ac130,naval,moab,para,seal,fob,repair,angel,awacs`
  recorta; `make_tela.py usa --icons <gpt|gemini> --column a10_1,naval1,drone,para1,repair1 --shot 5 drone,a10_1,naval1,para1 <sel>`
  monta e fotografa:
  - o comando é o botão com o relevo (luz em cima, sombra embaixo);
  - o passivo é a placa chapada;
  - o desbloqueio é a foto com o filete de aço.
- **As fotos, uma tela por imagem:** `docs/concept/poderes/icones_familias_<gpt|gemini>_sel_<a10_1|awacs>.jpg`.
- **A minha conferência, pelo critério da §5:**
  - **O subtipo lê sem o nome:**
    - os olhos são as térmicas cinza, os únicos sem cor: o Spy Drone e o Target Designation;
    - os golpes são quentes, alaranjados: o A-10, o Naval e a MOAB;
    - as tropas têm céu: o Paradrop e o posto avançado; o SEAL lê pelo soldado;
    - o apoio é azul: o conserto e o laser.
  - **Falha: o AC-130 do GPT ficou escuro demais** a 46 px (a cena noturna). Precisa de mais luz nos traçantes.
  - **O passivo não parece bloqueado:** a placa de aço com o azul lê diferente dos botões.
  - **O A-10 e o AC-130 não se confundem.**
  - **Falha: o relevo do botão quase não aparece** a 46 px. Hoje quem separa as famílias é a imagem: a placa chapada e a foto de
    estúdio do desbloqueio. Se a família tiver de ler pelo botão, o relevo tem de ser o do HUD de verdade (a arte do kit), não a borda
    em CSS.

## 8. A escolha dele (03/11) e a volta

- **No rumo:**
  - o GPT na maioria;
  - o Naval, a MOAB e o AC-130 do Gemini;
  - o conjunto escolhido está em `docs/poderes/tela/icones/usa_<poder>_esc.png`.
- **Reprovados:**
  - o Emergency Repair: "não dá pra saber que é de conserto";
  - o Guardian Angel: "parece um raio de atirar, todo mundo vai se enganar";
  - o Forward Operating Base: "não representa o que eu tinha entendido", e o poder ficou em dúvida.
- **A volta:**
  - **O conserto:** a chave inglesa é a convenção. O próprio HUD aprovado usa a chave no botão de consertar, e o ZH também. Fica a mão de
    luva apertando o parafuso da esteira de um tanque com a chave, em close, e a chave é a maior forma do ícone.
  - **O escudo:** tem de ler como proteção. A cúpula azul translúcida cobre os nossos tanques, e os mísseis inimigos estouram do lado de
    fora dela; a cúpula é a maior forma.
  - **O posto avançado:** volta pra discussão do poder (`docs/poderes/USA.md` §15).
- **Os dois novos** (`captures/concept/166_`, GPT): a chave na esteira e a cúpula com os mísseis estourando por fora. Recortados com
  `cut_sheet.py ... v2 repair,angel` e postos no conjunto escolhido.
- **As fotos:** `docs/concept/poderes/icones_escolhidos_sel_<angel|repair1>.jpg`.
- **A minha conferência:** a cúpula lê como escudo também a 46 px; a chave lê bem no detalhe, mas na casa pequena da grade fica
  escura.
- **03/11, a 2ª volta:**
  - **Ele:** o conserto "tá ruim de ver, bate o olho e não dá pra ler o que é"; o AWACS "a imagem não me representou isso [o aviso], precisamos
    ser inteligentes, criativos, originais".
  - **O conserto:** o problema era o fundo escuro e o detalhe; virou a chave grande e clara na diagonal, sobre a frente do tanque, com o
    fundo azul claro.
  - **O AWACS:** a placa chapada (passivo) com o AWACS visto de cima, e o disco de radar virou a tela, com as setas vermelhas dos ataques
    chegando e o ponto de cada impacto. Ele diz "eu vejo o ataque chegando", não "um avião".
  - O pedido é o `captures/concept/167_`; as fotos estão em `docs/concept/poderes/icones_escolhidos_v3_sel_<repair1|awacs>.jpg`.
- **03/11, o posto avançado** (ele manteve o poder e pediu "uma imagem mais bonita"):
  - o ícone tem de dizer o principal do poder, que os reforços saem dali lá na frente;
  - duas ideias: **o portão** (o posto de barreiras com a bandeira e o tanque saindo pelo portão na direção de quem olha) e **o forte
    visto de cima** (o quadrado de barreiras com os bastiões, o heliponto e a sombra do C-17);
  - no GPT e no Gemini (`captures/concept/168_`); recortados com `cut_sheet.py ... fobPortao,fobCima`;
  - as fotos, uma tela por imagem: `docs/concept/poderes/posto_avancado_<gpt|gemini>_<fobPortao|fobCima>.jpg`.
  - O conjunto escolhido segue com o antigo até ele escolher.

## 9. O conjunto da USA hoje (03/11)

Os arquivos estão em `docs/poderes/tela/icones/usa_<poder>_esc.png`; a tela é `docs/concept/poderes/icones_usa_conjunto.jpg`.

| Poder | De onde veio |
|---|---|
| Spy Drone, Target Designation, A-10, Paradrop, SEAL | o GPT da 1ª folha (`164_`) |
| AC-130, Naval Gunfire, MOAB | o Gemini da 1ª folha (`164_`), a escolha dele |
| Guardian Angel | a cúpula do GPT (`166_`) |
| Emergency Repair, AWACS | a chave clara e a placa do radar com os ataques chegando, do GPT (`167_`) |
| Forward Operating Base | o portão do Gemini (`168_`), a escolha dele |

