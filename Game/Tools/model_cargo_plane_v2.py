"""O avião de carga da USA (o do Supply Drop Zone), VERSÃO B no detalhe AAA (01/11/2026): a versão A (Game/Tools/model_cargo_plane.py,
aprovada em 23/10 com a Drop Zone: "ta tudo top") fica como está; esta é a que ele compara com ela.

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_cargo_plane_v2.py C:/Users/luisf/explosion/captures/game/models/cargo_plane_v2

O pedido (31/10 e 01/11): "TEXTURIZADO, DETALHADO [...] AVIÕES, TUDO [...] NADA DE QUINAS QUADRADAS". Não há ficha: a base é o C-130
Hercules (asa alta reta, 4 turboélices, a cauda levantada com a rampa, as carenagens do trem nos lados da barriga) e o layout é o do A
(as mesmas estações, a mesma envergadura de 30 m, a rampa aberta, DROP_AT igual), refeito no que o estudo pede pra aeronave
(docs/ESTUDO_DETALHE_AAA.md §3):
  - a fuselagem lisa interpolada com os SULCOS das cavernas, as juntas corridas, o radome com a junta, as janelas da cabine em
    vidros com os montantes, as janelinhas redondas do porão, as portas (a da tripulação, as dos paraquedistas), as antenas e o fio
    da antena de HF;
  - a carenagem da asa no teto e as do trem nos lados da barriga LISAS (o A eram caixas com chanfro), com as portas do trem;
  - a asa de perfil com os flapes, os ailerons, as juntas, a ponta na cor do jogador e as luzes;
  - as 4 naceles lisas com as juntas da capota, a boca de ar funda embaixo do cubo, o escape escuro, o cubo torneado e as hélices de
    4 pás largas que afinam e torcem;
  - a deriva com o leme e a faixa na cor do jogador, os estabilizadores com os profundores;
  - a rampa aberta com o vão escuro, a rampa de chapa e os batentes.
A USA não tem desgaste pintado (13/10). O A já é desenhado na escala final (30 m de envergadura): as medidas são do jogo.

Peça (o NOME do A; origem no centro do avião, no eixo da fuselagem embaixo da asa; frente em +X): SM_USA_CargoPlane.
Slots (os do A): Body, Trim, Dark, Glass, Player, Light, Beacon. DROP_AT (−6,6; 0; −1,6) mantido.
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_air_parts as ap  # noqa: E402
from model_china_vehicle_kit import bounds, tris  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'cargo_plane_v2')
os.makedirs(OUT, exist_ok=True)
NAME = 'SM_USA_CargoPlane'

COLORS.update({'Body': (176, 180, 178), 'Trim': (70, 70, 68), 'Dark': (40, 40, 38), 'Glass': (30, 50, 80),
               'Player': (86, 108, 150), 'Light': (255, 230, 170), 'Beacon': (255, 170, 40)})

# ---------------------------------------------------------------------------------- medidas do A (escala final, m)
FUS = [(11.0, 0.25, 0.25, 0.05), (10.75, 1.3, 1.25, 0.0), (10.3, 2.2, 2.15, 0.1), (9.6, 2.85, 2.85, 0.2),
       (8.6, 3.2, 3.25, 0.3), (7.0, 3.3, 3.3, 0.3), (-3.2, 3.3, 3.3, 0.3), (-5.0, 3.2, 2.85, 0.52),
       (-7.0, 2.8, 2.05, 0.9), (-9.0, 2.05, 1.35, 1.25), (-10.6, 1.0, 0.75, 1.45), (-11.2, 0.3, 0.3, 1.5)]
PEXP = 2.6
SPAN = 30.0
WING_ROOT_LE, WING_TIP_LE = 2.3, 1.7
WING_ROOT_C, WING_TIP_C = 3.7, 2.0
WING_Z, DIHEDRAL = 2.05, 0.35
WING_T = (0.55, 0.22)
NACELLES = (5.4, 9.6)
PROP_R = 1.55
DROP_AT = (-6.6, 0.0, -1.6)
# o perfil da asa do A (com arqueamento): a espessura × 0,6 no A = × 1,2 aqui (o Wing usa a meia espessura)
FOIL_A = [(0.0, 0.0), (0.04, 0.42), (0.15, 0.75), (0.35, 0.85), (0.65, 0.6), (1.0, 0.06), (1.0, -0.04), (0.65, -0.25),
          (0.35, -0.38), (0.15, -0.35), (0.04, -0.24)]

SEC = ap.Section([(x, w, h, zc, PEXP) for x, w, h, zc in FUS], smooth=5)
SW = 0.02                         # a linha de painel (o avião passa alto: 2 cm)
GW, GD = 0.035, 0.014             # o sulco das cavernas


def deg(a):
    return math.radians(a)


def wing_le(y):
    return WING_ROOT_LE + (WING_TIP_LE - WING_ROOT_LE) * abs(y) / (SPAN / 2)


def wing_c(y):
    return WING_ROOT_C + (WING_TIP_C - WING_ROOT_C) * abs(y) / (SPAN / 2)


def wing_z(y):
    return WING_Z + DIHEDRAL * abs(y) / (SPAN / 2)


def wing_t(y):
    return WING_T[0] + (WING_T[1] - WING_T[0]) * abs(y) / (SPAN / 2)


def side_th(x, z, s):
    if s > 0:
        return ap.th_for(SEC, x, 2, z, -math.pi / 2 + 0.01, math.pi / 2 - 0.01)
    return ap.th_for(SEC, x, 2, z, 3 * math.pi / 2 - 0.01, math.pi / 2 + 0.01)


# ---------------------------------------------------------------------------------- a fuselagem
def fuselage(p):
    ths = ap.angles(40)
    xs = sorted(set([round(r[0], 4) for r in SEC.rows] + [6.0, 5.0, 4.0, 3.0, 2.0, 1.0, 0.0, -1.0, -2.0]), reverse=True)
    nose = [x for x in xs if x >= 10.3]
    body = [x for x in xs if x <= 10.3]
    ap.skin(p, 'Trim#nose', SEC, nose, ths, caps=(True, True))      # fechado: as normais do radome saem pra fora
    ap.skin(p, 'Body#fus', SEC, body, ths, grooves=[(x, GW, GD) for x in (9.9, 7.6, 4.3, -0.4, -3.6, -6.4, -9.2)], caps=(False, True))
    # as juntas corridas (os lados, o teto, a barriga)
    for s in (1, -1):
        for z in (1.25, -0.55):
            xs_ = [9.4 + (-9.8 - 9.4) * k / 30 for k in range(31)]
            pts, ns = [], []
            for x in xs_:
                if SEC.P(x, math.pi / 2).z < z + 0.1 or SEC.P(x, -math.pi / 2).z > z - 0.1:
                    continue
                th = side_th(x, z, s)
                pts.append(SEC.P(x, th))
                ns.append(SEC.N(x, th))
            ap.ribbon(p, 'Trim', pts, ns, SW)
    # o para-brisa e as janelas da cabine (os vidros na pele com os montantes) e as janelas de baixo do nariz
    for (xa, xb, ta, tb) in ((9.78, 9.36, 56, 84), (9.62, 9.18, 30, 52), (9.0, 8.55, 34, 50), (10.05, 9.7, 4, 24)):
        for s in (1, -1):
            a, b = (deg(ta), deg(tb)) if s > 0 else (math.pi - deg(tb), math.pi - deg(ta))
            ap.sec_patch(p, 'Glass', SEC, xa, xb, a, b, nx=4, nt=4, lift=0.008, t=0.01)
            ap.sec_rect(p, 'Trim', SEC, xa, xb, a, b, 0.035, lift=0.012)
    ap.sec_ribbon(p, 'Trim', SEC, 9.82, 9.32, math.pi / 2, 0.06, n=4, lift=0.012)
    # as janelinhas redondas do porão (4 de cada lado) e as portas: a da tripulação (frente-esquerda) e as dos paraquedistas (atrás)
    for s in (1, -1):
        for x in (5.6, 3.2, -1.6, -3.0):
            th = side_th(x, 0.75, s)
            ap.round_window(p, 'Glass', 'Trim', SEC.P(x, th), SEC.N(x, th), 0.13)
        for (xa, xb, za, zb) in ((-3.9, -4.85, -0.9, 0.95),):
            ta, tb = sorted((side_th((xa + xb) / 2, za, s), side_th((xa + xb) / 2, zb, s)))
            ap.sec_rect(p, 'Trim', SEC, xa, xb, ta, tb, 0.03)
            th = side_th((xa + xb) / 2, 0.55, s)
            ap.round_window(p, 'Glass', 'Trim', SEC.P((xa + xb) / 2, th), SEC.N((xa + xb) / 2, th), 0.12)
    ta, tb = sorted((side_th(7.9, -1.0, -1), side_th(7.9, 0.9, -1)))
    ap.sec_rect(p, 'Trim', SEC, 8.4, 7.4, ta, tb, 0.03)
    th = side_th(7.9, 0.5, -1)
    ap.round_window(p, 'Glass', 'Trim', SEC.P(7.9, th), SEC.N(7.9, th), 0.13)
    # as antenas de lâmina (em cima e embaixo); o fio da antena de HF ficou de fora (3 cm: pisca com a TSR, o estudo §1)
    for x, th, h in ((6.2, 90, 0.4), (-2.6, 90, 0.34), (4.0, 270, 0.32), (-1.2, 270, 0.3)):
        q, n = SEC.P(x, deg(th)), SEC.N(x, deg(th))
        vp.prism(p, 'Trim', [(0.0, 0.0), (-0.42, 0.0), (-0.36, h), (-0.14, h)], q - n * 0.03 - Vector((0, 0.025, 0)), Vector((1, 0, 0)), n,
                 Vector((0, 1, 0)), 0.05)
    # as sirenes (em cima da carenagem da asa e na barriga), torneadas
    vp.lathe(p, 'Beacon', (0.2, 0.0, 2.25), 'z', [(0.0, 0.0), (0.12, 0.0), (0.12, 0.08), (0.09, 0.15), (0.0, 0.18)], seg=14)
    vp.lathe(p, 'Beacon', (0.6, 0.0, -1.36), (0, 0, -1), [(0.0, 0.0), (0.12, 0.0), (0.12, 0.08), (0.09, 0.14), (0.0, 0.16)], seg=14)


def fairings(p):
    """A carenagem da asa no teto e as do trem nos lados da barriga, LISAS (seções de quina redonda interpoladas)."""
    rows = []
    for x, w, h in ((2.5, 0.5, 0.1), (2.1, 1.9, 0.5), (1.2, 2.3, 0.6), (-0.6, 2.3, 0.6), (-1.6, 1.9, 0.45), (-2.4, 0.6, 0.12)):
        rows.append((x, w, h * 2, 1.62, 2.8))
    R = ap.Section(rows, smooth=4)
    ap.skin(p, 'Body#fair', R, [r[0] for r in R.rows], ap.angles(28), grooves=[(0.3, GW, GD)], caps=(True, True))
    for s in (-1, 1):
        rows = []
        for x, w, h in ((2.4, 0.2, 0.3), (1.9, 0.62, 0.92), (1.0, 0.72, 1.02), (-2.2, 0.72, 1.02), (-3.0, 0.6, 0.86), (-3.6, 0.2, 0.3)):
            rows.append((x, w, h, -0.82, 2.6, s * 1.55))
        G = ap.Section(rows, smooth=4)
        ap.skin(p, 'Body#spon', G, [r[0] for r in G.rows], ap.angles(28), grooves=[(-0.6, GW, GD)], caps=(True, True))
        # as portas do trem (o contorno embaixo) e a junta com o corpo
        th0, th1 = (deg(-80), deg(-30)) if s > 0 else (math.pi + deg(30), math.pi + deg(80))
        ap.sec_rect(p, 'Trim', G, 1.4, -0.4, th0, th1, SW)
        ap.sec_rect(p, 'Trim', G, -0.8, -2.6, th0, th1, SW)


def wings(p):
    ys = [0.3, 1.6, 3.5, NACELLES[0], 7.5, NACELLES[1], 12.3, SPAN / 2]
    W = ap.Wing([(wing_le(y), y, wing_z(y), wing_c(y), wing_t(y) * 1.2) for y in ys], foil=FOIL_A)
    W.build(p, 'Body#wing', sides=(1, -1))
    y0 = SPAN / 2 - 0.9
    tip = ap.Wing([(wing_le(y) + 0.015, y, wing_z(y), wing_c(y) + 0.03, wing_t(y) * 1.2 * 1.18) for y in (y0, SPAN / 2 + 0.02)], foil=FOIL_A)
    tip.build(p, 'Player#tip', sides=(1, -1))
    sides = (1, -1)
    for top in (True, False):
        W.chord_line(p, 'Trim', 0.72, 1.7, 11.0, SW, sides=sides, top=top, n=10)          # os flapes
        W.chord_line(p, 'Trim', 0.74, 11.2, 14.2, SW, sides=sides, top=top, n=5)          # o aileron
        W.chord_line(p, 'Trim', 0.08, 1.7, 14.5, SW, sides=sides, top=top, n=12)          # a junta do bordo de ataque
        for y in (1.7, 4.3, 6.6, 8.6, 11.0, 11.2, 14.2):
            W.span_line(p, 'Trim', y, 0.72 if y not in (4.3, 6.6, 8.6) else 0.08, 1.0 if y not in (4.3, 6.6, 8.6) else 0.72, SW,
                        sides=sides, top=top, n=4 if y not in (4.3, 6.6, 8.6) else 8)
    # as luzes de navegação nas pontas (a gota torneada)
    for s in (-1, 1):
        vp.lathe(p, 'Light', (wing_le(SPAN / 2) - 0.4, s * (SPAN / 2 + 0.06), wing_z(SPAN / 2)), (1, 0, 0),
                 [(0.0, -0.18), (0.05, -0.15), (0.07, 0.0), (0.05, 0.12), (0.0, 0.16)], seg=10)
    return W


def nacelles(p):
    """As 4 naceles: o corpo liso (as juntas da capota), a boca de ar funda embaixo do cubo, o escape escuro, o cubo torneado e as
    hélices de 4 pás largas que afinam e torcem (paradas: a peça é única)."""
    for s in (-1, 1):
        for yn in NACELLES:
            y = s * yn
            le = wing_le(yn)
            zc = wing_z(yn) - 0.3
            rows = [(le + dx, w, h, zc + dz, 2.6, y) for dx, w, h, dz in ((2.1, 0.62, 0.66, 0.0), (1.85, 0.96, 1.06, 0.0), (1.2, 1.0, 1.18, -0.04),
                                                                        (0.3, 1.0, 1.2, -0.05), (-1.0, 0.95, 1.1, 0.0), (-2.0, 0.7, 0.75, 0.08),
                                                                        (-2.6, 0.3, 0.32, 0.15))]
            N = ap.Section(rows, smooth=4)
            ap.skin(p, 'Body#nac', N, [r[0] for r in N.rows], ap.angles(24), grooves=[(le + 1.55, GW, GD), (le + 0.2, GW, GD)],
                    caps=(True, True))
            # a junta da capota de cima (a tampa articulada) dos dois lados
            for th in (deg(35), deg(145)):
                ap.sec_ribbon(p, 'Trim', N, le + 1.5, le - 0.9, th, SW, n=6)
            # a boca de ar embaixo do cubo (oval, funda)
            c = Vector((le + 1.95, y, zc - 0.42))
            outer = [tuple(c + Vector((0.0, u, v))) for u, v in vp.rounded_rect(0.42, 0.24, 0.1, 3)]
            outer = [(q[0] - 0.12 * abs(q[2] - c.z) / 0.12, q[1], q[2]) for q in outer]
            inner = [(q[0] - 0.02, c.y + (q[1] - c.y) * 0.72, c.z + (q[2] - c.z) * 0.6) for q in outer]
            ap.duct(p, 'Dark#duct', outer, inner, (-1, 0, 0), 0.5)
            # a carenagem lisa do queixo em volta da boca (aberta na frente: a boca escura fica dentro)
            Q = ap.Section([(le + 1.97, 0.5, 0.3, zc - 0.42, 2.4, y), (le + 1.5, 0.56, 0.36, zc - 0.4, 2.4, y), (le + 0.6, 0.4, 0.2, zc - 0.32, 2.4, y)],
                           smooth=3)
            ap.skin(p, 'Body#chin', Q, [r[0] for r in Q.rows], ap.angles(20), caps=(False, True))
            # o escape (o tubo escuro do lado de fora, em cima, virado pra trás)
            ex = Vector((le - 1.0, y + s * 0.42, zc + 0.32))
            vp.lathe(p, 'Trim', ex, (-1, 0, 0), [(0.0, -0.3), (0.12, -0.3), (0.14, 0.0), (0.14, 0.32), (0.12, 0.36), (0.09, 0.36),
                                                 (0.09, 0.05), (0.0, 0.05)], seg=14)
            # o cubo (o "spinner") torneado e as 4 pás
            vp.lathe(p, 'Body', (le + 2.05, y, zc), 'x', [(0.0, 0.0), (0.3, 0.0), (0.33, 0.05), (0.32, 0.25), (0.26, 0.45), (0.15, 0.62),
                                                         (0.0, 0.72)], seg=20)
            for k in range(4):
                a = math.radians(45 + 90 * k)
                d = Vector((0.0, math.cos(a), math.sin(a)))
                ap.blade(p, 'Dark', (le + 2.28, y, zc), d, 0.26, PROP_R, 0.34, 0.06, twist=(42.0, 22.0), taper=0.75, n=8,
                         axis=(1, 0, 0), root_chord=0.2)


def tail(p):
    """A deriva com o leme e a faixa na cor do jogador, os estabilizadores com os profundores, a luz de cauda."""
    Fn = ap.Wing([(-5.6, 0.0, 1.7, 4.6, 0.42 * 1.2), (-7.15, 0.0, 3.7, 3.35, 0.3 * 1.2), (-8.7, 0.0, 5.7, 2.1, 0.18 * 1.2)], n=(0, 1, 0),
                 foil=ap.FOIL)
    Fn.build(p, 'Body#fin', sides=(1,))

    def fin_pt(z, u, side):
        st = Fn.st
        for a, b in zip(st[:-1], st[1:]):
            if a[2] <= z <= b[2]:
                t = (z - a[2]) / (b[2] - a[2])
                xl, _, _, c, th = [a[j] + (b[j] - a[j]) * t for j in range(5)]
                break
        else:
            xl, _, _, c, th = st[-1]
        return Vector((xl - u * c, side * Fn.half(u) * th / 2, z))
    for side in (1, -1):
        zs = [2.0 + (5.5 - 2.0) * k / 7 for k in range(8)]
        ap.ribbon(p, 'Trim', [fin_pt(z, 0.68, side) for z in zs], [Vector((0, side, 0))] * 8, SW)
        for z in (2.0, 5.5):
            us = [0.68 + 0.32 * k / 3 for k in range(4)]
            ap.ribbon(p, 'Trim', [fin_pt(z, u, side) for u in us], [Vector((0, side, 0))] * 4, SW)

    def fin_at(z):
        t = (z - 1.7) / 4.0
        return -5.6 + (-8.7 + 5.6) * t, 4.6 + (2.1 - 4.6) * t, 0.42 + (0.18 - 0.42) * t
    band = []
    for z in (4.35, 5.05):
        le, c, t = fin_at(z)
        band.append((le + 0.02, 0.0, z, c + 0.04, t * 1.2 * 1.18))
    ap.Wing(band, n=(0, 1, 0)).build(p, 'Player#fin', sides=(1,))
    vp.lathe(p, 'Light', (-8.85, 0.0, 5.72), 'z', [(0.0, 0.0), (0.06, 0.0), (0.06, 0.05), (0.04, 0.09), (0.0, 0.1)], seg=12)
    S = ap.Wing([(-7.7, 0.3, 1.75, 3.1, 0.3 * 1.2), (-8.8, 6.0, 1.85, 1.6, 0.14 * 1.2)], foil=ap.FOIL)
    S.build(p, 'Body#stab', sides=(1, -1))
    for top in (True, False):
        S.chord_line(p, 'Trim', 0.68, 0.9, 5.7, SW, sides=(1, -1), top=top, n=5)


def ramp(p):
    """A rampa aberta: o vão escuro da barriga levantada (o porão), a rampa de chapa nivelada com o piso, os batentes e os trilhos."""
    rings = []
    for x in (-4.4, -5.0, -6.0, -7.0, -8.0, -8.6):
        ths = [deg(a) for a in (236, 245, 253, 261, 270, 279, 287, 295, 304)]
        outer = [tuple(SEC.Q(x, t, 0.025)) for t in ths]
        inner = [tuple(SEC.Q(x, t, -0.03)) for t in reversed(ths)]
        rings.append(outer + inner)
    ap.sheet(p, 'Dark#door', rings, closed=True, smooth=True, caps=(True, True))
    zb = SEC.P(-4.4, deg(270)).z
    p.bm('Body#plate', 0.04)
    vp.rbox(p, 'Body#plate', (-5.8, 0.0, zb - 0.08), (3.0, 2.1, 0.12), r=0.12, k=3)
    for k in range(7):
        vp.rbox(p, 'Trim', (-4.5 - k * 0.42, 0.0, zb - 0.015), (0.05, 1.8, 0.012), r=0.01, k=1)
    for s in (-1, 1):
        vp.tube(p, 'Trim', vp.bent_bar([(-4.4, s * 1.05, zb + 0.35), (-4.4, s * 1.05, zb - 0.1), (-7.3, s * 1.05, zb - 0.1)], R=0.12, n=3),
                0.035, seg=8)
        vp.lathe(p, 'Trim', (-4.4, s * 0.8, zb - 0.08), (0, s, 0), [(0.0, -0.18), (0.07, -0.18), (0.07, 0.18), (0.0, 0.18)], seg=12)


def plane():
    p = Part(NAME)
    fuselage(p)
    fairings(p)
    wings(p)
    nacelles(p)
    tail(p)
    ramp(p)
    return p.finish()


def write_spec():
    import json
    parts = [[NAME, [0, 0, 8.0]]]
    views = [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1400, 1100]],
             ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1600, 600]],
             ['side', 'ortho', [0, 1, 0], [0, 0, 1], [1600, 600]],
             ['three_quarter', 'persp', 30.0, 215.0, [1600, 1000], 35.0, 0.62],
             ['back_three_quarter', 'persp', 25.0, 35.0, [1600, 1000], 35.0, 0.62]]
    spec = {'parts': parts, 'views': views, 'usa_at': [0.0, 0.0, 0], 'tank_views': False,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85]]}
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)


def main():
    clear_scene()
    o = plane()
    t = export(o, OUT)
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([o])
    print('MODEL total', tris(o), 'tris (antes da grade do executor)')
    print('MODEL comprimento %.2f m, envergadura %.2f m, altura %.2f m' % (ax1 - ax0, ay1 - ay0, az1 - az0))
    print('MODEL drop_at_m', DROP_AT)
    write_spec()


main()
