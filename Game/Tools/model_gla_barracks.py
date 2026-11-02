"""O Quartel (Barracks) da GLA provisório, modelado no Blender por script (o elenco das 3 facções, 31/10/2026: docs/ELENCO.md §5.1).

Rodar (Blender sem janela, só CPU; não usa a placa):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_gla_barracks.py -- captures/game/models/gla_barracks

A palavra: O PÁTIO DE TREINO COM A PISTA DE PNEUS E OS SACOS DE AREIA, junto da casa de adobe de 2 andares.
A inspiração real: os campos de treino de guerrilha (a pista de pneus deitados em zigue-zague, os muros de sacos pra pular,
a trincheira em zigue-zague forrada de sacos, o arame baixo pra rastejar, a barra de macaco, o paredão de tábuas, a torre
de escalada com a rede de corda e a corda com nós, o estande de tiro com os alvos redondos e as silhuetas contra a barreira
de terra) dentro do pátio murado de uma casa de adobe; a família da Oficina aprovada (a borda verde no alto dos
muros, os pilaretes com capa, as bandeiras, os rabiscos, a escada maciça por fora).
- De cima: a casa no canto de trás-direita e o pátio em L na frente; o que lê primeiro é a pista de pneus (as 2 fileiras
  de círculos escuros), a trincheira em zigue-zague, o ninho redondo de sacos no canto da frente e o estande de tiro (a
  faixa com os alvos). (A 1ª volta tinha um poste alto com trave e 2 cordas: lia forca; virou a torre de escalada.)
- O que se mexe: as bandeiras, o fogo dos tambores (a fogueira, slot Fire) e as roupas no varal do teto.
- A função: os soldados saem pela porta da casa (+X) e pelo portão da frente (+X) do pátio.

Lote 18 × 22 m (X × Y), a frente em +X. Alturas: os muros do pátio 2,2 m (o pátio aparece por cima), a casa 6,2 m
(+ o parapeito e o posto de vigia de sacos), a torre de escalada 5,3 m (+ a bandeira).

Peças (origem no chão, no centro do lote; Unreal X frente, Y direita, Z cima, metros):
  SM_GLA_Barracks_Base: tudo;
  SM_GLA_Barracks_Scaffold: o andaime da obra em madeira.
Materiais (slots): Plaster, Earth, Rust, Wood, Cloth, Net, Crate, Metal, Player, Dark, Trim, Light, Fire (as brasas dos
  tambores de fogueira), Hazard (só no andaime).
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, flag, scaffold  # noqa: E402
from model_faction_kit import arch_door, barrel, crate_stack, lamp, railing, window  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'gla_barracks')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_GLA_Barracks'

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
HX, HY = 9.0, 11.0              # o lote 18 × 22
WF, WS, WB = 8.0, 10.0, -8.0    # os eixos dos muros (a face de fora a 8,3 / 10,3; a areia no pé até 8,9 / 10,9)
WALL, T = 2.2, 0.55
GATE = (3.4, 7.2)               # o portão da frente (y de/até)
HX0, HX1, HY0, HY1, HH = -7.7, -1.2, 3.0, 9.7, 6.2   # a casa (2 andares) no canto de trás-direita
ROOF = G + HH
DOOR_Y = 4.6


def house(p):
    """A casa de 2 andares: a porta com o toldo (+X), a sacada de madeira com o estandarte, as janelas com sacos de areia,
    as vigas, as canaletas; a escada maciça no lado −Y (o que a câmera do jogo vê); o teto com o posto de vigia de sacos,
    a caixa d'água, o varal e a bandeira."""
    # (31/10, o detalhe AAA e pronto pra degradar) a casa OCA: as paredes de adobe de 0,45 m com a quina gasta em volta dos vãos de
    # verdade (a porta, as janelas no vão fundo com a verga de madeira, a porta da sacada), a laje do teto e a do 2º andar, as
    # vigas de tronco que atravessam e saem pela frente, o esteio de dentro
    W_F = [(8.2, G + 1.1, 1.1), (8.2, G + 3.75, 1.1)]
    W_S = [(-2.4, G + 4.1, 1.05), (-4.0, G + 4.9, 1.05)]
    W_B = [(4.8, G + 3.75, 1.0), (7.9, G + 3.75, 1.0)]
    W_N = [(-2.8, G + 3.75, 1.0), (-5.8, G + 3.75, 1.0)]
    holes = {'+x': [(DOOR_Y - 0.85, DOOR_Y + 0.85, G, G + 2.3), (DOOR_Y - 0.55, DOOR_Y + 0.55, G + 3.5, G + 5.6)] +
                   [(y - 0.5, y + 0.5, z, z + h) for (y, z, h) in W_F],
             '-y': [(x - 0.5, x + 0.5, z, z + h) for (x, z, h) in W_S], '-x': [(y - 0.45, y + 0.45, z, z + h) for (y, z, h) in W_B],
             '+y': [(x - 0.45, x + 0.45, z, z + h) for (x, z, h) in W_N]}
    adobe_block(p, HX0, HX1, HY0, HY1, G, ROOF, t=0.45, holes=holes, parapet=0, beams=('+x',), beam_step=0.9, beam_out=0.38,
                beam_z=ROOF - 1.0, cols=[(-4.4, 6.3)])
    p.box('Plaster#slab', HX0 + 0.4, HX1 - 0.4, HY0 + 0.4, HY1 - 0.4, G + 3.2, G + 3.45, bevel=0.0)          # o piso do 2º andar
    for (f, at, ws, w_) in (('+x', HX1, W_F, 0.5), ('-y', HY0, W_S, 0.5), ('-x', HX0, W_B, 0.45), ('+y', HY1, W_N, 0.45)):
        F = WallFace(f, at, 0.0)
        for (u, z, h) in ws:
            adobe_window(p, F, u - w_, u + w_, z, z + h, t=0.45, shutter='Wood#shut' if z > 2 else None)
    Fh = WallFace('+x', HX1, 0.0)
    adobe_door(p, Fh, DOOR_Y - 0.85, DOOR_Y + 0.85, G, G + 2.3, t=0.45, open_frac=0.35)
    adobe_door(p, Fh, DOOR_Y - 0.55, DOOR_Y + 0.55, G + 3.5, G + 5.6, t=0.45, open_frac=0.5)
    ac_unit(p, WallFace('-y', HY0, 0.0), -6.9, G + 5.0)
    for (x, y) in ((HX1, HY0), (HX1, HY1), (HX0, HY0)):
        frustum(p, 'Plaster', x - 0.6, x + 0.6, y - 0.6, y + 0.6, G, ROOF + 0.9, 0.17)
        p.box('Plaster#lo', x - 0.48, x + 0.48, y - 0.48, y + 0.48, ROOF + 0.9, ROOF + 1.05)
    rebar_tuft(p, HX1, HY0, ROOF + 1.05, 0.25, seed=2)
    # o parapeito redondo (abre na chegada da escada no lado −Y) e a faixa verde por fora
    wall_seg(p, (HX1 - 0.18, HY0 + 0.18), (HX1 - 0.18, HY1 - 0.18), 0.55, t=0.34, z0=ROOF, band=False)
    wall_seg(p, (HX0 + 0.18, HY1 - 0.18), (HX1 - 0.18, HY1 - 0.18), 0.55, t=0.34, z0=ROOF, band=False)
    wall_seg(p, (HX0 + 0.18, HY0 + 0.18), (HX0 + 0.18, HY1 - 0.18), 0.55, t=0.34, z0=ROOF, band=False)
    wall_seg(p, (HX0 + 0.18, HY0 + 0.18), (-7.1, HY0 + 0.18), 0.55, t=0.34, z0=ROOF, band=False)
    wall_seg(p, (-5.0, HY0 + 0.18), (HX1 - 0.18, HY0 + 0.18), 0.55, t=0.34, z0=ROOF, band=False)
    zb0, zb1 = ROOF - 0.75, ROOF - 0.25
    paint_band_block(p, HX0, HX1, HY0, HY1, zb0, zb1, faces='+x-y-x', seed=5)
    spout(p, -3.3, HY0, ROOF + 0.05, '-y')
    spout(p, HX1, 8.6, ROOF + 0.05, '+x')
    # a frente (+X): a porta em arco com o toldo de chapa nos paus, a lâmpada, as janelas (as de baixo com sacos)
    corrugated_sheet = __import__('model_faction_kit').corrugated
    corrugated_sheet(p, HX1, HX1 + 1.6, DOOR_Y - 1.4, DOOR_Y + 1.4, G + 3.25, G + 2.95, axis='x', m='Rust#sheet', patch=True, seed=2)
    for s in (-1, 1):
        p.cyl('Wood', (HX1 + 1.5, DOOR_Y + s * 1.3, G + 1.48), 'z', 0.05, 2.95, seg=8, bevel=0.0)
    lamp(p, HX1, DOOR_Y + 1.7, G + 2.6, face='+x')
    bag_row(p, [(HX1 + 0.22, 7.7), (HX1 + 0.22, 8.7)], G + 1.1, layers=2, bag_size=(0.55, 0.32, 0.22), seed=3)
    # a sacada de madeira do 2º andar (sobre a porta), com o corrimão e o estandarte verde pendurado
    bx0, bx1 = HX1, HX1 + 1.15
    p.box('Wood', bx0, bx1, DOOR_Y - 1.5, DOOR_Y + 1.5, G + 3.35, G + 3.5, bevel=0.0)
    for y in (DOOR_Y - 1.3, DOOR_Y + 1.3):
        rod(p, 'Wood', (bx1 - 0.1, y, G + 3.35), (HX1, y, G + 2.6), 0.05)
    railing(p, [(bx0, DOOR_Y - 1.45), (bx1 - 0.05, DOOR_Y - 1.45), (bx1 - 0.05, DOOR_Y + 1.45), (bx0, DOOR_Y + 1.45)], G + 3.5, h=0.95, m='Wood', post_step=0.7)
    p.box('Player', bx1 - 0.02, bx1 + 0.01, DOOR_Y - 0.6, DOOR_Y + 0.6, G + 2.3, G + 4.4, bevel=0.0)
    p.cyl('Cloth', (bx1 + 0.02, DOOR_Y, G + 3.55), 'x', 0.3, 0.02, seg=20, bevel=0.0)
    # o lado −Y: as janelas (a de baixo emparedada com sacos), o rabisco, o ar-condicionado
    # a escada maciça do lado −Y: do pátio (frente) até o teto, a mureta triangular
    x_lo, x_hi, n = HX1 - 0.25, -6.3, 20
    run = (x_lo - x_hi) / n
    rise = HH / n
    y0, y1 = HY0 - 1.2, HY0
    for k in range(n):
        xa = x_lo - k * run
        p.box('Plaster#lo', xa - run, xa, y0 + 0.25, y1, G, G + (k + 1) * rise)
    prism(p, 'Plaster', [(x_lo + 0.05, y0, G), (x_lo + 0.05, y0, G + 0.95), (x_hi, y0, ROOF + 0.95), (x_hi, y0, G)], (0.0, 0.27, 0.0))
    p.box('Plaster', x_hi - 0.9, x_hi, y0 + 0.25, y1, G, ROOF, bevel=0.04)
    p.box('Plaster', x_hi - 0.9, x_hi, y0, y0 + 0.27, G, ROOF + 0.95, bevel=0.04)
    scrawl(p, (-4.0, y0, G + 1.35), '-y', size=1.4, seed=7)
    # o teto: o posto de vigia de sacos (canto da frente +Y) com a lona em cima, a caixa d'água, o varal, a chaminé, a bandeira
    bag_row(p, [(-1.75, 6.9), (-1.75, 9.1), (-4.0, 9.1)], ROOF, layers=3, seed=5)
    for (x, y) in ((-1.8, 6.9), (-4.0, 9.15), (-4.0, 6.9)):
        p.cyl('Wood', (x, y, ROOF + 1.15), 'z', 0.05, 2.3, seg=8, bevel=0.0)
    tarp_sheet(p, [(-4.1, 6.8, ROOF + 2.3), (-1.7, 6.8, ROOF + 2.1), (-1.7, 9.25, ROOF + 2.1), (-4.1, 9.25, ROOF + 2.3)], sag=0.1)
    water_tank(p, -6.5, 8.4, ROOF, r=0.7, h=1.3, stand=0.6)
    stovepipe(p, -6.9, 4.1, ROOF, 1.6, r=0.16)
    for (x, y) in ((-6.2, 4.3), (-6.2, 7.2)):
        p.cyl('Wood', (x, y, ROOF + 0.95), 'z', 0.035, 1.9, seg=6, bevel=0.0)
    rod(p, 'Dark', (-6.2, 4.3, ROOF + 1.85), (-6.2, 7.2, ROOF + 1.85), 0.01, seg=4)
    for k, m in enumerate(('Cloth', 'Player', 'Cloth', 'Crate', 'Cloth')):
        y = 4.7 + k * 0.55
        p.obox(m, (-6.2, y, ROOF + 1.55), (0.02, 0.42, 0.6 - 0.08 * (k % 2)), roll=(k % 3 - 1) * 4, bevel=0.0)
    flagpole(p, HX0 + 0.5, HY1 - 0.5, ROOF + 0.55, 2.5, length=1.4, height=0.9, phase=0.7)


