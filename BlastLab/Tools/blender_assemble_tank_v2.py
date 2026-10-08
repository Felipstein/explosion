"""Monta o tanque básico da USA com as peças "na mão" (Hunyuan + pintura
projetada das nossas vistas + esteira por script) e fotografa:
- de lado, ortográfica, no mesmo recorte da ficha (pra sobrepor);
- na câmera do jogo (55°, tanque com ~260 px numa imagem de 1024), no chão
  de areia com sol baixo, pra comparar com a "foto do jogo".

Uso: blender -b --factory-startup -P blender_assemble_tank_v2.py -- <config.json>
config: {"hull": glb, "turret": glb, "barrel": glb, "track": glb,
         "wheel": glb ou null, "out_dir": ...}
hull e turret já com a frente em +X (saída de blender_project_views.py);
barrel do Tripo (ao longo de Y, boca em -Y); track de blender_track.py
(lado esquerdo, coordenadas do tanque em metros).
Eixos: X frente, Y esquerda, Z cima, metros, chão em z = 0.
Medidas da ficha (casco 382 px = 7,9 m): casco 7,9 m; torre 3,31 m de
largura (com as caixas) e centro em x = -0,165; teto do casco ~1,55 m.
"""
import json
import math
import os
import sys

import bpy
from mathutils import Vector

