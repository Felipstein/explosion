"""O Supply Drop Zone da USA provisório, modelado no Blender por script (sessão Modelos 3D, 22/10/2026).

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_drop_zone.py -- <pasta de saída>

A estrutura que recebe dinheiro com o tempo (economia v4; raiz no Supply Drop Zone do ZH): o avião de carga passa a
cada 2 min e solta as caixas de paraquedas no pátio dela (model_supply_crate.py, model_cargo_plane.py). Provisório
(o Felipe: "contra a arte final TODOS horríveis"): limpo, legível de cima, coerente com a facção.

O desenho (a fórmula do docs/CONCEITO_CONSTRUCOES.md §1 e o estudo do §3b):
- a palavra é O ALVO DE LANÇAMENTO: o pátio de concreto com o X branco grande dentro do anel amarelo e as cantoneiras
  de perigo nos cantos, a "zona" que se lê de cima num relance (o botão do HUD, docs/concept/hud/kit/photos/buttons/
  usa_drop_zone.png, é o avião passando baixo sobre o pátio). O MEIO FICA LIVRE: é onde as caixas caem;
- a função na forma: a torre de controle pequena num canto (o radar de aproximação girando), a pilha das caixas
  recebidas no canto de trás (a mesma caixa que cai, olive com a faixa amarela) e a carga coberta com a lona azul,
  o mastro de luz com os holofotes virados pro alvo e a sirene, a biruta (o avião lê o vento) e as luzes de pista
  em volta do pátio;
- a linguagem da USA: concreto claro, metal cinza-claro, a faixa azul (cor do jogador) na torre e a lona azul;
- vida: o radar gira (peça spin, para sem energia), a sirene e a baliza da torre piscam, as luzes de pista acesas;
- assentado no chão: só a borda de fundação do lote e a laje do pátio.

Lote 16 × 16 m (4 × 4 quadrados da grade de 4 m: o pedido era ~14 × 14, arredondado pra grade, docs/PRODUCAO_3D.md
§2), o pátio 10 × 10 m no centro (o anel do alvo com 7,1 m). Frente em +X (sem porta: o avião chega de qualquer lado).
Baixo: a pilha 1,9 m, a torre 7,3 m, o mastro de luz 7,3 m (peças finas, docs/PRODUCAO_3D.md §3).

Peças (origem no chão, no centro do lote; coordenadas da Unreal, X frente, Y direita):
  SM_USA_DropZone_Base (hull): fundação, pátio com as marcas, luzes de pista, torre, pilhas, mastro de luz, biruta;
  SM_USA_DropZone_Radar (spin; origem no eixo, at_m = RADAR_AT, ~15 rpm, para sem energia): o radar de aproximação;
  SM_USA_DropZone_Scaffold: o andaime da obra (em volta da torre).
Materiais (os slots das construções, import_provisional.py): Body, Trim, Dark, Player, Concrete, Hazard, Glass, Light,
Beacon, Yellow, Paint, Crate, CivCloth.
"""
import math
import os
import sys


sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, scaffold  # noqa: E402
from model_supply_crate import LID_TOP, crate_into  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_drop_zone')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_USA_DropZone'

COLORS.update({'Body': (190, 190, 184), 'Trim': (70, 70, 68), 'Dark': (40, 40, 38), 'Player': (86, 108, 150),
               'Concrete': (150, 146, 136), 'Hazard': (214, 170, 30), 'Glass': (30, 50, 80), 'Light': (255, 230, 170),
               'Beacon': (255, 170, 40), 'Yellow': (214, 164, 32), 'Paint': (222, 222, 216), 'Crate': (96, 98, 62),
               'CivCloth': (150, 128, 96)})

HX = HY = 8.0                   # o lote 16 × 16
PAD = 5.0                       # o pátio: meia largura (10 × 10 m)
Z0 = 0.14                       # o topo da laje do pátio
MARK = 0.02                     # a espessura da tinta (2 cm: não pisca contra a laje)
RING = (3.15, 3.55)             # o anel amarelo do alvo
TC = (-6.3, -6.3)               # a torre de controle (canto de trás, esquerda)
CAB_Z0, CAB_Z1 = 4.25, 6.05     # a cabine da torre
RADAR_AT = (TC[0], TC[1], 6.4)  # o eixo do radar, em cima do teto da torre


