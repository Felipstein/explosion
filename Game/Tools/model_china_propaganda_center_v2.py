"""O Propaganda Center da China, versão 2 (refeita depois da galeria de 01/11/2026), modelada no Blender por script.

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- building Game/Tools/model_china_propaganda_center_v2.py C:/Users/luisf/explosion/captures/game/models/china_propaganda_center_v2

O veredito da v1 (docs/APROVADOS.md, 01/11): REFAZER, sem comentário. A v1 (model_china_propaganda_center.py, guardada) era um prédio de
pagode retangular comprido com o telão na frente e a torre das cornetas num canto: de cima lia "um prédio com muitas coisas", e as
cornetas (a assinatura) eram pequenas e fora do centro (o erro do §3b de docs/CONCEITO_CONSTRUCOES.md).

A palavra (o que lê de cima em 1 segundo): A TORRE DE ALTO-FALANTES. No meio do prédio, a torre de transmissão de treliça sai do topo
de um SALÃO REDONDO de linguagem chinesa (o do Templo do Céu: o pódio redondo de 2 degraus com a balaustrada branca, as 16 colunas
vermelhas, o tambor de concreto oliva e os 2 telhados redondos de telha verde vidrada com a borda vermelha e as pontas levantadas), e
lá em cima o CACHO DE 16 CORNETAS GIGANTES brancas em 2 andares, apontando pra todos os lados: de cima, o leque de cornetas é uma
explosão de raios brancos sobre os círculos verdes do telhado (a assinatura redonda num mundo de retângulos, §3b regra 2) e ele GIRA
devagar (regra 3: a assinatura se mexe). Ao lado, o TELÃO DE PROPAGANDA (7,4 × 4 m, inclinado pra trás pra câmera de cima): o campo
vermelho, a moldura dourada, os 16 raios de sol e a estrela do ELP no meio (a do kit, aprovada em 01/11: a vermelha fixa, slot Red,
com a borda dourada e o 八一: docs/elenco/EMBLEMAS.md),
sobre 2 pilares de concreto, com as 3 BANDEIRAS VERMELHAS nos mastros atrás dele (as bandeiras com física do jogo: o pano sai pro
flags.json). Na escada da frente, os 2 postes de alto-falante da praça (a assinatura em miniatura). A base acompanha a forma: o salão
redondo pisa no pódio redondo; o telão e os mastros pisam nas próprias sapatas; nada de laje do lote.

O lote: 14 × 24 m (x −7..7, y −12..12, o mesmo do Units.json), frente em +X. O salão no centro HC = (−0,6; 3,4): o pódio r 6,4 m, o
beiral r 6,0 m; o telão na ala −Y. Alturas: o pódio 0,75 m, o beiral 3,85 m, o 1º telhado até 5,55 m, o 2º até 7,55 m, a treliça até
Z_PLAT = 10,9 m (a plataforma), as cornetas até ~13,8 m e a estrela da agulha a ~14,1 m (a baliza ~14,6 m: a v1 ia a 14,4 m); o telão
de 2,7 a 6,6 m, as bandeiras até 9,6 m.

Peças (origem no chão, no centro do lote; coordenadas da Unreal, X frente, Y direita, Z cima, metros):
  SM_China_PropagandaCenter_Base: tudo o que não se mexe;
  SM_China_PropagandaCenter_Speakers: o cacho de cornetas (a mesa giratória, o tambor, as 16 cornetas, a agulha com as estrelas e a
    baliza): spin em Z, origem no eixo, no piso da plataforma: at_m = SPK_AT (o Units.json tem o da v1, [-1.0, 7.4, 11.05]: muda);
  SM_China_PropagandaCenter_Scaffold: o andaime da obra (tubos de aço, scaffold() do model_lib).
Materiais (slots do model_faction_kit.py): CnConcrete, CnPlinth, CnRoof, CnMetal, CnGold, Player (o vermelho do jogador: colunas, viga do
beiral, bordas dos telhados, o telão, as bandeiras), Red (a estrela do ELP, fixa), Paint (o branco: as cornetas e a balaustrada de mármore), Dark, Glass, Light,
Beacon.
"""
import json
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, clear_scene, export, flag, scaffold  # noqa: E402,F401
from model_faction_kit import bayi_strokes, disc_ring, star_points  # noqa: E402
from model_building_parts import BPart, bake_in_place, round_prof, sweep  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'china_propaganda_center_v2')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_China_PropagandaCenter'

HX, HY = 7.0, 12.0                  # o lote 14 × 24
HC = (-0.6, 3.4)                    # o eixo do salão redondo e da torre
POD_R, POD_Z1, TER_R, TER_Z = 6.4, 0.38, 5.75, 0.75     # o pódio: o degrau de baixo e o terraço
COL_R, N_COL = 4.85, 16             # as colunas vermelhas
WALL_R, ZE1 = 4.25, 3.85            # o tambor e o beiral do 1º telhado
ROOF1 = {'re': 6.0, 'ze': ZE1, 'curl': 0.18, 'rt': 2.95, 'zt': 5.55, 'rw': 4.0}
DRUM2_R, ZE2 = 2.6, 6.4             # o tambor de cima e o beiral do 2º telhado
ROOF2 = {'re': 3.55, 'ze': ZE2, 'curl': 0.14, 'rt': 1.05, 'zt': 7.55, 'rw': 2.3}
CAP_Z = 7.85                        # o topo do capitel de concreto (o pé da treliça)
Z_PLAT = 10.9                       # o piso da plataforma das cornetas
SPK_AT = (HC[0], HC[1], Z_PLAT)
# o telão: o meio da borda de baixo da face, a largura (ao longo de Y), a altura, o z de baixo, a inclinação pra trás
SC, SW, SH, SZ, TILT = (1.15, -7.7), 7.4, 4.0, 2.7, 10.0
MAST_X, MAST_TOP = -0.45, 9.8       # os mastros das bandeiras, atrás do telão