def walls(p):
    """Os muros baixos do pátio (2,2 m, a borda verde, o topo redondo), os pilaretes, o portão aberto com as bandeiras."""
    wall_seg(p, (WF, -WS), (WF, GATE[0]), WALL, t=T)
    wall_seg(p, (WF, GATE[1]), (WF, WS), WALL, t=T)
    wall_seg(p, (WF, -WS), (WB, -WS), WALL, t=T)
    wall_seg(p, (HX1 + 0.3, WS), (WF, WS), WALL, t=T)
    wall_seg(p, (WB, -WS), (WB, HY0 + 0.2), WALL, t=T)
    for (x, y) in ((WF, -WS), (WB, -WS)):
        pilaster(p, x, y, WALL + 0.6, s=0.5, rebar=True)
    for y in (-6.2, -2.0):
        pilaster(p, WF, y, WALL + 0.45)
    pilaster(p, WF, 8.6, WALL + 0.45)
    for x in (4.2, 0.0, -4.2):
        pilaster(p, x, -WS, WALL + 0.45)
    pilaster(p, WB, -3.0, WALL + 0.45)
    pilaster(p, 3.4, WS, WALL + 0.45)
    # o portão: os 2 pilares altos com a capa, as bandeiras e as lâmpadas; a viga de madeira em cima com o pano verde
    for y in GATE:
        pilaster(p, WF, y, 3.6, s=0.55)
        lamp(p, WF + 0.67, y, G + 2.6, face='+x')
    flagpole(p, WF - 0.1, GATE[0], G + 3.6, 2.6, length=1.3, height=0.8, phase=0.2)
    flagpole(p, WF - 0.1, GATE[1], G + 3.6, 2.6, length=1.3, height=0.8, phase=1.6)
    p.box('Wood', WF - 0.15, WF + 0.15, GATE[0], GATE[1], G + 3.2, G + 3.42, bevel=0.0)
    p.box('Player', WF + 0.15, WF + 0.17, GATE[0] + 0.7, GATE[1] - 0.7, G + 2.45, G + 3.2, bevel=0.0)
    # os rabiscos de fora (a frente e o lado −Y, o que a câmera vê) e a areia no pé
    scrawl(p, (WF + T / 2, -4.0, G + 1.2), '+x', size=1.5, seed=1)
    scrawl(p, (WF + T / 2, 1.6, G + 1.15), '+x', size=1.3, seed=2)
    scrawl(p, (2.0, -WS - T / 2, G + 1.2), '-y', size=1.6, seed=3)
    scrawl(p, (-6.0, -WS - T / 2, G + 1.2), '-y', size=1.3, seed=4)
    drift(p, (WF + T / 2, -WS + 0.5), (WF + T / 2, GATE[0] - 0.6), (1, 0), seed=1)
    drift(p, (6.6, -WS - T / 2), (-7.6, -WS - T / 2), (0, -1), seed=2)
    drift(p, (WB - T / 2, -WS + 0.5), (WB - T / 2, 2.5), (-1, 0), seed=3)


