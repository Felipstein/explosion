"""O tiro de canhão na areia simulado no Mantaflow do Blender (16/10, a arte 4.1; o item 2 da fila em docs/TIRO_E_ACERTO.md).

Uso:
    blender -b --factory-startup -P blender_pyro_ground.py -- <saida_dir> [chave=valor ...]

Por que simular (16/10): as versões de jogo (v5, v6) esticavam gravações feitas pra outra coisa (a saia da morte de
tanque e uma poeira da JangaFX) e saíam com três defeitos que nenhum ajuste tira: a nuvem nascia pronta (o relógio
rápido do começo), o anel virava um domo creme cheio e o jato de terra era uma bola marrom.

O fenômeno (docs/ESTUDO_ONDA_DE_CHOQUE.md, docs/TIRO_E_ACERTO.md "O tiro na areia"), uma granada de 120 mm de 1–2,5 kg:
- o choque passa por cima da areia em poucos ms e a levanta NO LUGAR (o ar só anda décimos de metro) até onde ainda tem
  força (~7–10 m pra essa carga, Kingery-Bulmash): uma camada fina de poeira em todo o disco, mais densa perto;
- o gás da detonação sai do buraco e varre o chão pra fora: empurra a poeira de perto e a junta num anel que rola, com
  o meio limpo (a cratera e o jato ficam no meio), a 2 m de altura no máximo;
- o jato de terra escura (a areia de baixo, úmida e escura, e os torrões) sobe em leque alto, inclinado pra longe do
  atirador (a 4.1: mais alto que as picapes), e cai de volta;
- a areia é pesada: assenta (empuxo pela densidade negativo), não sobe como fumaça.

A cena (metros, Z pra cima, chão fechado em z = 0, o impacto na origem, o atirador do lado −X):
- SOPRO: meia esfera de 0,9 m no chão que só solta velocidade pra fora (sem densidade) nos 2 primeiros quadros;
- AREIA: dois anéis chatos no chão (0,8–3,5 m denso e 3,5–7 m ralo), a poeira levantada pelo choque, subindo devagar;
- JATO: um cilindro de 0,45 m no centro, a terra escura saindo rápido pra cima, inclinado 'inclina' graus pra +X.
A terra escura e a areia saem separadas: o jato sai com temperatura 1, que aqui é só uma marca (sem empuxo pelo calor
e sem queima, ela anda junto com a fumaça; o Blender 5.2 não grava o combustível na cache), e o build_raymarch_vdb.py
('terra') põe a parte marcada na fuligem (a terra escura) e o resto na poeira (a areia).
O sopro rente ao chão: 12 fontes em volta do centro soprando na horizontal pra fora (a meia esfera soprava pra cima
também e a areia virava um domo).
Cache OpenVDB (density, temperature, velocity) → build_raymarch_vdb.py.
"""
import math
import os
import sys
import time

import random

import bmesh
import bpy

argv = sys.argv[sys.argv.index('--') + 1:]
OUT = os.path.abspath(argv[0])
PARAMS = {
    'res': 128, 'quadros': 75, 'fps': 30,
    'largura': 24.0, 'altura': 12.0,       # o domínio (m)
    'sopro': 30.0, 'sopro_r': 1.0, 'sopro_up': 0.05,   # o gás saindo do buraco: pra fora (m/s), o raio das fontes, a fração pra cima
    'areia_vz': 3.0, 'areia_vn': 2.0,      # a poeira levantada pelo choque: subindo e um pouco pra fora (m/s)
    'areia': 1.0, 'areia_rala': 0.35,      # a densidade da areia varrida e a do disco ralo
    'anel_r': 1.5, 'anel_v': 20.0, 'anel_h': 0.6,   # a areia varrida: o raio das fontes, a velocidade pra fora (m/s) e a altura
    'jato': 30.0, 'jato_r': 0.6, 'inclina': 15.0, 'terra': 1.6, 'jato_quadros': 3,
    'alpha': -0.3, 'vort': 0.35,           # a areia assenta; os redemoinhos
    'noise': 0, 'semente': 7, 'dissolve': 40,
    'tempo': 3.0,                          # (calibrado 16/10: num domínio de 20 m o Mantaflow anda a ~1/3; 3 = tempo real)
}
for a in argv[1:]:
    k, v = a.split('=', 1)
    PARAMS[k] = type(PARAMS[k])(float(v)) if k in PARAMS else float(v)
