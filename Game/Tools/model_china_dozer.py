"""O Dozer da China (o trator de esteira de obra, o construtor) provisório, modelado no Blender por script (o elenco, 31/10/2026).

Rodar (Blender sem janela, só CPU; não usa a placa):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_china_dozer.py -- captures/game/models/china_dozer

Fictício inspirado nos tratores de esteira chineses (a linha Shantui SD, a dos batalhões de engenharia do ELP), não cópia; a
estrutura das peças é a do Dozer da USA (Game/Tools/model_usa_dozer.py). A palavra: a LÂMINA. De cima: a lâmina larga e curva
na frente (mais larga que as esteiras), o capô comprido com a grade do radiador, a cabine quadrada atrás com a grade nas
janelas e a sirene, o escarificador de 3 dentes atrás. Da família da China: o oliva escuro com a camuflagem digital, a
estrela vermelha na porta, a lona dobrada, o aço escuro; o Dozer da USA é o "minerador" claro e facetado, este é o trator de
obra pesado e quadrado.
Medidas (apoio: abaixo da torre do tanque USA, que é 2,48 m):
  - 6,8 m da lâmina ao escarificador, 3,3 m de lâmina, o corpo de 4,3 × 2,64 m (com as esteiras), o capô a 1,78 m, o teto da
    cabine a 2,26 m, a sirene a ~2,5 m;
  - a esteira oval (motriz atrás, um pouco alta; roda-guia na frente; 5 roletes embaixo e 2 em cima), 0,56 m de largura, a
    bitola de 2,0 m, as sapatas com a garra alta de trator;
  - a lâmina: 3,3 × 1,25 m, a face côncava pra frente, o fio de corte, os cantos; os braços de empurrar (o C) por fora das
    esteiras até o PIVÔ no quadro (BLADE_PIVOT, a origem da peça _Blade; o jogo gira em Y quando o Dozer trabalha); os 2
    cilindros de levantar presos no capô;
  - a cabine 1,4 × 1,44 m com as janelas dos 4 lados, a grade de barras na frente e nos lados, a estrela na porta, os faróis de
    trabalho e a SIRENE (a cúpula âmbar no casco; a luz que gira é a peça _Beacon, a origem no eixo, BEACON);
  - atrás: o tanque de combustível, o escarificador de 3 dentes no paralelogramo com os 2 cilindros.
Cor do jogador (slot Team): a estrela vermelha nas duas portas, a lona dobrada presa no para-lama de trás.

Peças (origem no chão, no centro do corpo, frente em +X; o casco, as rodas e as esteiras no lugar delas):
  SM_China_Dozer_Hull     corpo, capô, grade, cabine, sirene (a cúpula), escarificador, cilindros da lâmina, para-lamas;
  SM_China_Dozer_Blade    a lâmina com os braços: a ORIGEM no pivô dos braços (BLADE_PIVOT; blade.at_m no Units.json);
  SM_China_Dozer_Beacon   a luz que gira dentro da cúpula (refletor + lâmpada): a ORIGEM no eixo (BEACON; spin at_m, ~90 rpm);
  SM_China_Dozer_Wheels   motriz, roda-guia, roletes (uma malha);
  SM_China_Dozer_Track_l / _Track_r  as esteiras.
Materiais: CnTankPaint, CnTankCamo, CnTankMetal, Track, Rubber, Team, Lens, Dark, Cloth, Light e Beacon (a cúpula da sirene que
pisca, o mesmo slot do Dozer da USA).
"""
import math
import os
import sys

from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import Part, V, clear_scene, export  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
from model_china_vehicle_kit import (antenna, basis, bounds, camo_faces, camo_poly, find_face, frame_box, headlight, idler,  # noqa: E402
                                     on_face, plane_point, roller, solid, sprocket, star, tarp_fold, track, tris, write_preview)

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'china_dozer')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_China_Dozer'

# ---------------------------------------------------------------------------------- medidas
TRACK_Y, TRACK_W = 1.0, 0.56
SPROCKET = (-1.6, 0.62, 0.4)
IDLER = (1.6, 0.42, 0.34)
ROLLERS_X, ROLLER_Z, ROLLER_R = [1.05, 0.5, -0.05, -0.6, -1.12], 0.22, 0.17
CARRIERS_X, CARRIER_Z, CARRIER_R = [0.75, -0.55], 0.86, 0.09
BODY_HW = 0.68                   # o corpo entre as esteiras
FENDER = (0.72, 1.32, 1.12)      # o para-lama: de y, até y, a altura
HOOD = (0.2, 2.15, 0.62, 1.12, 1.78)    # o capô: x0, x1, meia-largura, base, topo
CAB = (-1.55, -0.15, 0.72, 1.16, 2.18)  # a cabine: x0, x1, meia-largura, piso, teto
BLADE_PIVOT = (-0.2, 0.0, 0.45)  # o pivô dos braços da lâmina (blade.at_m)
BLADE_X = 2.95                   # as costas da lâmina
BEACON = (-1.28, -0.52, 2.3)     # o eixo da luz que gira (spin at_m)


