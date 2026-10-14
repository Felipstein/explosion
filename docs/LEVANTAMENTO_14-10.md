# Levantamento de 14/10/2026: o que o Felipe pediu ou aprovou, contra o que está no jogo hoje

Feito em 14/10, de madrugada, só lendo: os documentos, as 12 transcrições (as falas dele de 10 a 14/10), a
configuração, o código e as fotos do jogo. Nada foi aberto na placa, nada foi mudado.

## O resumo, em linguagem simples

**São 100 itens** (pedidos e aprovações dele que dizem respeito ao jogo).

| No jogo? | Quantos |
|---|---|
| ✅ está no jogo | **50** |
| 🟡 pela metade (chegou diferente, ou chegou e ele reprovou, ou falta ele ver) | **22** |
| ❌ não chegou | **19** (7 deles foram combinados pra depois: v3, facção GLA, ideias, online; **12 deviam ter chegado**) |
| ❓ sem prova (precisa de uma foto do jogo pra saber) | **9** |

**O que esse número esconde.** Em quantidade, metade chegou. Mas o que chegou é sobretudo mecânica, rede,
controles, luz e modelos. O que ele mais repetiu é o que não chegou:
- Ele fez **9 pedidos 5 vezes ou mais**. Desses, **só 1 está inteiro no jogo** (o óleo do asfalto). **5 estão pela
  metade** (a cratera, a poeira, o HUD, a documentação, a otimização). **3 não chegaram** (a areia da maquete, as
  lagartas, a estrada 3D).
- As lagartas ele pediu **13 vezes**, a areia **10 vezes**, a estrada 3D **9 vezes**, a cratera **8 vezes** (mais
  uma pelos buracos de tiro).

**Dois achados que ninguém tinha dito a ele:**
1. **O chão do mapa do jogo é a areia ANTIGA, não a da maquete.** O mapa `World_TestSalt2` foi montado pela
   última vez em 14/10 às 00:40, e o registro da montagem diz `FORGE desert sand: classic`
   (`WorldForge/Saved/Logs/WorldForge-backup-2026.10.14-03.45.32.log`, linha 1639). O material do chão é o
   `M_SliceGround_world_desert_classic`. Desde 13/10 às 03:31 a sessão do WorldForge monta o mapa do jogo com
   `SAND=classic` ("remontado com a areia de antes"). O motivo era a luz antiga do jogo; a luz mudou na noite de
   13/10 pra da bancada do Look, e ninguém trocou a areia. Por isso "a textura não aparece em nenhum momento do jogo".
   O documento de receitas (`docs/RECEITAS_DO_LOOK.md`, linha 65) diz que "o WorldForge usa no mapa": é o mapa de
   teste, não o do jogo.
