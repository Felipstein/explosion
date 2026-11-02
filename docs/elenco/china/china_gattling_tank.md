# Gattling Tank (China, o guarda-chuva da horda)

## 1. Ficha rápida

| | |
|---|---|
| Papel | anti-infantaria e antiaéreo que anda com os tanques |
| Custo e tempo | $800, 10 s |
| Vida e blindagem | 300, tanque |
| Velocidade | 10,5 m/s (mais rápido que o Battlemaster), giro do casco e da torre 180°/s |
| Visão | 52,7 m |
| Arma (chão) | 2 tambores rotativos: gattling 15 a cada 0,4 s, alcance 39,5 m |
| Arma (ar) | gattling 12 a cada 0,4 s, alcance 92,2 m |
| Esmagador | nível 2 |
| Requer | War Factory |

## 2. A ideia

**Os dois tambores de 6 canos.** Um chassi mais leve que o tanque, a torre-caixa larga com um feixe de canos de cada lado e o prato
do radar de busca girando no alto. Onde ele está, soldado não chega e helicóptero não fica.

- **A palavra de cima:** os DOIS FEIXES DE CANOS saindo dos lados da torre + o prato do radar girando.
- **Inspiração real:** o desenho dos antiaéreos de lagarta do ELP (o PGZ-95 e o PGZ-09: os canhões nos lados da torre, o radar de
  busca atrás, o radar de tiro na frente). **Os de verdade usam canhões automáticos de cano fixo;** o tambor rotativo é a escapada
  do jogo, emprestada dos canhões rotativos navais chineses (Type 730).
- **Por que existe:** o Battlemaster não atira pra cima e quase não fere soldado. O Gattling Tank tapa os dois buracos da horda.

## 3. Como funciona

- **No jogo, no chão:** 37,5/s em soldado (gattling 100%), **3,75/s em tanque e prédio** (10%). Mata um Red Guard ou um Rebel
  (120) em ~3 s, um Ranger (180) em ~4,5 s. Contra um tanque USA: 128 s (não é caça-tanque, de propósito).
- **No jogo, no ar:** 30/s (avião e helicóptero 100%; o Chinook 50%). O Raptor (160) cai em ~5 s, o Apache (220) em ~6 s, o Chinook
  (300) em ~8 s. Alcança 92 m pra cima: mais que tudo que vem do chão.
- **Forte contra:** infantaria (inclusive o RPG e o Javelin que caçam os tanques), helicóptero, avião.
- **Fraco contra:** tanque (um tanque o mata em 8 s), artilharia, o Rocket Buggy de longe; até a Technical (180, blindagem de
  tanque) leva ~48 s pra cair: ele espanta veículo leve, não mata.
- **O contra dele:** qualquer tanque. **Ele é o contra de:** o grupo de RPG/Javelin escondido, o Apache, o Raptor, a infantaria em
  massa da GLA.

## 4. Em batalha

- **Temperamento: o vigia nervoso.** Ligeiro, desconfiado, sempre olhando pra cima. Atira antes de perguntar.
- **O ritmo:** o giro dos canos e o rasgo contínuo; quando o ar chega, o radar trava e os canos sobem.
- **O jogador bom** põe 1 Gattling Tank pra cada 3 ou 4 tanques, no meio do bolo; nunca na frente (ele é casca fina).
- **A IA** o põe na mistura da fábrica pelo que falta (como armado comum: ele atira no chão também, então a IA não o trata como
  antiaéreo puro).
- **Sozinho e ferido:** foge mal (10,5 m/s ajuda), mas tende a ficar atirando; a fumaça e a chama nos limiares de sempre.

## 5. A vida

- **No jogo:** o prato do radar de busca gira em cima da torre (peça `_Radar`, 30 rpm, `on_turret`); a torre gira 180°/s; o
  traçante e o clarão pequeno (`light_gun`); as esteiras e a poeira; os estados de dano.
- **Ideia (parado):** o radar gira sem parar; os canos ficam levantados a ~20° (a pose de vigia, lê "antiaéreo"); a luz pequena
  verde no radar de tiro.
