"""O Centro de Comando da GLA provisório, modelado no Blender por script (o elenco das 3 facções, 31/10/2026: docs/ELENCO.md §5.1).

Rodar (Blender sem janela, só CPU; não usa a placa):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_gla_command_center.py -- captures/game/models/gla_command_center

A palavra: O PÁTIO DE ADOBE MURADO COM O PRÉDIO DE COMANDO DE CÚPULA BAIXA E A ANTENA DE RÁDIO TORTA E ALTA.
Da mesma família da Oficina aprovada (docs/concept/buildings/gla_war_factory*.webp, Game/Tools/model_gla_war_factory.py):
adobe cor de areia, a borda verde no alto dos muros, pilaretes com capa, chaminé e caixa d'água de ferrugem, bandeiras
verdes, rabiscos verdes à mão (todo o verde é a cor do jogador, slot Player). A inspiração real: as kasbahs do Marrocos
(Aït Benhaddou: as torres redondas afuniladas com ameias, o portal em arco), as casas-pátio do Iraque e da Síria (o teto
plano com o parapeito, a escada maciça por fora, as vigas de madeira saindo da parede, as canaletas, o vergalhão
espetado no pilar esperando o próximo andar, a caixa d'água no teto) e as torres de rádio de sucata com estais.
- De cima (a regra 2 do estudo §3b): o círculo grande da cúpula no meio do prédio, o círculo do prato na ponta da
  antena com os 3 estais em leque, e as 2 torres redondas nos cantos da frente, num lote quadrado.
- O que se mexe (a regra 3): o prato de satélite velho gira devagar na antena; a bandeira no topo; a fumaça preta do
  escapamento do gerador.
- A função: o Worker sai pela porta do prédio (o iwan em arco, +X), atravessa o pátio e sai pelo portão da frente (+X).
- Vive da ruína: o canto de sucata do Worker (a Technical sucateada com o tripé tirando o motor, a pilha de chapas e
  pneus debaixo da rede de camuflagem), o gerador a diesel num puxadinho de chapa com os tambores, a lona do canto da
  conversa com a mesa do rádio e o quadro do mapa.

Lote 28 × 28 m (7 × 7 quadrados de 4 m), a frente em +X. Alturas: os muros 3,0 m (+ o topo redondo), as torres da
frente 5,2 m, o prédio 6,4 m (+ o parapeito), a cúpula até 9,7 m, a antena até ~16,3 m com a bandeira (o pedido: ~16 m).

Peças (origem no chão, no centro do lote; Unreal X frente, Y direita, Z cima, metros):
  SM_GLA_CommandCenter_Base: o lote, os muros, as torres, o portão, o prédio, a cúpula, a antena (treliça, cano e
    estais), o pátio inteiro;
  SM_GLA_CommandCenter_Dish (gira; a origem no eixo do cano da antena, at_m = DISH_AT, ~4 rpm): o colar, o braço, o
    prato velho de pétalas com o alimentador;
  SM_GLA_CommandCenter_Scaffold: o andaime da obra em madeira.
Materiais (slots): Plaster (adobe), Earth (o chão batido do lote e a areia no pé dos muros), Rust (chapa enferrujada,
  tambores, caixa d'água, a carcaça), Wood, Cloth (lona, sacos de areia, tapete), Net (a rede de camuflagem), Crate,
  Metal, Player (o verde: bandeiras, rabiscos, borda dos muros, faixas), Dark, Trim, Light, Beacon (a luz do topo da
  antena), Hazard (só no andaime).
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, flag, scaffold  # noqa: E402
from model_faction_kit import barrel, crate_stack, disc_ring, dome, lamp, revolve, window  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'gla_command_center')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_GLA_CommandCenter'

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
HX = HY = 14.0                  # o lote 28 × 28
WALL, T = 3.0, 0.6              # os muros: altura (acima de G) e espessura
WF, WS, WB = 13.0, 13.0, -13.0  # os eixos do muro da frente (x), dos lados (±y) e de trás (x): a face de fora a 13,3 m
TOWER_R0, TOWER_R1, TOWER_H = 1.64, 1.42, 5.2
TOWER_C = 12.3                  # as torres da frente em (12,3; ±12,3): a borda a 13,98 m
GATE_W = 3.8                    # o vão do portão
# o prédio de comando (2 andares) e a cúpula baixa no teto
MX0, MX1, MY0, MY1, MH = -12.7, -1.0, -6.8, 6.8, 6.4   # o fundo encostado no muro de trás
ROOF = G + MH
DOME_C, DRUM_R, DRUM_H, DOME_R, DOME_H = (-6.9, 0.0), 3.55, 0.85, 3.35, 2.25
# a ala de rádio (1 andar, +Y)
WX0, WX1, WY0, WY1, WH = -12.7, -5.2, 6.8, 12.7, 3.6
# a antena: a treliça torta de 3 pernas, o cano em cima, o prato que gira no cano
ANT_BASE = (-9.3, -10.3)
ANT_LEVELS = [0.0, 1.6, 3.2, 4.8, 6.4, 8.0, 9.6, 11.2, 12.6]
ANT_OFF = [(0.0, 0.0), (0.02, 0.01), (0.05, 0.0), (0.09, -0.02), (0.24, -0.06), (0.27, -0.05), (0.31, -0.09), (0.36, -0.08), (0.38, -0.08)]
ANT_TOP = 16.0                  # o topo do cano (a bandeira e a luz)
DISH_Z = 13.25                  # o colar do prato no cano
ANT_AXIS = (ANT_BASE[0] + ANT_OFF[-1][0], ANT_BASE[1] + ANT_OFF[-1][1])
DISH_AT = (ANT_AXIS[0], ANT_AXIS[1], DISH_Z)


def lattice_center(k):
    return (ANT_BASE[0] + ANT_OFF[k][0], ANT_BASE[1] + ANT_OFF[k][1], G + 0.3 + ANT_LEVELS[k])


def lattice_corner(k, i):
    """O canto i (0..2) do triângulo da treliça no nível k: a largura afina de 1,15 m a 0,5 m e torce um pouco."""
    cx, cy, z = lattice_center(k)
    t = k / (len(ANT_LEVELS) - 1)
    rad = (1.15 * (1 - t) + 0.5 * t) / math.sqrt(3)
    a = math.radians(90 + 120 * i + (3.0 if k in (4, 5) else 0.0) - 4.0 * t)
    return (cx + rad * math.cos(a), cy + rad * math.sin(a), z)


def antenna(p):
    """A torre de rádio de sucata: treliça triangular torta (seções de ferro diferente, uma emenda com chapa na dobra),
    o cano reto no topo (onde o prato gira), a antena Yagi de lado, a bandeira, a luz e os 3 estais."""
    bx, by = ANT_BASE
    p.box('Plaster', bx - 0.95, bx + 0.95, by - 0.95, by + 0.95, G - 0.1, G + 0.3, bevel=0.04)          # a sapata
    n = len(ANT_LEVELS)
    mats = ['Trim', 'Trim', 'Rust', 'Rust', 'Metal', 'Trim', 'Rust', 'Trim']
    for k in range(n - 1):
        m = mats[k]
        for i in range(3):
            a0, a1 = lattice_corner(k, i), lattice_corner(k + 1, i)
            rod(p, m, a0, a1, 0.055 if k < 4 else 0.045, seg=6)                                         # as pernas
            b0 = lattice_corner(k, (i + 1) % 3)
            b1 = lattice_corner(k + 1, (i + 1) % 3)
            rod(p, m, a0, b1 if (k + i) % 2 == 0 else b0, 0.028, seg=5)                                 # a diagonal
            if (k + i) % 2 == 1:
                rod(p, m, b0, a1, 0.028, seg=5)
            rod(p, m, a1, b1, 0.03, seg=5)                                                              # o anel
    # a emenda torta na dobra (as chapas parafusadas nas pernas)
    for i in range(3):
        q = lattice_corner(4, i)
        p.box('Metal', q[0] - 0.12, q[0] + 0.12, q[1] - 0.12, q[1] + 0.12, q[2] - 0.35, q[2] + 0.3, bevel=0.01)
    # o cano reto em cima (o eixo do prato) e o que segura ele na treliça
    ax, ay = ANT_AXIS
    ztop = lattice_center(n - 1)[2]
    p.cyl('Trim', (ax, ay, (ztop - 1.2 + ANT_TOP) / 2), 'z', 0.085, ANT_TOP - ztop + 1.2, seg=12, bevel=0.0)
    for i in range(3):
        q = lattice_corner(n - 1, i)
        rod(p, 'Trim', q, (ax, ay, q[2] + 0.02), 0.035)
        q = lattice_corner(n - 2, i)
        rod(p, 'Trim', q, (ax, ay, ztop - 1.15), 0.03)
    p.cyl('Trim', (ax, ay, ztop + 0.05), 'z', 0.32, 0.1, seg=16, bevel=0.0)                              # a mesa do colar
    # a bandeira e a luz no topo (acima do giro do prato)
    flag(p, (ax, ay), ANT_TOP - 0.1, 1.3, 0.8, m='Player', phase=0.5)
    p.cyl('Dark', (ax, ay, ANT_TOP + 0.05), 'z', 0.1, 0.1, seg=10, bevel=0.0)
    p.cyl('Beacon', (ax, ay, ANT_TOP + 0.17), 'z', 0.075, 0.16, seg=12, bevel=0.0, smooth=True)
    # a Yagi de lado (a antena de rádio de verdade, apontada pra frente-direita), abaixo dos estais de cima
    yz = lattice_center(6)[2] + 0.4
    cx, cy, _ = lattice_center(6)
    d = (math.cos(math.radians(35)), math.sin(math.radians(35)))
    rod(p, 'Trim', (cx, cy, yz), (cx + d[0] * 2.4, cy + d[1] * 2.4, yz), 0.025)
    for k in range(6):
        t = 0.6 + k * 0.35
        L = 1.0 - k * 0.07
        q = (cx + d[0] * t, cy + d[1] * t)
        rod(p, 'Trim', (q[0] - d[1] * L / 2, q[1] + d[0] * L / 2, yz), (q[0] + d[1] * L / 2, q[1] - d[0] * L / 2, yz), 0.012, seg=4)
    # a escada de serviço numa perna (até a dobra)
    for k in range(1, 5):
        q0, q1 = lattice_corner(k - 1, 0), lattice_corner(k, 0)
        for s in range(5):
            u = (s + 0.5) / 5
            q = (q0[0] + (q1[0] - q0[0]) * u, q0[1] + (q1[1] - q0[1]) * u, q0[2] + (q1[2] - q0[2]) * u)
            rod(p, 'Dark', (q[0] - 0.18, q[1] + 0.04, q[2]), (q[0] + 0.18, q[1] + 0.04, q[2]), 0.012, seg=4)
    # os estais (3 direções, 2 alturas) até os chumbadores: o canto do muro de trás, o muro do lado e o teto do prédio
    anchors = [(-12.7, -12.7, G + WALL + 0.3), (-2.4, -12.75, G + WALL + 0.3), (-11.9, -6.05, ROOF + 2.75)]
    for (x, y, z) in anchors:
        p.box('Dark', x - 0.15, x + 0.15, y - 0.15, y + 0.15, z - 0.12, z + 0.06, bevel=0.0)
        for lv in (4, 7):
            cx, cy, cz = lattice_center(lv)
            rod(p, 'Dark', (x, y, z), (cx, cy, cz), 0.018, seg=4)


def gatehouse(p):
    """O portão: o bloco de adobe com o vão em arco (a moldura verde), as folhas de madeira abertas pra dentro, as
    lâmpadas, as ameias e o ninho de sacos de areia com a DShK no teto."""
    x0, x1, h = WF - 1.3, WF + 0.62, 5.2
    r = GATE_W / 2
    for s in (-1, 1):
        y0, y1 = sorted((s * r, s * (r + 1.9)))
        p.box('Plaster', x0, x1, y0, y1, G, G + h, bevel=0.07)
    arch_fill(p, 'Plaster#arch', ((x0 + x1) / 2, 0.0), 'x', GATE_W, G + 2.9, G + h, x1 - x0)
    arch_band(p, 'Player', ((x0 + x1) / 2, 0.0), 'x', GATE_W, G + 2.9, (x1 - x0) / 2 + 0.005, width=0.3, legs=2.9)
    band_y = r + 1.9
    p.box('Player', x1, x1 + 0.02, -band_y, band_y, G + h - 0.62, G + h - 0.18, bevel=0.0)              # a borda verde
    p.box('Plaster', x0 - 0.1, x1 + 0.1, -band_y - 0.1, band_y + 0.1, G + h, G + h + 0.22, bevel=0.03)   # a capa
    round_merlons(p, (x1 - 0.12, -band_y + 0.1), (x1 - 0.12, band_y - 0.1), G + h + 0.22, n=6, w=0.55, t=0.35, h=0.4)
    round_merlons(p, (x0 + 0.12, -band_y + 0.1), (x0 + 0.12, band_y - 0.1), G + h + 0.22, n=6, w=0.55, t=0.35, h=0.4)
    for s in (-1, 1):
        lamp(p, x1, s * (r + 0.95), G + 3.55, face='+x')
        # a folha da porta aberta pra dentro, encostada no lado de dentro do pilar
        yy = s * (r - 0.1)
        p.box('Wood', x0 - 1.95, x0 - 0.05, min(yy, yy + s * 0.09), max(yy, yy + s * 0.09), G + 0.05, G + 3.15, bevel=0.0)
        for zz in (G + 0.6, G + 2.55):
            p.box('Dark', x0 - 1.9, x0 - 0.1, min(yy - s * 0.02, yy + s * 0.11), max(yy - s * 0.02, yy + s * 0.11), zz, zz + 0.12, bevel=0.0)
    # no teto: o ninho de sacos (U aberto pra trás), a metralhadora apontada pra frente, a bandeira
    bag_row(p, [(x0 + 0.3, -2.6), (x1 - 0.35, -2.6), (x1 - 0.35, 2.6), (x0 + 0.3, 2.6)], G + h + 0.22, layers=3, seed=4)
    dshk(p, x0 + 0.65, 0.2, G + h + 0.22, yaw=0.0)
    flagpole(p, x0 + 0.3, -3.4, G + h + 0.22, 3.0, phase=1.1)


def main_building(p):
    """O prédio de comando: 2 andares, o portal em arco (o iwan) na frente, a cúpula baixa no teto."""
    # (31/10, o detalhe AAA e pronto pra degradar) o prédio OCO: as paredes de adobe de 0,5 m com a quina gasta, montadas em volta
    # dos vãos de verdade (a porta do iwan, as janelas no vão fundo com a verga de madeira e o peitoril), a laje do teto e a do 2º
    # andar com espessura, as vigas de tronco que atravessam e saem pela frente e pelos fundos, os esteios de dentro
    WIN_F = [(s_ * 5.6, z) for s_ in (-1, 1) for z in (G + 1.15, G + 3.75)]
    WIN_S = [(-2.6, G + 3.9), (-10.6, G + 3.9), (-10.6, G + 1.2), (-5.3, G + 3.9)]
    WIN_N = [(-3.0, G + 3.9), (-6.0, G + 3.9)]
    WIN_B = [(-4.0, G + 3.9), (0.0, G + 3.9), (4.0, G + 3.9)]
    holes = {'+x': [(-0.95, 0.95, G, G + 2.7)] + [(y - 0.48, y + 0.48, z, z + 1.1) for (y, z) in WIN_F],
             '-y': [(x - 0.48, x + 0.48, z, z + 1.05) for (x, z) in WIN_S],
             '+y': [(x - 0.48, x + 0.48, z, z + 1.05) for (x, z) in WIN_N],
             '-x': [(y - 0.45, y + 0.45, z, z + 1.0) for (y, z) in WIN_B]}
    adobe_block(p, MX0, MX1, MY0, MY1, G, ROOF, t=0.5, holes=holes, parapet=0, beams=('+x', '-x'), beam_step=0.95,
                beam_out=0.4, beam_r=0.09, beam_z=ROOF - 1.05, cols=[(-4.0, -3.0), (-4.0, 3.0), (-9.5, -3.0), (-9.5, 3.0)])
    p.box('Plaster#slab', MX0 + 0.45, MX1 - 0.45, MY0 + 0.45, MY1 - 0.45, G + 3.05, G + 3.3, bevel=0.0)        # o piso do 2º andar
    for (f, at, ws, h_) in (('+x', MX1, WIN_F, 1.1), ('-y', MY0, WIN_S, 1.05), ('+y', MY1, WIN_N, 1.05), ('-x', MX0, WIN_B, 1.0)):
        F = WallFace(f, at, 0.0)
        for (u, z) in ws:
            w_ = 0.45 if f == '-x' else 0.48
            adobe_window(p, F, u - w_, u + w_, z, z + h_, t=0.5, shutter='Wood#shut' if z < 2 else None, grille=(f == '-x'))
    # o arco da porta do iwan (os tímpanos de adobe dentro do vão, na espessura da parede)
    arch_fill(p, 'Plaster#arch', (MX1 - 0.25, 0.0), 'x', 1.9, G + 1.75, G + 2.7, 0.5)
    # os contrafortes afunilados nos 4 cantos (passam do parapeito; o vergalhão nos da frente)
    for (x, y) in ((MX0, MY0), (MX0, MY1), (MX1, MY0), (MX1, MY1)):
        frustum(p, 'Plaster', x - 0.7, x + 0.7, y - 0.7, y + 0.7, G, ROOF + 0.95, 0.2)
        p.box('Plaster', x - 0.55, x + 0.55, y - 0.55, y + 0.55, ROOF + 0.95, ROOF + 1.12, bevel=0.03)
        if x == MX1:
            rebar_tuft(p, x, y, ROOF + 1.12, 0.28, seed=int(y))
    # o parapeito redondo com a borda verde por fora (abre na chegada da escada, −Y, entre x −8,7 e −7,5)
    pz = ROOF
    wall_seg(p, (MX0 + 0.2, MY1 - 0.2), (MX1 - 0.2, MY1 - 0.2), 0.55, t=0.35, z0=pz, band=False)
    wall_seg(p, (MX0 + 0.2, MY0 + 0.2), (MX0 + 0.2, MY1 - 0.2), 0.55, t=0.35, z0=pz, band=False)
    wall_seg(p, (MX0 + 0.2, MY0 + 0.2), (-8.7, MY0 + 0.2), 0.55, t=0.35, z0=pz, band=False)
    wall_seg(p, (-7.5, MY0 + 0.2), (MX1 - 0.2, MY0 + 0.2), 0.55, t=0.35, z0=pz, band=False)
    wall_seg(p, (MX1 - 0.2, MY0 + 0.2), (MX1 - 0.2, MY1 - 0.2), 0.55, t=0.35, z0=pz, band=False)
    zb0, zb1 = ROOF - 0.78, ROOF - 0.25                                                                # a faixa verde à mão
    paint_band_block(p, MX0, MX1, MY0, MY1, zb0, zb1, faces='-x-y+y', seed=11)
    from model_building_parts import paint_band
    for (y0, y1) in ((MY0, -3.3), (3.3, MY1)):
        paint_band(p, (MX1, y0), (MX1, y1), zb0, zb1, side=-1, off=0.0, seed=int(y0 * 7))
    # as canaletas no parapeito (as vigas agora atravessam o prédio: adobe_block)
    for x in (-3.2, -10.6):
        spout(p, x, MY0, ROOF + 0.05, '-y')
        spout(p, x, MY1, ROOF + 0.05, '+y')
    spout(p, MX0, -3.5, ROOF + 0.05, '-x')
    spout(p, MX0, 3.5, ROOF + 0.05, '-x')
    # ---- o iwan na frente: os pilares, o arco com a moldura verde, a capa com 3 ameias redondas, a porta aberta
    px0, px1 = MX1 - 0.1, MX1 + 1.7
    for s in (-1, 1):
        y0, y1 = sorted((s * 1.6, s * 3.1))
        p.box('Plaster', px0, px1, y0, y1, G, ROOF + 1.0, bevel=0.07)
    arch_fill(p, 'Plaster#arch', ((px0 + px1) / 2, 0.0), 'x', 3.2, G + 3.0, ROOF + 1.0, px1 - px0)
    arch_band(p, 'Player', ((px0 + px1) / 2, 0.0), 'x', 3.2, G + 3.0, (px1 - px0) / 2 + 0.005, width=0.32, legs=3.0)
    p.box('Plaster', px0 - 0.05, px1 + 0.12, -3.25, 3.25, ROOF + 1.0, ROOF + 1.2, bevel=0.03)
    p.box('Player', px1, px1 + 0.02, -3.1, 3.1, ROOF + 0.35, ROOF + 0.8, bevel=0.0)
    round_merlons(p, (px1 - 0.25, -2.4), (px1 - 0.25, 2.4), ROOF + 1.2, n=3, w=0.7, t=0.4, h=0.45)
    # a porta do fundo do iwan: o vão escuro em arco, as 2 folhas abertas, a lâmpada pendurada
    for (a, b) in ((-1.25, -0.95), (0.95, 1.25)):                                                     # o batente redondo da porta
        p.box('Plaster#lo', MX1 - 0.05, MX1 + 0.22, a, b, G, G + 1.75, bevel=0.06)
    from model_faction_kit import circle_on_face  # noqa: F401
    arch_band(p, 'Plaster#lo', (MX1 + 0.1, 0.0), 'x', 1.9, G + 1.75, 0.12, width=0.3, t=0.14)
    for s in (-1, 1):
        p.box('Wood', MX1 + 0.25, MX1 + 1.15, min(s * 0.98, s * 1.06), max(s * 0.98, s * 1.06), G + 0.02, G + 2.6, bevel=0.0)
    p.cyl('Dark', (MX1 + 0.85, 0.0, G + 4.25), 'z', 0.012, 0.5, seg=4, bevel=0.0)
    p.cyl('Light', (MX1 + 0.85, 0.0, G + 3.95), 'z', 0.12, 0.14, seg=10, bevel=0.0, smooth=True)
    for s in (-1, 1):
        lamp(p, px1, s * 2.35, G + 3.3, face='+x')
    # as faixas verdes (os estandartes) dos 2 lados do iwan, com o círculo claro, e as janelas
    for s in (-1, 1):
        y = s * 4.0
        p.cyl('Wood', (MX1 + 0.12, y, ROOF - 0.05), 'y', 0.04, 1.2, seg=6, bevel=0.0)
        p.box('Player', MX1 + 0.04, MX1 + 0.07, y - 0.5, y + 0.5, ROOF - 3.3, ROOF - 0.08, bevel=0.0)
        p.box('Player', MX1 + 0.04, MX1 + 0.07, y - 0.18, y + 0.18, ROOF - 3.55, ROOF - 3.3, bevel=0.0)
        p.cyl('Cloth', (MX1 + 0.08, y, ROOF - 1.35), 'x', 0.27, 0.02, seg=20, bevel=0.0)
    # o lado −Y (o que a câmera do jogo vê): o ar-condicionado no suporte com o dreno e o cabo, o conduíte descendo
    ac_unit(p, WallFace('-y', MY0, 0.0), -8.2, G + 2.55)
    pipe_run(p, [(-6.6, MY0 - 0.08, G + 0.4), (-6.6, MY0 - 0.08, ROOF - 0.3), (-6.6, MY0 + 0.3, ROOF + 0.3)], 0.035, m='Trim#cond',
             wall=(0.0, 1.0, 0.04), step=1.3, flanges=False, seg=8)
    # ---- o teto: o tambor redondo com as frestas, a faixa verde, a cúpula baixa, o remate e as 2 antenas de chicote
    cx, cy = DOME_C
    p.cyl('Plaster#s', (cx, cy, ROOF + DRUM_H / 2), 'z', DRUM_R, DRUM_H, seg=48, bevel=0.0, smooth=True)
    disc_ring(p, 'Plaster', (cx, cy), DRUM_R + 0.12, DRUM_R - 0.25, ROOF + DRUM_H - 0.04, ROOF + DRUM_H + 0.12, seg=48)
    disc_ring(p, 'Player', (cx, cy), DRUM_R + 0.02, DRUM_R - 0.1, ROOF + 0.18, ROOF + 0.52, seg=48)
    for k in range(12):
        a = 2 * math.pi * (k + 0.5) / 12
        p.obox('Dark', (cx + (DRUM_R - 0.02) * math.cos(a), cy + (DRUM_R - 0.02) * math.sin(a), ROOF + 0.62), (0.08, 0.4, 0.14), yaw=math.degrees(a), bevel=0.0)
    dome(p, 'Plaster#s', (cx, cy), DOME_R, DOME_H, ROOF + DRUM_H + 0.08, seg=48, rings=12)
    revolve(p, 'Plaster#s', (cx, cy), [(0.55, ROOF + DRUM_H + DOME_H - 0.05), (0.42, ROOF + DRUM_H + DOME_H + 0.2),
                                       (0.3, ROOF + DRUM_H + DOME_H + 0.38), (0.12, ROOF + DRUM_H + DOME_H + 0.5), (0.02, ROOF + DRUM_H + DOME_H + 0.53)], seg=20)
    # a escotilha de chapa remendada na cúpula (o remendo do GLA) e a escadinha de ferro subindo nela
    a = math.radians(-60)
    hx, hy = cx + 2.2 * math.cos(a), cy + 2.2 * math.sin(a)
    hz = ROOF + DRUM_H + 0.08 + DOME_H * math.sin(math.acos(2.2 / DOME_R))
    p.obox('Rust', (hx, hy, hz + 0.02), (1.1, 1.0, 0.08), yaw=math.degrees(a), pitch=-38, bevel=0.0)
    for (deg, hh) in ((130, 2.6), (160, 1.9)):
        a = math.radians(deg)
        q = (cx + (DRUM_R - 0.05) * math.cos(a), cy + (DRUM_R - 0.05) * math.sin(a))
        p.cyl('Trim', (q[0], q[1], ROOF + DRUM_H + hh / 2), 'z', 0.025, hh, seg=6, bevel=0.0)
        p.cyl('Dark', (q[0], q[1], ROOF + DRUM_H + 0.08), 'z', 0.08, 0.16, seg=8, bevel=0.0)
    # a caixa d'água nos pés (fundo, +Y), a escotilha do teto, o ninho de sacos com a DShK (frente, +Y)
    water_tank(p, -11.0, 5.25, ROOF, r=0.78, h=1.45, stand=0.75)
    p.box('Plaster', -3.4, -2.4, -5.6, -4.6, ROOF, ROOF + 0.45, bevel=0.04)
    p.box('Rust', -3.35, -2.45, -5.55, -4.65, ROOF + 0.45, ROOF + 0.52, bevel=0.0)
    bag_row(p, [(-1.7, 3.2), (-1.7, 6.3), (-4.6, 6.3)], ROOF, layers=3, seed=7)
    dshk(p, -2.7, 5.0, ROOF, yaw=25.0)
    crate_stack(p, -4.0, 4.9, ROOF, 2, yaw=10, size=(1.1, 0.6, 0.42))
    # o quartinho do teto (a casinha da escada, canto de trás −Y): a porta virada pra frente, o parapeito com a faixa,
    # a chaminé do fogão e o chumbador do estai da antena
    rx0, rx1, ry0, ry1, rh = -12.45, -9.3, -6.55, -3.7, 2.35
    p.box('Plaster', rx0, rx1, ry0, ry1, ROOF, ROOF + rh, bevel=0.06)
    parapet_round = [((rx0 + 0.15, ry0 + 0.15), (rx1 - 0.15, ry0 + 0.15)), ((rx1 - 0.15, ry0 + 0.15), (rx1 - 0.15, ry1 - 0.15)),
                     ((rx1 - 0.15, ry1 - 0.15), (rx0 + 0.15, ry1 - 0.15)), ((rx0 + 0.15, ry1 - 0.15), (rx0 + 0.15, ry0 + 0.15))]
    for (a_, b_) in parapet_round:
        wall_seg(p, a_, b_, 0.35, t=0.28, z0=ROOF + rh, band=False)
    p.box('Player', rx1, rx1 + 0.02, ry0, ry1, ROOF + rh - 0.55, ROOF + rh - 0.15, bevel=0.0)
    p.box('Player', rx0, rx1, ry0 - 0.02, ry0, ROOF + rh - 0.55, ROOF + rh - 0.15, bevel=0.0)
    p.box('Plaster', rx1, rx1 + 0.18, -5.75, -4.55, ROOF, ROOF + 2.15, bevel=0.02)
    p.box('Wood', rx1 + 0.18, rx1 + 0.22, -5.6, -4.7, ROOF, ROOF + 1.95, bevel=0.0)
    lamp(p, rx1, -4.1, ROOF + 1.9, face='+x')
    window(p, -10.9, ry0, ROOF + 1.0, '-y', w=0.7, h=0.6, frame='Plaster#lo')
    stovepipe(p, -11.9, -4.3, ROOF + rh, 1.3, r=0.16)
    # os remendos de chapa (a GLA vive da ruína): no fundo do prédio, acima do muro, e na lateral +Y
    p.box('Rust', MX0 - 0.04, MX0, 1.2, 2.8, G + 3.6, G + 5.1, bevel=0.0)
    for k in range(5):
        p.box('Dark', MX0 - 0.06, MX0 - 0.04, 1.28 + k * 0.33, 1.32 + k * 0.33, G + 3.65, G + 5.05, bevel=0.0)
    p.box('Rust', -9.0, -7.6, MY1, MY1 + 0.04, G + 4.6, G + 5.5, bevel=0.0)


def wing(p):
    """A ala do rádio (1 andar, +Y): a porta com o toldo, o terraço com o pergolado e as antenas."""
    # (31/10) a ala OCA: as paredes de 0,45 m em volta da porta e da janela de verdade, a laje, as vigas que saem pelos lados
    adobe_block(p, WX0, WX1, WY0, WY1, G, G + WH, t=0.45, holes={'+x': [(8.3, 9.7, G, G + 2.5), (11.2, 12.2, G + 1.3, G + 2.3)]},
                parapet=0, beams=('-y', '+y'), beam_out=0.35)
    zt = G + WH
    wall_seg(p, (WX0 + 0.18, WY0 + 0.15), (WX0 + 0.18, WY1 - 0.18), 0.5, t=0.32, z0=zt, band=False)
    wall_seg(p, (WX0 + 0.18, WY1 - 0.18), (WX1 - 0.18, WY1 - 0.18), 0.5, t=0.32, z0=zt, band=False)
    wall_seg(p, (WX1 - 0.18, WY0 + 0.6), (WX1 - 0.18, WY1 - 0.18), 0.5, t=0.32, z0=zt, band=False)
    paint_band_block(p, WX0, WX1, WY0, WY1, zt - 0.72, zt - 0.22, faces='+x-x', seed=13)
    for (x, y) in ((WX1, WY1), (WX0, WY1)):
        frustum(p, 'Plaster', x - 0.6, x + 0.6, y - 0.6, y + 0.6, G, zt + 0.75, 0.15)
    # a frente (+X): a porta de chapa com o toldo de ferrugem, a janela, a lâmpada, o rabisco
    Fw = WallFace('+x', WX1, 0.0)
    wbox(p, 'Rust#door', Fw, 8.3, 9.7, G, G + 2.5, -0.28, -0.24, bevel=0.012)                         # a porta de chapa, recuada
    for z in (G + 0.8, G + 1.7):
        wbox(p, 'Dark', Fw, 8.3, 9.7, z, z + 0.06, -0.24, -0.22, bevel=0.0)
    from model_building_parts import lintel
    lintel(p, Fw, 8.3, 9.7, G + 2.52)
    adobe_window(p, Fw, 11.2, 12.2, G + 1.3, G + 2.3, t=0.45, shutter='Wood#shut')
    lamp(p, WX1, 10.4, G + 2.7, face='+x')
    scrawl(p, (WX1, 11.3, G + 2.85), '+x', size=1.2, seed=6)
    # o terraço: o pergolado de madeira com a lona por cima de metade, as antenas de chicote, o rádio
    for (x, y) in ((-11.6, 7.6), (-8.4, 7.6), (-11.6, 12.5), (-8.4, 12.5)):
        p.cyl('Wood', (x, y, zt + 1.15), 'z', 0.06, 2.3, seg=8, bevel=0.0)
    for y in (7.6, 12.5):
        p.box('Wood', -11.8, -8.2, y - 0.06, y + 0.06, zt + 2.2, zt + 2.34, bevel=0.0)
    for x in (-11.6, -10.5, -9.5, -8.4):
        p.box('Wood', x - 0.05, x + 0.05, 7.4, 12.7, zt + 2.34, zt + 2.44, bevel=0.0)
    tarp_sheet(p, [(-11.8, 7.5, zt + 2.5), (-9.9, 7.5, zt + 2.5), (-9.9, 12.6, zt + 2.5), (-11.8, 12.6, zt + 2.5)], sag=0.12)
    for (x, y, h) in ((-6.2, 12.4, 4.2), (-6.6, 12.0, 3.0)):
        p.cyl('Trim', (x, y, zt + h / 2), 'z', 0.022, h, seg=6, bevel=0.0)
        p.box('Dark', x - 0.12, x + 0.12, y - 0.12, y + 0.12, zt, zt + 0.25, bevel=0.0)
    p.box('Dark', -7.4, -6.6, 9.2, 9.8, zt, zt + 0.55, bevel=0.0)
    p.box('Metal', -7.35, -6.65, 9.25, 9.75, zt + 0.55, zt + 0.6, bevel=0.0)
    crate_stack(p, -10.7, 10.0, zt, 1, yaw=15, size=(1.0, 0.6, 0.4))


def stair(p):
    """A escada maciça de adobe por fora, do pátio (frente) ao teto, encostada no lado −Y do prédio, com a mureta."""
    x_lo, x_hi, n = -1.25, -7.6, 20
    run = (x_lo - x_hi) / n
    rise = MH / n
    y0, y1 = MY0 - 1.25, MY0
    for k in range(n):
        xa = x_lo - k * run
        p.box('Plaster#lo', xa - run, xa, y0 + 0.25, y1, G, G + (k + 1) * rise)
    # a mureta do lado de fora: o triângulo maciço do chão até o corrimão de adobe
    pts = [(x_lo + 0.05, y0, G), (x_lo + 0.05, y0, G + 0.95), (x_hi, y0, ROOF + 0.95), (x_hi, y0, G)]
    prism(p, 'Plaster', pts, (0.0, 0.27, 0.0))
    # o patamar no topo
    p.box('Plaster', x_hi - 1.0, x_hi, y0 + 0.25, y1, G, ROOF, bevel=0.04)
    p.box('Plaster', x_hi - 1.0, x_hi, y0, y0 + 0.27, G, ROOF + 0.95, bevel=0.04)


def salvage_corner(p):
    """O canto de sucata do Worker (frente, −Y): a Technical sucateada sem rodas de um lado, o tripé tirando o motor,
    a pilha de chapas e pneus debaixo da rede de camuflagem."""
    yaw = -22.0
    cx, cy = 8.6, -9.9
    f = Frame(p, (cx, cy, G + 0.04), yaw=yaw, roll=4.0)
    # o chassi e o pára-choque
    f.box('Dark', (0.0, 0.0, 0.55), (4.7, 0.95, 0.2))
    f.box('Dark', (2.5, 0.0, 0.62), (0.14, 1.9, 0.16))
    for s in (-1, 1):
        f.box('Dark', (2.52, s * 0.55, 0.85), (0.08, 0.08, 0.5))
    # a cabine queimada: a lataria, o vão escuro dos vidros, as colunas, o teto amassado
    f.box('Rust', (0.82, 0.0, 1.2), (1.5, 1.82, 0.8))
    f.box('Dark', (0.82, 0.0, 1.82), (1.3, 1.62, 0.46))
    f.box('Rust', (0.72, 0.0, 2.08), (1.12, 1.7, 0.07), pitch=-3)
    for s in (-1, 1):
        f.box('Rust', (1.42, s * 0.8, 1.83), (0.08, 0.09, 0.52), pitch=-24)
        f.box('Rust', (0.15, s * 0.8, 1.83), (0.1, 0.1, 0.48))
    # o capô arrancado: o cofre aberto e escuro, os pára-lamas
    f.box('Dark', (1.98, 0.0, 0.95), (1.0, 1.2, 0.5))
    for s in (-1, 1):
        f.box('Rust', (2.0, s * 0.82, 1.05), (1.1, 0.18, 0.5))
    f.box('Rust', (2.48, 0.0, 1.0), (0.1, 1.5, 0.5))
    # a caçamba: o assoalho, as laterais, a tampa caída, o pedestal torto da metralhadora
    f.box('Rust', (-1.25, 0.0, 0.86), (2.3, 1.76, 0.07))
    for s in (-1, 1):
        f.box('Rust', (-1.25, s * 0.86, 1.12), (2.3, 0.06, 0.48))
    f.box('Rust', (-0.12, 0.0, 1.12), (0.06, 1.76, 0.48))
    f.box('Rust', (-2.55, 0.0, 0.62), (0.06, 1.7, 0.48), pitch=-62)
    f.cyl('Dark', (-1.0, 0.0, 1.35), 'z', 0.06, 0.9, seg=8)
    f.box('Dark', (-0.62, 0.15, 1.9), (1.2, 0.12, 0.14), yaw=28, pitch=-22)
    # as rodas: só as do lado −Y; o lado +Y nos calços de madeira
    for x in (1.95, -1.6):
        f.cyl('Dark', (x, -0.84, 0.38), 'y', 0.38, 0.28, seg=18)
        f.cyl('Metal', (x, -0.99, 0.38), 'y', 0.16, 0.03, seg=12)
        f.box('Wood', (x, 0.72, 0.17), (0.45, 0.32, 0.34))
    # o rabisco verde na porta (já é da GLA)
    for ang in (25, -20):
        f.box('Player', (0.85, -0.925, 1.2), (0.85, 0.01, 0.11), pitch=ang)
    # o tripé de ferro sobre o cofre, a corrente e o motor saindo, o gancho
    g = Frame(p, (cx, cy, G), yaw=yaw)
    apex = (2.05, 0.0, 3.75)
    for leg in ((3.35, 0.0, 0.0), (1.35, -1.45, 0.0), (1.35, 1.45, 0.0)):
        g.rod('Trim', leg, apex, 0.05)
    g.box('Dark', (2.05, 0.0, 3.55), (0.2, 0.14, 0.3))
    g.rod('Dark', (2.05, 0.0, 3.4), (2.05, 0.0, 2.35), 0.018)
    g.box('Metal', (2.05, 0.0, 1.98), (0.78, 0.6, 0.62), yaw=8)
    g.box('Dark', (2.05, 0.0, 2.33), (0.5, 0.4, 0.1), yaw=8)
    g.cyl('Dark', (1.62, 0.12, 1.85), 'x', 0.1, 0.25, seg=10)
    # um pneu solto no chão e a caixa de ferramentas
    tire(p, (cx - 2.4, cy + 2.6, G + 0.14), R=0.4, r=0.14)
    p.obox('Rust', (cx + 0.6, cy - 2.2, G + 0.18), (0.7, 0.32, 0.32), yaw=40, bevel=0.02)
    # debaixo da rede: a pilha de chapas onduladas, os pneus empilhados, a roda de tanque, o motor no palete, os canos
    for k in range(5):
        p.obox('Rust', (2.4 + 0.05 * k, -11.3, G + 0.03 + k * 0.06), (2.6, 1.15, 0.04), yaw=8 - k * 4, bevel=0.0)
    for k in range(3):
        tire(p, (4.6 + 0.04 * (k % 2), -11.9, G + 0.14 + k * 0.27), R=0.42, r=0.14)
    p.cyl('Metal', (0.6, -12.2, G + 0.5), (0.25, 1.0, 0.0), 0.48, 0.18, seg=24, bevel=0.0)
    p.cyl('Dark', (0.6, -12.2, G + 0.5), (0.25, 1.0, 0.0), 0.2, 0.24, seg=16, bevel=0.0)
    p.box('Wood', 0.9, 2.1, -9.9, -8.9, G, G + 0.14, bevel=0.0)
    p.obox('Metal', (1.5, -9.4, G + 0.48), (0.85, 0.62, 0.66), yaw=12)
    p.obox('Dark', (1.5, -9.4, G + 0.84), (0.55, 0.42, 0.08), yaw=12, bevel=0.0)
    for k in range(5):
        p.cyl('Rust', (3.6, -9.3 + k * 0.13, G + 0.08 + (k % 2) * 0.12), 'x', 0.065, 2.6, seg=8, bevel=0.0)
    # a rede de camuflagem: presa no alto do muro (−Y) e em 2 postes do lado de dentro
    corners = [(-0.6, -12.65, G + WALL + 0.55), (6.2, -12.65, G + WALL + 0.55), (6.2, -8.2, G + 3.0), (-0.6, -8.2, G + 3.15)]
    camo_net_rect(p, corners, sag=0.45, seed=11, holes=0.36, light=0.58, blotch=1.2)
    for (x, y, z) in corners:
        z0 = G + WALL + 0.1 if y < -12.5 else G
        p.cyl('Wood', (x, y, (z0 + z) / 2), 'z', 0.07, z - z0 + 0.1, seg=8, bevel=0.0)


def generator_shed(p):
    """O gerador a diesel no puxadinho de chapa encostado no muro +Y: o gerador no skid, o escapamento, os tambores."""
    x0, x1, y0, y1 = 2.0, 7.2, 10.4, 12.7
    for (x, y) in ((x0 + 0.15, y0 + 0.12), (x1 - 0.15, y0 + 0.12)):
        p.cyl('Wood', (x, y, G + 1.3), 'z', 0.07, 2.6, seg=8, bevel=0.0)
    p.box('Wood', x0, x1, y0 + 0.05, y0 + 0.2, G + 2.5, G + 2.66, bevel=0.0)
    from model_faction_kit import corrugated
    corrugated(p, x0 - 0.2, x1 + 0.2, y0 - 0.3, y1, G + 2.62, G + 3.35, axis='y', m='Rust#sheet')
    # o gerador: o skid, o motor, o radiador com a grade, o alternador, o painel, o tanque em cima
    gx0, gx1, gy0, gy1 = 2.8, 5.6, 11.1, 12.4
    p.box('Dark', gx0 - 0.1, gx1 + 0.1, gy0 - 0.05, gy1 + 0.05, G, G + 0.18, bevel=0.0)
    p.box('Metal', gx0 + 0.5, gx1 - 0.9, gy0 + 0.1, gy1 - 0.1, G + 0.18, G + 1.3, bevel=0.03)
    p.box('Dark', gx1 - 0.9, gx1 - 0.55, gy0, gy1, G + 0.18, G + 1.45, bevel=0.0)
    for k in range(6):
        p.box('Metal', gx1 - 0.92, gx1 - 0.88, gy0 + 0.1 + k * 0.2, gy0 + 0.16 + k * 0.2, G + 0.3, G + 1.35, bevel=0.0)
    p.cyl('Rust', (gx0 + 0.25, (gy0 + gy1) / 2, G + 0.72), 'x', 0.42, 0.7, seg=18, bevel=0.0, smooth=True)
    p.box('Trim', gx1 - 0.5, gx1, gy0 + 0.2, gy0 + 0.75, G + 0.6, G + 1.5, bevel=0.0)
    p.box('Light', gx1, gx1 + 0.02, gy0 + 0.3, gy0 + 0.45, G + 1.2, G + 1.3, bevel=0.0)
    p.cyl('Rust', (gx0 + 1.4, (gy0 + gy1) / 2, G + 1.55), 'x', 0.3, 1.5, seg=18, bevel=0.0, smooth=True)
    stovepipe(p, gx0 + 1.0, gy0 + 0.35, G + 1.3, 3.0, r=0.13)
    # os tambores (um caído, um pintado de verde) e o galão
    for (x, y, m) in ((7.7, 12.2, 'Rust'), (8.35, 12.25, 'Rust'), (8.0, 11.6, 'Player'), (7.5, 11.0, 'Rust')):
        barrel(p, x, y, G, r=0.3, h=0.9, m=m)
    p.cyl('Rust', (6.8, 9.8, G + 0.3), (1.0, 0.3, 0.0), 0.3, 0.9, seg=16, bevel=0.0, smooth=True)
    p.obox('Rust', (8.9, 11.7, G + 0.25), (0.35, 0.18, 0.5), yaw=20, bevel=0.01)
    # o cabo grosso do gerador até a porta da ala
    for (a, b) in (((2.8, 11.7, G + 0.05), (-0.5, 11.7, G + 0.05)), ((-0.5, 11.7, G + 0.05), (-5.0, 9.5, G + 0.05)),
                   ((-5.0, 9.5, G + 0.05), (-5.0, 9.5, G + 2.4))):
        rod(p, 'Dark', a, b, 0.035, seg=5)


def meeting_tarp(p):
    """O canto da conversa debaixo da lona (entre a ala e o gerador): o tapete, a mesa do rádio, o quadro do mapa."""
    tarp_sheet(p, [(-5.0, 7.4, G + 3.35), (1.4, 7.4, G + 2.55), (1.4, 12.4, G + 2.55), (-5.0, 12.4, G + 3.35)], sag=0.2)
    for y in (7.4, 12.4):
        p.cyl('Wood', (1.4, y, G + 1.3), 'z', 0.06, 2.6, seg=8, bevel=0.0)
        rod(p, 'Dark', (1.4, y, G + 2.55), (2.4, y + (0.6 if y < 10 else -0.6) * -1, G), 0.012, seg=4)
    p.box('Cloth', -3.4, -0.4, 8.6, 11.0, G, G + 0.025, bevel=0.0)
    for (x0, x1, y0, y1) in ((-3.4, -0.4, 8.6, 8.85), (-3.4, -0.4, 10.75, 11.0), (-3.4, -3.15, 8.6, 11.0), (-0.65, -0.4, 8.6, 11.0)):
        p.box('Player', x0, x1, y0, y1, G + 0.025, G + 0.035, bevel=0.0)
    p.box('Wood', -2.6, -1.2, 9.4, 10.2, G + 0.72, G + 0.8, bevel=0.0)
    for (x, y) in ((-2.5, 9.5), (-1.3, 9.5), (-2.5, 10.1), (-1.3, 10.1)):
        p.box('Wood', x - 0.04, x + 0.04, y - 0.04, y + 0.04, G, G + 0.72, bevel=0.0)
    p.box('Dark', -2.3, -1.8, 9.55, 9.9, G + 0.8, G + 1.08, bevel=0.0)
    p.cyl('Trim', (-1.9, 9.6, G + 1.6), 'z', 0.01, 1.1, seg=4, bevel=0.0)
    for (x, y, yaw) in ((-3.0, 9.8, 90), (-0.7, 9.8, 90)):
        p.obox('Crate', (x, y, G + 0.22), (0.7, 0.42, 0.44), yaw=yaw)
    # o quadro do mapa no cavalete (virado pra frente)
    p.box('Wood', -4.55, -4.45, 9.0, 11.0, G + 0.9, G + 2.2, bevel=0.0)
    p.box('Cloth', -4.45, -4.42, 9.1, 10.9, G + 1.0, G + 2.1, bevel=0.0)
    for (y, z, w) in ((9.6, 1.7, 0.5), (10.3, 1.4, 0.35)):
        p.box('Player', -4.42, -4.41, y - w / 2, y + w / 2, G + z - 0.04, G + z + 0.04, bevel=0.0)
    for y in (9.05, 10.95):
        rod(p, 'Wood', (-4.5, y, G + 2.2), (-4.2, y, G), 0.03)
        rod(p, 'Wood', (-4.5, y, G + 2.2), (-5.0, y, G), 0.03)


def walls(p):
    """Os muros do pátio (a borda verde, o topo redondo), os pilaretes, as torres redondas da frente, a areia no pé."""
    r = GATE_W / 2 + 1.9
    yt = TOWER_C - TOWER_R0 + 0.3
    # a frente: 2 trechos entre as torres e o portão
    for s in (-1, 1):
        wall_seg(p, (WF, s * r), (WF, s * yt), WALL, t=T)
    # os lados (das torres até o muro de trás) e o fundo
    for s in (-1, 1):
        wall_seg(p, (TOWER_C - TOWER_R0 + 0.3, s * WS), (WB, s * WS), WALL, t=T)
    wall_seg(p, (WB, -WS), (WB, WS), WALL, t=T)
    # os pilaretes (o vergalhão nos 2 cantos de trás)
    for s in (-1, 1):
        for x in (6.4, 0.6, -5.2, -10.0):
            pilaster(p, x, s * WS, WALL + 0.55)
        pilaster(p, WB, s * WS, WALL + 0.75, s=0.55, rebar=True)
        pilaster(p, WF, s * (r + 0.4), WALL + 0.55)
        pilaster(p, WF, s * 8.6, WALL + 0.55)
    for y in (-8.8, -4.4, 0.0, 4.4, 8.8):
        pilaster(p, WB, y, WALL + 0.55)
    # as torres redondas dos cantos da frente, com as bandeiras e a porta de dentro
    for s in (-1, 1):
        c = (TOWER_C, s * TOWER_C)
        round_tower(p, c, TOWER_R0, TOWER_R1, TOWER_H, merlons=8)
        flagpole(p, c[0] - 0.3, c[1], G + TOWER_H + 0.45, 3.2, length=1.4, height=0.9, phase=0.3 + s)
        a = math.radians(225 if s < 0 else 135)
        q = (c[0] + (TOWER_R0 - 0.05) * math.cos(a), c[1] + (TOWER_R0 - 0.05) * math.sin(a))
        p.obox('Wood', (q[0], q[1], G + 1.1), (0.12, 0.95, 2.1), yaw=math.degrees(a), bevel=0.0)
        # as seteiras (as frestas escuras) viradas pra fora: a frente e o lado
        for (deg, z) in ((0, 2.6), (0, 3.9), (s * 90, 3.3), (s * 45, 2.2)):
            a2 = math.radians(deg)
            rr = TOWER_R0 - (TOWER_R0 - TOWER_R1) * (z / TOWER_H) - 0.02
            p.obox('Dark', (c[0] + rr * math.cos(a2), c[1] + rr * math.sin(a2), G + z), (0.08, 0.16, 0.6), yaw=deg, bevel=0.0)
    # os rabiscos verdes nas faces de fora (a frente e o lado −Y, o que a câmera do jogo vê) e um remendo de chapa
    scrawl(p, (WF + T / 2, -7.6, G + 1.6), '+x', size=1.7, seed=1)
    scrawl(p, (WF + T / 2, 8.0, G + 1.5), '+x', size=1.5, seed=2)
    scrawl(p, (3.6, -WS - T / 2, G + 1.7), '-y', size=1.8, seed=3)
    scrawl(p, (-7.8, -WS - T / 2, G + 1.5), '-y', size=1.4, seed=4)
    scrawl(p, (-2.0, WS + T / 2, G + 1.6), '+y', size=1.6, seed=5)
    p.obox('Rust', (-1.9, -WS - T / 2 - 0.03, G + 0.95), (1.5, 0.04, 1.2), roll=0, yaw=0, bevel=0.0)
    for k in range(6):
        p.box('Dark', -2.6 + k * 0.28, -2.56 + k * 0.28, -WS - T / 2 - 0.06, -WS - T / 2 - 0.05, G + 0.4, G + 1.5, bevel=0.0)
    # a areia acumulada do lado de fora
    drift(p, (WF + T / 2, -r - 0.3), (WF + T / 2, -yt), (1, 0), seed=1)
    drift(p, (WF + T / 2, r + 0.3), (WF + T / 2, yt), (1, 0), seed=2)
    drift(p, (10.8, -WS - T / 2), (-13.0, -WS - T / 2), (0, -1), seed=3)
    drift(p, (10.8, WS + T / 2), (-13.0, WS + T / 2), (0, 1), seed=4)
    drift(p, (WB - T / 2, -13.0), (WB - T / 2, 13.0), (-1, 0), seed=5)


def base():
    p = new_part(PREFIX + '_Base')
    # o lote: o chão batido com a saia (desnível) e a borda escura, como a Oficina
    open_box(p, 'Earth', -HX, HX, -HY, HY, -1.2, G, bevel=0.08)
    ring_boxes(p, 'Dark', -HX - 0.03, HX + 0.03, -HY - 0.03, HY + 0.03, G - 0.12, G - 0.09, 0.25, bevel=0.0)
    walls(p)
    gatehouse(p)
    main_building(p)
    wing(p)
    stair(p)
    antenna(p)
    salvage_corner(p)
    generator_shed(p)
    meeting_tarp(p)
    # as pedras do caminho do portão até o iwan (o chão que o Worker pisa)
    for k in range(9):
        x = 1.6 + k * 1.15
        for j in range(2):
            y = (j - 0.5) * 1.0 + (_hash(k, j, 31) - 0.5) * 0.3
            p.obox('Plaster', (x + (_hash(k, j, 33) - 0.5) * 0.25, y, G + 0.02), (0.85, 0.75, 0.06), yaw=_hash(k, j, 35) * 30, bevel=0.02)
    return finish(p)


def dish():
    """O prato velho de pétalas (o de banda C dos telhados do Oriente Médio, reaproveitado): origem no eixo do cano,
    na altura do colar; gira em Z. O colar, o braço, a mesa de elevação, a casca com as nervuras e o aro, o braço do
    alimentador com a caixa (LNB), o pano verde amarrado."""
    p = new_part(PREFIX + '_Dish')
    p.cyl('Trim', (0.0, 0.0, 0.25), 'z', 0.17, 0.5, seg=16, bevel=0.0)
    p.cyl('Dark', (0.0, 0.0, 0.03), 'z', 0.22, 0.06, seg=16, bevel=0.0)
    el = math.radians(32.0)
    N = Vector((math.cos(el), 0.0, math.sin(el)))
    U = Vector((-math.sin(el), 0.0, math.cos(el)))
    Y = Vector((0.0, 1.0, 0.0))
    AX = Vector((0.62, 0.0, 0.62))                        # o centro de trás do prato
    R, F, TH = 1.32, 0.95, 0.05
    rod(p, 'Trim', (0.12, 0.0, 0.32), tuple(AX - N * 0.25), 0.06)
    rod(p, 'Trim', (0.12, 0.0, 0.08), tuple(AX - N * 0.3 - U * 0.3), 0.045)
    p.cyl('Dark', tuple(AX - N * 0.12), tuple(N), 0.28, 0.22, seg=16, bevel=0.0)

    def pt(r, t, back=0.0):
        return AX + N * (r * r / (4 * F) - back) + (U * math.cos(t) + Y * math.sin(t)) * r
    b = p.bm('Cloth#dish', 0.0)
    seg, nr = 40, 6
    radii = [0.1 + (R - 0.1) * i / nr for i in range(nr + 1)]
    fr = [[b.verts.new(V(*pt(r, 2 * math.pi * k / seg))) for k in range(seg)] for r in radii]
    bk = [[b.verts.new(V(*pt(r, 2 * math.pi * k / seg, TH))) for k in range(seg)] for r in radii]
    for i in range(nr):
        for k in range(seg):
            j = (k + 1) % seg
            b.faces.new([fr[i][k], fr[i][j], fr[i + 1][j], fr[i + 1][k]]).smooth = True
            b.faces.new([bk[i + 1][k], bk[i + 1][j], bk[i][j], bk[i][k]]).smooth = True
    for ring_f, ring_b in ((fr[0], bk[0]), (fr[-1], bk[-1])):
        for k in range(seg):
            j = (k + 1) % seg
            b.faces.new([ring_f[k], ring_b[k], ring_b[j], ring_f[j]])
    # as nervuras das pétalas (na frente) e o aro de ferrugem
    for k in range(12):
        t = 2 * math.pi * k / 12
        rings = []
        for i in range(6):
            r = 0.15 + (R - 0.15) * i / 5
            q = pt(r, t, -0.012)
            tv = -U * math.sin(t) + Y * math.cos(t)
            rings.append([tuple(q - tv * 0.025), tuple(q + tv * 0.025), tuple(q + tv * 0.025 + N * 0.03), tuple(q - tv * 0.025 + N * 0.03)])
        p.loft('Rust#s', rings, bevel=0.0)
    for k in range(seg):
        t0, t1 = 2 * math.pi * k / seg, 2 * math.pi * (k + 1) / seg
        rod(p, 'Rust', tuple(pt(R, t0, TH / 2)), tuple(pt(R, t1, TH / 2)), 0.045, seg=6)
    # o alimentador: 3 braços do aro até a caixa no foco
    Fp = AX + N * F
    for k in range(3):
        t = 2 * math.pi * k / 3 + math.pi / 2
        rod(p, 'Trim', tuple(pt(R * 0.95, t, -0.02)), tuple(Fp - N * 0.08), 0.022, seg=5)
    p.cyl('Dark', tuple(Fp), tuple(N), 0.11, 0.3, seg=12, bevel=0.0)
    p.cyl('Trim', tuple(Fp + N * 0.17), tuple(N), 0.13, 0.05, seg=12, bevel=0.0)
    # o pano verde amarrado no braço
    p.obox('Player', (0.3, 0.0, 0.2), (0.12, 0.08, 0.45), roll=8, pitch=-6, bevel=0.0)
    return finish(p)


def main():
    clear_scene()
    total = export(base(), OUT)
    clear_scene()
    total += export(bake_in_place(dish(), DISH_AT), OUT)   # (31/10) o cozimento com o prato no lugar dele
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -HX, HX, -HY, HY, ROOF + 0.8, post='Wood', plank='Crate', stripe='Hazard'), OUT)
    print('MODEL total', total, 'tris')
    print('MODEL lote 28 x 28 m; o predio', round(ROOF, 2), 'm; a cupula ate', round(ROOF + DRUM_H + 0.08 + DOME_H + 0.53, 2),
          'm; a antena ate', ANT_TOP + 0.25, 'm; dish at_m', tuple(round(v, 3) for v in DISH_AT), 'rpm 4')


main()
