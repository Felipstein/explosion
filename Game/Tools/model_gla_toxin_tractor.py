"""O Toxin Tractor da GLA (gla_toxin_tractor) provisório, modelado no Blender por script (o elenco, 31/10/2026).

Rodar (Blender sem janela, só CPU; não usa a placa):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_gla_toxin_tractor.py -- captures/game/models/gla_toxin_tractor

O que é (docs/ELENCO.md §5.2, a ideia guardada da GLA): o anti-infantaria que limpa prédio, o trator agrícola velho
carregando o tanque de veneno e espalhando pela barra de pulverização. A palavra, de cima: "o tanque de veneno com a
caveira, no trator". A inspiração real: o trator de roda dos anos 60 (o Belarus MTZ e o Massey Ferguson 35: o capô estreito,
as rodas de trás enormes com as garras em V, os para-lamas em quarto de círculo, o banco de bacia de chapa na mola, os pesos
de mala na frente, o escapamento em pé com a tampinha) com o pulverizador montado nos 3 pontos (o tanque deitado no berço, a
bomba na tomada de força, as mangueiras) e a barra de pulverização com os bicos. Não é cópia: a barra fica na frente, alta,
num mastro giratório com o canhão de jato no meio (a parte que mira: o jogo gira só ela), e o tanque carrega o símbolo de
veneno pintado em cima (o losango com a caveira e os ossos) que lê de cima. A folha da paleta amarela
(docs/concept/palettes/gla_palette_yellow.webp) tem o trator com o tanque claro atrás e as barras abertas: a família.
O tanque é verde-oliva amarelado (slot Crate, o verde-oliva do material militar), o tambor extra de ferrugem no estribo da
esquerda, sem o operador (como pedido).
A GLA (CONCEITO_UNIDADES.md §3): o amarelo-areia fosco com a listra verde-escura no capô e nos para-lamas, os pesos que não
combinam (ferrugem e ferro preto), o braço da direita da barra de outra cor, o remendo parafusado no para-lama. A cor do
jogador (Team): a bandeira no mastro do berço, a faixa torta pintada à mão em volta do tanque, os rabiscos no capô, os panos
amarrados nas pontas da barra e a flâmula no alto do mastro da barra (gira com ela).

Medidas (frente +X, origem no chão no centro): ~5,4 m de comprimento (os pesos na frente até a ponta do tanque), 2,3 m nas
rodas de trás e ~3,0 m com a barra aberta; o capô a 1,55 m, o tanque a 2,5 m no alto, a barra a 2,0 m, a bandeira a ~3,35 m.
Rodas: as de trás raio 0,85 m × 0,46 m (garras em V), bitola ±0,92 m, eixo em x −0,55; as da frente raio 0,46 m × 0,24 m
(frisos), bitola ±0,78 m, eixo em x +1,55. O tanque USA tem 8,1 × 3,8 m e a torre a 2,48 m: o trator fica em ~67% dele.

Peças (origem no chão, no centro, frente em +X; todas no lugar delas):
  SM_GLA_ToxinTractor_Hull     tudo que não gira (o trator, o berço, o tanque, a bomba, as mangueiras, o tambor);
  SM_GLA_ToxinTractor_Turret   a barra de pulverização (o mastro giratório, os 2 braços de treliça com os bicos, o canhão de
                               jato no meio, os cabos, os panos e a flâmula): gira em Z em volta de (PIVOT_X, 0);
  SM_GLA_ToxinTractor_Wheels   4 malhas wheel_001..004 (o jogo gira cada uma pelo centro da caixa dela).
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

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'gla_toxin_tractor')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_GLA_ToxinTractor'

COLORS.update({'Team': (78, 152, 58)})

# ---------------------------------------------------------------------------------- medidas
RA, FA = -0.55, 1.55
RR_R, RR_W, RR_Y = 0.85, 0.46, 0.92      # as rodas de trás (enormes)
FR_R, FR_W, FR_Y = 0.46, 0.24, 0.78      # as da frente
HOOD_X = (0.55, 2.05)
HOOD_HW, HOOD_TOP = 0.36, 1.55
FENDER_R, FENDER_Y = 0.97, (0.64, 1.20)
TANK_X = (-0.95, -2.70)                  # o tanque de veneno (deitado, ao longo de X)
TANK_R, TANK_Z = 0.60, 1.90
PIVOT_X = 2.25                           # o eixo da barra (turret_pivot_m)
SWIVEL_Z = 1.13
BOOM_Z = 2.0
BOOM_HALF = 1.45
DROPS_Y = (0.30, 0.62, 0.94, 1.26)
CANNON_Z = 1.68                          # o canhão passa por cima do capô quando a barra gira
CANNON_PITCH = -8.0


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


def hose(p, pts, r=0.035, m='Dark'):
    """A mangueira: a polilinha suavizada (Chaikin) em tubos."""
    pts = [Vector(q) for q in pts]
    for _ in range(2):
        new = [pts[0]]
        for a, b in zip(pts[:-1], pts[1:]):
            new += [a * 0.75 + b * 0.25, a * 0.25 + b * 0.75]
        new.append(pts[-1])
        pts = new
    for a, b in zip(pts[:-1], pts[1:]):
        tube(p, m, a, b, r, seg=8)


def tank_z(y, r=TANK_R):
    return TANK_Z + math.sqrt(max(r * r - y * y, 0.0))


def conform_decal(p, m, poly, lift, t=0.004, nx=14, ny=8):
    """Um desenho pintado no alto do tanque (um polígono convexo [(x, y)...] visto de cima), colado na curva do cilindro: uma
    grade (fatias em x, pontos em y) com espessura."""
    xs = [q[0] for q in poly]
    x0, x1 = min(xs) + 1e-4, max(xs) - 1e-4
    rings = []
    for i in range(nx + 1):
        x = x0 + (x1 - x0) * i / nx
        ys = []
        n = len(poly)
        for k in range(n):
            (ax, ay), (bx, by) = poly[k], poly[(k + 1) % n]
            if (ax - x) * (bx - x) <= 0 and ax != bx:
                ys.append(ay + (by - ay) * (x - ax) / (bx - ax))
        if len(ys) < 2:
            continue
        ylo, yhi = min(ys), max(ys)
        top = [(x, ylo + (yhi - ylo) * j / ny) for j in range(ny + 1)]
        ring = [(x, y, tank_z(y) + lift) for (x, y) in top] + [(x, y, tank_z(y) + lift + t) for (x, y) in reversed(top)]
        rings.append(ring)
    p.loft(m, rings, bevel=0.0)


def circle_poly(cx, cy, r, n=24, sx=1.0):
    return [(cx + r * sx * math.cos(2 * math.pi * k / n), cy + r * math.sin(2 * math.pi * k / n)) for k in range(n)]


def rect_poly(cx, cy, L, W, ang):
    c, s = math.cos(math.radians(ang)), math.sin(math.radians(ang))
    return [(cx + c * u - s * v, cy + s * u + c * v) for (u, v) in ((-L / 2, -W / 2), (L / 2, -W / 2), (L / 2, W / 2), (-L / 2, W / 2))]


def tank_band(p, m, x0, w, lift=0.004, t=0.005, seed=0.0):
    """A faixa pintada à mão em volta do tanque: torta (o x ondula) e de largura variando."""
    rings = []
    n = 48
    for k in range(n + 1):
        a = 2 * math.pi * k / n
        x = x0 + 0.05 * math.sin(3 * a + seed) + 0.02 * math.sin(7 * a)
        ww = w * (1.0 + 0.18 * math.sin(5 * a + seed * 2))
        r0, r1 = TANK_R + lift, TANK_R + lift + t
        cy, cz = math.cos(a), math.sin(a)
        rings.append([(x - ww / 2, r0 * cy, TANK_Z + r0 * cz), (x + ww / 2, r0 * cy, TANK_Z + r0 * cz),
                      (x + ww / 2, r1 * cy, TANK_Z + r1 * cz), (x - ww / 2, r1 * cy, TANK_Z + r1 * cz)])
    p.loft(m, rings, bevel=0.0)


# ---------------------------------------------------------------------------------- o trator
FENDER_PATH = [(RA + FENDER_R * math.cos(math.radians(a)), RR_R + FENDER_R * math.sin(math.radians(a))) for a in range(162, 14, -8)]


def fender(p, s):
    """O para-lama em quarto de círculo da roda de trás: a casca de cima (a aba de fora dobrada) e a parede de dentro."""
    y_in, y_out = FENDER_Y
    sec = [(y_in, 0.0), (y_out - 0.06, 0.0), (y_out, -0.05), (y_out, -0.13), (y_out - 0.035, -0.13), (y_out - 0.035, -0.06),
           (y_out - 0.08, -0.035), (y_in, -0.035)]
    rings = []
    P = FENDER_PATH
    for i, (px, pz) in enumerate(P):
        j0, j1 = max(i - 1, 0), min(i + 1, len(P) - 1)
        tx, tz = P[j1][0] - P[j0][0], P[j1][1] - P[j0][1]
        L = math.hypot(tx, tz)
        nx, nz = -tz / L, tx / L
        if nz < 0:
            nx, nz = -nx, -nz
        rings.append([(px + nx * h, yy * s, pz + nz * h) for (yy, h) in sec])
    p.loft('GlaPaint#fender', rings)
    # a parede de dentro (entre o para-lama e o assoalho)
    inner = [(P[0][0], 0.98)] + [(x, z - 0.03) for (x, z) in P] + [(P[-1][0], 0.98)]
    y0, y1 = sorted((y_in * s, (y_in + 0.03) * s))
    p.prism_xz('GlaPaint#fender', inner, y0, y1)


def hull():
    p = Part(PREFIX + '_Hull')
    p.bm('GlaPaint#body', 0.03)
    p.bm('GlaPaint#fender', 0.015)
    p.bm('GlaPaint', 0.015)
    p.bm('GlaPaint#thin', 0.005)
    p.bm('GlaStripe', 0.0)
    p.bm('Team', 0.0)
    p.bm('Rust', 0.01)
    p.bm('Metal', 0.006)
    p.bm('Dark', 0.008)
    p.bm('Lens', 0.0)
    p.bm('Crate', 0.0)
    p.bm('Crate#tank', 0.0)
    # ------------------------------------------------------------ o bloco do motor, a caixa de câmbio e os eixos
    p.box('Dark', 0.6, 2.05, -0.27, 0.27, 0.52, 0.98)                                       # o motor (aparece embaixo do capô)
    for k in range(4):
        p.box('Metal', 0.75 + k * 0.32, 0.95 + k * 0.32, -0.30, 0.30, 0.86, 0.94, bevel=0.0)   # os cabeçotes / as tampas
    p.box('Dark', -0.78, 0.62, -0.32, 0.32, 0.44, 1.0)                                      # o câmbio e o diferencial
    for s in (-1, 1):
        p.cyl('Dark', (RA, 0.5 * s, RR_R), 'y', 0.13, 0.4, seg=16)                         # a trombeta do eixo de trás
        p.cyl('Dark', (RA, 0.69 * s, RR_R), 'y', 0.2, 0.06, seg=20)
    p.cyl('Dark', (FA, 0.0, FR_R + 0.02), 'y', 0.07, 2 * FR_Y - FR_W - 0.06, seg=12)          # o eixo da frente
    p.box('Dark', FA - 0.12, FA + 0.12, -0.15, 0.15, FR_R - 0.02, 0.62)                       # o pino do eixo
    for s in (-1, 1):
        p.box('Dark', FA - 0.05, FA + 0.05, (FR_Y - FR_W / 2 - 0.08) * s - 0.03, (FR_Y - FR_W / 2 - 0.08) * s + 0.03, FR_R - 0.1, FR_R + 0.12, bevel=0.0)
        tube(p, 'Metal', (FA - 0.25, 0.2 * s, FR_R + 0.1), (FA - 0.05, (FR_Y - FR_W / 2 - 0.08) * s, FR_R + 0.08), 0.02)   # a barra de direção
    # ------------------------------------------------------------ o capô estreito, a grade, a listra, os faróis, o escape, o pré-filtro
    rings = []
    for x in (HOOD_X[0], HOOD_X[1] - 0.05, HOOD_X[1]):
        zt = HOOD_TOP - (0.03 if x == HOOD_X[1] else 0.0) - 0.04 * (x - HOOD_X[0]) / (HOOD_X[1] - HOOD_X[0])
        hw = HOOD_HW - (0.015 if x == HOOD_X[1] else 0.0)
        rings.append([(x, -hw, 0.96), (x, hw, 0.96), (x, hw, zt - 0.09), (x, hw - 0.08, zt), (x, -hw + 0.08, zt), (x, -hw, zt - 0.09)])
    p.loft('GlaPaint#body', rings)
    hood_z = lambda x: HOOD_TOP - 0.04 * (x - HOOD_X[0]) / (HOOD_X[1] - HOOD_X[0])
    gx = HOOD_X[1]
    p.box('Dark', gx, gx + 0.012, -0.29, 0.29, 0.6, 1.44, bevel=0.0)                         # a grade do radiador
    for k in range(8):
        z = 0.66 + k * 0.1
        p.box('Metal', gx + 0.006, gx + 0.03, -0.28, 0.28, z, z + 0.035, bevel=0.0)
    p.box('GlaPaint', gx - 0.02, gx + 0.035, -0.33, 0.33, 0.58, 0.62)
    p.loft('GlaStripe', [[(x, -0.12, hood_z(x) + 0.003), (x, 0.12, hood_z(x) + 0.003), (x, 0.12, hood_z(x) + 0.008), (x, -0.12, hood_z(x) + 0.008)]
                         for x in (HOOD_X[0] + 0.1, HOOD_X[1] - 0.06)], bevel=0.0)
    for s in (-1, 1):
        O, U, W, N = Vector((0.0, (HOOD_HW + 0.002) * s, 0.0)), Vector((1.0, 0.0, 0.0)), Vector((0.0, 0.0, 1.0)), Vector((0.0, s, 0.0))
        stroke(p, 'Team', [(0.95, 1.08), (1.25, 1.38), (1.55, 1.10), (1.85, 1.36)], 0.07, plane=(O, U, W, N), lift=0.004, seed=1.0 + s)
        for k in range(5):                                                                   # as venezianas do lado
            p.box('Dark', 1.55 + k * 0.08, 1.58 + k * 0.08, (HOOD_HW + 0.001) * s - 0.004, (HOOD_HW + 0.001) * s + 0.004, 1.18, 1.42, bevel=0.0)
        # o farol redondo no lado do capô
        p.cyl('GlaPaint', (1.90, (HOOD_HW + 0.09) * s, 1.32), 'x', 0.085, 0.14, seg=20, bevel=0.0)
        p.cyl('Lens', (1.975, (HOOD_HW + 0.09) * s, 1.32), 'x', 0.07, 0.01, seg=20, bevel=0.0)
        p.box('Metal', 1.84, 1.88, (HOOD_HW - 0.01) * s if s > 0 else -(HOOD_HW + 0.06), (HOOD_HW + 0.06) * s if s > 0 else -(HOOD_HW - 0.01), 1.28, 1.34, bevel=0.0)
    p.box('Rust', 0.7, 1.2, -HOOD_HW - 0.006, -HOOD_HW - 0.001, 1.0, 1.28, bevel=0.0)          # o painel remendado
    # o escapamento em pé com a tampinha de chuva, e o pré-filtro de ar baixo
    ex, ey = 0.66, 0.30
    tube(p, 'Rust', (ex + 0.15, ey - 0.05, 1.05), (ex, ey, 1.25), 0.05)
    p.cyl('Rust', (ex, ey, 1.78), 'z', 0.05, 1.06, seg=14, bevel=0.0)
    p.cyl('Dark', (ex, ey, 1.6), 'z', 0.07, 0.3, seg=14, bevel=0.0)                           # a capa furada
    p.obox('Metal', (ex - 0.03, ey, 2.33), (0.14, 0.12, 0.012), pitch=25, bevel=0.0)
    p.cyl('Metal', (0.72, -0.18, hood_z(0.72) + 0.06), 'z', 0.035, 0.12, seg=10, bevel=0.0)
    p.cyl('Metal', (0.72, -0.18, hood_z(0.72) + 0.14), 'z', 0.07, 0.07, seg=16, bevel=0.0)
    # ------------------------------------------------------------ o suporte da frente com os pesos de mala (alguns de ferrugem) e o flange da barra
    p.box('GlaPaint', HOOD_X[1], PIVOT_X + 0.1, -0.32, 0.32, 0.62, 1.05)
    for k in range(8):
        y = -0.33 + k * 0.094
        m = 'Rust' if k in (1, 5, 6) else 'Dark'
        p.box(m, PIVOT_X + 0.1, PIVOT_X + 0.3, y - 0.04, y + 0.04, 0.55, 0.98, bevel=0.012)
        p.box('Metal', PIVOT_X + 0.16, PIVOT_X + 0.24, y - 0.015, y + 0.015, 0.98, 1.04, bevel=0.0)    # a alça
    p.cyl('Metal', (PIVOT_X + 0.2, 0.0, 0.77), 'y', 0.02, 0.8, seg=8, bevel=0.0)              # a barra que segura os pesos
    p.cyl('Metal', (PIVOT_X, 0.0, 1.085), 'z', 0.17, 0.07, seg=28, bevel=0.0)                # o flange do giro
    p.cyl('Dark', (PIVOT_X, 0.0, 1.06), 'z', 0.12, 0.04, seg=20, bevel=0.0)
    # ------------------------------------------------------------ os para-lamas de trás, o assoalho, o banco de bacia, o volante, as alavancas
    for s in (-1, 1):
        fender(p, s)
        # a listra verde-escura no alto do para-lama e o remendo parafusado (direita)
        p.loft('GlaStripe', [[(px, 0.80 * s, pz + 0.003), (px, 1.02 * s, pz + 0.003), (px, 1.02 * s, pz + 0.009), (px, 0.80 * s, pz + 0.009)]
                             for (px, pz) in FENDER_PATH[4:-4]], bevel=0.0)
    p.obox('Rust', (RA + 0.2, 0.92, RR_R + FENDER_R + 0.015), (0.4, 0.3, 0.012), pitch=-12, bevel=0.0)
    for (dx, dy) in ((-0.15, -0.11), (0.15, -0.11), (-0.15, 0.11), (0.15, 0.11)):
        p.obox('Dark', (RA + 0.2 + dx, 0.92 + dy, RR_R + FENDER_R + 0.025), (0.03, 0.03, 0.012), pitch=-12, bevel=0.0)
    p.box('Metal', -0.62, 0.55, -0.64, 0.64, 0.95, 0.98, bevel=0.0)                          # o assoalho
    for k in range(6):
        p.box('Dark', -0.5 + k * 0.18, -0.46 + k * 0.18, -0.6, 0.6, 0.98, 0.985, bevel=0.0)
    for s in (-1, 1):                                                                        # os degraus
        p.box('Metal', 0.05, 0.4, (0.62) * s - 0.02 if s > 0 else -0.86, 0.86 if s > 0 else -0.62 + 0.02, 0.62, 0.66, bevel=0.0)
        tube(p, 'Metal', (0.1, 0.74 * s, 0.64), (0.1, 0.66 * s, 0.98), 0.018)
        tube(p, 'Metal', (0.35, 0.74 * s, 0.64), (0.35, 0.66 * s, 0.98), 0.018)
    sx = -0.28
    p.box('Dark', sx - 0.04, sx + 0.04, -0.04, 0.04, 0.98, 1.30, bevel=0.0)                  # a mola do banco
    p.obox('Dark', (sx + 0.1, 0.0, 1.16), (0.42, 0.06, 0.04), pitch=35, bevel=0.0)
    p.cyl('Metal', (sx, 0.0, 1.36), 'z', 0.24, 0.05, seg=24, r2=0.27, bevel=0.0)              # a bacia de chapa
    p.obox('Metal', (sx - 0.22, 0.0, 1.52), (0.04, 0.46, 0.26), pitch=-14, bevel=0.0)        # o encosto
    for k in range(5):                                                                       # os furos da bacia
        a = math.radians(k * 72)
        p.cyl('Dark', (sx + 0.13 * math.cos(a), 0.13 * math.sin(a), 1.386), 'z', 0.025, 0.004, seg=8, bevel=0.0)
    p.box('GlaPaint', 0.45, 0.6, -0.30, 0.30, 0.98, 1.58)                                     # o painel (a traseira do capô)
    for y in (-0.15, 0.15):
        p.cyl('Metal', (0.448, y, 1.42), 'x', 0.045, 0.012, seg=14, bevel=0.0)
        p.cyl('Lens', (0.44, y, 1.42), 'x', 0.035, 0.006, seg=14, bevel=0.0)
    col_a, col_b = Vector((0.52, 0.0, 1.5)), Vector((0.20, 0.0, 1.80))
    tube(p, 'Dark', col_a, col_b, 0.025)
    sw = Part('_sw')
    sw.torus('Dark', (0.0, 0.0, 0.0), 'z', 0.21, 0.018, seg=26, rseg=6)
    for k in range(3):
        a = math.radians(90 + k * 120)
        sw.cyl('Dark', (0.1 * math.cos(a), 0.1 * math.sin(a), 0.0), (math.cos(a), math.sin(a), 0.0), 0.012, 0.2, seg=6, bevel=0.0)
    swo = sw.finish()
    d = (col_b - col_a).normalized()
    swo.data.transform(Matrix.Translation(V(*col_b)) @ Vector((0, 0, 1)).rotation_difference(V(*d)).to_matrix().to_4x4())
    for (a, b) in (((0.0, 0.42, 0.98), (0.15, 0.40, 1.45)), ((-0.1, -0.42, 0.98), (0.05, -0.44, 1.38)), ((0.3, 0.2, 0.98), (0.38, 0.22, 1.32))):
        tube(p, 'Metal', a, b, 0.012)
        p.cyl('Dark', b, 'z', 0.03, 0.06, seg=8, bevel=0.0)
    # ------------------------------------------------------------ os 3 pontos, o berço do tanque e a bomba na tomada de força
    for s in (-1, 1):
        tube(p, 'Dark', (-0.72, 0.36 * s, 0.52), (-1.55, 0.44 * s, 0.50), 0.035)            # os braços de baixo
        tube(p, 'Rust', (-0.85, 0.44 * s, 1.27), (-2.62, 0.44 * s, 1.27), 0.045)            # as longarinas do berço
        tube(p, 'Rust', (-0.85, 0.44 * s, 1.27), (-0.66, 0.40 * s, 0.98), 0.04)             # o pé da frente (no câmbio)
        tube(p, 'Rust', (-1.55, 0.44 * s, 0.50), (-1.55, 0.44 * s, 1.27), 0.04)            # o pé de trás (nos braços)
        tube(p, 'Rust', (-1.55, 0.44 * s, 0.6), (-2.5, 0.44 * s, 1.27), 0.03)               # a escora
    tube(p, 'Dark', (-0.70, 0.0, 1.0), (-1.55, 0.0, 1.22), 0.035)                            # o braço de cima
    tube(p, 'Rust', (-1.55, -0.46, 1.27), (-1.55, 0.46, 1.27), 0.04)
    tube(p, 'Rust', (-1.55, -0.46, 0.50), (-1.55, 0.46, 0.50), 0.04)
    for x in (-1.2, -2.42):                                                                  # as selas (os berços curvos)
        for s in (-1, 1):
            tube(p, 'Rust', (x, 0.44 * s, 1.27), (x, 0.3 * s, tank_z(0.3) - 2 * math.sqrt(TANK_R ** 2 - 0.09) + 0.01), 0.035)
        p.loft('Metal', [[(x - 0.04, TANK_R * 1.02 * math.cos(a), TANK_Z - TANK_R * 1.02 * math.sin(a)),
                          (x + 0.04, TANK_R * 1.02 * math.cos(a), TANK_Z - TANK_R * 1.02 * math.sin(a)),
                          (x + 0.04, TANK_R * 1.08 * math.cos(a), TANK_Z - TANK_R * 1.08 * math.sin(a)),
                          (x - 0.04, TANK_R * 1.08 * math.cos(a), TANK_Z - TANK_R * 1.08 * math.sin(a))]
                         for a in [math.radians(20 + 7 * k) for k in range(21)]], bevel=0.0)
    p.box('Metal', -1.0, -0.72, -0.16, 0.16, 0.66, 0.92, bevel=0.01)                          # a bomba
    p.cyl('Dark', (-0.86, 0.0, 0.79), 'y', 0.15, 0.36, seg=20, bevel=0.0)
    tube(p, 'Metal', (-0.72, 0.0, 0.62), (-0.55, 0.0, 0.58), 0.03)                            # o eixo da tomada de força
    # ------------------------------------------------------------ o tanque de veneno (deitado), as cintas, a boca, a caveira pintada em cima
    tx0, tx1 = TANK_X[1], TANK_X[0]
    L = tx1 - tx0
    p.cyl('Crate#tank', ((tx0 + tx1) / 2, 0.0, TANK_Z), 'x', TANK_R, L - 0.12, seg=44, bevel=0.0, smooth=True)
    for (x, d) in ((tx1 - 0.06, 1), (tx0 + 0.06, -1)):                                        # os tampos abaulados
        p.cyl('Crate#tank', (x + d * 0.04, 0.0, TANK_Z), 'x', TANK_R, 0.08, r2=TANK_R * 0.82, seg=44, bevel=0.0, smooth=True) if d > 0 else \
            p.cyl('Crate#tank', (x + d * 0.04, 0.0, TANK_Z), 'x', TANK_R * 0.82, 0.08, r2=TANK_R, seg=44, bevel=0.0, smooth=True)
        p.cyl('Crate#tank', (x + d * 0.10, 0.0, TANK_Z), 'x', TANK_R * 0.82, 0.04, r2=TANK_R * 0.6, seg=44, bevel=0.0) if d > 0 else \
            p.cyl('Crate#tank', (x + d * 0.10, 0.0, TANK_Z), 'x', TANK_R * 0.6, 0.04, r2=TANK_R * 0.82, seg=44, bevel=0.0)
        p.cyl('Rust', (x, 0.0, TANK_Z), 'x', TANK_R + 0.006, 0.02, seg=44, bevel=0.0)        # o cordão de solda
    for x in (-1.2, -2.42):                                                                  # as cintas
        p.cyl('Dark', (x, 0.0, TANK_Z), 'x', TANK_R + 0.012, 0.06, seg=44, bevel=0.0)
    p.cyl('Rust', ((tx0 + tx1) / 2, 0.0, TANK_Z), 'x', TANK_R + 0.005, 0.015, seg=44, bevel=0.0)
    # a boca de encher em cima, a tampa com os parafusos, o respiro e o visor de nível
    mx = -1.35
    p.cyl('Metal', (mx, 0.0, TANK_Z + TANK_R + 0.03), 'z', 0.2, 0.1, seg=28, bevel=0.0)
    p.cyl('Metal', (mx, 0.0, TANK_Z + TANK_R + 0.09), 'z', 0.22, 0.03, seg=28, bevel=0.0)
    for k in range(8):
        a = math.radians(k * 45)
        p.cyl('Dark', (mx + 0.19 * math.cos(a), 0.19 * math.sin(a), TANK_Z + TANK_R + 0.11), 'z', 0.015, 0.02, seg=6, bevel=0.0)
    tube(p, 'Metal', (mx - 0.3, 0.22, TANK_Z + TANK_R - 0.01), (mx - 0.3, 0.22, TANK_Z + TANK_R + 0.22), 0.025)
    p.cyl('Metal', (mx - 0.3, 0.22, TANK_Z + TANK_R + 0.25), 'z', 0.045, 0.05, seg=12, bevel=0.0)
    p.box('Metal', tx0 + 0.25, tx0 + 0.31, -TANK_R - 0.03, -TANK_R + 0.02, TANK_Z - 0.35, TANK_Z + 0.25, bevel=0.0)
    p.box('Lens', tx0 + 0.26, tx0 + 0.30, -TANK_R - 0.035, -TANK_R - 0.03, TANK_Z - 0.3, TANK_Z + 0.2, bevel=0.0)
    # o losango de veneno com a caveira e os ossos (lê de cima): a borda escura, o campo claro, a caveira escura
    hx = -2.08
    D = 0.5
    dia = [(hx + D, 0.0), (hx, D), (hx - D, 0.0), (hx, -D)]
    din = [(hx + D - 0.09, 0.0), (hx, D - 0.09), (hx - D + 0.09, 0.0), (hx, -D + 0.09)]
    for k in range(4):                                                                       # a borda (4 trapézios)
        conform_decal(p, 'Dark', [dia[k], dia[(k + 1) % 4], din[(k + 1) % 4], din[k]], lift=0.004, nx=8, ny=4)
    conform_decal(p, 'GlaPaint', din, lift=0.003, nx=16, ny=10)
    conform_decal(p, 'Dark', circle_poly(hx + 0.07, 0.0, 0.15, sx=0.95), lift=0.007, nx=10, ny=8)        # o crânio
    conform_decal(p, 'Dark', rect_poly(hx - 0.07, 0.0, 0.1, 0.17, 0.0), lift=0.007, nx=4, ny=6)          # o queixo
    for ey in (-0.06, 0.06):
        conform_decal(p, 'GlaPaint#thin', circle_poly(hx + 0.08, ey, 0.04, n=12), lift=0.010, nx=6, ny=4)  # os olhos
    for ang in (35, -35):                                                                    # os ossos cruzados
        conform_decal(p, 'Dark', rect_poly(hx - 0.04, 0.0, 0.6, 0.05, ang), lift=0.006, nx=10, ny=3)
        for e in (-1, 1):
            ca, sa = math.cos(math.radians(ang)), math.sin(math.radians(ang))
            for off in (-0.03, 0.03):
                cx = hx - 0.04 + e * 0.3 * ca - off * sa
                cy = e * 0.3 * sa + off * ca
                conform_decal(p, 'Dark', circle_poly(cx, cy, 0.04, n=12), lift=0.006, nx=5, ny=4)
    # a faixa torta pintada à mão em volta do tanque (cor do jogador)
    tank_band(p, 'Team', -2.6, 0.12, seed=0.7)
    tank_band(p, 'Team', -1.62, 0.07, seed=2.1)
    # ------------------------------------------------------------ as mangueiras: do tanque à bomba, da bomba pela direita até o giro da barra
    hose(p, [(-1.6, 0.25, TANK_Z - TANK_R + 0.02), (-1.3, 0.22, 1.05), (-1.0, 0.1, 0.84)])
    hose(p, [(-0.75, 0.15, 0.82), (-0.3, 0.42, 0.9), (0.6, 0.42, 0.96), (1.6, 0.36, 0.98), (2.05, 0.30, 1.0), (PIVOT_X - 0.02, 0.2, 1.0), (PIVOT_X, 0.08, 1.02)])
    for x in (0.0, 1.0, 1.9):                                                                # as braçadeiras
        p.cyl('Metal', (x, 0.42 if x < 1.5 else 0.33, 0.95), 'x', 0.045, 0.03, seg=10, bevel=0.0)
    # ------------------------------------------------------------ o tambor extra de ferrugem no estribo da esquerda, amarrado, com a mangueira
    dx, dy = 0.78, -0.66
    p.box('Metal', dx - 0.33, dx + 0.33, dy - 0.3, dy + 0.3, 0.55, 0.59, bevel=0.0)
    barrel_ = Part('_drum')
    barrel_.bm('Rust', 0.012)
    barrel_.cyl('Rust', (dx, dy, 0.59 + 0.44), 'z', 0.29, 0.88, seg=24, bevel=0.012, smooth=True)
    for dz in (0.29, 0.59):
        barrel_.cyl('Dark', (dx, dy, 0.59 + dz), 'z', 0.305, 0.04, seg=24, bevel=0.0)
    barrel_.cyl('Metal', (dx + 0.12, dy - 0.1, 0.59 + 0.89), 'z', 0.04, 0.03, seg=10, bevel=0.0)
    drum = barrel_.finish()
    p.cyl('Dark', (dx, dy, 1.2), 'z', 0.302, 0.05, seg=24, bevel=0.0)                        # a cinta no capô
    hose(p, [(dx + 0.12, dy - 0.1, 1.5), (dx - 0.2, dy + 0.1, 1.6), (0.3, -0.3, 1.1), (-0.6, -0.2, 0.9), (-0.82, -0.1, 0.84)], r=0.025)
    # ------------------------------------------------------------ a bandeira no mastro do berço (frente da direita)
    fx, fy = -0.9, 0.5
    tube(p, 'Metal', (fx, fy, 1.27), (fx, fy, 3.35), 0.02)
    p.box('Dark', fx - 0.04, fx + 0.04, fy - 0.07, fy + 0.04, 1.27, 1.33, bevel=0.0)
    flag(p, (fx, fy), 3.33, 0.85, 0.55, m='Team', phase=0.3)
    o = p.finish()
    return [o, swo, drum]


# ---------------------------------------------------------------------------------- a barra de pulverização (gira)
def turret():
    p = Part(PREFIX + '_Turret')
    p.bm('GlaPaint', 0.01)
    p.bm('Rust', 0.0)
    p.bm('Metal', 0.0)
    p.bm('Dark', 0.0)
    p.bm('Team', 0.0)
    px = PIVOT_X
    p.cyl('Metal', (px, 0.0, SWIVEL_Z + 0.035), 'z', 0.15, 0.07, seg=28, bevel=0.0)          # o anel do giro
    p.cyl('Dark', (px, 0.0, SWIVEL_Z + 0.08), 'z', 0.09, 0.03, seg=16, bevel=0.0)
    p.cyl('GlaPaint', (px, 0.0, (SWIVEL_Z + 0.07 + BOOM_Z) / 2), 'z', 0.065, BOOM_Z - SWIVEL_Z - 0.07, seg=16, bevel=0.0)   # o mastro
    p.box('Metal', px - 0.1, px + 0.1, -0.13, 0.13, BOOM_Z - 0.12, BOOM_Z + 0.06, bevel=0.01)       # a cabeça do T
    tube(p, 'Metal', (px, 0.0, BOOM_Z + 0.06), (px, 0.0, 2.38), 0.022)                        # o poste dos cabos
    # os 2 braços de treliça (o da direita é de ferrugem: de outro pulverizador), o cano de pulverizar e os bicos
    for s in (-1, 1):
        m = 'GlaPaint' if s < 0 else 'Rust'
        top_a, top_b = (px, 0.1 * s, BOOM_Z + 0.04), (px, BOOM_HALF * s, BOOM_Z)
        bot_a, bot_b = (px, 0.1 * s, BOOM_Z - 0.10), (px, BOOM_HALF * s, BOOM_Z - 0.03)
        tube(p, m, top_a, top_b, 0.036)
        tube(p, m, bot_a, bot_b, 0.03)
        for k, y in enumerate((0.25, 0.55, 0.85, 1.15, 1.40)):
            t0 = y / BOOM_HALF
            za = BOOM_Z + 0.04 - 0.04 * t0
            zb = BOOM_Z - 0.10 + 0.07 * t0
            tube(p, m, (px, y * s, za), (px, y * s, zb), 0.016, seg=6)
            if k < 4:
                y2 = (y + 0.30) / BOOM_HALF
                tube(p, m, (px, y * s, zb), (px, (y + 0.3) * s, BOOM_Z + 0.04 - 0.04 * y2), 0.014, seg=6)
        tube(p, 'Metal', (px, 0.0, 2.36), (px, BOOM_HALF * s, BOOM_Z + 0.01), 0.007, seg=6)    # o cabo
        tube(p, 'Dark', (px + 0.06, 0.08 * s, BOOM_Z - 0.14), (px + 0.06, (BOOM_HALF - 0.04) * s, BOOM_Z - 0.10), 0.026, seg=8)   # o cano
        for y in DROPS_Y:
            zt = BOOM_Z - 0.14 + 0.04 * y / BOOM_HALF
            tube(p, 'Dark', (px + 0.05, y * s, zt), (px + 0.07, y * s, zt - 0.17), 0.014, seg=6)
            p.cyl('Metal', (px + 0.075, y * s, zt - 0.2), (0.4, 0.0, -1.0), 0.012, 0.06, seg=8, r2=0.026, bevel=0.0)   # o bico
        rag(p, (px, (BOOM_HALF - 0.08) * s, BOOM_Z + 0.0), 'y', hang=0.28)                    # o pano na ponta
    # o canhão de jato no meio (a parte que mira): o tubo, a boca em funil, as alças
    g = Fr((px + 0.06, 0.0, CANNON_Z), 0.0, CANNON_PITCH, 0.0)
    g.cyl(p, 'Metal', (0.16, 0.0, 0.0), 'x', 0.045, 0.34, seg=14)
    g.cyl(p, 'Dark', (0.0, 0.0, 0.0), 'x', 0.07, 0.1, seg=14)
    g.cyl(p, 'Metal', (0.38, 0.0, 0.0), 'x', 0.05, 0.12, seg=16, r2=0.075)
    g.cyl(p, 'Dark', (0.445, 0.0, 0.0), 'x', 0.06, 0.012, seg=16)
    for s in (-1, 1):
        g.cyl(p, 'Metal', (0.12, 0.09 * s, 0.05), 'z', 0.012, 0.14, seg=6, bevel=0.0)
        g.cyl(p, 'Dark', (0.12, 0.09 * s, 0.13), 'z', 0.022, 0.05, seg=8, bevel=0.0)
    tube(p, 'Dark', (px, 0.06, CANNON_Z - 0.18), (px + 0.08, 0.04, CANNON_Z - 0.02), 0.03)     # a mangueira do mastro ao canhão
    # a flâmula no alto do poste dos cabos (gira com a barra)
    flag(p, (px, 0.0), 2.38, 0.4, 0.24, m='Team', phase=0.8)
    return p.finish()


# ---------------------------------------------------------------------------------- as rodas, a conferência
def wheels():
    objs = []
    n = 0
    for (x, R, W, Y, style, rim, nl, lh, rm) in ((FA, FR_R, FR_W, FR_Y, 'rib', 0.25, 24, 0.03, 'GlaPaint'),
                                                (RA, RR_R, RR_W, RR_Y, 'tractor', 0.47, 20, 0.07, 'GlaPaint')):
        for s in (-1, 1):
            n += 1
            objs.append(tyre(f'wheel_{n:03d}', x, Y * s, R, R, W, s, rim, style=style, rim_m=('Rust' if n == 3 else rm), n_lugs=nl, lug_h=lh))
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
    print('MODEL giro da barra: %s' % ('sem colisao em 360 graus' if not hits else 'COLISAO em %s' % hits[:24]))


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
    h = join(hull(), PREFIX + '_Hull')
    t = turret()
    ws = wheels()
    sweep_check(h, t)
    total = export(h, OUT)
    total += export(t, OUT)
    total += export_group(ws, OUT, PREFIX + '_Wheels')
    print('MODEL total', total, 'tris')
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h, t] + ws)
    (hx0, hx1), (hy0, hy1), _ = bounds([h] + ws)
    print('MODEL medidas: comprimento %.3f m (x %.3f..%.3f), largura %.3f m com a barra (%.3f sem), altura %.3f m (a bandeira); tanque no alto %.2f'
          % (ax1 - ax0, ax0, ax1, ay1 - ay0, hy1 - hy0, az1, TANK_Z + TANK_R))
    g = Fr((PIVOT_X + 0.06, 0.0, CANNON_Z), 0.0, CANNON_PITCH, 0.0)
    m = g.pt(0.45, 0.0, 0.0)
    print('MODEL turret_pivot_m', [PIVOT_X, 0.0])
    print('MODEL a boca do canhao de jato (x, y, z):', [round(m.x, 3), 0.0, round(m.z, 3)], 'muzzle_height_m', round(m.z, 3),
          '(%.3f m a frente do pivo)' % (m.x - PIVOT_X))
    print('MODEL os bicos da barra: y +-%s, z ~%.2f, x %.2f (relativo ao pivo: +0.075)' % (list(DROPS_Y), BOOM_Z - 0.36, PIVOT_X + 0.075))
    print('MODEL o simbolo de veneno: o losango no alto do tanque, centro x %.2f, 1,0 m de diagonal' % -2.08)
    print('MODEL rodas: tras raio %.2f largura %.2f y +-%.2f x %.2f; frente raio %.2f largura %.2f y +-%.2f x %.2f'
          % (RR_R, RR_W, RR_Y, RA, FR_R, FR_W, FR_Y, FA))
    print('MODEL bandeira: o topo do mastro', [-0.9, 0.5, 3.35], '(script; no jogo o y espelha)')


main()
