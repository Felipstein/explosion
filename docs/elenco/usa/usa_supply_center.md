# Supply Center (USA, a economia)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o centro de suprimentos: onde o caminhão e o Chinook entregam as caixas que viram dinheiro |
| Custo e tempo | $1 500, 10 s |
| Vida e blindagem | 2 000, prédio |
| Energia | −1 |
| Visão | 52,7 m |
| Lote | 18,6 × 19,0 m |
| Requer | Cold Fusion Reactor |
| Produz | Supply Truck ($600) e Chinook ($1 400, pede também o Airfield) |
| De graça | 1 Supply Truck quando fica pronto |
| Libera | a War Factory |
| Conta pra vitória | sim |

## 2. A ideia

**A doca.** Um armazém baixo com a baía de descarga na frente, a ponte rolante amarela por cima da vaga do caminhão, o "H" de
pouso no teto e as pilhas de caixas recebidas debaixo da lona azul. Tudo nele diz "chega carga aqui".

- **A palavra de cima:** o "H" no teto (o heliponto do Chinook) e a ponte rolante amarela sobre a baía.
- **Inspiração real:** os centros de distribuição logística do exército (o armazém de lona e aço, o pátio de paletes, a
  empilhadeira, o heliponto de reabastecimento). No ZH, o Supply Center da USA tinha o Chinook pousando; aqui o coletor de
  partida é o caminhão (decisão de 10/10) e o Chinook é o coletor melhor, mais tarde.
- **Por que existe:** a USA paga caro por tudo. Sem centro, não há renda; sem a renda segura, não há Airfield nem Paladin.

## 3. Como funciona

- **No jogo:** o caminhão leva 4 caixas de $75 ($300) por viagem: 1 s por caixa no depósito do mapa e 0,4 s por caixa no centro.
  Rende ~11 $/s com o depósito a 100 m e ~8 $/s a 150 m. O Chinook leva 8 caixas ($600), 0,6 s por caixa, entrega em 2,9 s
  pousado no "H" do teto: ~22 $/s a 100 m.
- **O depósito procura** até 184 m em linha reta (o `SupplyWarehouseScanDistance` do ZH).
- **A IA** quer 3 caminhões por centro e expande cedo: com 25% das caixas de casa, põe um centro novo no depósito do meio.
- **Forte contra:** nada (não atira). **Fraco contra:** o raide (o Apache e a Technical caçam o caminhão no caminho, não o
  prédio); o ataque de mísseis.
- **Vender:** $750 em 6 s.

## 4. Em batalha

- **Temperamento: o intendente.** Prático, contador, sempre ocupado; fala em números. Não se assusta com tiro, se assusta com
  caminhão parado.
- **O ritmo:** o vaivém. Caminhão chega, a ponte rolante desce, as caixas somem, o caminhão sai. Um compasso de ~20 s por
  caminhão.
- **O jogador bom** põe o centro colado no depósito (o caminho curto é a renda), com uma Patriot cobrindo a rota.
- **Sozinho e ferido:** segue recebendo até cair; a renda não cai com a vida.

## 5. A vida

- **Parado:** as luzes da baía acesas; a ponte rolante parada em cima da vaga; a lona azul das pilhas batendo no vento; a
  biruta do heliponto.
- **Trabalhando:** a ponte rolante corre até a caçamba, desce o gancho, sobe com a caixa e larga na pilha; a sirene âmbar da
  baía gira enquanto o caminhão está encostado. O Chinook pousa no teto, abre a rampa (39°) e a poeira do rotor varre o teto.
- **Na obra:** o galpão sobe no andaime, a ponte rolante é a última peça (desce pendurada no guindaste).
- **Ferido:** com 70% fumaça num canto do telhado; com 30% as pilhas de caixas pegam fogo e a lona queima.
- **Sem energia:** só as luzes caem pra metade; a ponte segue trabalhando, porque a descarga não depende de energia nos dados (o
  desenho não pode mentir sobre a simulação).

## 6. O visual

- **De cima:** um quadrado quase perfeito, baixo; o "H" claro num canto do teto; a ponte rolante amarela atravessando a baía da
  frente; as pilhas azuis atrás.
- **Assinatura:** o "H" com a ponte rolante. Os dois pedem o movimento (o Chinook pousando, a ponte correndo).
- **Cores:** metal cinza-claro, concreto; o amarelo da ponte e das faixas de perigo da baía; a cor do jogador nas lonas das
  pilhas e na faixa da parede.
- **Nunca:** pilha de caixas gigante (é do depósito do mapa e da GLA); contêiner de navio empilhado.

## 7. Tiro, projétil, impacto, destruição

- **Impacto:** o canhão fura o painel e as caixas da pilha voam em pedaços de madeira; o fuzil faz faísca na chapa.
- **A morte:** a ponte rolante despenca na baía, o teto do galpão cede do lado do impacto, as pilhas queimam com a lona; o "H"
  racha. Fogo baixo e espalhado (madeira e papelão), fumaça cinza clara, mais longa que a de um prédio de aço.
- **O que fica:** o entulho com as caixas queimadas; o Dozer recolhe 10% do custo ($150) em ~11,5 s.

## 8. O som

- **Seleção (variantes):** o rádio limpo com o bipe; o motor da ponte rolante; a corrente do gancho; a buzina curta da
  empilhadeira.
- **As falas** (o intendente; voz de meia-idade, prática, com o rádio de fundo):
  - selecionado: "Logistics, go ahead." / "Depot's open." / "What do you need moved?" / "Supply desk."
  - produzindo: "Truck's on its way." / "Another hauler rolling." / "Bird's spinning up." / "Chinook cleared for lift."
  - sob ataque: "They're hitting the depot!" / "Supply line under fire!" / "Taking hits on the dock!" / "Protect the cargo!"
  - pronto: "Supply center open for business." / "Dock is operational." / "Cargo station ready." / "Let's get paid."
- **O corpo:** o motor elétrico da ponte (o zumbido que sobe e desce com a corrida), a corrente, a caixa batendo na pilha (o
  "toc" oco da madeira), a sirene da baía, o "bip-bip" de ré do caminhão encostando.
- **A obra:** chapas de galpão, rebites, a ponte rolante descendo no guindaste por último (o rangido longo).
- **A morte:** o metal da ponte caindo (o maior som), a madeira estalando no fogo, o teto de chapa desabando em ondas.

## 9. Ideias pra depois

- **A pilha que conta o dinheiro:** as pilhas atrás do prédio crescem com as entregas dos últimos 60 s e encolhem quando o jogador
  gasta. O inimigo lê de longe se a base está rica (a USA não esconde, mostra).
- **O comboio escoltado:** caminhões que andam a até 10 m de um Stryker levam 30% menos dano (o comboio de verdade: a escolta
  fixa o atacante). Dá papel econômico ao Stryker.
- **O Chinook que desce a carga de longe:** no depósito longe da base, o Chinook paira e leva as 8 caixas no gancho, numa rede
  (a ficha aprovada mostra a carga pendurada); derrubado, a rede cai inteira no chão como a carga que cai.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#usa_supply_center` (feito no fim, com o modelo provisório como anexo de forma).
