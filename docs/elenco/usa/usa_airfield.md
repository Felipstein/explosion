# Airfield (USA, o ar)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o aeroporto: nasce, rearma e conserta o ar da USA |
| Custo e tempo | $2 500, 40 s |
| Vida e blindagem | 2 000, prédio |
| Energia | −3 |
| Visão | 52,7 m |
| Lote | 40 × 32 m (1,45× a área da War Factory; o maior prédio da base) |
| Produz | F-22 Raptor (nasce na vaga), AH-64 Apache (nasce no heliponto) |
| Libera | o Chinook (que sai do Supply Center) |
| Vagas | 4, em fila, viradas pra 1 pista; o heliponto à parte |
| Rearme e cura | o jato rearma na vaga no tempo do pente (Raptor 8 s) e cura 10 de vida por segundo; o Apache conserta no heliponto |
| Requer | War Factory |
| Conta pra vitória | sim |

## 2. A ideia

**A pista com as quatro vagas.** Um lote compacto e denso: a pista na frente, as 4 vagas pintadas de amarelo viradas pra ela, o
"H" do heliponto, o hangar de teto curvo e a torre com o radar girando. Quando um jato corre na pista com o pós-combustor aceso,
a base inteira parece outra.

- **A palavra de cima:** a PISTA com as 4 vagas (as marcas amarelas em fila).
- **Inspiração real:** as bases avançadas de caça (a pista curta, o abrigo, a torre de controle, a biruta, o caminhão-tanque);
  a regra das 4 vagas é do ZH (o `ParkingPlaceBehavior` 2 × 2) e virou o padrão da série.
- **Por que existe:** o ar é o "acerta primeiro" da USA. Mas o ar caro e sem resposta decidia partidas; por isso o Airfield é o
  3º degrau (depois da fábrica), custa −3 de energia e leva os jatos estacionados junto quando cai (`docs/AEREO.md` §6).

## 3. Como funciona

- **No jogo (o ciclo do jato):** o Raptor reserva a vaga e a pista; **um jato por vez na faixa** (quem decola espera na vaga com o
  motor ligado; quem volta circula a entrada em andares de 6 m); a decolagem passa na frente do pouso. Taxia pelo eixo da pista
  (o "backtrack"), pausa 0,5 s na cabeceira, corre, sobe a 30 m. Volta sozinho sem míssil ou parado 10 s, pousa, taxia, rearma.
- **As vagas cheias travam o botão:** com 4 jatos, o botão mostra o cadeado e "Airfield full: all 4 pads are taken".
- **Airfield destruído:** os jatos estacionados morrem com ele; os do ar procuram outro Airfield com vaga; sem nenhum, circulam
  onde ele ficava e perdem 10% da vida por segundo (morrem em ~10 s).
- **Forte contra:** nada (não atira). **Fraco contra:** tudo que chega no chão (o jato parado é alvo de chão), o ataque de
  mísseis no meio das vagas, o Tomahawk.
- **Vender:** $1 250 em 6 s.

## 4. Em batalha

- **Temperamento: o controlador de voo.** Calmo, ritmado, a voz que organiza o caos. Nunca deixa dois na pista.
- **O ritmo:** um jato a cada ~8 s na decolagem (medido: 7,1 / 15,0 / 23,7 / 33,1 s); o rugido, o silêncio, o rugido.
- **O jogador bom** põe o Airfield no fundo, a Patriot entre ele e a frente, e não deixa os 4 jatos estacionados quando a base
  apanha (manda patrulhar: eles voltam sozinhos).
- **A IA** faz o Airfield depois da fábrica, com a janela de guardar dinheiro; até 3 Apaches e 2 Raptors (quando o inimigo tem
  helicóptero ou Tomahawk).

## 5. A vida

- **Parado:** o radar gira na torre (10 rpm) e **para sem energia**; a biruta no vento do mapa; as luzes da pista acesas; o jato
  estacionado com o motor em marcha lenta (o tremor de calor atrás dos bocais).
