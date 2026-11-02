"""As miudezas de rua, de quintal e de telhado do kit civil v2 (01/11/2026; docs/KIT_CIVIL_V2.md), uma GLB por peça.

Rodar (Blender sem janela, só CPU; a classe 'vehicle' dá a grade fina de 14 cm e a sombra de contato de 1,1 m: são peças pequenas):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_civ_props.py C:/Users/luisf/explosion/captures/game/models/civ_v2/props [nome1,nome2...]

As peças (a frente/o comprido em +Y quando tem; origem no centro do pé, no chão) e de onde vêm (os recortes em docs/KIT_CIVIL_V2.md):
  SM_civ_barrels          SUBSTITUI o civ_barrels (2 × 2 m): os tambores de aço de 200 L (os aros de rolar, as tampas com os bujões),
                          um deitado, a bombona de plástico (16_beauty_pass/b_gpt: os tambores azuis e cinzas na esquina);
  SM_civ_crates           SUBSTITUI o civ_crates (2,4 × 2,4 m): o palete, os caixotes de ripa com as travessas, a caixa de papelão e a
                          lona jogada por cima (16_beauty_pass/b_gpt, as pilhas no pátio);
  SM_civ_sandbags         SUBSTITUI o civ_sandbags (4 × 1,6 m): o murete de sacos em 3 fiadas desencontradas, as pontas dobradas;
  SM_civ_tires            a pilha de pneus velhos (2 pilhas + 2 encostados);
  SM_civ_water_tank       a caixa d'água de plástico na base (a do teto, solta: o gerador da vila põe nas lajes);
  SM_civ_water_tank_stand a mesma no cavalete alto;
  SM_civ_tank_horizontal  a caixa d'água de aço deitada no cavalete;
  SM_civ_dish             a antena parabólica no pé;
  SM_civ_roof_ac          a condensadora;
  SM_civ_laundry          o varal com os 2 postes e as roupas (2,4 m);
  SM_civ_awning_wall      o toldo de parede com as mãos-francesas (3 × 1,6 m, pendurado a ~2,45 m, sai pra +Y);
  SM_civ_fence_corr       o tapume de chapa ondulada enferrujada nos paus (4 m; town_battle_top_gemini: as barricadas de chapa);
  SM_civ_jersey           a barreira de concreto (New Jersey, 3 m; street_battle_usa_china_gemini: os blocos da rua);
  SM_civ_dumpster         a caçamba de lixo de aço com os sacos;
  SM_civ_streetlamp       o poste de luz de rua de braço (7 m);
  SM_civ_market_stall     a banca da feira (a mesa, o toldo de pano, as caixas de fruta);
  SM_civ_rubble           o monte de entulho solto (3 × 3 m).
"""
import math
import os
import sys

from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import Part, V, clear_scene, export  # noqa: E402
import model_civ_house_parts as hp  # noqa: E402
import model_civ_parts as cp  # noqa: E402
from model_building_parts import bake_in_place, hexa, tarp  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402

ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
OUT = ARGS[0] if ARGS else os.path.join(os.getcwd(), 'civ_v2', 'props')
ONLY = ARGS[1].split(',') if len(ARGS) > 1 else None
os.makedirs(OUT, exist_ok=True)


