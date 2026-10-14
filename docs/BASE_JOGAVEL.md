# Base jogável: a lista (pra nunca esquecer)

Começada em 08/10/2026 a partir do levantamento de todas as sessões (7 leitores, mais a pesquisa
de como os AAA fazem, mais um crítico de completude) e revisada por mais 3 críticos (contra o
levantamento, os documentos do projeto e o vídeo-alvo). **Toda sessão que mexer num item marca aqui.**

**Legenda:** `[ ]` a fazer · `[~]` em andamento ou feito sem aprovação · `[x]` aprovado pelo Felipe.
Entre parênteses: quem faz.

**O que é a base:**
- Multiplayer de verdade, com 2 ou mais jogadores em rede.
- Cada jogador tem uma **War Factory gerando tanque**, **sem economia**.
- **Névoa de guerra** e **minimapa**.
- **Tudo visual na versão final**: modelos, explosões, água, trilhas, efeitos e detalhes.

Economia, construção livre e unidades novas ficam pra depois, só quando o Felipe pedir.

**O alvo:**
- **Em movimento:** `captures/concept/33_video/gameplay_v2.mp4` (folha `gameplay_v2_sheet.jpg`), a confirmar com o Felipe. Mostra a War Factory USA com tanques no pátio e saindo, poeira atrás de cada tanque, anel de poeira na explosão, fumaça preta e carcaças queimando.
- **Parado:** a foto oficial `docs/concept/scenes/desert_golden_hour/more/game_shot_usa_vs_gla.webp` e as folhas do diretor de arte, `captures/game/quadro_alvo_vs_jogo.jpg` e `captures/game/morte_alvo_vs_jogo.jpg`.
- **Os 7 defeitos da 1ª gameplay** (diretor de arte, 08/10):
  1. morte branca;
  2. os dois lados iguais;
  3. anéis grossos;
  4. desfile;
  5. tiro sem clarão nem poeira;
  6. chão que não guarda nada;
  7. fumaça cobrindo as unidades.

**Quem é quem:**

| Sessão | Cuida de |
|---|---|
| "RTS game architecture e gráficos" | o jogo, `Game/` |
| "C&C Zero Hour conceitual modernizado" | arte, diretor de arte e o **FXLab** (o fogo da carcaça). O tiro e o arsenal foram **pausados** pelo Felipe nela: vão pras sessões de explosão |
| "Fluxo de explosão realista AAA" | BlastLab |
| "Gerador de mundo RTS" | WorldForge |
| Gráficos (nova, 09/10) | o look do quadro inteiro (luz, céu, névoa, exposição, curva de cor, pós, padrão de material) pra ficar idêntico aos game shots; vira plugin comum pro jogo, o "ver na versão final" do WorldForge e o BlastLab. Entrada: `docs/GRAFICOS.md` |
| "Projeto complexo" | ExplosionLab, noutro repositório: `C:\Users\luisf\explosion-lab` |

---

## 0. Prioridade, em cima de tudo

- [~] **Minimapa, que responde pela névoa** (jogo, começado em 08/10)
  - [x] **Feito e testado pelo Felipe (08/10, "ficou bom"), commit `e927a02`:** foto de cima na tela de carregar, losango alinhado com a câmera padrão, pontos na cor do jogador, caixa da câmera (cores do ZH), névoa, clique do ZH (o minimapa fica com o gesto inteiro), o "sob ataque" do ZH e o Espaço. Detalhes em `docs/JOGO.md` Parte 6.
  - **O que mostra:**
    - o mapa pronto visto de cima: captura ortográfica do mapa final, porque o `minimap.png` do WorldForge é o do rascunho;
    - as unidades e a fábrica como pontos na cor do jogador;
    - o retângulo do que a câmera vê, que é um trapézio na câmera inclinada.
  - **Névoa no minimapa:** o nunca visto fica preto e o explorado escurecido. Inimigo só aparece quando visível, e o prédio inimigo já visto fica como "fantasma". Usa a mesma textura de visão da névoa da tela.
  - **Clique:** o clique move a câmera. Com unidades selecionadas, o clique ou o arrasto no minimapa dá a ordem (andar, ataque andando).
  - **Alertas:** pisca "sob ataque" no ponto do minimapa e da borda da tela; a fábrica avisa quando o tanque fica pronto.
  - **Decisão do Felipe:** sempre ligado, ou só com radar, como no ZH?
- [~] **Névoa de guerra** (jogo, começada em 08/10): simulação e desenho
  - [x] **Na simulação, feito e testado pelo Felipe (08/10), commit `e927a02`:** visão por time em inteiros (`Vision.h`), disco de 39,5 m carimbado como no ZH, explorado, a visão do morto fica 1 s, ordem de ataque em alvo invisível recusada, o comando `SetFog` de depuração (entra no replay e na impressão digital). A IA de teste **não** foi mexida (o Felipe: IA fica pra depois).
  - [x] **No desenho, feito e testado pelo Felipe (08/10), commit `e927a02`:** a passada própria que escurece só o opaco, antes das explosões (como o W3DShroud do ZH), com os níveis do ZH (0, 127/255, 255; 1 nível por segundo), a borda suave, a exposição local só bilateral, o que some na névoa e os vazamentos do HUD fechados. No fim, o mapa se revela.
  - [x] **Liga e desliga de debug** (pedido do Felipe, testado em 08/10): F9, botão na pausa e no menu principal. A regra vai pela fila só no skirmish local (`bAllowDebugCommands`); online, vira regra da sala.
  - **Na simulação:** visão por time numa grade de 4 m, em inteiros, com dois estados: vista agora e já explorada. Ela é igual em todos os PCs por causa do lockstep.
    - Só se mira no que é visível.
    - A IA só usa o que vê; hoje ela lê o mapa inteiro (`SkirmishAI.cpp`).
    - A visão continua um instante depois que a unidade morre.
  - **No desenho:**
    - uma textura de visão do jogador local, com transição de 0,5–1 s;
    - borda suave sem cara de grade borrada (desfoque separável mais ruído na borda);
    - um pós-processo que escurece e tira a cor (a técnica do ZH, do League of Legends e do SC2, nas fontes).
  - **O que esconder:**
    - **Unidades:** inimigo na névoa some com anel e barra.
    - **Prédios:** a fábrica inimiga já vista fica "fantasma", sem seleção.
    - **Efeitos e luzes:** o tiro escondido não mostra o clarão; o traçante só aparece a partir de onde entra na área visível. A luz de clarão e de fogo escondido não acende o chão visível.
    - **Som:** a batalha escondida fica muda (`docs/CONCEITO_SOM.md`).
    - **Marcas:** carcaça, cratera e marca de esteira feitas na névoa só aparecem quando a área é revista.
  - **Vazamentos de hoje a fechar:**
    - o cursor mira o inimigo escondido (`PickUnit`);
    - o "Inimigo N" no topo conta todos;
    - a seta pro inimigo e o "siga a seta vermelha";
    - a sombra e o Lumen de unidade escondida.
  - Efeitos e luzes de explosão fora da visão: o jogo zera a opacidade e as luzes (o raymarch desenha todo volume visível).
  - Mais vazamentos: explosão escondida não mexe na exposição, no bloom nem projeta sombra de fumaça; o orçamento das explosões conta só o que o jogador vê; a fábrica fantasma fica congelada (chaminé, portão e bandeira parados); o pós-processo da névoa entra depois do passo do raymarch.
  - Maphack: no lockstep todo PC sabe tudo e a névoa é só desenho. Pro ranqueado, anti-cheat e análise de replay (depois).
- [ ] **O que falta da névoa e do minimapa** (depende de outras peças):
  - a fábrica inimiga "fantasma" e o aviso "tanque pronto", que vêm com a War Factory;
  - a batalha escondida muda, que vem com o som;
  - o custo da passada da névoa, a medir com a placa livre.
