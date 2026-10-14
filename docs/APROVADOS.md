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
| 13/10 | O asfalto da volta 15 (no rumo, com variações por mapa no WorldForge depois) | "boa, ficou bem melhor a estrada" | `docs/aprovados/look/2026-10-13_asfalto_volta15.jpg` | `docs/RECEITAS_DO_LOOK.md` §5 | Look |
| 13/10 | **A cratera v2** (a bacia, o halo, os torrões, as pedras), provada na maquete; no jogo depende do relevo (v3) | "caralho, mas já ficou bom pra kct" | `docs/aprovados/look/2026-10-13_cratera_v2.jpg` | `docs/RECEITAS_DO_LOOK.md` §2; `docs/CHAO_DEFORMAVEL.md` | Look |
| 13/10 | **A estrada com volume** (a laje de 12 cm em cima da areia, com a borda quebrada), em 3 tipos: intacta com areia na beira, gasta (quebra em trechos), destruída (a borda toda). Protótipo da maquete. **Nunca foi pro jogo:** em 14/10 ele procurou na vila do jogo e não achou. Vai pra v3 (a v2 não tem mais ajuste) | "gostei da bastante quebrada, e eu queria uma opção sem quebrada igual 14 e 12, é uma possibilidade termos todas opções na construção do mundo" | `docs/aprovados/look/2026-10-13_estrada_volume_3tipos.jpg` | `docs/RECEITAS_DO_LOOK.md` §6 (`look_ground.py`, `road_slab`) | Look (o kit) + WorldForge (o traçado) |
| 13/10 | **REPROVADO:** borrar a foto aérea do asfalto (AER_BLUR) pra tirar a repetição | "regressão do krl, a versão original tá tão boa" | `docs/aprovados/look/2026-10-13_asfalto_borrado_REPROVADO.jpg` | `docs/RECEITAS_DO_LOOK.md` §5 | Look |
| 13/10 | **Pendente → v3 (14/10, o Felipe: "pra v2 não vai ter mais nenhum ajuste, isso aí tudo vai ser pra v3"):** as lagartas e as crateras no jogo como a areia "C" e a cratera v2 (hoje saem como risco chapado e mancha). O afundar de geometria ficou pra v3 (fura no zoom perto). | "eu só queria as lagartas funcionando igual como a gente mapeou" | o alvo: `docs/concept/ground/tracks_sand_C_approved.jpg` | `docs/CHAO_DEFORMAVEL.md` (o topo) | Look |

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
| 04–06/10 | O simulador WebGL: **a referência visual aprovada** das explosões e da fumaça (marcos v1 a v6) | as tags `v1-antes-composicao-conjunta` … `v6-fumaca-interacao` | *falta a imagem dentro de `docs/`* (o simulador: `reference/webgl/`, congelado na v6) | `docs/ARCHITECTURE.md`; `docs/MIGRACAO.md` | Fluxo (BlastLab) |
| 07/10 | A coluna de fumaça da JangaFX na morte de tanque | "tá boa" | *falta a imagem* | `docs/STATUS.md` ("Uma fumaça só na morte de tanque") | Fluxo (BlastLab) |
| 11–12/10 | O BlastLab rodando liso e bonito: sombra do fogo por campo de distância, presets | "acho que dá pra aprovar" | *falta a imagem* | `docs/STATUS.md` ("Rodar liso e lindo") | Fluxo (BlastLab) |
| 13/10 | **A morte de tanque da v2, SEM o fogo da carcaça:** a bola de fuligem cinza-carvão e a areia que sobe (`eg_tq_v2`), a torre arremessada pro lado livre, os destroços com rastro, a carcaça queimada. Só pra tanques; as construções ficam com as explosões antigas. | "ok ficaremos sem o fogo na v2, ficaremos desse jeito aí pra explosão de tanques" | `docs/aprovados/explosoes/2026-10-13_morte_tanque_v2.jpg` (o vídeo: `captures/gravacao_so_explosao.mp4`) | `docs/STATUS.md` ("A morte de tanque pelo combustível"); no jogo `Game/Source/BlastFx/Private/BlastFxDirector.cpp` (TankDeath) | Fluxo (BlastLab) |
| 13/10 | **Regra da cor:** a fuligem de tanque é cinza-carvão neutra; o amarelo é só a areia que a onda levanta | "a fumaça e fuligem de um tanque tem uma cor específica... o que é amarelo é a areia" | a mesma acima | `docs/STATUS.md` (`-RmSoot=1.0,0.97,0.94,0.30`) | Fluxo (BlastLab) |
| 13/10 | **REPROVADO:** a fumaça preta e as esteiras voando junto com a torre | "compara a porra das cores"; "que porra é essa da lagarta voando?" | — | `docs/STATUS.md` (a bronca da gravação) | Fluxo (BlastLab) |
| 13/10 | **REPROVADO:** o quadrado escuro (a chapa da sombra de contato queimada) | "esse quadrado ridículo do nada aparecendo" | — | `docs/STATUS.md` | Fluxo (BlastLab) |
| 13/10 | **REPROVADO, vai pra v3:** o fogo da carcaça depois da explosão (dois efeitos que se distinguem, pulso de brilho, um retângulo em cima do casco, rápido demais, baixa resolução) | "dá pra distinguir muito fácil que são 2 tipos de efeitos"; "tá spawnando de cima do tanque, um retângulo" | — | `docs/STATUS.md` (defeitos abertos: fumaça creme, faixas, topo achatado, coluna × tapar unidades) | Fluxo (BlastLab) |
| 14/10 | **Pendente:** no jogo, os destroços com rastro, a sombra da explosão no chão e a névoa do mapa na explosão (sem eles a fumaça "parece um PNG" no jogo) | "coloca rastro de fumaça também, é tudo que eu pedi ali que a gente homologou" | a folha antes/depois, quando ele vir | `Game/Source/BlastFx` (BlastDebris, BlastRaymarchSubsystem) | Fluxo (BlastLab) |
| — | Os tipos de explosão (conceito) | — | `docs/concept/explosions/` | `docs/CONCEITO_EXPLOSOES.md` | sessões de explosão (preencher o estado de cada uma) |

