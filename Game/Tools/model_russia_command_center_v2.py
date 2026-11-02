"""O Quartel-General da Rússia (Command Center, o Штаб), v2 (01/11/2026): refeito pelo veredito da galeria ("parece uma prisão";
o v1, model_russia_command_center.py, fica guardado pra virar outra coisa).

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_russia_command_center_v2.py C:/Users/luisf/explosion/captures/game/models/russia_command_center_v2

A ideia: o ARRANHA-CÉU STALINISTA EM MINIATURA (as "Sete Irmãs" de Moscou: o MID, a Universidade, o Kotelnicheskaya) assentado num
BUNKER DE COMANDO moderno. A palavra (o que lê de cima num relance): A TORRE EM DEGRAUS COM A AGULHA DOURADA. De cima, os 3 degraus
de telhado verde contornados pelas cornijas brancas, um dentro do outro, com os obeliscos nas quinas (o "bolo de casamento" que só
Moscou tem); na câmera do jogo, a agulha de ouro subindo do tambor octogonal e, no topo, a ESTRELA como a do Kremlin, com o emblema
aprovado em 01/11 (docs/elenco/EMBLEMAS.md: a estrela vermelha com as bordas branca, azul e vermelha, a das Forças Armadas russas; a
águia bicéfala da 1ª proposta saiu). Embaixo, o bunker de concreto de paredes inclinadas e
cantos redondos (a base acompanha a forma: nada de laje quadrada embaixo de tudo), o portão blindado de 2 folhas abertas por onde
sai o trator, a antena de radar que gira, as parabólicas e o mastro de antenas.
Por que não a prisão: o v1 era o bloco de painel cinza de janelinhas iguais com a cerca de concreto e a guarita (a cara de presídio).
Aqui a fachada é VERTICAL (os pilares brancos subindo, as janelas em faixas escuras entre eles), o prédio afina pra cima e termina
na agulha: lê como monumento, não como bloco; e a cerca e a guarita saíram (o quartel é que tem o portão com a cancela).

Peça por peça:
- o bunker (21,6 x 25,2 m no pé, 3,4 m de parede inclinada ~16°, os cantos redondos de 3,2 m): o concreto com as juntas, o
  rodapé, a coroa da mureta, as tomadas de ar com grade, as saídas de emergência, o PORTAL do portão (o bloco reto saindo da
  parede inclinada, as 2 folhas de aço de 0,35 m abertas pra fora com as nervuras e os ferrolhos, o perigo nos batentes, a rampa);
- o terraço do bunker: o RADAR que gira (o refletor de treliça parabólica no pedestal: _Radar), as 2 parabólicas, o mastro
  treliçado com os dipolos, as antenas de chicote, os exaustores do gerador, as escotilhas, as 2 bandeiras da cor do jogador;
- a torre: 3 degraus (6,0 + 3,6 + 2,8 m) de pilares brancos com as faixas de janela, a cornija branca e a mureta com os
  pilaretes em cada degrau, os 12 OBELISCOS das quinas com a bola de ouro, o portal da frente com as colunas e as 2 faixas
  verticais da cor do jogador, o tambor octogonal com as janelas altas, a base de pedra da agulha com as nervuras, a AGULHA DOURADA,
  o globo e a ESTRELA de dupla face (1,9 m de ponta a ponta), e a mesma estrela em cima do portão do bunker.

Lote 26 x 28 m (X x Y), frente em +X. Alturas: o bunker 3,85 m na mureta, a torre 16,0 m no 3º degrau, o tambor 17,6, a agulha
23,8, a estrela até ~25,9 m.

Peças (origem no chão, no centro do lote; coordenadas da Unreal, X frente, Y direita, Z cima, metros):
  SM_Russia_CommandCenter_Base: tudo o que não se mexe (as bandeiras saem no flags.json);
  SM_Russia_CommandCenter_Radar: a antena de radar (gira em Z; origem no eixo, no topo do pedestal: at_m = RADAR_AT, 6 rpm);
  SM_Russia_CommandCenter_Scaffold: o andaime da obra.
Materiais (os slots da Rússia): Concrete, Roof, Metal, Red, White, Paint, Team, Glass, Light, Dark, Trim, Hazard, Wood, Rubber; Beacon;
Gold (a agulha, os anéis, o globo e as bolas dos obeliscos; o importador leva pro CnGold) e Navy (a borda azul da estrela do
ru_star, o emblema aprovado de 01/11: na ponta da agulha e em cima do portão).
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import clear_scene, export, scaffold  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
from model_faction_kit import lamp, revolve  # noqa: E402
from model_building_parts import WallFace, wbox, tube, splits, bake_in_place  # noqa: E402
from model_russia_parts import RuPart, ru_star, stencil, flag_pole, lattice_mast, vent_shaft, ac_box  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'russia_command_center_v2')
PREFIX = 'SM_Russia_CommandCenter'

HX, HY = 13.0, 14.0                  # o lote 26 x 28
BC = (-1.6, 0.0)                     # o centro do bunker
B_BOT = (11.0, 12.6, 3.2)            # (meia-largura x, meia-largura y, raio do canto) no pé
B_TOP = (10.0, 11.6, 2.4)            # no alto da parede inclinada
BZ = 3.4                             # o alto da parede inclinada
CZ = 3.85                            # o topo da mureta
RZ = 3.6                             # o piso do terraço
CX, CY = -2.6, 0.0                   # o eixo da torre
TIERS = [(5.4, 6.0, RZ, 9.6, 2), (3.8, 4.3, 9.6, 13.2, 1), (2.4, 2.7, 13.2, 16.0, 1)]   # (meia x, meia y, z0, z1, andares)
DRUM_R, DRUM_Z = 1.6, (16.0, 17.6)
SPIRE_Z = (17.6, 19.8, 23.8)         # a base de pedra, o pé da agulha de ouro, a ponta
STAR_R = 0.62                        # o raio da estrela vermelha do meio (a de fora: 0,95 m; 1,9 m de ponta a ponta)
STAR_C = (CX, CY, 23.8 + 0.47 + 0.42 * 1.53 * STAR_R - 0.04)   # o vértice de baixo do meio assenta 4 cm no globo
PORTAL = (8.0, 10.4, 2.4, 3.3, 4.4)  # o portal do portão: x de trás, x da frente, meia-largura do vão, altura do vão, topo
RADAR_AT = (-7.6, 8.9, RZ + 1.05)


# ===================================================================================================== utilidades
def rr_ring(cx, cy, hx, hy, r, z, kx=40, ky=48, kc=12):
    """O anel do retângulo de cantos redondos (sempre o mesmo número de pontos: os anéis viram loft), sentido anti-horário."""
    pts = []
    sx, sy = hx - r, hy - r
    for i in range(ky):
        pts.append((cx + hx, cy - sy + 2 * sy * i / ky, z))
    for i in range(kc):
        a = 0.5 * math.pi * i / kc
        pts.append((cx + sx + r * math.cos(a), cy + sy + r * math.sin(a), z))
    for i in range(kx):
        pts.append((cx + sx - 2 * sx * i / kx, cy + hy, z))
    for i in range(kc):
        a = 0.5 * math.pi * (1 + i / kc)
        pts.append((cx - sx + r * math.cos(a), cy + sy + r * math.sin(a), z))
    for i in range(ky):
        pts.append((cx - hx, cy + sy - 2 * sy * i / ky, z))
    for i in range(kc):
        a = 0.5 * math.pi * (2 + i / kc)
        pts.append((cx - sx + r * math.cos(a), cy - sy + r * math.sin(a), z))
    for i in range(kx):
        pts.append((cx - sx + 2 * sx * i / kx, cy - hy, z))
    for i in range(kc):
        a = 0.5 * math.pi * (3 + i / kc)
        pts.append((cx + sx + r * math.cos(a), cy - sy + r * math.sin(a), z))
    return pts


def lerp_ring(A, B, t):
    return [tuple(a[k] + (b[k] - a[k]) * t for k in range(3)) for a, b in zip(A, B)]


def bunker_at(z):
    """(hx, hy, r) da parede inclinada do bunker na altura z."""
    t = max(0.0, min(1.0, z / BZ))
    return tuple(B_BOT[k] + (B_TOP[k] - B_BOT[k]) * t for k in range(3))


def batter_frame(face, z):
    """O referencial da face reta inclinada do bunker ('+x', '-x', '+y', '-y') na altura z: (ponto da face no meio dela, a direção ao
    longo, a direção subindo pela face, a normal pra fora)."""
    hx, hy, _ = bunker_at(z)
    lean = (B_BOT[0] - B_TOP[0]) / BZ
    s = 1 if face[0] == '+' else -1
    if face[1] == 'x':
        c = Vector((BC[0] + s * hx, BC[1], z))
        along = Vector((0, 1, 0))
        upv = Vector((-s * lean, 0, 1)).normalized()
        n = Vector((s, 0, lean)).normalized()
    else:
        c = Vector((BC[0], BC[1] + s * hy, z))
        along = Vector((1, 0, 0))
        upv = Vector((0, -s * lean, 1)).normalized()
        n = Vector((0, s, lean)).normalized()
    return c, along, upv, n


def on_batter(p, m, face, u, z0, z1, w, out, proud):
    """A caixa assentada na face inclinada do bunker (de z0 a z1 ao longo da face, largura w, saindo 'proud'; 'out' = quanto entra
    na parede por trás)."""
    from model_russia_vehicle_kit import frame_box
    c0, al, upv, n = batter_frame(face, z0)
    c1, _, _, _ = batter_frame(face, z1)
    L = (c1 - c0).length
    mid = (c0 + c1) / 2 + al * u + n * ((proud - out) / 2)
    frame_box(p, m, mid, al, upv, n, w, L, proud + out, bevel=0.0)


# ===================================================================================================== o bunker
def bunker(p):
    """O casco do bunker: um loft só (o rodapé, a parede inclinada, a mureta com a coroa, o piso do terraço até o pé da torre)."""
    rings = []
    # o rodapé (um pouco pra fora, abaixo do chão) e a parede inclinada em fileiras de ~0,38 m
    hx, hy, r = B_BOT
    rings.append(rr_ring(BC[0], BC[1], hx + 0.18, hy + 0.18, r + 0.18, -0.06))
    rings.append(rr_ring(BC[0], BC[1], hx + 0.18, hy + 0.18, r + 0.18, 0.3))
    hx, hy, r = bunker_at(0.3)
    rings.append(rr_ring(BC[0], BC[1], hx, hy, r, 0.3))
    n = 9
    for k in range(1, n + 1):
        z = 0.3 + (BZ - 0.3) * k / n
        hx, hy, r = bunker_at(z)
        rings.append(rr_ring(BC[0], BC[1], hx, hy, r, z))
    # a mureta: sobe reta, a coroa arredondada, desce por dentro até o piso
    hx, hy, r = B_TOP
    rings.append(rr_ring(BC[0], BC[1], hx + 0.06, hy + 0.06, r + 0.06, BZ + 0.08))
    rings.append(rr_ring(BC[0], BC[1], hx + 0.06, hy + 0.06, r + 0.06, CZ - 0.06))
    rings.append(rr_ring(BC[0], BC[1], hx - 0.04, hy - 0.04, r - 0.04, CZ))
    rings.append(rr_ring(BC[0], BC[1], hx - 0.34, hy - 0.34, r - 0.34, CZ - 0.02))
    rings.append(rr_ring(BC[0], BC[1], hx - 0.4, hy - 0.4, r - 0.4, RZ))
    # o piso do terraço, em passos de ~0,4 m até o pé da torre (o retângulo de cantos quase vivos embaixo do 1º degrau)
    outer = rings[-1]
    inner = rr_ring(CX, CY, TIERS[0][0] - 0.3, TIERS[0][1] - 0.3, 0.25, RZ)
    steps = 14
    for k in range(1, steps + 1):
        rings.append(lerp_ring(outer, inner, k / steps))
    p.loft('Concrete#bunker', rings, bevel=0.0, smooth=False, caps=(False, True))
    # as juntas de dilatação (as frestas escuras descendo a parede inclinada) nos 4 lados retos
    for face, us in (('+x', [-8.8, -6.6, -4.6, 4.6, 6.6, 8.8]), ('-x', [-8.6, -3.0, 0.0, 3.0, 8.6]),
                     ('+y', [-7.4, -2.6, 0.4, 5.4]), ('-y', [-7.4, -2.6, 0.4, 5.4])):
        for u in us:
            on_batter(p, 'Rubber#joint', face, u, 0.32, BZ - 0.02, 0.05, 0.02, 0.012)
    # a faixa de concreto mais escuro na altura do rodapé (as marcas da forma) e os grampos de içar na coroa
    for face in ('+y', '-y', '-x'):
        on_batter(p, 'Concrete#band', face, 0.0, 1.55, 1.75, 2 * (B_BOT[1 if face[1] == 'x' else 0] - B_BOT[2]) - 0.6, 0.02, 0.025)


def portal(p):
    """O portal do portão do trator na frente: o bloco reto saindo da parede inclinada, o vão escuro, as 2 folhas blindadas abertas pra
    fora, os batentes com o perigo, a rampa e a luz."""
    x0, x1, hw, dh, zt = PORTAL
    F = WallFace('+x', x1, 0.0)
    for (a, b) in ((-hw - 1.2, -hw), (hw, hw + 1.2)):
        p.box('Concrete#portal', x0, x1, a, b, -0.05, zt, bevel=0.08)
    p.box('Concrete#lintel', x0, x1 - 0.02, -hw - 0.02, hw + 0.02, dh, zt - 0.02, bevel=0.06)
    p.box('Concrete#lintel2', x1 - 0.18, x1 + 0.12, -hw - 1.35, hw + 1.35, zt - 0.02, zt + 0.25, bevel=0.06)     # a coroa do portal
    p.box('Dark', x1 - 0.62, x1 - 0.6, -hw, hw, 0.0, dh, bevel=0.0)                                                # o fundo do vão
    p.box('Light', x1 - 0.58, x1 - 0.56, -0.6, 0.6, dh - 0.25, dh - 0.18, bevel=0.0)
    # o perigo nos batentes (as listras diagonais pintadas, por faixas) e a soleira de aço
    for s in (-1, 1):
        y = s * hw
        for k in range(6):
            z = 0.1 + k * 0.32
            m = 'Hazard' if k % 2 == 0 else 'Dark'
            wbox(p, m + '#jamb', WallFace('+x', x1, 0.0), y - 0.04 if s > 0 else y - 0.38, y + 0.38 if s > 0 else y + 0.04,
                 z, z + 0.3, 0.0, 0.015, bevel=0.0)
    p.box('Metal#sill', x1 - 0.6, x1 + 0.1, -hw, hw, -0.02, 0.05, bevel=0.0)
    # as 2 folhas blindadas de 0,35 m abertas a 100° pra fora (as nervuras, os ferrolhos, as dobradiças grandes)
    for s in (-1, 1):
        hy_ = s * (hw + 0.05)
        ang = math.radians(100.0)
        dirv = Vector((math.sin(ang), -s * math.cos(ang), 0.0))           # da dobradiça pra ponta livre
        Lf = hw - 0.05
        from model_russia_vehicle_kit import frame_box
        nrm = Vector((dirv.y * s, -dirv.x * s, 0.0)) * -1.0
        if nrm.y * s < 0:
            nrm = -nrm
        c = Vector((x1 + 0.2, hy_, 0.0)) + dirv * (Lf / 2)
        frame_box(p, 'Paint#door', (c.x, c.y, dh / 2 + 0.02), dirv, Vector((0, 0, 1)), nrm, Lf, dh - 0.04, 0.35, bevel=0.03)
        for k in range(4):                                   # as nervuras do lado de fora
            q = Vector((x1 + 0.2, hy_, 0.0)) + dirv * (0.35 + k * (Lf - 0.6) / 3)
            frame_box(p, 'Paint#rib', Vector((q.x, q.y, dh / 2 + 0.02)) + nrm * 0.21, dirv, Vector((0, 0, 1)), nrm, 0.12, dh - 0.3,
                      0.08, bevel=0.015)
        for z in (0.6, dh - 0.6):                            # os ferrolhos e as dobradiças
            q = Vector((x1 + 0.2, hy_, z)) + dirv * (Lf - 0.25)
            frame_box(p, 'Metal#bolt', q - nrm * 0.22, dirv, Vector((0, 0, 1)), nrm, 0.36, 0.12, 0.08, bevel=0.01)
            p.cyl('Metal#hinge', (x1 + 0.12, hy_, z), 'z', 0.13, 0.55, seg=14, bevel=0.0)
        p.cyl('Metal#hinge', (x1 + 0.12, hy_, dh / 2), 'z', 0.1, dh - 0.2, seg=12, bevel=0.0)
    # a rampa (o avental de concreto na frente do vão, com a junta e as faixas) e a luz em cima do vão
    p.box('Concrete#apron', x1 - 0.05, HX - 0.15, -hw - 0.6, hw + 0.6, -0.05, 0.08, bevel=0.04)
    for y in (-hw - 0.35, hw + 0.35):
        p.box('Hazard#apron', x1 + 0.2, HX - 0.4, y - 0.08, y + 0.08, 0.08, 0.085, bevel=0.0)
    # o emblema (a estrela aprovada: ru_star) no meio da verga, as 2 luzes dos lados e o número do portão
    ru_star(p, F, 0.0, dh + 0.5, 0.24, n=0.0, t=0.025)
    for u in (-1.55, 1.55):
        lamp(p, x1 + 0.06, u, dh + 0.42, face='+x')
    stencil(p, F, -hw + 0.1, dh + 0.62, 'ШБ-01', h=0.26, m='White', n=0.0)


def side_features(p):
    """As tomadas de ar com grade (o bloco saindo da parede inclinada) e as saídas de emergência dos lados e de trás."""
    from model_russia_vehicle_kit import frame_box
    for face, u in (('+y', -4.4), ('+y', 3.6), ('-y', -4.4), ('-y', 3.6), ('-x', -5.5), ('-x', 5.5)):
        c, al, upv, n = batter_frame(face, 0.0)
        s = 1 if face[0] == '+' else -1
        hn = Vector((n.x, n.y, 0)).normalized()
        base = c + al * u
        # o bloco reto (em pé) que sai da parede: 1,8 de largo, 1,5 de alto, 1,1 pra fora
        q = base + hn * 0.2
        frame_box(p, 'Concrete#vent', (q.x, q.y, 0.75), al, Vector((0, 0, 1)), hn, 1.8, 1.55, 1.6, bevel=0.06)
        g = base + hn * 1.0
        frame_box(p, 'Dark', (g.x, g.y, 0.85), al, Vector((0, 0, 1)), hn, 1.3, 0.8, 0.02, bevel=0.0)
        for k in range(6):
            frame_box(p, 'Metal#grille', Vector((g.x, g.y, 0.52 + k * 0.13)) + hn * 0.02, al, Vector((0, 0, 1)), hn, 1.3, 0.04, 0.04,
                      bevel=0.0)
        frame_box(p, 'Concrete#ventcap', (q.x, q.y, 1.58), al, Vector((0, 0, 1)), hn, 2.0, 0.12, 1.42, bevel=0.03)
    # as 2 saídas de emergência (a porta de aço no bloquinho) nos lados ±y, perto de trás
    for face, u in (('+y', -6.9), ('-y', -6.9)):
        c, al, upv, n = batter_frame(face, 0.0)
        hn = Vector((n.x, n.y, 0)).normalized()
        q = c + al * u + hn * 0.05
        frame_box(p, 'Concrete#exit', (q.x, q.y, 1.2), al, Vector((0, 0, 1)), hn, 1.9, 2.4, 1.6, bevel=0.06)
        d = c + al * u + hn * 0.86
        frame_box(p, 'Paint#exitdoor', (d.x, d.y, 1.05), al, Vector((0, 0, 1)), hn, 0.95, 1.95, 0.05, bevel=0.01)
        frame_box(p, 'Metal#exitbar', Vector((d.x, d.y, 1.1)) + hn * 0.04, al, Vector((0, 0, 1)), hn, 0.7, 0.06, 0.04, bevel=0.0)


# ===================================================================================================== a torre
def obelisk(p, g, x, y, z, h=2.0, b=0.5):
    """O obelisco da quina (o pináculo stalinista): o pedestal, o fuste afinando em 4 faces, a bola de ouro (g)."""
    p.box('White#obped', x - b * 0.62, x + b * 0.62, y - b * 0.62, y + b * 0.62, z, z + 0.45, bevel=0.04)
    p.box('White#obcap', x - b * 0.7, x + b * 0.7, y - b * 0.7, y + b * 0.7, z + 0.45, z + 0.55, bevel=0.02)
    rings = []
    for (hb, zz) in ((b * 0.48, z + 0.55), (b * 0.24, z + 0.55 + h * 0.92)):
        rings.append([(x + hb, y + hb, zz), (x - hb, y + hb, zz), (x - hb, y - hb, zz), (x + hb, y - hb, zz)])
    p.loft('White#obel', rings, bevel=0.0, caps=(False, True))
    top = z + 0.55 + h * 0.92
    g.cyl('Gold#ob', (x, y, top + 0.06), 'z', 0.05, 0.12, seg=10, bevel=0.0)
    vp.lathe(g, 'Gold#ob', (x, y, top + 0.12), 'z', [(0.0, 0.0), (0.09, 0.02), (0.13, 0.12), (0.09, 0.22), (0.02, 0.27), (0.015, 0.4),
                                                    (0.0, 0.42)], seg=14)


def tier(p, g, k):
    """Um degrau da torre: o miolo, os pilares brancos subindo (a fachada vertical), as faixas de janela escuras entre eles, o soco, a
    cornija branca, a mureta com os pilaretes, o telhado verde e os 4 obeliscos das quinas."""
    hx, hy, z0, z1, floors = TIERS[k]
    x0, x1, y0, y1 = CX - hx, CX + hx, CY - hy, CY + hy
    p.box('Concrete#tier%d' % k, x0, x1, y0, y1, z0 - 0.1, z1, bevel=0.05)
    fh = (z1 - z0 - 0.5) / floors
    for face, at, (a, b) in (('+x', x1, (y0, y1)), ('-x', x0, (y0, y1)), ('+y', y1, (x0, x1)), ('-y', y0, (x0, x1))):
        F = WallFace(face, at, 0.0)
        span = b - a - 1.4
        n = max(2, int(round(span / 1.55)))
        us = [a + 0.7 + span * i / n for i in range(n + 1)]
        portal_bay = (k == 0 and face == '+x')
        # o soco (a faixa de pedra embaixo) e a cinta entre os andares
        for (sa, sb) in (((a + 0.02, CY - 1.3), (CY + 1.3, b - 0.02)) if portal_bay else ((a + 0.02, b - 0.02),)):
            wbox(p, 'White#socle%d%s' % (k, face), F, sa, sb, z0 - 0.05, z0 + 0.45, -0.02, 0.1, bevel=0.03)
        for i in range(n + 1):
            u = us[i]
            if portal_bay and abs(u - CY) < 2.0:
                continue
            wbox(p, 'White#pier%d%s' % (k, face), F, u - 0.2, u + 0.2, z0 + 0.45, z1 - 0.05, -0.02, 0.2, bevel=0.03)
        for i in range(n):
            ua, ub = us[i] + 0.2, us[i + 1] - 0.2
            mid = (ua + ub) / 2
            if portal_bay and abs(mid - CY) < 1.6:
                continue
            for f in range(floors):
                zf = z0 + 0.5 + f * fh
                wz0, wz1 = zf + 0.55, zf + fh - 0.5
                lit = ((i * 7 + f * 3 + k * 5 + len(face)) % 9 == 0)
                wbox(p, ('Light' if lit else 'Glass') + '#w', F, ua + 0.12, ub - 0.12, wz0, wz1, 0.0, 0.02, bevel=0.0)
                wbox(p, 'White#wf%d' % k, F, mid - 0.04, mid + 0.04, wz0, wz1, 0.0, 0.06, bevel=0.0)
                wbox(p, 'White#wf%d' % k, F, ua + 0.12, ub - 0.12, wz1 - 0.42, wz1 - 0.36, 0.0, 0.06, bevel=0.0)
                wbox(p, 'White#sill%d' % k, F, ua + 0.06, ub - 0.06, wz0 - 0.08, wz0, 0.0, 0.12, bevel=0.01)
                wbox(p, 'Trim#spand', F, ua + 0.12, ub - 0.12, zf + 0.06, wz0 - 0.1, 0.0, 0.015, bevel=0.0)
    # as quinas (os pilares grossos das 4 quinas, de baldes próprios)
    for (sx, sy) in ((1, 1), (1, -1), (-1, 1), (-1, -1)):
        x, y = CX + sx * hx, CY + sy * hy
        p.box('White#corner%d' % k, x - 0.42, x + 0.42, y - 0.42, y + 0.42, z0 - 0.05, z1 + 0.02, bevel=0.05)
    # a cornija em 2 fiadas (as do x correm inteiras; as do y entre elas, noutro balde: nada de vértice coincidente)
    # (revisão: as 2 fiadas no mesmo balde soldavam vértice com vértice, NONMANIFOLD; cada fiada no seu balde)
    for c_, (h0, h1, o) in enumerate(((z1 - 0.02, z1 + 0.22, 0.28), (z1 + 0.22, z1 + 0.36, 0.4))):
        p.box('White#cornx%d%d' % (k, c_), x0 - o, x1 + o, y1 - 0.4, y1 + o, h0, h1, bevel=0.04)
        p.box('White#cornx%d%d' % (k, c_), x0 - o, x1 + o, y0 - o, y0 + 0.4, h0, h1, bevel=0.04)
        p.box('White#corny%d%d' % (k, c_), x1 - 0.4, x1 + o, y0 + 0.403, y1 - 0.403, h0, h1, bevel=0.04)
        p.box('White#corny%d%d' % (k, c_), x0 - o, x0 + 0.4, y0 + 0.403, y1 - 0.403, h0, h1, bevel=0.04)
    # a mureta com os pilaretes (o ritmo dos pilares continua acima da cornija)
    zp = z1 + 0.36
    t = 0.32
    p.box('Concrete#parx%d' % k, x0 - 0.1, x1 + 0.1, y1 - t, y1 + 0.1, zp, zp + 0.6, bevel=0.04)
    p.box('Concrete#parx%d' % k, x0 - 0.1, x1 + 0.1, y0 - 0.1, y0 + t, zp, zp + 0.6, bevel=0.04)
    p.box('Concrete#pary%d' % k, x1 - t, x1 + 0.1, y0 + t + 0.003, y1 - t - 0.003, zp, zp + 0.6, bevel=0.04)
    p.box('Concrete#pary%d' % k, x0 - 0.1, x0 + t, y0 + t + 0.003, y1 - t - 0.003, zp, zp + 0.6, bevel=0.04)
    for (xa, xb, ya, yb) in ((x0, x1, y1, y1), (x0, x1, y0, y0)):
        for x in splits(x0 + 1.2, x1 - 1.2, 1.55):
            p.box('White#post%d' % k, x - 0.17, x + 0.17, ya - 0.17, ya + 0.17, zp + 0.6, zp + 0.95, bevel=0.03)
    for xx in (x0, x1):
        for y in splits(y0 + 1.2, y1 - 1.2, 1.55):
            p.box('White#post%d' % k, xx - 0.17, xx + 0.17, y - 0.17, y + 0.17, zp + 0.6, zp + 0.95, bevel=0.03)
    # o telhado verde (a chapa da laje entre as muretas) e os 4 obeliscos nas quinas
    p.box('Roof#roof%d' % k, x0 + t - 0.02, x1 - t + 0.02, y0 + t - 0.02, y1 - t + 0.02, z1 - 0.1, z1 + 0.42, bevel=0.0)
    for (sx, sy) in ((1, 1), (1, -1), (-1, 1), (-1, -1)):
        obelisk(p, g, CX + sx * (hx - 0.05), CY + sy * (hy - 0.05), zp + 0.6, h=2.2 - 0.4 * k, b=0.62 - 0.08 * k)


def front_portal(p):
    """O portal da frente do 1º degrau: o pórtico de 4 colunas brancas com o frontão reto, a porta alta de vidro, as 2 faixas verticais
    da cor do jogador penduradas nas varas dos 2 lados."""
    hx, hy, z0, z1, _ = TIERS[0]
    xf = CX + hx
    F = WallFace('+x', xf, 0.0)
    # a porta (o vidro alto com o caixilho) e o fundo
    wbox(p, 'Glass#door', F, -1.2, 1.2, z0, z0 + 3.4, 0.0, 0.02, bevel=0.0)
    for u in (-1.2, -0.4, 0.4, 1.2):
        wbox(p, 'White#dframe', F, u - 0.05, u + 0.05, z0, z0 + 3.4, 0.0, 0.08, bevel=0.0)
    for z in (z0 + 2.4, z0 + 3.35):
        wbox(p, 'White#dframe', F, -1.2, 1.2, z - 0.05, z + 0.05, 0.0, 0.08, bevel=0.0)
    wbox(p, 'Glass#door', F, -1.2, 1.2, z0 + 3.6, z1 - 0.6, 0.0, 0.02, bevel=0.0)                 # a janela alta do salão
    for u in (-1.2, 0.0, 1.2):
        wbox(p, 'White#dframe', F, u - 0.05, u + 0.05, z0 + 3.55, z1 - 0.55, 0.0, 0.08, bevel=0.0)
    # o pórtico: 4 colunas redondas com a base e o capitel, a viga e o frontão reto (a estrela fica no topo da agulha)
    for u in (-1.75, -0.6, 0.6, 1.75):
        p.cyl('White#col', (xf + 1.2, u, z0 + 2.4), 'z', 0.24, 4.6, seg=20, bevel=0.0, smooth=True)
        p.box('White#colb', xf + 0.92, xf + 1.48, u - 0.28, u + 0.28, z0, z0 + 0.22, bevel=0.03)
        p.box('White#colc', xf + 0.92, xf + 1.48, u - 0.28, u + 0.28, z0 + 4.6, z0 + 4.78, bevel=0.03)
    p.box('White#entab', xf - 0.05, xf + 1.6, -2.2, 2.2, z0 + 4.78, z0 + 5.3, bevel=0.05)
    p.box('White#entab2', xf - 0.05, xf + 1.7, -2.3, 2.3, z0 + 5.3, z0 + 5.42, bevel=0.03)
    p.box('Concrete#stepsP', xf + 0.4, xf + 2.0, -2.4, 2.4, z0 - 0.02, z0 + 0.12, bevel=0.03)
    stencil(p, WallFace('+x', xf + 1.6, 0.0), 0.0, z0 + 4.88, 'ГЛЯЖИ', h=0.3, m='Red', n=0.0)
    # as 2 faixas verticais da cor do jogador (o pano pendurado na vara, a barra pesada embaixo, a ondulação leve)
    for y in (-3.1, 3.1):
        p.cyl('Metal#rod', (xf + 0.32, y, z1 - 0.7), 'y', 0.04, 1.7, seg=8, bevel=0.0)
        for s in (-1, 1):
            p.box('Metal#rodb', xf - 0.02, xf + 0.34, y + s * 0.82 - 0.03, y + s * 0.82 + 0.03, z1 - 0.74, z1 - 0.66, bevel=0.0)
        zt_, zb_ = z1 - 0.72, z0 + 1.2
        rows = []
        nz_, ny_ = 16, 8
        b = p.bm('Team#banner', 0.0)
        from model_lib import V
        top_, bot_ = [], []
        for j in range(nz_ + 1):
            v = j / nz_
            z = zt_ + (zb_ - zt_) * v
            row = []
            for i in range(ny_ + 1):
                u = i / ny_
                yy = y - 0.78 + 1.56 * u
                xx = xf + 0.3 + 0.05 * math.sin(u * 9.0 + v * 2.0) * (0.3 + v)
                row.append((xx, yy, z))
            rows.append(row)
        vf = [[b.verts.new(V(x + 0.008, yy, z)) for (x, yy, z) in row] for row in rows]
        vb = [[b.verts.new(V(x - 0.008, yy, z)) for (x, yy, z) in row] for row in rows]
        for j in range(nz_):
            for i in range(ny_):
                b.faces.new([vf[j][i], vf[j][i + 1], vf[j + 1][i + 1], vf[j + 1][i]])
                b.faces.new([vb[j + 1][i], vb[j + 1][i + 1], vb[j][i + 1], vb[j][i]])
        edge = [(0, i) for i in range(ny_)] + [(j, ny_) for j in range(nz_)] + [(nz_, i) for i in range(ny_, 0, -1)] + \
               [(j, 0) for j in range(nz_, 0, -1)]
        for (j0, i0), (j1, i1) in zip(edge, edge[1:] + edge[:1]):
            b.faces.new([vf[j0][i0], vb[j0][i0], vb[j1][i1], vf[j1][i1]])
        tube(p, 'Metal#hem', [(rows[-1][i][0], rows[-1][i][1], zb_ - 0.02) for i in range(ny_ + 1)], 0.035, seg=6)


def crown(p, g):
    """O alto: o tambor octogonal com as janelas altas e as pilastras, a cornija, a base de pedra da agulha com as nervuras, a AGULHA
    DOURADA com os anéis, o globo e a ESTRELA (g: a peça; o ouro no slot Gold)."""
    za, zb = DRUM_Z
    R = DRUM_R
    revolve(p, 'White#drumb', (CX, CY), [(R + 0.25, za - 0.05), (R + 0.25, za + 0.35), (R, za + 0.35)], seg=8, smooth=False)
    revolve(p, 'Concrete#drum', (CX, CY), [(R, za + 0.3), (R, zb - 0.3)], seg=8, smooth=False, caps=(False, False))
    revolve(p, 'White#drumc', (CX, CY), [(R - 0.02, zb - 0.32), (R + 0.3, zb - 0.32), (R + 0.3, zb - 0.1), (R + 0.15, zb), (0.9, zb)],
            seg=8, smooth=False)
    for k in range(8):
        a = 2 * math.pi * k / 8
        c, s_ = math.cos(a), math.sin(a)
        # as pilastras nas quinas do octógono e as janelas altas (escuras) no meio de cada face
        p.obox('White#pil', (CX + (R + 0.05) * c, CY + (R + 0.05) * s_, (za + zb) / 2), (0.24, 0.24, zb - za - 0.6),
               yaw=math.degrees(a), bevel=0.02)
        am = a + math.pi / 8
        rm = R * math.cos(math.pi / 8)
        p.obox('Dark#dw', (CX + (rm + 0.01) * math.cos(am), CY + (rm + 0.01) * math.sin(am), (za + zb) / 2 + 0.05), (0.04, 0.42, 0.85),
               yaw=math.degrees(am), bevel=0.0)
    # a base de pedra da agulha: o octógono afinando com as nervuras (8), o anel de cima
    z0, z1, z2 = SPIRE_Z
    revolve(p, 'White#spb', (CX, CY), [(1.15, z0 - 0.02), (1.05, z0 + 0.6), (0.8, z1 - 0.2), (0.62, z1)], seg=8, smooth=False)
    for k in range(8):
        a = 2 * math.pi * k / 8
        pts = [(CX + rr * math.cos(a), CY + rr * math.sin(a), zz) for (rr, zz) in ((1.2, z0), (1.1, z0 + 0.6), (0.85, z1 - 0.2),
                                                                              (0.66, z1 - 0.02))]
        tube(p, 'White#rib', pts, 0.05, seg=6)
    # a agulha de ouro (8 faces, os 3 anéis) e o globo
    revolve(g, 'Gold#needle', (CX, CY), [(0.6, z1 - 0.04), (0.5, z1 + 0.2), (0.32, z1 + 1.6), (0.16, z1 + 3.0), (0.05, z2), (0.02, z2 + 0.1)],
            seg=8, smooth=False)
    for (rr, zz) in ((0.56, z1 + 0.1), (0.4, z1 + 0.95), (0.25, z1 + 2.1)):
        vp.lathe(g, 'Gold#ring', (CX, CY, zz), 'z', [(rr - 0.02, -0.05), (rr + 0.05, -0.04), (rr + 0.06, 0.0), (rr + 0.05, 0.04),
                                                     (rr - 0.02, 0.05)], seg=16, closed=True)
    vp.lathe(g, 'Gold#orb', (CX, CY, z2 + 0.05), 'z', [(0.0, 0.0), (0.12, 0.03), (0.21, 0.2), (0.12, 0.38), (0.03, 0.42), (0.0, 0.42)],
             seg=18)
    spire_star(g, STAR_C, STAR_R)


# ===================================================================================================== a estrela da agulha
def spire_star(p, c, r, d=0.1):
    """A ESTRELA do topo da agulha (a do Kremlin em cima das Sete Irmãs, com o emblema aprovado de 01/11: a vermelha com as bordas
    branca, azul e vermelha, a das Forças Armadas): o miolo vermelho com a espessura d e as camadas do ru_star nas 2 faces (±X), a
    ponta pra cima; r = o raio da estrela vermelha do meio (a de fora tem 1,53 r)."""
    from model_faction_kit import star_points
    cx, cy, cz = c
    R = r * 1.53
    pts = star_points(0.0, 0.0, R, R * 0.42, rot_deg=90.0)
    vp.prism(p, 'Red#score', [(u, v) for (u, v) in pts], Vector((cx - d / 2, cy, cz)), Vector((0, 1, 0)), Vector((0, 0, 1)),
             Vector((1, 0, 0)), d)
    for face, at in (('+x', cx + d / 2), ('-x', cx - d / 2)):
        ru_star(p, WallFace(face, at, 0.0), cy, cz, r, n=0.0, t=0.03)


# ===================================================================================================== o terraço
def terrace(p):
    """O equipamento do bunker: as parabólicas, o mastro treliçado, as antenas de chicote, os exaustores do gerador, as escotilhas, o
    pedestal do radar, as bandeiras."""
    z = RZ
    # o pedestal do radar (a peça _Radar gira em cima dele)
    rx, ry, rz = RADAR_AT
    p.box('Concrete#rped', rx - 0.9, rx + 0.9, ry - 0.9, ry + 0.9, z - 0.05, z + 0.55, bevel=0.06)
    p.cyl('Metal#rcol', (rx, ry, z + 0.8), 'z', 0.32, 0.5, seg=20, bevel=0.0)
    p.box('Paint#rbox', rx - 0.8, rx - 0.15, ry - 1.5, ry - 0.95, z, z + 0.85, bevel=0.03)
    # as 2 parabólicas (a boca virada pro céu do sul, inclinadas)
    for (dx, dy, R, yaw) in ((-4.4, -9.4, 1.0, 200.0), (-7.6, -8.6, 0.75, 160.0)):
        p.box('Concrete#dped', dx - 0.45, dx + 0.45, dy - 0.45, dy + 0.45, z - 0.05, z + 0.35, bevel=0.04)
        p.cyl('Metal#dcol', (dx, dy, z + 0.75), 'z', 0.08, 0.8, seg=10, bevel=0.0)
        ex, ey, ez = vp.ypr_frame(yaw, -50.0, 0.0)
        cen = Vector((dx, dy, z + 1.2 + R * 0.3))
        vp.lathe(p, 'White#dish', cen, ex, [(0.0, -0.05), (R * 0.5, 0.0), (R * 0.85, 0.09), (R, 0.15), (R + 0.02, 0.13), (R * 0.85, 0.05),
                                           (R * 0.5, -0.04), (0.06, -0.09), (0.0, -0.09)], seg=28)
        for k in range(3):
            a = 2 * math.pi * k / 3
            tip = cen + ex * (R * 0.75)
            q = cen + (ey * math.cos(a) + ez * math.sin(a)) * (R * 0.85) + ex * 0.13
            tube(p, 'Metal#feed', [tuple(q), tuple(tip)], 0.018, seg=5)
        p.cyl('Dark#feed', tuple(cen + ex * (R * 0.78)), tuple(ex), 0.06, 0.14, seg=10, bevel=0.0)
    # o mastro treliçado com os dipolos (atrás, no meio) e as antenas de chicote
    lattice_mast(p, (-10.4, 0.0), z, z + 9.5, 0.7, 0.22, 6)
    for k, zz in enumerate((z + 5.0, z + 6.8, z + 8.6)):
        for sg in (-1, 1):
            tube(p, 'Metal#dip', [(-10.4, 0.0, zz), (-10.4, sg * (1.3 - 0.25 * k), zz)], 0.03, seg=6)
            tube(p, 'White#dip', [(-10.4, sg * (1.3 - 0.25 * k), zz - 0.5), (-10.4, sg * (1.3 - 0.25 * k), zz + 0.5)], 0.025, seg=6)
    p.cyl('Beacon', (-10.4, 0.0, z + 9.65), 'z', 0.08, 0.18, seg=12, bevel=0.0, smooth=True)
    for (ax, ay, hh) in ((-9.8, -6.6, 5.0), (-9.4, 6.8, 4.2), (6.6, -10.0, 3.8), (-1.0, 9.8, 4.6)):
        vp.lathe(p, 'Metal#whipb', (ax, ay, z), 'z', [(0.0, 0.0), (0.12, 0.0), (0.12, 0.12), (0.06, 0.16), (0.05, 0.4), (0.0, 0.42)], seg=12)
        p.cyl('Dark#whip', (ax, ay, z + 0.42 + hh / 2), 'z', 0.015, hh, seg=6, bevel=0.0)
    # os 2 exaustores do gerador (atrás, à direita) e as saídas de ventilação, as escotilhas
    for y in (3.6, 5.0):
        p.cyl('Metal#stack', (-10.6, y, z + 0.9), 'z', 0.24, 1.8, seg=16, bevel=0.0)
        p.cyl('Dark', (-10.6, y, z + 1.81), 'z', 0.2, 0.02, seg=16, bevel=0.0)
        p.cyl('Metal#stackc', (-10.6, y, z + 1.7), 'z', 0.3, 0.08, seg=16, bevel=0.0)
    for (x, y) in ((2.0, -9.6), (5.4, 7.8), (-10.6, -3.6)):
        vent_shaft(p, x, y, z, sx=1.0, sy=0.8, h=0.75)
    for (x, y) in ((-0.2, -9.4), (-0.2, 9.4)):
        ac_box(p, x, y, z, yaw=90)
    for (x, y) in ((6.4, 4.0), (-9.8, -9.4)):
        p.cyl('Concrete#hatchb', (x, y, z + 0.2), 'z', 0.65, 0.4, seg=24, bevel=0.0)
        vp.hatch_round(p, (x, y, z + 0.4), 0.48, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
    # o periscópio de observação (a cúpula baixa com as fendas) no canto da frente esquerda
    p.cyl('Concrete#cup', (4.8, -8.8, z + 0.35), 'z', 0.9, 0.7, seg=24, bevel=0.0)
    p.cyl('Dark#slit', (4.8, -8.8, z + 0.55), 'z', 0.905, 0.12, seg=24, bevel=0.0)
    vp.lathe(p, 'Concrete#cupt', (4.8, -8.8, z + 0.7), 'z', [(0.0, 0.25), (0.6, 0.2), (0.95, 0.04), (0.95, 0.0), (0.0, 0.0)], seg=24)
    # as 2 bandeiras da cor do jogador nos cantos da frente do terraço e a luz de rodapé
    for y in (-8.2, 8.2):
        flag_pole(p, 7.0, y, z, 6.5, cloth=(1.9, 1.25), phase=0.4 if y > 0 else 0.0)


# ===================================================================================================== o radar que gira
def radar():
    """A antena de busca moderna (o refletor de treliça em calha parabólica, como o radar de vigilância dos bunkers): a mesa
    giratória, o braço, o refletor curvo de barras (lê como grade curva escura), a corneta na frente, a caixa do receptor atrás e a luz.
    Origem no eixo, no topo do pedestal; a frente do refletor pra +X em repouso."""
    p = RuPart(PREFIX + '_Radar', cull=False)
    p.cyl('Metal#turn', (0.0, 0.0, 0.1), 'z', 0.45, 0.2, seg=24, bevel=0.0)
    p.cyl('Dark', (0.0, 0.0, 0.21), 'z', 0.36, 0.03, seg=24, bevel=0.0)
    p.cyl('Paint#mast', (0.0, 0.0, 0.55), 'z', 0.14, 0.7, seg=12, bevel=0.0)
    p.box('Paint#box', -0.95, -0.3, -0.38, 0.38, 0.25, 0.85, bevel=0.04)
    zc, W, H, f = 1.6, 2.2, 0.85, 0.6                # (revisão: maior; é a peça que gira, a 2ª leitura do prédio)
    # as barras da calha (horizontais, ao longo de y) na parábola x = (z - zc)² / (4 f) − recuo
    def px(z):
        return (z - zc) ** 2 / (4 * f) - 0.15
    nz = 9
    for k in range(nz):
        z = zc - H + 2 * H * k / (nz - 1)
        tube(p, 'Metal#bar', [(px(z), -W, z), (px(z), W, z)], 0.03, seg=6)
    for y in (-W, -W / 2, 0.0, W / 2, W):
        pts = [(px(zc - H + 2 * H * k / 12), y, zc - H + 2 * H * k / 12) for k in range(13)]
        tube(p, 'Paint#rib', pts, 0.045 if abs(y) in (0.0, W) else 0.035, seg=6)
    # o braço do mastro até o refletor, a corneta no foco e os 2 tirantes
    tube(p, 'Paint#arm', [(0.0, 0.0, 0.85), (0.0, 0.0, zc - 0.3), (px(zc) + 0.02, 0.0, zc)], 0.07, seg=8)
    tube(p, 'Paint#arm', [(px(zc), 0.0, zc), (f, 0.0, zc)], 0.04, seg=6)
    for sg in (-1, 1):
        tube(p, 'Metal#stay', [(px(zc + sg * H * 0.8), 0.0, zc + sg * H * 0.8), (f, 0.0, zc)], 0.02, seg=5)
    p.box('Paint#horn', f - 0.05, f + 0.2, -0.16, 0.16, zc - 0.12, zc + 0.12, bevel=0.02)
    p.box('Dark', f + 0.2, f + 0.21, -0.12, 0.12, zc - 0.09, zc + 0.09, bevel=0.0)
    p.cyl('Beacon', (px(zc + H) - 0.02, 0.0, zc + H + 0.12), 'z', 0.07, 0.16, seg=12, bevel=0.0, smooth=True)
    return p.finish()


# ===================================================================================================== montagem
def base():
    p = RuPart(PREFIX + '_Base')
    g = p                                   # o ouro (slot Gold) vai na mesma peça: o RuPart não troca o Gold
    bunker(p)
    portal(p)
    side_features(p)
    for k in range(len(TIERS)):
        tier(p, g, k)
    front_portal(p)
    crown(p, g)
    terrace(p)
    return p.finish()


def write_preview():
    import json
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]], [PREFIX + '_Radar', [RADAR_AT[0], -RADAR_AT[1], RADAR_AT[2]]]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1400, 1400]], ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1600, 1100]],
                      ['left', 'ortho', [0, -1, 0], [0, 0, 1], [1600, 1100]],
                      ['three_quarter', 'persp', 34.0, 215.0, [1600, 1100], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 32.0, 35.0, [1600, 1100], 30.0, 0.95]],
            'usa_at': [15.6, 6.0, 0.0], 'tank_views': True,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, indent=1)


def main():
    os.makedirs(OUT, exist_ok=True)
    clear_scene()
    total = export(base(), OUT)
    clear_scene()
    total += export(bake_in_place(radar(), RADAR_AT), OUT)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -HX + 1.9, HX - 2.6, -HY + 2.2, HY - 2.2, TIERS[1][3] + 0.6, post='Metal', plank='Wood',
                             stripe='Hazard'), OUT)
    write_preview()
    print('MODEL total', total, 'tris')
    print('MODEL lote 26 x 28 m; radar at_m', tuple(round(v, 2) for v in RADAR_AT), 'rpm 6; portão do trator em X', PORTAL[1],
          'Y', -PORTAL[2], '..', PORTAL[2])
    print('MODEL alturas: bunker', CZ, 'degraus', [t[3] for t in TIERS], 'tambor', DRUM_Z[1], 'agulha', SPIRE_Z[2], 'estrela até',
          round(STAR_C[2] + STAR_R * 1.53, 2))
    print('MODEL fire_points_m', [(-8.0, -7.5, RZ + 0.1), (2.0, 5.0, TIERS[0][3] + 0.5), (0.4, -3.6, TIERS[1][3] + 0.5)])


if __name__ == '__main__':
    main()
