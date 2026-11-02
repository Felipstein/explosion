"""Leva os modelos PROVISÓRIOS da base pra Unreal (rodar: game.ps1 models, que antes gera os GLB
com o Blender: Tools/blender_provisional.py).

- importa cada GLB de captures/game/prov em /Game/Structures/<pasta>/ com Nanite;
- monta os materiais da base: M_Structure (opaco) e M_StructureBuild (a obra: corta acima da
  altura construída, com a linha de solda acesa no corte e o lado de dentro mais escuro), e uma
  instância de cada um por nome de material (Body, Trim, ... os nomes que o Blender deu);
- troca os materiais do importador pelos nossos (os do importador não têm a marca de Nanite:
  fix_unit_materials.py do BlastLab, 07/10) e apaga os dele.

O material (estudo de 10/10, o mesmo método do reboco das casas da sessão de gráficos):
- sem UV: o concreto escaneado (asphalt_04, CC0, da fatia do WorldForge) projetado nos 3 planos
  do mundo, e o resto com a variação larga dele (o metal pintado não fica "de plástico");
- terra no pé da parede e poeira no que olha pra cima: o prédio senta no deserto (a regra de
  coerência: o mesmo pó da areia);
- faixa de perigo procedural (preto e amarelo na diagonal do mundo);
- a cor do jogador por dado de primitiva (índices 0–3), a energia (4: as luzes apagam sem
  energia ou em obra) e a altura da obra (5, metros acima do pivô).
Sem a fatia (Slice), cai na textura de ruído do motor.
"""
import glob
import os

import unreal

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
SRC = os.path.join(REPO, 'captures', 'game', 'prov')
ROOT = '/Game/Structures'
MATS = ROOT + '/Materials'

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
log = lambda *a: unreal.log('PROVISIONAL ' + ' '.join(str(x) for x in a))
F1, F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT1, unreal.CustomMaterialOutputType.CMOT_FLOAT3

# nome do material no Blender → (cor linear, rugosidade, metal, extras)
# cores pela bíblia (CONCEITO_CONSTRUCOES.md): metal claro cinza-branco, concreto claro, amarelo
# de pórtico, a cor do jogador só em faixas e lonas; luz quente, nunca neon
SLOTS = {
    # (29/10, a sessão do Blender: o jogo contra o r1_raid_slabs e a ficha aprovada da War Factory) as tintas claras ×0,58 e o
    # amarelo ×0,75: medido na mesma câmera, os neutros da War Factory saíam 210,198,183 no jogo contra 98,89,87 no alvo e
    # 86,79,77 na ficha (o amarelo 236,183,70 contra 187,122,25 e 177,114,29); a parede da ficha lê ~1,1× a areia ao sol, ou
    # seja ~0,3 de albedo (era 0,50–0,62, tinta branca nova). Mais sujeira (Grime) no que é prédio
    # (29/10, mais tarde, o Felipe vendo a War Factory levar tiro: "essa textura da construção toda fodida tá horrível, dá uma
    # amenizada nisso") o desgaste no meio do caminho entre o de antes da v5 (sujeira 0,2–0,25, sem Weather) e o da v5 (0,35 e 1,0):
    # Weather 0,5 (as juntas, o tom por placa, o escorrido e a areia no pé pela metade) e a sujeira 0,25. As cores ficam (medidas)
    'Body': ((0.29, 0.284, 0.267), 0.6, 0.0, {'Grime': 0.25, 'Weather': 0.5}),
    'Trim': ((0.13, 0.13, 0.13), 0.45, 0.65, {'Grime': 0.15}),
    'Dark': ((0.025, 0.025, 0.025), 0.55, 0.3, {}),
    'Yellow': ((0.525, 0.30, 0.019), 0.55, 0.0, {'Grime': 0.25, 'Weather': 0.5}),
    'Player': ((0.5, 0.5, 0.5), 0.55, 0.0, {'PlayerMask': 1.0, 'Grime': 0.18, 'Weather': 0.5}),
    'Concrete': ((0.25, 0.24, 0.22), 0.92, 0.0, {'ConcreteMask': 1.0, 'Weather': 0.5}),
    'Hazard': ((0.54, 0.335, 0.016), 0.65, 0.0, {'Stripes': 1.0, 'Grime': 0.22, 'Weather': 0.5}),
    'Glass': ((0.008, 0.010, 0.013), 0.06, 0.0, {}),
    'Light': ((0.8, 0.7, 0.55), 0.4, 0.0, {'EmissiveColor': (1.0, 0.74, 0.46), 'EmissiveNits': 40000.0}),
    'Core': ((0.9, 0.86, 0.8), 0.3, 0.0, {'EmissiveColor': (1.0, 0.92, 0.80), 'EmissiveNits': 90000.0}),
    # (01/11, a usina v3 da USA: o microrreator com a janela do núcleo acesa em AZUL, a luz de Cherenkov; o Core quente segue nos outros)
    'UsCore': ((0.55, 0.75, 0.95), 0.3, 0.0, {'EmissiveColor': (0.35, 0.62, 1.0), 'EmissiveNits': 70000.0}),
    # (01/11, o Felipe: a usina precisa de "mais um aspecto que defina que é um power plant") o cabo de alta tensão da usina da USA: escuro,
    # com o pulso azul correndo do pórtico pra torre enquanto há energia (o Pulse; o Power do jogo apaga junto com as luzes)
    'PowerLine': ((0.03, 0.03, 0.032), 0.5, 0.4, {'EmissiveColor': (0.35, 0.62, 1.0), 'EmissiveNits': 25000.0, 'Pulse': 0.7}),
    'Beacon': ((0.8, 0.4, 0.05), 0.4, 0.0, {'EmissiveColor': (1.0, 0.42, 0.05), 'EmissiveNits': 70000.0, 'Blink': 1.4}),
    'Crate': ((0.10, 0.10, 0.045), 0.8, 0.0, {'Grime': 0.25}),
    'Rubber': ((0.012, 0.012, 0.012), 0.92, 0.0, {}),
    'Paint': ((0.34, 0.34, 0.325), 0.65, 0.0, {'Grime': 0.25, 'Weather': 0.5}),
    # o tanque do Look (look_tank.py, as cores dele em linear): a areia da USA, a cor do jogador
    # nas faixas, anéis e retalhos (slot Blue, que vira Team) e na lona (Cloth, mais escura)
    # (29/10) a tinta do M1 ×0,72 e mais sujeira: no r1_raid_slabs na mesma câmera o tanque da USA lê cinza-claro (p50 0,41,
    # 3,8% acima de 0,8) e o nosso saía branco estourado (0,65, 24%); continua 'cinza-claro, quase branco' da bíblia
    'TankPaint': ((0.40, 0.405, 0.38), 0.55, 0.0, {'Grime': 0.3}),
    'TankTeam': ((0.5, 0.5, 0.5), 0.5, 0.0, {'PlayerMask': 1.0, 'Grime': 0.15}),
    'TankCloth': ((0.5, 0.5, 0.5), 0.85, 0.0, {'PlayerMask': 1.0, 'Grime': 0.3}),
    'TankMetal': ((0.114, 0.114, 0.107), 0.45, 0.6, {'Grime': 0.15}),
    'TankTrack': ((0.036, 0.034, 0.030), 0.85, 0.3, {}),
    # (01/11) a esteira de elos que anda (o M_StructureTrack desliza os elos pela fase do jogo): só os modelos do elenco, que trazem a
    # tangente do elo no UV2/UV3 (model_run_detail.track_uv); o tanque aprovado da USA continua no TankTrack parado
    'TankTrackRun': ((0.036, 0.034, 0.030), 0.85, 0.3, {}),
    # (01/11, item 40) a rodagem da USA só dela (o TankMetal, a esteira e a borracha são da China e da GLA também): a poeira de baixo
    'UsPaint': ((0.40, 0.405, 0.38), 0.55, 0.0, {'Grime': 0.3}),
    'UsMetal': ((0.114, 0.114, 0.107), 0.45, 0.6, {'Grime': 0.15}),
    'UsTrackRun': ((0.036, 0.034, 0.030), 0.85, 0.3, {}),
    'UsRubber': ((0.012, 0.012, 0.012), 0.92, 0.0, {}),
    # (01/11, item 40, a 2ª volta dos prédios) a parede e o concreto da USA só dela (o Body e o Concrete são do avião de carga, do pátio
    # de suprimento e do canteiro também): a MESMA luz medida em 29/10 contra o alvo (~0,28 de luminância; a ficha lê ~1,1× a areia),
    # o tom mais quente (o alvo R/B 1,21; o nosso saía 1,08 e 1,11 na 1ª volta)
    'UsBody': ((0.300, 0.282, 0.262), 0.6, 0.0, {'Grime': 0.25, 'Weather': 0.5}),
    'UsConcrete': ((0.262, 0.243, 0.218), 0.92, 0.0, {'ConcreteMask': 1.0, 'Weather': 0.5}),
    # o kit civil da vila (as casas de adobe do Look, look_houses.py: as cores dele em linear) até o
    # M_Plaster do plugin RTSLook chegar
    'CivPlaster': ((0.49, 0.35, 0.20), 0.9, 0.0, {'Grime': 0.3, 'ToneVariation': 1.0}),
    'CivWood': ((0.107, 0.048, 0.019), 0.8, 0.0, {'Grime': 0.2}),
    'CivMetal': ((0.188, 0.181, 0.155), 0.5, 0.5, {'Grime': 0.25}),
    'CivCloth': ((0.305, 0.216, 0.122), 0.9, 0.0, {'Grime': 0.2}),
    # as carcaças e miudezas do kit (os slots do tanque do Look: carvão e ferrugem)
    'CivChar': ((0.010, 0.008, 0.007), 0.92, 0.0, {}),
    'CivRust': ((0.068, 0.027, 0.013), 0.85, 0.2, {'Grime': 0.2}),
    # (31/10, o elenco das 3 facções: docs/ELENCO.md; Game/Tools/model_faction_kit.py) a China: escura, massiva (o concreto puxado
    # pro oliva, de cima ~0,55× o claro da USA: docs/CONCEITO_UNIDADES.md §3), a telha verde vidrada, o aço escuro, o latão da
    # estrela, a boca do forno acesa; os veículos no oliva escuro com a mancha digital mais escura
    'CnConcrete': ((0.12, 0.125, 0.095), 0.85, 0.0, {'ConcreteMask': 1.0, 'Grime': 0.25, 'Weather': 0.5}),
    'CnPlinth': ((0.17, 0.168, 0.145), 0.9, 0.0, {'ConcreteMask': 1.0, 'Weather': 0.5}),
    'CnRoof': ((0.045, 0.095, 0.05), 0.42, 0.0, {'Grime': 0.2, 'Weather': 0.4}),
    'CnMetal': ((0.045, 0.047, 0.042), 0.5, 0.5, {'Grime': 0.15}),
    'CnGold': ((0.45, 0.30, 0.08), 0.35, 0.8, {}),
    'Fire': ((1.0, 0.45, 0.1), 0.4, 0.0, {'EmissiveColor': (1.0, 0.36, 0.05), 'EmissiveNits': 60000.0}),
    'CnTankPaint': ((0.075, 0.085, 0.048), 0.6, 0.0, {'Grime': 0.3}),
    # (01/11, a Rússia provisória: docs/FACCAO_RUSSIA.md §2, a paleta A até ele escolher) o aço frio azulado (L ~0,42 de cima), as manchas
    # grandes pretas e cinza-claras (Camo 2, ~1,8 m, o passo de 4 cm: a borda quase lisa); o concreto de painel; o verde da chapa
    # ondulada; o vermelho das listras e da estrela
    'RuPaint': ((0.118, 0.140, 0.158), 0.6, 0.0, {'Grime': 0.3}),
    # (01/11) o mesmo aço frio SEM a camuflagem, pros prédios (o rufo, a calha, a manta do teto plano, o contêiner): a mancha de 1,8 m
    # da camuflagem nos tetos lia como neve no telhado (o tour da Rússia, 01/11)
    'RuSteel': ((0.118, 0.140, 0.158), 0.6, 0.0, {'Grime': 0.3}),
    'RuMetal': ((0.05, 0.052, 0.055), 0.45, 0.6, {'Grime': 0.15}),
    'RuConcrete': ((0.20, 0.20, 0.19), 0.88, 0.0, {'ConcreteMask': 1.0, 'Grime': 0.25, 'Weather': 0.5}),
    'RuRoof': ((0.055, 0.085, 0.060), 0.55, 0.2, {'Grime': 0.25, 'Weather': 0.4}),
    'RuRed': ((0.30, 0.03, 0.025), 0.6, 0.0, {'Grime': 0.2}),
    'RuWhite': ((0.62, 0.62, 0.60), 0.6, 0.0, {'Grime': 0.25}),
    # (01/11, docs/elenco/EMBLEMAS.md) o azul da tricolor no escudo da Rússia (a estrela ficou com a China); o dourado da borda é o CnGold
    'RuBlue': ((0.02, 0.07, 0.30), 0.6, 0.0, {'Grime': 0.2}),
    'CnTankCamo': ((0.040, 0.048, 0.028), 0.65, 0.0, {'Grime': 0.3}),
    'CnTankMetal': ((0.05, 0.05, 0.045), 0.45, 0.6, {'Grime': 0.15}),
    # a GLA: o chão batido do lote, a rede de camuflagem, e os veículos no amarelo-areia fosco com a listra verde-escura gasta
    # (o adobe, a madeira, a lona e a ferrugem são os da vila: CivPlaster, CivWood, CivCloth, CivRust)
    'GlaEarth': ((0.36, 0.27, 0.165), 0.95, 0.0, {'Grime': 0.2}),
    'GlaNet': ((0.085, 0.10, 0.05), 0.9, 0.0, {}),
    'GlaPaint': ((0.40, 0.31, 0.14), 0.7, 0.0, {'Grime': 0.35}),
    'GlaStripe': ((0.05, 0.07, 0.03), 0.75, 0.0, {'Grime': 0.3}),
    # o azulejo turquesa gasto da cúpula do Palace
    'GlaTile': ((0.06, 0.20, 0.18), 0.4, 0.0, {'Grime': 0.3, 'Weather': 0.4}),
    # (31/10, os estados de dano: model_damage.py) o miolo que o corte mostra: o concreto claro e áspero; o barro do adobe
    'Broken': ((0.30, 0.29, 0.26), 0.95, 0.0, {'ConcreteMask': 1.0, 'Grime': 0.3}),
    'BrokenAdobe': ((0.40, 0.30, 0.18), 0.95, 0.0, {'Grime': 0.3}),
}
# (31/10, o detalhe AAA: docs/ESTUDO_DETALHE_AAA.md) o desgaste de quina, o pó nas frestas e o tom por chapa de cada slot. Só agem no
# modelo que traz as máscaras na cor de vértice (o model_detail.py); o modelo antigo, sem elas, fica igual. Por facção (a proposta do
# estudo §5: a USA pouco gasta, a China média, a GLA muito) e a camuflagem digital do tanque da China no material
MASK_PARAMS = {
    'CnTankPaint': {'Wear': 0.5, 'CavityDirt': 0.6, 'PanelTone': 1.0, 'Camo': 1.0, 'CamoPix': 0.16, 'CamoColor': (0.040, 0.048, 0.028)},
    'CnTankCamo': {'Wear': 0.5, 'CavityDirt': 0.6, 'PanelTone': 1.0},
    'CnTankMetal': {'Wear': 0.5, 'CavityDirt': 0.6, 'PanelTone': 0.6},
    'GlaPaint': {'Wear': 0.9, 'CavityDirt': 0.8, 'PanelTone': 1.0},
    'GlaStripe': {'Wear': 0.9, 'CavityDirt': 0.8, 'PanelTone': 1.0},
}
for _k in ('CnConcrete', 'CnPlinth', 'CnRoof', 'CnMetal', 'CnGold'):
    MASK_PARAMS[_k] = {'Wear': 0.4, 'CavityDirt': 0.7, 'PanelTone': 0.6}
