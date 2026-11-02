"""O Arms Dealer da GLA (a fábrica: a War Factory da GLA) provisório, modelado no Blender por script (o elenco das 3 facções,
31/10/2026: docs/ELENCO.md §5.1). A 2ª volta da Oficina (Game/Tools/model_gla_war_factory.py, 13/10), no lote do elenco.

Rodar (Blender sem janela, só CPU; não usa a placa):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_gla_arms_dealer.py -- captures/game/models/gla_arms_dealer

A palavra: a ficha aprovada (docs/concept/buildings/gla_war_factory.webp e gla_war_factory_sheet.webp, "por enquanto"): O
ARMAZÉM FECHADO, A GAIOLA DE ARMAS E A RAMPA DO TÚNEL SOB A REDE DE CAMUFLAGEM. A mesma planta da ficha, conferida nas 5
vistas: o armazém de 8 m atrás-esquerda (+Y) com o portão de galpão e o toldo de chapa, o pátio dele com o canto chanfrado
na frente; a rampa no meio com as faixas de perigo, a boca do túnel emoldurada em vigas de madeira com as luzes no teto e o
piso descendo (a unidade sobe de debaixo da terra), a rede de camuflagem por cima da metade de trás; a casa de 2 andares
atrás-direita (−Y) com a escada maciça subindo pela parede da rampa até o terraço da lona; a gaiola de armas no pátio da
frente-direita (a tela, o teto de tábuas, as granadas em pé, os foguetes). A família: a borda verde no alto dos muros e a
saia verde embaixo (como a ficha), os pilaretes com capa e os canos escuros, as chaminés de ferrugem, as bandeiras, os
rabiscos.

Onde a 1ª volta (13/10) ficou pobre e o que mudou: a rede era um plano verde liso (agora é a rede mosqueada com buracos);
a boca do túnel era uma caixa preta que aparecia por trás (agora é a estrutura coberta com o fundo, o poço cortado no chão
e a moldura de vigas); a gaiola era só postes (agora tem a tela, o teto, as granadas e os foguetes); a escada era uma
rampa de blocos (agora é maciça com a mureta e o terraço); os portões e o toldo eram placas (agora são portas de galpão com
as travessas em Z e o toldo de chapa ondulada nas mãos-francesas).

Desenho no frame da ficha (24 × 32 m, a Oficina) e exportado na escala S = 0,79: o lote fica 18,96 × 25,28 m (cabe no
19 × 26 do elenco). A frente em +X: os veículos saem pela rampa (meia-largura 4,35 m depois da escala), nascem na boca do
túnel (x −6,3 m). Alturas depois da escala: os muros 2,7 m, a casa 5,7 m, o armazém 6,5 m, a chaminé 9,6 m.

Peças (origem no chão, no centro do lote; Unreal X frente, Y direita, Z cima, metros):
  SM_GLA_ArmsDealer_Base: tudo;
  SM_GLA_ArmsDealer_Scaffold: o andaime da obra em madeira.
Materiais (slots): Plaster, Earth, Rust, Wood, Cloth, Net, Crate (as caixas, as portas e os tubos verde-oliva), Metal,
  Player, Dark, Trim, Light, Hazard (as faixas da rampa e o andaime), CnGold (o latão das granadas).
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, flag, scaffold, select_only  # noqa: E402
from model_faction_kit import arch_door, band, barrel, corrugated, crate_stack, lamp, railing, revolve, window  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'gla_arms_dealer')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_GLA_ArmsDealer'

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
        pts = [(p0[0], p0[1], -0.05), (p0[0] + nrm[0] * ww, p0[1] + nrm[1] * ww, -0.05), (p0[0], p0[1], hh)]
        prism(p, 'Earth#drift', pts, (p1[0] - p0[0], p1[1] - p0[1], 0.0))


def stones(p, segs, seed=5):
    """(01/11) As pedras e os torrões soltos na areia do pé dos muros (a ficha): o casco convexo de 7 pontos (anguloso, nunca bola),
    de 8 a 28 cm, um terço enterrado; ao longo de cada trecho [(a, b)...], a ~0,9 m um do outro com o sorteio fixo."""
    k = 0
    for (ax, ay), (bx, by) in segs:
        L = math.hypot(bx - ax, by - ay)
        nx, ny = -(by - ay) / L, (bx - ax) / L
        for i in range(max(1, int(L / 0.9))):
            k += 1
            if _hash(k, seed, 3) < 0.35:
                continue
            u = (i + _hash(k, seed, 5)) / max(1, int(L / 0.9))
            o = (_hash(k, seed, 7) - 0.5) * 0.4
            x, y = ax + (bx - ax) * u + nx * o, ay + (by - ay) * u + ny * o
            s = 0.08 + 0.2 * _hash(k, seed, 9) ** 2
            b = p.bm('Trim' if _hash(k, seed, 11) < 0.3 else 'Earth#drift', 0.0)
            pts = [b.verts.new(V(x + (_hash(k, j, 13) - 0.5) * 2 * s, y + (_hash(k, j, 17) - 0.5) * 1.6 * s,
                                 -0.06 + _hash(k, j, 19) * 1.1 * s)) for j in range(7)]
            r = bmesh.ops.convex_hull(b, input=pts)
            junk = list({g for g in r['geom_interior'] + r['geom_unused'] if isinstance(g, bmesh.types.BMVert) and g.is_valid})
            if junk:
                bmesh.ops.delete(b, geom=junk, context='VERTS')


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
# verde pintada à mão (a borda ondula), a areia do pé arredondada, a rede com volume e os tufos, a lona com as dobras das cordas
from model_building_parts import (BPart, WallFace, wbox, adobe_block, adobe_window, adobe_door, barn_gate, open_box,  # noqa: E402
                                  ring_boxes, ac_unit, cable, pipe_run, paint_band_block, gla_drift)
from model_building_parts import gla_wall_seg as wall_seg, gla_net_surface as net_surface, gla_tarp_sheet as tarp_sheet  # noqa: E402,F811
drift = gla_drift  # noqa: F811


# ------------------------------------------------------------------------------------------------- medidas (frame da ficha)
S = 0.79                        # a escala da exportação (24 × 32 → 18,96 × 25,28)
HX, HY = 12.0, 16.0
WALL = 3.2
BAY = 5.5                       # a meia-largura da rampa
MOUTH_X = -8.0                  # a boca do túnel
PIT_X = -11.0                   # o fundo do poço (a parede de trás do túnel)
# (01/11, o Felipe: "o tanque sai de túnel, então o modelo tem uma rampa que sai lá de baixo"; "tem que ter túnel, os veículos vão
# sair pelo túnel"; "as construções vão poder manipular o chão [...] se uma construção dessa é posta e é finalizada, ela recorta parte
# do chão pra ficar funcional") a rampa desce de verdade, abaixo do terreno: do chão da pista em RAMP_TOP até DEEP na boca do túnel,
# entre os muros de arrimo. O prédio pronto recorta o chão do mapa sobre ela (visual.ground_cut_m, o M_SliceGround com o buraco do
# MPC_GroundCut) e o veículo novo sobe dela (visual.tunnel_exit)
RAMP_TOP = 4.0                  # onde a rampa chega no chão da pista
DEEP = -3.8                     # o fundo do túnel (−3 m depois da escala)
ROOF_T = G + 2.2                # o teto da estrutura do túnel (o abrigo baixo da boca: a boca desce até o fundo)
WF, WSD = HX - 0.9, HY - 0.9    # os eixos dos muros de fora (a frente e os lados): a areia do pé cabe no lote
AX0, AX1, AY0, AY1, AH = -11.6, -1.5, 5.5, 15.1, 8.0      # o armazém
HX0, HX1, HY0, HY1, HH = -11.6, -4.0, -15.1, -7.5, 7.0    # a casa
TERR_Z = G + 4.0                # o terraço da lona (o topo da escada)


def outer_wall(p, a, b, h=WALL, t=0.55, skirt=True, pipes=True, nrm=None):
    """O muro de fora da ficha: o adobe com o topo redondo, a borda verde no alto, a saia verde embaixo (só por fora) e os
    canos escuros encostados de tempos em tempos."""
    # (01/11) o pé do muro entra 30 cm na areia do mapa (sem a laje em volta, o muro nasce do chão; o topo fica onde era)
    wall_seg(p, a, b, h + G + 0.3, t=t, z0=-0.3)
    (ax, ay), (bx, by) = a, b
    L = math.hypot(bx - ax, by - ay)
    dx, dy = (bx - ax) / L, (by - ay) / L
    nx, ny = nrm if nrm else (dy, -dx)
    yaw = math.degrees(math.atan2(dy, dx))
    o = t / 2 + 0.012
    if skirt:
        p.obox('Player', ((ax + bx) / 2 + nx * o, (ay + by) / 2 + ny * o, 0.25), (L, 0.02, 0.42), yaw=yaw, bevel=0.0)
    if pipes:
        n = max(1, int(L / 3.4))
        for k in range(n):
            u = (k + 0.62) / n
            x, y = ax + (bx - ax) * u + nx * (t / 2 + 0.06), ay + (by - ay) * u + ny * (t / 2 + 0.06)
            p.cyl('Trim', (x, y, G + h / 2), 'z', 0.04, h + 0.1, seg=8, bevel=0.0)
            for z in (0.6, h - 0.6):
                p.box('Dark', x - 0.05, x + 0.05, y - 0.05, y + 0.05, G + z - 0.03, G + z + 0.03, bevel=0.0)


def lot(p):
    """O chão batido do lote em pedaços (a rampa e o túnel são cortados), o fundo do túnel, a rampa calçada descendo até ele entre os
    muros de arrimo, e a pista plana até a frente."""
    W = BAY + 0.25                       # o meio dos muros de arrimo (a borda do recorte do chão do mapa fica dentro deles)
    # (01/11, o Felipe: "esse chão quadradão em volta tá bem ruim, não encaixa nada com a construção, parece que foi algo mal feito e
    # colocado em cima, reveja a ficha") a ficha não tem laje em volta: os muros nascem da areia do mapa (o pé deles e dos prédios
    # entra no chão), o chão batido só existe por dentro deles (as bordas escondidas embaixo dos muros) e a pista sai pela frente
    # descendo rente ao chão
    xb = -HX + 0.4
    for (x0, x1, y0, y1) in ((xb, WF, -WSD, -W), (xb, 7.6, W, WSD), (7.6, WF, W, 11.4)):
        open_box(p, 'Earth', x0, x1, y0, y1, -0.6, G, bevel=0.0)
    prism(p, 'Earth', [(7.6, 11.4, -0.6), (WF, 11.4, -0.6), (7.6, WSD, -0.6)], (0.0, 0.0, G + 0.6))
    # o pé de adobe da casa e do armazém (as paredes de fora estão na linha do muro: sem ele boiavam 20 cm sobre a areia do mapa)
    for (x0, x1, y0, y1) in ((HX0, HX1, HY0, HY1), (AX0, AX1, AY0, AY1)):
        p.box('Plaster', x0 - 0.1, x1 + 0.1, y0 - 0.1, y1 + 0.1, -0.4, G + 0.08, bevel=0.06)
    # a pista sai pela frente: da linha do muro até a borda do lote a laje desce do chão batido até enterrar na areia
    run_a = HX - WF + 0.4
    drop = G + 0.12
    p.obox('Plaster#s', ((WF + HX + 0.4) / 2, 0.0, G / 2 - 0.12), (math.hypot(run_a, drop), 2 * BAY - 0.1, 0.3),
           pitch=-math.degrees(math.atan2(drop, run_a)), bevel=0.0)
    # as pedras e os torrões no pé dos muros de fora (a ficha: as pedrinhas espalhadas na areia em volta)
    stones(p, [((WF + 0.45, -WSD), (WF + 0.45, -W - 0.6)), ((WF + 0.45, W + 0.6), (WF + 0.45, 11.4)),
               ((HX - 1.0, -WSD - 0.45), (xb, -WSD - 0.45)), ((7.0, WSD + 0.45), (xb, WSD + 0.45)), ((xb - 0.3, -WSD), (xb - 0.3, WSD))])
    # o fundo do túnel (escuro, com as marcas de lagarta claras) e o chão da boca até a rampa
    p.box('Dark', PIT_X, MOUTH_X + 0.02, -BAY + 0.05, BAY - 0.05, DEEP - 0.3, DEEP, bevel=0.0)
    for s in (-1, 1):
        p.box('Earth', PIT_X + 0.4, MOUTH_X, s * 1.45 - 0.32, s * 1.45 + 0.32, DEEP, DEEP + 0.012, bevel=0.0)
    # a rampa calçada: a laje inclinada da boca (DEEP) até RAMP_TOP (G), as faixas de perigo nas bordas e as juntas, tudo no plano dela
    run, rise = RAMP_TOP - MOUTH_X, G - DEEP
    L = math.hypot(run, rise)
    pitch = math.degrees(math.atan2(rise, run))
    mid = ((MOUTH_X + RAMP_TOP) / 2, (DEEP + G) / 2)
    nz = (-rise / L, run / L)            # a normal da laje (x, z)
    p.obox('Plaster#s', (mid[0] - nz[0] * 0.15, 0.0, mid[1] - nz[1] * 0.15), (L, 2 * BAY - 0.1, 0.3), pitch=pitch, bevel=0.0)
    for s in (-1, 1):
        n = int(L / 1.2)
        for k in range(n):
            u = (k + 0.5) / n
            c = (MOUTH_X + run * u + nz[0] * 0.005, s * (BAY - 0.375), DEEP + rise * u + nz[1] * 0.005)
            p.obox('Hazard' if k % 2 == 0 else 'Dark', c, (L / n, 0.35, 0.012), pitch=pitch, bevel=0.0)
    for k in range(1, 6):
        u = k / 6
        c = (MOUTH_X + run * u + nz[0] * 0.006, 0.0, DEEP + rise * u + nz[1] * 0.006)
        p.obox('Dark', c, (0.04, 2 * BAY - 1.2, 0.012), pitch=pitch, bevel=0.0)
    # a pista plana (o piso de concreto claro) da rampa até a frente, as faixas de perigo e as juntas
    p.box('Plaster#s', RAMP_TOP, WF, -BAY + 0.05, BAY - 0.05, -0.6, G + 0.04, bevel=0.0)
    for s in (-1, 1):
        for k in range(int((WF - RAMP_TOP) / 1.2)):
            x = RAMP_TOP + k * 1.2
            y0, y1 = sorted((s * (BAY - 0.55), s * (BAY - 0.2)))
            p.box('Hazard' if k % 2 == 0 else 'Dark', x, min(x + 1.2, WF), y0, y1, G + 0.04, G + 0.05, bevel=0.0)
    for k in range(1, 4):
        x = RAMP_TOP + k * (WF - RAMP_TOP) / 4
        p.box('Dark', x - 0.02, x + 0.02, -BAY + 0.6, BAY - 0.6, G + 0.04, G + 0.046, bevel=0.0)
    # os muros de arrimo da rampa (do fundo até o chão; a parte de cima é o muro baixo do yards()): o adobe com os contrafortes, o
    # escoramento de madeira (os esteios e as tábuas que a GLA pregou) e as lâmpadas descendo com a rampa
    for s in (-1, 1):
        y = s * W
        wall_seg(p, (MOUTH_X, y), (RAMP_TOP, y), G - DEEP + 0.3, t=0.5, z0=DEEP - 0.3, band=False, round_top=False)
        for k in range(4):
            x = MOUTH_X + 1.4 + k * (run - 2.0) / 3
            zf = DEEP + rise * (x - MOUTH_X) / run
            yb0, yb1 = sorted((s * BAY, s * (BAY - 0.38)))
            frustum(p, 'Plaster', x - 0.32, x + 0.32, yb0, yb1, zf - 0.05, G - 0.05, 0.06)
    # (01/11, o Felipe: "que porcarada de luz mal feitas espalhadas são essas?") as luzes da pista em pares, os dois lados no mesmo x e
    # na mesma altura (logo abaixo da capa do muro), num ritmo só da boca até a frente
    for x in (-5.4, -1.6, 2.2, 6.4):
        for s in (-1, 1):
            lamp(p, x, s * BAY, G + 0.85, face='-y' if s > 0 else '+y')


def tunnel(p):
    """A boca do túnel: a estrutura coberta (o fundo, os ombros, o teto de adobe), a moldura de vigas de madeira (os 2
    esteios, a viga de cima e as mãos-francesas), o escuro de dentro, as 3 luzes do teto, as lâmpadas, a borda verde."""
    x0, x1 = PIT_X - 0.6, MOUTH_X
    p.box('Plaster', x0, PIT_X, -BAY - 0.5, BAY + 0.5, DEEP - 0.3, ROOF_T + 0.6, bevel=0.07)                         # o fundo
    for s in (-1, 1):
        y0, y1 = sorted((s * BAY, s * (BAY + 0.55)))
        p.box('Plaster', x0, x1, y0, y1, DEEP - 0.3, ROOF_T, bevel=0.07)                                          # os ombros
    p.box('Plaster', x0, x1 + 0.25, -BAY - 0.55, BAY + 0.55, ROOF_T, ROOF_T + 0.6, bevel=0.07)          # o teto
    band(p, x0, x1 + 0.25, -BAY - 0.55, BAY + 0.55, ROOF_T + 0.05, ROOF_T + 0.5, faces='+x')
    # o escuro de dentro (a face do fundo, os lados e o forro)
    p.box('Dark', PIT_X, PIT_X + 0.03, -BAY + 0.02, BAY - 0.02, DEEP, ROOF_T, bevel=0.0)
    for s in (-1, 1):
        y0, y1 = sorted((s * (BAY - 0.02), s * (BAY + 0.01)))
        p.box('Dark', PIT_X, x1, y0, y1, DEEP, ROOF_T, bevel=0.0)
    p.box('Dark', PIT_X, x1, -BAY, BAY, ROOF_T - 0.03, ROOF_T, bevel=0.0)
    for k in range(3):                                                                                 # as vigas do forro
        x = PIT_X + 0.6 + k * 0.9
        p.box('Wood', x - 0.12, x + 0.12, -BAY, BAY, ROOF_T - 0.3, ROOF_T - 0.03, bevel=0.0)
    for y in (-3.4, 0.0, 3.4):
        p.box('Light', MOUTH_X - 1.7, MOUTH_X - 1.3, y - 0.4, y + 0.4, ROOF_T - 0.42, ROOF_T - 0.3, bevel=0.0)
        p.box('Dark', MOUTH_X - 1.75, MOUTH_X - 1.25, y - 0.45, y + 0.45, ROOF_T - 0.32, ROOF_T - 0.3, bevel=0.0)
    # a moldura de vigas na boca: os 2 esteios, a viga de cima, as mãos-francesas, as lâmpadas nos esteios
    for s in (-1, 1):
        p.box('Wood', MOUTH_X - 0.1, MOUTH_X + 0.25, s * (BAY - 0.05) - 0.18, s * (BAY - 0.05) + 0.18, DEEP, ROOF_T - 0.3, bevel=0.0)
        rod(p, 'Wood', (MOUTH_X + 0.08, s * (BAY - 0.3), ROOF_T - 1.3), (MOUTH_X + 0.08, s * (BAY - 1.3), ROOF_T - 0.45), 0.09, seg=6)
        lamp(p, MOUTH_X + 0.25, s * (BAY - 0.05), DEEP + 3.2, face='+x')
    p.box('Wood', MOUTH_X - 0.1, MOUTH_X + 0.3, -BAY - 0.3, BAY + 0.3, ROOF_T - 0.5, ROOF_T - 0.02, bevel=0.0)
    for k in range(9):
        y = -BAY + 0.3 + k * (2 * BAY - 0.6) / 8
        p.cyl('Dark', (MOUTH_X + 0.31, y, ROOF_T - 0.26), 'x', 0.035, 0.02, seg=8, bevel=0.0)
    # no teto do túnel (debaixo da rede): as caixas e o tambor (a ficha: a fileira de caixas atrás)
    for (x, y, n, yaw) in ((-10.4, -3.6, 2, 5), (-10.3, -1.6, 1, -8), (-10.5, 2.2, 2, 12), (-9.0, 3.9, 1, -4)):
        crate_stack(p, x, y, ROOF_T + 0.6, n, yaw=yaw, size=(1.5, 0.85, 0.5))
    barrel(p, -9.0, 0.6, ROOF_T + 0.6)


def warehouse(p):
    """O armazém fechado (a ficha: o bloco alto atrás-esquerda): o portão de galpão com as travessas em Z e o toldo de chapa
    ondulada nas mãos-francesas, a janela de grade, as vigas, as canaletas, o parapeito, a plataforma da chaminé e da caixa
    d'água, a bandeira; o lado de fora com a porta em arco, a lona-abrigo e as caixas."""
    dy0, dy1, dz = 8.7, 13.3, G + 3.9
    win = (6.4, 7.4, G + 5.0, G + 5.95)
    door_b = (-9.1, -7.5, G, G + 2.4)
    adobe_block(p, AX0, AX1, AY0, AY1, G, G + AH, t=0.5, holes={'+x': [(dy0, dy1, G, dz), win], '+y': [door_b]},
                beams=('-y', '+y'), beam_step=0.95, beam_out=0.42, beam_r=0.1, cols=[(-8.2, 10.3), (-4.8, 10.3)],
                parapet=0.6, par_t=0.42)
    p.box('Plaster#slab', AX0 + 0.45, AX1 - 0.45, AY0 + 0.45, AY1 - 0.45, G + 3.9, G + 4.1, bevel=0.0)          # o mezanino de dentro
    for (x, y) in ((AX0, AY0), (AX0, AY1), (AX1, AY0), (AX1, AY1)):
        frustum(p, 'Plaster', x - 0.6, x + 0.6, y - 0.6, y + 0.6, G, G + AH + 0.85, 0.12)
        p.box('Plaster#lo', x - 0.52, x + 0.52, y - 0.52, y + 0.52, G + AH + 0.85, G + AH + 1.0)
    rebar_tuft(p, AX1, AY1, G + AH + 1.0, 0.25, seed=5)
    paint_band_block(p, AX0, AX1, AY0, AY1, G + AH - 1.0, G + AH - 0.4, seed=3)
    for y in (7.4, 13.4):
        spout(p, AX1, y, G + AH + 0.05, '+x')
    spout(p, -6.5, AY1, G + AH + 0.05, '+y')
    # o portão de galpão no vão fundo da parede: as 2 folhas de tábua verde-oliva com as travessas em Z, o batente, a verga
    Fx = WallFace('+x', AX1, 0.0)
    barn_gate(p, Fx, dy0, dy1, G, dz, t=0.5)
    # o toldo de chapa ondulada (caindo pra frente) nas mãos-francesas de ferro, com a borda dobrada e o remendo
    corrugated(p, AX1 + 0.05, AX1 + 1.5, dy0 - 0.5, dy1 + 0.5, dz + 0.75, dz + 0.4, axis='x', m='Rust#sheet', patch=True, seed=4)
    for y in (dy0 - 0.3, (dy0 + dy1) / 2, dy1 + 0.3):
        rod(p, 'Trim', (AX1 + 0.05, y, dz + 0.72), (AX1 + 1.4, y, dz + 0.42), 0.03, seg=5)
        rod(p, 'Trim', (AX1 + 0.05, y, dz - 0.15), (AX1 + 1.0, y, dz + 0.48), 0.03, seg=5)
    # a janela de grade, as lâmpadas, o rabisco, o cano
    adobe_window(p, Fx, win[0], win[1], win[2], win[3], t=0.5, grille=True)
    lamp(p, AX1, dy0 - 0.75, G + 3.6, face='+x')
    lamp(p, AX1, dy1 + 0.75, G + 3.6, face='+x')
    scrawl(p, (AX1, 14.3, G + 2.0), '+x', size=1.3, seed=9)
    # a plataforma do canto de trás no teto: a chaminé grande, a caixa d'água, as caixas, a bandeira
    sx0, sx1, sy0, sy1 = -11.3, -7.8, 11.2, 14.8
    p.box('Plaster', sx0, sx1, sy0, sy1, G + AH, G + AH + 1.2, bevel=0.05)
    band(p, sx0, sx1, sy0, sy1, G + AH + 0.65, G + AH + 1.05)
    p.cyl('Rust#s', (-9.9, 9.6, G + AH + 1.9), 'z', 0.72, 3.8, seg=24, bevel=0.0, smooth=True)
    for z in (1.0, 2.2, 3.2):
        p.cyl('Dark', (-9.9, 9.6, G + AH + z), 'z', 0.745, 0.06, seg=24, bevel=0.0)
    p.cyl('Dark', (-9.9, 9.6, G + AH + 3.85), 'z', 0.82, 0.14, seg=24, bevel=0.0)
    p.cyl('Dark', (-9.9, 9.6, G + AH + 3.8), 'z', 0.55, 0.08, seg=16, bevel=0.0)
    water_tank(p, -9.4, 13.0, G + AH + 1.2, r=0.85, h=1.4, stand=0.0)
    crate_stack(p, -4.2, 7.1, G + AH, 2, yaw=10, size=(1.5, 0.85, 0.5))
    crate_stack(p, -3.0, 13.5, G + AH, 1, yaw=-30, size=(1.5, 0.85, 0.5))
    flagpole(p, AX0 + 1.3, AY1 - 0.6, G + AH + 1.2, 3.0, length=1.3, height=0.9, phase=0.3)
    # o lado de fora (+Y): a porta em arco, a lona-abrigo nos paus, as caixas e os tambores, o rabisco, o remendo
    adobe_door(p, WallFace('+y', AY1, 0.0), door_b[0], door_b[1], door_b[2], door_b[3], t=0.5)
    # a vida na parede: o ar-condicionado no suporte, o conduíte e o fio até o poste da rede
    ac_unit(p, WallFace('+y', AY1, 0.0), -4.2, G + 2.6)
    pipe_run(p, [(-10.6, AY1 + 0.08, G + 0.3), (-10.6, AY1 + 0.08, G + AH - 0.4), (-10.6, AY1 - 0.3, G + AH + 0.25)], 0.035,
             m='Trim#cond', wall=(0.0, -1.0, 0.04), step=1.3, flanges=False, seg=8)
    tarp_sheet(p, [(-6.6, AY1 + 0.02, G + 2.7), (-3.4, AY1 + 0.02, G + 2.7), (-3.4, AY1 + 0.42, G + 2.3), (-6.6, AY1 + 0.42, G + 2.3)], sag=0.02, nx=4, ny=2)
    p.box('Rust', -2.9, -1.9, AY1, AY1 + 0.04, G + 3.0, G + 4.4, bevel=0.0)
    scrawl(p, (-5.0, AY1, G + 5.4), '+y', size=1.5, seed=10)
    # o lado da rampa (−Y): as lâmpadas viradas pra rampa
    for x in (-6.5, -3.0):
        lamp(p, x, AY0, G + 3.4, face='-y')


