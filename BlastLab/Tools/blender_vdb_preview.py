"""Prévia de uma sequência OpenVDB qualquer (ex.: as simulações CC0 da JangaFX) na câmera do vídeo.

Uso:
    blender -b --factory-startup -P blender_vdb_preview.py -- <pasta_dos_vdb> <saida_dir> <metros_por_voxel> \
        <quadros_vdb separados por vírgula> [densidade] [chama] [amostras] [cam]

Mesma cena da blender_pyro_preview.py (chão de areia, sol a 15°, bloco do
tanque de 7,9 m, câmera baixa do vídeo), pra comparar a FORMA de fontes
diferentes com o mesmo olho. A escala (metros por voxel) é escolhida pra a
bola bater com o vídeo (≈ 1–1,2 tanque de largura no pico).
densidade/chama: nomes das grades (EmberGen: density/flames; Mantaflow: density/flame).
"""
import glob
import math
import os
import re
import sys

import bpy
from mathutils import Vector

argv = sys.argv[sys.argv.index('--') + 1:]
SRC, OUT, VOX = os.path.abspath(argv[0]), os.path.abspath(argv[1]), float(argv[2])
FRAMES = [int(f) for f in argv[3].split(',')]
DEN = argv[4] if len(argv) > 4 else 'density'
FLAME = argv[5] if len(argv) > 5 else 'flames'
SAMPLES = int(argv[6]) if len(argv) > 6 else 24
CAM = argv[7] if len(argv) > 7 else 'low'
FLAME_SCALE = float(argv[8]) if len(argv) > 8 else 1.0   # chama do EmberGen vai até ~7 em alguns: normaliza pra ~1
RES_X = int(argv[9]) if len(argv) > 9 else 960
# 'dust': poeira de areia (albedo alto da cor do chão, sem emissão) e ganho da densidade
DUST = len(argv) > 10 and argv[10] == 'dust'
DENS_GAIN = float(argv[11]) if len(argv) > 11 else 6.0
os.makedirs(OUT, exist_ok=True)

files = sorted(glob.glob(os.path.join(SRC, '*.vdb')), key=lambda f: int(re.findall(r'(\d+)\.vdb$', f)[0]))
sc = bpy.context.scene
for ob in list(bpy.data.objects):
    bpy.data.objects.remove(ob, do_unlink=True)


def surface(name, color, rough):
    mt = bpy.data.materials.new(name)
    mt.use_nodes = True
    t = mt.node_tree
    t.nodes.clear()
    o = t.nodes.new('ShaderNodeOutputMaterial')
    b = t.nodes.new('ShaderNodeBsdfPrincipled')
    b.inputs['Base Color'].default_value = color
    b.inputs['Roughness'].default_value = rough
    t.links.new(b.outputs[0], o.inputs['Surface'])
    return mt


def pyro_material():
    mat = bpy.data.materials.new('Pyro')
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    pv = nt.nodes.new('ShaderNodeVolumePrincipled')
    dens = nt.nodes.new('ShaderNodeAttribute')
    dens.attribute_name = DEN
    flame = nt.nodes.new('ShaderNodeAttribute')
    flame.attribute_name = FLAME
    dmul = nt.nodes.new('ShaderNodeMath')
    dmul.operation = 'MULTIPLY'
    dmul.inputs[1].default_value = DENS_GAIN   # densidade por metro (ajustada no olho, igual à prévia)
    nt.links.new(dens.outputs['Fac'], dmul.inputs[0])
    nt.links.new(dmul.outputs[0], pv.inputs['Density'])
    pv.inputs['Color'].default_value = (0.66, 0.52, 0.36, 1.0) if DUST else (0.05, 0.05, 0.05, 1.0)
    pv.inputs['Absorption Color'].default_value = (0.0, 0.0, 0.0, 1.0)
    ramp = nt.nodes.new('ShaderNodeValToRGB')
    cr = ramp.color_ramp
    cr.elements[0].position, cr.elements[0].color = 0.0, (0.0, 0.0, 0.0, 1.0)
    cr.elements[1].position, cr.elements[1].color = 1.0, (1.0, 0.95, 0.85, 1.0)
    for pos, col in ((0.15, (0.35, 0.03, 0.0, 1.0)), (0.35, (1.0, 0.25, 0.02, 1.0)), (0.6, (1.0, 0.6, 0.15, 1.0))):
        e = cr.elements.new(pos)
        e.color = col
    fs = nt.nodes.new('ShaderNodeMath')
    fs.operation = 'MULTIPLY'
    fs.use_clamp = True
    fs.inputs[1].default_value = FLAME_SCALE
    nt.links.new(flame.outputs['Fac'], fs.inputs[0])
    nt.links.new(fs.outputs[0], ramp.inputs['Fac'])
    p4 = nt.nodes.new('ShaderNodeMath')
    p4.operation = 'POWER'
    p4.inputs[1].default_value = 4.0
    nt.links.new(fs.outputs[0], p4.inputs[0])
    g = nt.nodes.new('ShaderNodeMath')
    g.operation = 'MULTIPLY'
    g.inputs[1].default_value = 400.0
    nt.links.new(p4.outputs[0], g.inputs[0])
    nt.links.new(ramp.outputs['Color'], pv.inputs['Emission Color'])
    if not DUST:
        nt.links.new(g.outputs[0], pv.inputs['Emission Strength'])
    nt.links.new(pv.outputs['Volume'], out.inputs['Volume'])
    return mat


