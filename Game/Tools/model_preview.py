"""Folha de conferência de um modelo provisório (Cycles na CPU: não usa a placa).

Uso: blender -b --factory-startup -P Game/Tools/model_preview.py -- <pasta_glb> <pasta_saida> <modelo> [tanque_usa_dir]
Sai com as vistas da ficha de cada modelo (ortográficas em fundo cinza e as 3/4 em perspectiva) mais a câmera
do jogo (37,5°) no chão de areia com o tanque USA do lado, pra medir a proporção. As peças que se mexem entram
no lugar delas (a mesma origem do modelo). Convenção: Unreal X frente, Y direita; no Blender a direita é −Y.
"""
import json
import math
import os
import sys

import bpy
from mathutils import Quaternion, Vector

args = sys.argv[sys.argv.index('--') + 1:]
SRC, OUT, MODEL = args[0], args[1], args[2]
TANK = args[3] if len(args) > 3 else ''
os.makedirs(OUT, exist_ok=True)

USA_PARTS = ['SM_LL_USATank_Hull', 'SM_LL_USATank_Turret', 'SM_LL_USATank_Barrel', 'SM_LL_USATank_Wheels',
             'SM_LL_USATank_Track_l', 'SM_LL_USATank_Track_r']

# por modelo: as peças (GLB, posição), as vistas e onde o tanque USA fica na câmera do jogo
# vista ortográfica: ('ortho', direção do olhar em Blender, o "cima" da imagem, (largura, altura))
# vista em perspectiva: ('persp', inclinação, giro, (largura, altura), campo de visão, margem)
MODELS = {
    'gla_tank': {
        'parts': [('SM_GLA_MBT_Hull', (0, 0, 0)), ('SM_GLA_MBT_Turret', (0, 0, 0)), ('SM_GLA_MBT_Barrel', (0, 0, 0)),
                  ('SM_GLA_MBT_Wheels', (0, 0, 0)), ('SM_GLA_MBT_Track_l', (0, 0, 0)), ('SM_GLA_MBT_Track_r', (0, 0, 0))],
        'views': [('front', 'ortho', (-1, 0, 0), (0, 0, 1), (900, 900)),
                  ('back', 'ortho', (1, 0, 0), (0, 0, 1), (900, 900)),
                  ('left', 'ortho', (0, -1, 0), (0, 0, 1), (1800, 820)),
                  ('top', 'ortho', (0, 0, -1), (0, -1, 0), (1800, 1000)),
                  ('three_quarter', 'persp', 32.0, 215.0, (1600, 900), 35.0, 0.95),
                  ('three_quarter_right', 'persp', 32.0, 145.0, (1600, 900), 35.0, 0.95)],
        'usa_at': (0, -6.0, 0),
        'game': [('game_cam', 37.5, 35.0, 50.0, 0.8), ('game_cam_close', 37.5, 35.0, 28.0, 0.75)],
    },
    'usa_dozer': {
        'parts': [('SM_USA_Dozer_Hull', (0, 0, 0)), ('SM_USA_Dozer_Blade', (-0.352, 0.0, 0.66)), ('SM_USA_Dozer_Beacon', (0.396, -0.264, 2.86)), ('SM_USA_Dozer_Wheels', (0, 0, 0)),
                  ('SM_USA_Dozer_Track_l', (0, 0, 0)), ('SM_USA_Dozer_Track_r', (0, 0, 0))],
        'views': [('front', 'ortho', (-1, 0, 0), (0, 0, 1), (900, 900)),
                  ('back', 'ortho', (1, 0, 0), (0, 0, 1), (900, 900)),
                  ('left', 'ortho', (0, -1, 0), (0, 0, 1), (1800, 820)),
                  ('top', 'ortho', (0, 0, -1), (0, -1, 0), (1800, 1000)),
                  ('three_quarter', 'persp', 27.0, 232.0, (1600, 1000), 30.0, 0.95),
                  ('three_quarter_right', 'persp', 30.0, 40.0, (1600, 900), 35.0, 0.95)],
        'usa_at': (0, 6.0, 0),
        'game': [('game_cam', 37.5, 35.0, 50.0, 0.8), ('game_cam_close', 37.5, 35.0, 28.0, 0.75)],
    },
    'usa_supply_truck': {
        'parts': [('SM_USA_SupplyTruck_Hull', (0, 0, 0)), ('SM_USA_SupplyTruck_Wheels', (0, 0, 0))]
                 + [('SM_USA_SupplyCrate_Small', (x * 0.72, -y * 0.72, 1.3 * 0.72)) for (x, y) in ((-2.45, -0.68), (-2.45, 0.68), (-0.85, -0.68), (-0.85, 0.68))],
        'views': [('front', 'ortho', (-1, 0, 0), (0, 0, 1), (900, 900)),
                  ('back', 'ortho', (1, 0, 0), (0, 0, 1), (900, 900)),
                  ('left', 'ortho', (0, -1, 0), (0, 0, 1), (1800, 820)),
                  ('top', 'ortho', (0, 0, -1), (0, -1, 0), (1800, 1000)),
                  ('three_quarter', 'persp', 27.0, 232.0, (1600, 1000), 30.0, 0.95),
                  ('three_quarter_right', 'persp', 30.0, 40.0, (1600, 900), 35.0, 0.95)],
        'usa_at': (0, 6.0, 0),
        'game': [('game_cam', 37.5, 35.0, 50.0, 0.8), ('game_cam_close', 37.5, 35.0, 28.0, 0.75)],
    },
    # o Tomahawk (o chassi do caminhão com os 2 tubos deitados, Game/Tools/model_tomahawk.py)
    'usa_tomahawk': {
        'parts': [('SM_USA_Tomahawk_Hull', (0, 0, 0)), ('SM_USA_Tomahawk_Wheels', (0, 0, 0)),
                  ('SM_USA_Tomahawk_Turret', (0, 0, 0)), ('SM_USA_Tomahawk_Pod', (0, 0, 0))],
        'views': [('front', 'ortho', (-1, 0, 0), (0, 0, 1), (900, 900)),
                  ('back', 'ortho', (1, 0, 0), (0, 0, 1), (900, 900)),
                  ('left', 'ortho', (0, -1, 0), (0, 0, 1), (1800, 820)),
                  ('top', 'ortho', (0, 0, -1), (0, -1, 0), (1800, 1000)),
                  ('three_quarter', 'persp', 27.0, 232.0, (1600, 1000), 30.0, 0.95),
                  ('three_quarter_right', 'persp', 30.0, 40.0, (1600, 900), 35.0, 0.95)],
        'usa_at': (0, 6.0, 0),
        'game': [('game_cam', 37.5, 35.0, 50.0, 0.8), ('game_cam_close', 37.5, 35.0, 28.0, 0.75)],
    },
    # ... e com o lançador levantado 45° (a pose de tiro: pose/ não vai pro jogo)
    'usa_tomahawk_fire': {
        'parts': [('SM_USA_Tomahawk_Hull', (0, 0, 0)), ('SM_USA_Tomahawk_Wheels', (0, 0, 0)),
                  ('captures/game/models/usa_tomahawk/pose/SM_USA_Tomahawk_Launcher_Fire', (0, 0, 0))],
        'views': [('fire_left', 'ortho', (0, -1, 0), (0, 0, 1), (1800, 1000)),
                  ('fire_three_quarter', 'persp', 27.0, 232.0, (1600, 1000), 30.0, 0.95),
                  ('fire_three_quarter_right', 'persp', 30.0, 40.0, (1600, 1000), 35.0, 0.95)],
        'usa_at': (0, 6.0, 0),
        'game': [('fire_game_cam', 37.5, 35.0, 50.0, 0.8), ('fire_game_cam_close', 37.5, 35.0, 28.0, 0.75)],
    },
    # (25/10, v5) o transporte de tropas 8x8 (Game/Tools/model_usa_stryker.py): o casco, as 8 rodas, a estação de armas (torre) e o cano
    'usa_stryker': {
        'parts': [('SM_USA_Stryker_Hull', (0, 0, 0)), ('SM_USA_Stryker_Wheels', (0, 0, 0)),
                  ('SM_USA_Stryker_Turret', (0, 0, 0)), ('SM_USA_Stryker_Barrel', (0, 0, 0))],
        # (a ficha tem a frente à direita em cima e de lado: 'top' com a esquerda do carro em cima e 'right' = o lado da direita)
        'views': [('front', 'ortho', (-1, 0, 0), (0, 0, 1), (900, 900)),
                  ('back', 'ortho', (1, 0, 0), (0, 0, 1), (900, 900)),
                  ('left', 'ortho', (0, -1, 0), (0, 0, 1), (1800, 820)),
                  ('right', 'ortho', (0, 1, 0), (0, 0, 1), (1800, 820)),
                  ('top', 'ortho', (0, 0, -1), (0, 1, 0), (1800, 1000)),
                  ('three_quarter', 'persp', 27.0, 232.0, (1600, 1000), 30.0, 0.95),
                  ('three_quarter_right', 'persp', 30.0, 40.0, (1600, 900), 35.0, 0.95)],
        'usa_at': (0, 6.0, 0),
        'game': [('game_cam', 37.5, 35.0, 50.0, 0.8), ('game_cam_close', 37.5, 35.0, 28.0, 0.75)],
    },
    # (25/10, v5) o antiaéreo de lagarta (Game/Tools/model_usa_linebacker.py): os canhões deitados (a pose de marcha) e o radar parado
    'usa_linebacker': {
        'parts': [('SM_USA_Linebacker_Hull', (0, 0, 0)), ('SM_USA_Linebacker_Wheels', (0, 0, 0)),
                  ('SM_USA_Linebacker_Track_l', (0, 0, 0)), ('SM_USA_Linebacker_Track_r', (0, 0, 0)),
                  ('SM_USA_Linebacker_Turret', (0, 0, 0)), ('SM_USA_Linebacker_Guns', (0, 0, 0)),
                  ('captures/game/models/usa_linebacker/pose/SM_USA_Linebacker_Dish_Placed', (0, 0, 0))],
        'views': [('front', 'ortho', (-1, 0, 0), (0, 0, 1), (900, 900)),
                  ('back', 'ortho', (1, 0, 0), (0, 0, 1), (900, 900)),
                  ('left', 'ortho', (0, -1, 0), (0, 0, 1), (1800, 820)),
                  ('top', 'ortho', (0, 0, -1), (0, -1, 0), (1800, 1000)),
                  ('three_quarter', 'persp', 27.0, 232.0, (1600, 1000), 30.0, 0.95),
                  ('three_quarter_right', 'persp', 30.0, 40.0, (1600, 900), 35.0, 0.95)],
        'usa_at': (0, 6.0, 0),
        'game': [('game_cam', 37.5, 35.0, 50.0, 0.8), ('game_cam_close', 37.5, 35.0, 28.0, 0.75)],
    },
    # ... e com os canhões levantados (a pose de tiro: pose/ não vai pro jogo)
    'usa_linebacker_fire': {
        'parts': [('SM_USA_Linebacker_Hull', (0, 0, 0)), ('SM_USA_Linebacker_Wheels', (0, 0, 0)),
                  ('SM_USA_Linebacker_Track_l', (0, 0, 0)), ('SM_USA_Linebacker_Track_r', (0, 0, 0)),
                  ('captures/game/models/usa_linebacker/pose/SM_USA_Linebacker_Turret_Fire', (0, 0, 0))],
        'views': [('fire_left', 'ortho', (0, -1, 0), (0, 0, 1), (1800, 1000)),
                  ('fire_three_quarter', 'persp', 27.0, 232.0, (1600, 1000), 30.0, 0.95),
                  ('fire_three_quarter_right', 'persp', 30.0, 40.0, (1600, 1000), 35.0, 0.95)],
        'usa_at': (0, 6.0, 0),
        'game': [('fire_game_cam', 37.5, 35.0, 50.0, 0.8), ('fire_game_cam_close', 37.5, 35.0, 28.0, 0.75)],
    },
    'usa_patriot': {
        'parts': [('SM_USA_Patriot_Base', (0, 0, 0)), ('SM_USA_Patriot_Turret', (0, 0, 0)), ('SM_USA_Patriot_Dish', (-2.35, 2.35, 4.4))],
        'views': [('top', 'ortho', (0, 0, -1), (-1, 0, 0), (1200, 1200)),
                  ('front', 'ortho', (-1, 0, 0), (0, 0, 1), (1200, 900)),
                  ('side', 'ortho', (0, 1, 0), (0, 0, 1), (1200, 900)),
                  ('three_quarter', 'persp', 32.0, 215.0, (1600, 1000), 30.0, 0.95),
                  ('three_quarter_right', 'persp', 32.0, 145.0, (1600, 1000), 30.0, 0.95),
                  ('back_three_quarter', 'persp', 30.0, 35.0, (1600, 1000), 30.0, 0.95)],
        'usa_at': (0, 7.0, 0),
        'game': [('game_cam', 37.5, 200.0, 50.0, 0.8), ('game_cam_close', 37.5, 200.0, 28.0, 0.75)],
        'scaffold': 'SM_USA_Patriot_Scaffold',
    },
    'supply_dock': {
        'parts': [('SM_SupplyDock_Yard', (0, 0, 0))],
        'layout': 'dock_layout.json',                                                  # as caixas do monte e as vazias
        'views': [('top', 'ortho', (0, 0, -1), (-1, 0, 0), (1400, 1400)),
                  ('three_quarter', 'persp', 34.0, 215.0, (1600, 1000), 32.0, 0.95),
                  ('three_quarter_right', 'persp', 34.0, 145.0, (1600, 1000), 32.0, 0.95),
                  ('low', 'persp', 18.0, 200.0, (1600, 900), 35.0, 0.95)],
        'usa_at': (2.0, 8.5, 0),
        'game': [('game_cam', 37.5, 200.0, 50.0, 0.85), ('game_cam_close', 37.5, 200.0, 30.0, 0.8)],
    },
    'supply_yard': {
        'parts': [('SM_SupplyYard', (0, 0, 0))],
        'layout': 'yard_layout.json',
        'views': [('top', 'ortho', (0, 0, -1), (-1, 0, 0), (1400, 1400)),
                  ('three_quarter', 'persp', 34.0, 215.0, (1600, 1000), 32.0, 0.95),
                  ('three_quarter_right', 'persp', 34.0, 145.0, (1600, 1000), 32.0, 0.95),
                  ('low', 'persp', 16.0, 200.0, (1600, 900), 35.0, 0.95)],
        'usa_at': (0, 0.0, 0),
        'game': [('game_cam', 37.5, 200.0, 50.0, 0.85), ('game_cam_close', 37.5, 200.0, 30.0, 0.8)],
    },
    'usa_strategy_center': {
        'parts': [('SM_USA_StrategyCenter_Base', (0, 0, 0)), ('SM_USA_StrategyCenter_Dish', (0.0, 0.0, 5.8))],
        'views': [('top', 'ortho', (0, 0, -1), (-1, 0, 0), (1300, 1300)),
                  ('front', 'ortho', (-1, 0, 0), (0, 0, 1), (1400, 900)),
                  ('side', 'ortho', (0, 1, 0), (0, 0, 1), (1400, 900)),
                  ('three_quarter', 'persp', 34.0, 215.0, (1600, 1000), 30.0, 0.95),
                  ('back_three_quarter', 'persp', 32.0, 35.0, (1600, 1000), 30.0, 0.95)],
        'usa_at': (2.0, 13.8, 0),
        'game': [('game_cam', 37.5, 200.0, 50.0, 0.85), ('game_cam_close', 37.5, 200.0, 30.0, 0.8)],
        'scaffold': 'SM_USA_StrategyCenter_Scaffold',
    },
    'usa_drop_zone': {
        'parts': [('SM_USA_DropZone_Base', (0, 0, 0)), ('SM_USA_DropZone_Radar', (-6.3, 6.3, 6.4))],
        'views': [('top', 'ortho', (0, 0, -1), (-1, 0, 0), (1300, 1300)),
                  ('front', 'ortho', (-1, 0, 0), (0, 0, 1), (1400, 800)),
                  ('three_quarter', 'persp', 34.0, 215.0, (1600, 1000), 30.0, 0.95),
                  ('back_three_quarter', 'persp', 32.0, 35.0, (1600, 1000), 30.0, 0.95)],
        'usa_at': (0.0, 11.5, 0),
        'game': [('game_cam', 37.5, 200.0, 50.0, 0.85), ('game_cam_close', 37.5, 200.0, 30.0, 0.8)],
        'scaffold': 'SM_USA_DropZone_Scaffold',
    },
    # o lançamento na câmera do jogo: 2 caixas já no X, 1 caindo de paraquedas (as peças do supply_drop)
    'usa_drop_zone_drop': {
        'parts': [('SM_USA_DropZone_Base', (0, 0, 0)), ('SM_USA_DropZone_Radar', (-6.3, 6.3, 6.4)),
                  ('captures/game/models/supply_drop/SM_SupplyDrop_Crate', (0.6, 0.4, 0.18)),
                  ('captures/game/models/supply_drop/SM_SupplyDrop_Crate', (-0.9, -0.7, 0.18)),
                  ('captures/game/models/supply_drop/SM_SupplyDrop_Crate', (1.2, -1.5, 9.0)),
                  ('captures/game/models/supply_drop/SM_SupplyDrop_Canopy', (1.2, -1.5, 9.0))],
        'views': [],
        'usa_at': (0.0, 11.5, 0),
        'game': [('game_cam', 37.5, 200.0, 50.0, 0.85)],
    },
    'supply_drop': {
        'parts': [('SM_SupplyDrop_Crate', (0, 0, 0)), ('SM_SupplyDrop_Canopy', (0, 0, 0))],
        'views': [('top', 'ortho', (0, 0, -1), (-1, 0, 0), (1000, 1000)),
                  ('front', 'ortho', (-1, 0, 0), (0, 0, 1), (900, 1100)),
                  ('three_quarter', 'persp', 30.0, 215.0, (1200, 1200), 35.0, 0.95)],
        'usa_at': (0.0, 5.5, 0),
        'game': [('game_cam', 37.5, 200.0, 50.0, 0.8)],
    },
    'supply_drop_crate': {
        'parts': [('SM_SupplyDrop_Crate', (0, 0, 0))],
        'views': [('top', 'ortho', (0, 0, -1), (-1, 0, 0), (900, 900)),
                  ('three_quarter', 'persp', 30.0, 215.0, (1200, 900), 35.0, 0.9),
                  ('three_quarter_right', 'persp', 30.0, 145.0, (1200, 900), 35.0, 0.9)],
        'usa_at': (0.0, 3.5, 0),
        'game': [('game_cam_close', 37.5, 200.0, 28.0, 0.75)],
    },
    'cargo_plane': {
        'parts': [('SM_USA_CargoPlane', (0, 0, 8.0))],
        'views': [('top', 'ortho', (0, 0, -1), (-1, 0, 0), (1400, 1100)),
                  ('front', 'ortho', (-1, 0, 0), (0, 0, 1), (1600, 600)),
                  ('side', 'ortho', (0, 1, 0), (0, 0, 1), (1600, 600)),
                  ('three_quarter', 'persp', 30.0, 215.0, (1600, 1000), 35.0, 0.95),
                  ('back_three_quarter', 'persp', 25.0, 35.0, (1600, 1000), 35.0, 0.95)],
        'usa_at': (0.0, 0.0, 0),
        'game': [('game_cam', 37.5, 200.0, 50.0, 0.85)],
    },
    'gla_war_factory': {
        'parts': [('gla_war_factory', (0, 0, 0))],
        'views': [('top', 'ortho', (0, 0, -1), (-1, 0, 0), (1500, 1200)),           # a planta: frente embaixo, direita (+Y) à esquerda
                  ('front', 'ortho', (-1, 0, 0), (0, 0, 1), (1800, 700)),
                  ('side', 'ortho', (0, 1, 0), (0, 0, 1), (1800, 700)),              # a face da direita (+Y Unreal)
                  ('three_quarter', 'persp', 30.0, 145.0, (1600, 900), 32.0, 0.95),  # pela frente-direita, como a ficha
                  ('back_three_quarter', 'persp', 30.0, 35.0, (1600, 900), 32.0, 0.95)],
        'usa_at': (2.0, 0.0, 0),                                                  # o tanque saindo da rampa
        'game': [('game_cam', 37.5, 20.0, 50.0, 0.9), ('game_cam_close', 37.5, 20.0, 30.0, 0.8)],
        'scaffold': 'gla_war_factory_scaffold',
    },
    # as aeronaves da USA e o Airfield (24/10, model_usa_raptor/comanche/chinook/airfield.py): o tanque USA entra também
    # nas vistas ortográficas (tank_views), na frente e ao lado, pra régua; peça com 3 valores = (malha, posição, giro em graus)
    'usa_raptor': {
        'parts': [('SM_USA_Raptor_Body', (0, 0, 0)), ('SM_USA_Raptor_Gear', (0, 0, 0)), ('SM_USA_Raptor_Missiles', (0, 0, 0))],
        'views': [('top', 'ortho', (0, 0, -1), (1, 0, 0), (1200, 1400)),
                  ('front', 'ortho', (-1, 0, 0), (0, 0, 1), (1400, 600)),
                  ('side', 'ortho', (0, 1, 0), (0, 0, 1), (1600, 500)),
                  ('three_quarter', 'persp', 30.0, 215.0, (1600, 1000), 35.0, 0.95)],
        'usa_at': (11.0, -6.0, 0), 'tank_views': True,
        'game': [('game_cam', 37.5, 200.0, 50.0, 0.85)],
    },
    'usa_comanche': {
        'parts': [('SM_USA_Comanche_Body', (0, 0, 0)), ('SM_USA_Comanche_Rotor', (0, 0, 2.146)),
                  ('SM_USA_Comanche_TailFan', (-5.8, -0.197, 2.059)), ('SM_USA_Comanche_Pods', (0, 0, 0)),
                  ('SM_USA_Comanche_Missiles', (0, 0, 0))],
        'views': [('top', 'ortho', (0, 0, -1), (1, 0, 0), (1200, 1400)),
                  ('front', 'ortho', (-1, 0, 0), (0, 0, 1), (1400, 600)),
                  ('side', 'ortho', (0, 1, 0), (0, 0, 1), (1600, 500)),
                  ('three_quarter', 'persp', 30.0, 215.0, (1600, 1000), 35.0, 0.95)],
        'usa_at': (9.0, -5.0, 0), 'tank_views': True,
        'game': [('game_cam', 37.5, 200.0, 50.0, 0.85)],
    },
    'usa_chinook': {
        'parts': [('SM_USA_Chinook_Body', (0, 0, 0)), ('SM_USA_Chinook_RotorFront', (3.463, 0, 2.697)),
                  ('SM_USA_Chinook_RotorRear', (-3.463, 0, 3.277)), ('SM_USA_Chinook_Ramp', (-2.668, 0, 0.435))],
        'views': [('top', 'ortho', (0, 0, -1), (1, 0, 0), (1200, 1400)),
                  ('front', 'ortho', (-1, 0, 0), (0, 0, 1), (1400, 600)),
                  ('side', 'ortho', (0, 1, 0), (0, 0, 1), (1600, 500)),
                  ('three_quarter', 'persp', 30.0, 215.0, (1600, 1000), 35.0, 0.95),
                  ('back_three_quarter', 'persp', 25.0, 35.0, (1600, 1000), 35.0, 0.95)],
        'usa_at': (13.5, -5.5, 0), 'tank_views': True,
        'game': [('game_cam', 37.5, 200.0, 50.0, 0.85)],
    },
    'usa_airfield': {
        'parts': [('SM_USA_Airfield_Base', (0, 0, 0)), ('SM_USA_Airfield_Radar', (12.6, -12.2, 12.45))]
                 + [('SM_USA_Airfield_Pad', (x, -1.5, 0)) for x in (-14.4, -4.8, 4.8, 14.4)]
                 + [('captures/game/models/usa_raptor/SM_USA_Raptor_' + n, (x, -1.5, 0), -90.0) for n in ('Body', 'Gear', 'Missiles') for x in (-14.4, 4.8)]
                 + [('captures/game/models/usa_comanche/SM_USA_Comanche_' + n, (-12.0, -11.8, 0)) for n in ('Body', 'Pods', 'Missiles')]
                 + [('captures/game/models/usa_comanche/SM_USA_Comanche_Rotor', (-12.0, -11.8, 2.146)),
                    ('captures/game/models/usa_comanche/SM_USA_Comanche_TailFan', (-17.8, -11.603, 2.059))],
        'views': [('top', 'ortho', (0, 0, -1), (-1, 0, 0), (1400, 1400)),
                  ('front', 'ortho', (-1, 0, 0), (0, 0, 1), (1600, 700)),
                  ('three_quarter', 'persp', 34.0, 215.0, (1600, 1000), 30.0, 0.95),
                  ('back_three_quarter', 'persp', 32.0, 35.0, (1600, 1000), 30.0, 0.95)],
        'usa_at': (5.0, -4.0, 0), 'tank_views': True,
        'game': [('game_cam', 37.5, 200.0, 50.0, 0.85), ('game_cam_close', 37.5, 200.0, 30.0, 0.8)],
        'scaffold': 'SM_USA_Airfield_Scaffold',
    },
}


