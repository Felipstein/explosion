# Conceito: construções

Guia pra criar construções novas do RTS seguindo a mesma identidade de cada
facção. É o par do `CONCEITO_UNIDADES.md`: ferramentas, regras dos geradores,
medições e a bíblia de cores das facções estão lá. Aqui fica só o que é
específico de prédio.

Aprovadas em `docs/concept/buildings/`. Histórico completo (todas as rodadas,
inclusive as rejeitadas) em `captures/concept/21_*` a `24_*` (fora do git).

---

## 1. A fórmula (estudada nas War Factories originais do Zero Hour)

O que fazia os prédios do ZH ficarem na memória não era realismo, era isto:

1. **Diorama compacto:** um lote retangular com um prédio principal e 2 ou 3
   peças menores. **Poucas formas grandes e simples**, nada de entulho de
   objetos.
2. **Um elemento-assinatura** por prédio, que se reconhece de cima num relance
   e conta a história da facção. No ZH: o hangar abobadado com nervuras da USA,
   o telhado em camadas tipo pagode da China, o pátio de adobe com cara de
   mercado do Arms Dealer do GLA.
3. **A função aparece na forma:** porta de saída, pátio de reparo com braço
   ou guindaste e piso pintado (faixas de perigo, emblema no chão) mostram onde
   a unidade sai e onde volta pra consertar.
4. **Vida:** fumaça, ventilador girando, forno aceso, faísca. O prédio parece
   trabalhando.
5. **Uma cor forte de destaque** (a do jogador) nas peças certas, nunca o
   prédio inteiro pintado.
6. **Assentado no chão, sem estrada nem pátio** (Felipe, 11/10). A base básica não tem piso: o pátio
   e a estrada são uma mecânica que o jogador constrói (`docs/IDEIAS_JOGABILIDADE.md`, ideia 2).
   Pra o prédio não parecer "plantado na areia":
   - só a borda de fundação do próprio modelo (como o piso da War Factory aprovada);
   - areia acumulada no pé das paredes;
   - o chão em volta revolvido pelas esteiras e pneus;
   - uma saia de poeira e sujeira subindo o primeiro metro da parede.

Como erramos antes de acertar:

| Rodada | O que saiu | Por que falhou |
|---|---|---|
| v1 (`21_*`) | galpão pré-fabricado, bunker, ferro-velho | "inspirado em base real": genérico, sem graça. Ele "broxou na hora". |
| v2 (`22_*`) | ideias com assinatura | GPT foi pra ficção científica (neon azul, base espacial) e entulhou de caixinhas. |
| v3 (`23_*`) | fórmula do ZH no prompt | Acertou. Daqui saíram as finais. |
| finais (`24_*`) | mistura das favoritas | Precisou de 2 ou 3 ajustes de tamanho e de limpeza. |

---

## 2. A bíblia das facções (prédios)

### USA: base militar de alta tecnologia, limpa e organizada

- **Material:** painéis de metal cinza-claro e branco, concreto claro, com
  desgaste e poeira.
- **Linguagem:** volumes retos e robustos, estrutura industrial à mostra,
  amarelo industrial nos guindastes, faixas de perigo amarelas e pretas no
  piso, luzes de serviço.
- **Elementos da facção:** pórtico ou guindaste amarelo, domo de radar,
  bandeira dos EUA no mastro, chaminés soltando **vapor branco**, ventiladores
  grandes no teto, braço robótico de reparo num pátio com faixas de perigo.
- **Cor do jogador:** todo o azul (faixas nas paredes e **painéis de lona
  amarrados no teto**, o mesmo objeto do tanque).
- **Nunca:** neon, portas brilhando, cara de nave espacial; caixa genérica
  sem assinatura.

### China: indústria pesada monumental, propaganda e tradição

- **Material:** concreto **escuro, puxado pro verde-oliva** (combina com os
  tanques; o concreto cinza-claro ficava parecido com a USA).
- **Linguagem:** blocos pesados de concreto, paredes inclinadas, poucas placas
  grandes e limpas.
- **Elementos da facção:** **telhado em camadas tipo pagode** (verde com bordas
  vermelhas), **torre de alto-falantes de propaganda** com estandarte, pórtico
  vermelho com estrela, **estrela vermelha grande pintada no piso**, forno com
  a boca acesa em laranja, chaminé listrada de vermelho e branco, estandartes
  vermelhos com estrela.
- **Cor do jogador:** todo o vermelho forte (pórtico, estandartes, lonas e a
  estrela).
- **Nunca:** trem ou trilho (ele vetou); muitos estandartes (3 ou 4 bastam);
  cruz vermelha.

### GLA: complexo de adobe improvisado, mercado clandestino e túnel