MASK_PARAMS['RuPaint'] = {'Wear': 0.4, 'CavityDirt': 0.6, 'PanelTone': 0.45, 'Camo': 2.0, 'CamoPix': 0.04, 'CamoScale': 1.8,
                          'CamoColor': (0.022, 0.024, 0.026), 'CamoColor2': (0.30, 0.32, 0.32)}
for _k in ('RuMetal', 'RuSteel', 'RuConcrete', 'RuRoof', 'RuRed', 'RuWhite', 'RuBlue'):
    MASK_PARAMS[_k] = {'Wear': 0.4, 'CavityDirt': 0.7, 'PanelTone': 0.6}
for _k in ('GlaEarth', 'GlaNet', 'GlaTile'):
    MASK_PARAMS[_k] = {'Wear': 0.5, 'CavityDirt': 0.8, 'PanelTone': 0.5}
for _k in ('Body', 'Trim', 'Dark', 'Yellow', 'Player', 'Concrete', 'Hazard', 'Crate', 'Rubber', 'Paint', 'TankPaint', 'TankTeam',
           'TankCloth', 'TankMetal', 'TankTrack', 'TankTrackRun', 'CivPlaster', 'CivWood', 'CivMetal', 'CivCloth', 'CivRust'):
    MASK_PARAMS[_k] = {'Wear': 0.35, 'CavityDirt': 0.6, 'PanelTone': 0.6}
# (01/11, item 40: docs/elenco/TEXTURA_USA.md) a USA em campanha no deserto, contra o r1_raid_slabs: a poeira grossa no baixo e fina em
# cima, a lavagem escura na fresta funda, pouca lasca; a areia na fresta quase some (a tinta clara tem o valor dela). A cor do jogador
# fica forte (o Felipe, 25/10: "até a do game shot é fraca"): o TankTeam e a lona não levam poeira. A tinta da terra é a UsPaint
# (a TankPaint segue nos aéreos, sem a poeira de baixo)
MASK_PARAMS['UsPaint'] = {'Wear': 0.35, 'CavityDirt': 0.15, 'PanelTone': 0.8, 'DustCoat': 0.7, 'Wash': 0.8, 'Chip': 0.6}
MASK_PARAMS['UsMetal'] = {'Wear': 0.35, 'CavityDirt': 0.2, 'PanelTone': 0.6, 'DustCoat': 0.8, 'Wash': 0.4}
MASK_PARAMS['UsTrackRun'] = {'Wear': 0.35, 'CavityDirt': 0.6, 'PanelTone': 0.6, 'DustCoat': 0.9}
MASK_PARAMS['UsRubber'] = {'Wear': 0.35, 'CavityDirt': 0.6, 'PanelTone': 0.6, 'DustCoat': 0.8}
# (01/11 17h, item 40 nos prédios: a War Factory B no jogo contra a do r1_raid_slabs, a única aprovada da base: o alvo bege-quente,
# R/B 1,21 e a variação 0,59 (escorrido, junta escura, lasca, pó); o nosso cinza-frio, 1,08 e 0,34) a parede da USA (o Body, só dela)
# na mesma campanha dos veículos: o véu e o pó no pé, a lavagem escura nas juntas e o escorrido, poucas lascas
MASK_PARAMS['Body'] = {'Wear': 0.35, 'CavityDirt': 0.2, 'PanelTone': 0.8, 'DustCoat': 0.5, 'Wash': 0.8, 'Chip': 0.5}
# (01/11, 2ª volta) a parede e o concreto da USA: o pó mais forte (o véu quente e o pé), a lavagem e o tom por placa no máximo (a
# variação do alvo 0,59 contra 0,34)
MASK_PARAMS['UsBody'] = {'Wear': 0.35, 'CavityDirt': 0.2, 'PanelTone': 1.0, 'DustCoat': 0.65, 'Wash': 1.0, 'Chip': 0.5}
MASK_PARAMS['UsConcrete'] = {'Wear': 0.35, 'CavityDirt': 0.6, 'PanelTone': 0.8, 'DustCoat': 0.8, 'Wash': 0.6}
MASK_PARAMS['Glass'] = {'CavityDirt': 0.4, 'GlassMask': 1.0}
for _k, _v in MASK_PARAMS.items():
    SLOTS[_k][3].update(_v)
# (31/10) os slots de cada família de modelo do elenco → a instância (o que não está aqui fica com o próprio nome)
CHINA_BUILDING_SLOTS = {'Rubber': 'Rubber', 'Metal': 'CnMetal', 'Wood': 'CivWood', 'Cloth': 'CivCloth', 'Broken': 'Broken',
                        'Red': 'RuRed'}   # (01/11) o vermelho fixo da estrela do ELP (a borda dourada é o CnGold)
GLA_BUILDING_SLOTS = {'Plaster': 'CivPlaster', 'Wood': 'CivWood', 'Cloth': 'CivCloth', 'Rust': 'CivRust', 'Metal': 'CivMetal',
                      'Earth': 'GlaEarth', 'Net': 'GlaNet', 'Tile': 'GlaTile', 'Broken': 'BrokenAdobe'}
CHINA_VEHICLE_SLOTS = {'Team': 'TankTeam', 'Blue': 'TankTeam', 'Cloth': 'CivCloth', 'Track': 'TankTrackRun', 'Lens': 'Glass',
                       'Metal': 'TankMetal', 'Paint': 'CnTankPaint', 'Red': 'RuRed', 'Gold': 'CnGold'}
# (01/11) a Rússia: os slots de cada família (veículos e prédios) → a instância
RUSSIA_VEHICLE_SLOTS = {'Team': 'TankTeam', 'Blue': 'TankTeam', 'Cloth': 'CivCloth', 'Track': 'TankTrackRun', 'Lens': 'Glass',
                        'Metal': 'RuMetal', 'Paint': 'RuPaint', 'Red': 'RuRed', 'White': 'RuWhite', 'Rubber': 'Rubber',
                        'Navy': 'RuBlue'}   # (01/11) o azul da bandeira tricolor na torre (a estrela saiu)
RUSSIA_BUILDING_SLOTS = {'Rubber': 'Rubber', 'Metal': 'RuMetal', 'Wood': 'CivWood', 'Cloth': 'CivCloth', 'Broken': 'Broken',
                         'Concrete': 'RuConcrete', 'Roof': 'RuRoof', 'Red': 'RuRed', 'White': 'RuWhite', 'Paint': 'RuSteel',
                         'Team': 'Player', 'Navy': 'RuBlue', 'Gold': 'CnGold'}
GLA_VEHICLE_SLOTS = {'Team': 'TankTeam', 'Blue': 'TankTeam', 'Cloth': 'CivCloth', 'Track': 'TankTrack', 'Lens': 'Glass',
                     'Metal': 'TankMetal', 'Rust': 'CivRust', 'Wood': 'CivWood', 'Paint': 'GlaPaint', 'Bag': 'CivCloth',
                     'Plaster': 'CivPlaster'}
# o slot do Blender no tanque do Look → a instância nossa
TANK_SLOTS = {'Paint': 'TankPaint', 'Blue': 'TankTeam', 'Team': 'TankTeam', 'Cloth': 'TankCloth', 'Dark': 'Dark',
              'Metal': 'TankMetal', 'Lens': 'Glass', 'Track': 'TankTrack'}
