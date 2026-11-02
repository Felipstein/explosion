# TOS-1A (Rússia, o lança-foguetes termobárico)

## 1. Ficha rápida

| | |
|---|---|
| Papel | a artilharia de área da Rússia: o fogo que tira o soldado da ruína e cala a defesa parada |
| Nome na tela | TOS-1A (a chave é `russia_tos1a`) |
| Custo e tempo | $1 200, 15 s |
| Vida e blindagem | 220, tanque |
| Velocidade | 6,2 m/s, giro do casco 180°/s, a mesa 60°/s |
| Visão | 39,5 m (atira a 70: precisa de quem veja por ele) |
| Arma | a salva de 6 foguetes: 35 de fogo cada, um a cada 0,25 s, 10 s pra recarregar a salva; de 15 a 70 m; cheio até 4 m, 40% na borda (8 m); o foguete a 50 m/s |
| Esmagador | nível 2 |
| Requer | Tank Plant + Research Institute |
| Estado | nos dados (`Units.json`) e no modelo provisório; fora do menu até os modelos da Rússia serem importados |

## 2. A ideia

**O bloco de canos.** O chassi do T-72 com a mesa giratória baixa e, em cima, a caixa blindada com os 24 tubos grossos (6 × 4),
deitada pra frente na marcha; pra atirar, o bloco levanta pela dobradiça de trás. Por cima do bloco, a gaiola de ripas com a rede da
cor do jogador. A silhueta que ninguém tem.

- **A palavra de cima:** o BLOCO DE CANOS (a caixa comprida e larga cobrindo o meio do tanque); de frente, a grade de 24 bocas
  escuras.
- **Inspiração real:** o TOS-1A "Solntsepyok" (o lançador BM-1 sobre o chassi do T-72, 24 tubos de 220 mm, foguetes termobáricos e
  alcance curto pra uma artilharia: a escola soviética do lança-chamas pesado).
- **A física, conferida:** o foguete termobárico espalha uma nuvem de combustível no ar e acende: a bola de fogo larga e uma onda de
  pressão longa, que entra em trincheira, em porta e em janela (é por isso que ele limpa posição). Queima rápido; não deixa poça de
  fogo (não é napalm). O jogo usa o dano de fogo: 150% no soldado.
- **Por que existe:** a Rússia segura o chão e esmaga de longe; o TOS-1A é o "esmaga" de área, e a resposta ao soldado escondido e à
  defesa parada.

## 3. Como funciona

- **No jogo:** o fogo faz **150% no soldado** (52,5 cheio, 21 na borda), **25% no tanque** (8,75), **100% no prédio** (35). Uma salva
  inteira no mesmo ponto: 315 no soldado do centro (mata qualquer soldado do jogo), 126 na borda (mata o Red Guard, o Rebel, o
  Worker e o antitanque; o Motostrelok, 140, e o Ranger, 180, sobram), 52,5 no tanque (nada), 210 no prédio.
- **O ritmo:** 6 foguetes em 1,25 s e 10 s de recarga: **~19/s de área no prédio**, ~28/s no soldado (o Inferno Cannon da China: 7,5/s
  e 11/s).
- **A área:** 4 m cheio e 8 m na borda, a maior do jogo (o Inferno 2,6/5,3, o Tomahawk 2/5, o Rocket Buggy 1,3/2,6).
- **O alcance:** 70 m, a artilharia de mais perto (o Inferno e o Rocket Buggy 79, o Tomahawk 85), mas ainda além das defesas de base
  (55–59 m); não atira a menos de 15 m. A 70 m o foguete leva ~1,4 s no ar: o alvo que anda sai do lugar.
- **Contra defesa:** o Pantsir fixo, a Gattling Cannon, o Stinger Site ou o Patriot (1 000) caem em 5 salvas (~46 s), sem responder.
- **Forte contra:** infantaria em grupo e parada, defesa de base, quem se esconde no entulho e na carcaça.
- **Fraco contra:** tudo que chega perto (220 de vida: o T-90M o mata em 4 tiros, 6 s; o RPG em ~5 s; a Technical em ~18 s), tanque
  (25%), o ar.
- **O contra dele:** a Technical e o Rocket Buggy (rápidos), o Apache, o Raptor. **Ele é o contra de:** o Stinger Site, a Gattling
  Cannon, o Patriot, o grupo de Rebels e RPGs na vila.

## 4. Em batalha

- **Temperamento: a tempestade.** Quieto e lento entre as salvas; quando cai, cai tudo de uma vez.
- **O ritmo:** 10 s de silêncio; o bloco sobe; a salva de 6 em pouco mais de 1 s; 1,4 s de voo; as 6 bolas de fogo juntas; o bloco
  desce 3 s depois.
- **O jogador bom** o deixa atrás dos tanques, com o Pantsir-S1 dando a visão (79 m) e o BMPT de guarda; mira na defesa e no grupo
  parado, no ponto onde a infantaria vai parar; nunca deixa o inimigo chegar a 15 m.
- **A IA** o trata como artilharia (tem alcance mínimo): quer 1 + as defesas inimigas que conhece, com 2 tanques de escolta por
  lançador.
- **Sozinho e ferido:** morre rápido; não tem como fugir de nada que o alcance.

