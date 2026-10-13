"""Qual maquete: a planta da cena escolhida (LOOK_SCENE), lida por todos os scripts do look.

  LOOK_SCENE=desert_golden_hour (padrão)  -> Tools/look_layout.py
  LOOK_SCENE=desert_night                 -> Tools/look_layout_night.py
O nome da cena é o da pasta das artes (docs/concept/scenes/<cena>/) e o da receita de luz
(Tools/look_recipes/<cena>.json).
"""
import importlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
SCENE = os.environ.get('LOOK_SCENE', 'desert_golden_hour')
_MODULES = {'desert_golden_hour': 'look_layout', 'desert_night': 'look_layout_night',
            'desert_ground': 'look_layout_ground',     # a cena de perto contra a close_ground_light_gemini
            'civil_kit': 'look_layout_civil'}          # o kit civil da vila pro mapa jogável (só as malhas)
_m = importlib.import_module(_MODULES[SCENE])
globals().update({k: v for k, v in vars(_m).items() if not k.startswith('__')})
SCENE = os.environ.get('LOOK_SCENE', 'desert_golden_hour')
# as pastas de saída (o deserto dourado ficou na raiz de captures/look, a primeira maquete)
SUB = '' if SCENE == 'desert_golden_hour' else SCENE
# o sufixo dos assets da Unreal que dependem da cena (chão, planta pintada, material do chão)
SUFFIX = '' if SCENE == 'desert_golden_hour' else '_' + SCENE