TANK_SRC = os.path.join(REPO, 'captures', 'look', 'tank_glb')
TANK_PARTS = ['Hull', 'Turret', 'Barrel', 'Wheels', 'Track_l', 'Track_r']
TANK_ROOT = '/Game/Units/USA_MBT_LL'
# o Dozer provisório por script (sessão Modelos 3D, 13/10, aprovado pelo Felipe): Game/Tools/model_usa_dozer.py →
# captures/game/models/usa_dozer; os slots do tanque mais o amarelo da lâmina, a cúpula da sirene (pisca) e a lâmpada
DOZER_SRC = os.path.join(REPO, 'captures', 'game', 'models', 'usa_dozer')
DOZER_ROOT = '/Game/Units/USA_Dozer'
DOZER_SLOTS = dict(TANK_SLOTS, **{'Yellow': 'Yellow', 'Beacon': 'Beacon', 'Light': 'Light'})
# (01/11, item 40) os veículos de terra do elenco da USA (o tanque B, as 5 unidades B de terra e a Paladin): a rodagem nas instâncias só
# dela. Os aéreos ficam no 'dozer' (a poeira de baixo é da rodagem, não da barriga do avião)
USA_VEHICLE_SLOTS = dict(DOZER_SLOTS, Paint='UsPaint', Metal='UsMetal', Track='UsTrackRun', Rubber='UsRubber')
# (01/11, item 40) os prédios da USA em campanha: a parede, o concreto e a tinta dos detalhes só dela
USA_BUILDING_SLOTS = {'Body': 'UsBody', 'Concrete': 'UsConcrete', 'Paint': 'UsPaint'}
# o caminhão de suprimento novo (Modelos 3D, 14/10, aprovado pelo Felipe): o casco, as 6 rodas e a caixa pequena da carga,
# com a mesma tabela de slots do Dozer
TRUCK_SRC = os.path.join(REPO, 'captures', 'game', 'models', 'usa_supply_truck')
TRUCK_ROOT = '/Game/Units/USA_SupplyTruck'
# a defesa de base da USA (a Patriot; Modelos 3D, 14/10, aprovada pelo Felipe): a base, a caixa lançadora (torre), o radar
# (spin) e o andaime, com os slots dos prédios (Body, Trim, Dark, Player, Concrete, Hazard, Glass, Light, Beacon, Yellow)
PATRIOT_SRC = os.path.join(REPO, 'captures', 'game', 'models', 'usa_patriot')
PATRIOT_ROOT = '/Game/Structures/USA_Patriot'
# a vila (12/10): o kit civil do Look em captures/look/civil_glb/SM_civ_<tipo>.glb, com os tipos do
# village.json do WorldForge; o jogo põe cada peça pelo JSON (RTSMatchSubsystem::SpawnVillage)
# a v4 (22–23/10, provisórios por script da frente dos Modelos 3D): a Supply Drop Zone (prédio, slots das construções), o
# avião de carga e a caixa com paraquedas do lançamento (efeitos; slots das construções) e o Tomahawk (o chassi do caminhão,
# slots do Dozer). As pastas e os destinos: (pasta em captures/game/models, glob, destino, tabela de slots)
V4_MODELS = [
    ('usa_drop_zone', 'SM_USA_DropZone_*.glb', '/Game/Structures/usa_drop_zone', None),
    ('cargo_plane', 'SM_USA_CargoPlane*.glb', '/Game/Units/USA_CargoPlane', None),
    ('supply_drop', 'SM_SupplyDrop_*.glb', '/Game/Structures/SupplyDrop', None),
    ('usa_tomahawk', 'SM_USA_Tomahawk_*.glb', '/Game/Units/USA_Tomahawk', 'dozer'),
]
# as aeronaves da USA e o Airfield (24/10, provisórios por script: model_usa_airfield/raptor/comanche/chinook.py): o mesmo
# formato do V4_MODELS; o Airfield com os slots das construções, as aeronaves com os do caminhão (DOZER_SLOTS)
AIR_MODELS = [
    ('usa_airfield', 'SM_USA_Airfield_*.glb', '/Game/Structures/usa_airfield', None),
    ('usa_raptor', 'SM_USA_Raptor_*.glb', '/Game/Units/USA_Raptor', 'dozer'),
    ('usa_comanche', 'SM_USA_Comanche_*.glb', '/Game/Units/USA_Comanche', 'dozer'),
    ('usa_chinook', 'SM_USA_Chinook_*.glb', '/Game/Units/USA_Chinook', 'dozer'),
]
# (25/10, v5) os veículos de chão novos da USA (provisórios por script: model_usa_linebacker.py, o antiaéreo de lagarta, e
# model_usa_stryker.py, o transporte 8x8): o mesmo formato do V4_MODELS, com os slots do Dozer (a lona Cloth vira a TankCloth)
V5_MODELS = [
    ('usa_linebacker', 'SM_USA_Linebacker_*.glb', '/Game/Units/USA_Linebacker', 'dozer'),
    ('usa_stryker', 'SM_USA_Stryker_*.glb', '/Game/Units/USA_Stryker', 'dozer'),
]
# (31/10, o elenco das 3 facções: docs/ELENCO.md) os provisórios por script do elenco da China, da GLA e o Paladin da USA (os
# Game/Tools/model_<chave>.py): o mesmo formato do V4_MODELS, com a tabela de slots de cada família. Os prédios em
# /Game/Structures/<chave>, os veículos em /Game/Units/<chave>
FACTION_MODELS = [
    # China, prédios
    ('china_command_center', 'SM_China_CommandCenter_*.glb', '/Game/Structures/china_command_center', CHINA_BUILDING_SLOTS),
    ('china_power_plant', 'SM_China_PowerPlant_*.glb', '/Game/Structures/china_power_plant', CHINA_BUILDING_SLOTS),
    ('china_supply_center', 'SM_China_SupplyCenter_*.glb', '/Game/Structures/china_supply_center', CHINA_BUILDING_SLOTS),
    ('china_barracks', 'SM_China_Barracks_*.glb', '/Game/Structures/china_barracks', CHINA_BUILDING_SLOTS),
    ('china_war_factory', 'SM_China_WarFactory_*.glb', '/Game/Structures/china_war_factory', CHINA_BUILDING_SLOTS),
    ('china_propaganda_center', 'SM_China_PropagandaCenter_*.glb', '/Game/Structures/china_propaganda_center', CHINA_BUILDING_SLOTS),
    ('china_gattling_cannon', 'SM_China_GattlingCannon_*.glb', '/Game/Structures/china_gattling_cannon', CHINA_BUILDING_SLOTS),
    # (01/11, a galeria: itens 44 e 46) o Gattling Cannon e o Propaganda Center refeitos (v2); os de antes ficam nas pastas deles
    ('china_gattling_cannon_v2', 'SM_China_GattlingCannon_*.glb', '/Game/Structures/china_gattling_cannon_v2', CHINA_BUILDING_SLOTS),
    ('china_propaganda_center_v2', 'SM_China_PropagandaCenter_*.glb', '/Game/Structures/china_propaganda_center_v2', CHINA_BUILDING_SLOTS),
    # China, veículos
    ('china_dozer', 'SM_China_Dozer_*.glb', '/Game/Units/china_dozer', CHINA_VEHICLE_SLOTS),
    ('china_supply_truck', 'SM_China_SupplyTruck_*.glb', '/Game/Units/china_supply_truck', CHINA_VEHICLE_SLOTS),
    ('china_type99', 'SM_China_Type99_*.glb', '/Game/Units/china_type99', CHINA_VEHICLE_SLOTS),
    ('china_gattling_tank', 'SM_China_GattlingTank_*.glb', '/Game/Units/china_gattling_tank', CHINA_VEHICLE_SLOTS),
    ('china_inferno_cannon', 'SM_China_InfernoCannon_*.glb', '/Game/Units/china_inferno_cannon', CHINA_VEHICLE_SLOTS),
    ('china_overlord', 'SM_China_Overlord_*.glb', '/Game/Units/china_overlord', CHINA_VEHICLE_SLOTS),
    # GLA, prédios
    ('gla_command_center', 'SM_GLA_CommandCenter_*.glb', '/Game/Structures/gla_command_center', GLA_BUILDING_SLOTS),
    ('gla_supply_stash', 'SM_GLA_SupplyStash_*.glb', '/Game/Structures/gla_supply_stash', GLA_BUILDING_SLOTS),
    ('gla_barracks', 'SM_GLA_Barracks_*.glb', '/Game/Structures/gla_barracks', GLA_BUILDING_SLOTS),
    ('gla_arms_dealer', 'SM_GLA_ArmsDealer_*.glb', '/Game/Structures/gla_arms_dealer', GLA_BUILDING_SLOTS),
    ('gla_palace', 'SM_GLA_Palace_*.glb', '/Game/Structures/gla_palace', GLA_BUILDING_SLOTS),
    ('gla_stinger_site', 'SM_GLA_StingerSite_*.glb', '/Game/Structures/gla_stinger_site', GLA_BUILDING_SLOTS),
    # GLA, veículos
    ('gla_supply_pickup', 'SM_GLA_SupplyPickup_*.glb', '/Game/Units/gla_supply_pickup', GLA_VEHICLE_SLOTS),
    ('gla_quad_cannon', 'SM_GLA_QuadCannon_*.glb', '/Game/Units/gla_quad_cannon', GLA_VEHICLE_SLOTS),
    ('gla_rocket_buggy', 'SM_GLA_RocketBuggy_*.glb', '/Game/Units/gla_rocket_buggy', GLA_VEHICLE_SLOTS),
    ('gla_toxin_tractor', 'SM_GLA_ToxinTractor_*.glb', '/Game/Units/gla_toxin_tractor', GLA_VEHICLE_SLOTS),
    # USA: o Paladin (os slots do tanque) e o Strategy Center (22/10, fora do jogo até aqui)
    ('usa_paladin', 'SM_USA_Paladin_*.glb', '/Game/Units/usa_paladin', 'usa'),
    # (01/11) a Rússia provisória (docs/FACCAO_RUSSIA.md)
    ('russia_command_center', 'SM_Russia_CommandCenter_*.glb', '/Game/Structures/russia_command_center', RUSSIA_BUILDING_SLOTS),
    ('russia_power_plant', 'SM_Russia_PowerPlant_*.glb', '/Game/Structures/russia_power_plant', RUSSIA_BUILDING_SLOTS),
    ('russia_supply_center', 'SM_Russia_SupplyCenter_*.glb', '/Game/Structures/russia_supply_center', RUSSIA_BUILDING_SLOTS),
    ('russia_barracks', 'SM_Russia_Barracks_*.glb', '/Game/Structures/russia_barracks', RUSSIA_BUILDING_SLOTS),
    ('russia_war_factory', 'SM_Russia_WarFactory_*.glb', '/Game/Structures/russia_war_factory', RUSSIA_BUILDING_SLOTS),
    ('russia_institute', 'SM_Russia_Institute_*.glb', '/Game/Structures/russia_institute', RUSSIA_BUILDING_SLOTS),
    ('russia_pantsir_site', 'SM_Russia_PantsirSite_*.glb', '/Game/Structures/russia_pantsir_site', RUSSIA_BUILDING_SLOTS),
    ('russia_dozer', 'SM_Russia_Dozer_*.glb', '/Game/Units/russia_dozer', RUSSIA_VEHICLE_SLOTS),
    # (01/11, o tanque da USA B aprovado: "você refez o tanque e ficou super bom") a versão B (Game/Tools/model_usa_mbt.py) numa pasta
    # própria; a A (o tanque do Look, /Game/Units/USA_MBT_LL) fica guardada. Os slots do tanque, com a esteira que anda
    ('usa_mbt', 'SM_LL_USATank_*.glb', '/Game/Units/usa_mbt_v2', 'usa'),
    # (01/11, o Felipe: "as novas que você fez todas ficaram muito boas [...] mas os outros continuaram no lego") as versões B das
    # unidades da USA (os mesmos nomes e pivôs das A), cada uma numa pasta própria; as A ficam guardadas nas pastas de antes
    ('usa_dozer_v2', 'SM_USA_Dozer_*.glb', '/Game/Units/usa_dozer_v2', 'usa'),
    ('usa_supply_truck_v2', 'SM_USA_Supply*.glb', '/Game/Units/usa_supply_truck_v2', 'usa'),
    ('usa_stryker_v2', 'SM_USA_Stryker_*.glb', '/Game/Units/usa_stryker_v2', 'usa'),
    ('usa_linebacker_v2', 'SM_USA_Linebacker_*.glb', '/Game/Units/usa_linebacker_v2', 'usa'),
    ('usa_tomahawk_v2', 'SM_USA_Tomahawk_*.glb', '/Game/Units/usa_tomahawk_v2', 'usa'),
    ('usa_raptor_v2', 'SM_USA_Raptor_*.glb', '/Game/Units/usa_raptor_v2', 'dozer'),
    ('usa_comanche_v2', 'SM_USA_Comanche_*.glb', '/Game/Units/usa_comanche_v2', 'dozer'),
    ('usa_chinook_v2', 'SM_USA_Chinook_*.glb', '/Game/Units/usa_chinook_v2', 'dozer'),
    # (01/11) os prédios B da USA (os nomes, os slots e os pivôs do A), com os 3 estados de dano do destrutor; os A ficam nas pastas de antes
    ('usa_patriot_v2', 'SM_USA_Patriot_*.glb', '/Game/Structures/usa_patriot_v2', USA_BUILDING_SLOTS),
    ('usa_drop_zone_v2', 'SM_USA_DropZone_*.glb', '/Game/Structures/usa_drop_zone_v2', USA_BUILDING_SLOTS),
    ('usa_strategy_center_v2', 'SM_USA_StrategyCenter_*.glb', '/Game/Structures/usa_strategy_center_v2', USA_BUILDING_SLOTS),
    ('usa_airfield_v2', 'SM_USA_Airfield_*.glb', '/Game/Structures/usa_airfield_v2', USA_BUILDING_SLOTS),
    # (01/11, item 38: "refazer as construções iniciais da USA, ficaram todas quadradas, sem vida") o Supply Center, o quartel e a War
    # Factory B (os agentes de 01/11); o Command Center e a usina saem do zero (v3: o Pentágono e a subestação)
    ('usa_supply_center_v2', 'SM_USA_SupplyCenter_*.glb', '/Game/Structures/usa_supply_center_v2', USA_BUILDING_SLOTS),
    ('usa_barracks_v2', 'SM_USA_Barracks_*.glb', '/Game/Structures/usa_barracks_v2', USA_BUILDING_SLOTS),
    ('usa_war_factory_v2', 'SM_USA_WarFactory_*.glb', '/Game/Structures/usa_war_factory_v2', USA_BUILDING_SLOTS),
    # (01/11) o Command Center e a usina do zero (docs/elenco/USA_CC_E_USINA.md): o Pentágono e a subestação com a torre de alta tensão
    ('usa_command_center_v3', 'SM_USA_CommandCenter_*.glb', '/Game/Structures/usa_command_center_v3', USA_BUILDING_SLOTS),
    ('usa_power_plant_v3', 'SM_USA_PowerPlant_*.glb', '/Game/Structures/usa_power_plant_v3', dict(USA_BUILDING_SLOTS, Core='UsCore')),
    # (01/11, item 57/41) o Paladin v2 (docs/elenco/SILHUETAS_USA.md: o maior de lagarta, o nariz em V, o laser grande) e (item 56) o
    # Strategy Center v3 (o Pentágono como centro de inteligência: a radomo grande no pátio, sem o portão)
    ('usa_paladin_v2', 'SM_USA_Paladin_*.glb', '/Game/Units/usa_paladin_v2', 'usa'),
    # (01/11, item 38) o Command Center v4: o do Zero Hour quase copiado (docs/elenco/USA_CC_E_USINA.md §5), no acabamento das aprovadas
    ('usa_command_center_v4', 'SM_USA_CommandCenter_*.glb', '/Game/Structures/usa_command_center_v4', USA_BUILDING_SLOTS),
    ('usa_strategy_center_v3', 'SM_USA_StrategyCenter_*.glb', '/Game/Structures/usa_strategy_center_v3', USA_BUILDING_SLOTS),
    # (01/11, item 38) o Command Center v5: o nosso do zero, com a gramática do ZH e as formas próprias (docs/elenco/USA_CC_E_USINA.md §6)
    ('usa_command_center_v5', 'SM_USA_CommandCenter_*.glb', '/Game/Structures/usa_command_center_v5', USA_BUILDING_SLOTS),
    # (01/11, item 46) o Propaganda Center v3: a emissora no laço da CCTV com o telão aceso (docs/elenco/CHINA_PROPAGANDA.md)
    ('china_propaganda_center_v3', 'SM_China_PropagandaCenter_*.glb', '/Game/Structures/china_propaganda_center_v3', CHINA_BUILDING_SLOTS),
    # (01/11, itens 53, 54, 55) a Rússia refeita: o BMPT e o T-14 com a silhueta própria (longe do T-90M), o Shtab da torre em degraus
    # com a agulha (não a "prisão") e a Kazarma com o plaç pintado e o KPP
    ('russia_bmpt_v2', 'SM_Russia_BMPT_*.glb', '/Game/Units/russia_bmpt_v2', RUSSIA_VEHICLE_SLOTS),
    ('russia_t14_v2', 'SM_Russia_T14_*.glb', '/Game/Units/russia_t14_v2', RUSSIA_VEHICLE_SLOTS),
    ('russia_command_center_v2', 'SM_Russia_CommandCenter_*.glb', '/Game/Structures/russia_command_center_v2', RUSSIA_BUILDING_SLOTS),
    ('russia_barracks_v2', 'SM_Russia_Barracks_*.glb', '/Game/Structures/russia_barracks_v2', RUSSIA_BUILDING_SLOTS),
    ('russia_supply_truck', 'SM_Russia_SupplyTruck_*.glb', '/Game/Units/russia_supply_truck', RUSSIA_VEHICLE_SLOTS),
    ('russia_t90m', 'SM_Russia_T90M_*.glb', '/Game/Units/russia_t90m', RUSSIA_VEHICLE_SLOTS),
    ('russia_bmpt', 'SM_Russia_BMPT_*.glb', '/Game/Units/russia_bmpt', RUSSIA_VEHICLE_SLOTS),
    ('russia_pantsir', 'SM_Russia_Pantsir_*.glb', '/Game/Units/russia_pantsir', RUSSIA_VEHICLE_SLOTS),
    ('russia_tos1a', 'SM_Russia_TOS1A_*.glb', '/Game/Units/russia_tos1a', RUSSIA_VEHICLE_SLOTS),
    ('russia_t14', 'SM_Russia_T14_*.glb', '/Game/Units/russia_t14', RUSSIA_VEHICLE_SLOTS),
    ('usa_strategy_center', 'SM_USA_StrategyCenter_*.glb', '/Game/Structures/usa_strategy_center', None),
    # as armas da infantaria antitanque (model_infantry_launchers.py: RPG-7, Javelin, PF-98), na mão no lugar do fuzil
    ('infantry_launchers', 'SM_Launcher_*.glb', '/Game/Weapons/Launchers',
     {'Metal': 'TankMetal', 'Wood': 'CivWood', 'Paint': 'CnTankPaint', 'Lens': 'Glass'}),
    # (01/11, o canteiro da obra não começada: Game/Tools/model_site_props.py) o tapume novo (a chapa clara, a faixa na cor do jogador ou a
    # zebra do A/B), as estacas com a fita, o material empilhado (o bloco, o vergalhão, o cimento, a areia, a brita)
    ('site_props', 'SM_Site_*.glb', '/Game/Structures/site_props',
     {'Team': 'Player', 'Metal': 'Paint', 'Wood': 'CivWood', 'Cloth': 'CivCloth', 'Plaster': 'CivPlaster', 'Rust': 'CivRust'}),
]
CIVIL_SRC = os.path.join(REPO, 'captures', 'look', 'civil_glb')
CIVIL_ROOT = '/Game/Village'
CIVIL_SLOTS = {'Plaster': 'CivPlaster', 'Wood': 'CivWood', 'Metal': 'CivMetal', 'Cloth': 'CivCloth', 'Dark': 'Dark',
               'Paint': 'TankPaint', 'Lens': 'Glass', 'Track': 'TankTrack', 'Blue': 'CivCloth', 'Team': 'CivCloth',
               'Char': 'CivChar', 'Rust': 'CivRust'}

CONCRETE = '/Game/Slice/Surfaces/asphalt_04/T_asphalt_04_diff_4k'
CONCRETE_N = '/Game/Slice/Surfaces/asphalt_04/T_asphalt_04_nor_dx_4k'
STAIN = '/Game/Slice/Surfaces/aerial_beach_01/T_aerial_beach_01_diff_4k'
NOISE = '/Engine/EngineMaterials/Good64x64TilingNoiseHighFreq'
FLAT_N = '/Engine/EngineMaterials/DefaultNormal'