def polar(r, a_deg, z=None, c=HC):
    a = math.radians(a_deg)
    x, y = c[0] + r * math.cos(a), c[1] + r * math.sin(a)
    return (x, y) if z is None else (x, y, z)


def hlathe(p, m, prof, seg=64, closed=False):
    """O sólido de revolução em volta do eixo do salão: prof = [(raio, z)...]."""
    vp.lathe(p, m, (HC[0], HC[1], 0.0), 'z', prof, seg=seg, closed=closed)


def arc_solid(p, m, prof, a0, a1, n, bevel=0.0):
    """O perfil [(r, z)...] (polígono fechado) varrido em volta do eixo do salão de a0 a a1 graus, com as tampas."""
    rings = []
    for j in range(n + 1):
        a = a0 + (a1 - a0) * j / n
        rings.append([polar(r, a, z) for (r, z) in prof])
    p.loft(m, rings, bevel=bevel)


def tube(p, m, a, b, r, seg=8):
    vp.tube(p, m if '#' in m else m + '#t', [a, b], r, seg=seg)


def catenary(p, a, b, sag, n=14, r=0.022):
    pts = []
    for k in range(n + 1):
        t = k / n
        pts.append(tuple(Vector(a).lerp(Vector(b), t) - Vector((0.0, 0.0, sag * 4 * t * (1 - t)))))
    vp.tube(p, 'Dark#cable', pts, r, seg=5)


# ------------------------------------------------------------------ a estrela do ELP num plano qualquer
def emblem(p, M, r, n0, t=0.03):
    """A estrela do ELP aprovada (01/11, a star_wall do model_faction_kit.py: a borda CnGold, a estrela Red fixa, o 八一 em CnGold por
    cima), num plano qualquer: as mesmas 3 camadas com as mesmas peças do kit (star_points, bayi_strokes), pelo mapeador M(u, v, n) (u à
    direita de quem olha, v pra cima, n pra fora), a partir de n0. Serve no telão inclinado, na viga curva e nas estrelas da agulha (a
    star_wall só faz parede reta, e nas faces +x e −y ela espelha o 八一: a Unreal é de mão esquerda)."""
    def lay(mat, pts, d0, d1):
        p.loft(mat, [[tuple(M(u, v, n0 + d0)) for (u, v) in pts], [tuple(M(u, v, n0 + d1)) for (u, v) in pts]], bevel=0.0)
    lay('CnGold', star_points(0.0, 0.0, r * 1.12, r_in=r * 1.12 * 0.40, rot_deg=90.0), 0.0, t)
    lay('Red', star_points(0.0, 0.0, r, rot_deg=90.0), t, t * 1.6)
    for q in bayi_strokes(r):
        lay('CnGold', q, t * 1.6, t * 2.0)


def radial_map(a_deg, r0, z0):
    """O mapeador da face de fora de um cilindro do salão no ângulo a (u à direita de quem olha de fora, v pra cima, n pra fora)."""
    a = math.radians(a_deg)
    out = Vector((math.cos(a), math.sin(a), 0.0))
    right = Vector((math.sin(a), -math.cos(a), 0.0))        # a Unreal é de mão esquerda: olhando pro eixo, a direita
    C = Vector((HC[0], HC[1], z0)) + out * r0
    return lambda u, v, n: C + right * u + Vector((0.0, 0.0, v)) + out * n


# ===================================================================================================== o salão redondo
_TILE = round_prof(0.17, 0.085, base=-0.03, n=7)


def roof_rz(R, u):
    """O perfil do telhado redondo (u = 0 no beiral, 1 no topo): côncavo (sobe devagar no beiral, forte perto do topo: o 举折) e a
    ponta do beiral levantada ('curl')."""
    r = R['re'] + (R['rt'] - R['re']) * u
    z = R['ze'] + R['curl'] * (1 - u) ** 3 + (R['zt'] - R['ze'] - R['curl'] * 0.0) * u ** 1.6
    return r, z


