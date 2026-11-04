"""A TORRE DE ALTA TENSÃO de treliça (kit civil v3, 03/11/2026; docs/elenco/TAREFAS.md item 64), pro campo entre as vilas.

Rodar (Blender sem janela, só CPU), com o detalhe AAA e a máscara em todos os materiais, NA CLASSE building:
  RTS_VC_FIX=1 "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_civ_pylon.py C:/Users/luisf/explosion/captures/game/models/civ_v3/pylon
(03/11, medido) Na classe vehicle a grade de 14 cm picota cada cantoneira comprida (o X do corpo tem 4–7 m): 33,6 mil → 142 mil
triângulos. Na building (40 cm): 64 mil, a forma idêntica (a forma está no perfil em L; a grade só dá resolução à máscara, e 40 cm
numa torre de 26 m sobra). O chanfro mínimo da classe não muda nada aqui (nenhuma peça usa o chanfro do modificador).

A palavra: A TORRE DE AÇO EM "ÁRVORE DE NATAL" com os 3 pares de braços e as cadeias de isoladores penduradas. É a torre de circuito
duplo de 132 kV que corta o campo no mundo inteiro (e no Iraque, na Síria, no Egito): o corpo de treliça que afina dos 4 pés até a
CINTURA (14 m), a gaiola quase reta em cima com os 3 braços de cada lado (o do meio mais comprido, pra os fios não se baterem no vento),
as cadeias de 9 discos de vidro penduradas na ponta de cada braço, o pico do cabo-guarda no alto. Tudo de cantoneira (o perfil em L:
as pernas com o canto pra fora, o contraventamento em X no corpo e em zigue-zague na gaiola e nos braços, as chapas de ligação nos nós,
os diafragmas em X na cintura e nos braços, que leem de cima), as 4 sapatas de concreto com o toco enferrujado, o dispositivo
antiescalada de arame farpado a ~3,7 m, a placa amarela de perigo e a de número.
Vazada de propósito (jogabilidade): a câmera do RTS vê as unidades através dela; a sombra no chão é a treliça.

Medidas (Unreal: X frente, Y direita, Z cima, metros; origem no chão, no centro da torre):
- altura: a gaiola até 23,6 m, o pico 26,0 m, o grampo do cabo-guarda a 26,3 m (o ponto mais alto ~26,35 m);
- a base: os pés em (±3,0; ±3,0), as sapatas de 0,8 × 0,8 m: a pegada 6,8 × 6,8 m; a cintura 2,0 × 2,0 m a 14 m;
- os braços ao longo de X (como a cruzeta do poste): os fios correm ao longo de Y. Braço de baixo a 15,2 m (ponta x = ±3,9), do meio a
  18,8 m (±4,4), de cima a 22,4 m (±3,6). A envergadura: 8,8 m em X.
- OS PONTOS DE LIGAÇÃO DOS FIOS (o grampo no pé de cada cadeia, onde o RTSWireSystem liga o fio): (±3,9; 0; 13,56), (±4,4; 0; 17,16),
  (±3,6; 0; 20,76) e o cabo-guarda (0; 0; 26,3). Gravados também em pylon_sockets.json, na pasta de saída.
Peça: SM_civ_pylon (uma malha, origem no chão no centro).
Materiais (os slots do kit civil): Metal (o aço galvanizado), Rust (o toco das pernas na sapata), Concrete (as sapatas), Lens (o vidro
dos discos), Dark (o arame farpado), Yellow e Paint (as placas).
"""
import json
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import Part, clear_scene, export  # noqa: E402
import model_civ_parts as cp  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402

BASE = 3.0                  # a meia-largura dos pés no chão
ZW, HW = 14.0, 1.0          # a cintura
ZC, HC = 23.6, 0.75         # o alto da gaiola
ZP, HP = 26.0, 0.15         # o pico
EARTH_Z = 26.3
ARMS = [(15.2, 3.9), (18.8, 4.4), (22.4, 3.6)]     # (z da corda de baixo, a ponta em x)
ARM_H = 1.2                 # a altura do braço na raiz
TIP_HW, TIP_H = 0.12, 0.25  # a ponta do braço (meia-largura, altura)
N_DISC, DISC_PITCH = 9, 0.146
HANG = 1.64                 # do pé do braço até o ponto do fio (o grampo)
BODY_Z = [0.0, 3.2, 6.0, 8.5, 10.6, 12.4, ZW]
CAGE_Z = [ZW, 15.2, 16.4, 17.6, 18.8, 20.0, 21.2, 22.4, ZC]
PEAK_Z = [ZC, 24.8, ZP]


