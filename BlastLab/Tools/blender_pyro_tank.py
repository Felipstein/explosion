"""Simulação offline da morte de tanque (munição explodindo) no Mantaflow do Blender.

Uso:
    blender -b --factory-startup -P blender_pyro_tank.py -- <saida_dir> [resolucao] [quadros] [noise]

Recomeço da bola de fogo (07/10/2026): em vez de uma gravação grossa de 96³
com ruído falso por cima, uma simulação de gás com combustão em alta resolução
que já sai com a couve-flor, a casca de fuligem e o tronco de fogo. O cache
sai em OpenVDB (density, flame, heat, velocity), que a Unreal importa como
Sparse Volume Texture.

Unidades: metros, Z pra cima, chão em z = 0, tanque centrado na origem
(casco 7,9 × 3,6 × 1,8 m, anel da torre a ~2 m).

A cena:
- a CARGA: uma esfera de combustível no anel da torre, que solta tudo em ~0,15 s
  com velocidade pra cima e pra fora (a munição estourando pela escotilha);
- 4 jatos menores sorteados ao redor, que quebram a simetria (as
  sub-explosões e as escotilhas);
- a CARCAÇA: uma fonte fraca e contínua de fogo e fuligem em cima do casco,
  a partir de 0,4 s (o fogo que fica e alimenta o tronco e a coluna preta);
- o casco é um obstáculo (o gás contorna o tanque) e o chão é fechado.
"""
import math
import os
import random
import sys
import time

import bpy

argv = sys.argv[sys.argv.index('--') + 1:]
OUT = os.path.abspath(argv[0])
RES = int(argv[1]) if len(argv) > 1 else 128
FRAMES = int(argv[2]) if len(argv) > 2 else 120
NOISE = int(argv[3]) if len(argv) > 3 else 0
FPS = 30
os.makedirs(OUT, exist_ok=True)
random.seed(7)

sc = bpy.context.scene
for ob in list(bpy.data.objects):
    bpy.data.objects.remove(ob, do_unlink=True)
sc.render.fps = FPS
sc.frame_start, sc.frame_end = 1, FRAMES


def box(name, size, loc):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc)
    ob = bpy.context.object
    ob.name = name
    ob.scale = size
    bpy.ops.object.transform_apply(scale=True)
    return ob


def sphere(name, r, loc):
    bpy.ops.mesh.primitive_uv_sphere_add(radius=r, location=loc, segments=24, ring_count=12)
    ob = bpy.context.object
    ob.name = name
    return ob


# domínio: 26 × 26 m no chão, 32 m de altura (a bola do vídeo tem ~7–10 m e o
# topo passa de 11 m aos 1,8 s; a coluna acima disso segue no campo de fumaça do jogo)
DOM = (26.0, 26.0, 32.0)
dom = box('Domain', DOM, (0.0, 0.0, DOM[2] / 2))
m = dom.modifiers.new('Fluid', 'FLUID')
m.fluid_type = 'DOMAIN'
d = m.domain_settings
d.domain_type = 'GAS'
d.resolution_max = RES
d.use_adaptive_domain = False
d.time_scale = 1.0
d.cfl_condition = 4.0
d.timesteps_max = 8
d.timesteps_min = 2
d.use_collision_border_bottom = True
for side in ('front', 'back', 'left', 'right', 'top'):
    setattr(d, f'use_collision_border_{side}', False)
# fuligem e calor: empuxo pelo calor, fumaça pesada sobe devagar
d.alpha = 0.0           # densidade não puxa pra baixo
d.beta = 4.0            # calor sobe (o chapéu do vídeo sobe ~6 m/s no 1º segundo)
d.vorticity = 0.9
d.use_dissolve_smoke = False
# combustão: queima rápida, muita fuligem preta, chama girando
d.burning_rate = 0.42   # a bola queima até ~1,2 s (vídeo: branca até ~1 s, casca preta em 1,3–1,5 s)
d.flame_smoke = 1.4
d.flame_vorticity = 0.4   # alto vira línguas de fogueira; o alvo é couve-flor
d.flame_ignition = 1.2
d.flame_max_temp = 4.0
d.flame_smoke_color = (0.02, 0.02, 0.02)
d.use_noise = NOISE > 0
if NOISE:
    d.noise_scale = NOISE
    d.noise_strength = 1.2
    d.noise_pos_scale = 2.0
