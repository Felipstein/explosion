"""O kit dos veículos da China provisórios por script (o elenco das 3 facções, 31/10/2026: docs/ELENCO.md §4.2).

Os 6 veículos da China (model_china_type99.py, _overlord, _gattling_tank, _inferno_cannon, _dozer, _supply_truck) nascem
daqui pra serem da mesma família (a bíblia: docs/CONCEITO_UNIDADES.md §3, "escura, massiva, disciplinada"):
  - o casco e a torre em sólidos angulosos (o envoltório convexo dos cantos: placa de blindagem plana, quina dura com chanfro);
  - a CAMUFLAGEM DIGITAL: blocos de "pixel" (0,2 m) mais escuros (slot CnTankCamo) colados nas faces de fora, agrupados por um
    ruído no espaço do mundo (as manchas continuam de uma face pra outra, como a pintura de verdade); de cima lê ~0,30;
  - a estrela vermelha (slot Team: troca com a cor do jogador, decisão do Felipe de 05/10);
  - a cor do jogador = as lonas pequenas dobradas e presas rente (Team) e a faixa no cano (Team); nunca faixa pintada;
  - a rodagem (rodas duplas com borracha, roda motriz dentada, roda-guia, roletes) e a esteira por elos, a técnica do
    model_gla_tank.py;
  - a conferência: o giro da torre contra o casco (colisão de verdade por BVH) e as medidas.
Em cima do model_lib.py (Part, V, track_path). Não mexe no model_lib.py nem no model_faction_kit.py (outros agentes usam).

Os slots dos veículos da China: CnTankPaint (o oliva escuro), CnTankCamo (a mancha digital, mais escura), CnTankMetal (o aço
escuro), Track, Rubber, Team (a cor do jogador: lonas, faixa do cano, estrela), Lens, Dark, Cloth (a lona neutra), Light
(o farol aceso); o Dozer usa também o Beacon (a cúpula da sirene que pisca, o mesmo slot do Dozer da USA).
"""
import json
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

from model_lib import COLORS, DETAIL, Part, V, track_path  # noqa: F401
from model_faction_kit import star_points  # noqa: F401  (o kit também põe as cores de China e GLA no COLORS)
import model_vehicle_parts as vp  # noqa: E402  (31/10: as peças de detalhe: rodas, esteira, cano, lona, equipamento)

# as cores de conferência (o jogo troca pelas instâncias): a China escura (o oliva e a mancha do model_faction_kit.py), o
# vermelho da cor do jogador e a lona neutra num cáqui-oliva de lona (a do kit é a lona de adobe do GLA)
COLORS.update({'CnTankPaint': (82, 90, 62), 'CnTankCamo': (56, 64, 44), 'CnTankMetal': (52, 54, 50),
               'Track': (44, 42, 38), 'Rubber': (28, 28, 28), 'Team': (176, 38, 32), 'Lens': (40, 60, 80),
               'Dark': (36, 37, 34), 'Cloth': (104, 100, 74), 'Light': (255, 230, 170), 'Beacon': (255, 170, 40)})


# ===================================================================================================== utilidades
def U(co):
    """Do Blender pras coordenadas da Unreal (x, y, z)."""
    return Vector((co[0], -co[1], co[2]))


def frame_box(p, m, c, ex, ey, ez, sx, sy, sz, bevel=None):
    """Caixa num referencial qualquer: centro c, eixos ex/ey/ez (Unreal, ortonormais), tamanhos sx/sy/sz."""
    b = p.bm(m, bevel if bevel is not None else 0.02)
    c, ex, ey, ez = Vector(c), Vector(ex), Vector(ey), Vector(ez)
    vs = [b.verts.new(V(*(c + ex * (sx / 2 * i) + ey * (sy / 2 * j) + ez * (sz / 2 * k))))
          for k in (-1, 1) for j in (-1, 1) for i in (-1, 1)]
    for f in [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]:
        b.faces.new([vs[i] for i in f])


def basis(n, up=(0, 0, 1)):
    """Um referencial com o z na normal n: ex horizontal (ou ao longo de X numa face deitada), ey = n × ex."""
    n = Vector(n).normalized()
    if abs(n.z) > 0.7:
        ex = Vector((1, 0, 0)) - n * n.x
    else:
        ex = Vector(up).cross(n)
    ex.normalize()
    ey = n.cross(ex)
    return ex, ey, n


