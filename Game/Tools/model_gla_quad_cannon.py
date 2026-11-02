"""O Quad Cannon da GLA (gla_quad_cannon) provisório, modelado no Blender por script (o elenco, 31/10/2026).

Rodar (Blender sem janela, só CPU; não usa a placa):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_gla_quad_cannon.py -- captures/game/models/gla_quad_cannon

O que é (docs/ELENCO.md §5.2): o antiaéreo e anti-infantaria da GLA, 4 metralhadoras pesadas num reparo giratório na
carroceria de um caminhão 4×4 velho. A palavra, de cima: "os 4 canos levantados". A inspiração real: o reparo quádruplo
ZPU-4 (4 metralhadoras de 14,5 mm, o assento do atirador atrás, a alça de mira em anel, as caixas de munição do lado de
fora) montado em cima de um pedestal soldado na carroceria de um caminhão soviético de capô dos anos 50 (o GAZ-63/ZIL-157:
o capô estreito com a grade de barras em pé, os para-lamas soltos e redondos com o farol redondo em cima, a cabine de vidro
reto em 2 folhas). Não é cópia: os 4 canos ficam lado a lado (como a folha da paleta amarela,
docs/concept/palettes/gla_palette_yellow.webp, que o Felipe aprovou: de cima lê "4 canos"; no ZPU-4 real eles ficam 2 em
cima de 2), e o escudo é improvisado de chapas soldadas.
A GLA (CONCEITO_UNIDADES.md §3): o amarelo-areia fosco com as listras verde-escuras gastas (capô, teto, escudo), chapas que
não combinam (a porta da direita e o painel do capô de ferrugem), a carroceria de tábua, a muralha de sacos de areia em U em
volta do reparo, as caixas de munição e o estepe deitado, os galões. A cor do jogador (Team): as 2 bandeirinhas no
para-choque e a bandeira do escudo (gira com o reparo), os rabiscos à mão nas portas e no escudo, o pano amarrado num cano.

Medidas (frente +X, origem no chão no centro): ~6,4 × 2,3 m; o capô a 1,74 m, o teto da cabine a 2,42 m; o reparo no
pedestal a 1,95 m (desenhado e depois crescido 1,12× em volta do pé da mesa: na câmera do jogo ele lia miúdo), o munhão a
~2,57 m e os canos 22° pra cima (a pose de marcha: lê "antiaéreo"); as bocas a ~3,4 m. Rodas:
raio 0,55 m, largura 0,38 m (o pneu militar de chevron), bitola ±0,90 m, eixos em x +2,05 e −1,50. O tanque USA tem
8,1 × 3,8 m e a torre a 2,48 m: o caminhão fica em ~79% dele (unidade de combate leve).

Peças (origem no chão, no centro do chassi, frente em +X; todas no lugar delas):
  SM_GLA_QuadCannon_Hull     tudo que não gira (o caminhão, a carroceria, o pedestal, os sacos, as caixas, o estepe);
  SM_GLA_QuadCannon_Turret   o reparo inteiro (a mesa giratória, as armações, as 4 metralhadoras na pose de marcha, as caixas
                             de munição, o assento, a alça de mira em anel, as manivelas, o escudo e a bandeira): gira em Z em
                             volta de (PIVOT_X, 0) — turret_pivot_m;
  SM_GLA_QuadCannon_Wheels   4 malhas wheel_001..004 (o jogo gira cada uma pelo centro da caixa dela).
Slots: GlaPaint, GlaStripe, Rust, Cloth, Wood, Rubber, Team, Lens, Dark, Metal, Crate.
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

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'gla_quad_cannon')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_GLA_QuadCannon'

COLORS.update({'Team': (78, 152, 58)})

# ---------------------------------------------------------------------------------- medidas
FA, RA = 2.05, -1.50             # os eixos
WHEEL_R, WHEEL_W, WHEEL_Y = 0.55, 0.38, 0.90
FRONT = 3.0                      # a grade
BUMPER = (3.04, 3.18)
COWL = 1.70                      # a frente da cabine (o pé do para-brisa)
CAB_BACK = 0.25
CAB_HW = 1.0
CAB_Z0, BELT, ROOF_Z = 1.02, 1.78, 2.42
HOOD_HW = 0.55
BED_X = (0.12, -3.06)            # a carroceria (frente, trás)
BED_HW = 1.15
BED_FLOOR = 1.36                 # o piso de tábua
SIDE_TOP = 1.78                  # a borda das tábuas laterais
PIVOT_X = -1.50                  # o eixo do reparo (turret_pivot_m)
TT_Z = 1.95                      # o topo do pedestal = o pé da mesa giratória
HINGE = (PIVOT_X + 0.10, 2.50)   # o munhão dos canos (x, z), eixo Y
REST = 22.0                      # os canos levantados na pose de marcha
GUNS_Y = (-0.56, -0.24, 0.24, 0.56)
MUZZLE = 2.05                    # a boca, do munhão ao longo do cano
TS = 1.12                        # (31/10, 2ª volta: na câmera do jogo o reparo lia miúdo) o reparo inteiro 1,12× em volta do pé da mesa


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
    for i in range(11):
        u = i / 10
        bulge = math.sin(math.pi * u) ** 0.45
        w = W / 2 * (0.42 + 0.58 * bulge)
        t = H / 2 * (0.25 + 0.75 * math.sin(math.pi * u) ** 0.7)
        x = L * (u - 0.5) * (1.0 - 0.06 * math.cos(math.pi * u) ** 2)
        ring = []
        for k in range(12):
            th = 2 * math.pi * k / 12
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


# ---------------------------------------------------------------------------------- o caminhão
def hood_top(x):
    return 1.74 - 0.06 * (x - COWL) / (FRONT - COWL)


FENDER_PATH = ([(1.30, 0.97), (1.44, 0.99)] +
               [(FA + 0.66 * math.cos(math.radians(a)), WHEEL_R + 0.66 * math.sin(math.radians(a))) for a in range(140, 22, -9)] +
               [(2.80, 0.86), (2.98, 0.82)])


def fender(p, s):
    """O para-lama solto da frente (o dos caminhões dos anos 50): uma casca em L invertido que sai do estribo, passa por cima
    da roda e desce pra frente; o lado de fora dobra pra baixo (a saia)."""
    sec = [(0.57, 0.0), (1.05, 0.0), (1.13, -0.07), (1.13, -0.17), (1.09, -0.17), (1.09, -0.08), (1.04, -0.04), (0.57, -0.04)]
    rings = []
    P = FENDER_PATH
    for i, (px, pz) in enumerate(P):
        j0, j1 = max(i - 1, 0), min(i + 1, len(P) - 1)
        tx, tz = P[j1][0] - P[j0][0], P[j1][1] - P[j0][1]
        L = math.hypot(tx, tz)
        nx, nz = -tz / L, tx / L
        rings.append([(px + nx * h, yy * s, pz + nz * h) for (yy, h) in sec])
    p.loft('GlaPaint#fender', rings)


def hull():
    p = Part(PREFIX + '_Hull')
    p.bm('GlaPaint#body', 0.035)
    p.bm('GlaPaint#fender', 0.02)
    p.bm('GlaPaint', 0.02)
    p.bm('GlaPaint#thin', 0.006)
    p.bm('GlaStripe', 0.0)
    p.bm('Team', 0.0)
    p.bm('Rust', 0.012)
    p.bm('Metal', 0.008)
    p.bm('Dark', 0.008)
    p.bm('Wood', 0.012)
    p.bm('Lens', 0.0)
    p.bm('Rubber', 0.0)
    p.bm('Crate', 0.02)
    # ------------------------------------------------------------ o chassi (as longarinas, os eixos, o diferencial, as molas)
    for y in (-0.48, 0.48):
        p.box('Dark', -3.02, 3.02, y - 0.08, y + 0.08, 0.80, 1.02)
    for x in (2.7, 1.2, 0.0, -1.0, -2.0, -2.95):
        p.box('Dark', x - 0.05, x + 0.05, -0.48, 0.48, 0.84, 0.98, bevel=0.0)
    for ax in (FA, RA):
        p.cyl('Dark', (ax, 0.0, WHEEL_R), 'y', 0.09, 2 * WHEEL_Y - WHEEL_W, seg=12)
        p.cyl('Dark', (ax, 0.0, WHEEL_R), 'x', 0.2, 0.28, seg=16)
        for s in (-1, 1):
            p.box('Dark', ax - 0.7, ax + 0.7, 0.48 * s - 0.05, 0.48 * s + 0.05, WHEEL_R + 0.1, WHEEL_R + 0.18)    # o feixe de molas
            p.cyl('Dark', (ax + 0.4, 0.6 * s, WHEEL_R + 0.25), (0.3, 0.0, 1.0), 0.04, 0.45, seg=8, bevel=0.0)    # o amortecedor
    p.box('Dark', COWL - 0.1, FRONT - 0.05, -0.50, 0.50, 0.70, 1.06)                       # o motor (aparece pelos vãos)
    # ------------------------------------------------------------ o capô estreito, a grade de barras em pé, as venezianas
    rings = []
    for x in (COWL, (COWL + FRONT) / 2, FRONT - 0.06, FRONT):
        zt = hood_top(x) - (0.03 if x == FRONT else 0.0)
        hw_ = HOOD_HW - (0.02 if x == FRONT else 0.0)
        rings.append([(x, -hw_, 1.02), (x, hw_, 1.02), (x, hw_, zt - 0.10), (x, hw_ - 0.07, zt), (x, -hw_ + 0.07, zt), (x, -hw_, zt - 0.10)])
    p.loft('GlaPaint#body', rings)
    p.box('GlaPaint', FRONT - 0.02, FRONT + 0.04, -0.5, 0.5, 0.98, 1.68)                   # a moldura da grade
    p.box('Dark', FRONT + 0.04, FRONT + 0.05, -0.42, 0.42, 1.04, 1.62, bevel=0.0)
    for k in range(9):
        y = -0.4 + k * 0.1
        p.box('Metal', FRONT + 0.04, FRONT + 0.08, y - 0.022, y + 0.022, 1.04, 1.62, bevel=0.0)
    p.cyl('Dark', (FRONT + 0.06, 0.0, 0.98), 'x', 0.04, 0.06, seg=10, bevel=0.0)           # o furo da manivela
    for s in (-1, 1):                                                                      # as venezianas do lado do capô
        for k in range(6):
            x = 2.10 + k * 0.12
            p.obox('Dark', (x, (HOOD_HW + 0.004) * s, 1.38), (0.04, 0.012, 0.36), bevel=0.0)
    p.box('Rust', 2.0, 2.75, 0.553, 0.56, 1.12, 1.62, bevel=0.0)                            # o painel de ferrugem (não combina)
    # as listras verde-escuras no capô (duas) e a faixa pintada à mão (cor do jogador)
    for yc in (-0.25, 0.25):
        rings = []
        for k in range(9):
            x = COWL + 0.04 + k * (FRONT - 0.1 - COWL - 0.04) / 8
            hw_ = 0.1 * (1.0 if 0 < k < 8 else 0.8)
            z = hood_top(x) + 0.003
            rings.append([(x, yc - hw_, z), (x, yc + hw_, z), (x, yc + hw_, z + 0.006), (x, yc - hw_, z + 0.006)])
        p.loft('GlaStripe', rings, bevel=0.0)
    stroke(p, 'Team', [(1.80, 0.02), (2.2, -0.03), (2.6, 0.02), (2.88, -0.01)], 0.11, zfn=lambda x, y: hood_top(x), lift=0.012, seed=1.5)
    # ------------------------------------------------------------ os para-lamas redondos, os faróis em cima, os estribos
    for s in (-1, 1):
        fender(p, s)
        p.box('Dark', 1.30, 2.95, 0.50 * s - 0.04, 0.50 * s + 0.04, 0.95, 1.06)          # o avental entre o motor e o para-lama
        # o farol redondo na frente do para-lama (a caneca, o aro e a lente) e a grade de arame
        fx, fy, fz = 2.78, 0.82 * s, 1.0
        p.cyl('GlaPaint', (fx, fy, fz), 'x', 0.13, 0.2, seg=24, bevel=0.01, smooth=True)
        p.cyl('Metal', (fx + 0.1, fy, fz), 'x', 0.135, 0.03, seg=24, bevel=0.0)
        p.cyl('Lens', (fx + 0.115, fy, fz), 'x', 0.11, 0.01, seg=24, bevel=0.0)
        for dz in (-0.05, 0.05):
            p.box('Metal', fx + 0.13, fx + 0.145, fy - 0.12, fy + 0.12, fz + dz - 0.008, fz + dz + 0.008, bevel=0.0)
        p.box('Metal', fx - 0.05, fx + 0.05, fy - 0.03, fy + 0.03, fz - 0.2, fz - 0.12, bevel=0.0)
        # o estribo (a chapa do degrau entre o para-lama e a traseira da cabine) e o degrau de baixo
        y0, y1 = sorted((0.62 * s, 1.12 * s))
        p.box('GlaPaint', CAB_BACK - 0.05, 1.32, y0, y1, 0.93, 0.98)
        p.box('Metal', 0.5, 1.05, min(1.02 * s, 1.12 * s), max(1.02 * s, 1.12 * s), 0.62, 0.66, bevel=0.0)
        for x in (0.55, 1.0):
            p.box('Metal', x - 0.02, x + 0.02, min(1.04 * s, 1.08 * s), max(1.04 * s, 1.08 * s), 0.64, 0.93, bevel=0.0)
        # a listra verde-escura em cima do para-lama
        p.loft('GlaStripe', [[(px - 0.0, (0.72 - 0.0) * s, pz + 0.004), (px, 0.92 * s, pz + 0.004), (px, 0.92 * s, pz + 0.01), (px, 0.72 * s, pz + 0.01)]
                             for (px, pz) in FENDER_PATH[3:-2]], bevel=0.0)
    # ------------------------------------------------------------ o para-choque, os ganchos e as 2 bandeirinhas
    p.box('Rust', BUMPER[0], BUMPER[1], -1.08, 1.08, 0.60, 0.80)
    p.box('Dark', BUMPER[0] + 0.02, BUMPER[1] + 0.005, -1.0, 1.0, 0.64, 0.76, bevel=0.0)
    for s in (-1, 1):
        p.box('Metal', BUMPER[1], BUMPER[1] + 0.04, 0.5 * s - 0.05, 0.5 * s + 0.05, 0.62, 0.78, bevel=0.005)
        p.box('Metal', FRONT - 0.15, BUMPER[0] + 0.02, 0.48 * s - 0.06, 0.48 * s + 0.06, 0.62, 0.80, bevel=0.0)
        fx, fy = BUMPER[1] - 0.06, 0.98 * s
        p.cyl('Metal', (fx, fy, 1.45), 'z', 0.014, 1.3, seg=6, bevel=0.0)
        p.box('Dark', fx - 0.04, fx + 0.04, fy - 0.04, fy + 0.04, 0.80, 0.86, bevel=0.0)
        flag(p, (fx, fy), 2.08, 0.55, 0.36, m='Team', phase=1.3 * s)
    # ------------------------------------------------------------ a cabine de vidro reto (2 folhas), as portas, os retrovisores
    p.box('GlaPaint#body', CAB_BACK, COWL, -CAB_HW, CAB_HW, CAB_Z0, BELT)
    gh = [[(COWL - 0.04, -0.98, BELT), (COWL - 0.04, 0.98, BELT), (CAB_BACK, 0.98, BELT), (CAB_BACK, -0.98, BELT)],
          [(COWL - 0.18, -0.94, 2.34), (COWL - 0.18, 0.94, 2.34), (CAB_BACK + 0.02, 0.94, 2.34), (CAB_BACK + 0.02, -0.94, 2.34)],
          [(COWL - 0.26, -0.88, ROOF_Z), (COWL - 0.26, 0.88, ROOF_Z), (CAB_BACK + 0.07, 0.88, ROOF_Z), (CAB_BACK + 0.07, -0.88, ROOF_Z)]]
    p.loft('GlaPaint#body', gh)
    ws = [gh[0][0], gh[0][1], gh[1][1], gh[1][0]]
    panel(p, 'Lens', ws, 0.04, 0.485, 0.1, 0.92, out=(1.0, 0.0, 0.2))
    panel(p, 'Lens', ws, 0.515, 0.96, 0.1, 0.92, out=(1.0, 0.0, 0.2))
    panel(p, 'GlaPaint#thin', ws, 0.0, 1.0, 0.92, 1.0, out=(1.0, 0.0, 0.2), lift=0.0, t=0.07)      # a pala
    for s in (-1, 1):
        sq = [(COWL - 0.04, 0.98 * s, BELT), (CAB_BACK, 0.98 * s, BELT), (CAB_BACK + 0.02, 0.94 * s, 2.34), (COWL - 0.18, 0.94 * s, 2.34)]
        panel(p, 'Lens', sq, 0.10, 0.62, 0.12, 0.9, out=(0.0, s, 0.1))
        panel(p, 'Lens', sq, 0.68, 0.92, 0.12, 0.9, out=(0.0, s, 0.1))
        yo = (CAB_HW + 0.002) * s
        y0, y1 = sorted((yo, yo + 0.008 * s))
        for x in (COWL - 0.06, 0.62):                                                       # as juntas da porta
            p.box('Dark', x - 0.008, x + 0.008, y0, y1, CAB_Z0 + 0.06, BELT, bevel=0.0)
        p.box('Metal', 0.72, 0.86, min(yo, yo + 0.03 * s), max(yo, yo + 0.03 * s), 1.58, 1.62, bevel=0.0)
        # o retrovisor no braço comprido (de caminhão)
        a, b = Vector((COWL - 0.05, 1.0 * s, 1.72)), Vector((COWL + 0.05, 1.22 * s, 1.95))
        p.cyl('Dark', tuple((a + b) / 2), tuple(b - a), 0.016, (b - a).length, seg=6, bevel=0.0)
        p.box('Dark', COWL + 0.02, COWL + 0.08, 1.22 * s - 0.1, 1.22 * s + 0.1, 1.9, 2.18, bevel=0.01)
    # a porta da direita de ferrugem (de outro caminhão) e os rabiscos verdes à mão nas portas
    p.box('Rust', 0.64, COWL - 0.08, CAB_HW + 0.001, CAB_HW + 0.006, CAB_Z0 + 0.07, BELT - 0.01, bevel=0.0)
    for s in (-1, 1):
        O, U, W, N = Vector((0.0, (CAB_HW + 0.006) * s, 0.0)), Vector((1.0, 0.0, 0.0)), Vector((0.0, 0.0, 1.0)), Vector((0.0, s, 0.0))
        loop = [(1.16 + 0.27 * math.cos(math.radians(a)), 1.42 + 0.2 * math.sin(math.radians(a))) for a in range(200, -150, -25)]
        stroke(p, 'Team', loop, 0.07, plane=(O, U, W, N), lift=0.004, seed=3.0 + s)
        stroke(p, 'Team', [(0.72, 1.12), (1.2, 1.4), (1.62, 1.7)], 0.075, plane=(O, U, W, N), lift=0.006, seed=4.0 + s)
        stroke(p, 'Team', [(0.8, 1.66), (0.95, 1.5), (0.86, 1.3)], 0.06, plane=(O, U, W, N), lift=0.008, seed=7.0 + s)
    # as listras do teto
    for yc in (-0.25, 0.25):
        p.box('GlaStripe', CAB_BACK + 0.1, COWL - 0.3, yc - 0.1, yc + 0.1, ROOF_Z, ROOF_Z + 0.006)
    # o tanque de combustível (esquerda, embaixo da porta) e a caixa de ferramentas (direita)
    p.cyl('GlaPaint', (0.75, -0.80, 0.80), 'x', 0.2, 0.9, seg=20, bevel=0.01, smooth=True)
    for dx in (-0.3, 0.3):
        p.cyl('Dark', (0.75 + dx, -0.80, 0.80), 'x', 0.21, 0.04, seg=20, bevel=0.0)
    p.box('GlaPaint', 0.35, 1.15, 0.62, 0.98, 0.66, 0.92)
    p.box('Metal', 0.7, 0.8, 0.98, 0.995, 0.8, 0.86, bevel=0.0)
    # ------------------------------------------------------------ a carroceria de tábua (piso, laterais de abrir, a cabeceira e a grade)
    bx0, bx1 = BED_X[1], BED_X[0]
    p.box('Dark', bx0, bx1, -BED_HW + 0.05, BED_HW - 0.05, 1.12, BED_FLOOR - 0.06)
    for k in range(9):
        y0 = -BED_HW + k * 2 * BED_HW / 9
        p.box('Wood', bx0, bx1, y0 + 0.008, y0 + 2 * BED_HW / 9 - 0.008, BED_FLOOR - 0.06, BED_FLOOR)
    for s in (-1, 1):
        y0, y1 = sorted(((BED_HW - 0.06) * s, BED_HW * s))
        for z0 in (BED_FLOOR, BED_FLOOR + 0.215):
            p.box('Wood', bx0 + 0.04, bx1 - 0.04, y0, y1, z0 + 0.01, z0 + 0.205)
        for x in (bx1 - 0.08, -0.85, -1.85, bx0 + 0.08):                                      # as ferragens (dobradiças e trincos)
            yo0, yo1 = sorted((BED_HW * s, (BED_HW + 0.02) * s))
            p.box('Metal', x - 0.05, x + 0.05, yo0, yo1, BED_FLOOR - 0.05, SIDE_TOP - 0.02, bevel=0.0)
        # o para-lama de trás (a chapa reta em cima da roda, embaixo da carroceria)
        yy0, yy1 = sorted((0.68 * s, 1.12 * s))
        p.box('Metal', RA - 0.62, RA + 0.62, yy0, yy1, 1.13, 1.16, bevel=0.0)
        p.box('Rubber', RA - 0.66, RA - 0.64, yy0, yy1, 0.35, 1.13, bevel=0.0)              # o paralama de borracha
    for z0 in (BED_FLOOR, BED_FLOOR + 0.215):                                                 # a tampa de trás
        p.box('Wood', bx0, bx0 + 0.06, -BED_HW + 0.06, BED_HW - 0.06, z0 + 0.01, z0 + 0.205)
    p.box('GlaPaint', bx1 - 0.08, bx1, -BED_HW, BED_HW, BED_FLOOR, 1.86)                     # a cabeceira de chapa
    for y in (-0.95, -0.5, 0.0, 0.5, 0.95):                                                   # a grade de proteção da cabine
        p.cyl('Metal', (bx1 - 0.04, y, 2.08), 'z', 0.025, 0.44, seg=8, bevel=0.0)
    p.cyl('Metal', (bx1 - 0.04, 0.0, 2.30), 'y', 0.03, 1.96, seg=8, bevel=0.0)
    # a traseira: as lanternas, o engate e o para-choque
    for s in (-1, 1):
        p.box('Dark', bx0 - 0.06, bx0, 0.85 * s - 0.1, 0.85 * s + 0.1, 0.98, 1.1, bevel=0.0)
        p.box('Lens', bx0 - 0.065, bx0 - 0.06, 0.85 * s - 0.08, 0.85 * s + 0.08, 1.0, 1.08, bevel=0.0)
    p.box('GlaPaint', bx0 - 0.04, bx0 + 0.2, -1.0, 1.0, 0.86, 1.0)
    p.cyl('Metal', (bx0 - 0.05, 0.0, 0.92), 'x', 0.06, 0.08, seg=12, bevel=0.0)
    # ------------------------------------------------------------ o pedestal soldado do reparo (em cima, a mesa gira: é da torre)
    px = PIVOT_X
    for (dx, dy) in ((-0.42, -0.42), (-0.42, 0.42), (0.42, -0.42), (0.42, 0.42)):
        p.box('Rust', px + dx - 0.06, px + dx + 0.06, dy - 0.06, dy + 0.06, BED_FLOOR, TT_Z - 0.05)
    for s in (-1, 1):
        for (a, b) in (((px - 0.42, 0.42 * s, BED_FLOOR + 0.05), (px + 0.42, 0.42 * s, TT_Z - 0.1)),
                       ((px - 0.42, -0.42, BED_FLOOR + 0.05 if s > 0 else TT_Z - 0.1), (px - 0.42, 0.42, TT_Z - 0.1 if s > 0 else BED_FLOOR + 0.05))):
            a, b = Vector(a), Vector(b)
            p.cyl('Metal', tuple((a + b) / 2), tuple(b - a), 0.03, (b - a).length, seg=8, bevel=0.0)
    p.box('Rust', px - 0.5, px + 0.5, -0.5, 0.5, TT_Z - 0.06, TT_Z - 0.02)
    p.cyl('Dark', (px, 0.0, TT_Z - 0.035), 'z', 0.6, 0.02, seg=40, bevel=0.0)
    # ------------------------------------------------------------ a muralha de sacos de areia em U, as caixas de munição, o estepe, os galões
    bag_sz = (0.62, 0.36, 0.22)
    for s in (-1, 1):
        for lv in range(2):
            n = 5
            for k in range(n):
                x = (-0.62 if s < 0 else -0.92) - (k + 0.5 * (lv % 2)) * 0.5
                if x < bx0 + 0.42:
                    continue
                bag(p, (x, 0.92 * s, BED_FLOOR + 0.11 + lv * 0.2), bag_sz, yaw=(k * 7 % 9 - 4))
    for lv in range(2):
        for k in range(4):
            y = -0.78 + (k + 0.5 * (lv % 2)) * 0.5
            if y > 0.85:
                continue
            bag(p, (bx0 + 0.28, y, BED_FLOOR + 0.11 + lv * 0.2), bag_sz, yaw=90 + (k * 5 % 7 - 3))
    for (x, y, z, yaw) in ((-2.38, -0.48, BED_FLOOR, 4), (-2.38, -0.48, BED_FLOOR + 0.26, -3), (-2.38, 0.42, BED_FLOOR, 0),
                           (-0.25, -0.62, BED_FLOOR, 2), (-0.25, -0.62, BED_FLOOR + 0.26, -5)):
        fr = Fr((x, y, z), yaw)
        fr.box(p, 'Crate', (0.0, 0.0, 0.125), (0.58, 0.32, 0.25), bevel=0.02)
        fr.box(p, 'Dark', (0.0, 0.0, 0.252), (0.12, 0.2, 0.012), bevel=0.0)
        for dx in (-0.2, 0.2):
            fr.box(p, 'Dark', (dx, 0.0, 0.125), (0.04, 0.33, 0.26), bevel=0.0)
    # o estepe deitado na frente da direita, amarrado
    sx, sy, sz = -0.28, 0.52, BED_FLOOR + 0.19
    st = tyre('spare', sx, sy, sz, 0.5, 0.36, -1, 0.27, style='chevron', n_lugs=22)
    st.data.transform(Matrix.Translation(V(sx, sy, sz)) @ Matrix.Rotation(math.radians(90), 4, 'X') @ Matrix.Translation(-V(sx, sy, sz)))
    rope(p, [(sx - 0.55, sy - 0.2, BED_FLOOR + 0.02), (sx - 0.2, sy - 0.1, sz + 0.2), (sx + 0.2, sy + 0.1, sz + 0.2), (sx + 0.5, sy + 0.45, BED_FLOOR + 0.3)])
    # os galões no suporte do lado de fora da esquerda
    for k, x in enumerate((-2.55, -2.17)):
        fr = Fr((x, -(BED_HW + 0.1), BED_FLOOR + 0.0), 0.0, 0.0, 0.0)
        jerrycan(p, Fr((x, -(BED_HW + 0.1), BED_FLOOR + 0.0), 90.0))
    p.box('Metal', -2.8, -1.95, -(BED_HW + 0.2), -BED_HW, BED_FLOOR - 0.04, BED_FLOOR, bevel=0.0)
    p.box('Metal', -2.8, -1.95, -(BED_HW + 0.2), -(BED_HW + 0.18), BED_FLOOR, BED_FLOOR + 0.25, bevel=0.0)
    o = p.finish()
    return o, st


# ---------------------------------------------------------------------------------- o reparo (gira)
def turret():
    p = Part(PREFIX + '_Turret')
    p.bm('GlaPaint', 0.02)
    p.bm('GlaPaint#thin', 0.006)
    p.bm('GlaStripe', 0.0)
    p.bm('Team', 0.0)
    p.bm('Rust', 0.01)
    p.bm('Metal', 0.006)
    p.bm('Dark', 0.006)
    p.bm('Crate', 0.015)
    px = PIVOT_X
    z0 = TT_Z
    # a mesa giratória, o piso do atirador e as 2 armações laterais até o munhão
    p.cyl('Metal', (px, 0.0, z0 + 0.045), 'z', 0.62, 0.07, seg=40, bevel=0.01)
    p.cyl('Dark', (px, 0.0, z0 + 0.085), 'z', 0.5, 0.01, seg=40, bevel=0.0)
    p.box('GlaPaint', px - 0.95, px + 0.3, -0.5, 0.5, z0 + 0.08, z0 + 0.11)                      # o piso
    for k in range(5):
        p.box('Dark', px - 0.9 + k * 0.22, px - 0.86 + k * 0.22, -0.46, 0.46, z0 + 0.11, z0 + 0.115, bevel=0.0)
    hx, hz = HINGE
    for s in (-1, 1):
        y0, y1 = sorted((0.88 * s, 0.94 * s))
        p.prism_xz('GlaPaint', [(px - 0.45, z0 + 0.08), (px + 0.4, z0 + 0.08), (hx + 0.16, hz - 0.08), (hx + 0.05, hz + 0.1),
                               (hx - 0.12, hz + 0.1), (px - 0.25, z0 + 0.3)], y0, y1)
        p.cyl('Dark', (hx, 0.91 * s, hz), 'y', 0.09, 0.1, seg=16, bevel=0.0)                   # o mancal do munhão
    # o escudo de chapas soldadas (não levanta com os canos): a placa do meio com a fresta, as asas e os remendos
    sx = px + 0.62
    p.obox('GlaPaint', (sx, 0.0, z0 + 0.33), (0.04, 1.26, 0.48), pitch=-10, bevel=0.01)
    for s in (-1, 1):
        c = Vector((sx - 0.17, 0.81 * s, z0 + 0.33))
        p.obox('GlaPaint', tuple(c), (0.04, 0.44, 0.48), yaw=-62 * s, pitch=-10, bevel=0.01)
        p.box('Dark', sx - 0.05, sx + 0.02, 0.62 * s - 0.03, 0.62 * s + 0.03, z0 + 0.1, z0 + 0.5, bevel=0.0)   # o cordão de solda
        p.box('Metal', px - 0.15, sx - 0.02, 0.6 * s - 0.03, 0.6 * s + 0.03, z0 + 0.11, z0 + 0.16, bevel=0.0) # a mão-francesa
    p.obox('Dark', (sx + 0.025, 0.0, z0 + 0.36), (0.01, 0.34, 0.06), pitch=-10, bevel=0.0)        # a fresta de mira
    p.obox('Rust', (sx + 0.03, 0.42, z0 + 0.22), (0.01, 0.34, 0.2), pitch=-10, roll=4, bevel=0.0)   # o remendo
    for (yy, zz) in ((0.28, 0.14), (0.56, 0.14), (0.28, 0.3), (0.56, 0.3)):
        p.obox('Dark', (sx + 0.04, yy, z0 + zz), (0.012, 0.03, 0.03), pitch=-10, bevel=0.0)
    O = Vector((sx + 0.025, 0.0, z0 + 0.30))
    U = Vector((0.0, 1.0, 0.0))
    W = Vector((math.sin(math.radians(-10)) * -1, 0.0, math.cos(math.radians(10))))
    N = Vector((math.cos(math.radians(10)), 0.0, math.sin(math.radians(10))))
    stroke(p, 'GlaStripe', [(-0.62, -0.17), (-0.2, -0.02), (0.2, 0.1), (0.4, 0.17)], 0.12, plane=(O, U, W, N), lift=0.004, seed=2.0)
    stroke(p, 'Team', [(-0.5, 0.12), (-0.28, -0.12), (-0.08, 0.1)], 0.07, plane=(O, U, W, N), lift=0.012, seed=5.0)
    stroke(p, 'Team', [(0.02, -0.15), (0.3, 0.02)], 0.06, plane=(O, U, W, N), lift=0.012, seed=6.0)
    # a bandeira do escudo (no canto da esquerda: gira com o reparo)
    fx, fy = sx - 0.30, -1.0
    p.cyl('Metal', (fx, fy, (z0 + 0.2 + 3.45) / 2), 'z', 0.016, 3.45 - z0 - 0.2, seg=8, bevel=0.0)
    p.box('Dark', fx - 0.03, fx + 0.03, fy - 0.03, fy + 0.06, z0 + 0.3, z0 + 0.36, bevel=0.0)
    flag(p, (fx, fy), 3.42, 0.7, 0.45, m='Team', phase=0.4)
    # o assento do atirador atrás, o encosto, o descanso dos pés e as 2 manivelas (elevação e giro)
    sx2 = px - 0.72
    p.cyl('Metal', (sx2, 0.0, z0 + 0.25), 'z', 0.04, 0.3, seg=10, bevel=0.0)
    p.box('Metal', sx2 - 0.17, sx2 + 0.17, -0.2, 0.2, z0 + 0.4, z0 + 0.43, bevel=0.0)          # o assento: a bacia de chapa
    p.box('Cloth', sx2 - 0.15, sx2 + 0.15, -0.18, 0.18, z0 + 0.43, z0 + 0.48, bevel=0.02)       # a almofada (um saco dobrado)
    for y in (-0.16, 0.16):
        p.cyl('Metal', (sx2 - 0.19, y, z0 + 0.58), (-0.2, 0.0, 1.0), 0.016, 0.34, seg=6, bevel=0.0)
    p.obox('Cloth', (sx2 - 0.22, 0.0, z0 + 0.66), (0.05, 0.3, 0.16), pitch=-12, bevel=0.02)       # o encosto de lona
    p.box('Metal', sx2 + 0.35, sx2 + 0.4, -0.3, 0.3, z0 + 0.12, z0 + 0.16, bevel=0.0)
    for s in (-1, 1):
        p.torus('Metal', (px - 0.32, 0.36 * s, z0 + 0.55), 'y', 0.13, 0.014, seg=18, rseg=6)
        p.cyl('Metal', (px - 0.32, 0.36 * s, z0 + 0.55), 'y', 0.02, 0.1, seg=8, bevel=0.0)
        p.cyl('Dark', (px - 0.32, 0.36 * s, z0 + 0.32), 'z', 0.035, 0.42, seg=8, bevel=0.0)
        p.cyl('Dark', (px - 0.32 + 0.13, (0.36 + 0.06) * s, z0 + 0.55), 'y', 0.015, 0.1, seg=6, bevel=0.0)
    # ------------------------------------------------------------ as 4 metralhadoras, levantadas REST graus no munhão
    g = Fr((hx, 0.0, hz), 0.0, REST, 0.0)
    g.cyl(p, 'Metal', (0.0, 0.0, -0.06), 'y', 0.05, 1.8, seg=14, bevel=0.0)                      # o eixo do munhão
    for s in (-1, 1):
        g.box(p, 'Dark', (-0.08, 0.40 * s, -0.12), (0.7, 0.48, 0.08), bevel=0.01)              # o berço de cada par
    for gy in GUNS_Y:
        g.box(p, 'Dark', (-0.18, gy, 0.0), (0.96, 0.17, 0.21), bevel=0.014)                     # a caixa da culatra
        g.box(p, 'Metal', (-0.22, gy, 0.115), (0.64, 0.12, 0.022), bevel=0.0)                   # a tampa
        g.box(p, 'Metal', (-0.62, gy, 0.0), (0.06, 0.19, 0.15), bevel=0.0)                      # a placa de trás
        g.box(p, 'Metal', (0.0, gy + 0.08 * (1 if gy > 0 else -1), 0.04), (0.14, 0.03, 0.04), bevel=0.0)   # a alavanca de armar
        g.cyl(p, 'Dark', (0.57, gy, 0.0), 'x', 0.066, 0.58, seg=16, bevel=0.0)                  # a camisa perfurada
        for k in range(6):
            g.cyl(p, 'Metal', (0.33 + k * 0.095, gy, 0.0), 'x', 0.071, 0.022, seg=16, bevel=0.0)
        g.cyl(p, 'Dark', (1.4, gy, 0.0), 'x', 0.04, 1.14, seg=12, bevel=0.0)                    # o cano
        g.cyl(p, 'Metal', (1.0, gy, 0.0), 'x', 0.052, 0.06, seg=12, bevel=0.0)                  # a alça de troca do cano
        g.box(p, 'Metal', (1.0, gy, 0.085), (0.05, 0.035, 0.12), bevel=0.0)
        g.cyl(p, 'Dark', (MUZZLE - 0.08, gy, 0.0), 'x', 0.05, 0.16, seg=14, r2=0.062, bevel=0.0)   # o quebra-chamas
        g.cyl(p, 'Dark', (MUZZLE + 0.001, gy, 0.0), 'x', 0.03, 0.004, seg=10, bevel=0.0)
    # as caixas de munição do lado de fora de cada par, com a calha até as armas
    for s in (-1, 1):
        g.box(p, 'Crate', (-0.22, 0.76 * s, -0.05), (0.5, 0.18, 0.36), bevel=0.015)
        g.box(p, 'Dark', (-0.22, 0.76 * s, 0.135), (0.1, 0.12, 0.012), bevel=0.0)
        g.box(p, 'Dark', (-0.1, 0.66 * s, 0.05), (0.18, 0.06, 0.05), bevel=0.0)
    # a alça de mira em anel (a de antiaéreo) no poste do meio, a massa de mira atrás
    g.box(p, 'Dark', (-0.1, 0.0, 0.0), (0.1, 0.06, 0.1), bevel=0.0)
    g.cyl(p, 'Metal', (0.05, 0.0, 0.16), 'z', 0.014, 0.32, seg=6, bevel=0.0)
    g.cyl(p, 'Metal', (0.05, 0.0, 0.34), 'x', 0.02, 0.02, seg=8, bevel=0.0)
    ring_c = g.pt(0.05, 0.0, 0.34)
    # o anel fica em pé, olhando ao longo do cano (o eixo do anel = o eixo da arma)
    rb = Part('_ring')
    rb.torus('Metal', (0.0, 0.0, 0.0), 'x', 0.15, 0.011, seg=28, rseg=6)
    rb.torus('Metal', (0.0, 0.0, 0.0), 'x', 0.075, 0.008, seg=20, rseg=5)
    rb.box('Metal', -0.006, 0.006, -0.15, 0.15, -0.006, 0.006, bevel=0.0)
    rb.box('Metal', -0.006, 0.006, -0.006, 0.006, -0.15, 0.15, bevel=0.0)
    ring_o = rb.finish()
    ring_o.data.transform(Matrix.Translation(V(*ring_c)) @ Matrix.Rotation(math.radians(-REST), 4, 'Y'))
    g.cyl(p, 'Metal', (-0.4, 0.0, 0.26), 'z', 0.01, 0.2, seg=6, bevel=0.0)
    g.cyl(p, 'Metal', (-0.4, 0.0, 0.36), 'x', 0.022, 0.02, seg=8, bevel=0.0)
    # o pano amarrado no cano de fora da direita (a marca da GLA)
    rq = g.pt(1.62, GUNS_Y[-1], 0.0)
    rag(p, tuple(rq), tuple(g.ex), hang=0.3)
    o = p.finish()
    return o, ring_o


# ---------------------------------------------------------------------------------- as rodas, a conferência
def wheels():
    objs = []
    n = 0
    for x in (FA, RA):
        for s in (-1, 1):
            n += 1
            rim = 'GlaPaint' if n == 2 else 'Dark'                            # um aro de outra cor (não combina)
            objs.append(tyre(f'wheel_{n:03d}', x, WHEEL_Y * s, WHEEL_R, WHEEL_R, WHEEL_W, s, 0.29, style='chevron', rim_m=rim, n_lugs=24))
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
    """O reparo girando não pode atravessar o casco (a cabine, a grade, os sacos, as caixas, o estepe): colisão de verdade
    (BVH, triângulo contra triângulo) a cada 5°."""
    def tree(o, M):
        vs = [M @ v.co for v in o.data.vertices]
        return BVHTree.FromPolygons(vs, [tuple(q.vertices) for q in o.data.polygons])
    ht = tree(h, Matrix.Identity(4))
    piv = V(PIVOT_X, 0.0, 0.0)
    hits = []
    for k in range(72):
        Y = Matrix.Translation(piv) @ Matrix.Rotation(math.radians(5 * k), 4, 'Z') @ Matrix.Translation(-piv)
        n = len(ht.overlap(tree(t, Y)))
        if n:
            hits.append((5 * k, n))
    print('MODEL giro do reparo: %s' % ('sem colisao em 360 graus' if not hits else 'COLISAO em %s' % hits[:20]))


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
    t, ring = turret()
    t = join([t, ring], PREFIX + '_Turret')
    piv = V(PIVOT_X, 0.0, TT_Z)
    t.data.transform(Matrix.Translation(piv) @ Matrix.Scale(TS, 4) @ Matrix.Translation(-piv))
    ws = wheels()
    sweep_check(h, t)
    total = export(h, OUT)
    total += export(t, OUT)
    total += export_group(ws, OUT, PREFIX + '_Wheels')
    print('MODEL total', total, 'tris')
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h, t] + ws)
    print('MODEL medidas: comprimento %.3f m (x %.3f..%.3f), largura %.3f m, altura %.3f m (a bandeira do escudo); teto da cabine %.2f'
          % (ax1 - ax0, ax0, ax1, ay1 - ay0, az1, ROOF_Z))
    a = math.radians(REST)
    sc = lambda x, z: (PIVOT_X + (x - PIVOT_X) * TS, TT_Z + (z - TT_Z) * TS)
    hx, hz = sc(*HINGE)
    mx, mz = sc(HINGE[0] + MUZZLE * math.cos(a), HINGE[1] + MUZZLE * math.sin(a))
    print('MODEL turret_pivot_m', [PIVOT_X, 0.0], '(o reparo na escala %.2f)' % TS)
    print('MODEL as 4 bocas (x, y, z) na pose de marcha:', [[round(mx, 3), round(gy * TS, 3), round(mz, 3)] for gy in GUNS_Y],
          'muzzle_height_m', round(mz, 3), '(%.3f m a frente do pivo)' % (mx - PIVOT_X))
    print('MODEL munhao (x, z), eixo Y:', [round(hx, 3), round(hz, 3)], 'pitch de repouso', REST)
    print('MODEL rodas: raio %.3f, largura %.3f, eixos x %s, centro y +-%.3f' % (WHEEL_R, WHEEL_W, [FA, RA], WHEEL_Y))


main()