# ===================================================================================================== peças
def drum(p, c, r=0.29, h=0.88, m='Rust#drum', lying=False, yaw=0.0, seed=0):
    """O tambor de aço de 200 L: o corpo com os 2 aros de rolar e as bordas viradas, a tampa rebaixada com o bujão grande e o pequeno."""
    x, y, z = c
    if lying:
        ex, ey, ez = vp.ypr_frame(yaw)
        A = ex
        C = Vector((x, y, z + r + 0.01))
    else:
        A = Vector((0, 0, 1))
        C = Vector((x, y, z + h / 2))
    prof = [(0.0, -h / 2 + 0.015), (r - 0.02, -h / 2 + 0.015), (r - 0.012, -h / 2), (r + 0.004, -h / 2 + 0.005), (r + 0.004, -h / 2 + 0.03),
            (r, -h / 2 + 0.035)]
    for zz in (-h / 6, h / 6):
        prof += [(r, zz - 0.03), (r + 0.016, zz - 0.012), (r + 0.016, zz + 0.012), (r, zz + 0.03)]
    prof += [(r, h / 2 - 0.035), (r + 0.004, h / 2 - 0.03), (r + 0.004, h / 2 - 0.005), (r - 0.012, h / 2), (r - 0.02, h / 2 - 0.015),
             (0.0, h / 2 - 0.015)]
    vp.lathe(p, m, tuple(C), tuple(A), prof, seg=24, smooth=True, hard=40.0)
    if not lying:
        for (dx, rr) in ((0.15, 0.04), (-0.17, 0.022)):
            vp.lathe(p, 'Dark#bung', (x + dx, y + 0.04, z + h - 0.015), 'z', [(0.0, 0.0), (rr, 0.0), (rr, 0.012), (0.0, 0.012)], seg=10)


def plastic_drum(p, c, r=0.28, h=0.92, m='Dark#plastic'):
    """A bombona de plástico (o tambor azul da região: aqui no escuro do plástico), os frisos, a tampa e as 2 alças moldadas."""
    x, y, z = c
    prof = [(0.0, 0.0), (r - 0.03, 0.0), (r, 0.03), (r, 0.18), (r - 0.012, 0.2), (r - 0.012, 0.24), (r, 0.26), (r, h - 0.26),
            (r - 0.012, h - 0.24), (r - 0.012, h - 0.2), (r, h - 0.18), (r, h - 0.05), (r - 0.04, h), (0.0, h)]
    vp.lathe(p, m, (x, y, z), 'z', prof, seg=24, smooth=True, hard=40.0)
    vp.lathe(p, m + 'cap', (x + 0.1, y, z + h), 'z', [(0.0, 0.0), (0.05, 0.0), (0.05, 0.025), (0.0, 0.025)], seg=10)


def crate(p, c, size, yaw=0.0, m='Wood#crate', slats=4, seed=0):
    """O caixote de ripa: as 4 cantoneiras de sarrafo, as ripas de cada face com a fresta (o escuro de dentro aparece), as travessas
    da tampa."""
    x, y, z = c
    L, W, H = size
    ex, ey, ez = vp.ypr_frame(yaw)
    C = Vector((x, y, z))
    vp.rbox(p, 'Dark#in', tuple(C + ez * (H / 2)), (L - 0.06, W - 0.06, H - 0.04), yaw=yaw, r=0.01, k=1)
    for sx in (-1, 1):
        for sy in (-1, 1):
            q = C + ex * (sx * (L / 2 - 0.03)) + ey * (sy * (W / 2 - 0.03)) + ez * (H / 2)
            vp.rbox(p, m, tuple(q), (0.06, 0.06, H), yaw=yaw, r=0.01, k=1)
    sh = (H - 0.02) / slats
    for k in range(slats):
        zz = 0.01 + sh * (k + 0.5)
        for s in (-1, 1):
            vp.rbox(p, m, tuple(C + ey * (s * (W / 2 - 0.012)) + ez * zz), (L - 0.02, 0.022, sh * 0.78), yaw=yaw, r=0.006, k=1)
            vp.rbox(p, m, tuple(C + ex * (s * (L / 2 - 0.012)) + ez * zz), (0.022, W - 0.02, sh * 0.78), yaw=yaw, r=0.006, k=1)
    n = max(3, int(L / 0.12))
    for k in range(n):
        u = -L / 2 + 0.04 + (L - 0.08) * (k + 0.5) / n
        vp.rbox(p, m, tuple(C + ex * u + ez * (H - 0.01)), ((L - 0.08) / n * 0.82, W - 0.02, 0.022), yaw=yaw, r=0.006, k=1)


