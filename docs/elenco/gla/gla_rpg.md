# RPG Trooper (GLA, o lança-rojão)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o antitanque e antiaéreo a pé da GLA |
| Nome na tela | RPG Trooper (a chave é `gla_rpg`) |
| Custo e tempo | $300, 5 s |
| Vida e blindagem | 100, soldado |
| Velocidade | 5,3 m/s |
| Visão | 26,3 m (atira a 46: precisa do Rebel pra ver) |
| Arma | rojão: 40 perfurante a cada 1 s, alcance 46 m, no chão **e no ar**; sai a 30 m/s e acelera até 70, corrige a rota (160°/s) |
| Requer | Barracks |

## 2. A ideia

**O tubo com a ogiva na ponta.** O símbolo da guerrilha do mundo inteiro: o lançador com a ogiva em cone saindo da boca, apoiado no
ombro, o homem agachado na esquina da ruína. Barato, simples, e o tanque mais caro do mapa tem medo dele.

- **A palavra de cima:** o TUBO com a OGIVA em cone na ponta (a silhueta mais conhecida de qualquer arma antitanque).
- **Inspiração real:** o RPG-7 soviético: o tubo reutilizável, a granada com a ogiva de carga oca maior que o tubo, a carga de
  lançamento e o motor do foguete que acende alguns metros depois de sair, o sopro pra trás. Fictício no jogo: o rojão dele corrige
  a rota e atira no ar como no ZH; o RPG-7 de verdade não é guiado.
- **Por que existe:** a GLA não tem o canhão pesado. O RPG é a resposta dela ao tanque e ao helicóptero, e o melhor amigo da emboscada.

## 3. Como funciona

- **No jogo:** 40 a cada 1 s = **40/s em tanque, prédio e aeronave** (perfurante 100%; o Chinook 50%), **4/s em soldado** (10%). 3 RPGs
  ($900) matam um tanque USA ($900) em ~4 s; 5 RPGs, um Overlord em ~5 s; 2 derrubam um Apache em 3 salvas.
- **O mesmo número do Tank Hunter e do Javelin** (o par antitanque das 3 facções é o mesmo, de propósito): a diferença é o jeito.
  O da GLA vem em bando com os Rebels, escondido na vila.
- **Forte contra:** tanque, veículo, helicóptero, avião baixo, prédio (40/s, como um canhão).
- **Fraco contra:** soldado (4/s: um Red Guard o mata em ~6 s), a metralhadora, o fogo, o veneno, ser esmagado.
- **O contra dele:** a infantaria, a Gattling, o Gattling Tank, o Inferno. **Ele é o contra de:** o Battlemaster, o Overlord, o tanque
  USA, o Paladin, o Apache.

## 4. Em batalha

- **Temperamento: o caçador de tanque.** Ousado, frio no disparo, eufórico no acerto. Espera o tanque passar e atira na traseira.
- **O ritmo:** o disparo seco, o chiado, o estouro; o grito; 1 s; de novo.
- **O jogador bom** mistura 1 RPG pra cada 2 Rebels nas ruínas (os Rebels veem a 39,5 m e o RPG atira a 46 m: o Rebel é o olho do
  RPG), deixa o tanque entrar na rua e dispara de dois lados.
- **A IA** mistura pelo que falta no quartel.
- **Sozinho:** morre fácil pra qualquer soldado; a supressão o prende.

## 5. A vida

- **No jogo:** o corpo humano com o lançador RPG-7 (o modelo `SM_Launcher_RPG7`), a roupa areia e o colete na cor da facção; o rojão
  modelado em voo com o rastro (`missile_model`); a supressão; as 6 mortes animadas.
- **Ideia (parado):** agachado, o tubo no ombro, a sacola de rojões nas costas (3 ogivas aparecendo); de vez em quando encaixa o
  próximo rojão.
- **Ideia (atirando):** ajoelha; o sopro levanta uma nuvem de areia atrás dele (o perigo real do sopro: ninguém fica atrás); entre
  tiros, tira a ogiva da sacola e encaixa no tubo.

## 6. O visual

- **De cima:** o soldado de roupa areia com o tubo comprido e o cone da ogiva na frente.
- **Assinatura:** a ogiva em cone na ponta do tubo.
- **Cores:** a roupa areia, o tubo de madeira e aço escuro, a ogiva verde-oliva; o verde do jogador no colete e no lenço.
- **Nunca:** míssil moderno com mira eletrônica (é o tubo velho); bandeira de grupo real.

## 7. Tiro, projétil, impacto, destruição

- **A boca:** o clarão curto e o **sopro atrás** (o cone de fumaça e areia, volume, aceso pelo clarão); o rojão sai devagar e o motor
  acende uns metros depois (a física do RPG-7: a carga empurra, o foguete acende longe do atirador).
- **O projétil:** o rojão com a chama na cauda e o rastro de fumaça cinza-branca torto (o RPG de verdade balança no vento lateral; no
  jogo, a curva da correção), a 70 m/s.
- **O impacto:**
  - no tanque: o estouro de carga oca (o clarão curto, o jato de faísca, a fumaça preta, o furo);
  - no ar: o estouro e o helicóptero inclinando;
  - no prédio: o buraco e o pó;
  - no chão (o erro): o estouro e a areia.
- **A morte:** a morte animada; o tubo cai.

## 8. O som

- **Seleção (variantes):** o rojão encaixando no tubo (o "clunk" de madeira e metal); a sacola de ogivas batendo; o grito curto; o
  rádio.
- **As falas** (inglês; voz rouca, ousada, rindo no acerto):
  - selecionado: "RPG ready." / "Show me a tank." / "One rocket, one tank." / "I'm waiting for armor." / "Loaded."
  - andando: "Moving." / "Finding a corner." / "To the ruins." / "Staying low."
  - atacando: "Rocket!" / "Back blast!" / "Hit it in the back!" / "Burn, tank, burn!"
  - sob fogo: "Too many soldiers!" / "I need cover!" / "Pull back!"
  - pronto: "RPG, ready to hunt."
- **O corpo:** a corrida curta; as ogivas batendo na sacola.
- **A arma:** o "WHOOMP" do disparo (o sopro), meio segundo depois o chiado do motor acendendo e indo embora; o estouro seco e metálico
  no tanque.
- **A morte:** o grito; o tubo caindo oco.

## 9. Ideias pra depois

- **O tiro na traseira:** o rojão que acerta o tanque por trás faz 150% (o motor e a blindagem fina): o RPG premia a emboscada, e a
  física é real (a traseira é o ponto fraco de todo tanque).
- **A ogiva de sucata:** o Worker que desmancha uma carcaça de tanque perto do quartel dá 3 ogivas "pesadas" pros RPGs próximos (+50%
  no próximo tiro).
- **O RPG no teto:** o RPG dentro de uma ruína de prédio atira de cima (+15% de alcance e o tanque não consegue baixar o cano pra
  ele).

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#gla_rpg` (feito no fim, com o modelo provisório como anexo de forma).
