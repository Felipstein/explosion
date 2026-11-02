"""O antiaéreo de lagarta da USA (o Linebacker; usa_avenger no Units.json), VERSÃO B no detalhe AAA (01/11/2026): a versão A
(Game/Tools/model_usa_linebacker.py, a v5 de 25/10, o design 1 da folha que o dono escolheu) fica como está; esta é a que ele
compara com ela.

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV, as máscaras, as rodas em wheel_* e a esteira que anda):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_usa_linebacker_v2.py C:/Users/luisf/explosion/captures/game/models/usa_linebacker_v2
e depois as poses da prévia (o prato no lugar e os canhões levantados, montados dos GLB já cozidos; pose/ não vai pro jogo):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_usa_linebacker_v2.py -- \
      C:/Users/luisf/explosion/captures/game/models/usa_linebacker_v2 pose

Não há ficha de imagem de 5 vistas: o alvo é o A aprovado (o casco de lagarta tipo Bradley, a torre larga e facetada com os DOIS
CANHÕES juntos na frente, o PRATO do radar no mastro atrás, as 4 células dos mísseis no deck de trás como escotilhas de navio, a lona
azul ao lado delas), a ficha de texto (docs/elenco/usa/usa_avenger.md §6: "o prato + os dois canos"; a cor do jogador na faixa
segmentada das saias, nos painéis dos lados da torre, nos anéis das camisas e na lona) e a linguagem do tanque USA no detalhe B
(model_usa_mbt.py): as saias em chapas grossas com a fresta e os parafusos, a esteira de elos que anda, as rodas duplas com o disco
rebaixado, as grelhas de aletas, os faróis na guarda em U, a lona dobrada nas cintas. A USA é limpa (o dono, 13/10): sem desgaste
pintado.

Escala: desenhado nas medidas do A e multiplicado por SCALE 0,75 no fim (como o A); o jogo ainda aplica mesh_scale 1,25.

Pivôs MANTIDOS (Units.json usa_avenger, os números do jogo = desenho × 0,75 × 1,25): turret_pivot_m (−0,328; 0); launcher.hinge_m
(0,629; 0; 2,156), rest_pitch 8°, tubes_m (3,618; ±0,281; 2,576); spin.at_m do prato (−1,406; 0; 2,606) = a origem da malha do
prato; vls_m (as 4 células, o centro da tampa); tracks_m (2,662; 0,581); track_contact_m (−2,512; 2,325).
Muda pra usar o B: a rodagem sai em wheel_* (o executor separa as rodas: o hull do Units.json aponta pra
SM_USA_Linebacker_Wheels/StaticMeshes/wheel_*) e visual.track_step_m = o tracks.json × 1,25 (a esteira que anda; o jogo anda a fase
pela distância no mundo).

Peças (os NOMES do A; origem no chão, no centro do casco, frente em +X; todas nas coordenadas do veículo, menos o prato):
  SM_USA_Linebacker_Hull, SM_USA_Linebacker_Wheels (wheel_*), SM_USA_Linebacker_Track_l / _r, SM_USA_Linebacker_Turret,
  SM_USA_Linebacker_Guns (na pose de marcha, REST_PITCH), SM_USA_Linebacker_Dish (a ORIGEM no eixo do mastro).
Slots (os do A): Paint, Team, Cloth, Dark, Metal, Track, Rubber, Lens, Light, Yellow.
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, select_only  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
from model_china_vehicle_kit import frame_box, solid, tris  # noqa: E402

ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
OUT = ARGS[0] if ARGS else os.path.join(os.getcwd(), 'usa_linebacker_v2')
MODE = ARGS[1] if len(ARGS) > 1 else 'model'
os.makedirs(OUT, exist_ok=True)
POSE = os.path.join(OUT, 'pose')
os.makedirs(POSE, exist_ok=True)
PREFIX = 'SM_USA_Linebacker'

COLORS.update({'Paint': (196, 198, 192), 'Team': (86, 108, 150), 'Cloth': (70, 92, 140), 'Yellow': (214, 164, 32),
               'Dark': (40, 40, 38), 'Metal': (95, 95, 92), 'Track': (55, 52, 48), 'Rubber': (32, 32, 31),
               'Lens': (92, 128, 168), 'Light': (255, 230, 170)})

# ---------------------------------------------------------------------------------- medidas (desenho, antes da escala: as do A)
SCALE = 0.75
NOSE_X, REAR_X = 3.62, -3.6
DECK = 1.75
HULL_HW = 1.06
SPON_HW = 1.78
SKIRT_Y = 1.84
TRACK_Y, TRACK_W = 1.42, 0.62
SPROCKET = (3.0, 0.7, 0.36)
IDLER = (-3.12, 0.6, 0.32)
ROAD_X = [2.15, 1.25, 0.35, -0.55, -1.45, -2.35]
ROAD_Z, ROAD_R = 0.37, 0.33
ROLLER_X, ROLLER_Z, ROLLER_R = [1.7, 0.0, -1.7], 1.07, 0.1
PIVOT = (-0.35, 0.0)
RING_R = 1.1
T_Z0, T_Z1, T_Z2 = 1.86, 2.34, 2.7
HINGE = (PIVOT[0] + 1.02, 2.3)
REST_PITCH = 8.0
FIRE_PITCH = 45.0
GUN_Y = 0.3
GUN_LEN = 3.22
DISH_AT = (PIVOT[0] - 1.15, 0.0, 2.78)
DISH_R = 0.6
VLS_X = (-2.42, -3.42)
VLS_Y = (0.1, 1.38)
VLS_TOP = 1.95
SKIRT_XS = [-3.5, -2.2, -0.9, 0.4, 1.7, 2.95]
GL = math.degrees(math.atan2(DECK - 1.12, NOSE_X - 2.32))          # o glacis (a rampa do nariz)


def glacis_z(x):
    return 1.12 + (DECK - 1.12) * (NOSE_X - x) / (NOSE_X - 2.32)


def tube(p, m, a, b, r, seg=10):
    vp.tube(p, m, [a, b], r, seg=seg)


def plate_y(p, m, x0, x1, z0, z1, y, t, r=0.04, k=4):
    """A chapa em pé na face de lado (normal ±y), a espessura t a partir de y (com sinal), as tampas em quads."""
    vp.rbox_q(p, m, ((x0 + x1) / 2, y + t / 2, (z0 + z1) / 2), (x1 - x0, z1 - z0, abs(t)), roll=90, r=r, k=k)


def plate_x(p, m, y0, y1, z0, z1, x, t, r=0.04, k=4):
    vp.rbox_q(p, m, (x + t / 2, (y0 + y1) / 2, (z0 + z1) / 2), (z1 - z0, y1 - y0, abs(t)), pitch=-90, r=r, k=k)


def grille_slats(p, c, size, n, frame='Paint#plate', slat='Paint#thin', axis_n=(0, 0, 1), along=(1, 0, 0), tilt=35.0):
    """A grelha numa face (a do tanque B): o fundo escuro, a moldura de 4 barras e as aletas inclinadas ao longo de 'along'."""
    eu, ev, en = vp.face_frame(axis_n, along)
    C = Vector(c)
    L, W = size
    vp.prism(p, 'Dark#vp', vp.rounded_rect(L + 0.02, W + 0.02, 0.03, 2), C - en * 0.004, eu, ev, en, 0.01)
    fw = 0.05
    for sgn in (-1, 1):
        frame_box(p, frame, C + ev * (sgn * (W / 2 + fw / 2)) + en * 0.022, eu, ev, en, L + 2 * fw, fw, 0.045)
        frame_box(p, frame, C + eu * (sgn * (L / 2 + fw / 2)) + en * 0.022, eu, ev, en, fw, W, 0.045)
    step = W / n
    a = math.radians(tilt)
    e2 = ev * math.cos(a) + en * math.sin(a)
    e3 = en * math.cos(a) - ev * math.sin(a)
    for k in range(n):
        frame_box(p, slat, C + ev * (-W / 2 + step * (k + 0.5)) + en * 0.022, eu, e2, e3, L, step * 0.8, 0.012)


def headlight(p, c, r=0.07):
    vp.lathe(p, 'Metal', c, 'x', [(0.0, -0.06), (r * 0.7, -0.06), (r + 0.018, -0.04), (r + 0.022, 0.02), (r + 0.022, 0.045),
                                  (r + 0.012, 0.055), (r, 0.05), (r, 0.035), (0.0, 0.035)], seg=16)
    vp.lathe(p, 'Lens', c, 'x', [(0.0, 0.056), (r * 0.6, 0.054), (r * 0.95, 0.046), (r, 0.04), (0.0, 0.04)], seg=16)
    vp.lathe(p, 'Light', c, 'x', [(0.0, 0.06), (r * 0.4, 0.058), (r * 0.5, 0.054), (0.0, 0.054)], seg=12)


# ---------------------------------------------------------------------------------- casco
def hull():
    p = Part(PREFIX + '_Hull')
    p.bm('Paint#shell', 0.06)
    p.bm('Paint#plate', 0.03)
    p.bm('Paint#thin', 0.008)
    p.bm('Lens', 0.0)
    # ---------------------------------------------------------------- o casco de baixo (entre as lagartas) e os sobre-lagartas (o A)
    low = [(REAR_X + 0.05, 0.5), (3.28, 0.5), (NOSE_X, 1.12), (2.32, DECK), (REAR_X, DECK), (REAR_X - 0.04, 1.0)]
    solid(p, 'Paint#shell', [(x, s * HULL_HW, z) for x, z in low for s in (-1, 1)])
    spon = [(REAR_X, 1.22), (3.42, 1.22), (NOSE_X - 0.08, 1.2), (2.32, DECK), (REAR_X, DECK)]
    for s in (-1, 1):
        solid(p, 'Paint#shell', [(x, s * yy, z) for x, z in spon for yy in (HULL_HW - 0.02, SPON_HW)])
    # ---------------------------------------------------------------- o glacis: a chapa de reforço do meio com a solda e os parafusos,
    # as juntas, a chapa de baixo, o para-choque, as manilhas, os faróis na guarda em U
    gmx, gmz = (NOSE_X + 2.32) / 2 + 0.02, (DECK + 1.12) / 2 + 0.03
    vp.rbox_q(p, 'Paint#plate', (gmx, 0.0, gmz), (1.4, 1.5, 0.05), pitch=-GL, r=0.08, k=4)
    en = Vector((math.sin(math.radians(GL)), 0.0, math.cos(math.radians(GL))))
    eu = Vector((-math.cos(math.radians(GL)), 0.0, math.sin(math.radians(GL))))
    for y in (-0.76, 0.76):
        vp.bolt_row(p, 'Metal', Vector((gmx, y * 0.95, gmz)) + en * 0.026 + eu * 0.6, Vector((gmx, y * 0.95, gmz)) + en * 0.026 - eu * 0.6,
                    6, en, r=0.02, h=0.014)
    for s in (-1, 1):
        a_ = Vector((NOSE_X - 0.06, 0.95 * s, glacis_z(NOSE_X - 0.06) + 0.004))
        b_ = Vector((2.4, 1.6 * s, glacis_z(2.4) + 0.004))
        vp.panel_seam(p, 'Dark', a_, b_, en, w=0.024, d=0.012)
    vp.weld_bead(p, 'Paint#thin', [(NOSE_X - 0.02, -1.0, 1.125), (NOSE_X - 0.02, 1.0, 1.125)], r=0.016)
    vp.rbox_q(p, 'Paint#plate', (3.45, 0.0, 0.8), (0.66, 2.0, 0.06), pitch=61.5, r=0.06, k=4)      # a chapa de baixo do nariz
    vp.rbox(p, 'Dark', (3.29, 0.0, 0.56), (0.14, 1.8, 0.12), r=0.03, k=2)
    for s in (-1, 1):
        for d in (-1, 1):
            vp.rbox(p, 'Metal', (3.42, 0.62 * s + d * 0.055, 0.84), (0.14, 0.035, 0.15), r=0.03, k=2)
        p.cyl('Metal#vp', (3.46, 0.62 * s, 0.86), 'y', 0.022, 0.16, seg=10, bevel=0.0)
        p.torus('Dark#vp', (3.5, 0.62 * s, 0.79), 'y', 0.055, 0.017, seg=14, rseg=6)
        # o farol: a caixa de quina redonda, o farol redondo, a guarda em U de tubo
        vp.rbox_q(p, 'Paint#plate', (3.22, 1.38 * s, 1.4), (0.3, 0.32, 0.24), r=0.05, k=4)
        headlight(p, (3.39, 1.38 * s, 1.4), r=0.07)
        vp.tube(p, 'Dark', vp.bent_bar([(3.36, 1.22 * s, 1.3), (3.5, 1.22 * s, 1.3), (3.5, 1.22 * s, 1.56), (3.5, 1.54 * s, 1.56),
                                        (3.5, 1.54 * s, 1.3), (3.36, 1.54 * s, 1.3)], R=0.07, n=3), 0.018, seg=8)
        vp.rbox(p, 'Yellow', (3.375, 1.53 * s, 1.47), (0.02, 0.04, 0.06), r=0.01, k=1)
    # ---------------------------------------------------------------- o deck da frente: a escotilha do motorista com os 3 periscópios,
    # a grelha da tomada de ar do motor, as juntas e os parafusos
    vp.hatch_round(p, (2.45, 0.68, DECK), 0.3, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
    for k in range(3):
        yy = 0.42 + k * 0.26
        x = 2.84
        vp.rbox(p, 'Paint#plate', (x, yy, glacis_z(x) + 0.05), (0.16, 0.18, 0.12), pitch=-GL, r=0.03, k=3)
        vp.rbox(p, 'Lens', (x + 0.06, yy, glacis_z(x) + 0.075), (0.012, 0.14, 0.05), pitch=-GL, r=0.01, k=1)
    grille_slats(p, (2.25, -0.92, DECK), (1.0, 1.1), 7, along=(0, 1, 0), tilt=30)
    for s in (-1, 1):
        vp.panel_seam(p, 'Dark', (2.3, 1.36 * s, DECK + 0.003), (-3.4, 1.36 * s, DECK + 0.003), (0, 0, 1), w=0.024, d=0.012)
        vp.bolt_row(p, 'Metal', (2.2, 1.7 * s, DECK), (-3.45, 1.7 * s, DECK), 26, (0, 0, 1), r=0.018, h=0.012)
        for x in (1.0, -0.2, -1.9):
            vp.panel_seam(p, 'Dark', (x, 1.38 * s, DECK + 0.003), (x, 1.76 * s, DECK + 0.003), (0, 0, 1), w=0.024, d=0.012)
        for x in (3.0, -3.45):
            p.torus('Metal#vp', (x if x < 0 else 2.2, 1.6 * s, DECK + 0.035), 'x', 0.05, 0.014, seg=12, rseg=6)
    # o anel da torre (a pista escura no deck)
    vp.lathe(p, 'Dark', (PIVOT[0], 0.0, DECK), 'z', [(RING_R - 0.06, 0.0), (RING_R + 0.08, 0.0), (RING_R + 0.08, 0.016),
                                                    (RING_R + 0.06, 0.03), (RING_R - 0.06, 0.03)], seg=64, closed=True)
    # ---------------------------------------------------------------- as 4 células dos mísseis (deck de trás, esquerda do script): o bloco
    # blindado, as tampas na moldura com a fresta, a dobradiça de nós, o selo redondo e os parafusos; a faixa de aviso na frente
    vx0, vx1 = VLS_X[1], VLS_X[0]
    vy0, vy1 = VLS_Y
    vp.rbox_q(p, 'Paint#plate', ((vx0 + vx1) / 2, (vy0 + vy1) / 2, (DECK + VLS_TOP - 0.03) / 2), (vx1 - vx0, vy1 - vy0, VLS_TOP - 0.03 - DECK),
              r=0.07, k=4)
    for kk in range(2):
        for j in range(2):
            cx0 = vx0 + 0.06 + kk * (vx1 - vx0 - 0.06) / 2
            cx1 = cx0 + (vx1 - vx0 - 0.18) / 2
            cy0 = vy0 + 0.06 + j * (vy1 - vy0 - 0.06) / 2
            cy1 = cy0 + (vy1 - vy0 - 0.18) / 2
            cx, cy = (cx0 + cx1) / 2, (cy0 + cy1) / 2
            vp.rbox_q(p, 'Dark', (cx, cy, VLS_TOP - 0.03), (cx1 - cx0 + 0.05, cy1 - cy0 + 0.05, 0.014), r=0.05, k=3)
            vp.rbox_q(p, 'Paint#plate', (cx, cy, VLS_TOP - 0.012), (cx1 - cx0, cy1 - cy0, 0.035), r=0.05, k=4)
            vp.hinge(p, 'Metal', (cx1 - 0.02, cy, VLS_TOP + 0.01), 'y', cy1 - cy0 - 0.14, r=0.026, knuckles=3)
            vp.lathe(p, 'Dark', (cx - 0.02, cy, VLS_TOP + 0.005), 'z', [(0.0, 0.0), (0.11, 0.0), (0.11, 0.01), (0.09, 0.016), (0.0, 0.018)],
                     seg=20)
            for (dx, dy) in ((-1, -1), (-1, 1), (1, -1), (1, 1)):
                vp.nut(p, 'Metal', (cx + dx * ((cx1 - cx0) / 2 - 0.05), cy + dy * ((cy1 - cy0) / 2 - 0.05), VLS_TOP + 0.005), (0, 0, 1),
                       r=0.016, h=0.01)
    n_st = 6
    for k in range(n_st):
        y0 = vy0 + 0.04 + k * (vy1 - vy0 - 0.08) / n_st
        vp.rbox(p, 'Yellow' if k % 2 == 0 else 'Dark', (vx1 + 0.006, y0 + (vy1 - vy0 - 0.08) / n_st / 2, DECK + 0.1),
                (0.012, (vy1 - vy0 - 0.08) / n_st, 0.08), r=0.002, k=1)
    for (x, y) in ((vx0 + 0.1, vy1 + 0.02), (vx1 - 0.1, vy1 + 0.02)):
        p.torus('Metal#vp', (x, y, DECK + 0.12), 'x', 0.04, 0.013, seg=12, rseg=6)
    # ---------------------------------------------------------------- a lona da cor do jogador dobrada nas cintas no deck de trás (direita
    # do script), por cima da carga, e as caixas de munição sobressalentes
    # (baixa e recuada: as caixas de carga da torre passam por cima girando, a 1,97 m do eixo e a 1,92 m do chão)
    # (01/11, a revisão) a lona voltou pra perto do tamanho do A (1,14 × 1,20 contra 1,04 × 1,00; o A tinha 1,36 × 1,48): é a
    # assinatura azul de cima. Fica fora do círculo das caixas da torre (1,97 m do eixo: a borda da frente em x −2,34 no lado de
    # dentro) e não chega na grelha do escape (y −1,36)
    vp.tarp_bundle(p, (-2.91, -0.72, DECK + 0.005), (1.14, 1.20, 0.16), m='Cloth', strap_m='Dark', buckle_m='Metal', straps=3, layers=2,
                   seed=3.0, sag_ends=0.25, swap=False)
    vp.ammo_box(p, (-2.2, -1.5, DECK + 0.13), size=(0.42, 0.2, 0.26), yaw=0, m='Metal', latch_m='Dark')
    # ---------------------------------------------------------------- a grelha do escape no sobre-lagarta de trás (direita do script)
    grille_slats(p, (-2.91, -(SPON_HW - 0.22), DECK), (0.3, 0.96), 8, along=(0, 1, 0), tilt=30)
    # ---------------------------------------------------------------- a traseira: a rampa com a porta, as dobradiças, as lanternas na
    # guarda, os ganchos, os degraus
    plate_x(p, 'Dark', -0.84, 0.84, 0.66, 1.64, REAR_X - 0.012, 0.016, r=0.05, k=3)
    plate_x(p, 'Paint#plate', -0.8, 0.8, 0.7, 1.6, REAR_X - 0.04, 0.04, r=0.05, k=4)
    plate_x(p, 'Paint#plate', -0.28, 0.28, 1.0, 1.32, REAR_X - 0.07, 0.03, r=0.04, k=3)
    vp.rbox(p, 'Dark', (REAR_X - 0.072, 0.0, 1.27), (0.006, 0.16, 0.05), r=0.01, k=1)
    vp.tube(p, 'Metal', vp.bent_bar([(REAR_X - 0.07, 0.18, 1.12), (REAR_X - 0.12, 0.18, 1.12), (REAR_X - 0.12, 0.18, 1.2)], R=0.03, n=2),
            0.012, seg=6)
    for y in (-0.5, 0.5):
        vp.hinge(p, 'Metal', (REAR_X - 0.05, y, 0.72), 'y', 0.3, r=0.04, knuckles=3)
    vp.bolt_row(p, 'Metal', (REAR_X - 0.042, -0.7, 1.56), (REAR_X - 0.042, 0.7, 1.56), 10, (-1, 0, 0), r=0.016, h=0.01)
    for s in (-1, 1):
        vp.rbox_q(p, 'Paint#plate', (REAR_X - 0.04, 1.38 * s, 1.51), (0.22, 0.28, 0.08), pitch=-90, r=0.03, k=3)
        for (dy, m) in ((-0.06, 'Light'), (0.06, 'Lens')):
            vp.lathe(p, 'Dark', (REAR_X - 0.08, 1.38 * s + dy, 1.51), (-1, 0, 0), [(0.0, 0.0), (0.05, 0.0), (0.05, 0.02), (0.0, 0.02)], seg=14)
            vp.lathe(p, m, (REAR_X - 0.1, 1.38 * s + dy, 1.51), (-1, 0, 0), [(0.0, 0.0), (0.04, 0.0), (0.0, 0.008)], seg=12)
        vp.tube(p, 'Dark', vp.bent_bar([(REAR_X - 0.08, 1.22 * s, 1.4), (REAR_X - 0.16, 1.22 * s, 1.4), (REAR_X - 0.16, 1.54 * s, 1.4),
                                        (REAR_X - 0.08, 1.54 * s, 1.4)], R=0.05, n=3), 0.014, seg=6)
        for d in (-1, 1):
            vp.rbox(p, 'Metal', (REAR_X - 0.06, 0.6 * s + d * 0.055, 0.82), (0.14, 0.035, 0.15), r=0.03, k=2)
        p.cyl('Metal#vp', (REAR_X - 0.1, 0.6 * s, 0.84), 'y', 0.022, 0.16, seg=10, bevel=0.0)
        p.torus('Dark#vp', (REAR_X - 0.14, 0.6 * s, 0.77), 'y', 0.055, 0.017, seg=14, rseg=6)
        vp.rbox(p, 'Paint#plate', (REAR_X - 0.1, 0.95 * s, 0.63), (0.2, 0.18, 0.04), r=0.03, k=2)
        vp.rbox(p, 'Dark', (REAR_X - 0.1, 0.95 * s, 0.652), (0.15, 0.13, 0.006), r=0.02, k=1)
    # ---------------------------------------------------------------- as saias: 5 chapas grossas com a fresta, a ponta da frente em cunha,
    # a faixa segmentada da cor do jogador, as fileiras de parafusos, a aba de cima e o para-barro
    t = SKIRT_Y - (SKIRT_Y - 0.07)
    for s in (-1, 1):
        y_in = (SKIRT_Y - 0.07) * s
        xs = SKIRT_XS
        for a, b in zip(xs[:-1], xs[1:]):
            plate_y(p, 'Paint#plate', a + 0.016, b - 0.016, 0.8, 1.56, y_in if s > 0 else -SKIRT_Y, 0.07, r=0.06)
            yb = SKIRT_Y * s
            nb = max(3, int(round((b - a) / 0.2)))
            vp.bolt_row(p, 'Metal', (a + 0.12, yb, 1.48), (b - 0.12, yb, 1.48), nb, (0, s, 0), r=0.02, h=0.014)
            for xb in (a + 0.12, b - 0.12):
                vp.nut(p, 'Metal', (xb, yb, 0.88), (0, s, 0), r=0.02, h=0.014)
            plate_y(p, 'Team', a + 0.09, b - 0.09, 1.1, 1.34, yb if s > 0 else yb - 0.008, 0.008, r=0.02, k=2)
        # a ponta da frente em cunha (a mesma rampa do A) e o para-barro
        prof = [(xs[-1] + 0.016, 0.8), (3.25, 1.05), (3.42, 1.56), (xs[-1] + 0.016, 1.56)]
        solid(p, 'Paint#plate', [(x, yy, z) for x, z in prof for yy in (y_in, y_in + 0.07 * s)])
        vp.mudflap(p, (3.2, (SKIRT_Y - 0.035) * s, 1.0), 0.3, 0.16, yaw=90, m='Rubber')
        # a aba de cima (a linha clara entre a saia e o deck) e o friso escuro
        yy0, yy1 = sorted((SPON_HW * s, (SKIRT_Y + 0.012) * s))
        vp.rbox(p, 'Paint#thin', ((xs[0] + 3.3) / 2, (yy0 + yy1) / 2, 1.585), (3.3 - xs[0], yy1 - yy0, 0.05), r=0.02, k=2)
        vp.rbox(p, 'Dark', ((xs[0] + 0.05 + xs[-1]) / 2, (SKIRT_Y + 0.004) * s, 1.42), (xs[-1] - xs[0] - 0.05, 0.008, 0.02), r=0.004, k=1)
    vp.poke_ngons(p)
    return p.finish()


# ---------------------------------------------------------------------------------- rodagem e lagartas
def wheels():
    """A motriz dentada na frente, a roda-guia atrás, as 6 rodas de apoio duplas (a banda de borracha, o disco rebaixado, o anel de
    porcas, o vão do dente-guia) e os 3 roletes de cima. O executor separa em wheel_* (o jogo gira cada uma)."""
    p = Part(PREFIX + '_Wheels')
    for s in (-1, 1):
        y = TRACK_Y * s
        for x in ROAD_X:
            vp.road_wheel(p, (x, y, ROAD_Z), ROAD_R, 0.54, s, 'Paint', 'Metal', 'Rubber', gap=0.1, seg=24)
        ix, iz, ir = IDLER
        vp.idler(p, (ix, y, iz), ir, 0.52, s, 'Paint', 'Metal', gap=0.1)
        sx, sz, sr = SPROCKET
        vp.sprocket(p, (sx, y, sz), sr, 0.52, s, 'Paint', 'Metal', teeth=11, holes_m='Dark')
        for x in ROLLER_X:
            vp.return_roller(p, (x, y, ROLLER_Z), ROLLER_R, 0.3, 'Metal', 'Rubber')
    return p.finish()


def track(side):
    s = 1 if side == 'r' else -1
    m = 0.06
    circles = [(SPROCKET[0], SPROCKET[1], SPROCKET[2] + m + 0.01), (IDLER[0], IDLER[1], IDLER[2] + m)] + \
              [(x, ROAD_Z, ROAD_R + m) for x in ROAD_X] + [(x, ROLLER_Z, ROLLER_R + m) for x in ROLLER_X]
    zt = ROLLER_Z + ROLLER_R + m
    sags = [(ROLLER_X[1] + 0.12, ROLLER_X[0] - 0.12, zt, 0.025), (ROLLER_X[2] + 0.12, ROLLER_X[1] - 0.12, zt, 0.025)]
    return vp.track_links(f'{PREFIX}_Track_{side}', circles, TRACK_Y * s, TRACK_W, pitch=0.17, t_in=0.05, t_out=0.014,
                          grouser=0.02, guide='single', m='Track', guide_m='Metal', sags=sags, center_gap=0.1, horn_w=0.07)


# ---------------------------------------------------------------------------------- torre
def turret_plan(k_side, x_front, x_back, hw, front_hw, back_hw):
    px, py = PIVOT
    pts = [(x_front, -front_hw), (x_front, front_hw), (x_front - 0.55 * k_side, hw), (x_back + 0.42, hw), (x_back, back_hw),
           (x_back, -back_hw), (x_back + 0.42, -hw), (x_front - 0.55 * k_side, -hw)]
    return [(px + x, py + y) for x, y in pts]


def turret():
    """A torre (gira em Z no PIVOT): o corpo facetado em 3 anéis (o do A) com as juntas e os parafusos, o vão escuro dos canhões, as
    bochechas de blindagem de encaixe, os lançadores de fumaça 2 × 2, o painel da cor do jogador, as caixas de carga com as cintas,
    as antenas; no teto a mira do comandante, a escotilha, a grelha, as alças; o mastro do radar com os colares."""
    p = Part(PREFIX + '_Turret')
    p.bm('Paint#body', 0.04)
    p.bm('Paint#plate', 0.025)
    p.bm('Paint#thin', 0.008)
    px, py = PIVOT
    vp.lathe(p, 'Paint', (px, py, DECK + 0.065), 'z', [(0.0, 0.0), (RING_R - 0.02, 0.0), (RING_R - 0.02, 0.06), (RING_R - 0.05, 0.085),
                                                      (0.0, 0.085)], seg=48)
    rings = []
    for z, plan in ((T_Z0, turret_plan(0.8, 1.12, -1.42, 1.18, 0.55, 0.85)),
                    (T_Z1, turret_plan(1.0, 1.3, -1.6, 1.32, 0.62, 0.95)),
                    (T_Z2, turret_plan(1.0, 1.0, -1.5, 1.08, 0.5, 0.8))):
        rings.append([(x, y, z) for x, y in plan])
    p.loft('Paint#body', rings, bevel=0.04)
    # a solda da cintura e os parafusos do teto
    vp.weld_bead(p, 'Paint#thin', [Vector(q) + Vector((0, 0, 0.004)) for q in rings[1]] + [Vector(rings[1][0]) + Vector((0, 0, 0.004))],
                 r=0.014)
    top = [Vector(q) for q in rings[2]]
    for a_, b_ in zip(top, top[1:] + top[:1]):
        c = (a_ + b_) / 2
        cin = c + (Vector((px, py, T_Z2)) - c).normalized() * 0.07
        L = (b_ - a_).length
        n = max(2, int(L / 0.25))
        d = (b_ - a_).normalized() * (L / 2 - 0.12)
        vp.bolt_row(p, 'Metal', cin - d, cin + d, n, (0, 0, 1), r=0.017, h=0.011)
    # o vão dos canhões na frente (o mantelete é dos canhões)
    vp.rbox_q(p, 'Dark', (px + 1.1, 0.0, (T_Z0 + T_Z2) / 2 + 0.005), (0.3, 1.0, T_Z2 - T_Z0 - 0.11), r=0.05, k=3)
    for s in (-1, 1):
        # a placa de encaixe da bochecha com os parafusos
        vp.rbox_q(p, 'Paint#plate', (px + 0.98, 0.98 * s, (T_Z0 + T_Z2) / 2 + 0.02), (0.5, 0.42, 0.52), yaw=-38 * s, r=0.06, k=4)
        ex = Vector((math.cos(math.radians(-38 * s)), math.sin(math.radians(-38 * s)), 0.0))
        en_ = Vector((-ex.y, ex.x, 0.0)) * s
        cc = Vector((px + 0.98, 0.98 * s, (T_Z0 + T_Z2) / 2 + 0.02)) + en_ * 0.21
        for dz in (-0.18, 0.18):
            vp.bolt_row(p, 'Metal', cc - ex * 0.18 + Vector((0, 0, dz)), cc + ex * 0.18 + Vector((0, 0, dz)), 3, en_, r=0.018, h=0.012)
        # os lançadores de fumaça 2 × 2: a caixa e as 4 bocas com a tampa, olhando pra fora, pra frente e um pouco pra cima
        out = Vector((math.cos(math.radians(50)), s * math.sin(math.radians(50)), 0.0))
        across = Vector((-out.y, out.x, 0.0))
        bc = Vector((px + 0.74, 1.2 * s, 2.2))
        vp.rbox_q(p, 'Paint#plate', bc, (0.36, 0.2, 0.36), yaw=50 * s, r=0.04, k=3)
        d3 = (out + Vector((0.0, 0.0, 0.35))).normalized()
        for (u, w) in ((-0.085, -0.085), (-0.085, 0.085), (0.085, -0.085), (0.085, 0.085)):
            q = bc + out * 0.06 + across * u + Vector((0.0, 0.0, w))
            vp.lathe(p, 'Dark', q, d3, [(0.0, 0.0), (0.06, 0.0), (0.06, 0.14), (0.045, 0.14), (0.045, 0.1), (0.0, 0.1)], seg=12)
            vp.lathe(p, 'Metal', q + d3 * 0.14, d3, [(0.0, -0.005), (0.062, -0.005), (0.062, 0.006), (0.0, 0.006)], seg=12)
        # o painel da cor do jogador no lado da torre e a junta das placas
        yo = 1.326 * s
        plate_y(p, 'Team', px - 0.45, px + 0.25, T_Z1 - 0.3, T_Z1 + 0.12, yo if s > 0 else yo - 0.012, 0.012, r=0.03, k=3)
        vp.panel_seam(p, 'Dark', (px + 0.37, yo, T_Z0 + 0.12), (px + 0.37, yo, T_Z1 + 0.1), (0, s, 0), w=0.022, d=0.01)
        # as caixas de carga no lado de trás da torre: a tampa, as 2 cintas, as travas
        bx0, bx1 = px - 1.25, px - 0.6
        by = 1.4 * s
        vp.rbox_q(p, 'Paint#plate', ((bx0 + bx1) / 2, by, (T_Z0 + 0.06 + T_Z1 + 0.08) / 2), (bx1 - bx0, 0.24, T_Z1 + 0.02 - T_Z0), r=0.04, k=4)
        vp.panel_seam(p, 'Dark', (bx0 + 0.04, by + 0.12 * s, T_Z1 - 0.02), (bx1 - 0.04, by + 0.12 * s, T_Z1 - 0.02), (0, s, 0), w=0.016, d=0.008)
        for xx in (px - 1.1, px - 0.75):
            vp.tube(p, 'Dark', vp.bent_bar([(xx, 1.27 * s, T_Z1 + 0.1), (xx, 1.53 * s, T_Z1 + 0.1), (xx, 1.53 * s, T_Z0 + 0.06)], R=0.04, n=2),
                    0.014, seg=6)
            vp.rbox(p, 'Metal', (xx, 1.53 * s, T_Z1 - 0.2), (0.05, 0.02, 0.05), r=0.008, k=1)
        # as antenas nos cantos de trás (a base, a mola, a vareta)
        ax, ay = px - 1.42, 0.97 * s
        vp.lathe(p, 'Dark', (ax, ay, T_Z2), 'z', [(0.0, 0.0), (0.06, 0.0), (0.06, 0.04), (0.04, 0.06), (0.04, 0.12), (0.03, 0.14),
                                                 (0.03, 0.24), (0.0, 0.25)], seg=12)
        p.cyl('Metal#vp', (ax, ay, T_Z2 + 0.25 + 0.55), 'z', 0.012, 1.1, seg=6, bevel=0.0)
        # as alças de mão no teto, dos lados
        vp.grab_handle(p, 'Metal', (px - 0.9, 0.9 * s, T_Z2), (px - 0.5, 0.9 * s, T_Z2), (0, 0, 1), height=0.06, r=0.014)
    # o retalho azul atrás, na chapa de baixo da traseira (inclinada 20,6°: o pé da torre é recuado)
    tb = 0.18 / 0.48
    un = Vector((-tb, 0.0, 1.0)).normalized()
    nn = Vector((-1.0, 0.0, -tb)).normalized()
    zc = 2.2
    vp.rbox_q(p, 'Team', Vector((px - 1.42 - (zc - T_Z0) * tb, 0.0, zc)) + nn * 0.007, (0.24, 0.84, 0.012), r=0.03, k=3,
              frame=(un, Vector((0, 1, 0)), nn))
    # o teto: a mira do comandante (a caixa com o vidro e a capota), a escotilha, a grelha do ventilador
    vp.rbox_q(p, 'Paint#plate', (px + 0.485, 0.59, T_Z2 + 0.12), (0.47, 0.42, 0.24), r=0.06, k=4)
    vp.rbox_q(p, 'Dark', (px + 0.722, 0.59, T_Z2 + 0.13), (0.14, 0.32, 0.012), pitch=-90, r=0.025, k=3)
    vp.rbox(p, 'Lens', (px + 0.73, 0.59, T_Z2 + 0.13), (0.006, 0.26, 0.1), r=0.02, k=2)
    vp.rbox(p, 'Paint#plate', (px + 0.55, 0.59, T_Z2 + 0.255), (0.5, 0.46, 0.03), pitch=-4, r=0.04, k=3)
    vp.hatch_round(p, (px - 0.2, -0.55, T_Z2), 0.28, 'Paint', 'Metal', hinge_dir=(0, -1, 0), handle_m='Metal')
    grille_slats(p, (px - 0.6, 0.5, T_Z2), (0.5, 0.4), 4, along=(0, 1, 0), tilt=30)
    # o mastro do radar: a base, o mastro com os colares, o mancal de cima (a cabeça que gira é o Dish)
    dx, dy, dz = DISH_AT
    vp.rbox_q(p, 'Paint#plate', (dx, dy, T_Z2 + 0.04), (0.46, 0.46, 0.08), r=0.08, k=4)
    vp.bolt_ring(p, 'Metal', (dx, dy, T_Z2 + 0.08), 'z', 0.17, 8, r=0.016, h=0.01)
    vp.lathe(p, 'Metal', (dx, dy, T_Z2 + 0.08), 'z', [(0.0, 0.0), (0.12, 0.0), (0.12, 0.03), (0.09, 0.05), (0.08, dz - T_Z2 - 0.2),
                                                     (0.095, dz - T_Z2 - 0.18), (0.095, dz - T_Z2 - 0.14), (0.08, dz - T_Z2 - 0.12),
                                                     (0.0, dz - T_Z2 - 0.12)], seg=16)
    vp.lathe(p, 'Dark', (dx, dy, dz - 0.04), 'z', [(0.0, 0.0), (0.13, 0.0), (0.13, 0.03), (0.11, 0.04), (0.0, 0.04)], seg=18)
    tube(p, 'Dark', (dx - 0.1, dy + 0.06, T_Z2 + 0.1), (dx - 0.09, dy + 0.06, dz - 0.1), 0.014, seg=6)        # o cabo do radar
    vp.poke_ngons(p)
    return p.finish()


def guns():
    """O mantelete e os 2 canhões, desenhados deitados e girados REST_PITCH na dobradiça (a pose de marcha): o munhão, o mantelete
    facetado com os parafusos, a camisa grossa com a faixa da cor do jogador e os anéis, o cano com a braçadeira, o freio de boca com
    as janelas e a boca funda; as calhas de munição entrando no mantelete."""
    p = Part(PREFIX + '_Guns')
    p.bm('Paint#plate', 0.03)
    hx, hz = HINGE
    vp.lathe(p, 'Dark', (hx, 0.0, hz), 'y', [(0.0, -0.51), (0.11, -0.51), (0.13, -0.49), (0.13, 0.49), (0.11, 0.51), (0.0, 0.51)], seg=18)
    p.prism_xz('Paint#plate', [(hx - 0.22, hz - 0.3), (hx + 0.2, hz - 0.3), (hx + 0.38, hz - 0.15), (hx + 0.38, hz + 0.15),
                               (hx + 0.2, hz + 0.3), (hx - 0.22, hz + 0.3)], -0.56, 0.56)
    vp.rbox(p, 'Dark', (hx + 0.385, 0.0, hz), (0.012, 1.04, 0.024), r=0.006, k=1)
    for z in (hz - 0.22, hz + 0.22):
        vp.bolt_row(p, 'Metal', (hx + 0.3, -0.48, z), (hx + 0.3, 0.48, z), 7, (0.6, 0, 0.8 if z > hz else -0.8), r=0.018, h=0.012)
    for s in (-1, 1):
        y = GUN_Y * s
        x0 = hx + 0.36
        # a camisa (o recuo): o colar, o corpo grosso afinando, a faixa da cor do jogador, os anéis escuros
        vp.lathe(p, 'Paint', (x0, y, hz), 'x', [(0.0, 0.0), (0.175, 0.0), (0.175, 0.06), (0.158, 0.08), (0.155, 0.7), (0.148, 0.9),
                                               (0.13, 1.1), (0.0, 1.1)], seg=28)
        vp.lathe(p, 'Dark', (x0 + 0.06, y, hz), 'x', [(0.17, -0.012), (0.18, -0.012), (0.18, 0.012), (0.17, 0.012)], seg=28, closed=True)
        vp.lathe(p, 'Team', (x0 + 0.69, y, hz), 'x', [(0.155, -0.075), (0.164, -0.075), (0.166, -0.07), (0.166, 0.07), (0.164, 0.075),
                                                     (0.155, 0.075)], seg=28, closed=True)
        vp.lathe(p, 'Dark', (hx + 1.47, y, hz), 'x', [(0.0, -0.03), (0.12, -0.03), (0.125, -0.02), (0.125, 0.02), (0.12, 0.03), (0.0, 0.03)],
                 seg=22)
        # o cano, a braçadeira, o freio de boca com as 2 janelas e a boca funda
        L = GUN_LEN - 0.3 - 1.47
        vp.lathe(p, 'Metal', (hx + 1.47, y, hz), 'x', [(0.0, 0.0), (0.08, 0.0), (0.074, 0.08), (0.07, L), (0.0, L)], seg=18)
        vp.lathe(p, 'Dark', (hx + 2.3, y, hz), 'x', [(0.07, -0.04), (0.09, -0.04), (0.094, -0.02), (0.094, 0.02), (0.09, 0.04), (0.07, 0.04)],
                 seg=18, closed=True)
        mb = hx + GUN_LEN - 0.3
        vp.lathe(p, 'Dark', (mb, y, hz), 'x', [(0.0, 0.0), (0.09, 0.0), (0.11, 0.02), (0.11, 0.27), (0.1, 0.3), (0.05, 0.3), (0.05, 0.28),
                                              (0.0, 0.28)], seg=18)
        for d in (-1, 1):
            for k in range(2):
                vp.rbox(p, 'Metal', (mb + 0.09 + k * 0.1, y + d * 0.108, hz), (0.07, 0.012, 0.1), r=0.012, k=1)
        # a calha da munição pelo lado de fora, entrando no mantelete
        vp.chute(p, 'Dark', [(hx + 0.8, y + 0.2 * s, hz - 0.02), (hx + 0.6, y + 0.21 * s, hz - 0.02), (hx + 0.4, y + 0.18 * s, hz - 0.03)],
                 0.09, 0.12, pitch=0.06)
    vp.poke_ngons(p)
    o = p.finish()
    rot = Matrix.Translation(V(hx, 0.0, hz)) @ Matrix.Rotation(math.radians(-REST_PITCH), 4, 'Y') @ Matrix.Translation(-V(hx, 0.0, hz))
    o.data.transform(rot)
    return o


def dish():
    """O prato do radar com a cabeça giratória: a ORIGEM no eixo do mastro (o jogo gira a peça em Z em volta dela). A casca do
    parabolóide do A (fechada, a frente côncava e o dorso), as nervuras, o aro, o alimentador nas 3 hastes; a cabeça (o transmissor)
    de quina redonda com as aletas de refrigeração, o mancal."""
    p = Part(PREFIX + '_Dish')
    p.bm('Paint#plate', 0.025)
    vp.lathe(p, 'Dark', (0.0, 0.0, 0.0), 'z', [(0.0, 0.0), (0.12, 0.0), (0.13, 0.02), (0.13, 0.05), (0.1, 0.06), (0.0, 0.06)], seg=18)
    vp.rbox_q(p, 'Paint#plate', (-0.02, 0.0, 0.175), (0.36, 0.32, 0.25), r=0.05, k=4)
    vp.rbox(p, 'Lens', (0.162, 0.0, 0.175), (0.008, 0.16, 0.09), r=0.015, k=1)
    for k in range(4):
        vp.rbox(p, 'Dark', (-0.205, 0.0, 0.1 + k * 0.05), (0.012, 0.24, 0.022), r=0.006, k=1)
    for s in (-1, 1):
        vp.bolt_row(p, 'Metal', (-0.14, 0.162 * s, 0.27), (0.1, 0.162 * s, 0.27), 4, (0, s, 0), r=0.012, h=0.008)
    c = Vector((0.3, 0.0, 0.38))
    tilt = math.radians(25)
    ax = Vector((math.cos(tilt), 0.0, math.sin(tilt)))
    up = Vector((-math.sin(tilt), 0.0, math.cos(tilt)))
    side = Vector((0.0, 1.0, 0.0))
    depth = 0.13
    rings_front, rings_back = [], []
    nr, na = 7, 36
    for i in range(1, nr + 1):
        r = DISH_R * i / nr
        d = depth * (r / DISH_R) ** 2
        rf, rb = [], []
        for k in range(na):
            a = 2 * math.pi * k / na
            q = c + up * (r * math.cos(a)) + side * (r * math.sin(a)) - ax * (depth - d)
            rf.append(tuple(q))
            rb.append(tuple(q - ax * 0.025))
        rings_front.append(rf)
        rings_back.append(rb)
    p.loft('Paint#dish', rings_front + list(reversed(rings_back)), bevel=0.0, smooth=True, caps=(True, True))
    for k in range(8):
        a = 2 * math.pi * k / 8
        pts = []
        for i in range(2, nr + 1):
            r = DISH_R * i / nr
            pts.append(c + up * (r * math.cos(a)) + side * (r * math.sin(a)) - ax * (depth - depth * (r / DISH_R) ** 2 - 0.008))
        vp.tube(p, 'Dark', pts, 0.009, seg=6)
    rim = []
    for rr, off in ((DISH_R + 0.025, 0.02), (DISH_R + 0.025, -0.045), (DISH_R - 0.01, -0.045), (DISH_R - 0.01, 0.02), (DISH_R + 0.025, 0.02)):
        rim.append([tuple(c + up * (rr * math.cos(2 * math.pi * k / na)) + side * (rr * math.sin(2 * math.pi * k / na)) + ax * off)
                    for k in range(na)])
    p.loft('Dark', rim, bevel=0.0, smooth=True, caps=(False, False))
    for k in range(4):
        a = math.radians(45 + 90 * k)
        tip = c - ax * (depth + 0.01) + up * (DISH_R * 0.92 * math.cos(a)) + side * (DISH_R * 0.92 * math.sin(a))
        base = c - ax * (depth + 0.06)
        vp.tube(p, 'Dark', [base, tip], 0.014, seg=6)
    arm = (c - ax * (depth + 0.06) + Vector((-0.02, 0.0, -0.14)))
    vp.rbox(p, 'Paint#plate', ((0.1 + arm.x + 0.04) / 2, 0.0, 0.27), (arm.x + 0.04 - 0.1, 0.14, 0.1), r=0.03, k=3)
    f = c + ax * 0.3
    vp.lathe(p, 'Dark', f - ax * 0.06, ax, [(0.0, 0.0), (0.05, 0.0), (0.05, 0.09), (0.035, 0.12), (0.0, 0.12)], seg=12)
    for k in range(3):
        a = math.radians(90 + 120 * k)
        b = c - ax * 0.02 + up * (DISH_R * 0.85 * math.cos(a)) + side * (DISH_R * 0.85 * math.sin(a))
        vp.tube(p, 'Metal', [b, f], 0.011, seg=6)
    vp.poke_ngons(p)
    return p.finish()


def lift_matrix(pitch_deg):
    hx, hz = HINGE[0] * SCALE, HINGE[1] * SCALE
    return (Matrix.Translation(Vector((hx, 0.0, hz))) @ Matrix.Rotation(math.radians(-pitch_deg), 4, 'Y')
            @ Matrix.Translation(Vector((-hx, 0.0, -hz))))


def sweep_check(h, t, g, d_at):
    """A torre girando (os canhões deitados e levantados até 80°, e o prato no lugar) não pode atravessar o casco (as células, a lona,
    os faróis); e o prato girando não pode bater nos canhões. Colisão de verdade (BVH), nas malhas já na escala (a do A)."""
    def tree(o, M):
        vs = [M @ v.co for v in o.data.vertices]
        return BVHTree.FromPolygons(vs, [tuple(q.vertices) for q in o.data.polygons])
    hull_t = tree(h, Matrix.Identity(4))
    piv = V(PIVOT[0] * SCALE, PIVOT[1] * SCALE, 0.0)
    for label, pitch in (('deitado', 0.0), ('a 40 graus', 40.0 - REST_PITCH), ('a 80 graus', 80.0 - REST_PITCH)):
        hits = []
        for k in range(72):
            Y = Matrix.Translation(piv) @ Matrix.Rotation(math.radians(5 * k), 4, 'Z') @ Matrix.Translation(-piv)
            n = len(hull_t.overlap(tree(t, Y))) + len(hull_t.overlap(tree(g, Y @ lift_matrix(pitch))))
            n += len(hull_t.overlap(tree(d_at, Y)))
            if n:
                hits.append((5 * k, n))
        print('MODEL giro da torre (%s): %s' % (label, 'sem colisao em 360 graus' if not hits else 'COLISAO em %s' % hits[:12]))
    hits = []
    da = V(DISH_AT[0] * SCALE, DISH_AT[1] * SCALE, DISH_AT[2] * SCALE)
    for pitch in (0.0, 40.0 - REST_PITCH, 80.0 - REST_PITCH):
        gt = tree(g, lift_matrix(pitch))
        for k in range(36):
            M = Matrix.Translation(da) @ Matrix.Rotation(math.radians(10 * k), 4, 'Z') @ Matrix.Translation(-da)
            n = len(gt.overlap(tree(d_at, M)))
            if n:
                hits.append((round(pitch + REST_PITCH), 10 * k, n))
    print('MODEL o prato girando contra os canhoes: %s' % ('livre' if not hits else 'COLISAO %s' % hits[:12]))


def write_spec():
    import json
    rel = 'captures/game/models/' + os.path.basename(OUT.rstrip('/\\')) + '/pose/'
    base = [[PREFIX + n, [0, 0, 0]] for n in ('_Hull', '_Wheels', '_Track_l', '_Track_r')]
    views = [['front', 'ortho', [-1, 0, 0], [0, 0, 1], [900, 900]],
             ['back', 'ortho', [1, 0, 0], [0, 0, 1], [900, 900]],
             ['left', 'ortho', [0, -1, 0], [0, 0, 1], [1800, 820]],
             ['top', 'ortho', [0, 0, -1], [0, -1, 0], [1800, 1000]],
             ['three_quarter', 'persp', 27.0, 232.0, [1600, 1000], 30.0, 0.8],
             ['three_quarter_right', 'persp', 30.0, 40.0, [1600, 1000], 30.0, 0.8]]
    game = [['game_cam', 37.5, 35.0, 50.0, 0.8], ['game_cam_close', 37.5, 35.0, 28.0, 0.75]]
    spec = {'parts': base + [[PREFIX + '_Turret', [0, 0, 0]], [PREFIX + '_Guns', [0, 0, 0]], [rel + PREFIX + '_Dish_Placed', [0, 0, 0]]],
            'views': views, 'usa_at': [0, 6.0, 0], 'tank_views': False, 'game': game}
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)
    fire = {'parts': base + [[rel + PREFIX + '_Turret_Fire', [0, 0, 0]]],
            'views': [['fire_left', 'ortho', [0, -1, 0], [0, 0, 1], [1800, 1000]],
                      ['fire_three_quarter', 'persp', 27.0, 232.0, [1600, 1000], 30.0, 0.8],
                      ['fire_three_quarter_right', 'persp', 30.0, 40.0, [1600, 1000], 30.0, 0.8]],
            'usa_at': [0, 6.0, 0], 'tank_views': False, 'game': [['fire_game_cam_close', 37.5, 35.0, 28.0, 0.75]]}
    with open(os.path.join(OUT, 'preview_fire.json'), 'w', encoding='utf-8') as f:
        json.dump(fire, f, ensure_ascii=False, indent=1)


def make_pose():
    """As poses da prévia (pose/, fora da importação), dos GLB já cozidos: o prato no lugar do mastro (Dish_Placed) e a torre com os
    canhões levantados FIRE_PITCH e o prato virado 35° (Turret_Fire)."""
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)

    def load(n):
        before = set(bpy.data.objects)
        bpy.ops.import_scene.gltf(filepath=os.path.join(OUT, PREFIX + n + '.glb'))
        new = [o for o in bpy.data.objects if o not in before and o.type == 'MESH']
        bpy.context.view_layer.update()
        for o in new:
            o.data.transform(o.matrix_world)
            o.matrix_world = Matrix.Identity(4)
        return new

    def join(objs, name):
        select_only(objs[0])
        for o in objs:
            o.select_set(True)
        bpy.ops.object.join()
        o = bpy.context.view_layer.objects.active
        o.name = name
        return o

    def write(o):
        select_only(o)
        path = os.path.join(POSE, o.name + '.glb')
        bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_selection=True, export_apply=True, export_yup=True,
                                  export_materials='EXPORT', export_normals=True, export_texcoords=True, export_vertex_color='ACTIVE',
                                  export_all_vertex_colors=False)
        print('MODEL pose ->', path)
    k = SCALE
    da = V(DISH_AT[0] * k, DISH_AT[1] * k, DISH_AT[2] * k)
    d = load('_Dish')
    for o in d:
        o.data.transform(Matrix.Translation(da))
    write(join(d, PREFIX + '_Dish_Placed'))
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    t = load('_Turret')
    g = load('_Guns')
    for o in g:
        o.data.transform(lift_matrix(FIRE_PITCH - REST_PITCH))
    d = load('_Dish')
    for o in d:
        o.data.transform(Matrix.Translation(da) @ Matrix.Rotation(math.radians(-35), 4, 'Z'))
    write(join(t + g + d, PREFIX + '_Turret_Fire'))


def main():
    clear_scene()
    h = hull()
    w = wheels()
    tl, tr = track('l'), track('r')
    t = turret()
    g = guns()
    d = dish()
    pre = {o.name: tris(o) for o in (h, w, tl, tr, t, g, d)}
    for o in (h, w, tl, tr, t, g, d):
        o.data.transform(Matrix.Scale(SCALE, 4))
    # a esteira que anda: o centro de cada elo e o passo (o track_links grava no desenho) vão pra escala da malha, senão o executor
    # casa cada elo com o centro errado (o model_run_detail.track_uv procura o elo mais perto de cada ilha)
    for o in (tl, tr):
        fr = list(o['track_frames'])
        for i in range(0, len(fr), 5):
            fr[i], fr[i + 1], fr[i + 2] = fr[i] * SCALE, fr[i + 1] * SCALE, fr[i + 2] * SCALE
        o['track_frames'] = fr
        o['track_step'] = o['track_step'] * SCALE
    da = V(DISH_AT[0] * SCALE, DISH_AT[1] * SCALE, DISH_AT[2] * SCALE)
    # (o truque do Dozer B) o prato cozinha no lugar do mastro e volta pra origem só na exportação
    d.data.transform(Matrix.Translation(da))
    sweep_check(h, t, g, d)
    import model_lib
    if model_lib.DETAIL.get('defer') is not None:
        _orig = model_lib.export

        def _export(o, out_dir):
            if o.name == PREFIX + '_Dish' and model_lib.DETAIL.get('defer') is None:
                o.data.transform(Matrix.Translation(-da))
            return _orig(o, out_dir)
        model_lib.export = _export
    else:
        d.data.transform(Matrix.Translation(-da))
    total = 0
    for o in (h, w, tl, tr, t, g, d):
        total += export(o, OUT)
    print('MODEL total', total, 'tris (antes da grade do executor); por peca', pre)
    k = SCALE
    print('MODEL turret_pivot_m (malha)', [round(PIVOT[0] * k, 4), 0.0], '(jogo x1,25)', [round(PIVOT[0] * k * 1.25, 3), 0.0])
    print('MODEL launcher hinge_m (malha)', [round(HINGE[0] * k, 4), 0.0, round(HINGE[1] * k, 4)], '(jogo)',
          [round(HINGE[0] * k * 1.25, 3), 0.0, round(HINGE[1] * k * 1.25, 3)], 'rest_pitch', REST_PITCH)
    hx, hz = HINGE
    a = math.radians(REST_PITCH)
    mx, mz = hx + GUN_LEN * math.cos(a), hz + GUN_LEN * math.sin(a)
    print('MODEL bocas tubes_m (jogo):', [[round(mx * k * 1.25, 3), round(s * GUN_Y * k * 1.25, 3), round(mz * k * 1.25, 3)] for s in (-1, 1)])
    print('MODEL spin do radar at_m (jogo)', [round(DISH_AT[0] * k * 1.25, 3), 0.0, round(DISH_AT[2] * k * 1.25, 3)])
    print('MODEL tracks_m (jogo)', [round(2 * TRACK_Y * k * 1.25, 3), round(TRACK_W * k * 1.25, 3)])
    write_spec()


if MODE == 'pose':
    make_pose()
else:
    main()
