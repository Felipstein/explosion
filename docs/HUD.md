# HUD do jogo: o plano

Pedido do Felipe (09/10/2026): "focar no HUD pra termos mais controles", depois os atalhos e
os detalhes visuais. Nas palavras dele: "eu gosto muito do HUD clássico do C&C ZH, mas
precisamos modernizá-lo". A base é a barra de baixo do ZH, modernizada.

Este plano saiu de 4 pesquisas e de um revisor adversarial (workflow `wf_2f42c938-3cc`, 09/10):
- o ZH pelo código GPL;
- os RTS modernos (SC2, AoE IV, CoH3, Tempest Rising, Stormgate, BAR, C&C Remastered);
- a técnica na Unreal 5.8;
- o inventário do HUD de hoje.

O bruto está no journal do workflow, na sessão do jogo.

**Já feito, antes do plano (09/10):**
- texto nítido: FSlateFontInfo no tamanho exato, Bold de verdade, posição em pixel inteiro;
- sem anel no chão (o ZH não tem nenhum): a seleção aparece pela barra de vida.

**A) Técnica**

**Slate puro em C++ para tudo que é tela**, em arquivos novos (por exemplo `Private/UI/SRTSHudRoot`, `SRTSCommandBar`, `SRTSMinimap`). Prova em casa: `SForgePanel.cpp` e `ForgeUITest.cpp:88-119`.
- A letra é rasterizada no tamanho final (`ElementBatcher.cpp:1256-1268, 3814-3817`).
- A escala segue a curva da Unreal (`BaseEngine.ini:1474-1475`: 900p→0,833, 1080p→1,0, 4K→2,0).
- **Texto mínimo Slate 14:** corpo de 18,4 px em 1080p e 36,8 px em 4K (XAG 101), medido na Roboto do motor.
- **Língua: inglês por padrão** (decisão do Felipe, 09/10). Todo texto de tela vai por `LOCTEXT`, e o português entra depois como tradução. Exemplos: "Your Tanks 6 · Destroyed 2", "USA Main Battle Tank", "Attack Move", "Guard", "Stop", "Unit under attack".

**Canvas** (o `RTSHUD.cpp`, da sessão de gráficos) fica com o que é preso ao mundo: anel, barra de vida, marca de ordem e seta. Some quando um painel abre.

**Fora:**
- UMG: é o mesmo Slate com mais objetos.
- CommonUI: briga com a trava do mouse (`CommonUIActionRouterBase.cpp:1866-1912` contra `RTSPlayerController.cpp:54-58`). Mudar a linha do `JOGO.md:247`.

**Regras de entrada:**
- Fundo da barra trata Down, Up e DoubleClick de esquerdo, direito e meio (`SWidget.cpp:491`, `PlayerInput.cpp:461-464`).
- Botões:
  - `IsFocusable(false)` (`SButton.cpp:89`, `:293-311`);
  - disparo ao apertar, `EButtonClickMethod::MouseDown` (ZH, `GadgetPushButton.cpp:72`);
  - cursor normal declarado.
- Navegação do Slate desligada.
- Minimapa como widget irmão, com clique só no losango, alimentando o controle.
- O controle ignora clique de mundo fora da área do mundo.
- Sem destaque de unidade sob o HUD.
- HUD retirado no fim da partida.
- Modo de entrada `GameAndUI`, como hoje.

**Custo:** antes e depois com o mesmo replay, medindo `stat unit`, `stat slate` e o trecho de GPU "SlateUI". Critério: dentro do ruído de duas corridas iguais. Sem desfoque de vidro.

**B) Layout em 16:9** (unidades Slate = px em 1080p; 1600×900 = ×0,833)

```
 0%  avisos (ZH 10,10)   [ 12:34 | Seus 6 | Destruidos 2 ]      (0,8%)
     MUNDO: tela toda, câmera intacta; a barra opaca tapa a faixa de 47–57 m
75%    /\
82% +-/  \-+----------------+---------------------------+------------+
    |< MAPA >| 1:4 2:2 grupos  | . . . . . ATK GUA  (7×2) | PRODUÇÃO   |
    | \  /  | foto Tanque USA  | . . . . . .   PAR        | fila 3×3   |
100%+--\/---+----------------+---------------------------+------------+
     270       ~664 (sobra)        684                     254
```