# ===================================================================================================== a forma
def h(z):
    """A meia-largura da torre (do eixo à quina de fora da perna) na altura z."""
    if z <= ZW:
        return BASE + (HW - BASE) * z / ZW
    if z <= ZC:
        return HW + (HC - HW) * (z - ZW) / (ZC - ZW)
    return HC + (HP - HC) * (z - ZC) / (ZP - ZC)


def dh(z):
    if z < ZW:
        return (HW - BASE) / ZW
    if z < ZC:
        return (HC - HW) / (ZC - ZW)
    return (HP - HC) / (ZP - ZC)


FACES = [('x', 1), ('x', -1), ('y', 1), ('y', -1)]


def face_pt(f, u, z, inset=0.0):
    ax, s = f
    hh = h(z) - inset
    return Vector((s * hh, u * hh, z)) if ax == 'x' else Vector((u * hh, s * hh, z))


def face_n(f, z):
    ax, s = f
    v = Vector((s, 0.0, -dh(z))) if ax == 'x' else Vector((0.0, s, -dh(z)))
    return v.normalized()


def angle(p, m, a, b, e1, e2, w, t, ext=0.0):
    """A cantoneira (o L) de a até b: o canto (o 'calcanhar') na linha a–b, as 2 abas ao longo de e1 e e2 (largura w, espessura t)."""
    a, b = Vector(a), Vector(b)
    d = (b - a).normalized()
    a, b = a - d * ext, b + d * ext
    e1 = Vector(e1) - d * Vector(e1).dot(d)
    e1.normalize()
    e2 = Vector(e2) - d * Vector(e2).dot(d) - e1 * Vector(e2).dot(e1)
    e2.normalize()
    prof = [(0, 0), (w, 0), (w, t), (t, t), (t, w), (0, w)]
    rings = [[tuple(P + e1 * u + e2 * v) for (u, v) in prof] for P in (a, b)]
    p.loft(m, rings, bevel=0.0, smooth=False, caps=(True, True))


def brace(p, f, a_uz, b_uz, w=0.075, t=0.008, m='Metal#brace', inset=0.016):
    """O contraventamento na face f, de (u, z) a (u, z): a aba chata no plano da face (um pouco pra dentro da perna), a outra pra
    dentro da torre, centrada na linha."""
    a = face_pt(f, a_uz[0], a_uz[1], inset)
    b = face_pt(f, b_uz[0], b_uz[1], inset)
    d = (b - a).normalized()
    N = face_n(f, (a.z + b.z) / 2)
    e1 = N.cross(d).normalized()
    a, b = a - e1 * (w / 2), b - e1 * (w / 2)
    angle(p, m, a, b, e1, -N, w, t, ext=0.02)


def leg_w(z):
    return 0.15 if z < ZW else (0.11 if z < ZC else 0.08)


def u_end(z, inset=0.07):
    """O u (−1..1) onde o contraventamento encosta na perna (a uns 7 cm da quina, na aba dela)."""
    return 1.0 - inset / max(h(z), 0.2)


# ===================================================================================================== a treliça
def legs(p):
    for sx in (-1, 1):
        for sy in (-1, 1):
            e1, e2 = Vector((-sx, 0, 0)), Vector((0, -sy, 0))
            corner = lambda z: Vector((sx * h(z), sy * h(z), z))
            # o toco enferrujado na sapata e a perna em 3 trechos retos (o corpo, a gaiola, o pico)
            angle(p, 'Rust#stub', corner(0.0) + Vector((0, 0, -0.1)), corner(0.72), e1, e2, 0.158, 0.016)
            for z0, z1, w, t in ((0.7, ZW, 0.15, 0.014), (ZW, ZC, 0.11, 0.011), (ZC, ZP, 0.08, 0.008)):
                angle(p, 'Metal#leg', corner(z0), corner(z1), e1, e2, w, t, ext=0.02)


