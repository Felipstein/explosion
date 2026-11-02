"""A Gattling Cannon da China, versão 2 (refeita depois da galeria de 01/11/2026), modelada no Blender por script.

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- building Game/Tools/model_china_gattling_cannon_v2.py C:/Users/luisf/explosion/captures/game/models/china_gattling_cannon_v2

O veredito da v1 (docs/APROVADOS.md, 01/11): "parece outra coisa, um bunker talvez [...] e o que ficou péssimo é a sua insistência
em querer colocar sempre um quadrado de chão embaixo das coisas, o bagulho é redondo". A v1 (model_china_gattling_cannon.py, guardada)
era um bunker redondo baixo com uma torre de tanque em cima, sentado numa laje quadrada; e no jogo saía SEM os canos (o tambor era
uma peça à parte que o Units.json nunca ligou): sobrava a casamata.

A palavra (o que lê de cima em 1 segundo): A METRALHADORA ROTATIVA. O feixe de 6 canos comprido (5,3 m da ponta do motor à boca,
2,2 m pra fora da torre), com as braçadeiras e o anel vermelho da boca, no reparo aberto que gira em Z (o berço entre as 2 bochechas,
a caixa de munição com a CINTA DOURADA subindo até a culatra, a mira com o radar de tiro do lado); tudo em cima de uma TORRE REDONDA
de concreto de placas (a faixa vermelha, a gola de aço aparafusada), no PÉ REDONDO: o anel de concreto pré-moldado com os sacos de
areia por cima e o fosso curto (o piso do ninho em volta da torre, com as juntas das placas), a entrada atrás. Nada quadrado
embaixo: a base acompanha a forma (o redondo pisa num redondo), e o que se vê do chão do lote é o terreno.
Inspiração: o reparo do canhão rotativo naval chinês (Type 730: o berço aberto, a caixa de munição do lado, a mira eletro-óptica) e
os ninhos de defesa antiaérea de campanha (o anel de sacos sobre o anel de concreto). A régua do ZH: a Gattling Cannon lê pelo feixe
de canos e pela base redonda.

A cor do jogador (Player), onde a China usa: a faixa da torre (a das torres de resfriamento), o anel da boca do feixe (o do Gattling
Tank), a lona dobrada no reparo. A estrela do ELP (a do kit, aprovada em 01/11: a vermelha FIXA, slot Red, com a borda dourada e o 八一
dourado no meio: docs/elenco/EMBLEMAS.md) na placa da frente da torre e na caixa de munição.

Medidas (lote 8 × 8 m, frente em +X, pra onde os canos apontam em repouso): o anel de sacos com r 3,95 m no pé; a torre r 1,82 m no
pé e 1,6 m em cima, a gola de aço até 2,30 m; o prato giratório (r 1,58 m) de 2,31 a 2,48 m; o eixo dos canos a GUN_Z = 3,05 m; a
boca a MUZZLE_X = 3,72 m do pivô (o giro não sai do lote); a mira a ~3,6 m e a antena a ~4,9 m. Nada fixo acima de 2,30 m e nada
da torre abaixo de 2,31 m (o giro livre).

Peças (origem no chão, no centro do lote; coordenadas da Unreal, X frente, Y direita, Z cima, metros):
  SM_China_GattlingCannon_Base: o pé, o anel de sacos, o fosso, a torre de concreto e a gola;
  SM_China_GattlingCannon_Turret: o reparo que gira em volta de (0, 0) (turret_pivot_m = [0, 0]), COM o feixe de canos (fixo na
    malha, como no Gattling Tank: o jogo hoje só gira peça em Z fora do ar, e o Units.json liga só esta malha na torre);
  SM_China_GattlingCannon_Scaffold: o andaime da obra (tubos de aço, scaffold() do model_lib).
Materiais (slots do model_faction_kit.py): CnConcrete, CnPlinth, CnMetal, CnGold, Player, Red (a estrela), Dark, Glass, Light, Beacon,
Cloth (os sacos: CivCloth no jogo), e o aço pintado do reparo com os slots do tanque chinês (CnTankPaint, CnTankMetal).
"""
import json
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, clear_scene, export, scaffold  # noqa: E402,F401
from model_faction_kit import bayi_strokes, disc_ring, star_points, star_wall  # noqa: E402
from model_building_parts import BPart  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
from model_china_vehicle_kit import pillow, sweep_check, tarp_fold  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'china_gattling_cannon_v2')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_China_GattlingCannon'

