"""O Strategy Center da USA provisório, modelado no Blender por script (sessão Modelos 3D, 22/10/2026).

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_strategy_center.py -- <pasta de saída>

O prédio de comando e tecnologia da USA (economia v4): pesquisa as melhorias e escolhe o "plano de batalha" que vale
pro exército todo. Provisório (o Felipe: "contra a arte final TODOS horríveis"): limpo, legível de cima, coerente
com a facção; a arte de verdade vem depois pela receita do docs/CONCEITO_CONSTRUCOES.md §4.

O desenho (a fórmula do §1 e o estudo do §3b):
- a palavra é A ANTENA DE SATÉLITE: o prato grande (8,4 m) apontado pro céu, girando devagar em cima de um bunker
  octogonal blindado. É o botão do HUD (docs/concept/hud/kit/photos/buttons/usa_tech_center.png: o prato branco
  grande no mastro, sobre o bloco baixo) e a regra 2 do estudo: a assinatura é redonda num mundo de retângulos (de
  cima, um círculo branco grande sobre o octógono). Diferente do Command Center, que tem o radar de busca virado
  pra frente numa torre treliçada: este é o uplink, quase deitado de cara pro céu;
- a linguagem da USA: painel cinza-claro, bunker de volumes retos com a saia inclinada (blindagem), a faixa azul
  (cor do jogador) nas faces e as lonas azuis amarradas no teto, o portão blindado com a faixa de perigo, as luzes
  de serviço, a bandeira no mastro; nada de neon;
- vida: o prato gira (peça spin, para sem energia), a sirene do subrefletor e as balizas do teto piscam;
- assentado no chão: só a borda de fundação do lote e a laje octogonal; o gerador e o tanque nos cantos de trás.

Lote 20 × 20 m (5 × 5 quadrados da grade de 4 m; entre o Supply Center 23 × 24 e a usina 12 × 16, abaixo do CC 32 ×
37: a hierarquia do §3b), frente em +X (o portão; o mesmo "Dir 0" dos prédios do jogo). Alturas: o bunker 4,8 m (esconde
6 m atrás), o prato até ~12,1 m (peça redonda e inclinada, o limite do docs/PRODUCAO_3D.md §3).

Peças (origem no chão, no centro do lote; coordenadas da Unreal, X frente, Y direita):
  SM_USA_StrategyCenter_Base (hull): fundação, laje, bunker, portão, teto (base do giro, lonas, ventilação, antenas),
    gerador, tanque, mastro da bandeira, poste de luz;
  SM_USA_StrategyCenter_Dish (spin; origem no eixo, at_m = DISH_AT, ~3 rpm, para sem energia): o prato, o garfo e a
    mesa giratória;
  SM_USA_StrategyCenter_Scaffold: o andaime da obra (octogonal, aberto no portão).
Materiais (os slots das construções, import_provisional.py): Body, Trim, Dark, Player, Concrete, Hazard, Glass, Light,
Beacon, Yellow, Paint.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, flag  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_strategy_center')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_USA_StrategyCenter'

COLORS.update({'Body': (190, 190, 184), 'Trim': (70, 70, 68), 'Dark': (40, 40, 38), 'Player': (86, 108, 150),
               'Concrete': (150, 146, 136), 'Hazard': (214, 170, 30), 'Glass': (30, 50, 80), 'Light': (255, 230, 170),
               'Beacon': (255, 170, 40), 'Yellow': (214, 164, 32), 'Paint': (222, 222, 216), 'Crate': (112, 100, 62)})

HX = HY = 10.0                   # o lote 20 × 20
SLAB_A = 8.8                     # a laje octogonal (apótema: do centro ao meio da face)
GLACIS_A, WALL_A = 7.2, 6.5      # o bunker: a saia inclinada no chão e a parede reta
Z0 = 0.14                        # o topo da laje
GLACIS_Z, WALL_Z = 1.5, 4.35     # a saia vai até 1,5 m; a parede até 4,35 m
ROOF_Z = 4.6                     # o piso do teto (dentro do parapeito)
DISH_AT = (0.0, 0.0, 5.8)        # o eixo do giro, em cima da base redonda do teto
OCT = 22.5                       # os cantos do octógono a 22,5° + k·45°: a face 0 olha pra +X (o portão)

# o prato (no referencial da peça Dish): o eixo de elevação A, 55° pra cima, parabólico com 8,4 m de diâmetro
EL = 55.0
AX = Vector((0.3, 0.0, 2.1))
R_DISH, F_DISH, OFF, THICK = 4.2, 3.2, 0.7, 0.12
N_DISH = Vector((math.cos(math.radians(EL)), 0.0, math.sin(math.radians(EL))))     # pra onde o prato olha
U_DISH = Vector((-math.sin(math.radians(EL)), 0.0, math.cos(math.radians(EL))))    # o "cima" do plano do prato
Y_DISH = Vector((0.0, 1.0, 0.0))


def corner_r(a):
    return a / math.cos(math.radians(OCT))


def octa_ring(a, z):
    r = corner_r(a)
    return [(r * math.cos(math.radians(OCT + 45 * k)), r * math.sin(math.radians(OCT + 45 * k)), z) for k in range(8)]


def octa(p, m, sections, bevel=0.04):
    """Prisma octogonal pelos anéis [(apótema, z)...] de baixo pra cima (saia inclinada, parede, tampa)."""
    p.loft(m, [octa_ring(a, z) for a, z in sections], bevel=bevel)


def octa_band(p, m, a_out, a_in, z0, z1, bevel=0.03):
    """Um anel octogonal (o parapeito): as faces de fora, de cima, de dentro e de baixo."""
    b = p.bm(m + '#band', bevel)
    rings = [[b.verts.new(V(*q)) for q in octa_ring(a, z)] for a, z in ((a_out, z0), (a_out, z1), (a_in, z1), (a_in, z0))]
    for i in range(4):
        ra, rb = rings[i], rings[(i + 1) % 4]
        for k in range(8):
            b.faces.new([ra[k], ra[(k + 1) % 8], rb[(k + 1) % 8], rb[k]])


def face_box(p, m, k, a, z0, z1, width, thick=0.03):
    """Uma placa fina encostada na face k do octógono de apótema a (a faixa azul, a junta escura)."""
    ang = 45.0 * k
    r = a + thick / 2
    p.obox(m, (r * math.cos(math.radians(ang)), r * math.sin(math.radians(ang)), (z0 + z1) / 2), (thick, width, z1 - z0), yaw=ang, bevel=0.0)


def tarp(p, c, yaw, L, W, z):
    """A lona azul (cor do jogador) amarrada sobre o equipamento do teto, com as 3 cintas escuras."""
    ca, sa = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    p.obox('Body', (c[0], c[1], z + 0.25), (L - 0.2, W - 0.2, 0.5), yaw=yaw, bevel=0.04)
    p.obox('Player', (c[0], c[1], z + 0.42), (L, W, 0.36), yaw=yaw, bevel=0.1)
    for t in (-0.3, 0.0, 0.3):
        p.obox('Dark', (c[0] + ca * L * t, c[1] + sa * L * t, z + 0.42), (0.1, W + 0.06, 0.42), yaw=yaw, bevel=0.0)


def scaffold_poly(name, pts, h, out=1.0, skip=(), post='Trim', plank='Crate', stripe='Hazard', bay=3.0, lift=2.0, z0=0.14):
    """O andaime da obra ao longo de um contorno qualquer (o do model_lib é retangular): postes, travessas por nível,
    tábuas, o rodapé de perigo por fora e a diagonal de travamento. skip = os lados sem andaime (o portão)."""
    p = Part(name)
    top = h + 1.0
    levels = max(2, int((top - z0) / lift))
    n = len(pts)
    cx, cy = sum(q[0] for q in pts) / n, sum(q[1] for q in pts) / n
    for i in range(n):
        if i in skip:
            continue
        (ax, ay), (bx, by) = pts[i], pts[(i + 1) % n]
        L = math.hypot(bx - ax, by - ay)
        dx, dy = (bx - ax) / L, (by - ay) / L
        ox, oy = dy, -dx
        if ox * ((ax + bx) / 2 - cx) + oy * ((ay + by) / 2 - cy) < 0:
            ox, oy = -ox, -oy
        yaw = math.degrees(math.atan2(dy, dx))
        m = max(1, int(L / bay))
        for k in range(m + 1):
            t = k / m
            px, py = ax + (bx - ax) * t, ay + (by - ay) * t
            for d in (0.0, out):
                p.cyl(post, (px + ox * d, py + oy * d, (z0 + top) / 2), 'z', 0.05, top - z0, seg=8, bevel=0.0)
        for lv in range(1, levels + 1):
            z = z0 + (top - z0) * lv / levels
            mx, my = (ax + bx) / 2, (ay + by) / 2
            for d in (0.0, out):
                p.cyl(post, (mx + ox * d, my + oy * d, z), (dx, dy, 0.0), 0.045, L, seg=8, bevel=0.0)
            p.obox(plank, (mx + ox * out / 2, my + oy * out / 2, z - 0.03), (L, abs(out) - 0.1, 0.06), yaw=yaw, bevel=0.01)
            p.obox(stripe, (mx + ox * out, my + oy * out, z + 0.09), (L, 0.04, 0.18), yaw=yaw, bevel=0.0)
        for k in range(m):
            t0, t1 = k / m, (k + 1) / m
            pa = Vector((ax + (bx - ax) * t0 + ox * out, ay + (by - ay) * t0 + oy * out, z0 + (top - z0) * ((k % levels) / levels)))
            pb = Vector((ax + (bx - ax) * t1 + ox * out, ay + (by - ay) * t1 + oy * out, z0 + (top - z0) * ((k % levels + 1) / levels)))
            d = pb - pa
            c = (pa + pb) / 2
            p.cyl(post, (c.x, c.y, c.z), (d.x, d.y, d.z), 0.035, d.length, seg=6, bevel=0.0)
    return p.finish()


def lamp_post(p, x, y, h=6.0):
    p.box('Concrete', x - 0.45, x + 0.45, y - 0.45, y + 0.45, -0.2, 0.3, bevel=0.04)
    p.box('Hazard', x - 0.3, x + 0.3, y - 0.3, y + 0.3, 0.3, 0.95, bevel=0.03)
    p.cyl('Trim', (x, y, (0.95 + h) / 2), 'z', 0.1, h - 0.95, seg=12, bevel=0.0)
    p.box('Dark', x - 0.4, x + 0.4, y - 0.25, y + 0.25, h, h + 0.3, bevel=0.03)
    p.box('Light', x - 0.32, x + 0.32, y - 0.18, y + 0.18, h - 0.06, h, bevel=0.0)


def base():
    p = Part(PREFIX + '_Base')
    # a borda de fundação do lote (só a borda: o prédio senta na areia) e a laje octogonal do bunker
    for (x0, x1, y0, y1) in ((-HX, HX, -HY, -HY + 0.4), (-HX, HX, HY - 0.4, HY), (-HX, -HX + 0.4, -HY + 0.4, HY - 0.4), (HX - 0.4, HX, -HY + 0.4, HY - 0.4)):
        p.box('Concrete', x0, x1, y0, y1, -0.3, 0.12, bevel=0.03)
    octa(p, 'Concrete', [(SLAB_A, -0.3), (SLAB_A, Z0)], bevel=0.03)
    # o bunker: a saia inclinada (a blindagem) e a parede reta, num prisma octogonal só
    octa(p, 'Body', [(GLACIS_A, Z0 - 0.05), (WALL_A, GLACIS_Z), (WALL_A, WALL_Z)], bevel=0.05)
    # o parapeito (um anel octogonal) e o piso do teto em concreto (é o que a câmera mais vê)
    octa_band(p, 'Trim', WALL_A + 0.1, WALL_A - 0.24, WALL_Z - 0.1, ROOF_Z + 0.38)
    octa(p, 'Concrete', [(WALL_A - 0.25, WALL_Z - 0.1), (WALL_A - 0.25, ROOF_Z)], bevel=0.0)
    # as faces: a faixa azul (cor do jogador) nas 7 faces sem portão, e as juntas escuras das placas
    for k in range(1, 8):
        face_box(p, 'Player', k, WALL_A, 3.15, 3.8, 4.3)
        face_box(p, 'Dark', k, WALL_A, 2.05, 2.13, 4.9, thick=0.02)
        face_box(p, 'Dark', k, WALL_A, 4.0, 4.06, 4.9, thick=0.02)
    for k in (2, 6):                                                                           # as luzes de serviço dos lados
        ang = math.radians(45 * k)
        r = WALL_A + 0.06
        p.obox('Trim', (r * math.cos(ang), r * math.sin(ang), 2.7), (0.12, 0.5, 0.2), yaw=45 * k, bevel=0.0)
        p.obox('Light', ((r + 0.065) * math.cos(ang), (r + 0.065) * math.sin(ang), 2.7), (0.02, 0.4, 0.12), yaw=45 * k, bevel=0.0)
    # o portão blindado na frente (+X): pilares, verga, o fundo escuro com as lâminas, a faixa de perigo nos pés,
    # as luzes e a placa azul em cima da porta
    PX0, PX1 = WALL_A - 0.3, 8.6
    for s in (-1, 1):
        p.box('Body', PX0, PX1, s * 1.9, s * 2.9, Z0, 4.0, bevel=0.06)
        p.box('Hazard', PX1, PX1 + 0.03, min(s * 1.95, s * 2.85), max(s * 1.95, s * 2.85), Z0, 1.1, bevel=0.0)
    p.box('Body', PX0, PX1, -1.9, 1.9, 3.1, 4.0, bevel=0.06)
    p.box('Body', PX0 - 0.1, PX1 + 0.1, -3.0, 3.0, 4.0, 4.18, bevel=0.03)
    p.box('Dark', PX1 + 0.1, PX1 + 0.12, -3.0, 3.0, 4.02, 4.1, bevel=0.0)
    p.box('Body', PX0, 7.85, -1.9, 1.9, Z0, 3.1, bevel=0.0)
    p.box('Dark', 7.85, 7.9, -1.85, 1.85, Z0, 3.05, bevel=0.0)                                  # a porta (recuada 70 cm)
    for k in range(6):
        z = Z0 + 0.3 + k * 0.48
        p.box('Trim', 7.9, 7.95, -1.8, 1.8, z, z + 0.08, bevel=0.0)
    p.box('Hazard', 7.9, PX1 + 0.15, -1.85, 1.85, Z0, Z0 + 0.025, bevel=0.0)                    # a soleira de perigo
    for y in (-1.45, 1.45):
        p.box('Dark', PX1, PX1 + 0.14, y - 0.32, y + 0.32, 3.3, 3.62, bevel=0.0)
        p.box('Light', PX1 + 0.14, PX1 + 0.16, y - 0.26, y + 0.26, 3.36, 3.56, bevel=0.0)
    p.box('Player', PX1, PX1 + 0.03, -0.9, 0.9, 3.28, 3.82, bevel=0.0)
    # o teto: a base redonda do giro (o anel escuro do trilho, o tambor), as lonas azuis, a ventilação, as antenas
    p.cyl('Dark', (0.0, 0.0, ROOF_Z + 0.02), 'z', 3.15, 0.04, seg=48, bevel=0.0)
    p.cyl('Trim', (0.0, 0.0, ROOF_Z + 0.18), 'z', 2.8, 0.36, seg=48, bevel=0.03)
    p.cyl('Body', (0.0, 0.0, (ROOF_Z + 0.36 + DISH_AT[2]) / 2), 'z', 2.35, DISH_AT[2] - ROOF_Z - 0.36, seg=48, bevel=0.03)
    p.cyl('Dark', (0.0, 0.0, DISH_AT[2] - 0.12), 'z', 2.37, 0.06, seg=48, bevel=0.0)
    for k in range(12):                                                                        # as nervuras do tambor
        a = math.radians(k * 30 + 15)
        p.obox('Trim', (2.37 * math.cos(a), 2.37 * math.sin(a), (ROOF_Z + 0.36 + DISH_AT[2] - 0.15) / 2), (0.1, 0.18, DISH_AT[2] - ROOF_Z - 0.55), yaw=k * 30 + 15, bevel=0.0)
    tarp(p, (-4.35 * math.cos(math.radians(25)), 4.35 * math.sin(math.radians(25)), 0), 90 - 25, 2.6, 1.7, ROOF_Z)
    tarp(p, (-4.35 * math.cos(math.radians(25)), -4.35 * math.sin(math.radians(25)), 0), -(90 - 25), 2.6, 1.7, ROOF_Z)
    vx, vy = 3.3, 3.3                                                                         # a ventilação (frente-direita)
    p.box('Body', vx - 0.9, vx + 0.9, vy - 0.7, vy + 0.7, ROOF_Z, ROOF_Z + 0.75, bevel=0.05)
    for k in range(5):
        x = vx - 0.64 + k * 0.32
        p.box('Dark', x - 0.07, x + 0.07, vy - 0.6, vy + 0.6, ROOF_Z + 0.75, ROOF_Z + 0.82, bevel=0.0)
    # o rack de rádio com as antenas (frente-esquerda), fora do giro do prato (a borda de baixo dele passa a 4,9 m do eixo)
    ax_, ay_ = 3.75, -3.75
    p.box('Body', ax_ - 0.55, ax_ + 0.55, ay_ - 0.55, ay_ + 0.55, ROOF_Z, ROOF_Z + 1.1, bevel=0.04)
    p.box('Dark', ax_ + 0.55, ax_ + 0.57, ay_ - 0.4, ay_ + 0.4, ROOF_Z + 0.2, ROOF_Z + 0.9, bevel=0.0)
    for (dx, dy, h) in ((0.3, 0.3, 3.6), (-0.3, -0.3, 2.6)):
        p.cyl('Trim', (ax_ + dx, ay_ + dy, ROOF_Z + 1.1 + h / 2), 'z', 0.035, h, seg=8, bevel=0.0)
    p.cyl('Beacon', (ax_ + 0.3, ay_ + 0.3, ROOF_Z + 1.1 + 3.6 + 0.08), 'z', 0.08, 0.16, seg=12, bevel=0.0, smooth=True)
    for k in (3, 4):                                                                           # as balizas dos cantos de trás
        r = corner_r(WALL_A - 0.15)
        a = math.radians(OCT + 45 * k)
        p.cyl('Dark', (r * math.cos(a), r * math.sin(a), ROOF_Z + 0.45), 'z', 0.09, 0.14, seg=12, bevel=0.0)
        p.cyl('Beacon', (r * math.cos(a), r * math.sin(a), ROOF_Z + 0.6), 'z', 0.07, 0.16, seg=12, bevel=0.0, smooth=True)
    # o gerador (canto de trás, esquerda): sapata, caixa com a faixa azul, a grelha e o escapamento; o conduíte pro bunker
    gx0, gx1, gy0, gy1 = -9.3, -6.7, -9.3, -7.3
    p.box('Concrete', gx0 - 0.2, gx1 + 0.2, gy0 - 0.2, gy1 + 0.2, -0.2, 0.2, bevel=0.03)
    p.box('Body', gx0, gx1, gy0, gy1, 0.2, 2.1, bevel=0.06)
    p.box('Player', gx0 + 0.15, gx1 - 0.15, gy1, gy1 + 0.03, 1.2, 1.6, bevel=0.0)
    p.box('Player', gx0 + 0.15, gx1 - 0.15, gy0 - 0.03, gy0, 1.2, 1.6, bevel=0.0)
    for k in range(6):
        p.obox('Dark', (gx1 + 0.02, (gy0 + gy1) / 2, 0.5 + k * 0.12), (0.03, 1.5, 0.05), roll=30, bevel=0.0)
    p.box('Trim', gx0 + 0.3, gx1 - 0.3, gy0 + 0.25, gy1 - 0.25, 2.1, 2.25, bevel=0.02)
    p.cyl('Dark', (gx0 + 0.45, gy0 + 0.45, 2.55), 'z', 0.1, 0.7, seg=10, bevel=0.0)
    p.obox('Dark', (-5.57, -6.87, 0.15), (3.3, 0.26, 0.3), yaw=45, bevel=0.0)                  # o conduíte, do gerador até a saia
    # o tanque de combustível deitado (canto de trás, direita), nos berços
    p.box('Concrete', -9.5, -6.9, 7.65, 9.35, -0.2, 0.2, bevel=0.03)
    for x in (-9.0, -7.4):
        p.box('Trim', x - 0.15, x + 0.15, 7.85, 9.15, 0.2, 0.75, bevel=0.02)
    p.cyl('Body', (-8.2, 8.5, 1.15), 'x', 0.62, 2.4, seg=24, bevel=0.03, smooth=True)
    p.cyl('Trim', (-6.98, 8.5, 1.15), 'x', 0.5, 0.06, seg=24, bevel=0.0)
    p.cyl('Player', (-8.2, 8.5, 1.15), 'x', 0.635, 0.4, seg=24, bevel=0.0, smooth=True)
    # o mastro com a bandeira (frente, esquerda) e o poste de luz (frente, direita)
    fx, fy = 8.6, -8.6
    p.box('Concrete', fx - 0.5, fx + 0.5, fy - 0.5, fy + 0.5, -0.2, 0.3, bevel=0.04)
    p.cyl('Trim', (fx, fy, 0.3 + 8.7 / 2), 'z', 0.08, 8.7, seg=12, bevel=0.0)
    p.cyl('Paint', (fx, fy, 9.08), 'z', 0.12, 0.16, seg=12, bevel=0.0, smooth=True)
    flag(p, (fx, fy), 8.9, 2.4, 1.45, m='Player')
    lamp_post(p, 8.6, 8.6)
    return p.finish()


def z_par(r):
    return r * r / (4 * F_DISH)


def dish_point(r, t, back=0.0):
    """Um ponto da superfície do prato: raio r, ângulo t (rad) no plano do prato; back = recuo pra trás (a espessura)."""
    return AX + N_DISH * (OFF + z_par(r) - back) + (U_DISH * math.cos(t) + Y_DISH * math.sin(t)) * r


def dish_annulus(p, key, r0, r1, nr, seg, thick):
    """Uma casca parabólica fechada entre os raios r0 e r1 (a frente, as costas e as duas bordas), lisa."""
    b = p.bm(key, 0.0)
    radii = [r0 + (r1 - r0) * i / nr for i in range(nr + 1)]
    front = [[b.verts.new(V(*dish_point(r, 2 * math.pi * k / seg))) for k in range(seg)] for r in radii]
    backv = [[b.verts.new(V(*dish_point(r, 2 * math.pi * k / seg, thick))) for k in range(seg)] for r in radii]
    for i in range(nr):
        for k in range(seg):
            j = (k + 1) % seg
            b.faces.new([front[i][k], front[i][j], front[i + 1][j], front[i + 1][k]]).smooth = True
            b.faces.new([backv[i + 1][k], backv[i + 1][j], backv[i][j], backv[i][k]]).smooth = True
    for ring_f, ring_b in ((front[0], backv[0]), (front[-1], backv[-1])):
        for k in range(seg):
            j = (k + 1) % seg
            b.faces.new([ring_f[k], ring_b[k], ring_b[j], ring_f[j]])


def dish():
    """O prato de satélite com a mesa giratória: origem no eixo (DISH_AT); o jogo gira em Z (spin, ~3 rpm)."""
    p = Part(PREFIX + '_Dish')
    # a mesa giratória, a caixa do azimute e o garfo com o eixo de elevação
    p.cyl('Trim', (0.0, 0.0, 0.15), 'z', 2.1, 0.3, seg=48, bevel=0.02)
    p.box('Body', -1.3, 1.0, -1.0, 1.0, 0.3, 1.3, bevel=0.06)
    p.box('Player', -1.31, -1.28, -0.8, 0.8, 0.65, 0.95, bevel=0.0)
    for s in (-1, 1):
        p.prism_xz('Body', [(-0.7, 1.3), (0.9, 1.3), (AX.x + 0.4, AX.z + 0.1), (AX.x - 0.4, AX.z + 0.1)], s * 1.05, s * 1.4, bevel=0.04)
        p.cyl('Trim', (AX.x, s * 1.47, AX.z), 'y', 0.36, 0.14, seg=20, bevel=0.01)
        p.cyl('Dark', (AX.x, s * 1.55, AX.z), 'y', 0.16, 0.05, seg=12, bevel=0.0)
    p.cyl('Trim', (AX.x, 0.0, AX.z), 'y', 0.3, 2.2, seg=16, bevel=0.0)
    p.box('Dark', 1.0, 1.04, -0.6, 0.6, 0.5, 1.1, bevel=0.0)                                    # a porta de serviço
    # o cubo atrás do prato e a corneta do alimentador
    p.cyl('Dark', tuple(AX + N_DISH * 0.42), tuple(N_DISH), 0.75, 0.74, seg=24, bevel=0.02)
    p.cyl('Trim', tuple(AX + N_DISH * (OFF + 0.6)), tuple(N_DISH), 0.32, 1.1, seg=16, r2=0.18, bevel=0.0)
    # a casca do prato (branca, lisa) e o aro escuro
    dish_annulus(p, 'Paint#dish', 0.6, R_DISH, 9, 64, THICK)
    dish_annulus(p, 'Trim#rim', R_DISH, R_DISH + 0.14, 1, 64, THICK + 0.06)
    # as nervuras atrás do prato (8, seguindo a curva) e o anel de travamento
    for k in range(8):
        t = 2 * math.pi * (k + 0.5) / 8
        tv = -U_DISH * math.sin(t) + Y_DISH * math.cos(t)
        rings = []
        for i in range(7):
            r = 0.7 + (R_DISH * 0.94 - 0.7) * i / 6
            q = dish_point(r, t, THICK + 0.11)
            h = 0.11 * (1.0 - 0.5 * i / 6)
            rings.append([tuple(q - tv * 0.06 - N_DISH * h), tuple(q + tv * 0.06 - N_DISH * h),
                          tuple(q + tv * 0.06 + N_DISH * 0.11), tuple(q - tv * 0.06 + N_DISH * 0.11)])
        p.loft('Dark', rings, bevel=0.0)
    # o subrefletor no foco, os 4 braços do aro até ele, e a sirene em cima
    F = AX + N_DISH * (OFF + F_DISH)
    p.cyl('Body', tuple(F), tuple(N_DISH), 0.55, 0.16, seg=24, bevel=0.01)
    p.cyl('Dark', tuple(F - N_DISH * 0.14), tuple(N_DISH), 0.3, 0.14, seg=16, bevel=0.0)
    for k in range(4):
        t = 2 * math.pi * (k + 0.5) / 4
        q0 = dish_point(R_DISH * 0.93, t, -0.02)
        q1 = F - N_DISH * 0.1 + (U_DISH * math.cos(t) + Y_DISH * math.sin(t)) * 0.35
        d = q1 - q0
        p.cyl('Trim', tuple((q0 + q1) / 2), tuple(d), 0.05, d.length, seg=8, bevel=0.0)
    p.cyl('Beacon', tuple(F + N_DISH * 0.16), tuple(N_DISH), 0.09, 0.16, seg=12, bevel=0.0, smooth=True)
    return p.finish()


def main():
    clear_scene()
    total = export(base(), OUT)
    clear_scene()
    total += export(dish(), OUT)
    clear_scene()
    pts = [(corner_r(GLACIS_A + 0.3) * math.cos(math.radians(OCT + 45 * k)), corner_r(GLACIS_A + 0.3) * math.sin(math.radians(OCT + 45 * k))) for k in range(8)]
    total += export(scaffold_poly(PREFIX + '_Scaffold', pts, WALL_Z + 0.4, out=1.0, skip=(7,)), OUT)
    print('MODEL total', total, 'tris')
    top = DISH_AT[2] + max((dish_point(R_DISH + 0.14, t)).z for t in [2 * math.pi * k / 64 for k in range(64)])
    print('MODEL lote 20 x 20 m; bunker', WALL_Z + 0.38, 'm; prato ate', round(top, 2), 'm; spin dish at_m', DISH_AT, 'rpm 3')


main()
