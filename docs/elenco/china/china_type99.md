# Battlemaster / Type 99 (China, o tanque da horda)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o tanque básico: o corpo da horda da China |
| Nome na tela | Battlemaster (a chave é `china_type99`) |
| Custo e tempo | $800, 10 s |
| Vida e blindagem | 400, tanque |
| Velocidade | 6,6 m/s, giro do casco e da torre 180°/s |
| Visão | 39,5 m |
| Arma | canhão: 60 perfurante a cada 2 s, alcance 39,5 m, a granada a 105 m/s |
| Esmagador | nível 2: passa por cima de soldado, carro, poste e árvore |
| Requer | War Factory |
| Sai de | War Factory |

## 2. A ideia

**A torre-escudo.** O tanque chinês de hoje: a torre comprida e pontuda na frente, como um escudo de batalha, a gaiola de carga
em U abraçando a traseira, um canhão só. Escuro, quadrado, numeroso. Um sozinho não assusta; dez em fila decidem a partida.

- **A palavra de cima:** a TORRE-ESCUDO (o hexágono alongado com o nariz estreito e as bochechas abrindo), com a gaiola atrás.
- **Inspiração real:** o ZTZ-99 (o tanque principal do ELP: canhão de alma lisa de 125 mm com carregador automático e 3 de
  tripulação; a versão 99A com a blindagem em cunha na frente da torre). Fictício, não cópia: a ficha aprovada
  `docs/concept/units/china_mbt_sheet.webp`.
- **Por que existe:** a China arrasa em massa. O Battlemaster é mais barato e mais fraco que o tanque da USA; a vantagem é o número.

## 3. Como funciona

- **No jogo:** 60 perfurante a cada 2 s = **30/s** em tanque e prédio, **3/s** em soldado (perfurante faz 10%). Mata um tanque USA
  (480) em 8 tiros (14 s), um Scorpion (370) em 7 (12 s), uma Technical (180) em 3 (4 s), um Rocket Buggy (120) em 2.
- **O duelo com o tanque USA:** perde no 1 contra 1 (o USA o mata em 12 s e tem o ricochete), mas custa $100 a menos; pela conta
  de Lanchester (`docs/ELENCO.md` §8.1) os dois empatam: 0,0188 contra 0,0178. Em 5 contra 4 pelo mesmo dinheiro, a China ganha.
- **Esmaga:** o soldado que fica na frente morre (200%); é assim que ele mata infantaria.
- **Forte contra:** tanque, veículo leve, prédio, defesa de base no alcance dele.
- **Fraco contra:** soldado antitanque em grupo (3 RPGs tiram 120/s: ele cai em ~3 s), ar (não atira pra cima), infantaria
  espalhada (o canhão quase não fere soldado), artilharia.
- **O contra dele:** o RPG e o Javelin em grupo, o Apache, o Rocket Buggy de longe. **Ele é o contra de:** a Technical, o
  Scorpion, a Quad Cannon, o caminhão, o Rocket Buggy pego de perto.

## 4. Em batalha

- **Temperamento: o soldado de linha.** Disciplinado, sem brilho, sem medo. Atira, avança, atira. Não sai da formação.
- **O ritmo:** um tiro a cada 2 s; em grupo, a salva irregular (BUM ... BUM-BUM ... BUM) que vira um trovão contínuo.
- **O jogador bom** faz bolo de 6 a 10, com 2 Gattling Tanks no meio (cuidam do ar e da infantaria) e o Overlord na frente;
  manda esmagar o grupo de RPG em vez de atirar nele.
- **A IA** faz dele o 1º da fábrica (vale por 2 na mistura).
- **Sozinho e ferido:** não recua sozinho; abaixo de 70% solta o fio de fumaça clara, abaixo de 35% a chama pelo anel da torre e
  a fumaça grossa.

## 5. A vida

- **No jogo:** a torre gira 180°/s e volta pra frente 2 s depois de parar; o cano recua no tiro; as esteiras e a poeira das
  lagartas (o volume da poeira, aprovado); a cratera A no chão onde a granada cai; os estados de dano (a fumaça abaixo de 70%, a
  chama e a fumaça grossa abaixo de 35%).
