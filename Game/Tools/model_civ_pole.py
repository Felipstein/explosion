"""O poste de madeira da rede (kit civil v2, 01/11/2026; docs/KIT_CIVIL_V2.md) e o mesmo com o transformador.

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_civ_pole.py C:/Users/luisf/explosion/captures/game/models/civ_v2/pole

O que é (road_town_battle_gemini: os postes ao longo da estrada; 20_beauty_new_tanks/usa_china_gpt.webp e china_gla_gpt.webp: os
postes com a cruzeta e o transformador; 16_beauty_pass/b_gpt.webp): o tronco roliço de madeira tratada (afina pra cima, a faixa mais
escura do pé que fica na terra, as rachaduras de sol em relevo), a cruzeta com as pontas chanfradas, as 2 mãos-francesas de ferro
chato, os 4 isoladores de porcelana com as saias, os parafusos passantes, os degraus de ferro. O transformador: o tambor com as
aletas, as buchas, o suporte e os 2 para-raios.

AS MEDIDAS QUE O JOGO USA FICAM IGUAIS ÀS DO POSTE APROVADO (look_houses.py pole(), docs/APROVADOS.md 17/10, o poste derrubado sem os
palitos; o RTSWireSystem liga o fio no isolador): 8,0 m de altura, a cruzeta ao longo de X a 7,25–7,40 m, os 4 isoladores em
x = ±0,35 e ±0,95 com o topo a 7,60 m (o fio sai a ~7,57 m), o pé de 0,26 m. Nada de ponta de fio modelada.
Saída: SM_civ_pole.glb (SUBSTITUI o civ_pole: a pegada 0,4 × 0,4 m, origem no pé) e SM_civ_pole_transformer.glb (novo).
REGRA DO GERADOR (Felipe, 13/10): poste nunca em cima de rua nem de outra peça.
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import Part, clear_scene, export  # noqa: E402
import model_civ_parts as cp  # noqa: E402
from model_building_parts import bake_in_place  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'civ_v2', 'pole')
os.makedirs(OUT, exist_ok=True)

ARM_Z0, ARM_Z1 = 7.25, 7.40
INS_X = (-0.95, -0.35, 0.35, 0.95)
INS_TOP = 7.60


def insulator(p, x, z0):
    """O isolador de pino de porcelana: o pino de ferro, o corpo com 2 saias (o perfil girado) e a cabeça com o sulco do fio."""
    vp.tube(p, 'Metal#pin', [(x, 0.0, z0 - 0.02), (x, 0.0, z0 + 0.04)], 0.012, seg=6)
    prof = [(0.0, 0.0), (0.03, 0.0), (0.055, 0.03), (0.04, 0.05), (0.048, 0.08), (0.032, 0.1), (0.035, 0.13), (0.03, 0.15),
            (0.034, 0.165), (0.026, 0.18), (0.0, 0.185)]
    vp.lathe(p, 'Lens#ins', (x, 0.0, z0 + INS_TOP - ARM_Z1 - 0.185), 'z', prof, seg=14, smooth=True, hard=40.0)


def pole(transformer=False):
    p = Part('SM_civ_pole_transformer' if transformer else 'SM_civ_pole')
    # o tronco: afina pra cima, o pé escuro (o creosoto e a terra), a cabeça chanfrada
    prof = [(0.0, -0.02), (0.135, -0.02), (0.133, 1.4), (0.118, 6.5), (0.11, 7.9), (0.08, 8.0), (0.0, 8.0)]
    vp.lathe(p, 'Wood#pole', (0.0, 0.0, 0.0), 'z', prof, seg=14, smooth=True, hard=40.0)
    vp.lathe(p, 'Dark#foot', (0.0, 0.0, 0.0), 'z', [(0.137, 0.0), (0.139, 0.02), (0.138, 1.1), (0.136, 1.3)], seg=14, closed=False)
    # as rachaduras de sol (os sulcos escuros compridos que leem de perto)
    for k in range(4):
        a = 2 * math.pi * cp.h3(k, 1, 3)
        z0 = 1.6 + 4.0 * cp.h3(k, 1, 5)
        L = 0.8 + 1.4 * cp.h3(k, 1, 7)
        r = 0.127 - 0.002 * z0
        vp.tube(p, 'Dark#crack', [(r * math.cos(a), r * math.sin(a), z0), (r * math.cos(a + 0.05), r * math.sin(a + 0.05), z0 + L)], 0.007, seg=4)
    # a cruzeta (as pontas chanfradas), os parafusos passantes, as mãos-francesas de ferro chato
    vp.rbox(p, 'Wood#arm', (0.0, 0.0, (ARM_Z0 + ARM_Z1) / 2), (2.2, 0.12, ARM_Z1 - ARM_Z0), r=0.025, k=2)
    for s in (-1, 1):
        vp.rbox(p, 'Wood#arm', (s * 1.11, 0.0, (ARM_Z0 + ARM_Z1) / 2 - 0.015), (0.04, 0.11, 0.1), pitch=0, r=0.02, k=1)
        a = Vector((s * 0.55, 0.0, ARM_Z0 - 0.01))
        b = Vector((0.0, 0.0, ARM_Z0 - 0.62))
        for dy in (-0.075, 0.075):
            vp.rbox(p, 'Metal#strap', tuple((a + b) / 2 + Vector((0, dy, 0))), ((a - b).length + 0.06, 0.012, 0.04), yaw=0,
                    pitch=math.degrees(math.atan2(a.z - b.z, abs(a.x))) * (-s), r=0.004, k=1)
    for z in (ARM_Z0 + 0.075, ARM_Z0 - 0.62):
        vp.tube(p, 'Metal#bolt', [(0.0, -0.17, z), (0.0, 0.17, z)], 0.012, seg=6)
    for x in INS_X:
        insulator(p, x, ARM_Z1)
    # os degraus de ferro (em zigue-zague dos 2 lados, de 2,4 m pra cima)
    for k in range(9):
        z = 2.4 + k * 0.48
        s = 1 if k % 2 else -1
        vp.tube(p, 'Metal#step', vp.bent_bar([(0.0, s * 0.1, z), (0.0, s * 0.26, z), (0.0, s * 0.27, z + 0.05)], R=0.02, n=2), 0.011, seg=5)
    # a placa de numeração e o fio de terra descendo pelo tronco
    vp.rbox(p, 'Metal#tag', (0.0, -0.135, 2.1), (0.12, 0.01, 0.16), r=0.01, k=1)
    vp.tube(p, 'Dark#ground', [(0.13, 0.03, 0.0), (0.12, 0.03, 4.0), (0.11, 0.03, ARM_Z0 - 0.7)], 0.006, seg=4)
    if transformer:
        # o transformador: o tambor com as aletas, a tampa, as buchas, o suporte de cinta, os 2 para-raios na cruzeta de baixo
        zc = 5.6
        C = Vector((0.0, 0.42, zc))
        vp.lathe(p, 'Metal#xfmr', tuple(C), 'z', [(0.0, -0.42), (0.26, -0.42), (0.28, -0.39), (0.28, 0.32), (0.26, 0.36), (0.22, 0.4),
                                                    (0.0, 0.41)], seg=20, smooth=True, hard=40.0)
        for k in range(10):
            a = 2 * math.pi * (k + 0.5) / 10
            if abs(math.sin(a)) > 0.85 and math.sin(a) < 0:
                continue
            q = C + Vector((math.cos(a), math.sin(a), 0)) * 0.3
            vp.rbox(p, 'Metal#xfmr', tuple(q), (0.06, 0.012, 0.56), yaw=math.degrees(a) + 90, r=0.004, k=1)
        for dz in (-0.25, 0.22):
            vp.rbox(p, 'Metal#strap', (0.0, 0.16, zc + dz), (0.1, 0.18, 0.06), r=0.01, k=1)
        for k, x in enumerate((-0.12, 0.12)):
            vp.lathe(p, 'Lens#ins', (x, 0.42, zc + 0.4), 'z', [(0.0, 0.0), (0.035, 0.0), (0.045, 0.04), (0.03, 0.06), (0.038, 0.09),
                                                                 (0.02, 0.12), (0.0, 0.125)], seg=10)
            vp.tube(p, 'Dark#lead', vp.bent_bar([(x, 0.42, zc + 0.52), (x, 0.42, zc + 0.75), (INS_X[1 + k] * 1.0, 0.04, ARM_Z1 + 0.15)],
                                                R=0.15, n=3), 0.006, seg=4)
        vp.rbox(p, 'Wood#arm', (0.0, 0.0, 6.6), (1.2, 0.1, 0.1), r=0.02, k=2)
        for x in (-0.45, 0.45):
            vp.lathe(p, 'Dark#arrester', (x, 0.0, 6.65), 'z', [(0.0, 0.0), (0.03, 0.0), (0.035, 0.03), (0.025, 0.06), (0.032, 0.09),
                                                               (0.02, 0.2), (0.0, 0.21)], seg=10)
    return p.finish()


clear_scene()
a = pole(False)
b = pole(True)
bake_in_place(b, (5.0, 0.0, 0.0))
print('CIV', a.name, cp.tris(a), b.name, cp.tris(b))
export(a, OUT)
export(b, OUT)
