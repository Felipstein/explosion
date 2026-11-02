# Ranger (USA, o fuzileiro)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o soldado básico: vê, segura a vila, caça a infantaria |
| Custo e tempo | $225, 5 s |
| Vida e blindagem | 180 (a mais alta dos soldados básicos), infantaria (perfurante 10%, arma leve 100%, explosivo 100%, **esmagamento 200%**, fogo 150%) |
| Velocidade | 5,3 m/s, giro 500°/s |
| Visão | 26,3 m |
| Arma | fuzil: rajada de 3 tiros de 5 (0,1 s entre eles), 0,7 s pra próxima rajada, 26,3 m (16,7 de dano por segundo); só no chão |
| Requer | Barracks |

## 2. A ideia

**O soldado de capacete e colete.** O homem de farda areia, o capacete e o colete na cor do jogador, o fuzil saindo pra frente (o
traço comprido que diz "armado"). Soldado solto, como no ZH (decisão dele, 30/10), a 1,25× o tamanho real pra ler de cima.

- **A palavra de cima:** o FUZIL pra frente e a sombra comprida no sol baixo (a 20° de sol, o homem projeta ~4,8 m de sombra: é
  a melhor leitura do soldado na câmera alta).
- **Inspiração real:** o Ranger do Exército americano (a infantaria leve de elite, o fuzil de 5,56 mm, o boné bege fora de combate).
  Os números são do Ranger do ZH.
- **Por que existe:** os sete empregos que só a infantaria faz (`docs/ESTUDO_INFANTARIA_DESIGN.md` §3.1): ver primeiro, segurar
  prédio, emboscar, capturar. Hoje ele faz o mais simples: mata soldado e dá olho barato.

## 3. Como funciona

- **No jogo:** 16,7/s no soldado (mata outro Ranger em ~10 s, um Javelin em ~5,6 s), 8,3/s no prédio (50%), 4,2/s no veículo (25%:
  uma Technical leva ~43 s, um tanque quase 2 min). Não atira no ar.
- **O esmagamento:** o veículo inimigo que vem pra cima dele com o casco cobrindo o centro mata na hora; parado ou indo embora, não.
- **A morte (a do ZH):** cai como tábua em ~0,45 s, fica 3 s, afunda e some aos 8 s; o esmagado fica achatado. Não deixa carcaça.
- **Forte contra:** o Javelin, o RPG, o Caçador de tanque e o Rebel (ganha de todos de perto pela vida maior), o Dozer e o caminhão
  devagar.
- **Fraco contra:** a metralhadora (o Stryker mata em ~5 s, o Quad Cannon em ~2 s, o canhão do Apache em ~3 s), a Gattling, o fogo
  (150%), o explosivo em área (Tomahawk, Patriot: ~3,5 s), o tanque que passa por cima.
- **O canhão quase não fere ele** (10%: 30 tiros de tanque): quem mata soldado é a metralhadora e o esmagamento.

## 4. Em batalha

- **Temperamento: o profissional da linha de frente.** Disciplinado, calmo sob fogo, orgulhoso; fala pouco e baixo, sempre "nós".
- **O ritmo:** TA-TA-TA ... TA-TA-TA, três piscadas rápidas a cada 0,9 s. **O desenho tem de mostrar a cadência real** do fuzil
  (~75 ms entre tiros, três piscadas), não um tiro lento por segundo (o "tiro lentíssimo" reprovado em 30/10).
- **O jogador bom** usa os Rangers à frente dos tanques (o olho e a limpeza da infantaria inimiga), atrás dos Javelins (que precisam
  de olho), e nunca no aberto contra metralhadora.
- **Sozinho:** segura; o grupo de 4–6 é que vale.

## 5. A vida

- **Parado:** respira (o corpo sobe e desce de leve), olha em volta; com o tempo, agacha (a ideia da postura).
- **Andando:** as pernas balançam pelo chão andado (o ciclo de 3,6 m: não patina), o corpo sobe e desce duas vezes por passada, a
  pegada na areia.
