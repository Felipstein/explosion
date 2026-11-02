"""O tanque da GLA v2 (o "Scorpion": o T-55 tomado pelos rebeldes), remodelado do zero no detalhe AAA (01/11/2026).

Rodar (Blender sem janela, só CPU; não usa a placa), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_gla_tank_v2.py C:/Users/luisf/explosion/captures/game/models/gla_tank_v2

O pedido (01/11, 19h40): "tem bolhas que parece que está doente, não parece sacos de areia, o pneu também não está posicionado na
diagonal como no game shot [...] precisa de uma modelagem nova, bem top [...] nada de quinas, nada de jogo de lego, detalhes top".
A v1 (Game/Tools/model_gla_tank.py, aprovada 13/10) fica como está; esta é a v2 que ele compara.

Os alvos: o game shot r1 (docs/aprovados/alvos/r1_raid_slabs_gemini.webp, os tanques da GLA à esquerda) e a ficha
docs/concept/units/gla_mbt_sheet.webp. O que eles têm e entra aqui:
  - os SACOS DE AREIA de verdade (sandbag): cada saco é um volume de pano com a costura achatada numa ponta (as "orelhas") e a boca
    franzida e amarrada na outra (as pregas, o nó, a sobra caída), o meio estufado, a barriga achatada; o saco ASSENTA no que está
    embaixo (raios contra o casco e os sacos já postos: o de cima deita por cima dos de baixo e cede no vão entre eles), escorrega um
    pouco no glacis (cisalhamento morro abaixo) e cai pela borda quando passa dela; as rugas de tensão no topo;
  - o PNEU sobressalente NA DIAGONAL atrás: em pé e tombado ~38° pra frente (a face olhando pra trás e pra cima), apoiado na quina do
    deck, no berço de barras soldado na traseira e amarrado com a corda em X por cima da face;
  - a torre fundida em cúpula (o perfil girado, liso), o mantelete arredondado com a capa de lona, o holofote;
  - as chapas soldadas de remendo (teal e ferrugem): curvadas na cúpula (a grade que segue a curva, a borda de maçarico, os
    parafusos e o cordão de solda), as saias da frente empenadas, o remendo no glacis;
  - a gaiola de barras chatas sobre as rodas de trás; os tanques externos de combustível no para-lama direito; as caixas, os
    caixotes de tábua, os galões e a lona enrolada, amarrados com corda;
  - a camuflagem pintada à mão (manchas teal e marrom com a borda irregular, coladas na superfície) e as faixas verdes da cor do
    jogador pintadas a pincel (a borda que treme, a ponta que afina): no glacis, no teto da torre e o esparadrapo no cano;
  - a esteira de elos (o passo do T-55, 137 mm) que ANDA no jogo, deitada por cima das rodas e cedendo entre elas (o T-55 não tem
    rolete de cima), e as rodas "starfish" de 5 raios.

As medidas da v1 (a jogabilidade usa): o casco 6,45 x 3,27 m, a torre girando em (0,35; 0), a boca a 2,125 m (x 6,19), as esteiras
em y = ±1,30 com 0,58 m. A torre já sai no tamanho final (a v1 escalava 1,3x na planta e 1,15x na altura no fim: aqui o perfil da
cúpula já vem escalado, as peças em cima não deformam).

Peças (origem no chão, no centro do casco, frente em +X; todas no lugar; os nomes da v1):
  SM_GLA_MBT_Hull     casco, para-lamas, saias, gaiola, carga, sacos, pneu, faixas e manchas;
  SM_GLA_MBT_Turret   a cúpula (gira em Z em volta de (TX, 0)), o mantelete, o holofote, as escotilhas, a DShK, as chapas, os sacos,
                      o cesto, as 2 bandeiras (o executor tira o pano pro flags.json) e a antena;
  SM_GLA_MBT_Barrel   o canhão de 100 mm com o evacuador na boca e o esparadrapo verde (recua em −X; a boca é o X máximo);
  SM_GLA_MBT_Wheels   a rodagem (o executor separa em wheel_001..: o jogo gira cada uma);
  SM_GLA_MBT_Track_l / _Track_r  as esteiras de elos (a tangente de cada elo no UV2/UV3: a esteira anda).
Slots (os do import_gla.py): Paint, Team, Bag, Patch, Rust, Wood, Metal, Dark, Rubber, Track, Lens (+ TeamFlag, que o executor tira).
"""
import json
import math
import os
import sys

from mathutils import Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, flag, track_path  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
from model_china_vehicle_kit import bounds, frame_box, hmg, solid, sweep_check, tris  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'gla_tank_v2')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_GLA_MBT'


def _srgb8(c):
    f = lambda x: 12.92 * x if x <= 0.0031308 else 1.055 * x ** (1 / 2.4) - 0.055
    return tuple(int(round(255 * f(x))) for x in c)


# as cores de conferência = as instâncias do jogo (import_gla.py, em linear) pra a folha ler como no jogo; o resto, as da v1
COLORS.update({'Paint': _srgb8((0.402, 0.314, 0.178)), 'Bag': _srgb8((0.456, 0.371, 0.235)), 'Patch': _srgb8((0.122, 0.172, 0.150)),
               'Rust': _srgb8((0.195, 0.076, 0.030)), 'Wood': _srgb8((0.181, 0.138, 0.064)),
               'Team': (78, 152, 58), 'Metal': (92, 90, 82), 'Dark': (40, 40, 38), 'Rubber': (32, 32, 31), 'Track': (58, 55, 50),
               'Lens': (18, 26, 40)})

# ---------------------------------------------------------------------------------- medidas (as da v1)
NOSE, REAR = 3.25, -3.2          # o casco: 6,45 m
BELLY, FENDER, DECK = 0.43, 1.10, 1.55
LOW, UP = 0.98, 1.635            # meia-largura do casco de baixo (entre as esteiras) e de cima (com os para-lamas)
TRACK_Y, TRACK_W = 1.30, 0.58
WHEELS_X = [2.10, 1.05, 0.12, -0.80, -1.72]
WHEEL_R, WHEEL_Z = 0.405, 0.47
IDLER = (2.98, 0.68, 0.30)       # (x, z, raio) a roda-guia
SPROCKET = (-2.72, 0.62, 0.32)   # a roda motriz
TX = 0.35                        # o eixo da torre (turret_pivot_m 0,385 com a escala 1,1 do jogo)
TS, TZ = 1.3, 1.15               # a torre da v1 crescida (29/10, a ficha): aqui já aplicado no perfil
GUN_Z = DECK + (2.05 - DECK) * TZ          # 2,125 (muzzle_height_m 2,338 com a escala 1,1)
BARREL_X0 = TX + 1.3 * TS                  # 2,04: o cano sai do mantelete
MUZZLE_X = 5.805 + 1.3 * (TS - 1.0)        # 6,195: a boca, como na v1
PITCH = 0.137                    # o passo do elo do T-55
T_IN, T_OUT, GROUSER = 0.03, 0.012, 0.016
HORN_W, HORN_H = 0.05, 0.07
ZUP = Vector((0.0, 0.0, 1.0))
GN = Vector((0.55, 0.0, 0.95)).normalized()          # a normal do glacis (30°)
GUP = Vector((-0.95, 0.0, 0.55)).normalized()        # o glacis morro acima

# a cúpula da v1 (distância ao eixo, altura acima do deck), escalada pro tamanho final
DOME0 = [(0.97, 0.0), (1.07, 0.10), (1.12, 0.25), (1.09, 0.42), (0.98, 0.57), (0.80, 0.70), (0.58, 0.79), (0.34, 0.845),
         (0.12, 0.862), (0.0, 0.875)]
DOME = [(d * TS, z * TZ) for d, z in DOME0]
DOME_C = Vector((TX + 0.05, 0.0, DECK - 0.25))      # o centro de onde as chapas e as faixas da cúpula são projetadas


def glacis_x(z):
    return NOSE - (z - 1.0) * (0.95 / 0.55)


def glacis_z(x):
    return 1.0 + (NOSE - x) * (0.55 / 0.95)


def fwd(h):
    """A cúpula bojuda pra frente (a da v1): o x anda com a altura."""
    return 0.05 * TS * min(max(h, 0.0) / (0.42 * TZ), 1.0)


def lerp(a, b, t):
    return a + (b - a) * t


def smooth01(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3.0 - 2.0 * t)


def h1(*a):
    """O sorteio fixo (o mesmo modelo sai sempre igual)."""
    s = 0.0
    for i, x in enumerate(a):
        s += x * (12.9898 + 37.719 * i) + 0.731 * i
    return (math.sin(s) * 43758.5453) % 1.0


# ---------------------------------------------------------------------------------- o que assenta em quê
class Support:
    """As superfícies onde as coisas assentam (o casco, a cúpula, os sacos já postos): raios contra as malhas da peça (coordenadas
    da Unreal; as malhas do Part estão no Blender, (x, −y, z))."""

    def __init__(self, items):
        self.trees = []
        for p, keys in items:
            for k in keys:
                bm = p.bms.get(k)
                if bm is not None and len(bm.faces):
                    bm.normal_update()                       # sem isso a BVH devolve a normal zero (as pinturas colapsavam)
                    self.trees.append(BVHTree.FromBMesh(bm))

    def cast(self, o, d, dist=3.0):
        o, d = Vector(o), Vector(d).normalized()
        ob, db = V(*o), Vector((d.x, -d.y, d.z))
        best = None
        for t in self.trees:
            loc, nrm, _, dd = t.ray_cast(ob, db, dist)
            if loc is not None and (best is None or dd < best[2]):
                n = Vector((nrm.x, -nrm.y, nrm.z)).normalized()
                if n.dot(d) > 0:
                    n = -n
                best = (Vector((loc.x, -loc.y, loc.z)), n, dd)
        return best


def land(sup, q, up=ZUP, h=1.2):
    """O ponto da superfície embaixo de q (ao longo de −up) e a normal lá."""
    u = Vector(up).normalized()
    hit = sup.cast(Vector(q) + u * h, -u, 2 * h + 1.5)
    return (hit[0], hit[1]) if hit else (Vector(q), u)


# ---------------------------------------------------------------------------------- o saco de areia
# (02/11, a volta dos sacos: "menos bolha: mais chatos e quadrados, altura ~0,6 da largura, a dobra marcada na boca e na ponta, o
# lado de cima quase reto") as estações ao longo do saco: mais juntas nas pontas e em cima das 2 dobras (0,11 e 0,84)
BAG_A = [0.0, 0.03, 0.07, 0.11, 0.15, 0.25, 0.38, 0.5, 0.62, 0.74, 0.8, 0.84, 0.88, 0.94, 1.0]
NU, NK = len(BAG_A) - 1, 16


def _seg_dist(px, py, ax, ay, bx, by):
    vx, vy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((px - ax) * vx + (py - ay) * vy) / max(vx * vx + vy * vy, 1e-9)))
    return math.hypot(px - ax - vx * t, py - ay - vy * t)


