"""O Quartel-General da Rússia (Command Center, o Штаб), v3 (03/11/2026): A FORTALEZA DO KREMLIN COM O RADAR DE CEBOLA.
Fiel ao conceito escolhido docs/concept/buildings/russia_shtab_kremlin_radar_ESCOLHIDO.png (a ficha de vistas do GPT, só pra proporção:
captures/concept/177_shtab_v3_ficha/raiz_r1_gpt.png). O v1 ("parece uma prisão") e o v2 ("casa civil, sem originalidade") ficam guardados.

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_russia_command_center_v3.py C:/Users/luisf/explosion/captures/game/models/russia_command_center_v3

A palavra (o que lê de cima num relance): A FORTALEZA DE AMEIAS EM RABO DE ANDORINHA COM A CEBOLA BRANCA NO MEIO. As muralhas de concreto
pré-moldado (o painel da família, as juntas fundas) coroadas pelas ameias do Kremlin de Moscou (largas e atarracadas, coladas umas
nas outras: ~1,4 m de largura, 0,95 m de altura, o vão de 0,5 m; só na coroa das muralhas de fora, do torreão e do 1º degrau da torre),
a torre octogonal em 2 degraus, o tambor de aço escuro e, em cima, o
RADOME EM CÚPULA DE CEBOLA (bojudo embaixo, afinando numa ponta, de painéis geodésicos brancos: a cebola do Kremlin reinventada como radar
militar; a bola branca é da USA), a agulha de treliça e a estrela das Forças Armadas. Na frente, o portão blindado entre os pilones
escalonados, o medalhão com a estrela em cima; à direita (−Y) o pátio de obra amarelo e preto com o GUINDASTE VERMELHO e a garra (a peça
_Crane); à esquerda (+Y) a ala de telhados verdes ondulados com a sala de rádio; as faixas azuis da cor do jogador nas paredes; o bloco de
canto da frente esquerda com a bandeira.

Peça por peça:
- o torreão (14,8 × 11,6 m, 7,6 m + ameias até 9,7 m): o bloco oco de painéis com a cornija em mísulas, o parapeito e as ameias em volta;
  no teto, o gerador, as caixas de munição, as saídas de ventilação, o mastro da antena que gira (_Radar);
- o bloco do portão (6,8 × 11,4 m, 6,4 m + ameias até 8,5 m): o portão de 6 × 4,9 m com o quadro de aço, a porta blindada levantada (a borda
  com o perigo), as guias, o salão aceso por dentro com a porta do fundo acesa; os 2 pilones escalonados e as 2 aletas de cada lado; o
  MEDALHÃO redondo com a estrela no meio das ameias da frente; a laje de aço na frente (a rampa de saída);
- as alas (5,0 m + ameias até 7,1 m): os telhados verdes de uma água em vãos com os rufos e as calhas; na esquerda, a sala de rádio com o
  telhado verde, as antenas de chicote e a parabólica; as pilastras e as seteiras nas paredes de fora;
- o bloco de canto da frente esquerda (o quadrado de quina redonda em fiadas, o talude no pé, o parapeito liso) com a bandeira; a outra
  bandeira na quina da frente do teto do torreão, sobre o pátio;
- a torre: o 1º degrau octogonal em talude (3 fiadas, as nervuras nas quinas, as janelas e seteiras, as ameias), o 2º degrau (as janelas
  altas, as ameias menores), o tambor de aço com as nervuras e a fita de janelas, a cornija, a CEBOLA geodésica (as faces planas dos
  painéis e as costuras), o colar, a agulha de treliça e a estrela de dupla face;
- o pátio de obra: o muro baixo com as ameias, o piso de chapas com a moldura de perigo e as 2 cruzes acesas de montagem, os batentes, a
  torre do guindaste no canto da frente (as fiadas, a escada, o anel de giro em cima), o tanque vermelho com a escada.

Lote 26 × 28 m (X × Y), frente em +X (o portão de onde o trator sai; a faixa de saída de 6,6 m fica livre na frente do lote).
Alturas: as alas 5,0 (ameias até 6,6), o portão 6,4 (8,0; o medalhão 9,1), o torreão 7,6 (9,2), o 1º degrau 11,8 (13,25), o 2º
13,6 (o parapeito 14,15), o tambor até 16,25, a cebola até ~21,5, a agulha até ~23,9, a estrela até ~25,6; a ponta da lança do guindaste
~15,2.

Peças (origem no chão, no centro do lote; coordenadas da Unreal, X frente, Y direita, Z cima, metros):
  SM_Russia_CommandCenter_Base: tudo o que não se mexe (o radome é fixo; as 2 bandeiras saem no flags.json);
  SM_Russia_CommandCenter_Crane: o guindaste (gira em Z; a origem no eixo do anel de giro, no topo da torre dele no canto da frente do
    pátio: CRANE_AT). Em repouso (giro 0) a lança levantada a 45° aponta pra CRANE_YAW = 146° (pra trás e pra dentro do pátio), a garra
    pendurada no meio do pátio. Balança de −10° a +20° em volta do repouso (mais que isso a garra bate no bloco do portão ou no muro);
  SM_Russia_CommandCenter_Radar: a antena de vigilância que gira (gira em Z; origem no eixo, no topo do mastro: RADAR_AT; 6 rpm como a do v2);
  SM_Russia_CommandCenter_Scaffold: o andaime da obra.
Materiais (os slots da Rússia): Concrete, Roof (o verde), Metal, Red, White, Paint, Team (as faixas e as bandeiras), Navy (a borda azul da
estrela; (04/11) a do meio em relevo facetado, ru_star_relief: "falta [...] a estrela chata"), Hazard (o pátio), Glass, Light, Dark, Wood, Rubber (as juntas), Beacon.
"""
import math
import os
import sys

import bmesh
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, V, clear_scene, export, scaffold  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
from model_faction_kit import lamp, revolve, disc_ring, star_points  # noqa: E402
from model_building_parts import WallFace, wbox, hexa, tube, splits, cable, bake_in_place, cn_rollup, EPS  # noqa: E402
from model_russia_parts import (RuPart, ru_block, ru_panel_wall, ru_star, stencil, flag_pole, lattice_mast, vent_shaft,  # noqa: E402
                                ac_box, prof_sheet, entry_door, ladder_on, cuts, JOINT, grid_box, ammo_crate)

# (só a cor de conferência da folha: o jogo troca a Team pela cor do jogador; azul como no conceito e nos prédios aprovados no jogo)
COLORS['Team'] = (44, 84, 176)

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'russia_command_center_v3')
PREFIX = 'SM_Russia_CommandCenter'

HX, HY = 13.0, 14.0                       # o lote 26 × 28
Z0 = -0.25                                # o pé das paredes (enterrado: o terreno ondulado não abre fresta)
T = 0.31                                  # a espessura das paredes de painel (0,31 + 0,08 de saliência = 0,39: a lateral do painel cabe
                                          # numa célula da grade do detalhe, 0,4 m; com 0,35 virava 2 e custava ~10% a mais)
# os blocos (x0, x1, y0, y1) e as alturas do teto (o topo das paredes; a cornija, o parapeito e as ameias vão por cima)
KEEP, KZ = (-10.6, 4.2, -5.8, 5.8), 7.6
GATEB, GZ = (4.2, 11.0, -5.7, 5.7), 6.4
LWING, RWING, WZ = (-10.6, 7.6, 5.80, 12.8), (-10.6, -1.0, -12.8, -5.80), 5.0
YARD_FRONT, YARD_SIDE, YZ, YT = 11.0, -12.8, 2.8, 0.7     # o muro do pátio: a face da frente, a face do lado, a altura, a espessura
# (03/11, 2ª volta: os 4 baluartes com 2 ameias liam como torre de xadrez, castelo de brinquedo; o conceito só tem os blocos de canto da
# frente) o bloco de canto da ala esquerda e o do pátio
BAST = {'fl': ((6.4, 10.2, 9.6, 13.2), 6.0)}
# a torre: o eixo, o 1º e o 2º degrau (raio do canto embaixo, em cima, z0, z1), o tambor (raio, z0, z1), a cebola
TC = (-3.2, 0.0)
T1 = (5.0, 4.35, KZ - 0.2, 11.8)
T2 = (3.5, 3.2, 11.8, 13.6)
DRUM = (2.0, 13.55, 16.25)
ONION_Z0 = 16.4
ONION_S = 1.06                            # a escala do perfil (o radome acompanha a torre maior)
# o perfil da cebola (raio, altura a partir do pé): o pescoço, a barriga em 1,4 m, a curva reversa afinando na ponta
ONION_PROF = [(2.05, 0.0), (2.34, 0.35), (2.6, 0.85), (2.72, 1.4), (2.66, 1.95), (2.44, 2.5), (2.07, 3.0), (1.62, 3.45), (1.14, 3.88),
              (0.72, 4.3), (0.42, 4.7), (0.22, 5.1)]
GATE = (-3.0, 3.0, 4.9)                   # o vão do portão: y0, y1, o topo
# (03/11, 3ª volta: o guindaste) a torre do guindaste no canto da frente do pátio (no lugar do bloco de canto): a lança levantada sobe pra
# trás e pra dentro, sobre o pátio, inclinada pra LONGE da câmera do jogo (que olha de +X, um pouco de −Y): lê o comprimento inteiro a
# 37,5° e a 55°. Com a torre no fundo do pátio, toda lança sobre o pátio apontava pra câmera e encurtava (a 40° pra cima, ~27% do
# comprimento: o quadrado de treliça)
CRANE_AT = (10.7, -12.1, 7.4)             # o eixo do guindaste (o topo da torre dele)
CRANE_TOWER = (9.6, 11.8, -13.2, -11.0)
CRANE_YAW = 146.0                         # em repouso a lança aponta pra cá (graus, +X → +Y): a garra no meio do pátio
PAD_MARKS = [(4.5, -9.0), (7.7, -9.0)]    # as 2 cruzes acesas de montagem no pátio
RADAR_AT = (-9.0, -4.3, 10.9)
PAD = (2.2, 10.0, -11.8, -6.2)            # as chapas do pátio de montagem
LUFF, BOOM_L, HOOK_Z = 45.0, 7.4, -1.35   # a lança levantada 45°, o comprimento dela, o moitão (local) em repouso


def lerp(a, b, t):
    return a + (b - a) * t


