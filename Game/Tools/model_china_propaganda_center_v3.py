"""O Propaganda Center da China, versão 3: a EMISSORA DE TV ESTATAL (a cabeça nova, 01/11/2026), modelada no Blender por script.

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- building Game/Tools/model_china_propaganda_center_v3.py C:/Users/luisf/explosion/captures/game/models/china_propaganda_center_v3

O veredito da v2 (docs/APROVADOS.md, 01/11): "continua sem criatividade nenhuma, muda completamente a mente pra fazer isso aí". O desenho
novo é o de docs/elenco/CHINA_PROPAGANDA.md: a propaganda da China de hoje é a televisão estatal, e o prédio é a emissora inspirada na
sede da CCTV em Pequim (OMA, 2012): o "arco", o anel quadrado torto, a peça de arquitetura mais reconhecível da China de hoje.

A palavra (o que lê em 1 segundo): O LAÇO COM A TV ACESA DENTRO.
- O laço: duas torres de vidro escuro com a grade diagonal de aço (os losangos da fachada real: aqui 2 × 3,2 m, um losango por andar,
  com a cinta de cada andar), baixas e grossas (3,2 × 4,2 m de pé, 11 m), nos cantos opostos do lote (BL atrás-esquerda e FR
  frente-direita), inclinadas uma pra outra 0,12 m por metro (6,8°; a CCTV tem 6°), unidas em cima pelo braço em L (6,4 a 11 m) que
  dobra no canto de trás-direita (BR: o balanço, com o vão aberto até o chão embaixo) e embaixo pela base em L (o estúdio, no concreto
  oliva da China: 3,2 m, a faixa vermelha, o portão com a estrela do ELP, as antenas de satélite do link de subida no teto; do lado de
  dentro a ala da frente é a ARQUIBANCADA de 4 degraus com os assentos vermelhos, virada pro telão). De cima: o anel escuro (os tetos
  de laje, as capas claras desenhando as bordas) com o furo claro no meio (a praça).
- O telão: a face de dentro do braço de trás (x = −3,2, virada pra +X, pra câmera do jogo) é um TELÃO DE LED de 10 × 3,5 m aceso: o
  campo vermelho fixo (Red), o sol de raios acesos (Fire) saindo das CINCO ESTRELAS douradas da bandeira (Light; o emblema escolhido
  da China, 01/11), o 中国 dourado grande, o letreiro do jornal correndo embaixo e a marquinha do laço no canto. Da câmera do jogo ele
  aparece emoldurado pelo laço, por cima da base.
- A transmissão: no meio do braço de trás, por cima do telão, o FEIXE DE CORNETAS (7 cornetas brancas de 3 m num buquê de ~4 m de
  boca, apontadas juntas, no garfo da mesa giratória: gira devagar e varre o campo como um farol de som); nas pontas do braço, as
  antenas (a de treliça com as micro-ondas no canto do balanço, a de painéis de TV em cima da torre BL) com as luzes vermelhas, e as
  luzes de obstáculo nos cantos do teto.
- As faixas: três faixas vermelhas verticais de slogan com os caracteres dourados nas torres (人民万岁 na frente da FR, 中国万岁 na face
  de dentro da BL, ao lado do telão, 中国人民 na face de fora da BL); as 3 bandeiras vermelhas no pé, na frente do estúdio.
- O chão: a praça só dentro do laço, na frente do telão (o lugar da multidão), com a estrela do ELP grande no piso; o resto do lote é o
  chão do mapa (nada de laje do lote: a regra do Felipe).

O lote: 14 × 24 m (x −7..7, y −12..12, o do Units.json), frente em +X. A câmera do jogo vem de +X/−Y (37,5°): vê a base na frente, a
praça, o telão no fundo do laço e as torres dos dois lados se inclinando pro meio.

Peças (origem no chão, no centro do lote; coordenadas da Unreal, X frente, Y direita, Z cima, metros):
  SM_China_PropagandaCenter_Base: tudo o que não se mexe;
  SM_China_PropagandaCenter_Speakers: o feixe de cornetas (a mesa giratória, o gabinete do transmissor com a estrela, o garfo, o tambor
    dos motores e as 7 cornetas): spin em Z, origem no eixo, no topo do pedestal: at_m = SPK_AT;
  SM_China_PropagandaCenter_Scaffold: o andaime da obra (tubos de aço, scaffold() do model_lib).
Materiais (os slots da China, model_faction_kit.py e import_provisional.py): CnConcrete, CnPlinth, CnMetal, CnGold, Glass, Player (as
faixas, a faixa da base, as bandeiras, a borda da marquise), Red (o campo do telão e a estrela do ELP, fixos), Fire (os raios acesos do
telão), Light (o dourado aceso do telão, o saguão aceso, as luminárias), Paint (o branco das cornetas e das antenas), Dark, Beacon.
"""
import json
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, clear_scene, export, flag, scaffold  # noqa: E402,F401
from model_faction_kit import _bar2, bayi_strokes, lamp, star_points, star_wall  # noqa: E402
from model_building_parts import (BPart, WallFace, _hash, bake_in_place, cn_rollup, cn_wall, cn_wall_skin, cn_window,  # noqa: E402
                                  hexa, interior_frame, roof_slabs, splits, wbox)
import model_vehicle_parts as vp  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'china_propaganda_center_v3')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_China_PropagandaCenter'

HX, HY = 7.0, 12.0                  # o lote 14 × 24
LEAN = 0.12                         # a inclinação das torres (m por m de subida: 6,8°)
H = 11.0                            # o topo do laço (a laje do teto do braço e das torres)
OZ = 6.4                            # o fundo do braço em balanço
BZ = 3.2                            # o teto da base (o estúdio)
XB0, XB1 = -6.4, -3.2               # a torre de trás (BL) e o braço de trás
XF0, XF1 = 2.2, 5.4                 # a torre da frente (FR)
YB0, YB1 = -11.0, -6.8              # o pé da torre BL (inclina pra +Y)
YF0, YF1 = 6.8, 11.0                # o pé da torre FR (inclina pra −Y)
BX0, BX1 = 2.0, 5.65                # a ala da frente da base (a face de dentro, a de fora)
BY0, BYI = -11.25, -7.8             # a ala esquerda da base (a face de fora, a de dentro)
DG_W, DG_S = 2.0, 1.6               # a grade diagonal: a largura do losango e a inclinação das barras (o nó a cada 1,6 m de altura)
FLOORS = (3.2, 6.4, 9.6)            # as cintas dos andares
# o telão (na face x = XB1): a caixa, a moldura e o campo (u = y)
HW, HZ0, HZ1 = 5.38, 6.5, 10.8
SW, SZ0, SZ1 = 5.0, 6.88, 10.42
SPK_AT = (-4.8, 0.0, H + 0.5)       # o eixo do feixe de cornetas, no topo do pedestal
DOOR_Y0, DOOR_Y1 = -3.5, -0.9       # o portão do estúdio (na face +X da base)
TX0, TX1, TY1 = 2.0, 4.0, 6.3       # a arquibancada (x do pé ao topo; y até a parede de fecho junto da torre FR)
STAR_C, STAR_R = (-0.65, -0.9), 1.75  # a estrela do ELP no piso da praça


def yb0(z):
    return YB0 + LEAN * z


def yb1(z):
    return YB1 + LEAN * z


def yf0(z):
    return YF0 - LEAN * z


def yf1(z):
    return YF1 - LEAN * z


def _f(v, z):
    return v(z) if callable(v) else v


def rect(u0, u1, z0, z1):
    return [(u0, z0), (u1, z0), (u1, z1), (u0, z1)]


def tube(p, m, a, b, r, seg=8):
    """O tubo reto de a até b."""
    d = Vector(b) - Vector(a)
    if d.length < 1e-4:
        return
    c = (Vector(a) + Vector(b)) / 2
    p.cyl(m if '#' in m else m + '#t', tuple(c), tuple(d), r, d.length, seg=seg, bevel=0.0)


def lay(p, m, M, pts, d0, d1):
    """O polígono (u, v) do plano M(u, v, n) extrudado de n = d0 até d1 (uma camada fina: a estrela, o traço, o letreiro)."""
    p.loft(m, [[tuple(M(u, v, d0)) for (u, v) in pts], [tuple(M(u, v, d1)) for (u, v) in pts]], bevel=0.0)


def report(o):
    cnt = {}
    for poly in o.data.polygons:
        n = o.data.materials[poly.material_index].name
        cnt[n] = cnt.get(n, 0) + len(poly.vertices) - 2
    print('TRIS', o.name, sorted(cnt.items(), key=lambda kv: -kv[1]))


# ===================================================================================================== geometria 2D
def poly_area(P):
    n = len(P)
    return 0.5 * sum(P[i][0] * P[(i + 1) % n][1] - P[(i + 1) % n][0] * P[i][1] for i in range(n))


def inside(pt, loops):
    """O ponto dentro da região (os laços: o contorno e os furos), pela regra par-ímpar."""
    c = False
    x, y = pt
    for loop in loops:
        n = len(loop)
        for i in range(n):
            (x0, y0), (x1, y1) = loop[i], loop[(i + 1) % n]
            if (y0 > y) != (y1 > y):
                xi = x0 + (y - y0) * (x1 - x0) / (y1 - y0)
                if xi > x:
                    c = not c
    return c


def clip_line(A, D, loops):
    """Os trechos [t0, t1] da reta A + t·D que ficam dentro da região (o contorno côncavo e os furos)."""
    ts = []
    for loop in loops:
        n = len(loop)
        for i in range(n):
            P, Q = loop[i], loop[(i + 1) % n]
            E = (Q[0] - P[0], Q[1] - P[1])
            den = D[0] * E[1] - D[1] * E[0]
            if abs(den) < 1e-12:
                continue
            w = (P[0] - A[0], P[1] - A[1])
            t = (w[0] * E[1] - w[1] * E[0]) / den
            s = (w[0] * D[1] - w[1] * D[0]) / den
            if -1e-9 <= s <= 1 + 1e-9:
                ts.append(t)
    ts.sort()
    out = []
    for a, b in zip(ts[:-1], ts[1:]):
        if b - a < 0.05:
            continue
        m = (a + b) / 2
        if inside((A[0] + D[0] * m, A[1] + D[1] * m), loops):
            out.append((a, b))
    return out


