# A noite de 12 → 13/10 no jogo (sessão do jogo)

Pedido do Felipe antes de dormir (repassado pela sessão de arte): terminar o que estava pendente, decidir sozinho (ou
deixar alternativas), estudar como os grandes fazem, e chegar no jogo 100% igual aos gameshots aprovados. **Nada foi
commitado**: tudo espera o "pode commitar".

## Como ver

```
Game\Tools\game.ps1 view            (tela cheia, a partida de sempre)
Game\Tools\game.ps1 view -Scenario vila      (a luta da vila, ~40 s: chão, crateras, carcaças)
Game\Tools\game.ps1 view -Scenario carcaca   (a mesma luta com a câmera a 50 m, pra ver a carcaça)
Game\Tools\game.ps1 view -Scenario cratera_pan -Folder x -Record   (vídeo da câmera deslizando sobre as crateras)
```

Na partida, **F4** troca entre o HUD de hoje e o HUD novo da sessão de arte (ou `-NewHud` na linha de comando).

## O que mudou pra você

1. **O HUD novo** (o do HUDLab) dentro do jogo, pra você jogar com ele: os grupos, a fila, Stop, Attack Move, as teclas
   QWERTYU / ZXCVBNM, o botão direito tirando da fila e as dicas com preço e tempo. Poderes e patente ainda não existem.
2. **O chão guarda a batalha:**
   - a esteira afunda (o relevo do Look);
   - o tanque que morre deixa o **avental** queimado em volta e 5–8 **destroços**;
   - a explosão sem carcaça deixa a **cratera** (bacia com sombra dentro, borda e torrões);
   - a morte escondida na névoa aparece quando a área for vista.
   Desde esta noite o chão do mapa pinta isso no próprio material, pela função do Look (antes era um decal por cima).
3. **A carcaça queimada:** marrom-carvão mosqueado, com fuligem embaixo e na boca da torre, ferrugem de calor e o
   miolo em brasa apagando em 30 s.
4. **O dano no tanque vivo:** lascas de tinta queimada a partir de 70% da vida e fuligem abaixo de 50%, crescendo
   até a morte, como os estados de dano do ZH.
5. **A silhueta:** a unidade atrás de prédio, casa ou palmeira aparece na cor do time, meio transparente.
6. **O tremor de câmera** nas mortes perto do foco (pequeno: 2–4 px).
7. **O vento único** (`MPC_Wind`): uma direção só pro mapa todo. Todas as sessões já têm o contrato.
8. **Arrastar com o botão direito** vira a formação (leque) só com tropas selecionadas. Antes, com só um prédio
   selecionado, o arrasto virava ordem.
9. **A palmeira transparente perto do cursor saiu** (você nunca aprovou).

## O que achei e consertei de madrugada (pra você saber)

- **O chão do mapa ficou xadrez** quando a função do Look entrou no material. Foram duas causas, o Look e o
  WorldForge corrigiram, e hoje o código vai embutido no material.
- **Os parâmetros não chegavam ao chão.** A Unreal só aceita trocar textura no Landscape com "instância dinâmica",
  e o mapa vinha sem. O jogo agora liga isso sozinho, inclusive na malha Nanite do chão.
- **O queimado sumia na câmera do RTS:** a função só lia o caderno de detalhe de perto. O Look passou a ler o caderno
  do meio também.
- **A carcaça e o dano não apareciam:** o ruído que decide as manchas era uma foto de areia que nunca passava dos
  limiares. Troquei por ruído procedural no shader.
  - Na primeira tentativa a carcaça virou camuflagem urbana.
  - Fiz um preview fora da placa e escolhi o mosqueado.
- **O queimado e a cratera de longe:** a brasa do avental estourava em manchas amarelo-claras (40 000 nits).
  - A brasa ficou em 3000 nits, o mesmo pico do miolo da carcaça: lê laranja.
  - O Look mediu o avental na street_battle e escureceu o tom.
  - A cratera saía borrada na câmera do jogo: a conta do tamanho do pixel na função do Look superestimava ~1,6× e nunca
    usava o caderno de detalhe. Corrigido com a medida certa. Gravei a câmera deslizando sobre as crateras
    (`captures/game/cratera_pan1/batalha.mp4`) e não cintila.
  - Com o escuro mais forte, o avental virou pele de onça. Deixei o queimado contínuo e a brasa em pontinhos.
  - O Look criticou a cratera contra a street_battle duas vezes ("poço redondo", depois "riscos de pente" e "miolo de
    uma cor só"). Ela ganhou contorno torto, riscos largos e macios de fuligem, núcleo carvão com pedaços claros caídos
    dentro e torrões angulosos fora do anel; o avental ganhou borda dentada e manchas de areia.
- **Um quadro com a câmera fora do lugar:** em 1 das 404 fotos de teste, colado nas mortes, a câmera apareceu no centro
  do mapa, deitada. Não reproduzi e não achei a causa; fica anotado.
- **O pacote não cozinhava:** os compute shaders das crateras precisavam de uma flag no SM5. Corrigido; o pacote leva
  junto as imagens e fontes do HUD novo.

## O custo

O chão inteiro (relevo, crateras, queimado) custa **~0,5 ms**: a cena da vila foi de 23,4 para 23,9 ms de mediana,
medido limpo.

## Pra você decidir

- **A cratera:** opções A/B/C em `captures/game/crateras/alvo_vs_A.png`, `alvo_vs_B.png` e `alvo_vs_C.png`. O jogo
  usa a **B**.
- **A carcaça, o avental e a cratera contra o alvo:** `captures/game/folha_noite_alvo_vs_jogo.png`, lado a lado com a
  street_battle. É a primeira versão que dá pra julgar. A carcaça ainda é um tanque inteiro queimado: a malha amassada
  vem dos modelos finais, e o fogo e a fumaça vêm das sessões de explosão.

## Ainda falta

- a paralaxe curta dentro da bacia (Look);
- a cratera no asfalto (precisa do tipo de chão);
- a marca envelhecendo;
- o tanque afundando ao passar na cratera;
- a silhueta dentro da fumaça;
- o teste da LAN em 2 PCs.