def sandbag(p, sup, c, along, up=ZUP, L=0.56, W=0.34, H=0.2, seed=0.0, shear=0.2, tail=True, key='Bag#sack'):
    """O saco de areia cheio e deitado (02/11, o do alvo: um TIJOLO DE PANO, não uma bolha):
    - a seção quase retangular (a lona esticada pela areia): o topo quase reto, os lados quase em pé barrigando na altura de ~45%, os
      cantos redondos e o fundo chato; altura ~0,6 da largura;
    - a ponta costurada (achata de leve, quase da largura toda: as "orelhas") e a BOCA dobrada por cima (a aba de pano deitada no
      topo da ponta, com o cordão); as 2 DOBRAS marcadas atravessando o saco perto de cada ponta;
    - o PESO: cada coluna desce até o que está embaixo (raios em −up contra 'sup': o casco e os sacos já postos), suavizada pela largura
      do pano: o de cima deita por cima dos de baixo e cede no vão, a ponta que passa da borda cai; na rampa escorrega um pouco (shear);
    - as rugas de tensão que saem das orelhas, rasas.
    c = um ponto no apoio (o centro do saco); along = o comprimento (projetado no plano de 'up')."""
    ez = Vector(up).normalized()
    ex = Vector(along) - ez * Vector(along).dot(ez)
    ex.normalize()
    jy = math.radians((h1(seed, 1.7) - 0.5) * 12.0)          # ninguém empilha saco no esquadro
    ey0 = ez.cross(ex)
    ex = (ex * math.cos(jy) + ey0 * math.sin(jy)).normalized()
    if h1(seed, 9.1) > 0.5:                                  # a boca pra um lado ou pro outro
        ex = -ex
    ey = ez.cross(ex)
    L *= 0.93 + 0.1 * h1(seed, 2.3)
    W *= 0.95 + 0.07 * h1(seed, 3.1)
    H *= 0.9 + 0.15 * h1(seed, 4.7)
    C = Vector(c)
    down = Vector((0.0, 0.0, -1.0)) - ez * (-ez.z)
    down = down.normalized() if down.length > 1e-3 else Vector()
    cr = (h1(seed, 11.0) - 0.5)
    # ---- a forma livre (x ao longo, yy de lado, hh acima da base da coluna)
    shape = []
    for a in BAG_A:
        if a < 0.11:                                          # a ponta costurada: achata de leve, quase da largura toda
            e = math.sin(0.5 * math.pi * a / 0.11)
            w = W / 2 * (0.93 + 0.07 * e)
            ht = H * (0.24 + 0.76 * e ** 0.7)
        elif a > 0.84:                                        # a boca dobrada: afina pouco e baixa
            s_ = (a - 0.84) / 0.16
            w = W / 2 * (1.0 - 0.16 * s_)
            ht = H * (1.0 - 0.58 * s_ ** 1.3)
        else:
            w, ht = W / 2, H
        mid = math.sin(math.pi * min(max((a - 0.1) / 0.76, 0.0), 1.0))
        w *= 1.0 + 0.025 * mid
        ht *= 1.0 + 0.03 * mid
        x = L * (a - 0.5)
        rim = 0.45 * ht
        fold = math.exp(-((a - 0.11) / 0.03) ** 2) + math.exp(-((a - 0.84) / 0.03) ** 2)   # as 2 dobras marcadas
        row = []
        for j in range(NK):
            th = 2 * math.pi * j / NK
            ct, st = math.cos(th), math.sin(th)
            yn = math.copysign(abs(ct) ** 0.3, ct)
            if st >= 0:
                hh = rim + (ht - rim) * st ** 0.25                 # o topo quase reto
            else:
                hh = rim * (1.0 - abs(st) ** 0.35)                 # o fundo chato
                yn *= 1.0 - 0.06 * abs(st)
            yn *= 1.0 - 0.04 * fold
            if st > 0.1:
                hh -= 0.014 * fold * smooth01((st - 0.1) / 0.3)
                k_ = smooth01((st - 0.15) / 0.4)
                ym = yn * w
                for sg in (-1, 1):                                 # as rugas de tensão rasas que saem das orelhas
                    d_ = _seg_dist(x, ym, -L * 0.47, sg * w * 0.9, -L * (0.1 - 0.1 * cr), sg * w * 0.15)
                    hh -= 0.006 * math.exp(-(d_ / 0.03) ** 2) * k_
            row.append((x, yn * w, hh))
        shape.append(row)
    # ---- o peso: a base de cada coluna no apoio
    base = [[None] * NK for _ in range(NU + 1)]
    for i in range(NU + 1):
        for j in range(NK):
            x, yy, _ = shape[i][j]
            q = C + ex * x + ey * yy
            hit = sup.cast(q + ez * 0.7, -ez, 1.5)
            if hit is not None and (hit[0] - C).dot(ez) > -0.16:      # mais fundo que isso: o pano não desce, cai pela borda
                base[i][j] = (hit[0] - C).dot(ez)
    raw = [[b for b in r] for r in base]
    for _ in range(12):                                       # o que passou da borda cai (a vizinhança − 4 cm)
        miss = False
        for i in range(NU + 1):
            for j in range(NK):
                if base[i][j] is None:
                    nb = [base[ii][jj] for ii, jj in ((i - 1, j), (i + 1, j), (i, (j - 1) % NK), (i, (j + 1) % NK))
                          if 0 <= ii <= NU and base[ii][jj] is not None]
                    if nb:
                        base[i][j] = sum(nb) / len(nb) - 0.04
                    else:
                        miss = True
        if not miss:
            break
    base = [[(b if b is not None else -0.05) for b in r] for r in base]
    sm = [r[:] for r in base]
    for _ in range(4):                                        # a largura do pano: não copia cada calombo
        nx = [r[:] for r in sm]
        for i in range(NU + 1):
            for j in range(NK):
                nb = [sm[max(i - 1, 0)][j], sm[min(i + 1, NU)][j], sm[i][(j - 1) % NK], sm[i][(j + 1) % NK]]
                nx[i][j] = 0.5 * sm[i][j] + 0.125 * sum(nb)
        sm = nx
    rings = []
    for i in range(NU + 1):
        ring = []
        for j in range(NK):
            x, yy, hh = shape[i][j]
            b0, bs = raw[i][j], sm[i][j]
            if b0 is None:
                b = bs
            elif bs > b0:                                     # no vão: cede até 55% do caminho
                b = b0 + 0.55 * (bs - b0)
            else:                                             # no calombo: afunda até 2 cm (o pano aperta)
                b = max(bs, b0 - 0.02)
            q = C + ex * x + ey * yy + ez * (b + hh) + down * (shear * hh)
            ring.append(tuple(q))
        rings.append(ring)
    p.loft(key, rings, smooth=True)
    # a aba da costura (a ponta achatada costurada)
    c0 = sum((Vector(q) for q in rings[0]), Vector()) / NK
    vp.prism(p, key, vp.rounded_rect(W * 0.88, 0.016, 0.007, 2), c0 + ex * 0.004, ey, ez, -ex, 0.024)
    if tail:
        # a boca DOBRADA por cima: a aba de pano deitada no topo da ponta, da dobra (0,84) até passar da ponta, e o cordão na raiz
        top = NK // 4                                          # o ponto de cima de cada anel (θ = 90°)
        t_fold = Vector(rings[BAG_A.index(0.84)][top])
        t_end = Vector(rings[-1][top]) + ex * 0.035 - ez * 0.03
        fx = (t_end - t_fold).normalized()
        fz = fx.cross(ey).normalized()
        if fz.dot(ez) < 0:
            fz = -fz
        ln = (t_end - t_fold).length + 0.02
        vp.rbox_q(p, key, (t_fold + t_end) / 2 + fz * 0.009, (ln, W * 0.8, 0.014), r=0.035, k=2, frame=(fx, ey, fz))
        vp.rbox_q(p, 'Wood', t_fold + fx * 0.03 + fz * 0.018, (0.014, W * 0.84, 0.008), r=0.004, k=1, frame=(fx, ey, fz))
    return rings


def bag_pile(p, base_keys, layers, up_default=ZUP, **kw):
    """Os sacos em camadas: cada camada assenta no casco e nas camadas de baixo (o apoio é refeito a cada camada).
    layers = [[(q, along, up|None, seed), ...], ...] (q = perto do apoio; o raio acha a superfície)."""
    for layer in layers:
        sup = Support([(p, base_keys + ['Bag#sack'])])
        for q, along, up, seed in layer:
            u = Vector(up if up is not None else up_default).normalized()
            hit, _ = land(sup, q, u)
            sandbag(p, sup, hit, along, u, seed=seed, **kw)


# ---------------------------------------------------------------------------------- pintura colada (camuflagem, faixas, ferrugem)
def decal_blob(p, sup, m, c, n, r, aspect=1.0, rot=0.0, seed=0.0, lift=0.004, K=30, J=2):
    """A mancha da camuflagem pintada à mão: o contorno irregular (4 senos de fase sorteada) colado na superfície por raios, uma
    casca fina fechada (o topo a 'lift' da superfície)."""
    n = Vector(n).normalized()
    eu, ev, _ = vp.face_frame(n)
    a = math.radians(rot)
    eu, ev = eu * math.cos(a) + ev * math.sin(a), ev * math.cos(a) - eu * math.sin(a)
    ph = [h1(seed, i) * 6.283 for i in range(5)]

    def rho(t, k=None):
        v = r * (1 + 0.24 * math.sin(2 * t + ph[0]) + 0.13 * math.sin(3 * t + ph[1]) + 0.07 * math.sin(5 * t + ph[2])
                 + 0.04 * math.sin(9 * t + ph[3]))
        if k is not None:                     # (02/11) a borda de pincel seco: dentes e lascas, não o recorte limpo de adesivo
            v *= 1 + 0.06 * math.sin(13 * t + ph[4]) + 0.045 * math.sin(23 * t + 2 * ph[0]) + 0.09 * (h1(seed, k, 7.7) - 0.5)
        return v
    C = Vector(c)
    hits = []
    hc = sup.cast(C + n * 0.35, -n, 0.8)
    if hc is None:
        return False
    for j in range(1, J + 1):
        ring = []
        for k in range(K):
            t = 2 * math.pi * k / K
            q = C + (eu * (math.cos(t) * aspect) + ev * math.sin(t)) * (rho(t, k if j == J else None) * j / J)
            hh = sup.cast(q + n * 0.35, -n, 0.8)
            if hh is None:
                return False
            ring.append(hh)
        hits.append(ring)
    b = p.bm(m + '#decal', 0.0)
    top_c = b.verts.new(V(*(hc[0] + hc[1] * lift)))
    bot_c = b.verts.new(V(*(hc[0] - hc[1] * 0.003)))
    T = [[b.verts.new(V(*(h[0] + h[1] * lift))) for h in ring] for ring in hits]
    B = [[b.verts.new(V(*(h[0] - h[1] * 0.003))) for h in ring] for ring in hits]
    for k in range(K):
        kk = (k + 1) % K
        b.faces.new([top_c, T[0][k], T[0][kk]]).smooth = True
        b.faces.new([bot_c, B[0][kk], B[0][k]])
        for j in range(J - 1):
            b.faces.new([T[j][k], T[j + 1][k], T[j + 1][kk], T[j][kk]]).smooth = True
            b.faces.new([B[j][kk], B[j + 1][kk], B[j + 1][k], B[j][k]])
        b.faces.new([T[-1][k], B[-1][k], B[-1][kk], T[-1][kk]])
    return True


