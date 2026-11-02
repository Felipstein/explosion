# O Command Center e a usina da USA, do zero (item 38 do `TAREFAS.md`, 01/11/2026)

O Felipe, 01/11 14h10, vendo as versões B dos agentes: "command center ficou muito básico, parecido com a anterior, quero ideia
nova, de arrepiar, foda, quero bem feito [...] e bem pensadas"; "quero uma outra versão do power plant, porque aí não parece, não tem
cara de power plant, quero outra variação, outra coisa bem feita". As reprovadas: `docs/aprovados/modelos/2026-11-01_usa_*_B_REPROVADO.jpg`.

## 1. Por que as duas falharam

- **O Command Center** seguiu a "palavra" de 09/10 (`docs/CONCEITO_CONSTRUCOES.md`: "o radar grande girando em cima do bloco
  blindado"): de cima, um retângulo com um disco. É o desenho do CC de 13/10 (reprovado, "tá horrível") com mais detalhe. Ideia
  velha bem acabada continua ideia velha.
- **A usina** seguiu "o reator cilíndrico com anéis" (o Cold Fusion Reactor do ZH): de cima, círculos concêntricos, que lêem como
  tanque de água, silo ou radar. Não há nada nela que diga **eletricidade**. E o ícone universal de usina, a torre de
  resfriamento com vapor, já é da China (`CONCEITO_CONSTRUCOES.md`, a tabela: "as torres de resfriamento com vapor").

## 2. A palavra nova

**Command Center: o PENTÁGONO.** O QG das forças americanas tem forma, e o mundo inteiro a reconhece de cima: cinco alas em volta de
um pátio. Nenhum RTS fez a base da USA nascer de um pentágono. De cima, nenhuma outra peça do jogo tem cinco lados. É "autêntico"
(o Pentágono existe e é o comando dos EUA) e "icônico" (uma forma só, lida em 1 segundo e a 100 m).
- As 5 alas baixas e blindadas, de concreto com as faixas da cor do jogador, em dois anéis (a fachada de fora e o anel de dentro,
  como o real) e o vão do pátio no meio.
- No pátio, a **torre de comando com o radar de antena plana de 4 faces** (os hexágonos do radar AN/SPY-6 dos destróieres e do
  AN/TPY-2 do THAAD, o radar americano de hoje): a peça mais alta, que sai do vão e dá a sombra longa. Em cima dela, o **domo do
  link de satélite** girando devagar.
- Uma ala com o **heliponto no teto**; a ala da frente (+X) com o **portão blindado da garagem do Dozer** (a faixa de saída do
  SimCore); a bandeira no mastro (a física das bandeiras); as antenas de chicote nas quinas; os HESCO/sacos no pé, a cerca baixa.
- O lote: 25,28 × 29,52 m; o pentágono com ~24 m entre as faces, a ponta pra trás (−X) e a face reta pra frente (o portão).

**Usina: a SUBESTAÇÃO com a TORRE DE ALTA TENSÃO.** O símbolo da eletricidade que todo mundo conhece, que não é da China: a torre
de treliça com os braços e os cabos pendurados, e o pátio dos transformadores com os isoladores. A fonte de energia é americana e
de hoje: o **microrreator móvel** (o Project Pele do Departamento de Defesa, um reator que cabe em contêineres) dentro do casco
blindado, com a janela do núcleo acesa em azul.
- A torre de alta tensão de treliça, a peça mais alta do lote, com os 3 cabos descendo em catenária até o pátio. A sombra dela no
  chão desenha a torre (no RTS a sombra lê tanto quanto a peça).
- O pátio dos transformadores: 2 transformadores grandes com as aletas do radiador e as buchas dos isoladores (os pratos em pilha),
  os seccionadores, a grade da cerca com a placa de perigo e o cascalho do pátio (o chão de subestação).
- O módulo do reator: o casco de contêineres blindados, a janela do núcleo acesa e pulsando, as caixas de ventilação, os cabos
  grossos no chão até os transformadores.
- O lote: 9,28 × 12,64 m (pequeno: a torre e o pátio têm que caber).

## 3. As regras do jogo que valem

- **Jogabilidade antes do realismo:** a torre de alta tensão é treliça (vazada: não tapa unidade); a torre do CC fica no pátio, no
  meio, e não passa de ~16 m.
- **A assinatura em 1 segundo de cima**, na câmera do jogo, ao lado do tanque (a escala).
- **O detalhe B** (`model_run_detail.py`: as máscaras, as chapas, as quinas), os 3 estados de dano (`model_damage.py`), a
  superfície do tiro, as luzes que apagam sem energia.
- **A tinta da USA** (o item 40): os prédios levam a mesma campanha do deserto (o pó no pé, a junta escura), medida contra a War
  Factory do alvo, que é a única aprovada da `r1_raid_slabs`.

## 4. Fontes

- O Pentágono (a planta de 5 anéis de alas em volta do pátio): https://en.wikipedia.org/wiki/The_Pentagon
- O radar AN/SPY-6 (as faces planas hexagonais): https://en.wikipedia.org/wiki/AN/SPY-6 ; o AN/TPY-2: https://en.wikipedia.org/wiki/AN/TPY-2_transportable_radar
- O Project Pele (o microrreator móvel do DoD): https://en.wikipedia.org/wiki/Project_Pele
- A subestação (transformador, buchas, seccionadores, pórtico): https://en.wikipedia.org/wiki/Electrical_substation
- A regra de "uma palavra por prédio" e a tabela das facções: `docs/CONCEITO_CONSTRUCOES.md` §3b.

## 5. O Command Center: o do Zero Hour, quase copiado (01/11, 18h40)

Depois das 4 tentativas reprovadas (o A, o B, o Pentágono e as duas folhas de conceito do Atelier: "um lixo, parece jogo de criança, não
encaixa com o jogo; desiste dessa IA, pensa você, faz você, olha o C&C ZH, nem que copie quase tudo"), o Command Center sai do original
do ZH (a referência: `docs/concept/buildings/ref_zh/zh_usa_command_center.jpg`, da wiki do C&C), no nível AAA das peças aprovadas:
- o galpão comprido do meio com o **teto curvo** de chapas claras e a faixa azul no beiral; o bloco baixo de entrada com a porta grande;
- **o hangar meia-lua** à direita, com **a estrela branca do Exército dos EUA num disco escuro** na empena e o portão (a saída do Dozer);
- **a torre de treliça alta com o prato grande do radar girando** à esquerda (a assinatura que todo mundo lembra);
- o bloco de rádio elevado atrás com as antenas de chicote e os ventiladores; a escada no teto; o mastro com a bandeira; a cerca de
  alambrado; a placa inclinada com o emblema; o piso só no contorno do prédio (a regra: nada de laje quadrada no lote inteiro).

## 6. O Command Center ORIGINAL (01/11, 20h30): a gramática do ZH, as formas nossas

O Felipe, vendo o v4: "você aprendeu a icônica forma, né, tá vendo o que é icônico; agora obviamente você vai fazer um modelo novo pra
gente do zero, porque não se pode copiar de outro jogo". O v4 fica como estudo (`docs/aprovados/modelos/2026-11-01_usa_command_center_v4_ZH_forma_aprendida.jpg`).

**O que o do ZH ensina (a gramática):** poucas peças grandes, cada uma com uma função que se lê; um elemento ALTO e que GIRA (a torre de
treliça com o prato); o salão principal com um teto que NÃO é caixa (a abóbada em gomos); o volume da SAÍDA com o emblema grande da
facção (o hangar com a estrela); o bloco de rádio com o feixe de antenas; a bandeira; tudo assimétrico, com o vão entre as peças.

**O nosso (as formas e proporções próprias):**
- **A torre do radar:** não a treliça quadrada do ZH, mas o MASTRO TUBULAR de comunicação militar (o tubo cinza com os anéis e as
  plataformas de serviço, estaiado), com a ANTENA DE RADAR RETANGULAR de painel (a moderna, tipo AN/TPS-75, plana e inclinada) girando
  no topo: de cima, o retângulo que varre, não o disco.
- **O salão de comando:** o bloco baixo e largo com o TETO EM SERRA invertida (as águas inclinadas com as claraboias escuras viradas
  pro norte, o telhado de fábrica americano), a faixa azul no beiral; na quina, a SALA DE OPERAÇÕES em balanço, envidraçada escura em
  três lados, acesa por dentro.
- **O volume da saída:** o pavilhão de chapas com o PORTÃO SECCIONAL alto em +X (a saída do Dozer, 6,6 m), o grande DISCO AZUL-ESCURO
  com a estrela branca do Exército na empena acima do portão (o emblema que lê de longe), as faixas amarelas da saída no chão.
- **O bloco de rádio:** a casa de equipamentos com o feixe de antenas de chicote, as caixas de ar-condicionado, o gerador encostado.
- **O mastro com a bandeira dos EUA**, a cerca de alambrado num lado só, os HESCO e os sacos no pé, sem laje no lote inteiro.