def clip_rect(poly, u0, u1, v0, v1):
    """O polígono convexo recortado no retângulo (Sutherland–Hodgman)."""
    def clip(pts, inside_fn, cut):
        out = []
        for i in range(len(pts)):
            a, b = pts[i - 1], pts[i]
            ia, ib = inside_fn(a), inside_fn(b)
            if ib:
                if not ia:
                    out.append(cut(a, b))
                out.append(b)
            elif ia:
                out.append(cut(a, b))
        return out

    def cut_u(k):
        return lambda a, b: (k, a[1] + (b[1] - a[1]) * (k - a[0]) / (b[0] - a[0]))

    def cut_v(k):
        return lambda a, b: (a[0] + (b[0] - a[0]) * (k - a[1]) / (b[1] - a[1]), k)
    pts = poly
    for (fn, cut) in ((lambda q: q[0] >= u0, cut_u(u0)), (lambda q: q[0] <= u1, cut_u(u1)),
                      (lambda q: q[1] >= v0, cut_v(v0)), (lambda q: q[1] <= v1, cut_v(v1))):
        if not pts:
            break
        pts = clip(pts, fn, cut)
    return pts


def _lnormal(a, b):
    L = math.hypot(b[0] - a[0], b[1] - a[1])
    return (-(b[1] - a[1]) / L, (b[0] - a[0]) / L)


def offset_run(pts, d, closed=False):
    """A polilinha de eixos (só trechos em X e Y) deslocada d pra esquerda de quem anda, com o canto em esquadro."""
    n = len(pts)
    out = []
    for i in range(n):
        ns = []
        if i > 0 or closed:
            ns.append(_lnormal(pts[i - 1], pts[i]))
        if i < n - 1 or closed:
            ns.append(_lnormal(pts[i], pts[(i + 1) % n]))
        if len(ns) == 2 and abs(ns[0][0] * ns[1][0] + ns[0][1] * ns[1][1]) < 0.5:
            o = (ns[0][0] + ns[1][0], ns[0][1] + ns[1][1])
        else:
            o = ns[0]
        out.append((pts[i][0] + d * o[0], pts[i][1] + d * o[1]))
    return out


def run_box(p, m, pts, d0, d1, z0, z1, closed=False, bevel=None):
    """A faixa de caixas ao longo da polilinha, de d0 a d1 pra esquerda de quem anda (o parapeito, a cornija, a faixa vermelha), com
    a quina em meia-esquadria."""
    A, B = offset_run(pts, d0, closed), offset_run(pts, d1, closed)
    n = len(pts)
    for i in range(n if closed else n - 1):
        j = (i + 1) % n
        P = []
        for z in (z0, z1):
            for q in (A, B):
                for k in (i, j):
                    P.append((q[k][0], q[k][1], z))
        hexa(p, m, P, bevel)


# ===================================================================================================== a pele de vidro com a grade diagonal
def member(p, m, F, a, b, wb, wt, n0, n1, ext=0.0, bevel=0.0):
    """A barra de perfil trapezoidal na face F, de a até b (u, z): a base wb no plano n0, o topo wt em n1 (os lados inclinados fazem o
    papel do chanfro: pegam o sol de um lado e a sombra do outro, sem o custo do chanfro de 3 segmentos)."""
    du, dz = b[0] - a[0], b[1] - a[1]
    L = math.hypot(du, dz)
    if L < 1e-3:
        return
    eu, ez = du / L, dz / L
    pu, pz = -ez, eu
    A = (a[0] - eu * ext, a[1] - ez * ext)
    B = (b[0] + eu * ext, b[1] + ez * ext)
    P = []
    for q in (A, B):
        for (n, w) in ((n0, wb), (n1, wt)):
            for s in (-0.5, 0.5):
                P.append(F.pt(q[0] + pu * s * w, q[1] + pz * s * w, n))
    hexa(p, m, P, bevel)


def frame_strip(p, m, F, a, b, inward, fw=0.3, ext=0.18, n0=-0.08, n1=0.18, ch=0.07):
    """O quadro de aço na borda da face (a coluna de quina, a viga do beiral, a borda do balanço): fw pra dentro e ext pra fora da
    borda (nas quinas os quadros das duas faces se cruzam e fecham a coluna cheia). O perfil é trapezoidal (o topo ch mais estreito
    de cada lado): o chanfro sai da forma, sem o modificador (na 1ª volta o chanfro de 3 segmentos nos quadros de 20 m, com a grade
    do detalhe, eram 118 mil triângulos)."""
    du, dz = b[0] - a[0], b[1] - a[1]
    L = math.hypot(du, dz)
    eu, ez = du / L, dz / L
    A = (a[0] - eu * ext, a[1] - ez * ext)
    B = (b[0] + eu * ext, b[1] + ez * ext)
    iu, iz = inward
    P = []
    for k, o in enumerate((-ext, fw)):
        for n in (n0, n1):
            oo = o + (ch if k == 0 else -ch) * (1.0 if n == n1 else 0.0)
            for q in (A, B):
                P.append(F.pt(q[0] + iu * oo, q[1] + iz * oo, n))
    hexa(p, m, P, 0.0)


def facade(p, F, poly, quads, holes=(), plinth=None):
    """A face de vidro da emissora: o vidro escuro recuado (com espessura, por quads), a grade diagonal de aço por cima (as barras dos
    losangos recortadas no contorno e nos furos), a cinta de cada andar, o quadro de aço em todas as bordas e o rodapé de concreto no pé
    (plinth = (u0, u1) no chão; podem ser funções da altura)."""
    for q in quads:
        corner = {(0, 0): q[0], (1, 0): q[1], (1, 1): q[2], (0, 1): q[3]}
        P = []
        for zi in (0, 1):
            for n in (-0.3, -0.04):
                for ui in (0, 1):
                    c = corner[(ui, zi)]
                    P.append(F.pt(c[0], c[1], n))
        hexa(p, 'Glass#gl', P, 0.0)
    loops = [poly] + [list(h) for h in holes]
    us = [q[0] for q in poly]
    zs = [q[1] for q in poly]
    u_lo, u_hi, z_hi = min(us), max(us), max(zs)
    u_off = (u_lo + u_hi) / 2
    for s in (1, -1):
        k0 = int(math.floor((u_lo - z_hi / DG_S - u_off) / DG_W)) - 1
        k1 = int(math.ceil((u_hi + z_hi / DG_S - u_off) / DG_W)) + 1
        for k in range(k0, k1 + 1):
            c = u_off + k * DG_W
            for (t0, t1) in clip_line((c, 0.0), (s / DG_S, 1.0), loops):
                member(p, 'CnMetal#dg', F, (c + s * t0 / DG_S, t0), (c + s * t1 / DG_S, t1), 0.2, 0.09, -0.05, 0.13)
    for zf in FLOORS:
        if any(abs(zf - q[1]) < 0.05 for q in poly):
            continue
        for (t0, t1) in clip_line((u_lo - 1.0, zf), (1.0, 0.0), loops):
            member(p, 'CnMetal#fl', F, (u_lo - 1.0 + t0, zf), (u_lo - 1.0 + t1, zf), 0.15, 0.15, -0.05, 0.07)
    sg = 1.0 if poly_area(poly) > 0 else -1.0
    for i in range(len(poly)):
        a, b = poly[i], poly[(i + 1) % len(poly)]
        L = math.hypot(b[0] - a[0], b[1] - a[1])
        if L < 1e-3:
            continue
        e = ((b[0] - a[0]) / L, (b[1] - a[1]) / L)
        frame_strip(p, 'CnMetal#frame', F, a, b, (-e[1] * sg, e[0] * sg))
    if plinth:
        u0, u1 = plinth
        wbox(p, 'CnPlinth#tal', F, (lambda z, f=u0: _f(f, z) - 0.28), (lambda z, f=u1: _f(f, z) + 0.28), -0.05, 0.6, -0.35, 0.28,
             bevel=0.06)


# ===================================================================================================== os caracteres e a estrela
# os caracteres do slogan em traços retos (a célula −0,5..0,5, u à direita de quem lê, v pra cima): o desenho simplificado de cada um,
# com o número e a direção dos traços certos pra ler como chinês de verdade na câmera do jogo
GLYPHS = {
    '中': [[(-0.36, 0.24), (-0.36, -0.2)], [(-0.36, 0.24), (0.36, 0.24), (0.36, -0.2)], [(-0.36, -0.2), (0.36, -0.2)],
          [(0.0, 0.5), (0.0, -0.5)]],
    '国': [[(-0.42, 0.46), (-0.42, -0.46)], [(-0.42, 0.46), (0.42, 0.46), (0.42, -0.46)], [(-0.42, -0.46), (0.42, -0.46)],
          [(-0.24, 0.24), (0.24, 0.24)], [(-0.2, 0.0), (0.2, 0.0)], [(-0.27, -0.25), (0.27, -0.25)], [(0.0, 0.24), (0.0, -0.25)],
          [(0.1, -0.08), (0.19, -0.17)]],
    '人': [[(0.03, 0.48), (-0.04, 0.12), (-0.42, -0.46)], [(-0.02, 0.1), (0.42, -0.46)]],
    '民': [[(-0.34, 0.46), (-0.34, -0.34)], [(-0.34, 0.46), (0.3, 0.46), (0.3, 0.16)], [(-0.34, 0.16), (0.3, 0.16)],
          [(-0.34, -0.08), (0.44, -0.08)], [(0.0, 0.16), (0.08, -0.18), (0.3, -0.42), (0.44, -0.3)], [(-0.34, -0.34), (-0.1, -0.24)]],
    '万': [[(-0.45, 0.38), (0.45, 0.38)], [(-0.1, 0.38), (-0.16, -0.04), (-0.42, -0.46)],
          [(-0.13, 0.08), (0.32, 0.08), (0.28, -0.38), (0.1, -0.3)]],
    '岁': [[(0.0, 0.5), (0.0, 0.18)], [(-0.32, 0.4), (-0.32, 0.18)], [(0.32, 0.4), (0.32, 0.18)], [(-0.32, 0.18), (0.32, 0.18)],
          [(-0.1, 0.08), (-0.38, -0.18)], [(-0.22, -0.02), (0.3, -0.02), (0.02, -0.32), (-0.36, -0.48)], [(-0.06, -0.14), (0.08, -0.24)]],
}


