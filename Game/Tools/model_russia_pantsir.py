"""O Pantsir-S1 da Rússia (o antiaéreo de canhão e míssil) provisório, modelado no Blender por script (01/11/2026; docs/FACCAO_RUSSIA.md §3:
"o radar redondo e os 2 canhões com os mísseis: o prato giratório e o 'rosto' de canos").

Rodar (Blender sem janela, só CPU; não usa a placa), pelo executor do detalhe:
  (model_run_detail.py) vehicle Game/Tools/model_russia_pantsir.py C:/Users/luisf/explosion/captures/game/models/russia_pantsir

A base é o Pantsir-S1 no KamAZ-6560 8×8 de verdade, na escala dos caminhões do jogo (~0,8: o de verdade tem 9,5 m): a cabine blindada do
KamAZ (as janelas pequenas com a moldura), a caixa do gerador atrás dela (as venezianas, as portas), o pedestal com os macacos de apoio
e o MÓDULO DE COMBATE que gira atrás: o corpo com o radar de tiro redondo na frente (o "rosto"), os 2 blocos de 6 mísseis (2 × 3) nos
lados e os 2 canhões gêmeos 2A38M por baixo deles; por cima, a gaiola de ripas com a lona e a rede da cor do jogador e, acima dela, o
RADAR DE BUSCA REDONDO que gira (a peça _Radar).
A palavra: o PRATO REDONDO GIRANDO e o rosto de canos. De cima: o caminhão comprido de 8 rodas, a cabine quadrada, o módulo largo atrás
com os 2 blocos de tubos e os canos finos pra frente, o prato oval por cima.

Peças (origem no chão, no centro do chassi, frente em +X):
  SM_Russia_Pantsir_Hull    o caminhão: cabine, chassi, a caixa do gerador, o pedestal, os macacos;
  SM_Russia_Pantsir_Wheels  8 malhas wheel_001..008 (o jogo gira cada uma pelo centro da caixa);
  SM_Russia_Pantsir_Turret  o módulo de combate (gira em Z em volta de (TX, 0)): o corpo, o radar de tiro, os mísseis, os canhões, a
                            gaiola;
  SM_Russia_Pantsir_Radar   o radar de busca (gira no próprio eixo, em cima do módulo): a ORIGEM no eixo (RADAR_AT; spin at_m, on_turret).
Slots: Paint, Metal, Team, Lens, Cloth, Rubber, Red, White, Dark, Light.
"""
import math
import os
import sys

from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import V, clear_scene, export, export_group  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_russia_vehicle_kit as rk  # noqa: E402
from model_russia_vehicle_kit import bounds, frame_box, solid, sweep_check, tris  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'russia_pantsir')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_Russia_Pantsir'

FRONT, REAR = 3.85, -3.8
WHEEL_R, WHEEL_W, WHEEL_Y = 0.5, 0.38, 1.02
AXLES_X = [2.9, 1.66, -1.55, -2.8]
FRAME_Z = (0.8, 1.02)
CAB_X1, CAB_D, CAB_HW, CAB_ZB, CAB_ZR = 3.72, 1.72, 1.2, 1.14, 2.42
BODY_HW = 1.2
GEN = (0.2, 1.9, 2.0)            # a caixa do gerador: de x, até x, o teto (baixa: os canos giram por cima)
DECK_Z = 1.32                    # o piso da plataforma de trás
TX = -1.62                       # o eixo do módulo
RING_Z = 1.95                    # o anel do módulo (no pedestal)
GUN_Z = 2.28                     # o eixo dos canhões
GUN_Y = 1.14
MUZZLE_X = 1.15
PACK_Y, PACK_Z = 1.14, 2.62      # os blocos de mísseis
RADAR_AT = (TX - 0.25, 0.0, 3.42)  # o eixo do radar de busca (o pé do prato)
CAGE = dict(x0=TX - 1.25, x1=TX + 1.05, hw=0.95, z=3.16)