def pallet(p, c, yaw=0.0, m='Wood#pallet'):
    """O palete (1,2 × 1,0 m): as 3 longarinas de bloco, as tábuas de cima com a fresta, as de baixo."""
    x, y, z = c
    ex, ey, ez = vp.ypr_frame(yaw)
    C = Vector((x, y, z))
    for s in (-1, 0, 1):
        vp.rbox(p, m, tuple(C + ey * (s * 0.44) + ez * 0.06), (1.2, 0.1, 0.08), yaw=yaw, r=0.01, k=1)
        for u in (-0.5, 0.0, 0.5):
            vp.rbox(p, m, tuple(C + ex * u + ey * (s * 0.44) + ez * 0.06), (0.14, 0.12, 0.1), yaw=yaw, r=0.01, k=1)
    for k in range(7):
        u = -0.55 + 1.1 * k / 6
        vp.rbox(p, m, tuple(C + ex * u + ez * 0.12), (0.12, 1.0, 0.022), yaw=yaw, r=0.006, k=1)
    for u in (-0.55, 0.0, 0.55):
        vp.rbox(p, m, tuple(C + ex * u + ez * 0.011), (0.12, 1.0, 0.022), yaw=yaw, r=0.006, k=1)


def old_tyre(p, c, R=0.33, w=0.2, axis='z', m='Rubber#old'):
    """O pneu velho solto (sem roda), oco: o do hp.loose_tyre."""
    hp.loose_tyre(p, c, R=R, w=w, axis=axis, m=m)


def bags_row(p, pts, z0, layers=3, size=(0.62, 0.36, 0.22), seed=0, m='Cloth#sand'):
    """O murete de sacos de areia: as fiadas desencontradas, cada saco com a sua barriga e as pontas dobradas pra dentro."""
    for lay in range(layers):
        for i, (a, b) in enumerate(zip(pts[:-1], pts[1:])):
            A, B = Vector(a), Vector(b)
            L = (B - A).length
            n = max(1, int(math.ceil(L / (size[0] * 0.86))))          # encostados (o saco cheio se espreme no vizinho)
            yaw = math.degrees(math.atan2(B.y - A.y, B.x - A.x))
            off = 0.5 if lay % 2 else 0.0
            for k in range(n + (1 if lay % 2 else 0)):
                u = (k + 0.5 - off) / n
                if u < 0.02 or u > 0.98:
                    continue
                q = A + (B - A) * u
                inset = lay * 0.035
                cp.bag(p, (q.x, q.y, z0 + lay * size[2] * 0.82), (size[0], size[1] - inset, size[2]), yaw=yaw + (cp.h3(k, lay, seed) - 0.5) * 8,
                       m=m, seed=seed * 100 + lay * 10 + k, pillow=True)


# ===================================================================================================== as GLBs
def barrels():
    p = Part('SM_civ_barrels')
    drum(p, (-0.42, -0.35, 0.0), m='Rust#drum')
    drum(p, (0.2, -0.45, 0.0), m='Metal#drum')
    drum(p, (-0.1, 0.18, 0.0), m='Crate#drum')
    plastic_drum(p, (0.55, 0.25, 0.0))
    drum(p, (-0.35, 0.72, 0.0), m='Rust#drum', lying=True, yaw=8)
    return p.finish()


def crates():
    p = Part('SM_civ_crates')
    pallet(p, (-0.45, -0.4, 0.0), yaw=5)
    crate(p, (-0.72, -0.42, 0.135), (0.62, 0.48, 0.42), yaw=5)
    crate(p, (-0.12, -0.38, 0.135), (0.62, 0.48, 0.42), yaw=3)
    crate(p, (-0.42, -0.4, 0.555), (0.62, 0.48, 0.42), yaw=12)
    crate(p, (0.55, 0.5, 0.0), (0.7, 0.55, 0.5), yaw=-20, m='Wood#crate2')
    crate(p, (0.45, 0.45, 0.5), (0.55, 0.45, 0.38), yaw=-8)
    vp.rbox(p, 'Cloth#box', (-0.55, 0.55, 0.22), (0.5, 0.42, 0.44), yaw=30, r=0.02, k=1)
    vp.rbox(p, 'Cloth#box', (-0.5, 0.55, 0.55), (0.42, 0.36, 0.22), yaw=24, r=0.02, k=1)
    tarp(p, [(-0.75, -0.7, 1.0), (-0.1, -0.68, 1.0), (-0.05, -0.08, 0.98), (-0.78, -0.1, 0.99)], m='Cloth#tarp', sag=0.02, folds=3,
         amp=0.03, thick=0.012, seed=2)
    return p.finish()