CODE = r"""
// As manchas da carcaça e do dano (13/10): ruído de valor 3D sem textura (o hash do Inigo Quilez), 3 oitavas.
// A foto da praia, limiarizada, dava chuvisco em qualquer mip (ela não tem mancha larga), e na distância do jogo
// o mip fixo serrilhava: a carcaça saía granulada como casca. N() sai esticado pela faixa medida em 400 mil pontos
// (p5/p95 0,30/0,70 → 0/1), então os limiares da receita do Look dão a cobertura dele: fuligem 23%, ferrugem
// 15%, cinza 21%.
struct FRTSNoise
{
    float H(float3 p) { p = frac(p * 0.3183099 + 0.1); p *= 17.0; return frac(p.x * p.y * p.z * (p.x + p.y + p.z)); }
    float V(float3 x)
    {
        float3 i = floor(x);
        float3 f = frac(x);
        f = f * f * (3.0 - 2.0 * f);
        return lerp(lerp(lerp(H(i), H(i + float3(1, 0, 0)), f.x), lerp(H(i + float3(0, 1, 0)), H(i + float3(1, 1, 0)), f.x), f.y),
                    lerp(lerp(H(i + float3(0, 0, 1)), H(i + float3(1, 0, 1)), f.x), lerp(H(i + float3(0, 1, 1)), H(i + float3(1, 1, 1)), f.x), f.y), f.z);
    }
    float N(float3 x) { return saturate(((0.5 * V(x) + 0.25 * V(x * 2.03 + 17.1) + 0.125 * V(x * 4.01 + 31.7)) / 0.875 - 0.30) / 0.40); }
};
FRTSNoise Nz;
float3 n = normalize(N);
float3 w = pow(abs(n), 4.0); w /= (w.x + w.y + w.z);
float3 q = WP * 0.01;
float3 P = (WP - Obj) * 0.01;
float T = 2.4;
float3 c = Conc.Sample(ConcSampler, q.yz / T).rgb * w.x + Conc.Sample(ConcSampler, q.xz / T + 0.31).rgb * w.y
         + Conc.Sample(ConcSampler, q.xy / T + 0.57).rgb * w.z;
float lum = dot(c, float3(0.3, 0.59, 0.11));
float stain = Stain.Sample(StainSampler, q.yz / 9.0).g * w.x + Stain.Sample(StainSampler, q.xz / 9.0 + 0.2).g * w.y
            + Stain.Sample(StainSampler, q.xy / 9.0 + 0.4).g * w.z;
float3 base = lerp(BaseColor, PlayerColor.rgb * 0.45, PlayerMask);
// (31/10, o detalhe AAA: docs/ESTUDO_DETALHE_AAA.md §2.2) a camuflagem digital no material, não em blocos de geometria colados (o
// "lego" literal): células de CamoPix no espaço da peça (continua de face pra face e anda com o veículo), o ruído largo de ~1,1 m
// lido no centro da célula; o pó, a terra e o desgaste vêm por cima
if (Camo > 1.5)
{
    // (01/11, a Rússia: docs/FACCAO_RUSSIA.md §2) Camo 2 = os 3 tons em manchas grandes de tinta de pistola. (01/11 12h, o Felipe: "a
    // textura ficou quadriculada, pixelada, longe de uma textura foda de RTS AAA") a mancha herdava a grade da digital da China (a
    // borda em escada de 4 cm, seca): agora é orgânica, o ruído largo com o domínio torcido (Quilez, "domain warping"), e a borda é a
    // da pistola, borrada e sem serrilhado (o fwidth: nunca mais fina que o pixel na tela)
    float3 x = LP * 0.01 / max(CamoScale, 0.2);
    float3 wq = float3(Nz.N(x * 1.7 + 3.1), Nz.N(x * 1.7 + 11.7), Nz.N(x * 1.7 + 23.3)) - 0.5;
    float cv = Nz.N(x + wq * 0.55 + 7.3);
    float e = max(fwidth(cv) * 1.5, 0.035);
    float dark = smoothstep(0.56 - e, 0.56 + e, cv);
    float light = 1.0 - smoothstep(0.36 - e, 0.36 + e, cv);
    base = lerp(base, CamoColor.rgb, dark * (1.0 - PlayerMask));
    base = lerp(base, CamoColor2.rgb, light * (1.0 - PlayerMask));
}
else if (Camo > 0.5)
{
    float3 cc = floor(LP * 0.01 / max(CamoPix, 0.02)) + 0.5;
    float cv = Nz.N(cc * max(CamoPix, 0.02) / max(CamoScale, 0.2) + 7.3);
    base = lerp(base, CamoColor.rgb, step(0.56, cv) * (1.0 - PlayerMask));
}
// metal pintado: a variação larga do concreto, fraca (manchas de uso, não textura)
base *= lerp(1.0 - Grime, 1.0 + Grime * 0.35, smoothstep(0.55, 1.45, lum / 0.26));
// concreto: o escaneado puxado pra cor pedida
base = lerp(base, c / 0.26 * BaseColor, ConcreteMask);
// faixa de perigo: preto e amarelo a 45° no mundo (0,45 m cada)
if (Stripes > 0.5 && frac((P.x + P.y + P.z) / 0.9) < 0.5) base = float3(0.018, 0.017, 0.016);
base *= lerp(0.86, 1.04, saturate((stain - 0.15) / 0.2));
// (29/10, a sessão do Blender: o r1_raid_slabs e a ficha aprovada da War Factory, medidos na mesma câmera) o desgaste
// do prédio: placas de 3 × 2,4 m com junta escura e um tom por placa, escorrido de sujeira na parede e a areia batida no
// pé. Tudo no espaço da peça (vai junto com o que anda); a normal da peça sai das derivadas da posição local (sem entrada
// nova: o M_UnitAtlas usa o mesmo código). Só nas peças grandes (> ~9 m): no veículo as juntas viravam riscos
if (Weather > 0.0)
{
    float3 Lw = LP * 0.01;
    float3 Bw0 = LMin * 0.01;
    float3 Bw1 = LMax * 0.01;
    float big = smoothstep(8.5, 11.0, max(Bw1.x - Bw0.x, Bw1.y - Bw0.y)) * Weather;
    float3 ln = abs(normalize(cross(ddx(Lw), ddy(Lw)) + 1e-7));
    float wall = 1.0 - smoothstep(0.4, 0.6, ln.z);
    float along = ln.x > ln.y ? Lw.y : Lw.x;
    float2 cs = wall > 0.5 ? float2(3.0, 2.4) : float2(3.0, 3.0);
    float2 cell = (wall > 0.5 ? float2(along, Lw.z) : Lw.xy) / cs;
    float2 fc = frac(cell);
    float2 dd = min(fc, 1.0 - fc) * cs;
    float seam = 1.0 - smoothstep(0.02, 0.05, min(dd.x, dd.y));
    float tone = frac(sin(dot(floor(cell) + (ln.x > ln.y ? 0.37 : 0.0), float2(12.9898, 78.233))) * 43758.5453);
    float streak = smoothstep(0.45, 0.75, Nz.N(float3(along * 2.2, along * 2.2, Lw.z * 0.22) + 7.0)) * wall;
    base *= lerp(1.0, lerp(0.84, 1.08, tone) * (1.0 - 0.45 * seam) * (1.0 - 0.3 * streak), big);
    float sandFoot = (1.0 - smoothstep(0.0, 1.0 + 1.4 * stain, Lw.z - Bw0.z)) * wall;
    base = lerp(base, float3(0.375, 0.27, 0.165), 0.55 * sandFoot * big);
    // (29/10, mais tarde: o r1_calm_slabs, a laje bege com areia por cima; a nossa saía cinza-fria e limpa) a laje de concreto
    // no deserto: um véu de pó no que olha pra cima e a areia soprada por cima, mais na borda (vem do chão em volta, até
    // ~1,5–3 m pra dentro) e em manchas de 2–5 m; a cor é a areia de duna do mapa
    float flat = (1.0 - wall) * ConcreteMask * big;
    float de = min(min(Lw.x - Bw0.x, Bw1.x - Lw.x), min(Lw.y - Bw0.y, Bw1.y - Lw.y));
    float low = 1.0 - smoothstep(0.4, 0.9, Lw.z - Bw0.z);
    float drift = max((1.0 - smoothstep(0.2, 1.4 + 1.6 * Nz.N(float3(Lw.xy * 0.45, 3.1)), de)) * low,
                      0.7 * smoothstep(0.62, 0.8, Nz.N(float3(Lw.xy * 0.3, 9.7))));
    base = lerp(base, float3(0.42, 0.29, 0.17), flat * (0.16 + 0.7 * drift));
}
// cada casa um tom (13/10, o Felipe pela comparação com a maquete; a receita do Look, look_lab.py PLASTER_CODE): ~38%
// caiadas, ~30% ocre, o resto o reboco de hoje, e a faixa de sujeira até ~1,5 m. Na maquete o sorteio é pelo lugar da
// casa; aqui as casas são instâncias de uma malha só, então é o aleatório de cada instância (só o reboco liga: ToneVariation)
float toneFoot = 0.0;
// (13/10, a v2: o Felipe pelo Look, "a variação de cor das casas está forte demais": a metade do contraste; e o mesmo tom
// pra casa e muros do mesmo terreno, pelo dado da instância que o jogo grava do lote do village.json; sem ele, o
// aleatório da instância de antes)
if (ToneVariation > 0.5)
{
    float hsh = ILot > 0.0005 ? frac(ILot) : IRand;
    base *= lerp(float3(0.965, 0.975, 0.99), float3(1.035, 1.015, 0.985), hsh);
    float hsh2 = frac(hsh * 7.31 + 0.17);
    // (o reboco do jogo é mais claro que o da maquete, 0,49/0,35/0,20 contra 0,40/0,31/0,22: o mesmo creme a 55% quase não
    // lia na câmera; a 70% num creme mais claro dá o mesmo contraste relativo da maquete)
    base = lerp(base, float3(0.56, 0.50, 0.40), 0.35 * step(0.62, hsh2));          // caiada (creme)
    base *= lerp(float3(1.0, 1.0, 1.0), float3(1.05, 0.96, 0.85), step(hsh2, 0.3));   // ocre
    float sn = saturate((stain - 0.215) / 0.045);
    toneFoot = 1.0 - smoothstep(0.8 + 0.5 * sn, 1.6 + 0.6 * sn, P.z);
    base = lerp(base, base * float3(0.55, 0.48, 0.40), 0.9 * toneFoot);
}
// terra no pé (sobe até ~1 m, mais onde a mancha é forte) e o pó do deserto no que olha pra cima (no reboco com tom, a
// faixa de cima já é a sujeira do pé)
float foot = (1.0 - smoothstep(0.0, 0.7 + 1.4 * stain, P.z)) * (ToneVariation > 0.5 ? 0.0 : 1.0);
base = lerp(base, base * float3(0.74, 0.62, 0.48), 0.6 * foot);
float up = smoothstep(0.6, 0.95, n.z) * (1.0 - ConcreteMask);
base = lerp(base, float3(0.40, 0.31, 0.21), 0.16 * up * saturate(stain * 2.0));
Rough = lerp(lerp(Roughness, 0.95, ConcreteMask), 0.9, 0.5 * foot + 0.3 * up);
Metal = Metallic * (1.0 - 0.6 * foot);
// (31/10, o detalhe AAA: docs/ESTUDO_DETALHE_AAA.md §2.2) as máscaras cozidas no Blender (model_detail.py) na cor de vértice:
// R = 1 − quina convexa, G = a oclusão de contato, B = 1 − fresta. Branco = neutro (a malha sem máscara não muda); o preto exato
// (alguma malha sem o buffer de cor) também vale neutro. Só no material com a chave RTS_MASKS (o M_UnitAtlas não tem as entradas)
float aoM = 1.0;
float creaseM = 0.0;   // (01/11) a fresta (as quinas de dentro dos vãos e as juntas): a rachadura do prédio nasce ali
#if RTS_MASKS
{
    float3 vc = VC.rgb;
    if (vc.r + vc.g + vc.b < 0.001) vc = float3(1.0, 1.0, 1.0);
    float edgeM = 1.0 - vc.r;
    float cavM = 1.0 - vc.b;
    creaseM = cavM;
    aoM = vc.g;
    float3 Lq = LP * 0.01;
    // cada chapa um tom (a semente da ilha no UV1.x): chapa de verdade, não plástico. Só no modelo com máscara, marcado pelo UV1.y fora de
    // [0, 1] (model_detail.uv_meters); no antigo o UV1 pode ser o de lightmap que a Unreal gera, e o tom viraria mancha (o aprovado é o piso)
    if (UV1.y > 1.5 || UV1.y < -0.5) base *= 1.0 + PanelTone * (frac(UV1.x * 91.7) - 0.5) * 0.14;
    // o pó e a areia nas frestas (a cor da duna do mapa, a mesma do pé), mais onde a mancha é forte
    base = lerp(base, float3(0.40, 0.31, 0.21), saturate(cavM * CavityDirt * (0.6 + 0.6 * stain)));
    // o desgaste de quina: a tinta gasta clareia e o aço aparece, recortado por ruído de ~6 cm no espaço da peça (lasca, não contorno)
    float wn = Nz.N(Lq / 0.06 + 3.1);
    float wear = smoothstep(0.35, 0.65, edgeM * (0.45 + wn)) * Wear;
    base = lerp(base, min(base * 1.65 + 0.035, 0.6), wear * 0.7);
    Rough = lerp(Rough, 0.42, wear * 0.6 * (1.0 - ConcreteMask));
    // a oclusão na cor só se pedir (o normal é o pino de AO: só a luz do céu e a indireta; o sol nas frestas já é sombra)
    base *= lerp(1.0, aoM, AOColor);
    // (01/11, item 40, o Felipe: "as texturas da USA estão ruins ainda, olhe os game shots") a tinta CLARA em campanha no deserto. A areia
    // na fresta e a quina clareada de cima não aparecem em tinta clara (mesmo valor): o alvo (r1_raid_slabs) tem a junta ESCURA e a
    // areia subindo do baixo. A ordem do modelismo de blindado no deserto: a poeira primeiro, grossa no baixo (rodagem, esteira, saia),
    // depois a lavagem escura só na fresta funda (não contornar tudo de preto) e pouca lasca (docs/elenco/TEXTURA_USA.md). Só no modelo
    // do detalhe (o UV1 marcado); a altura é a do espaço da peça (as peças do elenco vêm nas coordenadas do veículo, chão em z = 0)
    if ((DustCoat > 0.0 || Wash > 0.0) && (UV1.y > 1.5 || UV1.y < -0.5))
    {
        // (a 2ª volta, 01/11 14h: na 1ª o baixo ficou certo, mas o de cima seguia limpo, R/B da tinta 1,08–1,12 contra 1,16–1,21 no
        // alvo, e a junta mal aparecia na câmera do jogo) a lavagem pega a fresta mais rasa; o escorrido fino na parede (a folha de
        // exploração da USA: o pó que a chuva e o orvalho puxam pra baixo, faixas de ~15 cm × ~0,8 m); o véu 12% → 22%, mais pó em cima
        base *= 1.0 - Wash * 0.72 * smoothstep(0.12, 0.5, cavM);
        float3 lnv = abs(normalize(cross(ddx(Lq), ddy(Lq)) + 1e-7));
        float alongV = lnv.x > lnv.y ? Lq.y : Lq.x;
        float stk = smoothstep(0.55, 0.8, Nz.N(float3(alongV * 5.0, alongV * 5.0, Lq.z * 1.2) + 13.1)) * (1.0 - smoothstep(0.35, 0.6, lnv.z));
        base = lerp(base, base * float3(0.80, 0.74, 0.66), stk * Wash * 0.6);
        float chipN = Nz.N(Lq / 0.05 + 9.7);
        base = lerp(base, float3(0.075, 0.066, 0.055), smoothstep(0.62, 0.72, edgeM * (0.2 + chipN)) * Chip * 0.85);
        float dn = Nz.N(Lq * float3(1.4, 1.4, 0.8) + 5.3);
        float gr = Nz.N(Lq / 0.11 + 2.7);
        float low = 1.0 - smoothstep(0.30, 0.95 + 0.8 * dn, Lq.z);
        float upF = smoothstep(0.55, 0.95, n.z);
        float dust = saturate(0.85 * low + 0.6 * upF * smoothstep(0.25, 0.6, dn) + 0.22) * DustCoat;
        base = lerp(base, float3(0.40, 0.31, 0.21) * lerp(0.82, 1.08, gr), dust * lerp(0.75, 1.0, gr));
        Rough = lerp(Rough, 0.93, dust);
        Metal *= 1.0 - dust;
    }
}
#endif
// luzes: piscam (o giroflex), apagam sem energia ou em obra
float pulse = Blink > 0.0 ? (frac(Time * Blink) < 0.5 ? 1.0 : 0.06) : 1.0;
// (01/11) o pulso que corre no cabo (Pulse = voltas por segundo): duas faixas acesas de ~0,6 m andando pelo comprimento (o x e o z da peça:
// os cabos da usina vão do pórtico até a torre), no resto só um fio aceso
if (Pulse > 0.0)
{
    float pls = (LP.x * 0.01 - LP.z * 0.006) * 0.35 - Time * Pulse;
    pulse *= 0.12 + 0.88 * smoothstep(0.82, 0.97, frac(pls)) + 0.6 * smoothstep(0.9, 0.99, frac(pls * 2.0 + 0.37));
}
float3 emis = EmissiveColor.rgb * EmissiveNits * pulse * lerp(0.03, 1.0, saturate(Power));
// a obra: corta acima da altura construída; a linha de solda acesa no corte; o lado de dentro escuro
float d = Cut - P.z;
Mask = d >= 0.0 ? 1.0 : 0.0;
float weld = saturate(1.0 - d / 0.35) * (d >= 0.0 ? 1.0 : 0.0) * Building * Weld;
emis += float3(1.0, 0.52, 0.16) * 55000.0 * weld * weld;
base *= lerp(1.0, Face > 0.0 ? 1.0 : 0.45, Building);
// (30/10, item 31) o prédio furado (o M_StructureHoled, mascarado e de dois lados, o mesmo visual do pronto): o lado de dentro das paredes e
// do telhado, que o furo mostra, escuro. Pela normal contra a câmera (o TwoSidedSign falha no Nanite mascarado: o prédio inteiro escurecia)
if (Holed > 0.5 && dot(normalize(N), normalize(Cam - WP)) < 0.0) base *= 0.22;
// o dano no veículo vivo (13/10; os estados do ZH: danificado abaixo de 70% da vida, muito danificado abaixo de
// 35%; o dado de primitiva 9 = 1 − vida): as camadas da carcaça em placas de ~0,9 m que crescem com o dano, a
// tinta queimada e lascada primeiro, a fuligem de impacto depois
// (31/10, docs/ESTUDO_DEGRADACAO_PREDIOS.md §2c) o prédio (o dado 11 que o ator marca) não leva as pintas: elas liam como camuflagem
float isStruct = 0.0;
#if RTS_MASKS
isStruct = IsStruct;
#endif
if (Damage > 0.2 && Burnt < 0.5 && isStruct < 0.5)
{
    // (o ruído era a foto da praia, que no verde linear só vai de 0,22 a 0,25: nenhuma placa passava do limiar; com
    // o ruído novo em placas de 0,9 m o tanque vivo virou camuflagem, 13/10: lascas de ~0,35 m, de ~5% da
    // superfície aos 70% da vida a ~25% perto da morte; a fuligem de impacto abaixo de 50%, de ~5% a ~12%)
    float nD = Nz.N(q / 0.35 + 11.7);
    float k = saturate((Damage - 0.3) / 0.7);
    float chip = smoothstep(0.95 - 0.23 * k, 0.99 - 0.23 * k, nD);
    float k2 = saturate((Damage - 0.5) / 0.5);
    float sootD = smoothstep(0.95 - 0.13 * k2, 0.99 - 0.13 * k2, Nz.N(q / 0.5 + 3.3)) * step(0.5, Damage);
    base = lerp(base, float3(0.107, 0.069, 0.045), chip * 0.85);
    base = lerp(base, float3(0.016, 0.012, 0.010), sootD);
    Rough = lerp(Rough, 0.92, chip);
    Metal *= 1.0 - chip;
}
#if RTS_MASKS
// (31/10, docs/ESTUDO_DEGRADACAO_PREDIOS.md §2c) o prédio ferido: a rachadura que cresce com o dano (a borda de Voronoi F2 − F1 no espaço
// da peça, ~6 cm = 2 px na câmera, em células de ~1,3 m e de ~0,5 m), mais densa em volta dos tiros e dos furos (as marcas 12–35), e a
// fuligem em V acima de cada furo (por onde o fogo respira: alarga subindo até ~3 m, a pluma da perícia de incêndio)
if (isStruct > 0.5 && Damage > 0.12 && Burnt < 0.5)
{
    float3 Mq = MP * 0.01;
    float nearHit = 0.0;
    float vsoot = 0.0;
    float4 HMs[6] = { HM0, HM1, HM2, HM3, HM4, HM5 };
    [unroll] for (int i = 0; i < 6; ++i)
    {
        float4 h = HMs[i];
        if (h.w < 150000.0) continue;
        float d = distance(MP, h.xyz) * 0.01;
        nearHit = max(nearHit, 1.0 - smoothstep(0.8, 3.2, d));
        if (h.w >= 300000.0)
        {
            float r = (h.w - 300000.0) * 0.01;
            float dz = (MP.z - h.z) * 0.01;
            float lat = length(MP.xy - h.xy) * 0.01;
            float vv = step(0.0, dz) * (1.0 - smoothstep(0.0, 3.2, dz)) * (1.0 - smoothstep(r * 0.6 + dz * 0.45, r * 0.9 + dz * 0.6, lat));
            vsoot = max(vsoot, vv * (0.75 + 0.25 * Nz.N(Mq / 0.4 + 2.0)));
        }
    }
    // o Voronoi (F2 − F1) de 27 células, em duas escalas
    float crack = 0.0;
    [unroll] for (int s = 0; s < 2; ++s)
    {
        float cs = s == 0 ? 1.3 : 0.5;
        float3 x = Mq / cs;
        float3 ix = floor(x);
        float f1 = 9.0, f2 = 9.0;
        [unroll] for (int a = -1; a <= 1; ++a)
        [unroll] for (int b = -1; b <= 1; ++b)
        [unroll] for (int c = -1; c <= 1; ++c)
        {
            float3 g = ix + float3(a, b, c);
            float3 o = float3(Nz.H(g), Nz.H(g + 17.3), Nz.H(g + 41.9));
            float dd = length(g + o - x);
            if (dd < f1) { f2 = f1; f1 = dd; } else if (dd < f2) { f2 = dd; }
        }
        float lin = 1.0 - smoothstep(0.0, 0.06 / cs, f2 - f1);
        // onde a rachadura existe: cresce com o dano (o ruído largo abaixo do limiar) e perto dos tiros
        // (01/11) e nos cantos dos vãos (a fresta das máscaras: a tensão do concreto concentra na quina da janela e da porta)
        float pull = 0.45 * nearHit + 0.4 * saturate(creaseM * 1.5) * saturate(Damage * 2.0);
        // (01/11, a prova de perto: aos 60% de vida a rede cobria a fachada e o piso inteiros e lia como mosaico de pedra) a trinca nasce
        // perto dos tiros e nas quinas dos vãos e só se espalha pela parede com o prédio quase caindo: o limiar cai com o dano ao quadrado
        // (60% de vida: ~10% da parede fora das quinas; 30%: ~45%; 10%: ~70%)
        float spread = Damage * Damage;
        float grow = smoothstep(1.32 - 0.75 * spread - pull, 1.45 - 0.75 * spread - pull, Nz.N(Mq / 2.6 + 5.0 + s * 9.0) + 0.4);
        // a escala fina (0,5 m) só onde a tensão concentra (o tiro, a quina): em toda a parede ela virava o mosaico
        crack = max(crack, lin * grow * (s == 0 ? 1.0 : 0.7 * saturate(pull * 2.0)));
    }
    base = lerp(base, base * 0.25 + 0.012, crack * 0.85);
    Rough = lerp(Rough, 0.95, crack);
    base = lerp(base, float3(0.02, 0.017, 0.015), vsoot * 0.85);
    Rough = lerp(Rough, 0.97, vsoot);
}
// (01/11, o vidro quebrado do prédio ferido: docs/elenco/TAREFAS.md item 18) cada vidraça (~0,9 m) sorteia pelo dano: estourada (o buraco
// escuro, sem o reflexo), rachada (a estrela de linhas claras a partir de um ponto, com o anel do impacto) ou inteira; só no slot de vidro
if (isStruct > 0.5 && GlassMask > 0.5 && Damage > 0.25 && Burnt < 0.5)
{
    float3 Gq = MP * 0.01;
    float kg = saturate((Damage - 0.25) / 0.6);
    float3 cell = floor(Gq / 0.9);
    float rg = Nz.H(cell + 3.7);
    if (rg < 0.35 * kg)
    {
        base = float3(0.004, 0.004, 0.005);
        Rough = 0.95;
        Metal = 0.0;
    }
    else if (rg < 0.8 * kg)
    {
        float3 c = (cell + 0.2 + 0.6 * float3(Nz.H(cell + 1.1), Nz.H(cell + 2.3), Nz.H(cell + 5.9))) * 0.9;
        float3 dg = Gq - c;
        float ang = atan2(dg.z, dg.x + dg.y + 0.0001);
        float rad = length(dg);
        float rays = pow(abs(cos(ang * 4.0 + rg * 20.0)), 40.0) * (1.0 - smoothstep(0.0, 0.5, rad));
        float ring = (1.0 - smoothstep(0.0, 0.02, abs(rad - 0.12))) * 0.6;
        float cr = saturate(rays + ring);
        base = lerp(base, float3(0.35, 0.36, 0.37), cr * 0.7);
        Rough = lerp(Rough, 0.6, saturate(cr + 0.3));
    }
}
#endif
// a carcaça (13/10; dado de primitiva 6: 1 = veículo, 2 = prédio): a receita do Look medida na carcaça da
// street_battle, na volta 4 dele (look_lab.py, BURNT_CODE; o alvo dá p10/p50/p90 ≈ 0,03/0,33/0,60 na tela).
// Não é preta: marrom-carvão com variação forte. Tinta queimada 78,70,64 sRGB; fuligem 34,30,27 embaixo
// (até 0,6 m) e em manchas em ~25% do corpo, e no jogo também em volta do anel da torre, por onde a chama
// saiu; ferrugem de calor 104,70,48 em ~15%, mais nas quinas; cinza 165,155,142 em ~20% do teto.
// (13/10, no jogo: com limiar duro num ruído liso as placas viravam camuflagem urbana, branco, ferrugem e preto em
// recortes; aqui as mesmas cores em rampas largas, mosqueadas por uma oitava fina de 0,18 m, e a cinza só um pó
// raro no teto. Preview fora da placa em scratchpad/burnt, na mediana 0,33 do alvo.)
// As manchas vêm do ruído procedural (FRTSNoise, lá em cima), na escala da receita: 1,5 m e 0,8 m.
if (Burnt > 0.5)
{
    float3 Lp = LP * 0.01;
    float3 B0 = LMin * 0.01;
    float3 B1 = LMax * 0.01;
    float zr = Lp.z - B0.z;
    float ring = length(Lp.xy - float2(0.10, 0.0));
    float nA = Nz.N(q / 1.1);
    float nB = Nz.N(q / 0.6 + 5.3);
    float nF = Nz.N(q / 0.18 + 41.0);
    float edge = min(min(B1.x - Lp.x, Lp.x - B0.x), min(B1.y - Lp.y, Lp.y - B0.y));
    bool bVehicle = Burnt < 1.5;
    // a fuligem pela forma: no veículo embaixo até 0,6 m e na boca do anel; no prédio sobe até ~2,5 m
    float sootG = bVehicle ? max(1.0 - smoothstep(0.2, 0.6, zr), (1.0 - smoothstep(0.6, 1.3, ring)) * smoothstep(0.9, 1.4, zr))
                           : 1.0 - smoothstep(0.5, 2.5, zr);
    float soot = saturate(max(sootG * 0.85, (1.0 - smoothstep(0.0, 0.55, nA)) * 0.75));
    float rust = smoothstep(0.45, 1.0, nA + 0.08 * (1.0 - smoothstep(0.0, 0.6, edge))) * 0.6;
    float ash = smoothstep(0.6, 0.9, n.z) * smoothstep(0.8, 1.0, nB) * (1.0 - soot) * 0.25;
    base = float3(0.076, 0.061, 0.051);
    base = lerp(base, float3(0.140, 0.062, 0.030), rust);
    base = lerp(base, float3(0.016, 0.013, 0.011), soot);
    base = lerp(base, float3(0.30, 0.27, 0.23), ash);
    base *= 0.75 + 0.5 * nF;
    Rough = 0.93;
    Metal = 0.0;
    emis = 0.0;
    // o miolo quente: só no buraco do anel da torre (a torre voou), nas faces de cima e de dentro, em fresta
    // (ruído de ~0,5 m com limiar), no corpo negro de 1300 K esfriando a 900 K. Pico ~3000 nits (com o ev100
    // 14,3 do jogo lê laranja, não branco); I(t) = pico × calor² (o calor, o dado 7, cai de 1 a 0 em 30 s),
    // com a tremida de ruído; depois, 2–3 pontinhos a 5% (o dado 8, mais ~20 s)
    if (bVehicle)
    {
        float core = (1.0 - smoothstep(0.25, 1.1, ring)) * smoothstep(1.1, 1.5, zr) * saturate(n.z + 0.4);
        // (a fresta no ruído procedural de 0,5 m: o asfalto a 0,25 m virava a média na distância do jogo e o
        // miolo nunca acendia)
        float nC = Nz.N(q / 0.5 + 23.1);
        float crack = smoothstep(0.55, 0.75, nC);
        float flick = lerp(0.7, 1.0, Nz.N(float3(Time * 1.2, Obj.xy * 0.0007)));
        float3 bb = lerp(float3(1.0, 0.15, 0.02), float3(1.0, 0.35, 0.08), saturate(Heat));
        emis += bb * 3000.0 * Heat * Heat * core * crack * flick;
        emis += float3(1.0, 0.18, 0.03) * 150.0 * Embers * core * smoothstep(0.86, 0.9, nC);
    }
}
// (30/10, a marca do tiro na fachada: o M_Structure não tinha a do casco, o MF_HullMarks das unidades) o BlastFx grava cada acerto de
// canhão no prédio nos dados de primitiva 12–35 da peça (UBlastFxDirector::MarkHull, tipo 2): x, y, z no espaço da malha (cm) e a
// hora + 100 000 × o tipo. O tiro de 105–120 mm em concreto: a cratera de ~0,3–0,6 m, a fuligem e o respingo até ~1,5 m; a 67 m
// 1 px ≈ 3,3 cm, a marca tem ~40 px. A fuligem quase preta em volta (Ø ~1,4 m, a borda recortada pelo ruído) e a lasca no meio
// (Ø ~0,45 m: o concreto novo exposto, mais claro que a parede velha, com os furos escuros). Os tipos 0 e 1 (as unidades) não
// passam aqui
{
    float4 HM[6] = { HM0, HM1, HM2, HM3, HM4, HM5 };
    [unroll] for (int i = 0; i < 6; ++i)
    {
        float4 h = HM[i];
        // (30/10, item 31) o furo do prédio ferido (o tipo 3, ARTSUnitActor::OpenStructureHole: w = 300 000 + o raio em cm): o que caiu
        // sai (a máscara; o material da obra, mascarado e de dois lados, mostra o escuro de dentro) e a borda fica rasgada pelo ruído
        // (o concreto não quebra em círculo), com a ferragem e o queimado em volta
        if (h.w >= 300000.0)
        {
            float r = h.w - 300000.0;
            float dh = distance(MP, h.xyz);
            float nh = Nz.N(MP / 38.0 + h.xyz * 0.011);
            float nf = Nz.N(MP / 11.0 + h.xyz * 0.017);
            float re = dh / max(r, 1.0) * (0.72 + 0.45 * nh + 0.12 * nf);
            if (re < 1.0) Mask = 0.0;
            float rim = 1.0 - smoothstep(1.0, 1.55, re);
            float rebar = rim * smoothstep(0.82, 0.9, frac(dot(MP.xy, float2(0.07, 0.05)) + nf));
            base = lerp(base, float3(0.045, 0.04, 0.036), 0.8 * rim);
            base = lerp(base, float3(0.09, 0.05, 0.03), rebar);
            Rough = lerp(Rough, 0.95, rim);
            Metal *= 1.0 - rim;
            continue;
        }
        // (01/11, ele: "a fuligem preta de queimado [...] nas unidades e construções que levaram um impacto explosivo de mísseis, por
        // alguns segundos") o míssil (o tipo 1 do BlastFx: a ogiva e o vizinho da explosão) na fachada: a bola de fogo deixa a fuligem
        // quase preta e fosca em ~4 m, manchada, cheia até 1,5 s e clareando até 6 s; fica a marca de ~1,6 m em volta do ponto
        if (h.w >= 100000.0 && h.w < 200000.0)
        {
            float mkT = Time - (h.w - 100000.0);
            if (mkT < 0.0) continue;
            float mkD = distance(MP, h.xyz);
            if (mkD > 260.0) continue;
            float mkN = Nz.N(MP / 40.0 + h.xyz * 0.011);
            float mkA = 1.0 - smoothstep(1.5, 6.0, mkT);
            float mkBig = (1.0 - smoothstep(0.3, 1.0, mkD / 200.0 * (0.8 + 0.4 * mkN))) * mkA;
            float mkKeep = (1.0 - smoothstep(0.35, 0.95, mkD / 80.0 * (1.0 + 0.5 * (mkN - 0.5)))) * 0.85;
            float mkK = max(mkBig, mkKeep);
            base = lerp(base, float3(0.03, 0.028, 0.026), mkK);
            Rough = lerp(Rough, 0.95, mkK);
            Metal *= 1.0 - mkK;
            continue;
        }
        if (h.w < 150000.0) continue;
        float d = distance(MP, h.xyz);
        if (d > 80.0) continue;
        float nn = Nz.N(MP / 22.0 + h.xyz * 0.013);
        float re = d / 70.0 * (1.0 + 0.5 * (nn - 0.5));
        float soot = (1.0 - smoothstep(0.35, 0.95, re)) * 0.9;
        float core = 1.0 - smoothstep(0.26, 0.34, d / 70.0 * (1.0 + 0.6 * (nn - 0.5)));
        float pit = smoothstep(0.62, 0.72, frac(nn * 7.3));
        base = lerp(base, float3(0.035, 0.032, 0.03), soot * (1.0 - core));
        base = lerp(base, lerp(float3(0.50, 0.48, 0.44), float3(0.07, 0.065, 0.06), pit), core);
        Rough = lerp(Rough, 0.95, max(soot, core));
        Metal *= 1.0 - max(soot, core);
    }
}
Emis = emis;
float2 nX = ConcN.Sample(ConcNSampler, q.yz / T).rg * 2.0 - 1.0;
float2 nY = ConcN.Sample(ConcNSampler, q.xz / T + 0.31).rg * 2.0 - 1.0;
float2 nZ = ConcN.Sample(ConcNSampler, q.xy / T + 0.57).rg * 2.0 - 1.0;
Nrm = normalize(n + ConcreteMask * 0.6 * (w.x * float3(0.0, nX.x, nX.y) + w.y * float3(nY.x, 0.0, nY.y) + w.z * float3(nZ.x, nZ.y, 0.0)));
#if RTS_MASKS
AOut = lerp(1.0, aoM, 0.9);
#endif
return min(base, 0.9);
"""

