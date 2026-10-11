# Produção 3D: a ficha (08/10/2026)

A regra única pra todo modelo do jogo (unidade, prédio de facção, construção civil),
feito à mão no Blender. Sem ela cada peça sai numa escala e nada encaixa. Os números
vêm do jogo, não de chute: a câmera (`Game/Config/DefaultGame.ini`), a grade da
simulação (`Game/Config/RTS/Maps.json`, `cell_m` = 4) e o tanque USA (7,9 m).

O conceito vem antes do modelo: GPT cria, Gemini faz a ficha com 5 vistas, sempre com a
**vista de cima** (`docs/CONCEITO_UNIDADES.md`, `docs/CONCEITO_CONSTRUCOES.md`).

---

## 1. Escala e eixos

- **1 unidade do Blender = 1 metro** (Scene → Units: Metric, Unit Scale 1.0).
- **Chão em Z = 0, Z pra cima.** O pivô (origem do objeto) fica **no chão, no centro da planta**.
- **A frente olha pra −Y do Blender**: é o que aparece na vista Front (tecla 1 do teclado
  numérico). Porta da fábrica, frente da casa, cano do tanque: tudo pra lá.
- **Referência de escala sempre na cena:** o tanque USA (7,9 m com o cano) e um humano de
  1,8 m. O modelo-base `captures/art3d/RTS_Base.blend` já vem com os dois (seção 9).

## 2. A grade: a planta encaixa em quadrados de 4 m

A simulação enxerga o mapa em quadrados de **4 × 4 m**. Prédio ocupa quadrados inteiros, então
**a planta (o que encosta no chão) tem medidas múltiplas de 4 m**: 8 × 8, 8 × 12, 24 × 28...
Beiral, toldo e antena podem passar um pouco da planta, mas a parede não.

| Coisa | Planta | Em quadrados |
|---|---|---|
| Casa civil pequena | 8 × 8 m | 2 × 2 |
| Casa civil média | 8 × 12 m | 2 × 3 |
| War Factory | ~24–32 m de largura (3–4 tanques, a fórmula do Zero Hour) | 6–8 de largura |

## 3. Altura: o prédio não pode esconder as unidades

A câmera do jogo tem 37,5° de inclinação. Tudo que tem altura `h` esconde uma faixa de
`h ÷ tan 37,5° = 1,3 × h` atrás dele:

| Altura | Esconde atrás |
|---|---|
| 4 m (casa de 1 andar) | 5,2 m (menos que um tanque) |
| 8 m | 10,4 m |
| 12 m (teto da fábrica) | 15,6 m (2 tanques) |

**Regra:** casa civil até ~4,5 m; volume principal de prédio de facção até ~12 m. Peça alta e
**fina** (chaminé, antena, domo pequeno, mastro) pode passar, porque esconde pouca área.

## 4. Detalhe e textura (medido pela câmera)

O mais perto que o jogador chega (zoom perto, borda de baixo da tela): **51 px por metro em
1080p e 103 px/m em 4K.** A câmera cinematográfica (deitada) fica mais longe que isso.

- **Densidade de textura: 128 px por metro** (uma textura de 1024 cobre 8 m).
  **256 px/m** só em peça pequena que o jogador olha de perto (frente de unidade, porta).
  Mais que isso é memória jogada fora: a placa alvo tem 6 GB.
- **Parede grande = material que se repete** (reboco, adobe, concreto, chapa), com sujeira e
  desgaste por cima (pintura de vértice ou uma máscara pequena). Textura única só no que é único.
- **Detalhe miúdo que não aparece a 50 px/m vira textura, não geometria.** Parafuso, rejunte,
  rachadura fina: na normal. Degrau, janela, viga, cano: na malha.
- **Triângulos (com Nanite, o custo é a memória, não o desenho):** casa civil 5–20 mil;
  War Factory 100–300 mil; tanque: casco e torre 100–150 mil, cano ~20 mil
  (`docs/CONCEITO_UNIDADES.md` §11).
