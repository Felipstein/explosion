# Dozer (China, o trator de obra)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o construtor: ergue a base, conserta e desmancha carcaça em dinheiro |
| Custo e tempo | $1 000, 5 s |
| Vida e blindagem | 250, tanque |
| Velocidade | 7,9 m/s, giro 90°/s |
| Visão | 52,7 m |
| Arma | nenhuma |
| Esmagador | nível 2: passa por cima de soldado, carro, poste e árvore |
| Conserta | 2% da vida por segundo |
| Constrói | Command Center, Nuclear Reactor, Supply Center, Barracks, War Factory, Propaganda Center, Gattling Cannon |
| Sai de | Command Center (a China começa com 1) |

## 2. A ideia

**A lâmina.** O trator de esteira pesado e quadrado: a lâmina larga e curva na frente, o capô comprido, a cabine gradeada com a
sirene âmbar, o escarificador de 3 dentes atrás. É o operário da base, nunca um soldado.

- **A palavra de cima:** a LÂMINA, mais larga que as esteiras.
- **Inspiração real:** os tratores de esteira chineses de obra pesada (a linha Shantui SD), fictício, não cópia. O Dozer da USA é
  o "minerador" claro e facetado; este é o trator de obra brutal.
- **Por que existe:** sem ele não há base. E, com a sucata, ele também transforma a batalha em dinheiro.

## 3. Como funciona

- **No jogo:** constrói os 7 prédios da China; conserta prédio e veículo a 2% por segundo (um CC de 5 000 sobe 100 por segundo);
  **desmancha carcaça** (15% do custo de quem morreu, em 3 s + 1 s a cada $150: a carcaça de um Overlord dá $300 em ~16 s) e
  **entulho de prédio** (10% em 4 s + 1 s a cada $200). A IA manda o Dozer parado recolher a sucata da própria base.
- **Esmaga:** nível 2, como o tanque: o soldado que fica na frente dele morre (o esmagamento faz 200% no soldado).
- **Forte contra:** o tempo (base rápida), a sucata, o soldado distraído no caminho.
- **Fraco contra:** tudo que atira: 250 de vida (o RPG mata em ~6 s, a Technical em 20 s, o tanque em 8 s).
- **O contra dele:** qualquer raide. **Ele é o contra de:** a base inimiga que perde a guerra de sucata.

## 4. Em batalha

- **Temperamento: o operário teimoso.** Calmo, trabalhador, não se assusta fácil. Faz o serviço até o fim, mesmo com bala
  passando.
- **O ritmo:** o motor sobe e desce com a carga: lâmina empurra, recua, empurra.
- **O jogador bom** tem 2 Dozers: um ergue, o outro conserta e recolhe a sucata atrás da linha depois de cada combate (o dinheiro
  da batalha volta); nunca manda o Dozer na frente.
- **A IA** usa o Dozer pra lista da base e, parado, recolhe a carcaça mais perto até 90 m de casa.
- **Sozinho e ferido:** abaixo de 70%, o fio de fumaça clara; abaixo de 35%, a chama e a fumaça grossa. Ele não foge sozinho.

## 5. A vida

- **No jogo:** a sirene âmbar gira (a peça `_Beacon`, 90 rpm); a lâmina gira no pivô quando trabalha (peça `_Blade`); as esteiras
  e a poeira; os estados de dano do veículo (a fumaça abaixo de 70%, a chama abaixo de 35%).
- **Ideia (parado):** o diesel em marcha lenta solta um fio de fumaça escura pelo escape em pulsos; a lâmina descansa no chão.
- **Ideia (construindo):** a lâmina sobe e desce no ritmo; a poeira levanta na frente dela (volume, na luz do mundo); o escarificador
  risca o chão atrás; o terreno da obra fica aplainado (o campo que muda: `docs/VISAO_DO_JOGO.md`).
- **Ideia (desmanchando sucata):** a lâmina empurra a carcaça, que perde peças a cada passo (a torre cai, a chapa solta), e o
  pedaço some num monte de sucata na caçamba invisível (as faíscas do aço raspando).
- **Ideia (consertando):** a faísca de solda saindo de dentro da cabine (o operador com o maçarico) apontada pro prédio.

## 6. O visual

- **De cima:** o retângulo de 6,8 m com a lâmina larga e curva na frente (3,3 m), o capô comprido, a cabine quadrada atrás com a
  cúpula âmbar, o escarificador de 3 dentes.
- **Assinatura:** a lâmina.
- **Cores:** o oliva escuro com a camuflagem digital; o vermelho do jogador na estrela das duas portas e na lona dobrada no
  para-lama de trás; o âmbar da sirene.
- **Nunca:** arma; amarelo de obra civil (é da China, oliva); lona grande.

## 7. Tiro, projétil, impacto, destruição

- **Impacto (ideia):** a bala de fuzil faz faísca na grade da cabine; o canhão amassa a lâmina (a lâmina segura o primeiro tiro
  de frente, só desenho).
- **A morte (no jogo):** a morte de veículo; a carcaça fica 48 s queimando e afunda em 7 s; serve de cobertura pro soldado e de
  sucata pra outro construtor ($150 em ~10 s).
- **A morte (ideia):** o tanque de diesel atrás estoura primeiro (a bola laranja suja e a fumaça preta), a cabine pega fogo; a
  lâmina fica no chão, inteira, na frente da carcaça (a peça que todo mundo reconhece).

## 8. O som

- **Seleção (variantes):** o diesel acelerando grave com o "pfff" do turbo; a lâmina batendo no chão (o "clong"); a sirene âmbar
  com o bip de ré; a buzina grave curta.
- **As falas** (inglês; o operador veterano, voz de trabalhador, calma, com o sotaque do canteiro):
  - selecionado: "Dozer here." / "Ready to build." / "Where do we dig?" / "Show me the ground." / "Heavy work, heavy steel."
  - andando: "Rolling." / "On my way." / "Moving the blade." / "Clear a path."
  - construindo: "Breaking ground." / "Building for the people." / "Foundation going in."
  - sucata: "Scrap is steel." / "Nothing wasted." / "Clearing the wreck."
  - consertando: "Patching it up." / "Repairs underway."
  - sob fogo: "I'm not a tank!" / "Taking fire, get me cover!" / "They're shooting the builder!"
  - pronto: "Dozer ready."
- **O corpo:** o diesel pesado (o mais rouco dos veículos China, irregular, trabalhando); as esteiras com o clank lento e o rangido;
  o bip de ré; a hidráulica da lâmina (o "zuuu" do cilindro).
- **A construção:** a lâmina raspando o chão, o motor sob carga, as pancadas da obra.
- **A morte:** o estouro do diesel (abafado, gordo), o fogo crepitando, a lâmina caindo no chão.
- **A cara da China:** o diesel grave e o metal pesado.

## 9. Ideias pra depois

- **A rampa:** o Dozer cava uma rampa na encosta e abre um caminho novo pro tanque (o campo que muda, já na lista da visão).
- **A trincheira:** o Dozer empurra a areia e faz uma berma de 8 m: cobertura de soldado (como a carcaça) e tanque atrás dela
  leva só tiro na torre. Some com o tempo ou com explosão.
- **Empurrar a carcaça:** em vez de desmanchar, empurrar a carcaça pra fechar uma rua ou abrir uma (a carcaça vira peça de
  tabuleiro).

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#china_dozer` (feito no fim, com o modelo provisório como anexo de forma).
