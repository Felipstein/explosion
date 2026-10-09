# Gráficos: o look do jogo (começo da sessão de gráficos, 09/10/2026)

Documento de entrada da sessão de **gráficos**, escrito pela sessão de arte a pedido do Felipe.
O Felipe é o juiz de cada rodada.

## 1. O papel

**É seu:** o look do quadro inteiro.
- Luz: sol, céu, luz do céu, névoa e atmosfera.
- Exposição, curva de cor (tonemapper) e pós-processamento: bloom, exposição local, cor.
- O padrão de material do terreno e das unidades, pra combinar com a luz.

**A meta:** o jogo idêntico aos game shots aprovados, em cada hora do dia e em cada tipo de mapa.

**Não é seu:**
- explosões, que as sessões de explosão ainda estão fazendo;
- geração de mapa (WorldForge);
- código de jogabilidade (sessão do jogo);
- modelos 3D (o Felipe).

**O fim do trabalho, um padrão só reutilizado sem dor de cabeça:**
1. Montar o look uma vez, num cenário de referência.
2. Empacotar como um **plugin comum** usado por três:
   - o **jogo** (`Game/`);
   - o botão **"ver na versão final"** do WorldForge, que mostra o mapa com os gráficos do jogo sem abrir o jogo;
   - o **simulador de explosões** (BlastLab).

O **painel de montar do WorldForge continua com a luz leve de hoje**, nada realista. Não mexer nele.

## 2. As referências (`docs/concept/look/`)