- **Ideia (alvo no ar):** o radar de busca **para** apontado pro alvo e o radar de tiro da frente segue; os canos sobem; o jogador
  vê o tanque "olhando" pro céu antes do tiro.
- **Ideia (atirando):** os dois tambores giram (hoje o giro do cano não está nos dados); a chuva de cápsulas caindo dos lados da
  torre; a fumaça fina se acumulando em volta.
- **Ideia (ferido):** o radar gira torto, aos trancos.

## 6. O visual

- **De cima:** o chassi de 7 × 3,4 m, a torre-caixa larga atrás do meio, os dois feixes de canos saindo dos lados, o prato do radar
  atrás, a gaiola com a lona.
- **Assinatura:** os dois tambores (nenhum outro veículo tem cano rotativo dos dois lados).
- **Cores:** o oliva escuro com a camuflagem digital; o vermelho do jogador nas lonas dobradas (gaiola e deck), no anel da boca de
  cada tambor e nas estrelas das bochechas.
- **Nunca:** cano duplo de canhão (é do Overlord); faixa pintada.

## 7. Tiro, projétil, impacto, destruição

- **A boca:** dois clarões contínuos, um de cada lado, em estrela; a fumaça cinza fina saindo dos tambores.
- **O projétil:** o traçante laranja a cada 3 a 4 tiros; no ar, as duas linhas subindo e convergindo no alvo.
- **O impacto:**
  - no soldado: a linha de poeira andando no chão até ele;
  - no tanque: a cascata de faíscas, o "ping-ping", quase nada;
  - no ar: a faísca no casco e a fumaça do motor; o helicóptero balança;
  - no prédio: lascas pequenas.
- **A morte:** a morte de veículo (o fogo e a carcaça 48 s, a sucata: $120 em ~8 s); ideia: a munição dos dois tambores cozinha em
  estalos e o prato do radar voa pro lado.

## 8. O som

- **Seleção (variantes):** o motor elétrico dos tambores dando um giro curto no vazio; o bip do radar travando (3 bips rápidos); o
  diesel leve acelerando; o servo da torre.
- **As falas** (inglês; o artilheiro antiaéreo, jovem, ligeiro, com os olhos no céu):
  - selecionado: "Gattling tank." / "Eyes on the sky." / "Barrels ready." / "Nothing flies over us." / "Radar sweeping."
  - andando: "Moving." / "Covering the column." / "Keeping up." / "On it."
  - atacando: "Shred them!" / "Air target, engaging!" / "Spinning up!" / "Clear the sky."
  - sob fogo: "We're thin-skinned here!" / "Armor's on us, need cover!" / "Taking fire!"
  - pronto: "Gattling tank, ready."
- **O corpo:** o diesel mais leve e mais agudo que o do Battlemaster; as esteiras mais rápidas; o radar girando com o zumbido
  elétrico e o bip a cada volta (30 rpm = a cada 2 s).
- **A arma:** o "uiiin" de 0,3 s e o rasgo contínuo (BRRRRT) de cada tambor, um pouco defasados (o som estéreo dos dois lados); as
  cápsulas tilintando no casco. A cadência do dano (0,4 s) é do jogo; o som é contínuo.
- **A morte:** o estouro, a munição pipocando, o metal do prato do radar caindo.

## 9. Ideias pra depois

- **O radar da horda:** com o radar girando, o Gattling Tank mostra no minimapa as aeronaves a até 120 m (a horda ganha olhos).
- **A barreira de fogo:** uma habilidade de 5 s: os canos apontam pro alto e fazem uma cortina; todo míssil que passa por cima dele
  a até 15 m tem 50% de chance de ser derrubado (resposta ao Tomahawk, ao Rocket Buggy). Recarga de 40 s.
- **Os canos quentes:** depois de 8 s de fogo, os canos brilham vermelho e a cadência cai; à noite, o brilho mostra onde ele está
  (o preço da força).

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#china_gattling_tank` (feito no fim, com o modelo provisório como anexo de forma).
