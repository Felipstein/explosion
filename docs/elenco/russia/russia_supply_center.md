# Supply Depot (Rússia, a ponte rolante amarela)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o depósito de suprimento: onde o KamAZ descarrega as caixas e elas viram dinheiro |
| Nome na tela | Supply Depot (a chave é `russia_supply_center`) |
| Custo e tempo | $1 500, 10 s |
| Vida e blindagem | 2 000, prédio |
| Energia | −1 |
| Visão | 52,7 m |
| Lote | 22 × 20 m |
| Arma | nenhuma |
| Requer | Gas Power Plant |
| Produz | KamAZ ($700, 10 s); **vem com 1 KamAZ de graça** (`free_units`) |
| Estado | nos dados (`Units.json`) e no modelo provisório; fora do menu até os modelos da Rússia serem importados |

## 2. A ideia

**A ponte rolante amarela sobre a faixa do caminhão.** A base de abastecimento soviética: o armazém comprido de chapa verde sobre a
rampa alta de carga, a ponte rolante de pátio nas colunas de concreto com a cabine do operador pendurada, os contêineres de aço, as
caixas de munição verdes e a guarita envidraçada do despachante em cima do escritório. O caminhão entra pela frente, a ponte para em
cima dele, o guincho levanta a carga.

- **A palavra de cima:** o RETÂNGULO AMARELO das 2 vigas atravessando o pátio entre as colunas, ao lado do telhado verde comprido.
- **Inspiração real:** o armazém militar soviético (o склад com a rampa de carga de concreto e a marquise de chapa) e as pontes
  rolantes de pátio das estações de carga. A função aparece na forma.
- **Por que existe:** é o dinheiro. E o KamAZ pesado é o jeito da Rússia: menos caminhões na estrada, mais caixa por viagem.

## 3. Como funciona

- **No jogo:** o KamAZ leva **5 caixas** de $75 (**$375 por viagem**): 1 s por caixa no depósito, 0,4 s por caixa na doca. O prédio
  dá o 1º KamAZ de graça e produz mais. Gasta 1 de energia.
- **A conta:** com o depósito a ~60 m, a ida e volta a 8,9 m/s leva ~13,5 s + 5 s carregando + 2 s descarregando: **~$1 100 por
  minuto por caminhão**, ~4% mais que o caminhão da China na mesma rota (~$1 060). 2 a 3 KamAZ por depósito é o ponto bom.
- **Forte contra:** nada; ele é economia.
- **Fraco contra:** raide (Technical, Rocket Buggy), artilharia; os caminhões em volta dele são o alvo de verdade. Um tanque o
  derruba em ~67 s.
- **O contra dele:** quem mata o caminhão. **Ele é o contra de:** a pobreza.

## 4. Em batalha

- **Temperamento: o almoxarife.** Lento, minucioso, desconfiado. Confere cada caixa; nada sai sem a assinatura.
- **O ritmo:** o KamAZ chega devagar, 5 baques, sai; a buzina da ponte a cada carga.
- **O jogador bom** o põe perto do depósito (cada metro a menos é dinheiro), com o Pantsir fixo cobrindo a doca contra o Apache e a
  rota dos caminhões.
- **A IA** o põe depois da usina (a lista) e mantém os caminhões; o dinheiro do caminhão que falta fica guardado.
- **Sozinho e ferido:** segue recebendo caixa até cair; abaixo de 35%, o fogo nos 3 pontos.

## 5. A vida

- **No jogo:** a ponte rolante amarela (peça `_Crane`) entra na malha do prédio **parada** em cima da faixa do caminhão (o jogo ainda
  não corre ponte; ela afunda junto na ruína); a bandeira lisa na cor do jogador no mastro do escritório (`flags`); o andaime de aço
  na obra; abaixo de 35%, o fogo nos 3 pontos (`fire_points_m`).
- **Ideia (descarregando):** a ponte corre nos trilhos das vigas (o curso já está no script, `CRANE_TRAVEL_X`: de cima do caminhão
  até as pilhas do fundo), o guincho desce, as caixas sobem uma a uma (0,4 s cada, o ritmo do jogo), a luz âmbar gira; a carga vai
  pros contêineres.
