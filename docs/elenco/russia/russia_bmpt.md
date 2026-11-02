# BMPT Terminator (Rússia, o apoio de tanque)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o apoio de tanque: caça o soldado (inclusive o antitanque) que mata o tanque, com o míssil contra o blindado |
| Nome na tela | BMPT Terminator (a chave é `russia_bmpt`) |
| Custo e tempo | $900, 10 s |
| Vida e blindagem | 380, tanque |
| Velocidade | 7,9 m/s, giro do casco e da torre 180°/s |
| Visão | 52,7 m |
| Arma 1 | os 2 canhões de 30 mm: gattling 12 a cada 0,3 s, alcance 39,5 m |
| Arma 2 | o míssil Ataka: 50 perfurante a cada 4 s, alcance 50 m, guiado (sai a 30 m/s, acelera até 70, curva 160°/s) |
| No ar | **não atira** |
| Esmagador | nível 2 |
| Requer | Tank Plant |
| Estado | nos dados (`Units.json`) e no modelo provisório; fora do menu até os modelos da Rússia serem importados |

## 2. A ideia

**Os canos gêmeos e os ombros.** O chassi do T-90 com uma torre baixa e larga: no meio, o módulo dos 2 canhões de 30 mm juntos; nos
ombros, as 2 caixas blindadas compridas dos mísseis; por cima da parte de trás, a gaiola com a rede. Não é tanque: é o guarda-costas
do tanque.

- **A palavra de cima:** os 2 CANOS FINOS juntos saindo do meio + as CAIXAS compridas dos mísseis dos 2 lados da torre.
- **Inspiração real:** o BMPT-72 "Terminator" (o veículo de apoio de tanques: os 2 canhões 2A42 de 30 mm e os 4 mísseis Ataka),
  feito pra caçar o soldado antitanque escondido que mata o tanque na cidade.
- **Por que existe:** o T-90M quase não fere soldado (3/s); o BMPT tira o RPG, o Javelin e o Tank Hunter da frente dele.

## 3. Como funciona

- **No jogo:** as duas armas atiram juntas no mesmo alvo, cada uma no seu ritmo. Os canhões: **40/s em soldado** (gattling 100%),
  4/s em tanque e em prédio (gattling 10%). O míssil: **12,5/s em tanque e em prédio**, 1,25/s em soldado. No tanque, somadas, 16,5/s.
- **Quem ele mata:** um Red Guard ou um Rebel (120) em ~3 s, um Ranger (180) em ~4 s, um soldado antitanque (100) em ~2,5 s, 4
  Rangers em ~18 s; uma Technical em ~8 s, uma Quad Cannon em ~16 s, um tanque USA em ~28 s.
- **O par:** o Gattling Tank da China ($800: 37,5/s em soldado e o tiro no ar a 92 m). O BMPT custa $100 a mais, tem mais vida (380
  contra 300), o mesmo alcance (39,5 m) e tem o míssil, mas **não atira no ar**: o ar da Rússia é o Pantsir (o par é só contra a
  infantaria: 0,0188 contra 0,0176 por dólar).
- **Forte contra:** infantaria (inclusive o grupo antitanque), veículo leve de perto.
- **Fraco contra:** tanque (o T-90M e o tanque USA o matam em 7 tiros, 12 s; 3 soldados antitanque em ~3 s), ar, artilharia.
- **O contra dele:** qualquer tanque, o Apache. **Ele é o contra de:** o grupo de RPGs na vila, os Rangers, os Rebels, os Red
  Guards, a Technical que chega perto.

## 4. Em batalha

- **Temperamento: o guarda-costas.** Desconfiado, agressivo, sempre olhando as janelas e as ruínas. Atira antes do outro.
- **O ritmo:** o DUM-DUM-DUM duplo e contínuo dos 30 mm, com o "fuuush" do míssil a cada 4 s por cima.
- **O jogador bom** põe 1 BMPT pra cada 2 ou 3 T-90M, no meio da linha, virado pra onde o soldado se esconde (a vila, o entulho, as
  carcaças); nunca na frente dos tanques.
- **A IA** o põe na mistura da fábrica como armado comum (peso 1).
- **Sozinho e ferido:** contra tanque, perde; foge a 7,9 m/s.

## 5. A vida

- **No jogo:** a torre gira 180°/s; os 2 canos recuam (peça `_Barrel`); o traçante e o clarão pequeno (`light_gun`: 0,4 m, o risco de
  3,5 m, o espalhado de 2 m); o míssil modelado em voo com o rastro (`missile_model`, 1 m); as esteiras de elos e a poeira; os
  estados de dano do veículo.
