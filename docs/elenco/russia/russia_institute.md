# Research Institute (Rússia, o NII)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o prédio de tecnologia da Rússia: destrava o pesado e a artilharia |
| Nome na tela | Research Institute (o NII, o НИИ: o instituto de pesquisa) |
| Custo e tempo | $2 000, 45 s |
| Vida e blindagem | 1 000, prédio (frágil pro preço, como o Propaganda Center da China) |
| Energia | −2 |
| Visão | 52,7 m |
| Lote | 16 × 22 m |
| Arma | nenhuma |
| Requer | Tank Plant |
| Destrava | TOS-1A e T-14 Armata |
| Estado | nos dados (`Units.json`) e no modelo provisório; fora do menu até os modelos da Rússia serem importados |

## 2. A ideia

**O domo branco ao lado do bloco de painel, e o mastro espetado de antenas.** O instituto de pesquisa soviético dos anos 70: o bloco
de 3 andares com as janelas largas, o relevo do átomo na empena cega, a sputnik no pedestal da praça; ao lado, a torre do radar com o
domo geodésico branco, e atrás o mastro de treliça cheio de antenas de guerra eletrônica.

- **A palavra de cima:** o DOMO BRANCO (a maior bola branca da base: a assinatura redonda num mundo de retângulos) + o mastro de
  antenas no canto.
- **Inspiração real:** os НИИ soviéticos (o painel decorativo do átomo, a sputnik como monumento), os radomes geodésicos e as
  antenas de guerra eletrônica da doutrina russa (o Krasukha e o Leer-3: as log-periódicas, os painéis, as Yagis).
- **Por que existe:** é a porta do fim de jogo da Rússia. Caro, demorado e frágil: perder ele corta o TOS-1A e o T-14.

## 3. Como funciona

- **No jogo:** $2 000, 45 s, −2 de energia, 1 000 de vida. Pré-requisito do TOS-1A e do T-14 (`requires` deles). Não produz nem
  atira; nada gira na malha (o domo e o mastro são fixos).
- **O par:** igual ao Propaganda Center da China ($2 000, 45 s, 1 000, −2); mais barato e mais frágil que o Strategy Center da USA
  ($2 500, 60 s, 1 500) e o Palace da GLA ($2 500, 45 s, 3 000).
- **O peso:** com 1 000 de vida, um tanque o derruba em ~33 s e 4 Rocket Buggies em pouco mais de 2 salvas. Defender é obrigatório.
- **A guerra eletrônica:** o estudo (`docs/FACCAO_RUSSIA.md` §4.3) põe nele a antena que faz o míssil guiado do inimigo errar; **não
  está no jogo** (ideia, §9).
- **Forte contra:** nada; ele é a chave.
- **Fraco contra:** raide, artilharia, Rocket Buggy de longe, o Apache que vem direto nele.
- **O contra dele:** o raide cirúrgico. **Ele é o contra de:** o fim de jogo sem o T-14 e sem o TOS-1A.

## 4. Em batalha

- **Temperamento: o acadêmico.** Frio, distante, curioso. Fala baixo e em termos técnicos; acha a guerra um problema de engenharia.
- **O ritmo:** quase silencioso; o bip das antenas e o zumbido do radar dentro do domo.
- **O jogador bom** o constrói atrás da fábrica, coberto por um Pantsir fixo (o ar da USA mira nele) e fora dos 18 m da usina a gás.
- **A IA** faz um só, depois da fábrica, com a 1ª onda fora ou a partir de 5 min, e com o preço dele + um tanque sobrando
  (`docs/ELENCO.md` §7.5).
- **Sozinho e ferido:** abaixo de 35%, o fogo nos 3 pontos; o domo segue branco até cair.

## 5. A vida

- **No jogo:** o andaime de aço na obra (peça `_Scaffold`); a bandeira lisa na cor do jogador no mastro da praça (`flags`); abaixo de
  35%, o fogo nos 3 pontos (`fire_points_m`).
- **Ideia (parado):** as luzes de obstáculo vermelhas no topo do mastro e no para-raios do domo piscam (o slot `Beacon` já está no
  modelo); as 2 faixas verticais de pano da fachada balançam; as janelas acesas até tarde (o laboratório que não dorme).