def decal_stripe(p, sup, m, path, w, land_dir=(0, 0, -1), lift=0.006, seed=0.0, step=0.04, taper=0.07, wob=0.09, point=False):
    """A faixa pintada a pincel (a cor do jogador, o escorrido de ferrugem): o caminho (pontos de FORA da peça) pousa na superfície
    por raios em 'land_dir'; a largura treme dos dois lados (2 frequências), as pontas afinam (point: a de baixo vira ponta, o
    escorrido); cada seção é projetada pela normal de onde pousou. Casca fina fechada."""
    D = Vector(land_dir).normalized()
    P = [Vector(q) for q in path]
    segl = [(b_ - a_).length for a_, b_ in zip(P[:-1], P[1:])]
    total = sum(segl)
    n = max(2, int(total / step) + 1)
    samples = []
    for i in range(n):
        t = total * i / (n - 1)
        k = 0
        while k < len(segl) - 1 and t > segl[k]:
            t -= segl[k]
            k += 1
        samples.append(P[k] + (P[k + 1] - P[k]) * min(1.0, t / max(segl[k], 1e-9)))
    hits = [sup.cast(q, D, 8.0) for q in samples]
    ph = [h1(seed, i) * 6.283 for i in range(4)]
    secs = []
    for i, hh in enumerate(hits):
        if hh is None:
            secs.append(None)
            continue
        q, nrm, _ = hh
        j0, j1 = max(i - 1, 0), min(i + 1, n - 1)
        if hits[j0] is not None and hits[j1] is not None and j1 != j0:
            tan = hits[j1][0] - hits[j0][0]
        else:
            tan = samples[j1] - samples[j0]
        tan = tan - nrm * tan.dot(nrm)
        if tan.length < 1e-6:
            secs.append(None)
            continue
        tan.normalize()
        sd = nrm.cross(tan).normalized()
        u = i / (n - 1)
        e = min(1.0, u / taper, (1 - u) / taper) if not point else min(1.0, u / taper) * (1.0 - u) ** 0.7
        dist = total * u
        k0 = (0.45 + 0.55 * max(e, 0.0) ** 0.6) if not point else max(e, 0.05)
        wl = w / 2 * k0 * (1 + wob * math.sin(dist / 0.23 + ph[0]) + 0.5 * wob * math.sin(dist / 0.07 + ph[1]))
        wr = w / 2 * k0 * (1 + wob * math.sin(dist / 0.19 + ph[2]) + 0.5 * wob * math.sin(dist / 0.061 + ph[3]))
        row = []
        for off in (-wl, 0.0, wr):
            c0 = q + sd * off
            hc = sup.cast(c0 + nrm * 0.25, -nrm, 0.6)
            if hc is None:
                row = None
                break
            row.append((hc[0] + hc[1] * lift, hc[0] - hc[1] * 0.003))
        secs.append(row)
    b = p.bm(m + '#decal', 0.0)
    prev = None
    for row in secs + [None]:
        if row is None:
            if prev is not None and prev[2] > 0:                 # a tampa do fim
                pt, pb, _ = prev
                b.faces.new([pt[2], pt[1], pt[0], pb[0], pb[1], pb[2]])
            prev = None
            continue
        top = [b.verts.new(V(*t_)) for t_, _ in row]
        bot = [b.verts.new(V(*b_)) for _, b_ in row]
        if prev is None:
            b.faces.new([top[0], top[1], top[2], bot[2], bot[1], bot[0]])   # a tampa do começo
            prev = (top, bot, 0)
            continue
        pt, pb, cnt = prev
        for k in range(2):
            b.faces.new([pt[k], pt[k + 1], top[k + 1], top[k]]).smooth = True
            b.faces.new([pb[k + 1], pb[k], bot[k], bot[k + 1]])
        b.faces.new([pt[0], top[0], bot[0], pb[0]])
        b.faces.new([top[2], pt[2], pb[2], bot[2]])
        prev = (top, bot, cnt + 1)


# ---------------------------------------------------------------------------------- chapas
def bent_sheet(p, m, path_xz, ya, yb, t=0.022, R=0.15):
    """A chapa dobrada (o para-lama que desce sobre a roda-guia): a seção retangular varrida pelo caminho (x, z), a dobra redonda."""
    pts = vp.bent_bar([Vector((x, 0.0, z)) for x, z in path_xz], R=R, n=5)
    n = len(pts)
    rings = []
    for i, q in enumerate(pts):
        tan = (pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]).normalized()
        nr = Vector((-tan.z, 0.0, tan.x))
        if nr.z < 0:
            nr = -nr
        rings.append([tuple(q + Vector((0.0, y_, 0.0)) + nr * w_) for y_, w_ in ((ya, 0.0), (yb, 0.0), (yb, t), (ya, t))])
    p.loft(m, rings)


def side_plate(p, m, x0, x1, ztop, zbot, y, s, t=0.025, bow=0.012, n=10):
    """A chapa de remendo pendurada no lado (a saia de sucata): a borda de cima e a de baixo por função de x (o corte de maçarico,
    o canto cortado), empenada pra fora no meio (bow)."""
    rings = []
    for i in range(n + 1):
        u = i / n
        x = lerp(x0, x1, u)
        zt, zb = ztop(x), zbot(x)
        yo = y + s * bow * math.sin(math.pi * u)
        rings.append([(x, yo, zb), (x, yo + s * t, zb), (x, yo + s * t, zt), (x, yo, zt)])
    p.loft(m, rings)


def dome_plate(p, sup, m, az, el0, el1, daz, t=0.024, seed=0.0, nu=6, nv=5, bolts=True, weld=True):
    """A chapa de remendo soldada na cúpula: a grade segue a curva (a chapa dobrada no martelo), a borda de maçarico irregular, os
    parafusos nos cantos e o cordão de solda na borda de cima. Os ângulos são vistos do centro DOME_C (az, elevação em graus)."""
    T, B = [], []
    for j in range(nv + 1):
        rt, rb = [], []
        for i in range(nu + 1):
            u, v = i / nu, j / nv
            ju = (h1(seed, j, 1.0) - 0.5) * 0.08 if i in (0, nu) else 0.0
            jv = (h1(seed, i, 2.0) - 0.5) * 0.1 if j in (0, nv) else 0.0
            a = math.radians(az + daz * (u - 0.5 + ju))
            e = math.radians(lerp(el0, el1, v + jv))
            d = Vector((math.cos(e) * math.cos(a), math.cos(e) * math.sin(a), math.sin(e)))
            hh = sup.cast(DOME_C + d * 4.0, -d, 4.5)
            if hh is None:
                return None
            rt.append(hh[0] + hh[1] * (0.003 + t))
            rb.append(hh[0] + hh[1] * 0.003)
        T.append(rt)
        B.append(rb)
    b = p.bm(m + '#dplate', 0.012)
    VT = [[b.verts.new(V(*q)) for q in r] for r in T]
    VB = [[b.verts.new(V(*q)) for q in r] for r in B]
    for j in range(nv):
        for i in range(nu):
            b.faces.new([VT[j][i], VT[j][i + 1], VT[j + 1][i + 1], VT[j + 1][i]])
            b.faces.new([VB[j + 1][i], VB[j + 1][i + 1], VB[j][i + 1], VB[j][i]])
    edge = [(0, i) for i in range(nu)] + [(j, nu) for j in range(nv)] + [(nv, i) for i in range(nu, 0, -1)] + [(j, 0) for j in range(nv, 0, -1)]
    for (j0, i0), (j1, i1) in zip(edge, edge[1:] + edge[:1]):
        b.faces.new([VT[j0][i0], VB[j0][i0], VB[j1][i1], VT[j1][i1]])
    nrm = lambda j, i: (T[j][i] - B[j][i]).normalized()
    if bolts:
        for j, i in ((1, 1), (1, nu - 1), (nv - 1, 1), (nv - 1, nu - 1)):
            vp.nut(p, 'Metal', T[j][i], nrm(j, i), r=0.022, h=0.016)
    if weld:
        vp.weld_bead(p, 'Metal', [T[nv][i] + nrm(nv, i) * -0.006 for i in range(nu + 1)], r=0.012)
    return T


# ---------------------------------------------------------------------------------- peças do casco
def grille(p, c, size, n, along=(0, 1, 0), tilt=32.0):
    """A grelha do motor (a do T-55): o fundo escuro, a moldura de quina redonda e as aletas inclinadas (o vão escuro entre elas)."""
    eu, ev, en = vp.face_frame((0, 0, 1), along)
    C = Vector(c)
    L, W = size
    vp.prism(p, 'Dark#vp', vp.rounded_rect(L + 0.02, W + 0.02, 0.03, 2), C - en * 0.004, eu, ev, en, 0.012)
    fw = 0.05
    sec = vp.rounded_rect(fw, 0.045, 0.014, 1)
    for sgn in (-1, 1):
        o_ = C + ev * (sgn * (W / 2 + fw / 2)) + en * 0.0225
        vp.prism(p, 'Paint', sec, o_ - eu * (L / 2 + fw), ev, en, eu, L + 2 * fw)
        o_ = C + eu * (sgn * (L / 2 + fw / 2)) + en * 0.0225
        vp.prism(p, 'Paint', sec, o_ - ev * (W / 2), eu, en, ev, W)
    step = W / n
    a = math.radians(tilt)
    e2 = ev * math.cos(a) + en * math.sin(a)
    e3 = en * math.cos(a) - ev * math.sin(a)
    for k in range(n):
        q = C + ev * (-W / 2 + step * (k + 0.5)) + en * 0.022
        vp.prism(p, 'Paint', vp.rounded_rect(step * 0.8, 0.014, 0.006, 1), q - eu * (L / 2), e2, e3, eu, L)


def stow_box(p, c, size, yaw=0.0, m='Paint#plate', side=1):
    """A caixa de ferramentas do para-lama: o corpo de quinas redondas, a fresta da tampa, a dobradiça atrás e os 2 fechos na frente."""
    L, W, H = size
    x, y, z = c
    vp.rbox_q(p, m, (x, y, z + H / 2), size, yaw=yaw, r=0.05, k=3)
    ex, ey, ez = vp.ypr_frame(yaw)
    C = Vector((x, y, z))
    yo = ey * (side * (W / 2 + 0.002))
    vp.panel_seam(p, 'Dark', C + yo - ex * (L / 2 - 0.04) + ez * (H - 0.06), C + yo + ex * (L / 2 - 0.04) + ez * (H - 0.06), ey * side,
                  w=0.018, d=0.008)
    for sg in (-1, 1):
        vp.rbox(p, 'Metal', C + yo + ey * (side * 0.01) + ex * (sg * L * 0.28) + ez * (H - 0.09), (0.06, 0.02, 0.07), yaw=yaw, r=0.01, k=1)
    vp.hinge(p, 'Metal', C - ey * (side * (W / 2 - 0.02)) + ez * (H + 0.01), ex, L * 0.7, r=0.016, knuckles=3)


def crate(p, c, size, yaw=0.0, key='Wood#crate'):
    """O caixote de tábua: o corpo, as frestas das tábuas nos lados e no tampo, os sarrafos nos cantos e as cintas de cima e de baixo."""
    L, W, H = size
    x, y, z = c
    vp.rbox_q(p, key, (x, y, z + H / 2), size, yaw=yaw, r=0.02, k=2)
    ex, ey, ez = vp.ypr_frame(yaw)
    C = Vector((x, y, z))
    for sg in (-1, 1):
        for k in (1, 2):
            zz = H * k / 3
            vp.panel_seam(p, 'Dark', C + ey * (sg * (W / 2 + 0.001)) - ex * (L / 2 - 0.02) + ez * zz,
                          C + ey * (sg * (W / 2 + 0.001)) + ex * (L / 2 - 0.02) + ez * zz, ey * sg, w=0.012, d=0.006)
    for sx in (-1, 1):
        for sy in (-1, 1):
            q = C + ex * (sx * (L / 2 - 0.015)) + ey * (sy * (W / 2 - 0.015)) + ez * (H / 2)
            vp.rbox(p, 'Wood', q, (0.05, 0.05, H + 0.01), yaw=yaw, r=0.012, k=1)
    for zz in (H - 0.04,):
        for sy in (-1, 1):
            vp.rbox(p, 'Wood', C + ey * (sy * (W / 2 + 0.006)) + ez * zz, (L - 0.02, 0.018, 0.055), yaw=yaw, r=0.008, k=1)


