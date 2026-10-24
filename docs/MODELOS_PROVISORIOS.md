# Modelos provisórios: o mapa contra os game shots (13/10/2026)

Decisão do Felipe (13/10): enquanto ele estuda Blender pra modelar os finais, o jogo usa
**provisórios bonitos feitos por script**, no padrão do tanque do Look (`Game/Tools/look_tank.py`):
quina dura com chanfro, proporção e silhueta da ficha, **sem desgaste** (uma cor por material, o pó
do mundo por cima), slots de material por peça (`Team` = cor do jogador, `Paint`, `Dark`, `Metal`,
`Cloth`, `Lens`, `Track`), GLB separado por peça que se mexe, origem no chão e no centro.
A IA de 3D (Hunyuan, Tripo) ficou de lado: o tanque gerado em `captures/concept/31_manual_parts/`
está guardado e não é usado.

Este arquivo é a lista viva: **quem fizer uma peça marca aqui** (onde está o script e o GLB).

## 1. Os game shots usados

Base USA no deserto (`captures/concept/44_base_gameplay/`): `r1_raid_slabs_gemini` (o alvo nº 1),
`r1_calm_slabs_gemini`, `r1_raid_settled_gemini`, `r2_match_gemini`.
Vila e as outras facções (`docs/concept/scenes/desert_golden_hour/best/`): `usa_vs_china_gemini`,
`road_town_battle_gemini`, `town_battle_top_gemini`.

## 2. O que já existe (e onde)