## Unidades, construções e modelos provisórios

| Data | O quê | A frase dele | Imagem | Receita | Sessão |
|---|---|---|---|---|---|
| — | As fichas aprovadas (tanques, Dozer, War Factory das 3 facções) | — | `docs/concept/units/`, `docs/concept/buildings/` | `docs/CONCEITO_UNIDADES.md`, `docs/CONCEITO_CONSTRUCOES.md` | Arte |
| 13/10 | **O Dozer USA provisório** por script (a arte aprovada `docs/concept/units/usa_dozer.webp` virou modelo; medido contra o tanque: tão comprido, mais baixo; a sirene amarela no teto da cabine é pedido dele) | "gostei do trator, passa pro jogo usar" | `docs/aprovados/modelos/2026-10-13_dozer_arte_x_nosso.jpg` (a arte e o nosso no mesmo ângulo) e `docs/aprovados/modelos/2026-10-13_dozer_no_jogo.jpg` (dentro do jogo, ao lado do tanque) | `Game/Tools/model_usa_dozer.py`; `docs/MODELOS_PROVISORIOS.md` (Dozer: medidas, pivôs, `game.ps1 dozer`) | Modelos 3D |
| 13/10 | **O tanque GLA provisório** por script (T-55 com o kit do GLA pela ficha `docs/concept/units/gla_mbt_sheet.webp`; entra no jogo quando a facção GLA existir) | "top, salva, commita" | `docs/aprovados/modelos/2026-10-13_tanque_gla_ficha_x_nosso.jpg` (a ficha e o nosso, vista a vista) | `Game/Tools/model_gla_tank.py`; `docs/MODELOS_PROVISORIOS.md` | Modelos 3D |
| 14/10 | **O caminhão de suprimento da USA provisório** por script (sem arte: desenho pela raiz da USA, caminhão tático 6×6 com o guindaste amarelo atrás da cabine; **escala 0,72**, ~60% do tanque, depois da bronca "um tanque é pra ser grande") | "agora sim" (e antes: "o design tá bonito") | `docs/aprovados/modelos/2026-10-14_caminhao_suprimento_usa_com_tanque.jpg` (ao lado do tanque) e `..._34.jpg` | `Game/Tools/model_usa_supply_truck.py`; `docs/MODELOS_PROVISORIOS.md` (vagas da carga 2×2, caixa pequena) | Modelos 3D |
| 14/10 | **A defesa de base da USA (a Patriot) provisória** por script: a caixa lançadora de 2×2 células inclinada girando no bloco octogonal blindado, o radar no mastro, a faixa de perigo no chão (a fórmula do ZH + a bíblia da USA, sem arte) | "top, manda pros cara botar no jogo" | `docs/aprovados/modelos/2026-10-14_patriot_usa_34.jpg` e `..._com_tanque.jpg` | `Game/Tools/model_usa_patriot.py`; `docs/MODELOS_PROVISORIOS.md` (peças, pivôs, slots) | Modelos 3D |
| 14/10 | **Supply Dock: REPROVADOS** (4 rumos): o monte de caixas dos game shots ("simples demais"); a zona de lançamento aéreo e o avião que repõe ("vai fuder a mecânica: o depósito tem quantidade limitada, já começa no mapa"); o pátio de contêineres ("ridículo, não lê como suprimento"); a pirâmide de caixas ("já reprovei"). O que ele quer: uma ideia poderosa, entendível e icônica como no ZH, estoque finito e visível | "quero uma ideia poderosa, que encaixa bem com cenários, de arrepiar, pensar fora da caixinha" | `captures/game/models/supply_dock/preview/` (a pirâmide, fora do git) | `docs/MODELOS_PROVISORIOS.md`; a folha de ideias v3 em `captures/concept/_attach/supply_dock/PROMPT.md` | Modelos 3D |
| 13/10 | **Command Center: REPROVADO** | "tá horrível aquilo, mas depois a gente vê isso" | — | os provisórios da base: `blender_provisional.py` | jogo |

