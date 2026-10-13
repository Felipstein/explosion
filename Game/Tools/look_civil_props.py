"""O kit civil da vila: junta as malhas (casas do look_houses.py, carros e miudezas do look_tank.py) com os nomes
e a convenção do WorldForge (docs/WORLDFORGE.md §12): SM_civ_<tipo>.glb, a frente em +Y no giro 0, a origem no
centro da planta e no chão.

Rodar (Blender sem janela; o look.ps1 civil chama):
  blender -b --factory-startup --python Game/Tools/look_civil_props.py -- <repo>

Os carros e as miudezas foram feitos com a frente em +X (o padrão do tanque): giram 90° pra frente ir pra +Y.
As casas, os muros e o poste já saem no padrão (Tools/look_layout_civil.py) e só mudam de nome.
"""
import math
import os
import shutil
import sys

import bpy
from mathutils import Matrix, Vector

REPO = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.getcwd()
CAP = os.path.join(REPO, 'captures', 'look')
OUT = os.path.join(CAP, 'civil_glb')
os.makedirs(OUT, exist_ok=True)

HOUSES = ['civ_house_1story', 'civ_house_2story', 'civ_house_long', 'civ_house_porch', 'civ_wall', 'civ_wall_gate', 'civ_pole']
# (origem, tipo, giro em graus na Unreal pra frente ir de +X pra +Y)
PROPS = [('SM_LL_PickupCivil', 'civ_pickup', 90.0), ('SM_LL_Wreck', 'civ_car_burnt', 90.0),
         ('SM_LL_Prop_Crates', 'civ_crates', 90.0), ('SM_LL_Prop_Barrels', 'civ_barrels', 0.0),
         ('SM_LL_Prop_Sandbags', 'civ_sandbags', 0.0), ('SM_LL_Prop_Ammo', 'civ_ammo', 0.0)]


def clear():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)


def convert(src, kind, yaw):
    clear()
    bpy.ops.import_scene.gltf(filepath=src)
    objs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    for o in bpy.context.view_layer.objects:
        o.select_set(o in objs)
    bpy.context.view_layer.objects.active = objs[0]
    if len(objs) > 1:
        bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    # o giro na Unreal (Z pra cima, Y do Blender invertido): +90° na Unreal = −90° no Blender
    o.data.transform(Matrix.Rotation(math.radians(-yaw), 4, 'Z'))
    # a origem no centro da planta (X/Y) e no chão (o ponto mais baixo)
    vs = [v.co for v in o.data.vertices]
    cx = (min(v.x for v in vs) + max(v.x for v in vs)) / 2
    cy = (min(v.y for v in vs) + max(v.y for v in vs)) / 2
    z0 = min(v.z for v in vs)
    o.data.transform(Matrix.Translation(Vector((-cx, -cy, -z0))))
    o.name = 'SM_' + kind
    path = os.path.join(OUT, f'SM_{kind}.glb')
    bpy.ops.object.select_all(action='DESELECT')
    o.select_set(True)
    bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_selection=True, export_apply=True,
                              export_yup=True, export_materials='EXPORT', export_normals=True, export_texcoords=False)
    xs = [v.co.x for v in o.data.vertices]
    ys = [-v.co.y for v in o.data.vertices]
    print('LOOKCIVIL', kind, f'X {min(xs):.2f}..{max(xs):.2f} Y {min(ys):.2f}..{max(ys):.2f}', '->', path)


def main():
    for k in HOUSES:
        src = os.path.join(CAP, 'civil_kit', 'houses_glb', f'SM_LL_{k}.glb')
        if os.path.exists(src):
            shutil.copyfile(src, os.path.join(OUT, f'SM_{k}.glb'))
            print('LOOKCIVIL', k, '(casa/muro/poste, só o nome)')
    for name, kind, yaw in PROPS:
        src = os.path.join(CAP, 'tank_glb', name + '.glb')
        if os.path.exists(src):
            convert(src, kind, yaw)
        else:
            print('LOOKCIVIL falta', src)


main()
