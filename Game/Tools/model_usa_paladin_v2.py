"""O Paladin da USA (usa_paladin), o tanque pesado, VERSÃO 2 (01/11/2026): o desenho de docs/elenco/SILHUETAS_USA.md §3, no detalhe
AAA da família do tanque B (Game/Tools/model_usa_mbt.py, aprovado). A v1 (Game/Tools/model_usa_paladin.py) fica como está.

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV, as máscaras, as rodas em wheel_* e a esteira que anda):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_usa_paladin_v2.py C:/Users/luisf/explosion/captures/game/models/usa_paladin_v2

O pedido (01/11): "o Paladin e o Abrams não têm diferença nenhuma visualmente, eu bato o olho e não consigo distinguir, isso é PÉSSIMO
pra um RTS AAA onde cada unidade é ICÔNICA e AUTÊNTICA". A regra (SILHUETAS_USA.md §3): contra o Abrams, cinco diferenças de cima.
  1. o TAMANHO: o maior de lagarta, 9,34 × 4,40 m (o Abrams B: 7,9 × 3,8);
  2. o CONTORNO do casco em hexágono esticado: o nariz em V de duas chapas em bico (o "nariz de lúcio" do IS-3, as chapas em pé no
     plano do V) que passa da lagarta, e os lados em 3 DEGRAUS de cima: o deck do miolo (1,70), o para-lama/sponson (1,50) e a faixa
     de blindagem reativa por cima das saias (1,30). A traseira com os cantos cortados;
  3. a TORRE em ponta de flecha comprida e baixa (4,85 m × 3,24 m, o teto a 2,38 contra 2,48 do Abrams), recuada (o eixo em −0,50,
     o Abrams em +0,10), com as "farpas" da flecha atrás das bochechas e o rabo estreito; o canhão de 5,2 m, grosso (a luva de 0,40 m),
     com o evacuador e o FREIO DE BOCA de 2 câmaras;
  4. a ASSINATURA: o LASER de defesa de ponto, a cúpula branca de 1,30 m num pedestal alto no rabo da torre, com o olho da lente
     grande acesa (Light) apontado pra cima e pra frente; gira sozinho (a peça _Laser, origem no eixo dela);
  5. as LATERAIS: os blocos de blindagem reativa (ERA) em 2 fileiras desencontradas por cima das saias, sem a faixa azul; a cor do
     jogador numa SETA grande no teto da cunha (Team) e no anel do evacuador; a lona (Cloth) enrolada no para-lama de trás.
  E 8 rodas de 0,70 m (o Abrams: 7 de 0,63).
A mesma família do tanque B: a tinta, as chapas grossas com a fresta e os parafusos, os faróis na guarda em U, as grelhas de aletas,
a cúpula octogonal com os periscópios, as rodas duplas com o disco rebaixado, a esteira de elos que anda, a lona com as cintas.
Referências reais: o IS-3/IS-7 (o nariz de lúcio), o M1 TUSK e o Leopard 2A7 (as fileiras de ERA por cima das saias, a cunha da
torre), os diretores de feixe dos lasers de veículo (o DE M-SHORAD, o HEL-MD: a torrinha própria com a janela grande).

Peças (os NOMES da v1, pro Units.json só trocar a pasta; origem no chão, no centro do casco, frente em +X):
  SM_USA_Paladin_Hull     o casco, os para-lamas, as saias, a ERA, o deck, os faróis, a lona, o anel da torre;
  SM_USA_Paladin_Turret   a torre (gira em Z em volta de (TX, 0)), o mantelete, a mira, a cúpula, a escotilha, os lança-fumaça, o
                          pedestal do laser, as antenas, a SETA;
  SM_USA_Paladin_Barrel   o cano (recua em −X a partir daqui; a boca é o X máximo);
  SM_USA_Paladin_Laser    a cúpula do laser com a ORIGEM NO EIXO (gira em Z; fica em LASER_AT, em cima do pedestal da torre);
  SM_USA_Paladin_Wheels   a rodagem (o executor separa em wheel_001..);
  SM_USA_Paladin_Track_l / _Track_r  as esteiras de elos (a tangente de cada elo no UV2/UV3: a esteira que anda).
Slots: Paint, Team, Cloth, Dark, Metal, Lens, Light, Track (a borracha das rodas no Dark, como o tanque B).
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
from model_china_vehicle_kit import bounds, frame_box, solid, tris  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_paladin_v2')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_USA_Paladin'

COLORS.update({'Paint': (196, 198, 192), 'Team': (86, 108, 150), 'Dark': (40, 40, 38), 'Metal': (95, 95, 92),
               'Cloth': (64, 86, 132), 'Lens': (40, 90, 160), 'Track': (55, 52, 48), 'Light': (255, 230, 170)})

# ---------------------------------------------------------------------------------- medidas (m)
NOSE, REAR = 4.72, -4.62         # o bico do V e a traseira: 9,34 m
V_K = (NOSE - 3.42) / 2.20       # o V de cima: x = NOSE − V_K·|y| (os ombros em (3,42; ±2,20))
BELLY = 0.48
SPON_Z = 1.14                    # a barriga do para-lama (por cima da esteira)
FENDER_Z = 1.50                  # o 2º degrau: o para-lama (sponson)
DECK = 1.70                      # o miolo
ERA_TOP = 1.30                   # o 3º degrau: a faixa de ERA por cima das saias
LOW_HW, CORE_HW, UP_HW = 1.12, 1.30, 1.94
SKIRT = (1.94, 2.02)
ERA_Y = (2.02, 2.20)             # 4,40 m de largura
SKIRT_Z = (0.66, ERA_TOP)
ERA_ROWS = [(0.74, 1.005), (1.035, ERA_TOP)]
TRACK_Y, TRACK_W = 1.56, 0.72
WHEEL_R, WHEEL_Z = 0.35, 0.435   # 8 rodas de 0,70 m
WHEELS_X = [round(2.56 - 0.84 * i, 3) for i in range(8)]
IDLER = (3.22, 0.70, 0.29)
SPROCKET = (-3.98, 0.71, 0.30)
ROLLERS_X, ROLLER_Z, ROLLER_R = [1.30, -0.38, -2.06], 0.93, 0.09
TX = -0.50                       # o eixo da torre (turret_pivot_m)
RING_R = 1.05
T_Z0, T_Z1, T_Z2 = 1.74, 1.98, 2.38
GUN_Z = 2.06                     # muzzle_height_m
MANTLET_X = TX + 2.70
MUZZLE_X = 7.35
LASER_U = -1.62                  # o laser no rabo da torre (u = x − TX)
PED_H = 0.48                     # o pedestal alto
LASER_AT = (TX + LASER_U, 0.0, T_Z2 + PED_H)
LASER_TILT = 28.0                # o olho do laser, pra cima
GLACIS_X0, NOSE_TOP = 2.55, 1.28
REAR_DECK_X = -4.42
PROW_Z = 0.96                    # o pé das chapas do bico

# a planta da torre (meia planta, u = x − TX, y ≥ 0): a ponta do mantelete, a bochecha comprida até a FARPA, o entalhe pra dentro, o
# rabo estreito, o canto de trás. Z0 = o pé, Z1 = a cinta (a parte mais larga), Z2 = o teto (que desce na ponta)
PLAN0 = [(2.30, 0.0), (2.30, 0.26), (-0.50, 1.40), (-0.72, 1.08), (-1.90, 1.04), (-2.10, 0.80), (-2.14, 0.0)]
PLAN1 = [(2.58, 0.0), (2.58, 0.30), (-0.55, 1.62), (-0.80, 1.20), (-1.95, 1.16), (-2.22, 0.88), (-2.27, 0.0)]
PLAN2 = [(2.40, 0.0), (2.40, 0.26), (-0.52, 1.46), (-0.76, 1.12), (-1.92, 1.08), (-2.14, 0.82), (-2.18, 0.0)]


def v_x(y):
    """O V do nariz em planta."""
    return NOSE - V_K * abs(y)


def glacis_z(x):
    """O glacis (um plano só, de borda em V): do bico (NOSE; NOSE_TOP) ao deck (GLACIS_X0; DECK)."""
    return NOSE_TOP + (DECK - NOSE_TOP) * (NOSE - x) / (NOSE - GLACIS_X0)


def deck_z(x):
    return DECK if x <= GLACIS_X0 else glacis_z(x)


def roof_z(u):
    """O teto da torre: plano, descendo 14 cm por metro na ponta (a flecha baixa)."""
    return T_Z2 - max(0.0, u - 1.6) * 0.14


V_N = Vector((1.0, V_K, 0.0)).normalized()          # a normal da chapa do V do lado direito (o esquerdo: y trocado)


def lerp(a, b, t):
    return Vector(a) + (Vector(b) - Vector(a)) * t


def plate_xz(p, m, x0, x1, z0, z1, y, t, r=0.05, side=1):
    """A chapa em pé no plano XZ com os cantos redondos, de y pra fora (side) com a espessura t."""
    vp.rbox_q(p, m, ((x0 + x1) / 2, y + side * t / 2, (z0 + z1) / 2), (x1 - x0, z1 - z0, t), roll=90, r=r, k=3)


def grille_slats(p, c, size, n, frame='Paint#plate', slat='Paint#thin', axis_n=(0, 0, 1), along=(1, 0, 0), tilt=35.0):
    """A grelha do tanque B: o fundo escuro, a moldura de 4 barras e as aletas inclinadas."""
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
        q = C + ev * (-W / 2 + step * (k + 0.5)) + en * 0.022
        frame_box(p, slat, q, eu, e2, e3, L, step * 0.8, 0.012)


def headlight(p, c, n, r=0.075):
    """O farol redondo do tanque B (o copo com o aro e o vidro abaulado), virado pra n."""
    vp.lathe(p, 'Metal', c, n, [(0.0, -0.06), (r * 0.7, -0.06), (r + 0.018, -0.04), (r + 0.022, 0.02), (r + 0.022, 0.045),
                                (r + 0.012, 0.055), (r, 0.05), (r, 0.035), (0.0, 0.035)], seg=16)
    vp.lathe(p, 'Lens', c, n, [(0.0, 0.056), (r * 0.6, 0.054), (r * 0.95, 0.046), (r, 0.04), (0.0, 0.04)], seg=16)


def light_guard(p, c, n, w=0.34, h=0.32, depth=0.15):
    """A guarda em U dos faróis (tubo dobrado), na face de normal n: 2 pernas e o arco de cima."""
    n = Vector(n).normalized()
    t = Vector((0, 0, 1)).cross(n).normalized()
    C = Vector(c)
    pts = [C - t * (w / 2) - n * 0.02, C - t * (w / 2) + n * depth, C - t * (w / 2) + n * depth + Vector((0, 0, h)),
           C + t * (w / 2) + n * depth + Vector((0, 0, h)), C + t * (w / 2) + n * depth, C + t * (w / 2) - n * 0.02]
    vp.tube(p, 'Dark', vp.bent_bar(pts, R=0.09, n=4), 0.022, seg=8)


def flat_patch(p, m, pts, z, t=0.008):
    """A pintura em relevo fino (a SETA, os retalhos) num plano horizontal z: o polígono convexo pts [(x, y)] com espessura t."""
    vp.prism(p, m, pts, Vector((0.0, 0.0, z)), Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1)), t)


# ---------------------------------------------------------------------------------- casco
def hull():
    p = Part(PREFIX + '_Hull')
    p.bm('Paint#shell', 0.07)
    p.bm('Paint#plate', 0.03)
    p.bm('Paint#thin', 0.008)
    p.bm('Lens', 0.0)
    # ---------------------------------------------------------------- o miolo de baixo (entre as lagartas): o bico em V em pé e a
    # chapa de baixo que volta pra barriga (um paralelogramo: o pé do V e a frente da barriga paralelos)
    low = [(NOSE, 0.0, PROW_Z), (NOSE, 0.0, SPON_Z + 0.02), (v_x(0) - 0.97, 0.0, BELLY)]
    for s in (-1, 1):
        y = s * LOW_HW
        low += [(v_x(LOW_HW), y, PROW_Z), (v_x(LOW_HW), y, SPON_Z + 0.02), (v_x(LOW_HW) - 0.97, y, BELLY),
                (REAR + 0.03, y, SPON_Z + 0.02), (REAR + 0.03, y, 0.66), (-4.42, y, BELLY)]
    solid(p, 'Paint#shell', low)
    # ---------------------------------------------------------------- o miolo de cima: as 2 CHAPAS DO BICO em pé no plano do V, o
    # glacis (um plano de borda em V) subindo até o deck, o deck e a quina de trás
    up = [(NOSE, 0.0, SPON_Z), (NOSE, 0.0, NOSE_TOP)]
    for s in (-1, 1):
        y = s * CORE_HW
        x = v_x(CORE_HW)
        up += [(x, y, SPON_Z), (x, y, glacis_z(x)), (GLACIS_X0, y, DECK), (REAR_DECK_X, y, DECK), (REAR, y, 1.58), (REAR, y, SPON_Z)]
    solid(p, 'Paint#shell', up)
    # ---------------------------------------------------------------- os para-lamas (sponsons), o 2º degrau: a frente no mesmo V, o
    # topo descendo com o glacis na frente, os cantos de trás cortados
    xf = NOSE - (FENDER_Z - NOSE_TOP) * (NOSE - GLACIS_X0) / (DECK - NOSE_TOP)       # onde o glacis passa do topo do para-lama
    for s in (-1, 1):
        f = []
        for y in (CORE_HW, UP_HW):
            x = v_x(y)
            f += [(x, s * y, SPON_Z), (x, s * y, min(FENDER_Z, glacis_z(x)))]
            if xf < x:
                f.append((xf, s * y, FENDER_Z))
        for (x, y) in ((-4.36, UP_HW), (REAR, 1.62), (REAR, CORE_HW)):
            f += [(x, s * y, SPON_Z), (x, s * y, FENDER_Z)]
        solid(p, 'Paint#shell', f)
    # ---------------------------------------------------------------- as saias (as chapas grossas por baixo da ERA) e a ERA
    seams = [2.70, 1.62, 0.54, -0.54, -1.62, -2.70, -3.70, -4.30]
    for s in (-1, 1):
        y_in, t = SKIRT[0], SKIRT[1] - SKIRT[0]
        # a da frente: a ponta no plano do V e o canto de baixo cortado em diagonal (mostra a roda-guia)
        x0 = seams[0] + 0.016
        prof = [(x0, SKIRT_Z[1]), (None, SKIRT_Z[1]), (None, 1.10), (3.0, SKIRT_Z[0] + 0.02), (x0, SKIRT_Z[0])]
        pts = []
        for yy in (y_in, y_in + t):
            for (x, z) in prof:
                pts.append((v_x(yy) - 0.01 if x is None else x, s * yy, z))
        solid(p, 'Paint#plate', pts)
        for i in range(len(seams) - 1):
            xa, xb = seams[i + 1] + 0.016, seams[i] - 0.016
            plate_xz(p, 'Paint#plate', xa, xb, SKIRT_Z[0], SKIRT_Z[1], y_in if s > 0 else -SKIRT[1], t, r=0.06)
        # a ERA: 2 fileiras desencontradas de blocos (0,56 m, a fresta de 4 cm), com 2 parafusos cada; o bloco da frente da fileira de
        # cima é a cunha que acaba no V; a de baixo para antes do corte da saia
        ye0, ye1 = ERA_Y
        for row, (z0, z1) in enumerate(ERA_ROWS):
            x = -4.12 + (0.30 if row == 0 else 0.0)
            xmax = 2.98 if row == 0 else v_x(ye1) - 0.02
            while x + 0.3 < xmax:
                x1 = min(x + 0.56, xmax)
                if row == 1 and x + 0.56 >= xmax:
                    # a cunha da frente (a face da frente no plano do V)
                    w = []
                    for yy in (ye0, ye1):
                        for zz in (z0, z1):
                            w += [(x, s * yy, zz), (v_x(yy) - 0.02, s * yy, zz)]
                    solid(p, 'Paint#plate', w)
                    dxs = (0.12,)
                else:
                    plate_xz(p, 'Paint#plate', x, x1, z0, z1, ye0 if s > 0 else -ye1, ye1 - ye0, r=0.04)
                    dxs = ((x1 - x) / 2 - 0.12, (x1 - x) / 2 + 0.12)
                xc = x
                for dx in dxs:
                    vp.nut(p, 'Metal', (xc + dx, s * (ye1 + 0.002), (z0 + z1) / 2), (0, s, 0), r=0.026, h=0.018)
                x += 0.60
        # a tira de cima da ERA (o trilho onde os blocos penduram) e os parafusos do para-lama em cima dela
        vp.rbox(p, 'Paint#thin', ((-4.16 + v_x(ye1)) / 2 - 0.05, s * (ye0 + ye1) / 2, ERA_TOP + 0.012), (v_x(ye1) + 4.16 - 0.12, ye1 - ye0 + 0.02,
                0.024), r=0.01, k=1)
        vp.bolt_row(p, 'Metal', (3.30, s * (UP_HW + 0.002), 1.40), (-4.20, s * (UP_HW + 0.002), 1.40), 30, (0, s, 0), r=0.02, h=0.014)
    # ---------------------------------------------------------------- o bico: os ganchos, os faróis nos para-lamas (sem o cordão de
    # solda nas quinas do bico: o chanfro arredonda a quina pra trás e o cordão ficava solto na frente, uma vara no ar)
    for s in (-1, 1):
        n = Vector((V_N.x, s * V_N.y, 0.0))
        # os ganchos de reboque nas chapas de baixo do bico (as orelhas com o pino e a manilha)
        y = 0.62 * s
        c = Vector((v_x(0.62), y, 1.05))
        for d in (-1, 1):
            q = c + n * 0.06 + Vector((0, 0, d * 0.055))
            vp.rbox(p, 'Metal', q, (0.14, 0.12, 0.035), yaw=math.degrees(math.atan2(n.y, n.x)), r=0.03, k=2)
        p.cyl('Metal#vp', tuple(c + n * 0.09), 'z', 0.022, 0.16, seg=10, bevel=0.0)
        p.torus('Dark#vp', tuple(c + n * 0.13 - Vector((0, 0, 0.05))), 'y', 0.05, 0.016, seg=14, rseg=6)
        # os faróis com a guarda em U na frente do para-lama (no V)
        y = 1.62 * s
        c = Vector((v_x(1.62), y, 1.27))
        vp.rbox(p, 'Paint#plate', c + n * 0.01, (0.08, 0.26, 0.2), yaw=math.degrees(math.atan2(n.y, n.x)), r=0.05, k=4)
        headlight(p, c + n * 0.04 + Vector((0, 0, 0.02)), n, r=0.07)
        light_guard(p, c + Vector((0, 0, -0.11)), n, w=0.34, h=0.29, depth=0.14)
        # a junta da chapa do glacis paralela ao V e os parafusos dela
        a, b = Vector((4.18, 0.3 * s, 0.0)), Vector((3.50, 1.25 * s, 0.0))
        a.z, b.z = glacis_z(a.x) + 0.004, glacis_z(b.x) + 0.004
        gn = Vector((DECK - NOSE_TOP, 0.0, NOSE - GLACIS_X0)).normalized()
        vp.panel_seam(p, 'Dark', a, b, gn, w=0.025, d=0.012)
        for k in range(6):
            q = lerp(a, b, (k + 0.5) / 6) + Vector((-0.07, 0.0, 0.0))
            q.z = glacis_z(q.x)
            vp.nut(p, 'Metal', q, gn, r=0.02, h=0.014)
    # ---------------------------------------------------------------- o glacis: a escotilha do motorista e os periscópios
    ga = math.atan2(DECK - NOSE_TOP, NOSE - GLACIS_X0)
    eu = Vector((math.cos(ga), 0.0, -math.sin(ga)))
    en = Vector((math.sin(ga), 0.0, math.cos(ga)))
    vp.prism(p, 'Paint#plate', vp.rounded_rect(0.66, 0.82, 0.12, 3), Vector((3.12, 0.0, glacis_z(3.12) - 0.005)), eu, Vector((0, 1, 0)),
             en, 0.045)
    vp.hinge(p, 'Metal', (2.84, 0.0, glacis_z(2.84) + 0.035), 'y', 0.52, r=0.026, knuckles=3)
    vp.grab_handle(p, 'Metal', (3.28, -0.15, glacis_z(3.28) + 0.045), (3.28, 0.15, glacis_z(3.28) + 0.045), en, height=0.05, r=0.012)
    for k, yaw in enumerate((-25, 0, 25)):
        y = (k - 1) * 0.26
        x = 2.46 - 0.03 * abs(k - 1)
        vp.rbox(p, 'Paint#plate', (x, y, DECK + 0.05), (0.2, 0.22, 0.1), yaw=yaw, r=0.04, k=4)
        a = math.radians(yaw)
        p.obox('Lens', (x + 0.1 * math.cos(a), y + 0.1 * math.sin(a), DECK + 0.06), (0.02, 0.15, 0.05), yaw=yaw, bevel=0.0)
    # ---------------------------------------------------------------- os para-lamas: as juntas, os parafusos, as argolas, os elos de
    # reserva na frente, a lona enrolada (Cloth) atrás à esquerda, a caixa atrás à direita, as grelhas de ar
    for s in (-1, 1):
        vp.bolt_row(p, 'Metal', (3.40, s * (UP_HW - 0.06), FENDER_Z), (-4.25, s * (UP_HW - 0.06), FENDER_Z), 34, (0, 0, 1), r=0.018,
                    h=0.012)
        for x in (2.15, 0.55, -1.05, -2.65):
            vp.panel_seam(p, 'Dark', (x, s * (CORE_HW + 0.03), FENDER_Z + 0.003), (x, s * (UP_HW - 0.03), FENDER_Z + 0.003), (0, 0, 1),
                          w=0.024, d=0.012)
        vp.panel_seam(p, 'Dark', (3.45, s * (CORE_HW + 0.02), FENDER_Z + 0.003), (-4.30, s * (CORE_HW + 0.02), FENDER_Z + 0.003),
                      (0, 0, 1), w=0.024, d=0.012)
        for x in (3.25, 0.0, -4.15):
            p.torus('Metal#vp', (x, s * 1.80, FENDER_Z + 0.035), 'x', 0.05, 0.014, seg=12, rseg=6)
        vp.spare_links(p, (2.98, s * 1.62, FENDER_Z), (0, 0, 1), (1, 0, 0), 4, pitch=0.19, width=0.5, m='Track', guide_m='Metal',
                       t=0.05)
        grille_slats(p, (-2.05, s * 1.62, FENDER_Z), (0.5, 0.4), 4, along=(0, 1, 0), tilt=30)
    vp.tarp_roll(p, (-3.35, -1.60, FENDER_Z + 0.115), 'x', 1.2, 0.11, m='Cloth', straps=3, strap_m='Dark', buckle_m='Metal')
    for x in (-2.88, -3.82):                                     # os berços da lona
        vp.rbox(p, 'Dark', (x, -1.60, FENDER_Z + 0.03), (0.06, 0.3, 0.06), r=0.015, k=1)
    vp.rbox_q(p, 'Paint#plate', (-3.40, 1.60, FENDER_Z + 0.1), (1.1, 0.5, 0.2), r=0.06, k=3)   # a caixa de ferramentas
    vp.rbox(p, 'Paint#thin', (-3.40, 1.60, FENDER_Z + 0.205), (1.04, 0.44, 0.012), r=0.05, k=2)
    for x in (-3.0, -3.8):
        vp.rbox(p, 'Metal', (x, 1.60 + 0.255, FENDER_Z + 0.15), (0.08, 0.02, 0.06), r=0.008, k=1)        # os trincos
    # ---------------------------------------------------------------- o deck: as juntas, o anel da torre, o motor
    for s in (-1, 1):
        vp.panel_seam(p, 'Dark', (GLACIS_X0 - 0.05, s * 1.12, DECK + 0.003), (-2.6, s * 1.12, DECK + 0.003), (0, 0, 1), w=0.024, d=0.012)
        vp.bolt_row(p, 'Metal', (GLACIS_X0 - 0.08, s * 1.24, DECK), (-4.3, s * 1.24, DECK), 32, (0, 0, 1), r=0.018, h=0.012)
    vp.lathe(p, 'Dark', (TX, 0.0, DECK), 'z', [(RING_R - 0.06, 0.0), (RING_R + 0.07, 0.0), (RING_R + 0.07, 0.012),
                                             (RING_R + 0.05, 0.022), (RING_R - 0.06, 0.022)], seg=64, closed=True)
    for s in (-1, 1):
        grille_slats(p, (-3.62, s * 0.6, DECK), (1.1, 0.9), 8, along=(0, 1, 0), tilt=30)
        # as tampas de enchimento (fora do giro da ponta da torre: o pé dela passa a 4 cm do deck)
        vp.lathe(p, 'Paint', (-2.92, 0.78 * s, DECK), 'z', [(0.0, 0.0), (0.13, 0.0), (0.13, 0.04), (0.11, 0.06), (0.0, 0.065)], seg=18)
        p.cyl('Metal#vp', (-2.92, 0.78 * s, DECK + 0.07), 'z', 0.04, 0.02, seg=10, bevel=0.0)
    vp.panel_seam(p, 'Dark', (-2.68, -1.15, DECK + 0.003), (-2.68, 1.15, DECK + 0.003), (0, 0, 1), w=0.025, d=0.01)
    # ---------------------------------------------------------------- a traseira: as lanternas, as aletas do escape, o cabo
    for s in (-1, 1):
        vp.lathe(p, 'Paint', (REAR - 0.005, 1.1 * s, 1.40), (-1, 0, 0), [(0.0, 0.0), (0.13, 0.0), (0.13, 0.05), (0.115, 0.065),
                                                                      (0.1, 0.065), (0.1, 0.055), (0.0, 0.055)], seg=20)
        vp.lathe(p, 'Lens', (REAR - 0.005, 1.1 * s, 1.40), (-1, 0, 0), [(0.0, 0.07), (0.07, 0.066), (0.095, 0.058), (0.0, 0.058)], seg=16)
        for d in (-1, 1):
            vp.rbox(p, 'Metal', (REAR - 0.05, 0.62 * s + d * 0.055, 0.86), (0.12, 0.035, 0.15), r=0.03, k=2)
        p.cyl('Metal#vp', (REAR - 0.07, 0.62 * s, 0.88), 'y', 0.022, 0.16, seg=10, bevel=0.0)
        p.torus('Dark#vp', (REAR - 0.1, 0.62 * s, 0.82), 'y', 0.055, 0.017, seg=14, rseg=6)
    for k in range(6):
        vp.rbox(p, 'Dark', (REAR - 0.02, 0.0, 1.20 + k * 0.05), (0.03, 1.6, 0.028), pitch=-25, r=0.01, k=1)
    vp.tow_cable(p, 'Metal', [(REAR - 0.04, -0.9, 1.0), (REAR - 0.06, -0.7, 0.95), (REAR - 0.06, 0.7, 0.95), (REAR - 0.04, 0.9, 1.0)],
                 r=0.022)
    return p.finish()


# ---------------------------------------------------------------------------------- torre
def plan_ring(half, z_fn):
    """O anel completo da planta (frente → direita → trás → esquerda), absoluto (x = TX + u), z por ponto."""
    full = half + [(u, -y) for (u, y) in reversed(half[1:-1])]
    return [(TX + u, y, z_fn(u)) for (u, y) in full]


def inset_ring(rg, k, z_fn=None, c=(0.25, 0.0)):
    out = []
    for (x, y, z) in rg:
        u = (x - TX)
        nu = c[0] + (u - c[0]) * k
        out.append((TX + nu, c[1] + (y - c[1]) * k, z_fn(nu) if z_fn else z))
    return out


def turret():
    p = Part(PREFIX + '_Turret')
    p.bm('Paint#shell', 0.07)
    p.bm('Paint#plate', 0.03)
    p.bm('Paint#thin', 0.008)
    p.bm('Lens', 0.0)
    r0 = plan_ring(PLAN0, lambda u: T_Z0)
    r1 = plan_ring(PLAN1, lambda u: T_Z1)
    r2 = plan_ring(PLAN2, roof_z)
    rings = [inset_ring(r0, 0.12), inset_ring(r0, 0.45), inset_ring(r0, 0.78), r0, r1, r2,
             inset_ring(r2, 0.8, roof_z), inset_ring(r2, 0.55, roof_z), inset_ring(r2, 0.3, roof_z), inset_ring(r2, 0.08, roof_z)]
    p.loft('Paint#shell', rings)
    vp.lathe(p, 'Paint', (TX, 0.0, DECK), 'z', [(0.0, 0.025), (RING_R - 0.04, 0.025), (RING_R - 0.04, 0.07), (RING_R - 0.08, 0.075),
                                              (0.0, 0.075)], seg=48)                       # o pé no anel
    # ---------------------------------------------------------------- a SETA da cor do jogador no teto da cunha: a cabeça e a haste
    z = T_Z2 + 0.003
    flat_patch(p, 'Team', [(TX + 1.30, 0.0), (TX + 0.15, -0.68), (TX + 0.15, 0.68)], z)
    flat_patch(p, 'Team', [(TX + 0.13, -0.22), (TX + 0.13, 0.22), (TX - 0.95, 0.22), (TX - 0.95, -0.22)], z)
    # ---------------------------------------------------------------- o mantelete na ponta da flecha
    vp.rbox_q(p, 'Paint#plate', (TX + 2.56, 0.0, GUN_Z + 0.01), (0.30, 0.66, 0.46), r=0.09, k=3)
    vp.lathe(p, 'Paint', (MANTLET_X - 0.01, 0.0, GUN_Z), 'x', [(0.0, 0.0), (0.26, 0.0), (0.26, 0.035), (0.24, 0.055), (0.215, 0.055),
                                                            (0.215, 0.02), (0.0, 0.02)], seg=32)       # o colar do cano
    vp.lathe(p, 'Dark', (TX + 2.705, 0.2, GUN_Z + 0.14), 'x', [(0.0, 0.0), (0.045, 0.0), (0.045, 0.04), (0.0, 0.04)], seg=12)  # a coaxial
    vp.bolt_row(p, 'Metal', (TX + 2.71, -0.22, 1.89), (TX + 2.71, 0.22, 1.89), 5, (1, 0, 0), r=0.018, h=0.012)
    # ---------------------------------------------------------------- as bochechas: as juntas dos módulos, os parafusos, os
    # lança-fumaça perto das farpas
    for s in (-1, 1):
        # a junta ao longo da bochecha, 20 cm pra dentro da borda do teto (no teto plano, até antes da mira) e os parafusos dela
        (ua, ya), (ub, yb) = PLAN2[1], PLAN2[2]
        y_edge = lambda u: ya + (ua - u) * (yb - ya) / (ua - ub)
        a = Vector((TX + 1.55, s * (y_edge(1.55) - 0.2), T_Z2 + 0.003))
        b = Vector((TX + 0.25, s * (y_edge(0.25) - 0.2), T_Z2 + 0.003))
        vp.panel_seam(p, 'Dark', a, b, (0, 0, 1), w=0.024, d=0.012)
        for k in range(7):
            q = lerp(a, b, (k + 0.5) / 7) + Vector((0.0, -s * 0.07, -0.003))
            vp.nut(p, 'Metal', q, (0, 0, 1), r=0.02, h=0.014)
        # os lança-fumaça: o berço na bochecha (na cinta, perto da farpa) e 4 tubos pra cima, pra fora e pra frente
        (u1, y1), (u2, y2) = PLAN1[1], PLAN1[2]
        t = 0.80
        cu, cy = u1 + (u2 - u1) * t, y1 + (y2 - y1) * t
        n = Vector((y2 - y1, -(u2 - u1), 0.0)).normalized()
        n.y *= s
        base = Vector((TX + cu, s * cy, T_Z1 + 0.06)) - n * 0.03
        yaw = math.degrees(math.atan2(n.y, n.x))
        vp.rbox(p, 'Paint#plate', base + n * 0.06, (0.16, 0.5, 0.2), yaw=yaw, r=0.05, k=3)
        d = (n * 0.75 + Vector((0.45 * 1.0, 0.0, 0.0)) * 0.6 + Vector((0, 0, 0.55))).normalized()
        side = Vector((0, 0, 1)).cross(n).normalized()
        for i in range(4):
            q = base + n * 0.12 + side * ((i - 1.5) * 0.11) + Vector((0, 0, 0.05))
            vp.lathe(p, 'Paint', q, d, [(0.0, 0.0), (0.05, 0.0), (0.05, 0.2), (0.042, 0.21), (0.0, 0.21)], seg=12)
            vp.lathe(p, 'Dark', q + d * 0.212, d, [(0.0, 0.0), (0.036, 0.0), (0.0, 0.004)], seg=10)
        # os olhais de içar
        for (uu, yy) in ((1.45, 0.62), (-1.50, 0.98)):
            p.torus('Metal#vp', (TX + uu, yy * s, roof_z(uu) + 0.035), 'x', 0.05, 0.016, seg=12, rseg=6)
        # as caixas de estiva no rabo (rentes, dentro da farpa: o contorno da flecha fica limpo)
        vp.rbox_q(p, 'Paint#plate', (TX - 1.38, s * 1.20, 2.04), (0.95, 0.16, 0.36), r=0.05, k=3)
        vp.bolt_row(p, 'Metal', (TX - 1.0, s * 1.282, 2.14), (TX - 1.76, s * 1.282, 2.14), 4, (0, s, 0), r=0.016, h=0.01)
    # ---------------------------------------------------------------- o teto: a mira do atirador, a cúpula do comandante, a escotilha
    gx, gy = TX - 0.05, 0.95
    vp.rbox(p, 'Paint#plate', (gx, gy, T_Z2 + 0.15), (0.38, 0.4, 0.3), r=0.06, k=4)
    vp.rbox(p, 'Dark', (gx + 0.19, gy, T_Z2 + 0.17), (0.02, 0.3, 0.17), r=0.02, k=2)
    vp.rbox(p, 'Lens', (gx + 0.2, gy, T_Z2 + 0.17), (0.012, 0.25, 0.12), r=0.015, k=2)
    vp.rbox(p, 'Paint#thin', (gx + 0.16, gy, T_Z2 + 0.27), (0.12, 0.4, 0.02), pitch=-12, r=0.02, k=2)
    kx, ky = TX - 0.40, -0.72
    vp.lathe(p, 'Paint', (kx, ky, T_Z2), 'z', [(0.0, 0.0), (0.46, 0.0), (0.46, 0.15), (0.42, 0.18), (0.0, 0.18)], seg=8,
             phase=math.pi / 8, smooth=False)
    for k in range(8):
        if k == 4:
            continue
        a = math.radians(k * 45)
        p.obox('Lens', (kx + 0.435 * math.cos(a), ky + 0.435 * math.sin(a), T_Z2 + 0.095), (0.03, 0.19, 0.06), yaw=k * 45, bevel=0.0)
    vp.hatch_round(p, (kx, ky, T_Z2 + 0.15), 0.29, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
    vp.hatch_round(p, (TX - 0.62, 0.62, T_Z2), 0.28, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
    # o sensor de vento e as chapas de trás com os parafusos
    p.cyl('Metal#vp', (TX - 1.0, 0.62, T_Z2 + 0.15), 'z', 0.025, 0.3, seg=10, bevel=0.0)
    vp.lathe(p, 'Paint', (TX - 1.0, 0.62, T_Z2 + 0.3), 'z', [(0.0, 0.0), (0.06, 0.0), (0.06, 0.12), (0.04, 0.14), (0.0, 0.14)], seg=14)
    for s in (-1, 1):
        vp.panel_seam(p, 'Dark', (TX - 1.05, s * 0.66, T_Z2 + 0.003), (TX - 2.0, s * 0.66, T_Z2 + 0.003), (0, 0, 1), w=0.024, d=0.012)
    # ---------------------------------------------------------------- o pedestal alto do laser (fixo na torre; a cúpula em cima gira
    # sozinha): o pé com a flange, a coluna com as aletas de refrigeração, o colar da cor do jogador e o prato de cima
    lx, lz0 = TX + LASER_U, T_Z2
    vp.lathe(p, 'Paint', (lx, 0.0, lz0), 'z', [(0.0, -0.01), (0.54, -0.01), (0.54, 0.045), (0.44, 0.06), (0.36, 0.12), (0.34, 0.40),
                                              (0.40, 0.42), (0.40, 0.44), (0.0, 0.44)], seg=40)
    vp.bolt_ring(p, 'Metal', (lx, 0.0, lz0 + 0.05), 'z', 0.49, 16, r=0.018, h=0.012)
    vp.lathe(p, 'Team', (lx, 0.0, lz0), 'z', [(0.36, 0.42), (0.415, 0.42), (0.42, 0.43), (0.42, PED_H - 0.005), (0.36, PED_H - 0.005)],
             seg=40, closed=True)
    vp.lathe(p, 'Dark', (lx, 0.0, lz0), 'z', [(0.0, PED_H - 0.006), (0.40, PED_H - 0.006), (0.40, PED_H), (0.0, PED_H)], seg=40)
    for k in range(14):
        a = math.radians(k * 360 / 14 + 12.9)
        c = Vector((lx + 0.39 * math.cos(a), 0.39 * math.sin(a), lz0 + 0.26))
        vp.rbox(p, 'Paint#thin', c, (0.12, 0.022, 0.26), yaw=math.degrees(a), r=0.008, k=1)
    # ---------------------------------------------------------------- as 2 antenas nos cantos de trás
    for (ax, ay, hh) in ((TX - 1.95, 0.86, 1.6), (TX - 1.95, -0.86, 1.3)):
        vp.lathe(p, 'Dark', (ax, ay, T_Z2), 'z', [(0.0, 0.0), (0.06, 0.0), (0.06, 0.05), (0.04, 0.07), (0.035, 0.16), (0.0, 0.17)], seg=12)
        p.cyl('Metal#vp', (ax, ay, T_Z2 + 0.19), 'z', 0.02, 0.05, seg=8, bevel=0.0)
        p.cyl('Dark#vp', (ax, ay, T_Z2 + 0.2 + hh / 2), 'z', 0.009, hh, seg=6, bevel=0.0)
    return p.finish()


# ---------------------------------------------------------------------------------- o laser (a assinatura; origem no eixo)
def laser():
    """A cúpula do laser de defesa de ponto com a ORIGEM NO EIXO (o pé, em cima do pedestal): o prato giratório com a fresta do
    rolamento, o tambor, a CÚPULA branca de 1,30 m (as juntas dos gomos), o OLHO do feixe (a carcaça saindo da cúpula a 28° pra cima,
    a lente grande ACESA no aro escuro), os 2 rastreadores dos lados, as aletas de refrigeração atrás e o anel da cor do jogador."""
    p = Part(PREFIX + '_Laser')
    p.bm('Paint#shell', 0.05)
    p.bm('Paint#plate', 0.03)
    p.bm('Lens', 0.0)
    vp.lathe(p, 'Dark', (0, 0, 0), 'z', [(0.0, 0.0), (0.44, 0.0), (0.44, 0.03), (0.0, 0.03)], seg=48)
    vp.lathe(p, 'Paint', (0, 0, 0), 'z', [(0.0, 0.03), (0.56, 0.03), (0.58, 0.05), (0.58, 0.10), (0.56, 0.115), (0.0, 0.115)], seg=48)
    vp.bolt_ring(p, 'Metal', (0, 0, 0.115), 'z', 0.5, 16, r=0.018, h=0.012)
    # o tambor e a cúpula (meio elipsoide de 0,65 × 0,50)
    R, H, Z0 = 0.65, 0.50, 0.34
    prof = [(0.0, 0.115), (0.60, 0.115), (0.62, 0.135), (0.62, Z0 - 0.03), (R + 0.012, Z0 - 0.01), (R + 0.012, Z0 + 0.01)]
    for i in range(1, 15):
        a = math.radians(90.0 * i / 15)
        prof.append((R * math.cos(a), Z0 + H * math.sin(a)))
    prof.append((0.0, Z0 + H))
    vp.lathe(p, 'Paint', (0, 0, 0), 'z', prof, seg=48)
    vp.ring_band(p, 'Team', 0.19, 0.25, 0.62, 0.0, axis=(0, 0, 1), lip=0.006, seg=48)
    # as juntas dos gomos (6 meridianos, longe do olho) e o anel de cima
    for k in range(6):
        az = math.radians(30 + 60 * k)
        if abs(math.cos(az) - 1.0) < 0.2:
            continue
        pts = []
        for i in range(2, 13):
            a = math.radians(90.0 * i / 15)
            rr = R * math.cos(a) + 0.006
            pts.append((rr * math.cos(az), rr * math.sin(az), Z0 + (H + 0.006 / max(0.2, math.sin(a))) * math.sin(a)))
        vp.tube(p, 'Dark', pts, 0.011, seg=6)
    vp.lathe(p, 'Dark', (0, 0, 0), 'z', [(0.20, Z0 + H * 0.95), (0.235, Z0 + H * 0.93), (0.235, Z0 + H * 0.94 + 0.01),
                                         (0.20, Z0 + H * 0.96 + 0.01)], seg=32, closed=True)
    # o OLHO: a carcaça (Paint) saindo da cúpula, o aro escuro e a lente grande acesa (Light)
    tl = math.radians(LASER_TILT)
    d = Vector((math.cos(tl), 0.0, math.sin(tl)))
    C = Vector((0.0, 0.0, Z0 + 0.06))
    vp.lathe(p, 'Paint', C, d, [(0.0, 0.30), (0.33, 0.30), (0.33, 0.56), (0.32, 0.595), (0.30, 0.61), (0.30, 0.595), (0.0, 0.595)],
             seg=40)
    vp.lathe(p, 'Dark', C, d, [(0.245, 0.59), (0.305, 0.59), (0.305, 0.615), (0.29, 0.625), (0.245, 0.625)], seg=40, closed=True)
    vp.lathe(p, 'Light', C, d, [(0.0, 0.612), (0.13, 0.61), (0.22, 0.603), (0.25, 0.597), (0.25, 0.594), (0.0, 0.594)], seg=40)
    # os 2 rastreadores (as janelas pequenas) nos lados do olho, no tambor
    for s in (-1, 1):
        az = math.radians(48 * s)
        c = Vector((0.63 * math.cos(az), 0.63 * math.sin(az), 0.25))
        vp.rbox(p, 'Paint#plate', c, (0.16, 0.2, 0.16), yaw=math.degrees(az), r=0.04, k=3)
        vp.rbox(p, 'Lens', c + Vector((math.cos(az), math.sin(az), 0)) * 0.082, (0.01, 0.14, 0.1), yaw=math.degrees(az), r=0.02, k=2)
    # as aletas de refrigeração atrás (no tambor)
    for k in range(9):
        az = math.radians(180 - 48 + 12 * k)
        c = Vector((0.66 * math.cos(az), 0.66 * math.sin(az), 0.22))
        vp.rbox(p, 'Dark', c, (0.1, 0.018, 0.17), yaw=math.degrees(az), r=0.006, k=1)
    vp.poke_ngons(p)
    return p.finish()


# ---------------------------------------------------------------------------------- cano
def barrel():
    """O cano grosso e comprido: a luva de 0,40 m perto do mantelete, o degrau, o EVACUADOR (o bojo do meio) com o anel da cor do
    jogador, o tubo afinando até o FREIO DE BOCA de 2 câmaras com as janelas."""
    p = Part(PREFIX + '_Barrel')
    x0 = MANTLET_X
    st = [(x0 - 0.06, 0.20), (x0 + 0.05, 0.20), (x0 + 0.09, 0.185), (x0 + 1.25, 0.18), (x0 + 1.30, 0.168), (x0 + 1.36, 0.155),
          (x0 + 2.05, 0.152), (x0 + 2.14, 0.205), (x0 + 2.22, 0.215), (x0 + 2.86, 0.215), (x0 + 2.94, 0.205), (x0 + 3.02, 0.15),
          (MUZZLE_X - 0.68, 0.138), (MUZZLE_X - 0.04, 0.138)]
    vp.gun_barrel(p, st, GUN_Z, m='Paint', seg=32, bore=0.07, bore_depth=0.4, bore_m='Dark')
    for xx, r in ((x0 + 0.42, 0.183), (x0 + 0.84, 0.181), (x0 + 1.7, 0.153), (x0 + 3.5, 0.144)):
        vp.ring_band(p, 'Metal', xx - 0.03, xx + 0.03, r, GUN_Z, lip=0.01, seg=32)
        vp.rbox(p, 'Metal', (xx, 0.0, GUN_Z - r - 0.014), (0.05, 0.05, 0.03), r=0.012, k=1)
    vp.ring_band(p, 'Team', x0 + 2.40, x0 + 2.68, 0.215, GUN_Z, lip=0.007, seg=32)
    # o freio de boca: 2 câmaras (os blocos redondos) com as janelas escuras dos lados e a ponta chanfrada
    for (a, b) in ((MUZZLE_X - 0.64, MUZZLE_X - 0.36), (MUZZLE_X - 0.30, MUZZLE_X)):
        vp.lathe(p, 'Paint', (0.0, 0.0, GUN_Z), 'x', [(0.13, a), (0.20, a), (0.225, a + 0.025), (0.225, b - 0.03), (0.205, b),
                                                      (0.13, b)], seg=32, closed=True)
    for xc in (MUZZLE_X - 0.50, MUZZLE_X - 0.15):
        for sgn in (-1, 1):
            p.box('Dark#vp', xc - 0.06, xc + 0.06, sgn * 0.225 - 0.005, sgn * 0.225 + 0.005, GUN_Z - 0.09, GUN_Z + 0.09, bevel=0.0)
    for sgn in (-1, 1):
        p.box('Dark#vp', MUZZLE_X - 0.36, MUZZLE_X - 0.30, sgn * 0.14 - 0.004, sgn * 0.14 + 0.004, GUN_Z - 0.11, GUN_Z + 0.11, bevel=0.0)
    vp.bolt_ring(p, 'Metal', (MUZZLE_X - 0.64, 0.0, GUN_Z), (-1, 0, 0), 0.17, 8, r=0.014, h=0.01)
    return p.finish()


# ---------------------------------------------------------------------------------- rodagem
def wheels():
    p = Part(PREFIX + '_Wheels')
    for s in (-1, 1):
        y = TRACK_Y * s
        for x in WHEELS_X:
            vp.road_wheel(p, (x, y, WHEEL_Z), WHEEL_R, 0.60, s, 'Paint', 'Metal', 'Dark', gap=0.11, seg=26)
        ix, iz, ir = IDLER
        vp.idler(p, (ix, y, iz), ir, 0.58, s, 'Paint', 'Metal', gap=0.11)
        sx, sz, sr = SPROCKET
        vp.sprocket(p, (sx, y, sz), sr, 0.58, s, 'Paint', 'Metal', teeth=12, holes_m='Dark')
        for x in ROLLERS_X:
            vp.return_roller(p, (x, y, ROLLER_Z), ROLLER_R, 0.32, 'Metal', 'Dark')
    return p.finish()


def track(side):
    s = 1 if side == 'r' else -1
    m = 0.06
    circles = [(SPROCKET[0], SPROCKET[1], SPROCKET[2] + m + 0.01), (IDLER[0], IDLER[1], IDLER[2] + m)] + \
              [(x, WHEEL_Z, WHEEL_R + m) for x in WHEELS_X] + [(x, ROLLER_Z, ROLLER_R + m) for x in ROLLERS_X]
    zt = ROLLER_Z + ROLLER_R + m
    sags = [(ROLLERS_X[1] + 0.12, ROLLERS_X[0] - 0.12, zt, 0.03), (ROLLERS_X[2] + 0.12, ROLLERS_X[1] - 0.12, zt, 0.03)]
    return vp.track_links(f'{PREFIX}_Track_{side}', circles, TRACK_Y * s, TRACK_W, pitch=0.19, t_in=0.058, t_out=0.015,
                          grouser=0.022, guide='single', m='Track', guide_m='Metal', sags=sags, center_gap=0.12, horn_w=0.08)


# ---------------------------------------------------------------------------------- conferência e exportação
def tree(o, M=None, zmin=None):
    M = M or Matrix.Identity(4)
    vs = [M @ v.co for v in o.data.vertices]
    polys = [tuple(q.vertices) for q in o.data.polygons if zmin is None or min(o.data.vertices[i].co.z for i in q.vertices) > zmin]
    return BVHTree.FromPolygons(vs, polys)


def sweep_check(h, objs, l_placed):
    """A torre (com o cano e o laser no lugar) girando não pode atravessar o casco; o laser girando no eixo dele não pode bater na torre
    (o prato de baixo encosta no pedestal: só conta acima de 4 cm)."""
    ht = tree(h)
    piv = V(TX, 0.0, 0.0)
    hits = []
    for k in range(72):
        Y = Matrix.Translation(piv) @ Matrix.Rotation(math.radians(5 * k), 4, 'Z') @ Matrix.Translation(-piv)
        n = sum(len(ht.overlap(tree(o, Y))) for o in objs + [l_placed])
        if n:
            hits.append((5 * k, n))
    print('MODEL giro da torre: %s' % ('sem colisao em 360 graus' if not hits else 'COLISAO em %s' % hits[:12]))
    tt = tree(objs[0])
    la = V(*LASER_AT)
    hits = []
    for k in range(36):
        M = Matrix.Translation(la) @ Matrix.Rotation(math.radians(10 * k), 4, 'Z') @ Matrix.Translation(-la)
        n = len(tt.overlap(tree(l_placed, M, zmin=LASER_AT[2] + 0.04)))
        if n:
            hits.append((10 * k, n))
    print('MODEL giro do laser: %s' % ('livre' if not hits else 'COLISAO em %s' % hits[:12]))


def write_spec():
    import json
    spec = {
        'parts': [[PREFIX + n, [0, 0, 0]] for n in ('_Hull', '_Turret', '_Barrel', '_Wheels', '_Track_l', '_Track_r')]
                 + [[PREFIX + '_Laser', [LASER_AT[0], -LASER_AT[1], LASER_AT[2]]]],
        'views': [['top', 'ortho', [0, 0, -1], [0, -1, 0], [1800, 1000]],
                  ['left', 'ortho', [0, -1, 0], [0, 0, 1], [1800, 760]],
                  ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1000, 760]],
                  ['back', 'ortho', [1, 0, 0], [0, 0, 1], [1000, 760]],
                  ['three_quarter', 'persp', 30.0, 215.0, [1600, 1000], 30.0, 0.78],
                  ['back_three_quarter', 'persp', 30.0, 35.0, [1600, 1000], 30.0, 0.78]],
        'usa_at': [0, 7.0, 0], 'tank_views': False,
        'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
    }
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)


def main():
    clear_scene()
    h, t, b, w = hull(), turret(), barrel(), wheels()
    tl, tr = track('l'), track('r')
    l = laser()
    la = V(*LASER_AT)
    # (o truque do Dozer B e do Linebacker B) o laser cozinha no lugar dele (a sombra de contato com o pedestal) e volta pra origem
    # do eixo só na exportação
    l.data.transform(Matrix.Translation(la))
    sweep_check(h, [t, b], l)
    import model_lib
    if model_lib.DETAIL.get('defer') is not None:
        _orig = model_lib.export

        def _export(o, out_dir):
            if o.name == PREFIX + '_Laser' and model_lib.DETAIL.get('defer') is None:
                o.data.transform(Matrix.Translation(-la))
            return _orig(o, out_dir)
        model_lib.export = _export
    else:
        l.data.transform(Matrix.Translation(-la))
    pre = {o.name: tris(o) for o in (h, t, b, l, w, tl, tr)}
    total = 0
    for o in (h, t, b, l, w, tl, tr):
        total += export(o, OUT)
    print('MODEL total', total, 'tris (antes da grade do executor); por peca', pre)
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h, t, b, w, tl, tr])
    (hx0, hx1), (hy0, hy1), _ = bounds([h, w, tl, tr])
    print('MODEL medidas: comprimento %.2f m com o cano (x %.2f..%.2f), casco %.2f x %.2f m (x %.2f..%.2f), deck %.2f, teto %.2f'
          % (ax1 - ax0, ax0, ax1, hx1 - hx0, hy1 - hy0, hx0, hx1, DECK, T_Z2))
    # a caixa da torre (o corpo que voa na morte): sem as antenas (quebram); com o pedestal, a mira, a cúpula e o mantelete
    vs = [(v.co.x, -v.co.y, v.co.z) for v in t.data.vertices if v.co.z < LASER_AT[2] + 0.01]
    print('MODEL turret_box_m', [[round(min(q[i] for q in vs), 2) for i in range(3)], [round(max(q[i] for q in vs), 2) for i in range(3)]])
    print('MODEL turret_pivot_m', [TX, 0.0], 'muzzle_height_m', GUN_Z, 'a boca em x', MUZZLE_X, 'tracks_m',
          [round(2 * TRACK_Y, 3), TRACK_W], 'laser at_m', [round(v, 3) for v in LASER_AT])
    write_spec()


main()