# ---------------------------------------------------------------------------------- o caminhão
def hull():
    p = rk.new_part(PREFIX + '_Hull')
    # o chassi: as longarinas, as travessas, os eixos, os diferenciais, os feixes
    for y in (-0.47, 0.47):
        vp.rbox(p, 'Metal', ((REAR + 0.05 + FRONT - 0.2) / 2, y, sum(FRAME_Z) / 2), (FRONT - 0.2 - REAR - 0.05, 0.13, FRAME_Z[1] - FRAME_Z[0]),
                r=0.02, k=1)
    for x in (-3.5, -2.2, -0.8, 0.4, 1.0, 2.25, 3.4):
        p.box('Metal#vp', x - 0.04, x + 0.04, -0.54, 0.54, FRAME_Z[0] + 0.03, FRAME_Z[1] - 0.03, bevel=0.0)
    for x in AXLES_X:
        p.cyl('Dark#vp', (x, 0.0, WHEEL_R), 'y', 0.085, 2 * WHEEL_Y - WHEEL_W, seg=12, bevel=0.0)
        vp.lathe(p, 'Dark', (x, 0.0, WHEEL_R), 'x', [(0.0, -0.18), (0.12, -0.18), (0.18, -0.1), (0.18, 0.1), (0.12, 0.18), (0.0, 0.18)],
                 seg=16)
        for s in (-1, 1):
            for k in range(3):
                vp.rbox(p, 'Metal', (x, 0.47 * s, WHEEL_R + 0.14 + k * 0.03), (1.0 - k * 0.2, 0.09, 0.025), r=0.01, k=1)
    # a cabine blindada do KamAZ-6560 (o kit) e os para-lamas das 2 rodas da frente (o arco) e das de trás (a chapa)
    rk.kamaz_cab(p, CAB_X1, CAB_HW, CAB_ZB, CAB_ZR, depth=CAB_D, armored=True)
    for s in (-1, 1):
        y0, y1 = sorted(((WHEEL_Y - WHEEL_W / 2 - 0.04) * s, (WHEEL_Y + WHEEL_W / 2 + 0.06) * s))
        for x in AXLES_X[:2]:
            rk.arc_fender(p, 'Paint#vp', x, WHEEL_R, y0, y1, WHEEL_R + 0.1, a0=15, a1=165, t=0.03)
        vp.rbox(p, 'Paint#thin', ((AXLES_X[2] + AXLES_X[3]) / 2, (y0 + y1) / 2, WHEEL_R + 0.62), (2.5, y1 - y0, 0.03), r=0.03, k=2)
        for x in (AXLES_X[2] + 0.66, AXLES_X[3] - 0.66):
            vp.rbox(p, 'Paint#thin', (x, (y0 + y1) / 2, WHEEL_R + 0.48), (0.03, y1 - y0, 0.3), r=0.02, k=1)
        vp.mudflap(p, (AXLES_X[3] - 0.7, WHEEL_Y * s, WHEEL_R + 0.3), WHEEL_W + 0.08, 0.55, m='Rubber')
        vp.mudflap(p, (AXLES_X[1] - 0.68, WHEEL_Y * s, WHEEL_R + 0.3), WHEEL_W + 0.06, 0.45, m='Rubber')
    # os tanques e as caixas entre as rodas do meio (o combustível à esquerda, as baterias à direita) com os degraus
    vp.rbox(p, 'Paint', (0.05, -0.95, 0.82), (1.6, 0.42, 0.46), roll=90, r=0.12, k=3)
    for dx in (-0.5, 0.5):
        vp.rbox(p, 'Metal', (0.05 + dx, -0.95, 0.82), (0.05, 0.45, 0.49), roll=90, r=0.13, k=3)
    vp.rbox(p, 'Paint#plate', (0.05, 0.95, 0.85), (1.5, 0.38, 0.42), r=0.03, k=2)
    for dx in (-0.45, 0.0, 0.45):
        vp.rbox(p, 'Metal', (0.05 + dx, 1.15, 0.95), (0.05, 0.02, 0.07), r=0.008, k=1)
    # ---------------------------------------------------------------- a caixa do gerador atrás da cabine (as venezianas e as portas)
    g0, g1, gz = GEN
    vp.rbox(p, 'Paint#plate', ((g0 + g1) / 2, 0.0, (FRAME_Z[1] + gz) / 2), (g1 - g0, 2 * BODY_HW, gz - FRAME_Z[1]), r=0.06, k=3)
    vp.rbox(p, 'Paint#thin', ((g0 + g1) / 2, 0.0, gz + 0.02), (g1 - g0 + 0.04, 2 * BODY_HW + 0.04, 0.04), r=0.06, k=2)
    for s in (-1, 1):
        y = BODY_HW * s
        for (xa, xb) in ((g0 + 0.12, g0 + 0.8), (g0 + 0.92, g1 - 0.12)):
            p.box('Dark', xa - 0.012, xa + 0.012, min(y, y + 0.006 * s), max(y, y + 0.006 * s), FRAME_Z[1] + 0.1, gz - 0.1, bevel=0.0)
            p.box('Dark', xb - 0.012, xb + 0.012, min(y, y + 0.006 * s), max(y, y + 0.006 * s), FRAME_Z[1] + 0.1, gz - 0.1, bevel=0.0)
            vp.rbox(p, 'Metal', (xb - 0.12, (BODY_HW + 0.02) * s, (FRAME_Z[1] + gz) / 2), (0.12, 0.03, 0.04), r=0.01, k=1)
        # a veneziana grande do lado (o gerador respira)
        p.box('Dark', g0 + 0.95, g1 - 0.2, min(y, y + 0.008 * s), max(y, y + 0.008 * s), gz - 0.55, gz - 0.15, bevel=0.0)
        for k in range(6):
            z = gz - 0.52 + k * 0.065
            p.obox('Metal#vp', ((g0 + 0.95 + g1 - 0.2) / 2, (BODY_HW + 0.02) * s, z), (g1 - 0.2 - g0 - 0.95, 0.025, 0.035), roll=40 * s, bevel=0.0)
    # o escape do gerador (o cano curto saindo do lado esquerdo, baixo: os canos do módulo giram por cima da caixa), a grelha do teto e
    # a lona enrolada da cor do jogador no teto da cabine (fora do giro dos canos)
    vp.tube(p, 'Dark', vp.bent_bar([(g0 + 0.4, -BODY_HW + 0.1, gz - 0.35), (g0 + 0.4, -BODY_HW - 0.12, gz - 0.35),
                                    (g0 + 0.4, -BODY_HW - 0.12, gz - 0.6)], R=0.08, n=3), 0.07, seg=12)
    rk.grille_deck(p, g0 + 0.6, g1 - 0.15, -0.9, 0.25, gz + 0.04, 6, along='y')
    vp.tarp_roll(p, (CAB_X1 - CAB_D + 0.2, 0.0, CAB_ZR + 0.13), 'y', 1.5, 0.13, m='Team', straps=3, strap_m='Cloth', buckle_m='Metal')
    # ---------------------------------------------------------------- a plataforma de trás, o pedestal do módulo, os macacos
    vp.rbox(p, 'Paint#plate', ((REAR + g0) / 2, 0.0, DECK_Z - 0.1), (g0 - REAR, 2 * BODY_HW, 0.2), r=0.04, k=2)
    vp.lathe(p, 'Paint', (TX, 0.0, DECK_Z), 'z', [(0.0, 0.0), (1.2, 0.0), (1.2, 0.08), (1.05, 0.14), (1.0, RING_Z - DECK_Z - 0.04),
                                                 (1.05, RING_Z - DECK_Z - 0.006), (0.0, RING_Z - DECK_Z - 0.006)], seg=40)
    vp.lathe(p, 'Dark', (TX, 0.0, RING_Z - 0.035), 'z', [(0.98, 0.0), (1.08, 0.0), (1.08, 0.03), (0.98, 0.03)], seg=48, closed=True)
    for s in (-1, 1):
        for x in (REAR + 0.3, g0 - 0.25):
            # o macaco de apoio dobrado (o cilindro em pé na quina com a sapata)
            vp.lathe(p, 'Paint', (x, (BODY_HW - 0.05) * s, DECK_Z - 0.75), 'z', [(0.0, 0.0), (0.09, 0.0), (0.09, 0.62), (0.07, 0.65), (0.0, 0.65)],
                     seg=12)
            vp.lathe(p, 'Metal', (x, (BODY_HW - 0.05) * s, DECK_Z - 0.88), 'z', [(0.0, 0.0), (0.17, 0.0), (0.17, 0.04), (0.06, 0.07),
                                                                                (0.05, 0.14), (0.0, 0.14)], seg=14)
        # as caixas de ferramenta penduradas na plataforma e a escada
        vp.rbox(p, 'Paint#plate', (-3.1, (BODY_HW - 0.2) * s, DECK_Z - 0.32), (0.9, 0.34, 0.4), r=0.04, k=2)
    for k in range(4):
        vp.tube(p, 'Metal', [(REAR + 0.05, 0.55 + 0.0, 0.5 + k * 0.22), (REAR + 0.05, 0.95, 0.5 + k * 0.22)], 0.016, seg=6)
    for y in (0.55, 0.95):
        vp.tube(p, 'Metal', [(REAR + 0.05, y, 0.4), (REAR + 0.05, y, DECK_Z)], 0.02, seg=6)
    # as lanternas de trás, o para-choque
    for s in (-1, 1):
        vp.rbox(p, 'Dark', (REAR + 0.05, 0.9 * s, 0.86), (0.08, 0.3, 0.14), r=0.03, k=2)
        vp.rbox(p, 'Lens', (REAR + 0.005, 0.9 * s, 0.86), (0.012, 0.25, 0.1), r=0.03, k=2)
    vp.rbox(p, 'Metal', (REAR + 0.08, 0.0, 0.7), (0.12, 2.0, 0.12), r=0.03, k=2)
    return p.finish()


