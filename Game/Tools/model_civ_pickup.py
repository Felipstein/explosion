"""A picape civil (kit civil v2, 01/11/2026; docs/KIT_CIVIL_V2.md), carregada, e a carcaça queimada dela.

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_civ_pickup.py C:/Users/luisf/explosion/captures/game/models/civ_v3/pickup [burnt]

O que é: a picape de cabine simples dos anos 90 (a Hilux N80/N60, a mais vista na região e nos game shots: as 6 picapes de
road_town_battle_gemini, as de town_battle_top_gemini e as do 16_beauty_pass/b_gpt): o capô reto e alto, a grade com os faróis
quadrados, o para-choque cromado, a cabine curta com a janelinha de trás, a caçamba comprida com o friso de cima, o piso de chapa
canelada, as caixas de roda por dentro, a tampa com as lanternas em pé. A carga de quem trabalha: os sacos, os galões, o estepe deitado
e o tapete enrolado, amarrados.
Medidas reais: 4,85 × 1,69 × 1,67 m, entre-eixos 3,08 m, pneu 215/70 R15 (raio 0,355 m), bitola 1,43 m.

Saída: SM_civ_pickup.glb (SUBSTITUI o civ_pickup do kit: o mesmo nome, a pegada 2,0 × 5,2 m do WorldVillage.cpp, a frente em +Y) e,
com 'burnt', SM_civ_pickup_burnt.glb.

v3 (01/11 noite, a galeria: "aqui já pecou [...] parece jogo de lego de novo, muitas quinas, sem detalhes"; o alvo: as picapes do
road_town_battle_gemini): a picape de trabalho moída. A lataria mais redonda (o ombro, o vinco, os para-lamas quadrados que saltam,
a borda dos arcos), 5 amassados (a quina da frente esquerda, a porta direita, o capô, o teto, a caçamba esquerda), a porta esquerda
oliva (a de ferro-velho), o para-choque cromado torto de um lado, o retrovisor direito arrancado, MUITA ferrugem (o pé, os arcos, o
capô e o teto queimados de sol), as janelas abertas com o banco inteiriço, o painel e o volante; a caçamba com a tralha de quem
trabalha (os sacos, os galões, o estepe, o tapete, o botijão, as tábuas, a caixa de papelão, o caixote, a lona embolada, as cordas) e
o piso de chapa enferrujado; o pó grosso e o vidro sujo com o arco dos limpadores na cor de vértice.
A bandeira e a metralhadora do alvo NÃO entram na picape civil: arma num carro neutro lê como a Technical inimiga na batalha
(jogabilidade antes do realismo); a picape armada é a unidade da GLA.
"""
import math
import os
import sys

from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import Part, clear_scene, export  # noqa: E402
import model_civ_parts as cp  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402

ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
OUT = ARGS[0] if ARGS else os.path.join(os.getcwd(), 'civ_v3', 'pickup')
BURNT = 'burnt' in ARGS[1:]
os.makedirs(OUT, exist_ok=True)

