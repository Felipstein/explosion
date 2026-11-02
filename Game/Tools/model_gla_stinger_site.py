"""O Stinger Site da GLA (a defesa antiaérea) provisório, modelado no Blender por script (o elenco das 3 facções, 31/10/2026:
docs/ELENCO.md §5.1).

Rodar (Blender sem janela, só CPU; não usa a placa):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_gla_stinger_site.py -- captures/game/models/gla_stinger_site

A palavra: O NINHO REDONDO DE SACOS DE AREIA COM OS LANÇA-MÍSSEIS STINGER (3 tubos num suporte giratório).
A inspiração real: os ninhos de Stinger dos mujahidin dos anos 80 (o círculo de sacos cavado numa berma de terra, a rede
de camuflagem esticada em paus por cima de parte do buraco, as caixas compridas dos mísseis) e os suportes improvisados de
vários tubos (o pedestal com o assento de trator, o guidão e a mira de arame). Estilizado pra ler de cima: os tubos maiores
que os de verdade (2,45 m × 29 cm contra 1,5 m × 14 cm, a 45°), a antena de garfo (o IFF), a tampa da frente escura em cada
tubo e o escudo de chapa com o rabisco verde.
- De cima: o anel de sacos (o círculo, a regra 2 do §3b) com a mancha da rede na metade de trás e os 3 tubos verde-oliva
  apontados pra fora; a bandeira verde no muro de sacos.
- O que se mexe: o suporte gira em volta de (0, 0) (peça Turret) e os tubos apontam pro alvo.
- Hierarquia: a menor peça da base (a defesa), os sacos a 1,6 m, a rede a 3,3 m (o feixe gira por baixo dela).

Lote 8 × 8 m (2 × 2 quadrados de 4 m), a frente em +X (a entrada do ninho fica atrás, em −X).

Peças (origem no chão, no centro do lote; Unreal X frente, Y direita, Z cima, metros):
  SM_GLA_StingerSite_Base: o lote, a berma de terra, o anel de sacos, o pedestal fixo, a rede, as caixas, a bandeira;
  SM_GLA_StingerSite_Turret (gira em volta de (0, 0), a mesma origem do modelo): a mesa giratória, o garfo, os 3 tubos, a
    mira, o assento e o guidão; a boca dos tubos em MUZZLE (a altura no relatório);
  SM_GLA_StingerSite_Scaffold: o andaime da obra em madeira.
Materiais (slots): Plaster, Earth, Rust, Wood, Cloth, Net, Crate (os tubos e as caixas verde-oliva), Metal, Player, Dark,
  Trim, Hazard (só no andaime).
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, flag, scaffold  # noqa: E402
from model_faction_kit import barrel, disc_ring, revolve  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'gla_stinger_site')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_GLA_StingerSite'

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
HX = HY = 4.0                   # o lote 8 × 8
BERM_Z = G + 0.5                # o topo da berma de terra (o ninho fica meio cavado)
RING_R = 2.55                   # o anel de sacos
PIVOT_Z = 1.5                   # o eixo de elevação dos tubos
EL = 45.0                       # a elevação dos tubos em repouso
TUBE_L, TUBE_R = 2.45, 0.145
TRUNNION_X = 0.12               # o eixo de elevação, um pouco à frente do eixo do giro
TUBES = [(-0.16, -0.13), (0.16, -0.13), (0.0, 0.15)]   # (y, z) de cada tubo no feixe (2 embaixo, 1 em cima)


def tube_dir():
    e = math.radians(EL)
    return (math.cos(e), 0.0, math.sin(e))


def muzzle():
    """A boca do tubo de cima (o ponto mais alto da frente)."""
    d = tube_dir()
    e = math.radians(EL)
    up = (-math.sin(e), 0.0, math.cos(e))
    y, z = TUBES[2]
    c = (TRUNNION_X + up[0] * z + d[0] * 0.33, y, PIVOT_Z + up[2] * z + d[2] * 0.33)
    return (c[0] + d[0] * TUBE_L / 2, c[1], c[2] + d[2] * TUBE_L / 2)


def base():
    p = new_part(PREFIX + '_Base')
    # o lote: o chão batido e a berma de terra redonda (o ninho meio cavado), a areia por dentro
    open_box(p, 'Earth', -HX, HX, -HY, HY, -1.2, G, bevel=0.08)
    ring_boxes(p, 'Dark', -HX - 0.03, HX + 0.03, -HY - 0.03, HY + 0.03, G - 0.12, G - 0.09, 0.25, bevel=0.0)
    revolve(p, 'Earth#drift', (0.0, 0.0), [(3.95, G - 0.05), (3.55, G + 0.2), (3.05, BERM_Z), (2.45, BERM_Z), (2.3, G + 0.3), (2.2, G - 0.05)], seg=48)
    # o anel de sacos (4 fileiras em cima da berma), aberto atrás (a entrada em −X) com os degraus de sacos
    bag_ring(p, (0.0, 0.0), RING_R, BERM_Z - 0.05, layers=4, gap=(158, 202), seed=3)
    for k in range(3):
        bag_row(p, [(-2.2 - k * 0.42, -0.55), (-2.2 - k * 0.42, 0.55)], G + 0.05 + 0.0 * k, layers=1 + (2 - k) // 2, seed=10 + k)
    # o pedestal fixo no meio (a base de concreto, o tubo e o rolamento): o Turret gira em cima dele
    p.cyl('Plaster', (0.0, 0.0, G + 0.12), 'z', 0.55, 0.24, seg=24, bevel=0.03)
    p.cyl('Metal', (0.0, 0.0, G + 0.24 + 0.4), 'z', 0.13, 0.8, seg=16, bevel=0.0)
    p.cyl('Dark', (0.0, 0.0, G + 1.06), 'z', 0.2, 0.06, seg=16, bevel=0.0)
    for k in range(4):
        a = math.radians(45 + 90 * k)
        p.obox('Metal', (0.28 * math.cos(a), 0.28 * math.sin(a), G + 0.42), (0.5, 0.05, 0.36), yaw=45 + 90 * k, bevel=0.0)
    # as caixas compridas dos mísseis (verde-oliva com a cinta) e um tubo de reserva, o rádio e a bateria, por dentro
    for (x, y, z, yaw) in ((-1.15, 1.3, G, 62), (-1.2, 1.32, G + 0.32, 58), (1.05, 1.45, G, -30)):
        p.obox('Crate', (x, y, z + 0.16), (1.75, 0.42, 0.32), yaw=yaw)
        for t in (-0.55, 0.55):
            p.obox('Dark', (x + math.cos(math.radians(yaw)) * t, y + math.sin(math.radians(yaw)) * t, z + 0.16), (0.06, 0.44, 0.34), yaw=yaw, bevel=0.0)
    p.cyl('Crate', (-1.55, -1.05, G + 0.35), (math.cos(math.radians(-40)), math.sin(math.radians(-40)), 0.35), TUBE_R, TUBE_L, seg=14, bevel=0.0)
    p.box('Dark', 0.75, 1.3, -1.75, -1.4, G, G + 0.45, bevel=0.0)
    p.cyl('Trim', (1.2, -1.5, G + 1.2), 'z', 0.012, 1.5, seg=4, bevel=0.0)
    p.box('Rust', 0.9, 1.25, -1.2, -0.95, G, G + 0.28, bevel=0.0)
    # a rede de camuflagem em 3 paus por cima da metade de trás (alta: o feixe de tubos gira por baixo)
    corners = [(-3.15, -2.65, G + 2.55), (-0.55, -2.85, G + 3.1), (-0.55, 2.85, G + 3.1), (-3.15, 2.65, G + 2.55)]
    camo_net_rect(p, corners, sag=0.2, cell=0.22, seed=21, blotch=0.75, holes=0.27, light=0.57, ragged=0.22)
    for (x, y, z) in corners:
        p.cyl('Wood', (x, y, (BERM_Z - 0.05 + z) / 2), 'z', 0.06, z - BERM_Z + 0.15, seg=8, bevel=0.0)
    for (x, y, z) in ((-0.55, -2.85, G + 3.1), (-0.55, 2.85, G + 3.1)):
        sx, sy = x + 0.9, y * 1.32
        rod(p, 'Dark', (x, y, z), (sx, sy, G + 0.1), 0.012, seg=4)
        p.cyl('Wood', (sx, sy, G + 0.12), 'z', 0.035, 0.35, seg=6, bevel=0.0)
    # a bandeira verde no anel (frente-esquerda) e o pano verde amarrado no pau da rede
    flagpole(p, 1.6, -2.0, BERM_Z + 0.7, 2.3, length=1.1, height=0.7, phase=0.9)
    p.obox('Player', (-0.55, 2.85, G + 2.5), (0.06, 0.2, 0.6), roll=6, bevel=0.0)
    # do lado de fora: os tambores e a pilha de caixas de munição vazias (só um canto) e as estacas da berma
    barrel(p, 3.25, 3.15, G, r=0.28, h=0.85, m='Rust')
    barrel(p, 2.75, 3.4, G, r=0.28, h=0.85, m='Rust')
    p.obox('Crate', (-3.2, -3.3, G + 0.16), (1.3, 0.42, 0.32), yaw=20)
    return finish(p)


def turret():
    """O suporte giratório: a mesa, a coluna com o garfo, o feixe de 3 tubos Stinger elevado (as tampas da frente escuras,
    a antena de garfo, a empunhadura), a faixa verde, a mira de arame, o assento de trator e o guidão. Gira em (0, 0)."""
    p = new_part(PREFIX + '_Turret')
    p.cyl('Dark', (0.0, 0.0, G + 1.12), 'z', 0.24, 0.07, seg=20, bevel=0.0)
    p.cyl('Metal', (0.0, 0.0, G + 1.22), 'z', 0.3, 0.12, seg=20, bevel=0.0)
    # o garfo (2 braços até o eixo de elevação) e o eixo
    for s in (-1, 1):
        p.prism_xz('Metal', [(-0.18, G + 1.26), (0.2, G + 1.26), (TRUNNION_X + 0.09, PIVOT_Z + 0.06), (TRUNNION_X - 0.09, PIVOT_Z + 0.06)],
                   s * 0.3, s * 0.36, bevel=0.0)
        p.cyl('Dark', (TRUNNION_X, s * 0.38, PIVOT_Z), 'y', 0.07, 0.06, seg=12, bevel=0.0)
    p.cyl('Trim', (TRUNNION_X, 0.0, PIVOT_Z), 'y', 0.04, 0.72, seg=10, bevel=0.0)
    # o feixe: o berço (2 cintas) e os 3 tubos
    d = tube_dir()
    e = math.radians(EL)
    up = (-math.sin(e), 0.0, math.cos(e))
    for t in (-0.42, 0.42):
        c = (TRUNNION_X + d[0] * t, 0.0, PIVOT_Z + d[2] * t)
        p.obox('Dark', c, (0.08, 0.5, 0.48), pitch=EL, bevel=0.0)
    for (y, z) in TUBES:
        c = (TRUNNION_X + up[0] * z + d[0] * 0.33, y, PIVOT_Z + up[2] * z + d[2] * 0.33)
        p.cyl('Crate', c, d, TUBE_R, TUBE_L, seg=18, bevel=0.0)
        f = (c[0] + d[0] * TUBE_L / 2, c[1], c[2] + d[2] * TUBE_L / 2)
        b = (c[0] - d[0] * TUBE_L / 2, c[1], c[2] - d[2] * TUBE_L / 2)
        p.cyl('Dark', f, d, TUBE_R + 0.025, 0.12, seg=18, bevel=0.0)          # a tampa da frente
        p.cyl('Dark', b, d, TUBE_R + 0.02, 0.1, seg=18, bevel=0.0)           # a de trás
        q = (c[0] + d[0] * 0.25, c[1], c[2] + d[2] * 0.25)
        p.cyl('Player', q, d, TUBE_R + 0.012, 0.16, seg=18, bevel=0.0)         # a faixa verde pintada
    # a empunhadura e a antena de garfo (o IFF) no tubo de cima, a mira de arame
    y, z = TUBES[2]
    top = (TRUNNION_X + up[0] * (z + TUBE_R) + d[0] * 0.15, y, PIVOT_Z + up[2] * (z + TUBE_R) + d[2] * 0.15)
    p.obox('Dark', (top[0] + up[0] * 0.08, 0.0, top[2] + up[2] * 0.08), (0.28, 0.08, 0.14), pitch=EL, bevel=0.0)
    for s in (-1, 1):
        a0 = (top[0] + up[0] * 0.14, s * 0.05, top[2] + up[2] * 0.14)
        a1 = (a0[0] + up[0] * 0.28 + d[0] * 0.08, s * 0.24, a0[2] + up[2] * 0.28 + d[2] * 0.08)
        rod(p, 'Dark', a0, a1, 0.014, seg=4)
        rod(p, 'Dark', a1, (a1[0] + d[0] * 0.22, s * 0.24, a1[2] + d[2] * 0.22), 0.014, seg=4)
    sx = (TRUNNION_X + up[0] * 0.5 + d[0] * 0.75, PIVOT_Z + up[2] * 0.5 + d[2] * 0.75)
    for k in range(10):
        a0, a1 = 2 * math.pi * k / 10, 2 * math.pi * (k + 1) / 10
        rod(p, 'Trim', (sx[0] + up[0] * 0.13 * math.cos(a0), 0.13 * math.sin(a0), sx[1] + up[2] * 0.13 * math.cos(a0)),
            (sx[0] + up[0] * 0.13 * math.cos(a1), 0.13 * math.sin(a1), sx[1] + up[2] * 0.13 * math.cos(a1)), 0.01, seg=4)
    rod(p, 'Trim', (sx[0] - d[0] * 0.3, 0.0, sx[1] - d[2] * 0.3), (sx[0], 0.0, sx[1]), 0.012, seg=4)
    # o assento de trator atrás (na coluna do braço), o encosto, o descanso dos pés e o guidão
    rod(p, 'Metal', (-0.1, 0.0, G + 1.25), (-0.72, 0.0, G + 0.95), 0.04, seg=8)
    p.obox('Dark', (-0.78, 0.0, G + 0.98), (0.42, 0.42, 0.07), pitch=-6, bevel=0.0)
    p.obox('Dark', (-1.0, 0.0, G + 1.22), (0.06, 0.38, 0.45), pitch=-14, bevel=0.0)
    rod(p, 'Metal', (-0.3, 0.0, G + 1.2), (-0.35, 0.0, G + 0.45), 0.03, seg=6)
    p.box('Metal', -0.45, -0.2, -0.25, 0.25, G + 0.43, G + 0.47, bevel=0.0)
    for s in (-1, 1):
        rod(p, 'Dark', (-0.15, s * 0.1, PIVOT_Z - 0.18), (-0.42, s * 0.3, PIVOT_Z - 0.12), 0.02, seg=5)
        p.cyl('Dark', (-0.44, s * 0.32, PIVOT_Z - 0.12), 'y', 0.035, 0.12, seg=8, bevel=0.0)
    # o escudo de chapa na frente do atirador (embaixo do feixe), com o rabisco verde
    p.obox('Rust#s', (0.42, 0.0, G + 1.08), (0.05, 1.15, 0.62), pitch=-12, bevel=0.0)
    for s_ in (-1, 1):
        p.obox('Rust#s', (0.3, s_ * 0.62, G + 1.08), (0.32, 0.05, 0.6), yaw=s_ * 35, bevel=0.0)
    rod(p, 'Metal', (0.1, 0.0, G + 1.2), (0.4, 0.0, G + 0.95), 0.03, seg=6)
    for ang in (25, -20, 70):
        p.obox('Player', (0.455, 0.0, G + 1.1), (0.01, 0.6, 0.09), pitch=-12, roll=ang, bevel=0.0)
    # o pano verde amarrado no berço
    p.obox('Player', (TRUNNION_X - 0.05, -0.31, PIVOT_Z - 0.3), (0.12, 0.03, 0.45), roll=8, bevel=0.0)
    return finish(p)


def main():
    clear_scene()
    total = export(base(), OUT)
    clear_scene()
    total += export(turret(), OUT)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -HX, HX, -HY, HY, 2.6, post='Wood', plank='Crate', stripe='Hazard'), OUT)
    m = muzzle()
    print('MODEL total', total, 'tris')
    print('MODEL lote 8 x 8 m; turret_pivot_m (0, 0); muzzle (x, y, z) =', tuple(round(v, 2) for v in m), '; muzzle_height_m', round(m[2], 2),
          '; elevacao em repouso', EL, 'graus; eixo de elevacao em x', TRUNNION_X, 'z', PIVOT_Z)


main()