def sandbags():
    """O murete de sacos (4 × 1,6 m): a fiada da frente com 4 camadas desencontradas e a de trás com 3, encostadas (a parede de
    sacos: de cima o ritmo das costuras, de lado os degraus das camadas), a curva leve."""
    p = Part('SM_civ_sandbags')
    bags_row(p, [(-1.75, -0.18), (-0.2, 0.0), (1.75, -0.16)], 0.0, layers=4, size=(0.6, 0.36, 0.2), seed=3)
    bags_row(p, [(-1.6, 0.18), (-0.2, 0.36), (1.6, 0.2)], 0.0, layers=3, size=(0.6, 0.36, 0.2), seed=4)
    return p.finish()


def tires():
    p = Part('SM_civ_tires')
    for k in range(4):
        old_tyre(p, (-0.35 + 0.02 * (k % 2), 0.0 + 0.03 * k, 0.1 + 0.2 * k), axis='z')
    for k in range(2):
        old_tyre(p, (0.45, -0.25 + 0.02 * k, 0.1 + 0.2 * k), axis='z')
    t = Part('lean')
    old_tyre(t, (0.0, 0.0, 0.0), axis='y')
    to = t.finish()
    to.data.transform(Matrix.Translation(V(0.35, 0.42, 0.33)) @ Matrix.Rotation(math.radians(-18), 4, 'X'))
    o = p.finish()
    return cp.join([o, to], 'SM_civ_tires')


def water_tank():
    p = Part('SM_civ_water_tank')
    hp.water_tank(p, (0.0, 0.0, 0.0), r=0.55, h=1.15, stand=0.0)
    return p.finish()


def water_tank_stand():
    p = Part('SM_civ_water_tank_stand')
    hp.water_tank(p, (0.0, 0.0, 0.0), r=0.55, h=1.15, stand=1.3)
    return p.finish()


def tank_h():
    p = Part('SM_civ_tank_horizontal')
    hp.tank_horizontal(p, (0.0, 0.0, 0.0), r=0.42, L=1.6, yaw=90.0, stand=0.55)
    return p.finish()


def dish():
    p = Part('SM_civ_dish')
    hp.sat_dish(p, (0.0, 0.0, 0.0), r=0.45, yaw=90.0, tilt=34.0)
    return p.finish()


def roof_ac():
    p = Part('SM_civ_roof_ac')
    hp.roof_ac(p, (0.0, 0.0, 0.0), yaw=0.0)
    return p.finish()


def laundry():
    p = Part('SM_civ_laundry')
    hp.laundry(p, (-1.22, 0.0), (1.22, 0.0), 0.0, seed=12)
    return p.finish()


def awning_wall():
    p = Part('SM_civ_awning_wall')
    hp.awning(p, -1.5, 1.5, 0.0, 1.6, 2.45, 2.15, m='Cloth#awn', posts=False, seed=7)
    return p.finish()


def fence_corr():
    """O tapume de chapas onduladas enferrujadas de tamanhos e alturas diferentes, pregadas nos paus e nas 2 travessas, uma torta e
    uma remendada (town_battle_top_gemini: as barricadas)."""
    p = Part('SM_civ_fence_corr')
    xs = [-2.0, -1.12, -0.2, 0.75, 1.55, 2.0]
    for k, (a, b) in enumerate(zip(xs[:-1], xs[1:])):
        h = 1.85 + 0.35 * cp.h3(k, 1, 3)
        lean = (cp.h3(k, 2, 3) - 0.5) * 0.16
        m = 'Rust#sheet' if cp.h3(k, 3, 3) > 0.25 else 'Metal#sheet'
        hp.corr_panel(p, (a - 0.04, 0.02 * (k % 2)), (b + 0.04, 0.02 * (k % 2)), 0.02, h, m=m, pitch=0.075, amp=0.018, lean=lean)
    for x in (-1.95, -0.65, 0.65, 1.95):
        vp.rbox(p, 'Wood#post', (x, -0.08, 1.0), (0.1, 0.1, 2.1), yaw=0, r=0.02, k=1)
    for z in (0.45, 1.55):
        vp.rbox(p, 'Wood#rail', (0.0, -0.065, z), (4.0, 0.06, 0.09), r=0.015, k=1)
    # o remendo e os escoramentos
    hp.corr_panel(p, (-0.6, 0.05), (0.15, 0.05), 0.6, 1.3, m='Metal#sheet', pitch=0.075, amp=0.018)
    for x in (-1.3, 1.3):
        vp.tube(p, 'Wood#post', [(x, -0.1, 1.4), (x, -1.0, 0.0)], 0.04, seg=6)
    return p.finish()


