# Overlord (China, o pesado)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o tanque pesado: a ponta de lança da horda, o fim de jogo da China |
| Custo e tempo | $2 000, 20 s |
| Vida e blindagem | 1 100, tanque |
| Velocidade | 5,3 m/s (o mais lento dos tanques), giro do casco e da torre 90°/s |
| Visão | 39,5 m |
| Arma | 2 canos de 80 perfurante, rajada de 2 (0,3 s entre os dois), 2 s pra recarregar a rajada, alcance 46 m |
| Esmagador | nível 3: passa por cima de soldado, carro, poste e árvore |
| Requer | War Factory + Propaganda Center |
| Sai de | War Factory |

## 2. A ideia

**Um prédio que anda.** O tanque chinês gigante, a escapada de ficção que o mod Contra fez famosa: uma vez e meia o tanque normal,
dois canos lado a lado, esteiras largas como uma rua. Quando ele entra na tela, a batalha muda de assunto.

- **A palavra de cima:** os DOIS CANOS. O canhão duplo é exclusivo dele (nenhuma outra unidade do jogo tem dois canos lado a lado).
- **Inspiração real:** a escola soviética de tanque pesado (o IS-7, o T-10, o Objeto 279) e a blindagem modular chinesa de hoje; a
  cabeça é ficção, o material é real (aço, chapa aparafusada, borracha da saia).
- **Por que existe:** a China arrasa em massa, mas a massa dela é barata e frágil. O Overlord é o martelo que abre a defesa e
  segura a linha enquanto a horda passa. É caro (mais que dois tanques), lento e precisa da tecnologia: é decisão, não spam.

## 3. Como funciona

- **No jogo:** 2 tiros de 80 em 0,3 s, depois 2 s de recarga: 160 a cada 2,3 s (69,6/s no tanque). Mata um tanque USA (480) em 3
  rajadas, ~7 s. Pela conta de Lanchester (`docs/ELENCO.md` §8.1) ele vale o preço: 0,0191 contra 0,0178 do tanque USA.
- **O esmagamento:** o nível 3 passa por cima do que o tanque comum (nível 2) também passa, e a infantaria morre esmagada (200% do
  esmagamento). A ideia é a de abrir caminho por dentro da vila.
- **Forte contra:** tanque, prédio, defesa de base (a rajada dupla tira 160 de uma vez), infantaria no caminho dele (esmaga).
- **Fraco contra:** ar (não atira pra cima: o Comanche e o Raptor são o contra dele), soldado antitanque em grupo (3 Javelins
  tiram 120/s), artilharia (é lento demais pra fugir dela), ser cercado (a torre gira devagar).
- **O contra dele:** o Comanche (USA), o Rocket Buggy (GLA, de longe), o grupo de RPGs escondido na vila.
- **Ele é o contra de:** a linha de tanques, as defesas de base, a Technical e a Quad Cannon (morrem em uma rajada).

## 4. Em batalha

- **Temperamento: o rolo compressor.** Calmo, inevitável, sem pressa. Ele não corre; chega.
- **O ritmo:** BUM-BUM ... pausa ... BUM-BUM. A pausa é a hora de quem está apanhando reagir; o jogador sente o compasso.
- **O jogador bom** o põe na frente da horda, como escudo: os Type 99 atrás atirando, os Gattling Tanks do lado cuidando do ar e da
  infantaria. Nunca sozinho: sozinho, o Comanche o desmonta.
- **A IA** treina um quando tem o Propaganda Center e o dinheiro (ele entra na mistura da fábrica com peso 1, contra 2 do Type 99).
- **Sozinho e ferido:** segue atirando até o fim; é lento demais pra fugir, então a última rajada é a resposta.

## 5. A vida

- **Parado:** o motor a diesel em marcha lenta solta fumaça escura em pulsos pelas duas chaminés de trás; a torre faz uma varredura
  lenta de 15° pra cada lado a cada ~8 s; a antena balança.
- **Andando:** o chão treme (a câmera sente um pouco quando ele passa perto); as esteiras deixam o rastro mais largo do jogo; o
  estandarte vermelho pequeno na torre bate no vento.
- **Atirando:** os dois canos recuam um depois do outro (0,3 s), o casco inteiro balança pra trás com a rajada, a poeira do chão
  sobe na frente dos canos (o sopro da boca).