def training_yard(p):
    """O pátio de treino: a pista de pneus, os muros de sacos pra pular, a trincheira em zigue-zague forrada de sacos, o
    arame baixo, o paredão de tábuas, a torre de escalada com a rede de corda, a barra de macaco, o estande de tiro (a
    linha de sacos, os alvos e a barreira de terra), o ninho redondo de sacos do canto e os tambores de fogueira."""
    # a pista de pneus: 2 fileiras em zigue-zague, deitados (os círculos que leem de cima), e uma pilha de reserva
    for k in range(8):
        x = 6.5 - k * 0.92
        for j in (0, 1):
            y = -4.35 + j * 0.95 + (0.42 if k % 2 else 0.0)
            tire(p, (x, y, G + 0.14), R=0.42, r=0.15)
    for k in range(4):
        tire(p, (6.85, -1.5 + 0.03 * (k % 2), G + 0.14 + k * 0.28), R=0.42, r=0.15)
    # os 2 muros de sacos pra pular, logo depois dos pneus
    for x in (-0.6, -2.0):
        bag_row(p, [(x, -4.75), (x, -2.55)], G, layers=4, seed=int(-x * 10))
    # a trincheira de treino em zigue-zague (o chão escuro afundado e os sacos dos 2 lados): de cima é a linha que lê
    zz = [(5.6, 1.9), (3.6, 0.6), (1.6, 1.9), (-0.4, 0.6)]
    for kk, (a, b) in enumerate(zip(zz[:-1], zz[1:])):
        L = math.dist(a, b)
        yaw = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
        p.obox('Dark', ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, G + 0.004 + 0.004 * kk), (L + 0.6, 0.75, 0.01), yaw=yaw, bevel=0.0)
        nx, ny = -(b[1] - a[1]) / L, (b[0] - a[0]) / L
        for s in (-1, 1):
            bag_row(p, [(a[0] + nx * s * 0.62, a[1] + ny * s * 0.62), (b[0] + nx * s * 0.62, b[1] + ny * s * 0.62)], G, layers=2,
                    seed=int(a[0] * 10) + s)
    # o arame baixo pra rastejar: as estacas e os fios (entre os pneus e a trincheira)
    for i in range(4):
        for j in range(2):
            x, y = 1.4 + i * 1.5, -1.75 + j * 1.0
            p.cyl('Wood', (x, y, G + 0.3), 'z', 0.035, 0.6, seg=6, bevel=0.0)
            if i < 3:
                rod(p, 'Dark', (x, y, G + 0.55), (x + 1.5, y, G + 0.55), 0.01, seg=4)
                rod(p, 'Dark', (x, y, G + 0.55), (x + 1.5, y + (1.0 if j == 0 else -1.0), G + 0.55), 0.008, seg=4)
    # o paredão de tábuas em A (subir e descer)
    ax_, ay0, ay1 = -3.9, -4.8, -2.0
    for s in (-1, 1):
        p.obox('Wood', (ax_ + s * 0.55, (ay0 + ay1) / 2, G + 1.45), (0.12, ay1 - ay0, 3.1), pitch=s * 20, bevel=0.0)
        for k in range(5):
            z = G + 0.4 + k * 0.55
            xx = ax_ + s * (0.55 + math.tan(math.radians(20)) * (1.45 - (z - G))) + s * 0.07
            p.obox('Dark', (xx, (ay0 + ay1) / 2, z), (0.03, ay1 - ay0 + 0.02, 0.06), pitch=s * 20, bevel=0.0)
    for y in (ay0 + 0.1, ay1 - 0.1):
        rod(p, 'Wood', (ax_, y, G + 2.95), (ax_, y, G), 0.06)
    # a torre de escalada: o tronco com a plataforma no alto, a rede de corda inclinada até a trave no chão, a corda
    px, py = -6.3, -1.3
    p.cyl('Wood', (px, py, G + 2.8), 'z', 0.14, 5.6, seg=12, bevel=0.0)
    for (dx, dy) in ((-0.45, -0.45), (0.45, -0.45), (0.45, 0.45), (-0.45, 0.45)):
        rod(p, 'Wood', (px + dx * 0.2, py + dy * 0.2, G + 4.6), (px + dx, py + dy, G + 4.95), 0.045)
    p.box('Wood', px - 0.6, px + 0.6, py - 0.6, py + 0.6, G + 4.95, G + 5.07, bevel=0.0)
    railing(p, [(px - 0.6, py - 0.6), (px + 0.6, py - 0.6), (px + 0.6, py + 0.6)], G + 5.07, h=0.8, m='Wood', post_step=0.6)
    gx0, gy = px + 0.6, -3.9                                                    # a rede desce pra −Y, a trave no chão
    p.box('Wood', px - 0.7, px + 0.7, gy - 0.1, gy + 0.1, G, G + 0.2, bevel=0.0)
    for k in range(6):
        u = k / 5
        x = px - 0.6 + 1.2 * u
        rod(p, 'Cloth', (x, py - 0.6, G + 5.0), (x, gy, G + 0.2), 0.022, seg=4)
    for k in range(1, 8):
        t = k / 8
        y = py - 0.6 + (gy - py + 0.6) * t
        z = G + 5.0 + (0.2 - 5.0) * t
        rod(p, 'Cloth', (px - 0.6, y, z), (px + 0.6, y, z), 0.02, seg=4)
    rod(p, 'Cloth', (px + 0.62, py + 0.3, G + 5.0), (px + 0.66, py + 0.3, G + 0.4), 0.035, seg=6)
    for z in (1.4, 2.4, 3.4):
        p.cyl('Cloth', (px + 0.65, py + 0.3, G + z), 'z', 0.07, 0.12, seg=8, bevel=0.0)
    p.cyl('Trim', (px, py, G + 6.2), 'z', 0.035, 2.3, seg=6, bevel=0.0)
    flag(p, (px, py), G + 7.25, 1.0, 0.6, m='Player', phase=2.2)
    # a barra de macaco (o escadote deitado nos 2 pórticos de cano), encostada na escada da casa
    for x in (-5.4, -1.8):
        for y in (0.7, 1.5):
            p.cyl('Trim', (x, y, G + 1.3), 'z', 0.05, 2.6, seg=8, bevel=0.0)
    for y in (0.7, 1.5):
        rod(p, 'Trim', (-5.4, y, G + 2.55), (-1.8, y, G + 2.55), 0.04, seg=6)
    for k in range(10):
        x = -5.25 + k * 0.38
        rod(p, 'Trim', (x, 0.7, G + 2.55), (x, 1.5, G + 2.55), 0.022, seg=5)
    # o estande de tiro (a faixa −Y): a barreira de terra contra o muro de trás, os alvos, a linha de sacos, a bancada
    prism(p, 'Earth#drift', [(WB + 0.3, -9.6, G - 0.05), (WB + 2.4, -9.6, G - 0.05), (WB + 0.3, -9.6, G + 1.7)], (0.0, 4.0, 0.0))
    for (y, kind) in ((-9.0, 'round'), (-8.0, 'man'), (-7.0, 'round'), (-6.1, 'man')):
        x = WB + 2.6
        for s in (-1, 1):
            rod(p, 'Wood', (x, y + s * 0.35, G), (x, y + s * 0.35, G + 1.3), 0.035)
        if kind == 'round':
            p.box('Wood', x - 0.03, x + 0.03, y - 0.42, y + 0.42, G + 0.85, G + 1.75, bevel=0.0)
            p.cyl('Cloth', (x + 0.04, y, G + 1.3), 'x', 0.38, 0.02, seg=24, bevel=0.0)
            for (r, m) in ((0.27, 'Dark'), (0.18, 'Cloth'), (0.08, 'Player')):
                p.cyl(m, (x + 0.05 + (0.3 - r) * 0.02, y, G + 1.3), 'x', r, 0.02, seg=20, bevel=0.0)
        else:
            sil = [(-0.32, 0.85), (0.32, 0.85), (0.3, 1.35), (0.13, 1.45), (0.13, 1.6), (0.0, 1.72), (-0.13, 1.6), (-0.13, 1.45), (-0.3, 1.35)]
            prism(p, 'Wood', [(x, y + a, G + b) for (a, b) in sil], (0.05, 0.0, 0.0))
            p.cyl('Dark', (x + 0.06, y, G + 1.2), 'x', 0.1, 0.01, seg=12, bevel=0.0)
    bag_row(p, [(3.0, -9.6), (3.0, -6.0)], G, layers=3, seed=9)
    p.box('Wood', 3.6, 4.4, -9.3, -8.1, G + 0.75, G + 0.82, bevel=0.0)
    for (x, y) in ((3.7, -9.2), (4.3, -9.2), (3.7, -8.2), (4.3, -8.2)):
        p.box('Wood', x - 0.04, x + 0.04, y - 0.04, y + 0.04, G, G + 0.75, bevel=0.0)
    crate_stack(p, 4.0, -8.75, G + 0.82, 1, yaw=0, size=(0.6, 0.35, 0.25))
    for k in range(3):                                              # as estacas das distâncias, com o pano verde
        x = -1.0 - k * 3.0
        p.cyl('Wood', (x, -9.6, G + 0.45), 'z', 0.03, 0.9, seg=6, bevel=0.0)
        p.box('Player', x - 0.01, x + 0.01, -9.6, -9.35, G + 0.65, G + 0.85, bevel=0.0)
    # o ninho redondo de sacos no canto da frente (−Y) com a metralhadora apontada pra fora
    bag_ring(p, (6.0, -8.0), 1.45, G, layers=3, gap=(80, 125), seed=6)
    dshk(p, 6.0, -8.0, G, yaw=-40)
    # os tambores de fogueira (as brasas acesas, slot Fire)
    for (x, y) in ((1.6, -6.6), (6.7, 2.7)):
        barrel(p, x, y, G, r=0.3, h=0.9, m='Rust')
        p.cyl('Fire', (x, y, G + 0.88), 'z', 0.25, 0.04, seg=16, bevel=0.0)
        for k in range(3):
            p.obox('Dark', (x + 0.1 * math.cos(k * 2.1), y + 0.1 * math.sin(k * 2.1), G + 0.93), (0.4, 0.07, 0.07), yaw=k * 60, bevel=0.0)