HX = HY = 4.0                        # o lote 8 × 8
FZ = 0.06                            # o piso do ninho (o fosso)
R_FLOOR = 3.3
RING_IN, RING_OUT, RING_TOP, RING_TOP_R = 3.15, 3.95, 0.5, 3.66   # o anel de concreto: dentro, o pé de fora, o topo
R_BAGS = 3.42
GATE = (180.0, 16.0)                 # a entrada atrás: o ângulo do meio e a meia-abertura (graus)
T_R0, T_R1, T_Z0, T_Z1 = 1.82, 1.6, 0.32, 2.02    # o fuste da torre: o raio no pé e em cima, as alturas
COLLAR_Z = 2.30                      # o topo da gola de aço (o prato giratório começa 1 cm acima)
TT_R, TT_Z = 1.58, 2.48              # o prato giratório
GUN_Z = 3.05                         # o eixo do feixe
MUZZLE_X = 3.72                      # a boca, do pivô
R_CLUSTER, R_BARREL = 0.3, 0.085          # (a revisão) o feixe 20% mais grosso: de cima ele é a palavra


def tower_r(z):
    """O raio do fuste (as placas) na altura z: o tronco de cone (a parede inclinada da China)."""
    t = (z - T_Z0) / (T_Z1 - T_Z0)
    return T_R0 + (T_R1 - T_R0) * t


def polar(r, a_deg, z=None):
    a = math.radians(a_deg)
    return (r * math.cos(a), r * math.sin(a)) if z is None else (r * math.cos(a), r * math.sin(a), z)


def arc_solid(p, m, prof, a0, a1, n, bevel=0.0):
    """O sólido de um perfil [(r, z)...] (polígono fechado) varrido em volta do eixo Z de a0 a a1 graus, com as tampas nas pontas:
    o gomo de placa, o bloco do anel."""
    rings = []
    for j in range(n + 1):
        a = a0 + (a1 - a0) * j / n
        rings.append([polar(r, a, z) for (r, z) in prof])
    p.loft(m, rings, bevel=bevel)


def in_gate(a_deg, pad=0.0):
    d = (a_deg - GATE[0] + 180.0) % 360.0 - 180.0
    return abs(d) < GATE[1] + pad


# ------------------------------------------------------------------ a estrela do ELP
def pla_star(p, c, face, r, t=0.03):
    """A estrela do ELP aprovada (01/11, docs/aprovados/modelos/2026-11-01_estrelas_russia_china_APROVADAS.jpg): a do kit, star_wall do
    model_faction_kit.py (a borda CnGold, a estrela Red fixa, o 八一 em CnGold por cima), igual em todo lugar. Nas faces '-x' e '+y' é a
    própria star_wall. Nas faces '+x' e '-y' a star_wall leva o u (a direita de quem olha) pro lado errado (a Unreal é de mão esquerda:
    olhando pra −X a direita é −Y) e o 八一 sairia espelhado ("一八"): aí são as mesmas 3 camadas, com as mesmas peças do kit
    (star_points, bayi_strokes), e o u certo. Quando a star_wall for corrigida, é só chamar ela aqui."""
    if face in ('-x', '+y'):
        star_wall(p, c, face, r, t=t)
        return
    x, y, z = c
    if face == '+x':
        f = lambda u, v, d: (x + d, y - u, z + v)
    else:
        f = lambda u, v, d: (x - u, y - d, z + v)

    def lay(mat, pts2, d0, d1):
        p.loft(mat, [[f(u, v, d0) for (u, v) in pts2], [f(u, v, d1) for (u, v) in pts2]], bevel=0.0)
    lay('CnGold', star_points(0.0, 0.0, r * 1.12, r_in=r * 1.12 * 0.40, rot_deg=90.0), 0.0, t)
    lay('Red', star_points(0.0, 0.0, r, rot_deg=90.0), t, t * 1.6)
    for q in bayi_strokes(r):
        lay('CnGold', q, t * 1.6, t * 2.0)