- **Ferido:** fumaça preta saindo da grade do motor; abaixo de 30%, fogo pequeno no motor e as chapas da saia faltando.
- **Pronto na fábrica:** sai devagar, a torre já virada pra frente; a lona vermelha dobrada atrás da torre.

## 6. O visual

- **De cima:** um retângulo largo e comprido (10,5 × 5 m), a torre enorme e blocuda no meio e os dois canos grossos saindo pra frente
  além do casco. Lê como "o maior" sem precisar de zoom.
- **Assinatura:** os dois canos lado a lado, cada um com a faixa vermelha (a cor do jogador).
- **Cores:** o verde-oliva escuro da China com a camuflagem digital em blocos (sempre escuro, ~0,30 de luminância de cima); a cor do
  jogador na lona dobrada atrás da torre, nas faixas dos canos e no estandarte pequeno.
- **Nunca:** lona grande caída, faixa pintada de carro de corrida, cruz vermelha, cores vivas.

## 7. Tiro, projétil, impacto, destruição

- **A boca:** dois clarões laranja curtos e grossos, um depois do outro; o anel de fumaça cinza saindo de cada boca; a poeira do chão
  levantada na frente (o sopro).
- **O projétil:** o perfurante quase não se vê (105 m/s no jogo): um risco curto, quente, branco-amarelado. Dois riscos seguidos.
- **O impacto:**
  - no tanque: a faísca forte, a chapa que pula, a marca preta;
  - no prédio: o pedaço de concreto que voa e a poeira;
  - no chão: a cratera pequena e a areia que sobe (a cratera A do jogo);
  - no soldado: pouco efeito (perfurante faz 10% em soldado); ele mata soldado é passando por cima.
- **A morte:** a maior explosão de veículo do jogo: o diesel e a munição dos dois canos. O fogo de combustível sobe primeiro, depois
  o estouro da munição joga a torre pra cima (ela cai do lado, virada). A carcaça fica muito tempo queimando, com a fumaça preta
  grossa (o diesel dos pesados, que o Felipe adiou pra China e a GLA: `project-diesel-adiado`).

## 8. O som

- **Seleção (variantes aleatórias):** o motor acelerando grave (o diesel pesado), o servo da torre girando com o rangido do metal
  grande, a escotilha do comandante batendo, o alto-falante do rádio chiando curto antes da fala.
- **As falas** (em inglês; voz grave, calma, de comandante que não precisa gritar; atrás, o eco do metal da torre):
  - selecionado: "Overlord, standing by." / "The mountain moves." / "Give the order." / "We crush what stands."
  - andando: "Advancing." / "Nothing stops us." / "Clear the road." / "Rolling."
  - atacando: "Both guns." / "Target the strongest." / "Open fire." / "Break them."
  - sob fogo: "Armor holding." / "Is that all?" / "Keep moving."
  - pronto: "Overlord, ready for war."
- **O corpo:** o diesel V12 em marcha lenta (graves, pulsando); as esteiras largas com o clank-clank-clank mais lento que o do
  tanque normal; o chão rangendo; o servo da torre com o motor elétrico grande e o rangido do anel.
- **A arma:** cada tiro é um estrondo grave com o "tum" no peito; os dois tiros a 0,3 s viram o BUM-BUM que é a cara dele; depois o
  carregador automático (o clangor metálico da culatra, duas vezes) durante a recarga.
- **A morte:** o estouro abafado do combustível, o segundo estouro da munição (mais seco), o metal da torre caindo pesado, o fogo
  crepitando com o diesel.
- **A cara da China no som:** metal pesado, o rádio com o alto-falante, o eco.

## 9. Ideias pra depois

- **Os acessórios do Overlord** (a ideia do ZH, melhor): uma torre a mais em cima, à escolha, comprada depois de pronto:
  - o bunker de soldados (leva 5 e atiram de dentro);
  - a metralhadora gattling antiaérea (cobre o ponto fraco dele);
  - a torre de propaganda (cura devagar e acelera quem está em volta).
- **Esmagar veículo vivo:** passar por cima da Technical, do Humvee, da carcaça (a simulação ainda não tem; o nível 3 ficaria pra
  isso).
- **O tremor:** o chão que treme no passo dele tirando a mira de quem está perto (a infantaria erra mais).

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#china_overlord` (feito no fim, com o modelo provisório como anexo de forma).
