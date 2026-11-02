# Inferno Cannon (China, o obuseiro de fogo)

## 1. Ficha rápida

| | |
|---|---|
| Papel | a artilharia da China: fogo em área de longe, contra soldado e defesa parada |
| Custo e tempo | $900, 15 s |
| Vida e blindagem | **120**, tanque (o veículo mais frágil da China) |
| Velocidade | 7,9 m/s, giro do casco 180°/s, a torre 90°/s |
| Visão | 39,5 m (atira a 79: precisa de quem veja por ele) |
| Arma | granada incendiária: 30 de fogo a cada 4 s, de 13,2 a 79 m; cheio até 2,6 m, 40% na borda (5,3 m); a granada a 45 m/s em arco |
| Esmagador | nível 2 |
| Requer | War Factory + Propaganda Center |

## 2. A ideia

**O cano longo.** O obuseiro autopropulsado de lagarta: a torre-caixa grande e alta atrás do meio, o cano comprido (7 m do munhão
à boca) com o freio de boca, a pá de apoio levantada atrás. Ele não vê a batalha; ele a incendeia de longe.

- **A palavra de cima:** o CANO LONGO passando por cima do nariz.
- **Inspiração real:** o PLZ-05 (o obuseiro autopropulsado de 155 mm do ELP, de cano longo, com o motor na frente e a torre
  grande atrás). A granada incendiária é a escapada do jogo (`docs/ELENCO.md` §4.2); o PLZ-05 não é "lança-chamas".
- **Por que existe:** a horda da China empaca na defesa parada e no soldado entrincheirado. O Inferno queima os dois sem chegar
  perto.

## 3. Como funciona

- **No jogo:** o fogo faz **150% no soldado** (45 cheio, 18 na borda), **25% no tanque** (7,5), **100% no prédio** (30). Mata um Red
  Guard ou um Rebel em 3 granadas (~8 s), um Ranger em 4 (~12 s), e pega todo soldado a até 5,3 m do ponto.
- **Contra defesa:** só 7,5/s no prédio (a Gattling Cannon ou o Stinger Site de 1 000 levam ~2 min de um Inferno), mas de 79 m: a
  defesa (59 m) nunca responde. 3 Infernos tiram um Stinger Site em ~45 s, de graça.
- **O voo:** a 79 m a granada leva ~2 s no ar: o alvo que anda sai do lugar. Não atira a menos de 13,2 m.
- **Forte contra:** infantaria em grupo e parada, defesa de base, quem está escondido no entulho e na carcaça.
- **Fraco contra:** tudo que chega perto (120 de vida: uma Technical o mata em ~10 s, um RPG em 3 foguetes), tanque (25%), o ar.
- **O contra dele:** a Technical e o Rocket Buggy (rápidos), o Raptor, o Apache. **Ele é o contra de:** o Stinger Site, a Gattling
  Cannon, o Patriot, o grupo de Rebels e RPGs na vila.

## 4. Em batalha

- **Temperamento: o calculista frio.** Paciente, metódico, distante. Mede, espera, dispara, mede de novo. Nunca vê o rosto de quem
  queima.
- **O ritmo:** um tiro a cada 4 s, compassado: o estrondo longe, o silêncio de 2 s do voo, o "whump" do fogo lá na frente.
- **O jogador bom** o deixa atrás dos tanques, com um Gattling Tank do lado, e usa a visão dos outros pra mirar; mira no ponto onde
  a infantaria vai parar, não onde ela está.
- **A IA** o trata como artilharia (tem alcance mínimo): quer 1 + as defesas inimigas que conhece, com 2 tanques de escolta por
  lançador.
- **Sozinho e ferido:** morre rápido; não tem como fugir de nada que o alcance.

## 5. A vida

- **No jogo:** a torre gira 90°/s; o cano levanta no munhão pra atirar em arco e recua; a cratera A no chão; as esteiras e a
  poeira; os estados de dano.
- **Ideia (parado):** o cano em pose de marcha, travado no glacis; o diesel em marcha lenta.
- **Ideia (posicionando):** ao receber o alvo, a pá de apoio desce atrás (1 s, a poeira levanta), a trava solta, o cano sobe; ao
  andar de novo, tudo volta. Lê de cima: "ele parou pra atirar".
