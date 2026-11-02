# Red Guard (China, o fuzileiro em par)

## 1. Ficha rápida

| | |
|---|---|
| Papel | a infantaria barata da China: a massa que segura o chão |
| Nome na tela | Red Guard (a chave é `china_rifleman`) |
| Custo e tempo | **$300 o par**, 10 s: um clique, **dois soldados** (`produce_count 2`) |
| Vida e blindagem | 120 cada, soldado |
| Velocidade | 6,6 m/s (o soldado armado mais rápido do elenco) |
| Visão | 26,3 m |
| Arma | fuzil: 15 de arma leve a cada 1 s, alcance 26,3 m (só no chão) |
| Requer | Barracks |

## 2. A ideia

**Dois de cada vez.** O fuzileiro do ELP que nunca anda sozinho: sai do quartel em par, marcha em par e luta em par. Um soldado
não decide nada; vinte decidem.

- **A palavra de cima:** o PAR (dois capacetes juntos, sempre), com o colete vermelho do jogador.
- **Inspiração real:** o fuzileiro do ELP de hoje (o capacete, o colete, o fuzil). O nome "Red Guard" é o da tela; o soldado é o de
  hoje, sério, sem caricatura.
- **Por que existe:** a mecânica do par (o QuantityModifier do ZH, a que o dono pediu) dá à China a massa mais barata do jogo: $150
  por soldado, contra $225 do Ranger e $150 do Rebel (que vê mais longe, 39,5 m, e anda mais devagar, 5,3 m/s).

## 3. Como funciona

- **No jogo:** 15 a cada 1 s = **15/s em soldado**, 3,75/s em tanque (fuzil 25%), 7,5/s em prédio (50%). O par faz 30/s: mata um
  Rebel (120) em 4 s, um Ranger (180) em 6 s.
- **A conta (Lanchester, por soldado):** Red Guard 120 × 15 ÷ 150² = 0,080; Rebel 0,089; Ranger 0,059. Ele empata com o Rebel e
  ganha do Ranger pelo dinheiro.
- **A supressão (no jogo):** sob bala ele abaixa (o passo a 60%, a recarga a 135%) e, preso, rasteja (25%, 200%). Na carcaça ou no
  entulho, a bala faz metade.
- **Forte contra:** soldado (Rebel, Ranger, RPG, Javelin, o próprio Tank Hunter inimigo), o Worker, o raide a pé.
- **Fraco contra:** tanque (passa por cima: esmagamento 200%), a Gattling, a Quad Cannon, a Technical (50/s: o par cai em ~5 s), o
  Toxin Tractor, o Inferno (fogo 150%).
- **O contra dele:** a Quad Cannon, a Technical, o Gattling Tank, o Toxin Tractor. **Ele é o contra de:** o RPG e o Javelin que
  caçam os tanques da China.

## 4. Em batalha

- **Temperamento: o coro.** Disciplinado, unido, ruidoso em grupo. Sozinho, fica inseguro; com o par, firme.
- **O ritmo:** o tiro a cada 1 s, mas o grupo inteiro vira um crepitar contínuo; as falas saem em dupla (um começa, o outro
  responde).
- **O jogador bom** usa 4 a 6 pares na frente dos tanques pra pegar os RPGs antes de eles atirarem, e os esconde nas carcaças e no
  entulho (a cobertura nascida da batalha).
- **A IA** treina pelo que falta no quartel (o fuzil vale por 2 na mistura, e o par conta por compra).
- **Sozinho (o par perdeu um):** segue lutando; não foge.

## 5. A vida

- **No jogo:** o corpo humano com o fuzil (o modelo da Unreal), o colete e o capacete pintados na cor da facção; as pernas andam
  com o chão; o traçante a cada tiro (metade dos tiros, como no ZH) e o clarão pequeno; abaixado sob fogo e rastejando preso; as 6
  mortes animadas; achatado se esmagado.
- **Ideia (parado):** o par fica de frente pra lados opostos (um cobre o outro); de vez em quando um ajeita o capacete.
- **Ideia (andando):** os dois andam em passo igual, quase em marcha, mesmo no combate.
- **Ideia (atirando):** um atira, o outro atira 0,5 s depois (o fogo alternado do par: enquanto um recarrega, o outro cobre).

## 6. O visual

- **De cima:** dois soldados pequenos juntos, o colete vermelho (a cor do jogador) lendo como dois pontos vermelhos.
- **Assinatura:** o par.
- **Cores:** a farda oliva escura, o colete e o capacete na cor do jogador.
- **Nunca:** chapéu de pano maoísta, braçadeira vermelha de caricatura, bandeira (é da GLA).

## 7. Tiro, projétil, impacto, destruição

- **A boca:** o clarão pequeno e seco do fuzil; o fio de fumaça.
- **O projétil:** o traçante (2,6 m de risco, metade dos tiros), rápido.
- **O impacto:** no soldado, o tranco e o sangue (o ferido); no veículo, faísca; no prédio, o pó do reboco; no chão, a poeira
  pequena.
- **A morte (no jogo):** uma das 6 mortes animadas, o fuzil cai, o corpo fica 8 s; esmagado, achatado. Ideia: o parceiro vivo grita
  o nome do outro.

## 8. O som

- **Seleção (variantes):** a resposta em dupla (um fala, o outro completa); o ferrolho do fuzil; as botas batendo juntas; o rádio
  de ombro chiando curto.
- **As falas** (inglês; voz de soldado jovem, firme, a dois; muitas em dupla, a 2ª voz completando):
  - selecionado: "Red Guard!" — "Ready!" / "Two of us, sir." / "Together." / "For the people!" / "Standing by, both of us."
  - andando: "Moving, in step." / "Forward!" — "Forward!" / "Stay with me." / "Double time!"
  - atacando: "Open fire!" / "Cover me!" — "Covering!" / "Take them down." / "Shoot together!"
  - sob fogo: "Get down!" / "Pinned! Pinned!" / "Stay with me, brother!" / "Find cover!"
  - pronto: "Red Guard, reporting in pairs."
- **O corpo:** as botas na areia em passo igual; o equipamento batendo; a respiração no correr.
- **A arma:** o estalo seco do fuzil (uma vez por segundo cada, desencontrado entre os dois); o eco nos prédios.
- **A morte:** o grito curto; o fuzil caindo; o parceiro gritando.
- **A cara da China:** o coro, a disciplina, a voz em dupla.

## 9. Ideias pra depois

- **O par que não se separa:** se um morre, o outro recarrega 20% mais rápido por 10 s e não fica preso pela supressão (a fúria do
  parceiro); se os dois sobrevivem a uma luta, sobem de patente juntos.
- **A formação em linha:** 3 pares ou mais em linha de frente (o comando de formação) ganham 15% de dano (a salva coordenada), mas
  andam a 70%. A China disciplinada contra a GLA espalhada.
- **O assalto à baioneta:** o par corre a 9 m/s por 3 s e o soldado inimigo que estiver a menos de 2 m morre na hora (só contra
  soldado). Arriscado e com recarga de 30 s.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#china_rifleman` (feito no fim, com o modelo provisório como anexo de forma).
