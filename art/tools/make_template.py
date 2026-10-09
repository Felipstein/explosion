"""Gera o modelo-base do Blender pra modelar peças do jogo (docs/PRODUCAO_3D.md §9).

Uso (sem abrir o Blender):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python art/tools/make_template.py

Sai em captures/art3d/RTS_Base.blend (fora do git: leva o tanque, ~20 MB) e uma prévia
captures/art3d/RTS_Base_preview.png renderizada da câmera do jogo (Cycles na CPU).
Mude este script, não o arquivo.

O que vem na cena:
- a grade da simulação: quadrados de 4 m (Game/Config/RTS/Maps.json, cell_m), no chão Z = 0;
- o tanque USA com 7,9 m (as peças de captures/concept/30_to_3d/usa_tank/parts_v2), frente pra −Y;
- um humano de 1,8 m;
- a planta do 1º modelo: caixa 8 × 8 × 4 m (a casa de adobe, §10), só em arame;
- a câmera do jogo (Game/Config/DefaultGame.ini: 37,5° de inclinação, campo horizontal de 50°,
  61,1 m de altura) e a de zoom perto (31,6 m), olhando a planta na diagonal como no jogo;
- o sol do deserto do jogo (20° de altura) e um céu claro.
"""
import math
import os

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
TANK = os.path.join(REPO, 'captures', 'concept', '30_to_3d', 'usa_tank', 'parts_v2')
OUT = os.path.join(REPO, 'captures', 'art3d')
CELL_M = 4.0
TANK_LEN_M = 7.9
PITCH_DEG, FOV_DEG = 37.5, 50.0
HEIGHTS = {'Camera_Jogo': 61.1, 'Camera_Perto': 31.6}
SUN_ELEV_DEG = 20.0


def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    s = bpy.context.scene
    s.unit_settings.system = 'METRIC'
    s.unit_settings.scale_length = 1.0
    s.render.resolution_x, s.render.resolution_y = 1920, 1080


def collection(name):
    c = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(c)
    return c


def link(ob, col):
    for c in ob.users_collection:
        c.objects.unlink(ob)
    col.objects.link(ob)


def material(name, rgb, rough=0.9):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*rgb, 1.0)
    b.inputs['Roughness'].default_value = rough
    return m


def grid(col):
    # 200 m de lado em quadrados de 4 m: só as linhas (arame), mais um chão de areia logo abaixo
    n = int(200 / CELL_M)
    bpy.ops.mesh.primitive_grid_add(x_subdivisions=n, y_subdivisions=n, size=200.0, location=(0, 0, 0.005))
    g = bpy.context.active_object
    g.name = 'Grade_4m'
    g.display_type = 'WIRE'
    g.hide_render = True
    link(g, col)
    bpy.ops.mesh.primitive_plane_add(size=400.0, location=(0, 0, 0))
    floor = bpy.context.active_object
    floor.name = 'Chao_Areia'
    floor.data.materials.append(material('M_Areia', (0.35, 0.21, 0.11)))   # albedo da areia do deserto do jogo
    floor.hide_select = True
    link(floor, col)


def tank(col):
    before = set(bpy.data.objects)
    for f in sorted(os.listdir(TANK)):
        if f.endswith('.glb'):
            bpy.ops.import_scene.gltf(filepath=os.path.join(TANK, f))
    parts = [o for o in bpy.data.objects if o not in before and o.type == 'MESH']
    for o in [o for o in bpy.data.objects if o not in before]:
        link(o, col)
    # caixa do conjunto: escala pra 7,9 m no lado comprido, no chão, centrado
    bpy.context.view_layer.update()
    pts = [o.matrix_world @ Vector(c) for o in parts for c in o.bound_box]
    mn = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    mx = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    size = mx - mn
    root = bpy.data.objects.new('Tanque_USA_7.9m', None)
    col.objects.link(root)
    root.location = ((mn.x + mx.x) / 2, (mn.y + mx.y) / 2, mn.z)
    bpy.context.view_layer.update()
    for o in [o for o in bpy.data.objects if o not in before and o.parent is None and o is not root]:
        o.parent = root
        o.matrix_parent_inverse = root.matrix_world.inverted()
    s = TANK_LEN_M / max(size.x, size.y)
    root.scale = (s, s, s)
    # o cano: de que lado do centro ele está → gira pra frente (−Y)
    barrel = [o for o in parts if 'arrel' in o.name]
    if barrel:
        bc = sum((barrel[0].matrix_world @ Vector(c) for c in barrel[0].bound_box), Vector()) / 8
        d = Vector((bc.x - root.location.x, bc.y - root.location.y, 0))
        root.rotation_euler[2] = math.atan2(-1, 0) - math.atan2(d.y, d.x)
    root.location = (12.0, 0.0, 0.0)          # ao lado da planta, sem encostar
    for o in parts:
        o.hide_select = True


