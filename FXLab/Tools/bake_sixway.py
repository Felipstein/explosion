"""Cozimento 6-way de uma simulação (VDB): fogo e fumaça na mesma gravação, como os jogos fazem.

Uso:
    blender -b --factory-startup -P bake_sixway.py -- <pasta_vdb> <saida_dir> <m_por_voxel> <qps_da_sim> \
        <elevação_da_câmera_graus> [quadros=48] [quadro_inicial=5] [quadro_final=-1] [res=256] [amostras=64] [albedo=0.3]         [lado_m=auto] [desloca_direita_m=0] [desloca_cima_m=0]

Por que (07/10/2026): os jogos AAA não desenham a bola de fogo como volume ao
vivo. Simulam uma vez (Houdini/EmberGen), gravam a sequência e o jogo só toca
cartões virados pra câmera (Battlefront II, CoD, a "six-way lighting" da
Unity/Realtime VFX). A luz não fica gravada: cada quadro guarda como a fumaça
responde à luz vinda de 6 direções, e o material do jogo mistura as 6 pela
direção do sol de verdade. O fogo é a emissão da própria simulação (corpo negro
pela temperatura), então ele vira fumaça sem trocar de objeto.

Uma renderização por quadro, com 6 sóis brancos, cada um no seu grupo de luz
(passes separados do Cycles) e o passe de emissão:
- six_pos.exr: R = luz da direita, G = de cima, B = da frente (do lado da câmera), A = cobertura (alfa);
- six_neg.exr: R = da esquerda, G = de baixo, B = de trás (contraluz), A = cobertura;
- emit.exr: RGB = o fogo (radiância linear, já atenuado pela fuligem da frente), A = 1;
- contact.png: conferência com uma luz de exemplo; atlas.json: grade, tempos, tamanho em metros.
Direita/cima/frente são os eixos do cartão (a câmera do cozimento). Alfa
pré-multiplicado nas três (a resposta à luz já vem multiplicada pela cobertura).
Câmera ortográfica: o quadro vira um cartão de tamanho conhecido.
"""
import glob
import json
import math
import os
import re
import sys

import bpy
import numpy as np
import OpenImageIO as oiio
from mathutils import Vector

argv = sys.argv[sys.argv.index('--') + 1:]
SRC, OUT = os.path.abspath(argv[0]), os.path.abspath(argv[1])
VOX, SIM_FPS, ELEV = float(argv[2]), float(argv[3]), math.radians(float(argv[4]))
NFRAMES = int(argv[5]) if len(argv) > 5 else 48
F0 = int(argv[6]) if len(argv) > 6 else 5
F1 = int(argv[7]) if len(argv) > 7 else -1
RES = int(argv[8]) if len(argv) > 8 else 256
SAMPLES = int(argv[9]) if len(argv) > 9 else 64
ALBEDO = float(argv[10]) if len(argv) > 10 else 0.3
# lado do quadro em metros: a caixa da grade do VDB é bem maior que a fumaça (medido no 1º cozimento: o
# conteúdo ocupa ~75% da altura); com o lado medido, a mesma resolução vira mais detalhe
SIDE_FIX = float(argv[11]) if len(argv) > 11 else 0.0
# e o centro: a caixa da grade nem sempre está centrada na fumaça (Dust Impact: ~0.9 m pra direita)
SHIFT_R = float(argv[12]) if len(argv) > 12 else 0.0
SHIFT_U = float(argv[13]) if len(argv) > 13 else 0.0
COLS = 8
os.makedirs(OUT, exist_ok=True)

files = sorted(glob.glob(os.path.join(SRC, '*.vdb')), key=lambda f: int(re.findall(r'(\d+)\.vdb$', f)[0]))
F1 = len(files) - 1 if F1 < 0 else min(F1, len(files) - 1)
sim_frames = [int(round(F0 + (F1 - F0) * k / (NFRAMES - 1))) for k in range(NFRAMES)]

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene


def material():
    """Fuligem de albedo cinza (a resposta à luz não depende da cor) e fogo de corpo negro."""
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
    dmul.inputs[1].default_value = 6.0                  # a mesma densidade da prévia que o Felipe viu
    nt.links.new(dens.outputs['Fac'], dmul.inputs[0])
    nt.links.new(dmul.outputs[0], pv.inputs['Density'])
    pv.inputs['Color'].default_value = (ALBEDO, ALBEDO, ALBEDO, 1.0)
    pv.inputs['Absorption Color'].default_value = (0.0, 0.0, 0.0, 1.0)
    pv.inputs['Anisotropy'].default_value = 0.3         # fumaça espalha mais pra frente (contraluz brilha na borda)
    # temperatura: 800 K (brasa escura) a 2600 K (miolo) pela chama; a curva de tela do jogo é que estoura o miolo em branco
    fl = nt.nodes.new('ShaderNodeClamp')
    nt.links.new(flame.outputs['Fac'], fl.inputs['Value'])
    tk = nt.nodes.new('ShaderNodeMapRange')
    tk.inputs['To Min'].default_value = 800.0
    tk.inputs['To Max'].default_value = 2600.0
    nt.links.new(fl.outputs[0], tk.inputs['Value'])
    bb = nt.nodes.new('ShaderNodeBlackbody')
    nt.links.new(tk.outputs[0], bb.inputs['Temperature'])
    nt.links.new(bb.outputs[0], pv.inputs['Emission Color'])
    # intensidade: Stefan–Boltzmann suavizado (chama³), o ganho absoluto fica no material do jogo
    p3 = nt.nodes.new('ShaderNodeMath')
    p3.operation = 'POWER'
    p3.inputs[1].default_value = 3.0
    nt.links.new(fl.outputs[0], p3.inputs[0])
    g = nt.nodes.new('ShaderNodeMath')
    g.operation = 'MULTIPLY'
    g.inputs[1].default_value = 60.0
    nt.links.new(p3.outputs[0], g.inputs[0])
    nt.links.new(g.outputs[0], pv.inputs['Emission Strength'])
    nt.links.new(pv.outputs['Volume'], out.inputs['Volume'])
    return mat


mat = material()

# câmera ortográfica olhando pra +X, inclinada ELEV pra baixo; eixos do cartão
fwd = Vector((math.cos(ELEV), 0.0, -math.sin(ELEV)))
right = Vector((0.0, -1.0, 0.0))
up = right.cross(fwd).normalized()
if up.z < 0:
    up = -up

# enquadramento: a caixa de todos os quadros (amostras) projetada nos eixos do cartão
lo_r = lo_u = 1e9
hi_r = hi_u = -1e9
center3 = Vector((0, 0, 0))
probes = sorted(set(sim_frames[::6] + [sim_frames[-1]]))
for i, fr in enumerate(probes):
    v = bpy.data.volumes.new(f'p{i}')
    v.filepath = files[fr]
    v.grids.load()
    o = bpy.data.objects.new(f'p{i}', v)
    sc.collection.objects.link(o)
    o.scale = (VOX, VOX, VOX)
    bpy.context.view_layer.update()
    for c in o.bound_box:
        p = o.matrix_world @ Vector(c)
        lo_r, hi_r = min(lo_r, p.dot(right)), max(hi_r, p.dot(right))
        lo_u, hi_u = min(lo_u, p.dot(up)), max(hi_u, p.dot(up))
    bpy.data.objects.remove(o, do_unlink=True)
SIDE = SIDE_FIX if SIDE_FIX > 0 else max(hi_r - lo_r, hi_u - lo_u) * 1.04
center = right * ((lo_r + hi_r) / 2 + SHIFT_R) + up * ((lo_u + hi_u) / 2 + SHIFT_U)
print(f'SIXWAY enquadramento: lado {SIDE:.2f} m, centro {tuple(round(x, 2) for x in center)}')

cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
sc.collection.objects.link(cam)
sc.camera = cam
cam.data.type = 'ORTHO'
cam.data.ortho_scale = SIDE
cam.data.clip_end = 1000.0
cam.location = center - fwd * 200.0
cam.rotation_euler = fwd.to_track_quat('-Z', 'Y').to_euler()

# 6 sóis brancos, um por grupo de luz. "Frente" = a luz vem do lado da câmera; "trás" = contraluz.
vl = sc.view_layers[0]
DIRS = {'px': right, 'nx': -right, 'py': up, 'ny': -up, 'pz': -fwd, 'nz': fwd}
for name, to_light in DIRS.items():
    vl.lightgroups.add(name=name)
    s = bpy.data.objects.new(f'sun_{name}', bpy.data.lights.new(f'sun_{name}', 'SUN'))
    s.data.energy = 1.0
    s.data.angle = math.radians(0.5)
    s.rotation_euler = (-to_light).to_track_quat('-Z', 'Y').to_euler()
    s.lightgroup = name
    sc.collection.objects.link(s)
vl.use_pass_emit = True
world = bpy.data.worlds.new('Black')
sc.world = world
world.use_nodes = True
world.node_tree.nodes['Background'].inputs[1].default_value = 0.0

sc.render.engine = 'CYCLES'
sc.cycles.device = 'CPU'                 # a placa é da Unreal (a 1660 cai com as duas)
sc.cycles.samples = SAMPLES
sc.cycles.use_denoising = False          # o denoiser não age nos passes de grupo de luz
sc.cycles.volume_bounces = 1
sc.cycles.volume_step_rate = 1.5
sc.render.film_transparent = True
sc.render.resolution_x = sc.render.resolution_y = RES
sc.view_settings.view_transform = 'Standard'
if hasattr(sc.render.image_settings, 'media_type'):        # Blender 5: EXR de várias camadas é um tipo de mídia
    sc.render.image_settings.media_type = 'MULTI_LAYER_IMAGE'
