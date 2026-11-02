# Kornet Team (Rússia, o antitanque de tripé)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o antitanque a pé de longe: espera o blindado parado e o acerta de onde ninguém responde |
| Nome na tela | Kornet Team (a chave é `russia_kornet`) |
| Custo e tempo | $300, 6 s (hoje, **um soldado** por clique) |
| Vida e blindagem | 100, soldado |
| Velocidade | **5,0 m/s** (o soldado mais lento do jogo) |
| Visão | 26,3 m (atira a 55: precisa de quem veja por ele) |
| Arma | míssil: 50 perfurante a cada 2 s, alcance **55 m** (o tiro mais longe da infantaria; os outros antitanques: 46 m), **só no chão**; sai a 30 m/s, acelera até 70, corrige a rota (160°/s) |
| Requer | Barracks |
| Estado | nos dados (`Units.json`) e no soldado provisório; fora do menu até os modelos da Rússia serem importados |

## 2. A ideia

**O tubo no tripé.** A equipe de dois com o lançador baixo no tripé: um aponta, o outro carrega. Lenta pra montar e pra andar, mas o
tiro mais longe que um soldado dá no jogo. Hoje, no jogo, ela é um soldado só, com o lançador do Javelin no ombro (o provisório).

- **A palavra de cima:** o TRIPÉ com o tubo comprido e os dois soldados deitados do lado (ideia; hoje, o tubo no ombro).
- **Inspiração real:** o 9M133 Kornet (o míssil antitanque russo guiado por feixe de laser, lançado do tripé 9P163 por uma equipe de
  dois, com carga dupla em tandem; de verdade alcança 5,5 km).
- **Por que existe:** a Rússia segura o chão: o antitanque dela não corre atrás do tanque, espera ele de longe.

## 3. Como funciona

- **No jogo:** 70 a cada 2 s = **35/s em tanque e prédio** (perfurante 100%), 3,5/s em soldado (10%). 3 Kornets ($900) matam um
  tanque USA (480) em ~4 s (7 mísseis), um Battlemaster em ~4 s, um Paladin em ~6 s, um Overlord (1 100) em ~10 s.
- **O alcance:** 55 m passa de todos os canhões de tanque (39,5 a 52 m) e dos outros antitanques a pé (46 m); só as defesas de base
  (55–59 m) e a artilharia empatam ou passam. Mas ele vê só 26 m: sozinho, não usa o alcance.
- **A conta:** 100 × 35 ÷ 300² = **0,039**, 12% abaixo do Tank Hunter, do RPG e do Javelin (0,044), dentro dos ±15%. A diferença
  paga o alcance (o atirador que nunca é alcançado não morre) e o tiro mais pesado (70 contra 40).
- **Não atira no ar** (o Tank Hunter, o RPG e o Javelin atiram).
- **Forte contra:** tanque, veículo, prédio, de longe e parado.
- **Fraco contra:** soldado (3,5/s: um Red Guard o mata em ~6 s), as metralhadoras, o tanque que fecha a distância (o tanque USA anda
  os 15 m de diferença em ~2 s), ser esmagado, o ar, a artilharia; foge mal (5,0 m/s).
- **O contra dele:** a infantaria, as metralhadoras, o Apache, o Rocket Buggy. **Ele é o contra de:** o tanque que para no alcance, o
  Paladin, o Overlord lento.

## 4. Em batalha

- **Temperamento: o vigia da colina.** Paciente, metódico, imóvel. Não persegue: espera, e quando o alvo para, não erra.
- **O ritmo:** o "fuuum" a cada 2 s, o rastro longo e reto, o estouro seco e duplo; o silêncio de quem já está mirando o próximo.
- **O jogador bom** põe 2 ou 3 atrás dos Motostreloks, com o Pantsir-S1 ou o BMPT dando a visão (79 e 52,7 m), nas ruínas e atrás das
  carcaças; nunca na frente e nunca onde a Technical chega.
- **A IA** mistura pelo que falta no quartel (o Kornet conta por 1).
- **Sozinho:** morre fácil e não foge; a supressão o prende rápido (a bala o faz abaixar e recarregar a 135%).

## 5. A vida

