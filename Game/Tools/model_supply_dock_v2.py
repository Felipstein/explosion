"""O Supply básico GRANDE, A DOCA (o SupplyDock do ZH, 400 caixas = 30 000 $), 03/11/2026: o desenho 4 do conceito escolhido
docs/concept/buildings/supply_basico_4_tamanhos_ESCOLHIDO.png (a doca clássica modernizada). Substitui a pirâmide de caixas
reprovada (model_supply_dock.py, "feito como se fosse um jogo de lego"), que fica como está. A família: model_supply_basic_parts.py.

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_supply_dock_v2.py C:/Users/luisf/explosion/captures/game/models/supply_dock_v2

A palavra (o que lê em 1 segundo): A PLATAFORMA DE CONCRETO COM AS FILEIRAS DE PILHAS E O GALPÃO DE AÇO SÓ NO FUNDO. A plataforma de
carga elevada (1,2 m, o paredão com as juntas das placas, a cantoneira de aço e a faixa de perigo na beira de carga, os batentes de
borracha com o número de estêncil gasto de cada baia, a escada encostada na face), as rampas de concreto com os frisos de aço nas 2
pontas, o murinho no fundo; o galpão de aço (as colunas de perfil I, as tesouras, as terças, o telhado de 2 águas de chapa ondulada
com umas chapas enferrujadas trocadas, os 2 exaustores na cumeeira) cobrindo SÓ O TERÇO DE TRÁS: debaixo dele os paletes vazios, os
tambores e a empilhadeira (nada que pareça estoque). O ESTOQUE NO ABERTO: as fileiras de pilhas nos paletes na plataforma e 2 pilhas
no pátio, uma em cada ponta (da câmera do jogo o jogador conta o dinheiro pelas caixas). A guarita de reboco da vila com a caixa d'água
no cavalete no teto (canto +Y do fundo), 2 postes de holofote. Neutro: sem cor de facção; só os números de estêncil gastos.

Lote 26,33 × 32,12 m (x −13,165..13,165; y −16,06..16,06; a pegada do depósito de hoje no WorldForge), frente em +X.
A FAIXA DOS CAMINHÕES no lado +X (x 6,1..11,1, o meio em x 8,6), junto da beira de carga: entra em (8,6; −16,06) e sai em
(8,6; +16,06), livre. A plataforma: x −12,6..5,6, y −11,6..11,6, piso a 1,2 m. O estoque fica em x −13,2..5,6.
Alturas: o piso 1,2 m; as pilhas até 3,2 m (as colunas de 3 até 4,1 m); o beiral do galpão 5,8 m, a cumeeira 6,3 m (os exaustores até
~6,9 m); a guarita 2,9 m (a caixa d'água até ~5,1 m); os holofotes ~9 m.

Peças: SM_SupplyDock_Base (tudo menos as caixas do estoque) e o stock_layout.json (as vagas, na ordem em que somem).
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import clear_scene, export  # noqa: E402
import model_supply_basic_parts as sp  # noqa: E402
import model_civ_house_parts as hp  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
from model_building_parts import WallFace, sweep, wbox  # noqa: E402
from model_faction_kit import corrugated  # noqa: E402
from model_gla_palace_v2 import _hash, crate, prism  # noqa: E402

PREFIX = 'SM_SupplyDock'
LOT = (-13.165, 13.165, -16.06, 16.06)
LANE_X, LANE_HALF = 8.6, 2.5
PX0, PX1, PY0, PY1, PZ = -12.6, 5.6, -11.6, 11.6, 1.2       # a plataforma
RAMP_P = (1.4, 5.2)          # a rampa da ponta +Y (x)
RAMP_N = (-11.2, -7.4)       # a rampa da ponta −Y (x)
RAMP_L = 4.2
SHED_X = (-12.25, -7.15)     # as colunas do galpão (fundo, frente)
SHED_Y = (-11.2, -5.6, 0.0, 5.6, 11.2)
EAVE = PZ + 4.6              # o beiral (5,8 m)
RIDGE_X = (SHED_X[0] + SHED_X[1]) / 2
RIDGE = EAVE + 0.5
STAIR = (-1.0, 1.1)          # a escada encostada na face de carga (sobe de y −1,0 pra +1,1)
BUMPERS = (-9.6, -6.4, -3.2, 3.2, 6.4, 9.6)
GUARD = (-12.7, -8.7, 12.3, 15.7)   # a guarita (x0, x1, y0, y1)
GUARD_Z = 2.9


def iprof(w, h, tf, tw):
    return [(-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, -h / 2 + tf), (tw / 2, -h / 2 + tf), (tw / 2, h / 2 - tf), (w / 2, h / 2 - tf),
            (w / 2, h / 2), (-w / 2, h / 2), (-w / 2, h / 2 - tf), (-tw / 2, h / 2 - tf), (-tw / 2, -h / 2 + tf), (-w / 2, -h / 2 + tf)]


def ibeam(p, a, b, w=0.22, h=0.25, up=(0.0, 0.0, 1.0), m='Metal#beam'):
    sweep(p, m, [a, b], iprof(w, h, 0.03, 0.025), up=up)


def angle(p, a, b, s=0.08, t=0.012, up=(0.0, 0.0, 1.0), m='Metal#truss'):
    """A cantoneira (o L) de a até b."""
    sweep(p, m, [a, b], [(-s / 2, -s / 2), (s / 2, -s / 2), (s / 2, -s / 2 + t), (-s / 2 + t, -s / 2 + t), (-s / 2 + t, s / 2),
                         (-s / 2, s / 2)], up=up)


# ===================================================================================================== a plataforma
def platform(p):
    """A plataforma de carga: o bloco de concreto com a laje por cima (a pingadeira de 5 cm), as juntas das placas nas faces e no
    piso, a cantoneira de aço e a faixa de perigo na beira de carga, os batentes de borracha com o número de cada baia, o murinho do
    fundo e das pontas, as faixas amarelas do corredor, a escada encostada na face com o corrimão."""
    p.box('Concrete', PX0, PX1, PY0, PY1, -0.3, PZ - 0.16)
    p.box('Concrete#slab', PX0 - 0.05, PX1 + 0.05, PY0 - 0.05, PY1 + 0.05, PZ - 0.2, PZ, bevel=0.03)
    # as juntas das placas pré-moldadas nas 4 faces (o ritmo do paredão)
    for (f, at, u0, u1) in (('+x', PX1, PY0, PY1), ('-x', PX0, PY0, PY1), ('+y', PY1, PX0, PX1), ('-y', PY0, PX0, PX1)):
        F = WallFace(f, at, 0.0)
        n = max(2, int(round((u1 - u0) / 2.9)))
        for k in range(1, n):
            u = u0 + (u1 - u0) * k / n
            wbox(p, 'Dark#joint', F, u - 0.018, u + 0.018, 0.0, PZ - 0.21, -0.01, 0.004, bevel=0.0)
        wbox(p, 'Dark#joint', F, u0 + 0.05, u1 - 0.05, 0.42, 0.445, -0.01, 0.004, bevel=0.0)
    # as juntas do piso (a grade das lajes serradas)
    for k in range(1, 4):
        x = PX0 + (PX1 - PX0) * k / 4
        p.box('Dark#joint', x - 0.015, x + 0.015, PY0 + 0.1, PY1 - 0.1, PZ - 0.01, PZ + 0.003, bevel=0.0)
    for k in range(1, 5):
        y = PY0 + (PY1 - PY0) * k / 5
        p.box('Dark#joint', PX0 + 0.1, PX1 - 0.1, y - 0.015, y + 0.015, PZ - 0.01, PZ + 0.003, bevel=0.0)
    # a beira de carga: a cantoneira de aço (a aba de cima no piso e a da frente na pingadeira da laje), a faixa de perigo debaixo da
    # laje, os batentes de borracha e o número de estêncil gasto de cada baia
    p.box('Metal#edge', PX1 - 0.1, PX1 + 0.066, PY0 + 0.05, PY1 - 0.05, PZ - 0.004, PZ + 0.012, bevel=0.0)
    p.box('Metal#edge', PX1 + 0.05, PX1 + 0.068, PY0 + 0.05, PY1 - 0.05, PZ - 0.17, PZ + 0.012, bevel=0.0)
    F = WallFace('+x', PX1, 0.0)
    for (a, b) in ((PY0 + 0.3, STAIR[0] - 0.3), (STAIR[1] + 0.9, PY1 - 0.3)):
        wbox(p, 'Hazard', F, a, b, PZ - 0.44, PZ - 0.22, -0.01, 0.006, bevel=0.0)
    for k, y in enumerate(BUMPERS):
        vp.rbox(p, 'Rubber', (PX1 + 0.08, y, 0.53), (0.16, 0.5, 0.42), r=0.04, k=2)
        for dz in (-0.12, 0.12):
            vp.nut(p, 'Dark', (PX1 + 0.16, y, 0.53 + dz), (1, 0, 0), r=0.025, h=0.02)
        sp.stencil(p, (PX1 + 0.006, y + 0.62, 0.34), (0, -1, 0), (0, 0, 1), (1, 0, 0), '%d' % (k + 1), 0.34, seed=k + 3, wear=0.22)
    # o murinho do fundo e das pontas (fora das rampas)
    for (x0, x1, y0, y1) in ((PX0, PX0 + 0.25, PY0, PY1), (PX0 + 0.25, RAMP_P[0] - 0.1, PY1 - 0.25, PY1),
                             (PX0 + 0.25, RAMP_N[0] - 0.1, PY0, PY0 + 0.25), (RAMP_N[1] + 0.1, PX1 - 0.6, PY0, PY0 + 0.25)):
        p.box('Concrete#wall', x0, x1, y0, y1, PZ - 0.05, PZ + 0.85, bevel=0.03)
    # as faixas amarelas do corredor da empilhadeira (de rampa a rampa) e a linha da beira de carga
    for x in (1.45, 1.55):
        p.box('Yellow#line', x - 0.05, x + 0.05, PY0 + 0.4, PY1 - 0.4, PZ, PZ + 0.004, bevel=0.0)
    p.box('Yellow#line', PX1 - 0.75, PX1 - 0.65, PY0 + 0.4, PY1 - 0.4, PZ, PZ + 0.004, bevel=0.0)
    # a escada encostada na face de carga (sobe pra +Y) com o corrimão amarelo do lado de fora
    n = 7
    rise = PZ / n
    run = (STAIR[1] - STAIR[0]) / n
    for k in range(n):
        y0 = STAIR[0] + k * run
        p.box('Concrete#step', PX1, PX1 + 0.95, y0, STAIR[1] + 0.02, -0.05, (k + 1) * rise, bevel=0.012)
    p.box('Concrete#step', PX1, PX1 + 0.95, STAIR[1], STAIR[1] + 0.6, -0.05, PZ - 0.02, bevel=0.012)
    xr = PX1 + 0.9
    pts = [Vector((xr, STAIR[0] + 0.1, 0.95)), Vector((xr, STAIR[1] - 0.05, PZ + 0.95)), Vector((xr, STAIR[1] + 0.55, PZ + 0.95))]
    vp.tube(p, 'Yellow#rail', vp.bent_bar(pts, R=0.12, n=3), 0.025, seg=6)
    for y in (STAIR[0] + 0.15, (STAIR[0] + STAIR[1]) / 2, STAIR[1] + 0.5):
        zb = max(0.0, min(PZ, (y - STAIR[0]) / run * rise))
        vp.tube(p, 'Yellow#rail', [Vector((xr, y, zb)), Vector((xr, y, zb + 0.95))], 0.022, seg=6)


def ramp(p, x0, x1, y_top, sgn):
    """A rampa de concreto (do piso a 1,2 m até o chão em RAMP_L): a cunha, as muretas baixas dos lados acompanhando a descida, os frisos
    de aço atravessados (o antiderrapante), a chapa de pé no chão, o corrimão amarelo do lado de fora."""
    y_bot = y_top + sgn * RAMP_L
    pts = [(x0, y_top, -0.2), (x0, y_top, PZ), (x0, y_bot, 0.02), (x0, y_bot, -0.2)]
    prism(p, 'Concrete#ramp', pts, (x1 - x0, 0.0, 0.0))
    for xc in (x0 + 0.1, x1 - 0.1):
        prism(p, 'Concrete#lo', [(xc - 0.1, y_top, PZ - 0.1), (xc - 0.1, y_top, PZ + 0.18), (xc - 0.1, y_bot, 0.16), (xc - 0.1, y_bot, -0.1)],
              (0.2, 0.0, 0.0))
    k = 0
    while True:
        t = 0.06 + k * 0.33 / RAMP_L
        if t > 0.97:
            break
        y = y_top + sgn * RAMP_L * t
        z = PZ + (0.02 - PZ) * t
        p.box('Metal#cleat', x0 + 0.22, x1 - 0.22, y - 0.03, y + 0.03, z - 0.01, z + 0.035, bevel=0.0)
        k += 1
    p.box('Metal#cleat', x0 + 0.2, x1 - 0.2, y_bot - sgn * 0.02 - 0.12, y_bot - sgn * 0.02 + 0.12, -0.02, 0.03, bevel=0.0)
    xr = x1 - 0.1
    a, b = Vector((xr, y_top + sgn * 0.1, PZ + 0.18 + 0.9)), Vector((xr, y_bot - sgn * 0.25, 0.16 + 0.9))
    vp.tube(p, 'Yellow#rail', [a, b], 0.025, seg=6)
    for t in (0.04, 0.5, 0.94):
        q = a + (b - a) * t
        vp.tube(p, 'Yellow#rail', [Vector((q.x, q.y, q.z - 0.9)), q], 0.022, seg=6)


# ===================================================================================================== o galpão
def roof_z(x):
    return RIDGE - (RIDGE - EAVE) * abs(x - RIDGE_X) / ((SHED_X[1] - SHED_X[0]) / 2)


def shed(p):
    """O galpão de aço só no terço de trás: as 10 colunas de perfil I com a placa e os chumbadores, as vigas do beiral e a da
    cumeeira, as tesouras de cantoneira em cada pórtico (o banzo de cima seguindo as águas, o de baixo reto, os montantes e as
    diagonais), as mãos-francesas, os X de travamento nas baias das pontas, as terças, as chapas onduladas (umas enferrujadas, trocadas,
    tortas), a cumeeira, os 2 exaustores, a calha da frente com o tubo de descida."""
    for x in SHED_X:
        for y in SHED_Y:
            p.box('Metal#plate', x - 0.22, x + 0.22, y - 0.22, y + 0.22, PZ, PZ + 0.03, bevel=0.0)
            for (dx, dy) in ((-0.15, -0.15), (0.15, -0.15), (-0.15, 0.15), (0.15, 0.15)):
                vp.nut(p, 'Dark', (x + dx, y + dy, PZ + 0.03), (0, 0, 1), r=0.022, h=0.035)
            ibeam(p, (x, y, PZ + 0.03), (x, y, EAVE - 0.12), up=(1.0, 0.0, 0.0))
    # as vigas do beiral e a da cumeeira (correm em Y)
    for x in SHED_X:
        ibeam(p, (x, PY0 + 0.05, EAVE), (x, PY1 - 0.05, EAVE), w=0.18, h=0.28, up=(0.0, 0.0, 1.0))
    # as tesouras em cada pórtico
    xa, xb = SHED_X
    zb = EAVE - 0.05
    for y in SHED_Y:
        top = [(xa, y, EAVE + 0.12), (RIDGE_X, y, RIDGE + 0.05), (xb, y, EAVE + 0.12)]
        angle(p, top[0], top[1], s=0.12, up=(0.0, 1.0, 0.0))
        angle(p, top[1], top[2], s=0.12, up=(0.0, 1.0, 0.0))
        angle(p, (xa, y, zb), (xb, y, zb), s=0.1, up=(0.0, 1.0, 0.0))
        nseg = 6
        for k in range(1, nseg):
            x = xa + (xb - xa) * k / nseg
            angle(p, (x, y, zb), (x, y, roof_z(x) + 0.07), s=0.07, up=(0.0, 1.0, 0.0))
            if k < nseg - 1:
                x2 = xa + (xb - xa) * (k + 1) / nseg
                if x < RIDGE_X - 0.01:
                    angle(p, (x, y, zb), (x2, y, roof_z(x2) + 0.07), s=0.06, up=(0.0, 1.0, 0.0))
                elif x2 > RIDGE_X + 0.01:
                    angle(p, (x2, y, zb), (x, y, roof_z(x) + 0.07), s=0.06, up=(0.0, 1.0, 0.0))
        # as mãos-francesas das colunas na tesoura
        for x, s in ((xa, 1), (xb, -1)):
            angle(p, (x, y + 0.0, EAVE - 0.9), (x + s * 0.7, y, zb), s=0.07, up=(0.0, 1.0, 0.0))
    # os X de travamento nas baias das pontas (no plano das colunas de trás e no telhado)
    for (ya, yb) in ((SHED_Y[0], SHED_Y[1]), (SHED_Y[-2], SHED_Y[-1])):
        for (a, b) in (((xa, ya, PZ + 0.4), (xa, yb, EAVE - 0.3)), ((xa, yb, PZ + 0.4), (xa, ya, EAVE - 0.3))):
            vp.tube(p, 'Metal#rod', [Vector(a), Vector(b)], 0.02, seg=5)
        for (a, b) in (((xa + 0.2, ya, EAVE + 0.2), (RIDGE_X, yb, RIDGE + 0.05)), ((xa + 0.2, yb, EAVE + 0.2), (RIDGE_X, ya, RIDGE + 0.05))):
            vp.tube(p, 'Metal#rod', [Vector(a), Vector(b)], 0.016, seg=5)
    # as terças (o perfil C) correndo em Y em cima dos banzos
    for side in (-1, 1):
        for k in range(5):
            t = (k + 0.3) / 4.6
            x = RIDGE_X + side * t * (xb - xa) / 2 * 1.02
            z = roof_z(x) + 0.17
            p.box('Metal#purlin', x - 0.04, x + 0.04, PY0 - 0.1, PY1 + 0.1, z - 0.08, z + 0.08, bevel=0.0)
    # as chapas onduladas: 2 águas de painéis de ~1 m, umas enferrujadas (trocadas), cada uma com o seu desvio; o beiral de 0,55 m
    ov = 0.55
    n = 24
    y0r, y1r = PY0 - 0.35, PY1 + 0.35
    for side in (-1, 1):
        xe = (xa - ov) if side < 0 else (xb + ov)
        for k in range(n):
            ya = y0r + (y1r - y0r) * k / n - 0.04
            yb = y0r + (y1r - y0r) * (k + 1) / n + 0.04
            h = _hash(k, side + 5, 41)
            m = 'Rust#sheet' if h < 0.2 else 'Metal#sheet'
            lift = 0.255 + 0.012 * (k % 2) + (0.03 if h < 0.2 else 0.0)
            dz = (_hash(k, side + 5, 43) - 0.5) * 0.04
            ze = roof_z(xa if side < 0 else xb) - (RIDGE - EAVE) * ov / ((xb - xa) / 2) + lift + dz
            zr = RIDGE + lift + 0.02
            if side < 0:
                corrugated(p, xe, RIDGE_X + 0.02, ya, yb, ze, zr, axis='x', m=m, pitch=0.16, amp=0.03, bend=0.06)
            else:
                corrugated(p, RIDGE_X - 0.02, xe, ya, yb, zr, ze, axis='x', m=m, pitch=0.16, amp=0.03, bend=0.06)
    # a cumeeira (a chapa dobrada por cima da emenda das 2 águas)
    zc = RIDGE + 0.4
    for side in (-1, 1):
        p.obox('Metal#cap', (RIDGE_X + side * 0.16, 0.0, zc - 0.03), (0.34, y1r - y0r, 0.02), pitch=-side * 10.0, bevel=0.0)
    # os 2 exaustores na cumeeira (a turbina com as aletas e o chapéu)
    for y in (-5.6, 5.6):
        c = (RIDGE_X, y, zc)
        vp.lathe(p, 'Metal#vent', c, 'z', [(0.0, 0.0), (0.26, 0.0), (0.26, 0.12), (0.22, 0.16), (0.22, 0.2), (0.0, 0.2)], seg=16)
        for k in range(14):
            a = 2 * math.pi * k / 14
            q = Vector(c) + Vector((0.26 * math.cos(a), 0.26 * math.sin(a), 0.42))
            vp.rbox(p, 'Metal#vent', tuple(q), (0.025, 0.12, 0.42), yaw=math.degrees(a) + 30.0, r=0.006, k=1)
        vp.lathe(p, 'Metal#vent', (c[0], c[1], c[2] + 0.62), 'z', [(0.0, 0.0), (0.3, 0.0), (0.28, 0.05), (0.12, 0.13), (0.0, 0.15)], seg=16)
    # a calha da frente e o tubo de descida na coluna da ponta −Y
    xg = xb + ov - 0.02
    zg = roof_z(xb) - (RIDGE - EAVE) * ov / ((xb - xa) / 2) + 0.2
    p.box('Metal#gutter', xg - 0.08, xg + 0.08, y0r, y1r, zg - 0.14, zg - 0.02, bevel=0.0)
    vp.tube(p, 'Metal#gutter', vp.bent_bar([Vector((xg, PY0 - 0.2, zg - 0.1)), Vector((xb + 0.15, PY0 - 0.2, zg - 0.5)),
                                            Vector((xb + 0.15, PY0 - 0.2, PZ + 0.05))], R=0.12, n=3), 0.055, seg=8)


def under_shed(p):
    """O que fica debaixo do galpão (nada que pareça estoque): as pilhas de paletes vazios, os tambores, as caixas de madeira, a
    empilhadeira estacionada, a bancada."""
    z = PZ
    for (x, y, n, yaw, sd) in ((-10.9, -9.4, 6, 0.0, 1), (-10.8, -7.8, 3, 4.0, 2), (-8.6, -9.2, 2, 90.0, 3), (-11.0, 2.4, 5, 90.0, 4),
                               (-10.9, 7.9, 4, 0.0, 5)):
        for k in range(n):
            dx, dy = (_hash(k, sd, 1) - 0.5) * 0.08, (_hash(k, sd, 2) - 0.5) * 0.08
            sp.pallet(p, x + dx, y + dy, z + k * sp.PALLET_TOP, yaw + (_hash(k, sd, 3) - 0.5) * 6.0, seed=sd * 10 + k)
    # a caixa do estoque aberta e vazia (a tampa encostada) e a outra tombada: o que sobrou do que já saiu
    sp.crate_lite(p, -8.9, 6.6, z, yaw=12.0, lid=False)
    p.obox('Dark#lite', (-8.9, 6.6, z + 0.86), (1.0, 0.8, 0.02), yaw=12.0, bevel=0.0)
    p.obox('Crate#lid', (-8.25, 7.25, z + 0.55), (1.24, 1.04, 0.08), yaw=40.0, pitch=0.0, roll=68.0)
    p.obox('Yellow#lite', (-8.21, 7.29, z + 0.55), (1.18, 0.32, 0.012), yaw=40.0, roll=68.0, bevel=0.0)
    sp.crate_lite(p, -9.6, -6.2, z + 0.0, yaw=-20.0)
    for k, (x, y) in enumerate(((-11.6, -3.6), (-11.0, -3.8), (-11.35, -3.05), (-10.7, -3.25), (-11.6, 9.9), (-11.0, 10.1))):
        sp.drum(p, x, y, z, m='Rust#drum' if k % 3 else 'Metal#drum')
    sp.drum(p, -9.8, -3.4, z, lying=True, yaw=80.0)
    for k, (x, y, yaw) in enumerate(((-11.4, 5.0, 0.0), (-11.3, 5.9, 3.0), (-11.35, 5.4, -4.0))):
        crate(p, x, y, z + (0.0 if k < 2 else 0.55), size=(0.9, 0.8, 0.55), yaw=yaw, m='Wood#crate', strap='Dark')
    sp.forklift(p, -9.3, 0.4, yaw=170.0, seed=5)
    # a bancada de ferro com o torno e a caixa de ferramentas
    p.box('Metal#bench', -12.2, -11.5, -1.6, -0.2, z + 0.85, z + 0.9, bevel=0.0)
    for (dx, dy) in ((0.05, 0.05), (0.65, 0.05), (0.05, 1.35), (0.65, 1.35)):
        p.box('Dark', -12.2 + dx - 0.025, -12.2 + dx + 0.025, -1.6 + dy - 0.025, -1.6 + dy + 0.025, z, z + 0.85, bevel=0.0)
    vp.rbox(p, 'Hazard', (-11.85, -0.6, z + 0.98), (0.45, 0.22, 0.16), r=0.02, k=1)


# ===================================================================================================== a guarita e os postes
def guardhouse(p):
    """A guarita de reboco da vila (o kit civil: a quina gasta, o rodapé, os remendos) com a porta de ferro virada pra faixa, as
    janelas de grade, o parapeito com os dentes e, no teto, a caixa d'água no cavalete."""
    x0, x1, y0, y1 = GUARD
    holes = {'+x': [(13.0, 13.95, 0.0, 2.1), (14.5, 15.3, 1.0, 1.9)], '-y': [(-11.6, -10.6, 1.0, 1.9)],
             '-x': [(13.6, 14.4, 1.2, 1.9)], '+y': [(-11.2, -10.2, 1.1, 1.9)]}
    hp.house_block(p, x0, x1, y0, y1, GUARD_Z, t=0.25, holes=holes, seed=21, spalls=2, cracks=1, wear=0.8, mud=0.3, pocks=6)
    at = {'+x': x1, '-x': x0, '+y': y1, '-y': y0}
    for f, hs in holes.items():
        F = WallFace(f, at[f], 0.0)
        for k, (u0, u1, z0, z1) in enumerate(hs):
            if z0 < 0.1:
                hp.steel_door(p, F, u0, u1, z0, z1, t=0.25, seed=k)
            else:
                hp.window(p, F, u0, u1, z0, z1, t=0.25, kind='grille', seed=k + len(f) * 3, wear=0.8)
    hp.parapet(p, x0, x1, y0, y1, GUARD_Z, h=0.45, t=0.2, seed=23, wear=0.8)
    hp.water_tank(p, ((x0 + x1) / 2 - 0.4, (y0 + y1) / 2 + 0.2, GUARD_Z), r=0.62, h=1.2, stand=0.9, seed=7)
    hp.roof_ac(p, (x1 - 0.9, y0 + 0.8, GUARD_Z), yaw=0.0)
    hp.spout(p, WallFace('-y', y0, 0.0), x0 + 0.8, GUARD_Z + 0.05)
    hp.antenna(p, x0 + 0.5, y1 - 0.5, GUARD_Z, h=1.8)
    # o número do depósito em estêncil na parede da frente
    sp.stencil(p, (x1 + 0.012, 15.35, 2.2), (0, -1, 0), (0, 0, 1), (1, 0, 0), '04', 0.42, seed=12, wear=0.2)


