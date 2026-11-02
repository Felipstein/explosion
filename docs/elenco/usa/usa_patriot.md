# Patriot Missile System (USA, a defesa da base)

## 1. Ficha rápida

| | |
|---|---|
| Papel | a defesa de base: a parede que segura a onda de tanques e derruba aeronave |
| Custo e tempo | $1 000, 25 s |
| Vida e blindagem | 1 000, prédio |
| Energia | −3 (**sem energia fica muda**) |
| Visão | 94,8 m |
| Lote | 8 × 8 m (a menor peça da base) |
| Arma de chão | 4 mísseis de 30 explosivo, um a cada 0,25 s, 2 s pra recarregar as 4, alcance 59,2 m |
| Arma de ar | 4 mísseis de 25 explosivo (área de 1,3 m), mesmo ritmo, alcance 92,2 m |
| Mira assistida | as Patriots do mesmo dono a até 60 m passam a mirar no alvo da que atirou |
| Requer | Cold Fusion Reactor |
| Conta pra vitória | não |

## 2. A ideia

**A caixa lançadora.** Um bloco octogonal blindado baixo com o prato giratório em cima, a caixa de 2 × 2 células inclinada
pra frente, o mastro do radar girando ao lado e a faixa de perigo pintada no chão. Pequena, decidida, sempre virando pra onde
vem o perigo.

- **A palavra de cima:** a CAIXA de 4 células apontada pra fora (o quadrado inclinado que gira).
- **Inspiração real:** o sistema Patriot do Exército americano (o lançador de 4 contêineres, o míssil guiado por radar). Na
  vida real o radar do Patriot é um **painel fixo de varredura eletrônica**, que não gira, e o lançador fica num reboque; o nosso
  gira o prato e senta num bloco blindado pela leitura de RTS (decisão de arte, não engano).
- **Por que existe:** a USA é pouca e cara; a base precisa aguentar a onda sozinha enquanto o exército está fora. A Patriot é
  2,45× mais forte por dólar que o tanque (Lanchester, `docs/V4_ARSENAL.md` §3.4): por isso a onda de tanque morre nela, e por
  isso existe o Tomahawk.

## 3. Como funciona

- **No jogo:** 120 de dano a cada 2,75 s no chão (43,6/s, igual em tanque, soldado e prédio: o explosivo entra 100% em todos). O
  míssil de verdade na simulação: sai inclinado a 35°, guia e explode no contato, ~0,7 s de voo até 59 m.
- **No ar:** prefere a aeronave. 36/s no jato, 47/s no Apache (o explosivo entra 130% no helicóptero), 18/s no Chinook (50%).
  Derruba um Apache em ~4,4 s (medido) e um Raptor em ~4 s.
- **Quanto leva pra matar:** um Battle Tank (480) em ~10 s sozinha, ~5 s com outra ajudando; um Ranger em ~3,5 s.
- **Sem desperdício:** larga o alvo que os mísseis já no ar vão matar.
- **Forte contra:** a onda de tanques, o raide de Technical e Stryker, o Apache, o Raptor (numa passada leva ~100 e volta com 60),
  a infantaria.
- **Fraca contra:** o Tomahawk (enxerga a 70 m e atira a 85; a Patriot só alcança ~63 m contando o raio dele), o ataque de mísseis
  (~665 de 1 000, medido), a falta de energia (matar a usina apaga ela), dois Apaches juntos.
- **Ela é o contra de:** o rush de tanque, o ar cedo. **O contra dela:** o Tomahawk, o ataque de mísseis e quem mata a usina.

## 4. Em batalha

- **Temperamento: a sentinela.** Fria, rápida, sem emoção. Não ameaça; avisa uma vez e atira.
- **O ritmo:** TSS-TSS-TSS-TSS ... dois segundos de silêncio ... TSS-TSS-TSS-TSS. A rajada de 4 em 0,75 s é a assinatura.
- **O jogador bom** põe duas ou três a até 60 m umas das outras (a mira assistida concentra o fogo), espalhadas pra uma rajada de
  Tomahawk não pegar duas, e sempre com uma usina de folga.
- **A IA** põe uma Patriot a mais por aeronave inimiga vista; no ataque, pesa pouco a Patriot, a não ser que tenha artilharia
  junto.
- **Sozinha:** atira até o fim; sem energia, a caixa volta pra frente e para (o silêncio é o aviso).

## 5. A vida

- **Parada:** o radar gira no mastro (12 rpm) e **para sem energia**; a sirene âmbar do mastro pisca; a caixa varre devagar
  quando volta do alvo (2 s depois do último, volta pra frente); as luzes de serviço no bloco.