def fuel_tank(p, c, size):
    """O tanque externo de combustível do T-55 (no para-lama direito): a caixa de quinas bem redondas, o círculo com o X estampado
    no tampo, a tampa do bocal e as 2 cintas que prendem."""
    L, W, H = size
    x, y, z = c
    vp.rbox_q(p, 'Paint#plate', (x, y, z + H / 2), size, r=0.09, k=3)
    top = z + H + 0.001
    vp.lathe(p, 'Paint', (x, y, top), 'z', [(0.09, 0.0), (0.12, 0.0), (0.12, 0.01), (0.09, 0.01)], seg=20, closed=True)
    for sg in (1, -1):
        vp.rbox(p, 'Paint', (x, y, top + 0.005), (math.hypot(L, W) * 0.82, 0.022, 0.01), yaw=sg * math.degrees(math.atan2(W, L)),
                r=0.008, k=1)
    # (a tampa do bocal na borda de fora: na de dentro o saco caído da torre raspava nela girando)
    p.cyl('Metal#vp', (x + L * 0.32, y + W * 0.25, top + 0.03), 'z', 0.045, 0.06, seg=12, bevel=0.0)
    for dx in (-L * 0.3, L * 0.3):
        vp.tube(p, 'Dark', vp.bent_bar([(x + dx, y - W / 2 - 0.01, z), (x + dx, y - W / 2 - 0.012, top + 0.01),
                                        (x + dx, y + W / 2 + 0.012, top + 0.01), (x + dx, y + W / 2 + 0.01, z)], R=0.06, n=3), 0.012, seg=6)


def headlight(p, c, r=0.085):
    vp.lathe(p, 'Metal', c, 'x', [(0.0, -0.08), (r * 0.7, -0.08), (r + 0.02, -0.05), (r + 0.024, 0.02), (r + 0.024, 0.05),
                                  (r + 0.012, 0.06), (r, 0.056), (r, 0.04), (0.0, 0.04)], seg=16)
    vp.lathe(p, 'Lens', c, 'x', [(0.0, 0.062), (r * 0.6, 0.06), (r * 0.95, 0.052), (r, 0.045), (0.0, 0.045)], seg=16)


def tow_hook(p, x, y, z, s=1):
    for d in (-1, 1):
        vp.rbox(p, 'Metal', (x + 0.05 * s, y + d * 0.06, z), (0.14, 0.04, 0.17), r=0.03, k=2)
    p.cyl('Metal#vp', (x + 0.08 * s, y, z + 0.03), 'y', 0.024, 0.18, seg=10, bevel=0.0)
    p.torus('Dark#vp', (x + 0.11 * s, y, z - 0.04), 'y', 0.06, 0.018, seg=14, rseg=6)


def lug(p, c, axis='x'):
    """A argola de amarrar (onde a corda prende)."""
    p.torus('Metal#vp', c, axis, 0.032, 0.009, seg=10, rseg=5)


def rope_over(p, sup, pts_xy, m='Wood', r=0.011, lift=0.011, n=8, z0=4.0):
    """A corda passada por cima da carga: os pontos (x, y) [ou (x, y, z) fixos nas pontas] pousam na carga por raios verticais."""
    out = []
    for k, q in enumerate(pts_xy):
        if len(q) == 3:
            out.append(Vector(q))
            continue
        hit = sup.cast((q[0], q[1], z0), (0, 0, -1), 6.0)
        out.append(hit[0] + hit[1] * (r + lift * 0.3) if hit else Vector((q[0], q[1], DECK)))
    dense = []
    for a, b in zip(out[:-1], out[1:]):
        for i in range(n):
            t = i / n
            q = a + (b - a) * t
            if 0 < i:
                hit = sup.cast((q.x, q.y, z0), (0, 0, -1), 6.0)
                if hit and hit[0].z + r > q.z:
                    q = hit[0] + hit[1] * (r + 0.004)
            dense.append(q)
    dense.append(out[-1])
    vp.tube(p, m, dense, r, seg=6)