- [ ] **Minimapa também na sala de espera:** mapa com as bases e a escolha da largada.
- [~] **Multiplayer de verdade** (jogo, 10/10: LAN pronta, sem o teste do Felipe). Feito: menu MULTIPLAYER (LAN), sala com até 8, cor por jogador, névoa do anfitrião, a montagem do anfitrião pra todos, lockstep com a impressão digital, volta de todos pra sala, mapa com 4 bases (`docs/MULTIPLAYER.md` Parte 7). Em 11/10: times na sala (aliados lado a lado, vitória do time), IA nas vagas, espectador de quem cai, névoa em 3 tipos (explorada, preta, desligada), a seta "enemy base" fora. Em 13/10 (N5, sem o teste do Felipe): a pausa pra todos, render-se e a queda com espera de 60 s e votação, como no ZH (`docs/MULTIPLAYER.md` Parte 7). Falta: reconexão, barras de carregamento, sinal no mapa, dar dinheiro, o online (último do jogo). Ver a seção 1.
- [~] **War Factory funcionando, com a mecânica original do ZH** (jogo, 10/10, sem o teste do Felipe): Command Center + Dozer, energia, construção, depósitos e caminhão, a fila da fábrica, a carga que cai, vitória por base, modelos provisórios. O que está feito: `docs/ECONOMIA.md` Parte 5. Ver a seção 1.

---

## 1. O jogo (sessão "RTS game architecture e gráficos")

### Mecânica
- [ ] **Rede:**
  - **Conexão:** transporte UDP (LAN e IP direto primeiro) e sala de espera (vagas, cor, time, mapa).
  - **Mesma partida nos dois PCs:**
    - aperto de versão: executável, `Units.json`, grade e mapa;
    - a semente e a montagem vêm do anfitrião;
    - os dois começam no mesmo passo.
  - **Ritmo:** atraso de comando adaptativo e sequência por jogador nos comandos.
  - **Dessincronia:** troca da impressão digital e relatório de dessincronia.
  - **Queda:** quem cai é esperado.
- [ ] **Critério de pronto da rede:** 30 min sem dessincronia a 0, 100 e 200 ms com 2% de perda; dessincronia forçada pega em ≤ atraso + 1 passo, com o sistema apontado; clique→imagem medido (~200–250 ms na LAN).
- [ ] **Segurança e ritmo:** cifra própria com chave combinada na sala (LAN e IP direto não têm relay); se um PC não aguenta, a partida desacelera e mostra quem atrasa; quem cai tem espera de até 60 s e votação.
- [ ] **Carregamento em rede:** uma barra por jogador na cor dele; só começa com todos em 100%.
- [ ] **O resto do revisor do juiz:** faixas no `ReadSetup`, script que recusa float/double/FMath no SimCore, clang-cl no simtest; com o 2º PC, o mesmo hash em Intel e AMD.
- [ ] **Como o 2º PC recebe o conteúdo:** o mapa (262 MB), as fatias e os assets por link estão fora do git (o `Game/Content` tem 2,3 GB e só 2 arquivos versionados), e o GitHub recusa arquivo acima de 100 MB: pacote cozido ou Git LFS.
- [ ] **Câmera em rede:** o Alt+meio desce até 12° (cinematográfico); travar na faixa de jogo no multiplayer?
- [~] **Pausar, retomar, render-se e sair viram comandos da rede** (13/10, sem o teste do Felipe): a pausa é o anfitrião parar de fechar passo, então todos param no mesmo; render-se é o comando `Resign`; quem sai ou cai é rendido pelo anfitrião.
- [ ] **HUD pra N jogadores.** Hoje é "Seus tanques / Inimigo", com a cor do jogador 1 fixa.
- [ ] **Grade igual nos dois PCs:**
  - conferir o hash da grade na sala de espera;
  - plano B: o anfitrião manda a grade (~0,4 MB);
  - a versão final é a grade canônica do WorldForge (seção 4).
- [ ] **Como testar com uma placa só:**
  - 2º jogador sem placa (`-nullrhi`) ou a simulação pura fazendo o jogador remoto. Duas Unreal com janela derrubam a 1660 (`docs/GPU.md`).
  - Pro Felipe testar clicando: 2º PC, 1º jogo empacotado (marco M1c, nunca feito) e o aviso do firewall, que só ele aceita.
- [~] **War Factory na simulação** (feito 10/10, `docs/ECONOMIA.md` Parte 5):
  - prédio com área ocupada na grade, vida, dono e armadura de estrutura;
  - posta na base de cada jogador (as largadas do `Maps.json` hoje nem são usadas).
- [~] **Fila de produção** (feita 10/10 com custo, como no ZH; a porta e a saída pela faixa da fábrica):
  - comandos pra produzir, cancelar e marcar o ponto de encontro;
  - tempo por tanque (conferir o BuildTime do Crusader no ZH 1.04) e teto de fila;
  - a porta abre antes, o tanque nasce dentro e sai até a frente (UnitCreatePoint / NaturalRallyPoint no ZH), e espera se a saída estiver bloqueada;
  - a saída cai na região ligada;
  - bandeira e linha do ponto de encontro com a fábrica selecionada;
  - porcentagem da produção.
- [~] **Fábrica destruída** (feito 10/10): para de produzir e **devolve o dinheiro da fila** (ZH `ProductionUpdate.cpp:1130`; a nota antiga "perde a fila" estava errada, revisão de 10/10).
- [~] **Vitória com base** (feito 10/10, a regra do ZH: perde quem fica sem prédio). Com fábrica produzindo sem economia, a partida pode não acabar.
- [ ] **IA com fábrica:** usa a fila e o ponto de encontro. No multiplayer, roda no anfitrião mandando comandos como um jogador. Decidir se a base tem vagas de IA.
- [~] **Carcaça com física** (o Felipe, 13/10: "a carcaça não tem que ter opção, tem que ter física"; feito no mesmo dia, sem o teste dele). Ela fica no mapa e não tem opção na sala:
  - **Na simulação** (versão 12, igual em todo PC): o veículo morto vira carcaça no lugar.
    - O vivo que encosta empurra; ela pesa o mesmo que o vivo, então quem empurra anda à metade da velocidade.
    - A batida nunca é reta (±9°), então ela gira e escorrega pro lado. No teste, andou 5,7 m e girou 18°, e o tanque passou.
    - Contra penhasco ou prédio ela trava, e aí segura o tanque.
    - O casco é dois círculos no eixo, e cabem até 256 carcaças no mapa.
  - **No desenho:** o modelo, o fogo e as brasas vão junto, e a carcaça arrastada risca o chão.
  - **Os testes:** `simtest` (`[carcaca]`) e o cenário `carcaca_empurra`.
- [ ] **Vegetação baixa** amassa ou dobra quando o tanque passa (jogo + WorldForge, dono do material da vegetação).
- [ ] **Visão pelo relevo.** Se o morro tapar a visão, a simulação precisa da altura por célula, e isso entra no pedido da grade ao WorldForge. Se a visão for um círculo como no ZH, não precisa.
- [ ] **Direção do tiro** pros efeitos na blindagem: o desenho tira da posição do atirador, sem mexer na simulação.
- [ ] **Separar o impacto:** "acertou tanque vivo" contra "caiu no chão", e o efeito pelo tipo de chão (areia, rocha, asfalto; água dá respingo). Hoje os dois são a mesma esfera.
- [ ] **Ligar no jogo o que os efeitos novos pedem:** o plugin Niagara, o ProceduralMeshComponent e o Json (fogo do FXLab, destroços do BlastLab) e um módulo de shader global carregado cedo (raymarch).
- [ ] **Evento único, como o FXList do ZH:** efeito, pulso de luz, tremor, som e marca saem do mesmo evento; clarão em t = 0, som e tremor em d/343 s do ponto de mira.
- [~] **Ctrl força o tiro, como no C&C clássico** (pedido do Felipe, 13/10 de manhã; feito na mesma manhã, sem o teste dele): segurando Ctrl, o clique manda atirar em qualquer coisa:
  - no chão, que cava a cratera da arma onde o tiro cai;
  - na própria unidade, no aliado ou na casa civil.
  - Serve também pra ele ver a explosão e a cratera.
  - É comando da simulação, em ponto fixo e igual nos dois PCs (o ataque ao chão é um alvo-ponto). O cursor de mira muda com o Ctrl.
  - **Feito:**
    - os comandos `AttackGround` (o ponto) e `ForceAttack` (qualquer dono), na simulação versão 9;
    - no simtest, o colega leva tiro em 4 passos, e o tiro no chão chega 8 vezes no ponto sem ferir ninguém, com o mesmo hash nas 3 compilações;
    - no jogo, Ctrl + clique esquerdo: na unidade ou no prédio, de qualquer dono, ataca; no chão, atira no ponto até mandarem outra coisa. O tiro no chão cava a cratera da arma;
    - o cenário de teste é o `tiro_chao`.
