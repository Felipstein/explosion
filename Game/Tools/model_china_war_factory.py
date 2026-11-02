"""A War Factory da China provisória, modelada no Blender por script (o elenco das 3 facções, 31/10/2026: docs/ELENCO.md §4.1).

Rodar (Blender sem janela, só CPU), com o detalhe AAA (o chanfro redondo e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- building Game/Tools/model_china_war_factory.py captures/game/models/china_war_factory

A planta é a ficha APROVADA (docs/concept/buildings/china_war_factory.webp e china_war_factory_sheet.webp, 5 vistas), na
bíblia da China (docs/CONCEITO_CONSTRUCOES.md §2): indústria pesada monumental, concreto escuro puxado pro oliva, telhado de
pagode em camadas verde com a borda vermelha, a torre de alto-falantes com o estandarte, o pórtico vermelho com a estrela, a
estrela vermelha grande no piso, o forno com a boca acesa, a chaminé listrada de vermelho e branco. Nunca trem ou trilho:
o pórtico corre num trilho de ponte rolante embutido no piso (perfil de aço numa canaleta, sem dormente).

A palavra (o que lê de cima): O HANGAR ESCURO COM O PAGODE DE DOIS TELHADOS E O PÓRTICO VERMELHO SOBRE A ESTRELA DO PISO.

(31/10, a volta do detalhe AAA: "isso não é um jogo de lego [...] DETALHADO [...] nada de quinas quadradas"; docs/ESTUDO_DETALHE_AAA.md
§3 "Prédio de concreto" e docs/ESTUDO_DEGRADACAO_PREDIOS.md) O que mudou, na mesma planta:
- cada bloco é OCO (Game/Tools/model_building_parts.py, cn_block): paredes de 0,4 m, a laje do teto de 0,35 m, por dentro as vigas e
  os pilares: o furo do dano mostra um prédio, não um papel;
- a pele de PLACAS pré-moldadas: cada placa sai 10 cm da face com o chanfro largo e redondo, e a junta funda entre elas mostra o
  miolo; os PILARES DE QUINA chanfrados e os contrafortes; a CORNIJA em 2 degraus com a pingadeira; o parapeito com a CAPA;
- o HANGAR fundo (os 10 m do salão, não um recesso de 6 m), escuro, com as vigas do forro, os pilares, as luzes penduradas e o
  mezanino: a luz vem de dentro;
- as janelas e as portas RECUADAS na espessura da parede (o caixilho no meio da parede, o peitoril que sai), não pintadas na face;
- o PAGODE com a telha-canal redonda (a onda) e a telha de remate no beiral, as cumeeiras de perfil redondo, as colunas
  vermelhas redondas, as mísulas da viga pintada;
- o pátio em LAJES com o chanfro e a junta de verdade; as luminárias com o corpo; o cano e o duto assentados nos suportes.
O vão da porta não mudou (6,0 × 4,6 m), nem o pórtico (o curso e as posições).

Lote 24 × 26 m (X × Y), frente em +X (a porta de onde o tanque sai). Alturas: o hangar 6,2 m (6,45 com o parapeito), o pagode
até ~10,4 m na cumeeira, a torre até 11,1 m, as cornetas até ~13,3 m e as antenas até 14,0 m; a chaminé até 11,8 m.

Peças (origem no chão, no centro do lote; coordenadas da Unreal, X frente, Y direita, Z cima, metros):
  SM_China_WarFactory_Base: o pódio, o hangar, o pagode, a torre, as alas, o forno, a caldeira, a chaminé, os fundos;
  SM_China_WarFactory_Gantry: o PÓRTICO ROLANTE vermelho (as 2 pernas em cavalete com os truques e as rodas, a viga com a
    estrela, o carro do guincho com o gancho). Mesma origem do modelo; desliza em X sobre os trilhos (repouso X = 0, curso
    até +4,8 m, até os batentes; GANTRY_TRAVEL), pro jogo levar o pórtico por cima do veículo que sai;
  SM_China_WarFactory_Scaffold: o andaime da obra (tubos de aço, scaffold() do model_lib), em volta do pódio.
Materiais (slots do model_faction_kit.py, China prédio): CnConcrete, CnPlinth, CnRoof, CnMetal, CnGold, Player (o vermelho:
pórtico, estandartes, borda do telhado, estrela, faixas da chaminé, colunas), Fire (a boca do forno e a escória), Dark, Trim,
Metal (as cornetas), Glass, Light, Beacon, Hazard, Paint (o branco da chaminé); Crate (as caixas e as tábuas do andaime).

Este script é também a peça comum dos prédios da China (importada pelo Supply Center, o quartel e a Gattling Cannon, pra
nascerem da mesma família): cn_roof (o telhado de pagode com a telha-canal, a fáscia vermelha, as asas e o chiwen), cn_frieze
(a viga pintada com as mísulas), pagoda_story, wall_banner (o estandarte de parede), horn (a corneta), chimney5, tube, bar,
battered, louver, seams, parapet_ring, roof_vent. O main() só roda chamado direto (if __name__ == '__main__').
"""
import math
import os
import sys

import bpy  # noqa: F401
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, clear_scene, export, scaffold  # noqa: E402,F401
COLORS.setdefault('Metal', (92, 90, 82))
from model_faction_kit import (lamp, ladder, railing, polygon_prism, star_points, star_wall,  # noqa: E402
                               disc_ring, cone_horn)  # noqa: F401
from model_building_parts import (BPart, WallFace, wbox, hexa, cn_block, cn_wall_skin, cn_window, cn_rollup,  # noqa: E402
                                  ring_boxes, open_box, round_prof, pipe_run, interior_frame)
from model_building_parts import buttress as cn_buttress, sweep as sweep_path  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'china_war_factory')
PREFIX = 'SM_China_WarFactory'
COLORS.setdefault('Crate', (84, 86, 58))

HX, HY = 12.0, 13.0              # o lote 24 × 26
G = 0.30                         # o piso do pódio (e do pátio de saída)
YC = 1.2                         # o eixo da porta (a ficha tem a porta um pouco pra esquerda: o forno é largo)
WT = 0.4                         # a espessura das paredes de concreto
PROUD = 0.12                     # quanto as placas saem da face
# o hangar
HALL_X0, HALL_X1, HALL_Y0, HALL_Y1, HALL_Z = -9.0, 1.6, -4.6, 7.0, 6.2
DOOR_Y0, DOOR_Y1, DOOR_Z, DOOR_D = YC - 3.0, YC + 3.0, G + 4.6, HALL_X0 + WT   # o vão 6,0 × 4,6 e o fundo do salão
# o pórtico rolante: os trilhos (Y), o centro das pernas em repouso (X), a viga
RAIL_Y = (-3.4, 5.8)
GANTRY_X = 3.6
GANTRY_TRAVEL = 4.8
BEAM_Z0, BEAM_Z1 = 4.9, 5.9
# a torre dos alto-falantes
TOWER_X0, TOWER_X1, TOWER_Z = -0.7, 1.3, 11.1
# o pagode: o retângulo das paredes de cada andar (centro, meias-medidas); o soco sobe acima do parapeito do hangar
T1 = dict(cx=-3.7, cy=YC, ax=3.7, ay=4.6, z=6.6, wall=0.7, rise=1.15, over=0.9, inset=1.25)
T2 = dict(cx=-3.7, cy=YC, ax=2.3, ay=3.3, wall=0.7, rise=1.3, over=0.85)
TILE = round_prof(0.17, 0.085, base=-0.03, n=7)        # a telha-canal (a onda do telhado: 17 cm de largura, a cada 30 cm)
HIP = round_prof(0.30, 0.24, base=-0.05, n=7)          # a cumeeira do canto (o rolo de telha)


# ===================================================================================================== utilidades

def tube(p, m, pts, r, seg=16, smooth=True, caps=(True, True)):
    """Tubo redondo por uma polilinha [(x, y, z)...] (o duto, o cotovelo), com o quadro transportado (sem torcer)."""
    P = [Vector(q) for q in pts]
    T = []
    for i in range(len(P)):
        a = P[max(i - 1, 0)]
        b = P[min(i + 1, len(P) - 1)]
        T.append((b - a).normalized())
    ref = Vector((0, 0, 1)) if abs(T[0].z) < 0.9 else Vector((1, 0, 0))
    n = T[0].cross(ref).normalized()
    rings = []
    for i in range(len(P)):
        if i > 0:
            n = T[i - 1].rotation_difference(T[i]) @ n
        b2 = T[i].cross(n).normalized()
        rings.append([tuple(P[i] + (n * math.cos(2 * math.pi * k / seg) + b2 * math.sin(2 * math.pi * k / seg)) * r)
                      for k in range(seg)])
    p.loft(m, rings, bevel=0.0, smooth=smooth, caps=caps)


def bar(p, m, pts, w, h, up=(0.0, 0.0, 1.0), caps=(True, True)):
    """Barra de seção retangular (w de largura, h de altura pra cima) por uma polilinha."""
    U = Vector(up)
    P = [Vector(q) for q in pts]
    rings = []
    for i in range(len(P)):
        a = P[max(i - 1, 0)]
        b = P[min(i + 1, len(P) - 1)]
        t = (b - a).normalized()
        s = t.cross(U)
        if s.length < 1e-6:
            s = Vector((1, 0, 0))
        s.normalize()
        q = P[i]
        rings.append([tuple(q - s * w / 2 - U * 0.02), tuple(q + s * w / 2 - U * 0.02),
                      tuple(q + s * w / 2 + U * h), tuple(q - s * w / 2 + U * h)])
    p.loft(m, rings, bevel=0.0, caps=caps)


def arc(c, r, a0, a1, n, plane='xz'):
    """Pontos de um arco (o cotovelo do duto) no plano dado, ângulos em graus."""
    out = []
    for k in range(n + 1):
        a = math.radians(a0 + (a1 - a0) * k / n)
        u, v = r * math.cos(a), r * math.sin(a)
        if plane == 'xz':
            out.append((c[0] + u, c[1], c[2] + v))
        elif plane == 'yz':
            out.append((c[0], c[1] + u, c[2] + v))
        else:
            out.append((c[0] + u, c[1] + v, c[2]))
    return out


def battered(p, m, x0, x1, y0, y1, z0, z1, bx=(0.0, 0.0), by=(0.0, 0.0), bevel=0.05):
    """Bloco com as paredes inclinadas (a bíblia: 'paredes inclinadas'): o topo recua bx/by de cada lado (−, +)."""
    p.loft(m, [[(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)],
               [(x0 + bx[0], y0 + by[0], z1), (x1 - bx[1], y0 + by[0], z1), (x1 - bx[1], y1 - by[1], z1), (x0 + bx[0], y1 - by[1], z1)]],
           bevel=bevel)


