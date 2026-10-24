"""A caixa de suprimento lançada de paraquedas (o Supply Drop Zone da USA) provisória, modelada no Blender por script
(sessão Modelos 3D, 22/10/2026).

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_supply_crate.py -- <pasta de saída>

O avião de carga passa sobre o Drop Zone a cada 2 min e solta as caixas de paraquedas no pátio. A caixa é a mesma
linguagem da caixa de suprimento do depósito (model_supply_dock.py: olive com a FAIXA AMARELA na tampa, o símbolo do
dinheiro no C&C, lê de cima), menor (1,2 m) e preparada pro lançamento aéreo: a plataforma de metal com os patins
embaixo, as duas cintas de amarração e o saco do paraquedas (lona) amarrado na tampa.

O que sai daqui (coordenadas da Unreal, X frente, Y direita, Z cima; origem no chão, no centro da caixa):
- SM_SupplyDrop_Crate.glb: a caixa com o saco do paraquedas (1,24 × 1,04 × 1,17 m); é a que cai e a que fica no chão;
- SM_SupplyDrop_Canopy.glb: o paraquedas aberto (5,2 m de diâmetro, 12 gomos alternando a cor do jogador e branco,
  as cordas e as 4 cintas até os cantos da tampa); MESMA ORIGEM da caixa: o efeito põe as duas no mesmo transform
  enquanto cai e some com o paraquedas quando a caixa toca o chão.
Materiais (os slots das construções, import_provisional.py): Crate, Yellow, Dark, Trim, Paint, Player, CivCloth.

Módulo: crate_into(p, (x, y, z, giro), bag) põe a caixa numa peça de outro modelo (a pilha do model_drop_zone.py).
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export  # noqa: E402

PREFIX = 'SM_SupplyDrop'
COLORS.update({'Crate': (96, 98, 62), 'Yellow': (214, 164, 32), 'Dark': (40, 40, 38), 'Trim': (70, 70, 68),
               'Paint': (222, 222, 216), 'Player': (86, 108, 150), 'CivCloth': (150, 128, 96)})

CRATE_L, CRATE_W = 1.2, 1.0       # a caixa: comprimento (x), largura (y)
SKID_TOP = 0.1                    # a plataforma com os patins
BODY_TOP, LID_TOP = 0.85, 0.93    # o corpo e a tampa
BAG_TOP = 1.17                    # o saco do paraquedas em cima
CANOPY_R, CANOPY_DOME, CANOPY_SKIRT = 2.6, 1.5, 4.6     # o paraquedas: raio, altura da cúpula, a barra (acima do chão da caixa)
GORES = 12
CONFLUENCE = 2.1                  # onde as cordas se juntam (acima da caixa)


def lbox(p, m, f, x0, x1, y0, y1, z0, z1, bevel=None):
    """Caixa no referencial local f = (x, y, z, giro em graus) da caixa."""
    cx, cy, cz, yaw = f
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    lx, ly = (x0 + x1) / 2, (y0 + y1) / 2
    p.obox(m, (cx + lx * c - ly * s, cy + lx * s + ly * c, cz + (z0 + z1) / 2), (x1 - x0, y1 - y0, z1 - z0), yaw=yaw, bevel=bevel)


def crate_into(p, f=(0.0, 0.0, 0.0, 0.0), bag=True):
    """A caixa do lançamento: a plataforma com 3 patins, o corpo olive, a tampa com a faixa amarela (lê de cima), os
    cantos reforçados, as 2 cintas, o estêncil dos lados e (bag) o saco do paraquedas amarrado na tampa."""
    hx, hy = CRATE_L / 2, CRATE_W / 2
    lbox(p, 'Trim', f, -hx - 0.02, hx + 0.02, -hy - 0.02, hy + 0.02, 0.0, 0.035, bevel=0.01)
    for y in (-0.38, 0.0, 0.38):
        lbox(p, 'Dark', f, -hx, hx, y - 0.06, y + 0.06, 0.035, SKID_TOP, bevel=0.0)
    lbox(p, 'Crate', f, -hx, hx, -hy, hy, SKID_TOP, BODY_TOP, bevel=0.025)
    lbox(p, 'Crate', f, -hx - 0.02, hx + 0.02, -hy - 0.02, hy + 0.02, BODY_TOP, LID_TOP, bevel=0.02)
    lbox(p, 'Yellow', f, -hx - 0.021, hx + 0.021, -0.16, 0.16, BODY_TOP, LID_TOP + 0.006, bevel=0.0)     # a faixa amarela da tampa
    for s in (-1, 1):
        lbox(p, 'Dark', f, -hx - 0.021, hx + 0.021, min(s * 0.16, s * 0.17), max(s * 0.16, s * 0.17), BODY_TOP, LID_TOP + 0.007, bevel=0.0)
    for sx in (-1, 1):                                                                                  # os cantos reforçados
        for sy in (-1, 1):
            xa, xb = sorted((sx * (hx - 0.06), sx * (hx + 0.025)))
            ya, yb = sorted((sy * (hy - 0.06), sy * (hy + 0.025)))
            lbox(p, 'Dark', f, xa, xb, ya, yb, SKID_TOP, BODY_TOP, bevel=0.0)
    for x in (-0.3, 0.3):                                                                               # as 2 cintas de amarração
        lbox(p, 'Dark', f, x - 0.04, x + 0.04, -hy - 0.035, hy + 0.035, SKID_TOP + 0.01, LID_TOP + 0.015, bevel=0.0)
    for s in (-1, 1):                                                                                   # o estêncil dos lados
        y0, y1 = (s * hy, s * (hy + 0.006)) if s > 0 else (s * (hy + 0.006), s * hy)
        lbox(p, 'Paint', f, -0.2, 0.2, y0, y1, 0.36, 0.6, bevel=0.0)
        y0, y1 = (s * (hy + 0.006), s * (hy + 0.009)) if s > 0 else (s * (hy + 0.009), s * (hy + 0.006))
        lbox(p, 'Dark', f, -0.13, 0.13, y0, y1, 0.44, 0.52, bevel=0.0)
    if bag:                                                                                             # o saco do paraquedas
        lbox(p, 'CivCloth', f, -0.4, 0.4, -0.32, 0.32, LID_TOP, BAG_TOP, bevel=0.07)
        for x in (-0.18, 0.18):
            lbox(p, 'Dark', f, x - 0.03, x + 0.03, -0.335, 0.335, LID_TOP, BAG_TOP + 0.012, bevel=0.0)


def crate():
    p = Part(PREFIX + '_Crate')
    crate_into(p)
    return p.finish()


def canopy_point(ph, th, inner=False):
    """Um ponto da cúpula: ph do furo do topo (0) até a barra (pi/2), th em volta; o pano estufa entre as costuras."""
    fr = (th / (2 * math.pi / GORES)) % 1.0
    bulge = 1 + 0.06 * math.sin(math.pi * fr) * math.sin(ph)
    r = CANOPY_R * math.sin(ph) * bulge
    z = CANOPY_SKIRT + CANOPY_DOME * math.cos(ph)
    if inner:
        r *= 0.985
        z -= 0.035
    return (r * math.cos(th), r * math.sin(th), z)


def gore(p, key, k, rows=10, cols=4):
    """Um gomo da cúpula como casca fechada (fora, dentro e as 4 bordas), liso; a barra faz o recorte de vieira."""
    b = p.bm(key, 0.0)
    a0 = 2 * math.pi * k / GORES
    da = 2 * math.pi / GORES

    def grid(inner):
        g = []
        for i in range(rows + 1):
            ph = 0.12 + (math.pi / 2 - 0.12) * i / rows
            row = []
            for j in range(cols + 1):
                th = a0 + da * j / cols
                x, y, z = canopy_point(ph, th, inner)
                if i == rows:
                    z += 0.18 * (1 - math.sin(math.pi * j / cols))          # a costura levanta: a vieira da barra
                row.append(b.verts.new(V(x, y, z)))
            g.append(row)
        return g
    o, n = grid(False), grid(True)
    for i in range(rows):
        for j in range(cols):
            b.faces.new([o[i][j], o[i][j + 1], o[i + 1][j + 1], o[i + 1][j]]).smooth = True
            b.faces.new([n[i + 1][j], n[i + 1][j + 1], n[i][j + 1], n[i][j]]).smooth = True
    for i in range(rows):
        b.faces.new([o[i][0], o[i + 1][0], n[i + 1][0], n[i][0]])
        b.faces.new([n[i][cols], n[i + 1][cols], o[i + 1][cols], o[i][cols]])
    for j in range(cols):
        b.faces.new([o[0][j], n[0][j], n[0][j + 1], o[0][j + 1]])
        b.faces.new([o[rows][j + 1], n[rows][j + 1], n[rows][j], o[rows][j]])


def canopy():
    """O paraquedas aberto, com a mesma origem da caixa: 12 gomos (cor do jogador e branco), o tampo do furo, as
    cordas de cada costura até o ponto de junção e as 4 cintas até os cantos da tampa."""
    p = Part(PREFIX + '_Canopy')
    for k in range(GORES):
        gore(p, ('Player' if k % 2 == 0 else 'Paint') + '#gore', k)
    p.cyl('Trim', (0.0, 0.0, CANOPY_SKIRT + CANOPY_DOME * math.cos(0.12) - 0.01), 'z', CANOPY_R * math.sin(0.12) + 0.06, 0.06, seg=16, bevel=0.0)
    for k in range(GORES):
        th = 2 * math.pi * k / GORES
        top = canopy_point(math.pi / 2, th)
        top = (top[0], top[1], top[2] + 0.18)
        bot = (0.0, 0.0, CONFLUENCE)
        d = (top[0] - bot[0], top[1] - bot[1], top[2] - bot[2])
        L = math.sqrt(sum(c * c for c in d))
        p.cyl('Paint', tuple((a + c) / 2 for a, c in zip(top, bot)), d, 0.022, L, seg=6, bevel=0.0)
    p.box('Dark', -0.08, 0.08, -0.08, 0.08, CONFLUENCE - 0.1, CONFLUENCE + 0.06, bevel=0.0)
    for sx in (-1, 1):
        for sy in (-1, 1):
            q = (sx * (CRATE_L / 2 - 0.08), sy * (CRATE_W / 2 - 0.08), LID_TOP)
            d = (0.0 - q[0], 0.0 - q[1], CONFLUENCE - 0.1 - q[2])
            L = math.sqrt(sum(c * c for c in d))
            p.cyl('Dark', (q[0] + d[0] / 2, q[1] + d[1] / 2, q[2] + d[2] / 2), d, 0.03, L, seg=6, bevel=0.0)
    return p.finish()


def main():
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'supply_drop')
    os.makedirs(out, exist_ok=True)
    clear_scene()
    total = export(crate(), out)
    clear_scene()
    total += export(canopy(), out)
    print('MODEL total', total, 'tris; caixa', CRATE_L, 'x', CRATE_W, 'x', BAG_TOP, 'm; paraquedas', 2 * CANOPY_R, 'm, topo a',
          round(CANOPY_SKIRT + CANOPY_DOME, 2), 'm')


if __name__ == '__main__':
    main()