# ---------------------------------------------------------------------------------- o módulo de combate
def turret():
    p = rk.new_part(PREFIX + '_Turret')
    z0 = RING_Z + 0.02
    # o corpo: a caixa com a frente em cunha (o radar de tiro nela), a traseira com a porta
    pts = []
    for s in (-1, 1):
        pts += [(TX - 1.25, 0.82 * s, z0), (TX + 0.95, 0.82 * s, z0), (TX + 1.18, 0.6 * s, z0 + 0.25), (TX + 1.18, 0.6 * s, z0 + 0.8),
                (TX + 0.9, 0.78 * s, z0 + 1.02), (TX - 1.2, 0.8 * s, z0 + 1.02), (TX - 1.35, 0.7 * s, z0 + 0.85)]
    solid(p, 'Paint#shell', pts)
    vp.lathe(p, 'Paint', (TX, 0.0, RING_Z), 'z', [(0.0, 0.0), (1.0, 0.0), (1.0, 0.03), (0.96, 0.04), (0.0, 0.04)], seg=48)
    # o radar de tiro (o "rosto" redondo na frente: o tambor, a face escura, o aro)
    rx, rz = TX + 1.2, z0 + 0.52
    vp.lathe(p, 'Paint', (rx, 0.0, rz), 'x', [(0.0, 0.0), (0.46, 0.0), (0.48, 0.04), (0.48, 0.14), (0.45, 0.16), (0.0, 0.16)], seg=32)
    vp.lathe(p, 'Dark', (rx + 0.155, 0.0, rz), 'x', [(0.0, 0.0), (0.42, 0.0), (0.42, 0.012), (0.0, 0.02)], seg=32)
    vp.bolt_ring(p, 'Metal', (rx + 0.14, 0.0, rz), 'x', 0.455, 16, r=0.014, h=0.01)
    # a câmera óptica (a caixinha com o vidro em cima do radar)
    vp.rbox(p, 'Paint#plate', (TX + 1.0, 0.0, z0 + 1.1), (0.36, 0.36, 0.2), r=0.05, k=2)
    vp.rbox(p, 'Lens', (TX + 1.18, 0.0, z0 + 1.1), (0.012, 0.22, 0.1), r=0.03, k=2)
    # a porta de trás, as alças, a estrela contornada nos lados
    vp.rbox(p, 'Dark', (TX - 1.3, 0.0, z0 + 0.48), (0.02, 0.7, 0.62), pitch=-30, r=0.04, k=2)
    for s in (-1, 1):
        vp.grab_handle(p, 'Metal', (TX - 0.9, 0.82 * s, z0 + 0.85), (TX - 0.4, 0.82 * s, z0 + 0.85), (0, s, 0), height=0.06, r=0.014)
        rk.star_outlined(p, (TX - 0.75, 0.825 * s, z0 + 0.4), (0, s, 0), r=0.12)
    # ---------------------------------------------------------------- os blocos de mísseis (6 tubos: 2 × 3) e os canhões gêmeos
    for s in (-1, 1):
        y = PACK_Y * s
        # o braço que segura os 2 (a viga lateral que sai do corpo, o munhão de elevação)
        vp.rbox(p, 'Paint#plate', (TX + 0.1, (0.82 + 0.1) * s, (GUN_Z + PACK_Z) / 2), (0.7, 0.2, 0.78), r=0.05, k=2)
        p.cyl('Metal#vp', (TX + 0.1, (0.82 + 0.22) * s, (GUN_Z + PACK_Z) / 2), 'y', 0.12, 0.08, seg=16, bevel=0.0)
        # o bloco de mísseis: a caixa com os 6 tubos aparecendo na frente (as tampas), as cintas
        px0, px1 = TX - 1.05, TX + 1.3
        vp.rbox(p, 'Paint#plate', ((px0 + px1) / 2, y, PACK_Z), (px1 - px0, 0.66, 0.44), roll=0, r=0.06, k=3)
        for i in range(2):
            for j in range(3):
                yy = y + (j - 1) * 0.2
                zz = PACK_Z + (i - 0.5) * 0.2
                vp.lathe(p, 'Paint', (px1, yy, zz), 'x', [(0.0, 0.0), (0.09, 0.0), (0.09, 0.04), (0.075, 0.05), (0.0, 0.05)], seg=14)
                p.cyl('Metal#vp', (px0 - 0.01, yy, zz), 'x', 0.06, 0.03, seg=10, bevel=0.0)
        for x in (px0 + 0.45, px1 - 0.55):
            vp.rbox(p, 'Metal', (x, y, PACK_Z), (0.06, 0.7, 0.48), r=0.02, k=1)
        # o canhão gêmeo 2A38M (a caixa da culatra e os 2 canos, um em cima do outro, com a luva e o freio)
        vp.rbox(p, 'Paint#plate', (TX + 0.05, GUN_Y * s, GUN_Z), (1.0, 0.3, 0.3), r=0.05, k=3)
        vp.rbox(p, 'Metal', (TX - 0.5, GUN_Y * s, GUN_Z - 0.1), (0.3, 0.26, 0.16), r=0.04, k=2)              # a caixa de munição
        for dz in (-0.07, 0.07):
            st = [(TX + 0.5, 0.055), (TX + 0.85, 0.055), (TX + 0.9, 0.042), (MUZZLE_X - 0.25, 0.036), (MUZZLE_X - 0.23, 0.05),
                  (MUZZLE_X, 0.05)]
            vp.gun_barrel(p, st, GUN_Z + dz, y=GUN_Y * s, m='Paint', seg=14, bore=0.018, bore_depth=0.15, bore_m='Dark')
        vp.ring_band(p, 'Metal', TX + 1.6, TX + 1.66, 0.11, GUN_Z, y=GUN_Y * s, lip=0.005, seg=16)
    # ---------------------------------------------------------------- a gaiola por cima do corpo, a lona e a rede; o mastro do radar
    # passa no meio dela
    c = CAGE
    zt = z0 + 1.02
    posts = [(TX + 0.75, 0.7, zt), (TX + 0.75, -0.7, zt), (TX - 1.05, 0.72, zt), (TX - 1.05, -0.72, zt)]
    roof = rk.slat_cage(p, c['x0'], c['x1'], c['hw'], c['z'], c['z'], posts, visor=0.0, step=0.24)
    step = (c['x1'] - c['x0']) / max(2, int(round((c['x1'] - c['x0']) / 0.24)))
    rk.tarp_on_slats(p, c['x0'] - 0.04, TX - 0.55, -c['hw'] - 0.05, 0.6, roof, step, c['x0'], m='Team', seed=11.0,
                     drape=[('rear', 0.3), ('left', 0.22)])

    def zf(x, y):
        z = roof(x) + 0.035 + 0.06 * math.exp(-((x - TX - 0.4) ** 2 + (y - 0.4) ** 2) / 0.08)
        z -= 0.02 * math.sin(math.pi * (((x - c['x0']) / step) % 1.0))
        if y > c['hw']:
            z -= min(0.25, (y - c['hw']) * 1.4)
        return z
    rk.net_drape(p, TX - 0.4, TX + 0.95, -0.2, c['hw'] + 0.18, zf, step=0.22, r=0.017, jitter=0.06, seed=14.0)
    # o mastro do radar de busca (o pé fixo no corpo, o anel de giro em cima da gaiola)
    ax, ay, az = RADAR_AT
    vp.lathe(p, 'Paint', (ax, ay, zt), 'z', [(0.0, 0.0), (0.24, 0.0), (0.24, 0.05), (0.14, 0.1), (0.12, az - zt - 0.06),
                                            (0.18, az - zt - 0.04), (0.18, az - zt - 0.006), (0.0, az - zt - 0.006)], seg=18)
    return p.finish()