def body(p):
    """O corpo: o X em cada painel, as horizontais nos nós, a horizontal de alívio nos 2 painéis de baixo."""
    for f in FACES:
        for i, (z0, z1) in enumerate(zip(BODY_Z[:-1], BODY_Z[1:])):
            ua, ub = u_end(z0), u_end(z1)
            w = 0.09 if i < 2 else 0.075
            brace(p, f, (-ua, z0), (ub, z1), w=w)
            brace(p, f, (ua, z0), (-ub, z1), w=w)
            if i < 2:
                # o cruzamento do X (os painéis de baixo são compridos: a horizontal de alívio no meio)
                ha, hb = h(z0), h(z1)
                zx = z0 + (z1 - z0) * ha / (ha + hb)
                brace(p, f, (-u_end(zx), zx), (u_end(zx), zx), w=0.06, t=0.006)
        for z in (3.2, 6.0, 10.6, ZW):
            brace(p, f, (-u_end(z), z), (u_end(z), z), w=0.08)


def cage(p):
    """A gaiola: o zigue-zague (uma diagonal por painel, alternando), as horizontais em todos os nós."""
    for k, f in enumerate(FACES):
        for i, (z0, z1) in enumerate(zip(CAGE_Z[:-1], CAGE_Z[1:])):
            sgn = 1 if (i + k) % 2 == 0 else -1
            brace(p, f, (-sgn * u_end(z0), z0), (sgn * u_end(z1), z1), w=0.065, t=0.007)
        for z in CAGE_Z[1:]:
            brace(p, f, (-u_end(z), z), (u_end(z), z), w=0.065, t=0.007)
    # o pico: o X embaixo, a diagonal em cima, a horizontal no meio
    for k, f in enumerate(FACES):
        z0, z1, z2 = PEAK_Z
        brace(p, f, (-u_end(z0, 0.05), z0), (u_end(z1, 0.05), z1), w=0.055, t=0.006)
        brace(p, f, (u_end(z0, 0.05), z0), (-u_end(z1, 0.05), z1), w=0.055, t=0.006)
        brace(p, f, (-u_end(z1, 0.05), z1), (u_end(z1, 0.05), z1), w=0.055, t=0.006)
        sgn = 1 if k % 2 else -1
        brace(p, f, (-sgn * u_end(z1, 0.05), z1), (sgn * 0.3, z2 - 0.05), w=0.05, t=0.006)


def diaphragm(p, z, w=0.07):
    """O X em planta (a amarração horizontal da cintura e dos braços): lê de cima, na câmera do RTS."""
    hh = h(z) - 0.08
    for (ax, ay), (bx, by) in (((1, 1), (-1, -1)), ((1, -1), (-1, 1))):
        a, b = Vector((ax * hh, ay * hh, z)), Vector((bx * hh, by * hh, z))
        d = (b - a).normalized()
        e1 = Vector((0, 0, 1)).cross(d).normalized()
        angle(p, 'Metal#brace', a - e1 * (w / 2), b - e1 * (w / 2), e1, Vector((0, 0, -1)), w, 0.007, ext=0.02)


def gussets(p):
    """As chapas de ligação nos nós das pernas (o nó grosso que lê de perto), por fora do contraventamento."""
    for z in (3.2, 6.0, 8.5, 10.6, 12.4, ZW) + tuple(a for a, _ in ARMS):
        for f in FACES:
            N = face_n(f, z)
            for u in (-1, 1):
                q = face_pt(f, u * u_end(z, 0.17), z, 0.012)
                ex = (face_pt(f, 1, z) - face_pt(f, -1, z)).normalized()
                ez = N
                ey = ez.cross(ex)
                vp.rbox_q(p, 'Metal#plate', tuple(q), (0.24, 0.34 if z < ZW else 0.26, 0.01), r=0.03, k=1, frame=(ex, ey, ez))