- **Atirando:** a caixa vira a 180°/s, assenta, e as 4 células abrem uma depois da outra; o leque dos 4 rastros sai aberto (10°);
  o laser vermelho fino liga as baterias que ajudam (a mira assistida desenhada, provisória).
- **Na obra:** o bloco octogonal sobe primeiro, a caixa desce no guindaste por último.
- **Ferida:** fumaça saindo das grelhas de escape; com 30%, faísca no prato do radar e o giro engasgando.
- **Sem energia:** o radar para, a sirene apaga, a caixa abaixa e volta pro centro.

## 6. O visual

- **De cima:** o octógono pequeno com a caixa quadrada inclinada em cima e o mastro fino com o prato; a faixa de perigo em volta
  no chão.
- **Assinatura:** a caixa lançadora de 2 × 2 células, com a faixa azul (a cor do jogador).
- **Cores:** painel cinza-claro, concreto; faixa de perigo amarela e preta; o azul do jogador na faixa da caixa e num painel do
  bloco; a sirene âmbar.
- **Nunca:** a caixa inteira azul; o radar gigante maior que a caixa; míssil aparecendo de fora da célula.

## 7. Tiro, projétil, impacto, destruição

- **A boca:** a tampa da célula estoura pra fora, o clarão curto e branco-alaranjado, a nuvem de fumaça branca do motor de
  partida empurrada pra trás pelas grelhas.
- **O projétil:** o míssil pequeno e rápido com o fogo do motor atrás virando o rastro branco (110 m/s). **O que o dono pediu
  (30/10, reprovado como está):** um míssil modelado visível, rápido, simples, discreto; o rastro menor, menos borrado, mais
  textura, menos bolonas soltas. A régua de visível é o Cruise Missile.
- **O impacto:** a ogiva pequena (4 kg de TNT no desenho; 8 kg no ar): a bola de ~3 m acesa ~0,4 s, a fuligem que afina e o anel
  de areia baixo; no tanque, o tranco de ~1,5°; no ar, o estouro maior e a nuvem de estilhaço. O pedido aberto: cada arma com a
  explosão dela, sem repetir a do tanque.
- **A morte:** as células que ainda tinham míssil queimam (2 ou 3 estouros curtos, um rastro solto subindo torto), a caixa cai do
  prato, o mastro do radar dobra. Fogo pequeno e quente, fumaça branca e preta misturada.
- **O que fica:** o bloco queimado com a caixa caída ao lado.

## 8. O som

- **Seleção (variantes):** o servo elétrico da caixa girando (o "zzzt" curto e preciso); o bipe de radar de dois tons; o "clack"
  das travas das células; o rádio digital.
- **As falas** (a operadora da bateria; voz feminina, fria, curta, profissional):
  - selecionado: "Patriot, online." / "Air defense, ready." / "Radar's clear." / "Sector covered."
  - travando: "Contact." / "Tracking." / "Target locked." / "Engaging."
  - atirando: "Missiles away." / "Salvo out." / "Splash one." / "Good kill."
  - sob ataque: "Battery taking fire." / "We're hit, still tracking." / "Need cover on the battery." / "Holding the line."
  - pronto: "Battery operational." / "Patriot up and scanning." / "Air defense established." / "Grid locked in."
- **O corpo:** o zumbido do gerador do bloco, o motor do radar (o tique a cada volta), o servo da caixa com o "tum" do batente
  quando assenta.
- **A arma:** cada tiro é o "PSHHH" seco do motor de partida + o assovio que sobe e some; os 4 em 0,75 s viram uma rajada
  rasgada; no fim a recarga: o ventilar das células e o "clack-clack" das tampas fechando.
- **A morte:** os estouros da munição nas células (secos, em sequência), o rastro solto assobiando torto, o metal da caixa
  caindo, o radar raspando no chão.

## 9. Ideias pra depois

- **A interceptação (o Patriot de verdade é antimíssil):** no modo de defesa de míssil, a bateria gasta a rajada inteira pra
  derrubar um míssil grande inimigo (o Tomahawk, o ataque de mísseis, depois o Scud); uma interceptação por recarga, com a
  chance pela distância. O cerco de Tomahawk deixa de ser certeiro, mas a bateria que intercepta não atira no tanque.
- **O alcance que se vê:** com a Patriot selecionada, o anel do alcance de chão e o de ar no terreno, diferentes (a crítica 7 do
  ar no ZH).
- **O radar que alimenta o time:** a Patriot com energia revela as aeronaves inimigas até 120 m (o radar enxerga mais longe que
  o olho), sem revelar o chão.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#usa_patriot` (feito no fim, com o modelo provisório como anexo de forma).