d.cache_type = 'ALL'
d.cache_data_format = 'OPENVDB'
d.cache_noise_format = 'OPENVDB'
d.openvdb_cache_compress_type = 'BLOSC'
d.openvdb_data_depth = '16'
d.cache_directory = os.path.join(OUT, 'cache')
d.cache_frame_start = 1
d.cache_frame_end = FRAMES

# o tanque como obstáculo
hull = box('Hull', (7.9, 3.6, 1.6), (0.0, 0.0, 1.0))
e = hull.modifiers.new('Fluid', 'FLUID')
e.fluid_type = 'EFFECTOR'
e.effector_settings.effector_type = 'COLLISION'


def flow(ob, fuel, temp, vel_normal, vel_z, smoke_density=1.0, start=1, stop=None, ramp=0):
    mod = ob.modifiers.new('Fluid', 'FLUID')
    mod.fluid_type = 'FLOW'
    f = mod.flow_settings
    f.flow_type = 'BOTH'
    f.flow_behavior = 'INFLOW'
    f.flow_source = 'MESH'
    f.surface_distance = 0.6
    f.use_initial_velocity = True
    f.velocity_normal = vel_normal
    f.velocity_coord = (0.0, 0.0, vel_z)
    f.smoke_color = (0.03, 0.03, 0.03)
    f.subframes = 2
    keys = [(max(1, start - 1), 0.0), (start, 1.0)]
    if stop:
        keys += [(stop, 1.0), (stop + 1 + ramp, 0.0)]
    for frame, k in keys:
        f.fuel_amount = fuel * k
        f.temperature = temp * k
        f.density = smoke_density * k
        for prop in ('fuel_amount', 'temperature', 'density'):
            f.keyframe_insert(prop, frame=frame)
    return f


# a carga principal: estoura em 0,15 s pelo anel da torre
charge = sphere('Charge', 2.2, (0.0, 0.0, 2.6))
flow(charge, fuel=7.0, temp=5.0, vel_normal=15.0, vel_z=18.0, smoke_density=0.3, start=1, stop=5, ramp=1)
# jatos secundários: assimetria (escotilhas, sub-explosões até ~1 s)
for i in range(4):
    a = random.uniform(0, 2 * math.pi)
    r = random.uniform(1.0, 3.0)
    s = sphere(f'Jet{i}', random.uniform(0.6, 1.0), (math.cos(a) * r, math.sin(a) * r * 0.5, random.uniform(1.8, 3.0)))
    st = random.randint(2, 28)
    flow(s, fuel=2.0, temp=3.0, vel_normal=random.uniform(6, 12), vel_z=random.uniform(4, 10), smoke_density=0.8,
         start=st, stop=st + random.randint(1, 3), ramp=1)
# a carcaça queimando: fonte contínua, fraca, a partir de 0,4 s
wreck = box('Wreck', (3.0, 2.0, 0.6), (0.3, 0.0, 2.1))
flow(wreck, fuel=1.4, temp=3.0, vel_normal=0.5, vel_z=4.0, smoke_density=1.2, start=12)

bpy.context.view_layer.objects.active = dom
dom.select_set(True)
t0 = time.time()
with bpy.context.temp_override(object=dom, active_object=dom):
    bpy.ops.fluid.bake_all()
dt = time.time() - t0
size = sum(os.path.getsize(os.path.join(r, f)) for r, _, fs in os.walk(os.path.join(OUT, 'cache')) for f in fs)
print(f'PYRO res {RES} noise {NOISE} frames {FRAMES}: {dt / 60:.1f} min, cache {size / 1e6:.0f} MB')
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, 'pyro.blend'))
