"""Cozimento 6-way do jato de areia do tiro no chão: grãos simulados de verdade, não riscos pintados.

Uso:
    blender -b --factory-startup -P bake_debris.py -- <saida_dir> [quadros=48] [qps=30] [res=384] [amostras=48]

Por que (08/10/2026): no laboratório, os raios de areia desenhados à mão "parecem PNG" (Felipe).
Os jogos fazem esse jato com uma gravação: milhares de grãos lançados em leque, com gravidade e
arrasto, e o borrão de movimento é que vira os raios escuros da arte 4.1. Aqui a simulação é
uma conta simples (balística com arrasto, determinística) e cada grão vira um elipsoide esticado
na direção da velocidade pelo tempo do obturador (o borrão de movimento, feito na geometria).

Sai no mesmo formato do bake_sixway.py (six_pos/six_neg/emit + atlas.json): o laboratório e a
Unreal tocam igual à poeira. O leque inclina pra DIREITA do quadro (o jogo espelha o cartão
quando o tiro vai pra esquerda da tela).
"""
import json
import math
import os
import sys

import bmesh
import bpy
import numpy as np
import OpenImageIO as oiio
from mathutils import Vector

argv = sys.argv[sys.argv.index('--') + 1:]
OUT = os.path.abspath(argv[0])
NFRAMES = int(argv[1]) if len(argv) > 1 else 48
FPS = float(argv[2]) if len(argv) > 2 else 30.0
RES = int(argv[3]) if len(argv) > 3 else 384
SAMPLES = int(argv[4]) if len(argv) > 4 else 48
COLS = 8
os.makedirs(OUT, exist_ok=True)

# ---- simulação: grãos e pedras em leque ----------------------------------------------------------
rng = np.random.default_rng(7)
N_GRAIN, N_ROCK = 18000, 130        # 2600 saía ralo; 7000 de 1,5–6 cm virava enxame; 22 mil uniformes, um borrão (08/10)
TILT, SPREAD = math.radians(20), math.radians(22)      # o eixo do leque inclina 20° pra direita; abertura 22°
G, SHUTTER = 9.8, 1.0 / 30.0                           # obturador de 360° a 30 qps: o rastro vira o raio da arte


def cone(n, spread):
    a = rng.uniform(0, 2 * math.pi, n)
    r = np.sqrt(rng.uniform(0, 1, n)) * spread
    ax = np.array([math.sin(TILT), 0.0, math.cos(TILT)])           # x = direita do quadro, z = cima
    side = np.array([0.0, 1.0, 0.0])
    up2 = np.cross(ax, side)
    return (ax[None] * np.cos(r)[:, None] + (side[None] * np.cos(a)[:, None] + up2[None] * np.sin(a)[:, None]) * np.sin(r)[:, None])


def cone_rays(n, spread, nrays, jitter, frac):
    """os raios da arte: a maior parte dos grãos sai em poucas direções (um jato por raio), o resto enche o leque"""
    rays = cone(nrays, spread)
    pick = rng.integers(0, nrays, n)
    d = rays[pick] + rng.normal(0.0, jitter, (n, 3))
    d /= np.linalg.norm(d, axis=1, keepdims=True)
    use = rng.uniform(0, 1, n) < frac
    return np.where(use[:, None], d, cone(n, spread))


def make(n, spread, vmin, vmax, smin, smax, drag, rays=None):
    d = cone_rays(n, spread, *rays) if rays else cone(n, spread)
    v = d * rng.uniform(vmin, vmax, n)[:, None]
    # a maioria sai no começo, um rabo sai depois (o jato "fecha" em ~0.1 s)
    t0 = rng.exponential(0.025, n)
    size = rng.uniform(smin, smax, n) ** 1.0
    return {'v0': v, 't0': t0, 'size': size, 'drag': drag, 'p0': rng.normal(0, 0.15, (n, 3)) * np.array([1, 1, 0.2])}


grains = make(N_GRAIN, SPREAD, 8.0, 22.0, 0.006, 0.024, 1.2, rays=(14, 0.035, 0.8))   # 14 raios; sobe ~8–10 m
rocks = make(N_ROCK, SPREAD * 2.0, 6.0, 16.0, 0.06, 0.2, 0.3)     # as pedras visíveis da arte, em arco