def bar(p, a, b, r=0.02, m='CnTankMetal', seg=8):
    a, b = Vector(a), Vector(b)
    d = b - a
    p.cyl(m, tuple((a + b) / 2), tuple(d), r, d.length, seg=seg, bevel=0.0)


def beam(p, m, a, b, w, h, bevel=0.02):
    """Uma viga de seção retangular de a até b (o lado w na horizontal)."""
    a, b = Vector(a), Vector(b)
    d = b - a
    yaw = math.degrees(math.atan2(d.y, d.x))
    pitch = math.degrees(math.atan2(d.z, math.hypot(d.x, d.y)))
    p.obox(m, tuple((a + b) / 2), (d.length, w, h), yaw=yaw, pitch=pitch, bevel=bevel)


# ---------------------------------------------------------------------------------- casco
def hull():
    p = Part(PREFIX + '_Hull')
    p.bm('CnTankPaint#shell', 0.04)
    p.bm('CnTankPaint', 0.02)
    p.bm('CnTankPaint#thin', 0.006)
    p.bm('CnTankCamo', 0.0)
    p.bm('Team', 0.0)
    p.bm('Dark', 0.0)
    p.bm('CnTankMetal', 0.006)
    # o chassi entre as esteiras (o quadro baixo) e os quadros das esteiras com os suportes
    p.box('Dark', -1.95, 2.12, -BODY_HW, BODY_HW, 0.38, 0.62, bevel=0.02)
    fb = solid(p, 'CnTankPaint#shell', [(x, s * BODY_HW, z) for x, z in ((-1.95, 0.6), (2.12, 0.6), (2.12, FENDER[2]), (-1.95, FENDER[2]))
                                        for s in (-1, 1)])
    for s in (-1, 1):
        y = TRACK_Y * s
        p.box('Dark', -1.35, 1.4, y - 0.2, y + 0.2, 0.42, 0.58, bevel=0.015)
        p.box('CnTankPaint', -1.35, 1.4, y - 0.18, y + 0.18, 0.58, 0.72, bevel=0.015)
        yy0, yy1 = sorted((BODY_HW * s, (TRACK_Y - 0.2) * s))
        p.box('Dark', -1.2, -0.85, yy0, yy1, 0.45, 0.68, bevel=0.0)          # a ponte do quadro
        p.box('Dark', 0.6, 0.95, yy0, yy1, 0.45, 0.68, bevel=0.0)
        p.cyl('CnTankMetal', (BLADE_PIVOT[0], (TRACK_Y + 0.31) * s, BLADE_PIVOT[2]), 'y', 0.11, 0.12, seg=14)   # o munhão do braço
    # os para-lamas por cima das esteiras, com a dobra da frente e de trás
    for s in (-1, 1):
        y0, y1 = sorted((FENDER[0] * s, FENDER[1] * s))
        p.box('CnTankPaint', -1.95, 1.5, y0, y1, FENDER[2] - 0.04, FENDER[2] + 0.02, bevel=0.01)
        p.obox('CnTankPaint', (1.62, (FENDER[0] + FENDER[1]) / 2 * s, FENDER[2] - 0.14), (0.04, FENDER[1] - FENDER[0], 0.3), pitch=-35, bevel=0.01)
        p.obox('CnTankPaint', (-2.02, (FENDER[0] + FENDER[1]) / 2 * s, FENDER[2] - 0.14), (0.04, FENDER[1] - FENDER[0], 0.3), pitch=35, bevel=0.01)
        camo_poly(p, [(-1.9, y0 + 0.02, FENDER[2] + 0.02), (1.45, y0 + 0.02, FENDER[2] + 0.02), (1.45, y1 - 0.02, FENDER[2] + 0.02),
                      (-1.9, y1 - 0.02, FENDER[2] + 0.02)], (0, 0, 1), seed=41, inset=0.03)
    # ---------------------------------------------------------------- o capô comprido (com o caimento pra frente), a grade do radiador
    hx0, hx1, hw, hz0, hz1 = HOOD
    fh = solid(p, 'CnTankPaint#shell', [(hx0, s * hw, hz0) for s in (-1, 1)] + [(hx1, s * hw, hz0) for s in (-1, 1)] +
               [(hx0, s * (hw - 0.04), hz1) for s in (-1, 1)] + [(hx1, s * (hw - 0.06), hz1 - 0.1) for s in (-1, 1)])
    camo_faces(p, fh, seed=41, skip=lambda c, n: n.x > 0.9)
    camo_faces(p, fb, seed=41, skip=lambda c, n: n.z > 0.9 or n.x > 0.9)
    for s in (-1, 1):                                                      # as venezianas dos lados do capô
        yo = (hw - 0.02) * s
        for k in range(6):
            x = 0.75 + k * 0.17
            p.obox('Dark', (x, yo + 0.012 * s, 1.45), (0.04, 0.03, 0.3), bevel=0.0)
        p.box('CnTankMetal', 0.4, 0.55, hw * s - 0.01 if s > 0 else -hw - 0.02, hw * s + 0.02 if s > 0 else -hw + 0.01, 1.55, 1.62, bevel=0.0)
    # a grade do radiador na frente: a moldura grossa, o fundo escuro, as aletas, a guarda de barras
    gx = hx1
    p.box('CnTankPaint', gx, gx + 0.1, -0.7, 0.7, 0.55, 1.74, bevel=0.02)
    p.box('Dark', gx + 0.1, gx + 0.12, -0.56, 0.56, 0.7, 1.6, bevel=0.0)
    for k in range(10):
        p.obox('CnTankMetal', (gx + 0.13, 0.0, 0.76 + k * 0.088), (0.03, 1.08, 0.03), pitch=-30, bevel=0.0)
    for y in (-0.5, -0.25, 0.0, 0.25, 0.5):
        bar(p, (gx + 0.2, y, 0.62), (gx + 0.2, y, 1.7), r=0.022)
    for z in (0.66, 1.66):
        bar(p, (gx + 0.2, -0.6, z), (gx + 0.2, 0.6, z), r=0.025)
    for s in (-1, 1):
        bar(p, (gx + 0.1, 0.6 * s, 1.66), (gx + 0.2, 0.6 * s, 1.66), r=0.025)
        headlight(p, (gx + 0.06, 0.62 * s, 1.84), r=0.06, guard=False, lit=True)
        p.box('CnTankMetal', gx - 0.08, gx + 0.04, 0.62 * s - 0.04, 0.62 * s + 0.04, 1.76, 1.84, bevel=0.0)
    # o contrapeso/para-choque baixo da frente e os ganchos
    p.box('CnTankMetal', gx - 0.05, gx + 0.18, -0.58, 0.58, 0.38, 0.55, bevel=0.02)
    # o escape (o tubo com o chapéu) e o pré-filtro de ar no capô
    # (31/10) o escape torneado (a flange, o tubo com a curva, a boca) com o chapéu de chuva na dobradiça; o pré-filtro de ar (a
    # cuba com as aletas e a tampa)
    vp.lathe(p, 'CnTankMetal', (1.55, 0.32, hz1), 'z', [(0.0, 0.0), (0.12, 0.0), (0.12, 0.03), (0.08, 0.05), (0.072, 0.08), (0.072, 0.6),
                                                     (0.08, 0.62), (0.08, 0.64), (0.064, 0.64), (0.064, 0.5), (0.0, 0.5)], seg=16)
    vp.lathe(p, 'Dark', (1.55, 0.32, hz1 + 0.645), (0.34, 0.0, 0.94), [(0.0, 0.0), (0.1, 0.0), (0.1, 0.012), (0.0, 0.012)], seg=16)
    vp.lathe(p, 'CnTankPaint', (1.05, -0.3, hz1), 'z', [(0.0, 0.0), (0.06, 0.0), (0.06, 0.2), (0.13, 0.22), (0.14, 0.24), (0.14, 0.4),
                                                     (0.15, 0.41), (0.15, 0.44), (0.1, 0.46), (0.0, 0.465)], seg=20)
    for k in range(4):
        p.cyl('Dark#vp', (1.05, -0.3, hz1 + 0.27 + k * 0.035), 'z', 0.145, 0.01, seg=20, bevel=0.0)
    # os 2 cilindros de levantar a lâmina, presos na frente do capô (a outra ponta vai na lâmina)
    for s in (-1, 1):
        a = Vector((1.85, 0.76 * s, 1.58))
        b = Vector((BLADE_X + 0.02, 0.74 * s, 1.08))
        d = b - a
        p.box('CnTankMetal', 1.75, 1.95, 0.64 * s - 0.04 if s > 0 else -0.86, 0.86 if s > 0 else -0.64 + 0.04, 1.5, 1.66, bevel=0.0)
        L = d.length
        vp.lathe(p, 'CnTankPaint', a, d, [(0.0, 0.0), (0.075, 0.0), (0.095, 0.04), (0.095, L * 0.56), (0.08, L * 0.6), (0.0, L * 0.6)], seg=16)
        vp.lathe(p, 'CnTankMetal', a, d, [(0.0, L * 0.55), (0.05, L * 0.55), (0.05, L - 0.06), (0.06, L - 0.05), (0.06, L), (0.0, L)], seg=12)
        p.cyl('CnTankMetal#vp', tuple(a), 'y', 0.06, 0.2, seg=12, bevel=0.0)
    # ---------------------------------------------------------------- a cabine quadrada (as janelas dos 4 lados, a grade, a porta com
    # a estrela, o teto com a aba, os faróis de trabalho e a sirene)
    cx0, cx1, cw, cz0, cz1 = CAB
    zw = cz0 + 0.5                                                        # a linha das janelas
    fc = solid(p, 'CnTankPaint#shell', [(x, s * cw, z) for x in (cx0, cx1) for s in (-1, 1) for z in (cz0, zw)] +
               [(x, s * (cw - 0.05), cz1) for x in (cx0 + 0.04, cx1 - 0.04) for s in (-1, 1)] +
               [(x, s * cw, zw) for x in (cx0, cx1) for s in (-1, 1)])
    camo_faces(p, fc, seed=41, skip=lambda c, n: c.z > zw - 0.05 or n.z > 0.5)
    # os vidros (rentes às faces de cima) e os montantes escuros
    for s in (-1, 1):
        yo = (cw - 0.025) * s
        for (xa, xb) in ((cx0 + 0.08, -0.82), (-0.76, cx1 - 0.08)):
            p.box('Lens', xa, xb, min(yo, yo + 0.012 * s), max(yo, yo + 0.012 * s), zw + 0.06, cz1 - 0.08, bevel=0.0)
    p.box('Lens', cx1 - 0.03, cx1 - 0.015, -cw + 0.1, cw - 0.1, zw + 0.06, cz1 - 0.08, bevel=0.0)
    p.box('Lens', cx0 + 0.015, cx0 + 0.03, -cw + 0.1, cw - 0.1, zw + 0.06, cz1 - 0.08, bevel=0.0)
    # o teto com a aba e a grade de proteção (as barras na frente das janelas da frente e dos lados)
    p.box('CnTankPaint', cx0 - 0.06, cx1 + 0.08, -cw - 0.06, cw + 0.06, cz1, cz1 + 0.08, bevel=0.02)
    camo_poly(p, [(cx0 - 0.04, -cw - 0.04, cz1 + 0.08), (cx1 + 0.06, -cw - 0.04, cz1 + 0.08), (cx1 + 0.06, cw + 0.04, cz1 + 0.08),
                  (cx0 - 0.04, cw + 0.04, cz1 + 0.08)], (0, 0, 1), seed=41, inset=0.04)
    for k in range(9):
        y = -cw + 0.12 + k * (2 * cw - 0.24) / 8
        bar(p, (cx1 + 0.05, y, zw), (cx1 + 0.05, y, cz1), r=0.016)
    for z in (zw, (zw + cz1) / 2, cz1 - 0.02):
        bar(p, (cx1 + 0.05, -cw, z), (cx1 + 0.05, cw, z), r=0.018)
    for s in (-1, 1):
        yo = (cw + 0.05) * s
        for k in range(8):
            x = cx0 + 0.1 + k * (cx1 - cx0 - 0.2) / 7
            bar(p, (x, yo, zw), (x, yo, cz1), r=0.016)
        for z in (zw, cz1 - 0.02):
            bar(p, (cx0, yo, z), (cx1 + 0.05, yo, z), r=0.018)
        # a porta (o painel de baixo com a linha e a maçaneta) e a ESTRELA vermelha nela
        yd = cw * s
        p.box('Dark', -1.05, -1.02, min(yd, yd + 0.01 * s), max(yd, yd + 0.01 * s), cz0 + 0.04, zw, bevel=0.0)
        p.box('Dark', -0.3, -0.27, min(yd, yd + 0.01 * s), max(yd, yd + 0.01 * s), cz0 + 0.04, zw, bevel=0.0)
        p.box('CnTankMetal', -0.42, -0.32, min(yd, yd + 0.04 * s), max(yd, yd + 0.04 * s), zw - 0.14, zw - 0.1, bevel=0.0)
        star(p, (-0.66, yd, cz0 + 0.25), (0, s, 0), 0.17)
        # o degrau e o corrimão da porta
        yy0, yy1 = sorted((FENDER[1] * s - 0.05 * s, FENDER[1] * s))
        bar(p, (-0.95, (cw + 0.1) * s, cz0 + 0.1), (-0.95, (cw + 0.1) * s, zw + 0.2), r=0.018)
    # os faróis de trabalho no teto (na frente e atrás)
    for s in (-1, 1):
        headlight(p, (cx1 + 0.02, 0.5 * s, cz1 + 0.16), r=0.06, guard=False, lit=True)
        p.box('CnTankMetal', cx1 - 0.1, cx1 + 0.0, 0.5 * s - 0.03, 0.5 * s + 0.03, cz1 + 0.08, cz1 + 0.12, bevel=0.0)
    antenna(p, cx0 + 0.15, 0.55, cz1 + 0.08, 0.9)
    # (31/10) os retrovisores nos braços (dos 2 lados da frente da cabine), o extintor preso atrás
    for s in (-1, 1):
        root = Vector((cx1 + 0.04, (cw + 0.04) * s, cz1 - 0.12))
        vp.tube(p, 'CnTankMetal', vp.bent_bar([root, root + Vector((0.12, 0.14 * s, 0.0)), root + Vector((0.14, 0.24 * s, -0.08))], R=0.04, n=2),
                0.012, seg=6)
        vp.rbox(p, 'Dark', root + Vector((0.14, 0.27 * s, -0.16)), (0.03, 0.12, 0.2), yaw=15 * s, r=0.025, k=2)
    vp.lathe(p, 'Team', (cx0 - 0.08, -0.45, cz0 + 0.3), 'z', [(0.0, 0.0), (0.07, 0.0), (0.075, 0.02), (0.075, 0.42), (0.05, 0.47), (0.02, 0.48),
                                                             (0.02, 0.52), (0.0, 0.52)], seg=14)
    vp.jerrycan(p, (-1.0, 1.04, FENDER[2] + 0.255), yaw=0, m='CnTankCamo', cap_m='CnTankMetal')
    vp.ammo_box(p, (0.3, 1.02, FENDER[2] + 0.13), (0.7, 0.42, 0.24), m='CnTankPaint', latch_m='CnTankMetal')
    # a SIRENE (no canto de trás à esquerda do teto): o pé, a base, a cúpula âmbar (Beacon, pisca) e a tampa; a luz que gira é a peça
    # _Beacon
    bxp, byp, bzp = BEACON
    p.cyl('Dark', (bxp, byp, cz1 + 0.11), 'z', 0.12, 0.06, seg=16, bevel=0.0)
    p.cyl('Beacon', (bxp, byp, bzp + 0.08), 'z', 0.1, 0.2, seg=18, bevel=0.0, smooth=True)
    p.cyl('Dark', (bxp, byp, bzp + 0.195), 'z', 0.105, 0.03, seg=18, bevel=0.0)
    for k in range(3):
        a = math.radians(k * 120 + 30)
        p.cyl('CnTankMetal', (bxp + 0.13 * math.cos(a), byp + 0.13 * math.sin(a), bzp + 0.08), 'z', 0.011, 0.24, seg=6, bevel=0.0)
    # ---------------------------------------------------------------- atrás: o tanque de combustível, a lona, o escarificador
    p.box('CnTankPaint', -1.95, cx0, -0.7, 0.7, FENDER[2], 1.62, bevel=0.03)
    p.cyl('CnTankMetal', (-1.75, 0.45, 1.66), 'z', 0.08, 0.08, seg=12, bevel=0.0)
    # a lona DOBRADA (cor do jogador) presa no para-lama de trás, à esquerda
    tarp_fold(p, (-1.6, -1.04, FENDER[2] + 0.02), (0.6, 0.5, 0.16), m='Team', straps=2, seed=12.0)
    # o escarificador: o bloco de engate, o paralelogramo (2 braços por lado), a viga com os 3 dentes curvos e os cilindros
    p.box('CnTankMetal', -2.15, -1.95, -0.6, 0.6, 0.42, 1.05, bevel=0.02)
    bx = -2.78
    p.box('CnTankPaint', bx - 0.14, bx + 0.14, -0.9, 0.9, 0.7, 0.96, bevel=0.025)
    for s in (-1, 1):
        y = 0.5 * s
        beam(p, 'CnTankPaint', (-2.12, y, 0.98), (bx + 0.1, y, 0.92), 0.14, 0.14)
        beam(p, 'CnTankPaint', (-2.12, y, 0.5), (bx + 0.1, y, 0.74), 0.14, 0.14)
        for (x, z) in ((-2.12, 0.98), (-2.12, 0.5), (bx + 0.1, 0.92), (bx + 0.1, 0.74)):
            p.cyl('Dark', (x, y, z), 'y', 0.06, 0.18, seg=10, bevel=0.0)
        a = Vector((-1.98, 0.28 * s, 1.5))
        b = Vector((bx + 0.05, 0.28 * s, 1.0))
        d = b - a
        L = d.length
        vp.lathe(p, 'CnTankPaint', a, d, [(0.0, 0.0), (0.06, 0.0), (0.078, 0.04), (0.078, L * 0.56), (0.065, L * 0.6), (0.0, L * 0.6)], seg=14)
        vp.lathe(p, 'CnTankMetal', a, d, [(0.0, L * 0.55), (0.042, L * 0.55), (0.042, L - 0.05), (0.052, L - 0.04), (0.052, L), (0.0, L)], seg=12)
    for y in (-0.62, 0.0, 0.62):
        pts = [(bx, 0.82), (bx - 0.08, 0.5), (bx - 0.2, 0.26), (bx - 0.38, 0.1)]
        for (xa, za), (xb, zb) in zip(pts[:-1], pts[1:]):
            beam(p, 'CnTankPaint', (xa, y, za), (xb, y, zb), 0.1, 0.2, bevel=0.015)
        beam(p, 'Dark', (bx - 0.36, y, 0.11), (bx - 0.52, y, 0.04), 0.09, 0.12, bevel=0.0)
    # o engate e as lanternas de trás no tanque
    for s in (-1, 1):
        p.box('Dark', -1.97, -1.95, 0.55 * s - 0.08, 0.55 * s + 0.08, 1.42, 1.54, bevel=0.0)
        p.box('Lens', -1.975, -1.965, 0.55 * s - 0.05, 0.55 * s + 0.05, 1.45, 1.51, bevel=0.0)
    return p.finish()