def round_roof(p, R, hips=8, hip_off=22.5, pitch=0.36):
    """O telhado redondo de telha verde: o sólido (a face de cima côncava, a beirada, o forro), a fáscia vermelha, as fileiras de
    telha-canal radiais com o remate no beiral, as 8 cumeeiras radiais com a ponta vermelha levantada no beiral."""
    us = [0.0, 0.05, 0.13, 0.25, 0.4, 0.57, 0.77, 1.0]
    top = [roof_rz(R, u) for u in us]
    re, ze, c = R['re'], R['ze'], R['curl']
    prof = top + [(R['rt'] - 0.4, R['zt']), (R['rt'] - 0.4, ze - 0.25), (R['rw'], ze - 0.25), (re - 0.25, ze + c - 0.3),
                  (re + 0.03, ze + c - 0.28), (re + 0.03, ze + c - 0.04)]
    hlathe(p, 'CnRoof#roof', prof, seg=96, closed=True)
    # a fáscia vermelha em volta do beiral (o círculo vermelho que lê de cima)
    hlathe(p, 'Player#fascia', [(re - 0.3, ze + c - 0.36), (re + 0.08, ze + c - 0.36), (re + 0.08, ze + c + 0.05), (re - 0.3, ze + c + 0.05)],
           seg=96, closed=True)
    # as fileiras de telha (do beiral até o topo) com a telha de remate redonda na ponta
    n = int(2 * math.pi * re / pitch)
    ut = [0.0, 0.1, 0.24, 0.42, 0.62, 0.82, 0.97]
    for k in range(n):
        a = 360.0 * (k + 0.5) / n
        if hips and min(abs(((a - hip_off) % (360.0 / hips)) - 0.0), abs(((a - hip_off) % (360.0 / hips)) - 360.0 / hips)) < 1.2:
            continue
        pts = []
        for u in ut:
            r, z = roof_rz(R, u)
            pts.append(polar(r, a, z))
        sweep(p, 'CnRoof#tile', pts, _TILE, smooth=True)
        P0, P1 = Vector(pts[0]), Vector(pts[1])
        d = (P1 - P0).normalized()
        h = (Vector((0.0, 0.0, 1.0)) - d * d.z).normalized()
        p.cyl('CnRoof#tile', tuple(P0 + h * 0.03 - d * 0.015), tuple(d), 0.1, 0.05, seg=12, bevel=0.0)
    # as cumeeiras radiais (o rolo redondo) e as pontas vermelhas levantadas no beiral (a "asa" do telhado chinês)
    for k in range(hips):
        a = hip_off + 360.0 * k / hips
        pts = []
        for u in us[:-1] + [0.97]:
            r, z = roof_rz(R, u)
            pts.append(polar(r, a, z))
        sweep(p, 'CnRoof#hip', pts, round_prof(0.28, 0.22, base=-0.05, n=7), smooth=True)
        x, y, z = polar(re, a, ze + c)
        dx, dy = math.cos(math.radians(a)), math.sin(math.radians(a))
        p.obox('Player#tip', (x + dx * 0.25, y + dy * 0.25, z + 0.2), (0.85, 0.24, 0.16), yaw=a, pitch=34, bevel=0.03)
        p.obox('Player#tip', (x + dx * 0.55, y + dy * 0.55, z + 0.48), (0.32, 0.18, 0.16), yaw=a, pitch=66, bevel=0.03)
        p.cyl('CnGold#orn', (x - dx * 0.35, y - dy * 0.35, z + 0.32), 'z', 0.09, 0.2, seg=10, r2=0.04, bevel=0.0)