def load(path, at, yaw=0.0):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    for o in new:
        if o.parent is None:
            o.location = Vector(at) + o.location
            if yaw:
                o.rotation_euler.z += math.radians(-yaw)          # o giro da Unreal (o Blender tem o Y ao contrário)
    return new


EMISSIVE = {'Beacon': ((1.0, 0.55, 0.1), 12.0), 'Light': ((1.0, 0.85, 0.55), 20.0),
            # (31/10, o elenco) o forno aceso da China e o núcleo do reator
            'Fire': ((1.0, 0.42, 0.08), 18.0), 'Core': ((1.0, 0.92, 0.8), 14.0)}


def glb_base_colors():
    """(31/10) A cor de cada material lida nos GLB da pasta (o baseColorFactor): com a cor de vértice (as máscaras do model_detail.py)
    o importador do glTF multiplica a máscara na cor, e a folha saía tingida."""
    import glob
    import struct
    out = {}
    for f in glob.glob(os.path.join(SRC, '*.glb')):
        b = open(f, 'rb').read()
        n = struct.unpack('<I', b[12:16])[0]
        j = json.loads(b[20:20 + n])
        for mm in j.get('materials', []):
            out[mm.get('name', '')] = tuple(mm.get('pbrMetallicRoughness', {}).get('baseColorFactor', (1, 1, 1, 1)))
    return out