# ===================================================================================================== as ameias
def poly_ring(p, m, pts, n_lo, n_hi, z0, z1, closed=True, ext=(0.0, 0.0), bevel=0.05):
    """O anel (ou a faixa aberta) de seção retangular ao longo do contorno pts [(x, y)...] no sentido anti-horário (a normal de fora é a
    da direita do caminho), de n_lo a n_hi pra fora da linha, de z0 a z1; as quinas em meia-esquadria (um sólido só, fechado: nada de
    caixas se encostando). ext: quanto as pontas da faixa aberta passam do primeiro e do último ponto."""
    P = [Vector((x, y, 0.0)) for (x, y) in pts]
    k = len(P)
    segs = k if closed else k - 1
    D = [(P[(i + 1) % k] - P[i]).normalized() for i in range(segs)]
    N = [Vector((d.y, -d.x, 0.0)) for d in D]
    M = []
    for i in range(k):
        if closed:
            a, c = N[i - 1], N[i % segs]
        else:
            a = N[i - 1] if i > 0 else N[0]
            c = N[i] if i < segs else N[-1]
        M.append((a + c) / (1.0 + a.dot(c)))
    Q = [q.copy() for q in P]
    if not closed:
        Q[0] = Q[0] - D[0] * ext[0]
        Q[-1] = Q[-1] + D[-1] * ext[1]
    b = p.bm(m, bevel)

    def vv(i, n, z):
        q = Q[i] + M[i] * n
        return b.verts.new(V(q.x, q.y, z))
    lb = [vv(i, n_lo, z0) for i in range(k)]
    hb = [vv(i, n_hi, z0) for i in range(k)]
    ht = [vv(i, n_hi, z1) for i in range(k)]
    lt = [vv(i, n_lo, z1) for i in range(k)]
    for i in range(segs):
        j = (i + 1) % k
        b.faces.new([hb[i], hb[j], ht[j], ht[i]])
        b.faces.new([ht[i], ht[j], lt[j], lt[i]])
        b.faces.new([lt[i], lt[j], lb[j], lb[i]])
        b.faces.new([lb[i], lb[j], hb[j], hb[i]])
    if not closed:
        b.faces.new([lb[0], hb[0], ht[0], lt[0]])
        b.faces.new([lt[-1], ht[-1], hb[-1], lb[-1]])


def band_boxes(p, m, pts, n_lo, n_hi, z0, z1, closed=True, ext=(0.0, 0.0), bevel=0.05):
    """O mesmo anel/faixa do poly_ring nas muralhas retas (as quinas de 90°, convexas), em caixas retangulares: cada trecho passa da quina
    por fora (cobre o quadrado da quina) e o seguinte começa 2 mm dentro dele (sem vértice repetido; o ring_boxes do kit). Só retângulos: a
    face de cima da meia-esquadria tinha a aresta inclinada de 1,1 m e a grade do detalhe a cortava em 4 (o dobro dos triângulos)."""
    k = len(pts)
    ns = k if closed else k - 1
    for i in range(ns):
        A = Vector((pts[i][0], pts[i][1], 0.0))
        B = Vector((pts[(i + 1) % k][0], pts[(i + 1) % k][1], 0.0))
        L = (B - A).length
        d = (B - A) / L
        nv = Vector((d.y, -d.x, 0.0))
        s0 = (-n_lo - EPS) if (closed or i > 0) else -ext[0]
        s1 = (L + n_hi) if (closed or i < ns - 1) else L + ext[1]
        P = []
        for z in (z0, z1):
            for n in (n_lo, n_hi):
                for sv in (s0, s1):
                    q = A + d * sv + nv * n
                    P.append((q.x, q.y, z))
        hexa(p, m, P, bevel=bevel)


def merlon(p, m, A, d, nv, s, z, w, h, n0, n1):
    """A AMEIA EM RABO DE ANDORINHA do Kremlin de Moscou, LARGA E ATARRACADA (03/11, 3ª volta: as altas e finas liam como torre de xadrez):
    o bloco de concreto grosso com o corte em V no alto (0,36 da altura de fundo, a boca de metade da largura), os 2 chifres curtos abrindo um
    pouco pra fora. A em (x, y), d ao longo, nv pra fora; s = o centro ao longo; n0..n1 a espessura (pra fora da linha)."""
    fl = 0.04
    hz, nd = h * 0.55, h * 0.36
    prof = [(-w / 2, 0.0), (w / 2, 0.0), (w / 2, hz), (w / 2 + fl, h), (w * 0.25, h), (0.0, h - nd), (-w * 0.25, h), (-w / 2 - fl, h),
            (-w / 2, hz)]
    b = p.bm(m, 0.06)

    def P(u, zz, nn):
        q = A + d * (s + u) + nv * nn
        return V(q.x, q.y, z + zz)
    fr = [b.verts.new(P(u, zz, n1)) for (u, zz) in prof]
    bk = [b.verts.new(P(u, zz, n0)) for (u, zz) in prof]
    b.faces.new(fr)
    b.faces.new(list(reversed(bk)))
    for i in range(len(prof)):
        j = (i + 1) % len(prof)
        b.faces.new([fr[i], bk[i], bk[j], fr[j]])


# (04/11, depois de ver no jogo: de perto as ameias ainda puxavam pro castelo medieval) mais baixas e largas, como o bastião de concreto:
# 0,72 m de altura (era 0,95), 1,6 m de largura, o vão de 0,45 m
def merlons_on(p, pts, nface, z, closed=False, w=1.6, h=0.72, mt=0.6, gap=0.45, corner=0.45, end=0.25, skip=(), segs=None,
               m='Concrete#merl'):
    """As ameias ao longo do contorno (os segmentos de pts, anti-horário), com a face de fora em nface; coladas umas nas outras com o vão
    fixo 'gap' (a largura se ajusta ±25% pra fechar o segmento); nas quinas ficam a 'corner' da quina (as de um lado não entram nas do
    outro), nas pontas soltas a 'end'. segs: só nesses segmentos (None = todos); skip = [(x0, x1, y0, y1)...]: onde não vai ameia (o bloco de
    canto, o pilone, o medalhão, a torre do guindaste)."""
    k = len(pts)
    ns = k if closed else k - 1
    n = 0
    for i in range(ns):
        if segs is not None and i not in segs:
            continue
        A = Vector((pts[i][0], pts[i][1], 0.0))
        B = Vector((pts[(i + 1) % k][0], pts[(i + 1) % k][1], 0.0))
        L = (B - A).length
        d = (B - A) / L
        nv = Vector((d.y, -d.x, 0.0))
        m0 = corner if (closed or i > 0) else end
        m1 = corner if (closed or i < ns - 1) else end
        avail = L - m0 - m1
        if avail < w * 0.7:
            continue
        cnt = max(1, int(round((avail + gap) / (w + gap))))
        ww = (avail - (cnt - 1) * gap) / cnt
        if ww > 1.25 * w:
            cnt += 1
            ww = (avail - (cnt - 1) * gap) / cnt
        for j in range(cnt):
            s = m0 + ww / 2 + j * (ww + gap)
            c = A + d * s + nv * (nface - mt / 2)
            hw = ww / 2 + 0.15
            box = (c.x - hw, c.x + hw, c.y - hw, c.y + hw)
            if any(box[0] < q[1] and box[1] > q[0] and box[2] < q[3] and box[3] > q[2] for q in skip):
                continue
            merlon(p, m, A, d, nv, s, z, ww, h, nface - mt, nface)
            n += 1
    return n


def battlement(p, pts, z, closed=True, ext=(0.0, 0.0), skip=(), tag='', merlons=True, corbel=True, octagon=False, **kw):
    """O alto da muralha do Kremlin: a cornija em mísulas (o friso saliente), o parapeito por cima (saindo mais: a sombra fina embaixo dele
    lê de cima) e as ameias em cima do parapeito (merlons=False: só o parapeito liso, nos blocos de canto). z = o topo da parede (o teto);
    as ameias vão de z + 0,65 a z + 1,6."""
    ring = poly_ring if octagon else band_boxes
    if corbel:
        ring(p, 'Concrete#corb' + tag, pts, -0.18, 0.22, z - 0.55, z - 0.1, closed=closed, ext=ext, bevel=0.05)
    ring(p, 'Concrete#band' + tag, pts, -0.48, 0.30, z - 0.1, z + 0.65, closed=closed, ext=ext, bevel=0.06)
    if merlons:
        return merlons_on(p, pts, 0.26, z + 0.65, closed=closed, skip=skip, **kw)
    return 0


def rect(b):
    x0, x1, y0, y1 = b
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def grow(b, e):
    return (b[0] - e, b[1] + e, b[2] - e, b[3] + e)


# ===================================================================================================== as paredes
def face_us(block, walls, skin, f, pw=3.0, t=T):
    """As juntas verticais dos painéis da face f do ru_block (o mesmo cálculo de lá, sem os vãos estreitos): pra pôr a seteira, a pilastra
    e a faixa no meio de um painel."""
    x0, x1, y0, y1 = block
    if f[1] == 'x':
        a, b = y0 + (t if '-y' in walls else 0.0), y1 - (t if '+y' in walls else 0.0)
        a += 0.04 if '-y' in walls else 0.003
        b -= 0.04 if '+y' in walls else 0.003
    else:
        a = x0 - (0.08 if ('-x' in skin and '-x' in walls) else 0.0)
        b = x1 + (0.08 if ('+x' in skin and '+x' in walls) else 0.0)
    return cuts(a, b, pw)


def mids(us):
    return [(a + b) / 2 for a, b in zip(us[:-1], us[1:])]


def slit_holes(us, picks, z0, z1, w=0.32):
    return [(mids(us)[i] - w / 2, mids(us)[i] + w / 2, z0, z1) for i in picks]


def slit_backs(p, F, holes):
    """O fundo escuro das seteiras (a parede é oca: o vão mostra o escuro de dentro) e o peitoril de aço."""
    for (u0, u1, z0, z1) in holes:
        wbox(p, 'Dark', F, u0 - 0.02, u1 + 0.02, z0 - 0.02, z1 + 0.02, -T + 0.01, -T + 0.04, bevel=0.0)
        wbox(p, 'Metal#sill', F, u0 - 0.06, u1 + 0.06, z0 - 0.06, z0, -0.05, 0.16, bevel=0.0)


def pilasters(p, F, us, z0, z1, w=0.76):
    """As pilastras (o contraforte raso que sobe do soco até a cornija, sobre a junta: o ritmo vertical das muralhas do conceito)."""
    for u in us:
        wbox(p, 'Concrete#pil', F, u - w / 2, u + w / 2, z0, z1, -0.02, 0.32, bevel=0.06)