## HUD

| Data | O quê | A frase dele | Imagem | Receita | Sessão |
|---|---|---|---|---|---|
| 10/10 | O HUD peça por peça (a barra do ZH modernizada) | em `DECISOES.md` | `docs/concept/hud/` | `docs/concept/hud/DECISOES.md`, `docs/HUD.md` | Arte |
| 13/10 | O HUD novo no jogo (`-NewHud`): **não aprovado, ele ainda não viu jogando** | "eu nem vi o HUD novo, nem interagi" | — | `HUDLab/INTEGRACAO.md` | Arte e jogo |

## Luz e noite

| Data | O quê | A frase dele | Imagem | Receita | Sessão |
|---|---|---|---|---|---|
| 13/10 | Noite: o fogo pinta as unidades, opção **A** (a China bege perto da explosão) | escolheu a A | `docs/aprovados/look/2026-10-13_noite_A_fogo_pinta_unidades.jpg` | memória "Noite: o fogo pinta as unidades"; a receita `desert_night.json` | Look |
| 13/10 | A luz A (a da bancada do Look) aplicada no jogo | "esses gráficos aqui é qual? são bem bonitos" | `docs/aprovados/look/2026-10-13_luz_A_no_jogo.jpg` | `docs/RECEITAS_DO_LOOK.md` §8 | Look |
| 13/10 | **REPROVADA:** a luz B, calibrada pela cartela de cores (a gradação zerada): sépia, sem alta-luz | "ambas doem meus olhos ainda, ambas não são bonitas" (e, no jogo, a A ganhou) | `docs/aprovados/look/2026-10-13_luz_B_calibrada_REPROVADA.jpg` | `docs/RECEITAS_DO_LOOK.md` §8 | Look |
| 13/10 | O horário da hora dourada: **sol a 8°**, em contraluz pela direita, na câmera inicial de verdade | "8 graus" | `docs/aprovados/look/2026-10-13_hora_dourada_4_horarios_escolhido_8graus.jpg` (8° em cima à esquerda) | `docs/RECEITAS_DO_LOOK.md` §8 | Look |
| 13/10 | **A luz da v2:** a poeira dourada no alto do quadro, densidade **0,06** (`desert_golden_dust`, a padrão do World_TestSalt2) | "eu quero a 0,06" | `docs/aprovados/look/2026-10-13_poeira_dourada_escolhida_006.jpg` e `..._luz_v2_desert_golden_dust_no_jogo.jpg` | `docs/RECEITAS_DO_LOOK.md` §8; `Game/Tools/look_recipes/desert_golden_dust.json` | Look |

## Jogo (o que não é imagem)

| Data | O quê | A frase dele | Onde | Sessão |
|---|---|---|---|---|
| 11/10 | O multiplayer da v1 | aprovado (tag `jogo-v1-multiplayer`) | `docs/MULTIPLAYER.md` | jogo |
| 10/10 | A base do ZH: "perfeito pra v1 beta" | "perfeito pra v1 beta" | `docs/BASE_JOGAVEL.md` | jogo |

