# Gas Power Plant (Rússia, a usina a gás)

## 1. Ficha rápida

| | |
|---|---|
| Papel | a usina: a energia da base Rússia, no meio do caminho entre a USA e a China, com a bola de fogo do gás quando cai |
| Nome na tela | Gas Power Plant (a chave é `russia_power_plant`) |
| Custo e tempo | $900, 10 s |
| Vida e blindagem | 1 500, prédio |
| Energia | **+7** (a da USA dá +5 por $800; o reator da China, +10 por $1 000) |
| Visão | 52,7 m |
| Lote | 14 × 16 m |
| Arma | nenhuma em vida; **na morte, 250 explosivo** em volta (o `death_blast`) |
| Requer | nada |
| Produz | nada; destrava o Supply Depot e o Pantsir Site |
| Estado | nos dados (`Units.json`) e no modelo provisório; fora do menu até os modelos da Rússia serem importados |

## 2. A ideia

**A chaminé listrada sobre os pavilhões das turbinas.** A central a gás de bloco: os 2 pavilhões das turbinas de telhado verde, o
duto grosso dos gases quentes correndo por cima dos telhados até a chaminé alta de faixas vermelhas e brancas, o cano de gás
amarelo andando por cima do chão com a lira de dilatação e as válvulas de volante vermelho, os transformadores no pátio e a casa
de controle com a bandeira.

- **A palavra de cima:** a CHAMINÉ LISTRADA (o círculo vermelho e branco com a boca escura) e os 2 telhados verdes atravessados
  pelo duto.
- **Inspiração real:** as centrais de turbina a gás de bloco russas (as GTU-TETs: os pavilhões de painel, a chaminé com as faixas
  da sinalização de aviação) e o gás que na Rússia corre em cano aéreo pintado de amarelo, com o "П" da lira por cima das
  passagens.
- **A física da morte, conferida:** uma usina a gás não vira bomba. O que acontece é o cano romper e o gás (metano, mais leve que o
  ar) pegar fogo: uma bola de fogo curta que sobe rápido, e depois a chama em jato saindo do cano rompido, de pé, rugindo até a
  válvula fechar ou o gás acabar. Nada de cogumelo, nada de nuvem branca de vapor (essa é a do reator da China).
- **Por que existe:** a energia da Rússia fica no meio: mais por dólar que a USA (0,78 por $100 contra 0,63), menos risco que a
  China (250 até 18 m contra 400 até 26 m).

## 3. Como funciona

- **No jogo:** +7 de energia; 1 500 de vida. Quando cai **pronta**, solta o dano de morte: **250 explosivo** até 9 m da borda do
  lote, caindo em linha reta até **15% (37)** a 18 m; em **todo mundo** (aliado, inimigo, neutro), sem XP pra ninguém. A obra
  inacabada e a usina vendida **não** explodem (`World.cpp`, `ApplyDeathBlast`).
- **O que 250 faz:** mata qualquer soldado, o TOS-1A (220), a Technical, o Rocket Buggy e o Inferno; tira 45% do T-90M, 25% do
  T-14, 52% do tanque USA, 78% do Pantsir-S1; tira 5% de um Command Center, 17% de outra usina (não estoura em cadeia), 25% do
  quartel e do Pantsir fixo.
- **A conta da energia:** o depósito (−1), a fábrica (−1), o Instituto (−2) e um Pantsir fixo (−3) somam −7: uma usina paga a base
  mínima com uma defesa; a segunda defesa já pede a segunda usina. Sem energia, a produção cai pra 50–80% e o Pantsir fixo para
  (`needs_power`).
- **Forte contra:** nada em vida. Morta, castiga quem atirou colado nela.
- **Fraca contra:** tudo que mira prédio: o T-90M a derruba em ~50 s, o T-14 em ~30 s, o Overlord em ~22 s.
- **O contra dela:** o raide de tanque ou de artilharia na retaguarda. **Ela é o contra de:** o raide preguiçoso que para do lado
  dela pra atirar.

## 4. Em batalha

- **Temperamento: o foguista.** Trabalhador, quente, barulhento, inofensivo até o cano romper.
- **O ritmo:** o assobio contínuo das turbinas, sem pausa; quando ferida, o chiado do gás vazando por baixo (ideia).
- **O jogador bom** a põe atrás da base, fora dos 18 m do Shtab, da fábrica e do depósito (dá pra encostar mais que o reator da
  China, 26 m), com o Pantsir fixo perto o bastante pra cobrir, longe o bastante pra não levar os 250.
- **A IA** a constrói primeiro (a lista: usina, depósito, fábrica, defesa) e a conta de energia dela pede a próxima quando precisa.
- **Sozinha e ferida:** abaixo de 35%, o fogo nos 3 pontos; é a hora de afastar as unidades.

## 5. A vida

- **No jogo:** o andaime de aço na obra (peça `_Scaffold`); a bandeira lisa na cor do jogador no teto da casa de controle
  (`flags`); abaixo de 35%, o fogo nos 3 pontos (`fire_points_m`). Nada gira na malha.