- **Atirando:** o corpo inteiro gira pro alvo; o clarão pequeno da boca, o traçante fino laranja em metade dos tiros, o erro
  desenhado costurando a areia em volta do alvo.
- **Ferido (abaixo de 35%, o do ZH):** a corrida mancando, o tiro mais lento de levantar a arma (só desenho).

## 6. O visual

- **De cima:** capacete, ombros e o fuzil; a sombra comprida; o grupo espaçado (o raio de 1 m entre soldados).
- **Assinatura:** o capacete e o colete na cor do jogador.
- **Cores:** farda areia (0,36/0,31/0,22), o capacete e o colete pintados no azul do time.
- **Estado:** **reprovada a infantaria provisória** (30/10: "horrível [...] quadradinhos, sem detalhes"); hoje é o corpo do modelo da
  Unreal 5.8 com as peças da farda e o fuzil do modelo. O final é dele (MPFB2 por script e as animações do modelo e do Mixamo: nada
  comprado).
- **Nunca:** boina no campo; cores vivas; o soldado em tamanho real (vira formiga a 140 m).

## 7. Tiro, projétil, impacto, destruição

- **A boca:** o clarão pequeno (0,15 m) e o pulso de luz curto (o do ZH: 2,6 m, 200 ms), sem fumaça.
- **O projétil:** o traçante de 2,6 m, fino e laranja, em metade dos tiros (o do ZH), nunca o bastão branco.
- **O impacto:** no soldado, o tranco no corpo e o pó do uniforme; **com sangue** (decisão dele em 30/10: tom sério, sem exagero, a
  forma a decidir: a névoa vermelha curta no acerto e a mancha no chão); no veículo, a faísca; no chão, o esguicho de areia (a areia
  das rajadas foi reprovada pela textura em 30/10).
- **A morte:** cai (normal); voa e quica (explosão); some debaixo do casco (esmagado); em chamas, corre e cai (fogo).

## 8. O som

- **Seleção (variantes):** o rádio de ombro (o clique e o chiado curto); o equipamento batendo (o colete, as presilhas); o ferrolho
  do fuzil; a respiração.
- **As falas** (o líder da esquadra; voz jovem, firme, baixa, disciplinada):
  - selecionado: "Ranger, go." / "Boots ready." / "Squad's up." / "On your word."
  - andando: "Moving out." / "Copy, on foot." / "Stay sharp, people." / "Pushing up."
  - atacando: "Contact front!" / "Engaging!" / "Put rounds on 'em!" / "Hostiles down."
  - sob fogo: "Take cover!" / "We're pinned!" / "Man down!" / "Return fire!"
  - pronto: "Ranger ready." / "Fresh out of training, sir." / "Rifle's clean, let's work." / "Reporting in."
- **O corpo:** os passos (na areia: o chiado; no asfalto: o batido), o equipamento sacudindo, a respiração na corrida.
- **A arma:** o fuzil de 5,56: o "TA-TA-TA" seco e rápido em rajada de 3, o eco curto no deserto e longo na vila; a troca de
  carregador a cada tantas rajadas (só som).
- **A morte:** o grito curto (variado, nunca o mesmo duas vezes), o corpo batendo no chão, o fuzil caindo.

## 9. Ideias pra depois

- **A cobertura que a batalha cria (aceita pra testar, 30/10):** a carcaça, a cratera, a ruína e a mureta são cobertura com direção;
  o soldado parado procura sozinho a cobertura a até 6 m e agacha colado nela. O tanque que você mata vira a trincheira do seu
  soldado.
- **A supressão (aceita, 30/10):** a metralhadora prega o soldado (em pé → deitado → pregado), lida pela postura e pela areia
  costurada em volta.
- **O mergulho:** o Ranger em alerta que vê o tanque chegando se joga de lado (o contra-jogo do esmagamento).
- **A granada de luz (o flash-bang do ZH):** limpa o prédio guarnecido sem destruir; e o rapel do Chinook direto no telhado da vila.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#usa_ranger` (feito no fim, com o modelo provisório como anexo de forma).