- [ ] **v3: o tanque derruba poste e árvore** (pedido do Felipe, 13/10 de manhã: "depois que acabar tudo"; fora da v2, a decisão dele à noite):
  - o tanque empurra e derruba os postes e as árvores (as palmeiras);
  - o WorldForge vai criar os fios com física, então o poste que cai puxa o fio, que balança e arrebenta;
  - "física bonita e efeito de choquinho bem bonito": as faíscas e o arco no fio que arrebenta. O efeito de energia, realista, pode vir do EmberGen ou ser pedido à sessão do Look.
  - **A divisão:** os fios e as peças são do WorldForge; o empurrão, a queda e o evento na batalha são do jogo.
  - **O cuidado:**
    - a queda é desenho (Chaos), igual em todo PC pelo evento;
    - o "derrubado" que muda o caminho (o tanque passa por cima, como o esmagar do ZH) é da simulação, em ponto fixo, senão a partida diverge na LAN.
- [x] Simulação determinística, skirmish, colunas do grupo e nascer fora de platô isolado (commit `e7de5ce`).
- [x] **Tela de carregar que espera a cena pronta** (t7: 4,1 s) e a correção do teste do resultado (commit `e927a02`).

### Visual feito no jogo
- [ ] **Cor do jogador no tanque:**
  - máscara no material e cor por dono; a máscara vem da arte;
  - o `M_UnitPBR` é compartilhado com o BlastLab: combinar, reimportar com o app fechado e respeitar a trava da placa;
  - a carcaça perde a cor.
- [ ] **Seleção final:**
  - anel fino no chão, desenhado na cena: o tanque fica por cima do anel, não o anel por cima do tanque;
  - só no selecionado e sob o mouse; inimigo sem anel (só sob o mouse ou quando vira alvo);
  - barra pequena, de tamanho fixo na tela;
  - o tanque selecionado pisca na cor, como no ZH.
- [~] **Traçante** (jogo, 13/10, sem aprovação: curto, fino constante na tela, laranja, nasce 1,5 m à frente da boca; clarão da boca pequeno e laranja, sem as bolas brancas da luz nova. Ainda fraco: o traçante quase some e o clarão é um disco macio, não um estouro):
  - curto e laranja, com espessura constante na tela;
  - nasce um pouco à frente da boca, some no impacto e é aditivo;
  - o ZH tem o efeito Tracer com Speed, Length e Width.
- [~] **Marcas de esteira de verdade** (jogo, 11/10, `RTSGroundMemory`; o Felipe gostou em 12/10, "salva esse modelo": `captures/game/esteira_v1_salva/`; o chão que afunda é do Look, `docs/CHAO_DEFORMAVEL.md`):
  - **Feito:** uma fita por lagarta, com as garras de 17 cm em fase, que some em ~5 min. Ela é riscada num caderno do mapa (50 cm por pixel) e num de detalhe em volta da câmera (4,7 cm por pixel), e um decal só pinta tudo: o custo não cresce com as marcas.
  - **12/10:** aparecem no jogo (cenário rápido "vila"), duas por tanque, com as garras e a curva da manobra. Custam +0,2 ms na mediana e +0,9 no p95, mais ~80 MB de memória de vídeo.
  - **Falta:** a borda de areia levantada; areia marca forte, rocha não marca e estrada marca de leve (precisa do tipo de chão do WorldForge); riscar no escuro e mostrar quando a área for revista; ver em vídeo.
  - **13/10 à noite, o Felipe:** no jogo a lagarta "não modela" (risco chapado ou nada). O alvo é a folha `captures/look/areia_C_final.jpg` (a trilha fresca + a velha cruzando, meio coberta pelo vento). **Dono do visual: o Look** (a MF `look_mf.py` lendo os cadernos do relevo). Prova: no jogo, ao lado da folha. Estado em `docs/CHAO_DEFORMAVEL.md` (o topo). **14/10, madrugada: o Felipe tirou da v2** ("pra v2 não vai ter mais nenhum ajuste, isso aí tudo vai ser pra v3"). Lagarta e cratera no chão vão pra v3 com o plano de relevo por pixel (`docs/CHAO_DEFORMAVEL.md`, a decisão no topo).
- [ ] **Esteira andando:** textura correndo pela velocidade (as rodas já giram).
- [~] **Chão que guarda a batalha** (jogo, 11–13/10, a mesma memória do chão; estudo, revisão e números em `docs/CHAO_DEFORMAVEL.md`):
  - **13/10, depois do estudo e de duas críticas (o revisor e o Look):**
    - a morte do tanque deixa o **avental** em volta do casco (terra queimada e combustível, brasa nos primeiros ~30 s, areia espirrada e torrões);
    - as explosões sem carcaça em cima deixam a **cratera** (bacia de ~2,6 m com sombra dentro, halo escuro, borda quebrada, raios e torrões).
  - **Como é feito:** os atlas assados por `Game/Tools/bake_craters.py` (6 variantes de cada), carimbados por um compute no caderno.
  - **13/10, o Felipe: "deixe as 3, cada uma pra uma coisa":**
    - a A (0,75×) é o tiro de tanque: 20% dos tiros passam raspando e cavam 6–12 m além do alvo (`impact_crater` e `near_miss` no visual do Units.json), e todo tiro no chão (Ctrl) cava;
    - a B (1×) são as explosões do prédio que cai;
    - a C (1,3×) é o tanque explodindo (o avental C) e, quando existir, o míssil pesado.
    - A cena `vila` mostra as três lado a lado antes da luta.
  - **A altura:** vai numa folha R16F própria, com a sombra do sol dentro da bacia.
  - **Os destroços:** 5–8 peças por morte (`game.ps1 debris`).
  - **A cor final:** é da MF do Look, embutida no material do chão do World_TestSalt2 desde 13/10 (o WorldForge monta com o CODE do `look_mf.py`).
    - O jogo liga as instâncias dinâmicas do Landscape, inclusive na malha Nanite: sem elas, os parâmetros não chegavam.
    - O jogo manda os cadernos de detalhe e do meio (`GroundTrackMid`, pra o queimado ficar visível na câmera do RTS) e a brasa a 3000 nits.
    - O decal de piso sai sozinho quando o chão lê o caderno do meio.
  - **Na névoa:** a morte que acontece fora da vista guarda o avental e os destroços, que aparecem quando a área é vista, junto da carcaça.
