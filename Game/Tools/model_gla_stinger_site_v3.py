"""O Stinger Site da GLA (a defesa antiaérea), v3 (02/11/2026): A CAIXA D'ÁGUA. Do zero, pelo veredito de 02/11
(docs/elenco/IDEIAS_NOVAS_02_11.md: "quero ideias do 0, diferente, novas, bonitas, bem feitas, criativas"). O v1 (o anel de sacos murado) e
o v2 (o ninho em trevo sob as redes) ficam como estão.

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_gla_stinger_site_v3.py C:/Users/luisf/explosion/captures/game/models/gla_stinger_site_v3

A palavra (o que lê de cima): A CAIXA D'ÁGUA DA VILA TOMADA. O tanque de ferro rebitado sobre as 4 pernas de treliça (a coisa alta que toda
vila do deserto tem: o jogador lê "é dali que atiram"), com o topo cortado a maçarico (a borda serrilhada, mais baixa na frente) virando o
ninho: o piso de tábuas lá dentro, o anel de sacos de areia encostado no costado, os tubos de reserva, a lâmpada; o Stinger na mesa
giratória no meio, com o atirador sentado (a torre). O balcão em volta do tanque com o guarda-corpo, a escada de marinheiro com a gaiola
subindo pela perna da frente e a do costado com o gancho por cima da borda, o guincho de cano com a corda içando o caixote, a rede de
camuflagem enrolada em espiral em 2 pernas e o pedaço pendurado do balcão, a pichação verde e o anel verde pintado à mão no tanque, a
bandeira no guarda-corpo, os caixotes de míssil e os tambores no pé. As pernas são treliça e os tirantes são finos: não tapam unidade.
De cima: o círculo do tanque com o ninho dentro (o anel de sacos, o feixe de tubos) e a sombra das pernas cruzadas no chão.

Lote 8 × 8 m, frente em +X. Alturas: o fundo do tanque 6,0 m, o piso do ninho 7,45 m, a borda cortada 8,0–8,5 m, a boca dos tubos ~9,4 m.

Peças (origem no chão, no centro do lote; Unreal X frente, Y direita, Z cima, metros):
  SM_GLA_StingerSite_Base: a torre, o tanque, o balcão, as escadas, o ninho (sem o lançador), as redes, os caixotes (a bandeira sai no
    flags.json);
  SM_GLA_StingerSite_Turret (gira em volta de (0, 0), a mesma origem do modelo, no piso do ninho): a mesa, o pedestal, o garfo, os 3
    tubos, a antena do IFF, o assento e o atirador; a boca no relatório;
  SM_GLA_StingerSite_Scaffold: o andaime da obra em madeira.
Materiais (slots): Rust (a ferrugem do tanque), GlaPaint (a tinta velha que sobrou no tanque), Metal (a treliça, o balcão, as escadas), Dark,
  Plaster (os pilares), Wood, Cloth (os sacos, a roupa), Crate (os tubos e os caixotes verde-oliva), Net, Player (o verde: a pichação, o
  anel, o lenço, as faixas, a bandeira), Light (a lâmpada), Earth, Hazard (só no andaime).
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
import model_vehicle_parts as vp  # noqa: E402

# ===================================================================================== kit GLA v2 (01/11)
# (copiado do model_gla_barracks_v2.py como está; o kit v3 logo abaixo acrescenta o que as ideias de 02/11 pedem. O model_lib, o
# model_faction_kit e o model_building_parts não são editados)
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

# ===================================================================================================== kit GLA v3 (02/11)
# (a mesma cópia nos 3 prédios v3 da GLA: model_gla_barracks_v3/stinger_site_v3/supply_stash_v3.py; o kit v2 acima mais o que as
# ideias novas pedem: o loft com o material por face (a tinta que descasca pra ferrugem), a seção de cantos redondos, o rabisco à mão
# em verde, o pneu por perfil em qualquer eixo, a treliça)
COLORS.update({'Yellow': (204, 150, 40), 'Paint': (196, 194, 184), 'GlaPaint': (170, 150, 100), 'Fire': (255, 140, 40),
               'Glass': (24, 28, 32)})
BEVELS.update({'Yellow': 0.02, 'Paint': 0.02, 'GlaPaint': 0.02, 'Glass': 0.0, 'Fire': 0.0})


def loft_mat(p, rings, matfn, to_w=None, caps=True, cap_m=None, smooth=False, closed=True):
    """O loft dos anéis (pontos no espaço local; to_w(q) → Unreal) com o material de cada face escolhido por matfn(centro local, i, j)
    (a tinta, a ferrugem que aparece onde descascou): a emenda entre materiais é uma aresta dura, que lê como a borda da mancha."""
    to_w = to_w or (lambda q: q)
    n = len(rings[0])
    W = [[to_w(q) for q in r] for r in rings]
    cache = {}

    def vert(key, i, j):
        k = (key, i, j)
        if k not in cache:
            cache[k] = p.bm(key, BEVELS.get(key, 0.0)).verts.new(V(*W[i][j]))
        return cache[k]
    for i in range(len(rings) - 1):
        for j in range(n if closed else n - 1):
            jj = (j + 1) % n
            q = [Vector(rings[i][j]), Vector(rings[i][jj]), Vector(rings[i + 1][jj]), Vector(rings[i + 1][j])]
            key = matfn(sum(q, Vector()) / 4, i, j)
            f = p.bm(key, BEVELS.get(key, 0.0)).faces.new([vert(key, i, j), vert(key, i, jj), vert(key, i + 1, jj), vert(key, i + 1, j)])
            f.smooth = smooth
    if caps:
        for (i, rev) in ((0, True), (len(rings) - 1, False)):
            key = cap_m or matfn(sum((Vector(q) for q in rings[i]), Vector()) / n, i, 0)
            vs = [vert(key, i, j) for j in range(n)]
            p.bm(key, BEVELS.get(key, 0.0)).faces.new(list(reversed(vs)) if rev else vs)


def rrect(y0, y1, z0, z1, rb=0.08, rt=0.0, n=4):
    """A seção de cantos redondos no plano (y, z): de cima à esquerda, descendo, por baixo, subindo pela direita (fechada)."""
    pts = [(y0, z1 - rt)] if rt <= 0 else []
    if rt > 0:
        for k in range(n + 1):
            a = math.pi / 2 + (math.pi / 2) * k / n
            pts.append((y0 + rt + rt * math.cos(a), z1 - rt + rt * math.sin(a)))
    for k in range(n + 1):
        a = math.pi + (math.pi / 2) * k / n
        pts.append((y0 + rb + rb * math.cos(a), z0 + rb + rb * math.sin(a)))
    for k in range(n + 1):
        a = 1.5 * math.pi + (math.pi / 2) * k / n
        pts.append((y1 - rb + rb * math.cos(a), z0 + rb + rb * math.sin(a)))
    if rt > 0:
        for k in range(n + 1):
            a = (math.pi / 2) * k / n
            pts.append((y1 - rt + rt * math.cos(a), z1 - rt + rt * math.sin(a)))
    else:
        pts.append((y1, z1))
    return pts


def wheel(p, c, axis, R=0.5, w=0.28, rim=0.3, sunk=0.0, m='Rubber', rim_m='Metal#rim', hub_m='Dark#hub', bald=False):
    """O pneu por perfil (o ombro redondo, o flanco, a banda) e o aro de chapa com o cubo, em qualquer eixo (o eixo da roda). sunk: o
    pneu murcho (o centro mais baixo: a banda afunda no chão)."""
    cx, cy, cz = c
    c = (cx, cy, cz - sunk)
    hw = w / 2
    prof = [(rim, -hw + 0.02), (R - 0.07, -hw), (R - 0.02, -hw + 0.04), (R, -hw + 0.1), (R, hw - 0.1), (R - 0.02, hw - 0.04),
            (R - 0.07, hw), (rim, hw - 0.02)]
    vp.lathe(p, m + '#tyre', c, axis, prof, seg=20, closed=True)
    vp.lathe(p, rim_m, c, axis, [(0.0, -hw + 0.06), (rim * 0.55, -hw + 0.06), (rim, -hw + 0.03), (rim + 0.01, 0.0), (rim, hw - 0.03),
                                 (rim * 0.55, hw - 0.06), (0.0, hw - 0.06)], seg=16)
    A = Vector(axis).normalized()
    for s in (-1, 1):
        vp.lathe(p, hub_m, tuple(Vector(c) + A * s * (hw - 0.05)), tuple(A * s), [(0.0, 0.0), (0.09, 0.0), (0.07, 0.04), (0.0, 0.05)],
                 seg=10)


def stroke(F, m, a, b, w, y, t=0.012):
    """Um traço de pincel (a tinta verde à mão) no plano lateral y do quadro F, de a=(x, z) até b=(x, z), largura w."""
    L = math.hypot(b[0] - a[0], b[1] - a[1])
    if L < 1e-3:
        return
    ang = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
    F.box(m, ((a[0] + b[0]) / 2, y, (a[1] + b[1]) / 2), (L + w * 0.6, t, w), pitch=ang, bevel=0.0)


def tag(F, y, x0, z0, s=1.0, seed=0, m='Player#graf', mirror=False):
    """O nome pintado à mão (a caligrafia de mentira, sem letra de verdade): a linha de base ondulada, os traços altos, a barriga de
    uma letra, os pingos, o sublinhado em espada; ~2,6 m × 0,7 m em s = 1."""
    d = -1.0 if mirror else 1.0
    X = lambda x: x0 + d * x * s
    pts = [(X(0.0 + 0.32 * k), z0 + 0.035 * s * math.sin(k * 1.7 + seed)) for k in range(9)]
    for a, b in zip(pts[:-1], pts[1:]):
        stroke(F, m, a, b, 0.07 * s, y)
    for k, x in enumerate((0.15, 0.62, 1.05, 1.7, 2.3)):
        h = (0.4 + 0.25 * _hash(k, seed, 5)) * s
        stroke(F, m, (X(x), z0), (X(x + 0.06 * d), z0 + h), 0.065 * s, y)
    for k in range(6):
        a0, a1 = math.pi * (1 + k / 6), math.pi * (1 + (k + 1) / 6)
        cx, r = 1.38, 0.17
        stroke(F, m, (X(cx + r * math.cos(a0)), z0 + r * math.sin(a0)), (X(cx + r * math.cos(a1)), z0 + r * math.sin(a1)), 0.06 * s, y)
    for (x, z) in ((0.4, 0.3), (0.85, -0.18), (0.95, -0.18), (2.0, 0.35)):
        stroke(F, m, (X(x), z0 + z * s), (X(x + 0.07), z0 + z * s + 0.02), 0.07 * s, y)
    stroke(F, m, (X(-0.1), z0 - 0.3 * s), (X(2.6), z0 - 0.24 * s), 0.05 * s, y)
    stroke(F, m, (X(2.6), z0 - 0.24 * s), (X(2.75), z0 - 0.14 * s), 0.05 * s, y)


def truss(p, m, a, b, w, up=(0.0, 0.0, 1.0), chords=4, bay=None, r=0.035, rd=0.022, ends=True):
    """A treliça de 3 ou 4 banzos de a até b (a perna da caixa d'água, a lança do guindaste): os banzos, os montantes e as diagonais em
    zigue-zague em cada face (o vazado que não tapa unidade)."""
    A, B = Vector(a), Vector(b)
    d = (B - A)
    L = d.length
    d.normalize()
    u = Vector(up) - d * d.dot(Vector(up))
    if u.length < 1e-4:
        u = Vector((1.0, 0.0, 0.0)) - d * d.x
    u.normalize()
    s = d.cross(u)
    if chords == 3:
        offs = [u * (w * 0.577), -u * (w * 0.289) + s * (w * 0.5), -u * (w * 0.289) - s * (w * 0.5)]
    else:
        offs = [(u + s) * (w / 2), (u - s) * (w / 2), (-u - s) * (w / 2), (-u + s) * (w / 2)]
    bay = bay or max(w * 1.1, 0.3)
    n = max(1, int(round(L / bay)))
    for o in offs:
        rod(p, m, tuple(A + o), tuple(B + o), r, seg=6)
    for k in range(n + 1):
        t = k / n
        c = A + d * (L * t)
        if ends or 0 < k < n:
            for i in range(len(offs)):
                rod(p, m, tuple(c + offs[i]), tuple(c + offs[(i + 1) % len(offs)]), rd, seg=4)
        if k < n:
            c2 = A + d * (L * (k + 1) / n)
            for i in range(len(offs)):
                j = (i + 1) % len(offs)
                if (k + i) % 2 == 0:
                    rod(p, m, tuple(c + offs[i]), tuple(c2 + offs[j]), rd, seg=4)
                else:
                    rod(p, m, tuple(c + offs[j]), tuple(c2 + offs[i]), rd, seg=4)
# ===================================================================================================== fim do kit GLA v3


PREFIX = 'SM_GLA_StingerSite'
HX, HY = 4.0, 4.0
R_T = 1.95                          # o raio do tanque
Z_B = 6.0                           # o fundo do costado (a cinta de baixo do tanque, o piso do balcão)
Z_TOP = 8.55                        # o topo do costado antes do corte
FLOOR = 7.45                        # o piso de tábuas do ninho, dentro do tanque
LEG_FOOT, LEG_TOP = 2.85, 1.38      # as pernas: o pé (x = y) e o topo (no anel do fundo do tanque)
BAL_R = 2.6                         # o raio de fora do balcão
TT = FLOOR + 0.05                   # o topo do piso (a mesa giratória senta aqui)
TRUNNION = (0.12, 0.0, FLOOR + 1.3)  # o eixo de elevação dos tubos
EL = 32.0                           # a elevação dos tubos em repouso (graus)
TUBE_L, TUBE_R = 1.75, 0.085
TUBES = [(-0.13, 0.0), (0.13, 0.0), (0.0, 0.2)]   # (y, altura acima do eixo) de cada tubo


def rim_z(a):
    """O topo do costado cortado a maçarico: a borda serrilhada (o corte torto, os dentes onde o maçarico voltou), mais baixa na frente
    (+X, o lado da câmera: o ninho aparece)."""
    front = 0.5 + 0.5 * math.cos(a - math.radians(-20))
    z = Z_TOP - 0.38 * front - 0.06 * math.sin(3 * a + 0.4)
    z -= 0.07 * abs(math.sin(11 * a + 1.3)) + 0.05 * (fbm(math.cos(a) * 3 + 2, math.sin(a) * 3 + 5, 4) - 0.5)
    return z


def leg_pt(sx, sy, t):
    """O eixo da perna (sx, sy = ±1) na fração t (0 o pé, 1 o topo)."""
    r = LEG_FOOT + (LEG_TOP - LEG_FOOT) * t
    return Vector((sx * r, sy * r, 0.35 + (Z_B - 0.35) * t))


def tower(p):
    """As 4 pernas de treliça (os 4 banzos de cantoneira com o zigue-zague), os pilares de concreto do pé, as 2 cintas de travessa e os
    tirantes em X com o esticador em cada face, o anel do fundo."""
    legs = [(1, 1), (1, -1), (-1, -1), (-1, 1)]
    for (sx, sy) in legs:
        a, b = leg_pt(sx, sy, 0.0), leg_pt(sx, sy, 1.0)
        truss(p, 'Metal#leg', tuple(a), tuple(b), 0.42, up=(sx, sy, 0.0), chords=4, bay=0.55, r=0.045, rd=0.022)
        # o pilar de concreto (o tronco de pirâmide) e a placa da base com os chumbadores
        x, y = sx * LEG_FOOT, sy * LEG_FOOT
        p.cyl('Plaster#pier', (x, y, 0.12), 'z', 0.52, 0.5, seg=4, r2=0.4, bevel=0.03)
        p.obox('Metal#plate', (x, y, 0.4), (0.62, 0.62, 0.05), yaw=45, bevel=0.0)
        for (dx, dy) in ((0.22, 0), (-0.22, 0), (0, 0.22), (0, -0.22)):
            p.cyl('Dark#bolt', (x + dx, y + dy, 0.45), 'z', 0.03, 0.08, seg=6, bevel=0.0)
    # as cintas (a travessa de treliça baixa entre as pernas) e os tirantes em X
    levels = [0.0, 0.36, 0.7, 1.0]
    for i in range(4):
        (sx0, sy0), (sx1, sy1) = legs[i], legs[(i + 1) % 4]
        for t in levels[1:3]:
            a, b = leg_pt(sx0, sy0, t), leg_pt(sx1, sy1, t)
            truss(p, 'Metal#strut', tuple(a), tuple(b), 0.22, chords=3, bay=0.5, r=0.03, rd=0.016, ends=False)
        for (t0, t1) in zip(levels[:-1], levels[1:]):
            a0, b0 = leg_pt(sx0, sy0, t0), leg_pt(sx1, sy1, t0)
            a1, b1 = leg_pt(sx0, sy0, t1), leg_pt(sx1, sy1, t1)
            for (q0, q1) in ((a0, b1), (b0, a1)):
                rod(p, 'Dark#tie', tuple(q0), tuple(q1), 0.022, seg=5)
                m = (q0 + q1) / 2
                d = (q1 - q0).normalized()
                p.cyl('Metal#buckle', tuple(m + d * 0.4), tuple(d), 0.045, 0.3, seg=6, bevel=0.0)


def tank(p):
    """O tanque de ferro rebitado: o fundo em cuia, o costado de chapas (as costuras com a fileira de rebites em relevo), o corte torto
    em cima com a espessura da chapa à mostra, o balcão em volta (a grade, as mãos-francesas, o guarda-corpo), as escadas de marinheiro,
    a pichação verde, a mancha de ferrugem escorrida."""
    # o fundo em cuia (a meia-elipse) e o anel de apoio das pernas
    prof = [(0.02, Z_B - 0.7), (0.6, Z_B - 0.64), (1.15, Z_B - 0.46), (1.6, Z_B - 0.24), (1.88, Z_B - 0.06), (R_T, Z_B)]
    revolve(p, 'Rust#tank', (0.0, 0.0), prof, seg=48, smooth=True)
    disc_ring(p, 'Metal#ring', (0.0, 0.0), R_T + 0.06, R_T - 0.2, Z_B - 0.22, Z_B + 0.02, seg=48)
    # o costado: a casca de chapa (fora, dentro e o topo cortado), a ferrugem e a tinta velha por face
    N = 96
    ang = [2 * math.pi * k / N for k in range(N)]
    zs = splits(Z_B, Z_TOP, 0.3)

    # as chapas do costado: 12 colunas (30°) × 3 cintas (até 0,85, 1,7 e o corte), cada chapa com a sua cor (a tinta velha, a
    # ferrugem, uma trocada por outra cinza), e as listras de ferrugem escorrendo (colunas finas) nas chapas pintadas
    def shellmat(c, i, j):
        col, band = (i % N) // 8, (0 if j < 4 else (1 if j < 8 else 2))
        h = _hash(col, band, 4)
        if band == 2:
            m = 'Rust#tank' if h < 0.7 else 'GlaPaint#tank'
        elif band == 1:
            m = 'GlaPaint#tank' if h < 0.55 else ('Rust#tank' if h < 0.85 else 'Paint#tank')
        else:
            m = 'GlaPaint#tank' if h < 0.7 else 'Rust#tank'
        if m != 'Rust#tank' and _hash(i, 7, 5) > 0.82 and j >= (2 if band == 0 else (5 if band == 1 else 8)):
            return 'Rust#tank'
        return m
    for (r, key) in ((R_T, 'out'), (R_T - 0.03, 'in')):
        rings = []
        for k, a in enumerate(ang + [ang[0]]):
            zt = rim_z(a)
            zz = ([Z_B + 0.85 * q / 4 for q in range(5)] + [Z_B + 0.85 + 0.85 * q / 4 for q in range(1, 5)]
                  + [Z_B + 1.7 + (zt - Z_B - 1.7) * q / 3 for q in range(1, 4)])
            rings.append([(r * math.cos(a), r * math.sin(a), z) for z in zz])
        # o loft roda em volta (os anéis são as colunas de chapa); as faces de cima e de baixo ficam abertas
        loft_mat(p, rings, shellmat if key == 'out' else (lambda c, i, j: 'Rust#inner'), caps=False, closed=False)
    # o topo do corte (a espessura da chapa, a borda queimada)
    b = p.bm('Dark#cut', 0.0)
    for k in range(N):
        a0, a1 = ang[k], ang[(k + 1) % N]
        q = [(R_T * math.cos(a0), R_T * math.sin(a0), rim_z(a0)), ((R_T - 0.03) * math.cos(a0), (R_T - 0.03) * math.sin(a0), rim_z(a0)),
             ((R_T - 0.03) * math.cos(a1), (R_T - 0.03) * math.sin(a1), rim_z(a1)), (R_T * math.cos(a1), R_T * math.sin(a1), rim_z(a1))]
        b.faces.new([b.verts.new(V(*v)) for v in q])
    # as costuras: as 2 cintas de rebites (o anel em relevo) e as costuras de pé com os rebites
    for z in (Z_B + 0.85, Z_B + 1.7):
        for k in range(N):
            a0, a1 = ang[k], ang[(k + 1) % N]
            if min(rim_z(a0), rim_z(a1)) < z + 0.06:
                continue
            p.obox('Rust#seam', ((R_T + 0.012) * math.cos((a0 + a1) / 2), (R_T + 0.012) * math.sin((a0 + a1) / 2), z),
                   (0.13, 0.03, 0.09), yaw=math.degrees((a0 + a1) / 2) + 90, bevel=0.0)
    for k in range(12):
        a = 2 * math.pi * k / 12
        zt = rim_z(a)
        for z in splits(Z_B + 0.1, zt - 0.12, 0.2):
            p.cyl('Rust#rivet', ((R_T + 0.01) * math.cos(a), (R_T + 0.01) * math.sin(a), z), (math.cos(a), math.sin(a), 0.0),
                  0.028, 0.03, seg=6, bevel=0.0)
    # o balcão: a grade em anel, as mãos-francesas, o guarda-corpo (os montantes e os 2 corrimãos), o rodapé
    disc_ring(p, 'Metal#grate', (0.0, 0.0), BAL_R, R_T - 0.02, Z_B - 0.06, Z_B, seg=48)
    for k in range(48):
        a = 2 * math.pi * k / 48
        if k % 2 == 0:
            p.obox('Dark#grid', ((R_T + BAL_R) / 2 * math.cos(a), (R_T + BAL_R) / 2 * math.sin(a), Z_B + 0.004),
                   (BAL_R - R_T, 0.03, 0.012), yaw=math.degrees(a), bevel=0.0)
    for k in range(12):
        a = 2 * math.pi * k / 12 + math.pi / 12
        rod(p, 'Metal#brace', (R_T * math.cos(a), R_T * math.sin(a), Z_B - 0.75), (BAL_R * math.cos(a), BAL_R * math.sin(a), Z_B - 0.05),
            0.03, seg=5)
        rod(p, 'Metal#brace', ((R_T + 0.02) * math.cos(a), (R_T + 0.02) * math.sin(a), Z_B - 0.06),
            (BAL_R * math.cos(a), BAL_R * math.sin(a), Z_B - 0.06), 0.035, seg=5)
    for k in range(24):
        a = 2 * math.pi * k / 24
        if abs(math.degrees(a) - 315) < 6:
            continue                                         # a boca da escada
        rr = BAL_R - 0.05
        p.cyl('Metal#rail', (rr * math.cos(a), rr * math.sin(a), Z_B + 0.5), 'z', 0.025, 1.0, seg=6, bevel=0.0)
    for z in (Z_B + 1.0, Z_B + 0.52):
        p.torus('Metal#rail', (0.0, 0.0, z), 'z', BAL_R - 0.05, 0.025, seg=48, rseg=5)
    disc_ring(p, 'Rust#toe', (0.0, 0.0), BAL_R - 0.03, BAL_R - 0.06, Z_B, Z_B + 0.12, seg=48)
    # a pichação verde no costado (o lado da câmera) e o anel verde pintado à mão
    tag(CylFrame(p, R_T + 0.012, math.radians(-62)), 0.0, 0.0, Z_B + 1.2, s=0.95, seed=4)
    tag(CylFrame(p, R_T + 0.012, math.radians(48)), 0.0, 0.0, Z_B + 1.25, s=0.7, seed=9)
    for k in range(N):
        a0, a1 = ang[k], ang[(k + 1) % N]
        z = Z_B + 0.3 + 0.03 * math.sin(a0 * 5)
        p.obox('Player#ring', ((R_T + 0.008) * math.cos((a0 + a1) / 2), (R_T + 0.008) * math.sin((a0 + a1) / 2), z),
               (0.135, 0.02, 0.16 + 0.04 * fbm(a0 * 4, 1.0, 3)), yaw=math.degrees((a0 + a1) / 2) + 90, bevel=0.0)


class CylFrame:
    """O quadro do costado pro rabisco (o tag): x = o arco a partir do ângulo a0, y = o afastamento pra fora, z = a altura."""

    def __init__(self, p, r, a0):
        self.p, self.r, self.a0 = p, r, a0

    def box(self, m, c, size, yaw=0.0, pitch=0.0, roll=0.0, bevel=None):
        a = self.a0 + c[0] / self.r
        rr = self.r + c[1]
        self.p.obox(m, (rr * math.cos(a), rr * math.sin(a), c[2]), size, yaw=math.degrees(a) + 90 + yaw, pitch=pitch, roll=roll,
                    bevel=bevel if bevel is not None else 0.0)


def ladders(p):
    """A escada de marinheiro pela perna da frente (a da câmera): os 2 montantes junto da perna, os degraus, a gaiola de aros; e a do
    costado, do balcão até a borda cortada, com o gancho por cima."""
    sx, sy = 1, -1
    radial = Vector((sx, sy, 0.0)).normalized()
    tang = Vector((1.0, 1.0, 0.0)).normalized()
    a0, a1 = leg_pt(sx, sy, 0.02), leg_pt(sx, sy, 0.99)
    a0 = a0 + radial * 0.55
    a1 = a1 + radial * 0.55
    d = (a1 - a0).normalized()
    for s in (-1, 1):
        rod(p, 'Metal#ladder', tuple(a0 + tang * s * 0.22), tuple(a1 + tang * s * 0.22), 0.025, seg=5)
    L = (a1 - a0).length
    for k in range(int(L / 0.32)):
        c = a0 + d * (0.2 + k * 0.32)
        rod(p, 'Metal#rung', tuple(c - tang * 0.22), tuple(c + tang * 0.22), 0.016, seg=4)
    # a gaiola (os aros de meia-volta pra fora e as 3 barras de pé)
    out = radial
    for k in range(7):
        c = a0 + d * (2.0 + k * 0.62)
        pts = [tuple(c + tang * (0.36 * math.cos(t)) + out * (0.4 * math.sin(t))) for t in [math.pi * q / 8 for q in range(9)]]
        tube(p, 'Metal#cage', pts, 0.018, seg=4, smooth=False)
    for t in (math.pi * 0.25, math.pi * 0.5, math.pi * 0.75):
        o = tang * (0.36 * math.cos(t)) + out * (0.4 * math.sin(t))
        rod(p, 'Metal#cage', tuple(a0 + d * 2.0 + o), tuple(a0 + d * (2.0 + 6 * 0.62) + o), 0.016, seg=4)
    # a escada do costado: do balcão (no ângulo −45°) até a borda, com o gancho
    a = math.radians(-45.0)
    rr = R_T + 0.2
    zt = rim_z(a) + 0.15
    for s in (-1, 1):
        off = Vector((-math.sin(a), math.cos(a), 0.0)) * (s * 0.22)
        b0 = Vector((rr * math.cos(a), rr * math.sin(a), Z_B)) + off
        b1 = Vector((rr * math.cos(a), rr * math.sin(a), zt)) + off
        rod(p, 'Metal#ladder', tuple(b0), tuple(b1), 0.025, seg=5)
        tube(p, 'Metal#ladder', [tuple(b1), tuple(b1 + Vector((0, 0, 0.18))), tuple(Vector(((R_T - 0.15) * math.cos(a), (R_T - 0.15) * math.sin(a),
                                                                                           zt + 0.18)) + off)], 0.025, seg=5, smooth=False)
    for z in splits(Z_B + 0.25, zt - 0.1, 0.3):
        c = Vector((rr * math.cos(a), rr * math.sin(a), z))
        off = Vector((-math.sin(a), math.cos(a), 0.0)) * 0.22
        rod(p, 'Metal#rung', tuple(c - off), tuple(c + off), 0.016, seg=4)


def nest(p):
    """O ninho dentro do tanque: o piso de tábuas, o anel de sacos de areia encostado no costado (o topo aparece por cima dos cortes
    baixos), os tubos de reserva encostados, a caixa de munição aberta, a lâmpada pendurada, o guincho de mão com a corda descendo até o
    caixote que sobe, a bandeira no guarda-corpo."""
    # o piso: as tábuas cortadas no círculo
    for k, y in enumerate(splits(-1.85, 1.85, 0.24)):
        h = math.sqrt(max(0.0, (R_T - 0.06) ** 2 - y * y))
        if h < 0.2:
            continue
        p.box('Wood#floor', -h, h, y - 0.11, y + 0.11, FLOOR - 0.06, FLOOR + 0.02 * _hash(k, 1, 3), bevel=0.0)
    # os sacos de areia (4 fiadas, a boca da escada aberta em −45°)
    bag_ring(p, (0.0, 0.0), R_T - 0.27, FLOOR + 0.02, layers=4, gaps=((300.0, 330.0),), bag_size=(0.62, 0.34, 0.24), seed=3,
             lean=0.02)
    # os tubos de reserva encostados nos sacos (o verde-oliva, a faixa verde)
    for k, (rr, zz) in enumerate(((1.22, 0.1), (1.4, 0.1), (1.31, 0.26))):
        ar = math.radians(165.0)
        cc = Vector((rr * math.cos(ar), rr * math.sin(ar), FLOOR + zz))
        tg = Vector((-math.sin(ar), math.cos(ar), 0.0)) * 0.8
        c0, c1 = tuple(cc - tg), tuple(cc + tg)
        d = Vector(c1) - Vector(c0)
        p.cyl('Crate#tube', tuple((Vector(c0) + Vector(c1)) / 2), tuple(d), TUBE_R, d.length, seg=12, bevel=0.0)
        p.cyl('Player#tubeband', tuple(Vector(c0) + d * 0.7), tuple(d), TUBE_R + 0.01, 0.12, seg=12, bevel=0.0)
        p.cyl('Dark#cap', tuple(Vector(c1)), tuple(d), TUBE_R + 0.02, 0.08, seg=12, bevel=0.0)
    crate(p, -1.05, -0.6, FLOOR, size=(1.0, 0.45, 0.35), yaw=70)
    crate(p, 0.95, 0.85, FLOOR, size=(0.8, 0.45, 0.32), yaw=-30)
    # a lâmpada pendurada num arame atravessado
    rod(p, 'Dark#wire', (-1.5, -1.05, 8.2), (1.45, 1.1, 8.25), 0.01, seg=4)
    p.cyl('Light#bulb', (-0.4, -0.28, 7.98), 'z', 0.07, 0.14, seg=10, bevel=0.0)
    # o guincho: o braço de cano curvo na borda da frente-direita, a roldana, a corda até o caixote que sobe
    a = math.radians(25.0)
    base_ = ((R_T - 0.1) * math.cos(a), (R_T - 0.1) * math.sin(a), FLOOR + 0.1)
    top_ = ((R_T + 0.75) * math.cos(a), (R_T + 0.75) * math.sin(a), rim_z(a) + 0.95)
    tube(p, 'Metal#davit', [base_, (base_[0], base_[1], rim_z(a) + 0.85), ((R_T + 0.25) * math.cos(a), (R_T + 0.25) * math.sin(a),
                                                                           rim_z(a) + 1.0), top_], 0.05, seg=8, smooth=False)
    p.cyl('Dark#pulley', top_, (-math.sin(a), math.cos(a), 0.0), 0.12, 0.06, seg=12, bevel=0.0)
    hang = (top_[0], top_[1], 3.6)
    rod(p, 'Wood#hoist', (top_[0], top_[1], top_[2] - 0.1), hang, 0.014, seg=4)
    crate(p, top_[0], top_[1], hang[2] - 0.62, size=(1.3, 0.42, 0.42), yaw=math.degrees(a) + 90)
    for s in (-1, 1):
        rod(p, 'Wood#hoist', hang, (top_[0] + s * 0.45 * math.sin(a), top_[1] - s * 0.45 * math.cos(a), hang[2] - 0.2), 0.012, seg=4)
    # a bandeira no guarda-corpo de trás-direita
    a = math.radians(125.0)
    x, y = (BAL_R - 0.05) * math.cos(a), (BAL_R - 0.05) * math.sin(a)
    p.cyl('Wood#pole', (x, y, Z_B + 1.6), 'z', 0.045, 3.2, seg=8, bevel=0.0)
    flag(p, (x, y), Z_B + 3.15, 1.3, 0.85, m='Player', phase=0.7)


def nets(p):
    """A rede de camuflagem enrolada nas pernas (o rolo grosso amarrado em espiral em 2 pernas, com os trapos) e o pedaço pendurado do
    balcão entre as pernas de trás."""
    for (sx, sy, t0, t1, ph) in ((-1, -1, 0.12, 0.42, 0.0), (1, 1, 0.2, 0.5, 1.7)):
        pts = []
        for k in range(40):
            t = t0 + (t1 - t0) * k / 39
            c = leg_pt(sx, sy, t)
            d = (leg_pt(sx, sy, 1.0) - leg_pt(sx, sy, 0.0)).normalized()
            e1 = Vector((sx, -sy, 0.0)).normalized()
            e2 = d.cross(e1).normalized()
            th = ph + 2 * math.pi * 2.2 * k / 39
            pts.append(tuple(c + (e1 * math.cos(th) + e2 * math.sin(th)) * 0.36))
        tube(p, 'Net#roll', pts, 0.17, seg=8, smooth=True)
        for k in range(0, 40, 3):
            q = Vector(pts[k])
            p.obox('Cloth#tuft', tuple(q + Vector((0, 0, 0.12))), (0.32, 0.12, 0.05), yaw=40 * k, pitch=20, bevel=0.0)
        # as amarras (a corda nas pontas do rolo)
        for k in (2, 37):
            p.torus('Wood#rope', pts[k], 'z', 0.2, 0.02, seg=10, rseg=4)
    # o pedaço pendurado do balcão (entre as pernas de trás, a face −X), ondulando
    a0, a1 = math.radians(155.0), math.radians(205.0)

    def fn(u, v):
        a = a0 + (a1 - a0) * u
        r = BAL_R + 0.02 + 0.25 * v
        return (r * math.cos(a), r * math.sin(a), Z_B + 0.9 - 2.6 * v + 0.15 * math.sin(u * 9.0) * v)
    net_surface(p, fn, 14, 10, seed=5)


def gunner(p, c, yaw, seated=True, aim=(0.0, 0.0)):
    """O atirador (só o volume, de roupa e lenço: nenhum rosto): o tronco com o colete, a cabeça enrolada no lenço verde (a cor do
    jogador), os braços pras manoplas, as pernas sentadas, as botas."""
    F = Frame(p, c, yaw=yaw)
    hip = (0.0, 0.0, 0.48 if seated else 0.95)
    sh = (0.04, 0.0, hip[2] + 0.5)
    F.box('Crate#vest', (0.02, 0.0, hip[2] + 0.27), (0.24, 0.36, 0.42), pitch=-8, bevel=0.06)
    F.cyl('Cloth#body', (0.0, 0.0, hip[2] + 0.22), 'z', 0.15, 0.5, seg=10, bevel=0.0, smooth=True)
    F.cyl('Cloth#neck', (0.05, 0.0, sh[2] + 0.08), 'z', 0.06, 0.12, seg=8, bevel=0.0)
    hc = F.pt(0.07, 0.0, sh[2] + 0.24)
    revolve(p, 'Player#scarf', (hc[0], hc[1]), [(0.01, hc[2] - 0.12), (0.1, hc[2] - 0.1), (0.115, hc[2]), (0.1, hc[2] + 0.08),
                                                (0.05, hc[2] + 0.12), (0.01, hc[2] + 0.125)], seg=12)
    F.box('Dark#goggles', (0.16, 0.0, sh[2] + 0.25), (0.04, 0.14, 0.05), bevel=0.0)
    for s in (-1, 1):
        a = (0.04, s * 0.19, sh[2] - 0.02)
        e = (0.26, s * 0.24, sh[2] - 0.2)
        w = (aim[0] + 0.45, s * 0.2, sh[2] - 0.05 + aim[1])
        F.rod('Cloth#arm', a, e, 0.055)
        F.rod('Cloth#arm', e, w, 0.045)
        if seated:
            k = (0.42, s * 0.11, hip[2] + 0.05)
            f = (0.48, s * 0.12, 0.08)
            F.rod('Cloth#leg', (0.0, s * 0.1, hip[2]), k, 0.075)
            F.rod('Cloth#leg', k, f, 0.06)
            F.box('Dark#boot', (0.55, s * 0.12, 0.06), (0.26, 0.11, 0.12), bevel=0.02)


def turret():
    """A torre (gira em (0, 0)): a mesa giratória no piso do ninho, o pedestal, o garfo com os munhões, os 3 tubos do Stinger (o verde-
    oliva, as tampas escuras, a faixa verde), a antena do IFF em leque, a mira, o assento de trator, o atirador sentado nas manoplas."""
    p = new_part(PREFIX + '_Turret')
    p.cyl('Dark', (0.0, 0.0, TT + 0.035), 'z', 0.42, 0.07, seg=24, bevel=0.0)
    p.cyl('Metal', (0.0, 0.0, TT + 0.12), 'z', 0.36, 0.1, seg=24, bevel=0.0)
    p.cyl('Metal#post', (0.0, 0.0, (TT + TRUNNION[2]) / 2), 'z', 0.09, TRUNNION[2] - TT - 0.1, seg=12, bevel=0.0)
    for s in (-1, 1):
        p.prism_xz('Metal', [(-0.12, TRUNNION[2] - 0.35), (0.22, TRUNNION[2] - 0.35), (TRUNNION[0] + 0.09, TRUNNION[2] + 0.06),
                             (TRUNNION[0] - 0.09, TRUNNION[2] + 0.06)], s * 0.26, s * 0.31, bevel=0.0)
        p.cyl('Dark', (TRUNNION[0], s * 0.33, TRUNNION[2]), 'y', 0.06, 0.05, seg=12, bevel=0.0)
    p.box('Metal#yoke', -0.12, 0.22, -0.31, 0.31, TRUNNION[2] - 0.42, TRUNNION[2] - 0.34, bevel=0.0)
    e = math.radians(EL)
    d = (math.cos(e), 0.0, math.sin(e))
    up = (-math.sin(e), 0.0, math.cos(e))
    for t in (-0.4, 0.45):
        c = (TRUNNION[0] + d[0] * t, 0.0, TRUNNION[2] + d[2] * t)
        p.obox('Dark#clamp', c, (0.07, 0.42, 0.42), pitch=EL, bevel=0.0)
    for (y, z) in TUBES:
        c = (TRUNNION[0] + up[0] * z + d[0] * 0.25, y, TRUNNION[2] + up[2] * z + d[2] * 0.25)
        p.cyl('Crate', c, d, TUBE_R, TUBE_L, seg=16, bevel=0.0)
        f = (c[0] + d[0] * TUBE_L / 2, c[1], c[2] + d[2] * TUBE_L / 2)
        b = (c[0] - d[0] * TUBE_L / 2, c[1], c[2] - d[2] * TUBE_L / 2)
        p.cyl('Dark', f, d, TUBE_R + 0.02, 0.1, seg=16, bevel=0.0)
        p.cyl('Dark', b, d, TUBE_R + 0.015, 0.09, seg=16, bevel=0.0)
        q = (c[0] + d[0] * 0.3, c[1], c[2] + d[2] * 0.3)
        p.cyl('Player', q, d, TUBE_R + 0.01, 0.14, seg=16, bevel=0.0)
    # a antena do IFF (o leque de arame do Stinger) do lado esquerdo e a mira de quadro
    ac = (TRUNNION[0] + d[0] * 0.35 + up[0] * 0.1, -0.36, TRUNNION[2] + d[2] * 0.35 + up[2] * 0.1)
    for k in range(5):
        t = math.radians(-60 + 30 * k)
        tip = (ac[0] + 0.3 * math.cos(t) * d[0] - 0.3 * math.sin(t) * up[0], ac[1] - 0.12, ac[2] + 0.3 * math.cos(t) * d[2]
               - 0.3 * math.sin(t) * up[2])
        rod(p, 'Dark#iff', ac, tip, 0.01, seg=4)
    tube(p, 'Dark#iff', [(ac[0] + 0.3 * math.cos(math.radians(a)) * d[0] - 0.3 * math.sin(math.radians(a)) * up[0], ac[1] - 0.12,
                          ac[2] + 0.3 * math.cos(math.radians(a)) * d[2] - 0.3 * math.sin(math.radians(a)) * up[2])
                         for a in range(-60, 61, 15)], 0.01, seg=4, smooth=False)
    # o assento de trator atrás e o atirador
    p.cyl('Metal#seatpost', (-0.55, 0.0, TT + 0.25), 'z', 0.04, 0.42, seg=8, bevel=0.0)
    p.cyl('Rust#seat', (-0.55, 0.0, TT + 0.48), 'z', 0.2, 0.05, seg=14, bevel=0.0)
    rod(p, 'Metal#arm', (-0.1, 0.0, TT + 0.35), (-0.55, 0.0, TT + 0.3), 0.035, seg=6)
    gunner(p, (-0.62, 0.0, TT), 0.0, seated=True, aim=(0.2, 0.2))
    return finish(p)


def muzzle():
    e = math.radians(EL)
    d = (math.cos(e), 0.0, math.sin(e))
    up = (-math.sin(e), 0.0, math.cos(e))
    y, z = TUBES[2]
    c = (TRUNNION[0] + up[0] * z + d[0] * 0.25, y, TRUNNION[2] + up[2] * z + d[2] * 0.25)
    return (c[0] + d[0] * TUBE_L / 2, c[1], c[2] + d[2] * TUBE_L / 2)


def base():
    p = new_part(PREFIX + '_Base')
    for m in ('Wood#pole', 'Wood#rope', 'Rust#inner', 'Rust#tank', 'GlaPaint#tank'):
        p.bm(m, 0.0)
    p.bm('Plaster#stone', 0.03)
    # o chão batido em volta do pé da torre (o contorno redondo, sem laje) e os caixotes de míssil
    earth_pad(p, (0.0, 0.0), blob(0.0, 0.0, 3.85, 3.85, seed=6, amp=0.05), h=0.05)
    tower(p)
    tank(p)
    ladders(p)
    nest(p)
    nets(p)
    # os caixotes de míssil no pé (os compridos verde-oliva com as faixas, um aberto com os tubos), os tambores
    for k, (x, y, z, yaw) in enumerate(((-1.2, 1.6, 0.0, 12), (-1.15, 1.62, 0.42, 8), (-0.2, 2.25, 0.0, -78), (0.9, -1.9, 0.0, 30))):
        crate(p, x, y, z, size=(1.6, 0.48, 0.42), yaw=yaw)
        p.obox('Player#stencil', (x, y, z + 0.425), (0.5, 0.2, 0.01), yaw=yaw, bevel=0.0)
    crate(p, 1.6, 0.4, 0.0, size=(1.5, 0.6, 0.3), yaw=95)
    for k in range(3):
        p.cyl('Crate#tube', (1.6 + (k - 1) * 0.18, 0.4, 0.38), 'y', TUBE_R, 1.35, seg=12, bevel=0.0)
    barrel(p, -2.0, -0.9, 0.0, r=0.28, h=0.86, m='Rust')
    barrel(p, -1.55, -1.25, 0.0, r=0.28, h=0.86, m='Metal')
    return finish(p)


def write_preview(out):
    import json
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]], [PREFIX + '_Turret', [0, 0, 0]]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1200, 1200]], ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1000, 1400]],
                      ['side_left', 'ortho', [0, -1, 0], [0, 0, 1], [1000, 1400]],
                      ['three_quarter', 'persp', 30.0, 215.0, [1200, 1400], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 30.0, 35.0, [1200, 1400], 30.0, 0.95]],
            'usa_at': [7.0, -4.5, 0.0], 'tank_views': True,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(out, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, indent=1)


def main():
    out = out_dir('gla_stinger_site_v3')
    clear_scene()
    total = export(base(), out)
    clear_scene()
    total += export(turret(), out)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -LEG_FOOT, LEG_FOOT, -LEG_FOOT, LEG_FOOT, Z_TOP, post='Wood', plank='Crate',
                             stripe='Hazard'), out)
    write_preview(out)
    m = muzzle()
    print('MODEL total', total, 'tris')
    print('MODEL lote 8 x 8 m; turret_pivot_m (0, 0); a mesa no piso do ninho z', round(TT, 2), '; muzzle (x, y, z) =',
          tuple(round(v, 2) for v in m), '; muzzle_height_m', round(m[2], 2), '; eixo de elevacao x', TRUNNION[0], 'z', TRUNNION[2],
          '; elevacao em repouso', EL)
    print('MODEL fire_points_m', [(0.0, 0.0, round(Z_B + 1.0, 2)), (1.6, 1.6, 2.5), (-1.6, -1.6, 2.5)])


if __name__ == '__main__':
    main()