**Só as do Gemini** (decisão do Felipe, 09/10): as do GPT são "fantasia" demais, saturadas e
dramáticas. Exceção: o game shot de perto, em que ele escolheu o do GPT ("gosto muito dos gráficos
do GPT, mas é bem fantasia; vamos tentar GPT"), com o do Gemini guardado. A "foto do jogo" oficial (`game_shot_usa_vs_gla.webp`) é do GPT: vale pra cor e leitura
dos tanques, **não** como alvo de luz. **Mapas-mundo e a gameplay de hoje ficam fora**, pra não
confundir: o foco é o look.

| Folha | O que é | Originais |
|---|---|---|
| `1_dia.jpg` | **Dia, golden hour: o padrão do jogo.** Câmera do jogo (oficial) e câmera deitada | `docs/concept/scenes/day_game_cam.webp` (**o alvo nº 1**), `day_tilt_cam.webp` |
| `2_noite.jpg` | **Noite aprovada:** luar azul forte deixando o campo todo legível, mais as poças de luz laranja do fogo | `docs/concept/scenes/night_game_cam.webp` (aprovada), `night_tilt_cam_gemini.webp` |
| `3_faccoes.jpg` | **Leitura das facções sob a luz** | `docs/concept/scenes/usa_vs_china_gemini.webp`, `china_vs_gla_gemini.webp` |
| `perto_camera_do_jogo_gpt.webp` | **Game shot de perto, o escolhido** (Felipe, 09/10): tanque, chão e luz em detalhe. A cor bate com o dia aprovado (saturação 0,49 × 0,50, brilho 0,38 × 0,38, contraste 0,18 × 0,18). **Ignorar o desfoque em cima e embaixo** (efeito de miniatura, não existe na câmera de jogo); a câmera dele está um pouco mais baixa e mais perto que a do jogo | `docs/concept/look/` |
| `perto_camera_do_jogo_gemini.webp` | Game shot de perto, a alternativa guardada: a altura de câmera do jogo e a mesma família das outras; o USA saiu com pintura azul grande (fora da regra) e menos detalhe | `docs/concept/look/` |

Também servem de referência:
- `docs/concept/towns/`: todas as casas e cidades recortadas das cenas;
- `docs/concept/buildings/`: War Factory e construções civis;
- `docs/concept/units/`: fichas dos tanques com a cor do jogador.

## 3. O que já está decidido sobre a luz (não reabrir sem o Felipe)

- **O dia é golden hour:** sol baixo e quente, sombra longa, poeira no ar, "luz cinematográfica,
  look de Unreal Engine 5". Tirar isso por "cores naturais" achatou a luz (contraste −20%, medido).
- **A noite é legível:** luar azul forte com o campo todo visível, mais as poças de luz do fogo.
  A noite "realista" escura engole as unidades e foi rejeitada.
- **Cores desbotadas e empoeiradas, nunca saturadas.** O chão e os prédios ficam mais calmos que
  as unidades.
- **As facções se leem pelo claro e escuro:**
  - USA cinza-claro com azul; China oliva escuro; GLA amarelo-areia;
  - medido: cidade cinza apaga a USA e deserto apaga o GLA (ponto fraco aceito; a interface ajuda);
  - a luz e a cor não podem destruir esse contraste.
- **Jogabilidade antes do realismo:**
  - **Câmera do jogo** (`Game/Config/DefaultGame.ini`): 37,5° de inclinação, 45° de giro, 50° de campo e 61,1 m de altura (de 31,6 a 81,6). O modo cinematográfico deita até 12°.
  - **Sombra:** o sol do WorldForge está a 20°, com sombra de ~2,7× a altura. Ela desenha o relevo sem cobrir unidades.
  - **Névoa, atmosfera e bloom:** não podem cobrir nem lavar as unidades.
  - Na câmera alta, o halo de ofuscamento atravessa o chão; o jogo já o tirou.
- **Desempenho:** o Ultra é o máximo, mesmo travando na 1660. Cada preset (baixa/média/alta/ultra)
  segura ~60 qps na placa que o escolhe. Todo número sai de medida.

## 4. O estado de hoje: 3 luzes diferentes

| Projeto | A luz de hoje |
|---|---|
| **BlastLab** | O porte do WebGL (`BlastLabEnvironment`): sol de 128 000 lux por hora, céu e névoa por tabela, exposição e curva de cor do WebGL (`M_TonemapWebGL`). O céu e a névoa aprovados vêm de um **bug aprovado** do WebGL, reproduzido por tabela; não "corrigir" sem o Felipe. |
| **WorldForge** (botão de qualidade real) | Luz física por bioma (`WorldForge/Tools/build_slice.py`): sol de 120 000 lux a 20°, `SkyAtmosphere`, luz do céu em captura ao vivo, névoa, exposição travada no EV100 físico (~13,6 no deserto), curva Filmic, bloom 0,25 e exposição local 0,6/0,9. |
| **Jogo** | A luz do mapa do WorldForge, com a curva padrão da Unreal. |

**A consequência:** a explosão aprovada no BlastLab sai no jogo como uma **nuvem branca estourada**,
porque a curva de cor do jogo é outra (`docs/JOGO.md`, teste t4).

Quando o plugin for pro BlastLab, as explosões aprovadas têm de ser comparadas de novo com a
versão aprovada. O que já foi aprovado não pode piorar.

O efeito do FXLab (o fogo da carcaça) já lê a luz do mapa (`FXLab/Source/FXLab/Public/FXSceneLight.h`):
- o sol depois da atmosfera e o céu da luz do céu, direto no material;
- o fogo multiplicado por 1/exposição.

É um exemplo de efeito que se adapta a qualquer luz.

## 5. O que falta de referência (o Felipe decide se gera)

- **Outras horas:** só existem golden hour e noite. Faltam meio-dia, amanhecer, nublado,
  tempestade de areia e chuva, se o jogo for tê-las.
- **Neve e temperado de perto,** na câmera do jogo, com unidades.
- **Interface sobre a luz** (HUD, minimapa, névoa de guerra): a sessão de arte vai gerar.

Quando precisar de referência nova, o Felipe chama a sessão de arte.

## 6. Como trabalhar

- **A placa é uma só (GTX 1660):** toda Unreal com janela ou render entra na fila (`docs/GPU.md`).
- **O WorldForge é só leitura:** pedidos à sessão dele por mensagem.
- **O BlastLab é da sessão "Fluxo de explosão realista AAA".** Combine antes de mexer.
- **O `Game/` é dividido com a sessão do jogo:** combine quais arquivos de luz e pós são seus antes
  de editar.
- **A prova:**
  - a mesma câmera de cada referência, lado a lado com ela, em imagem;
  - visto em movimento (vídeo), não só em foto;
  - o Felipe julga.
- **Pesquise antes de mudar o visual:** como os jogos AAA e a própria Unreal fazem, um plano curto
  com as fontes e só então código.
- **Commit só com o "aprovado" do Felipe.**
- **A lista viva da base jogável** é `docs/BASE_JOGAVEL.md`: marque lá o que tocar.
