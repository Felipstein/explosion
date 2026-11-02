"""A Gattling Cannon (a defesa de base da China) provisória, modelada no Blender por script (o elenco, 31/10/2026: docs/ELENCO.md §4.1).

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_china_gattling_cannon.py -- captures/game/models/china_gattling_cannon

A defesa anti-infantaria e antiaérea da China (ZH: $1 200, 1 000 de vida, −3 de energia; chão 10 a cada 0,25 s a 59 m, ar 5 a
cada 0,25 s a 105 m). Na bíblia da China (docs/CONCEITO_CONSTRUCOES.md §2 e docs/CONCEITO_UNIDADES.md §3): concreto escuro
puxado pro oliva, o aço pintado do oliva dos tanques, a torre "angulosa como um escudo", a estrela vermelha, a lona vermelha
dobrada rente (a cor do jogador do tanque chinês), nunca cruz vermelha.

A palavra (o que lê de cima): A METRALHADORA DE 6 CANOS NUMA TORRE BAIXA BLINDADA SOBRE O BUNKER REDONDO. Inspiração real: o
canhão rotativo naval chinês Type 730 (7 canos de 30 mm) e Type 1130 (11 canos), aqui com 6 canos num tambor que gira; os
bunkers redondos de concreto com a saia inclinada (a terra empurrada contra a parede). Assinatura redonda num mundo de
retângulos (estudo §3b, regra 2): de cima, o círculo do bunker com a torre em ponta de escudo e os canos pra fora. Peça por
peça:
- o bunker: a saia inclinada, o tambor de concreto com as juntas da fôrma e a junta de concretagem, a cornija, o teto
  abaulado com o anel de rolamento; a placa da estrela vermelha na frente; as 4 grelhas de ventilação; a porta blindada
  atrás (o nicho de concreto, a faixa de perigo, os degraus, a luz) e a escada de mão; o alçapão de munição no teto; as luzes;
- a torre (gira): o corpo blindado facetado em ponta (o escudo), o mantelete inclinado na frente com a luva do tambor, a caixa
  de munição à esquerda (−Y) com a estrela vermelha e a calha articulada até a arma, o sensor eletro-óptico com a lente à
  direita, a antena atrás, o cesto atrás com a lona vermelha dobrada, os parafusos e a escotilha de manutenção;
- os canos (giram no eixo deles enquanto atira): o rotor, os 6 canos, as 2 braçadeiras, o eixo central e as bocas.

Lote 8 × 8 m (X × Y), frente em +X (pra onde os canos apontam em repouso). Alturas: o bunker baixo (2,2 m no teto, a saia
inclinada até 1 m), a torre 3,5 m (do tamanho da torre de um tanque: a arma é a palavra), o sensor 3,9 m, a antena 6,1 m; o eixo
dos canos a 2,97 m; as bocas a 4,0 m do pivô (o giro não sai do lote). As defesas são pequenas (estudo §3b, regra 4).

Peças (origem no chão, no centro do lote; coordenadas da Unreal, X frente, Y direita, Z cima, metros):
  SM_China_GattlingCannon_Base: a fundação e o bunker;
  SM_China_GattlingCannon_Turret: a torre (gira em volta de (0, 0); turret_pivot_m = [0, 0]); mesma origem do modelo;
  SM_China_GattlingCannon_Barrels: SÓ o tambor de 6 canos, origem no eixo dele (na face de trás do rotor); o jogo põe em
    BARREL_AT (no referencial da torre, que é o do modelo com a torre em repouso) e gira em volta do X local enquanto atira;
  SM_China_GattlingCannon_Scaffold: o andaime da obra (tubos de aço, scaffold() do model_lib).
Materiais (slots do model_faction_kit.py): CnConcrete, CnPlinth, CnMetal, CnGold, Player (a estrela, a lona), Dark, Trim,
Glass, Light, Hazard, Beacon; e o aço pintado da torre com os slots do tanque chinês (CnTankPaint, CnTankMetal), pra torre
bater com os tanques.
"""
import math
import os
import sys