def solid(p, m, pts, bevel=None):
    """O sólido anguloso: o envoltório convexo dos cantos (Unreal), com as faces coplanares fundidas (placas planas, o chanfro
    do harden pega só as quinas). Devolve as faces [(pontos, normal pra fora)] pra camuflagem e pra colar peças."""
    b = p.bm(m, bevel if bevel is not None else 0.04)
    vs = [b.verts.new(V(*q)) for q in pts]
    res = bmesh.ops.convex_hull(b, input=vs, use_existing_faces=False)
    kill = list({g for g in res['geom_interior'] + res['geom_unused'] if isinstance(g, bmesh.types.BMVert) and g.is_valid})
    if kill:
        bmesh.ops.delete(b, geom=kill, context='VERTS')
    faces = [g for g in res['geom'] if isinstance(g, bmesh.types.BMFace) and g.is_valid]
    edges = list({e for f in faces for e in f.edges})
    verts = list({v for f in faces for v in f.verts})
    bmesh.ops.dissolve_limit(b, angle_limit=math.radians(2.5), verts=verts, edges=edges)
    vset = {v for v in verts if v.is_valid}
    faces = list({f for v in vset for f in v.link_faces if all(w in vset for w in f.verts)})
    cen = sum((U(v.co) for v in verts if v.is_valid), Vector()) / max(1, len([v for v in verts if v.is_valid]))
    out = []
    for f in faces:
        q = [U(v.co) for v in f.verts]
        c = sum(q, Vector()) / len(q)
        n = U(f.normal)
        if n.dot(c - cen) < 0:
            n = -n
        out.append((q, n))
    return out


def face_area(q):
    a = Vector()
    for i in range(len(q)):
        a += q[i].cross(q[(i + 1) % len(q)])
    return a.length / 2


def find_face(faces, n, near=None):
    """A face cuja normal mais se parece com n (e mais perto do ponto 'near', se dado)."""
    n = Vector(n).normalized()
    best, bs = None, -9
    for q, fn in faces:
        s = fn.dot(n)
        if near is not None:
            c = sum(q, Vector()) / len(q)
            s -= 0.05 * (c - Vector(near)).length
        if s > bs:
            best, bs = (q, fn), s
    return best


def plane_point(face, x=None, y=None, z=None):
    """O ponto na face (plano) com duas coordenadas dadas (a terceira sai do plano)."""
    q, n = face
    d = q[0].dot(n)
    if x is None:
        return Vector(((d - n.y * y - n.z * z) / n.x, y, z))
    if y is None:
        return Vector((x, (d - n.x * x - n.z * z) / n.y, z))
    return Vector((x, y, (d - n.x * x - n.y * y) / n.z))


def on_face(p, m, face, c, size, up=(0, 0, 1), lift=0.0, bevel=None, yaw_in_plane=0.0):
    """Uma caixa deitada na face: centro c (no plano), tamanho (ao longo do ex, do ey, da espessura pra fora)."""
    q, n = face
    ex, ey, ez = basis(n, up)
    if yaw_in_plane:
        a = math.radians(yaw_in_plane)
        ex, ey = ex * math.cos(a) + ey * math.sin(a), ey * math.cos(a) - ex * math.sin(a)
    c = Vector(c) + ez * (size[2] / 2 + lift)
    frame_box(p, m, c, ex, ey, ez, size[0], size[1], size[2], bevel=bevel)


# ===================================================================================================== camuflagem digital
def _hash(i, j, k, seed):
    return (math.sin(i * 127.1 + j * 311.7 + k * 74.7 + seed * 19.19) * 43758.5453) % 1.0


