"""O Dozer da USA (usa_dozer), VERSÃO B no detalhe AAA (01/11/2026): a versão A (Game/Tools/model_usa_dozer.py, aprovada em 13/10:
"gostei do trator") fica como está; esta é a que ele compara com ela.

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_usa_dozer_v2.py C:/Users/luisf/explosion/captures/game/models/usa_dozer_v2

A referência é a arte aprovada docs/concept/units/usa_dozer.webp (não tem 5 vistas: compara-se no ângulo dela, 3/4 pela
frente-esquerda) e o porte do Caterpillar D9 blindado (~8 m da lâmina ao escarificador, a motriz alta que faz a esteira em triângulo,
a lâmina de 4,3 m × 1,9 m). O layout é o do A aprovado (já na escala final: o A era desenhado × 0,88), refeito peça por peça:
  - a esteira de elos com a garra única alta de trator (model_vehicle_parts.track_links), a motriz alta dentada, as 2 rodas-guia,
    os roletes de flange dupla; a saia blindada rebitada por cima dos roletes (a arte) com a faixa azul segmentada;
  - o casco em chapas de quina redonda com as juntas e os parafusos (a linguagem do tanque USA), o nariz blindado com a grade, a
    cabine blindada com o para-brisa de 2 vidros inclinado, a caixa do sensor, a antena, a sirene na gaiola (pedido dele);
  - o capô do motor com as venezianas dos lados, a tomada de ar, as 2 chaminés com a capa de calor e o cotovelo;
  - o deck de trás com a LONA AZUL dobrada nas cintas e o corrimão amarelo de tubo dobrado; o braço robótico amarelo dobrado (os
    segmentos de quina redonda, as juntas torneadas, os cilindros com a haste e a garra); o escarificador (o dente curvo maciço);
  - a lâmina: a concha côncava em perfil (o centro reto e as asas viradas pra frente), o fio de corte com os parafusos, o lábio de
    cima, as nervuras atrás, os braços de empurrar por fora das esteiras com os munhões, e os cilindros de levantar.

Pivôs MANTIDOS (Units.json usa_dozer): blade.at_m (−0,352; 0; 0,66) = a origem da malha da lâmina; spin[0].at_m (0,396; 0,264;
2,86) = a origem da luz da sirene; tracks_m (2,56; 0,62): a esteira do B fica na bitola do Units.json (a malha do A tinha 2,82).

Peças (origem no chão, no centro do casco, frente em +X; os NOMES do A):
  SM_USA_Dozer_Hull, SM_USA_Dozer_Blade (origem no pivô), SM_USA_Dozer_Beacon (origem no eixo), SM_USA_Dozer_Wheels (o executor
  separa em wheel_*), SM_USA_Dozer_Track_l / _Track_r.
Slots (os do A): Paint, Team, Yellow, Dark, Metal, Track, Lens, Beacon, Light.
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
from model_china_vehicle_kit import bounds, frame_box, solid, tris  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_dozer_v2')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_USA_Dozer'

COLORS.update({'Paint': (196, 198, 192), 'Team': (86, 108, 150), 'Yellow': (214, 164, 32), 'Dark': (40, 40, 38),
               'Metal': (95, 95, 92), 'Track': (55, 52, 48), 'Lens': (30, 60, 110), 'Beacon': (255, 170, 40), 'Light': (255, 230, 170)})

# ---------------------------------------------------------------------------------- medidas (m, escala final)
BLADE_PIVOT = (-0.352, 0.0, 0.66)      # blade.at_m (mantido)
BEACON = (0.396, 0.264, 2.86)          # spin.at_m (mantido)
TRACK_Y, TRACK_W = 1.28, 0.62          # tracks_m (2,56; 0,62)
SPROCKET = (-1.01, 1.10, 0.50)         # a motriz alta (a esteira em triângulo do D9)
IDLER_F = (1.54, 0.50, 0.37)
IDLER_R = (-1.72, 0.46, 0.33)
ROLLERS_X = [1.02, 0.5, -0.02, -0.54, -1.06]
ROLLER_Z, ROLLER_R = 0.325, 0.21
DECK = 1.80
NOSE, TAIL = 2.51, -2.51
BODY_HW = 0.95                          # o corpo entre as esteiras
SPON_HW = 1.66                          # os para-lamas por cima das esteiras
CAB = (0.18, 1.89, -1.01, 0.48, 2.55)   # a cabine: x0, x1, y0, y1, a altura do teto
ENG = (-1.50, 0.18, 2.42)               # o capô do motor: x0, x1, o topo
ARM_Y = 1.86                            # os braços de empurrar da lâmina, por fora das esteiras e da saia


def deck_z(x):
    return DECK


# ---------------------------------------------------------------------------------- utilidades
def cylinder(p, a, b, r_body, r_rod, body_m='Yellow', rod_m='Metal', frac=0.55, seg=16):
    """O cilindro hidráulico de a até b: o corpo torneado com as tampas e a haste cromada saindo, os olhais das pontas."""
    a, b = Vector(a), Vector(b)
    d = b - a
    L = d.length
    u = d.normalized()
    Lb = L * frac
    vp.lathe(p, body_m, a, u, [(0.0, 0.0), (r_body * 0.8, 0.0), (r_body, 0.03), (r_body, Lb - 0.05), (r_body * 1.12, Lb - 0.04),
                               (r_body * 1.12, Lb), (r_rod * 1.3, Lb + 0.01), (0.0, Lb + 0.01)], seg=seg)
    vp.lathe(p, rod_m, a, u, [(0.0, Lb - 0.02), (r_rod, Lb - 0.02), (r_rod, L - 0.04), (0.0, L - 0.04)], seg=max(10, seg - 4))
    for q, uu in ((a, -u), (b, u)):
        _, e1, _ = vp.axis_frame(u)
        vp.lathe(p, body_m if q is a else rod_m, q, e1, [(0.0, -0.035), (r_body * 0.9, -0.035), (r_body * 0.9, 0.035), (0.0, 0.035)], seg=12)


def boom(p, m, a, b, w, h, side=(0, 1, 0), r=0.06):
    """O segmento de braço (a viga de quina redonda) de a até b, largura w (ao longo de 'side'), altura h."""
    a, b = Vector(a), Vector(b)
    d = b - a
    ex = d.normalized()
    ey = Vector(side) - ex * Vector(side).dot(ex)
    ey.normalize()
    ez = ex.cross(ey)
    C = (a + b) / 2
    vp.prism(p, m, vp.rounded_rect(d.length, h, r, 4), C - ey * (w / 2), ex, ez, ey, w)


def plate_xz(p, m, x0, x1, z0, z1, y, t, r=0.05):
    vp.rbox(p, m, ((x0 + x1) / 2, y + t / 2, (z0 + z1) / 2), (x1 - x0, z1 - z0, abs(t)), roll=90, r=r, k=4)


def louvers(p, c, n_face, along, L, H, n, slat='Paint#thin', tilt=35.0):
    """A veneziana numa face (c = o centro na face, n_face = a normal): o fundo escuro, a moldura e as aletas inclinadas."""
    eu, ev, en = vp.face_frame(n_face, along)
    C = Vector(c)
    vp.prism(p, 'Dark#vp', vp.rounded_rect(L + 0.02, H + 0.02, 0.03, 2), C - en * 0.004, eu, ev, en, 0.01)
    fw = 0.045
    for sgn in (-1, 1):
        frame_box(p, 'Paint#plate', C + ev * (sgn * (H / 2 + fw / 2)) + en * 0.02, eu, ev, en, L + 2 * fw, fw, 0.04)
        frame_box(p, 'Paint#plate', C + eu * (sgn * (L / 2 + fw / 2)) + en * 0.02, eu, ev, en, fw, H, 0.04)
    step = H / n
    a = math.radians(tilt)
    e2 = ev * math.cos(a) + en * math.sin(a)
    e3 = en * math.cos(a) - ev * math.sin(a)
    for k in range(n):
        frame_box(p, slat, C + ev * (-H / 2 + step * (k + 0.5)) + en * 0.02, eu, e2, e3, L, step * 0.8, 0.012)


# ---------------------------------------------------------------------------------- casco
def hull():
    p = Part(PREFIX + '_Hull')
    p.bm('Paint#shell', 0.07)
    p.bm('Paint#plate', 0.03)
    p.bm('Paint#thin', 0.008)
    p.bm('Dark#plate', 0.02)
    p.bm('Yellow#plate', 0.03)
    p.bm('Lens', 0.0)
    # ---------------------------------------------------------------- o corpo entre as esteiras e os para-lamas por cima delas
    solid(p, 'Dark#plate', [(x, s * BODY_HW, z) for x, z in ((2.2, 0.42), (2.45, 0.9), (-2.3, 0.9), (-2.1, 0.42)) for s in (-1, 1)])
    body = [(NOSE, 1.0), (NOSE, 1.45), (2.15, DECK), (-2.42, DECK), (TAIL, 1.65), (TAIL, 1.0)]
    solid(p, 'Paint#shell', [(x, s * BODY_HW, z) for x, z in body for s in (-1, 1)])
    for s in (-1, 1):
        y0, y1 = sorted((BODY_HW * s, SPON_HW * s))
        # o para-lama: a frente em cunha por cima da roda-guia, o deck, o recorte por cima da motriz alta
        fr = [(2.48, 1.36), (2.2, DECK), (-0.38, DECK), (-0.38, 1.36)]
        solid(p, 'Paint#shell', [(x, yy, z) for x, z in fr for yy in (y0, y1)])
        rr = [(-1.62, DECK), (-2.42, DECK), (TAIL, 1.65), (TAIL, 1.36), (-1.9, 1.36)]
        solid(p, 'Paint#shell', [(x, yy, z) for x, z in rr for yy in (y0, y1)])
        vp.rbox(p, 'Paint#plate', (-1.0, (BODY_HW + SPON_HW) / 2 * s, DECK - 0.04), (1.26, SPON_HW - BODY_HW, 0.08), r=0.05, k=4)
        # a caixa blindada da frente do para-lama (a arte: o bloco com a faixa azul em pé) e as juntas
        vp.rbox(p, 'Paint#plate', (1.62, (SPON_HW - 0.02) * s, 1.6), (1.1, 0.12, 0.42), r=0.06, k=4)
        vp.rbox(p, 'Team', (1.5, (SPON_HW + 0.042) * s, 1.6), (0.14, 0.008, 0.36), r=0.02, k=2)
        vp.bolt_row(p, 'Metal', (1.15, (SPON_HW + 0.04) * s, 1.76), (2.08, (SPON_HW + 0.04) * s, 1.76), 7, (0, s, 0), r=0.018, h=0.012)
        for x in (0.55, -0.38, -1.62):
            vp.panel_seam(p, 'Dark', (x, BODY_HW * s, DECK + 0.002), (x, SPON_HW * s, DECK + 0.002), (0, 0, 1), w=0.024, d=0.012)
        # ---------------------------------------------------------------- a saia blindada por cima dos roletes (a arte), em 3 chapas
        # com a fresta e os parafusos, a faixa azul segmentada
        yo = (TRACK_Y + TRACK_W / 2 + 0.04) * s
        for i, (xa, xb) in enumerate(((0.62, 1.56), (-0.36, 0.58), (-1.34, -0.4))):
            plate_xz(p, 'Paint#plate', xa, xb, 0.34, 0.86, yo if s > 0 else yo - 0.1, 0.1, r=0.06)
            yb = yo + 0.1 * s if s > 0 else yo - 0.1
            vp.bolt_row(p, 'Metal', (xa + 0.08, yb, 0.79), (xb - 0.08, yb, 0.79), 5, (0, s, 0), r=0.02, h=0.014)
            vp.bolt_row(p, 'Metal', (xa + 0.08, yb, 0.41), (xb - 0.08, yb, 0.41), 5, (0, s, 0), r=0.02, h=0.014)
            if i < 2:
                plate_xz(p, 'Team', xa + 0.1, xb - 0.1, 0.56, 0.68, yb if s > 0 else yb - 0.006, 0.006, r=0.02)
        # o quadro dos roletes (a viga escura atrás da saia) e os suportes até o corpo
        vp.rbox(p, 'Dark#plate', (-0.08, TRACK_Y * s, 0.62), (3.0, 0.3, 0.2), r=0.04, k=2)
        for x in (0.75, -0.75):
            vp.rbox(p, 'Dark#plate', (x, 1.055 * s, 0.7), (0.3, 0.25, 0.22), r=0.04, k=2)
        # o munhão da lâmina no quadro (o pivô dos braços de empurrar: a lâmina gira em volta dele)
        vp.lathe(p, 'Metal', (BLADE_PIVOT[0], (TRACK_Y + TRACK_W / 2) * s, BLADE_PIVOT[2]), (0, s, 0),
                 [(0.0, 0.0), (0.16, 0.0), (0.16, 0.12), (0.12, 0.17), (0.0, 0.17)], seg=18)
    # ---------------------------------------------------------------- o nariz blindado: a grade do radiador, os faróis, os ganchos
    vp.rbox(p, 'Paint#plate', (NOSE + 0.03, 0.0, 1.2), (0.1, 1.86, 0.62), r=0.07, k=4)
    louvers(p, (NOSE + 0.08, 0.0, 1.22), (1, 0, 0), (0, 1, 0), 1.0, 0.44, 7, tilt=-30)
    for s in (-1, 1):
        vp.lathe(p, 'Metal', (NOSE + 0.03, 0.78 * s, 1.24), 'x', [(0.0, -0.04), (0.07, -0.04), (0.085, -0.02), (0.085, 0.04),
                                                                 (0.07, 0.05), (0.0, 0.05)], seg=14)
        vp.lathe(p, 'Light', (NOSE + 0.03, 0.78 * s, 1.24), 'x', [(0.0, 0.056), (0.06, 0.054), (0.07, 0.05), (0.0, 0.05)], seg=12)
        vp.tube(p, 'Dark', vp.bent_bar([(NOSE + 0.04, 0.66 * s, 1.1), (NOSE + 0.11, 0.66 * s, 1.1), (NOSE + 0.11, 0.66 * s, 1.38),
                                        (NOSE + 0.11, 0.9 * s, 1.38), (NOSE + 0.11, 0.9 * s, 1.1), (NOSE + 0.04, 0.9 * s, 1.1)], R=0.06, n=3),
                0.016, seg=6)
        for d in (-1, 1):
            vp.rbox(p, 'Metal', (NOSE + 0.07, 0.45 * s + d * 0.05, 0.86), (0.12, 0.035, 0.14), r=0.03, k=2)
        p.cyl('Metal#vp', (NOSE + 0.1, 0.45 * s, 0.88), 'y', 0.022, 0.15, seg=10, bevel=0.0)
    vp.weld_bead(p, 'Paint#thin', [(NOSE - 0.02, -0.9, 1.47), (NOSE - 0.02, 0.9, 1.47)], r=0.016)
    # ---------------------------------------------------------------- a cabine blindada (à esquerda da frente)
    cx0, cx1, cy0, cy1, ch = CAB
    pts = []
    for y in (cy0, cy1):
        pts += [(cx0, y, DECK), (cx1, y, DECK), (cx1, y, 2.18), (cx1 - 0.32, y, ch), (cx0, y, ch)]
    # (os cantos de cima chanfrados em planta: a cabine blindada da arte, facetada)
    pts += [(cx1 - 0.4, cy0 + 0.1, ch + 0.06), (cx0 + 0.08, cy0 + 0.1, ch + 0.06), (cx1 - 0.4, cy1 - 0.1, ch + 0.06), (cx0 + 0.08, cy1 - 0.1, ch + 0.06)]
    solid(p, 'Paint#shell', pts)
    # o para-brisa de 2 vidros na face inclinada (de 2,18 a ch) com a moldura, a janela de lado
    ang = math.degrees(math.atan2(ch - 2.18, 0.32))
    for (ya, yb) in ((cy0 + 0.12, (cy0 + cy1) / 2 - 0.04), ((cy0 + cy1) / 2 + 0.04, cy1 - 0.12)):
        c = Vector((cx1 - 0.16 + 0.012, (ya + yb) / 2, (2.18 + ch) / 2 + 0.006))
        L = math.hypot(0.32, ch - 2.18)
        ex = Vector((-0.32, 0.0, ch - 2.18)).normalized()
        en = Vector((ch - 2.18, 0.0, 0.32)).normalized()
        vp.prism(p, 'Lens#vp', vp.rounded_rect(L * 0.78, yb - ya, 0.04, 3), c - en * 0.006, ex, Vector((0, 1, 0)), en, 0.014)
        vp.prism(p, 'Dark#vp', vp.rounded_rect(L * 0.84, yb - ya + 0.06, 0.05, 3), c - en * 0.012, ex, Vector((0, 1, 0)), en, 0.01)
    plate_xz(p, 'Lens', cx1 - 0.78, cx1 - 0.44, 2.2, 2.38, cy0 - 0.012, 0.012, r=0.03)          # a janelinha de lado (a arte)
    plate_xz(p, 'Dark', cx1 - 0.81, cx1 - 0.41, 2.17, 2.41, cy0 - 0.008, 0.008, r=0.04)
    vp.rbox(p, 'Paint#plate', (cx1 - 0.14, 0.0 + (cy0 + cy1) / 2, ch + 0.02), (0.18, cy1 - cy0 + 0.08, 0.05), pitch=-8, r=0.04, k=4)  # a viseira
    # a faixa azul em pé no canto da cabine (a arte) e o retalho do lado
    vp.rbox(p, 'Team', (cx1 - 0.45, cy0 - 0.012, 2.3), (0.13, 0.008, 0.42), r=0.02, k=2)
    # o teto: a escotilha, a caixa do sensor com a lente, a caixinha, a antena, as alças
    vp.hatch_round(p, (cx0 + 0.55, -0.35, ch + 0.06), 0.26, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
    vp.lathe(p, 'Paint', (1.18, -0.72, ch + 0.06), 'z', [(0.0, 0.0), (0.16, 0.0), (0.16, 0.04), (0.11, 0.07), (0.1, 0.14), (0.0, 0.14)], seg=18)
    vp.rbox(p, 'Paint#plate', (1.18, -0.72, ch + 0.33), (0.4, 0.36, 0.26), r=0.06, k=4)
    vp.rbox(p, 'Lens', (1.385, -0.72, ch + 0.34), (0.012, 0.22, 0.12), r=0.02, k=2)
    vp.rbox(p, 'Paint#plate', (1.3, 0.15, ch + 0.14), (0.26, 0.22, 0.16), r=0.04, k=4)
    vp.lathe(p, 'Dark', (0.8, 0.12, ch + 0.06), 'z', [(0.0, 0.0), (0.05, 0.0), (0.05, 0.05), (0.035, 0.06), (0.03, 0.15), (0.0, 0.16)], seg=10)
    p.cyl('Dark#vp', (0.8, 0.12, ch + 0.2 + 0.6), 'z', 0.008, 1.2, seg=6, bevel=0.0)
    for (y, xa) in ((cy0 + 0.1, cx0 + 0.2), (cy1 - 0.1, cx0 + 0.7)):
        vp.grab_handle(p, 'Metal', (xa, y, ch + 0.06), (xa + 0.35, y, ch + 0.06), (0, 0, 1), height=0.06, r=0.014)
    # a sirene amarela no canto de trás do teto (pedido dele): o mastro, a base, a cúpula âmbar (Beacon: pisca) e a gaiola
    bx, by, bz = BEACON
    vp.lathe(p, 'Dark', (bx, by, ch + 0.06), 'z', [(0.0, 0.0), (0.06, 0.0), (0.06, 0.03), (0.04, 0.05), (0.035, bz - 0.13 - ch - 0.06),
                                                  (0.0, bz - 0.13 - ch - 0.06)], seg=12)
    vp.lathe(p, 'Dark', (bx, by, bz - 0.15), 'z', [(0.0, 0.0), (0.13, 0.0), (0.135, 0.02), (0.13, 0.05), (0.0, 0.05)], seg=20)
    vp.lathe(p, 'Beacon', (bx, by, bz - 0.1), 'z', [(0.0, 0.0), (0.1, 0.0), (0.1, 0.16), (0.09, 0.2), (0.06, 0.225), (0.0, 0.235)], seg=20)
    vp.lathe(p, 'Dark', (bx, by, bz + 0.13), 'z', [(0.0, 0.0), (0.06, 0.0), (0.06, 0.02), (0.0, 0.025)], seg=12)
    for k in range(4):
        a = math.radians(k * 90 + 45)
        vp.tube(p, 'Metal', [(bx + 0.13 * math.cos(a), by + 0.13 * math.sin(a), bz - 0.1),
                             (bx + 0.13 * math.cos(a), by + 0.13 * math.sin(a), bz + 0.12),
                             (bx + 0.05 * math.cos(a), by + 0.05 * math.sin(a), bz + 0.17)], 0.01, seg=6)
    p.torus('Metal#vp', (bx, by, bz + 0.05), 'z', 0.13, 0.01, seg=20, rseg=6)
    # o degrau e a escada à direita da cabine (a subida do operador) com o corrimão
    vp.rbox(p, 'Paint#plate', (0.95, 1.18, DECK + 0.12), (0.62, 0.56, 0.24), r=0.06, k=4)       # a caixa de ferramentas
    for xx in (0.78, 1.12):
        vp.rbox(p, 'Metal', (xx, 1.465, DECK + 0.17), (0.06, 0.02, 0.05), r=0.01, k=1)
    for zz in (1.45, 1.1):
        vp.rbox(p, 'Paint#plate', (2.25, 1.35, zz), (0.22, 0.4, 0.035), r=0.03, k=2)
        vp.rbox(p, 'Dark', (2.25, 1.35, zz + 0.019), (0.16, 0.32, 0.006), r=0.02, k=2)
    # ---------------------------------------------------------------- o capô do motor: o bloco inclinado, as venezianas, a tampa
    ex0, ex1, ez = ENG
    sh = [[(ex0, -1.4, DECK), (ex0, 1.4, DECK), (ex1, 1.4, DECK), (ex1, -1.4, DECK)],
          [(ex0 + 0.05, -1.35, DECK + 0.42), (ex0 + 0.05, 1.35, DECK + 0.42), (ex1 - 0.04, 1.35, DECK + 0.42), (ex1 - 0.04, -1.35, DECK + 0.42)],
          [(ex0 + 0.18, -1.12, ez), (ex0 + 0.18, 1.12, ez), (ex1 - 0.08, 1.12, ez), (ex1 - 0.08, -1.12, ez)]]
    p.loft('Paint#shell', sh)
    for s in (-1, 1):
        # a veneziana grande do lado do capô (a arte: a grade em pé atrás da cabine)
        louvers(p, (-0.6, 1.378 * s, DECK + 0.22), (0, s, 0), (1, 0, 0), 1.2, 0.3, 5, tilt=-35 * s)
        louvers(p, (-0.62, 1.235 * s, DECK + 0.52), (0, 0.656 * s, 0.754), (1, 0, 0), 1.1, 0.16, 3, tilt=-30 * s)
    vp.rbox(p, 'Paint#plate', (-0.62, 0.0, ez + 0.08), (0.9, 1.4, 0.16), r=0.08, k=4)            # a tampa de cima
    for xx in (-0.2, -1.04):
        vp.bolt_row(p, 'Metal', (xx, -0.62, ez + 0.162), (xx, 0.62, ez + 0.162), 6, (0, 0, 1), r=0.018, h=0.012)
    # as chapas rebitadas (a arte: o casco todo em placas com os rebites): as juntas e as fileiras de parafusos do capô, da cabine e
    # dos para-lamas
    for s in (-1, 1):
        vp.bolt_row(p, 'Metal', (ex0 + 0.1, 1.405 * s, DECK + 0.06), (ex1 - 0.1, 1.405 * s, DECK + 0.06), 10, (0, s, 0), r=0.018, h=0.012)
        vp.bolt_row(p, 'Metal', (cx0 + 0.1, (cy0 - 0.004) if s < 0 else (cy1 + 0.004), DECK + 0.08),
                    (cx1 - 0.1, (cy0 - 0.004) if s < 0 else (cy1 + 0.004), DECK + 0.08), 9, (0, s, 0), r=0.018, h=0.012)
        vp.bolt_row(p, 'Metal', (0.98, (SPON_HW + 0.004) * s, 1.42), (-0.3, (SPON_HW + 0.004) * s, 1.42), 7, (0, s, 0), r=0.018, h=0.012)
        for x in (1.05, 0.25):
            vp.panel_seam(p, 'Dark', (x, (SPON_HW + 0.002) * s, 1.38), (x, (SPON_HW + 0.002) * s, DECK - 0.04), (0, s, 0), w=0.022, d=0.01)
    yy = cy0 - 0.004
    vp.panel_seam(p, 'Dark', (cx0 + 0.55, yy, DECK + 0.05), (cx0 + 0.55, yy, ch - 0.05), (0, -1, 0), w=0.022, d=0.01)
    for xx in (-0.15, -1.1):
        vp.panel_seam(p, 'Dark', (xx, -1.1, ez + 0.003), (xx, 1.1, ez + 0.003), (0, 0, 1), w=0.024, d=0.012)
    vp.bolt_row(p, 'Metal', (cx0 + 0.1, cy0 + 0.12, ch + 0.065), (cx1 - 0.45, cy0 + 0.12, ch + 0.065), 7, (0, 0, 1), r=0.016, h=0.012)
    # a tomada de ar (a caixa com a tela em cima, na frente-direita do capô)
    vp.rbox(p, 'Paint#plate', (-0.05, 0.55, ez + 0.14), (0.36, 0.42, 0.28), r=0.06, k=4)
    vp.rbox(p, 'Dark', (-0.05, 0.55, ez + 0.285), (0.28, 0.34, 0.012), r=0.04, k=2)
    # as 2 chaminés de escape (esquerda): o tubo, a capa de calor furada, a abraçadeira e o cotovelo pra trás
    for y in (-0.84, -0.35):
        x = -1.06
        vp.lathe(p, 'Dark', (x, y, ez), 'z', [(0.0, 0.0), (0.16, 0.0), (0.16, 0.06), (0.12, 0.09), (0.12, 0.96), (0.125, 1.0), (0.0, 1.0)], seg=16)
        vp.lathe(p, 'Metal', (x, y, ez + 0.2), 'z', [(0.13, 0.0), (0.145, 0.02), (0.145, 0.5), (0.13, 0.52), (0.125, 0.52), (0.125, 0.0)],
                 seg=16, closed=True)
        for k in range(4):
            vp.rbox(p, 'Dark', (x - 0.146, y, ez + 0.28 + k * 0.1), (0.012, 0.08, 0.04), r=0.01, k=1)
        elbow = vp.bent_bar([(x, y, ez + 0.96), (x, y, ez + 1.08), (x - 0.22, y, ez + 1.12)], R=0.1, n=5)
        vp.tube(p, 'Dark', elbow, 0.12, seg=14)
        vp.lathe(p, 'Dark', (x - 0.22, y, ez + 1.12), (-1, 0, 0), [(0.0, 0.0), (0.135, 0.0), (0.135, 0.06), (0.11, 0.06), (0.11, 0.02), (0.0, 0.02)], seg=14)
    # ---------------------------------------------------------------- o deck de trás: a lona azul dobrada, o corrimão amarelo
    vp.rbox(p, 'Paint#plate', ((TAIL + 0.1 + ex0) / 2, 0.0, DECK + 0.035), (ex0 - TAIL - 0.1, 2.5, 0.07), r=0.06, k=4)
    vp.tarp_bundle(p, (-1.98, -0.38, DECK + 0.07), (0.9, 1.5, 0.26), m='Team', strap_m='Dark', buckle_m='Metal', straps=2, layers=3,
                   seed=6.0, sag_ends=0.25, swap=True)
    rail = [(-0.4, -1.58), (TAIL + 0.06, -1.58), (TAIL + 0.06, 1.58), (-0.4, 1.58)]
    for z in (DECK + 0.55, DECK + 1.0):
        vp.tube(p, 'Yellow', vp.bent_bar([(x, y, z) for x, y in rail], R=0.16, n=5), 0.026, seg=8)
    for (x0, y0), (x1, y1) in zip(rail[:-1], rail[1:]):
        n = max(1, int(math.hypot(x1 - x0, y1 - y0) / 0.75))
        for k in range(n + 1):
            u = k / n
            if (k == 0 and (x0, y0) != rail[0]) :
                continue
            x, y = x0 + (x1 - x0) * u, y0 + (y1 - y0) * u
            vp.tube(p, 'Yellow', [(x, y, DECK + 0.07), (x, y, DECK + 1.0)], 0.024, seg=8)
            vp.lathe(p, 'Yellow', (x, y, DECK + 0.07), 'z', [(0.0, 0.0), (0.05, 0.0), (0.05, 0.015), (0.0, 0.02)], seg=10)
    # ---------------------------------------------------------------- o braço robótico amarelo dobrado (à direita, por cima de trás)
    bx_, by_ = -1.22, 0.78
    vp.lathe(p, 'Yellow', (bx_, by_, ENG[2]), 'z', [(0.0, 0.0), (0.4, 0.0), (0.4, 0.06), (0.36, 0.1), (0.3, 0.12), (0.3, 0.3), (0.26, 0.34),
                                                 (0.0, 0.34)], seg=24)
    vp.bolt_ring(p, 'Metal', (bx_, by_, ENG[2] + 0.06), 'z', 0.36, 12, r=0.018, h=0.012)
    sh0 = Vector((bx_, by_, ENG[2] + 0.52))
    elbow = Vector((-1.95, by_, 3.3))
    tip = Vector((-2.78, by_ - 0.16, 2.5))
    vp.rbox(p, 'Yellow#plate', (bx_, by_, ENG[2] + 0.5), (0.46, 0.42, 0.34), r=0.08, k=4)                     # o ombro
    boom(p, 'Yellow#plate', sh0, elbow, 0.36, 0.42, r=0.1)
    boom(p, 'Yellow#plate', elbow, tip, 0.3, 0.34, r=0.08)
    for q, w in ((sh0, 0.5), (elbow, 0.44)):
        vp.lathe(p, 'Yellow', q, (0, 1, 0), [(0.0, -w / 2), (0.2, -w / 2), (0.22, -w / 2 + 0.03), (0.22, w / 2 - 0.03), (0.2, w / 2),
                                            (0.0, w / 2)], seg=20)
        vp.lathe(p, 'Metal', q, (0, 1, 0), [(0.0, -w / 2 - 0.03), (0.08, -w / 2 - 0.03), (0.08, w / 2 + 0.03), (0.0, w / 2 + 0.03)], seg=12)
    cylinder(p, sh0 + Vector((0.35, 0.24, -0.1)), elbow + Vector((0.7, 0.24, -0.28)), 0.08, 0.045)
    cylinder(p, elbow + Vector((0.25, -0.22, 0.22)), elbow + (tip - elbow) * 0.62 + Vector((0.0, -0.18, 0.18)), 0.065, 0.038)
    # a garra na ponta (de ponta pra baixo): o punho e os 2 dedos curvos
    vp.lathe(p, 'Yellow', tip, (tip - elbow).normalized(), [(0.0, 0.0), (0.17, 0.0), (0.17, 0.16), (0.12, 0.2), (0.0, 0.2)], seg=16)
    d = (tip - elbow).normalized()
    for sgn in (-1, 1):
        pts = [tip + d * 0.18, tip + d * 0.36 + Vector((0, 0.14 * sgn, 0)), tip + d * 0.52 + Vector((0, 0.06 * sgn, 0))]
        vp.tube(p, 'Dark', pts, 0.045, seg=8, r_fn=lambda i: 0.05 - 0.012 * i)
    # o apoio do braço dobrado (o berço no deck)
    vp.rbox(p, 'Yellow#plate', (-2.95, by_ - 0.16, DECK + 0.13), (0.3, 0.3, 0.12), r=0.04, k=3)
    # ---------------------------------------------------------------- o escarificador atrás: o quadro, os braços, o dente curvo
    vp.rbox(p, 'Yellow#plate', (-2.62, 0.0, 1.18), (0.3, 1.3, 0.6), r=0.08, k=4)
    for s in (-1, 1):
        for z in (1.0, 1.38):
            boom(p, 'Yellow#plate', (-2.62, 0.48 * s, z), (-3.32, 0.48 * s, z + 0.26), 0.2, 0.22, r=0.07)
            vp.lathe(p, 'Metal', (-2.62, 0.48 * s, z), (0, 1, 0), [(0.0, -0.13), (0.08, -0.13), (0.08, 0.13), (0.0, 0.13)], seg=12)
        cylinder(p, (-2.5, 0.25 * s, 2.0), (-3.3, 0.25 * s, 1.92), 0.09, 0.05)
        vp.rbox(p, 'Yellow#plate', (-2.44, 0.25 * s, 1.88), (0.22, 0.18, 0.26), r=0.05, k=4)
    vp.rbox(p, 'Yellow#plate', (-3.4, 0.0, 1.5), (0.42, 1.1, 0.6), r=0.1, k=4)          # a barra do dente
    # o dente: a viga curva maciça (a seção de quina redonda varrida pelo arco) e a ponta escura
    shank = []
    # (a arte: o dente desce da barra e a ponta curva pra FRENTE, pra baixo do trator)
    ctrl = [Vector((-3.48, 0.0, 1.42)), Vector((-3.66, 0.0, 1.0)), Vector((-3.66, 0.0, 0.58)), Vector((-3.46, 0.0, 0.28)),
            Vector((-3.24, 0.0, 0.18))]
    for k in range(13):
        t = k / 12 * (len(ctrl) - 1)
        i = min(int(t), len(ctrl) - 2)
        u = t - i
        a0 = ctrl[max(i - 1, 0)]
        a1, a2, a3 = ctrl[i], ctrl[i + 1], ctrl[min(i + 2, len(ctrl) - 1)]
        # Catmull-Rom (a curva lisa pelos pontos)
        shank.append(0.5 * ((2 * a1) + (-a0 + a2) * u + (2 * a0 - 5 * a1 + 4 * a2 - a3) * u * u + (-a0 + 3 * a1 - 3 * a2 + a3) * u ** 3))
    rings = []
    for k, q in enumerate(shank):
        dd = (shank[min(k + 1, 12)] - shank[max(k - 1, 0)]).normalized()
        n_ = Vector((dd.z, 0.0, -dd.x))
        w = 0.26 - 0.1 * k / 12
        h = 0.34 - 0.16 * k / 12
        ring = []
        for (u, v) in vp.rounded_rect(h, w, 0.05, 3):
            P = q + n_ * u + Vector((0, 1, 0)) * v
            ring.append((P.x, P.y, P.z))
        rings.append(ring)
    p.loft('Yellow#shank', rings[:11], bevel=0.0, smooth=True)
    p.loft('Dark#shank', rings[10:], bevel=0.0, smooth=True)
    return p.finish()


# ---------------------------------------------------------------------------------- lâmina (origem no pivô)
def blade():
    p = Part(PREFIX + '_Blade')
    p.bm('Yellow#plate', 0.03)
    p.bm('Yellow#shell', 0.03)
    # a concha côncava em perfil (x, z): a frente curva (o arco), o fio de corte embaixo e o lábio em cima; as costas retas
    face = []
    for k in range(9):
        t = k / 8
        face.append((3.53 - 0.36 * math.sin(math.pi * t) - 0.08 * t, 0.04 + 1.66 * t))
    back = [(2.88, 1.7), (2.72, 1.42), (2.68, 0.8), (2.78, 0.3), (2.92, 0.04)]
    prof = face + back
    hw_c, hw = 1.4, 1.94
    secs = []
    # o centro reto e as asas viradas pra frente (a lâmina SU do D9)
    for y, fwd in ((-hw, 0.28), (-hw_c, 0.0), (hw_c, 0.0), (hw, 0.28)):
        secs.append([(x + fwd, y, z) for (x, z) in prof])
    p.loft('Yellow#shell', secs)
    # o fio de corte (as chapas escuras com os parafusos) embaixo da frente
    for (y0, y1, fwd0, fwd1) in ((-hw, -hw_c, 0.28, 0.0), (-hw_c, hw_c, 0.0, 0.0), (hw_c, hw, 0.0, 0.28)):
        n = max(2, int((y1 - y0) / 0.42))
        for k in range(n):
            ya = y0 + (y1 - y0) * k / n + 0.01
            yb = y0 + (y1 - y0) * (k + 1) / n - 0.01
            f = fwd0 + (fwd1 - fwd0) * ((ya + yb) / 2 - y0) / (y1 - y0)
            x = face[0][0] + f - 0.03
            vp.rbox(p, 'Yellow#plate', (x, (ya + yb) / 2, 0.15), (0.05, yb - ya, 0.22), pitch=-25, r=0.02, k=2)   # (a arte: amarelo)
            for d in (-0.25, 0.25):
                vp.nut(p, 'Metal', (x + 0.03, (ya + yb) / 2 + d * (yb - ya), 0.17), (0.9, 0, 0.42), r=0.022, h=0.016)
    # o lábio de cima (a barra redonda que corre a lâmina)
    for a, b in ((secs[0][8], secs[1][8]), (secs[1][8], secs[2][8]), (secs[2][8], secs[3][8])):
        vp.tube(p, 'Yellow', [Vector(a) + Vector((-0.04, 0, 0.02)), Vector(b) + Vector((-0.04, 0, 0.02))], 0.055, seg=12)
    # as nervuras atrás (em pé), a viga do meio e os batentes dos braços e dos cilindros
    for y in (-1.2, -0.42, 0.42, 1.2):
        vp.rbox(p, 'Yellow#plate', (2.67, y, 0.95), (0.18, 0.14, 1.5), r=0.05, k=4)
    vp.rbox(p, 'Yellow#plate', (2.67, 0.0, 1.05), (0.16, 2.9, 0.26), r=0.06, k=4)
    # as abas dos lados (as chapas de ponta)
    for s in (-1, 1):
        prof_s = [(x + 0.28, z) for (x, z) in prof]
        pts = [(x, (hw + 0.0) * s, z) for (x, z) in prof_s] + [(x, (hw + 0.06) * s, z) for (x, z) in prof_s]
        solid(p, 'Yellow#plate', pts)
    for s in (-1, 1):
        # os braços de empurrar: a viga de quina redonda das costas da lâmina até o munhão, por fora da esteira
        a = Vector((2.72, ARM_Y * s, 0.58))
        b = Vector((BLADE_PIVOT[0] + 0.12, ARM_Y * s, BLADE_PIVOT[2]))
        boom(p, 'Yellow#plate', a, b, 0.22, 0.34, r=0.08)
        vp.rbox(p, 'Yellow#plate', (2.66, (ARM_Y - 0.15) * s, 0.58), (0.32, 0.5, 0.44), r=0.08, k=4)     # o batente na lâmina
        vp.lathe(p, 'Yellow', (BLADE_PIVOT[0], (ARM_Y - 0.11) * s, BLADE_PIVOT[2]), (0, s, 0),
                 [(0.0, 0.0), (0.22, 0.0), (0.24, 0.03), (0.24, 0.19), (0.22, 0.22), (0.0, 0.22)], seg=20)
        vp.lathe(p, 'Metal', (BLADE_PIVOT[0], (ARM_Y + 0.11) * s, BLADE_PIVOT[2]), (0, s, 0),
                 [(0.0, 0.0), (0.1, 0.0), (0.1, 0.04), (0.07, 0.06), (0.0, 0.06)], seg=14)
        vp.bolt_ring(p, 'Metal', (BLADE_PIVOT[0], (ARM_Y + 0.11) * s, BLADE_PIVOT[2]), (0, s, 0), 0.16, 8, r=0.018, h=0.014)
        # a escora diagonal (o braço de inclinação) da lâmina pro braço
        boom(p, 'Yellow#plate', (2.62, 1.05 * s, 1.35), (1.4, (ARM_Y - 0.1) * s, 0.62), 0.14, 0.16, r=0.05)
        # a ponta do cilindro de levantar (o olhal na lâmina; o corpo fica no casco)
        vp.rbox(p, 'Yellow#plate', (2.7, 1.23 * s, 1.52), (0.3, 0.24, 0.3), r=0.06, k=4)
    o = p.finish()
    o.data.transform(Matrix.Translation(-V(*BLADE_PIVOT)))         # a origem no pivô (o jogo gira a lâmina em volta dele)
    return o


def lift_cylinders(p):
    """Os cilindros de levantar a lâmina, no casco (a lâmina leva o olhal da ponta): do canto da frente do capô até a lâmina."""
    for s in (-1, 1):
        # (a arte: o conjunto amarelo grande no canto da frente, o cilindro grosso descendo do alto até as costas da lâmina)
        a = Vector((1.84, 0.84 * s, DECK + 0.36))
        b = Vector((2.66, 1.23 * s, 1.56))
        cylinder(p, a, b, 0.14, 0.075, frac=0.6, seg=18)
        vp.rbox(p, 'Yellow#plate', (1.8, 0.84 * s, DECK + 0.2), (0.46, 0.42, 0.4), r=0.08, k=4)          # a torre do munhão
        vp.lathe(p, 'Yellow', (1.84, (0.84 + 0.21) * s, DECK + 0.36), (0, s, 0), [(0.0, 0.0), (0.15, 0.0), (0.16, 0.02), (0.16, 0.08),
                                                                                (0.12, 0.1), (0.0, 0.1)], seg=18)
        vp.lathe(p, 'Metal', (1.84, (0.84 + 0.31) * s, DECK + 0.36), (0, s, 0), [(0.0, 0.0), (0.07, 0.0), (0.07, 0.03), (0.0, 0.035)], seg=12)
        # a mangueira hidráulica do cilindro
        vp.tube(p, 'Dark', vp.bent_bar([(1.7, 0.7 * s, DECK + 0.02), (1.95, 0.72 * s, DECK + 0.45), (2.2, 0.95 * s, DECK + 0.25)], R=0.12, n=3),
                0.022, seg=6)


def beacon():
    """A luz que gira dentro da cúpula da sirene: o refletor (a meia concha torneada) e a lâmpada, com a ORIGEM no eixo."""
    p = Part(PREFIX + '_Beacon')
    vp.lathe(p, 'Metal', (0.0, 0.0, 0.0), 'z', [(0.0, 0.0), (0.03, 0.0), (0.03, 0.04), (0.0, 0.04)], seg=10)
    vp.lathe(p, 'Metal', (-0.035, 0.0, 0.075), (-1, 0, 0), [(0.0, 0.0), (0.06, -0.01), (0.075, 0.02), (0.07, 0.025), (0.055, 0.0), (0.0, 0.01)],
             seg=14)
    vp.lathe(p, 'Light', (-0.02, 0.0, 0.075), 'x', [(0.0, -0.02), (0.03, -0.02), (0.035, 0.0), (0.03, 0.02), (0.0, 0.025)], seg=12)
    o = p.finish()
    return o


# ---------------------------------------------------------------------------------- rodagem
def wheels():
    """A motriz alta dentada (a assinatura do D9), as 2 rodas-guia e os roletes de flange dupla."""
    p = Part(PREFIX + '_Wheels')
    for s in (-1, 1):
        y = TRACK_Y * s
        sx, sz, sr = SPROCKET
        vp.sprocket(p, (sx, y, sz), sr, 0.5, s, 'Paint', 'Metal', teeth=15, holes_m='Dark')
        for (x, z, r) in (IDLER_F, IDLER_R):
            vp.idler(p, (x, y, z), r, 0.5, s, 'Paint', 'Metal', gap=0.0)
        for x in ROLLERS_X:
            vp.track_roller(p, (x, y, ROLLER_Z), ROLLER_R, 0.5, 'Metal', s)
    return p.finish()


def track(side):
    s = 1 if side == 'r' else -1
    m = 0.05
    circles = [(SPROCKET[0], SPROCKET[1], SPROCKET[2] + m),
               (IDLER_F[0], IDLER_F[1], IDLER_F[2] + m), (IDLER_R[0], IDLER_R[1], IDLER_R[2] + m)] + \
              [(x, ROLLER_Z, ROLLER_R + m) for x in ROLLERS_X]
    return vp.track_links(f'{PREFIX}_Track_{side}', circles, TRACK_Y * s, TRACK_W, pitch=0.21, t_in=0.045, t_out=0.02, grouser=0.045,
                          guide=None, m='Track', guide_m='Metal', gap=0.12, center_gap=0.0)


# ---------------------------------------------------------------------------------- exportação
def write_spec():
    import json
    bp = (BLADE_PIVOT[0], -BLADE_PIVOT[1], BLADE_PIVOT[2])
    bc = (BEACON[0], -BEACON[1], BEACON[2])
    spec = {
        'parts': [[PREFIX + '_Hull', [0, 0, 0]], [PREFIX + '_Blade', list(bp)], [PREFIX + '_Beacon', list(bc)],
                  [PREFIX + '_Wheels', [0, 0, 0]], [PREFIX + '_Track_l', [0, 0, 0]], [PREFIX + '_Track_r', [0, 0, 0]]],
        'views': [['top', 'ortho', [0, 0, -1], [0, -1, 0], [1800, 1000]],
                  ['left', 'ortho', [0, -1, 0], [0, 0, 1], [1800, 820]],
                  ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [900, 900]],
                  ['back', 'ortho', [1, 0, 0], [0, 0, 1], [900, 900]],
                  ['three_quarter', 'persp', 27.0, 232.0, [1600, 1000], 30.0, 0.8],
                  ['three_quarter_right', 'persp', 30.0, 40.0, [1600, 1000], 30.0, 0.8]],
        'usa_at': [0, 6.0, 0], 'tank_views': False,
        'game': [['game_cam', 37.5, 35.0, 50.0, 0.8], ['game_cam_close', 37.5, 35.0, 28.0, 0.75]],
    }
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)


def main():
    clear_scene()
    h = hull()
    # os cilindros de levantar a lâmina vão no casco: uma peça à parte juntada (o hull já fechou)
    pc = Part(PREFIX + '_HullCyl')
    pc.bm('Yellow#plate', 0.03)
    lift_cylinders(pc)
    hc = pc.finish()
    from model_lib import select_only
    select_only(h)
    hc.select_set(True)
    bpy.ops.object.join()
    h = bpy.context.view_layer.objects.active
    h.name = PREFIX + '_Hull'
    bl = blade()
    bc = beacon()
    w = wheels()
    tl, tr = track('l'), track('r')
    # (como o laser do Paladin) a lâmina e a luz da sirene ficam NO LUGAR pro cozimento das máscaras e voltam pra origem do pivô só
    # na hora de exportar
    import model_lib
    detail = model_lib.DETAIL.get('defer') is not None
    moved = {bl.name: V(*BLADE_PIVOT), bc.name: V(*BEACON)}
    if detail:
        bl.data.transform(Matrix.Translation(moved[bl.name]))
        bc.data.transform(Matrix.Translation(moved[bc.name]))
        _orig = model_lib.export

        def _export(o, out_dir):
            if o.name in moved and model_lib.DETAIL.get('defer') is None:
                o.data.transform(Matrix.Translation(-moved[o.name]))
            return _orig(o, out_dir)
        model_lib.export = _export
    total = 0
    for o in (h, bl, bc, w, tl, tr):
        total += export(o, OUT)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL tris por peca', {o.name: tris(o) for o in (h, bl, bc, w, tl, tr)})
    if detail:
        (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h, bl, bc, w, tl, tr])
        print('MODEL medidas: comprimento %.2f m (x %.2f..%.2f, da ponta do dente ao fio da lamina), largura %.2f m, altura %.2f m'
              % (ax1 - ax0, ax0, ax1, ay1 - ay0, az1))
    print('MODEL blade.at_m', BLADE_PIVOT, 'spin.at_m', BEACON, 'tracks_m', [round(2 * TRACK_Y, 3), TRACK_W])
    write_spec()


main()