- **Material:** adobe cor de areia, chapas enferrujadas, lona, rede de
  camuflagem.
- **Linguagem:** pátio murado, telhados planos, escadas externas, peças
  remendadas.
- **Elementos da facção:** **unidade subindo de uma rampa subterrânea**
  (os túneis são a marca do GLA), rede de camuflagem por cima, gaiola de
  armas, casinhas de adobe nos cantos, armazém fechado, chaminés com **fumaça
  preta**, bandeiras verdes, rabiscos verdes pintados à mão nas paredes, borda
  verde no alto dos muros.
- **Cor do jogador:** todo o verde (bandeiras, rabiscos e borda dos muros,
  o mesmo jeito do tanque).
- **Nunca:** morro ou penhasco (ele vetou: tudo em chão plano); "vila
  inteira" com dezenas de casas e barracas; montanhas de caixas, projéteis e
  tambores.

---

## 3. O que já está aprovado

Cada prédio tem a imagem aprovada e a ficha técnica (`*_sheet.webp`, 5 vistas,
feita no Gemini).

| Prédio | Arquivo |
|---|---|
| War Factory da USA | `docs/concept/buildings/usa_war_factory.webp` e `usa_war_factory_sheet.webp` |
| War Factory da China | `docs/concept/buildings/china_war_factory.webp` e `china_war_factory_sheet.webp` |
| Oficina do GLA | `docs/concept/buildings/gla_war_factory.webp` e `gla_war_factory_sheet.webp` (aprovada "por enquanto": armazém fechado à esquerda, casa e gaiola de armas à direita, rampa sob a rede de camuflagem) |

---

### A base da USA (11/10/2026): ainda sem prédio novo aprovado

Cena da base inteira, com a War Factory aprovada anexada (`captures/concept/43_usa_base/`, notas em `NOTAS.md`).
O método da cena manteve a War Factory igual nos dois geradores, mas:
- **O Felipe: "ambos bem broxantes".** Nenhum prédio novo (Centro de Comando, Supply Center, usina) saiu icônico como os do ZH. Ficam como arte conceitual; o desenho volta **bem depois**, com estudo próprio.
- **O acabamento-alvo é o do Gemini:** texturas com cara de jogo. O GPT detalhou tanto que ficou ruim.
- **Direção aprovada pras unidades:** o Chinook do Gemini e o Dozer do GPT.

## 3b. Estudo: o que faz um prédio do ZH ficar na memória (13/10/2026)

O Felipe deixou o desenho da base pra "bem depois, com estudo próprio do que torna um prédio icônico".
Este é o estudo, feito sobre os prédios do Generals/Zero Hour que todo jogador lembra.

| Facção | Prédio | A assinatura (o que se lembra) | Como lê de cima |
|---|---|---|---|
| USA | Command Center | o radar grande girando em cima do bloco blindado, o heliponto | um círculo grande (a antena) no meio de um retângulo |
| USA | Cold Fusion Reactor (usina) | o reator cilíndrico com anéis | círculos concêntricos |
| USA | Supply Center | o heliponto com o Chinook pousando e decolando com a carga | o "H" e o helicóptero indo e vindo |
| USA | Barracks | o hangar arredondado com sacos de areia e a bandeira | meio-cilindro com nervuras |
| USA | Strategy Center | o domo blindado com os telões de plano de batalha | um domo, a maior bola da base |
| USA | Particle Cannon | a antena-canhão apontada pro céu | um disco grande |
| China | Command Center / Nuclear Reactor / Propaganda Center | o telhado de pagode; as torres de resfriamento com vapor; os alto-falantes e a estrela vermelha | as camadas do telhado; dois círculos fumegando; a estrela |
| GLA | Command Center / Palace / Scud Storm / Supply Stash | o pátio de adobe com lona; o palácio de cúpula; os trilhos de lançamento; o monte de caixas | pátios abertos, cúpula, trilhos, pilhas |

**O que se repete (as regras):**
1. **Uma palavra por prédio.** Cada um é UM objeto que todo mundo conhece: radar, reator, heliponto,
   hangar, domo, canhão. A pergunta é "qual objeto é este prédio?", e a resposta tem de ser uma
   palavra só, antes de qualquer detalhe.
2. **A assinatura é redonda num mundo de retângulos.** Quase todas são círculos vistos de cima
   (antena, reator, domo, torre de resfriamento, heliponto). Num chão de lotes retangulares, o círculo
   é o que o olho acha primeiro. Isso vale porque a câmera do RTS vê de cima.
3. **A assinatura se mexe.** O radar gira, o Chinook pousa e decola, o reator pulsa, a torre solta
   vapor, a bandeira tremula, o alto-falante chia. O movimento faz o prédio parecer vivo e aponta o
   olho pra ele.