# ===================================================================================================== o pé e a torre
def base():
    p = BPart(PREFIX + '_Base')
    # ---------------------------------------------------------------- o fosso: o piso redondo do ninho (a saia entra no chão)
    vp.lathe(p, 'CnPlinth#floor', (0.0, 0.0, 0.0), 'z', [(1.7, -0.3), (R_FLOOR, -0.3), (R_FLOOR, FZ - 0.03), (R_FLOOR - 0.03, FZ),
                                                         (1.7, FZ)], seg=64, closed=True)
    # as juntas das placas do piso (os anéis e as radiais)
    disc_ring(p, 'Dark#joint', (0.0, 0.0), 2.74, 2.7, FZ - 0.004, FZ + 0.006, seg=64)
    for k in range(16):
        a = 360.0 * (k + 0.5) / 16
        if in_gate(a, 4.0):
            continue
        x, y = polar(2.67, a)
        p.obox('Dark#joint', (x, y, FZ + 0.001), (0.9, 0.04, 0.012), yaw=a, bevel=0.0)
    # (a revisão) a canaleta de grelha em volta do pé da torre saiu: de cima, o anel escuro com as barras lia como um trilho redondo
    # (e trilho a China não tem: docs/CONCEITO_CONSTRUCOES.md §2); fica o anel de junta mais perto da torre
    disc_ring(p, 'Dark#joint', (0.0, 0.0), 2.2, 2.16, FZ - 0.004, FZ + 0.006, seg=64)
    # ---------------------------------------------------------------- o anel de concreto pré-moldado (os blocos) com a entrada atrás
    prof = [(RING_IN, -0.3), (RING_OUT, -0.3), (RING_OUT, 0.03), (RING_TOP_R, RING_TOP), (RING_IN + 0.04, RING_TOP),
            (RING_IN, RING_TOP - 0.04)]
    a_start, a_end = GATE[0] + GATE[1], GATE[0] - GATE[1] + 360.0
    nb = 18
    span = (a_end - a_start) / nb
    for k in range(nb):
        g = math.degrees(0.025 / RING_IN)
        arc_solid(p, 'CnConcrete#ring', prof, a_start + k * span + g, a_start + (k + 1) * span - g, 5, bevel=0.04)
    # as luzes de rodapé no topo do anel (viradas pro fosso)
    for a in (40.0, -40.0, 115.0, -115.0):
        x, y = polar(RING_IN + 0.02, a)
        p.obox('Dark#lamp', (x, y, RING_TOP - 0.16), (0.1, 0.3, 0.16), yaw=a, bevel=0.0)
        x, y = polar(RING_IN - 0.02, a)
        p.obox('Light', (x, y, RING_TOP - 0.16), (0.03, 0.22, 0.1), yaw=a, bevel=0.0)
    # ---------------------------------------------------------------- os sacos de areia em 3 fiadas no topo do anel
    bag = (0.62, 0.36, 0.22)
    for lay in range(3):
        r = R_BAGS - lay * 0.035
        circ = math.radians(a_end - a_start) * r
        n = int(circ / (bag[0] * 0.93))
        for k in range(n):
            a = a_start + (a_end - a_start) * (k + 0.5 + (0.5 if lay else 0.0)) / n
            if a > a_end - 1.5:
                continue
            x, y = polar(r, a)
            h = math.sin(k * 12.9898 + lay * 78.233) * 43758.5453 % 1.0
            x, y = polar(r + (h - 0.5) * 0.05, a)
            pillow(p, 'Cloth', (x, y, RING_TOP + bag[2] * 0.3 + lay * bag[2] * 0.8), (bag[0] * (0.92 + 0.1 * h), bag[1] - lay * 0.03,
                                                                                      bag[2]), yaw=a + 90 + (h - 0.5) * 10, seed=k + 17 * lay,
                   square=0.45, flat=0.5)
    # as pontas da entrada: 2 sacos deitados de través (o arremate) em cada fiada
    for a in (a_start + 1.2, a_end - 1.2):
        for lay in range(3):
            x, y = polar(R_BAGS - 0.02, a)
            pillow(p, 'Cloth', (x, y, RING_TOP + bag[2] * 0.3 + lay * bag[2] * 0.8), (0.66, 0.34, bag[2]), yaw=a + (4 if lay % 2 else -3),
                   seed=40 + lay, square=0.45, flat=0.5)
    # ---------------------------------------------------------------- a torre redonda
    # o pé claro (o degrau de concreto em volta), o miolo e as placas em gomos (2 alturas desencontradas, a junta funda entre elas)
    vp.lathe(p, 'CnPlinth#plinth', (0.0, 0.0, 0.0), 'z', [(1.3, 0.0), (1.98, 0.0), (1.98, T_Z0 - 0.07), (1.93, T_Z0), (1.3, T_Z0)],
             seg=64, closed=True)
    vp.lathe(p, 'CnConcrete#core', (0.0, 0.0, 0.0), 'z', [(0.0, T_Z0 - 0.02), (tower_r(T_Z0) - 0.1, T_Z0 - 0.02),
                                                           (tower_r(T_Z1) - 0.1, T_Z1), (0.0, T_Z1)], seg=48)
    z_mid = 1.16
    for (za, zb, off) in ((T_Z0, z_mid - 0.025, 0.0), (z_mid + 0.025, T_Z1, 0.5)):
        sec = [(tower_r(za) - 0.22, za), (tower_r(za), za), (tower_r(zb), zb), (tower_r(zb) - 0.22, zb)]
        for k in range(16):
            g = math.degrees(0.03 / T_R1)
            a0 = 360.0 * (k + off) / 16 + g
            a1 = 360.0 * (k + 1 + off) / 16 - g
            arc_solid(p, 'CnConcrete#gomo', sec, a0, a1, 4, bevel=0.06)
    # a faixa vermelha (a cor do jogador, a mesma das torres de resfriamento) no alto do fuste
    za, zb = 1.47, 1.77
    vp.lathe(p, 'Player#band', (0.0, 0.0, 0.0), 'z', [(tower_r(za) - 0.06, za), (tower_r(za) + 0.035, za), (tower_r(zb) + 0.035, zb),
                                                      (tower_r(zb) - 0.06, zb)], seg=64, closed=True)
    vp.lathe(p, 'CnGold#band', (0.0, 0.0, 0.0), 'z', [(tower_r(zb + 0.04) - 0.05, zb + 0.02), (tower_r(zb + 0.04) + 0.03, zb + 0.02),
                                                      (tower_r(zb + 0.08) + 0.03, zb + 0.08), (tower_r(zb + 0.08) - 0.05, zb + 0.08)],
             seg=64, closed=True)
    # a gola de aço (o anel que segura o rolamento), as porcas em volta e o rolamento escuro (a fresta do giro)
    vp.lathe(p, 'CnMetal#collar', (0.0, 0.0, 0.0), 'z', [(1.25, T_Z1 - 0.06), (1.66, T_Z1 - 0.06), (1.7, T_Z1 - 0.02), (1.7, COLLAR_Z - 0.04),
                                                        (1.65, COLLAR_Z), (1.25, COLLAR_Z)], seg=64, closed=True)
    for k in range(36):
        a = 360.0 * k / 36
        x, y = polar(1.7, a)
        vp.nut(p, 'CnMetal', (x, y, T_Z1 + 0.13), (math.cos(math.radians(a)), math.sin(math.radians(a)), 0.0), r=0.035, h=0.03)
    disc_ring(p, 'Dark#race', (0.0, 0.0), 1.6, 1.2, COLLAR_Z - 0.005, COLLAR_Z + 0.004, seg=64)
    # a placa da frente com a estrela do ELP (a placa redonda chumbada no fuste: a borda de trás entra no concreto)
    zs = 0.86
    xr = tower_r(zs)
    vp.lathe(p, 'CnPlinth#plaque', (xr - 0.2, 0.0, zs), 'x', [(0.0, 0.0), (0.5, 0.0), (0.5, 0.26), (0.47, 0.29), (0.0, 0.29)], seg=40)
    pla_star(p, (xr + 0.09, 0.0, zs), '+x', 0.38, t=0.03)
    # as luzes da gola (frente) e as grelhas de ventilação dos lados
    for a in (30.0, -30.0):
        x, y = polar(1.72, a)
        p.obox('Dark#lamp', (x, y, T_Z1 + 0.15), (0.12, 0.26, 0.16), yaw=a, bevel=0.0)
        x, y = polar(1.79, a)
        p.obox('Light', (x, y, T_Z1 + 0.15), (0.03, 0.2, 0.1), yaw=a, bevel=0.0)
    for a in (90.0, -90.0):
        z = 0.75
        x, y = polar(tower_r(z) - 0.05, a)
        p.obox('CnMetal#vent', (x, y, z), (0.2, 0.8, 0.5), yaw=a, pitch=-7, bevel=0.0)
        for k in range(4):
            x, y = polar(tower_r(z) + 0.06, a)
            p.obox('Dark', (x, y, z - 0.17 + k * 0.11), (0.05, 0.7, 0.045), yaw=a, pitch=-7 + 35, bevel=0.0)
    # a porta blindada atrás (−X, de frente pra entrada do anel): o portal de concreto que sai do fuste, a folha de aço recuada, o
    # degrau e a luz
    p.box('CnConcrete#portal', -2.1, -1.2, -0.78, 0.78, 0.0, 1.92, bevel=0.05)
    p.box('CnConcrete#portal', -2.18, -1.2, -0.86, 0.86, 1.8, 2.0, bevel=0.03)
    p.box('Dark', -2.12, -2.09, -0.52, 0.52, T_Z0 - 0.04, 1.66, bevel=0.0)                 # a fresta escura em volta da folha
    p.box('CnMetal#door', -2.16, -2.1, -0.46, 0.46, T_Z0, 1.6, bevel=0.0)                   # a folha de aço
    for zz in (0.66, 1.16):
        p.box('Dark', -2.17, -2.155, -0.42, 0.42, zz - 0.02, zz + 0.02, bevel=0.0)
    p.box('CnMetal', -2.21, -2.16, 0.28, 0.36, 0.9, 1.05, bevel=0.0)                        # a maçaneta (a alavanca)
    p.box('CnPlinth#step', -2.5, -2.08, -0.62, 0.62, 0.0, T_Z0 - 0.12, bevel=0.02)
    p.box('Dark#lamp', -2.28, -2.1, -0.12, 0.12, 1.68, 1.78, bevel=0.0)
    p.box('Light', -2.26, -2.14, -0.09, 0.09, 1.665, 1.68, bevel=0.0)
    # a escada de mão (atrás, à direita) do fosso até a gola, afastada do fuste nas mãos-francesas
    la = 128.0
    e = Vector((-math.sin(math.radians(la)), math.cos(math.radians(la)), 0.0))     # a tangente
    rr = 1.98
    c = Vector(polar(rr, la, 0.0))
    for s in (-1, 1):
        q = c + e * (0.24 * s)
        vp.tube(p, 'CnMetal#ladder', [(q.x, q.y, FZ), (q.x, q.y, COLLAR_Z - 0.02)], 0.03, seg=8)
        for zz in (0.5, 1.9):
            w = Vector(polar(tower_r(zz) - 0.05, la, zz)) + e * (0.24 * s)
            vp.tube(p, 'CnMetal#ladder', [(q.x, q.y, zz), (w.x, w.y, zz)], 0.025, seg=6)
    for k in range(7):
        zz = 0.3 + k * 0.3
        a_ = c - e * 0.24
        b_ = c + e * 0.24
        vp.tube(p, 'CnMetal#ladder', [(a_.x, a_.y, zz), (b_.x, b_.y, zz)], 0.022, seg=6)
    # as caixas de munição encostadas no anel, por dentro (o paiol de pronto uso), e o extintor
    for (a, n) in ((222.0, 3), (58.0, 2)):
        x, y = polar(2.8, a)
        tx, ty = polar(1.0, a + 90.0)                                              # a tangente (as caixas deitadas ao longo do anel)
        for k, (u, lay) in enumerate([(-0.42, 0), (0.42, 0), (0.0, 1)][:n]):
            vp.ammo_box(p, (x + tx * u, y + ty * u, FZ + 0.19 + lay * 0.39), (0.78, 0.38, 0.38), yaw=a + 90 + (5 if k % 2 else -3),
                        m='CnTankPaint', latch_m='Dark')
    x, y = polar(2.55, 212.0)
    vp.lathe(p, 'Player#ext', (x, y, FZ), 'z', [(0.0, 0.0), (0.09, 0.0), (0.09, 0.5), (0.05, 0.56), (0.0, 0.58)], seg=12)
    return p.finish()