CAB_X = -0.42                    # o fundo da cabine
BED_X0, BED_X1 = -0.47, -2.425   # a caçamba (a cabeceira, a tampa)
FLOOR, TOP, SILL = 0.86, 1.20, 0.50
SPEC = {
    'L': 4.85, 'FA': 1.565, 'RA': -1.515, 'paint': 'Paint',
    'R': 0.355, 'tw': 0.205, 'rim_r': 0.19, 'track': 0.715, 'arch_r': 0.425, 'arch_dz': 0.03,
    'zb': SILL, 'rs': 0.085, 'rb': 0.075, 'tumble': 0.03, 'bulge': 0.012,
    'crease': 0.012, 'tc': 0.6, 'flare': 0.03,
    'body': [(2.425, 0.53, 0.86, 0.79, 0.008), (2.41, 0.48, 0.93, 0.82, 0.012), (2.37, 0.46, 0.97, 0.835, 0.016),
             (2.25, 0.45, 1.00, 0.845, 0.02), (1.20, 0.47, 1.07, 0.845, 0.025), (1.10, 0.48, 1.10, 0.845, 0.02),
             (CAB_X, 0.48, 1.11, 0.845, 0.02)],
    'gh': [(0.0, 1.06, 1.12, -0.405, 0.80, 0.10), (0.10, 1.12, 1.08, -0.405, 0.795, 0.105), (0.45, 1.33, 0.80, -0.40, 0.77, 0.12),
           (0.80, 1.55, 0.52, -0.395, 0.74, 0.14), (1.0, 1.64, 0.40, -0.39, 0.725, 0.15)],
    'crown': 0.03, 'roof_round': 0.07,
    # (v3) as 2 janelas de lado abertas (o braço pra fora no calor)
    'windows': [(0, 0.03, 0.97, 0.13, 0.95), (4, 0.12, 0.88, 0.22, 0.88), (2, 0.05, 0.88, 0.14, 0.94, 0.85), (6, 0.12, 0.95, 0.14, 0.94, 0.9)],
    'pillars': (), 'roof_sag': 0.07, 'tyre_style': 'at', 'hubcap': False, 'rim_m': 'Metal#rim',
    'dents': [('corner', 2.42, -1, 0.72, 0.45, 0.075), ('side', 0.45, 1, 0.78, 0.36, 0.04), ('top', 1.85, 0.32, 0.32, 0.03),
              ('roof', 0.02, 0.22, 0.30, 0.025), ('side', -1.85, -1, 0.92, 0.42, 0.045)],
    'rust': 0.8, 'dust': 0.95, 'dust_h': 0.6,
    'details': {
        'body_x1': CAB_X, 'rear_x': BED_X1, 'under': (2.2, -2.3, 0.30, 0.52), 'bump_m': 'Metal#chrome',
        'bump_f': (0.47, 0.63, 0.07, 0.18), 'bump_r': (0.50, 0.62, 0.07, 0.10), 'plate_r_z': 0.74,
        'head': (0.60, 0.86, 0.32, 0.16), 'sig': (0.80, 0.83), 'grille': (0.84, 0.18, 0.86, 4), 'tail': (0.78, 0.97, 0.12, 0.30),
        'hood': (1.15, 2.30, 0.76),
        'door_v': [(1.08, 0.52, 1.10), (-0.33, 0.52, 1.11)],
        'door_lines': [[(1.08, 0.53), (0.4, 0.53), (-0.33, 0.53)]],
        'trim': [(1.0, -0.28, 0.80)], 'handles': [(-0.18, 0.98)], 'mirror': (1.02, 1.14),
        'fuel': (-1.0, 0.98), 'antenna': (1.35, 0.80, 1.06), 'interior': (1.02, -0.36, 0.56, 1),
        'door_swap': [(-1, -0.33, 1.08, 'Crate')], 'bump_hang': ('f', 1, 4.0), 'mirror_missing': 1, 'seat_cover': 'Rust#cover',
    },
}


def mat(car, c, base='Paint#bed'):
    return car.burn_mat(c) if car.burnt else base