- **Barra: 194 de altura (18%).**
  - Duas fileiras de botões do ZH, 90×79 (50×44 em 800×600 na escala uniforme), com passo de 99×92 (`ControlBar.wnd`).
  - Margens de 11–12; teto de 20%, que é o do ZH (`GlobalData.cpp:846`).
- **Mundo:** desenhado na tela inteira (opção iii).
  - Perda medida: 6,6% de chão, só a faixa perto. Tanque do mesmo tamanho.
  - Todo "na tela" usa a área do mundo (problema 6), e o pulo da câmera centraliza no meio dela (41% da altura).
- **Minimapa: 25% da altura**, o radar do ZH (152 de 600). A ponta sobe 76 px e cobre 0,28%. Fora do losango, o clique vai para o mundo.
- **Produção:** o painel direito do ZH, 254 de largura (141×109 em 800×600), sempre à vista (C&C3, Tempest Rising).
- **Seleção:** a sobra. Fica com 472 em 16:10.
- **Dica:** painel próprio acima da grade, sem atraso (ZH `TOOLTIPDELAY=1`). Traz nome, tecla em amarelo (`GlobalData.cpp:923`) e o motivo de estar indisponível (`ControlBarPopupDescription.cpp:300-345`). Aparece por cima do mundo só enquanto o mouse está no botão (estimativa ~1,9%, a medir).
- **Objetivo e ajuda:** saem do meio e de baixo e vão para a área de avisos, no canto superior esquerdo (ZH, `InGameUI.ini`).

**Por seleção:**

| Seleção | Grade | Painel |
|---|---|---|
| Nada | some (`ControlBar.cpp:2182-2228`) | grupos |
| 1 tanque | Atacar andando (casa 11), Guarda (13), Parar (14) | retrato, nome, vida em número e barra, o que está fazendo, recarga (`World.h:75-96`) |
| Vários | interseção por casa (`ControlBarMultiSelect.cpp:113-190`) | ícones com vida; clique pega só aquele, Shift+clique tira (SC2, CoH2) |
| Fábrica | Ponto de encontro (casa 13) | Construir fica só no bloco de produção |

**Do ZH:** barra única em blocos, casas fixas, borda por tipo (cores da USA em `ControlBarScheme.ini`), relógio na fila, disparo ao apertar, moldura por facção.

**Moderno:** escala pela resolução, tecla impressa no botão (SC2 1.2), vida no painel, grupos visíveis, produção sempre à vista.

**C) Passos** (cada um termina com fotos lado a lado mandadas como imagem ao Felipe, tiradas pela captura interna `RTSAutoTest.cpp:540`, em 1600×900 e 1920×1080 com `-ForceRes`, sempre pela fila da placa)

1. **Texto nítido** (em curso, à parte). Só depois de aprovado se mexe no HUD.
2. **Esqueleto invisível, regras de entrada e área do mundo.**
   - O `FindButton` procura primeiro nos widgets.
   - **Você vê:** nada muda.
   - **Teste automático:** o atual inteiro passa, mais:
     - duplo clique sobre um painel sem ordem;
     - botão apagado sem ordem;
     - A, S e Espaço depois do clique;
     - voltar ao menu e jogar de novo com um HUD só.
3. **Minimapa como widget**, igual ao aprovado (30%, mesmo lugar).
   - A/B de foto com a versão aprovada.
   - Passam `minimapa_camera` e `minimapa_ordem` (`RTSAutoTest.cpp:365-430`).
4. **Barra com a grade, Atacar andando e Parar.** É o primeiro "mais controles".
   - **Você vê:**
     - os botões com a letra no canto e a dica;
     - o minimapa encaixado, já em 25%, com foto lado a lado de antes e depois.
   - **Teste clicando:**
     - Parar para o tanque;
     - Atacar andando muda o cursor;
     - clique na barra não anda;
     - caixa que começa no mundo e termina na barra seleciona.
   - **Teste automático:**
     - botão e tecla dão o mesmo comando no replay;
     - `clique_na_barra_nao_ordena`, `caixa_ate_a_barra`, clique direito no painel.
   - Mais o custo medido.