def flat_ring(p, m, c, r0, r1, z0, z1, seg=64):
    """Um anel achatado (a tinta do alvo): as faces de fora, de cima, de dentro e de baixo."""
    b = p.bm(m + '#ring', 0.0)
    rings = []
    for r, z in ((r1, z0), (r1, z1), (r0, z1), (r0, z0)):
        rings.append([b.verts.new(V(c[0] + r * math.cos(2 * math.pi * k / seg), c[1] + r * math.sin(2 * math.pi * k / seg), z)) for k in range(seg)])
    for i in range(4):
        ra, rb = rings[i], rings[(i + 1) % 4]
        for k in range(seg):
            b.faces.new([ra[k], ra[(k + 1) % seg], rb[(k + 1) % seg], rb[k]])


def pad(p):
    """O pátio: a laje, o filete branco da borda, as cantoneiras de perigo, o anel amarelo e o X branco por cima."""
    p.box('Concrete', -PAD, PAD, -PAD, PAD, -0.3, Z0, bevel=0.04)
    e0, e1 = PAD - 0.47, PAD - 0.25
    for (x0, x1, y0, y1) in ((-e1, e1, -e1, -e0), (-e1, e1, e0, e1), (-e1, -e0, -e0, e0), (e0, e1, -e0, e0)):
        p.box('Paint', x0, x1, y0, y1, Z0 - 0.01, Z0 + MARK, bevel=0.0)
    for sx in (-1, 1):
        for sy in (-1, 1):
            a, b = sorted((sx * 2.55, sx * 4.35))
            c, d = sorted((sy * 3.9, sy * 4.35))
            p.box('Hazard', a, b, c, d, Z0 - 0.01, Z0 + MARK, bevel=0.0)
            a, b = sorted((sx * 3.9, sx * 4.35))
            c, d = sorted((sy * 2.55, sy * 3.9))
            p.box('Hazard', a, b, c, d, Z0 - 0.01, Z0 + MARK, bevel=0.0)
    flat_ring(p, 'Yellow', (0.0, 0.0), RING[0], RING[1], Z0 - 0.01, Z0 + MARK)
    # o X branco, 2 cm acima do anel: um contorno só em cruz (duas barras sobrepostas deixavam o cruzamento preto)
    A, w = 4.0, 0.275
    plus = [(A, w), (w, w), (w, A), (-w, A), (-w, w), (-A, w), (-A, -w), (-w, -w), (-w, -A), (w, -A), (w, -w), (A, -w)]
    c45 = math.cos(math.radians(45))
    xy = [((u - v) * c45, (u + v) * c45) for u, v in plus]
    b = p.bm('Paint#x', 0.0)
    bot = [b.verts.new(V(x, y, Z0 - 0.01)) for x, y in xy]
    top = [b.verts.new(V(x, y, Z0 + 2 * MARK)) for x, y in xy]
    b.faces.new(top)
    b.faces.new(list(reversed(bot)))
    for i in range(len(xy)):
        j = (i + 1) % len(xy)
        b.faces.new([bot[i], bot[j], top[j], top[i]])
    # as luzes de pista na borda do pátio (3 por lado)
    for t in (-2.5, 0.0, 2.5):
        for (x, y) in ((PAD - 0.12, t), (-PAD + 0.12, t), (t, PAD - 0.12), (t, -PAD + 0.12)):
            p.box('Dark', x - 0.11, x + 0.11, y - 0.11, y + 0.11, Z0 - 0.01, Z0 + 0.08, bevel=0.01)
            p.box('Light', x - 0.065, x + 0.065, y - 0.065, y + 0.065, Z0 + 0.08, Z0 + 0.12, bevel=0.0)


