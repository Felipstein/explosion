"""O SAM Site da GLA (prédio novo, só antiaéreo), 03/11/2026: OS MÍSSEIS DE CAÇA ROUBADOS. Fiel ao conceito aprovado
docs/concept/buildings/gla_sam_site_misseis_roubados_APROVADO.jpg.

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_gla_sam_site.py captures/game/models/gla_sam_site

A palavra (o que lê em 1 segundo): OS 2 MÍSSEIS LONGOS E CINZA DEITADOS SOBRE UM TANQUE MORTO NA AREIA. A carcaça de um tanque soviético
(o casco do T-55: o glacis, os para-lamas, a grelha do motor, a lagarta e as rodas aparecendo onde a areia foi cavada, o rombo escuro na
frente) meio enterrada no monte de areia; no anel da torre que não existe mais, o anel em degraus que gira, a coluna, o berço soldado e os
2 trilhos com os mísseis ar-ar roubados (cinza, a ogiva longa, as aletas em X na cauda, as letras em cirílico "Р-27" e a faixa verde a
pincel antes da ogiva); atrás, o banco do operador (a poltrona de couro velha no estrado) com o painel e o guarda-sol verde de franja.
No casco, o mastro com o cabo enrolado, a bola do sensor e a lâmpada vermelha; em volta, os sacos de areia e os caixotes compridos.

Lote 10 × 10 m, frente em +X. Alturas: o convés do casco ~0,85 m (a areia até ~0,35 m nos lados), o eixo de elevação 1,83 m, os mísseis
~2,1–2,6 m, o guarda-sol até 3,3 m, o mastro até ~3,95 m.

Peças (origem no chão, no centro do lote; Unreal X frente, Y direita, Z cima, metros):
  SM_GLA_SamSite_Base: o monte de areia, a carcaça, o mastro, os sacos, os caixotes;
  SM_GLA_SamSite_Turret (gira em volta de (0, 0), a mesma origem do modelo; mísseis pra +X em repouso): o anel em degraus, a coluna, o
    berço, os trilhos, os 2 mísseis, o estrado com o banco, o painel e o guarda-sol;
  SM_GLA_SamSite_Scaffold: o andaime da obra em madeira.
Materiais (slots da GLA e genéricos): Earth (o monte), Crate (a tinta oliva do tanque, os caixotes), Rust (a ferrugem, o couro do banco),
  Metal (os mísseis, o anel, os trilhos), Dark, Trim, Player (o verde: a faixa dos mísseis, o guarda-sol, o trapo), Cloth (os sacos),
  Glass (as lentes), Beacon (a lâmpada que pisca), Hazard (só no andaime).
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, V, clear_scene, export, flag, scaffold  # noqa: E402,F401
from model_faction_kit import revolve  # noqa: E402
from model_building_parts import tube  # noqa: E402
from model_gla_palace_v2 import (new_part, finish, _hash, fbm, rod, prism, bag, crate, Frame, rag, smooth, surface)  # noqa: E402,F401

COLORS.update({'Glass': (30, 34, 38), 'Beacon': (255, 70, 40)})
PREFIX = 'SM_GLA_SamSite'
# a carcaça (o quadro do casco: x pra frente, y pra direita, z do convés; a origem no centro do anel da torre)
HULL_YAW, HULL_PITCH, HULL_ROLL = 45.0, -2.0, 3.0   # o casco atravessado como no conceito (de cima, na diagonal)
DECK = 0.85
HX0, HX1 = -3.75, 2.5            # a traseira e o nariz
HW, FW = 1.45, 1.72              # a meia-largura do casco e por cima dos para-lamas
PROF = [(-3.75, -1.55), (2.0, -1.55), (2.5, -0.62), (1.3, 0.0), (-3.5, 0.0), (-3.75, -0.28)]   # o perfil do lado (x, z)
SAND = 0.34                      # a altura da areia encostada no casco
# a torre que gira
EL = 9.0                         # a elevação dos mísseis em repouso
TZ = DECK + 0.98                 # o eixo de elevação (os munhões)
MR, ML, MY = 0.19, 4.1, 0.6      # o raio e o comprimento do míssil, a meia-distância entre os 2
SEAT = (-0.95, -1.55)            # o banco do operador
MAST_L = (-3.2, -1.3)            # o mastro no quadro do casco


def hull_frame(p):
    return Frame(p, (0.0, 0.0, DECK), yaw=HULL_YAW, pitch=HULL_PITCH, roll=HULL_ROLL)


def to_local(x, y):
    c, s = math.cos(math.radians(HULL_YAW)), math.sin(math.radians(HULL_YAW))
    return (x * c + y * s, -x * s + y * c)


def to_world(lx, ly):
    c, s = math.cos(math.radians(HULL_YAW)), math.sin(math.radians(HULL_YAW))
    return (lx * c - ly * s, lx * s + ly * c)


# ===================================================================================== o monte de areia
def mz(x, y):
    """A altura do monte no ponto (x, y) do mundo: a areia encostada no casco (SAND no pé, o perfil côncavo de repouso que morre a ~2,2 m),
    o ruído baixo, a cova do lado da câmera (as rodas aparecendo) e a da frente (a lagarta), e a borda do lote."""
    lx, ly = to_local(x, y)
    cx, hx = (HX0 + HX1) / 2, (HX1 - HX0) / 2 + 0.05
    dx, dy = abs(lx - cx) - hx, abs(ly) - FW
    d = math.hypot(max(dx, 0.0), max(dy, 0.0)) + min(max(dx, dy), 0.0)
    h = SAND if d <= 0 else SAND * max(0.0, 1.0 - d / 2.3) ** 1.6
    h += 0.07 * (fbm(x * 0.7 + 3.1, y * 0.7 + 1.7, 5) - 0.5) * min(1.0, h / 0.08)
    # a cova do lado −y (a câmera do jogo vê): as 2 rodas de apoio meio de fora
    if ly < 0:
        h -= 0.42 * (1.0 - smooth(0.7, 1.5, abs(lx + 1.3))) * (1.0 - smooth(0.6, 1.9, -ly - FW))
    # a cova da frente (a lagarta em volta da roda-guia e o rombo)
    h -= 0.4 * (1.0 - smooth(0.4, 1.4, math.hypot(lx - 2.6, ly + 1.35)))
    h *= 1.0 - smooth(3.9, 4.8, math.hypot(x, y) * 0.55 + max(abs(x), abs(y)) * 0.45)
    return max(-0.03, h)


def sdf(x, y):
    lx, ly = to_local(x, y)
    cx, hx = (HX0 + HX1) / 2, (HX1 - HX0) / 2 + 0.05
    dx, dy = abs(lx - cx) - hx, abs(ly) - FW
    return math.hypot(max(dx, 0.0), max(dy, 0.0)) + min(max(dx, dy), 0.0)


def mound(p):
    """O monte (só o tampo) em anéis em volta do casco: a borda acompanha a forma (o retângulo redondo a ~2,3 m do casco, cortado na
    beira do lote), nada de grade quadrada; o miolo debaixo do casco fica aberto (o casco tapa)."""
    b = p.bm('Earth#mound', 0.0)
    c = to_world((HX0 + HX1) / 2, 0.0)
    N, R = 88, 18
    edge = []
    for k in range(N):
        a = 2 * math.pi * k / N
        r = 0.0
        while r < 7.5:
            x, y = c[0] + r * math.cos(a), c[1] + r * math.sin(a)
            if sdf(x, y) > 2.3 or math.hypot(x, y) * 0.55 + max(abs(x), abs(y)) * 0.45 > 4.8:
                break
            r += 0.04
        edge.append(r * (1.0 + 0.08 * (fbm(math.cos(a) * 2.6, math.sin(a) * 2.6, 9) - 0.5)))
    rings = []
    for j in range(R + 1):
        t = 0.25 + 0.75 * j / R
        ring = []
        for k in range(N):
            a = 2 * math.pi * k / N
            x, y = c[0] + edge[k] * t * math.cos(a), c[1] + edge[k] * t * math.sin(a)
            ring.append((b.verts.new(V(x, y, -0.03 if j == R else mz(x, y))), sdf(x, y)))
        rings.append(ring)
    for j in range(R):
        for k in range(N):
            k2 = (k + 1) % N
            q = [rings[j][k], rings[j + 1][k], rings[j + 1][k2], rings[j][k2]]
            if all(d < -0.35 for (_, d) in q):
                continue
            f = b.faces.new([v for (v, _) in q])
            f.smooth = True
    # as pedras e os torrões soltos na beira do monte
    for k in range(40):
        a = 2 * math.pi * _hash(k, 1, 17)
        r = 3.0 + 1.6 * _hash(k, 2, 17)
        x, y = r * math.cos(a), r * math.sin(a)
        if max(abs(x), abs(y)) > 4.8:
            continue
        sz = 0.08 + 0.15 * _hash(k, 3, 17) ** 2
        p.obox('Earth#clod', (x, y, mz(x, y) + sz * 0.25), (sz * 1.3, sz, sz * 0.7), yaw=_hash(k, 4, 17) * 180, pitch=(_hash(k, 5, 17) - 0.5) * 30,
               bevel=0.03)


# ===================================================================================== a carcaça
def hull(p):
    """O casco do T-55 morto: o corpo (o perfil com o glacis), os para-lamas (o da frente esquerda torto), a grelha do motor e a tampa do
    ventilador, a escotilha do motorista, as caixas e os tambores de combustível nos para-lamas, as lagartas por cima, a roda-guia e as 2
    rodas de apoio na cova, o rombo escuro no glacis com as chapas abertas, os remendos de ferrugem, o anel da torre vazio."""
    F = hull_frame(p)
    prism(p, 'Crate#hull', [F.pt(x, -HW, z) for (x, z) in PROF], F.vec((0.0, 2 * HW, 0.0)))
    # o convés: a beira de chapa grossa em volta (o friso que pega luz) e o anel da torre (a pista fixa)
    for s in (-1, 1):
        F.box('Crate#lip', (-0.6, s * (HW - 0.05), 0.02), (5.7, 0.1, 0.05), bevel=0.0)
    F.cyl('Metal#race', (0.0, 0.0, 0.03), 'z', 1.1, 0.08, seg=48, bevel=0.0)
    F.cyl('Dark#race', (0.0, 0.0, 0.075), 'z', 1.06, 0.02, seg=48, bevel=0.0)
    for k in range(20):
        a = 2 * math.pi * (k + 0.5) / 20
        F.cyl('Dark#bolt', (1.15 * math.cos(a), 1.15 * math.sin(a), 0.02), 'z', 0.035, 0.05, seg=6, bevel=0.0)
    # os para-lamas (o da frente do lado −y arrancado e torto pra cima)
    for s in (-1, 1):
        F.box('Crate#fender', (-1.0, s * (HW + 0.135), -0.33), (5.4, 0.27, 0.035), bevel=0.0)
        if s > 0:
            F.box('Crate#fender', (1.95, s * (HW + 0.135), -0.33), (0.7, 0.27, 0.035), bevel=0.0)
        F.box('Crate#fender', (2.3, s * (HW + 0.135), -0.45), (0.05, 0.27, 0.25), pitch=-35, bevel=0.0)
    F.box('Rust#fender', (1.85, -(HW + 0.2), -0.18), (0.75, 0.27, 0.03), pitch=22, roll=-14, bevel=0.0)
    # as caixas de ferramenta e os 2 tambores no para-lama +y (o lado de longe)
    for x in (-0.3, 0.55):
        F.box('Crate#box', (x, HW + 0.14, -0.15), (0.72, 0.24, 0.32), bevel=0.02)
        F.box('Dark#latch', (x, HW + 0.265, -0.1), (0.1, 0.02, 0.06), bevel=0.0)
    for x in (-2.95, -2.3):
        F.cyl('Rust#drum', (x, HW + 0.15, -0.08), 'x', 0.27, 0.58, seg=16, bevel=0.0, smooth=True)
        for dx in (-0.2, 0.2):
            F.cyl('Dark#hoop', (x + dx, HW + 0.15, -0.08), 'x', 0.285, 0.03, seg=16, bevel=0.0)
    # a grelha do motor (as venezianas escuras), a tampa redonda do ventilador, as dobradiças da tampa do motor
    for k in range(9):
        x = -3.3 + k * 0.18
        F.box('Dark#louver', (x, -0.35, 0.012), (0.08, 1.5, 0.03), pitch=12, bevel=0.0)
    F.box('Crate#plate', (-2.55, -0.35, 0.02), (1.75, 1.7, 0.03), bevel=0.0)
    F.cyl('Crate#fan', (-2.45, 0.85, 0.05), 'z', 0.38, 0.08, seg=24, bevel=0.0)
    for k in range(6):
        a = math.pi * k / 6
        F.box('Dark#fanbar', (-2.45, 0.85, 0.095), (0.7, 0.04, 0.02), yaw=math.degrees(a), bevel=0.0)
    # a escotilha redonda do motorista (fechada), os periscópios, o farol
    F.cyl('Crate#hatch', (0.95, -0.72, 0.04), 'z', 0.33, 0.07, seg=20, bevel=0.0)
    F.cyl('Dark#hinge', (0.95, -0.38, 0.08), 'x', 0.05, 0.25, seg=8, bevel=0.0)
    for dy in (-0.2, 0.2):
        F.box('Dark#scope', (1.28, -0.72 + dy, 0.02), (0.12, 0.14, 0.08), bevel=0.0)
    F.cyl('Dark#lamp', (2.05, 1.1, -0.25), (0.88, 0.0, 0.47), 0.1, 0.12, seg=12, bevel=0.0)
    # as lagartas por cima (a correia com os elos), a volta na roda-guia, a roda-guia; as 2 rodas de apoio na cova (lado −y)
    for s in (-1, 1):
        F.box('Dark#track', (-0.7, s * (HW + 0.13), -0.42), (5.5, 0.27, 0.06), bevel=0.0)
        for k in range(24):
            F.box('Dark#link', (-3.4 + k * 0.23, s * (HW + 0.13), -0.38), (0.07, 0.29, 0.03), bevel=0.0)
        F.cyl('Metal#idler', (2.08, s * (HW + 0.13), -0.78), 'y', 0.29, 0.2, seg=20, bevel=0.0)
        F.cyl('Dark#hub', (2.08, s * (HW + 0.25), -0.78), 'y', 0.1, 0.06, seg=12, bevel=0.0)
        for k in range(8):
            a = math.radians(90 - k * 25)
            F.box('Dark#link', (2.08 + 0.36 * math.cos(a), s * (HW + 0.13), -0.78 + 0.36 * math.sin(a)), (0.21, 0.29, 0.07),
                  pitch=math.degrees(a) - 90, bevel=0.0)
    for x in (-0.85, -1.75):
        F.cyl('Metal#wheel', (x, -(HW + 0.13), -0.95), 'y', 0.39, 0.16, seg=24, bevel=0.0)
        F.cyl('Dark#tyre', (x, -(HW + 0.13), -0.95), 'y', 0.41, 0.1, seg=24, bevel=0.0)
        F.cyl('Dark#hub', (x, -(HW + 0.24), -0.95), 'y', 0.12, 0.06, seg=12, bevel=0.0)
        for k in range(5):
            a = 2 * math.pi * k / 5
            F.cyl('Dark#hole', (x + 0.24 * math.cos(a), -(HW + 0.215), -0.95 + 0.24 * math.sin(a)), 'y', 0.06, 0.012, seg=8, bevel=0.0)
    # o rombo escuro no glacis (o lado −y) e as chapas abertas pra fora em volta dele
    gp = -math.degrees(math.atan2(0.62, 1.2))
    F.box('Dark#breach', (1.92, -0.78, -0.3), (0.72, 0.62, 0.03), pitch=gp, bevel=0.0)
    for (dx, dy, yw, pt, rl) in ((0.25, -0.38, 15, gp + 30, 20), (-0.3, -0.5, -20, gp + 25, -15), (0.1, -1.05, 40, gp + 40, 30)):
        F.box('Rust#shard', (1.92 + dx * 0.8, -0.78 + dy * 0.8 + 0.25, -0.3 + 0.06), (0.22, 0.14, 0.025), yaw=yw, pitch=pt, roll=rl, bevel=0.0)
    # os remendos de ferrugem e de metal raspado (o casco queimado)
    for k in range(5):
        x = HX0 + 0.5 + (HX1 - HX0 - 1.3) * _hash(k, 1, 23)
        y = (_hash(k, 2, 23) - 0.5) * 2.2
        if abs(x) < 1.25 and abs(y) < 1.25:
            continue
        m = 'Rust#patch' if k % 3 else 'Metal#scrape'
        F.box(m, (x, y, 0.008), (0.3 + 0.35 * _hash(k, 3, 23), 0.22 + 0.3 * _hash(k, 4, 23), 0.012), yaw=_hash(k, 5, 23) * 90, bevel=0.0)
    # (os remendos do lado do casco saíram: liam como etiquetas laranja)
    # o cabo de reboque na traseira e os ganchos
    for s in (-1, 1):
        F.box('Dark#hook', (HX0 - 0.06, s * 0.9, -0.55), (0.12, 0.12, 0.18), bevel=0.0)
    tube(p, 'Dark#cable', [F.pt(HX0 - 0.03, -0.9, -0.4), F.pt(HX0 - 0.08, -0.3, -0.62), F.pt(HX0 - 0.08, 0.3, -0.62), F.pt(HX0 - 0.03, 0.9, -0.4)],
         0.03, seg=6)


def mast(p):
    """O mastro soldado na traseira do casco (lado −y): o cano com o cabo enrolado em espiral, a bola do sensor com as 2 lentes no topo,
    o braço com a lâmpada vermelha, o trapo verde amarrado."""
    F = hull_frame(p)
    bx, by, bz = F.pt(MAST_L[0], MAST_L[1], 0.0)
    top = 3.62
    F.box('Metal#plate', (MAST_L[0], MAST_L[1], 0.02), (0.36, 0.36, 0.04), bevel=0.0)
    for (dx, dy) in ((0.12, 0.0), (-0.12, 0.0), (0.0, 0.12)):
        F.box('Metal#gusset', (MAST_L[0] + dx, MAST_L[1] + dy, 0.15), (0.03 if dy == 0 else 0.2, 0.2 if dy == 0 else 0.03, 0.25), bevel=0.0)
    p.cyl('Trim#mast', (bx, by, (bz + top) / 2), 'z', 0.05, top - bz, seg=12, bevel=0.0)
    pts = []
    for k in range(60):
        t = k / 59
        z = bz + 0.3 + (top - bz - 0.6) * t
        a = t * 2 * math.pi * 7
        pts.append((bx + 0.062 * math.cos(a), by + 0.062 * math.sin(a), z))
    tube(p, 'Dark#cable', pts, 0.014, seg=5)
    sc = (bx, by, top + 0.2)
    revolve(p, 'Crate#ball', (sc[0], sc[1]), [(0.02, sc[2] - 0.2), (0.12, sc[2] - 0.17), (0.18, sc[2] - 0.09), (0.2, sc[2]),
                                               (0.18, sc[2] + 0.09), (0.12, sc[2] + 0.17), (0.02, sc[2] + 0.2)], seg=20)
    p.torus('Dark#band', sc, 'z', 0.2, 0.02, seg=20, rseg=5)
    d = Vector((math.cos(math.radians(20)), math.sin(math.radians(20)), 0.0))
    q = Vector(sc) + d * 0.18
    p.cyl('Dark#lens', tuple(q), tuple(d), 0.075, 0.08, seg=14, bevel=0.0)
    p.cyl('Glass', tuple(q + d * 0.042), tuple(d), 0.055, 0.01, seg=14, bevel=0.0)
    # o braço com a lâmpada (pra fora, longe do giro da torre)
    out = Vector((bx, by, 0.0)).normalized()
    a0 = Vector((bx, by, top - 0.55))
    a1 = a0 + out * 0.38
    rod(p, 'Trim#arm', tuple(a0), tuple(a1), 0.025, seg=6)
    p.cyl('Dark#lamp', tuple(a1 + Vector((0, 0, 0.05))), 'z', 0.08, 0.07, seg=12, bevel=0.0)
    p.cyl('Beacon', tuple(a1 + Vector((0, 0, 0.18))), 'z', 0.07, 0.2, seg=14, bevel=0.0, smooth=True)
    p.cyl('Dark#lamp', tuple(a1 + Vector((0, 0, 0.3))), 'z', 0.08, 0.04, seg=12, bevel=0.0)
    rag(p, (bx + 0.05, by - 0.04, 2.55), yaw=HULL_YAW + 70, L=0.5, w=0.15)


def bag_line(p, pts, layers=2, size=(0.66, 0.38, 0.25), seed=0):
    """A fileira de sacos pela polilinha (no quadro do casco), cada saco sentado no monte."""
    L_, W_, H_ = size
    for (a, b) in zip(pts[:-1], pts[1:]):
        (ax, ay), (bx, by) = to_world(*a), to_world(*b)
        L = math.hypot(bx - ax, by - ay)
        yaw = math.degrees(math.atan2(by - ay, bx - ax))
        n = max(1, int(round(L / (L_ * 0.92))))
        for lv in range(layers):
            for k in range(n):
                t = (k + 0.5 + (0.25 if lv % 2 else 0.0)) / n
                if t > 1.0:
                    continue
                x, y = ax + (bx - ax) * t, ay + (by - ay) * t
                z = mz(x, y) - 0.04
                bag(p, (x, y, z + H_ * 0.42 + lv * H_ * 0.82), size, yaw=yaw + (_hash(k, lv, seed) - 0.5) * 14)


def yard(p):
    """Os sacos (o lado −y e a quina de trás, e o grupo da frente do lado +y) e os caixotes compridos de míssil no chão do lado −y."""
    bag_line(p, [(0.15, -2.25), (-1.9, -2.42), (-3.55, -2.3), (-4.25, -1.55), (-4.35, -0.3)], layers=2, seed=3)
    bag_line(p, [(2.3, 2.15), (0.4, 2.3)], layers=2, seed=5)
    bag_line(p, [(-2.4, 2.25), (-3.6, 2.15)], layers=1, seed=7)
    for (lx, ly, dz, yw) in ((0.85, -2.9, 0.0, 6.0), (0.85, -3.38, 0.0, 4.0), (0.9, -3.12, 0.4, 9.0)):
        x, y = to_world(lx, ly)
        crate(p, x, y, mz(x, y) - 0.02 + dz, size=(1.7, 0.44, 0.4), yaw=HULL_YAW + yw)
    x, y = to_world(0.9, -3.12)
    p.obox('Player#stencil', (x, y, mz(x, y) + 0.785), (0.5, 0.22, 0.01), yaw=HULL_YAW + 9, bevel=0.0)


def base():
    p = new_part(PREFIX + '_Base')
    for m, b in (('Earth#mound', 0.0), ('Crate#hull', 0.05), ('Earth#clod', 0.03)):
        p.bm(m, b)
    mound(p)
    hull(p)
    mast(p)
    yard(p)
    return finish(p)


# ===================================================================================== a torre que gira
def frame_vecs():
    e = math.radians(EL)
    return Vector((math.cos(e), 0.0, math.sin(e))), Vector((-math.sin(e), 0.0, math.cos(e))), Vector((0.0, 1.0, 0.0))


def missile_center(s):
    d, u, _ = frame_vecs()
    return Vector((0.0, s * MY, TZ)) + d * 0.42 + u * 0.47


def missile(p, s):
    """O míssil ar-ar (o R-27 roubado): a ogiva longa, o corpo, o bocal; as 4 aletas em X na cauda; a calha de cabos por cima, os 2
    ganchos de baixo; a faixa verde a pincel antes da ogiva; as letras "Р-27" no lado de fora. Devolve a ponta da ogiva."""
    d, u, w = frame_vecs()
    c = missile_center(s)
    seg = 22
    ao = ML / 2 - 0.95

    def ring(a, r):
        return [tuple(c + d * a + (u * math.cos(2 * math.pi * k / seg) + w * math.sin(2 * math.pi * k / seg)) * r) for k in range(seg)]
    prof = [(-ML / 2, MR * 0.8), (-ML / 2 + 0.03, MR * 0.9), (-ML / 2 + 0.16, MR), (ao, MR)]
    for t in (0.16, 0.32, 0.48, 0.63, 0.77, 0.88, 0.96):
        prof.append((ao + 0.95 * t, MR * math.cos(t * math.pi / 2) ** 0.62))
    prof.append((ML / 2, 0.012))
    p.loft('Metal#missile', [ring(a, r) for (a, r) in prof], bevel=0.0, smooth=True)
    p.cyl('Dark#nozzle', tuple(c - d * (ML / 2 - 0.005)), tuple(d), MR * 0.6, 0.03, seg=16, bevel=0.0)
    # as 4 aletas em X (a placa trapezoidal no plano do raio)
    for k in range(4):
        th = math.radians(45 + 90 * k)
        e = u * math.cos(th) + w * math.sin(th)
        n = d.cross(e).normalized()
        a0 = -ML / 2
        pts = [c + d * (a0 + 0.1) + e * MR, c + d * (a0 + 0.66) + e * MR, c + d * (a0 + 0.34) + e * (MR + 0.3), c + d * (a0 + 0.05) + e * (MR + 0.3)]
        prism(p, 'Metal#fin', [tuple(q - n * 0.013) for q in pts], tuple(n * 0.026))
    # a calha de cabos e os ganchos
    p.obox('Metal#race', tuple(c + u * (MR + 0.012) - d * 0.35), (2.2, 0.07, 0.035), pitch=EL, bevel=0.0)
    for a in (-0.6, 0.55):
        p.obox('Dark#lug', tuple(c - u * (MR + 0.03) + d * a), (0.16, 0.08, 0.08), pitch=EL, bevel=0.0)
    # a faixa verde a pincel (a borda tremida)
    rings = []
    for (a, rr) in ((ao - 0.34, MR + 0.008), (ao - 0.06, MR + 0.008), (ao - 0.06, MR - 0.004), (ao - 0.34, MR - 0.004)):
        ring_ = []
        for k in range(seg):
            th = 2 * math.pi * k / seg
            wob = 0.03 * math.sin(3 * th + s) + 0.02 * (_hash(k, int(a * 100), 7) - 0.5)
            ring_.append(tuple(c + d * (a + wob) + (u * math.cos(th) + w * math.sin(th)) * rr))
        rings.append(ring_)
    rings.append(rings[0])
    p.loft('Player#band', rings, bevel=0.0, smooth=True, caps=(False, False))
    # as letras "Р-27" no lado de fora (lidas de quem olha de fora: o texto corre ao contrário do lado +y)
    strokes = [((0, 0), (0, 1)), ((0, 1), (0.42, 1)), ((0.42, 1), (0.56, 0.86)), ((0.56, 0.86), (0.56, 0.64)), ((0.56, 0.64), (0.42, 0.5)),
               ((0.42, 0.5), (0, 0.5)), ((0.85, 0.5), (1.2, 0.5)), ((1.45, 0.84), (1.58, 1)), ((1.58, 1), (1.9, 1)), ((1.9, 1), (2.03, 0.84)),
               ((2.03, 0.84), (2.03, 0.66)), ((2.03, 0.66), (1.45, 0)), ((1.45, 0), (2.08, 0)), ((2.3, 1), (2.9, 1)), ((2.9, 1), (2.52, 0))]
    h, sw = 0.25, 0.2
    th_c = math.radians(68) * s
    a_start = -0.05

    def surf(tx, b, off):
        a = a_start - s * (tx - 1.45) * h
        th = th_c - s * (b - 0.5) * h / MR
        return tuple(c + d * a + (u * math.cos(th) + w * math.sin(th)) * (MR + off))
    for (p0, p1) in strokes:
        dx, db = p1[0] - p0[0], p1[1] - p0[1]
        L = math.hypot(dx, db)
        tx, tb = dx / L, db / L
        nx, nb = -tb * sw / 2, tx * sw / 2
        q0 = (p0[0] - tx * sw / 2, p0[1] - tb * sw / 2)
        q1 = (p1[0] + tx * sw / 2, p1[1] + tb * sw / 2)
        quad = [(q0[0] + nx, q0[1] + nb), (q1[0] + nx, q1[1] + nb), (q1[0] - nx, q1[1] - nb), (q0[0] - nx, q0[1] - nb)]
        p.loft('Dark#stencil', [[surf(x, b, 0.004) for (x, b) in quad], [surf(x, b, 0.013) for (x, b) in quad]], bevel=0.0)
    return tuple(c + d * ML / 2)


def umbrella(p, cx, cy, z0, ztop):
    """O guarda-sol de praia velho (verde): a vara presa no banco, a lona de 8 gomos com a barriga entre as varetas e a borda recortada,
    as varetas por baixo, a franja de trapos na borda, o remate."""
    R, zr = 1.02, ztop - 0.32
    p.cyl('Trim#pole', (cx, cy, (z0 + ztop) / 2), 'z', 0.025, ztop - z0, seg=8, bevel=0.0)

    def fn(uu, vv):
        th = 2 * math.pi * uu
        fr = (th / (math.pi / 4)) % 1.0
        r = 0.05 + (R - 0.05) * vv
        r *= 1.0 - 0.07 * math.sin(math.pi * fr) * vv ** 2
        z = ztop - (ztop - zr) * vv ** 1.15 - 0.06 * math.sin(math.pi * fr) * math.sin(math.pi * min(1.0, vv * 1.1))
        return (cx + r * math.cos(th), cy + r * math.sin(th), z)
    surface(p, 'Player#canopy', fn, 40, 6, thick=0.025)
    for k in range(8):
        th = 2 * math.pi * k / 8
        tip = (cx + R * math.cos(th), cy + R * math.sin(th), zr - 0.03)
        rod(p, 'Trim#rib', (cx, cy, ztop - 0.03), tip, 0.011, seg=4)
        rod(p, 'Trim#rib', (cx, cy, ztop - 0.75), (cx + 0.5 * R * math.cos(th), cy + 0.5 * R * math.sin(th), zr + 0.12), 0.009, seg=4)
    for k in range(40):
        th = 2 * math.pi * (k + 0.5) / 40
        x, y, z = fn((k + 0.5) / 40, 1.0)
        p.obox('Player#fringe', (x, y, z - 0.07), (0.035, 0.07, 0.13 + 0.06 * _hash(k, 1, 9)), yaw=math.degrees(th) + 90,
               roll=(_hash(k, 2, 9) - 0.5) * 30, bevel=0.0)
    p.cyl('Dark#finial', (cx, cy, ztop + 0.05), 'z', 0.04, 0.1, seg=8, bevel=0.0)


def turret():
    """A torre (gira em (0, 0)): o anel em degraus com os parafusos, a coluna, a forquilha com os munhões, o berço soldado (a travessa, os
    2 trilhos com a treliça e as selas), os 2 mísseis; o estrado em balanço com a poltrona, o painel com os manches, as caixas e o
    guarda-sol."""
    p = new_part(PREFIX + '_Turret')
    for m in ('Metal#missile', 'Player#band', 'Player#canopy'):
        p.bm(m, 0.0)
    z0 = DECK + 0.02
    for (r, za, zb) in ((1.02, z0 - 0.05, z0 + 0.17), (0.8, z0 + 0.17, z0 + 0.33), (0.56, z0 + 0.33, z0 + 0.48)):
        p.cyl('Metal#ring', (0.0, 0.0, (za + zb) / 2), 'z', r, zb - za, seg=48, bevel=0.012)
        p.torus('Dark#step', (0.0, 0.0, zb - 0.01), 'z', r - 0.035, 0.018, seg=48, rseg=5)
    for k in range(24):
        a = 2 * math.pi * (k + 0.5) / 24
        p.cyl('Dark#bolt', (0.92 * math.cos(a), 0.92 * math.sin(a), z0 + 0.19), 'z', 0.035, 0.05, seg=6, bevel=0.0)
    p.cyl('Metal#post', (0.0, 0.0, (z0 + 0.48 + TZ - 0.22) / 2), 'z', 0.27, TZ - 0.22 - z0 - 0.48, seg=20, bevel=0.0, smooth=True)
    p.cyl('Dark#collar', (0.0, 0.0, TZ - 0.24), 'z', 0.31, 0.06, seg=20, bevel=0.0)
    for s in (-1, 1):
        p.prism_xz('Metal#yoke', [(-0.34, TZ - 0.27), (0.34, TZ - 0.27), (0.17, TZ + 0.13), (-0.17, TZ + 0.13)], s * 0.33, s * 0.4, bevel=0.0)
        p.cyl('Dark#pin', (0.0, s * 0.43, TZ), 'y', 0.1, 0.08, seg=14, bevel=0.0)
    d, u, w = frame_vecs()
    T = Vector((0.0, 0.0, TZ))
    # o berço: a travessa, os 2 trilhos de viga com a treliça soldada por baixo e as selas
    p.obox('Metal#cradle', tuple(T + u * 0.05), (0.32, 2 * MY + 0.32, 0.2), pitch=EL, bevel=0.02)
    noses = []
    for s in (-1, 1):
        rc = T + w * s * MY + u * 0.2 + d * 0.3
        p.obox('Metal#rail', tuple(rc), (2.8, 0.13, 0.12), pitch=EL, bevel=0.015)
        p.obox('Dark#rail', tuple(rc + u * 0.07), (2.7, 0.05, 0.02), pitch=EL, bevel=0.0)
        for a in (-1.25, 1.65):
            rod(p, 'Metal#truss', tuple(T + w * s * MY * 0.55 + u * 0.0), tuple(rc + d * a - u * 0.05), 0.035, seg=6)
        for k in range(5):
            a0, a1 = -1.2 + k * 0.6, -0.9 + k * 0.6
            rod(p, 'Metal#truss', tuple(rc + d * a0 - u * 0.06), tuple(rc + d * a1 - u * 0.25), 0.02, seg=5)
            rod(p, 'Metal#truss', tuple(rc + d * a1 - u * 0.25), tuple(rc + d * (a0 + 0.6) - u * 0.06), 0.02, seg=5)
        rod(p, 'Metal#truss', tuple(rc + d * -1.2 - u * 0.25), tuple(rc + d * 1.5 - u * 0.25), 0.022, seg=5)
        mc = missile_center(s)
        for a in (-0.6, 0.55):
            q = mc + d * a
            p.obox('Dark#saddle', tuple(q - u * (MR + 0.06)), (0.22, 0.2, 0.1), pitch=EL, bevel=0.0)
        noses.append(missile(p, s))
    # o estrado em balanço com a poltrona, o painel e as caixas
    sx, sy = SEAT
    zs = z0 + 0.22
    p.box('Metal#plate', sx - 0.5, sx + 0.5, sy - 0.5, sy + 0.45, zs - 0.05, zs, bevel=0.0)
    for (ax, ay) in ((0.55, -0.62), (-0.65, -0.55)):
        rod(p, 'Metal#arm', (ax, ay, z0 + 0.1), (sx + (0.4 if ax > 0 else -0.4), sy + 0.3, zs - 0.06), 0.045, seg=6)
    rod(p, 'Metal#arm', (0.0, -0.75, z0 + 0.02), (sx, sy, zs - 0.06), 0.04, seg=6)
    for (dx, dy) in ((-0.22, -0.22), (0.22, -0.22), (-0.22, 0.22), (0.22, 0.22)):
        rod(p, 'Metal#seatleg', (sx + dx, sy + dy, zs), (sx + dx * 0.9, sy + dy * 0.9, zs + 0.36), 0.025, seg=6)
    p.obox('Rust#seat', (sx, sy, zs + 0.44), (0.56, 0.56, 0.16), bevel=0.05)
    p.obox('Rust#seat', (sx - 0.28, sy, zs + 0.82), (0.13, 0.56, 0.66), pitch=12, bevel=0.05)
    p.obox('Rust#seat', (sx - 0.33, sy, zs + 1.2), (0.12, 0.34, 0.16), pitch=12, bevel=0.04)
    for s in (-1, 1):
        p.obox('Rust#seat', (sx + 0.02, sy + s * 0.32, zs + 0.64), (0.48, 0.1, 0.08), bevel=0.03)
        rod(p, 'Metal#seatleg', (sx + 0.2, sy + s * 0.32, zs + 0.44), (sx + 0.2, sy + s * 0.32, zs + 0.6), 0.02, seg=5)
    rod(p, 'Metal#seatleg', (sx + 0.42, sy, zs), (sx + 0.42, sy, zs + 0.72), 0.03, seg=6)
    p.obox('Crate#console', (sx + 0.48, sy, zs + 0.84), (0.24, 0.42, 0.26), pitch=-20, bevel=0.03)
    p.obox('Dark#screen', (sx + 0.42, sy, zs + 0.98), (0.04, 0.3, 0.14), pitch=-35, bevel=0.0)
    for s in (-1, 1):
        rod(p, 'Dark#stick', (sx + 0.5, sy + s * 0.13, zs + 0.95), (sx + 0.46, sy + s * 0.13, zs + 1.1), 0.018, seg=5)
    crate(p, sx - 0.05, sy - 0.62 + 0.18, zs, size=(0.5, 0.28, 0.26), yaw=5)
    crate(p, sx + 0.25, sy - 0.33, zs, size=(0.36, 0.26, 0.22), yaw=-12)
    umbrella(p, sx - 0.32, sy - 0.32, zs + 0.3, 3.3)
    o = finish(p)
    return o, noses


def write_preview(out):
    import json
    # (a folha: a torre girada 45°, os mísseis atravessando o casco como no conceito; o repouso do jogo é +X)
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]], [PREFIX + '_Turret', [0, 0, 0], 45.0]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1300, 1300]], ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1300, 800]],
                      ['side_left', 'ortho', [0, -1, 0], [0, 0, 1], [1300, 800]],
                      ['three_quarter', 'persp', 32.0, 215.0, [1500, 1000], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 32.0, 35.0, [1500, 1000], 30.0, 0.95]],
            'usa_at': [6.5, -7.5, 0.0], 'tank_views': True,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(out, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, indent=1)


def main():
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'gla_sam_site')
    os.makedirs(out, exist_ok=True)
    clear_scene()
    total = export(base(), out)
    clear_scene()
    t, noses = turret()
    total += export(t, out)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -4.4, 4.4, -4.0, 4.0, 2.7, post='Wood', plank='Crate', stripe='Hazard'), out)
    write_preview(out)
    F = None
    bx, by = to_world(*MAST_L)
    print('MODEL total', total, 'tris')
    print('MODEL lote 10 x 10 m; turret_pivot_m (0, 0); munhoes z', round(TZ, 2), '; elevacao', EL, '; muzzles (pontas das ogivas)',
          [tuple(round(v, 2) for v in n) for n in noses], '; muzzle_height_m', round(sum(n[2] for n in noses) / 2, 2))
    print('MODEL mastro (x, y)', (round(bx, 2), round(by, 2)), 'ate z 3.82; lampada ~3.2')
    lx, ly = to_world(-2.5, 0.2)
    fx, fy = to_world(1.9, -0.8)
    print('MODEL fire_points_m', [(round(lx, 2), round(ly, 2), 1.0), (round(fx, 2), round(fy, 2), 0.7), (0.0, 0.0, 1.5)])


if __name__ == '__main__':
    main()
