# Aprovados: o índice único

O pedido do Felipe (13/10/2026, noite): "ninguém tá documentando imagens aprovadas nem docs como a gente fazia no começo".
Exemplo do problema: a sessão do Look não sabia qual imagem usar pras lagartas no chão. A aprovada existia
(`docs/concept/ground/tracks_sand_C_approved.jpg`), mas nada levava até ela. E o alvo principal, a `r1_raid_slabs`, só
existia em `captures/`, que fica fora do git e vai pra Lixeira quando o disco aperta.

**A regra (está no `CLAUDE.md`):**
- **Aprovou, registrou na hora:** quando o Felipe aprova uma coisa (ou escolhe entre opções, ou reprova), a sessão dona registra no mesmo turno.
  - A imagem aprovada vai pra dentro do repositório: `docs/aprovados/<área>/AAAA-MM-DD_<nome>.jpg`, em jpg leve, até ~500 KB.
  - Vídeo vira 1 a 3 quadros, mais o caminho do mp4.
  - Imagem de conceito continua indo pra `docs/concept/`.
  - A linha entra aqui com seis partes:
    - a data;
    - o quê;
    - a frase dele, entre aspas;
    - a imagem, num caminho dentro de `docs/`;
    - onde está a receita (o como, os números, os arquivos);
    - a sessão dona.
- **Antes de mexer em qualquer coisa,** ler a área dela aqui e abrir a imagem aprovada.
- **`captures/` não é registro:** é rascunho fora do git. Nada aprovado mora só lá.
- **Reprovado também entra,** pra ninguém repetir.

O como de cada visual continua nos documentos de cada área: `docs/RECEITAS_DO_LOOK.md`, `docs/POEIRA_DAS_LAGARTAS.md`, `docs/FXLAB.md`, `docs/concept/hud/DECISOES.md`... Este índice só aponta pra eles, com a imagem.

---

## Alvos (as cenas que o jogo tem de bater)

| Data | O quê | A frase dele | Imagem | Receita / onde se usa | Sessão |
|---|---|---|---|---|---|
| 11–13/10 | **O alvo do deserto:** os game shots Gemini da base, sobretudo a `r1_raid_slabs`. As 7 da `best/` (11/10) eram conceito inicial; as do GPT são inspiração. | (memória "Alvos do deserto", 13/10) | `docs/aprovados/alvos/r1_raid_slabs_gemini.webp` (cópia de `captures/concept/44_base_gameplay/`) | `captures/concept/44_base_gameplay/NOTAS.md` | Arte |
| 06/10 | O vídeo de gameplay de referência: War Factory, tanques e poeira atrás de cada um. A poeira nasce onde este vídeo mostra. | referência dada por ele; "a confirmar" no `BASE_JOGAVEL.md` | `docs/aprovados/alvos/gameplay_v2_sheet.jpg` (o vídeo: `captures/concept/33_video/gameplay_v2.mp4`) | `docs/POEIRA_DAS_LAGARTAS.md` | Arte |
| 11/10 | As cenas do pôr do sol no deserto (o conceito inicial) | — | `docs/concept/scenes/desert_golden_hour/best/` | `docs/concept/AMBIENTES.md` | Arte |

## Chão

| Data | O quê | A frase dele | Imagem | Receita | Sessão |
|---|---|---|---|---|---|
| 12–13/10 | **A areia v2 "C"** com a trilha da lagarta em relevo e a trilha velha cruzando: o alvo das lagartas no chão | "vale ir com a C mais marcada" | `docs/concept/ground/tracks_sand_C_approved.jpg` | `docs/RECEITAS_DO_LOOK.md` §1; `docs/concept/ground/README.md` | Look |
| 13/10 | O asfalto da volta 15 (no rumo, com variações por mapa no WorldForge depois) | *falta a frase* | *falta a imagem* | `docs/RECEITAS_DO_LOOK.md` | Look |

## Poeira das lagartas

| Data | O quê | A frase dele | Imagem | Receita | Sessão |
|---|---|---|---|---|---|
| 13/10 | **A v11 é o caminho** (a gravação nova do EmberGen, a nuvem pela velocidade) | "v11 é o caminho!" | os quadros da linha de baixo (a v12 é a v11 com o rastro longo) | `docs/POEIRA_DAS_LAGARTAS.md` (Estado) | Arte |
| 13/10 | **O rastro longo** pro tanque pesado: denso até 1,2 tanque, some em 3 (`FXLab/tread_v12_ember.json`) | "isso pra esse tipo de tanque pesado o longo" | `docs/aprovados/poeira_lagartas/2026-10-13_comprimento_r1_longo_escolhido.jpg` e `..._comprimento_jogo_longo_escolhido.jpg` (embaixo, o LONGO) | `docs/POEIRA_DAS_LAGARTAS.md`; o plugin do jogo: `FXLab/Plugins/RTSTreadDust/INTEGRACAO.md` | Arte |
| 13/10 | A v1 (EmberGen) e o plano B feito à mão: **guardados, não aprovados** | "salva a do ember o plano b pq não ficaram tão ruim" | — | `FXLab/tread_versions/v1_13-10/` | Arte |