# ---------------------------------------------------------------------------------- casco
def hull():
    p = Part(PREFIX + '_Hull')
    for k, bv in (('Paint#shell', 0.06), ('Paint#plate', 0.025), ('Paint#thin', 0.01), ('Patch#plate', 0.012),
                  ('Rust#plate', 0.012), ('Rust#gplate', 0.012), ('Patch#gplate', 0.012), ('Bag#sack', 0.0), ('Wood#crate', 0.02),
                  ('Lens', 0.0)):
        p.bm(k, bv)
    # ---------------------------------------------------------------- o casco: o de baixo (entre as esteiras) e o de cima (inteiro)
    low = [(2.95, BELLY), (NOSE, 1.0), (2.3, DECK), (-3.0, DECK), (REAR, 1.45), (REAR, 0.6), (-3.0, BELLY)]
    solid(p, 'Paint#shell', [(x, s * LOW, z) for x, z in low for s in (-1, 1)])
    spon = [(glacis_x(FENDER), FENDER), (2.3, DECK), (-3.0, DECK), (REAR, 1.45), (REAR, FENDER)]
    for s in (-1, 1):                     # (a junta do sobre-esteira com o casco: a costura que corre o deck)
        solid(p, 'Paint#shell', [(x, yy * s, z) for x, z in spon for yy in (LOW - 0.02, UP)])
    # os para-lamas de chapa dobrada (frente: desce sobre a roda-guia; trás) e o para-barro de borracha atrás
    for s in (-1, 1):
        ya, yb = sorted(((LOW + 0.03) * s, UP * s))
        bent_sheet(p, 'Paint#thin', [(2.98, FENDER + 0.002), (3.42, FENDER + 0.002), (3.63, 0.94)], ya, yb, t=0.022)
        bent_sheet(p, 'Paint#thin', [(-3.0, FENDER + 0.002), (-3.5, FENDER + 0.002), (-3.63, 0.98)], ya, yb, t=0.022)
        vp.mudflap(p, (-3.645, (LOW + UP) / 2 * s, 0.975), UP - LOW - 0.08, 0.3, yaw=0, m='Rubber')
    # o anel da torre (a pista escura no deck)
    vp.lathe(p, 'Dark', (TX, 0.0, DECK), 'z', [(1.2, 0.0), (1.36, 0.0), (1.36, 0.008), (1.34, 0.012), (1.2, 0.012)], seg=64,
             closed=True)
    # ---------------------------------------------------------------- o nariz: ganchos, faróis com a guarda, a escotilha do motorista
    for s in (-1, 1):
        tow_hook(p, NOSE - 0.12, 0.72 * s, 0.72, 1)
        tow_hook(p, REAR - 0.02, 0.88 * s, 0.82, -1)
    for y, r in ((1.2, 0.09), (0.94, 0.07)):
        z = 1.3
        headlight(p, (glacis_x(z) + 0.07, y, z), r)
    hx = glacis_x(1.3) + 0.15
    vp.tube(p, 'Metal', vp.bent_bar([(glacis_x(1.18), 0.8, 1.18), (hx, 0.8, 1.18), (hx, 1.35, 1.18), (glacis_x(1.18), 1.35, 1.18)],
                                    R=0.05, n=3), 0.014, seg=6)
    vp.tube(p, 'Metal', vp.bent_bar([(glacis_x(1.42), 0.8, 1.42), (hx, 0.8, 1.42), (hx, 1.35, 1.42), (glacis_x(1.42), 1.35, 1.42)],
                                    R=0.05, n=3), 0.014, seg=6)
    vp.hatch_round(p, (2.02, -0.62, DECK), 0.29, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
    for k, yy in enumerate((-0.78, -0.48)):
        vp.rbox(p, 'Paint#plate', (2.36, yy, DECK + 0.05), (0.16, 0.2, 0.1), r=0.035, k=2)
        p.obox('Lens', (2.445, yy, DECK + 0.06), (0.012, 0.15, 0.045), bevel=0.0)
    # as juntas do glacis e do deck (a solda das chapas)
    vp.weld_bead(p, 'Paint#vp', [(2.31, -1.55, DECK + 0.004), (2.31, 1.55, DECK + 0.004)], r=0.014)
    vp.weld_bead(p, 'Paint#vp', [(NOSE - 0.02, -0.95, 1.0), (NOSE - 0.02, 0.95, 1.0)], r=0.016)
    # ---------------------------------------------------------------- o deck do motor: as 2 grelhas, a veneziana de trás, as tampas
    for s in (-1, 1):
        grille(p, (-2.3, 0.44 * s, DECK), (0.58, 0.9), 8, along=(0, 1, 0), tilt=32)
        vp.lathe(p, 'Paint', (-1.75, 1.0 * s, DECK), 'z', [(0.0, 0.0), (0.12, 0.0), (0.12, 0.035), (0.1, 0.055), (0.0, 0.06)], seg=18)
    grille(p, (-2.93, 0.0, DECK), (2.1, 0.16), 3, along=(0, 1, 0), tilt=35)
    for x0 in (-1.55, -2.84):
        vp.panel_seam(p, 'Dark', (x0, -1.2, DECK + 0.003), (x0, 1.2, DECK + 0.003), (0, 0, 1), w=0.022, d=0.01)
    for s in (-1, 1):
        vp.panel_seam(p, 'Dark', (-1.55, 1.2 * s, DECK + 0.003), (-2.84, 1.2 * s, DECK + 0.003), (0, 0, 1), w=0.022, d=0.01)
        vp.bolt_row(p, 'Metal', (2.2, 1.58 * s, DECK), (-2.9, 1.58 * s, DECK), 14, (0, 0, 1), r=0.018, h=0.01)
    # ---------------------------------------------------------------- as saias de sucata da frente (teal e ferrugem) e a gaiola atrás
    for s in (-1, 1):
        yb = (UP + 0.002) * s
        if s > 0:
            plates = [('Rust#plate', 0.1, 1.5, lambda x: 1.47 - 0.03 * (1.5 - x) / 1.4, lambda x: 0.72 + 0.04 * math.sin(x * 3.1), 0.0),
                      ('Patch#plate', 1.42, 2.76, lambda x: 1.48, lambda x: 0.7 + 0.3 * smooth01((x - 2.35) / 0.41), 0.026),
                      ('Patch#plate', 0.5, 0.92, lambda x: 1.3, lambda x: 0.96, 0.03)]
        else:
            plates = [('Patch#plate', 0.1, 1.15, lambda x: 1.46, lambda x: 0.74 + 0.03 * math.sin(x * 4.0), 0.0),
                      ('Rust#plate', 1.08, 1.9, lambda x: 1.45 - 0.02 * (x - 1.08), lambda x: 0.71, 0.026),
                      ('Patch#plate', 1.84, 2.76, lambda x: 1.48, lambda x: 0.72 + 0.28 * smooth01((x - 2.35) / 0.41), 0.0),
                      ('Rust#plate', 0.35, 0.78, lambda x: 1.12, lambda x: 0.8, 0.03)]
        for m, x0, x1, zt, zb, off in plates:
            side_plate(p, m, x0, x1, zt, zb, yb + s * off, s, t=0.022, bow=0.012)
            yo = (UP + 0.026 + off + 0.002) * s
            vp.bolt_row(p, 'Metal', (x0 + 0.08, yo, zt(x0 + 0.08) - 0.06), (x1 - 0.08, yo, zt(x1 - 0.08) - 0.06),
                        max(3, int((x1 - x0) / 0.26)), (0, s, 0), r=0.02, h=0.014)
            vp.weld_bead(p, 'Metal', [(x0 + 0.012, yo - 0.012 * s, zb(x0) + 0.04), (x0 + 0.012, yo - 0.012 * s, zt(x0) - 0.03)], r=0.011)
        # os suportes da saia (do para-lama pra fora)
        for x in (2.5, 1.7, 0.95, 0.25):
            vp.tube(p, 'Metal', [(x, UP * 0.98 * s, FENDER + 0.02), (x, (UP + 0.03) * s, FENDER + 0.02)], 0.02, seg=6)
        # a gaiola de barras chatas sobre as rodas de trás: 7 barras (5 cm, a borda redonda), os montantes, o corrimão e os braços
        yc = (UP + 0.1) * s
        x0, x1 = -2.8, 0.06
        for k in range(7):
            z = 0.74 + k * 0.092
            vp.prism(p, 'Metal', [(0.025, 0.007), (-0.025, 0.007), (-0.025, -0.007), (0.025, -0.007)],
                     Vector((x1, yc, z)), ZUP, Vector((0, 1, 0)), Vector((-1, 0, 0)), x1 - x0)
        vp.tube(p, 'Metal', [(x1 + 0.06, yc, 1.37), (x0 - 0.06, yc, 1.37)], 0.022, seg=8)
        for k in range(7):
            x = lerp(x0 + 0.02, x1 - 0.02, k / 6)
            vp.tube(p, 'Metal', [(x, yc - 0.012 * s, 0.69), (x, yc - 0.012 * s, 1.38)], 0.019, seg=6)
            if k % 2 == 0:
                vp.tube(p, 'Metal', [(x, UP * 0.98 * s, 1.37), (x, yc, 1.37)], 0.02, seg=6)
                vp.tube(p, 'Metal', [(x, UP * 0.98 * s, FENDER + 0.03), (x, yc - 0.01 * s, 0.86)], 0.017, seg=6)
        # o costado atrás da gaiola (do para-lama ao deck): remendos e o cordão de solda
        for (xa, xb, za, zb_, m) in (((-2.7, -1.9, 1.16, 1.48, 'Rust#plate'), (-1.55, -0.85, 1.14, 1.42, 'Patch#plate'),
                                      (-0.6, 0.0, 1.18, 1.47, 'Rust#plate')) if s > 0 else
                                     ((-2.6, -1.75, 1.15, 1.46, 'Patch#plate'), (-1.4, -0.55, 1.17, 1.48, 'Rust#plate'))):
            side_plate(p, m, xa, xb, lambda x, zb_=zb_: zb_, lambda x, za=za: za, (UP + 0.002) * s, s, t=0.018, bow=0.006, n=6)
    # ---------------------------------------------------------------- o remendo do glacis (esquerda): a chapa de ferrugem e a teal
    eu, ev, en = Vector((0, 1, 0)), GUP, GN
    gc = Vector((glacis_x(1.3), -0.9, 1.3))
    vp.rbox_q(p, 'Rust#gplate', gc + en * 0.015, (1.1, 0.8, 0.024), r=0.05, k=3, frame=(eu, ev, en))
    gc2 = Vector((glacis_x(1.46), -1.25, 1.46))
    vp.rbox_q(p, 'Patch#gplate', gc2 + en * 0.04, (0.5, 0.36, 0.022), r=0.04, k=3, frame=(eu.copy(), (ev * math.cos(0.12) + eu * math.sin(0.12)), en))
    for su in (-1, 1):
        for sv in (-1, 1):
            vp.nut(p, 'Metal', gc + en * 0.027 + eu * (su * 0.49) + ev * (sv * 0.34), en, r=0.022, h=0.016)
    vp.weld_bead(p, 'Metal', [gc + en * 0.008 + ev * 0.4 + eu * u_ for u_ in (-0.54, 0.54)], r=0.012)
    # ---------------------------------------------------------------- a carga dos para-lamas
    # direita: a caixa da frente, as ferramentas baixas perto da torre, os 3 tanques externos de combustível atrás
    stow_box(p, (1.86, 1.42, DECK), (0.62, 0.36, 0.24), side=1)
    vp.shovel(p, (0.45, 1.5, DECK + 0.03), yaw=180, handle_m='Wood', blade_m='Metal', length=1.1)
    vp.crowbar(p, (0.3, 1.36, DECK + 0.02), yaw=0, m='Metal', length=1.3)
    for x in (-0.95, -1.72, -2.49):
        fuel_tank(p, (x, 1.41, DECK), (0.7, 0.4, 0.3))
    vp.tube(p, 'Metal', [(-0.6, 1.62, DECK + 0.08), (-2.84, 1.62, DECK + 0.08)], 0.018, seg=6)
    # esquerda: a caixa da frente, o caixote, a caixa comprida baixa, os 2 caixotes de trás com a lona dobrada
    stow_box(p, (1.98, -1.42, DECK), (0.6, 0.36, 0.24), side=-1)
    crate(p, (1.4, -1.46, DECK), (0.42, 0.32, 0.24), yaw=4)
    vp.rbox_q(p, 'Paint#plate', (0.18, -1.54, DECK + 0.055), (1.1, 0.19, 0.11), r=0.04, k=2)
    vp.tow_cable(p, 'Metal', [(0.85, -1.37, DECK + 0.024), (-0.4, -1.37, DECK + 0.024)], r=0.02)
    crate(p, (-0.95, -1.42, DECK), (0.52, 0.4, 0.26), yaw=-3)
    crate(p, (-1.55, -1.38, DECK), (0.56, 0.44, 0.25), yaw=6)
    vp.tarp_bundle(p, (-1.25, -1.4, DECK + 0.255), (1.0, 0.42, 0.1), m='Bag', strap_m='Wood', buckle_m='Metal', straps=2, layers=2,
                   seed=3.0, n=14, k=16)
    # a lona enrolada atravessada atrás da torre, com as voltas de corda
    vp.tarp_roll(p, (-1.66, 0.0, DECK + 0.12), 'y', 1.25, 0.12, m='Bag', straps=3, strap_m='Wood', rope_ties=True)
    # os galões nos para-lamas de trás (em pé, na grade de barra), presos pela cinta
    # (a prateleira do para-lama que sobra atrás da chapa de trás: x −3,2 a −3,5)
    for s in (1,):                        # (02/11) só na direita: na esquerda eles tapavam o pneu de lado
        for k, yy in enumerate((1.18, 1.5)):
            vp.jerrycan(p, (-3.33, yy * s, FENDER + 0.024 + 0.235), yaw=90 + (k * 2 - 1) * 4, m='Patch', cap_m='Dark')
        vp.tube(p, 'Dark', vp.bent_bar([(-3.19, 0.97 * s, FENDER + 0.32), (-3.45, 0.97 * s, FENDER + 0.32), (-3.45, 1.7 * s, FENDER + 0.32),
                                        (-3.19, 1.7 * s, FENDER + 0.32)], R=0.05, n=3), 0.012, seg=6)
        vp.tube(p, 'Metal', vp.bent_bar([(-3.19, 0.96 * s, FENDER + 0.04), (-3.47, 0.96 * s, FENDER + 0.04), (-3.47, 1.71 * s, FENDER + 0.04),
                                         (-3.19, 1.71 * s, FENDER + 0.04)], R=0.04, n=3), 0.016, seg=6)
    # ---------------------------------------------------------------- o pneu sobressalente NA DIAGONAL atrás
    # (02/11, a volta do pneu: o da ficha, ~1,3 x 0,45 m; na ficha de lado ele ENCOSTA o alto na quina de trás do deck e o pé sai pra
    # trás e pra baixo: a elipse tombada ~31° da vertical; o alto fica 6 cm abaixo do cano girado pra trás)
    a = math.radians(33.0)
    A = Vector((-math.cos(a), 0.0, math.sin(a)))          # a face (o aro) olhando pra trás e pra cima
    Uv = Vector((math.sin(a), 0.0, math.cos(a)))          # o "pra cima" no plano do pneu: o alto tomba pra frente, sobre o deck
    TR, TW = 0.65, 0.45
    TC = Vector((-3.58, 0.05, 1.30))
    vp.tyre(p, TC, TR, TW, 1, rim_r=0.33, rubber='Rubber', rim='Paint', hub='Metal', nut_m='Metal', seg=36, lugs=True, lug_h=0.026,
            nuts=8, holes_m='Dark', axis=A, style='mud')
    # o suporte soldado na traseira: os 2 braços que descem por baixo do pé do pneu (paralelos à banda) com a aba atrás, e o batente
    # no deck onde o alto encosta
    for yy in (-0.36, 0.44):
        vp.tube(p, 'Metal', vp.bent_bar([(REAR + 0.02, yy, 0.6), (-3.72, yy, 0.56), (-4.15, yy, 0.84), (-4.2, yy, 0.97)], R=0.05, n=3),
                0.024, seg=8)
        vp.rbox(p, 'Metal', (REAR - 0.01, yy, 0.62), (0.04, 0.1, 0.14), r=0.015, k=1)
    vp.tube(p, 'Metal', vp.bent_bar([(-2.95, -0.42, DECK + 0.01), (-3.06, -0.42, DECK + 0.07), (-3.06, 0.5, DECK + 0.07),
                                     (-2.95, 0.5, DECK + 0.01)], R=0.05, n=3), 0.022, seg=8)
    # a corda em X por cima da face e as voltas que descem pros ganchos
    hw = TW / 2
    E2 = Vector((0.0, 1.0, 0.0))
    rp = []
    for ang in (45.0, 135.0):
        t0, t1 = math.radians(ang), math.radians(ang + 180.0)
        p0 = TC + (Uv * math.cos(t0) + E2 * math.sin(t0)) * (TR + 0.004) + A * (hw * 0.6)
        p1 = TC + (Uv * math.cos(t1) + E2 * math.sin(t1)) * (TR + 0.004) + A * (hw * 0.6)
        mid = TC + A * (hw + 0.07)
        pts = [p0 - A * (hw * 1.2), p0 + A * (hw * 0.5), p0 + A * (hw + 0.035) - (p0 - TC).normalized() * 0.05]
        pts += [mid + (p0 - mid) * 0.35, mid, mid + (p1 - mid) * 0.35]
        pts += [p1 + A * (hw + 0.035) - (p1 - TC).normalized() * 0.05, p1 + A * (hw * 0.5), p1 - A * (hw * 1.2)]
        vp.tube(p, 'Wood', vp.bent_bar(pts, R=0.04, n=2), 0.014, seg=6)
        rp.append((p0 - A * (hw * 1.2), p1 - A * (hw * 1.2)))
    # as pontas da corda presas: as de baixo nos braços do berço, as de cima no batente do deck
    for (pa, pb) in rp:
        for q in (pa, pb):
            tgt = Vector((-3.06, max(-0.4, min(0.48, q.y)), DECK + 0.07)) if q.z > TC.z else Vector((REAR - 0.03, max(-0.34, min(0.42, q.y)), 0.66))
            vp.tube(p, 'Wood', [q, q + (tgt - q) * 0.5 + Vector((0, 0, -0.03)), tgt], 0.013, seg=6)
    # ---------------------------------------------------------------- os SACOS DE AREIA (assentam no casco e uns nos outros)
    base_keys = ['Paint#shell', 'Paint#thin', 'Paint#plate', 'Rust#gplate', 'Patch#gplate', 'Wood#crate']
    G = lambda x, y: Vector((x, y, glacis_z(x)))
    ay = Vector((0, 1, 0))
    ax_ = Vector((1, 0, 0))
    gz = (GN + ZUP).normalized()
    layers = [
        # a frente-direita: o monte no canto (glacis e para-lama), deitado no glacis
        [(G(3.04, 1.3), ay, GN, 1.0), (G(2.6, 1.32), ay, GN, 2.0), (G(2.78, 0.72), ay, GN, 3.0), (Vector((3.3, 1.32, 1.14)), ay, ZUP, 4.0),
         (G(3.12, 0.42), ay, GN, 5.0),
         # a frente-esquerda: dois no glacis por cima do remendo e um no para-lama
         (G(3.0, -1.3), ay, GN, 6.0), (G(2.58, -1.36), ay, GN, 7.0), (Vector((3.3, -1.32, 1.14)), ay, ZUP, 8.0),
         # o canto de trás-esquerda: no deck, um passando da borda (cai)
         (Vector((-2.74, -1.47, DECK)), ax_, ZUP, 9.0), (Vector((-2.16, -1.47, DECK)), ax_, ZUP, 10.0),
         (Vector((-2.45, -1.07, DECK)), ax_, ZUP, 11.0), (Vector((-3.06, -1.22, DECK)), ay, ZUP, 12.0)],
        [(G(2.86, 1.08), (0.25, 1, 0), gz, 13.0), (G(2.5, 1.2), ay, GN, 14.0), (G(2.86, 0.5), ay, GN, 15.0),
         (G(2.82, -1.2), ay, gz, 16.0),
         (Vector((-2.44, -1.33, DECK + 0.2)), ax_, ZUP, 17.0), (Vector((-1.92, -1.2, DECK + 0.2)), (1, 0.15, 0), ZUP, 18.0)],
        [(G(2.7, 1.2), (1, 0.4, 0), ZUP, 19.0)],
    ]
    bag_pile(p, base_keys, layers)
    # as cordas por cima do monte de trás e dos caixotes (presas nas argolas da borda)
    sup = Support([(p, base_keys + ['Bag#sack', 'Bag#cloth'])])
    for xx in (-2.3, -2.75):
        lug(p, (xx, -1.63, DECK - 0.03), 'x')
        lug(p, (xx, -0.82, DECK + 0.01), 'x')
        rope_over(p, sup, [(xx, -1.66, DECK - 0.03), (xx, -1.5), (xx, -1.2), (xx, -0.95), (xx, -0.82, DECK + 0.03)])
    for xx in (-0.95, -1.55):
        rope_over(p, sup, [(xx, -1.64, DECK - 0.04), (xx, -1.55), (xx, -1.4), (xx, -1.25), (xx, -1.17, DECK + 0.02)], m='Bag')
    rope_over(p, sup, [(1.4, -1.66, DECK - 0.04), (1.4, -1.56), (1.4, -1.38), (1.4, -1.25, DECK + 0.02)], m='Bag')
    rope_over(p, sup, [(2.95, 1.66, glacis_z(2.95) - 0.06), (2.95, 1.4), (2.85, 1.0), (2.8, 0.55), (2.8, 0.2, glacis_z(2.8) + 0.02)])
    rope_over(p, sup, [(2.6, 1.66, glacis_z(2.6) - 0.06), (2.6, 1.45), (2.66, 1.1), (2.72, 0.75), (2.76, 0.42, glacis_z(2.76) + 0.02)])
    rope_over(p, sup, [(2.92, -1.66, glacis_z(2.92) - 0.06), (2.92, -1.45), (2.86, -1.15), (2.8, -0.85, glacis_z(2.8) + 0.03)])
    rope_over(p, sup, [(2.58, -1.66, glacis_z(2.58) - 0.06), (2.58, -1.42), (2.62, -1.12), (2.66, -0.92, glacis_z(2.66) + 0.03)])
    for (xx, sg) in ((2.95, 1), (2.6, 1), (2.92, -1), (2.58, -1)):
        lug(p, (xx, 1.645 * sg, glacis_z(xx) - 0.06), 'x')
    # ---------------------------------------------------------------- a pintura: as faixas verdes (cor do jogador), as manchas, a ferrugem
    sup_paint = Support([(p, ['Paint#shell'])])
    # a faixa do meio: no deck, descendo o glacis até o nariz (a ficha de frente e de cima)
    decal_stripe(p, sup_paint, 'Team', [(1.42, 0.0, 3.0), (2.3, 0.0, 3.0), (3.22, 0.0, 3.0)], 0.2, seed=1.0)
    # as 2 faixas diagonais pintadas no remendo do glacis (a ficha de 3/4)
    sup_gp = Support([(p, ['Rust#gplate', 'Patch#gplate', 'Paint#shell'])])
    decal_stripe(p, sup_gp, 'Team', [(2.47, -1.34, 3.0), (3.02, -0.7, 3.0)], 0.13, seed=2.0, lift=0.006)
    decal_stripe(p, sup_gp, 'Team', [(2.47, -1.06, 3.0), (3.02, -0.42, 3.0)], 0.13, seed=3.0, lift=0.006)
    # a camuflagem pintada à mão: manchas teal e marrom no glacis, no deck e na traseira
    for (x, y, m, r, rot, asp) in ((2.75, 0.62, 'Patch', 0.22, 20, 1.4), (1.75, 0.55, 'Rust', 0.2, -30, 1.3), (1.55, -0.25, 'Patch', 0.18, 60, 1.2),
                                   (-1.15, 0.55, 'Rust', 0.2, 10, 1.5), (-0.6, -0.95, 'Patch', 0.2, -20, 1.3), (2.62, 0.05, 'Rust', 0.16, 45, 1.2)):
        hit, n = land(sup_paint, (x, y, DECK), ZUP)
        decal_blob(p, sup_paint, m, hit, n, r, aspect=asp, rot=rot, seed=x * 3 + y)
    for (y, z, m, r) in ((0.55, 1.22, 'Rust', 0.16), (-0.75, 0.95, 'Patch', 0.18), (-0.1, 0.78, 'Rust', 0.14)):
        decal_blob(p, sup_paint, m, (REAR, y, z), (-1, 0, 0), r, aspect=1.4, rot=15, seed=y * 7 + z)
    # o escorrido de ferrugem debaixo dos parafusos das saias, e o cáqui pintado por cima do teal (a ficha)
    sup_side = Support([(p, ['Patch#plate', 'Rust#plate'])])
    for (x, z, s, m, r, rot) in ((1.85, 1.22, 1, 'Paint', 0.17, 20), (2.5, 0.98, 1, 'Paint', 0.12, -30), (0.62, 1.12, -1, 'Paint', 0.16, 40),
                                 (2.25, 1.02, -1, 'Paint', 0.15, -10), (1.25, 0.95, 1, 'Patch', 0.12, 0), (1.5, 1.25, -1, 'Patch', 0.11, 15)):
        decal_blob(p, sup_side, m, (x, UP * s, z), (0, s, 0), r, aspect=1.5, rot=rot, seed=x * 5 + z * s, lift=0.004)
    for s in (-1, 1):
        for (x, z0, L) in ((2.4, 1.4, 0.38), (1.9, 1.39, 0.3), (1.05, 1.36, 0.42), (0.5, 1.37, 0.26)):
            decal_stripe(p, sup_side, 'Rust', [(x, 3.0 * s, z0), (x + 0.01, 3.0 * s, z0 - L)], 0.05, land_dir=(0, -s, 0), lift=0.004,
                         seed=x * s + z0, point=True, taper=0.12, step=0.05)
    return p.finish()


# ---------------------------------------------------------------------------------- torre
def dome_profile():
    """O perfil da cúpula fundida (Catmull-Rom entre os pontos da v1, 3 por trecho), do anel ao teto."""
    P = DOME
    out = []
    for i in range(len(P) - 1):
        p0, p1, p2, p3 = P[max(i - 1, 0)], P[i], P[i + 1], P[min(i + 2, len(P) - 1)]
        for k in range(3):
            t = k / 3
            t2, t3 = t * t, t * t * t
            out.append(tuple(0.5 * ((2 * p1[c]) + (-p0[c] + p2[c]) * t + (2 * p0[c] - 5 * p1[c] + 4 * p2[c] - p3[c]) * t2
                                    + (-p0[c] + 3 * p1[c] - 3 * p2[c] + p3[c]) * t3) for c in (0, 1)))
    out.append(P[-1])
    return out


def turret():
    p = Part(PREFIX + '_Turret')
    for k, bv in (('Paint#dome', 0.0), ('Paint#mant', 0.0), ('Paint#plate', 0.025), ('Paint#thin', 0.01), ('Bag#sack', 0.0),
                  ('Wood#crate', 0.02), ('Lens', 0.0)):
        p.bm(k, bv)
    # ---------------------------------------------------------------- a cúpula fundida (lisa) e o bojo pra frente
    prof = [(d, max(z, 0.014)) for d, z in dome_profile()]               # o pé 1,4 cm acima do deck: o giro não raspa no anel
    vp.lathe(p, 'Paint#dome', (TX, 0.0, DECK), 'z', prof, seg=66, hard=50.0)      # 66: a aresta < 14 cm (a grade não corta)
    bd = p.bms['Paint#dome']
    foot = [v for v in bd.verts if abs(v.co.z - (DECK + 0.014)) < 1e-5]
    foot.sort(key=lambda v: math.atan2(v.co.y, v.co.x - TX))
    bd.faces.new(foot)                    # a tampa de baixo (escondida no deck) num polígono só
    for v in p.bms['Paint#dome'].verts:
        v.co.x += fwd(v.co.z - DECK)
    sup_d = Support([(p, ['Paint#dome'])])

    def on_dome(x, y):
        hit = sup_d.cast((x, y, DECK + 2.0), (0, 0, -1), 3.0)
        return (hit[0], hit[1]) if hit else (Vector((x, y, DECK)), ZUP)

    def base_z(x, y, r):
        """O pé de uma peça redonda na cúpula: a altura mais baixa da cúpula embaixo do contorno."""
        return min(on_dome(x + r * math.cos(a), y + r * math.sin(a))[0].z for a in [k * math.pi / 4 for k in range(8)])
    # ---------------------------------------------------------------- o mantelete arredondado, a capa de lona, a coaxial
    mant = [(0.0, 1.42), (0.33, 1.42), (0.35, 1.56), (0.345, 1.72), (0.32, 1.86), (0.28, 1.96), (0.22, 2.03), (0.165, 2.055),
            (0.0, 2.055)]
    vp.lathe(p, 'Paint#mant', (0.0, 0.0, GUN_Z), 'x', mant, seg=36)
    for v in p.bms['Paint#mant'].verts:
        v.co.z = GUN_Z + (v.co.z - GUN_Z) * 0.82
    vp.bellows(p, 'Bag', 1.99, 2.17, 0.215, 0.135, GUN_Z, folds=3, depth=0.016, seg=28)
    vp.lathe(p, 'Dark', (1.93, 0.21, GUN_Z - 0.02), 'x', [(0.0, 0.0), (0.035, 0.0), (0.035, 0.07), (0.0, 0.07)], seg=10)
    # ---------------------------------------------------------------- o holofote grande à direita (o L-2 do T-55) com o braço
    lc = Vector((1.62, 0.98, 2.42))
    vp.lathe(p, 'Paint', lc, 'x', [(0.0, -0.17), (0.15, -0.17), (0.22, -0.12), (0.25, -0.03), (0.25, 0.09), (0.235, 0.115),
                                   (0.215, 0.105), (0.0, 0.105)], seg=28)
    vp.lathe(p, 'Lens', lc, 'x', [(0.0, 0.112), (0.15, 0.11), (0.2, 0.104), (0.216, 0.1), (0.0, 0.1)], seg=28)
    vp.tube(p, 'Metal', [(lc.x - 0.05, lc.y - 0.2, lc.z - 0.06), (1.72, 0.45, GUN_Z + 0.08), (1.85, 0.25, GUN_Z + 0.04)], 0.024, seg=8)
    vp.rbox(p, 'Metal', (lc.x - 0.02, lc.y, lc.z - 0.27), (0.14, 0.09, 0.08), r=0.02, k=1)
    vp.tube(p, 'Metal', [(lc.x - 0.02, lc.y, lc.z - 0.27), (lc.x - 0.12, lc.y - 0.12, 2.12)], 0.022, seg=6)
    # ---------------------------------------------------------------- as chapas de remendo soldadas na cúpula (curvadas)
    plates = [('Patch', -82.0, 15.0, 40.0, 34.0, 1.0), ('Rust', -40.0, 17.0, 37.0, 22.0, 2.0), ('Rust', 28.0, 19.0, 41.0, 18.0, 3.0),
              ('Rust', 46.0, 14.0, 33.0, 15.0, 4.0), ('Patch', -60.0, 36.0, 47.0, 20.0, 5.0)]
    done_keys = ['Paint#dome']
    for m, az, e0, e1, daz, sd in plates:
        sup_pl = Support([(p, done_keys)])
        dome_plate(p, sup_pl, m, az, e0, e1, daz, seed=sd)
        if m + '#dplate' not in done_keys:
            done_keys.append(m + '#dplate')
    # ---------------------------------------------------------------- a camuflagem e as faixas pintadas (por cima das chapas)
    sup_pt = Support([(p, done_keys)])
    for (az, d, m, r, rot, asp) in ((20, 1.0, 'Patch', 0.25, 30, 1.4), (78, 0.92, 'Rust', 0.22, -20, 1.3), (-35, 0.78, 'Patch', 0.2, 70, 1.2),
                                    (-125, 1.05, 'Rust', 0.24, 10, 1.4), (160, 0.72, 'Patch', 0.2, -40, 1.3), (-80, 0.42, 'Rust', 0.17, 0, 1.2),
                                    (45, 0.4, 'Patch', 0.15, 20, 1.3), (-165, 1.18, 'Patch', 0.22, 80, 1.5), (5, 1.35, 'Rust', 0.14, 0, 1.6),
                                    (115, 1.3, 'Patch', 0.18, 0, 1.4)):
        x, y = TX + d * math.cos(math.radians(az)), d * math.sin(math.radians(az))
        hit, n = on_dome(x, y)
        decal_blob(p, sup_pt, m, hit, n, r * 0.9, aspect=asp, rot=rot, seed=az * 0.1 + d)
    # a faixa ao longo do teto (da frente pra trás) e as 2 diagonais na frente-esquerda (a ficha de cima e de 3/4)
    decal_stripe(p, sup_pt, 'Team', [(TX + 1.33, 0.0, DECK + 2.5), (TX - 1.28, 0.0, DECK + 2.5)], 0.17, lift=0.007, seed=4.0)
    for k, off in enumerate((0.0, 0.26)):
        a0 = Vector((TX + 1.08 - off * 0.6, -0.12 - off * 0.85, DECK + 2.5))
        a1 = Vector((TX + 0.12 - off * 0.6, -1.3 - off * 0.55, DECK + 2.5))
        decal_stripe(p, sup_pt, 'Team', [a0, (a0 + a1) / 2 + Vector((0.05, 0.0, 0.0)), a1], 0.14, lift=0.007, seed=5.0 + k)
    # ---------------------------------------------------------------- a cúpula do comandante (esquerda), a escotilha do municiador
    cx, cy = TX - 0.234, -0.598
    zc = base_z(cx, cy, 0.4)
    vp.lathe(p, 'Paint', (cx, cy, zc - 0.06), 'z', [(0.0, 0.0), (0.42, 0.0), (0.42, 0.2), (0.39, 0.24), (0.33, 0.25), (0.0, 0.25)], seg=32)
    for k in range(7):
        a = math.radians(k * 45 - 150)
        p.obox('Lens', (cx + 0.405 * math.cos(a), cy + 0.405 * math.sin(a), zc + 0.1), (0.02, 0.13, 0.05), yaw=k * 45 - 150, bevel=0.0)
    vp.hatch_round(p, (cx, cy, zc + 0.15), 0.29, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
    lx, ly = TX - 0.26, 0.65
    zl = base_z(lx, ly, 0.36)
    vp.lathe(p, 'Paint', (lx, ly, zl - 0.06), 'z', [(0.0, 0.0), (0.37, 0.0), (0.37, 0.12), (0.34, 0.15), (0.0, 0.15)], seg=28)
    vp.hatch_round(p, (lx, ly, zl + 0.04), 0.28, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
    # a DShK de 12,7 mm no pedestal na frente da escotilha, apontando pra frente
    gx, gy = TX + 0.16, 0.5
    hmg(p, (gx, gy, base_z(gx, gy, 0.08) - 0.02), yaw=0.0, scale=1.18, m='Metal', dark='Dark')
    # o ventilador (o cogumelo na frente-direita) e a mira do atirador (a capota na frente-esquerda)
    vx, vy = TX + 0.82, 0.3
    zv = base_z(vx, vy, 0.14)
    vp.lathe(p, 'Paint', (vx, vy, zv - 0.04), 'z', [(0.0, 0.0), (0.15, 0.0), (0.15, 0.06), (0.13, 0.1), (0.07, 0.13), (0.0, 0.135)], seg=20)
    vp.lathe(p, 'Dark', (vx, vy, zv + 0.03), 'z', [(0.148, 0.0), (0.156, 0.0), (0.156, 0.02), (0.148, 0.02)], seg=20, closed=True)
    sx_, sy_ = TX + 0.88, -0.36
    zs = base_z(sx_, sy_, 0.14)
    vp.rbox(p, 'Paint#plate', (sx_, sy_, zs + 0.06), (0.3, 0.22, 0.16), r=0.06, k=3)
    vp.rbox(p, 'Lens', (sx_ + 0.15, sy_, zs + 0.09), (0.01, 0.15, 0.06), r=0.01, k=1)
    # os corrimãos dos lados da cúpula
    for (a0, a1, d) in ((-128.0, -108.0, 1.3), (-58.0, -40.0, 1.33), (58.0, 72.0, 1.33)):
        pa, na = on_dome(TX + d * math.cos(math.radians(a0)), d * math.sin(math.radians(a0)))
        pb, nb = on_dome(TX + d * math.cos(math.radians(a1)), d * math.sin(math.radians(a1)))
        vp.grab_handle(p, 'Metal', pa, pb, (na + nb).normalized(), height=0.07, r=0.014)
    # ---------------------------------------------------------------- o cesto de tubo atrás da torre, com a carga
    bx0, bx1, by = -1.0, -1.62, 0.82
    z0, z1 = 1.97, 2.34
    for z in (z0, z1):
        vp.tube(p, 'Metal', vp.bent_bar([(bx0, -by, z), (bx1, -by, z), (bx1, by, z), (bx0, by, z)], R=0.1, n=4), 0.02, seg=8)
    for s in (-1, 1):
        for x in (bx0 + 0.04, bx1 + 0.03):
            vp.tube(p, 'Metal', [(x, by * s, z0 - 0.02), (x, by * s, z1 + 0.01)], 0.018, seg=8)
    for k in range(6):
        y = -by + 2 * by * (k + 0.5) / 6
        vp.tube(p, 'Metal', [(bx1, y, z0), (bx1, y, z1)], 0.014, seg=6)
    for y in (-0.6, -0.2, 0.2, 0.6):
        vp.tube(p, 'Metal', [(bx0 - 0.1, y, z0 - 0.01), (bx1, y, z0 - 0.01)], 0.017, seg=6)
    for s in (-1, 1):
        pa, _ = on_dome(bx0 + 0.12, 0.5 * s)
        vp.tube(p, 'Metal', [(bx0 + 0.04, 0.5 * s, z0), (pa.x + 0.02, pa.y, pa.z)], 0.02, seg=6)
    crate(p, (-1.33, -0.42, z0 + 0.02), (0.44, 0.52, 0.28), yaw=90)
    vp.ammo_box(p, (-1.32, 0.16, z0 + 0.15), (0.42, 0.22, 0.26), yaw=0, m='Patch', latch_m='Dark')
    vp.ammo_box(p, (-1.34, 0.42, z0 + 0.15), (0.42, 0.22, 0.26), yaw=4, m='Patch', latch_m='Dark')
    vp.rbox(p, 'Rust#plate', (bx1 - 0.03, 0.55, z1 + 0.2), (0.025, 0.36, 0.5), roll=6, r=0.03, k=2)   # a chapa em pé (a ficha de 3/4)
    # ---------------------------------------------------------------- os sacos: a meia-lua na borda da cúpula, da direita até atrás
    lay1, lay2 = [], []
    for k, az in enumerate((72, 96, 117, 142, 163, 189, 211, 234)):
        a = math.radians(az)
        d = 1.2 + 0.05 * (h1(k, 3.3) - 0.5)
        hit, n = on_dome(TX + d * math.cos(a), d * math.sin(a))
        lay1.append((hit, (-math.sin(a), math.cos(a), 0.0), (n + ZUP * 0.8).normalized(), 30.0 + k))
    for k, az in enumerate((106, 129, 152, 175, 198)):
        a = math.radians(az)
        hit, n = on_dome(TX + 1.06 * math.cos(a), 1.06 * math.sin(a))
        lay2.append((hit + n * 0.1, (-math.sin(a), math.cos(a), 0.0), (n + ZUP * 1.4).normalized(), 50.0 + k))
    bag_pile(p, ['Paint#dome', 'Patch#dplate', 'Rust#dplate'], [lay1, lay2], shear=0.35)
    sup_b = Support([(p, ['Paint#dome', 'Bag#sack'])])
    ring_pts = []
    for k, az in enumerate(range(64, 246, 13)):
        a = math.radians(az)
        q = (TX + 1.28 * math.cos(a), 1.28 * math.sin(a))
        if k == 0 or az + 13 >= 246:
            hit, _ = on_dome(TX + 1.22 * math.cos(a), 1.22 * math.sin(a))
            ring_pts.append((hit.x, hit.y, hit.z + 0.01))
        else:
            ring_pts.append(q)
    rope_over(p, sup_b, ring_pts, m='Wood', n=4)
    # os 2 sacos no cesto
    bag_pile(p, ['Wood#crate', 'Patch#round', 'Metal#vp'],
             [[(Vector((-1.3, -0.1, z0 + 0.4)), (0, 1, 0), ZUP, 70.0), (Vector((-1.34, 0.32, z0 + 0.5)), (0.3, 1, 0), ZUP, 71.0)]])
    # ---------------------------------------------------------------- a antena e as 2 bandeiras (o executor tira o pano pro flags.json)
    ax, ay = TX - 0.55, 0.62
    za = base_z(ax, ay, 0.05)
    p.cyl('Dark#vp', (ax, ay, za + 0.04), 'z', 0.05, 0.12, seg=10, bevel=0.0)
    p.cyl('Metal#vp', (ax, ay, za + 0.85), 'z', 0.011, 1.5, seg=6, bevel=0.0)
    fx, fy = TX - 0.76, -0.42
    zf = base_z(fx, fy, 0.05)
    top = 4.12
    p.cyl('Dark#vp', (fx, fy, zf + 0.05), 'z', 0.05, 0.12, seg=10, bevel=0.0)
    p.cyl('Metal#vp', (fx, fy, (zf + top) / 2), 'z', 0.018, top - zf, seg=8, bevel=0.0)
    flag(p, (fx, fy), top - 0.02, 0.72, 0.46)
    p.cyl('Metal#vp', (bx1 + 0.03, by, (z1 + 3.05) / 2), 'z', 0.014, 3.05 - z1, seg=6, bevel=0.0)
    flag(p, (bx1 + 0.03, by), 3.03, 0.4, 0.27, phase=1.3)
    return p.finish()


# ---------------------------------------------------------------------------------- cano
def barrel():
    """O canhão de 100 mm (D-10T): a luva grossa no mantelete, o degrau, o tubo fino e o EVACUADOR na boca (o do T-55 fica na
    ponta), a boca com o furo; o esparadrapo verde enrolado à mão (3 voltas tortas) e as 2 braçadeiras."""
    p = Part(PREFIX + '_Barrel')
    x0, M = BARREL_X0, MUZZLE_X
    st = [(x0 - 0.14, 0.118), (x0 + 0.02, 0.118), (x0 + 0.06, 0.112), (x0 + 0.92, 0.106), (x0 + 0.98, 0.097), (x0 + 1.06, 0.091),
          (M - 0.8, 0.083), (M - 0.75, 0.104), (M - 0.69, 0.116), (M - 0.17, 0.116), (M - 0.11, 0.104), (M - 0.04, 0.098), (M, 0.094)]
    vp.gun_barrel(p, st, GUN_Z, m='Paint', seg=32, bore=0.05, bore_depth=0.35, bore_m='Dark')
    for xa in (M - 0.66, M - 0.24):
        vp.ring_band(p, 'Metal', xa, xa + 0.04, 0.116, GUN_Z, lip=0.007)
    r = 0.087
    for xc, tilt, yaw in ((x0 + 2.42, 5.0, 30.0), (x0 + 2.5, -4.0, -60.0), (x0 + 2.58, 3.0, 110.0)):
        t, w_ = math.radians(tilt), math.radians(yaw)
        A = Vector((math.cos(t), math.sin(t) * math.cos(w_), math.sin(t) * math.sin(w_)))
        vp.lathe(p, 'Team', (xc, 0.0, GUN_Z), A, [(r - 0.004, -0.04), (r + 0.007, -0.04), (r + 0.01, -0.032), (r + 0.01, 0.032),
                                                  (r + 0.007, 0.04), (r - 0.004, 0.04)], seg=32, closed=True)
    return p.finish()


# ---------------------------------------------------------------------------------- rodagem
def starfish_wheel(p, c, side):
    """A roda de apoio "starfish" do T-55 (o par de discos com o vão do dente-guia): a banda de borracha de ombro redondo, o disco com
    os 5 RAIOS em relevo e os vãos escuros entre eles, o cubo saliente com o anel de porcas."""
    C = Vector(c)
    A = Vector((0.0, 1.0, 0.0))
    R = WHEEL_R
    hw = 0.06
    gap = 0.07
    for i, off in enumerate((side * (gap / 2 + hw), -side * (gap / 2 + hw))):
        Cw = C + A * off
        if i:                 # a de dentro (só aparece pelo vão): o perfil curto e menos lados
            band = vp._band(R, R - 0.075, hw, k=0.018)
            vp.lathe(p, 'Rubber', Cw, A, [band[0], band[2], band[4], band[5], band[7], band[9]], seg=14, smooth=True, closed=True)
            vp.lathe(p, 'Paint', Cw, A, [(0.1, hw - 0.012), (R - 0.07, hw - 0.012), (R - 0.07, -hw + 0.012), (0.1, -hw + 0.012)], seg=14,
                     smooth=False, closed=True)
            continue
        vp.lathe(p, 'Rubber', Cw, A, vp._band(R, R - 0.075, hw, k=0.018), seg=24, smooth=True, closed=True)
        As = A * side
        prof = [(0.0, hw + 0.05), (0.07, hw + 0.05), (0.085, hw + 0.036), (0.1, hw + 0.022), (0.125, hw + 0.012), (0.29, hw + 0.004),
                (0.31, hw - 0.006), (R - 0.07, hw - 0.008), (R - 0.07, -hw + 0.01), (0.0, -hw + 0.01)]
        vp.lathe(p, 'Paint', Cw, As, prof, seg=24)
        for k in range(5):
            a = 2 * math.pi * k / 5 + 0.3
            radial = Vector((math.cos(a), 0.0, math.sin(a)))
            tang = As.cross(radial).normalized()
            vp.rbox_q(p, 'Paint', Cw + As * (hw + 0.016) + radial * 0.21, (0.2, 0.075, 0.026), r=0.03, k=2, frame=(radial, tang, As))
            a2 = a + math.pi / 5
            rad2 = Vector((math.cos(a2), 0.0, math.sin(a2)))
            p.cyl('Dark#vp', tuple(Cw + As * (hw - 0.002) + rad2 * 0.215), tuple(As), 0.062, 0.006, seg=10, bevel=0.0)
        vp.bolt_ring(p, 'Metal', Cw + As * (hw + 0.024), As, 0.1, 6, r=0.017, h=0.018, phase=0.2)
        p.cyl('Metal#vp', tuple(Cw + As * (hw + 0.06)), tuple(As), 0.05, 0.03, seg=12, bevel=0.0)
    p.cyl('Metal#vp', tuple(C), 'y', 0.08, gap + 0.02, seg=12, bevel=0.0)


def wheels():
    p = Part(PREFIX + '_Wheels')
    for s in (-1, 1):
        y = TRACK_Y * s
        for x in WHEELS_X:
            starfish_wheel(p, (x, y, WHEEL_Z), s)
        vp.idler(p, (IDLER[0], y, IDLER[1]), IDLER[2], 0.3, s, 'Paint', 'Metal', gap=0.07, seg=22, nuts=6)
        vp.sprocket(p, (SPROCKET[0], y, SPROCKET[1]), SPROCKET[2], 0.3, s, 'Paint', 'Metal', teeth=13, seg=22, holes_m='Dark')
    return p.finish()


def track_samples():
    """O caminho da esteira do T-55: o envoltório das rodas, com o trecho de cima DEITADO nas rodas de apoio (sem rolete de cima: a
    esteira apoia no topo de cada roda e cede entre elas), reamostrado no passo do elo."""
    m = T_IN + 0.004
    circles = [(SPROCKET[0], SPROCKET[1], SPROCKET[2] + m + 0.012), (IDLER[0], IDLER[1], IDLER[2] + m)] + \
              [(x, WHEEL_Z, WHEEL_R + m) for x in WHEELS_X]
    dense = [Vector((x, 0.0, z)) for (x, z), _ in track_path(circles, 0.01)]
    sups = sorted([(cx, cz + r) for cx, cz, r in circles], key=lambda t: t[0])
    for q in dense:
        if not (SPROCKET[0] < q.x < IDLER[0]) or q.z < WHEEL_Z + 0.25:
            continue
        caps = [cz + math.sqrt(r * r - (q.x - cx) ** 2) for cx, cz, r in circles if abs(q.x - cx) < r]
        cap = max(caps) if caps else -9.0
        zc = q.z
        for (xa, za), (xb, zb) in zip(sups[:-1], sups[1:]):
            if xa <= q.x <= xb:
                t = (q.x - xa) / (xb - xa)
                zc = lerp(za, zb, t) - 0.03 * (xb - xa) * math.sin(math.pi * t)
                break
        q.z = min(q.z, max(cap, zc))
    n = len(dense)
    segl = [(dense[(i + 1) % n] - dense[i]).length for i in range(n)]
    total = sum(segl)
    cnt = int(round(total / PITCH))
    step = total / cnt
    out = []
    k, acc = 0, 0.0
    for i in range(cnt):
        t = i * step
        while acc + segl[k] < t:
            acc += segl[k]
            k += 1
        a, b = dense[k], dense[(k + 1) % n]
        q = a + (b - a) * ((t - acc) / max(segl[k], 1e-9))
        out.append(q)
    res = []
    for i in range(cnt):
        a, b = out[(i - 1) % cnt], out[(i + 1) % cnt]
        res.append(((out[i].x, out[i].z), math.atan2(b.z - a.z, b.x - a.x)))
    return res, step


def track(side):
    """A esteira de elos do T-55 (OMSh: a sapata de uma peça com a garra, o dente-guia no meio, o pino com as pontas nas bordas)."""
    s = 1 if side == 'r' else -1
    p = Part(f'{PREFIX}_Track_{side}')
    y = TRACK_Y * s
    samples, step = track_samples()
    L = PITCH * 0.86
    hw = TRACK_W / 2
    sw = 0.556            # a sapata (os pinos saem até 0,58): a grade de 14 cm corta em 4, não em 8
    shoe = vp._shoe_pts(L, T_IN, T_OUT, GROUSER, PITCH * 0.22, k=1)
    horn = [(-PITCH * 0.3, T_OUT * 0.3), (PITCH * 0.3, T_OUT * 0.3), (PITCH * 0.3, -T_IN + 0.004), (PITCH * 0.16, -T_IN - HORN_H),
            (-PITCH * 0.16, -T_IN - HORN_H), (-PITCH * 0.3, -T_IN + 0.004)]
    ey = Vector((0.0, 1.0, 0.0))
    T = T_IN + T_OUT
    for (x, z), ang in samples:
        eu = Vector((math.cos(ang), 0.0, math.sin(ang)))
        en = Vector((math.sin(ang), 0.0, -math.cos(ang)))
        O = Vector((x, y, z))
        vp.prism(p, 'Track', shoe, O + ey * (-sw / 2), eu, en, ey, sw)
        J = O + eu * (PITCH / 2) + en * ((T_OUT - T_IN) / 2)
        p.cyl('Metal#vp', tuple(J + ey * s * (sw / 2 + 0.008)), 'y', T * 0.42 + 0.004, 0.026, seg=6, bevel=0.0)
        vp.prism(p, 'Track', horn, O + ey * (-HORN_W / 2), eu, en, ey, HORN_W)
    o = p.finish()
    o['track_step'] = step
    o['track_frames'] = [c for (x, z), ang in samples for c in (x, y, z, math.cos(ang), math.sin(ang))]
    return o


# ---------------------------------------------------------------------------------- conferência e exportação
def write_spec():
    spec = {
        'parts': [[PREFIX + n, [0, 0, 0]] for n in ('_Hull', '_Turret', '_Barrel', '_Wheels', '_Track_l', '_Track_r')],
        'views': [['top', 'ortho', [0, 0, -1], [0, -1, 0], [1800, 1000]],
                  ['left', 'ortho', [0, -1, 0], [0, 0, 1], [1800, 760]],
                  ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1000, 760]],
                  ['back', 'ortho', [1, 0, 0], [0, 0, 1], [1000, 760]],
                  ['three_quarter', 'persp', 30.0, 215.0, [1600, 1000], 30.0, 0.78],
                  ['back_three_quarter', 'persp', 30.0, 35.0, [1600, 1000], 30.0, 0.78]],
        'usa_at': [1.0, -7.0, 0], 'tank_views': False,
        'game': [['game_cam', 37.5, 40.0, 50.0, 0.85], ['game_cam_close', 37.5, 40.0, 30.0, 0.8]],
    }
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)


