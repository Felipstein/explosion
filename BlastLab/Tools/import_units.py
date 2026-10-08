"""Importa as peças de unidade geradas por IA (GLB) e mede cada uma.

Uso: blast.ps1 py Tools\\import_units.py
Lê captures/concept/30_to_3d/<unidade>/<subpasta>/*.glb e cria
/Game/Units/<Unidade>/SM_<peça> com Nanite ligado. No fim imprime o
tamanho de cada peça (em metros, eixos da Unreal: X frente, Y lado, Z cima),
que é o que a montagem usa pra escalar e encaixar por medida.
"""
import glob
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ue_util import REPO  # noqa: E402

UNITS = {
    # pasta em captures/concept/30_to_3d  →  (subpasta das peças, pasta no Content)
    # parts_v2: o tanque montado "na mão" (blender_export_unit.py), já no lugar
    'usa_tank': ('parts_v2', '/Game/Units/USA_MBT'),
}


def import_glb(path, dest, name):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', path)
    task.set_editor_property('destination_path', dest)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return list(task.get_editor_property('imported_object_paths') or [])


def enable_nanite(mesh):
    s = mesh.get_editor_property('nanite_settings')
    if not s.get_editor_property('enabled'):
        s.set_editor_property('enabled', True)
        mesh.set_editor_property('nanite_settings', s)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)


def size_m(mesh):
    b = mesh.get_bounding_box()
    mn, mx = b.min, b.max
    return (mx.x - mn.x) / 100, (mx.y - mn.y) / 100, (mx.z - mn.z) / 100, mn, mx


def main():
    root = os.path.join(REPO, 'captures', 'concept', '30_to_3d')
    for folder, (sub, dest) in UNITS.items():
        files = sorted(glob.glob(os.path.join(root, folder, sub, '*.glb')))
        for f in files:
            part = os.path.splitext(os.path.basename(f))[0]
            name = part if part.startswith('SM_') else 'SM_' + part
            paths = import_glb(f, dest, name)
            meshes = [unreal.load_asset(p) for p in paths]
            meshes = [m for m in meshes if isinstance(m, unreal.StaticMesh)]
            if not meshes:
                unreal.log_error(f'{folder}/{part}: nenhuma malha importada ({paths})')
                continue
            for m in meshes:
                enable_nanite(m)
                sx, sy, sz, mn, mx = size_m(m)
                tris = m.get_num_triangles(0) if hasattr(m, 'get_num_triangles') else -1
                unreal.log(f'UNIT {folder}/{part}: {m.get_name()} tamanho X={sx:.3f} Y={sy:.3f} Z={sz:.3f} m '
                           f'min=({mn.x:.1f},{mn.y:.1f},{mn.z:.1f}) max=({mx.x:.1f},{mx.y:.1f},{mx.z:.1f}) cm tris={tris}')
            others = [p for p in paths if not isinstance(unreal.load_asset(p), unreal.StaticMesh)]
            unreal.log(f'UNIT {folder}/{part}: também importou {len(others)} assets (materiais/texturas)')


main()
# as instâncias do importador glTF não servem pro Nanite (pai do plugin sem a marca de uso):
# troca pelo M_UnitPBR (fix_unit_materials.py)
with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fix_unit_materials.py'), encoding='utf-8') as fh:
    exec(fh.read())