def radar():
    """O radar de busca redondo (o prato oval de face plana, deitado pra trás 20°, com o reforço de trás e a cabeça giratória), feito no
    lugar (RADAR_AT); o jogo gira em Z em volta dele."""
    p = rk.new_part(PREFIX + '_Radar')
    ax, ay, az = RADAR_AT
    vp.lathe(p, 'Metal', (ax, ay, az), 'z', [(0.0, 0.0), (0.17, 0.0), (0.17, 0.08), (0.13, 0.12), (0.0, 0.12)], seg=18)
    vp.rbox(p, 'Paint#plate', (ax - 0.05, ay, az + 0.22), (0.3, 0.34, 0.22), r=0.05, k=2)
    # o prato: o disco oval (1,4 × 0,82 m) inclinado, a face escura e a borda; 2 braços até a cabeça
    W, H, t = 1.4, 0.82, 0.11
    tilt = 20.0
    cz = az + 0.52
    ex, ey, ez = vp.ypr_frame(0.0, tilt, 0.0)        # ex = a normal da face (pra frente e pra cima)
    C = Vector((ax + 0.12, ay, cz))
    ring = []
    n = 36
    for k in range(n):
        a = 2 * math.pi * k / n
        ring.append((math.cos(a) * W / 2, math.sin(a) * H / 2))
    vp.prism(p, 'Paint#plate', ring, C - ex * t / 2, ey, ez, ex, t)
    vp.prism(p, 'Dark', [(u * 0.92, v * 0.9) for u, v in ring], C + ex * (t / 2 - 0.004), ey, ez, ex, 0.012)
    for u in (-0.45, 0.0, 0.45):                                     # as nervuras de trás
        vp.rbox(p, 'Paint#plate', C - ex * (t / 2 + 0.05) + ey * u, (0.08, 0.06, H * 0.85), pitch=tilt, r=0.02, k=1)
    for d in (-0.25, 0.25):
        vp.tube(p, 'Metal', [Vector((ax - 0.05, ay + d, az + 0.3)), C - ex * (t / 2 + 0.08) + ey * d - ez * 0.2], 0.035, seg=8)
    return p.finish()


