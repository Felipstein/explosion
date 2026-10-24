# O tanque esmaga e derruba (v3, pedidos de 16/10)

Os pedidos 2, 3 e 5 de 16/10 de manhã (`docs/BASE_JOGAVEL.md`):

- o tanque passando por cima dos carros civis e esmagando, "bem bonito e de arrepiar";
- o tanque derrubando árvores, palmeiras e postes;
- o arbusto amassado e a pedra pequena quebrada.

A regra do Felipe: "construído pensando num jogo AAA que vai deixar arrepiando".

**Dono:**
- **O jogo:** a regra na simulação, em ponto fixo; o evento; a passagem no caminho; o provisório do desenho.
- **O Look:** o carro amassado (a malha e o material) e a vegetação deitada.
- **O WorldForge:** as árvores, os postes e os fios com física, e a queda.
- **O Fluxo e a Arte:** o vidro, a poeira, a faísca do metal, o arco e o clarão do fio.

## 1. Como o Zero Hour faz (o fonte, `newcnc/GeneralsGameCode/GeneralsMD`)

### Quem esmaga o quê

**Os níveis** (`ThingTemplate.cpp:266`): `CrusherLevel` × `CrushableLevel`, inteiros.
- **A regra** (`Object::canCrushOrSquish`, `Object.cpp:1153`): esmaga quando o nível de esmagador é **maior** que o de esmagável. Nunca esmaga aliado.
- **Os valores do INI de varejo** (da memória, sem o INIZH.big aqui): esmagador 1 = infantaria, 2 = árvores, 3 = veículos; esmagável 0 = infantaria, 1 = árvores e carros civis, 2 = veículos.
- **Os níveis das unidades:** o Crusader e o Battlemaster têm 2; o Overlord, 3.

### O carro esmagado

`PhysicsBehavior::checkForOverlapCollision` (`PhysicsUpdate.cpp:1482`) e `CrushDie` (`CrushDie.cpp:163`):
- **Três pontos ao longo do eixo do carro:** a frente, o meio e a traseira, a meio comprimento do centro.
- **O momento:** o carro é esmagado quando o tanque **já passou** do ponto mais perto da linha dele.
- **O resultado:** o carro morre e troca o modelo para `FRONTCRUSHED` / `BACKCRUSHED`. O meio amassa os dois.
- **O tanque não desacelera:** o esmagar sai antes de qualquer força. Só no desenho a lagarta sobe e desce por cima (`Drawable::calcPhysicsXformTreads`, `Drawable.cpp:1623`: o objeto achatado tem meia altura, mais um tranco de pitch na saída).

### A árvore e o poste que caem

`ToppleUpdate.cpp`:
- **O que derruba:** só a colisão com esmagador de nível ≥ 2. A explosão comum não derruba; só a nuke, a onda e o script.
- **A direção:** do centro do veículo até a árvore (radial, 377–380). A velocidade do veículo só dá a força: a velocidade angular inicial é 0,2 × a do veículo, e a aceleração 0,01 × (rad/quadro).
- **A queda:** cai até 87,2° e quica a −0,3 da velocidade até parar (menos de 0,01). Na prática bate no chão em ~0,25 s e para em ~1 s.
- **O toco e o fim:** nasce o toco (`StumpName`); `KillWhenFinishedToppling` remove a árvore do jogo.
- **Os postes:** usam o mesmo módulo. O objeto chato gira antes de cair, pra deitar de lado (o comentário cita os postes, 182–207). **Fios não existem no ZH.**

### O caminho e a rede

- **No ZH, árvore e carro não são obstáculo pro caminho** (`AIPathfind.cpp:4389`: só prédio imóvel entra).
  - Os tanques atravessam e esmagam. Os outros veículos passam por dentro, porque objeto sem IA não empurra ninguém (`AIUpdate.cpp:1440`).
  - A cerca é a exceção: a célula dela só fica livre pra quem esmaga (`crusherGround`).
- **Tudo isso roda na lógica, igual em todos os PCs** (o xfer salva o estado).
  - No multiplayer as árvores são sempre lógicas, "so it syncs properly" (`GameLogic.cpp:1847`).
  - Em single player, as árvores otimizadas caem só no cliente.

## 2. O que os outros fazem (de memória; a 2ª opinião confere com fonte, seção 5)

- **Company of Heroes:** o tanque derruba cerca, sebe e árvore pequena, que ficam no chão.
- **World of Tanks:** a árvore e o objeto destrutível caem pra longe de quem bateu; o tanque perde velocidade no choque com os objetos grandes.
- **A física** (60 t contra um carro de ~1,5 t): o tanque sobe pela frente, a lagarta achata o teto e o tanque desce com um tranco. O carro fica mais baixo, torcido pro lado da lagarta que passou.