- **Texturas por modelo:** um conjunto de 2048 no máximo (cor + normal + ORM ≈ 13 MB com mips);
  prédio civil pequeno: 1024.

## 5. Materiais e texturas (padrão da Unreal)

- **PBR metal/rugosidade**, três texturas por conjunto:
  - `T_<Nome>_BC`: cor (sRGB);
  - `T_<Nome>_N`: normal no **padrão DirectX** (o Blender assa no padrão OpenGL: inverter o
    canal verde ao exportar ou marcar "Flip Green Channel" na importação);
  - `T_<Nome>_ORM`: R = oclusão, G = rugosidade, B = metal (linear, sem sRGB).
- **Cor do jogador:** as faces que levam a cor do jogador ganham **um material separado
  chamado `PlayerColor`** (o jogo troca a cor dele). Regras de onde ela vai:
  - prédio: a cor forte do prédio É a cor do jogador (azul USA, vermelho China, verde GLA);
  - unidade USA/China: detalhes pequenos espalhados seguindo as placas + uma lona; nunca
    pintura grande (vira brinquedo);
  - GLA: rabiscos, bandeiras e panos;
  - civil: nenhuma (é neutro).

## 6. Estados de dano

Num RTS o prédio precisa mostrar que está apanhando. Do jeito barato e bonito
(`docs/CONCEITO_UNIDADES.md` §11):

1. **Inteiro:** a malha principal.
2. **Danificado:** a mesma malha; o material mistura marcas de tiro e queimado conforme a
   vida cai, mais o fogo e a fumaça dos efeitos. Não precisa de outra malha.
3. **Destruído:** uma malha separada `_Rubble` (escombro), com **a mesma planta e o mesmo
   pivô**, baixa (até ~1,5 m), pra unidade não sumir atrás dela.

Peça que se mexe é objeto separado: torre, cano, porta da fábrica, ventilador.

**Como o prédio cai no jogo (plano de 08/10/2026).** A simulação do jogo (exata nos dois PCs)
decide a vida de cada parte e a hora da queda; a Unreal só mostra. Como o dano é só visual, a
física pode rodar ao vivo e sair um pouco diferente em cada PC sem quebrar o multiplayer.
**Agora (decidido 08/10):** o mais simples e leve. Dano no material (marcas, fuligem, rachaduras)
+ fogo e fumaça; ao zerar, toca um desabamento gravado (2–3 versões por casa) com a nuvem de
poeira; fica o `_Rubble`. **Projeto futuro:** os itens 1 e 2 abaixo (quebra pelo lado do tiro e
queda na direção dele). O modelo já sai pronto pro futuro (sólido fechado com espessura), pra não remodelar.

1. **Quebra onde o tiro bate** (Battlefield, Chaos da Unreal): a Unreal parte a casa inteira em
   muitos pedaços (um quebra-cabeça que não segue as paredes) e a explosão solta os pedaços em volta
   do ponto atingido. Tiro forte no canto leva o canto (pedaço de duas paredes e da laje); no meio da
   parede, abre o buraco ali. Separar a casa por parede NÃO serve (o Felipe apontou, 08/10).
2. **O desabamento cai pro lado do tiro:** perto da câmera e dentro do orçamento, física ao vivo a
   partir do ponto e na direção do último tiro; fora do orçamento (muitas juntas, longe), uma de 8
   quedas gravadas, a da direção mais próxima. Orçamento como o das explosões do jogo; quantas ao
   vivo a 1660 aguenta: medir na primeira casa.
3. Depois fica o `_Rubble`. O entulho é só visual: nunca bloqueia unidade por acaso (a passagem
   depois da queda é regra da simulação).

Pra isso o modelo precisa (e é mais simples que separar paredes):
- **a casa como um sólido fechado** (sem buraco na malha, sem face solta), com **espessura de
  verdade** nas paredes e na laje; pode ser um objeto só;
- a porta e o que for de outro material, objetos à parte;
- o miolo (tijolo de barro, `M_Civ_Brick`) não se pinta no Blender: as faces de dentro nascem na
  Unreal quando ela fratura, e o material delas se escolhe lá (Fracture Mode);
