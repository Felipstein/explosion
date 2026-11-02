"""O pesado da Rússia (T-14 Armata) provisório, modelado no Blender por script (01/11/2026; docs/FACCAO_RUSSIA.md §5.2: "a torre sem
tripulação e o casco longo de 7 rodas").

Rodar (Blender sem janela, só CPU; não usa a placa), pelo executor do detalhe:
  (model_run_detail.py) vehicle Game/Tools/model_russia_t14.py C:/Users/luisf/explosion/captures/game/models/russia_t14

A base é o T-14 de verdade, na escala de verdade: casco de 8,7 m (10,8 com o cano), 3,6 m de largura com a saia, o deck alto (1,7 m) com
a CÁPSULA DA TRIPULAÇÃO na frente (as 3 escotilhas em fila e os periscópios), o glacis íngreme com o nariz em quilha, 7 rodas por lado, a
saia com os blocos grandes do Malakhit e a aba de borracha, a grade de barras atrás; a TORRE SEM TRIPULAÇÃO baixa e retangular (a frente
em cunha rasa com os tijolos, os lados retos, o bustle), as miras do atirador e do comandante (a panorâmica alta), a estação remota
pequena atrás, o cinto do Afganit em volta do pé da torre (os tubos de lançar e os 4 painéis do radar nos cantos), o canhão 2A82 longo
com a capa térmica; a gaiola LEVE (ripas mais espaçadas, só a parte de trás) com a lona e a rede da cor do jogador.
Maior que o T-90M (6,9 m de casco), menor que o Overlord da China (10,5 m).
A palavra: o CASCO LONGO E A TORRE BAIXA. De cima: o retângulo comprido, as 3 escotilhas em fila na frente, a torre caixote mais pra
trás com a gaiola leve, o cano comprido.

Peças (origem no chão, no centro do casco, frente em +X):
  SM_Russia_T14_Hull     casco, saias, deck, a lona enrolada, o anel;
  SM_Russia_T14_Turret   a torre (gira em Z em volta de (TX, 0)), as miras, o Afganit, a gaiola;
  SM_Russia_T14_Barrel   o canhão (recua em −X; a boca é o X máximo);
  SM_Russia_T14_Wheels   a rodagem (wheel_*);
  SM_Russia_T14_Track_l / _Track_r  as esteiras.
Slots: Paint, Metal, Team, Track, Lens, Cloth, Rubber, Red, White, Dark.
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import clear_scene, export  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_russia_vehicle_kit as rk  # noqa: E402
from model_russia_vehicle_kit import bounds, frame_box, solid, sweep_check, tris  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'russia_t14')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_Russia_T14'


class T14:
    NOSE, REAR = 4.35, -4.35
    BELLY, DECK = 0.5, 1.7
    LOW_HW = 1.05
    UP_HW = 1.62                  # o casco de cima cobre as esteiras (o T-14 é uma caixa larga)
    SPON_Z = 1.12
    SKIRT = (1.66, 1.74)
    TRACK_Y, TRACK_W = 1.4, 0.64
    WHEEL_R = 0.36
    WHEEL_Z = 0.45
    WHEELS_X = [3.0, 2.0, 1.0, 0.0, -1.0, -2.0, -3.0]
    IDLER = (3.86, 0.66, 0.28)
    SPROCKET = (-3.95, 0.7, 0.3)
    ROLLERS_X, ROLLER_Z, ROLLER_R = [2.5, 0.5, -1.5], 0.94, 0.075
    GLACIS = ((4.35, 1.18), (3.1, 1.7))
    RING_X, RING_R = -0.35, 1.12


C = T14
TX = C.RING_X
T_Z0, T_Z1, T_Z2 = 1.76, 2.18, 2.42
GUN_Z = 2.08
MANTLET_X = TX + 1.9
MUZZLE_X = 6.45                  # 10,8 m com o cano
CAGE = dict(x0=TX - 2.15, x1=TX + 0.55, hw=1.32, z=2.98)


def glacis_z(x):
    (x0, z0), (x1, z1) = C.GLACIS
    return z0 + (z1 - z0) * (x - x0) / (x1 - x0)


# ---------------------------------------------------------------------------------- casco
def hull():
    p = rk.new_part(PREFIX + '_Hull')
    low = [(3.95, C.BELLY), (C.NOSE - 0.05, 0.98), (C.NOSE - 0.05, C.SPON_Z), (C.REAR + 0.03, C.SPON_Z), (C.REAR + 0.03, 0.72),
           (-4.1, C.BELLY)]
    solid(p, 'Paint#shell', [(x, s * C.LOW_HW, z) for x, z in low for s in (-1, 1)])
    (gx0, gz0), (gx1, gz1) = C.GLACIS
    pts = []
    for s in (-1, 1):
        # o nariz em quilha: o meio mais à frente, as quinas recuadas (de cima: a ponta rasa)
        pts += [(C.NOSE, 0.5 * s, C.SPON_Z - 0.04), (C.NOSE, 0.5 * s, gz0), (C.NOSE - 0.32, C.UP_HW * s, C.SPON_Z - 0.04),
                (C.NOSE - 0.32, C.UP_HW * s, glacis_z(C.NOSE - 0.32)), (gx1, C.UP_HW * s, gz1), (-4.25, C.UP_HW * s, C.DECK),
                (C.REAR, C.UP_HW * s, 1.6), (C.REAR, C.UP_HW * s, C.SPON_Z - 0.04)]
    solid(p, 'Paint#shell', pts)
    # o glacis: os tijolos em 2 fileiras (o Malakhit) e a régua de respingo
    eu = Vector((gx1 - gx0, 0.0, gz1 - gz0)).normalized()
    en = Vector((-eu.z, 0.0, eu.x)) * -1.0
    if en.z < 0:
        en = -en
    L = (Vector((gx1, 0, gz1)) - Vector((gx0, 0, gz0))).length
    rk.era_grid(p, Vector((gx0 - 0.05, -1.3, gz0 + 0.02)) + eu * 0.25, eu, Vector((0, 1, 0)), en, 2, 8, (L - 0.45) / 2, 2.6 / 8, 0.09)
    # a cápsula da tripulação: as 3 escotilhas em fila, os periscópios na frente de cada uma
    for y in (-0.62, 0.0, 0.62):
        vp.hatch_round(p, (2.55, y, C.DECK), 0.25, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
        for d in (-0.12, 0.12):
            vp.rbox(p, 'Paint#plate', (2.92, y + d, C.DECK + 0.05), (0.14, 0.18, 0.1), r=0.03, k=2)
            p.obox('Lens', (2.995, y + d, C.DECK + 0.06), (0.012, 0.14, 0.04), bevel=0.0)
    # os faróis nos 2 cantos da frente (no recorte do nariz) com a grade, os ganchos
    for s in (-1, 1):
        rk.headlight(p, (C.NOSE - 0.36, 1.32 * s, C.SPON_Z + 0.08), r=0.07)
        for d in (-1, 1):
            vp.rbox(p, 'Metal', (C.NOSE + 0.0, 0.75 * s + d * 0.055, 0.92), (0.12, 0.035, 0.15), r=0.03, k=2)
            vp.rbox(p, 'Metal', (C.REAR - 0.02, 0.75 * s + d * 0.055, 0.92), (0.12, 0.035, 0.15), r=0.03, k=2)
        p.torus('Dark#vp', (C.NOSE + 0.05, 0.75 * s, 0.86), 'y', 0.055, 0.017, seg=14, rseg=6)
        p.torus('Dark#vp', (C.REAR - 0.07, 0.75 * s, 0.86), 'y', 0.055, 0.017, seg=14, rseg=6)
    # ---------------------------------------------------------------- a saia: os 3 blocos grandes do Malakhit na frente, as chapas, a aba
    # de borracha embaixo, a grade de barras atrás
    for s in (-1, 1):
        yi = C.SKIRT[0] * s
        t = C.SKIRT[1] - C.SKIRT[0]
        cuts = [3.95, 2.75, 1.55, 0.35]
        for k in range(3):
            xa, xb = cuts[k + 1] + 0.02, cuts[k] - 0.02
            vp.rbox(p, 'Paint#plate', ((xa + xb) / 2, (C.SKIRT[0] + t / 2) * s, 1.12), (xb - xa, 0.6, t), roll=90, r=0.05, k=2)
            frame_box(p, 'Paint#plate', ((xa + xb) / 2, (C.SKIRT[1] + 0.06) * s, 1.12), Vector((1, 0, 0)), Vector((0, 0, 1)),
                      Vector((0, s, 0)), xb - xa - 0.06, 0.5, 0.12)
            rk.era_grid(p, Vector((xa + 0.05, (C.SKIRT[1] + 0.12) * s, 0.9)), Vector((1, 0, 0)), Vector((0, 0, 1)), Vector((0, s, 0)),
                        3, 1, (xb - xa - 0.1) / 3, 0.44, 0.03, bolts=True)
            vp.mudflap(p, ((xa + xb) / 2, (C.SKIRT[0] + t / 2) * s, 0.84), xb - xa, 0.28, yaw=90 * s, m='Rubber', t=0.022, folds=1)
        for (xa, xb) in ((0.33, -0.95),):
            vp.rbox(p, 'Paint#plate', ((xa + xb) / 2, (C.SKIRT[0] + t / 2) * s, 1.1), (xa - xb, 0.62, t), roll=90, r=0.05, k=2)
            vp.mudflap(p, ((xa + xb) / 2, (C.SKIRT[0] + t / 2) * s, 0.8), xa - xb, 0.28, yaw=90 * s, m='Rubber', t=0.022, folds=1)
        rk.slat_screen(p, -0.98, C.REAR + 0.15, 0.74, C.SPON_Z + 0.25, yi, s)
    # o forro escuro do casco de baixo atrás das rodas
    for s in (-1, 1):
        yy0, yy1 = sorted((C.LOW_HW * s, (C.LOW_HW + 0.015) * s))
        p.box('Dark', -4.0, 3.9, yy0, yy1, 0.55, C.SPON_Z - 0.05, bevel=0.0)
    # ---------------------------------------------------------------- o deck: o anel, as caixas, o motor atrás, a lona enrolada
    vp.lathe(p, 'Dark', (TX, 0.0, C.DECK), 'z', [(C.RING_R - 0.06, 0.0), (C.RING_R + 0.06, 0.0), (C.RING_R + 0.06, 0.012),
                                                (C.RING_R + 0.045, 0.02), (C.RING_R - 0.06, 0.02)], seg=64, closed=True)
    for s in (-1, 1):
        for (xa, xb) in ((2.95, 2.15), (-2.8, -3.6)):          # (fora do giro da torre: raio > 2,4 m do pivô)
            vp.rbox(p, 'Paint#plate', ((xa + xb) / 2, 1.38 * s, C.DECK + 0.1), (xa - xb, 0.42, 0.2), r=0.04, k=2)
            vp.rbox(p, 'Paint#plate', ((xa + xb) / 2, 1.38 * s, C.DECK + 0.215), (xa - xb + 0.04, 0.45, 0.03), r=0.045, k=2)
    rk.grille_deck(p, -3.95, -2.85, -1.25, -0.05, C.DECK, 8, along='y')
    rk.grille_deck(p, -3.95, -2.85, 0.05, 1.25, C.DECK, 8, along='y')
    vp.panel_seam(p, 'Dark', (-2.75, -1.4, C.DECK + 0.003), (-2.75, 1.4, C.DECK + 0.003), (0, 0, 1), w=0.024, d=0.01)
    vp.tarp_roll(p, (C.REAR + 0.16, 0.0, C.DECK + 0.1), 'y', 2.3, 0.12, m='Team', straps=3, strap_m='Cloth', buckle_m='Metal')
    # a traseira: a grade grande do escape, as lanternas, a grade de barras
    p.box('Dark', C.REAR - 0.012, C.REAR, -1.0, 1.0, 1.15, 1.5, bevel=0.0)
    for k in range(7):
        vp.rbox(p, 'Metal', (C.REAR - 0.02, 0.0, 1.18 + k * 0.045), (0.03, 1.95, 0.022), pitch=-30, r=0.008, k=1)
    for s in (-1, 1):
        vp.rbox(p, 'Dark', (C.REAR - 0.03, 1.35 * s, 1.5), (0.06, 0.16, 0.1), r=0.03, k=2)
        p.box('Lens', C.REAR - 0.07, C.REAR - 0.05, 1.35 * s - 0.05, 1.35 * s + 0.05, 1.47, 1.53, bevel=0.0)
    rk.slat_screen_x(p, -1.4, 1.4, 0.75, 1.1, C.REAR, -1)
    vp.tow_cable(p, 'Metal', [(C.REAR - 0.18, -0.95, 0.82), (C.REAR - 0.2, -0.7, 0.74), (C.REAR - 0.2, 0.7, 0.74),
                              (C.REAR - 0.18, 0.95, 0.82)], r=0.022)
    return p.finish()


# ---------------------------------------------------------------------------------- torre
def turret():
    p = rk.new_part(PREFIX + '_Turret')
    pts = []
    for s in (-1, 1):
        pts += [(TX + 1.75, 0.45 * s, T_Z0), (TX + 1.35, 1.3 * s, T_Z0), (TX - 1.75, 1.3 * s, T_Z0), (TX - 1.95, 1.1 * s, T_Z0)]
        pts += [(TX + 1.78, 0.46 * s, T_Z1), (TX + 1.36, 1.32 * s, T_Z1), (TX - 1.75, 1.32 * s, T_Z1), (TX - 1.95, 1.12 * s, T_Z1)]
        pts += [(TX + 1.45, 0.42 * s, T_Z2), (TX + 1.15, 1.18 * s, T_Z2), (TX - 1.7, 1.2 * s, T_Z2), (TX - 1.9, 1.02 * s, T_Z2)]
    solid(p, 'Paint#shell', pts)
    vp.lathe(p, 'Paint', (TX, 0.0, C.DECK), 'z', [(0.0, 0.03), (1.05, 0.03), (1.05, 0.06), (1.0, 0.07), (0.0, 0.07)], seg=56)
    # a frente em cunha: os tijolos nas 2 bochechas
    for s in (-1, 1):
        a, b = Vector((TX + 1.77, 0.47 * s, 0)), Vector((TX + 1.36, 1.31 * s, 0))
        d = (b - a).normalized()
        n = Vector((d.y, -d.x, 0.0))
        if n.x < 0:
            n = -n
        rk.era_grid(p, a + Vector((0, 0, T_Z0 + 0.04)), d, Vector((0, 0, 1)), n, 3, 2, (b - a).length / 3, (T_Z1 - T_Z0 - 0.08) / 2, 0.06)
    # o mantelete estreito com o fole e a coaxial
    vp.rbox(p, 'Paint#plate', (TX + 1.78, 0.0, GUN_Z), (0.25, 0.5, 0.38), r=0.07, k=3)
    vp.bellows(p, 'Cloth', MANTLET_X - 0.04, MANTLET_X + 0.2, 0.19, 0.15, GUN_Z, folds=4, depth=0.02, seg=28)
    # o cinto do Afganit: os 5 tubos de lançar inclinados em cada lado do pé da torre e os 4 painéis do radar nos cantos
    for s in (-1, 1):
        for k in range(5):
            x = TX + 0.9 - k * 0.42
            vp.lathe(p, 'Metal', (x, 1.36 * s, T_Z0 + 0.12), (0, s * 0.9, 0.45), [(0.0, 0.0), (0.07, 0.0), (0.07, 0.24), (0.06, 0.26),
                                                                                 (0.0, 0.26)], seg=12)
        for (x, yaw) in ((TX + 1.2, 60 * s), (TX - 1.7, 120 * s)):
            vp.rbox(p, 'Paint#plate', (x, 1.22 * s, T_Z1 + 0.04), (0.06, 0.42, 0.34), yaw=yaw, r=0.03, k=2)
            vp.rbox(p, 'Dark', (x + 0.03 * math.cos(math.radians(yaw)), 1.22 * s + 0.03 * math.sin(math.radians(yaw)), T_Z1 + 0.04),
                    (0.012, 0.34, 0.26), yaw=yaw, r=0.02, k=2)
        rk.star_outlined(p, (TX - 0.7, 1.325 * s, 1.98), (0, s, 0), r=0.15)
    # ---------------------------------------------------------------- o teto: as miras, a estação remota, as antenas
    top = T_Z2
    gx, gy = TX + 1.05, -0.6
    vp.rbox(p, 'Paint#plate', (gx, gy, top + 0.17), (0.45, 0.4, 0.34), r=0.06, k=3)
    vp.rbox(p, 'Dark', (gx + 0.225, gy, top + 0.2), (0.02, 0.32, 0.18), r=0.03, k=2)
    vp.rbox(p, 'Lens', (gx + 0.235, gy - 0.06, top + 0.2), (0.012, 0.14, 0.12), r=0.03, k=2)
    vp.rbox(p, 'Lens', (gx + 0.235, gy + 0.1, top + 0.2), (0.012, 0.09, 0.09), r=0.03, k=2)
    cx, cy = TX + 0.75, 0.62
    vp.lathe(p, 'Paint', (cx, cy, top), 'z', [(0.0, 0.0), (0.22, 0.0), (0.22, 0.05), (0.13, 0.08), (0.11, 0.42), (0.0, 0.42)], seg=18)
    vp.rbox(p, 'Paint#plate', (cx, cy, top + 0.54), (0.42, 0.36, 0.26), r=0.06, k=3)
    vp.rbox(p, 'Lens', (cx + 0.21, cy, top + 0.55), (0.012, 0.24, 0.13), r=0.03, k=2)
    rk.kord_rws(p, (TX - 0.5, 0.3, top), yaw=0.0, k=0.85)
    for s in (-1, 1):
        vp.grab_handle(p, 'Metal', (TX - 1.1, 1.25 * s, top - 0.05), (TX - 0.6, 1.25 * s, top - 0.05), (0, s, 0), height=0.06, r=0.014)
    for (ax, ay, hh) in ((TX - 1.6, 0.85, 1.7), (TX - 1.65, -0.85, 1.4)):
        vp.lathe(p, 'Dark', (ax, ay, top), 'z', [(0.0, 0.0), (0.06, 0.0), (0.06, 0.05), (0.04, 0.07), (0.035, 0.16), (0.0, 0.17)], seg=12)
        p.cyl('Dark#vp', (ax, ay, top + 0.2 + hh / 2), 'z', 0.009, hh, seg=6, bevel=0.0)
    vp.panel_seam(p, 'Dark', (TX + 0.2, -1.15, top + 0.003), (TX + 0.2, 1.15, top + 0.003), (0, 0, 1), w=0.024, d=0.012)
    vp.panel_seam(p, 'Dark', (TX - 1.0, -1.18, top + 0.003), (TX - 1.0, 1.18, top + 0.003), (0, 0, 1), w=0.024, d=0.012)
    # ---------------------------------------------------------------- a gaiola LEVE (ripas a cada 36 cm, só a metade de trás: as miras
    # da frente ficam de fora), a lona dobrada e a rede pequena
    c = CAGE
    posts = [(TX + 0.4, 1.0, top), (TX + 0.4, -1.0, top), (TX - 1.65, 1.0, top), (TX - 1.65, -1.0, top)]
    roof = rk.slat_cage(p, c['x0'], c['x1'], c['hw'], c['z'], c['z'], posts, visor=0.0, step=0.36)
    step = (c['x1'] - c['x0']) / max(2, int(round((c['x1'] - c['x0']) / 0.36)))
    vp.tarp_bundle(p, (TX - 1.55, 0.55, roof(TX - 1.55) + 0.03), (0.9, 0.6, 0.16), m='Team', strap_m='Cloth', buckle_m='Metal', straps=2,
                   layers=2, seed=3.0, swap=True)

    def zf(x, y):
        z = roof(x) + 0.035 + 0.06 * math.exp(-((x - TX + 0.6) ** 2 + (y + 0.5) ** 2) / 0.1)
        z -= 0.02 * math.sin(math.pi * (((x - c['x0']) / step) % 1.0))
        if y < -c['hw']:
            z -= min(0.25, (-c['hw'] - y) * 1.4)
        return z
    rk.net_drape(p, TX - 1.2, TX + 0.2, -c['hw'] - 0.2, 0.1, zf, step=0.22, r=0.017, jitter=0.06, seed=21.0)
    return p.finish()


def barrel():
    p = rk.new_part(PREFIX + '_Barrel')
    x0 = MANTLET_X
    st = [(x0 - 0.06, 0.165), (x0 + 0.1, 0.165), (x0 + 0.14, 0.135), (x0 + 1.3, 0.128), (x0 + 1.34, 0.122),
          (3.55, 0.12), (3.6, 0.148), (3.68, 0.16), (4.2, 0.16), (4.28, 0.148), (4.33, 0.118),
          (6.15, 0.11), (6.19, 0.114), (MUZZLE_X - 0.04, 0.114), (MUZZLE_X, 0.106)]
    vp.gun_barrel(p, st, GUN_Z, m='Paint', seg=32, bore=0.0625, bore_depth=0.4, bore_m='Dark')
    for xx, r in ((x0 + 0.7, 0.13), (3.2, 0.121), (4.9, 0.115), (5.6, 0.112)):
        vp.ring_band(p, 'Metal', xx - 0.03, xx + 0.03, r, GUN_Z, lip=0.009, seg=32)
        vp.rbox(p, 'Metal', (xx, 0.0, GUN_Z - r - 0.013), (0.05, 0.05, 0.03), r=0.012, k=1)
    for xx in (3.66, 4.22):
        vp.bolt_ring(p, 'Metal', (xx, 0.0, GUN_Z), 'x', 0.15, 10, r=0.014, h=0.012)
    vp.rbox(p, 'Metal', (MUZZLE_X - 0.24, 0.0, GUN_Z + 0.15), (0.13, 0.08, 0.06), r=0.015, k=2)
    p.box('Metal', MUZZLE_X - 0.27, MUZZLE_X - 0.21, -0.02, 0.02, GUN_Z + 0.1, GUN_Z + 0.13, bevel=0.0)
    return p.finish()


def wheels():
    p = rk.new_part(PREFIX + '_Wheels')
    for s in (-1, 1):
        y = C.TRACK_Y * s
        for x in C.WHEELS_X:
            rk.ribbed_wheel(p, (x, y, C.WHEEL_Z), C.WHEEL_R, 0.6, s, 'Paint', ribs=6)
        ix, iz, ir = C.IDLER
        vp.idler(p, (ix, y, iz), ir, 0.54, s, 'Paint', 'Metal', gap=0.1)
        sx, sz, sr = C.SPROCKET
        vp.sprocket(p, (sx, y, sz), sr, 0.56, s, 'Paint', 'Metal', teeth=12, holes_m='Dark')
        for x in C.ROLLERS_X:
            vp.return_roller(p, (x, y, C.ROLLER_Z), C.ROLLER_R, 0.3, 'Metal', 'Rubber')
    return p.finish()


def main():
    clear_scene()
    h, t, b, w = hull(), turret(), barrel(), wheels()
    tl, tr = rk.t72_track(PREFIX + '_Track_l', 'l', C=T14, pitch=0.16), rk.t72_track(PREFIX + '_Track_r', 'r', C=T14, pitch=0.16)
    sweep_check([h], [t, b], (TX, 0.0))
    total = 0
    for o in (h, t, b, w, tl, tr):
        total += export(o, OUT)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL tris por peca', {o.name: tris(o) for o in (h, t, b, w, tl, tr)})
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h, t, b, w, tl, tr])
    (hx0, hx1), (hy0, hy1), _ = bounds([h, w, tl, tr])
    print('MODEL medidas: comprimento %.2f m com o cano (x %.2f..%.2f), casco %.2f x %.2f m, altura %.2f m, deck %.2f, teto da torre %.2f'
          % (ax1 - ax0, ax0, ax1, hx1 - hx0, hy1 - hy0, az1, C.DECK, T_Z2))
    print('MODEL turret_pivot_m', [TX, 0.0], 'muzzle_height_m', GUN_Z, 'a boca em x', MUZZLE_X, 'tracks_m', [round(2 * C.TRACK_Y, 3), C.TRACK_W])
    rk.write_spec(OUT, [(PREFIX + n, (0, 0, 0)) for n in ('_Hull', '_Turret', '_Barrel', '_Wheels', '_Track_l', '_Track_r')])


main()
