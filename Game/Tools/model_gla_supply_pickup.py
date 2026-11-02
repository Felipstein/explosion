"""A picape da carga da GLA (gla_supply_pickup) provisória, modelada no Blender por script (o elenco, 31/10/2026).

Rodar (Blender sem janela, só CPU; não usa a placa):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_gla_supply_pickup.py -- captures/game/models/gla_supply_pickup

O que é (docs/ELENCO.md §5.2 e §7.4): a coleta da GLA, mais barata e mais frágil que o caminhão da USA. A palavra, de cima:
"a picape velha carregada". A mesma família da Technical aprovada (docs/concept/units/gla_technical_sheet.webp, a picape 1:
cabine simples quadrada, capô reto, para-lamas com a aba, quebra-mato de cano, o santo-antônio atrás da cabine) e da paleta
amarela (docs/concept/palettes/gla_palette_yellow.webp: as duas listras verde-escuras no capô e no teto), mas CIVIL (sem
arma nem blindagem) e REMENDADA: a porta da esquerda de outra picape (ferrugem), o remendo de chapa parafusado no para-lama
de trás, a tampa com o remendo, o para-choque da frente de sucata (uma viga I cortada a maçarico e o quebra-mato de cano
soldado), o de trás de uma viga de madeira, uma roda de aro diferente. A inspiração real: a Toyota Hilux de cabine simples
dos anos 80 que as guerrilhas carregam até o teto (o bagageiro de tubo por cima do para-brisa com o estepe deitado e os
galões, as tábuas nas laterais da caçamba amarradas nas estacas, o snorkel no pé do para-brisa).

A carga: a caçamba tem o espaço pras 4 caixas em 2 × 2 (a caixa pequena do caminhão da USA, SM_USA_SupplyCrate_Small,
1,08 × 0,94 × 0,72 m, origem no pé); as vagas (cargo.slots_m, o centro do pé de cada caixa) saem impressas no fim. As caixas
ficam acima das tábuas (lê "carregada" de lado) e abaixo do teto da cabine.
A lona e as cordas: a lona enrolada e amarrada em cima do santo-antônio, as amarras de corda das tábuas nas estacas, o rolo
de corda pendurado no santo-antônio e as cordas que prendem o estepe e os galões no bagageiro.
A cor do jogador (slot Team, CONCEITO_UNIDADES.md §3: rabiscos e listras à mão, bandeirinhas e panos amarrados): a bandeira
no mastro do santo-antônio, a seta pintada à mão no capô (lê de cima), o rabisco na porta da direita, a faixa torta na
tampa, o pano amarrado no retrovisor e na antena.

Medidas (frente +X, origem no chão no centro): ~5,26 × 2,06 m; capô a 1,21 m, teto da cabine a 1,87 m (o bagageiro com o
estepe até ~2,2 m), o mastro da bandeira a ~3,05 m. Rodas: raio 0,43 m, largura 0,30 m, bitola ±0,82 m, entre-eixos 3,08 m
(eixos em x +1,66 e −1,42). O tanque USA tem 8,1 × 3,8 m e a torre a 2,48 m: a picape fica em ~65% dele (apoio).

Peças (origem no chão, no centro do chassi, frente em +X; todas no lugar delas):
  SM_GLA_SupplyPickup_Hull    tudo que não gira;
  SM_GLA_SupplyPickup_Wheels  4 malhas wheel_001..004 (uma por roda: o jogo gira cada uma pelo centro da caixa dela, o raio
                              é a meia altura da caixa e o lado de dentro da caixa é a borracha).
Slots: GlaPaint, GlaStripe, Rust, Cloth, Wood, Rubber, Team, Lens, Dark, Metal, Crate (os galões verde-oliva).
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, export_group, flag  # noqa: E402
import model_faction_kit  # noqa: E402,F401  (as cores de conferência da GLA entram no COLORS)
import model_vehicle_parts as vp  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'gla_supply_pickup')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_GLA_SupplyPickup'

COLORS.update({'Team': (78, 152, 58)})       # o verde da GLA (o jogo pinta pela cor do jogador)

# ---------------------------------------------------------------------------------- medidas
FA, RA = 1.66, -1.42             # os eixos (x)
WHEEL_R, WHEEL_W, WHEEL_Y = 0.43, 0.30, 0.82
ARCH_R = 0.52                    # o vão da roda (em volta do centro da roda)
SILL = 0.60                      # a soleira (o pé da lataria)
HW = 0.97                        # meia-largura da frente e da cabine
BELT = 1.22                      # a cintura (o capô junto do para-brisa, o pé dos vidros)
FRONT = 2.32                     # a cara (a grade)
COWL = 1.12                      # o pé do para-brisa
CAB_BACK = -0.24
ROOF_Z = 1.87
BED_X0, BED_IN0, BED_IN1, BED_X1 = -0.28, -0.34, -2.56, -2.60   # a caçamba: fora, dentro, dentro, fora
BED_IN_HW, BED_HW = 0.96, 1.02
BED_FLOOR = 0.99                 # o piso de tábua (= z das vagas)
BED_TOP = 1.42                   # a borda das laterais
CRATE = (1.08, 0.94, 0.72)
SLOTS = [(BED_IN0 - 0.555, -0.485), (BED_IN0 - 0.555, 0.485), (BED_IN1 + 0.555, -0.485), (BED_IN1 + 0.555, 0.485)]
RACK_X = (0.84, -0.10)           # o bagageiro do teto (passa por cima do para-brisa)
RACK_Z = 2.0
BAR_X = -0.29                    # o santo-antônio (em cima da cabeceira)
BAR_Y = 0.95
FLAG_POLE = (BAR_X, BAR_Y + 0.09)
FLAG_TOP = 3.05


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


def tyre(name, x, y, zc, R, W, side, rim_r, style='at', rim_m='Dark', n_lugs=24, lug_h=0.035):
    """Uma roda inteira (o jogo gira a malha pelo centro da caixa). 31/10, o detalhe AAA (model_vehicle_parts.tyre): o pneu com o
    FLANCO ABAULADO e o ombro redondo, os blocos do estilo ('at' de lama/estrada, 'chevron' o militar, 'tractor' as garras grandes,
    'paddle' as pás de areia, 'rib' os frisos; 'mud' o V com o ombro), a roda de aço REBAIXADA com a aba, os furos, o cubo saliente
    e o anel de porcas de 4 cm (as de 2 cm sumiam). Os blocos em múltiplo de 4: simétrico em volta do eixo, a caixa no centro."""
    w = Part(name)
    vp.tyre(w, (x, y, zc), R, W, side, rim_r=rim_r, rubber='Rubber', rim=rim_m, hub='Metal', nut_m='Metal', seg=40, lug_h=lug_h,
            style=style, n_lugs=n_lugs, holes_m='Dark', beadlock=style in ('chevron', 'tractor'), nuts=8)
    return w.finish()


# ---------------------------------------------------------------------------------- a forma
def hood_top(x):
    """O alto do capô: sobe um pouco até o para-brisa e arredonda pra baixo na frente."""
    if x <= 2.24:
        return BELT - 0.05 * (x - COWL) / (2.24 - COWL)
    u = (x - 2.24) / (FRONT - 2.24)
    return BELT - 0.05 - 0.09 * u * u


def arch_z(x, ax, floor):
    d = x - ax
    if abs(d) >= ARCH_R:
        return floor
    return max(floor, WHEEL_R + math.sqrt(ARCH_R ** 2 - d * d))


def front_clip(p):
    """O capô e os para-lamas da frente: um loft de seções (o pé recorta o vão da roda; o alto é o capô com a quina chanfrada)."""
    xs = [FRONT, FRONT - 0.01, FRONT - 0.03, FRONT - 0.05, 2.24]
    xs += [FA + ARCH_R - k * (2 * ARCH_R) / 22 for k in range(23) if FA + ARCH_R - k * (2 * ARCH_R) / 22 < 2.24]
    xs += [COWL]
    rings = []
    for x in xs:
        zb = arch_z(x, FA, SILL - 0.02)
        zt = hood_top(x)
        rings.append([(x, -HW, zb), (x, HW, zb), (x, HW, zt - 0.08), (x, HW - 0.05, zt - 0.012), (x, HW - 0.13, zt),
                      (x, -(HW - 0.13), zt), (x, -(HW - 0.05), zt - 0.012), (x, -HW, zt - 0.08)])
    p.loft('GlaPaint#body', rings)


def bed_side(p, s):
    xs = [BED_X0] + [RA + ARCH_R - k * (2 * ARCH_R) / 22 for k in range(23)] + [BED_X1]
    rings = []
    y0, y1 = sorted((BED_IN_HW * s, BED_HW * s))
    for x in xs:
        zb = arch_z(x, RA, SILL + 0.02)
        rings.append([(x, y0, zb), (x, y1, zb), (x, y1, BED_TOP), (x, y0, BED_TOP)])
    p.loft('GlaPaint#body', rings)


def hull():
    p = Part(PREFIX + '_Hull')
    # as chaves na ordem do chanfro (a 1ª chamada de cada chave fixa o chanfro)
    p.bm('GlaPaint#body', 0.035)
    p.bm('GlaPaint', 0.02)
    p.bm('GlaPaint#thin', 0.006)
    p.bm('GlaStripe', 0.0)
    p.bm('Team', 0.0)
    p.bm('Rust', 0.012)
    p.bm('Metal', 0.008)
    p.bm('Dark', 0.008)
    p.bm('Wood', 0.01)
    p.bm('Lens', 0.0)
    p.bm('Rubber', 0.0)
    # ------------------------------------------------------------ o chassi embaixo (aparece pelos vãos e de lado)
    for y in (-0.45, 0.45):
        p.box('Dark', -2.58, 2.30, y - 0.07, y + 0.07, 0.46, 0.64)
    p.box('Dark', COWL + 0.02, 2.30, -0.60, 0.60, 0.40, 1.0)                     # o motor e o forro dos vãos da frente
    p.box('Dark', BED_IN1, BED_X0, -0.62, 0.62, 0.62, 0.92)                       # o forro dos vãos de trás
    for ax in (FA, RA):
        p.cyl('Dark', (ax, 0.0, WHEEL_R), 'y', 0.075, 2 * WHEEL_Y - WHEEL_W, seg=12)  # o eixo
        p.cyl('Dark', (ax, 0.0, WHEEL_R), 'x', 0.16, 0.22, seg=14)                  # o diferencial
    for s in (-1, 1):
        p.box('Dark', RA - 0.62, RA + 0.62, 0.45 * s - 0.04, 0.45 * s + 0.04, 0.52, 0.58)   # o feixe de molas
    p.box('Dark', -1.0, -0.4, -0.88, -0.5, 0.42, 0.66)                              # o tanque de combustível
    # ------------------------------------------------------------ a frente: capô, para-lamas, grade, faróis
    front_clip(p)
    p.box('GlaPaint', FRONT - 0.06, FRONT + 0.02, -HW + 0.02, HW - 0.02, SILL - 0.06, SILL + 0.14)   # a saia da frente
    p.box('Dark', FRONT, FRONT + 0.012, -0.44, 0.44, 0.76, 1.03, bevel=0.0)                   # a grade
    for k in range(5):
        z = 0.79 + k * 0.05
        p.box('Metal', FRONT + 0.012, FRONT + 0.03, -0.42, 0.42, z, z + 0.022, bevel=0.0)
    p.box('GlaPaint#thin', FRONT, FRONT + 0.03, -0.47, 0.47, 1.03, 1.06)
    for s in (-1, 1):
        y = 0.68 * s
        p.box('Metal', FRONT, FRONT + 0.03, y - 0.15, y + 0.15, 0.83, 1.02)                      # o aro do farol (quadrado)
        p.box('Lens', FRONT + 0.03, FRONT + 0.035, y - 0.13, y + 0.13, 0.85, 1.0, bevel=0.0)
        p.box('Lens', FRONT, FRONT + 0.025, 0.88 * s - 0.05, 0.88 * s + 0.05, 0.70, 0.78, bevel=0.0)   # a seta
    # as abas dos para-lamas (a de cima do vão, como a da Technical) e o forro escuro por dentro
    for s in (-1, 1):
        y0, y1 = sorted(((HW - 0.01) * s, (HW + 0.045) * s))
        arch_flare(p, 'GlaPaint', FA, WHEEL_R, ARCH_R, y0, y1, w=0.06, a0=12, a1=168)
        y0, y1 = sorted(((BED_HW - 0.01) * s, (BED_HW + 0.04) * s))
        arch_flare(p, 'GlaPaint', RA, WHEEL_R, ARCH_R, y0, y1, w=0.06, a0=12, a1=168)
    # as listras verde-escuras no capô (as da paleta amarela): duas, gastas nas pontas
    for yc in (-0.25, 0.25):
        rings = []
        for k in range(15):
            x = 1.16 + k * (2.30 - 1.16) / 14
            hw_ = 0.105 * (1.0 if 0 < k < 14 else 0.8)
            z = hood_top(x) + 0.004
            rings.append([(x, yc - hw_, z), (x, yc + hw_, z), (x, yc + hw_, z + 0.006), (x, yc - hw_, z + 0.006)])
        p.loft('GlaStripe', rings, bevel=0.0)
    # a faixa pintada à mão no meio do capô, entre as listras, e a pincelada que cruza (a cor do jogador, lê de cima: o mesmo
    # jeito da faixa verde no glacis do tanque GLA)
    stroke(p, 'Team', [(1.20, 0.01), (1.55, -0.02), (1.85, 0.02), (2.18, -0.01)], 0.12, zfn=lambda x, y: hood_top(x), lift=0.012, seed=1.0)
    stroke(p, 'Team', [(1.62, -0.42), (1.80, -0.1), (2.0, 0.36)], 0.07, zfn=lambda x, y: hood_top(x), lift=0.016, seed=2.0)
    # a entrada de ar no capô e o remendo de ferrugem no canto
    p.box('Dark', 1.35, 1.75, 0.55, 0.75, hood_top(1.55) - 0.01, hood_top(1.55) + 0.025)
    for k in range(5):
        p.box('GlaPaint#thin', 1.38 + k * 0.08, 1.42 + k * 0.08, 0.57, 0.73, hood_top(1.55) + 0.02, hood_top(1.55) + 0.04)
    rings = []
    for (x, y) in ((2.05, -0.82), (2.26, -0.82), (2.23, -0.58), (2.0, -0.62)):
        rings.append((x, y))
    p.loft('Rust', [[(x, y, hood_top(x) + 0.004) for (x, y) in rings], [(x, y, hood_top(x) + 0.014) for (x, y) in rings]], bevel=0.0)
    # ------------------------------------------------------------ o para-choque de sucata: a viga I cortada e o quebra-mato de cano
    bx0, bx1 = FRONT + 0.03, FRONT + 0.16
    p.box('Rust', bx0, bx1, -1.04, 1.0, 0.50, 0.53)                     # a aba de baixo
    p.box('Rust', bx0, bx1, -1.04, 1.0, 0.69, 0.72)                     # a aba de cima
    p.box('Rust', bx0 + 0.055, bx1 - 0.055, -1.04, 1.0, 0.53, 0.69)     # a alma
    p.obox('Rust', (bx1 - 0.02, -1.0, 0.61), (0.06, 0.12, 0.24), yaw=-30, bevel=0.0)   # a ponta cortada torta
    for s in (-1, 1):                                                   # os ganchos e as chapas de solda
        p.box('Metal', bx1, bx1 + 0.045, 0.55 * s - 0.05, 0.55 * s + 0.05, 0.56, 0.66, bevel=0.005)
        p.box('Metal', FRONT - 0.02, bx0 + 0.04, 0.45 * s - 0.07, 0.45 * s + 0.07, 0.50, 0.72, bevel=0.0)
    # o quebra-mato: 2 montantes, o arco de cima e a diagonal soldada (um pouco torto: feito à mão)
    for y in (-0.36, 0.38):
        p.cyl('Metal', (bx1 - 0.03, y, 0.92), 'z', 0.035, 0.42, seg=10, bevel=0.0)
    p.cyl('Metal', (bx1 - 0.03, 0.01, 1.12), (0.0, 1.0, 0.02), 0.035, 0.84, seg=10, bevel=0.0)
    for s in (-1, 1):
        p.cyl('Metal', (bx1 - 0.03, 0.45 * s + 0.0, 1.05), (0.0, 0.1 * s, -0.14), 0.035, 0.2, seg=10, bevel=0.0)
    p.cyl('Metal', (bx1 - 0.03, 0.0, 0.88), (0.0, 0.74, 0.22), 0.028, 0.80, seg=10, bevel=0.0)
    for s in (-1, 1):                                                   # a guarda dos faróis (2 barras)
        for dz in (0.88, 0.97):
            p.cyl('Metal', (bx1 - 0.05, 0.68 * s, dz), 'y', 0.015, 0.36, seg=6, bevel=0.0)
        p.cyl('Metal', (bx1 - 0.05, 0.68 * s, 0.82), 'z', 0.02, 0.24, seg=6, bevel=0.0)
    # ------------------------------------------------------------ a cabine: o pé (as portas) e a estufa (os vidros)
    p.box('GlaPaint#body', CAB_BACK, COWL, -HW, HW, SILL, BELT)
    gh = [[(1.08, -0.95, BELT), (1.08, 0.95, BELT), (CAB_BACK, 0.95, BELT), (CAB_BACK, -0.95, BELT)],
          [(0.66, -0.88, 1.80), (0.66, 0.88, 1.80), (-0.22, 0.88, 1.80), (-0.22, -0.88, 1.80)],
          [(0.58, -0.82, ROOF_Z), (0.58, 0.82, ROOF_Z), (-0.18, 0.82, ROOF_Z), (-0.18, -0.82, ROOF_Z)]]
    p.loft('GlaPaint#body', gh)
    p.box('GlaPaint', COWL - 0.06, COWL + 0.02, -HW + 0.02, HW - 0.02, BELT - 0.02, BELT + 0.03)     # a grelha do pé do para-brisa
    p.box('Dark', COWL - 0.04, COWL + 0.0, -0.8, 0.8, BELT + 0.03, BELT + 0.035, bevel=0.0)
    # os vidros: o para-brisa, as janelas das portas e o de trás
    ws = [gh[0][0], gh[0][1], gh[1][1], gh[1][0]]
    panel(p, 'Lens', ws, 0.05, 0.95, 0.06, 0.94, out=(1.0, 0.0, 0.6))
    for s in (-1, 1):
        sq = [(1.08, 0.95 * s, BELT), (CAB_BACK, 0.95 * s, BELT), (-0.22, 0.88 * s, 1.80), (0.66, 0.88 * s, 1.80)]
        panel(p, 'Lens', sq, 0.07, 0.80, 0.10, 0.90, out=(0.0, s, 0.2))
        panel(p, 'Dark', sq, 0.80, 0.83, 0.02, 0.98, out=(0.0, s, 0.2), lift=0.002, t=0.006)   # a coluna B (a junta)
    for y in (-0.42, 0.18):                                              # os limpadores
        p.obox('Dark', (1.02, y, BELT + 0.07), (0.02, 0.5, 0.02), yaw=8, pitch=0, roll=0, bevel=0.0)
    rq = [(CAB_BACK, -0.95, BELT), (CAB_BACK, 0.95, BELT), (-0.22, 0.88, 1.80), (-0.22, -0.88, 1.80)]
    panel(p, 'Lens', rq, 0.18, 0.82, 0.18, 0.86, out=(-1.0, 0.0, 0.0))
    # as listras do teto (continuam as do capô, por baixo do bagageiro)
    for yc in (-0.25, 0.25):
        p.box('GlaStripe', -0.16, 0.56, yc - 0.105, yc + 0.105, ROOF_Z, ROOF_Z + 0.006)
    # as portas: as juntas, a maçaneta, o retrovisor
    for s in (-1, 1):
        yo = (HW + 0.002) * s
        y0, y1 = sorted((yo, yo + 0.008 * s))
        for x in (1.05, -0.14):
            p.box('Dark', x - 0.008, x + 0.008, y0, y1, SILL + 0.06, BELT, bevel=0.0)
        p.box('Dark', -0.14, 1.05, y0, y1, SILL + 0.05, SILL + 0.065, bevel=0.0)
        p.box('Metal', -0.06, 0.08, min(yo, yo + 0.03 * s), max(yo, yo + 0.03 * s), 1.08, 1.12, bevel=0.0)
        ma = (0.98, 0.98 * s, 1.30)
        mb = (0.90, 1.16 * s, 1.40)
        p.cyl('Dark', tuple((Vector(ma) + Vector(mb)) / 2), tuple(Vector(mb) - Vector(ma)), 0.015, (Vector(mb) - Vector(ma)).length, seg=6, bevel=0.0)
        p.box('Dark', 0.86, 0.93, 1.16 * s - 0.1, 1.16 * s + 0.1, 1.33, 1.50, bevel=0.01)
        # o estribo de tubo
        p.cyl('Metal', (0.45, 1.02 * s, 0.52), 'x', 0.035, 1.3, seg=10, bevel=0.0)
        for x in (-0.05, 0.95):
            p.box('Metal', x - 0.03, x + 0.03, min(0.9 * s, 1.02 * s), max(0.9 * s, 1.02 * s), 0.5, 0.56, bevel=0.0)
    rag(p, (0.94, -1.08, 1.36), (0.08, -0.18, 0.1), hang=0.3)            # o pano amarrado no braço do retrovisor da esquerda
    # a porta da esquerda é de outra picape (ferrugem: não combina) e o rabisco verde na porta da direita
    p.box('Rust', -0.13, 1.04, -HW - 0.006, -HW - 0.001, SILL + 0.07, BELT - 0.01, bevel=0.0)
    p.box('Metal', -0.06, 0.08, -HW - 0.035, -HW - 0.007, 1.08, 1.12, bevel=0.0)
    O, U, W, N = Vector((0.0, HW, 0.0)), Vector((1.0, 0.0, 0.0)), Vector((0.0, 0.0, 1.0)), Vector((0.0, 1.0, 0.0))
    loop = [(0.48 + 0.26 * math.cos(math.radians(a)), 0.94 + 0.17 * math.sin(math.radians(a))) for a in range(200, -150, -25)]
    stroke(p, 'Team', loop, 0.065, plane=(O, U, W, N), lift=0.004, seed=3.0)
    stroke(p, 'Team', [(0.08, 0.74), (0.5, 0.93), (0.98, 1.14)], 0.07, plane=(O, U, W, N), lift=0.006, seed=4.0)
    # o snorkel no pé do para-brisa (direita): sobe do para-lama pela coluna e a boca olhando pra frente
    sn = [(1.24, 1.0, 1.0), (1.20, 1.02, 1.26), (0.70, 0.98, 1.88), (0.70, 0.98, 1.95)]
    for a, b in zip(sn[:-1], sn[1:]):
        a, b = Vector(a), Vector(b)
        p.cyl('Dark', tuple((a + b) / 2), tuple(b - a), 0.055, (b - a).length + 0.05, seg=12, bevel=0.0)
    p.box('Dark', 0.62, 0.84, 0.90, 1.06, 1.92, 2.04, bevel=0.015)
    for k in range(3):
        p.box('Metal', 0.84, 0.85, 0.92, 1.04, 1.94 + k * 0.03, 1.955 + k * 0.03, bevel=0.0)
    # a antena no para-lama da direita, com o pano amarrado
    p.cyl('Dark', (1.30, 0.86, BELT), 'z', 0.03, 0.06, seg=8, bevel=0.0)
    p.cyl('Metal', (1.30, 0.86, BELT + 0.72), 'z', 0.008, 1.44, seg=6, bevel=0.0)
    rag(p, (1.30, 0.86, BELT + 1.18), 'z', hang=0.26)
    # ------------------------------------------------------------ o bagageiro do teto (passa por cima do para-brisa): o estepe e os galões
    rx0, rx1 = RACK_X[1], RACK_X[0]
    for s in (-1, 1):
        p.cyl('Metal', ((rx0 + rx1) / 2, 0.84 * s, RACK_Z), 'x', 0.025, rx1 - rx0, seg=8, bevel=0.0)
        p.cyl('Metal', ((rx0 + rx1) / 2, 0.84 * s, RACK_Z + 0.1), 'x', 0.02, rx1 - rx0, seg=8, bevel=0.0)
        for x in (rx0, 0.22, rx1):
            p.cyl('Metal', (x, 0.84 * s, RACK_Z + 0.05), 'z', 0.018, 0.1, seg=6, bevel=0.0)
        for x in (-0.06, 0.54):                                           # os pés na calha do teto
            p.box('Metal', x - 0.04, x + 0.04, 0.80 * s - 0.04, 0.80 * s + 0.04, ROOF_Z - 0.01, RACK_Z, bevel=0.0)
    for x in (rx0, rx1):
        p.cyl('Metal', (x, 0.0, RACK_Z), 'y', 0.025, 1.68, seg=8, bevel=0.0)
        p.cyl('Metal', (x, 0.0, RACK_Z + 0.1), 'y', 0.02, 1.68, seg=8, bevel=0.0)
    for k in range(5):                                                   # as ripas
        x = rx0 + 0.12 + k * (rx1 - rx0 - 0.24) / 4
        p.box('Metal', x - 0.03, x + 0.03, -0.84, 0.84, RACK_Z - 0.015, RACK_Z + 0.008, bevel=0.0)
    # o estepe deitado (o pneu inteiro com o aro) e os 2 galões deitados, amarrados com corda
    sx, sy, sz = 0.36, -0.36, RACK_Z + 0.008 + 0.15
    st = tyre('spare', sx, sy, sz, 0.42, 0.29, -1, 0.23, n_lugs=20)
    st.data.transform(Matrix.Translation(V(sx, sy, sz)) @ Matrix.Rotation(math.radians(90), 4, 'X') @ Matrix.Translation(-V(sx, sy, sz)))
    for k, x in enumerate((0.58, 0.14)):
        jerrycan(p, Fr((x, 0.80, RACK_Z + 0.008 + 0.0825), 0.0, 0.0, 90.0 + 4 * k))
        rope(p, [(x, 0.84, RACK_Z + 0.1), (x, 0.76, RACK_Z + 0.19), (x, 0.36, RACK_Z + 0.19), (x, 0.26, RACK_Z + 0.02)])
    rope(p, [(rx1, -0.84, RACK_Z + 0.1), (sx + 0.1, sy - 0.05, sz + 0.17), (sx - 0.2, sy + 0.2, sz + 0.17), (rx0 + 0.1, 0.30, RACK_Z + 0.1)])
    rope(p, [(rx0, -0.84, RACK_Z + 0.1), (sx - 0.15, sy - 0.1, sz + 0.17), (sx + 0.15, sy + 0.25, sz + 0.17), (rx1 - 0.05, 0.30, RACK_Z + 0.1)])
    # ------------------------------------------------------------ a caçamba: piso de tábua, laterais com o vão, a tampa, as tábuas nas estacas
    p.box('GlaPaint', BED_IN1, BED_IN0, -BED_IN_HW, BED_IN_HW, 0.90, BED_FLOOR - 0.03)
    n_pl = 6
    for k in range(n_pl):
        y0 = -BED_IN_HW + 0.01 + k * (2 * BED_IN_HW - 0.02) / n_pl
        p.box('Wood', BED_IN1 + 0.01, BED_IN0 - 0.01, y0 + 0.01, y0 + (2 * BED_IN_HW - 0.02) / n_pl - 0.01, BED_FLOOR - 0.03, BED_FLOOR)
    for s in (-1, 1):
        bed_side(p, s)
        p.box('GlaPaint', BED_X1, BED_X0, BED_HW * s - 0.08 if s > 0 else -BED_HW - 0.02, BED_HW * s + 0.02 if s > 0 else -BED_HW + 0.08,
              BED_TOP - 0.02, BED_TOP + 0.035)                                # o friso de cima
        # a listra verde-escura comprida (gasta: as pontas afinam)
        O, U, W, N = Vector((0.0, BED_HW * s, 0.0)), Vector((1.0, 0.0, 0.0)), Vector((0.0, 0.0, 1.0)), Vector((0.0, s, 0.0))
        stroke(p, 'GlaStripe', [(-0.42, 1.22), (-1.2, 1.24), (-2.5, 1.20)], 0.13, plane=(O, U, W, N), lift=0.004, t=0.004, seed=5.0 + s, step=0.08)
    p.box('GlaPaint#body', BED_IN0, BED_X0, -BED_HW, BED_HW, 0.88, BED_TOP + 0.02)          # a cabeceira
    # a tampa de trás (um pouco mais baixa), os frisos, a faixa verde torta e o remendo
    p.box('GlaPaint', BED_X1, BED_IN1, -BED_IN_HW + 0.005, BED_IN_HW - 0.005, 0.86, BED_TOP - 0.02)
    for z in (1.02, 1.24):
        p.box('GlaPaint#thin', BED_X1 - 0.015, BED_X1, -0.86, 0.86, z, z + 0.05)
    p.box('Metal', BED_X1 - 0.03, BED_X1, -0.12, 0.12, 1.31, 1.36, bevel=0.0)              # o trinco
    O, U, W, N = Vector((BED_X1, 0.0, 0.0)), Vector((0.0, 1.0, 0.0)), Vector((0.0, 0.0, 1.0)), Vector((-1.0, 0.0, 0.0))
    stroke(p, 'Team', [(-0.82, 0.96), (-0.2, 1.08), (0.4, 1.0), (0.84, 1.16)], 0.1, plane=(O, U, W, N), lift=0.02, seed=6.0)
    p.box('Rust', BED_X1 - 0.022, BED_X1 - 0.016, 0.42, 0.86, 0.9, 1.12, bevel=0.0)
    for (yy, zz) in ((0.46, 0.94), (0.82, 0.94), (0.46, 1.08), (0.82, 1.08)):
        p.box('Dark', BED_X1 - 0.03, BED_X1 - 0.02, yy - 0.015, yy + 0.015, zz - 0.015, zz + 0.015, bevel=0.0)
    # as lanternas (nos cantos de trás das laterais) e o para-choque de trás de viga de madeira
    for s in (-1, 1):
        p.box('Dark', BED_X1 - 0.04, BED_X1, 0.95 * s - 0.065, 0.95 * s + 0.065, 1.02, 1.32, bevel=0.0)
        p.box('Lens', BED_X1 - 0.045, BED_X1 - 0.04, 0.95 * s - 0.05, 0.95 * s + 0.05, 1.04, 1.30, bevel=0.0)
        p.box('Metal', -2.66, -2.50, 0.45 * s - 0.05, 0.45 * s + 0.05, 0.54, 0.66, bevel=0.0)   # as mãos-francesas
    p.box('Wood', -2.71, -2.61, -0.96, 0.96, 0.54, 0.70)
    for s in (-1, 1):
        p.cyl('Dark', (-2.715, 0.45 * s, 0.62), 'x', 0.025, 0.012, seg=8, bevel=0.0)        # os parafusos da viga
    # o escape (sai embaixo, atrás da roda da direita)
    p.cyl('Rust', (-0.2, 0.62, 0.40), 'x', 0.035, 3.6, seg=10, bevel=0.0)
    p.cyl('Rust', (-2.05, 0.62, 0.40), (-1.0, 0.0, -0.3), 0.035, 0.18, seg=10, bevel=0.0)
    # os paralamas de borracha atrás das rodas de trás
    for s in (-1, 1):
        y0, y1 = sorted((0.68 * s, 0.97 * s))
        p.box('Rubber', RA - ARCH_R - 0.03, RA - ARCH_R - 0.015, y0, y1, 0.24, 0.62, bevel=0.0)
    # o remendo de chapa parafusado na lateral da direita (atrás da roda)
    p.box('Metal', -2.45, -1.98, BED_HW + 0.002, BED_HW + 0.018, 0.90, 1.16, bevel=0.006)
    for (xx, zz) in ((-2.41, 0.94), (-2.02, 0.94), (-2.41, 1.12), (-2.02, 1.12), (-2.22, 0.94), (-2.22, 1.12)):
        vp.nut(p, 'Dark', (xx, BED_HW + 0.018, zz), (0, 1, 0), r=0.02, h=0.014)
    # as tábuas nas laterais: as estacas nos bolsos, 2 tábuas, as amarras de corda em cada estaca
    stakes = [-0.42, -1.12, -1.80, -2.48]
    for s in (-1, 1):
        ys = (BED_HW + 0.03) * s
        for x in stakes:
            p.box('Wood', x - 0.035, x + 0.035, ys - 0.03, ys + 0.03, 1.12, 1.70)
            p.box('Metal', x - 0.06, x + 0.06, ys - 0.045, ys + 0.045, 1.30, 1.40, bevel=0.0)   # o bolsinho
            lashing(p, (x, ys, 1.53), 'z', 0.06, turns=3)
        for z0, z1 in ((1.47, 1.56), (1.60, 1.69)):
            y0, y1 = sorted((0.99 * s, 1.02 * s))
            p.box('Wood', stakes[-1] - 0.12, stakes[0] + 0.1, y0, y1, z0, z1)
        # a corda de amarrar a carga, enrolada em ziguezague nos ganchos (por fora)
        yr = (BED_HW + 0.07) * s
        zz = [(x, 1.22 if k % 2 else 1.66) for k, x in enumerate([-0.42, -0.77, -1.12, -1.46, -1.80, -2.14, -2.48])]
        rope(p, [(x, yr, z) for (x, z) in zz])
        for x in (-0.77, -1.46, -2.14):
            p.box('Metal', x - 0.03, x + 0.03, min(yr, BED_HW * s), max(yr, BED_HW * s), 1.18, 1.24, bevel=0.0)
    # ------------------------------------------------------------ o santo-antônio: tubos, a lona enrolada amarrada em cima, o rolo de corda
    for s in (-1, 1):
        p.cyl('Metal', (BAR_X, BAR_Y * s, (BED_TOP + 2.02) / 2), 'z', 0.04, 2.02 - BED_TOP, seg=12, bevel=0.0)
        a, b = Vector((BAR_X, BAR_Y * s, 1.98)), Vector((BAR_X - 0.62, 0.995 * s, BED_TOP + 0.03))
        p.cyl('Metal', tuple((a + b) / 2), tuple(b - a), 0.032, (b - a).length, seg=10, bevel=0.0)
        p.box('Metal', BAR_X - 0.05, BAR_X + 0.05, BAR_Y * s - 0.06, BAR_Y * s + 0.06, BED_TOP + 0.02, BED_TOP + 0.05, bevel=0.0)
    p.cyl('Metal', (BAR_X, 0.0, 2.02), 'y', 0.04, 2 * BAR_Y + 0.08, seg=12, bevel=0.0)
    p.cyl('Metal', (BAR_X, 0.0, 1.70), 'y', 0.03, 2 * BAR_Y, seg=10, bevel=0.0)
    for y in (-0.3, 0.3):                                                 # a grade da janela de trás
        p.cyl('Metal', (BAR_X, y, 1.86), 'z', 0.02, 0.32, seg=8, bevel=0.0)
    # a lona enrolada (estufada entre as amarras) por cima do tubo
    tz = 2.02 + 0.04 + 0.13
    # (31/10, o detalhe AAA) o rolo de verdade (a espiral da última volta, as pontas com as voltas, estufando entre as amarras de
    # corda) e a ponta da lona caída atrás do tubo com as dobras (antes uma caixa)
    vp.tarp_roll(p, (BAR_X, 0.0, tz), 'y', 1.66, 0.14, m='Cloth', straps=3, strap_m='Wood#rope', rope_ties=True)
    vp.tarp_drape(p, (BAR_X - 0.13, 0.64, tz - 0.04), (BAR_X - 0.13, 0.34, tz - 0.04), 0.32, (-1.0, 0.0, 0.0), m='Cloth', folds=2,
                  amp=0.014, ties=())
    # o rolo de corda pendurado no montante da esquerda
    p.torus('Wood#rope', (BAR_X - 0.02, -1.07, 1.68), 'y', 0.16, 0.032, seg=20, rseg=6)
    p.torus('Wood#rope', (BAR_X - 0.04, -1.10, 1.66), 'y', 0.14, 0.03, seg=20, rseg=6)
    p.box('Wood#rope', BAR_X - 0.03, BAR_X + 0.03, -1.07, -0.99, 1.80, 1.86, bevel=0.0)
    # o mastro da bandeira preso no montante da direita (as abraçadeiras) e a bandeira
    fx, fy = FLAG_POLE
    p.cyl('Metal', (fx, fy, (BED_TOP + FLAG_TOP) / 2), 'z', 0.02, FLAG_TOP - BED_TOP, seg=8, bevel=0.0)
    for z in (1.6, 1.9):
        p.box('Dark', fx - 0.04, fx + 0.04, fy - 0.1, fy + 0.03, z - 0.03, z + 0.03, bevel=0.0)
    flag(p, (fx, fy), FLAG_TOP - 0.03, 0.9, 0.58, m='Team', phase=0.6)
    rag(p, (BAR_X, -0.75, 2.02), 'y', hang=0.34)                         # o pano amarrado no santo-antônio
    o = p.finish()
    return o, st


def wheels():
    objs = []
    n = 0
    for x in (FA, RA):
        for s in (-1, 1):
            n += 1
            rim = 'Rust' if n == 4 else 'Dark'                       # um aro diferente (o estepe que virou roda)
            objs.append(tyre(f'wheel_{n:03d}', x, WHEEL_Y * s, WHEEL_R, WHEEL_R, WHEEL_W, s, 0.235, rim_m=rim))
    return objs


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
    # o estepe entra no casco (não gira)
    from model_lib import select_only
    select_only(h)
    spare.select_set(True)
    bpy.ops.object.join()
    h = bpy.context.view_layer.objects.active
    h.name = PREFIX + '_Hull'
    ws = wheels()
    total = export(h, OUT)
    total += export_group(ws, OUT, PREFIX + '_Wheels')
    print('MODEL total', total, 'tris')
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h] + ws)
    print('MODEL medidas: comprimento %.3f m (x %.3f..%.3f), largura %.3f m, altura %.3f m (o mastro), teto da cabine %.3f m'
          % (ax1 - ax0, ax0, ax1, ay1 - ay0, az1, ROOF_Z))
    print('MODEL cargo.slots_m (o centro do pe de cada caixa, x y z):', [[round(x, 3), round(y, 3), BED_FLOOR] for (x, y) in SLOTS],
          'a caixa', CRATE, '(SM_USA_SupplyCrate_Small)')
    print('MODEL a caçamba por dentro: x %.2f..%.2f, y +-%.2f, piso %.2f, borda %.2f' % (BED_IN1, BED_IN0, BED_IN_HW, BED_FLOOR, BED_TOP))
    print('MODEL rodas: raio %.3f, largura %.3f, eixos x %s, centro y +-%.3f' % (WHEEL_R, WHEEL_W, [FA, RA], WHEEL_Y))
    print('MODEL bandeira: o topo do mastro', [FLAG_POLE[0], FLAG_POLE[1], FLAG_TOP], '(script; no jogo o y espelha)')


main()
