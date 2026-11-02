# Tank Hunter (China, o caçador de blindado)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o antitanque e antiaéreo a pé da China |
| Custo e tempo | $300, 5 s |
| Vida e blindagem | 100, soldado |
| Velocidade | 5,3 m/s |
| Visão | 26,3 m (atira a 46: precisa de quem veja por ele) |
| Arma | foguete: 40 perfurante a cada 1 s, alcance 46 m, no chão **e no ar**; sai a 30 m/s e acelera até 70, corrige a rota (160°/s) |
| Requer | Barracks |

## 2. A ideia

**O tubo no ombro.** O soldado que fica deitado na areia esperando o tanque passar. Leve, barato e mortal pra tudo que tem motor.

- **A palavra de cima:** o TUBO comprido no ombro (mais largo que o fuzil), apontado pra frente.
- **Inspiração real:** o PF-98 (o lançador antitanque de 120 mm do ELP, de tubo reutilizável). Fictício no jogo: o foguete dele
  corrige a rota como o do ZH; o PF-98 de verdade não é um míssil guiado.
- **Por que existe:** a infantaria da China precisa de uma resposta barata ao tanque e ao helicóptero; a horda não pode depender só
  dos tanques.

## 3. Como funciona

- **No jogo:** 40 a cada 1 s = **40/s em tanque, prédio e aeronave** (perfurante 100%; o Chinook 50%), **4/s em soldado** (10%). 3
  Tank Hunters ($900) matam um tanque USA ($900) em ~4 s; 2 derrubam um Apache (220) em 3 salvas (~2 s), um Raptor (160) em 2.
- **O foguete:** sai a 30 m/s, acelera (160 m/s²) até 70 m/s, curva 160°/s, vive 2,5 s; o espalhado de 2° e o boost de 0,1 s.
- **A conta:** o par antitanque das 3 facções é o mesmo (o RPG e o Javelin têm os mesmos números): o equilíbrio é exato; muda o
  jeito, o som e o visual.
- **Forte contra:** tanque (o Overlord cai pra 5 Tank Hunters em ~5 s), veículo leve, helicóptero, avião baixo.
- **Fraco contra:** soldado (4/s: um Red Guard o mata em ~6 s), a Gattling, a Quad Cannon, a Technical, o Toxin Tractor, ser
  esmagado.
- **O contra dele:** a infantaria e as metralhadoras. **Ele é o contra de:** o tanque USA, o Paladin, o Scorpion, a Technical, o
  Apache.

## 4. Em batalha

- **Temperamento: o caçador paciente.** Calado, frio, concentrado. Espera, mira, dispara e já está carregando.
- **O ritmo:** o "thump" do disparo, o chiado do foguete, o estouro no alvo; 1 s; de novo.
- **O jogador bom** os põe em 3 ou 4 atrás dos Red Guards (que os protegem da infantaria), nas ruínas e nas carcaças da vila,
  esperando o tanque entrar no alcance; nunca na frente de uma Technical.
- **A IA** mistura pelo que falta no quartel.
- **Sozinho:** morre fácil; a supressão o prende rápido (a bala o faz abaixar e recarregar a 135%).

## 5. A vida

- **No jogo:** o corpo humano com o lançador PF-98 (o modelo `SM_Launcher_PF98`), o colete e o capacete na cor da facção; o foguete
  modelado em voo com o rastro (`missile_model`, 1 m); a supressão e as mortes animadas.
- **Ideia (parado):** apoiado num joelho, o tubo no ombro, o olho na mira; de vez em quando confere o foguete de reserva nas costas.
- **Ideia (atirando):** ajoelha antes do primeiro tiro; o sopro de trás levanta a areia atrás dele (o perigo do sopro é real nos
  lançadores sem recuo: ninguém fica atrás); entre tiros, encaixa o próximo foguete no tubo.

## 6. O visual

- **De cima:** o soldado com o tubo comprido saindo dos dois lados do ombro; o colete vermelho.
- **Assinatura:** o tubo grosso no ombro.
- **Cores:** a farda oliva escura, o colete e o capacete na cor do jogador, o tubo oliva.
- **Nunca:** bandeira; mochila de foguetes de caricatura (3 foguetes nas costas no máximo).

## 7. Tiro, projétil, impacto, destruição

- **A boca:** o clarão na frente e o **sopro atrás** (o cone de fumaça e poeira saindo pra trás do tubo: volume, iluminado pelo
  clarão); o tubo sobe um pouco no disparo.
- **O projétil:** o foguete de 1 m com a chama na cauda e o rastro de fumaça branca-cinza fino (`trail_width` 0,2), que fica no ar
  ~1 s; acelera visivelmente depois de sair.
- **O impacto:**
  - no tanque: o estouro de carga oca: o clarão curto, o jato de faísca na chapa, a fumaça preta, o furo escuro;
  - no ar: o estouro e o helicóptero inclinando, a fumaça saindo;
  - no prédio: o buraco na parede e o pó;
  - no soldado: pouco (10%).
- **A morte:** a morte animada; o tubo cai.

## 8. O som

- **Seleção (variantes):** o tubo batendo no capacete; o "clack" do foguete encaixando; o rádio de ombro; a respiração curta.
- **As falas** (inglês; o caçador, voz baixa, calma, concentrada, quase um sussurro antes do disparo):
  - selecionado: "Tank Hunter." / "I hunt steel." / "Point me at the armor." / "Ready to strike." / "Quiet. I'm aiming."
  - andando: "Moving low." / "Stalking." / "Finding a firing spot." / "Staying in cover."
  - atacando: "Rocket away!" / "Armor in sight." / "Back blast clear!" / "Hit it again."
  - sob fogo: "Infantry on me!" / "I need cover!" / "Too close, too close!"
  - pronto: "Tank Hunter, ready."
- **O corpo:** o passo leve, o equipamento batendo, o foguete de reserva chacoalhando.
- **A arma:** o "THUMP" grave do disparo com o "fsssh" do sopro, o chiado do motor do foguete acelerando e passando, o estouro seco
  e metálico no tanque.
- **A morte:** o grito; o tubo caindo no chão com o som oco.

## 9. Ideias pra depois

- **A emboscada:** parado e deitado por 5 s, ele some da vista do inimigo até atirar (camuflado na areia); o primeiro foguete da
  emboscada faz 50% a mais.
- **A dupla de caça:** o Tank Hunter ao lado de um par de Red Guards tem o carregamento 20% mais rápido (o municiador do lado), a
  equipe de antitanque real (atirador e municiador).
- **O foguete termobárico:** upgrade no Propaganda Center: o foguete faz 30 em área (2 m) em soldado e em prédio ocupado (troca o
  antitanque por anti-casa).

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#china_tank_hunter` (feito no fim, com o modelo provisório como anexo de forma).