def _vnoise(x, y, z, seed):
    i, j, k = math.floor(x), math.floor(y), math.floor(z)
    fx, fy, fz = x - i, y - j, z - k
    sx, sy, sz = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy), fz * fz * (3 - 2 * fz)
    def h(a, b, c):
        return _hash(i + a, j + b, k + c, seed)
    x00 = h(0, 0, 0) + (h(1, 0, 0) - h(0, 0, 0)) * sx
    x10 = h(0, 1, 0) + (h(1, 1, 0) - h(0, 1, 0)) * sx
    x01 = h(0, 0, 1) + (h(1, 0, 1) - h(0, 0, 1)) * sx
    x11 = h(0, 1, 1) + (h(1, 1, 1) - h(0, 1, 1)) * sx
    y0 = x00 + (x10 - x00) * sy
    y1 = x01 + (x11 - x01) * sy
    return y0 + (y1 - y0) * sz


def camo_value(P, seed=0):
    """O campo da camuflagem: manchas de ~0,6 m com a borda bem quebrada (2 oitavas: a digital do ELP tem os blocos grandes
    picotados em pixels soltos), no espaço do mundo. Metade dos pontos passa de 0,49; 45% passam de 0,51."""
    return 0.55 * _vnoise(P.x / 0.6, P.y / 0.6, P.z / 0.6, seed) + 0.45 * _vnoise(P.x / 0.22 + 17.3, P.y / 0.22, P.z / 0.22, seed + 5)


def _inside(u, v, poly, inset):
    """Ponto dentro do polígono (pela paridade dos cruzamentos) e a 'inset' das bordas."""
    n = len(poly)
    c = False
    for i in range(n):
        (u0, v0), (u1, v1) = poly[i], poly[(i + 1) % n]
        if (v0 > v) != (v1 > v):
            t = (v - v0) / (v1 - v0)
            if u < u0 + t * (u1 - u0):
                c = not c
        du, dv = u1 - u0, v1 - v0
        L2 = du * du + dv * dv
        if L2 > 1e-12:
            t = max(0.0, min(1.0, ((u - u0) * du + (v - v0) * dv) / L2))
            if math.hypot(u - u0 - t * du, v - v0 - t * dv) < inset:
                return False
    return c


def camo_poly(p, pts, n, seed=0, pix=0.16, thr=0.51, inset=0.06, lift=0.002, t=0.006, m='CnTankCamo', holes=()):
    """Os blocos da camuflagem digital numa face plana (pontos Unreal, normal pra fora): as células do grid de 'pix' em que o
    campo passa de 'thr' viram placas finas, juntadas em tiras por linha (menos triângulos). holes = [(u0, u1, v0, v1)...] no
    plano (sem camuflagem, ex.: debaixo da estrela)."""
    if DETAIL['on']:
        return 0   # (31/10, o detalhe AAA) a camuflagem vai pro material (o Camo do CnTankPaint): os blocos colados eram o "lego" literal
    pts = [Vector(q) for q in pts]
    ex, ey, nz = basis(n)
    d = pts[0].dot(nz)
    poly = [(q.dot(ex), q.dot(ey)) for q in pts]
    us, vs = [a for a, _ in poly], [b for _, b in poly]
    i0, i1 = math.floor(min(us) / pix), math.ceil(max(us) / pix)
    j0, j1 = math.floor(min(vs) / pix), math.ceil(max(vs) / pix)
    b = p.bm(m, 0.0)
    count = 0

    def emit(ua, ub, va, vb):
        cs = [(ua, va), (ub, va), (ub, vb), (ua, vb)]
        lo = [b.verts.new(V(*(ex * u + ey * v + nz * (d + lift)))) for u, v in cs]
        hi = [b.verts.new(V(*(ex * u + ey * v + nz * (d + lift + t)))) for u, v in cs]
        b.faces.new(lo[::-1])
        b.faces.new(hi)
        for k in range(4):
            kk = (k + 1) % 4
            b.faces.new([lo[k], lo[kk], hi[kk], hi[k]])

    for j in range(j0, j1):
        va, vb = j * pix, (j + 1) * pix
        run = None
        for i in range(i0, i1 + 1):
            on = False
            if i < i1:
                ua, ub = i * pix, (i + 1) * pix
                P = ex * ((ua + ub) / 2) + ey * ((va + vb) / 2) + nz * d
                on = camo_value(P, seed) > thr and all(_inside(u, v, poly, inset) for u, v in ((ua, va), (ub, va), (ub, vb), (ua, vb)))
                if on and holes:
                    on = not any(h[0] - pix < (ua + ub) / 2 < h[1] + pix and h[2] - pix < (va + vb) / 2 < h[3] + pix for h in holes)
            if on and run is None:
                run = i
            elif not on and run is not None:
                emit(run * pix, i * pix, va, vb)
                count += 1
                run = None
    return count


