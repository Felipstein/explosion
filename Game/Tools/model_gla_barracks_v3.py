"""O Quartel da GLA (Barracks), v3 (02/11/2026): O CEMITÉRIO DE ÔNIBUS. Do zero, pelo veredito de 02/11 (docs/elenco/IDEIAS_NOVAS_02_11.md:
"quero ideias do 0, diferente, novas, bonitas, bem feitas, criativas"). O v1 (o pátio murado) e o v2 (a tenda beduína) ficam como estão.

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_gla_barracks_v3.py C:/Users/luisf/explosion/captures/game/models/gla_barracks_v3

A palavra (o que lê de cima): TRÊS ÔNIBUS VELHOS EM LEQUE. Tudo o que a GLA tem é reaproveitado (a Technical é picape; o quartel é ônibus):
o escolar amarelo no meio, apontado pra frente (+X), e os 2 de linha (o branco com a faixa turquesa, o bege com a faixa de ferrugem) abertos
em leque a partir de trás, os 3 com a tinta descascando em ferrugem, as janelas tapadas uma a uma (a chapa rebitada, o tapete pendurado, as
tábuas, uns vidros que sobraram), o L sem as rodas de trás (o eixo nos blocos), os murchos afundando. Entre eles as LONAS esticadas de teto a
teto (a verde, a cor do jogador; a crua remendada), com a vara e a corda na beira da frente. No teto do R as camas (os colchões, os cobertores
verdes) com a escada de corda descendo; no escolar a caixa d'água preta, as malas, o fardo e o mastro com a bandeira. Na frente, o pátio: a
fogueira acesa com o tripé e a panela, os bancos de pneu e caixote, os tapetes, o varal com as roupas, o cavalete dos fuzis. Os nomes e os
rabiscos verdes pintados à mão nos lados que a câmera vê. De cima: os 3 retângulos compridos de teto redondo em leque (o amarelo, o branco, o
bege) com as escotilhas, a carga e as camas, e as 2 lonas fechando as cunhas: nenhum prédio do jogo tem isso.

Lote 18 × 22 m (X × Y), frente em +X. A infantaria sai pela porta sanfonada do escolar (o lado +Y, junto do capô; a porta aberta com o
degrau) e anda pela faixa livre do meio (Y −3,3..3,3) da frente do capô até a borda +X. Sem laje: o chão batido acompanha o acampamento.

Peças (origem no chão, no centro do lote; Unreal X frente, Y direita, Z cima, metros):
  SM_GLA_Barracks_Base: tudo (a bandeira sai no flags.json; o jogo desenha o pano no vento);
  SM_GLA_Barracks_Scaffold: o andaime da obra em madeira.
Materiais (slots): Yellow (a tinta do escolar), Paint (a do ônibus branco), GlaPaint (a do bege), Rust (a ferrugem onde a tinta
  descascou, as chapas, os tapetes vermelhos), Tile (a faixa turquesa), Glass, Dark, Metal, Rubber, Player (o verde: a lona, os rabiscos,
  os cobertores, a bandeira), Cloth, Wood, Crate, Plaster (as pedras e os blocos), Earth, Fire (a brasa), Beacon (as luzes do escolar),
  Hazard (só no andaime).
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


def rifle(p, base, top, side=(0.0, 1.0)):
    """O fuzil (o AK de cabo de madeira) encostado: a coronha, o corpo, o carregador curvo, o cano (o do quartel v2)."""
    b, t = Vector(base), Vector(top)
    d = (t - b).normalized()
    L = (t - b).length
    q = lambda f: tuple(b + d * (L * f))
    rod(p, 'Wood#gun', q(0.0), q(0.3), 0.035, seg=5)
    rod(p, 'Dark#gun', q(0.3), q(0.62), 0.032, seg=5)
    rod(p, 'Wood#gun', q(0.62), q(0.78), 0.028, seg=5)
    rod(p, 'Dark#gun', q(0.78), q(1.0), 0.012, seg=4)
    m0 = Vector(q(0.5))
    rod(p, 'Dark#gun', tuple(m0), (m0.x + side[0] * 0.18, m0.y + side[1] * 0.18, m0.z - 0.08), 0.022, seg=4)


def rack(p, x, y0, y1):
    """O cavalete das armas: os 2 cavaletes em A, as 2 travessas, os fuzis encostados dos 2 lados, as caixas de munição."""
    for y in (y0, y1):
        for s in (-1, 1):
            rod(p, 'Wood#rack', (x + s * 0.45, y, 0.0), (x, y, 1.15), 0.04, seg=6)
    for z in (1.08, 0.45):
        rod(p, 'Wood#rack', (x, y0 - 0.1, z), (x, y1 + 0.1, z), 0.035, seg=6)
    for k, y in enumerate(splits(y0 + 0.2, y1 - 0.2, 0.32)):
        s = 1 if k % 2 == 0 else -1
        rifle(p, (x + s * 0.5, y, 0.02), (x + s * 0.06, y + 0.03, 1.12), side=(s, 0.0))
    crate(p, x - 0.95, (y0 + y1) / 2, 0.0, size=(0.9, 0.5, 0.4), yaw=82)
    crate(p, x - 0.9, (y0 + y1) / 2 + 0.05, 0.4, size=(0.8, 0.45, 0.36), yaw=96)


PREFIX = 'SM_GLA_Barracks'
HX, HY = 9.0, 11.0
LANE = 3.3                          # a meia-largura da faixa livre da saída (Y −3,3..3,3, da porta até a frente)

# os 3 ônibus: (tipo, origem do centro no chão (x, y), yaw, tinta, faixa, semente, roll, pneus murchos)
BUSES = {
    'M': ('school', (-2.75, 0.55), 0.0, 'Yellow', 'Dark', 3, -1.2, ('fl',)),
    'L': ('city', (-3.05, -5.75), -40.0, 'Paint', 'Tile', 7, 2.4, ('rl', 'rr')),
    'R': ('city', (-2.85, 6.25), 34.0, 'GlaPaint', 'Rust', 11, -2.0, ('fr',)),
}
SPEC = {
    # meia-comprimento, meia-largura, o pé da lataria, a cintura (o peitoril), o topo das janelas, a beira do teto, a coroa,
    # o raio da quina do teto, a frente da caixa (o capô fica na frente dela), os eixos, o vão das janelas
    'school': dict(hl=5.2, hw=1.2, zb=0.62, zbelt=1.66, zw=2.42, ze=2.6, zc=2.86, rr=0.26, xbody=3.5, axles=(3.95, -2.55),
                   bay=0.86, R=0.5),
    'city': dict(hl=5.8, hw=1.26, zb=0.42, zbelt=1.22, zw=2.48, ze=2.66, zc=2.92, rr=0.32, xbody=5.8, axles=(3.65, -2.25),
                 bay=1.12, R=0.5),
}


def bus_frame(p, key):
    kind, (x, y), yaw, *_rest = BUSES[key]
    roll = BUSES[key][6]
    return Frame(p, (x, y, 0.0), yaw=yaw, roll=roll)


def plan_hw(x, s, hw):
    """A meia-largura em planta (as quinas redondas da caixa nas 2 pontas, raio 0,3 m)."""
    rp = 0.3
    xe = s['xbody'] if x > 0 else -s['hl']
    t = (abs(x) - (abs(xe) - rp)) / rp
    if t <= 0:
        return hw
    t = min(t, 1.0)
    return hw - rp * (1.0 - math.sqrt(max(0.0, 1.0 - t * t)))


def stations(x0, x1, step=0.35):
    xs = splits(x0, x1, step)
    for e in (x0, x1):
        sgn = 1 if e == x0 else -1
        xs += [e + sgn * d for d in (0.02, 0.06, 0.12, 0.2)]
    return sorted(set(round(v, 4) for v in xs))


def bus(p, key):
    """O ônibus velho que virou alojamento: a caixa em 3 faixas (a lataria de baixo e o teto como lofts de seção redonda, com a tinta
    que descasca pra ferrugem; a faixa das janelas no meio, os vidros recuados com as colunas), as janelas tapadas uma a uma (a chapa
    rebitada, o tapete pendurado, as tábuas; poucas com o vidro), o para-choque, os faróis, as rodas (as murchas afundam), as escotilhas
    do teto, os frisos, a porta. O escolar tem o capô na frente (a silhueta que todo mundo conhece) e os frisos pretos."""
    kind, (ox, oy), yaw, paint, band, seed, roll, flats = BUSES[key]
    s = SPEC[kind]
    F = bus_frame(p, key)
    hl, hw, zb, zbelt, zw, ze, zc, rr = s['hl'], s['hw'], s['zb'], s['zbelt'], s['zw'], s['ze'], s['zc'], s['rr']
    xr, xf = -hl, s['xbody']
    to_w = lambda q: F.pt(*q)

    def peel(c, i, j, base=paint):
        """A ferrugem onde a tinta foi embora, nos lugares em que o ônibus velho enferruja de verdade (nunca mancha solta em xadrez,
        que lia camuflagem): a saia de baixo (a barra ondulada), a volta das caixas de roda e a calha do teto em trechos compridos. As
        listras escorridas debaixo dos peitoris são tiras finas à parte (Rust#streak)."""
        n = fbm(c.x * 1.5 + seed, 1.0 + c.y * 0.3, seed)
        if c.z < zb + 0.16 + 0.14 * n:
            return 'Rust#peel'
        for xa in s['axles']:
            if (c.x - xa) ** 2 + (c.z - s['R']) ** 2 < (s['R'] + 0.24 + 0.14 * n) ** 2:
                return 'Rust#peel'
        if c.z > zw and abs(c.y) > hw - 0.3 and fbm(c.x * 0.3 + seed, 0.5, seed + 5) > 0.45:
            return 'Rust#peel'                              # a calha do teto (a quina) enferrujada em trechos compridos
        return base + '#body'
    # a lataria de baixo (o pé redondo, a cintura reta), amassada de leve
    rings = []
    for x in stations(xr, xf):
        h = plan_hw(x, s, hw) * (1.0 - 0.012 * fbm(x * 1.3, 2.0, seed))
        rings.append([(x, y, z) for (y, z) in rrect(-h, h, zb, zbelt, rb=0.12, rt=0.0, n=3)])
    loft_mat(p, rings, peel, to_w)
    # o teto: a beira com a quina redonda e a coroa, das janelas pra cima (a faixa clara do teto do escolar é a mesma tinta)
    rings = []
    for x in stations(xr, xf):
        h = plan_hw(x, s, hw) - 0.03
        r_ = min(rr, h * 0.5)
        sec = [(h, zw)]
        for k in range(5):
            a = (math.pi / 2) * k / 4
            sec.append((h - r_ + r_ * math.cos(a), ze - r_ + r_ * math.sin(a)))
        for k in range(1, 6):
            yy = (h - r_) * (1 - 2 * k / 6)
            sec.append((yy, ze + (zc - ze) * (1 - (yy / max(h - r_, 1e-3)) ** 2)))
        for k in range(5):
            a = math.pi / 2 + (math.pi / 2) * k / 4
            sec.append((-h + r_ + r_ * math.cos(a), ze - r_ + r_ * math.sin(a)))
        sec.append((-h, zw))
        rings.append([(x, y, z) for (y, z) in sec])
    loft_mat(p, rings, lambda c, i, j: peel(c, i, j), to_w)
    # a faixa das janelas: o vidro recuado (o fundo escuro), as colunas, as janelas tapadas
    rings = []
    for x in stations(xr + 0.04, xf - 0.04):
        h = plan_hw(x, s, hw) - 0.075
        rings.append([(x, y, z) for (y, z) in rrect(-h, h, zbelt - 0.02, zw + 0.03, rb=0.02, rt=0.02, n=1)])
    loft_mat(p, rings, lambda c, i, j: 'Glass#win', to_w)
    nb = max(4, int(round((xf - xr - 0.9) / s['bay'])))
    xs = splits(xr + 0.35, xf - 0.45, (xf - xr - 0.8) / nb)
    for side in (-1, 1):
        y = side * (hw - 0.035)
        for k, x in enumerate(xs):
            F.box(paint + '#post', (x, y, (zbelt + zw) / 2), (0.13, 0.08, zw - zbelt + 0.02), bevel=0.0)
        # cada vão: a chapa rebitada, o tapete pendurado, as tábuas pregadas ou o vidro que sobrou
        for k, (a, b) in enumerate(zip(xs[:-1], xs[1:])):
            if kind == 'school' and side == 1 and b > xf - 1.3:
                continue                                    # a porta
            if kind == 'city' and side == 1 and (a > xf - 1.6 or abs((a + b) / 2 + 0.6) < 0.6):
                continue                                    # as 2 portas do ônibus de linha
            h = _hash(k, side + 5, seed)
            xc, wv = (a + b) / 2, b - a - 0.13
            yy = side * (hw - 0.02)
            if h < 0.42:
                F.box('Rust#sheet', (xc, yy, (zbelt + zw) / 2 + 0.02), (wv + 0.06, 0.025, zw - zbelt + 0.06), roll=side * 2.0,
                      pitch=(h - 0.2) * 8.0, bevel=0.0)
                for (dx, dz) in ((-0.42, -0.36), (0.42, -0.36), (-0.42, 0.36), (0.42, 0.36)):
                    F.cyl('Dark#rivet', (xc + dx * wv, yy + side * 0.015, (zbelt + zw) / 2 + dz * (zw - zbelt)), 'y', 0.025, 0.02,
                          seg=6, bevel=0.0)
            elif h < 0.66:
                rug_m = ('Player#rug', 'Rust#rug', 'Cloth#rug')[int(h * 97) % 3]
                drop = 0.25 + 0.2 * _hash(k, 3, seed)
                F.box(rug_m, (xc, yy + side * 0.035, zw - (zw - zbelt + drop) / 2), (wv, 0.02, zw - zbelt + drop), bevel=0.0)
                F.box('Cloth#rugb', (xc, yy + side * 0.047, zw - (zw - zbelt + drop) / 2), (wv - 0.16, 0.015,
                                                                                          zw - zbelt + drop - 0.16), bevel=0.0)
                for q in range(int(wv / 0.08)):
                    F.box('Cloth#fringe', (xc - wv / 2 + 0.04 + q * 0.08, yy + side * 0.035, zbelt - drop - 0.04),
                          (0.02, 0.012, 0.09), bevel=0.0)
            elif h < 0.82:
                for q in range(3):
                    zz = zbelt + 0.13 + q * (zw - zbelt - 0.2) / 2
                    F.box('Wood#plank', (xc, yy + side * 0.01, zz), (wv + 0.1, 0.03, 0.2), pitch=(_hash(k, q, 7) - 0.5) * 9,
                          bevel=0.0)
            else:
                F.box('Dark#broken', (xc, side * (hw - 0.06), (zbelt + zw) / 2), (wv - 0.05, 0.02, zw - zbelt - 0.08), bevel=0.0)
    # as listras de ferrugem escorrendo debaixo dos peitoris e das colunas (as tiras finas por cima da tinta)
    for side in (-1, 1):
        for k, x in enumerate(splits(xr + 0.5, xf - 0.5, 0.42)):
            h = _hash(k, side + 3, seed + 9)
            if h < 0.45:
                continue
            L = 0.18 + 0.6 * _hash(k, side + 7, seed)
            w = 0.04 + 0.05 * _hash(k, side + 8, seed)
            F.box('Rust#streak', (x + 0.1 * (h - 0.7), side * (hw + 0.005), zbelt - L / 2 - 0.02), (w, 0.012, L), bevel=0.0)
            F.box('Rust#streak', (x + 0.1 * (h - 0.7), side * (hw + 0.005), zbelt - L - 0.02), (w * 0.6, 0.012, 0.08), bevel=0.0)
    # os frisos: o escolar com as 3 faixas pretas de proteção, o de linha com a faixa da cor dele embaixo das janelas
    for side in (-1, 1):
        if kind == 'school':
            for z in (zb + 0.3, zb + 0.62, zbelt - 0.12):
                F.box('Dark#rail', ((xr + xf) / 2, side * (hw + 0.012), z), (xf - xr - 0.5, 0.03, 0.07), bevel=0.0)
            F.box('Dark#rail', ((xr + xf) / 2, side * (hw - 0.02), zw + 0.07), (xf - xr - 0.4, 0.05, 0.08), bevel=0.0)
        else:
            F.box(band + '#band', ((xr + xf) / 2, side * (hw + 0.006), zbelt - 0.2), (xf - xr - 0.7, 0.02, 0.22), bevel=0.0)
            F.box(band + '#band', ((xr + xf) / 2, side * (hw + 0.006), zb + 0.22), (xf - xr - 0.7, 0.02, 0.1), bevel=0.0)
    # as rodas (as murchas afundam; as que faltam: o eixo nos blocos de concreto)
    for ia, xa in enumerate(s['axles']):
        for side in (-1, 1):
            tag_ = ('f' if ia == 0 else 'r') + ('l' if side < 0 else 'r')
            c = F.pt(xa, side * (hw - 0.18), s['R'])
            ax = F.vec((0, 1, 0))
            if tag_ in flats and key == 'L':
                p.obox('Plaster#block', F.pt(xa, side * (hw - 0.4), 0.2), (0.4, 0.4, 0.4), yaw=BUSES[key][2] + 10, bevel=0.03)
                p.obox('Plaster#block', F.pt(xa + 0.1, side * (hw - 0.4), 0.52), (0.38, 0.38, 0.25), yaw=BUSES[key][2] - 5, bevel=0.03)
                continue
            wheel(p, c, ax, R=s['R'], w=0.3, rim=0.3, sunk=0.16 if tag_ in flats else 0.0)
            # a caixa da roda (o arco escuro na lataria)
            F.cyl('Dark#arch', (xa, side * (hw + 0.004), s['R'] + 0.04), 'y', s['R'] + 0.1, 0.02, seg=16, bevel=0.0)
    # a frente e a traseira
    if kind == 'school':
        hood(p, F, s, paint, seed)
    else:
        # o para-choque, os faróis, o para-brisa grande (o vidro recuado já é a frente da faixa), a caixa do letreiro, o limpador
        F.box('Dark#bumper', (xf + 0.08, 0.0, zb + 0.18), (0.2, 2 * hw - 0.1, 0.32), bevel=0.03)
        for side in (-1, 1):
            F.box('Glass#lamp', (xf + 0.01, side * (hw - 0.32), zb + 0.55), (0.04, 0.38, 0.18), bevel=0.0)
            F.box('Metal#lamprim', (xf + 0.005, side * (hw - 0.32), zb + 0.55), (0.03, 0.44, 0.24), bevel=0.0)
        F.box('Dark#sign', (xf - 0.02, 0.0, zw - 0.08), (0.06, 2 * hw - 0.5, 0.22), bevel=0.0)
        F.box('Glass#shield', (xf - 0.05, 0.0, (zbelt + zw) / 2 - 0.25), (0.03, 2 * hw - 0.35, zw - zbelt + 0.25), pitch=-8, bevel=0.0)
        F.box('Dark#wiper', (xf + 0.02, -0.3, zbelt - 0.1), (0.02, 0.04, 0.7), pitch=0, roll=40, bevel=0.0)
        # a traseira: a grade do motor
        for q in range(5):
            F.box('Dark#grille', (xr - 0.01, 0.0, zb + 0.25 + q * 0.12), (0.03, 1.6, 0.05), bevel=0.0)
    F.box('Dark#bumper', (xr - 0.08, 0.0, zb + 0.15), (0.18, 2 * hw - 0.1, 0.28), bevel=0.03)
    for side in (-1, 1):
        F.box('Rust#taillamp', (xr - 0.01, side * (hw - 0.2), zb + 0.6), (0.04, 0.16, 0.28), bevel=0.0)
    # o espelho do motorista e o da porta (o braço e o espelho)
    for side in (-1, 1):
        x0 = (xf + 0.15) if kind == 'city' else (xf + 0.3)
        a = (x0 - 0.15, side * hw, zbelt + 0.3)
        b = (x0 + 0.15, side * (hw + 0.35), zbelt + 0.5)
        F.rod('Dark#mirror', a, b, 0.02)
        F.box('Dark#mirror', (b[0], b[1], b[2] - 0.1), (0.05, 0.2, 0.32), bevel=0.0)
    # as escotilhas do teto (a moldura, a tampa entreaberta)
    for k, x in enumerate((xr + 2.6, xf - 2.8)):
        F.box('Metal#hatch', (x, 0.0, zc + 0.02), (0.75, 0.75, 0.08), bevel=0.0)
        F.box(paint + '#hatchlid', (x + 0.08, 0.0, zc + 0.12), (0.7, 0.7, 0.05), pitch=14 + 10 * k, bevel=0.0)
    # a porta (a da frente, do lado direito): o escolar (o do meio) com a porta sanfonada aberta e o degrau; o resto fechado
    side = 1
    if kind == 'school':
        xd0, xd1 = xf - 1.12, xf - 0.22
        F.box('Dark#door', ((xd0 + xd1) / 2, side * (hw - 0.03), (zb + zw) / 2 - 0.1), (xd1 - xd0, 0.08, zw - zb + 0.2), bevel=0.0)
        for q, xq in enumerate((xd0 + 0.08, xd1 - 0.08)):
            F.box('Glass#doorleaf', (xq, side * (hw + 0.22), (zb + zw) / 2), (0.05, 0.42, zw - zb - 0.15), yaw=(-1) ** q * 70, bevel=0.0)
            F.box(paint + '#doorframe', (xq, side * (hw + 0.22), zw - 0.05), (0.06, 0.44, 0.08), yaw=(-1) ** q * 70, bevel=0.0)
        for q in range(3):
            F.box('Metal#step', ((xd0 + xd1) / 2, side * (hw - 0.25 + 0.12 * (2 - q)), zb - 0.12 + q * 0.28),
                  (xd1 - xd0 - 0.1, 0.32, 0.06), bevel=0.0)
        F.box('Metal#step', ((xd0 + xd1) / 2, side * (hw + 0.45), 0.08), (1.1, 0.7, 0.16), yaw=4, bevel=0.0)
    else:
        for (xd0, xd1) in ((xf - 1.45, xf - 0.35), (-1.2, 0.0)):
            F.box('Glass#door', ((xd0 + xd1) / 2, side * (hw + 0.004), (zb + zw) / 2), (xd1 - xd0, 0.02, zw - zb - 0.1), bevel=0.0)
            F.box(paint + '#doorframe', ((xd0 + xd1) / 2, side * (hw + 0.012), (zb + zw) / 2), (0.06, 0.03, zw - zb - 0.1), bevel=0.0)
    return F


def hood(p, F, s, paint, seed):
    """O capô do escolar (a frente comprida do ônibus americano): a caixa do motor de topo redondo, os paralamas redondos sobre as
    rodas, a grade com as barras, os faróis redondos, o para-choque preto; o para-brisa em 2 vidros na frente da caixa."""
    xf, hl, zb = s['xbody'], s['hl'], s['zb']
    rings = []
    for x in stations(xf - 0.05, hl - 0.05, 0.25):
        t = min(1.0, max(0.0, (x - xf) / (hl - xf)))
        zt = 1.62 - 0.14 * t ** 1.5
        h = 0.86 - 0.08 * t ** 2
        rings.append([(x, y, z) for (y, z) in rrect(-h, h, 0.62, zt, rb=0.05, rt=0.26, n=3)])
    loft_mat(p, rings, lambda c, i, j: ('Rust#peel' if fbm(c.x * 1.4 + 2.0, c.z * 2.0 + c.y, seed + 4) > 0.8 else paint + '#hood'),
             lambda q: F.pt(*q))
    # os paralamas (o meio-cilindro sobre a roda, pra fora do capô)
    xa = s['axles'][0]
    for side in (-1, 1):
        rings = []
        for k in range(9):
            a = math.pi * k / 8
            x = xa + 0.68 * math.cos(a)
            z = s['R'] + 0.05 + 0.62 * math.sin(a)
            rings.append([(x, side * yy, z) for yy in (0.62, 1.16)] + [(x, side * 1.16, z - 0.16), (x, side * 0.62, z - 0.16)])
        F.p.loft(paint + '#fender', [[F.pt(*q) for q in r] for r in rings], bevel=0.0)
    # a grade e as barras, os faróis redondos, o para-choque
    F.box('Dark#grille', (hl - 0.02, 0.0, 1.05), (0.06, 1.1, 0.75), bevel=0.0)
    for k in range(7):
        F.box('Metal#bar', (hl + 0.01, -0.45 + k * 0.15, 1.05), (0.03, 0.04, 0.72), bevel=0.0)
    for side in (-1, 1):
        F.cyl('Metal#lamprim', (hl - 0.02, side * 0.72, 1.18), 'x', 0.15, 0.08, seg=14, bevel=0.0)
        F.cyl('Glass#lamp', (hl + 0.02, side * 0.72, 1.18), 'x', 0.11, 0.03, seg=14, bevel=0.0)
    F.box('Dark#bumper', (hl + 0.08, 0.0, 0.62), (0.2, 2.3, 0.26), bevel=0.03)
    # o para-brisa em 2 vidros (a coluna no meio) e o letreiro apagado em cima
    F.box(paint + '#post', (xf + 0.03, 0.0, (s['zbelt'] + s['zw']) / 2), (0.06, 0.1, s['zw'] - s['zbelt']), bevel=0.0)
    F.box('Dark#sign', (xf + 0.02, 0.0, s['zw'] + 0.08), (0.05, 1.3, 0.16), bevel=0.0)
    for side in (-1, 1):
        F.box('Beacon#warn', (xf + 0.03, side * 0.85, s['zw'] + 0.1), (0.05, 0.16, 0.12), bevel=0.0)


def roof_load(p):
    """O que mora nos tetos: as camas no teto do R (os colchões, o cobertor verde, o travesseiro, a grade do bagageiro) com a escada de
    corda descendo pelo lado da câmera; a caixa d'água preta e as malas no M; os fardos amarrados no L."""
    s = SPEC['city']
    F = bus_frame(p, 'R')
    z = s['zc'] + 0.02
    for side in (-1, 1):
        F.rod('Metal#rack', (-4.6, side * 1.0, z + 0.18), (2.2, side * 1.0, z + 0.18), 0.025)
    for x in splits(-4.6, 2.2, 1.7):
        for side in (-1, 1):
            F.rod('Metal#rack', (x, side * 1.0, z - 0.02), (x, side * 1.0, z + 0.2), 0.022)
        F.rod('Metal#rack', (x, -1.0, z + 0.18), (x, 1.0, z + 0.18), 0.02)
    for k, (x, yy, ya) in enumerate(((-3.6, -0.42, 2.0), (-3.4, 0.45, -3.0), (-1.3, -0.35, 4.0))):
        F.box('Cloth#mat', (x, yy, z + 0.13), (1.9, 0.78, 0.16), yaw=ya, bevel=0.05)
        F.box(('Player#blanket', 'Rust#blanket', 'Player#blanket')[k], (x - 0.25, yy, z + 0.23), (1.2, 0.84, 0.05),
              yaw=ya + 3, roll=2, bevel=0.0)
        bag(p, F.pt(x + 0.75, yy, z + 0.28), (0.5, 0.32, 0.14), yaw=BUSES['R'][2] + 90 + ya, m='Cloth#pillow')
    # a escada de corda pelo lado esquerdo (o da câmera), do teto até o chão
    for x in (-0.15, 0.3):
        a = F.pt(x, -1.24, z + 0.1)
        b = F.pt(x + 0.05, -1.62, 0.02)
        rope(p, a, b, sag=0.05, r=0.025, n=10)
    for k in range(8):
        t = (k + 0.5) / 8
        xa = -0.15 + 0.025 * t
        q0 = F.pt(xa, -1.24 - 0.38 * t, z + 0.1 - (z + 0.08) * t)
        q1 = F.pt(xa + 0.45, -1.24 - 0.38 * t, z + 0.1 - (z + 0.08) * t)
        rod(p, 'Wood#rung', q0, q1, 0.03, seg=6)
    # o M (o escolar): a caixa d'água de plástico preto, as malas e o fardo de lona, o mastro com a bandeira
    F = bus_frame(p, 'M')
    s = SPEC['school']
    z = s['zc'] + 0.01
    F.cyl('Rubber#tank', (-3.9, 0.0, z + 0.5), 'z', 0.55, 1.0, seg=20, bevel=0.0, smooth=True)
    for dz in (0.25, 0.5, 0.75):
        F.cyl('Rubber#rib', (-3.9, 0.0, z + dz), 'z', 0.57, 0.05, seg=20, bevel=0.0)
    F.cyl('Rubber#lid', (-3.9, 0.0, z + 1.03), 'z', 0.25, 0.08, seg=14, bevel=0.0)
    for k, (x, yy, sz, m) in enumerate(((-1.6, 0.3, (0.7, 0.45, 0.3), 'Rust#case'), (-1.0, -0.4, (0.6, 0.4, 0.35), 'Wood#case'),
                                         (-1.2, 0.5, (0.55, 0.38, 0.25), 'Crate'))):
        F.box(m, (x, yy, z + sz[2] / 2 + 0.02), sz, yaw=8 * k - 5, bevel=0.03)
    F.cyl('Cloth#bundle', (0.6, 0.0, z + 0.3), 'y', 0.3, 1.5, seg=12, bevel=0.0, smooth=True)
    for x in (0.35, 0.85):
        F.cyl('Dark#strap', (x, 0.0, z + 0.3), 'y', 0.32, 0.05, seg=12, bevel=0.0)
    mast = F.pt(-4.6, 0.75, z)
    p.cyl('Wood#pole', (mast[0], mast[1], z + 1.75), 'z', 0.05, 3.5, seg=8, bevel=0.0)
    flag(p, (mast[0], mast[1]), z + 3.4, 1.35, 0.85, m='Player', phase=0.4)
    # o L: os fardos e um tapete enrolado
    F = bus_frame(p, 'L')
    s = SPEC['city']
    z = s['zc'] + 0.01
    for k, x in enumerate((-2.9, -1.9, 1.5)):
        F.box(('Cloth#bale', 'Rust#bale', 'Cloth#bale')[k], (x, 0.15 * (-1) ** k, z + 0.25), (0.9, 0.7, 0.5), yaw=6 * k, bevel=0.06)
    F.cyl('Player#rugroll', (0.2, 0.0, z + 0.18), 'y', 0.17, 2.0, seg=12, bevel=0.0, smooth=True)


def tarps(p):
    """As lonas esticadas de teto a teto (o acampamento entre os ônibus): a verde (a cor do jogador) entre o M e o L, a crua remendada
    entre o M e o R; cada uma com a vara no meio da beira da frente."""
    sM, sC = SPEC['school'], SPEC['city']
    FM, FL, FR = bus_frame(p, 'M'), bus_frame(p, 'L'), bus_frame(p, 'R')
    zM, zC = sM['ze'] + 0.05, sC['ze'] + 0.05
    for (Fo, sign, m, xa) in ((FL, 1, 'Player#tarp', (-4.3, 0.6)), (FR, -1, 'Cloth#tarp', (-4.2, 0.4))):
        side_M = -1 if sign == 1 else 1
        c0 = FM.pt(-4.8, side_M * (sM['hw'] - 0.05), zM)
        c1 = FM.pt(-1.3, side_M * (sM['hw'] - 0.05), zM)
        c2 = Fo.pt(xa[1], sign * (sC['hw'] - 0.05), zC)
        c3 = Fo.pt(xa[0], sign * (sC['hw'] - 0.05), zC)
        tarp(p, [c0, c1, c2, c3], m=m, sag=0.45, folds=3, amp=0.07, thick=0.035, seed=7 if sign > 0 else 9)
        # a vara que segura o meio da beira da frente e as cordas até as estacas
        mid = ((c1[0] + c2[0]) / 2, (c1[1] + c2[1]) / 2)
        p.cyl('Wood#pole', (mid[0], mid[1], 1.35), 'z', 0.05, 2.7, seg=8, bevel=0.0)
        top = (mid[0], mid[1], 2.7)
        st = stake(p, mid[0] + 1.3, mid[1] - sign * 0.5, toward=mid)
        rope(p, top, st, sag=0.04)
        # os remendos (o pano de outra cor costurado por cima)
        if m.startswith('Cloth'):
            cx, cy = (c0[0] + c1[0] + c2[0] + c3[0]) / 4, (c0[1] + c1[1] + c2[1] + c3[1]) / 4
            p.obox('Rust#patch', (cx - 0.4, cy, zC - 0.32), (1.1, 0.8, 0.03), yaw=BUSES['R'][2] / 2 + 12, bevel=0.0)


def camp(p):
    """O pátio na frente: a fogueira (as pedras, as achas, a brasa acesa, o tripé com a panela), os bancos de pneu e de caixote em volta,
    o varal do R até a vara (as roupas penduradas, uma verde), os tambores, a pilha de pneus, o cavalete dos fuzis."""
    x, y = 4.6, -6.3
    for k in range(10):
        a = 2 * math.pi * k / 10 + 0.3
        r = 0.55 + 0.05 * _hash(k, 1, 7)
        p.obox('Plaster#stone', (x + r * math.cos(a), y + r * math.sin(a), 0.08), (0.26, 0.21, 0.17), yaw=math.degrees(a) + 30 * _hash(k, 3, 7),
               bevel=0.04)
    p.cyl('Cloth#ash', (x, y, 0.025), 'z', 0.46, 0.04, seg=18, bevel=0.0)
    for k in range(6):
        a = 2 * math.pi * k / 6 + 0.5
        p.obox('Dark#char', (x + 0.16 * math.cos(a), y + 0.16 * math.sin(a), 0.12), (0.5, 0.08, 0.08), yaw=math.degrees(a) + 90,
               pitch=18, bevel=0.0)
    p.cyl('Fire#ember', (x, y, 0.06), 'z', 0.24, 0.06, seg=12, bevel=0.0)
    for k in range(3):
        a = 2 * math.pi * k / 3 + 0.4
        rod(p, 'Metal#tripod', (x + 0.75 * math.cos(a), y + 0.75 * math.sin(a), 0.0), (x, y, 1.25), 0.025, seg=5)
    rod(p, 'Dark#chain', (x, y, 1.25), (x, y, 0.62), 0.012, seg=4)
    revolve(p, 'Dark#pot', (x, y), [(0.12, 0.38), (0.2, 0.42), (0.22, 0.52), (0.2, 0.62), (0.18, 0.64)], seg=14)
    for k, (dx, dy) in enumerate(((1.35, 0.3), (-0.3, 1.3), (0.2, -1.35), (-1.25, -0.4))):
        if k % 2 == 0:
            for q in range(2):
                tire(p, (x + dx, y + dy, 0.12 + q * 0.24), R=0.38, r=0.13)
        else:
            crate(p, x + dx, y + dy, 0.0, size=(0.8, 0.5, 0.42), yaw=40 * k)
    for (dx, dy, yaw, m) in ((0.9, 0.9, 30, 'Rust'), (-0.9, -0.9, -60, 'Player')):
        p.obox(m + '#rug', (x + dx, y + dy, 0.015), (1.5, 0.8, 0.014), yaw=yaw, bevel=0.0)
        p.obox('Cloth#rugb2', (x + dx, y + dy, 0.024), (1.2, 0.5, 0.012), yaw=yaw, bevel=0.0)
    # o varal: do espelho do R até a vara, com as roupas
    FR = bus_frame(p, 'R')
    a = FR.pt(5.95, -1.2, 2.05)
    b = (7.6, 3.6, 2.05)
    p.cyl('Wood#pole', (b[0], b[1], 1.05), 'z', 0.05, 2.15, seg=8, bevel=0.0)
    st = stake(p, b[0] + 0.9, b[1] - 0.6, toward=b)
    rope(p, (b[0], b[1], 2.08), st, sag=0.03)
    pts = catenary(a, b, 0.22, n=14)
    tube(p, 'Wood#rope', pts, 0.015, seg=5, smooth=True)
    yaw = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
    for k, t in enumerate((0.18, 0.3, 0.44, 0.58, 0.72, 0.85)):
        q = pts[int(t * 14)]
        m = ('Cloth#wash', 'Player#wash', 'Rust#wash', 'Cloth#wash', 'Player#wash', 'Paint#wash')[k]
        h = 0.75 + 0.35 * _hash(k, 2, 5)
        p.obox(m, (q[0], q[1], q[2] - h / 2), (0.65 + 0.25 * _hash(k, 4, 5), 0.03, h), yaw=yaw, roll=(_hash(k, 6, 5) - 0.5) * 10,
               bevel=0.0)
    # os tambores, a pilha de pneus, o gás e o cavalete dos fuzis encostados no M (o lado da câmera)
    for (bx, by) in ((-8.45, -0.2), (-8.4, 0.45), (2.9, 4.8)):
        barrel(p, bx, by, 0.0, r=0.28, h=0.86, m='Rust')
    for q in range(4):
        tire(p, (-8.4, 2.4, 0.13 + q * 0.25), R=0.4, r=0.14)
    p.cyl('Metal#gas', (3.35, 4.55, 0.42), 'z', 0.17, 0.84, seg=12, bevel=0.0, smooth=True)
    rack(p, 1.5, -2.65, -1.25)


def base():
    p = new_part(PREFIX + '_Base')
    for m in ('Wood#pole', 'Wood#rope', 'Wood#stake', 'Glass#win'):
        p.bm(m, 0.0)
    # a lataria (os lofts com a ferrugem por face) com o chanfro do harden: as pontas redondas, as faces grandes lisas
    for m in ('Rust#peel', 'Yellow#body', 'Paint#body', 'GlaPaint#body', 'Yellow#hood'):
        p.bm(m, 0.02)
    p.bm('Plaster#stone', 0.03)
    # o chão batido do acampamento (a terra pisada em volta dos ônibus e do pátio): o contorno orgânico, sem laje
    earth_pad(p, (-0.6, 0.0), blob(-0.6, 0.0, 8.4, 10.6, seed=4, amp=0.06), h=0.05)
    for key in ('M', 'L', 'R'):
        bus(p, key)
    # o nome e os rabiscos verdes à mão, nos lados que a câmera vê (o L do R e o L do M) e na traseira do L
    FR, FM, FL = bus_frame(p, 'R'), bus_frame(p, 'M'), bus_frame(p, 'L')
    sC, sM = SPEC['city'], SPEC['school']
    tag(FR, -(sC['hw'] + 0.012), -4.4, 0.95, s=1.25, seed=2)
    tag(FR, -(sC['hw'] + 0.012), 0.9, 0.85, s=0.8, seed=5)
    tag(FM, -(sM['hw'] + 0.03), -4.6, 1.15, s=1.1, seed=3)
    tag(FL, (sC['hw'] + 0.012), -3.0, 0.85, s=1.0, seed=8)
    tag(FL, -(sC['hw'] + 0.012), -2.0, 0.85, s=1.0, seed=6)
    roof_load(p)
    tarps(p)
    camp(p)
    flagpole(p, 8.1, -3.9, 0.0, 3.6, length=1.3, height=0.85, phase=1.1)
    return finish(p)


def write_preview(out):
    import json
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1400, 1400]], ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1600, 900]],
                      ['left', 'ortho', [0, -1, 0], [0, 0, 1], [1600, 900]],
                      ['three_quarter', 'persp', 34.0, 215.0, [1600, 1000], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 32.0, 35.0, [1600, 1000], 30.0, 0.95]],
            'usa_at': [12.5, 0.0, 0.0], 'tank_views': True,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(out, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, indent=1)


def main():
    out = out_dir('gla_barracks_v3')
    clear_scene()
    total = export(base(), out)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -8.6, 3.0, -10.6, 10.6, 3.3, post='Wood', plank='Crate', stripe='Hazard'), out)
    write_preview(out)
    s = SPEC['school']
    door = bus_frame(None, 'M').pt(s['xbody'] - 0.67, s['hw'] + 0.45, 0.0)
    print('MODEL total', total, 'tris')
    print('MODEL lote 18 x 22 m; a porta do escolar (saida) em', tuple(round(v, 2) for v in door), '; a faixa livre Y', -LANE, '..', LANE,
          'de x', round(bus_frame(None, 'M').pt(s['hl'], 0, 0)[0], 2), 'ate', HX)
    fp = [tuple(round(v, 2) for v in bus_frame(None, k).pt(-1.0, 0.0, SPEC[BUSES[k][0]]['zc'])) for k in ('M', 'L', 'R')]
    print('MODEL fire_points_m', fp)


if __name__ == '__main__':
    main()
