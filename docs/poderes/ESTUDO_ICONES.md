# Estudo: como o ícone diz o tipo do poder

Levantamento de 03/11/2026. Base pros ícones dos poderes do general: o tipo tem de se ler pelo desenho, sem legenda
(regra dele: "se precisa de legenda, algo está mal feito"). O texto do mouse continua valendo: é a "reação ao mouse".

**Convenções**
- **n/c** = não confirmado (a fonte é de fã, de fórum ou de memória, ou não deu pra abrir).
- **Leitura minha** = conclusão minha a partir das fontes, não fato publicado.
- **Imagem vista** = baixei o arquivo do wiki e olhei.
- **ZH pelo código** = motor liberado ([GeneralsGameCode](https://github.com/TheSuperHackers/GeneralsGameCode), cópia em
  `C:\Users\luisf\newcnc\GeneralsGameCode`; as linhas são da cópia) e INI do ZH 1.04
  ([GeneralsGamePatch](https://github.com/TheSuperHackers/GeneralsGamePatch/tree/main/Patch104pZH/GameFilesOriginalZH/Data/INI)).

---

## Resumo

1. Nenhum jogo grande separa 6 tipos só pela moldura. Os que funcionam mostram de 2 a 4 famílias de relance e deixam o resto pra imagem.
2. O corte mais usado é pelo jeito de usar: o que se aperta tem relevo de botão, e o passivo é chapado e sem tecla (Warcraft III, Dota 2).
3. O desbloqueio quase sempre é a própria imagem da unidade. No ZH, o Paladin da patente usa o mesmo cameo do botão de construir.
4. O ZH já tinha um código de tipo, uma borda de 1 px (azul = construir, verde = ação, laranja = melhoria). Na tela de patente, tudo é laranja.
5. O Red Alert 3 separa poder de unidade pelo jeito de desenhar: o protocolo é um pictograma chapado na cor da facção, a unidade é um render 3D.
6. O Hearthstone muda a forma da janela da arte por tipo de carta (oval, arco, anel, arco gótico) e deixa o resto igual.
7. A simbologia militar (APP-6) usa forma e cor redundantes e funciona sem cor. Os glifos dela foram feitos pra ler pequeno.
8. Cor nunca sozinha (Xbox XAG 103). Forma e cor juntas não saltam aos olhos (Treisman). Perto de 6 categorias, a leitura cai.
9. O que dá errado: ícones parecidos (CoH3), árvore confusa (HW2), marcas pequenas demais (Gage), brilho com dois sentidos (Riot).
10. No fim há 3 sistemas candidatos: pelo jeito de usar, pela marca militar num lugar fixo e pelo meio da imagem.

---

## 0. O nosso problema

- **6 tipos:** golpe, olhos, tropas, suporte, bônus/passivo e desbloqueio (de unidade ou prédio).
- **3 tamanhos:** 46 px na grade da tela de poderes, **56×40 px (deitado)** na coluna do HUD e 90 px no detalhe.
- **A cor já tem dono:** verde = comprado, dourado = dá pra comprar (`USA.md` §13).
- **Facções:** cada uma tem o seu HUD aprovado, e já houve a ideia de um estilo de ícone por facção (`USA.md` §12).
- **A tentativa de 03/11 foi reprovada** ("mal pensada, sem embasamento"): uma moldura com forma por tipo, um selo colorido no canto e
  4 estilos de arte. O motivo, na leitura minha, está no §4.

---

## 1. Como os jogos codificam o tipo, jogo a jogo

### 1.1 A série C&C

| Jogo | O que separa | Como | Fonte |
|---|---|---|---|
| **Generals / Zero Hour**, barra de comando | construir × ação × melhoria × sistema | Todo cameo é o mesmo quadrado pintado. O motor desenha **uma borda de 1 px** com a cor do tipo: BUILD azul (67,108,190), ACTION verde (1,175,2), UPGRADE laranja (208,108,0), SYSTEM amarelo (207,195,2). A forma não muda. | ZH pelo código: `ControlBar.h:276` (os 4 tipos), `ControlBar.cpp:2417` e `:2526`, `W3DPushButton.cpp:270` (`drawOpenRect(..., 1, cor)`); cores em `ControlBarScheme.ini › America8x6` |
| ZH, uso | — | 450 botões de construir são BUILD, 77 poderes são ACTION, 135 sciences e melhorias de jogador são UPGRADE. | `CommandButton.ini` (contagem minha) |
| **ZH, tela de patente** | desbloqueio × poder | **Toda science tem a borda laranja (UPGRADE).** A borda não separa nada ali. O que separa é a imagem: o Paladin da patente usa `SAPaladin`, a mesma do botão de construir o Paladin; o A-10 usa `SSA10Attack`, a mesma do botão do poder. | `CommandButton.ini › Command_PurchaseSciencePaladinTank` × `Command_ConstructAmericaVehiclePaladin`; `› Command_A10ThunderboltMissileStrike` |
| ZH, nível e estado | I, II, III; comprado | Cada nível tem a sua arte (`SSA10Attack`, `SSA10Attack2`, `SAWarthog`; `SACParatroopers`, `SACParatroopers2`). O comprado fica sempre colorido, e o resto fica desabilitado. A recarga é um relógio escuro (preto, alfa 160) que varre o botão. | `CommandButton.ini`; `ControlBar.cpp` (`WIN_STATUS_ALWAYS_COLOR`); `ControlBarCommand.cpp` (`GadgetButtonDrawInverseClock`), `BuildUpClockColor` |
| **Red Alert 2 / Yuri's Revenge** | estrutura × defesa e poder × infantaria × veículo | **Pela aba.** São 4 abas na barra lateral (Q, W, E, R), e a 2ª junta as defesas e os poderes. O cameo é um retrato, e a aba diz o tipo. | [C&C Wiki: Sidebar](https://cnc.fandom.com/wiki/Sidebar) |
| **Tiberian Sun** | — | n/c: a fonte só diz que a barra lateral é parecida com a anterior. | [C&C Wiki: Sidebar](https://cnc.fandom.com/wiki/Sidebar) |
| **C&C 3 / Kane's Wrath** | produção × poderes | **Pela posição.** A barra lateral ganhou abas (até aeronaves), e os poderes ficam numa coluna própria na borda esquerda da tela. A arte dos ícones de poder: n/c. | [C&C Wiki: Sidebar](https://cnc.fandom.com/wiki/Sidebar) |
| **Red Alert 3**, protocolos | protocolo × unidade | **Pelo jeito de desenhar** (imagem vista). O protocolo é um pictograma chapado na cor da facção (Aliados azul, Sovietes vermelho), com fundo de raios ou de listras diagonais. A unidade é um render 3D com moldura própria (o Guardian Tank). O protocolo de níveis leva um chip dourado com o algarismo romano (I, III) no canto de baixo à direita. | [Top Secret Protocols](https://cnc.fandom.com/wiki/Top_Secret_Protocols); ícones [Surgical Strike](https://cnc.fandom.com/wiki/File:RA3_Surgical_Strike_Icons.png), [Paratroop Platoon](https://cnc.fandom.com/wiki/File:RA3_Paratroop_Platoon_Icons.png), [High Technology](https://cnc.fandom.com/wiki/File:RA3_High_Technology_Icons.png), [Guardian Tank](https://cnc.fandom.com/wiki/File:RA3_Guardian_Tank_Icons.png) |
| RA3, entre protocolos | passivo × ativo | **Não separa.** A engrenagem (High Technology), o gráfico (Free Trade) e as lagartas (Grinder Treads) têm o mesmo jeito do floco de neve (Cryoshot) e do bombardeiro (Surgical Strike). Só o assunto muda. | imagem vista; [Top Secret Protocols](https://cnc.fandom.com/wiki/Top_Secret_Protocols) |

**O que tirar da série**
- O C&C nunca pôs o tipo no desenho do poder. Ele usa a aba ou a posição (RA2, C&C3), o render (RA3) ou uma borda de 1 px (ZH).
- A borda de 1 px do ZH é um sinal fraco, e quase ninguém sabe que ela existe (leitura minha).
- O desbloqueio do ZH é a coisa mais simples que existe: a imagem da unidade.

### 1.2 Blizzard: Warcraft III e StarCraft II

| Jogo | O que separa | Como | Fonte |
|---|---|---|---|
| **Warcraft III** | ativo × passivo × desabilitado | Há 4 arquivos por ícone. O BTN (normal) "tem moldura"; o PASBTN (passivo) "não tem moldura"; o DISBTN (desabilitado) não tem moldura e é bem mais escuro. | [Creating Custom Icons](https://world-editor-tutorials.thehelper.net/icons.php) |
| WC3 | autocast | Quem pode ter autocast leva um colchete amarelo em cada canto. Ligado, ganha faíscas amarelas que giram na borda. | [Wowpedia: Auto-cast](https://wowpedia.fandom.com/wiki/Auto-cast) |
| WC3 | passivo no cartão | O passivo ganha um botão no cartão de comando, mas "está sempre ligado". | [Battle.net: Spell Basics](http://classic.battle.net/war3/basics/spellbasics.shtml) |
| **StarCraft II** | posição | O perfil Grid liga 15 teclas a 15 posições fixas: o lugar do botão é fixo. | [StarCraft Wiki: Hotkey](https://starcraft.fandom.com/wiki/Hotkey) |
| SC2 | autocast | Há uma lista grande de autocast (Heal, Repair, Build Interceptor). A borda animada vem do WC3 (n/c no desenho exato). | [StarCraft Wiki: Autocast](https://starcraft.fandom.com/wiki/Autocast) |
| SC2 | passivo × ativo | Um modder diz que o ativo tem uma borda que o passivo não tem: n/c (o fórum saiu do ar). | — |
| SC2 Co-op | poderes do comandante | n/c (não achei fonte do desenho). | — |

### 1.3 MOBA: Dota 2 e League of Legends

| Jogo | O que separa | Como | Fonte |
|---|---|---|---|
| **Dota 2** | ativo | Há um **chanfro em volta do ícone, "como um botão"**. Ao usar, o chanfro fica verde e o ícone afunda, "como um botão apertado". Ao mirar, fica mais claro. Tem a tecla no canto de cima e o custo de mana embaixo (imagem vista). | [Dota 2 Wiki: Abilities](https://dota2.fandom.com/wiki/Abilities) |
| Dota 2 | passivo | Não tem chanfro e parece **"apertado pra dentro do HUD"**. A aparência nunca muda e não há tecla. | idem |
| Dota 2 | autocast | Ganha um 2º chanfro, mais grosso, que **brilha laranja** quando ligado. | idem |
| Dota 2 | buff × debuff | Borda verde = buff, borda vermelha = debuff. | [Dota 2 Wiki: HUD](https://dota2.fandom.com/wiki/Head-up_display) |
| Dota 2 | ultimate | Um losango embaixo do retrato do herói: verde = pronto, azul = sem mana, cinza = em recarga. | idem |
| **League of Legends** | inato × básico × ultimate | Há um inato sem nível, 3 básicos com 5 níveis e o ultimate com 3. O tipo vem do lugar (o R) e do número de níveis. | [LoL Wiki: Champion ability](https://leagueoflegends.fandom.com/wiki/Champion_ability) |
| LoL | estado | O ultimate tem um aviso no retrato (preto = não aprendido, azul = recarga, verde = pronto). Sem recurso, o ícone fica azul. | idem |
| LoL | ícone de item (10.24) | Metas da Riot: silhueta mais clara, menos ruído, mais separação de cor. n/c (a matéria bloqueou; veio de resumo de busca). | Dotesports, via busca |

**Riot, "Clarity in League"** (regras pra personagem e habilidade; a aplicação a ícone é leitura minha):
- um traço principal reconhecível na sombra;
- a hierarquia visual igual à importância no jogo;
- evitar um brilho quase permanente que possa querer dizer duas coisas;
- testar contra todo fundo onde aparece.
- Fonte: [Clarity in League](https://www.leagueoflegends.com/en-us/news/dev/clarity-in-league/).

### 1.4 Jogos de carta: Hearthstone e Magic

| Jogo | O que separa | Como | Fonte |
|---|---|---|---|
| **Hearthstone** | lacaio × feitiço × arma × local | **A forma da janela da arte muda; o resto da carta é igual** (imagem vista): lacaio = oval em pé; feitiço = retângulo de topo em arco; arma = anel de metal com espetos; local = arco ornamentado. | [Minion](https://hearthstone.wiki.gg/wiki/Minion_card), [Spell](https://hearthstone.wiki.gg/wiki/Spell_card), [Weapon](https://hearthstone.wiki.gg/wiki/Weapon_card), [Location](https://hearthstone.wiki.gg/wiki/Location) |
| Hearthstone, no tabuleiro | palavras-chave | **Só a mais importante muda a silhueta:** o Taunt ganha uma borda de escudo. As outras são **glifos pequenos num lugar fixo, embaixo do retrato**: raio (efeito disparado), caveira (Deathrattle), frasco (Poison), coração partido (Lifesteal). | [Minion card › Portraits](https://hearthstone.wiki.gg/wiki/Minion_card) |
| Hearthstone | arma | "Tentamos ser bem claros: uma arma só, não quem a segura" (artista, sobre as cartas de arma). Em 2020, as molduras de arma ganharam a classe. | [GameSpot](https://www.gamespot.com/articles/the-glorious-art-behind-hearthstones-the-grand-tou/1100-6429343/); [Weapon card](https://hearthstone.wiki.gg/wiki/Weapon_card) |
| **Magic: The Gathering** | cor × tipo | **A moldura diz a cor** (o mana); **o tipo vem escrito** na linha de tipo. Só alguns tipos ganham moldura própria: o planeswalker (texto translúcido e escudos de lealdade), a batalha (**a carta deitada**), o lendário (coroa no título), o veículo (padrão bronze e cinza) e o encantamento (o céu estrelado de Nyx). | [MTG Wiki: Card frame](https://mtg.fandom.com/wiki/Card_frame) |
| Magic, o erro | branco × artefato | Na moldura de 2003, branco e artefato se confundiam "de relance". A correção foi **escurecer** a moldura do artefato (diferença de valor, não de cor). | idem |
| Magic, a regra | — | Mark Rosewater: a moldura faz o trabalho de muitas palavras. | idem |

### 1.5 RPG e tiro: WoW, Diablo IV, Path of Exile, Destiny 2, Overwatch, Valorant

| Jogo | O que separa | Como | Fonte |
|---|---|---|---|
| **World of Warcraft** | tipo do debuff | **Só a cor da borda** do ícone: Curse roxo (0,6; 0; 1), Disease marrom (0,6; 0,4; 0), Magic azul (0,2; 0,6; 1), Poison verde (0; 0,6; 0), o resto vermelho. Existe desde o patch 1.11. | [Warcraft Wiki: Debuff](https://warcraft.wiki.gg/wiki/Debuff) |
| WoW | autocast | Um triângulo amarelo pequeno em cada canto. | [Wowpedia: Auto-cast](https://wowpedia.fandom.com/wiki/Auto-cast) |
| **Diablo IV** | ativa × melhoria × passiva | Na árvore, o nó quadrado é a habilidade ativa e o redondo melhora. As fontes divergem sobre o passivo e são da prévia de 2020: n/c no jogo lançado. | [guia de 2023](https://diablosguides.blogspot.com/2023/03/diablo-iv-skill-tree-guide.html) (n/c) |
| **Path of Exile** | atributo; ativa × suporte | A cor da gema é o atributo (vermelho, verde, azul). **A gema de suporte põe uma letra ou um símbolo colorido no ícone da habilidade** na barra. | [PoE Wiki: Support gem](https://pathofexile.fandom.com/wiki/Support_gem), [Skill gem](https://pathofexile.fandom.com/wiki/Skill_gem) |
| PoE, árvore passiva | pequeno × notável × keystone | O tamanho e a moldura crescem com o peso do nó. n/c (o wiki oficial bloqueou). | — |
| **Destiny 2** | elemento | A cor é o elemento (Solar laranja, Arc azul-claro, Void roxo, Stasis azul-escuro, Strand verde). Arc e Stasis são dois azuis (leitura minha: risco pra daltônico). Um daltônico conta que a troca pra escudos com forma ajudou: n/c. | [Destiny Wiki: Element](https://d2.destinygamewiki.com/wiki/Element) (n/c); [fórum Bungie](https://www.bungie.net/de/Forums/Post/262865955) |
| **Overwatch 2** | função (tanque, dano, suporte) | Cada função tem um glifo próprio, e o passivo da função usa o glifo num círculo. Desenho exato: n/c. | [Overwatch Wiki: Role](https://overwatch.fandom.com/wiki/Role) |
| **Valorant** | habilidades | O desenho é branco e chapado. O tipo vem da tecla (C, Q, E, X), e o X mostra as cargas. n/c. | — |

### 1.6 Estratégia moderna

| Jogo | O que separa | Como | Fonte |
|---|---|---|---|
| **Civilization VI** | militar × econômica × diplomática × curinga | Cada tipo de carta tem uma vaga de **cor própria com uma marca-d'água** (imagem vista): vermelho com escudo, amarelo, verde, e o curinga com cor mista. O curinga aceita qualquer carta. O desenho exato das outras marcas: n/c. | [Policy card (Civ6)](https://civilization.fandom.com/wiki/Policy_card_(Civ6)); [Cards_merged.png](https://civilization.fandom.com/wiki/File:Cards_merged.png) |
| **Age of Empires IV** | tecnologia × unidade | Pelo fundo: verde = tecnologia, marrom = unidade (jogador no fórum, n/c oficial). O símbolo chapado gerou o debate dos "sinais de trânsito": personalidade × leitura. | [fórum: UI icons = Traffic Signs?](https://forums.ageofempires.com/t/ui-icons-traffic-signs/134362) |
| **Total War: Warhammer** | unidade × magia | **Pela posição:** os cartões de unidade ficam embaixo (divisas de experiência; piscam em vermelho sob ataque e ficam cinza na fuga), e as magias têm um painel próprio. A codificação das habilidades por tipo: n/c. | [TW Wiki: Battle UI](https://totalwarwarhammer.fandom.com/wiki/Battle_User_Interface) |
| **Company of Heroes 3** | ativa × unidade chamada × passiva | O grupo de batalha mistura as três. Um patch ordenou o painel de baixo pra cima, as ativas antes das passivas (**pela posição**). O retrabalho de ícones achou "ícones parecidos demais" e transparência que misturava com o HUD, e pediu bordas mais duras. | [RetroStyle: análise do CoH3](https://retrostylegames.com/blog/analyzing-company-of-heroes-3-game-design/); [fórum oficial](https://community.companyofheroes.com/coh-franchise-home/company-of-heroes-3/forums/1-general-discussion/threads/1602-company-of-heroes-3-battlegroups) (n/c) |
| **Halo Wars 2** | poderes do líder | No beta, as árvores de evolução eram "confusas, ou não distintas o bastante". O jogo final usa um menu radial. | [Xbox Wire](https://news.xbox.com/en-us/?p=43371); [Halopedia](https://halopedia.org/Leader_power) |
| **Tempest Rising** | poderes × doutrinas | O poder abre com o prédio pré-requisito, e cada lado tem um HUD diferente. O desenho dos ícones: n/c. | [wiki](https://tempestrising.wiki.gg/wiki/Tempest_Rising); [Game8](https://game8.co/articles/reviews/tempest-rising-review) |

### 1.7 Fora dos jogos: a simbologia militar (APP-6 / MIL-STD-2525)

É o sistema real mais próximo do nosso tom: tipo de relance, em tamanho pequeno e sem depender da cor.

| O que codifica | Como | Fonte |
|---|---|---|
| Afiliação | **Forma e cor juntas, redundantes:** amigo = retângulo azul, hostil = losango vermelho, neutro = quadrado verde, desconhecido = trevo amarelo. | [Wikipedia: NATO Joint Military Symbology](https://en.wikipedia.org/wiki/NATO_Joint_Military_Symbology) |
| Sem cor | O símbolo vale em tela monocromática ("tela vermelha pra preservar a visão noturna"), e o preenchimento é opcional. | idem |
| Dimensão | **A abertura da moldura:** aberta embaixo = ar e espaço, aberta em cima = submarino, fechada = terra e superfície. | idem |
| Função | Um glifo dentro: infantaria = X cruzado, blindado = a lagarta (oval), artilharia = a bala de canhão (ponto), sanitário = a cruz, manutenção = a chave, transmissões = o raio, suprimento = a linha da estrada, reconhecimento = a correia do sabre da cavalaria (a diagonal), defesa antiaérea = a cúpula. | idem |
| Escalão | Fica acima da moldura: brigada = X, divisão = XX, com um X por estrela do general. | idem |
| Modificadores | Cada um tem um lugar fixo em volta da moldura. | idem |

---

## 2. Os princípios publicados

### 2.1 Cor nunca sozinha
- Xbox (XAG 103): "a cor sozinha nunca deve representar informação". O que se diz por cor precisa de mais um sinal: forma, padrão, ícone ou texto.
  O bloqueado não é só cinza: leva cadeado (os exemplos são Sea of Thieves e Forza). [XAG 103](https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/103)
- O mesmo vale em [Game Accessibility Guidelines](http://gameaccessibilityguidelines.com/ensure-no-essential-information-is-conveyed-by-a-colour-alone).
- Microsoft, ícones: "não dependa só da cor; use forma e metáfora". [App icon design](https://learn.microsoft.com/en-us/windows/apps/design/iconography/app-icon-design)
- A APP-6 é o exemplo real (§1.7). O WoW (§1.5) é o contrário: o tipo do debuff é só cor.

### 2.2 Uma diferença salta aos olhos; duas juntas, não
- Treisman e Gelade (1980): o alvo que difere numa característica só (cor **ou** forma) é achado em paralelo. O alvo que precisa de duas juntas
  (vermelho **e** vertical) pede busca item por item: ~40 ms × ~92 ms por item.
  [artigo](https://www.cs.princeton.edu/courses/archive/spring08/cos598B/Readings/TreismanGelade1980.pdf),
  [resumo](https://en.wikipedia.org/wiki/Feature_integration_theory)
- Estudos depois (Guided Search) mostram busca de conjunção mais rápida que isso. A regra prática continua: o 1º sinal tem de ser **uma**
  característica só. [idem](https://en.wikipedia.org/wiki/Feature_integration_theory)

### 2.3 Quantas categorias o olho separa
- Em gráficos de categorias, a precisão cai a partir de **6 categorias** (o limite da subitização), e a cor resiste melhor que a forma.
  [Wang et al. 2025](https://arxiv.org/html/2508.17460v1)
- Na prática se usam de 5 a 8 cores, conforme a tela e quanto erro se aceita. n/c se o número é de Christ (1975).
  [Christ 1975](https://doi.org/10.1177/001872087501700602)
- Miller (1956): o julgamento absoluto de um estímulo de uma dimensão fica perto de 7 ± 2. [DOI](https://doi.org/10.1037/h0043158)
- **Leitura minha:** 6 tipos num sinal só é o limite. Abaixo de 4 de relance é seguro; o resto vai pra 2ª leitura.

### 2.4 A hierarquia das três leituras
- Zach Gage (GDC 2018): 1ª leitura = o que se entende na hora; 2ª = o importante que não é imediato; 3ª = o detalhe pra quem já se interessou.
  Lotar a tela de ícones, números e marcas pequenas impede o jogador de entender. [Gigazine sobre a palestra](https://gigazine.net/gsc_news/en/20180501-game-design-subway-legibility)
- **No nosso caso (leitura minha):** 1ª = o tipo, 2ª = qual poder (o assunto), 3ª = o nível e o estado.

### 2.5 Ler pequeno
| Regra | Fonte |
|---|---|
| Um elemento só, de formas simples; no máximo 2 metáforas por ícone | [Microsoft](https://learn.microsoft.com/en-us/windows/apps/design/iconography/app-icon-design) |
| Sem texto dentro do ícone | idem |
| A silhueta tem de ser distinta e legível pequena, com poucas formas e poucos cantos | idem |
| Detalhe só na camada da frente | idem |
| Luz ambiente de cima à esquerda; vista de frente (3/4 só se a forma pedir) | idem |
| Metade do ícone com contraste ≥ 3:1 contra o fundo | idem |
| Grade fixa (48×48 na Microsoft) pra todos terem o mesmo peso | idem |
| A silhueta se reconhece "de primeira"; testar o valor (claro e escuro) **antes** da cor, e conferir em cinza durante e depois | Valve, guia de arte do Dota 2 ([Steam](https://help.steampowered.com/faqs/view/0688-7692-4D5A-1935), [CG Channel](https://www.cgchannel.com/2012/06/valve-releases-useful-character-design-guide)) |
| "Contraste chama atenção; se tudo chama, nada chama." Planejar o valor da tela inteira | [Game Developer](https://www.gamedeveloper.com/design/how-to-reduce-visual-confusion-in-your-game) |

### 2.6 Linguagem de forma (círculo, ângulo, quadrado)
- Pessoas preferem objetos curvos a angulosos (Bar e Neta 2006), e a ponta pra baixo ativa o circuito de ameaça (Larson et al. 2009).
  [Bar e Neta](https://doi.org/10.1111/j.1467-9280.2006.01759.x), [Larson](https://doi.org/10.1162/jocn.2009.21111)
- O "ângulo = ameaça" é contestado: a preferência por curva não vem só da ameaça. [Palumbo et al. 2015](https://pmc.ncbi.nlm.nih.gov/articles/PMC4603793)
- **Leitura minha:** a forma pode reforçar (o golpe anguloso, o suporte arredondado), mas não sustenta sozinha um código de 6 tipos.

### 2.7 Consistência e convenção
- NN/g: são raros os ícones universais. O ícone tem de querer dizer a mesma coisa em todo lugar. Se você leva mais de 5 s pra achar o desenho
  de um conceito, ele não vai comunicar. [Icon Usability](https://www.nngroup.com/articles/icon-usability/)
- A NN/g pede rótulo de texto sempre. **Em jogo**, o jogador aprende pela repetição, e o tooltip do mouse cobre o resto. É a "reação ao mouse"
  da regra dele.

### 2.8 O que dá errado (com fonte)
| Erro | Onde |
|---|---|
| Duas categorias com o mesmo valor se confundem de relance | Magic, branco × artefato ([Card frame](https://mtg.fandom.com/wiki/Card_frame)) |
| Ícones parecidos demais; transparência que mistura com o HUD | CoH3 ([RetroStyle](https://retrostylegames.com/blog/analyzing-company-of-heroes-3-game-design/)) |
| Árvore de poderes "confusa, ou não distinta o bastante" | Halo Wars 2, beta ([Xbox Wire](https://news.xbox.com/en-us/?p=43371)) |
| Muitas marcas pequenas: o jogador não entende a regra | Gage, GDC 2018 ([Gigazine](https://gigazine.net/gsc_news/en/20180501-game-design-subway-legibility)) |
| Brilho permanente que pode querer dizer duas coisas | Riot ([Clarity](https://www.leagueoflegends.com/en-us/news/dev/clarity-in-league/)) |
| Cor como único sinal | WoW, debuff (§1.5); contra a XAG 103 |
| Sinal fraco demais pra ser notado | ZH, borda de 1 px (leitura minha) |
| Símbolo frio sem personalidade ("sinal de trânsito") | AoE IV ([fórum](https://forums.ageofempires.com/t/ui-icons-traffic-signs/134362)) |

---

## 3. Padrão de mercado × solução original

| Sinal | Quer dizer | Quem usa | Convenção ou original? |
|---|---|---|---|
| Relevo de botão; sem relevo e "afundado" | dá pra apertar; passivo | Warcraft III, Dota 2 | **Convenção** (de 2002 até hoje) |
| Borda dourada ou laranja animada | autocast ligado | WC3, Dota 2, WoW (triângulos), SC2 | **Convenção** |
| Relógio escuro que varre o ícone | recarga ou produção | ZH (código); comum no gênero | **Convenção** |
| Cinza com cadeado | bloqueado | XAG 103 (Sea of Thieves, Forza) | **Convenção** e acessibilidade |
| Letra da tecla num canto | atalho | Dota 2 | **Convenção** |
| Algarismo romano | nível I, II, III | RA3 (chip), ZH (arte por nível) | **Convenção** no C&C |
| Verde = pronto/buff, vermelho = debuff, azul = sem recurso | estado | Dota 2, LoL | **Convenção** |
| O mais forte num lugar separado | ultimate | LoL (o R), Dota 2 (o losango) | **Convenção** |
| Aba ou coluna própria | tipo pela posição | RA2, C&C3, Total War, CoH3 | **Convenção** de RTS |
| Cor por categoria em ícone pequeno | tipo de efeito | WoW, Civ VI (com marca-d'água) | Convenção, fraca sem 2º sinal |
| O desbloqueio é a imagem da própria unidade | desbloqueio | ZH | **Original e simples**, funcionou |
| Render diferente pra unidade e pra poder | poder × unidade | RA3 | **Original**, funcionou |
| Forma da janela da arte por tipo, o resto igual | tipo de carta | Hearthstone | **Original**, funcionou (em carta grande) |
| Só o mais importante muda a silhueta; o resto é glifo num lugar fixo | hierarquia | Hearthstone (Taunt × caveira, raio) | **Original**, funcionou |
| Glifo do suporte desenhado sobre o ícone da habilidade | modificador | Path of Exile | **Original** |
| A carta deitada pra um tipo novo | tipo raro | Magic (batalha) | **Original** |
| Forma e cor redundantes; abertura da moldura = dimensão | afiliação e dimensão | APP-6 | **Padrão real**, militar |

---

## 4. O que o nosso sistema tem de aguentar

- **46 px:** a borda útil tem 2 a 3 px. Uma marca cabe com ~12 px, no limite (leitura minha, pela grade de 48 px da Microsoft).
- **56×40 deitado:** forma redonda, hexagonal ou de escudo perde área ou deixa sobra. O que depende de retângulo ou de um lugar fixo se adapta melhor.
- **90 px:** cabe tudo; o risco é o detalhe virar ruído no 46.
- **Estados que já existem:** comprado (verde), dá pra comprar (dourado), bloqueado, recarga, pronto e nível I a III. O tipo não pode usar
  esses canais.
- **Por que a tentativa de 03/11 não se sustentou (leitura minha):**
  - 5 formas de moldura **e** 5 cores de selo pra dizer a mesma coisa. Dois códigos juntos fazem uma conjunção (§2.2), e a cor briga com o
    verde e o dourado do estado.
  - As formas não vinham de uso nem de convenção (chanfro = golpe, hexágono = desbloqueio). O jogador teria de decorar.
  - Selo no canto: é a marca pequena disputando lugar com o nível e com o estado (§2.8, Gage).
  - Os 4 estilos de arte respondiam a outra pergunta (o visual), misturada com a do tipo.
  - Formas redondas e de escudo perdem área no 56×40.

---

## 5. Três sistemas candidatos

Os três usam a mesma base de estado, tirada das convenções do §3: cadeado e cinza no bloqueado, relógio escuro na recarga, algarismo romano
num canto fixo, tecla no canto oposto e um brilho de pronto curto (não permanente). Nenhum usa a cor pro tipo.

### Sistema A: pelo jeito de usar (botão, placa, cameo)

**Como o tipo se lê**

| Tipo | 1ª leitura, de relance | 2ª leitura |
|---|---|---|
| Golpe | **botão em relevo** (claro em cima e à esquerda, escuro embaixo e à direita) com a tecla | o avião ou o navio atirando, com impacto |
| Olhos | botão em relevo | o drone, o satélite, o avião-radar, sem arma nem fogo |
| Tropas | botão em relevo | os paraquedistas, o cargueiro |
| Suporte | botão em relevo | a chave ou o escudo sobre o veículo |
| Bônus/passivo | **placa embutida**: sem relevo, sem tecla, "afundada no painel" | o assunto do bônus |
| Desbloqueio | **o cameo da própria unidade ou prédio**, o mesmo do menu de construir | qual unidade |

- **Por que funciona a 46 px:** o relevo é diferença de valor na borda; lê em cinza e sem cor. É uma característica só (relevo sim ou não),
  então salta aos olhos. O cameo em render 3D tem outro material que a pintura do poder. São 3 famílias, abaixo do limite de 4 a 6. No 56×40,
  o relevo acompanha qualquer retângulo.
- **De onde veio:** Warcraft III (BTN × PASBTN), Dota 2 (chanfro × "apertado pra dentro do HUD"), ZH (o Paladin com o mesmo cameo), RA3
  (render × pictograma).
- **O risco:**
  - Golpe, olhos, tropas e suporte só se separam pelo assunto (2ª leitura). O A-10 (golpe) e o AWACS (olhos) são aviões: precisa de uma regra
    de arte (olhos nunca com arma ou fogo; golpe sempre com o impacto).
  - O relevo é linguagem de "botão físico" e tem de casar com o HUD aprovado de cada facção.
  - O passivo chapado pode parecer bloqueado. O bloqueado precisa do cadeado; o passivo continua colorido.

### Sistema B: a marca militar num lugar fixo (glifo da APP-6)

**Como o tipo se lê**
- A moldura é igual pra todos, e a arte ocupa o ícone inteiro.
- Há **um único lugar fixo, recortado na própria moldura** (não um selo colado): no meio de baixo no 46 e no 90; na faixa da esquerda no 56×40.
- Ali vai um glifo **monocromático**, de traço igual pra todos, tirado da simbologia militar:

| Tipo | O glifo | De onde vem |
|---|---|---|
| Golpe | o ponto da artilharia (os fogos) | APP-6: a bala de canhão |
| Olhos | a diagonal do reconhecimento | APP-6: a correia do sabre da cavalaria |
| Tropas | o paraquedas | APP-6: o aerotransportado (desenho: n/c no texto lido) |
| Suporte | a chave (manutenção) ou a cruz (sanitário) | APP-6 |
| Desbloqueio | o retângulo de unidade amiga, vazio ("uma unidade nova") | APP-6: a moldura |
| Bônus/passivo | **nada**: o recorte fica liso | Dota 2: o passivo sem tecla |

- O nível pode seguir o escalão da APP-6 (X, XX, XXX, como as estrelas do general) ou o algarismo romano do RA3.
- **Por que funciona a 46 px:** os glifos da APP-6 foram feitos pra ler pequeno e sem cor. O lugar é sempre o mesmo, então o olho aprende onde
  olhar, como os glifos embaixo do retrato no Hearthstone. É forma, não cor, e não briga com o verde e o dourado. Distingue os 6 tipos, não só 3.
- **De onde veio:** APP-6 / MIL-STD-2525; Hearthstone (um glifo num lugar fixo, só a exceção mudando a silhueta); Path of Exile (o símbolo do
  suporte sobre o ícone).
- **O risco:**
  - É uma marca pequena, da família do selo reprovado. A diferença precisa aparecer: um lugar só, recortado, sem cor e com uma linguagem que
    existe.
  - No 46, ~12 px é o limite, e glifo de traço fino some. É 2ª leitura, não 1ª.
  - Pouca gente conhece a APP-6: aprende-se no uso e no tooltip.
  - O passivo sem marca pode parecer "esqueceram". O glifo do desbloqueio é o mais fraco dos seis.

### Sistema C: pelo meio da imagem (como a imagem foi feita)

**Como o tipo se lê**

| Tipo | O meio | Por quê |
|---|---|---|
| Golpe, tropas, suporte (vão ao mapa e agem) | **a cena pintada**, em cor cheia (o cameo do ZH) | ação |
| Olhos | **a imagem do sensor**: térmica ou visão noturna, monocromática | o poder é ver |
| Desbloqueio | **a planta técnica** da unidade ou do prédio: linhas claras em fundo escuro | o jogador ganha o projeto pra construir |
| Bônus/passivo | **a insígnia** (o distintivo bordado ou de metal) | uma marca permanente da tropa |

- **Por que funciona a 46 px:** o meio muda a distribuição de claro e escuro e a textura do ícone inteiro. A cena é de meios-tons; o sensor é
  um assunto claro no escuro, granulado; a planta é feita de linhas finas; a insígnia é uma forma sólida de borda grossa. É uma propriedade
  global, não um detalhe, e não acrescenta nada pra ler. No teste de 03/11, a silhueta e o emblema foram os que leram melhor a 64 px
  (`USA.md` §12). São 4 famílias.
- **De onde veio:** RA3 (render × pictograma); ZH (o desbloqueio é a imagem da unidade); Magic (a textura da moldura diz o tipo: o céu de Nyx,
  o padrão do veículo); o próprio teste dos 4 estilos.
- **O risco:**
  - Briga com a ideia do estilo por facção (USA = sensor, China = cartaz). O meio não pode dizer a facção e o tipo ao mesmo tempo; a facção
    teria de ir pra paleta e pro HUD.
  - 4 meios na mesma tela podem parecer colagem.
  - Golpe, tropas e suporte ainda dependem do assunto.
  - O sensor e a planta puxam pra cor (verde, azul): cada meio precisa de diferença de valor, não só de cor.
  - São 4 receitas no Atelier, por facção: o custo mais alto dos três.

---

## Fontes principais (as outras estão em cada linha)

- ZH pelo código e INI: [GeneralsGameCode](https://github.com/TheSuperHackers/GeneralsGameCode),
  [ControlBarScheme.ini](https://github.com/TheSuperHackers/GeneralsGamePatch/blob/main/Patch104pZH/GameFilesOriginalZH/Data/INI/ControlBarScheme.ini),
  [CommandButton.ini](https://github.com/TheSuperHackers/GeneralsGamePatch/blob/main/Patch104pZH/GameFilesOriginalZH/Data/INI/CommandButton.ini)
- C&C: [Sidebar](https://cnc.fandom.com/wiki/Sidebar), [Top Secret Protocols](https://cnc.fandom.com/wiki/Top_Secret_Protocols)
- Dota 2: [Abilities](https://dota2.fandom.com/wiki/Abilities), [HUD](https://dota2.fandom.com/wiki/Head-up_display)
- Hearthstone: [Minion card](https://hearthstone.wiki.gg/wiki/Minion_card); Magic: [Card frame](https://mtg.fandom.com/wiki/Card_frame)
- APP-6: [NATO Joint Military Symbology](https://en.wikipedia.org/wiki/NATO_Joint_Military_Symbology)
- Acessibilidade: [XAG 103](https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/103)
- Ícone pequeno: [Microsoft, app icon design](https://learn.microsoft.com/en-us/windows/apps/design/iconography/app-icon-design),
  [NN/g, Icon Usability](https://www.nngroup.com/articles/icon-usability/)
- Percepção: [Treisman e Gelade 1980](https://www.cs.princeton.edu/courses/archive/spring08/cos598B/Readings/TreismanGelade1980.pdf),
  [Wang et al. 2025](https://arxiv.org/html/2508.17460v1)
- Hierarquia: [Gage, GDC 2018](https://gigazine.net/gsc_news/en/20180501-game-design-subway-legibility);
  clareza: [Riot](https://www.leagueoflegends.com/en-us/news/dev/clarity-in-league/)