import bpy  # noqa: F401
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, clear_scene, export, scaffold  # noqa: E402
from model_faction_kit import foundation, ladder, polygon_prism, star_points, star_wall, revolve, disc_ring  # noqa: E402
from model_china_war_factory import bar, louver  # noqa: E402
from model_building_parts import BPart, bake_in_place  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'china_gattling_cannon')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_China_GattlingCannon'

HX = HY = 4.0                    # o lote 8 × 8
G = 0.14                         # o topo da laje (a fundação do kit)
R_SKIRT, R_WALL = 3.55, 2.85     # a saia (no chão) e o tambor do bunker
Z_SKIRT, Z_WALL, Z_ROOF = G + 0.85, 1.85, 2.2   # o topo da saia, o topo do tambor (a cornija), o topo do teto abaulado
RING_Z = Z_ROOF                  # onde a torre senta
S = 1.25                         # a escala da torre (a arma é a palavra: a torre do tamanho da torre de um tanque)
GUN_Z = 2.97                     # o eixo dos canos
BARREL_AT = (1.9, 0.0, GUN_Z)    # a face de trás do rotor, no eixo
BARREL_LEN = 2.1                 # do rotor até a boca (a boca a 4,0 m do pivô: o giro não sai do lote)
N_BARRELS, R_CLUSTER, R_BARREL = 6, 0.235, 0.075


def ring_obox(p, m, a_deg, r, z, size, tilt=0.0):
    """Uma caixa encostada na parede redonda no ângulo a (a face de fora tangente): as juntas, as grelhas, as luzes."""
    a = math.radians(a_deg)
    p.obox(m, (r * math.cos(a), r * math.sin(a), z), size, yaw=a_deg, pitch=tilt, bevel=0.0)