- ~~**Árvore transparente perto do cursor**~~: **saiu em 13/10** (o Felipe nunca aprovou pro jogo; vai ter tanque escondido nas árvores, `docs/IDEIAS_JOGABILIDADE.md` ideia 6). A árvore nunca entrega unidade escondida.
- [~] **Silhueta da unidade** (jogo, 13/10, sem a foto aprovada; **só as suas e as dos aliados**: o Felipe, 13/10 à noite, "nenhum tanque inimigo pode ser exibido atrás das coisas", senão a mecânica de se esconder atrás da árvore vai pro lixo): atrás de prédio, casa ou palmeira, a unidade aparece na cor do time, meio transparente e com a borda mais forte, como no SC2 (custom depth com o dono no stencil + o pós-processo `M_UnitSilhouette`; `-RTSNoSilhouette` desliga). Dentro da fumaça ainda não: a fumaça não grava profundidade.
- [~] **Contato com o chão** (jogo, 12/10: mancha macia de cantos redondos sob cada tanque, `M_UnitContact`; ainda fraca): sombra de contato e uma mancha escura macia sob cada tanque. A 140 px, a fresta escura embaixo do casco é o que diz "pesado".
- [~] **Tremor de câmera** (jogo, 13/10, sem teste do Felipe): só nas mortes vistas até 70 m do foco, caindo com o quadrado da distância; ruído de ~25 Hz apagando em ~0,35 s, 18 cm no tanque e 35 cm no prédio (2–4 px na câmera do jogo, então o clique quase não sente).
- [~] **Estados de dano** (jogo, 13/10, sem a foto aprovada; **14/10: fora dos veículos**. O Felipe viu a camuflagem pintada por cima e disse "um negócio porco"; volta quando for bem feito, e o prédio segue com as marcas): as marcas no material (o M_Structure) crescem a partir dos 70% de vida, como o modelo danificado do ZH: placas de tinta queimada e lascada, e a fuligem abaixo de 50%. A carcaça ganhou a receita do Look medida na street_battle: a tinta queimada, a fuligem embaixo e no anel da torre, a ferrugem de calor nas quinas, a cinza no teto, e a brasa no buraco da torre apagando em 30 s. A luz do fogo saiu do teto do casco (o "ponto de lâmpada"). As fumaças finas e escuras vêm do FXLab. **13/10 de madrugada:** nada disso aparecia no jogo. O ruído das manchas era a foto da praia, que no verde linear só vai de 0,22 a 0,25 e nunca passava dos limiares; o asfalto do miolo virava a média na distância da câmera.
  - Agora o ruído é procedural, no shader (`FRTSNoise` no `import_provisional.py`).
  - A carcaça é mosqueada, em rampas largas: a 1ª versão em recortes virou camuflagem urbana. Preview fora da placa na mediana 0,33 do alvo.
  - O dano vem em lascas de ~0,35 m: ~5% aos 70% da vida, ~25% perto da morte.
  - Cena de teste: `game.ps1 view -Scenario carcaca`.
- [~] **Fábrica viva no desenho** (10/10, provisório: ventiladores girando, luzes que acendem e apagam com a energia, a obra subindo com andaime e solda; falta portão, ponte rolante e braço ligados à produção): um ator do prédio anima portão, ponte rolante, braço e ventiladores e acende as luzes, ligado aos eventos de produção; mais a entrada do prédio no `import_units.py` e o link de Buildings no `game.ps1`.
- [ ] **Paleta das cores dos jogadores** (8, como no ZH): testada no deserto e no zoom mais afastado; as mesmas no tanque, na fábrica, no minimapa e no HUD; cor única por vaga na sala (dois USA sairiam azuis).
- [ ] **Lado legível sem o anel no inimigo:** no USA contra USA sobra só a cor em detalhes pequenos. Testar no zoom máximo (81,6 m); se não der, o inimigo ganha uma marca mínima na cor.
- [ ] **Sujeira acumulada:** o tanque que andou e lutou fica empoeirado (CoH3).
- [~] **A maquete do Look vira o jogo** (ideia do Felipe, 11/10, "surreal e fenomenal"; "não só o tanque: a casa, a rua, o look atmosférico, tudo"; ainda pensando, provável centro da v2, porque os modelos finais à mão podem não chegar nela). Do lado do jogo:
  - **Tanque:** escala de 7,9 m, pivô no chão, torre separada (gira e recua), cor do jogador (hoje os dois lados saem iguais), carcaça queimada.
  - **Casas e rua no mapa jogável:** a casa vira obstáculo na grade da simulação (o caminho desvia, não se constrói em cima). A grade tem que sair igual em todos os PCs (o hash da grade da LAN).
  - **Look (luz, céu, névoa, pós):** a passada da névoa de guerra, a foto do minimapa e a leitura das unidades na câmera de RTS continuam valendo.
  - **Custo:** medir em tempo real na 1660 com 50 tanques (o da maquete é render de cinema, com aquecimento).
  - Combinar com a sessão do Look: ela faz o visual, e o jogo leva pro mapa jogável.
  - **Feito em 11/10 (sem a aprovação do Felipe):**
    - **O tanque do Look é o USA_MBT do jogo** (`game.ps1 looktank`). Usa os GLBs dela em `captures/look/tank_glb`, o material provisório dos prédios e o slot Team com a cor do jogador; a torre gira em x 0,10 m. Teste base com 0 falhas e o custo igual (23,0 ms).
    - **Ainda fraco:** lê quase branco e a cor do time sai pequena. Espera o M_UnitDust e a receita do plugin RTSLook.
    - **O plugin RTSLook:** o desenho dela está revisado (`docs/GRAFICOS.md` §8). A cor do time vai no CPD 0..3 e o queimado no CPD 6.
    - **Casas e rua:** o formato do WorldForge (JSON com a pegada na grade de 4 m) foi combinado.
    - **A vila subiu na frente** (decisão do Felipe em 11/10: "o mapa do Look foi sensacional, é eles mesmo que eu quero"). Peças: o kit civil aprovado (1 andar 8×8, 2 andares 8×8, varanda 8×12, comprida 16×8, muro com portão), a rua e as miudezas do Look.
      - O WorldForge põe a vila no World_TestSalt2 e exporta o JSON, sem tapar as 4 largadas (~88 m de raio), os 6 depósitos e o caminho entre as bases.
      - O Look faz as malhas e o material.
      - O jogo monta a simulação e o desenho a partir do JSON.
- [ ] **Perspectiva aérea na câmera do jogo:** o terço de cima da tela clareia e esquenta com a distância, como no alvo; igual em todos os presets e sem tapar unidade (com o WorldForge).
- [ ] **Céu na câmera deitada** (Alt+meio): pôr do sol com nuvens como no alvo v3; nuvem nunca entre a câmera e as unidades.
- [ ] **Telas de resultado e 3-2-1 pra N jogadores:** anéis e barras do mundo somem quando o painel abre (hoje uma barra atravessa o "VITÓRIA"), letra nítida.
- [ ] **Menu e opções:** botões Multiplayer e Opções; preset, resolução, limite de quadros, volumes e teclas; preset **Auto** que mede a nossa cena (o detector da Unreal joga a 1660 no máximo, e a meta de 60 qps depende dele).
  - [x] 14/10 (sessão do jogo): a linha **QUALITY** (Low/Medium/High/Ultra) no Esc e no menu principal, além do F5–F8; fica guardada no `GameUserSettings.ini` e a 1ª vez num PC abre na Medium (a que rodou no notebook de 2014 na v1). Falta o resto da tela de opções e o Auto.
- [ ] **Áudio do lado do código:** ouvinte no ponto de mira a ~12 m, limite de cópias por som, mixagem que abaixa o resto, batalha escondida muda.
- [ ] **Legibilidade medida como critério:** quanto fumaça, explosão e névoa cobrem de uma unidade; o tanque contra cada chão (ΔE ≥ 20 ou contraste 3:1). A salina clara apaga a USA cinza-clara.
- [~] **HUD final** (o foco do jogo desde 09/10; o plano, com as fontes, está em `docs/HUD.md`: a barra do ZH modernizada, em Slate). Já feito: o texto nítido e a saída dos anéis do chão.
  - **13/10, o HUD novo do HUDLab no jogo** (pra o Felipe testar jogando): o plugin `RTSHud` da arte em `Game/Plugins`, ligado por `RTSHUDNew.cpp`.
    - Liga com `-NewHud`; na partida, **F4 troca** entre o novo e o de hoje.
    - O minimapa continua o do jogo (orientação, névoa, clique), desenhado no buraco da peça nova.
    - Os botões de sempre seguem pelos mesmos ids. Os grupos, a fila, Stop, Attack Move e as teclas das casas (QWERTYU / ZXCVBNM; o R posicionando prédio segue girando) funcionam; poderes e patente ainda não existem na partida.
  - painel da fábrica (fila, botão do tanque), sala de espera e tela de conexão;
  - cursor próprio e fonte final (a licença vai no `CREDITOS.md`);
  - tela de carregar com arte.
  - A arte de referência vem da sessão de arte.