def state(s, t):
    """posição e velocidade em t (balística com arrasto linear k: v' = −k v − g)."""
    tt = np.maximum(t - s['t0'], 0.0)[:, None]
    k = s['drag']
    g = np.array([0, 0, -G])
    e = np.exp(-k * tt)
    v = (s['v0'] + g / k) * e - g / k
    p = s['p0'] + (s['v0'] + g / k) * (1 - e) / k - g / k * tt
    alive = (t >= s['t0']) & (p[:, 2] > -0.05)
    return p, v, alive


# ---- cena -------------------------------------------------------------------------------------
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
mat = bpy.data.materials.new('Grain')
mat.use_nodes = True
bsdf = mat.node_tree.nodes['Principled BSDF'] if 'Principled BSDF' in mat.node_tree.nodes else None
if bsdf is None:
    mat.node_tree.nodes.clear()
    bsdf = mat.node_tree.nodes.new('ShaderNodeBsdfPrincipled')
    out = mat.node_tree.nodes.new('ShaderNodeOutputMaterial')
    mat.node_tree.links.new(bsdf.outputs[0], out.inputs['Surface'])
bsdf.inputs['Base Color'].default_value = (0.3, 0.3, 0.3, 1.0)       # cinza: a cor da areia entra no material do jogo
bsdf.inputs['Roughness'].default_value = 0.9

# enquadramento: a caixa do leque em todo o tempo (direita = x, cima = z na câmera de lado inclinada)
ELEV = math.radians(35)
fwd = Vector((0.0, 1.0, 0.0))                       # a câmera olha pra +Y; x é a direita do quadro
fwd = Vector((0.0, math.cos(ELEV), -math.sin(ELEV)))
right = Vector((1.0, 0.0, 0.0))
up = right.cross(fwd).normalized() * -1.0
if up.z < 0:
    up = -up
pts = []
for k in range(NFRAMES):
    p, _, al = state(grains, k / FPS)
    pts.append(p[al])
allp = np.concatenate(pts)
pr = allp @ np.array(right)
pu = allp @ np.array(up)
lo_r, hi_r = np.percentile(pr, 1), np.percentile(pr, 99)          # o leque; os grãos soltos nas pontas podem sair
lo_u, hi_u = min(np.percentile(pu, 1), -0.3), np.percentile(pu, 99)
SIDE = max(hi_r - lo_r, hi_u - lo_u) * 1.08
center = right * ((lo_r + hi_r) / 2) + up * ((lo_u + hi_u) / 2)
print(f'DEBRIS enquadramento: lado {SIDE:.2f} m')

cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
sc.collection.objects.link(cam)
sc.camera = cam
cam.data.type = 'ORTHO'
cam.data.ortho_scale = SIDE
cam.data.clip_end = 500.0
cam.location = center - fwd * 100.0
cam.rotation_euler = fwd.to_track_quat('-Z', 'Y').to_euler()

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
sc.cycles.device = 'CPU'
sc.cycles.samples = SAMPLES
sc.cycles.use_denoising = False
sc.render.film_transparent = True
sc.render.resolution_x = sc.render.resolution_y = RES
sc.view_settings.view_transform = 'Standard'
if hasattr(sc.render.image_settings, 'media_type'):
    sc.render.image_settings.media_type = 'MULTI_LAYER_IMAGE'
sc.render.image_settings.file_format = 'OPEN_EXR_MULTILAYER'
sc.render.image_settings.color_depth = '16'

# um elipsoide-base (icosfera) esticado por grão
base = bmesh.new()
bmesh.ops.create_icosphere(base, subdivisions=1, radius=1.0)
base_v = np.array([v.co[:] for v in base.verts])
base_f = [[v.index for v in f.verts] for f in base.faces]
base.free()