SCALARS = [('Roughness', 0.6), ('Metallic', 0.0), ('PlayerMask', 0.0), ('ConcreteMask', 0.0), ('Stripes', 0.0),
           ('Grime', 0.0), ('EmissiveNits', 0.0), ('Blink', 0.0), ('Weld', 1.0), ('ToneVariation', 0.0), ('Weather', 0.0),
           # (31/10, o detalhe AAA) o desgaste de quina, o pó nas frestas, o tom por chapa, a oclusão na cor e a camuflagem no material
           ('Wear', 0.0), ('CavityDirt', 0.0), ('PanelTone', 0.0), ('AOColor', 0.0), ('Camo', 0.0), ('CamoPix', 0.16),
           ('GlassMask', 0.0),   # (01/11) 1 = o slot de vidro (a vidraça quebra no prédio ferido)
           ('CamoScale', 1.1),   # (01/11) o tamanho da mancha (m): 1,1 o digital da China; ~1,8 as manchas grandes da Rússia
           ('DustCoat', 0.0), ('Wash', 0.0), ('Chip', 0.0),
           ('Pulse', 0.0)]   # (01/11) o pulso que corre no cabo de alta tensão (voltas/s)   # (01/11, item 40) a poeira de baixo, a lavagem escura e a lasca escura
