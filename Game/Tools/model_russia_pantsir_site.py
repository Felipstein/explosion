"""O Pantsir (a defesa de base da Rússia) provisório, modelado no Blender por script (a facção nova, 01/11/2026: docs/FACCAO_RUSSIA.md
§3 e §5.1: "o Pantsir no reparo fixo, com o radar giratório; o radar redondo e os 2 canhões com os mísseis").

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- building C:/Users/luisf/explosion/Game/Tools/model_russia_pantsir_site.py C:/Users/luisf/explosion/captures/game/models/russia_pantsir_site

A planta: o módulo de combate do Pantsir-S1 (o 96K6) tirado do caminhão e montado num reparo fixo de concreto, como a defesa de ponto
das bases russas: a torre com o radar de tiro redondo na frente (o "rosto"), os 2 canhões duplos de 30 mm (os 2A38M) dos lados, os 2
blocos de 6 tubos de míssil por fora, e em cima, no eixo do giro, o radar de busca redondo que gira sozinho. A base: a laje octogonal, o
pedestal de aço com o anel de giro, o anel de sacos de areia em 3 fiadas com a entrada atrás, as caixas de munição.

A palavra (o que lê de cima): O PRATO REDONDO GIRANDO em cima dos canos e dos tubos, no círculo de sacos. Assinatura redonda (o radar e o
anel de sacos), e o "rosto" de canos apontando pra fora.

Peças (origem no chão, no centro do lote; coordenadas da Unreal, X frente, Y direita, Z cima, metros):
  SM_Russia_PantsirSite_Base: a laje, o pedestal com o anel de giro, os sacos, as caixas, o gerador;
  SM_Russia_PantsirSite_Turret: a torre (gira em volta de (0, 0); turret_pivot_m = [0, 0]); mesma origem do modelo; os canos em
    repouso apontam pra +X, na horizontal; a boca dos canos a MUZZLE_X do pivô, na altura MUZZLE_Z;
  SM_Russia_PantsirSite_Radar: o radar de busca redondo (gira em Z; origem no eixo, no topo da coluna da torre: at_m = RADAR_AT,
    no eixo do pivô, então não sai do lugar quando a torre gira; 30 rpm);
  SM_Russia_PantsirSite_Scaffold: o andaime da obra (tubos de aço, scaffold() do model_lib).
Materiais (os slots da Rússia): Concrete, Metal, Paint (o aço frio azulado da torre, o mesmo dos tanques), Red, White, Team (a lona
dobrada atrás da torre), Glass, Light, Dark, Trim, Hazard, Cloth (os sacos), Wood, Rubber, Roof (as caixas de munição verdes).
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, clear_scene, export, scaffold  # noqa: E402,F401
from model_faction_kit import disc_ring, revolve, sandbag_ring, star_points  # noqa: E402
from model_building_parts import WallFace, wbox, tube, cable, bake_in_place, hexa  # noqa: E402
from model_russia_parts import RuPart, ammo_crate, ru_star, stencil, tarp_stack, face_prism  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'russia_pantsir_site')
PREFIX = 'SM_Russia_PantsirSite'

HX, HY = 4.0, 4.0                   # o lote 8 × 8
Z0 = 0.14
PAD_Z = 0.42                        # o topo da laje octogonal
RING_Z = 1.55                       # o topo do anel de giro (a base da torre)
GUN_Y, GUN_Z = 1.22, 2.3            # os canhões: o eixo de cada um (±Y) e a altura do eixo
BARREL_L = 2.55
MUZZLE_X = 3.05                     # a boca dos canos (a partir do pivô)
COL_TOP = 3.42                      # o topo da coluna do radar (no eixo)
RADAR_AT = (0.0, 0.0, COL_TOP)


def base():
    """A laje octogonal chanfrada, o pedestal de aço com o anel de giro e os parafusos, o anel de sacos de areia (3 fiadas, a entrada
    atrás), os degraus, as caixas de munição, o gerador com o cabo, as luzes."""
    p = RuPart(PREFIX + '_Base')
    # a laje octogonal (a saia que entra no chão) e o friso
    oc = [(3.75 * math.cos(math.radians(22.5 + 45 * k)), 3.75 * math.sin(math.radians(22.5 + 45 * k))) for k in range(8)]
    p.loft('Concrete#pad', [[(x, y, -0.6) for (x, y) in oc], [(x, y, PAD_Z) for (x, y) in oc]], bevel=0.08)
    ic = [(2.25 * math.cos(math.radians(22.5 + 45 * k)), 2.25 * math.sin(math.radians(22.5 + 45 * k))) for k in range(8)]
    p.loft('Concrete#pad2', [[(x, y, PAD_Z - 0.02) for (x, y) in ic], [(x, y, PAD_Z + 0.16) for (x, y) in ic]], bevel=0.05)
    # o pedestal: o tronco de cone de aço, as nervuras, o anel de giro escuro com os parafusos
    revolve(p, 'Paint#ped', (0.0, 0.0), [(1.25, PAD_Z + 0.12), (1.25, PAD_Z + 0.3), (0.95, PAD_Z + 0.42), (0.85, RING_Z - 0.2),
                                         (1.05, RING_Z - 0.1), (1.05, RING_Z - 0.06)], seg=40, smooth=False)
    for k in range(8):
        a = 2 * math.pi * k / 8
        p.obox('Paint#rib', (0.92 * math.cos(a), 0.92 * math.sin(a), (PAD_Z + RING_Z) / 2 - 0.05), (0.32, 0.07, RING_Z - PAD_Z - 0.5),
               yaw=math.degrees(a), bevel=0.0)
    disc_ring(p, 'Dark#ring', (0.0, 0.0), 1.08, 0.5, RING_Z - 0.06, RING_Z, seg=40)
    for k in range(24):
        a = 2 * math.pi * k / 24
        p.cyl('Metal#bolt', (1.15 * math.cos(a), 1.15 * math.sin(a), PAD_Z + 0.33), 'z', 0.045, 0.06, seg=8, bevel=0.0)
    # o anel de sacos de areia em 3 fiadas, com a entrada atrás (−X), e o pé de sacos da entrada
    sandbag_ring(p, (0.0, 0.0), 3.12, z0=PAD_Z, layers=3, gap_deg=(150.0, 210.0), m='Cloth#bag', bag=(0.62, 0.36, 0.22))
    # as caixas de munição (verdes) encostadas nos sacos, atrás do pedestal; o gerador com o cabo até o pedestal
    for (x, y, yaw, z) in ((-1.6, -1.9, 30, PAD_Z), (-1.85, -1.55, 34, PAD_Z), (-1.62, -1.88, 28, PAD_Z + 0.425), (1.2, -2.35, -70, PAD_Z)):
        ammo_crate(p, x, y, z, yaw=yaw)
    gx, gy = -1.7, 1.9
    p.obox('Paint#gen', (gx, gy, PAD_Z + 0.45), (1.2, 0.75, 0.75), yaw=-35, bevel=0.05)
    p.obox('Dark', (gx + 0.36, gy - 0.25, PAD_Z + 0.5), (0.02, 0.45, 0.4), yaw=-35, bevel=0.0)
    p.cyl('Metal#gen', (gx - 0.2, gy + 0.15, PAD_Z + 1.0), 'z', 0.06, 0.4, seg=8, bevel=0.0)
    tube(p, 'Dark#cable', [(gx + 0.5, gy - 0.3, PAD_Z + 0.12), (-0.6, 0.9, PAD_Z + 0.12), (-0.9, 0.55, PAD_Z + 0.5)], 0.035, seg=6)
    # as luzes no friso da laje e a placa estêncil no pedestal
    for k in range(4):
        a = math.radians(45 + 90 * k)
        q = (2.2 * math.cos(a), 2.2 * math.sin(a), PAD_Z + 0.08)
        p.cyl('Dark#lamp', q, 'z', 0.12, 0.1, seg=10, bevel=0.0)
        p.cyl('Light', (q[0], q[1], q[2] + 0.055), 'z', 0.08, 0.02, seg=10, bevel=0.0)
    return p.finish()


def missile_pack(p, y, s):
    """O bloco de 6 tubos de míssil (2 colunas × 3 fileiras) no lado s da torre: os tubos, as tampas da frente com o aro, a cinta, o
    suporte; levemente levantado (o pacote do Pantsir)."""
    pitch = 8.0
    cx, cz = -0.05, GUN_Z + 0.45
    for col in range(2):
        for row in range(3):
            yy = y + s * (col * 0.27)
            zz = cz + row * 0.27
            c = Vector((cx, yy, zz))
            d = Vector((math.cos(math.radians(pitch)), 0.0, math.sin(math.radians(pitch))))
            p.cyl('Paint#tube', tuple(c), tuple(d), 0.12, 2.9, seg=16, bevel=0.0, smooth=True)
            p.cyl('Metal#cap', tuple(c + d * 1.47), tuple(d), 0.125, 0.06, seg=16, bevel=0.0)
            p.cyl('Red#cap', tuple(c + d * 1.5), tuple(d), 0.1, 0.02, seg=16, bevel=0.0)
            p.cyl('Metal#cap', tuple(c - d * 1.47), tuple(d), 0.125, 0.06, seg=16, bevel=0.0)
    for xx in (-0.9, 0.7):                                                          # as cintas
        zc = cz + 0.27 + xx * math.sin(math.radians(pitch))
        p.box('Metal#band', xx - 0.06, xx + 0.06, min(y, y + s * 0.27) - 0.16, max(y, y + s * 0.27) + 0.16, zc - 0.43, zc + 0.43, bevel=0.012)
    p.box('Paint#mount', -0.5, 0.5, min(y, y + s * 0.27) - 0.1, max(y, y + s * 0.27) + 0.1, GUN_Z + 0.16, cz - 0.1, bevel=0.03)


def gun(p, y):
    """O canhão duplo de 30 mm (o 2A38M) no lado: a caixa da culatra, os 2 canos um em cima do outro com as luvas e os quebra-chamas, a
    caixa de munição do lado de fora."""
    p.box('Paint#gun', -0.75, 0.75, y - 0.22, y + 0.22, GUN_Z - 0.28, GUN_Z + 0.28, bevel=0.04)
    p.box('Metal#gun', 0.6, 0.85, y - 0.18, y + 0.18, GUN_Z - 0.24, GUN_Z + 0.24, bevel=0.02)
    for dz in (-0.11, 0.11):
        z = GUN_Z + dz
        p.cyl('Metal#barrel', (0.85 + BARREL_L / 2, y, z), 'x', 0.05, BARREL_L, seg=12, bevel=0.0, smooth=True)
        p.cyl('Metal#barrel', (1.05, y, z), 'x', 0.08, 0.4, seg=12, bevel=0.0)
        p.cyl('Dark#muzzle', (MUZZLE_X - 0.1, y, z), 'x', 0.075, 0.2, seg=12, bevel=0.0)
    p.box('Metal#gun', 1.6, 1.7, y - 0.14, y + 0.14, GUN_Z - 0.2, GUN_Z + 0.2, bevel=0.0)             # a braçadeira dos canos


def turret():
    """A torre: a base giratória, o corpo blindado facetado (o aço frio azulado), o radar de tiro redondo na frente (o "rosto"), os 2
    canhões duplos e os 2 blocos de mísseis dos lados, o sensor óptico, a coluna do radar de busca no eixo, a cesta de trás com a lona da
    cor do jogador, a estrela contornada na lateral, as escotilhas, as antenas."""
    p = RuPart(PREFIX + '_Turret')
    p.cyl('Paint#tbase', (0.0, 0.0, RING_Z + 0.08), 'z', 1.15, 0.16, seg=40, bevel=0.03)
    # o corpo facetado (a frente em cunha, o topo inclinado pra trás)
    zb, zt = RING_Z + 0.16, 3.05
    body = [(-1.45, -0.82), (0.75, -0.82), (1.2, -0.45), (1.2, 0.45), (0.75, 0.82), (-1.45, 0.82)]
    top = [(-1.35, -0.7), (0.62, -0.7), (0.98, -0.38), (0.98, 0.38), (0.62, 0.7), (-1.35, 0.7)]
    p.loft('Paint#body', [[(x, y, zb) for (x, y) in body], [(x, y, zt - 0.2) for (x, y) in body], [(x, y, zt) for (x, y) in top]],
           bevel=0.05)
    # o radar de tiro redondo na frente (o prato plano com o aro e a cobertura)
    fc = Vector((1.22, 0.0, 2.38))
    p.cyl('Paint#frad', tuple(fc), 'x', 0.52, 0.12, seg=32, bevel=0.0)
    p.torus('Metal#frad', tuple(fc + Vector((0.07, 0, 0))), 'x', 0.52, 0.045, seg=32, rseg=6)
    p.cyl('Dark#frad', tuple(fc + Vector((0.07, 0, 0))), 'x', 0.46, 0.01, seg=32, bevel=0.0)
    for k in range(4):                                                              # a grade de elementos (o ritmo)
        z = fc.z - 0.3 + k * 0.2
        p.box('Metal#frad', fc.x + 0.07, fc.x + 0.09, -0.42, 0.42, z - 0.012, z + 0.012, bevel=0.0)
    # o sensor óptico (a caixa com as 2 lentes) em cima da frente, à direita
    p.box('Paint#eo', 0.3, 0.85, 0.25, 0.65, zt - 0.05, zt + 0.35, bevel=0.03)
    for dy in (0.35, 0.55):
        p.cyl('Glass', (0.86, dy, zt + 0.15), 'x', 0.07, 0.03, seg=12, bevel=0.0)
    # os canhões e os mísseis dos 2 lados, nos munhões
    for s in (-1, 1):
        y = s * GUN_Y
        p.box('Paint#trun', -0.5, 0.5, s * 0.82 - (0.0 if s > 0 else 0.2), s * 0.82 + (0.2 if s > 0 else 0.0), GUN_Z - 0.2, GUN_Z + 0.2, bevel=0.03)
        p.cyl('Metal#trun', (0.0, s * 0.95, GUN_Z), 'y', 0.16, 0.3, seg=16, bevel=0.0)
        gun(p, y)
        missile_pack(p, s * 1.62, s)
    # a coluna do radar de busca no eixo (sobe do teto até COL_TOP) e a base giratória dela
    p.cyl('Paint#col', (0.0, 0.0, (zt + COL_TOP) / 2), 'z', 0.2, COL_TOP - zt, seg=20, bevel=0.0)
    p.cyl('Dark#col', (0.0, 0.0, COL_TOP - 0.03), 'z', 0.26, 0.06, seg=20, bevel=0.0)
    # a cesta de trás com a lona dobrada da cor do jogador, as escotilhas, as antenas, a estrela contornada na lateral
    for s in (-1, 1):
        tube(p, 'Metal#rack', [(-1.45, s * 0.7, zb + 0.5), (-1.95, s * 0.7, zb + 0.5), (-1.95, s * 0.7, zb + 1.0)], 0.03, seg=6)
    tube(p, 'Metal#rack', [(-1.95, -0.7, zb + 1.0), (-1.95, 0.7, zb + 1.0)], 0.03, seg=6)
    p.box('Metal#rack', -1.95, -1.45, -0.72, 0.72, zb + 0.48, zb + 0.52, bevel=0.0)
    tarp_stack(p, -1.7, 0.0, zb + 0.52, 0.45, 1.3, 0.38, yaw=0.0, seed=4)
    p.cyl('Paint#hatch', (-0.6, -0.35, zt + 0.03), 'z', 0.3, 0.07, seg=20, bevel=0.0)
    p.box('Metal#hinge', -0.95, -0.85, -0.45, -0.25, zt, zt + 0.08, bevel=0.0)
    for (x, y, h) in ((-1.2, 0.5, 1.6), (-1.25, -0.55, 1.1)):
        p.cyl('Metal#whip', (x, y, zt + h / 2), 'z', 0.022, h, seg=6, bevel=0.0)
        p.cyl('Dark', (x, y, zt + 0.04), 'z', 0.06, 0.08, seg=8, bevel=0.0)
    for s in (-1, 1):
        F = WallFace('+y' if s > 0 else '-y', s * 0.82, 0.0)
        ru_star(p, F, -0.75, 2.62, 0.2, n=0.0, t=0.012, w=0.12)
    return p.finish()


def radar():
    """O radar de busca redondo (o prato plano inclinado pra trás com o aro, a grade de elementos, a forquilha e o mancal), com a origem no
    eixo, no pé do mancal. Olha pra +X em repouso."""
    p = RuPart(PREFIX + '_Radar')
    p.cyl('Metal#brg', (0.0, 0.0, 0.08), 'z', 0.24, 0.16, seg=20, bevel=0.0)
    p.box('Paint#yoke', -0.12, 0.12, -0.12, 0.12, 0.16, 0.5, bevel=0.02)
    tilt = math.radians(18.0)
    c = Vector((0.06, 0.0, 0.92))
    n = Vector((math.cos(tilt), 0.0, math.sin(tilt)))
    p.cyl('Paint#dish', tuple(c), tuple(n), 0.82, 0.1, seg=36, bevel=0.0)
    p.cyl('Metal#dish', tuple(c + n * 0.055), tuple(n), 0.84, 0.02, seg=36, bevel=0.0)
    p.cyl('Dark#dish', tuple(c + n * 0.068), tuple(n), 0.74, 0.01, seg=36, bevel=0.0)
    up = Vector((-math.sin(tilt), 0.0, math.cos(tilt)))
    for k in range(-3, 4):                                                          # os elementos em fileira (o ritmo do prato)
        q = c + n * 0.08 + up * (k * 0.2)
        w = math.sqrt(max(0.0, 0.7 ** 2 - (k * 0.2) ** 2))
        p.obox('White#elem', tuple(q), (0.02, 2 * w, 0.05), pitch=-math.degrees(tilt), bevel=0.0)
    for s in (-1, 1):                                                               # a forquilha
        tube(p, 'Paint#yoke', [(0.0, s * 0.1, 0.5), (0.0, s * 0.62, 0.62), (c.x - 0.1, s * 0.62, c.z)], 0.045, seg=8)
        p.cyl('Metal#brg', (c.x - 0.1, s * 0.66, c.z), 'y', 0.08, 0.1, seg=12, bevel=0.0)
    p.box('Paint#yoke', -0.4, -0.15, -0.2, 0.2, 0.45, 0.8, bevel=0.03)              # a caixa do transmissor (contrapeso)
    return p.finish()


def main():
    os.makedirs(OUT, exist_ok=True)
    clear_scene()
    total = export(base(), OUT)
    clear_scene()
    total += export(turret(), OUT)
    clear_scene()
    total += export(bake_in_place(radar(), RADAR_AT), OUT)        # o cozimento com o radar no lugar dele
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -HX + 1.9, HX - 1.9, -HY + 1.9, HY - 1.9, RING_Z + 1.6, post='Metal', plank='Wood',
                             stripe='Hazard'), OUT)
    print('MODEL total', total, 'tris')
    print('MODEL turret_pivot_m [0, 0]; muzzle_height_m', GUN_Z, '; muzzle a', MUZZLE_X, 'm do pivo (canos em Y +-', GUN_Y,
          '); radar at_m', RADAR_AT, 'rpm 30')
    print('MODEL fire_points_m', [(-1.6, -1.8, PAD_Z + 0.5), (1.5, 2.2, PAD_Z + 0.5), (-0.3, 0.0, 3.12)])


if __name__ == '__main__':
    main()
