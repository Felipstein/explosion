# Supply Pickup (GLA, a picape da carga)

## 1. Ficha rápida

| | |
|---|---|
| Papel | a coletora da GLA: leva as caixas do depósito ao Supply Stash |
| Custo e tempo | $400, 6 s (a 1ª vem de graça com o Stash) |
| Vida e blindagem | 200, tanque |
| Velocidade | 11 m/s (o coletor mais rápido), giro 120°/s, acelera e freia forte (63 m/s²) |
| Visão | 39,5 m |
| Carga | **3 caixas** de $75 = $225 por viagem (o caminhão leva 4) |
| Arma | nenhuma |
| Esmagador | nível 1: o soldado na frente dela morre |
| Requer | Supply Stash |

## 2. A ideia

**A picape velha carregada.** Não é veículo militar: é a picape civil de cabine simples, remendada com peças de outras, carregada
até o teto, as tábuas nas laterais da caçamba amarradas nas estacas, o estepe e os galões no bagageiro. A economia da GLA anda numa
lata-velha.

- **A palavra de cima:** a CAÇAMBA cheia de caixas + a seta verde pintada à mão no capô.
- **Inspiração real:** as picapes de cabine simples dos anos 80 que as guerrilhas e os mercados do deserto carregam até não caber
  (a Toyota Hilux é a mais famosa: a "guerra das Toyotas", no Chade de 1987, ficou com esse nome). Fictícia, não cópia; da mesma
  família da Technical aprovada (a picape 1).
- **Por que existe:** no ZH o Worker coletava; aqui a coleta fica com a picape (`docs/ELENCO.md` §7.4), e o Worker fica livre pra
  construir e sucatear. A economia da GLA fica a mais barata e a que mais sofre com raide.

## 3. Como funciona

- **No jogo:** 3 caixas (1 s cada no depósito, 0,4 s cada na entrega): $225 por viagem. Com o depósito a ~60 m, ~$900 por minuto.
  Mais rápida que o caminhão (11 contra 10,5 m/s), mais barata ($400 contra $600), mais frágil (200 contra 300).
- **A carga que cai (no jogo):** se ela morre carregada, as caixas ficam no chão numa pilha que qualquer coletor pega.
- **Esmaga:** nível 1: o soldado que fica no caminho morre (200%).
- **Forte contra:** nada; é economia.
- **Fraca contra:** tudo: uma Technical a mata em 16 s, um Gattling Tank em ~53 s (espanta), um RPG em 5 foguetes, um tanque em 4
  tiros.
- **O contra dela:** a Technical, o Stryker, o Rocket Buggy. **Ela é o contra de:** a GLA pobre.

## 4. Em batalha

- **Temperamento: o motorista doido.** Apressado, falador, sem medo nenhum (e sem juízo). Dirige rápido demais com carga demais.
- **O ritmo:** o motor gritando, a freada brusca, as 3 caixas jogadas, a arrancada cantando pneu.
- **O jogador bom** faz 4 ou 5 picapes por Stash (são baratas), põe o Stash colado no depósito e caça as pilhas da carga que cai pelo
  mapa (a GLA vive do que os outros deixam).
- **A IA** mantém as picapes; o inimigo da IA mira coletor primeiro.
- **Sozinha e ferida:** segue a rota; abaixo de 70%, a fumaça clara; abaixo de 35%, a chama.

## 5. A vida

- **No jogo:** as caixas na caçamba conforme a carga (`cargo`); as 4 rodas giram; a poeira e a marca dos pneus; os estados de dano;
  a morte de gasolina (`death_kind gasoline`).
- **Ideia (parado):** o motor velho em marcha lenta, tremendo a carroceria inteira; a bandeira no mastro do santo-antônio bate; o
  pano amarrado no retrovisor e na antena balançam.
- **Ideia (andando):** a carroceria sacode, as caixas pulam a cada buraco (uma pode cair: só desenho), a suspensão afunda com a carga;
  o escape solta fumaça azulada de motor gasto na arrancada.
- **Ideia (carregando):** a traseira abaixa a cada caixa; o motorista buzina impaciente.

## 6. O visual

- **De cima:** a picape de 5,3 m com a cabine quadrada, o bagageiro com o estepe e os galões, a caçamba de tábuas com as caixas, a
  seta verde no capô.
- **Assinatura:** a caçamba carregada demais.
- **Cores:** o amarelo-areia fosco com as duas listras verde-escuras gastas, a porta de outra cor (ferrugem), os remendos; o verde do
  jogador na bandeira, na seta do capô, no rabisco da porta, na faixa da tampa e nos panos.
- **Nunca:** arma; blindagem; pintura limpa ou branca.

## 7. Tiro, projétil, impacto, destruição

- **Impacto (ideia):** a bala fura a lataria fina (o "tunc" e o buraco com a tinta saltando), estoura um pneu (ela segue mancando,
  só desenho), fura um galão que vaza.
- **A morte (no jogo):** a morte de gasolina, em escala de picape (`death_scale` 0,4): a bola rápida e amarela, a carcaça queimando
  48 s; a pilha da carga que cai; a carcaça é sucata ($60 em ~6 s).
- **A morte (ideia):** os galões do bagageiro estouram primeiro (duas bolas de fogo pequenas e seguidas), a cabine pega fogo, as
  tábuas da caçamba voam e as caixas caem em volta formando a pilha.

## 8. O som

- **Seleção (variantes):** o motor de 4 cilindros velho acelerando com a válvula batendo; a buzina rouca e desafinada; a porta
  enferrujada batendo (duas vezes, porque não fecha na primeira); o rádio do painel com música chiada.
- **As falas** (inglês; o motorista, voz rápida, animada, meio louca, rindo à toa):
  - selecionado: "Pickup here!" / "Where's the loot?" / "She's old but she runs!" / "Load me up, brother!" / "Fast and full!"
  - andando: "Hold on to something!" / "Going, going!" / "Shortcut!" / "No brakes, no problem."
  - carregando: "Three crates, full!" / "Back to the stash!" / "That's all she can take!"
  - sob fogo: "They're shooting at my truck!" / "Not the tires, not the tires!" / "I'm just the driver!"
  - pronta: "Pickup ready to roll."
- **O corpo:** o motor velho (agudo, irregular, com o escape furado roncando); a carroceria batendo; as caixas pulando; o pneu careca
  cantando nas curvas.
- **A carga:** a caixa batendo na caçamba de tábua (mais oco que a caçamba de aço do caminhão da China).
- **A morte:** o estouro rápido da gasolina (mais agudo e curto que o do diesel), os galões estourando em seguida, a madeira
  queimando.
- **A cara da GLA:** o motor velho, o metal batendo, o rádio chiado.

## 9. Ideias pra depois

- **O sobrepeso:** o jogador pode mandar a picape levar 4 caixas (uma a mais, amarrada no teto): rende $300, mas anda a 8 m/s e tem
  50% de chance de perder a caixa de cima a cada salto (a caixa cai e vira pilha no meio do caminho).
- **A picape catadora:** a picape vazia que passa a até 10 m de uma carcaça de veículo leve pega uma "caixa de sucata" ($30) sem
  parar (a GLA catando tudo que acha no caminho).
- **A picape-bomba:** o jogador pode encher uma picape de explosivo ($300 a mais) e mandar contra o inimigo (a morte vira um
  estouro de 150 explosivo em 8 m). Tom sério: é tática de guerrilha real; o motorista salta antes (o soldado volta a pé).

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#gla_supply_pickup` (feito no fim, com o modelo provisório como anexo de forma).