def arm(p, za, L, s):
    """O braço do lado s (±X) com a corda de baixo a za e a ponta em x = s·L: as 2 cordas de baixo e as 2 de cima convergindo pra
    ponta, o zigue-zague dos lados, as travessas de baixo e de cima, a chapa da ponta."""
    zt = za + ARM_H
    for sy in (-1, 1):
        Rb = Vector((s * (h(za) - 0.02), sy * (h(za) - 0.02), za))
        Rt = Vector((s * (h(zt) - 0.02), sy * (h(zt) - 0.02), zt))
        Tb = Vector((s * L, sy * TIP_HW, za))
        Tt = Vector((s * L, sy * TIP_HW, za + TIP_H))
        angle(p, 'Metal#chord', Rb, Tb, Vector((0, -sy, 0)), Vector((0, 0, 1)), 0.09, 0.009, ext=0.02)
        angle(p, 'Metal#chord', Rt, Tt, Vector((0, -sy, 0)), Vector((0, 0, -1)), 0.08, 0.008, ext=0.02)
        n = 3
        B = [Rb + (Tb - Rb) * (i / n) for i in range(n + 1)]
        T = [Rt + (Tt - Rt) * (i / n) for i in range(n + 1)]
        nrm = (Tb - Rb).cross(Rt - Rb).normalized() * sy * s
        for i in range(n):
            a, b = B[i] + Vector((0, 0, 0.04)), T[i + 1] - Vector((0, 0, 0.04))
            d = (b - a).normalized()
            e1 = nrm.cross(d).normalized()
            angle(p, 'Metal#lace', a - e1 * 0.03, b - e1 * 0.03, e1, Vector((0, -sy, 0)), 0.06, 0.006, ext=0.02)
            if 0 < i:
                a, b = B[i] + Vector((0, 0, 0.04)), T[i] - Vector((0, 0, 0.04))
                d = (b - a).normalized()
                e1 = Vector((1, 0, 0)).cross(d).normalized() if abs(d.x) < 0.9 else Vector((0, 1, 0))
                angle(p, 'Metal#lace', a - e1 * 0.03, b - e1 * 0.03, e1, Vector((0, -sy, 0)), 0.06, 0.006, ext=0.02)
    # as travessas de baixo (de corda a corda) e de cima, e o zigue-zague de baixo
    Bm = lambda t, sy: Vector((s * ((h(za) - 0.02) + (L - h(za) + 0.02) * t), sy * ((h(za) - 0.02) + (TIP_HW - h(za) + 0.02) * t), za))
    Tm = lambda t, sy: Vector((s * ((h(zt) - 0.02) + (L - h(zt) + 0.02) * t), sy * ((h(zt) - 0.02) + (TIP_HW - h(zt) + 0.02) * t),
                               zt + (za + TIP_H - zt) * t))
    for t in (1 / 3, 2 / 3):
        for fn, up in ((Bm, 1), (Tm, -1)):
            a, b = fn(t, -1), fn(t, 1)
            angle(p, 'Metal#lace', a + Vector((s * 0.03, 0, 0)), b + Vector((s * 0.03, 0, 0)), Vector((-s, 0, 0)), Vector((0, 0, up)), 0.06,
                  0.006, ext=0.01)
    for i, (t0, t1) in enumerate(((0.0, 1 / 3), (1 / 3, 2 / 3), (2 / 3, 1.0))):
        sy = 1 if i % 2 == 0 else -1
        a, b = Bm(t0, -sy), Bm(t1, sy)
        d = (b - a).normalized()
        e1 = Vector((0, 0, 1)).cross(d).normalized()
        angle(p, 'Metal#lace', a - e1 * 0.03, b - e1 * 0.03, e1, Vector((0, 0, 1)), 0.06, 0.006, ext=0.01)
    # a chapa da ponta (fecha o braço e segura a cadeia)
    vp.rbox_q(p, 'Metal#plate', (s * (L + 0.012), 0.0, za + TIP_H / 2), (0.016, 2 * TIP_HW + 0.08, TIP_H + 0.06), r=0.025, k=1)
    vp.rbox_q(p, 'Metal#plate', (s * (L - 0.06), 0.0, za - 0.008), (0.16, 2 * TIP_HW + 0.06, 0.016), r=0.02, k=1)


