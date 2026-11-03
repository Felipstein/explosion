"""O Palace da GLA, v2 (01/11/2026): refeito pela galeria (o Felipe sobre ele: "sai um pouco dessa de tudo ser murado assim"; o plano
em docs/elenco/GLA_PREDIOS.md, a ideia em docs/elenco/gla/gla_palace.md §2). O v1 (model_gla_palace.py) fica como estava.

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_gla_palace_v2.py C:/Users/luisf/explosion/captures/game/models/gla_palace_v2

A palavra (o que lê de cima): O PALÁCIO TOMADO, ABERTO. Sem muro: o palácio solto no terreno, sobre o pódio em CRUZ com os degraus nas
4 pontas. No meio a cúpula grande de azulejo turquesa gasta (o reboco aparece onde caiu o azulejo; a faixa verde do jogador na base;
o remate de bola de latão, sem crescente); nos 4 cantos as torrinhas quadradas, cada uma com a cupulinha turquesa (de cima, o quincunce:
o círculo grande e os 4 pequenos); nos 4 lados a arcada (o arco grande do meio e os 2 menores) com o terraço de ameias redondas. A GLA
por cima: os ninhos de sacos com a metralhadora no terraço da frente e no do lado, os estandartes verdes pendurados nos arcos, os
tapetes saqueados jogados no parapeito, os rabiscos verdes, e o carro de luxo roubado debaixo da lona no canto (+X, +Y).
A inspiração real: os pavilhões persas de cúpula central com os 4 cantos (o Hasht Behesht de Isfahan, o chahar-taq), sem minarete.

Lote 18 × 24 m, a frente em +X. Alturas: o pódio 1,05 m, a arcada 5,25 m (o terraço), o corpo 6,35 m, as torrinhas 7,45 m (as
cupulinhas até ~9,6 m), a cúpula até 11,1 m e o remate até ~12,3 m.

Peças (origem no chão, no centro do lote; Unreal X frente, Y direita, Z cima, metros):
  SM_GLA_Palace_Base: tudo (as bandeiras saem no flags.json);
  SM_GLA_Palace_Scaffold: o andaime da obra em madeira.
Materiais (slots): Plaster (o reboco e a pedra), Tile (o azulejo turquesa: só as cúpulas e os frisos), Player (o verde: a faixa da
  cúpula, os estandartes, as bandeiras, os rabiscos), CnGold (o latão do remate e das lanternas), Cloth, Rust (os tapetes), Wood,
  Dark (o escuro dos vãos e o carro preto), Glass, Metal, Trim, Light, Earth, Crate, Hazard (só no andaime).
"""
import math
import os
import sys

import bmesh
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, V, clear_scene, export, flag, scaffold  # noqa: E402,F401
from model_faction_kit import barrel, disc_ring, dome, lamp, revolve  # noqa: E402,F401
from model_building_parts import (BPart, WallFace, wbox, splits, tube, catenary, adobe_wall, merlons, lintel, tarp,  # noqa: E402,F401
                                  adobe_window, paint_band)
from model_building_parts import (gla_wall_seg as wall_seg, gla_net_surface as net_surface,  # noqa: E402,F401
                                  gla_round_merlons as round_merlons, gla_drift as drift)

# ===================================================================================== kit GLA v2 (01/11)
# (a mesma cópia nos 4 prédios v2 da GLA: model_gla_barracks_v2/palace_v2/stinger_site_v2/supply_stash_v2.py; o kit do
# elenco de 31/10 mais o que os 4 pedem: o chão batido que acompanha a forma, o pano de superfície livre, a corda com a
# barriga e a estaca, o tufo de trapo verde. O model_lib, o model_faction_kit e o model_building_parts não são editados)
COLORS.update({'Player': (86, 150, 62), 'Plaster': (205, 180, 135), 'Earth': (168, 142, 104), 'Rust': (118, 70, 40),
               'Wood': (120, 88, 52), 'Cloth': (150, 128, 92), 'Net': (96, 104, 62), 'Crate': (84, 86, 58),
               'Metal': (92, 90, 82), 'Dark': (40, 40, 38), 'Trim': (70, 68, 62), 'Light': (255, 220, 160),
               'Beacon': (255, 170, 40), 'Hazard': (210, 170, 30), 'Tile': (62, 138, 128), 'Rubber': (40, 36, 33),
               'CnGold': (190, 150, 70), 'Glass': (30, 34, 38)})
G = 0.0                          # (v2) sem laje: tudo nasce do chão do mapa (o chão batido é uma lente fina que acompanha a forma)
BEVELS = {'Plaster': 0.13, 'Plaster#lo': 0.012, 'Plaster#s': 0.0, 'Plaster#arch': 0.0, 'Earth': 0.06, 'Earth#drift': 0.0,
          'Earth#pad': 0.0, 'Player': 0.0, 'Net': 0.0, 'Cloth#net': 0.0, 'Cloth#tarp': 0.0, 'Cloth#bag': 0.0, 'Cloth': 0.015,
          'Dark': 0.0, 'Trim': 0.0, 'Rust': 0.015, 'Rust#sheet': 0.0, 'Rust#s': 0.0, 'Wood': 0.015, 'Crate': 0.03, 'Metal': 0.015,
          'Metal#s': 0.0, 'Light': 0.0, 'Beacon': 0.0, 'Hazard': 0.0, 'Tile': 0.0, 'Rubber': 0.0, 'CnGold': 0.0}


def out_dir(default):
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), default)
    os.makedirs(out, exist_ok=True)
    return out


def new_part(name):
    """A peça com o chanfro de cada material já escolhido (o primeiro uso de um material fixa o chanfro dele)."""
    p = BPart(name)
    for m, b in BEVELS.items():
        p.bm(m, b)
    return p


def finish(p):
    """Junta a peça (Part.finish) e mostra os triângulos por material (pra caber na conta do caderno)."""
    o = p.finish()
    bm = bmesh.new()
    bm.from_mesh(o.data)
    lim = 120.0
    bad = [v for v in bm.verts if v.co.length > lim]
    for v in bad:
        nb = [e.other_vert(v).co for e in v.link_edges if e.other_vert(v).co.length <= lim]
        if nb:
            v.co = sum(nb, Vector()) / len(nb)
    if bad:
        print('WARN', o.name, len(bad), 'vertices perdidos do chanfro, devolvidos aos vizinhos')
        bm.to_mesh(o.data)
    bm.free()
    cnt = {}
    for poly in o.data.polygons:
        k = o.data.materials[poly.material_index].name if o.data.materials else '?'
        cnt[k] = cnt.get(k, 0) + len(poly.vertices) - 2
    print('TRIS', o.name, sum(cnt.values()), sorted(cnt.items(), key=lambda kv: -kv[1]))
    return o


