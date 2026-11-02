# Worker (GLA, o construtor a pé)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o construtor da GLA: ergue a base, conserta e **desmancha carcaça em dinheiro** |
| Custo e tempo | $200, 3 s (o mais barato e o mais rápido de fazer do jogo) |
| Vida e blindagem | 100, soldado |
| Velocidade | 7,9 m/s (o soldado mais rápido do jogo) |
| Visão | 26,3 m |
| Arma | nenhuma |
| Conserta | 2% da vida por segundo |
| Constrói | Command Center, Supply Stash, Barracks, Arms Dealer, Palace, Stinger Site |
| Sai de | Command Center (a GLA começa com 2) |

## 2. A ideia

**O homem da ruína.** Um civil de roupa de trabalho, sem fuzil, com a ferramenta na mão. Onde a batalha deixa carcaça, ele chega
correndo, desmonta e leva embora em caixas. A GLA não tem trator: tem gente.

- **A palavra de cima:** o SOLDADO SEM ARMA, de roupa areia, sempre agachado mexendo em alguma coisa.
- **Inspiração real:** os catadores de sucata e os trabalhadores das guerras do Oriente Médio e da África, que desmontam o tanque
  queimado e vendem o aço; o pedreiro de obra de adobe.
- **Por que existe:** é o construtor da GLA (no lugar do Dozer) e a identidade dela: **vive da ruína** (`docs/ELENCO.md` §7.3). A
  coleta ficou com a picape, então o Worker fica livre pra construir e sucatear.

## 3. Como funciona

- **No jogo:** constrói os 6 prédios da GLA; conserta prédio e veículo a 2% por segundo; **desmancha carcaça** (15% do custo de quem
  morreu, em 3 s + 1 s a cada $150: a carcaça de um tanque USA dá $135 em 9 s; a de um Overlord, $300 em ~16 s) e **entulho de
  prédio** (10% do custo, em 4 s + 1 s a cada $200: a War Factory da China dá $200 em 14 s). É o mesmo sucateio do Dozer
  (`TestSalvage`, decidido em 30/10).
- **A conta:** 2 Workers ($400) desmanchando as carcaças de uma batalha de 6 tanques ($5 400) tiram ~$800: dois Workers se pagam numa
  luta.
- **Forte contra:** o tempo (3 s pra fazer, 7,9 m/s), a sucata, a base espalhada.
- **Fraco contra:** tudo: 100 de vida, soldado (a Technical o mata em 2 s, o veneno em 2,5 s, o par de Red Guards em ~3,5 s);
  esmagado por qualquer veículo.
- **O contra dele:** qualquer coisa armada. **Ele é o contra de:** a economia do inimigo que perde a guerra da sucata.

## 4. Em batalha

- **Temperamento: o sobrevivente.** Esperto, ligeiro, nervoso, oportunista. Não luta; aproveita.
- **O ritmo:** a corrida curta, o martelo, a chave, o "clang", a caixa; corre pra próxima carcaça.
- **O jogador bom** mantém 3 a 5 Workers: 2 na base, 1 ou 2 seguindo o exército a 20 m atrás, desmanchando cada carcaça assim que o
  combate passa (antes de ela afundar, 48 s + 7 s); e faz Workers sobrando pra base em vários lugares (a GLA não precisa de energia).
- **A IA** usa o Worker pra lista da base e, parado, recolhe a carcaça e o entulho mais perto até 90 m de casa.
- **Sozinho e ferido:** foge (é a coisa mais rápida a pé do jogo); a supressão o prende fácil.

## 5. A vida

- **No jogo:** o corpo humano sem arma (`rifle` vazio), a roupa areia e o colete na cor da facção; as pernas andam com o chão; a
  supressão (abaixa e rasteja); as 6 mortes animadas; achatado se esmagado.
- **Ideia (parado):** agachado, mexendo numa caixa de ferramentas; enxuga o suor; olha em volta (desconfiado).
- **Ideia (construindo):** a pá, o tijolo de adobe, o martelo; ele sobe no andaime de madeira.
- **Ideia (desmanchando):** o maçarico com a faísca azul-laranja e a fumaça na carcaça (volume, acesa pela chama), a chave de roda nas
  esteiras, a peça arrancada jogada pra trás; no fim, a carcaça some e ele sai com uma caixa no ombro.
- **Ideia (consertando):** o mesmo maçarico, a faísca no prédio ou no veículo.

## 6. O visual

- **De cima:** o soldado pequeno de roupa areia, sem o risco do fuzil, com a ferramenta.
- **Assinatura:** a ferramenta (a pá ou o maçarico) no lugar da arma.
- **Cores:** a roupa areia gasta, o lenço; o verde do jogador no colete e no lenço.
- **Nunca:** fuzil; uniforme militar; capacete.

## 7. Tiro, projétil, impacto, destruição

- **O "tiro" dele:** a faísca do maçarico, contínua, com o brilho que acende a carcaça e a fumaça em volta.
- **O impacto nele:** o tranco e o sangue (o ferido); ele larga a ferramenta e corre.
- **A morte (no jogo):** uma das 6 mortes animadas; esmagado, achatado; o corpo fica 8 s.
- **A morte (ideia):** a ferramenta cai do lado e fica no chão (a pá cravada na areia) até o corpo sumir.

## 8. O som

- **Seleção (variantes):** o martelo batendo duas vezes; o maçarico acendendo ("pfft"); a chave de roda rodando a catraca; a respiração
  ofegante.
- **As falas** (inglês; o trabalhador, voz rápida, nervosa, esperta, de quem negocia):
  - selecionado: "Worker." / "What do we build?" / "I can fix that." / "Scrap is money." / "Yes, yes, I'm here."
  - andando: "Going." / "Quick, quick." / "On my way." / "Keeping my head down."
  - construindo: "Building." / "Brick by brick." / "It will stand." / "Mud and steel."
  - sucata: "Good metal here!" / "Strip it." / "Nothing goes to waste." / "That tank is mine now."
  - consertando: "Patching it." / "Hold still, I'm fixing it."
  - sob fogo: "I'm not a soldier!" / "Don't shoot, don't shoot!" / "Run!"
  - pronto: "Worker, ready."
- **O corpo:** os passos rápidos na areia; a caixa de ferramentas chacoalhando.
- **A sucata:** o maçarico chiando, o metal estalando com o calor, a chave de roda, a chapa caindo no chão (o "clang" grande), a caixa
  pousando.
- **A construção:** a pá, o adobe batendo, o martelo, o serrote.
- **A morte:** o grito; a ferramenta caindo.
- **A cara da GLA:** o metal batendo, o improviso, o fervor.

## 9. Ideias pra depois

- **O sucateiro esperto:** o Worker que desmancha uma carcaça do **inimigo** ganha 20% a mais do que da carcaça da própria GLA (a
  pilhagem); de cima, ele arranca a bandeira do inimigo antes de desmontar.
- **A armadilha na carcaça:** em vez de desmanchar, o Worker pode armar a carcaça (5 s, $100): o próximo veículo inimigo que passar a
  até 6 m dela leva 150 de explosivo. A carcaça vira mina; a ruína vira arma.
- **O mutirão:** 3 Workers na mesma obra a fazem 2,5× mais rápido (não 3×: um atrapalha o outro); a GLA ergue uma base inteira em
  minutos com gente.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#gla_worker` (feito no fim, com o modelo provisório como anexo de forma).