def frame_mesh(t):
    verts, faces = [], []
    for s in (grains, rocks):
        p, v, al = state(s, t)
        for i in np.nonzero(al)[0]:
            sp = np.linalg.norm(v[i])
            d = v[i] / sp if sp > 1e-4 else np.array([0, 0, 1.0])
            r = s['size'][i] * 0.5
            stretch = max(r, sp * SHUTTER * 0.5)               # o borrão de movimento, na geometria
            # base ortonormal com x ao longo da velocidade
            a2 = np.cross(d, [0, 0, 1.0]) if abs(d[2]) < 0.95 else np.cross(d, [1.0, 0, 0])
            a2 /= np.linalg.norm(a2)
            a3 = np.cross(d, a2)
            M = np.stack([d * stretch, a2 * r, a3 * r], 1)
            o = len(verts)
            verts.extend((base_v @ M.T + p[i]).tolist())
            faces.extend([[o + j for j in f] for f in base_f])
    me = bpy.data.meshes.new('grains')
    me.from_pydata(verts, [], faces)
    me.materials.append(mat)
    return me


def read_passes(path):
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


rows = -(-NFRAMES // COLS)
atl_pos = np.zeros((rows * RES, COLS * RES, 4), np.float32)
atl_neg = np.zeros_like(atl_pos)
atl_emit = np.zeros_like(atl_pos)
times = []
for k in range(NFRAMES):
    t = k / FPS
    me = frame_mesh(t)
    ob = bpy.data.objects.new('grains', me)
    sc.collection.objects.link(ob)
    path = os.path.join(OUT, f'f{k:03d}.exr')
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(ob, do_unlink=True)
    bpy.data.meshes.remove(me)
    ch = read_passes(path)
    lum = {n: (ch[f'Combined_{n}.R'] + ch[f'Combined_{n}.G'] + ch[f'Combined_{n}.B']) / 3 for n in DIRS}
    a = ch['Combined.A']
    r, c = divmod(k, COLS)
    sl = (slice(r * RES, (r + 1) * RES), slice(c * RES, (c + 1) * RES))
    atl_pos[sl] = np.stack([lum['px'], lum['py'], lum['pz'], a], -1)
    atl_neg[sl] = np.stack([lum['nx'], lum['ny'], lum['nz'], a], -1)
    atl_emit[sl] = np.stack([a * 0, a * 0, a * 0, np.ones_like(a)], -1)
    times.append(t)
    print(f'DEBRIS {k + 1}/{NFRAMES} t={t:.2f}s')


def write(name, arr):
    o = oiio.ImageOutput.create(os.path.join(OUT, name))
    o.open(os.path.join(OUT, name), oiio.ImageSpec(arr.shape[1], arr.shape[0], 4, oiio.HALF))
    o.write_image(arr)
    o.close()


write('six_pos.exr', atl_pos)
write('six_neg.exr', atl_neg)
write('emit.exr', atl_emit)
sun = 0.55 * atl_pos[..., 0] + 0.6 * atl_pos[..., 1] + 0.45 * atl_pos[..., 2]
lit = (sun * 3.0)[..., None] * np.array([0.5, 0.42, 0.33])
comp = (1 - np.exp(-lit)) + np.array([0.35, 0.33, 0.31]) * (1 - atl_pos[..., 3:4])
img = (np.clip(comp, 0, 1) ** (1 / 2.2) * 255).astype(np.uint8)
o = oiio.ImageOutput.create(os.path.join(OUT, 'contact.png'))
o.open(os.path.join(OUT, 'contact.png'), oiio.ImageSpec(img.shape[1], img.shape[0], 3, oiio.UINT8))
o.write_image(img)
o.close()
# o ponto do impacto no quadro (o pivô): a origem projetada
pv_r = (0.0 - (np.array(center) @ np.array(right))) / SIDE + 0.5
pv_u = (0.0 - (np.array(center) @ np.array(up))) / SIDE + 0.5
json.dump({'cols': COLS, 'rows': rows, 'frames': NFRAMES, 'res': RES, 'times_s': times, 'side_m': SIDE,
           'camera_elev_deg': math.degrees(ELEV), 'albedo': 0.3, 'pivot': [pv_r, pv_u], 'ball_bottom': [0.0] * NFRAMES,
           'fire_end': NFRAMES - 1, 'sim_frames': list(range(NFRAMES)),
           'six_pos': 'R=+right G=+up B=+front A=alfa', 'six_neg': 'R=-right G=-up B=back A=alfa', 'alpha': 'premultiplied',
           'source': 'bake_debris.py (grãos balísticos com arrasto, borrão de movimento na geometria)'},
          open(os.path.join(OUT, 'atlas.json'), 'w'), indent=1)
print('DEBRIS pronto ->', OUT)