def jersey():
    """A barreira New Jersey de concreto (3 m): o perfil de verdade (a base larga com o pé de 7,5 cm, a quebra a 33 cm, a parede
    inclinada até o topo de 15 cm, 81 cm de altura), os furos de içar e as lascas nas pontas."""
    p = Part('SM_civ_jersey')
    p.bm('Concrete#j', 0.02)
    prof = [(-0.305, 0.0), (0.305, 0.0), (0.305, 0.075), (0.18, 0.33), (0.075, 0.81), (-0.075, 0.81), (-0.18, 0.33), (-0.305, 0.075)]
    L = 3.0
    p.loft('Concrete#j', [[(-L / 2, y, z) for (y, z) in prof], [(L / 2, y, z) for (y, z) in prof]])
    for x in (-0.9, 0.9):
        vp.rbox(p, 'Dark#hole', (x, 0.0, 0.76), (0.1, 0.16, 0.1), r=0.02, k=1)
    vp.rbox(p, 'Dark#hole', (0.0, 0.0, 0.06), (0.5, 0.62, 0.08), r=0.02, k=1)
    return p.finish()


def dumpster():
    """A caçamba de lixo de aço (2 × 1,2 m): o corpo trapezoidal com os reforços, as 2 tampas (uma aberta), os munhões de içar, as
    rodinhas, os sacos de lixo saindo por cima e caídos no pé."""
    p = Part('SM_civ_dumpster')
    m = 'Crate#dump'
    hexa(p, m, [(-1.0, -0.55, 0.15), (1.0, -0.55, 0.15), (-1.0, 0.55, 0.15), (1.0, 0.55, 0.15),
                (-1.05, -0.62, 1.2), (1.05, -0.62, 1.2), (-1.05, 0.62, 1.2), (1.05, 0.62, 1.2)], bevel=0.02)
    for x in (-0.6, 0.0, 0.6):
        vp.rbox(p, m, (x, -0.6, 0.68), (0.08, 0.06, 1.0), pitch=0, roll=-3.5, r=0.01, k=1)
        vp.rbox(p, m, (x, 0.6, 0.68), (0.08, 0.06, 1.0), roll=3.5, r=0.01, k=1)
    vp.rbox(p, 'Rust#rim', (0.0, 0.0, 1.21), (2.14, 1.28, 0.06), r=0.02, k=1)
    vp.rbox(p, 'Dark#in', (0.0, 0.0, 1.2), (1.98, 1.12, 0.04), r=0.01, k=1)
    vp.rbox(p, 'Dark#lid', (-0.52, 0.0, 1.27), (1.0, 1.28, 0.05), roll=0, r=0.02, k=1)
    lid = Part('lid')
    vp.rbox(lid, 'Dark#lid', (0.52, 0.0, 1.27), (1.0, 1.28, 0.05), r=0.02, k=1)
    lo = lid.finish()
    piv = V(0.52, 0.64, 1.25)
    lo.data.transform(Matrix.Translation(piv) @ Matrix.Rotation(math.radians(100), 4, 'X') @ Matrix.Translation(-piv))
    for x in (-1.08, 1.08):
        vp.lathe(p, 'Metal#trun', (x, 0.0, 0.85), 'x', [(0.0, 0.0), (0.06, 0.0), (0.06, 0.12 * (1 if x > 0 else -1)), (0.0, 0.12 * (1 if x > 0 else -1))],
                 seg=12)
    for (x, y) in ((-0.8, -0.4), (0.8, -0.4), (-0.8, 0.4), (0.8, 0.4)):
        vp.lathe(p, 'Rubber#w', (x, y, 0.07), 'y', [(0.0, -0.03), (0.07, -0.03), (0.07, 0.03), (0.0, 0.03)], seg=12)
    for (x, y, z, s) in ((0.4, 0.2, 1.2, 0.5), (0.65, -0.25, 1.15, 0.45), (1.3, 0.75, 0.0, 0.55), (1.0, 0.95, 0.0, 0.42)):
        cp.bag(p, (x, y, z), (s, s * 0.8, s * 0.65), yaw=40 * x, m='Dark#trash', seed=x * 3)
    o = p.finish()
    return cp.join([o, lo], 'SM_civ_dumpster')