- [~] **Vento único** (jogo, 13/10): `/Game/Materials/MPC_Wind`, vetor `Wind` = (x, y, m/s, rajada), com x, y a direção pra onde sopra; o jogo preenche no começo da partida pelo Maps.json (`wind_yaw`, `wind_speed`). A memória do chão já usa. Falta cada sessão ler: a fumaça (explosões), a areia (Look), as palmeiras (WorldForge) e as bandeiras. O contrato já foi mandado pra elas.
- [ ] **Efeitos sem colisão com o clique:** destroço, cartão de fumaça, carcaça e cratera não podem roubar o clique de mover nem dar tranco na câmera.
- [ ] **Antisserrilhado com efeito translúcido e linha fina:** cartões de fumaça borram quando a câmera anda, e traçante e anel cintilam. Ajustar o TSR.
- [ ] **Justiça entre presets:** o que esconde unidade (fumaça, poeira) tem de ser igual em todos os presets, senão quem joga no baixo vê mais.
- [~] **A vila do WorldForge no jogo** (12/10): o jogo lê o village.json (cópia pelo game.ps1 worlds, hash conferido com a etiqueta do mapa). As peças que bloqueiam fecham as células da pegada na grade (242 peças, 279 células). O jogo desenha cada peça com o kit civil do Look (/Game/Village, game.ps1 village; caixa provisória do tamanho certo quando faltar a malha). Teste "vila_rua": a coluna atravessa a vila pela rua desviando das casas. Falta: conferir o lado dos portões e a folga das picapes na beira da rua (com o WorldForge), as casas tomáveis e destruíveis (ZH) e o pacote novo da família.
- [~] **Desfile** (11/10, estudo de SC2, AoE4, BAR, SupCom, 0 A.D. e OpenRA, pelo código):
  - **Feito:** a área de reunião da fábrica. Os tanques param em fileiras escalonadas de 4 viradas pra longe da fábrica, com um tremido fixo de ±0,3 m e ±4°. O grupo chega virado pra direção da marcha (simulação versão 7, teste "ponto de encontro").
  - **Falta:**
    - as vagas do grupo sem cruzamento (húngaro até ~24, por N e nunca por tempo; acima disso, troca dos pares que se cruzam);
    - o mais forte na frente quando houver mais tipos;
    - [x] **a formação segurando F** (simulação versão 8, teste "formação com F"): o grupo vai no passo do mais lento, mantendo a forma, como o Felipe decidiu em 11/10 ("segurando uma tecla, como no C&C; sem segurar, cada um na sua velocidade"). Grupo espalhado demais é apertado até um bloco. Falta testar no jogo;
    - [x] **as torres em leque** (simulação versão 8): as da ponta olham até 25° pra fora e as do meio pra frente; em fila indiana, alternam esquerda e direita. Vale andando e parado, e na área de reunião também (testes "ponto de encontro" e "formação com F"). Falta ver no jogo;
    - [x] **o arraste com direção** (decisão do Felipe, 12/10: o clique rápido do esquerdo continua mandando andar). Com tropa selecionada, arrastar o esquerdo põe a linha de frente no ponto apertado, virada pra direção do arraste; o comprimento do arraste é a largura da linha (abaixo de 12 m, só a direção). Shift + arrastar, ou começar em cima de uma unidade nossa, faz a caixa de seleção. Funciona com o F e com o ataque andando. Teste "arraste com direção" na simulação (versão 8). Limite conhecido: com a linha virada de lado em relação à marcha, os tanques entram no mesmo corredor e alguns param antes da vaga (o certo é marchar em formação e só abrir perto). Falta testar no jogo.
  - A fila da IA pela praia (t7) é a mesma coisa, no caminho.

### Desempenho e estabilidade
- [ ] **Aquecer todos os efeitos antes da partida:** tiro, impacto, carcaça, poeira, marcas no chão e fábrica. No lockstep, a travada de um PC segura todos. Cache de PSO no jogo empacotado.
- [ ] **Diretor de orçamento pra todos os efeitos:** poeira do andar e fumaça de dano somem primeiro; tiro, impacto e morte ficam sempre (as prioridades do ZH).
- [ ] **M0, a batalha de referência:**
  - 50 tanques e 10 mortes juntas, medido por preset;
  - medir ms e memória de vídeo: mapa ~2,2 GB, mais explosões, cartões, fábrica e névoa, contra 6 GB (a placa caiu a 4,6 GB em 07/10);
  - meta: 60 qps no preset que a placa escolhe;
  - o plano põe no BlastLab (`docs/JOGO.md` Parte 4); falta o OK do Felipe pra pedir.
- [ ] **Medir MegaLights e Lumen Lite na 1660** antes de fechar os 60 qps: podem cortar os ~7 ms de luz e sombra das explosões.
- [ ] **Medir o mapa por preset no jogo.** O "ultra" do jogo é o cine (16× os raios do Lumen): rever.
- [ ] **Água igual à revisada:** no jogo, os reflexos do Lumen estão desligados até o alta. Fazer o A/B.
- [ ] **Congelar a versão dos assets.** Os links físicos quebram calados quando outra sessão regrava um arquivo, e no multiplayer os dois PCs precisam do mesmo conteúdo.
- [x] **Usar a fila da placa** (`C:\Users\luisf\.gpu_queue\gpu_queue.ps1`, que o `docs/GPU.md` prefere) no `game.ps1`: o teste e o jogo do Felipe seguram a placa até fechar; sem a fila, cai na trava (commit `e927a02`).
- [ ] Cada peça aprovada que chegar entra com folha lado a lado contra o alvo, **em vídeo**.

---

## 2. Arte e FXLab (sessão "C&C Zero Hour conceitual modernizado")

- [ ] **War Factory USA em 3D** (não existe; é o maior gargalo). O Felipe decidiu modelar **à mão**, ele mesmo no Blender (08/10; a modelagem por script da arte foi rejeitada: "massa de modelar"). Antes, ele aprende numa casa civil.
  - [~] ficha de produção feita (`docs/PRODUCAO_3D.md`, 08/10) + modelo-base do Blender (`art/tools/make_template.py` → `captures/art3d/RTS_Base.blend`), cobrindo: escala pelo tanque de 7,9 m (largura do prédio e do vão, 5–6 tanques é o limite), planta encaixada na grade de 4 m (vira a área ocupada na simulação, que nenhum documento define), pivô no chão, altura que não tape unidade na câmera de 37,5°, vista de cima primeiro, orçamento de triângulos e textura;
  - a folha tem a 3/4 de trás; faltam a elevação de trás e o lado oposto;
  - peças separadas: os **2 ventiladores grandes do teto** (o maior elemento visto de cima), ponte rolante amarela, braço robótico, bandeira e domo;
  - **portão:** a arte aprovada é um galpão aberto, sem portão. A regra do ZH (porta abre, tanque sai) pede portão novo (enrolável?) ou outro sinal (luz, a ponte baixando o tanque). Isso muda arte aprovada: decisão do Felipe;
  - luzes de serviço, postes e balizas acesas (sem neon), interior do galpão iluminado;
  - estados: inteiro, danificado e escombro;
  - máscara da cor do jogador: a regra já manda lona no teto (`CONCEITO_CONSTRUCOES.md`), e a vista de cima da folha já tem painéis azuis. Medir de novo na vista de cima antes de inventar peça;
  - os objetos do lote: postes, piso com faixas;
  - a versão destruída (alvo 4.5), se a vitória for pela fábrica.
- [ ] **Tanque USA final** (o de hoje não foi aprovado pelo Felipe; o nível "leve" de ~49 mil triângulos foi escolha da arte, decisão dele):
  - fiel à ficha: hoje a frente da lagarta e a roda motriz saem em blocos lisos, e o azul fica apagado na câmera alta;
  - máscara da cor: a lona do cesto é a maior área vista de cima; passar no teste de visão de longe;
  - texturas de dano e queimado, a carcaça e as antenas.
