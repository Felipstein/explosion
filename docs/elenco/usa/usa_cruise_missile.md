# Cruise Missile Strike (USA, o poder do general; a munição `usa_cruise_missile`)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o poder da patente 2: uma salva de mísseis de cruzeiro em qualquer ponto já explorado |
| Custo | grátis; 180 s de recarga depois de cada uso (pronto assim que libera) |
| Libera | na patente 2, com o Command Center pronto e vivo; cresce na patente 4 |
| A salva | patente 2: **6 mísseis num círculo de 12 m**; patente 4: **9 em 14 m**; um a cada 0,2 s, num girassol fixo (sem sorteio) |
| Cada míssil | 150 explosivo em área: cheio até 3 m da borda do alvo, 50% em 6 m |
| O voo | nasce 150 m atrás do ponto (do lado de quem lançou) e 45 m acima; 55 m/s, guia a 90°/s; as explosões caem 2,8–3,8 s depois do clique |
| O aviso | quem é atacado vê o anel vermelho e "Missile strike incoming!" |

## 2. A ideia

**A chuva que vem do lado da sua base.** Seis rastros brancos entrando baixos pelo céu, em leque, e caindo em girassol sobre o alvo,
um a cada 0,2 s. É o "acerta primeiro" da USA na forma mais pura: o que não foge, morre.

- **Inspiração real:** o míssil de cruzeiro Tomahawk (subsônico, voa baixo, a ogiva real de ~450 kg). A ogiva de verdade não cabe na
  leitura do RTS (a bola teria ~7,7 m); o jogo usa a explosão clássica em volume e a areia na escala do Tomahawk (~150 kg de TNT
  equivalente só pra levantar a areia).
- **Por que existe:** vale um prédio de renda (~$700–1 000 por minuto), não mais; ajuda quem está atrás (o valor é fixo) e pune o
  bolo parado.

## 3. Como funciona

- **No jogo (medido):** a Patriot leva ~665 de 1 000; a War Factory ~900 de 2 000; a Drop Zone ~890 de 1 200 (74%); a usina morre
  (~800 de 800) e as Patriots dela apagam; 6 tanques parados: ~1 morto e o resto com ~45%.
- **Premia a atenção:** um tanque a 7,9 m/s sai de 12 m em menos de 2,5 s; o anel avisa ~3 s antes.
- **A IA** dá nota a cada ponto inimigo visto a cada 3 s (a zona 3×, a defesa, a usina e a artilharia 1,5×, a unidade andando 0,4×) e
  dispara acima de 2 000.
- **Forte contra:** usina, Patriot, Drop Zone, cerco parado, bolo de unidades. **Fraco contra:** quem se mexe; matar o CC desliga.

## 4. Em batalha

- **Temperamento: a sentença.** Frio, inevitável, impessoal. O general não discute; assina.
- **O ritmo:** o clique, o silêncio de ~2 s com o assovio crescendo, e BUM-BUM-BUM-BUM-BUM-BUM em 1 s.

## 5. A vida

- O botão acende "READY" na coluna de poderes; F1 põe o círculo azul de 12 m no cursor; o clique esquerdo dispara.

## 6. O visual

- Os rastros brancos da Patriot aprovada (sem fumaça nova), o anel no chão, a explosão clássica em volume na metade pesada (1 de cada
  2), a ogiva de 12 kg da mesma família de luz na outra metade, a areia densa simulada explodindo em volta e por baixo das bolas e a
  onda de base rolando (aprovada em 24/10: "que acerto foda!"), a cratera C grande e funda. **A régua de visível de todos os mísseis**
  (30/10: "um dos mais bem feitos"); aceito, não aprovado como definitivo (29/10).

## 7. Tiro, projétil, impacto, destruição

- **O projétil:** o míssil no rastro branco, entrando baixo e inclinando pro alvo nos últimos metros.
- **O impacto:** a bola clássica, os torrões de terra voando (os maiores levantam o respingo de areia no 1º toque), a cratera de ~6 m
  de boca, as unidades sumindo na areia e lendo de novo ~2 s depois.

## 8. O som

- **A chegada:** o assovio de turbina de cada míssil (o motor a jato pequeno, agudo), crescendo e passando em Doppler por cima da
  câmera, 6 vezes defasadas.
- **As falas:** o general (pronto): "Strike package ready." / "Cruise missiles on standby." / "Authorize the strike." / "Missiles
  fueled, awaiting coordinates." O lançamento: "Coordinates locked." / "Birds away." / "Strike inbound." / "Impact in five."
- **O inimigo ouve:** o alerta grave de sirene e "Missile strike incoming!" (o aviso do jogo).
- **O impacto:** cada explosão com o estalo e o corpo grave (`docs/CONCEITO_SOM.md`: o estalo, o corpo de 30–150 Hz, os destroços, a
  cauda do deserto), 6 vezes em 1 s, a mixagem abaixando o resto; depois a terra caindo como chuva grossa.

## 9. Ideias pra depois

- **A interceptação:** a Patriot (no modo antimíssil) e o laser do Paladin podem derrubar parte da salva; o ataque deixa de ser certo
  contra quem investiu na defesa.
- **A rota que se vê:** os mísseis nascem fora do mapa e entram baixos seguindo o relevo; o antiaéreo inimigo no caminho pode
  derrubar um ou dois (a rota vem sempre do lado da base de quem lançou: o defensor aprende onde pôr o Linebacker).
- **A foto do estrago:** com o Strategy Center vivo, o ponto fica revelado 5 s depois das explosões.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#usa_cruise_missile` (feito no fim, com o modelo provisório como anexo de forma).