def banner(p, face, at, u, ztop, zbot, w=1.15):
    """A faixa vertical de pano na cor do jogador (Team) pendurada da cornija, com a onda leve, as barras de aço em cima e embaixo e a
    estrela das Forças Armadas (ru_star) no terço de cima."""
    F = WallFace(face, at, 0.0)
    s = 1 if face[0] == '+' else -1
    n = 0.13
    top = [F.pt(u - w / 2 + w * k / 8, ztop, n + 0.03 * math.sin(k * math.pi / 2.0)) for k in range(9)]
    bot = [F.pt(u - w / 2 + w * k / 8, zbot, n + 0.03 * math.sin(k * math.pi / 2.0)) for k in range(9)]
    top2 = [F.pt(u - w / 2 + w * k / 8, ztop, n + 0.02 + 0.03 * math.sin(k * math.pi / 2.0)) for k in range(9)]
    bot2 = [F.pt(u - w / 2 + w * k / 8, zbot, n + 0.02 + 0.03 * math.sin(k * math.pi / 2.0)) for k in range(9)]
    p.loft('Team#banner', [bot + list(reversed(bot2)), top + list(reversed(top2))], bevel=0.0)
    ax = 'y' if face[1] == 'x' else 'x'
    for zz in (ztop + 0.02, zbot - 0.05):
        p.cyl('Paint#bar', F.pt(u, zz, n + 0.04), ax, 0.04, w + 0.22, seg=8, bevel=0.0)
    for du in (-w / 2 - 0.05, w / 2 + 0.05):                                  # os ganchos na cornija
        wbox(p, 'Metal#hook', F, u + du - 0.03, u + du + 0.03, ztop, ztop + 0.25, 0.0, n + 0.06, bevel=0.0)
    ru_star(p, WallFace(face, at + s * (n + 0.07), 0.0), u, ztop - (ztop - zbot) * 0.3, 0.27 * w / 1.15, n=0.0, t=0.012, relief=0.25)


def keep(p):
    """O torreão: o bloco oco de painéis, a cornija, o parapeito e as ameias em volta; as seteiras, as pilastras do fundo, a porta do pátio,
    as faixas; o salão do portão vê a porta acesa na face da frente."""
    walls = ('+x', '-x', '+y', '-y')
    us_m = face_us(KEEP, walls, walls, '-x')
    us_s = face_us(KEEP, walls, walls, '-y')
    h_lo = slit_holes(us_m, [0, 3], 2.6, 3.8)
    h_back = slit_holes(us_m, [0, 1, 3], 5.55, 6.75)
    # a face −y: a parte de x > −1 dá pro pátio (a porta de enrolar, as seteiras); o resto fica atrás da ala direita
    roll = (1.7, 3.7, 0.26, 3.3)
    h_side = slit_holes(us_s, [0, 1, 2, 4], 5.55, 6.75)
    h_top = slit_holes(face_us(KEEP, walls, walls, '+y'), range(len(us_s) - 1), 5.75, 6.75)
    # (03/11, 3ª volta: os triângulos) o torreão em 3 blocos: embaixo de ZS (uma junta dos painéis, logo acima do teto das alas) as faces
    # ±Y que as alas escondem são parede lisa; os painéis só no fundo, na frente (o salão do portão) e na face do pátio (x > −1); em cima
    # de ZS, os painéis nas 4 faces. A junta entre os 2 andares de painel leva a massa escura como as outras
    x0, x1, y0, y1 = KEEP
    ZS, XS = 5.17, RWING[1]
    ru_block(p, x0, XS, y0, y1, Z0, ZS, t=T, walls=('-x', '+y', '-y'), skin=('-x',), pw=3.0, zs=[1.97, 3.57], parapet=0, roof=None,
             holes={'-x': h_lo}, interior=False, step=3.0)
    ru_block(p, XS + 0.003, x1, y0, y1, Z0, ZS, t=T, walls=('+x', '+y', '-y'), skin=('-y',), pw=3.0, zs=[1.97, 3.57], parapet=0,
             roof=None, holes={'-y': [roll]}, ukeep={'-y': [u for u in us_s if XS + 0.4 < u < x1 - 0.4]}, interior=False, step=3.0)
    ru_block(p, x0, x1, y0, y1, ZS + 0.08, KZ, t=T, pw=3.0, zs=[6.77], socle=0.0, parapet=0, roof='flat', roof_skip=(-6.4, 0.0, -3.2, 3.2),
             holes={'-x': h_back, '-y': h_side, '+y': h_top}, step=3.0)
    for (f_, at, a_, b_) in (('-x', x0, y0, y1), ('+x', x1, y0, y1), ('-y', y0, x0 - 0.08, x1 + 0.08), ('+y', y1, x0 - 0.08, x1 + 0.08)):
        wbox(p, JOINT, WallFace(f_, at, 0.0), a_, b_, ZS - 0.006, ZS + 0.086, -T + 0.012, 0.0, bevel=0.0)
    slit_backs(p, WallFace('-x', KEEP[0], 0.0), h_back + h_lo)
    slit_backs(p, WallFace('-y', KEEP[2], 0.0), h_side)
    slit_backs(p, WallFace('+y', KEEP[3], 0.0), h_top)
    cn_rollup(p, WallFace('-y', KEEP[2], 0.0), roll[0], roll[1], roll[2], roll[3], t=T)
    wbox(p, 'Hazard', WallFace('-y', KEEP[2], 0.0), roll[0] - 0.22, roll[0] - 0.04, 0.0, 1.4, 0.0, 0.12, bevel=0.0)
    wbox(p, 'Hazard', WallFace('-y', KEEP[2], 0.0), roll[1] + 0.04, roll[1] + 0.22, 0.0, 1.4, 0.0, 0.12, bevel=0.0)
    pilasters(p, WallFace('-x', KEEP[0], 0.0), [us_m[2]], Z0 + 0.62, KZ - 0.55)
    battlement(p, rect(KEEP), KZ, closed=True, tag='k')
    # a porta de dentro, acesa, no fundo do salão do portão (a face +x do torreão)
    Fi = WallFace('+x', KEEP[1], 0.0)
    wbox(p, 'Dark', Fi, -1.5, 1.5, 0.12, 3.0, -0.01, 0.02, bevel=0.0)
    wbox(p, 'Paint#door', Fi, -1.42, -0.02, 0.12, 2.9, 0.02, 0.08, bevel=0.012)
    wbox(p, 'Light', Fi, -1.3, 1.3, 3.08, 3.26, -0.01, 0.02, bevel=0.0)
    wbox(p, 'Metal#frame', Fi, -1.75, -1.5, 0.1, 3.32, -0.02, 0.12, bevel=0.012)
    wbox(p, 'Metal#frame', Fi, 1.5, 1.75, 0.1, 3.32, -0.02, 0.12, bevel=0.012)
    wbox(p, 'Metal#frame', Fi, -1.76, 1.76, 3.3, 3.55, -0.02, 0.12, bevel=0.012)
    # as faixas: no fundo (−x) e na face do pátio (−y)
    banner(p, '-x', KEEP[0], mids(us_m)[2] if len(us_m) > 3 else 0.0, KZ - 0.62, 3.2)
    banner(p, '-y', KEEP[2], mids(us_s)[3], KZ - 0.62, 3.6)