def camo_faces(p, faces, seed=0, skip=None, min_area=0.15, **kw):
    """A camuflagem em todas as faces de fora de um sólido (sem a de baixo e as que 'skip(c, n)' recusar)."""
    total = 0
    for q, n in faces:
        if n.z < -0.35 or face_area(q) < min_area:
            continue
        c = sum(q, Vector()) / len(q)
        if skip and skip(c, n):
            continue
        total += camo_poly(p, q, n, seed=seed, **kw)
    return total


# ===================================================================================================== estrela, lonas
def star(p, c, n, r, up=(0, 0, 1), m='Team', t=0.008, lift=0.011):
    """A estrela colada na face (normal n), a ponta pra 'up' (projetado no plano). (01/11, APROVADA:
    `docs/aprovados/modelos/2026-11-01_estrelas_russia_china_APROVADAS.jpg`) o emblema do Exército de Libertação Popular: a estrela
    VERMELHA (o slot Red, fixo; o m de antes não pinta mais) com a borda dourada (CnGold) e o 八一 dourado no meio."""
    from model_faction_kit import bayi_strokes
    n = Vector(n).normalized()
    upv = Vector(up) - n * Vector(up).dot(n)
    if upv.length < 1e-4:
        upv = Vector((1, 0, 0)) - n * n.x
    upv.normalize()
    rt = upv.cross(n)
    c = Vector(c)

    def lay(mat, pts2, l0, l1, mir=1.0):
        q = [(mir * u, v) for (u, v) in pts2]
        if mir < 0:
            q = q[::-1]
        ring0 = [tuple(c + rt * u + upv * v + n * l0) for (u, v) in q]
        ring1 = [tuple(c + rt * u + upv * v + n * l1) for (u, v) in q]
        p.loft(mat, [ring0, ring1], bevel=0.0)
    lay('CnGold', star_points(0.0, 0.0, r * 1.12, r_in=r * 1.12 * 0.40, rot_deg=90.0), lift, lift + t)
    lay('Red', star_points(0.0, 0.0, r, rot_deg=90.0), lift + t, lift + 1.6 * t)
    # o 八一 lido de frente: rt = up × n anda pra esquerda de quem olha a face (a Unreal é de mão esquerda), então espelha
    for q in bayi_strokes(r):
        lay('CnGold', q, lift + 1.6 * t, lift + 2.2 * t, mir=-1.0)


def _frame(yaw, pitch, roll):
    cy, sy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    cp, sp = math.cos(math.radians(pitch)), math.sin(math.radians(pitch))
    cr, sr = math.cos(math.radians(roll)), math.sin(math.radians(roll))
    ex = Vector((cp * cy, cp * sy, sp))
    ey0, ez0 = Vector((-sy, cy, 0.0)), Vector((-sp * cy, -sp * sy, cp))
    return ex, ey0 * cr + ez0 * sr, ez0 * cr - ey0 * sr


def pillow(p, m, c, size, yaw=0.0, pitch=0.0, roll=0.0, square=0.32, flat=0.6, seed=0.0, n=10, k=16):
    """Um volume de pano (a lona dobrada, o saco): a seção é uma superelipse quase quadrada (o pano esticado nas quinas), estufa
    um pouco no meio e afina nas pontas; a barriga achata onde apoia. x local ao longo de 'size[0]'."""
    L, W, H = size
    ex, ey, ez = _frame(yaw, pitch, roll)
    C = Vector(c)
    rings = []
    for i in range(n + 1):
        u = i / n
        bulge = math.sin(math.pi * u) ** 0.25
        wob = 0.03 * math.sin(7.0 * u + seed)
        w = W / 2 * (0.86 + 0.14 * bulge + wob)
        t = H / 2 * (0.8 + 0.2 * bulge)
        x = L * (u - 0.5)
        ring = []
        for j in range(k):
            th = 2 * math.pi * j / k
            ct, st = math.cos(th), math.sin(th)
            yy = w * math.copysign(abs(ct) ** square, ct)
            zz = t * math.copysign(abs(st) ** square, st)
            if zz < 0:
                zz *= flat
            q = C + ex * x + ey * yy + ez * zz
            ring.append((q.x, q.y, q.z))
        rings.append(ring)
    p.loft(m + '#cloth', rings, bevel=0.0, smooth=True)