def _seg(a, b, w, ext):
    du, dv = b[0] - a[0], b[1] - a[1]
    L = math.hypot(du, dv) or 1.0
    eu, ev = du / L, dv / L
    return _bar2(a[0] - eu * ext, a[1] - ev * ext, b[0] + eu * ext, b[1] + ev * ext, w)


def _in_quad(pt, q):
    s = 0
    for i in range(4):
        a, b = q[i], q[(i + 1) % 4]
        c = (b[0] - a[0]) * (pt[1] - a[1]) - (b[1] - a[1]) * (pt[0] - a[0])
        if abs(c) > 1e-12:
            if s == 0:
                s = 1 if c > 0 else -1
            elif (c > 0) != (s > 0):
                return False
    return True


def raster_rects(quads, N, R=0.6):
    """Os traços (quads) rasterizados na grade N × N do quadrado [−R, R]² e juntados em retângulos que só encostam (as fileiras de
    células acesas, e as fileiras iguais empilhadas): [(u0, u1, v0, v1)...]."""
    c = 2 * R / N
    on = [[any(_in_quad((-R + (i + 0.5) * c, -R + (j + 0.5) * c), q) for q in quads) for i in range(N)] for j in range(N)]
    rows = []
    for j in range(N):
        runs, i = [], 0
        while i < N:
            if on[j][i]:
                i0 = i
                while i < N and on[j][i]:
                    i += 1
                runs.append((i0, i))
            else:
                i += 1
        rows.append(runs)
    out, open_ = [], {}
    for j in range(N + 1):
        cur = set(rows[j]) if j < N else set()
        for key in list(open_):
            if key not in cur:
                j0 = open_.pop(key)
                out.append((-R + key[0] * c, -R + key[1] * c, -R + j0 * c, -R + j * c))
        for key in cur:
            open_.setdefault(key, j)
    return out


def glyph(p, M, ch, cu, cv, size, n0, t, m='CnGold', w=0.105, outline=None, N=24):
    """O caractere ch de tamanho 'size' centrado em (cu, cv) no plano M(u, v, n) (u à direita de quem lê). outline = (material,
    a sobra, a espessura): o contorno escuro atrás de cada traço (o dourado aceso não se perde nos raios).
    (2ª volta) os traços rasterizados em blocos que só encostam (o pixel do LED, ~5 cm na faixa): os traços sobrepostos deixavam
    vértices dentro do outro traço, e a oclusão do detalhe pintava um quadradinho preto em cada junta."""
    quads = [_seg(a, b, w, w / 2) for line in GLYPHS[ch] for (a, b) in zip(line[:-1], line[1:])]
    for (u0, u1, v0, v1) in raster_rects(quads, N):
        lay(p, m, M, rect(cu + u0 * size, cu + u1 * size, cv + v0 * size, cv + v1 * size), n0, n0 + t)
    if outline:
        om, ow, ot = outline
        oq = [_seg(a, b, w + 2 * ow / size, w / 2 + ow / size) for line in GLYPHS[ch] for (a, b) in zip(line[:-1], line[1:])]
        for (u0, u1, v0, v1) in raster_rects(oq, N):
            lay(p, om, M, rect(cu + u0 * size, cu + u1 * size, cv + v0 * size, cv + v1 * size), n0 - ot, n0)


def emblem(p, M, r, n0, t=0.03):
    """A estrela do ELP aprovada (01/11, a star_wall do model_faction_kit.py: a borda CnGold, a estrela Red fixa, o 八一 em CnGold por
    cima), num plano qualquer M(u, v, n) (u à direita de quem olha, v pra cima, n pra fora): as mesmas 3 camadas e peças do kit."""
    lay(p, 'CnGold', M, star_points(0.0, 0.0, r * 1.12, r_in=r * 1.12 * 0.40, rot_deg=90.0), n0, n0 + t)
    lay(p, 'Red', M, star_points(0.0, 0.0, r, rot_deg=90.0), n0 + t, n0 + t * 1.6)
    for q in bayi_strokes(r):
        lay(p, 'CnGold', M, q, n0 + t * 1.6, n0 + t * 2.0)


# ===================================================================================================== o laço (as torres e o braço)
def faces():
    """As 8 faces de vidro do laço: (a face, o contorno (u, z), os quads do vidro, os furos, o rodapé)."""
    FC = []
    # 1. a fachada de trás (x = XB0, olha −X): a torre BL e o braço de trás até a quina do balanço
    FC.append((WallFace('-x', XB0), [(YB0, 0.0), (YB1, 0.0), (yb1(OZ), OZ), (yf1(OZ), OZ), (yf1(H), H), (yb0(H), H)],
               [[(YB0, 0.0), (YB1, 0.0), (yb1(OZ), OZ), (yb0(OZ), OZ)], [(yb0(OZ), OZ), (yf1(OZ), OZ), (yf1(H), H), (yb0(H), H)]],
               [], (yb0, yb1)))
    # 2. a face de fora da torre BL (y = yb0(z), olha −Y, deita pra dentro)
    FC.append((WallFace('-y', YB0, 0.0, LEAN), rect(XB0, XB1, 0.0, H), [rect(XB0, XB1, 0.0, H)], [], (XB0, XB1)))
    # 3. a face de dentro de trás (x = XB1, olha +X, pra câmera): a torre BL (embaixo a base encosta) e o braço com o telão
    FC.append((WallFace('+x', XB1),
               [(BYI, 0.0), (YB1, 0.0), (yb1(OZ), OZ), (yf0(OZ), OZ), (yf0(H), H), (yb0(H), H), (yb0(BZ), BZ), (BYI, BZ)],
               [[(BYI, 0.0), (YB1, 0.0), (yb1(BZ), BZ), (BYI, BZ)], [(yb0(BZ), BZ), (yb1(BZ), BZ), (yb1(OZ), OZ), (yb0(OZ), OZ)],
                [(yb0(OZ), OZ), (yf0(OZ), OZ), (yf0(H), H), (yb0(H), H)]],
               [rect(-HW, HW, HZ0, HZ1)], (BYI, yb1)))
    # 4. a face +Y da torre BL (debaixo do braço, debruçada sobre o vão)
    FC.append((WallFace('+y', YB1, 0.0, -LEAN), rect(XB0, XB1, 0.0, OZ), [rect(XB0, XB1, 0.0, OZ)], [], (XB0, XB1)))
    # 5. a frente da torre FR (x = XF1, olha +X; embaixo a base passa na frente)
    FC.append((WallFace('+x', XF1), [(YF0, 0.0), (YF1, 0.0), (yf1(H), H), (yf0(H), H)],
               [[(YF0, 0.0), (YF1, 0.0), (yf1(H), H), (yf0(H), H)]], [], (YF0, yf1)))
    # 6. a fachada da direita (y = yf1(z), olha +Y): a torre FR e o braço até a quina do balanço
    FC.append((WallFace('+y', YF1, 0.0, LEAN), [(XF0, 0.0), (XF1, 0.0), (XF1, H), (XB0, H), (XB0, OZ), (XF0, OZ)],
               [rect(XF0, XF1, 0.0, OZ), rect(XB0, XF1, OZ, H)], [], (XF0, XF1)))
    # 7. a face de dentro da direita (y = yf0(z), olha −Y): a torre FR em cima da base e o braço da direita
    FC.append((WallFace('-y', YF0, 0.0, -LEAN), [(XF0, BZ), (XF1, BZ), (XF1, H), (XB1, H), (XB1, OZ), (XF0, OZ)],
               [rect(XF0, XF1, BZ, OZ), rect(XB1, XF1, OZ, H)], [], None))
    # 8. as costas da torre FR (x = XF0, olha −X, debaixo do braço)
    FC.append((WallFace('-x', XF0), [(YF0, 0.0), (YF1, 0.0), (yf1(OZ), OZ), (yf0(OZ), OZ)],
               [[(YF0, 0.0), (YF1, 0.0), (yf1(OZ), OZ), (yf0(OZ), OZ)]], [], (yf0, yf1)))
    return FC


ROOF = [(XB0, yb0(H)), (XB1, yb0(H)), (XB1, yf0(H)), (XF1, yf0(H)), (XF1, yf1(H)), (XB0, yf1(H))]   # o contorno do teto (anti-horário)