def house(p):
    """A casa de 2 andares (a ficha: atrás-direita): a porta com o toldo de chapa nos paus, as janelas com as venezianas,
    as vigas, o parapeito, o teto com a caixa d'água, a chaminé e a bandeira; o terraço da lona no topo da escada, a escada
    maciça subindo pela parede da rampa."""
    hwin = 1.0, 1.05
    win_x = [(y, z) for (y, z) in ((-9.1, G + 1.2), (-9.1, G + 4.4), (-13.7, G + 4.4), (-11.6, G + 4.4))]
    win_y = [(x, z) for (x, z) in ((-9.6, G + 4.4), (-6.2, G + 4.4), (-6.2, G + 1.2))]
    door_h = (-12.3, -10.9, G, G + 2.3)
    door_t = (-6.75, -5.65, TERR_Z, TERR_Z + 2.1)
    adobe_block(p, HX0, HX1, HY0, HY1, G, G + HH, t=0.45,
                holes={'+x': [door_h] + [(y - 0.5, y + 0.5, z, z + 1.05) for (y, z) in win_x],
                       '-y': [(x - 0.5, x + 0.5, z, z + 1.05) for (x, z) in win_y], '+y': [door_t]},
                beams=('+x', '-x'), beam_step=0.9, beam_out=0.4, cols=[(-7.8, -11.3)], parapet=0.55, par_t=0.38)
    p.box('Plaster#slab', HX0 + 0.4, HX1 - 0.4, HY0 + 0.4, HY1 - 0.4, TERR_Z - 0.25, TERR_Z, bevel=0.0)        # o piso do 2º andar
    Fh, Fs = WallFace('+x', HX1, 0.0), WallFace('-y', HY0, 0.0)
    for (y, z) in win_x:
        adobe_window(p, Fh, y - 0.5, y + 0.5, z, z + 1.05, t=0.45, shutter='Wood#shut' if z > 2 else None, grille=z < 2)
    for (x, z) in win_y:
        adobe_window(p, Fs, x - 0.5, x + 0.5, z, z + 1.05, t=0.45, shutter='Wood#shut' if z > 2 else None)
    adobe_door(p, Fh, door_h[0], door_h[1], door_h[2], door_h[3], t=0.45, open_frac=0.3)
    adobe_door(p, WallFace('+y', HY1, 0.0), door_t[0], door_t[1], door_t[2], door_t[3], t=0.45)
    ac_unit(p, Fs, -8.0, G + 2.5)
    for (x, y) in ((HX0, HY0), (HX1, HY0), (HX1, HY1)):
        frustum(p, 'Plaster', x - 0.55, x + 0.55, y - 0.55, y + 0.55, G, G + HH + 0.8, 0.12)
        p.box('Plaster#lo', x - 0.47, x + 0.47, y - 0.47, y + 0.47, G + HH + 0.8, G + HH + 0.95)
    paint_band_block(p, HX0, HX1, HY0, HY1, G + HH - 1.0, G + HH - 0.4, seed=7)
    spout(p, HX1, -14.0, G + HH + 0.05, '+x')
    spout(p, -8.0, HY0, G + HH + 0.05, '-y')
    # a frente (+X): a porta com o toldo de chapa nos 2 paus, a lâmpada, as janelas (2 andares) com venezianas, o rabisco
    corrugated(p, HX1 + 0.05, HX1 + 1.5, -12.8, -10.4, G + 3.2, G + 2.85, axis='x', m='Rust#sheet', patch=True, seed=9)
    for y in (-12.7, -10.5):
        p.cyl('Wood', (HX1 + 1.4, y, G + 1.45), 'z', 0.05, 2.9, seg=8, bevel=0.0)
    lamp(p, HX1, -13.4, G + 2.6, face='+x')
    # o fio da casa até o poste da rede (a vida: a luz puxada no gato)
    cable(p, (HX1 - 0.3, -7.6, G + HH + 0.2), (0.4, -7.7, G + 6.3), sag=0.5, r=0.022)
    # o teto: a caixa d'água, a chaminé, as caixas, a bandeira
    water_tank(p, -9.8, -9.6, G + HH, r=0.82, h=1.4, stand=0.35)
    stovepipe(p, -6.0, -13.6, G + HH, 2.6, r=0.42)
    crate_stack(p, -8.2, -14.3, G + HH, 1, yaw=20, size=(1.2, 0.7, 0.45))
    flagpole(p, HX0 + 1.3, HY0 + 0.6, G + HH, 3.2, length=1.25, height=0.85, phase=0.8)