def tarp_fold(p, c, size, yaw=0.0, pitch=0.0, roll=0.0, m='Team', straps=2, strap_m='Cloth', seed=0.0, layers=2):
    """A lona pequena DOBRADA e presa rente (a cor do jogador da China). 31/10, o detalhe AAA (model_vehicle_parts.tarp_bundle): o
    fardo em camadas (o sulco entre as dobras no lado), estrangulado pelas cintas que abraçam a forma, as pontas murchas e as rugas
    do topo; com pitch 90 e uma camada, a ABA caída na grade (tarp_drape: as dobras de pé, a bainha e os nós de corda).
    'c' é o centro da base (onde ela apoia); size = (comprimento, largura, altura) no referencial (yaw, pitch, roll)."""
    L, W, H = size
    if abs(pitch) > 60 and layers <= 1:
        ex, ey, ez = vp.ypr_frame(yaw, pitch, roll)
        C = Vector(c)
        top = C + ex * (L / 2)
        vp.tarp_drape(p, top - ey * (W / 2), top + ey * (W / 2), L, ez, m=m, folds=max(3, int(W / 0.26)), amp=0.016,
                      seed=seed, tie_m=strap_m)
        return
    vp.tarp_bundle(p, c, size, yaw, pitch, roll, m=m, strap_m=strap_m, buckle_m='CnTankMetal', straps=straps,
                   layers=3 if layers > 1 else 2, seed=seed, swap=L < W)


def tarp_roll(p, c, axis, length, r, m='Team', straps=3, strap_m='Cloth'):
    """A lona ENROLADA (o rolo amarrado): model_vehicle_parts.tarp_roll (a espiral, as cintas que apertam, as pontas com as voltas)."""
    vp.tarp_roll(p, c, axis, length, r, m=m, straps=straps, strap_m=strap_m, buckle_m='CnTankMetal')


# ===================================================================================================== equipamentos
def hmg(p, c, yaw=0.0, scale=1.0, m='CnTankMetal', dark='Dark'):
    """A metralhadora pesada de 12,7 mm (QJC-88) no berço (31/10, o detalhe AAA): o pino com a base, o berço, a caixa da arma de
    quinas redondas com a tampa do carregador, o cano redondo com o quebra-chamas, a lata de munição com a alça e os punhos atrás."""
    k = scale
    ex, ey, ez = _frame(yaw, 0, 0)
    C = Vector(c)
    vp.lathe(p, m, C, ez, [(0.0, 0.0), (0.075 * k, 0.0), (0.075 * k, 0.025 * k), (0.042 * k, 0.04 * k), (0.038 * k, 0.29 * k),
                           (0.05 * k, 0.3 * k), (0.0, 0.3 * k)], seg=12)
    vp.rbox(p, m, C + ez * (0.32 * k), (0.24 * k, 0.17 * k, 0.05 * k), yaw=yaw, r=0.02 * k, k=2)
    vp.rbox(p, dark, C + ez * (0.42 * k), (0.62 * k, 0.12 * k, 0.13 * k), yaw=yaw, r=0.025 * k, k=2)         # a caixa da arma
    vp.rbox(p, dark, C + ez * (0.5 * k) + ex * (0.02 * k), (0.3 * k, 0.1 * k, 0.035 * k), yaw=yaw, r=0.02 * k, k=2)  # a tampa
    vp.ammo_box(p, C + ez * (0.41 * k) + ey * (0.17 * k), (0.26 * k, 0.14 * k, 0.18 * k), yaw=yaw, m=m, latch_m=dark)
    tip = C + ez * (0.43 * k) + ex * (0.31 * k)
    vp.lathe(p, dark, tip, ex, [(0.0, 0.0), (0.034 * k, 0.0), (0.03 * k, 0.04 * k), (0.026 * k, 0.1 * k), (0.024 * k, 0.86 * k),
                                (0.036 * k, 0.88 * k), (0.038 * k, 1.0 * k), (0.03 * k, 1.01 * k), (0.012 * k, 1.01 * k),
                                (0.012 * k, 0.97 * k), (0.0, 0.97 * k)], seg=12)
    vp.tube(p, m, vp.bent_bar([tip + ex * (0.06 * k) + ez * 0.02 * k, tip + ex * (0.06 * k) + ez * 0.11 * k,
                               tip + ex * (0.26 * k) + ez * 0.11 * k, tip + ex * (0.26 * k) + ez * 0.02 * k], R=0.03 * k, n=2), 0.012 * k, seg=6)
    # os punhos (espadas) atrás
    back = C + ez * (0.42 * k) - ex * (0.32 * k)
    for sgn in (-1, 1):
        vp.tube(p, dark, vp.bent_bar([back + ey * (sgn * 0.04 * k), back - ex * (0.08 * k) + ey * (sgn * 0.07 * k),
                                      back - ex * (0.1 * k) + ey * (sgn * 0.07 * k) - ez * (0.1 * k)], R=0.03 * k, n=2), 0.016 * k, seg=6)