def bunker():
    p = BPart(PREFIX + '_Base')
    foundation(p, HX, HY, m='CnPlinth', z=G, skirt=1.5)
    # o corpo: a saia inclinada, o degrau, o tambor, a cornija, o teto abaulado (um sólido de revolução só)
    prof = [(0.02, G - 0.05), (R_SKIRT, G - 0.05), (R_SKIRT, G + 0.04), (R_WALL + 0.12, Z_SKIRT), (R_WALL, Z_SKIRT + 0.07),
            (R_WALL, Z_WALL - 0.05), (R_WALL + 0.12, Z_WALL), (R_WALL + 0.12, Z_WALL + 0.2), (R_WALL - 0.3, Z_WALL + 0.24),
            (2.1, Z_ROOF - 0.07), (1.5, Z_ROOF), (0.02, Z_ROOF)]
    p.bm('CnConcrete#body', 0.03)                # o chanfro duro nas quinas da saia, do tambor e da cornija
    revolve(p, 'CnConcrete#body', (0.0, 0.0), prof, seg=48, smooth=True)
    # (31/10, o detalhe AAA) as placas pré-moldadas em gomos: a saia inclinada e o tambor em 16 segmentos cada, com o chanfro largo
    # e a junta funda entre eles (o sulco mostra o miolo; antes eram juntas pintadas), o tambor desencontrado meio gomo da saia
    def gomo(prof, a0, a1, m):
        n = 4
        rings = []
        for j in range(n + 1):
            a = a0 + (a1 - a0) * j / n
            rings.append([(r * math.cos(a), r * math.sin(a), z) for (r, z) in prof])
        p.loft(m, rings, bevel=0.08)
    skirt = [(R_SKIRT - 0.35, G - 0.05), (R_SKIRT + 0.08, G - 0.05), (R_WALL + 0.22, Z_SKIRT + 0.03), (R_WALL - 0.05, Z_SKIRT - 0.02)]
    drum = [(R_WALL - 0.25, Z_SKIRT + 0.05), (R_WALL + 0.09, Z_SKIRT + 0.05), (R_WALL + 0.09, Z_WALL - 0.07), (R_WALL - 0.25, Z_WALL - 0.07)]
    for k in range(16):
        for (prof, r, off) in ((skirt, R_SKIRT - 0.2, 0.0), (drum, R_WALL, 0.5)):
            g = 0.035 / r
            a0 = 2 * math.pi * (k + off) / 16 + g
            a1 = 2 * math.pi * (k + 1 + off) / 16 - g
            gomo(prof, a0, a1, 'CnConcrete#gomo')
    # o anel de rolamento da torre no teto
    disc_ring(p, 'CnMetal', (0.0, 0.0), 2.05, 1.8, Z_ROOF - 0.02, Z_ROOF + 0.05, seg=48)
    # a placa da estrela na frente (+X): a placa de concreto, o aro dourado e a estrela vermelha
    p.box('CnPlinth#plate', R_WALL - 0.25, R_WALL + 0.2, -0.6, 0.6, Z_SKIRT - 0.05, Z_WALL - 0.08, bevel=0.06)
    zc = (Z_SKIRT + Z_WALL) / 2
    p.cyl('CnGold', (R_WALL + 0.22, 0.0, zc), 'x', 0.44, 0.03, seg=32, bevel=0.0)
    p.cyl('CnConcrete', (R_WALL + 0.235, 0.0, zc), 'x', 0.39, 0.03, seg=32, bevel=0.0)
    star_wall(p, (R_WALL + 0.245, 0.0, zc), '+x', 0.34, t=0.02)
    # as 4 grelhas de ventilação (nos quartos) e as 2 luzes da frente
    for a in (55.0, -55.0, 125.0, -125.0):
        ring_obox(p, 'CnMetal', a, R_WALL + 0.11, zc + 0.05, (0.08, 0.9, 0.4))
        for k in range(3):
            ring_obox(p, 'Dark', a, R_WALL + 0.16, zc - 0.07 + k * 0.12, (0.04, 0.8, 0.05), tilt=35)
    for a in (28.0, -28.0):
        ring_obox(p, 'Dark', a, R_WALL + 0.14, zc + 0.15, (0.12, 0.24, 0.2))
        ring_obox(p, 'Light', a, R_WALL + 0.21, zc + 0.15, (0.03, 0.2, 0.16))
    # a porta blindada atrás (−X): o nicho de concreto que atravessa a saia (sobe um pouco acima da cornija), a porta, a
    # faixa de perigo, os degraus, a luz
    zn = G + 2.3
    p.box('CnConcrete', -R_SKIRT, -R_WALL + 0.3, -0.95, 0.95, G, zn, bevel=0.05)
    p.box('CnConcrete', -R_SKIRT - 0.1, -R_WALL + 0.3, -1.05, 1.05, zn - 0.05, zn + 0.22, bevel=0.03)
    dx = -R_SKIRT
    p.box('CnMetal', dx - 0.04, dx, -0.55, 0.55, G + 0.3, G + 2.0, bevel=0.0)
    for zz in (G + 0.7, G + 1.4):
        p.box('Dark', dx - 0.06, dx - 0.03, -0.5, 0.5, zz - 0.03, zz + 0.03, bevel=0.0)
    p.box('Dark', dx - 0.07, dx - 0.04, 0.32, 0.42, G + 1.1, G + 1.3, bevel=0.0)
    for (y0, y1) in ((-0.75, -0.55), (0.55, 0.75)):
        for k in range(6):
            zz = G + 0.3 + k * 0.28
            p.box('Hazard' if k % 2 == 0 else 'Dark', dx - 0.03, dx, y0, y1, zz, zz + 0.28, bevel=0.0)
    for k in range(2):
        p.box('CnPlinth', dx - 0.21 * (2 - k), dx, -0.7, 0.7, G, G + 0.15 * (k + 1), bevel=0.02)
    p.box('Dark', dx - 0.14, dx, -0.12, 0.12, G + 2.05, G + 2.22, bevel=0.0)
    p.box('Light', dx - 0.18, dx - 0.13, -0.1, 0.1, G + 2.07, G + 2.2, bevel=0.0)
    ladder(p, -R_WALL - 0.12, 1.05, 1.5, Z_SKIRT, Z_WALL + 0.25)
    # o alçapão de munição no teto (atrás) e os 2 tubos de respiro (cogumelos) dos lados
    p.box('CnMetal', -2.65, -2.15, -0.35, 0.35, Z_ROOF - 0.12, Z_ROOF - 0.0, bevel=0.02)
    p.box('Dark', -2.45, -2.35, -0.25, 0.25, Z_ROOF - 0.01, Z_ROOF + 0.03, bevel=0.0)
    for a in (90.0, -90.0):
        r = 2.45
        c = (r * math.cos(math.radians(a)), r * math.sin(math.radians(a)))
        p.cyl('CnMetal', (c[0], c[1], Z_ROOF + 0.05), 'z', 0.08, 0.5, seg=10, bevel=0.0)
        p.cyl('CnMetal', (c[0], c[1], Z_ROOF + 0.32), 'z', 0.18, 0.1, seg=12, r2=0.06, bevel=0.0)
    return p.finish()