def tower(p):
    """A torre de controle pequena: 4 pernas treliçadas, a escada, a cabine com a faixa de janelas e a faixa azul, o
    teto com a base do radar, a baliza e a antena."""
    cx, cy = TC
    h = 0.7
    legs = [(cx + sx * h, cy + sy * h) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    for (x, y) in legs:
        p.box('Concrete', x - 0.25, x + 0.25, y - 0.25, y + 0.25, -0.2, 0.22, bevel=0.03)
        p.cyl('Trim', (x, y, (0.22 + CAB_Z0) / 2), 'z', 0.1, CAB_Z0 - 0.22, seg=10, bevel=0.0)
    for i in range(4):                                                                             # as travessas e os X de cada face
        (ax, ay), (bx, by) = legs[i], legs[(i + 1) % 4]
        for (za, zb) in ((0.5, 2.3), (2.3, 4.1)):
            for (z0, z1) in ((za, zb), (zb, za)):
                d = (bx - ax, by - ay, z1 - z0)
                L = math.sqrt(sum(c * c for c in d))
                p.cyl('Trim', ((ax + bx) / 2, (ay + by) / 2, (z0 + z1) / 2), d, 0.045, L, seg=6, bevel=0.0)
            p.cyl('Trim', ((ax + bx) / 2, (ay + by) / 2, zb), (bx - ax, by - ay, 0.0), 0.05, 2 * h, seg=6, bevel=0.0)
    lx = cx + h + 0.25                                                                             # a escada (lado do pátio, +X)
    for y in (cy - 0.25, cy + 0.25):
        p.box('Trim', lx - 0.03, lx + 0.03, y - 0.03, y + 0.03, 0.22, CAB_Z0, bevel=0.0)
    for k in range(13):
        z = 0.5 + k * 0.3
        p.box('Trim', lx - 0.02, lx + 0.02, cy - 0.25, cy + 0.25, z, z + 0.04, bevel=0.0)
    # a plataforma, a cabine, as janelas (com os montantes), a faixa azul e o teto
    p.box('Trim', cx - 1.25, cx + 1.25, cy - 1.25, cy + 1.25, CAB_Z0 - 0.15, CAB_Z0, bevel=0.03)
    p.box('Body', cx - 1.15, cx + 1.15, cy - 1.15, cy + 1.15, CAB_Z0, CAB_Z1, bevel=0.06)
    w0, w1 = CAB_Z0 + 0.8, CAB_Z1 - 0.25
    for s in (-1, 1):
        a, b = sorted((s * 1.15, s * 1.175))
        p.box('Glass', cx + a, cx + b, cy - 0.95, cy + 0.95, w0, w1, bevel=0.0)
        p.box('Glass', cx - 0.95, cx + 0.95, cy + a, cy + b, w0, w1, bevel=0.0)
        a, b = sorted((s * 1.15, s * 1.18))
        p.box('Player', cx + a, cx + b, cy - 0.95, cy + 0.95, CAB_Z0 + 0.3, CAB_Z0 + 0.6, bevel=0.0)
        p.box('Player', cx - 0.95, cx + 0.95, cy + a, cy + b, CAB_Z0 + 0.3, CAB_Z0 + 0.6, bevel=0.0)
        a, b = sorted((s * 1.175, s * 1.19))
        for t in (-0.32, 0.32):
            p.box('Dark', cx + a, cx + b, cy + t - 0.03, cy + t + 0.03, w0, w1, bevel=0.0)
            p.box('Dark', cx + t - 0.03, cx + t + 0.03, cy + a, cy + b, w0, w1, bevel=0.0)
    p.box('Trim', cx - 1.35, cx + 1.35, cy - 1.35, cy + 1.35, CAB_Z1, CAB_Z1 + 0.2, bevel=0.04)
    p.cyl('Dark', (RADAR_AT[0], RADAR_AT[1], (CAB_Z1 + 0.2 + RADAR_AT[2]) / 2), 'z', 0.3, RADAR_AT[2] - CAB_Z1 - 0.2, seg=16, bevel=0.0)
    bx, by = cx - 1.0, cy - 1.0                                                                    # a baliza (canto de trás)
    p.cyl('Trim', (bx, by, CAB_Z1 + 0.2 + 0.5), 'z', 0.04, 1.0, seg=8, bevel=0.0)
    p.cyl('Beacon', (bx, by, CAB_Z1 + 1.28), 'z', 0.09, 0.16, seg=12, bevel=0.0, smooth=True)
    p.cyl('Trim', (cx + 1.0, cy - 1.0, CAB_Z1 + 0.2 + 1.1), 'z', 0.025, 2.2, seg=6, bevel=0.0)        # a antena de rádio


def stacks(p):
    """As caixas recebidas no canto de trás (a mesma caixa que cai, sem o saco) e a carga coberta com a lona azul."""
    ax, ay = -6.4, 6.3
    jit = [2.0, -3.0, 1.5, -1.0, 4.0, -2.5]
    k = 0
    for dx in (-0.53, 0.53):
        for dy in (-0.63, 0.63):
            crate_into(p, (ax + dx, ay + dy, 0.0, 90.0 + jit[k]), bag=False)
            k += 1
    for dy in (-0.55, 0.55):
        crate_into(p, (ax, ay + dy, LID_TOP + 0.005, jit[k]), bag=False)
        k += 1
    bxc, byc = -6.4, 3.3                                                                           # a carga coberta
    for dx in (-0.53, 0.53):
        crate_into(p, (bxc + dx, byc, 0.0, 90.0), bag=False)
    x0, x1, y0, y1 = bxc - 1.1, bxc + 1.1, byc - 0.68, byc + 0.68
    p.box('Player', x0, x1, y0, y1, LID_TOP - 0.02, LID_TOP + 0.14, bevel=0.06)
    for x in (x0, x1):
        p.box('Player', x - 0.03, x + 0.03, y0, y1, 0.4, LID_TOP + 0.1, bevel=0.0)
    for y in (byc - 0.35, byc + 0.35):
        p.box('Dark', x0 - 0.05, x1 + 0.05, y - 0.04, y + 0.04, 0.38, LID_TOP + 0.16, bevel=0.0)


def light_mast(p, x, y, h=7.0):
    """O mastro de luz (frente, esquerda): os 2 holofotes virados pro alvo e a sirene no topo."""
    p.box('Concrete', x - 0.45, x + 0.45, y - 0.45, y + 0.45, -0.2, 0.3, bevel=0.04)
    p.box('Hazard', x - 0.3, x + 0.3, y - 0.3, y + 0.3, 0.3, 0.9, bevel=0.03)
    p.cyl('Trim', (x, y, (0.9 + h) / 2), 'z', 0.11, h - 0.9, seg=12, bevel=0.0)
    yaw = math.degrees(math.atan2(-y, -x))                                                         # pro centro do pátio
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    p.obox('Trim', (x, y, h - 0.35), (0.12, 1.3, 0.12), yaw=yaw, bevel=0.0)                         # a travessa
    for t in (-0.42, 0.42):
        hx, hy = x - s * t + c * 0.18, y + c * t + s * 0.18
        p.obox('Dark', (hx, hy, h - 0.5), (0.32, 0.55, 0.42), yaw=yaw, pitch=-30, bevel=0.02)
        p.obox('Light', (hx + c * 0.14, hy + s * 0.14, h - 0.58), (0.06, 0.46, 0.33), yaw=yaw, pitch=-30, bevel=0.0)
    p.cyl('Dark', (x, y, h + 0.05), 'z', 0.14, 0.1, seg=12, bevel=0.0)
    p.cyl('Beacon', (x, y, h + 0.2), 'z', 0.1, 0.2, seg=12, bevel=0.0, smooth=True)


def windsock(p, x, y, h=5.0, length=1.9):
    """A biruta (frente, direita): o poste, o aro e a manga em 5 gomos alternando amarelo e branco, pra fora do lote."""
    p.box('Concrete', x - 0.35, x + 0.35, y - 0.35, y + 0.35, -0.2, 0.25, bevel=0.03)
    p.cyl('Trim', (x, y, (0.25 + h) / 2), 'z', 0.07, h - 0.25, seg=10, bevel=0.0)
    p.cyl('Trim', (x, y, h + 0.05), 'z', 0.1, 0.1, seg=10, bevel=0.0)
    w = (0.55, 0.75, -0.2)
    n = math.sqrt(sum(c * c for c in w))
    w = tuple(c / n for c in w)
    r0, r1, seg = 0.3, 0.12, 5
    for i in range(seg):
        t0, t1 = i / seg, (i + 1) / seg
        ra, rb = r0 + (r1 - r0) * t0, r0 + (r1 - r0) * t1
        mid = (t0 + t1) / 2 * length
        c = (x + w[0] * (mid + 0.12), y + w[1] * (mid + 0.12), h - 0.05 + w[2] * mid)
        p.cyl('Yellow' if i % 2 == 0 else 'Paint', c, w, ra, length / seg, seg=16, r2=rb, bevel=0.0, smooth=True)
    p.cyl('Trim', (x + w[0] * 0.12, y + w[1] * 0.12, h - 0.05), w, r0 + 0.02, 0.04, seg=16, bevel=0.0)


def base():
    p = Part(PREFIX + '_Base')
    for (x0, x1, y0, y1) in ((-HX, HX, -HY, -HY + 0.35), (-HX, HX, HY - 0.35, HY), (-HX, -HX + 0.35, -HY + 0.35, HY - 0.35), (HX - 0.35, HX, -HY + 0.35, HY - 0.35)):
        p.box('Concrete', x0, x1, y0, y1, -0.3, 0.12, bevel=0.03)
    pad(p)
    tower(p)
    stacks(p)
    light_mast(p, 6.4, -6.4)
    windsock(p, 6.3, 6.2)
    return p.finish()


def radar():
    """O radar de aproximação (a antena em barra que gira): origem no eixo (RADAR_AT); o jogo gira em Z (~15 rpm)."""
    p = Part(PREFIX + '_Radar')
    p.cyl('Trim', (0.0, 0.0, 0.08), 'z', 0.24, 0.16, seg=16, bevel=0.0)
    p.box('Body', -0.14, 0.14, -0.14, 0.14, 0.16, 0.6, bevel=0.03)
    p.box('Trim', -0.1, 0.1, -1.15, 1.15, 0.6, 0.68, bevel=0.0)                                     # a travessa de trás
    for (y, yaw, x) in ((0.0, 0.0, 0.12), (-0.86, 14.0, 0.2), (0.86, -14.0, 0.2)):                   # o refletor curvo, 3 painéis
        p.obox('Paint', (x, y, 0.92), (0.07, 0.86, 0.5), yaw=yaw, pitch=12, bevel=0.0)
        p.obox('Dark', (x - 0.06, y, 0.92), (0.04, 0.8, 0.42), yaw=yaw, pitch=12, bevel=0.0)
    d = (0.62, 0.0, 0.18)
    L = math.sqrt(sum(c * c for c in d))
    p.cyl('Trim', (0.12 + d[0] / 2, 0.0, 0.62 + d[2] / 2), d, 0.03, L, seg=8, bevel=0.0)              # o braço do alimentador
    p.box('Dark', 0.7, 0.86, -0.1, 0.1, 0.74, 0.88, bevel=0.01)
    return p.finish()


def main():
    clear_scene()
    total = export(base(), OUT)
    clear_scene()
    total += export(radar(), OUT)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', TC[0] - 0.85, TC[0] + 0.85, TC[1] - 0.85, TC[1] + 0.85, CAB_Z1, gap=0.3), OUT)
    print('MODEL total', total, 'tris')
    print('MODEL lote 16 x 16 m; patio 10 x 10, alvo (centro) [0, 0], anel', RING, '; spin radar at_m', RADAR_AT, 'rpm 15')


main()