# ===================================================================================================== o estoque
def stock(p, st):
    """As fileiras de pilhas na plataforma (no aberto: a frente do galpão em x −7,15) e as 2 do pátio, uma em cada ponta."""
    z = PZ
    st.stack(p, (3.45, -9.1), [[2, 2], [2, 3]], yaw=2.5, z0=z, seed=1)
    st.stack(p, (3.7, -4.3), [[2, 2], [1, 2]], yaw=88.0, z0=z, seed=2)
    st.stack(p, (3.4, 2.6), [[2, 2], [2, 1]], yaw=-3.0, z0=z, seed=3)
    st.stack(p, (3.55, 7.8), [[2, 3], [2, 2]], yaw=1.5, z0=z, seed=4)
    st.stack(p, (-0.75, -7.7), [[2, 3, 2], [2, 2, 1]], yaw=-1.5, z0=z, seed=5)
    st.stack(p, (-0.3, -1.6), [[2, 2], [2, 2]], yaw=3.5, z0=z, seed=6)
    st.stack(p, (-0.6, 4.6), [[2, 2], [2, 2]], yaw=91.5, z0=z, seed=7)
    st.stack(p, (-4.4, -3.4), [[2, 2], [2, 2]], yaw=86.0, z0=z, seed=8)
    st.stack(p, (-4.7, 2.3), [[3, 2], [2, 1]], yaw=-2.0, z0=z, seed=9)
    # o pátio: uma pilha em cada ponta, no chão
    st.stack(p, (-2.0, -13.9), [[2, 2], [2, 2]], yaw=90.0, z0=0.0, seed=10)
    st.stack(p, (-4.6, 13.9), [[2, 1], [2, 1]], yaw=90.0, z0=0.0, seed=11)
    # 2 soltas na beira de carga (as que esperam o caminhão)
    st.loose([[(4.6, -0.9 + 0.3, z, 7.0)], [(4.75, 5.15, z, -12.0)]])