- [ ] **Se for USA contra GLA:** tanque GLA e Oficina GLA em 3D (as fichas já estão aprovadas).
- [ ] **Fatia vertical do deserto** (proposta da arte, aceita pelo Felipe em 12/10). Entra **quando as frentes atuais das outras sessões terminarem**, sem atropelar ninguém.
  - **O que é:** um canto do mapa do deserto, na câmera do jogo, **lado a lado com `docs/concept/scenes/desert_golden_hour/best/town_battle_top_gemini.webp`**. É a prova de que o jogo chega nos game shots.
  - **Os modelos:** provisórios **caprichados e fiéis às fichas aprovadas** (War Factory, tanque, Dozer e Chinook da USA, mais o tanque e a picape do GLA). Quina dura, a proporção e a silhueta da ficha, material com desgaste, o pó subindo de baixo pra cima e a cor do time. Os finais do Felipe substituem depois, sem mudar o lugar.
    - **Dozer (13/10, sessão Modelos 3D):** refeito por script pela arte aprovada (`Game/Tools/model_usa_dozer.py`, medido contra o tanque), aprovado pelo Felipe e no jogo (`game.ps1 dozer`, `Units.json`, sirene amarela que gira e acende só na obra). Mapa dos provisórios: `docs/MODELOS_PROVISORIOS.md`.
    - **Por que provisório serve:** na câmera de 61 m o tanque tem ~120 px. No teste do Dozer, o rebite virou granulado e o que leu foi a silhueta, a proporção, o material e a luz (`captures/concept/43_usa_base/dozer_game_scale_test.jpg`).
  - **Quem faz cada peça:**
    - os provisórios: quem já faz (o jogo, com `Game/Tools/blender_provisional.py`; o look, com o tanque);
    - a conferência: a arte, com a folha "provisório × ficha" na câmera do jogo;
    - o chão: o WorldForge (alvo `close_ground_light_gemini.webp`);
    - a luz: o look (a receita das sete);
    - uma explosão: as sessões de explosão.
  - **A prova:** a folha lado a lado e um vídeo. O Felipe julga.
- [~] **Fogo da carcaça do FXLab** ("o melhor até agora", salvo; **não aprovado como final**, Felipe 09/10), levar pro jogo:
  - no deserto com a luz do jogo ficou igual ao aprovado (14:39 de 08/10; o Felipe: "fez um ótimo trabalho com o fogo"); parados: o vídeo no deserto e a medida com 1+1 e 3+3;
  - ciclo de vida: pico, cauda e apagar (hoje solta sem fim);
  - emissor em Niagara (hoje é C++ com malha procedural), textura comprimida (BC6H/BC7) e várias carcaças ao mesmo tempo;
  - caminhos: o cozimento e o preset moram em `captures/` e no FXLab, fora do git e do pacote (e `captures/` é candidata a apagar pra liberar disco: perder é perder o aprovado); o sol é achado pela tag "GameSun", que o mapa do jogo não tem; o material 6-way vira etapa do `build_content.py`;
  - duração (a proposta era 2 min) e o "espalhar pelo capim" que o Felipe perguntou;
  - ela leva quando o Felipe mandar.
- [ ] **Clarão de boca do canhão** (dono: sessões de explosão; o arsenal foi tirado da arte): chama curta, luz no casco e no chão, fumaça saindo do cano e poeira levantada no chão à frente (o ZH empilha 5–6 camadas).
- [ ] **Faíscas quando o tiro acerta o tanque** (alvo 4.2; dono: sessões de explosão): leque de riscos quentes, clarão no metal, chapinhas e uma baforada cinza. **É o efeito que mais aparece no jogo**, porque o tiro quase sempre acerta.
- [ ] **Tiro na areia** (alvo 4.1): **pausado pelo Felipe** na arte (~14:56 de 08/10); fica com as sessões de explosão e não se retoma no FXLab sem ele pedir.
- [~] **Poeira do andar** atrás das lagartas: cor pelo tipo de chão, baixa, some em 3–6 s e é a primeira a sair no preset baixo.
  - **No jogo desde 14/10, de madrugada, sem o teste do Felipe:** o plugin RTSTreadDust da arte (a v12 do FXLab), em `Game/Plugins/RTSTreadDust`, com o passo a passo no `INTEGRACAO.md` dele.
    - O jogo manda a cada quadro os veículos de lagarta perto da câmera: a posição desenhada, a velocidade com sinal, o giro, a areia pelo `surface.png` do WorldForge (para no asfalto) e a névoa.
    - O preset vai pelo F5–F8; `-RTSNoDust` desliga.
    - O custo: na `chao` (12 tanques, alta), a mediana subiu de ~25 pra 31 ms.
  - 13/10, a arte: feita no FXLab (navegador e Unreal, mesma lógica) contra as cenas aprovadas; o estudo e o como estão em `docs/FXLAB.md`;
  - pro jogo: o ator `AFXTreadDust` (um só pra todas as unidades, `SetSources()` por quadro);
  - 13/10 à noite, a arte: a v12 (o caminho escolhido, rastro longo) empacotada no plugin `FXLab/Plugins/RTSTreadDust` (`ARTSTreadDust`, presets 0–3), com o passo a passo pro jogo em `FXLab/Plugins/RTSTreadDust/INTEGRACAO.md`; falta a sessão do jogo ligar (fontes de `SurfaceAt`, névoa, preset);
  - 13/10, o Felipe: a poeira tem de ser como a das gameplays em vídeo, gravada no EmberGen; a feita à mão fica de plano B. A arte simulou no EmberGen (`docs/FXLAB.md`): a versão EmberGen é a ativa (`FXLab/tread.json`); falta o Felipe ver o vídeo e a ponte do jogo.
- [ ] **Chaminés da fábrica:** vapor branco na USA (fumaça preta é a da Oficina GLA), acima do nível das unidades, lendo o vento único.
- [ ] **Cratera e queimado** com textura de verdade (escaneada, não ruído); destroços que ficam.
- [ ] **Fumaça de dano** pro tanque ferido, a mesma da carcaça, menor.
- [ ] **Calor** (distorção sobre fogo e tiro) e **brasas** subindo da carcaça e da cratera.
- [ ] **Respingo** de tiro na água.
- [~] **Kit civil do deserto** pros mapas (pedido do Felipe pra 1ª gameplay jogável): casas de vila modulares, veículos civis abandonados, postes, cercas e sacos de areia.
  - fichas aprovadas (08/10): casa de 1 andar, 2 andares, varanda, comprida e muro com portão, em `docs/concept/buildings/civil/` (tiradas da cena do jogo); referência de todas as casas das cenas em `docs/concept/towns/`;
  - kit pronto: os pagos da Fab foram descartados (custo) e os grátis do Sketchfab achados ruins (09/10, `captures/art3d/kits/`); **o Felipe modela à mão**, começando pela casa de 1 andar (`captures/art3d/civ_house_1story/`, com as 5 vistas);
  - 6 variações a mais (quartinho no teto, ruína, pátio, fortificada, mesquita, loja): **adiadas** por ele.
- [ ] **Ícone do tanque** pro botão do painel da fábrica e o **retrato da unidade** (unit portrait) do tanque, o que aparece no painel quando ele está selecionado.
- [ ] **Se for USA contra GLA, além dos modelos:** as bandeirinhas verdes (cor do jogador do GLA) balançando no vento; a morte do GLA com outra assinatura (sucata, rodas, fumaça preta oleosa); a Oficina solta o tanque por rampa subterrânea (outra saída); o amarelo do GLA some no deserto (fica pra interface); a máscara da Oficina marcada à mão.
- [~] **Alinhar a sessão de arte com esta lista** (09/10): a arte leu a lista; o 1º teste é uma War Factory por lado, sem Centro de Comando, Dozer nem suprimento (decisão do Felipe, 08/10).
- [~] **Arte de referência do HUD, do minimapa e da névoa** (explorado contra nunca visto). Pedida em 09/10 com as medidas do ZH (`captures/concept/hud/PEDIDO.md`). Os prompts dos 3 estados já estão com o Felipe; a aprovada vai pra `docs/concept/hud/`.
- [ ] **Som**, a especificação, que está adiada:
  - tiro, impacto, morte, motor e esteira;
  - fábrica produzindo e "unidade pronta";
  - interface, música, ambiente (vento do deserto), voz das unidades e locutor;
  - o pacote da Sonniss não tem música nem voz.