def seams(p, face, at, u0, u1, z0, z1, along, m='Dark', t=0.025, w=0.05):
    """As juntas das placas grandes de concreto pintadas na face (o jeito antigo; os prédios novos usam a pele de placas do
    model_building_parts, com o sulco de verdade)."""
    s = 1 if face[0] == '+' else -1
    for u in along:
        if face in ('+x', '-x'):
            p.box(m, min(at, at + s * t), max(at, at + s * t), u - w / 2, u + w / 2, z0, z1, bevel=0.0)
        else:
            p.box(m, u - w / 2, u + w / 2, min(at, at + s * t), max(at, at + s * t), z0, z1, bevel=0.0)


def louver(p, x0, x1, y0, y1, z0, z1, face, n=6, frame='CnMetal'):
    """A grelha de ventilação (moldura de aço chanfrada com as lâminas escuras inclinadas, recuadas), colada na face."""
    s = 1 if face[0] == '+' else -1
    fm = frame + '#louv'
    if face in ('+x', '-x'):
        x = x0 if s < 0 else x1
        F = WallFace(face, x, 0.0)
        u0, u1 = y0, y1
    else:
        y = y0 if s < 0 else y1
        F = WallFace(face, y, 0.0)
        u0, u1 = x0, x1
    fw = 0.09
    wbox(p, fm, F, u0, u1, z0, z0 + fw, -0.02, 0.1, bevel=0.0)
    wbox(p, fm, F, u0, u1, z1 - fw, z1, -0.02, 0.1, bevel=0.02)
    wbox(p, fm, F, u0, u0 + fw, z0, z1, -0.02, 0.1, bevel=0.02)
    wbox(p, fm, F, u1 - fw, u1, z0, z1, -0.02, 0.1, bevel=0.02)
    wbox(p, 'Dark', F, u0 + fw, u1 - fw, z0 + fw, z1 - fw, -0.02, 0.0, bevel=0.0)
    for k in range(n):
        z = z0 + fw + (z1 - z0 - 2 * fw) * (k + 0.5) / n
        hh = (z1 - z0 - 2 * fw) / n * 0.5
        P = []
        for (zz, n0, n1) in ((z - hh, 0.0, 0.05), (z + hh, 0.02, 0.07)):
            for nn in (n0, n1):
                for uu in (u0 + fw, u1 - fw):
                    P.append(F.pt(uu, zz, nn))
        hexa(p, frame + '#blade', P, bevel=0.0)


