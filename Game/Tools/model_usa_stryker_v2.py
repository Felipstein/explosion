"""A Stryker da USA (usa_stryker, o transporte de tropas 8×8), VERSÃO B no detalhe AAA (01/11/2026): a versão A
(Game/Tools/model_usa_stryker.py, a ficha do Gemini aprovada em 25/10: "gemini, sem dúvida nenhuma, exatamente, quase uma cópia
perfeita") fica como está; esta é a que ele compara com ela.

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_usa_stryker_v2.py C:/Users/luisf/explosion/captures/game/models/usa_stryker_v2

A régua é a ficha docs/concept/units/usa_stryker_sheet_gemini.png, vista por vista. O dono, sobre os da USA (01/11): "deixe eles
muito mais parecidos [com os] detalhes da ficha". O casco é o do A (a seção de cintura em diamante, o nariz em V raso, o teto plano
com o chanfro, medidos na ficha e esticados ~15% no meio, como combinado em 25/10); o que muda é o DETALHE de cada peça, como a
ficha tem:
  - o teto: as 4 escotilhas da tropa com a moldura, as 2 dobradiças de nós, a alça e a trava; as juntas das chapas e as fileiras de
    parafusos; a grelha do motor de aletas inclinadas na moldura, a grelha pequena e o respiro de chapa; a cúpula de 8 faces com os
    periscópios e a tampa abaulada; as alças de mão e os olhais de içar; a antena curta da ficha; a lona da cor do jogador enrolada;
  - os lados: a quina da cintura em barra redonda (a linha que pega o sol, a "cintura" da ficha), a faixa azul grossa segmentada,
    os 2 cestos da tropa de barras com a carga dentro (caixa, saco, galão), as caixas de lado com a tampa, as travas e as dobradiças,
    os arcos escuros com a suspensão (a mola, o braço, o amortecedor);
  - o nariz: as chapas do glacis com as dobradiças e os parafusos, os faróis redondos nas caixas dos cantos com a grade de proteção,
    as luzes de comboio, as manilhas de reboque;
  - a traseira: a rampa da tropa com a porta embutida, o visor, a alavanca, as dobradiças de baixo, as lanternas redondas na guarda,
    os degraus;
  - a estação de armas: a base em 2 degraus com o anel de parafusos, o corpo facetado, o berço, a .50 com a caixa e a tampa do
    alimentador, a caixa do sensor com a janela da lente e a pala, a lata de munição com a calha; o cano com a luva furada e o
    quebra-chamas;
  - as rodas: o pneu de verdade (model_vehicle_parts.tyre: o flanco abaulado, os blocos em V), a roda funda com o anel de porcas e a
    tampa do CTIS no cubo.
A USA é o exército limpo (o dono, 13/10): nada de desgaste pintado; a sujeira vem do material do jogo.

Escala: desenhado nas medidas do A (o "desenho") e multiplicado por SCALE 0,72 no fim, como o A; o jogo ainda aplica mesh_scale
1,25 (no jogo, × 0,9 do desenho: as porcas de 2,2 cm do desenho ficam com 2 cm).

Pivôs MANTIDOS (Units.json usa_stryker, já × 1,25): turret_pivot_m (−0,315; 0) = o PIVOT (−0,35; 0) do desenho; muzzle_height_m
2,592 = GUN_Z 2,88; a boca da .50 em x 1,27 do desenho (o X máximo do Barrel); wheels.tyre_w_m 0,414 = o pneu de 0,46; os eixos em
±1,45/±0,55 m da malha e a roda em y ±0,983 m (os mesmos do A).

Peças (origem no chão, no centro do casco, frente em +X; TODAS nas coordenadas do veículo, no lugar delas; os NOMES do A):
  SM_USA_Stryker_Hull, SM_USA_Stryker_Wheels (wheel_001..008, a ordem do A), SM_USA_Stryker_Turret, SM_USA_Stryker_Barrel.
Slots (os do A): Paint, Team, Cloth, Dark, Metal, Rubber, Lens, Light.
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, export_group  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
from model_china_vehicle_kit import frame_box, tris  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_stryker_v2')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_USA_Stryker'

COLORS.update({'Paint': (196, 198, 192), 'Team': (86, 108, 150), 'Cloth': (70, 92, 140), 'Dark': (40, 40, 38),
               'Metal': (95, 95, 92), 'Rubber': (32, 32, 31), 'Lens': (92, 128, 168), 'Light': (255, 230, 170)})

# ---------------------------------------------------------------------------------- medidas (desenho, antes da escala: as do A)
FRONT, REAR = 3.68, -3.62
ROOF_Z = 2.22
CHINE = (1.58, 1.31)
ROOF_E = (1.375, 2.10)
ROOF_HW = 1.24
GLACIS_X = FRONT - 1.11
BEAK_X, BEAK_Z = FRONT - 0.12, 1.50
NOSE_X0 = FRONT - 0.75
NOSE_V = 0.18
BELLY_CZ, TIP_CZ = 0.46, 1.12
BODY = [(0.72, 0.54), (1.04, 1.08), (1.30, 1.18), (CHINE[0], 1.27), CHINE]
TIP = [(1.10, 1.14), (1.20, 1.16), (1.28, 1.19), (1.36, 1.24), (1.36, 1.30)]
TIP_W = 1.36
WHEEL_R, WHEEL_W = 0.51, 0.46
WHEEL_Y = CHINE[0] + 0.015 - WHEEL_W / 2
SCALE = 0.72
AXLES_M = [1.45, 0.55, -0.55, -1.45]
AXLES_X = [a / SCALE for a in AXLES_M]
PIVOT = (-0.35, 0.0)
RING_R, RING_H = 0.64, 0.025
BASE_R = 0.60
GUN_Z = 2.88
GUN_X0 = 0.40
MUZZLE = 1.62
SEAMS = [-2.20, -1.20, 0.62, 1.60]
HATCHES = [(x0, x1, y0, y1) for (x0, x1) in ((-3.02, -2.26), (-2.10, -1.34)) for (y0, y1) in ((-0.76, -0.14), (0.14, 0.76))]
CUPOLA = (1.38, -0.66)

GL_K = (ROOF_Z - BEAK_Z) / (BEAK_X - GLACIS_X)
SIDE_K = (CHINE[0] - ROOF_E[0]) / (ROOF_E[1] - CHINE[1])


# ---------------------------------------------------------------------------------- o casco (a forma do A)
def nose_dx(x, y):
    v = min(1.0, max(0.0, (x - NOSE_X0) / (FRONT - NOSE_X0)))
    return -NOSE_V * v * min(1.0, abs(y) / TIP_W)


def ring(x):
    """A seção do casco na estação x (16 pontos; ver o A): o índice 5 é a quina da cintura da direita, o 11 a da esquerda."""
    v = min(1.0, max(0.0, (x - NOSE_X0) / (FRONT - NOSE_X0)))
    low = [(a[0] + (b[0] - a[0]) * v, a[1] + (b[1] - a[1]) * v) for a, b in zip(BODY, TIP)]
    cz = BELLY_CZ + (TIP_CZ - BELLY_CZ) * v
    ye, yr, zt = ROOF_E[0], ROOF_HW, ROOF_Z
    if x > GLACIS_X:
        u = min(1.0, (x - GLACIS_X) / (BEAK_X - GLACIS_X))
        zt = ROOF_Z + (BEAK_Z - ROOF_Z) * u
        ye, yr = ROOF_E[0] - 0.10 * u, ROOF_HW - 0.06 * u
    if x > BEAK_X:
        zt = BEAK_Z - 0.06 * (x - BEAK_X) / (FRONT - BEAK_X)
    right = low + [(ye, zt - (ROOF_Z - ROOF_E[1])), (yr, zt)]
    pts = [(0.0, cz)] + right + [(0.0, zt)] + [(-y, z) for (y, z) in reversed(right)]
    return [(x + nose_dx(x, y), y, z) for (y, z) in pts]


def hull_shell(p, stations, m='Paint#hull', bevel=0.045):
    b = p.bm(m, bevel)
    rings = [[b.verts.new(V(*q)) for q in ring(x)] for x in stations]
    n = len(rings[0])
    for r0, r1 in zip(rings[:-1], rings[1:]):
        for i in range(n):
            j = (i + 1) % n
            b.faces.new([r0[i], r0[j], r1[j], r1[i]])
    b.faces.new(list(reversed(rings[0])))
    r, k = rings[-1], n // 2
    b.faces.new(r[0:k + 1])
    b.faces.new(r[k:] + [r[0]])


def side_pt(x, z, s, h=0.0):
    y = CHINE[0] - (z - CHINE[1]) * SIDE_K
    n = 1.0 / math.hypot(1.0, SIDE_K)
    return (x, s * (y + h * n), z + h * SIDE_K * n)


def side_frame(s):
    """O referencial do lado de cima (a chapa que fecha pra dentro): (eu = x, ev = subindo pela chapa, en = a normal pra fora)."""
    n = 1.0 / math.hypot(1.0, SIDE_K)
    en = Vector((0.0, s * n, SIDE_K * n))
    ev = Vector((0.0, -s * SIDE_K * n, n))
    return Vector((1.0, 0.0, 0.0)), ev, en


def side_plate(p, m, x0, x1, z0, z1, s, off=0.006, t=0.012, bevel=None):
    r = lambda x: [side_pt(x, z0, s, off), side_pt(x, z0, s, off + t), side_pt(x, z1, s, off + t), side_pt(x, z1, s, off)]
    p.loft(m, [r(x0), r(x1)], bevel=bevel)


def side_box(p, m, x0, x1, z0, z1, s, depth, r=0.035, k=4, off=0.0):
    """A caixa de quinas redondas colada no lado de cima (as tampas em quads), de x0 a x1, da altura z0 a z1, 'depth' pra fora."""
    eu, ev, en = side_frame(s)
    zc = (z0 + z1) / 2
    c = Vector(side_pt((x0 + x1) / 2, zc, s, off + depth / 2))
    L = (z1 - z0) * math.hypot(1.0, SIDE_K)
    vp.rbox_q(p, m, c, (x1 - x0, L, depth), r=r, k=k, frame=(eu, ev, en))


def gl_pt(x, y, h=0.0):
    n = 1.0 / math.hypot(1.0, GL_K)
    return (x + h * GL_K * n + nose_dx(x, y), y, ROOF_Z - GL_K * (x - GLACIS_X) + h * n)


def gl_plate(p, m, x0, x1, y0, y1, off=0.0, t=0.03, bevel=None):
    r = lambda y: [gl_pt(x0, y, off), gl_pt(x1, y, off), gl_pt(x1, y, off + t), gl_pt(x0, y, off + t)]
    p.loft(m, [r(y0), r(y1)], bevel=bevel)


def gl_normal():
    n = 1.0 / math.hypot(1.0, GL_K)
    return Vector((GL_K * n, 0.0, n))


def low_nose_z(x, y):
    v = min(1.0, max(0.0, (x - NOSE_X0) / (FRONT - NOSE_X0)))
    cz = BELLY_CZ + (TIP_CZ - BELLY_CZ) * v
    ay = BODY[0][0] + (TIP[0][0] - BODY[0][0]) * v
    az = BODY[0][1] + (TIP[0][1] - BODY[0][1]) * v
    return cz + (az - cz) * min(1.0, abs(y) / ay)


def tube(p, m, a, b, r, seg=10):
    vp.tube(p, m, [a, b], r, seg=seg)


# ---------------------------------------------------------------------------------- peças de teto
def roof_hatch(p, x0, x1, y0, y1):
    """A escotilha da tropa (a ficha): o batente escuro, a tampa de quinas redondas com o miolo em relevo, as 2 dobradiças de nós do
    lado de dentro, a alça em U e a trava redonda do lado de fora."""
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    w, h = x1 - x0, y1 - y0
    vp.rbox_q(p, 'Dark', (cx, cy, ROOF_Z + 0.008), (w + 0.07, h + 0.07, 0.016), r=0.09, k=4)
    vp.rbox_q(p, 'Paint#plate', (cx, cy, ROOF_Z + 0.03), (w, h, 0.044), r=0.08, k=4)
    vp.rbox_q(p, 'Paint#thin', (cx, cy, ROOF_Z + 0.058), (w - 0.18, h - 0.18, 0.016), r=0.05, k=3)
    y_in = y0 + 0.04 if y0 > 0 else y1 - 0.04
    y_out = y1 - 0.12 if y0 > 0 else y0 + 0.12
    for hx in (x0 + 0.16, x1 - 0.16):
        vp.hinge(p, 'Metal', (hx, y_in, ROOF_Z + 0.06), 'x', 0.14, r=0.026, knuckles=3)
        vp.rbox(p, 'Metal', (hx, y_in + (0.06 if y0 > 0 else -0.06), ROOF_Z + 0.054), (0.12, 0.1, 0.012), r=0.02, k=2)
    vp.grab_handle(p, 'Metal', (cx - 0.1, y_out, ROOF_Z + 0.066), (cx + 0.1, y_out, ROOF_Z + 0.066), (0, 0, 1), height=0.05, r=0.012)
    vp.lathe(p, 'Metal', (x1 - 0.1, y_out, ROOF_Z + 0.052), 'z', [(0.0, 0.0), (0.035, 0.0), (0.035, 0.02), (0.02, 0.035), (0.0, 0.035)],
             seg=10)


def roof_grille(p, x0, x1, y0, y1, n, bar=None):
    """A grelha do motor no teto (a ficha: as aletas atravessadas em y, repetidas ao longo de x): a moldura de quinas redondas, o fundo
    escuro fundo, as aletas inclinadas 30° e a travessa do meio."""
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    vp.rbox_q(p, 'Dark', (cx, cy, ROOF_Z + 0.006), (x1 - x0 - 0.04, y1 - y0 - 0.04, 0.012), r=0.03, k=3)
    fw = 0.05
    for sgn in (-1, 1):
        vp.rbox(p, 'Paint#plate', (cx, cy + sgn * ((y1 - y0) / 2 - fw / 2), ROOF_Z + 0.03), (x1 - x0, fw, 0.06), r=0.02, k=2)
        vp.rbox(p, 'Paint#plate', (cx + sgn * ((x1 - x0) / 2 - fw / 2), cy, ROOF_Z + 0.03), (fw, y1 - y0 - 2 * fw + 0.004, 0.06), r=0.02, k=2)
    step = (x1 - x0 - 2 * fw) / n
    ex, ey, ez = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
    a = math.radians(30)
    e1 = ex * math.cos(a) + ez * math.sin(a)
    e3 = ez * math.cos(a) - ex * math.sin(a)
    for k in range(n):
        x = x0 + fw + step * (k + 0.5)
        frame_box(p, 'Metal', (x, cy, ROOF_Z + 0.035), e1, ey, e3, step * 0.85, y1 - y0 - 2 * fw, 0.012, bevel=0.0)
    if bar is not None:
        vp.rbox(p, 'Paint#plate', (bar, cy, ROOF_Z + 0.04), (0.05, y1 - y0, 0.07), r=0.02, k=2)


def cupola(p, cx, cy):
    """A cúpula do comandante (a ficha: o anel de 8 faces na frente do teto): o anel facetado com os 5 periscópios na frente e nos
    lados, a tampa abaulada com a dobradiça e a alça."""
    z = ROOF_Z
    vp.lathe(p, 'Paint', (cx, cy, z), 'z', [(0.0, 0.0), (0.44, 0.0), (0.44, 0.03), (0.41, 0.06), (0.39, 0.15), (0.36, 0.17), (0.0, 0.17)],
             seg=8, phase=math.pi / 8, smooth=False)
    vp.lathe(p, 'Dark', (cx, cy, z + 0.17), 'z', [(0.3, 0.0), (0.35, 0.0), (0.35, 0.012), (0.3, 0.012)], seg=24, closed=True)
    vp.lathe(p, 'Paint', (cx, cy, z + 0.17), 'z', [(0.0, 0.065), (0.15, 0.06), (0.26, 0.045), (0.31, 0.025), (0.315, 0.0), (0.0, 0.0)], seg=24)
    vp.hinge(p, 'Metal', (cx - 0.33, cy, z + 0.19), 'y', 0.24, r=0.025, knuckles=3)
    vp.grab_handle(p, 'Metal', (cx + 0.12, cy - 0.08, z + 0.23), (cx + 0.12, cy + 0.08, z + 0.23), (0, 0, 1), height=0.04, r=0.011)
    for k in range(-2, 3):                              # os periscópios (o bloco escuro com o vidro), nas faces da frente e dos lados
        a = math.radians(k * 45)
        r_ = 0.405
        c = (cx + r_ * math.cos(a), cy + r_ * math.sin(a), z + 0.105)
        vp.rbox(p, 'Dark', c, (0.05, 0.17, 0.07), yaw=k * 45, r=0.012, k=2)
        vp.rbox(p, 'Lens', (cx + (r_ + 0.026) * math.cos(a), cy + (r_ + 0.026) * math.sin(a), z + 0.105), (0.006, 0.13, 0.04),
                yaw=k * 45, r=0.008, k=1)
        vp.rbox(p, 'Paint', (cx + (r_ + 0.01) * math.cos(a), cy + (r_ + 0.01) * math.sin(a), z + 0.15), (0.08, 0.2, 0.014), yaw=k * 45,
                r=0.01, k=1)


def lift_eye(p, x, y, axis='x'):
    vp.rbox(p, 'Metal', (x, y, ROOF_Z + 0.015), (0.12 if axis == 'x' else 0.06, 0.06 if axis == 'x' else 0.12, 0.03), r=0.015, k=2)
    p.torus('Metal#vp', (x, y, ROOF_Z + 0.07), 'y' if axis == 'x' else 'x', 0.04, 0.014, seg=12, rseg=6)


# ---------------------------------------------------------------------------------- o casco inteiro
def hull():
    p = Part(PREFIX + '_Hull')
    p.bm('Paint#hull', 0.045)
    p.bm('Paint#plate', 0.022)
    p.bm('Paint#thin', 0.006)
    p.bm('Paint', 0.02)
    p.bm('Team', 0.005)
    p.bm('Dark', 0.0)
    p.bm('Metal', 0.0)
    hull_shell(p, [REAR, GLACIS_X, NOSE_X0, BEAK_X, FRONT])
    # ---------------------------------------------------------------- a quina da cintura em barra redonda (a linha que pega o sol)
    for idx, s in ((5, 1), (11, -1)):
        xs = [REAR + 0.04] + [REAR + 0.04 + (NOSE_X0 - REAR - 0.04) * k / 10 for k in range(1, 11)] + \
             [NOSE_X0 + (FRONT - 0.04 - NOSE_X0) * k / 6 for k in range(1, 7)]
        pts = []
        for x in xs:
            q = ring(x)[idx]
            pts.append((q[0], q[1] + 0.012 * s, q[2] - 0.004))
        vp.tube(p, 'Paint', pts, 0.03, seg=10)
    # ---------------------------------------------------------------- o teto: o anel fixo da estação com os parafusos, as escotilhas,
    # as juntas e os parafusos das chapas, as alças e os olhais
    vp.lathe(p, 'Dark', (PIVOT[0], PIVOT[1], ROOF_Z), 'z', [(RING_R - 0.08, 0.0), (RING_R + 0.02, 0.0), (RING_R + 0.02, RING_H - 0.006),
                                                           (RING_R, RING_H), (RING_R - 0.08, RING_H)], seg=48, closed=True)
    vp.bolt_ring(p, 'Metal', (PIVOT[0], PIVOT[1], ROOF_Z + RING_H), 'z', RING_R - 0.008, 24, r=0.013, h=0.008)
    for (hx0, hx1, hy0, hy1) in HATCHES:
        roof_hatch(p, hx0, hx1, hy0, hy1)
    for jx in SEAMS[:3]:
        vp.panel_seam(p, 'Dark', (jx, -ROOF_HW + 0.06, ROOF_Z + 0.002), (jx, ROOF_HW - 0.06, ROOF_Z + 0.002), (0, 0, 1), w=0.02, d=0.01)
        for s in (-1, 1):
            vp.bolt_row(p, 'Metal', (jx - 0.05, 0.86 * s, ROOF_Z), (jx - 0.05, 1.18 * s, ROOF_Z), 3, (0, 0, 1), r=0.016, h=0.01)
    for s in (-1, 1):
        vp.panel_seam(p, 'Dark', (REAR + 0.1, 0.98 * s, ROOF_Z + 0.002), (SEAMS[2], 0.98 * s, ROOF_Z + 0.002), (0, 0, 1), w=0.02, d=0.01)
        vp.bolt_row(p, 'Metal', (REAR + 0.12, (ROOF_HW - 0.07) * s, ROOF_Z), (GLACIS_X - 0.1, (ROOF_HW - 0.07) * s, ROOF_Z), 26, (0, 0, 1),
                    r=0.016, h=0.01)
        for x in (-3.3, -1.75, -0.95, 0.35):
            vp.grab_handle(p, 'Metal', (x - 0.1, 1.1 * s, ROOF_Z), (x + 0.1, 1.1 * s, ROOF_Z), (0, 0, 1), height=0.05, r=0.012)
    vp.bolt_row(p, 'Metal', (REAR + 0.07, -1.1, ROOF_Z), (REAR + 0.07, 1.1, ROOF_Z), 14, (0, 0, 1), r=0.016, h=0.01)
    for (ex, ey, ax) in ((-3.52, 1.12, 'x'), (-3.52, -1.12, 'x'), (2.46, 1.12, 'y'), (2.46, -1.12, 'y')):
        lift_eye(p, ex, ey, ax)
    # ---------------------------------------------------------------- a frente do teto: a cúpula, a caixa ao lado, as grelhas, o respiro
    cupola(p, *CUPOLA)
    vp.rbox_q(p, 'Paint#plate', (0.85, -0.73, ROOF_Z + 0.065), (0.3, 0.82, 0.13), r=0.04, k=4)
    for y in (-1.0, -0.46):
        vp.rbox(p, 'Metal', (1.003, y, ROOF_Z + 0.09), (0.012, 0.06, 0.05), r=0.01, k=1)
    roof_grille(p, 0.80, 2.22, -0.10, 0.58, 18, bar=1.50)
    roof_grille(p, 1.18, 1.78, 0.72, 1.12, 7)
    vp.rbox_q(p, 'Dark', (2.07, 0.91, ROOF_Z + 0.012), (0.3, 0.34, 0.024), r=0.03, k=3)
    for i in range(4):
        for j in range(4):
            vp.rbox(p, 'Metal', (1.96 + i * 0.075, 0.8 + j * 0.075, ROOF_Z + 0.028), (0.04, 0.012, 0.01), yaw=45 if (i + j) % 2 else -45,
                    r=0.004, k=1)
    # o periscópio do motorista na frente do teto e o limpador da janela (a ficha: a caixinha na borda do glacis)
    vp.rbox(p, 'Dark', (2.42, 0.66, ROOF_Z + 0.06), (0.1, 0.32, 0.12), r=0.02, k=2)
    vp.rbox(p, 'Lens', (2.475, 0.66, ROOF_Z + 0.07), (0.008, 0.26, 0.06), r=0.01, k=1)
    vp.rbox(p, 'Paint', (2.43, 0.66, ROOF_Z + 0.125), (0.16, 0.38, 0.016), r=0.015, k=2)
    # ---------------------------------------------------------------- a lona da cor do jogador enrolada no teto de trás e a antena
    vp.tarp_roll(p, (-3.31, 0.0, ROOF_Z + 0.12), 'y', 1.72, 0.12, m='Cloth', straps=3, strap_m='Dark', buckle_m='Metal')
    for y in (-0.6, 0.0, 0.6):
        vp.rbox(p, 'Dark', (-3.18, y, ROOF_Z + 0.02), (0.08, 0.05, 0.04), r=0.01, k=1)
    vp.rbox_q(p, 'Paint#plate', (-2.88, -1.06, ROOF_Z + 0.05), (0.2, 0.2, 0.1), r=0.04, k=3)
    vp.lathe(p, 'Dark', (-2.88, -1.06, ROOF_Z + 0.1), 'z', [(0.0, 0.0), (0.05, 0.0), (0.05, 0.04), (0.035, 0.06), (0.032, 0.5),
                                                           (0.045, 0.52), (0.045, 0.56), (0.0, 0.57)], seg=12)
    # ---------------------------------------------------------------- os lados
    for s in (-1, 1):
        # a faixa da cor do jogador (grossa, 0,26 m de chapa), segmentada pelas juntas das placas
        segs = [(-3.30, SEAMS[0] - 0.04), (SEAMS[0] + 0.04, SEAMS[1] - 0.04), (SEAMS[1] + 0.04, SEAMS[2] - 0.04),
                (SEAMS[2] + 0.04, SEAMS[3] - 0.04), (SEAMS[3] + 0.04, 2.46)]
        for (a, b) in segs:
            side_plate(p, 'Team', a, b, 1.37, 1.62, s, off=0.004, t=0.008)
        for (a, b) in zip(segs[:-1], segs[1:]):
            jx = (a[1] + b[0]) / 2
            side_plate(p, 'Dark', jx - 0.01, jx + 0.01, 1.35, 2.08, s, off=0.0, t=0.006)
        # as fileiras de parafusos das chapas (por cima e por baixo da faixa)
        eu, ev, en = side_frame(s)
        for z in (1.66,):
            a_ = Vector(side_pt(-3.4, z, s, 0.004))
            b_ = Vector(side_pt(2.4, z, s, 0.004))
            vp.bolt_row(p, 'Metal', a_, b_, 30, en, r=0.016, h=0.01)
        # os 2 cestos da tropa: a chapa de fundo, a carga (a caixa, o saco, o galão), os montantes, as 2 barras da frente e o
        # fundo do cesto
        for (a, b) in ((-3.08, -2.28), (-2.12, -1.28)):
            side_plate(p, 'Dark', a + 0.02, b - 0.02, 1.64, 2.06, s, off=0.0, t=0.012)              # o fundo escuro do cesto
            side_box(p, 'Paint#plate', a + 0.05, a + 0.37, 1.70, 1.98, s, 0.1, r=0.03)
            side_box(p, 'Paint#plate', a + 0.41, b - 0.24, 1.70, 1.87, s, 0.085, r=0.05)            # o saco (quinas bem redondas)
            vp.lathe(p, 'Metal', Vector(side_pt(b - 0.13, 1.83, s, 0.06)), en, [(0.0, -0.06), (0.06, -0.06), (0.065, -0.04), (0.065, 0.04),
                                                                            (0.06, 0.06), (0.0, 0.06)], seg=12)
            side_box(p, 'Paint#plate', a, b, 1.62, 1.66, s, 0.14, r=0.015, k=2)                      # o fundo do cesto
            for x in (a + 0.02, (a + b) / 2, b - 0.02):
                side_box(p, 'Paint', x - 0.02, x + 0.02, 1.62, 2.08, s, 0.14, r=0.012, k=2)
            for z in (1.84, 2.02):
                side_box(p, 'Paint', a, b, z, z + 0.035, s, 0.03, r=0.01, k=2, off=0.11)
        # as alças de mão do lado (a ficha: os U pequenos espalhados nas chapas) e os olhais de amarração na cintura
        for (x, z) in ((-0.95, 1.95), (0.15, 1.95), (2.05, 1.75), (-3.3, 1.75)):
            vp.grab_handle(p, 'Metal', side_pt(x - 0.08, z, s, 0.0), side_pt(x + 0.08, z, s, 0.0), en, height=0.045, r=0.011)
        for x in (-2.7, -0.1, 1.9):
            q = Vector(side_pt(x, 1.36, s, 0.0))
            p.torus('Metal#vp', tuple(q + en * 0.02), 'x', 0.035, 0.011, seg=10, rseg=5)
        # a caixa de ferramentas do meio e a caixa comprida da frente: a tampa, as travas, as dobradiças
        side_box(p, 'Paint#plate', -0.62, -0.30, 1.72, 2.05, s, 0.09, r=0.035)
        for z in (1.94,):
            vp.rbox(p, 'Metal', side_pt(-0.46, z, s, 0.1), (0.1, 0.03, 0.04), r=0.01, k=1)
        vp.grab_handle(p, 'Metal', side_pt(-0.53, 2.07, s, 0.04), side_pt(-0.39, 2.07, s, 0.04), en + ev * 0.6,
                       height=0.035, r=0.01)
        side_box(p, 'Paint#plate', 0.72, 1.52, 1.84, 2.08, s, 0.08, r=0.035)
        side_plate(p, 'Dark', 0.72, 1.52, 2.0, 2.008, s, off=0.08, t=0.004)
        for x in (0.9, 1.34):
            vp.rbox(p, 'Metal', side_pt(x, 1.93, s, 0.088), (0.07, 0.03, 0.05), r=0.01, k=1)
        # o arco das rodas: o forro escuro com o barro, a suspensão de cada roda (a mola, o braço, o amortecedor)
        fl = lambda x: [(x, 0.76 * s, 0.60), (x, 1.07 * s, 1.09), (x, 1.10 * s, 1.09), (x, 0.79 * s, 0.60)]
        p.loft('Dark', [fl(AXLES_X[3] - 0.60), fl(AXLES_X[0] + 0.60)], bevel=0.0)
        yo = 1.09 * s
        p.prism_xz('Paint#plate', [(-0.30, 1.07), (0.30, 1.07), (0.18, 0.78), (-0.18, 0.78)], min(yo, yo + 0.04 * s), max(yo, yo + 0.04 * s))
        vp.bolt_row(p, 'Metal', (-0.2, yo + 0.04 * s, 1.02), (0.2, yo + 0.04 * s, 1.02), 4, (0, s, 0), r=0.016, h=0.01)
        for x in AXLES_X:
            # a mola helicoidal (o perfil em zigue-zague girado) e o amortecedor inclinado
            prof = [(0.0, 0.0), (0.1, 0.0)] + [(0.1 if k % 2 else 0.075, 0.02 + k * 0.025) for k in range(10)] + [(0.1, 0.26), (0.0, 0.26)]
            vp.lathe(p, 'Dark', (x, 0.93 * s, 0.85), 'z', prof, seg=12)
            y0, y1 = sorted((0.70 * s, (WHEEL_Y - WHEEL_W / 2 - 0.01) * s))
            vp.rbox(p, 'Dark', (x, (y0 + y1) / 2, WHEEL_R), (0.18, y1 - y0, 0.1), r=0.03, k=2)
            tube(p, 'Metal', (x - 0.31, 0.96 * s, 0.62), (x - 0.17, 0.96 * s, 1.02), 0.035, seg=10)
            tube(p, 'Dark', (x - 0.33, 0.96 * s, 0.56), (x - 0.28, 0.96 * s, 0.72), 0.045, seg=10)
        for (xa, xb) in ((AXLES_X[1], AXLES_X[0]), (AXLES_X[3], AXLES_X[2])):
            y0, y1 = sorted((0.66 * s, 0.98 * s))
            vp.rbox(p, 'Dark', ((xa + xb) / 2, (y0 + y1) / 2, 0.71), (xb - xa + 0.32, y1 - y0, 0.18), r=0.04, k=2)
            vp.rbox(p, 'Dark', ((xa + xb) / 2, (y0 + y1) / 2, 0.89), (0.4, y1 - y0, 0.18), r=0.04, k=2)
        # as manilhas de reboque na cintura da frente
        q = Vector((NOSE_X0 - 0.17, CHINE[0] * s, 1.22))
        vp.rbox(p, 'Metal', q, (0.16, 0.06, 0.1), r=0.02, k=2)
        p.torus('Dark#vp', tuple(q + Vector((0, 0.04 * s, -0.07))), 'y', 0.045, 0.014, seg=12, rseg=6)
    # ---------------------------------------------------------------- o nariz: as chapas do glacis (a escotilha grande com as dobradiças e
    # a alça, a chapa da direita), os parafusos, os faróis nas caixas, as luzes de comboio, os ganchos
    gl_plate(p, 'Dark', 2.70, 3.36, -1.10, -0.10, off=0.0, t=0.012)
    gl_plate(p, 'Paint#plate', 2.73, 3.33, -1.07, -0.13, off=0.0, t=0.04)
    gl_plate(p, 'Paint#thin', 2.82, 3.24, -0.96, -0.24, off=0.04, t=0.012)
    gl_plate(p, 'Dark', 2.78, 3.26, 0.22, 0.98, off=0.0, t=0.012)
    gl_plate(p, 'Paint#plate', 2.81, 3.23, 0.25, 0.95, off=0.0, t=0.034)
    gn = gl_normal()
    for (xa, xb) in ((2.86, 2.98), (3.11, 3.23)):                       # as dobradiças de nós no lado de fora da chapa grande
        a_, b_ = Vector(gl_pt(xa, -1.12, 0.03)), Vector(gl_pt(xb, -1.12, 0.03))
        vp.hinge(p, 'Metal', (a_ + b_) / 2, (b_ - a_).normalized(), (b_ - a_).length, r=0.026, knuckles=3)
    a_, b_ = Vector(gl_pt(3.0, -0.3, 0.052)), Vector(gl_pt(3.0, -0.12, 0.052))
    vp.grab_handle(p, 'Metal', a_ - Vector((0, 0.1, 0)), b_ - Vector((0, 0.1, 0)), gn, height=0.045, r=0.012)
    for (x0, x1, y0, y1) in ((2.70, 3.36, -1.10, -0.10), (2.78, 3.26, 0.22, 0.98)):
        for y in (y0 + 0.05, y1 - 0.05):
            vp.bolt_row(p, 'Metal', gl_pt(x0 + 0.06, y, 0.012), gl_pt(x1 - 0.06, y, 0.012), 6, gn, r=0.016, h=0.01)
    for s in (-1, 1):
        fy = 1.03 * s
        lx = FRONT - 0.20 + nose_dx(FRONT - 0.20, fy)
        vp.rbox_q(p, 'Paint#plate', (lx - 0.12, fy, 1.69), (0.24, 0.28, 0.34), r=0.05, k=4)
        vp.lathe(p, 'Dark', (lx + 0.0, fy, 1.72), 'x', [(0.0, -0.01), (0.1, -0.01), (0.105, 0.01), (0.1, 0.04), (0.0, 0.04)], seg=18)
        vp.lathe(p, 'Metal', (lx + 0.03, fy, 1.72), 'x', [(0.08, 0.0), (0.09, 0.0), (0.09, 0.016), (0.08, 0.02)], seg=18, closed=True)
        vp.lathe(p, 'Lens', (lx + 0.03, fy, 1.72), 'x', [(0.0, 0.012), (0.075, 0.01), (0.08, 0.0), (0.0, 0.0)], seg=16)
        vp.lathe(p, 'Light', (lx + 0.03, fy, 1.72), 'x', [(0.0, 0.016), (0.045, 0.014), (0.05, 0.012), (0.0, 0.012)], seg=12)
        vp.rbox(p, 'Paint#plate', (lx + 0.02, fy, 1.875), (0.12, 0.31, 0.03), pitch=-8, r=0.02, k=2)       # a pala
        for k in (-1, 0, 1):                                                                           # a grade de barras na frente
            tube(p, 'Metal', (lx + 0.07, fy + k * 0.06, 1.6), (lx + 0.07, fy + k * 0.06, 1.85), 0.009, seg=5)
        tube(p, 'Metal', (lx - 0.0, fy - 0.12, 1.85), (lx + 0.07, fy - 0.12, 1.85), 0.01, seg=5)
        tube(p, 'Metal', (lx - 0.0, fy + 0.12, 1.85), (lx + 0.07, fy + 0.12, 1.85), 0.01, seg=5)
        tube(p, 'Metal', (lx + 0.07, fy - 0.12, 1.6), (lx + 0.07, fy + 0.12, 1.6), 0.01, seg=5)
        tube(p, 'Metal', (lx + 0.07, fy - 0.12, 1.85), (lx + 0.07, fy + 0.12, 1.85), 0.01, seg=5)
        # as luzes de comboio no bico (a fenda escura com o vidro) e os ganchos na chapa de baixo
        yaw = math.degrees(math.atan2(NOSE_V, TIP_W)) * s
        bx = FRONT + nose_dx(FRONT, fy) + 0.002
        vp.rbox(p, 'Dark', (bx, fy, 1.27), (0.024, 0.34, 0.08), yaw=yaw, r=0.02, k=2)
        vp.rbox(p, 'Lens', (bx + 0.012, fy, 1.27), (0.006, 0.28, 0.04), yaw=yaw, r=0.01, k=1)
        hx = FRONT - 0.30
        hz = low_nose_z(hx, 0.55)
        vp.rbox(p, 'Metal', (hx + nose_dx(hx, 0.55) + 0.03, 0.55 * s, hz - 0.02), (0.1, 0.07, 0.16), pitch=-38, r=0.02, k=2)
        p.torus('Dark#vp', (hx + nose_dx(hx, 0.55) + 0.1, 0.55 * s, hz - 0.08), 'y', 0.05, 0.016, seg=12, rseg=6)
    # ---------------------------------------------------------------- a traseira: a rampa com a porta embutida, o visor, a alavanca, as
    # dobradiças; as lanternas redondas na guarda; os blocos dos para-lamas com o degrau; o engate
    vp.rbox_q(p, 'Dark', (REAR - 0.01, 0.0, 1.41), (1.34, 1.52, 0.02), pitch=-90, r=0.06, k=3)
    vp.rbox_q(p, 'Paint#plate', (REAR - 0.03, 0.0, 1.41), (1.26, 1.42, 0.05), pitch=-90, r=0.05, k=4)
    for (a, b) in (((REAR - 0.056, -0.3, 0.86), (REAR - 0.056, -0.3, 1.86)), ((REAR - 0.056, 0.3, 0.86), (REAR - 0.056, 0.3, 1.86)),
                   ((REAR - 0.056, -0.3, 1.86), (REAR - 0.056, 0.3, 1.86))):
        vp.panel_seam(p, 'Dark', a, b, (-1, 0, 0), w=0.018, d=0.008)
    for s in (-1, 1):
        vp.bolt_row(p, 'Metal', (REAR - 0.055, 0.64 * s, 0.86), (REAR - 0.055, 0.64 * s, 1.96), 10, (-1, 0, 0), r=0.016, h=0.01)
    vp.bolt_row(p, 'Metal', (REAR - 0.055, -0.6, 2.0), (REAR - 0.055, 0.6, 2.0), 10, (-1, 0, 0), r=0.016, h=0.01)
    vp.rbox(p, 'Paint#plate', (REAR - 0.07, 0.0, 1.84), (0.04, 0.24, 0.17), r=0.02, k=2)
    vp.rbox(p, 'Dark', (REAR - 0.092, 0.0, 1.85), (0.006, 0.16, 0.08), r=0.01, k=1)
    vp.tube(p, 'Metal', vp.bent_bar([(REAR - 0.06, -0.14, 1.38), (REAR - 0.11, -0.14, 1.38), (REAR - 0.11, -0.14, 1.12)], R=0.04, n=3),
            0.016, seg=6)
    vp.lathe(p, 'Metal', (REAR - 0.06, -0.14, 1.38), (-1, 0, 0), [(0.0, 0.0), (0.05, 0.0), (0.05, 0.02), (0.0, 0.025)], seg=10)
    vp.rbox(p, 'Metal', (REAR - 0.07, 0.47, 1.39), (0.04, 0.1, 0.18), r=0.015, k=2)
    for y in (-0.45, 0.45):
        vp.hinge(p, 'Metal', (REAR - 0.04, y, 0.78), 'y', 0.26, r=0.04, knuckles=3)
    for s in (-1, 1):
        ly = 1.22 * s
        vp.lathe(p, 'Dark', (REAR, ly, 1.48), (-1, 0, 0), [(0.0, 0.0), (0.11, 0.0), (0.11, 0.035), (0.095, 0.05), (0.0, 0.05)], seg=18)
        vp.lathe(p, 'Lens', (REAR - 0.045, ly, 1.48), (-1, 0, 0), [(0.0, 0.0), (0.08, 0.0), (0.082, 0.01), (0.0, 0.016)], seg=16)
        vp.tube(p, 'Metal', vp.bent_bar([(REAR, ly - 0.13, 1.36), (REAR - 0.1, ly - 0.13, 1.36), (REAR - 0.1, ly - 0.13, 1.6),
                                         (REAR - 0.1, ly + 0.13, 1.6), (REAR - 0.1, ly + 0.13, 1.36), (REAR, ly + 0.13, 1.36)], R=0.04, n=2),
                0.012, seg=6)
        vp.rbox_q(p, 'Paint#plate', (REAR - 0.04, 1.08 * s, 1.91), (0.26, 0.15, 0.08), pitch=-90, r=0.03, k=3)
        vp.rbox(p, 'Lens', (REAR - 0.082, 1.08 * s, 1.92), (0.006, 0.08, 0.14), r=0.01, k=1)
        y0, y1 = sorted((0.98 * s, 1.54 * s))
        vp.rbox_q(p, 'Paint#plate', (REAR + 0.145, (y0 + y1) / 2, 0.925), (0.35, y1 - y0, 0.57), r=0.05, k=4)
        vp.rbox(p, 'Metal', (REAR - 0.03, (y0 + y1) / 2, 0.62), (0.14, y1 - y0 - 0.08, 0.035), r=0.015, k=2)
        vp.rbox(p, 'Dark', (REAR - 0.03, (y0 + y1) / 2, 0.64), (0.1, y1 - y0 - 0.14, 0.006), r=0.01, k=1)
        vp.bolt_row(p, 'Metal', (REAR - 0.032, y0 + 0.08, 1.15), (REAR - 0.032, y1 - 0.08, 1.15), 4, (-1, 0, 0), r=0.016, h=0.01)
    vp.rbox(p, 'Metal', (REAR - 0.05, 0.0, 0.77), (0.1, 0.24, 0.14), r=0.03, k=2)
    p.torus('Dark#vp', (REAR - 0.12, 0.0, 0.74), 'z', 0.055, 0.018, seg=14, rseg=6)
    vp.poke_ngons(p)
    return p.finish()


# ---------------------------------------------------------------------------------- rodas
def wheels():
    """8 rodas em pares (a ordem do A): o pneu de verdade com os blocos em V (o pneu militar grosso da ficha), a roda funda de aço com
    o anel de porcas e a tampa do CTIS (o cubo com a válvula, a mangueira do enchimento central)."""
    objs = []
    n = 0
    for x in AXLES_X:
        for s in (-1, 1):
            n += 1
            w = Part(f'wheel_{n:03d}')
            c = Vector((x, WHEEL_Y * s, WHEEL_R))
            vp.tyre(w, c, WHEEL_R, WHEEL_W, s, rim_r=0.29, rubber='Rubber', rim='Paint', hub='Metal', seg=36, style='mud', n_lugs=24,
                    lug_h=0.034, nuts=10, dish=0.09)
            o = WHEEL_W / 2 * 0.84 - 0.09 + 0.075
            vp.lathe(w, 'Dark', c + Vector((0, s * o, 0)), (0, s, 0), [(0.0, 0.0), (0.075, 0.0), (0.08, 0.02), (0.06, 0.05), (0.025, 0.06),
                                                                         (0.0, 0.06)], seg=14)
            vp.tube(w, 'Dark', [c + Vector((0.0, s * (o + 0.05), 0.0)), c + Vector((0.12, s * (o + 0.035), 0.08)),
                                c + Vector((0.2, s * (o + 0.0), 0.12))], 0.012, seg=6)
            objs.append(w.finish())
    return objs


# ---------------------------------------------------------------------------------- a estação de armas e o cano
def turret():
    p = Part(PREFIX + '_Turret')
    p.bm('Paint#body', 0.025)
    p.bm('Paint#plate', 0.02)
    p.bm('Paint', 0.02)
    p.bm('Team', 0.005)
    p.bm('Dark', 0.008)
    px, py = PIVOT
    X = lambda dx: px + dx
    z0 = ROOF_Z + RING_H + 0.006
    # a base em 2 degraus (a chapa larga e baixa e o degrau menor), a junta, o anel de parafusos
    vp.lathe(p, 'Paint', (px, py, z0), 'z', [(0.0, 0.0), (BASE_R, 0.0), (BASE_R, 0.05), (BASE_R - 0.02, 0.085), (BASE_R - 0.04, 0.09),
                                             (BASE_R - 0.11, 0.09), (BASE_R - 0.12, 0.1), (BASE_R - 0.12, 0.135), (BASE_R - 0.135, 0.14),
                                             (0.0, 0.14)], seg=48)
    vp.lathe(p, 'Dark', (px, py, z0 + 0.012), 'z', [(BASE_R - 0.004, -0.008), (BASE_R + 0.006, -0.008), (BASE_R + 0.006, 0.008),
                                                    (BASE_R - 0.004, 0.008)], seg=48, closed=True)
    vp.bolt_ring(p, 'Metal', (px, py, z0 + 0.09), 'z', BASE_R - 0.075, 16, r=0.014, h=0.01)
    zt = z0 + 0.14
    zb = zt + 0.26
    # o corpo compacto facetado (a frente e a traseira em chanfro) e a tampa da cor do jogador com as dobradiças
    p.prism_xz('Paint#body', [(X(-0.42), zt), (X(0.24), zt), (X(0.34), zt + 0.1), (X(0.34), zt + 0.17), (X(0.25), zb),
                              (X(-0.40), zb), (X(-0.48), zb - 0.08), (X(-0.48), zt + 0.06)], -0.31, 0.31)
    vp.rbox_q(p, 'Team', (X(-0.29), 0.0, zb + 0.008), (0.26, 0.54, 0.016), r=0.03, k=3)
    for y in (-0.18, 0.18):
        vp.hinge(p, 'Metal', (X(-0.43), y, zb + 0.012), 'y', 0.09, r=0.016, knuckles=2)
    for s in (-1, 1):                                                     # as juntas e os parafusos das chapas do corpo
        vp.bolt_row(p, 'Metal', (X(-0.38), 0.312 * s, zt + 0.04), (X(0.2), 0.312 * s, zt + 0.04), 6, (0, s, 0), r=0.014, h=0.008)
    # o berço: as duas chapas que seguram a arma e o munhão
    for sy in (-1, 1):
        y0, y1 = sorted((0.085 * sy, 0.125 * sy))
        p.prism_xz('Paint#plate', [(X(-0.14), zb), (X(0.26), zb), (X(0.26), GUN_Z - 0.03), (X(0.16), GUN_Z + 0.05), (X(-0.14), GUN_Z + 0.05)],
                   y0, y1)
    vp.lathe(p, 'Dark', (X(0.05), 0.0, GUN_Z - 0.04), 'y', [(0.0, -0.16), (0.04, -0.16), (0.05, -0.14), (0.05, 0.14), (0.04, 0.16),
                                                           (0.0, 0.16)], seg=14)
    # a caixa da .50 (sem o cano): o corpo, a tampa do alimentador com a dobradiça, as chapas de lado com os rebites, a alça de troca
    vp.rbox(p, 'Dark', (X(-0.32 + (GUN_X0 + 0.32) / 2), 0.0, GUN_Z - 0.0075), (GUN_X0 + 0.32, 0.14, 0.135), r=0.02, k=2)
    vp.rbox(p, 'Dark', (X(0.05), 0.0, GUN_Z + 0.075), (0.34, 0.12, 0.03), r=0.012, k=2)
    vp.hinge(p, 'Metal', (X(-0.13), 0.0, GUN_Z + 0.085), 'y', 0.1, r=0.012, knuckles=2)
    for s in (-1, 1):
        vp.bolt_row(p, 'Metal', (X(-0.28), 0.071 * s, GUN_Z + 0.03), (X(0.3), 0.071 * s, GUN_Z + 0.03), 5, (0, s, 0), r=0.01, h=0.006)
    vp.tube(p, 'Metal', vp.bent_bar([(X(0.28), 0.0, GUN_Z + 0.06), (X(0.28), 0.0, GUN_Z + 0.11), (X(0.36), 0.0, GUN_Z + 0.11),
                                     (X(0.36), 0.0, GUN_Z + 0.06)], R=0.02, n=2), 0.012, seg=6)
    # (3ª volta, a ficha de frente: a caixa do sensor com a lente grande fica à DIREITA do cano e a lata de munição à esquerda; o A tinha
    # espelhado) o cubo do sensor: a caixa, a janela da lente na moldura, a câmera, a pala, a faixa da cor do jogador
    sx0, sx1, sy0, sy1, sz0, sz1 = X(-0.14), X(0.24), 0.15, 0.50, zb - 0.04, GUN_Z + 0.13
    vp.rbox_q(p, 'Paint#plate', ((sx0 + sx1) / 2, (sy0 + sy1) / 2, (sz0 + sz1) / 2), (sx1 - sx0, sy1 - sy0, sz1 - sz0), r=0.04, k=4)
    vp.rbox(p, 'Dark', (X(0.04), 0.135, GUN_Z - 0.04), (0.12, 0.03, 0.1), r=0.01, k=1)
    vp.rbox_q(p, 'Dark', (sx1 + 0.006, 0.36, GUN_Z), (0.21, 0.22, 0.014), pitch=-90, r=0.03, k=3)
    vp.rbox(p, 'Lens', (sx1 + 0.014, 0.36, GUN_Z), (0.006, 0.18, 0.16), r=0.025, k=2)
    vp.lathe(p, 'Dark', (sx1, 0.20, GUN_Z + 0.06), 'x', [(0.0, 0.0), (0.04, 0.0), (0.042, 0.02), (0.035, 0.03), (0.0, 0.03)], seg=14)
    vp.lathe(p, 'Lens', (sx1 + 0.03, 0.20, GUN_Z + 0.06), 'x', [(0.0, 0.0), (0.028, 0.0), (0.0, 0.004)], seg=12)
    vp.rbox(p, 'Paint#plate', (sx1 + 0.005, (sy0 + sy1) / 2, sz1 - 0.01), (0.13, sy1 - sy0 + 0.02, 0.02), pitch=-6, r=0.015, k=2)
    vp.rbox(p, 'Team', (X(0.0), (sy0 + sy1) / 2, sz1 + 0.006), (0.12, sy1 - sy0 + 0.004, 0.012), r=0.006, k=1)
    vp.rbox(p, 'Team', (X(0.0), sy1 + 0.005, (sz0 + 0.08 + sz1) / 2), (0.12, 0.012, sz1 - sz0 - 0.08), r=0.006, k=1)
    # a lata de munição à esquerda (a tampa com as travas e a alça) e a calha articulada até a arma
    vp.ammo_box(p, (X(-0.03), -0.275, (zb - 0.02 + GUN_Z + 0.04) / 2), size=(0.34, 0.25, GUN_Z + 0.06 - zb), m='Paint', latch_m='Dark')
    vp.chute(p, 'Dark', [(X(-0.02), -0.16, GUN_Z + 0.02), (X(-0.01), -0.11, GUN_Z + 0.01), (X(0.0), -0.075, GUN_Z + 0.0)], 0.07, 0.04,
             pitch=0.035)
    vp.poke_ngons(p)
    return p.finish()


def barrel():
    """O cano da .50 (aponta pra +X em repouso; a boca é o X máximo): a luva furada na saída da caixa, o cano fino, a mira e o
    quebra-chamas com as fendas."""
    p = Part(PREFIX + '_Barrel')
    x0 = PIVOT[0] + GUN_X0
    x1 = PIVOT[0] + MUZZLE
    C = Vector((x0, 0.0, GUN_Z))
    vp.lathe(p, 'Dark', C, 'x', [(0.0, 0.0), (0.06, 0.0), (0.063, 0.01), (0.063, 0.25), (0.055, 0.26), (0.0, 0.26)], seg=16)
    for k in range(4):                                                # os furos da luva (as fendas escuras em anel)
        for j in range(6):
            a = 2 * math.pi * j / 6 + (0.5 if k % 2 else 0.0)
            vp.rbox(p, 'Metal', (x0 + 0.05 + k * 0.05, 0.064 * math.cos(a), GUN_Z + 0.064 * math.sin(a)), (0.026, 0.012, 0.004),
                    roll=math.degrees(a) - 90, r=0.003, k=1)
    L = (x1 - 0.13) - (x0 + 0.26)
    vp.lathe(p, 'Dark', C + Vector((0.26, 0, 0)), 'x', [(0.0, 0.0), (0.04, 0.0), (0.036, 0.04), (0.034, L), (0.0, L)], seg=12)
    vp.rbox(p, 'Dark', (x1 - 0.2, 0.0, GUN_Z + 0.045), (0.03, 0.012, 0.03), r=0.004, k=1)
    vp.lathe(p, 'Metal', (x1 - 0.13, 0.0, GUN_Z), 'x', [(0.0, 0.0), (0.045, 0.0), (0.052, 0.02), (0.052, 0.12), (0.045, 0.13), (0.026, 0.13),
                                                       (0.026, 0.125), (0.0, 0.125)], seg=14)
    for j in range(4):
        a = 2 * math.pi * j / 4 + math.pi / 4
        vp.rbox(p, 'Dark', (x1 - 0.06, 0.053 * math.cos(a), GUN_Z + 0.053 * math.sin(a)), (0.07, 0.016, 0.004), roll=math.degrees(a) - 90,
                r=0.004, k=1)
    return p.finish()


def sweep_check(h, t, b):
    """A estação girando não pode atravessar o casco: colisão de verdade (BVH, triângulo contra triângulo) a cada 5° (a do A)."""
    def tree(o, M):
        vs = [M @ v.co for v in o.data.vertices]
        return BVHTree.FromPolygons(vs, [tuple(q.vertices) for q in o.data.polygons])
    hull_t = tree(h, Matrix.Identity(4))
    piv = V(PIVOT[0], PIVOT[1], 0.0)
    hits = []
    for k in range(72):
        Y = Matrix.Translation(piv) @ Matrix.Rotation(math.radians(5 * k), 4, 'Z') @ Matrix.Translation(-piv)
        pairs = [('T', q) for q in hull_t.overlap(tree(t, Y))] + [('B', q) for q in hull_t.overlap(tree(b, Y))]
        if pairs:
            cs = [h.data.polygons[i].center for _, (i, j) in pairs]
            box = tuple(round(f(c[ax] * (-1 if ax == 1 else 1) for c in cs), 2) for ax in range(3) for f in (min, max))
            hits.append((5 * k, len(pairs), ''.join(sorted(set(w for w, _ in pairs))), box))
    print('MODEL giro da estacao: %s' % ('sem colisao em 360 graus' if not hits else 'COLISAO em %d posicoes' % len(hits)))
    for hit in hits[:12]:
        print('MODEL   ', hit)


def bounds(objs):
    xs, ys, zs = [], [], []
    for o in objs:
        for v in o.data.vertices:
            xs.append(v.co.x)
            ys.append(-v.co.y)
            zs.append(v.co.z)
    return (min(xs), max(xs)), (min(ys), max(ys)), (min(zs), max(zs))


def write_spec():
    import json
    parts = [[PREFIX + n, [0, 0, 0]] for n in ('_Hull', '_Wheels', '_Turret', '_Barrel')]
    views = [['front', 'ortho', [-1, 0, 0], [0, 0, 1], [900, 900]],
             ['back', 'ortho', [1, 0, 0], [0, 0, 1], [900, 900]],
             ['left', 'ortho', [0, -1, 0], [0, 0, 1], [1800, 820]],
             ['right', 'ortho', [0, 1, 0], [0, 0, 1], [1800, 820]],
             ['top', 'ortho', [0, 0, -1], [0, 1, 0], [1800, 1000]],
             ['three_quarter', 'persp', 27.0, 232.0, [1600, 1000], 30.0, 0.8],
             ['three_quarter_right', 'persp', 30.0, 40.0, [1600, 1000], 30.0, 0.8],
             ['ficha_34', 'persp', 26.0, 220.0, [1600, 1000], 30.0, 0.8]]
    game = [['game_cam', 37.5, 35.0, 50.0, 0.8], ['game_cam_close', 37.5, 35.0, 28.0, 0.75]]
    spec = {'parts': parts, 'views': views, 'usa_at': [0, 6.0, 0], 'tank_views': False, 'game': game}
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)


def main():
    clear_scene()
    h = hull()
    ws = wheels()
    t = turret()
    b = barrel()
    sweep_check(h, t, b)
    pre = {h.name: tris(h), 'wheels': sum(tris(w) for w in ws), t.name: tris(t), b.name: tris(b)}
    for o in [h, t, b] + ws:
        o.data.transform(Matrix.Scale(SCALE, 4))
    total = export(h, OUT)
    total += export_group(ws, OUT, PREFIX + '_Wheels')
    total += export(t, OUT)
    total += export(b, OUT)
    print('MODEL total', total, 'tris (antes da grade do executor); por peca', pre)
    k = SCALE
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h, t, b] + ws)
    print('MODEL medidas (malha): comprimento %.3f m, largura %.3f m, altura total %.3f m' % (ax1 - ax0, ay1 - ay0, az1))
    print('MODEL turret_pivot_m (malha)', [round(PIVOT[0] * k, 4), 0.0], '(jogo x1,25)', [round(PIVOT[0] * k * 1.25, 3), 0.0])
    (bx0, bx1), _, _ = bounds([b])
    print('MODEL boca da .50: x %.4f (malha), GUN_Z %.4f (malha) -> muzzle_height_m %.3f (jogo)' % (bx1, GUN_Z * k, GUN_Z * k * 1.25))
    print('MODEL pneu: raio %.4f, largura %.4f (tyre_w_m jogo %.3f); eixos %s; y +-%.4f (malha)'
          % (WHEEL_R * k, WHEEL_W * k, WHEEL_W * k * 1.25, [round(x * k, 3) for x in AXLES_X], WHEEL_Y * k))
    write_spec()


main()
