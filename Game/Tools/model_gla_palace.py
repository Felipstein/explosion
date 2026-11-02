"""O Palácio da GLA (o centro de tecnologia) provisório, modelado no Blender por script (o elenco das 3 facções, 31/10/2026:
docs/ELENCO.md §5.1).

Rodar (Blender sem janela, só CPU; não usa a placa):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_gla_palace.py -- captures/game/models/gla_palace

A palavra: O PALÁCIO DE CÚPULA. A cúpula grande de azulejo turquesa gasto (faltam pedaços: o reboco aparece) com a faixa
verde, no meio do bloco do palácio; na frente a arcada de colunas com o terraço em cima; o pátio murado com a fonte seca no
cruzamento dos 2 caminhos (o jardim em cruz dos palácios persas e mouriscos) e as palmeiras cortadas nos vasos; os muros com
as ameias redondas. É a riqueza tomada à força pela GLA: os tapetes pendurados no terraço e nas sacadas, os sacos de areia
nas janelas, a metralhadora no teto, o estandarte verde enorme na fachada, o carro de luxo roubado debaixo da lona.
- Não é mesquita (o pedido): sem minarete, sem crescente; o remate da cúpula é uma bola de latão; o que lê é palácio (a
  arcada com o terraço, as sacadas, o jardim, as torrinhas dos cantos, o estandarte).
- De cima (a regra 2 do §3b): o círculo grande turquesa (a maior bola da base GLA) e o octógono da fonte no meio da cruz.
- O que se mexe: as bandeiras e o estandarte; a fonte (seca) pode ganhar água quando a pesquisa termina, se quiserem.

Lote 18 × 24 m (X × Y), a frente em +X. Alturas: os muros 2,6 m (+ as ameias), o palácio 6,6 m (+ o parapeito e as
torrinhas a 8,1 m), a cúpula até ~11,6 m e o remate até ~12,2 m.

Peças (origem no chão, no centro do lote; Unreal X frente, Y direita, Z cima, metros):
  SM_GLA_Palace_Base: tudo;
  SM_GLA_Palace_Scaffold: o andaime da obra em madeira.
Materiais (slots): Plaster, Earth, Tile (o azulejo turquesa: o ÚNICO slot novo; se o importador não tiver, cai no CnRoof com
  a cor turquesa), Player (o verde), CnGold (o latão do remate, das lâmpadas e da bica da fonte), Rust (os vasos de barro, a
  chapa), Wood, Cloth (os tapetes, a lona, os sacos), Crate, Metal, Dark, Trim, Light, Hazard (só no andaime).
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, flag, scaffold  # noqa: E402
from model_faction_kit import arch_door, band, barrel, crate_stack, disc_ring, dome, lamp, railing, revolve, window  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'gla_palace')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_GLA_Palace'

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

COLORS.update({'Tile': (62, 148, 146)})        # o azulejo turquesa gasto (só no palácio)

# ------------------------------------------------------------------------------------------------- medidas
HX, HY = 9.0, 12.0              # o lote 18 × 24
WF, WS, WB = 8.0, 11.0, -8.0    # os eixos dos muros (a face de fora a 8,3 / 11,3; a areia até 8,9 / 11,9)
WALL, T = 2.6, 0.6
GATE = 2.3                      # meia-largura do portão
PX0, PX1, PY, PH = -7.7, 1.0, 7.4, 6.6   # o bloco do palácio (fundo encostado no muro de trás)
ROOF = G + PH
DOME_C = (-3.35, 0.0)
DRUM_R, DRUM_H, DOME_R, DOME_H = 3.95, 1.15, 3.75, 3.3
ARC_X1 = 3.4                    # a frente da arcada
ARC_H = 4.3                     # o terraço em cima da arcada
PT = 0.55                       # (31/10) a espessura das paredes do palácio (o adobe oco: pronto pro furo do dano)
COLS = [-6.0, -3.6, -1.2, 1.2, 3.6, 6.0]
FOUNT = (5.6, 0.0)
FOUNT_A = 1.5                   # o apótema do octógono da fonte


def tile_dome(p):
    """A cúpula: o casco de reboco e, por cima, a camada de azulejo turquesa que falta aos pedaços (o reboco aparece nos
    buracos: o "gasto" sem textura pintada), a faixa verde na base, as nervuras de latão, o remate de bola."""
    cx, cy = DOME_C
    z0 = ROOF + DRUM_H + 0.1
    dome(p, 'Plaster#s', (cx, cy), DOME_R, DOME_H, z0, seg=56, rings=14)

    def fn(u, v):
        a = 2 * math.pi * u
        t = 0.04 + 0.9 * v
        ang = t * math.pi / 2
        rr = (DOME_R + 0.035) * math.cos(ang)
        return (cx + rr * math.cos(a), cy + rr * math.sin(a), z0 + (DOME_H + 0.035) * math.sin(ang))
    net_surface(p, fn, 80, 18, seed=17, blotch=0.62, holes=0.31, light=9.0, ragged=0.0, thick=0.02, closed_u=True,
                mats=('Tile', 'Tile'))
    # as 16 nervuras de reboco (os gomos da cúpula, como as de palácio mouriscas), do friso até o remate
    for k in range(16):
        a = 2 * math.pi * k / 16
        rings = []
        for j in range(9):
            ang = (0.12 + 0.82 * j / 8) * math.pi / 2
            rr = (DOME_R + 0.05) * math.cos(ang)
            zz = z0 + (DOME_H + 0.05) * math.sin(ang)
            nx_, nz_ = math.cos(ang) / DOME_R, math.sin(ang) / DOME_H
            ln = math.hypot(nx_, nz_)
            nx_, nz_ = nx_ / ln, nz_ / ln
            w = 0.07 * (1.0 - 0.6 * j / 8)
            tx, ty = -math.sin(a), math.cos(a)
            px, py = cx + rr * math.cos(a), cy + rr * math.sin(a)
            ox, oy, oz = nx_ * math.cos(a) * 0.05, nx_ * math.sin(a) * 0.05, nz_ * 0.05
            rings.append([(px - tx * w, py - ty * w, zz), (px + tx * w, py + ty * w, zz),
                          (px + tx * w + ox, py + ty * w + oy, zz + oz), (px - tx * w + ox, py - ty * w + oy, zz + oz)])
        p.loft('Plaster#s', rings, bevel=0.0)
    # a faixa verde larga na base da cúpula e o friso de reboco em cima dela
    disc_ring(p, 'Player', (cx, cy), DOME_R + 0.06, DOME_R - 0.2, z0 - 0.02, z0 + 0.55, seg=56)
    disc_ring(p, 'Plaster', (cx, cy), DOME_R + 0.1, DOME_R - 0.2, z0 + 0.55, z0 + 0.7, seg=56)
    # o remate: o pescoço, a bola de latão e a ponta (sem crescente)
    zt = z0 + DOME_H
    revolve(p, 'Plaster#s', (cx, cy), [(0.75, zt - 0.12), (0.5, zt + 0.12), (0.32, zt + 0.32), (0.3, zt + 0.42), (0.02, zt + 0.44)], seg=24)
    revolve(p, 'CnGold', (cx, cy), [(0.02, zt + 0.4), (0.22, zt + 0.48), (0.3, zt + 0.62), (0.22, zt + 0.78), (0.06, zt + 0.86),
                                     (0.04, zt + 1.15), (0.01, zt + 1.2)], seg=20)


def palace_block(p):
    """O bloco do palácio: as torrinhas dos cantos, o parapeito com ameias redondas e a faixa verde, o tambor com as
    janelinhas em arco, as janelas grandes em arco (as de baixo com sacos), as sacadas dos lados com os tapetes."""
    # (31/10, o detalhe AAA e pronto pra degradar) o palácio OCO: as paredes de adobe de 0,55 m com a quina gasta em volta dos vãos
    # de verdade (a porta grande e as janelas em ARCO no vão fundo, com os tímpanos de adobe na espessura), a laje, o 2º andar, as
    # vigas e os esteios por dentro
    FR = [(y, G + 1.0, 1.0, 1.7) for y in (-4.8, -2.4, 2.4, 4.8)] + [(y, ARC_H + G + 0.62, 0.9, 1.2) for y in (-4.8, -2.4, 2.4, 4.8)]
    SD = [(-6.2, G + 1.0, 1.0, 1.6), (-1.2, G + 1.0, 1.0, 1.6), (-6.2, G + 4.0, 0.9, 1.3)]
    BK = [(y, G + 4.0, 0.9, 1.3) for y in (-4.5, 0.0, 4.5)]
    hl = lambda ws: [(u - w / 2, u + w / 2, z, z + h) for (u, z, w, h) in ws]
    holes = {'+x': hl(FR) + [(-1.2, 1.2, G, G + 4.2)], '-y': hl(SD) + [(-4.0, -2.8, G + 3.6, G + 5.6)],
             '+y': hl(SD) + [(-4.0, -2.8, G + 3.6, G + 5.6)], '-x': hl(BK)}
    adobe_block(p, PX0, PX1, -PY, PY, G, ROOF, t=PT, holes=holes, parapet=0, beams=('in',), beam_step=1.0,
                cols=[(-5.0, -3.5), (-5.0, 3.5), (-1.7, -3.5), (-1.7, 3.5)])
    p.box('Plaster#slab', PX0 + PT, PX1 - PT, -PY + PT, PY - PT, ARC_H + G - 0.05, ARC_H + G + 0.2, bevel=0.0)  # o 2º andar
    arch_fill(p, 'Plaster#arch', (PX1 - PT / 2, 0.0), 'x', 2.4, G + 3.0, G + 4.2, PT)                    # o arco da porta grande
    adobe_door(p, WallFace('+x', PX1, 0.0), -1.2, 1.2, G, G + 3.0, t=PT, open_frac=0.25)
    for (f, at) in (('-y', -PY), ('+y', PY)):
        adobe_door(p, WallFace(f, at, 0.0), -4.0, -2.8, G + 3.6, G + 5.6, t=PT, open_frac=0.4)
    zb0, zb1 = ROOF - 0.8, ROOF - 0.25
    paint_band_block(p, PX0, PX1, -PY, PY, zb0, zb1, faces='+x-y+y', seed=9)
    p.box('Plaster#lo', PX0 - 0.05, PX1 + 0.12, -PY - 0.12, PY + 0.12, ROOF - 0.1, ROOF + 0.1)        # a cornija
    # o parapeito com as ameias redondas
    for (a_, b_) in (((PX1 - 0.2, -PY + 1.0), (PX1 - 0.2, PY - 1.0)), ((PX0 + 1.0, -PY + 0.2), (PX1 - 1.0, -PY + 0.2)),
                     ((PX0 + 1.0, PY - 0.2), (PX1 - 1.0, PY - 0.2)), ((PX0 + 0.2, -PY + 1.0), (PX0 + 0.2, PY - 1.0))):
        wall_seg(p, a_, b_, 0.45, t=0.35, z0=ROOF + 0.1, band=False, round_top=False)
        round_merlons(p, a_, b_, ROOF + 0.55, w=0.5, t=0.35, h=0.42)
    # as torrinhas dos 4 cantos (sobem 1,5 m acima do teto, as ameias redondas, a faixa verde)
    for (x, y) in ((PX1, -PY), (PX1, PY), (PX0, -PY), (PX0, PY)):
        frustum(p, 'Plaster', x - 0.95, x + 0.95, y - 0.95, y + 0.95, G, ROOF + 1.3, 0.12)
        p.box('Plaster#lo', x - 0.95, x + 0.95, y - 0.95, y + 0.95, ROOF + 1.3, ROOF + 1.45)
        sx, sy = (1 if x > 0 else -1), (1 if y > 0 else -1)
        band(p, x - 0.83, x + 0.83, y - 0.83, y + 0.83, ROOF + 0.75, ROOF + 1.15)
        for (a_, b_) in (((x - 0.75, y - 0.78), (x + 0.75, y - 0.78)), ((x - 0.75, y + 0.78), (x + 0.75, y + 0.78)),
                         ((x - 0.78, y - 0.5), (x - 0.78, y + 0.5)), ((x + 0.78, y - 0.5), (x + 0.78, y + 0.5))):
            round_merlons(p, a_, b_, ROOF + 1.45, n=2, w=0.42, t=0.3, h=0.36)
        window(p, x + sx * 0.83, y, G + 4.6, '+x' if sx > 0 else '-x', w=0.3, h=0.9, frame='Plaster#s')
    # o tambor da cúpula: as janelinhas em arco escuras, o friso de azulejo
    cx, cy = DOME_C
    p.cyl('Plaster#s', (cx, cy, ROOF + DRUM_H / 2 + 0.05), 'z', DRUM_R, DRUM_H + 0.1, seg=56, bevel=0.0, smooth=True)
    disc_ring(p, 'Tile', (cx, cy), DRUM_R + 0.03, DRUM_R - 0.1, ROOF + DRUM_H - 0.2, ROOF + DRUM_H + 0.1, seg=56)
    for k in range(16):
        a = 2 * math.pi * (k + 0.5) / 16
        q = (cx + (DRUM_R - 0.03) * math.cos(a), cy + (DRUM_R - 0.03) * math.sin(a))
        p.obox('Dark', (q[0], q[1], ROOF + 0.45), (0.08, 0.36, 0.42), yaw=math.degrees(a), bevel=0.0)
        p.cyl('Dark', (q[0], q[1], ROOF + 0.66), (math.cos(a), math.sin(a), 0.0), 0.18, 0.08, seg=12, bevel=0.0)
    tile_dome(p)
    # a fachada da frente (atrás da arcada): a porta grande em arco, as janelas de cima, o estandarte enorme
    for k in range(5):                                                          # os cravos dourados da porta grande
        p.box('CnGold', PX1 - PT * 0.55 + 0.04, PX1 - PT * 0.55 + 0.07, -0.9 + k * 0.45 - 0.035, -0.9 + k * 0.45 + 0.035, G + 0.3, G + 2.9, bevel=0.0)
    arch_band(p, 'Plaster#s', (PX1, 0.0), 'x', 2.4, G + 3.0, 0.005, width=0.2, t=0.08, seg=16, legs=3.0)
    for y in (-4.8, -2.4, 2.4, 4.8):
        arch_window(p, PX1, y, G + 1.0, '+x', w=1.0, h=1.7)
    for y in (-4.8, -2.4, 2.4, 4.8):
        arch_window(p, PX1, y, ARC_H + G + 0.62, '+x', w=0.9, h=1.2)
    p.cyl('Wood', (PX1 + 0.15, 0.0, ROOF - 0.2), 'y', 0.05, 2.6, seg=8, bevel=0.0)
    p.box('Player', PX1 + 0.05, PX1 + 0.09, -1.15, 1.15, ARC_H + G + 0.5, ROOF - 0.2, bevel=0.0)
    for s in (-1, 1):
        p.box('Player', PX1 + 0.05, PX1 + 0.09, s * 1.15 - (0.35 if s > 0 else 0.0), s * 1.15 + (0.35 if s < 0 else 0.0),
              ARC_H + G + 0.2, ARC_H + G + 0.5, bevel=0.0)
    p.cyl('Cloth', (PX1 + 0.1, 0.0, ARC_H + G + 1.35), 'x', 0.62, 0.02, seg=28, bevel=0.0)
    p.cyl('Player', (PX1 + 0.12, 0.0, ARC_H + G + 1.35), 'x', 0.42, 0.02, seg=24, bevel=0.0)
    p.cyl('Cloth', (PX1 + 0.14, 0.0, ARC_H + G + 1.35), 'x', 0.2, 0.02, seg=20, bevel=0.0)
    # os lados (±Y): as janelas (as de baixo com sacos de areia), a sacada de madeira com o tapete pendurado
    for s in (-1, 1):
        face = '+y' if s > 0 else '-y'
        for x in (-6.2, -1.2):
            arch_window(p, x, s * PY, G + 1.0, face, w=1.0, h=1.6)
            bag_row(p, [(x - 0.55, s * (PY + 0.2)), (x + 0.55, s * (PY + 0.2))], G + 1.0, layers=2, bag_size=(0.55, 0.32, 0.22), seed=int(x) + s)
        arch_window(p, -6.2, s * PY, G + 4.0, face, w=0.9, h=1.3)
        # a sacada (x −4,6..−2,2) com o corrimão, as mãos-francesas e o tapete por cima do corrimão
        y0, y1 = sorted((s * PY, s * (PY + 1.05)))
        p.box('Wood', -4.7, -2.1, y0, y1, G + 3.45, G + 3.6, bevel=0.0)
        for x in (-4.5, -2.3):
            rod(p, 'Wood', (x, s * (PY + 0.95), G + 3.45), (x, s * PY, G + 2.6), 0.05)
        railing(p, [(-4.65, s * PY), (-4.65, s * (PY + 1.0)), (-2.15, s * (PY + 1.0)), (-2.15, s * PY)], G + 3.6, h=0.95, m='Wood', post_step=0.6)
        carpet(p, (-3.4, s * (PY + 1.05)), G + 4.55, face, w=1.7, h=2.0, seed=3 + s)
    # os fundos (acima do muro de trás): janelas e um remendo de chapa
    for y in (-4.5, 0.0, 4.5):
        arch_window(p, PX0, y, G + 4.0, '-x', w=0.9, h=1.3)
    # o teto: o ninho de sacos com a DShK no canto da frente (−Y), a caixa d'água, o prato de TV roubado, as bandeiras
    bag_row(p, [(-0.2, -5.3), (-0.2, -6.6), (-2.2, -6.6)], ROOF + 0.1, layers=3, seed=11)
    dshk(p, -0.9, -5.7, ROOF + 0.1, yaw=-30)
    crate_stack(p, -2.0, -5.5, ROOF + 0.1, 1, yaw=10, size=(0.9, 0.5, 0.4))
    water_tank(p, -6.4, 5.6, ROOF + 0.1, r=0.65, h=1.2, stand=0.5)
    p.cyl('Trim', (-1.0, 5.6, ROOF + 0.6), 'z', 0.04, 1.0, seg=6, bevel=0.0)
    p.cyl('Metal', (-0.85, 5.6, ROOF + 1.2), (0.6, 0.0, 0.8), 0.55, 0.06, seg=24, bevel=0.0)
    for (x, y, ph) in ((PX0 + 0.45, -PY, 0.4), (PX0 + 0.45, PY, 1.9)):
        flagpole(p, x, y, ROOF + 1.45, 2.6, length=1.25, height=0.85, phase=ph)


def arch_window(p, x, y, z, face, w=1.0, h=1.6, m='Dark', t=None):
    """(31/10, o detalhe AAA) A janela alta em arco no VÃO FUNDO da parede oca (o vão é cortado pelo adobe_block): o arco de
    adobe (os tímpanos na espessura da parede), o escuro recuado com o caixilho de madeira em cruz, o peitoril gasto saindo da
    face e a moldura de reboco em volta do arco."""
    t = t or PT
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


def carpet(p, top_c, z_top, face, w=1.6, h=2.0, seed=0):
    """O tapete pendurado (a riqueza saqueada): o pano com a barra verde, o campo escuro, o medalhão claro e as franjas;
    colado na face (face +x/−x/+y/−y), descendo de z_top."""
    x, y = top_c
    s = 1 if face[0] == '+' else -1
    lay = [('Cloth#tarp', w, h, 0.0), ('Player', w - 0.16, h - 0.16, 0.012), ('Rust#s', w - 0.36, h - 0.36, 0.024)]
    for (m, ww, hh, d) in lay:
        zc = z_top - h / 2
        if face[1] == 'x':
            p.box(m, min(x + s * d, x + s * (d + 0.02)), max(x + s * d, x + s * (d + 0.02)), y - ww / 2, y + ww / 2, zc - hh / 2, zc + hh / 2, bevel=0.0)
        else:
            p.box(m, x - ww / 2, x + ww / 2, min(y + s * d, y + s * (d + 0.02)), max(y + s * d, y + s * (d + 0.02)), zc - hh / 2, zc + hh / 2, bevel=0.0)
    zc = z_top - h / 2
    pts = [(0.0, 0.42), (0.28, 0.0), (0.0, -0.42), (-0.28, 0.0)]
    d = 0.036
    if face[1] == 'x':
        prism(p, 'Cloth#tarp', [(x + s * d, y + a * w / 1.6, zc + b * h / 2.0) for (a, b) in pts], (s * 0.01, 0.0, 0.0))
    else:
        prism(p, 'Cloth#tarp', [(x + a * w / 1.6, y + s * d, zc + b * h / 2.0) for (a, b) in pts], (0.0, s * 0.01, 0.0))
    for k in range(9):
        u = -w / 2 + 0.1 + k * (w - 0.2) / 8
        if face[1] == 'x':
            p.box('Cloth#tarp', min(x, x + s * 0.02), max(x, x + s * 0.02), y + u - 0.02, y + u + 0.02, z_top - h - 0.12, z_top - h, bevel=0.0)
        else:
            p.box('Cloth#tarp', x + u - 0.02, x + u + 0.02, min(y, y + s * 0.02), max(y, y + s * 0.02), z_top - h - 0.12, z_top - h, bevel=0.0)


def arcade(p):
    """A arcada da frente: as colunas redondas com a base e o capitel (o anel de azulejo), os arcos de reboco entre elas
    com a moldura verde, o terraço em cima com a balaustrada de ameias e os tapetes pendurados por cima dela."""
    x0, x1 = PX1, ARC_X1
    zt = G + ARC_H
    cxs = x1 - 0.35
    for y in COLS:
        p.box('Plaster#lo', cxs - 0.38, cxs + 0.38, y - 0.38, y + 0.38, G, G + 0.4)
        p.cyl('Plaster#s', (cxs, y, G + 0.4 + 1.25), 'z', 0.26, 2.5, seg=20, bevel=0.0, smooth=True)
        p.cyl('Tile', (cxs, y, G + 2.75), 'z', 0.3, 0.2, seg=20, bevel=0.0)
        p.cyl('Plaster#s', (cxs, y, G + 2.95), 'z', 0.38, 0.2, seg=20, r2=0.3, bevel=0.0)
        p.box('Plaster#lo', cxs - 0.4, cxs + 0.4, y - 0.4, y + 0.4, G + 3.05, G + 3.2)
    for (ya, yb) in zip(COLS[:-1], COLS[1:]):
        yc, w = (ya + yb) / 2, yb - ya - 0.8
        arch_fill(p, 'Plaster#arch', (cxs, yc), 'x', w, G + 3.2, zt, 0.7)
        arch_band(p, 'Player', (cxs, yc), 'x', w, G + 3.2, 0.355, width=0.2, legs=0.0)
    # as pontas (as paredes dos lados da arcada) e o forro
    for s in (-1, 1):
        y0, y1 = sorted((s * 6.4, s * 7.0))
        p.box('Plaster', x0, x1, y0, y1, G, zt, bevel=0.06)
        arch_window(p, x1, s * 6.7, G + 1.2, '+x', w=0.4, h=1.0)
    p.box('Plaster', x0, x1, -7.0, 7.0, zt - 0.05, zt + 0.25, bevel=0.04)
    p.box('Player', x1, x1 + 0.02, -7.0, 7.0, zt - 0.02, zt + 0.22, bevel=0.0)
    # o terraço: a balaustrada de ameias redondas e os 3 tapetes pendurados por cima dela (a riqueza saqueada)
    wall_seg(p, (x1 - 0.18, -6.9), (x1 - 0.18, 6.9), 0.55, t=0.3, z0=zt + 0.25, band=False, round_top=False)
    round_merlons(p, (x1 - 0.18, -6.9), (x1 - 0.18, 6.9), zt + 0.8, w=0.42, t=0.3, h=0.35)
    for s in (-1, 1):
        wall_seg(p, (x0 + 0.1, s * 6.85), (x1 - 0.1, s * 6.85), 0.55, t=0.3, z0=zt + 0.25, band=False, round_top=False)
    for (y, sd) in ((-2.4, 1), (2.4, 3)):
        carpet(p, (x1 + 0.0, y), zt + 0.95, '+x', w=1.45, h=1.55, seed=sd)
    # as lâmpadas de latão penduradas nos arcos
    for (ya, yb) in zip(COLS[:-1], COLS[1:]):
        yc = (ya + yb) / 2
        p.cyl('Dark', (cxs, yc, zt - 0.35), 'z', 0.01, 0.5, seg=4, bevel=0.0)
        p.cyl('CnGold', (cxs, yc, zt - 0.7), 'z', 0.14, 0.3, seg=10, r2=0.08, bevel=0.0)
        p.cyl('Light', (cxs, yc, zt - 0.9), 'z', 0.09, 0.12, seg=10, bevel=0.0, smooth=True)


def courtyard(p):
    """O pátio: o jardim em cruz (os 2 caminhos de lajes), a fonte seca octogonal no meio (a borda de azulejo, o fundo de
    terra rachado, o pedestal com a taça e a bica de latão), as palmeiras cortadas nos vasos de barro."""
    fx, fy = FOUNT
    # os caminhos de lajes soltas (o do portão até a arcada e o atravessado): 2 fileiras desencontradas
    for k in range(int((WF - ARC_X1 - 0.6) / 0.62)):
        x = ARC_X1 + 0.4 + k * 0.62
        if abs(x - fx) < FOUNT_A + 0.35:
            continue
        for j in range(3):
            y = (j - 1) * 0.66 + (0.2 if k % 2 else -0.1) * (1 if j != 1 else 0)
            p.obox('Plaster#s', (x + (_hash(k, j, 7) - 0.5) * 0.12, y, G + 0.02), (0.54, 0.58, 0.05), yaw=(_hash(k, j, 9) - 0.5) * 14)
    for k in range(-16, 17):
        y = k * 0.62
        if abs(y) < FOUNT_A + 0.35 or abs(y) > 10.2:
            continue
        for j in range(2):
            x = fx + (j - 0.5) * 0.66 + (0.12 if k % 2 else -0.12)
            p.obox('Plaster#s', (x, y + (_hash(k, j, 11) - 0.5) * 0.1, G + 0.02), (0.58, 0.54, 0.05), yaw=(_hash(k, j, 13) - 0.5) * 14)
    # a fonte octogonal seca
    ring_o = [(fx + FOUNT_A / math.cos(math.radians(22.5)) * math.cos(math.radians(22.5 + 45 * k)),
               fy + FOUNT_A / math.cos(math.radians(22.5)) * math.sin(math.radians(22.5 + 45 * k))) for k in range(8)]
    ring_i = [(fx + (FOUNT_A - 0.32) / math.cos(math.radians(22.5)) * math.cos(math.radians(22.5 + 45 * k)),
               fy + (FOUNT_A - 0.32) / math.cos(math.radians(22.5)) * math.sin(math.radians(22.5 + 45 * k))) for k in range(8)]
    for k in range(8):
        a, b = ring_o[k], ring_o[(k + 1) % 8]
        c, d = ring_i[(k + 1) % 8], ring_i[k]
        p.loft('Plaster', [[(a[0], a[1], G), (b[0], b[1], G), (c[0], c[1], G), (d[0], d[1], G)],
                           [(a[0], a[1], G + 0.62), (b[0], b[1], G + 0.62), (c[0], c[1], G + 0.62), (d[0], d[1], G + 0.62)]])
        p.loft('Tile', [[(a[0], a[1], G + 0.62), (b[0], b[1], G + 0.62), (c[0], c[1], G + 0.62), (d[0], d[1], G + 0.62)],
                        [(a[0], a[1], G + 0.69), (b[0], b[1], G + 0.69), (c[0], c[1], G + 0.69), (d[0], d[1], G + 0.69)]], bevel=0.0)
    p.loft('Earth#drift', [[(x, y, G + 0.02) for (x, y) in ring_i], [(x, y, G + 0.08) for (x, y) in ring_i]], bevel=0.0)
    for k in range(7):                                                      # as rachaduras do fundo seco
        a = k * 0.9 + 0.3
        r0, r1 = 0.4 + 0.2 * _hash(k, 1, 3), FOUNT_A - 0.4
        rod(p, 'Dark', (fx + r0 * math.cos(a), fy + r0 * math.sin(a), G + 0.085), (fx + r1 * math.cos(a + 0.25), fy + r1 * math.sin(a + 0.25), G + 0.085), 0.02, seg=4)
    revolve(p, 'Plaster#s', (fx, fy), [(0.45, G + 0.05), (0.4, G + 0.3), (0.22, G + 0.45), (0.2, G + 1.0), (0.3, G + 1.1),
                                      (0.85, G + 1.25), (0.9, G + 1.38), (0.75, G + 1.4), (0.2, G + 1.3), (0.15, G + 1.55),
                                      (0.24, G + 1.6), (0.02, G + 1.62)], seg=28)
    disc_ring(p, 'Tile', (fx, fy), 0.92, 0.8, G + 1.3, G + 1.39, seg=28)
    revolve(p, 'CnGold', (fx, fy), [(0.12, G + 1.6), (0.1, G + 1.75), (0.04, G + 1.9), (0.06, G + 1.95), (0.01, G + 2.0)], seg=12)
    # as palmeiras cortadas nos vasos de barro (os 4 cantos da cruz e 2 na frente da arcada)
    for (x, y, h) in ((4.3, 4.4, 2.0), (4.3, -4.4, 1.6), (7.0, 4.4, 1.4), (7.0, -4.4, 2.1),
                      (2.6, 9.4, 1.8), (2.6, -9.4, 1.5), (-6.6, 9.5, 1.7), (-1.6, -9.4, 1.9)):
        potted_palm(p, x, y, h, seed=int(x * 10 + y))


def potted_palm(p, x, y, h, seed=0):
    """O vaso grande de barro (o jarro com a boca larga) e a palmeira de tronco curto anelado com um tufo baixo de folhas
    secas (sem copa grande: o pedido)."""
    revolve(p, 'Rust#s', (x, y), [(0.28, G), (0.45, G + 0.18), (0.55, G + 0.45), (0.5, G + 0.75), (0.4, G + 0.88), (0.48, G + 0.95),
                                 (0.48, G + 1.0), (0.38, G + 1.0)], seg=24, caps=(True, True))
    disc_ring(p, 'Player', (x, y), 0.555, 0.45, G + 0.4, G + 0.5, seg=24)
    lean = (_hash(seed, 1, 5) - 0.5) * 0.25
    for k in range(int(h / 0.3)):
        z = G + 0.95 + k * 0.3
        p.cyl('Wood', (x + lean * (z - G) / h, y, z + 0.15), 'z', 0.17 - 0.01 * (k % 2), 0.3, seg=8, r2=0.14, bevel=0.0)
    ztop = G + 0.95 + h
    tx = x + lean * (ztop - G) / h
    for k in range(7):
        a = 2 * math.pi * k / 7 + seed
        L = 0.7 + 0.3 * _hash(k, seed, 9)
        p.obox('Net', (tx + math.cos(a) * L * 0.45, y + math.sin(a) * L * 0.45, ztop + 0.12), (L, 0.16, 0.03),
               yaw=math.degrees(a), pitch=18, bevel=0.0)
    p.cyl('Wood', (tx, y, ztop + 0.05), 'z', 0.14, 0.12, seg=10, bevel=0.0)


def walls(p):
    """Os muros com as ameias redondas e a borda verde, os pilares do portão com as bolas e o arco de ferro."""
    for s in (-1, 1):
        wall_seg(p, (WF, s * GATE), (WF, s * WS), WALL, t=T, round_top=False)
        round_merlons(p, (WF, s * (GATE + 0.6)), (WF, s * (WS - 0.4)), G + WALL, n=int((WS - GATE - 1.0) / 1.25), w=0.55, t=T * 0.75, h=0.45)
        wall_seg(p, (WF, s * WS), (WB, s * WS), WALL, t=T, round_top=False)
        round_merlons(p, (WF - 0.4, s * WS), (WB + 0.4, s * WS), G + WALL, n=12, w=0.55, t=T * 0.75, h=0.45)
    wall_seg(p, (WB, -WS), (WB, WS), WALL, t=T, round_top=False)
    round_merlons(p, (WB, -WS + 0.4), (WB, -PY - 1.0), G + WALL, w=0.5, t=T * 0.75, h=0.45)
    round_merlons(p, (WB, PY + 1.0), (WB, WS - 0.4), G + WALL, w=0.5, t=T * 0.75, h=0.45)
    for s in (-1, 1):
        for (x, y) in ((WF, s * WS), (WB, s * WS)):
            pilaster(p, x, y, WALL + 0.8, s=0.55)
        for y in (s * 6.6,):
            pilaster(p, WF, y, WALL + 0.55)
        for x in (3.6, -1.6):
            pilaster(p, x, s * WS, WALL + 0.55)
    # o portão: os pilares altos, as bolas de reboco com o anel verde, o arco de ferro com a placa verde, as lâmpadas
    for s in (-1, 1):
        y = s * (GATE + 0.05)
        p.box('Plaster', WF - 0.6, WF + 0.6, y - 0.6, y + 0.6, G, G + 3.9, bevel=0.06)
        p.box('Plaster#lo', WF - 0.72, WF + 0.72, y - 0.72, y + 0.72, G + 3.9, G + 4.1)
        revolve(p, 'Plaster#s', (WF, y), [(0.45, G + 4.1), (0.5, G + 4.35), (0.42, G + 4.65), (0.22, G + 4.85), (0.02, G + 4.9)], seg=20)
        band(p, WF - 0.6, WF + 0.6, y - 0.6, y + 0.6, G + 3.2, G + 3.6)
        lamp(p, WF + 0.6, y, G + 2.6, face='+x', m='Light')
        p.box('CnGold', WF + 0.72, WF + 0.76, y - 0.12, y + 0.12, G + 2.85, G + 2.9, bevel=0.0)
    for k in range(13):
        a0, a1 = math.pi * k / 13, math.pi * (k + 1) / 13
        r = GATE - 0.6
        rod(p, 'Trim', (WF, -r * math.cos(a0), G + 3.7 + 0.9 * math.sin(a0)), (WF, -r * math.cos(a1), G + 3.7 + 0.9 * math.sin(a1)), 0.04, seg=6)
    rod(p, 'Trim', (WF, -GATE + 0.55, G + 3.7), (WF, GATE - 0.55, G + 3.7), 0.04, seg=6)
    p.box('Player', WF - 0.03, WF + 0.03, -0.8, 0.8, G + 3.85, G + 4.3, bevel=0.0)
    # as folhas de grade abertas pra dentro
    for s in (-1, 1):
        y = s * (GATE - 0.62)
        p.box('Trim', WF - 2.0, WF - 0.55, y - 0.03, y + 0.03, G + 0.1, G + 0.18, bevel=0.0)
        p.box('Trim', WF - 2.0, WF - 0.55, y - 0.03, y + 0.03, G + 2.45, G + 2.53, bevel=0.0)
        for k in range(8):
            x = WF - 0.6 - k * 0.2
            p.box('Trim', x - 0.02, x + 0.02, y - 0.02, y + 0.02, G + 0.1, G + 2.7, bevel=0.0)
    # os rabiscos e a areia de fora
    scrawl(p, (WF + T / 2, -7.6, G + 1.3), '+x', size=1.5, seed=1)
    scrawl(p, (WF + T / 2, 8.6, G + 1.3), '+x', size=1.3, seed=2)
    scrawl(p, (0.8, -WS - T / 2, G + 1.3), '-y', size=1.6, seed=3)
    drift(p, (WF + T / 2, -WS + 0.6), (WF + T / 2, -GATE - 0.8), (1, 0), seed=1)
    drift(p, (WF + T / 2, GATE + 0.8), (WF + T / 2, WS - 0.6), (1, 0), seed=2)
    drift(p, (7.2, -WS - T / 2), (-7.4, -WS - T / 2), (0, -1), seed=3)
    drift(p, (7.2, WS + T / 2), (-7.4, WS + T / 2), (0, 1), seed=4)


def side_yards(p):
    """Os pátios dos lados: o carro de luxo roubado debaixo da lona (+Y) e o posto de sacos com os tambores (−Y)."""
    # o carro debaixo da lona: o corpo (a caixa baixa com o teto) e a lona caindo por cima dele até quase o chão
    cx, cy, L, W = -1.2, 9.4, 4.9, 1.85

    def car_h(x, y):
        u, v = (x - cx) / (L / 2), (y - cy) / (W / 2)
        if abs(u) > 1.08 or abs(v) > 1.1:
            return 0.0
        body = 1.0 * min(1.0, (1.08 - abs(u)) * 6) * min(1.0, (1.1 - abs(v)) * 6)
        cabin = 0.48 * max(0.0, min(1.0, (0.42 - abs(u + 0.05)) * 4)) * max(0.0, min(1.0, (0.85 - abs(v)) * 6))
        return G + 0.35 + 0.65 * body + cabin
    for (x, s) in ((cx + 1.45, -1), (cx - 1.5, -1), (cx + 1.45, 1), (cx - 1.5, 1)):
        p.cyl('Dark', (x, cy + s * 0.82, G + 0.34), 'y', 0.34, 0.24, seg=18, bevel=0.0)
        p.cyl('Metal', (x, cy + s * 0.95, G + 0.34), 'y', 0.17, 0.02, seg=14, bevel=0.0)
    p.box('Dark', cx - L / 2 + 0.1, cx + L / 2 - 0.1, cy - W / 2 + 0.1, cy + W / 2 - 0.1, G + 0.3, G + 0.95, bevel=0.0)
    from model_lib import sheet

    def tz(u, v, z):
        x = cx - L / 2 - 0.35 + (L + 0.7) * u
        y = cy - W / 2 - 0.35 + (W + 0.7) * v
        h = 0.0
        for (ox, oy) in ((0, 0), (0.25, 0), (-0.25, 0), (0, 0.25), (0, -0.25)):
            h = max(h, car_h(x + ox, y + oy) - 0.3 * math.hypot(ox, oy))
        return max(G + 0.45, h + 0.05)
    x0_, y0_ = cx - L / 2 - 0.45, cy - W / 2 - 0.45

    def cf(u, v):
        x = x0_ + (L + 0.9) * u
        y = y0_ + (W + 0.9) * v
        return (x, y, tz((x - (cx - L / 2 - 0.35)) / (L + 0.7), (y - (cy - W / 2 - 0.35)) / (W + 0.7), 0.0))
    net_surface(p, cf, 26, 12, seed=29, blotch=0.8, holes=0.3, light=0.6, ragged=0.35)
    p.box('CnGold', cx + L / 2 + 0.02, cx + L / 2 + 0.06, cy - 0.45, cy + 0.45, G + 0.45, G + 0.6, bevel=0.0)   # a grade cromada
    # o posto de sacos do lado −Y com a lona e os tambores
    bag_row(p, [(-5.6, -8.4), (-3.2, -8.4), (-3.2, -10.4)], G, layers=3, seed=12)
    for (x, y) in ((-5.0, -9.6), (-4.3, -10.1), (-5.6, -10.2)):
        barrel(p, x, y, G, r=0.3, h=0.9, m='Rust')
    crate_stack(p, 0.6, -9.6, G, 2, yaw=-12, size=(1.2, 0.7, 0.45))


def base():
    p = new_part(PREFIX + '_Base')
    p.bm('Tile', 0.0)
    open_box(p, 'Earth', -HX, HX, -HY, HY, -1.2, G, bevel=0.08)
    ring_boxes(p, 'Dark', -HX - 0.03, HX + 0.03, -HY - 0.03, HY + 0.03, G - 0.12, G - 0.09, 0.25, bevel=0.0)
    walls(p)
    palace_block(p)
    arcade(p)
    courtyard(p)
    side_yards(p)
    return finish(p)


def main():
    clear_scene()
    total = export(base(), OUT)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -HX, HX, -HY, HY, ROOF + 0.9, post='Wood', plank='Crate', stripe='Hazard'), OUT)
    print('MODEL total', total, 'tris')
    zt = ROOF + DRUM_H + 0.1 + DOME_H
    print('MODEL lote 18 x 24 m; o palacio', round(ROOF, 2), 'm; a cupula ate', round(zt, 2), 'm; o remate ate', round(zt + 1.2, 2), 'm')


main()
