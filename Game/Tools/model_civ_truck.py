"""O caminhão pequeno de carroceria (kit civil v2, 01/11/2026; docs/KIT_CIVIL_V2.md), carregado, e a carcaça queimada dele.

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_civ_truck.py C:/Users/luisf/explosion/captures/game/models/civ_v3/truck [burnt]

O que é: o caminhão leve de cabine avançada (o Isuzu Elf/NKR, o Mitsubishi Canter: o de entrega das feiras e das obras): a cabine
quadrada de cantos redondos com o para-brisa quase em pé, a grade larga embaixo dele, os faróis no para-choque, os espelhos grandes
nos braços; o chassi de longarinas aparecendo entre a cabine e a carroceria, o tanque de diesel do lado; a carroceria de tábuas
(o piso de tábua, as guardas de 3 tábuas com os esteios de ferro e as dobradiças, a cabeceira alta com a grade de proteção da
cabine), as rodas de trás duplas com o para-lama e as borrachas; a carga (os sacos de cimento empilhados, as caixas, a lona dobrada).
Nos game shots: os caminhões de carga das ruas (town_battle_top_gemini, ao fundo; docs/concept/towns/partes).
Medidas reais: 5,80 × 1,95 × 2,25 m, entre-eixos 3,0 m, pneu 7.00 R16 (raio 0,37 m), traseiras duplas.

Saída: SM_civ_truck.glb e, com 'burnt', SM_civ_truck_burnt.glb (a frente em +Y, origem no centro, no chão).

v3 (01/11 noite, a galeria: "parece jogo de lego de novo, muitas quinas, sem detalhes"): a cabine mais redonda (o ombro, o teto, a
borda do arco), 2 amassados (a quina da frente direita, a porta esquerda), a porta direita bege, a ferrugem, as 2 janelas abertas com
o banco e o volante, a carroceria com uma tábua quebrada e uma faltando; o pó na cor de vértice.
"""
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import clear_scene, export  # noqa: E402
import model_civ_parts as cp  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402

ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
OUT = ARGS[0] if ARGS else os.path.join(os.getcwd(), 'civ_v3', 'truck')
BURNT = 'burnt' in ARGS[1:]
os.makedirs(OUT, exist_ok=True)