def smoke_launchers(p, c, yaw, rows=2, cols=3, pitch=25.0, m='CnTankMetal', cap='Dark', r=0.045, L=0.32):
    """O lançador de granadas de fumaça: o bloco com os tubos inclinados pra frente e pra cima."""
    ex, ey, ez = _frame(yaw, pitch, 0)
    C = Vector(c)
    frame_box(p, m, C - ex * (L * 0.25), ex, ey, ez, L * 0.6, cols * r * 2.6, rows * r * 2.6, bevel=0.01)
    for i in range(rows):
        for j in range(cols):
            q = C + ey * ((j - (cols - 1) / 2) * r * 2.4) + ez * ((i - (rows - 1) / 2) * r * 2.4)
            p.cyl(m, tuple(q + ex * (L * 0.2)), tuple(ex), r, L * 0.7, seg=12, bevel=0.0)
            p.cyl(cap, tuple(q + ex * (L * 0.56)), tuple(ex), r * 0.8, 0.02, seg=12, bevel=0.0)


def grille(p, x0, x1, y0, y1, z, n=10, along='x', m='Dark', slat='CnTankMetal', frame_m='CnTankPaint'):
    """A grelha do motor: a moldura, o fundo escuro e as aletas (n), na altura z."""
    p.box(frame_m, x0, x1, y0, y1, z, z + 0.03, bevel=0.01)
    p.box(m, x0 + 0.05, x1 - 0.05, y0 + 0.05, y1 - 0.05, z + 0.03, z + 0.035, bevel=0.0)
    if along == 'x':
        step = (x1 - x0 - 0.1) / n
        for k in range(n):
            xx = x0 + 0.05 + step * (k + 0.5)
            p.obox(slat, (xx, (y0 + y1) / 2, z + 0.055), (0.025, y1 - y0 - 0.1, 0.05), pitch=35, bevel=0.0)
    else:
        step = (y1 - y0 - 0.1) / n
        for k in range(n):
            yy = y0 + 0.05 + step * (k + 0.5)
            p.obox(slat, ((x0 + x1) / 2, yy, z + 0.055), (x1 - x0 - 0.1, 0.025, 0.05), roll=35, bevel=0.0)


def hatch(p, x, y, z, r=0.32, m='CnTankPaint', hinge_side=-1, handle=True):
    """A escotilha redonda (31/10, o detalhe AAA: model_vehicle_parts.hatch_round): o aro soldado em relevo, a tampa abaulada, a
    dobradiça de nós e a alça em U."""
    vp.hatch_round(p, (x, y, z), r, m, 'CnTankMetal', n=(0, 0, 1), hinge_dir=(0, hinge_side, 0),
                   handle_m='CnTankMetal' if handle else None)