## 3. A nossa versão (o ZH como esqueleto, e onde passa dele)

**A regra** (ponto fixo, na simulação, igual em todo PC):

- **Os níveis do ZH:**
  - o tanque tem esmagador 2; o caminhão e o Dozer, 1;
  - o carro civil, a palmeira, o poste e a pedra pequena têm esmagável 1;
  - a caixa, o barril, o pneu e as peças pequenas da vila têm esmagável 0.
  - Ou seja: o tanque esmaga tudo isso; o caminhão e o Dozer só amassam as peças pequenas.
- **O contato:** o objeto (um círculo) dentro da caixa do casco (o comprimento e a largura do tanque, girados com ele), com o veículo andando.
- **O carro:** amassa a frente, a traseira ou os dois, pelos 3 pontos do ZH. A segunda passada amassa o resto. Fica no mapa achatado (as marcas que contam a batalha), sem bloquear ninguém.
- **A árvore, a palmeira e o poste:** caem na direção **do centro do tanque até o objeto, mais a frente do tanque** (o radial do ZH puxado pro rumo dele, pra não cair de lado na batida de raspão). A força da queda vem da velocidade do tanque (o 0,2 do ZH). Ficam deitados no mapa; a árvore não some (o Felipe, 13/10: "árvore não some").
- **A pedra pequena** (menos de 1 m) quebra. A de 1–2 m o tanque passa por cima: só no desenho, como o carro achatado do ZH. A de mais de 2 m segue bloqueando.
- **O peso:** o ZH não freia; nós freamos de leve. Esmagando o carro, o tanque vai a 70% por 0,5 s; derrubando a palmeira, a 85% por 0,3 s. É o "vende o peso" do World of Tanks, com números que dão pra mudar no A/B; poste e caixa não freiam.
- **O caminho:**
  - o carro e a palmeira deixam a célula **fechada só pra quem não esmaga** (a célula "esmagável", como o `crusherGround` do ZH);
  - o tanque passa por cima no caminho; o caminhão e o Dozer contornam;
  - derrubado ou achatado, a célula abre pra todos;
  - o poste e as peças pequenas não fecham célula nenhuma (o poste tem 30 cm).
- **O evento:** `PropCrushed` e `PropToppled`, com o objeto, o ponto, quem foi e a direção e a força da queda. É o desenho que faz o resto.

**O desenho:**
- **O provisório é do jogo** (os donos refazem):
  - o carro achatado e torcido pelo lado da lagarta;
  - a palmeira e o poste caindo pelo ZH (a aceleração, os 87° e o quique de −0,3) e ficando no chão;
  - o fio arrebentado com faísca;
  - a caixa e o barril amassados;
  - o tanque subindo e descendo por cima do carro e da pedra média (o tranco do ZH).
- **O final:**
  - **Look:** o carro amassado de verdade (a malha deformada pela lagarta, o vidro quebrado) e a vegetação deitada na trilha;
  - **WorldForge:** a queda com física (Chaos) e o fio que balança e arrebenta;
  - **Fluxo e Arte:** o arco elétrico, as faíscas e o clarão, pelo EmberGen ou pelo FXLab.

## 5. A segunda opinião (16/10, o revisor) e o plano revisado

O que mudou depois da revisão (com as linhas do código que ela apontou):

- **Na simulação, só o que muda o caminho: os carros e as palmeiras.**
  - O resto fica só no desenho: o poste (30 cm, não fecha célula), as ~600 peças pequenas, as pedras, os arbustos e os fios.
  - O desenho calcula a partir das posições do passo (iguais em todo PC) e respeita a névoa.
  - O fio e a queda com física (Chaos) do WorldForge não são determinísticos de qualquer jeito.
- **As palmeiras:**
  - saem das instâncias de DatePalm do mapa, ordenadas pela posição;
  - a montagem vai inteira pela rede e pelo replay (WriteSetup), então ninguém precisa recontar;
  - fecham só a célula do tronco, não mais a copa;
  - **o carro:** as células com o centro dentro da caixa do carro + 0,5 m.
- **O contato do carro:**
  - os 3 pontos do ZH ficam a ±¼ do comprimento (não ½);
  - o ponto é esmagado quando entra **na caixa do casco**, com o tanque se mexendo, inclusive girando no lugar ou sendo empurrado (a regra do ZH, pelo centro do tanque, deixava a batida de raspão atravessar o carro);
  - o lado da lagarta sai do lado do ponto no casco;
  - o carro abre a célula já no primeiro esmagar: a carcaça que morre em cima não trava.
