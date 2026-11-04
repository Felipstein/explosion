"""O poste de CONCRETO da cidade (kit civil v3, 03/11/2026; docs/elenco/TAREFAS.md item 64) e o mesmo com a luminária de rua.

Rodar (Blender sem janela, só CPU), com o detalhe AAA e a máscara em todos os materiais:
  RTS_VC_FIX=1 "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_civ_pole_concrete.py C:/Users/luisf/explosion/captures/game/models/civ_v3/pole

A palavra: O POSTE DE CONCRETO EM H COM OS FUROS (a "escada"). O poste pré-moldado de seção em H que se vê nas cidades do Oriente
Médio e do norte da África (Iraque, Síria, Egito, Líbia): as 2 abas de concreto afinando pra cima (0,30 → 0,17 m de fundo), a alma
entre elas vazada pelos furos de pontas redondas de 0,9 a 6,9 m (o eletricista sobe por eles), as quinas chanfradas; o pé lascado
com o ferro da armadura enferrujado aparecendo (a barra do canto e o estribo), 2 lascas menores mais acima, o bloco de concreto do pé.
No alto, a cruzeta de AÇO: 2 cantoneiras galvanizadas apertando o poste dos 2 lados (o parafuso passante pela alma), as chapas dos
pinos, as 4 mãos-francesas de ferro chato na braçadeira; os mesmos isoladores de vidro do poste de madeira. A caixa de luz de chapa
(a porta com a dobradiça, o trinco, a aba de chuva, a ferrugem no pé) na face −X com o eletroduto descendo pro chão e subindo até a
cruzeta; os cartazes rasgados na face +X.
SM_civ_pole_concrete_lamp: o mesmo com a LUMINÁRIA DE RUA (o braço de tubo curvo com a escora, a cabeça "cobra" com o refrator de
vidro embaixo e a fotocélula em cima) saindo pra +X a 6,2–6,8 m, alcance de 2,25 m.

TROCÁVEL COM O POSTE DE MADEIRA (as medidas do jogo, model_civ_pole.py; WorldDetail.h PoleInsulator()): a cruzeta ao longo de X a
7,25–7,40 m (a cantoneira de 7,335 a 7,395, a chapa dos pinos até 7,403), os 4 isoladores em x = ±0,35 e ±0,95 (y = 0) com o topo a
7,60 m (o fio sai a 7,572 m, no pescoço), o topo a 8,0 m, a origem no pé, a pegada 0,4 × 0,4 m (o pé de 0,30 × 0,20 m e o bloco de
0,40 × 0,36 m). Nada de ponta de fio modelada.
A LUMINÁRIA APONTA PRA +X LOCAL: o gerador põe a da rua com o +X virado pro asfalto (o poste está a 1,5 m da beira; o braço passa
2,25 m pra dentro da pista, a 6,2–6,9 m de altura: não bloqueia).
Materiais (os slots do kit civil): Concrete (o poste, o bloco), Broken (o concreto lascado), Rust (a armadura, a ferrugem da caixa),
Metal (a cruzeta, as ferragens, a caixa, a luminária), Lens (o vidro dos isoladores e do refrator), Dark (o eletroduto, a fotocélula),
Cloth e Plaster (os cartazes).
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import Part, clear_scene, export  # noqa: E402
import model_civ_parts as cp  # noqa: E402
from model_building_parts import bake_in_place  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
from model_civ_pole import INS_X, TOP, insulator, flat_bar, _faces_mat, fix_orientation  # noqa: E402

Z_BOT = -0.15               # o poste enterrado (some no chão; pouco, como o de madeira: o tanque derruba girando pelo pé)
ANG_TOP = 7.395             # o topo das cantoneiras da cruzeta
ANG_W, ANG_T = 0.06, 0.006
ANG_Y = 0.07                # a face de dentro da cantoneira (encostada na aba)
CH = 0.012                  # o chanfro das quinas do poste
# as lascas: (lado da aba ±1, a quina, z0, z1, fundura); a do pé mostra a armadura
CHIPS = [(1, 'out+', 0.08, 0.8, 0.07), (-1, 'out-', 2.85, 3.3, 0.03), (1, 'in-', 5.05, 5.4, 0.024), (-1, 'out+', 0.1, 0.32, 0.025)]


def D(z):
    """O fundo (X) de aba a aba."""
    return 0.30 - 0.13 * min(max(z, 0.0), TOP) / TOP


def W(z):
    """A largura da aba (Y)."""
    return 0.20 - 0.07 * min(max(z, 0.0), TOP) / TOP


def TF(z):
    return 0.055 - 0.015 * min(max(z, 0.0), TOP) / TOP


def TW(z):
    return 0.065 - 0.015 * min(max(z, 0.0), TOP) / TOP


def chip_depth(k, z):
    s, c, z0, z1, d = CHIPS[k]
    if not (z0 < z < z1):
        return 0.0
    u = (z - z0) / (z1 - z0)
    return d * max(0.0, math.sin(math.pi * u)) ** 0.6 * (0.7 + 0.6 * cp.vnoise3(z * 9.0, k * 2.3, 0.0, 31))


def flange_ring(s, z, shrink=0.0):
    """A seção da aba do lado s (x > 0 pra s = +1) na altura z: o retângulo de quinas chanfradas, com as lascas. Devolve os 8 pontos
    (x, y) e a lista dos índices das faces lascadas (a face entre o ponto i e o i+1)."""
    xo, xi = s * (D(z) / 2 - shrink), s * (D(z) / 2 - TF(z) + shrink)
    w = W(z) / 2 - shrink
    c = CH
    pts = [[xo, -w + c], [xo, w - c], [xo - s * c, w], [xi + s * c, w], [xi, w - c], [xi, -w + c], [xi + s * c, -w], [xo - s * c, -w]]
    chipped = []
    for k, (cs, corner, z0, z1, d0) in enumerate(CHIPS):
        if cs != s:
            continue
        d = chip_depth(k, z)
        if d <= 0.0:
            continue
        # (03/11) o recuo pela face de lado não passa da espessura da aba (senão a face se cruza e sai do avesso)
        dx = min(d, TF(z) - 2 * c - 0.006)
        if corner == 'out+':
            pts[1][1] -= d
            pts[2][0] -= s * dx
            if d > 0.006:
                chipped.append(1)
        elif corner == 'out-':
            pts[0][1] += d
            pts[7][0] -= s * dx
            if d > 0.006:
                chipped.append(7)
        elif corner == 'in-':
            pts[5][1] += d * 0.6
            pts[6][0] += s * dx
            if d > 0.006:
                chipped.append(5)
    return [tuple(q) for q in pts], chipped


def flanges(p):
    zs = [Z_BOT, 0.0]
    z = 0.05
    while z < TOP - 0.05:
        dense = any(z0 - 0.08 < z < z1 + 0.08 for (_, _, z0, z1, _) in CHIPS)
        zs.append(round(z, 3))
        z += 0.06 if dense else 0.135
    zs += [TOP - 0.015, TOP]
    for s in (-1, 1):
        rings, chips = [], []
        for z in zs:
            pts, ch = flange_ring(s, z, shrink=0.01 if z >= TOP else 0.0)
            rings.append([(x, y, z) for (x, y) in pts])
            chips.append(ch)

        def matfn(i, j, chips=chips):
            return 'Broken#chip' if (j in chips[i] and j in chips[i + 1]) else 'Concrete#pole'
        _faces_mat(p, rings, matfn, caps=('Concrete#pole', 'Concrete#pole'), smooth=False)


def web(p):
    """A alma entre as abas, vazada pelos furos de pontas redondas: as travessas (o polígono com os arcos dos furos) extrudadas em Y."""
    def ch(z):
        return D(z) / 2 - TF(z) + 0.004

    holes = []
    z = 0.9
    while True:
        hh = 0.30 - 0.08 * z / 7.0
        if z + hh > 6.9:
            break
        holes.append((z, z + hh))
        z += hh + 0.13
    def arc(zc, rr, a0, a1, n=7):
        return [(rr * math.cos(math.radians(a0 + (a1 - a0) * i / n)), zc + rr * math.sin(math.radians(a0 + (a1 - a0) * i / n)))
                for i in range(n + 1)]

    def hole_r(h0, h1):
        return min(D((h0 + h1) / 2) / 2 - TF((h0 + h1) / 2), (h1 - h0) / 2) * 0.97

    pieces = []
    # o pé maciço até o 1º furo
    h0, h1 = holes[0]
    rr = hole_r(h0, h1)
    zt = h0 + rr
    pieces.append([(-ch(Z_BOT), Z_BOT), (ch(Z_BOT), Z_BOT), (ch(zt), zt)] + arc(zt, rr, 0, -180) + [(-ch(zt), zt)])
    for (a0, a1), (b0, b1) in zip(holes[:-1], holes[1:]):
        ra, rb = hole_r(a0, a1), hole_r(b0, b1)
        zb, zt = a1 - ra, b0 + rb
        pieces.append([(-ch(zb), zb)] + arc(zb, ra, 180, 0) + [(ch(zb), zb), (ch(zt), zt)] + arc(zt, rb, 0, -180) + [(-ch(zt), zt)])
    a0, a1 = holes[-1]
    ra = hole_r(a0, a1)
    zb = a1 - ra
    zt = TOP - 0.012
    pieces.append([(-ch(zb), zb)] + arc(zb, ra, 180, 0) + [(ch(zb), zb), (ch(zt), zt), (-ch(zt), zt)])
    for pts in pieces:
        zm = sum(v for _, v in pts) / len(pts)
        t = TW(zm)
        vp.prism(p, 'Concrete#web', pts, (0.0, -t / 2, 0.0), Vector((1, 0, 0)), Vector((0, 0, 1)), Vector((0, 1, 0)), t)
    return holes


def rebar(p):
    """O ferro da armadura que aparece na lasca do pé: a barra do canto e 2 estribos (o resto fica dentro do concreto)."""
    xs = lambda z: D(z) / 2 - 0.022           # o cobrimento de ~2 cm (a lasca de 8 cm na face leva ele embora)
    ys = lambda z: W(z) / 2 - 0.024
    vp.tube(p, 'Rust#rebar', [(xs(z), ys(z), z) for z in (-0.12, 0.3, 0.95)], 0.0065, seg=6)
    for z in (0.34, 0.58):
        x, y = xs(z), ys(z)
        vp.tube(p, 'Rust#rebar', vp.bent_bar([(x, y - 0.07, z), (x, y, z), (x - 0.04, y, z)], R=0.012, n=2), 0.0042, seg=5)
    # a ponta torta de uma barra solta (o concreto que caiu levou o cobrimento)
    vp.tube(p, 'Rust#rebar', vp.bent_bar([(xs(0.5) - 0.01, ys(0.5) - 0.045, 0.3), (xs(0.5) + 0.004, ys(0.5) - 0.045, 0.52),
                                          (xs(0.5) + 0.03, ys(0.5) - 0.03, 0.6)], R=0.03, n=2), 0.005, seg=5)


def crossarm_steel(p):
    """A cruzeta de aço: 2 cantoneiras apertando o poste (a aba de cima pra fora), o parafuso passante, as chapas dos pinos, a
    braçadeira das mãos-francesas e os isoladores."""
    X0, X1 = -1.15, 1.15
    for sy in (-1, 1):
        heel = lambda x: Vector((x, sy * ANG_Y, ANG_TOP))
        e1, e2 = Vector((0, sy, 0)), Vector((0, 0, -1))
        prof = [(0, 0), (ANG_W, 0), (ANG_W, ANG_T), (ANG_T, ANG_T), (ANG_T, ANG_W), (0, ANG_W)]
        rings = []
        for x in (X0, -0.6, 0.0, 0.6, X1):
            H = heel(x)
            rings.append([tuple(H + e1 * u + e2 * v) for (u, v) in prof])
        p.loft('Metal#arm', rings, bevel=0.0, smooth=False, caps=(True, True))
    # o parafuso passante (pela alma maciça do alto) e as porcas
    zb = ANG_TOP - 0.035
    vp.tube(p, 'Metal#bolt', [(0.0, -ANG_Y - 0.05, zb), (0.0, ANG_Y + 0.05, zb)], 0.0095, seg=6)
    for sy in (-1, 1):
        vp.nut(p, 'Metal#bolt', (0.0, sy * (ANG_Y + ANG_T), zb), (0, sy, 0), r=0.02, h=0.015)
    # as chapas dos pinos (por cima das 2 cantoneiras) e os isoladores
    for x in INS_X:
        vp.rbox_q(p, 'Metal#plate', (x, 0.0, ANG_TOP + 0.004), (0.08, 2 * (ANG_Y + ANG_W) + 0.01, 0.008), r=0.012, k=1)
        for sy in (-1, 1):
            vp.nut(p, 'Metal#plate', (x, sy * (ANG_Y + ANG_W - 0.012), ANG_TOP + 0.008), (0, 0, 1), r=0.012, h=0.008)
        insulator(p, x, ANG_TOP + 0.008, nut_z=ANG_TOP - 0.003)
    # a braçadeira (o ferro chato atravessado nas 2 faces das abas) e as 4 mãos-francesas
    zc = 6.85
    for sx in (-1, 1):
        x = sx * (D(zc) / 2 + 0.004)
        vp.rbox_q(p, 'Metal#brace', (x, 0.0, zc), (0.008, W(zc) + 0.05, 0.05), r=0.003, k=1)
        for sy in (-1, 1):
            vp.nut(p, 'Metal#brace', (x, sy * (W(zc) / 2 + 0.012), zc), (sx, 0, 0), r=0.016, h=0.012)
            flat_bar(p, 'Metal#brace', (x + sx * 0.004, sy * (W(zc) / 2 + 0.012), zc),
                     (sx * 0.62, sy * (ANG_Y + 0.004), ANG_TOP - ANG_W), w=0.034, t=0.006, thick_dir=(0.0, 1.0, 0.0))
            vp.nut(p, 'Metal#brace', (sx * 0.62, sy * (ANG_Y + ANG_T + 0.003), ANG_TOP - ANG_W + 0.01), (0, sy, 0), r=0.016, h=0.012)


def flat_paper(p, m, x_face, sx, y0, y1, zb_fn, zt, nu=6, off=0.004):
    """O cartaz rasgado colado na face da aba (a face x = x_face, virada pra sx): a placa fina com o pé rasgado."""
    # (u = y, v = z) no plano da face: o canto de cima, o pé rasgado de y0 a y1, o outro canto de cima; a espessura pra fora
    pts = [(y0, zt)] + [(y0 + (y1 - y0) * (i / nu), zb_fn(i / nu)) for i in range(nu + 1)] + [(y1, zt)]
    vp.prism(p, m, pts, (x_face + sx * off, 0.0, 0.0), Vector((0, 1, 0)), Vector((0, 0, 1)), Vector((sx, 0, 0)), 0.002)


def box_and_conduit(p):
    """A caixa de luz de chapa na face −X (1,52–1,92 m), presa por 2 braçadeiras; o eletroduto descendo pro chão e subindo até a
    cruzeta; os cartazes na face +X."""
    zc, hb = 1.72, 0.40
    xf = -D(zc) / 2
    bw, bd = 0.24, 0.15
    C = Vector((xf - 0.012 - bd / 2, 0.0, zc))
    vp.rbox_q(p, 'Metal#box', tuple(C), (bd, bw, hb), r=0.012, k=2)
    # a porta (a moldura saliente), a dobradiça, o trinco, a aba de chuva, a ferrugem que escorre do pé
    vp.rbox_q(p, 'Metal#box', (C.x - bd / 2 - 0.004, 0.0, zc - 0.01), (0.01, bw - 0.03, hb - 0.05), r=0.01, k=1)
    vp.tube(p, 'Metal#box', [(C.x - bd / 2 - 0.006, -bw / 2 + 0.012, zc - 0.17), (C.x - bd / 2 - 0.006, -bw / 2 + 0.012, zc + 0.15)], 0.007, seg=6)
    vp.rbox_q(p, 'Dark#latch', (C.x - bd / 2 - 0.012, bw / 2 - 0.035, zc), (0.016, 0.022, 0.05), r=0.005, k=1)
    hood = Vector((C.x - 0.01, 0.0, zc + hb / 2 + 0.012))
    vp.rbox_q(p, 'Metal#box', tuple(hood), (bd + 0.05, bw + 0.03, 0.01), pitch=-12.0, r=0.006, k=1)
    vp.rbox_q(p, 'Rust#stain', (C.x - bd / 2 - 0.0095, -0.03, zc - hb / 2 + 0.05), (0.003, 0.08, 0.07), r=0.02, k=2)
    vp.rbox_q(p, 'Rust#stain', (C.x, 0.0, zc - hb / 2 - 0.002), (bd - 0.02, bw - 0.03, 0.006), r=0.02, k=2)
    for z in (zc - 0.12, zc + 0.12):
        vp.rbox_q(p, 'Metal#strap', (xf - 0.006, 0.0, z), (0.012, W(z) + 0.04, 0.03), r=0.004, k=1)
    # o eletroduto (sai do pé da caixa pro chão; do alto da caixa sobe pela face até a cruzeta)
    cy = -0.06
    lo = [(xf - 0.028, cy, zc - hb / 2), (-D(1.0) / 2 - 0.028, cy, 1.0), (-D(0.0) / 2 - 0.027, cy, 0.0), (-D(0.0) / 2 - 0.027, cy, -0.1)]
    vp.tube(p, 'Dark#conduit', lo, 0.022, seg=8)
    zt = ANG_TOP - 0.25
    up = [(xf - 0.035, cy, zc + hb / 2)] + [(-D(z) / 2 - 0.035, cy, z) for z in (3.0, 4.5, 6.0, zt - 0.2)]
    up = vp.bent_bar(up + [(-D(zt) / 2 - 0.035, cy, zt), (-0.4, -ANG_Y - 0.02, ANG_TOP - 0.065)], R=0.12, n=3)
    vp.tube(p, 'Dark#conduit', up, 0.016, seg=8)
    for z in (2.6, 4.0, 5.4):
        vp.rbox_q(p, 'Metal#strap', (-D(z) / 2 - 0.02, cy, z), (0.03, 0.07, 0.025), r=0.006, k=1)
    # os cartazes rasgados na face +X (um velho em cima de outro)
    xr = D(1.7) / 2
    flat_paper(p, 'Cloth#paper', xr, 1, -0.085, 0.07, lambda u: 1.4 + 0.06 * cp.h3(int(u * 6), 3, 1) + 0.03 * math.sin(u * 7.0), 1.95)
    flat_paper(p, 'Plaster#paper', xr + 0.0025, 1, -0.06, 0.08, lambda u: 1.62 + 0.05 * cp.h3(int(u * 6), 7, 2), 1.86, off=0.005)


def lamp(p):
    """A luminária de rua pra +X: as 2 placas de fixação na aba, o braço curvo de tubo com a escora, a cabeça cobra, o refrator e a
    fotocélula."""
    zf = 6.2
    xf = D(zf) / 2
    for z in (zf, zf - 0.32):
        vp.rbox_q(p, 'Metal#lamp', (D(z) / 2 + 0.008, 0.0, z), (0.016, W(z) + 0.03, 0.1), r=0.006, k=1)
        for sy in (-1, 1):
            vp.nut(p, 'Metal#lamp', (D(z) / 2 + 0.016, sy * (W(z) / 2 - 0.01), z), (1, 0, 0), r=0.016, h=0.012)
    arm = vp.bent_bar([(xf + 0.01, 0.0, zf), (0.45, 0.0, zf + 0.1), (1.15, 0.0, zf + 0.5), (1.72, 0.0, zf + 0.58)], R=0.35, n=6)
    vp.tube(p, 'Metal#lamp', arm, 0.03, seg=10)
    vp.tube(p, 'Metal#lamp', [(D(zf - 0.32) / 2 + 0.012, 0.0, zf - 0.32), (0.75, 0.0, zf + 0.26)], 0.018, seg=8)
    # a cabeça (o loft de seções: a ponta do braço entra nela, a barriga arredondada, o nariz)
    z0 = zf + 0.5
    rings = []
    for x, wy, ht, hb in ((1.66, 0.045, 0.05, 0.04), (1.72, 0.09, 0.085, 0.05), (1.82, 0.14, 0.11, 0.055), (1.98, 0.165, 0.12, 0.058),
                          (2.12, 0.16, 0.11, 0.055), (2.22, 0.13, 0.085, 0.05), (2.27, 0.08, 0.055, 0.04), (2.285, 0.03, 0.03, 0.02)):
        ring = []
        for j in range(16):
            a = 2 * math.pi * j / 16
            c, s = math.cos(a), math.sin(a)
            z = z0 + (ht * s if s > 0 else hb * s * 0.35)
            ring.append((x + 0.012 * s, wy * c, z))
        rings.append(ring)
    p.loft('Metal#lamp', rings, bevel=0.0, smooth=True, caps=(True, True))
    # o refrator de vidro embaixo e a fotocélula em cima
    vp.rbox_q(p, 'Lens#lamp', (2.0, 0.0, z0 - 0.035), (0.4, 0.24, 0.045), r=0.09, k=3)
    vp.lathe(p, 'Dark#cell', (1.86, 0.0, z0 + 0.105), 'z', [(0.0, 0.0), (0.03, 0.0), (0.03, 0.035), (0.022, 0.045), (0.0, 0.048)], seg=10)


def collar(p):
    """O bloco de concreto do pé (a fundação que aparece 3 cm acima do chão)."""
    vp.rbox_q(p, 'Concrete#collar', (0.0, 0.0, -0.035), (0.40, 0.36, 0.13), r=0.035, k=2)


def pole_concrete(with_lamp=False):
    p = Part('SM_civ_pole_concrete_lamp' if with_lamp else 'SM_civ_pole_concrete')
    flanges(p)
    web(p)
    rebar(p)
    collar(p)
    crossarm_steel(p)
    box_and_conduit(p)
    if with_lamp:
        lamp(p)
    return fix_orientation(p.finish())


if __name__ == '__main__':
    OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'civ_v3', 'pole')
    os.makedirs(OUT, exist_ok=True)
    clear_scene()
    a = pole_concrete(False)
    b = pole_concrete(True)
    bake_in_place(b, (5.0, 0.0, 0.0))
    print('CIV', a.name, cp.tris(a), b.name, cp.tris(b))
    export(a, OUT)
    export(b, OUT)
