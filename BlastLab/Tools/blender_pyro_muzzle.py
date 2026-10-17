"""O disparo do canhão de tanque simulado no Mantaflow do Blender (16/10 noite, o item 8 da fila em docs/TIRO_E_ACERTO.md).

Uso:
    blender -b --factory-startup -P blender_pyro_muzzle.py -- <saida_dir> [chave=valor ...]

A referência que o Felipe elogiou (Downloads/gemini_generated_video_a0c04d8e.mp4, ~6,0 s): a bola branco-amarela colada
na boca por ~0,17 s, virando laranja, e a fumaça cinza aos ~0,4 s. A versão de antes (a morte de tanque em miniatura
solta à frente, cor de areia) foi reprovada: aqui o fenômeno é simulado, não emprestado.

O fenômeno (docs/ESTUDO_ONDA_DE_CHOQUE.md; Klingenberg e Heimerl, AIAA 1992):
- o jato de gás da pólvora sai pelo eixo do cano (os canhões de tanque não têm freio de boca: nada sai pros lados);
- o gás é rico em CO e H₂ e reacende quando se mistura com o ar: o clarão secundário, uma bola de 2–5 m que dura
  dezenas de ms;
- depois sobra a fumaça cinza-clara da pólvora, que se abre e vai com o vento;
- o evacuador ainda empurra um sopro fraco pela boca por ~0,3–1,5 s.

A cena (metros, Z pra cima, chão fechado em z = 0, a boca em (0, 0, 'altura_boca') apontando pra +X):
- JATO: cilindros curtos na boca soltando combustível e fumaça rápido pra +X nos primeiros quadros (o fogo e a bola),
  um no eixo e sub-jatos abertos até 'abre' graus com a força sorteada (com um só, o anel de vórtice sai limpo demais);
- RESTO: o do eixo soltando só fumaça devagar depois (o evacuador).
O tempo em dois andamentos: a escala de tempo do domínio fica em câmera lenta no começo (o fogo dura poucos quadros em
tempo real) e passa ao tempo real depois (a fumaça). No jogo, o volume toca os dois andamentos (TimeScale e
SlowAfterS/SlowTimeScale) e volta ao tempo real.
Cache OpenVDB (density, flame, temperature, velocity) → build_raymarch_vdb.py.
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
    'res': 160, 'quadros': 110, 'fps': 30,
    'comprimento': 14.0, 'largura': 6.0, 'altura': 6.0, 'atras': 2.0,   # o domínio (m): de −atras a comprimento−atras em X
    'altura_boca': 2.3, 'boca_r': 0.18,
    'jato': 120.0, 'jato_quadros': 2, 'combustivel': 1.6, 'fumaca_jato': 0.6, 'calor': 3.0,
    'resto': 6.0, 'resto_de': 6, 'resto_ate': 45, 'fumaca_resto': 0.35,
    'queima': 1.2, 'fumaca_chama': 1.4, 'vort_chama': 0.6, 'ignicao': 1.25, 'temp_max': 3.0,
    'alpha': 0.0, 'beta': 0.6, 'vort': 0.3, 'dissolve': 0,
    'jatos': 5, 'abre': 7.0, 'semente': 11,                            # a quebra da simetria: sub-jatos com rumo e força sorteados
    'lento': 0.53, 'real': 2.1, 'lento_ate': 36, 'real_de': 46,     # a escala de tempo (calibrada: 20 m → 3,0 = real; aqui 14 m → ~2,1)
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


Lx, Wy, Hz = P['comprimento'], P['largura'], P['altura']
dom = mesh(bpy.ops.mesh.primitive_cube_add, 'Domain', size=1.0, location=(Lx / 2 - P['atras'], 0.0, Hz / 2))
dom.scale = (Lx, Wy, Hz)
bpy.ops.object.transform_apply(scale=True)
m = dom.modifiers.new('Fluid', 'FLUID')
m.fluid_type = 'DOMAIN'
d = m.domain_settings
d.domain_type = 'GAS'
d.resolution_max = int(P['res'])
d.use_adaptive_domain = False
d.cfl_condition = 4.0
d.timesteps_max = 16
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
d.flame_smoke_color = (0.55, 0.55, 0.55)
d.use_dissolve_smoke = P['dissolve'] > 0
if P['dissolve'] > 0:
    d.dissolve_speed = int(P['dissolve'])
    d.use_dissolve_smoke_log = True
# os dois andamentos: câmera lenta no fogo, tempo real na fumaça
for frame, v in ((1, P['lento']), (int(P['lento_ate']), P['lento']), (int(P['real_de']), P['real'])):
    d.time_scale = v
    d.keyframe_insert('time_scale', frame=frame)
d.cache_type = 'ALL'
d.cache_data_format = 'OPENVDB'
d.cache_noise_format = 'OPENVDB'
d.openvdb_cache_compress_type = 'BLOSC'
d.openvdb_data_depth = '16'
d.cache_directory = os.path.join(OUT, 'cache')
d.cache_frame_start = 1
d.cache_frame_end = int(P['quadros'])

# a boca: cilindros curtos deitados no eixo X. O jato de verdade não é um cilindro perfeito (o sabot se abrindo, o gás
# vazando em volta do projétil): com um só, o Mantaflow faz um anel de vórtice limpo demais. Aqui 'jatos' sub-jatos com o
# rumo aberto até 'abre' graus e a força sorteada; o resto (o evacuador) sai do do meio
rnd = random.Random(int(P['semente']))
jq, r0, r1 = int(P['jato_quadros']), int(P['resto_de']), int(P['resto_ate'])
n = max(1, int(P['jatos']))
for i in range(n):
    if i == 0:
        yaw, pitch, k = 0.0, 0.0, 1.0
    else:
        a = 2 * math.pi * (i + rnd.uniform(-0.3, 0.3)) / (n - 1)
        r = math.radians(P['abre']) * rnd.uniform(0.4, 1.0)
        yaw, pitch, k = r * math.cos(a), r * math.sin(a), rnd.uniform(0.7, 1.15)
    muzzle = mesh(bpy.ops.mesh.primitive_cylinder_add, f'Muzzle{i}', radius=P['boca_r'] * (1.0 if i == 0 else 0.6), depth=0.4,
                  location=(0.2, 0.0, P['altura_boca']), rotation=(0.0, math.pi / 2, 0.0), vertices=24)
    bpy.ops.object.transform_apply(rotation=True)
    mod = muzzle.modifiers.new('Fluid', 'FLUID')
    mod.fluid_type = 'FLOW'
    f = mod.flow_settings
    f.flow_type = 'BOTH'
    f.flow_behavior = 'INFLOW'
    f.flow_source = 'MESH'
    f.surface_distance = 0.25
    f.use_initial_velocity = True
    f.subframes = 4
    f.smoke_color = (0.6, 0.6, 0.6)
    dx, dy, dz = math.cos(pitch) * math.cos(yaw), math.cos(pitch) * math.sin(yaw), math.sin(pitch)
    # o jato (quadros 1..jq): combustível, fumaça e calor rápidos; o resto (r0..r1, só o do meio): fumaça devagar
    rest = 1.0 if i == 0 else 0.0
    keys = [(1, P['combustivel'] * k, P['fumaca_jato'], P['calor'], P['jato'] * k),
            (jq, P['combustivel'] * k, P['fumaca_jato'], P['calor'], P['jato'] * k),
            (jq + 1, 0.0, 0.0, 0.0, P['resto']),
            (r0 - 1, 0.0, 0.0, 0.0, P['resto']),
            (r0, 0.0, P['fumaca_resto'] * rest, 0.3 * rest, P['resto']),
            (r1, 0.0, P['fumaca_resto'] * 0.3 * rest, 0.1 * rest, P['resto'] * 0.5),
            (r1 + 1, 0.0, 0.0, 0.0, 0.0)]
    for frame, fuel, dens, temp, v in keys:
        f.fuel_amount = fuel
        f.density = dens
        f.temperature = temp
        f.velocity_coord = (v * dx, v * dy, v * dz)
        for prop in ('fuel_amount', 'density', 'temperature', 'velocity_coord'):
            f.keyframe_insert(prop, frame=frame)

bpy.context.view_layer.objects.active = dom
dom.select_set(True)
t0 = time.time()
with bpy.context.temp_override(object=dom, active_object=dom):
    bpy.ops.fluid.bake_all()
dt = time.time() - t0
size = sum(os.path.getsize(os.path.join(r, n)) for r, _, fs in os.walk(os.path.join(OUT, 'cache')) for n in fs)
print(f'PYRO boca res {int(P["res"])} quadros {int(P["quadros"])}: {dt / 60:.1f} min, cache {size / 1e6:.0f} MB, {P}')