- **Trabalhando:** o jato sai da vaga, vira pelo trem do nariz (curva lenta de 60°/s), taxia pelo eixo, para, acende o
  pós-combustor na corrida (a chama com os diamantes de choque e a luz laranja no chão) e sobe. O Apache sobe do "H" levantando o
  anel de poeira.
- **Na obra:** o hangar e a torre sobem; a pista é pintada por último (a faixa central primeiro).
- **Ferido:** fumaça do hangar; com 30%, o tanque de combustível pega fogo (a coluna preta mais alta da base).
- **Sem energia:** o radar para e as luzes da pista apagam (o jato ainda decola e pousa: as vagas não dependem de energia nos
  dados).

## 6. O visual

- **De cima:** a faixa escura da pista na frente de ponta a ponta, as 4 marcas amarelas em fila atrás dela, o "H" claro num
  canto, o hangar de teto curvo e a torre com o prato no fundo.
- **Assinatura:** a pista com as 4 vagas.
- **Cores:** concreto claro, asfalto escuro da pista com as faixas brancas, amarelo das marcas de vaga e da linha de entrada, a
  barra de espera dupla; a cor do jogador nas faixas do hangar, na torre e numa lona.
- **Nunca:** a pista gigante que domina o mapa (o de 40 × 48 m ficou "GIGANTE"); a linha de táxi paralela (o táxi é pelo eixo).

## 7. Tiro, projétil, impacto, destruição

- **Impacto:** o canhão no hangar fura a chapa curva; na pista, a cratera A e o asfalto lascado (só desenho).
- **A morte:** o tanque de combustível explode primeiro (a bola laranja e a coluna preta), o hangar desaba no meio, a torre
  tomba com o radar, os jatos estacionados explodem cada um na sua vaga (em sequência, ~0,3 s um do outro).
- **O que fica:** a pista inteira (o asfalto não some) com o entulho do hangar e o queimado em cada vaga (o jato não deixa carcaça
  nos dados: `wreck: false`).

## 8. O som

- **Seleção (variantes):** o rádio de torre de controle (o chiado e o bipe); o vento na biruta; o radar; o motor de um jato
  parado em marcha lenta (o assovio).
- **As falas** (o controlador de voo; voz grave, pausada, fraseologia de torre):
  - selecionado: "Tower, go ahead." / "Airfield, standing watch." / "Flight line's yours." / "Runway's clear, awaiting tasking."
  - produzindo: "Aircraft on the pad." / "New bird on the line." / "Rotor turning on the pad." / "Fueled and armed."
  - sob ataque: "Airfield under attack!" / "Get those birds off the ground!" / "Hits on the flight line!" / "Tower's taking fire!"
  - pista cheia: "All pads occupied." / "No room on the line." / "Clear a pad first." / "Flight line is full."
  - pronto: "Airfield operational." / "Runway open." / "Tower is up." / "Flight line ready for traffic."
- **O corpo:** o motor do radar, o vento, o compressor do hangar, o jato parado assobiando; à distância, o rugido do pós-combustor
  na decolagem (o maior som da base, com o Doppler da subida).
- **A obra:** o rolo compactando a pista, o hangar de chapa curva, a torre montando; no fim o primeiro "teste" das luzes da pista
  acendendo em sequência.
- **A morte:** o estouro do combustível (o grave longo e a bola), o hangar caindo, os jatos estacionados explodindo um a um.

## 9. Ideias pra depois

- **A pista que se fecha:** a cratera na pista (do Tomahawk, do ataque de mísseis, da artilharia) fecha a decolagem e o pouso até
  um Dozer tapar (o "campo que muda" e a guerra de verdade: negar a pista é a primeira coisa que um ataque aéreo faz). Os jatos no
  ar esperam circulando, e o relógio dos 10%/s não corre enquanto houver Dozer trabalhando.
- **A equipe de solo:** dois mecânicos pequenos e o carrinho de munição que vão até a vaga no rearme e voltam (só desenho): o
  rearme de 8 s se vê.
- **As luzes da noite:** as luzes da pista em sequência na aproximação, o farol da torre, a luz de pouso do jato acendendo a
  pista (do escopo B, ficou de fora).

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#usa_airfield` (feito no fim, com o modelo provisório como anexo de forma).