def hall(p):
    # ---------------------------------------------------------------- o pódio redondo (2 degraus) e a escada da frente
    hlathe(p, 'CnPlinth#pod', [(3.5, -0.4), (POD_R, -0.4), (POD_R, POD_Z1 - 0.06), (POD_R - 0.06, POD_Z1), (3.5, POD_Z1)], closed=True, seg=96)
    hlathe(p, 'CnPlinth#ter', [(3.5, POD_Z1 - 0.05), (TER_R, POD_Z1 - 0.05), (TER_R, TER_Z - 0.05), (TER_R - 0.05, TER_Z), (3.5, TER_Z)],
           closed=True, seg=96)
    disc_ring(p, 'Dark#joint', HC, POD_R - 0.28, POD_R - 0.31, POD_Z1 - 0.004, POD_Z1 + 0.005, seg=96)
    disc_ring(p, 'Dark#joint', HC, 5.2, 5.17, TER_Z - 0.004, TER_Z + 0.005, seg=96)
    x0 = HC[0]
    for k in range(5):
        p.box('CnPlinth#step', x0 + 5.3, x0 + 6.95 - k * 0.32, HC[1] - 1.5, HC[1] + 1.5, -0.1, 0.15 * (k + 1), bevel=0.02)
    X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
    for s in (-1, 1):
        y0 = HC[1] + (1.5 if s > 0 else -1.85)
        vp.prism(p, 'CnPlinth#cheek', [(x0 + 5.2, -0.1), (x0 + 7.1, -0.1), (x0 + 7.1, 0.3), (x0 + 5.75, 1.08), (x0 + 5.2, 1.08)],
                 (0.0, y0, 0.0), X, Z, Y, 0.35, bevel=0.012)
    # a balaustrada de mármore branca na borda do terraço (o anel branco que lê de cima), aberta na escada
    gap = 16.0
    for (a0, a1) in ((gap, 360.0 - gap),):
        arc_solid(p, 'Paint#rail', [(TER_R - 0.2, 1.22), (TER_R - 0.08, 1.22), (TER_R - 0.08, 1.31), (TER_R - 0.2, 1.31)], a0, a1, 120)
        arc_solid(p, 'Paint#rail', [(TER_R - 0.19, TER_Z), (TER_R - 0.09, TER_Z), (TER_R - 0.09, TER_Z + 0.1), (TER_R - 0.19, TER_Z + 0.1)],
                  a0, a1, 120)
    n = int(2 * math.pi * (TER_R - 0.14) / 0.85)
    for k in range(n + 1):
        a = gap + (360.0 - 2 * gap) * k / n
        x, y = polar(TER_R - 0.14, a)
        p.obox('Paint#post', (x, y, TER_Z + 0.3), (0.15, 0.15, 0.6), yaw=a, bevel=0.0)
        vp.lathe(p, 'Paint#cap', (x, y, TER_Z + 0.6), 'z', [(0.0, 0.0), (0.1, 0.0), (0.1, 0.06), (0.07, 0.14), (0.0, 0.2)], seg=12)
    # ---------------------------------------------------------------- o tambor de concreto, as janelas, a porta vermelha com as tachas
    hlathe(p, 'CnConcrete#wall', [(0.0, TER_Z - 0.05), (WALL_R, TER_Z - 0.05), (WALL_R, ZE1 + 0.1), (0.0, ZE1 + 0.1)], seg=96)
    for k in range(1, 8):
        a = 45.0 * k
        M = radial_map(a, WALL_R, 0.0)
        for (u0, u1, v0, v1, n0, n1, m) in ((-0.62, 0.62, 1.0, 3.0, -0.05, 0.1, 'CnConcrete#frame'), (-0.48, 0.48, 1.12, 2.88, 0.08, 0.11, 'Dark'),
                                             (-0.03, 0.03, 1.12, 2.88, 0.1, 0.14, 'CnGold#mull'), (-0.48, 0.48, 1.75, 1.8, 0.1, 0.14, 'CnGold#mull'),
                                             (-0.48, 0.48, 2.3, 2.35, 0.1, 0.14, 'CnGold#mull')):
            p.loft(m, [[tuple(M(u, v, n0)) for (u, v) in ((u0, v0), (u1, v0), (u1, v1), (u0, v1))],
                       [tuple(M(u, v, n1)) for (u, v) in ((u0, v0), (u1, v0), (u1, v1), (u0, v1))]], bevel=0.0)
    xd = HC[0] + WALL_R
    yc = HC[1]
    p.box('CnConcrete#portal', xd - 0.3, xd + 0.22, yc - 1.25, yc + 1.25, TER_Z - 0.05, 3.25, bevel=0.04)
    p.box('Dark', xd + 0.2, xd + 0.24, yc - 0.98, yc + 0.98, TER_Z, 2.92, bevel=0.0)
    for s in (-1, 1):
        y0, y1 = sorted((yc + s * 0.02, yc + s * 0.92))
        p.box('Player#door', xd + 0.22, xd + 0.28, y0, y1, TER_Z + 0.02, 2.88, bevel=0.0)
        for i in range(5):
            for j in range(7):
                yy = yc + s * (0.16 + 0.15 * i)
                p.cyl('CnGold#stud', (xd + 0.29, yy, TER_Z + 0.35 + 0.31 * j), 'x', 0.04, 0.03, seg=8, bevel=0.0)
        vp.lathe(p, 'CnGold#ring', (xd + 0.28, yc + s * 0.1, 1.85), 'x', [(0.12, 0.0), (0.14, 0.0), (0.14, 0.04), (0.12, 0.04)], seg=16, closed=True)
    # ---------------------------------------------------------------- as 16 colunas vermelhas (a pedra da base, o fuste, o capitel)
    for k in range(N_COL):
        a = 360.0 / N_COL * (k + 0.5)
        x, y = polar(COL_R, a)
        vp.lathe(p, 'CnPlinth#base', (x, y, 0.0), 'z', [(0.0, TER_Z - 0.02), (0.34, TER_Z - 0.02), (0.34, 0.92), (0.3, 0.97), (0.0, 0.97)],
                 seg=20)
        vp.lathe(p, 'Player#col', (x, y, 0.0), 'z', [(0.0, 0.95), (0.22, 0.95), (0.2, 1.08), (0.2, 3.3), (0.0, 3.3)], seg=20)
        vp.lathe(p, 'CnGold#capital', (x, y, 0.0), 'z', [(0.0, 3.28), (0.25, 3.28), (0.25, 3.47), (0.0, 3.47)], seg=20)
    # a viga vermelha do beiral (a arquitrave em volta, sobre as colunas) e o friso dourado
    hlathe(p, 'Player#beam', [(WALL_R - 0.1, 3.45), (5.15, 3.45), (5.15, ZE1 + 0.02), (WALL_R - 0.1, ZE1 + 0.02)], seg=96, closed=True)
    hlathe(p, 'CnGold#frieze', [(5.1, 3.38), (5.18, 3.38), (5.18, 3.47), (5.1, 3.47)], seg=96, closed=True)
    # a placa da estrela do ELP na viga, por cima da porta, e as 2 lanternas vermelhas
    M = radial_map(0.0, 5.16, 3.6)
    vp.lathe(p, 'CnGold#plaque', (HC[0] + 5.05, HC[1], 3.6), 'x', [(0.0, 0.0), (0.55, 0.0), (0.55, 0.15), (0.51, 0.18), (0.0, 0.18)], seg=40)
    emblem(p, lambda u, v, n: M(u, v, n + 0.07), 0.42, 0.0)
    for s in (-1, 1):
        x, y = polar(5.0, 17.0 * s)
        lantern(p, x, y, 3.45)
    # ---------------------------------------------------------------- o 1º telhado, o tambor de cima com as janelas, o 2º telhado
    round_roof(p, ROOF1)
    hlathe(p, 'CnConcrete#drum2', [(0.0, ROOF1['zt'] - 0.3), (DRUM2_R, ROOF1['zt'] - 0.3), (DRUM2_R, ZE2 + 0.1), (0.0, ZE2 + 0.1)], seg=64)
    for k in range(12):
        a = 30.0 * k + 15.0
        x, y = polar(DRUM2_R + 0.04, a)
        p.obox('Player#pil', (x, y, (ROOF1['zt'] + ZE2) / 2), (0.12, 0.2, ZE2 - ROOF1['zt'] + 0.1), yaw=a, bevel=0.0)
        M = radial_map(a + 15.0, DRUM2_R, 0.0)
        p.loft('Dark', [[tuple(M(u, v, -0.05)) for (u, v) in ((-0.42, 5.75), (0.42, 5.75), (0.42, 6.2), (-0.42, 6.2))],
                        [tuple(M(u, v, 0.03)) for (u, v) in ((-0.42, 5.75), (0.42, 5.75), (0.42, 6.2), (-0.42, 6.2))]], bevel=0.0)
    hlathe(p, 'Player#beam2', [(DRUM2_R - 0.1, 6.2), (3.05, 6.2), (3.05, ZE2 + 0.02), (DRUM2_R - 0.1, ZE2 + 0.02)], seg=64, closed=True)
    round_roof(p, ROOF2, hips=8, hip_off=0.0)
    # o capitel de concreto (o pé da treliça) com a chapa de aço
    hlathe(p, 'CnConcrete#cap', [(0.0, 7.25), (1.38, 7.25), (1.38, CAP_Z - 0.12), (1.3, CAP_Z - 0.06), (0.0, CAP_Z - 0.06)], seg=48)
    hlathe(p, 'CnMetal#plate', [(0.0, CAP_Z - 0.08), (1.2, CAP_Z - 0.08), (1.2, CAP_Z), (0.0, CAP_Z)], seg=48)
    hlathe(p, 'CnGold#capband', [(1.36, 7.45), (1.42, 7.45), (1.42, 7.6), (1.36, 7.6)], seg=48, closed=True)


