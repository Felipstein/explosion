"""O hatch civil (kit civil v2, 01/11/2026; docs/KIT_CIVIL_V2.md), com o bagageiro de teto carregado, e a carcaça queimada dele.

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_civ_hatch.py C:/Users/luisf/explosion/captures/game/models/civ_v3/hatch [burnt]

O que é: o compacto de 2 volumes dos anos 80/90 (o Golf Mk2, o Peugeot 205, o Kia Pride): o capô curto, o teto que vai até o fim
com a tampa de trás quase em pé, as lanternas altas nos cantos. O que lê de cima é o BAGAGEIRO CARREGADO (a trouxa de lona amarrada e o
tapete enrolado): o carro de família da região, como nos game shots de cidade (docs/concept/towns/partes, as ruas com carro parado).
Medidas reais: 3,95 × 1,66 × 1,41 m (1,60 com a carga), entre-eixos 2,47 m, pneu 175/70 R13 (raio 0,29 m).

Saída: SM_civ_hatch.glb e, com 'burnt', SM_civ_hatch_burnt.glb (a frente em +Y, origem no centro, no chão).

v3 (01/11 noite, a galeria: "parece jogo de lego de novo, muitas quinas, sem detalhes"; KIT_CIVIL_V2 §6: "o sedã e o hatch têm a
mesma cara"): a FRENTE PRÓPRIA do Golf Mk2 (os faróis redondos com o aro cromado e a grade preta estreita de 2 frisos entre eles); a
lataria mais redonda (o vinco, o para-lama que salta, a borda do arco), 3 amassados (a quina de trás direita, a porta esquerda, o
capô), o capô de primer cinza (o de ferro-velho), a ferrugem, a janela do motorista aberta com o interior aparecendo, o pó e o vidro
sujo na cor de vértice.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import clear_scene, export  # noqa: E402
import model_civ_parts as cp  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402

ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
OUT = ARGS[0] if ARGS else os.path.join(os.getcwd(), 'civ_v3', 'hatch')
BURNT = 'burnt' in ARGS[1:]
os.makedirs(OUT, exist_ok=True)

ROOF = 1.41
SPEC = {
    'L': 3.95, 'FA': 1.195, 'RA': -1.275, 'paint': 'GlaPaint',
    'R': 0.29, 'tw': 0.175, 'rim_r': 0.165, 'track': 0.70, 'arch_r': 0.355, 'arch_dz': 0.03,
    'zb': 0.30, 'rs': 0.105, 'rb': 0.08, 'tumble': 0.035, 'bulge': 0.016,
    'crease': 0.009, 'tc': 0.6, 'flare': 0.02,
    'body': [(1.975, 0.40, 0.62, 0.74, 0.010), (1.962, 0.33, 0.67, 0.78, 0.014), (1.93, 0.29, 0.71, 0.815, 0.02),
             (1.85, 0.27, 0.74, 0.825, 0.025), (1.55, 0.28, 0.78, 0.83, 0.03), (0.72, 0.30, 0.875, 0.83, 0.03),
             (0.62, 0.30, 0.895, 0.83, 0.02), (-1.62, 0.30, 0.91, 0.83, 0.02), (-1.85, 0.32, 0.905, 0.82, 0.02),
             (-1.94, 0.36, 0.88, 0.80, 0.015), (-1.975, 0.43, 0.83, 0.77, 0.010)],
    'gh': [(0.0, 0.845, 0.72, -1.93, 0.78, 0.13), (0.12, 0.905, 0.67, -1.905, 0.775, 0.135), (0.40, 1.06, 0.42, -1.85, 0.755, 0.15),
           (0.70, 1.22, 0.17, -1.76, 0.725, 0.17), (0.90, 1.33, 0.0, -1.67, 0.70, 0.18), (1.0, 1.375, -0.08, -1.60, 0.69, 0.19)],
    'crown': 0.035, 'roof_round': 0.08,
    'windows': [(0, 0.03, 0.97, 0.15, 0.95), (4, 0.06, 0.94, 0.22, 0.92),
                (2, 0.05, 0.43, 0.16, 0.94), (2, 0.49, 0.73, 0.16, 0.94), (2, 0.78, 0.93, 0.2, 0.9),
                (6, 0.07, 0.22, 0.2, 0.9), (6, 0.27, 0.51, 0.16, 0.94), (6, 0.57, 0.95, 0.16, 0.94, 0.9)],
    'pillars': ((2, 0.43, 0.49), (2, 0.73, 0.78), (6, 0.22, 0.27), (6, 0.51, 0.57)),
    'hubcap_missing': [(0, 1)], 'roof_sag': 0.08, 'tyre_style': None, 'hubcap': True,
    'dents': [('corner', -1.97, 1, 0.55, 0.36, 0.06), ('side', -0.45, -1, 0.55, 0.30, 0.03), ('top', 1.45, -0.28, 0.26, 0.022)],
    'rust': 0.45, 'dust': 0.85,
    'details': {
        'bump_f': (0.30, 0.44, 0.07, 0.25), 'bump_r': (0.32, 0.46, 0.07, 0.22), 'plate_r_z': 0.60,
        # (v3) a frente do Golf: os faróis redondos (r 9 cm) e a grade preta de 2 frisos entre eles
        'front': 'round', 'head_round': 0.09, 'head_in': 0.02,
        'head': (0.54, 0.645, 0.30, 0.14), 'sig': (0.74, 0.52), 'grille': (0.80, 0.12, 0.645, 2), 'tail': (0.68, 0.72, 0.16, 0.22),
        'hood_swap': 'Metal',
        'hood': (0.78, 1.82, 0.735),
        'door_v': [(0.685, 0.34, 0.89), (-0.30, 0.32, 0.90)],
        'door_lines': [[(-1.06, 0.905), (-1.06, 0.77), (-0.97, 0.68), (-0.91, 0.55), (-0.92, 0.32)],
                       [(0.685, 0.335), (0.2, 0.335), (-0.3, 0.335), (-0.7, 0.335), (-0.92, 0.335)]],
        'trim': [(0.85, -0.85, 0.54)], 'handles': [(0.22, 0.80), (-0.78, 0.80)], 'mirror': (0.58, 0.93),
        'fuel': (-1.62, 0.80), 'antenna': (-1.45, -0.55, ROOF - 0.03), 'interior': (0.62, -1.62, 0.36, 2),
    },
}


def extra(car, p):
    burnt = car.burnt
    rm = 'Rust#rack' if burnt else 'Dark#rack'
    cp.roof_rack(p, -1.45, 0.05, 0.60, ROOF - 0.01, h=0.07, m=rm, bars=4)
    zr = ROOF + 0.06 + 0.02
    if not burnt:
        # a carga: a trouxa de lona amarrada, o tapete enrolado atravessado e as cordas até as longarinas
        vp.tarp_bundle(p, (-0.80, 0.04, zr), (1.05, 0.86, 0.30), yaw=2, m='Cloth#load', strap_m='Wood#rope', buckle_m='Dark',
                       straps=2, layers=3, seed=2.0, n=14, k=14)
        cp.carpet_roll(p, (-0.05, 0.0, zr + 0.11), 'y', 1.35, 0.11)
        for x in (-0.55, -1.05):
            cp.rope_over(p, (x, -0.60, zr - 0.01), (x + 0.04, 0.60, zr - 0.01), zr + 0.33)
        cp.rope_over(p, (0.02, -0.60, zr - 0.01), (-0.08, 0.60, zr - 0.01), zr + 0.23)
        # o limpador de trás
        a = car.gh.point(0.3, 4, 0.5)
        vp.tube(p, 'Dark#wiper', [tuple(a), (a.x + 0.02, a.y - 0.35, a.z + 0.22)], 0.009, seg=4)


clear_scene()
obj, _ = cp.passenger_car(SPEC, BURNT, 'SM_civ_hatch_burnt' if BURNT else 'SM_civ_hatch', extra=extra, seed=7)
print('CIV', obj.name, cp.tris(obj), 'tris (antes da grade)')
export(obj, OUT)
