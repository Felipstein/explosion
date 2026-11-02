"""O Rocket Buggy da GLA (gla_rocket_buggy) provisório, modelado no Blender por script (o elenco, 31/10/2026).

Rodar (Blender sem janela, só CPU; não usa a placa):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_gla_rocket_buggy.py -- captures/game/models/gla_rocket_buggy

O que é (docs/ELENCO.md §5.2): a artilharia rápida da GLA (o "pesado" dela é o enxame de foguetes). A palavra, de cima: "o
buggy com os 2 casulos de foguete". A inspiração real: o buggy de areia de tubo (o "sand rail" com o motor boxer à mostra
atrás, a gaiola de tubos, os 2 bancos concha, a suspensão de braços longos com as molas à mostra, as rodas de trás grandes
com as pás de areia e as da frente de frisos) e os técnicos de foguete das guerrilhas (os casulos de lançador múltiplo de 107
mm, 12 tubos, soldados num pedestal em cima de qualquer coisa que anda). Não é cópia: os 2 casulos de 6 tubos (3 × 2) ficam
no alto, atrás, num garfo giratório por cima da gaiola (giram 360° sem bater em nada; o atirador não existe, como pedido).
A GLA (CONCEITO_UNIDADES.md §3): os tubos pintados de amarelo-areia, o bico com as listras verde-escuras e o estepe deitado e
amarrado, os galões e o caixote de foguetes de reserva nos lados, o escape de ferrugem. A cor do jogador (Team): a
bandeirinha do chicote (a antena alta de buggy de duna, a flâmula em triângulo), a bandeirinha do casulo da esquerda (gira
com o lançador), a faixa à mão no bico, os rabiscos nos casulos e o pano amarrado na gaiola.

Medidas (frente +X, origem no chão no centro): ~4,6 × 2,3 m; a gaiola a 1,78 m; o lançador no munhão a 2,32 m, os casulos 22°
pra cima (o topo a ~2,75 m: o lançador é a silhueta); o chicote a ~3,1 m. Rodas: a da frente raio 0,40 m × 0,28 m (frisos),
bitola ±0,88 m, eixo em x +1,45; a de trás raio 0,50 m × 0,42 m (pás), bitola ±0,92 m, eixo em x −1,35. O tanque USA tem
8,1 × 3,8 m e a torre a 2,48 m.

Peças (origem no chão, no centro, frente em +X; todas no lugar delas):
  SM_GLA_RocketBuggy_Hull     tudo que não gira (a gaiola, o motor, a suspensão, o bico, os bancos, o pedestal do lançador);
  SM_GLA_RocketBuggy_Turret   o lançador (a mesa, o garfo, o munhão, os 2 casulos de 6 tubos com os foguetes, a bandeirinha):
                              gira em Z em volta de (PIVOT_X, 0) — turret_pivot_m;
  SM_GLA_RocketBuggy_Wheels   4 malhas wheel_001..004 (o jogo gira cada uma pelo centro da caixa dela).
Slots: GlaPaint, GlaStripe, Rust, Cloth, Rubber, Team, Lens, Dark, Metal, Crate.
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, export_group, flag  # noqa: E402
import model_faction_kit  # noqa: E402,F401  (as cores de conferência da GLA entram no COLORS)
import model_vehicle_parts as vp  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'gla_rocket_buggy')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_GLA_RocketBuggy'

COLORS.update({'Team': (78, 152, 58)})

# ---------------------------------------------------------------------------------- medidas
FA, RA = 1.45, -1.35
FR_R, FR_W, FR_Y = 0.40, 0.28, 0.88     # as rodas da frente (frisos)
RR_R, RR_W, RR_Y = 0.50, 0.42, 0.92     # as de trás (pás de areia)
HOOP_X = -0.52                           # o arco principal da gaiola (atrás dos bancos)
CAGE_TOP = 1.75
PIVOT_X = -1.25                          # o eixo do lançador (turret_pivot_m)
TT_Z = 1.50                              # o pé da mesa giratória
HINGE_Z = 2.32                           # o munhão dos casulos (eixo Y)
REST = 22.0                              # os casulos levantados
POD_Y = 0.42                             # o centro de cada casulo (±)
POD_L = 1.15                             # o comprimento dos tubos
TUBE_R, TUBE_DY, TUBE_DZ = 0.072, 0.17, 0.165


# ---------------------------------------------------------------------------------- utilidades (cópias locais: o kit não muda)
def fbox(p, m, c, ex, ey, ez, size, bevel=None):
    """Caixa com os eixos dados (vetores da Unreal): o tamanho (sx, sy, sz) ao longo de ex, ey, ez."""
    b = p.bm(m, bevel if bevel is not None else 0.0)
    r = bmesh.ops.create_cube(b, size=1.0)
    X, Y, Z = (V(*Vector(e).normalized()) for e in (ex, ey, ez))
    C = V(*c)
    M = Matrix(((X.x * size[0], Y.x * size[1], Z.x * size[2], C.x),
                (X.y * size[0], Y.y * size[1], Z.y * size[2], C.y),
                (X.z * size[0], Y.z * size[1], Z.z * size[2], C.z),
                (0.0, 0.0, 0.0, 1.0)))
    bmesh.ops.transform(b, matrix=M, verts=r['verts'])


class Fr:
    """Um referencial local (origem e giro em graus, o mesmo jeito do obox): pra montar uma peça torta inteira."""

    def __init__(self, o, yaw=0.0, pitch=0.0, roll=0.0):
        cy, sy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        cp, sp = math.cos(math.radians(pitch)), math.sin(math.radians(pitch))
        cr, sr = math.cos(math.radians(roll)), math.sin(math.radians(roll))
        self.o = Vector(o)
        self.ex = Vector((cp * cy, cp * sy, sp))
        ey0, ez0 = Vector((-sy, cy, 0.0)), Vector((-sp * cy, -sp * sy, cp))
        self.ey, self.ez = ey0 * cr + ez0 * sr, ez0 * cr - ey0 * sr

    def pt(self, x, y, z):
        return self.o + self.ex * x + self.ey * y + self.ez * z

    def box(self, p, m, c, size, bevel=None):
        fbox(p, m, tuple(self.pt(*c)), self.ex, self.ey, self.ez, size, bevel)

    def cyl(self, p, m, c, axis, r, L, seg=16, r2=None, bevel=None, smooth=False):
        a = {'x': self.ex, 'y': self.ey, 'z': self.ez}[axis]
        p.cyl(m, tuple(self.pt(*c)), tuple(a), r, L, seg=seg, r2=r2, bevel=bevel, smooth=smooth)


def resample(path, step):
    out = [path[0]]
    for (u0, w0), (u1, w1) in zip(path[:-1], path[1:]):
        L = math.hypot(u1 - u0, w1 - w0)
        n = max(1, int(L / step))
        for k in range(1, n + 1):
            out.append((u0 + (u1 - u0) * k / n, w0 + (w1 - w0) * k / n))
    return out


def stroke(p, m, path, width, plane=None, zfn=None, lift=0.006, t=0.005, seed=0.0, step=0.035):
    """A pincelada à mão (a cor do jogador da GLA): uma fita pela polilinha path [(u, w)...], afinando nas pontas e variando a
    largura. plane = (O, U, W, N) (o plano: origem, os 2 eixos e a normal, vetores da Unreal) ou zfn(x, y) (no chão de uma
    superfície quase deitada: u = x, w = y)."""
    pts = resample(path, step)
    n = len(pts)
    rings = []
    for i, (u, w) in enumerate(pts):
        j0, j1 = max(i - 1, 0), min(i + 1, n - 1)
        du, dw = pts[j1][0] - pts[j0][0], pts[j1][1] - pts[j0][1]
        L = math.hypot(du, dw) or 1.0
        pu, pw = -dw / L, du / L
        s = i / max(1, n - 1)
        taper = min(1.0, s / 0.1, (1 - s) / 0.1)
        ww = width * (0.4 + 0.6 * taper) * (1.0 + 0.14 * math.sin(i * 1.9 + seed))
        if zfn:
            N = Vector((0.0, 0.0, 1.0))
            c = Vector((u, w, zfn(u, w) + lift))
            side = Vector((pu, pw, 0.0))
        else:
            O, U, W, N = plane
            c = O + U * u + W * w + N * lift
            side = U * pu + W * pw
        a, b = c + side * ww / 2, c - side * ww / 2
        rings.append([tuple(a), tuple(b), tuple(b + N * t), tuple(a + N * t)])
    p.loft(m, rings, bevel=0.0)


def rope(p, pts, r=0.016, m='Wood#rope'):
    """A corda (31/10, o detalhe AAA): um tubo só pela polilinha, com as dobras redondas (antes eram cilindros soltos com a junta
    aparecendo)."""
    vp.tube(p, m, vp.bent_bar([Vector(q) for q in pts], R=0.04, n=2), r, seg=6)


def lashing(p, c, axis, R, turns=3, r=0.012, m='Wood#rope'):
    """As voltas de corda de uma amarração (em volta de um poste ou de um rolo)."""
    for k in range(turns):
        off = (k - (turns - 1) / 2) * 2.3 * r
        cc = [c[0], c[1], c[2]]
        cc['xyz'.index(axis)] += off
        p.torus(m, tuple(cc), axis, R, r, seg=14, rseg=5)


def rag(p, c, along, hang=0.32, m='Team'):
    """O pano amarrado (a marca da GLA): o nó em volta da barra e as duas pontas caindo com a dobra."""
    x, y, z = c
    if along == 'z':
        p.cyl(m, (x, y, z), 'z', 0.035, 0.07, seg=10, bevel=0.0)
    else:
        p.cyl(m, (x, y, z), along, 0.04, 0.07, seg=10, bevel=0.0)
    for k, (dx, dy, ph) in enumerate(((0.03, 0.02, 0.0), (-0.02, -0.03, 1.7))):
        rings = []
        for i in range(7):
            u = i / 6
            w = 0.07 * (1 - 0.35 * u)
            xx = x + dx + 0.05 * math.sin(u * 2.4 + ph) - 0.06 * u
            yy = y + dy + 0.03 * math.sin(u * 3.0 + ph)
            zz = z - 0.03 - hang * (0.7 + 0.3 * k) * u
            rings.append([(xx - w / 2, yy - 0.004, zz), (xx + w / 2, yy - 0.004, zz), (xx + w / 2, yy + 0.004, zz), (xx - w / 2, yy + 0.004, zz)])
        p.loft(m, rings, bevel=0.0, smooth=True)


def jerrycan(p, fr, m='Crate#can'):
    """O galão de 20 L (0,47 de altura, 0,34 de comprimento, 0,165 de largura) num referencial: o pé em (0, 0, 0). 31/10, o detalhe
    AAA (model_vehicle_parts.jerrycan_frame): o corpo de quinas redondas, o X estampado em relevo dos dois lados, as 3 alças e a tampa."""
    vp.jerrycan_frame(p, fr.o, fr.ex, fr.ey, fr.ez, m=m, cap_m='Dark')


def bag(p, c, size, yaw=0.0, pitch=0.0, roll=0.0, m='Cloth#bag'):
    """O saco de juta cheio (a cópia do model_gla_tank.py): a seção superelipse, estufado no meio, as pontas amarradas."""
    L, W, H = size
    h = (math.sin(c[0] * 12.9898 + c[1] * 78.233 + c[2] * 37.719) * 43758.5453) % 1.0
    L *= 0.94 + 0.12 * h
    H *= 0.9 + 0.2 * ((h * 7.31) % 1.0)
    fr = Fr(c, yaw, pitch, roll)
    rings = []
    for i in range(13):
        u = i / 12
        bulge = math.sin(math.pi * u) ** 0.45
        w = W / 2 * (0.42 + 0.58 * bulge)
        t = H / 2 * (0.25 + 0.75 * math.sin(math.pi * u) ** 0.7)
        x = L * (u - 0.5) * (1.0 - 0.06 * math.cos(math.pi * u) ** 2)
        ring = []
        for k in range(14):
            th = 2 * math.pi * k / 14
            ct, st = math.cos(th), math.sin(th)
            yy = w * math.copysign(abs(ct) ** 0.7, ct)
            zz = t * math.copysign(abs(st) ** 0.7, st)
            if zz < 0:
                zz *= 0.55
            ring.append(tuple(fr.pt(x, yy, zz + 0.12 * H * (1 - bulge))))
        rings.append(ring)
    p.loft(m, rings, bevel=0.0, smooth=True)


def quad_pt(q, u, v):
    a, b, c, d = (Vector(x) for x in q)
    return a * (1 - u) * (1 - v) + b * u * (1 - v) + c * u * v + d * (1 - u) * v


def panel(p, m, q, u0, u1, v0, v1, out, lift=0.004, t=0.008):
    """Uma chapa fina deitada num quadrilátero q (4 cantos em ordem), recortada em [u0, u1] × [v0, v1], afastada pela normal
    (do lado de 'out')."""
    a, b, c, d = quad_pt(q, u0, v0), quad_pt(q, u1, v0), quad_pt(q, u1, v1), quad_pt(q, u0, v1)
    n = (b - a).cross(d - a).normalized()
    if n.dot(Vector(out)) < 0:
        n = -n
    r0 = [x + n * lift for x in (a, b, c, d)]
    p.loft(m, [[tuple(x) for x in r0], [tuple(x + n * t) for x in r0]], bevel=0.0)


def arch_flare(p, m, ax, zc, R, y_in, y_out, w=0.07, a0=8.0, a1=172.0, seg=20):
    """A aba do para-lama em volta do vão da roda: uma fita em arco (no plano xz), de y_in a y_out."""
    rings = []
    for k in range(seg + 1):
        a = math.radians(a0 + (a1 - a0) * k / seg)
        cx, cz = math.cos(a), math.sin(a)
        r0, r1 = R, R + w
        rings.append([(ax + r0 * cx, y_in, zc + r0 * cz), (ax + r1 * cx, y_in, zc + r1 * cz),
                      (ax + r1 * cx, y_out, zc + r1 * cz), (ax + r0 * cx, y_out, zc + r0 * cz)])
    p.loft(m, rings, bevel=0.0)


def tyre(name, x, y, zc, R, W, side, rim_r, style='chevron', rim_m='Dark', n_lugs=24, lug_h=0.04):
    """Uma roda inteira (o jogo gira a malha pelo centro da caixa). 31/10, o detalhe AAA (model_vehicle_parts.tyre): o pneu com o
    FLANCO ABAULADO e o ombro redondo, os blocos do estilo ('at' de lama/estrada, 'chevron' o militar, 'tractor' as garras grandes,
    'paddle' as pás de areia, 'rib' os frisos; 'mud' o V com o ombro), a roda de aço REBAIXADA com a aba, os furos, o cubo saliente
    e o anel de porcas de 4 cm (as de 2 cm sumiam). Os blocos em múltiplo de 4: simétrico em volta do eixo, a caixa no centro."""
    w = Part(name)
    vp.tyre(w, (x, y, zc), R, W, side, rim_r=rim_r, rubber='Rubber', rim=rim_m, hub='Metal', nut_m='Metal', seg=40, lug_h=lug_h,
            style=style, n_lugs=n_lugs, holes_m='Dark', beadlock=style in ('chevron', 'tractor'), nuts=8)
    return w.finish()


def tube(p, m, a, b, r=0.035, seg=10):
    a, b = Vector(a), Vector(b)
    d = b - a
    p.cyl(m, tuple((a + b) / 2), tuple(d), r, d.length + r * 0.6, seg=seg, bevel=0.0)


def mirror_tube(p, m, a, b, r=0.035, seg=10):
    for s in (-1, 1):
        tube(p, m, (a[0], a[1] * s, a[2]), (b[0], b[1] * s, b[2]), r, seg)


def coilover(p, a, b, r=0.045):
    """O amortecedor com a mola à mostra: o corpo, a haste e as espiras."""
    a, b = Vector(a), Vector(b)
    d = (b - a)
    tube(p, 'Metal', a, a + d * 0.55, r * 0.8)
    tube(p, 'Dark', a + d * 0.5, b, r * 0.45)
    n = d.normalized()
    for k in range(7):
        c = a + d * (0.18 + k * 0.1)
        p.cyl('Rust', tuple(c), tuple(n), r * 1.45, 0.022, seg=14, bevel=0.0)


def pennant(p, pole, top, length, height, m='Team', phase=0.0):
    """A flâmula em triângulo do chicote (a bandeirinha do buggy de duna), com a onda."""
    rings = []
    n = 9
    for k in range(n + 1):
        u = k / n
        x = pole[0] - length * u
        y = pole[1] + 0.06 * math.sin(2 * math.pi * u * 1.2 + phase) * u
        h = max(0.008, height * (1 - u))
        z1 = top - 0.03 * u
        z0 = z1 - h
        t = 0.005
        rings.append([(x, y - t, z0), (x, y + t, z0), (x, y + t, z1), (x, y - t, z1)])
    p.loft(m, rings, bevel=0.0, smooth=True)


# ---------------------------------------------------------------------------------- o buggy
def nose_top(x):
    return 0.92 - (x - 0.72) * (0.34 / 1.43)


def hull():
    p = Part(PREFIX + '_Hull')
    p.bm('GlaPaint#body', 0.03)
    p.bm('GlaPaint', 0.0)
    p.bm('GlaStripe', 0.0)
    p.bm('Team', 0.0)
    p.bm('Rust', 0.0)
    p.bm('Metal', 0.0)
    p.bm('Dark', 0.0)
    p.bm('Lens', 0.0)
    p.bm('Cloth', 0.02)
    p.bm('Crate', 0.015)
    # ------------------------------------------------------------ a gaiola de tubos (pintada de areia)
    T = 'GlaPaint'
    mirror_tube(p, T, (2.05, 0.30, 0.42), (-2.05, 0.45, 0.42), 0.04)                  # as longarinas de baixo
    mirror_tube(p, T, (0.72, 0.62, 0.80), (HOOP_X, 0.72, 0.78), 0.035)                # as barras das portas
    mirror_tube(p, T, (0.72, 0.58, 0.40), (HOOP_X, 0.72, 0.40), 0.035)
    for s in (-1, 1):                                                                # o arco principal com as quinas
        tube(p, T, (HOOP_X, 0.72 * s, 0.40), (HOOP_X + 0.03, 0.68 * s, 1.62), 0.042)
        tube(p, T, (HOOP_X + 0.03, 0.68 * s, 1.62), (HOOP_X + 0.04, 0.56 * s, CAGE_TOP), 0.042)
    tube(p, T, (HOOP_X + 0.04, -0.56, CAGE_TOP), (HOOP_X + 0.04, 0.56, CAGE_TOP), 0.042)
    tube(p, T, (HOOP_X + 0.02, -0.68, 1.55), (HOOP_X, 0.70, 0.45), 0.035)            # a diagonal do arco
    tube(p, T, (HOOP_X + 0.02, -0.70, 1.05), (HOOP_X + 0.02, 0.70, 1.05), 0.032)     # a barra dos ombros (os cintos)
    for s in (-1, 1):                                                                # o teto, as colunas da frente, o arco do painel
        tube(p, T, (HOOP_X + 0.04, 0.58 * s, CAGE_TOP), (0.25, 0.60 * s, 1.72), 0.038)
        tube(p, T, (0.25, 0.60 * s, 1.72), (0.72, 0.62 * s, 0.92), 0.038)
        tube(p, T, (0.72, 0.62 * s, 0.92), (0.74, 0.56 * s, 0.38), 0.038)
        tube(p, T, (0.72, 0.62 * s, 0.92), (1.25, 0.40 * s, 0.70), 0.035)              # até a frente do chassi
        tube(p, T, (1.25, 0.40 * s, 0.70), (2.05, 0.30 * s, 0.50), 0.035)
        tube(p, T, (HOOP_X + 0.04, 0.58 * s, CAGE_TOP), (-2.0, 0.55 * s, 0.85), 0.038)  # as escoras de trás
        tube(p, T, (-2.0, 0.55 * s, 0.85), (-2.05, 0.45 * s, 0.42), 0.035)
    tube(p, T, (0.25, -0.62, 1.72), (0.25, 0.62, 1.72), 0.038)
    tube(p, T, (0.72, -0.62, 0.92), (0.72, 0.62, 0.92), 0.035)
    tube(p, T, (-2.0, -0.55, 0.85), (-2.0, 0.55, 0.85), 0.035)
    tube(p, T, (0.25, 0.6, 1.72), (HOOP_X + 0.04, -0.4, CAGE_TOP), 0.03)             # o X do teto
    # a listra verde-escura num tubo do teto e o pano amarrado na gaiola (cor do jogador)
    tube(p, 'GlaStripe', (0.10, -0.605, 1.725), (-0.30, -0.59, 1.738), 0.044)
    rag(p, (0.25, 0.42, 1.72), 'y', hang=0.3)
    # a barra de faróis no teto: 4 faróis redondos de milha
    for y in (-0.42, -0.14, 0.14, 0.42):
        p.cyl('GlaPaint#body', (0.31, y, 1.80), 'x', 0.085, 0.12, seg=18, bevel=0.0)
        p.cyl('Lens', (0.375, y, 1.80), 'x', 0.072, 0.01, seg=18, bevel=0.0)
        p.box('Dark', 0.24, 0.28, y - 0.02, y + 0.02, 1.72, 1.78, bevel=0.0)
    # ------------------------------------------------------------ o assoalho, os bancos concha, o volante, o painel
    p.box('Metal', -0.48, 0.74, -0.58, 0.58, 0.33, 0.36)
    for s in (-1, 1):
        y = 0.36 * s
        p.box('Dark', -0.45, 0.08, y - 0.22, y + 0.22, 0.38, 0.48, bevel=0.0)
        p.box('Cloth', -0.42, 0.06, y - 0.19, y + 0.19, 0.48, 0.55)
        p.obox('Dark', (-0.42, y, 0.85), (0.08, 0.44, 0.7), pitch=-14, bevel=0.0)
        p.obox('Cloth', (-0.37, y, 0.84), (0.05, 0.36, 0.6), pitch=-14)
        for dy in (-0.12, 0.12):                                                    # os cintos
            p.obox('Dark', (-0.33, y + dy, 0.86), (0.012, 0.05, 0.62), pitch=-14, bevel=0.0)
    p.box('Dark', 0.60, 0.70, -0.55, 0.55, 0.70, 0.88, bevel=0.0)                      # o painel
    for y in (-0.48, -0.25, 0.25):
        p.cyl('Metal', (0.595, y, 0.80), 'x', 0.035, 0.012, seg=12, bevel=0.0)
    sw = Vector((0.36, -0.36, 0.92))
    p.torus('Dark', tuple(sw), 'x', 0.16, 0.018, seg=24, rseg=6)
    tube(p, 'Dark', sw, (0.66, -0.36, 0.80), 0.022)
    p.box('Dark', 0.33, 0.39, -0.37, -0.35, 0.78, 1.06, bevel=0.0)
    tube(p, 'Metal', (0.15, 0.0, 0.36), (0.1, 0.0, 0.62), 0.02)                         # a alavanca do câmbio
    # ------------------------------------------------------------ o bico (o capô em cunha), as listras, o estepe deitado, o para-choque
    rings = []
    for x, hw, zt in ((0.72, 0.56, nose_top(0.72)), (1.4, 0.46, nose_top(1.4)), (2.1, 0.34, nose_top(2.1)), (2.18, 0.30, nose_top(2.18) - 0.06)):
        rings.append([(x, -hw, 0.36), (x, hw, 0.36), (x, hw, zt - 0.06), (x, hw - 0.06, zt), (x, -hw + 0.06, zt), (x, -hw, zt - 0.06)])
    p.loft('GlaPaint#body', rings)
    for yc in (-0.2, 0.2):
        p.loft('GlaStripe', [[(x, yc - 0.075, nose_top(x) + 0.003), (x, yc + 0.075, nose_top(x) + 0.003),
                              (x, yc + 0.075, nose_top(x) + 0.008), (x, yc - 0.075, nose_top(x) + 0.008)] for x in (0.78, 2.08)], bevel=0.0)
    stroke(p, 'Team', [(0.8, -0.3), (1.05, 0.0), (1.2, 0.32)], 0.1, zfn=lambda x, y: nose_top(x), lift=0.01, seed=1.0)
    pitch = math.degrees(math.atan2(0.34, 1.43))
    sx = 1.52
    sz = nose_top(sx) + 0.135
    st = tyre('spare', sx, 0.0, sz, 0.36, 0.26, -1, 0.2, style='rib', rim_m='Metal')
    st.data.transform(Matrix.Translation(V(sx, 0.0, sz)) @ Matrix.Rotation(math.radians(pitch), 4, 'Y')
                      @ Matrix.Rotation(math.radians(90), 4, 'X') @ Matrix.Translation(-V(sx, 0.0, sz)))
    for y in (-0.16, 0.16):
        rope(p, [(sx - 0.42, y * 1.8, nose_top(sx - 0.42) + 0.01), (sx - 0.15, y, sz + 0.14), (sx + 0.2, y, sz + 0.06), (sx + 0.42, y * 1.8, nose_top(sx + 0.42) + 0.01)])
    tube(p, 'Metal', (2.24, -0.46, 0.46), (2.24, 0.46, 0.46), 0.045)                    # o para-choque de tubo
    mirror_tube(p, 'Metal', (2.24, 0.42, 0.46), (2.05, 0.30, 0.55), 0.035)
    mirror_tube(p, 'Metal', (2.24, 0.42, 0.46), (2.15, 0.36, 0.30), 0.035)
    p.box('Metal', 1.6, 2.2, -0.3, 0.3, 0.26, 0.30, bevel=0.0)                            # a chapa de proteção de baixo
    p.cyl('Metal', (2.28, 0.0, 0.40), 'y', 0.05, 0.06, seg=10, bevel=0.0)
    # ------------------------------------------------------------ a suspensão da frente (bandejas, molas à mostra) e a de trás (braços longos)
    for s in (-1, 1):
        hub = (FA, (FR_Y - FR_W / 2 - 0.04) * s, FR_R)
        p.box('Dark', FA - 0.06, FA + 0.06, hub[1] - 0.03, hub[1] + 0.03, FR_R - 0.14, FR_R + 0.16, bevel=0.0)   # a manga
        for (z0, z1) in ((0.30, 0.30), (0.62, 0.54)):
            tube(p, 'Metal', (FA - 0.2, 0.40 * s, z0), (FA, hub[1], z1), 0.025)
            tube(p, 'Metal', (FA + 0.2, 0.36 * s, z0), (FA, hub[1], z1), 0.025)
        coilover(p, (FA - 0.04, (hub[1] - 0.12 * s), 0.34), (FA - 0.1, 0.44 * s, 0.88))
        tube(p, 'Dark', (FA + 0.12, 0.30 * s, 0.42), (FA + 0.12, hub[1], 0.42), 0.015)      # a barra de direção
        rhub = (RA, (RR_Y - RR_W / 2 - 0.03) * s, RR_R)
        p.box('Dark', RA - 0.08, RA + 0.08, rhub[1] - 0.04, rhub[1] + 0.02 if s > 0 else rhub[1] + 0.04, RR_R - 0.12, RR_R + 0.12, bevel=0.0)
        tube(p, 'Metal', (-0.55, 0.46 * s, 0.44), (RA, rhub[1], RR_R - 0.04), 0.04)        # o braço longo
        tube(p, 'Metal', (-0.75, 0.40 * s, 0.52), (RA + 0.1, rhub[1] - 0.05 * s, RR_R + 0.02), 0.025)
        coilover(p, (RA + 0.14, (rhub[1] - 0.1 * s), RR_R + 0.04), (-1.05, 0.62 * s, 1.40), r=0.05)
        tube(p, 'Dark', (-1.42, 0.2 * s, 0.50), (RA, rhub[1], RR_R), 0.03)                 # o semieixo
    # ------------------------------------------------------------ o motor boxer à mostra atrás: o cárter, os cilindros aletados, a turbina, o filtro
    p.box('Dark', -1.62, -1.15, -0.22, 0.22, 0.36, 0.64, bevel=0.0)                      # a caixa de câmbio
    p.box('Metal', -2.0, -1.6, -0.25, 0.25, 0.42, 0.80, bevel=0.0)                        # o cárter
    for s in (-1, 1):
        for x in (-1.70, -1.90):
            p.cyl('Dark', (x, 0.40 * s, 0.62), 'y', 0.085, 0.3, seg=16, bevel=0.0)
            for k in range(5):
                p.cyl('Metal', (x, (0.28 + k * 0.055) * s, 0.62), 'y', 0.095, 0.012, seg=16, bevel=0.0)
        p.box('Metal', -2.02, -1.58, 0.55 * s - 0.05, 0.55 * s + 0.05, 0.5, 0.74, bevel=0.0)   # o cabeçote
    p.box('GlaPaint#body', -2.02, -1.62, -0.42, 0.42, 0.80, 1.02)                        # a carcaça da turbina
    p.cyl('GlaPaint#body', (-1.92, 0.0, 1.0), 'x', 0.2, 0.2, seg=24, bevel=0.0)
    p.cyl('Dark', (-2.03, 0.0, 1.0), 'x', 0.15, 0.02, seg=24, bevel=0.0)
    p.cyl('Metal', (-1.70, 0.0, 1.12), 'z', 0.06, 0.2, seg=12, bevel=0.0)                # o carburador e o filtro de ar
    p.cyl('Metal', (-1.70, 0.0, 1.24), 'z', 0.16, 0.08, seg=24, bevel=0.0)
    p.cyl('Dark', (-1.70, 0.0, 1.285), 'z', 0.08, 0.012, seg=16, bevel=0.0)
    # o escape de ferrugem: os coletores, o abafador atravessado, as 2 ponteiras pra cima
    for s in (-1, 1):
        tube(p, 'Rust', (-1.8, 0.56 * s, 0.50), (-2.08, 0.30 * s, 0.56), 0.03)
    p.cyl('Rust', (-2.12, 0.0, 0.56), 'y', 0.1, 0.62, seg=16, bevel=0.0)
    for s in (-1, 1):
        tube(p, 'Rust', (-2.12, 0.22 * s, 0.6), (-2.30, 0.30 * s, 0.82), 0.035)
    tube(p, 'Metal', (-2.25, -0.5, 0.42), (-2.25, 0.5, 0.42), 0.04)                       # o para-choque de trás
    mirror_tube(p, 'Metal', (-2.25, 0.45, 0.42), (-2.05, 0.45, 0.42), 0.035)
    # ------------------------------------------------------------ o pedestal do lançador (a travessa nas escoras de trás e o cubo)
    zb = 1.285
    tube(p, 'GlaPaint', (PIVOT_X, -0.6, zb), (PIVOT_X, 0.6, zb), 0.045)
    p.box('Rust', PIVOT_X - 0.16, PIVOT_X + 0.16, -0.2, 0.2, zb - 0.06, zb + 0.06, bevel=0.0)    # a chapa de solda
    p.cyl('Metal', (PIVOT_X, 0.0, (zb + TT_Z - 0.035) / 2), 'z', 0.12, TT_Z - 0.035 - zb, seg=20, bevel=0.0)
    p.cyl('Dark', (PIVOT_X, 0.0, TT_Z - 0.02), 'z', 0.24, 0.03, seg=28, bevel=0.0)
    for s in (-1, 1):
        tube(p, 'Metal', (PIVOT_X - 0.15, 0.1 * s, TT_Z - 0.04), (-1.85, 0.5 * s, 0.87), 0.025)
    # ------------------------------------------------------------ os galões (esquerda) e o caixote de foguetes de reserva (direita)
    p.box('Metal', -0.85, -0.45, -0.98, -0.74, 0.36, 0.40, bevel=0.0)
    for k, x in enumerate((-0.78, -0.55)):
        jerrycan(p, Fr((x, -0.86, 0.40), 90.0 + 3 * k))
    tube(p, 'Dark', (-0.92, -0.98, 0.72), (-0.40, -0.98, 0.72), 0.012)
    fr = Fr((-0.30, 0.86, 0.40), 0.0)
    fr.box(p, 'Crate', (0.0, 0.0, 0.13), (1.0, 0.26, 0.26), bevel=0.015)
    for dx in (-0.36, 0.36):
        fr.box(p, 'Dark', (dx, 0.0, 0.13), (0.04, 0.27, 0.27), bevel=0.0)
    fr.box(p, 'Dark', (0.0, 0.135, 0.13), (0.3, 0.012, 0.08), bevel=0.0)
    p.box('Metal', -0.85, 0.25, 0.72, 0.98, 0.36, 0.40, bevel=0.0)
    # ------------------------------------------------------------ o chicote de antena com a flâmula (canto de trás da direita)
    wx, wy = -2.22, 0.66
    p.box('Dark', wx - 0.04, wx + 0.04, wy - 0.04, wy + 0.04, 0.80, 0.88, bevel=0.0)
    tube(p, 'Metal', (-2.0, 0.55, 0.85), (wx, wy, 0.84), 0.025)
    p.cyl('Metal', (wx, wy, (0.88 + 3.1) / 2), 'z', 0.008, 3.1 - 0.88, seg=6, bevel=0.0)
    pennant(p, (wx, wy), 3.08, 0.62, 0.34, phase=0.5)
    o = p.finish()
    return o, st


# ---------------------------------------------------------------------------------- o lançador (gira)
def turret():
    p = Part(PREFIX + '_Turret')
    p.bm('GlaPaint', 0.015)
    p.bm('GlaPaint#tube', 0.0)
    p.bm('Crate', 0.006)
    p.bm('Crate#tube', 0.0)
    p.bm('GlaStripe', 0.0)
    p.bm('Team', 0.0)
    p.bm('Rust', 0.0)
    p.bm('Metal', 0.0)
    p.bm('Dark', 0.0)
    px = PIVOT_X
    p.cyl('Metal', (px, 0.0, TT_Z + 0.03), 'z', 0.26, 0.06, seg=32, bevel=0.0)              # a mesa
    p.cyl('Dark', (px, 0.0, TT_Z + 0.062), 'z', 0.2, 0.006, seg=28, bevel=0.0)
    # o garfo: 2 chapas que sobem convergindo até o munhão (passam entre os casulos)
    for s in (-1, 1):
        y0, y1 = sorted((0.07 * s, 0.11 * s))
        p.prism_xz('GlaPaint', [(px - 0.24, TT_Z + 0.06), (px + 0.24, TT_Z + 0.06), (px + 0.1, HINGE_Z - 0.04),
                               (px - 0.1, HINGE_Z - 0.04)], y0, y1)
        p.obox('GlaPaint', (px, 0.17 * s, TT_Z + 0.25), (0.36, 0.03, 0.4), roll=-30 * s, bevel=0.0)
    p.cyl('Metal', (px, 0.0, HINGE_Z), 'y', 0.05, 2 * POD_Y + 0.62, seg=14, bevel=0.0)       # o eixo do munhão
    # o setor dentado da elevação e o macaco (do lado de fora do garfo)
    for s in (-1, 1):
        p.cyl('Dark', (px - 0.02, 0.115 * s, HINGE_Z), 'y', 0.2, 0.012, seg=20, bevel=0.0)
    tube(p, 'Metal', (px - 0.2, 0.0, TT_Z + 0.1), (px - 0.18, 0.0, HINGE_Z - 0.18), 0.035)
    # os 2 casulos de 6 tubos (3 × 2), levantados REST graus
    g = Fr((px, 0.0, HINGE_Z), 0.0, REST, 0.0)
    L = POD_L
    for s in (-1, 1):
        cy = POD_Y * s
        W = 3 * TUBE_DY + 0.06
        H = 2 * TUBE_DZ + 0.06
        # a caixa do casulo: as 4 chapas (aberta na frente e atrás: os tubos aparecem)
        # o casulo da direita veio de outro lançador (o verde-oliva: peça que não combina)
        pm = 'GlaPaint' if s < 0 else 'Crate'
        for (dy, dz, sy, sz) in ((0.0, H / 2, W, 0.022), (0.0, -H / 2, W, 0.022), (W / 2, 0.0, 0.022, H), (-W / 2, 0.0, 0.022, H)):
            g.box(p, pm, (0.0, cy + dy, dz), (L - 0.12, sy, sz), bevel=0.006)
        for xx in (-L / 2 + 0.08, L / 2 - 0.08):                                           # as molduras das pontas
            for (dy, dz, sy, sz) in ((0.0, H / 2 + 0.012, W + 0.03, 0.03), (0.0, -H / 2 - 0.012, W + 0.03, 0.03),
                                     (W / 2 + 0.012, 0.0, 0.03, H + 0.05), (-W / 2 - 0.012, 0.0, 0.03, H + 0.05)):
                g.box(p, 'Metal', (xx, cy + dy, dz), (0.06, sy, sz), bevel=0.0)
        g.box(p, 'Metal', (0.0, cy, -H / 2 - 0.05), (0.5, 0.12, 0.07), bevel=0.0)          # o berço no munhão
        # os 6 tubos, a boca de cada um e o foguete aparecendo em 4 deles
        for i in range(3):
            for j in range(2):
                ty = cy + (i - 1) * TUBE_DY
                tz = (j - 0.5) * TUBE_DZ
                g.cyl(p, 'GlaPaint#tube' if s < 0 else 'Crate#tube', (0.0, ty, tz), 'x', TUBE_R, L, seg=18)
                g.cyl(p, 'Dark', (L / 2 + 0.001, ty, tz), 'x', TUBE_R - 0.012, 0.004, seg=18, bevel=0.0)
                g.cyl(p, 'Dark', (-L / 2 - 0.001, ty, tz), 'x', TUBE_R - 0.012, 0.004, seg=18, bevel=0.0)
                if (i + j + (1 if s > 0 else 0)) % 3 != 0:
                    g.cyl(p, 'Rust', (L / 2 - 0.06, ty, tz), 'x', TUBE_R - 0.022, 0.12, seg=14, r2=0.012, bevel=0.0)
                g.cyl(p, 'Dark', (L / 2 - 0.03, ty, tz), 'x', TUBE_R + 0.006, 0.05, seg=18, bevel=0.0)   # o anel da boca
        # as cintas, a listra verde-escura, o rabisco verde e o cabo de disparo
        for xx in (-0.22, 0.22):
            g.box(p, 'Dark', (xx, cy, 0.0), (0.05, W + 0.05, H + 0.05), bevel=0.0)
        g.box(p, 'GlaStripe', (0.05, cy + s * (W / 2 + 0.013), 0.0), (0.16, 0.006, H - 0.02), bevel=0.0)
        Ov = g.pt(-0.3, cy + s * (W / 2 + 0.016), 0.0)
        stroke(p, 'Team', [(-0.12, -0.08), (0.02, 0.09), (0.12, -0.06)], 0.05, plane=(Ov, g.ex, g.ez, g.ey * s), lift=0.004, seed=2.0 + s)
        g.cyl(p, 'Dark', (-L / 2 - 0.03, cy, -H / 2), 'y', 0.015, 0.2, seg=6, bevel=0.0)
    # a bandeirinha no casulo da esquerda (gira com o lançador)
    fb = g.pt(-L / 2 + 0.1, -POD_Y - 0.25, 2 * TUBE_DZ / 2 + 0.05)
    p.cyl('Metal', (fb.x, fb.y, fb.z + 0.32), 'z', 0.01, 0.64, seg=6, bevel=0.0)
    flag(p, (fb.x, fb.y), fb.z + 0.62, 0.36, 0.24, m='Team', phase=1.0)
    return p.finish()


# ---------------------------------------------------------------------------------- as rodas, a conferência
def wheels():
    objs = []
    n = 0
    for (x, R, W, Y, style, rim) in ((FA, FR_R, FR_W, FR_Y, 'rib', 0.22), (RA, RR_R, RR_W, RR_Y, 'paddle', 0.24)):
        for s in (-1, 1):
            n += 1
            objs.append(tyre(f'wheel_{n:03d}', x, Y * s, R, R, W, s, rim, style=style, rim_m='Metal', n_lugs=16 if style == 'paddle' else 24,
                             lug_h=0.05 if style == 'paddle' else 0.03))
    return objs


def join(objs, name):
    from model_lib import select_only
    select_only(objs[0])
    for o in objs:
        o.select_set(True)
    bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    o.name = name
    return o


def sweep_check(h, t):
    def tree(o, M):
        vs = [M @ v.co for v in o.data.vertices]
        return BVHTree.FromPolygons(vs, [tuple(q.vertices) for q in o.data.polygons])
    ht = tree(h, Matrix.Identity(4))
    piv = V(PIVOT_X, 0.0, 0.0)
    hits = []
    for k in range(72):
        Y = Matrix.Translation(piv) @ Matrix.Rotation(math.radians(5 * k), 4, 'Z') @ Matrix.Translation(-piv)
        pairs = ht.overlap(tree(t, Y))
        if pairs:
            cs = [h.data.polygons[i].center for i, j in pairs]
            box = tuple(round(f(c[ax] * (-1 if ax == 1 else 1) for c in cs), 2) for ax in range(3) for f in (min, max))
            hits.append((5 * k, len(pairs), box))
    print('MODEL giro do lancador: %s' % ('sem colisao em 360 graus' if not hits else 'COLISAO em %s' % hits[:24]))


def bounds(objs):
    xs, ys, zs = [], [], []
    for o in objs:
        for v in o.data.vertices:
            xs.append(v.co.x)
            ys.append(-v.co.y)
            zs.append(v.co.z)
    return (min(xs), max(xs)), (min(ys), max(ys)), (min(zs), max(zs))


def main():
    clear_scene()
    h, spare = hull()
    h = join([h, spare], PREFIX + '_Hull')
    t = turret()
    ws = wheels()
    sweep_check(h, t)
    total = export(h, OUT)
    total += export(t, OUT)
    total += export_group(ws, OUT, PREFIX + '_Wheels')
    print('MODEL total', total, 'tris')
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h, t] + ws)
    _, _, (_, tz1) = bounds([t])
    print('MODEL medidas: comprimento %.3f m (x %.3f..%.3f), largura %.3f m, altura %.3f m (o chicote); o topo do lancador %.3f m'
          % (ax1 - ax0, ax0, ax1, ay1 - ay0, az1, tz1))
    g = Fr((PIVOT_X, 0.0, HINGE_Z), 0.0, REST, 0.0)
    mz = []
    tubes = []
    for s in (-1, 1):
        for i in range(3):
            for j in range(2):
                q = g.pt(POD_L / 2, POD_Y * s + (i - 1) * TUBE_DY, (j - 0.5) * TUBE_DZ)
                tubes.append([round(q.x, 3), round(q.y, 3), round(q.z, 3)])
                mz.append(q.z)
    print('MODEL turret_pivot_m', [PIVOT_X, 0.0])
    print('MODEL as 12 bocas (x, y, z; script, no jogo o y espelha):', tubes)
    print('MODEL muzzle_height_m (a media das bocas) %.3f (de %.3f a %.3f); munhao z %.2f, pitch de repouso %.0f'
          % (sum(mz) / len(mz), min(mz), max(mz), HINGE_Z, REST))
    print('MODEL rodas: frente raio %.2f largura %.2f y +-%.2f x %.2f; tras raio %.2f largura %.2f y +-%.2f x %.2f'
          % (FR_R, FR_W, FR_Y, FA, RR_R, RR_W, RR_Y, RA))


main()