def lantern(p, x, y, z_hang, drop=0.45, r=0.3, h=0.52):
    """A lanterna vermelha pendurada: o cordão, as tampas douradas, o corpo de gomos e a borla."""
    zc = z_hang - drop - h / 2
    p.cyl('Dark', (x, y, z_hang - drop / 2), 'z', 0.012, drop, seg=6, bevel=0.0)
    p.cyl('CnGold', (x, y, zc + h / 2 + 0.03), 'z', r * 0.45, 0.07, seg=12, bevel=0.0)
    vp.lathe(p, 'Player#lantern', (x, y, 0.0), 'z', [(0.0, zc - h / 2), (r * 0.48, zc - h / 2), (r * 0.82, zc - h / 2 + 0.07), (r, zc - 0.07),
                                                      (r, zc + 0.07), (r * 0.82, zc + h / 2 - 0.07), (r * 0.48, zc + h / 2),
                                                      (0.0, zc + h / 2)], seg=16)
    p.cyl('CnGold', (x, y, zc - h / 2 - 0.03), 'z', r * 0.45, 0.07, seg=12, bevel=0.0)
    p.cyl('CnGold', (x, y, zc - h / 2 - 0.2), 'z', 0.05, 0.28, seg=8, r2=0.09, bevel=0.0)


# ===================================================================================================== a torre de transmissão
def lattice_mast(p, c, z0, z1, a0, a1, levels, m='CnMetal', leg=0.08, brace=0.04):
    """A torre treliçada quadrada (rádio): 4 pernas de a0 (meio-lado no pé) a a1 (no topo), os anéis e os X de cada face."""
    def corner(k, z):
        t = (z - z0) / (z1 - z0)
        a = a0 + (a1 - a0) * t
        sx, sy = ((-1, -1), (1, -1), (1, 1), (-1, 1))[k]
        return (c[0] + sx * a, c[1] + sy * a, z)
    for k in range(4):
        tube(p, m + '#leg', corner(k, z0), corner(k, z1), leg, seg=8)
    for lv in range(levels + 1):
        z = z0 + (z1 - z0) * lv / levels
        for k in range(4):
            tube(p, m + '#br', corner(k, z), corner((k + 1) % 4, z), brace * 1.2, seg=6)
    for lv in range(levels):
        za, zb = z0 + (z1 - z0) * lv / levels, z0 + (z1 - z0) * (lv + 1) / levels
        for k in range(4):
            tube(p, m + '#br', corner(k, za), corner((k + 1) % 4, zb), brace, seg=6)
            tube(p, m + '#br', corner((k + 1) % 4, za), corner(k, zb), brace, seg=6)


def tower(p):
    """A treliça do capitel até a plataforma, a plataforma redonda com a borda vermelha, a escada de marinheiro e os cabos."""
    lattice_mast(p, HC, CAP_Z - 0.02, Z_PLAT - 0.12, 0.85, 0.42, 4)
    hlathe(p, 'CnMetal#plat', [(0.0, Z_PLAT - 0.14), (1.05, Z_PLAT - 0.14), (1.05, Z_PLAT), (0.0, Z_PLAT)], seg=40)
    hlathe(p, 'Player#rim', [(1.02, Z_PLAT - 0.17), (1.1, Z_PLAT - 0.17), (1.1, Z_PLAT + 0.03), (1.02, Z_PLAT + 0.03)], seg=40, closed=True)
    for k in range(4):                                                     # as mãos-francesas da plataforma
        sx, sy = ((1, 1), (1, -1), (-1, 1), (-1, -1))[k]
        tube(p, 'CnMetal#br', (HC[0] + sx * 0.47, HC[1] + sy * 0.47, Z_PLAT - 0.9), (HC[0] + sx * 0.72, HC[1] + sy * 0.72, Z_PLAT - 0.14), 0.04)
    # a escada de marinheiro por dentro da treliça (o lado de trás)
    for s in (-1, 1):
        tube(p, 'CnMetal#lad', (HC[0] - 0.2, HC[1] + s * 0.2, CAP_Z), (HC[0] - 0.2, HC[1] + s * 0.2, Z_PLAT - 0.14), 0.025, seg=6)
    for k in range(int((Z_PLAT - CAP_Z) / 0.3)):
        z = CAP_Z + 0.2 + k * 0.3
        tube(p, 'CnMetal#lad', (HC[0] - 0.2, HC[1] - 0.2, z), (HC[0] - 0.2, HC[1] + 0.2, z), 0.02, seg=6)
    # o cabo grosso de energia descendo pela treliça até o capitel
    vp.tube(p, 'Dark#cable', [(HC[0] + 0.15, HC[1] - 0.1, Z_PLAT - 0.14), (HC[0] + 0.15, HC[1] - 0.1, CAP_Z)], 0.05, seg=8)