# ---------------------------------------------------------------------------------- lâmina (gira em volta de BLADE_PIVOT)
def blade():
    p = Part(PREFIX + '_Blade')
    p.bm('CnTankPaint#shell', 0.03)
    p.bm('CnTankPaint', 0.02)
    p.bm('CnTankCamo', 0.0)
    p.bm('Dark', 0.0)
    p.bm('CnTankMetal', 0.008)
    W = 1.65
    # a lâmina: a face côncava pra frente, o fio de corte embaixo, o lábio de cima pra frente, as costas retas
    # (31/10) a curva da face (o arco da lâmina de verdade, 9 pontos: nada de quebra de chapa onde a ficha tem curva)
    face = [(BLADE_X + 0.6, 0.04), (BLADE_X + 0.5, 0.18), (BLADE_X + 0.42, 0.33), (BLADE_X + 0.36, 0.5), (BLADE_X + 0.33, 0.66),
            (BLADE_X + 0.33, 0.82), (BLADE_X + 0.36, 0.98), (BLADE_X + 0.41, 1.12), (BLADE_X + 0.48, 1.25)]
    back = [(BLADE_X + 0.12, 1.25), (BLADE_X, 0.95), (BLADE_X, 0.4), (BLADE_X + 0.12, 0.04)]
    p.prism_xz('CnTankPaint#shell', face + back, -W, W)
    p.obox('CnTankMetal', (BLADE_X + 0.6, 0.0, 0.08), (0.06, 2 * W + 0.02, 0.16), pitch=-40, bevel=0.0)   # o fio de corte
    # os parafusos do fio de corte (a fileira: as facas são aparafusadas) e a junta das 3 facas
    en = Vector((math.sin(math.radians(40)), 0.0, math.cos(math.radians(40))))
    vp.bolt_row(p, 'Dark', (BLADE_X + 0.615, -W + 0.12, 0.095), (BLADE_X + 0.615, W - 0.12, 0.095), 16, en, r=0.022, h=0.014)
    for yy in (-W / 3, W / 3):
        p.obox('Dark#vp', (BLADE_X + 0.62, yy, 0.09), (0.07, 0.02, 0.17), pitch=-40, bevel=0.0)
    p.box('Dark', BLADE_X + 0.1, BLADE_X + 0.52, -W, W, 1.25, 1.3, bevel=0.0)                             # o lábio de cima
    for s in (-1, 1):                                                                                       # os cantos (as bicas)
        p.prism_xz('CnTankMetal', [(BLADE_X + 0.66, 0.02), (BLADE_X + 0.44, 0.34), (BLADE_X + 0.3, 0.36), (BLADE_X + 0.3, 0.02)],
                   (W - 0.3) * s if s > 0 else -W - 0.01, W + 0.01 if s > 0 else -(W - 0.3), bevel=0.0)
        yy0, yy1 = sorted((W * s, (W + 0.06) * s))
        p.prism_xz('CnTankPaint', [(BLADE_X + 0.64, 0.04), (BLADE_X + 0.46, 0.32), (BLADE_X + 0.36, 0.66), (BLADE_X + 0.38, 0.98),
                                   (BLADE_X + 0.52, 1.25), (BLADE_X + 0.06, 1.25), (BLADE_X, 0.04)], yy0, yy1, bevel=0.01)
    # as nervuras atrás (com a camuflagem nas costas)
    for y in (-1.2, -0.4, 0.4, 1.2):
        p.box('CnTankPaint', BLADE_X - 0.1, BLADE_X + 0.02, y - 0.06, y + 0.06, 0.15, 1.15, bevel=0.01)
    p.box('CnTankPaint', BLADE_X - 0.1, BLADE_X + 0.02, -W + 0.1, W - 0.1, 0.55, 0.75, bevel=0.01)
    camo_poly(p, [(BLADE_X, -W + 0.05, 0.42), (BLADE_X, W - 0.05, 0.42), (BLADE_X, W - 0.05, 0.93), (BLADE_X, -W + 0.05, 0.93)],
              (-1, 0, 0), seed=42, inset=0.02)
    # os braços de empurrar (o C) por fora das esteiras: das costas da lâmina ao pivô no quadro, com a escora diagonal
    for s in (-1, 1):
        a = Vector((BLADE_X - 0.05, 1.22 * s, 0.5))
        b = Vector((BLADE_PIVOT[0], (TRACK_Y + 0.42) * s, BLADE_PIVOT[2]))
        beam(p, 'CnTankPaint', a, b, 0.16, 0.26, bevel=0.02)
        beam(p, 'CnTankPaint', (BLADE_X - 0.05, 1.22 * s, 0.5), (BLADE_X - 0.05, (TRACK_Y + 0.42) * s, 0.5), 0.2, 0.26, bevel=0.02)
        p.cyl('CnTankMetal', (b.x, b.y, b.z), 'y', 0.14, 0.2, seg=16, bevel=0.0)
        p.cyl('Dark', (b.x, b.y + 0.1 * s, b.z), 'y', 0.06, 0.04, seg=12, bevel=0.0)
        beam(p, 'CnTankMetal', (BLADE_X - 0.02, 0.9 * s, 1.0), (1.9, (TRACK_Y + 0.42) * s, 0.5), 0.07, 0.07, bevel=0.0)  # a escora
        p.box('CnTankMetal', BLADE_X - 0.1, BLADE_X + 0.02, 0.74 * s - 0.08, 0.74 * s + 0.08, 0.98, 1.18, bevel=0.0)   # o olhal do cilindro
    o = p.finish()
    o.data.transform(Matrix.Translation(-V(*BLADE_PIVOT)))
    return o


