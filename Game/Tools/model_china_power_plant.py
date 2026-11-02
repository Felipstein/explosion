"""O Nuclear Reactor da China provisório, modelado no Blender por script (o elenco das 3 facções, 31/10/2026: docs/ELENCO.md §4.1).

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_china_power_plant.py -- captures/game/models/china_power_plant

A palavra: AS DUAS TORRES DE RESFRIAMENTO em ampulheta (o estudo do §3b de docs/CONCEITO_CONSTRUCOES.md: "dois círculos
fumegando"), com o DOMO DO REATOR na frente, no meio delas. A referência é a usina nuclear de verdade (Daya Bay, as VVER
russas, as torres hiperbólicas de concreto com a cintura e a boca alargada, os pés em V por onde o ar entra; o prédio de
contenção cilíndrico com os contrafortes dos cabos de protensão e o domo), estilizada como os prédios do ZH: poucas formas
grandes. A China arrasa (docs/ELENCO.md §7.2: o reator derrete quando cai): ele tem de parecer perigoso e poderoso.

- as 2 torres (atrás): a casca em ampulheta (a tangente vertical na cintura, a 71% da altura, e a curva parabólica dos dois
  lados: a cintura marcada e a boca alargada), o lado de dentro escuro, o aro de cima, a faixa vermelha perto da boca, a
  ESTRELA VERMELHA pintada na casca virada pra frente (seguindo a curva), os pés em V, a bacia; as balizas no aro (Beacon);
- o domo do reator (frente, no meio): o cilindro de concreto claro com os contrafortes, o ANEL VERMELHO na linha do domo, o
  domo com o TREVO DA RADIAÇÃO amarelo e preto pintado em cima (lê de cima), a janela redonda com o brilho do núcleo (Light) e
  o anel de perigo pintado no chão em volta;
- a tubulação grossa: em cada torre, a linha quente (alta, revestida, clara) e a fria (baixa, de aço, com a válvula), e a
  linha de vapor até a chaminé de ventilação listrada de vermelho e branco (a bíblia) entre as torres;
- o bloco de controle (frente-esquerda): baixo, de paredes inclinadas, as janelas, a porta, o telhado de pagode pequeno, a
  estrela, o corredor coberto até a eclusa do domo;
- os transformadores (frente-direita) com os isoladores, o pórtico de saída da linha e a cerca.
Os estandartes: 2 (a bíblia: 3 ou 4 no máximo).

O lote: 12 × 16 m (x −6..6, y −8..8), a frente em +X. Alturas: as torres 10,4 m, a chaminé 12,3 m (+ a baliza), o domo 5,7 m,
o bloco de controle ~4,0 m.

Peças (origem no chão, no centro do lote; coordenadas da Unreal, X frente, Y direita, Z cima, metros):
  SM_China_PowerPlant_Base: tudo (não tem peça que gire; a vida é o vapor das torres, o núcleo e as balizas, do jogo);
  SM_China_PowerPlant_Scaffold: o andaime da obra (tubos de aço, scaffold() do model_lib).
Materiais (slots do model_faction_kit.py): CnConcrete, CnPlinth, CnRoof, CnMetal, CnGold, Player (o vermelho: faixa das
torres, anel do domo, listras da chaminé, estandartes, estrela, borda do telhado), Paint (o branco da chaminé e o revestimento
da linha quente, os isoladores), Yellow (o trevo), Hazard, Dark, Glass, Light (o núcleo e as luminárias), Beacon.
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, scaffold  # noqa: E402,F401
from model_faction_kit import disc_ring, foundation, lamp, revolve, bayi_strokes, star_points, star_wall  # noqa: E402
from model_building_parts import BPart, WallFace, wbox, cn_block, cn_window, pipe_run  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'china_power_plant')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_China_PowerPlant'

HX, HY = 6.0, 8.0                   # o lote 12 × 16
Z0 = 0.14
# as torres de resfriamento
TOWERS = ((-2.75, -4.7), (-2.75, 4.7))
T_RB, T_RW, T_RT, T_H = 2.7, 1.52, 2.12, 10.4     # raio no pé da casca, na cintura, na boca; altura
T_ZB = Z0 + 1.7                     # o pé da casca (em cima dos pés em V)
T_WAIST = 0.71                      # a cintura (fração da altura da casca)
# o domo do reator
DC = (2.0, 0.0)
DR, DZS, DH = 2.6, 3.5, 2.2          # raio, a linha do domo (o topo do cilindro), a altura do domo
# o bloco de controle e os transformadores
CTRL = (2.3, 5.0, -7.0, -3.5)
CTRL_H = 3.0
YARD = (2.2, 5.7, 3.3, 7.6)
STACK = (-4.45, 0.0)


# ===================================================================================================== utilidades
def sgn(v):
    return (v > 1e-9) - (v < -1e-9)


def lerp(a, b, t):
    return a + (b - a) * t


def tube(p, m, a, b, r, seg=8):
    """Tubo reto de a até b (sem chanfro: chave própria)."""
    d = Vector(b) - Vector(a)
    if d.length < 1e-4:
        return
    c = (Vector(a) + Vector(b)) / 2
    p.cyl(m.split('#')[0] + '#t', (c.x, c.y, c.z), (d.x, d.y, d.z), r, d.length, seg=seg, bevel=0.0)


def report(o):
    cnt = {}
    for poly in o.data.polygons:
        n = o.data.materials[poly.material_index].name
        cnt[n] = cnt.get(n, 0) + len(poly.vertices) - 2
    print('TRIS', o.name, sorted(cnt.items(), key=lambda kv: -kv[1]))


def batter(p, m, x0, x1, y0, y1, z0, z1, ins, bevel=0.05):
    p.loft(m, [[(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)],
               [(x0 + ins, y0 + ins, z1), (x1 - ins, y0 + ins, z1), (x1 - ins, y1 - ins, z1), (x0 + ins, y1 - ins, z1)]], bevel=bevel)


def band_rev(p, m, c, prof, out=0.035, inn=0.03, seg=48):
    """Uma faixa colada numa superfície de revolução: prof = [(r, z)...] de baixo pra cima, sobre a superfície."""
    ring = [(prof[0][0] - inn, prof[0][1])] + [(r + out, z) for (r, z) in prof] + [(prof[-1][0] - inn, prof[-1][1])]
    revolve(p, m, c, ring, seg=seg, smooth=True, caps=(False, False))


# (31/10, o detalhe AAA: docs/ESTUDO_DETALHE_AAA.md) a telha-canal redonda, a mesma da War Factory
from model_building_parts import round_prof as _round_prof, sweep as _sweep  # noqa: E402
_TILE = _round_prof(0.17, 0.085, base=-0.03, n=7)


def _tile(p, m, pts):
    """Uma fileira de telha-canal (o rolo redondo) com a telha de remate redonda na ponta do beiral."""
    _sweep(p, m, pts, _TILE, smooth=True)
    P0, P1 = Vector(pts[0]), Vector(pts[1])
    d = (P1 - P0).normalized()
    h = (Vector((0.0, 0.0, 1.0)) - d * d.z).normalized()
    p.cyl(m, tuple(P0 + h * 0.03 - d * 0.015), tuple(d), 0.1, 0.05, seg=12, bevel=0.0)


# ------------------------------------------------------------------ o telhado chinês (o mesmo do Command Center)
U_ROOF = (0.0, 0.10, 0.22, 0.36, 0.52, 0.70, 0.86, 1.0)
U_RIB = (0.0, 0.22, 0.52, 0.86)
PW = 2.6


def ring_uv(nx, ny):
    out = []
    for k in range(nx):
        out.append((-1 + 2 * k / nx, -1.0))
    for k in range(ny):
        out.append((1.0, -1 + 2 * k / ny))
    for k in range(nx):
        out.append((1 - 2 * k / nx, 1.0))
    for k in range(ny):
        out.append((-1.0, 1 - 2 * k / ny))
    return out


def roof_point(R, u, ux, uy):
    sx = lerp(R['SXe'], R['SXt'], u)
    sy = lerp(R['SYe'], R['SYt'], u)
    w = min(abs(ux), abs(uy)) ** PW
    decay = (1 - u) ** 2
    k = R['kick'] * w * decay
    return (R['cx'] + ux * sx + sgn(ux) * k, R['cy'] + uy * sy + sgn(uy) * k,
            R['z0'] + R['rise'] * u ** 1.5 + R['curl'] * w * decay)


def cn_roof(p, cx, cy, wx, wy, z0, rise, over, top, curl=0.55, kick=0.35, ridge_h=0.0, thick=0.35, pitch=0.34,
            m='CnRoof', edge='Player', orn='CnGold', ribs=True, corner_tips=True):
    """O telhado chinês de um andar (o mesmo desenho do model_china_command_center.py: o quadril côncavo, a fáscia vermelha
    seguindo o beiral curvo, as quinas levantadas, as cumeeiras dos cantos, as fileiras de telha e os ornamentos)."""
    R = {'cx': cx, 'cy': cy, 'SXe': wx + over, 'SYe': wy + over, 'SXt': top[0], 'SYt': top[1], 'z0': z0, 'rise': rise,
         'curl': curl, 'kick': kick}
    nx = max(6, 2 * round(R['SXe'] / 0.75))
    ny = max(6, 2 * round(R['SYe'] / 0.75))
    uv = ring_uv(nx, ny)
    rings = [[(cx + ux * (R['SXe'] - 0.45), cy + uy * (R['SYe'] - 0.45), z0 - thick) for (ux, uy) in uv]]
    rings.append([(q[0], q[1], q[2] - 0.1) for q in (roof_point(R, 0.0, ux, uy) for (ux, uy) in uv)])
    for u in U_ROOF:
        rings.append([roof_point(R, u, ux, uy) for (ux, uy) in uv])
    p.loft(m, rings, bevel=0.03)
    b = p.bm(edge + '#fascia', 0.0)
    secs = []
    for (ux, uy) in uv:
        P = roof_point(R, 0.0, ux, uy)
        nx_ = sgn(ux) if abs(ux) > 0.999 else 0
        ny_ = sgn(uy) if abs(uy) > 0.999 else 0
        sec = [(P[0] + nx_ * 0.06, P[1] + ny_ * 0.06, P[2] + 0.04), (P[0] + nx_ * 0.06, P[1] + ny_ * 0.06, P[2] - 0.26),
               (P[0] - nx_ * 0.24, P[1] - ny_ * 0.24, P[2] - 0.26), (P[0] - nx_ * 0.24, P[1] - ny_ * 0.24, P[2] + 0.04)]
        secs.append([b.verts.new(V(*q)) for q in sec])
    for i in range(len(secs)):
        A, B = secs[i], secs[(i + 1) % len(secs)]
        for j in range(4):
            b.faces.new([A[j], A[(j + 1) % 4], B[(j + 1) % 4], B[j]])
    for (ux, uy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        sx_, sy_ = -uy / math.sqrt(2), ux / math.sqrt(2)
        pts = [roof_point(R, u, ux, uy) for u in U_ROOF]
        _sweep(p, m + '#hip', pts, _round_prof(0.3, 0.24, base=-0.05, n=7), smooth=True)        # o rolo redondo da cumeeira
        if corner_tips:
            q = pts[0]
            yaw = math.degrees(math.atan2(uy, ux))
            dx, dy = ux / math.sqrt(2), uy / math.sqrt(2)
            p.obox(edge, (q[0] + dx * 0.22, q[1] + dy * 0.22, q[2] + 0.16), (0.55, 0.16, 0.13), yaw=yaw, pitch=38, bevel=0.02)
            p.obox(edge, (q[0] + dx * 0.45, q[1] + dy * 0.45, q[2] + 0.37), (0.22, 0.13, 0.13), yaw=yaw, pitch=68, bevel=0.02)
    if ribs:
        rb = p.bm(m + '#ribs', 0.0)
        for (axis, sgn_side) in (('x', -1), ('x', 1), ('y', -1), ('y', 1)):
            se, st = (R['SXe'], R['SXt']) if axis == 'x' else (R['SYe'], R['SYt'])
            n = int(2 * se / pitch)
            for i in range(n):
                a = -se + (i + 0.5) * 2 * se / n
                u_end = 1.0 if abs(a) <= st else (se - abs(a) - 0.15) / (se - st)
                if u_end < 0.12:
                    continue
                us = [u for u in U_RIB if u < u_end - 0.05] + [min(u_end, 1.0)]
                ring_pts = []
                for u in us:
                    s = lerp(se, st, u)
                    t = max(-1.0, min(1.0, a / s))
                    q = roof_point(R, u, t, sgn_side) if axis == 'x' else roof_point(R, u, sgn_side, t)
                    ring_pts.append(q)
                # (31/10, o detalhe AAA) a telha-canal redonda (a onda que pega o sol em listras) e a telha de remate no beiral
                if len(ring_pts) >= 2:
                    _tile(p, m + '#ribs', ring_pts)
    ztop = z0 + rise
    if ridge_h > 0:
        L = top[1] if top[1] >= top[0] else top[0]
        along_y = top[1] >= top[0]
        if along_y:
            p.box(m, cx - 0.15, cx + 0.15, cy - L - 0.1, cy + L + 0.1, ztop - 0.1, ztop + ridge_h, bevel=0.04)
        else:
            p.box(m, cx - L - 0.1, cx + L + 0.1, cy - 0.15, cy + 0.15, ztop - 0.1, ztop + ridge_h, bevel=0.04)
        for s in (-1, 1):
            ex, ey = (cx, cy + s * (L + 0.05)) if along_y else (cx + s * (L + 0.05), cy)
            zb = ztop + ridge_h
            if along_y:
                p.obox(orn, (ex, ey, zb + 0.2), (0.22, 0.24, 0.44), roll=s * 12, bevel=0.03)
                p.obox(orn, (ex, ey - s * 0.13, zb + 0.44), (0.2, 0.3, 0.13), roll=-s * 30, bevel=0.02)
            else:
                p.obox(orn, (ex, ey, zb + 0.2), (0.24, 0.22, 0.44), pitch=-s * 12, bevel=0.03)
                p.obox(orn, (ex - s * 0.13, ey, zb + 0.44), (0.3, 0.2, 0.13), pitch=s * 30, bevel=0.02)
    return ztop


def banner2(p, pole, z0, top, height=3.0, width=1.1, face='+x', m='Player', star='CnGold', pole_m='CnMetal'):
    """O estandarte da War Factory aprovada: o pano vermelho com a estrela dourada e o fio dourado embaixo dela."""
    x, y = pole
    p.cyl(pole_m, (x, y, (z0 + top) / 2), 'z', 0.07, top - z0, seg=10, bevel=0.0)
    p.cyl(star, (x, y, top + 0.14), 'z', 0.11, 0.28, seg=10, r2=0.02, bevel=0.0)
    s = 1 if face[0] == '+' else -1
    off = 0.1 * s
    zt = top - 0.18
    if face in ('+x', '-x'):
        p.cyl(pole_m, (x + off, y, zt), 'y', 0.04, width + 0.24, seg=6, bevel=0.0)
        p.box(m, x + off - 0.02, x + off + 0.02, y - width / 2, y + width / 2, zt - height, zt, bevel=0.0)
        star_wall(p, (x + off + 0.02 * s, y, zt - height * 0.2), face, width * 0.27, m=star, t=0.012)
        p.box(star, x + off + 0.02 * s - 0.006, x + off + 0.02 * s + 0.006, y - 0.035, y + 0.035, zt - height * 0.86, zt - height * 0.36, bevel=0.0)
        p.box(star, x + off - 0.025, x + off + 0.025, y - width / 2, y + width / 2, zt - height - 0.06, zt - height, bevel=0.0)


# ===================================================================================================== as peças
def tower_r(z):
    """O raio da casca na altura z: a tangente vertical na cintura e a curva parabólica dos dois lados (a hipérbole de
    verdade quase vira cone no pé e a ampulheta não lê; a parábola espalha a curva pela altura toda: a cintura marcada e a
    boca alargada, o desenho dos RTS); r = T_RB no pé da casca e T_RT na boca."""
    zw = T_ZB + (T_H - T_ZB) * T_WAIST
    if z <= zw:
        return T_RW + (T_RB - T_RW) * ((zw - z) / (zw - T_ZB)) ** 1.8
    return T_RW + (T_RT - T_RW) * ((z - zw) / (T_H - zw)) ** 1.8


def cooling_tower(p, c, seg=48):
    zs = [lerp(T_ZB, T_H, (k / 22) ** 1.15) for k in range(23)]
    prof = [(tower_r(z), z) for z in zs]
    revolve(p, 'CnConcrete#shell', c, prof, seg=seg, smooth=True, caps=(False, False))
    # (01/11, a galeria: "no topo das usinas, por dentro, ficou vazando") o lado de dentro era uma superfície solta, sem espessura:
    # o Part.finish recalcula as normais de cada ilha pra fora do centro dela, e a casca de dentro saía com a normal virada pra
    # fora (pra parede); na Unreal (face de um lado só) a parede do fundo sumia vista de cima e a boca mostrava a areia e os
    # tanques através da torre. Agora o lado de dentro é um SÓLIDO FECHADO (o perfil fechado girado: a face de dentro a 0,2 m da
    # casca desce, o pé fecha, a face de fora sobe a 0,05 m dela, o topo fecha dentro do aro): as normais saem certas sempre, a
    # face de dentro olha pro eixo. As pontas entram no aro e na viga do pé (nada solto, nada a menos de 2,5 cm de outra face:
    # o corte dos fundos do BPart não abre o sólido) e os anéis a 3 cm das pontas prendem o sombreado liso.
    zs_in = [T_H] + [z for z in reversed(zs) if T_ZB + 0.03 < z < T_H - 0.03] + [T_ZB]
    zs_in = [zs_in[0], zs_in[0] - 0.03] + zs_in[1:-1] + [zs_in[-1] + 0.03, zs_in[-1]]
    inner = [(tower_r(z) - 0.2, z) for z in zs_in] + [(tower_r(z) - 0.05, z) for z in reversed(zs_in)]
    revolve(p, 'Dark#in', c, inner + [inner[0]], seg=seg, smooth=True, caps=(False, False))
    # o fundo fechado: o deque do enchimento (o "fill" das torres de verdade, molhado e escuro) logo acima da viga do pé, preso
    # dentro do sólido escuro; de cima, pela cintura, o que se vê é ele (o vapor é do jogo)
    rd = tower_r(T_ZB + 0.25) - 0.12
    p.cyl('Dark#deck', (c[0], c[1], T_ZB + 0.17), 'z', rd, 0.16, seg=seg, bevel=0.0)
    # o aro de cima e a viga do pé (fecham a espessura da casca)
    disc_ring(p, 'CnPlinth', c, T_RT + 0.1, T_RT - 0.26, T_H - 0.06, T_H + 0.2, seg=seg)
    rb = tower_r(T_ZB)
    disc_ring(p, 'CnPlinth', c, rb + 0.1, rb - 0.32, T_ZB - 0.3, T_ZB + 0.04, seg=seg)
    # a faixa vermelha perto da boca e o friso claro embaixo dela
    za, zb = T_H - 1.75, T_H - 0.75
    band_rev(p, 'Player#band', c, [(tower_r(z), z) for z in (za, lerp(za, zb, 0.5), zb)], seg=seg)
    za2 = za - 0.45
    band_rev(p, 'Paint#band', c, [(tower_r(z), z) for z in (za2, za2 + 0.22)], seg=seg)
    # os pés em V (o ar entra por baixo): de cada ponto do chão, dois pilares até a viga
    n = 22
    for k in range(n):
        a = 2 * math.pi * k / n
        g = (c[0] + (rb + 0.15) * math.cos(a), c[1] + (rb + 0.15) * math.sin(a), Z0 + 0.4)
        for d in (-1, 1):
            b = a + d * math.pi / n
            t = (c[0] + (rb - 0.08) * math.cos(b), c[1] + (rb - 0.08) * math.sin(b), T_ZB - 0.2)
            tube(p, 'CnPlinth', g, t, 0.12, seg=6)
        p.box('CnPlinth#foot', g[0] - 0.22, g[0] + 0.22, g[1] - 0.22, g[1] + 0.22, Z0, Z0 + 0.42, bevel=0.015)
    # a bacia (o muro baixo em volta e a água escura dentro)
    disc_ring(p, 'CnConcrete', c, rb + 0.55, rb + 0.32, Z0 - 0.02, Z0 + 0.55, seg=seg)
    p.cyl('Dark', (c[0], c[1], Z0 + 0.15), 'z', rb + 0.33, 0.06, seg=seg, bevel=0.0)
    # as balizas vermelhas no aro (3) e a escada de serviço por dentro do aro (o corrimão do topo)
    for k in range(3):
        a = 2 * math.pi * (k + 0.25) / 3
        x, y = c[0] + (T_RT - 0.08) * math.cos(a), c[1] + (T_RT - 0.08) * math.sin(a)
        p.cyl('Dark', (x, y, T_H + 0.27), 'z', 0.09, 0.14, seg=10, bevel=0.0)
        p.cyl('Beacon', (x, y, T_H + 0.42), 'z', 0.08, 0.16, seg=10, bevel=0.0, smooth=True)


def shell_poly(p, c, ang, pts, m, lift, t, n=3):
    """Um polígono (estrelado em volta do centro) pintado na casca da torre, seguindo a curva: desenhado no plano 'desenrolado' da
    casca (s = o arco ao longo da volta, z = a altura), o leque do centro subdividido (n) e cada vértice levado pra superfície (o
    ângulo s / r(z) a partir de 'ang', o raio r(z) + lift). Sólido fechado (a face de cima, a de baixo e a borda)."""
    b = p.bm(m + '#tstar', 0.0)
    cache = {}

    def vert(s, z, top):
        key = (round(s, 5), round(z, 5), top)
        if key not in cache:
            r = tower_r(z) + lift + (t if top else 0.0)
            a = math.radians(ang) + s / tower_r(z)
            cache[key] = b.verts.new(V(c[0] + r * math.cos(a), c[1] + r * math.sin(a), z))
        return cache[key]
    C = (sum(q[0] for q in pts) / len(pts), sum(q[1] for q in pts) / len(pts))
    N = len(pts)
    for k in range(N):
        P, Q = pts[k], pts[(k + 1) % N]
        for i in range(n):
            for j in range(n - i):
                # a subdivisão baricêntrica: (C, P, Q) → triângulos pequenos
                def bary(ii, jj):
                    u, v = ii / n, jj / n
                    return (C[0] + (P[0] - C[0]) * u + (Q[0] - C[0]) * v, C[1] + (P[1] - C[1]) * u + (Q[1] - C[1]) * v)
                tris = [(bary(i, j), bary(i + 1, j), bary(i, j + 1))]
                if i + j < n - 1:
                    tris.append((bary(i + 1, j), bary(i + 1, j + 1), bary(i, j + 1)))
                for (A, B, D) in tris:
                    b.faces.new([vert(*A, True), vert(*B, True), vert(*D, True)])
                    b.faces.new([vert(*D, False), vert(*B, False), vert(*A, False)])
        for i in range(n):                                    # a borda (o lado P→Q do contorno)
            A = (P[0] + (Q[0] - P[0]) * i / n, P[1] + (Q[1] - P[1]) * i / n)
            B = (P[0] + (Q[0] - P[0]) * (i + 1) / n, P[1] + (Q[1] - P[1]) * (i + 1) / n)
            b.faces.new([vert(*A, True), vert(*A, False), vert(*B, False), vert(*B, True)])


def shell_star(p, c, ang, zc, R, lift=0.015, t=0.02, n=3):
    """A estrela pintada na casca da torre. (01/11, item 43: a aprovada, `docs/elenco/EMBLEMAS.md`) o emblema do Exército de Libertação
    Popular, como a star_wall: a borda dourada (CnGold), a estrela vermelha (Red, fixa; antes era a cor do jogador) e o 八一 dourado no
    meio. O s da casca anda pra direita de quem olha de fora no Blender; a Unreal espelha (o y troca de sinal): o 八一 vai espelhado
    pra ler de frente no jogo."""
    shell_poly(p, c, ang, star_points(0.0, zc, R * 1.12, r_in=R * 1.12 * 0.40, rot_deg=90.0), 'CnGold', lift, t, n)
    shell_poly(p, c, ang, star_points(0.0, zc, R, rot_deg=90.0), 'Red', lift + t, 0.6 * t, n)
    for q in bayi_strokes(R):
        qm = [(-u, zc + v) for (u, v) in q][::-1]   # espelhado (a ordem inverte: a normal fica pra fora)
        shell_poly(p, c, ang, qm, 'CnGold', lift + 1.6 * t, 0.4 * t, 2)


def dome_z(r):
    return DZS + DH * math.sqrt(max(0.0, 1 - (r / DR) ** 2))


def decal(p, m, r0, r1, a0, a1, nr, na, lift, t=0.02):
    """Um setor de anel pintado no domo (r0..r1, a0..a1 em graus), seguindo a curva: a grade (nr × na) projetada na
    superfície, com espessura t (sólido fechado)."""
    b = p.bm(m + '#decal', 0.0)
    top, bot = [], []
    for i in range(nr + 1):
        r = lerp(r0, r1, i / nr)
        rt_, rb_ = [], []
        for j in range(na + 1):
            a = math.radians(lerp(a0, a1, j / na))
            x, y = DC[0] + r * math.cos(a), DC[1] + r * math.sin(a)
            z = dome_z(r) + lift
            rt_.append(b.verts.new(V(x, y, z + t)))
            rb_.append(b.verts.new(V(x, y, z)))
        top.append(rt_)
        bot.append(rb_)
    for i in range(nr):
        for j in range(na):
            b.faces.new([top[i][j], top[i][j + 1], top[i + 1][j + 1], top[i + 1][j]])
            b.faces.new([bot[i + 1][j], bot[i + 1][j + 1], bot[i][j + 1], bot[i][j]])
    edge = [(0, j) for j in range(na)] + [(i, na) for i in range(nr)] + [(nr, j) for j in range(na, 0, -1)] + [(i, 0) for i in range(nr, 0, -1)]
    for (i0, j0), (i1, j1) in zip(edge, edge[1:] + edge[:1]):
        b.faces.new([top[i0][j0], bot[i0][j0], bot[i1][j1], top[i1][j1]])


def reactor(p):
    cx, cy = DC
    # a sapata, o cilindro de contenção (concreto claro), os contrafortes e o anel vermelho na linha do domo
    p.cyl('CnConcrete', (cx, cy, Z0 + 0.2), 'z', DR + 0.35, 0.5, seg=48, bevel=0.04)
    # (31/10, pronto pra degradar) a contenção com a parede de 0,5 m de verdade (a face de fora, a de dentro, os anéis), o domo com
    # a casca de 0,4 m, e por dentro o vaso do reator, o piso e o anel da ponte rolante: o furo mostra o reator
    z_a, z_b = Z0 + 0.4, DZS
    revolve(p, 'CnPlinth#wall', (cx, cy), [(DR, z_a), (DR, (z_a + z_b) / 2), (DR, z_b)], seg=48, smooth=True, caps=(False, False))
    revolve(p, 'CnPlinth#wall', (cx, cy), [(DR - 0.5, z_b), (DR - 0.5, z_a)], seg=48, smooth=True, caps=(False, False))
    disc_ring(p, 'CnPlinth#wallr', (cx, cy), DR - 0.003, DR - 0.497, z_a - 0.003, z_a + 0.003, seg=48)
    p.cyl('Dark#core', (cx, cy, Z0 + 0.5), 'z', DR - 0.45, 0.2, seg=32, bevel=0.0)                              # o piso de dentro
    p.cyl('CnMetal#core', (cx, cy, Z0 + 1.6), 'z', 0.9, 2.0, seg=24, bevel=0.0, smooth=True)                    # o vaso
    p.cyl('CnMetal#core', (cx, cy, Z0 + 2.7), 'z', 1.05, 0.2, seg=24, bevel=0.0)
    p.torus('CnMetal#core', (cx, cy, DZS - 0.3), 'z', DR - 0.7, 0.12, seg=32, rseg=6)                            # a ponte rolante
    for k in range(6):
        a = math.radians(30 + 60 * k)
        if abs(math.degrees(a) - 270) < 1:
            continue                                            # o lado da eclusa (o corredor entra aqui)
        p.obox('CnConcrete', (cx + (DR + 0.12) * math.cos(a), cy + (DR + 0.12) * math.sin(a), (Z0 + DZS) / 2 + 0.1),
               (0.42, 0.5, DZS - Z0 - 0.2), yaw=math.degrees(a), bevel=0.03)
    disc_ring(p, 'Player', (cx, cy), DR + 0.06, DR - 0.05, DZS - 0.42, DZS + 0.02, seg=48)
    disc_ring(p, 'CnGold', (cx, cy), DR + 0.07, DR - 0.05, DZS - 0.5, DZS - 0.44, seg=48)
    # o domo (meia-elipse) com o trevo da radiação pintado em cima: o fundo amarelo, a borda preta, o miolo e as 3 pás
    prof = [(DR * math.cos(t), DZS + DH * math.sin(t)) for t in [math.pi / 2 * k / 14 for k in range(15)]]
    revolve(p, 'CnPlinth#dome', (cx, cy), prof, seg=48, smooth=True, caps=(False, False))
    revolve(p, 'CnPlinth#dome', (cx, cy), [((r - 0.4) * 0.995 if r > 0.5 else 0.1, z - 0.4 * (1 - (r / DR) ** 2) - 0.002)
                                           for (r, z) in reversed(prof)], seg=48, smooth=True, caps=(False, False))
    disc_ring(p, 'CnPlinth#wallr', (cx, cy), DR - 0.003, DR - 0.497, DZS - 0.003, DZS + 0.003, seg=48)
    R1 = 0.22                                                   # o raio do miolo (as medidas do símbolo padrão)
    decal(p, 'Dark', 1.24, 1.36, 0, 360, 1, 48, 0.012)
    decal(p, 'Yellow', 0.01, 1.25, 0, 360, 4, 48, 0.012)
    decal(p, 'Dark', 0.005, R1, 0, 360, 2, 24, 0.03)
    for k in range(3):                                          # uma pá aponta pra frente (+X), como na placa
        a = 120 * k
        decal(p, 'Dark', 1.5 * R1, 5.0 * R1, a - 30, a + 30, 3, 10, 0.03)
    # a janela redonda do núcleo na frente (+X): a moldura saliente, o vidro aceso, os parafusos
    zc = 1.75
    p.cyl('CnMetal', (cx + DR - 0.05, cy, zc), 'x', 0.66, 0.5, seg=32, bevel=0.02)
    p.cyl('Dark', (cx + DR + 0.2, cy, zc), 'x', 0.52, 0.04, seg=32, bevel=0.0)
    p.cyl('Light', (cx + DR + 0.215, cy, zc), 'x', 0.46, 0.05, seg=32, bevel=0.0)
    for k in range(8):
        a = 2 * math.pi * k / 8
        p.cyl('CnGold', (cx + DR + 0.22, cy + 0.58 * math.cos(a), zc + 0.58 * math.sin(a)), 'x', 0.04, 0.04, seg=6, bevel=0.0)
    for (dy, dz) in ((0.0, 0.85),):                             # a placa de perigo sobre a janela
        p.box('Yellow', cx + DR - 0.06, cx + DR + 0.06, cy - 0.35, cy + 0.35, zc + dz - 0.05, zc + dz + 0.35, bevel=0.0)
        p.box('Dark', cx + DR + 0.06, cx + DR + 0.07, cy - 0.12, cy + 0.12, zc + dz + 0.05, zc + dz + 0.25, bevel=0.0)
    # o anel de perigo pintado no chão em volta do domo (amarelo e preto)
    n = 36
    for k in range(n):
        a0, a1 = 360 * k / n, 360 * (k + 1) / n
        m = 'Hazard' if k % 2 == 0 else 'Dark'
        pts = []
        for a in (a0, a1):
            pts.append((cx + 3.2 * math.cos(math.radians(a)), cy + 3.2 * math.sin(math.radians(a))))
        for a in (a1, a0):
            pts.append((cx + 3.5 * math.cos(math.radians(a)), cy + 3.5 * math.sin(math.radians(a))))
        p.loft(m + '#ring', [[(x, y, Z0 + 0.001) for (x, y) in pts], [(x, y, Z0 + 0.013) for (x, y) in pts]], bevel=0.0)
    # as luminárias no pé (iluminam o domo de noite)
    for a in (35, -35, 150, -150):
        r = DR + 0.75
        x, y = cx + r * math.cos(math.radians(a)), cy + r * math.sin(math.radians(a))
        p.obox('Dark', (x, y, Z0 + 0.3), (0.35, 0.35, 0.3), yaw=a, bevel=0.0)
        p.obox('Light', (x - 0.17 * math.cos(math.radians(a)), y - 0.17 * math.sin(math.radians(a)), Z0 + 0.35), (0.04, 0.26, 0.18), yaw=a, bevel=0.0)


def pipes(p):
    cx, cy = DC
    for (tx, ty) in TOWERS:
        d = Vector((tx - cx, ty - cy, 0.0))
        dist = d.length
        d.normalize()
        # a linha quente (alta, revestida de alumínio: clara), do domo até a casca da torre, com os flanges
        z = 4.55
        a = (cx + d.x * (DR * math.sqrt(1 - ((z - DZS) / DH) ** 2) - 0.25), cy + d.y * (DR * math.sqrt(1 - ((z - DZS) / DH) ** 2) - 0.25), z)
        b = (tx - d.x * (tower_r(z) - 0.25), ty - d.y * (tower_r(z) - 0.25), z)
        tube(p, 'Paint', a, b, 0.42, seg=20)
        L = math.dist(a, b)
        for t in (0.22, 0.5, 0.78):
            q = (lerp(a[0], b[0], t), lerp(a[1], b[1], t), z)
            tube(p, 'CnMetal', (q[0] - d.x * 0.06, q[1] - d.y * 0.06, z), (q[0] + d.x * 0.06, q[1] + d.y * 0.06, z), 0.5, seg=20)
        q = (lerp(a[0], b[0], 0.5), lerp(a[1], b[1], 0.5))
        p.box('CnConcrete', q[0] - 0.35, q[0] + 0.35, q[1] - 0.35, q[1] + 0.35, Z0, z - 0.4, bevel=0.03)   # o pilar
        tube(p, 'CnMetal', (q[0], q[1], z - 0.42), (q[0], q[1], z - 0.4), 0.45)
        # a linha fria (baixa, de aço), entrando entre os pés da torre, com a válvula de volante vermelho
        z2 = 1.05
        a2 = (cx + d.x * (DR - 0.1), cy + d.y * (DR - 0.1), z2)
        b2 = (tx - d.x * (T_RB - 0.4), ty - d.y * (T_RB - 0.4), z2)
        tube(p, 'CnMetal', a2, b2, 0.34, seg=18)
        for t in (0.15, 0.85):
            q2 = (lerp(a2[0], b2[0], t), lerp(a2[1], b2[1], t), z2)
            tube(p, 'Dark', (q2[0] - d.x * 0.06, q2[1] - d.y * 0.06, z2), (q2[0] + d.x * 0.06, q2[1] + d.y * 0.06, z2), 0.42, seg=18)
            p.box('CnConcrete', q2[0] - 0.3, q2[0] + 0.3, q2[1] - 0.3, q2[1] + 0.3, Z0, z2 - 0.3, bevel=0.02)
        q2 = (lerp(a2[0], b2[0], 0.5), lerp(a2[1], b2[1], 0.5), z2)
        p.cyl('CnMetal', (q2[0], q2[1], z2 + 0.15), 'z', 0.42, 0.55, seg=16, bevel=0.02)                  # o corpo da válvula
        p.cyl('CnMetal', (q2[0], q2[1], z2 + 0.75), 'z', 0.05, 0.7, seg=8, bevel=0.0)
        p.torus('Player#wheel', (q2[0], q2[1], z2 + 1.1), 'z', 0.32, 0.04, seg=20, rseg=6)
        for k in range(3):
            aa = math.pi * k / 3
            tube(p, 'Player', (q2[0] - 0.3 * math.cos(aa), q2[1] - 0.3 * math.sin(aa), z2 + 1.1),
                 (q2[0] + 0.3 * math.cos(aa), q2[1] + 0.3 * math.sin(aa), z2 + 1.1), 0.025, seg=6)
    # a linha de vapor do domo até a chaminé (por trás, entre as torres)
    z3 = 4.0
    xa = cx - DR * math.sqrt(1 - ((z3 - DZS) / DH) ** 2) + 0.2
    tube(p, 'CnMetal', (xa, 0.0, z3), (STACK[0] + 0.3, 0.0, z3), 0.26, seg=16)
    for x in (lerp(xa, STACK[0], 0.35), lerp(xa, STACK[0], 0.7)):
        tube(p, 'Dark', (x - 0.05, 0, z3), (x + 0.05, 0, z3), 0.33, seg=16)
        p.box('CnConcrete', x - 0.22, x + 0.22, -0.22, 0.22, Z0, z3 - 0.24, bevel=0.02)


def stack(p):
    """A chaminé de ventilação listrada de vermelho e branco (a da bíblia), com a plataforma, a escada e a baliza."""
    x, y = STACK
    p.cyl('CnConcrete', (x, y, Z0 + 0.4), 'z', 0.85, 0.8, seg=24, bevel=0.03)
    h, r0, r1, z0 = 11.4, 0.5, 0.36, Z0 + 0.8
    nb = 10
    for k in range(nb):
        za, zb = z0 + h * k / nb, z0 + h * (k + 1) / nb
        ra, rb = lerp(r0, r1, k / nb), lerp(r0, r1, (k + 1) / nb)
        m = ('Player' if k % 2 else 'Paint') + '#stack'
        p.cyl(m, (x, y, (za + zb) / 2), 'z', ra, zb - za, seg=24, r2=rb, bevel=0.0, smooth=True)
    zt = z0 + h
    p.cyl('Dark', (x, y, zt + 0.08), 'z', r1 + 0.06, 0.16, seg=24, bevel=0.0)
    p.cyl('Dark', (x, y, zt + 0.12), 'z', r1 * 0.75, 0.12, seg=16, bevel=0.0)
    p.cyl('Beacon', (x, y, zt + 0.3), 'z', 0.1, 0.2, seg=12, bevel=0.0, smooth=True)
    # a plataforma de serviço com o guarda-corpo, a 2/3 da altura, e a escada de marinheiro
    zp = z0 + h * 0.62
    disc_ring(p, 'CnMetal', (x, y), 0.95, lerp(r0, r1, 0.62) - 0.02, zp - 0.08, zp, seg=24)
    for k in range(10):
        a = 2 * math.pi * k / 10
        p.cyl('CnMetal#t', (x + 0.92 * math.cos(a), y + 0.92 * math.sin(a), zp + 0.45), 'z', 0.025, 0.9, seg=6, bevel=0.0)
    p.torus('CnMetal#t', (x, y, zp + 0.9), 'z', 0.92, 0.03, seg=24, rseg=6)
    for s in (-1, 1):
        tube(p, 'CnMetal', (x + 0.55, y + s * 0.2, z0), (x + 0.42, y + s * 0.2, zt - 0.2), 0.025, seg=6)
    for k in range(int((zt - z0) / 0.6)):
        zz = z0 + 0.3 + k * 0.6
        t = (zz - z0) / (zt - z0)
        xr = lerp(x + 0.55, x + 0.42, t)
        tube(p, 'CnMetal', (xr, y - 0.2, zz), (xr, y + 0.2, zz), 0.02, seg=4)


def control(p):
    x0, x1, y0, y1 = CTRL
    # (31/10, o detalhe AAA) o bloco OCO de placas (as paredes de 0,35 m inclinadas, os pilares de quina, a cornija), a faixa de
    # janelas e a porta nos vãos de verdade (o caixilho recuado, o peitoril)
    wy0, wy1 = y0 + 0.35, y0 + 1.85
    lean = 0.15 / (CTRL_H - Z0 + 0.05)
    cn_block(p, x0, x1, y0, y1, Z0 - 0.05, CTRL_H, t=0.35, lean={f: lean for f in ('+x', '-x', '+y', '-y')},
             skin=('+x', '-x', '-y'), holes={'+x': [(wy0, wy1, 1.2, 2.15), (y1 - 1.2, y1 - 0.4, Z0 - 0.05, 2.2)],
                                            '-y': [(xx - 0.4, xx + 0.4, 1.4, 2.5) for xx in (3.0, 4.3)]},
             roof_t=0.25, parapet=0.0, corners=True, pier=0.6, interior=True, plinth=0.55, pw=1.4, zs=[Z0 - 0.05, 1.05, 2.3, CTRL_H - 0.25])
    lean_x = lean
    Fx = WallFace('+x', x1, Z0 - 0.05, lean_x)
    cn_window(p, Fx, wy0, wy1, 1.2, 2.15, t=0.35, mull=3)
    Fy = WallFace('-y', y0, Z0 - 0.05, lean)
    for xx in (3.0, 4.3):
        cn_window(p, Fy, xx - 0.4, xx + 0.4, 1.4, 2.5, t=0.35, mull=0)
    wbox(p, 'CnMetal#door', Fx, y1 - 1.2, y1 - 0.4, Z0, 2.2, -0.22, -0.17, bevel=0.012)                        # a porta de aço recuada
    wbox(p, 'Dark', Fx, y1 - 0.82, y1 - 0.78, Z0 + 0.2, 2.0, -0.17, -0.15, bevel=0.0)
    p.box('Hazard', x1 + 0.16, x1 + 0.22, y1 - 1.35, y1 - 0.25, Z0, Z0 + 0.02, bevel=0.0)
    lamp(p, x1 + 0.16, y1 - 0.8, 2.62, '+x')
    # a estrela vermelha no aro dourado, em cima das janelas
    from model_faction_kit import circle_on_face
    circle_on_face(p, 'CnGold', (x1 - 0.05, (wy0 + wy1) / 2, 2.45), '+x', 0.27, 0.06)
    star_wall(p, (x1 + 0.0, (wy0 + wy1) / 2, 2.45), '+x', 0.23, m='Player', t=0.03)
    # o lado do domo (+Y): o corredor coberto até a eclusa
    # o telhado de pagode pequeno (a linguagem da facção), a cumeeira em Y
    cxr, cyr = (x0 + x1) / 2, (y0 + y1) / 2
    cn_roof(p, cxr, cyr, (x1 - x0) / 2 - 0.12, (y1 - y0) / 2 - 0.12, CTRL_H + 0.05, 0.8, 0.42, (0.1, 0.8), curl=0.26,
            kick=0.15, ridge_h=0.16, thick=0.22, pitch=0.5)
    # o corredor (de y1 até a eclusa no cilindro do domo) e a eclusa (o cilindro deitado com a porta redonda)
    cxw = 3.3
    p.box('CnConcrete', cxw - 0.75, cxw + 0.75, y1 - 0.1, -1.35, Z0 - 0.05, 2.5, bevel=0.04)
    p.box('CnPlinth', cxw - 0.85, cxw + 0.85, y1 - 0.1, -1.45, 2.5, 2.7, bevel=0.03)
    p.box('Glass', cxw + 0.75, cxw + 0.79, y1 + 0.2, -2.4, 1.4, 2.1, bevel=0.0)
    p.box('Hazard', cxw + 0.75, cxw + 0.8, y1 + 0.1, -2.2, 0.5, 0.75, bevel=0.0)


def yard(p):
    """Os transformadores: a laje, os 2 transformadores (as aletas do radiador, o tanque de expansão, os 3 isoladores), o
    pórtico de saída da linha com os cabos e a cerca."""
    x0, x1, y0, y1 = YARD
    p.box('CnPlinth', x0, x1, y0, y1, Z0 - 0.05, Z0 + 0.2, bevel=0.03)
    zb = Z0 + 0.2
    for yc in (4.5, 6.55):
        xc = 4.4
        p.box('CnMetal', xc - 0.65, xc + 0.65, yc - 0.6, yc + 0.6, zb, zb + 1.55, bevel=0.04)
        p.box('CnMetal', xc - 0.7, xc + 0.7, yc - 0.65, yc + 0.65, zb + 1.55, zb + 1.65, bevel=0.02)
        for s in (-1, 1):                                     # as aletas do radiador dos dois lados
            for k in range(6):
                yy = yc - 0.5 + k * 0.2
                p.box('Dark', min(xc + s * 0.65, xc + s * 0.95), max(xc + s * 0.65, xc + s * 0.95), yy - 0.03, yy + 0.03, zb + 0.2, zb + 1.4, bevel=0.0)
        p.cyl('CnMetal', (xc - 0.35, yc, zb + 1.95), 'y', 0.22, 1.0, seg=16, bevel=0.0)    # o tanque de expansão
        p.box('Player', xc + 0.65, xc + 0.67, yc - 0.3, yc + 0.3, zb + 0.95, zb + 1.3, bevel=0.0)  # a placa de perigo
        p.box('Yellow', xc + 0.67, xc + 0.68, yc - 0.2, yc + 0.2, zb + 1.0, zb + 1.25, bevel=0.0)
        for k in range(3):                                    # os isoladores (o miolo e os discos de porcelana)
            yy = yc - 0.38 + k * 0.38
            xx = xc + 0.25
            p.cyl('CnMetal#t', (xx, yy, zb + 2.15), 'z', 0.05, 1.0, seg=8, bevel=0.0)
            for j in range(6):
                p.cyl('Paint#ins', (xx, yy, zb + 1.8 + j * 0.13), 'z', 0.14, 0.05, seg=12, bevel=0.0)
            p.cyl('CnMetal#t', (xx, yy, zb + 2.7), 'z', 0.08, 0.1, seg=8, bevel=0.0)
            tube(p, 'Dark', (xx, yy, zb + 2.72), (x1 - 0.3, yy * 0.2 + 5.5 * 0.8, 5.6), 0.02, seg=4)
    # o pórtico de saída (o H de aço na borda da frente) com os isoladores pendurados
    gx = x1 - 0.3
    for y in (y0 + 0.4, y1 - 0.4):
        tube(p, 'CnMetal', (gx, y, zb), (gx, y, 6.0), 0.1, seg=8)
    tube(p, 'CnMetal', (gx, y0 + 0.4, 5.8), (gx, y1 - 0.4, 5.8), 0.09, seg=8)
    for k in range(3):
        yy = 4.75 + k * 0.75
        for j in range(4):
            p.cyl('Paint#ins', (gx, yy, 5.62 - j * 0.1), 'z', 0.11, 0.04, seg=10, bevel=0.0)
    # a cerca (postes e dois arames) e a placa de perigo no portão
    pts = [(x0 + 0.1, y0 + 0.1), (x0 + 0.1, y1 - 0.1), (x1 - 0.1, y1 - 0.1)]
    for (a, b) in zip(pts[:-1], pts[1:]):
        L = math.dist(a, b)
        n = max(1, round(L / 1.4))
        for k in range(n + 1):
            q = (lerp(a[0], b[0], k / n), lerp(a[1], b[1], k / n))
            p.cyl('CnMetal#t', (q[0], q[1], zb + 0.8), 'z', 0.035, 1.6, seg=6, bevel=0.0)
        for zz in (zb + 0.7, zb + 1.5):
            tube(p, 'Dark', (a[0], a[1], zz), (b[0], b[1], zz), 0.015, seg=4)
    p.box('Hazard', x0 + 0.05, x0 + 0.08, 5.0, 5.6, zb + 0.9, zb + 1.3, bevel=0.0)


def base():
    p = BPart(PREFIX + '_Base')
    foundation(p, HX, HY, m='CnPlinth', z=Z0)
    for c in TOWERS:
        cooling_tower(p, c)
        shell_star(p, c, 0.0, 5.3, 1.05)
    reactor(p)
    pipes(p)
    stack(p)
    control(p)
    yard(p)
    # os 2 estandartes: na frente, flanqueando a janela do núcleo
    for s in (-1, 1):
        banner2(p, (5.35, s * 2.75), Z0, 5.6, height=3.0, width=1.0, face='+x')
    return p.finish()


def main():
    clear_scene()
    o = base()
    report(o)
    total = export(o, OUT)
    clear_scene()
    sc = scaffold(PREFIX + '_Scaffold', -HX + 0.3, HX - 0.3, -HY + 0.3, HY - 0.3, T_H, post='CnMetal', plank='Crate', stripe='Hazard')
    total += export(sc, OUT)
    print('MODEL total', total, 'tris')
    print('MODEL lote 12 x 16 m; torres', T_H, 'm (cintura r', T_RW, 'a', round(T_ZB + (T_H - T_ZB) * T_WAIST, 2), 'm); domo', DZS + DH, 'm')


main()