- **No jogo:** o soldado provisório (o corpo do Ranger com as peças do soldado da China na farda cinza-esverdeada) com o **lançador do
  Javelin no ombro** (`SM_Launcher_Javelin`: o tripé do Kornet ainda não existe); o míssil modelado em voo com o rastro
  (`missile_model`, 1 m); a ogiva de 1,2 kg no desenho do impacto (`warhead_kg`; o Javelin e o Tank Hunter têm 1,0); a supressão e as
  mortes animadas.
- **Ideia (montando):** ao parar, os dois abrem o tripé e encaixam o tubo (2 s); andando, o tubo vai nas costas do municiador.
- **Ideia (atirando):** o apontador com o olho na mira, imóvel, até o impacto (o míssil de feixe precisa da mira no alvo o voo
  inteiro); o municiador encaixa o próximo tubo.
- **Ideia (sob fogo):** os dois deitam do lado do tripé; se for preciso sair, desmontam antes (2 s).

## 6. O visual

- **De cima:** hoje, o soldado com o tubo comprido no ombro e o colete na cor do jogador; ideia: os dois deitados com o tripé baixo
  entre eles e os tubos de reserva em volta.
- **Assinatura:** o tripé (ideia).
- **Cores:** a farda cinza-esverdeada; o colete e o capacete na cor do jogador; o tubo verde.
- **Nunca:** o tubo no ombro na versão final (é do Javelin); o gorro de pele de caricatura; a fita branca; Z ou V.

## 7. Tiro, projétil, impacto, destruição

- **A boca:** o "pop" de ejeção e a nuvem branca curta na frente do tubo; o motor acende a 1 m; o sopro atrás levanta a areia (o
  perigo do sopro é real: ninguém fica atrás).
- **O projétil:** o míssil de 1 m com a chama na cauda e o rastro fino que fica no ar ~1 s; voa reto e "liso", sem as curvas bruscas do
  RPG (o feixe de laser o segura na linha).
- **O impacto:**
  - no tanque: a carga dupla (ideia: dois clarões colados, o primeiro tira o tijolo de reativa, o segundo fura), o jato de faísca, o
    furo escuro, a fumaça preta;
  - no prédio: o buraco na parede e o pó;
  - no soldado: pouco (10%).
- **A morte (no jogo):** a morte animada; o tubo cai.

## 8. O som

- **Seleção (variantes):** as pernas do tripé abrindo (o "clack" triplo); o tubo encaixando; o bip da mira; o rádio de ombro.
- **As falas** (inglês, com o sotaque russo leve; a equipe de dois: o apontador calmo, o municiador respondendo):
  - selecionado: "Kornet team." — "Ready." / "Tripod set." / "Long range is our job." / "Give us armor." / "Quiet. Aiming."
  - andando: "Moving the tripod." / "Relocating." / "Slow going."
  - atacando: "Missile away!" / "Guiding..." — "On target." / "Reload!" — "Loaded!" / "Hit."
  - sob fogo: "Infantry on us!" / "Pack it up, move!" / "Too close!"
  - pronto: "Kornet team, ready."
- **O corpo:** o passo pesado (o tubo e o tripé nas costas), o equipamento batendo.
- **A arma:** o "pop" da ejeção, um instante de silêncio, o "SHHHHH" longo e uniforme do motor (o míssil voa mais tempo que o do RPG) e
  o estouro seco e duplo no tanque.
- **A morte:** o grito; o tubo caindo no chão com o som oco.
- **A cara da Rússia:** a voz baixa e seca, a paciência, o rádio de onda curta.

## 9. Ideias pra depois

- **A dupla e o tripé** (estudo `docs/FACCAO_RUSSIA.md` §5.3): sai em par (`produce_count` 2, como o Red Guard: um aponta, o outro
  carrega) e precisa de 2 s parado pra montar antes do 1º tiro; andando, não atira. Com isso a conta por dólar volta pro par dos
  outros antitanques.
- **O posto Kornet** (estudo §5.1): o bunker de sacos e concreto com a seteira e o tubo no reparo: a defesa antitanque da base.
- **O míssil termobárico** (a versão de verdade existe): trocar o antitanque por 30 em área (2 m) contra o soldado no prédio e no
  entulho.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#russia_kornet` (feito no fim, com o modelo provisório como anexo de forma).