def bed(car, p):
    burnt = car.burnt
    for sg in (-1, 1):
        cp.bed_wall(car, p, BED_X0, BED_X1 + 0.03, 0.845 * sg, TOP, SILL)
        # os bolsos das estacas no friso e o gancho de amarrar
        for x in (-0.62, -1.45, -2.28):
            vp.rbox(p, 'Dark#pocket', (x, 0.80 * sg, TOP + 0.004), (0.07, 0.05, 0.012), r=0.01, k=1)
        for x in (-0.9, -2.0):
            vp.tube(p, 'Metal#hook', vp.bent_bar([(x - 0.05, 0.78 * sg, TOP - 0.06), (x - 0.05, 0.74 * sg, TOP - 0.10),
                                                  (x + 0.05, 0.74 * sg, TOP - 0.10), (x + 0.05, 0.78 * sg, TOP - 0.06)], R=0.02, n=2), 0.008, seg=5)
    # a cabeceira (a chapa da frente da caçamba, com o friso) e o piso de chapa canelada
    m = mat(car, Vector((BED_X0, 0, 1.0)))
    vp.rbox(p, m, (BED_X0 - 0.02, 0.0, (SILL + TOP) / 2 + 0.01), (0.05, 1.68, TOP - SILL + 0.02), pitch=0, r=0.03, k=2)
    vp.rbox(p, m, (BED_X0 - 0.02, 0.0, TOP + 0.005), (0.09, 1.70, 0.03), r=0.012, k=2)
    vp.rbox(p, mat(car, Vector((-1.4, 0, FLOOR))), ((BED_X0 + BED_X1) / 2, 0.0, FLOOR - 0.02), (BED_X0 - BED_X1 - 0.04, 1.62, 0.04),
            r=0.02, k=1)
    for k in range(7):
        y = -0.66 + 0.22 * k
        # (v3) as nervuras gastas: a tinta foi embora no que a carga arrasta (3 enferrujadas, uma só no meio)
        rib = 'Rust#rib' if k in (1, 4, 5) else 'Paint#rib'
        vp.rbox(p, mat(car, Vector((-1.4, y, FLOOR)), rib), ((BED_X0 + BED_X1) / 2 - 0.02, y, FLOOR + 0.008),
                (BED_X0 - BED_X1 - 0.16, 0.07, 0.022), r=0.03, k=2)
    if not burnt:
        vp.rbox(p, 'Rust#floor', (-1.15, 0.05, FLOOR + 0.002), (1.1, 1.2, 0.008), yaw=4, r=0.25, k=3)
    # as caixas de roda por dentro (o volume que a carga contorna)
    for sg in (-1, 1):
        vp.rbox(p, mat(car, Vector((car.s['RA'], 0.7 * sg, 1.0))), (car.s['RA'], 0.68 * sg, FLOOR + 0.07), (0.92, 0.26, 0.17), r=0.08, k=3)
    # a tampa: em pé (inteira) ou caída aberta (a carcaça), com os 2 frisos estampados e a corrente
    tg = Part('tailgate')
    vp.rbox(tg, mat(car, Vector((BED_X1, 0, 0.9))), (BED_X1 + 0.02, 0.0, (0.55 + TOP) / 2), (0.05, 1.62, TOP - 0.55), r=0.025, k=2)
    for z in (0.78, 0.98):
        vp.rbox(tg, mat(car, Vector((BED_X1, 0, z)), 'Paint#bed'), (BED_X1 - 0.004, 0.0, z), (0.02, 1.40, 0.05), r=0.02, k=2)
    if not burnt:
        vp.rbox(tg, 'Metal#chrome', (BED_X1 - 0.008, 0.0, 1.10), (0.02, 0.22, 0.05), r=0.02, k=2)       # o trinco
    tgo = tg.finish()
    if burnt:
        piv = cp.V(BED_X1, 0.0, 0.57)
        tgo.data.transform(Matrix.Translation(piv) @ Matrix.Rotation(math.radians(-88), 4, 'Y') @ Matrix.Translation(-piv))
    car.hang_objs = list(getattr(car, 'hang_objs', [])) + [tgo]
    if burnt:
        # o que sobrou da carga: o galão queimado e o monte de carvão dos sacos
        vp.jerrycan(p, (-1.9, -0.5, FLOOR), yaw=80, m='Rust#burnt', cap_m='Char#burnt')
        return
    # ---- a carga: 3 sacos, 2 galões, o estepe deitado, o tapete enrolado, as cordas
    z = FLOOR + 0.01
    cp.bag(p, (-0.80, -0.35, z), (0.80, 0.48, 0.24), yaw=8, m='Cloth#bag', seed=1)
    cp.bag(p, (-0.82, 0.22, z), (0.80, 0.48, 0.24), yaw=-6, m='Cloth#bag', seed=2)
    cp.bag(p, (-0.86, -0.08, z + 0.17), (0.78, 0.46, 0.22), yaw=88, m='Cloth#bag2', seed=3)
    vp.jerrycan(p, (-1.38, 0.55, z), yaw=90, m='Crate#can', cap_m='Dark')
    vp.jerrycan(p, (-1.38, 0.36, z), yaw=95, m='Crate#can', cap_m='Dark')
    sp = Part('spare')
    vp.tyre(sp, (-1.95, -0.30, z + 0.105), 0.355, 0.205, 1, rim_r=0.19, rubber='Rubber', rim='Metal#rim', hub='Metal#hub',
            nut_m='Metal#hub', seg=26, lugs=True, lug_h=0.016, nuts=5, dish=0.03, axis='z', style='at')
    car.hang_objs.append(sp.finish())
    cp.carpet_roll(p, (-1.75, 0.38, z + 0.12), 'x', 1.10, 0.12)
    # (v3) a tralha de quem trabalha (a caçamba do alvo é cheia e bagunçada): o botijão deitado, as tábuas atravessadas por cima dos
    # sacos, a caixa de papelão amassada, o caixote de ripa, a lona embolada no canto, o balde
    vp.lathe(p, 'Metal#gas', (-1.45, -0.45, z + 0.16), (0.15, 1.0, 0.0), [(0.0, -0.30), (0.10, -0.30), (0.15, -0.26), (0.16, -0.18),
                                                                       (0.16, 0.20), (0.13, 0.27), (0.05, 0.30), (0.05, 0.36), (0.0, 0.36)],
             seg=16, smooth=True)
    for k, (dy, dz, yaw) in enumerate(((-0.20, 0.0, 6.0), (0.02, 0.025, 3.0), (0.22, 0.0, 8.5))):
        vp.rbox(p, 'Wood#plank', (-1.05 + 0.05 * k, dy, z + 0.42 + dz), (1.55, 0.16, 0.035), yaw=yaw, pitch=-6.0 + 2 * k, r=0.01, k=1)
    vp.rbox(p, 'Cloth#box', (-2.15, 0.45, z + 0.42), (0.42, 0.40, 0.36), yaw=-14.0, roll=4.0, r=0.03, k=2)
    vp.rbox(p, 'Cloth#box', (-2.13, 0.43, z + 0.62), (0.30, 0.32, 0.05), yaw=-10.0, roll=-6.0, r=0.02, k=1)
    vp.rbox(p, 'Wood#crate', (-0.80, 0.62, z + 0.17), (0.48, 0.34, 0.32), yaw=4.0, r=0.02, k=1)
    for dz in (-0.1, 0.1):
        vp.rbox(p, 'Wood#slat', (-0.80, 0.62, z + 0.17 + dz), (0.492, 0.352, 0.05), yaw=4.0, r=0.008, k=1)
    vp.tarp_bundle(p, (-2.05, -0.36, z + 0.21), (0.55, 0.62, 0.24), yaw=20, m='Cloth#tarp', strap_m='Wood#rope', buckle_m='Dark',
                   straps=1, layers=2, seed=5.0, n=10, k=10)
    vp.lathe(p, 'Dark#bucket', (-0.62, -0.22, z + 0.36), 'z', [(0.0, 0.0), (0.11, 0.0), (0.14, 0.26), (0.13, 0.26), (0.10, 0.02),
                                                             (0.0, 0.02)], seg=12)
    cp.rope_over(p, (-0.62, -0.80, TOP - 0.03), (-0.62, 0.80, TOP - 0.03), z + 0.62)
    cp.rope_over(p, (-1.95, -0.80, TOP - 0.03), (-1.80, 0.80, TOP - 0.03), z + 0.30)


clear_scene()
obj, _ = cp.passenger_car(SPEC, BURNT, 'SM_civ_pickup_burnt' if BURNT else 'SM_civ_pickup', extra=bed, seed=11)
print('CIV', obj.name, cp.tris(obj), 'tris (antes da grade)')
export(obj, OUT)