# ===================================================================================================== o reparo (gira em Z)
def gun(p):
    """A metralhadora rotativa: o motor atrás com as aletas, a caixa da culatra, o rotor nervurado, os 6 canos com a boca alargada e
    furada, o eixo do meio, as 2 braçadeiras, o anel vermelho da boca (a cor do jogador, o do Gattling Tank)."""
    c = (0.0, 0.0, GUN_Z)
    vp.lathe(p, 'CnTankMetal#motor', c, 'x', [(0.0, -1.64), (0.2, -1.64), (0.27, -1.6), (0.27, -1.22), (0.0, -1.22)], seg=24)
    for x in (-1.54, -1.44, -1.34):
        vp.ring_band(p, 'CnTankMetal#fin', x, x + 0.05, 0.27, GUN_Z, lip=0.035, seg=24)
    vp.lathe(p, 'CnTankPaint#gun', c, 'x', [(0.0, -1.24), (0.38, -1.24), (0.47, -1.15), (0.5, -0.95), (0.5, 0.42), (0.47, 0.55),
                                           (0.0, 0.55)], seg=36)
    for x in (-0.82, 0.18):
        vp.ring_band(p, 'CnTankMetal#band', x, x + 0.08, 0.5, GUN_Z, lip=0.02, seg=36)
    vp.lathe(p, 'CnTankMetal#rotor', c, 'x', [(0.0, 0.54), (0.43, 0.54), (0.47, 0.58), (0.47, 0.86), (0.43, 0.9), (0.0, 0.9)], seg=36)
    for k in range(6):
        a = math.radians(60 * k)
        p.obox('CnTankMetal#rib', (0.72, 0.47 * math.cos(a), GUN_Z + 0.47 * math.sin(a)), (0.34, 0.08, 0.08), roll=-math.degrees(a),
               bevel=0.0)
    for k in range(6):
        a = math.radians(60 * k + 30)
        y, z = R_CLUSTER * math.cos(a), GUN_Z + R_CLUSTER * math.sin(a)
        vp.gun_barrel(p, [(0.86, 0.104), (1.1, 0.104), (1.18, R_BARREL), (MUZZLE_X - 0.2, R_BARREL), (MUZZLE_X - 0.15, 0.1),
                          (MUZZLE_X, 0.1)], z, y=y, m='CnMetal#barrel', seg=12, bore=0.04, bore_depth=0.18, bore_m='Dark')
    p.cyl('CnTankMetal#axle', ((0.9 + MUZZLE_X - 0.15) / 2, 0.0, GUN_Z), 'x', 0.1, MUZZLE_X - 0.15 - 0.9, seg=12, bevel=0.0)
    rc = R_CLUSTER + R_BARREL + 0.055
    for (x0, x1) in ((1.72, 1.86), (2.84, 2.96)):
        vp.lathe(p, 'CnTankMetal#clamp', c, 'x', [(0.0, x0), (rc - 0.02, x0), (rc, x0 + 0.02), (rc, x1 - 0.02), (rc - 0.02, x1), (0.0, x1)], seg=36)
        vp.bolt_ring(p, 'CnTankMetal', (x1, 0.0, GUN_Z), 'x', rc - 0.035, 6, r=0.022, h=0.02, phase=0.0)
    x0, x1 = MUZZLE_X - 0.46, MUZZLE_X - 0.3
    vp.lathe(p, 'Player#ring', c, 'x', [(0.0, x0), (rc - 0.02, x0), (rc, x0 + 0.02), (rc, x1 - 0.02), (rc - 0.02, x1), (0.0, x1)], seg=36)