sc.render.image_settings.file_format = 'OPEN_EXR_MULTILAYER'
sc.render.image_settings.color_depth = '16'


def read_passes(path):
    """EXR de várias camadas do Blender 5: cada passe é uma parte do arquivo. Devolve {"Passe.C": imagem}."""
    inp = oiio.ImageInput.open(path)
    chans, sub = {}, 0
    while inp.seek_subimage(sub, 0):
        spec = inp.spec()
        px = inp.read_image(oiio.FLOAT).reshape(spec.height, spec.width, spec.nchannels)
        for i, n in enumerate(spec.channelnames):
            chans[n.split('.', 1)[1] if '.' in n else n] = px[..., i]
        sub += 1
    inp.close()
    return chans


atl_pos = np.zeros(((-(-NFRAMES // COLS)) * RES, COLS * RES, 4), np.float32)
atl_neg = np.zeros_like(atl_pos)
atl_emit = np.zeros_like(atl_pos)
times = []
for k, fr in enumerate(sim_frames):
    vol = bpy.data.volumes.new(f'v{k}')
    vol.filepath = files[fr]
    vol.grids.load()
    ob = bpy.data.objects.new(f'VDB{k}', vol)
    sc.collection.objects.link(ob)
    vol.materials.append(mat)
    ob.scale = (VOX, VOX, VOX)
    path = os.path.join(OUT, f'f{k:03d}.exr')
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(ob, do_unlink=True)
    bpy.data.volumes.remove(vol)

    ch = read_passes(path)
    if k == 0:
        print('SIXWAY passes:', sorted(ch))
    # sóis brancos e albedo cinza: R = G = B
    lum = {name: (ch[f'Combined_{name}.R'] + ch[f'Combined_{name}.G'] + ch[f'Combined_{name}.B']) / 3 for name in DIRS}
    a = ch['Combined.A']
    emit = np.stack([ch['Emission.R'], ch['Emission.G'], ch['Emission.B']], -1)
    r, c = divmod(k, COLS)
    sl = (slice(r * RES, (r + 1) * RES), slice(c * RES, (c + 1) * RES))
    atl_pos[sl] = np.stack([lum['px'], lum['py'], lum['pz'], a], -1)
    atl_neg[sl] = np.stack([lum['nx'], lum['ny'], lum['nz'], a], -1)
    atl_emit[sl] = np.concatenate([emit, np.ones_like(a)[..., None]], -1)
    times.append((fr - sim_frames[0]) / SIM_FPS)
    print(f'SIXWAY {k + 1}/{NFRAMES} sim {fr} t={times[-1]:.2f}s')


def write(name, arr):
    spec = oiio.ImageSpec(arr.shape[1], arr.shape[0], 4, oiio.HALF)
    o = oiio.ImageOutput.create(os.path.join(OUT, name))
    o.open(os.path.join(OUT, name), spec)
    o.write_image(arr)
    o.close()


write('six_pos.exr', atl_pos)
write('six_neg.exr', atl_neg)
write('emit.exr', atl_emit)

# conferência: sol de cima-direita-frente + um pouco de céu, fogo somado, curva simples, sobre cinza
sun = 0.55 * atl_pos[..., 0] + 0.6 * atl_pos[..., 1] + 0.45 * atl_pos[..., 2]
amb = 0.15 * (atl_pos[..., :3].sum(-1) + atl_neg[..., :3].sum(-1)) / 6
lit = (sun * 3.0 + amb)[..., None] * np.array([1.0, 0.8, 0.6]) + atl_emit[..., :3]
ldr = 1 - np.exp(-lit)
comp = ldr + np.array([0.35, 0.33, 0.31]) * (1 - atl_pos[..., 3:4])
img = (np.clip(comp, 0, 1) ** (1 / 2.2) * 255).astype(np.uint8)
o = oiio.ImageOutput.create(os.path.join(OUT, 'contact.png'))
o.open(os.path.join(OUT, 'contact.png'), oiio.ImageSpec(img.shape[1], img.shape[0], 3, oiio.UINT8))
o.write_image(img)
o.close()
json.dump({'cols': COLS, 'rows': -(-NFRAMES // COLS), 'frames': NFRAMES, 'res': RES, 'times_s': times,
           'side_m': SIDE, 'camera_elev_deg': math.degrees(ELEV), 'albedo': ALBEDO,
           'six_pos': 'R=+right G=+up B=+front(lado da câmera) A=alfa', 'six_neg': 'R=-right G=-up B=back(contraluz) A=alfa',
           'emit': 'RGB radiância linear do fogo (corpo negro 800–2600 K, chama³ × 60)', 'alpha': 'premultiplied',
           'source': SRC, 'voxel_m': VOX, 'sim_fps': SIM_FPS, 'sim_frames': sim_frames},
          open(os.path.join(OUT, 'atlas.json'), 'w'), indent=1)
print('SIXWAY pronto ->', OUT)