5. **Topo, avisos, objetivo e ajuda em Slate**, no canto do ZH. A seta e os avisos usam a área do mundo.
6. **Painel da seleção.**
   - Passam a ir para o desenho: a ordem, o alvo e a recarga.
   - Retrato capturado pela ferramenta.
   - Teste: clicar num ícone pega só aquele tanque; Shift+clique tira.
7. **Grupos visíveis**, com as regras do ZH: Ctrl+número sem seleção limpa; um tanque fica num grupo só.
   - Clicar na aba seleciona; dois cliques levam a câmera.
8. **Guarda:** comando na simulação, botão na casa 13 e tecla G.
   - `simtest.ps1` com o replay idêntico.
   - Vídeo do tanque saindo e voltando.
9. **Pausa, resultado, telas de espera, carregando, menu e Opções em Slate.**
   - Opções com os presets baixa, média, alta e ultra.
   - Um sistema de texto só em tudo.
10. **Produção**, quando a fábrica existir.
11. **Moldura da facção**, quando chegar a arte.
12. **(Desempenho, opcional) recorte fora do eixo:** −18% de pixels, com A/B mostrando a imagem acima da barra idêntica.

**D) Comandos que entram já**

- **Atacar andando:** casa 11, tecla A (já é hoje, `RTSPlayerController.cpp:248`).
- **Parar:** casa 14, tecla S.
- **Guarda:** casa 13, tecla G. A letra do ZH não foi conferida, porque o `generals.csf` é binário.
  - Clique no chão: ataca o que o time vê dentro de 1,8× a visão (71,1 m), persegue até 2,2× (86,9 m) ou 10 s e volta. Procura a cada 0,5 s, e a cada 1 s na volta (`AIData.ini` do ZH 1.04).
  - Clique num tanque seu: segue e protege (`GUICommandTranslator.cpp:225-250`). Teste próprio.
  - O círculo tem o raio do ZH (`InGameUI.cpp:1466-1467`). Como passa da tela, o teste olha o comportamento, não o círculo inteiro.
  - Difere do parado de hoje, que atira no que entra no alcance e só persegue quem atirou nele, até 1,5× a visão (`World.cpp:859-873`).
- **Ataque forçado (Ctrl) e pontos de passagem (Alt):** fase dos atalhos.
- **Fora:** Dispersar, Guarda sem perseguir e Vender.

**E) Dependências e o substituto enquanto não chegam**

- **Fábrica na simulação** (`BASE_JOGAVEL.md` seção 1):
  - produção no bloco do painel direito;
  - clique soma 1, Shift soma 5 (BAR);
  - clique na fila cancela (`ControlBarCommand.cpp:546-689`);
  - relógio mais a % (`:783-796`);
  - ponto de encontro pelo clique no chão (`CommandXlat.cpp:2334-2336`) e pela casa 13;
  - aviso "tanque pronto".
  - Tamanho da fila: proposta 9 (`ControlBar.h:418`), que é a decisão 9 dele. O painel aceita qualquer tamanho.
  - Até lá, o bloco mostra a arte lisa da facção (`ControlBar.cpp:2707-2717`).
- **Ícones:** pedido à sessão de arte. Enquanto isso, palavra e letra, sem ícone falso.
- **Retrato:** captura pela ferramenta do modelo atual, refeita quando chegar o tanque final.
- **Moldura:** pedir à sessão de arte a folha de referência (`BASE_JOGAVEL.md:240`). Até lá, painel liso na cor da USA com a borda por tipo do ZH.
- **Decisões dele que já estão na lista:** vitória (5), minimapa sempre ligado ou com radar (8) e fila (9). O layout funciona com qualquer resposta.
- **Fonte final:** uma linha (`SlateFontInfo.h:246`) mais `CREDITOS.md`.
- **Som e voz:** anotados na lista.
- **Rede:** "Render-se" escondido; o topo montado para N jogadores.
- **Coordenação:** marcar os itens em `docs/BASE_JOGAVEL.md`, e a regra da barra de vida no mundo vai ao dono da "Seleção final".

**F) Decisão do Felipe** (só esta é genuinamente dele)