- **Ideia (parado):** o diesel em marcha lenta solta um pulso de fumaça escura pelo escape de trás a cada ~3 s; a antena balança;
  a torre faz uma varredura de 10° de vez em quando.
- **Ideia (andando):** a lona dobrada na gaiola bate no vento; o casco inclina pra trás na arrancada e pra frente na freada (a
  suspensão).
- **Ideia (atirando):** o casco balança pra trás com o recuo; o sopro da boca levanta a poeira na frente do cano; depois de 2 s, o
  carregador automático (o som).

## 6. O visual

- **De cima:** o retângulo de 7,8 × 3,6 m, a torre comprida em ponta de escudo um pouco atrás do meio, a gaiola em U atrás dela, o
  canhão saindo 2,15 m além do nariz.
- **Assinatura:** a torre-escudo com a gaiola.
- **Cores:** o oliva escuro com a camuflagem digital (L ~0,30 de cima); o vermelho do jogador na lona dobrada em cima da gaiola, no
  rolo atrás do motor, na faixa do cano e nas estrelas das bochechas.
- **Nunca:** cano duplo (é do Overlord), faixa pintada, lona grande caída, cor clara.

## 7. Tiro, projétil, impacto, destruição

- **A boca:** o clarão laranja curto e grosso; o anel de fumaça cinza que sai da boca e abre (volume, acesa pelo clarão); a poeira
  levantada no chão na frente (o sopro).
- **O projétil:** o risco curto, quente, branco-amarelado, quase invisível (105 m/s no jogo).
- **O impacto:**
  - no tanque: a faísca forte, a chapa que salta, a marca preta; o raspão (o tiro que passa perto) levanta a areia atrás do alvo;
  - no prédio: o pedaço de concreto que voa e a poeira;
  - no chão: a cratera A e a areia que sobe;
  - no soldado: quase nada (10%); ele mata soldado passando por cima.
- **A morte:** o diesel e a munição: a bola de fogo, a torre que salta ou fica torta no anel, a carcaça queimando 48 s, que vira
  cobertura pro soldado e sucata ($120 em ~8 s) pro construtor.

## 8. O som

- **Seleção (variantes):** o diesel acelerando (grave, mas mais leve que o do Overlord); o servo da torre girando curto; a escotilha
  batendo; o rádio do tanque chiando com o alto-falante.
- **As falas** (inglês; o comandante de tanque, voz firme, jovem, de manual, sem floreio):
  - selecionado: "Battlemaster ready." / "Tank, standing by." / "Awaiting orders." / "For the motherland." / "Crew ready."
  - andando: "Moving out." / "Advancing in formation." / "Rolling forward." / "On the move."
  - atacando: "Target acquired." / "Fire!" / "Engaging armor." / "Load and fire."
  - sob fogo: "Taking hits!" / "Armor holding!" / "We need support!" / "Hold the line!"
  - pronto: "Battlemaster, reporting."
- **O corpo:** o diesel de tanque em marcha lenta (graves pulsando); as esteiras com o clank-clank médio; o servo elétrico da torre
  com o rangido do anel; a suspensão batendo nos buracos.
- **A arma:** o estrondo seco com o "tum" no peito; o eco nos prédios; 1,5 s depois, o carregador automático (o "clank" da culatra,
  o motor do carregador girando e o "clack" final).
- **A morte:** o estouro do combustível, os estalos da munição cozinhando, o metal da torre batendo no casco, o fogo crepitando.
- **A cara da China:** o metal pesado e o rádio com o alto-falante.

## 9. Ideias pra depois

- **A formação cerrada:** 4 Battlemasters a até 12 m um do outro somam a blindagem: cada um leva 10% menos dano (os escudos lado a
  lado; o jogador vê as torres alinhadas). Recompensa a horda disciplinada.
- **O carregador automático aquecido:** a 1ª salva depois de 10 s parado sai em 1,2 s em vez de 2 s (o carregador pronto); premia a
  emboscada da China, que não é o jogo dela.
- **A estrela de veterano:** o tanque que sobe de patente ganha uma estrela pequena pintada a mais na bochecha da torre (bandeira
  é só da GLA); de perto, o jogador acha o veterano do grupo.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#china_type99` (feito no fim, com o modelo provisório como anexo de forma).
