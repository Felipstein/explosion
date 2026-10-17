"""A carcaça de tanque queimando, simulada no Mantaflow do Blender (17/10 madrugada, o item 9 da fila; docs/ESTUDO_CARCACA.md).

Uso:
    blender -b --factory-startup -P blender_pyro_wreck.py -- <saida_dir> [chave=valor ...]

O fenômeno (docs/ESTUDO_CARCACA.md, com as fontes): o diesel, o hidráulico e a borracha queimando no anel da torre
(arrancada) e na grade do motor, 5–15 MW [E]; chama de 1,5–3 m nos alvos (Heskestad dá até ~4–5 m no pico), pulsando a
~1 Hz (f ≈ 1,5/√D); a fumaça preta de fuligem (albedo ~0,2–0,3) subindo a ~6 m/s, densa nos primeiros metros e rala
acima. Nos alvos o ar está quase parado: a simulação é sem vento, e a inclinação (10–25°) entra na conversão
(build_raymarch_vdb.py, o cisalhamento por altura) e o rumo no jogo (o giro da instância pro vento do mapa).

A cena (metros, Z pra cima, chão fechado em z = 0, o casco no meio):
- CASCO: um bloco de 8,1 × 3,8 × 1,8 m como obstáculo (a fumaça contorna, a chama sai de cima);
- ANEL: 'anel_n' fontes num disco de 1,1 m no teto (o buraco da torre, ~70% do combustível);
- MOTOR: 'motor_n' fontes na grade de trás (1,5 × 1 m, ~30%);
- cada fonte com o combustível sorteado a cada 0,3–0,6 s (a chama não sai de um retângulo liso; 13/10: "retângulo").
O combustível sobe nos primeiros 'subida_s' s e fica constante: a janela do loop (build_raymarch_vdb.py 'loop') sai do
trecho estável. Cache OpenVDB (density, flame, temperature, velocity).
"""
import math
import os
import random
import sys
import time

import bpy

argv = sys.argv[sys.argv.index('--') + 1:]
OUT = os.path.abspath(argv[0])
PARAMS = {
    'res': 160, 'segundos': 20.0, 'fps': 10,
    'largura_x': 16.0, 'largura_y': 12.0, 'altura': 24.0,
    'casco_x': 8.1, 'casco_y': 3.8, 'casco_z': 1.8,
    'anel_r': 1.1, 'anel_n': 7, 'motor_n': 4, 'motor_x': 1.5, 'motor_y': 1.0,
    'combustivel': 1.0, 'calor': 1.6, 'fumaca': 0.6, 'sobe': 2.0, 'subida_s': 2.0,
    'queima': 0.75, 'fumaca_chama': 1.6, 'vort_chama': 0.5, 'ignicao': 1.25, 'temp_max': 2.0,
    'alpha': 0.0, 'beta': 1.0, 'vort': 0.4, 'dissolve': 0,
    'tempo': 3.6, 'semente': 5,
}
for a in argv[1:]:
    k, v = a.split('=', 1)
    PARAMS[k] = type(PARAMS[k])(float(v)) if k in PARAMS else float(v)
P = PARAMS
os.makedirs(OUT, exist_ok=True)

sc = bpy.context.scene
for ob in list(bpy.data.objects):
    bpy.data.objects.remove(ob, do_unlink=True)
fps = int(P['fps'])
frames = int(round(P['segundos'] * fps))
sc.render.fps = fps
sc.frame_start, sc.frame_end = 1, frames


def mesh(op, name, **kw):
    op(**kw)
    ob = bpy.context.object
    ob.name = name
    return ob


Wx, Wy, H = P['largura_x'], P['largura_y'], P['altura']
dom = mesh(bpy.ops.mesh.primitive_cube_add, 'Domain', size=1.0, location=(0.0, 0.0, H / 2))
dom.scale = (Wx, Wy, H)
bpy.ops.object.transform_apply(scale=True)
m = dom.modifiers.new('Fluid', 'FLUID')
m.fluid_type = 'DOMAIN'
d = m.domain_settings
d.domain_type = 'GAS'
d.resolution_max = int(P['res'])
d.use_adaptive_domain = False
d.time_scale = P['tempo']
d.cfl_condition = 4.0
d.timesteps_max = 12
d.timesteps_min = 2
d.use_collision_border_bottom = True
for side in ('front', 'back', 'left', 'right', 'top'):
    setattr(d, f'use_collision_border_{side}', False)