- **Ideia (quando a fábrica entrega um TOS-1A ou um T-14):** as antenas do mastro acendem em sequência, de baixo pra cima, e o bip
  longo: o instituto "assina" a máquina.
- **Ideia (sem energia):** as luzes do mastro apagam, o radar para; o domo fica cinza no escuro.
- **Ideia (ferido):** o domo perde os gomos triangulares um a um (a geodésica abrindo), e as antenas pendem.

## 6. O visual

- **De cima:** a bola branca do domo geodésico sobre o tambor de painéis curvos, o bloco de teto azul-acinzentado com a casa de
  máquinas e os condensadores, o mastro de treliça vermelho e branco espetado de antenas atrás do domo, a sputnik no pedestal na
  praça da frente; de frente, as 2 faixas verticais vermelhas com a estrela na fachada e o relevo do átomo branco na empena.
- **Assinatura:** o domo branco + o mastro de antenas.
- **Cores:** o painel cinza, o branco do domo e das órbitas do átomo (o núcleo vermelho); o vermelho e branco do mastro (a
  sinalização de aviação, fixa); a cor do jogador nas 2 faixas verticais da fachada e na bandeira da praça.
- **Nunca:** o prato de satélite girando (é da USA); o alto-falante (China); a cúpula de palácio (GLA); Z ou V; o letreiro que forme
  palavra.

## 7. Tiro, projétil, impacto, destruição

- **Impacto (ideia):** a bala no domo fura o painel e deixa o furo escuro; o canhão arranca um gomo inteiro; o míssil no mastro
  derruba uma antena, que fica pendurada pelo cabo.
- **A morte (no jogo):** a morte de prédio (a grande no meio, 3 menores, o chão queimado, o entulho: $200 em 14 s pro construtor).
- **A morte (ideia):** o domo afunda dentro do tambor (a casca branca quebrando em triângulos), o mastro tomba sobre o bloco; a
  sputnik rola do pedestal e fica no entulho, inteira e brilhando.

## 8. O som

- **Seleção (variantes):** o "bip-bip" curto de sinal de satélite (a citação da Sputnik de 1957); o chiado de interferência subindo
  e cortando (a guerra eletrônica); os relés estalando; o teclado mecânico.
- **As falas** (inglês, com o sotaque russo leve; o cientista-chefe, voz calma, culta, um pouco distante):
  - selecionado: "Institute." / "The calculations are complete." / "Science serves the front." / "Interesting." / "We are listening
    to their radios."
  - sob ataque: "They are targeting the institute!" / "Protect the laboratory!" / "Save the data!"
  - pronto: "Research institute operational." / "New designs are available."
- **O corpo:** o zumbido do radar dentro do domo; o chiado baixo das antenas, que muda de tom de vez em quando (a varredura de
  frequência); os relés.
- **A construção:** os painéis do tambor assentados, os gomos do domo encaixando um a um (o "toc" leve e oco), o mastro aparafusado;
  no fim, o "bip-bip" e o chiado que vira tom.
- **A morte:** os estouros; o domo quebrando (o plástico e o metal leve); o mastro tombando; a interferência subindo até o silêncio.
- **A cara da Rússia:** o rádio de onda curta, o concreto grave, a voz baixa e seca.

## 9. Ideias pra depois

- **A guerra eletrônica** (estudo §4.3): numa área de ~40 m em volta do mastro, os mísseis guiados do inimigo (o Javelin, o
  Tomahawk, o Patriot) erram mais (o espalhado vezes 3) e os drones caem; de cima, o anel tremido de interferência. É a resposta à
  precisão da USA.
- **O Krasukha:** o caminhão de guerra eletrônica (o mastro dobrável sobre o chassi de 8 rodas), destravado aqui: a mesma bolha,
  móvel, que anda com o exército.
- **O drone observador:** o Instituto lança o Orlan (o drone pequeno de asa fixa), que dá visão a 70 m pra artilharia (o TOS-1A
  atira no que ele vê); a guerra eletrônica inimiga o derruba.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#russia_institute` (feito no fim, com o modelo provisório como anexo de forma).
