"""A caixa do lançamento aéreo (supply_drop), VERSÃO B no detalhe AAA (01/11/2026): a versão A (Game/Tools/model_supply_crate.py,
aprovada com a Drop Zone em 23/10) fica como está; esta é a que ele compara com ela.

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice), na classe VEHICLE (a
caixa tem 1,2 m: a grade de 14 cm e a sombra de contato de 1,1 m são as da peça pequena):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_supply_crate_v2.py C:/Users/luisf/explosion/captures/game/models/supply_drop_v2

O mesmo desenho e as mesmas medidas do A (1,2 × 1,0 m; a plataforma com os patins até 0,10, o corpo até 0,85, a tampa até 0,93, o
saco do paraquedas até 1,17; o paraquedas de 5,2 m com 12 gomos, a cor do jogador e o branco), refeitos peça por peça na escala de 2 px
(docs/ESTUDO_DETALHE_AAA.md):
  - a caixa de compensado olive com o QUADRO de ripas em relevo (a cinta de baixo, a de cima e as 2 de pé por baixo das cintas), as
    cantoneiras de aço arredondadas com os parafusos, a tampa com a aba e as travas, a faixa amarela (o símbolo do dinheiro) que dobra
    nas pontas, as alças de tubo nas cabeceiras, o estêncil branco;
  - a plataforma de aço com os 3 patins de ponta chanfrada e os olhais de amarração;
  - as 2 cintas de carga que abraçam a caixa (a faixa com espessura, as quinas redondas) com a catraca;
  - o saco do paraquedas: a lona DOBRADA em fardo com as cintas (model_vehicle_parts.tarp_bundle), não a caixa lisa;
  - o paraquedas: os gomos que estufam entre as costuras, as fitas de reforço em cima das costuras, a bainha da barra, o anel do furo
    do topo com as linhas cruzadas, as 12 linhas até o elo de junção (o anel de aço) e os 4 tirantes chatos até os cantos da tampa.
A USA limpa (13/10): sem desgaste pintado; o pó vem do material do jogo.

Peças (os NOMES e a origem do A: no chão, no centro da caixa; o paraquedas com a MESMA origem):
  SM_SupplyDrop_Crate, SM_SupplyDrop_Canopy.
Slots (os do A): Crate, Yellow, Dark, Trim, Paint, Player, CivCloth.
Módulo: crate_into(p, (x, y, z, giro), bag) põe a caixa numa peça de outro modelo (a pilha do model_drop_zone_v2.py), como no A.
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402

PREFIX = 'SM_SupplyDrop'
COLORS.update({'Crate': (96, 98, 62), 'Yellow': (214, 164, 32), 'Dark': (40, 40, 38), 'Trim': (70, 70, 68),
               'Paint': (222, 222, 216), 'Player': (86, 108, 150), 'CivCloth': (150, 128, 96)})

# ---------------------------------------------------------------------------------- as medidas do A (mantidas)
CRATE_L, CRATE_W = 1.2, 1.0       # a caixa: comprimento (x), largura (y)
SKID_TOP = 0.1                    # a plataforma com os patins
BODY_TOP, LID_TOP = 0.85, 0.93    # o corpo e a tampa
BAG_TOP = 1.17                    # o saco do paraquedas em cima
CANOPY_R, CANOPY_DOME, CANOPY_SKIRT = 2.6, 1.5, 4.6     # o paraquedas: raio, altura da cúpula, a barra (acima do chão da caixa)
GORES = 12
CONFLUENCE = 2.1                  # onde as cordas se juntam (acima da caixa)
# o chanfro das peças da caixa: < 1,5 cm vira 2× no harden (2,4 cm, 3 segmentos) nas DUAS classes do executor; a caixa da pilha da
# Drop Zone (classe building, chanfro mínimo 9 cm) sai igual à que cai
BEV = 0.012


class Frame:
    """O referencial local da caixa: f = (x, y, z, giro em graus, Unreal)."""

    def __init__(self, f):
        self.c = Vector(f[:3])
        self.yaw = f[3]
        a = math.radians(f[3])
        self.ex = Vector((math.cos(a), math.sin(a), 0.0))
        self.ey = Vector((-math.sin(a), math.cos(a), 0.0))
        self.ez = Vector((0.0, 0.0, 1.0))

    def P(self, x, y, z):
        return self.c + self.ex * x + self.ey * y + self.ez * z

    def D(self, x, y, z):
        return self.ex * x + self.ey * y + self.ez * z


def _keys(p):
    for k, b in (('Crate#c', BEV), ('Crate#lid', BEV), ('Dark#c', BEV), ('Trim#c', BEV)):
        p.bm(k, b)


def band(p, m, pts, wdir, width, thick, center):
    """A cinta chata pela polilinha pts (Unreal): a largura ao longo de wdir, a espessura pra fora do 'center' (a caixa que ela
    abraça). Loft de 4 cantos por ponto, com as tampas."""
    W = Vector(wdir).normalized()
    C = Vector(center)
    P = [Vector(q) for q in pts]
    rings = []
    for i, q in enumerate(P):
        t = (P[min(i + 1, len(P) - 1)] - P[max(i - 1, 0)]).normalized()
        n = W.cross(t).normalized()
        if n.dot(q - C) < 0:
            n = -n
        rings.append([tuple(q + W * (s * width / 2) + n * (h * thick)) for s, h in ((-1, 0.0), (1, 0.0), (1, 1.0), (-1, 1.0))])
    p.loft(m, rings, bevel=0.0, smooth=False)


def crate_into(p, f=(0.0, 0.0, 0.0, 0.0), bag=True):
    """A caixa do lançamento no referencial f = (x, y, z, giro): a plataforma com os patins, o corpo de compensado com o quadro de
    ripas, as cantoneiras, a tampa com a faixa amarela, as alças, as travas, as 2 cintas e (bag) o saco do paraquedas."""
    _keys(p)
    F = Frame(f)
    yaw = F.yaw
    hx, hy = CRATE_L / 2, CRATE_W / 2
    # ---------------------------------------------------------------- a plataforma de aço, os 3 patins, os olhais
    vp.rbox(p, 'Trim#c', F.P(0, 0, 0.0175), (CRATE_L + 0.06, CRATE_W + 0.06, 0.035), yaw=yaw, r=0.05, k=4)
    skid = [(-hx, 0.033), (hx, 0.033), (hx, 0.07), (hx - 0.04, SKID_TOP + 0.004), (-hx + 0.04, SKID_TOP + 0.004), (-hx, 0.07)]
    for y in (-0.38, 0.0, 0.38):
        vp.prism(p, 'Dark#c', skid, F.P(0, y - 0.06, 0), F.ex, F.ez, F.ey, 0.12)
    for x in (-0.36, 0.36):                                       # os olhais de amarração na borda da plataforma
        for s in (-1, 1):
            p.torus('Trim#vp', tuple(F.P(x, s * (hy + 0.045), 0.05)), 'y' if abs(F.ey.y) > 0.5 else 'x', 0.038, 0.011, seg=12, rseg=6)
    # ---------------------------------------------------------------- o corpo: o compensado recuado e o quadro de ripas em relevo
    vp.rbox(p, 'Crate#c', F.P(0, 0, (SKID_TOP + BODY_TOP) / 2), (CRATE_L - 0.04, CRATE_W - 0.04, BODY_TOP - SKID_TOP), yaw=yaw, r=0.03, k=4)
    # (a caixa reta do harden: o chanfro de 3 segmentos arredonda TODAS as quinas e custa metade do perfil arredondado + chanfro)
    for (z0, z1) in ((SKID_TOP, SKID_TOP + 0.1), (BODY_TOP - 0.1, BODY_TOP)):
        p.obox('Crate#c', tuple(F.P(0, 0, (z0 + z1) / 2)), (CRATE_L, CRATE_W, z1 - z0), yaw=yaw)
    for x in (-0.3, 0.3):                                         # as ripas de pé dos lados compridos (as cintas passam por cima)
        for s in (-1, 1):
            p.obox('Crate#c', tuple(F.P(x, s * (hy - 0.005), (SKID_TOP + BODY_TOP) / 2)), (0.1, 0.05, BODY_TOP - SKID_TOP - 0.04), yaw=yaw)
    for s in (-1, 1):                                             # a ripa de pé no meio das cabeceiras
        p.obox('Crate#c', tuple(F.P(s * (hx - 0.005), 0, (SKID_TOP + BODY_TOP) / 2)), (0.05, 0.1, BODY_TOP - SKID_TOP - 0.04), yaw=yaw)
    # as cantoneiras de aço (a peça em L que abraça a quina, de quina redonda) com os parafusos
    for sx in (-1, 1):
        for sy in (-1, 1):
            cx, cy = sx * (hx - 0.05), sy * (hy - 0.05)
            vp.rbox(p, 'Dark#c', F.P(cx, cy, (SKID_TOP + BODY_TOP) / 2 + 0.005), (0.15, 0.15, BODY_TOP - SKID_TOP + 0.03), yaw=yaw,
                    r=0.05, k=4)
            for z in (0.22, 0.72):
                vp.nut(p, 'Trim', F.P(cx - sx * 0.02, sy * (hy + 0.025), z), F.D(0, sy, 0), r=0.016, h=0.012)
                vp.nut(p, 'Trim', F.P(sx * (hx + 0.025), cy - sy * 0.02, z), F.D(sx, 0, 0), r=0.016, h=0.012)
    # o estêncil branco dos lados compridos (entre as ripas de pé) com a marca escura
    for s in (-1, 1):
        vp.rbox(p, 'Paint#vp', F.P(0, s * (hy - 0.016), 0.48), (0.4, 0.012, 0.24), yaw=yaw, r=0.03, k=2)
        vp.rbox(p, 'Dark#vp', F.P(0, s * (hy - 0.008), 0.48), (0.26, 0.008, 0.07), yaw=yaw, r=0.015, k=2)
    # as alças de tubo nas cabeceiras
    for s in (-1, 1):
        vp.grab_handle(p, 'Trim', F.P(s * (hx + 0.02), -0.16, 0.6), F.P(s * (hx + 0.02), 0.16, 0.6), F.D(s, 0, 0), height=0.07,
                       r=0.016, R=0.03)
    # ---------------------------------------------------------------- a tampa: a placa com a aba, a faixa amarela que dobra nas pontas
    vp.rbox(p, 'Crate#lid', F.P(0, 0, (BODY_TOP + LID_TOP) / 2), (CRATE_L + 0.04, CRATE_W + 0.04, LID_TOP - BODY_TOP), yaw=yaw,
            r=0.05, k=4)
    vp.rbox(p, 'Yellow#vp', F.P(0, 0, LID_TOP + 0.004), (CRATE_L - 0.02, 0.32, 0.012), yaw=yaw, r=0.02, k=2)
    for s in (-1, 1):
        vp.rbox(p, 'Yellow#vp', F.P(s * (hx + 0.024), 0, (BODY_TOP + LID_TOP) / 2 + 0.004), (0.012, 0.32, LID_TOP - BODY_TOP + 0.01),
                yaw=yaw, r=0.004, k=1)
        # o filete escuro dos dois lados da faixa (o A tinha 1 cm; 3 cm leem na câmera)
        vp.rbox(p, 'Dark#vp', F.P(0, s * 0.175, LID_TOP + 0.005), (CRATE_L - 0.02, 0.03, 0.012), yaw=yaw, r=0.012, k=2)
    # as travas da tampa (2 por lado comprido): o fecho de alavanca
    for x in (-0.42, 0.42):
        for s in (-1, 1):
            p.obox('Trim#c', tuple(F.P(x, s * (hy + 0.03), BODY_TOP - 0.02)), (0.08, 0.03, 0.12), yaw=yaw)
            vp.lathe(p, 'Trim', F.P(x, s * (hy + 0.05), BODY_TOP - 0.06), F.ex, [(0.0, -0.035), (0.016, -0.035), (0.016, 0.035),
                                                                                (0.0, 0.035)], seg=8)
    # ---------------------------------------------------------------- as 2 cintas de carga abraçando a caixa, com a catraca
    for x in (-0.3, 0.3):
        y_out = hy + 0.032
        path = vp.bent_bar([F.P(x, -y_out, SKID_TOP + 0.02), F.P(x, -y_out, LID_TOP + 0.012), F.P(x, y_out, LID_TOP + 0.012),
                            F.P(x, y_out, SKID_TOP + 0.02)], R=0.05, n=4)
        band(p, 'Dark#vp', path, F.ex, 0.075, 0.01, F.P(x, 0, 0.5))
        p.obox('Trim#c', tuple(F.P(x, y_out + 0.018, 0.34)), (0.1, 0.035, 0.14), yaw=yaw)                  # a catraca
        vp.lathe(p, 'Trim', F.P(x, y_out + 0.04, 0.37), F.ex, [(0.0, -0.06), (0.022, -0.06), (0.022, 0.06), (0.0, 0.06)], seg=10)
    # ---------------------------------------------------------------- o saco do paraquedas: a lona dobrada em fardo com as cintas
    if bag:
        vp.tarp_bundle(p, tuple(F.P(0, 0, LID_TOP + 0.012)), (0.8, 0.64, BAG_TOP - LID_TOP - 0.01), yaw=yaw, m='CivCloth',
                       strap_m='Dark', buckle_m='Trim', straps=2, layers=2, seed=3.0, n=16, k=18, sag_ends=0.18)


def crate():
    p = Part(PREFIX + '_Crate')
    crate_into(p)
    return p.finish()


# ---------------------------------------------------------------------------------- o paraquedas
def canopy_point(ph, th, inner=False):
    """Um ponto da cúpula (o mesmo do A): ph do furo do topo (0) até a barra (pi/2), th em volta; o pano estufa entre as costuras."""
    fr = (th / (2 * math.pi / GORES)) % 1.0
    bulge = 1 + 0.06 * math.sin(math.pi * fr) * math.sin(ph)
    r = CANOPY_R * math.sin(ph) * bulge
    z = CANOPY_SKIRT + CANOPY_DOME * math.cos(ph)
    if inner:
        r *= 0.985
        z -= 0.035
    return Vector((r * math.cos(th), r * math.sin(th), z))


PH0 = 0.12                         # o furo do topo


def gore(p, key, k, rows=12, cols=6):
    """Um gomo da cúpula como casca fechada (fora, dentro e as 4 bordas), liso; a barra faz o recorte de vieira."""
    b = p.bm(key, 0.0)
    a0 = 2 * math.pi * k / GORES
    da = 2 * math.pi / GORES

    def grid(inner):
        g = []
        for i in range(rows + 1):
            ph = PH0 + (math.pi / 2 - PH0) * i / rows
            row = []
            for j in range(cols + 1):
                th = a0 + da * j / cols
                q = canopy_point(ph, th, inner)
                if i == rows:
                    q.z += 0.18 * (1 - math.sin(math.pi * j / cols))          # a costura levanta: a vieira da barra
                row.append(b.verts.new(V(*q)))
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


def seam_tape(p, th, rows=12):
    """A fita de reforço em cima da costura (6 cm, 1 cm acima do pano), do furo do topo até a barra."""
    pts = []
    for i in range(rows + 1):
        ph = PH0 + (math.pi / 2 - PH0) * i / rows
        q = canopy_point(ph, th)
        if i == rows:
            q.z += 0.18
        pts.append(q)
    rings = []
    for i, q in enumerate(pts):
        t = (pts[min(i + 1, rows)] - pts[max(i - 1, 0)]).normalized()
        out = (q - Vector((0.0, 0.0, CANOPY_SKIRT - 0.6))).normalized()
        side = t.cross(out).normalized()
        nrm = side.cross(t).normalized()
        if nrm.dot(out) < 0:
            nrm = -nrm
        rings.append([tuple(q + side * (s * 0.035) + nrm * h) for s, h in ((-1, -0.004), (1, -0.004), (1, 0.014), (-1, 0.014))])
    p.loft('Paint#tape', rings, bevel=0.0, smooth=False)


def canopy():
    """O paraquedas aberto, com a mesma origem da caixa: 12 gomos (cor do jogador e branco), as fitas das costuras, a bainha, o anel
    do furo do topo com as linhas cruzadas, as 12 linhas até o elo e os 4 tirantes até os cantos da tampa."""
    p = Part(PREFIX + '_Canopy')
    for k in range(GORES):
        gore(p, ('Player' if k % 2 == 0 else 'Paint') + '#gore', k)
    for k in range(GORES):
        seam_tape(p, 2 * math.pi * k / GORES)
    # a bainha da barra: o cordão grosso que segue a vieira (as pontas das costuras ficam 18 cm mais altas)
    hem = []
    for k in range(GORES * 8 + 1):
        th = 2 * math.pi * k / (GORES * 8)
        fr = (th / (2 * math.pi / GORES)) % 1.0
        q = canopy_point(math.pi / 2, th)
        q.z += 0.18 * (1 - math.sin(math.pi * fr))
        hem.append(q * 1.0 + Vector((0, 0, 0)))
    hem = [Vector((q.x * 1.004, q.y * 1.004, q.z)) for q in hem]
    vp.tube(p, 'Paint', hem, 0.025, seg=6, caps=False)
    # o anel do furo do topo e as linhas cruzadas do furo
    ztop = CANOPY_SKIRT + CANOPY_DOME * math.cos(PH0)
    rtop = CANOPY_R * math.sin(PH0)
    p.torus('Trim#vp', (0.0, 0.0, ztop - 0.005), 'z', rtop, 0.03, seg=20, rseg=6)
    for k in range(3):
        a = math.pi * k / 3
        vp.tube(p, 'Paint', [(rtop * math.cos(a), rtop * math.sin(a), ztop - 0.02), (0.0, 0.0, ztop - 0.07),
                             (-rtop * math.cos(a), -rtop * math.sin(a), ztop - 0.02)], 0.012, seg=5)
    # as linhas de suspensão, da ponta de cada costura até o elo de junção
    conf = Vector((0.0, 0.0, CONFLUENCE))
    for k in range(GORES):
        th = 2 * math.pi * k / GORES
        top = canopy_point(math.pi / 2, th)
        top.z += 0.18
        vp.tube(p, 'Paint', [top, conf + (top - conf).normalized() * 0.09], 0.016, seg=6)
    # o elo de junção (o anel de aço) e os 4 tirantes chatos até os cantos da tampa
    p.torus('Trim#vp', (0.0, 0.0, CONFLUENCE), 'x', 0.075, 0.02, seg=14, rseg=6)
    vp.lathe(p, 'Trim', (0.0, 0.0, CONFLUENCE - 0.16), 'z', [(0.0, 0.0), (0.045, 0.0), (0.05, 0.03), (0.05, 0.08), (0.03, 0.1),
                                                             (0.0, 0.1)], seg=10)
    for sx in (-1, 1):
        for sy in (-1, 1):
            q = Vector((sx * (CRATE_L / 2 - 0.08), sy * (CRATE_W / 2 - 0.08), LID_TOP + 0.01))
            top = Vector((0.0, 0.0, CONFLUENCE - 0.16))
            d = (top - q).normalized()
            w = Vector((-sy, sx, 0.0)).normalized()
            band(p, 'Dark#vp', [q, top - d * 0.02], w, 0.05, 0.008, Vector((0.0, 0.0, 0.0)) + w.cross(d) * -5.0)
            vp.rbox(p, 'Trim', tuple(q + Vector((0, 0, 0.02))), (0.07, 0.07, 0.04), yaw=math.degrees(math.atan2(sy, sx)), r=0.012, k=1)
    return p.finish()


def write_spec(out):
    """As folhas da prévia: a caixa com o paraquedas (a do A, supply_drop) e a caixa sozinha (supply_drop_crate)."""
    import json
    spec = {'parts': [[PREFIX + '_Crate', [0, 0, 0]], [PREFIX + '_Canopy', [0, 0, 0]]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1000, 1000]],
                      ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [900, 1100]],
                      ['three_quarter', 'persp', 30.0, 215.0, [1200, 1200], 35.0, 0.95]],
            'usa_at': [0.0, 5.5, 0], 'game': [['game_cam', 37.5, 200.0, 50.0, 0.8]]}
    with open(os.path.join(out, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)
    spec = {'parts': [[PREFIX + '_Crate', [0, 0, 0]]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [900, 900]],
                      ['three_quarter', 'persp', 30.0, 215.0, [1200, 900], 35.0, 0.9],
                      ['three_quarter_right', 'persp', 30.0, 145.0, [1200, 900], 35.0, 0.9]],
            'usa_at': [0.0, 3.5, 0], 'game': [['game_cam_close', 37.5, 200.0, 28.0, 0.75]]}
    with open(os.path.join(out, 'preview_crate.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)


def main():
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'supply_drop_v2')
    os.makedirs(out, exist_ok=True)
    clear_scene()
    c = crate()
    total = export(c, out)
    clear_scene()
    cn = canopy()
    total += export(cn, out)
    print('MODEL total', total, 'tris (antes da grade do executor); caixa', CRATE_L, 'x', CRATE_W, 'x', BAG_TOP, 'm; paraquedas',
          2 * CANOPY_R, 'm, topo a', round(CANOPY_SKIRT + CANOPY_DOME, 2), 'm')
    write_spec(out)


if __name__ == '__main__':
    main()