def beacon():
    """A luz que gira dentro da cúpula da sirene (refletor e lâmpada), com a ORIGEM no eixo (BEACON)."""
    p = Part(PREFIX + '_Beacon')
    p.cyl('CnTankMetal', (0.0, 0.0, 0.0), 'z', 0.03, 0.04, seg=10, bevel=0.0)
    p.obox('CnTankMetal', (-0.04, 0.0, 0.08), (0.02, 0.1, 0.12), bevel=0.0)
    p.obox('Light', (-0.015, 0.0, 0.08), (0.03, 0.06, 0.08), bevel=0.0)
    return p.finish()


# ---------------------------------------------------------------------------------- rodagem
def wheels():
    p = Part(PREFIX + '_Wheels')
    for s in (-1, 1):
        y = TRACK_Y * s
        sprocket(p, SPROCKET[0], y, SPROCKET[1], SPROCKET[2], 0.48, s, teeth=13)
        idler(p, IDLER[0], y, IDLER[1], IDLER[2], 0.46, s)
        for x in ROLLERS_X:
            vp.track_roller(p, (x, y, ROLLER_Z), ROLLER_R, 0.42, 'CnTankMetal', s)
        for x in CARRIERS_X:
            roller(p, x, y, CARRIER_Z, CARRIER_R, 0.3)
    return p.finish()


