# A estrela é de quem? Os emblemas da China e da Rússia (item 43 do `TAREFAS.md`, 01/11/2026)

O Felipe, 01/11 14h35: "tem um problema que descobrimos no ícone, a estrela da Rússia e a da China são exatamente iguais... quem deve
ficar com a estrela? o que o outro deve ter? [...] a outra sessão disse que a Rússia hoje em dia não usa mais estrela".

## 0. DECIDIDO (01/11, 19h10, aprovado com a imagem `docs/aprovados/modelos/2026-11-01_estrelas_russia_china_APROVADAS.jpg`)

As DUAS ficam com a estrela vermelha, cada uma com a sua (ele: "quando se fala de estrela vermelha, o que todo mundo vai ver na mente
primeiro: Rússia ou China?"): **a Rússia com a borda branca, azul e vermelha** (a insígnia das Forças Armadas russas nos aviões de hoje)
e **a China com a borda dourada e o 八一 dourado no meio** (o emblema do Exército de Libertação Popular). A águia, o escudo e a
bandeirinha tricolor das seções abaixo ficaram pra trás. As bandeiras de pano seguem: a da Rússia com a tricolor, a da USA com as
listras e as estrelas. O ícone da facção no HUD é da sessão do HUD (este arquivo é a referência).

## 1. O que cada uma usa de verdade

- **China: a estrela é dela.** O emblema do Exército de Libertação Popular é a estrela vermelha de 5 pontas com a borda dourada e os
  caracteres dourados **八一** no meio (o 1º de agosto de 1927, a fundação). Adotado em 1949, vai no quepe, nos veículos, nos navios,
  nos aviões e nos prédios. A bandeira de guerra é vermelha com a estrela amarela e o 八一 no canto.
  [Wikipédia, o emblema do ELP](https://en.wikipedia.org/wiki/Emblem_of_the_People%27s_Liberation_Army);
  [a bandeira do ELP](https://en.wikipedia.org/wiki/Flag_of_the_People%27s_Liberation_Army).
- **Rússia: a outra sessão está meio certa.** A estrela vermelha sobrevive no quepe e, contornada de branco e azul, nos aviões. Mas não é
  mais o símbolo da força: desde 1997 o emblema das Forças Armadas é a **águia bicéfala dourada** (a do brasão do país, segurando a espada).
  Os blindados não têm uma insígnia nacional fixa: levam a **bandeira tricolor** (branco, azul, vermelho) e o número tático. As letras
  da guerra atual (Z, V) ficam fora, como já diz o `docs/FACCAO_RUSSIA.md` ("nunca o símbolo da guerra real").
  [Lista das bandeiras russas](https://en.wikipedia.org/wiki/List_of_Russian_flags); [as Forças Terrestres](https://en.wikipedia.org/wiki/Russian_Army).

## 2. A decisão

| | Hoje | Fica |
|---|---|---|
| **China** | a estrela vermelha | **a estrela do ELP de verdade**: vermelha com a borda dourada e o 八一 dourado no meio (o miolo que a da Rússia nunca teve) |
| **Rússia** | a estrela vermelha contornada de branco (a "tradição", `FACCAO_RUSSIA.md` §A estrela) | **a águia bicéfala dourada** nos prédios (o frontão do Shtab, a fábrica, o Instituto) e nas bandeiras; nos blindados, a **tricolor** pequena na lateral da torre e o número tático branco |

Por que assim: a estrela é **o** emblema da China hoje (está em tudo do ELP); na Rússia ela virou detalhe de quepe e de avião. A águia
de duas cabeças é única no jogo e lê de longe pela silhueta (as duas cabeças e as asas abertas), e a tricolor dá a cor da Rússia sem
repetir o vermelho da China.

## 3. O que muda (na vez dele na ordem do `TAREFAS.md`)

- Os modelos da Rússia: a `ru_star` (`Game/Tools/model_russia_parts.py`) sai dos 6 prédios e vira a águia em relevo (o escudo com a
  silhueta da águia, dourada no slot de latão) e a tricolor nos veículos; refazer o destrutor, o tiro e a importação dos que mudam.
- As bandeiras com física (`M_FlagCloth`, o emblema 3 = a águia): o desenho da águia como máscara (uma silhueta simples e forte; o
  Atelier gera a referência) e a bandeira russa (a tricolor com a águia).
- A estrela da China ganha a borda dourada e o 八一 onde ela aparece grande (as bandeiras, o portão).
- O ícone da facção no HUD é da sessão do HUD; esta decisão vale pra ele também (o Felipe passa pra ela).

## 4. Achado no jogo (01/11 16h45): a bandeira da USA é um pano azul liso

Nas fotos da base (`captures/game/usa_base_predios`), as bandeiras do quartel e da War Factory saem como um retângulo azul sem nada
(o emblema 0 = nenhum). A USA também precisa do símbolo dela no pano: as listras e o cantão de estrelas, com o cantão na cor do
jogador (a leitura do time) e as listras em branco e vermelho apagado. Entra junto com a águia da Rússia (o emblema 4 no `M_FlagCloth`).