- **O caminho** (a célula esmagável):
  - `CellPass::Crushable` e a segunda folga; a mobilidade de cada unidade num `MobilityOf` só;
  - no grupo misto (tanque com caminhão), a folga é a maior e o "esmaga" só vale se todos esmagam;
  - um contador de objetos de pé por célula;
  - o prédio que some devolve a célula como ela está agora, não como era na largada;
  - abrir a célula não manda ninguém refazer o caminho, porque o caminho só fica mais curto;
  - o validador do replay aceita o valor novo.
- **O freio:**
  - o usa_mbt acelera 263 m/s², então o freio é um **teto** na velocidade que volta em rampa;
  - **no carro:** 85% voltando a 100% em 0,5 s (pela física, o carro tira 5–10% da energia do tanque; 85% já exagera, pra ler); o A/B testa 75/85/95;
  - **na palmeira:** 95% por 0,2 s;
  - **o peso:** quem vende é o tranco no desenho.
- **A queda:**
  - **a direção:** a normal da face do casco que bateu + 0,5 × a frente do tanque. A simulação procura o rumo livre de prédio (±22,5°, ±45°… até ±90°) e passa o comprimento livre.
  - **o tempo:** a fórmula do ZH estava com unidade errada (a dele é ~0,3 m). O desenho usa a da vara que cai, com o empurrão pela velocidade do tanque; o A/B em vídeo testa 0,7/1,2/1,8 s.
- **Os níveis:** tanque 2, Dozer 2 (o trator de esteira não contorna palmeira), caminhão 1; carro e árvore 1.
- **A névoa:** a queda e o esmagar só aparecem quando a célula está à vista. O WoT faz o contrário (o inimigo vê a árvore cair), e isso fica como pergunta pro Felipe.
- **A copa deitada não cobre unidade:** fica achatada no chão.

## 4. Os números (pra conferir no jogo)

- **A caixa do casco do usa_mbt:** 7,9 × 3,7 m (meio lado 3,95 × 1,85 m).
- **O carro civil:** ~4,6 × 1,8 m; o achatado fica a 45% da altura.
- **A palmeira:**
  - o tronco é um círculo de 0,4 m;
  - cai a 87°;
  - a velocidade angular inicial é 0,2 × a velocidade do tanque em m/quadro (a do ZH, em rad/quadro).
- **O poste:** 8 m, raio 0,15 m (o village.json).
- **O freio no carro:** 70% por 15 passos.

## 6. O caminhão e o Tomahawk derrubam pelo choque (v4, 23/10 à noite, simulação 20)

O pedido: os de roda derrubam o poste e a palmeira como o tanque, por uma regra física, não por exceção.
- **A regra** (`World.h`, `KnocksOver`): o veículo derruba quando a energia do choque, ½·m·v², passa da que o objeto absorve até cair. A massa é o `mass_t` do visual (agora também da simulação). Estimativas [E]: o poste de madeira de 30 cm parte com ~40 kJ; a palmeira de ~35 cm na areia tomba com ~60 kJ. O caminhão de 15 t derruba a palmeira acima de 2,8 m/s e o poste acima de 2,3; o Tomahawk de 30 t, acima de 2,0 e 1,6. O tanque e o Dozer seguem o nível do ZH (o `crusher_level`), em qualquer velocidade.
- **O caminho:** a célula da árvore virou classe própria (`CellPass::Toppleable`): aberta pra quem esmaga e pra quem derruba na velocidade de cruzeiro (o bit `ToppleBit` da mobilidade); a do carro segue só de quem esmaga (a roda não sobe no carro: o caminhão contorna). Na mesma célula, o carro de pé manda.
- **O poste** segue só no desenho (`CheckPoles`), com a mesma conta.
- **O toque lento** (revisão de 23/10): a conta vale na velocidade de cruzeiro, não na da hora. Ela diz *quem* derruba (o caminho abre a célula da árvore pra ele), e esse derruba em qualquer toque andando ou empurrado, como o tanque: o motor que o leva a cruzeiro empurra o tronco até cair. Pela velocidade da hora, o caminhão parado e empurrado pelo aliado (ou freado numa curva) entrava na célula aberta e atravessava o tronco de pé. A queda começa devagar quando o toque é devagar (a velocidade vai no evento). O teste: "caminhao parado empurrado contra a palmeira" (derruba no toque, nenhum casco dentro do tronco de pé). O poste do desenho, igual.
- Testes: "caminhao derruba" no simtest; as cenas `v4_caminhao_postes` e `v4_tomahawk_palmeira`.