def turret():
    p = BPart(PREFIX + '_Turret')
    z0 = COLLAR_Z + 0.01
    # o prato giratório com as porcas em volta e o piso antiderrapante
    vp.lathe(p, 'CnTankMetal#tt', (0.0, 0.0, 0.0), 'z', [(0.0, z0), (TT_R - 0.05, z0), (TT_R, z0 + 0.05), (TT_R, TT_Z - 0.04),
                                                        (TT_R - 0.04, TT_Z), (0.0, TT_Z)], seg=64)
    vp.bolt_ring(p, 'CnTankMetal', (0.0, 0.0, TT_Z), 'z', TT_R - 0.11, 30, r=0.028, h=0.025)
    disc_ring(p, 'Dark#deck', (0.0, 0.0), TT_R - 0.2, TT_R - 0.24, TT_Z - 0.003, TT_Z + 0.004, seg=64)
    # as 2 bochechas do berço (a lápide de aço: o pé largo no prato e a cabeça redonda em volta do munhão) e os munhões
    pts = [(-0.8, TT_Z - 0.01), (0.8, TT_Z - 0.01)]
    for k in range(13):
        a = math.radians(-12 + 204 * k / 12)
        pts.append((0.6 * math.cos(a), GUN_Z + 0.6 * math.sin(a)))
    X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
    for s in (-1, 1):
        y0 = 0.6 if s > 0 else -0.74
        vp.prism(p, 'CnTankPaint#cheek', pts, (0.0, y0, 0.0), X, Z, Y, 0.14, bevel=0.012)
        vp.lathe(p, 'CnTankMetal#hub', (0.0, 0.74 * s, GUN_Z), (0.0, s, 0.0), [(0.0, 0.0), (0.24, 0.0), (0.26, 0.02), (0.26, 0.1),
                                                                                (0.2, 0.15), (0.0, 0.15)], seg=28)
        vp.bolt_ring(p, 'CnTankMetal', (0.0, 0.89 * s, GUN_Z), (0.0, s, 0.0), 0.15, 6, r=0.022, h=0.02)
        p.cyl('CnTankMetal#pin', (0.0, 0.56 * s, GUN_Z), 'y', 0.12, 0.16, seg=16, bevel=0.0)
    gun(p)
    # a caixa de munição (−Y, atrás, encostada na bochecha) com a estrela do ELP, a tampa, e a CINTA DOURADA subindo até a culatra
    bc = Vector((-0.55, -0.95, TT_Z + 0.42))
    vp.rbox(p, 'CnTankPaint#ammo', tuple(bc), (0.85, 0.5, 0.84), r=0.06, k=3, bevel=0.012)
    vp.rbox(p, 'CnTankMetal#lid', (bc.x, bc.y, TT_Z + 0.86), (0.89, 0.54, 0.06), r=0.07, k=3, bevel=0.0)
    for x in (bc.x - 0.25, bc.x + 0.25):
        vp.grab_handle(p, 'CnTankMetal', (x - 0.09, bc.y - 0.25, TT_Z + 0.6), (x + 0.09, bc.y - 0.25, TT_Z + 0.6), (0, -1, 0), height=0.06,
                       r=0.016)
    pla_star(p, (bc.x, bc.y - 0.25, TT_Z + 0.38), '-y', 0.17, t=0.015)
    p.box('Dark#slot', bc.x + 0.05, bc.x + 0.33, bc.y + 0.02, bc.y + 0.2, TT_Z + 0.885, TT_Z + 0.895, bevel=0.0)
    belt = vp.bent_bar([(bc.x + 0.2, bc.y + 0.11, TT_Z + 0.9), (bc.x + 0.2, bc.y + 0.14, GUN_Z + 0.66), (-0.82, -0.4, GUN_Z + 0.7),
                        (-0.82, -0.3, GUN_Z + 0.52)], R=0.16, n=5)
    vp.chute(p, 'CnGold#belt', belt, 0.24, 0.07, pitch=0.055)
    vp.rbox(p, 'CnTankMetal#feed', (-0.82, -0.22, GUN_Z + 0.49), (0.34, 0.28, 0.14), r=0.03, k=2)
    # a mira (+Y, à frente): o poste, a cabeça blindada com as 2 lentes e a pala, e o radar de tiro redondo em cima
    sx, sy = 0.05, 1.14
    p.cyl('CnTankMetal#post', (sx, sy, TT_Z + 0.27), 'z', 0.1, 0.54, seg=16, bevel=0.0)
    vp.rbox(p, 'CnTankPaint#sight', (sx + 0.05, sy, TT_Z + 0.76), (0.64, 0.42, 0.44), r=0.06, k=3, bevel=0.012)
    p.cyl('Dark', (sx + 0.38, sy - 0.08, TT_Z + 0.78), 'x', 0.13, 0.04, seg=20, bevel=0.0)
    p.cyl('Glass', (sx + 0.4, sy - 0.08, TT_Z + 0.78), 'x', 0.1, 0.02, seg=20, bevel=0.0)
    p.cyl('Glass', (sx + 0.39, sy + 0.12, TT_Z + 0.82), 'x', 0.055, 0.02, seg=12, bevel=0.0)
    p.obox('CnTankMetal#hood', (sx + 0.42, sy, TT_Z + 1.0), (0.22, 0.46, 0.04), pitch=-14, bevel=0.0)
    p.cyl('CnTankMetal#post', (sx - 0.05, sy, TT_Z + 1.08), 'z', 0.05, 0.2, seg=10, bevel=0.0)
    d = Vector((math.cos(math.radians(15)), 0.0, math.sin(math.radians(15))))
    vp.lathe(p, 'CnTankMetal#dish', (sx - 0.08, sy, TT_Z + 1.4), tuple(d), [(0.0, 0.0), (0.12, 0.012), (0.28, 0.06), (0.32, 0.085),
                                                                            (0.3, 0.095), (0.14, 0.05), (0.0, 0.036)], seg=32)
    p.cyl('CnTankMetal', tuple(Vector((sx - 0.08, sy, TT_Z + 1.4)) + d * 0.18), tuple(d), 0.018, 0.3, seg=6, bevel=0.0)
    p.cyl('Dark', tuple(Vector((sx - 0.08, sy, TT_Z + 1.4)) + d * 0.33), tuple(d), 0.04, 0.06, seg=10, bevel=0.0)
    p.box('CnTankMetal#post', sx - 0.15, sx - 0.02, sy - 0.04, sy + 0.04, TT_Z + 1.16, TT_Z + 1.38, bevel=0.0)
    # atrás: a caixa de força com a lona vermelha dobrada em cima (a cor do jogador) e o cabo até a culatra; a antena com a baliza
    vp.rbox(p, 'CnTankPaint#pwr', (-1.04, 0.6, TT_Z + 0.25), (0.46, 0.6, 0.5), r=0.05, k=3, bevel=0.012)
    for k in range(3):
        p.box('Dark', -1.28, -1.265, 0.38 + k * 0.16, 0.48 + k * 0.16, TT_Z + 0.1, TT_Z + 0.4, bevel=0.0)
    tarp_fold(p, (-1.04, 0.6, TT_Z + 0.5), (0.42, 0.56, 0.16), m='Player', strap_m='Cloth', seed=5.0)
    vp.tube(p, 'Dark#cable', [(-0.81, 0.5, TT_Z + 0.3), (-0.66, 0.36, TT_Z + 0.2), (-0.55, 0.2, GUN_Z - 0.47), (-0.55, 0.05, GUN_Z - 0.41)],
            0.035, seg=8)
    ax_, ay_ = -1.3, -0.5
    p.cyl('CnTankMetal#ant', (ax_, ay_, TT_Z + 0.06), 'z', 0.08, 0.12, seg=12, bevel=0.0)
    p.cyl('Dark#ant', (ax_, ay_, TT_Z + 0.12 + 1.1), 'z', 0.018, 2.2, seg=6, bevel=0.0)
    p.cyl('Beacon', (ax_, ay_, TT_Z + 2.36), 'z', 0.045, 0.08, seg=10, bevel=0.0)
    return p.finish()