def _hash(i, j, s=0):
    n = (int(i) * 374761393 + int(j) * 668265263 + int(s) * 1442695041) & 0xFFFFFFFF
    n = ((n ^ (n >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((n ^ (n >> 16)) & 0xFFFF) / 65535.0


def vnoise(x, y, s=0):
    i, j = math.floor(x), math.floor(y)
    fx, fy = x - i, y - j
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a, b, c, d = _hash(i, j, s), _hash(i + 1, j, s), _hash(i, j + 1, s), _hash(i + 1, j + 1, s)
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def fbm(x, y, s=0):
    return vnoise(x, y, s) * 0.65 + vnoise(x * 2.13 + 5.3, y * 2.13 + 1.7, s + 1) * 0.35


def smooth(a, b, x):
    t = min(1.0, max(0.0, (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


def rot3(yaw=0.0, pitch=0.0, roll=0.0):
    """O giro do obox (Unreal: yaw +X→+Y, pitch nariz pra cima, roll), como matriz no espaço do Blender."""
    return (Matrix.Rotation(math.radians(-yaw), 3, 'Z') @ Matrix.Rotation(math.radians(-pitch), 3, 'Y')
            @ Matrix.Rotation(math.radians(roll), 3, 'X'))


def mbox(p, m, c, size, R, bevel=None):
    """Caixa com uma matriz de giro qualquer (Blender), centro c (Unreal)."""
    b = p.bm(m, bevel if bevel is not None else 0.02)
    r = bmesh.ops.create_cube(b, size=1.0)
    M = Matrix.Translation(V(*c)) @ R.to_4x4() @ Matrix.Diagonal((size[0], size[1], size[2], 1.0))
    bmesh.ops.transform(b, matrix=M, verts=r['verts'])


class Frame:
    """Um referencial local (origem, yaw, pitch, roll) pra montar um conjunto girado/inclinado (o tripé, o carro)."""

    def __init__(self, p, origin, yaw=0.0, pitch=0.0, roll=0.0):
        self.p, self.R, self.o = p, rot3(yaw, pitch, roll), V(*origin)

    def pt(self, x, y, z):
        w = self.o + self.R @ V(x, y, z)
        return (w.x, -w.y, w.z)

    def vec(self, d):
        w = self.R @ V(*d)
        return (w.x, -w.y, w.z)

    def box(self, m, c, size, yaw=0.0, pitch=0.0, roll=0.0, bevel=None):
        mbox(self.p, m, self.pt(*c), size, self.R @ rot3(yaw, pitch, roll), bevel)

    def cyl(self, m, c, axis, r, length, **kw):
        a = {'x': (1, 0, 0), 'y': (0, 1, 0), 'z': (0, 0, 1)}.get(axis, axis)
        self.p.cyl(m, self.pt(*c), self.vec(a), r, length, **kw)

    def rod(self, m, a, b, r, seg=6):
        rod(self.p, m, self.pt(*a), self.pt(*b), r, seg)


def rod(p, m, a, b, r, seg=6):
    """Um tubo (ou cabo) de a até b (numa malha própria do material, sem chanfro: o tubo fino não precisa)."""
    d = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
    L = math.sqrt(d[0] ** 2 + d[1] ** 2 + d[2] ** 2)
    if L > 1e-4:
        key = m if '#' in m else m + '#r'
        p.cyl(key, ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2), d, r, L, seg=seg, bevel=0.0)


def prism(p, m, pts, d):
    """Prisma de um polígono plano convexo [(x, y, z)...] extrudado pelo vetor d."""
    b = p.bm(m, BEVELS.get(m, 0.02))
    a = [b.verts.new(V(*q)) for q in pts]
    c = [b.verts.new(V(q[0] + d[0], q[1] + d[1], q[2] + d[2])) for q in pts]
    b.faces.new(a)
    b.faces.new(list(reversed(c)))
    n = len(pts)
    for i in range(n):
        j = (i + 1) % n
        b.faces.new([a[i], a[j], c[j], c[i]])


def bag(p, c, size, yaw=0.0, m='Cloth#bag'):
    """O saco de areia estufado: a seção superelipse, cheio no meio, as pontas amarradas, a barriga achatada no apoio."""
    L, W, H = size
    hsh = (math.sin(c[0] * 12.9898 + c[1] * 78.233 + c[2] * 37.719) * 43758.5453) % 1.0
    L *= 0.94 + 0.12 * hsh
    H *= 0.9 + 0.2 * ((hsh * 7.31) % 1.0)
    cy, sy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    rings = []
    n, k_ = 5, 8
    for i in range(n + 1):
        u = i / n
        bulge = math.sin(math.pi * u) ** 0.45
        w = W / 2 * (0.45 + 0.55 * bulge)
        t = H / 2 * (0.3 + 0.7 * math.sin(math.pi * u) ** 0.7)
        x = L * (u - 0.5)
        ring = []
        for k in range(k_):
            th = 2 * math.pi * k / k_
            ct, st = math.cos(th), math.sin(th)
            yy = w * math.copysign(abs(ct) ** 0.7, ct)
            zz = t * math.copysign(abs(st) ** 0.7, st)
            if zz < 0:
                zz *= 0.6
            ring.append((c[0] + x * cy - yy * sy, c[1] + x * sy + yy * cy, c[2] + zz + 0.1 * H * (1 - bulge)))
        rings.append(ring)
    p.loft(m, rings, bevel=0.0, smooth=True)


def bag_row(p, pts, z0, layers=3, bag_size=(0.66, 0.36, 0.24), seed=0, m='Cloth#bag'):
    """A mureta de sacos de areia ao longo da polilinha, desencontrados por fileira."""
    L_, W_, H_ = bag_size
    for (ax, ay), (bx, by) in zip(pts[:-1], pts[1:]):
        L = math.hypot(bx - ax, by - ay)
        yaw = math.degrees(math.atan2(by - ay, bx - ax))
        n = max(1, int(round(L / (L_ * 0.92))))
        for lv in range(layers):
            off = 0.5 if lv % 2 else 0.0
            for k in range(n):
                t = (k + 0.5 + off * 0.5) / n
                if t > 1.0:
                    continue
                x, y = ax + (bx - ax) * t, ay + (by - ay) * t
                bag(p, (x, y, z0 + H_ * 0.42 + lv * H_ * 0.82), (L_, W_, H_), yaw=yaw + (_hash(k, lv, seed) - 0.5) * 12, m=m)


def bag_ring(p, c, r, z0, layers=3, gaps=(), bag_size=(0.66, 0.36, 0.24), seed=0, lean=0.03):
    """O ninho redondo de sacos de areia; gaps = [(de, até) em graus] pras passagens; cada fileira um pouco pra dentro."""
    L_, W_, H_ = bag_size
    n = max(8, int(2 * math.pi * r / (L_ * 0.9)))
    for lv in range(layers):
        rr = r - lv * lean
        for k in range(n):
            a = 2 * math.pi * (k + (0.5 if lv % 2 else 0.0)) / n
            deg = math.degrees(a) % 360
            if any((g0 <= deg <= g1) or (g0 <= deg + 360 <= g1) or (g0 <= deg - 360 <= g1) for (g0, g1) in gaps):
                continue
            bag(p, (c[0] + rr * math.cos(a), c[1] + rr * math.sin(a), z0 + H_ * 0.42 + lv * H_ * 0.82), (L_, W_, H_),
                yaw=math.degrees(a) + 90 + (_hash(k, lv, seed) - 0.5) * 10)


def tire(p, c, R=0.36, r=0.13, axis='z', m='Rubber'):
    p.torus(m, c, axis, R, r, seg=18, rseg=8)


def flagpole(p, x, y, z0, h, length=1.3, height=0.85, phase=0.0, m_pole='Trim'):
    """O mastro com a bandeira verde (cor do jogador) no topo, o pano pra trás (−X): o executor do detalhe tira o pano pro
    flags.json e o jogo desenha ele no vento."""
    p.cyl(m_pole, (x, y, z0 + h / 2), 'z', 0.045, h, seg=8, bevel=0.0)
    p.cyl(m_pole, (x, y, z0 + h + 0.05), 'z', 0.07, 0.1, seg=8, bevel=0.0)
    flag(p, (x, y), z0 + h - 0.05, length, height, m='Player', phase=phase)


def surface(p, m, fn, nu, nv, thick=0.04, smooth_=True):
    """O pano de forma livre: a grade fn(u, v) → (x, y, z) com espessura (uma camada em cima, outra embaixo, as bordas
    fechadas: a malha é fechada e as normais saem certas)."""
    b = p.bm(m, 0.0)
    P = [[fn(i / nu, j / nv) for i in range(nu + 1)] for j in range(nv + 1)]
    top = [[b.verts.new(V(x, y, z + thick / 2)) for (x, y, z) in row] for row in P]
    bot = [[b.verts.new(V(x, y, z - thick / 2)) for (x, y, z) in row] for row in P]
    for j in range(nv):
        for i in range(nu):
            b.faces.new([top[j][i], top[j][i + 1], top[j + 1][i + 1], top[j + 1][i]]).smooth = smooth_
            b.faces.new([bot[j + 1][i], bot[j + 1][i + 1], bot[j][i + 1], bot[j][i]]).smooth = smooth_
    edge = [(0, i) for i in range(nu)] + [(j, nu) for j in range(nv)] + [(nv, i) for i in range(nu, 0, -1)] + [(j, 0) for j in range(nv, 0, -1)]
    for (j0, i0), (j1, i1) in zip(edge, edge[1:] + edge[:1]):
        b.faces.new([top[j0][i0], bot[j0][i0], bot[j1][i1], top[j1][i1]])


def ribbon(p, m, pts, w, t=0.03, up=(0.0, 0.0, 1.0), upf=None):
    """A fita (a cinta de tensão, a faixa tecida): a seção retangular w × t varrida pela polilinha, deitada no plano do 'up' (ou da
    normal upf(ponto) da superfície onde ela deita)."""
    P = [Vector(q) for q in pts]
    rings = []
    for i in range(len(P)):
        U = Vector(upf(tuple(P[i]))) if upf else Vector(up)
        tg = (P[min(i + 1, len(P) - 1)] - P[max(i - 1, 0)]).normalized()
        h = U - tg * U.dot(tg)
        h.normalize()
        s = tg.cross(h).normalized()
        q = P[i]
        rings.append([tuple(q + s * a + h * c) for (a, c) in ((-w / 2, -t / 2), (w / 2, -t / 2), (w / 2, t / 2), (-w / 2, t / 2))])
    p.loft(m, rings, bevel=0.0)


def earth_pad(p, c, rfn, h=0.07, m='Earth#pad', step=0.38, arc=0.42, z0=-0.04):
    """O chão batido que ACOMPANHA A FORMA (a regra da galeria: nada de laje quadrada): a lente baixa de terra com o contorno
    r(θ) (rfn(θ) → raio a partir de c), alta no miolo (h) e morrendo no chão na borda (enterrada em z0). Só o tampo tem
    grade (passo 'step' × 'arc', abaixo da célula do detalhe: a grade não corta de novo); o fundo é um polígono só."""
    rmax = max(rfn(2 * math.pi * k / 72) for k in range(72))
    N = max(24, int(2 * math.pi * rmax / arc))
    R = max(3, int(rmax / step))
    b = p.bm(m, 0.0)
    th = [2 * math.pi * k / N for k in range(N)]
    rr = [rfn(a) for a in th]
    rings = []
    for j in range(1, R + 1):
        t = j / R
        zz = z0 + (h - z0) * (1.0 - smooth(0.72, 1.0, t)) + 0.012 * (fbm(t * 9.0, 3.1) - 0.5) * (1 - t)
        rings.append([b.verts.new(V(c[0] + rr[k] * t * math.cos(th[k]), c[1] + rr[k] * t * math.sin(th[k]),
                                    z0 if j == R else zz)) for k in range(N)])
    cv = b.verts.new(V(c[0], c[1], h))
    for k in range(N):
        b.faces.new([cv, rings[0][k], rings[0][(k + 1) % N]])
    for j in range(R - 1):
        for k in range(N):
            k2 = (k + 1) % N
            b.faces.new([rings[j][k], rings[j + 1][k], rings[j + 1][k2], rings[j][k2]])
    b.faces.new(list(reversed(rings[-1])))


def blob(cx, cy, rx, ry, seed=0, amp=0.12, freq=3.0):
    """O contorno orgânico (a elipse com o ruído): devolve rfn(θ) pro earth_pad."""
    def rfn(a):
        e = 1.0 / math.sqrt((math.cos(a) / rx) ** 2 + (math.sin(a) / ry) ** 2)
        n = fbm(math.cos(a) * freq + 7.1, math.sin(a) * freq + 3.3, seed) - 0.5
        return e * (1.0 + amp * 2 * n)
    return rfn


def stake(p, x, y, toward=(0.0, 0.0), h=0.45, m='Wood#stake'):
    """A estaca de madeira cravada inclinada pra fora (contra a corda) com a pedra no pé."""
    dx, dy = x - toward[0], y - toward[1]
    L = math.hypot(dx, dy) or 1.0
    dx, dy = dx / L, dy / L
    top = (x + dx * 0.12, y + dy * 0.12, h)
    rod(p, m, (x - dx * 0.04, y - dy * 0.04, -0.08), top, 0.035, seg=6)
    p.obox('Plaster#stone', (x - dx * 0.18, y - dy * 0.18, 0.06), (0.24, 0.2, 0.14), yaw=math.degrees(math.atan2(dy, dx)) + 20, bevel=0.03)
    return top


def rope(p, a, b, sag=0.08, r=0.024, m='Wood#rope', n=8):
    """A corda com a barriga (6 lados: o fio fino não precisa de mais)."""
    tube(p, m, catenary(a, b, sag, n), r, seg=6, smooth=True)


def rag(p, c, yaw=0.0, L=0.45, w=0.16, m='Player'):
    """O trapo verde amarrado (a cor do jogador): a tira de pano pendurada, um pouco torta."""
    p.obox(m, (c[0], c[1], c[2] - L / 2), (0.02, w, L), yaw=yaw, roll=(_hash(int(c[0] * 10), int(c[1] * 10), 3) - 0.5) * 20, bevel=0.0)


def crate(p, x, y, z, size=(1.0, 0.6, 0.5), yaw=0.0, m='Crate', strap='Dark'):
    """O caixote com o chanfro, as 2 cintas e as ripas das pontas em relevo."""
    c = (x, y, z + size[2] / 2)
    p.obox(m, c, size, yaw=yaw, bevel=0.03)
    a = math.radians(yaw)
    for s in (-1, 1):
        o = s * size[0] * 0.3
        p.obox(strap, (c[0] + o * math.cos(a), c[1] + o * math.sin(a), c[2]), (0.05, size[1] + 0.02, size[2] + 0.02), yaw=yaw, bevel=0.0)
    for s in (-1, 1):
        o = s * (size[0] / 2 + 0.01)
        p.obox(m + '#batten', (c[0] + o * math.cos(a), c[1] + o * math.sin(a), c[2]), (0.025, size[1] * 0.9, size[2] * 0.88), yaw=yaw, bevel=0.01)
# ===================================================================================== fim do kit GLA v2

PREFIX = 'SM_GLA_Palace'
HX, HY = 9.0, 12.0
PZ = 1.05                       # o topo do pódio
ARM = 3.3                       # a meia-largura dos braços da cruz
PX_END, PY_END = 7.95, 10.6     # as pontas dos braços (depois os degraus)
STEP = 0.35
C = 5.0                         # o corpo (o quadrado do meio)
ROOF = PZ + 5.3
PV0, PV1 = 2.9, 5.6             # as torrinhas dos cantos (|x| e |y| entre PV0 e PV1)
PV_TOP = PZ + 6.4
PORCH = {'+x': 7.35, '-x': 7.35, '+y': 8.1, '-y': 8.1}   # a face da arcada de cada lado
PORCH_H = PZ + 3.9              # o forro da arcada (a laje em cima até +0,3)
PT = 0.5                        # a espessura das paredes
DRUM_R, DRUM_H = 3.45, 1.15
DOME_R, DOME_H = 3.3, 3.55
PAV_DOME_R, PAV_DOME_H = 0.95, 1.25


# ===================================================================================== o arco (do kit do elenco)
def _plane(c, face_axis, s, z, n):
    """Um ponto no plano de uma parede: s ao longo da parede, z, n na normal (face_axis 'x': a parede olha pra ±X)."""
    return (c[0] + n, c[1] + s, z) if face_axis == 'x' else (c[0] + s, c[1] + n, z)


def arch_fill(p, m, c, face_axis, w, spring, top, depth, seg=16):
    """O enchimento sobre um vão em arco redondo (os tímpanos e o intradorso), sem booleano: o vão de largura w centrado em c=(x, y),
    a imposta em z=spring, a parede até z=top, a espessura depth na normal."""
    r = w / 2
    b = p.bm(m, BEVELS.get(m, 0.0))
    arc = [(-r * math.cos(math.pi * k / seg), spring + r * math.sin(math.pi * k / seg)) for k in range(seg + 1)]
    half = seg // 2
    sides = []
    for n in (-depth / 2, depth / 2):
        A = [b.verts.new(V(*_plane(c, face_axis, s, z, n))) for (s, z) in arc]
        TL = b.verts.new(V(*_plane(c, face_axis, -r, top, n)))
        TR = b.verts.new(V(*_plane(c, face_axis, r, top, n)))
        sides.append((A, TL, TR))
        for k in range(half):
            b.faces.new([TL, A[k], A[k + 1]])
        for k in range(half, seg):
            b.faces.new([TR, A[k], A[k + 1]])
        b.faces.new([TL, A[half], TR])
    (A0, TL0, TR0), (A1, TL1, TR1) = sides
    for k in range(seg):
        b.faces.new([A0[k], A0[k + 1], A1[k + 1], A1[k]])
    b.faces.new([TL0, TR0, TR1, TL1])
    b.faces.new([TL0, TL1, A1[0], A0[0]])
    b.faces.new([TR0, A0[seg], A1[seg], TR1])


def arch_band(p, m, c, face_axis, w, spring, out, width=0.28, t=0.025, seg=16):
    """A moldura em volta de um arco, colada na face (out = o deslocamento na normal até a face, com o sinal do lado de fora)."""
    r0, r1 = w / 2 + 0.04, w / 2 + 0.04 + width
    b = p.bm(m, 0.0)
    s = 1 if out >= 0 else -1
    for k in range(seg):
        a0, a1 = math.pi * k / seg, math.pi * (k + 1) / seg
        quad = [(-r0 * math.cos(a0), spring + r0 * math.sin(a0)), (-r1 * math.cos(a0), spring + r1 * math.sin(a0)),
                (-r1 * math.cos(a1), spring + r1 * math.sin(a1)), (-r0 * math.cos(a1), spring + r0 * math.sin(a1))]
        f = [b.verts.new(V(*_plane(c, face_axis, q[0], q[1], out))) for q in quad]
        g = [b.verts.new(V(*_plane(c, face_axis, q[0], q[1], out + s * t))) for q in quad]
        b.faces.new(f)
        b.faces.new(list(reversed(g)))
        for i in range(4):
            j = (i + 1) % 4
            b.faces.new([f[i], f[j], g[j], g[i]])


def tile_layer(p, fn, nu, nv, seed=1, blotch=1.5, holes=0.3, thick=0.02, closed_u=False, m='Tile'):
    """A camada de azulejo que falta aos pedaços (o reboco aparece nos buracos): o campo de ruído decide onde há azulejo; o contorno
    pelo marching squares (as manchas redondas). Sem os tufos da rede."""
    P = [[fn(i / nu, j / nv) for i in range(nu + 1)] for j in range(nv + 1)]
    F = []
    for j in range(nv + 1):
        row = []
        for i in range(nu + 1):
            n = fbm(P[j][i][0] / blotch + 0.37, P[j][i][1] / blotch + 0.71 + P[j][i][2] / blotch * 0.6, seed)
            row.append(n)
        F.append(row)
    b = p.bm(m, 0.0)
    for j in range(nv):
        for i in range(nu):
            cs = [(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)]
            poly = []
            for k in range(4):
                (ia, ja), (ib, jb) = cs[k], cs[(k + 1) % 4]
                fa, fb = F[ja][ia], F[jb][ib]
                if fa >= holes:
                    poly.append(P[ja][ia])
                if (fa >= holes) != (fb >= holes):
                    s_ = (holes - fa) / (fb - fa)
                    pa, pb = P[ja][ia], P[jb][ib]
                    poly.append((pa[0] + (pb[0] - pa[0]) * s_, pa[1] + (pb[1] - pa[1]) * s_, pa[2] + (pb[2] - pa[2]) * s_))
            if len(poly) < 3:
                continue
            b.faces.new([b.verts.new(V(*q)) for q in poly]).smooth = True


def pdome(r, h, t):
    """O perfil da cúpula persa (a leve ponta em cima): o raio e a altura no parâmetro t (0 na base, 1 no topo)."""
    a = t * math.pi / 2
    rr = r * math.cos(a) ** 1.12 * (1.0 + 0.05 * math.sin(math.pi * t))
    return rr, h * math.sin(a) ** 0.92


def tile_dome(p, c, r, h, z0, seg=56, rings=16, ribs=16, band=True, seed=17, holes=0.3):
    """A cúpula: o casco de reboco, a camada de azulejo turquesa que falta aos pedaços, as nervuras de reboco (os gomos), a faixa
    verde do jogador na base, o remate (o pescoço de reboco, a bola e a ponta de latão)."""
    cx, cy = c
    prof = [pdome(r, h, j / rings) for j in range(rings + 1)]
    revolve(p, 'Plaster#s', c, [(max(rr, 0.02), z0 + zz) for (rr, zz) in prof], seg=seg)

    def fn(u, v):
        a = 2 * math.pi * u
        rr, zz = pdome(r + 0.035, h + 0.035, 0.03 + 0.9 * v)
        return (cx + rr * math.cos(a), cy + rr * math.sin(a), z0 + zz)
    tile_layer(p, fn, int(seg * 1.4), int(rings * 1.15), seed=seed, blotch=0.55 * r / 3.3 + 0.12, holes=holes, closed_u=True)
    for k in range(ribs):
        a = 2 * math.pi * k / ribs
        rr_ = []
        for j in range(9):
            t = 0.08 + 0.82 * j / 8
            rr, zz = pdome(r + 0.05, h + 0.05, t)
            w = 0.07 * (r / 3.3) * (1.0 - 0.6 * j / 8) + 0.015
            tx, ty = -math.sin(a), math.cos(a)
            px, py = cx + rr * math.cos(a), cy + rr * math.sin(a)
            o = 0.045
            rr_.append([(px - tx * w, py - ty * w, z0 + zz), (px + tx * w, py + ty * w, z0 + zz),
                        (px + tx * w + math.cos(a) * o, py + ty * w + math.sin(a) * o, z0 + zz + o * 0.6),
                        (px - tx * w + math.cos(a) * o, py - ty * w + math.sin(a) * o, z0 + zz + o * 0.6)])
        p.loft('Plaster#rib', rr_, bevel=0.0)
    if band:
        disc_ring(p, 'Player', c, r + 0.06, r - 0.2, z0 - 0.02, z0 + 0.5 * r / 3.3, seg=seg)
        disc_ring(p, 'Plaster', c, r + 0.1, r - 0.2, z0 + 0.5 * r / 3.3, z0 + 0.65 * r / 3.3, seg=seg)
    k = r / 3.3
    zt = z0 + h
    revolve(p, 'Plaster#s', c, [(0.72 * k, zt - 0.12 * k), (0.48 * k, zt + 0.12 * k), (0.3 * k, zt + 0.3 * k), (0.02, zt + 0.42 * k)], seg=20)
    revolve(p, 'CnGold', c, [(0.02, zt + 0.38 * k), (0.22 * k, zt + 0.46 * k), (0.3 * k, zt + 0.6 * k), (0.22 * k, zt + 0.76 * k),
                             (0.06 * k, zt + 0.84 * k), (0.04 * k, zt + 1.12 * k), (0.01, zt + 1.18 * k)], seg=16)


def arch_window(p, x, y, z, face, w=1.0, h=1.6, t=PT, m='Dark'):
    """A janela alta em arco no vão fundo da parede (os tímpanos na espessura, o escuro recuado com o caixilho em cruz, o peitoril
    gasto, a moldura de reboco em volta do arco). O vão tem de estar aberto na parede (o furo do adobe_block)."""
    s = 1 if face[0] == '+' else -1
    F = WallFace(face, x if face[1] == 'x' else y, 0.0)
    u = y if face[1] == 'x' else x
    spring = z + h - w / 2
    c = (x - s * t / 2, y) if face[1] == 'x' else (x, y - s * t / 2)
    arch_fill(p, 'Plaster#arch', c, face[1], w, spring, z + h, t)
    d = t * 0.6
    wbox(p, m, F, u - w / 2, u + w / 2, z, spring, -d - 0.03, -d, bevel=0.0)
    p.cyl(m, F.pt(u, spring, -d - 0.015), face[1], w / 2, 0.03, seg=16, bevel=0.0)
    wbox(p, 'Wood#win', F, u - 0.03, u + 0.03, z, spring + w * 0.45, -d, -d + 0.04, bevel=0.0)
    wbox(p, 'Wood#win', F, u - w / 2, u + w / 2, spring - 0.03, spring + 0.03, -d, -d + 0.04, bevel=0.0)
    wbox(p, 'Plaster#sill', F, u - w / 2 - 0.1, u + w / 2 + 0.1, z - 0.12, z + 0.01, -d + 0.04, 0.1, bevel=0.03)
    arch_band(p, 'Plaster#s', (x, y), face[1], w, spring, s * 0.005, width=0.12, t=0.06, seg=12)


def carpet(p, top_c, z_top, face, w=1.6, h=2.0):
    """O tapete saqueado jogado no parapeito: o pano com a barra verde, o campo vermelho, o medalhão claro e as franjas, colado na face."""
    x, y = top_c
    s = 1 if face[0] == '+' else -1
    lay = [('Cloth#rug', w, h, 0.0), ('Player#rug', w - 0.16, h - 0.16, 0.012), ('Rust#rug', w - 0.36, h - 0.36, 0.024)]
    zc = z_top - h / 2
    for (m, ww, hh, d) in lay:
        if face[1] == 'x':
            p.box(m, min(x + s * d, x + s * (d + 0.02)), max(x + s * d, x + s * (d + 0.02)), y - ww / 2, y + ww / 2, zc - hh / 2, zc + hh / 2, bevel=0.0)
        else:
            p.box(m, x - ww / 2, x + ww / 2, min(y + s * d, y + s * (d + 0.02)), max(y + s * d, y + s * (d + 0.02)), zc - hh / 2, zc + hh / 2, bevel=0.0)
    pts = [(0.0, 0.42), (0.28, 0.0), (0.0, -0.42), (-0.28, 0.0)]
    d = 0.036
    if face[1] == 'x':
        prism(p, 'Cloth#rug', [(x + s * d, y + a * w / 1.6, zc + b * h / 2.0) for (a, b) in pts], (s * 0.01, 0.0, 0.0))
    else:
        prism(p, 'Cloth#rug', [(x + a * w / 1.6, y + s * d, zc + b * h / 2.0) for (a, b) in pts], (0.0, s * 0.01, 0.0))
    for k in range(7):
        u = -w / 2 + 0.1 + k * (w - 0.2) / 6
        if face[1] == 'x':
            p.box('Cloth#rug', min(x, x + s * 0.02), max(x, x + s * 0.02), y + u - 0.02, y + u + 0.02, z_top - h - 0.12, z_top - h, bevel=0.0)
        else:
            p.box('Cloth#rug', x + u - 0.02, x + u + 0.02, min(y, y + s * 0.02), max(y, y + s * 0.02), z_top - h - 0.12, z_top - h, bevel=0.0)


def banner(p, F, u, z_top, w=0.78, L=1.85, n_in=-0.3):
    """O estandarte verde pendurado no arco (a cor do jogador): o pano com a barriga leve, a barra de pau em cima, o disco cru do
    emblema, a ponta em V embaixo."""
    def fn(a, b):
        uu = u - w / 2 + w * a
        z = z_top - L * b
        n = n_in + 0.05 * math.sin(math.pi * a) * b + 0.03 * math.sin(b * 5.0)
        if b > 0.86:
            z += (0.86 - b) * 0.0 + abs(a - 0.5) * 0.0
        return F.pt(uu, z, n)
    surface(p, 'Player#ban', fn, 4, 10, thick=0.025)
    q0, q1 = F.pt(u - w / 2 - 0.08, z_top + 0.03, n_in), F.pt(u + w / 2 + 0.08, z_top + 0.03, n_in)
    rod(p, 'Wood#ban', q0, q1, 0.025, seg=6)
    c = F.pt(u, z_top - L * 0.42, n_in + 0.03 * F.s)
    p.cyl('Cloth#emb', c, F.ax, 0.2, 0.02, seg=16, bevel=0.0)
    # a ponta em V (o rabo do estandarte)
    zb = z_top - L
    prism(p, 'Player#ban', [F.pt(u - w / 2, zb + 0.01, n_in), F.pt(u + w / 2, zb + 0.01, n_in), F.pt(u, zb - 0.28, n_in)],
          (0.02 * F.s, 0.0, 0.0) if F.ax == 'x' else (0.0, 0.02 * F.s, 0.0))


def dshk(p, x, y, z, yaw=0.0):
    """A metralhadora pesada (a DShK) no tripé, com a caixa de munição."""
    f = Frame(p, (x, y, z), yaw=yaw)
    for k in range(3):
        a = math.radians(180 + (k - 1) * 70)
        f.rod('Dark', (0.0, 0.0, 0.78), (0.75 * math.cos(a), 0.75 * math.sin(a), 0.0), 0.03)
    f.cyl('Dark', (0.0, 0.0, 0.82), 'z', 0.06, 0.12, seg=8)
    f.box('Dark', (0.05, 0.0, 0.95), (0.62, 0.17, 0.2))
    f.cyl('Metal', (0.62, 0.0, 0.97), 'x', 0.065, 0.5, seg=10)
    f.cyl('Dark', (1.1, 0.0, 0.97), 'x', 0.035, 0.9, seg=8)
    f.cyl('Dark', (1.55, 0.0, 0.97), 'x', 0.06, 0.08, seg=8)
    f.box('Dark', (-0.3, 0.0, 0.98), (0.16, 0.06, 0.3))
    f.box('Crate', (0.05, 0.26, 0.9), (0.32, 0.16, 0.2))


def scrawl(p, c, face, size=1.8, seed=0, m='Player'):
    """O rabisco verde pintado à mão na parede (a cor do jogador): 4 pinceladas afinadas nas pontas, coladas na face."""
    x, y, z = c
    sgn = 1 if face[0] == '+' else -1
    for k in range(4):
        ang = math.radians(k * 47 + 18 + (_hash(k, seed, 61) - 0.5) * 30)
        L = size * (0.55 + 0.45 * _hash(k, seed, 63))
        w = 0.13 + 0.09 * _hash(k, seed, 65)
        ox = (_hash(k, seed, 67) - 0.5) * size * 0.25
        oz = (_hash(k, seed, 69) - 0.5) * size * 0.2
        ca, sa = math.cos(ang), math.sin(ang)
        pts2 = [(-L / 2, 0.0), (-L / 2 + 0.25 * L, -w), (L / 2 - 0.15 * L, -w * 0.8), (L / 2, 0.0), (L / 2 - 0.2 * L, w * 0.9), (-L / 2 + 0.3 * L, w)]
        d0 = 0.012 + 0.006 * k
        pts = []
        for (a_, b_) in pts2:
            s_, t_ = ox + a_ * ca - b_ * sa, oz + a_ * sa + b_ * ca
            pts.append((x + sgn * d0, y + s_, z + t_) if face[1] == 'x' else (x + s_, y + sgn * d0, z + t_))
        prism(p, m, pts, (sgn * 0.005, 0.0, 0.0) if face[1] == 'x' else (0.0, sgn * 0.005, 0.0))


# ===================================================================================== o pódio em cruz
CROSS = [(PX_END, -ARM), (PX_END, ARM), (ARM, ARM), (ARM, PY_END), (-ARM, PY_END), (-ARM, ARM), (-PX_END, ARM), (-PX_END, -ARM),
         (-ARM, -ARM), (-ARM, -PY_END), (ARM, -PY_END), (ARM, -ARM)]


def podium(p):
    """O pódio em cruz (sem muro): os 3 blocos de pedra rebocada, o rodapé e a cornija acompanhando o contorno da cruz, os degraus
    nas 4 pontas com as muretas, a areia acumulada no pé, os rabiscos verdes."""
    from model_building_parts import open_box
    open_box(p, 'Plaster#pod', -PX_END, PX_END, -ARM, ARM, -0.3, PZ, bevel=0.06)
    for s in (-1, 1):
        y0, y1 = sorted((s * (ARM - 0.002), s * PY_END))
        open_box(p, 'Plaster#pod', -ARM, ARM, y0, y1, -0.3, PZ, bevel=0.06)
    n = len(CROSS)
    for i in range(n):
        (ax, ay), (bx, by) = CROSS[i], CROSS[(i + 1) % n]
        L = math.hypot(bx - ax, by - ay)
        dx, dy = (bx - ax) / L, (by - ay) / L
        nx, ny = dy, -dx                                         # a normal pra fora (o contorno vai no sentido anti-horário)
        yaw = math.degrees(math.atan2(dy, dx))
        cx, cy = (ax + bx) / 2, (ay + by) / 2
        for (z0, z1, out) in ((0.0, 0.28, 0.1), (PZ - 0.16, PZ + 0.02, 0.12)):
            p.obox('Plaster#trim', (cx + nx * out / 2, cy + ny * out / 2, (z0 + z1) / 2), (L + out * 2, out + 0.02, z1 - z0), yaw=yaw, bevel=0.03)
        # o friso de azulejo fino logo abaixo da cornija (o persa) e os nichos cegos rasos no meio da face
        p.obox('Tile#pod', (cx + nx * 0.012, cy + ny * 0.012, PZ - 0.26), (L - 0.1, 0.03, 0.08), yaw=yaw, bevel=0.0)
        is_end = (abs(ax) == abs(bx) == PX_END) or (abs(ay) == abs(by) == PY_END)
        if L > 4.0 and not is_end:
            for u in splits(-L / 2 + 0.9, L / 2 - 0.9, 1.25):
                q = (cx + dx * u + nx * 0.01, cy + dy * u + ny * 0.01)
                face = ('+x' if nx > 0.5 else '-x') if abs(nx) > 0.5 else ('+y' if ny > 0.5 else '-y')
                arch_band(p, 'Plaster#niche', q, face[1], 0.5, 0.48, (1 if face[0] == '+' else -1) * 0.002, width=0.08, t=0.04, seg=10)
        if L > 4.0 and not is_end:
            drift(p, (ax + nx * 0.12, ay + ny * 0.12), (bx + nx * 0.12, by + ny * 0.12), (nx, ny), h=0.3, w=0.55, seed=i, z=0.0)
    # os degraus nas 4 pontas (2 degraus de 0,35 e o chão), com as muretas dos lados
    RUN = 0.42
    for (axis, s, end, hw) in (('x', 1, PX_END, 2.3), ('x', -1, PX_END, 2.3), ('y', 1, PY_END, 2.3), ('y', -1, PY_END, 2.3)):
        for k in range(2):
            lo, hi = sorted((s * (end - 0.02 + k * RUN), s * (end + (k + 1) * RUN)))
            zt = PZ - STEP * (k + 1)
            if axis == 'x':
                p.box('Plaster#step%d' % k, lo, hi, -hw, hw, -0.1, zt, bevel=0.04)
            else:
                p.box('Plaster#step%d' % k, -hw, hw, lo, hi, -0.1, zt, bevel=0.04)
        for t in (-1, 1):
            lo, hi = sorted((s * (end - 0.02), s * (end + 2 * RUN + 0.12)))
            if axis == 'x':
                y0, y1 = sorted((t * hw, t * (hw + 0.45)))
                p.box('Plaster#cheek', lo, hi, y0, y1, -0.1, PZ + 0.3, bevel=0.1)
            else:
                x0, x1 = sorted((t * hw, t * (hw + 0.45)))
                p.box('Plaster#cheek', x0, x1, lo, hi, -0.1, PZ + 0.3, bevel=0.1)
    scrawl(p, (PX_END + 0.1, -2.9 - 0.0, 0.62), '+x', size=0.9, seed=4)
    scrawl(p, (ARM + 0.1, -8.6, 0.6), '+x', size=1.0, seed=5)
    scrawl(p, (-1.6, -PY_END - 0.1, 0.62), '-y', size=0.8, seed=6)


# ===================================================================================== o corpo, as torrinhas e a cúpula
def core(p):
    """O corpo: as 4 paredes (oco) com a porta do iwan em cada lado e as janelinhas altas por cima dos terraços, a laje, o friso de
    azulejo, a cornija, as ameias redondas no meio de cada lado, o tambor com as janelinhas e a cúpula grande."""
    from model_building_parts import adobe_block
    iwan = (-0.85, 0.85, PZ, PZ + 2.9)
    clere = [(u - 0.35, u + 0.35, PZ + 4.45, PZ + 5.15) for u in (-1.5, 1.5)]
    holes = {f: [iwan] + clere for f in ('+x', '-x', '+y', '-y')}
    adobe_block(p, -C, C, -C, C, PZ, ROOF, t=PT, holes=holes, parapet=0, beams=(), interior=False, roof_t=0.3)
    for f in ('+x', '-x', '+y', '-y'):
        F = WallFace(f, C, 0.0) if f[0] == '+' else WallFace(f, -C, 0.0)
        at = C if f[0] == '+' else -C
        s = F.s
        # a porta do iwan: o arco no vão, o escuro de dentro, as 2 folhas de madeira cravejadas entreabertas
        cx_ = (at - s * PT / 2, 0.0) if f[1] == 'x' else (0.0, at - s * PT / 2)
        arch_fill(p, 'Plaster#arch', cx_, f[1], 1.7, PZ + 2.05, PZ + 2.9, PT)
        wbox(p, 'Dark', F, -0.85, 0.85, PZ, PZ + 2.05, -PT + 0.02, -PT + 0.05, bevel=0.0)
        p.cyl('Dark', F.pt(0.0, PZ + 2.05, -PT + 0.035), f[1], 0.85, 0.03, seg=16, bevel=0.0)
        for (ua, ub) in ((-0.82, -0.25), (0.25, 0.82)):
            wbox(p, 'Wood#door', F, ua, ub, PZ + 0.02, PZ + 2.0, -PT * 0.6, -PT * 0.6 + 0.07, bevel=0.012)
            for z in splits(PZ + 0.3, PZ + 1.8, 0.5):
                for uu in (ua + 0.12, ub - 0.12):
                    wbox(p, 'CnGold#stud', F, uu - 0.03, uu + 0.03, z - 0.03, z + 0.03, -PT * 0.6 + 0.07, -PT * 0.6 + 0.1, bevel=0.0)
        arch_band(p, 'Tile#arch', (at, 0.0) if f[1] == 'x' else (0.0, at), f[1], 1.7, PZ + 2.05, s * 0.005, width=0.14, t=0.04, seg=14)
        for u in (-1.5, 1.5):
            x_, y_ = (at, u) if f[1] == 'x' else (u, at)
            arch_window(p, x_, y_, PZ + 4.45, f, w=0.7, h=0.7)
        # o friso de azulejo e a cornija (só no meio: os cantos são das torrinhas)
        wbox(p, 'Tile#frieze', F, -PV0, PV0, ROOF - 0.62, ROOF - 0.42, 0.0, 0.03, bevel=0.0)
        wbox(p, 'Plaster#lo', F, -PV0, PV0, ROOF - 0.12, ROOF + 0.06, -0.1, 0.12)
        a_, b_ = ((at - s * 0.2, -PV0 + 0.15), (at - s * 0.2, PV0 - 0.15)) if f[1] == 'x' else ((-PV0 + 0.15, at - s * 0.2), (PV0 - 0.15, at - s * 0.2))
        wall_seg(p, a_, b_, 0.4, t=0.35, z0=ROOF + 0.05, band=False, round_top=False)
        round_merlons(p, a_, b_, ROOF + 0.45, n=7, w=0.5, t=0.33, h=0.38)
    # o tambor e a cúpula
    p.cyl('Plaster#s', (0.0, 0.0, ROOF + DRUM_H / 2), 'z', DRUM_R, DRUM_H, seg=64, bevel=0.0, smooth=True)
    disc_ring(p, 'Tile', (0.0, 0.0), DRUM_R + 0.03, DRUM_R - 0.1, ROOF + DRUM_H - 0.25, ROOF + DRUM_H + 0.05, seg=64)
    disc_ring(p, 'Plaster#lo', (0.0, 0.0), DRUM_R + 0.14, DRUM_R - 0.3, ROOF + DRUM_H + 0.05, ROOF + DRUM_H + 0.2, seg=64)
    for k in range(16):
        a = 2 * math.pi * (k + 0.5) / 16
        q = (( DRUM_R - 0.03) * math.cos(a), (DRUM_R - 0.03) * math.sin(a))
        p.obox('Dark', (q[0], q[1], ROOF + 0.42), (0.08, 0.34, 0.44), yaw=math.degrees(a), bevel=0.0)
        p.cyl('Dark', (q[0], q[1], ROOF + 0.64), (math.cos(a), math.sin(a), 0.0), 0.17, 0.08, seg=12, bevel=0.0)
    tile_dome(p, (0.0, 0.0), DOME_R, DOME_H, ROOF + DRUM_H + 0.2, seg=64, rings=18, ribs=16, band=True, seed=17, holes=0.3)


def pavilions(p):
    """As 4 torrinhas dos cantos (no chão, cada uma no rodapé dela: a base acompanha a forma): a quina gasta, as janelas em arco nas
    2 faces de fora (as de baixo com sacos de areia), o friso de azulejo, a cornija, as ameias, o tambor octogonal e a cupulinha
    turquesa com o remate de latão; as bandeiras nas 2 da frente."""
    for sx in (-1, 1):
        for sy in (-1, 1):
            x0, x1 = sorted((sx * PV0, sx * PV1))
            y0, y1 = sorted((sy * PV0, sy * PV1))
            p.box('Plaster#pav', x0 - 0.18, x1 + 0.18, y0 - 0.18, y1 + 0.18, -0.2, 0.42, bevel=0.06)
            p.box('Plaster', x0, x1, y0, y1, 0.3, PV_TOP, bevel=0.13)
            cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
            for (f, at, u) in (('+x' if sx > 0 else '-x', x1 if sx > 0 else x0, cy), ('+y' if sy > 0 else '-y', y1 if sy > 0 else y0, cx)):
                F = WallFace(f, at, 0.0)
                # a janela alta em arco (o vão raso: o escuro recuado com a moldura) e a de baixo com o ninho de sacos
                for (zb, h, w) in ((PZ + 3.3, 1.5, 0.75), (1.05, 1.25, 0.7)):
                    wbox(p, 'Dark', F, u - w / 2, u + w / 2, zb, zb + h - w / 2, -0.02, 0.012, bevel=0.0)
                    p.cyl('Dark', F.pt(u, zb + h - w / 2, -0.004), f[1], w / 2, 0.032, seg=16, bevel=0.0)
                    arch_band(p, 'Plaster#s', (at, u) if f[1] == 'x' else (u, at), f[1], w, zb + h - w / 2, F.s * 0.005, width=0.12, t=0.05, seg=12)
                    wbox(p, 'Plaster#sill', F, u - w / 2 - 0.1, u + w / 2 + 0.1, zb - 0.1, zb + 0.01, -0.02, 0.1, bevel=0.03)
                wbox(p, 'Tile#frieze', F, (y0 if f[1] == 'x' else x0) + 0.15, (y1 if f[1] == 'x' else x1) - 0.15, PV_TOP - 0.62, PV_TOP - 0.44,
                     0.0, 0.03, bevel=0.0)
                wbox(p, 'Player#band', F, (y0 if f[1] == 'x' else x0) + 0.15, (y1 if f[1] == 'x' else x1) - 0.15, PV_TOP - 1.2, PV_TOP - 0.95,
                     0.0, 0.025, bevel=0.0)
                # o ninho de sacos na frente da janela de baixo (no chão)
                n = F.normal()
                q0 = (F.pt(u - 0.75, 0.0, 0.45)[0], F.pt(u - 0.75, 0.0, 0.45)[1])
                q1 = (F.pt(u + 0.75, 0.0, 0.45)[0], F.pt(u + 0.75, 0.0, 0.45)[1])
                bag_row(p, [q0, q1], 0.0, layers=4, bag_size=(0.62, 0.36, 0.25), seed=int(cx * 3 + cy))
            p.box('Plaster#lo', x0 - 0.12, x1 + 0.12, y0 - 0.12, y1 + 0.12, PV_TOP - 0.05, PV_TOP + 0.12)
            for (a_, b_) in (((x0 + 0.2, y0 + 0.17), (x1 - 0.2, y0 + 0.17)), ((x0 + 0.2, y1 - 0.17), (x1 - 0.2, y1 - 0.17)),
                             ((x0 + 0.17, y0 + 0.5), (x0 + 0.17, y1 - 0.5)), ((x1 - 0.17, y0 + 0.5), (x1 - 0.17, y1 - 0.5))):
                round_merlons(p, a_, b_, PV_TOP + 0.12, n=3, w=0.42, t=0.3, h=0.34)
            # o tambor octogonal com as janelinhas e a cupulinha
            p.cyl('Plaster#s', (cx, cy, PV_TOP + 0.12 + 0.33), 'z', 1.02, 0.66, seg=8, bevel=0.0)
            for k in range(8):
                a = 2 * math.pi * (k + 0.5) / 8
                q = (cx + 0.96 * math.cos(a), cy + 0.96 * math.sin(a))
                p.obox('Dark', (q[0], q[1], PV_TOP + 0.45), (0.06, 0.18, 0.32), yaw=math.degrees(a), bevel=0.0)
            tile_dome(p, (cx, cy), PAV_DOME_R, PAV_DOME_H, PV_TOP + 0.78, seg=28, rings=10, ribs=8, band=False, seed=40 + sx * 3 + sy,
                      holes=0.36)
    for sy in (-1, 1):
        flagpole(p, PV1 - 0.35, sy * (PV1 - 0.35), PV_TOP + 0.12, 2.5, length=1.3, height=0.85, phase=0.4 if sy < 0 else 1.6)


# ===================================================================================== as arcadas
def porch(p, f):
    """A arcada de um lado: a parede da frente com o arco grande do meio e os 2 menores (os pilares com a quina gasta, os tímpanos, a
    moldura), as paredes dos lados com um arco cada, a laje, o friso de azulejo, a cornija e o terraço de ameias redondas, as
    lanternas de latão penduradas."""
    s = 1 if f[0] == '+' else -1
    ax = f[1]
    D = PORCH[f]
    F = WallFace(f, s * D, 0.0)
    n0 = s * (D - PT)                                            # o lado de dentro da parede da frente
    u0, u1 = -PV0, PV0
    piers = [(-PV0, -2.4), (-1.4, -0.9), (0.9, 1.4), (2.4, PV0)]
    for (a, b) in piers:
        wbox(p, 'Plaster', F, a, b, PZ - 0.02, PORCH_H, -PT, 0.0)
        wbox(p, 'Plaster#base', F, a - 0.06, b + 0.06, PZ - 0.02, PZ + 0.3, -PT - 0.06, 0.06, bevel=0.03)
        wbox(p, 'Tile#cap', F, a - 0.03, b + 0.03, PZ + 2.0, PZ + 2.12, -PT - 0.03, 0.03, bevel=0.0)
    for (a, b, spring) in ((-2.4, -1.4, PZ + 2.12), (-0.9, 0.9, PZ + 2.4), (1.4, 2.4, PZ + 2.12)):
        w = b - a
        cu = (a + b) / 2
        c = (s * (D - PT / 2), cu) if ax == 'x' else (cu, s * (D - PT / 2))
        arch_fill(p, 'Plaster#arch', c, ax, w, spring, PORCH_H, PT)
        arch_band(p, 'Plaster#s', (s * D, cu) if ax == 'x' else (cu, s * D), ax, w, spring, s * 0.005, width=0.16, t=0.05, seg=16)
        # a lanterna de latão pendurada no arco
        q = F.pt(cu, spring + w / 2 - 0.05, -PT / 2)
        rod(p, 'Dark', q, (q[0], q[1], q[2] - 0.45), 0.008, seg=4)
        p.cyl('CnGold#lamp', (q[0], q[1], q[2] - 0.6), 'z', 0.12, 0.26, seg=10, r2=0.07, bevel=0.0)
        p.cyl('Light', (q[0], q[1], q[2] - 0.78), 'z', 0.08, 0.12, seg=10, bevel=0.0, smooth=True)
    # as paredes dos lados (de cada torrinha até a frente), com um arco cada
    for t in (-1, 1):
        Fs = WallFace(('+y' if t > 0 else '-y') if ax == 'x' else ('+x' if t > 0 else '-x'), t * PV0, 0.0)
        na, nb = PV1, D
        if nb - na > 0.2:
            lo, hi = sorted((s * na, s * nb))
            mid = (lo + hi) / 2
            w = min(0.95, (hi - lo) - 0.6)
            for (a, b) in ((lo, mid - w / 2), (mid + w / 2, hi)):
                wbox(p, 'Plaster', Fs, a, b, PZ - 0.02, PORCH_H, -PT, 0.0)
            if w > 0.3:
                c = (mid, t * (PV0 - PT / 2)) if ax == 'x' else (t * (PV0 - PT / 2), mid)
                arch_fill(p, 'Plaster#arch', c, 'y' if ax == 'x' else 'x', w, PZ + 1.95, PORCH_H, PT)
    # a laje, o forro escuro de madeira, o friso de azulejo, a cornija e o terraço
    lo, hi = sorted((s * C, s * D))
    if ax == 'x':
        p.box('Plaster#slab', lo, hi, -PV0, PV0, PORCH_H, PORCH_H + 0.3, bevel=0.03)
        p.box('Wood#ceil', lo, hi, -PV0 + PT, PV0 - PT, PORCH_H - 0.08, PORCH_H, bevel=0.0)
    else:
        p.box('Plaster#slab', -PV0, PV0, lo, hi, PORCH_H, PORCH_H + 0.3, bevel=0.03)
        p.box('Wood#ceil', -PV0 + PT, PV0 - PT, lo, hi, PORCH_H - 0.08, PORCH_H, bevel=0.0)
    wbox(p, 'Tile#frieze', F, u0, u1, PORCH_H - 0.4, PORCH_H - 0.22, 0.0, 0.03, bevel=0.0)
    wbox(p, 'Plaster#lo', F, u0 - 0.06, u1 + 0.06, PORCH_H + 0.18, PORCH_H + 0.36, -0.1, 0.12)
    z = PORCH_H + 0.36
    if ax == 'x':
        segs = [((s * (D - 0.18), -PV0 + 0.2), (s * (D - 0.18), PV0 - 0.2))]
        segs += [((s * (PV1 + 0.1), t * (PV0 - 0.18)), (s * (D - 0.35), t * (PV0 - 0.18))) for t in (-1, 1)]
    else:
        segs = [((-PV0 + 0.2, s * (D - 0.18)), (PV0 - 0.2, s * (D - 0.18)))]
        segs += [((t * (PV0 - 0.18), s * (PV1 + 0.1)), (t * (PV0 - 0.18), s * (D - 0.35))) for t in (-1, 1)]
    # (revisão: as ameias em volta dos 4 terraços liam como muretas de cima) as ameias só na frente; nos lados o parapeito liso e baixo
    for i, (a_, b_) in enumerate(segs):
        wall_seg(p, a_, b_, 0.35 if i == 0 else 0.22, t=0.3, z0=z, band=False, round_top=i > 0)
        if i == 0:
            L = math.dist(a_, b_)
            round_merlons(p, a_, b_, z + 0.35, n=max(2, int(L / 0.95)), w=0.42, t=0.28, h=0.32)
    return F


def occupation(p, faces):
    """A GLA no palácio: os estandartes verdes nos arcos, os tapetes no parapeito, os ninhos de sacos com a metralhadora nos terraços
    da frente e do lado −Y, a caixa d'água e a antena no teto do corpo."""
    Ff = faces['+x']
    for u in (-1.9, 1.9):
        banner(p, Ff, u, PZ + 2.55, w=0.66, L=1.6, n_in=-PT / 2)
    banner(p, faces['-y'], 1.9, PZ + 2.55, w=0.66, L=1.6, n_in=-PT / 2)
    banner(p, faces['+y'], -1.9, PZ + 2.55, w=0.66, L=1.6, n_in=-PT / 2)
    zt = PORCH_H + 0.71
    for u in (-1.15, 1.15):
        carpet(p, (PORCH['+x'] + 0.02, u), zt, '+x', w=0.95, h=1.25)
    carpet(p, (0.6, -PORCH['-y'] - 0.02), zt, '-y', w=1.1, h=1.3)
    # os ninhos de sacos nos terraços (o U aberto pra dentro) e as metralhadoras
    zr = PORCH_H + 0.3
    bag_row(p, [(C + 0.5, -2.3), (PORCH['+x'] - 0.45, -2.3), (PORCH['+x'] - 0.45, -0.3)], zr, layers=3, seed=31)
    dshk(p, C + 1.15, -1.3, zr, yaw=-20)
    crate(p, C + 0.6, -0.6, zr, size=(0.7, 0.45, 0.35), yaw=8)
    bag_row(p, [(1.6, -C - 0.5), (1.6, -PORCH['-y'] + 0.45), (-0.4, -PORCH['-y'] + 0.45)], zr, layers=3, seed=32)
    dshk(p, 0.7, -C - 1.4, zr, yaw=-100)
    # o teto do corpo: a caixa d'água de ferrugem nos pés, o prato de TV roubado, a antena de fio
    tx, ty = -4.2, 1.7
    for (dx, dy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        rod(p, 'Trim', (tx + dx * 0.35, ty + dy * 0.35, ROOF), (tx + dx * 0.35, ty + dy * 0.35, ROOF + 0.6), 0.04)
    p.box('Trim', tx - 0.45, tx + 0.45, ty - 0.45, ty + 0.45, ROOF + 0.55, ROOF + 0.62, bevel=0.0)
    p.cyl('Rust', (tx, ty, ROOF + 1.17), 'z', 0.48, 1.1, seg=20, bevel=0.0, smooth=True)
    p.cyl('Rust', (tx, ty, ROOF + 1.8), 'z', 0.5, 0.16, seg=20, r2=0.13, bevel=0.0)
    p.cyl('Trim', (4.25, 1.9, ROOF + 0.5), 'z', 0.04, 1.0, seg=6, bevel=0.0)
    p.cyl('Metal', (4.3, 1.85, ROOF + 1.05), (0.6, -0.3, 0.75), 0.45, 0.05, seg=20, bevel=0.0)
    rod(p, 'Dark', (-4.3, -1.9, ROOF), (-4.35, -1.85, ROOF + 2.6), 0.015, seg=4)


# ===================================================================================== o carro de luxo e os cantos
def luxury_car(p, cx, cy, yaw):
    """O carro de luxo roubado (o sedã preto comprido, a grade e as rodas de latão) debaixo da lona amarrada: a lona verde-oliva cobre a
    traseira e a cabine, o capô e a grade ficam à mostra."""
    f = Frame(p, (cx, cy, 0.0), yaw=yaw)
    L, W = 5.1, 1.9
    xs = [-2.55, -2.45, -1.9, -1.2, -0.7, 0.2, 0.75, 1.6, 2.4, 2.55]
    zt = [0.78, 0.98, 1.02, 1.04, 1.42, 1.44, 1.02, 0.98, 0.9, 0.72]
    wd = [1.6, 1.82, 1.9, 1.9, 1.86, 1.86, 1.9, 1.9, 1.84, 1.62]

    def height(x):
        for i in range(len(xs) - 1):
            if xs[i] <= x <= xs[i + 1]:
                t = (x - xs[i]) / (xs[i + 1] - xs[i])
                return zt[i] + (zt[i + 1] - zt[i]) * t, wd[i] + (wd[i + 1] - wd[i]) * t
        return 0.0, 0.0
    rings = []
    for (x, z, w) in zip(xs, [min(z_, 1.05) for z_ in zt], wd):
        zb = 0.22
        zm, hz = (zb + z) / 2, (z - zb) / 2
        ring = []
        for k in range(12):
            a = 2 * math.pi * k / 12
            yy = w / 2 * math.copysign(abs(math.cos(a)) ** 0.35, math.cos(a))
            zz = zm + hz * math.copysign(abs(math.sin(a)) ** 0.45, math.sin(a))
            ring.append(f.pt(x, yy, zz))
        rings.append(ring)
    p.loft('Dark#car', rings, bevel=0.0, smooth=True)
    gx = [-1.25, -0.7, 0.2, 0.8]
    gz = [1.02, 1.42, 1.44, 1.02]
    rings = []
    for (x, z) in zip(gx, gz):
        w = 1.62 if z > 1.2 else 1.8
        rings.append([f.pt(x, -w / 2, 1.0), f.pt(x, w / 2, 1.0), f.pt(x, w / 2 * 0.92, z), f.pt(x, -w / 2 * 0.92, z)])
    p.loft('Glass#car', rings, bevel=0.0)
    for x in (-1.6, 1.55):
        for s in (-1, 1):
            f.cyl('Rubber', (x, s * 0.82, 0.36), 'y', 0.36, 0.26, seg=18, bevel=0.0)
            f.cyl('CnGold#rim', (x, s * 0.96, 0.36), 'y', 0.22, 0.03, seg=14, bevel=0.0)
    f.box('CnGold#grille', (2.56, 0.0, 0.7), (0.04, 0.9, 0.26))
    for s in (-1, 1):
        f.box('Light#head', (2.54, s * 0.68, 0.74), (0.04, 0.3, 0.12))
    # a lona: da traseira até o para-brisa, caindo até perto do chão, as dobras, as cordas por baixo
    def fn(u, v):
        x = -2.85 + 3.75 * u
        y = -1.25 + 2.5 * v
        z = 0.0
        for (ox, oy) in ((0, 0), (0.22, 0), (-0.22, 0), (0, 0.22), (0, -0.22)):
            zz, w = height(min(max(x + ox, -2.55), 2.55))
            if abs(y + oy) <= w / 2:
                z = max(z, zz + 0.05 - 0.25 * math.hypot(ox, oy))
        z = max(z, 0.32 - 0.2 * abs(v - 0.5))
        z += 0.03 * (fbm(x * 2.0, y * 2.0, 3) - 0.5)
        return f.pt(x, y, z)
    surface(p, 'Crate#tarp', fn, 26, 14, thick=0.03)
    for x in (-2.0, -0.4):
        f.rod('Dark#rope', (x, -1.2, 0.32), (x, -0.96, 1.02), 0.016, seg=4)
        f.rod('Dark#rope', (x, 1.2, 0.32), (x, 0.96, 1.02), 0.016, seg=4)


def corners(p):
    """Os cantos do lote (fora da cruz, no chão do mapa): o carro de luxo (+X, +Y), as palmeiras cortadas (−X, −Y), o gerador com os
    tambores (−X, +Y), o posto de sacos (+X, −Y)."""
    earth_pad(p, (7.3, 8.6), blob(7.3, 8.6, 1.7, 3.3, seed=2, amp=0.1), h=0.03)
    luxury_car(p, 7.3, 8.6, 92.0)
    for (x, y, h, sd) in ((-7.2, -6.4, 1.9, 1), (-6.4, -9.6, 1.4, 2), (-8.0, -9.0, 2.3, 3), (6.9, -9.9, 1.6, 4)):
        lean = (_hash(sd, 1, 5) - 0.5) * 0.25
        for k in range(int(h / 0.3)):
            z = k * 0.3
            p.cyl('Wood#palm', (x + lean * z / h, y, z + 0.15), 'z', 0.2 - 0.012 * (k % 2), 0.3, seg=8, r2=0.17, bevel=0.0)
        tx = x + lean
        # (revisão: as folhas retas liam asterisco) a folha em arco: sobe um pouco e cai, larga, as de baixo secas e penduradas
        for k in range(7):
            a = 2 * math.pi * k / 7 + sd
            Lf = 1.0 + 0.45 * _hash(k, sd, 9)
            dry = k % 3 == 0
            ca, sa = math.cos(a), math.sin(a)
            p0 = (tx, y, h + 0.08)
            p1 = (tx + ca * Lf * 0.45, y + sa * Lf * 0.45, h + 0.08 + (0.05 if not dry else -0.25))
            p2 = (tx + ca * Lf * 0.85, y + sa * Lf * 0.85, h + 0.08 - (0.35 if not dry else 0.9))
            for (q0, q1, w) in ((p0, p1, 0.3), (p1, p2, 0.22)):
                c = ((q0[0] + q1[0]) / 2, (q0[1] + q1[1]) / 2, (q0[2] + q1[2]) / 2)
                L_ = math.dist(q0, q1)
                pit = math.degrees(math.atan2(q1[2] - q0[2], math.hypot(q1[0] - q0[0], q1[1] - q0[1])))
                p.obox('Wood#dry' if dry else 'Net#frond', c, (L_ + 0.04, w, 0.025), yaw=math.degrees(a), pitch=pit, roll=12, bevel=0.0)
        p.cyl('Wood#palm', (tx, y, h + 0.06), 'z', 0.15, 0.12, seg=8, bevel=0.0)
    # o gerador a diesel com os tambores e o cabo indo pro palácio
    gx, gy = -6.6, 7.6
    p.box('Rust#gen', gx - 0.9, gx + 0.9, gy - 0.5, gy + 0.5, 0.0, 0.25, bevel=0.0)
    p.box('Crate#gen', gx - 0.75, gx + 0.55, gy - 0.42, gy + 0.42, 0.25, 1.05, bevel=0.04)
    p.cyl('Rust', (gx + 0.6, gy - 0.25, 1.4), 'z', 0.07, 1.2, seg=8, bevel=0.0)
    for (x, y) in ((gx - 0.4, gy + 1.0), (gx + 0.25, gy + 1.05), (gx + 1.4, gy + 0.2)):
        barrel(p, x, y, 0.0, r=0.29, h=0.86, m='Rust')
    tube(p, 'Dark#cable', [(gx + 0.9, gy, 0.3), (gx + 2.0, gy - 0.4, 0.04), (-PV1 - 0.2, PV1 - 0.6, 0.04), (-PV1 - 0.02, PV1 - 0.6, 0.5)], 0.03, seg=6)
    # o posto de sacos com a metralhadora (+X, −Y), virado pra fora
    bag_ring(p, (6.9, -6.6), 1.0, 0.0, layers=3, gaps=[(105.0, 165.0)], bag_size=(0.6, 0.34, 0.24), seed=9)
    dshk(p, 6.8, -6.5, 0.0, yaw=-35)


def base():
    p = new_part(PREFIX + '_Base')
    for m in ('Tile', 'Plaster#rib', 'Player#ban', 'Cloth#rug', 'Player#rug', 'Rust#rug', 'Dark#car', 'Glass#car', 'Crate#tarp'):
        p.bm(m, 0.0)
    p.bm('Plaster#pod', 0.13)
    podium(p)
    core(p)
    pavilions(p)
    faces = {f: porch(p, f) for f in ('+x', '-x', '+y', '-y')}
    occupation(p, faces)
    corners(p)
    return finish(p)


def write_preview(out):
    import json
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1400, 1400]], ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1600, 1000]],
                      ['side_left', 'ortho', [0, -1, 0], [0, 0, 1], [1600, 1000]],
                      ['three_quarter', 'persp', 34.0, 215.0, [1600, 1000], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 32.0, 35.0, [1600, 1000], 30.0, 0.95]],
            'usa_at': [13.0, -4.0, 0], 'tank_views': True,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(out, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, indent=1)


def main():
    out = out_dir('gla_palace_v2')
    clear_scene()
    total = export(base(), out)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -PX_END + 0.6, PX_END - 0.6, -PY_END + 0.6, PY_END - 0.6, ROOF + 0.9, post='Wood', plank='Crate',
                             stripe='Hazard'), out)
    write_preview(out)
    zt = ROOF + DRUM_H + 0.2 + DOME_H
    print('MODEL total', total, 'tris')
    print('MODEL lote 18 x 24 m; o podio', PZ, 'm; o corpo', round(ROOF, 2), 'm; as torrinhas', round(PV_TOP, 2), 'm; a cupula ate', round(zt, 2),
          'm; o remate ate', round(zt + 1.18, 2), 'm')
    print('MODEL fire_points_m', [(0.0, 0.0, round(zt - 1.2, 2)), (-(PV0 + PV1) / 2, -(PV0 + PV1) / 2, round(PV_TOP + 0.6, 2))])


if __name__ == '__main__':
    main()
