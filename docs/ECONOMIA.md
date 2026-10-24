# Economia e construção: o estudo

Pedido do Felipe (10/10/2026): Command Center com Dozer, economia de suprimentos, construção, energia e War Factory gerando tanque. A regra dele: "cada feature não se faz de cabeça; se faz com estudos, inspirações, como fazem hoje, qual a melhor ideia, algo de arrepiar, inovador, discussão".

Este arquivo junta o estudo:
- **Parte 1**, os números e as regras do ZH;
- **Parte 2**, como os RTS fazem hoje;
- **Parte 3**, as ideias novas e a crítica.

A decisão de cada regra sai da discussão com o Felipe e fica marcada aqui.

---

## Parte 1. O Zero Hour 1.04: números e regras

Tudo saiu dos INI originais do ZH 1.04 e do código GPL. De código, só leitura (clean-room): nada foi copiado, só números e regras.

**Fontes.**
- **INI:** `https://github.com/TheSuperHackers/GeneralsGamePatch/blob/main/Patch104pZH/GameFilesOriginalZH/Data/INI/`. Siglas:

  | Sigla | Arquivo |
  |---|---|
  | FB | `Object/FactionBuilding.ini` |
  | AV | `Object/AmericaVehicle.ini` |
  | AA | `Object/AmericaAir.ini` |
  | CV | `Object/ChinaVehicle.ini` |
  | CB | `Object/CivilianBuilding.ini` |
  | GD | `GameData.ini` |
  | MP | `multiplayer.ini` |
  | PT | `PlayerTemplate.ini` |
  | CS | `CommandSet.ini` |
  | LO | `Locomotor.ini` |
  | UP | `Upgrade.ini` |
  | AR | `Armor.ini` |
  | EV | `Eva.ini` |