def roof_vent(p, x, y, z, sx=1.6, sy=1.2, h=0.7):
    """O exaustor do teto: a caixa de concreto chanfrada com as grelhas nas 4 faces e a tampa de aço que sai em aba."""
    p.box('CnConcrete#vent', x - sx / 2, x + sx / 2, y - sy / 2, y + sy / 2, z, z + h, bevel=0.06)
    for face, (a0, a1, b0, b1) in (('+x', (x - sx / 2, x + sx / 2, y - sy / 2 + 0.18, y + sy / 2 - 0.18)),
                                   ('-x', (x - sx / 2, x + sx / 2, y - sy / 2 + 0.18, y + sy / 2 - 0.18)),
                                   ('+y', (x - sx / 2 + 0.18, x + sx / 2 - 0.18, y - sy / 2, y + sy / 2)),
                                   ('-y', (x - sx / 2 + 0.18, x + sx / 2 - 0.18, y - sy / 2, y + sy / 2))):
        louver(p, a0, a1, b0, b1, z + 0.12, z + h - 0.12, face, n=3)
    p.box('CnMetal#cap', x - sx / 2 - 0.14, x + sx / 2 + 0.14, y - sy / 2 - 0.14, y + sy / 2 + 0.14, z + h + 0.06, z + h + 0.16, bevel=0.012)
    for (dx, dy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        p.box('CnMetal#cap', x + dx * (sx / 2 - 0.1) - 0.04, x + dx * (sx / 2 - 0.1) + 0.04, y + dy * (sy / 2 - 0.1) - 0.04,
              y + dy * (sy / 2 - 0.1) + 0.04, z + h - 0.01, z + h + 0.07, bevel=0.0)


def parapet_ring(p, x0, x1, y0, y1, z, h=0.4, t=0.3, m='CnConcrete', skip=()):
    """O parapeito do teto com a capa (a pingadeira saindo 6 cm dos 2 lados: a sombra fina que desenha o topo)."""
    ring_boxes(p, m, x0, x1, y0, y1, z, z + h, t, skip=skip, bevel=0.03)
    ring_boxes(p, 'CnPlinth#cap', x0 - 0.06, x1 + 0.06, y0 - 0.06, y1 + 0.06, z + h, z + h + 0.09, t + 0.12, skip=skip, bevel=0.03)


def buttress(p, x, y, face, h, z0=G, w=0.7, d=0.55, m='CnConcrete'):
    """O contraforte chanfrado encostado na face (a lâmina de concreto com o topo em rampa)."""
    F = WallFace(face, x if face[1] == 'x' else y, z0)
    cn_buttress(p, F, y if face[1] == 'x' else x, z0, h, w=w, d=d, top=min(d * 1.2, h * 0.4), m=m + '#pier')


# ===================================================================================================== o telhado de pagode

def roof_point(cx, cy, Ex, Ey, inset, rise, z0, curl, sweep_, side, u, s, push=0.0, dz=0.0):
    """Um ponto da superfície do telhado chinês. side: 0 (−Y), 1 (+X), 2 (+Y), 3 (−X); u = a coordenada ao longo do lado;
    s = 0 no beiral, 1 no topo. A curva côncava (raso no beiral, íngreme no topo) e as pontas levantadas nos cantos (o
    beiral sobe e abre pra fora perto da quina, a 'asa'). push = afastamento pra fora (a fáscia), dz = desnível."""
    Hx, Hy = Ex - inset * s, Ey - inset * s
    H, P = (Hx, Hy) if side in (0, 2) else (Hy, Hx)
    d = min(1.0, abs(u) / H) if H > 1e-6 else 1.0
    k = d ** 3 * (1.0 - s) ** 2
    z = z0 + rise * (s ** 1.6) + curl * k + dz
    out = sweep_ * k + push
    along = u + math.copysign(sweep_ * k * 0.6, u) + (math.copysign(push, u) if d > 0.999 else 0.0)
    if side == 0:
        return (cx + along, cy - P - out, z)
    if side == 1:
        return (cx + P + out, cy + along, z)
    if side == 2:
        return (cx + along, cy + P + out, z)
    return (cx - P - out, cy + along, z)


def roof_ring(cx, cy, Ex, Ey, inset, rise, z0, curl, sweep_, s, n, push=0.0, dz=0.0):
    """O anel do telhado no nível s: 4 lados com n pontos cada, a partir do canto (−X, −Y), girando por +X."""
    Hx, Hy = Ex - inset * s, Ey - inset * s
    ring = []
    for side in range(4):
        H = Hx if side in (0, 2) else Hy
        for i in range(n):
            t = i / n
            u = -H + 2 * H * t if side in (0, 1) else H - 2 * H * t
            ring.append(roof_point(cx, cy, Ex, Ey, inset, rise, z0, curl, sweep_, side, u, s, push, dz))
    return ring


def cn_roof(p, cx, cy, ax, ay, z0, rise, over=1.0, inset=None, curl=0.5, sweep=0.28, n=10, rib_step=0.30,
            m='CnRoof', edge='Player', ridge=True, th=0.2, tip=1.0):
    """O telhado de pagode de um andar: o quadril côncavo coberto de TELHA-CANAL (a fileira redonda a cada rib_step, a onda que
    pega o sol em listras, com a telha de remate redonda no beiral), a fáscia vermelha seguindo o beiral (com a espessura do
    beiral), as cumeeiras dos cantos de perfil redondo com a ponta levantada (a asa) e o enfeite vermelho, a cumeeira de cima com
    os ornamentos dourados. ax, ay = as meias-medidas da parede; o beiral sai 'over' além dela, em z0.
    inset = o recuo do topo (None: quadril inteiro até a cumeeira); tip = a escala da asa da quina (telhadinho: menor).
    Devolve (z do topo, meias-medidas do topo)."""
    Ex, Ey = ax + over, ay + over
    full = inset is None
    if full:
        inset = min(Ex, Ey) - 0.16
    S = [0.0, 0.1, 0.25, 0.45, 0.7, 1.0]
    # a casca: o forro (do retângulo da parede até o beiral, por baixo), a espessura do beiral, a superfície e o topo
    soffit = []
    for side in range(4):
        H = ax if side in (0, 2) else ay
        for i in range(n):
            t = i / n
            u = -H + 2 * H * t if side in (0, 1) else H - 2 * H * t
            P = ay if side in (0, 2) else ax
            q = {0: (cx + u, cy - P), 1: (cx + P, cy + u), 2: (cx + u, cy + P), 3: (cx - P, cy + u)}[side]
            soffit.append((q[0], q[1], z0 - th))
    rings = [soffit, roof_ring(cx, cy, Ex, Ey, inset, rise, z0, curl, sweep, 0.0, n, dz=-th)]
    rings += [roof_ring(cx, cy, Ex, Ey, inset, rise, z0, curl, sweep, s, n) for s in S]
    p.loft(m, rings, bevel=0.0, smooth=False)
    # a telha-canal (cada fileira: o rolo redondo do beiral até a cumeeira do canto) e a telha de remate no beiral
    for side in range(4):
        E_al = Ex if side in (0, 2) else Ey
        nr = int(2 * E_al / rib_step)
        for r in range(nr):
            u = -E_al + (r + 0.5) * (2 * E_al / nr)
            smax = min(1.0, (E_al - abs(u) - 0.12) / inset) if inset > 0 else 1.0
            if smax < 0.06:
                continue
            pts = [roof_point(cx, cy, Ex, Ey, inset, rise, z0, curl, sweep, side, u, smax * j / 5) for j in range(6)]
            sweep_tile(p, m + '#rib', pts)
    # a fáscia vermelha acompanhando o beiral (um tubo de seção retangular em volta: a espessura do beiral)
    def fring(push, dz):
        return roof_ring(cx, cy, Ex, Ey, inset, rise, z0, curl, sweep, 0.0, n, push=push, dz=dz)
    p.loft(edge, [fring(0.06, -th - 0.1), fring(0.06, 0.03), fring(-0.1, 0.03), fring(-0.1, -th - 0.1), fring(0.06, -th - 0.1)],
           bevel=0.0, caps=(False, False))
    # as cumeeiras dos cantos (o rolo redondo), da quina do topo até a ponta levantada, e o enfeite na ponta
    top_z = z0 + rise
    for side in range(4):
        H0 = Ex if side in (0, 2) else Ey
        u0 = -H0 if side in (0, 1) else H0
        pts = []
        for j in range(5, -1, -1):
            s = S[j]
            H = (Ex if side in (0, 2) else Ey) - inset * s
            u = -H if side in (0, 1) else H
            pts.append(roof_point(cx, cy, Ex, Ey, inset, rise, z0, curl, sweep, side, u, s, dz=0.05))
        c = Vector(roof_point(cx, cy, Ex, Ey, inset, rise, z0, curl, sweep, side, u0, 0.0))
        dpl = Vector((c.x - cx, c.y - cy, 0.0)).normalized()
        pts.append(tuple(c + dpl * 0.35 * tip + Vector((0, 0, 0.2 * tip))))
        pts.append(tuple(c + dpl * 0.62 * tip + Vector((0, 0, 0.5 * tip))))
        sweep_path(p, m + '#hip', pts, HIP)
        tq = c + dpl * 0.66 * tip + Vector((0, 0, 0.62 * tip))
        p.obox(edge, tuple(tq), (0.22 * tip, 0.22 * tip, 0.26 * tip), yaw=math.degrees(math.atan2(dpl.y, dpl.x)), pitch=35, bevel=0.02)
        # as 3 figurinhas na cumeeira do canto (os bichinhos do telhado imperial, em miniatura)
        for f in range(3):
            q = Vector(pts[-3 - f]) if len(pts) > 4 else c
            p.obox('CnGold', tuple(q + Vector((0, 0, 0.3))), (0.14, 0.12, 0.18), yaw=math.degrees(math.atan2(dpl.y, dpl.x)), bevel=0.0)
    Tx, Ty = Ex - inset, Ey - inset
    if ridge:
        if full:
            along_x = Tx >= Ty
            L = max(Tx, Ty)
            # a cumeeira: o assento de telha (a base) e o rolo grande por cima, de perfil redondo
            if along_x:
                line = [(cx - L - 0.1 + (2 * L + 0.2) * k / 6, cy, top_z - 0.05) for k in range(7)]
            else:
                line = [(cx, cy - L - 0.1 + (2 * L + 0.2) * k / 6, top_z - 0.05) for k in range(7)]
            sweep_path(p, m + '#ridge', line, [(-0.24, -0.06), (0.24, -0.06), (0.24, 0.16), (-0.24, 0.16)])
            sweep_path(p, m + '#ridge', [(q[0], q[1], q[2] + 0.14) for q in line], round_prof(0.36, 0.26, base=-0.02, n=9))
            sweep_path(p, edge, [(q[0], q[1], q[2] + 0.13) for q in line], [(-0.25, 0.0), (0.25, 0.0), (0.25, 0.05), (-0.25, 0.05)])
            for sgn in (-1, 1):
                ox, oy = (cx + sgn * (L + 0.05), cy) if along_x else (cx, cy + sgn * (L + 0.05))
                # o ornamento da ponta da cumeeira (o chiwen, o peixe-dragão: a peça que sobe da cumeeira e enrola o rabo
                # pra dentro), uma fita curva de seção retangular desenhada no plano da cumeeira
                curl_ = [(0.0, 0.18), (0.06, 0.5), (0.02, 0.78), (-0.14, 0.95), (-0.32, 0.93), (-0.4, 0.8), (-0.33, 0.68)]
                if along_x:
                    pts = [(ox + sgn * u, oy - 0.12, top_z + 0.1 + v) for (u, v) in curl_]
                    bar(p, 'CnGold', pts, 0.24, 0.24, up=(0.0, 1.0, 0.0))
                else:
                    pts = [(ox - 0.12, oy + sgn * u, top_z + 0.1 + v) for (u, v) in curl_]
                    bar(p, 'CnGold', pts, 0.24, 0.24, up=(1.0, 0.0, 0.0))
            # a pérola no meio da cumeeira
            p.cyl('CnGold', (cx, cy, top_z + 0.55), 'z', 0.16, 0.24, seg=12, r2=0.06, bevel=0.0)
    return top_z, Tx, Ty


def sweep_tile(p, m, pts):
    """Uma fileira de telha-canal (o rolo redondo) e a telha de remate redonda na ponta do beiral."""
    sweep_path(p, m, pts, TILE, smooth=True)
    P0, P1 = Vector(pts[0]), Vector(pts[1])
    d = (P1 - P0).normalized()
    up = Vector((0.0, 0.0, 1.0))
    h = (up - d * up.dot(d)).normalized()
    p.cyl(m, tuple(P0 + h * 0.03 - d * 0.015), tuple(d), 0.1, 0.05, seg=12, bevel=0.0)


def cn_frieze(p, cx, cy, ax, ay, z1, h=0.32, m='CnRoof', dot='CnGold', step=0.95):
    """A viga pintada debaixo do beiral (a faixa verde) com as MÍSULAS douradas (os 2 blocos em degrau que seguram o beiral: a
    sombra miúda em ritmo que a câmera lê), em volta da parede."""
    o = 0.06
    for (x0, x1, y0, y1) in ((cx - ax - o, cx + ax + o, cy - ay - o, cy - ay + 0.05), (cx - ax - o, cx + ax + o, cy + ay - 0.05, cy + ay + o),
                             (cx - ax - o, cx - ax + 0.05, cy - ay + 0.05 - 0.002, cy + ay - 0.05 + 0.002),
                             (cx + ax - 0.05, cx + ax + o, cy - ay + 0.05 - 0.002, cy + ay - 0.05 + 0.002)):
        p.box(m + '#frz', x0, x1, y0, y1, z1 - h, z1, bevel=0.02)
    for side in range(4):
        L = 2 * (ax if side in (0, 2) else ay)
        k = int(L / step)
        out = {0: (0, -1), 1: (1, 0), 2: (0, 1), 3: (-1, 0)}[side]
        for i in range(k + 1):
            u = -L / 2 + i * L / k
            if side == 0:
                c = (cx + u, cy - ay - o)
            elif side == 2:
                c = (cx + u, cy + ay + o)
            elif side == 1:
                c = (cx + ax + o, cy + u)
            else:
                c = (cx - ax - o, cy + u)
            for (wd, d_out, za, zb) in ((0.15, 0.12, z1 - h * 0.85, z1 - h * 0.45), (0.24, 0.2, z1 - h * 0.45, z1 - h * 0.05)):
                if out[0]:
                    xa, xb = sorted((c[0], c[0] + out[0] * d_out))
                    p.box(dot + '#brk', xa, xb, c[1] - wd / 2, c[1] + wd / 2, za, zb, bevel=0.0)
                else:
                    ya, yb = sorted((c[1], c[1] + out[1] * d_out))
                    p.box(dot + '#brk', c[0] - wd / 2, c[0] + wd / 2, ya, yb, za, zb, bevel=0.0)


def pagoda_story(p, cx, cy, ax, ay, z0, wall, win_step=1.4):
    """A parede curta do andar do pagode: o concreto escuro, as COLUNAS VERMELHAS REDONDAS nos cantos (a base e o capitel), as
    janelas de treliça recuadas (o escuro atrás, a grade vermelha na frente) e a viga pintada com as mísulas."""
    p.box('CnConcrete', cx - ax, cx + ax, cy - ay, cy + ay, z0, z0 + wall, bevel=0.03)
    for (sx, sy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        x, y = cx + sx * ax, cy + sy * ay
        p.cyl('Player#col', (x, y, z0 + wall / 2), 'z', 0.19, wall, seg=16, bevel=0.0, smooth=True)
        p.cyl('CnPlinth#col', (x, y, z0 + 0.05), 'z', 0.25, 0.1, seg=16, bevel=0.0)
        p.cyl('CnGold#col', (x, y, z0 + wall - 0.04), 'z', 0.23, 0.08, seg=16, bevel=0.0)
    zw0, zw1 = z0 + 0.1, z0 + wall - 0.34
    if zw1 - zw0 > 0.15:
        for side in range(4):
            L = 2 * (ax if side in (0, 2) else ay) - 0.8
            k = max(2, int(L / win_step))
            face = {0: '-y', 1: '+x', 2: '+y', 3: '-x'}[side]
            at = {0: cy - ay, 1: cx + ax, 2: cy + ay, 3: cx - ax}[side]
            F = WallFace(face, at, 0.0)
            for i in range(k):
                u = -L / 2 + (i + 0.5) * L / k + (cx if side in (0, 2) else cy)
                wbox(p, 'Dark', F, u - 0.3, u + 0.3, zw0, zw1, -0.04, 0.005, bevel=0.0)
                for (a, b) in ((u - 0.34, u - 0.28), (u + 0.28, u + 0.34), (u - 0.025, u + 0.025)):
                    wbox(p, 'Player#lat', F, a, b, zw0 - 0.03, zw1 + 0.03, -0.01, 0.05, bevel=0.0)
                for zz in (zw0 - 0.03, (zw0 + zw1) / 2 - 0.02, zw1 - 0.03):
                    wbox(p, 'Player#lat', F, u - 0.34, u + 0.34, zz, zz + 0.05, -0.01, 0.05, bevel=0.0)
    cn_frieze(p, cx, cy, ax, ay, z0 + wall)


# ===================================================================================================== peças da China

def wall_banner(p, x, y, z_top, face, w=1.2, h=3.0, notch=0.45, m='Player'):
    """O estandarte pendurado na parede: o pano vermelho com a estrela dourada e a faixa dourada embaixo dela, a ponta em
    rabo de andorinha, a vara de aço com as pontas douradas e as 2 mãos-francesas até a parede. O pano tem uma leve barriga
    (a onda do tecido: 3 dobras verticais rasas), não é uma placa."""
    s = 1 if face[0] == '+' else -1
    off = 0.12
    outline = [(-w / 2, 0.0), (-w / 6, 0.0), (w / 6, 0.0), (w / 2, 0.0), (w / 2, -h), (0.0, -h + notch), (-w / 2, -h)]

    def bulge(u):
        return 0.035 * math.cos(3 * math.pi * u / w)
    if face in ('+x', '-x'):
        xa = x + s * off
        poly = lambda dd: [(xa + s * (dd + bulge(u)), y + u, z_top + v) for (u, v) in outline]
        p.loft(m, [poly(0.0), poly(0.03)], bevel=0.0)
        p.cyl('CnMetal', (x + s * (off + 0.02), y, z_top + 0.04), 'y', 0.045, w + 0.3, seg=8, bevel=0.0)
        for e in (-1, 1):
            p.cyl('CnGold', (x + s * (off + 0.02), y + e * (w / 2 + 0.17), z_top + 0.04), 'y', 0.07, 0.08, seg=8, bevel=0.0)
            p.box('CnMetal', min(x, x + s * (off + 0.03)), max(x, x + s * (off + 0.03)), y + e * (w / 2 - 0.05) - 0.03, y + e * (w / 2 - 0.05) + 0.03, z_top, z_top + 0.07, bevel=0.0)
        xb = xa + s * (0.03 + bulge(0.0))
        star_wall(p, (xb, y, z_top - w * 0.55), face, w * 0.3, m='CnGold', t=0.012)
        p.box('CnGold', min(xb, xb + s * 0.012), max(xb, xb + s * 0.012), y - 0.05, y + 0.05, z_top - h + notch + 0.25, z_top - w * 0.95, bevel=0.0)
    else:
        ya = y + s * off
        poly = lambda dd: [(x + u, ya + s * (dd + bulge(u)), z_top + v) for (u, v) in outline]
        p.loft(m, [poly(0.0), poly(0.03)], bevel=0.0)
        p.cyl('CnMetal', (x, y + s * (off + 0.02), z_top + 0.04), 'x', 0.045, w + 0.3, seg=8, bevel=0.0)
        for e in (-1, 1):
            p.cyl('CnGold', (x + e * (w / 2 + 0.17), y + s * (off + 0.02), z_top + 0.04), 'x', 0.07, 0.08, seg=8, bevel=0.0)
            p.box('CnMetal', x + e * (w / 2 - 0.05) - 0.03, x + e * (w / 2 - 0.05) + 0.03, min(y, y + s * (off + 0.03)), max(y, y + s * (off + 0.03)), z_top, z_top + 0.07, bevel=0.0)
        yb = ya + s * (0.03 + bulge(0.0))
        star_wall(p, (x, yb, z_top - w * 0.55), face, w * 0.3, m='CnGold', t=0.012)
        p.box('CnGold', x - 0.05, x + 0.05, min(yb, yb + s * 0.012), max(yb, yb + s * 0.012), z_top - h + notch + 0.25, z_top - w * 0.95, bevel=0.0)


def horn(p, base, d, r0=0.1, r1=0.42, L=0.95):
    """A corneta do alto-falante: o motor (a lata de trás), a garganta e a boca aberta com o aro rolado, o escuro dentro."""
    D = Vector(d).normalized()
    B = Vector(base)
    p.cyl('CnMetal', tuple(B - D * 0.12), tuple(D), r1 * 0.38, 0.28, seg=12, bevel=0.0)
    p.cyl('Metal', tuple(B + D * (L * 0.3)), tuple(D), r0, L * 0.6, seg=16, r2=r0 * 1.9, bevel=0.0, smooth=True)
    p.cyl('Metal', tuple(B + D * (L * 0.78)), tuple(D), r0 * 1.9, L * 0.44, seg=16, r2=r1, bevel=0.0, smooth=True)
    p.cyl('Dark', tuple(B + D * (L - 0.02)), tuple(D), r1 * 0.9, 0.04, seg=16, bevel=0.0)
    c = B + D * (L - 0.01)
    p.cyl('Metal', tuple(c), tuple(D), r1 + 0.03, 0.05, seg=16, bevel=0.0)


def chimney5(p, c, r0, r1, z0, h, bands=5):
    """A chaminé listrada (branco, vermelho, branco, vermelho, branco: a da ficha), o aro escuro e a boca preta, os anéis de
    reforço redondos (o relevo que pega luz)."""
    seg = 28
    for k in range(bands):
        za, zb = z0 + h * k / bands, z0 + h * (k + 1) / bands
        ra, rb = r0 + (r1 - r0) * k / bands, r0 + (r1 - r0) * (k + 1) / bands
        p.cyl('Player' if k % 2 else 'Paint', (c[0], c[1], (za + zb) / 2), 'z', ra, zb - za, seg=seg, r2=rb, bevel=0.0, smooth=True)
    p.torus('Dark', (c[0], c[1], z0 + h + 0.08), 'z', r1, 0.11, seg=seg, rseg=8)
    p.cyl('Dark', (c[0], c[1], z0 + h - 0.3), 'z', r1 - 0.08, 0.6, seg=seg, bevel=0.0)
    for zz in (z0 + h * 0.4, z0 + h * 0.8):                                     # os anéis de reforço
        p.torus('Trim', (c[0], c[1], zz), 'z', r0 + (r1 - r0) * (zz - z0) / h + 0.01, 0.045, seg=seg, rseg=6)


# ===================================================================================================== a base

def podium(p):
    """O pódio de concreto (a saia que entra no chão, sem o fundo), o pátio de saída em LAJES (o chanfro e a junta de verdade)
    rebaixado no pódio, a rampa até o chão na frente, as muretas da frente com a ponta inclinada e as divisas de perigo, a
    calçada das alas, as luzes no rodapé."""
    FX = 8.6                                                                     # a frente do pódio (as alas)
    PY = HY - 0.2                                                                # os lados do pódio (as luzes do rodapé ficam no lote)
    AY0, AY1 = -4.4, 6.8
    GL = G - 0.06                                                                # o leito das lajes do pátio
    # o pódio em 3 pedaços: o de trás (até o hangar), os 2 da frente fora do pátio, e o leito do pátio 6 cm abaixo
    open_box(p, 'CnPlinth', -HX, HALL_X1, -PY, PY, -1.0, G, bevel=0.06)
    open_box(p, 'CnPlinth', HALL_X1 - 0.002, FX, -PY, AY0 - 0.6, -1.0, G, bevel=0.06)
    open_box(p, 'CnPlinth', HALL_X1 - 0.002, FX, AY1 + 0.6, PY, -1.0, G, bevel=0.06)
    open_box(p, 'CnPlinth#bed', HALL_X1 - 0.004, FX, AY0 - 0.6 - 0.002, AY1 + 0.6 + 0.002, -1.0, GL, bevel=0.0)
    ring_boxes(p, 'Dark', -HX - 0.03, FX + 0.03, -PY - 0.03, PY + 0.03, 0.02, 0.06, 0.2, bevel=0.0, skip=(1,))   # o friso do rodapé
    # a rampa (o leito) até o chão na frente
    p.prism_xz('CnPlinth#bed', [(FX - 0.1, -1.0), (HX, -1.0), (HX, 0.03 - 0.06), (9.6, GL), (FX - 0.1, GL)], AY0 - 0.6, AY1 + 0.6, bevel=0.0)
    # as lajes do pátio: em X juntas em 4,6 e 7,0 (e a rampa de 9,6 até a ponta, inclinada); em Y as faixas entre os trilhos
    xs = [HALL_X1 + 0.02, 4.6, 7.0, 9.6]
    ys = [(AY0 - 0.6, RAIL_Y[0] - 0.32), (RAIL_Y[0] + 0.32, YC - 0.03), (YC + 0.03, RAIL_Y[1] - 0.32), (RAIL_Y[1] + 0.32, AY1 + 0.6)]
    g = 0.035
    for (xa, xb) in zip(xs[:-1], xs[1:]):
        for (ya, yb) in ys:
            p.box('CnPlinth#slab', xa + g, xb - g, ya + g, yb - g, GL - 0.03, G, bevel=0.07)
    for (ya, yb) in ys:                                                          # a laje da rampa (inclinada até o chão)
        p.prism_xz('CnPlinth#slab', [(9.6 + g, GL - 0.03), (HX - 0.05, -0.03), (HX - 0.05, 0.03), (9.6 + g, G)], ya + g, yb - g, bevel=0.07)
    # as muretas dos lados do pátio, do pé das alas até a frente, com a ponta em cunha e as divisas
    for (y0, y1, x_start) in ((AY0 - 1.4, AY0 - 0.6, 3.4), (AY1 + 0.6, AY1 + 1.4, 3.0)):
        p.prism_xz('CnConcrete#mur', [(x_start, -1.0), (HX, -1.0), (HX, 0.18), (10.2, G + 1.0), (x_start, G + 1.0)], y0, y1, bevel=0.1)
        p.box('CnPlinth#cap', x_start - 0.06, 10.2, y0 - 0.07, y1 + 0.07, G + 1.0, G + 1.1, bevel=0.03)            # a capa da mureta
        slope = math.degrees(math.atan2(G + 1.0 - 0.18, HX - 10.2))
        for k in range(4):                                                       # as divisas na face inclinada
            t = (k + 0.5) / 4
            x = 10.2 + (HX - 10.2) * t
            z = G + 1.0 + (0.18 - G - 1.0) * t
            p.obox('Hazard' if k % 2 == 0 else 'Dark', (x, (y0 + y1) / 2, z + 0.03), ((HX - 10.2) / 4 * 1.05, y1 - y0 + 0.02, 0.05), pitch=-slope, bevel=0.0)
        for x in (4.6, 6.6, 8.6):                                                # as luzes de dentro das muretas
            yy = y1 if y0 < 0 else y0
            lamp(p, x, yy, G + 0.55, face='+y' if y0 < 0 else '-y', kind='flush')
    # o piso do pátio: as faixas amarelas da pista, a estrela grande (a ponta pro hangar) e o ralo
    for y in (YC - 3.25, YC + 3.25):
        p.box('Hazard', 1.6, 11.6, y - 0.09, y + 0.09, G, G + 0.012, bevel=0.0)
    p.box('Hazard', 2.0, 2.18, YC - 3.25, YC + 3.25, G, G + 0.012, bevel=0.0)                  # a faixa de parada
    polygon_prism(p, 'Player', star_points(7.0, YC, 2.5, rot_deg=180.0), G, G + 0.014)
    p.box('Dark', 9.35, 9.85, AY0 + 0.2, AY1 - 0.2, G - 0.1, G - 0.06, bevel=0.0)                # o ralo (o fundo)
    for k in range(int((AY1 - AY0 - 0.4) / 0.25)):
        y = AY0 + 0.32 + k * 0.25
        p.box('CnMetal#grate', 9.42, 9.78, y - 0.05, y + 0.05, G - 0.05, G - 0.005, bevel=0.0)
    # os trilhos do pórtico: o perfil de aço numa canaleta escura, nivelado com o piso (ponte rolante, não trem)
    for y in RAIL_Y:
        p.box('CnMetal', 1.65, 10.6, y - 0.3, y + 0.3, GL - 0.04, G - 0.02, bevel=0.0)
        p.box('Dark', 1.65, 10.6, y - 0.14, y + 0.14, G - 0.03, G - 0.01, bevel=0.0)
        p.box('CnMetal#rail', 1.65, 10.6, y - 0.07, y + 0.07, G - 0.03, G + 0.06, bevel=0.015)
        for x in (1.75, 10.5):                                                   # os batentes nas pontas
            p.box('Hazard', x - 0.18, x + 0.18, y - 0.3, y + 0.3, G, G + 0.45, bevel=0.03)
            p.box('Dark', x - 0.19, x + 0.19, y - 0.31, y + 0.31, G + 0.15, G + 0.28, bevel=0.0)
    # a calçada da frente das alas: a mureta baixa com a capa e as luzes
    for (y0, y1) in ((-PY, AY0 - 1.4), (AY1 + 1.4, PY)):
        p.box('CnConcrete#mur', FX - 0.6, FX, y0, y1, G, G + 0.85, bevel=0.1)
        p.box('CnPlinth#cap', FX - 0.66, FX + 0.06, y0 - 0.02, y1 + 0.02, G + 0.85, G + 0.95, bevel=0.03)
        for k in range(3):
            yy = y0 + (y1 - y0) * (k + 0.5) / 3
            lamp(p, FX, yy, G + 0.45, face='+x', kind='flush')
    for (y0, y1, face) in ((-PY, -PY + 0.5, '-y'), (PY - 0.5, PY, '+y')):           # as muretas dos lados das calçadas
        p.box('CnConcrete#mur', 3.0, FX - 0.62, y0, y1, G, G + 0.85, bevel=0.1)
    for x in (-9.5, -5.0, -0.5, 4.0):                                            # as luzes do rodapé dos lados
        lamp(p, x, -PY, 0.1, face='-y')
        lamp(p, x, PY, 0.1, face='+y')


def hall(p):
    """O hangar: o bloco OCO (as paredes de 0,4 m com a pele de placas na frente e nos fundos, a laje do teto, a cornija em
    degraus, o parapeito com a capa, os pilares de quina), a porta grande com o pórtico de aço chanfrado e as defensas de
    perigo, e por dentro o salão FUNDO (os 10 m): os pilares, as vigas do forro, as luzes penduradas, o mezanino no fundo, a
    bancada, as caixas, o rolo da porta. A luz vem de dentro; o fundo é escuro."""
    x0, x1, y0, y1, H = HALL_X0, HALL_X1, HALL_Y0, HALL_Y1, HALL_Z
    cn_block(p, x0, x1, y0, y1, G, H, t=WT, skin=('+x', '-x'), holes={'+x': [(DOOR_Y0, DOOR_Y1, G, DOOR_Z)]},
             roof_t=0.35, parapet=0.22, corners=True, pier=0.9, interior=False, pw=2.8, zs=[G, G + 1.6, DOOR_Z, H - 0.35], plinth=0)
    # o interior: os pilares (fora do eixo da porta), as vigas do forro cruzando o salão, a laje e o piso
    zt = H - 0.35
    interior_frame(p, x0 + WT, x1 - WT, y0 + WT, y1 - WT, G, zt, cols=[(-5.4, DOOR_Y0 - 0.9), (-5.4, DOOR_Y1 + 0.9)],
                   along='x', step=2.4, beam=(0.3, 0.55))
    for x in (-7.8, -5.4, -3.0, -0.6):                                           # as luzes penduradas entre as vigas
        for y in (YC - 1.6, YC + 1.6):
            p.cyl('Dark#lamp', (x, y, zt - 0.85), 'z', 0.012, 0.6, seg=4, bevel=0.0)
            p.box('Dark#lamp', x - 0.6, x + 0.6, y - 0.16, y + 0.16, zt - 1.0, zt - 0.88, bevel=0.02)
            p.box('Light', x - 0.55, x + 0.55, y - 0.11, y + 0.11, zt - 1.02, zt - 0.99, bevel=0.0)
    # a porta: o pórtico de aço (as ombreiras e a verga chanfradas, à frente da pele de placas), as cantoneiras do vão, as
    # defensas listradas nos pés, a soleira e o rolo da porta lá dentro
    fx = x1 + PROUD
    p.box('CnMetal#frame', fx - 0.15, fx + 0.22, DOOR_Y0 - 0.55, DOOR_Y0, G, DOOR_Z + 0.5, bevel=0.06)
    p.box('CnMetal#frame', fx - 0.15, fx + 0.22, DOOR_Y1, DOOR_Y1 + 0.55, G, DOOR_Z + 0.5, bevel=0.06)
    p.box('CnMetal#frame', fx - 0.15, fx + 0.22, DOOR_Y0 - 0.55 + 0.002, DOOR_Y1 + 0.55 - 0.002, DOOR_Z, DOOR_Z + 0.5, bevel=0.06)
    for k in range(5):                                                          # os parafusos da verga (o ritmo)
        yy = DOOR_Y0 + 0.4 + k * (DOOR_Y1 - DOOR_Y0 - 0.8) / 4
        p.cyl('Dark#bolt', (fx + 0.23, yy, DOOR_Z + 0.25), 'x', 0.05, 0.04, seg=8, bevel=0.0)
    for yy in (DOOR_Y0, DOOR_Y1):
        a, b = (yy, yy + 0.08) if yy == DOOR_Y0 else (yy - 0.08, yy)
        p.box('CnMetal#frame', x1 - WT, fx + 0.1, a, b, G, DOOR_Z, bevel=0.02)
    for yy in (DOOR_Y0 - 0.28, DOOR_Y1 + 0.28):
        for k in range(6):
            z = G + k * 0.26
            p.box('Hazard' if k % 2 == 0 else 'Dark', fx + 0.22, fx + 0.28, yy - 0.2, yy + 0.2, z, z + 0.26, bevel=0.0)
    p.box('Hazard', x1 - 0.3, fx + 0.2, DOOR_Y0, DOOR_Y1, G, G + 0.015, bevel=0.0)
    p.cyl('CnMetal#frame', (x1 - WT - 0.4, YC, DOOR_Z - 0.32), 'y', 0.3, DOOR_Y1 - DOOR_Y0 - 0.1, seg=16, bevel=0.0)   # o rolo da porta
    # o fundo: o mezanino com o guarda-corpo e os pilares, a escada, a porta de dentro com a luz, a bancada, as caixas, o gancho
    bx = DOOR_D
    p.box('CnMetal#mez', bx, bx + 1.6, y0 + WT, y1 - WT, G + 2.9, G + 3.1, bevel=0.012)
    railing(p, [(bx + 1.55, y0 + WT + 0.1), (bx + 1.55, y1 - WT - 0.1)], G + 3.1, h=0.95, m='Trim', post_step=1.0)
    for y in (y0 + WT + 0.6, YC - 1.5, YC + 1.5, y1 - WT - 0.6):
        p.box('CnMetal#mez', bx + 1.35, bx + 1.5, y - 0.08, y + 0.08, G, G + 2.9, bevel=0.0)
    for k in range(10):                                                          # a escada do mezanino
        p.box('CnMetal#mez', bx + 1.6 + k * 0.28, bx + 1.6 + (k + 1) * 0.28, y1 - WT - 1.0, y1 - WT - 0.1, G, G + 0.29 * (10 - k), bevel=0.0)
    p.box('Dark', bx + 0.0, bx + 0.03, YC - 1.3, YC + 1.3, G, G + 2.6, bevel=0.0)                               # a porta de dentro
    p.box('Light', bx + 0.01, bx + 0.05, YC - 1.25, YC + 1.25, G + 2.65, G + 2.75, bevel=0.0)
    p.box('CnMetal#mez', bx + 2.4, bx + 4.6, DOOR_Y0 + 0.05, DOOR_Y0 + 0.8, G + 0.85, G + 0.95, bevel=0.02)   # a bancada
    for xx in (bx + 2.5, bx + 4.5):
        p.box('CnMetal#mez', xx - 0.04, xx + 0.04, DOOR_Y0 + 0.1, DOOR_Y0 + 0.7, G, G + 0.85, bevel=0.0)
    for (xx, yy, n) in ((bx + 2.8, DOOR_Y1 - 0.5, 2), (bx + 3.9, DOOR_Y1 - 0.55, 1), (bx + 0.9, y0 + WT + 0.8, 2)):
        for k in range(n):
            p.box('Crate', xx - 0.45, xx + 0.45, yy - 0.35, yy + 0.35, G + k * 0.62, G + k * 0.62 + 0.6, bevel=0.03)
    p.cyl('Dark', (bx + 5.0, YC + 1.0, zt - 1.2), 'z', 0.025, 1.6, seg=6, bevel=0.0)                               # a corrente
    p.box('Hazard', bx + 4.85, bx + 5.15, YC + 0.85, YC + 1.15, zt - 2.25, zt - 2.0, bevel=0.0)
    # o teto: o alçapão
    p.box('CnMetal#cap', -8.4, -7.8, 6.0, 6.5, H, H + 0.22, bevel=0.02)


def pagoda(p):
    """O pagode de 2 telhados em cima do hangar (o elemento-assinatura da facção, a ficha) e a torre quadrada na frente,
    com o estandarte, a grelha da base, as janelas recuadas, a cornija, o mastro dos alto-falantes (8 cornetas) e as antenas."""
    H = HALL_Z
    # o soco do pagode: o pedestal chanfrado (sobe acima do parapeito do hangar) e a borda de capa
    p.box('CnConcrete#soco', T1['cx'] - T1['ax'] - 0.5, T1['cx'] + T1['ax'] + 0.5, T1['cy'] - T1['ay'] - 0.5, T1['cy'] + T1['ay'] + 0.5, H, T1['z'] - 0.08, bevel=0.1)
    p.box('CnPlinth#cap', T1['cx'] - T1['ax'] - 0.58, T1['cx'] + T1['ax'] + 0.58, T1['cy'] - T1['ay'] - 0.58, T1['cy'] + T1['ay'] + 0.58, T1['z'] - 0.1, T1['z'], bevel=0.03)
    # o 1º andar (a parede curta com as janelinhas e as colunas vermelhas) e o 1º telhado, truncado (o 2º senta nele)
    pagoda_story(p, T1['cx'], T1['cy'], T1['ax'], T1['ay'], T1['z'], T1['wall'])
    z1 = T1['z'] + T1['wall']
    top1, tx1, ty1 = cn_roof(p, T1['cx'], T1['cy'], T1['ax'], T1['ay'], z1, T1['rise'], over=T1['over'], inset=T1['inset'], curl=0.5)
    p.box('CnRoof', T1['cx'] - tx1, T1['cx'] + tx1, T1['cy'] - ty1, T1['cy'] + ty1, top1 - 0.05, top1 + 0.05, bevel=0.0)
    # o 2º andar e o 2º telhado (o quadril inteiro, a cumeeira de lado a lado, como na ficha)
    pagoda_story(p, T2['cx'], T2['cy'], T2['ax'], T2['ay'], top1, T2['wall'], win_step=1.2)
    z2 = top1 + T2['wall']
    top2, _, _ = cn_roof(p, T2['cx'], T2['cy'], T2['ax'], T2['ay'], z2, T2['rise'], over=T2['over'], curl=0.45)
    # a torre na frente do pagode: o bloco oco de placas que sobe do teto do hangar, atravessa o 1º telhado e passa da cumeeira
    tx0, tx1_, ty0, ty1 = TOWER_X0, TOWER_X1, YC - 1.0, YC + 1.0
    wins = [(-0.35, 0.35, 9.0, 9.6), (-0.35, 0.35, 9.9, 10.5)]
    cn_block(p, tx0, tx1_, ty0, ty1, H, TOWER_Z, t=0.3, skin=('+x', '-x', '+y', '-y'),
             holes={'+x': [(ty0 + 0.4, ty1 - 0.4, H + 0.3, H + 1.05)], '+y': wins, '-y': wins},
             roof_t=0.3, parapet=0.0, corners=True, pier=0.5, interior=False, pw=1.2, zs=[H, H + 1.5, 8.6], plinth=0)
    for f, at in (('+y', ty1), ('-y', ty0)):
        F = WallFace(f, at, 0.0)
        for (a, b, c, d) in wins:
            cn_window(p, F, a, b, c, d, t=0.3, mull=0)
    louver(p, tx1_ - 0.25, tx1_, ty0 + 0.4, ty1 - 0.4, H + 0.3, H + 1.05, '+x', n=4)                           # a grelha da base
    wall_banner(p, tx1_ + PROUD, YC, TOWER_Z - 0.75, '+x', w=1.35, h=3.1, notch=0.42)
    # o mastro dos alto-falantes: o pedestal, o poste quadrado com 2 níveis de cornetas (4 em cada), a tampa e as antenas
    mx, my = (tx0 + tx1_) / 2, YC
    p.box('CnConcrete#soco', mx - 0.7, mx + 0.7, my - 0.7, my + 0.7, TOWER_Z, TOWER_Z + 0.25, bevel=0.06)
    p.box('CnMetal', mx - 0.3, mx + 0.3, my - 0.3, my + 0.3, TOWER_Z + 0.25, TOWER_Z + 2.1, bevel=0.02)
    for (zz, yaw0, big) in ((TOWER_Z + 0.8, 0.0, 0.58), (TOWER_Z + 1.65, 0.0, 0.5)):
        for k in range(4):
            a = math.radians(yaw0 + 90 * k)
            d = (math.cos(a) * 0.97, math.sin(a) * 0.97, -0.2)
            horn(p, (mx + math.cos(a) * 0.38, my + math.sin(a) * 0.38, zz), d, r1=big if k == 0 else big * 0.88, L=1.15)
        p.box('CnMetal', mx - 0.42, mx + 0.42, my - 0.42, my + 0.42, zz - 0.08, zz + 0.0, bevel=0.0)              # o anel do nível
    p.box('CnMetal', mx - 0.4, mx + 0.4, my - 0.4, my + 0.4, TOWER_Z + 2.1, TOWER_Z + 2.22, bevel=0.01)
    for (dx, dy, h) in ((0.15, 0.15, 0.6), (-0.18, 0.1, 0.45), (0.05, -0.2, 0.35)):
        p.cyl('Trim', (mx + dx, my + dy, TOWER_Z + 2.22 + h / 2), 'z', 0.025, h, seg=6, bevel=0.0)
        p.cyl('Beacon' if h > 0.5 else 'Trim', (mx + dx, my + dy, TOWER_Z + 2.22 + h + 0.04), 'z', 0.05, 0.08, seg=8, bevel=0.0)
    from model_building_parts import cable
    cable(p, (mx + 0.3, my - 0.3, TOWER_Z + 1.9), (tx1_ + 0.2, ty0 + 0.2, TOWER_Z + 0.1), sag=0.12, r=0.02)
    return top2


def skylight(p, x, y, z, sx=1.4, sy=2.0):
    """A claraboia do teto: o meio-fio de concreto chanfrado e as lâminas de vidro em dente de serra."""
    p.box('CnConcrete#vent', x - sx / 2, x + sx / 2, y - sy / 2, y + sy / 2, z, z + 0.35, bevel=0.06)
    n = int(sy / 0.34)
    for k in range(n):
        yy = y - sy / 2 + 0.17 + k * (sy - 0.2) / n
        p.obox('Glass', (x, yy + 0.08, z + 0.38), (sx - 0.22, 0.24, 0.04), roll=25, bevel=0.0)
        p.obox('CnMetal#cap', (x, yy, z + 0.4), (sx - 0.18, 0.05, 0.12), bevel=0.0)


def wings(p):
    """As alas: a da esquerda (+Y: o bloco oco com a parede de fora inclinada, as janelas altas recuadas, os contrafortes, o
    estandarte, o teto com o exaustor e as claraboias, a porta dos fundos com o pórtico de vigas vermelhas) e o bloco do
    estandarte à direita (−Y)."""
    # ---- a ala +Y
    wx0, wx1, wy0, wy1, wz = -10.0, 3.0, HALL_Y1, 12.6, 5.0
    lean = 0.85 / (wz - G)
    wins = [(xx - 0.55, xx + 0.55, 3.25, 4.05) for xx in (-7.6, -4.4, -1.2)]
    door = (7.8, 9.0, G, G + 2.4)
    cn_block(p, wx0, wx1, wy0, wy1, G, wz, t=WT, lean={'+y': lean}, skin=('+x', '+y', '-x'),
             holes={'+y': wins, '-x': [door]}, roof_t=0.3, parapet=0.3, corners=True, pier=0.8, interior=True,
             cols=[(-4.0, (wy0 + wy1) / 2 - 0.4)], pw=2.6, zs=[G, G + 1.2, 3.1, 4.2, wz - 0.3])
    F = WallFace('+y', wy1, G, lean)
    for (a, b, c, d) in wins:
        cn_window(p, F, a, b, c, d, t=WT)
    for xx in (-6.0, -2.8, 0.4):                                                 # os contrafortes da face inclinada
        cn_buttress(p, F, xx, G, 3.0, w=0.7, d=0.55, top=0.7)
    wall_banner(p, wx1 + PROUD, 9.5, 4.3, '+x', w=1.2, h=2.9)
    lamp(p, wx1 + PROUD, 9.5, 4.55, face='+x')
    for yy in (7.7, 11.4):
        lamp(p, wx1 + PROUD, yy, 3.4, face='+x')
    # o teto: o exaustor grande, a fila de claraboias, o alçapão
    roof_vent(p, -1.2, 9.6, wz + 0.04, sx=1.8, sy=1.4, h=0.8)
    for k in range(3):
        skylight(p, -8.2 + k * 1.9, 9.4, wz + 0.04)
    p.box('CnMetal#cap', -4.6, -3.8, 10.6, 11.4, wz + 0.04, wz + 0.3, bevel=0.02)
    # a porta dos fundos da ala (no vão): a folha escura recuada, a luz, e o pórtico de vigas vermelhas encostadas
    Fb = WallFace('-x', wx0, G)
    cn_rollup(p, Fb, door[0], door[1], door[2], door[3], t=WT, open_frac=0.0)
    lamp(p, wx0 - PROUD, 8.4, G + 2.75, face='-x')
    for yy in (7.45, 9.35):
        p.obox('Player', (wx0 - 0.35, yy, G + 2.1), (0.3, 0.32, 4.2), pitch=-5, bevel=0.03)
    p.box('Player', wx0 - 0.55, wx0 - 0.1, 7.2, 9.6, G + 3.9, G + 4.25, bevel=0.03)
    # ---- o bloco do estandarte (−Y), entre o hangar e o forno
    bx0, bx1, by0, by1, bz = -8.0, 3.4, -7.4, HALL_Y0, 4.8
    cn_block(p, bx0, bx1, by0, by1, G, bz, t=WT, walls=('+x', '-x', '-y', '+y'), skin=('+x',), roof_t=0.3, parapet=0.3,
             corners=True, pier=0.8, interior=True, pw=2.6, zs=[G, G + 1.2, 3.0, bz - 0.3])
    wall_banner(p, bx1 + PROUD, -6.0, 4.15, '+x', w=1.15, h=2.7)
    lamp(p, bx1 + PROUD, -6.0, 4.4, face='+x')
    lamp(p, bx1 + PROUD, -5.0, 3.4, face='+x')


def furnace(p):
    """O forno (o bloco oco com a parede de lado inclinada e a BOCA ACESA funda: a câmara de fogo de 1,4 m dentro da parede de
    0,6 m, a moldura pesada de aço chanfrada com os parafusos, a grade baixa, a porta levantada, a coifa, a escória escorrendo
    pra calha de grade no piso, os frades de perigo), o bloco alto atrás (a caldeira) com a chaminé listrada e a plataforma, e o
    duto grande em cotovelo assentado no berço, que sai do bloco do estandarte e entra na caldeira."""
    fx0, fx1, fy0, fy1, fz = 0.6, 4.2, -12.6, -7.4, 4.4
    my, mw, mh = -10.0, 2.4, 2.5
    z0 = G + 0.15
    hole = (my - mw / 2, my + mw / 2, z0, z0 + mh)
    cn_block(p, fx0, fx1, fy0, fy1, G, fz, t=0.6, lean={'-y': 0.45 / (fz - G)}, walls=('+x', '-y', '+y'), skin=('+x', '-y'),
             holes={'+x': [hole]}, roof_t=0.3, parapet=0.25, corners=True, pier=0.8, interior=False, pw=2.2, zs=[G, G + 1.4, 3.0, fz - 0.3])
    # a câmara de fogo (dentro da parede: o fundo, os lados, o teto e o piso acesos)
    cx0, cx1 = fx1 - 1.5, fx1 - 0.55
    p.box('Fire', cx0, cx0 + 0.05, my - mw / 2 - 0.05, my + mw / 2 + 0.05, z0 - 0.05, z0 + mh + 0.05, bevel=0.0)
    for s in (-1, 1):
        p.box('Fire', cx0, cx1, my + s * (mw / 2 + 0.02) - 0.03, my + s * (mw / 2 + 0.02) + 0.03, z0, z0 + mh, bevel=0.0)
    p.box('Fire', cx0, cx1, my - mw / 2, my + mw / 2, z0 + mh - 0.02, z0 + mh + 0.02, bevel=0.0)
    p.box('Fire', cx0, fx1 + 0.05, my - mw / 2, my + mw / 2, z0 - 0.02, z0 + 0.02, bevel=0.0)
    for k in range(5):                                                           # as brasas (o monte aceso no fundo)
        p.obox('Fire', (cx0 + 0.35 + 0.12 * k, my - 0.8 + 0.4 * k, z0 + 0.12), (0.6, 0.5, 0.22), yaw=17 * k, bevel=0.0)
    # a moldura de aço na frente da pele (os pilares, a verga, a soleira), chanfrada, com os parafusos
    mx = fx1 + PROUD
    for s in (-1, 1):
        ya, yb = (my + mw / 2, my + mw / 2 + 0.55) if s > 0 else (my - mw / 2 - 0.55, my - mw / 2)
        p.box('CnMetal#frame', mx - 0.2, mx + 0.6, ya, yb, G, z0 + mh + 0.55, bevel=0.08)
        for k in range(5):                                                       # os parafusos da moldura
            zz = z0 + 0.3 + k * 0.55
            p.cyl('Dark#bolt', (mx + 0.62, my + s * (mw / 2 + 0.27), zz), 'x', 0.055, 0.05, seg=8, bevel=0.0)
    p.box('CnMetal#frame', mx - 0.2, mx + 0.6, my - mw / 2 - 0.55 + 0.002, my + mw / 2 + 0.55 - 0.002, z0 + mh, z0 + mh + 0.55, bevel=0.08)
    p.box('CnMetal#frame', mx - 0.1, mx + 0.7, my - mw / 2 - 0.2, my + mw / 2 + 0.2, G, z0, bevel=0.04)
    for k in range(1, 6):                                                        # a grade baixa
        yy = my - mw / 2 + k * mw / 6
        p.box('Dark', mx + 0.2, mx + 0.28, yy - 0.04, yy + 0.04, z0, z0 + 0.9, bevel=0.0)
    p.box('Dark', mx + 0.2, mx + 0.28, my - mw / 2, my + mw / 2, z0 + 0.85, z0 + 0.95, bevel=0.0)
    # a porta do forno levantada (a chapa grossa por cima da boca) e a coifa inclinada
    p.box('CnMetal#frame', mx + 0.6, mx + 0.82, my - mw / 2 - 0.3, my + mw / 2 + 0.3, z0 + mh + 0.05, z0 + mh + 1.0, bevel=0.05)
    p.prism_xz('CnMetal#frame', [(mx - 0.2, z0 + mh + 1.05), (mx + 1.35, z0 + mh + 0.95), (mx + 1.35, z0 + mh + 1.07), (mx - 0.2, z0 + mh + 1.6)],
               my - mw / 2 - 0.7, my + mw / 2 + 0.7, bevel=0.03)
    lamp(p, mx, my - mw / 2 - 1.05, z0 + mh + 0.4, face='+x')
    lamp(p, mx, my + mw / 2 + 1.05, z0 + mh + 0.4, face='+x')
    # a escória escorrendo da boca pra calha, e a calha com a grade por cima (brilha no piso da frente)
    p.obox('Fire', (mx + 0.95, my, G + 0.06), (0.7, 1.0, 0.08), pitch=-12, bevel=0.0)
    cx0_, cx1_ = mx + 0.7, 8.0
    p.box('CnMetal', cx0_, cx1_, my - 0.95, my + 0.95, G - 0.3, G + 0.05, bevel=0.0)
    p.box('Fire', cx0_, cx1_ - 0.1, my - 0.75, my + 0.75, G - 0.06, G - 0.02, bevel=0.0)
    for k in range(int((cx1_ - cx0_) / 0.28)):
        xx = cx0_ + 0.14 + k * 0.28
        p.box('Dark', xx - 0.045, xx + 0.045, my - 0.8, my + 0.8, G - 0.02, G + 0.04, bevel=0.0)
    for yy in (my - 0.85, my + 0.85):
        p.box('CnMetal', cx0_, cx1_, yy - 0.05, yy + 0.05, G - 0.02, G + 0.05, bevel=0.0)
    # os frades de perigo dos lados da boca (o topo arredondado)
    for yy in (my - mw / 2 - 0.85, my + mw / 2 + 0.85):
        p.cyl('Hazard', (mx + 1.15, yy, G + 0.55), 'z', 0.17, 1.1, seg=12, bevel=0.0)
        for zz in (G + 0.35, G + 0.75):
            p.cyl('Dark', (mx + 1.15, yy, zz), 'z', 0.175, 0.14, seg=12, bevel=0.0)
        p.cyl('Dark', (mx + 1.15, yy, G + 1.13), 'z', 0.17, 0.08, seg=12, r2=0.1, bevel=0.0)
    p.box('CnMetal#cap', 1.6, 2.6, -11.6, -10.6, fz + 0.04, fz + 0.6, bevel=0.03)                             # a caixa de válvulas
    pipe_run(p, [(2.6, -11.1, fz + 0.3), (3.6, -11.1, fz + 0.3), (3.6, -11.1, fz + 0.9)], 0.12, feet=fz + 0.04, step=0.9)
    # ---- o bloco alto atrás (a caldeira), da altura do hangar: oco, de placas, com os contrafortes, a grelha, a chaminé
    bx0, bx1, by0, by1, bz = -11.6, fx0, -12.6, -7.4, HALL_Z
    lv = (bz - G)
    cn_block(p, bx0, bx1, by0, by1, G, bz, t=WT, skin=('-y', '-x'), holes={'-x': [(-11.4, -8.4, 2.0, 3.6)]},
             roof_t=0.35, parapet=0.3, corners=True, pier=0.8, interior=True, cols=[(-5.5, -10.0)], pw=2.6,
             zs=[G, G + 1.6, 3.6, fz, bz - 0.35])
    del lv
    louver(p, bx0 - 0.02, bx0, -11.4, -8.4, 2.0, 3.6, '-x', n=5)
    Fs = WallFace('-y', by0, G)
    for xx in (-9.6, -6.4, -3.2):
        cn_buttress(p, Fs, xx, G, 4.8, w=0.75, d=0.45, top=0.6)
    lamp(p, -8.0, by0 - PROUD, 4.0, face='-y')
    lamp(p, -1.6, by0 - PROUD, 4.0, face='-y')
    roof_vent(p, -3.4, -10.6, bz + 0.04, sx=2.0, sy=1.5, h=0.8)
    p.box('CnMetal#cap', -1.6, -0.4, -11.9, -11.2, bz + 0.04, bz + 0.3, bevel=0.02)
    # os tubos do teto, nos cavaletes (o cano assentado: as braçadeiras e os pés)
    pipe_run(p, [(-6.6, -8.2, bz + 0.5), (-0.2, -8.2, bz + 0.5)], 0.16, feet=bz + 0.04, step=1.6)
    pipe_run(p, [(-6.6, -8.65, bz + 0.42), (-1.4, -8.65, bz + 0.42), (-1.4, -9.6, bz + 0.42)], 0.11, feet=bz + 0.04, step=1.6)
    louver(p, bx1 + PROUD, bx1 + PROUD + 0.01, -9.2, -8.0, fz + 0.5, fz + 1.2, '+x', n=3)
    cc = (-8.6, -10.0)
    p.cyl('CnConcrete#soco', (cc[0], cc[1], bz + 0.35), 'z', 1.55, 0.6, seg=32, bevel=0.06)                   # o pé da chaminé
    p.cyl('CnConcrete#soco', (cc[0], cc[1], bz + 0.78), 'z', 1.3, 0.3, seg=32, bevel=0.06)
    chimney5(p, cc, 1.05, 0.88, bz + 0.9, 11.8 - bz - 0.9 - 0.22)
    disc_ring(p, 'CnMetal', cc, 2.0, 1.1, bz + 1.9, bz + 2.0, seg=32)                                          # a plataforma
    for k in range(12):
        a = 2 * math.pi * (k + 0.5) / 12
        p.cyl('Trim', (cc[0] + 1.95 * math.cos(a), cc[1] + 1.95 * math.sin(a), bz + 2.45), 'z', 0.03, 0.9, seg=6, bevel=0.0)
        p.obox('CnMetal', (cc[0] + 1.55 * math.cos(a), cc[1] + 1.55 * math.sin(a), bz + 1.8), (0.85, 0.08, 0.16), yaw=math.degrees(a), bevel=0.0)
    p.torus('Trim', (cc[0], cc[1], bz + 2.88), 'z', 1.95, 0.035, seg=32, rseg=6)
    ladder(p, cc[0] + 1.04, cc[1] - 0.25, cc[1] + 0.25, bz + 2.0, bz + 4.8)
    ladder(p, cc[0] + 1.75, cc[1] - 0.25, cc[1] + 0.25, bz, bz + 1.9)
    # o duto grande em cotovelo: sai do teto do bloco do estandarte, sobe, vira 90 graus e entra na parede da caldeira
    dx, y_up, R, r = 1.2, -6.0, 0.75, 0.48
    zs = 4.8 + 0.35
    elbow = arc((dx, y_up - R, zs + 0.2), R, 0, 90, 10, plane='yz')
    tube(p, 'CnMetal', [(dx, y_up, 4.8)] + elbow + [(dx, -7.45, zs + 0.2 + R)], r, seg=24)
    for q in ((dx, y_up, 4.95, 'z'), (dx, -7.3, zs + 0.2 + R, 'y'), (dx, y_up - R + 0.02, zs + 0.2 + R, 'y')):
        p.torus('Trim', (q[0], q[1], q[2]), q[3], r + 0.06, 0.06, seg=24, rseg=6)                                  # as flanges
    p.box('CnConcrete#soco', dx - 0.9, dx + 0.9, y_up - 0.9, y_up + 0.9, 4.8 + 0.02, 5.0, bevel=0.06)          # a base do duto
    # o berço do duto (a sela de aço que segura o trecho deitado)
    p.box('CnMetal#cap', dx - 0.55, dx + 0.55, -7.15, -6.95, 4.8 + 0.04, zs + 0.2 + R - r * 0.6, bevel=0.03)
    # o tubo fino que acompanha o duto (o vapor) e a válvula
    pipe_run(p, [(dx - 0.95, -5.4, 4.8), (dx - 0.95, -5.4, 5.6), (dx - 0.95, -7.45, 5.6)], 0.12, step=0.9)
    p.box('Hazard', dx - 1.1, dx - 0.8, -6.5, -6.2, 5.45, 5.75, bevel=0.0)


def back(p):
    """Os fundos: o anexo oco com a porta de enrolar recuada, os contrafortes, a gaiola com os tambores, a escada de mão até
    o teto."""
    x0 = HALL_X0
    cn_block(p, -11.0, x0, -2.4, 3.4, G, 4.16, t=WT, walls=('-x', '-y', '+y'), skin=('-x', '-y', '+y'),
             holes={'-x': [(-1.4, 1.4, G, G + 3.2)]}, roof_t=0.3, parapet=0.25, corners=True, pier=0.7, interior=False,
             pw=2.0, zs=[G, G + 1.3, 3.5, 3.86])
    cn_rollup(p, WallFace('-x', -11.0, G), -1.4, 1.4, G, G + 3.2, t=WT, open_frac=0.0)
    p.box('Hazard', -11.0 - PROUD - 0.06, -11.0 - PROUD, -1.62, -1.42, G, 3.3, bevel=0.0)
    p.box('Hazard', -11.0 - PROUD - 0.06, -11.0 - PROUD, 1.42, 1.62, G, 3.3, bevel=0.0)
    lamp(p, -11.0 - PROUD, 2.3, 3.5, face='-x')
    roof_vent(p, -10.0, 0.5, 4.2, sx=1.2, sy=2.2, h=0.5)
    for yy in (-4.0, 4.4, 6.2):
        buttress(p, x0 - PROUD, yy, '-x', 5.4, w=0.75, d=0.6)
    ladder(p, x0 - PROUD - 0.12, -3.4, -2.9, G, HALL_Z + 0.3)
    # a gaiola de tela com os tambores (canto de trás, +Y)
    gx0, gx1, gy0, gy1, gh = -11.85, -10.35, 9.9, 12.5, 2.3
    for (xx, yy) in ((gx0, gy0), (gx0, gy1), (gx1, gy0), (gx1, gy1)):
        p.cyl('CnMetal', (xx, yy, G + gh / 2), 'z', 0.05, gh, seg=8, bevel=0.0)
    for zz in (G + 0.05, G + gh):
        for yy in (gy0, gy1):
            p.box('CnMetal', gx0, gx1, yy - 0.03, yy + 0.03, zz - 0.03, zz + 0.03, bevel=0.0)
        for xx in (gx0, gx1):
            p.box('CnMetal', xx - 0.03, xx + 0.03, gy0 + 0.03, gy1 - 0.03, zz - 0.03, zz + 0.03, bevel=0.0)
    for k in range(1, 8):                                                          # a tela (os arames, ≥ 4 cm: lê como ritmo)
        yy = gy0 + (gy1 - gy0) * k / 8
        p.box('Trim', gx0 - 0.02, gx0 + 0.02, yy - 0.02, yy + 0.02, G, G + gh, bevel=0.0)
    for k in range(1, 7):
        zz = G + gh * k / 7
        p.box('Trim', gx0 - 0.02, gx0 + 0.02, gy0, gy1, zz - 0.02, zz + 0.02, bevel=0.0)
    for k in range(1, 5):
        xx = gx0 + (gx1 - gx0) * k / 5
        for yy in (gy0, gy1):
            p.box('Trim', xx - 0.02, xx + 0.02, yy - 0.02, yy + 0.02, G, G + gh, bevel=0.0)
    from model_faction_kit import barrel
    for (xx, yy) in ((-11.4, 10.4), (-10.8, 10.55), (-11.35, 11.1), (-10.8, 11.25), (-11.4, 11.85)):
        barrel(p, xx, yy, G, r=0.28, h=0.9, m='CnMetal#drum', ring='Hazard')
    p.box('Crate', -11.0, -10.45, 11.75, 12.35, G, G + 0.6, bevel=0.03)


def base():
    p = BPart(PREFIX + '_Base')
    podium(p)
    hall(p)
    top = pagoda(p)
    wings(p)
    furnace(p)
    back(p)
    print('MODEL pagode ate', round(top, 2), 'm')
    return p.finish()


# ===================================================================================================== o pórtico rolante

def gantry():
    """O pórtico rolante vermelho (cor do jogador) sobre a porta: as 2 pernas em cavalete (as colunas inclinadas, as
    travessas e o X), os truques com as rodas de aba no trilho e os para-choques listrados, a viga-caixão com as nervuras e
    a placa da estrela dourada com o anel de parafusos, o carro do guincho com o moitão e o gancho, as balizas nas pontas, a
    escada numa perna."""
    p = BPart(PREFIX + '_Gantry')
    X = GANTRY_X
    for i, y in enumerate(RAIL_Y):
        # o truque: a viga de baixo com as 2 rodas, os motores e os para-choques
        p.box('Player', X - 1.75, X + 1.75, y - 0.38, y + 0.38, G + 0.55, G + 1.05, bevel=0.05)
        for xx in (X - 1.15, X + 1.15):
            p.cyl('CnMetal', (xx, y, G + 0.06 + 0.32), 'y', 0.32, 0.2, seg=24, bevel=0.01)
            for s in (-1, 1):                                                   # as abas da roda (o friso do trilho)
                p.cyl('CnMetal', (xx, y + s * 0.12, G + 0.38), 'y', 0.37, 0.04, seg=24, bevel=0.0)
            p.cyl('Dark', (xx, y, G + 0.38), 'y', 0.12, 0.34, seg=12, bevel=0.0)
            p.box('Player', xx - 0.42, xx + 0.42, y - 0.42, y - 0.18, G + 0.2, G + 0.6, bevel=0.02)            # as guardas da roda
            p.box('Player', xx - 0.42, xx + 0.42, y + 0.18, y + 0.42, G + 0.2, G + 0.6, bevel=0.02)
        p.box('CnMetal', X - 0.45, X + 0.45, y + (0.38 if i == 1 else -0.78), y + (0.78 if i == 1 else -0.38), G + 0.55, G + 1.0, bevel=0.03)
        for s in (-1, 1):
            p.box('Hazard', X + s * 1.75 - (0.0 if s > 0 else 0.22), X + s * 1.75 + (0.22 if s > 0 else 0.0), y - 0.3, y + 0.3, G + 0.6, G + 1.0, bevel=0.02)
            p.box('Dark', X + s * 1.75 - (0.0 if s > 0 else 0.23), X + s * 1.75 + (0.23 if s > 0 else 0.0), y - 0.31, y + 0.31, G + 0.75, G + 0.85, bevel=0.0)
        # o cavalete: 2 colunas-caixão inclinadas do truque até a viga, as travessas e o X entre elas
        zb, zt = G + 1.05, BEAM_Z0
        for s in (-1, 1):
            xb, xt = X + s * 1.35, X + s * 0.32
            L = math.hypot(xb - xt, zt - zb)
            ang = math.degrees(math.atan2(xb - xt, zt - zb))
            p.obox('Player', ((xb + xt) / 2, y, (zb + zt) / 2), (0.42, 0.62, L), pitch=ang, bevel=0.04)
        for zz in (G + 2.3, G + 3.6):
            t = (zz - zb) / (zt - zb)
            w = 1.35 + (0.32 - 1.35) * t
            p.box('Player', X - w, X + w, y - 0.22, y + 0.22, zz - 0.14, zz + 0.14, bevel=0.03)
        for (za, zb_) in ((G + 1.05, G + 2.3), (G + 2.3, G + 3.6)):
            for s in (-1, 1):
                ta, tb = (za - zb) / (zt - zb), (zb_ - zb) / (zt - zb)
                xa = X + s * (1.35 + (0.32 - 1.35) * ta) * 0.8
                xb_ = X - s * (1.35 + (0.32 - 1.35) * tb) * 0.8
                d = Vector((xb_ - xa, 0.0, zb_ - za))
                p.cyl('Player', ((xa + xb_) / 2, y, (za + zb_) / 2), (d.x, d.y, d.z), 0.07, d.length, seg=8, bevel=0.0)
        # a placa da perna com os parafusos (a junta com a viga)
        p.box('CnMetal', X - 0.55, X + 0.55, y - 0.45, y + 0.45, BEAM_Z0 - 0.12, BEAM_Z0, bevel=0.02)
    ladder(p, X + 0.7, RAIL_Y[1] + 0.34, RAIL_Y[1] + 0.74, G + 1.05, BEAM_Z0, m='Trim')
    # a viga-caixão (de perna a perna, um pouco além), com as abas, as nervuras e as tampas
    y0, y1 = RAIL_Y[0] - 0.75, RAIL_Y[1] + 0.75
    p.box('Player', X - 0.5, X + 0.5, y0, y1, BEAM_Z0, BEAM_Z1, bevel=0.05)
    p.box('Player', X - 0.62, X + 0.62, y0 - 0.05, y1 + 0.05, BEAM_Z1 - 0.1, BEAM_Z1 + 0.04, bevel=0.02)       # a aba de cima
    p.box('Player', X - 0.6, X + 0.6, y0 - 0.05, y1 + 0.05, BEAM_Z0 - 0.04, BEAM_Z0 + 0.08, bevel=0.02)        # a aba de baixo
    for k in range(9):
        yy = y0 + 0.5 + k * (y1 - y0 - 1.0) / 8
        if abs(yy - YC) < 1.0:
            continue
        for s in (-1, 1):
            p.box('Player', X + s * 0.5 - (0.0 if s > 0 else 0.07), X + s * 0.5 + (0.07 if s > 0 else 0.0), yy - 0.05, yy + 0.05, BEAM_Z0 + 0.08, BEAM_Z1 - 0.1, bevel=0.0)
    for yy in (y0, y1):
        p.box('CnMetal', X - 0.52, X + 0.52, yy - 0.04 if yy == y0 else yy, yy if yy == y0 else yy + 0.04, BEAM_Z0, BEAM_Z1, bevel=0.0)
    # a placa da estrela no meio da viga (na frente), a estrela dourada no círculo, o anel de parafusos
    zc = (BEAM_Z0 + BEAM_Z1) / 2 + 0.03
    p.box('Player', X + 0.5, X + 0.62, YC - 0.75, YC + 0.75, BEAM_Z0 - 0.15, BEAM_Z1 + 0.2, bevel=0.03)
    p.cyl('CnGold', (X + 0.63, YC, zc), 'x', 0.6, 0.02, seg=32, bevel=0.0)
    p.cyl('Player', (X + 0.645, YC, zc), 'x', 0.53, 0.02, seg=32, bevel=0.0)
    star_wall(p, (X + 0.655, YC, zc), '+x', 0.46, m='CnGold', t=0.02)
    for k in range(10):
        a = 2 * math.pi * k / 10
        p.cyl('CnGold', (X + 0.645, YC + 0.66 * math.cos(a), zc + 0.66 * math.sin(a)), 'x', 0.045, 0.04, seg=8, bevel=0.0)
    # a mesma estrela atrás (o pórtico é visto dos dois lados)
    p.box('Player', X - 0.62, X - 0.5, YC - 0.75, YC + 0.75, BEAM_Z0 - 0.15, BEAM_Z1 + 0.2, bevel=0.03)
    star_wall(p, (X - 0.63, YC, zc), '-x', 0.42, m='CnGold', t=0.02)
    # o carro do guincho (correndo por baixo da viga), o tambor, os cabos e o moitão com o gancho
    ty = YC + 1.6
    p.box('CnMetal', X - 0.62, X + 0.62, ty - 0.7, ty + 0.7, BEAM_Z0 - 0.62, BEAM_Z0 - 0.04, bevel=0.03)
    p.cyl('Dark', (X, ty, BEAM_Z0 - 0.42), 'y', 0.24, 1.2, seg=16, bevel=0.0)
    p.box('Player', X - 0.63, X + 0.63, ty - 0.71, ty + 0.71, BEAM_Z0 - 0.2, BEAM_Z0 - 0.12, bevel=0.0)
    for dy in (-0.18, 0.18):
        p.cyl('Dark', (X, ty + dy, BEAM_Z0 - 0.9), 'z', 0.02, 0.6, seg=6, bevel=0.0)
    p.box('Hazard', X - 0.22, X + 0.22, ty - 0.3, ty + 0.3, BEAM_Z0 - 1.5, BEAM_Z0 - 1.2, bevel=0.03)          # o moitão
    for k in range(3):
        p.box('Dark', X - 0.23, X + 0.23, ty - 0.3 + k * 0.24, ty - 0.24 + k * 0.24, BEAM_Z0 - 1.5, BEAM_Z0 - 1.2, bevel=0.0)
    p.torus('CnMetal', (X, ty, BEAM_Z0 - 1.78), 'x', 0.17, 0.05, seg=16, rseg=6)
    # as balizas e as luzes nas pontas da viga
    for yy in (y0 + 0.25, y1 - 0.25):
        p.cyl('Dark', (X, yy, BEAM_Z1 + 0.1), 'z', 0.1, 0.12, seg=12, bevel=0.0)
        p.cyl('Beacon', (X, yy, BEAM_Z1 + 0.24), 'z', 0.08, 0.16, seg=12, bevel=0.0, smooth=True)
        p.box('Dark', X + 0.5, X + 0.64, yy - 0.2, yy + 0.2, BEAM_Z0 + 0.15, BEAM_Z0 + 0.4, bevel=0.0)
        p.box('Light', X + 0.64, X + 0.66, yy - 0.16, yy + 0.16, BEAM_Z0 + 0.19, BEAM_Z0 + 0.36, bevel=0.0)
    return p.finish()


def main():
    os.makedirs(OUT, exist_ok=True)
    clear_scene()
    total = export(base(), OUT)
    clear_scene()
    total += export(gantry(), OUT)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -HX + 1.9, 8.6 - 1.9, -HY + 1.9, HY - 1.9, HALL_Z + 0.5, post='CnMetal'), OUT)
    print('MODEL total', total, 'tris')
    print('MODEL lote 24 x 26 m; porta', DOOR_Y1 - DOOR_Y0, 'x', round(DOOR_Z - G, 2), 'm; gantry slide X 0 ..', GANTRY_TRAVEL,
          '(pernas em X', GANTRY_X, ', trilhos Y', RAIL_Y, ')')
    print('MODEL fire_points_m', [(-8.4, -3.2, HALL_Z + 0.05), (-6.0, 10.6, 5.05), (-3.6, -11.2, HALL_Z + 0.05)])


if __name__ == '__main__':      # os outros prédios da China importam o telhado, o estandarte e as cornetas daqui
    main()
