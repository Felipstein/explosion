"""Flipbook da explosão: a simulação (VDB) renderizada no Cycles, em qualidade de prévia, pra o jogo só tocar.

Uso:
    blender -b --factory-startup -P blender_flipbook.py -- <pasta_vdb> <saida_dir> <m_por_voxel> <qps_da_sim> \
        <elevação_da_câmera_graus> [quadros=42] [qps_do_flipbook=15] [res=512] [amostras=48] [lado_m=18]

Por que (07/10/2026): a mesma simulação do EmberGen renderizada a frio fica muito
melhor que o volume desenhado ao vivo no jogo (meia resolução, simulação
reduzida, luz aproximada). Os jogos AAA fazem assim: a bola de fogo é
renderizada antes e o jogo toca a sequência (Battlefront II, CoD).

O que sai:
- fNNN.exr: cada quadro, RGBA em ponto flutuante, alfa pré-multiplicado
  (a cor já multiplicada pela cobertura), fundo transparente;
- atlas.exr: os quadros lado a lado (8 colunas), e atlas.json com a grade,
  o tempo de cada quadro e o tamanho em metros (o quadro tem `lado_m` de
  largura e altura, com o chão na borda de baixo e a explosão no centro);
- contact.png: os quadros em 8 bits pra conferir.
Luz: a da cena herói de comparação (16h45, sol a 6,4° em contraluz, 12° à
direita da câmera) e céu quente. O material é o mesmo da prévia
(blender_vdb_preview.py) que o Felipe viu.
Câmera ortográfica (sem perspectiva): o quadro vira um cartão de tamanho
conhecido no jogo, virado pra câmera.
"""
import glob
import json
import math
import os
import re
import sys

import bpy
import numpy as np
from mathutils import Vector

argv = sys.argv[sys.argv.index('--') + 1:]
SRC, OUT = os.path.abspath(argv[0]), os.path.abspath(argv[1])
VOX, SIM_FPS, ELEV = float(argv[2]), float(argv[3]), math.radians(float(argv[4]))
NFRAMES = int(argv[5]) if len(argv) > 5 else 42
FB_FPS = float(argv[6]) if len(argv) > 6 else 15.0
RES = int(argv[7]) if len(argv) > 7 else 512
SAMPLES = int(argv[8]) if len(argv) > 8 else 48
SIDE = float(argv[9]) if len(argv) > 9 else 18.0
# afunda a simulação (m): na Gasoline Explosion a bola nasce a ~12 m de altura (o combustível é jogado
# pra cima); na morte de tanque do vídeo ela nasce no tanque. O que fica abaixo do chão sai do quadro.
DROP = float(argv[10]) if len(argv) > 10 else 0.0
# 'all' (padrão), 'emit' (só o fogo: sem sol nem céu) ou 'lit' (só a fumaça iluminada: sem emissão).
# Separados, o jogo controla o fogo e a fumaça com ganhos próprios (como o Battlefront II).
PASS = argv[11] if len(argv) > 11 else 'all'
# sol: altura e ângulo à direita da direção da câmera (padrão: a contraluz da comparação, 6,4° e 12°;
# a prévia que o Felipe gostou tinha o sol a 15° de altura e 35° pro lado: luz de lado/frente)
if len(argv) > 13:
    SUN_EL, SUN_RIGHT = math.radians(float(argv[12])), math.radians(float(argv[13]))
# 'agx': grava a imagem já com a transformada de vista AgX do Blender (cor de tela, 8 bits, a mesma
# da prévia que o Felipe aprovou); o jogo desfaz o próprio tonemapper no material pra mostrar
# exatamente isso. Padrão: radiância linear em EXR.
DISPLAY = argv[14] if len(argv) > 14 else 'linear'
COLS = 8
SUN_EL, SUN_RIGHT = math.radians(6.4), math.radians(12.0)
os.makedirs(OUT, exist_ok=True)

files = sorted(glob.glob(os.path.join(SRC, '*.vdb')), key=lambda f: int(re.findall(r'(\d+)\.vdb$', f)[0]))
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene


def pyro_material():
    mat = bpy.data.materials.new('Pyro')
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    pv = nt.nodes.new('ShaderNodeVolumePrincipled')
    dens = nt.nodes.new('ShaderNodeAttribute')
    dens.attribute_name = 'density'
    flame = nt.nodes.new('ShaderNodeAttribute')
    flame.attribute_name = 'flames'
    dmul = nt.nodes.new('ShaderNodeMath')
    dmul.operation = 'MULTIPLY'
    dmul.inputs[1].default_value = 6.0
    nt.links.new(dens.outputs['Fac'], dmul.inputs[0])
    nt.links.new(dmul.outputs[0], pv.inputs['Density'])
    pv.inputs['Color'].default_value = (0.05, 0.05, 0.05, 1.0)
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
    fs.inputs[1].default_value = 1.0
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
    nt.links.new(g.outputs[0], pv.inputs['Emission Strength'])
    nt.links.new(pv.outputs['Volume'], out.inputs['Volume'])
    return mat


mat = pyro_material()
if PASS == 'lit':
    next(n for n in mat.node_tree.nodes if n.type == 'MATH' and n.inputs[1].default_value == 400.0).inputs[1].default_value = 0.0
# câmera ortográfica olhando pra +X, inclinada ELEV pra baixo
fwd = Vector((math.cos(ELEV), 0.0, -math.sin(ELEV)))
center = Vector((0.0, 0.0, SIDE / 2 - 0.5))
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
sc.collection.objects.link(cam)
sc.camera = cam
cam.data.type = 'ORTHO'
cam.data.ortho_scale = SIDE
cam.location = center - fwd * 80.0
cam.rotation_euler = fwd.to_track_quat('-Z', 'Y').to_euler()
# sol na frente da câmera (contraluz), 12° à direita, 6,4° de altura; a luz anda pra trás do sol
right = Vector((0.0, -1.0, 0.0))
az = Vector((1.0, 0.0, 0.0)) * math.cos(SUN_RIGHT) + right * math.sin(SUN_RIGHT)
to_sun = (az * math.cos(SUN_EL) + Vector((0, 0, 1)) * math.sin(SUN_EL)).normalized()
sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN'))
sun.data.energy = 0.0 if PASS == 'emit' else 5.0
sun.data.angle = math.radians(0.6)
sun.data.color = (1.0, 0.78, 0.55)
sun.rotation_euler = (-to_sun).to_track_quat('-Z', 'Y').to_euler()
sc.collection.objects.link(sun)
world = bpy.data.worlds.new('Sky')
sc.world = world
world.use_nodes = True
wn = world.node_tree
wn.nodes.clear()
bg = wn.nodes.new('ShaderNodeBackground')
wout = wn.nodes.new('ShaderNodeOutputWorld')
bg.inputs[0].default_value = (0.85, 0.68, 0.52, 1.0)      # céu de fim de tarde com poeira
bg.inputs[1].default_value = 0.0 if PASS == 'emit' else 0.5
wn.links.new(bg.outputs['Background'], wout.inputs['Surface'])

sc.render.engine = 'CYCLES'
sc.cycles.samples = SAMPLES
sc.cycles.use_denoising = True
sc.cycles.volume_bounces = 2
sc.cycles.device = 'CPU'
sc.render.film_transparent = True
sc.render.resolution_x = sc.render.resolution_y = RES
sc.render.image_settings.color_mode = 'RGBA'
if DISPLAY == 'agx':
    sc.view_settings.view_transform = 'AgX'
    sc.view_settings.exposure = 0.0
    sc.render.image_settings.file_format = 'PNG'
    sc.render.image_settings.color_depth = '8'
else:
    sc.view_settings.view_transform = 'Standard'
    sc.render.image_settings.file_format = 'OPEN_EXR'
    sc.render.image_settings.color_depth = '16'
EXT = 'png' if DISPLAY == 'agx' else 'exr'

