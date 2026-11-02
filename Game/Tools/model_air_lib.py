"""Peças comuns das aeronaves provisórias da USA (sessão Modelos 3D, 24/10/2026): Raptor, Comanche e Chinook.

Em cima do model_lib.py (coordenadas da Unreal, X frente, Y direita, Z cima, metros; uma bmesh por material).
O que é daqui: o loft com a costura dura (a quina da fuselagem furtiva), as seções (quina, caixa arredondada),
as superfícies de perfil (asa, deriva inclinada), a caixa orientada (pá torcida, vidro na curva), o anel com furo
(o duto do rotor de cauda) e o rotor de pás.
"""
import math

import bmesh
from mathutils import Vector

from model_lib import V

# o perfil simétrico das asas e das derivas: (fração da corda a partir do bordo de ataque, meia espessura relativa)
FOIL = [(0.0, 0.0), (0.05, 0.45), (0.2, 0.9), (0.4, 1.0), (0.7, 0.62), (1.0, 0.06), (1.0, -0.06), (0.7, -0.62),
        (0.4, -1.0), (0.2, -0.9), (0.05, -0.45)]

# a seção furtiva (o F-22 e o Comanche): de cima a quina dos lados, a barriga mais funda; (y, z) normalizados,
# começando na quina da direita e dando a volta por cima. As faces entre (±1, ±0,04) são a quina (dura)
CHINE = [(1.0, 0.04), (0.84, 0.42), (0.58, 0.8), (0.28, 0.97), (0.0, 1.0), (-0.28, 0.97), (-0.58, 0.8), (-0.84, 0.42),
         (-1.0, 0.04), (-1.0, -0.04), (-0.82, -0.5), (-0.48, -0.86), (0.0, -1.0), (0.48, -0.86), (0.82, -0.5), (1.0, -0.04)]
CHINE_HARD = {7, 8, 15}           # os segmentos duros (o da quina e os vizinhos de baixo)


def loft2(p, m, rings, smooth=True, hard=(), caps=(True, True), bevel=0.0):
    """Anéis ligados como o Part.loft, mas com segmentos duros (índice i = do ponto i ao i+1): a quina fica viva."""
    b = p.bm(m, bevel)
    vr = [[b.verts.new(V(*q)) for q in ring] for ring in rings]
    if caps[0]:
        b.faces.new(list(reversed(vr[0])))
    if caps[1]:
        b.faces.new(vr[-1])
    n = len(rings[0])
    for r0, r1 in zip(vr[:-1], vr[1:]):
        for i in range(n):
            j = (i + 1) % n
            f = b.faces.new([r0[i], r0[j], r1[j], r1[i]])
            f.smooth = smooth and i not in hard


def chine_ring(x, w, ht, hb, zc, yc=0.0):
    """A seção furtiva em x: largura w (quina a quina), ht acima da quina, hb abaixo, a quina na altura zc."""
    return [(x, yc + y * w / 2, zc + z * (ht if z > 0 else hb)) for y, z in CHINE]


def round_ring(x, w, h, zc, pexp=3.0, seg=24, yc=0.0, a0=0.0, a1=2 * math.pi):
    """Superelipse (pexp 2 = elipse; 3 = caixa de cantos redondos), de a0 a a1 (0 = lado direito, sobe por cima)."""
    out = []
    full = abs(a1 - a0 - 2 * math.pi) < 1e-6
    for k in range(seg if full else seg + 1):
        t = a0 + (a1 - a0) * k / seg
        c, s = math.cos(t), math.sin(t)
        out.append((x, yc + (w / 2) * math.copysign(abs(c) ** (2 / pexp), c), zc + (h / 2) * math.copysign(abs(s) ** (2 / pexp), s)))
    return out


def surf(p, m, stations, n=(0.0, 0.0, 1.0), mirror=False, smooth=True, foil=FOIL):
    """Asa, estabilizador ou deriva: estações (x do bordo de ataque, y, z, corda, espessura), a espessura ao longo de n.
    mirror: a outra metade em −y (n espelhado)."""
    for s in ((1, -1) if mirror else (1,)):
        nn = Vector((n[0], n[1] * s, n[2]))
        rings = []
        for xl, y, z, c, t in stations:
            rings.append([(xl - u * c + nn.x * f * t / 2, s * y + nn.y * f * t / 2, z + nn.z * f * t / 2) for u, f in foil])
        loft2(p, m, rings, smooth=smooth, hard={5})


def mbox(p, m, c, ex, ey, ez, size, bevel=0.0):
    """Caixa orientada pelos eixos (ex, ey, ez) da Unreal: a pá torcida, o vidro assentado na curva."""
    b = p.bm(m, bevel)
    r = bmesh.ops.create_cube(b, size=1.0)
    ex, ey, ez = Vector(ex).normalized(), Vector(ey).normalized(), Vector(ez).normalized()
    for v in r['verts']:
        w = Vector(c) + ex * (v.co.x * size[0]) + ey * (v.co.y * size[1]) + ez * (v.co.z * size[2])
        v.co = V(w.x, w.y, w.z)