def stair_and_terrace(p):
    """A escada maciça (do pátio da frente-direita até o terraço, encostada na parede da rampa), a mureta, o terraço na
    altura do 2º andar ao lado da casa com o corrimão e a lona esticada nos paus (a ficha)."""
    x_lo, x_hi, n = 1.8, -3.6, 14
    run = (x_lo - x_hi) / n
    rise = (TERR_Z - G) / n
    y0, y1 = -7.45, -6.05
    for k in range(n):
        xa = x_lo - k * run
        p.box('Plaster#lo', xa - run, xa, y0 + 0.28, y1, G, G + (k + 1) * rise)
    prism(p, 'Plaster', [(x_lo + 0.05, y0, G), (x_lo + 0.05, y0, G + 0.95), (x_hi, y0, TERR_Z + 0.95), (x_hi, y0, G)], (0.0, 0.28, 0.0))
    # o terraço (a massa de adobe do chão até o piso), o corrimão de madeira e a lona
    tx0 = -7.4
    p.box('Plaster', tx0, x_hi, y0, y1, G, TERR_Z, bevel=0.05)
    railing(p, [(x_hi, y1 - 0.1), (tx0 + 0.1, y1 - 0.1)], TERR_Z, h=1.0, m='Wood', post_step=0.8)
    for (x, y) in ((tx0 + 0.15, y0 + 0.15), (x_hi - 0.1, y0 + 0.15), (tx0 + 0.15, y1 - 0.12), (x_hi - 0.1, y1 - 0.12)):
        p.cyl('Wood', (x, y, TERR_Z + 1.1), 'z', 0.05, 2.2, seg=8, bevel=0.0)
    tarp_sheet(p, [(tx0 - 0.1, y0 - 0.2, TERR_Z + 2.25), (x_hi + 0.2, y0 - 0.2, TERR_Z + 2.05), (x_hi + 0.2, y1 + 0.3, TERR_Z + 2.05),
                   (tx0 - 0.1, y1 + 0.3, TERR_Z + 2.25)], sag=0.12, nx=8, ny=3)
    crate_stack(p, -6.6, -6.9, TERR_Z, 1, yaw=8, size=(1.1, 0.6, 0.42))
    lamp(p, 0.2, -6.0, G + 2.2, face='+y')


