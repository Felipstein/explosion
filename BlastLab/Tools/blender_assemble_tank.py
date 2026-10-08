"""Monta o tanque básico da USA a partir das peças geradas por IA (GLB) e
fotografa nos ângulos da ficha aprovada, pra comparar.

Uso: blender -b --factory-startup -P blender_assemble_tank.py -- <pasta_da_unidade>
Lê <pasta>/parts/{hull,turret,barrel,track}.glb; grava <pasta>/assembled.blend
e as fotos em <pasta>/compare/.

Eixos (Blender): X frente, Y esquerda, Z cima; metros; chão em z = 0.
As peças do Tripo vêm normalizadas (maior lado = 1). As escalas e posições
abaixo vêm de medidas da ficha (docs/concept/units/usa_mbt_player_color.webp,
vistas de cima e de lado: casco 382 px = 7,9 m) e das peças (fotos
ortográficas de blender_views.py):
- casco: frente em +X; fundo da saia em z_local -0,045; teto em +0,087;
- torre: frente (furo do cano) em +X; fundo em z_local -0,227; furo em
  (y 0, z -0,149);
- cano: ao longo de Y, boca em -Y, colar em +Y;
- lagarta: ao longo de Y, roda dentada em +Y, lado de fora em +X.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

UNIT = sys.argv[sys.argv.index('--') + 1]
PARTS = os.path.join(UNIT, 'parts')
OUT = os.path.join(UNIT, 'compare')
os.makedirs(OUT, exist_ok=True)

HULL_LEN = 7.9                 # m (ficha: casco 382 px; Abrams real 7,93)
SKIRT_BOTTOM = 0.69            # m acima do chão (ficha, vista de lado)
TURRET_LEN = 4.59              # m (ficha: da ponta da cunha ao fim do cesto)
TURRET_X = -0.165              # centro da torre (ficha)
BARREL_REAR, BARREL_TIP = 1.95, 6.05   # m em X (ficha: boca no px 9 da vista de cima)
BARREL_DIAM_SCALE = 2.6        # cano do Tripo é grosso pro comprimento: cano fino ~0,19 m
TRACK_LEN, TRACK_HEIGHT_SCALE = 7.7, 6.3   # comprimento real; altura achatada (ver nota)
TRACK_WIDTH = 0.85             # m (ficha, vista de frente)


def load(name):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.join(PARTS, name + '.glb'))
    obs = [o for o in bpy.data.objects if o not in before and o.type == 'MESH']
    ob = obs[0]
    ob.name = name
    ob.parent = None
    return ob


def place(ob, scale, rot_z, loc):
    ob.rotation_mode = 'XYZ'
    ob.rotation_euler = (0, 0, rot_z)
    ob.scale = scale
    ob.location = loc


bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene

hull = load('hull')
s = HULL_LEN
deck = 0.087 * s
hull_z = SKIRT_BOTTOM + 0.045 * s
place(hull, (s, s, s), 0, (0, 0, hull_z))
deck_z = hull_z + deck

turret = load('turret')
t = TURRET_LEN
turret_z = deck_z + 0.227 * t
place(turret, (t, t, t), 0, (TURRET_X, 0, turret_z))
bore_z = turret_z - 0.149 * t

barrel = load('barrel')
blen = BARREL_TIP - BARREL_REAR
# rotação +90° em Z: Y local → -X... (boca em -Y vai pra +X)
place(barrel, (BARREL_DIAM_SCALE, blen, BARREL_DIAM_SCALE), math.pi / 2,
      ((BARREL_REAR + BARREL_TIP) / 2, 0, bore_z))

track_l = load('track')
w = TRACK_WIDTH / 0.153
track_y = 0.249 * s - 0.05 - TRACK_WIDTH / 2
track_z = 0.129 * TRACK_HEIGHT_SCALE
# +90° em Z: comprimento (Y local) vai pra X, roda dentada (+Y) vai pra trás (-X),
# lado de fora (+X local) vai pra +Y (esquerda do tanque)
place(track_l, (w, TRACK_LEN, TRACK_HEIGHT_SCALE), math.pi / 2, (0, track_y, track_z))
track_r = track_l.copy()
track_r.data = track_l.data
track_r.name = 'track_r'
sc.collection.objects.link(track_r)
place(track_r, (-w, TRACK_LEN, TRACK_HEIGHT_SCALE), math.pi / 2, (0, -track_y, track_z))

print(f'ASSEMBLY deck_z={deck_z:.3f} turret_z={turret_z:.3f} bore_z={bore_z:.3f} '
      f'track_y={track_y:.3f} track_top={0.259 * TRACK_HEIGHT_SCALE:.3f}')

# --- fotos ---
sc.render.engine = 'BLENDER_WORKBENCH'
sc.display.shading.light = 'STUDIO'
sc.display.shading.color_type = 'TEXTURE'
sc.display.shading.show_shadows = True
sc.render.resolution_x, sc.render.resolution_y = 1600, 900
sc.world = bpy.data.worlds.new('w')
sc.world.color = (0.42, 0.43, 0.45)
cam_data = bpy.data.cameras.new('cam')
cam = bpy.data.objects.new('cam', cam_data)
sc.collection.objects.link(cam)
sc.camera = cam
center = Vector((1.0, 0, 1.3))


def shoot(name, loc, rot, ortho=None, lens=50):
    if ortho:
        cam_data.type = 'ORTHO'
        cam_data.ortho_scale = ortho
    else:
        cam_data.type = 'PERSP'
        cam_data.lens = lens
    cam.location = loc
    cam.rotation_euler = rot
    sc.render.filepath = os.path.join(OUT, name)
    bpy.ops.render.render(write_still=True)


d = 30
# como na ficha: frente pra esquerda nas vistas de cima e de lado
shoot('top.png', (center.x, 0, d), (0, 0, math.pi), ortho=12.5)
shoot('side.png', (center.x, d, center.z), (math.pi / 2, 0, math.pi), ortho=12.5)
shoot('front.png', (d, 0, 1.4), (math.pi / 2, 0, math.pi / 2), ortho=6.0)
shoot('back.png', (-d, 0, 1.4), (math.pi / 2, 0, -math.pi / 2), ortho=6.0)
# 3/4 como na ficha: de cima, pela frente-esquerda
p = Vector((14.0, 12.0, 11.0))
look = Vector((0.5, 0, 1.0))
rot = (look - p).to_track_quat('-Z', 'Y').to_euler()
shoot('three_quarter.png', p, rot, lens=55)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(UNIT, 'assembled.blend'))