def blade(p, m, hub, d, r0, r1, chord, thick, twist=8.0, tip=None, tip_len=0.0):
    """Uma pá de rotor de r0 a r1 ao longo de d (unitário, no plano do rotor, eixo z), corda e espessura; a torção
    inclina a corda em volta do eixo da pá. tip: o material da ponta (a faixa de cor), com tip_len de comprimento."""
    d = Vector(d).normalized()
    up = Vector((0.0, 0.0, 1.0)) if abs(d.z) < 0.9 else Vector((1.0, 0.0, 0.0))
    side = up.cross(d).normalized()                  # a corda (o bordo de ataque vai pra cá)
    a = math.radians(twist)
    ch = (side * math.cos(a) + up * math.sin(a)).normalized()
    th = d.cross(ch).normalized()
    L = r1 - r0 - tip_len
    mbox(p, m, Vector(hub) + d * (r0 + L / 2), d, ch, th, (L, chord, thick))
    if tip:
        mbox(p, tip, Vector(hub) + d * (r1 - tip_len / 2), d, ch, th, (tip_len, chord, thick * 1.02))


def annulus_y(p, m, c, r_in, ax, az, thick, seg=32, pexp=4.0):
    """O anel com furo de eixo Y (o duto do rotor de cauda): furo redondo r_in, fora uma superelipse ax × az."""
    b = p.bm(m, 0.0)
    rings = []
    for y in (-thick / 2, thick / 2):
        inner, outer = [], []
        for k in range(seg):
            t = 2 * math.pi * k / seg
            cs, sn = math.cos(t), math.sin(t)
            inner.append(b.verts.new(V(c[0] + r_in * cs, c[1] + y, c[2] + r_in * sn)))
            outer.append(b.verts.new(V(c[0] + ax * math.copysign(abs(cs) ** (2 / pexp), cs), c[1] + y,
                                       c[2] + az * math.copysign(abs(sn) ** (2 / pexp), sn))))
        rings.append((inner, outer))
    (i0, o0), (i1, o1) = rings
    for k in range(seg):
        j = (k + 1) % seg
        b.faces.new([i0[k], o0[k], o0[j], i0[j]])
        b.faces.new([i1[j], o1[j], o1[k], i1[k]])
        b.faces.new([o0[k], o1[k], o1[j], o0[j]])
        f = b.faces.new([i0[j], i1[j], i1[k], i0[k]])
        f.smooth = True


def rotor(p, n_blades, radius, chord, thick, hub_r, hub_h, phase=0.0, twist=8.0, droop=0.0, blade_m='Dark',
          hub_m='Metal', tip_m='Yellow', tip_len=0.35, grips=True):
    """O rotor inteiro com origem no centro do cubo (o eixo é o Z da peça): cubo, punhos e as pás."""
    p.cyl(hub_m, (0.0, 0.0, 0.0), 'z', hub_r, hub_h, seg=20, bevel=0.0, smooth=True)
    p.cyl(hub_m, (0.0, 0.0, hub_h / 2 + 0.06), 'z', hub_r * 0.55, 0.12, seg=16, r2=hub_r * 0.25, bevel=0.0, smooth=True)
    for k in range(n_blades):
        a = math.radians(phase + 360.0 * k / n_blades)
        d = Vector((math.cos(a), math.sin(a), -droop))
        if grips:                                     # o punho (a articulação) entre o cubo e a pá
            mbox(p, hub_m, d.normalized() * (hub_r + 0.18), d, Vector((-math.sin(a), math.cos(a), 0.0)), (0.0, 0.0, 1.0),
                 (0.42, chord * 0.7, thick * 2.6))
        blade(p, blade_m, (0.0, 0.0, 0.0), d, hub_r + 0.35, radius, chord, thick, twist=twist, tip=tip_m, tip_len=tip_len)


def wheel(p, c, r, w, axis='y', hub_m='Metal', tyre_m='Rubber'):
    """Roda de avião: o pneu (Rubber) e o cubo (Metal) dos dois lados."""
    p.cyl(tyre_m, c, axis, r, w, seg=20, bevel=0.0, smooth=True)
    p.cyl(hub_m, c, axis, r * 0.55, w + 0.02, seg=14, bevel=0.0)


def strut(p, m, a, b, r, seg=10):
    """Uma haste (o trem de pouso, o suporte) de a até b."""
    a, b = Vector(a), Vector(b)
    d = b - a
    p.cyl(m, tuple((a + b) / 2), (d.x, d.y, d.z), r, d.length, seg=seg, bevel=0.0)