def streetlamp():
    """O poste de luz de rua (7 m): o pé de concreto, o tubo cônico de aço, o braço curvo, a luminária fechada com a lente embaixo,
    o reator e a caixa de inspeção no pé."""
    p = Part('SM_civ_streetlamp')
    vp.lathe(p, 'Concrete#base', (0.0, 0.0, 0.0), 'z', [(0.0, 0.0), (0.24, 0.0), (0.24, 0.25), (0.2, 0.32), (0.0, 0.32)], seg=16)
    vp.lathe(p, 'Metal#lamp', (0.0, 0.0, 0.3), 'z', [(0.0, 0.0), (0.09, 0.0), (0.085, 0.4), (0.06, 6.6), (0.0, 6.65)], seg=14)
    vp.rbox(p, 'Metal#lamp', (0.0, 0.1, 1.2), (0.14, 0.06, 0.32), r=0.02, k=1)
    pts = vp.bent_bar([(0.0, 0.0, 6.2), (0.0, 0.0, 6.75), (0.0, 1.2, 6.95), (0.0, 1.75, 6.9)], R=0.5, n=6)
    vp.tube(p, 'Metal#lamp', pts, 0.045, seg=10)
    vp.rbox(p, 'Metal#head', (0.0, 1.95, 6.86), (0.32, 0.62, 0.14), r=0.08, k=3)
    vp.rbox(p, 'Lens#lamp', (0.0, 1.98, 6.79), (0.24, 0.48, 0.02), r=0.06, k=2)
    vp.tube(p, 'Dark#cable', [(0.08, 0.0, 6.3), (0.4, 0.0, 6.55)], 0.012, seg=4)
    return p.finish()


def market_stall():
    """A banca da feira: a mesa de tábua nos cavaletes, as caixas de fruta (as cores da carga: os montes redondos), o toldo de pano
    nos 4 paus inclinado pra frente, a lona do lado, os sacos no chão e a balança."""
    p = Part('SM_civ_market_stall')
    vp.rbox(p, 'Wood#table', (0.0, 0.0, 0.82), (2.4, 1.0, 0.05), r=0.015, k=1)
    for x in (-1.0, 1.0):
        for s in (-1, 1):
            vp.tube(p, 'Wood#leg', [(x, s * 0.4, 0.0), (x, s * 0.3, 0.8)], 0.03, seg=6)
    for k, (x, y, m) in enumerate(((-0.8, 0.15, 'Rust#fruit'), (-0.15, 0.15, 'Crate#fruit'), (0.5, 0.15, 'Yellow#fruit'),
                                   (-0.5, -0.25, 'GlaPaint#fruit'), (0.2, -0.25, 'Rust#fruit2'))):
        crate(p, (x, y, 0.845), (0.55, 0.38, 0.16), yaw=4 * k, m='Wood#crate', slats=2)
        for j in range(9):
            a, r_ = 2 * math.pi * cp.h3(j, k, 3), 0.15 * cp.h3(j, k, 5) ** 0.5
            vp.lathe(p, m, (x + r_ * math.cos(a) * 1.3, y + r_ * math.sin(a) * 0.8, 0.99 + 0.03 * cp.h3(j, k, 7)), 'z',
                     [(0.0, -0.05), (0.04, -0.045), (0.055, 0.0), (0.04, 0.04), (0.0, 0.05)], seg=8)
    for (x, y) in ((-1.3, -0.65), (1.3, -0.65), (-1.3, 0.85), (1.3, 0.85)):
        vp.tube(p, 'Wood#pole', [(x, y, 0.0), (x, y, 2.35 if y > 0 else 2.1)], 0.035, seg=6)
    tarp(p, [(-1.4, -0.75, 2.12), (1.4, -0.75, 2.12), (1.4, 0.95, 2.38), (-1.4, 0.95, 2.38)], m='Cloth#awn', sag=0.1, folds=3, amp=0.03,
         thick=0.015, seed=4)
    vp.tarp_drape(p, (-1.38, 0.95, 2.36), (-1.38, -0.73, 2.1), 1.1, (-1, 0, 0), m='Rust', folds=4, amp=0.015, thick=0.012, seed=5, nu=16, nv=5,
                  hem=True, ties=())
    for (x, y) in ((-0.9, -0.9), (-0.45, -1.0)):
        cp.bag(p, (x, y, 0.0), (0.6, 0.4, 0.3), yaw=80, m='Cloth#bag')
    return p.finish()


