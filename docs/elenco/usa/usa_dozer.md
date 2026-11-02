# Dozer (USA, o construtor)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o construtor: ergue os prédios, conserta e recolhe a sucata do campo |
| Custo e tempo | $1 000, 5 s |
| Vida e blindagem | 250, tanque |
| Velocidade | 7,9 m/s (a do tanque), mas arranca devagar (7,9 m/s²: ~1 s pra pegar a velocidade); giro 90°/s |
| Visão | 52,7 m |
| Arma | nenhuma |
| Esmagador | nível 2: passa por cima de soldado, carro civil e palmeira |
| Constrói | Command Center, usina, Supply Center, War Factory, Barracks, Patriot, Airfield, Drop Zone, Strategy Center |
| Conserta | prédio dele ou do aliado: 2% da vida cheia por segundo, de graça |
| Recolhe sucata | a carcaça dá 15% do custo de quem morreu (o Battle Tank: $135 em 9 s); o entulho de prédio, 10% |
| Sai de | Command Center (a partida começa com 1) |

## 2. A ideia

**O trator de mineração blindado.** Comprido como o tanque e mais baixo, com a lâmina amarela maciça de uma peça na frente, a
esteira em triângulo com a roda motriz alta atrás, o escarificador amarelo, o braço robótico dobrado sobre a traseira, as duas
chaminés e o corrimão amarelo em volta do deck. A arte aprovada (`docs/concept/units/usa_dozer.webp`) virou o modelo aprovado
(13/10: "gostei do trator, passa pro jogo usar").

- **A palavra de cima:** a LÂMINA amarela (o retângulo amarelo largo na frente do corpo claro).
- **Inspiração real:** o trator de esteira pesado (a roda motriz elevada, que dá a esteira em triângulo, é a marca da Caterpillar;
  o porte é o do D9, ~49 t) na versão blindada que os exércitos usam pra abrir caminho e cavar posições.
- **Por que existe:** é a mão da base. Sem Dozer não há prédio; com dois, a base cresce no dobro do ritmo. E agora ele limpa o
  campo: a batalha deixa sucata, e a sucata volta como dinheiro.

## 3. Como funciona

- **No jogo:** constrói com o andaime e a solda; conserta a 2%/s (um Dozer por prédio; o CC de 5 000 enche em 50 s do zero); recolhe
  a carcaça ou o entulho parado do lado (um Dozer por sucata). Pode vender e mandar consertar pelo HUD.
- **A sucata (simulação 30):** carcaça = 15% do custo em 3 s + 1 s a cada $150; entulho = 10% em 4 s + 1 s a cada $200. A IA manda o
  Dozer parado recolher a sucata da base (até 90 m de casa).
- **Forte contra:** nada (não atira); esmaga o soldado inimigo que estiver no caminho dele.
- **Fraco contra:** tudo. 250 de vida: o Stryker mata em ~33 s (a .50 entra 25%), o Battle Tank em ~8 s (5 tiros), o Apache em
  ~3,5 s (os 4 mísseis e o canhão). É o alvo nº 1 do raide.
- **Ele é o contra de:** a base inimiga que fica sem conserto. **O contra dele:** o raide rápido (Stryker, Technical, Apache).

## 4. Em batalha

- **Temperamento: o operário.** Bem-humorado, pé no chão, trabalhador; não é soldado e não finge ser. Reclama do perigo, mas vai.
- **O ritmo:** lento pra começar, constante depois. O motor pesado acelerando, a lâmina baixando, o trabalho.
- **O jogador bom** tem 2 Dozers cedo, manda um consertar a frente durante o cerco (a Patriot que se conserta vale o dobro) e
  recolhe as carcaças depois de cada luta ganha (o campo vencido paga a batalha).
- **A IA** usa o Dozer pra obra, conserto e sucata em casa; nunca leva o Dozer pra frente.
- **Sozinho:** foge (não tem arma). Ferido, segue trabalhando.

## 5. A vida

- **Parado:** o diesel em marcha lenta pulsa fumaça cinza pelas duas chaminés; a lâmina descansa no chão; a antena balança.
- **Andando:** a esteira em triângulo com o clank pesado, a lâmina levantada, a poeira das lagartas (o mesmo sistema do tanque,
  aprovado).