# ===================================================================================================== o telão e as bandeiras
def screen(p):
    t = math.radians(TILT)
    eu = Vector((0.0, -1.0, 0.0))                           # a direita de quem olha o telão (de +X)
    ev = Vector((-math.sin(t), 0.0, math.cos(t)))
    en = Vector((math.cos(t), 0.0, math.sin(t)))
    O = Vector((SC[0], SC[1], SZ))

    def S(u, v, n=0.0):
        return O + eu * u + ev * v + en * n

    def rect(m, u0, u1, v0, v1, n0, n1):
        q = ((u0, v0), (u1, v0), (u1, v1), (u0, v1))
        p.loft(m, [[tuple(S(u, v, n0)) for (u, v) in q], [tuple(S(u, v, n1)) for (u, v) in q]], bevel=0.0)
    W2 = SW / 2
    rect('CnMetal#scr', -W2 - 0.25, W2 + 0.25, -0.25, SH + 0.22, -0.4, 0.0)            # a caixa do telão
    bw = 0.22
    for (u0, u1, v0, v1) in ((-W2, W2, 0.0, bw), (-W2, W2, SH - bw, SH), (-W2, -W2 + bw, 0.0, SH), (W2 - bw, W2, 0.0, SH)):
        rect('CnGold#frame', u0, u1, v0, v1, 0.0, 0.08)
    rect('Player#field', -W2 + bw, W2 - bw, bw, SH - bw, 0.0, 0.04)
    # os 16 raios de sol dourados (a "explosão" dos cartazes) e a estrela do ELP no meio
    vc = SH * 0.5
    hw, hh = W2 - bw - 0.1, SH / 2 - bw - 0.1
    for k in range(16):
        th = math.radians(11.25 + 22.5 * k)
        c_, s_ = math.cos(th), math.sin(th)
        d = min(hw / abs(c_) if abs(c_) > 1e-6 else 1e9, hh / abs(s_) if abs(s_) > 1e-6 else 1e9)
        r0 = 1.55
        r1 = d if k % 2 == 0 else r0 + 0.6 * (d - r0)
        if r1 <= r0 + 0.2:
            continue
        half = math.radians(3.2)
        pts = []
        for (r, sg) in ((r0, -1), (r1, -1), (r1, 1), (r0, 1)):
            a = th + sg * half * (0.35 if r == r0 else 1.0)
            pts.append((r * math.cos(a), vc + r * math.sin(a)))
        p.loft('CnGold#ray', [[tuple(S(u, v, 0.04)) for (u, v) in pts], [tuple(S(u, v, 0.06)) for (u, v) in pts]], bevel=0.0)
    emblem(p, lambda u, v, n: S(u, vc + v, n), 1.3, 0.04, t=0.04)
    # os 2 pilares de concreto (as sapatas no chão: nada de laje) e as cintas que seguram a caixa
    for u in (-2.6, 2.6):
        yc = SC[1] - u
        p.loft('CnConcrete#pylon', [[(-0.05, yc - 0.6, -0.3), (1.35, yc - 0.6, -0.3), (1.35, yc + 0.6, -0.3), (-0.05, yc + 0.6, -0.3)],
                                    [(0.15, yc - 0.45, SZ + 0.35), (1.0, yc - 0.45, SZ + 0.35), (1.0, yc + 0.45, SZ + 0.35),
                                     (0.15, yc + 0.45, SZ + 0.35)]], bevel=0.06)
        p.box('CnPlinth#pylon', -0.12, 1.42, yc - 0.67, yc + 0.67, -0.3, 0.45, bevel=0.04)
    # os 3 refletores nos braços em cima, virados pro telão
    for u in (-2.6, 0.0, 2.6):
        a, b = S(u, SH + 0.1, -0.2), S(u, SH + 0.55, 1.15)
        tube(p, 'CnMetal', tuple(a), tuple(b), 0.05)
        p.obox('Dark#lamp', tuple(b + Vector((0, 0, 0.05))), (0.36, 0.5, 0.28), pitch=-50, bevel=0.0)
        p.obox('Light', tuple(b + Vector((-0.1, 0, -0.08))), (0.04, 0.4, 0.2), pitch=40, bevel=0.0)
    # os 3 mastros atrás (as sapatas redondas), a treliça que segura a caixa e as bandeiras vermelhas no alto
    poles = []
    for k, u in enumerate((-2.6, 0.0, 2.6)):
        x, y = MAST_X, SC[1] - u
        poles.append((x, y))
        vp.lathe(p, 'CnPlinth#foot', (x, y, 0.0), 'z', [(0.0, -0.3), (0.42, -0.3), (0.42, 0.3), (0.36, 0.36), (0.0, 0.36)], seg=20)
        p.cyl('CnMetal#mast', (x, y, (0.3 + MAST_TOP) / 2), 'z', 0.09, MAST_TOP - 0.3, seg=12, bevel=0.0)
        vp.lathe(p, 'CnGold#finial', (x, y, MAST_TOP), 'z', [(0.0, 0.0), (0.12, 0.0), (0.12, 0.06), (0.06, 0.12), (0.0, 0.42)], seg=12)
        flag(p, (x - 0.1, y), MAST_TOP - 0.2, 2.3, 1.5, m='Player', trail=-1.0, phase=k * 1.7)
    for z in (SZ + 0.6, SZ + 2.6):
        tube(p, 'CnMetal#tr', (MAST_X, poles[0][1], z), (MAST_X, poles[2][1], z), 0.06)
        # as escoras de cada mastro até as costas da caixa (o plano n = −0,4 do telão, na altura z)
        v = (z - SZ + 0.4 * math.sin(t)) / math.cos(t)
        x_back = SC[0] - v * math.sin(t) - 0.4 * math.cos(t)
        for (x, y) in poles:
            tube(p, 'CnMetal#tr', (x, y, z), (x_back + 0.03, y, z), 0.05)
    for i in range(2):
        (xa, ya), (xb, yb) = poles[i], poles[i + 1]
        tube(p, 'CnMetal#tr', (xa, ya, SZ + 0.6), (xb, yb, SZ + 2.6), 0.04)
        tube(p, 'CnMetal#tr', (xa, ya, SZ + 2.6), (xb, yb, SZ + 0.6), 0.04)
    return poles