cfg = json.load(open(sys.argv[sys.argv.index('--') + 1]))
OUT = cfg['out_dir']
os.makedirs(OUT, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene


def load(path, name):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    obs = [o for o in bpy.data.objects if o not in before and o.type == 'MESH']
    ob = obs[0]
    ob.name = name
    ob.rotation_mode = 'XYZ'      # o importador de glTF usa quaternion
    mw = ob.matrix_world.copy()
    ob.parent = None
    ob.matrix_world = mw
    bpy.context.view_layer.objects.active = ob
    for o in bpy.data.objects:
        o.select_set(o == ob)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return ob


def bounds(ob):
    vs = [v.co for v in ob.data.vertices]
    return (Vector((min(v.x for v in vs), min(v.y for v in vs), min(v.z for v in vs))),
            Vector((max(v.x for v in vs), max(v.y for v in vs), max(v.z for v in vs))))


def set_box(ob, scale, center_xy, z_min):
    mn, mx = bounds(ob)
    ob.scale = (scale, scale, scale)
    c = (mn + mx) / 2
    ob.location = (center_xy[0] - c.x * scale, center_xy[1] - c.y * scale, z_min - mn.z * scale)


HULL_LEN, HULL_ZMIN = 7.9, 0.22
hull = load(cfg['hull'], 'hull')
mn, mx = bounds(hull)
hs = HULL_LEN / (mx.x - mn.x)
set_box(hull, hs, (0, 0), HULL_ZMIN)
# a ficha tem o casco 17% mais alto que o do gerador (teto a 1,82 m, medido
# de lado); estica só a altura, com o pé da saia (z = 0,62 m) parado
HULL_STRETCH_Z, SKIRT_Z = 1.17, 0.62
hull.location.z = SKIRT_Z - (SKIRT_Z - hull.location.z) * HULL_STRETCH_Z
hull.scale.z = hs * HULL_STRETCH_Z
deck = hull.location.z + mx.z * hull.scale.z
print(f'ASM hull scale={hs:.3f} width={(mx.y - mn.y) * hs:.2f} top={deck:.2f}')

TURRET_W, TURRET_X = 3.31, 0.05    # ficha: torre de x = 2,09 (cunha) a -2,65 m (medido; o gerador tem o cesto mais comprido)
tur = load(cfg['turret'], 'turret')
mn, mx = bounds(tur)
ts = TURRET_W / (mx.y - mn.y)
# assenta a torre no teto: 1,55 m (ficha), não no topo da caixa do casco
turret_z = 1.70
set_box(tur, ts, (TURRET_X, 0), turret_z)
# a cunha da frente do gerador é ~9% mais curta que a da ficha: estica o comprimento
TURRET_STRETCH_X = 1.0
tur.scale.x = ts * TURRET_STRETCH_X
tur.location.x = TURRET_X - (mn.x + mx.x) / 2 * ts * TURRET_STRETCH_X
bpy.context.view_layer.update()
# eixo do cano: centro da ponta do tubo (vértices mais à frente)
vs = [v.co for v in tur.data.vertices]
front = max(v.x for v in vs)
tip = [v for v in vs if v.x > front - 0.02]
bore_y = sum(v.y for v in tip) / len(tip)
bore_z = sum(v.z for v in tip) / len(tip)
tw = tur.matrix_world
tip_w = tw @ Vector((front, bore_y, bore_z))
print(f'ASM turret scale={ts:.3f} length={(mx.x - mn.x) * ts:.2f} tip={tuple(round(c, 2) for c in tip_w)}')

# cano: do fim do tubo até a boca (ficha: boca em x ≈ 9,5 m na frente do centro? usa 4,9 m de cano)
BARREL_LEN, BARREL_TUBE = 3.3, 0.19   # ficha: boca em x = 5,5 m; tubo ~0,19 m
bar = load(cfg['barrel'], 'barrel')
mn, mx = bounds(bar)
if cfg.get('barrel_real'):
    # cano de blender_barrel.py: já em metros, ao longo de +X, base em x = 0 (o eixo é o do tubo)
    base_x, muzzle_x = tip_w.x - 0.8, 5.55
    bar.rotation_euler = (0, 0, 0)
    bar.scale = ((muzzle_x - base_x) / (mx.x - mn.x), 1.0, 1.0)
    bar.location = (base_x, tip_w.y, tip_w.z)
else:
    # cano do Tripo (ao longo de Y, boca em -Y, tamanho normalizado)
    bar.rotation_euler = (0, 0, math.pi / 2)      # boca (-Y) → +X
    d_s = BARREL_TUBE / 0.072              # o tubo do Tripo tem 0,072 de diâmetro (o colar, 0,14)
    bar.scale = (d_s, BARREL_LEN / (mx.y - mn.y), d_s)
    c = (mn + mx) / 2
    bpy.context.view_layer.update()
    # após girar 90°: y local → -x mundo; queremos a base (+Y local) encostada na ponta do tubo
    base_offset = (mx.y - c.y) * bar.scale.y
    # a base do cano (o colar escuro do Tripo) entra fundo no mantelete, escondida;
    # a boca fica onde a ficha mostra (x = 5,55 m)
    base_x, muzzle_x = tip_w.x - 0.8, 5.55
    BARREL_LEN = muzzle_x - base_x
    bar.scale.y = BARREL_LEN / (mx.y - mn.y)
    bar.location = (0, tip_w.y, tip_w.z)
    bar.location.x = (base_x + muzzle_x) / 2
    bar.location.y -= c.x * bar.scale.x
    bar.location.z -= c.z * bar.scale.z

for ob in (hull, tur, bar):
    for m in ob.data.materials:
        bs = next((n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
        if bs is None:
            continue
        for k in ('Metallic', 'Roughness'):
            for l in list(bs.inputs[k].links):
                m.node_tree.links.remove(l)
        bs.inputs['Metallic'].default_value = 0.0
        bs.inputs['Roughness'].default_value = 0.72

track_l = load(cfg['track'], 'track_l')
track_r = track_l.copy()
track_r.data = track_l.data.copy()
track_r.name = 'track_r'
sc.collection.objects.link(track_r)
track_r.scale = (1, -1, 1)

# rodas (posições da ficha, iguais às de blender_track.py)
M = 7.9 / 382.0
road_x = [3.95 - (p - 381) * M for p in (448.75, 488.75, 528.75, 567.5, 606.25, 645.0, 685.0)]
wheels = [(x, 0.36, 0.31) for x in road_x] + [(3.95 - 19 * M, 33 * M, 0.27), (3.95 - 354 * M, 34 * M, 0.29)]
if cfg.get('wheel'):
    wproto = load(cfg['wheel'], 'wheel')
    for m in wproto.data.materials:
        bs = next((n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
        if bs:
            for k in ('Metallic', 'Roughness'):
                for l in list(bs.inputs[k].links):
                    m.node_tree.links.remove(l)
            bs.inputs['Metallic'].default_value = 0.0
            bs.inputs['Roughness'].default_value = 0.8
else:
    bpy.ops.mesh.primitive_cylinder_add(radius=1, depth=0.55, vertices=48, rotation=(math.pi / 2, 0, 0))
    wproto = bpy.context.object
    m = bpy.data.materials.new('M_WheelTmp')
    m.use_nodes = True
    m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.42, 0.42, 0.4, 1)
    wproto.data.materials.append(m)
# origem no centro da roda, pra girar e posicionar
bpy.context.view_layer.objects.active = wproto
for o in bpy.data.objects:
    o.select_set(o == wproto)
bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY', center='BOUNDS')
mn, mx = bounds(wproto)
wdiam = max(mx.x - mn.x, mx.y - mn.y, mx.z - mn.z)   # o maior lado é o diâmetro
for side in (1, -1):
    for (x, z, r) in wheels:
        w = wproto.copy()
        w.data = wproto.data
        sc.collection.objects.link(w)
        s = 2 * r / wdiam
        w.scale = (s, s, s)
        # a roda pintada (blender_project_views) tem a face de fora (cubo com
        # parafusos) olhando pra +X: gira 90° pra ela olhar pra fora do tanque
        w.rotation_euler = (0, 0, math.pi / 2 if side > 0 else -math.pi / 2)
        w.location = (x, side * 1.45, z)
wproto.hide_render = True

# chão e luz de fim de tarde
bpy.ops.mesh.primitive_plane_add(size=200)
ground = bpy.context.object
gm = bpy.data.materials.new('M_Sand')
gm.use_nodes = True
gb = gm.node_tree.nodes['Principled BSDF']
gb.inputs['Base Color'].default_value = (0.55, 0.38, 0.22, 1)
gb.inputs['Roughness'].default_value = 0.95
ground.data.materials.append(gm)
sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN'))
sun.data.energy = 4.0
sun.data.color = (1.0, 0.82, 0.62)
sun.data.angle = math.radians(1.5)
sun.rotation_euler = (math.radians(72), 0, math.radians(-125))   # sol a ~18° do horizonte
sc.collection.objects.link(sun)
w = bpy.data.worlds.new('w')
w.use_nodes = True
w.node_tree.nodes['Background'].inputs[0].default_value = (0.55, 0.6, 0.7, 1)
w.node_tree.nodes['Background'].inputs[1].default_value = 0.6
sc.world = w
sc.render.engine = 'BLENDER_EEVEE'
sc.view_settings.view_transform = 'AgX'
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
sc.collection.objects.link(cam)
sc.camera = cam


def shoot(name, loc, look, lens=None, ortho=None, res=(1024, 576)):
    sc.render.resolution_x, sc.render.resolution_y = res
    if ortho:
        cam.data.type = 'ORTHO'
        cam.data.ortho_scale = ortho
    else:
        cam.data.type = 'PERSP'
        cam.data.lens = lens
    cam.location = loc
    cam.rotation_euler = (Vector(look) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = os.path.join(OUT, name)
    bpy.ops.render.render(write_still=True)


# câmera do jogo: 55° pra baixo, de frente-esquerda; tanque com ~260 px
d = 30.0
el = math.radians(55)
az = math.radians(35)
look = (0.3, 0, 1.0)
loc = (look[0] + d * math.cos(el) * math.cos(az), look[1] + d * math.cos(el) * math.sin(az), look[2] + d * math.sin(el))
shoot('game_cam.png', loc, look, lens=35)
# câmera deitada próxima
shoot('tilt_cam.png', (14, 9, 3.2), (0, 0, 1.2), lens=45)
# de lado, ortográfica, mesmo recorte da ficha (px 290..770 x 320..560)
ground.hide_render = True
x_l, x_r = 3.95 - (290 - 381) * M, 3.95 - (770 - 381) * M
z_t, z_b = (540 - 320) * M, (540 - 560) * M
shoot('side_ortho.png', ((x_l + x_r) / 2, 30, (z_t + z_b) / 2), ((x_l + x_r) / 2, 0, (z_t + z_b) / 2),
      ortho=abs(x_l - x_r), res=(960, 480))
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, 'tank_v2.blend'))