- **Código:** `C:\Users\luisf\newcnc\GeneralsGameCode\`. Siglas: MD = `GeneralsMD/Code/GameEngine/Source/`, CORE = `Core/GameEngine/Source/`.

**Escala:** 1 u = 0,2633 m (`Game/Config/RTS/Units.json`, `docs/JOGO.md`).
- Vem do Crusader: `GeometryMajorRadius = 15` é meio comprimento, então 30 u = 7,9 m.
- `MajorRadius` e `MinorRadius` são meias medidas; `GeometryHeight` é a altura inteira (`MD/Common/System/Geometry.cpp`).

### 1. Skirmish

| Item | Valor | Fonte |
|---|---|---|
| Dinheiro inicial | **10 000** (opções 5 000 / 10 000 / 20 000 / 50 000) | MP:92-107; GD:440; `MD/Common/RTS/Player.cpp:438` |
| Começa com | **1 Command Center pronto e 1 Dozer** | PT:60-61 |
| CC | no waypoint `Player_N_Start`, pronto, virado a −135° (`PlacementViewAngle`) | `MD/GameLogic/System/GameLogic.cpp:548-603`; FB:1132 |
| Dozer | num anel de 0,7 a 1,3 × o raio da esfera do CC (≈ 17,6 a 32,7 m) | GameLogic.cpp:575-600 |
| Mortalha no começo | **não existe**: o mapa todo começa como "já visto" (`UseShroud = No`) | MP:38; GameLogic.cpp:1823 |
| Derrota | sem nenhum objeto vivo STRUCTURE + MP_COUNT_FOR_VICTORY. **Unidade não conta** | GameLogic.cpp:1710; `MD/GameLogic/ScriptEngine/VictoryConditions.cpp:328-356` |
| Contam (USA) | CC, Power Plant, Supply Center, War Factory, Barracks, Airfield, Strategy Center, Drop Zone, Particle Cannon. **Não contam:** muros, Patriot, Fire Base | KindOf em FB |
| Ao perder | as unidades que sobram são destruídas | VictoryConditions.cpp:207, 232 |

Inferência pelo código: a fundação em obra conta como prédio.

### 2. Prédios USA

| | Command Center | Power Plant | Supply Center | War Factory |
|---|---|---|---|---|
| Custo | 2000 | 800 | 2000 | 2000 |
| Tempo | 45 s | 10 s | 10 s | 15 s |
| Vida | 5000 | 800 | 2000 | 2000 |
| Energia | 0 | **+5** (+5 com Advanced Control Rods) | **−1** | **−1** |
| Pré-requisito | — | — | Power Plant | Supply Center |
| Tamanho (C × L × A) | 31,6 × 36,9 × 12,9 m | 11,6 × 15,8 × 12,1 m | 23,2 × 23,7 × 6,3 m | 27,9 × 31,6 × 10,5 m |
| Visão | ≈ 79 m | ≈ 52,7 m | ≈ 52,7 m | ≈ 52,7 m |
| Produz | Dozer | (upgrade) | Chinook | Crusader e outros |
| Grátis | Radar | — | **1 Chinook quando a obra termina** | — |
| Saída (u) | nasce (−18, 35), anda até (60, 35) | — | nasce (0, 0), anda até (44, 0) | nasce (−10, −30), anda até (53, −30) |
| Portão (abrir / aberto / fechar) | 1,5 / 3 / 1,5 s | — | — | 3,25 / 3 / 4 s |

Fontes da tabela: FB (CC 1132-1328, Power Plant 4814-4902, Supply Center 18710-18809, War Factory 24920-25022) e CS:696-794.

Mais:
- O CC tem armadura StructureArmorTough; os outros três, StructureArmor.
- O Supply Center não pode ficar perto do depósito: precisa de 20 u ≈ 5,3 m de folga.
- A War Factory conserta veículos.
- Na China: o reator dá +10 (1000 de custo, 10 s), e o Supply Center custa 1500 e dá 1 caminhão grátis.

### 3. Unidades

| | Dozer USA | Supply Truck (China) | Chinook (USA) | Crusader |
|---|---|---|---|---|
| Custo | 1000 | 600 | 1200 | 900 |
| Tempo | 5 s | 10 s | 10 s | 10 s |
| Vida | 250 | 300 | 300 | 480 |
| Caixas por viagem | — | **4** | **8** | — |
| Velocidade | 7,9 m/s | 10,5 m/s | 39,5 m/s (voa a ≈ 26 m) | 7,9 m/s |
| Tamanho | 7,9 × 5,3 × 3,9 m | 9,0 × 3,7 × 3,7 m | 10,5 × 3,2 × 3,2 m | 7,9 × 5,3 × 2,6 m |
| Visão | 52,7 m | 39,5 m | 79 m | 39,5 m |

Fontes: AV:1427-1801, CV:2003-2105, AA:2201-2328, LO.

### 4. Suprimentos

Os depósitos do mapa são imortais (CB):

| Objeto | Caixas | Some ao esvaziar? | Vagas na fila |
|---|---|---|---|
| SupplyDock | 400 (30 000 $) | não | ilimitadas |
| SupplyWarehouse | 400 | não | 9 |
| SupplyPile | 150 (11 250 $) | **sim** | 5 |
| SupplyPileSmall | 50 (3 750 $) | **sim** | ilimitadas |

**Valores e tempos:**
- Cada caixa vale **75 $** (GD:248). O caminhão leva 4 caixas (**300 $ por viagem**) e o Chinook leva 8 (**600 $**).
- Pegar no depósito leva **por caixa**: 1,0 s no caminhão, 1,2 s no Chinook.
- Entregar no centro leva **uma vez só**: 0,4 s no caminhão, 2,9 s no Chinook.
- A espera vem antes de cada ação (`MD/GameLogic/AI/AIDock.cpp:574-634`). No total: o caminhão fica 4 s carregando e 0,4 s descarregando; o Chinook, 9,6 s e 2,9 s.

**Como o caminhão escolhe** (`MD/Common/RTS/ResourceGatheringManager.cpp:108-250`):
- **Depósito:** o mais perto em **linha reta**, com caixas, não inimigo, com vaga, a até 700 u ≈ 184 m.
- **Entrega:** no **Supply Center do próprio jogador** mais perto, sem limite. O centro de um aliado não serve.
- **Clique do jogador:** o depósito ou centro clicado vira a "doca preferida".
- Com qualquer caixa a bordo ele vai entregar; vazio, vai buscar.
- **Fila:** só um caminhão é atendido por vez em cada doca (`DockUpdate.cpp:216-428`).
- **Começo:** o caminhão que sai do Supply Center começa a coletar sozinho (`SupplyCenterProductionExitUpdate.cpp:125-130`).

**Depósito vazio:**
- O Dock fica no mapa, vazio; o Pile some.
- Sem depósito ao alcance, o caminhão volta pra perto do Supply Center (sem centro, do CC) e espera.

### 5. Produção

- **O dinheiro sai inteiro quando entra na fila.** Sem dinheiro, não entra (`ProductionUpdate.cpp:422`, `BuildAssistant.cpp:1348`).
- **Cancelar devolve 100%.** Se a fábrica é destruída ou vendida, devolve a fila toda.
- **Fila:** máximo de **9** itens. Só o primeiro é produzido, e ter mais fábricas não acelera.
- **Tempo = BuildTime ÷ fator de energia** (`ThingTemplate.cpp:1565-1617`; GD:258-260). A razão é r = produção ÷ consumo:

  | r = produção ÷ consumo | Fator | Tempo |
  |---|---|---|
  | r ≥ 1 | 1,0 | normal |
  | 0,8 ≤ r < 1 | 0,8 | × 1,25 |
  | 0,5 ≤ r < 0,8 | r | × 1/r |
  | r < 0,5 | 0,5 | × 2 |
  | **produção 0 e consumo 0** | **0,5** | **× 2** |

  A última linha é um defeito do ZH (issue TheSuperHackers/GeneralsGameCode #1390): no começo da partida USA, sem usina, o Dozer leva 10 s em vez de 5.
- **Cuidado ao portar:** no ZH o progresso da unidade é recalculado a cada quadro, então ele salta quando a energia muda (o defeito da #1390). Na obra, o progresso soma por quadro.
- **Portão:**
  - a unidade só aparece depois que o portão termina de abrir; com ele ainda aberto (3 s depois da última), ela sai na hora;
  - primeiro Crusader: 10 + 3,25 ≈ 13,25 s;
  - depois de nascer, a unidade anda até o `NaturalRallyPoint`, mais 20 u, e então até o rally do jogador.

### 6. Construção com o Dozer

**Onde dá pra construir** (`BuildAssistant.cpp:914-1072`):
- **o centro da obra está à vista agora** (névoa não serve);
- o terreno é plano: no máximo 2,6 m de diferença;
- sem água nem penhasco, e a mais de 7,9 m da borda do mapa;
- sem prédio, objeto, inimigo ou amigo parado em cima (amigo livre sai da frente);
- fora da faixa de saída de 25 u de uma fábrica;
- o Dozer precisa ter caminho até lá;
- **não existe raio de construção**: dá pra construir longe da base, se o lugar estiver à vista.

**Dinheiro:** sai **quando a obra é posicionada**. A fundação aparece na hora, e o Dozer vai depois (`DozerAIUpdate.cpp:1700-1730`).

**Obra:**
- nasce com **1 de vida**;
- a cada quadro ganha 1/quadros do progresso e da vida máxima;
- o tempo só corre com o Dozer no ponto dele, na borda da obra, do lado mais perto (`DozerAIUpdate.cpp:502-543`);
- no fim, o Dozer se afasta ≈ 13 m;
- a energia do prédio só conta depois de pronto.

**Dozer:**
- tem uma obra por vez, e uma ordem nova deixa a atual parada como fundação;
- **outro Dozer retoma a obra parada**, mas só um trabalha de cada vez: dois não aceleram;
- **cancelar a obra devolve 100%**, em qualquer fase;
- conserta só prédios: 2% da vida máxima por segundo, de graça. Parado há 5 s, conserta sozinho o que estiver a até ≈ 39,5 m.

### 7. Energia

**Barra do HUD:** escala logarítmica, base 7. O preenchimento é a produção e o ponteiro, o consumo. As cores:
- verde quando sobram 5 ou mais;
- amarelo quando sobram de 0 a 4;
- vermelho quando o consumo passa da produção.

**Sem energia:** o radar desliga, o Patriot e o Particle Cannon desligam, e produção e obra ficam mais lentas pela tabela da seção 5. O EVA avisa no máximo a cada 120 s.

### 8. Mais números

- **Dano:** o tiro AP do Crusader faz 100% em prédio, Dozer e caminhão. Um Crusader sozinho leva ≈ 168 s pra derrubar o CC.
- **Cura do prédio:** 0,3% da vida por segundo, depois de 3 s sem dano, nunca durante a obra.
- **Vender:** devolve 50% (o Supply Center, 400). Leva 3 s e devolve a fila.
- **Limites:** não há limite de Dozers nem dos 4 prédios.

### Achados de passagem

1. **A visão que fica é mais curta aqui que no ZH.** O ZH de verdade usa `UnlookPersistDuration = 5000` ms (GD:442); o nosso SimCore usa 1 s, que é só o padrão do código.
2. **O skirmish do ZH não tem "nunca visto"** (`UseShroud = No`). O nosso tem: é decisão nossa.
3. **O Vanguard erra no reembolso.** `newcnc/docs/balance/zh-reference.md` diz 50%, mas isso é a venda; cancelar devolve 100%.

### O que não achou

- Qual depósito cada mapa oficial usa e se algum script muda as caixas.
- A arte e as medidas da barra de energia.
- A lista de POWERED da China.

**Os INI baixados** estão no scratchpad da sessão do jogo (`zhini/`).

As perguntas que o levantamento da Parte 2 deixou abertas já têm resposta no código do ZH (Parte 1):
- a produção desacelera com energia baixa (seção 5);
- dois Dozers não aceleram a mesma obra (seção 6);
- a pilha do depósito encolhe no desenho (`W3DSupplyDraw.cpp`);
- a fila tem 9 itens;
- não há teto de população.

---

## Parte 2. Como os RTS fazem hoje (levantamento, 10/10)

Fontes com link no fim do relatório do agente, guardado no journal da sessão do jogo. Aqui fica o resumo, com o que importa pra decidir.

**Coleta.**
- O ZH tem 3 coletores com cara própria:
  - o Chinook da USA, 600 por viagem, que voa;
  - o caminhão da China, 300;
  - o Worker da GLA, que constrói e coleta.
- As docas são finitas e atendem um coletor por vez, o que obriga a expandir.
- E há rendas que são a identidade da facção, todas alvos físicos no mapa:
  - Oil Derrick: 1000 na captura e 200 a cada 12 s;
  - Supply Drop Zone, cujo C-130 solta caixas que **o inimigo também pode pegar**;
  - Hacker, Black Market e o salvage da GLA;
  - o roubo de dinheiro do Black Lotus.
- O RA3 trocou o campo infinito por mina com 1 coletor ("resource gating").
- O Tempest Rising tem um coletor blindado que mostra a carga num medidor em cima, e a refinaria móvel da Dynasty.
- O SC2 tem saturação de 2 por bloco e metade dos blocos com 750, pra forçar a expansão.
- O Stormgate distribui os workers sozinho.
- O CoH só paga o setor ligado ao QG, e o setor cortado pisca no minimapa.
- O BAR/SupCom e o Iron Harvest fazem da carcaça renda (reclaim), cobertura ou bloqueio.
- O Dune tem a minhoca, que treme o chão antes de comer a colheitadeira.

**Construção.**
- No C&C clássico, o Construction Yard constrói dentro de um raio, e o MCV desdobra.
- No ZH, o **Dozer constrói em qualquer lugar do mapa**, o que permite base avançada e proxy.
- No RA3, cada facção expande de um jeito, e dá pra construir no mar.
- No AoE IV, o Landmark é uma escolha de idade que vira prédio no mapa.
- No Stormgate, o Quick Macro constrói sem selecionar o worker.
- O C&C 4 tirou a construção (Crawler) e foi rejeitado.

**Energia.**
- No ZH, cair a energia derruba radar e defesas.
- O Overcharge chinês dá mais energia e danifica o reator, que explode e contamina o chão.
- A GLA não usa energia.
- No Tempest Rising, a usina tem um modo distribuição que acelera a produção vizinha e danifica os prédios.
- No They Are Billions, a energia é a área onde se pode construir.

**Produção.**
- O ZH foi o primeiro da série com fila por fábrica, paga inteira na compra.
- No C&C clássico, a fila é compartilhada, paga aos poucos e pausa quando o dinheiro acaba.
- O SupCom 2 mudou por patch de "paga ao enfileirar" pra "paga ao começar".
- A interface converge pra produzir sem selecionar a fábrica: a sidebar clássica, as abas do Tempest, o Quick Macro do Stormgate.

**O que converge:**
- coleta automática, com coletor grátis junto do prédio;
- ponto de recurso finito, com saturação limitada;
- renda por ponto capturado;
- uma economia por facção;
- filas paralelas com painel global;
- ação de rotina só quando for escolha (o chrono ficou, o inject virou candidato a sair).

**O que morreu:**
- a pilha infinita de coletores;
- a rotina sem decisão;
- o RTS sem base (C&C 4 e o C&C cancelado de 2013, que era "economy-based multiplayer");
- a economia de fluxo no grande público: Chris Taylor disse "to our original fanbase, this strategy was a failure";
- e baixar o APM sozinho não salva um jogo, como mostrou o lançamento do Stormgate.

**Os momentos de arrepiar e o que os causa:**
- o MCV desdobrando;
- cortar a linha de suprimento (CoH);
- a carcaça como renda (a do comandante no BAR vale 1250);
- o C-130 soltando caixas que o inimigo rouba;
- o derrick capturado, com o "cha-ching";
- o roubo de dinheiro;
- a minhoca do Dune;
- o Overcharge e a explosão do reator.

O que eles têm em comum:
1. a economia é um **objeto visível no mundo**;
2. existe um instante de compromisso ou de risco;
3. **a perda de um é o ganho do outro**;
4. causa e efeito se leem na hora;
5. a recompensa vem com som e imagem.

**O que combina com o nosso jogo** (inferência do levantamento):
1. Manter o esqueleto do ZH (Dozer, doca finita, coletor grátis, construção em qualquer lugar, fila por fábrica paga na compra). Os fracassos foram se afastar dele.
2. Uma assinatura por facção na coleta.
3. A economia legível de cima, que a câmera de 37,5° favorece.
4. **A carcaça vira cobertura ou dinheiro**: é o ponto mais "nosso".
5. Energia como decisão, não imposto.
6. Rendas que puxam a briga pro meio do mapa.
7. Produzir sem selecionar a fábrica, sem ação de rotina.
8. Tudo na simulação, por causa do lockstep.
9. Coletor, carcaça e caixa precisam se ler de cima. A crítica do Tempest reclamou de unidades difíceis de distinguir.

---

## Parte 3. Ideias pro nosso jogo (rodada de ideias, 10/10)

**A tese:** não inventar outra economia. O laço do ZH já pede pouca tarefa repetitiva. O que só nós temos é um campo que guarda a batalha (carcaça que fica, fogo, névoa por time) e um visual tão rico quanto quisermos, desde que só leia a simulação.

**A. O núcleo pra base agora**
- **A0. O laço do ZH 1.04:**
  - o CC pronto e o Dozer, que só constrói enquanto trabalha;
  - depósito finito; caminhão de 4 caixas × 75;
  - usina; fila de 9;
  - o sistema nasce em dados: "coleta", "constrói" e "precisa de energia" são capacidades, assim o Worker da GLA e a facção sem energia entram depois;
  - detalhes do ZH que pouca gente lembra: o depósito abaixo de 35% de vida para de servir; o Dozer parado conserta.
- **A1. Toque 1: toda carcaça tem valor.**
  - O Dozer desmancha a carcaça **fria** (proposta: 25% do custo, ~8 s), em 3 estágios (cano, torre, casco baixo).
  - Cada estágio baixa a cobertura (o mesmo sistema da mureta).
  - **Carcaça queimando não se desmancha**: o fogo vira relógio da simulação.
  - A carcaça do caminhão guarda a carga.
  - **O dilema:** escudo na entrada da base ou dinheiro agora.
  - Inspiração: o reclaim do TA/SupCom/BAR, o CoH, o salvage do próprio ZH.
  - Riscos: bola de neve pra quem vence, compensada em parte porque o ataque que morre na porta alimenta o defensor; medir com IA contra IA. O chão queimado fica.
- **A2. Toque 2: a economia se lê no mundo.**
  - o depósito com ~10 blocos de paletes que somem com o estoque (o ZH já fazia, `W3DSupplyDraw.cpp`);
  - a caçamba com 0 a 4 caixas;
  - **a montagem visível na War Factory**: a ponte rolante baixa a torre no casco, e a velocidade da ponte e dos ventiladores é a velocidade da produção;
  - **sem energia, a base apaga**: as luzes morrem da usina pra fora.
  - **O momento:** de noite, derrubar a usina e ver a base inimiga apagar. Só lê a simulação: risco zero pro lockstep.
- **A3. Menos tarefa repetitiva:** botão de Dozer parado, fila de obras com Shift, caminhão que vai direto trabalhar.

**B. Ousadas, pra depois:**
- o Dozer empurra carcaça;
- rebocar e reaproveitar o tanque inimigo;
- uma assinatura por facção:
  - USA: Chinook, Drop Zone e prédio pré-moldado chegando de helicóptero;
  - China: Overcharge e o meltdown do reator como explosão herói;
  - GLA: sucata, Worker e buraco que se reconstrói;
- mureta ou gabião construído pelo Dozer;
- estrada que nasce do tráfego dos caminhões;
- a torre que voa vira sucata onde cai;
- reparo pago no pátio da fábrica;
- poço de petróleo neutro que queima.

**C. Armadilhas:**
- rede elétrica com cabos;
- mais de um recurso;
- combustível e munição realistas;
- cadeias de produção;
- desmanche forte demais;
- tirar o caminhão;
- manutenção por unidade;
- painéis de economia ao vivo;
- depósito que se regenera.

**Decisões do Felipe que a rodada levantou:**
1. o valor do desmanche e se só o Dozer desmancha;
2. o minimapa depende de radar e energia?
3. a IA de teste entra junto com a economia? Sem ela, o skirmish não testa o laço.

**O gargalo é a arte.** O núcleo pede 6 modelos novos sem conceito ainda: CC, Dozer, Supply Center, caminhão, usina e depósito. Os provisórios montados de formas simples seguram até lá.

---

## Parte 4. A revisão adversarial (10/10) e o núcleo proposto

**Veredito do revisor:** a tese está certa (manter o esqueleto do ZH), mas:
- o A0 misturava facções: prédios da USA com o caminhão da China;
- o encanamento ainda não tem as regras que fazem a doca ser finita;
- o desmanche (A1) depende de 4 coisas que não existem;
- sem um robô de teste, nada se mede.

**Problemas graves que ele achou no encanamento:**
- **Doca infinita na prática.** Falta tudo isto:
  - 1 caminhão atendido por vez (ZH `DockUpdate.cpp`);
  - o limite de 184 m;
  - o "mais perto" medido pelo caminho, não em linha reta;
  - o tempo de 1 s por caixa;
  - a volta pro centro quando não há doca.
- **Regras de lugar furadas.** O ZH exige:
  - o centro **visível agora**, não só explorado;
  - terreno plano (no máximo 2,6 m de diferença);
  - 7,9 m da borda;
  - a faixa de saída da fábrica;
  - o Supply Center a 5,3 m da doca;
  - caminho pro Dozer;
  - pré-requisito nos dados (Power Plant → Supply Center → War Factory).

  E a fundação de 1 de vida com 100% de volta vira muro grátis.
- **Vitória.** No ZH a fundação conta como prédio. Com o nosso "nunca visto", uma fundação escondida segura o jogador vivo.
- **Energia.** Falta a tabela do ZH (fator r entre 0,5 e 0,8) e falta aplicá-la na obra. E o 0/0 do ZH (#1390) deixa a 1ª usina da USA em 20 s em vez de 10.
- **Fábrica destruída.** Devolve a fila (ZH `ProductionUpdate.cpp:1130`). A `BASE_JOGAVEL.md` dizia "perde a fila", o que estava errado.

**Contas de renda** (estimativa do revisor, caminhão do ZH):

| Distância doca → centro | $/min por caminhão |
|---|---|
| 15 m | ~1 450 |
| 50 m | ~950 |
| 100 m | ~630 |
| 184 m | ~400 |

- A doca lotada rende no máximo ~3 600 $/min, e os 30 000 $ dela duram ~8 min.
- A War Factory a pleno gasta 5 400 $/min.
- 1º tanque em ~63 s; 1ª luta no mapa de 1024 m em ~3:30.

**Núcleo proposto, em etapas:**
1. **Regras na simulação, testadas no simtest:**
   - números de um conjunto só (o caminhão vai com o Supply Center chinês de 1500 e 1 caminhão grátis);
   - as regras de lugar do ZH, com dois ajustes:
     - a fundação só ocupa a grade quando o Dozer começa;
     - todo prédio deixa 1 célula livre em volta da doca;
   - a doca do ZH: 1 por vez, vagas de espera, 184 m, "mais perto" pelo caminho;
   - a tabela de energia, também na obra, com o 0/0 corrigido pra 1,0;
   - a vitória por prédio **pronto** da lista.
2. **Robô de teste**, uma lista fixa de construção com ondas (não é a IA), e IA contra IA sem tela.
3. **Desenho e interface mínima:**
   - formas simples;
   - fantasma com o motivo;
   - grade do Dozer;
   - painéis;
   - barra de energia do ZH;
   - pilha da doca e caçamba;
   - o "+$300".
4. **O toque de arrepiar que entra já: a carga que cai.** O caminhão destruído solta as caixas no chão e **qualquer caminhão pega**, inclusive o do inimigo.
   - Raiz no ZH: o caminhão chinês já solta caixa de salvage, e o C-130 solta caixas que o inimigo rouba.
   - Reaproveita a doca, é uma caixa só de arte e dá motivo pra caçar caminhão.

**Fica pra depois:**
- o desmanche, quando houver IA, carcaça na simulação e fogo com duração;
- os ventiladores da War Factory na velocidade da produção, com o modelo dela;
- a base que apaga, quando houver modelos, crepúsculo e o custo da luz medido;
- o Chinook, com a 1ª antiaérea, porque sem ela vira batedor invulnerável que anula a névoa.

---

## Decisões do Felipe (10/10)

1. **Robô de teste: sim.** Uma lista fixa de construção com ondas de ataque, usada como ferramenta de teste. Não é a IA, que vem depois.
2. **Radar e energia dependem da facção.** "Tem facção que nem energia vai ter." Isso fica nos dados de cada facção.
   - A USA de hoje segue o ZH: o radar vem com o Command Center e desliga quando falta energia.
3. **Sem usina, mais lento: igual ao ZH.** Inclusive o 0/0, que dá metade da velocidade: "sem usina as coisas são mais lerdas mesmo, é normal".
   - O HUD mostra quando a produção está mais lenta por falta de energia, pra barra não mentir.
   - O progresso só soma: não salta pra trás nem pra frente quando a energia muda, ao contrário do ZH (#1390).
4. **A carga que cai: entra já.** "Revolucionário isso."
   - O caminhão destruído solta as caixas que levava numa pilha pequena no chão.
   - Qualquer caminhão pega, o seu ou o do inimigo.
   - A pilha some quando esvazia (regra do SupplyPileSmall do ZH).

Também decidido pelo que ele já tinha dito:
- **Coletor:** o caminhão ("um carrinho pra coletar os recursos"), com os números do conjunto chinês do ZH.
- **Prédios:** formas simples montadas como teste de regra ("modelos montados rápidos e práticos").
- **Docas:** fixas no mapa de 1024 m até o WorldForge mandar as dele.

---

## Parte 5. O que está feito (10/10, ainda sem o teste do Felipe)

**Na simulação** (`Game/Source/SimCore`, `WorldEconomy.cpp`; testes em `Game/Tests/Sim`, determinismo nas 3 compilações):
- prédio é unidade (o ZH: tudo é Object), ocupa a grade e tem as regras do BuildAssistant do ZH pra caber (borda, desnível de 2,6 m, folga do depósito, saída da fábrica livre, visível, inimigo em cima, pré-requisito, dinheiro, o Dozer chegar lá);
- a obra do Dozer começa com 1 de vida e cresce; cancelar devolve 100%; outro Dozer retoma a obra parada;
- a fila de produção paga na hora, até 9, cancela com devolução; a fábrica destruída devolve a fila;
- a energia com o fator do ZH (0/0 = metade da velocidade), na obra e na produção;
- o caminhão coleta pelas faixas do depósito do ZH (entra numa ponta, 1 s por caixa, sai na outra), entrega na porta do centro; 1050 $/min por caminhão;
- a carga que cai: o caminhão destruído solta a pilha, qualquer caminhão pega;
- a vitória com base: perde quem fica sem prédio;
- o robô de teste joga a partida inteira com base (usina → centro perto do depósito → fábrica → ondas de 6 tanques).

**Na Unreal** (`Game/Source/RTSGame`):
- o painel provisório (dinheiro, energia com as cores do ZH, "LOW POWER", produzir, fila, cancelar, ponto de encontro, a carga do caminhão), os cliques do ZH (retomar a obra, coletar, ponto de encontro);
- o fantasma do prédio na hora de posicionar: o próprio modelo em holograma, verde cabe e vermelho não, com o motivo escrito;
- os modelos provisórios (`Game/Tools/blender_provisional.py`, `import_provisional.py`, `game.ps1 models`): War Factory pela arte aprovada, Command Center com a parabólica girando, usina com o núcleo aceso, centro de suprimentos com a ponte rolante e o H, depósito com as caixas que somem do estoque, Dozer com a lâmina que trabalha, caminhão com 0 a 4 caixas;
- a obra: o prédio sobe por dentro do andaime, com a linha de solda acesa no corte e faíscas; pronto, o andaime some e as luzes acendem piscando; sem energia, as luzes apagam e o que gira para;
- a morte do prédio: a explosão grande e mais 3 espalhadas em 1,5 s, ele queima e afunda;
- a cor do jogador nas faixas, lonas e bandeiras;
- o teste automático da partida com base (`game.ps1 view -Folder x`, o padrão agora é `-Script base`).

**Falta:** os depósitos virem do WorldForge; a arte final dos prédios; o painel de verdade (a arte do HUD).

## Parte 6. Vender e consertar (v4, 23/10 à noite, simulação 20; sem o teste do Felipe)

**Vender** (o `BuildAssistant::sellObject` do ZH, lido no fonte do TheSuperHackers; o `SellPercentage = 50%` do GameData.ini):
- o botão Sell em todo prédio pronto do jogador (a casa do Cancelar da obra; a zona de lançamento só tem ele, sem ponto de encontro: não produz). Sem confirmação, como no ZH;
- o comando `Sell` (lockstep, no replay): a fila volta inteira na hora (o `cancelAndRefundAllProduction`), e o prédio já sai de tudo (o `OBJECT_STATUS_SOLD`): a energia para de contar, a defesa para de atirar, os poderes e o pré-requisito não valem mais, o centro não recebe caixa, o Dozer que o consertava larga, e ele sai da seleção (o `UNSELECTABLE`);
- desmonta em 6 s (o ZH: 1,5 s de andaime, 3 s do 100% ao 0%, 1,5 s até −50%): o desenho é a obra ao contrário (o andaime, o corte descendo com a solda, a laje). No fim, 50% do custo volta e ele some sem explodir, com o "+$"; destruído no meio, nada volta;
- as caixas no pátio da zona vendida ficam no chão (a carga que cai).

**O conserto do Dozer** (o `DozerAIUpdate` do ZH: `RepairHealthPercentPerSecond = 2%`; o `ActionManager::canRepairObject`):
- o Dozer selecionado sobre um prédio avariado (dele ou do aliado, pronto, sem estar sendo vendido): o cursor vira a chave (a do kit, recortada da placa); o clique manda o comando `Repair`;
- o Dozer mais perto do grupo vai até o prédio (o mesmo alcance da obra) e devolve 2% da vida cheia por segundo, de graça (o ZH não cobra), até encher; um Dozer por prédio (o `canAcceptNewRepair`); a ordem de andar, atacar ou parar larga o conserto;
- o conserto sozinho do ZH (o Dozer à toa 5 s procura prédio avariado a 150 u) ficou de fora: mexeria nos Dozers da IA.