def human(col):
    bpy.ops.mesh.primitive_cylinder_add(radius=0.25, depth=1.8, location=(6.0, -6.0, 0.9))
    h = bpy.context.active_object
    h.name = 'Humano_1.8m'
    h.data.materials.append(material('M_Humano', (0.6, 0.15, 0.1)))
    link(h, col)


def footprint(col):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0, 2.0))
    b = bpy.context.active_object
    b.name = 'Planta_8x8x4m'
    b.scale = (8.0, 8.0, 4.0)
    b.display_type = 'WIRE'
    b.hide_render = True
    link(b, col)


def cameras(col):
    target = bpy.data.objects.new('Alvo_Camera', None)
    col.objects.link(target)
    target.location = (4.0, 0.0, 0.0)          # entre a planta e o tanque
    p = math.radians(PITCH_DEG)
    az = math.radians(225.0)                   # na diagonal, do lado da frente (−Y), como no jogo
    for name, h in HEIGHTS.items():
        cam = bpy.data.cameras.new(name)
        cam.sensor_fit = 'HORIZONTAL'
        cam.angle = math.radians(FOV_DEG)
        cam.clip_end = 2000.0
        ob = bpy.data.objects.new(name, cam)
        col.objects.link(ob)
        d = h / math.sin(p)
        ob.location = target.location + Vector((math.cos(az) * math.cos(p), math.sin(az) * math.cos(p), math.sin(p))) * d
        t = ob.constraints.new('TRACK_TO')
        t.target = target
        t.track_axis = 'TRACK_NEGATIVE_Z'
        t.up_axis = 'UP_Y'
    bpy.context.scene.camera = bpy.data.objects['Camera_Jogo']


def light(col):
    sun = bpy.data.lights.new('Sol_20graus', 'SUN')
    sun.energy = 4.0
    sun.angle = math.radians(0.53)
    ob = bpy.data.objects.new('Sol_20graus', sun)
    col.objects.link(ob)
    # luz de lado e um pouco de trás da câmera, como no deserto do jogo
    ob.rotation_euler = (math.radians(90 - SUN_ELEV_DEG), 0.0, math.radians(-120.0))
    w = bpy.data.worlds.new('Ceu')
    w.use_nodes = True
    w.node_tree.nodes['Background'].inputs['Color'].default_value = (0.45, 0.6, 0.85, 1.0)
    w.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.8
    bpy.context.scene.world = w


def main():
    clear()
    ref = collection('Referencia (nao exportar)')
    grid(ref)
    tank(ref)
    human(ref)
    footprint(ref)
    cameras(ref)
    light(ref)
    collection('Modelo')                      # o que você modela vai aqui
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, 'RTS_Base.blend')
    bpy.ops.wm.save_as_mainfile(filepath=path)
    print('TEMPLATE salvo', path)
    # prévia da câmera do jogo (CPU, rápida), pra conferir a escala sem abrir o Blender
    s = bpy.context.scene
    s.render.engine = 'CYCLES'
    s.cycles.device = 'CPU'
    s.cycles.samples = 16
    s.render.resolution_percentage = 50
    s.render.filepath = os.path.join(OUT, 'RTS_Base_preview.png')
    bpy.ops.render.render(write_still=True)
    print('TEMPLATE prévia', s.render.filepath)


main()
