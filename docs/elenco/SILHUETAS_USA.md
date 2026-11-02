# Quem é quem: os blindados de lagarta da USA de cima (item 41 do `TAREFAS.md`, 01/11/2026)

O pedido (01/11, 13h30): "o Paladin e o Abrams não têm diferença nenhuma visualmente [...] eu bato o olho e não consegue ler, não
consegue distinguir, isso é PÉSSIMO pra um RTS AAA onde cada unidade é ICÔNICA e AUTÊNTICA"; "até o de atirar pra cima [o
Linebacker] tem esse probleminha na parte de baixo, só a torre que se distingue".

## 1. O diagnóstico (a folha `captures/game/usa_b_tour_A`, os três de cima e de lado)

- **Paladin × Abrams:** o mesmo casco de retângulo, as mesmas saias lisas com os retângulos azuis, 7 rodas, a torre larga no meio.
  O Paladin é só **6% maior** (8,6 × 4,0 m contra 8,1 × 3,8 m). O laser, que devia ser a assinatura, é uma torrinha de ~0,7 m
  que de cima vira um ponto. A cunha da torre (o design 3 de `docs/concept/units/usa_mbt_exploration.webp`) não basta: os 4
  designs daquela folha têm a mesma silhueta (a folha era pra escolher o tanque básico, não pra separar unidades).
- **Linebacker × Abrams:** a torre se separa (os 2 canos e o prato), mas o casco é o do tanque, com as mesmas saias lisas e
  a faixa azul em retângulos, e com o `mesh_scale` 1,25 ele fica do tamanho do tanque.

## 2. Como os outros resolvem

- **A ordem da leitura:** primeiro a silhueta, depois o valor e a cor, só então o detalhe. Cada classe do TF2 se reconhece só
  pela sombra; o detalhe fica no miolo e nunca quebra o contorno.
  [Mitchell, Francke e Eng, "Illustrative Rendering in Team Fortress 2", NPAR 2007](https://steamcdn-a.akamaihd.net/apps/valve/2007/NPAR07_IllustrativeRenderingInTeamFortress2.pdf).
- **Os blindados de verdade já fazem isso**, e é o que dá a cara "autêntica": o M2/M6 Bradley tem o casco **curto e alto**, a frente
  em rampa com o motor na frente, **6 rodas** e a **torre deslocada pra esquerda**. O M1 tem o casco **comprido e baixo**, 7 rodas, a
  torre larga no meio e o cesto atrás. Os tanques pesados de geração nova (o Leopard 2A6, o Challenger 3, o protótipo AbramsX) lêem
  pelo **bico em cunha** na frente da torre e pelos módulos de blindagem por cima das saias.
- **No ZH:** o Crusader e o Paladin dividem casco, e o Paladin se lê pelo laser em cima da torre. O Avenger tem casco próprio e a
  grande torre do laser. A nossa regra é mais dura ("cada unidade ICÔNICA"): **cada um com o casco próprio**, não só a torre.

## 3. A regra: cada um diferente em CINCO coisas, de cima

| | **Abrams** (aprovado, fica) | **Paladin** (o pesado, fim de jogo) | **Linebacker** (o antiaéreo) |
|---|---|---|---|
| Tamanho no jogo | 8,1 × 3,8 m (a régua) | **~9,4 × 4,4 m** (+16%, +15%): o maior de lagarta | **~6,8 × 3,6 m** (−16%): menor que o tanque |
| Contorno do casco | retângulo | **hexágono esticado**: o nariz em **V** (as duas chapas em bico) e os **sponsons** de blindagem por cima das saias, em 2 degraus | **caixa curta e alta**, frente reta em rampa, com a prancha da frente |
| A torre | larga, chata, no meio | **ponta de flecha comprida e baixa, recuada** ~0,6 m; o canhão **mais longo e grosso**, com o **freio de boca** | **pequena, deslocada pra ESQUERDA** (como o Bradley); os 2 canos |
| A assinatura | o cesto e a lona azul atrás | **o LASER grande**: a cúpula de ~1,3 m num pedestal alto atrás da torre, a lente grande acesa (gira pra mirar o míssil) | **o prato do radar** no mastro e as **4 tampas** dos mísseis atrás |
| As laterais e a cor do jogador | as saias lisas com a faixa azul | **a blindagem em blocos** (ERA) por cima das saias, sem a faixa; a cor do jogador numa **seta grande** no teto da cunha | **a grade de ladrilhos** de blindagem (o BUSK do Bradley) nos lados; a cor do jogador nos ladrilhos e nas 4 tampas |
| Rodas | 7 | **8**, maiores | **6** |

Os três ficam com a mesma família: a tinta, as chapas, a lona, a esteira que anda, o detalhe B (`model_usa_mbt.py`).

## 4. A prova

A folha "quem é quem": os três lado a lado **na câmera do jogo** (de cima e na 3/4 do game shot) e um teste cego: das fotos
cortadas em ~150 px (o tamanho do tanque na tela), cada um tem que se reconhecer só pela silhueta preta.