def fix_materials():
    cols = glb_base_colors()
    for m in bpy.data.materials:
        base = m.name.split('.')[0]
        bsdf = next((n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None) if m.node_tree else None
        if bsdf and base in cols and bsdf.inputs['Base Color'].is_linked:
            for l in list(bsdf.inputs['Base Color'].links):
                m.node_tree.links.remove(l)
            bsdf.inputs['Base Color'].default_value = cols[base]
    for m in bpy.data.materials:
        base = m.name.split('.')[0]
        if base in EMISSIVE and m.node_tree:
            (r, g, b), strength = EMISSIVE[base]
            bsdf = next((n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
            if bsdf:
                bsdf.inputs['Emission Color'].default_value = (r, g, b, 1)
                bsdf.inputs['Emission Strength'].default_value = strength


# (31/10, o detalhe AAA) PREVIEW_MASKS=raw mostra as máscaras do model_detail.py (a cor de vértice: R = 1 − quina, G = oclusão,
# B = 1 − fresta); PREVIEW_MASKS=game aproxima o material do jogo (o desgaste clareia a quina, o pó na fresta, a oclusão escurece um
# pouco) pra conferir sem a Unreal. Sem a variável, nada muda
MASKS = os.environ.get('PREVIEW_MASKS', '')


def mask_materials():
    for m in bpy.data.materials:
        if not m.node_tree:
            continue
        bsdf = next((n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
        if not bsdf:
            continue
        nt = m.node_tree
        ca = nt.nodes.new('ShaderNodeVertexColor')
        ca.layer_name = ''   # o atributo de cor ativo (o importador do glTF o chama de 'Color')
        if MASKS == 'raw':
            nt.links.new(ca.outputs['Color'], bsdf.inputs['Base Color'])
            bsdf.inputs['Metallic'].default_value = 0.0
            continue
        base = bsdf.inputs['Base Color'].default_value[:]
        sep = nt.nodes.new('ShaderNodeSeparateColor')
        camo = None
        if m.name.split('.')[0] == 'CnTankPaint':
            # a camuflagem digital do material (o Camo do jogo): células de 16 cm no espaço da peça, o ruído largo lido na célula
            tc = nt.nodes.new('ShaderNodeTexCoord')
            snap = nt.nodes.new('ShaderNodeVectorMath'); snap.operation = 'SNAP'
            snap.inputs[1].default_value = (0.16, 0.16, 0.16)
            nt.links.new(tc.outputs['Object'], snap.inputs[0])
            nz = nt.nodes.new('ShaderNodeTexNoise'); nz.inputs['Scale'].default_value = 0.9; nz.inputs['Detail'].default_value = 2.0
            nt.links.new(snap.outputs['Vector'], nz.inputs['Vector'])
            gt = nt.nodes.new('ShaderNodeMath'); gt.operation = 'GREATER_THAN'; gt.inputs[1].default_value = 0.53
            nt.links.new(nz.outputs['Fac'], gt.inputs[0])
            camo = nt.nodes.new('ShaderNodeMix'); camo.data_type = 'RGBA'
            camo.inputs['A'].default_value = base
            camo.inputs['B'].default_value = (0.040, 0.048, 0.028, 1.0)
            nt.links.new(gt.outputs[0], camo.inputs['Factor'])
        nt.links.new(ca.outputs['Color'], sep.inputs['Color'])
        # o desgaste: 1 − R, clareia 1,65× (+0,035); o pó: 1 − B, a cor da duna; a oclusão: G, 35% na cor (o pino de AO não existe aqui)
        wear = nt.nodes.new('ShaderNodeMath'); wear.operation = 'SUBTRACT'; wear.inputs[0].default_value = 1.0
        nt.links.new(sep.outputs['Red'], wear.inputs[1])
        cav = nt.nodes.new('ShaderNodeMath'); cav.operation = 'SUBTRACT'; cav.inputs[0].default_value = 1.0
        nt.links.new(sep.outputs['Blue'], cav.inputs[1])
        m1 = nt.nodes.new('ShaderNodeMix'); m1.data_type = 'RGBA'
        m1.inputs['A'].default_value = base
        if camo:
            nt.links.new(camo.outputs['Result'], m1.inputs['A'])
        m1.inputs['B'].default_value = tuple(min(c * 1.65 + 0.035, 0.6) for c in base[:3]) + (1.0,)
        wf = nt.nodes.new('ShaderNodeMath'); wf.operation = 'MULTIPLY'; wf.inputs[1].default_value = 0.6
        nt.links.new(wear.outputs[0], wf.inputs[0])
        nt.links.new(wf.outputs[0], m1.inputs['Factor'])
        m2 = nt.nodes.new('ShaderNodeMix'); m2.data_type = 'RGBA'
        nt.links.new(m1.outputs['Result'], m2.inputs['A'])
        m2.inputs['B'].default_value = (0.40, 0.31, 0.21, 1.0)
        cf = nt.nodes.new('ShaderNodeMath'); cf.operation = 'MULTIPLY'; cf.inputs[1].default_value = 0.6
        nt.links.new(cav.outputs[0], cf.inputs[0])
        nt.links.new(cf.outputs[0], m2.inputs['Factor'])
        ao = nt.nodes.new('ShaderNodeMix'); ao.data_type = 'RGBA'; ao.blend_type = 'MULTIPLY'
        ao.inputs['Factor'].default_value = 0.35
        nt.links.new(m2.outputs['Result'], ao.inputs['A'])
        gg = nt.nodes.new('ShaderNodeCombineColor')
        for k in ('Red', 'Green', 'Blue'):
            nt.links.new(sep.outputs['Green'], gg.inputs[k])
        nt.links.new(gg.outputs['Color'], ao.inputs['B'])
        nt.links.new(ao.outputs['Result'], bsdf.inputs['Base Color'])


def scene_setup():
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'
    sc.cycles.device = 'CPU'
    sc.cycles.samples = 64
    sc.cycles.use_denoising = True
    sc.view_settings.view_transform = 'AgX'
    w = bpy.data.worlds.new('w')
    sc.world = w
    w.use_nodes = True
    bg = w.node_tree.nodes['Background']
    bg.inputs['Color'].default_value = (0.45, 0.45, 0.46, 1)
    bg.inputs['Strength'].default_value = 1.0
    sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN'))
    sun.data.energy = 3.0
    sun.data.angle = math.radians(3.0)
    sc.collection.objects.link(sun)
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    sc.collection.objects.link(cam)
    sc.camera = cam
    return cam, sun, bg


def bounds(objs):
    pts = [o.matrix_world @ Vector(c) for o in objs if o.type == 'MESH' for c in o.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return lo, hi


def look(cam, center, direction, up, dist):
    """A câmera a 'dist' do centro, olhando ao longo de 'direction' (Blender), com 'up' pra cima na imagem."""
    d = Vector(direction).normalized()
    cam.location = Vector(center) - d * dist
    q = d.to_track_quat('-Z', 'Y')
    cam_up = q @ Vector((0, 1, 0))
    want = Vector(up) - d * Vector(up).dot(d)
    if want.length > 1e-6:
        want.normalize()
        ang = cam_up.angle(want)
        if cam_up.cross(want).dot(d) < 0:
            ang = -ang
        q = Quaternion(d, ang) @ q
    cam.rotation_euler = q.to_euler()


def ortho(cam, objs, direction, up, res, margin=1.12, floor=True):
    lo, hi = bounds(objs)
    if floor:
        lo.z = max(lo.z, 0.0)          # a saia do lote (abaixo do chão) não entra na vista
    center = (lo + hi) / 2
    cam.data.type = 'ORTHO'
    cam.data.sensor_fit = 'HORIZONTAL'
    d = Vector(direction).normalized()
    u = Vector(up) - d * Vector(up).dot(d)
    u.normalize()
    r = d.cross(u)
    pts = [Vector((x, y, z)) for x in (lo.x, hi.x) for y in (lo.y, hi.y) for z in (lo.z, hi.z)]
    w = max(p.dot(r) for p in pts) - min(p.dot(r) for p in pts)
    h = max(p.dot(u) for p in pts) - min(p.dot(u) for p in pts)
    cam.data.ortho_scale = max(w, h * res[0] / res[1]) * margin
    cam.data.clip_end = 500
    bpy.context.scene.render.resolution_x, bpy.context.scene.render.resolution_y = res
    look(cam, center, direction, up, 120.0)


def persp(cam, objs, pitch_deg, yaw_deg, res, fov=50.0, margin=0.9):
    lo, hi = bounds(objs)
    lo.z = max(lo.z, 0)
    center = (lo + hi) / 2
    radius = (hi - lo).length / 2
    cam.data.type = 'PERSP'
    cam.data.sensor_fit = 'HORIZONTAL'
    cam.data.angle = math.radians(fov)
    cam.data.clip_end = 1000
    f = min(cam.data.angle, cam.data.angle * res[1] / res[0])
    dist = radius * margin / math.sin(f / 2)
    p, y = math.radians(pitch_deg), math.radians(yaw_deg)
    d = Vector((math.cos(p) * math.cos(y), math.cos(p) * math.sin(y), -math.sin(p)))     # pra onde a câmera olha
    bpy.context.scene.render.resolution_x, bpy.context.scene.render.resolution_y = res
    look(cam, center, d, (0, 0, 1), dist)


def render(name):
    bpy.context.scene.render.filepath = os.path.join(OUT, name + ('_masks_' + MASKS if MASKS else '') + '.png')
    bpy.ops.render.render(write_still=True)
    print('PREVIEW', name)


def ground(bg, sun):
    bpy.ops.mesh.primitive_plane_add(size=600, location=(0, 0, 0))
    g = bpy.context.active_object
    gm = bpy.data.materials.new('ground')
    gm.use_nodes = True
    gb = gm.node_tree.nodes['Principled BSDF']
    gb.inputs['Base Color'].default_value = (0.42, 0.33, 0.21, 1)
    gb.inputs['Roughness'].default_value = 0.95
    g.data.materials.append(gm)
    bg.inputs['Color'].default_value = (0.36, 0.48, 0.66, 1)
    sun.data.energy = 4.5
    sun.rotation_euler = (math.radians(50), 0, math.radians(-35))
    return g


spec = MODELS.get(MODEL)
if spec is None:
    # (31/10, o elenco das 3 facções: docs/ELENCO.md) o modelo novo traz a própria folha num preview.json na pasta dos GLB, no
    # mesmo formato das entradas acima (listas no lugar das tuplas), pra vários modelos serem feitos ao mesmo tempo sem mexer aqui
    import json
    spec = json.load(open(os.path.join(SRC, os.environ.get('PREVIEW_SPEC', 'preview.json')), encoding='utf-8'))   # (31/10) PREVIEW_SPEC: outra folha
    spec['parts'] = [tuple(p) for p in spec['parts']]
    spec['views'] = [tuple(v) for v in spec['views']]
    spec['game'] = [tuple(g) for g in spec['game']]
cam, sun, bg = scene_setup()
# o cubo da cena de fábrica (--factory-startup) sai: aparecia como um pedestal branco embaixo dos veículos nas 3/4
if 'Cube' in bpy.data.objects:
    bpy.data.objects.remove(bpy.data.objects['Cube'], do_unlink=True)
objs = []
REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
for item in spec['parts']:
    name, at = item[0], item[1]
    path = os.path.join(REPO, name + '.glb') if '/' in name else os.path.join(SRC, name + '.glb')
    objs += load(path, at, item[2] if len(item) > 2 else 0.0)
if spec.get('layout'):
    import json
    lay = json.load(open(os.path.join(SRC, spec['layout']), encoding='utf-8'))
    for key, mesh in (('heap_m', lay['crate']), ('empties_m', lay.get('crate_empty'))):
        if not mesh:
            continue
        src_objs = load(os.path.join(SRC, mesh + '.glb'), (0, 0, 0))
        src = [o for o in src_objs if o.type == 'MESH'][0]
        for (x, y, z, yaw) in lay.get(key, []):
            c = src.copy()
            c.data = src.data
            bpy.context.scene.collection.objects.link(c)
            c.location = Vector((x, -y, z))
            c.rotation_euler = (0.0, 0.0, math.radians(-yaw))
            objs.append(c)
        for o in src_objs:
            bpy.data.objects.remove(o, do_unlink=True)
            objs = [q for q in objs if q is not o]
fix_materials()
if MASKS:
    mask_materials()
bpy.context.view_layer.update()
lo, hi = bounds(objs)
print(f'MEASURE {MODEL}: comprimento {hi.x - lo.x:.2f} m (x {lo.x:.2f}..{hi.x:.2f}), largura {hi.y - lo.y:.2f} m, altura {hi.z:.2f} m')



def load_tank():
    tank_objs = []
    for n in USA_PARTS:
        tank_objs += load(os.path.join(TANK, n + '.glb'), (0, 0, 0))
    bpy.context.view_layer.update()
    tlo, thi = bounds(tank_objs)
    print(f'MEASURE tanque USA: comprimento {thi.x - tlo.x:.2f} m (casco {thi.x - tlo.x - 1.6:.2f} sem o cano), largura {thi.y - tlo.y:.2f} m, altura {thi.z:.2f} m')
    for o in tank_objs:
        if o.parent is None:
            o.location = o.location + Vector((spec['usa_at'][0], -spec['usa_at'][1], spec['usa_at'][2]))
    bpy.context.view_layer.update()               # a caixa das vistas usa a matriz do mundo: atualizar depois de mover
    return tank_objs


tank_done = False
if TANK and spec.get('tank_views'):
    objs += load_tank()
    tank_done = True

# luz da ficha: de cima, da frente e um pouco da esquerda
sun.rotation_euler = (math.radians(35), math.radians(20), math.radians(-150))
# (31/10) PREVIEW_ONLY=three_quarter,game_cam: só essas vistas (a conferência rápida)
ONLY = [s for s in os.environ.get('PREVIEW_ONLY', '').split(',') if s]
for view in spec['views']:
    tag, kind = view[0], view[1]
    if ONLY and tag not in ONLY:
        continue
    if kind == 'ortho':
        ortho(cam, objs, view[2], view[3], view[4])
    else:
        persp(cam, objs, view[2], view[3], view[4], fov=view[5], margin=view[6])
    render(tag)

# a câmera do jogo no chão de areia, com o tanque USA como régua
ground(bg, sun)
if TANK and not tank_done:
    objs += load_tank()
for tag, pitch, yaw, fov, margin in spec['game']:
    if ONLY and tag not in ONLY:
        continue
    persp(cam, objs, pitch, yaw, (1600, 900), fov=fov, margin=margin)
    render(tag)
# o prédio em obra: o andaime por cima, na câmera do jogo
if spec.get('scaffold') and os.path.exists(os.path.join(SRC, spec['scaffold'] + '.glb')) and not ONLY:
    objs += load(os.path.join(SRC, spec['scaffold'] + '.glb'), (0, 0, 0))
    tag, pitch, yaw, fov, margin = spec['game'][0]
    persp(cam, objs, pitch, yaw, (1600, 900), fov=fov, margin=margin)
    render('game_cam_scaffold')
print('PREVIEW fim')
