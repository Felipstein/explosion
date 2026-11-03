"""O Supply Stash da GLA (o depósito de suprimento), v3 (02/11/2026): O FERRO-VELHO DO GUINDASTE. Do zero, pelo veredito de 02/11
(docs/elenco/IDEIAS_NOVAS_02_11.md: "quero ideias do 0, diferente, novas, bonitas, bem feitas, criativas"). O v1 (o pátio murado com as
caixas) e o v2 (o mercado sob a rede) ficam como estão.

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_gla_supply_stash_v3.py C:/Users/luisf/explosion/captures/game/models/gla_supply_stash_v3

A palavra (o que lê de cima): O GUINDASTE DO FERRO-VELHO. O dinheiro da GLA vem da sucata e do roubo (no ZH o Worker desmancha carro em
dinheiro): o guindaste de lança treliçada montado num caminhão velho (o turquesa desbotado, as sapatas abertas nos calços de madeira), com o
ÍMÃ redondo pendurado segurando a carcaça amassada de um carro, gira devagar sobre o pátio (a peça que gira: a lança, a cabine do operador,
o contrapeso de pneus amarrados, o ímã); a PILHA de carcaças de carro amassadas, cruzadas em camadas, encostada na CERCA de chapa ondulada
(só no lado +Y: a remendada, de chapas de cor diferente, com o nome verde à mão); 2 carcaças inteiras enferrujadas no chão, sem rodas; a
carga que sai dali na frente (os caixotes nos paletes, os tambores, os fardos), onde a picape encosta; os pneus, o bloco de motor, as
garrafas do maçarico, as manchas de óleo. De cima: a lança comprida com o círculo do ímã (a vida do prédio), a pilha colorida de
carcaças e a cerca riscando o lado.

Lote 16 × 16 m, frente em +X. A picape entra e sai pela faixa livre da frente (Y −3,3..3,3, de x 0 até a borda) e encosta na carga (+Y).

Peças (origem no chão, no centro do lote; Unreal X frente, Y direita, Z cima, metros):
  SM_GLA_SupplyStash_Base: tudo o que não gira (o caminhão, a mesa de giro fixa, a pilha, a cerca, a carga; a bandeira sai no flags.json);
  SM_GLA_SupplyStash_Crane: a parte de cima do guindaste (a coroa, a cabine, o contrapeso de pneus, o cavalete, a lança, os cabos, o ímã e a
    carcaça presa nele), feita com a origem no eixo de giro, no topo da mesa: spin em Z, at_m = CRANE_AT;
  SM_GLA_SupplyStash_Scaffold: o andaime da obra em madeira.
Materiais (slots): Tile (o turquesa do caminhão), GlaPaint, Paint, Yellow (as cores das carcaças e do guindaste), Rust (a ferrugem, as chapas),
  Metal, Dark, Rubber, Glass, Wood, Crate, Cloth, Player (o verde: o nome, os trapos, a bandeira, a faixa da lança), Earth, Hazard (as
  listras do contrapeso e o andaime), Light.
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
                                  adobe_window, paint_band, bake_in_place)
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


PREFIX = 'SM_GLA_SupplyStash'
HX, HY = 8.0, 8.0
LANE = 3.3                          # a meia-largura da faixa livre da frente (a picape)
TRUCK = (-1.8, 0.0)                 # o centro do caminhão (de frente pra −Y: a cabine no −Y)
PIV = (-1.8, 1.25)                  # o eixo de giro do guindaste (na carroceria)
Z_TT = 1.78                         # o topo da mesa de giro fixa (a origem da peça que gira)
CRANE_YAW = 62.0                    # o giro da lança em repouso (de +X pra +Y; de lado pra câmera do jogo)
FOOT = (0.75, 0.0, 0.95)            # o pino do pé da lança (no quadro do guindaste)
BOOM_L, BOOM_EL = 8.2, 41.0         # o comprimento e a elevação da lança
HOOK = 3.35                         # o cabo do ímã (da ponta até a argola)
CRANE_AT = (PIV[0], PIV[1], Z_TT)


def stations(x0, x1, step=0.3):
    xs = splits(x0, x1, step)
    for e in (x0, x1):
        sgn = 1 if e == x0 else -1
        xs += [e + sgn * d for d in (0.03, 0.08, 0.15)]
    return sorted(set(round(v, 4) for v in xs))


def boom_tip():
    e = math.radians(BOOM_EL)
    return (FOOT[0] + BOOM_L * math.cos(e), 0.0, FOOT[2] + BOOM_L * math.sin(e))


# ===================================================================================================== as carcaças
def crushed(p, c, yaw, paint, seed, L=4.0, W=1.75, H=0.55, roll=0.0, pitch=0.0, m_key='#car'):
    """O carro amassado pela prensa (a carcaça chata): a lataria esmagada em faces tortas (o amassado é a faceta), a tinta que sobrou
    e a ferrugem, a faixa escura da cabine esmagada dos 2 lados, o para-choque solto torto, o aro de uma roda aparecendo, os faróis."""
    F = Frame(p, c, yaw=yaw, pitch=pitch, roll=roll)
    rings = []
    for k, x in enumerate(stations(-L / 2, L / 2, 0.32)):
        t = x / (L / 2)
        hw = W / 2 * (1.0 - 0.07 * abs(t) ** 3) * (1.0 + 0.05 * (fbm(x * 1.9, 1.0, seed) - 0.5))
        hump = math.exp(-((x + 0.15) / 0.85) ** 2)               # a cabine esmagada (o calombo do teto no meio)
        top = H * (0.62 + 0.55 * hump + 0.22 * fbm(x * 1.4 + 3.0, 2.0, seed + 1))
        ring = []
        for (y, z) in rrect(-hw, hw, 0.0, top, rb=0.06, rt=0.12, n=2):
            dz = 0.06 * (fbm(x * 2.3 + y * 1.7, 4.0, seed + 2) - 0.5) * (z / top)
            dy = 0.05 * (fbm(x * 2.0, z * 3.0, seed + 3) - 0.5) * (1 if y > 0 else -1)
            ring.append((x, y + dy, max(0.0, z + dz)))
        rings.append(ring)

    def mat(cc, i, j):
        n = fbm(cc.x * 1.1 + seed, cc.y * 1.3 + cc.z * 2.0, seed)
        return 'Rust#scrap' if n > 0.6 else paint + m_key
    loft_mat(p, rings, mat, lambda q: F.pt(*q))
    for s in (-1, 1):
        F.box('Dark#crush', (-0.15, s * (W / 2 - 0.05), H * 0.78), (L * 0.42, 0.05, H * 0.22), roll=s * 6, bevel=0.0)
        F.cyl('Metal#rim', (L / 2 - 0.75, s * (W / 2 - 0.12), 0.18), 'y', 0.24, 0.12, seg=12, bevel=0.0)
    F.box('Metal#bumper', (L / 2 + 0.05, 0.1, H * 0.35), (0.08, W * 0.9, 0.12), yaw=8 * (_hash(seed, 1, 2) - 0.5), roll=12, bevel=0.0)
    for s in (-1, 1):
        F.box('Glass#lamp', (L / 2 + 0.01, s * (W / 2 - 0.3), H * 0.5), (0.04, 0.26, 0.1), bevel=0.0)


def shell_car(p, c, yaw, paint, seed):
    """A carcaça inteira enferrujada no chão (sem rodas, sentada nos aros): a lataria de baixo, a cabine sem vidro (o escuro de dentro),
    o capô torto aberto, a tinta que sobrou em manchas."""
    F = Frame(p, c, yaw=yaw, roll=2.0 * (_hash(seed, 3, 1) - 0.5))
    L, W = 4.3, 1.74

    def mat(cc, i, j):
        n = fbm(cc.x * 1.2 + seed, cc.z * 2.4 + cc.y, seed)
        return paint + '#shell' if n > 0.66 else 'Rust#shell'
    rings = []
    for x in stations(-L / 2, L / 2, 0.3):
        t = abs(x) / (L / 2)
        hw = W / 2 * (1.0 - 0.1 * t ** 4)
        top = 0.88 - 0.12 * smooth(0.6, 1.0, t)
        rings.append([(x, y, z) for (y, z) in rrect(-hw, hw, 0.2, top, rb=0.1, rt=0.12, n=2)])
    loft_mat(p, rings, mat, lambda q: F.pt(*q))
    rings = []
    for x in stations(-1.1, 0.75, 0.25):
        t = (x + 0.175) / 0.925
        hw = W / 2 - 0.12 - 0.12 * abs(t) ** 2
        top = 1.36 - 0.18 * max(0.0, abs(t) - 0.55) / 0.45
        rings.append([(x, y, z) for (y, z) in rrect(-hw, hw, 0.86, top, rb=0.02, rt=0.16, n=2)])
    loft_mat(p, rings, mat, lambda q: F.pt(*q))
    for s in (-1, 1):
        F.box('Dark#hole', (-0.2, s * (W / 2 - 0.16), 1.1), (1.5, 0.05, 0.36), bevel=0.0)
        for x in (1.3, -1.35):
            F.cyl('Metal#rim', (x, s * (W / 2 - 0.15), 0.25), 'y', 0.25, 0.14, seg=12, bevel=0.0)
    F.box('Dark#hole', (0.82, 0.0, 1.08), (0.05, W - 0.4, 0.36), pitch=-30, bevel=0.0)
    F.box('Rust#hood', (1.55, 0.0, 1.15), (1.15, W - 0.1, 0.05), pitch=38, bevel=0.0)


def pile(p):
    """A pilha de carcaças amassadas cruzadas em camadas (as cores que sobraram: o branco, o bege, o turquesa, o amarelo, a ferrugem),
    encostada na cerca; uma de lado escorada, os pneus soltos por cima."""
    H = 0.55
    lay = [  # (x, y, camada, yaw, cor, roll)
        (-4.4, 4.55, 0, 2, 'Paint', 0), (-4.3, 6.4, 0, -3, 'GlaPaint', 0), (-0.9, 6.55, 0, 7, 'Tile', 0),
        (-5.4, 5.45, 1, 92, 'Yellow', 0), (-3.35, 5.5, 1, 86, 'Paint', 2),
        (-4.45, 4.95, 2, -2, 'Tile', -2), (-4.55, 6.55, 2, 5, 'GlaPaint', 3),
        (-4.7, 5.6, 3, 96, 'Paint', 0), (-4.25, 5.35, 4, 12, 'Yellow', 6),
    ]
    for k, (x, y, lv, yaw, m, roll) in enumerate(lay):
        crushed(p, (x, y, lv * H * 0.98), yaw, m, 11 + k, H=H, roll=roll)
    # a de lado escorada na pilha (a frente pra cima)
    crushed(p, (-6.7, 4.2, 0.75), 70, 'Rust', 31, H=0.5, pitch=58)
    for (x, y, z) in ((-4.0, 5.2, 2.75), (-4.9, 6.1, 2.2)):
        tire(p, (x, y, z), R=0.36, r=0.13)


def fence(p):
    """A cerca de chapa ondulada só no lado +Y (do canto de trás até a frente da pilha): as chapas de pé de cores diferentes, o topo
    torto, os postes de cano com as travessas, uma chapa faltando, o nome verde pintado à mão."""
    y0 = 7.65
    xs = splits(-7.7, 3.6, 1.25)
    cols = ['Rust', 'GlaPaint', 'Rust', 'Paint', 'Tile', 'Rust', 'GlaPaint', 'Rust', 'Rust', 'Paint']
    for k, (a, b) in enumerate(zip(xs[:-1], xs[1:])):
        if k == 6:
            continue                                         # a chapa que falta (o vão)
        m = cols[k % len(cols)] + '#sheet'
        h0 = 2.2 + 0.25 * _hash(k, 1, 4)
        h1 = 2.2 + 0.25 * _hash(k + 1, 1, 4) if k % 3 else h0 - 0.2
        lean = 0.03 * (_hash(k, 2, 4) - 0.5)
        wave_sheet(p, m, (a - 0.04, y0 + 0.03 * (k % 2)), (b + 0.04, y0 + 0.03 * (k % 2)), 0.0, h0, h1, lean=lean)
    for x in splits(-7.7, 3.6, 2.5):
        p.cyl('Metal#post', (x, y0 + 0.08, 1.2), 'z', 0.05, 2.5, seg=8, bevel=0.0)
    for z in (0.5, 1.9):
        rod(p, 'Metal#rail', (-7.7, y0 + 0.09, z), (3.6, y0 + 0.09, z), 0.03, seg=6)
    # o nome verde à mão (do lado de dentro, o que a câmera vê) e a bandeira na ponta da cerca
    Fz = Frame(p, (0.0, y0 - 0.08, 0.0), yaw=0.0)
    tag(Fz, 0.0, -3.0, 1.35, s=1.3, seed=12)
    p.cyl('Wood#pole', (3.75, y0 - 0.1, 2.0), 'z', 0.05, 4.0, seg=8, bevel=0.0)
    flag(p, (3.75, y0 - 0.1), 3.95, 1.4, 0.9, m='Player', phase=0.3)


def wave_sheet(p, m, a, b, z0, h0, h1, lean=0.0, pitch=0.16, amp=0.035, th=0.02):
    """A chapa ondulada de pé de a até b (a onda ao longo da cerca), o topo de h0 a h1 (torto), inclinada 'lean' (m por m)."""
    A, B = Vector((a[0], a[1], 0.0)), Vector((b[0], b[1], 0.0))
    L = (B - A).length
    d = (B - A).normalized()
    nrm_ = Vector((-d.y, d.x, 0.0))
    n = max(6, int(L / pitch) * 3)
    bm_ = p.bm(m, 0.0)
    rows = []
    for side in (-1, 1):
        row = []
        for k in range(n + 1):
            t = k / n
            w = amp * math.sin(2 * math.pi * t * L / (2 * pitch))
            q = A + d * (L * t) + nrm_ * (w + side * th / 2)
            zt = h0 + (h1 - h0) * t + 0.04 * math.sin(t * 17.0)
            row.append((bm_.verts.new(V(q.x, q.y, z0)), bm_.verts.new(V(q.x + nrm_.x * lean * zt, q.y + nrm_.y * lean * zt, zt))))
        rows.append(row)
    f, bk = rows[1], rows[0]
    for k in range(n):
        bm_.faces.new([f[k][0], f[k + 1][0], f[k + 1][1], f[k][1]]).smooth = True
        bm_.faces.new([bk[k + 1][0], bk[k][0], bk[k][1], bk[k + 1][1]]).smooth = True
        bm_.faces.new([f[k][1], f[k + 1][1], bk[k + 1][1], bk[k][1]])
    bm_.faces.new([bk[0][0], f[0][0], f[0][1], bk[0][1]])
    bm_.faces.new([f[n][0], bk[n][0], bk[n][1], f[n][1]])


# ===================================================================================================== o caminhão
def truck(p):
    """O caminhão velho (de frente pra −Y): a cabine redonda turquesa desbotada com o vidro e as portas, o capô com os paralamas
    redondos e a grade, o chassi, a carroceria de tábuas com a mesa de giro fixa, as sapatas abertas nos calços de madeira (o guindaste
    trabalha parado), as rodas (a da frente murcha), o tanque de diesel, o escapamento, o nome verde na porta."""
    F = Frame(p, (TRUCK[0], TRUCK[1], 0.0), yaw=-90.0)
    paint = 'Tile'

    def peel(c, i, j, base=paint + '#cab'):
        # a ferrugem do caminhão velho: a saia e a barra das portas, e umas manchas pequenas (a tinta lisa no resto)
        n = fbm(c.x * 2.6 + 4.0, c.z * 3.0 + c.y * 0.9, 21)
        return 'Rust#peel' if (c.z < 1.3 + 0.12 * n) or n > 0.82 else base
    # o chassi e a carroceria
    for s in (-1, 1):
        F.box('Dark#frame', (-0.2, s * 0.48, 0.88), (8.0, 0.14, 0.26), bevel=0.0)
    F.box('Wood#deck', (-1.4, 0.0, 1.2), (5.6, 2.4, 0.12), bevel=0.0)
    for x in splits(-4.15, 1.35, 0.5):
        F.box('Dark#deckgap', (x, 0.0, 1.265), (0.03, 2.38, 0.01), bevel=0.0)
    for s in (-1, 1):
        F.box('Metal#deckedge', (-1.4, s * 1.2, 1.15), (5.6, 0.08, 0.22), bevel=0.0)
    # a cabine (loft de seção redonda), o vidro recuado, a porta, o teto
    rings = []
    for x in stations(1.55, 3.0, 0.22):
        rings.append([(x, y, z) for (y, z) in rrect(-1.1, 1.1, 1.05, 2.72, rb=0.06, rt=0.32, n=3)])
    loft_mat(p, rings, peel, lambda q: F.pt(*q))
    F.box('Glass#cab', (2.98, 0.0, 2.25), (0.06, 1.85, 0.62), pitch=-6, bevel=0.0)
    for s in (-1, 1):
        F.box('Glass#cab', (2.4, s * 1.1, 2.28), (0.85, 0.04, 0.5), bevel=0.0)
        F.box('Dark#seam', (2.3, s * 1.105, 1.6), (0.02, 0.02, 1.1), bevel=0.0)
        F.box('Dark#handle', (2.0, s * 1.12, 1.75), (0.16, 0.03, 0.04), bevel=0.0)
        F.rod('Dark#mirror', (2.95, s * 1.1, 2.3), (3.05, s * 1.45, 2.45), 0.02)
        F.box('Dark#mirror', (3.05, s * 1.45, 2.3), (0.04, 0.18, 0.32), bevel=0.0)
    # o capô, os paralamas, a grade, os faróis, o para-choque
    rings = []
    for x in stations(2.95, 4.15, 0.2):
        t = (x - 2.95) / 1.2
        rings.append([(x, y, z) for (y, z) in rrect(-0.78, 0.78, 0.9, 1.86 - 0.12 * t ** 1.4, rb=0.04, rt=0.24, n=3)])
    loft_mat(p, rings, lambda c, i, j: peel(c, i, j, paint + '#hood'), lambda q: F.pt(*q))
    for s in (-1, 1):
        rings = []
        for k in range(9):
            a = math.pi * k / 8
            x = 3.55 + 0.62 * math.cos(a)
            z = 0.55 + 0.55 * math.sin(a)
            rings.append([F.pt(x, s * yy, z) for yy in (0.72, 1.18)] + [F.pt(x, s * 1.18, z - 0.14), F.pt(x, s * 0.72, z - 0.14)])
        p.loft(paint + '#fender', rings, bevel=0.0)
    F.box('Dark#grille', (4.16, 0.0, 1.35), (0.05, 1.2, 0.72), bevel=0.0)
    for k in range(6):
        F.box('Metal#bar', (4.19, -0.5 + k * 0.2, 1.35), (0.03, 0.05, 0.7), bevel=0.0)
    for s in (-1, 1):
        F.cyl('Metal#lamprim', (4.12, s * 0.95, 1.2), 'x', 0.14, 0.08, seg=12, bevel=0.0)
        F.cyl('Glass#lamp', (4.16, s * 0.95, 1.2), 'x', 0.1, 0.03, seg=12, bevel=0.0)
    F.box('Dark#bumper', (4.25, 0.0, 0.62), (0.2, 2.3, 0.24), bevel=0.03)
    # as rodas: a frente (a esquerda murcha) e o tandem de trás (as duplas)
    for (x, s, sunk) in ((3.55, -1, 0.14), (3.55, 1, 0.0), (-2.2, -1, 0.0), (-2.2, 1, 0.0), (-3.45, -1, 0.0), (-3.45, 1, 0.08)):
        wheel(p, F.pt(x, s * 0.95, 0.52), F.vec((0, 1, 0)), R=0.52, w=0.36, rim=0.3, sunk=sunk)
    # o tanque de diesel, o escapamento atrás da cabine
    F.cyl('Metal#tank', (1.0, -1.05, 0.95), 'x', 0.28, 1.1, seg=14, bevel=0.0, smooth=True)
    F.cyl('Dark#exhaust', (1.42, 0.95, 2.4), 'z', 0.07, 2.4, seg=8, bevel=0.0)
    # a mesa de giro fixa (o pedestal com os parafusos) e as sapatas abertas nos calços
    c = F.pt(-1.25, 0.0, 0.0)
    p.cyl('Metal#base', (c[0], c[1], (1.26 + Z_TT - 0.02) / 2), 'z', 0.95, Z_TT - 0.02 - 1.26, seg=28, bevel=0.0)
    p.cyl('Dark#basering', (c[0], c[1], Z_TT - 0.06), 'z', 1.02, 0.08, seg=28, bevel=0.0)
    for k in range(16):
        a = 2 * math.pi * k / 16
        p.cyl('Dark#bolt', (c[0] + 0.88 * math.cos(a), c[1] + 0.88 * math.sin(a), Z_TT - 0.01), 'z', 0.035, 0.05, seg=6, bevel=0.0)
    for x in (-3.9, 0.95):
        for s in (-1, 1):
            F.box('Yellow#outrig', (x, s * 1.55, 0.95), (0.3, 1.5, 0.26), bevel=0.0)
            F.box('Hazard#outrig', (x, s * 2.25, 0.95), (0.32, 0.12, 0.28), bevel=0.0)
            F.cyl('Metal#jack', (x, s * 2.2, 0.5), 'z', 0.08, 0.8, seg=10, bevel=0.0)
            for q in range(3):
                F.box('Wood#cribbing', (x, s * 2.2, 0.06 + q * 0.12), (0.75 if q % 2 == 0 else 0.22, 0.22 if q % 2 == 0 else 0.75,
                                                                       0.12), yaw=5 * q, bevel=0.0)
    # o nome na porta da cabine (o lado da câmera, +X do mundo = +y da cabine)
    tag(F, 1.12, 1.75, 1.55, s=0.38, seed=15)


# ===================================================================================================== a carga, o pátio
def cargo(p):
    """A carga que sai dali (do lado da faixa, onde a picape encosta): os paletes com os caixotes cintados, os tambores azuis e
    enferrujados em grupo, os fardos de lona, os sacos; o bloco de motor no palete, as garrafas do maçarico, os pneus, as manchas de óleo."""
    for k, (x, y, yaw) in enumerate(((2.6, 4.6, 4), (4.3, 4.7, -6), (2.7, 6.1, 9), (5.6, 6.0, 2))):
        p.obox('Wood#pallet', (x, y, 0.07), (1.25, 1.05, 0.14), yaw=yaw, bevel=0.0)
        for q in range(2 if k % 2 == 0 else 1):
            crate(p, x, y, 0.14 + q * 0.5, size=(1.15, 0.95, 0.5), yaw=yaw + 3 * q)
    for (x, y, m) in ((6.6, 4.2, 'Tile'), (7.15, 4.45, 'Tile'), (6.8, 4.8, 'Rust'), (4.2, 6.6, 'Metal'), (4.7, 6.8, 'Rust')):
        barrel(p, x, y, 0.0, r=0.29, h=0.88, m=m)
    for k, (x, y) in enumerate(((1.4, 5.3), (1.3, 6.0))):
        bag(p, (x, y, 0.2), (0.8, 0.5, 0.36), yaw=80 + 10 * k, m='Cloth#sack')
    bag(p, (1.35, 5.65, 0.52), (0.8, 0.5, 0.32), yaw=95, m='Cloth#sack')
    # o bloco de motor no palete
    p.obox('Wood#pallet', (-0.3, -6.3, 0.07), (1.2, 1.0, 0.14), yaw=20, bevel=0.0)
    p.obox('Rust#engine', (-0.3, -6.3, 0.5), (0.85, 0.6, 0.65), yaw=20, bevel=0.05)
    p.obox('Rust#engine', (-0.3, -6.3, 0.9), (0.55, 0.5, 0.2), yaw=20, bevel=0.04)
    for s in (-1, 1):
        p.cyl('Dark#pulley', (-0.3 + s * 0.36 * math.cos(math.radians(20)), -6.3 + s * 0.36 * math.sin(math.radians(20)), 0.6),
              (math.cos(math.radians(20)), math.sin(math.radians(20)), 0.0), 0.14, 0.05, seg=12, bevel=0.0)
    # as garrafas do maçarico (o oxigênio e o acetileno) amarradas, a mangueira
    for (x, y, m) in ((1.3, -6.6, 'Metal#bottle'), (1.6, -6.7, 'Rust#bottle')):
        p.cyl(m, (x, y, 0.62), 'z', 0.12, 1.24, seg=12, bevel=0.0, smooth=True)
        p.cyl('Dark#valve', (x, y, 1.3), 'z', 0.05, 0.12, seg=8, bevel=0.0)
    tube(p, 'Dark#hose', [(1.3, -6.6, 1.3), (1.0, -6.0, 0.4), (0.4, -5.6, 0.04), (-0.2, -5.4, 0.04)], 0.02, seg=5, smooth=True)
    for q in range(4):
        tire(p, (-6.9, 1.6, 0.13 + q * 0.25), R=0.42, r=0.14)
    for q in range(2):
        tire(p, (-6.3, 2.3, 0.13 + q * 0.25), R=0.42, r=0.14)
    for (x, y, r) in ((-0.2, 3.2, 0.9), (-2.4, -4.9, 0.7), (3.8, 2.9, 0.6)):
        p.cyl('Dark#oil', (x, y, 0.005), 'z', r, 0.012, seg=16, bevel=0.0)


# ===================================================================================================== a peça que gira
def crane():
    """A parte de cima do guindaste (gira em Z no eixo da mesa): a coroa, o piso, a cabine do operador (amarela, a ferrugem, o vidro, a
    porta aberta), o contrapeso de PNEUS empilhados na gaiola com as correntes e as listras de perigo, o cavalete em A com os cabos de
    levantar a lança, a LANÇA de treliça (a faixa verde, a cabeça com a roldana), o cabo do ímã, o ÍMÃ redondo com as correntes e a caixa
    de ligação, e a carcaça amassada grudada embaixo dele. Feito na origem (o eixo, no topo da mesa), a lança pra +X girada CRANE_YAW."""
    p = new_part(PREFIX + '_Crane')
    for m in ('Yellow#cabin', 'Rust#peel'):
        p.bm(m, 0.02)
    F = Frame(p, (0.0, 0.0, 0.0), yaw=CRANE_YAW)
    F.cyl('Dark#ring', (0.0, 0.0, 0.06), 'z', 1.0, 0.12, seg=28, bevel=0.0)
    F.box('Rust#deck', (-0.35, 0.0, 0.22), (2.9, 2.2, 0.2), bevel=0.03)
    # a cabine do operador (à esquerda da lança)
    rings = []
    for x in stations(-0.2, 1.25, 0.2):
        rings.append([(x, y, z) for (y, z) in rrect(-1.1, -0.2, 0.32, 2.15, rb=0.04, rt=0.16, n=2)])

    def peel(c, i, j):
        return 'Rust#peel' if fbm(c.x * 1.6 + 1.0, c.z * 2.0 + c.y, 33) > 0.68 else 'Yellow#cabin'
    loft_mat(p, rings, peel, lambda q: F.pt(*q))
    F.box('Glass#cabin', (1.26, -0.65, 1.55), (0.05, 0.8, 0.85), pitch=-8, bevel=0.0)
    F.box('Glass#cabin', (0.55, -1.105, 1.6), (1.0, 0.04, 0.65), bevel=0.0)
    F.box('Glass#cabin', (0.55, -0.195, 1.6), (1.0, 0.04, 0.65), bevel=0.0)
    F.box('Dark#door', (0.1, -1.12, 1.05), (0.5, 0.03, 1.3), bevel=0.0)
    F.box('Metal#step', (0.15, -1.3, 0.35), (0.5, 0.3, 0.05), bevel=0.0)
    F.box('Dark#roofvent', (0.5, -0.65, 2.2), (0.35, 0.35, 0.1), bevel=0.0)
    F.box('Yellow#visor', (1.38, -0.65, 2.08), (0.36, 0.98, 0.04), pitch=-12, bevel=0.0)
    for x in (1.27, -0.21):
        F.box('Dark#frame', (x, -0.65, 1.55), (0.06, 0.92, 0.06), bevel=0.0)
    F.rod('Metal#handrail', (-0.1, -1.18, 0.6), (-0.1, -1.18, 1.9), 0.02)
    for y in (-1.0, -0.3):
        F.rod('Metal#handrail', (0.0, y, 2.22), (1.1, y, 2.22), 0.018)
    # o contrapeso de pneus: a gaiola de cantoneira, as 2 pilhas de pneus deitados, as correntes, as listras de perigo
    for (x, y) in ((-1.75, -0.95), (-1.75, 0.95), (-0.95, -0.95), (-0.95, 0.95)):
        F.rod('Metal#cage', (x, y, 0.3), (x, y, 2.3), 0.04)
    for z in (0.35, 1.3, 2.28):
        for (a, b) in (((-1.75, -0.95), (-1.75, 0.95)), ((-0.95, -0.95), (-0.95, 0.95)), ((-1.75, -0.95), (-0.95, -0.95)),
                       ((-1.75, 0.95), (-0.95, 0.95))):
            F.rod('Metal#cage', (a[0], a[1], z), (b[0], b[1], z), 0.035)
    for y in (-0.48, 0.48):
        for q in range(7):
            c = F.pt(-1.35, y, 0.45 + q * 0.27)
            tire(p, c, R=0.36, r=0.13)
    for y in (-0.48, 0.48):
        F.rod('Dark#chain', (-1.35, y + 0.38, 0.4), (-1.35, y + 0.38, 2.3), 0.025)
        F.rod('Dark#chain', (-1.35, y - 0.38, 0.4), (-1.35, y - 0.38, 2.3), 0.025)
    for k in range(4):
        F.box('Hazard#stripe', (-1.78, -0.6 + 0.4 * k, 0.6), (0.04, 0.2, 0.5), roll=35, bevel=0.0)
    # o cavalete em A (atrás do pé da lança) e o tambor do guincho
    apex = (-0.25, 0.0, 2.9)
    for s in (-1, 1):
        F.rod('Yellow#gantry', (-0.75, s * 0.75, 0.32), apex, 0.07)
        F.rod('Yellow#gantry', (0.35, s * 0.75, 0.32), apex, 0.06)
    F.cyl('Dark#drum', (0.05, 0.0, 0.62), 'y', 0.3, 1.0, seg=14, bevel=0.0)
    F.cyl('Metal#drumcap', (0.05, 0.52, 0.62), 'y', 0.38, 0.05, seg=14, bevel=0.0)
    F.cyl('Metal#drumcap', (0.05, -0.52, 0.62), 'y', 0.38, 0.05, seg=14, bevel=0.0)
    # a lança de treliça: o pé com o pino e as orelhas, a faixa verde, a cabeça com a roldana
    e = math.radians(BOOM_EL)
    d = Vector((math.cos(e), 0.0, math.sin(e)))
    foot = Vector(FOOT)
    tip = foot + d * BOOM_L
    for s in (-1, 1):
        F.box('Yellow#lug', (FOOT[0] - 0.1, s * 0.33, FOOT[2] - 0.25), (0.4, 0.06, 0.6), bevel=0.0)
    F.cyl('Dark#pin', FOOT, 'y', 0.07, 0.8, seg=10, bevel=0.0)
    fw = lambda q: F.pt(*q)
    a, b = foot + d * 0.15, tip - d * 0.25
    truss(p, 'Yellow#boom', fw(tuple(a)), fw(tuple(b)), 0.5, up=F.vec((-math.sin(e), 0.0, math.cos(e))), chords=4, bay=0.6, r=0.05,
          rd=0.025)
    for t in (0.35, 0.4):
        c = foot + d * (BOOM_L * t)
        F.box('Player#boomband', tuple(c), (0.32, 0.56, 0.56), pitch=BOOM_EL, bevel=0.0)
    F.box('Yellow#head', tuple(tip - d * 0.15), (0.5, 0.5, 0.45), pitch=BOOM_EL, bevel=0.02)
    F.cyl('Dark#sheave', tuple(tip + Vector((0.1, 0.0, 0.0))), 'y', 0.28, 0.14, seg=16, bevel=0.0)
    # os cabos de levantar a lança (do cavalete até a cabeça) e o cabo do ímã
    for s in (-1, 1):
        F.rod('Dark#cable', (apex[0], s * 0.05, apex[2]), (tip.x - 0.2, s * 0.18, tip.z + 0.15), 0.018)
    ring = (tip.x + 0.38, 0.0, tip.z - HOOK)
    F.rod('Dark#cable', (tip.x + 0.38, 0.0, tip.z - 0.05), ring, 0.022)
    # o ímã: o disco grosso com as nervuras, a tampa, as 3 correntes até a argola, a caixa de ligação e o cabo de força
    mz = ring[2] - 0.8
    F.cyl('Dark#magnet', (ring[0], 0.0, mz - 0.18), 'z', 0.82, 0.38, seg=28, bevel=0.0)
    F.cyl('Metal#magtop', (ring[0], 0.0, mz + 0.03), 'z', 0.7, 0.06, seg=28, bevel=0.0)
    for k in range(8):
        a_ = 2 * math.pi * k / 8
        F.box('Dark#rib', (ring[0] + 0.5 * math.cos(a_), 0.5 * math.sin(a_), mz + 0.09), (0.5, 0.05, 0.1), yaw=math.degrees(a_), bevel=0.0)
    F.box('Hazard#magband', (ring[0], 0.0, mz - 0.18), (1.66, 0.12, 0.12), bevel=0.0)
    for k in range(3):
        a_ = 2 * math.pi * k / 3 + 0.5
        F.rod('Dark#chain', (ring[0] + 0.5 * math.cos(a_), 0.5 * math.sin(a_), mz + 0.1), ring, 0.025)
    p.torus('Dark#ring', F.pt(*ring), 'z', 0.1, 0.03, seg=10, rseg=4)
    F.box('Metal#jbox', (ring[0] + 0.2, 0.3, mz + 0.2), (0.3, 0.22, 0.2), bevel=0.0)
    pts = [F.pt(ring[0] + 0.2, 0.3, mz + 0.3), F.pt(ring[0] + 0.5, 0.45, mz + 1.0), F.pt(tip.x + 0.1, 0.4, tip.z - 1.6),
           F.pt(tip.x - 0.6, 0.3, tip.z - 0.6)]
    tube(p, 'Dark#powercable', pts, 0.03, seg=6, smooth=True)
    # a carcaça grudada no ímã (atravessada na lança: não varre longe)
    crushed(p, F.pt(ring[0], 0.0, mz - 0.37 - 0.5), CRANE_YAW + 90.0 + 8.0, 'GlaPaint', 41, H=0.5, roll=4.0, m_key='#held')
    o = finish(p)
    return o


def base():
    p = new_part(PREFIX + '_Base')
    for m in ('Wood#pole', 'Wood#rope', 'Glass#cab'):
        p.bm(m, 0.0)
    for m in ('Rust#peel', 'Tile#cab', 'Tile#hood'):
        p.bm(m, 0.02)
    p.bm('Plaster#stone', 0.03)
    # o chão batido do ferro-velho (o contorno orgânico, sem laje)
    earth_pad(p, (-0.6, 0.6), blob(-0.6, 0.6, 7.3, 7.2, seed=9, amp=0.06), h=0.05)
    truck(p)
    pile(p)
    fence(p)
    shell_car(p, (-6.1, -3.6, 0.0), 72.0, 'Paint', 51)
    shell_car(p, (-4.6, -6.4, 0.0), 8.0, 'Tile', 52)
    cargo(p)
    return finish(p)


def write_preview(out):
    import json
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]], [PREFIX + '_Crane', [CRANE_AT[0], -CRANE_AT[1], CRANE_AT[2]]]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1400, 1400]], ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1600, 900]],
                      ['side_left', 'ortho', [0, -1, 0], [0, 0, 1], [1600, 900]],
                      ['three_quarter', 'persp', 32.0, 215.0, [1600, 1000], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 32.0, 35.0, [1600, 1000], 30.0, 0.95]],
            'usa_at': [9.0, -5.0, 0.0], 'tank_views': True,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(out, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, indent=1)


def main():
    out = out_dir('gla_supply_stash_v3')
    clear_scene()
    total = export(base(), out)
    clear_scene()
    c = crane()
    bake_in_place(c, CRANE_AT)
    total += export(c, out)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -7.6, 3.8, -6.6, 7.6, 3.2, post='Wood', plank='Crate', stripe='Hazard'), out)
    write_preview(out)
    tip = boom_tip()
    reach = tip[0] + 0.38
    print('MODEL total', total, 'tris')
    print('MODEL lote 16 x 16 m; crane spin at_m', [round(v, 3) for v in CRANE_AT], '; a ponta da lança a', round(tip[2] + Z_TT, 2),
          'm; o ímã a', round(reach, 2), 'm do eixo, a', round(tip[2] - HOOK - 1.2 + Z_TT, 2), 'm (o fundo da carcaça presa); giro em repouso',
          CRANE_YAW, 'graus')
    print('MODEL fire_points_m', [(-1.8, -2.6, 2.6), (-4.4, 5.4, 2.4), (3.4, 5.2, 1.0)])
    print('MODEL a faixa livre da picape: Y', -LANE, '..', LANE, 'de x 0,6 até', HX)


if __name__ == '__main__':
    main()