def weapons_cage(p):
    """A gaiola de armas (a ficha: no pátio da frente-direita): os postes, as travessas, a tela (os arames verticais e os
    X), o portão, o teto de tábuas sobre a metade de fora; dentro, a prateleira de granadas em pé (latão e verde-oliva), os
    foguetes no cavalete, as caixas, a bancada."""
    cx0, cx1, cy0, cy1, ch = 2.5, 9.5, -14.4, -8.2, 3.0
    posts = [(cx0, cy0), (cx0, cy1), (cx1, cy0), (cx1, cy1), ((cx0 + cx1) / 2, cy0), ((cx0 + cx1) / 2, cy1), (cx0, (cy0 + cy1) / 2), (cx1, (cy0 + cy1) / 2)]
    for (x, y) in posts:
        p.cyl('Trim', (x, y, G + ch / 2), 'z', 0.06, ch, seg=8, bevel=0.0)
    for z in (G + 0.1, G + ch - 0.04):
        for y in (cy0, cy1):
            rod(p, 'Trim', (cx0, y, z), (cx1, y, z), 0.035, seg=6)
        for x in (cx0, cx1):
            rod(p, 'Trim', (x, cy0, z), (x, cy1, z), 0.035, seg=6)
    gate_y = (-11.8, -10.6)                                                                          # o portão na face +X
    for (a, b) in (((cx0, cy0), (cx1, cy0)), ((cx0, cy1), (cx1, cy1)), ((cx0, cy0), (cx0, cy1)), ((cx1, cy0), (cx1, cy1))):
        L = math.dist(a, b)
        m = int(L / 0.3)
        for k in range(1, m):
            u = k / m
            x, y = a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u
            if a[0] == cx1 and b[0] == cx1 and gate_y[0] < y < gate_y[1]:
                continue
            p.cyl('Dark#wire', (x, y, (G + 0.1 + G + ch - 0.05) / 2), 'z', 0.022, ch - 0.15, seg=6, bevel=0.0)
    for y in gate_y:
        p.cyl('Trim', (cx1, y, G + ch / 2), 'z', 0.05, ch, seg=8, bevel=0.0)
    # o portão aberto (a folha de tela girada pra fora)
    gx, gy = cx1, gate_y[1]
    for k in range(6):
        u = k / 5
        p.box('Dark', gx + 0.05 + 1.1 * u * 0.7, gx + 0.07 + 1.1 * u * 0.7, gy + 1.1 * u * 0.7 - 0.01, gy + 1.1 * u * 0.7 + 0.01, G + 0.1, G + 2.2, bevel=0.0)
    rod(p, 'Trim', (gx, gy, G + 2.2), (gx + 0.77, gy + 0.77, G + 2.2), 0.03, seg=5)
    rod(p, 'Trim', (gx, gy, G + 0.15), (gx + 0.77, gy + 0.77, G + 0.15), 0.03, seg=5)
    # o teto de tábuas na metade de fora (−Y), nas travessas
    for k in range(10):
        x = cx0 + 0.1 + k * (cx1 - cx0 - 0.2) / 9
        p.obox('Wood', (x, (cy0 + (cy0 + cy1) / 2) / 2, G + ch + 0.05), (0.62, (cy1 - cy0) / 2 + 0.4, 0.06), roll=-4, bevel=0.0)
    for x in (cx0 + 0.2, cx1 - 0.2):
        p.box('Wood', x - 0.08, x + 0.08, cy0, (cy0 + cy1) / 2 + 0.2, G + ch - 0.05, G + ch, bevel=0.0)
    # a prateleira das granadas em pé (as de latão e as verde-oliva, a ponta escura), atrás
    p.box('Wood', cx0 + 0.3, cx0 + 4.6, cy0 + 0.35, cy0 + 1.0, G + 0.25, G + 0.35, bevel=0.0)
    for (x, y) in ((cx0 + 0.35, cy0 + 0.4), (cx0 + 4.55, cy0 + 0.4), (cx0 + 0.35, cy0 + 0.95), (cx0 + 4.55, cy0 + 0.95)):
        p.box('Wood', x - 0.04, x + 0.04, y - 0.04, y + 0.04, G, G + 0.25, bevel=0.0)
    for k in range(14):
        x = cx0 + 0.5 + k * 0.29
        for j, y in enumerate((cy0 + 0.55, cy0 + 0.82)):
            if (k + j) % 5 == 4:
                continue
            m = 'CnGold' if (k + j) % 3 == 0 else 'Crate'
            revolve(p, m, (x, y), [(0.11, G + 0.35), (0.11, G + 0.95), (0.09, G + 1.1), (0.05, G + 1.25), (0.02, G + 1.3)], seg=10)
    # os foguetes no cavalete (de lado) e as caixas
    p.box('Dark', cx0 + 0.6, cx0 + 1.1, cy1 - 3.2, cy1 - 0.8, G, G + 1.5, bevel=0.0)
    for k in range(7):
        y = cy1 - 3.0 + k * 0.33
        p.cyl('Crate', (cx0 + 0.85, y, G + 0.85), 'z', 0.07, 1.5, seg=10, bevel=0.0)
        p.cyl('Dark', (cx0 + 0.85, y, G + 1.66), 'z', 0.11, 0.18, seg=10, r2=0.02, bevel=0.0)
    crate_stack(p, cx1 - 1.3, cy1 - 1.2, G, 2, yaw=0, size=(1.2, 0.7, 0.45))
    crate_stack(p, cx1 - 1.0, cy0 + 2.6, G, 1, yaw=35, size=(1.2, 0.7, 0.45))
    p.box('Wood', 5.2, 7.6, cy1 - 1.4, cy1 - 0.6, G + 0.85, G + 0.93, bevel=0.0)
    for (x, y) in ((5.3, cy1 - 1.3), (7.5, cy1 - 1.3), (5.3, cy1 - 0.7), (7.5, cy1 - 0.7)):
        p.box('Wood', x - 0.04, x + 0.04, y - 0.04, y + 0.04, G, G + 0.85, bevel=0.0)
    p.obox('Crate', (6.2, cy1 - 1.0, G + 1.0), (0.9, 0.12, 0.12), yaw=10, bevel=0.0)


