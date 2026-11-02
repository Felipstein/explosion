# Motostrelok (Rússia, o fuzileiro motorizado)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o fuzileiro da Rússia: pouco, firme, segura o chão na frente dos tanques |
| Nome na tela | Motostrelok (a chave é `russia_motostrelok`) |
| Custo e tempo | $200, 8 s: **um soldado por clique** |
| Vida e blindagem | **140**, soldado (entre o Red Guard e o Rebel, 120, e o Ranger, 180) |
| Velocidade | 6,6 m/s |
| Visão | 26,3 m |
| Arma | fuzil: 16 de arma leve a cada 1 s, alcance 26,3 m (só no chão) |
| Requer | Barracks |
| Estado | nos dados (`Units.json`) e no soldado provisório; fora do menu até os modelos da Rússia serem importados |

## 2. A ideia

**O capacete com a capa de camuflagem.** O fuzileiro motorizado russo de hoje: o fuzil AK-12, o capacete 6B47 com a capa, o colete
6B45. Não é a massa da China nem o soldado caro da USA: é o soldado profissional que anda com o blindado e segura o chão que o
blindado tomou.

- **A palavra de cima:** o CAPACETE redondo com a capa, e o colete da cor do jogador.
- **Inspiração real:** o motostrelok (a infantaria motorizada, a base do exército russo) com o equipamento de hoje (estudo
  `docs/FACCAO_RUSSIA.md` §5.3). O soldado de hoje, sério, sem caricatura.
- **Por que existe:** a Rússia precisa de quem tire o soldado antitanque inimigo de perto dos tanques. Um por clique, mais vida que o
  Red Guard, mais barato que o Ranger.

## 3. Como funciona

- **No jogo:** 16 a cada 1 s = **16/s em soldado**, 4/s em tanque (fuzil 25%), 8/s em prédio (50%). Mata um Red Guard ou um Rebel
  (120) em 8 tiros (7 s), um Ranger (180) em 12 (11 s), um soldado antitanque (100) em 7 (6 s).
- **O duelo:** ganha de cada fuzileiro no 1 contra 1 (o Red Guard leva ~9 s pra matá-lo; o Ranger e o Rebel, ~8 s).
- **A conta (Lanchester, por soldado):** 140 × 16 ÷ 200² = **0,056**; o Ranger 0,059, o Red Guard 0,080, o Rebel 0,089. Como o Ranger:
  ganha no 1 contra 1, perde pelo dinheiro pra massa barata (2 Red Guards custam $300; 1,5 Motostrelok).
- **A supressão (no jogo):** sob bala ele abaixa (o passo a 60%, a recarga a 135%) e, preso, rasteja (25%, 200%). Na carcaça ou no
  entulho, a bala faz metade.
- **Forte contra:** soldado (o RPG, o Javelin e o Tank Hunter que caçam o T-90M; o Rebel; o Worker).
- **Fraco contra:** tanque (passa por cima: esmagamento 200%), a Gattling, a Quad Cannon, a Technical, o Inferno (fogo 150%), o Toxin
  Tractor.
- **O contra dele:** as metralhadoras e o fogo. **Ele é o contra de:** o soldado antitanque inimigo.

## 4. Em batalha

- **Temperamento: o profissional.** Calado, firme, resistente. Não se exibe; aguenta.
- **O ritmo:** um tiro seco a cada 1 s; em grupo, o estalo irregular que não para.
- **O jogador bom** põe 4 a 6 na frente dos tanques, nas ruínas e nas carcaças, com 1 BMPT atrás; usa-os pra pegar o RPG antes de ele
  atirar.
- **A IA** faz dele o 1º da Kazarma (vale por 2 na mistura).
- **Sozinho:** segue lutando; recua só preso pela supressão.

## 5. A vida

- **No jogo:** o corpo humano da Unreal (o do Ranger, com as animações) vestido com as peças do soldado da China na farda
  cinza-esverdeada (o provisório: a farda própria da Rússia é da sessão da infantaria, `docs/elenco/TAREFAS.md` 36); o fuzil do
  Ranger na mão (o AK-12 é ideia); as pernas andam com o chão; o traçante (2,6 m, metade dos tiros) e o clarão pequeno (0,15 m);
  abaixado sob fogo e rastejando preso; as 6 mortes animadas; achatado se esmagado.
- **Ideia (parado):** apoiado num joelho, o fuzil baixo; de vez em quando confere o carregador.
- **Ideia (andando):** em fila atrás do blindado, aproveitando a sombra do casco (o motorizado anda com o veículo).
- **Ideia (atirando):** a rajada curta de 2 tiros (o modo do AK-12 de verdade), no mesmo ritmo do dano do jogo.

## 6. O visual

- **De cima:** o soldado pequeno com o capacete redondo e o colete na cor do jogador.
- **Assinatura:** o capacete com a capa de camuflagem (a silhueta redonda e "felpuda" que separa do capacete liso dos outros).
- **Cores:** a farda cinza-esverdeada; o colete e o capacete na cor do jogador.
- **Nunca:** o gorro de pele com orelhas (caricatura); a fita branca no braço ou na perna (o símbolo da guerra real); Z ou V;
  bandeira (é da GLA).

## 7. Tiro, projétil, impacto, destruição

- **A boca:** o clarão pequeno e seco; o fio de fumaça.
- **O projétil:** o traçante (2,6 m de risco, metade dos tiros), rápido.
- **O impacto:** no soldado, o tranco e o ferido; no veículo, faísca; no prédio, o pó do reboco; no chão, a poeira pequena.
- **A morte (no jogo):** uma das 6 mortes animadas, o fuzil cai, o corpo fica 8 s; esmagado, achatado.

## 8. O som

- **Seleção (variantes):** o ferrolho do AK puxado (o "clack-clack" seco e metálico); as botas na areia; o rádio de ombro chiando
  curto; o equipamento batendo.
- **As falas** (inglês, com o sotaque russo leve; o soldado profissional, voz baixa, firme, curta):
  - selecionado: "Motostrelok." / "Ready." / "Standing by." / "Infantry here." / "Orders?"
  - andando: "Moving." / "On my way." / "Following the armor." / "Advancing on foot."
  - atacando: "Open fire!" / "Contact!" / "Suppressing!" / "Take them down."
  - sob fogo: "Under fire!" / "Get down!" / "Pinned!" / "Find cover!"
  - pronto: "Rifleman, reporting."
- **O corpo:** o passo pesado (mais equipamento que o Red Guard); o colete rangendo; a respiração no correr.
- **A arma:** o estalo do AK-12 (o 5,45 mm: mais "tac" seco que "bang"), em pares curtos; o eco nos prédios.
- **A morte:** o grito curto; o fuzil caindo.
- **A cara da Rússia:** a voz baixa e seca, o rádio de onda curta.

## 9. Ideias pra depois

- **O BTR (o "motorizado" de verdade):** o blindado de 8 rodas que leva 8 Motostreloks e eles atiram das escotilhas (o transporte da
  simulação 35, que o Stryker já usa); sem o BTR, o Motostrelok é meio soldado.
- **A trincheira:** dentro da trincheira do IMR-3M, a bala faz um terço e a supressão não prende; o Motostrelok é o soldado que
  defende o chão cavado.
- **O lança-granadas sob o cano:** uma granada a cada 10 s, área de 2 m, contra o soldado escondido no entulho e na carcaça (que hoje
  só leva metade da bala).

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#russia_motostrelok` (feito no fim, com o modelo provisório como anexo de forma).