def shelter(p):
    """A lona-abrigo perto do portão: os bancos, o cavalete com os fuzis, o caixote de munição, o tambor de fogueira."""
    tarp_sheet(p, [(1.6, 7.5, G + 2.7), (6.6, 7.5, G + 2.45), (6.6, 9.6, G + 2.45), (1.6, 9.6, G + 2.7)], sag=0.15)
    for (x, y) in ((1.7, 7.6), (6.5, 7.6)):
        p.cyl('Wood', (x, y, G + 1.3), 'z', 0.06, 2.6, seg=8, bevel=0.0)
    for y in (8.0, 9.1):
        p.box('Wood', 2.2, 5.9, y - 0.18, y + 0.18, G + 0.45, G + 0.52, bevel=0.0)
        for x in (2.4, 5.7):
            p.box('Wood', x - 0.05, x + 0.05, y - 0.15, y + 0.15, G, G + 0.45, bevel=0.0)
    # o cavalete dos fuzis (as AKs encostadas)
    p.box('Wood', 3.1, 3.2, 9.45, 9.55, G, G + 1.2, bevel=0.0)
    p.box('Wood', 5.0, 5.1, 9.45, 9.55, G, G + 1.2, bevel=0.0)
    p.box('Wood', 3.1, 5.1, 9.4, 9.55, G + 1.05, G + 1.15, bevel=0.0)
    for k in range(7):
        x = 3.3 + k * 0.27
        p.obox('Dark', (x, 9.32, G + 0.6), (0.05, 0.06, 1.0), roll=-12, bevel=0.0)
        p.obox('Wood', (x, 9.24, G + 0.2), (0.06, 0.1, 0.28), roll=-12, bevel=0.0)
    crate_stack(p, 6.0, 8.6, G, 2, yaw=10, size=(0.9, 0.5, 0.4))