def periscope(p, x, y, z, yaw=0.0, m='Dark'):
    p.obox('CnTankPaint', (x, y, z + 0.05), (0.18, 0.24, 0.1), yaw=yaw, bevel=0.01)
    a = math.radians(yaw)
    p.obox(m, (x + 0.09 * math.cos(a), y + 0.09 * math.sin(a), z + 0.06), (0.02, 0.18, 0.06), yaw=yaw, bevel=0.0)


def headlight(p, c, face='+x', r=0.09, guard=True, lit=False):
    """O farol redondo (31/10, o detalhe AAA): o copo de aço com o aro, o vidro abaulado (aceso só se lit: o caminhão e o Dozer) e a
    guarda de barra dobrada na frente."""
    x, y, z = c
    vp.lathe(p, 'CnTankMetal', c, 'x', [(0.0, -0.08), (r * 0.7, -0.08), (r + 0.02, -0.05), (r + 0.026, 0.02), (r + 0.026, 0.05),
                                        (r + 0.014, 0.062), (r, 0.058), (r, 0.04), (0.0, 0.04)], seg=16)
    vp.lathe(p, 'Lens', c, 'x', [(0.0, 0.062), (r * 0.6, 0.06), (r * 0.95, 0.052), (r, 0.045), (0.0, 0.045)], seg=16)
    if lit:
        p.cyl('Light', (x + 0.062, y, z), 'x', r * 0.5, 0.008, seg=12, bevel=0.0)
    if guard:
        g = r + 0.05
        for dz in (-0.35, 0.35):
            vp.tube(p, 'CnTankMetal', vp.bent_bar([(x - 0.02, y - g, z + dz * r), (x + 0.15, y - g, z + dz * r),
                                                    (x + 0.15, y + g, z + dz * r), (x - 0.02, y + g, z + dz * r)], R=0.05, n=3), 0.013, seg=6)
        vp.tube(p, 'CnTankMetal', [(x + 0.15, y, z - r - 0.02), (x + 0.15, y, z + r + 0.02)], 0.013, seg=6)


def antenna(p, x, y, z, h, base_r=0.05, m='Dark'):
    p.cyl(m, (x, y, z + 0.06), 'z', base_r, 0.12, seg=10, bevel=0.0)
    p.cyl('CnTankMetal', (x, y, z + 0.12 + 0.05), 'z', base_r * 0.6, 0.1, seg=8, bevel=0.0)
    p.cyl(m, (x, y, z + 0.2 + h / 2), 'z', 0.008, h, seg=6, bevel=0.0)


def tow_hook(p, x, y, z, s=1):
    """O gancho de reboque (31/10, o detalhe AAA): as duas orelhas soldadas, o pino e a manilha pendurada."""
    for d in (-1, 1):
        vp.rbox(p, 'CnTankMetal', (x + 0.05 * s, y + d * 0.055, z), (0.12, 0.035, 0.16), r=0.03, k=2)
    p.cyl('CnTankMetal#vp', (x + 0.07 * s, y, z + 0.03), 'y', 0.022, 0.16, seg=10, bevel=0.0)
    p.torus('Dark#vp', (x + 0.09 * s, y, z - 0.03), 'y', 0.055, 0.017, seg=14, rseg=6)


# ===================================================================================================== rodagem
def road_wheel(p, x, y, z, r, w, side, paint='CnTankPaint', seg=24):
    """A roda de apoio dupla (31/10, o detalhe AAA: model_vehicle_parts.road_wheel): a banda de borracha com o ombro redondo, o disco
    de aço REBAIXADO, o cubo com o anel de porcas, e o vão do meio onde o dente-guia da esteira passa."""
    vp.road_wheel(p, (x, y, z), r, w, side, paint, 'CnTankMetal', 'Rubber', gap=0.08, seg=seg)


def sprocket(p, x, y, z, r, w, side, teeth=13, paint='CnTankPaint'):
    """A roda motriz: as duas coroas DENTADAS, o tambor, o cubo com as porcas e os furos de alívio."""
    vp.sprocket(p, (x, y, z), r, w, side, paint, 'CnTankMetal', teeth=teeth, holes_m='Dark')


def idler(p, x, y, z, r, w, side, paint='CnTankPaint'):
    """A roda-guia: os dois discos de aço com a aba, o prato e o cubo com as porcas."""
    vp.idler(p, (x, y, z), r, w, side, paint, 'CnTankMetal')