# posição: o chão é o índice z = 0 da grade (o 1º quadro do EmberGen começa no ar, então a caixa
# dele não serve); a caixa de um quadro do meio (a bola já encostada no chão) dá o chão e o centro
probe = bpy.data.volumes.new('probe')
probe.filepath = files[min(len(files) - 1, int(len(files) * 0.45))]
probe.grids.load()
pob = bpy.data.objects.new('probe', probe)
sc.collection.objects.link(pob)
pob.scale = (VOX, VOX, VOX)
bpy.context.view_layer.update()
bb = [pob.matrix_world @ Vector(c) for c in pob.bound_box]
offset = Vector((-(min(p.x for p in bb) + max(p.x for p in bb)) / 2,
                 -(min(p.y for p in bb) + max(p.y for p in bb)) / 2, -min(p.z for p in bb) - DROP))
bpy.data.objects.remove(pob, do_unlink=True)
print(f'FLIPBOOK caixa do quadro de referência: {tuple(round(x, 2) for x in bb[0])} .. {tuple(round(x, 2) for x in bb[6])}')

times, frames = [], []
for k in range(NFRAMES):
    t = k / FB_FPS
    fr = min(int(round(t * SIM_FPS)), len(files) - 1)
    vol = bpy.data.volumes.new(f'v{k}')
    vol.filepath = files[fr]
    vol.grids.load()
    ob = bpy.data.objects.new(f'VDB{k}', vol)
    sc.collection.objects.link(ob)
    vol.materials.append(mat)
    ob.scale = (VOX, VOX, VOX)
    bpy.context.view_layer.update()
    ob.location = offset
    path = os.path.join(OUT, f'f{k:03d}.{EXT}')
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(ob, do_unlink=True)
    times.append(t)
    frames.append(path)
    print(f'FLIPBOOK {k + 1}/{NFRAMES} t={t:.2f}s sim {fr}')

# atlas (8 colunas) em ponto flutuante + folha de conferência
rows = -(-NFRAMES // COLS)
atlas = np.zeros((rows * RES, COLS * RES, 4), dtype=np.float32)
for k, path in enumerate(frames):
    img = bpy.data.images.load(path)
    px = np.array(img.pixels[:], dtype=np.float32).reshape(RES, RES, 4)[::-1]     # de baixo pra cima → de cima pra baixo
    r, c = divmod(k, COLS)
    atlas[r * RES:(r + 1) * RES, c * RES:(c + 1) * RES] = px
    bpy.data.images.remove(img)
out_img = bpy.data.images.new('atlas', COLS * RES, rows * RES, alpha=True, float_buffer=DISPLAY != 'agx')
out_img.pixels = atlas[::-1].ravel().tolist()
out_img.filepath_raw = os.path.join(OUT, 'atlas.' + EXT)
out_img.file_format = 'PNG' if DISPLAY == 'agx' else 'OPEN_EXR'
out_img.save()
ldr = np.clip(atlas[..., :3] / (1.0 + atlas[..., :3]), 0, 1)                         # só pra ver
bgc = np.array([0.25, 0.22, 0.2])
comp = ldr + bgc * (1 - atlas[..., 3:4])
prev = bpy.data.images.new('contact', COLS * RES, rows * RES)
prev.pixels = np.concatenate([np.clip(comp, 0, 1), np.ones_like(comp[..., :1])], -1)[::-1].ravel().tolist()
prev.filepath_raw = os.path.join(OUT, 'contact.png')
prev.file_format = 'PNG'
prev.save()
json.dump({'cols': COLS, 'rows': rows, 'frames': NFRAMES, 'res': RES, 'fps': FB_FPS, 'times_s': times,
           'side_m': SIDE, 'ground_at': 'bottom edge, 0.5 m below frame', 'camera_elev_deg': math.degrees(ELEV),
           'sun': {'elev_deg': math.degrees(SUN_EL), 'right_of_camera_deg': math.degrees(SUN_RIGHT)},
           'alpha': 'straight (PNG, cor de tela AgX)' if DISPLAY == 'agx' else 'premultiplied', 'display': DISPLAY,
           'source': SRC,
           'voxel_m': VOX, 'sim_fps': SIM_FPS, 'drop_m': DROP, 'pass': PASS},
          open(os.path.join(OUT, 'atlas.json'), 'w'), indent=1)
print('FLIPBOOK pronto ->', OUT)