def tris(o):
    return sum(len(poly.vertices) - 2 for poly in o.data.polygons)


def main():
    clear_scene()
    b = base()
    total = export(b, OUT)
    t = turret()
    sweep_check([b], [t], (0.0, 0.0), step=10, label='torre (o reparo contra o pé)')
    total += export(t, OUT)
    s = scaffold(PREFIX + '_Scaffold', -2.1, 2.1, -2.1, 2.1, COLLAR_Z + 0.4, post='CnMetal')
    total += export(s, OUT)
    print('MODEL total', total, 'tris (antes do detalhe)')
    print('MODEL turret_pivot_m [0, 0]; muzzle_height_m', GUN_Z, '; a boca a', MUZZLE_X, 'm do pivô (+X em repouso); o prato de',
          COLLAR_Z + 0.01, 'a', TT_Z, 'm')
    print('MODEL fire_points_m', [[-2.6, 1.3, 0.7], [2.2, -2.3, 0.7], [0.0, 0.0, 2.6]])
    # a folha da prévia (model_preview.py lê o preview.json da pasta)
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]], [PREFIX + '_Turret', [0, 0, 0]]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1200, 1200]],
                      ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1200, 900]],
                      ['side', 'ortho', [0, -1, 0], [0, 0, 1], [1200, 900]],
                      ['three_quarter', 'persp', 32.0, 215.0, [1600, 1000], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 30.0, 35.0, [1600, 1000], 30.0, 0.95]],
            'usa_at': [1.0, 8.5, 0.0], 'tank_views': True,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 28.0, 0.75]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, indent=1)


if __name__ == '__main__':
    main()