def roller(p, x, y, z, r, w):
    vp.return_roller(p, (x, y, z), r, w, 'CnTankMetal', 'Rubber')


def track(name, circles, y, width, pitch=0.16, thick=0.05, guide=True, double_guide=False, bevel=0.006, sags=(), corner_k=2,
          grouser=0.022):
    """A esteira de elos COM FORMA (model_vehicle_parts.track_links): as 2 almofadas com a garra, o conector do pino nas bordas e
    o dente-guia no vão. A face de dentro fica ~0,75 'thick' pra dentro do caminho (a roda assenta), a de fora 0,3 'thick' pra fora."""
    return vp.track_links(name, circles, y, width, pitch=pitch, t_in=thick * 0.75, t_out=thick * 0.3, grouser=grouser,
                          guide=('double' if double_guide else 'single') if guide else None, m='Track', guide_m='CnTankMetal',
                          sags=sags, corner_k=corner_k)


# ===================================================================================================== conferência
def bvh(o, M=None):
    M = M or Matrix.Identity(4)
    vs = [M @ v.co for v in o.data.vertices]
    return BVHTree.FromPolygons(vs, [tuple(q.vertices) for q in o.data.polygons])


def sweep_check(hull_objs, turret_objs, pivot, step=5, label='torre'):
    """A torre girando não pode atravessar o casco: colisão de verdade (BVH, triângulo contra triângulo) a cada 'step' graus."""
    trees = [bvh(h) for h in hull_objs]
    piv = V(pivot[0], pivot[1], 0.0)
    hits = []
    for k in range(int(360 / step)):
        Y = Matrix.Translation(piv) @ Matrix.Rotation(math.radians(step * k), 4, 'Z') @ Matrix.Translation(-piv)
        n = 0
        where = []
        for t in turret_objs:
            tt = bvh(t, Y)
            for hi, ht in enumerate(trees):
                pairs = ht.overlap(tt)
                n += len(pairs)
                for (i, j) in pairs[:3]:
                    c = hull_objs[hi].data.polygons[i].center
                    where.append((round(c.x, 2), round(-c.y, 2), round(c.z, 2)))
        if n:
            hits.append((step * k, n, where[:3]))
    print('MODEL giro da %s: %s' % (label, 'sem colisao em 360 graus' if not hits else 'COLISAO em %d posicoes' % len(hits)))
    for h in hits[:12]:
        print('MODEL   ', h)
    return hits


def bounds(objs):
    xs, ys, zs = [], [], []
    for o in objs:
        for v in o.data.vertices:
            xs.append(v.co.x)
            ys.append(-v.co.y)
            zs.append(v.co.z)
    return (min(xs), max(xs)), (min(ys), max(ys)), (min(zs), max(zs))


def tris(o):
    return sum(len(q.vertices) - 2 for q in o.data.polygons)


def write_preview(out, parts, usa_at=(12.0, 6.0, 0.0), extra=None):
    """O preview.json do model_preview.py (as peças com a posição no Blender, as vistas e a câmera do jogo)."""
    spec = {
        'parts': [[n, [round(a, 4) for a in at]] for n, at in parts],
        'views': [['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1400, 800]],
                  ['back', 'ortho', [1, 0, 0], [0, 0, 1], [1400, 800]],
                  ['left', 'ortho', [0, -1, 0], [0, 0, 1], [1800, 820]],
                  ['top', 'ortho', [0, 0, -1], [0, -1, 0], [1800, 1100]],
                  ['three_quarter', 'persp', 30.0, 215.0, [1600, 1000], 32.0, 0.72],
                  ['three_quarter_right', 'persp', 32.0, 145.0, [1600, 1000], 32.0, 0.72],
                  ['back_three_quarter', 'persp', 30.0, 35.0, [1600, 1000], 32.0, 0.72]],
        'usa_at': list(usa_at), 'tank_views': True,
        'game': [['game_cam', 37.5, 35.0, 50.0, 0.8], ['game_cam_close', 37.5, 35.0, 28.0, 0.75]],
    }
    if extra:
        spec.update(extra)
    with open(os.path.join(out, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)