def rubble():
    """O monte de entulho solto (3 × 3 m): os blocos de alvenaria quebrados empilhados no monte, as lascas de laje com o ferro."""
    p = Part('SM_civ_rubble')
    p.bm('Plaster#rub', 0.0)
    R = 1.5
    for i in range(110):
        a = 2 * math.pi * cp.h3(i, 7, 21)
        r = R * cp.h3(i, 7, 23) ** 0.6
        x, y = r * math.cos(a), r * math.sin(a)
        z = 0.85 * max(0.0, 1.0 - r / R) ** 1.3 * (0.35 + 0.65 * cp.h3(i, 7, 25))
        s = 0.15 + 0.4 * cp.h3(i, 7, 27) ** 2
        hp.chunk(p, (x, y, z + s * 0.15), s, i + 7000, m='Plaster#rub' if cp.h3(i, 7, 29) > 0.25 else 'BrokenAdobe#rub')
    for i in range(4):
        a = 2 * math.pi * cp.h3(i, 7, 31)
        ex, ey, ez = vp.ypr_frame(360 * cp.h3(i, 7, 37), (cp.h3(i, 7, 39) - 0.5) * 40, (cp.h3(i, 7, 41) - 0.5) * 30)
        C = Vector((0.7 * math.cos(a), 0.7 * math.sin(a), 0.35))
        L = 0.7
        vp.prism(p, 'Plaster#rub', [(-L / 2, -L * 0.35), (L / 2, -L * 0.4), (L * 0.42, L * 0.33), (-L * 0.1, L * 0.42)], C, ex, ey, ez, 0.14)
        vp.tube(p, 'Rust#rebar', [C + ex * (L * 0.45) + ez * 0.07, C + ex * (L * 0.75) + ez * 0.2 + ey * 0.1], 0.01, seg=4)
    return p.finish()


ALL = [('barrels', barrels), ('crates', crates), ('sandbags', sandbags), ('tires', tires), ('water_tank', water_tank),
       ('water_tank_stand', water_tank_stand), ('tank_horizontal', tank_h), ('dish', dish), ('roof_ac', roof_ac),
       ('laundry', laundry), ('awning_wall', awning_wall), ('fence_corr', fence_corr), ('jersey', jersey), ('dumpster', dumpster),
       ('streetlamp', streetlamp), ('market_stall', market_stall), ('rubble', rubble)]

clear_scene()
objs = []
for i, (name, fn) in enumerate(ALL):
    if ONLY and name not in ONLY:
        continue
    o = fn()
    o.name = 'SM_civ_' + name
    # cada peça cozida longe das outras (a sombra de contato de uma não cai na outra); volta pra origem na exportação
    bake_in_place(o, ((i % 4) * 12.0, (i // 4) * 12.0, 0.0))
    print('CIV', o.name, cp.tris(o), 'tris (antes da grade)')
    objs.append(o)
for o in objs:
    export(o, OUT)