- **Ideia (parado):** o despachante na guarita envidraçada (a luz acesa); os holofotes do mastro de 10 m acesos à noite; as portas
  de correr do armazém meio abertas.
- **Ideia (sem energia):** a ponte para no meio do curso e os holofotes apagam; a descarga continua no braço (ninguém fica sem
  dinheiro, mas o prédio "parece" sem força).

## 6. O visual

- **De cima:** o telhado verde comprido do armazém de um lado, o quadro amarelo da ponte atravessando o pátio entre as colunas com as
  faixas amarelas e pretas, o escritório com a guarita de vidro no teto, a faixa do caminhão marcada no chão, os contêineres e as
  pilhas com a lona vermelha no fundo do pátio.
- **Assinatura:** a ponte rolante amarela (a da China é vermelha: a cor separa as duas).
- **Cores:** o painel cinza, a chapa verde; o amarelo da ponte e o amarelo e preto de perigo nas colunas (fixos: a cor de segurança
  da máquina); as caixas de munição verdes; a cor do jogador na bandeira do escritório e nas lonas das pilhas.
- **Nunca:** a ponte vermelha (é da China); caixas esparramadas (a carga fica em pilhas e contêineres); Z ou V.

## 7. Tiro, projétil, impacto, destruição

- **Impacto (ideia):** a bala na viga amarela faz faísca e "plim"; o canhão abre a chapa do armazém e uma caixa voa de dentro.
- **A morte (no jogo):** a morte de prédio (a grande no meio, 3 menores, o chão queimado, o entulho: $150 em ~11,5 s pro
  construtor); a ponte afunda junto.
- **A morte (ideia):** a ponte cai atravessada sobre o pátio (a viga amarela fica no entulho, legível de cima); os contêineres
  tombam; as caixas do armazém se espalham e algumas viram a "carga que cai" (a pilha pequena que qualquer caminhão pega, a
  mecânica que já existe pro caminhão morto).

## 8. O som

- **Seleção (variantes):** a buzina curta da ponte rolante (o aviso de carga suspensa); o motor elétrico do carro do guincho; a
  corrente; o carimbo do despachante.
- **As falas** (inglês, com o sotaque russo leve; o despachante, voz seca, burocrática, de quem confere a lista):
  - selecionado: "Depot." / "Bring the crates." / "Everything is counted." / "Bay one is clear." / "Sign here, Commander."
  - produzindo: "KamAZ dispatched." / "Another truck on the road."
  - sob ataque: "The depot is under fire!" / "Protect the trucks!" / "They are hitting the stores!"
  - pronto: "Supply depot operational." / "Open the gates."
- **O corpo:** o zumbido do motor da ponte; a corrente do guincho; as caixas de madeira batendo no concreto da rampa; as portas de
  correr; de vez em quando, o apito do turno ao longe.
- **A construção:** as colunas de concreto içadas, a viga da ponte assentando com o "clang" grande, os rebites; no fim, a buzina da
  ponte e o primeiro KamAZ dando a partida.
- **A morte:** os estouros em série; a viga amarela caindo (o metal grande, longo); as caixas despencando; a buzina morrendo.
- **A cara da Rússia:** o metal grave, a máquina pesada, a voz baixa e seca.

## 9. Ideias pra depois

- **A ponte que anda:** o primeiro passo: a ponte corre de verdade no curso do script a cada caixa descarregada, e o jogador lê a
  economia pelo vai e vem amarelo.
- **O trem de suprimento** (estudo `docs/FACCAO_RUSSIA.md` §5.4): no fim da árvore, o depósito ferroviário; um trem passa pelos
  trilhos do mapa e entrega a carga: caro, mas não morre na estrada como o caminhão. Pede trilho no mapa (os tipos de mapa do
  WorldForge).
- **A reserva:** um upgrade ($600) que guarda 10% de cada caixa num contêiner do pátio; se o depósito cair, o contêiner fica no
  entulho como uma pilha que só o KamAZ do dono pega (a Rússia que guarda pro inverno).

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#russia_supply_center` (feito no fim, com o modelo provisório como anexo de forma).