# o volume: cada quadro pedido vira um objeto próprio (render a render)
mat = pyro_material()
bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0.0, 0.0, 1.0))
hull = bpy.context.object
hull.scale = (7.9, 3.6, 1.6)
hull.data.materials.append(surface('HullGrey', (0.35, 0.36, 0.34, 1), 0.6))
bpy.ops.mesh.primitive_plane_add(size=600, location=(0, 0, 0))
bpy.context.object.data.materials.append(surface('Sand', (0.62, 0.47, 0.32, 1), 0.95))

SUN_EL, SUN_AZ = math.radians(15.0), math.radians(200.0)
bpy.ops.object.light_add(type='SUN')
sun = bpy.context.object
sun.data.energy = 5.0
sun.data.angle = math.radians(0.6)
sun.data.color = (1.0, 0.86, 0.68)
sun.rotation_euler = (math.pi / 2 - SUN_EL, 0.0, SUN_AZ)
world = bpy.data.worlds.new('Sky')
sc.world = world
world.use_nodes = True
wn = world.node_tree
wn.nodes.clear()
bg = wn.nodes.new('ShaderNodeBackground')
wout = wn.nodes.new('ShaderNodeOutputWorld')
bg.inputs[0].default_value = (0.42, 0.58, 0.85, 1.0)
bg.inputs[1].default_value = 0.6
wn.links.new(bg.outputs['Background'], wout.inputs['Surface'])

bpy.ops.object.camera_add()
cam = bpy.context.object
sc.camera = cam
if CAM == 'rts':
    pos, look, vfov = Vector((48.0, -34.0, 72.0)), Vector((0.0, 0.0, 4.0)), 33.0
else:
    pos, look, vfov = Vector((19.0, -13.0, 2.2)), Vector((0.0, 0.0, 6.5)), 42.0
cam.location = pos
cam.rotation_euler = (look - pos).to_track_quat('-Z', 'Y').to_euler()
cam.data.sensor_fit = 'VERTICAL'
cam.data.angle = math.radians(vfov)
sc.render.engine = 'CYCLES'
sc.cycles.samples = SAMPLES
sc.cycles.use_denoising = True
sc.cycles.volume_bounces = 2
sc.cycles.device = 'CPU'
sc.render.resolution_x, sc.render.resolution_y = RES_X, RES_X * 9 // 16
sc.cycles.volume_step_rate = 2.0
sc.view_settings.view_transform = 'AgX'

for fr in FRAMES:
    vol = bpy.data.volumes.new(f'vdb{fr}')
    vol.filepath = files[fr]
    vol.grids.load()
    ob = bpy.data.objects.new(f'VDB_{fr}', vol)
    sc.collection.objects.link(ob)
    vol.materials.append(mat)
    # o pé da simulação (índice z = 0, centro em x/y da caixa ativa) no centro do chão
    ob.scale = (VOX, VOX, VOX)
    bpy.context.view_layer.update()
    bb = [ob.matrix_world @ Vector(c) for c in ob.bound_box]
    cx = (min(p.x for p in bb) + max(p.x for p in bb)) / 2
    cy = (min(p.y for p in bb) + max(p.y for p in bb)) / 2
    ob.location = (-cx, -cy, -min(p.z for p in bb))
    sc.render.filepath = os.path.join(OUT, f'vdb_{CAM}_{fr:03d}.png')
    bpy.ops.render.render(write_still=True)
    print(f'VDBPREVIEW {fr} -> {sc.render.filepath}')
    bpy.data.objects.remove(ob, do_unlink=True)
