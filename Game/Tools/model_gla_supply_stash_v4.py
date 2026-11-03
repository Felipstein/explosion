"""O Supply Stash da GLA (o depósito de dinheiro), v4 (03/11/2026): O YAKHCHAL. A direção fechada em docs/elenco/ICONES_03_11.md (5ª
rodada: o ferro-velho do guindaste do v3 caiu por ser objeto de fora do jogo; o pedido é o ícone da família, autêntico, que todo mundo
lembra, sem copiar o ZH). O v1, o v2 e o v3 ficam como estão.

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_gla_supply_stash_v4.py captures/game/models/gla_supply_stash_v4

A palavra (o que lê em 1 segundo): O CONE DE BARRO EM DEGRAUS. A geladeira persa (o yakhchal de Meybod: a cúpula cônica de colmeia que
guardava o gelo do inverno; agora guarda o dinheiro da GLA): o cone gigante (~10 m) de barro com os degraus na casca (as fiadas que
recuam, a quina gasta de cada uma), o anel verde pintado (a cor do jogador) a meia altura, o respiro no topo com a bandeira; ao lado, as
2 torres de vento de Yazd (os badgir: o fuste quadrado, a cabeça com as fendas verticais entre as aletas, as traves de madeira, a faixa
verde) ligadas ao cone pelos dutos baixos. Na frente (+X), a boca do depósito (o iwan com o arco escuro e as caixas lá dentro); do lado
−Y, o toldo listrado sobre as pilhas moderadas de caixas e os tambores; jogada no ombro do cone, de frente pra câmera, a lona grande com o
emblema da GLA (o anel verde, o rosto enrolado no lenço, as 2 lâminas cruzadas) amarrada nos degraus. O pátio de terra batida onde a
picape encosta pela +X (a faixa de Y −3,3..3,3 livre). De cima: o círculo grande com os anéis dos degraus e o anel verde, as 2 torres
quadradas dos lados, a boca na frente.

Lote 16 × 16 m, frente em +X. Alturas: o cone até ~10 m (o respiro até ~10,4 m), os badgir até ~7,9 m, a boca 3,6 m.

Peças (origem no chão, no centro do lote; Unreal X frente, Y direita, Z cima, metros):
  SM_GLA_SupplyStash_Base: tudo (as bandeiras saem no flags.json);
  SM_GLA_SupplyStash_Scaffold: o andaime da obra em madeira.
Materiais (slots da GLA): Plaster (o barro), Player (o verde), Wood, Cloth (a lona, as listras claras), Rust (as listras escuras, o
emblema), Crate (as caixas), Dark, Metal, Earth (o pátio), Trim, Hazard (só no andaime).
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, V, clear_scene, export, flag, scaffold  # noqa: E402,F401
from model_faction_kit import barrel, disc_ring, revolve, crate_stack  # noqa: E402,F401
from model_building_parts import WallFace, wbox, splits, tube, catenary, rect_minus, paint_band, adobe_door  # noqa: E402,F401
from model_building_parts import gla_round_merlons as round_merlons  # noqa: E402
from model_gla_palace_v2 import (new_part, finish, _hash, fbm, rod, prism, earth_pad, blob, crate, flagpole, arch_fill,  # noqa: E402,F401
                                 arch_band, rope, tire, scrawl)
from model_gla_barracks_v4 import awning  # noqa: E402

PREFIX = 'SM_GLA_SupplyStash'
CX, CY = -2.6, 0.0               # o eixo do cone
RB, H = 4.15, 10.0               # o raio no pé e a altura do cone
COURSE = 0.62                    # a altura de cada fiada (degrau)
MOUTH = (-0.9, 2.75, 1.85, 3.55)  # a boca: x0, x1, meia-largura, topo
ARCH_W, ARCH_SPRING = 2.3, 1.95
BADGIR = [(-3.7, -5.55), (-3.7, 5.55)]
BG_HW, BG_H0, BG_H1 = 0.95, 5.3, 7.35
TARP = dict(theta=-30.0, span=84.0, z0=4.2, z1=7.6)


def r_cone(z):
    """O perfil da colmeia: convexo (mais cheio que o cone reto), fechando no respiro."""
    t = max(0.0, min(1.0, z / H))
    return RB * (1.0 - t) ** 0.78 + 0.32 * t


def lumpy(a, k, s=0):
    return 0.014 * (fbm(math.cos(a) * 2.0 + k * 0.7, math.sin(a) * 2.0 + k * 0.3, s) - 0.5) * 2


# ===================================================================================== o cone
def cone(p):
    """O cone em fiadas: cada degrau sobe COURSE (o espelho levemente inclinado) e recua até o raio da fiada de cima (o piso do degrau);
    o chanfro largo do material arredonda as quinas (a quina gasta do barro). O anel verde pintado num espelho a meia altura; o respiro no
    topo (o tamborzinho com a tampa e a boca escura)."""
    seg = 64
    n = int((H - 0.6) / COURSE)
    rings = []

    def ring(r, z, k):
        return [(CX + r * (1 + lumpy(2 * math.pi * i / seg, k)) * math.cos(2 * math.pi * i / seg),
                 CY + r * (1 + lumpy(2 * math.pi * i / seg, k)) * math.sin(2 * math.pi * i / seg), z) for i in range(seg)]
    z = 0.0
    rings.append(ring(r_cone(0.0) + 0.12, -0.08, 0))
    for k in range(n):
        r0 = r_cone(z)
        r1 = r_cone(z + COURSE)
        rings.append(ring(r0 - 0.035, z + COURSE, k))          # o espelho (sobe quase a prumo)
        z += COURSE
        rings.append(ring(r1, z, k + 1))                       # o piso do degrau (recua até a fiada de cima)
    zt = z
    rt = r_cone(zt)
    rings.append(ring(rt * 0.86, zt + 0.25, n + 1))
    rings.append(ring(0.42, zt + 0.32, n + 1))
    rings.append(ring(0.42, zt + 0.7, n + 1))
    rings.append(ring(0.52, zt + 0.72, n + 1))
    rings.append(ring(0.52, zt + 0.84, n + 1))
    rings.append(ring(0.3, zt + 0.86, n + 1))
    rings.append(ring(0.28, zt + 0.7, n + 1))
    p.loft('Plaster#cone', rings, bevel=0.0, smooth=True, caps=(True, True))
    p.cyl('Dark', (CX, CY, zt + 0.72), 'z', 0.27, 0.04, seg=20, bevel=0.0)
    # o anel verde pintado (cobre um espelho inteiro, a borda de baixo ondulando)
    kb = 7
    za = kb * COURSE + 0.06
    zb = (kb + 1) * COURSE - 0.08
    rr = []
    for (zz, d) in ((za, 0.0), (zb, 0.0), (zb, -0.05), (za, -0.05)):
        q = []
        for i in range(seg):
            a = 2 * math.pi * i / seg
            wob = 0.04 * math.sin(a * 9 + 0.7) if zz == za else 0.0
            t = (zz + wob - kb * COURSE) / COURSE
            r = (r_cone(kb * COURSE) - 0.035 * t) * (1 + lumpy(a, kb)) + 0.02 + d
            q.append((CX + r * math.cos(a), CY + r * math.sin(a), zz + wob))
        rr.append(q)
    rr.append(rr[0])
    p.loft('Player#band', rr, bevel=0.0, smooth=True, caps=(False, False))
    # a bandeira no respiro
    flagpole(p, CX + 0.2, CY + 0.2, zt + 0.84, 2.2, length=1.3, height=0.85, phase=0.4)
    return zt + 0.86


# ===================================================================================== a boca do depósito
def mouth(p):
    """A boca (o iwan): o bloco de barro saindo do cone com o arco fundo, o escuro de dentro com as caixas, a moldura, a faixa verde, as
    ameias redondas, a lanterna; a soleira de tábuas pra picape."""
    x0, x1, hw, top = MOUTH
    F = WallFace('+x', x1, 0.0)
    depth = x1 - x0
    for (a, b, c, d, lo, hi) in rect_minus(-hw, hw, 0.0, top, [(-ARCH_W / 2, ARCH_W / 2, 0.0, ARCH_SPRING + ARCH_W / 2)]):
        wbox(p, 'Plaster', F, a, b, c, d, -depth, 0.0)
    p.box('Plaster', x0, x1, -hw, hw, top - 0.4, top, bevel=0.13)
    # o arco fundo até o fundo escuro (a porta do corredor que entra no cone: o escuro tapa a casca do cone que passa ali atrás)
    xd = 1.62
    arch_fill(p, 'Plaster#arch', ((x1 + xd) / 2, 0.0), 'x', ARCH_W, ARCH_SPRING, ARCH_SPRING + ARCH_W / 2 + 0.01, x1 - xd)
    arch_band(p, 'Plaster#s', (x1, 0.0), 'x', ARCH_W, ARCH_SPRING, 0.005, width=0.2, t=0.07, seg=18)
    p.box('Dark', xd - 0.08, xd, -ARCH_W / 2, ARCH_W / 2, 0.0, ARCH_SPRING + ARCH_W / 2, bevel=0.0)
    # as 2 folhas do portão de tábuas, abertas contra as paredes do arco
    for s in (-1, 1):
        y = s * (ARCH_W / 2 - 0.12)
        for k in range(4):
            xa = xd + 0.05 + k * 0.24
            p.box('Wood#door', xa, xa + 0.22, y - 0.04, y + 0.04, 0.04, ARCH_SPRING - 0.05, bevel=0.012)
        for zz in (0.4, ARCH_SPRING - 0.45):
            p.box('Dark#gate', xd + 0.05, xd + 1.0, y - 0.07 * s - 0.02, y - 0.07 * s + 0.02, zz - 0.07, zz + 0.07, bevel=0.0)
    p.box('Wood#sill', x1 - 0.2, x1 + 0.35, -ARCH_W / 2 - 0.1, ARCH_W / 2 + 0.1, -0.02, 0.1, bevel=0.03)
    # a moldura (o alfiz) e a faixa verde por cima do arco, as ameias e a lanterna
    for (a, b, c, d) in ((-hw + 0.15, -hw + 0.33, 0.25, top - 0.55), (hw - 0.33, hw - 0.15, 0.25, top - 0.55),
                         (-hw + 0.15, hw - 0.15, top - 0.72, top - 0.55)):
        wbox(p, 'Plaster#s', F, a, b, c, d, -0.02, 0.06, bevel=0.0)
    paint_band(p, (x1, -hw), (x1, hw), top - 0.5, top - 0.15, side=-1, off=0.0, m='Player', seed=3)
    for s in (-1, 1):
        paint_band(p, (x0 + 1.0, s * hw), (x1, s * hw), top - 0.5, top - 0.15, side=s, off=0.0, m='Player', seed=4 + s)
    p.box('Plaster#lo', x0 + 0.8, x1 + 0.1, -hw - 0.1, hw + 0.1, top - 0.04, top + 0.1, bevel=0.0)
    round_merlons(p, (x1 - 0.15, -hw + 0.2), (x1 - 0.15, hw - 0.2), top + 0.1, n=4, w=0.46, t=0.32, h=0.38)
    for s in (-1, 1):
        round_merlons(p, (x0 + 1.1, s * (hw - 0.16)), (x1 - 0.4, s * (hw - 0.16)), top + 0.1, n=2, w=0.46, t=0.32, h=0.38)
    q = (x1 - 0.5, 0.0, ARCH_SPRING + ARCH_W / 2 - 0.05)
    rod(p, 'Dark', q, (q[0], q[1], q[2] - 0.45), 0.008, seg=4)
    p.cyl('Metal#lamp', (q[0], q[1], q[2] - 0.58), 'z', 0.12, 0.24, seg=10, r2=0.07, bevel=0.0)
    p.cyl('Light', (q[0], q[1], q[2] - 0.76), 'z', 0.08, 0.12, seg=10, bevel=0.0, smooth=True)
    # as vigas de madeira saindo da parede da boca (o adobe de verdade)
    for y in splits(-hw + 0.35, hw - 0.35, 0.62):
        if abs(y) < ARCH_W / 2 + 0.1:
            continue
        rod(p, 'Wood#beam', (x1 - 0.3, y, top - 0.85), (x1 + 0.32, y, top - 0.85), 0.075, seg=7)


# ===================================================================================== as torres de vento
def badgir(p, cx, cy, seed=0):
    """A torre de vento de Yazd: o rodapé, o fuste quadrado levemente afunilado com as traves de madeira atravessando, a faixa verde, a
    cornija, a cabeça com as fendas verticais (o escuro) entre as aletas de barro em cada face, as traves (kalak) cruzando as fendas, o
    chapéu com o beiral e o parapeito baixo; o duto baixo de abóbada até o cone."""
    hw = BG_HW
    p.box('Plaster#plinth', cx - hw - 0.18, cx + hw + 0.18, cy - hw - 0.18, cy + hw + 0.18, -0.05, 0.4, bevel=0.1)
    p.loft('Plaster#shaft', [[(cx - hw - 0.07, cy - hw - 0.07, 0.3), (cx + hw + 0.07, cy - hw - 0.07, 0.3), (cx + hw + 0.07, cy + hw + 0.07, 0.3),
                              (cx - hw - 0.07, cy + hw + 0.07, 0.3)],
                             [(cx - hw, cy - hw, BG_H0), (cx + hw, cy - hw, BG_H0), (cx + hw, cy + hw, BG_H0), (cx - hw, cy + hw, BG_H0)]], bevel=0.13)
    for z in (2.1, 3.7):
        for (dx, dy, ax) in ((0, -1, 'y'), (0, 1, 'y'), (-1, 0, 'x'), (1, 0, 'x')):
            for o in (-0.45, 0.45):
                if ax == 'y':
                    rod(p, 'Wood#beam', (cx + o, cy + dy * (hw - 0.1), z), (cx + o, cy + dy * (hw + 0.35), z), 0.07, seg=7)
                else:
                    rod(p, 'Wood#beam', (cx + dx * (hw - 0.1), cy + o, z + 0.1), (cx + dx * (hw + 0.35), cy + o, z + 0.1), 0.07, seg=7)
    # a faixa verde e a cornija sob a cabeça
    for f in ('+x', '-x', '+y', '-y'):
        s = 1 if f[0] == '+' else -1
        if f[1] == 'x':
            paint_band(p, (cx + s * hw, cy - hw), (cx + s * hw, cy + hw), BG_H0 - 0.75, BG_H0 - 0.38, side=-s, off=0.0, m='Player', seed=seed + s)
        else:
            paint_band(p, (cx - hw, cy + s * hw), (cx + hw, cy + s * hw), BG_H0 - 0.75, BG_H0 - 0.38, side=s, off=0.0, m='Player', seed=seed + 3 + s)
    p.box('Plaster#lo', cx - hw - 0.14, cx + hw + 0.14, cy - hw - 0.14, cy + hw + 0.14, BG_H0 - 0.05, BG_H0 + 0.14, bevel=0.0)
    # a cabeça: o miolo escuro, os pilares de canto, as aletas, as traves
    p.box('Dark', cx - hw + 0.3, cx + hw - 0.3, cy - hw + 0.3, cy + hw - 0.3, BG_H0 + 0.1, BG_H1, bevel=0.0)
    for (sx, sy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        x, y = cx + sx * (hw - 0.15), cy + sy * (hw - 0.15)
        p.box('Plaster#pier', x - 0.15, x + 0.15, y - 0.15, y + 0.15, BG_H0 + 0.1, BG_H1, bevel=0.06)
    inner = hw - 0.3
    for f in ('+x', '-x', '+y', '-y'):
        s = 1 if f[0] == '+' else -1
        for k in range(3):
            u = -inner + (k + 1) * 2 * inner / 4
            if f[1] == 'x':
                p.box('Plaster#fin', cx + s * hw - 0.32 if s > 0 else cx - hw, cx + hw if s > 0 else cx - hw + 0.32, cy + u - 0.07, cy + u + 0.07,
                      BG_H0 + 0.1, BG_H1, bevel=0.03)
            else:
                p.box('Plaster#fin', cx + u - 0.07, cx + u + 0.07, cy + s * hw - 0.32 if s > 0 else cy - hw, cy + hw if s > 0 else cy - hw + 0.32,
                      BG_H0 + 0.1, BG_H1, bevel=0.03)
    for z in (BG_H0 + 0.75, BG_H0 + 1.45):
        for s in (-1, 1):
            rod(p, 'Wood#kalak', (cx - hw - 0.12, cy + s * (hw - 0.12), z), (cx + hw + 0.12, cy + s * (hw - 0.12), z), 0.05, seg=6)
            rod(p, 'Wood#kalak', (cx + s * (hw - 0.12), cy - hw - 0.12, z + 0.06), (cx + s * (hw - 0.12), cy + hw + 0.12, z + 0.06), 0.05, seg=6)
    # o chapéu: o beiral, a laje e o parapeito baixo de topo redondo
    p.box('Plaster#cap', cx - hw - 0.2, cx + hw + 0.2, cy - hw - 0.2, cy + hw + 0.2, BG_H1, BG_H1 + 0.22, bevel=0.08)
    p.box('Plaster#cap', cx - hw - 0.08, cx + hw + 0.08, cy - hw - 0.08, cy + hw + 0.08, BG_H1 + 0.22, BG_H1 + 0.42, bevel=0.1)
    # o duto baixo de abóbada até o cone
    d = Vector((CX - cx, CY - cy, 0.0))
    L = d.length - r_cone(1.0) + 0.6
    d.normalize()
    a0 = Vector((cx, cy, 0.0)) + d * (hw - 0.1)
    a1 = a0 + d * L
    yaw = math.degrees(math.atan2(d.y, d.x))
    mid = (a0 + a1) / 2
    p.obox('Plaster#duct', (mid.x, mid.y, 0.95), (L, 1.2, 1.9), yaw=yaw, bevel=0.1)
    p.cyl('Plaster#duct', (mid.x, mid.y, 1.9), (d.x, d.y, 0.0), 0.6, L, seg=16, bevel=0.0, smooth=True)
    return BG_H1 + 0.42


# ===================================================================================== a lona com o emblema
def tarp_S(u, v):
    """A lona jogada no ombro do cone: u ∈ [0, 1] em volta (o ângulo), v ∈ [0, 1] de cima pra baixo; deita na linha das quinas dos
    degraus, com as dobras rasas que descem e a barriga entre as cordas."""
    th = math.radians(TARP['theta'] + (u - 0.5) * TARP['span'])
    z = TARP['z1'] - (TARP['z1'] - TARP['z0']) * v
    # o pano apoiado nas quinas dos degraus (a quina k fica em z = (k + 1)·COURSE, no raio da fiada k) e caindo um pouco entre elas
    q = z / COURSE - 1.0
    k = max(0, int(math.floor(q)))
    f = q - k
    rn0 = r_cone(k * COURSE) - 0.035
    rn1 = r_cone((k + 1) * COURSE) - 0.035
    r = rn0 + (rn1 - rn0) * f + 0.06 - 0.07 * math.sin(math.pi * f)
    # as dobras que descem dos cantos amarrados e a barriga no meio
    r += 0.045 * math.sin(u * 19.0 + 0.6) * (0.3 + 0.7 * v) * math.sin(math.pi * u) + 0.04 * math.sin(math.pi * u) * math.sin(math.pi * v)
    return Vector((CX + r * math.cos(th), CY + r * math.sin(th), z))


def tarp_N(u, v):
    e = 1e-3
    du = tarp_S(u + e, v) - tarp_S(u - e, v)
    dv = tarp_S(u, v + e) - tarp_S(u, v - e)
    n = dv.cross(du).normalized()
    c = tarp_S(u, v) - Vector((CX, CY, tarp_S(u, v).z))
    return n if n.dot(c) > 0 else -n


def tarp_uv(x, y):
    """O ponto (x pra direita de quem olha, y pra cima, em metros, a partir do centro da lona) em (u, v)."""
    zc = (TARP['z0'] + TARP['z1']) / 2
    rc = r_cone(zc - COURSE) + 0.07
    span_m = math.radians(TARP['span']) * rc
    return 0.5 + x / span_m, 0.5 - y / (TARP['z1'] - TARP['z0']) * 0.92


def on_tarp(p, m, poly, lift, t, rsub=7):
    """O desenho do emblema (o polígono 2D em metros, em leque a partir do centro) em relevo fino sobre a lona: o tampo é uma grade em
    leque (os raios do centro até cada vértice, rsub pontos em cada), então ele acompanha a curva do cone e as dobras do pano (um
    polígono grande e plano cortava o pano)."""
    b = p.bm(m, 0.0)
    cx = sum(x for (x, _) in poly) / len(poly)
    cy = sum(y for (_, y) in poly) / len(poly)

    def vert(x, y, d):
        u, v = tarp_uv(x, y)
        q, n = tarp_S(u, v), tarp_N(u, v)
        return b.verts.new(V(*(q + n * (0.02 + lift + d))))
    k = len(poly)
    for d, flip in ((t, False), (0.0, True)):
        c = vert(cx, cy, d)
        spokes = [[vert(cx + (x - cx) * j / rsub, cy + (y - cy) * j / rsub, d) for j in range(1, rsub + 1)] for (x, y) in poly]
        for i in range(k):
            A, B = spokes[i], spokes[(i + 1) % k]
            f = [c, A[0], B[0]]
            b.faces.new(f[::-1] if flip else f)
            for j in range(rsub - 1):
                f = [A[j], A[j + 1], B[j + 1], B[j]]
                b.faces.new(f[::-1] if flip else f)
        if not flip:
            rim_top = [s_[-1] for s_ in spokes]
        else:
            rim_bot = [s_[-1] for s_ in spokes]
    for i in range(k):
        j = (i + 1) % k
        b.faces.new([rim_top[i], rim_bot[i], rim_bot[j], rim_top[j]])


def on_tarp_strip(p, m, pts, w, lift, t):
    """A tira em relevo pela linha de centro (a lâmina curva do emblema): os 2 lados a ±w/2 em cada ponto, colados no pano."""
    b = p.bm(m, 0.0)
    rows = []
    n = len(pts)
    for i, (x, y) in enumerate(pts):
        x0, y0 = pts[max(0, i - 1)]
        x1, y1 = pts[min(n - 1, i + 1)]
        dx, dy = x1 - x0, y1 - y0
        L = math.hypot(dx, dy) or 1.0
        nx, ny = -dy / L * w / 2, dx / L * w / 2
        row = []
        for (a, d) in ((-1, 0.0), (1, 0.0), (1, t), (-1, t)):
            u, v = tarp_uv(x + a * nx, y + a * ny)
            q, nn = tarp_S(u, v), tarp_N(u, v)
            row.append(tuple(q + nn * (0.02 + lift + d)))
        rows.append(row)
    p.loft(m, rows, bevel=0.0)


def on_tarp_ring(p, m, r0, r1, lift, t, cx=0.0, cy=0.0, seg=40):
    b = p.bm(m, 0.0)
    rows = []
    for (r, d) in ((r0, 0.0), (r1, 0.0), (r1, t), (r0, t)):
        row = []
        for i in range(seg):
            a = 2 * math.pi * i / seg
            u, v = tarp_uv(cx + r * math.cos(a), cy + r * math.sin(a))
            q, n = tarp_S(u, v), tarp_N(u, v)
            row.append(b.verts.new(V(*(q + n * (0.02 + lift + d)))))
        rows.append(row)
    for k in range(4):
        A, B = rows[k], rows[(k + 1) % 4]
        for i in range(seg):
            j = (i + 1) % seg
            b.faces.new([A[i], A[j], B[j], B[i]])


def circle(r, n=32, cx=0.0, cy=0.0, sx=1.0, sy=1.0):
    return [(cx + r * sx * math.cos(2 * math.pi * k / n), cy + r * sy * math.sin(2 * math.pi * k / n)) for k in range(n)]


def emblem_tarp(p):
    """A lona grande (o pano com espessura, as bordas fechadas) e o emblema da GLA pintado: o disco escuro, o anel verde (a cor do
    jogador), o campo de bronze, o rosto enrolado no lenço (a cabeça, o turbante em faixas, a fresta dos olhos) e as 2 lâminas cruzadas
    embaixo; as cordas dos cantos até os degraus."""
    nu, nv = 24, 30
    b = p.bm('Cloth#tarp', 0.0)
    top = [[b.verts.new(V(*(tarp_S(i / nu, j / nv) + tarp_N(i / nu, j / nv) * 0.02))) for i in range(nu + 1)] for j in range(nv + 1)]
    bot = [[b.verts.new(V(*(tarp_S(i / nu, j / nv) - tarp_N(i / nu, j / nv) * 0.02))) for i in range(nu + 1)] for j in range(nv + 1)]
    for j in range(nv):
        for i in range(nu):
            b.faces.new([top[j][i], top[j][i + 1], top[j + 1][i + 1], top[j + 1][i]]).smooth = True
            b.faces.new([bot[j + 1][i], bot[j + 1][i + 1], bot[j][i + 1], bot[j][i]]).smooth = True
    edge = [(0, i) for i in range(nu)] + [(j, nu) for j in range(nv)] + [(nv, i) for i in range(nu, 0, -1)] + [(j, 0) for j in range(nv, 0, -1)]
    for (j0, i0), (j1, i1) in zip(edge, edge[1:] + edge[:1]):
        b.faces.new([top[j0][i0], bot[j0][i0], bot[j1][i1], top[j1][i1]])
    # o emblema
    R = 1.22
    cy0 = 0.05
    on_tarp(p, 'Dark#emb', circle(R, 40, 0.0, cy0), 0.0, 0.012)
    on_tarp_ring(p, 'Player#emb', R * 0.72, R * 0.94, 0.012, 0.012, cy=cy0)
    on_tarp(p, 'Rust#emb', circle(R * 0.66, 36, 0.0, cy0), 0.012, 0.01)
    # o rosto enrolado: a cabeça (o turbante largo em cima, o queixo), os ombros, a fresta clara dos olhos, as faixas do turbante
    head = [(0.0, 0.62), (0.3, 0.56), (0.42, 0.38), (0.4, 0.12), (0.3, -0.12), (0.18, -0.26), (0.0, -0.3), (-0.18, -0.26), (-0.3, -0.12),
            (-0.4, 0.12), (-0.42, 0.38), (-0.3, 0.56)]
    on_tarp(p, 'Dark#emb', [(x, y + cy0) for (x, y) in head], 0.022, 0.01)
    shoulders = [(-0.7, -0.62), (-0.55, -0.3), (-0.2, -0.2), (0.2, -0.2), (0.55, -0.3), (0.7, -0.62), (0.4, -0.78), (-0.4, -0.78)]
    on_tarp(p, 'Dark#emb', [(x, y + cy0) for (x, y) in shoulders], 0.022, 0.01)
    on_tarp(p, 'Cloth#emb', [(-0.34, 0.22 + cy0), (0.34, 0.22 + cy0), (0.3, 0.1 + cy0), (-0.3, 0.1 + cy0)], 0.032, 0.008)
    for (y0, y1, tilt) in ((0.36, 0.44, 0.06), (0.5, 0.56, -0.05)):
        on_tarp(p, 'Rust#emb', [(-0.38, y0 + tilt + cy0), (0.38, y0 - tilt + cy0), (0.36, y1 - tilt + cy0), (-0.36, y1 + tilt + cy0)], 0.032, 0.008)
    # as 2 lâminas cruzadas embaixo (as curvas de adaga), por cima do anel
    for s in (-1, 1):
        pts = []
        for k in range(9):
            t = k / 8
            x = s * (-0.42 + 0.84 * t)
            y = -R * 0.84 + 0.36 * t + 0.1 * math.sin(math.pi * t)
            pts.append((x, y + cy0))
        on_tarp_strip(p, 'Cloth#emb', pts, 0.075, 0.034 + (0.004 if s > 0 else 0.0), 0.008)
    # as cordas dos 4 cantos até as quinas dos degraus
    for (u, v) in ((0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 1.0)):
        q = tarp_S(u, v)
        z_to = max(0.6, q.z - 1.3 if v > 0.5 else q.z - 0.6)
        th = math.radians(TARP['theta'] + (u - 0.5) * TARP['span'] * 1.25)
        r = r_cone(z_to - COURSE * 0.5) + 0.05
        g = (CX + r * math.cos(th), CY + r * math.sin(th), z_to)
        rope(p, tuple(q), g, sag=0.05, r=0.02, m='Dark#rope')


# ===================================================================================== o pátio
def yard(p):
    """O pátio de terra batida (a picape encosta na boca pela +X: a faixa Y −3,3..3,3 fica livre), o toldo listrado do lado −Y com as
    pilhas moderadas de caixas e os tambores, o canto +Y com os tambores e o pneu, o rabisco verde."""
    earth_pad(p, (2.6, 0.0), blob(2.6, 0.0, 5.3, 7.6, seed=8, amp=0.08), h=0.05)
    p.cyl('Plaster#foot', (CX, CY, 0.15), 'z', RB + 0.3, 0.36, seg=64, bevel=0.0, smooth=True)
    awning(p, 0.7, 4.5, -7.1, -3.6, 3.05, 2.35, n=7, sag=0.12)
    for y in (-3.68, -7.02):
        rod(p, 'Wood#aw', (0.72, y, 0.0), (0.72, y, 3.08), 0.05)
    # as caixas sob o toldo (3 pilhas moderadas) e os tambores
    crate_stack(p, 1.5, -4.6, 0.0, n=3, yaw=8, size=(1.2, 0.75, 0.5))
    crate_stack(p, 2.75, -6.0, 0.0, n=2, yaw=-12, size=(1.2, 0.75, 0.5))
    crate_stack(p, 3.6, -4.55, 0.0, n=2, yaw=84, size=(1.0, 0.7, 0.45))
    crate(p, 1.6, -6.3, 0.0, size=(0.8, 0.55, 0.45), yaw=30)
    for (x, y, m) in ((4.1, -6.4, 'Rust'), (4.0, -5.7, 'Metal')):
        barrel(p, x, y, 0.0, r=0.29, h=0.86, m=m)
    # o canto +Y: os tambores e o pneu, um caixote aberto
    for (x, y, m) in ((1.6, 4.4, 'Rust'), (2.15, 4.75, 'Rust'), (1.7, 5.05, 'Metal')):
        barrel(p, x, y, 0.0, r=0.29, h=0.86, m=m)
    tire(p, (3.2, 4.6, 0.13), R=0.36, r=0.13)
    crate(p, 2.7, 6.0, 0.0, size=(1.1, 0.7, 0.5), yaw=-20)
    scrawl(p, (MOUTH[1] + 0.01, -1.55, 1.2), '+x', size=0.55, seed=9)


def base():
    p = new_part(PREFIX + '_Base')
    for m in ('Player#band', 'Cloth#tarp', 'Cloth#aw', 'Rust#aw', 'Dark#rope', 'Dark#emb', 'Player#emb', 'Rust#emb', 'Cloth#emb'):
        p.bm(m, 0.0)
    p.bm('Plaster#cone', 0.13)
    p.bm('Plaster#shaft', 0.13)
    top = cone(p)
    mouth(p)
    tops = [badgir(p, x, y, seed=k) for k, (x, y) in enumerate(BADGIR)]
    for k, (x, y) in enumerate(BADGIR):
        flagpole(p, x + 0.5, y + (0.5 if y > 0 else -0.5), BG_H1 + 0.42, 1.9, length=1.2, height=0.75, phase=1.1 + k)
    emblem_tarp(p)
    yard(p)
    return finish(p), top, max(tops)


def write_preview(out):
    import json
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1300, 1300]], ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1500, 1100]],
                      ['side_left', 'ortho', [0, -1, 0], [0, 0, 1], [1500, 1100]],
                      ['three_quarter', 'persp', 30.0, 215.0, [1400, 1300], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 30.0, 35.0, [1400, 1300], 30.0, 0.95]],
            'usa_at': [7.5, -12.0, 0], 'tank_views': True,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(out, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, indent=1)


def main():
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'gla_supply_stash_v4')
    os.makedirs(out, exist_ok=True)
    clear_scene()
    b, top, btop = base()
    total = export(b, out)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', CX - RB - 0.2, MOUTH[1] + 0.2, -6.7, 6.7, 7.0, post='Wood', plank='Crate', stripe='Hazard'), out)
    write_preview(out)
    print('MODEL total', total, 'tris')
    print('MODEL lote 16 x 16 m; o cone ate', round(top, 2), 'm; os badgir ate', round(btop, 2), 'm; a boca', MOUTH[3], 'm')
    print('MODEL a picape encosta na boca: (', MOUTH[1] + 0.4, ', 0 ); a faixa Y -3,3..3,3 livre ate a borda +X')
    print('MODEL fire_points_m', [(CX + 1.2, -2.6, 4.2), (CX + 1.2, 2.6, 4.2), (BADGIR[0][0], BADGIR[0][1], BG_H0)])


if __name__ == '__main__':
    main()