def insulator_string(p, x, za):
    """A cadeia de suspensão pendurada no pé da ponta do braço: a manilha, o olhal, os 9 discos de vidro (a saia com as nervuras por
    baixo, a campânula de ferro, o pino), o encaixe de baixo e o grampo do fio. Devolve o ponto do fio."""
    # a manilha (o U) e o olhal
    vp.tube(p, 'Metal#hw', vp.bent_bar([(x - 0.035, 0.0, za - 0.012), (x - 0.035, 0.0, za - 0.09), (x + 0.035, 0.0, za - 0.09),
                                        (x + 0.035, 0.0, za - 0.012)], R=0.03, n=3), 0.009, seg=6)
    vp.tube(p, 'Metal#hw', [(x, 0.0, za - 0.08), (x, 0.0, za - 0.16)], 0.012, seg=8)
    glass = [(0.0, 0.028), (0.03, 0.03), (0.038, 0.046), (0.058, 0.036), (0.068, 0.052), (0.088, 0.04), (0.098, 0.056), (0.12, 0.044),
             (0.128, 0.052), (0.126, 0.062), (0.09, 0.078), (0.052, 0.092), (0.0, 0.096)]
    cap = [(0.0, 0.088), (0.05, 0.088), (0.052, 0.11), (0.044, 0.132), (0.0, 0.138)]
    for i in range(N_DISC):
        zb = za - 0.30 - i * DISC_PITCH
        vp.lathe(p, 'Lens#disc', (x, 0.0, zb), 'z', glass, seg=12, smooth=True, hard=40.0)
        vp.lathe(p, 'Metal#cap', (x, 0.0, zb), 'z', cap, seg=10, smooth=True, hard=40.0)
        vp.tube(p, 'Metal#cap', [(x, 0.0, zb - 0.012), (x, 0.0, zb + 0.032)], 0.009, seg=6)
    z_last = za - 0.30 - (N_DISC - 1) * DISC_PITCH
    vp.lathe(p, 'Metal#hw', (x, 0.0, z_last - 0.1), 'z', [(0.0, 0.0), (0.02, 0.0), (0.024, 0.03), (0.016, 0.05), (0.016, 0.09),
                                                          (0.0, 0.092)], seg=8)
    # o grampo de suspensão (a calha do fio ao longo de Y) e as 2 orelhas
    zc = za - HANG
    vp.rbox_q(p, 'Metal#hw', (x, 0.0, zc - 0.012), (0.07, 0.3, 0.05), r=0.022, k=2)
    for sx in (-1, 1):
        vp.rbox_q(p, 'Metal#hw', (x + sx * 0.03, 0.0, zc + 0.04), (0.012, 0.05, 0.1), r=0.012, k=1)
    return (round(x, 3), 0.0, round(zc, 3))


def footings(p):
    """As 4 sapatas: o bloco de concreto com o topo em pirâmide (escorre a água), as quinas gastas."""
    for sx in (-1, 1):
        for sy in (-1, 1):
            c = Vector((sx * BASE, sy * BASE, 0.0))
            vp.rbox_q(p, 'Concrete#foot', tuple(c + Vector((0, 0, 0.0))), (0.8, 0.8, 0.7), r=0.05, k=2)
            rings = []
            for z, hw_ in ((0.34, 0.39), (0.36, 0.385), (0.45, 0.24)):
                rings.append([tuple(c + Vector((u * hw_, v * hw_, z))) for u, v in vp.rounded_rect(2.0, 2.0, 0.12, k=1)])
            p.loft('Concrete#cap', rings, bevel=0.0, smooth=False, caps=(True, True))


def anti_climb(p):
    """O dispositivo antiescalada (~3,5–4,0 m): os suportes de cantoneira saindo das pernas pra fora e pra cima, os 4 fios de arame
    farpado dando a volta na torre."""
    z0 = 3.55
    strands = []
    for k in range(4):
        strands.append([])
    corners = [(1, 1), (-1, 1), (-1, -1), (1, -1)]
    for (sx, sy) in corners:
        base = Vector((sx * h(z0), sy * h(z0), z0))
        out = Vector((sx, sy, 0)).normalized()
        tip = base + out * 0.42 + Vector((0, 0, 0.42))
        d = (tip - base).normalized()
        e1 = Vector((0, 0, 1)).cross(out).normalized()
        angle(p, 'Metal#lace', base - e1 * 0.025, tip - e1 * 0.025, e1, Vector((0, 0, 1)), 0.05, 0.005)
        for k in range(4):
            strands[k].append(base + (tip - base) * ((k + 1) / 4.2))
    for k in range(4):
        pts = []
        loop = strands[k] + [strands[k][0]]
        for a, b in zip(loop[:-1], loop[1:]):
            for i in range(10):
                t = i / 10
                q = a + (b - a) * t
                q.z -= 0.05 * 4 * t * (1 - t)
                pts.append(q)
        pts.append(loop[-1])
        vp.tube(p, 'Dark#barb', pts, 0.005, seg=4)
        # as farpas (os nós a cada ~40 cm, só o ritmo que lê)
        for q in pts[1::2]:
            vp.tube(p, 'Dark#barb', [q + Vector((0.0, 0.0, -0.018)), q + Vector((0.0, 0.0, 0.018))], 0.004, seg=4, caps=False)