- **Ideia (parado):** os canos fazem uma varredura em altura (o BMPT de verdade eleva os canhões alto pra atirar no andar de cima dos
  prédios), a torre vira devagar de janela em janela.
- **Ideia (atirando):** as cápsulas caindo dos lados do módulo; a tampa redonda da caixa do Ataka salta no disparo e fica aberta até a
  recarga.
- **Ideia (ferido):** uma das caixas de míssil fica torta, aberta.

## 6. O visual

- **De cima:** o chassi igual ao do T-90M (7,2 × 3,9 m), a torre mais larga com os 2 canos finos juntos no meio, as caixas compridas
  dos mísseis nos dois lados, a gaiola com a rede vermelha sobre a parte de trás e a lona vermelha dobrada do lado, o rolo vermelho
  atrás.
- **Assinatura:** os canos gêmeos (finos e juntos: nenhum outro veículo tem).
- **Cores:** o aço frio azulado com as manchas grandes pretas e cinza-claras; a cor do jogador na rede, na lona e no rolo do motor.
- **Nunca:** o tambor rotativo (é do Gattling Tank da China); o canhão grosso de tanque; Z ou V; a faixa branca.

## 7. Tiro, projétil, impacto, destruição

- **A boca:** os 2 clarões alternados, pequenos e rápidos; a fumaça fina saindo do freio de boca furado; o míssil: o "pop" da caixa,
  a nuvem branca curta e o motor acendendo.
- **O projétil:** o traçante curto; o míssil com a chama e o rastro branco fino, corrigindo a rota.
- **O impacto:**
  - no soldado: a linha de poeira andando até ele e o estouro pequeno de cada granada de 30 mm (é explosiva de verdade);
  - no tanque: a faísca dos 30 mm e, a cada 4 s, o estouro de carga oca do míssil (o clarão, o jato de faísca, o furo escuro);
  - no prédio: as lascas e o buraco do míssil.
- **A morte (no jogo):** a morte de veículo; a carcaça fica 48 s queimando e afunda em 7 s; sucata ($135 em ~9 s) pro construtor.
- **A morte (ideia):** a munição de 30 mm cozinha em estalos rápidos; os mísseis das caixas saem tortos girando no chão (sem dano); a
  torre fica.

## 8. O som

- **Seleção (variantes):** o "clack-clack" da culatra dupla; o servo da torre; o diesel V-12; o rádio de onda curta.
- **As falas** (inglês, com o sotaque russo leve; o atirador, voz jovem, tensa, rápida):
  - selecionado: "Terminator ready." / "Watching the windows." / "Infantry is mine." / "Covering the tanks." / "Eyes on the ruins."
  - andando: "Moving with the tanks." / "Escorting." / "Close behind."
  - atacando: "Infantry, engaging!" / "Guns!" / "Missile away!" / "Clearing the building."
  - sob fogo: "Taking hits!" / "Tank on us!" / "Need the heavy guns!"
  - pronto: "BMPT, ready."
- **O corpo:** o diesel de tanque; as esteiras; o servo; as cápsulas tilintando no casco.
- **A arma:** os 2 canhões de 30 mm: o "DUM-DUM-DUM" grave, tiros que se separam (não é rasgo de rotativo), alternado esquerda e
  direita no estéreo; a cadência do dano (0,3 s) é do jogo; o míssil: o "FUMP" da saída e o chiado longo; o estouro seco no tanque.
- **A morte:** o estouro, a munição pipocando, os mísseis chiando no chão.
- **A cara da Rússia:** o metal grave, o diesel com o turbo, o rádio de onda curta.

## 9. Ideias pra depois

- **O tiro pra cima no prédio:** quando a infantaria puder ocupar prédio, o BMPT é o único que atira no andar de cima a 39,5 m (os
  canhões elevam alto); os outros só acertam a parede.
- **Os lança-granadas:** os 2 lança-granadas automáticos do BMPT original: área pequena (2 m) contra soldado em trincheira e no
  entulho, que hoje ignora a cobertura.
- **A escolta:** o BMPT posto pra guardar um grupo de tanques (o comando Guard já existe) vira a torre sozinho pro soldado antitanque
  mais perto do tanque mais ferido.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#russia_bmpt` (feito no fim, com o modelo provisório como anexo de forma).