def lattice_mast(p, c, z0, z1, a0, a1, levels, m='CnMetal', leg=0.07, brace=0.035):
    """A torre treliçada quadrada (rádio): 4 pernas de a0 (meio-lado no pé) a a1 (no topo), os anéis e os X de cada face."""
    def corner(k, z):
        t = (z - z0) / (z1 - z0)
        a = a0 + (a1 - a0) * t
        sx, sy = ((-1, -1), (1, -1), (1, 1), (-1, 1))[k]
        return (c[0] + sx * a, c[1] + sy * a, z)
    for k in range(4):
        tube(p, m + '#leg', corner(k, z0), corner(k, z1), leg, seg=8)
    for lv in range(levels + 1):
        z = z0 + (z1 - z0) * lv / levels
        for k in range(4):
            tube(p, m + '#br', corner(k, z), corner((k + 1) % 4, z), brace * 1.2, seg=6)
    for lv in range(levels):
        za, zb = z0 + (z1 - z0) * lv / levels, z0 + (z1 - z0) * (lv + 1) / levels
        for k in range(4):
            tube(p, m + '#br', corner(k, za), corner((k + 1) % 4, zb), brace, seg=6)
            tube(p, m + '#br', corner((k + 1) % 4, za), corner(k, zb), brace, seg=6)


def beacon(p, x, y, z, r=0.1):
    """A luz vermelha de obstáculo: o pé de aço e a cúpula acesa (Beacon, pisca)."""
    p.cyl('CnMetal#bk', (x, y, z + 0.06), 'z', r * 0.8, 0.12, seg=10, bevel=0.0)
    vp.lathe(p, 'Beacon', (x, y, z + 0.12), 'z', [(0.0, 0.0), (r, 0.0), (r, 0.1), (r * 0.6, 0.18), (0.0, 0.2)], seg=12)


def loop(p):
    """O laço: as 8 faces de vidro, o forro do balanço com as vigas, a laje do teto com as lajotas, o parapeito com a capa clara, as
    luzes dos cantos, o pedestal das cornetas, as duas antenas e a casa de máquinas em cima da FR."""
    for (F, poly, quads, holes, pl) in faces():
        facade(p, F, poly, quads, holes, pl)
    # o forro do balanço (o fundo do braço, aço escuro) e as vigas que descem dele
    p.box('CnMetal#soffit', XB0, XB1, yb1(OZ) - 0.02, yf1(OZ), OZ, OZ + 0.3, bevel=0.03)
    p.box('CnMetal#soffit', XB1 - 0.02, XF0 + 0.02, yf0(OZ), yf1(OZ), OZ, OZ + 0.3, bevel=0.03)
    for y in splits(yb1(OZ) + 1.4, yf0(OZ) - 0.5, 2.4):
        p.box('CnMetal#sbeam', XB0 + 0.25, XB1 - 0.25, y - 0.13, y + 0.13, OZ - 0.24, OZ + 0.01, bevel=0.03)
    for x in splits(XB0 + 0.6, XF0 - 0.5, 2.4):
        p.box('CnMetal#sbeam', x - 0.13, x + 0.13, yf0(OZ) + 0.25, yf1(OZ) - 0.25, OZ - 0.24, OZ + 0.01, bevel=0.03)
    # a laje do teto e as lajotas (o anel escuro que a câmera vê de cima)
    p.box('CnConcrete#deck', XB0, XB1, yb0(H), yf1(H), H - 0.3, H, bevel=0.03)
    p.box('CnConcrete#deck', XB1 - 0.02, XF1, yf0(H), yf1(H), H - 0.3, H, bevel=0.03)
    roof_slabs(p, XB0 + 0.33, XB1 - 0.33, yb0(H) + 0.33, yf1(H) - 0.33, H, size=3.2, m='CnConcrete#rslab')
    roof_slabs(p, XB1 - 0.33, XF1 - 0.33, yf0(H) + 0.33, yf1(H) - 0.33, H, size=3.2, m='CnConcrete#rslab')
    # o parapeito (por fora cobre a ponta dos quadros de aço) com a capa clara: desenha o anel de cima
    run_box(p, 'CnConcrete#par', ROOF, -0.2, 0.3, H - 0.02, H + 0.5, closed=True, bevel=0.03)
    run_box(p, 'CnPlinth#cap', ROOF, -0.28, 0.38, H + 0.5, H + 0.6, closed=True, bevel=0.03)
    # as luzes de obstáculo nos cantos de fora do teto
    for (x, y) in ((XB0 + 0.1, yb0(H) + 0.1), (XB1 - 0.1, yb0(H) + 0.1), (XB0 + 0.1, yf1(H) - 0.1), (XF1 - 0.1, yf1(H) - 0.1),
                   (XF1 - 0.1, yf0(H) + 0.1)):
        beacon(p, x, y, H + 0.6)
    # o pedestal do feixe de cornetas (o tambor de concreto e a chapa de aço), no meio do braço de trás
    x, y = SPK_AT[0], SPK_AT[1]
    vp.lathe(p, 'CnConcrete#ped', (x, y, 0.0), 'z', [(0.0, H - 0.05), (1.22, H - 0.05), (1.22, H + 0.38), (1.14, H + 0.44),
                                                    (0.0, H + 0.44)], seg=40)
    vp.lathe(p, 'CnMetal#plate', (x, y, 0.0), 'z', [(0.0, H + 0.42), (1.1, H + 0.42), (1.1, SPK_AT[2]), (0.0, SPK_AT[2])], seg=40)
    vp.lathe(p, 'CnGold#pband', (x, y, 0.0), 'z', [(1.2, H + 0.12), (1.25, H + 0.12), (1.25, H + 0.24), (1.2, H + 0.24)], seg=40,
             closed=True)
    # a antena de treliça no canto do balanço (BR): as micro-ondas, a vareta e as luzes vermelhas
    ca = (-4.8, (yf0(H) + yf1(H)) / 2)
    p.box('CnConcrete#pad', ca[0] - 0.8, ca[0] + 0.8, ca[1] - 0.8, ca[1] + 0.8, H - 0.02, H + 0.22, bevel=0.04)
    lattice_mast(p, ca, H + 0.2, H + 4.6, 0.55, 0.22, 4)
    for (az, z) in ((200.0, H + 2.9), (300.0, H + 3.5)):
        a = math.radians(az)
        d = Vector((math.cos(a), math.sin(a), 0.0))
        c = Vector((ca[0], ca[1], z)) + d * 0.62
        tube(p, 'CnMetal#arm', (ca[0], ca[1], z), tuple(c), 0.04)
        p.cyl('Paint#mw', tuple(c + d * 0.15), tuple(d), 0.44, 0.34, seg=24, bevel=0.0)
        p.cyl('CnMetal#mwrim', tuple(c + d * 0.34), tuple(d), 0.46, 0.05, seg=24, bevel=0.0)
        p.cyl('Dark#mw', tuple(c + d * 0.36), tuple(d), 0.38, 0.02, seg=24, bevel=0.0)
    p.cyl('CnMetal#whip', (ca[0], ca[1], H + 5.2), 'z', 0.04, 1.2, seg=8, bevel=0.0)
    beacon(p, ca[0], ca[1], H + 4.6, r=0.13)
    for s in (-1, 1):
        beacon(p, ca[0] + s * 0.42, ca[1], H + 2.4, r=0.09)
    # a antena de painéis de TV em cima da torre BL (a coluna com 3 fileiras de painéis brancos em 3 lados)
    cb = (-4.8, (yb0(H) + yb1(H)) / 2)
    p.box('CnConcrete#pad', cb[0] - 0.75, cb[0] + 0.75, cb[1] - 0.75, cb[1] + 0.75, H - 0.02, H + 0.22, bevel=0.04)
    p.cyl('CnMetal#col', (cb[0], cb[1], H + 2.2), 'z', 0.22, 4.0, seg=14, bevel=0.0)
    for lv in range(3):
        z = H + 1.0 + lv * 1.05
        for k in range(3):
            a = math.radians(60.0 + 120.0 * k)
            d = Vector((math.cos(a), math.sin(a), 0.0))
            c = Vector((cb[0], cb[1], z)) + d * 0.42
            tube(p, 'CnMetal#arm', tuple(Vector((cb[0], cb[1], z + 0.25)) + d * 0.2), tuple(c + Vector((0, 0, 0.25))), 0.035)
            tube(p, 'CnMetal#arm', tuple(Vector((cb[0], cb[1], z - 0.25)) + d * 0.2), tuple(c - Vector((0, 0, 0.25))), 0.035)
            p.obox('Paint#panel', tuple(c + d * 0.05), (0.12, 0.42, 0.85), yaw=60.0 + 120.0 * k, bevel=0.02)
    p.cyl('CnMetal#col', (cb[0], cb[1], H + 4.35), 'z', 0.12, 0.3, seg=10, bevel=0.0)
    beacon(p, cb[0], cb[1], H + 4.5, r=0.13)
    # a calha dos cabos no teto (do pedestal até as duas antenas)
    for (y0, y1) in ((SPK_AT[1] + 1.3, ca[1] - 0.85), (cb[1] + 0.8, SPK_AT[1] - 1.3)):
        p.box('CnMetal#tray', SPK_AT[0] - 0.16, SPK_AT[0] + 0.16, y0, y1, H + 0.2, H + 0.3, bevel=0.0)
        for yy in splits(y0 + 0.3, y1 - 0.3, 1.2):
            p.box('CnMetal#tray', SPK_AT[0] - 0.2, SPK_AT[0] + 0.2, yy - 0.04, yy + 0.04, H + 0.08, H + 0.2, bevel=0.0)
    # a casa de máquinas em cima da torre FR (os elevadores, o ar): o bloco de concreto, as venezianas, a porta, a capa
    mx0, mx1, my0, my1 = 2.7, 4.9, yf0(H) + 0.6, yf1(H) - 0.6
    p.box('CnConcrete#mr', mx0, mx1, my0, my1, H - 0.02, H + 1.6, bevel=0.06)
    p.box('CnPlinth#cap', mx0 - 0.08, mx1 + 0.08, my0 - 0.08, my1 + 0.08, H + 1.6, H + 1.72, bevel=0.03)
    for k in range(5):
        z = H + 0.45 + 0.18 * k
        p.box('Dark#louver', mx1 - 0.01, mx1 + 0.03, my0 + 0.5, my1 - 0.5, z, z + 0.08, bevel=0.0)
    p.box('CnMetal#door', mx0 - 0.04, mx0 + 0.02, (my0 + my1) / 2 - 0.45, (my0 + my1) / 2 + 0.45, H + 0.05, H + 1.3, bevel=0.0)
    beacon(p, mx1 - 0.15, my1 - 0.15, H + 1.72, r=0.08)
    # o ar-condicionado no teto do braço da direita
    for x in (-1.9, 0.3):
        hvac(p, x, (yf0(H) + yf1(H)) / 2, H)


