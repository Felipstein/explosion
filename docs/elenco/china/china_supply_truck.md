# Supply Truck (China, o caminhão 6×6)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o coletor: leva as caixas do depósito ao Supply Center |
| Custo e tempo | $600, 10 s (o 1º vem de graça com o Supply Center) |
| Vida e blindagem | 300, tanque |
| Velocidade | 10,5 m/s, giro 90°/s, acelera e freia forte (63 m/s²) |
| Visão | 39,5 m |
| Carga | **4 caixas** de $75 = $300 por viagem |
| Arma | nenhuma |
| Esmagador | nível 1: o soldado na frente dele morre |
| Requer | Supply Center |

## 2. A ideia

**A caçamba com as 4 caixas.** O caminhão militar de cabine avançada, baixo e largo, com a grade grande e a estrela, a caçamba
aberta mostrando a carga. É a veia da China: enquanto ele roda, a horda existe.

- **A palavra de cima:** a CAÇAMBA ABERTA com as 4 caixas em 2 × 2.
- **Inspiração real:** os caminhões 6×6 do ELP (a família Shaanxi: a cabine avançada chata, os pneus grandes), fictício, não cópia.
- **Por que existe:** é o dinheiro. Mais pesado e mais caro que a picape da GLA (4 caixas contra 3, 300 de vida contra 200).

## 3. Como funciona

- **No jogo:** carrega 4 caixas no depósito (1 s cada) e descarrega no Supply Center (0,4 s cada): $300 por viagem. Com o depósito
  a ~60 m, rende ~$1 100 por minuto.
- **A carga que cai (no jogo):** se ele morre carregado, as caixas que levava ficam no chão numa pilha pequena que **qualquer**
  caminhão pega, o dele ou o do inimigo.
- **Esmaga:** nível 1, o soldado que fica no caminho morre (200%).
- **Forte contra:** nada; é economia (e o soldado distraído na estrada).
- **Fraco contra:** raide: a Technical (12,5/s no tanque) mata em 24 s; o RPG em ~7 s; 3 Rocket Buggies numa salva só.
- **O contra dele:** a Technical e o Rocket Buggy. **Ele é o contra de:** a pobreza.

## 4. Em batalha

- **Temperamento: o carregador.** Obediente, apressado, medroso. Ele não briga; ele entrega.
- **O ritmo:** ida, 4 baques de caixa, volta, 4 baques de caixa. O jogador sente a economia no som.
- **O jogador bom** escolhe o depósito mais perto e põe uma Gattling Cannon cobrindo a rota; quando a pilha de um caminhão morto
  cai no meio do mapa, manda o próprio buscar antes do inimigo.
- **A IA** mantém os caminhões (o dinheiro do que falta fica guardado); o inimigo da IA mira caminhão e artilharia primeiro.
- **Sozinho e ferido:** segue a rota; abaixo de 70%, a fumaça clara; abaixo de 35%, a chama.

## 5. A vida

- **No jogo:** as 4 caixas aparecem e somem da caçamba conforme a carga (`cargo.slots_m`); as 6 rodas giram; a poeira e a marca dos
  pneus; os estados de dano do veículo.
- **Ideia (parado):** o diesel em marcha lenta, o fio de fumaça do escape vertical atrás da cabine; a lona dobrada no suporte
  balança.
- **Ideia (carregando):** a caçamba abaixa um pouco a cada caixa (a mola sente o peso); a poeira levanta quando ele arranca cheio.
- **Ideia (descarregando):** a ponte rolante do Supply Center tira as caixas; a caçamba sobe leve.

## 6. O visual

- **De cima:** o retângulo de 5,4 m com a cabine curta e baixa na frente, o estepe e a lona no vão, a caçamba aberta com as 4
  caixas.
- **Assinatura:** a caçamba carregada (a carga é o que se lê).
- **Cores:** o oliva escuro com a camuflagem digital; o vermelho do jogador na estrela da grade (troca com a cor do jogador), na
  lona dobrada atrás da cabine e nas estrelas pequenas das portas.
- **Nunca:** arma; lona cobrindo a carga (a carga tem de aparecer de cima).

## 7. Tiro, projétil, impacto, destruição

- **Impacto (ideia):** a bala fura a lona e a lataria (faísca e "tunc"); o tiro na caçamba faz uma caixa pular e cair (só desenho).
- **A morte (no jogo):** a morte de veículo; a carcaça fica 48 s e afunda em 7 s; a pilha da carga que cai fica no chão; a carcaça
  é sucata ($90 em 7 s pro construtor).
- **A morte (ideia):** o tanque de diesel estoura embaixo da cabine (a bola suja e a fumaça preta), a cabine pega fogo; as caixas
  voam da caçamba e caem em volta, formando a pilha (o jogador vê a carga cair).

## 8. O som

- **Seleção (variantes):** o diesel de caminhão dando a partida e acelerando; a buzina de duas notas; o freio a ar ("pssh"); a porta
  da cabine batendo.
- **As falas** (inglês; o motorista, jovem, apressado, um pouco nervoso):
  - selecionado: "Supply truck." / "Ready to haul." / "Where's the cargo?" / "Full tank, empty bed." / "Supplies for the people."
  - andando: "On the road." / "Heading out." / "Fast as I can." / "Hauling."
  - carregando: "Loading crates." / "Full load!" / "Heading back."
  - sob fogo: "They're shooting at me!" / "I need an escort!" / "I'm unarmed!"
  - pronto: "Supply truck ready."
- **O corpo:** o diesel de caminhão (mais agudo que o do tanque, roncando nas trocas de marcha); os pneus na areia; a carroceria
  batendo nos buracos; o bip de ré na doca.
- **A carga:** cada caixa de madeira entrando na caçamba é um baque seco (1 por segundo, o ritmo do jogo); na doca, 4 baques
  rápidos e o "clang" da tampa da caçamba fechando (nada de tilintar de moeda: não combina com o tom sério).
- **A morte:** o estouro do diesel, os pneus queimando, as caixas despencando de madeira.

## 9. Ideias pra depois

- **O comboio:** 3 caminhões seguidos na mesma rota andam 10% mais rápido (o vácuo e a disciplina), e o jogador vê a fila.
- **A blindagem de cabine:** upgrade no Supply Center ($500) que dá +100 de vida e as chapas soldadas na cabine (lê de cima: a
  cabine fica mais escura e blocada).
- **O caminhão-isca:** o caminhão vazio pode levar uma carga falsa (caixas cheias de explosivo): quem pega a pilha dele quando ele
  morre leva uma explosão. Truque da China, caro e de uma vez só.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#china_supply_truck` (feito no fim, com o modelo provisório como anexo de forma).
