"""Prévia da simulação do Mantaflow (blender_pyro_tank.py) na câmera baixa do vídeo.

Uso:
    blender -b <saida_dir>/pyro.blend -P blender_pyro_preview.py -- <saida_dir> [quadros separados por vírgula] [amostras] [cam]

Serve pra julgar a FORMA e o tempo da simulação contra o vídeo do Veo antes de
levar pra Unreal: chão de areia, sol a ~15°, céu físico, um bloco cinza do
tamanho do tanque (7,9 m) pra escala. O material:
- fuligem: densidade → extinção alta, albedo ~0,05 (quase preta);
- fogo: o campo "flame" → emissão de corpo negro aproximada por rampa
  (vermelho escuro → laranja → amarelo → branco), com brilho que cresce
  muito com a temperatura (a casca fria escurece, o miolo estoura).
cam: low (câmera baixa do vídeo, padrão) ou rts (55°, de cima).
"""
import math
import os
import sys

import bpy
from mathutils import Vector

argv = sys.argv[sys.argv.index('--') + 1:]
OUT = os.path.abspath(argv[0])
FRAMES = [int(f) for f in (argv[1] if len(argv) > 1 else '4,10,16,31,46,61,91').split(',')]
SAMPLES = int(argv[2]) if len(argv) > 2 else 48
CAM = argv[3] if len(argv) > 3 else 'low'

sc = bpy.context.scene
dom = bpy.data.objects['Domain']
for name in ('Charge', 'Wreck', 'Hatch') + tuple(f'Jet{i}' for i in range(8)) + tuple(f'Vent{i}' for i in range(4)):
    ob = bpy.data.objects.get(name)
    if ob:
        ob.hide_render = True
hull = bpy.data.objects['Hull']

# material da fumaça e do fogo
mat = bpy.data.materials.new('Pyro')
mat.use_nodes = True
nt = mat.node_tree
nt.nodes.clear()
out = nt.nodes.new('ShaderNodeOutputMaterial')
pv = nt.nodes.new('ShaderNodeVolumePrincipled')
dens = nt.nodes.new('ShaderNodeAttribute')
dens.attribute_name = 'density'
flame = nt.nodes.new('ShaderNodeAttribute')
flame.attribute_name = 'flame'
dmul = nt.nodes.new('ShaderNodeMath')
dmul.operation = 'MULTIPLY'
dmul.inputs[1].default_value = 6.0
nt.links.new(dens.outputs['Fac'], dmul.inputs[0])
nt.links.new(dmul.outputs[0], pv.inputs['Density'])
pv.inputs['Color'].default_value = (0.05, 0.05, 0.05, 1.0)
pv.inputs['Absorption Color'].default_value = (0.0, 0.0, 0.0, 1.0)
ramp = nt.nodes.new('ShaderNodeValToRGB')
cr = ramp.color_ramp
cr.elements[0].position = 0.0
cr.elements[0].color = (0.0, 0.0, 0.0, 1.0)
cr.elements[1].position = 1.0
cr.elements[1].color = (1.0, 0.95, 0.85, 1.0)
for pos, col in ((0.15, (0.35, 0.03, 0.0, 1.0)), (0.35, (1.0, 0.25, 0.02, 1.0)), (0.6, (1.0, 0.6, 0.15, 1.0))):
    e = cr.elements.new(pos)
    e.color = col
nt.links.new(flame.outputs['Fac'], ramp.inputs['Fac'])
# brilho: cresce como flame^4 (o corpo negro cresce muito mais rápido que linear)
p4 = nt.nodes.new('ShaderNodeMath')
p4.operation = 'POWER'
p4.inputs[1].default_value = 4.0
nt.links.new(flame.outputs['Fac'], p4.inputs[0])
g = nt.nodes.new('ShaderNodeMath')
g.operation = 'MULTIPLY'
g.inputs[1].default_value = 400.0
nt.links.new(p4.outputs[0], g.inputs[0])
nt.links.new(ramp.outputs['Color'], pv.inputs['Emission Color'])
nt.links.new(g.outputs[0], pv.inputs['Emission Strength'])
nt.links.new(pv.outputs['Volume'], out.inputs['Volume'])
dom.data.materials.clear()
dom.data.materials.append(mat)

# tanque cinza (escala) e chão de areia
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


hull.data.materials.clear()
hull.data.materials.append(surface('HullGrey', (0.35, 0.36, 0.34, 1), 0.6))
bpy.ops.mesh.primitive_plane_add(size=600, location=(0, 0, 0))
ground = bpy.context.object
ground.data.materials.append(surface('Sand', (0.62, 0.47, 0.32, 1), 0.95))

# sol a 15° e céu físico
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
bg.inputs[0].default_value = (0.42, 0.58, 0.85, 1.0)   # céu de fim de tarde, só a luz de ambiente
bg.inputs[1].default_value = 0.6
wn.links.new(bg.outputs['Background'], wout.inputs['Surface'])

# câmera
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
sc.render.resolution_x, sc.render.resolution_y = 960, 540
sc.view_settings.view_transform = 'AgX'
sc.view_settings.exposure = 0.0
for f in FRAMES:
    sc.frame_set(f)
    sc.render.filepath = os.path.join(OUT, f'prev_{CAM}_{f:03d}.png')
    bpy.ops.render.render(write_still=True)
    print(f'PREVIEW frame {f} -> {sc.render.filepath}')