def yards(p):
    """Os pátios e os muros da ficha: o da casa (frente-direita) e o do armazém (frente-esquerda, o canto chanfrado), os
    muros da rampa, os pilaretes, as bandeiras dos cantos, os rabiscos, a areia; as caixas e os tambores encostados."""
    # os muros baixos da rampa (dos 2 lados, da boca até a frente), com as lâmpadas viradas pra rampa
    wall_seg(p, (MOUTH_X, BAY + 0.25), (WF, BAY + 0.25), 1.25, t=0.5, band=False)
    wall_seg(p, (MOUTH_X, -BAY - 0.25), (WF, -BAY - 0.25), 1.25, t=0.5, band=False)
    for x in (-2.0, 2.4):
        pilaster(p, x, BAY + 0.25, 1.75, s=0.35)
    for x in (-2.0, 6.0):
        pilaster(p, x, -BAY - 0.25, 1.75, s=0.35)
    # o pátio da casa (−Y): o muro de fora e o da frente, os pilaretes, a bandeira do canto
    outer_wall(p, (HX1, -WSD), (WF, -WSD), nrm=(0, -1))
    outer_wall(p, (WF, -WSD), (WF, -BAY - 0.5), nrm=(1, 0))
    for (x, y) in ((WF, -WSD), (WF, -BAY - 0.5), (HX1, -WSD), (4.0, -WSD), (WF, -11.0)):
        pilaster(p, x, y, WALL + 0.45 + G + 0.3, z0=-0.3)
    flagpole(p, WF - 0.25, -WSD + 0.25, G + WALL + 0.45, 3.0, length=1.3, height=0.85, phase=2.0)
    scrawl(p, (WF + 0.275, -8.8, G + 1.7), '+x', size=1.7, seed=21)
    scrawl(p, (6.6, -WSD - 0.275, G + 1.6), '-y', size=1.6, seed=22)
    barrel(p, 10.2, -14.2, G)
    barrel(p, 9.9, -13.5, G)
    barrel(p, 10.3, -12.8, G, m='Player')
    crate_stack(p, -2.0, -14.0, G, 2, yaw=-15)
    crate_stack(p, -0.4, -14.6, G, 1, yaw=8)
    # o pátio do armazém (+Y): o muro de fora até o chanfro, o da frente até o chanfro, o chanfro, os pilaretes, a bandeira
    outer_wall(p, (AX1, WSD), (7.6, WSD), nrm=(0, 1))
    outer_wall(p, (WF, BAY + 0.5), (WF, 11.4), nrm=(1, 0))
    outer_wall(p, (WF, 11.4), (7.6, WSD), nrm=(0.727, 0.687))
    for (x, y) in ((WF, BAY + 0.5), (WF, 11.4), (7.6, WSD), (AX1 + 0.4, WSD), (3.0, WSD), (WF, 8.4)):
        pilaster(p, x, y, WALL + 0.45 + G + 0.3, z0=-0.3)
    flagpole(p, WF - 0.25, 11.2, G + WALL + 0.45, 3.0, length=1.3, height=0.85, phase=1.3)
    scrawl(p, (WF + 0.275, 9.9, G + 1.7), '+x', size=1.6, seed=23)
    # as caixas e os tambores encostados no muro de dentro (a ficha) e o carrinho
    for (x, y, n, yaw) in ((4.2, 14.2, 3, 5), (6.0, 14.2, 2, -12), (8.8, 11.8, 2, 45), (9.9, 7.2, 1, 90)):
        crate_stack(p, x, y, G, n, yaw=yaw, size=(1.5, 0.85, 0.5))
    for (x, y) in ((1.0, 14.4), (1.8, 14.1), (1.3, 13.6)):
        barrel(p, x, y, G)
    # o muro de trás (−X): fecha o pátio da casa e o do armazém atrás dos blocos
    outer_wall(p, (-HX + 0.4, -WSD), (-HX + 0.4, WSD), nrm=(-1, 0), pipes=False)
    drift(p, (WF + 0.275, -WSD + 0.5), (WF + 0.275, -BAY - 1.0), (1, 0), seed=1, z=-0.02)
    drift(p, (WF + 0.275, BAY + 1.0), (WF + 0.275, 11.0), (1, 0), seed=2, z=-0.02)
    drift(p, (10.4, -WSD - 0.275), (-11.2, -WSD - 0.275), (0, -1), seed=3, z=-0.02)
    drift(p, (6.8, WSD + 0.275), (-11.2, WSD + 0.275), (0, 1), seed=4, z=-0.02)


