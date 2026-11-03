"""O Stinger Site da GLA (a defesa antiaérea), v4 (03/11/2026): O BURJ. A direção fechada em docs/elenco/ICONES_03_11.md (5ª rodada: a
caixa d'água do v3 caiu por ser objeto de fora do jogo; o pedido é o ícone da família, autêntico, que todo mundo lembra, sem copiar o
ZH). O v1, o v2 e o v3 ficam como estão.

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_gla_stinger_site_v4.py captures/game/models/gla_stinger_site_v4

A palavra (o que lê em 1 segundo): A TORRE REDONDA DE BARRO COM O NINHO NO ALTO. As torres redondas dos fortes de Omã (Nizwa, Jabrin, Al
Hazm): o tambor de barro afunilado (~5 m de parede, o pé mais largo, a casca irregular de reboco feito à mão), a cornija, a faixa verde
(a cor do jogador) pintada em volta, o parapeito com a COROA DE AMEIAS ARREDONDADAS; lá em cima, o ninho de sacos de areia com o Stinger
na mesa giratória (os 3 tubos e o atirador sentado: a torre que gira) e a rede de camuflagem verde esticada nas 4 varas por cima. Embaixo,
a porta baixa de madeira no batente saliente, a caixa de mata-cães (a sabbaba de Omã) em cima dela, as seteiras, as calhas de madeira
saindo do topo e a escada de mão encostada (do lado que a câmera vê). De cima: o círculo da torre com o anel de ameias, o ninho e a rede.

Lote 8 × 8 m, frente em +X. Alturas: a parede 5,0 m (o piso do ninho), o parapeito 5,8 m, as ameias até ~6,4 m, a rede ~7,2–7,9 m.

Peças (origem no chão, no centro do lote; Unreal X frente, Y direita, Z cima, metros):
  SM_GLA_StingerSite_Base: a torre, o ninho (sem o lançador), a rede, a escada, os caixotes (a bandeira sai no flags.json);
  SM_GLA_StingerSite_Turret (gira em volta de (0, 0), a mesma origem do modelo; a mesa no piso do ninho): a mesa, o pedestal, o garfo,
    os 3 tubos, a antena do IFF, o assento e o atirador; a boca no relatório;
  SM_GLA_StingerSite_Scaffold: o andaime da obra em madeira.
Materiais (slots da GLA): Plaster (o barro), Player (o verde), Wood, Cloth (os sacos, a roupa), Crate (os tubos e os caixotes), Net,
  Metal, Dark, Rust, Earth, Light, Trim, Hazard (só no andaime).
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, V, clear_scene, export, flag, scaffold  # noqa: E402,F401
from model_faction_kit import barrel, disc_ring, revolve  # noqa: E402,F401
from model_building_parts import WallFace, wbox, splits, tube, catenary, merlons  # noqa: E402,F401
from model_building_parts import gla_net_surface as net_surface  # noqa: E402
from model_gla_palace_v2 import (new_part, finish, _hash, fbm, rod, prism, bag, bag_ring, tire, earth_pad, blob, crate,  # noqa: E402,F401
                                 Frame, rope, rag, scrawl)

PREFIX = 'SM_GLA_StingerSite'
R0, R1, ZW = 2.62, 2.12, 5.0        # o raio no pé, no topo da parede, a altura da parede (o piso do ninho)
PAR_T, PAR_H = 0.36, 0.78           # o parapeito: espessura e altura acima do piso
TT = ZW + 0.05                      # a mesa giratória (o piso do ninho)
TRUNNION = (0.06, 0.0, TT + 0.98)   # o eixo de elevação
EL = 32.0                           # a elevação dos tubos em repouso
TUBE_R, TUBE_L = 0.075, 1.52
TUBES = [(-0.105, 0.0), (0.105, 0.0), (0.0, 0.18)]
POLES = [(45.0, 7.35), (135.0, 7.85), (225.0, 7.85), (315.0, 7.35)]   # as varas da rede: o ângulo e o topo


def r_wall(z):
    """O raio da parede na altura z (o pé largo, a escarpa que se fecha pra cima)."""
    t = max(0.0, min(1.0, z / ZW))
    return R0 - (R0 - R1) * t ** 0.78


def lumpy(a, z, s=0):
    """A casca feita à mão: o raio ondula de leve (o reboco passado em camadas)."""
    return 0.035 * (fbm(math.cos(a) * 2.2 + z * 0.9, math.sin(a) * 2.2 + z * 0.4, s) - 0.5) + 0.012 * math.sin(z * 9.0 + a * 3.0)


# ===================================================================================== a torre
def tower(p):
    """O tambor: a parede afunilada com a casca irregular (anéis com o ruído), a cornija, o parapeito de topo redondo e o piso do ninho
    (uma revolução só, fechada), a faixa verde, a coroa de ameias arredondadas."""
    seg = 56
    zs = [0.0, 0.25] + [0.25 + (ZW - 0.25) * k / 12 for k in range(1, 13)]
    rings = []
    for z in zs:
        r = r_wall(z) + (0.06 if z < 0.3 else 0.0)
        rings.append([(r * (1 + lumpy(2 * math.pi * k / seg, z)) * math.cos(2 * math.pi * k / seg),
                       r * (1 + lumpy(2 * math.pi * k / seg, z)) * math.sin(2 * math.pi * k / seg), z) for k in range(seg)])
    # a cornija, o parapeito com a coroa redonda, a face de dentro e o piso do ninho
    rp = R1 - 0.02
    prof = [(R1 + 0.12, ZW), (R1 + 0.14, ZW + 0.16), (rp, ZW + 0.2), (rp - 0.03, ZW + PAR_H - 0.12), (rp - 0.09, ZW + PAR_H - 0.02),
            (rp - PAR_T / 2, ZW + PAR_H + 0.02), (rp - PAR_T + 0.09, ZW + PAR_H - 0.02), (rp - PAR_T + 0.02, ZW + PAR_H - 0.12),
            (rp - PAR_T, TT), (0.9, TT), (0.02, TT)]
    for (r, z) in prof:
        rings.append([(r * math.cos(2 * math.pi * k / seg), r * math.sin(2 * math.pi * k / seg), z) for k in range(seg)])
    p.loft('Plaster#tower', rings, bevel=0.0, smooth=True, caps=(True, True))
    # a faixa verde pintada em volta (o cone fino colado na parede), logo abaixo da cornija
    for (za, zb) in ((ZW - 0.78, ZW - 0.38),):
        rr = []
        for (z, d) in ((za, 0.0), (zb, 0.0), (zb, -0.06), (za, -0.06)):
            ring = []
            for k in range(seg):
                a = 2 * math.pi * k / seg
                wob = 0.03 * math.sin(a * 7 + 1.3) if z == za else 0.0
                r = r_wall(z + wob) * (1 + lumpy(a, z)) + 0.018 + d
                ring.append((r * math.cos(a), r * math.sin(a), z + wob))
            rr.append(ring)
        rr.append(rr[0])
        p.loft('Player#band', rr, bevel=0.0, smooth=True, caps=(False, False))
    # a coroa de ameias arredondadas em cima do parapeito
    n = 12
    rm = rp - PAR_T / 2
    for k in range(n):
        a = 2 * math.pi * (k + 0.5) / n
        d = 0.27 / rm
        A = (rm * math.cos(a - d), rm * math.sin(a - d))
        B = (rm * math.cos(a + d), rm * math.sin(a + d))
        merlons(p, A, B, ZW + PAR_H - 0.06, n=1, w=0.5, t=PAR_T - 0.04, h=0.62, m='Plaster#merlon', bevel=0.1)
    # as calhas de madeira (as bicas do terraço) saindo logo abaixo do parapeito, e as seteiras
    for a_deg in (20.0, 150.0, 250.0):
        a = math.radians(a_deg)
        r = R1 + 0.1
        rod(p, 'Wood#spout', (r * 0.85 * math.cos(a), r * 0.85 * math.sin(a), ZW + 0.08), ((r + 0.5) * math.cos(a), (r + 0.5) * math.sin(a), ZW + 0.02), 0.07, seg=6)
    # as pontas das traves de palmeira (o toron) saindo da parede num anel, fora da caixa da porta e da escada
    for k in range(16):
        a_deg = k * 22.5 + 11.25
        if min(abs((a_deg - d + 180) % 360 - 180) for d in (0.0, -78.0)) < 16:
            continue
        a = math.radians(a_deg)
        z = 3.3 + 0.06 * math.sin(k * 2.3)
        r = r_wall(z) * (1 + lumpy(a, z))
        rod(p, 'Wood#toron', ((r - 0.2) * math.cos(a), (r - 0.2) * math.sin(a), z), ((r + 0.32) * math.cos(a), (r + 0.32) * math.sin(a), z - 0.02),
            0.065, seg=6)
    for (a_deg, z) in ((60, 2.5), (120, 2.6), (200, 2.4), (300, 2.5), (240, 4.0), (330, 4.0), (100, 4.0), (35, 4.05)):
        a = math.radians(a_deg)
        r = r_wall(z) * (1 + lumpy(a, z)) + 0.005
        p.obox('Dark#slit', (r * math.cos(a), r * math.sin(a), z), (0.06, 0.12, 0.5), yaw=a_deg, bevel=0.0)
        r2 = r + 0.04
        p.obox('Plaster#jamb', (r2 * math.cos(a), r2 * math.sin(a), z + 0.3), (0.1, 0.28, 0.08), yaw=a_deg, bevel=0.02)


def door(p):
    """A porta baixa de madeira no batente saliente (o bloco de barro com a quina gasta), a verga de tronco, as tábuas com os cravos, e
    em cima dela a caixa de mata-cães (a sabbaba) nas mísulas de madeira, com a fresta escura embaixo."""
    x = r_wall(0.0)
    F = WallFace('+x', x + 0.32, 0.0)
    for (a, b, c, d) in ((-0.88, -0.5, 0.0, 2.15), (0.5, 0.88, 0.0, 2.15), (-0.88, 0.88, 1.82, 2.35)):
        wbox(p, 'Plaster#frame', F, a, b, c, d, -0.62, 0.0, bevel=0.1)
    wbox(p, 'Dark', F, -0.5, 0.5, 0.0, 1.82, -0.6, -0.5, bevel=0.0)
    for k in range(5):
        ua = -0.48 + k * 0.192
        wbox(p, 'Wood#door', F, ua, ua + 0.18, 0.02, 1.8, -0.3, -0.24, bevel=0.012)
    for zz in (0.35, 1.4):
        wbox(p, 'Wood#door', F, -0.46, 0.46, zz - 0.07, zz + 0.07, -0.24, -0.2, bevel=0.0)
        for u in (-0.36, 0.0, 0.36):
            wbox(p, 'Metal#stud', F, u - 0.03, u + 0.03, zz - 0.03, zz + 0.03, -0.2, -0.17, bevel=0.0)
    wbox(p, 'Wood#lintel', F, -0.75, 0.75, 1.84, 2.02, -0.66, 0.08, bevel=0.03)
    # a sabbaba (a caixa saliente em cima da porta), nas 3 mísulas, com a fresta escura embaixo
    zb = 3.75
    xw = r_wall(zb)
    Fs = WallFace('+x', xw, 0.0)
    wbox(p, 'Plaster#box', Fs, -0.62, 0.62, zb, zb + 0.78, -0.2, 0.58, bevel=0.1)
    wbox(p, 'Plaster#box', Fs, -0.68, 0.68, zb + 0.76, zb + 0.9, -0.2, 0.64, bevel=0.03)
    wbox(p, 'Dark', Fs, -0.4, 0.4, zb - 0.01, zb + 0.01, 0.12, 0.45, bevel=0.0)
    for u in (-0.48, 0.0, 0.48):
        wbox(p, 'Wood#corbel', Fs, u - 0.07, u + 0.07, zb - 0.28, zb, -0.1, 0.5, bevel=0.0)
        wbox(p, 'Wood#corbel', Fs, u - 0.07, u + 0.07, zb - 0.55, zb - 0.28, -0.1, 0.22, bevel=0.0)
    wbox(p, 'Dark#slit', Fs, -0.05, 0.05, zb + 0.25, zb + 0.6, 0.58, 0.59, bevel=0.0)


def ladder(p):
    """A escada de mão de madeira encostada no lado −Y (o que a câmera vê), do chão até passar do parapeito."""
    y0, y1, zt = -3.62, -2.07, ZW + PAR_H + 0.3
    for x in (0.3, 0.86):
        rod(p, 'Wood#ladder', (x, y0, -0.05), (x - 0.04, y1, zt), 0.05, seg=6)
    n = int(zt / 0.33)
    for k in range(1, n):
        t = k / n
        rod(p, 'Wood#rung', (0.3 - 0.04 * t, y0 + (y1 - y0) * t, zt * t), (0.86 - 0.04 * t, y0 + (y1 - y0) * t, zt * t), 0.025, seg=5)
    for (x, y, z) in ((0.27, -2.12, ZW + 0.8), (0.83, -2.12, ZW + 0.8)):
        p.torus('Dark#rope', (x, y, z), 'y', 0.06, 0.016, seg=8, rseg=4)


def nest(p):
    """O ninho em cima: o anel de sacos de areia (2 fileiras) encostado no parapeito, aberto onde a escada chega; os sacos tapando 2
    vãos das ameias; os tubos de reserva e a caixa de munição; a lâmpada."""
    bag_ring(p, (0.0, 0.0), 1.5, TT, layers=2, gaps=[(250.0, 290.0)], bag_size=(0.6, 0.34, 0.24), seed=3, lean=0.03)
    rm = R1 - 0.02 - PAR_T / 2
    for a_deg in (0.0, 120.0):
        a = math.radians(a_deg)
        bag(p, (rm * math.cos(a), rm * math.sin(a), ZW + PAR_H + 0.1), (0.56, 0.34, 0.22), yaw=a_deg + 90)
    for k in range(2):
        a = math.radians(160 + k * 8)
        p.cyl('Crate#tube', (1.2 * math.cos(a), 1.2 * math.sin(a), TT + 0.55), (0.25 * math.cos(a + 1.5), 0.25 * math.sin(a + 1.5), 1.0),
              TUBE_R, 1.4, seg=12, bevel=0.0)
    crate(p, -0.95, 0.55, TT, size=(0.7, 0.42, 0.34), yaw=30)


def nets(p):
    """As 4 varas no parapeito (as de trás mais altas) e a rede de camuflagem esticada nelas, com a barriga, os buracos e os tufos; os
    estais das varas até o parapeito; a bandeira na vara de trás da esquerda."""
    rm = R1 - 0.02 - PAR_T / 2
    tops = []
    for (a_deg, h) in POLES:
        a = math.radians(a_deg)
        x, y = rm * math.cos(a), rm * math.sin(a)
        rod(p, 'Wood#pole', (x, y, ZW + 0.3), (x * 1.04, y * 1.04, h), 0.055, seg=6)
        tops.append((x * 1.04, y * 1.04, h - 0.08))
        # o estai pra fora até a cornija
        rope(p, (x * 1.04, y * 1.04, h - 0.1), (x * 1.32, y * 1.32, ZW + 0.12), sag=0.04, r=0.014, m='Dark#rope')
    c0, c1, c2, c3 = tops[2], tops[3], tops[0], tops[1]           # 225°, 315°, 45°, 135° (o sentido horário de cima)

    def fn(u, v):
        x = (1 - u) * (1 - v) * c0[0] + u * (1 - v) * c1[0] + u * v * c2[0] + (1 - u) * v * c3[0]
        y = (1 - u) * (1 - v) * c0[1] + u * (1 - v) * c1[1] + u * v * c2[1] + (1 - u) * v * c3[1]
        z = (1 - u) * (1 - v) * c0[2] + u * (1 - v) * c1[2] + u * v * c2[2] + (1 - u) * v * c3[2]
        k = 1.12                                       # a rede passa um pouco das varas (a borda solta cai)
        x, y = x * k, y * k
        edge = min(u, 1 - u, v, 1 - v)
        z -= 0.42 * math.sin(math.pi * u) * math.sin(math.pi * v) + 0.35 * max(0.0, 0.08 - edge) / 0.08
        return (x, y, z)
    net_surface(p, fn, 18, 18, seed=7, blotch=1.2, holes=0.33, light=0.64, ragged=0.3, thick=0.04)
    a = math.radians(POLES[1][0])
    x, y = rm * 1.04 * math.cos(a), rm * 1.04 * math.sin(a)
    p.cyl('Trim', (x, y, POLES[1][1] + 0.55), 'z', 0.03, 1.1, seg=6, bevel=0.0)
    flag(p, (x, y), POLES[1][1] + 1.05, 1.15, 0.72, m='Player', phase=0.8)


def foot(p):
    """O pé da torre: o chão batido redondo, os caixotes compridos de míssil (um aberto com os tubos), os tambores, o rabisco verde."""
    earth_pad(p, (0.0, 0.0), blob(0.0, 0.0, 3.85, 3.85, seed=6, amp=0.05), h=0.05)
    p.cyl('Plaster#foot', (0.0, 0.0, 0.12), 'z', R0 + 0.22, 0.3, seg=56, bevel=0.0, smooth=True)
    for (x, y, z, yaw) in ((-3.0, 1.2, 0.0, 70), (-3.05, 1.25, 0.42, 66), (-1.9, 2.75, 0.0, 128)):
        crate(p, x, y, z, size=(1.5, 0.46, 0.42), yaw=yaw)
        p.obox('Player#stencil', (x, y, z + 0.425), (0.48, 0.2, 0.01), yaw=yaw, bevel=0.0)
    crate(p, 2.9, 2.3, 0.0, size=(1.4, 0.58, 0.3), yaw=-35)
    for k in range(3):
        a = math.radians(-35)
        o = (k - 1) * 0.17
        p.cyl('Crate#tube', (2.9 - o * math.sin(a), 2.3 + o * math.cos(a), 0.38), (math.cos(a), math.sin(a), 0.0), TUBE_R, 1.3, seg=12, bevel=0.0)
    barrel(p, -2.6, -2.2, 0.0, r=0.28, h=0.86, m='Rust')
    barrel(p, -2.05, -2.75, 0.0, r=0.28, h=0.86, m='Metal')
    tire(p, (2.8, -2.6, 0.13), R=0.36, r=0.13)


def base():
    p = new_part(PREFIX + '_Base')
    for m in ('Player#band', 'Wood#pole', 'Dark#rope'):
        p.bm(m, 0.0)
    p.bm('Plaster#tower', 0.13)
    foot(p)
    tower(p)
    door(p)
    ladder(p)
    nest(p)
    nets(p)
    return finish(p)


# ===================================================================================== a torre que gira
def gunner(p, c, yaw, aim=(0.0, 0.0)):
    """O atirador sentado (só o volume, de roupa e lenço: nenhum rosto): o colete, a cabeça enrolada no lenço verde, os braços pras
    manoplas, as pernas, as botas."""
    F = Frame(p, c, yaw=yaw)
    hip = (0.0, 0.0, 0.48)
    sh = (0.04, 0.0, hip[2] + 0.5)
    F.box('Crate#vest', (0.02, 0.0, hip[2] + 0.27), (0.24, 0.36, 0.42), pitch=-8, bevel=0.06)
    F.cyl('Cloth#body', (0.0, 0.0, hip[2] + 0.22), 'z', 0.15, 0.5, seg=10, bevel=0.0, smooth=True)
    F.cyl('Cloth#neck', (0.05, 0.0, sh[2] + 0.08), 'z', 0.06, 0.12, seg=8, bevel=0.0)
    hc = F.pt(0.07, 0.0, sh[2] + 0.24)
    revolve(p, 'Player#scarf', (hc[0], hc[1]), [(0.01, hc[2] - 0.12), (0.1, hc[2] - 0.1), (0.115, hc[2]), (0.1, hc[2] + 0.08),
                                                (0.05, hc[2] + 0.12), (0.01, hc[2] + 0.125)], seg=12)
    F.box('Dark#goggles', (0.16, 0.0, sh[2] + 0.25), (0.04, 0.14, 0.05), bevel=0.0)
    for s in (-1, 1):
        a = (0.04, s * 0.19, sh[2] - 0.02)
        e = (0.26, s * 0.24, sh[2] - 0.2)
        w = (aim[0] + 0.45, s * 0.2, sh[2] - 0.05 + aim[1])
        F.rod('Cloth#arm', a, e, 0.055)
        F.rod('Cloth#arm', e, w, 0.045)
        k = (0.42, s * 0.11, hip[2] + 0.05)
        f = (0.48, s * 0.12, 0.08)
        F.rod('Cloth#leg', (0.0, s * 0.1, hip[2]), k, 0.075)
        F.rod('Cloth#leg', k, f, 0.06)
        F.box('Dark#boot', (0.55, s * 0.12, 0.06), (0.26, 0.11, 0.12), bevel=0.02)


def turret():
    """A torre (gira em (0, 0)): a mesa giratória, o pedestal, o garfo com os munhões, os 3 tubos do Stinger (o verde-oliva, as tampas
    escuras, a faixa verde), a antena do IFF em leque, o assento de trator e o atirador sentado nas manoplas."""
    p = new_part(PREFIX + '_Turret')
    p.cyl('Dark', (0.0, 0.0, TT + 0.035), 'z', 0.44, 0.07, seg=24, bevel=0.0)
    p.cyl('Metal', (0.0, 0.0, TT + 0.12), 'z', 0.37, 0.1, seg=24, bevel=0.0)
    p.cyl('Metal#post', (0.0, 0.0, (TT + TRUNNION[2]) / 2), 'z', 0.09, TRUNNION[2] - TT - 0.1, seg=12, bevel=0.0)
    for s in (-1, 1):
        p.prism_xz('Metal', [(-0.12, TRUNNION[2] - 0.35), (0.22, TRUNNION[2] - 0.35), (TRUNNION[0] + 0.09, TRUNNION[2] + 0.06),
                             (TRUNNION[0] - 0.09, TRUNNION[2] + 0.06)], s * 0.26, s * 0.31, bevel=0.0)
        p.cyl('Dark', (TRUNNION[0], s * 0.33, TRUNNION[2]), 'y', 0.06, 0.05, seg=12, bevel=0.0)
    p.box('Metal#yoke', -0.12, 0.22, -0.31, 0.31, TRUNNION[2] - 0.42, TRUNNION[2] - 0.34, bevel=0.0)
    e = math.radians(EL)
    d = (math.cos(e), 0.0, math.sin(e))
    up = (-math.sin(e), 0.0, math.cos(e))
    for t in (-0.4, 0.45):
        c = (TRUNNION[0] + d[0] * t, 0.0, TRUNNION[2] + d[2] * t)
        p.obox('Dark#clamp', c, (0.07, 0.42, 0.42), pitch=EL, bevel=0.0)
    for (y, z) in TUBES:
        c = (TRUNNION[0] + up[0] * z + d[0] * 0.25, y, TRUNNION[2] + up[2] * z + d[2] * 0.25)
        p.cyl('Crate', c, d, TUBE_R, TUBE_L, seg=16, bevel=0.0)
        f = (c[0] + d[0] * TUBE_L / 2, c[1], c[2] + d[2] * TUBE_L / 2)
        b = (c[0] - d[0] * TUBE_L / 2, c[1], c[2] - d[2] * TUBE_L / 2)
        p.cyl('Dark', f, d, TUBE_R + 0.02, 0.1, seg=16, bevel=0.0)
        p.cyl('Dark', b, d, TUBE_R + 0.015, 0.09, seg=16, bevel=0.0)
        q = (c[0] + d[0] * 0.3, c[1], c[2] + d[2] * 0.3)
        p.cyl('Player', q, d, TUBE_R + 0.01, 0.14, seg=16, bevel=0.0)
    ac = (TRUNNION[0] + d[0] * 0.35 + up[0] * 0.1, -0.36, TRUNNION[2] + d[2] * 0.35 + up[2] * 0.1)
    for k in range(5):
        t = math.radians(-60 + 30 * k)
        tip = (ac[0] + 0.3 * math.cos(t) * d[0] - 0.3 * math.sin(t) * up[0], ac[1] - 0.12, ac[2] + 0.3 * math.cos(t) * d[2] - 0.3 * math.sin(t) * up[2])
        rod(p, 'Dark#iff', ac, tip, 0.01, seg=4)
    tube(p, 'Dark#iff', [(ac[0] + 0.3 * math.cos(math.radians(a)) * d[0] - 0.3 * math.sin(math.radians(a)) * up[0], ac[1] - 0.12,
                          ac[2] + 0.3 * math.cos(math.radians(a)) * d[2] - 0.3 * math.sin(math.radians(a)) * up[2]) for a in range(-60, 61, 15)],
         0.01, seg=4, smooth=False)
    p.cyl('Metal#seatpost', (-0.55, 0.0, TT + 0.25), 'z', 0.04, 0.42, seg=8, bevel=0.0)
    p.cyl('Rust#seat', (-0.55, 0.0, TT + 0.48), 'z', 0.2, 0.05, seg=14, bevel=0.0)
    rod(p, 'Metal#arm', (-0.1, 0.0, TT + 0.35), (-0.55, 0.0, TT + 0.3), 0.035, seg=6)
    gunner(p, (-0.62, 0.0, TT), 0.0, aim=(0.2, 0.2))
    return finish(p)


def muzzle():
    e = math.radians(EL)
    d = (math.cos(e), 0.0, math.sin(e))
    up = (-math.sin(e), 0.0, math.cos(e))
    y, z = TUBES[2]
    c = (TRUNNION[0] + up[0] * z + d[0] * 0.25, y, TRUNNION[2] + up[2] * z + d[2] * 0.25)
    return (c[0] + d[0] * TUBE_L / 2, c[1], c[2] + d[2] * TUBE_L / 2)


def write_preview(out):
    import json
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]], [PREFIX + '_Turret', [0, 0, 0]]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1200, 1200]], ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1000, 1300]],
                      ['side_left', 'ortho', [0, -1, 0], [0, 0, 1], [1000, 1300]],
                      ['three_quarter', 'persp', 30.0, 215.0, [1200, 1400], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 30.0, 35.0, [1200, 1400], 30.0, 0.95]],
            'usa_at': [4.5, -8.0, 0.0], 'tank_views': True,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(out, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, indent=1)


def main():
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'gla_stinger_site_v4')
    os.makedirs(out, exist_ok=True)
    clear_scene()
    total = export(base(), out)
    clear_scene()
    total += export(turret(), out)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -R0 - 0.1, R0 + 0.1, -R0 - 0.1, R0 + 0.1, ZW + 1.3, post='Wood', plank='Crate',
                             stripe='Hazard'), out)
    write_preview(out)
    m = muzzle()
    print('MODEL total', total, 'tris')
    print('MODEL lote 8 x 8 m; turret_pivot_m (0, 0); a mesa no piso do ninho z', round(TT, 2), '; muzzle (x, y, z) =',
          tuple(round(v, 2) for v in m), '; muzzle_height_m', round(m[2], 2), '; eixo de elevacao x', TRUNNION[0], 'z',
          round(TRUNNION[2], 2), '; elevacao em repouso', EL)
    print('MODEL fire_points_m', [(0.0, 0.0, round(ZW + 0.6, 2)), (1.7, -1.7, 3.0), (-1.7, 1.7, 3.0)])


if __name__ == '__main__':
    main()