def speaker_pole(p, x, y, h=5.0):
    """O poste de alto-falante da praça (a assinatura em miniatura): a sapata redonda, o poste, as 3 cornetas brancas, a luminária."""
    vp.lathe(p, 'CnPlinth#foot', (x, y, 0.0), 'z', [(0.0, -0.3), (0.38, -0.3), (0.38, 0.4), (0.32, 0.46), (0.0, 0.46)], seg=20)
    p.cyl('CnMetal#pole', (x, y, (0.46 + h) / 2), 'z', 0.085, h - 0.46, seg=10, bevel=0.0)
    p.cyl('CnMetal#pole', (x, y, h + 0.1), 'z', 0.16, 0.2, seg=12, bevel=0.0)
    for yaw in (0.0, 120.0, 240.0):
        a = math.radians(yaw)
        d = Vector((math.cos(a) * math.cos(math.radians(-12)), math.sin(a) * math.cos(math.radians(-12)), math.sin(math.radians(-12))))
        horn(p, Vector((x, y, h - 0.15)) + d * 0.1, d, 0.07, 0.3, 0.8)
    p.box('Dark#lamp', x - 0.75, x - 0.45, y - 0.12, y + 0.12, h - 0.85, h - 0.65, bevel=0.0)
    p.box('Light', x - 0.72, x - 0.48, y - 0.09, y + 0.09, h - 0.88, h - 0.85, bevel=0.0)
    tube(p, 'CnMetal', (x, y, h - 0.75), (x - 0.5, y, h - 0.75), 0.03)


# ===================================================================================================== as cornetas (giram)
def horn(p, base, d, r0, rh, L, m='Paint#horn'):
    """A corneta de lata (a boca alargada exponencial, o lábio e o fundo cônico escuro): sólido fechado (polo atrás e polo no fundo
    da boca), mais o disco escuro no fundo e o aro dourado perto da boca."""
    k = math.log(rh / r0)
    prof = [(0.0, 0.0)] + [(r0 * math.exp(k * t ** 1.6), L * t) for t in (0.0, 0.2, 0.4, 0.6, 0.75, 0.87, 0.95, 1.0)]
    prof += [(rh - 0.035 - 0.02 * rh, L), (rh * 0.6, L - 0.17 * rh / 0.6), (0.0, L - 0.32 * rh / 0.6)]
    vp.lathe(p, m, tuple(base), tuple(d), prof, seg=24)
    D = Vector(d).normalized()
    p.cyl('Dark#mouth', tuple(Vector(base) + D * (L - 0.2 * rh / 0.6)), tuple(D), rh * 0.66, 0.03, seg=20, bevel=0.0)
    p.cyl('CnGold#horn', tuple(Vector(base) + D * (L * 0.82)), tuple(D), r0 * math.exp(k * 0.82 ** 1.6) + 0.025, 0.07, seg=20, bevel=0.0)


