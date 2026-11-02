"""A van de passageiros / micro-ônibus (kit civil v2, 01/11/2026; docs/KIT_CIVIL_V2.md), com o bagageiro carregado, e a carcaça.

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_civ_van.py C:/Users/luisf/explosion/captures/game/models/civ_v3/van [burnt]

O que é: a van de cabine avançada dos anos 90 (a HiAce H100 de teto alto, o "micro" das linhas entre vilas): o bico curto, o
para-brisa quase em pé, as rodas da frente embaixo dos bancos, a fileira de janelas, a porta de correr da direita com o trilho, a
grade larga de lâminas, as lanternas em pé atrás e o BAGAGEIRO de teto inteiro, carregado (as trouxas, o tapete, o estepe, o galão):
de cima é a silhueta da van da região. Nos game shots: as vans da city_modern_noon e a carga de teto das ruas do 16_beauty_pass.
Medidas reais: 4,85 × 1,70 × 2,07 m (2,45 com a carga), entre-eixos 2,80 m, pneu 195 R14C (raio 0,31 m).

Saída: SM_civ_van.glb e, com 'burnt', SM_civ_van_burnt.glb (a frente em +Y, origem no centro, no chão).

v3 (01/11 noite, a galeria: "parece jogo de lego de novo, muitas quinas, sem detalhes"): a van da linha de terra. O teto e as
quinas mais redondos, o vinco e a borda dos arcos, 3 amassados (a lateral esquerda, a quina de trás direita, a frente direita), a porta
de correr bege (a de ferro-velho), a ferrugem no pé, a janela do motorista aberta e uma de trás quebrada (o banco aparecendo), os 3
bancos por dentro; o pó grosso subindo até a cintura e o vidro sujo com o arco dos limpadores na cor de vértice.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import clear_scene, export  # noqa: E402
import model_civ_parts as cp  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402

ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
OUT = ARGS[0] if ARGS else os.path.join(os.getcwd(), 'civ_v3', 'van')
BURNT = 'burnt' in ARGS[1:]
os.makedirs(OUT, exist_ok=True)

ROOF = 2.07
SPEC = {
    'L': 4.85, 'FA': 1.475, 'RA': -1.325, 'paint': 'Paint',
    'R': 0.31, 'tw': 0.195, 'rim_r': 0.178, 'track': 0.725, 'arch_r': 0.375, 'arch_dz': 0.03,
    'zb': 0.33, 'rs': 0.085, 'rb': 0.07, 'tumble': 0.02, 'bulge': 0.01,
    'crease': 0.009, 'tc': 0.55, 'flare': 0.016,
    'body': [(2.425, 0.42, 0.75, 0.80, 0.008), (2.41, 0.36, 0.82, 0.83, 0.012), (2.37, 0.33, 0.88, 0.845, 0.015),
             (2.30, 0.32, 0.93, 0.85, 0.015), (2.20, 0.32, 0.99, 0.85, 0.012), (2.12, 0.33, 1.02, 0.85, 0.01),
             (-2.36, 0.34, 1.02, 0.85, 0.01), (-2.405, 0.38, 1.0, 0.835, 0.01), (-2.425, 0.42, 0.97, 0.82, 0.008)],
    'gh': [(0.0, 0.99, 2.20, -2.40, 0.835, 0.10), (0.08, 1.045, 2.165, -2.40, 0.83, 0.11), (0.40, 1.42, 2.02, -2.39, 0.815, 0.13),
           (0.75, 1.82, 1.88, -2.38, 0.79, 0.15), (0.92, 1.98, 1.79, -2.37, 0.775, 0.17), (1.0, 2.035, 1.71, -2.35, 0.76, 0.19)],
    'crown': 0.03, 'roof_round': 0.12,
    'windows': [(0, 0.03, 0.97, 0.10, 0.93), (4, 0.06, 0.94, 0.12, 0.72),
                (2, 0.02, 0.165, 0.10, 0.93), (2, 0.20, 0.42, 0.14, 0.92), (2, 0.45, 0.68, 0.14, 0.92), (2, 0.71, 0.95, 0.14, 0.92),
                (6, 0.05, 0.29, 0.14, 0.92), (6, 0.32, 0.55, 0.14, 0.92, 1.0), (6, 0.58, 0.80, 0.14, 0.92), (6, 0.835, 0.98, 0.10, 0.93, 0.9)],
    'pillars': ((2, 0.165, 0.20), (2, 0.42, 0.45), (2, 0.68, 0.71), (6, 0.29, 0.32), (6, 0.55, 0.58), (6, 0.80, 0.835)),
    'roof_sag': 0.10, 'tyre_style': None, 'hubcap': True,
    'dents': [('side', -0.85, -1, 0.72, 0.48, 0.035), ('corner', -2.42, 1, 0.62, 0.36, 0.05), ('side', 1.75, 1, 0.62, 0.30, 0.03)],
    'rust': 0.5, 'dust': 0.95, 'dust_h': 0.75,
    'details': {
        'bump_f': (0.34, 0.52, 0.08, 0.22), 'bump_r': (0.36, 0.52, 0.08, 0.22), 'plate_r_z': 0.66,
        'head': (0.66, 0.74, 0.25, 0.17), 'sig': (0.81, 0.58), 'grille': (0.96, 0.20, 0.74, 5), 'grille_bar': 'Metal#slat',
        'tail': (0.76, 0.76, 0.14, 0.34),
        'door_v': [(2.10, 0.36, 1.0), (1.22, 0.36, 1.0)],
        'door_lines': [[(2.10, 0.37), (1.66, 0.37), (1.22, 0.37)]],
        'trim': [(1.0, -2.2, 0.62)], 'handles': [(1.36, 0.92)], 'mirror': (2.15, 1.12),
        'fuel': (-1.95, 0.85), 'interior': (2.05, -2.0, 0.42, 3),
        'under': (2.0, -1.9, 0.17, 0.36),
        'door_swap': [(1, -0.05, 1.18, 'GlaPaint')], 'seat_cover': 'Rust#cover',
    },
}


def extra(car, p):
    burnt = car.burnt
    # a porta de correr da direita: as juntas (a frente, o fundo, a de baixo) e o trilho por fora, na cintura de trás
    if not burnt:
        for x in (1.18, -0.05):
            car.seam_v(p, x, 0.36, 1.0, 1)
        car.seam_line(p, [(x, car.side_y(x, 0.37) + 0.0015, 0.37) for x in (1.18, 0.6, -0.05)])
        car.trim_strip(p, -0.10, -2.30, 1.02, 1, m='Dark#rail', h=0.05)
        # a porta de trás (o portão de cima): a junta em volta
        xr = car.xr + 0.004
        car.seam_line(p, [(xr, y, 0.42) for y in (-0.72, 0.0, 0.72)])
        for sg in (-1, 1):
            car.seam_line(p, [(xr + 0.01, 0.72 * sg, z) for z in (0.42, 0.9, 1.3, 1.8)])
        vp.rbox(p, 'Dark#handle', (car.xr - 0.004, 0.0, 0.95), (0.02, 0.18, 0.04), r=0.015, k=2)
        # o degrau da porta de correr
        vp.rbox(p, 'Dark#step', (0.55, 0.80, 0.33), (1.15, 0.12, 0.04), r=0.02, k=2)
    # o bagageiro de teto inteiro, com a carga amarrada
    rm = 'Rust#rack' if burnt else 'Dark#rack'
    cp.roof_rack(p, -2.15, 1.55, 0.70, ROOF - 0.02, h=0.12, m=rm, bars=7)
    zr = ROOF + 0.10 + 0.02
    if burnt:
        return
    vp.tarp_bundle(p, (-1.30, -0.05, zr), (1.25, 1.10, 0.38), yaw=1, m='Cloth#load', strap_m='Wood#rope', buckle_m='Dark',
                   straps=2, layers=3, seed=4.0, n=14, k=14)
    cp.bag(p, (-0.35, -0.38, zr), (0.80, 0.50, 0.26), yaw=84, m='Cloth#bag', seed=4)
    cp.bag(p, (-0.30, 0.22, zr), (0.78, 0.48, 0.26), yaw=92, m='Cloth#bag2', seed=5)
    cp.bag(p, (-0.34, -0.05, zr + 0.20), (0.74, 0.46, 0.24), yaw=96, m='Cloth#bag', seed=6)
    cp.carpet_roll(p, (0.55, 0.0, zr + 0.12), 'y', 1.45, 0.12)
    cp.carpet_roll(p, (0.82, 0.05, zr + 0.11), 'y', 1.30, 0.11, m='Cloth#rug2', band_m='Dark#rug')
    vp.tyre(p, (-2.0 + 0.20, 0.42, zr + 0.10), 0.31, 0.195, 1, rim_r=0.178, rubber='Rubber', rim='Metal#rim', hub='Metal#hub',
            nut_m='Metal#hub', seg=24, lugs=False, nuts=5, dish=0.03, axis='z')
    vp.jerrycan(p, (1.30, -0.45, zr - 0.01), yaw=0, m='Crate#can', cap_m='Dark')
    vp.jerrycan(p, (1.30, -0.22, zr - 0.01), yaw=4, m='Crate#can', cap_m='Dark')
    for x in (-1.0, -1.65, 0.0, 0.7):
        cp.rope_over(p, (x, -0.70, zr - 0.01), (x + 0.05, 0.70, zr - 0.01), zr + (0.42 if x < -0.5 else 0.30))


clear_scene()
obj, _ = cp.passenger_car(SPEC, BURNT, 'SM_civ_van_burnt' if BURNT else 'SM_civ_van', extra=extra, seed=13)
print('CIV', obj.name, cp.tris(obj), 'tris (antes da grade)')
export(obj, OUT)