# ===================================================================================================== o telão
def screen(p):
    """O telão de LED na face de dentro do braço (x = XB1, virado pra câmera): o gabinete de aço, a moldura, a borda preta, o campo
    vermelho fixo e a propaganda acesa por cima (os raios Fire, as 5 estrelas e o 中国 em Light com o contorno escuro, o letreiro), a
    marquise por cima com as mãos-francesas."""
    X0 = XB1

    def S(u, v, n):
        return Vector((X0 + n, -u, v))                  # u: a direita de quem olha de +X (−Y); v = z; n pra fora (+X)
    p.box('CnMetal#scr', X0 - 0.02, X0 + 0.3, -HW, HW, HZ0, HZ1, bevel=0.03)
    fw = 0.26
    for (u0, u1, v0, v1) in ((-HW, HW, HZ0, HZ0 + fw), (-HW, HW, HZ1 - fw, HZ1), (-HW, -HW + fw, HZ0 + fw, HZ1 - fw),
                             (HW - fw, HW, HZ0 + fw, HZ1 - fw)):
        p.box('CnMetal#bezel', X0 + 0.28, X0 + 0.44, -u1, -u0, v0, v1, bevel=0.04)
    # a borda preta e o campo vermelho
    ib = SW + 0.12
    for (u0, u1, v0, v1) in ((-ib, ib, SZ0 - 0.12, SZ0), (-ib, ib, SZ1, SZ1 + 0.12), (-ib, -SW, SZ0, SZ1), (SW, ib, SZ0, SZ1)):
        lay(p, 'Dark#border', S, rect(u0, u1, v0, v1), 0.29, 0.33)
    lay(p, 'Red#field', S, rect(-SW, SW, SZ0, SZ1), 0.29, 0.33)
    # o letreiro do jornal embaixo (a faixa escura, a etiqueta acesa, o texto em blocos dourados)
    tv0, tv1 = SZ0 + 0.07, SZ0 + 0.52
    lay(p, 'Dark#ticker', S, rect(-SW + 0.04, SW - 0.04, tv0, tv1), 0.33, 0.345)
    lay(p, 'Fire', S, rect(-SW + 0.1, -SW + 1.05, tv0 + 0.05, tv1 - 0.05), 0.345, 0.355)
    for k in range(2):
        u = -SW + 0.3 + k * 0.38
        lay(p, 'Light', S, rect(u, u + 0.26, tv0 + 0.1, tv1 - 0.1), 0.355, 0.365)
    u, k = -SW + 1.25, 0
    while u < SW - 0.35:
        w = 0.17 + 0.14 * _hash(k, 7, 3)
        lay(p, 'Light', S, rect(u, u + w, tv0 + 0.11, tv1 - 0.11), 0.345, 0.357)
        u += w + (0.3 if _hash(k, 7, 5) > 0.78 else 0.08)
        k += 1
    # o sol: os raios acesos saindo da estrela grande (cortados no campo, por cima do letreiro)
    cu, cv = -3.3, 9.0
    for k in range(18):
        th = math.radians(20.0 * k + 10.0)
        hw = math.radians(5.0)
        wedge = [(cu + 1.15 * math.cos(th - hw * 0.4), cv + 1.15 * math.sin(th - hw * 0.4)),
                 (cu + 14.0 * math.cos(th - hw), cv + 14.0 * math.sin(th - hw)),
                 (cu + 14.0 * math.cos(th + hw), cv + 14.0 * math.sin(th + hw)),
                 (cu + 1.15 * math.cos(th + hw * 0.4), cv + 1.15 * math.sin(th + hw * 0.4))]
        pts = clip_rect(wedge, -SW, SW, tv1 + 0.06, SZ1)
        if len(pts) >= 3:
            lay(p, 'Fire', S, pts, 0.33, 0.342)
    # as 5 estrelas douradas (a bandeira: a grande e as 4 pequenas viradas pra ela), com o contorno escuro
    U = 0.317
    stars = [(cu, cv, 3 * U, 90.0)]
    for (du, dv) in ((5, 3), (7, 1), (7, -2), (5, -4)):
        su, sv = cu + du * U, cv + dv * U
        stars.append((su, sv, U, math.degrees(math.atan2(cv - sv, cu - su))))
    for (su, sv, r, rot) in stars:
        lay(p, 'Dark#outline', S, star_points(su, sv, r * 1.22, r_in=r * 1.22 * 0.42, rot_deg=rot), 0.342, 0.352)
        lay(p, 'Light', S, star_points(su, sv, r, rot_deg=rot), 0.352, 0.366)
    # o 中国 dourado grande
    for (ch, gu) in (('中', 0.85), ('国', 3.3)):
        glyph(p, S, ch, gu, 8.72, 2.05, 0.352, 0.014, m='Light', w=0.11, outline=('Dark#outline', 0.07, 0.01))
    # a marquinha do canal no canto (o próprio laço: o quadrado torto)
    bu, bv, bs = 4.42, 10.02, 0.5
    for q in ([(bu - bs / 2, bv - bs / 2), (bu + bs / 2 - 0.1, bv - bs / 2), (bu + bs / 2 - 0.1, bv - bs / 2 + 0.11),
               (bu - bs / 2, bv - bs / 2 + 0.11)],
              [(bu - bs / 2, bv - bs / 2), (bu - bs / 2 + 0.11, bv - bs / 2), (bu - bs / 2 + 0.2, bv + bs / 2),
               (bu - bs / 2 + 0.09, bv + bs / 2)],
              [(bu - bs / 2 + 0.09, bv + bs / 2 - 0.11), (bu + bs / 2, bv + bs / 2 - 0.11), (bu + bs / 2, bv + bs / 2),
               (bu - bs / 2 + 0.09, bv + bs / 2)],
              [(bu + bs / 2 - 0.21, bv - bs / 2), (bu + bs / 2 - 0.1, bv - bs / 2), (bu + bs / 2, bv + bs / 2),
               (bu + bs / 2 - 0.11, bv + bs / 2)]):
        lay(p, 'Light', S, q, 0.342, 0.356)
    # a marquise por cima (o sombreiro de aço) e as mãos-francesas
    p.box('CnMetal#hood', X0 + 0.25, X0 + 1.0, -HW - 0.1, HW + 0.1, HZ1 - 0.02, HZ1 + 0.12, bevel=0.03)
    p.box('Player#hood', X0 + 0.97, X0 + 1.05, -HW - 0.12, HW + 0.12, HZ1 - 0.1, HZ1 + 0.14, bevel=0.0)
    for y in (-4.0, 0.0, 4.0):                       # as mãos-francesas em cima da moldura (não passam na frente do campo)
        zb = HZ1 - fw + 0.02
        hexa(p, 'CnMetal#hbr', [(X0 + 0.42, y - 0.05, zb), (X0 + 0.48, y - 0.05, zb), (X0 + 0.42, y + 0.05, zb),
                                 (X0 + 0.48, y + 0.05, zb), (X0 + 0.42, y - 0.05, HZ1), (X0 + 0.95, y - 0.05, HZ1),
                                 (X0 + 0.42, y + 0.05, HZ1), (X0 + 0.95, y + 0.05, HZ1)], 0.0)


# ===================================================================================================== as faixas de slogan
def banner(p, F, uc, mir, text, z_top=10.3, z_bot=4.1, w=1.5, nb=0.34):
    """A faixa vermelha vertical do slogan pendurada na face F (u = uc; mir = −1 nas faces em que o u corre pra esquerda de quem
    olha): o pano (cor do jogador) com o friso dourado das bordas, os caracteres dourados de cima pra baixo, o varão de cima com as
    pontas douradas, o peso de baixo e as mãos-francesas até a fachada."""
    wbox(p, 'Player#banner', F, uc - w / 2, uc + w / 2, z_bot, z_top, nb, nb + 0.04, bevel=0.0)
    for s in (-1, 1):
        u = uc + s * (w / 2 - 0.05)
        wbox(p, 'CnGold#trim', F, u - 0.04, u + 0.04, z_bot + 0.05, z_top - 0.05, nb + 0.04, nb + 0.055, bevel=0.0)

    def M(u, v, n):
        return Vector(F.pt(uc + mir * u, v, n))
    cs = w - 0.36
    for k, ch in enumerate(text):
        cv = z_top - 0.42 - cs / 2 - k * (cs + 0.24)
        glyph(p, M, ch, 0.0, cv, cs, nb + 0.04, 0.02, m='CnGold#glyph', w=0.11)
    # o varão de cima (com as pontas) e o peso de baixo, ao longo de u
    for (z, r) in ((z_top + 0.05, 0.06), (z_bot - 0.04, 0.05)):
        a, b = Vector(F.pt(uc - w / 2 - 0.14, z, nb + 0.02)), Vector(F.pt(uc + w / 2 + 0.14, z, nb + 0.02))
        tube(p, 'CnMetal#rod', tuple(a), tuple(b), r, seg=10)
        for q in (a, b):
            p.cyl('CnGold#knob', tuple(q), tuple(b - a), r * 1.6, 0.1, seg=10, bevel=0.0)
    for z in (z_top + 0.05, z_bot - 0.04):
        for s in (-1, 1):
            u = uc + s * (w / 2 - 0.2)
            wbox(p, 'CnMetal#brk', F, u - 0.05, u + 0.05, z - 0.06, z + 0.06, 0.0, nb + 0.03, bevel=0.0)


