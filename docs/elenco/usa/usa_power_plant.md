# Cold Fusion Reactor (USA, a energia)

## 1. Ficha rápida

| | |
|---|---|
| Papel | a usina: dá a energia que a base consome; o prédio mais repetido da partida (4 a 6) |
| Custo e tempo | $800, 10 s |
| Vida e blindagem | 800, prédio |
| Energia | **+5** |
| Visão | 52,7 m |
| Lote | 9,3 × 12,6 m (o menor prédio da USA depois da Patriot) |
| Requer | nada |
| Libera | o Supply Center e a Patriot |
| Conta pra vitória | sim |

## 2. A ideia

**O reator.** Um cilindro de anéis com o núcleo aceso em cima, branco quente, cercado de aletas de troca de calor, e o bloco de
controle do lado com os isoladores. Pequeno, limpo e caro de perder: quando ele cai, a defesa da base apaga.

- **A palavra de cima:** os CÍRCULOS CONCÊNTRICOS (o reator visto de cima). A única forma redonda grande do prédio.
- **Inspiração real:** os reatores de pesquisa de fusão (o tokamak: o anel de ímãs em volta do plasma) e as subestações
  militares. **"Fusão fria" não existe** no mundo real: é o nome do ZH, a escapada de ficção que o jogo herda. O material é
  real (aço, concreto, isolador de porcelana, cabo grosso).
- **Por que existe:** a energia é o freio da USA. Cada prédio forte custa energia (a Patriot, a Drop Zone e o Airfield −3 cada),
  e cada usina é um alvo pequeno, barato e vital. É a escolha de onde esconder.
- **Proibido:** a torre de resfriamento em ampulheta (é da China).

## 3. Como funciona

- **No jogo:** +5 de energia. A conta de uma base típica: Supply Center −1, War Factory −1, Patriot −3, Strategy Center −2,
  Airfield −3, Drop Zone −3. Uma base com fábrica, Airfield e duas Patriots pede 3 usinas.
- **Sem energia (gastando mais do que dá):** a produção cai pra 50–80% da velocidade (na proporção da falta, o
  `MinLowEnergyProductionSpeed`/`Max` do ZH); a Patriot fica muda; a Drop Zone para a contagem; as peças que giram param. O HUD
  avisa "Low power".
- **Fraqueza:** 800 de vida. Dois Battle Tanks matam em ~13 s; o ataque de mísseis mata sozinho (~800 de 800) e apaga as
  Patriots junto: é o alvo nº 1 do ataque (a nota da IA sobe 1,5× pra usina).
- **Forte contra:** nada (não atira). **Fraca contra:** tudo que chega perto, o ataque de mísseis, o Raptor (4 × 100 numa passada
  deixam ela com 400).
- **Vender:** volta $400 em 6 s (a energia sai já no começo da desmontagem).

## 4. Em batalha

- **Temperamento: o técnico de plantão.** Metódico, quieto, um pouco nervoso quando a carga sobe. Não aparece até faltar.
- **O ritmo:** o núcleo pulsa devagar (uma respiração a cada ~4 s). É a pulsação da base.
- **O jogador bom** espalha as usinas atrás dos prédios grandes, nunca em fila (uma fila é um ataque de mísseis só); guarda uma
  usina "a mais" pra não cair em falta quando perder uma.
- **A IA** olha a energia da obra que vem: a obra de −3 não sobe sem a usina antes.
- **Sozinha e ferida:** segue dando os +5 cheios até cair (no ZH também: a energia não cai com a vida).

## 5. A vida

- **Parada:** o núcleo aceso em branco quente no topo, pulsando devagar; o vapor branco saindo das aletas em sopros; os
  isoladores com o leve brilho do arco nos dias úmidos (só de noite se vê).
- **Na obra:** o anel sobe peça a peça no andaime; no pronto o núcleo acende do centro pra fora em ~1 s.
- **Ferida:** com 70% o vapor vira fumaça cinza num lado; com 30% o núcleo pisca irregular, faísca nos isoladores e o fogo no
  bloco de controle.
