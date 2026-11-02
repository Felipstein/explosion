# Supply Center (China, a doca da ponte rolante)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o centro de suprimento: onde o caminhão descarrega as caixas e elas viram dinheiro |
| Custo e tempo | $1 500, 10 s |
| Vida e blindagem | 2 000, prédio |
| Energia | −1 |
| Visão | 52,7 m |
| Lote | 22 × 20 m |
| Arma | nenhuma |
| Requer | Nuclear Reactor |
| Produz | Supply Truck ($600, 10 s); **vem com 1 caminhão de graça** (`free_units`) |

## 2. A ideia

**A ponte rolante vermelha sobre a doca de descarga.** A indústria da China organizada: o caminhão entra na faixa amarela, para
debaixo da ponte, o carrinho levanta as caixas e o armazém comprido engole tudo. A função aparece na forma.

- **A palavra de cima:** o QUADRO VERMELHO da ponte rolante atravessado ao lado do telhado comprido verde.
- **Inspiração real:** os pátios de carga industriais com ponte rolante (pórtico sobre colunas, o carrinho com guincho) e os
  armazéns de telhado de quadril. Nada de trem ou trilho no chão (vetado): a ponte corre em vigas no alto.
- **Por que existe:** é o motor do dinheiro. Sem ele, a China não paga a horda.

## 3. Como funciona

- **No jogo:** o caminhão leva **4 caixas** de $75 (**$300 por viagem**): 1 s por caixa no depósito, 0,4 s por caixa na doca. O
  prédio dá o 1º caminhão de graça e produz mais. Gasta 1 de energia.
- **A conta:** com o depósito a ~60 m, uma viagem de ida e volta a 10,5 m/s leva ~11 s + 4 s carregando + 1,6 s descarregando:
  ~$1 100 por minuto por caminhão. 3 caminhões por centro é o ponto bom.
- **Forte contra:** nada; ele é economia.
- **Fraco contra:** raide (Technical, Rocket Buggy), artilharia; os caminhões em volta dele são o alvo de verdade.
- **O contra dele:** quem mata o caminhão. **Ele é o contra de:** a pobreza.

## 4. Em batalha

- **Temperamento: o operário.** Ocupado, metódico, sem pausa. Trabalha no compasso da sirene do turno.
- **O ritmo:** caminhão chega, ponte desce, caixa sobe, caixa entra, caminhão sai. Um relógio de fábrica.
- **O jogador bom** o põe perto do depósito (cada metro a menos é dinheiro), com uma Gattling Cannon cobrindo a doca e os
  caminhões.
- **A IA** o põe depois do reator (a lista) e mantém os caminhões; o dinheiro do caminhão que falta fica guardado.
- **Sozinho e ferido:** segue recebendo caixa até cair; o caminhão não para de vir.

## 5. A vida

- **No jogo:** o andaime de aço na obra; abaixo de 35%, o fogo nos 2 pontos do telhado e os buracos.
- **Ideia (parado):** o carrinho da ponte (peça `_Crane` já modelada no script, ainda fora dos dados) corre de leve pra lá e pra
  cá; as portas de enrolar meio abertas com a luz amarela de dentro; a chaminé fina da caldeira solta um fio de fumaça cinza; a
  baliza da ponte pisca.
- **Ideia (descarregando):** a ponte para em cima do caminhão, o carrinho desce o balancim, as caixas sobem uma a uma (0,4 s cada,
  o ritmo do jogo), a luz giratória âmbar acende; a caixa entra pela porta.
- **Ideia (sem energia):** a ponte para no meio do caminho e as portas apagam; a descarga continua no braço (ninguém fica sem
  dinheiro, mas o prédio "parece" sem força).

## 6. O visual

- **De cima:** o retângulo verde comprido do armazém, o quadro vermelho da ponte do lado, a faixa amarela do caminhão entrando pela
  frente, a caixa d'água redonda de 4 pernas (o círculo do lote).
- **Assinatura:** a ponte rolante vermelha.
- **Cores:** concreto escuro oliva; telha verde; o vermelho do jogador nas colunas e na ponte, nos 2 estandartes, na estrela do
  oitão e na chaminé listrada.
- **Nunca:** trilho no chão, trem, montanha de caixas esparramadas (as caixas ficam em pilhas organizadas).

## 7. Tiro, projétil, impacto, destruição

- **Impacto (ideia):** a bala no aço da ponte faz faísca e "plim"; o canhão no armazém abre a telha e a caixa de dentro voa.
- **A morte (no jogo):** a morte de prédio (a grande no meio, 3 menores, o chão queimado, o entulho que vale $150 em ~12 s pro
  Dozer).
- **A morte (ideia):** a ponte rolante cai atravessada sobre a doca (a viga vermelha fica no entulho, legível de cima); as caixas
  do armazém se espalham e algumas viram a "carga que cai" (uma pilha pequena que qualquer caminhão pega, a mecânica que já
  existe pro caminhão morto).

## 8. O som

- **Seleção (variantes):** a sirene curta de turno; o motor elétrico da ponte rolando; a corrente do guincho; o alto-falante
  chamando o próximo caminhão.
- **As falas** (inglês; o capataz da doca, voz alta, prática, gritando por cima do barulho):
  - selecionado: "Supply depot, working." / "Bring the crates!" / "Every crate feeds the army." / "Next truck to bay one." /
    "Unloading."
  - produzindo: "Truck dispatched." / "Another truck for the line."
  - sob ataque: "The depot is under fire!" / "Protect the trucks!" / "They're hitting our supply!"
  - pronto: "Supply center operational." / "Open the bays."
- **O corpo:** o zumbido do motor da ponte; a corrente do guincho; as caixas de madeira batendo no concreto da doca; as portas de
  enrolar; uma vez por minuto, a sirene do turno ao longe.
- **A construção:** as colunas de aço içadas e aparafusadas (a chave de impacto), a viga da ponte assentando com o "clang"
  grande; no fim, a sirene do turno e o primeiro caminhão dando a partida.
- **A morte:** os estouros em série; a viga da ponte caindo (o metal grande, longo); as caixas despencando; a sirene morrendo.

## 9. Ideias pra depois

- **A esteira de abastecimento:** um upgrade ($600) que acelera a descarga pela metade (0,2 s por caixa), com a esteira rolante
  aparecendo da doca pra dentro do armazém.
- **O cartaz da cota:** o telão pequeno no escritório mostra, de cima, a cota do minuto (o dinheiro que entrou), um número que o
  jogador lê sem abrir menu.
- **Os caminhões com nome de turno:** cada caminhão novo sai com o número pintado na porta (1, 2, 3); o que morre deixa a porta
  queimada com o número no entulho, pro jogador saber qual perdeu.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#china_supply_center` (feito no fim, com o modelo provisório como anexo de forma).