def banners(p):
    banner(p, WallFace('+x', XF1), 8.03, -1, '人民万岁')
    banner(p, WallFace('+x', XB1), -8.03, -1, '中国万岁')
    banner(p, WallFace('-y', YB0, 0.0, LEAN), -4.8, -1, '中国人民')


# ===================================================================================================== a base (o estúdio)
def podium(p):
    """A base em L (o estúdio de concreto oliva da China): as paredes com a pele de placas, o portão aceso com a marquise e a estrela do
    ELP no brasão, as janelas, o saguão aceso virado pra praça, a quina chanfrada, o talude no pé, a faixa vermelha e a cornija no alto,
    o parapeito com a capa clara, o teto de lajotas com as antenas de satélite e o ar-condicionado."""
    T, WZ, ZS = 0.4, 2.9, [0.0, 0.95, 2.55]
    pier = 0.8
    uin = pier / 2 + 0.02
    Ff, Fl = WallFace('+x', BX1), WallFace('-y', BY0)
    Fo, Fj = WallFace('-x', TX1), WallFace('+y', BYI)
    door = (DOOR_Y0, DOOR_Y1, 0.0, 2.45)
    wins_f = [(-10.1, -8.9, 1.05, 2.3), (-7.0, -5.8, 1.05, 2.3), (1.4, 2.6, 1.05, 2.3), (4.0, 5.2, 1.05, 2.3)]
    roll_l = (-1.7, 0.5, 0.0, 2.4)
    wins_l = [(1.6, 2.8, 1.05, 2.3), (3.5, 4.7, 1.05, 2.3)]
    door_j = (-1.2, 0.4, 0.0, 2.45)
    cn_wall_skin(p, Ff, BY0 + uin, 6.8, 0.0, WZ, holes=[door] + wins_f, zs=ZS, t=T)
    cn_wall(p, Ff, BY0, BY0 + uin + 0.012, 0.0, WZ, t=T, m='CnConcrete#core', panels=False)
    cn_wall_skin(p, Fl, XB1, BX1 - uin, 0.0, WZ, holes=[roll_l] + wins_l, zs=ZS, t=T)
    cn_wall(p, Fl, BX1 - uin - 0.012, BX1, 0.0, WZ, t=T, m='CnConcrete#core', panels=False)
    # (2ª volta) a ala da frente virou a ARQUIBANCADA virada pro telão (o lugar da multidão): na 1ª volta a ala cheia escondia a praça
    # inteira da câmera do jogo e o furo do laço lia vazio. Fica o bloco de fora (o portão, as janelas) com a parede de dentro curta
    # por cima do último degrau
    cn_wall(p, Fo, BYI, TY1, 2.3, WZ, t=T, m='CnConcrete#core', panels=False)
    cn_wall_skin(p, Fj, XB1, TX1, 0.0, WZ, holes=[door_j], zs=ZS, t=T)
    cn_window(p, Ff, DOOR_Y0, DOOR_Y1, 0.0, 2.45, t=T, mull=1, sill=False, lit=True)
    for (u0, u1, z0, z1) in wins_f:
        cn_window(p, Ff, u0, u1, z0, z1, t=T)
    for (u0, u1, z0, z1) in wins_l:
        cn_window(p, Fl, u0, u1, z0, z1, t=T)
    cn_rollup(p, Fl, roll_l[0], roll_l[1], roll_l[2], roll_l[3], t=T)
    cn_window(p, Fj, door_j[0], door_j[1], door_j[2], door_j[3], t=T, mull=1, sill=False, lit=True)
    tribune(p)
    # a quina chanfrada da frente-esquerda (FL), como os pilares de quina do cn_block
    o = 0.12
    P = []
    for z in (0.0, BZ + 0.02):
        for yy in (BY0 + pier / 2, BY0 - o):
            for xx in (BX1 - pier / 2, BX1 + o):
                P.append((xx, yy, z))
    hexa(p, 'CnConcrete#pier', P, bevel=0.12)
    # a laje do teto (com espessura) e o interior mínimo da ala esquerda (as vigas)
    p.box('CnConcrete#slab', TX1, BX1, BYI - 0.02, 6.8, WZ, BZ, bevel=0.03)
    p.box('CnConcrete#slab', XB1, BX1, BY0, BYI, WZ, BZ, bevel=0.03)
    interior_frame(p, XB1 + 0.05, BX1 - T, BY0 + T, BYI - T, 0.0, WZ, along='x', step=3.2)
    # o talude no pé das faces de fora (sai 0,36 m embaixo e morre na placa), cortado no portão e na porta de enrolar
    for (F, segs) in ((Ff, [(BY0 + 0.3, DOOR_Y0 - 0.25), (DOOR_Y1 + 0.25, 6.8)]),
                      (Fl, [(XB1, roll_l[0] - 0.25), (roll_l[1] + 0.25, BX1 - 0.3)])):
        for (a, b) in segs:
            wbox(p, 'CnConcrete#tal', F, a, b, 0.0, 0.7, -0.2, (lambda z: 0.12 + 0.36 * max(0.0, 1.0 - z / 0.7) + 0.005), bevel=0.1)
    # a faixa vermelha e a cornija no alto das faces de fora; a cornija simples nas de dentro; o parapeito com a capa clara
    outer = [(XB1, BY0), (BX1, BY0), (BX1, 6.8)]
    inner = [(TX1, TY1), (TX1, BYI), (XB1, BYI)]          # (a esquerda de quem anda é o lado de dentro da base nos dois)
    run_box(p, 'Player#fascia', outer, -0.26, 0.05, 2.5, 2.98, bevel=0.02)
    run_box(p, 'CnConcrete#corn', outer, -0.34, 0.1, 2.98, BZ + 0.02, bevel=0.04)
    run_box(p, 'CnConcrete#corn', inner, -0.22, 0.1, 2.98, BZ + 0.02, bevel=0.04)
    for run in (outer, inner):
        run_box(p, 'CnConcrete#par', run, -0.05, 0.27, BZ, BZ + 0.45, bevel=0.03)
        run_box(p, 'CnPlinth#cap', run, -0.13, 0.35, BZ + 0.45, BZ + 0.55, bevel=0.03)
    roof_slabs(p, TX1 + 0.32, BX1 - 0.32, BYI - 0.32, 6.4, BZ, size=2.6, m='CnConcrete#rslab')
    roof_slabs(p, XB1 + 0.05, BX1 - 0.32, BY0 + 0.32, BYI - 0.32, BZ, size=2.6, m='CnConcrete#rslab')
    # o portão: a soleira, a marquise de aço em balanço com a borda vermelha e os tirantes, as luminárias
    cy = (DOOR_Y0 + DOOR_Y1) / 2
    p.box('CnPlinth#step', BX1 - 0.1, BX1 + 0.95, DOOR_Y0 - 0.45, DOOR_Y1 + 0.45, -0.1, 0.12, bevel=0.03)
    p.box('CnMetal#canopy', BX1 + 0.1, BX1 + 1.22, DOOR_Y0 - 0.6, DOOR_Y1 + 0.6, 2.52, 2.7, bevel=0.03)
    p.box('Player#canopy', BX1 + 1.2, BX1 + 1.3, DOOR_Y0 - 0.62, DOOR_Y1 + 0.62, 2.47, 2.75, bevel=0.0)
    for y in (DOOR_Y0 - 0.3, DOOR_Y1 + 0.3):
        tube(p, 'CnMetal#tie', (BX1 + 1.1, y, 2.7), (BX1 + 0.14, y, 3.05), 0.035)
    for y in (DOOR_Y0 - 0.55, DOOR_Y1 + 0.55):
        lamp(p, BX1 + 0.12, y, 2.0, '+x')
    # o brasão em cima do portão: o bloco de concreto que sobe do parapeito com a estrela do ELP aprovada
    p.box('CnConcrete#crest', BX1 - 0.35, BX1 + 0.16, cy - 1.15, cy + 1.15, WZ, BZ + 1.75, bevel=0.06)
    p.box('CnPlinth#cap', BX1 - 0.42, BX1 + 0.23, cy - 1.22, cy + 1.22, BZ + 1.75, BZ + 1.86, bevel=0.03)
    p.box('Player#crest', BX1 + 0.16, BX1 + 0.2, cy - 1.15, cy + 1.15, BZ + 1.45, BZ + 1.6, bevel=0.0)
    star_wall(p, (BX1 + 0.16, cy, BZ + 0.72), '+x', 0.62, t=0.03)
    # as antenas de satélite do link de subida no teto da ala esquerda (viradas pro céu do lado da câmera)
    for x in (-1.0, 2.5):
        sat_dish(p, x, -9.5, BZ)