def main():
    clear_scene()
    h, t, b, w = hull(), turret(), barrel(), wheels()
    tl, tr = track('l'), track('r')
    sweep_check([h], [t, b], (TX, 0.0))
    total = 0
    for o in (h, t, b, w, tl, tr):
        total += export(o, OUT)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL tris por peca', {o.name: tris(o) for o in (h, t, b, w, tl, tr)})
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h, t, b, w, tl, tr])
    (hx0, hx1), (hy0, hy1), _ = bounds([w, tl, tr])
    (tx0, tx1), (ty0, ty1), (tz0, tz1) = bounds([t])
    print('MODEL medidas: comprimento %.2f m com o cano (x %.2f..%.2f), largura %.2f, altura %.2f; rodagem x %.2f..%.2f y %.2f..%.2f'
          % (ax1 - ax0, ax0, ax1, ay1 - ay0, az1, hx0, hx1, hy0, hy1))
    print('MODEL torre: x %.2f..%.2f, y %.2f..%.2f, z %.2f..%.2f' % (tx0, tx1, ty0, ty1, tz0, tz1))
    print('MODEL turret_pivot_m', [TX, 0.0], 'muzzle_height_m %.3f' % GUN_Z, 'a boca em x %.3f' % MUZZLE_X, 'tracks_m',
          [round(2 * TRACK_Y, 3), TRACK_W], 'track_step %.5f' % tl['track_step'])
    write_spec()


main()