4. **Hierarquia de tamanho.** O Centro de Comando é o maior e o mais "importante"; as defesas são
   pequenas. Dá pra contar a base de longe pela altura das coisas.
5. **A função aparece:** a porta de onde a unidade sai, o pátio de reparo, o heliponto da carga.
6. **Três estados de dano** bem visíveis: inteiro, danificado (fogo e fumaça numa parte) e quase
   destruído (estrutura à mostra, fogo grande). No ZH, o jogador lê a vida do prédio pelo fogo, não pela
   barra.
7. **A cor do jogador sempre nos mesmos objetos** (na USA, a lona azul e as faixas, como no tanque).

**Como erramos na base v1 (11/10):** os prédios novos saíram como "um prédio com muitas coisas", e
não como "um objeto". O Centro de Comando era um bloco grande com detalhe espalhado; nada nele era a
palavra.

**A proposta pra USA (uma palavra e o objeto que se mexe):**

| Prédio | A palavra | O que se mexe |
|---|---|---|
| Centro de Comando | o radar (antena grande girando) | a antena gira; o Dozer sai da garagem |
| Usina | o reator (cilindro com anéis) | o vapor branco sobe; os anéis acendem com a carga |
| Quartel | o hangar arredondado | a bandeira; os soldados saem pela porta |
| Supply Center | o heliponto | o Chinook pousa e decola com a carga |
| Tech Center | o domo blindado | o domo abre uma fresta; as antenas giram |
| Aeroporto | a pista com o hangar | os aviões taxiando |
| Orbital Lance | o disco do uplink | o disco aponta pro céu e carrega |

**A ordem (uma de cada vez, regra da cena):** o Centro de Comando primeiro, porque é o maior e é
dele que sai o Dozer. Depois a usina e o Supply Center, que já aparecem na partida de hoje.

O prompt da folha de ideias do Centro de Comando está em `captures/concept/44_usa_buildings/PROMPTS.md`.

**21/10: a usina vem primeiro** (pedido do Felipe, começando a paleta que falta da USA). É o prédio mais repetido
da base (4 a 6 por partida), então define o desenho da base mais que qualquer outro. Folha de ideias com 4
assinaturas (o reator com anéis, o anel de fusão deitado, a torre baixa com o ventilador gigante, o tambor com 4
contêineres plugados) em `captures/concept/47_usa_power_plant/PROMPTS.md`. **Proibido na usina da USA:** a torre
de resfriamento em ampulheta (é da China).

## 4. Como fazer uma construção nova

1. **Folha de ideias (GPT):** 4 designs, cada um com **um elemento-assinatura
   diferente que você define no prompt**. Não deixe o gerador inventar a
   assinatura. Anexe a ficha do tanque da facção (pra escala e estilo) e um
   prédio aprovado da mesma facção (pra manter a linguagem).
2. **O Felipe escolhe**, quase sempre misturando peças de dois ou três
   designs. Anote o que ele gostou de cada um.