def speakers():
    """O cacho: a mesa giratória, o tambor com a faixa vermelha, 2 andares de 8 cornetas brancas (a de baixo maior e inclinada pra
    baixo, a de cima desencontrada meio passo), o motor de cada uma e o braço até o tambor, a agulha dourada com as 2 estrelas do ELP
    cruzadas e a baliza. Feito na origem (o eixo, no piso da plataforma); gira em Z."""
    p = BPart(PREFIX + '_Speakers')
    vp.lathe(p, 'CnMetal#tt', (0.0, 0.0, 0.0), 'z', [(0.0, 0.0), (0.82, 0.0), (0.82, 0.14), (0.76, 0.2), (0.0, 0.2)], seg=32)
    vp.lathe(p, 'CnMetal#drum', (0.0, 0.0, 0.0), 'z', [(0.0, 0.18), (0.44, 0.18), (0.44, 2.3), (0.38, 2.38), (0.0, 2.38)], seg=24)
    vp.lathe(p, 'Player#band', (0.0, 0.0, 0.0), 'z', [(0.42, 1.1), (0.47, 1.1), (0.47, 1.36), (0.42, 1.36)], seg=24, closed=True)
    vp.lathe(p, 'CnGold#band', (0.0, 0.0, 0.0), 'z', [(0.36, 2.3), (0.5, 2.3), (0.5, 2.42), (0.36, 2.42)], seg=24, closed=True)
    for (z, n, rh, L, tilt, yaw0) in ((0.72, 8, 0.72, 2.5, -14.0, 0.0), (1.78, 8, 0.58, 2.05, -3.0, 22.5)):
        for k in range(n):
            a = math.radians(yaw0 + 360.0 * k / n)
            d = Vector((math.cos(a) * math.cos(math.radians(tilt)), math.sin(a) * math.cos(math.radians(tilt)), math.sin(math.radians(tilt))))
            b0 = Vector((math.cos(a) * 0.4, math.sin(a) * 0.4, z))
            vp.lathe(p, 'CnMetal#motor', tuple(b0 - d * 0.02), tuple(d), [(0.0, 0.0), (0.21, 0.0), (0.23, 0.05), (0.23, 0.3), (0.15, 0.38),
                                                                        (0.0, 0.38)], seg=16)
            horn(p, b0 + d * 0.36, d, 0.12, rh, L - 0.36)
            tube(p, 'CnMetal#arm', (0.0, 0.0, z + 0.45), tuple(b0 + d * (0.36 + (L - 0.36) * 0.45) + Vector((0, 0, 0.12 * rh / 0.6))), 0.03, seg=6)
    # a agulha dourada, as 2 estrelas do ELP cruzadas (lê de qualquer lado) e a baliza
    vp.lathe(p, 'CnGold#spire', (0.0, 0.0, 0.0), 'z', [(0.0, 2.38), (0.14, 2.38), (0.08, 2.75), (0.05, 3.0), (0.0, 3.05)], seg=16)
    for ax in ('x', 'y'):
        for s in (-1, 1):
            if ax == 'x':
                M = (lambda s_: lambda u, v, n: Vector((s_ * n, -s_ * u, 3.32 + v)))(s)
            else:
                M = (lambda s_: lambda u, v, n: Vector((s_ * u, s_ * n, 3.32 + v)))(s)
            emblem(p, M, 0.4, 0.0, t=0.02)
    p.cyl('Beacon', (0.0, 0.0, 3.85), 'z', 0.08, 0.16, seg=12, bevel=0.0, smooth=True)
    p.cyl('CnGold', (0.0, 0.0, 3.72), 'z', 0.04, 0.12, seg=8, bevel=0.0)
    return p.finish()


# ===================================================================================================== o prédio
def base():
    p = BPart(PREFIX + '_Base')
    hall(p)
    tower(p)
    poles = screen(p)
    # os 2 postes de alto-falante da praça, flanqueando a escada
    for s in (-1, 1):
        x, y = polar(6.85, 21.0 * s)
        speaker_pole(p, x, y)
    # os cabos da torre até o mastro do meio do telão (o vão grande com a barriga) e do poste até a casa
    catenary(p, (HC[0] + 0.5, HC[1] - 0.5, Z_PLAT - 0.8), (poles[1][0], poles[1][1] + 0.1, MAST_TOP - 2.6), 0.9)
    catenary(p, (HC[0] + 0.5, HC[1] - 0.52, Z_PLAT - 1.4), (poles[0][0], poles[0][1] + 0.1, MAST_TOP - 3.2), 0.7)
    return p.finish()


def tris(o):
    return sum(len(poly.vertices) - 2 for poly in o.data.polygons)


def main():
    clear_scene()
    b = base()
    total = export(b, OUT)
    s = speakers()
    bake_in_place(s, SPK_AT)             # no executor do detalhe, a sombra de contato com as cornetas no lugar delas
    total += export(s, OUT)
    sc = scaffold(PREFIX + '_Scaffold', HC[0] - 4.9, HC[0] + 4.9, HC[1] - 4.9, HC[1] + 4.9, 8.0, post='CnMetal', plank='Crate', stripe='Hazard')
    total += export(sc, OUT)
    print('MODEL total', total, 'tris (antes do detalhe)')
    print('MODEL lote 14 x 24 m; speakers spin at_m', [round(v, 2) for v in SPK_AT], 'rpm 3 (o Units.json tem [-1.0, 7.4, 11.05])')
    print('MODEL fire_points_m', [[round(HC[0] + 3.2, 2), round(HC[1] + 2.4, 2), 4.6], [round(HC[0] - 3.3, 2), round(HC[1] - 2.0, 2), 4.6],
                                  [0.6, SC[1], 4.4]])
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]], [PREFIX + '_Speakers', [SPK_AT[0], -SPK_AT[1], SPK_AT[2]]]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1100, 1600]],
                      ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1600, 1000]],
                      ['side', 'ortho', [0, 1, 0], [0, 0, 1], [1200, 1000]],
                      ['three_quarter', 'persp', 34.0, 215.0, [1600, 1000], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 32.0, 35.0, [1600, 1000], 30.0, 0.95]],
            'usa_at': [11, -15, 0], 'tank_views': False,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, indent=1)


if __name__ == '__main__':
    main()