def base():
    p = sp.new_part(PREFIX + '_Base', seed=11, dirt=0.75)
    sp.ground_pad(p, -13.4, 6.4, -16.3, 16.3, r=3.0, h=0.04, fall=1.6, step=0.8, seed=9)
    sp.lane_pad(p, LANE_X, -16.6, 16.6, half=LANE_HALF, seed=9)
    platform(p)
    ramp(p, RAMP_P[0], RAMP_P[1], PY1, 1)
    ramp(p, RAMP_N[0], RAMP_N[1], PY0, -1)
    shed(p)
    under_shed(p)
    guardhouse(p)
    # a vida na plataforma: os paletes que já esvaziaram jogados no corredor, a paleteira, a cinta de carga
    for (x, y, yaw, sd) in ((1.6, -5.9, 14.0, 81), (1.25, 9.9, -7.0, 82), (-2.5, 1.2, 63.0, 83)):
        sp.pallet(p, x, y, PZ, yaw, seed=sd)
    sp.pallet_jack(p, 1.75, 0.9, PZ, yaw=160.0)
    sp.flood_pole(p, -12.55, -14.9, 0.0, h=8.6, aim=50.0)
    sp.flood_pole(p, 5.72, 14.9, 0.0, h=8.6, aim=215.0)
    st = sp.Stock()
    stock(p, st)
    return sp.finish(p), st


def main():
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'supply_dock_v2')
    os.makedirs(out, exist_ok=True)
    clear_scene()
    o, st = base()
    total = export(o, out)
    pile = st.write(out)
    sp.write_preview(out, PREFIX, st, three_quarter=(34.0, 215.0), usa_at=(8.8, -20.0, 0.0), truck=(LANE_X, 1.5, 90.0))
    print('MODEL total', total, 'tris; vagas', len(pile), '; lote 26.33 x 32.12 m; faixa x', LANE_X - LANE_HALF, '..', LANE_X + LANE_HALF,
          'entra (%.1f, %.2f) sai (%.1f, %.2f)' % (LANE_X, LOT[2], LANE_X, LOT[3]))
    print('MODEL plataforma x', PX0, '..', PX1, 'y', PY0, '..', PY1, 'piso', PZ, '; beiral', EAVE, 'cumeeira', RIDGE)


if __name__ == '__main__':
    main()
