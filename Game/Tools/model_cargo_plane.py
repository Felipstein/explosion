"""O avião de carga da USA provisório (o do Supply Drop Zone), modelado no Blender por script (sessão Modelos 3D, 22/10/2026).

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_cargo_plane.py -- <pasta de saída>

O cargueiro que passa sobre o Drop Zone a cada 2 min e solta as caixas de paraquedas pela rampa de trás (no ZH é o
C-130 do Supply Drop Zone e do paraquedista). A base real é o C-130 Hercules (29,8 m de comprimento, 40,4 m de
envergadura): asa alta reta, 4 turboélices, a cauda levantada com a rampa de carga, as carenagens do trem de pouso
nos lados da barriga. Escala 0,74 pra caber na câmera do RTS (~30 m de envergadura, o pedido): o que se lê de cima é
a cruz da asa comprida com as 4 hélices e a cauda em T invertido. A rampa vai aberta (é o avião do lançamento).

Uma peça só (origem no centro do avião, no eixo da fuselagem embaixo da asa; coordenadas da Unreal, X frente, Y direita):
  SM_USA_CargoPlane: fuselagem (o radome do nariz em cinza-escuro, para-brisa), asa com as 4 naceles e as hélices de 4 pás
  (paradas: a peça é única), a deriva com a faixa azul (cor do jogador) e as pontas de asa azuis, os estabilizadores,
  as carenagens do trem, a rampa aberta, as luzes de navegação e as sirenes (cima e barriga).
DROP_AT = de onde as caixas saem (atrás da rampa), no referencial do avião.
Materiais (os slots das construções, import_provisional.py): Body, Trim, Dark, Glass, Player, Light, Beacon.
"""
import math
import os
import sys

import bmesh
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'cargo_plane')
os.makedirs(OUT, exist_ok=True)
NAME = 'SM_USA_CargoPlane'

COLORS.update({'Body': (176, 180, 178), 'Trim': (70, 70, 68), 'Dark': (40, 40, 38), 'Glass': (30, 50, 80),
               'Player': (86, 108, 150), 'Light': (255, 230, 170), 'Beacon': (255, 170, 40)})

# a fuselagem: estações (x, largura, altura, centro z); o topo é reto, o nariz cai e a barriga sobe na cauda
FUS = [(11.0, 0.25, 0.25, 0.05), (10.75, 1.3, 1.25, 0.0), (10.3, 2.2, 2.15, 0.1), (9.6, 2.85, 2.85, 0.2),
       (8.6, 3.2, 3.25, 0.3), (7.0, 3.3, 3.3, 0.3), (-3.2, 3.3, 3.3, 0.3), (-5.0, 3.2, 2.85, 0.52),
       (-7.0, 2.8, 2.05, 0.9), (-9.0, 2.05, 1.35, 1.25), (-10.6, 1.0, 0.75, 1.45), (-11.2, 0.3, 0.3, 1.5)]
PEXP = 2.6                        # o expoente da seção (superelipse: redonda com os lados retos)
SPAN = 30.0
WING_ROOT_LE, WING_TIP_LE = 2.3, 1.7
WING_ROOT_C, WING_TIP_C = 3.7, 2.0
WING_Z, DIHEDRAL = 2.05, 0.35     # o meio da asa na raiz e quanto sobe até a ponta
WING_T = (0.55, 0.22)
NACELLES = (5.4, 9.6)
PROP_R = 1.55
DROP_AT = (-6.6, 0.0, -1.6)
# o perfil da asa (fração da corda a partir do bordo de ataque, fração da espessura)
FOIL = [(0.0, 0.0), (0.04, 0.42), (0.15, 0.75), (0.35, 0.85), (0.65, 0.6), (1.0, 0.06), (1.0, -0.04), (0.65, -0.25),
        (0.35, -0.38), (0.15, -0.35), (0.04, -0.24)]


def fus_at(x):
    """(largura, altura, centro z) da fuselagem em x, interpolado entre as estações."""
    for (xa, wa, ha, za), (xb, wb, hb, zb) in zip(FUS[:-1], FUS[1:]):
        if xb <= x <= xa:
            t = (xa - x) / (xa - xb)
            return wa + (wb - wa) * t, ha + (hb - ha) * t, za + (zb - za) * t
    return FUS[-1][1:] if x < FUS[-1][0] else FUS[0][1:]


def se(c):
    return math.copysign(abs(c) ** (2.0 / PEXP), c)


def fus_point(x, th, out=0.0):
    w, h, zc = fus_at(x)
    y, z = (w / 2) * se(math.cos(th)), (h / 2) * se(math.sin(th))
    n = Vector((0.0, y / max(w / 2, 1e-3) ** 2, z / max(h / 2, 1e-3) ** 2))
    n = n.normalized() if n.length > 1e-6 else Vector((0.0, 0.0, 1.0))
    return Vector((x, y, zc + z)) + n * out


def ring_at(x, seg=24):
    return [tuple(fus_point(x, 2 * math.pi * k / seg)) for k in range(seg)]