| Kit | Script | Saída | Peças |
|---|---|---|---|
| Tanque e veículos do Look | `Game/Tools/look_tank.py` (`look.ps1 tank`) | `captures/look/tank_glb/` → `/Game/Units/USA_MBT_LL` (o jogo) | tanque USA (casco, torre, cano, rodas, esteira esq./dir.), carcaça, picape armada e civil, Dozer, paraquedas, miudezas (ar-condicionado, munição, toldo, tambores, muro quebrado ×2, caixotes, parabólica, cerca ×2, varal, entulho ×3, sacos de areia, sucata ×2, pneus, caixa d'água) |
| Casas da vila | `Game/Tools/look_houses.py` + `look_civil_props.py` (`look.ps1 civil`) | `captures/look/civil_glb/`, `civil_kit/` | casa 1 andar, 2 andares, comprida, varanda; muro, muro com portão, poste; carro queimado |
| Base USA do jogo | `Game/Tools/blender_provisional.py` + `import_provisional.py` (`game.ps1 models`) | `captures/game/prov/` → `/Game/Structures/` | Command Center (+radar, andaime), Power Plant (+andaime), Supply Center (+andaime), Supply Dock, caixa de suprimento, caminhão de suprimento, War Factory (+ventilador, andaime), Dozer (+lâmina), destroços |
| Mundo | WorldForge | — | palmeiras (PlantCatalog), arbustos, monólitos de rocha |

## 3. O mapa: o que aparece nos game shots × o que temos

Prioridade: **A** = entra no 1º teste (USA × GLA, uma War Factory por lado); **B** = depois; **C** = fora do escopo agora.

### Base USA

| Peça | Nos game shots | Temos | Estado | Prior. |
|---|---|---|---|---|
| War Factory | galpão aberto com ponte rolante amarela, 2 ventiladores no teto, pilares de concreto, anexo com faixa azul, braço robótico no pátio (`usa_war_factory_sheet`) | prov: caixa + ventiladores + pórtico | **longe da ficha** aprovada | **A** |
| Supply Dock | monte/anel irregular de caixas verdes (os game shots); o Felipe (14/10): "simples demais, quero uma ideia poderosa"; reprovou contêineres ("não lê como suprimento") e qualquer ideia que reponha o estoque | **a pirâmide de suprimento (14/10)**: a caixa de suprimento do C&C (olive, faixa amarela na tampa) em 5 degraus (6×5 … 2×1, 70 caixas, 5 m), rampa de madeira na doca (+X), paletes vazios e a flâmula; a altura É o estoque (o jogo tira o topo primeiro). `Game/Tools/model_supply_dock.py` → `captures/game/models/supply_dock/SM_SupplyCrate.glb` (a caixa, instância do jogo), `SM_SupplyDock_Yard.glb` (o que fica), `dock_layout.json` (`heap_m` por caixa, na ordem de sumir; `pads_m`+`stack_m` de reserva pro código de hoje). Pro jogo: 1 linha no `RTSGameData.cpp` (se houver `heap_m`, CrateSlots vem dela com o giro, como o `pile_m`). Folha de ideias pro GPT em `captures/concept/_attach/supply_dock/PROMPT.md` (4 assinaturas de caixas) | 2 voltas; esperando o Felipe julgar | **A** |
| Power Plant | 2 chaminés altas gêmeas com fumaça + domo branco + tubulação | prov: 1 tanque cilíndrico + domo + 1 chaminé | parcial | B |
| Command Center | bloco com parabólica grande, mastro com bandeira, faixa azul | prov | ok de longe; faltam a parabólica grande e o mastro | B |
| Supply Center | prédio baixo com faixa azul e laje de descarga | prov (com heliponto) | ok | C |
| Obra (andaime com grua) | gaiola de andaime em volta do prédio | prov: andaime por prédio | ok | C |
| **Defesa de base (Patriot)** | não aparece nos game shots (a arte notou: "faltam os sinais de partida: a defesa atirando") | **feita em 14/10** (pedido do Felipe: "a pegada clássica icônica do ZH"; sem arte: a fórmula do `CONCEITO_CONSTRUCOES.md` §1 e a bíblia da USA §2 sobre a Patriot do ZH): `Game/Tools/model_usa_patriot.py` → `captures/game/models/usa_patriot/SM_USA_Patriot_{Base,Turret,Dish,Scaffold}.glb`, 11 mil triângulos, lote 8 × 8 m. A assinatura: a caixa lançadora de 2×2 células inclinada 45° girando no bloco octogonal blindado, o radar pequeno no mastro. Peças: Base (hull: bloco, pedestal, mastro com a sirene no slot `Beacon`, gerador, faixa de perigo no chão, borda de fundação), Turret (`turret_pivot_m` [0, 0]), Dish (`spin`, `at_m` (−2,35; −2,35; 4,4), 12 rpm, para sem energia), Scaffold (andaime). Slots das construções: Body, Trim, Dark, Player, Concrete, Hazard, Glass, Light, Beacon, Yellow, Crate | **aprovado pelo Felipe (14/10, "top, manda pros cara botar no jogo")**; a simulação ainda não tem construção que atira (a sessão do Jogo cria a unidade: no ZH é a AmericaPatriotBattery) | **A** |
| **Strategy Center** (economia v4) | não aparece (prédio novo) | **feito em 22/10** (sem arte: a fórmula do `CONCEITO_CONSTRUCOES.md` §1/§3b e o botão do HUD `usa_tech_center.png`): `Game/Tools/model_strategy_center.py` → `captures/game/models/usa_strategy_center/SM_USA_StrategyCenter_{Base,Dish,Scaffold}.glb`, 16 mil triângulos, lote 20 × 20 m, frente (portão) em +X. A palavra: o prato de satélite de 8,4 m quase deitado (55°) girando sobre o bunker octogonal blindado de 4,7 m (o prato vai a 12,2 m). Peças: Base (hull: laje, bunker com a saia inclinada, faixa azul nas faces, portão, teto com as lonas azuis, gerador, tanque, bandeira, poste), Dish (`spin`, `at_m` (0; 0; 5,8), ~3 rpm), Scaffold (octogonal, aberto no portão). Prévia em `preview/` | esperando o Felipe julgar | **A** |
| **Supply Drop Zone** (economia v4) | não aparece (prédio novo) | **feito em 22/10** (sem arte: o botão `usa_drop_zone.png`): `Game/Tools/model_drop_zone.py` → `captures/game/models/usa_drop_zone/SM_USA_DropZone_{Base,Radar,Scaffold}.glb`, 14 mil triângulos, lote 16 × 16 m (o pedido era ~14, arredondado pra grade de 4 m). A palavra: o alvo de lançamento (pátio 10 × 10 com o X branco no anel amarelo e as cantoneiras de perigo; **o meio livre**, o alvo em (0; 0)). Torre de controle no canto de trás-esquerdo com o radar de aproximação (`spin`, `at_m` (−6,3; −6,3; 6,4), ~15 rpm), pilhas das caixas recebidas e a carga com a lona azul no canto de trás-direito, mastro de luz com os holofotes e a sirene, biruta, luzes de pista. Peças: Base, Radar, Scaffold (em volta da torre) | esperando o Felipe julgar | **A** |
| Caixa lançada + paraquedas, avião de carga (o efeito do Drop Zone) | não aparece | **feitos em 22/10**: `Game/Tools/model_supply_crate.py` → `captures/game/models/supply_drop/SM_SupplyDrop_{Crate,Canopy}.glb` (a caixa olive com a faixa amarela, 1,24 × 1,04 × 1,17 m com o saco do paraquedas; o paraquedas aberto de 5,2 m com 12 gomos cor do jogador/branco, **mesma origem da caixa**, topo a 6,1 m). `Game/Tools/model_cargo_plane.py` → `captures/game/models/cargo_plane/SM_USA_CargoPlane.glb` (estilo C-130 em 0,74: 22,2 m × 30,2 m de envergadura × 7,2 m, peça única com as hélices paradas e a rampa aberta; origem no eixo da fuselagem; as caixas saem em `drop_at_m` (−6,6; 0; −1,6)) | esperando o Felipe julgar | **A** |
| Chinook | pousando no heliponto ou trazendo caixa pendurada (`usa_chinook_sheet`) | nada | falta (ainda não existe na simulação) | B |
| Poste de luz da base | postes baixos nas lajes | poste civil | adaptar | C |
| Lajes de concreto com faixas | sob cada prédio | chão (Look/WorldForge) | não é modelo | — |

### Unidades USA

| Peça | Nos game shots | Temos | Estado | Prior. |
|---|---|---|---|---|
| Tanque USA | todos | Look | **pronto** | — |
| Dozer | base calma e atacada | **refeito em 13/10** pela arte aprovada (`usa_dozer.webp`): `Game/Tools/model_usa_dozer.py` → `captures/game/models/usa_dozer/SM_USA_Dozer_{Hull,Blade,Beacon,Wheels,Track_l,Track_r}.glb`, 33 mil triângulos, escala 0,88 aplicada no script (medido contra o tanque: 7,65 m da lâmina ao escarificador × 3,87 m × cabine a 2,6 m; o tanque tem 8,1 m de casco × 3,8 m × torre a 2,5 m, mastro a 4,15). A lâmina tem a origem no pivô (`blade.at_m` = (−0,352; 0; 0,66)); a **sirene amarela** (pedido do Felipe): cúpula no casco (slot `Beacon`, pisca) e a luz que gira é a peça `SM_USA_Dozer_Beacon` (origem no eixo, `beacon.at_m` = (0,396; 0,264; 2,86), ~90 rpm, só enquanto constrói: o jogo precisa ligar isso) | 10 voltas, comparado lado a lado com a arte no mesmo ângulo (`preview/lado_a_lado_v10.jpg`, folha `preview/compare_arte_v10.jpg`); **aprovado pelo Felipe (13/10, "gostei do trator") e no jogo:** `game.ps1 dozer` importa pra `/Game/Units/USA_Dozer` (modo `PROV_DOZER_ONLY` do `import_provisional.py`, reaproveita os materiais), o `Units.json` aponta pras 6 peças (blade e spin com os pivôs acima), e o `RTSUnitActor.cpp` gira a sirene e acende a cúpula (`DataPower`) só enquanto constrói; cenário rápido `dozer_foto` pra conferir | — |
| Caminhão de suprimento | não aparece (é regra do jogo) | **refeito em 14/10** (sem arte aprovada: desenho pela raiz da USA, ficha do tanque + Dozer; base real o caminhão tático 6×6 do exército, LMTV 6,4 × 2,4 × 2,85 m): `Game/Tools/model_usa_supply_truck.py` → `captures/game/models/usa_supply_truck/SM_USA_SupplyTruck_{Hull,Wheels}.glb` (6 rodas separadas) + `SM_USA_SupplyCrate_Small.glb` (a caixa da carga, 0,72 da do depósito), 19 mil triângulos. **Escala 0,72** aplicada no script depois da bronca do Felipe ("um tanque é pra ser grande"): 5,1 × 2,3 m, cabine a 1,75 m, ~60% do tanque. Carga em 2×2: `cargo.slots_m` = (−1,76; ∓0,49; 0,94) e (−0,61; ∓0,49; 0,94), `cargo.mesh` = a caixa pequena. Guindaste amarelo atrás da cabine com a lança por cima da carga (a assinatura do coletor); cor do jogador nas saias, portas e lona | 5 voltas (`preview/folha_v5.jpg`); esperando o Felipe julgar; o design ele já disse que "tá bonito" | — |
| Tomahawk Launcher (`usa_tomahawk`, artilharia de 85 m) | não aparece | **feito em 22/10** (provisório; sem arte aprovada: o botão `usa_rocket_launcher.png` é só um caminhão com caixa): o **chassi do caminhão de suprimento** (mesmo script de origem, mesma escala 0,72, mesmo raio) com o lançador no lugar da caçamba. Inspiração real: o TEL do BGM-109G (tubos que se levantam pela traseira), o HIMARS no FMTV e o NMESIS de 2 células. A assinatura de cima: **2 tubos grossos deitados** num berço blindado com a faixa azul segmentada, 2 anéis azuis em cada tubo (como os do cano do tanque), as pontas dos mísseis afiladas com o radome escuro saindo da boca; macacos amarelos nos cantos de trás. `Game/Tools/model_tomahawk.py` → `captures/game/models/usa_tomahawk/SM_USA_Tomahawk_{Hull,Wheels,Turret,Pod}.glb`, 26 mil triângulos; 5,13 × 2,32 m, lançador a 2,0 m (antenas 2,77). Turret (mesa + berço) e Pod (os tubos, deitados) nas coordenadas do veículo: as duas na lista `turret`, `turret_pivot_m` (−1,296; 0). A elevação (o jogo ainda não levanta): eixo Y em `pod_hinge_m` (x −2,304; z 1,404), pose de tiro 45° em `pose/` (fora da importação). O giro de 360° foi testado contra o casco (colisão BVH, deitado e a 45°): limpo. Slots do caminhão (`DOZER_SLOTS`) | 1 volta (`preview/folha_v1.jpg`); esperando o Felipe julgar | — |
| Humvee com metralhadora | `r1_raid_slabs` (canto inferior direito), `town_battle_top` | nada | sem ficha | C |
| Helicóptero de ataque | `road_town_battle`, `town_battle_top` | nada | sem ficha | C |
| Drone (quadricóptero) | `road_town_battle`, `town_battle_top` | nada | sem ficha | C |
| Blindado de rodas/esteira (APC) | `road_town_battle` | nada | sem ficha | C |
| Infantaria | em todos os de combate | nada | precisa de animação; fora do escopo | C |

### GLA

| Peça | Nos game shots | Temos | Estado | Prior. |
|---|---|---|---|---|
| Tanque GLA (Scorpion) | `r1_raid_*` (verde-oliva com faixa verde, pneu atrás, sacos de areia) | **feito em 13/10** (sessão Modelos 3D): `Game/Tools/model_gla_tank.py` → `captures/game/models/gla_tank/SM_GLA_MBT_{Hull,Turret,Barrel,Wheels,Track_l,Track_r}.glb`, ~48 mil triângulos; folha ficha × nosso em `captures/game/models/gla_tank/preview/compare_ficha_v*.jpg` (`model_preview.py` + `model_sheet.py`) | **aprovado pelo Felipe (13/10)**. Pronto pra importar assim que a facção GLA entrar no jogo: os GLBs têm a origem do tanque (no chão, no centro), a torre gira em (0,35; 0) e o cano recua em +X; a importação precisa de 5 slots novos na tabela do `import_provisional.py` (Bag, Patch, Rust, Rubber, Wood). Falta a carcaça (`_Wreck`) | **A** |
| Oficina GLA (War Factory) | ficha `gla_war_factory_sheet` (galpão de barro com rede de camuflagem, chaminés, bandeiras) | **1ª volta montada em 13/10**: `Game/Tools/model_gla_war_factory.py` → `captures/game/models/gla_war_factory/gla_war_factory{,_scaffold}.glb` (24 × 32 m, frente em +X, 19 mil + 4 mil triângulos); folha `preview/compare_ficha_v1.jpg` ainda não conferida contra a ficha | fora da v2 (decisão do Felipe, 13/10); continua depois do Dozer | B |
| Picape técnica com bandeira verde | `r1_raid_*` | Look: picape armada | **falta a bandeira verde** (cor do jogador) | **A** |
| Bandeirinha verde (pano ao vento) | em todo veículo GLA | nada | pano com animação simples | B |
| Buggy aberto | `town_battle_top` | nada | sem ficha | C |
| Muros de sucata/barricada | `town_battle_top` | props: cerca, sucata, muro quebrado | parcial | C |

### China

| Peça | Nos game shots | Temos | Estado | Prior. |
|---|---|---|---|---|
| Tanque China (Battlemaster) | `usa_vs_china`, `road_town_battle`, `town_battle_top` | nada | ficha aprovada (`china_mbt_sheet`) | B |
| War Factory China | ficha `china_war_factory_sheet` | nada | ficha aprovada | B |
| Picape/caminhão com bandeira vermelha | `road_town_battle` | Look: picape | variante de cor + bandeira | C |

### Civil (vila)

| Peça | Nos game shots | Temos | Estado | Prior. |
|---|---|---|---|---|
| Casas (1 andar, 2 andares, comprida, varanda) | todos os de vila | Look | **prontas** | — |
| Muro, portão, poste, varal, caixa d'água, parabólica, ar-condicionado | idem | Look | prontos | — |
| Carro queimado, picape civil | idem | Look | prontos | — |
| Palmeiras, arbustos | idem | WorldForge | prontos | — |
| Carro civil inteiro (sedã) | `road_town_battle` (carros voando na explosão) | nada | falta | C |
| Quartinho no teto, ruína, pátio, fortificada, mesquita, loja | — | — | adiadas pelo Felipe (08/10) | — |

### Estados (obra, dano, destruído)

| Peça | Temos | Estado | Prior. |
|---|---|---|---|
| Carcaça do tanque USA | Look (`SM_LL_Wreck`) | pronta | — |
| Carcaça do tanque GLA / China | nada | sai junto com cada tanque | A / B |
| Carcaça da picape | nada | falta | C |
| Escombro (`_Rubble`) das casas e dos prédios (ficha `PRODUCAO_3D.md` §6) | nada | falta; o prédio cai no 1º teste | B |
| Andaime da obra | prov | ok | — |

## 4. Ordem proposta

1. **Tanque GLA** (+ carcaça), pela ficha `gla_mbt_sheet`.
2. **Oficina GLA**, pela ficha `gla_war_factory_sheet`.
3. **War Factory USA** no nível da ficha aprovada (a de hoje é a que mais destoa dos game shots).
4. **Supply Dock como monte** de caixas (barato, muda muito a leitura).
5. **Bandeira verde** nas picapes e no tanque GLA.
6. **Power Plant** com as 2 chaminés e a tubulação; **Command Center** com a parabólica grande.
7. **Chinook** (quando a simulação tiver o que ele faz).
8. **Escombro** das casas e dos prédios.
9. China: tanque e War Factory.
10. Humvee, helicóptero de ataque, drone, APC, buggy, sedã.

## 5. Como cada peça nova é feita

- Ficha com a **vista de cima** primeiro (`docs/CONCEITO_UNIDADES.md`, `docs/CONCEITO_CONSTRUCOES.md`);
  medidas pela régua do tanque USA (7,9 m) e pela grade de 4 m (`docs/PRODUCAO_3D.md`).
- Script próprio por peça em `Game/Tools/model_<nome>.py` (Blender sem janela, só CPU, fora da
  fila da placa). **Não mexer no `look_tank.py`**: é da sessão do Look.
- Conferência antes de entregar: folha com as 5 vistas lado a lado com a ficha, mais a câmera do
  jogo com o tanque USA do lado (`PRODUCAO_3D.md` §8).
- GLB por peça móvel (torre, cano, porta, ventilador), origem no chão e no centro; unidade com a
  frente em +X (padrão do tanque), construção com a frente em +Y (padrão do WorldForge).