def circles():
    return [(IDLER[0], IDLER[1], IDLER[2] + 0.05), (SPROCKET[0], SPROCKET[1], SPROCKET[2] + 0.05)] + \
           [(x, ROLLER_Z, ROLLER_R + 0.05) for x in ROLLERS_X] + [(x, CARRIER_Z, CARRIER_R + 0.05) for x in CARRIERS_X]


def tracks(side):
    """A esteira de trator: a sapata larga com a garra alta (o elo de 19 cm)."""
    s = 1 if side == 'r' else -1
    zt = CARRIER_Z + CARRIER_R + 0.05
    sags = [(CARRIERS_X[1] + 0.1, CARRIERS_X[0] - 0.1, zt, 0.03)]
    p = track(f'{PREFIX}_Track_{side}', circles(), TRACK_Y * s, TRACK_W, pitch=0.19, thick=0.05, bevel=0.005, guide=False,
              sags=sags, grouser=0.05)
    return p


# ---------------------------------------------------------------------------------- exportação
def main():
    clear_scene()
    h, bl, be, w = hull(), blade(), beacon(), wheels()
    tl, tr = tracks('l'), tracks('r')
    # (31/10) com o executor do detalhe (model_run_detail.py) as máscaras são cozidas depois do script, com as peças onde estão, e a
    # exportação vem depois: a lâmina voltava pro lugar (o bounds abaixo) e SAÍA com a origem errada; a luz da sirene era cozida na
    # origem, dentro do casco. Aqui as duas ficam no lugar pro cozimento e vão pra origem do pivô/eixo só na hora de exportar
    import model_lib
    detail = model_lib.DETAIL.get('defer') is not None
    if detail:
        be.data.transform(Matrix.Translation(V(*BEACON)))
        _orig = model_lib.export

        def _export(o, out_dir):
            if model_lib.DETAIL.get('defer') is None:
                if o is bl:
                    o.data.transform(Matrix.Translation(-V(*BLADE_PIVOT)))
                elif o is be:
                    o.data.transform(Matrix.Translation(-V(*BEACON)))
            return _orig(o, out_dir)
        model_lib.export = _export
    total = 0
    for o in (h, bl, be, w, tl, tr):
        total += export(o, OUT)
    print('MODEL total', total, 'tris')
    bl.data.transform(Matrix.Translation(V(*BLADE_PIVOT)))
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h, bl, w, tl, tr])
    (bx0, bx1), (by0, by1), (bz0, bz1) = bounds([bl])
    (hx0, hx1), (hy0, hy1), (hz0, hz1) = bounds([h, w, tl, tr])
    print('MODEL medidas: comprimento total %.2f m (x %.2f..%.2f, da lamina ao escarificador), largura %.2f m (a lamina), altura %.2f m'
          % (ax1 - ax0, ax0, ax1, ay1 - ay0, az1))
    print('MODEL corpo (sem a lamina): %.2f x %.2f m; o teto da cabine a %.2f m; a lamina %.2f x %.2f m (x %.2f..%.2f)'
          % (hx1 - hx0, hy1 - hy0, CAB[4] + 0.08, by1 - by0, bz1 - bz0, bx0, bx1))
    print('MODEL blade.at_m', list(BLADE_PIVOT), '(o pivo dos bracos; gira em Y)', 'beacon spin at_m', list(BEACON), 'rpm 90')
    print('MODEL tracks_m', [round(2 * TRACK_Y, 3), TRACK_W])
    print('MODEL tris por peca', {o.name: tris(o) for o in (h, bl, be, w, tl, tr)})
    write_preview(OUT, [(PREFIX + '_Hull', (0, 0, 0)), (PREFIX + '_Blade', (BLADE_PIVOT[0], -BLADE_PIVOT[1], BLADE_PIVOT[2])),
                        (PREFIX + '_Beacon', (BEACON[0], -BEACON[1], BEACON[2])), (PREFIX + '_Wheels', (0, 0, 0)),
                        (PREFIX + '_Track_l', (0, 0, 0)), (PREFIX + '_Track_r', (0, 0, 0))],
                  extra={'tank_views': False, 'usa_at': [0, 6.0, 0]})


main()