P = PARAMS
os.makedirs(OUT, exist_ok=True)

sc = bpy.context.scene
for ob in list(bpy.data.objects):
    bpy.data.objects.remove(ob, do_unlink=True)
sc.render.fps = int(P['fps'])
sc.frame_start, sc.frame_end = 1, int(P['quadros'])


def mesh(op, name, **kw):
    op(**kw)
    ob = bpy.context.object
    ob.name = name
    return ob


def annulus(name, r0, r1, h, seg=96):
    """Um anel chato fechado (prisma entre r0 e r1, de z = 0 a h)."""
    me = bpy.data.meshes.new(name)
    bm = bmesh.new()
    rings = []
    for z in (0.0, h):
        for r in (r0, r1):
            rings.append([bm.verts.new((r * math.cos(2 * math.pi * i / seg), r * math.sin(2 * math.pi * i / seg), z)) for i in range(seg)])
    bi, bo, ti, to = rings
    for i in range(seg):
        j = (i + 1) % seg
        bm.faces.new((bi[i], bi[j], bo[j], bo[i]))      # baixo
        bm.faces.new((ti[i], to[i], to[j], ti[j]))      # cima
        bm.faces.new((bo[i], bo[j], to[j], to[i]))      # fora
        bm.faces.new((bi[i], ti[i], ti[j], bi[j]))      # dentro
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    sc.collection.objects.link(ob)
    return ob


def flow(ob, kind, density, mark, frames, vn=0.0, vcoord=(0.0, 0.0, 0.0), surface=0.4):
    """Fonte que solta por 'frames' quadros e para (densidade, a marca da terra escura e a velocidade inicial)."""
    mod = ob.modifiers.new('Fluid', 'FLUID')
    mod.fluid_type = 'FLOW'
    f = mod.flow_settings
    f.flow_type = kind
    f.flow_behavior = 'INFLOW'
    f.flow_source = 'MESH'
    f.surface_distance = surface
    f.volume_density = 1.0
    f.use_initial_velocity = True
    f.velocity_normal = vn
    f.velocity_coord = vcoord
    f.subframes = 2
    for frame, k in ((1, 1.0), (frames, 1.0), (frames + 1, 0.0)):
        f.density = density * k
        f.temperature = mark * k
        f.keyframe_insert('density', frame=frame)
        f.keyframe_insert('temperature', frame=frame)
    return mod


W, H = P['largura'], P['altura']
dom = mesh(bpy.ops.mesh.primitive_cube_add, 'Domain', size=1.0, location=(0.0, 0.0, H / 2))
dom.scale = (W, W, H)
bpy.ops.object.transform_apply(scale=True)
m = dom.modifiers.new('Fluid', 'FLUID')
m.fluid_type = 'DOMAIN'
d = m.domain_settings
d.domain_type = 'GAS'
d.resolution_max = int(P['res'])
d.use_adaptive_domain = False
d.time_scale = P['tempo']
d.cfl_condition = 4.0
d.timesteps_max = 10
d.timesteps_min = 2
d.use_collision_border_bottom = True
for side in ('front', 'back', 'left', 'right', 'top'):
    setattr(d, f'use_collision_border_{side}', False)
d.alpha = P['alpha']
d.beta = 0.0                            # a temperatura é só a marca da terra escura: não empurra
d.vorticity = P['vort']
# a poeira some com o tempo (dissolve logarítmico): a areia assenta, e a advecção do Mantaflow ganha massa onde a
# velocidade varia muito (16/10: a massa de areia ia de 645 a 1520 em 1,5 s sem fonte nenhuma, e a névoa rala subia a 10 m)
d.use_dissolve_smoke = P['dissolve'] > 0
if P['dissolve'] > 0:
    d.dissolve_speed = int(P['dissolve'])
    d.use_dissolve_smoke_log = True
d.use_noise = P['noise'] > 0
if P['noise'] > 0:
    d.noise_scale = int(P['noise'])
    d.noise_strength = 1.0
    d.noise_pos_scale = 2.0