def net(p):
    """A rede de camuflagem da ficha: alta no parapeito do armazém (+Y), baixa nos 2 postes do lado da casa (−Y), por cima
    da boca do túnel e da metade de trás da rampa (a frente da rede na linha da frente do armazém), com a barriga; os cabos."""
    xf = 0.4
    corners = [(-9.6, BAY + 0.35, G + AH + 0.3), (xf, BAY + 0.35, G + AH - 0.2), (xf, -7.7, G + 6.7), (-9.6, -7.0, G + 7.1)]
    camo_net_rect(p, corners, sag=0.9, cell=0.32, seed=31, blotch=1.2, holes=0.36, light=0.68, ragged=0.35)
    for (x, y, h) in ((xf, -7.7, 6.8), (xf, BAY + 0.35, AH - 0.2)):
        p.cyl('Trim', (x, y, G + h / 2), 'z', 0.08, h, seg=8, bevel=0.0)
    rod(p, 'Dark', (xf, -7.7, G + 6.7), (-9.6, -7.0, G + 7.1), 0.02, seg=4)
    rod(p, 'Dark', (xf, BAY + 0.35, G + AH - 0.2), (-9.6, BAY + 0.35, G + AH + 0.3), 0.02, seg=4)
    rod(p, 'Dark', (xf, -7.7, G + 6.75), (xf + 1.6, -8.9, G + 0.1), 0.014, seg=4)
    rod(p, 'Dark', (xf, BAY + 0.35, G + AH - 0.25), (xf + 1.8, BAY + 1.8, G + 0.1), 0.014, seg=4)


def base():
    p = new_part(PREFIX + '_Base')
    lot(p)
    tunnel(p)
    warehouse(p)
    house(p)
    stair_and_terrace(p)
    weapons_cage(p)
    yards(p)
    net(p)
    o = finish(p)
    # a escala do elenco (o desenho é no frame da ficha, 24 × 32): aplicada na malha, a origem fica no chão no centro
    select_only(o)
    o.scale = (S, S, S)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return o


def main():
    clear_scene()
    total = export(base(), OUT)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -HX * S, HX * S, -HY * S, HY * S, (G + AH) * S + 0.4, post='Wood', plank='Crate',
                             stripe='Hazard'), OUT)
    print('MODEL total', total, 'tris')
    print('MODEL lote', round(2 * HX * S, 2), 'x', round(2 * HY * S, 2), 'm; a rampa meia-largura', round(BAY * S, 2), 'm; a boca do tunel em x',
          round(MOUTH_X * S, 2), '; o armazem', round((G + AH) * S, 2), 'm; a casa', round((G + HH) * S, 2), 'm; a chamine ate',
          round((G + AH + 3.95) * S, 2), 'm')


main()