def mbox(p, m, c, ex, ey, ez, size, bevel=0.0):
    """Caixa orientada pelos eixos (ex, ey, ez) dados no referencial da Unreal (a pá torcida, o vidro na curva)."""
    b = p.bm(m, bevel)
    r = bmesh.ops.create_cube(b, size=1.0)
    for v in r['verts']:
        lx, ly, lz = v.co.x * size[0], v.co.y * size[1], v.co.z * size[2]
        w = Vector(c) + Vector(ex) * lx + Vector(ey) * ly + Vector(ez) * lz
        v.co = V(w.x, w.y, w.z)


def foil_ring(le, chord, mid, t, axis):
    """Um perfil: axis 'y' = o perfil no plano x-z (asa, estabilizador); 'z' = no plano x-y (a deriva)."""
    out = []
    for u, f in FOIL:
        x = le - u * chord
        d = f * t * 0.6
        out.append((x, mid[0], mid[1] + d) if axis == 'y' else (x, mid[0] + d, mid[1]))
    return out


def surface(p, m, stations, axis, mirror):
    """Asa, estabilizador ou deriva por estações [(posição, bordo de ataque, corda, espessura, altura)...]."""
    for s in ((1, -1) if mirror else (1,)):
        rings = []
        for pos, le, chord, t, h in stations:
            if axis == 'y':
                rings.append(foil_ring(le, chord, (s * pos, h), t, 'y'))
            else:
                rings.append(foil_ring(le, chord, (h, pos), t, 'z'))
        p.loft(m, rings, bevel=0.0, smooth=True)


def wing_le(y):
    return WING_ROOT_LE + (WING_TIP_LE - WING_ROOT_LE) * abs(y) / (SPAN / 2)


def wing_c(y):
    return WING_ROOT_C + (WING_TIP_C - WING_ROOT_C) * abs(y) / (SPAN / 2)


def wing_z(y):
    return WING_Z + DIHEDRAL * abs(y) / (SPAN / 2)


def wing_t(y):
    return WING_T[0] + (WING_T[1] - WING_T[0]) * abs(y) / (SPAN / 2)