- [ ] **Créditos e licenças:**
  - o site do Hunyuan usado;
  - declarar IA na Steam (inclui a pintura do Gemini);
  - o que sai do EmberGen em teste.
- [ ] **Documentos:**
  - o FXLab entra no `CLAUDE.md` e no `README.md` (sessão nova não sabe que existe);
  - o §10 do `CONCEITO_UNIDADES.md` está desatualizado.

## 3. BlastLab (sessão "Fluxo de explosão realista AAA")

- [ ] **Levar pro jogo a bola de fogo aprovada em 08/10** (o raymarch, commit `317aca6`):
  - como peça comum aos projetos, um plugin, e não mais uma cópia;
  - os dados (3 × 213 MB) num lugar que vai no pacote.
  - Hoje o jogo desenha com o volume antigo rejeitado.
- [ ] **Morte de tanque completa:** clarão, anel de poeira, bola laranja, destroços e faíscas-cometa, torre voando com rastro e coluna preta, numa família só de fumaça.
- [ ] **Refazer o clarão branco** sem lavar a tela nem tapar unidades: bola HDR, bloom e luz curta. Já está na fila dela.
- [ ] **A fumaça não cobre as unidades:** sobe e sai do nível delas em segundos (defeito 7).
- [ ] **Batalha grande:** mais de 16 explosões juntas, e o tranco com 12.
- [ ] Atualizar a memória `project-raymarch-portado`, que ainda diz "não aprovado".
- [~] **A fila real dela:** 1º o detalhe fino da fumaça de perto (mexe no `BlastRaymarch.usf` que o jogo vai levar, então portar antes de fechar gera cópia velha); 2º o clarão.
  - 09/10: as bolhas de perto entraram (commit `bedd4e7`); 128³, sombra fina e borda afiada não mudaram nada visível. A maciez vem da simulação gravada, então agora é **"mudar quem grava"**: a mesma explosão do WebGL, EmberGen, Blender (e depois Unreal e Houdini) tocada no mesmo raymarch, lado a lado (`-RmCompare=`, `Tools/build_raymarch_vdb.py`). Em curso.
- [ ] **Anel de poeira da morte como peça própria:** o raymarch não tem a saia de poeira; levar só a bola perde o anel do alvo (claro, rente ao chão, some em ~1,5 s). Candidato: o cartão deitado `shockwave_top` do FXLab. Hoje o volume antigo deixa uma rosca escura no nível das unidades por dezenas de segundos.
- [ ] **Morte de tanque:** custa 32/41 ms na câmera de RTS (precisa caber nos 60 qps com várias juntas) e ainda está no volume antigo no próprio BlastLab (duas fumaças na mesma batalha).
- [ ] **Variação da morte por sorteio:** explode já ou queima 2 s antes, sub-explosões, 4 sementes; senão 10 mortes iguais se repetem.
- [ ] **A coluna da JangaFX** (aprovada, "tá boa") está linkada no jogo e nada usa: entra na família única ou sai.
- [ ] **Sombra da fumaça no chão** (light function no sol, 0,06 ms) e a **deformação da fumaça** pelo que passa (aprovada) ainda não existem no jogo.
- [ ] **Onda de reação em d/343 s** (fase 3b): bandeira chicoteia, poeira cai do telhado da fábrica, tanque vizinho balança, palmeira curva.
- [ ] **A ferramenta de A/B oficial** (Seq_Explosion pelo Movie Render Queue) sai lavada; hoje a comparação é ao vivo.

## 4. WorldForge (sessão "Gerador de mundo RTS", parada desde 07/10 esperando o Felipe)

- [ ] **Mapa oficial congelado,** com nome e versão: o jogo copia em vez de linkar.
- [ ] **Grade de passagem canônica:** `pass.png`, obstáculos, versão e hash, tirada da altura final.
- [ ] **A estrada com volume** (v3; 14/10 o Felipe procurou na vila do jogo e não achou, ficou só na maquete): a laje de 12 cm com a borda quebrada, nos 3 tipos (intacta com areia na beira, gasta, destruída) escolhidos por trecho no WorldForge. Protótipo e imagem: `docs/RECEITAS_DO_LOOK.md` §6 e `docs/aprovados/look/2026-10-13_estrada_volume_3tipos.jpg`. Donos: Look (o kit: perfil, borda, material), WorldForge (o traçado e o leito rebaixado), jogo (a altura na simulação). A faixa pintada nova (§7, "em teste") também não foi pro jogo.
- [ ] **Lote plano da War Factory em cada base,** sem seixo nem marola, com a estrada chegando na porta e a saída marcada.
- [ ] **Tipo de chão por camada** (areia, cascalho, terra, rocha, salina, asfalto): dá a cor da poeira e dos torrões.
- [ ] **"Memória do chão"** no material do terreno, pros rastros de esteira, o queimado e a cratera.
- [ ] **Simetria:** as pedras pequenas não ficam iguais entre os dois lados (o deslocamento não gira com o mapa).
- [ ] **Mapa de 4 bases** se for 2v2 ou cada um por si. Já existe o candidato a mapa oficial 1v1, o "AlUla" (`docs/concept/maps/alula_1v1_candidate.webp`).
- [ ] **Mapa sem campos de recurso,** já que não há economia.
- [ ] **Minimapa do mapa pronto,** ou captura feita pelo jogo.
- [ ] **Aprovação da fase 1,** que o mapa do jogo já usa: oásis, tamareiras, salina, água e borda de serra. O Felipe nunca viu, e não tem commit.
- [ ] **Chão do deserto:** está mais vazio e liso que o alvo. Faltam pedregulho e cascalho em grupos.
- [ ] **Folhagem** lendo o vento único.
- [ ] **As marcas de pneu fixas** (~1.900 decalques, aprovadas) vão conviver com a esteira dinâmica: decidir se ficam. A avaliação de arte as chamou de "marca de esteira" por engano.
- [ ] **Times no mapa** (`FTeamLayout`, pedido 1 do `JOGO.md`) pros mapas por time e de 4 bases, com as regras de justiça.
- [ ] **Oclusão igual por jogador:** com o giro de 180° e a câmera sempre do mesmo lado, o que fica na frente de uma base fica atrás da outra. Aceite: oclusão ≤ 3% e igual entre jogadores (±5%).
- [ ] Se o mapa oficial for o AlUla, ele depende da fase 2 do WorldForge (~2 semanas).
- [ ] O minimapa do mapa pronto tem dois donos possíveis (o jogo captura ou o WorldForge exporta): **o jogo captura** (decidido aqui em 08/10, porque já está fazendo).

---

## 5. Decisões do Felipe (travam o resto)

1. **Curva de tom e luz únicas,** a mais urgente.
   - A explosão foi aprovada na curva do BlastLab, e o mapa e o fogo da carcaça na padrão; por isso a morte sai branca.
   - Junto: quem é dono da exposição e do bloom no jogo.