CAB_X1 = 1.25                       # o fundo da cabine
BED_X0, BED_X1 = 1.12, -2.90        # a carroceria
FLOOR = 1.07                        # o piso de tábua
SIDE_H = 0.46                       # a guarda (3 tábuas)
BHW = 0.975                         # a meia-largura da carroceria
SPEC = {
    'L': 5.80, 'FA': 1.90, 'RA': -1.10, 'paint': 'Paint',
    'R': 0.37, 'tw': 0.20, 'rim_r': 0.20, 'track': 0.70, 'arch_r': 0.44, 'arch_dz': 0.03, 'dual_rear': True,
    'zb': 0.55, 'rs': 0.095, 'rb': 0.06, 'tumble': 0.015, 'bulge': 0.008,
    'crease': 0.008, 'tc': 0.55, 'lips': (0,),
    'body': [(2.90, 0.62, 0.95, 0.80, 0.008), (2.885, 0.56, 1.05, 0.83, 0.01), (2.85, 0.53, 1.12, 0.845, 0.012),
             (2.75, 0.52, 1.18, 0.85, 0.01), (1.32, 0.52, 1.20, 0.85, 0.01), (CAB_X1, 0.55, 1.20, 0.845, 0.008)],
    'gh': [(0.0, 1.16, 2.82, 1.285, 0.835, 0.08), (0.08, 1.23, 2.80, 1.285, 0.83, 0.09), (0.50, 1.70, 2.74, 1.29, 0.815, 0.11),
           (0.88, 2.12, 2.66, 1.295, 0.79, 0.14), (1.0, 2.215, 2.60, 1.30, 0.775, 0.16)],
    'crown': 0.025, 'roof_round': 0.11,
    'windows': [(0, 0.03, 0.97, 0.10, 0.93), (4, 0.25, 0.75, 0.35, 0.80), (2, 0.05, 0.85, 0.12, 0.93, 0.85), (6, 0.15, 0.95, 0.12, 0.93, 0.9)],
    'pillars': (), 'roof_sag': 0.08, 'tyre_style': 'at', 'hubcap': False, 'rim_m': 'Metal#rim',
    'dents': [('corner', 2.90, 1, 0.85, 0.42, 0.06), ('side', 2.05, -1, 0.88, 0.30, 0.035)],
    'rust': 0.6, 'dust': 0.95, 'dust_h': 0.8,
    'details': {
        'body_x1': CAB_X1, 'rear_x': BED_X1 - 0.04, 'under': (2.72, CAB_X1 + 0.05, 0.42, 0.58),
        'bump_f': (0.52, 0.72, 0.08, 0.10), 'bump_m': 'Dark#bump', 'bump_r': (0.66, 0.74, 0.06, 0.02), 'plate_r_z': 0.80,
        'head': (0.62, 0.86, 0.24, 0.15), 'head_in': 0.02, 'sig': (0.80, 0.86), 'grille': (0.98, 0.17, 1.06, 4),
        'grille_bar': 'Metal#slat', 'tail': (0.82, 0.83, 0.20, 0.10),
        'door_v': [(2.76, 0.62, 1.17), (1.34, 0.60, 1.17)],
        'door_lines': [[(2.76, 0.62), (2.05, 0.62), (1.34, 0.60)]],
        'handles': [(1.52, 1.05)], 'mirror': (2.84, 1.42), 'interior': (2.75, 1.4, 0.70, 1),
        'door_swap': [(1, 1.34, 2.76, 'GlaPaint')], 'seat_cover': 'Rust#cover',
    },
}


def wmat(car, base='Wood#plank'):
    return 'Char#burnt' if car.burnt else base


