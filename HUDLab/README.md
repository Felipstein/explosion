# HUDLab

O laboratório do HUD do jogo, feito pela sessão de arte (Unreal 5.8), separado do `Game/`.

- **O HUD** é o plugin `Plugins/RTSHud`: só desenha um `FRTSHudState` (dados puros, `RTSHudTypes.h`) no Canvas, nas posições da planta (`docs/concept/hud/wireframe/`, 1920×1080).
- **O laboratório** (`Source/HUDLab`) monta os 5 estados de teste da planta (`HudLabStates.cpp`) por cima de uma cena favorita do deserto.
- **As regras de cada peça** estão em `docs/concept/hud/DECISOES.md`, e os nomes em `NOMES.md`.
- **O kit:**
  - as molduras (a pele, `skin/`), os ícones e as fotos vêm direto de `docs/concept/hud/kit/`;
  - a fonte é a Barlow Semi Condensed (`Fonts/`, SIL OFL; `Fonts/OFL.txt`).

```
powershell -File HUDLab\Tools\hudlab.ps1 assets          # o fundo, o minimapa e os emblemas em HUDLab\LabAssets\
powershell -File HUDLab\Tools\hudlab.ps1 compile
powershell -File HUDLab\Tools\hudlab.ps1 shot -State all -NoQueue # captures\hudlab\state_1..6.png, sem janela
powershell -File HUDLab\Tools\hudlab.ps1 view            # janela; teclas 1 a 6 trocam o estado; o mouse acende e aperta
powershell -File HUDLab\Tools\hudlab.ps1 video -State 2 -NoQueue   # 3 s em movimento: captures\hudlab\video_state_2.mp4
```

Opções da foto:
- `-NoQueue`: sem a fila da placa (a foto é só Canvas, leve);
- `-Res 3440x1440`: outra tela;
- `-Faction china|gla`: a pele da facção;
- `-Cols 9`: a grade da planta (o padrão é 7);
- `-Measure`: o custo do HUD vai pro log (`Saved/Logs/HUDLab.log`, "custo do HUD").

**Telas:** a escala é a menor entre altura/1080 e largura/1920.
- Em 16:10 sobra espaço em cima e embaixo, e nada se sobrepõe.
- Mais largo que 16:9 (ultrawide), o HUD fica numa faixa 16:9 no meio, pro olho não atravessar a tela inteira. A seta de evento continua na borda de verdade.
- `FRTSHudPainter::Layout` e `MinimapHole` dão as contas pro jogo.

**Custo (13/10, Development, 1920×1080, CPU do Draw por quadro):**
- 1,1 a 1,3 ms;
- o texto é ~0,5 ms;
- as molduras e as formas são o resto.
- No Shipping deve cair pra perto da metade. Se precisar de mais, o caminho é desenhar as molduras paradas numa textura só quando o estado muda e redesenhar por cima só o que pulsa.

Os estados:
1. tanques selecionados;
2. War Factory cheia;
3. Dozer cheio;
4. prédio com melhorias;
5. nada selecionado, partida de 8;
6. o 1 com várias superarmas dos dois lados.

Na janela, o mouse acende o botão e o clique afunda; o READY respira, a superarma perto pulsa, a energia faltando pisca e o aviso entra deslizando. O vídeo mostra também o painel subindo.

A integração no jogo está em `INTEGRACAO.md`.