2. **Uma família só de explosão:** o raymarch do BlastLab, os cartões do FXLab, o ExplosionLab ou uma costura medida entre eles.
3. **Modelos finais:** o Felipe decidiu fazer à mão (08/10). Falta: quem modela, ele no Blender ou a arte por script.
4. **Lados:** USA contra USA, só com a cor, ou USA contra GLA como no vídeo (o dobro de modelos).
5. **Vitória:** perde quem perde a fábrica?
6. **Rede:** 1 contra 1 por LAN ou IP primeiro? Tem um 2º PC? O relay da Steam fica pra depois (precisa de App ID).
7. **Névoa:** no estilo do ZH (preto no nunca visto, cinza no explorado)? Explosão dentro da névoa aparece? A coluna de fumaça alta vira pista?
8. **Minimapa:** sempre ligado ou só com radar?
9. **Fábrica:** fila de quantos? Teto de tanques? Cada um começa com tanques além da fábrica?
10. **Carcaça:** já decidido (06/10): fica como cobertura nos testes e vira opção no jogo. Falta: se bloqueia na simulação.
11. **Som** entra nesta base? Se sim, OK pra baixar o pacote da Sonniss.
12. **Mapa:** o World_TestSalt2 (1024 m, ~2:50 até o contato) ou um 1v1 do tamanho do ZH (700–800 m)?
13. **Quem integra o visual no jogo.**
14. **Liberar a sessão do WorldForge** pros pedidos da seção 4.
15. **O "manda"** pra sessão de arte levar o fogo da carcaça pro jogo.
16. **Commits pendentes:**
    - `FXLab/` (o fogo aprovado só existe lá);
    - `docs/concept/`, os `CONCEITO_*.md` e o `CREDITOS.md`;
    - a fase 1 do WorldForge;
    - as duas correções do jogo.
17. **O Vanguard** (`C:\Users\luisf\newcnc`) continua como referência de leitura?
18. **Sol do jogo:** elevação, direção em relação à câmera e cor, junto da curva de tom. Cada peça foi aprovada sob um sol: o fogo da carcaça a 10°, o mapa a 20°, o tiro bate com a arte a ~30°, as explosões às 16 h com poeira no ar. No alvo a luz é contraluz quente, com sombras pra baixo-esquerda e preenchidas; no jogo o sol vem da esquerda e a sombra sai escura e azulada.
19. **Quem monta o cenário de luz de referência** que vira o plugin comum de ambiente (BlastLab, WorldForge e jogo): **decidido (09/10), a sessão nova de gráficos** (`docs/GRAFICOS.md`).
20. **Geração automática** pra arte (codex login / APIs), sem colar cada prompt.
21. **EmberGen pago** (hoje em teste) só se as gravações CC0 da JangaFX não bastarem.
22. **O que é "trilhas":** marca de esteira, rastro de poeira ou traçante?
23. **WorldForge:** os pedidos do jogo entram antes ou depois do que já está na fila dela (ferramentas de edição, Desert Fury)?
24. **Cratera como cobertura**, com o veículo passando mais devagar (mexe na simulação). Mesmo sistema da **mureta que protege o tanque** (ideia de 09/10): as ideias de jogabilidade pra depois ficam em `docs/IDEIAS_JOGABILIDADE.md`.
25. **Teste final da base visual:** um roteiro (tanque morre a ~55 m da fábrica, ao pôr do sol), gravado em vídeo com som e comparado ao vídeo do Veo.

---

## 6. Riscos e cuidados (o que já mordeu ou vai morder)

- **Uma placa pra todos.**
  - O teste do Felipe é dele: ninguém roda nada por cima.
  - Duas Unreal com janela derrubam a 1660.
  - Seguir a trava ou a fila (`docs/GPU.md`).
- **No lockstep, engasgo vira travada de rede:** o p95 do quadro e o aquecimento passam a ser critério de rede.
- **Medição contaminada:** cozimentos na CPU de outras sessões dão número falso.
- **Memória de vídeo:** a placa tem 6 GB, e ninguém mediu tudo junto.
- **Disco:** ~32 GB livres (08/10, 15h). O jogo empacotado, os caches, a Sonniss (~7,5 GB) e os dados das explosões disputam esse espaço.
- **Links físicos** quebram calados: congelar versões.
- **Uma árvore de trabalho só, com 5 sessões:** o commit de uma leva o trabalho das outras (já aconteceu no `317aca6`). Nunca usar checkout ou stash.
- **3–4 cópias da explosão divergindo:** decidir o plugin comum.
- **Gargalo de aprovação:** as explosões andam uma coisa de cada vez, e o que a base precisa está no fim da fila delas.
- **Presets diferentes** dão vantagem no multiplayer se o corte esconder ou mostrar unidade.
- **Licenças:** IA declarada; o código do ZH só pra ler (copiar números serve, código não).
- **O repositório é PÚBLICO no GitHub** (Felipstein/explosion): cada commit publica. Commitar o `FXLab/` publica o `.ember` feito no EmberGen em teste; os arquivos do Tripo não podem entrar; som da Sonniss solto no repositório é redistribuição (conferir a licença). Créditos: o `ThirdPartyNotices.txt` (o MIT do céu e da curva do WebGL, se ela for pro jogo).
- **A câmera do jogo é 37,5° (ZH) e as artes-alvo foram pedidas a ~55°:** a folha alvo × jogo compara câmeras diferentes, e a regra de altura do prédio foi pensada a 55°.
- **Fechar processo só pelo PID:** o teste de rede abre dois jogos ao lado das Unreal das outras sessões.

---

## 7. Como os AAA fazem (fontes do levantamento de 08/10)

| Item | Técnica | Referência |
|---|---|---|
| Clarão de boca | 5–6 camadas curtas: clarão em cone, luz curta sem sombra, fumaça do cano, poeira no chão, calor, recuo | ZH `FXList.cpp` (LightPulse, Tracer); ARL-TR-909 (clarão do 120 mm) |
| Traçante | partícula alinhada à velocidade, comprimento de 1/60–1/30 s de voo, aditiva, espessura constante | ZH `TracerFXNugget`; Niagara Velocity Aligned |
| Impacto na blindagem | faíscas, flash, estilhaços, marca no casco | CoH3 (dano por zona); arte 4.2 |
| Poeira do andar | partículas nas duas lagartas, na razão da velocidade, prioridade baixa | ZH `W3DTankDraw` (TreadDebris), `ParticleSys.h` (DUST_TRAIL) |
| Marcas de esteira | tira de quads que some em 300 s; ou decal; ou render target / RVT | ZH `W3DTerrainTracks`; Rise of the Tomb Raider (GPU Pro 7) |
| Cratera e queimado | decal no relevo com limite e saída do mais velho | ZH `addScorch` (500); BAR `gfx_decals_gl4` |
| Cor do jogador | máscara num canal mais cor por instância | ZH houseColor; SC2 (alfa do difuso); Custom Primitive Data |
| Seleção e barras | barra só selecionado ou sob o mouse, 3 px; anel no chão sob o modelo | ZH `Drawable.cpp`; SC2; AoE IV |
| Névoa | textura de 1 texel por célula, 1 s de transição, visível/explorado/nunca; desfoque separável | ZH `W3DShroud`; Riot, "A Story of Fog and War"; Liquipedia SC2 |
| Fábrica | porta abre, espera e fecha; unidade nasce dentro e sai até a frente; intervalo entre saídas | ZH `ProductionUpdate`, `QueueProductionExitUpdate` |
| Água | Single Layer Water; respingo; ondinhas por render target | Unreal SLW; `NiagaraDataInterfaceWater` |
| Som | limite de vozes, prioridade, microfone no ponto de mira, HDR de áudio | ZH `GameAudio.cpp`; DICE GDC 2009; Diablo IV GDC 2024 |
| Contato com o chão | SSAO, sombras translúcidas, sombra de contato | SC2 SIGGRAPH 2008 |
| Silhueta | profundidade e estêncil próprios, contorno só onde está tapado | SC2 3.0; Tom Looman (Custom Depth) |
| Dano | estados aos 50% e 10% de vida, efeitos presos no modelo | ZH `BodyModule`, `BoneFXUpdate` |
| Tremor | "trauma" ao quadrado com ruído; clique sem tremor | ZH `W3DView`; Eiserloh GDC 2016 |
| Orçamento | prioridade por sistema de partículas, cortes por preset | ZH `GameLOD`; Niagara Effect Type |
| Coerência | tudo aprovado sob a mesma curva, exposição e luz da cena do jogo | `docs/JOGO.md` (achado do t4); unity 6-way |

O bruto do levantamento (status com prova, por área) está no journal do workflow `wf_5c0196e0-291`
da sessão do jogo, e o resumo das decisões já tomadas está nas memórias do projeto.