- o `_Rubble` na mesma planta e no mesmo pivô.

## 7. Nomes e pastas

- Malhas: `SM_<Facção>_<Nome>` (+ `_Rubble`); facções: `USA`, `CHN`, `GLA`, `Civ`.
  Ex.: `SM_Civ_AdobeHouse_A`, `SM_Civ_AdobeHouse_A_Rubble`, `SM_USA_WarFactory`.
- Peça móvel: `SM_USA_WarFactory_Door`, `SM_USA_MBT_Turret`.
- Texturas: `T_<Nome>_BC/_N/_ORM`; materiais `M_`, instâncias `MI_`.
- **Arquivos-fonte (.blend, texturas em 4K de trabalho):** `captures/art3d/<Nome>/` (fora do
  git) até decidir o armazenamento de arquivo grande (Git LFS, pendente em `docs/JOGO.md`).
- **Exportação:** `.glb` (glTF binário), "Apply Modifiers" ligado, transformações aplicadas
  (Ctrl+A → All Transforms) antes de exportar. É o caminho que já funcionou com o tanque
  (`BlastLab/Tools/blender_export_unit.py`).

## 8. Conferência antes de entregar

1. **Vista de cima** lado a lado com a ficha do conceito: é a que a câmera mais vê.
2. **Câmera do jogo** do modelo-base (Numpad 0) com o tanque do lado: lê de cima? a silhueta é a da ficha?
3. Planta em múltiplos de 4 m, pivô no chão e no centro, frente pra −Y.
4. Altura dentro da seção 3.
5. Triângulos e tamanho de textura dentro da seção 4 (Blender: Overlays → Statistics).
6. Material `PlayerColor` onde a regra manda; estado `_Rubble` feito.
7. Nomes da seção 7.

## 9. O modelo-base do Blender

`captures/art3d/RTS_Base.blend` (fora do git: leva o tanque, ~20 MB), gerado por
`art/tools/make_template.py` (mude o script, não o arquivo):

```
"C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python art/tools/make_template.py
```

Abre com:

- a grade de 4 m no chão (as linhas são a grade da simulação);
- o tanque USA (7,9 m) e um humano de 1,8 m pra escala;
- a câmera do jogo (37,5°, campo de 50°, 61,1 m de altura; Numpad 0) e a de zoom perto (31,6 m);
- o sol do deserto do jogo (20° de altura);
- uma caixa de planta 8 × 8 × 4 m pro primeiro modelo;
- a coleção `Modelo`, vazia: é onde vai o que você modela (a `Referencia` não se exporta).

---

## 10. O primeiro modelo: a casa do vilarejo do deserto

Pra aprender: é feita de caixas e é a cara do deserto do primeiro mapa. **O alvo é a casa
da nossa cena do jogo** (`docs/concept/scenes/desert_golden_hour/best/day_game_cam.webp`), não uma casa nova: a
ficha sai do Gemini/GPT com a cena anexada ("a casa da direita, com a caixinha no teto").
Uma rodada com casa inventada (vigas, reboco descascado, escada externa, caixa-d'água)
saiu bonita sozinha, mas não encaixava na cena (08/10).

O que faz as casas da cena encaixarem:
- caixas grossas e simples, cantos quase retos;
- teto plano com uma borda baixa e quase nada em cima (no máximo uma caixinha);
- reboco cinza-areia empoeirado e uniforme, mais sujo perto do chão, sem tijolo à mostra;
- aberturas pequenas e escuras (janela e porta afundadas na parede);
- casas ligadas por muros baixos de quintal, com portão.

Medidas: **planta 8 × 8 m** (2 × 2 quadrados), **~4 m de altura** (esconde 5,2 m atrás).
Escombro (`_Rubble`): paredes quebradas de 0,5 a 1,5 m e monte de blocos.
Depois, pelo mesmo caminho: a casa de dois andares, a de varanda com toldo de madeira e o
muro de quintal com portão.