def plane():
    p = Part(NAME)
    # a fuselagem: o nariz do radar escuro e o resto, lisos
    xs = [s[0] for s in FUS]
    nose = [x for x in xs if x >= 10.3]
    body = [x for x in xs if x <= 10.3]
    p.loft('Trim#nose', [ring_at(x) for x in nose], bevel=0.0, smooth=True)
    p.loft('Body#fus', [ring_at(x) for x in body], bevel=0.0, smooth=True)
    # o para-brisa e as janelas laterais da cabine: placas finas assentadas na curva do nariz
    for (x, th, sx, sy) in ((9.55, 72, 0.6, 0.62), (9.55, 108, 0.6, 0.62), (9.35, 40, 0.55, 0.5), (9.35, 140, 0.55, 0.5),
                            (8.75, 35, 0.45, 0.42), (8.75, 145, 0.45, 0.42)):
        t = math.radians(th)
        c = fus_point(x, t, 0.012)
        tx = (fus_point(x + 0.05, t) - fus_point(x - 0.05, t)).normalized()
        tt = (fus_point(x, t + 0.02) - fus_point(x, t - 0.02)).normalized()
        n = tx.cross(tt).normalized()
        if n.dot(c - Vector((x, 0.0, fus_at(x)[2]))) < 0:
            n = -n
        mbox(p, 'Glass', c, tx, tt, n, (sx, sy, 0.03))
    # a carenagem da asa no teto e as do trem de pouso nos lados da barriga
    p.box('Body', -1.2, 2.0, -1.15, 1.15, 1.6, 2.2, bevel=0.25)
    for s in (-1, 1):
        a, b = sorted((s * 1.25, s * 1.95))
        p.box('Body', -2.8, 1.8, a, b, -1.32, -0.3, bevel=0.3)
    # a asa (as duas metades), a ponta azul (cor do jogador) e as luzes de navegação
    ys = [0.3, 1.6, NACELLES[0], NACELLES[1], SPAN / 2]            # a raiz começa dentro da fuselagem (as metades não se tocam)
    surface(p, 'Body#wing', [(y, wing_le(y), wing_c(y), wing_t(y), wing_z(y)) for y in ys], 'y', True)
    y0 = SPAN / 2 - 0.9
    surface(p, 'Player#tip', [(y, wing_le(y) + 0.015, wing_c(y) + 0.03, wing_t(y) * 1.2, wing_z(y)) for y in (y0, SPAN / 2 + 0.02)], 'y', True)
    for s in (-1, 1):
        y = s * (SPAN / 2 + 0.05)
        p.box('Light', wing_le(SPAN / 2) - 0.5, wing_le(SPAN / 2) - 0.25, min(y, y + s * 0.06), max(y, y + s * 0.06), wing_z(SPAN / 2) - 0.05, wing_z(SPAN / 2) + 0.05, bevel=0.0)
    # as 4 naceles com o cubo, a hélice de 4 pás torcidas, a entrada de ar e o escapamento
    for s in (-1, 1):
        for yn in NACELLES:
            y = s * yn
            le = wing_le(yn)
            zc = wing_z(yn) - 0.3
            rings = []
            for (dx, w, h, dz) in ((2.1, 0.55, 0.6, 0.0), (1.8, 0.95, 1.05, 0.0), (0.8, 1.0, 1.2, -0.05), (-1.0, 0.95, 1.1, 0.0), (-2.6, 0.45, 0.5, 0.15)):
                rings.append([(le + dx, y + (w / 2) * se(math.cos(2 * math.pi * k / 16)), zc + dz + (h / 2) * se(math.sin(2 * math.pi * k / 16))) for k in range(16)])
            p.loft('Body#nac', rings, bevel=0.0, smooth=True)
            p.cyl('Body', (le + 2.45, y, zc), 'x', 0.3, 0.7, seg=16, r2=0.04, bevel=0.0, smooth=True)          # o cubo
            p.box('Dark', le + 1.5, le + 2.0, y - 0.22, y + 0.22, zc - 0.62, zc - 0.42, bevel=0.03)            # a entrada de ar
            p.box('Dark', le - 1.3, le - 0.6, y + s * 0.3 - 0.1, y + s * 0.3 + 0.1, zc + 0.42, zc + 0.6, bevel=0.02)   # o escapamento
            hub = Vector((le + 2.2, y, zc))
            for k in range(4):
                ph = math.radians(45 + 90 * k)
                d = Vector((0.0, math.cos(ph), math.sin(ph)))
                tang = Vector((0.0, -math.sin(ph), math.cos(ph)))
                beta = math.radians(28)
                chord = (tang * math.cos(beta) + Vector((1.0, 0.0, 0.0)) * math.sin(beta)).normalized()
                th = d.cross(chord).normalized()
                L = PROP_R - 0.2
                mbox(p, 'Dark', hub + d * (0.2 + L / 2), d, chord, th, (L, 0.3, 0.05))
    # a deriva com a faixa azul (cor do jogador), os estabilizadores
    fin = [(1.7, -5.6, 4.6, 0.42, 0.0), (3.7, -7.15, 3.35, 0.3, 0.0), (5.7, -8.7, 2.1, 0.18, 0.0)]
    surface(p, 'Body#fin', fin, 'z', False)

    def fin_at(z):
        t = (z - 1.7) / 4.0
        return -5.6 + (-8.7 + 5.6) * t, 4.6 + (2.1 - 4.6) * t, 0.42 + (0.18 - 0.42) * t
    band = []
    for z in (4.35, 5.05):
        le, c, t = fin_at(z)
        band.append((z, le + 0.02, c + 0.04, t * 1.3, 0.0))
    surface(p, 'Player#fin', band, 'z', False)
    p.box('Light', -8.95, -8.75, -0.05, 0.05, 5.62, 5.72, bevel=0.0)                                   # a luz de cauda
    surface(p, 'Body#stab', [(0.3, -7.7, 3.1, 0.3, 1.75), (6.0, -8.8, 1.6, 0.14, 1.85)], 'y', True)
    # a rampa de carga aberta (nivelada com o piso) e o vão escuro da barriga levantada
    rings = []
    for x in (-4.4, -5.0, -6.0, -7.0, -8.0, -8.6):
        ths = [math.radians(a) for a in (236, 253, 270, 287, 304)]
        outer = [tuple(fus_point(x, t, 0.025)) for t in ths]
        inner = [tuple(fus_point(x, t, -0.03)) for t in reversed(ths)]
        rings.append(outer + inner)
    p.loft('Dark#door', rings, bevel=0.0)
    zb = fus_point(-4.4, math.radians(270)).z
    p.box('Body', -7.3, -4.3, -1.05, 1.05, zb - 0.14, zb - 0.02, bevel=0.03)
    for s in (-1, 1):
        p.box('Trim', -7.3, -4.4, s * 1.05 - 0.03, s * 1.05 + 0.03, zb - 0.14, zb + 0.35, bevel=0.0)          # os batentes
    # as sirenes (em cima da carenagem da asa e na barriga)
    p.cyl('Beacon', (0.2, 0.0, 2.3), 'z', 0.1, 0.18, seg=12, bevel=0.0, smooth=True)
    p.cyl('Beacon', (0.6, 0.0, -1.42), 'z', 0.1, 0.16, seg=12, bevel=0.0, smooth=True)
    return p.finish()


def main():
    clear_scene()
    o = plane()
    tris = export(o, OUT)
    xs = [c[0] for c in o.bound_box]
    ys = [c[1] for c in o.bound_box]
    zs = [c[2] for c in o.bound_box]
    print('MODEL total', tris, 'tris; comprimento %.2f m, envergadura %.2f m, altura %.2f m' % (max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs)))
    print('MODEL drop_at_m', DROP_AT)


main()