VECTORS = [('BaseColor', (0.5, 0.5, 0.5)), ('EmissiveColor', (0.0, 0.0, 0.0)), ('CamoColor', (0.04, 0.048, 0.028)),
           ('CamoColor2', (0.3, 0.3, 0.3))]


def load(path):
    return unreal.load_asset(path) if eal.does_asset_exist(path) else None


def fresh(name, folder, cls, factory):
    path = f'{folder}/{name}'
    if eal.does_asset_exist(path):
        a = unreal.load_asset(path)
        if isinstance(a, unreal.Material):
            mel.delete_all_material_expressions(a)
        return a
    return tools.create_asset(name, folder, cls, factory)


TRACK_WPO = r"""
// (01/11, a esteira que anda) o dado 10 = I + fase: a fase (0..1) do passo do elo e I/1024 o quanto ela andou neste quadro (o jogo,
// ARTSUnitActor::StepTracks). Cada elo desliza pela tangente dele × passo (UV2.x, UV3.x, em metros, no quadro do veículo) até o lugar do
// próximo; na volta da fase ele cai exatamente onde o anterior estava (sem salto). O quadro anterior (a velocidade do TSR) com a fase de
// antes, sem o salto: o elo nunca 'pula' pro TSR. Ao contrário da tangente (a ordem do caminho): pra frente, o trecho de baixo vai pra trás
float I = floor(Ph);
float ph = Ph - I;
float3 t = -float3(UV2.x, 0.0, UV3.x) * 100.0;
Prev = t * (ph - I / 1024.0);
return t * ph;
"""