1. **Janela de 1600×900 ou tela cheia sem borda de 1920×1080?** **Decidido pelo Felipe (09/10): "tela cheia sempre".**
   - Na janela, a escala de 0,833 deixa o corpo do texto em 15,3 px. Em tela cheia são 18,4 px, com 44% mais pixels pra placa.
   - O `game.ps1 view` abre em tela cheia sem borda, na resolução do monitor. O teste automático continua em 1600×900; as fotos pra ele saem também em 1920×1080.

Decidido com fonte, e desfaço se ele rejeitar:
- barra opaca por cima, com a câmera intacta;
- teclas por letra (A, S, G);
- produção sempre à vista;
- grade 7×2 do ZH.

**G) Fontes**

**ZH (código GPL, `C:\Users\luisf\newcnc\GeneralsGameCode`):**
- `ControlBar.cpp:2182-2228, 2707-2717, 3825-3828`
- `ControlBar.h:410, 418`
- `GlobalData.cpp:846, 923`
- `W3DView.cpp:208-217, 241`
- `ControlBarMultiSelect.cpp:113-190`
- `ControlBarCommand.cpp:546-689, 783-796`
- `HotKey.cpp:161-191, 203`
- `GUICommandTranslator.cpp:225-250`
- `CommandXlat.cpp:2334-2336`
- `AIGuard.cpp:203-209`
- `InGameUI.cpp:1466-1467`
- `GeneralsMD/.../Common/RTS/Player.cpp:3701-3717`
- `ControlBarPopupDescription.cpp:300-345`
- `GadgetPushButton.cpp:72`

**Dados do ZH 1.04 (TheSuperHackers/GeneralsGamePatch, `Patch104pZH/GameFilesOriginalZH`):** `Data/INI/Default/AIData.ini`, `ControlBar.wnd`, `ControlBarScheme.ini`, `CommandSet.ini`, `InGameUI.ini`.

**Unreal 5.8:**
- `BaseEngine.ini:1474-1475, 2900`
- `CameraStackTypes.cpp:219-224, 286-333`
- `CameraTypes.h:129`
- `CameraComponent.cpp:81`
- `SlateFontInfo.h:15, 246`
- `ElementBatcher.cpp:1256-1268, 3814-3817`
- `SlateApplication.cpp:6022-6070`
- `SWidget.cpp:491`
- `SButton.cpp:89, 293-311, 351-402`
- `SceneViewport.cpp:973-996`
- `PlayerInput.cpp:461-464, 2111-2127`
- `SlateUser.cpp:52-56`
- `GameEngine.cpp:417-420`
- `GameViewportClient.cpp:462-481`
- `CommonUIActionRouterBase.cpp:1866-1912`
- `Engine/Content/Slate/Fonts/Roboto-*.ttf`, medidas próprias

**Projeto:**
- `RTSPlayerController.cpp:54-58, 248, 286, 302, 400-466, 496, 607, 641, 670`
- `RTSHUD.cpp:400-489, 680-690`
- `RTSAutoTest.cpp:150-200, 365-430, 499-515, 540`
- `World.cpp:859-873`
- `World.h:75-96`
- `DefaultGame.ini:14-20`
- `game.ps1:20, 136`
- `docs/BASE_JOGAVEL.md`: seções 0, 1, 5 e 6, linhas 52, 145-149, 162, 172, 240, 365
- `docs/JOGO.md:59-66, 247`
- `SForgePanel.cpp:796-814`
- `ForgeUITest.cpp:99-105`
- `ForgePlayerController.cpp:58-61`
- `C:\Users\luisf\newcnc\docs\adr\0021-hud-readability-layer.md:71-76, 101-102, 127-146`

**Modernos:**
- XAG 101 (definição da altura do corpo) e XAG 102
- WCAG 2.5.5
- SC2 1.2 (co-optimus)
- Stormgate 0.3.0
- C&C3 (WorthPlaying)
- Tempest Rising (GameWatcher)
- BAR GRID

**A conferir:** a letra G do ZH (o `generals.csf` é binário e baixá-lo pede permissão), e o fator de humor da IA (`AI_VISIONFACTOR_MOOD`), que pode mudar o raio da Guarda.