def extra(car, p):
    burnt = car.burnt
    dk = 'Char#burnt' if burnt else 'Dark#chassis'
    # ---- o chassi: as 2 longarinas, as travessas, o tanque de diesel, a bateria, o para-lama das rodas duplas
    for sg in (-1, 1):
        vp.rbox(p, dk, ((2.55 + BED_X1) / 2, 0.42 * sg, 0.74), (2.55 - BED_X1, 0.08, 0.22), r=0.015, k=1)
    for x in (2.3, 1.0, 0.0, -1.10, -2.2, BED_X1 + 0.06):
        vp.rbox(p, dk, (x, 0.0, 0.76), (0.08, 0.84, 0.16), r=0.015, k=1)
    tank = 'Rust#burnt' if burnt else 'Metal#tank'
    vp.lathe(p, tank, (0.45, -0.66, 0.70), 'x', [(0.0, -0.42), (0.17, -0.42), (0.19, -0.40), (0.19, 0.40), (0.17, 0.42), (0.0, 0.42)],
             seg=18, smooth=True)
    for x in (0.18, 0.72):
        vp.lathe(p, dk, (x, -0.66, 0.70), 'x', [(0.195, -0.02), (0.205, 0.0), (0.195, 0.02)], seg=18, closed=True)
    vp.rbox(p, dk, (0.4, 0.62, 0.70), (0.55, 0.26, 0.26), r=0.03, k=2)
    for sg in (-1, 1):
        cp.arch_liner(p, dk if burnt else 'Dark#guard', car.s['RA'], car.zc + 0.03, 0.47, sg * 0.56, sg * 0.95, a0=-5, a1=185, t=0.012)
        cp.arch_liner(p, dk, car.s['RA'], car.zc + 0.03, 0.47, sg * 0.56 - 0.0, sg * 0.56 + sg * 0.02, a0=-5, a1=185, t=0.06)
        if not burnt:
            vp.mudflap(p, (car.s['RA'] - 0.52, 0.76 * sg, 0.30), 0.34, 0.38, yaw=0, m='Rubber')
    # o degrau da cabine (atrás da roda da frente)
    for sg in (-1, 1):
        vp.rbox(p, dk, (1.42, 0.80 * sg, 0.48), (0.24, 0.16, 0.035), r=0.015, k=1)
    # o para-choque de trás (a barra de proteção) e a travessa das lanternas
    vp.rbox(p, dk, (BED_X1 - 0.02, 0.0, 0.85), (0.08, 1.90, 0.12), r=0.02, k=2)
    # ---- a carroceria: as vigas de baixo, o piso de tábua, as guardas de 3 tábuas, os esteios, a cabeceira com a grade
    for sg in (-1, 1):
        vp.rbox(p, dk, ((BED_X0 + BED_X1) / 2, (BHW - 0.04) * sg, FLOOR - 0.07), (BED_X0 - BED_X1, 0.08, 0.10), r=0.015, k=1)
    for x in [BED_X0 - 0.2 - 0.5 * k for k in range(8)]:
        vp.rbox(p, dk, (x, 0.0, FLOOR - 0.06), (0.06, 2 * BHW - 0.04, 0.08), r=0.01, k=1)
    n = 12
    for k in range(n):
        y = -BHW + (k + 0.5) * 2 * BHW / n
        vp.rbox(p, wmat(car), ((BED_X0 + BED_X1) / 2, y, FLOOR - 0.015), (BED_X0 - BED_X1 - 0.02, 2 * BHW / n - 0.012, 0.035),
                r=0.008, k=1)
    post = 'Rust#burnt' if burnt else 'Metal#post'
    xs_post = [BED_X0 - 0.04, (BED_X0 + BED_X1) / 2, BED_X1 + 0.04]
    for sg in (-1, 1):
        for b in range(3):
            if burnt and (b == 2 or (b == 1 and sg > 0)):
                continue                                  # a tábua de cima queimou (e a do meio de um lado)
            if not burnt and sg < 0 and b == 1:
                continue                                  # (v3) a tábua do meio da esquerda faltando
            z = FLOOR + 0.02 + b * (SIDE_H / 3) + SIDE_H / 6
            if not burnt and sg > 0 and b == 2:
                # (v3) a tábua de cima da direita quebrada: o pedaço de trás caído pra fora, preso num esteio
                xm = -0.9
                vp.rbox(p, wmat(car), ((BED_X0 + xm) / 2, (BHW - 0.02) * sg, z), (BED_X0 - xm - 0.04, 0.035, SIDE_H / 3 - 0.012), r=0.008, k=1)
                vp.rbox(p, wmat(car), ((xm + BED_X1) / 2 - 0.02, (BHW + 0.02) * sg, z - 0.06), (xm - BED_X1 - 0.06, 0.035, SIDE_H / 3 - 0.012),
                        roll=-8.0, pitch=3.5, r=0.008, k=1)
                continue
            vp.rbox(p, wmat(car), ((BED_X0 + BED_X1) / 2, (BHW - 0.02) * sg, z), (BED_X0 - BED_X1 - 0.06, 0.035, SIDE_H / 3 - 0.012),
                    r=0.008, k=1)
        for x in xs_post:
            vp.rbox(p, post, (x, (BHW + 0.005) * sg, FLOOR + SIDE_H / 2), (0.06, 0.03, SIDE_H + 0.04), r=0.01, k=1)
        for x in [BED_X0 - 0.6, -0.4, -1.6, BED_X1 + 0.5]:
            vp.hinge(p, post, (x, (BHW + 0.012) * sg, FLOOR + 0.01), 'x', 0.14, r=0.016, knuckles=2)
    # a guarda de trás (a tampa) e a cabeceira alta com a grade de tubo por cima da cabine
    for b in range(3):
        z = FLOOR + 0.02 + b * (SIDE_H / 3) + SIDE_H / 6
        if burnt and b == 2:
            continue
        vp.rbox(p, wmat(car), (BED_X1 + 0.02, 0.0, z), (0.035, 2 * BHW - 0.06, SIDE_H / 3 - 0.012), r=0.008, k=1)
    for b in range(5):
        z = FLOOR + 0.02 + b * 0.16 + 0.08
        vp.rbox(p, wmat(car), (BED_X0 - 0.02, 0.0, z), (0.035, 2 * BHW - 0.06, 0.148), r=0.008, k=1)
    for sg in (-1, 1):
        vp.tube(p, post, [(BED_X0 - 0.02, (BHW - 0.04) * sg, FLOOR), (BED_X0 - 0.02, (BHW - 0.04) * sg, 2.05)], 0.03, seg=8)
    vp.tube(p, post, vp.bent_bar([(BED_X0 - 0.02, -(BHW - 0.04), 2.05), (BED_X0 + 0.25, -(BHW - 0.04), 2.32),
                                  (BED_X0 + 0.25, BHW - 0.04, 2.32), (BED_X0 - 0.02, BHW - 0.04, 2.05)], R=0.08, n=3), 0.025, seg=8)
    for k in range(5):
        y = -0.7 + 0.35 * k
        vp.tube(p, post, [(BED_X0 - 0.02, y, FLOOR + 0.82), (BED_X0 - 0.02, y, 2.05), (BED_X0 + 0.25, y, 2.32)], 0.014, seg=6)
    if burnt:
        return
    # ---- a carga: os sacos de cimento empilhados na frente, as caixas atrás, a lona dobrada e as cordas
    z0 = FLOOR + 0.005
    for lay in range(2):                                 # (v3, o orçamento: 2 camadas de sacos, era 3)
        for i in range(3):
            for j in range(2):
                x = 0.70 - j * 0.48 - (0.04 if lay % 2 else 0.0)
                y = -0.62 + i * 0.62 + (0.05 if lay % 2 else 0.0)
                cp.bag(p, (x, y, z0 + lay * 0.17), (0.58, 0.42, 0.18), yaw=90 + (lay * 7 + i * 3) % 9 - 4, m='Cloth#cement',
                       seed=lay * 10 + i * 3 + j)
    for (x, y, zz, w) in ((-0.55, -0.45, 0, 0.62), (-0.55, 0.30, 0, 0.62), (-1.25, -0.45, 0, 0.62), (-0.85, -0.10, 0.5, 0.58)):
        c = Vector((x, y, z0 + zz + 0.25))
        vp.rbox(p, 'Wood#crate', tuple(c), (w, 0.62, 0.50), yaw=3 * (x + y), r=0.02, k=1)
        for dz in (-0.17, 0.0, 0.17):
            vp.rbox(p, 'Wood#slat', tuple(c + Vector((0, 0, dz))), (w + 0.012, 0.632, 0.06), yaw=3 * (x + y), r=0.01, k=1)
    vp.tarp_bundle(p, (-2.15, 0.0, z0), (1.20, 1.60, 0.34), yaw=90, m='Cloth#load', strap_m='Wood#rope', buckle_m='Dark',
                   straps=2, layers=2, seed=6.0, n=9, k=9)
    for x in (0.45, -0.70, -1.95):
        cp.rope_over(p, (x, -BHW, FLOOR + SIDE_H), (x - 0.05, BHW, FLOOR + SIDE_H), z0 + (0.45 if x > 0 else 0.95 if x > -1 else 0.42))


clear_scene()
obj, _ = cp.passenger_car(SPEC, BURNT, 'SM_civ_truck_burnt' if BURNT else 'SM_civ_truck', extra=extra, seed=17)
print('CIV', obj.name, cp.tris(obj), 'tris (antes da grade)')
export(obj, OUT)