d.alpha = P['alpha']
d.beta = P['beta']
d.vorticity = P['vort']
d.burning_rate = P['queima']
d.flame_smoke = P['fumaca_chama']
d.flame_vorticity = P['vort_chama']
d.flame_ignition = P['ignicao']
d.flame_max_temp = P['temp_max']
d.flame_smoke_color = (0.08, 0.08, 0.08)
d.use_dissolve_smoke = P['dissolve'] > 0
if P['dissolve'] > 0:
    d.dissolve_speed = int(P['dissolve'])
    d.use_dissolve_smoke_log = True
d.cache_type = 'ALL'
d.cache_data_format = 'OPENVDB'
d.cache_noise_format = 'OPENVDB'
d.openvdb_cache_compress_type = 'BLOSC'
d.openvdb_data_depth = '16'
d.cache_directory = os.path.join(OUT, 'cache')
d.cache_frame_start = 1
d.cache_frame_end = frames

# o casco (obstáculo)
hull = mesh(bpy.ops.mesh.primitive_cube_add, 'Hull', size=1.0, location=(0.0, 0.0, P['casco_z'] / 2))
hull.scale = (P['casco_x'], P['casco_y'], P['casco_z'])
bpy.ops.object.transform_apply(scale=True)
hm = hull.modifiers.new('Fluid', 'FLUID')
hm.fluid_type = 'EFFECTOR'
hm.effector_settings.effector_type = 'COLLISION'

rnd = random.Random(int(P['semente']))


def fire_source(name, x, y, w, share):
    """Uma boca de fogo em cima do casco: combustível e calor sorteados a cada 0,3–0,6 s, subindo nos primeiros s."""
    b = mesh(bpy.ops.mesh.primitive_cube_add, name, size=1.0, location=(x, y, P['casco_z'] + 0.12))
    b.scale = (w, w, 0.2)
    bpy.ops.object.transform_apply(scale=True)
    mod = b.modifiers.new('Fluid', 'FLUID')
    mod.fluid_type = 'FLOW'
    f = mod.flow_settings
    f.flow_type = 'BOTH'
    f.flow_behavior = 'INFLOW'
    f.flow_source = 'MESH'
    f.surface_distance = 0.15
    f.use_initial_velocity = True
    f.velocity_coord = (0.0, 0.0, P['sobe'])
    f.smoke_color = (0.08, 0.08, 0.08)
    f.subframes = 1
    t = 0.0
    while t <= P['segundos'] + 0.5:
        ramp = min(1.0, t / max(P['subida_s'], 0.01))
        k = ramp * share * rnd.uniform(0.35, 1.0)
        frame = 1 + int(round(t * fps))
        f.fuel_amount = P['combustivel'] * k
        f.temperature = P['calor'] * (0.5 + 0.5 * k / max(share, 1e-3))
        f.density = P['fumaca'] * k
        for prop in ('fuel_amount', 'temperature', 'density'):
            f.keyframe_insert(prop, frame=frame)
        t += rnd.uniform(0.3, 0.6)


# o anel da torre: fontes num disco de anel_r (a torre foi arrancada), um pouco à frente do meio do casco
n = int(P['anel_n'])
for i in range(n):
    a = 2 * math.pi * (i + rnd.uniform(-0.3, 0.3)) / n
    r = P['anel_r'] * math.sqrt(rnd.uniform(0.1, 1.0))
    fire_source(f'Ring{i}', 0.6 + r * math.cos(a), r * math.sin(a), 0.55, 0.7 * 7.0 / n)
# a grade do motor, atrás
for i in range(int(P['motor_n'])):
    fire_source(f'Engine{i}', -P['casco_x'] / 2 + 1.2 + rnd.uniform(-0.5, 0.5) * P['motor_x'], rnd.uniform(-0.5, 0.5) * P['motor_y'] * 2,
                0.45, 0.3 * 4.0 / max(int(P['motor_n']), 1))

bpy.context.view_layer.objects.active = dom
dom.select_set(True)
t0 = time.time()
with bpy.context.temp_override(object=dom, active_object=dom):
    bpy.ops.fluid.bake_all()
dt = time.time() - t0
size = sum(os.path.getsize(os.path.join(r, n_)) for r, _, fs in os.walk(os.path.join(OUT, 'cache')) for n_ in fs)
print(f'PYRO carcaca res {int(P["res"])} quadros {frames}: {dt / 60:.1f} min, cache {size / 1e6:.0f} MB, {P}')
