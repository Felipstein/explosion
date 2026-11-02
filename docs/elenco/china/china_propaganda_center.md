# Propaganda Center (China, a voz do Estado)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o prédio de tecnologia da China: destrava o pesado e a artilharia |
| Custo e tempo | $2 000, 45 s |
| Vida e blindagem | 1 000, prédio (frágil pro preço) |
| Energia | −2 |
| Visão | 52,7 m |
| Lote | 14 × 24 m |
| Arma | nenhuma |
| Requer | War Factory |
| Destrava | Inferno Cannon e Overlord |

## 2. A ideia

**A torre de alto-falantes gigante ao lado do telão vermelho com a estrela.** A China vence também pela voz: o palanque de
discurso sobre a praça, o telão de propaganda com a estrela dourada e os raios de sol, e a torre com o cacho de cornetas que gira
devagar e fala pra base inteira.

- **A palavra de cima:** a FLOR DE CORNETAS (12 cornetas claras em 2 andares, vistas de cima) + o retângulo vermelho do telão.
- **Inspiração real:** os postes de alto-falante das praças e aldeias chinesas (o cacho de cornetas de lata virado pros quatro
  lados), os palanques de discurso e os cartazes de propaganda com a estrela e os raios.
- **Por que existe:** é a porta do fim de jogo da China. Caro, demorado e frágil: perder ele no meio da partida corta o Overlord.

## 3. Como funciona

- **No jogo:** $2 000, 45 s, −2 de energia, 1 000 de vida. Pré-requisito do Inferno Cannon e do Overlord (`requires` deles). Não
  produz nem atira. O cacho de cornetas gira (peça `_Speakers`, 3 rpm no `Units.json`).
- **O peso:** com 1 000 de vida é o prédio-chave mais frágil da China (o tanque derruba em ~33 s; 4 Rocket Buggies em pouco mais
  de 2 salvas). Defender é obrigatório.
- **Forte contra:** nada; ele é a chave.
- **Fraco contra:** raide, artilharia, Rocket Buggy de longe.
- **O contra dele:** o raide cirúrgico. **Ele é o contra de:** o fim de jogo sem o Overlord.

## 4. Em batalha

- **Temperamento: o orador.** Inflamado, incansável, nunca para de falar. O único prédio que tem opinião.
- **O ritmo:** a cada ~20 s, o anúncio; entre eles, a música marcial baixa.
- **O jogador bom** o constrói atrás da fábrica, coberto por 2 Gattling Cannons, e não o põe no raio do reator.
- **A IA** faz um só, depois da fábrica, com a 1ª onda fora ou a partir de 5 min, e com o preço dele + um tanque sobrando
  (`docs/ELENCO.md` §7.5).
- **Sozinho e ferido:** segue transmitindo até cair; o telão pisca e chia (ideia).

## 5. A vida

- **No jogo:** o cacho de cornetas gira a 3 rpm; o andaime de aço na obra; abaixo de 35%, o fogo nos 2 pontos e os buracos.
- **Ideia (parado):** os 3 refletores do telão acendem a estrela e os raios à noite; a baliza da agulha pisca; a antena de
  transmissão com a luz vermelha no topo; os 3 estandartes batem no vento.
- **Ideia (anunciando):** quando uma unidade pesada fica pronta ou quando a base é atacada, o cacho de cornetas para de girar e
  aponta pro lado do evento (o jogador lê de cima onde está acontecendo).
- **Ideia (sem energia):** o telão apaga, as cornetas param e o prédio fica mudo: o silêncio é o alarme.
- **Ideia (ferido):** o telão perde pedaços (a estrela fica pela metade); as cornetas tortas.

## 6. O visual

- **De cima:** o retângulo comprido do pódio, o telão vermelho grande virado pra frente, a torre redonda na ponta com a flor de
  cornetas claras girando.
- **Assinatura:** a torre de alto-falantes (a China já mostra cornetas em todos os prédios; aqui é a maior e a que gira).
- **Cores:** concreto escuro oliva, telha verde; o vermelho do jogador no telão, no guarda-corpo, nos estandartes e na estrela; o
  dourado na moldura e nos raios.
- **Nunca:** retrato de líder real, texto legível em chinês real com lema político real (inventar o lema), mais de 4 estandartes.

## 7. Tiro, projétil, impacto, destruição

- **Impacto (ideia):** a bala nas cornetas faz "pling" metálico e uma corneta entorta; o canhão abre o telão (o pano vermelho
  rasga e balança).
- **A morte (no jogo):** a morte de prédio (a grande no meio, 3 menores, o chão queimado, o entulho: $200 em 14 s pro Dozer).
- **A morte (ideia):** a torre tomba inteira de lado, as cornetas espalham pelo chão; o telão cai pra frente e queima a estrela
  por último.

## 8. O som

- **Seleção (variantes):** o "dó-ré-mi" de três notas do sinal de anúncio; a microfonia curta (o assobio do alto-falante); a
  multidão ao longe aplaudindo; o motor da torre girando.
- **As falas** (inglês; o locutor da propaganda, voz de rádio estatal, enfática, sorrindo):
  - selecionado: "The people are united." / "Victory is our destiny." / "Broadcasting." / "Our voice reaches every soldier." /
    "Glory to the workers."
  - anúncios (ao longe, abafados, a cada ~20 s): "Production quotas exceeded." / "The enemy is weak and divided." / "Every
    soldier is a hero." / "Steel and courage."
  - sob ataque: "They want to silence us!" / "The voice must not fall!" / "Defend the broadcast!"
  - pronto: "Propaganda center online." / "The people will hear."
- **O corpo:** a música marcial baixa e metálica (o som de corneta de lata, cortado nos graves); o eco do anúncio batendo nos
  prédios da base; o motor das cornetas.
- **A construção:** os andaimes, o rebite, a corneta sendo testada ("um, dois, um, dois", com microfonia); no fim, o hino curto
  na corneta de lata.
- **A morte:** os estouros; a microfonia que sobe e corta; a torre caindo com o metal; a música para no meio da frase.

## 9. Ideias pra depois

- **A transmissão:** as unidades da China a até 50 m da torre recarregam 10% mais rápido e se curam 1% por segundo (a ideia do
  ZH, mais legível: as cornetas apontam pra elas).
- **A contra-propaganda:** um poder da China que manda o alto-falante pro rádio inimigo: as unidades inimigas num raio perdem a
  ordem por 3 s (param e olham em volta). Só com o Propaganda vivo.
- **O telão que mostra a guerra:** o telão passa, de verdade, a imagem da batalha (a câmera da última unidade da China que matou
  algo), em baixa resolução e vermelha. De cima não se lê; na câmera deitada vira o momento especial.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#china_propaganda_center` (feito no fim, com o modelo provisório como anexo de forma).
