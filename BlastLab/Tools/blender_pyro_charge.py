"""A Carga Pesada simulada no Mantaflow do Blender (a comparação "quem grava", 09/10).

Uso:
    blender -b --factory-startup -P blender_pyro_charge.py -- <saida_dir> [chave=valor ...]
    (res=64 quadros=90 ... ; ver PARAMS)

A mesma explosão do simulador do WebGL (reference/webgl/js/fluid.js DEFAULTS),
no que o Mantaflow tem: um núcleo de combustível de 3,2 m de raio a 1,9 m do
chão (coreRadius, detonate pos), quente, que solta tudo em ~0,1 s (blastTau)
com velocidade pra fora e pra cima. O Mantaflow não tem a expansão da queima
(a fonte de divergência do fluid.js, expansion 2.6 e blastSrc 96/s): o soco
vem só da velocidade inicial. Empuxo pelo calor (beta), vorticidade e a queima
são ajustados até o TAMANHO bater com a gravação do WebGL nos mesmos instantes
(caixa justa da r96_v0: 14 × 9 m aos 0,3 s, 16 × 19 m aos 1,2 s, 19 × 26,5 m
aos 1,8 s, 23 × 34 m aos 2,5 s). Domínio de 38 m como a do WebGL, chão fechado.
Cache OpenVDB (density, flame, temperature, velocity) → build_raymarch_vdb.py.
"""
import os
import sys
import time

import bpy

argv = sys.argv[sys.argv.index('--') + 1:]
OUT = os.path.abspath(argv[0])
PARAMS = {
    'res': 64, 'quadros': 90, 'fps': 30,
    'raio': 3.2, 'altura': 1.9,            # o núcleo (fluid.js coreRadius, detonate pos)
    'fuel': 3.0, 'temp': 3.0, 'fumaca': 0.2,
    'vn': 22.0, 'vz': 8.0, 'emite': 3,     # velocidade inicial (m/s) e por quantos quadros solta
    'beta': 3.0, 'vort': 0.6,
    'queima': 0.5, 'fuligem': 1.6, 'fvort': 0.3, 'tmax': 3.0,
    'noise': 0,
    'tempo': 1.0,                          # time_scale: o Mantaflow anda devagar num domínio grande
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


D = 38.0
dom = mesh(bpy.ops.mesh.primitive_cube_add, 'Domain', size=1.0, location=(0.0, 0.0, D / 2))
dom.scale = (D, D, D)
bpy.ops.object.transform_apply(scale=True)
m = dom.modifiers.new('Fluid', 'FLUID')
m.fluid_type = 'DOMAIN'
d = m.domain_settings
d.domain_type = 'GAS'
d.resolution_max = int(P['res'])
d.use_adaptive_domain = False
d.time_scale = P['tempo']
d.cfl_condition = 4.0
d.timesteps_max = 8
d.timesteps_min = 2
d.use_collision_border_bottom = True
for side in ('front', 'back', 'left', 'right', 'top'):
    setattr(d, f'use_collision_border_{side}', False)
d.alpha = 0.0
d.beta = P['beta']
d.vorticity = P['vort']
d.use_dissolve_smoke = False
d.burning_rate = P['queima']
d.flame_smoke = P['fuligem']
d.flame_vorticity = P['fvort']
d.flame_ignition = 1.2
d.flame_max_temp = P['tmax']
d.flame_smoke_color = (0.02, 0.02, 0.02)
d.use_noise = P['noise'] > 0
if P['noise'] > 0:
    d.noise_scale = int(P['noise'])
    d.noise_strength = 1.2
    d.noise_pos_scale = 2.0
d.cache_type = 'ALL'
d.cache_data_format = 'OPENVDB'
d.cache_noise_format = 'OPENVDB'
d.openvdb_cache_compress_type = 'BLOSC'
d.openvdb_data_depth = '16'
d.cache_directory = os.path.join(OUT, 'cache')
d.cache_frame_start = 1
d.cache_frame_end = int(P['quadros'])

core = mesh(bpy.ops.mesh.primitive_uv_sphere_add, 'Core', radius=P['raio'], location=(0.0, 0.0, P['altura']),
            segments=32, ring_count=16)
mod = core.modifiers.new('Fluid', 'FLUID')
mod.fluid_type = 'FLOW'
f = mod.flow_settings
f.flow_type = 'BOTH'
f.flow_behavior = 'INFLOW'
f.flow_source = 'MESH'
f.surface_distance = 0.5
f.volume_density = 1.0
f.use_initial_velocity = True
f.velocity_normal = P['vn']
f.velocity_coord = (0.0, 0.0, P['vz'])
f.smoke_color = (0.03, 0.03, 0.03)
f.subframes = 3
stop = int(P['emite'])
for frame, k in ((1, 1.0), (stop, 1.0), (stop + 1, 0.0)):
    f.fuel_amount = P['fuel'] * k
    f.temperature = P['temp'] * k
    f.density = P['fumaca'] * k
    for prop in ('fuel_amount', 'temperature', 'density'):
        f.keyframe_insert(prop, frame=frame)

bpy.context.view_layer.objects.active = dom
dom.select_set(True)
t0 = time.time()
with bpy.context.temp_override(object=dom, active_object=dom):
    bpy.ops.fluid.bake_all()
dt = time.time() - t0
size = sum(os.path.getsize(os.path.join(r, n)) for r, _, fs in os.walk(os.path.join(OUT, 'cache')) for n in fs)
print(f'PYRO carga res {int(P["res"])} quadros {int(P["quadros"])}: {dt / 60:.1f} min, cache {size / 1e6:.0f} MB, {P}')