def turret():
    """A torre (gira em volta de (0, 0)): o anel, o corpo facetado em ponta, as placas aparafusadas, o mantelete, o berço
    da arma, a caixa de munição com a calha, o sensor, a antena, o cesto com a lona vermelha."""
    p = BPart(PREFIX + '_Turret')
    z0 = RING_Z + 0.05
    p.cyl('CnTankMetal', (0.0, 0.0, z0 + 0.09), 'z', 1.9, 0.18, seg=40, bevel=0.01)
    zb, zt = z0 + 0.18, z0 + 1.22
    bot = [(x * S, y * S) for (x, y) in ((1.25, -0.6), (1.25, 0.6), (0.45, 1.2), (-1.15, 1.2), (-1.42, 0.8), (-1.42, -0.8), (-1.15, -1.2), (0.45, -1.2))]
    top = [(x * S, y * S) for (x, y) in ((0.98, -0.45), (0.98, 0.45), (0.35, 0.95), (-1.0, 0.95), (-1.2, 0.62), (-1.2, -0.62), (-1.0, -0.95), (0.35, -0.95))]
    p.loft('CnTankPaint', [[(x, y, zb) for (x, y) in bot], [(x, y, zb + 0.22) for (x, y) in bot], [(x, y, zt) for (x, y) in top]], bevel=0.03)
    # a aba de baixo (a saia da torre) e a junta no meio do teto
    p.loft('CnTankMetal', [[(x * 1.03, y * 1.03, zb - 0.02) for (x, y) in bot], [(x * 1.03, y * 1.03, zb + 0.06) for (x, y) in bot]], bevel=0.0)
    p.box('Dark', -1.1 * S, 0.85 * S, -0.015, 0.015, zt - 0.005, zt + 0.01, bevel=0.0)
    # o mantelete inclinado (o escudo da arma) com a luva por onde o tambor sai
    p.obox('CnTankPaint', (1.62, 0.0, GUN_Z + 0.02), (0.3, 1.6, 1.1), pitch=14, bevel=0.04)
    p.cyl('CnTankMetal', (1.79, 0.0, GUN_Z), 'x', 0.47, 0.22, seg=24, bevel=0.01)
    p.cyl('Dark', (1.89, 0.0, GUN_Z), 'x', 0.4, 0.03, seg=24, bevel=0.0)
    for s in (-1, 1):                                                            # os parafusos do mantelete
        for k in range(3):
            p.cyl('CnTankMetal', (1.79, s * 0.64, GUN_Z - 0.32 + k * 0.32), 'x', 0.045, 0.06, seg=6, bevel=0.0)
    # a caixa de munição (−Y) com a estrela vermelha, a tampa, as alças e a calha articulada até o berço
    ax0, ax1, ay0, ay1 = -0.95, 0.6, -2.2, -1.45
    p.box('CnTankPaint', ax0, ax1, ay0, ay1, zb + 0.06, zb + 0.95, bevel=0.03)
    p.box('CnTankMetal', ax0 - 0.04, ax1 + 0.04, ay0 - 0.04, ay1 + 0.02, zb + 0.95, zb + 1.03, bevel=0.01)
    star_wall(p, ((ax0 + ax1) / 2, ay0, zb + 0.5), '-y', 0.3, t=0.015)
    for xx in (ax0 + 0.2, ax1 - 0.2):
        p.box('Dark', xx - 0.12, xx + 0.12, ay0 - 0.06, ay0, zb + 0.82, zb + 0.86, bevel=0.0)
    chute = [(ax1 - 0.1, -1.82, zb + 1.03), (ax1 + 0.28, -1.74, zb + 1.12), (1.12, -1.35, zb + 1.05), (1.4, -0.9, zb + 0.8), (1.48, -0.48, GUN_Z + 0.02)]
    bar(p, 'CnTankMetal', chute, 0.28, 0.13)
    for q0, q1 in zip(chute[:-1], chute[1:]):                                    # os elos da calha
        c = (Vector(q0) + Vector(q1)) / 2
        d = Vector(q1) - Vector(q0)
        p.obox('Dark', tuple(c + Vector((0, 0, 0.07))), (0.05, 0.32, 0.06), yaw=math.degrees(math.atan2(d.y, d.x)), bevel=0.0)
    # o sensor eletro-óptico (à direita, em cima) com a lente e a pala
    p.box('CnTankMetal', -0.3, 0.55, 0.4, 1.0, zt, zt + 0.42, bevel=0.03)
    p.cyl('Glass', (0.57, 0.7, zt + 0.21), 'x', 0.13, 0.05, seg=16, bevel=0.0)
    p.cyl('Dark', (0.56, 0.7, zt + 0.21), 'x', 0.16, 0.04, seg=16, bevel=0.0)
    p.obox('CnTankMetal', (0.63, 0.7, zt + 0.44), (0.25, 0.62, 0.04), pitch=-12, bevel=0.0)
    # a escotilha de manutenção
    p.cyl('CnTankMetal', (-0.55, -0.4, zt + 0.03), 'z', 0.36, 0.06, seg=20, bevel=0.0)
    p.box('Dark', -0.6, -0.5, -0.68, -0.12, zt + 0.06, zt + 0.1, bevel=0.0)
    # a antena (atrás, à direita) com a base e a ponta
    p.cyl('CnTankMetal', (-1.15, 0.75, zt + 0.08), 'z', 0.09, 0.16, seg=10, bevel=0.0)
    p.cyl('Trim', (-1.15, 0.75, zt + 0.16 + 1.2), 'z', 0.02, 2.4, seg=6, bevel=0.0)
    p.cyl('Beacon', (-1.15, 0.75, zt + 2.6), 'z', 0.045, 0.08, seg=8, bevel=0.0)
    # o cesto atrás com a lona vermelha dobrada e presa (a cor do jogador do tanque chinês)
    bx0, bx1 = -2.4, -1.76
    for y in (-0.85, 0.85):
        for z in (zb + 0.25, zb + 0.75):
            p.box('Trim', bx0, bx1, y - 0.025, y + 0.025, z - 0.025, z + 0.025, bevel=0.0)
        p.box('Trim', bx0 - 0.025, bx0 + 0.025, y - 0.025, y + 0.025, zb + 0.2, zb + 0.8, bevel=0.0)
    p.box('Trim', bx0 - 0.025, bx0 + 0.025, -0.85, 0.85, zb + 0.75, zb + 0.8, bevel=0.0)
    p.box('CnTankMetal', bx0, bx1, -0.85, 0.85, zb + 0.2, zb + 0.25, bevel=0.0)
    p.obox('Player', ((bx0 + bx1) / 2 + 0.02, 0.0, zb + 0.48), (0.52, 1.5, 0.44), bevel=0.1)
    for y in (-0.45, 0.45):
        p.obox('Dark', ((bx0 + bx1) / 2 + 0.02, y, zb + 0.48), (0.54, 0.06, 0.46), bevel=0.0)
    return p.finish()