def plates(p):
    """A placa amarela de perigo (o raio preto) e a de número, na horizontal de 3,2 m das faces ±X."""
    for s in (1, -1):
        f = ('x', s)
        N = face_n(f, 3.0)
        q = face_pt(f, 0.0, 2.92, -0.02)
        ey = Vector((0, 0, 1)) - N * N.z
        ey.normalize()
        ex = ey.cross(N) * s
        vp.rbox_q(p, 'Yellow#sign', tuple(q), (0.36, 0.28, 0.006), r=0.02, k=1, frame=(ex, ey, N))
        bolt = [(-0.02, 0.1), (0.05, 0.1), (0.01, 0.01), (0.05, 0.01), (-0.04, -0.11), (-0.005, -0.02), (-0.045, -0.02)]
        vp.prism(p, 'Dark#sign', [(u * 1.0, v * 1.0) for u, v in bolt], tuple(q + N * 0.003), ex, ey, N, 0.002)
        q2 = face_pt(f, 0.0, 2.6, -0.02)
        vp.rbox_q(p, 'Paint#sign', tuple(q2), (0.3, 0.14, 0.006), r=0.015, k=1, frame=(ex, ey, N))
        for du in (-0.08, 0.0, 0.08):
            vp.rbox_q(p, 'Dark#sign', tuple(q2 + ex * du + N * 0.004), (0.045, 0.08, 0.002), r=0.008, k=1, frame=(ex, ey, N))
        # os 2 ferros que seguram as placas na horizontal de 3,2 m
        for du in (-0.13, 0.13):
            a = face_pt(f, 0.0, 3.2, 0.0) + ex * du
            b = q2 + ex * du - ey * 0.05
            vp.tube(p, 'Metal#hw', [tuple(a), tuple(b)], 0.008, seg=5)


def peak(p):
    """O alto: a chapa do pico e o grampo do cabo-guarda."""
    vp.rbox_q(p, 'Metal#plate', (0.0, 0.0, ZP), (0.42, 0.42, 0.018), r=0.04, k=1)
    for sy in (-1, 1):
        vp.rbox_q(p, 'Metal#plate', (0.0, sy * 0.03, ZP + 0.14), (0.16, 0.012, 0.28), r=0.03, k=1)
    vp.rbox_q(p, 'Metal#hw', (0.0, 0.0, EARTH_Z - 0.02), (0.06, 0.26, 0.05), r=0.02, k=2)
    vp.tube(p, 'Metal#hw', [(0.0, -0.05, EARTH_Z + 0.02), (0.0, 0.05, EARTH_Z + 0.02)], 0.012, seg=6)
    return (0.0, 0.0, EARTH_Z)


def pylon():
    p = Part('SM_civ_pylon')
    footings(p)
    legs(p)
    body(p)
    cage(p)
    diaphragm(p, ZW)
    for za, _ in ARMS:
        diaphragm(p, za, w=0.06)
    gussets(p)
    sockets = []
    for za, L in ARMS:
        for s in (-1, 1):
            arm(p, za, L, s)
            sockets.append(insulator_string(p, s * L, za))
    earth = peak(p)
    anti_climb(p)
    plates(p)
    return p.finish(), sockets, earth


if __name__ == '__main__':
    OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'civ_v3', 'pylon')
    os.makedirs(OUT, exist_ok=True)
    clear_scene()
    o, sockets, earth = pylon()
    print('CIV', o.name, cp.tris(o))
    with open(os.path.join(OUT, 'pylon_sockets.json'), 'w', encoding='utf-8') as f:
        json.dump({'SM_civ_pylon': {
            'about': 'Unreal meters, mesh space (origin on the ground at the tower center). Arms along local X, wires run along local Y '
                     '(same convention as SM_civ_pole). phases = the 6 conductor clamps (bottom of each insulator string), '
                     'pairs (-X, +X) from the bottom arm up; earth = the earth-wire clamp on the peak.',
            'height_m': round(EARTH_Z + 0.05, 2), 'footprint_m': [6.8, 6.8], 'span_x_m': 8.8,
            'phases': [list(s) for s in sockets], 'earth': list(earth)}}, f, indent=1)
    export(o, OUT)