def base():
    p = new_part(PREFIX + '_Base')
    open_box(p, 'Earth', -HX, HX, -HY, HY, -1.2, G, bevel=0.08)
    ring_boxes(p, 'Dark', -HX - 0.03, HX + 0.03, -HY - 0.03, HY + 0.03, G - 0.12, G - 0.09, 0.25, bevel=0.0)
    walls(p)
    house(p)
    training_yard(p)
    shelter(p)
    # o caminho batido da porta ao portão (as pedras)
    for k in range(7):
        x = -0.4 + k * 1.25
        y = DOOR_Y + (GATE[0] + GATE[1] - 2 * DOOR_Y) / 2 * (k / 6) + (_hash(k, 1, 31) - 0.5) * 0.3
        p.obox('Plaster#lo', (x, y, G + 0.02), (0.8, 0.95, 0.06), yaw=_hash(k, 2, 35) * 25)
    return finish(p)


def main():
    clear_scene()
    total = export(base(), OUT)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -HX, HX, -HY, HY, ROOF + 0.6, post='Wood', plank='Crate', stripe='Hazard'), OUT)
    print('MODEL total', total, 'tris')
    print('MODEL lote 18 x 22 m; a casa', round(ROOF, 2), 'm; o muro', round(G + WALL, 2), 'm; a torre de escalada', round(G + 5.07, 2), 'm')


main()