def barrels():
    """O tambor de 6 canos (origem no eixo, na face de trás do rotor; o eixo é o X local): o rotor com as nervuras, o eixo
    central, os 6 canos com o reforço da boca, as 2 braçadeiras e as bocas escuras."""
    p = BPart(PREFIX + '_Barrels')
    L = BARREL_LEN
    p.cyl('CnTankMetal', (0.17, 0.0, 0.0), 'x', 0.38, 0.34, seg=24, bevel=0.01)
    p.cyl('Dark', (0.35, 0.0, 0.0), 'x', 0.34, 0.03, seg=24, bevel=0.0)
    for k in range(6):                                                            # as nervuras do rotor (mostram o giro)
        a = 2 * math.pi * (k + 0.5) / 6
        cy_, cz_ = 0.37 * math.cos(a), 0.37 * math.sin(a)
        p.box('CnMetal', 0.02, 0.32, cy_ - 0.04, cy_ + 0.04, cz_ - 0.04, cz_ + 0.04, bevel=0.0)
    p.cyl('CnTankMetal', (L / 2, 0.0, 0.0), 'x', 0.06, L - 0.1, seg=10, bevel=0.0)
    for k in range(N_BARRELS):
        a = 2 * math.pi * k / N_BARRELS
        y, z = R_CLUSTER * math.cos(a), R_CLUSTER * math.sin(a)
        p.cyl('CnMetal', ((0.34 + L) / 2, y, z), 'x', R_BARREL, L - 0.34, seg=10, bevel=0.0)
        p.cyl('Dark', (L + 0.005, y, z), 'x', R_BARREL * 0.6, 0.02, seg=8, bevel=0.0)
        p.cyl('CnMetal', (L - 0.1, y, z), 'x', R_BARREL * 1.25, 0.16, seg=10, bevel=0.0)       # o reforço da boca
    for (x, w) in ((0.95, 0.1), (L - 0.32, 0.12)):
        p.cyl('CnTankMetal', (x, 0.0, 0.0), 'x', R_CLUSTER + R_BARREL + 0.05, w, seg=24, bevel=0.0)
    return p.finish()


def main():
    clear_scene()
    total = export(bunker(), OUT)
    clear_scene()
    total += export(turret(), OUT)
    clear_scene()
    total += export(bake_in_place(barrels(), BARREL_AT), OUT)   # (31/10) o cozimento com os canos no lugar deles
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -HX + 1.9, HX - 1.9, -HY + 1.9, HY - 1.9, Z_ROOF + 0.4, post='CnMetal'), OUT)
    print('MODEL total', total, 'tris')
    print('MODEL turret_pivot_m [0, 0]; muzzle_height_m', GUN_Z, '; muzzle a', round(BARREL_AT[0] + BARREL_LEN, 2),
          'm do pivo; barrels at_m', BARREL_AT, 'eixo +X local, rpm ~480 atirando')
    print('MODEL fire_points_m', [(-2.3, 1.6, Z_ROOF - 0.1), (-1.8, -2.1, Z_ROOF - 0.1), (2.3, 1.3, Z_ROOF - 0.12)])


if __name__ == '__main__':
    main()