3. **Versão final, imagem única:** a mistura descrita peça por peça ("from
   design 2 keep...", "add from design 4...").
4. **Ajustes, um por vez**, recriando ("Recreate the design of the attached
   image as a new image") em vez de editar. A edição do GPT derruba a
   resolução e quase não muda nada.
5. **Ficha técnica no Gemini** (prompt 5.4), a partir da imagem aprovada.
6. Depois de aprovado: copiar a imagem e a ficha pra `docs/concept/buildings/`
   e atualizar a seção 3 deste guia.

---

## 5. Prompts

### 5.1 Folha de ideias (o que acertou na v3)

```
Concept art sheet for a modern real-time strategy game, 16:9: four <building> designs for the <faction> faction, labeled 1 to 4. Design them like the iconic buildings of classic 2000s strategy games, modernized: a compact diorama on a rectangular footprint, built from a few big, simple, chunky shapes with a strong silhouette, clean and readable from the high game camera, not cluttered with small props and not science fiction. Every design has: <the faction's fixed elements from the bible>, and signs of life (<smoke / fans / glow>). Team color only as <the faction's team color carrier>. Each one is built around a different signature element:
1. <signature 1>
2. <signature 2>
3. <signature 3>
4. <signature 4>
<faction materials and colors>. The attached <faction> tank next to each design for scale. High game camera (about 55 degrees looking down), dusty desert ground, golden hour. Photorealistic AAA game asset renders. No other text.
```

### 5.2 Versão final (mistura)

```
Final design of the <building> of the <faction> faction for a modern real-time strategy game: a single building in one 16:9 image. Use design <N> of the attached sheet as the base and keep it almost exactly: <list the parts he liked>. Add from design <M> of the same sheet: <the part he liked>. Same style, materials and lighting; high game camera (about 55 degrees looking down), golden hour, dusty desert ground. The <faction> tank <rolling out of the door> for scale. Photorealistic AAA game asset render. No text.
```

Com duas folhas (de geradores diferentes): "Use design 4 of image 1 as the
base... Bring in the arrangement of design 4 of image 2... Keep the
photorealistic materials, detail and lighting of image 1, not the toy-like
look of image 2."

### 5.3 Ajuste de tamanho e limpeza (o que o deixou satisfeito)

```
Recreate the design of the attached image as a new image. Same style, materials, lighting and camera; keep beautiful surface detail, but not exaggerated, and not low-poly. Changes:
- <a mudança de forma, se houver>
- Fewer panels, bolts, seams, pipes and small boxes on the walls: larger, cleaner surfaces.
- More compact and lower: the whole building is only about three tank lengths wide.
- Remove most of the loose crates and props: keep at most <N>.
Keep <os elementos-assinatura>. High game camera (about 55 degrees looking down), golden hour, dusty desert ground. Photorealistic AAA game asset render. No text.
```

### 5.4 Ficha técnica (no Gemini, com a imagem aprovada anexada)

Erros que apareceram e a frase que corrigiu: forno apagado ("The furnace
mouth glows bright orange with fire in every view where it is visible"); porta
vazada mostrando o fundo ("The main door opens into a lit interior hall; it is
not see-through"); vista repetida no lugar da lateral ("5 different views, no
view repeated ... side elevation seen from the left side").

```
Model sheet for a real-time strategy game building: the <building> from the attached image. Keep the design, shapes, materials, colors and details exactly the same; do not redesign or simplify it, and not low-poly. Show the same building in 5 views on a plain neutral grey background with even studio lighting: straight top-down view (as the strategy camera sees it), game camera view from the front (about 55 degrees looking down, three-quarter, like the attached image), game camera view from the back (the opposite side, about 55 degrees looking down), front elevation and side elevation. Include its tank next to it in the top-down view for scale. Photorealistic AAA game asset render. No text.
```

---

## 6. Como avaliar um prédio

- **Tem um elemento-assinatura** que se reconhece de cima e é da facção?
  Tampe a cor: ainda dá pra saber de quem é pela forma?
- **Tamanho:** quantos tanques cabem na largura? No ZH eram 3 ou 4. As finais
  ficaram em 5 ou 6 e passaram, mas é o limite. Detalhe miúdo (janelinhas,
  grades, caixas) faz o prédio **parecer** maior do que é.
- **Altura:** prédio alto esconde as unidades que passam atrás, com a câmera
  a 55°. Torre e chaminé tudo bem; o corpo do prédio tem de ser baixo.
- **Entulho:** caixas, postes, tambores e botijões soltos são objetos
  separados no jogo (colocados quando o mapa é montado). Na imagem de
  conceito, poucos ou nenhum.
- **Tom da facção:** USA clara, China escura, GLA areia. De longe, o prédio
  tem de bater com as unidades.
- **A unidade sai de um jeito que faz sentido:** porta no chão (USA e China),
  rampa vindo de baixo da terra (GLA). O GPT às vezes "esquece" e põe a
  unidade saindo de uma parede ou pendurada num guindaste.
- **Nível de detalhe:** acabamento bonito (desgaste, placas, luzes), sem
  exagero. Nunca low poly: o Felipe repete isso toda vez.

---

## 7. Geradores, no caso de prédio

- **GPT: pra criar e ajustar o prédio.** Com a fórmula no prompt, ele "bota
  pra quebrar". Mas **erra nas fichas** de várias vistas.
- **Gemini: pra ficha técnica** (as 5 vistas a partir da imagem aprovada),
  onde ele é perfeito, igual nos tanques. Pra criar prédio ele não serve: sai
  casa de bonecas low poly (o Felipe achou que parecia Red Alert), e as
  edições dele não mudaram nada.

---

## 8. A decidir

- **Cor do jogador nos prédios (decidido pelo Felipe, 05/10/2026):** a cor
  forte de cada prédio **é** a cor do jogador: azul na USA, vermelho na China,
  verde no GLA. Não dá pra manter a cor da facção e pôr a do jogador junto, então
  essas peças trocam inteiras; por padrão o jogador usa a cor da facção. A
  estrela da China também troca (no prédio e no tanque). Testar com recoloração
  em 8 cores, como nos tanques.
- **Próximas construções:** Centro de Comando, quartel, usina de energia,
  centro de suprimentos, aeroporto, superarmas. Mesma receita: fórmula da seção
  1, bíblia da seção 2, folha de ideias com 4 assinaturas definidas por nós.