---

**Onde falta (cada sessão completa a dela):** as linhas com *falta a imagem* ou *falta a frase*, e tudo que foi aprovado e ainda não está aqui. A sessão dona copia a imagem pra `docs/aprovados/<área>/` e corrige a linha.

## Mundos (WorldForge: mapas, vila, estrada, água, depósitos)

| Data | O quê | A frase dele | Imagem | Receita | Sessão |
|---|---|---|---|---|---|
| 13/10 | **Asfalto v2 em zonas do Look no mapa do jogo, sem as trilhas de roda** (o v2d, padrão `ASPHALT2=1`) | "a qualidade do novo asfalto tá surreal de bom, mas essas trilhas aí não sei não, você não vê isso no dia a dia [...] já o resto tá top" | `docs/aprovados/mundos/2026-10-13_asfalto_v2_sem_trilhas.jpg` | `docs/WORLDFORGE.md` §12.2; `Game/Tools/look_asphalt.py` (`ASPHALT_V2_CODE`) | WorldForge (material: Look) |
| 13/10 | REPROVADO: as trilhas de roda no asfalto v2 | (a mesma frase) | `docs/aprovados/mundos/2026-10-13_asfalto_v2_trilhas_reprovadas.jpg` | — | WorldForge |
| 13/10 | REPROVADO: o asfalto da volta 15 do Look no mapa | "a qualidade do asfalto do outro ali, a textura tava melhor, senti uma regressão" | `docs/aprovados/mundos/2026-10-13_asfalto_volta15_reprovado.jpg` | — | WorldForge |
| 13/10 | REPROVADO: a foto aérea borrada por cima do asfalto (`AERBLUR`, desligado) | "regressão do caralho, tudo borrado, simples, pode deixar antes mesmo com a marca de pneu" | `docs/aprovados/mundos/2026-10-13_asfalto_aerea_borrada_reprovada.jpg` | — | WorldForge |
| 13/10 | **As manchas de óleo v4** no asfalto | "ok, dá pra manter" | `docs/aprovados/mundos/2026-10-13_oleo_v4.jpg` | `docs/WORLDFORGE.md` §12.2 | WorldForge |
| 13/10 | **A poça brilhante com o contorno suavizado** (rara; `OILPUDDLE=1` força) | "boa, pode ser" | `docs/aprovados/mundos/2026-10-13_poca_brilhante_contorno.jpg` | `docs/WORLDFORGE.md` §12.2 | WorldForge |
| 13/10 | **O oásis verde:** a terra úmida verde-oliva de 2 a 16 m da água, menos tufo e mais moita (`OASIS2=1`) | "ok, ficou bom" | `docs/aprovados/mundos/2026-10-13_oasis_verde.jpg` | `docs/WORLDFORGE.md` §12.2; `build_slice.py` (`oasis2_scatter`, nó `wet`) | WorldForge |
| 13/10 | **Os depósitos do jogo e as mini vilas em volta deles** (a planta 2D; o rumo, não as casas) | "top, pode ser, gostei, segue" | `docs/aprovados/mundos/2026-10-13_depositos_e_mini_vilas_planta.jpg` | `WorldGenerator.cpp` `PlaceResources` (os depósitos das folgas < 60 m); `village.json` `supply_docks` | WorldForge |
| 13/10 | REPROVADO: a mini vila v1 (casas soltas em volta do pátio). Refeita com rua de terra e lotes, na gramática da vila do centro (v2, ainda sem a palavra dele) | "tá fraco essa mini vila, parece cabanas só jogadas aí de forma porca, sem qualidade" | `docs/aprovados/mundos/2026-10-13_mini_vila_v1_reprovada.jpg` | `WorldVillage.cpp` `BuildHamlets` | WorldForge |
| 13/10 | REPROVADO: a estrada entre o lago oeste e o maciço sem acesso (os dois lados). Corrigido com o acostamento e o talude pelo desnível (`RoadCut`), ainda sem a palavra dele | "hoje nem dá pra acessar lá, tá mais econômico ir por baixo das montanhas ao invés dessa estrada" | `docs/aprovados/mundos/2026-10-13_estrada_sem_acesso_print.jpg` (o print dele); a sonda antes e depois: `docs/aprovados/mundos/2026-10-13_estrada_acesso_correcao.jpg` | `WorldDetail.h` `RoadCut` | WorldForge |