def track_wpo(m, y):
    """Os nós do deslocamento dos elos no mestre da esteira: o código, o local → mundo e o quadro anterior."""
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -600, y)
    c.set_editor_property('code', TRACK_WPO)
    c.set_editor_property('output_type', F3)
    c.set_editor_property('description', 'TrackWPO')
    ins = []
    for n in ('UV2', 'UV3', 'Ph'):
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    c.set_editor_property('inputs', ins)
    co = unreal.CustomOutput()
    co.set_editor_property('output_name', 'Prev')
    co.set_editor_property('output_type', F3)
    c.set_editor_property('additional_outputs', [co])
    for k, (pin, idx) in enumerate((('UV2', 2), ('UV3', 3))):
        e = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, y + 100 * k)
        e.set_editor_property('coordinate_index', idx)
        assert mel.connect_material_expressions(e, '', c, pin), pin
    ph = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, y + 200)
    ph.set_editor_property('parameter_name', 'TrackPhase')
    ph.set_editor_property('use_custom_primitive_data', True)
    ph.set_editor_property('primitive_data_index', 10)
    ph.set_editor_property('default_value', 0.0)
    assert mel.connect_material_expressions(ph, '', c, 'Ph'), 'Ph'
    outs = []
    for k, pin in enumerate(('', 'Prev')):
        tr = mel.create_material_expression(m, unreal.MaterialExpressionTransform, -350, y + 120 * k)
        tr.set_editor_property('transform_source_type', unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL)
        tr.set_editor_property('transform_type', unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
        assert mel.connect_material_expressions(c, pin, tr, ''), 'tr ' + pin
        outs.append(tr)
    sw = mel.create_material_expression(m, unreal.MaterialExpressionPreviousFrameSwitch, -150, y)
    ok = False
    for a, b in (('CurrentFrame', 'PreviousFrame'), ('Current Frame', 'Previous Frame')):
        if mel.connect_material_expressions(outs[0], '', sw, a):
            ok = mel.connect_material_expressions(outs[1], '', sw, b)
            break
    assert ok, 'PreviousFrameSwitch'
    assert mel.connect_material_property(sw, '', unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET), 'WPO'
    try:
        m.set_editor_property('max_world_position_offset_displacement', 30.0)   # o passo do elo (~17 cm) com folga: a caixa da Nanite
    except Exception as e:
        log('sem max_world_position_offset_displacement:', e)


def master(name, build, holed=False, track=False):
    m = fresh(name, MATS, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('tangent_space_normal', False)
    m.set_editor_property('used_with_nanite', True)
    m.set_editor_property('used_with_instanced_static_meshes', True)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED if build or holed else unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property('two_sided', build or holed)
    custom = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property('code', CODE)
    custom.set_editor_property('output_type', F3)
    custom.set_editor_property('description', 'Structure')
    inputs = ['WP', 'N', 'Obj', 'LP', 'LMin', 'LMax', 'Conc', 'ConcN', 'Stain', 'Time', 'Face', 'Cam', 'Holed', 'PlayerColor', 'Power', 'Cut', 'Building',
              'Burnt', 'Heat', 'Embers', 'Damage', 'IRand', 'ILot', 'MP', 'HM0', 'HM1', 'HM2', 'HM3', 'HM4', 'HM5', 'VC', 'UV1', 'IsStruct'] + \
             [n for n, _ in SCALARS] + [n for n, _ in VECTORS]
    # (31/10) as máscaras da cor de vértice: a chave do código (o M_UnitAtlas usa o mesmo código sem elas)
    d = unreal.CustomDefine()
    d.set_editor_property('define_name', 'RTS_MASKS')
    d.set_editor_property('define_value', '1')
    custom.set_editor_property('additional_defines', [d])
    ins = []
    for n in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    custom.set_editor_property('inputs', ins)
    outs = []
    for n, t in (('Rough', F1), ('Metal', F1), ('Emis', F3), ('Mask', F1), ('Nrm', F3), ('AOut', F1)):
        co = unreal.CustomOutput()
        co.set_editor_property('output_name', n)
        co.set_editor_property('output_type', t)
        outs.append(co)
    custom.set_editor_property('additional_outputs', outs)

    y = -1200
    def node(cls, pin):
        nonlocal y
        y += 110
        e = mel.create_material_expression(m, cls, -800, y)
        if pin:
            assert mel.connect_material_expressions(e, '', custom, pin), pin
        return e
    wp = node(unreal.MaterialExpressionWorldPosition, 'WP')
    node(unreal.MaterialExpressionVertexNormalWS, 'N')
    node(unreal.MaterialExpressionObjectPositionWS, 'Obj')
    node(unreal.MaterialExpressionPerInstanceRandom, 'IRand')   # (o tom de cada casa da vila, que são instâncias)
    # o tom do lote (13/10): o dado 0 da instância, que o jogo grava pelo lote do village.json (casa e muros iguais); 0 = sem
    lot = node(unreal.MaterialExpressionPerInstanceCustomData, 'ILot')
    lot.set_editor_property('data_index', 0)
    # a carcaça: a posição no espaço da peça e a caixa dela (a fuligem embaixo, a ferrugem perto das quinas)
    lp = node(unreal.MaterialExpressionTransformPosition, 'LP')
    lp.set_editor_property('transform_source_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    lp.set_editor_property('transform_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    assert mel.connect_material_expressions(wp, '', lp, ''), 'LP in'
    lb = node(unreal.MaterialExpressionObjectLocalBounds, None)
    assert mel.connect_material_expressions(lb, 'Min', custom, 'LMin'), 'LMin'
    assert mel.connect_material_expressions(lb, 'Max', custom, 'LMax'), 'LMax' 
    node(unreal.MaterialExpressionTime, 'Time')
    node(unreal.MaterialExpressionTwoSidedSign, 'Face')
    node(unreal.MaterialExpressionCameraPositionWS, 'Cam')
    hc = node(unreal.MaterialExpressionConstant, 'Holed')
    hc.set_editor_property('r', 1.0 if holed else 0.0)
    for pin, path, fallback in (('Conc', CONCRETE, NOISE), ('ConcN', CONCRETE_N, FLAT_N), ('Stain', STAIN, NOISE)):
        tex = load(path) or load(fallback)
        e = node(unreal.MaterialExpressionTextureObject, None)
        e.set_editor_property('texture', tex)
        if pin == 'ConcN':
            e.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        assert mel.connect_material_expressions(e, '', custom, pin), pin
        log(name, pin, tex.get_path_name() if tex else None)
    pc = node(unreal.MaterialExpressionVectorParameter, 'PlayerColor')
    pc.set_editor_property('parameter_name', 'PlayerColor')
    pc.set_editor_property('use_custom_primitive_data', True)
    pc.set_editor_property('primitive_data_index', 0)
    pc.set_editor_property('default_value', unreal.LinearColor(0.10, 0.45, 1.0, 1.0))
    for pin, index, default in (('Power', 4, 1.0), ('Cut', 5, 1000.0), ('Burnt', 6, 0.0), ('Heat', 7, 0.0), ('Embers', 8, 0.0), ('Damage', 9, 0.0),
                                ('IsStruct', 11, 0.0)):   # (31/10) o 11: o prédio (o ator marca; a rachadura e a fuligem em V)
        s = node(unreal.MaterialExpressionScalarParameter, pin)
        s.set_editor_property('parameter_name', pin)
        s.set_editor_property('use_custom_primitive_data', True)
        s.set_editor_property('primitive_data_index', index)
        s.set_editor_property('default_value', default)
    # (30/10) as marcas de tiro na fachada: a posição na malha (cm, a mesma do BlastFx) e as 6 vagas dos dados 12–35
    node(unreal.MaterialExpressionLocalPosition, 'MP')
    for k in range(6):
        hm = node(unreal.MaterialExpressionVectorParameter, None)
        hm.set_editor_property('parameter_name', f'HitMark{k}')
        hm.set_editor_property('use_custom_primitive_data', True)
        hm.set_editor_property('primitive_data_index', 12 + 4 * k)
        hm.set_editor_property('default_value', unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
        # (os 4 números: a saída RGBA; a de sempre é só o RGB, e o nó pedia float4)
        assert mel.connect_material_expressions(hm, 'RGBA', custom, f'HM{k}'), f'HM{k}'
    # (31/10) as máscaras do model_detail.py (a cor de vértice) e a semente da chapa (o UV1)
    vc = node(unreal.MaterialExpressionVertexColor, None)
    assert mel.connect_material_expressions(vc, '', custom, 'VC'), 'VC'
    uv1 = node(unreal.MaterialExpressionTextureCoordinate, 'UV1')
    uv1.set_editor_property('coordinate_index', 1)
    b = node(unreal.MaterialExpressionConstant, 'Building')
    b.set_editor_property('r', 1.0 if build else 0.0)
    for n, default in SCALARS:
        s = node(unreal.MaterialExpressionScalarParameter, n)
        s.set_editor_property('parameter_name', n)
        s.set_editor_property('default_value', default)
    for n, default in VECTORS:
        v = node(unreal.MaterialExpressionVectorParameter, n)
        v.set_editor_property('parameter_name', n)
        v.set_editor_property('default_value', unreal.LinearColor(*default, 1.0))
    P = unreal.MaterialProperty
    ok = [mel.connect_material_property(custom, '', P.MP_BASE_COLOR),
          mel.connect_material_property(custom, 'Rough', P.MP_ROUGHNESS),
          mel.connect_material_property(custom, 'Metal', P.MP_METALLIC),
          mel.connect_material_property(custom, 'Emis', P.MP_EMISSIVE_COLOR),
          mel.connect_material_property(custom, 'Nrm', P.MP_NORMAL),
          mel.connect_material_property(custom, 'AOut', P.MP_AMBIENT_OCCLUSION)]
    if build or holed:
        ok.append(mel.connect_material_property(custom, 'Mask', P.MP_OPACITY_MASK))
    if not all(ok):
        raise RuntimeError(f'{name}: ligações falharam {ok}')
    if track:
        track_wpo(m, y + 300)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log('material', name)
    return m


GHOST_CODE = r"""
float3 n = normalize(N);
float3 v = normalize(V);
float f = pow(1.0 - saturate(abs(dot(n, v))), 3.0);
// as linhas do holograma sobem devagar (uma a cada 0,6 m), e uma faixa clara varre de baixo pra cima
float z = (WP.z - Obj.z) * 0.01;
float scan = frac(z / 0.6 - Time * 0.5) < 0.1 ? 1.0 : 0.0;
float sweep = saturate(1.0 - abs(frac(Time * 0.35) * 14.0 - z) / 0.8);
Opa = saturate(0.10 + 0.55 * f + 0.15 * scan + 0.25 * sweep);
return Color.rgb * Nits * (0.22 + 1.5 * f + 0.45 * scan + 0.8 * sweep);
"""


# (01/11, o Felipe: "tudo que tem bandeira tem que ter física [...] hoje nosso Technical é o único com bandeira com física que balança no
# vento que o mapa oferece"; e "cuidado com otimização") o pano de toda bandeira do jogo: a malha fixa SM_FlagCloth (1 × 1 m, 24 × 14,
# Tools/model_flag_cloth.py) que este material balança na placa, com a mesma conta da bandeira da Technical (a onda que corre do mastro
# pra ponta, a queda sem vento, a ponta mais pesada). A CPU só manda o vento quando ele muda (RTSUnitActor::SendFlagWind): o prédio uma
# vez, o veículo quando acelera ou vira. Os dados de primitiva: 0–3 a cor do jogador, 5 o comprimento e 6 a altura (cm), 7 a direção pra
# onde o pano aponta (rad, mundo), 8 o quanto levanta (0,12 pendurado a 1 esticado), 9 o ritmo da onda (voltas/s), 10 a fase no tempo
# zero (a continuidade quando o ritmo muda e o desencontro entre bandeiras), 11 o emblema (0 nenhum, 1 o círculo da GLA, 2 a estrela da China, 3 a tricolor da Rússia, 4 as listras da USA)
FLAG_P = r"""
struct FlagF
{
    // (01/11, o Felipe: "parece um png só, mal feita") o lado do pano: duas ondas que correm do mastro pra ponta, a de cima adiantada (a
    // dobra em diagonal da bandeira de verdade), crescendo pra ponta solta; 2 ondas no comprimento (era 1,3: lia como chapa torta)
    float W(float U, float Vv, float Lift, float Ph, float L)
    {
        float amp = (0.035 + 0.075 * Lift) * L * pow(max(U, 0.0), 0.8);
        return amp * (sin(6.2832 * (2.0 * U - Ph) + 1.1 * Vv) + 0.3 * sin(6.2832 * (4.3 * U - 1.6 * Ph) + 2.1 * Vv));
    }
    float3 P(float U, float Vv, float3 Mast, float Yaw, float Lift, float Ph, float L, float H)
    {
        float droop = (1.0 - Lift) * 1.3614;
        float D = droop * (0.85 + 0.3 * U);
        float X = U * L;
        float3 ax = float3(cos(Yaw), sin(Yaw), 0.0);
        float3 sd = float3(-ax.y, ax.x, 0.0);
        return Mast + ax * (X * cos(D)) - float3(0.0, 0.0, 1.0) * (X * sin(D) + Vv * H) + sd * W(U, Vv, Lift, Ph, L);
    }
};
FlagF F;
float Ph = Time * Rate + Off;
float Lc = max(L, 1.0), Hc = max(H, 1.0);
"""
FLAG_WPO = FLAG_P + r"""
float U = saturate(LP.x / 100.0);
float Vv = saturate(-LP.z / 100.0);
UV = float2(U, Vv);
return F.P(U, Vv, Mast, Yaw, Lift, Ph, Lc, Hc) - WP;
"""
FLAG_PIX = FLAG_P + r"""
// (só no pixel: o fwidth não existe no vértice)
struct FlagPx
{
    // o ruído de valor 2D (a sujeira do pano em manchas, sem textura)
    float Hs(float2 p) { return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453); }
    float Vn(float2 p)
    {
        float2 i = floor(p), f = frac(p);
        f = f * f * (3.0 - 2.0 * f);
        return lerp(lerp(Hs(i), Hs(i + float2(1, 0)), f.x), lerp(Hs(i + float2(0, 1)), Hs(i + float2(1, 1)), f.x), f.y);
    }
    // a parte do pixel dentro de [0, 1) × [0, 1) (a caixa filtrada: a borda do cantão e da faixa sem serrilhado)
    float Box(float2 c)
    {
        float2 w = max(fwidth(c), 1e-4);
        float2 k = (1.0 - smoothstep(1.0 - w, 1.0 + w, c)) * smoothstep(-w, w, c);
        return k.x * k.y;
    }
};
FlagPx G;
float U = UVi.x, Vv = UVi.y;
// a normal do pano dobrado (a malha fixa não tem a dobra): a diferença da forma em u e em v, virada pra câmera (o pano tem dois lados)
float3 p0 = F.P(U, Vv, Mast, Yaw, Lift, Ph, Lc, Hc);
float3 pu = F.P(U + 0.01, Vv, Mast, Yaw, Lift, Ph, Lc, Hc);
float3 pv = F.P(U, Vv + 0.02, Mast, Yaw, Lift, Ph, Lc, Hc);
float3 n = normalize(cross(pv - p0, pu - p0));
float3 toCam = Cam - WP;
if (dot(n, toCam) < 0.0) n = -n;
Nrm = n;
// a sombra da dobra: o vale da onda visto do lado da câmera (a curvatura do lado do pano, relativa à da onda principal)
float e = 0.03;
float c2 = F.W(U + e, Vv, Lift, Ph, Lc) + F.W(max(U - e, 0.0), Vv, Lift, Ph, Lc) - 2.0 * F.W(U, Vv, Lift, Ph, Lc);
float3 sdv = float3(-sin(Yaw), cos(Yaw), 0.0);
float side = dot(toCam, sdv) > 0.0 ? 1.0 : -1.0;
float ref = (0.035 + 0.075 * Lift) * Lc * 0.142 * pow(max(U, 0.05), 0.8);
float fold = 1.0 - 0.35 * saturate(c2 * side / max(ref, 1e-3));
// o emblema no pano (no espaço do pano, em cm: não estica com o tamanho); as cores de campanha (o sol e o pó desbotam: o branco
// fica creme, o vermelho fica tijolo)
float2 q = float2(U * Lc, Vv * Hc);
float3 base = PC.rgb;
const float3 cWhite = float3(0.66, 0.64, 0.58);
const float3 cRed = float3(0.46, 0.065, 0.05);
if (Emblem > 0.5 && Emblem < 1.5)
{
    // a GLA (a ficha): o círculo claro no meio, com o anel
    float r = length(q - float2(0.5 * Lc, 0.5 * Hc)) / (0.3 * Hc);
    float aw = max(fwidth(r), 1e-3);
    base = lerp(base, cWhite, (1.0 - smoothstep(0.96 - aw, 0.96 + aw, r)) * smoothstep(0.66 - aw, 0.66 + aw, r));
    base = lerp(base, cWhite, 1.0 - smoothstep(0.46 - aw, 0.46 + aw, r));
}
else if (Emblem > 1.5 && Emblem < 2.5)
{
    // a China: a estrela de 5 pontas amarela no canto de cima, perto do mastro
    float2 c = q - float2(0.24 * Lc, 0.3 * Hc);
    float a = atan2(c.x, -c.y);
    float seg = 6.2832 / 5.0;
    float k = abs(frac(a / seg + 0.5) - 0.5) * seg;
    float rr = length(c) / (0.22 * Hc);
    float edge = lerp(1.0, 0.382, saturate(k / (seg * 0.5)));
    float aw = max(fwidth(rr), 1e-3);
    base = lerp(base, float3(0.80, 0.60, 0.08), 1.0 - smoothstep(edge - aw, edge + aw, rr));
}
else if (Emblem > 2.5 && Emblem < 3.5)
{
    // (01/11, docs/elenco/EMBLEMAS.md: a estrela é da China) a Rússia: a tricolor (branco, azul, vermelho) no quarto de cima perto do
    // mastro, o resto do pano na cor do time (a leitura do jogador)
    float2 r0 = q / float2(0.42 * Lc, 0.5 * Hc);
    float fy = max(fwidth(r0.y), 1e-4);
    float3 tri = lerp(lerp(cWhite, float3(0.05, 0.15, 0.48), smoothstep(0.3333 - fy, 0.3333 + fy, r0.y)), cRed,
                      smoothstep(0.6667 - fy, 0.6667 + fy, r0.y));
    base = lerp(base, tri, G.Box(r0));
}
else if (Emblem > 3.5)
{
    // (01/11) a USA: as 13 listras vermelhas e brancas (a onda quadrada filtrada pela caixa do pixel: longe vira o tom médio em vez de
    // serrilhar) e o cantão na cor do time com as estrelas brancas em grade (a leitura do jogador fica no cantão)
    float x = Vv * 6.5;
    float fw = max(fwidth(x), 1e-4);
    float a0 = x - 0.5 * fw, a1 = x + 0.5 * fw;
    float red = saturate(((floor(a1) * 0.5 + min(frac(a1), 0.5)) - (floor(a0) * 0.5 + min(frac(a0), 0.5))) / fw);
    base = lerp(cWhite, cRed, red);
    float2 cq = q / float2(0.40 * Lc, 0.54 * Hc);
    float2 g = cq * float2(6.0, 5.0);
    float d = length(frac(g) - 0.5);
    float aw = max(fwidth(d), 1e-3);
    float star = lerp(1.0 - smoothstep(0.19 - aw, 0.19 + aw, d), 0.11, saturate(max(fwidth(g.x), fwidth(g.y)) * 3.0 - 0.6));
    base = lerp(base, lerp(PC.rgb * 0.85, cWhite, star), G.Box(cq));
}
// a bainha de lona do lado do mastro (a tira clara de ~5 cm que o mastro segura)
base = lerp(base, float3(0.50, 0.47, 0.40), 1.0 - smoothstep(4.0, 6.0, q.x));
// a sujeira do pano: manchas de ~25 e ~9 cm, e o pó que sobe da barra de baixo e da ponta que bate
float gr = 0.6 * G.Vn(q / 25.0) + 0.4 * G.Vn(q / 9.0 + 7.3);
base *= lerp(0.86, 1.04, gr);
base = lerp(base, float3(0.50, 0.42, 0.31), 0.22 * saturate(Vv * Vv + 0.5 * U * U) * (0.6 + 0.4 * gr));
base *= fold;
// o tecido deixa passar a luz (o sol atrás da bandeira acende o pano, como a folha): a cor dele, mais fraca
Sss = base * 0.55;
return base;
"""


def flag_material():
    """O mestre do pano das bandeiras (M_FlagCloth): o balanço no vértice, a normal e o emblema no pixel."""
    m = fresh('M_FlagCloth', MATS, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('two_sided', True)
    # (01/11, "parece um png só") o pano é fino: a luz do sol passa por ele (o modelo de folhagem de dois lados da Unreal)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    m.set_editor_property('tangent_space_normal', False)
    m.set_editor_property('used_with_static_lighting', False)
    F2 = unreal.CustomMaterialOutputType.CMOT_FLOAT2
    y = -1000

    def node(cls, x=-1300):
        nonlocal y
        y += 110
        return mel.create_material_expression(m, cls, x, y)

    def custom(code, ins, out_type, extra, x):
        c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, x, 0)
        c.set_editor_property('code', code)
        c.set_editor_property('output_type', out_type)
        c.set_editor_property('description', 'FlagCloth')
        cis = []
        for n in ins:
            ci = unreal.CustomInput()
            ci.set_editor_property('input_name', n)
            cis.append(ci)
        c.set_editor_property('inputs', cis)
        outs = []
        for n, ty in extra:
            co = unreal.CustomOutput()
            co.set_editor_property('output_name', n)
            co.set_editor_property('output_type', ty)
            outs.append(co)
        c.set_editor_property('additional_outputs', outs)
        return c
    common = ['Mast', 'Time', 'L', 'H', 'Yaw', 'Lift', 'Rate', 'Off']
    vs = custom(FLAG_WPO, ['LP', 'WP'] + common, F3, [('UV', F2)], -700)
    ps = custom(FLAG_PIX, ['UVi', 'Cam', 'WP', 'PC', 'Emblem'] + common, F3, [('Nrm', F3), ('Sss', F3)], -300)
    # as entradas comuns (o mesmo nó nas duas). (01/11, o Felipe: "a bandeira mal posicionada") o mastro é o pivô do componente (o
    # local (0, 0, 0) levado pro mundo): o ObjectPositionWS é o CENTRO DA CAIXA da malha (a doc da Epic), e a do pano de 1 × 1 m tem o
    # centro a 50 cm pra ponta e 50 cm pra baixo: toda bandeira saía solta do mastro
    zero = node(unreal.MaterialExpressionConstant3Vector)
    zero.set_editor_property('constant', unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
    mast = node(unreal.MaterialExpressionTransformPosition)
    mast.set_editor_property('transform_source_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    mast.set_editor_property('transform_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    assert mel.connect_material_expressions(zero, '', mast, ''), 'mast in'
    shared = {'Mast': mast, 'Time': node(unreal.MaterialExpressionTime)}
    for pin, idx in (('L', 5), ('H', 6), ('Yaw', 7), ('Lift', 8), ('Rate', 9), ('Off', 10), ('Emblem', 11)):
        s = node(unreal.MaterialExpressionScalarParameter)
        s.set_editor_property('parameter_name', 'Flag' + pin)
        s.set_editor_property('use_custom_primitive_data', True)
        s.set_editor_property('primitive_data_index', idx)
        s.set_editor_property('default_value', {'L': 100.0, 'H': 65.0, 'Lift': 0.5, 'Rate': 1.5}.get(pin, 0.0))
        shared[pin] = s
    for pin in common:
        assert mel.connect_material_expressions(shared[pin], '', vs, pin), 'vs ' + pin
        assert mel.connect_material_expressions(shared[pin], '', ps, pin), 'ps ' + pin
    assert mel.connect_material_expressions(shared['Emblem'], '', ps, 'Emblem'), 'Emblem'
    assert mel.connect_material_expressions(node(unreal.MaterialExpressionLocalPosition), '', vs, 'LP'), 'LP'
    wpv = node(unreal.MaterialExpressionWorldPosition)
    wpv.set_editor_property('world_position_shader_offset', unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    assert mel.connect_material_expressions(wpv, '', vs, 'WP'), 'WP vs'
    # o u e o v da malha, do vértice pro pixel (o interpolador da Unreal)
    vi = mel.create_material_expression(m, unreal.MaterialExpressionVertexInterpolator, -500, 300)
    assert mel.connect_material_expressions(vs, 'UV', vi, ''), 'interp'
    assert mel.connect_material_expressions(vi, '', ps, 'UVi'), 'UVi'
    assert mel.connect_material_expressions(node(unreal.MaterialExpressionCameraPositionWS), '', ps, 'Cam'), 'Cam'
    assert mel.connect_material_expressions(node(unreal.MaterialExpressionWorldPosition), '', ps, 'WP'), 'WP ps'
    pc = node(unreal.MaterialExpressionVectorParameter)
    pc.set_editor_property('parameter_name', 'PlayerColor')
    pc.set_editor_property('use_custom_primitive_data', True)
    pc.set_editor_property('primitive_data_index', 0)
    pc.set_editor_property('default_value', unreal.LinearColor(0.10, 0.45, 1.0, 1.0))
    assert mel.connect_material_expressions(pc, '', ps, 'PC'), 'PC'
    rough = node(unreal.MaterialExpressionConstant)
    rough.set_editor_property('r', 0.9)
    spec = node(unreal.MaterialExpressionConstant)   # (o tecido quase não tem brilho de espelho: sem ele o pano lia plástico)
    spec.set_editor_property('r', 0.3)
    P = unreal.MaterialProperty
    ok = [mel.connect_material_property(ps, '', P.MP_BASE_COLOR), mel.connect_material_property(ps, 'Nrm', P.MP_NORMAL),
          mel.connect_material_property(rough, '', P.MP_ROUGHNESS), mel.connect_material_property(spec, '', P.MP_SPECULAR),
          mel.connect_material_property(ps, 'Sss', P.MP_SUBSURFACE_COLOR),
          mel.connect_material_property(vs, '', P.MP_WORLD_POSITION_OFFSET)]
    if not all(ok):
        raise RuntimeError(f'M_FlagCloth: ligações falharam {ok}')
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log('material M_FlagCloth')
    # a malha do pano: sem Nanite (a malha é pequena e o WPO na Nanite iria pro raster programável), com o material
    src = os.path.join(REPO, 'captures', 'game', 'models', 'flag_cloth', 'SM_FlagCloth.glb')
    if not os.path.exists(src):
        raise RuntimeError('sem o SM_FlagCloth.glb: rode o Tools/model_flag_cloth.py no Blender')
    for path in import_glb(src, ROOT + '/Flag', 'SM_FlagCloth'):
        a = unreal.load_asset(path)
        if isinstance(a, unreal.StaticMesh):
            s = a.get_editor_property('nanite_settings')
            s.set_editor_property('enabled', False)
            a.set_editor_property('nanite_settings', s)
            for k in range(len(a.get_editor_property('static_materials'))):
                a.set_material(k, m)
            eal.save_loaded_asset(a)
            log('mesh', a.get_path_name(), 'pano de bandeira (sem Nanite)')
        elif isinstance(a, (unreal.MaterialInterface, unreal.Texture)):
            eal.delete_asset(path.split('.')[0])


def ghost_material():
    """O fantasma do prédio na hora de posicionar: o modelo como holograma (verde cabe, vermelho
    não; a cor vem pelo dado de primitiva 0–3). O ZH mostra o modelo transparente com a cor;
    aqui a borda acende (fresnel) e as linhas sobem, pra ler de cima por cima do chão claro."""
    m = fresh('M_StructureGhost', MATS, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('two_sided', False)
    custom = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property('code', GHOST_CODE)
    custom.set_editor_property('output_type', F3)
    ins = []
    for n in ('WP', 'N', 'V', 'Obj', 'Time', 'Color', 'Nits'):
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    custom.set_editor_property('inputs', ins)
    co = unreal.CustomOutput()
    co.set_editor_property('output_name', 'Opa')
    co.set_editor_property('output_type', F1)
    custom.set_editor_property('additional_outputs', [co])
    y = -600
    for pin, cls in (('WP', unreal.MaterialExpressionWorldPosition), ('N', unreal.MaterialExpressionVertexNormalWS),
                     ('V', unreal.MaterialExpressionCameraVectorWS), ('Obj', unreal.MaterialExpressionObjectPositionWS),
                     ('Time', unreal.MaterialExpressionTime)):
        y += 110
        e = mel.create_material_expression(m, cls, -800, y)
        assert mel.connect_material_expressions(e, '', custom, pin), pin
    col = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -800, y + 110)
    col.set_editor_property('parameter_name', 'GhostColor')
    col.set_editor_property('use_custom_primitive_data', True)
    col.set_editor_property('primitive_data_index', 0)
    col.set_editor_property('default_value', unreal.LinearColor(0.25, 1.0, 0.45, 1.0))
    assert mel.connect_material_expressions(col, '', custom, 'Color')
    nits = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -800, y + 220)
    nits.set_editor_property('parameter_name', 'Nits')
    nits.set_editor_property('default_value', 6000.0)
    assert mel.connect_material_expressions(nits, '', custom, 'Nits')
    P = unreal.MaterialProperty
    ok = [mel.connect_material_property(custom, '', P.MP_EMISSIVE_COLOR), mel.connect_material_property(custom, 'Opa', P.MP_OPACITY)]
    if not all(ok):
        raise RuntimeError(f'M_StructureGhost: ligações falharam {ok}')
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log('material M_StructureGhost')


def instances(parent, suffix):
    out = {}
    for slot, (color, rough, metal, extra) in SLOTS.items():
        name = f'MI_{slot}{suffix}'
        mi = fresh(name, MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property('parent', parent)
        mel.clear_all_material_instance_parameters(mi)
        mel.set_material_instance_vector_parameter_value(mi, 'BaseColor', unreal.LinearColor(*color, 1.0))
        mel.set_material_instance_scalar_parameter_value(mi, 'Roughness', rough)
        mel.set_material_instance_scalar_parameter_value(mi, 'Metallic', metal)
        for k, v in extra.items():
            if isinstance(v, tuple):
                mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*v, 1.0))
            else:
                mel.set_material_instance_scalar_parameter_value(mi, k, v)
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
        out[slot] = mi
    return out


def import_glb(path, dest, name):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', path)
    task.set_editor_property('destination_path', dest)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', True)
    tools.import_asset_tasks([task])
    return list(task.get_editor_property('imported_object_paths') or [])


def import_meshes(files, dest_of, slot_of, opaque):
    for f in files:
        stem = os.path.splitext(os.path.basename(f))[0]
        paths = import_glb(f, dest_of(f), stem)
        assets = [unreal.load_asset(p) for p in paths]
        for a in assets:
            if not isinstance(a, unreal.StaticMesh):
                continue
            s = a.get_editor_property('nanite_settings')
            s.set_editor_property('enabled', True)
            a.set_editor_property('nanite_settings', s)
            # set_material, um por um (trocar a lista inteira de uma vez não ficava gravado: a malha
            # seguiu apontando pros materiais do importador, que são apagados abaixo; teste base1, 10/10)
            mats = a.get_editor_property('static_materials')
            missing = []
            for k, sm in enumerate(mats):
                slot = str(sm.get_editor_property('material_slot_name'))
                key = slot_of(slot.split('.')[0].split('_')[0])
                if key in opaque:
                    a.set_material(k, opaque[key])
                else:
                    missing.append(slot)
            eal.save_loaded_asset(a)
            wrong = [str(m.get_editor_property('material_interface').get_path_name()) for m in a.get_editor_property('static_materials')
                     if not m.get_editor_property('material_interface') or not m.get_editor_property('material_interface').get_path_name().startswith(MATS)]
            if wrong:
                raise RuntimeError(f'{a.get_path_name()}: materiais fora de {MATS}: {wrong}')
            b = a.get_bounding_box()
            log('mesh', a.get_path_name(), 'slots', len(mats), 'sem material:', missing,
                'caixa (m) %.2f..%.2f %.2f..%.2f %.2f..%.2f' % (b.min.x / 100, b.max.x / 100, b.min.y / 100, b.max.y / 100,
                                                            b.min.z / 100, b.max.z / 100))
        # os materiais e texturas do importador: não servem (sem Nanite) e ninguém mais usa
        for a, p in zip(assets, paths):
            if isinstance(a, (unreal.MaterialInterface, unreal.Texture)):
                eal.delete_asset(p.split('.')[0])


def existing_instances():
    """Os MI_<slot> que já existem em /Game/Structures/Materials, sem regravar nada (o jogo aberto segura o
    M_Structure.uasset; o modo 'só o Dozer' não precisa mexer nos materiais)."""
    out = {}
    for slot in SLOTS:
        a = unreal.load_asset(f'{MATS}/MI_{slot}')
        if a:
            out[slot] = a
    if not out:
        raise RuntimeError(f'sem materiais em {MATS}: rode game.ps1 models uma vez antes')
    return out


def faction_instances(slots):
    """(31/10) Só as instâncias dos slots novos (MI_<slot>, _Build e _Holed), com os mestres que já existem: não regrava o
    M_Structure (o jogo aberto segura ele) nem as instâncias dos outros."""
    parents = {'': unreal.load_asset(f'{MATS}/M_Structure'), '_Build': unreal.load_asset(f'{MATS}/M_StructureBuild'),
               '_Holed': unreal.load_asset(f'{MATS}/M_StructureHoled')}
    if not parents['']:
        raise RuntimeError(f'sem o {MATS}/M_Structure: rode game.ps1 models uma vez antes')
    for suffix, parent in parents.items():
        if not parent:
            continue
        for slot in slots:
            color, rough, metal, extra = SLOTS[slot]
            name = f'MI_{slot}{suffix}'
            mi = fresh(name, MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            mi.set_editor_property('parent', parent)
            mel.clear_all_material_instance_parameters(mi)
            mel.set_material_instance_vector_parameter_value(mi, 'BaseColor', unreal.LinearColor(*color, 1.0))
            mel.set_material_instance_scalar_parameter_value(mi, 'Roughness', rough)
            mel.set_material_instance_scalar_parameter_value(mi, 'Metallic', metal)
            for k, v in extra.items():
                if isinstance(v, tuple):
                    mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*v, 1.0))
                else:
                    mel.set_material_instance_scalar_parameter_value(mi, k, v)
            mel.update_material_instance(mi)
            eal.save_loaded_asset(mi)


def main():
    if os.environ.get('PROV_FACTION_ONLY'):
        # (31/10, o elenco das 3 facções) as instâncias dos slots novos e os modelos do FACTION_MODELS que existirem;
        # PROV_KEYS=china_type99,gla_palace: só essas pastas
        faction_instances(['UsCore', 'PowerLine', 'UsBody', 'UsConcrete', 'UsPaint', 'RuBlue', 'RuPaint', 'RuSteel', 'RuMetal', 'RuConcrete', 'RuRoof', 'RuRed', 'RuWhite',
                           'CnConcrete', 'CnPlinth', 'CnRoof', 'CnMetal', 'CnGold', 'Fire', 'CnTankPaint', 'CnTankCamo', 'CnTankMetal',
                           'GlaEarth', 'GlaNet', 'GlaPaint', 'GlaStripe', 'GlaTile', 'Broken', 'BrokenAdobe'])
        opaque = existing_instances()
        keys = [k.strip() for k in os.environ.get('PROV_KEYS', '').split(',') if k.strip()]
        done = []
        for folder, pattern, dest, slots in FACTION_MODELS:
            if keys and folder not in keys:
                continue
            files = sorted(glob.glob(os.path.join(REPO, 'captures', 'game', 'models', folder, pattern)))
            if not files:
                log('sem GLB em captures/game/models/' + folder + ' (fica de fora)')
                continue
            if slots == 'usa':
                slot_of = lambda k: USA_VEHICLE_SLOTS.get(k, k)
            elif slots == 'dozer':
                # (01/11) os slots do Dozer, com a esteira que anda (a Paladin do elenco)
                slot_of = lambda k: dict(DOZER_SLOTS, Track='TankTrackRun').get(k, k)
            elif slots:
                slot_of = lambda k, t=slots: t.get(k, k)
            else:
                slot_of = lambda k: k
            import_meshes(files, lambda f, d=dest: d, slot_of, opaque)
            done.append(folder)
        log('done (o elenco das facções:', ', '.join(done) + ')')
        return
    if os.environ.get('PROV_TRUCK_ONLY'):
        opaque = existing_instances()
        files = sorted(glob.glob(os.path.join(TRUCK_SRC, 'SM_USA_Supply*.glb')))
        if not files:
            raise RuntimeError(f'nenhum SM_USA_Supply*.glb em {TRUCK_SRC}: rode o Blender antes (model_usa_supply_truck.py)')
        import_meshes(files, lambda f: TRUCK_ROOT, lambda k: DOZER_SLOTS.get(k, k), opaque)
        log('done (o caminhão)')
        return
    if os.environ.get('PROV_PATRIOT_ONLY'):
        opaque = existing_instances()
        files = sorted(glob.glob(os.path.join(PATRIOT_SRC, 'SM_USA_Patriot_*.glb')))
        if not files:
            raise RuntimeError(f'nenhum SM_USA_Patriot_*.glb em {PATRIOT_SRC}: rode o Blender antes (model_usa_patriot.py)')
        import_meshes(files, lambda f: PATRIOT_ROOT, lambda k: k, opaque)
        log('done (a Patriot)')
        return
    if os.environ.get('PROV_V4_ONLY') or os.environ.get('PROV_AIR_ONLY') or os.environ.get('PROV_V5_ONLY'):
        opaque = existing_instances()
        # (24/10, escopo B do ar) PROV_AIR_KEYS=usa_airfield,usa_comanche: só essas pastas (o jogo aberto segura as outras malhas)
        keys = [k.strip() for k in os.environ.get('PROV_AIR_KEYS', '').split(',') if k.strip()]
        group = AIR_MODELS if os.environ.get('PROV_AIR_ONLY') else (V5_MODELS if os.environ.get('PROV_V5_ONLY') else V4_MODELS)
        for folder, pattern, dest, slots in group:
            if keys and folder not in keys:
                continue
            # só a pasta de cima (o usa_tomahawk/pose tem a pose de tiro, que é só da prévia)
            files = sorted(glob.glob(os.path.join(REPO, 'captures', 'game', 'models', folder, pattern)))
            if not files:
                raise RuntimeError(f'nenhum {pattern} em captures/game/models/{folder}: rode o Blender antes')
            import_meshes(files, lambda f, d=dest: d, (lambda k: DOZER_SLOTS.get(k, k)) if slots == 'dozer' else (lambda k: k), opaque)
        log('done (as aeronaves e o Airfield)' if os.environ.get('PROV_AIR_ONLY') else
            ('done (o Linebacker e o Stryker)' if os.environ.get('PROV_V5_ONLY') else 'done (os modelos da v4)'))
        return
    if os.environ.get('PROV_DOZER_ONLY'):
        opaque = existing_instances()
        files = sorted(glob.glob(os.path.join(DOZER_SRC, 'SM_USA_Dozer_*.glb')))
        if not files:
            raise RuntimeError(f'nenhum SM_USA_Dozer_*.glb em {DOZER_SRC}: rode o Blender antes (model_usa_dozer.py)')
        import_meshes(files, lambda f: DOZER_ROOT, lambda k: DOZER_SLOTS.get(k, k), opaque)
        log('done (o Dozer)')
        return
    opaque = instances(master('M_Structure', False), '')
    instances(master('M_StructureBuild', True), '_Build')
    # (30/10, item 31) o prédio ferido com furos: o mesmo visual do pronto, mascarado e de dois lados (ARTSUnitActor::OpenStructureHole)
    instances(master('M_StructureHoled', False, holed=True), '_Holed')
    # (01/11) a esteira que anda: o mestre com o deslocamento dos elos só na instância dela (o WPO custa: o resto fica sem)
    track_master = master('M_StructureTrack', False, track=True)
    for slot in ('TankTrackRun', 'UsTrackRun'):
        mi = opaque[slot]
        mi.set_editor_property('parent', track_master)
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
    ghost_material()
    flag_material()
    if os.environ.get('PROV_MATERIALS_ONLY'):
        log('done (só os materiais)')
        return
    if os.environ.get('PROV_CIVIL_ONLY'):
        files = sorted(glob.glob(os.path.join(CIVIL_SRC, 'SM_civ_*.glb')))
        if not files:
            raise RuntimeError(f'nenhum SM_civ_*.glb em {CIVIL_SRC}: o kit civil vem da sessão do Look')
        # (13/10) as miudezas da maquete do Look (caixa d'água, ar-condicionado, toldo, entulho...): o village.json v3 do
        # WorldForge põe como peças sem bloqueio; os mesmos slots do kit civil (captures/look/kit_vila/manifest.json)
        files += sorted(glob.glob(os.path.join(TANK_SRC, 'SM_LL_Prop_*.glb')))
        import_meshes(files, lambda f: CIVIL_ROOT, lambda k: CIVIL_SLOTS.get(k, k), opaque)
        log('done (a vila)')
        return
    if os.environ.get('PROV_FOLDER'):
        # só uma pasta (game.ps1 debris: os destroços), sem reimportar os prédios
        files = sorted(glob.glob(os.path.join(SRC, os.environ['PROV_FOLDER'], '*.glb')))
        if not files:
            raise RuntimeError(f'nenhum GLB em {SRC}/{os.environ["PROV_FOLDER"]}')
        import_meshes(files, lambda f: f'{ROOT}/{os.path.basename(os.path.dirname(f))}', lambda k: k, opaque)
        log('done (só', os.environ['PROV_FOLDER'] + ')')
        return
    if os.environ.get('PROV_TANK_ONLY') is None:
        files = sorted(glob.glob(os.path.join(SRC, '*', '*.glb')))
        if not files:
            raise RuntimeError(f'nenhum GLB em {SRC}: rode o Blender antes (game.ps1 models)')
        import_meshes(files, lambda f: f'{ROOT}/{os.path.basename(os.path.dirname(f))}', lambda k: k, opaque)
    # o tanque da maquete do Look (pedido do Felipe, 11/10): só lê os GLBs dela (look.ps1 tank)
    tank = [os.path.join(TANK_SRC, f'SM_LL_USATank_{p}.glb') for p in TANK_PARTS]
    if all(os.path.exists(f) for f in tank):
        import_meshes(tank, lambda f: TANK_ROOT, lambda k: TANK_SLOTS.get(k, k), opaque)
    else:
        log('sem o tanque do Look em', TANK_SRC, '(look.ps1 tank): fica o de antes')
    log('done')


main()
# (30/10) reimportar volta a malha pra escala 1: a escala de cada unidade de novo (Units.json visual.mesh_scale, unit_scale_apply.py)
import sys; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import unit_scale_apply; unit_scale_apply.apply()