- **Ideia (atirando):** o casco inteiro afunda pra trás no recuo; a fumaça sai pelo freio de boca dos dois lados (as duas nuvens
  laterais típicas do freio de boca); o evacuador do cano solta um fio de fumaça depois.

## 6. O visual

- **De cima:** o chassi de 7,5 × 3,4 m, a torre grande atrás do meio, o cano longo passando 3,85 m além do nariz, a pá de apoio na
  traseira.
- **Assinatura:** o cano longo com o freio de boca.
- **Cores:** o oliva escuro com a camuflagem digital; o vermelho do jogador nas lonas dobradas (cesto atrás da torre e deck do
  motor), na faixa do cano e nas estrelas dos lados da torre.
- **Nunca:** tanque de combustível de lança-chamas à mostra (é obuseiro); cano duplo; faixa pintada.

## 7. Tiro, projétil, impacto, destruição

- **A boca:** o clarão grande e curto, as duas nuvens laterais do freio de boca (cinza, volume, acesas pelo clarão), a poeira do
  chão levantada em anel na frente.
- **O projétil:** a granada em arco alto, quase invisível, com um rastro fino de fumaça (ideia: o rastro tênue pra o jogador ver de
  onde veio).
- **O impacto (ideia, a física de uma carga incendiária):** a granada abre no chão em um clarão laranja-branco, joga brasas em leque
  até ~5 m (o raio do dano) e deixa o fogo baixo e a fumaça escura subindo; tudo volumétrico, na luz do mundo, acendendo as unidades
  em volta;
  - no soldado: o soldado pega fogo e cai rolando (a morte de soldado em chamas);
  - no tanque: o fogo escorre pela chapa e apaga em ~2 s (25%);
  - no prédio: o fogo pega no telhado e a fumaça preta sobe;
  - no chão: a mancha preta queimada que fica (o chão que lembra).
- **A morte:** a morte de veículo; ideia: as granadas incendiárias do paiol pegam fogo juntas, a torre se enche de chama que sai
  pela escotilha e pelo cano; a carcaça arde mais que a de um tanque.

## 8. O som

- **Seleção (variantes):** o diesel; o motor hidráulico do cano subindo (o "zuuum" longo); a culatra fechando (o "clank" pesado); o
  rádio com as coordenadas cantadas.
- **As falas** (inglês; o chefe de peça, voz fria, técnica, quase sussurrada, de quem faz conta):
  - selecionado: "Inferno, ready." / "Give me coordinates." / "Range and bearing." / "Shells loaded." / "The fire waits."
  - andando: "Relocating." / "Moving to firing position." / "Changing position."
  - atacando: "Fire mission." / "Shell away." / "Burn them out." / "Splash in three, two, one."
  - sob fogo: "We're exposed!" / "I can't fight up close!" / "Get them off me!"
  - pronto: "Inferno cannon, ready to fire."
- **O corpo:** o diesel de lagarta; a pá de apoio batendo no chão; a hidráulica do cano.
- **A arma:** o estrondo grave e longo (mais grave que o do tanque, com o eco rolando pelo mapa); o assobio da granada no alto
  (ouvido por quem está perto do impacto); no impacto, o "whump" abafado do fogo pegando e o crepitar.
- **A morte:** o estouro e o rugido do fogo do paiol, os estalos.

## 9. Ideias pra depois

- **O chão que queima:** a granada deixa o fogo no chão por 6 s (5 de fogo por segundo no soldado que passa por ali), o que o ZH
  tinha e aqui ainda não tem. Fecha a passagem da infantaria; o capim e a palmeira em volta pegam fogo e ficam queimados (o campo que
  muda).
- **A barragem:** o jogador traça uma linha e o Inferno distribui 5 granadas ao longo dela (a cortina de fogo), em vez de um ponto.
- **O observador:** um par de Red Guards marcado como observador do Inferno: o Inferno atira sozinho em tudo que o par vê, até os
  79 m dele (hoje ele só mira sozinho no que vê a 39,5 m). A dupla artilharia e observador avançado, como na guerra real.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#china_inferno_cannon` (feito no fim, com o modelo provisório como anexo de forma).
