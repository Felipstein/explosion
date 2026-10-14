"""Importa o tanque USA provisório da maquete do look (Game/Tools/look_tank.py) pro BlastLab, pra comparar o
tamanho das explosões com uma unidade de verdade (pedido do Felipe, 12/10).

Uso: blast.ps1 py Tools\\import_look_tank.py
Lê captures/look/tank_glb/SM_LL_USATank_*.glb (peças com a origem do tanque, no chão e no centro; casco de
7,9 m) e cria /Game/Units/LL_USATank/<peça>. O jogo põe o tanque na cena com -Tank=x,y (BlastLabController).
"""
import glob
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ue_util import REPO  # noqa: E402

DEST = '/Game/Units/LL_USATank'


def main():
    files = sorted(glob.glob(os.path.join(REPO, 'captures', 'look', 'tank_glb', 'SM_LL_USATank_*.glb')))
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for f in files:
        name = os.path.splitext(os.path.basename(f))[0]
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', f)
        task.set_editor_property('destination_path', DEST)
        task.set_editor_property('destination_name', name)
        task.set_editor_property('automated', True)
        task.set_editor_property('replace_existing', True)
        task.set_editor_property('save', True)
        tools.import_asset_tasks([task])
        for p in task.get_editor_property('imported_object_paths') or []:
            m = unreal.load_asset(p)
            if isinstance(m, unreal.StaticMesh):
                b = m.get_bounding_box()
                unreal.log('LOOKTANK %s %s  %.2f x %.2f x %.2f m' % (name, p, (b.max.x - b.min.x) / 100, (b.max.y - b.min.y) / 100, (b.max.z - b.min.z) / 100))


main()