## 5. A vida

- **No jogo:** a mesa gira 60°/s; o bloco levanta pela dobradiça de trás até 40° (a 40°/s) pra atirar e desce 3 s depois (a 25°/s),
  com a mola na parada (o bloco balança quando para: `launcher` no `Units.json`); os foguetes saem das bocas dos 24 tubos
  (`tubes_m`); a cratera A no chão; as esteiras de elos e a poeira; os estados de dano do veículo.
- **Ideia (parado):** o bloco deitado em marcha; a rede da gaiola mexe no vento; o diesel em marcha lenta.
- **Ideia (atirando):** o casco balança a cada foguete; o sopro de trás levanta a nuvem de poeira atrás dele (volume, acesa pelos
  motores dos foguetes); as tampas de trás dos tubos estouram e caem no chão.
- **Ideia (recarregando):** o bloco desce, e os tubos vazios ficam com a boca escura e fumegando (o jogador lê "vazio" de perto).

## 6. O visual

- **De cima:** o chassi de 7,2 × 3,9 m com a caixa larga do bloco cobrindo o meio, a gaiola de ripas com a rede vermelha por cima, a
  lona vermelha dobrada na frente do bloco, o rolo vermelho atrás; de frente, a grade de 24 bocas escuras; a estrela vermelha
  contornada pequena na lateral do bloco.
- **Assinatura:** o bloco de canos.
- **Cores:** o aço frio azulado com as manchas grandes pretas e cinza-claras; a cor do jogador na rede, na lona do bloco e no rolo do
  motor.
- **Nunca:** os tubos finos de foguete pequeno (é o Rocket Buggy da GLA); o tanque de combustível de lança-chamas à mostra; Z ou V;
  a faixa branca.

## 7. Tiro, projétil, impacto, destruição

- **A boca:** o clarão de cada tubo e o sopro de trás (a nuvem de poeira e fumaça que cobre o TOS por um instante: volume, acesa pelo
  fogo dos motores); o casco balança.
- **O projétil:** o foguete grosso em arco baixo, com a chama e o rastro de fumaça cinza (50 m/s nos dados; o foguete modelado em voo
  ainda não existe: ideia).
- **O impacto (ideia, a física do termobárico):** a nuvem clara curta (o combustível espalhado, um instante) e a bola de fogo laranja
  larga (8 m) que sobe e vira coluna de fumaça preta e marrom; a onda de pressão levanta a poeira em anel; as 6 juntas viram um tapete
  de fogo de ~1 s; tudo volumétrico, na luz do mundo, acendendo as unidades em volta;
  - no soldado: a onda o derruba (a morte em chamas);
  - no tanque: o fogo escorre pela chapa e apaga (25%);
  - no prédio: a onda estoura as janelas e o fogo entra; a fumaça sai pelas janelas;
  - no chão: a mancha queimada larga.
- **A morte (no jogo):** a morte de veículo; a carcaça fica 48 s queimando e afunda em 7 s; sucata ($180 em ~11 s) pro construtor.
- **A morte (ideia):** os foguetes que sobram estouram dentro do bloco (a caixa abre como uma flor de chapa) e a bola de fogo é maior
  que a de um tanque.

## 8. O som

- **Seleção (variantes):** o motor hidráulico do bloco (o "zuuum" longo subindo); o diesel V-12; o rádio de onda curta com as
  coordenadas lidas; o "clack" das travas do bloco.
- **As falas** (inglês, com o sotaque russo leve; o chefe do lançador, voz fria, baixa, contida):
  - selecionado: "TOS ready." / "Coordinates." / "Give me a grid." / "Salvo loaded." / "Mark the target."
  - andando: "Relocating." / "Moving to position." / "Changing position."
  - atacando: "Salvo!" / "Launching." / "Burn the position." / "Six away."
  - sob fogo: "We are exposed!" / "I cannot fight close!" / "Get them off me!"
  - pronto: "TOS-1A ready."
- **O corpo:** o diesel de tanque; o motor hidráulico do bloco; a mesa girando com o rangido.
- **A arma:** a salva de 6 em 1,25 s: o "VUSH" grave de cada foguete saindo, emendados, e o chiado indo embora; no impacto, o "WHOOMP"
  fundo e longo do termobárico (mais grave e mais comprido que uma explosão comum: a onda de pressão), e depois o ar voltando.
- **A morte:** o estouro, os foguetes estourando dentro do bloco, o fogo crepitando.
- **A cara da Rússia:** o fogo grave, o metal pesado, a voz baixa e seca.

## 9. Ideias pra depois

- **O drone observador** (estudo `docs/FACCAO_RUSSIA.md` §4.1, o Orlan): o TOS-1A atira sozinho em tudo que o drone vê até os 70 m
  dele; a guerra eletrônica inimiga derruba o drone. A dupla artilharia e observador, como na guerra real.
- **A salva inteira:** os 24 tubos de uma vez (4 salvas em ~5 s) e 40 s de recarga: o golpe que limpa uma vila; a escolha é do
  jogador.
- **O chão marcado:** a área da salva fica preta e o capim queimado pelo resto da partida (o campo que muda); o entulho atingido fica
  fumegando.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#russia_tos1a` (feito no fim, com o modelo provisório como anexo de forma).