- **Ideia (parado):** a boca da chaminé solta o ar quente tremido (a distorção do calor, quase sem fumaça: o gás queima limpo) e, em
  mapa frio, um fio de vapor branco; as luzes de obstáculo vermelhas das 2 plataformas da chaminé piscam juntas (o slot `Beacon` já
  está no modelo); os exaustores redondos dos telhados giram; a janela acesa da casa de controle.
- **Ideia (ferida):** acima de 50%, uma válvula do cano amarelo chia e solta a chama azulada pequena; abaixo de 35%, a fumaça preta
  sai pela porta de enrolar do pavilhão (a turbina queimando o óleo) e a sirene de alerta.
- **Ideia (vendida ou na obra):** a chaminé fria, sem o tremido.

## 6. O visual

- **De cima:** a chaminé listrada no canto de trás (a coisa mais alta da base: 18 m), os 2 pavilhões de telhado verde de duas
  águas, o duto grosso subindo dos pavilhões e entrando na chaminé, o cano amarelo pelo chão, os 2 transformadores
  azul-acinzentados e o pórtico de saída treliçado, a casa de controle pequena com a janela acesa.
- **Assinatura:** a chaminé listrada (proibida nas outras: é da Rússia).
- **Cores:** o painel cinza, a chapa verde dos telhados; o vermelho e branco da chaminé (a sinalização de aviação, fixa); o amarelo
  do cano de gás; o vermelho dos volantes das válvulas; a cor do jogador na bandeira da casa de controle.
- **Nunca:** torre de resfriamento e domo (são do reator da China); fumaça preta saindo da chaminé em vida (o gás queima limpo);
  cogumelo; Z ou V.

## 7. Tiro, projétil, impacto, destruição

- **Impacto (ideia):** a bala no cano amarelo abre o furo e o jato de gás chiando (sem fogo; o próximo tiro acende); o canhão lasca
  o concreto de uma faixa da chaminé.
- **A morte (no jogo):** o dano de 250 no raio + a morte de prédio (a explosão grande no meio e 3 menores em ~1,5 s, o chão
  queimado, o entulho: $90 em ~8,5 s pro construtor).
- **A morte (ideia, em 3 tempos, conferida pela física):**
  1. o cano rompe e o gás pega: a bola de fogo laranja-amarela que sobe rápido e se abre; a onda levanta a poeira em anel até os
     18 m (o raio do dano, pro jogador ver de cima o tamanho do perigo);
  2. a chama em jato, de pé, saindo do cano rompido, rugindo por ~8 s (volume, acendendo tudo em volta: vive na luz do mundo, faz
     sombra e acende as unidades perto);
  3. a chaminé racha no pé e tomba pro lado de fora, as faixas vermelhas e brancas no chão (legível de cima); os pavilhões ficam
     abertos, queimando baixo.

## 8. O som

- **Seleção (variantes):** o assobio agudo da turbina a gás subindo de tom; o volante da válvula girando com o rangido; o chiado
  curto do gás; o bip do painel da casa de controle.
- **As falas** (inglês, com o sotaque russo leve; o engenheiro de turno, voz cansada, calma, técnica):
  - selecionado: "Gas plant online." / "Turbines steady." / "Pressure nominal." / "Power to the base." / "Mind the gas line."
  - ferida: "Gas leak!" / "Pressure dropping!" / "Close the valves!" / "Get clear of the line!"
  - pronta: "Gas plant running." / "Power is flowing."
- **O corpo:** o assobio contínuo e metálico das turbinas (agudo: o contrário do "uuum" grave do reator da China e do zumbido limpo
  da fusão da USA); o ronco do duto; o zumbido do transformador.
- **A construção:** os painéis assentados, a chaminé subindo em anéis (a fôrma, o vibrador do concreto), o cano soldado; no fim, a
  ignição (o "fuuump" do gás) e o assobio das turbinas subindo até o tom de trabalho.
- **A morte:** o "WHOOMP" da bola de fogo, o rugido contínuo da chama em jato (como um maçarico gigante), o concreto da chaminé
  rachando e caindo, depois o silêncio com o chiado do gás.
- **A cara da Rússia:** o metal grave, o assobio da máquina, a voz baixa e seca.

## 9. Ideias pra depois

- **O fecho do gás:** um botão na usina ferida: ela para de dar energia na hora e o dano de morte cai de 250 pra 50 enquanto
  estiver fechada (sem gás no cano, só a chama pequena). A escolha no meio do ataque: a base no escuro ou a bomba armada.
- **O gasoduto do mapa:** nos tipos de mapa com gasoduto (o cano amarelo do WorldForge), a usina construída encostada nele dá +2; o
  inimigo que corta o gasoduto apaga a Rússia (o campo que muda).
- **A cicatriz:** onde a usina explodiu, o chão fica preto e o cano retorcido fica no entulho pelo resto da partida (o chão que
  lembra, `docs/VISAO_DO_JOGO.md`).

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#russia_power_plant` (feito no fim, com o modelo provisório como anexo de forma).