def roof_keep(p):
    """O teto do torreão: o gerador com o escape, as caixas de munição verdes, as saídas de ventilação, o alçapão, o mastro da antena."""
    z = KZ + 0.05
    # o gerador (a caixa de chapa com as venezianas) e o escape, na frente da torre à esquerda
    p.box('Paint#gen', 2.0, 3.4, -4.4, -2.1, z, z + 1.25, bevel=0.06)
    for k in range(5):
        yy = -4.15 + k * 0.42
        p.box('Dark', 3.4, 3.42, yy, yy + 0.3, z + 0.35, z + 1.0, bevel=0.0)
    p.cyl('Metal#exh', (2.3, -2.5, z + 1.75), 'z', 0.12, 1.0, seg=12, bevel=0.0)
    p.cyl('Dark', (2.3, -2.5, z + 2.26), 'z', 0.1, 0.02, seg=12, bevel=0.0)
    for (x, y, yaw, zz) in ((2.6, 2.4, 3, z), (2.62, 2.4, 6, z + 0.42), (2.7, 3.5, -4, z), (2.1, 4.6, 88, z)):
        ammo_crate(p, x, y, zz, yaw=yaw)
    vent_shaft(p, 2.8, 0.2, z, sx=0.9, sy=0.7, h=0.75)
    vent_shaft(p, -9.4, 2.4, z, sx=0.9, sy=0.7, h=0.75)
    ac_box(p, -9.3, 4.3, z, yaw=90)
    p.cyl('Concrete#hatchb', (-8.9, 0.4, z + 0.18), 'z', 0.6, 0.36, seg=24, bevel=0.0)
    vp.hatch_round(p, (-8.9, 0.4, z + 0.36), 0.45, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
    # o mastro treliçado da antena que gira (a antena é a peça _Radar, no topo)
    rx, ry, rz = RADAR_AT
    p.box('Concrete#mastb', rx - 0.6, rx + 0.6, ry - 0.6, ry + 0.6, z - 0.05, z + 0.25, bevel=0.04)
    lattice_mast(p, (rx, ry), z + 0.25, rz - 0.25, 0.5, 0.3, 4, m='Paint#mast', leg=0.06, brace=0.03)
    p.cyl('Metal#rtop', (rx, ry, rz - 0.15), 'z', 0.42, 0.3, seg=20, bevel=0.0)
    cable(p, (rx + 0.2, ry + 0.2, rz - 0.6), (-8.3, -1.5, z + 0.05), sag=0.4, r=0.025, m='Dark#cable')


def gate_block(p):
    """O bloco do portão: as paredes de painel com o vão de 6 × 4,9 m; o quadro de aço, a porta blindada levantada, as guias, o salão
    aceso; os pilones escalonados e as aletas; o medalhão com a estrela; a laje de aço da saída; as ameias."""
    walls = ('+x', '+y', '-y')
    us_y = face_us(GATEB, walls, walls, '-y')
    h_s, h_p = slit_holes(us_y, [1, 2], 3.5, 4.7), slit_holes(us_y, [2], 3.5, 4.7)
    ru_block(p, *GATEB, Z0, GZ, t=T, walls=walls, skin=walls, pw=3.0, zs=[1.97, 3.57, 5.17], parapet=0, roof='flat',
             holes={'+x': [(GATE[0], GATE[1], Z0, GATE[2])], '-y': h_s, '+y': h_p}, cols=[], step=3.0)
    slit_backs(p, WallFace('-y', GATEB[2], 0.0), h_s)
    slit_backs(p, WallFace('+y', GATEB[3], 0.0), h_p)
    F = WallFace('+x', GATEB[1], 0.0)
    y0, y1, zt = GATE
    # o quadro de aço (as ombreiras com o perigo no pé, a verga com o letreiro)
    for (a, b) in ((y1, y1 + 0.5), (y0 - 0.5, y0)):
        wbox(p, 'Paint#jamb', F, a, b, 0.0, zt + 0.05, -T, 0.32, bevel=0.04)
        wbox(p, 'Hazard', F, a - 0.01, b + 0.01, 0.05, 1.7, 0.3, 0.34, bevel=0.0)
        for zz in splits(0.6, zt - 0.4, 0.9):                                    # os parafusos grandes da ombreira
            p.cyl('Metal#bolt', F.pt((a + b) / 2, zz, 0.34), 'x', 0.05, 0.05, seg=8, bevel=0.0)
    wbox(p, 'Paint#jamb', F, y0 - 0.5, y1 + 0.5, zt, zt + 0.75, -T, 0.36, bevel=0.04)
    stencil(p, F, 0.0, zt + 0.2, 'ГЖЦ-01', h=0.34, m='White', n=0.36)
    # a porta blindada levantada (a borda de baixo com o perigo, recuada) e as guias de aço dos lados do vão
    wbox(p, 'Paint#door', F, y0 + 0.02, y1 - 0.02, zt - 0.55, zt + 0.02, -T + 0.06, -T + 0.24, bevel=0.03)
    wbox(p, 'Hazard', F, y0 + 0.04, y1 - 0.04, zt - 0.55, zt - 0.34, -T + 0.23, -T + 0.25, bevel=0.0)
    for (a, b) in ((y0, y0 + 0.16), (y1 - 0.16, y1)):
        wbox(p, 'Metal#guide', F, a, b, 0.0, zt, -T - 0.05, -T + 0.28, bevel=0.012)
    # o salão: o piso com as faixas, as luminárias no teto
    grid_box(p, 'Concrete#hall', GATEB[0] + 0.3, GATEB[1] - 0.3, GATEB[2] + 0.35, GATEB[3] - 0.35, Z0, 0.1, bevel=0.0)
    for y in (-2.6, 2.6):
        p.box('Hazard', GATEB[0] + 0.4, GATEB[1] - 0.4, y - 0.08, y + 0.08, 0.1, 0.115, bevel=0.0)
    for x in (7.0,):
        for y in (-1.6, 1.6):
            p.box('Dark#lamp', x - 0.7, x + 0.7, y - 0.2, y + 0.2, GZ - 0.86, GZ - 0.78, bevel=0.0)
            p.box('Light', x - 0.62, x + 0.62, y - 0.12, y + 0.12, GZ - 0.88, GZ - 0.86, bevel=0.0)
    # os pilones escalonados (o degrau com o lavadouro inclinado) e as aletas mais finas por fora
    for s in (-1, 1):
        a, b = sorted((s * 3.5, s * 4.7))
        p.prism_xz('Concrete#pylon', [(10.9, Z0), (12.3, Z0), (12.3, 1.8), (11.95, 2.25), (11.95, 4.0), (11.62, 4.45), (11.62, 6.6),
                                      (11.32, 7.05), (11.32, 7.55), (10.9, 7.55)], a, b, bevel=0.09)
        c, e = sorted((s * 5.0, s * 5.5))
        p.prism_xz('Concrete#fin', [(10.9, Z0), (11.7, Z0), (11.7, 3.1), (11.45, 3.45), (11.45, 5.5), (11.2, 5.85), (10.9, 5.85)], c, e,
                   bevel=0.07)
    # o medalhão no meio das ameias da frente: o disco de concreto, o anel branco, a estrela das Forças Armadas
    zc = GZ + 0.65 + 1.0
    p.box('Concrete#medb', 10.5, 11.05, -0.75, 0.75, GZ + 0.3, zc + 0.2, bevel=0.06)
    p.cyl('Concrete#med', (11.07, 0.0, zc), 'x', 1.05, 0.5, seg=36, bevel=0.06)
    p.torus('White#ring', (11.33, 0.0, zc), 'x', 0.93, 0.05, seg=40, rseg=6)
    ru_star(p, WallFace('+x', 11.32, 0.0), 0.0, zc - 0.03, 0.52, n=0.0, t=0.03, relief=0.25)
    # a laje de aço da saída (as chapas com as frestas, a beirada de perigo no fim do lote)
    for (ya, yb) in zip(splits(-3.4, 3.4, 1.15)[:-1], splits(-3.4, 3.4, 1.15)[1:]):
        p.box('Metal#apron', GATEB[1] + 0.02, 12.6, ya + 0.025, yb - 0.025, -0.1, 0.08, bevel=0.02)
    p.box('Hazard', 12.6, 12.9, -3.4, 3.4, -0.1, 0.07, bevel=0.0)
    battlement(p, [(GATEB[0], GATEB[2]), (GATEB[1], GATEB[2]), (GATEB[1], GATEB[3]), (GATEB[0], GATEB[3])], GZ, closed=False,
               ext=(-0.05, -0.05), skip=[(10.0, 12.5, -1.45, 1.45), (10.0, 12.5, 3.3, 4.9), (10.0, 12.5, -4.9, -3.3), (4.0, 7.75, 5.0, 6.6)],
               segs=[1, 2], corbel=False, tag='g')
    # o teto: as saídas de ventilação, o condensador
    z = GZ + 0.05
    vent_shaft(p, 9.7, -3.9, z, sx=0.9, sy=0.7, h=0.7)
    vent_shaft(p, 9.7, 3.9, z, sx=0.9, sy=0.7, h=0.7)
    ac_box(p, 8.3, -2.4, z, yaw=0)
    p.box('Paint#gen', 4.9, 6.3, -4.9, -2.6, z, z + 1.2, bevel=0.06)
    for k in range(5):
        yy = -4.7 + k * 0.42
        p.box('Dark', 6.3, 6.32, yy, yy + 0.3, z + 0.32, z + 0.95, bevel=0.0)
    p.cyl('Metal#exh', (5.2, -3.0, z + 1.65), 'z', 0.12, 0.9, seg=12, bevel=0.0)
    for (x, y, yaw, zz) in ((5.7, 3.0, 2, z), (5.72, 3.0, 6, z + 0.42), (5.8, 4.2, -3, z), (7.0, 4.4, 88, z)):
        ammo_crate(p, x, y, zz, yaw=yaw)
    p.cyl('Concrete#hatchb', (8.2, 1.4, z + 0.15), 'z', 0.55, 0.3, seg=24, bevel=0.0)
    vp.hatch_round(p, (8.2, 1.4, z + 0.3), 0.42, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')


def shed_bays(p, x_bays, y_lo, y_hi, z_lo, z_hi, gut=None):
    """Os telhados verdes de uma água (a chapa trapezoidal verde da família, o профнастил) em vãos ao longo de X, caindo de y_hi (alto,
    na muralha de fora) pra y_lo (baixo, a calha no pé da parede do torreão), com o rufo de chapa entre os vãos e a calha."""
    for (xa, xb) in x_bays:
        P0 = Vector((xa, y_hi, z_hi))
        Pa = Vector((xa, y_lo, z_lo))
        Pb = Vector((xb, y_hi, z_hi))
        prof_sheet(p, 'Roof', P0, Pa, Pb, period=0.36, depth=0.045, drip=0.05)
        # as terças de aço embaixo da beirada dos lados (o vão não flutua) e o rufo de cobertura nas bordas
        for xx in (xa + 0.05, xb - 0.05):
            tube(p, 'Paint#purl', [(xx, y_hi - 0.1, z_hi - 0.08), (xx, y_lo + 0.1, z_lo - 0.08)], 0.05, seg=6)
        for xx in (xa, xb):
            tube(p, 'Paint#ridge', [(xx, y_hi + 0.02, z_hi + 0.07), (xx, y_lo - 0.02, z_lo + 0.07)], 0.06, seg=6)
    ym = y_lo + 0.1 * (1 if y_hi > y_lo else -1)
    x0, x1 = min(b[0] for b in x_bays), max(b[1] for b in x_bays)
    x0, x1 = (x0, x1) if gut is None else gut
    p.cyl('Paint#gut', ((x0 + x1) / 2, ym, z_lo - 0.02), 'x', 0.09, x1 - x0, seg=10, bevel=0.0)
    # o rufo de encosto no alto (a chapa dobrada que sobe do telhado pro parapeito; o caimento fica em 6,38 m: com 6,4+ a grade do detalhe
    # cortava a chapa em 32 em vez de 16)
    sg = 1 if y_hi > y_lo else -1
    p.box('Paint#flash', x0, x1, *sorted((y_hi - sg * 0.08, y_hi + sg * 0.17)), z_hi + 0.03, z_hi + 0.07, bevel=0.0)


def wing(p, side):
    """A ala (esquerda +Y ou direita −Y): o bloco de painéis com a face do torreão aberta, as pilastras e as seteiras na muralha de fora,
    os telhados verdes em vãos, as ameias; na esquerda a porta do pátio da frente e a sala de rádio, na direita a porta pro pátio."""
    blk = LWING if side > 0 else RWING
    # (03/11, 3ª volta: os triângulos) sem a parede de dentro: a ala encosta na parede do torreão e do bloco do portão (a laje encosta nelas e
    # a calha corre no pé)
    walls = ('+x', '+y' if side > 0 else '-y', '-x')
    fo = '+y' if side > 0 else '-y'                                          # a muralha de fora
    sk_ = ('+x', fo, '-x')
    us_o = face_us(blk, walls, sk_, fo)
    us_b = face_us(blk, walls, sk_, '-x')
    h_o = slit_holes(us_o, [i for i in range(len(us_o) - 1) if i % 2 == 1], 2.4, 3.6)
    h_b = slit_holes(us_b, [0, len(us_b) - 2], 2.4, 3.6)
    holes = {fo: h_o, '-x': h_b}
    if side > 0:
        door = (6.55, 7.65, 0.26, 2.6)
        holes['+x'] = [door]
    else:
        door = (-7.6, -6.4, 0.26, 2.6)
        holes['+x'] = [door] + slit_holes(face_us(blk, walls, sk_, '+x'), [0], 2.4, 3.6)
    skin = ('+x', fo, '-x')
    ru_block(p, *blk, Z0, WZ, t=T, walls=walls, skin=skin, pw=3.0, zs=[1.97, 3.57], parapet=0, roof=None, holes=holes, interior=False,
             step=3.0)
    slit_backs(p, WallFace(fo, blk[3] if side > 0 else blk[2], 0.0), h_o)
    slit_backs(p, WallFace('-x', blk[0], 0.0), h_b)
    if side < 0:
        slit_backs(p, WallFace('+x', blk[1], 0.0), holes['+x'][1:])
    entry_door(p, WallFace('+x', blk[1], 0.0), door[0], door[1], door[2], door[3], t=T, canopy=1.2)
    # as pilastras da muralha de fora (nas juntas pares, fora dos baluartes)
    Fo = WallFace(fo, blk[3] if side > 0 else blk[2], 0.0)
    pil = [u for i, u in enumerate(us_o[1:-1]) if i % 2 == 0 and -10.0 < u < 6.0]
    pilasters(p, Fo, pil, Z0 + 0.62, WZ - 0.55)
    # as faixas
    if side > 0:
        banner(p, fo, blk[3], mids(us_o)[2], WZ - 0.62, 1.1)
        banner(p, '+x', blk[1], 8.85, WZ - 0.62, 1.1, w=0.95)
    else:
        banner(p, fo, blk[2], mids(us_o)[2], WZ - 0.62, 1.1)
    # o alto: as ameias (as pontas entram 5 cm na parede do torreão e do bloco do portão) e os telhados verdes
    x0, x1, y0, y1 = blk
    if side > 0:
        pts = [(x1, y0), (x1, y1), (x0, y1), (x0, y0)]
        sk = [grow(BAST['fl'][0], 0.1)]
        shed_bays(p, [(-10.15, -6.15), (-6.11, -2.11), (-2.07, 1.93), (1.97, 5.97), (6.01, 7.15)], y0 + 0.02, y1 - 0.6, WZ + 0.12,
                  WZ + 0.5,
                  gut=(-10.25, 7.25))
    else:
        pts = [(x0, y1), (x0, y0), (x1, y0), (x1, y1)]
        sk = []
        shed_bays(p, [(-10.15, -7.2), (-7.16, -4.4), (-4.36, -1.45)], y1 - 0.02, y0 + 0.6, WZ + 0.12, WZ + 0.5, gut=(-10.25, -1.35))
    battlement(p, pts, WZ, closed=False, ext=(0.15, 0.15), skip=sk, segs=[0, 1, 2] if side > 0 else [0, 1], corbel=False, tag='w')


def radio_room(p):
    """A sala de rádio no teto da ala esquerda: o bloco pequeno de painéis com a porta, o telhado verde de uma água, as antenas de chicote,
    a parabólica pequena e o cabo que desce."""
    x0, x1, y0, y1 = -3.4, 0.2, 8.6, 11.6
    z0, z1 = WZ - 0.05, WZ + 2.3
    ru_block(p, x0, x1, y0, y1, z0, z1, t=0.25, pw=1.6, zs=[z0 + 1.2], socle=0.0, parapet=0, roof='slab', interior=False,
             holes={'+x': [(9.5, 10.6, z0 + 0.1, z0 + 2.1)]})
    entry_door(p, WallFace('+x', x1, 0.0), 9.5, 10.6, z0 + 0.12, z0 + 2.1, t=0.25, canopy=0.0)
    prof_sheet(p, 'Roof', Vector((x0 - 0.25, y1 + 0.25, z1 + 0.45)), Vector((x0 - 0.25, y0 - 0.3, z1 + 0.06)),
               Vector((x1 + 0.25, y1 + 0.25, z1 + 0.45)), drip=0.05)
    for xx in (x0 + 0.2, x1 - 0.2):
        p.box('Paint#purl', xx - 0.06, xx + 0.06, y0, y1, z1, z1 + 0.06, bevel=0.0)
    for (ax, ay, hh) in ((x0 + 0.5, y1 - 0.4, 4.2), (x1 - 0.4, y1 - 0.5, 3.2), (x0 + 1.6, y1 - 0.3, 2.4)):
        vp.lathe(p, 'Metal#whipb', (ax, ay, z1 + 0.4), 'z', [(0.0, 0.0), (0.1, 0.0), (0.1, 0.1), (0.05, 0.14), (0.04, 0.35), (0.0, 0.37)],
                 seg=10)
        p.cyl('Dark#whip', (ax, ay, z1 + 0.77 + hh / 2), 'z', 0.015, hh, seg=6, bevel=0.0)
    # a parabólica pequena na parede +x (virada pro céu da frente)
    ex, ey, ez = vp.ypr_frame(20.0, 35.0, 0.0)
    cen = Vector((x1 + 0.55, 9.0, z0 + 1.7))
    R = 0.55
    vp.lathe(p, 'White#dish', cen, ex, [(0.0, -0.03), (R * 0.5, 0.0), (R * 0.85, 0.06), (R, 0.1), (R + 0.02, 0.09), (R * 0.85, 0.03),
                                       (R * 0.5, -0.03), (0.04, -0.06), (0.0, -0.06)], seg=24)
    tube(p, 'Metal#dbrk', [tuple(cen - ex * 0.05), (x1 + 0.02, 9.0, z0 + 1.6)], 0.04, seg=6)
    cable(p, (x0 + 0.5, y1 - 0.4, z1 + 0.5), (-5.5, 12.2, WZ + 0.7), sag=0.3, r=0.02, m='Dark#cable')


# ===================================================================================================== os baluartes
def bastion(p, key, flag=False):
    """O baluarte do canto: o quadrado de quina redonda em fiadas (a junta funda escura entre elas), o talude no pé, as seteiras, a cornija,
    o parapeito e as ameias; o teto de concreto com o alçapão; a bandeira no meio (flag)."""
    (x0, x1, y0, y1), zt = BAST[key]
    p.box('Concrete#bplinth', x0 - 0.3, x1 + 0.3, y0 - 0.3, y1 + 0.3, Z0, 0.55, bevel=0.15)
    zs = splits(0.55, zt, 1.75)
    for k, (za, zb) in enumerate(zip(zs[:-1], zs[1:])):
        p.box('Concrete#bast', x0, x1, y0, y1, za + 0.04, zb - (0.04 if k < len(zs) - 2 else 0.0), bevel=0.14)
        if k > 0:
            poly_ring(p, JOINT, rect((x0, x1, y0, y1)), -0.4, -0.06, za - 0.06, za + 0.06, bevel=0.0)
    # as seteiras (2 por face, na fiada de cima)
    if zt > 4.0:
        zz = zs[-3] + 0.3
        for (f, at, a, b) in (('+x', x1, y0, y1), ('-x', x0, y0, y1), ('+y', y1, x0, x1), ('-y', y0, x0, x1)):
            F = WallFace(f, at, 0.0)
            for u in (lerp(a, b, 0.32), lerp(a, b, 0.68)):
                wbox(p, 'Dark', F, u - 0.15, u + 0.15, zz, zz + 1.0, -0.02, 0.012, bevel=0.0)
                wbox(p, 'Metal#sill', F, u - 0.2, u + 0.2, zz - 0.06, zz, -0.02, 0.12, bevel=0.0)
    battlement(p, rect((x0, x1, y0, y1)), zt, closed=True, merlons=False, corbel=False, tag='b')
    vp.hatch_round(p, ((x0 + x1) / 2 - 0.9, (y0 + y1) / 2, zt + 0.0), 0.4, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
    if flag:
        flag_pole(p, (x0 + x1) / 2 + 0.3, (y0 + y1) / 2, zt, 5.2, cloth=(1.8, 1.15), phase=0.6 if key == 'fl' else 0.0)


# ===================================================================================================== a torre
def octo(c, R, rot=22.5):
    return [(c[0] + R * math.cos(math.radians(rot + 45.0 * k)), c[1] + R * math.sin(math.radians(rot + 45.0 * k))) for k in range(8)]


def octo_tier(p, spec, courses, tag, rib_w=0.5):
    """Um degrau octogonal em talude: as fiadas (a junta funda escura entre elas) e as nervuras salientes nas 8 quinas."""
    Rb, Rt, zb, zt = spec
    R = lambda z: lerp(Rb, Rt, (z - zb) / (zt - zb))
    zs = [lerp(zb, zt, k / courses) for k in range(courses + 1)]
    for k, (za, zz) in enumerate(zip(zs[:-1], zs[1:])):
        a, c = za + (0.04 if k else 0.0), zz - (0.04 if k < courses - 1 else 0.0)
        p.loft('Concrete#' + tag, [[(x, y, a) for (x, y) in octo(TC, R(a))], [(x, y, c) for (x, y) in octo(TC, R(c))]], bevel=0.1,
               caps=(False, False))
        if k:
            poly_ring(p, JOINT, octo(TC, R(za)), -0.4, -0.06, za - 0.06, za + 0.06, bevel=0.0)
    for k in range(8):
        a = math.radians(22.5 + 45.0 * k)
        rh, th = Vector((math.cos(a), math.sin(a), 0.0)), Vector((-math.sin(a), math.cos(a), 0.0))
        P = []
        for z in (zb + 0.02, zt - 0.5):
            for n in (-0.2, 0.14):
                for u in (-rib_w / 2, rib_w / 2):
                    q = Vector((TC[0], TC[1], 0.0)) + rh * (R(z) + n) + th * u
                    P.append((q.x, q.y, z))
        hexa(p, 'Concrete#rib' + tag, P, bevel=0.06)
    return R


def on_face(R, spec, k_face, z, n=0.0):
    """O centro na face k (a normal em 45 k graus) do degrau na altura z, saindo n; e o talude (graus) da face."""
    Rb, Rt, zb, zt = spec
    a = math.radians(45.0 * k_face)
    ap = R(z) * math.cos(math.radians(22.5))
    batter = math.degrees(math.atan((Rb - Rt) * math.cos(math.radians(22.5)) / (zt - zb)))
    return (TC[0] + (ap + n) * math.cos(a), TC[1] + (ap + n) * math.sin(a), z), 45.0 * k_face, batter


def tower(p):
    """A torre escalonada: o 1º degrau (as janelas de veneziana nas 4 faces retas, as seteiras nas diagonais, as ameias), o 2º degrau (as
    janelas altas, algumas acesas, as ameias menores), o tambor de aço escuro (as nervuras, a fita de janelas, a cornija)."""
    R1 = octo_tier(p, T1, 3, 't1')
    poly_ring(p, 'Concrete#t1top', octo(TC, T1[1]), -1.4, -0.02, T1[3] - 0.16, T1[3], bevel=0.0)
    for k in range(8):
        zm = (T1[2] + T1[3]) / 2 + 0.35
        if k % 2 == 0:
            c, yaw, bat = on_face(R1, T1, k, zm, 0.0)
            p.obox('Dark#t1w', c, (0.08, 1.9, 1.3), yaw=yaw, pitch=bat, bevel=0.0)
            c2, _, _ = on_face(R1, T1, k, zm, 0.06)
            for dz in (-0.42, -0.14, 0.14, 0.42):                              # a veneziana de aço
                cz = (c2[0], c2[1], c2[2] + dz)
                p.obox('Metal#louv', cz, (0.08, 1.8, 0.07), yaw=yaw, pitch=bat + 25.0, bevel=0.0)
            for (dy, w, h) in ((0.0, 2.2, 0.12),):
                cf, _, _ = on_face(R1, T1, k, zm - 0.72, 0.06)
                p.obox('Metal#t1f', cf, (0.18, w, h), yaw=yaw, pitch=bat, bevel=0.02)
                cf, _, _ = on_face(R1, T1, k, zm + 0.72, 0.06)
                p.obox('Metal#t1f', cf, (0.18, w, h), yaw=yaw, pitch=bat, bevel=0.02)
        else:
            for du in (-0.55, 0.55):
                c, yaw, bat = on_face(R1, T1, k, zm, 0.0)
                a = math.radians(yaw)
                cc = (c[0] - math.sin(a) * du, c[1] + math.cos(a) * du, c[2])
                p.obox('Dark#t1s', cc, (0.08, 0.26, 1.2), yaw=yaw, pitch=bat, bevel=0.0)
    battlement(p, octo(TC, T1[1]), T1[3], closed=True, corner=0.3, w=1.25, h=0.62, mt=0.5, gap=0.38, octagon=True, tag='t1')
    # o 2º degrau
    R2 = octo_tier(p, T2, 2, 't2', rib_w=0.4)
    poly_ring(p, 'Concrete#t2top', octo(TC, T2[1]), -1.3, -0.02, T2[3] - 0.16, T2[3], bevel=0.0)
    for k in range(8):
        c, yaw, bat = on_face(R2, T2, k, (T2[2] + T2[3]) / 2 - 0.05, 0.0)
        lit = k in (1, 4, 6)
        p.obox('Light' if lit else 'Glass', c, (0.08, 0.55, 1.25), yaw=yaw, pitch=bat, bevel=0.0)
        cf, _, _ = on_face(R2, T2, k, (T2[2] + T2[3]) / 2 - 0.05, 0.04)
        for dz in (-0.66, 0.66):
            p.obox('Metal#t2f', (cf[0], cf[1], cf[2] + dz), (0.12, 0.75, 0.1), yaw=yaw, pitch=bat, bevel=0.0)
        for du in (-0.32, 0.32):
            a = math.radians(yaw)
            p.obox('Metal#t2f', (cf[0] - math.sin(a) * du, cf[1] + math.cos(a) * du, cf[2]), (0.12, 0.09, 1.4), yaw=yaw, pitch=bat,
                   bevel=0.0)
    poly_ring(p, 'Concrete#corbt2', octo(TC, T2[1]), -0.2, 0.16, T2[3] - 0.45, T2[3] - 0.08, bevel=0.04)
    poly_ring(p, 'Concrete#bandt2', octo(TC, T2[1]), -0.32, 0.22, T2[3] - 0.08, T2[3] + 0.55, bevel=0.05)
    # o tambor de aço escuro (o pescoço da cebola)
    r, za, zb = DRUM
    p.cyl('Metal#drum', (TC[0], TC[1], (za + zb) / 2), 'z', r, zb - za, seg=40, bevel=0.0, smooth=True)
    for k in range(16):
        a = 2 * math.pi * k / 16
        p.obox('Paint#drib', (TC[0] + (r + 0.04) * math.cos(a), TC[1] + (r + 0.04) * math.sin(a), (za + zb) / 2 + 0.05),
               (0.14, 0.16, zb - za - 0.1), yaw=math.degrees(a), bevel=0.0)
        am = a + math.pi / 16
        lit = k % 4 == 1
        p.obox('Light' if lit else 'Glass', (TC[0] + (r + 0.005) * math.cos(am), TC[1] + (r + 0.005) * math.sin(am), zb - 0.95),
               (0.04, 0.42, 0.42), yaw=math.degrees(am), bevel=0.0)
    vp.lathe(p, 'Paint#cor', (TC[0], TC[1], zb - 0.1), 'z', [(r - 0.05, 0.0), (r + 0.12, 0.0), (r + 0.22, 0.12), (r + 0.22, 0.25),
                                                              (r - 0.05, 0.25)], seg=40, smooth=False)
    for k in range(4):                                                           # as luzes de obstáculo no tambor
        a = math.radians(45 + 90 * k)
        p.cyl('Beacon', (TC[0] + (r + 0.12) * math.cos(a), TC[1] + (r + 0.12) * math.sin(a), zb - 0.45), 'z', 0.07, 0.14, seg=10,
              bevel=0.0, smooth=True)


def catmull(pts, n=10):
    """O perfil suave pelos pontos (Catmull-Rom), n por trecho."""
    P = [pts[0]] + list(pts) + [pts[-1]]
    out = []
    for i in range(1, len(P) - 2):
        p0, p1, p2, p3 = P[i - 1], P[i], P[i + 1], P[i + 2]
        for k in range(n):
            t = k / n
            out.append(tuple(0.5 * ((2 * p1[j]) + (-p0[j] + p2[j]) * t + (2 * p0[j] - 5 * p1[j] + 4 * p2[j] - p3[j]) * t * t
                                    + (-p0[j] + 3 * p1[j] - 3 * p2[j] + p3[j]) * t ** 3) for j in range(2)))
    out.append(pts[-1])
    return out


def onion(p):
    """O RADOME EM CÚPULA DE CEBOLA: os painéis geodésicos brancos (os triângulos planos em fiadas alternadas, quase equiláteros, seguindo o
    perfil da cebola: 24 na barriga, 12 e 6 perto da ponta) com as costuras em relevo em todas as arestas; a ponta acaba no colar de aço."""
    prof = [(r * ONION_S, h * ONION_S) for (r, h) in catmull(ONION_PROF, 12)]
    S = [0.0]
    for a, b in zip(prof[:-1], prof[1:]):
        S.append(S[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))

    def at(s):
        for i in range(len(S) - 1):
            if S[i + 1] >= s:
                t = (s - S[i]) / max(S[i + 1] - S[i], 1e-9)
                return lerp(prof[i][0], prof[i + 1][0], t), lerp(prof[i][1], prof[i + 1][1], t)
        return prof[-1]
    rings = []                                                                   # [(n, fase, [pontos])]
    s, j = 0.0, 0
    while True:
        r, h = at(s)
        n = 20 if r > 1.6 else (10 if r > 0.8 else 5)
        ph = 0.5 * (j % 2)
        rings.append((n, ph, [(TC[0] + r * math.cos(2 * math.pi * (i + ph) / n), TC[1] + r * math.sin(2 * math.pi * (i + ph) / n),
                               ONION_Z0 + h) for i in range(n)]))
        if s >= S[-1] - 1e-6:
            break
        s = min(S[-1], s + 0.87 * 2 * math.pi * r / n)
        if S[-1] - s < 0.12:
            s = S[-1]
        j += 1
    b = p.bm('White#dome', 0.0)
    V_ = [[b.verts.new(V(*q)) for q in pts] for (_, _, pts) in rings]
    edges = set()

    def tri(a, c, d):
        f = b.faces.new([a, c, d])
        f.smooth = False
        for (u, v) in ((a, c), (c, d), (d, a)):
            edges.add((min(u.index, v.index), max(u.index, v.index)))
    b.verts.index_update()
    for (na, pa, _), (nb, pb, _), A, B in zip(rings[:-1], rings[1:], V_[:-1], V_[1:]):
        # o zíper entre 2 anéis (de quantos pontos for): avança o lado cujo próximo ângulo é menor
        ta = [2 * math.pi * (i + pa) / na for i in range(na + 1)]
        k0 = max(k for k in range(-nb, nb) if 2 * math.pi * (k + pb) / nb <= ta[0] + 1e-9)
        tb = [2 * math.pi * (k0 + k + pb) / nb for k in range(nb + 1)]
        i = k = 0
        while i < na or k < nb:
            if k >= nb or (i < na and ta[i + 1] <= tb[k + 1]):
                tri(A[i % na], A[(i + 1) % na], B[(k0 + k) % nb])
                i += 1
            else:
                tri(A[i % na], B[(k0 + k + 1) % nb], B[(k0 + k) % nb])
                k += 1
    # o fundo e a boca de cima fechados (o radome é casca: o interior não aparece)
    b.faces.new(list(reversed(V_[0])))
    b.faces.new(V_[-1])
    b.verts.ensure_lookup_table()
    # as costuras em relevo
    for (u, v) in edges:
        a_, c_ = b.verts[u].co, b.verts[v].co
        tube(p, 'White#seam', [(a_.x, -a_.y, a_.z), (c_.x, -c_.y, c_.z)], 0.034, seg=3, smooth=True)
    # o colar de aço na ponta, a agulha de treliça, a estrela
    rt, ht = ONION_PROF[-1][0] * ONION_S, ONION_PROF[-1][1] * ONION_S
    zt = ONION_Z0 + ht
    vp.lathe(p, 'Metal#fin', (TC[0], TC[1], zt - 0.12), 'z', [(0.0, 0.0), (rt + 0.06, 0.0), (rt + 0.08, 0.12), (0.2, 0.32), (0.16, 0.42),
                                                            (0.0, 0.42)], seg=20, smooth=False)
    zs = zt + 0.3
    lattice_mast(p, TC, zs, zs + 2.1, 0.32, 0.07, 4, m='Metal#spire', leg=0.045, brace=0.022, tri=False)
    disc_ring(p, 'Metal#spire', TC, 0.42, 0.12, zs + 0.85, zs + 0.9, seg=20)
    star_c = (TC[0], TC[1], zs + 2.1 + 0.62 * 0.5 * 1.53 * 0.809 / 0.62 - 0.04)
    spire_star(p, star_c, 0.5)
    return star_c


def spire_star(p, c, r, d=0.1):
    """A estrela da agulha (a das Forças Armadas, aprovada em 01/11: a vermelha com as bordas branca, azul e vermelha), dupla face (±X)."""
    cx, cy, cz = c
    R = r * 1.53
    pts = star_points(0.0, 0.0, R, R * 0.42, rot_deg=90.0)
    vp.prism(p, 'Red#score', [(u, v) for (u, v) in pts], Vector((cx - d / 2, cy, cz)), Vector((0, 1, 0)), Vector((0, 0, 1)),
             Vector((1, 0, 0)), d)
    for face, at in (('+x', cx + d / 2), ('-x', cx - d / 2)):
        ru_star(p, WallFace(face, at, 0.0), cy, cz, r, n=0.0, t=0.03, relief=0.22)


# ===================================================================================================== o pátio de obra
def yard(p):
    """O pátio de obra (−Y, na frente): o muro baixo de painéis com a capa e as ameias, o piso, as chapas de aço com a moldura de perigo e
    as 2 cruzes acesas de montagem, os batentes; a torre do guindaste (as fiadas, a escada, o anel de giro), o tanque vermelho."""
    # o muro: a face da frente (x = 11) e a do lado (y = −12,8), painéis de 0,7 m de espessura (sólidos)
    Ff = WallFace('+x', YARD_FRONT, 0.0)
    ru_panel_wall(p, Ff, cuts(-11.2, GATEB[2] - 0.01, 2.8), [Z0, 1.3, YZ], t=YT)
    Fs = WallFace('-y', YARD_SIDE, 0.0)
    ru_panel_wall(p, Fs, cuts(RWING[1] - 0.05, 9.8, 2.8), [Z0, 1.3, YZ], t=YT)
    wbox(p, 'Concrete#ycap', Ff, -11.2, GATEB[2], YZ - 0.02, YZ + 0.12, -YT - 0.06, 0.15, bevel=0.03)
    wbox(p, 'Concrete#ycap', Fs, RWING[1] - 0.05, 9.8, YZ - 0.02, YZ + 0.12, -YT - 0.06, 0.15, bevel=0.03)
    merlons_on(p, [(RWING[1], YARD_SIDE), (YARD_FRONT, YARD_SIDE), (YARD_FRONT, GATEB[2])], 0.12, YZ + 0.12, closed=False, end=0.3,
               skip=[grow(CRANE_TOWER, 0.1), (10.0, 12.5, -6.4, -5.0)], m='Concrete#merly')
    banner(p, '+x', YARD_FRONT, -6.75, YZ - 0.12, 0.55, w=1.0)
    # o piso do pátio e as chapas
    grid_box(p, 'Concrete#yard', RWING[1] + 0.03, YARD_FRONT - YT + 0.03, YARD_SIDE + YT - 0.03, KEEP[2] - 0.02, Z0, 0.06, bevel=0.0)
    px0, px1, py0, py1 = PAD
    xs, ys = splits(px0 + 0.3, px1 - 0.3, 1.9), splits(py0 + 0.3, py1 - 0.3, 1.25)
    for (xa, xb) in zip(xs[:-1], xs[1:]):
        for (ya, yb) in zip(ys[:-1], ys[1:]):
            p.box('Metal#pad', xa + 0.025, xb - 0.025, ya + 0.025, yb - 0.025, 0.04, 0.13, bevel=0.0)
    for (a, b, c, d) in ((px0, px1, py0, py0 + 0.3), (px0, px1, py1 - 0.3, py1), (px0, px0 + 0.3, py0 + 0.3, py1 - 0.3),
                         (px1 - 0.3, px1, py0 + 0.3, py1 - 0.3)):
        p.box('Hazard', a + (0.003 if c > py0 + 0.1 and d < py1 - 0.1 else 0.0), b, c, d, 0.04, 0.15, bevel=0.0)
    # as 2 cruzes acesas de montagem (onde a garra assenta as peças)
    for (cx, cy) in PAD_MARKS:
        p.box('Light#mark', cx - 0.55, cx + 0.55, cy - 0.06, cy + 0.06, 0.13, 0.145, bevel=0.0)
        p.box('Light#markb', cx - 0.06, cx + 0.06, cy - 0.55, cy + 0.55, 0.13, 0.143, bevel=0.0)
    for (x, y) in ((px0 + 0.15, py0 + 0.15), (px1 - 0.15, py0 + 0.15), (px0 + 0.15, py1 - 0.15), (px1 - 0.15, py1 - 0.15)):
        p.cyl('Hazard#boll', (x, y, 0.45), 'z', 0.13, 0.7, seg=12, bevel=0.0, smooth=True)
        p.cyl('Dark', (x, y, 0.81), 'z', 0.14, 0.03, seg=12, bevel=0.0)
    # a torre do guindaste no canto da frente (as fiadas como o bloco de canto; o anel de giro no topo; a escada na face de dentro do pátio)
    x0, x1, y0, y1 = CRANE_TOWER
    zt = CRANE_AT[2]
    p.box('Concrete#cplinth', x0 - 0.2, x1 + 0.2, y0 - 0.2, y1 + 0.2, Z0, 0.45, bevel=0.12)
    zs = splits(0.45, zt - 0.3, 1.6)
    for k, (za, zb) in enumerate(zip(zs[:-1], zs[1:])):
        p.box('Concrete#ctow', x0, x1, y0, y1, za + 0.04, zb - 0.04, bevel=0.12)
        if k:
            poly_ring(p, JOINT, rect((x0, x1, y0, y1)), -0.4, -0.06, za - 0.06, za + 0.06, bevel=0.0)
    p.box('Concrete#ctop', x0 - 0.15, x1 + 0.15, y0 - 0.15, y1 + 0.15, zt - 0.35, zt - 0.12, bevel=0.05)
    p.cyl('Metal#cring', (CRANE_AT[0], CRANE_AT[1], zt - 0.06), 'z', 1.18, 0.14, seg=36, bevel=0.0)
    ladder_on(p, WallFace('-x', x0, 0.0), y1 - 0.45, 0.2, zt - 0.3)
    wbox(p, 'Hazard', WallFace('-x', x0, 0.0), y1 - 1.6, y1 - 0.95, 0.45, 2.0, 0.0, 0.03, bevel=0.0)
    # as janelas estreitas da torre (a escada interna)
    for (f_, at, u) in (('+x', x1, (y0 + y1) / 2), ('-y', y0, (x0 + x1) / 2)):
        wbox(p, 'Dark', WallFace(f_, at, 0.0), u - 0.15, u + 0.15, 3.0, 4.0, -0.02, 0.012, bevel=0.0)
        wbox(p, 'Dark', WallFace(f_, at, 0.0), u - 0.15, u + 0.15, 5.2, 6.2, -0.02, 0.012, bevel=0.0)
    # o tanque vermelho (o diesel do pátio) com a base, as cintas, a escada
    tx, ty = 0.3, -11.2
    p.cyl('Concrete#tankb', (tx, ty, 0.15), 'z', 0.85, 0.3, seg=28, bevel=0.04)
    p.cyl('Red#tank', (tx, ty, 1.4), 'z', 0.75, 2.2, seg=28, bevel=0.0, smooth=True)
    vp.lathe(p, 'Dark#tank', (tx, ty, 2.5), 'z', [(0.75, 0.0), (0.7, 0.18), (0.45, 0.36), (0.12, 0.42), (0.0, 0.43)], seg=28)
    for zz in (0.75, 2.1):
        p.torus('White#tband', (tx, ty, zz), 'z', 0.76, 0.04, seg=28, rseg=6)
    ladder_on(p, WallFace('+x', tx + 0.75, 0.0), ty, 0.3, 2.6, w=0.4, cage=False, n=0.06)


# ===================================================================================================== o guindaste (gira)
def crane():
    """O GUINDASTE VERMELHO (03/11, 3ª volta: a LANÇA LEVANTADA, como o guindaste de treliça em repouso, pra o comprimento ler de qualquer
    giro na câmera do jogo; o braço deitado apontava pra câmera e virava um bloco). Origem no eixo do anel de giro; a lança sobe pra +X a
    LUFF graus. A mesa de giro, a casa de máquinas com a cabine envidraçada do lado, o contrapeso atrás, o cavalete (a pirâmide de 4 pernas)
    com os tirantes até a ponta, a LANÇA EM TRELIÇA DE CAIXA afinando (os 4 banzos, os quadros, as diagonais nas 4 faces), as polias da
    ponta, os 4 cabos descendo, o moitão vermelho com o perigo, o rotor e a GARRA de 4 dedos com os cilindros, pendurada sobre o pátio."""
    p = RuPart(PREFIX + '_Crane', cull=False)
    p.cyl('Metal#slew', (0.0, 0.0, 0.12), 'z', 1.2, 0.24, seg=36, bevel=0.0)
    p.cyl('Dark', (0.0, 0.0, 0.26), 'z', 1.05, 0.04, seg=36, bevel=0.0)
    # a casa de máquinas (as venezianas dos lados), a cabine do lado +Y e o contrapeso de concreto atrás
    # (a traseira e o contrapeso cabem no lote em todo o giro: o canto do lote fica a ~2,3 m do eixo)
    p.box('Red#house', -1.5, 1.1, -1.05, 1.05, 0.27, 2.05, bevel=0.08)
    for sg in (-1, 1):
        for k in range(3):
            xx = -1.25 + k * 0.4
            p.box('Dark#louv', xx, xx + 0.28, sg * 1.05 - 0.012, sg * 1.05 + 0.012, 0.8, 1.6, bevel=0.0)
    p.box('Red#cab', 0.15, 1.75, 1.05, 2.05, 0.4, 2.55, bevel=0.06)
    p.box('Glass', 1.73, 1.77, 1.15, 1.95, 1.35, 2.4, bevel=0.0)
    p.box('Glass', 0.25, 1.65, 2.03, 2.07, 1.35, 2.4, bevel=0.0)
    p.box('Paint#roofc', 0.1, 1.8, 1.0, 2.1, 2.55, 2.62, bevel=0.0)
    for k in range(2):
        xx = -1.95 + k * 0.24
        p.box('Concrete#cw', xx, xx + 0.2, -0.7, 0.7, 0.45, 2.0, bevel=0.05)
    p.box('Hazard', -1.97, -1.93, -0.6, 0.6, 0.5, 0.95, bevel=0.0)
    # o cavalete atrás do pé da lança, com a luz
    apex = Vector((-0.9, 0.0, 4.5))
    for q in ((-1.4, -0.9, 2.05), (-1.4, 0.9, 2.05), (0.2, -0.85, 2.05), (0.2, 0.85, 2.05)):
        tube(p, 'Red#aframe', [q, tuple(apex)], 0.1, seg=8)
    p.cyl('Red#apex', tuple(apex + Vector((0, 0, 0.08))), 'y', 0.2, 0.6, seg=12, bevel=0.0)
    p.cyl('Beacon', tuple(apex + Vector((0, 0, 0.38))), 'z', 0.08, 0.16, seg=10, bevel=0.0, smooth=True)
    # o pé da lança (as orelhas com o pino na frente da casa)
    foot = Vector((0.95, 0.0, 2.35))
    for sg in (-1, 1):
        p.box('Red#lug', 0.55, 1.35, sg * 0.62 - 0.08, sg * 0.62 + 0.08, 2.05, 2.75, bevel=0.03)
    p.cyl('Metal#pin', tuple(foot), 'y', 0.12, 1.5, seg=12, bevel=0.0)
    # a lança em treliça de caixa: afina de 1,2 × 1,24 m no pé pra 0,64 × 0,68 m na ponta
    a = math.radians(LUFF)
    dv, up, ey = Vector((math.cos(a), 0.0, math.sin(a))), Vector((-math.sin(a), 0.0, math.cos(a))), Vector((0.0, 1.0, 0.0))

    def C(t, sy, sz):
        return tuple(foot + dv * (BOOM_L * t) + ey * (sy * lerp(0.6, 0.32, t)) + up * (sz * lerp(0.62, 0.34, t)))
    for sy in (-1, 1):
        for sz in (-1, 1):
            tube(p, 'Red#chord', [C(0.0, sy, sz), C(1.0, sy, sz)], 0.1, seg=8)
    nb = 11
    ts = [k / nb for k in range(nb + 1)]
    for k, t in enumerate(ts):
        ring = [C(t, -1, -1), C(t, 1, -1), C(t, 1, 1), C(t, -1, 1)]
        for q in range(4):
            tube(p, 'Red#lace', [ring[q], ring[(q + 1) % 4]], 0.05, seg=6)
        if k < nb:
            t2 = ts[k + 1]
            al = k % 2 == 0
            for (sy, sz0, sz1) in ((-1, -1, 1), (1, -1, 1)):                     # as diagonais dos 2 lados
                tube(p, 'Red#lace', [C(t, sy, sz0 if al else sz1), C(t2, sy, sz1 if al else sz0)], 0.045, seg=6)
            for (sz, sy0, sy1) in ((-1, -1, 1), (1, -1, 1)):                     # e as de cima e de baixo
                tube(p, 'Red#lace', [C(t, sy0 if al else sy1, sz), C(t2, sy1 if al else sy0, sz)], 0.04, seg=6)
    # a ponta: as chapas da cabeça, as 2 polias grandes, os tirantes do cavalete
    tip = foot + dv * BOOM_L
    for sg in (-1, 1):
        p.obox('Red#headp', tuple(tip + ey * (sg * 0.36) + dv * 0.15), (0.9, 0.06, 0.9), pitch=LUFF, bevel=0.02)
    for dy in (-0.16, 0.16):
        p.cyl('Metal#sheave', tuple(tip + dv * 0.35 + ey * dy), 'y', 0.42, 0.1, seg=20, bevel=0.0)
    p.cyl('Metal#pin', tuple(tip + dv * 0.35), 'y', 0.07, 0.8, seg=10, bevel=0.0)
    for yy in (-0.32, 0.32):
        tube(p, 'Dark#tie', [tuple(apex + Vector((0, yy * 0.6, 0))), tuple(tip + ey * yy + up * 0.35)], 0.04, seg=5)
    # os 4 cabos da ponta até o moitão, o moitão (as polias, o perigo), o rotor e a garra
    hx = tip.x + 0.35 * dv.x + 0.42
    hz = HOOK_Z
    for (dx, dy) in ((-0.13, -0.11), (0.13, -0.11), (-0.13, 0.11), (0.13, 0.11)):
        tube(p, 'Dark#rope', [(hx + dx, dy, tip.z + 0.35 * dv.z - 0.05), (hx + dx, dy, hz + 0.62)], 0.03, seg=5)
    p.box('Red#hook', hx - 0.38, hx + 0.38, -0.3, 0.3, hz, hz + 0.68, bevel=0.05)
    for sg in (-1, 1):
        ya, yb = sorted((sg * 0.3, sg * 0.312))
        p.box('Hazard', hx - 0.3, hx + 0.3, ya, yb, hz + 0.08, hz + 0.32, bevel=0.0)
        p.cyl('Metal#hsheave', (hx, sg * 0.32, hz + 0.48), 'y', 0.2, 0.05, seg=16, bevel=0.0)
    p.cyl('Metal#swivel', (hx, 0.0, hz - 0.15), 'z', 0.14, 0.3, seg=12, bevel=0.0)
    p.cyl('Metal#rot', (hx, 0.0, hz - 0.38), 'z', 0.4, 0.2, seg=20, bevel=0.0)
    p.cyl('Red#hub', (hx, 0.0, hz - 0.62), 'z', 0.48, 0.3, seg=20, bevel=0.0)
    for k in range(4):
        a = math.radians(45 + 90 * k)
        ca, sa = math.cos(a), math.sin(a)

        def R(r, z):
            return (hx + r * ca, r * sa, hz + z)
        tine = [R(0.35, -0.64), R(0.95, -0.94), R(1.22, -1.52), R(1.06, -2.07), R(0.62, -2.3)]
        tube(p, 'Red#claw', tine[:-1], 0.13, seg=8)
        tube(p, 'Metal#tip', tine[-2:], 0.1, seg=8)
        tube(p, 'Metal#hyd', [R(0.22, -0.44), R(0.95, -1.12)], 0.055, seg=6)
        p.obox('Red#webc', R(0.88, -1.14), (0.48, 0.05, 0.6), yaw=math.degrees(a), pitch=-20, bevel=0.0)
    # o giro de repouso (a lança modelada em +X vira pra CRANE_YAW; no Blender o Y é o contrário: o giro também)
    Rm = Matrix.Rotation(math.radians(-CRANE_YAW), 3, 'Z')
    for b_ in p.bms.values():
        bmesh.ops.rotate(b_, verts=b_.verts, cent=(0.0, 0.0, 0.0), matrix=Rm)
    return p.finish()


# ===================================================================================================== a antena que gira
def radar():
    """A antena de vigilância (o painel de matriz branco, inclinado 10° pra trás, com a treliça atrás, a barra do IFF em cima): a mesa de
    giro, a caixa do receptor, a luz. Origem no eixo, no topo do mastro; a frente da antena pra +X em repouso."""
    p = RuPart(PREFIX + '_Radar', cull=False)
    p.cyl('Metal#turn', (0.0, 0.0, 0.1), 'z', 0.38, 0.2, seg=20, bevel=0.0)
    p.box('Paint#rbox', -0.4, 0.3, -0.32, 0.32, 0.2, 0.8, bevel=0.04)
    p.obox('White#array', (0.18, 0.0, 1.3), (0.16, 3.4, 0.82), pitch=10, bevel=0.03)
    for k in range(9):                                                            # as costuras do painel (as fileiras de antenas)
        yy = -1.6 + k * 0.4
        p.obox('Paint#seam', (0.27, yy, 1.3), (0.02, 0.025, 0.8), pitch=10, bevel=0.0)
    for yy in (-1.4, 0.0, 1.4):
        tube(p, 'Metal#truss', [(0.0, 0.0, 0.8), (0.05, yy, 1.0), (0.05, yy, 1.62)], 0.035, seg=6)
    tube(p, 'Metal#truss', [(0.05, -1.5, 1.0), (0.05, 1.5, 1.0)], 0.035, seg=6)
    tube(p, 'Metal#truss', [(-0.02, -1.5, 1.62), (-0.02, 1.5, 1.62)], 0.035, seg=6)
    p.obox('Metal#iff', (0.12, 0.0, 1.95), (0.1, 2.4, 0.14), bevel=0.0)
    for yy in (-0.9, 0.9):
        p.cyl('Metal#iffp', (0.08, yy, 1.82), 'z', 0.025, 0.18, seg=6, bevel=0.0)
    p.cyl('Beacon', (0.0, 0.0, 2.12), 'z', 0.07, 0.14, seg=10, bevel=0.0, smooth=True)
    return p.finish()


# ===================================================================================================== montagem
def forecourt(p):
    """O pátio da frente esquerda (entre o portão e o baluarte): o piso de lajes e as caixas de munição encostadas na ala."""
    xs, ys = splits(LWING[1] + 0.1, GATEB[1] - 0.02, 1.7), splits(GATEB[3] + 0.12, BAST['fl'][0][2] - 0.02, 1.75)
    for (xa, xb) in zip(xs[:-1], xs[1:]):
        for (ya, yb) in zip(ys[:-1], ys[1:]):
            p.box('Concrete#court', xa + 0.02, xb - 0.02, ya + 0.02, yb - 0.02, -0.1, 0.06, bevel=0.03)
    for (x, y, yaw, z) in ((9.3, 8.6, 0, 0.06), (9.32, 8.6, 5, 0.48), (10.7, 8.0, 88, 0.06)):
        ammo_crate(p, x, y, z, yaw=yaw)


def base():
    p = RuPart(PREFIX + '_Base')
    keep(p)
    roof_keep(p)
    gate_block(p)
    wing(p, 1)
    wing(p, -1)
    radio_room(p)
    for key in BAST:
        bastion(p, key, flag=True)
    flag_pole(p, 3.3, -4.95, KZ + 0.05, 5.4, cloth=(1.8, 1.15), phase=1.1)
    banner(p, '+x', BAST['fl'][0][1], 11.4, BAST['fl'][1] - 0.62, 2.0, w=0.95)
    tower(p)
    star_c = onion(p)
    yard(p)
    forecourt(p)
    return p.finish(), star_c


def write_preview():
    import json
    bl = lambda q: [q[0], -q[1], q[2]]
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]], [PREFIX + '_Crane', bl(CRANE_AT)], [PREFIX + '_Radar', bl(RADAR_AT)]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1400, 1400]], ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1600, 1100]],
                      ['side', 'ortho', [0, -1, 0], [0, 0, 1], [1600, 1100]],
                      ['three_quarter', 'persp', 34.0, 215.0, [1600, 1100], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 32.0, 35.0, [1600, 1100], 30.0, 0.95]],
            'usa_at': [16.2, 7.6, 0.0], 'tank_views': True,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8],
                     ['game_cam_high', 55.0, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, indent=1)


