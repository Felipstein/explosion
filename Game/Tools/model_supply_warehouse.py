"""O Supply básico GRANDE da VILA, O ARMAZÉM (o SupplyWarehouse do ZH, 400 caixas = 30 000 $), 03/11/2026: o desenho 3 do conceito
escolhido docs/concept/buildings/supply_basico_4_tamanhos_ESCOLHIDO.png, com 2 mudanças pedidas: o toldo de chapa CURTO (só a
marquise) e o estoque nos paletes no PÁTIO DA FRENTE, no aberto (da câmera do jogo o jogador conta o dinheiro pelas caixas).
A família: model_supply_basic_parts.py; a linguagem das casas da vila: model_civ_house_parts.py.

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_supply_warehouse.py C:/Users/luisf/explosion/captures/game/models/supply_warehouse

A palavra (o que lê em 1 segundo): O ARMAZÉM COMPRIDO DE REBOCO COM AS 3 PORTAS ABERTAS E O PÁTIO DE PILHAS NA FRENTE. O bloco térreo
alto da vila (a quina gasta, o rodapé, os remendos de reboco e de barro, as placas caídas, os buracos de tiro, as vergas de madeira) de
teto plano com o parapeito de dentes, as 3 portas grandes abertas (a porta de enrolar recolhida na caixa, as guias; dentro, o piso de
carga e as caixas de enfeite nos paletes), a marquise curta de chapa ondulada nas vigas de madeira com os tirantes, o degrau de carga de
concreto na frente toda com as 3 escadas (as 2 pontas e o meio); no teto as caixas d'água, os condicionadores, a claraboia, a antena,
a escada de mão na empena. Na frente, o pátio de terra batida com as 2 fileiras de pilhas do estoque nos paletes, a empilhadeira, os
paletes vazios e os tambores. Neutro: só os números de estêncil gastos (as portas, o depósito).

Lote 26 × 32 m (x −13..13; y −16..16), frente em +X. A FAIXA DOS CAMINHÕES no lado +X (x 6,1..11,1, o meio em x 8,6): entra em
(8,6; −16) e sai em (8,6; +16), livre. O armazém: x −12,6..−4,6, y −12,4..12,4; o degrau x −4,6..−2,8 (0,9 m); o estoque no pátio
x −1,4..5,4 (o pátio todo, que bloqueia, x −13..6,2).
Alturas: o piso de carga 0,9 m; as portas até 4,0 m; o teto 4,6 m, o parapeito 5,2 m (as caixas d'água até ~6,9 m); a marquise
4,3 → 4,0 m (1,45 m de fundo); as pilhas 2,0 m (as colunas de 3: 2,98 m).

Peças: SM_SupplyWarehouse_Base (tudo menos as caixas do estoque) e o stock_layout.json (as vagas, na ordem em que somem).
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import clear_scene, export  # noqa: E402
import model_supply_basic_parts as sp  # noqa: E402
from model_supply_basic_parts import PX, PY, STACK_DZ, PALLET_TOP  # noqa: E402
import model_civ_house_parts as hp  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
from model_building_parts import WallFace, wbox  # noqa: E402
from model_gla_palace_v2 import _hash  # noqa: E402

PREFIX = 'SM_SupplyWarehouse'
LOT = (-13.0, 13.0, -16.0, 16.0)
LANE_X, LANE_HALF = 8.6, 2.5
X0, X1, Y0, Y1 = -12.6, -4.6, -12.4, 12.4
ZR, T = 4.6, 0.3
FLOOR = 0.9                                  # o piso de carga (o do degrau e o de dentro)
DOORS = [(-9.8, -5.4), (-2.2, 2.2), (5.4, 9.8)]
DOOR_TOP = 4.0
STEP_X = (X1, -2.8)                          # o degrau de carga
STEP_Y = (-12.0, 12.0)
CAN_X = -3.15                                # a beira da marquise (1,45 m: da câmera a 37,5–55° a porta aparece)
CAN_Z = (4.32, 4.02)                         # na parede, na beira


def building(p):
    """O armazém: o bloco da vila (house_block) furado pelas 3 portas grandes e pelas janelas altas, o piso de carga de dentro, as portas
    de enrolar recolhidas (a caixa do rolo, as guias), as janelas de grade com a verga de madeira, o parapeito de dentes, as coisas do
    teto, a escada de mão, os números de estêncil."""
    holes = {'+x': [(a, b, FLOOR, DOOR_TOP) for (a, b) in DOORS] + [(-4.0, -3.3, 3.1, 3.7), (3.3, 4.0, 3.1, 3.7)],
             '-x': [(-9.4, -8.3, 2.5, 3.4), (-3.6, -2.5, 2.5, 3.4), (2.5, 3.6, 2.5, 3.4), (8.3, 9.4, 2.5, 3.4)],
             '-y': [(-11.5, -10.4, 2.4, 3.3), (-9.1, -8.0, 2.4, 3.3)],
             '+y': [(-9.6, -8.4, 2.4, 3.3)]}
    hp.FINE['u'] = 0.4                        # (o orçamento: o prédio de 25 m) a estria do escorrido a cada 40 cm
    hp.house_block(p, X0, X1, Y0, Y1, ZR, t=T, holes=holes, seed=31, spalls=3, cracks=2, wear=0.95, mud=0.4, pocks=5, patches=3)
    at = {'+x': X1, '-x': X0, '+y': Y1, '-y': Y0}
    for f, hs in holes.items():
        F = WallFace(f, at[f], 0.0)
        for k, (u0, u1, z0, z1) in enumerate(hs):
            if f == '+x' and k < 3:
                continue
            hp.window(p, F, u0, u1, z0, z1, t=T, kind='grille' if f != '+x' else 'dark', seed=k + 7 * len(f) + ord(f[1]), wear=0.95,
                      lintel='wood')
    # o piso de carga de dentro (o pé das portas na altura do degrau)
    p.box('Concrete#floor', X0 + T - 0.02, X1 - T + 0.02, Y0 + T - 0.02, Y1 - T + 0.02, 0.06, FLOOR, bevel=0.0)
    # as portas abertas: o rolo recolhido na caixa em cima, as guias dos lados, a soleira de aço
    F = WallFace('+x', X1, 0.0)
    for k, (a, b) in enumerate(DOORS):
        wbox(p, 'Metal#frame', F, a, a + 0.12, FLOOR, DOOR_TOP, -0.2, -0.05, bevel=0.0)
        wbox(p, 'Metal#frame', F, b - 0.12, b, FLOOR, DOOR_TOP, -0.2, -0.05, bevel=0.0)
        wbox(p, 'Metal#roll', F, a - 0.05, b + 0.05, DOOR_TOP - 0.42, DOOR_TOP + 0.02, -0.26, -0.02)
        wbox(p, 'Rust#shutter', F, a + 0.1, b - 0.1, DOOR_TOP - 0.5, DOOR_TOP - 0.38, -0.2, -0.12, bevel=0.0)
        wbox(p, 'Metal#edge', F, a, b, FLOOR - 0.02, FLOOR + 0.012, -T - 0.05, 0.02, bevel=0.0)
        # o número da porta em estêncil gasto no pilar da esquerda
        sp.stencil(p, (X1 + 0.016, b + 0.85, 2.25), (0, -1, 0), (0, 0, 1), (1, 0, 0), '%d' % (k + 1), 0.5, seed=k + 21, wear=0.2)
    # o número do depósito, grande, na empena −Y (a que a câmera do jogo vê)
    sp.stencil(p, (-5.4, Y0 - 0.03, 1.5), (-1, 0, 0), (0, 0, 1), (0, -1, 0), '07', 1.15, seed=31, wear=0.08, t=0.006)
    # o parapeito, as coisas do teto
    hp.parapet(p, X0, X1, Y0, Y1, ZR, h=0.62, t=0.22, seed=33, wear=0.9, rebar_at=[(X0 + 0.21, Y1 - 0.21)])
    hp.water_tank(p, (X0 + 1.5, Y0 + 2.2, ZR), r=0.6, h=1.2, stand=0.9, seed=3)
    hp.water_tank(p, (X0 + 1.3, 7.6, ZR), r=0.55, h=1.1, stand=0.0, seed=4)
    hp.roof_ac(p, (X1 - 1.2, -6.5, ZR), yaw=0.0)
    hp.roof_ac(p, (X1 - 1.2, 5.2, ZR), yaw=0.0)
    hp.tank_horizontal(p, (-9.0, 10.6, ZR), r=0.42, L=1.6, yaw=90.0, stand=0.55)
    hp.sat_dish(p, (-6.0, -10.8, ZR), r=0.42, yaw=200.0, tilt=34.0)
    hp.antenna(p, X0 + 0.7, -0.6, ZR, h=2.1)
    hp.roof_junk(p, -11.5, -8.5, -3.0, 1.0, ZR, n=3, seed=5)
    hp.roof_junk(p, -8.0, -6.0, 6.5, 9.0, ZR, n=2, seed=7)
    # a claraboia (o quadro de aço com o vidro escuro) e o alçapão
    cx, cy = -8.6, -2.0
    vp.rbox(p, 'Metal#sky', (cx, cy, ZR + 0.18), (1.6, 2.4, 0.36), r=0.05, k=2)
    vp.rbox(p, 'Glass', (cx, cy, ZR + 0.37), (1.4, 2.2, 0.03), r=0.04, k=2)
    for k in range(3):
        vp.rbox(p, 'Metal#sky', (cx, cy - 0.55 + 0.55 * k, ZR + 0.39), (1.42, 0.05, 0.03), r=0.01, k=1)
    vp.rbox(p, 'Metal#hatch', (-10.8, 4.0, ZR + 0.06), (0.8, 0.8, 0.12), r=0.03, k=1)
    for (f, u) in (('-x', -6.0), ('-x', 6.0), ('-y', -6.0), ('+y', -6.0)):
        hp.spout(p, WallFace(f, at[f], 0.0), u, ZR + 0.08)
    # a escada de mão encostada na empena +Y (a do conceito) e o eletroduto com a caixa de luz
    hp.ladder(p, (-6.2, Y1 + 0.95, 0.0), (-6.2, Y1 + 0.06, ZR + 0.95))
    vp.rbox(p, 'Metal#meter', (X1 + 0.08, 11.2, 2.4), (0.14, 0.34, 0.42), r=0.03, k=2)
    vp.tube(p, 'Dark#cable', vp.bent_bar([Vector((X1 + 0.06, 11.2, 2.62)), Vector((X1 + 0.06, 11.2, ZR + 0.3)),
                                          Vector((X1 - 0.05, 11.2, ZR + 0.68))], R=0.1, n=3), 0.014, seg=6)


def step(p):
    """O degrau de carga de concreto na frente toda (0,9 m), com a cantoneira de aço na beira, as juntas, as 3 escadas (as 2 pontas e o
    meio) e o corrimão amarelo da escada do meio."""
    xa, xb = STEP_X
    ya, yb = STEP_Y
    p.box('Concrete', xa - 0.05, xb, ya, yb, -0.25, FLOOR - 0.14)
    p.box('Concrete#slab', xa - 0.05, xb + 0.05, ya - 0.05, yb + 0.05, FLOOR - 0.18, FLOOR, bevel=0.03)
    p.box('Metal#edge', xb - 0.08, xb + 0.066, ya + 0.1, yb - 0.1, FLOOR - 0.004, FLOOR + 0.012, bevel=0.0)
    p.box('Metal#edge', xb + 0.05, xb + 0.068, ya + 0.1, yb - 0.1, FLOOR - 0.17, FLOOR + 0.012, bevel=0.0)
    F = WallFace('+x', xb, 0.0)
    for k in range(1, 8):
        u = ya + (yb - ya) * k / 8
        if abs(u) < 1.0:
            continue
        wbox(p, 'Dark#joint', F, u - 0.018, u + 0.018, 0.0, FLOOR - 0.19, -0.01, 0.004, bevel=0.0)
    # a escada do meio (desce pra +X) e as das 2 pontas (descem pra ±Y)
    n = 5
    rise = FLOOR / n
    for k in range(n):
        x0 = xb + 1.5 - (k + 1) * 0.3
        p.box('Concrete#step', xb - 0.02, x0 + 0.3, -0.8, 0.8, -0.05, (k + 1) * rise, bevel=0.012)
    for sgn, yy in ((1, yb), (-1, ya)):
        for k in range(n):
            y0 = yy + sgn * (1.5 - (k + 1) * 0.3)
            lo, hi = sorted((yy - sgn * 0.02, y0 + sgn * 0.3))
            p.box('Concrete#step', xa + 0.1, xb - 0.1, lo, hi, -0.05, (k + 1) * rise, bevel=0.012)
    for s in (-1, 1):
        a = Vector((xb + 1.35, s * 0.72, 0.95))
        b = Vector((xb - 0.1, s * 0.72, FLOOR + 0.95))
        vp.tube(p, 'Yellow#rail', [a, b], 0.025, seg=6)
        for t in (0.05, 0.95):
            q = a + (b - a) * t
            zb = max(0.0, min(FLOOR, (xb + 1.5 - q.x) / 0.3 * rise))
            vp.tube(p, 'Yellow#rail', [Vector((q.x, q.y, zb)), q], 0.022, seg=6)


def canopy(p):
    """A marquise curta (1,45 m: cobre o pé das portas, não o pátio): a viga de madeira chumbada na parede, os caibros, a viga da beira, a chapa
    ondulada em painéis (a maioria enferrujada, umas galvanizadas novas, cada uma com o seu desvio), os tirantes de ferro do alto da
    parede até a beira (a marquise não tem pilar: o pé do degrau fica livre)."""
    zw, ze = CAN_Z
    ya, yb = Y0 + 0.1, Y1 - 0.1
    p.box('Wood#beam', X1 + 0.02, X1 + 0.18, ya, yb, zw - 0.32, zw - 0.12, bevel=0.012)
    p.box('Wood#beam', CAN_X - 0.06, CAN_X + 0.08, ya, yb, ze - 0.24, ze - 0.06, bevel=0.012)
    n = int((yb - ya) / 1.0)
    for k in range(n + 1):
        y = ya + (yb - ya) * k / n
        za = zw - 0.12
        zc = ze - 0.06
        L = ((CAN_X + 0.08 - X1) ** 2 + (za - zc) ** 2) ** 0.5
        ang = math.degrees(math.atan2(za - zc, CAN_X - X1))
        p.obox('Wood#rafter', ((X1 + CAN_X) / 2 + 0.05, y, (za + zc) / 2 + 0.03), (L, 0.07, 0.12), pitch=-ang, bevel=0.0)
    for k in range(n):
        y_a = ya + (yb - ya) * k / n - 0.03
        y_b = ya + (yb - ya) * (k + 1) / n + 0.03
        h = _hash(k, 3, 51)
        m = 'Metal#sheet' if h < 0.22 else 'Rust#sheet'
        dz = (_hash(k, 3, 53) - 0.5) * 0.03 + 0.012 * (k % 2)
        hp.corrugated(p, X1 + 0.04, CAN_X + 0.14, y_a, y_b, zw + 0.03 + dz, ze + 0.03 + dz * 0.5, axis='x', m=m, pitch=0.1, amp=0.025)
    for y in (-11.4, -6.6, -3.6, 0.0, 3.6, 6.6, 11.4):
        a = Vector((X1 + 0.05, y, ZR + 0.25))
        b = Vector((CAN_X + 0.02, y, ze - 0.08))
        vp.tube(p, 'Dark#rod', [a, b], 0.016, seg=5)
        vp.rbox(p, 'Dark#rod', (X1 + 0.06, y, ZR + 0.25), (0.04, 0.12, 0.16), r=0.01, k=1)


def inside(p):
    """O de dentro, que se vê pelas portas: as caixas de ENFEITE nos paletes (não contam), uns paletes vazios, os tambores."""
    z = FLOOR
    for k, (a, b) in enumerate(DOORS):
        yc = (a + b) / 2
        for (dx, dy, hgt, yaw) in ((-1.9, -1.0, 3, 0.0), (-1.9, 0.5, 2, 0.0), (-4.2, -0.6, 3, 90.0), (-4.3, 1.3, 2, 90.0)):
            x, y = X1 + dx, yc + dy + (k - 1) * 0.15
            sp.pallet(p, x, y, z, yaw, seed=k * 10 + int(dx * 3))
            for j in range(hgt):
                sp.crate_lite(p, x + (_hash(k, j, 3) - 0.5) * 0.04, y, z + PALLET_TOP + j * STACK_DZ, yaw + (_hash(k, j, 4) - 0.5) * 3)
        sp.pallet_pile(p, X1 - 1.6, yc + 1.75, 3 + k, yaw=90.0, seed=40 + k)
    sp.drum(p, X1 - 0.9, DOORS[0][0] + 0.5, z)
    sp.drum(p, X1 - 0.95, DOORS[2][1] - 0.45, z, m='Metal#drum')


def stock(p, st):
    """As 2 fileiras de pilhas do estoque no pátio da frente (no aberto) e as soltas."""
    st.stack(p, (4.0, -10.0), [[2, 2], [2, 3]], yaw=0.0, seed=1)
    st.stack(p, (4.0, -5.1), [[2, 2], [2, 2]], yaw=90.0, seed=2)
    st.stack(p, (4.0, 0.6), [[2, 2, 2], [2, 3, 2]], yaw=0.0, seed=3)
    st.stack(p, (4.0, 6.0), [[2, 2], [3, 2]], yaw=0.0, seed=4)
    st.stack(p, (4.0, 10.6), [[2, 1], [2, 2]], yaw=90.0, seed=5)
    st.stack(p, (0.0, -8.4), [[2, 2, 2], [2, 2, 2]], yaw=0.0, seed=6)
    st.stack(p, (0.0, -2.6), [[2, 2], [3, 3]], yaw=0.0, seed=7)
    st.stack(p, (0.0, 3.4), [[2, 2], [2, 2]], yaw=90.0, seed=8)
    st.stack(p, (0.0, 8.8), [[2, 2, 2], [2, 2, 1]], yaw=0.0, seed=9)
    st.loose([[(-1.7, 6.1, 0.0, 12.0)], [(-1.6, -5.2, 0.0, -18.0)], [(1.6, 12.6, 0.0, 25.0)]])


def yard(p):
    """O resto do pátio: a empilhadeira na ponta −Y, a pilha de paletes vazios e os tambores na ponta +Y."""
    sp.forklift(p, -0.4, -13.4, yaw=8.0, seed=7)
    sp.pallet_pile(p, -0.5, 13.7, 6, yaw=90.0, seed=12)
    for (x, y) in ((2.1, 13.9), (2.7, 14.05), (2.4, 14.55)):
        sp.drum(p, x, y, m='Rust#drum')
    sp.drum(p, 3.4, 14.4, lying=True, yaw=30.0)


def base():
    p = sp.new_part(PREFIX + '_Base', seed=31, dirt=0.95)
    sp.ground_pad(p, -13.2, 6.3, -15.9, 15.9, r=3.0, h=0.04, fall=1.6, step=0.8, seed=13)
    sp.lane_pad(p, LANE_X, -16.6, 16.6, half=LANE_HALF, seed=13)
    building(p)
    step(p)
    canopy(p)
    inside(p)
    yard(p)
    st = sp.Stock()
    stock(p, st)
    return sp.finish(p), st


def main():
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'supply_warehouse')
    os.makedirs(out, exist_ok=True)
    clear_scene()
    o, st = base()
    total = export(o, out)
    pile = st.write(out)
    sp.write_preview(out, PREFIX, st, three_quarter=(34.0, 215.0), usa_at=(8.8, -20.0, 0.0), truck=(LANE_X, 1.5, 90.0))
    print('MODEL total', total, 'tris; vagas', len(pile), '; lote 26 x 32 m; faixa x', LANE_X - LANE_HALF, '..', LANE_X + LANE_HALF,
          'entra (%.1f, %.1f) sai (%.1f, %.1f)' % (LANE_X, LOT[2], LANE_X, LOT[3]))


if __name__ == '__main__':
    main()