def tribune(p):
    """A arquibancada da praça (o lugar da multidão, virada pro telão): 4 degraus de concreto (0,5 m de piso, 0,6 m de espelho) com o
    focinho claro e a fileira de assentos vermelhos (o vermelho fixo: os bancos do Grande Salão do Povo) em cada um, a escada do meio
    cortando os degraus, e a parede de fecho junto da torre FR."""
    AW = 0.7                                         # a meia-largura da escada do meio
    for k in range(4):
        x0, x1, top = TX1 - 0.5 * (k + 1), TX1 - 0.5 * k, 2.4 - 0.6 * k
        for (ya, yb) in ((BYI, -AW), (AW, TY1)):
            p.box('CnConcrete#step', x0, x1 + 0.02, ya, yb, -0.05, top, bevel=0.04)
            p.box('CnPlinth#nose', x0 - 0.04, x0 + 0.2, ya + 0.05, yb - 0.05, top - 0.1, top + 0.04, bevel=0.03)
            p.box('Red#seat', x1 - 0.2, x1 - 0.08, ya + 0.2, yb - 0.2, top, top + 0.42, bevel=0.0)
            p.box('Red#seat', x1 - 0.42, x1 - 0.08, ya + 0.2, yb - 0.2, top + 0.18, top + 0.24, bevel=0.0)
    # a escada do meio (os degraus miúdos de 0,3 m entre os grandes) e os corrimãos
    for j in range(8):
        x1 = TX1 - 0.25 * (7 - j)
        p.box('CnPlinth#stair', x1 - 0.25, x1 + 0.02, -AW, AW, -0.05, 0.3 * (j + 1), bevel=0.03)
    ra, rb = (TX0 - 0.1, 0.95), (TX1 - 0.2, 2.4 + 0.95)
    for s in (-1, 1):
        tube(p, 'CnMetal#rail', (ra[0], s * (AW - 0.08), ra[1]), (rb[0], s * (AW - 0.08), rb[1]), 0.04)
        for k in range(4):
            x = TX0 + 0.3 + 0.5 * k
            zb = 0.3 * (int((x - TX0) / 0.25) + 1)
            zt = ra[1] + (x - ra[0]) * (rb[1] - ra[1]) / (rb[0] - ra[0])
            p.cyl('CnMetal#rail', (x, s * (AW - 0.08), (zb + zt) / 2), 'z', 0.035, zt - zb, seg=6, bevel=0.0)
    # a parede de fecho junto da torre FR (cobre o pé da torre) com a capa
    p.box('CnConcrete#cheek', TX0, TX1 + 0.02, TY1, 6.8, -0.05, BZ + 0.02, bevel=0.06)
    p.box('CnPlinth#cap', TX0 - 0.07, TX1 + 0.02, TY1 - 0.07, 6.8, BZ + 0.02, BZ + 0.12, bevel=0.03)


def sat_dish(p, x, y, z0, r=1.3, az=-22.0, el=36.0):
    """A antena de satélite do link de subida (a emissora): a sapata, o pedestal, a cabeça da montagem, o prato branco (a parábola com
    a borda e as costas), o cubo e as costelas atrás, o tripé e a corneta do alimentador no foco."""
    p.box('CnConcrete#pad', x - 0.6, x + 0.6, y - 0.6, y + 0.6, z0 - 0.02, z0 + 0.25, bevel=0.04)
    p.cyl('CnMetal#ped', (x, y, z0 + 0.85), 'z', 0.17, 1.2, seg=14, bevel=0.0)
    hub = Vector((x, y, z0 + 1.55))
    a, e = math.radians(az), math.radians(el)
    d = Vector((math.cos(e) * math.cos(a), math.cos(e) * math.sin(a), math.sin(e)))
    p.obox('CnMetal#mount', tuple(hub), (0.5, 0.44, 0.36), yaw=az, bevel=0.03)
    V0 = hub + d * 0.42
    f = 0.95
    prof = [(r * k / 8, (r * k / 8) ** 2 / (4 * f)) for k in range(9)]
    rim = r * r / (4 * f)
    prof += [(r + 0.04, rim), (r + 0.04, rim - 0.08)]
    prof += [(r * k / 8 * 0.97, (r * k / 8 * 0.97) ** 2 / (4 * f) - 0.08 - 0.05 * (1 - k / 8)) for k in range(8, -1, -1)]
    vp.lathe(p, 'Paint#dish', tuple(V0), tuple(d), prof, seg=40)
    p.cyl('CnMetal#hub', tuple(V0 - d * 0.22), tuple(d), 0.3, 0.3, seg=16, bevel=0.0)
    tube(p, 'CnMetal#arm', tuple(hub), tuple(V0 - d * 0.1), 0.12, seg=10)
    E1 = d.cross(Vector((0.0, 0.0, 1.0))).normalized()
    E2 = E1.cross(d).normalized()
    Fc = V0 + d * f
    for k in range(3):
        t = math.radians(90.0 + 120.0 * k)
        q = V0 + d * (rim - 0.02) + (E1 * math.cos(t) + E2 * math.sin(t)) * (r - 0.06)
        tube(p, 'CnMetal#strut', tuple(q), tuple(Fc - d * 0.1), 0.028, seg=6)
    for k in range(4):                                                     # as costelas atrás do prato
        t = math.radians(45.0 + 90.0 * k)
        q0 = V0 - d * 0.1 + (E1 * math.cos(t) + E2 * math.sin(t)) * 0.28
        q1 = V0 + d * (rim * 0.55 - 0.1) + (E1 * math.cos(t) + E2 * math.sin(t)) * (r * 0.75)
        tube(p, 'CnMetal#rib', tuple(q0), tuple(q1), 0.04, seg=6)
    p.cyl('CnMetal#feed', tuple(Fc - d * 0.06), tuple(d), 0.09, 0.26, seg=12, r2=0.13, bevel=0.0)
    p.cyl('Dark#feed', tuple(Fc - d * 0.2), tuple(d), 0.1, 0.06, seg=12, bevel=0.0)


def hvac(p, x, y, z0):
    """A unidade de ar-condicionado do teto: a base, a caixa, a grade do ventilador redonda em cima, as aletas do lado."""
    p.box('CnMetal#hvbase', x - 0.85, x + 0.85, y - 0.6, y + 0.6, z0 - 0.02, z0 + 0.15, bevel=0.0)
    p.box('CnMetal#hvac', x - 0.8, x + 0.8, y - 0.55, y + 0.55, z0 + 0.15, z0 + 1.0, bevel=0.04)
    for s in (-1, 1):
        c = (x + s * 0.4, y, z0 + 1.0)
        p.cyl('Dark#fan', (c[0], c[1], c[2] + 0.01), 'z', 0.32, 0.03, seg=20, bevel=0.0)
        p.torus('CnMetal#fanring', (c[0], c[1], c[2] + 0.04), 'z', 0.33, 0.03, seg=20, rseg=6)
        p.box('CnMetal#fanring', c[0] - 0.33, c[0] + 0.33, c[1] - 0.02, c[1] + 0.02, c[2] + 0.02, c[2] + 0.06, bevel=0.0)
    for k in range(6):
        xx = x - 0.65 + k * 0.26
        p.box('Dark#fin', xx - 0.03, xx + 0.03, y - 0.58, y - 0.55, z0 + 0.25, z0 + 0.9, bevel=0.0)


# ===================================================================================================== a praça e o pé
def horn(p, base, d, r0, rh, L, m='Paint#horn'):
    """A corneta de lata (a boca alargada exponencial, o lábio e o fundo cônico escuro): sólido fechado, com o disco escuro no fundo
    da boca e o aro dourado perto da boca (a mesma da v2)."""
    k = math.log(rh / r0)
    prof = [(0.0, 0.0)] + [(r0 * math.exp(k * t ** 1.6), L * t) for t in (0.0, 0.2, 0.4, 0.6, 0.75, 0.87, 0.95, 1.0)]
    prof += [(rh - 0.035 - 0.02 * rh, L), (rh * 0.6, L - 0.17 * rh / 0.6), (0.0, L - 0.32 * rh / 0.6)]
    vp.lathe(p, m, tuple(base), tuple(d), prof, seg=24)
    D = Vector(d).normalized()
    p.cyl('Dark#mouth', tuple(Vector(base) + D * (L - 0.2 * rh / 0.6)), tuple(D), rh * 0.66, 0.03, seg=20, bevel=0.0)
    p.cyl('CnGold#horn', tuple(Vector(base) + D * (L * 0.82)), tuple(D), r0 * math.exp(k * 0.82 ** 1.6) + 0.025, 0.07, seg=20, bevel=0.0)


def speaker_pole(p, x, y, h=4.8, yaw0=0.0):
    """O poste de alto-falante da praça (o feixe em miniatura): a sapata, o poste, as 3 cornetas brancas e a luminária."""
    vp.lathe(p, 'CnPlinth#foot', (x, y, 0.0), 'z', [(0.0, -0.3), (0.34, -0.3), (0.34, 0.36), (0.28, 0.42), (0.0, 0.42)], seg=20)
    p.cyl('CnMetal#pole', (x, y, (0.42 + h) / 2), 'z', 0.08, h - 0.42, seg=10, bevel=0.0)
    p.cyl('CnMetal#pole', (x, y, h + 0.08), 'z', 0.15, 0.18, seg=12, bevel=0.0)
    for yaw in (yaw0, yaw0 + 120.0, yaw0 + 240.0):
        a = math.radians(yaw)
        d = Vector((math.cos(a) * math.cos(math.radians(-12)), math.sin(a) * math.cos(math.radians(-12)), math.sin(math.radians(-12))))
        horn(p, Vector((x, y, h - 0.15)) + d * 0.1, d, 0.07, 0.28, 0.75)


def lamp_post(p, x, y, h=4.4, face=1.0):
    """O poste de luz da praça: a sapata, o poste, o braço e a luminária acesa virada pra baixo."""
    vp.lathe(p, 'CnPlinth#foot', (x, y, 0.0), 'z', [(0.0, -0.3), (0.3, -0.3), (0.3, 0.3), (0.24, 0.36), (0.0, 0.36)], seg=16)
    p.cyl('CnMetal#pole', (x, y, (0.36 + h) / 2), 'z', 0.07, h - 0.36, seg=10, bevel=0.0)
    tube(p, 'CnMetal#parm', (x, y, h - 0.1), (x - 0.75 * face, y, h + 0.05), 0.05)
    p.box('Dark#lamp', x - 0.95 * face - 0.22, x - 0.95 * face + 0.22, y - 0.16, y + 0.16, h - 0.05, h + 0.12, bevel=0.02)
    p.box('Light', x - 0.95 * face - 0.18, x - 0.95 * face + 0.18, y - 0.12, y + 0.12, h - 0.08, h - 0.04, bevel=0.0)