def main():
    os.makedirs(OUT, exist_ok=True)
    clear_scene()
    b, star_c = base()
    total = export(b, OUT)
    clear_scene()
    total += export(bake_in_place(crane(), CRANE_AT), OUT)
    clear_scene()
    total += export(bake_in_place(radar(), RADAR_AT), OUT)
    clear_scene()
    # (o andaime em volta de tudo, com os blocos de canto: passa 0,3 m do lote nos lados ±Y, só na obra)
    total += export(scaffold(PREFIX + '_Scaffold', -10.6, 11.8, -13.2, 13.2, KZ + 0.6, post='Metal', plank='Wood', stripe='Hazard',
                             gap=0.12), OUT)
    write_preview()
    print('MODEL total', total, 'tris')
    print('MODEL lote 26 x 28 m; portao (vao) x', GATEB[1], 'y', GATE[0], '..', GATE[1], 'altura', GATE[2], '; laje de saida ate x 12.9')
    a = math.radians(LUFF)
    hx = 0.95 + BOOM_L * math.cos(a) + 0.35 * math.cos(a) + 0.42
    cy, sy = math.cos(math.radians(CRANE_YAW)), math.sin(math.radians(CRANE_YAW))
    print('MODEL crane_at_m', CRANE_AT, 'giro em Z; em repouso a lanca aponta a', CRANE_YAW, 'graus; balanca de -10 a +20 graus; lanca', LUFF,
          'graus, ponta a', round(CRANE_AT[2] + 2.35 + BOOM_L * math.sin(a), 2), 'm; garra em', (round(CRANE_AT[0] + hx * cy, 2),
                                                                                          round(CRANE_AT[1] + hx * sy, 2)),
          'fundo a', round(CRANE_AT[2] + HOOK_Z - 2.4, 2), 'm')
    print('MODEL radar_at_m', RADAR_AT, 'giro em Z, 6 rpm')
    print('MODEL alturas: alas', WZ, 'portao', GZ, 'torreao', KZ, 'degraus', T1[3], T2[3], 'tambor', DRUM[2], 'cebola ate',
          round(ONION_Z0 + ONION_PROF[-1][1], 2), 'estrela ate', round(star_c[2] + 0.5 * 1.53, 2))
    print('MODEL fire_points_m', [(-6.5, 9.4, WZ + 0.5), (-6.4, -9.6, WZ + 0.5), (2.7, 3.4, KZ + 0.1), (round(TC[0] + 3.85, 2), 1.2, T1[3] + 0.7),
                                  (8.2, 2.2, GZ + 0.1)])


if __name__ == '__main__':
    main()