## Explosões e fogo

| Data | O quê | A frase dele | Imagem | Receita | Sessão |
|---|---|---|---|---|---|
| 09/10 | O fogo da carcaça no FXLab: **o melhor até agora, salvo, NÃO é o final** | "melhor até agora! deixa salvo..." | *falta a imagem dentro de `docs/`* | `docs/FXLAB.md` ("Aprovado") | Arte |
| — | Os tipos de explosão (conceito) | — | `docs/concept/explosions/` | `docs/CONCEITO_EXPLOSOES.md` | sessões de explosão (preencher o estado de cada uma) |

## Unidades, construções e modelos provisórios

| Data | O quê | A frase dele | Imagem | Receita | Sessão |
|---|---|---|---|---|---|
| — | As fichas aprovadas (tanques, Dozer, War Factory das 3 facções) | — | `docs/concept/units/`, `docs/concept/buildings/` | `docs/CONCEITO_UNIDADES.md`, `docs/CONCEITO_CONSTRUCOES.md` | Arte |
| 13/10 | **O Dozer USA provisório** por script (a arte aprovada `docs/concept/units/usa_dozer.webp` virou modelo; medido contra o tanque: tão comprido, mais baixo; a sirene amarela no teto da cabine é pedido dele) | "gostei do trator, passa pro jogo usar" | `docs/aprovados/modelos/2026-10-13_dozer_arte_x_nosso.jpg` (a arte e o nosso no mesmo ângulo) e `docs/aprovados/modelos/2026-10-13_dozer_no_jogo.jpg` (dentro do jogo, ao lado do tanque) | `Game/Tools/model_usa_dozer.py`; `docs/MODELOS_PROVISORIOS.md` (Dozer: medidas, pivôs, `game.ps1 dozer`) | Modelos 3D |
| 13/10 | **O tanque GLA provisório** por script (T-55 com o kit do GLA pela ficha `docs/concept/units/gla_mbt_sheet.webp`; entra no jogo quando a facção GLA existir) | "top, salva, commita" | `docs/aprovados/modelos/2026-10-13_tanque_gla_ficha_x_nosso.jpg` (a ficha e o nosso, vista a vista) | `Game/Tools/model_gla_tank.py`; `docs/MODELOS_PROVISORIOS.md` | Modelos 3D |
| 13/10 | **Command Center: REPROVADO** | "tá horrível aquilo, mas depois a gente vê isso" | — | — | quem fez preenche |

## HUD

| Data | O quê | A frase dele | Imagem | Receita | Sessão |
|---|---|---|---|---|---|
| 10/10 | O HUD peça por peça (a barra do ZH modernizada) | em `DECISOES.md` | `docs/concept/hud/` | `docs/concept/hud/DECISOES.md`, `docs/HUD.md` | Arte |
| 13/10 | O HUD novo no jogo (`-NewHud`): **não aprovado, ele ainda não viu jogando** | "eu nem vi o HUD novo, nem interagi" | — | `HUDLab/INTEGRACAO.md` | Arte e jogo |

## Luz e noite

| Data | O quê | A frase dele | Imagem | Receita | Sessão |
|---|---|---|---|---|---|
| 13/10 | Noite: o fogo pinta as unidades, opção **A** (a China bege perto da explosão) | escolheu a A | *falta a imagem* | memória "Noite: o fogo pinta as unidades" | quem fez preenche |

## Jogo (o que não é imagem)

| Data | O quê | A frase dele | Onde | Sessão |
|---|---|---|---|---|
| 11/10 | O multiplayer da v1 | aprovado (tag `jogo-v1-multiplayer`) | `docs/MULTIPLAYER.md` | jogo |
| 10/10 | A base do ZH: "perfeito pra v1 beta" | "perfeito pra v1 beta" | `docs/BASE_JOGAVEL.md` | jogo |

---

**Onde falta (cada sessão completa a dela):** as linhas com *falta a imagem* ou *falta a frase*, e tudo que foi aprovado e ainda não está aqui. A sessão dona copia a imagem pra `docs/aprovados/<área>/` e corrige a linha.