- **Construindo:** a sirene amarela do teto da cabine gira (90 rpm) e acende **só enquanto ele constrói**; a lâmina abaixa e sobe
  devagar como quem empurra terra; o braço robótico se estica pro andaime.
- **Recolhendo:** encostado na carcaça, a lâmina empurra, o braço puxa uma chapa, a carcaça diminui e some.
- **Ferido:** fumaça preta de diesel pelas chaminés em vez da cinza; abaixo de 30%, o motor falhando (soluços de fumaça) e faísca
  na esteira.

## 6. O visual

- **De cima:** o corpo claro comprido, a lâmina amarela larga na frente (mais larga que as esteiras), o braço amarelo dobrado
  atrás, as duas chaminés redondas.
- **Assinatura:** a lâmina amarela + a sirene na cabine.
- **Cores:** o cinza-claro facetado do tanque USA; o amarelo industrial na lâmina, no escarificador, no braço e no corrimão; a cor
  do jogador na lona amarrada no deck de trás e na faixa segmentada das saias.
- **Nunca:** o Dozer inteiro amarelo (vira máquina civil); a lâmina pintada de azul; arma no teto.

## 7. Tiro, projétil, impacto, destruição

- **Impacto:** o tiro na lâmina faz a faísca grande e o "gongo" (a lâmina é a parte mais grossa); no corpo, o furo e a fumaça.
- **A morte:** o diesel: a bola de fogo laranja do tanque de combustível, a cabine estoura, a lâmina cai pra frente no chão. A
  carcaça fica queimando ~48 s com a fumaça preta (o diesel da USA está adiado só nos pesados; no Dozer a fumaça preta de diesel é
  a coerente, a decidir com ele).
- **O que fica:** a carcaça com a lâmina no chão, que outro Dozer pode recolher ($150 em ~9,7 s).

## 8. O som

- **Seleção (variantes):** o motor diesel acelerando grosso; o hidráulico da lâmina (o chiado e o "tum"); a buzina de trabalho
  curta; o rádio de obra (mais chiado que o militar).
- **As falas** (o operador; voz de homem de obra, rouca, simpática, um pouco irônica):
  - selecionado: "Dozer here." / "What're we building?" / "Blade's ready." / "Construction, go."
  - andando: "On my way." / "Rolling." / "Coming through." / "Heading out."
  - construindo: "Breaking ground." / "Let's build something." / "Foundation's going in." / "Give me a minute here."
  - consertando / recolhendo: "Patching her up." / "Fixing what they broke." / "Hauling scrap." / "Waste not."
  - sob fogo: "I'm not built for this!" / "Taking fire, I'm unarmed!" / "Get me some cover!" / "They're shooting the dozer!"
  - pronto: "Dozer ready for work." / "Builder reporting for duty." / "Fueled and ready to dig." / "Let's get to work."
- **O corpo:** o diesel pesado (mais grave e mais lento que o do caminhão), a esteira com o rangido do triângulo (a roda alta
  cantando), o hidráulico da lâmina, o "bip-bip" de ré.
- **O trabalho:** a lâmina raspando o chão, o braço robótico (servo), a solda da obra, o metal sendo dobrado na sucata.
- **A morte:** o estouro do diesel (abafado e grave), a cabine estalando, a lâmina batendo no chão (o "clang" seco), o fogo de
  diesel crepitando.

## 9. Ideias pra depois

- **A rampa e o aterro (o "campo que muda", a lista da `docs/VISAO_DO_JOGO.md`):** o Dozer abre uma rampa no barranco (caminho
  novo) e levanta um aterro de areia onde o tanque se esconde até a torre (o "hull-down" de verdade: a mureta que protege o tanque,
  `docs/IDEIAS_JOGABILIDADE.md` §1). O tanque atrás do aterro leva menos dano de frente.
- **Tapar a cratera:** o Dozer apaga a cratera e o chão machucado onde passa a lâmina; a estrada esburacada volta a ser rápida.
- **Abrir a rua:** empurra a carcaça e o entulho pra fora do caminho (rápido, sem recolher), pro comboio passar no meio da luta.
- **O engenheiro pesado** (`docs/IDEIAS_JOGABILIDADE.md` §5): o trator grande com o braço que segura os painéis, pra obra de muro,
  ponte e base avançada, com a patente.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#usa_dozer` (feito no fim, com o modelo provisório como anexo de forma).