2. **A v2 do jogo não está salva no git.** O último commit do jogo é de 11/10 (`d827f6c`). Hoje há 60 arquivos do
   jogo alterados (~6 mil linhas) e 138 arquivos novos fora do git, entre eles a memória do chão, a poeira, o HUD
   novo, os destroços, os cenários de teste e a receita da luz aprovada. Ele pediu em 13/10 às 15:45 pra todos
   commitarem ("não quero perder nada feito até agora"); a sessão do jogo esperou ele ver ("como vou commitar se
   eu nem vi o resultado", 13/10 16:04). Se o disco cair, a v2 se perde.

**Os 10 piores** (o que ele mais pediu ou mais gostou e não chegou, ou chegou ruim):

| # | O quê | Quantas vezes | No jogo hoje |
|---|---|---|---|
| 1 | **As lagartas como na folha "C"**: sulco com relevo, garras, bordinha, a trilha velha cruzando | 13 | ❌ risco fino e chapado; ele empurrou pra v3 em 14/10 01:14, decepcionado |
| 2 | **A areia da maquete** (a textura e a ondulação da v2 "C") | 10 (a textura) e 3 (a ondulação) | ❌ o mapa é montado com a areia antiga |
| 3 | **A estrada 3D com a borda quebrada** (os 3 tipos) | 9 | ❌ o asfalto é pintado no chão |
| 4 | **A cratera v2 e os buracos de tiro** na qualidade da maquete | 8 e 1 | 🟡 sai uma bacia pintada, sem relevo ("no jogo ficou cagado") |
| 5 | **A poeira das lagartas como no vídeo de gameplay** | 5 (e mais 6 falas sobre ela) | 🟡 chegou, e ele achou "exagerada, jogo de 2012", entrando nos tanques |
| 6 | **O HUD aprovado peça por peça** | ~20 falas | 🟡 está no código e virou padrão às 01:09 de 14/10, mas ele nunca jogou com ele |
| 7 | **Os prédios provisórios "top e de arrepiar"** | 2 (e mais 3 sobre o Command Center e o depósito) | 🟡 o Command Center reprovado ("tá horrível"), o depósito ainda é a grade de caixas |
| 8 | **A Patriot** ("manda pros cara botar no jogo") | 2 | ❌ não existe no jogo |
| 9 | **A faixa pintada com areia por cima e a trinca cortando** | 2 | ❌ só na maquete |
| 10 | **Jogo liso, "otimização perfeita"** (60 quadros por segundo) | 5 | 🟡 hoje 39 a 46 quadros por segundo nas medidas da madrugada |

**A causa comum dos piores:** quase tudo foi aprovado **na maquete** (foto de estúdio, câmera a 45 m, render sem
pressa) e nunca foi provado **na câmera do jogo** antes da aprovação. Na passagem pro jogo o material passou por três
sessões (Look, WorldForge, jogo) e se perdeu: a ondulação desliga na distância do jogo, o relevo virou pintura, e o
mapa ficou com a areia antiga sem ninguém conferir.

---

## Como ler as tabelas

- **Datas** no horário dele (Brasil). "N vezes" = falas diferentes dele sobre a mesma coisa.
- **A frase** é dele, curta (até 15 palavras); "[...]" corta um pedaço.
- **A prova** é sempre do jogo: arquivo e linha, a configuração ou uma foto do jogo. Maquete e WorldForge não contam.
- As fotos do jogo usadas: `captures/game/v2_final_base/` (14/10, 01:01–01:04), `captures/game/lk_esteira_depois/`
  (01:08), `captures/game/look_ab_b_vila_foto_perto/03_cena_005s.png` (13/10 20:08), `captures/game/v2_crateras/`
  (00:43), `captures/game/fx_carcaca_v2_rec4/` (01:13), `captures/game/hud_base2/` (13/10 03:25) e a folha
  `captures/game/ab_parada_destrocos.jpg` (01:15). **Atenção:** o jogo foi recompilado às 01:37 de 14/10; o HUD
  novo como padrão e a explosão clássica dos prédios entraram depois dessas fotos.
- **Donos:** Jogo = "RTS game architecture e gráficos"; Look = "Look visual do RTS C&C"; WorldForge = "Gerador de
  mundo RTS"; Fluxo = "Fluxo de explosão realista AAA" (BlastLab); Arte = "C&C Zero Hour conceitual modernizado"
  (FXLab, HUDLab); Modelos 3D = "Modelos 3D para RTS AAA".

---

## 1. Chão e areia

| O que ele pediu ou aprovou | Data(s) e quantas vezes pediu | A frase dele | Imagem | No jogo? | A prova | Dono |
|---|---|---|---|---|---|---|
| A areia da maquete do Look no chão do jogo (a textura que ele chamou de "uma das melhores até agora", "salve pro WorldForge usar") | 11/10 23:52; 12/10 00:37, 01:02, 22:35, 22:38; 13/10 00:23, 23:32, 23:34; 14/10 01:23, 01:29. **10 vezes** | "aquela textura extremamente bonita [...] não aparece em nenhum momento do jogo" | `docs/concept/ground/tracks_sand_C_approved.jpg` | ❌ | O mapa do jogo é montado com a areia antiga: `WorldForge/Saved/Logs/WorldForge-backup-2026.10.14-03.45.32.log:1639` ("FORGE desert sand: classic", 14/10 00:40); `WorldForge/Tools/build_slice.py:2340-2342`; material `Game/Content/Slice/Materials/M_SliceGround_world_desert_classic.uasset`. Fotos `v2_final_base/02_base_inicio.png`, `08_usina_pronta.png`: areia lisa | WorldForge (montagem) + Look (material) |
| A ondulação da areia v2 "C" (a v1 como base, a ondulação rara e em lugares certos) | 12/10 22:40, 23:13; aprovou 13/10 00:12. **3 vezes** | "vale ir com a C mais marcada, gostei" | `docs/concept/ground/tracks_sand_C_approved.jpg` | ❌ | O mesmo registro (areia antiga, sem zonas nem ondulação). Mesmo na v2, a onda se desliga na distância da câmera do jogo (`docs/RECEITAS_DO_LOOK.md:55-63`). Foto `lk_esteira_depois/04_cena_005s.png`: sem ondulação | Look + WorldForge |
| Pedrinhas e arbustos secos espalhados ao acaso | 12/10 22:35. 1 vez | "essa geração de arbustos mortos e pedrinhas aleatoriamente também, tops" | — | ✅ | Registro da montagem do mapa, linhas 1925–2098 (pedras, galhos secos, capim, moitas); foto `v2_final_base/08_usina_pronta.png` | WorldForge |
| O chão sem padrão repetido | 10/10 22:28; 12/10 22:35, 22:57, 23:46. **4 vezes** | "cuidado no padrão muito padronizado, muito repetitivo na tela" | — | 🟡 | A areia antiga usa a foto aérea de 45 m contra a repetição (`build_slice.py:2496`); as zonas da v2, feitas pra isso, não estão no mapa do jogo | WorldForge + Look |
| O capim que não vire risco nem "pixel espalhado"; o capim do oásis de volta | 13/10 01:11, 01:15, 09:44. 3 vezes | "só parece um monte de pixel espalhado, nem dá pra distinguir que é capim" | — | ✅ | Foto `lk_esteira_depois/04_cena_005s.png` (tufos legíveis); a luz do tufo pelo chão (`docs/WORLDFORGE.md:1550-1552`). Ele não deu a palavra final no jogo | WorldForge |
| O oásis verde (aprovado) | 13/10. 1 vez | "ok, ficou bom" | `docs/aprovados/mundos/2026-10-13_oasis_verde.jpg` | ✅ | Registro da montagem do mapa do jogo, linha 1641 ("FORGE oasis2") | WorldForge |
| Menos amarelo, "coisa viva" como nos game shots | 13/10 15:19, 15:55. 2 vezes | "tá tudo muito apegado pro amarelo ainda, não tem coisa viva" | — | ❓ | Ninguém mediu de novo no jogo depois da luz nova | Look |

## 2. Estrada e vila

| O que ele pediu ou aprovou | Data(s) e quantas vezes pediu | A frase dele | Imagem | No jogo? | A prova | Dono |
|---|---|---|---|---|---|---|
| A estrada 3D: a laje com volume e a borda quebrada, em 3 tipos (intacta, gasta, destruída) | 13/10 10:45, 10:47, 13:13, 13:22, 13:39, 13:43, 13:47; 14/10 01:19, 01:23. **9 vezes** | "cadê as estradas 3D com as texturas tops na vila? não vi isso no jogo" | `docs/aprovados/look/2026-10-13_estrada_volume_3tipos.jpg` | ❌ | Foto `look_ab_b_vila_foto_perto/03_cena_005s.png`: o asfalto é pintado no chão. `docs/RECEITAS_DO_LOOK.md:217` ("só na maquete") | Look (o kit) + WorldForge (o traçado) + Jogo (a altura) |
| O asfalto v2, sem as trilhas de roda | aprovou 13/10 13:10. 1 vez | "a qualidade do novo asfalto tá surreal de bom" | `docs/aprovados/mundos/2026-10-13_asfalto_v2_sem_trilhas.jpg` | ✅ | Registro da montagem do mapa do jogo, linha 1688 ("asphalt Look v2 em zonas sobre o de antes") | WorldForge (material do Look) |
| As manchas de óleo v4 e a poça brilhante, raras e aleatórias, com respingos | 13/10 13:39, 13:46, 13:49, 14:44, 14:45, 15:28. **6 vezes** | "ok, dá pra manter" (o óleo); "boa, pode ser" (a poça) | `docs/aprovados/mundos/2026-10-13_oleo_v4.jpg`, `..._poca_brilhante_contorno.jpg` | ✅ | Dentro do asfalto v2 do mapa (linha 1688 do registro; `docs/WORLDFORGE.md:1327-1328`). Sem foto de perto no jogo | WorldForge + Look |
| A faixa pintada: areia por cima da faixa, a trinca cortando, as idades | 13/10 14:32, 15:19. 2 vezes | "areia em cima da faixa branca vai tampar a faixa branca" | (estudo em `captures/look/faixa_branca_estudo.jpg`) | ❌ | `docs/RECEITAS_DO_LOOK.md:238` ("em teste", só na maquete); `docs/BASE_JOGAVEL.md:377` | Look + WorldForge |
| Tipos de cor e composição da estrada por mapa, escolhidos no WorldForge | 13/10 09:44. 1 vez | "futuramente podemos criar mais tipos de cores [...] no WorldForge" | — | ❌ (combinado: depois) | Pedido pro futuro | WorldForge |
| A vila da maquete do Look no mapa do jogo (as casas, a rua, a estrada um pouco maior, a rua da vila um pouco menor) | 11/10 18:00, 19:55; 13/10 10:09, 10:11. **4 vezes** | "o mapa do Look foi sensacional, é eles mesmo que eu quero" | — | ✅ | `Game/Config/RTS/Villages/World_TestSalt2.json` (1106 peças: 60 casas térreas, 26 compridas, 18 com varanda, 5 sobrados, muros, carros); `Game/Content/Village/`; foto `look_ab_b_vila_foto_perto/03_cena_005s.png` | WorldForge + Look + Jogo |
| Os postes na direção certa, o fio bonito, nunca em cima de rua | 13/10 10:14, 10:15. 2 vezes | "cuidado colocar poste em cima de rua ou coisa, você já fez isso antes" | — | ✅ | `Villages/World_TestSalt2.json` (24 postes, 82 fios); foto `look_ab_b_vila_foto_perto/03_cena_005s.png`. A física do fio é da v3 | WorldForge |
| A cor das casas com menos contraste; o muro sem variar dentro do mesmo lote | 13/10 22:46. 1 vez | "essa aleatorização de cor nas casas ficou muito forte" | — | ❓ | `docs/WORLDFORGE.md:1310` ("Falta: o tom por casa no material do kit"); não achei código no jogo | Look + WorldForge |
| As mini vilas em volta dos depósitos | 13/10 21:19; aprovou a planta 21:46; reprovou a v1 22:48. 3 vezes | "top, pode ser, gostei, segue"; depois "tá fraco essa mini vila" | `docs/aprovados/mundos/2026-10-13_depositos_e_mini_vilas_planta.jpg` | 🟡 | `Villages/World_TestSalt2.json`: 432 peças nas vilas 10, 11, 14, 15, 18 e 20. É a v2, refeita depois da bronca, sem a palavra dele (`docs/APROVADOS.md:122`) | WorldForge |
| O acesso da estrada entre o lago oeste e o maciço | 13/10 21:46. 1 vez | "hoje nem dá pra acessar lá" | `docs/aprovados/mundos/2026-10-13_estrada_acesso_correcao.jpg` | 🟡 | A correção (`RoadCut`) é anterior à última montagem do mapa (14/10 00:40), mas não há foto do jogo nem a palavra dele | WorldForge |

## 3. Lagartas e crateras

| O que ele pediu ou aprovou | Data(s) e quantas vezes pediu | A frase dele | Imagem | No jogo? | A prova | Dono |
|---|---|---|---|---|---|---|
| As marcas de esteira, a 1ª versão ("salva esse modelo") | 12/10 01:35, 22:38. 2 vezes | "o chão onde o tanque passou já tá muito bom! salva esse modelo" | (`captures/game/esteira_v1_salva/`, fora do git) | ✅ | `Game/Source/RTSGame/Private/RTSGroundMemory.cpp` (arquivo novo, fora do git); fotos `lk_esteira_depois/` | Jogo |
| As lagartas como a folha "C": sulco com relevo, garras, bordinha, a trilha velha cruzando e a nova por cima | 12/10 22:38, 22:40; 13/10 00:12, 00:28, 00:33, 15:54, 23:13, 23:27, 23:28, 23:30, 23:34; 14/10 01:09, 01:23. **13 vezes** | "eu só queria as lagartas funcionando igual como a gente mapeou" | `docs/concept/ground/tracks_sand_C_approved.jpg` | ❌ | Fotos `lk_esteira_depois/04_cena_005s.png` e `10_cena_028s.png`: risco fino e chapado. `docs/CHAO_DEFORMAVEL.md:8-19`. Ele mandou pra v3 em 14/10 01:14, decepcionado | Look (o visual) + Jogo (os cadernos) |
| A cratera v2 (a bacia, o halo, os torrões, as pedras) | 13/10 09:29, 09:44, 10:47, 12:44 (aprovou), 15:20, 15:47, 15:50; 14/10 01:09. **8 vezes** | "caralho, mas já ficou bom pra kct"; depois "no jogo ficou cagado" | `docs/aprovados/look/2026-10-13_cratera_v2.jpg` | 🟡 | Foto `v2_crateras/crateras_recorte.png`: a bacia escura, o halo e as pedras saem, mas pintados, sem relevo. `Game/Config/RTS/Ground/crater_A/B/C.png`; `docs/CHAO_DEFORMAVEL.md:8-12` (o relevo foi pra v3) | Look + Jogo |
| Os buracos de tiro no chão com a qualidade da maquete | 14/10 00:12. 1 vez | "faltou os testes também dos buracos de tiros onde pegam no chão" | — | 🟡 | `Game/Config/RTS/Units.json:57-58` (a cratera A no tiro, 20% raspando); a foto "depois" do tiro não rodou (`docs/CHAO_DEFORMAVEL.md:12`) | Look + Jogo |
| As três crateras, cada uma pra uma coisa | 13/10 09:50. 1 vez | "deixe as 3, um tiro de tanque pode ser a primeira" | — | ✅ | `Units.json:57`; `Game/Tools/bake_craters.py`; `Game/Config/RTS/Ground/crater_*.png` | Jogo |
| O chão guarda a batalha (queimado, avental da morte, destroços) | 11/10 20:28. 1 vez | "ok, sim, pode fazer também a memória do chão" | — | ✅ | `RTSGroundMemory.cpp`; `Ground/wreck_A/B/C.png`; foto `fx_carcaca_v2_rec4/06_cena_032s.png` | Jogo |
| O impacto diferente em cada chão (areia, estrada, grama) | 13/10 09:48. 1 vez | "cada chão muda, o impacto na areia é diferente, impacto na estrada é outro" | — | 🟡 | Só a cor da cratera muda no asfalto (a entrada `Surf`, `docs/RECEITAS_DO_LOOK.md:129`); o efeito do tiro é o mesmo em todo chão (`docs/BASE_JOGAVEL.md:142`) | Look + Jogo + Fluxo |

## 4. Luz e céu

| O que ele pediu ou aprovou | Data(s) e quantas vezes pediu | A frase dele | Imagem | No jogo? | A prova | Dono |
|---|---|---|---|---|---|---|
| A luz A (a da bancada do Look) no jogo | 13/10 20:00. 1 vez | "esses gráficos aqui é qual? são bem bonitos" | `docs/aprovados/look/2026-10-13_luz_A_no_jogo.jpg` | ✅ | `Game/Config/RTS/Maps.json:41` (`look_recipe: desert_golden_dust`, feita sobre a A); `RTSGameModes.cpp:30-38` | Look |
| O sol a 8°, na hora dourada | 13/10 21:44. 1 vez | "8 graus" | `docs/aprovados/look/2026-10-13_hora_dourada_4_horarios_escolhido_8graus.jpg` | ✅ | `Game/Tools/look_recipes/desert_golden_dust.json:5` (elevation 8.0) | Look |
| A poeira dourada no alto do quadro, 0,06, o degradê mais suave e mais longo | 13/10 20:09, 21:11, 22:46, 23:18. **4 vezes** | "eu quero a 0,06" | `docs/aprovados/look/2026-10-13_poeira_dourada_escolhida_006.jpg` | ✅ | `desert_golden_dust.json:154` (density 0.06) | Look |
| O look atmosférico da maquete no jogo | 11/10 18:00, 19:55. 2 vezes | "não só o tanque, a casa, rua, look visual atmosférico, tudo" | — | ✅ | O plugin `Game/Plugins/RTSLook` (o `LookDirector` aplica a receita na partida); fotos `v2_final_base/` | Look + Jogo |
| A noite: o fogo pinta as unidades (a opção A) | 11/10 19:50; escolheu a A em 13/10. 2 vezes | "eu preferia antes quando ficava bege" | `docs/aprovados/look/2026-10-13_noite_A_fogo_pinta_unidades.jpg` | 🟡 | A receita existe (`Game/Plugins/RTSLook/Content/Recipes/DA_Look_desert_night.uasset`), mas só entra por chave de teste (`-RTSLookRecipe=`, `RTSGameModes.cpp:34-36`). O jogo não tem mapa de noite | Look |

## 5. Água

| O que ele pediu ou aprovou | Data(s) e quantas vezes pediu | A frase dele | Imagem | No jogo? | A prova | Dono |
|---|---|---|---|---|---|---|
| Água bonita (não "a água horrível de hoje") | 12/10 01:10. 1 vez | "a água horrível de hoje" | — | 🟡 | A receita só tinge o material velho do oceano (`Game/Plugins/RTSLook/Source/RTSLook/Private/LookDirector.cpp:259-295`). Os materiais novos de água do Look (`M_Water_SLW`, `M_Water_Trans`) não estão no jogo. Foto `lk_esteira_depois/04_cena_005s.png`: faixas verde-claras na água | Look + WorldForge |
| Água só onde existe água, sem o plano por baixo do mapa | 13/10 16:05. 1 vez | "por que tem água no chão? [...] não é a solução mais moderna" | — | ✅ | Registro da montagem do mapa do jogo: uma malha por oásis (`SM_Water_World_TestSalt2_1` e `_2`); `Game/Config/RTS/Grid/World_TestSalt2.json`; o vau na simulação (`SimCore/Public/Sim/SimMap.h:27`) | WorldForge + Jogo |

## 6. Unidades e modelos

| O que ele pediu ou aprovou | Data(s) e quantas vezes pediu | A frase dele | Imagem | No jogo? | A prova | Dono |
|---|---|---|---|---|---|---|
| O tanque da maquete do Look no lugar do tanque "borrado" | 11/10 17:59, 18:00. 2 vezes | "o que ele fez tá surreal e fenomenal" | — | ✅ | `Units.json:19-58` (`USA_MBT_LL`); fotos `lk_esteira_depois/` (a cor do time também: azul e vermelho) | Look + Jogo |
| O Dozer aprovado, com a sirene amarela que gira na obra | 13/10 21:47, 23:09. 2 vezes | "ok, gostei do trator, passa pro jogo usar" | `docs/aprovados/modelos/2026-10-13_dozer_no_jogo.jpg` | ✅ | `Units.json:253` (`USA_Dozer`, a sirene a 90 rpm); foto `v2_final_base/08_usina_pronta.png` | Modelos 3D + Jogo |
| O caminhão de suprimento (~60% do tanque, guindaste amarelo) | 13/10 23:35, 23:52; 14/10 00:12, 00:42. **4 vezes** | "agora sim, fala pros cara lá botar no jogo" | `docs/aprovados/modelos/2026-10-14_caminhao_suprimento_usa_com_tanque.jpg` | ✅ | `Units.json:309`; `Game/Content/Units/USA_SupplyTruck` (14/10 00:45); foto `v2_final_base/15_primeira_entrega.png` | Modelos 3D + Jogo |
| O tanque do GLA | 13/10 19:42, 20:53. 2 vezes | "top, salva, commita [...] quando fizermos a facção GLA" | `docs/aprovados/modelos/2026-10-13_tanque_gla_ficha_x_nosso.jpg` | ❌ (combinado: entra com a facção GLA) | Não está no `Units.json` | Modelos 3D |
| A carcaça com física (o vivo empurra, ela segura) | 13/10 20:49. 1 vez | "a carcaça não tem que ter opção, a carcaça tem que ter física" | — | ✅ | `Game/Source/SimCore/Public/Sim/Types.h:23` (simulação versão 12); cenário `carcaca_empurra`. Ele não testou | Jogo |
| A carcaça queimada com a ferrugem | 13/10 09:29. 1 vez | "a textura de ferrugem, muito bonito" | — | ✅ | Foto `fx_carcaca_v2_rec4/06_cena_032s.png` | Look + Jogo |
| Tirar o dano pintado dos veículos ("negócio porco") | 14/10 00:45. 1 vez | "se for pra fazer que fique bem feito, e não um negócio porco" | — | ✅ | `Game/Source/RTSGame/Private/RTSUnitActor.cpp:585` | Jogo |
| Nenhum inimigo aparece atrás das coisas (a silhueta só das suas e dos aliados) | 13/10 23:23. 1 vez | "nenhum tanque inimigo pode ser exibido atrás das coisas" | — | ✅ | `RTSMatchSubsystem.cpp:2388-2391` | Jogo |
| A silhueta "bugando" no caminhão com crateras em cima | 13/10 20:52. 1 vez | "o outline dos veículos [...] tá bugando no caminhão com crateras" | — | ❓ | Não achei a correção | Jogo |
| A árvore não some perto do cursor | 13/10 00:17. 1 vez | "essa ideia das árvores sumir com o cursor eu nunca aprovei" | — | ✅ | Não há sumiço de árvore no código do jogo (`Game/Source`); `docs/BASE_JOGAVEL.md:203` | Jogo |

## 7. Prédios

| O que ele pediu ou aprovou | Data(s) e quantas vezes pediu | A frase dele | Imagem | No jogo? | A prova | Dono |
|---|---|---|---|---|---|---|
| Os prédios provisórios "top e de arrepiar" | 10/10 14:15, 16:05. 2 vezes | "mesmo que é provisório temporário era pra você fazer fera" | — | 🟡 | Estão no jogo (`Units.json:67-252`; fotos `v2_final_base/`), mas o Command Center foi reprovado ("tá horrível", 13/10 20:55), a War Factory está "longe da ficha" e o depósito é um "estacionamento" (`docs/MODELOS_PROVISORIOS.md:37-41`) | Jogo |
| O Command Center virado pra quem joga (pra baixo e à esquerda) | 10/10 21:42, 22:10. 2 vezes | "coloca ele virado pra mim igual o default" | — | ✅ | `RTSMatchSubsystem.cpp:716-729`; foto `v2_final_base/02_base_inicio.png` | Jogo |
| O satélite do Command Center "voando" e a placa sumindo de frente | 10/10 22:08. 1 vez | "o satélite tá voando no model do command center" | — | 🟡 | Na foto `v2_final_base/02_base_inicio.png` o radar está preso na torre; a placa que some de frente ninguém conferiu | Jogo |
| O portão e a linha preta piscando nas construções | 10/10 21:42. 1 vez | "repare no portão e na linha preta [...] ela fica blinkando" | — | ❓ | Sem prova | Jogo |
| O brilho das construções fraco demais | 10/10 22:22. 1 vez | "os brilhos da construção ficaram muito fracos agora" | — | ❓ | Sem prova | Jogo |
| A Patriot (a defesa de base) | 14/10 00:42, 00:59. 2 vezes | "top, manda pros cara botar no jogo lá" | `docs/aprovados/modelos/2026-10-14_patriot_usa_34.jpg` | ❌ | Não está no `Units.json` nem no menu do Dozer (foto `v2_final_base/15_primeira_entrega.png`: só 4 prédios); a simulação não tem prédio que atira (`docs/MODELOS_PROVISORIOS.md:43`) | Modelos 3D + Jogo |
| O depósito de suprimento icônico (o monte de caixas, como no ZH) | 14/10 01:09, 01:25. 2 vezes | "precisa ser algo entendível e icônico que todo mundo entende que é uma supply" | — | ❌ | `Units.json:365` (o depósito antigo); foto `v2_final_base/15_primeira_entrega.png` (a grade de caixas verdes). Pedido feito 30 min antes deste levantamento | Modelos 3D |
| O depósito com dinheiro limitado, que já começa no mapa | 14/10 01:16. 1 vez | "o supply dock ali tem quantidade limitada de dinheiro" | — | ✅ | `Maps.json:91` (400 caixas por depósito) | Jogo |

## 8. Explosões e fumaça

| O que ele pediu ou aprovou | Data(s) e quantas vezes pediu | A frase dele | Imagem | No jogo? | A prova | Dono |
|---|---|---|---|---|---|---|
| A explosão Carga Pesada (o desenhador do WebGL) no lugar da explosão feia | 10/10 21:42. 1 vez | "você já pode usar no lugar desse feio aqui" | (referência: `reference/webgl/`, marcos v1–v6) | ✅ | `Game/Source/BlastFx/Private/BlastRaymarchSubsystem.cpp:136-146` (as gravações em `Game/Intermediate/Raymarch`); o shader do jogo é o do BlastLab (5 linhas de diferença) | Fluxo |
| A morte de tanque da v2, sem o fogo da carcaça | 13/10 22:26. 1 vez | "ficaremos desse jeito aí pra explosão de tanques" | `docs/aprovados/explosoes/2026-10-13_morte_tanque_v2.jpg` | ✅ | `BlastFxDirector.cpp:148-185` (a gravação `eg_tq_v2`); foto `lk_esteira_depois/10_cena_028s.png` | Fluxo + Jogo |
| Os prédios com a explosão clássica | 13/10 22:26; 14/10 01:08. 2 vezes | "eu havia pedido pra explosão das construções manter a clássica [...] ninguém me escuta" | — | ✅ (corrigido depois da bronca) | `BlastFxDirector.cpp:158-159` e `RTSMatchSubsystem.cpp:2298`: salvo às 01:09, um minuto depois da bronca, e compilado às 01:37. Ele ainda não viu | Jogo |
| O rastro de fumaça nos destroços e na torre | 13/10 23:20. 1 vez | "coloca rastro de fumaça também, é tudo que eu pedi" | — | ✅ | `Game/Source/BlastFx/Private/BlastDebris.cpp:245-266` (arquivo novo, fora do git); `BlastFxDirector.cpp:176-183`; folha `captures/game/ab_parada_destrocos.jpg` | Fluxo + Jogo |
| A fumaça não "parecer um PNG" no jogo (a sombra no chão, a névoa do mapa na frente) | 10/10 16:16; 13/10 00:40, 01:14, 23:20. **4 vezes** | "eu vi que a fumaça parece um png no jogo" | — | ✅ (ele não viu) | `BlastRaymarchSubsystem.cpp:338-450` (a sombra e a névoa) | Fluxo |
| A fuligem do tanque cinza-carvão; o amarelo é só a areia | 13/10 15:27. 1 vez | "o que é amarelo é a areia de baixo sendo empurrada" | `docs/aprovados/explosoes/2026-10-13_morte_tanque_v2.jpg` | ✅ | `BlastFxDirector.cpp:47-48` (a fuligem e a areia separadas). Ele não comparou no jogo | Fluxo |
| A explosão na escala do tanque, não "bomba de avião" | 13/10 00:52. 1 vez | "essa explosão tá ENORME em comparação com os modelos" | — | ✅ | `BlastFxDirector.cpp:46` (escala 0,35, medida contra os game shots) | Fluxo |
| A fumaça e a areia sem atravessar os veículos e as casas | 13/10 10:47, 19:52. 2 vezes | "nossas explosões não impactam nos veículos, elas entram dentro" | — | ❓ | Não achei código nem foto | Fluxo |
| A coluna de fumaça da JangaFX na morte de tanque (aprovada em 07/10) | 07/10. 1 vez | "tá boa" | — | ❌ | `docs/BASE_JOGAVEL.md:368` ("linkada no jogo e nada usa") | Fluxo |
| A sombra no clarão, a luz de uma explosão na fumaça da outra, a névoa na explosão (aprovadas no BlastLab) | 10/10 21:51. 1 vez | "ok sombra no clarão [...] névoa na explosão ok ótimo" | — | ✅ | `Game/Source/BlastFx/Private/ExplosionVolume.cpp` (o clarão com sombra); o shader do jogo é o do BlastLab | Fluxo |

## 9. Poeira

| O que ele pediu ou aprovou | Data(s) e quantas vezes pediu | A frase dele | Imagem | No jogo? | A prova | Dono |
|---|---|---|---|---|---|---|
| A poeira atrás das lagartas como no vídeo de gameplay e na `r1_raid_slabs` | 11/10 20:13; 13/10 13:32, 14:41, 14:47, 15:59. **5 vezes** | "fala que tem que ficar exatamente assim na areia" | `docs/aprovados/alvos/gameplay_v2_sheet.jpg` | 🟡 | Chegou: `Game/Plugins/RTSTreadDust` (fora do git), `RTSMatchSubsystem.cpp:854-865`; foto `fx_carcaca_v2_rec4/06_cena_032s.png`. E ele reprovou o que viu (as linhas abaixo) | Arte (FXLab) + Jogo |
| A v11, com o rastro longo pro tanque pesado | 13/10 19:45, 19:59. 2 vezes | "v11 é o caminho!"; "pra esse tipo de tanque pesado o longo" | `docs/aprovados/poeira_lagartas/2026-10-13_comprimento_jogo_longo_escolhido.jpg` | ✅ | O plugin do jogo é a v12 (a v11 com o rastro longo), `docs/BASE_JOGAVEL.md:318-324` | Arte |
| A poeira realista, não exagerada | 14/10 01:10. 1 vez | "achei bem exagerado a poeira agora [...] parece muito que é jogo de 2012" | — | ❌ | A fala dele | Arte |
| A poeira da cor exata do chão | 14/10 01:11. 1 vez | "se ele passou numa areia mais clara, fica mais clara" | — | 🟡 | Ela para no asfalto e no molhado (`RTSMatchSubsystem.cpp:1634`), mas a cor é uma só (`Game/Plugins/RTSTreadDust/Source/RTSTreadDust/Private/RTSTreadDust.cpp:77-79`) | Arte + Jogo |
| A poeira não entrar dentro dos tanques | 14/10 01:12. 1 vez | "a poeira hoje entra dentro dos outros tanques e do próprio tanque" | — | ❌ | A fala dele | Arte |
| Deixar a poeira ligada na v2 | 14/10 01:12. 1 vez | "não vou desativar a poeira na v2, vou deixar" | — | ✅ | Ligada por padrão (`-RTSNoDust` desliga) | Jogo |

## 10. HUD

| O que ele pediu ou aprovou | Data(s) e quantas vezes pediu | A frase dele | Imagem | No jogo? | A prova | Dono |
|---|---|---|---|---|---|---|
| O HUD aprovado peça por peça (a barra do ZH modernizada) | 10/10 (o dia todo); 12/10 00:27, 00:34, 22:53, 23:15; 13/10 00:27–00:57, 20:55; 14/10 01:08. **~20 falas** | "eu nem vi o HUD novo, nem interagi" | `docs/concept/hud/` (`refs/`, `pieces/`) | 🟡 | `Game/Plugins/RTSHud` (fora do git) + `RTSHUDNew.cpp`; virou o padrão às 01:09 de 14/10 (`RTSHUD.cpp:522`); foto `hud_base2/06_usina_lugar.png` (13/10). As fotos de 14/10 01:01 ainda mostram o HUD velho | Arte (HUDLab) + Jogo |
| O minimapa e os botões com fundo, não "voando" | 10/10 21:48; 12/10 23:15. 2 vezes | "o minimap voando sem fundo" | `docs/concept/hud/pieces/` | ✅ | Foto `hud_base2/06_usina_lugar.png` | Arte + Jogo |
| As superarmas: botão quadrado e simples, a coluna crescendo de baixo pra cima | 13/10 00:27, 00:34, 00:57. 3 vezes | "as superweapons estão posicionadas de um jeito que me deixa arrepiado de tão ruim" | — | 🟡 | Desenhadas (`Game/Plugins/RTSHud/Source/RTSHud/Private/RTSHudPainter.cpp:796, 951`), mas a partida não tem superarma nem poder | Arte |
| A fila da War Factory embaixo do cartão | 13/10 00:27. 1 vez | "a lista do war factory foi pra cima enquanto era pra ficar embaixo" | — | ❓ | `docs/concept/hud/DECISOES.md:177`; não há foto da fábrica selecionada no HUD novo dentro do jogo | Arte + Jogo |
| Os cartões dos jogadores sem nada vazando; o olho do espião | 13/10 00:50, 00:57. 2 vezes | "o ícone de espionagem HORRÍVEL, parece uma criança" | — | 🟡 | Refeito no HUDLab (`DECISOES.md:74-75`); no jogo não existe espionagem | Arte |
| O aviso com o triângulo liso e os parafusos cor de aço | 13/10 00:27, 00:35. 2 vezes | "os triângulos dos alertas estão desalinhados" | — | ✅ | Foto `hud_base2/06_usina_lugar.png` ("Low power") | Arte |
| O FPS na tela, escondido num canto | 14/10 01:08. 1 vez | "mostra o FPS na tela [...] escondido num canto" | — | ✅ | `RTSHUD.cpp:618-623` | Jogo |

## 11. Economia

| O que ele pediu ou aprovou | Data(s) e quantas vezes pediu | A frase dele | Imagem | No jogo? | A prova | Dono |
|---|---|---|---|---|---|---|
| A base do ZH: Command Center e Dozer, energia, obra, depósitos e caminhão, fila da fábrica, vitória pela base | 10/10 14:15, 21:26, 21:42. 3 vezes | "cara, está perfeito para uma v1 beta!" | — | ✅ | `Units.json:67-365`; fotos `v2_final_base/02` a `25` (do começo da base aos tanques prontos) | Jogo |
| A carga que cai | 10/10 13:49. 1 vez | "top, gostei da ideia 4, revolucionário isso" | — | ✅ | `Game/Source/SimCore/Private/WorldEconomy.cpp:803` | Jogo |
| Sem usina, a produção mais lenta (como no ZH) | 10/10 13:48. 1 vez | "sem usina as coisas são mais lerdas mesmo, é normal" | — | ✅ | `Units.json:355`; foto `v2_final_base/02_base_inicio.png` ("LOW POWER") | Jogo |
| A War Factory nível 2 (2 veículos) e a patente, ganhas na luta | 11/10 17:11, 17:15, 17:16. 3 vezes | "pras pessoas precisarem ir pra luta" | — | ❌ (combinado: ideia anotada) | `docs/IDEIAS_JOGABILIDADE.md` (commit `f32052c`) | Arte + Jogo |
| Construir chão, muros e estradas na base | 11/10 17:07. 1 vez | "pode ser uma feature magnífica de arrepiar na construção de uma base" | — | ❌ (combinado: ideia) | `docs/IDEIAS_JOGABILIDADE.md` | Arte + Jogo |
| A espionagem (ver o dinheiro e a energia do inimigo por um tempo) | 10/10 15:35. 1 vez | "teremos uma forma de espionagem no jogo que dura um tempo" | — | ❌ (combinado: ideia) | Só no desenho do HUD (`DECISOES.md:73-74`) | Arte + Jogo |

## 12. Multiplayer

| O que ele pediu ou aprovou | Data(s) e quantas vezes pediu | A frase dele | Imagem | No jogo? | A prova | Dono |
|---|---|---|---|---|---|---|
| O multiplayer em LAN da v1 | 10/10 21:26; aprovou 11/10 18:34. 2 vezes | "tá testado, aprovado 100%, pode começar" | — | ✅ | A marca `jogo-v1-multiplayer` (commit `d827f6c`) | Jogo |
| 4 jogadores, a cor de cada um, entrar na sala de outro, todos voltando pra mesma tela | 10/10 23:18. 1 vez | "esse mapa suporta 4 jogadores? precisava disso pro teste [...] com minha família" | — | ✅ | `Maps.json:12-29` (4 bases); `docs/MULTIPLAYER.md` Parte 7 | Jogo |
| O espectador depois de eliminado; alianças e times | 10/10 23:56. 1 vez | "após você morrer e ser eliminado, tem que virar espectador" | — | ✅ | O espectador em `RTSMatchSubsystem.cpp`; os times (`docs/BASE_JOGAVEL.md:86`) | Jogo |
| Sem a seta pro inimigo; a névoa "mapa já explorado" | 11/10 11:38. 1 vez | "lembre-se de tirar a setinha vermelha [...] e cria a opção de névoa transparente" | — | ✅ | `RTSHUD.cpp:279-289` (explorado, preto, desligado) | Jogo |
| Rodar no PC da família (Windows e Linux) | 11/10 11:55, 16:18. 2 vezes | "como fazer isso rodar no PC da minha família, Windows e o outro Linux" | — | 🟡 | O Windows foi testado em casa (11/10 18:34); o Linux não ("o Linux não vai testar agora") | Jogo |
| Testar a v2 em multiplayer | 13/10 20:49, 20:51. 2 vezes | "precisamos disso tudo pra hoje pra testar a v2 em multiplayer" | — | 🟡 | A pausa, o render-se e a queda estão feitos sem o teste dele (`docs/BASE_JOGAVEL.md:86, 109`); a v2 não foi testada em rede | Jogo |
| O jogo online | 10/10 22:12. 1 vez | "online será a última coisa do jogo" | — | ❌ (combinado: por último) | — | Jogo |

## 13. Câmera e controles

| O que ele pediu ou aprovou | Data(s) e quantas vezes pediu | A frase dele | Imagem | No jogo? | A prova | Dono |
|---|---|---|---|---|---|---|
| O zoom com o ângulo suave, como no Red Alert 3 | 10/10 22:51. 1 vez | "deixar bem pouquinho o pitch [...] olha o do Red Alert 3" | — | ✅ | `Game/Source/RTSGame/Private/RTSCameraPawn.cpp:51-60` | Jogo |
| Deitar a câmera segurando o scroll | 10/10 22:22. 1 vez | "a gente por enquanto adotou segurar o scroll e deitar dessa forma" | — | ✅ | `RTSCameraPawn.cpp:141` | Jogo |
| Arrastar a câmera com o direito sem cancelar o prédio que vai construir | 10/10 22:22. 1 vez | "só cancelar no clique único igual o C&C" | — | ✅ | `RTSPlayerController.cpp:831` | Jogo |
| O giro do botão do meio invertido | 10/10 22:22. 1 vez | "o scroll que gira a câmera está invertido" | — | ❓ | `RTSPlayerController.cpp:869`; sem prova de que foi trocado | Jogo |
| A formação segurando uma tecla (F) | 11/10 20:16. 1 vez | "segurando uma tecla porque no C&C era assim" | — | ✅ | Simulação versão 8; teste "formação com F" (`docs/BASE_JOGAVEL.md:256`) | Jogo |
| Arrastar com o botão **direito** pra dar a direção da tropa | 12/10 00:06. 1 vez | "arrastar botão direito, porque é o que já manda eles irem lá" | — | 🟡 | Foi feito com o **esquerdo** (`RTSPlayerController.cpp:716-742`); o direito continua rolando a câmera (`:807`). A sessão respondeu "o mouse ficou como você pediu" | Jogo |
| O Ctrl força o tiro (no chão, na própria unidade, no aliado, na casa) | 13/10 10:41. 1 vez | "permite atirar no chão segurando o control igual os C&C clássico" | — | ✅ | `Game/Source/SimCore/Private/Commands.cpp` (`AttackGround`, `ForceAttack`) | Jogo |
| Um desfile melhor que o do ZH | 11/10 20:04. 1 vez | "pensar como um AAA faria, que é bem melhor os desfiles" | — | 🟡 | Na simulação (versões 7 e 8); "falta ver no jogo" (`docs/BASE_JOGAVEL.md:251-258`) | Jogo |
| Um zoom bom pra entender o jogo, ainda com detalhe | 14/10 01:26, 01:27. 2 vezes | "nosso RTS precisa ter um zoom bom pra entender o jogo" | — | ❓ | Ficou pra v3 (`docs/RECEITAS_DO_LOOK.md:62`) | Look + Jogo |

## 14. Outros

| O que ele pediu ou aprovou | Data(s) e quantas vezes pediu | A frase dele | Imagem | No jogo? | A prova | Dono |
|---|---|---|---|---|---|---|
| Documentar tudo; "aprovou, registrou" | 10/10 23:27; 13/10 12:46, 23:30, 23:33; 14/10 01:31. **5 vezes** | "o projeto no começo era mó top documentado" | — | 🟡 | `docs/APROVADOS.md` existe desde 13/10, mas 4 linhas ainda estão sem imagem (`:59-62`) e a receita da areia diz que ela está no mapa (`RECEITAS_DO_LOOK.md:65`), quando o mapa do jogo está na areia antiga | todas |
| Salvar (commitar) tudo, pra não perder | 13/10 15:45 (2 falas) | "não quero perder nada feito até agora" | — | 🟡 | O último commit do jogo é de 11/10 (`d827f6c`). Fora do git: 60 arquivos alterados (~6 mil linhas) e 138 novos (a memória do chão, a poeira, o HUD novo, os destroços, os cenários, a receita da luz). Ele mesmo disse ao jogo "como vou commitar se eu nem vi o resultado" (13/10 16:04) | Jogo |
| A otimização "perfeita" e rodar em PC fraco | 10/10 23:31; 13/10 20:55; 14/10 00:12, 01:08, 01:13. **5 vezes** | "foco na otimização, otimização perfeita, como os grandes otimizam" | — | 🟡 | Os destroços caíram de 47 pra 40 ms na hora das mortes (`docs/OTIMIZACAO_EXPLOSOES.md`). A partida: 21,7 ms no médio (`captures/game/perf_media.log`) e 25,8 ms no teste da base (`v2_final_base.log`), ou 39 a 46 quadros por segundo, não 60 | todas |
| Tudo provado no jogo antes de ser dado como pronto | 13/10 23:34, 23:35. 2 vezes | "bonito = jogo bonito otimizado" | — | ❌ | A areia "C", a cratera e a estrada foram aprovadas na maquete sem a foto do jogo ao lado (`docs/CHAO_DEFORMAVEL.md:19`) | todas |
| O jogo igual aos game shots (o alvo `r1_raid_slabs`) | 13/10 00:59, 15:57. 2 vezes | "quero semelhanças aos game shots 100%, por favor!" | `docs/aprovados/alvos/r1_raid_slabs_gemini.webp` | ❌ | Não existe folha "jogo × r1_raid_slabs" na câmera do jogo; a fatia vertical (`docs/BASE_JOGAVEL.md:295`) não começou | todas |
| O tanque derruba poste e árvore; o fio com física e "choquinho" | 13/10 10:14, 14:37. 2 vezes | "física bonita e efeito de choquinho bem bonito" | — | ❌ (combinado: v3) | `docs/BASE_JOGAVEL.md:155` | Jogo + WorldForge |

---

## O que ele reprovou: saiu do jogo? (fora da conta)

| O que ele reprovou | Quando | Hoje no jogo |
|---|---|---|
| A luz B, calibrada pela cartela ("doem meus olhos") | 13/10 | saiu: o jogo usa a `desert_golden_dust` (`Maps.json:41`) |
| O asfalto da volta 15 no mapa ("senti uma regressão") | 13/10 | saiu: o mapa usa o asfalto v2 (registro da montagem, linha 1688) |
| A foto aérea borrada por cima do asfalto | 13/10 | saiu (`docs/WORLDFORGE.md:1338`) |
| As trilhas de roda no asfalto v2 | 13/10 | saíram (o v2d) |
| O fogo da carcaça depois da explosão (vai pra v3) | 13/10 | saiu (`BlastFxDirector.cpp:43`) |
| O dano pintado nos veículos | 14/10 | saiu (`RTSUnitActor.cpp:585`) |
| A árvore que some perto do cursor | 13/10 | saiu |
| **O Command Center** ("tá horrível aquilo, mas depois a gente vê") | 13/10 | **continua no jogo** (`Units.json:67`; fotos `v2_final_base/`) |
| A mini vila v1 ("cabanas só jogadas") | 13/10 | trocada pela v2, que ele ainda não julgou |
| O quadrado escuro do nada (a chapa da sombra de contato) | 13/10 | sem prova |
| A lagarta voando junto com a torre; a fumaça preta | 13/10 | sem prova |

## Fora do jogo (fora da conta)

- **WorldForge, a ferramenta** (pedidos de 12/10): tirar o deslize na borda da tela, o painel em abas, a lista de
  mapas e os 28 mapas do Zero Hour. Feitos na madrugada de 13/10 (`docs/WORLDFORGE.md:1594-1599`); ele não testou.
- **O EmberGen fora da fila da placa** (pediu 4 vezes, "já avisei 10x", 13/10 14:27): feito com a fila própria.
- **A arte conceitual, os game shots e as rodadas do HUDLab:** são referências, não o jogo.

---

## O que funciona e custou caro (o que está sólido no jogo hoje)

Pra pesar recomeçar do zero contra continuar. Cada linha tem prova no código, na configuração ou numa foto do jogo.

1. **A simulação própria, determinística, em ponto fixo** (`Game/Source/SimCore`, ~7,6 mil linhas, versão 12). É a
   parte mais difícil de um RTS em rede. Ela roda igual em todo PC, tem testes sem a Unreal (`Game/Tools/simtest.ps1`)
   e já carrega a economia, a névoa, a água, a formação, o Ctrl e a carcaça com física.
2. **O multiplayer em LAN aprovado** por ele em 11/10 ("aprovado 100%"), com 4 jogadores, times, espectador e o
   aperto de mão pela impressão digital. Na v2 entraram a pausa, o render-se e a queda.
3. **A base do Zero Hour inteira**: Command Center, Dozer, energia, obra, depósitos, caminhão, fila da fábrica, a
   carga que cai e a vitória pela base. Ele disse "perfeito pra uma v1 beta" e "FENOMENAL".
4. **A névoa de guerra e o minimapa**, aprovados e testados por ele em 08/10 (commit `e927a02`).
5. **As explosões**: o desenhador do WebGL portado pra Unreal (1 a 2 ms), as gravações do EmberGen tocando nele, a
   morte de tanque da v2 aprovada, os destroços com rastro, a sombra e a névoa. O shader do jogo é o mesmo do
   BlastLab.
6. **A luz aprovada dentro do jogo** (o plugin `RTSLook` com as receitas: o sol a 8°, a poeira dourada a 0,06).
7. **O caminho do WorldForge pro jogo**: o mapa por semente, a vila em JSON com a grade e a conferência pelo hash, a
   água por corpo na simulação, o asfalto v2, o oásis verde e o tipo de chão (`surface.png`).
8. **Os modelos por script**: o tanque do Look, o Dozer e o caminhão aprovados e no jogo; a Patriot e o tanque do GLA
   aprovados, prontos pra entrar. Quatro aprovações em dois dias.
9. **A memória do chão** (cadernos que guardam esteira, queimado e cratera por +0,2 ms): é a base do que a v3 precisa.
10. **As ferramentas de teste**: os cenários rápidos de ~40 s com semente fixa (`game.ps1 view -Scenario`), as fotos
    automáticas, a fila da placa entre as sessões.

**O que falhou não foi a tecnologia, foi a passagem da maquete pro jogo.** O que não chegou (a areia, as lagartas, a
cratera, a estrada 3D) foi aprovado em foto de estúdio, sem a foto do jogo ao lado; passou por três sessões; e o mapa
do jogo ficou com a areia antiga sem ninguém conferir. Recomeçar do zero apagaria sobretudo os itens 1 a 10 acima,
que funcionam; o defeito de processo continuaria igual se o jeito de aprovar não mudar (a prova sempre no jogo, na
câmera do jogo, antes do "aprovado").

**Antes de qualquer decisão, dois riscos de hoje:** a v2 do jogo está fora do git, e o mapa do jogo está na areia
antiga.

---

## Como foi feito e os limites

- **As falas:** 542 mensagens dele de 10 a 14/10 (as diretas, sem as repetidas entre transcrições) e mais 174 que ele
  mandou enquanto a sessão trabalhava (a "fila"). Os resumos automáticos de contexto ficaram de fora.
- **O jogo não foi aberto.** Os itens ❓ precisam de uma foto do jogo pra fechar.
- **As fotos de 14/10 (01:01–01:15) são de antes da compilação das 01:37.** O HUD novo como padrão e a explosão
  clássica dos prédios estão no código, mas não nessas fotos.
- **Os aprovados de antes de 10/10** entraram quando estão no índice (`docs/APROVADOS.md`) e dizem respeito ao jogo.