d.cache_type = 'ALL'
d.cache_data_format = 'OPENVDB'
d.cache_noise_format = 'OPENVDB'
d.openvdb_cache_compress_type = 'BLOSC'
d.openvdb_data_depth = '16'
d.cache_directory = os.path.join(OUT, 'cache')
d.cache_frame_start = 1
d.cache_frame_end = int(P['quadros'])

# o sopro rente ao chão: 12 fontes em volta do centro, cada uma soprando na horizontal pra fora (um pouco pra cima),
# nos 2 primeiros quadros, com um fiapo de poeira (sem densidade a fonte não leva a velocidade)
# (16/10: em números primos e com o ângulo, a posição e a força sorteados: com 12 e 16 iguais a grade do Mantaflow
# juntava tudo em 4 gomos, um X)
rnd = random.Random(int(P['semente']))
for i in range(13):
    ang = 2.0 * math.pi * (i + rnd.uniform(-0.3, 0.3)) / 13
    ux, uy = math.cos(ang), math.sin(ang)
    r = P['sopro_r'] * rnd.uniform(0.85, 1.15)
    b = mesh(bpy.ops.mesh.primitive_cube_add, f'Blast{i}', size=1.0, location=(ux * r, uy * r, 0.45))
    b.scale = (0.7, 0.7, 0.6)
    b.rotation_euler = (0.0, 0.0, ang)
    bpy.ops.object.transform_apply(scale=True, rotation=True)
    v = P['sopro'] * rnd.uniform(0.8, 1.2)
    flow(b, 'SMOKE', 0.02, 0.0, 2, vcoord=(ux * v, uy * v, v * P['sopro_up']))
# a areia de perto varrida pelo gás: 16 fontes num círculo, soprando a areia na horizontal pra fora (a frente do anel)
for i in range(17):
    ang = 2.0 * math.pi * (i + 0.5 + rnd.uniform(-0.3, 0.3)) / 17
    ux, uy = math.cos(ang), math.sin(ang)
    r = P['anel_r'] * rnd.uniform(0.85, 1.15)
    b = mesh(bpy.ops.mesh.primitive_cube_add, f'SandNear{i}', size=1.0, location=(ux * r, uy * r, 0.45))
    b.scale = (0.9, 2.0 * math.pi * P['anel_r'] / 17 * 1.1, P['anel_h'])
    b.rotation_euler = (0.0, 0.0, ang)
    bpy.ops.object.transform_apply(scale=True, rotation=True)
    v = P['anel_v'] * rnd.uniform(0.75, 1.25)
    flow(b, 'SMOKE', P['areia'] * rnd.uniform(0.7, 1.3), 0.0, 2, vcoord=(ux * v, uy * v, P['areia_vz'] * rnd.uniform(0.6, 1.4)))
# a de longe levantada no lugar pelo choque: três anéis ralos com a densidade caindo pra fora (16/10: um disco só tinha
# a borda dura, um círculo nítido no chão), subindo devagar
for i, (r0, r1, k) in enumerate(((3.0, 4.5, 1.0), (4.5, 6.0, 0.55), (6.0, 7.5, 0.25))):
    far = annulus(f'SandFar{i}', r0, r1, 0.5)
    flow(far, 'SMOKE', P['areia_rala'] * k, 0.0, 2, vn=P['areia_vn'] * 0.5, vcoord=(0.0, 0.0, P['areia_vz'] * 0.6))
# o jato de terra escura: pra cima, inclinado pra longe do atirador (+X)
a = math.radians(P['inclina'])
jet = mesh(bpy.ops.mesh.primitive_cylinder_add, 'Jet', radius=P['jato_r'], depth=0.8, location=(0.0, 0.0, 0.5), vertices=24)
flow(jet, 'SMOKE', P['terra'], 1.0, int(P['jato_quadros']), vcoord=(P['jato'] * math.sin(a), 0.0, P['jato'] * math.cos(a)))

bpy.context.view_layer.objects.active = dom
dom.select_set(True)
t0 = time.time()
with bpy.context.temp_override(object=dom, active_object=dom):
    bpy.ops.fluid.bake_all()
dt = time.time() - t0
size = sum(os.path.getsize(os.path.join(r, n)) for r, _, fs in os.walk(os.path.join(OUT, 'cache')) for n in fs)
print(f'PYRO chao res {int(P["res"])} quadros {int(P["quadros"])}: {dt / 60:.1f} min, cache {size / 1e6:.0f} MB, {P}')
