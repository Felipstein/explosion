"""Fogo da carcaça do tanque (contínuo, pra tocar em loop) no Mantaflow do Blender.

Uso:
    blender -b --factory-startup -P blender_pyro_wreck.py -- <saida_dir> [resolucao] [quadros]

O vídeo-alvo, de 1,3 a 4 s, mostra a base do tanque com fogo forte e brilhante
(bolas de fogo rolando do casco, ~0,6 × 0,7 tanque) que liga a coluna preta ao
casco. A bola da explosão é outra fonte (EmberGen); isto é só o que fica
queimando. O conversor (convert_pyro_vdb.py ... loop) recorta uma janela e
emenda o fim no começo.

Cena: casco de 7,9 × 3,6 × 1,6 m como obstáculo; combustível contínuo no
convés (escotilha aberta) e dois respiros que pulsam em tempos sorteados
(as "bolas" do vídeo); chão fechado; domínio de 8 × 8 × 12 m.
"""
import os
import random
import sys
import time

import bpy

argv = sys.argv[sys.argv.index('--') + 1:]
OUT = os.path.abspath(argv[0])
RES = int(argv[1]) if len(argv) > 1 else 128
FRAMES = int(argv[2]) if len(argv) > 2 else 240
FPS = 30
os.makedirs(OUT, exist_ok=True)
random.seed(11)

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
    bpy.ops.mesh.primitive_uv_sphere_add(radius=r, location=loc, segments=20, ring_count=10)
    ob = bpy.context.object
    ob.name = name
    return ob


DOM = (8.0, 8.0, 12.0)
dom = box('Domain', DOM, (0.0, 0.0, DOM[2] / 2))
m = dom.modifiers.new('Fluid', 'FLUID')
m.fluid_type = 'DOMAIN'
d = m.domain_settings
d.domain_type = 'GAS'
d.resolution_max = RES
d.use_adaptive_domain = False
d.cfl_condition = 4.0
d.timesteps_max = 8
d.timesteps_min = 2
d.use_collision_border_bottom = True
for side in ('front', 'back', 'left', 'right', 'top'):
    setattr(d, f'use_collision_border_{side}', False)
d.alpha = 0.0
d.beta = 3.0
d.vorticity = 0.5
d.use_dissolve_smoke = False
d.burning_rate = 0.55   # chama mais alta: ~0,7 tanque (5,5 m) no vídeo
d.flame_smoke = 1.3
d.flame_vorticity = 0.6
d.flame_ignition = 1.2
d.flame_max_temp = 3.0
d.flame_smoke_color = (0.02, 0.02, 0.02)
d.cache_type = 'ALL'
d.cache_data_format = 'OPENVDB'
d.openvdb_cache_compress_type = 'BLOSC'
d.openvdb_data_depth = '16'
d.cache_directory = os.path.join(OUT, 'cache')
d.cache_frame_start = 1
d.cache_frame_end = FRAMES

hull = box('Hull', (7.9, 3.6, 1.6), (0.0, 0.0, 1.0))
e = hull.modifiers.new('Fluid', 'FLUID')
e.fluid_type = 'EFFECTOR'
e.effector_settings.effector_type = 'COLLISION'


def flow(ob, fuel, temp, vel_z, keys):
    """keys: [(quadro, fator)] — o fluxo é animado em fuel/temperatura/densidade."""
    mod = ob.modifiers.new('Fluid', 'FLUID')
    mod.fluid_type = 'FLOW'
    f = mod.flow_settings
    f.flow_type = 'BOTH'
    f.flow_behavior = 'INFLOW'
    f.flow_source = 'MESH'
    f.surface_distance = 0.4
    f.use_initial_velocity = True
    f.velocity_coord = (0.0, 0.0, vel_z)
    f.smoke_color = (0.03, 0.03, 0.03)
    f.subframes = 1
    for frame, k in keys:
        f.fuel_amount = fuel * k
        f.temperature = temp * k
        f.density = 0.4 * k
        for prop in ('fuel_amount', 'temperature', 'density'):
            f.keyframe_insert(prop, frame=frame)


# a escotilha aberta: contínua, com uma oscilação lenta (a chama respira)
hatch = box('Hatch', (3.0, 2.2, 0.3), (0.4, 0.0, 1.95))
keys = [(f, 0.75 + 0.25 * random.random()) for f in range(1, FRAMES + 1, 6)]
flow(hatch, fuel=3.0, temp=3.0, vel_z=4.5, keys=keys)
# dois respiros (motor e torre arrancada) com bolas de fogo em tempos sorteados
for i, loc in enumerate(((-2.6, 0.6, 1.9), (1.8, -0.9, 1.9))):
    vent = sphere(f'Vent{i}', 0.9, loc)
    keys, f = [(1, 0.0)], 4 + i * 7
    while f < FRAMES:
        keys += [(f, 0.0), (f + 1, 1.0), (f + 4, 1.0), (f + 6, 0.0)]
        f += random.randint(14, 30)                      # uma bola a cada ~0,5–1 s
    flow(vent, fuel=5.0, temp=3.5, vel_z=8.0, keys=keys)

bpy.context.view_layer.objects.active = dom
dom.select_set(True)
t0 = time.time()
with bpy.context.temp_override(object=dom, active_object=dom):
    bpy.ops.fluid.bake_all()
size = sum(os.path.getsize(os.path.join(r, f)) for r, _, fs in os.walk(os.path.join(OUT, 'cache')) for f in fs)
print(f'PYRO wreck res {RES} frames {FRAMES}: {(time.time() - t0) / 60:.1f} min, cache {size / 1e6:.0f} MB')
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, 'pyro.blend'))
