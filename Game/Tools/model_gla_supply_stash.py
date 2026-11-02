"""O Supply Stash da GLA provisório, modelado no Blender por script (o elenco das 3 facções, 31/10/2026: docs/ELENCO.md §5.1).

Rodar (Blender sem janela, só CPU; não usa a placa):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_gla_supply_stash.py -- captures/game/models/gla_supply_stash

A palavra: A PILHA DE CAIXAS DEBAIXO DA LONA E DA REDE DE CAMUFLAGEM, COM A BOCA DO TÚNEL POR ONDE AS CAIXAS DESCEM.
O Supply Stash do ZH é "o monte de caixas" (docs/CONCEITO_CONSTRUCOES.md §3b); aqui ele vira um objeto só: a pilha de
caixas paletizada (degraus: alta no meio, baixa nas bordas) com a rede de camuflagem jogada por cima das 4 fileiras de trás
(deitada nas caixas, as quinas marcando por baixo, um pico baixo no meio onde o pau da bandeira a levanta, a barra presa
nas estacas) e a lona amarrada na frente-direita; a frente-esquerda à mostra. De cima: o monte mosqueado arredondado com
a bandeira no meio (a assinatura, regra 2 do §3b); a 1ª versão (a rede em tenda num mastro alto) lia barraca, não pilha.
- A função (regra 5): a picape da carga encosta de ré na frente (+X) na doca de descarga (o tablado na altura da caçamba,
  os pneus de para-choque, a rampa de tábuas) e na balança de plataforma; as caixas vão pra pilha e descem pro túnel no
  fundo (o poço com o colar de madeira) pelo A de ferro com a talha e a corrente (uma caixa descendo, pendurada).
- O que se mexe (regra 3): a bandeira do mastro; a caixa da talha (pode descer e subir no jogo, se quiserem).
- A família da Oficina: o muro de adobe do fundo com a borda verde e os pilaretes, os pilares da frente com as bandeiras,
  e nos lados a cerca de chapa ondulada remendada (a borda verde pintada no alto, os rabiscos).
- Nunca (a bíblia): montanha de caixas e tambores esparramada; aqui é UMA pilha organizada em paletes, sob a rede.

Lote 16 × 16 m (4 × 4 quadrados de 4 m), a frente em +X. Alturas: a cerca 2,1 m, o muro do fundo 2,4 m, a pilha até
2,5 m, o pico da rede 2,8 m (+ a bandeira até 5,1 m), o A de ferro 4,7 m.

Peças (origem no chão, no centro do lote; Unreal X frente, Y direita, Z cima, metros):
  SM_GLA_SupplyStash_Base: tudo;
  SM_GLA_SupplyStash_Scaffold: o andaime da obra em madeira.
Materiais (slots): Plaster, Earth, Rust, Wood, Cloth, Net, Crate, Metal, Player, Dark, Trim, Light, Hazard (só no andaime).
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, flag, scaffold  # noqa: E402
from model_faction_kit import barrel, lamp  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'gla_supply_stash')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_GLA_SupplyStash'

# ===================================================================================== kit GLA do elenco (31/10)
# (a mesma cópia nos 6 prédios da GLA do elenco: model_gla_command_center/supply_stash/barracks/palace/stinger_site/
# arms_dealer.py; o model_lib e o model_faction_kit não são editados, outros agentes usam ao mesmo tempo)
COLORS.update({'Player': (86, 150, 62), 'Plaster': (205, 180, 135), 'Earth': (168, 142, 104), 'Rust': (118, 70, 40),
               'Wood': (120, 88, 52), 'Cloth': (150, 128, 92), 'Net': (96, 104, 62), 'Crate': (84, 86, 58),
               'Metal': (92, 90, 82), 'Dark': (40, 40, 38), 'Trim': (70, 68, 62), 'Light': (255, 220, 160),
               'Beacon': (255, 170, 40), 'Hazard': (210, 170, 30)})
G = 0.2                          # o chão batido do lote (o topo da fundação)
BEVELS = {'Plaster': 0.13, 'Plaster#lo': 0.012, 'Plaster#s': 0.0, 'Plaster#arch': 0.0, 'Earth': 0.06, 'Earth#drift': 0.0, 'Player': 0.0,
          'Net': 0.0, 'Cloth#net': 0.0, 'Cloth#tarp': 0.0, 'Cloth#bag': 0.0, 'Cloth': 0.015, 'Dark': 0.0, 'Trim': 0.0,
          'Rust': 0.015, 'Rust#sheet': 0.0, 'Rust#s': 0.0, 'Wood': 0.015, 'Crate': 0.03, 'Metal': 0.015, 'Metal#s': 0.0,
          'Light': 0.0, 'Beacon': 0.0, 'Hazard': 0.0}


def new_part(name):
    """A peça com o chanfro de cada material já escolhido (o primeiro uso de um material fixa o chanfro dele)."""
    p = BPart(name)
    for m, b in BEVELS.items():
        p.bm(m, b)
    return p


def finish(p):
    """Junta a peça (Part.finish) e mostra os triângulos por material (pra caber na conta do caderno)."""
    o = p.finish()
    # a rede de segurança: o chanfro (Bevel) às vezes solta um espeto (um vértice a quilômetros); ele volta pra média dos
    # vizinhos sãos, sem abrir buraco, e o log avisa
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
    n = (i * 374761393 + j * 668265263 + s * 1442695041) & 0xFFFFFFFF
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
    """Um referencial local (origem, yaw, pitch, roll) pra montar um conjunto girado/inclinado (a carcaça, o tripé)."""

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


def frustum(p, m, x0, x1, y0, y1, z0, z1, inset):
    """O bloco afunilado (o contraforte de adobe, mais grosso embaixo): a base x0..x1/y0..y1, o topo encolhido 'inset'."""
    p.loft(m, [[(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)],
               [(x0 + inset, y0 + inset, z1), (x1 - inset, y0 + inset, z1), (x1 - inset, y1 - inset, z1), (x0 + inset, y1 - inset, z1)]])


def _plane(c, face_axis, s, z, n):
    """Um ponto no plano de uma parede: s ao longo da parede, z, n na normal (face_axis 'x': a parede olha pra ±X)."""
    return (c[0] + n, c[1] + s, z) if face_axis == 'x' else (c[0] + s, c[1] + n, z)


def arch_fill(p, m, c, face_axis, w, spring, top, depth, seg=16):
    """O enchimento de adobe sobre um vão em arco redondo (os tímpanos e o intradorso), sem booleano: o vão de largura w
    centrado em c=(x, y), a imposta em z=spring, a parede até z=top (top ≥ spring + w/2), a espessura depth na normal."""
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


def arch_band(p, m, c, face_axis, w, spring, out, width=0.28, t=0.025, seg=16, legs=0.0):
    """A moldura pintada em volta de um arco (a faixa verde seguindo o arco), colada na face (out = o deslocamento na
    normal até a face, com o sinal do lado de fora). legs > 0 desce a faixa pelos lados até spring - legs."""
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
    if legs > 0:
        for sgn in (-1, 1):
            ss = sgn * (r0 + r1) / 2
            q = _plane(c, face_axis, ss, spring - legs / 2, out + s * t / 2)
            size = (t, width, legs) if face_axis == 'x' else (width, t, legs)
            p.box(m, q[0] - size[0] / 2, q[0] + size[0] / 2, q[1] - size[1] / 2, q[1] + size[1] / 2, q[2] - size[2] / 2, q[2] + size[2] / 2, bevel=0.0)


def wall_seg(p, a, b, h, t=0.6, z0=G, band=True, round_top=True, m='Plaster', band_hi=0.12, band_lo=0.55):
    """Um trecho de muro de adobe de a=(x, y) até b, com o topo arredondado e a borda verde (cor do jogador) nas 2 faces."""
    (ax, ay), (bx, by) = a, b
    L = math.hypot(bx - ax, by - ay)
    dx, dy = (bx - ax) / L, (by - ay) / L
    yaw = math.degrees(math.atan2(dy, dx))
    cx, cy = (ax + bx) / 2, (ay + by) / 2
    p.obox(m, (cx, cy, z0 + h / 2), (L, t, h), yaw=yaw)
    if round_top:
        p.cyl(m + '#s', (cx, cy, z0 + h), (dx, dy, 0.0), t / 2 * 0.97, L - 0.012, seg=12, bevel=0.0, smooth=True)
    if band:
        for s in (-1, 1):
            o = s * (t / 2 + 0.012)
            p.obox('Player', (cx - dy * o, cy + dx * o, z0 + h - (band_hi + band_lo) / 2), (L, 0.02, band_lo - band_hi), yaw=yaw)


def pilaster(p, x, y, h, z0=G, s=0.45, rebar=False):
    """O pilarete de adobe com a capa (o da Oficina) e, se pedido, o vergalhão espetado no topo."""
    frustum(p, 'Plaster', x - s - 0.06, x + s + 0.06, y - s - 0.06, y + s + 0.06, z0, z0 + h - 0.25, 0.06)
    p.box('Plaster#lo', x - s - 0.1, x + s + 0.1, y - s - 0.1, y + s + 0.1, z0 + h - 0.25, z0 + h)
    if rebar:
        rebar_tuft(p, x, y, z0 + h, s * 0.6)


def rebar_tuft(p, x, y, z, s=0.25, n=4, h=1.1, seed=3):
    """O vergalhão espetado no topo do pilar (o andar de cima que nunca veio): n ferros, tortos na ponta."""
    for k in range(n):
        a = 2 * math.pi * k / n + 0.6
        bx, by = x + s * math.cos(a), y + s * math.sin(a)
        hh = h * (0.75 + 0.5 * _hash(k, seed, 9))
        bend = 0.12 + 0.25 * _hash(k, seed, 11)
        top = (bx + bend * math.cos(a + 1.1), by + bend * math.sin(a + 1.1), z + hh + 0.15)
        rod(p, 'Rust', (bx, by, z - 0.1), (bx, by, z + hh), 0.022, seg=5)
        rod(p, 'Rust', (bx, by, z + hh), top, 0.022, seg=5)


def round_tower(p, c, r0, r1, h, z0=G, merlons=8, band=True):
    """A torre redonda de adobe afunilada (a da kasbah), com o parapeito, as ameias de topo redondo e a borda verde."""
    x, y = c
    revolve(p, 'Plaster#s', (x, y), [(r0, z0), (r0 - (r0 - r1) * 0.5, z0 + h * 0.5), (r1, z0 + h)], seg=40, smooth=True)
    disc_ring(p, 'Plaster', (x, y), r1 + 0.12, r1 - 0.4, z0 + h - 0.05, z0 + h + 0.45, seg=40)       # o parapeito
    for k in range(merlons):
        a = 2 * math.pi * (k + 0.5) / merlons
        mx, my = x + (r1 - 0.13) * math.cos(a), y + (r1 - 0.13) * math.sin(a)
        p.obox('Plaster#lo', (mx, my, z0 + h + 0.65), (0.42, 0.5, 0.4), yaw=math.degrees(a))
        p.cyl('Plaster#s', (mx, my, z0 + h + 0.85), (math.cos(a), math.sin(a), 0.0), 0.21, 0.5, seg=12, bevel=0.0, smooth=True)
    if band:
        zb = z0 + h - 0.62
        rb = r0 - (r0 - r1) * ((zb - z0) / h) + 0.02
        disc_ring(p, 'Player', (x, y), rb, rb - 0.15, zb, zb + 0.42, seg=40)


def round_merlons(p, a, b, z, n=None, w=0.5, t=0.45, h=0.45, m='Plaster'):
    """A fileira de ameias de topo redondo ao longo de a→b (o coroamento dos muros do palácio e do portão)."""
    (ax, ay), (bx, by) = a, b
    L = math.hypot(bx - ax, by - ay)
    dx, dy = (bx - ax) / L, (by - ay) / L
    n = n or max(2, int(L / (w * 2.0)))
    yaw = math.degrees(math.atan2(dy, dx))
    for k in range(n):
        u = (k + 0.5) / n
        x, y = ax + (bx - ax) * u, ay + (by - ay) * u
        p.obox(m + '#s', (x, y, z + h / 2), (w, t, h), yaw=yaw)
        p.cyl(m + '#s', (x, y, z + h), (-dy, dx, 0.0), w / 2, t + 0.03, seg=10, bevel=0.0, smooth=True)


def beams(p, face, fixed, s0, s1, z, step=0.9, out=0.38, r=0.085):
    """As pontas das vigas de madeira saindo da parede logo abaixo do teto (o adobe do Oriente Médio e do Sahel)."""
    n = max(1, int(abs(s1 - s0) / step))
    sgn = 1 if face[0] == '+' else -1
    for k in range(n + 1):
        s = s0 + (s1 - s0) * k / n
        rr = r * (0.85 + 0.3 * _hash(k, int(s * 10), 5))
        if face[1] == 'x':
            p.cyl('Wood', (fixed + sgn * out / 2, s, z), 'x', rr, out, seg=8, bevel=0.0)
        else:
            p.cyl('Wood', (s, fixed + sgn * out / 2, z), 'y', rr, out, seg=8, bevel=0.0)


def spout(p, x, y, z, face, L=0.75):
    """A canaleta do teto (a calha de madeira com o fundo escuro), saindo da parede pra jogar a água longe."""
    sgn = 1 if face[0] == '+' else -1
    if face[1] == 'x':
        p.box('Wood', min(x, x + sgn * L), max(x, x + sgn * L), y - 0.11, y + 0.11, z - 0.06, z + 0.08, bevel=0.0)
        p.box('Dark', min(x, x + sgn * L), max(x, x + sgn * L) + 0.002, y - 0.06, y + 0.06, z + 0.08, z + 0.085, bevel=0.0)
    else:
        p.box('Wood', x - 0.11, x + 0.11, min(y, y + sgn * L), max(y, y + sgn * L), z - 0.06, z + 0.08, bevel=0.0)
        p.box('Dark', x - 0.06, x + 0.06, min(y, y + sgn * L), max(y, y + sgn * L), z + 0.08, z + 0.085, bevel=0.0)


def drift(p, a, b, nrm, h=0.4, w=0.6, seed=1):
    """A areia acumulada no pé do muro (do lado de fora): o prisma baixo encostado de a até b, a normal nrm=(nx, ny)."""
    (ax, ay), (bx, by) = a, b
    L = math.hypot(bx - ax, by - ay)
    k = max(1, int(L / 3.0))
    for i in range(k):
        u0, u1 = i / k + 0.04, (i + 1) / k - 0.04
        hh = h * (0.6 + 0.6 * _hash(i, seed, 21))
        ww = w * (0.6 + 0.4 * _hash(i, seed, 23))          # nunca passa de w (fica no lote)
        p0 = (ax + (bx - ax) * u0, ay + (by - ay) * u0)
        p1 = (ax + (bx - ax) * u1, ay + (by - ay) * u1)
        pts = [(p0[0], p0[1], G - 0.05), (p0[0] + nrm[0] * ww, p0[1] + nrm[1] * ww, G - 0.05), (p0[0], p0[1], G + hh)]
        prism(p, 'Earth#drift', pts, (p1[0] - p0[0], p1[1] - p0[1], 0.0))


def water_tank(p, x, y, z, r=0.8, h=1.4, stand=0.0, m='Rust'):
    """A caixa d'água de chapa (a ferrugem), com os aros, a tampa cônica e o cano; stand > 0 põe nos 4 pés."""
    if stand > 0:
        for (dx, dy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            rod(p, 'Trim', (x + dx * r * 0.62, y + dy * r * 0.62, z), (x + dx * r * 0.62, y + dy * r * 0.62, z + stand), 0.05)
        for k in range(4):
            a0, a1 = [(-1, -1), (1, -1), (1, 1), (-1, 1)][k], [(-1, -1), (1, -1), (1, 1), (-1, 1)][(k + 1) % 4]
            rod(p, 'Trim', (x + a0[0] * r * 0.62, y + a0[1] * r * 0.62, z + 0.15), (x + a1[0] * r * 0.62, y + a1[1] * r * 0.62, z + stand - 0.05), 0.025)
        p.box('Trim', x - r * 0.75, x + r * 0.75, y - r * 0.75, y + r * 0.75, z + stand - 0.08, z + stand, bevel=0.0)
    zb = z + stand
    p.cyl(m, (x, y, zb + h / 2), 'z', r, h, seg=28, bevel=0.0, smooth=True)
    for f in (0.3, 0.7):
        p.cyl('Dark', (x, y, zb + h * f), 'z', r + 0.018, 0.05, seg=28, bevel=0.0)
    p.cyl(m, (x, y, zb + h + 0.11), 'z', r + 0.03, 0.22, seg=28, r2=r * 0.25, bevel=0.0)
    p.cyl('Dark', (x, y, zb + h + 0.25), 'z', 0.14, 0.1, seg=12, bevel=0.0)
    rod(p, 'Dark', (x + r * 0.7, y, zb + 0.1), (x + r * 0.7, y, z - 0.02 if stand > 0 else zb + 0.02), 0.04)


def stovepipe(p, x, y, z0, h, r=0.18, m='Rust'):
    """A chaminé de cano (ferrugem) com o chapéu de chuva escuro nos 3 pezinhos: solta a fumaça preta."""
    p.cyl(m, (x, y, z0 + h / 2), 'z', r, h, seg=16, bevel=0.0, smooth=True)
    p.cyl('Dark', (x, y, z0 + h - 0.12), 'z', r + 0.02, 0.24, seg=16, bevel=0.0)
    for k in range(3):
        a = 2 * math.pi * k / 3
        rod(p, 'Dark', (x + r * 0.8 * math.cos(a), y + r * 0.8 * math.sin(a), z0 + h), (x + r * 0.8 * math.cos(a), y + r * 0.8 * math.sin(a), z0 + h + 0.22), 0.015, seg=4)
    p.cyl('Dark', (x, y, z0 + h + 0.3), 'z', r * 2.0, 0.1, seg=16, r2=r * 0.5, bevel=0.0)


def bag(p, c, size, yaw=0.0, m='Cloth#bag'):
    """O saco de areia estufado (o do tanque GLA, model_gla_tank.py, mais leve): a seção superelipse, cheio no meio, as
    pontas amarradas, a barriga achatada no apoio; cada saco varia um pouco."""
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


def bag_row(p, pts, z0, layers=3, bag_size=(0.66, 0.36, 0.24), seed=0):
    """A mureta de sacos de areia ao longo da polilinha, desencontrados por fileira."""
    L_, W_, H_ = bag_size
    for (ax, ay), (bx, by) in zip(pts[:-1], pts[1:]):
        L = math.hypot(bx - ax, by - ay)
        yaw = math.degrees(math.atan2(by - ay, bx - ax))
        n = max(1, int(round(L / (L_ * 0.92))))
        for lv in range(layers):
            off = 0.5 if lv % 2 else 0.0
            for k in range(n + (0 if off else 0)):
                t = (k + 0.5 + off * 0.5) / n
                if t > 1.0:
                    continue
                x, y = ax + (bx - ax) * t, ay + (by - ay) * t
                bag(p, (x, y, z0 + H_ * 0.42 + lv * H_ * 0.82), (L_, W_, H_), yaw=yaw + (_hash(k, lv, seed) - 0.5) * 12)


def bag_ring(p, c, r, z0, layers=3, gap=None, bag_size=(0.66, 0.36, 0.24), seed=0):
    """O ninho redondo de sacos de areia; gap = (de, até) em graus pra a entrada."""
    L_, W_, H_ = bag_size
    n = max(8, int(2 * math.pi * r / (L_ * 0.9)))
    for lv in range(layers):
        rr = r - lv * 0.03
        for k in range(n):
            a = 2 * math.pi * (k + (0.5 if lv % 2 else 0.0)) / n
            deg = math.degrees(a) % 360
            if gap and (gap[0] <= deg <= gap[1] or gap[0] <= deg + 360 <= gap[1]):
                continue
            bag(p, (c[0] + rr * math.cos(a), c[1] + rr * math.sin(a), z0 + H_ * 0.42 + lv * H_ * 0.82), (L_, W_, H_),
                yaw=math.degrees(a) + 90 + (_hash(k, lv, seed) - 0.5) * 10)


def net_surface(p, fn, nu, nv, seed=1, blotch=1.5, holes=0.3, light=0.62, ragged=0.35, thick=0.03, closed_u=False,
                mats=('Net', 'Cloth#net')):
    """A rede de camuflagem de verdade (de cima é o mosqueado da ficha, não um plano verde): a grade fn(u, v) → (x, y, z);
    um campo de ruído decide onde há rede (Net), retalho claro de juta por cima (Cloth) e buraco (o chão aparece). Os
    contornos saem pelo marching squares (interpolados nas arestas: manchas redondas, nunca escada de pixel); a borda
    se desfia (o ruído cai perto da borda). Duas faces (em cima e embaixo) com a espessura."""
    P = [[fn(i / nu, j / nv) for i in range(nu + 1)] for j in range(nv + 1)]
    F = []
    for j in range(nv + 1):
        row = []
        for i in range(nu + 1):
            u, v = i / nu, j / nv
            n = fbm(P[j][i][0] / blotch + 0.37, P[j][i][1] / blotch + 0.71, seed)
            ec = (1.0 - v) * nv if closed_u else min(v * nv, (1.0 - v) * nv, u * nu, (1.0 - u) * nu)   # células até a borda
            row.append(n - ragged * max(0.0, 1.0 - ec / 1.6) - (0.0 if ec > 0 else 0.6))
        F.append(row)
    for (m, t, lift) in ((mats[0], holes, 0.0), (mats[1], light, 0.016)):
        b = p.bm(m, 0.0)
        for j in range(nv):
            for i in range(nu):
                cs = [(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)]
                poly = []
                for k in range(4):
                    (ia, ja), (ib, jb) = cs[k], cs[(k + 1) % 4]
                    fa, fb = F[ja][ia], F[jb][ib]
                    if fa >= t:
                        poly.append(P[ja][ia])
                    if (fa >= t) != (fb >= t):
                        s_ = (t - fa) / (fb - fa)
                        pa, pb = P[ja][ia], P[jb][ib]
                        poly.append((pa[0] + (pb[0] - pa[0]) * s_, pa[1] + (pb[1] - pa[1]) * s_, pa[2] + (pb[2] - pa[2]) * s_))
                if len(poly) < 3:
                    continue
                top = [b.verts.new(V(x, y, z + thick / 2 + lift)) for (x, y, z) in poly]
                bot = [b.verts.new(V(x, y, z - thick / 2 + lift)) for (x, y, z) in poly]
                b.faces.new(top).smooth = True
                b.faces.new(list(reversed(bot))).smooth = True


def camo_net_rect(p, corners, sag=0.5, cell=0.3, seed=1, blotch=1.5, **kw):
    """A rede esticada entre 4 cantos (x, y, z) em volta, com a barriga no meio."""
    (x0, y0, z0), (x1, y1, z1), (x2, y2, z2), (x3, y3, z3) = corners
    L = max(math.dist((x0, y0), (x1, y1)), math.dist((x3, y3), (x2, y2)))
    W = max(math.dist((x1, y1), (x2, y2)), math.dist((x0, y0), (x3, y3)))
    nu, nv = max(4, int(L / cell)), max(4, int(W / cell))

    def fn(u, v):
        x = (1 - u) * (1 - v) * x0 + u * (1 - v) * x1 + u * v * x2 + (1 - u) * v * x3
        y = (1 - u) * (1 - v) * y0 + u * (1 - v) * y1 + u * v * y2 + (1 - u) * v * y3
        z = (1 - u) * (1 - v) * z0 + u * (1 - v) * z1 + u * v * z2 + (1 - u) * v * z3
        return (x, y, z - sag * math.sin(math.pi * u) * math.sin(math.pi * v))
    net_surface(p, fn, nu, nv, seed=seed, blotch=blotch, **kw)


def tarp_sheet(p, corners, sag=0.15, nx=8, ny=6, m='Cloth#tarp'):
    """A lona esticada entre 4 cantos (a do model_lib, sem chanfro)."""
    from model_lib import sheet
    sheet(p, m, corners, nx, ny, sag=sag, thick=0.03)


def scrawl(p, c, face, size=1.8, seed=0, m='Player'):
    """O rabisco verde pintado à mão na parede (a cor do jogador): 4 pinceladas afinadas nas pontas, em ângulos e
    comprimentos que variam, cada uma um pouco mais pra fora (sem sobrepor no mesmo plano), colado na face."""
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


def tire(p, c, R=0.42, r=0.14, axis='z', m='Dark'):
    p.torus(m, c, axis, R, r, seg=18, rseg=8)


def flagpole(p, x, y, z0, h, length=1.3, height=0.85, phase=0.0, m_pole='Trim'):
    """O mastro com a bandeira verde (cor do jogador) no topo, o pano pra trás (−X)."""
    p.cyl(m_pole, (x, y, z0 + h / 2), 'z', 0.045, h, seg=8, bevel=0.0)
    p.cyl(m_pole, (x, y, z0 + h + 0.05), 'z', 0.07, 0.1, seg=8, bevel=0.0)
    flag(p, (x, y), z0 + h - 0.05, length, height, m='Player', phase=phase)


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
# ===================================================================================== fim do kit GLA do elenco
# (31/10, o detalhe AAA: docs/ESTUDO_DETALHE_AAA.md §3 "Prédio de adobe e sucata") o kit trocado pelas peças do
# model_building_parts.py, mesma assinatura: o muro de uma peça só com a base mais larga, o topo de pão e a quina gasta, a faixa
# verde pintada à mão (a borda ondula), as ameias de uma peça, a areia do pé arredondada, a rede com volume e os tufos, a lona com
# as dobras das cordas
from model_building_parts import (BPart, WallFace, wbox, adobe_block, adobe_window, adobe_door, barn_gate, open_box,  # noqa: E402,F401
                                  ring_boxes, ac_unit, cable, pipe_run, paint_band_block, gla_drift, bake_in_place)
from model_building_parts import (gla_wall_seg as wall_seg, gla_net_surface as net_surface, gla_tarp_sheet as tarp_sheet,  # noqa: E402,F811
                                  gla_round_merlons as round_merlons)
drift = gla_drift  # noqa: F811


# ------------------------------------------------------------------------------------------------- medidas
HX = HY = 8.0                   # o lote 16 × 16
PILE_C = (-1.5, 0.35)           # o centro da pilha
NET_C = (-2.8, 0.35)            # o pico da rede (o pau da bandeira por baixo dela)
SHAFT = (-5.0, -5.3)            # o poço do túnel (fundo, −Y)
SHAFT_W = 2.0
CRANE_TOP = 4.4
SLOT = (1.3, 1.12)              # o palete (passo da grade da pilha)
CRATE = (1.2, 0.95, 0.55)       # a caixa padrão


def pile_layout():
    """A pilha: uma grade 5 × 4 de paletes; a altura cai do meio pras bordas (degraus), a fileira da frente baixa (a
    descarga chega por ali), as caixas giradas um pouco. Devolve [(x, y, yaw, n, material)]."""
    heights = [[1, 2, 2, 1],
               [2, 3, 3, 2],
               [2, 4, 4, 3],
               [1, 3, 3, 2],
               [1, 2, 1, 0]]
    out = []
    for i in range(5):
        for j in range(4):
            n = heights[i][j]
            if n == 0:
                continue
            x = PILE_C[0] + (i - 2) * SLOT[0]
            y = PILE_C[1] + (j - 1.5) * SLOT[1]
            yaw = (_hash(i, j, 41) - 0.5) * 8
            m = 'Crate' if (i + j) % 3 else 'Wood'
            out.append((x, y, yaw, n, m))
    return out


PILE = pile_layout()


def stack_boxes():
    """As caixas da pilha como footprints (x0, x1, y0, y1, topo) pro caimento da rede e da lona."""
    out = []
    for (x, y, yaw, n, m) in PILE:
        top = G + 0.14 + n * CRATE[2]
        out.append((x - CRATE[0] / 2, x + CRATE[0] / 2, y - CRATE[1] / 2, y + CRATE[1] / 2, top))
    return out


BOXES = stack_boxes()


def pile_h(x, y, k):
    """A altura da superfície que cai por cima da pilha: o topo de cada caixa, caindo k metros por metro fora dela."""
    h = 0.0
    for (x0, x1, y0, y1, top) in BOXES:
        dx = max(x0 - x, 0.0, x - x1)
        dy = max(y0 - y, 0.0, y - y1)
        h = max(h, top - k * math.hypot(dx, dy))
    return h


def crate(p, x, y, z, yaw, m, size=CRATE):
    """Uma caixa: a de munição (Crate, verde-oliva, com as alças e a cinta) ou a de madeira (Wood, com as ripas)."""
    p.obox(m, (x, y, z + size[2] / 2), size, yaw=yaw)
    ca, sa = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    if m == 'Crate':
        p.obox('Dark', (x, y, z + size[2] / 2), (size[0] * 0.3, size[1] + 0.02, size[2] + 0.02), yaw=yaw, bevel=0.0)
    else:
        for t in (-0.32, 0.32):
            p.obox('Dark', (x + ca * size[0] * t, y + sa * size[0] * t, z + size[2] / 2), (0.05, size[1] + 0.02, size[2] + 0.02), yaw=yaw, bevel=0.0)


def pallet(p, x, y, z, yaw, size=(1.25, 1.05)):
    ca, sa = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    for t in (-0.42, 0.0, 0.42):
        p.obox('Wood', (x - sa * size[1] * t, y + ca * size[1] * t, z + 0.05), (size[0], 0.12, 0.1), yaw=yaw, bevel=0.0)
    for t in (-0.4, -0.13, 0.13, 0.4):
        p.obox('Wood', (x + ca * size[0] * t, y + sa * size[0] * t, z + 0.12), (0.18, size[1], 0.04), yaw=yaw, bevel=0.0)


def the_pile(p):
    """A pilha paletizada; a rede de camuflagem armada no mastro cobre a metade de trás e pousa nas caixas (a borda nas
    estacas e, na frente, levantada em 2 paus); a lona amarrada por cima da frente-direita; a frente-esquerda à mostra."""
    for (x, y, yaw, n, m) in PILE:
        pallet(p, x, y, G, yaw)
        for k in range(n):
            mm = m if k % 2 == 0 else ('Wood' if m == 'Crate' else 'Crate')
            crate(p, x + (_hash(k, int(x * 10), 3) - 0.5) * 0.06, y, G + 0.14 + k * CRATE[2], yaw + (k % 2) * 3, mm)
    # a lona amarrada por cima da frente-direita (justa nas caixas, cai pelos lados), com as cordas
    tx0, tx1, ty0, ty1 = -0.9, 1.75, 0.5, 2.8

    def tz(u, v, z):
        x = tx0 + (tx1 - tx0) * u
        y = ty0 + (ty1 - ty0) * v
        return max(G + 0.05, pile_h(x, y, 2.8) + 0.035)
    from model_lib import sheet
    sheet(p, 'Cloth#tarp', [(tx0, ty0, 0.0), (tx1, ty0, 0.0), (tx1, ty1, 0.0), (tx0, ty1, 0.0)], 16, 14, z_fn=tz, thick=0.03)
    for x in (-0.3, 0.6, 1.4):
        pts = []
        for k in range(13):
            y = ty0 - 0.05 + (ty1 - ty0 + 0.1) * k / 12
            pts.append((x, y, max(G + 0.06, pile_h(x, y, 2.8) + 0.07)))
        for a_, b_ in zip(pts[:-1], pts[1:]):
            rod(p, 'Dark', a_, b_, 0.015, seg=4)
    # a rede de camuflagem jogada por cima das 4 fileiras de trás: deitada nas caixas (as quinas marcam por baixo), um pico
    # baixo no meio (o pau da bandeira por baixo), a barra caindo até o chão e presa nas estacas
    cx, cy = NET_C
    nx0, nx1, ny0, ny1 = -5.55, 0.15, -2.55, 3.25

    def nz(x, y):
        d = math.hypot(x - cx, y - cy)
        return max(G + 0.06, pile_h(x, y, 1.15) + 0.12) + max(0.0, 0.75 - 0.42 * d)

    def fn(u, v):
        x = nx0 + (nx1 - nx0) * u
        y = ny0 + (ny1 - ny0) * v
        return (x, y, nz(x, y))
    net_surface(p, fn, 40, 40, seed=5, blotch=1.1, holes=0.36, light=0.58, ragged=0.45)
    ztop = nz(cx, cy)
    p.cyl('Wood', (cx, cy, (G + ztop + 0.3) / 2), 'z', 0.07, ztop + 0.3 - G, seg=10, bevel=0.0)
    p.cyl('Trim', (cx, cy, ztop + 0.3 + 1.0), 'z', 0.04, 2.0, seg=8, bevel=0.0)
    flag(p, (cx, cy), ztop + 2.25, 1.6, 1.0, m='Player', phase=0.6)
    # as estacas na barra da rede e as cordas
    for k in range(12):
        t = k / 12
        per = 2 * ((nx1 - nx0) + (ny1 - ny0))
        d = t * per
        if d < nx1 - nx0:
            x, y, ox, oy = nx0 + d, ny0, 0, -1
        elif d < (nx1 - nx0) + (ny1 - ny0):
            x, y, ox, oy = nx1, ny0 + d - (nx1 - nx0), 1, 0
        elif d < 2 * (nx1 - nx0) + (ny1 - ny0):
            x, y, ox, oy = nx1 - (d - (nx1 - nx0) - (ny1 - ny0)), ny1, 0, 1
        else:
            x, y, ox, oy = nx0, ny1 - (d - 2 * (nx1 - nx0) - (ny1 - ny0)), -1, 0
        if x > -0.6 and ox >= 0:
            continue
        x, y = x - ox * 0.35, y - oy * 0.35
        z = nz(x, y)
        sx, sy = x + ox * 0.75, y + oy * 0.75
        p.cyl('Wood', (sx, sy, G + 0.15), (ox * 0.3, oy * 0.3, 1.0), 0.04, 0.5, seg=6, bevel=0.0)
        rod(p, 'Dark', (x, y, z), (sx, sy, G + 0.3), 0.012, seg=4)


def shaft_and_crane(p):
    """A boca do túnel: o poço quadrado com o colar de vigas, a escada descendo, os sacos de areia; por cima o A de ferro
    (2 cavaletes em A e a viga), a talha no carrinho, a corrente e uma caixa descendo."""
    sx, sy = SHAFT
    h = SHAFT_W / 2
    p.box('Dark', sx - h + 0.02, sx + h - 0.02, sy - h + 0.02, sy + h - 0.02, G - 3.0, G + 0.0, bevel=0.0)          # o escuro do poço
    for (x0, x1, y0, y1) in ((sx - h - 0.3, sx + h + 0.3, sy - h - 0.3, sy - h), (sx - h - 0.3, sx + h + 0.3, sy + h, sy + h + 0.3),
                             (sx - h - 0.3, sx - h, sy - h, sy + h), (sx + h, sx + h + 0.3, sy - h, sy + h)):
        p.box('Wood', x0, x1, y0, y1, G - 0.05, G + 0.28, bevel=0.0)                                             # o colar
    for (x, y) in ((sx - h - 0.15, sy - h - 0.15), (sx + h + 0.15, sy - h - 0.15), (sx + h + 0.15, sy + h + 0.15), (sx - h - 0.15, sy + h + 0.15)):
        p.box('Wood', x - 0.18, x + 0.18, y - 0.18, y + 0.18, G - 0.05, G + 0.42, bevel=0.0)
    for k in range(4):                                                                                           # o forro de tábuas
        z = G - 0.4 - k * 0.55
        p.box('Wood', sx - h + 0.02, sx + h - 0.02, sy - h + 0.02, sy - h + 0.1, z - 0.22, z, bevel=0.0)
        p.box('Wood', sx + h - 0.1, sx + h - 0.02, sy - h + 0.02, sy + h - 0.02, z - 0.22, z, bevel=0.0)
    ladder_x = sx - h + 0.25
    for y in (sy - 0.25, sy + 0.25):
        p.box('Trim', ladder_x - 0.03, ladder_x + 0.03, y - 0.03, y + 0.03, G - 2.5, G + 0.9, bevel=0.0)
    for k in range(10):
        z = G - 2.3 + k * 0.32
        p.box('Trim', ladder_x - 0.025, ladder_x + 0.025, sy - 0.25, sy + 0.25, z - 0.02, z + 0.02, bevel=0.0)
    bag_row(p, [(sx + h + 0.55, sy - h - 0.3), (sx + h + 0.55, sy + h + 0.3)], G, layers=2, seed=8)
    # o A de ferro: 2 cavaletes (pernas de cantoneira), a viga de cima, as travessas
    for yy in (sy - h - 0.55, sy + h + 0.55):
        for s in (-1, 1):
            rod(p, 'Rust', (sx + s * 1.45, yy, G), (sx, yy, CRANE_TOP), 0.075, seg=6)
            p.box('Rust', sx + s * 1.45 - 0.2, sx + s * 1.45 + 0.2, yy - 0.2, yy + 0.2, G, G + 0.08, bevel=0.0)
        rod(p, 'Rust', (sx - 0.82, yy, G + 1.9), (sx + 0.82, yy, G + 1.9), 0.05, seg=6)
    p.box('Rust', sx - 0.11, sx + 0.11, sy - h - 0.8, sy + h + 0.8, CRANE_TOP - 0.05, CRANE_TOP + 0.22, bevel=0.01)
    p.box('Dark', sx - 0.16, sx + 0.16, sy - h - 0.8, sy + h + 0.8, CRANE_TOP + 0.22, CRANE_TOP + 0.26, bevel=0.0)
    # a talha: o carrinho na viga, o corpo da talha, a corrente de mão, a corrente da carga e a caixa pendurada
    p.box('Dark', sx - 0.18, sx + 0.18, sy - 0.25, sy + 0.25, CRANE_TOP - 0.25, CRANE_TOP - 0.05, bevel=0.0)
    p.cyl('Rust', (sx, sy, CRANE_TOP - 0.55), 'y', 0.22, 0.32, seg=14, bevel=0.0)
    p.box('Dark', sx - 0.08, sx + 0.08, sy - 0.08, sy + 0.08, CRANE_TOP - 0.85, CRANE_TOP - 0.7, bevel=0.0)
    rod(p, 'Dark', (sx, sy + 0.18, CRANE_TOP - 0.6), (sx + 0.05, sy + 0.2, G + 1.0), 0.012, seg=4)
    rod(p, 'Dark', (sx, sy - 0.18, CRANE_TOP - 0.6), (sx - 0.05, sy - 0.2, G + 1.2), 0.012, seg=4)
    zc = G + 0.85
    rod(p, 'Dark', (sx, sy, CRANE_TOP - 0.85), (sx, sy, zc + 0.95), 0.025, seg=5)
    p.box('Dark', sx - 0.07, sx + 0.07, sy - 0.07, sy + 0.07, zc + 0.85, zc + 1.0, bevel=0.0)
    for s in (-1, 1):
        rod(p, 'Dark', (sx, sy, zc + 0.9), (sx + s * 0.45, sy, zc + 0.56), 0.012, seg=4)
    crate(p, sx, sy, zc, 0.0, 'Crate', size=(1.1, 0.75, 0.55))


def scale_and_unloading(p):
    """A frente: a balança de plataforma com o mostrador, os paletes de sacos, o carrinho de mão com a caixa, a lâmpada."""
    bx, by = 4.7, -3.9
    p.box('Dark', bx - 1.3, bx + 1.3, by - 0.9, by + 0.9, G - 0.02, G + 0.1, bevel=0.0)
    p.box('Metal', bx - 1.22, bx + 1.22, by - 0.82, by + 0.82, G + 0.1, G + 0.16, bevel=0.0)
    for k in range(5):
        p.box('Dark', bx - 1.2 + k * 0.6, bx - 1.17 + k * 0.6, by - 0.8, by + 0.8, G + 0.16, G + 0.17, bevel=0.0)
    # a coluna e o mostrador redondo virados pra frente
    p.box('Trim', bx - 1.55, bx - 1.35, by - 1.15, by - 0.95, G, G + 1.35, bevel=0.0)
    p.cyl('Trim', (bx - 1.4, by - 1.05, G + 1.62), 'x', 0.32, 0.18, seg=24, bevel=0.0)
    p.cyl('Cloth', (bx - 1.3, by - 1.05, G + 1.62), 'x', 0.26, 0.03, seg=24, bevel=0.0)
    p.obox('Dark', (bx - 1.28, by - 1.0, G + 1.68), (0.01, 0.03, 0.22), roll=-35, bevel=0.0)
    p.obox('Player', (bx - 1.37, by - 1.05, G + 2.05), (0.05, 0.35, 0.12), bevel=0.0)
    # na balança: 2 sacos e uma caixa
    bag(p, (bx - 0.3, by + 0.1, G + 0.3), (0.85, 0.52, 0.3), yaw=8)
    bag(p, (bx - 0.25, by + 0.15, G + 0.55), (0.85, 0.52, 0.3), yaw=-6)
    crate(p, bx + 0.6, by - 0.1, G + 0.16, 12, 'Wood', size=(0.9, 0.7, 0.5))
    # os paletes de sacos (a farinha, o grão) no canto da frente-direita
    for (x, y, yaw, n) in ((4.6, 3.3, 5, 3), (5.9, 4.9, -10, 2)):
        pallet(p, x, y, G, yaw)
        for lv in range(n):
            for (dx, dy) in ((-0.3, -0.27), (0.3, -0.27), (-0.3, 0.27), (0.3, 0.27)):
                if lv == n - 1 and (dx, dy) == (0.3, 0.27):
                    continue
                ca, sa = math.cos(math.radians(yaw + lv * 90)), math.sin(math.radians(yaw + lv * 90))
                bag(p, (x + dx * ca - dy * sa, y + dx * sa + dy * ca, G + 0.32 + lv * 0.26), (0.62, 0.5, 0.28), yaw=yaw + lv * 90)
    # o carrinho de mão com uma caixa, na boca da pilha
    hx, hy = 3.1, 1.4
    for s in (-1, 1):
        p.cyl('Dark', (hx, hy + s * 0.32, G + 0.18), 'y', 0.18, 0.08, seg=14, bevel=0.0)
        rod(p, 'Trim', (hx - 0.05, hy + s * 0.25, G + 0.12), (hx - 0.55, hy + s * 0.25, G + 1.35), 0.025, seg=5)
    p.box('Trim', hx - 0.1, hx + 0.25, hy - 0.3, hy + 0.3, G + 0.05, G + 0.09, bevel=0.0)
    crate(p, hx - 0.2, hy, G + 0.12, 70, 'Crate', size=(0.75, 0.55, 0.45))
    # a doca de descarga na frente (a picape encosta de ré em +X): o tablado na altura da caçamba sobre caixas, a rampa
    # de tábuas descendo pro pátio, os pneus de para-choque na face da frente, o poste com a lâmpada
    dx0, dx1, dy0, dy1, dz = 5.7, 7.55, -1.5, 1.5, G + 0.85
    for (x, y) in ((5.95, -1.2), (7.3, -1.2), (5.95, 1.2), (7.3, 1.2), (6.6, 0.0)):
        p.box('Wood', x - 0.22, x + 0.22, y - 0.22, y + 0.22, G, dz - 0.08, bevel=0.0)
    p.box('Wood', dx0, dx1, dy0, dy1, dz - 0.08, dz - 0.02, bevel=0.0)
    for k in range(9):
        y = dy0 + 0.17 + k * 0.33
        p.box('Wood', dx0, dx1, y - 0.15, y + 0.15, dz - 0.02, dz + 0.03, bevel=0.0)
    p.box('Dark', dx1, dx1 + 0.06, dy0, dy1, dz - 0.28, dz + 0.03, bevel=0.0)
    for y in (-1.0, 0.0, 1.0):
        tire(p, (dx1 + 0.2, y, dz - 0.35), R=0.36, r=0.12, axis='x')
    pts = [(dx0, -0.75, dz), (dx0, 0.75, dz), (dx0 - 1.6, 0.75, G + 0.02), (dx0 - 1.6, -0.75, G + 0.02)]
    for k in range(5):
        y0_, y1_ = -0.75 + k * 0.3, -0.75 + (k + 1) * 0.3 - 0.03
        prism(p, 'Wood', [(dx0, y0_, dz - 0.06), (dx0, y0_, dz + 0.0), (dx0 - 1.6, y0_, G + 0.06), (dx0 - 1.6, y0_, G)], (0.0, y1_ - y0_, 0.0))
    crate(p, 6.9, 0.7, dz + 0.03, 15, 'Crate', size=(0.9, 0.65, 0.45))
    crate(p, 6.3, -0.8, dz + 0.03, -8, 'Wood', size=(0.8, 0.6, 0.45))
    # 2 caixas soltas a caminho da pilha e um tambor
    crate(p, 3.2, -1.1, G, 22, 'Crate', size=(1.1, 0.8, 0.5))
    crate(p, 3.25, -1.05, G + 0.5, 30, 'Wood', size=(0.9, 0.65, 0.45))
    barrel(p, 6.6, -5.3, G, r=0.3, h=0.9, m='Rust')
    # o poste de luz de madeira com a lâmpada e o fio
    p.cyl('Wood', (7.3, -2.1, G + 1.9), 'z', 0.07, 3.8, seg=8, bevel=0.0)
    p.box('Wood', 6.75, 7.35, -2.15, -2.05, G + 3.6, G + 3.7, bevel=0.0)
    p.cyl('Light', (6.8, -2.1, G + 3.45), 'z', 0.1, 0.16, seg=10, bevel=0.0, smooth=True)
    p.cyl('Dark', (6.8, -2.1, G + 3.58), 'z', 0.16, 0.06, seg=10, r2=0.04, bevel=0.0)


def fence(p, y, x0, x1, side, seed=0):
    """A cerca de chapa ondulada remendada nos postes de madeira (alturas e cores de chapa variando, uma inclinada), a borda
    verde pintada no alto e o rabisco; side = +1 (a face de fora em +Y) ou −1."""
    n = int((x1 - x0) / 1.2)
    w = (x1 - x0) / n
    for k in range(n + 1):
        x = x0 + k * w
        if k % 2 == 0:
            p.cyl('Wood', (x, y, G + 1.15), 'z', 0.07, 2.3, seg=8, bevel=0.0)
    for zr in (0.45, 1.85):
        p.box('Wood', x0, x1, y - 0.05, y + 0.05, G + zr - 0.05, G + zr + 0.05, bevel=0.0)
    for k in range(n):
        xa = x0 + k * w + 0.02
        xb = xa + w - 0.04
        top = G + 2.0 + (_hash(k, seed, 51) - 0.5) * 0.35
        lean = (_hash(k, seed, 53) - 0.5) * 0.12
        m = 'Rust#sheet' if _hash(k, seed, 55) < 0.75 else 'Metal'
        yy = y + side * 0.08
        # a chapa de pé: ondulada na vertical (o corrugated do kit é deitado; aqui a onda vai em x)
        b = p.bm(m, 0.0)
        cols = 9
        vs_o, vs_i = [], []
        for c in range(cols + 1):
            x = xa + (xb - xa) * c / cols
            dy = 0.035 if c % 2 else -0.035
            vs_o.append((b.verts.new(V(x, yy + side * (0.02 + dy) + lean * 0.0, G + 0.05)), b.verts.new(V(x + lean, yy + side * (0.02 + dy), top))))
            vs_i.append((b.verts.new(V(x, yy + side * (-0.01 + dy), G + 0.05)), b.verts.new(V(x + lean, yy + side * (-0.01 + dy), top))))
        for c in range(cols):
            b.faces.new([vs_o[c][0], vs_o[c + 1][0], vs_o[c + 1][1], vs_o[c][1]])
            b.faces.new([vs_i[c][1], vs_i[c + 1][1], vs_i[c + 1][0], vs_i[c][0]])
        p.box('Player', min(xa, xb), max(xa, xb) + lean, yy + side * 0.06 - 0.01, yy + side * 0.06 + 0.01, top - 0.32, top - 0.08, bevel=0.0)


def walls(p):
    """O muro de adobe do fundo (a família da Oficina), os pilares da frente com as bandeiras, as cercas dos lados."""
    WB = -HX + 1.0
    wall_seg(p, (WB, -HY + 0.75), (WB, HY - 0.75), 2.4, t=0.55)
    for y in (-HY + 0.75, -3.0, 2.4, HY - 0.75):
        pilaster(p, WB, y, 2.95, rebar=(abs(y) > 7))
    scrawl(p, (WB - 0.28, 0.6, G + 1.3), '-x', size=1.6, seed=1)
    drift(p, (WB - 0.28, -6.9), (WB - 0.28, 6.9), (-1, 0), seed=3)
    # as cercas dos lados (do muro até perto da frente)
    fence(p, HY - 0.75, WB + 0.4, 4.6, 1, seed=1)
    fence(p, -HY + 0.75, WB + 0.4, 4.6, -1, seed=2)
    scrawl(p, (0.0, HY - 0.75 + 0.1, G + 1.15), '+y', size=1.5, seed=2)
    scrawl(p, (-2.8, -HY + 0.75 - 0.1, G + 1.15), '-y', size=1.5, seed=3)
    # os pilares de adobe da frente (fim das cercas), com o trecho curto de muro e as bandeiras
    for s in (-1, 1):
        wall_seg(p, (4.6, s * (HY - 0.75)), (7.2, s * (HY - 0.75)), 2.4, t=0.55)
        pilaster(p, 7.2, s * (HY - 0.75), 3.1, s=0.5)
        flagpole(p, 7.1, s * (HY - 0.75), G + 3.1, 2.6, length=1.2, height=0.75, phase=s * 0.9)
        lamp(p, 7.76, s * (HY - 0.75), G + 2.3, face='+x')
        pilaster(p, 4.6, s * (HY - 0.75), 2.95)


def base():
    p = new_part(PREFIX + '_Base')
    open_box(p, 'Earth', -HX, HX, -HY, HY, -1.2, G, bevel=0.08)
    ring_boxes(p, 'Dark', -HX - 0.03, HX + 0.03, -HY - 0.03, HY + 0.03, G - 0.12, G - 0.09, 0.25, bevel=0.0)
    walls(p)
    the_pile(p)
    shaft_and_crane(p)
    scale_and_unloading(p)
    return finish(p)


def main():
    clear_scene()
    total = export(base(), OUT)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -HX, HX, -HY, HY, 3.4, post='Wood', plank='Crate', stripe='Hazard'), OUT)
    print('MODEL total', total, 'tris')
    zt = max(G + 0.06, pile_h(NET_C[0], NET_C[1], 1.15) + 0.12) + 0.75
    print('MODEL lote 16 x 16 m; a pilha ate', round(max(b_[4] for b_ in BOXES), 2), 'm; o pico da rede', round(zt, 2), 'm; a bandeira ate',
          round(zt + 2.3, 2), 'm; o A de ferro', CRANE_TOP + 0.26, 'm')


main()
