"""A defesa de base da USA (a bateria Patriot) provisória, modelada no Blender por script (sessão Modelos 3D, 14/10/2026).

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_usa_patriot.py -- <pasta de saída>

Pedido do Felipe (14/10): "a torreta que protege a base, com a pegada clássica icônica do C&C Zero Hour, como nas
construções" (não é cópia). O estudo (a fórmula de docs/CONCEITO_CONSTRUCOES.md §1 e a bíblia da USA §2, sobre o
Patriot Missile System do ZH, que é a defesa de base da USA: custa 1000, consome energia e desliga sem ela):
- o que faz o Patriot do ZH ficar na memória: a CAIXA LANÇADORA de 4 células inclinada pra cima, girando numa base
  quadrada baixa, com o radar pequeno ao lado; lê de cima num relance e "conta" a facção (míssil guiado, alta
  tecnologia). É esse o elemento-assinatura, redesenhado na linguagem das nossas construções da USA: painéis de
  metal cinza-claro, volumes retos e robustos, faixa de perigo amarela e preta no piso, luz de serviço, a cor do
  jogador em faixas e painéis (nunca a peça inteira), nada de neon;
- a função na forma: as 4 tampas das células na frente, as grelhas do escape atrás, o radar que gira;
- vida: o radar girando (peça "spin", para sem energia), a sirene âmbar piscando, as luzes de serviço;
- assentado no chão: só a borda de fundação e a faixa de perigo pintada; sem pátio.

Lote 8 × 8 m (2 × 2 quadrados da grade de 4 m; no ZH a Patriot é pequena), frente em +X (pra onde a caixa aponta
em repouso). Alturas: bloco blindado 1,2 m, caixa lançadora até ~5,3 m, mastro do radar 4,4 m: peças altas e finas,
dentro da regra (docs/PRODUCAO_3D.md §3).

Peças (origem no chão, no centro do lote):
  SM_USA_Patriot_Base (hull): borda de fundação, faixa de perigo, bloco octogonal blindado, pedestal, mastro do radar
    com a sirene (slot Beacon, pisca), gerador, escada, luzes;
  SM_USA_Patriot_Turret (turret, gira em Z; turret_pivot_m = [0, 0]): o prato giratório, o berço e a CAIXA LANÇADORA
    de 2 × 2 células inclinada 45°, com a faixa azul (cor do jogador);
  SM_USA_Patriot_Dish (spin, rpm ~12; at_m = DISH_AT): o radar no topo do mastro;
  SM_USA_Patriot_Scaffold: o andaime da obra.
Materiais (os slots das construções, import_provisional.py): Body, Trim, Dark, Player, Concrete, Hazard, Glass,
Light, Beacon, Yellow.
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, mat, scaffold, select_only  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_patriot')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_USA_Patriot'

COLORS.update({'Body': (190, 190, 184), 'Trim': (70, 70, 68), 'Dark': (40, 40, 38), 'Player': (86, 108, 150),
               'Concrete': (150, 146, 136), 'Hazard': (214, 170, 30), 'Glass': (30, 50, 80), 'Light': (255, 230, 170),
               'Beacon': (255, 170, 40), 'Yellow': (214, 164, 32), 'Crate': (112, 100, 62)})

HX = HY = 4.0                    # o lote 8 × 8
BASE_R, BASE_H = 2.3, 1.1        # o bloco octogonal blindado (raio até o canto, altura)
PED_Z = BASE_H + 0.75            # o topo do pedestal (o prato giratório senta aqui)
MAST = (-2.35, -2.35)            # o mastro do radar (canto de trás, esquerda)
MAST_H = 4.4
DISH_AT = (MAST[0], MAST[1], MAST_H)
BOX = (3.8, 2.3, 1.7)            # a caixa lançadora: comprimento, largura, altura
BOX_PITCH = 45.0
BOX_C = (0.6, 0.0, 3.75)         # o centro da caixa (sobre o berço)


def hazard_ring(p, x0, x1, y0, y1, w=0.45, z=0.13, seg=0.7):
    """A faixa de perigo pintada no chão (quadrados amarelos e pretos alternados) em volta de um retângulo."""
    def run(ax, ay, bx, by):
        L = math.hypot(bx - ax, by - ay)
        n = max(1, int(L / seg))
        for k in range(n):
            t0, t1 = k / n, (k + 1) / n
            cx, cy = ax + (bx - ax) * (t0 + t1) / 2, ay + (by - ay) * (t0 + t1) / 2
            m = 'Hazard' if k % 2 == 0 else 'Dark'
            if abs(bx - ax) > abs(by - ay):
                p.box(m, min(ax, bx) + L * t0, min(ax, bx) + L * t1, cy - w / 2, cy + w / 2, z, z + 0.012, bevel=0.0)
            else:
                p.box(m, cx - w / 2, cx + w / 2, min(ay, by) + L * t0, min(ay, by) + L * t1, z, z + 0.012, bevel=0.0)
    run(x0, y0 - w / 2, x1, y0 - w / 2)
    run(x0, y1 + w / 2, x1, y1 + w / 2)
    run(x0 - w / 2, y0 - w, x0 - w / 2, y1 + w)
    run(x1 + w / 2, y0 - w, x1 + w / 2, y1 + w)


def base():
    p = Part(PREFIX + '_Base')
    # a borda de fundação do lote (só a borda: o prédio senta na areia) e a laje da própria estrutura
    for (x0, x1, y0, y1) in ((-HX, HX, -HY, -HY + 0.35), (-HX, HX, HY - 0.35, HY), (-HX, -HX + 0.35, -HY + 0.35, HY - 0.35), (HX - 0.35, HX, -HY + 0.35, HY - 0.35)):
        p.box('Concrete', x0, x1, y0, y1, -0.3, 0.12, bevel=0.03)
    p.cyl('Concrete', (0.0, 0.0, 0.0), 'z', 3.25, 0.26, seg=8, bevel=0.03)                         # a laje octogonal
    for (x0, x1, y0, y1) in ((-2.95, 2.95, -2.95, -2.55), (-2.95, 2.95, 2.55, 2.95), (-2.95, -2.55, -2.55, 2.55), (2.55, 2.95, -2.55, 2.55)):
        p.box('Hazard', x0, x1, y0, y1, 0.13, 0.142, bevel=0.0)                                    # a faixa de perigo em volta (o material lista)
    # o bloco octogonal blindado, com a placa de topo, os painéis das faces e a cor do jogador em dois painéis
    p.cyl('Body', (0.0, 0.0, 0.13 + BASE_H / 2), 'z', BASE_R, BASE_H, seg=8, bevel=0.06)
    p.cyl('Body', (0.0, 0.0, 0.13 + BASE_H + 0.04), 'z', BASE_R + 0.08, 0.08, seg=8, bevel=0.01)
    p.cyl('Dark', (0.0, 0.0, 0.13 + BASE_H + 0.085), 'z', BASE_R - 0.35, 0.01, seg=8, bevel=0.0)
    p.cyl('Body', (0.0, 0.0, 0.13 + BASE_H + 0.09), 'z', BASE_R - 0.5, 0.01, seg=8, bevel=0.0)
    for k in range(8):
        a = math.radians(k * 45 + 22.5)
        r = BASE_R * math.cos(math.radians(22.5))
        cx, cy = r * math.cos(a), r * math.sin(a)
        m = 'Player' if k in (0, 7) else 'Dark'                                                      # as duas faces da frente: azul
        size = (0.012, 1.3, 0.55) if m == 'Player' else (0.012, 1.3, 0.08)
        z = 0.13 + 0.55 if m == 'Player' else 0.13 + 0.95
        p.obox(m, (cx + 0.006 * math.cos(a), cy + 0.006 * math.sin(a), z), size, yaw=k * 45 + 22.5, bevel=0.0)
        if m == 'Dark':
            p.obox('Dark', (cx + 0.006 * math.cos(a), cy + 0.006 * math.sin(a), 0.13 + 0.35), (0.012, 1.3, 0.08), yaw=k * 45 + 22.5, bevel=0.0)
    # o pedestal e o rolamento do prato
    p.cyl('Body', (0.0, 0.0, 0.13 + BASE_H + 0.09 + (PED_Z - BASE_H - 0.22) / 2), 'z', 1.05, PED_Z - BASE_H - 0.22, seg=24, bevel=0.03)
    for k in range(8):
        a = math.radians(k * 45)
        p.obox('Dark', (1.055 * math.cos(a), 1.055 * math.sin(a), 0.13 + BASE_H + 0.09 + (PED_Z - BASE_H - 0.22) / 2), (0.012, 0.18, PED_Z - BASE_H - 0.5), yaw=k * 45, bevel=0.0)
    p.cyl('Trim', (0.0, 0.0, PED_Z + 0.1), 'z', 1.18, 0.1, seg=32, bevel=0.01)
    # o mastro do radar (canto de trás, esquerda) com a caixa de serviço, a escada e a sirene âmbar
    mx, my = MAST
    p.box('Body', mx - 0.2, mx + 0.2, my - 0.2, my + 0.2, 0.12, MAST_H - 0.1, bevel=0.03)
    p.box('Trim', mx - 0.26, mx + 0.26, my - 0.26, my + 0.26, MAST_H - 0.1, MAST_H, bevel=0.01)
    p.box('Dark', mx - 0.35, mx + 0.35, my + 0.2, my + 0.55, 0.12, 1.3, bevel=0.02)                   # a caixa de serviço
    for k in range(5):
        p.box('Trim', mx - 0.3, mx + 0.3, my + 0.56, my + 0.57, 0.3 + k * 0.18, 0.33 + k * 0.18, bevel=0.0)
    p.cyl('Dark', (mx + 0.32, my - 0.32, 1.55), 'z', 0.05, 0.1, seg=10, bevel=0.0)                   # a sirene (cúpula âmbar) no braço
    p.box('Trim', mx + 0.2, mx + 0.36, my - 0.36, my - 0.28, 1.45, 1.5, bevel=0.0)
    p.cyl('Beacon', (mx + 0.32, my - 0.32, 1.72), 'z', 0.09, 0.24, seg=14, bevel=0.0, smooth=True)
    p.cyl('Dark', (mx + 0.32, my - 0.32, 1.855), 'z', 0.095, 0.03, seg=14, bevel=0.0)
    # o gerador (canto da frente, direita) com a grelha e o escape; as luzes de serviço no bloco
    gx, gy = 2.4, 2.2
    p.box('Body', gx - 0.75, gx + 0.75, gy - 0.5, gy + 0.5, 0.12, 1.05, bevel=0.04)
    for k in range(6):
        p.obox('Dark', (gx + 0.76, gy, 0.35 + k * 0.1), (0.02, 0.7, 0.045), roll=30, bevel=0.0)
    p.cyl('Dark', (gx - 0.5, gy - 0.3, 1.3), 'z', 0.06, 0.5, seg=10, bevel=0.0)
    p.box('Player', gx - 0.6, gx + 0.6, gy - 0.512, gy - 0.5, 0.4, 0.75, bevel=0.0)                   # a faixa azul do gerador
    for a in (60.0, 300.0):                                                                           # as luzes de serviço
        r = BASE_R * math.cos(math.radians(22.5)) + 0.03
        cx, cy = r * math.cos(math.radians(a)), r * math.sin(math.radians(a))
        p.obox('Trim', (cx, cy, 0.13 + BASE_H - 0.2), (0.1, 0.3, 0.14), yaw=a, bevel=0.0)
        p.obox('Light', (cx + 0.06 * math.cos(math.radians(a)), cy + 0.06 * math.sin(math.radians(a)), 0.13 + BASE_H - 0.2), (0.02, 0.22, 0.08), yaw=a, bevel=0.0)
    # a escada de acesso ao bloco (lado esquerdo) e o corrimão
    ex = -BASE_R * math.cos(math.radians(22.5)) - 0.12
    for k in range(4):
        p.box('Trim', ex - 0.02, ex + 0.3, -0.4, 0.4, 0.3 + k * 0.28, 0.34 + k * 0.28, bevel=0.0)
    for y in (-0.42, 0.42):
        p.box('Trim', ex - 0.04, ex, y - 0.02, y + 0.02, 0.13, 0.13 + BASE_H + 0.05, bevel=0.0)
    return p.finish()


def turret():
    p = Part(PREFIX + '_Turret')
    # o prato giratório e o berço (dois munhões) que segura a caixa
    p.cyl('Body', (0.0, 0.0, PED_Z + 0.2 + 0.175), 'z', 1.05, 0.35, seg=24, bevel=0.03)
    p.cyl('Dark', (0.0, 0.0, PED_Z + 0.2 + 0.36), 'z', 0.9, 0.04, seg=24, bevel=0.0)
    for s in (-1, 1):
        p.prism_xz('Body', [(-0.9, PED_Z + 0.55), (0.8, PED_Z + 0.55), (0.6, 3.0), (-0.3, 3.0)], 0.95 * s, 1.2 * s, bevel=0.03)
        p.cyl('Trim', (0.15, 1.22 * s, 2.95), 'y', 0.3, 0.12, seg=18, bevel=0.01)                    # os munhões
        p.cyl('Dark', (0.15, 1.3 * s, 2.95), 'y', 0.12, 0.06, seg=12, bevel=0.0)
    p.box('Body', -0.9, 0.8, -0.95, 0.95, PED_Z + 0.55, PED_Z + 0.85, bevel=0.03)                     # a mesa entre os munhões
    # o cilindro de elevação, do prato até embaixo da caixa
    d = Vector((-0.1, 0.0, 3.1)) - Vector((-1.0, 0.0, PED_Z + 0.6))
    p.cyl('Trim', (-1.0 + d.x * 0.5, 0.0, PED_Z + 0.6 + d.z * 0.5), (d.x, d.y, d.z), 0.09, d.length, seg=12, bevel=0.0)
    p.cyl('Yellow', (-1.0 + d.x * 0.25, 0.0, PED_Z + 0.6 + d.z * 0.25), (d.x, d.y, d.z), 0.14, d.length * 0.45, seg=12, bevel=0.01)
    # a CAIXA LANÇADORA (a assinatura): 2 × 2 células, inclinada 45°, com as 4 tampas na frente, as grelhas atrás,
    # as placas laterais com a faixa azul (cor do jogador) e o painel azul em cima
    L, W, H = BOX
    cx, cy, cz = BOX_C
    p.obox('Body', (cx, cy, cz), (L, W, H), pitch=BOX_PITCH, bevel=0.06)
    ca, sa = math.cos(math.radians(BOX_PITCH)), math.sin(math.radians(BOX_PITCH))
    fwd = Vector((ca, 0.0, sa))                  # o eixo da caixa (pra frente e pra cima)
    up = Vector((-sa, 0.0, ca))                  # o "cima" da caixa
    nose = Vector((cx, cy, cz)) + fwd * (L / 2)
    tail = Vector((cx, cy, cz)) - fwd * (L / 2)
    for (dy, dz) in ((-0.55, -0.4), (0.55, -0.4), (-0.55, 0.4), (0.55, 0.4)):                        # as 4 tampas das células (redondas)
        c = nose + Vector((0, dy, 0)) + up * dz
        p.cyl('Trim', (c.x + fwd.x * 0.03, c.y, c.z + fwd.z * 0.03), (fwd.x, fwd.y, fwd.z), 0.42, 0.06, seg=24, bevel=0.01)
        p.cyl('Dark', (c.x + fwd.x * 0.065, c.y, c.z + fwd.z * 0.065), (fwd.x, fwd.y, fwd.z), 0.34, 0.02, seg=24, bevel=0.0)
        p.cyl('Body', (c.x + fwd.x * 0.08, c.y, c.z + fwd.z * 0.08), (fwd.x, fwd.y, fwd.z), 0.08, 0.02, seg=12, bevel=0.0)
    p.obox('Dark', (nose.x + fwd.x * 0.012, nose.y, nose.z + fwd.z * 0.012), (0.02, 0.05, H - 0.1), pitch=BOX_PITCH, bevel=0.0)     # as juntas da frente
    p.obox('Dark', (nose.x + fwd.x * 0.012, nose.y, nose.z + fwd.z * 0.012), (0.02, W - 0.1, 0.05), pitch=BOX_PITCH, bevel=0.0)
    for s in (-1, 1):                                                                                  # a junta entre as células nas laterais e em cima
        cs = Vector((cx, cy + (W / 2 + 0.006) * s, cz))
        p.obox('Dark', (cs.x, cs.y, cs.z), (L - 0.3, 0.012, 0.04), pitch=BOX_PITCH, bevel=0.0)
    cj = Vector((cx, cy, cz)) + up * (H / 2 + 0.006)
    p.obox('Dark', (cj.x, cj.y, cj.z), (L - 0.3, 0.04, 0.012), pitch=BOX_PITCH, bevel=0.0)
    for (dy, dz) in ((-0.55, -0.38, ), (0.55, -0.38), (-0.55, 0.38), (0.55, 0.38)):                    # as grelhas do escape atrás
        c = tail + Vector((0, dy, 0)) + up * dz
        p.obox('Dark', (c.x - fwd.x * 0.015, c.y, c.z - fwd.z * 0.015), (0.03, 0.84, 0.6), pitch=BOX_PITCH, bevel=0.0)
        for k in range(5):
            cc = c + up * (-0.24 + k * 0.12)
            p.obox('Trim', (cc.x - fwd.x * 0.03, cc.y, cc.z - fwd.z * 0.03), (0.03, 0.76, 0.03), pitch=BOX_PITCH, bevel=0.0)
    for s in (-1, 1):                                                                                  # as placas laterais e a faixa azul
        c = Vector((cx, cy + (W / 2 + 0.03) * s, cz))
        p.obox('Body', (c.x, c.y, c.z), (L * 0.9, 0.06, H * 0.8), pitch=BOX_PITCH, bevel=0.015)
        c2 = c + up * (-H * 0.3) + Vector((0, 0.035 * s, 0))
        p.obox('Player', (c2.x, c2.y, c2.z), (L * 0.78, 0.012, 0.26), pitch=BOX_PITCH, bevel=0.0)
        c3 = c + up * (H * 0.28) + Vector((0, 0.035 * s, 0))
        p.obox('Dark', (c3.x, c3.y, c3.z), (L * 0.5, 0.012, 0.06), pitch=BOX_PITCH, bevel=0.0)
    ct = Vector((cx, cy, cz)) + up * (H / 2 + 0.008)
    p.obox('Player', (ct.x - fwd.x * 0.3, ct.y, ct.z - fwd.z * 0.3), (L * 0.4, W * 0.36, 0.012), pitch=BOX_PITCH, bevel=0.0)     # o painel azul em cima (lê de cima)
    cb = Vector((cx, cy, cz)) + up * (H / 2 + 0.01) - fwd * (L * 0.38)
    p.obox('Dark', (cb.x, cb.y, cb.z), (0.5, 0.4, 0.02), pitch=BOX_PITCH, bevel=0.0)                  # a tampa de manutenção
    # o sensor de tiro no munhão direito
    p.box('Dark', 0.3, 0.7, 1.25, 1.55, 2.95, 3.3, bevel=0.02)
    p.obox('Glass', (0.71, 1.4, 3.12), (0.02, 0.22, 0.22), bevel=0.0)
    return p.finish()


def dish():
    """O radar no topo do mastro: origem no eixo (DISH_AT); o jogo gira em Z (spin, ~12 rpm, para sem energia)."""
    p = Part(PREFIX + '_Dish')
    p.cyl('Trim', (0.0, 0.0, 0.08), 'z', 0.12, 0.16, seg=12, bevel=0.0)
    p.box('Body', -0.12, 0.12, -0.12, 0.12, 0.16, 0.55, bevel=0.02)
    p.cyl('Trim', (0.0, 0.0, 0.55), 'y', 0.1, 0.36, seg=12, bevel=0.0)
    # o prato (uma calota rasa) inclinado 35° pra cima, com o alimentador na frente
    a = math.radians(35)
    n = Vector((math.cos(a), 0.0, math.sin(a)))
    c = Vector((0.0, 0.0, 0.55)) + n * 0.1
    p.cyl('Body', (c.x, c.y, c.z), (n.x, n.y, n.z), 0.42, 0.22, seg=28, r2=0.78, bevel=0.0, smooth=True)     # a calota (cone raso)
    p.cyl('Trim', (c.x + n.x * 0.11, c.y, c.z + n.z * 0.11), (n.x, n.y, n.z), 0.8, 0.03, seg=28, bevel=0.0)    # o aro
    p.cyl('Dark', (c.x - n.x * 0.13, c.y, c.z - n.z * 0.13), (n.x, n.y, n.z), 0.3, 0.06, seg=16, bevel=0.0)    # as costas
    f = c + n * 0.11
    for k in range(3):                                                                                 # o tripé do alimentador
        a = math.radians(k * 120 + 90)
        side = Vector((0.0, math.cos(a), 0.0)) * 0.55 + Vector((-n.z, 0.0, n.x)) * (math.sin(a) * 0.55)
        q0 = f + side
        q1 = f + n * 0.5
        d = q1 - q0
        p.cyl('Trim', ((q0.x + q1.x) / 2, (q0.y + q1.y) / 2, (q0.z + q1.z) / 2), (d.x, d.y, d.z), 0.02, d.length, seg=6, bevel=0.0)
    h = f + n * 0.5
    p.box('Dark', h.x - 0.07, h.x + 0.07, h.y - 0.07, h.y + 0.07, h.z - 0.07, h.z + 0.07, bevel=0.01)
    return p.finish()


def main():
    clear_scene()
    total = 0
    for build in (base, turret, dish):
        total += export(build(), OUT)
    total += export(scaffold(PREFIX + '_Scaffold', -3.0, 3.0, -3.0, 3.0, 5.3), OUT)
    print('MODEL total', total, 'tris')
    print('MODEL turret_pivot_m [0, 0]; spin dish at_m', DISH_AT, 'rpm 12; beacon no casco (slot Beacon)')


main()
