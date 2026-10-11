"""Folha de conferência dos modelos provisórios (Cycles na CPU: não usa a placa).

Uso: blender -b --factory-startup -P Game/Tools/blender_provisional_preview.py -- <dir_glb> <dir_saida> [tanque_dir]
Cada modelo sai em 2 vistas: a da câmera do jogo (37,5° de inclinação, o ângulo do RTS) e uma
3/4 baixa pra ver o detalhe. As peças que mexem entram no lugar delas (o mesmo do Units.json).
O tanque entra do lado, pra medir a proporção.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index('--') + 1:]
SRC, OUT = args[0], args[1]
TANK = args[2] if len(args) > 2 else ''
os.makedirs(OUT, exist_ok=True)

# onde vai cada peça que mexe (metros, X frente): o mesmo que o Units.json manda pra Unreal
MODELS = {
    'war_factory': [('war_factory', (0, 0, 0)), ('war_factory_fan', (-3.5, -3.4, 9.5)), ('war_factory_fan', (-3.5, 3.4, 9.5))],
    'command_center': [('command_center', (0, 0, 0)), ('command_center_radar', (-6.0, 12.6, 12.9))],
    'power_plant': [('power_plant', (0, 0, 0))],
    'supply_center': [('supply_center', (0, 0, 0))],
    'supply_dock': [('supply_dock', (0, 0, 0))] + [('supply_crate', (-9.6 + gx * 4.8 + dx, -12.6 + gy * 4.2 + dy, dz))
                                                    for gx in range(5) for gy in range(6) if not (gx <= 1 and gy >= 4)
                                                    for dx, dy, dz in ((-0.75, -0.65, 0.22), (0.75, -0.65, 0.22), (-0.75, 0.65, 0.22), (0.75, 0.65, 0.22), (0.0, 0.0, 1.22))
                                                    if (gx + gy) % 3 != 2],
    'dozer': [('dozer', (0, 0, 0)), ('dozer_blade', (-0.6, 0, 1.1)), ('@tank_tracks', (0, 0, 0)), ('@tank_wheels', (0, 0, 0))],
    'supply_truck': [('supply_truck', (0, 0, 0)), ('supply_truck_wheels', (0, 0, 0))]
                    + [('supply_crate', (x, 0, 1.38)) for x in (-3.55, -2.0, -0.45)],
}
TANK_PARTS = {'@tank_tracks': 'SM_USA_MBT_Tracks', '@tank_wheels': 'SM_USA_MBT_Wheels', '@tank_hull': 'SM_USA_MBT_Hull',
              '@tank_turret': 'SM_USA_MBT_Turret', '@tank_barrel': 'SM_USA_MBT_Barrel'}
EMISSIVE = {'Light': (1.0, 0.75, 0.4, 25.0), 'Core': (1.0, 0.93, 0.8, 60.0), 'Beacon': (1.0, 0.45, 0.05, 30.0)}


def glb_path(name):
    if name.startswith('@'):
        return os.path.join(TANK, TANK_PARTS[name] + '.glb')
    folder = {'war_factory_fan': 'war_factory', 'command_center_radar': 'command_center', 'dozer_blade': 'dozer',
              'supply_truck_wheels': 'supply_truck'}.get(name, name)
    return os.path.join(SRC, folder, name + '.glb')


def load(name, at):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=glb_path(name))
    new = [o for o in bpy.data.objects if o not in before]
    for o in new:
        if o.parent is None:
            o.location = Vector(at) + o.location
    return new


def fix_materials():
    for m in bpy.data.materials:
        base = m.name.split('.')[0]
        if base in EMISSIVE and m.node_tree:
            r, g, b, s = EMISSIVE[base]
            bsdf = next((n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
            if bsdf:
                bsdf.inputs['Emission Color'].default_value = (r, g, b, 1)
                bsdf.inputs['Emission Strength'].default_value = s


def scene_setup():
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'
    sc.cycles.device = 'CPU'
    sc.cycles.samples = 48
    sc.cycles.use_denoising = True
    sc.render.resolution_x, sc.render.resolution_y = 1280, 800
    sc.view_settings.view_transform = 'AgX'
    w = bpy.data.worlds.new('w')
    sc.world = w
    w.use_nodes = True
    bg = w.node_tree.nodes['Background']
    bg.inputs['Color'].default_value = (0.42, 0.52, 0.68, 1)
    bg.inputs['Strength'].default_value = 0.9
    sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN'))
    sun.data.energy = 4.5
    sun.data.angle = math.radians(1.5)
    sun.rotation_euler = (math.radians(40), 0, math.radians(-35))
    sc.collection.objects.link(sun)
    bpy.ops.mesh.primitive_plane_add(size=600, location=(0, 0, 0))
    ground = bpy.context.active_object
    gm = bpy.data.materials.new('ground')
    gm.use_nodes = True
    gb = gm.node_tree.nodes['Principled BSDF']
    gb.inputs['Base Color'].default_value = (0.36, 0.30, 0.22, 1)
    gb.inputs['Roughness'].default_value = 0.95
    ground.data.materials.append(gm)
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    sc.collection.objects.link(cam)
    sc.camera = cam
    return cam


def frame(cam, objs, pitch_deg, yaw_deg, lens=None, margin=0.8):
    pts = [o.matrix_world @ Vector(c) for o in objs if o.type == 'MESH' for c in o.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(max(p.z, 0) for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    center = (lo + hi) / 2
    radius = (hi - lo).length / 2
    cam.data.sensor_fit = 'HORIZONTAL'
    cam.data.angle = math.radians(50) if lens is None else 2 * math.atan(18 / lens)
    fov = min(cam.data.angle, cam.data.angle * 800 / 1280)
    dist = radius * margin / math.sin(fov / 2)
    p, y = math.radians(pitch_deg), math.radians(yaw_deg)
    # o jogo olha pra +X com yaw 0 (a câmera fica atrás, em −X); aqui o yaw gira em volta do modelo
    d = Vector((-math.cos(p) * math.cos(y), -math.cos(p) * math.sin(y), math.sin(p)))
    cam.location = center + d * dist
    cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()


cam = scene_setup()
only = os.environ.get('PROV_ONLY', '')
for model, parts in MODELS.items():
    if only and model not in only.split(','):
        continue
    objs = []
    for name, at in parts:
        objs += load(name, at)
    if TANK and model in ('war_factory', 'command_center', 'supply_center', 'power_plant', 'supply_dock'):
        # o tanque saindo pela frente (proporção)
        off = {'war_factory': (19.0, 0.0), 'command_center': (21.0, 0.0), 'supply_center': (15.0, 0.0),
               'power_plant': (10.0, -4.0), 'supply_dock': (17.0, 0.0)}[model]
        for t in ('@tank_tracks', '@tank_wheels', '@tank_hull', '@tank_turret', '@tank_barrel'):
            objs += load(t, (off[0], off[1], 0))
    fix_materials()
    # vista do jogo: de trás e de lado como o jogador vê (a câmera olha pra frente do mapa, os
    # prédios ficam com a frente virada pra câmera ou de lado; aqui a de 3/4 de frente)
    for tag, pitch, yaw in (('rts', 37.5, 200.0), ('low', 18.0, 215.0), ('top', 89.0, 180.0)):
        frame(cam, objs, pitch, yaw, lens=None if tag != 'low' else 60)
        bpy.context.scene.render.filepath = os.path.join(OUT, f'{model}_{tag}.png')
        bpy.ops.render.render(write_still=True)
        print('PREVIEW', model, tag)
    for o in objs:
        bpy.data.objects.remove(o, do_unlink=True)
print('PREVIEW fim')
