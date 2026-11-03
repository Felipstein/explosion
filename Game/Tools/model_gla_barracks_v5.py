"""O Quartel da GLA (Barracks), v5 (03/11/2026): O QUARTEL DO ZERO HOUR, QUASE IGUAL, NO NOSSO DETALHE, COM O CAMPO DE TREINAMENTO.

O pedido (03/11, depois do pátio murado, da tenda, dos ônibus, das cúpulas em fila e da toca reprovados): "faz igual o command center da
USA [...] pega o barracks do C&C ZH, seja criativo, faz uma modificaçãozinha ou outra, e coloca esse campo de treinamento aí, um pátio".
O precedente: o Command Center v4 da USA (docs/elenco/USA_CC_E_USINA.md §5). A referência: docs/concept/buildings/ref_zh/zh_gla_barracks.jpg.
O v1 a v4 ficam como estão.

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_gla_barracks_v5.py captures/game/models/gla_barracks_v5

O que é do ZH (a composição, quase igual): o salão quadrado de adobe com o teto plano de parapeito em CRUZ (o corpo e os 3 braços) e a
CÚPULA DOURADA octogonal de gomos retos no meio, no tambor quadrado, com a ponta de lança (a peça mais alta e o ícone); o portão em arco
escuro com a grade de ferro (aberta: a infantaria sai por ele); a faixa verde dupla no alto das paredes; os 2 toldos listrados (no ombro
do salão e no anexo da direita); os anexos baixos de telhado inclinado de esteira (o da esquerda com a chaminé, o da direita com a caixa
de chapa no teto) e o galpão de chapa no fundo; o pátio de terra batida com a lona quadrada nos 4 postes e as caixas embaixo; o poço
octogonal com o sarilho; as colunas com os 2 anéis verdes; os muros baixos; as caixas.
O que é nosso (as 2 modificações, as 2 lidas de cima):
  1. A CÚPULA RACHADA E REMENDADA: a rachadura escura descendo 2 gomos e a LONA VERDE (a cor do jogador) amarrada por cima, as cordas
     presas na ponta de lança e nos sacos de areia do tambor. O ícone do ZH, agora da GLA da guerra.
  2. A TORRE DE VIGIA de toras no canto da frente (+X, +Y), com o teto de chapa, os sacos e a metralhadora no tablado e a BANDEIRA
     GRANDE no mastro acima do teto, vigiando o campo.
E o CAMPO DE TREINAMENTO no pátio, aberto pra frente (+X, sem muro na frente; os muros baixos só nos lados): do lado −Y a pista de
obstáculos em linha (as 6 duplas de pneus no chão, o muro de escalar de tábuas com a corda, a rede baixa de rastejar nas estacas); do
lado +Y o estande (os bonecos de saco nas estacas com o alvo verde na frente do barranco de terra, os 3 ninhos de sacos em C da linha
de tiro) e o CAVALETE DAS ARMAS (os AK e
os RPG encostados: o quartel faz o rebelde e o RPG). A faixa Y −3,3..3,3 fica livre do portão até a borda +X (as lajes de pedra só onde
a tropa pisa: a trilha do portão).

Lote 18 × 22 m (X × Y), frente em +X. A saída: a porta do portão em (−1,8; 0), o arco em (−0,2; 0), a faixa livre Y ±3,3 até X 9.
Alturas: o salão 5,2 m (o parapeito até ~5,7), o tambor 6,3 m, a cúpula até ~8,85 m e a ponta de lança até ~10,0 m; os anexos ~3,0–3,6 m;
a torre 6,1 m (o tablado em 4,0) e a bandeira até ~8,1 m.

Peças (origem no chão, no centro do lote; Unreal X frente, Y direita, Z cima, metros):
  SM_GLA_Barracks_Base: tudo (as bandeiras saem no flags.json; o jogo desenha o pano no vento);
  SM_GLA_Barracks_Scaffold: o andaime da obra em madeira.
Materiais (slots da GLA e os genéricos): Plaster (o adobe e a pedra), CnGold (a cúpula e a ponta), Player (o verde: as faixas, os anéis
das colunas, a lona do remendo, as bandeiras, os alvos), Wood, Cloth (a esteira dos telhados, a lona do pátio, os sacos, as listras
claras dos toldos), GlaRust (as listras laranja dos toldos), Rust, Metal, Rubber (os pneus), Dark, Trim, Crate, Earth (o chão batido),
Light, Hazard (só no andaime).
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, V, clear_scene, export, flag, scaffold  # noqa: E402,F401
from model_faction_kit import barrel, corrugated, disc_ring, revolve  # noqa: E402,F401
from model_building_parts import (WallFace, wbox, splits, tube, catenary, adobe_door, adobe_window, adobe_wall,  # noqa: E402,F401
                                  paint_band, rect_minus)
from model_building_parts import gla_wall_seg as wall_seg  # noqa: E402,F401
# o kit GLA dos prédios aprovados (o Palace v2: a peça, o arco, os sacos, o chão batido, o mastro, o pano livre, a fita)
from model_gla_palace_v2 import (new_part, finish, _hash, fbm, rod, prism, bag, bag_row, tire, flagpole, earth_pad, blob,  # noqa: E402,F401
                                 arch_fill, arch_band, arch_window, scrawl, crate, surface, ribbon, Frame, dshk)

PREFIX = 'SM_GLA_Barracks'
# as cores só da conferência (o jogo usa as instâncias dos slots): o laranja dos toldos é a GlaRust do import (linear 0,195/0,076/0,030)
COLORS.update({'GlaRust': (123, 78, 49), 'CnGold': (190, 150, 70), 'Broken': (170, 149, 118)})

LX, LY = 9.0, 11.0                    # o meio lote
HZ1 = 5.2                             # o topo das paredes do salão (a laje logo abaixo)
WT = 0.55                             # a espessura das paredes do salão
HC = (-5.0, 0.0)                      # o centro do corpo do salão (o eixo da cúpula)
# o contorno do salão (o teto em cruz do ZH): o corpo X −8,2..−1,8 × Y ±4,4, o braço do portão até X −0,2 (Y ±2,2) e os braços dos
# lados até Y ±5,2 (X −6,8..−3,4)
HALL = [(-0.2, -2.2), (-0.2, 2.2), (-1.8, 2.2), (-1.8, 4.4), (-3.4, 4.4), (-3.4, 5.2), (-6.8, 5.2), (-6.8, 4.4), (-8.2, 4.4),
        (-8.2, -4.4), (-6.8, -4.4), (-6.8, -5.2), (-3.4, -5.2), (-3.4, -4.4), (-1.8, -4.4), (-1.8, -2.2)]
ARCH_W, ARCH_SPRING = 2.2, 2.4        # o arco do portão (o topo do arco em 3,5)
DOOR_X = -1.8                         # a parede do fundo do portão (a porta de onde a tropa sai)
DRUM = 1.8                            # o meio lado do tambor quadrado
DOME_A, DOME_H = 1.62, 2.15           # o apótema do octógono da cúpula e a altura dos gomos
EXIT_HALF = 3.3


# ===================================================================================== o contorno retilíneo (as paredes, o rodapé)
def edges(P):
    """As arestas do contorno com a face pra fora, a coordenada da face, o intervalo ao longo dela e se cada ponta é quina pra fora
    (convexa) ou pra dentro."""
    n = len(P)
    A = 0.5 * sum(P[i][0] * P[(i + 1) % n][1] - P[(i + 1) % n][0] * P[i][1] for i in range(n))
    s = 1 if A > 0 else -1
    out = []
    for i in range(n):
        a, b, pv, nx_ = P[i], P[(i + 1) % n], P[i - 1], P[(i + 2) % n]
        ex, ey = b[0] - a[0], b[1] - a[1]
        L = math.hypot(ex, ey)
        dx, dy = ex / L, ey / L
        onx, ony = (dy, -dx) if s > 0 else (-dy, dx)
        ca = ((a[0] - pv[0]) * ey - (a[1] - pv[1]) * ex) * s > 0
        cb = (ex * (nx_[1] - b[1]) - ey * (nx_[0] - b[0])) * s > 0
        face = ('+x' if onx > 0 else '-x') if abs(onx) > 0.5 else ('+y' if ony > 0 else '-y')
        xf = face[1] == 'x'
        out.append({'a': a, 'b': b, 'face': face, 'at': a[0] if xf else a[1], 'ua': a[1] if xf else a[0], 'ub': b[1] if xf else b[0],
                    'ca': ca, 'cb': cb, 'n': (onx, ony), 'd': (dx, dy), 'side': -s, 'xf': xf})
    return out


def _span(e, convex_x=0.0, concave_x=0.0, convex_y=0.0, concave_y=0.0):
    """O intervalo ao longo da aresta com as pontas ajustadas (positivo = estica além do vértice)."""
    ua, ub = e['ua'], e['ub']
    sg = 1 if ub > ua else -1
    if e['xf']:
        ka = convex_x if e['ca'] else concave_x
        kb = convex_x if e['cb'] else concave_x
    else:
        ka = convex_y if e['ca'] else concave_y
        kb = convex_y if e['cb'] else concave_y
    ua -= sg * ka
    ub += sg * kb
    return ua, ub


def _pt(e, u):
    return (e['at'], u) if e['xf'] else (u, e['at'])


def outline_walls(p, P, z0, z1, t, holes, m='Plaster'):
    """As paredes de um contorno retilíneo (as faces ±x encurtam t nas quinas convexas, as ±y esticam t nas côncavas: a quina é de
    uma parede só e nada fica coplanar), com os vãos {(face, at): [(u0, u1, z0, z1)...]}. As pontas passam 4 mm da outra parede: o
    vértice nunca coincide (o remove_doubles do Part soldava as 2 caixas e o chanfro abria ponta)."""
    for e in edges(P):
        ua, ub = _span(e, convex_x=-(t - 0.004), concave_y=t + 0.004)
        F = WallFace(e['face'], e['at'], 0.0)
        hs = holes.get((e['face'], round(e['at'], 2)), [])
        for (a, b, c, d, lo, hi) in rect_minus(min(ua, ub), max(ua, ub), z0, z1, hs):
            wbox(p, m, F, a, b, c, d, -t, 0.0)


def outline_plinth(p, P, h=0.36, out=0.12, back=0.1, m='Plaster#plinth'):
    """O rodapé de pedra rebocada em volta do contorno (as faces ±x embrulham as quinas convexas, as ±y param atrás delas)."""
    for e in edges(P):
        ua, ub = _span(e, convex_x=out, convex_y=-back)
        F = WallFace(e['face'], e['at'], 0.0)
        wbox(p, m, F, min(ua, ub), max(ua, ub), -0.1, h, -back, out, bevel=0.06)


def outline_band(p, P, zl, zh, seed=0, wob=0.06, trim=0.16):
    """A faixa verde pintada à mão em volta do contorno (a ponta morre antes da quina gasta convexa)."""
    for k, e in enumerate(edges(P)):
        ua, ub = _span(e, convex_x=-trim, convex_y=-trim)
        if abs(ub - ua) < 0.3:
            continue
        paint_band(p, _pt(e, ua), _pt(e, ub), zl, zh, side=e['side'], off=0.0, m='Player', seed=seed + k, wob=wob)


def outline_parapet(p, P, z, h=0.5, t=0.42):
    """O parapeito de topo redondo acompanhando o contorno, por dentro da face (os segmentos passam meia espessura de cada vértice:
    a quina fica cheia)."""
    E = edges(P)
    d = t / 2
    n = len(E)
    inset = []
    for i in range(n):
        e0, e1 = E[i - 1], E[i]
        v = e1['a']
        inset.append((v[0] - d * (e0['n'][0] + e1['n'][0]), v[1] - d * (e0['n'][1] + e1['n'][1])))
    for i in range(n):
        a, b = inset[i], inset[(i + 1) % n]
        dx, dy = E[i]['d']
        adobe_wall(p, (a[0] - dx * d, a[1] - dy * d), (b[0] + dx * d, b[1] + dy * d), h, t=t, z0=z, m='Plaster#wall', batter=0.0, crown=0.6)


def inset_rect_band(p, x0, x1, y0, y1, zl, zh, trim=0.14, seed=0, wob=0.04):
    """A faixa verde em volta de um bloco retangular (o tambor), a ponta antes da quina gasta."""
    for (a, b, side) in (((x1, y0 + trim), (x1, y1 - trim), -1), ((x0, y0 + trim), (x0, y1 - trim), 1),
                         ((x0 + trim, y1), (x1 - trim, y1), 1), ((x0 + trim, y0), (x1 - trim, y0), -1)):
        paint_band(p, a, b, zl, zh, side=side, off=0.0, m='Player', seed=seed, wob=wob)
        seed += 1


# ===================================================================================== o salão
def hall(p):
    """O salão quadrado de adobe com o teto em cruz: as paredes com espessura, a laje, o piso de dentro, o rodapé, a faixa verde dupla,
    o parapeito de topo redondo, as janelas em arco, a porta de trás, o portão em arco com a grade de ferro aberta e a lanterna."""
    sp = ARCH_SPRING + ARCH_W / 2
    holes = {('+x', -0.2): [(-ARCH_W / 2, ARCH_W / 2, 0.0, sp)],
             ('+x', -1.8): [(u - 0.3, u + 0.3, 1.5, 2.7) for u in (-3.3, 3.3)],
             ('+y', 5.2): [(-5.1 - 0.35, -5.1 + 0.35, 1.4, 2.8)], ('-y', -5.2): [(-5.1 - 0.35, -5.1 + 0.35, 1.4, 2.8)],
             ('-x', -8.2): [(-0.7, 0.7, 0.0, 2.3)] + [(u - 0.35, u + 0.35, 1.5, 2.8) for u in (-2.8, 2.8)],
             ('+y', 4.4): [(-7.5 - 0.3, -7.5 + 0.3, 1.5, 2.7)], ('-y', -4.4): [(-7.5 - 0.3, -7.5 + 0.3, 1.5, 2.7)]}
    outline_walls(p, HALL, 0.0, HZ1, WT, holes)
    outline_plinth(p, HALL)
    # a parede do fundo do portão (a porta de onde a tropa sai), por baixo da laje
    Fd = WallFace('+x', DOOR_X, 0.0)
    for (a, b, c, d, lo, hi) in rect_minus(-2.2 + WT - 0.004, 2.2 - WT + 0.004, 0.0, HZ1 - 0.34, [(-0.95, 0.95, 0.0, 2.7)]):
        wbox(p, 'Plaster', Fd, a, b, c, d, -WT, 0.0)
    # as lajes do teto (sem sobrepor: o corpo, o braço do portão e os 2 braços dos lados) e o piso de dentro
    zt = HZ1 - 0.32
    for (x0, x1, y0, y1) in ((-7.7, -2.3, -3.9, 3.9), (-2.3, -0.7, -1.7, 1.7), (-6.3, -3.9, 3.9, 4.7), (-6.3, -3.9, -4.7, -3.9)):
        p.box('Plaster#slab', x0, x1, y0, y1, zt, HZ1 - 0.02, bevel=0.03)
    p.box('Plaster#slab', -7.7, -2.3, -3.9, 3.9, -0.05, 0.06, bevel=0.0)
    # o escuro de dentro atrás das portas (no jogo o vão lê fundo)
    wbox(p, 'Dark', Fd, -0.95, 0.95, 0.0, 2.7, -WT - 1.3, -WT - 1.27, bevel=0.0)
    Fb = WallFace('-x', -8.2, 0.0)
    adobe_door(p, Fb, -0.7, 0.7, 0.0, 2.3, t=WT)
    # as janelas em arco
    for u in (-3.3, 3.3):
        arch_window(p, -1.8, u, 1.5, '+x', w=0.6, h=1.2, t=WT)
    arch_window(p, -5.1, 5.2, 1.4, '+y', w=0.7, h=1.4, t=WT)
    arch_window(p, -5.1, -5.2, 1.4, '-y', w=0.7, h=1.4, t=WT)
    arch_window(p, -7.5, 4.4, 1.5, '+y', w=0.6, h=1.2, t=WT)
    arch_window(p, -7.5, -4.4, 1.5, '-y', w=0.6, h=1.2, t=WT)
    for u in (-2.8, 2.8):
        arch_window(p, -8.2, u, 1.5, '-x', w=0.7, h=1.3, t=WT)
    # a faixa verde dupla do ZH (a larga e a fina) e o parapeito
    outline_band(p, HALL, HZ1 - 0.98, HZ1 - 0.56, seed=3)
    outline_band(p, HALL, HZ1 - 1.26, HZ1 - 1.12, seed=21, wob=0.02)
    outline_parapet(p, HALL, HZ1 - 0.03, h=0.5, t=0.42)
    # as gárgulas de madeira (as calhas que saem do parapeito), 2 por lado comprido
    for (x, y, dx, dy) in ((-8.2, -2.0, -1, 0), (-8.2, 2.0, -1, 0), (-5.1, 5.2, 0, 1), (-5.1, -5.2, 0, -1)):
        p.obox('Wood#spout', (x + dx * 0.25, y + dy * 0.25, HZ1 - 0.12), (0.7 if dx else 0.16, 0.16 if dx else 0.7, 0.12), bevel=0.0)
    portal(p)
    # o alçapão do teto com a escadinha e o pote de barro (a vida no teto, pouca: o ZH tem o teto limpo, a cúpula manda)
    p.box('Wood#hatch', -7.3, -6.5, 2.5, 3.3, HZ1 - 0.02, HZ1 + 0.1, bevel=0.015)
    for y in (2.62, 3.18):
        rod(p, 'Wood#lad', (-6.6, y, HZ1 + 0.1), (-6.25, y, HZ1 + 1.0), 0.03)
    revolve(p, 'Plaster#jar', (-2.9, -3.3), [(0.1, HZ1 - 0.02), (0.24, HZ1 + 0.1), (0.3, HZ1 + 0.35), (0.25, HZ1 + 0.6), (0.14, HZ1 + 0.72),
                                             (0.15, HZ1 + 0.78)], seg=14)
    # as 2 bandeiras nas quinas do braço do portão
    for s in (-1, 1):
        flagpole(p, -0.41, s * 1.99, HZ1 + 0.47, 2.2, length=1.2, height=0.75, phase=0.4 + 1.1 * (s > 0))


def portal(p):
    """O portão do ZH: o arco no braço da frente (os tímpanos na espessura, a moldura, o anel verde, o alfiz), o fundo escuro com a
    porta, as 2 folhas da grade de ferro abertas pra dentro, a soleira de pedra e a lanterna pendurada."""
    x1 = -0.2
    F = WallFace('+x', x1, 0.0)
    arch_fill(p, 'Plaster#arch', (x1 - WT / 2, 0.0), 'x', ARCH_W, ARCH_SPRING, ARCH_SPRING + ARCH_W / 2 + 0.01, WT)
    arch_band(p, 'Plaster#s', (x1, 0.0), 'x', ARCH_W, ARCH_SPRING, 0.005, width=0.2, t=0.07, seg=18)
    arch_band(p, 'Player#arch', (x1, 0.0), 'x', ARCH_W + 0.46, ARCH_SPRING, 0.005, width=0.11, t=0.03, seg=18)
    for (a, b, c, d) in ((-1.95, -1.78, 0.36, 3.95), (1.78, 1.95, 0.36, 3.95), (-1.95, 1.95, 3.78, 3.95)):
        wbox(p, 'Plaster#s', F, a, b, c, d, -0.02, 0.06, bevel=0.0)
    # o batente de pedra nos 2 lados do arco (as pedras desencontradas)
    for s in (-1, 1):
        for k, z in enumerate(splits(0.05, ARCH_SPRING - 0.25, 0.42)[:-1]):
            w = 0.22 if k % 2 else 0.32
            u0, u1 = sorted((s * (ARCH_W / 2 - 0.02), s * (ARCH_W / 2 + w)))
            wbox(p, 'Plaster#stone', F, u0, u1, z, z + 0.38, -0.03, 0.05, bevel=0.03)
    p.box('Plaster#floor', DOOR_X - 0.02, x1 + 0.12, -ARCH_W / 2 - 0.4, ARCH_W / 2 + 0.4, -0.05, 0.05, bevel=0.02)
    # a porta do fundo: o batente e a verga de madeira, as 2 folhas de tábua abertas pra dentro
    Fd = WallFace('+x', DOOR_X, 0.0)
    for (a, b) in ((-1.05, -0.93), (0.93, 1.05)):
        wbox(p, 'Wood#frame', Fd, a, b, 0.0, 2.7, -0.12, 0.05, bevel=0.015)
    wbox(p, 'Wood#lintel', Fd, -1.25, 1.25, 2.7, 2.92, -0.3, 0.08, bevel=0.03)
    # a grade de ferro do portão (as 2 folhas abertas 75° pra dentro, presas no batente do arco)
    for s in (-1, 1):
        f = Frame(p, (x1 - 0.5, s * (ARCH_W / 2 - 0.06), 0.0), yaw=180.0 + s * 15.0)
        W = 0.98
        for z in (0.12, 1.2, 2.3):
            f.box('Trim#gate', (W / 2, 0.0, z), (W, 0.05, 0.06))
        for k in range(8):
            u = 0.06 + (W - 0.12) * k / 7
            f.rod('Trim#gate', (u, 0.0, 0.05), (u, 0.0, 2.36), 0.018)
        f.rod('Trim#gate', (0.0, 0.0, 0.0), (0.0, 0.0, 2.4), 0.035)
        f.rod('Trim#gate', (W, 0.0, 0.0), (W, 0.0, 2.4), 0.03)
        for z in (0.5, 1.75):                                                   # as volutas no meio (o ferro trabalhado)
            p.torus('Trim#gate', f.pt(W / 2, 0.0, z + 0.1), 'y', 0.16, 0.014, seg=14, rseg=4)
    # a lanterna pendurada no teto do portão
    q = (-1.15, 0.0, HZ1 - 0.32)
    rod(p, 'Dark', q, (q[0], q[1], 3.3), 0.01, seg=4)
    p.cyl('CnGold#lamp', (q[0], q[1], 3.16), 'z', 0.14, 0.3, seg=10, r2=0.08, bevel=0.0)
    p.cyl('Light', (q[0], q[1], 2.95), 'z', 0.1, 0.16, seg=10, bevel=0.0, smooth=True)


# ===================================================================================== a cúpula dourada e o remendo
def corner_r(t):
    """O raio do canto do octógono da cúpula na altura t (0 na base, 1 na ponta): a barriga leve e a ponta (o perfil do ZH)."""
    a = t * math.pi / 2
    r = DOME_A * (max(math.cos(a), 0.0) ** 1.05) * (1.0 + 0.12 * math.sin(math.pi * min(t / 0.9, 1.0)))
    return max(r, 0.025) / math.cos(math.pi / 8)


def dome_z(t, zb):
    return zb + DOME_H * math.sin(t * math.pi / 2) ** 0.85


def corner(j, t, zb, off=0.0):
    ang = math.radians(22.5 + 45.0 * j)
    r = corner_r(t) + off
    return (HC[0] + r * math.cos(ang), HC[1] + r * math.sin(ang), dome_z(t, zb))


def facet(k, u, t, zb, off=0.0):
    """O ponto no gomo k (centrado no ângulo 45k°: o 0 olha pra +X, o 7 pra +X −Y) em (u ao longo do gomo, t na altura), afastado 'off'
    pra fora pela normal do gomo."""
    a, b = corner(k - 1, t, zb), corner(k, t, zb)
    P = [a[i] + (b[i] - a[i]) * u for i in range(3)]
    if off:
        n = facet_normal(k, t, zb)
        P = [P[i] + n[i] * off for i in range(3)]
    return tuple(P)


def facet_normal(k, t, zb):
    ang = math.radians(45.0 * k)
    t0, t1 = max(0.0, t - 0.02), min(1.0, t + 0.02)
    dr = corner_r(t1) * math.cos(math.pi / 8) - corner_r(t0) * math.cos(math.pi / 8)
    dz = dome_z(t1, zb) - dome_z(t0, zb)
    L = math.hypot(dr, dz) or 1.0
    # a normal no plano radial: (dz, −dr) normalizada, girada pro ângulo do gomo
    nr, nz = dz / L, -dr / L
    return (nr * math.cos(ang), nr * math.sin(ang), nz)


def dome(p):
    """O tambor quadrado com as frestas e a faixa verde, a cornija, o colar dourado octogonal e a cúpula de 8 gomos retos com as
    nervuras nos cantos e a ponta de lança (a bola e o espigão)."""
    cx, cy = HC
    z0 = HZ1 - 0.05
    p.box('Plaster', cx - DRUM, cx + DRUM, cy - DRUM, cy + DRUM, z0, HZ1 + 1.0, bevel=0.13)
    p.box('Plaster#lo', cx - DRUM - 0.12, cx + DRUM + 0.12, cy - DRUM - 0.12, cy + DRUM + 0.12, HZ1 + 0.98, HZ1 + 1.12)
    inset_rect_band(p, cx - DRUM, cx + DRUM, cy - DRUM, cy + DRUM, HZ1 + 0.62, HZ1 + 0.86, seed=40)
    for f in ('+x', '-x', '+y', '-y'):
        at = (cx if f[1] == 'x' else cy) + (DRUM if f[0] == '+' else -DRUM)
        F = WallFace(f, at, 0.0)
        for u in (-0.75, 0.0, 0.75):
            uu = u + (cy if f[1] == 'x' else cx)
            wbox(p, 'Dark', F, uu - 0.09, uu + 0.09, HZ1 + 0.12, HZ1 + 0.5, -0.04, 0.012, bevel=0.0)
    zc = HZ1 + 1.12
    zb = zc + 0.38
    # o colar dourado (o octógono em pé) e o friso
    ring = lambda z, r: [(cx + r / math.cos(math.pi / 8) * math.cos(math.radians(22.5 + 45 * j)),
                          cy + r / math.cos(math.pi / 8) * math.sin(math.radians(22.5 + 45 * j)), z) for j in range(8)]
    p.loft('CnGold#collar', [ring(zc - 0.02, DOME_A + 0.05), ring(zb, DOME_A + 0.05)], bevel=0.0)
    p.loft('CnGold#rib', [ring(zb - 0.01, DOME_A + 0.1), ring(zb + 0.08, DOME_A + 0.1)], bevel=0.0)
    # os 8 gomos retos (a casca facetada)
    N = 16
    rings = [[corner(j, i / N, zb) for j in range(8)] for i in range(N + 1)]
    p.loft('CnGold#dome', rings, bevel=0.0, smooth=False)
    # as nervuras nos cantos
    for j in range(8):
        ang = math.radians(22.5 + 45 * j)
        tx, ty = -math.sin(ang), math.cos(ang)
        rr = []
        for i in range(12):
            t = 0.02 + 0.9 * i / 11
            x, y, z = corner(j, t, zb, off=0.0)
            w = 0.055 * (1.0 - 0.5 * i / 11)
            o = 0.05
            rr.append([(x - tx * w, y - ty * w, z), (x + tx * w, y + ty * w, z),
                       (x + tx * w + math.cos(ang) * o, y + ty * w + math.sin(ang) * o, z + o * 0.4),
                       (x - tx * w + math.cos(ang) * o, y - ty * w + math.sin(ang) * o, z + o * 0.4)])
        p.loft('CnGold#rib', rr, bevel=0.0)
    zt = zb + DOME_H
    revolve(p, 'CnGold', (cx, cy), [(0.02, zt - 0.2), (0.14, zt - 0.12), (0.12, zt + 0.02), (0.07, zt + 0.08), (0.2, zt + 0.22),
                                    (0.22, zt + 0.34), (0.18, zt + 0.46), (0.06, zt + 0.55), (0.045, zt + 0.62), (0.08, zt + 0.68),
                                    (0.035, zt + 0.74), (0.012, zt + 1.12), (0.004, zt + 1.18)], seg=12)
    return zb, zt


def dome_patch(p, zb, zt):
    """(nosso) A rachadura e o remendo: a rachadura escura descendo o gomo 0 (o da frente) e passando pro gomo 7, por baixo da LONA
    VERDE (a cor do jogador) amarrada no gomo 7 (o que a câmera do jogo vê), com as dobras e a barriga; as cordas de cima presas no
    pescoço da ponta de lança e as de baixo descendo até os sacos de areia no canto do tambor."""
    K = 7
    U0, U1, T0, T1 = 0.1, 0.9, 0.16, 0.66

    def fn(u, v):
        uu = U0 + (U1 - U0) * u
        tt = T0 + (T1 - T0) * v
        bul = 0.05 + 0.07 * math.sin(math.pi * u) * math.sin(math.pi * v) + 0.018 * math.sin(11.0 * u + 3.0 * v) * math.sin(math.pi * v)
        return facet(K, uu, tt, zb, off=bul)
    surface(p, 'Player#patch', fn, 12, 12, thick=0.03)
    # a bainha dobrada na borda de baixo (a lona puxada pela corda)
    ribbon(p, 'Player#patch', [facet(K, U0 + (U1 - U0) * i / 8, T0 + 0.01, zb, off=0.06) for i in range(9)], 0.09, t=0.035,
           upf=lambda q: facet_normal(K, T0, zb))
    # as cordas: de cima até o pescoço da ponta, de baixo até os sacos no canto do tambor
    neck = (HC[0], HC[1], zt + 0.05)
    for u in (U0 + 0.04, U1 - 0.04):
        pts = [facet(K, u + (0.5 - u) * (i / 6) ** 2, T1 + (0.97 - T1) * i / 6, zb, off=0.05) for i in range(7)] + [neck]
        tube(p, 'Wood#rope', pts, 0.022, seg=5, smooth=True)
    ang = math.radians(45.0 * K)
    sx, sy = HC[0] + 1.3 * math.cos(ang) * 1.414, HC[1] + 1.3 * math.sin(ang) * 1.414
    zr = HZ1 + 1.12
    for k, u in enumerate((U0 + 0.05, U1 - 0.05)):
        q0 = facet(K, u, T0 + 0.02, zb, off=0.06)
        q1 = facet(K, u, 0.01, zb, off=0.06)
        tgt = (sx + (k - 0.5) * 0.5 * math.sin(ang), sy - (k - 0.5) * 0.5 * math.cos(ang), zr + 0.22)
        tube(p, 'Wood#rope', [q0, q1, (q1[0] + (tgt[0] - q1[0]) * 0.5, q1[1] + (tgt[1] - q1[1]) * 0.5, zr + 0.42), tgt], 0.022, seg=5)
        bag(p, (tgt[0], tgt[1], zr + 0.12), (0.62, 0.36, 0.24), yaw=math.degrees(ang) + 90 + k * 14)
    # a rachadura: a linha quebrada escura (o vão aberto no dourado) descendo o gomo 0 e entrando por baixo da lona no gomo 7
    path0 = [(0.62, 0.86), (0.5, 0.78), (0.56, 0.7), (0.4, 0.6), (0.46, 0.52), (0.28, 0.44), (0.18, 0.4), (0.04, 0.33)]
    pts = [facet(0, u, t, zb, off=0.012) for (u, t) in path0] + [facet(K, 0.96, 0.3, zb, off=0.012), facet(K, 0.88, 0.26, zb, off=0.012)]
    for i in range(len(pts) - 1):
        a, b = pts[i], pts[i + 1]
        nrm = facet_normal(0 if i < len(path0) - 1 else K, 0.6, zb)
        w = 0.11 if 2 <= i <= 6 else 0.07
        ribbon(p, 'Dark#crack', [a, b], w, t=0.02, upf=lambda q, n=nrm: n)
    # os galhos finos da rachadura
    for (u0, t0, u1, t1) in ((0.4, 0.6, 0.3, 0.68), (0.46, 0.52, 0.62, 0.47), (0.28, 0.44, 0.33, 0.35), (0.5, 0.78, 0.66, 0.74)):
        ribbon(p, 'Dark#crack', [facet(0, u0, t0, zb, off=0.012), facet(0, u1, t1, zb, off=0.012)], 0.05, t=0.02,
               upf=lambda q: facet_normal(0, 0.6, zb))
    # as lascas de dourado levantadas na beira da rachadura (o metal rasgado pega o sol)
    for (u, t, s) in ((0.52, 0.74, 1), (0.42, 0.57, -1), (0.31, 0.46, 1)):
        c = facet(0, u + s * 0.04, t, zb, off=0.05)
        p.obox('CnGold#shard', c, (0.18, 0.025, 0.12), yaw=s * 30.0, pitch=25.0, roll=s * 15.0, bevel=0.0)


# ===================================================================================== os anexos e o galpão
def gable_walls(p, x0, x1, y0, y1, z_wall, z_lo, z_hi, holes, t=0.45, hi_side='+y'):
    """O anexo de telhado de uma água: as 4 paredes até z_wall com os vãos, a empena triangular nas 2 paredes ±x (a água cai ao longo
    de Y) e a faixa de parede do lado alto até z_hi."""
    P = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    outline_walls(p, P, 0.0, z_wall, t, holes)
    outline_plinth(p, P, h=0.3, out=0.1, back=0.08)
    ylo, yhi = (y0, y1) if hi_side == '+y' else (y1, y0)
    for (x, d) in ((x1, -t), (x0, t)):
        prism(p, 'Plaster', [(x, ylo, z_wall - 0.01), (x, yhi, z_wall - 0.01), (x, yhi, z_hi), (x, ylo, z_wall + 0.06)], (d, 0.0, 0.0))
    yy0, yy1 = sorted((yhi, yhi + (-t if hi_side == '+y' else t)))
    p.box('Plaster', x0 + t - 0.004, x1 - t + 0.004, yy0, yy1, z_wall - 0.012, z_hi, bevel=0.13)


def reed_roof(p, x0, x1, y0, y1, z_lo, z_hi, lo_side='-y', patch=None, seed=0):
    """O telhado de esteira (o amarelo-palha do ZH): a esteira ondulada de junco caindo pra lo_side, as varas por cima amarrando, os
    caibros de tronco saindo no beiral e a chapa de remendo."""
    if lo_side == '-y':
        corrugated(p, x0, x1, y0, y1, z_lo, z_hi, axis='y', m='Cloth#reed', pitch=0.13, amp=0.026, bend=0.0)
    else:
        corrugated(p, x0, x1, y0, y1, z_hi, z_lo, axis='y', m='Cloth#reed', pitch=0.13, amp=0.026, bend=0.0)
    zf = (lambda y: z_lo + (z_hi - z_lo) * (y - y0) / (y1 - y0)) if lo_side == '-y' else (lambda y: z_hi + (z_lo - z_hi) * (y - y0) / (y1 - y0))
    for y in splits(y0 + 0.35, y1 - 0.35, 1.1):
        rod(p, 'Wood#batten', (x0 - 0.05, y, zf(y) + 0.05), (x1 + 0.05, y, zf(y) + 0.05), 0.04, seg=6)
    for k, x in enumerate(splits(x0 + 0.3, x1 - 0.3, 0.9)):
        r = 0.07 * (0.9 + 0.2 * _hash(k, seed, 3))
        ya, yb = (y0 - 0.12, y1 - 0.4) if lo_side == '-y' else (y0 + 0.4, y1 + 0.12)
        rod(p, 'Wood#rafter', (x, ya, zf(ya) - 0.1), (x, yb, zf(yb) - 0.1), r, seg=7)
    if patch:
        (px0, px1, py0, py1) = patch
        corrugated(p, px0, px1, py0, py1, zf(py0) + 0.06, zf(py1) + 0.06, axis='y', m='Rust#sheet', pitch=0.16, amp=0.03, bend=0.0)


def annex_south(p):
    """O anexo da esquerda do ZH (aqui −Y): a casa baixa de telhado de esteira caindo pra fora, a chaminé (a fumaça do ZH), a porta de
    tábuas e a janela de veneziana viradas pro pátio, os rabiscos verdes, a luminária, os tambores e o pote."""
    x0, x1, y0, y1 = -8.8, -2.9, -10.7, -6.2
    holes = {('+x', -2.9): [(-7.3, -6.3, 0.0, 2.15), (-9.65, -8.85, 1.05, 2.0)], ('+y', -6.2): [(-5.4, -4.7, 1.1, 1.9)]}
    gable_walls(p, x0, x1, y0, y1, 2.6, 2.6, 3.36, holes, t=0.45, hi_side='+y')
    Fx = WallFace('+x', x1, 0.0)
    adobe_door(p, Fx, -7.3, -6.3, 0.0, 2.15, t=0.45, open_frac=0.4)
    adobe_window(p, Fx, -9.65, -8.85, 1.05, 2.0, t=0.45, shutter='Wood#shut')
    adobe_window(p, WallFace('+y', y1, 0.0), -5.4, -4.7, 1.1, 1.9, t=0.45)
    wbox(p, 'Dark', Fx, -7.3, -6.3, 0.0, 2.15, -0.45 - 1.0, -0.45 - 0.97, bevel=0.0)
    p.box('Plaster#slab', x0 + 0.4, x1 - 0.4, y0 + 0.4, y1 - 0.4, -0.05, 0.05, bevel=0.0)
    reed_roof(p, x0 - 0.35, x1 + 0.35, y0 - 0.3, y1 + 0.3, 2.62, 3.5, lo_side='-y', patch=(-5.2, -4.0, -9.4, -8.2), seed=1)
    # a chaminé de adobe com o cano de chapa e o chapéu (a fumaça do ZH sai daqui: smoke_points_m)
    cx, cy = -7.7, -7.0
    p.box('Plaster#chim', cx - 0.36, cx + 0.36, cy - 0.36, cy + 0.36, 2.3, 4.25, bevel=0.09)
    p.box('Plaster#lo', cx - 0.42, cx + 0.42, cy - 0.42, cy + 0.42, 4.2, 4.32)
    p.cyl('Rust#pipe', (cx, cy, 4.62), 'z', 0.13, 0.62, seg=12, bevel=0.0)
    p.cyl('Rust#pipe', (cx, cy, 5.02), 'z', 0.3, 0.06, seg=12, r2=0.08, bevel=0.0)
    for (dx, dy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        rod(p, 'Dark', (cx + dx * 0.08, cy + dy * 0.08, 4.88), (cx + dx * 0.2, cy + dy * 0.2, 4.98), 0.012, seg=4)
    p.box('Dark#soot', cx - 0.15, cx + 0.15, cy - 0.15, cy + 0.15, 4.3, 4.33, bevel=0.0)
    # a luminária de parede e os rabiscos
    q = Fx.pt(-7.9, 2.35, 0.0)
    rod(p, 'Dark', q, (q[0] + 0.3, q[1], q[2]), 0.015, seg=4)
    p.cyl('Light', (q[0] + 0.32, q[1], q[2] - 0.08), 'z', 0.07, 0.12, seg=8, bevel=0.0, smooth=True)
    scrawl(p, (x1 + 0.01, -8.0, 1.0), '+x', size=0.8, seed=31)
    scrawl(p, (-6.0, y0 - 0.01, 1.4), '-y', size=1.1, seed=32)
    for (x, y, m) in ((-2.45, -7.85, 'Rust'), (-2.4, -8.5, 'Metal'), (-2.6, -6.0, 'Rust')):
        barrel(p, x, y, 0.0, r=0.29, h=0.86, m=m)


def annex_north(p):
    """O anexo da direita do ZH (aqui +Y): a casa baixa de esteira com a CAIXA DE CHAPA no teto (a caixa-d'água do ZH no berço de
    madeira), o toldo listrado pro pátio, a porta e a janela, a parabólica velha."""
    x0, x1, y0, y1 = -4.0, 0.6, 7.0, 10.7
    holes = {('-y', 7.0): [(-2.7, -1.7, 0.0, 1.95), (-0.9, -0.2, 1.05, 1.85)], ('+x', 0.6): [(8.2, 9.0, 1.05, 1.9)]}
    gable_walls(p, x0, x1, y0, y1, 2.45, 2.45, 3.2, holes, t=0.45, hi_side='-y')
    Fy = WallFace('-y', y0, 0.0)
    adobe_door(p, Fy, -2.7, -1.7, 0.0, 1.95, t=0.45, open_frac=0.35)
    wbox(p, 'Dark', Fy, -2.7, -1.7, 0.0, 1.95, -1.4, -1.37, bevel=0.0)
    adobe_window(p, Fy, -0.9, -0.2, 1.05, 1.85, t=0.45, shutter='Wood#shut')
    adobe_window(p, WallFace('+x', x1, 0.0), 8.2, 9.0, 1.05, 1.9, t=0.45, grille=True)
    p.box('Plaster#slab', x0 + 0.4, x1 - 0.4, y0 + 0.4, y1 - 0.4, -0.05, 0.05, bevel=0.0)
    ry0, ry1 = y0 - 0.3, y1 + 0.3
    reed_roof(p, x0 - 0.3, x1 + 0.3, ry0, ry1, 2.48, 3.36, lo_side='+y', seed=2)
    rz = lambda y: 3.36 + (2.48 - 3.36) * (y - ry0) / (ry1 - ry0)
    # a caixa de chapa no berço (o cinza do ZH no teto do anexo)
    bx, by = -1.9, 8.3
    zr = rz(by)
    for (dx, dy) in ((-0.7, -0.5), (0.7, -0.5), (-0.7, 0.5), (0.7, 0.5)):
        zz = rz(by + dy)
        rod(p, 'Wood#post', (bx + dx, by + dy, zz), (bx + dx, by + dy, zr + 0.42), 0.06, seg=6)
    p.box('Wood#cradle', bx - 0.85, bx + 0.85, by - 0.62, by + 0.62, zr + 0.38, zr + 0.5, bevel=0.015)
    p.box('Metal#tank', bx - 0.78, bx + 0.78, by - 0.55, by + 0.55, zr + 0.5, zr + 1.35, bevel=0.04)
    for x in (bx - 0.4, bx + 0.4):
        p.box('Metal#tank', x - 0.03, x + 0.03, by - 0.57, by + 0.57, zr + 0.5, zr + 1.37, bevel=0.0)
    p.box('Rust#lid', bx - 0.2, bx + 0.25, by - 0.2, by + 0.2, zr + 1.35, zr + 1.42, bevel=0.0)
    # a parabólica velha no canto do teto
    p.cyl('Trim', (0.15, 10.2, 3.0), 'z', 0.035, 0.8, seg=6, bevel=0.0)
    p.cyl('Metal#dish', (0.1, 10.15, 3.45), (-0.6, -0.4, 0.7), 0.42, 0.04, seg=18, bevel=0.0)
    # o toldo listrado virado pro pátio (−Y)
    awning(p, (-3.7, y0), (0.35, y0), (0.0, -1.0), 1.65, 2.55, 2.02, n=8, poles=(0.2, 0.8))
    scrawl(p, (x1 + 0.01, 7.9, 1.2), '+x', size=0.8, seed=35)


def back_shed(p):
    """O galpão de chapa do fundo do ZH (+Y, atrás): o canto de muro de adobe, os 4 esteios de tronco, o teto de chapa ondulada cinza
    com os remendos de ferrugem, e embaixo as pilhas de caixotes, os sacos e os pneus de reserva."""
    x0, x1, y0, y1 = -8.8, -4.7, 6.0, 10.7
    wall_seg(p, (x0 + 0.25, y1 - 0.25), (x1, y1 - 0.25), 2.3, t=0.5, z0=0.0, band=False, round_top=False)
    wall_seg(p, (x0 + 0.25, y0 + 0.2), (x0 + 0.25, y1 - 0.25), 2.3, t=0.5, z0=0.0, band=False, round_top=False)
    ry0, ry1 = y0 - 0.35, y1 + 0.1
    rz = lambda y: 3.02 + (2.58 - 3.02) * (y - ry0) / (ry1 - ry0)
    for (x, y) in ((x1 - 0.1, y0 + 0.15), (-6.75, y0 + 0.15), (x0 + 0.3, y0 + 0.15), (x1 - 0.1, y1 - 0.45)):
        rod(p, 'Wood#post', (x, y, -0.1), (x, y, rz(y) - 0.2), 0.1, seg=8)
    rod(p, 'Wood#beam', (x0 + 0.1, y0 + 0.15, rz(y0 + 0.15) - 0.17), (x1 + 0.1, y0 + 0.15, rz(y0 + 0.15) - 0.17), 0.09, seg=8)
    corrugated(p, x0 - 0.1, x1 + 0.35, ry0, ry1, 3.02, 2.58, axis='y', m='Metal#sheet', pitch=0.2, amp=0.04, bend=0.08)
    corrugated(p, -7.6, -6.4, 7.0, 8.6, rz(7.0) + 0.05, rz(8.6) + 0.05, axis='y', m='Rust#sheet', pitch=0.17, amp=0.035, bend=0.0)
    for (x, y, n, yaw) in ((-7.6, 9.7, 3, 4.0), (-6.3, 9.75, 2, -6.0), (-7.75, 8.2, 2, 90.0)):
        for k in range(n):
            crate(p, x + 0.04 * (k % 2), y, k * 0.5, size=(1.1, 0.7, 0.5), yaw=yaw + k * 3)
    for (x, y) in ((-5.6, 9.9), (-5.15, 9.5)):
        barrel(p, x, y, 0.0, r=0.29, h=0.86, m='Rust')
    for k in range(3):
        tire6(p, (-5.5, 7.4, 0.13 + k * 0.25), R=0.36, r=0.13)
    bag_row(p, [(-7.6, 6.7), (-6.0, 6.7)], 0.0, layers=2, bag_size=(0.6, 0.34, 0.22), seed=51)


def awning(p, a, b, out, depth, z_hi, z_lo, n=7, sag=0.1, poles=(0.0, 1.0)):
    """O toldo listrado (o laranja e o cru do ZH, as listras correndo da parede pra beira): as tiras com a barriga, a sanefa recortada
    na beira, a vara da parede e a da frente, os paus até o chão nas frações 'poles' da beira."""
    ax, ay = a
    bx, by = b
    ox, oy = out
    for k in range(n):
        u0, u1 = k / n, (k + 1) / n
        pa = (ax + (bx - ax) * u0, ay + (by - ay) * u0)
        pb = (ax + (bx - ax) * u1, ay + (by - ay) * u1)
        m = 'Cloth#aw' if k % 2 == 0 else 'GlaRust#aw'
        rings = []
        N = 8
        for i in range(N + 1):
            t = i / N
            z = z_hi + (z_lo - z_hi) * t - sag * math.sin(math.pi * t)
            q0 = (pa[0] + ox * depth * t, pa[1] + oy * depth * t)
            q1 = (pb[0] + ox * depth * t, pb[1] + oy * depth * t)
            rings.append([(q0[0], q0[1], z - 0.012), (q1[0], q1[1], z - 0.012), (q1[0], q1[1], z + 0.012), (q0[0], q0[1], z + 0.012)])
        p.loft(m, rings, bevel=0.0)
        # a aba da sanefa (o arco embaixo da beira)
        e0 = (pa[0] + ox * depth, pa[1] + oy * depth)
        e1 = (pb[0] + ox * depth, pb[1] + oy * depth)
        em = ((e0[0] + e1[0]) / 2, (e0[1] + e1[1]) / 2)
        q15 = (e0[0] + (e1[0] - e0[0]) * 0.15, e0[1] + (e1[1] - e0[1]) * 0.15)
        q85 = (e0[0] + (e1[0] - e0[0]) * 0.85, e0[1] + (e1[1] - e0[1]) * 0.85)
        prism(p, m, [(e0[0], e0[1], z_lo), (e1[0], e1[1], z_lo), (q85[0], q85[1], z_lo - 0.22), (em[0], em[1], z_lo - 0.3),
                     (q15[0], q15[1], z_lo - 0.22)], (ox * 0.02, oy * 0.02, 0.0))
    ea = (ax + ox * depth, ay + oy * depth)
    eb = (bx + ox * depth, by + oy * depth)
    rod(p, 'Wood#aw', (ea[0], ea[1], z_lo + 0.02), (eb[0], eb[1], z_lo + 0.02), 0.04)
    rod(p, 'Wood#aw', (ax + ox * 0.02, ay + oy * 0.02, z_hi + 0.03), (bx + ox * 0.02, by + oy * 0.02, z_hi + 0.03), 0.035)
    for f in poles:
        x, y = ea[0] + (eb[0] - ea[0]) * f, ea[1] + (eb[1] - ea[1]) * f
        rod(p, 'Wood#aw', (x - ox * 0.03, y - oy * 0.03, -0.05), (x - ox * 0.03, y - oy * 0.03, z_lo + 0.05), 0.045)


# ===================================================================================== o pátio do ZH
def courtyard(p):
    """O chão batido do pátio (a lente que acompanha o campo, sem laje), as lajes de pedra só na trilha do portão, o toldo listrado do
    ombro do salão, o poço octogonal com o sarilho, a lona quadrada nos 4 postes com as caixas, os caixotes soltos."""
    earth_pad(p, (4.0, 0.0), blob(4.0, 0.0, 5.1, 10.75, seed=4, amp=0.05), h=0.05, step=0.55, arc=0.6)
    # a trilha de lajes do portão (some aos poucos na terra: a tropa pisa ali)
    for i in range(6):
        x = 0.2 + i * 0.66
        for j, y in enumerate((-0.37, 0.37) if i % 2 else (-0.74, 0.0, 0.74)):
            if _hash(i, j, 77) > 1.05 - i / 6.5:
                continue
            w = 0.58 + 0.08 * _hash(i, j, 78)
            d = 0.66 + 0.06 * _hash(i, j, 79)
            p.obox('Plaster#flag', (x + 0.05 * (_hash(i, j, 80) - 0.5), y, 0.035), (w, d, 0.09), yaw=(_hash(i, j, 81) - 0.5) * 8, bevel=0.03)
    # o toldo listrado no ombro −Y do salão (o do ZH à direita do portão): os paus fora da faixa da saída
    awning(p, (-1.8, -2.35), (-1.8, -4.3), (1.0, 0.0), 1.75, 3.15, 2.5, n=7, poles=(0.53, 0.97))
    well(p, 1.95, 5.0)
    canopy(p)
    crate(p, 6.3, -4.4, 0.0, size=(0.9, 0.6, 0.5), yaw=12)
    crate(p, -2.2, 6.3, 0.0, size=(0.8, 0.55, 0.45), yaw=-8)


def well(p, x, y):
    """O poço do ZH: o bocal octogonal de pedra rebocada com a borda de pedras, a água escura lá embaixo, o sarilho de 2 forquilhas com
    a corda e o balde, o anel de pedras no chão em volta e o pote de barro."""
    ro, ri = 0.86, 0.6
    ring = lambda z, r: [(x + r / math.cos(math.pi / 8) * math.cos(math.radians(22.5 + 45 * j)),
                          y + r / math.cos(math.pi / 8) * math.sin(math.radians(22.5 + 45 * j)), z) for j in range(8)]
    for j in range(8):
        a0, a1 = math.radians(22.5 + 45 * j), math.radians(22.5 + 45 * (j + 1))
        c0 = 1 / math.cos(math.pi / 8)
        pts = [(x + ro * c0 * math.cos(a0), y + ro * c0 * math.sin(a0)), (x + ro * c0 * math.cos(a1), y + ro * c0 * math.sin(a1)),
               (x + ri * c0 * math.cos(a1), y + ri * c0 * math.sin(a1)), (x + ri * c0 * math.cos(a0), y + ri * c0 * math.sin(a0))]
        p.loft('Plaster#well', [[(px, py, -0.05) for (px, py) in pts], [(px, py, 0.72) for (px, py) in pts]], bevel=0.0)
    p.loft('Plaster#wcap', [ring(0.7, ro + 0.06), ring(0.82, ro + 0.06)], bevel=0.0)
    p.loft('Glass#water', [ring(0.3, ri + 0.02), ring(0.33, ri + 0.02)], bevel=0.0)
    p.cyl('Dark', (x, y, 0.2), 'z', ri, 0.2, seg=8, bevel=0.0)
    for k in range(8):
        a = math.radians(45 * k + 10)
        p.obox('Plaster#stone', (x + 1.08 * math.cos(a), y + 1.08 * math.sin(a), 0.05), (0.42, 0.3, 0.12), yaw=math.degrees(a) + 90,
               bevel=0.03)
    # o sarilho: as 2 forquilhas (no eixo Y, fora da faixa da saída), o rolo com a corda e a manivela, o balde pendurado
    for s in (-1, 1):
        yy = y + s * 0.98
        rod(p, 'Wood#post', (x, yy, -0.1), (x, yy, 1.45), 0.07, seg=7)
        rod(p, 'Wood#post', (x, yy, 1.3), (x + 0.12, yy, 1.62), 0.04, seg=5)
        rod(p, 'Wood#post', (x, yy, 1.3), (x - 0.12, yy, 1.62), 0.04, seg=5)
    p.cyl('Wood#roll', (x, y, 1.42), 'y', 0.11, 1.9, seg=10, bevel=0.0)
    p.torus('Wood#rope', (x, y, 1.42), 'y', 0.13, 0.03, seg=12, rseg=4)
    rod(p, 'Dark', (x, y + 1.0, 1.42), (x + 0.3, y + 1.05, 1.42), 0.02, seg=4)
    rod(p, 'Wood#rope', (x + 0.13, y, 1.42), (x + 0.13, y, 0.92), 0.012, seg=4)
    p.cyl('Metal#bucket', (x + 0.13, y, 0.8), 'z', 0.15, 0.24, seg=10, r2=0.12, bevel=0.0)
    revolve(p, 'Plaster#jar', (x + 1.0, y + 0.75), [(0.1, 0.0), (0.25, 0.1), (0.32, 0.38), (0.26, 0.66), (0.14, 0.78), (0.15, 0.84)], seg=14)


def canopy(p):
    """A lona quadrada do ZH nos 4 postes (aqui no lado −Y do pátio, os postes fora da faixa da saída): os postes de tronco, as varas de
    cima, a lona com as dobras que saem dos cantos amarrados e a barriga, as caixas escuras embaixo (as do ZH) e o banco."""
    from model_building_parts import tarp
    x0, x1, y0, y1 = 0.35, 3.75, -7.1, -3.85
    h = 2.85
    for (x, y) in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
        rod(p, 'Wood#post', (x, y, -0.1), (x, y, h + 0.12), 0.085, seg=8)
    for (a, b) in (((x0, y0), (x1, y0)), ((x0, y1), (x1, y1)), ((x0, y0), (x0, y1)), ((x1, y0), (x1, y1))):
        rod(p, 'Wood#rail', (a[0], a[1], h), (b[0], b[1], h), 0.055, seg=7)
    tarp(p, [(x0 - 0.12, y0 - 0.12, h + 0.08), (x1 + 0.12, y0 - 0.12, h + 0.08), (x1 + 0.12, y1 + 0.12, h + 0.08), (x0 - 0.12, y1 + 0.12, h + 0.08)],
         m='Cloth#tarp', sag=0.2, folds=3, amp=0.06, thick=0.03, seed=5)
    for (x, y, yaw) in ((1.55, -5.85, 8.0), (2.45, -5.95, -12.0)):
        crate(p, x, y, 0.0, size=(0.8, 0.55, 0.42), yaw=yaw, m='Crate', strap='Dark')
    crate(p, 1.95, -5.9, 0.42, size=(0.7, 0.5, 0.36), yaw=3.0)
    p.box('Wood#bench', 1.0, 3.1, -4.55, -4.25, 0.38, 0.45, bevel=0.015)
    for x in (1.15, 2.95):
        p.box('Wood#bench', x - 0.04, x + 0.04, -4.5, -4.3, 0.0, 0.38, bevel=0.0)


# ===================================================================================== o campo de treinamento
def obstacle_course(p):
    """A pista de obstáculos em linha no lado −Y (corre em X, do anexo até a frente): as 6 duplas de pneus deitados desencontrados, o
    muro de escalar de tábuas com as escoras e a corda, a rede baixa de rastejar nas estacas, a pilha de pneus do fim."""
    yc = -8.8
    for k in range(6):
        x = -2.25 + k * 0.82
        for s in (-1, 1):
            tire6(p, (x + 0.1 * s * (1 if k % 2 else -1), yc + s * 0.43, 0.13), R=0.36, r=0.13)
    # o muro de escalar: as tábuas deitadas nos 3 postes, a viga de cima, as escoras atrás (+X), a corda na frente, o degrau
    xw, h = 3.0, 2.05
    ys = (-9.95, -7.65)
    for y in (ys[0], yc, ys[1]):
        rod(p, 'Wood#post', (xw + 0.12, y, -0.1), (xw + 0.12, y, h + 0.12), 0.09, seg=8)
        rod(p, 'Wood#post', (xw + 0.12, y, h - 0.15), (xw + 1.15, y, -0.05), 0.07, seg=7)
    for k in range(9):
        z0 = 0.1 + k * 0.22
        p.box('Wood#plank', xw - 0.03, xw + 0.03, ys[0] - 0.08, ys[1] + 0.08, z0, z0 + 0.2, bevel=0.012)
    rod(p, 'Wood#beam', (xw + 0.04, ys[0] - 0.15, h + 0.05), (xw + 0.04, ys[1] + 0.15, h + 0.05), 0.075, seg=8)
    tube(p, 'Wood#rope', [(xw - 0.05, yc + 0.5, h + 0.05), (xw - 0.12, yc + 0.52, h - 0.6), (xw - 0.16, yc + 0.48, 0.7), (xw - 0.45, yc + 0.6, 0.05)],
         0.025, seg=6)
    for y in (yc - 0.6, yc - 0.1):
        p.box('Wood#step', xw - 0.3, xw - 0.03, y - 0.15, y + 0.15, 0.55, 0.62, bevel=0.01)
    # a rede de rastejar: as estacas e as cordas trançadas a 0,5 m do chão (a barriga entre as estacas)
    rx0, rx1, ry0, ry1, zr = 4.4, 7.7, -9.85, -7.75, 0.5
    sx = splits(rx0, rx1, 1.1)
    sy = splits(ry0, ry1, 1.05)
    for x in sx:
        for y in sy:
            rod(p, 'Wood#stake', (x, y, -0.1), (x, y, zr + 0.05), 0.04, seg=6)
    for y in splits(ry0, ry1, 0.24):
        pts = []
        for x in splits(rx0, rx1, 0.18):
            dip = min(abs(x - q) for q in sx) / 0.55
            pts.append((x, y, zr - 0.07 * dip - 0.04 * min(abs(y - q) for q in sy) / 0.52))
        tube(p, 'Cloth#rope', pts, 0.017, seg=5)
    for x in splits(rx0, rx1, 0.24):
        pts = []
        for y in splits(ry0, ry1, 0.18):
            dip = min(abs(y - q) for q in sy) / 0.52
            pts.append((x, y, zr + 0.015 - 0.04 * dip - 0.07 * min(abs(x - q) for q in sx) / 0.55))
        tube(p, 'Cloth#rope', pts, 0.017, seg=5)
    # o fim da pista: a pilha de pneus e a bandeirola verde na estaca (a chegada)
    for (x, y, n) in ((8.25, -8.2, 3), (8.3, -9.1, 2)):
        for k in range(n):
            tire6(p, (x + 0.03 * (k % 2), y, 0.13 + k * 0.25), R=0.36, r=0.13)
    rod(p, 'Wood#stake', (8.0, -7.45, -0.1), (8.0, -7.45, 1.55), 0.035, seg=6)
    p.obox('Player#pennant', (7.83, -7.45, 1.38), (0.34, 0.015, 0.26), bevel=0.0)


def firing_range(p):
    """O estande no lado +Y: o barranco de terra (o anteparo) na frente do muro, os bonecos de saco nas estacas com o alvo verde virado
    pra −Y, os 3 ninhos de sacos em C da linha de tiro (abertos pra trás: de cima, os 3 C lêem o estande), o CAVALETE DAS ARMAS (os AK e
    os RPG encostados nos 2 lados) e as caixas de munição."""
    # o barranco: o perfil de terra amontoada correndo em X, morrendo nas pontas
    rings = []
    for i in range(13):
        t = i / 12
        x = 1.15 + 5.3 * t
        env = math.sin(math.pi * t) ** 0.35
        h = 0.95 * env + 0.03
        ring = [(x, 9.45, -0.06), (x, 9.75, h * 0.55), (x, 9.95, h), (x, 10.2, h * 0.8), (x, 10.33, -0.06)]
        rings.append(ring)
    p.loft('Earth#berm', rings, bevel=0.0, smooth=True)
    for k, x in enumerate((2.0, 3.25, 4.5, 5.75)):
        y = 9.05
        rod(p, 'Wood#post', (x, y, -0.1), (x, y, 1.9), 0.06, seg=6)
        rod(p, 'Wood#post', (x - 0.45, y, 1.45), (x + 0.45, y, 1.45), 0.045, seg=6)
        revolve(p, 'Cloth#sack', (x, y), [(0.05, 0.75), (0.2, 0.8), (0.26, 1.0), (0.27, 1.35), (0.22, 1.55), (0.1, 1.6), (0.04, 1.62)], seg=14)
        revolve(p, 'Cloth#sack', (x, y), [(0.03, 1.62), (0.13, 1.66), (0.15, 1.76), (0.12, 1.86), (0.03, 1.9)], seg=10)
        for zz in (0.95, 1.42):
            p.torus('Dark#rope', (x, y, zz), 'z', 0.265, 0.018, seg=12, rseg=4)
        p.cyl('Cloth#target', (x, y - 0.25, 1.2), 'y', 0.23, 0.02, seg=16, bevel=0.0)
        p.cyl('Player#target', (x, y - 0.262, 1.2), 'y', 0.16, 0.02, seg=16, bevel=0.0)
        p.cyl('Dark#target', (x, y - 0.274, 1.2), 'y', 0.065, 0.02, seg=10, bevel=0.0)
    # a linha de tiro: os 3 ninhos de sacos em C, abertos pra −Y (o atirador deita atrás, de frente pros alvos)
    from model_gla_palace_v2 import bag_ring
    for k, x in enumerate((3.9, 5.1, 6.3)):
        bag_ring(p, (x, 6.45), 0.62, 0.0, layers=2, gaps=[(215.0, 325.0)], bag_size=(0.56, 0.32, 0.22), seed=61 + k, lean=0.04)
    # o cavalete das armas (o A de madeira com a trava de baixo), paralelo a X, entre a saída e a linha de tiro
    rack(p, 3.55, 5.35, 4.55)
    crate(p, 6.0, 4.35, 0.0, size=(0.7, 0.45, 0.35), yaw=6, m='Crate')
    crate(p, 6.05, 4.85, 0.0, size=(0.6, 0.4, 0.3), yaw=-10, m='Crate')
    crate(p, 6.03, 4.6, 0.35, size=(0.55, 0.38, 0.28), yaw=20, m='Crate')


def ak(p, origin, yaw, pitch):
    """O AK (0,88 m): a coronha e o guarda-mão de madeira, a caixa, o carregador curvo e o cano escuros."""
    f = Frame(p, origin, yaw=yaw, pitch=pitch)
    f.box('Wood#gun', (0.14, 0.0, -0.01), (0.27, 0.045, 0.09))
    f.box('Dark#gun', (0.4, 0.0, 0.0), (0.27, 0.055, 0.08))
    f.box('Dark#gun', (0.43, 0.0, -0.1), (0.07, 0.035, 0.15), pitch=-22.0)
    f.box('Wood#gun', (0.6, 0.0, 0.0), (0.16, 0.06, 0.065))
    f.cyl('Dark#gun', (0.77, 0.0, 0.01), 'x', 0.013, 0.24, seg=6, bevel=0.0)


def rpg(p, origin, yaw, pitch):
    """O RPG-7 (o tubo, as 2 empunhaduras e a ogiva verde-oliva na boca)."""
    f = Frame(p, origin, yaw=yaw, pitch=pitch)
    f.cyl('Metal#gun', (0.42, 0.0, 0.0), 'x', 0.038, 0.84, seg=8, bevel=0.0)
    f.cyl('Wood#gun', (0.42, 0.0, 0.0), 'x', 0.05, 0.28, seg=8, bevel=0.0)
    f.box('Dark#gun', (0.36, 0.0, -0.08), (0.04, 0.03, 0.12))
    f.cyl('Crate#gun', (0.9, 0.0, 0.0), 'x', 0.06, 0.16, seg=8, r2=0.085, bevel=0.0)
    f.cyl('Crate#gun', (1.07, 0.0, 0.0), 'x', 0.085, 0.18, seg=8, r2=0.01, bevel=0.0)


def rack(p, x0, x1, y):
    """O cavalete: os 2 A de tronco nas pontas, a travessa de cima, as 2 travas de baixo; 5 AK e 2 RPG encostados (os 2 lados)."""
    zt = 0.78
    for x in (x0, x1):
        for s in (-1, 1):
            rod(p, 'Wood#rack', (x, y + s * 0.42, -0.05), (x, y, zt + 0.08), 0.045, seg=6)
        rod(p, 'Wood#rack', (x, y - 0.3, 0.22), (x, y + 0.3, 0.22), 0.03, seg=5)
    rod(p, 'Wood#rack', (x0 - 0.12, y, zt), (x1 + 0.12, y, zt), 0.04, seg=6)
    for s in (-1, 1):
        rod(p, 'Wood#rack', (x0, y + s * 0.34, 0.1), (x1, y + s * 0.34, 0.1), 0.03, seg=5)
    lean = math.degrees(math.atan2(zt - 0.02, 0.37))
    items = [('ak', -1), ('ak', -1), ('rpg', -1), ('ak', -1), ('ak', 1), ('rpg', 1), ('ak', 1)]
    xs = splits(x0 + 0.2, x1 - 0.2, (x1 - x0 - 0.4) / 3.0)
    k_ = {-1: 0, 1: 0}
    for (kind, s) in items:
        i = k_[s]
        k_[s] += 1
        if i >= len(xs):
            continue
        x = xs[i] + (0.1 if s > 0 else 0.0)
        origin = (x, y + s * 0.4, 0.02)
        yaw = -90.0 if s > 0 else 90.0
        (ak if kind == 'ak' else rpg)(p, origin, yaw, lean)


def watchtower(p):
    """(nosso) A torre de vigia de toras no canto da frente (+X, +Y): as 4 pernas afunilando, os X de travamento, o tablado de tábuas, a
    mureta de sacos com a metralhadora, a escada, o teto de chapa e o MASTRO DA BANDEIRA GRANDE acima do teto (a bandeira do campo)."""
    cx, cy = 7.5, 9.0
    b0, b1, zp = 0.95, 0.78, 4.0
    legs = [(sx, sy) for sx in (-1, 1) for sy in (-1, 1)]
    for (sx, sy) in legs:
        rod(p, 'Wood#log', (cx + sx * b0, cy + sy * b0, -0.1), (cx + sx * b1, cy + sy * b1, zp + 2.0 + (0.16 if sx > 0 else 0.0)), 0.1, seg=8)
    lerp = lambda z: b0 + (b1 - b0) * (z + 0.1) / (zp + 2.1)
    for (z0, z1) in ((0.35, 2.05), (2.05, 3.8)):
        for (ax, ay, bx, by) in ((-1, -1, 1, -1), (-1, 1, 1, 1), (-1, -1, -1, 1), (1, -1, 1, 1)):
            a0 = (cx + ax * lerp(z0), cy + ay * lerp(z0), z0)
            b1_ = (cx + bx * lerp(z1), cy + by * lerp(z1), z1)
            a1 = (cx + ax * lerp(z1), cy + ay * lerp(z1), z1)
            b0_ = (cx + bx * lerp(z0), cy + by * lerp(z0), z0)
            rod(p, 'Wood#brace', a0, b1_, 0.05, seg=6)
            rod(p, 'Wood#brace', b0_, a1, 0.05, seg=6)
            rod(p, 'Wood#brace', a1, b1_, 0.055, seg=6)
    # o tablado
    for k in range(9):
        y = cy - 0.98 + k * 0.245
        p.box('Wood#plank', cx - 1.05, cx + 1.05, y - 0.11, y + 0.11, zp - 0.02, zp + 0.06, bevel=0.01)
    for s in (-1, 1):
        rod(p, 'Wood#beam', (cx - 1.1, cy + s * 0.9, zp - 0.1), (cx + 1.1, cy + s * 0.9, zp - 0.1), 0.08, seg=7)
    # a mureta de sacos (aberta no canto da escada, −X −Y) e a metralhadora virada pra frente do mapa
    bag_row(p, [(cx - 0.85, cy - 0.2), (cx - 0.85, cy + 0.85), (cx + 0.85, cy + 0.85), (cx + 0.85, cy - 0.85), (cx + 0.2, cy - 0.85)], zp + 0.06,
            layers=2, bag_size=(0.52, 0.3, 0.2), seed=71)
    dshk(p, cx + 0.1, cy + 0.1, zp + 0.06, yaw=-30.0)
    # a escada no lado −Y (de frente pro pátio)
    lx = cx - 0.5
    for s in (-1, 1):
        rod(p, 'Wood#lad', (lx + s * 0.25, cy - 1.75, -0.05), (lx + s * 0.25, cy - 1.0, zp + 0.9), 0.04, seg=6)
    for k in range(13):
        t = (k + 0.5) / 13
        y = cy - 1.75 + 0.75 * t
        z = -0.05 + (zp + 0.95) * t
        rod(p, 'Wood#lad', (lx - 0.27, y, z), (lx + 0.27, y, z), 0.025, seg=5)
    # o teto de chapa em uma água (cai pra −X) nas pontas das pernas
    corrugated(p, cx - 1.3, cx + 1.25, cy - 1.25, cy + 1.25, zp + 1.98, zp + 2.24, axis='x', m='Rust#sheet', pitch=0.17, amp=0.035, bend=0.08)
    # o mastro da bandeira grande (preso na perna +X +Y, passa do teto)
    mx, my = cx + 1.36, cy + 1.32
    rod(p, 'Trim#mast', (mx, my, zp + 0.5), (mx, my, zp + 4.1), 0.05, seg=8)
    for z in (zp + 0.7, zp + 1.7):
        rod(p, 'Dark#clamp', (cx + lerp(z), cy + lerp(z), z), (mx, my, z), 0.03, seg=5)
    p.cyl('CnGold#mast', (mx, my, zp + 4.15), 'z', 0.08, 0.1, seg=8, bevel=0.0)
    flag(p, (mx, my), zp + 4.05, 2.1, 1.35, m='Player', phase=0.9)
    # os trapos verdes amarrados nas pernas
    for (sx, sy, z) in ((-1, -1, 2.3), (1, -1, 1.6)):
        p.obox('Player#rag', (cx + sx * 0.95, cy + sy * 0.95, z), (0.03, 0.16, 0.42), yaw=45.0 * sx, roll=10.0, bevel=0.0)


# ===================================================================================== os muros baixos e as colunas
def column(p, x, y, h=2.35):
    """A coluna do ZH: o fuste octogonal de reboco no pé largo, os 2 anéis verdes (a cor do jogador), o capitel e a capa redonda."""
    p.cyl('Plaster#col', (x, y, 0.18), 'z', 0.46, 0.46, seg=8, bevel=0.0)
    p.cyl('Plaster#col', (x, y, 0.4 + (h - 0.4) / 2), 'z', 0.32, h - 0.4, seg=8, bevel=0.0)
    for (z0, z1) in ((0.62, 0.86), (h - 0.72, h - 0.5)):
        p.cyl('Player#ring', (x, y, (z0 + z1) / 2), 'z', 0.345, z1 - z0, seg=8, bevel=0.0)
    revolve(p, 'Plaster#cap', (x, y), [(0.3, h - 0.05), (0.42, h + 0.05), (0.46, h + 0.18), (0.4, h + 0.22), (0.3, h + 0.34), (0.12, h + 0.45),
                                       (0.02, h + 0.5)], seg=16)


def tire6(p, c, R=0.36, r=0.13):
    """O pneu deitado (o toro de 16 × 6: de cima o anel preto lê igual, metade dos triângulos)."""
    p.torus('Rubber', c, 'z', R, r, seg=16, rseg=6)


def low_walls(p):
    """Os muros baixos só nos lados (a frente fica aberta: o pedido de não murar tudo), cada um terminando na coluna do canto; no lado
    −Y o trecho caído (o entulho de adobe); as 2 colunas que marcam a saída da tropa (fora da faixa Y ±3,3)."""
    yw = LY - 0.42
    wall_seg(p, (-2.9, -yw), (4.3, -yw), 0.95, t=0.5, z0=0.0, band=False, round_top=True)
    wall_seg(p, (6.6, -yw), (8.15, -yw), 0.95, t=0.5, z0=0.0, band=False, round_top=True)
    # o trecho caído: o toco baixo e os blocos de adobe soltos
    wall_seg(p, (4.3, -yw), (5.0, -yw), 0.45, t=0.5, z0=0.0, band=False, round_top=False)
    for k in range(9):
        x = 4.5 + 2.0 * _hash(k, 1, 91)
        y = -yw + 0.35 * (_hash(k, 2, 91) - 0.5)
        s = 0.22 + 0.2 * _hash(k, 3, 91)
        p.obox('Plaster#rubble', (x, y, s * 0.35), (s * 1.4, s, s * 0.8), yaw=_hash(k, 4, 91) * 90, pitch=(_hash(k, 5, 91) - 0.5) * 30,
               bevel=0.04)
    p.obox('Broken#rubble', (5.3, -yw + 0.05, 0.12), (0.5, 0.38, 0.24), yaw=20, bevel=0.03)
    wall_seg(p, (0.6, yw), (8.15, yw), 0.95, t=0.5, z0=0.0, band=False, round_top=True)
    for (x, y) in ((8.5, -yw), (8.5, yw), (8.45, -3.95), (8.45, 3.95)):
        column(p, x, y)
    scrawl(p, (1.6, -yw - 0.27, 0.62), '-y', size=0.7, seed=81)


def base():
    p = new_part(PREFIX + '_Base')
    for m in ('Earth#berm', 'CnGold#dome', 'CnGold#rib', 'CnGold#collar', 'CnGold#shard', 'Player#patch', 'Dark#crack', 'Cloth#aw', 'GlaRust#aw',
              'Cloth#sack', 'Player#arch', 'Player#ring', 'Player#pennant', 'Player#rag', 'Cloth#reed', 'Metal#sheet', 'Rust#sheet',
              'Cloth#tarp', 'Glass#water', 'Dark#soot'):
        p.bm(m, 0.0)
    p.bm('Plaster#plinth', 0.06)
    p.bm('Plaster#wall', 0.14)
    p.bm('Plaster#col', 0.0)
    hall(p)
    zb, zt = dome(p)
    dome_patch(p, zb, zt)
    annex_south(p)
    annex_north(p)
    back_shed(p)
    courtyard(p)
    obstacle_course(p)
    firing_range(p)
    watchtower(p)
    low_walls(p)
    return finish(p), zb, zt


def write_preview(out):
    import json
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1300, 1500]], ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1600, 900]],
                      ['side_left', 'ortho', [0, 1, 0], [0, 0, 1], [1300, 900]],
                      ['zh_pose', 'persp', 40.0, 225.0, [1600, 1200], 30.0, 0.92],
                      ['three_quarter', 'persp', 32.0, 215.0, [1600, 1100], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 32.0, 35.0, [1600, 1100], 30.0, 0.95]],
            'usa_at': [12.6, -1.6, 0], 'tank_views': False,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(out, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, indent=1)


def main():
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'gla_barracks_v5')
    os.makedirs(out, exist_ok=True)
    clear_scene()
    b, zb, zt = base()
    total = export(b, out)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -8.3, -0.6, -10.3, 10.3, HZ1 + 1.0, post='Wood', plank='Crate', stripe='Hazard'), out)
    write_preview(out)
    print('MODEL total', total, 'tris')
    print('MODEL lote 18 x 22 m; o salao', HZ1, 'm; a cupula de', round(zb, 2), 'a', round(zt, 2), 'm; a ponta ate', round(zt + 1.18, 2), 'm')
    print('MODEL saida: a porta em (', DOOR_X, ', 0 ), o arco em ( -0.2, 0 ); a faixa livre Y -3,3..3,3 ate a borda +X')
    print('MODEL fire_points_m', [(HC[0], -2.6, round(HZ1 + 0.6, 2)), (-5.8, -8.4, 3.6), (-1.7, 8.9, 3.3)])
    print('MODEL smoke_points_m', [(-7.7, -7.0, 5.1)])


if __name__ == '__main__':
    main()