def wheels():
    return rk.tyre_wheels('wheel_%03d', AXLES_X, WHEEL_Y, WHEEL_R, WHEEL_W, 0.31, style='mud')


def main():
    clear_scene()
    h, t, r = hull(), turret(), radar()
    ws = wheels()
    sweep_check([h], [t], (TX, 0.0), label='modulo')
    sweep_check([t], [r], (RADAR_AT[0], RADAR_AT[1]), label='radar')
    rk.keep_in_place_until_export([(r, RADAR_AT)])
    total = export(h, OUT) + export(t, OUT) + export(r, OUT)
    total += export_group(ws, OUT, PREFIX + '_Wheels')
    print('MODEL total', total, 'tris')
    import model_lib
    if model_lib.DETAIL.get('defer') is None:
        r.data.transform(Matrix.Translation(V(*RADAR_AT)))
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h, t, r] + ws)
    print('MODEL medidas: comprimento %.2f m (x %.2f..%.2f), largura %.2f m, altura %.2f m (o radar)' % (ax1 - ax0, ax0, ax1, ay1 - ay0, az1))
    print('MODEL turret_pivot_m', [TX, 0.0], 'muzzle_height_m', GUN_Z, 'bocas [x, y, z]',
          [[MUZZLE_X, GUN_Y * s, round(GUN_Z + dz, 3)] for s in (-1, 1) for dz in (-0.07, 0.07)])
    print('MODEL radar spin at_m', [round(v, 3) for v in RADAR_AT], 'on_turret true, rpm 40')
    print('MODEL misseis: as 12 bocas', [[round(TX + 1.35, 3), round(PACK_Y * s + (j - 1) * 0.2, 3), round(PACK_Z + (i - 0.5) * 0.2, 3)]
                                         for s in (-1, 1) for i in range(2) for j in range(3)])
    print('MODEL pneu: raio %.2f, largura %.2f (tyre_w_m); eixos x %s; o centro da roda em y +-%.2f' % (WHEEL_R, WHEEL_W, AXLES_X, WHEEL_Y))
    print('MODEL tris por peca', {h.name: tris(h), t.name: tris(t), r.name: tris(r), 'wheels': sum(tris(w) for w in ws)})
    rk.write_spec(OUT, [(PREFIX + '_Hull', (0, 0, 0)), (PREFIX + '_Wheels', (0, 0, 0)), (PREFIX + '_Turret', (0, 0, 0)),
                        (PREFIX + '_Radar', (RADAR_AT[0], -RADAR_AT[1], RADAR_AT[2]))], usa_at=(0, 6.0, 0))


main()