- **Sem energia na base:** ela mesma continua acesa (é ela que dá): quem apaga são os outros prédios. O núcleo fica mais forte,
  como sobrecarregado.

## 6. O visual

- **De cima:** o círculo do reator com os anéis (2 ou 3) e o ponto branco do núcleo no meio, o retângulo do bloco de controle
  do lado. Lê como "usina" no relance, e não pode ser confundida com o domo do Strategy Center (que é prato, não anel).
- **Assinatura:** os anéis com o núcleo branco quente.
- **Cores:** metal cinza-claro, concreto claro; o núcleo branco quente (nunca o azul neon); a cor do jogador nas faixas dos
  anéis e numa lona amarrada no bloco.
- **Nunca:** a ampulheta de resfriamento; o brilho azul de ficção científica; a cúpula de usina nuclear civil.

## 7. Tiro, projétil, impacto, destruição

- **Impacto:** o canhão amassa a chapa dos anéis e solta o vapor pelo furo (um jato branco por 2 s); o fuzil faz faísca.
- **A morte:** a fusão **apaga, não derrete**: quando o confinamento falha, o plasma esfria e a reação para (é a física de
  verdade, e o contraste com o reator chinês que explode). Então: o núcleo dá um clarão branco curto (0,2 s) e apaga; o vapor
  preso sai num jorro alto; os anéis racham e o bloco de controle pega fogo. Sem onda de dano em volta (os dados não têm
  `death_blast`).
- **O que fica:** o anel rachado e escuro, o bloco queimado, os cabos soltando faísca por ~10 s.

## 8. O som

- **Seleção (variantes):** o zumbido elétrico grave subindo um tom; o "tum" de um contator fechando; o sopro do vapor; o bipe do
  painel.
- **As falas** (o técnico no rádio; voz jovem, precisa, um pouco tensa):
  - selecionado: "Reactor nominal." / "Power grid, go ahead." / "Core is stable." / "Output holding at full."
  - pronto: "Reactor online." / "Grid is hot." / "Power on the line." / "New core, all green."
  - sob ataque: "They're hitting the grid!" / "Reactor under fire!" / "Containment taking hits!" / "Protect the power!"
  - sem energia (na base): "We're overdrawn." / "Grid can't hold the load." / "Need more power, Command." / "Brownout across the base."
- **O corpo:** o zumbido de transformador (o grave de rede, 60 Hz e os harmônicos), as bombas de refrigeração, o chiado do vapor a
  cada pulso do núcleo, o "tic-tic" dos relés.
- **A obra:** as chapas curvas encaixando, o aperto dos parafusos grandes, o cabo sendo puxado; no fim o "power-up": o zumbido
  subindo do grave ao médio em ~1 s.
- **A morte:** o zumbido cai de tom de uma vez (o "power-down"), o estalo do clarão, o jorro de vapor longo, o metal rachando, o
  fogo pequeno nos cabos.

## 9. Ideias pra depois

- **O núcleo mostra a conta da base:** o brilho do núcleo pela folga de energia (branco calmo com sobra; pulsando rápido e
  laranja-branco perto do zero). O jogador lê a energia olhando pra base, não pro HUD.
- **A sobrecarga:** um botão no prédio: +3 de energia por 45 s, o núcleo fica branco-azulado intenso, o vapor dobra e o chão em
  volta tremula de calor; depois 30 s em +2 pra esfriar. Salva a base no aperto, cobra depois.
- **O ímã supercondutor** (a melhoria de energia, no lugar das "barras de controle" do ZH, que são de reator de fissão e não
  fazem sentido em fusão): mais energia por usina, pago no próprio prédio.
- **O cabo que se vê:** linhas de cabo no chão da usina até os prédios que ela alimenta (só desenho), que faíscam quando a usina
  apanha. A rede da base vira desenho.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#usa_power_plant` (feito no fim, com o modelo provisório como anexo de forma).
