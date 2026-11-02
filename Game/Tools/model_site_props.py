"""O canteiro da obra não começada (01/11/2026; o pedido do Felipe na avaliação de 31/10: "o placeholder da construção não iniciada ainda
tá ruim, tinha que ser muros de construção [...] faça de arrepiar, bem feito, sem parecer jogo low poly, sem parecer jogo de lego"),
uma GLB por peça, com as bibliotecas do kit civil v2 aprovado (a grade de detalhe, as máscaras de quina, contato e fresta).

O canteiro de verdade antes da máquina chegar: o gabarito (as estacas de madeira com a fita no contorno do prédio), o tapume de chapa
NOVA (galvanizada, não a enferrujada da vila) com a faixa na cor do time, e o material chegando, limpo e empilhado (o bloco de concreto
no palete, o feixe de vergalhão nos caibros, os sacos de cimento, os montes de areia e de brita). Nada de entulho: no jogo o entulho quer
dizer prédio destruído.

Rodar (Blender sem janela, só CPU; a classe 'vehicle' dá a grade fina de 14 cm e a sombra de contato de 1,1 m):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_site_props.py C:/Users/luisf/explosion/captures/game/models/site_props [nome1,nome2...]

As peças (origem no centro do pé, no chão; o comprido em +X):
  SM_Site_Stake       a estaca do gabarito (1,1 m), a ponta chanfrada, a fita amarrada no alto (2 pontas soltas) na cor do time;
  SM_Site_Tape        a fita de 4 m entre duas estacas, a 0,9 m, com a barriga do peso (o jogo põe em fila no contorno);
  SM_Site_Hoarding    o tapume de 4 m × 2,2 m: as chapas galvanizadas novas, os 3 pés com as mãos-francesas atrás, as 2 travessas e
                      a faixa de 0,35 m na cor do time perto do topo, e a placa da obra;
  SM_Site_HoardingZebra  o mesmo com a faixa zebrada laranja e preta (o A/B que ele pediu: "não sei se gostei do laranja e preto");
  SM_Site_Blocks      o palete com o bloco de concreto em 4 fiadas amarradas (0,39 × 0,19 × 0,19), uma fiada faltando um canto;
  SM_Site_Rebar       o feixe de vergalhão de 6 m (21 barras) nos 3 caibros, amarrado com arame em 3 pontos;
  SM_Site_Cement      o palete com os sacos de cimento em 3 camadas;
  SM_Site_Sand        o monte de areia (3,2 × 2,6 m, 0,9 m) com a borda escorrida;
  SM_Site_Gravel      o monte de brita menor (2,2 × 1,8 m, 0,6 m).
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export  # noqa: E402
import model_civ_house_parts as hp  # noqa: E402
import model_civ_parts as cp  # noqa: E402
from model_building_parts import bake_in_place  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402

ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
OUT = ARGS[0] if ARGS else os.path.join(os.getcwd(), 'site_props')
ONLY = ARGS[1].split(',') if len(ARGS) > 1 else None
os.makedirs(OUT, exist_ok=True)

# as cores de conferência (o jogo troca pelas instâncias dos slots: a Team vira a cor do jogador, a Hazard a zebra)
for k, v in {'Team': (86, 108, 150), 'Hazard': (230, 150, 20), 'Yellow': (220, 170, 40), 'Concrete': (150, 146, 138),
             'Plaster': (196, 160, 112), 'Rust': (110, 62, 40), 'Dark': (40, 40, 38), 'Cloth': (178, 172, 160),
             'Metal': (168, 172, 174), 'Wood': (150, 112, 70), 'Trim': (60, 62, 64), 'Crate': (120, 96, 60)}.items():
    COLORS.setdefault(k, v)


# ===================================================================================================== peças
def pallet(p, c, yaw=0.0, m='Wood#pallet'):
    """O palete (1,2 × 1,0 m): as 3 longarinas de bloco, as tábuas de cima com a fresta, as de baixo (o do kit civil)."""
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


def stake_at(p, x, y, h=1.1, seed=0, tape=True):
    """A estaca de 5 × 5 cm com a ponta chanfrada enterrada, levemente torta, e a fita amarrada no alto com as 2 pontas soltas."""
    lean_x = (cp.h3(seed, 1, 5) - 0.5) * 3.0
    lean_y = (cp.h3(seed, 2, 5) - 0.5) * 3.0
    vp.rbox(p, 'Wood#stake', (x, y, h / 2), (0.05, 0.05, h), pitch=lean_x, roll=lean_y, r=0.006, k=1)
    vp.rbox(p, 'Wood#stake', (x, y, h + 0.025), (0.035, 0.035, 0.05), pitch=lean_x, roll=lean_y, r=0.01, k=1)
    if tape:
        # o nó: a volta da fita em torno da estaca e as 2 pontas que caem soltas (o vento as deixa tortas)
        vp.rbox(p, 'Team#tape', (x, y, h - 0.12), (0.075, 0.075, 0.05), r=0.004, k=1)
        for k, ang in enumerate((20.0 + 40.0 * cp.h3(seed, 3, 5), 200.0 + 40.0 * cp.h3(seed, 4, 5))):
            a = math.radians(ang)
            L = 0.22 + 0.12 * cp.h3(seed, 5 + k, 5)
            vp.rbox(p, 'Team#tape', (x + math.cos(a) * 0.06, y + math.sin(a) * 0.06, h - 0.12 - L / 2), (0.004, 0.05, L),
                    yaw=ang, pitch=8.0 + 10.0 * k, r=0.001, k=1)


def stake():
    p = Part('SM_Site_Stake')
    stake_at(p, 0.0, 0.0, seed=3)
    return p.finish()


def tape():
    """A fita de 4 m (de −2 a +2 em X) entre duas estacas, a 0,9 m, com a barriga do peso (~5 cm no meio) e as torções."""
    p = Part('SM_Site_Tape')
    n = 16
    for k in range(n):
        x0 = -2.0 + 4.0 * k / n
        x1 = -2.0 + 4.0 * (k + 1) / n
        xm = 0.5 * (x0 + x1)
        sag = 0.05 * (1.0 - (xm / 2.0) ** 2)
        twist = 25.0 * math.sin(xm * 2.3)
        vp.rbox(p, 'Team#tape', (xm, 0.0, 0.9 - sag), (x1 - x0 + 0.004, 0.003, 0.05), roll=twist, r=0.0005, k=1)
    for x in (-2.0, 2.0):
        stake_at(p, x, 0.0, seed=int(x * 10) + 11, tape=False)
    return p.finish()


def hoarding_panel(p, zebra=False):
    """O tapume de 4 m (−2 a +2 em X, a frente em −Y), 2,2 m de altura: as chapas galvanizadas novas com uma pequena diferença de tom por
    chapa, as 2 travessas, os 3 pés com as mãos-francesas atrás, a faixa do time e a placa da obra."""
    xs = [-2.0, -1.0, 0.0, 1.0, 2.0]
    for k, (a, b) in enumerate(zip(xs[:-1], xs[1:])):
        m = 'Metal#galv%d' % (k % 2)
        hp.corr_panel(p, (a - 0.02, -0.06), (b + 0.02, -0.06), 0.03, 2.2, m=m, pitch=0.068, amp=0.014)
    # a faixa do time (ou a zebra) por cima das chapas, a 1,6–1,95 m
    vp.rbox(p, ('Hazard#zebra' if zebra else 'Team#band'), (0.0, -0.085, 1.78), (4.0, 0.012, 0.35), r=0.004, k=1)
    # os pés de madeira (atrás), as travessas e as mãos-francesas
    for x in (-1.95, 0.0, 1.95):
        vp.rbox(p, 'Wood#post', (x, 0.03, 1.12), (0.09, 0.09, 2.3), r=0.015, k=1)
        vp.tube(p, 'Wood#post', [(x, 0.06, 1.5), (x, 1.05, 0.02)], 0.035, seg=6)
        vp.rbox(p, 'Concrete#foot', (x, 0.55, 0.06), (0.32, 1.25, 0.12), r=0.02, k=1)   # o pé de concreto que segura a mão-francesa
    for z in (0.5, 1.75):
        vp.rbox(p, 'Wood#rail', (0.0, 0.0, z), (4.0, 0.05, 0.08), r=0.012, k=1)
    # a placa da obra (0,9 × 0,6 m) na chapa da esquerda: o fundo amarelo com a moldura escura
    vp.rbox(p, 'Dark#frame', (-1.0, -0.092, 1.05), (0.94, 0.012, 0.64), r=0.006, k=1)
    vp.rbox(p, 'Yellow#sign', (-1.0, -0.099, 1.05), (0.88, 0.008, 0.58), r=0.004, k=1)
    for k in range(3):
        vp.rbox(p, 'Dark#text', (-1.0 - 0.12 * (k == 2), -0.104, 1.2 - 0.13 * k), (0.66 - 0.2 * (k == 2), 0.004, 0.05), r=0.002, k=1)


def hoarding():
    p = Part('SM_Site_Hoarding')
    hoarding_panel(p, zebra=False)
    return p.finish()


def hoarding_zebra():
    p = Part('SM_Site_HoardingZebra')
    hoarding_panel(p, zebra=True)
    return p.finish()


def blocks():
    """O palete com o bloco de concreto: 4 fiadas desencontradas (3 × 5 blocos de 0,39 × 0,19 × 0,19), a fita de arquear em cruz, e a fiada
    de cima faltando 2 blocos num canto (já começaram a tirar)."""
    p = Part('SM_Site_Blocks')
    pallet(p, (0.0, 0.0, 0.0))
    bx, by, bz = 0.39, 0.19, 0.19
    for lay in range(4):
        rows = 5
        cols = 3
        for r in range(rows):
            for c in range(cols):
                if lay == 3 and r >= 3 and c == 2:
                    continue
                x = -0.39 + (c * 0.395) + (0.0 if lay % 2 == 0 else 0.0)
                y = -0.4 + r * 0.2
                jit = (cp.h3(r, c, lay) - 0.5) * 0.012
                vp.rbox(p, 'Concrete#block', (x + jit, y, 0.132 + bz * (lay + 0.5)), (bx - 0.006, by - 0.006, bz - 0.006), r=0.008, k=1)
                # o furo de cima do bloco (2 vazados), só na fiada de cima
                if lay == 3:
                    for s in (-1, 1):
                        vp.rbox(p, 'Dark#hole', (x + jit + s * 0.095, y, 0.132 + bz * 4.0 - 0.002), (0.13, 0.11, 0.006), r=0.01, k=1)
    for s in (-1, 1):
        vp.rbox(p, 'Dark#strap', (0.0, s * 0.25, 0.132 + bz * 2), (1.2, 0.02, bz * 4 + 0.01), r=0.003, k=1)
    return p.finish()


def rebar():
    """O feixe de vergalhão de 6 m: 21 barras de 16 mm (o feixe hexagonal, cada uma um pouco torta e com a ponta cortada), nos 3 caibros, e
    o arame em 3 pontos."""
    p = Part('SM_Site_Rebar')
    for x in (-2.2, 0.0, 2.2):
        vp.rbox(p, 'Wood#sleeper', (x, 0.0, 0.05), (0.12, 0.9, 0.1), r=0.015, k=1)
    rr = 0.009
    k = 0
    for row in range(4):
        n = 6 - abs(row - 1)
        for i in range(n):
            y = (i - (n - 1) / 2) * 0.022 + (cp.h3(row, i, 9) - 0.5) * 0.004
            z = 0.1 + rr + row * 0.019
            bend = (cp.h3(row, i, 7) - 0.5) * 0.02
            off = (cp.h3(row, i, 8) - 0.5) * 0.25
            vp.tube(p, 'Rust#rebar', [(-3.0 + off, y, z), (0.0 + off, y + bend, z + abs(bend) * 0.5), (3.0 + off, y, z)], rr, seg=6)
            k += 1
    for x in (-1.6, 0.4, 2.2):
        vp.rbox(p, 'Dark#wire', (x, 0.0, 0.17), (0.012, 0.16, 0.11), r=0.004, k=1)
    return p.finish()


def cement():
    """O palete com 3 camadas de sacos de cimento (0,65 × 0,42 × 0,13 m: o saco de 50 kg), cada camada cruzada com a de baixo."""
    p = Part('SM_Site_Cement')
    pallet(p, (0.0, 0.0, 0.0))
    for lay in range(3):
        z = 0.14 + lay * 0.115
        if lay % 2 == 0:
            for i in range(2):
                for j in range(2):
                    cp.bag(p, (-0.3 + 0.6 * i, -0.22 + 0.44 * j, z), (0.62, 0.42, 0.13), yaw=0.0, m='Cloth#cement', seed=lay * 10 + i * 2 + j,
                           pillow=True)
        else:
            for i in range(3):
                cp.bag(p, (-0.4 + 0.4 * i, 0.0, z), (0.42, 0.62, 0.13), yaw=0.0, m='Cloth#cement', seed=lay * 10 + i, pillow=True)
    return p.finish()


def pile(name, rx, ry, h, m, seed, grains=0, repose_deg=33.0):
    """O monte solto (01/11, v2: o cone único lia "lego" no canteiro da v36): o que o caminhão deixa são 2–3 descargas sobrepostas, cada
    uma um cone no ângulo de repouso (a areia ~33°, a brita ~38°), o cume arredondado pelo vento e pela pá; a superfície com os calombos
    de 3–5 cm do material solto e o pé escorrido, irregular, afundando 2 cm na areia. Campo de alturas numa grade de 8 cm (o máximo dos
    cones, a união suave nos encontros), e as pedras soltas no pé (a brita)."""
    p = Part(name)
    slope = math.tan(math.radians(repose_deg))
    # as descargas: a principal no meio e 1–2 menores encostadas (a posição e a altura pela semente)
    lobes = [(0.0, 0.0, h)]
    lobes.append((0.62 * rx * (0.85 + 0.3 * cp.h3(1, 1, seed)), 0.35 * ry * (cp.h3(1, 2, seed) - 0.3), h * (0.72 + 0.12 * cp.h3(1, 3, seed))))
    if rx > 1.2:
        lobes.append((-0.55 * rx * (0.85 + 0.3 * cp.h3(2, 1, seed)), -0.4 * ry * (0.6 + 0.4 * cp.h3(2, 2, seed)), h * (0.6 + 0.12 * cp.h3(2, 3, seed))))
    sx = ry / rx   # a elipse do pé: o cone mais curto em y

    def vnoise(x, y, cell, k):
        # ruído de valor suave (a grade de "cell" m, interpolada com o smoothstep), média ~0, de -0,5 a 0,5
        gx, gy = x / cell, y / cell
        ix, iy = math.floor(gx), math.floor(gy)
        fx, fy = gx - ix, gy - iy
        ux, uy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
        a = cp.h3(ix, iy, seed * 31 + k); b = cp.h3(ix + 1, iy, seed * 31 + k)
        c = cp.h3(ix, iy + 1, seed * 31 + k); d = cp.h3(ix + 1, iy + 1, seed * 31 + k)
        return (a + (b - a) * ux + (c - a) * uy + (a - b - c + d) * ux * uy) - 0.5

    def height(x, y):
        zs = []
        for (cx, cy, hh) in lobes:
            dx, dy = x - cx, (y - cy) / sx
            # o pé irregular: o raio de cada direção varia ±12% (o material escorre mais pra um lado)
            ang = math.atan2(dy, dx)
            wob = 1.0 + 0.12 * vnoise(math.cos(ang) * 2.0 + cx, math.sin(ang) * 2.0 + cy, 0.9, 5)
            r = math.hypot(dx, dy) / wob
            zc = hh - slope * r
            # o cume redondo: a parábola nos últimos 18% da altura
            top = 0.18 * hh
            if zc > hh - top:
                u = (hh - zc) / top
                zc = hh - top * (1.0 - (1.0 - u) ** 2) * 0.85 - 0.15 * top * u
            zs.append(zc)
        zs.sort(reverse=True)
        z = zs[0]
        if len(zs) > 1 and zs[1] > -0.2:
            # a união suave dos encontros (o material da descarga de cima escorre na de baixo)
            k = 0.12
            hm = max(k - abs(zs[0] - zs[1]), 0.0) / k
            z = max(zs[0], zs[1]) + hm * hm * k * 0.25
        if z <= 0.0:
            return z
        # os calombos do material solto (o grosso de 40 cm e o fino de 12 cm), mais fracos no pé
        amp = min(z / 0.25, 1.0)
        return z + amp * (0.05 * vnoise(x, y, 0.4, 1) + 0.025 * vnoise(x, y, 0.12, 2))

    # a malha radial em volta do meio: a borda segue o contorno de verdade (a grade quadrada deixava o pé em degraus de serra); em cada
    # rumo, o raio do pé onde o campo zera, achado andando de 2 em 2 cm pra fora
    na, nr = 72, 16
    bb = p.bm(m, 0.0)
    cx0 = sum(l[0] for l in lobes) / len(lobes) * 0.5
    cy0 = sum(l[1] for l in lobes) / len(lobes) * 0.5
    edge = []
    for a in range(na):
        ang = 2 * math.pi * a / na
        r, last_in = 0.0, 0.0
        while r < 3.5 * max(rx, ry):
            if height(cx0 + r * math.cos(ang), cy0 + r * math.sin(ang)) > 0.0:
                last_in = r
            r += 0.02
        edge.append(last_in + 0.02)
    rings = []
    for k in range(nr + 1):
        t = k / nr
        ring = []
        for a in range(na):
            ang = 2 * math.pi * a / na
            rr = edge[a] * (1.0 - t) ** 1.15   # os anéis mais juntos no pé, onde a forma muda mais
            x, y = cx0 + rr * math.cos(ang), cy0 + rr * math.sin(ang)
            z = max(height(x, y), 0.0) if k > 0 else 0.0
            ring.append(bb.verts.new(V(x, y, z - 0.02)))   # o pé afunda 2 cm na areia
        rings.append(ring)
    centre = bb.verts.new(V(cx0, cy0, max(height(cx0, cy0), 0.0) - 0.02))
    for k in range(nr):
        for a in range(na):
            b = (a + 1) % na
            bb.faces.new([rings[k][a], rings[k][b], rings[k + 1][b], rings[k + 1][a]]).smooth = True
    for a in range(na):
        b = (a + 1) % na
        bb.faces.new([rings[nr][a], rings[nr][b], centre]).smooth = True
    for g in range(grains):
        ang = 2 * math.pi * cp.h3(g, 1, seed)
        rad = 1.0 + 0.18 * cp.h3(g, 2, seed)
        s = 0.05 + 0.06 * cp.h3(g, 3, seed)
        vp.rbox(p, m.split('#')[0] + '#stone', (rad * rx * math.cos(ang), rad * ry * math.sin(ang), s * 0.4), (s, s * 0.8, s * 0.7),
                yaw=360 * cp.h3(g, 4, seed), r=s * 0.3, k=1)
    return p.finish()


def sand():
    return pile('SM_Site_Sand', 1.6, 1.3, 0.9, 'Plaster#sand', 3)


def gravel():
    return pile('SM_Site_Gravel', 1.1, 0.9, 0.6, 'Concrete#gravel', 7, grains=26, repose_deg=38.0)   # (a brita, mais íngreme)


def scaffold_bay():
    """(01/11, ele: "os andaimes não estão em volta da construção, estão dentro, no centro") um vão do andaime de tubo e braçadeira, pro
    jogo montar em volta da obra inteira (RTSUnitActor::UpdateScaffoldRing), lance por lance com a obra: 3 m ao longo da parede (+X), 1 m
    de fundo pra fora (+Y; o lado da parede em y = 0), um lance de 2 m. Os montantes na ponta de cá (o vão seguinte fecha a outra), as
    longarinas e as travessas no alto do lance, o assoalho de tábuas, o guarda-corpo de fora a 1 m e o rodapé zebrado, a diagonal na face
    de fora. Os slots dos andaimes dos prédios: Trim (o tubo), Crate (a tábua), Hazard (o rodapé)."""
    p = Part('SM_Site_ScaffoldBay')
    L, D, H = 3.0, 1.0, 2.0
    for y in (0.0, D):
        p.cyl('Trim', (0.0, y, H / 2), 'z', 0.024, H, seg=8, bevel=0.0)             # os montantes (48 mm)
        p.cyl('Trim', (L / 2, y, H - 0.03), 'x', 0.024, L, seg=8, bevel=0.0)        # a longarina
    p.cyl('Trim', (0.0, D / 2, H - 0.03), 'y', 0.024, D, seg=8, bevel=0.0)          # a travessa
    for k in range(4):                                                              # o assoalho: 4 tábuas de 22 cm com a fresta
        y0 = 0.04 + k * 0.235
        p.box('Crate', 0.02, L - 0.02, y0, y0 + 0.22, H - 0.01, H + 0.035, bevel=0.006)
    p.cyl('Trim', (L / 2, D, H + 1.0), 'x', 0.024, L, seg=8, bevel=0.0)             # o guarda-corpo
    p.cyl('Trim', (L / 2, D, H + 0.5), 'x', 0.024, L, seg=8, bevel=0.0)             # o intermediário
    p.cyl('Trim', (0.0, D, H + 0.5), 'z', 0.024, 1.0, seg=8, bevel=0.0)             # o montante do guarda-corpo
    p.box('Hazard', 0.0, L, D - 0.015, D + 0.015, H + 0.035, H + 0.185, bevel=0.0)  # o rodapé zebrado de 15 cm
    import math as _m
    d = _m.hypot(L, H)
    p.cyl('Trim', (L / 2, D + 0.03, H / 2), (L / d, 0.0, H / d), 0.02, d, seg=6, bevel=0.0)   # a diagonal de travamento
    return p.finish()


ALL = [('Stake', stake), ('Tape', tape), ('Hoarding', hoarding), ('HoardingZebra', hoarding_zebra), ('Blocks', blocks),
       ('Rebar', rebar), ('Cement', cement), ('Sand', sand), ('Gravel', gravel), ('ScaffoldBay', scaffold_bay)]

clear_scene()
objs = []
for i, (name, fn) in enumerate(ALL):
    if ONLY and name not in ONLY:
        continue
    o = fn()
    o.name = 'SM_Site_' + name
    bake_in_place(o, ((i % 4) * 12.0, (i // 4) * 12.0, 0.0))
    print('SITE', o.name, cp.tris(o), 'tris (antes da grade)')
    objs.append(o)
for o in objs:
    export(o, OUT)
