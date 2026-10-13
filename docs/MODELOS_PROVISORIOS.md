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
| Supply Dock | monte/anel irregular de caixas verdes | prov: grade regular ("estacionamento", nota da arte) | refazer como monte | **A** |
| Power Plant | 2 chaminés altas gêmeas com fumaça + domo branco + tubulação | prov: 1 tanque cilíndrico + domo + 1 chaminé | parcial | B |
| Command Center | bloco com parabólica grande, mastro com bandeira, faixa azul | prov | ok de longe; faltam a parabólica grande e o mastro | B |
| Supply Center | prédio baixo com faixa azul e laje de descarga | prov (com heliponto) | ok | C |
| Obra (andaime com grua) | gaiola de andaime em volta do prédio | prov: andaime por prédio | ok | C |
| Chinook | pousando no heliponto ou trazendo caixa pendurada (`usa_chinook_sheet`) | nada | falta (ainda não existe na simulação) | B |
| Poste de luz da base | postes baixos nas lajes | poste civil | adaptar | C |
| Lajes de concreto com faixas | sob cada prédio | chão (Look/WorldForge) | não é modelo | — |

### Unidades USA

| Peça | Nos game shots | Temos | Estado | Prior. |
|---|---|---|---|---|
| Tanque USA | todos | Look | **pronto** | — |
| Dozer | base calma e atacada | Look + prov | pronto | — |
| Caminhão de suprimento | não aparece (é regra do jogo) | prov | ok | — |
| Humvee com metralhadora | `r1_raid_slabs` (canto inferior direito), `town_battle_top` | nada | sem ficha | C |
| Helicóptero de ataque | `road_town_battle`, `town_battle_top` | nada | sem ficha | C |
| Drone (quadricóptero) | `road_town_battle`, `town_battle_top` | nada | sem ficha | C |
| Blindado de rodas/esteira (APC) | `road_town_battle` | nada | sem ficha | C |
| Infantaria | em todos os de combate | nada | precisa de animação; fora do escopo | C |

### GLA

| Peça | Nos game shots | Temos | Estado | Prior. |
|---|---|---|---|---|
| Tanque GLA (Scorpion) | `r1_raid_*` (verde-oliva com faixa verde, pneu atrás, sacos de areia) | **feito em 13/10** (sessão Modelos 3D): `Game/Tools/model_gla_tank.py` → `captures/game/models/gla_tank/SM_GLA_MBT_{Hull,Turret,Barrel,Wheels,Track_l,Track_r}.glb`, ~48 mil triângulos; folha ficha × nosso em `captures/game/models/gla_tank/preview/compare_ficha_v*.jpg` (`model_preview.py` + `model_sheet.py`) | **aprovado pelo Felipe (13/10)**. Pronto pra importar assim que a facção GLA entrar no jogo: os GLBs têm a origem do tanque (no chão, no centro), a torre gira em (0,35; 0) e o cano recua em +X; a importação precisa de 5 slots novos na tabela do `import_provisional.py` (Bag, Patch, Rust, Rubber, Wood). Falta a carcaça (`_Wreck`) | **A** |
| Oficina GLA (War Factory) | ficha `gla_war_factory_sheet` (galpão de barro com rede de camuflagem, chaminés, bandeiras) | nada | ficha aprovada | **A** |
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
