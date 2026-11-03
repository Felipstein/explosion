"""O Stinger Site da GLA (a defesa antiaérea padrão), v5 (03/11/2026): O ALÇAPÃO. Fiel ao conceito aprovado
docs/concept/buildings/gla_stinger_site_alcapao_APROVADO.jpg. O v1 a v4 ficam como estão.

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_gla_stinger_site_v5.py captures/game/models/gla_stinger_site_v5

A palavra (o que lê em 1 segundo): A TAMPA REDONDA ABERTA EM DUAS COM O ELEVADOR SUBINDO. No chão, o anel de sacos de areia (alguns com a
faixa verde a pincel) em volta do alçapão: o anel de faixa de perigo pintado no chão, o anel grosso de chapa rebitada e, aberta em duas
como uma concha, a tampa redonda de chapa rebitada (as 2 folhas em pé, cada uma com o meio círculo verde por fora: fechadas, o círculo
verde inteiro). Do poço escuro sobe o elevador: o pistão, a plataforma com a grade e o rodapé de perigo, a equipe de 2 e o lançador duplo
no ombro do atirador. Atrás, à direita (de quem olha do jogo), o mastro fino de treliça com a bola do sensor e a bandeira, e a lâmpada
vermelha no pau ao lado; na borda, os caixotes compridos de míssil.

Lote 8 × 8 m, frente em +X. Alturas: o anel de chapa 0,42 m, as folhas abertas até ~2,2 m, a plataforma 1,25 m (a grade até 2,25 m),
o lançador ~2,9 m, o mastro até ~4,9 m (a bandeira ~5,8 m).

Peças (origem no chão, no centro do lote; Unreal X frente, Y direita, Z cima, metros):
  SM_GLA_StingerSite_Base: o chão batido, os sacos, o anel de perigo, o anel de chapa, o poço, o mastro, a lâmpada, os caixotes (a
    bandeira sai no flags.json);
  SM_GLA_StingerSite_Turret (gira em volta de (0, 0), a mesma origem do modelo): o pistão, a plataforma, a grade, a equipe e o lançador;
  SM_GLA_StingerSite_HatchL / _HatchR (a origem na dobradiça: HINGE_L / HINGE_R; o eixo HAXIS, a linha das
    dobradiças girada HYAW = −20° pra acompanhar a câmera do jogo): as 2 folhas, modeladas
    ABERTAS (OPEN graus); fecham girando OPEN graus em volta do eixo (a borda solta desce pro centro);
  SM_GLA_StingerSite_Scaffold: o andaime da obra em madeira.
Materiais (slots da GLA e genéricos): Plaster (as pedras), Earth, Cloth (os sacos, a roupa), Player (o verde), Metal (a chapa), Dark,
  Trim, Hazard (o anel e o rodapé de perigo; o andaime), Crate (os tubos, os coletes, os caixotes), Glass (as lentes do sensor),
  Beacon (a lâmpada que pisca: não há slot vermelho aceso; o Beacon é o laranja que pisca), Rust.
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, V, clear_scene, export, flag, scaffold  # noqa: E402,F401
from model_faction_kit import disc_ring, revolve, barrel  # noqa: E402,F401
from model_building_parts import tube, fillet, bake_in_place  # noqa: E402
from model_gla_palace_v2 import (new_part, finish, _hash, fbm, rod, bag, earth_pad, blob, crate, Frame, rope)  # noqa: E402,F401

COLORS.update({'Glass': (30, 34, 38), 'Beacon': (255, 70, 40)})
PREFIX = 'SM_GLA_StingerSite'
# o alçapão
RH = 1.42            # o raio do furo (a boca do poço)
RS = 1.86            # o raio de fora do anel de chapa
ZR = 0.42            # o topo do anel de chapa
RL, HL = 1.74, 0.62  # a tampa: o raio e a altura da concha (fechada)
ZH = ZR + 0.05       # a altura da linha das dobradiças
OPEN = 66.0          # o quanto as folhas estão abertas (graus): antes do prumo, o lado de fora (o meio círculo verde) olha pra cima
HYAW = -20.0         # a linha das dobradiças girada pra acompanhar a câmera do jogo (as 2 folhas mostram o lado de fora, como no conceito)
_c, _s = math.cos(math.radians(HYAW)), math.sin(math.radians(HYAW))
HAXIS = (round(_c, 4), round(_s, 4), 0.0)   # o eixo das dobradiças


def hrot(x, y):
    """Do quadro do alçapão (as dobradiças em ±Y, o eixo em X) pro mundo: o giro HYAW."""
    return (x * _c - y * _s, x * _s + y * _c)


HINGE_L = hrot(0.0, -RL) + (ZH,)
HINGE_R = hrot(0.0, RL) + (ZH,)
# o elevador
PZ = 1.25            # o piso da plataforma
PH = 0.72            # a meia-largura da plataforma
EL = 24.0            # a elevação do lançador em repouso
LT_L, LT_R = 1.5, 0.075
# o anel de sacos
RB = 3.02
MAST = (-1.85, -1.85)   # dentro do anel de sacos, atrás à direita (de quem olha do jogo), fora das folhas abertas
LAMP = (-1.2, -2.35)


# ===================================================================================== o chão e o anel
def ground(p):
    """O chão batido redondo (sobe um pouco no anel de sacos, como o monte do conceito), as pedras soltas em volta, o anel de perigo
    pintado no chão em volta do anel de chapa."""
    earth_pad(p, (0.0, 0.0), blob(0.0, 0.0, 3.95, 3.9, seed=4, amp=0.05), h=0.04)
    # o monte de terra sob os sacos (o anel baixo de perfil arredondado)
    rings = []
    seg = 64
    for (r, z) in ((RB - 0.75, 0.0), (RB - 0.45, 0.13), (RB, 0.2), (RB + 0.45, 0.13), (RB + 0.85, -0.03)):
        rings.append([((r + 0.06 * (fbm(math.cos(2 * math.pi * k / seg) * 2, math.sin(2 * math.pi * k / seg) * 2, 3) - 0.5)) * math.cos(2 * math.pi * k / seg),
                       (r + 0.06 * (fbm(math.cos(2 * math.pi * k / seg) * 2, math.sin(2 * math.pi * k / seg) * 2, 3) - 0.5)) * math.sin(2 * math.pi * k / seg), z)
                      for k in range(seg)])
    _ring_surface(p, 'Earth#mound', rings)
    # as pedras soltas (o entulho do buraco cavado) em volta do anel de sacos
    for k in range(46):
        a = 2 * math.pi * _hash(k, 1, 7)
        r = RB + 0.55 + 0.75 * _hash(k, 2, 7)
        if abs(math.degrees(a) % 360 - 300) < 22 and r > RB + 0.6:
            continue                                                  # onde estão os caixotes
        s = 0.09 + 0.17 * _hash(k, 3, 7) ** 2
        x, y = r * math.cos(a), r * math.sin(a)
        if max(abs(x), abs(y)) > 3.9:
            continue
        p.obox('Plaster#stone', (x, y, s * 0.3), (s * 1.3, s, s * 0.8), yaw=_hash(k, 4, 7) * 180, pitch=(_hash(k, 5, 7) - 0.5) * 30, bevel=0.03)
    # o anel de perigo pintado no chão (a faixa larga em volta do anel de chapa)
    disc_ring(p, 'Hazard', (0.0, 0.0), 2.42, RS + 0.05, 0.03, 0.075, seg=72)
    disc_ring(p, 'Dark#edge', (0.0, 0.0), 2.46, 2.42, 0.03, 0.07, seg=72)


def _ring_surface(p, m, rings):
    """Os anéis (fechados) ligados de dentro pra fora, só o tampo (o fundo fica no chão)."""
    b = p.bm(m, 0.0)
    vr = [[b.verts.new(V(*q)) for q in ring] for ring in rings]
    n = len(rings[0])
    for r0, r1 in zip(vr[:-1], vr[1:]):
        for i in range(n):
            j = (i + 1) % n
            f = b.faces.new([r0[i], r1[i], r1[j], r0[j]])
            f.smooth = True


def hatch_ring(p):
    """O anel grosso de chapa rebitada (a gola do poço), a borda de dentro descendo pro escuro, o degrau de dentro, a escada de
    marinheiro descendo, os rebites, as dobradiças fixas das folhas e o fundo escuro (o poço: o chão do mapa fica embaixo)."""
    # a gola: o perfil de chapa (o topo levemente abaulado, a saia de fora descendo pro chão, a parede de dentro)
    revolve(p, 'Metal#ring', (0.0, 0.0), [(RS + 0.06, 0.0), (RS + 0.04, 0.2), (RS, ZR - 0.06), (RS - 0.06, ZR), (RH + 0.08, ZR),
                                         (RH, ZR - 0.05), (RH, 0.02)], seg=72, smooth=True, caps=(False, False))
    # o aro de reforço de fora (o rebordo que pega a luz) e o de dentro (o batente onde a tampa assenta)
    p.torus('Metal#lip', (0.0, 0.0, ZR - 0.02), 'z', RS - 0.02, 0.045, seg=72, rseg=8)
    p.torus('Dark#lip', (0.0, 0.0, ZR - 0.01), 'z', RH + 0.04, 0.035, seg=64, rseg=6)
    # os rebites: 2 fileiras no topo (as cabeças de 8 cm: o que a câmera ainda lê)
    for row, r in enumerate((RS - 0.14, RH + 0.18)):
        n = 36 if row == 0 else 28
        for k in range(n):
            a = 2 * math.pi * (k + 0.5 * row) / n
            p.cyl('Metal#rivet', (r * math.cos(a), r * math.sin(a), ZR + 0.01), 'z', 0.04, 0.05, seg=8, r2=0.025, bevel=0.0)
    # as 8 chapas de emenda (as juntas soldadas da gola: o ritmo que o conceito mostra)
    for k in range(8):
        a = 2 * math.pi * (k + 0.25) / 8
        p.obox('Dark#seam', ((RS + 0.035) * math.cos(a), (RS + 0.035) * math.sin(a), 0.2), (0.025, 0.07, 0.36), yaw=math.degrees(a), bevel=0.0)
    # o poço: a parede de dentro em anéis (as cintas de chapa), o degrau, a escada e o fundo escuro
    for z in (0.3, 0.17):
        p.torus('Dark#rib', (0.0, 0.0, z), 'z', RH - 0.01, 0.025, seg=48, rseg=5)
    disc_ring(p, 'Metal#step', (0.0, 0.0), RH, RH - 0.22, 0.1, 0.14, seg=48)
    p.cyl('Dark#pit', (0.0, 0.0, 0.065), 'z', RH, 0.03, seg=48, bevel=0.0)
    for s in (-1, 1):
        rod(p, 'Trim#ladder', (-RH + 0.1, s * 0.22, 0.07), (-RH + 0.1, s * 0.22, ZR + 0.05), 0.022, seg=6)
    for z in (0.16, 0.29, 0.42):
        rod(p, 'Trim#ladder', (-RH + 0.1, -0.22, z), (-RH + 0.1, 0.22, z), 0.018, seg=5)
    # as dobradiças fixas (as orelhas soldadas na gola, onde as folhas giram)
    for s in (-1, 1):
        for x in (-0.95, 0.0, 0.95):
            p.obox('Metal#hinge', hrot(x, s * (RL - 0.02)) + (ZR + 0.02,), (0.22, 0.14, 0.12), yaw=HYAW, bevel=0.02)
            p.cyl('Dark#pin', hrot(x, s * RL) + (ZH,), HAXIS, 0.05, 0.28, seg=10, bevel=0.0)
    # o pistão sai do fundo: a sapata dele (fixa)
    p.cyl('Metal#base', (0.0, 0.0, 0.11), 'z', 0.42, 0.1, seg=24, bevel=0.0)
    p.cyl('Dark#base', (0.0, 0.0, 0.19), 'z', 0.3, 0.08, seg=20, bevel=0.0)


def banded_bag(p, c, size, yaw, band):
    """O saco de areia do kit e, se band, a faixa verde a pincel em volta do meio (o conceito: alguns sacos marcados de verde)."""
    bag(p, c, size, yaw=yaw)
    if not band:
        return
    L, W, H = size
    cy, sy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    rings = []
    for u in (-0.06, 0.08):
        x = L * u
        ring = []
        for k in range(10):
            th = 2 * math.pi * k / 10
            ct, st = math.cos(th), math.sin(th)
            yy = (W / 2 + 0.012) * math.copysign(abs(ct) ** 0.7, ct)
            zz = (H / 2 + 0.01) * math.copysign(abs(st) ** 0.7, st)
            if zz < 0:
                zz *= 0.6
            ring.append((c[0] + x * cy - yy * sy, c[1] + x * sy + yy * cy, c[2] + zz))
        rings.append(ring)
    p.loft('Player#bagband', rings, bevel=0.0, smooth=True)


def bags(p):
    """O anel de sacos (3 fileiras, os de baixo mais largos) em volta do alçapão, aberto atrás (−X) pra entrada; 1 em cada ~5 com a
    faixa verde."""
    L_, W_, H_ = 0.72, 0.42, 0.27
    n = int(2 * math.pi * RB / (L_ * 0.9))
    for lv in range(3):
        rr = RB - lv * 0.05
        for k in range(n):
            a = 2 * math.pi * (k + (0.5 if lv % 2 else 0.0)) / n
            deg = math.degrees(a) % 360
            if 166 <= deg <= 194:
                continue
            z = 0.2 + H_ * 0.42 + lv * H_ * 0.8
            band = _hash(k, lv, 41) > 0.78
            banded_bag(p, (rr * math.cos(a), rr * math.sin(a), z - 0.05), (L_, W_, H_), math.degrees(a) + 90 + (_hash(k, lv, 43) - 0.5) * 10, band)
    # as pontas da entrada (os sacos de pé, travando)
    for s in (-1, 1):
        a = math.radians(180 + s * 17)
        banded_bag(p, (RB * math.cos(a), RB * math.sin(a), 0.32), (L_, W_, H_), math.degrees(a), False)


# ===================================================================================== o mastro, a lâmpada, os caixotes
def mast(p):
    """O mastro fino de treliça (4 pernas e o X) com a bola do sensor (as 2 lentes) e a bandeira por cima; a lâmpada vermelha no pau ao
    lado, com o cabo até o mastro; a sapata de concreto."""
    x0, y0 = MAST
    top = 4.25
    w0, w1 = 0.2, 0.13
    p.box('Plaster#foot', x0 - 0.24, x0 + 0.24, y0 - 0.24, y0 + 0.24, -0.05, 0.2, bevel=0.06)
    lv = [0.22 + (top - 0.22) * k / 8 for k in range(9)]

    def corner(k, i):
        t = (lv[k] - 0.22) / (top - 0.22)
        w = w0 + (w1 - w0) * t
        sx, sy = ((-1, -1), (1, -1), (1, 1), (-1, 1))[i]
        return (x0 + sx * w, y0 + sy * w, lv[k])
    for i in range(4):
        rod(p, 'Trim#mast', corner(0, i), corner(8, i), 0.028, seg=6)
    for k in range(8):
        for i in range(4):
            a, b = corner(k, i), corner(k + 1, (i + 1) % 4)
            rod(p, 'Trim#lace', a, b, 0.012, seg=4)
            rod(p, 'Trim#lace', corner(k + 1, i), corner(k + 1, (i + 1) % 4), 0.012, seg=4)
    # o topo: a chapa, a bola do sensor (o corpo cinza, a faixa escura, as 2 lentes viradas pra frente), o mastro da bandeira
    p.box('Metal', x0 - 0.18, x0 + 0.18, y0 - 0.18, y0 + 0.18, top, top + 0.05, bevel=0.0)
    sc = (x0, y0, top + 0.34)
    revolve(p, 'Metal#ball', (sc[0], sc[1]), [(0.02, sc[2] - 0.29), (0.15, sc[2] - 0.25), (0.25, sc[2] - 0.15), (0.29, sc[2]),
                                               (0.25, sc[2] + 0.15), (0.15, sc[2] + 0.25), (0.02, sc[2] + 0.29)], seg=24)
    p.cyl('Metal#neck', (x0, y0, top + 0.05), 'z', 0.07, 0.12, seg=10, bevel=0.0)
    p.torus('Dark#band', sc, 'z', 0.29, 0.025, seg=24, rseg=5)
    for dy in (-0.09, 0.09):
        d = Vector((math.cos(math.radians(25)), math.sin(math.radians(25)), 0.0))
        q = Vector((sc[0], sc[1] + dy, sc[2] + 0.03)) + d * 0.25
        p.cyl('Dark#lens', tuple(q), tuple(d), 0.075, 0.1, seg=14, bevel=0.0)
        p.cyl('Glass', tuple(q + d * 0.052), tuple(d), 0.055, 0.01, seg=14, bevel=0.0)
    p.cyl('Trim', (x0, y0, sc[2] + 0.29 + 0.55), 'z', 0.025, 1.1, seg=6, bevel=0.0)
    flag(p, (x0, y0), sc[2] + 0.29 + 1.05, 1.05, 0.62, m='Player', phase=0.4)
    # a lâmpada vermelha: o pau, a caixa de ligação, a cúpula com a grade e a luz; o cabo até o mastro
    lx, ly = LAMP
    p.cyl('Trim#lamp', (lx, ly, 1.55), 'z', 0.035, 3.1, seg=8, bevel=0.0)
    p.box('Plaster#foot', lx - 0.17, lx + 0.17, ly - 0.17, ly + 0.17, -0.05, 0.14, bevel=0.04)
    p.box('Dark#jbox', lx + 0.035, lx + 0.13, ly - 0.08, ly + 0.08, 1.0, 1.24, bevel=0.0)
    p.cyl('Dark#lamp', (lx, ly, 3.14), 'z', 0.1, 0.08, seg=14, bevel=0.0)
    p.cyl('Beacon', (lx, ly, 3.29), 'z', 0.085, 0.22, seg=16, bevel=0.0, smooth=True)
    p.cyl('Dark#lamp', (lx, ly, 3.42), 'z', 0.095, 0.04, seg=14, bevel=0.0)
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        rod(p, 'Dark#grille', (lx + 0.095 * math.cos(a), ly + 0.095 * math.sin(a), 3.18), (lx + 0.095 * math.cos(a), ly + 0.095 * math.sin(a), 3.42), 0.008, seg=4)
    tube(p, 'Dark#cable', [(lx, ly, 2.9), ((lx + x0) / 2, (ly + y0) / 2, 2.45), (x0 + 0.1, y0 - 0.1, 2.6)], 0.016, seg=6)
    tube(p, 'Dark#cable', [(lx + 0.08, ly, 1.0), (lx + 0.2, ly + 0.1, 0.2), (lx + 0.5, ly + 0.5, 0.06), (RS * 0.75, -RS * 0.75, 0.06)], 0.018, seg=6)


def crates(p):
    """Os caixotes compridos de míssil na borda (de fora do anel de sacos, à direita de quem olha do jogo): 2 no chão e 2 por cima, um
    com a tampa aberta mostrando os tubos."""
    a = math.radians(-62)
    t = (math.cos(a + math.pi / 2), math.sin(a + math.pi / 2))
    yaw = math.degrees(a) + 90
    for k, (r, z, dt) in enumerate(((3.55, 0.0, 0.0), (3.98, 0.0, 0.1), (3.57, 0.37, -0.06), (3.96, 0.37, 0.15))):
        x, y = r * math.cos(a) + t[0] * dt, r * math.sin(a) + t[1] * dt
        crate(p, x, y, z + 0.02, size=(1.6, 0.42, 0.36), yaw=yaw + (_hash(k, 0, 51) - 0.5) * 6)
        p.obox('Player#stencil', (x, y, z + 0.02 + 0.365), (0.5, 0.22, 0.01), yaw=yaw, bevel=0.0)


def base():
    p = new_part(PREFIX + '_Base')
    for m in ('Earth#mound', 'Metal#ring', 'Dark#pit', 'Player#bagband'):
        p.bm(m, 0.0)
    ground(p)
    hatch_ring(p)
    bags(p)
    mast(p)
    crates(p)
    return finish(p)


# ===================================================================================== as folhas da tampa
def _shell_pt(r, a, d=0.0):
    """O ponto da concha fechada (o raio r, o ângulo a, d pra dentro da casca): o perfil de calota (a borda em ZR, o topo em ZR + HL)."""
    t = min(1.0, r / RL)
    z = ZR + 0.03 + HL * (1.0 - t * t) ** 0.62 - d
    rr = r - d * 0.4 * t
    return (rr * math.cos(a), rr * math.sin(a), z)


def hatch_leaf(side):
    """Uma folha da tampa (a metade da concha de chapa: −Y é a L, +Y a R), com a casca de 6 cm, o rebordo de chapa em toda a volta, os
    rebites, o meio círculo verde por fora, os 3 braços da dobradiça e o puxador por dentro. Modelada FECHADA no mundo e girada
    aberta em volta da dobradiça; sai com a origem na dobradiça."""
    name = PREFIX + ('_HatchL' if side < 0 else '_HatchR')
    hz = (0.0, side * RL, ZH)                       # a dobradiça no quadro do alçapão
    ang = math.radians(OPEN) * (1 if side < 0 else -1)
    ca, sa = math.cos(ang), math.sin(ang)

    def T(q):
        """O ponto da folha fechada (quadro do alçapão) → a folha aberta, relativo à dobradiça, girado pro mundo (HYAW)."""
        x, y, z = q
        ry, rz = y - hz[1], z - hz[2]
        return hrot(x, ry * ca - rz * sa) + (ry * sa + rz * ca,)
    p = new_part(name)
    for m in ('Metal#shell', 'Player#ring', 'Metal#rim'):
        p.bm(m, 0.0)
    a0, a1 = (math.pi, 2 * math.pi) if side < 0 else (0.0, math.pi)
    na, nr = 28, 10
    T_ = lambda r, a, d=0.0: T(_shell_pt(r, a, d))
    # a casca (por fora e por dentro), fechada nas bordas
    b = p.bm('Metal#shell', 0.0)
    outer = [[b.verts.new(V(*T_(RL * i / nr if i else 0.001, a0 + (a1 - a0) * k / na))) for k in range(na + 1)] for i in range(nr + 1)]
    inner = [[b.verts.new(V(*T_(RL * i / nr if i else 0.001, a0 + (a1 - a0) * k / na, 0.06))) for k in range(na + 1)] for i in range(nr + 1)]
    for i in range(nr):
        for k in range(na):
            b.faces.new([outer[i][k], outer[i + 1][k], outer[i + 1][k + 1], outer[i][k + 1]]).smooth = True
            b.faces.new([inner[i][k + 1], inner[i + 1][k + 1], inner[i + 1][k], inner[i][k]]).smooth = True
    for k in range(na):                                                         # o ápice (o furinho de r = 1 mm fechado)
        b.faces.new([outer[0][k + 1], inner[0][k + 1], inner[0][k], outer[0][k]])
    for k in range(na):                                                         # a borda redonda de fora
        b.faces.new([outer[nr][k], inner[nr][k], inner[nr][k + 1], outer[nr][k + 1]])
    for i in range(nr):                                                         # as 2 bordas retas (o corte no meio)
        b.faces.new([outer[i][0], inner[i][0], inner[i + 1][0], outer[i + 1][0]])
        b.faces.new([outer[i + 1][na], inner[i + 1][na], inner[i][na], outer[i][na]])
    # o rebordo (a cantoneira em volta: a borda redonda e o corte reto)
    pts = [T_(RL + 0.02, a0 + (a1 - a0) * k / na, -0.01) for k in range(na + 1)]
    tube(p, 'Metal#rim', pts, 0.045, seg=8, smooth=True)
    cut = [T_(RL * (1 - i / nr), a0, -0.01) for i in range(nr + 1)] + [T_(RL * i / nr, a1, -0.01) for i in range(1, nr + 1)]
    tube(p, 'Metal#rim', cut, 0.04, seg=8, smooth=True)
    # os rebites: no rebordo de fora e no corte
    for k in range(1, 18):
        a = a0 + (a1 - a0) * k / 18
        q = T_(RL - 0.1, a, -0.035)
        c = Vector(q)
        n = (Vector(T_(RL - 0.1, a, -0.08)) - Vector(T_(RL - 0.1, a, 0.0))).normalized()
        p.cyl('Metal#rivet', tuple(c), tuple(n), 0.038, 0.04, seg=8, r2=0.024, bevel=0.0)
    for i in range(1, 9):
        for a in (a0, a1):
            r = RL * i / 9
            q = Vector(T_(r, a + (0.08 if a == a0 else -0.08) / max(r, 0.3), -0.035))
            n = (Vector(T_(r, a, -0.08)) - Vector(T_(r, a, 0.0))).normalized()
            p.cyl('Metal#rivet', tuple(q), tuple(n), 0.036, 0.04, seg=8, r2=0.022, bevel=0.0)
    # o meio círculo verde por fora (fechada, o círculo inteiro no topo da tampa): a faixa larga a pincel, a borda irregular
    bb = p.bm('Player#ring', 0.0)
    r0, r1 = 0.82, 1.18
    nn = 22
    rows = []
    for k in range(nn + 1):
        a = a0 + (a1 - a0) * (0.03 + 0.94 * k / nn)
        w0 = r0 + 0.035 * (_hash(k, side, 3) - 0.5)
        w1 = r1 + 0.04 * (_hash(k, side, 5) - 0.5)
        rows.append([bb.verts.new(V(*T_(w, a, -0.012))) for w in (w0, (w0 + w1) / 2, w1)])
    for k in range(nn):
        for j in range(2):
            bb.faces.new([rows[k][j], rows[k][j + 1], rows[k + 1][j + 1], rows[k + 1][j]]).smooth = True
    # o puxador por dentro e as costelas de reforço (vistas de quem olha a folha aberta)
    for a in (a0 + (a1 - a0) * 0.3, a0 + (a1 - a0) * 0.7):
        rod(p, 'Dark#rib', T_(0.25, a, 0.075), T_(RL - 0.12, a, 0.075), 0.035, seg=6)
    am = (a0 + a1) / 2
    rod(p, 'Dark#handle', T_(0.55, am - 0.25, 0.08), T_(0.55, am - 0.25, 0.22), 0.022, seg=6)
    rod(p, 'Dark#handle', T_(0.55, am + 0.25, 0.08), T_(0.55, am + 0.25, 0.22), 0.022, seg=6)
    rod(p, 'Dark#handle', T_(0.55, am - 0.25, 0.22), T_(0.55, am + 0.25, 0.22), 0.022, seg=6)
    # os braços da dobradiça (as 3 orelhas da folha em volta do pino)
    for x in (-0.6, 0.6):
        ax = math.atan2(side * math.sqrt(RL * RL - x * x), x)
        q0 = Vector(T((x, hz[1], hz[2])))
        q1 = Vector(T_(RL - 0.45, ax, -0.02))
        p.cyl('Dark#knuckle', tuple(q0), HAXIS, 0.075, 0.3, seg=12, bevel=0.0)
        rod(p, 'Metal#strap', tuple(q0), tuple(q1), 0.04, seg=6)
    # T() já dá o ponto no quadro da dobradiça (a folha aberta): a origem da peça é a dobradiça
    return finish(p)


# ===================================================================================== a torre que gira
def soldier(p, c, yaw, arms, lean=0.0, seed=0):
    """O soldado em pé (só o volume: a roupa cor de areia, o colete oliva com as bolsas, a cabeça enrolada no lenço verde e os óculos;
    nenhum rosto). arms = [(cotovelo, mão)] da direita e da esquerda, no quadro do soldado (x pra frente, y pra direita, z do pé)."""
    F = Frame(p, c, yaw=yaw)
    for s in (-1, 1):
        F.box('Dark#boot', (0.05, s * 0.12, 0.06), (0.28, 0.12, 0.12), bevel=0.02)
        F.rod('Cloth#leg', (0.0, s * 0.12, 0.1), (0.04, s * 0.115, 0.5), 0.07)
        F.rod('Cloth#leg', (0.04, s * 0.115, 0.5), (lean * 0.3, s * 0.1, 0.9), 0.08)
    hx = lean * 0.3
    F.box('Cloth#body', (hx, 0.0, 0.93), (0.24, 0.34, 0.2), bevel=0.05)
    F.cyl('Cloth#body', (hx + 0.02, 0.0, 1.18), 'z', 0.16, 0.5, seg=10, bevel=0.0, smooth=True)
    F.box('Crate#vest', (hx + 0.03, 0.0, 1.2), (0.28, 0.38, 0.4), pitch=-lean * 10, bevel=0.06)
    for s in (-1, 0, 1):
        F.box('Crate#pouch', (hx + 0.18, s * 0.11, 1.09), (0.07, 0.1, 0.13), bevel=0.02)
    F.cyl('Cloth#neck', (hx + 0.04, 0.0, 1.47), 'z', 0.06, 0.1, seg=8, bevel=0.0)
    hc = F.pt(hx + 0.05, 0.0, 1.61)
    revolve(p, 'Player#scarf', (hc[0], hc[1]), [(0.01, hc[2] - 0.12), (0.1, hc[2] - 0.1), (0.118, hc[2]), (0.105, hc[2] + 0.08),
                                                (0.055, hc[2] + 0.13), (0.01, hc[2] + 0.135)], seg=12)
    F.box('Dark#goggles', (hx + 0.15, 0.0, 1.63), (0.05, 0.15, 0.05), bevel=0.0)
    for s, (elbow, hand) in zip((1, -1), arms):
        sh = (hx + 0.02, s * 0.2, 1.4)
        F.rod('Cloth#arm', sh, elbow, 0.055)
        F.rod('Cloth#arm', elbow, hand, 0.045)
        F.box('Dark#glove', hand, (0.08, 0.07, 0.08), bevel=0.0)


def launcher(p, sh, el):
    """O lançador duplo no ombro (os 2 tubos oliva lado a lado, as tampas escuras, a faixa verde, a coronha com o gatilho, a mira e a
    caixinha do IFF): o centro na altura do ombro sh; devolve as 2 bocas."""
    e = math.radians(el)
    d = Vector((math.cos(e), 0.0, math.sin(e)))
    up = Vector((-math.sin(e), 0.0, math.cos(e)))
    c0 = Vector(sh) + up * 0.1 + d * 0.15
    mouths = []
    for dy in (-0.085, 0.085):
        c = c0 + Vector((0.0, dy, 0.0))
        p.cyl('Crate#tube', tuple(c), tuple(d), LT_R, LT_L, seg=16, bevel=0.0)
        f = c + d * LT_L / 2
        bk = c - d * LT_L / 2
        p.cyl('Dark#cap', tuple(f), tuple(d), LT_R + 0.018, 0.1, seg=16, bevel=0.0)
        p.cyl('Dark#cap', tuple(bk), tuple(d), LT_R + 0.015, 0.09, seg=16, bevel=0.0)
        p.cyl('Player#tube', tuple(c + d * 0.35), tuple(d), LT_R + 0.008, 0.13, seg=16, bevel=0.0)
        mouths.append(tuple(f + d * 0.05))
    for t in (-0.45, 0.4):
        p.obox('Dark#clamp', tuple(c0 + d * t), (0.06, 0.36, 0.2), pitch=el, bevel=0.0)
    p.obox('Dark#grip', tuple(c0 - up * 0.13 - d * 0.05), (0.32, 0.08, 0.1), pitch=el, bevel=0.0)
    p.obox('Dark#grip', tuple(c0 - up * 0.24 + d * 0.0), (0.06, 0.06, 0.18), pitch=el - 15, bevel=0.0)
    p.obox('Metal#sight', tuple(c0 + up * 0.1 + d * 0.1 + Vector((0.0, -0.17, 0.0))), (0.2, 0.06, 0.1), pitch=el, bevel=0.0)
    p.obox('Crate#iff', tuple(c0 - up * 0.12 + d * 0.3 + Vector((0.0, 0.16, 0.0))), (0.18, 0.1, 0.12), pitch=el, bevel=0.02)
    return mouths


def turret():
    """A torre (gira em (0, 0)): o pistão telescópico do fundo do poço até a plataforma, a plataforma de chapa xadrez com o rodapé de
    perigo e a grade, os 2 da equipe e o lançador duplo no ombro do atirador (apontado pra +X em repouso)."""
    p = new_part(PREFIX + '_Turret')
    for m in ('Metal#deck', 'Hazard#kick'):
        p.bm(m, 0.0)
    # o pistão (3 estágios) e o colar
    for (r, z0, z1) in ((0.22, 0.2, 0.62), (0.165, 0.6, 1.0), (0.12, 0.98, PZ - 0.1)):
        p.cyl('Metal#ram', (0.0, 0.0, (z0 + z1) / 2), 'z', r, z1 - z0, seg=20, bevel=0.0, smooth=True)
        p.cyl('Dark#seal', (0.0, 0.0, z1 - 0.02), 'z', r + 0.02, 0.05, seg=20, bevel=0.0)
    # a plataforma: o quadro de vigas, a chapa, o rodapé de perigo nos 4 lados, as mãos-francesas até o pistão
    p.box('Metal#deck', -PH, PH, -PH, PH, PZ - 0.06, PZ, bevel=0.0)
    for (x0, x1, y0, y1) in ((-PH, PH, -PH, -PH + 0.1), (-PH, PH, PH - 0.1, PH), (-PH, -PH + 0.1, -PH, PH), (PH - 0.1, PH, -PH, PH)):
        p.box('Dark#frame', x0, x1, y0, y1, PZ - 0.2, PZ - 0.06, bevel=0.0)
    for k in range(5):
        y = -PH + 0.1 + k * (2 * PH - 0.2) / 4
        p.box('Dark#frame', -PH + 0.1, PH - 0.1, y - 0.03, y + 0.03, PZ - 0.16, PZ - 0.06, bevel=0.0)
    for (x, y) in ((PH - 0.1, 0.0), (-PH + 0.1, 0.0), (0.0, PH - 0.1), (0.0, -PH + 0.1)):
        rod(p, 'Dark#brace', (x * 0.85, y * 0.85, PZ - 0.18), (x * 0.18, y * 0.18, PZ - 0.62), 0.035, seg=6)
    # a chapa xadrez (as nervuras em losango: o relevo que a luz rasante pega)
    for i in range(-5, 6):
        for j in range(-5, 6):
            if (i + j) % 2:
                continue
            p.obox('Metal#tread', (i * 0.125, j * 0.125, PZ + 0.008), (0.09, 0.025, 0.016), yaw=45 if (i % 2) else -45, bevel=0.0)
    kick = 0.16
    for (x0, x1, y0, y1) in ((-PH - 0.02, PH + 0.02, -PH - 0.02, -PH + 0.01), (-PH - 0.02, PH + 0.02, PH - 0.01, PH + 0.02),
                             (PH - 0.01, PH + 0.02, -PH, PH), (-PH - 0.02, -PH + 0.01, -PH, PH)):
        p.box('Hazard#kick', x0, x1, y0, y1, PZ, PZ + kick, bevel=0.0)
    # a grade: os postes nos cantos e no meio dos lados, os 2 tubos; aberta atrás (−X) no meio
    rail_h = 1.0
    posts = [(PH - 0.04, -PH + 0.04), (PH - 0.04, PH - 0.04), (-PH + 0.04, PH - 0.04), (-PH + 0.04, -PH + 0.04),
             (PH - 0.04, 0.0), (0.0, PH - 0.04), (0.0, -PH + 0.04), (-PH + 0.04, 0.32), (-PH + 0.04, -0.32)]
    for (x, y) in posts:
        rod(p, 'Trim#rail', (x, y, PZ), (x, y, PZ + rail_h), 0.03, seg=8)
    runs = [[(-PH + 0.04, 0.32), (-PH + 0.04, PH - 0.04), (PH - 0.04, PH - 0.04), (PH - 0.04, -PH + 0.04), (-PH + 0.04, -PH + 0.04),
             (-PH + 0.04, -0.32)]]
    for z in (PZ + rail_h, PZ + rail_h * 0.52):
        for run in runs:
            pts = [(x, y, z) for (x, y) in run]
            tube(p, 'Trim#rail', fillet(pts, 0.09, n=4), 0.026, seg=8)
    # a equipe: o atirador com o lançador no ombro direito (em pé, um pé à frente) e o segundo de pé ao lado, a mão na grade
    sh_a = (0.0, 0.2, PZ + 1.43)
    mouths = launcher(p, (sh_a[0] + 0.02, sh_a[1] + 0.06, sh_a[2] + 0.02), EL)
    soldier(p, (-0.08, 0.02, PZ), 0.0, [((0.22, 0.32, 1.18), (0.32, 0.22, 1.33)), ((0.24, -0.12, 1.12), (0.42, 0.08, 1.38))],
            lean=0.15, seed=1)
    soldier(p, (-0.18, -0.4, PZ), 20.0, [((0.12, 0.28, 1.12), (0.3, 0.22, 0.98)), ((0.22, -0.2, 1.15), (0.5, -0.2, 1.0))], seed=2)
    o = finish(p)
    return o, mouths


def write_preview(out):
    import json
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]], [PREFIX + '_Turret', [0, 0, 0]],
                      [PREFIX + '_HatchL', [HINGE_L[0], -HINGE_L[1], HINGE_L[2]]], [PREFIX + '_HatchR', [HINGE_R[0], -HINGE_R[1], HINGE_R[2]]]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1200, 1200]], ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1200, 900]],
                      ['side_left', 'ortho', [0, -1, 0], [0, 0, 1], [1200, 900]],
                      ['three_quarter', 'persp', 32.0, 215.0, [1400, 1100], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 32.0, 35.0, [1400, 1100], 30.0, 0.95]],
            'usa_at': [5.0, -6.5, 0.0], 'tank_views': True,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(out, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, indent=1)


def main():
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'gla_stinger_site_v5')
    os.makedirs(out, exist_ok=True)
    clear_scene()
    total = export(base(), out)
    clear_scene()
    t, mouths = turret()
    total += export(t, out)
    for side, hz in ((-1, HINGE_L), (1, HINGE_R)):
        clear_scene()
        total += export(bake_in_place(hatch_leaf(side), hz), out)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -3.3, 3.3, -3.3, 3.3, 2.2, post='Wood', plank='Crate', stripe='Hazard'), out)
    write_preview(out)
    print('MODEL total', total, 'tris')
    print('MODEL lote 8 x 8 m; turret_pivot_m (0, 0); plataforma z', PZ, '; muzzles', [tuple(round(v, 2) for v in m) for m in mouths],
          '; muzzle_height_m', round(sum(m[2] for m in mouths) / 2, 2), '; elevacao', EL)
    print('MODEL hatch_L hinge', tuple(round(v, 3) for v in HINGE_L), 'hatch_R hinge', tuple(round(v, 3) for v in HINGE_R), 'eixo', HAXIS,
          '; aberta', OPEN, 'graus')
    print('MODEL fire_points_m', [(0.0, 0.0, 0.5), (-2.3, 1.6, 0.6), (1.9, 2.2, 0.6)])


if __name__ == '__main__':
    main()