def plaza(p):
    """A praça dentro do laço, na frente do telão: as pedras claras com a junta, o meio-fio dos lados abertos, a estrela do ELP grande
    no piso, os postes de luz e os postes de alto-falante."""
    x0, x1, y0, y1 = -2.9, TX0 - 0.03, -7.6, 5.8
    xs, ys = splits(x0, x1, 1.5), splits(y0, y1, 1.5)
    for i in range(len(xs) - 1):
        for j in range(len(ys) - 1):
            p.box('CnPlinth#pave', xs[i] + 0.025, xs[i + 1] - 0.025, ys[j] + 0.025, ys[j + 1] - 0.025, -0.06, 0.07, bevel=0.03)
    p.box('Dark#pave', x0, x1, y0, y1, -0.08, 0.02, bevel=0.0)
    p.box('CnConcrete#curb', x0 - 0.25, x0, y0, y1 + 0.25, -0.1, 0.13, bevel=0.03)
    p.box('CnConcrete#curb', x0 - 0.25, x1, y1, y1 + 0.25, -0.1, 0.13, bevel=0.03)

    def M(u, v, n):
        return Vector((STAR_C[0] - v, STAR_C[1] - u, 0.07 + n))       # deitada, a ponta pra −X: lê de pé na câmera
    emblem(p, M, STAR_R, 0.0, t=0.025)
    for y in (-7.0, 5.2):
        lamp_post(p, 1.3, y)
    for y in (-6.5, 5.0):                           # (as cornetas a 0, 120 e 240°: nenhuma encosta na torre BL nem na base)
        speaker_pole(p, -2.45, y, yaw0=0.0)


def flags(p):
    """As 3 bandeiras vermelhas no pé, na frente do estúdio (o pano vai pro flags.json: o jogo o desenha no vento)."""
    for k, (y, top) in enumerate(((-9.6, 8.3), (-8.1, 8.9), (-6.6, 8.3))):
        x = 6.55
        vp.lathe(p, 'CnPlinth#foot', (x, y, 0.0), 'z', [(0.0, -0.3), (0.3, -0.3), (0.3, 0.32), (0.25, 0.38), (0.0, 0.38)], seg=20)
        p.cyl('CnMetal#mast', (x, y, (0.36 + top) / 2), 'z', 0.075, top - 0.36, seg=12, bevel=0.0)
        vp.lathe(p, 'CnGold#finial', (x, y, top), 'z', [(0.0, 0.0), (0.11, 0.0), (0.11, 0.06), (0.05, 0.12), (0.0, 0.4)], seg=12)
        flag(p, (x - 0.09, y), top - 0.2, 2.1, 1.4, m='Player', trail=-1.0, phase=k * 1.7)


# ===================================================================================================== o feixe de cornetas (gira)
def speakers():
    """O feixe: a mesa giratória com o aro dourado, o gabinete do transmissor atrás (com as aletas e a estrela do ELP), o garfo com os
    munhões, o tambor dos motores com a faixa vermelha e as 7 cornetas brancas gigantes num buquê (uma no meio e seis em volta,
    abrindo), presas pelo anel de aço; a luz vermelha em cima do gabinete. Feito na origem (o eixo, no topo do pedestal), apontando
    pra +X e inclinado 8° pra baixo; gira em Z (varre o campo)."""
    p = BPart(PREFIX + '_Speakers')
    vp.lathe(p, 'CnMetal#tt', (0.0, 0.0, 0.0), 'z', [(0.0, 0.0), (1.05, 0.0), (1.05, 0.14), (0.98, 0.2), (0.0, 0.2)], seg=40)
    vp.lathe(p, 'CnGold#ring', (0.0, 0.0, 0.0), 'z', [(1.02, 0.03), (1.09, 0.03), (1.09, 0.11), (1.02, 0.11)], seg=40, closed=True)
    # o gabinete do transmissor (o contrapeso) com as aletas e a estrela
    p.box('CnMetal#amp', -0.98, -0.18, -0.56, 0.56, 0.18, 1.35, bevel=0.04)
    for k in range(5):
        x = -0.88 + k * 0.15
        for s in (-1, 1):
            p.box('Dark#fin', x - 0.03, x + 0.03, s * 0.56 - 0.02, s * 0.56 + 0.02, 0.35, 1.2, bevel=0.0)
    p.box('CnMetal#ampcap', -1.02, -0.14, -0.6, 0.6, 1.35, 1.43, bevel=0.02)
    star_wall(p, (-0.98, 0.0, 0.78), '-x', 0.32, t=0.02)
    beacon(p, -0.58, 0.0, 1.43, r=0.11)
    # o garfo e os munhões
    # (2ª volta) o buquê maior (cornetas de 3 m, boca de 1,2 m, o feixe com ~4 m de boca): na 1ª volta lia como uma florzinha branca
    # em cima do telão, pequeno demais pra ser a assinatura
    PX, PZ = 0.25, 2.55
    for s in (-1, 1):
        y0, y1 = sorted((s * 0.78, s * 0.92))
        hexa(p, 'CnMetal#yoke', [(-0.3, y0, 0.18), (0.8, y0, 0.18), (-0.3, y1, 0.18), (0.8, y1, 0.18),
                                 (PX - 0.3, y0, PZ + 0.32), (PX + 0.3, y0, PZ + 0.32), (PX - 0.3, y1, PZ + 0.32),
                                 (PX + 0.3, y1, PZ + 0.32)], 0.04)
        p.cyl('CnMetal#trun', (PX, s * 0.8, PZ), 'y', 0.22, 0.5, seg=16, bevel=0.0)      # entra no tambor (r 0,66)
        p.cyl('CnGold#cap', (PX, s * 1.07, PZ), 'y', 0.14, 0.05, seg=16, bevel=0.0)
    # o tambor dos motores (inclinado 10° pra baixo) com a faixa vermelha
    t = math.radians(-10.0)
    A = Vector((math.cos(t), 0.0, math.sin(t)))
    E1 = Vector((0.0, 1.0, 0.0))
    E2 = A.cross(E1).normalized()
    if E2.z < 0:
        E2 = -E2
    P0 = Vector((PX, 0.0, PZ))
    vp.lathe(p, 'CnMetal#drum', tuple(P0 - A * 0.6), tuple(A), [(0.0, 0.0), (0.52, 0.0), (0.66, 0.12), (0.66, 0.86), (0.6, 0.95),
                                                             (0.0, 0.95)], seg=28)
    vp.lathe(p, 'Player#band', tuple(P0 - A * 0.6), tuple(A), [(0.64, 0.32), (0.7, 0.32), (0.7, 0.58), (0.64, 0.58)], seg=28,
             closed=True)
    F0 = P0 + A * 0.35
    LH, RM, RT = 3.0, 1.3, 0.36
    mouths = []
    for k in range(7):
        if k == 0:
            base, d, L = F0, A, LH + 0.2
        else:
            ph = math.radians(60.0 * k)
            o = E1 * math.cos(ph) + E2 * math.sin(ph)
            base = F0 + o * RT
            d = (A * LH + o * (RM - RT)).normalized()
            L = (A * LH + o * (RM - RT)).length
        vp.lathe(p, 'CnMetal#motor', tuple(base - d * 0.05), tuple(d), [(0.0, 0.0), (0.18, 0.0), (0.2, 0.05), (0.2, 0.24),
                                                                         (0.13, 0.32), (0.0, 0.32)], seg=16)
        horn(p, base + d * 0.27, d, 0.12, 0.6, L - 0.27)
        mouths.append(base + d * (0.27 + (L - 0.27) * 0.55))
    # o anel de aço que segura o buquê (o hexágono por fora das cornetas, a 55% do comprimento) e os raios até a do meio
    for k in range(1, 7):
        a, b = mouths[k], mouths[k % 6 + 1]
        tube(p, 'CnMetal#hoop', tuple(a), tuple(b), 0.05, seg=8)
        tube(p, 'CnMetal#hoop', tuple(a), tuple(mouths[0]), 0.035, seg=6)
    # o cabo grosso do gabinete até o tambor
    vp.tube(p, 'Dark#cable', [(-0.4, 0.3, 1.35), (-0.3, 0.3, 1.9), (PX - 0.55, 0.3, PZ + 0.15)], 0.05, seg=8)
    return p.finish()


# ===================================================================================================== o prédio
def base():
    p = BPart(PREFIX + '_Base')
    loop(p)
    screen(p)
    banners(p)
    podium(p)
    plaza(p)
    flags(p)
    o = p.finish()
    report(o)
    return o


def main():
    clear_scene()
    b = base()
    total = export(b, OUT)
    s = speakers()
    report(s)
    bake_in_place(s, SPK_AT)             # no executor do detalhe, a sombra de contato com as cornetas no lugar delas
    total += export(s, OUT)
    sc = scaffold(PREFIX + '_Scaffold', XB0, BX1, BY0, YF1, 10.5, post='CnMetal', plank='Crate', stripe='Hazard')
    total += export(sc, OUT)
    print('MODEL total', total, 'tris (antes do detalhe)')
    print('MODEL lote 14 x 24 m; speakers spin at_m', [round(v, 2) for v in SPK_AT], 'rpm 3 (o Units.json tem [-1.0, 7.4, 11.05])')
    print('MODEL fire_points_m', [[4.8, -4.2, BZ + 0.6], [-4.8, -3.6, H + 0.3], [3.8, 7.6, H + 1.8]])   # o bloco do portão, o braço, a casa de máquinas
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]], [PREFIX + '_Speakers', [SPK_AT[0], -SPK_AT[1], SPK_AT[2]]]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1100, 1600]],
                      ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1600, 1000]],
                      ['side', 'ortho', [0, 1, 0], [0, 0, 1], [1200, 1000]],
                      ['three_quarter', 'persp', 34.0, 215.0, [1600, 1000], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 32.0, 35.0, [1600, 1000], 30.0, 0.95]],
            'usa_at': [11, -15, 0], 'tank_views': False,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, indent=1)


if __name__ == '__main__':
    main()
