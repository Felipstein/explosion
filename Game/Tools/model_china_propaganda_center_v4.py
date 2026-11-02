"""O Propaganda Center da China, versão 4: O HERÓI DO MEGAFONE (02/11/2026), modelado no Blender por script.

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_china_propaganda_center_v4.py C:/Users/luisf/explosion/captures/game/models/china_propaganda_center_v4

O veredito de 02/11 (docs/elenco/IDEIAS_NOVAS_02_11.md): o pagode (v1), o salão redondo (v2) e a emissora no laço da CCTV (v3) foram
reprovados; "quero ideias do 0, diferente, novas, bonitas, bem feitas, criativas". O erro comum: a ideia era o lugar, não uma COISA.

A palavra (o que lê em 1 segundo): A ESTÁTUA DO SOLDADO COM O MEGAFONE ERGUIDO.
- A estátua monumental no realismo socialista (os soldados de bronze dos memoriais chineses), de um soldado ANÔNIMO do ELP: o casaco
  de inverno comprido aberto na frente e jogado pra trás pelo vento, o quepe mole com a estrela, a passada larga com o pé da frente
  na pedra, o peito pra frente, o braço direito erguido com o MEGAFONE na mão (a voz do prédio sai dele), o punho esquerdo fechado
  lançado pra trás, o lenço ao vento. Bronze escurecido (o verdete nas dobras). 8,5 m de figura (um homem de 1,80 × 4,7), o punho a
  ~14,9 m e o topo do megafone a ~16,3 m do chão. O rosto é genérico (a forma do crânio, o queixo, o nariz, a sobrancelha): nunca uma pessoa.
  A figura é feita de volumes (troncos de seção elíptica, elipsoides, a casca do casaco), unidos pelo remesh de voxel e alisados:
  a forma é a do escultor (a carne e o pano), não a do boneco de juntas.
- O pedestal escalonado de granito vermelho, que por dentro é o estúdio da rádio: os 3 degraus de granito cinza em volta, o bloco
  vermelho com o rodapé e a cornija escuros (o labradorito), a PORTA DE BRONZE na frente (+X) no batente fundo de granito claro, as
  janelas estreitas acesas (as frestas altas) dos 4 lados, a estrela do ELP com o 八一 no plinto de cima e as escorridas de verdete
  que o bronze deixa na pedra.
- A praça: o caminho de lajes da frente até a porta, ladeado pela FILA DE BANDEIRAS vermelhas (o pano no vento: flags.json); os 4
  POSTES DE CORNETA (o concreto, a cruzeta, as 4 cornetas) com os fios até o pedestal. O resto é o chão do mapa (nada de laje).
- De cima: o bloco vermelho com a cruz dos degraus, a figura escura com o braço e o megafone apontando pra frente-direita, e a sombra
  comprida da estátua no chão; a fila vermelha das bandeiras na frente.

O lote: 14 × 24 m (x −7..7, y −12..12, o do Units.json), frente em +X. A câmera do jogo vem de +X/−Y (37,5°).

Peças (origem no chão, no centro do lote; coordenadas da Unreal, X frente, Y direita, Z cima, metros):
  SM_China_PropagandaCenter_Base: tudo o que não se mexe (o pedestal, a estátua sem o megafone, a praça, os postes);
  SM_China_PropagandaCenter_Speakers: o megafone (o cabo de pistola, o corpo, a campânula), feito com a origem no eixo do cabo, no
    meio do punho: spin em Z, at_m = SPK_AT (a mão não sai do lugar; o megafone gira no punho e varre o campo);
  SM_China_PropagandaCenter_Scaffold: o andaime da obra (tubos de aço, scaffold() do model_lib).
Materiais (os slots da China): Red (o granito vermelho: falta o slot próprio, CnGranite, ver o relatório), CnPlinth (o granito cinza
dos degraus, o batente, o caminho), Dark (o labradorito: rodapé e cornija), CnMetal (o BRONZE da estátua, da porta e do megafone:
falta o slot próprio, CnBronze), GlaTile (o verdete nas dobras e as escorridas), CnGold (a estrela, os frisos), CnConcrete (os postes),
Paint (as cornetas), Light (as frestas acesas), Glass, Player (as bandeiras), Trim, Hazard (só no andaime).
"""
import json
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import model_lib  # noqa: E402
from model_lib import COLORS, V, clear_scene, export, flag, scaffold, select_only  # noqa: E402,F401
from model_faction_kit import star_wall, speaker_cluster  # noqa: E402
from model_building_parts import BPart, _hash, bake_in_place, catenary, splits, tube  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'china_propaganda_center_v4')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_China_PropagandaCenter'

# as cores de conferência (sRGB): o bronze e o granito com a cor que eu quero (os slots novos, ver o relatório); o verdete no GlaTile
COLORS.update({'CnMetal': (77, 63, 48), 'Red': (124, 72, 62), 'GlaTile': (69, 124, 118), 'Paint': (196, 196, 190)})

HX, HY = 7.0, 12.0                    # o lote 14 × 24
PX = -1.6                             # o eixo do pedestal (x); y = 0
STEPS = [(4.4, 4.6, 0.17), (4.0, 4.2, 0.34), (3.6, 3.8, 0.51)]   # os degraus: meia-largura x, meia-largura y, topo
SX0, SX1, SY = -3.2 + PX, 3.2 + PX, 3.4                         # o bloco do estúdio (x0, x1, meia-largura y)
Z0, ZS, ZC = 0.51, 3.6, 3.95          # o pé do estúdio, o topo da parede, o topo da cornija
TX0, TX1, TY, ZT = -2.7 + PX, 2.7 + PX, 2.9, 4.6                 # o plinto de cima
LEAN = 0.025                          # o talude da parede vermelha (m por m)
S = 4.7                               # a escala da figura (homem de 1,80 m → 8,5 m; o topo do megafone a ~16,3 m)
FIG = (PX + 0.15, 0.0, 5.0)           # a origem da figura (o topo da laje de bronze), Unreal
FIG_YAW = -8.0                        # a figura vira um pouco pra câmera (−Y)
VOXEL = 0.045                         # o voxel do remesh da figura (m)


# ===================================================================================================== a figura: o espaço dela
def fig_w(q):
    """Do espaço da figura (o homem de 1,80 m, os pés em z = 0, de frente pra +X, a direita em +Y) pro mundo (Unreal)."""
    a = math.radians(FIG_YAW)
    x, y = q[0] * math.cos(a) - q[1] * math.sin(a), q[0] * math.sin(a) + q[1] * math.cos(a)
    return Vector((FIG[0] + S * x, FIG[1] + S * y, FIG[2] + S * q[2]))


def nrm(v):
    v = Vector(v)
    return v / max(v.length, 1e-9)


def ik(a, t, l1, l2, pole):
    """O joelho/cotovelo das 2 peças (l1, l2) da raiz a até o alvo t, dobrando pro lado do pole."""
    A, T = Vector(a), Vector(t)
    d = min((T - A).length, l1 + l2 - 1e-4)
    u = nrm(T - A)
    x = (l1 * l1 - l2 * l2 + d * d) / (2 * d)
    h = math.sqrt(max(l1 * l1 - x * x, 0.0))
    pv = Vector(pole) - u * u.dot(Vector(pole))
    return A + u * x + nrm(pv) * h


class Sculpt:
    """Os volumes da figura numa bmesh só (cada um fechado; o remesh de voxel une tudo)."""

    def __init__(self):
        self.b = bmesh.new()

    def loft(self, rings, caps=True):
        vr = [[self.b.verts.new(V(*fig_w(q))) for q in r] for r in rings]
        n = len(rings[0])
        for r0, r1 in zip(vr[:-1], vr[1:]):
            for i in range(n):
                j = (i + 1) % n
                self.b.faces.new([r0[i], r0[j], r1[j], r1[i]])
        if caps:
            self.b.faces.new(list(reversed(vr[0])))
            self.b.faces.new(vr[-1])

    def limb(self, a, b, prof, front=(1.0, 0.0, 0.0), seg=16, n=10, expo=2.0):
        """O tronco de a até b, a seção elíptica prof(t) → (raio na frente, raio do lado), a frente pra 'front'."""
        A, B = Vector(a), Vector(b)
        d = nrm(B - A)
        f = Vector(front) - d * d.dot(Vector(front))
        if f.length < 1e-4:
            f = Vector((0.0, 0.0, 1.0)) - d * d.z
        f = nrm(f)
        s = d.cross(f)
        rings = []
        for i in range(n + 1):
            t = i / n
            c = A + (B - A) * t
            rf, rs = prof(t)
            ring = []
            for k in range(seg):
                th = 2 * math.pi * k / seg
                ct, st = math.cos(th), math.sin(th)
                if expo != 2.0:
                    ct, st = math.copysign(abs(ct) ** (2 / expo), ct), math.copysign(abs(st) ** (2 / expo), st)
                ring.append(c + f * (rf * ct) + s * (rs * st))
            rings.append(ring)
        self.loft(rings)

    def ell(self, c, r, R=None, seg=16, rings=10):
        """O elipsoide de raios r (no quadro R: as colunas são os eixos)."""
        R = R or Matrix.Identity(3)
        C = Vector(c)
        rr = []
        for j in range(1, rings):
            ph = math.pi * j / rings - math.pi / 2
            rr.append([C + R @ Vector((r[0] * math.cos(ph) * math.cos(2 * math.pi * k / seg),
                                       r[1] * math.cos(ph) * math.sin(2 * math.pi * k / seg), r[2] * math.sin(ph)))
                       for k in range(seg)])
        bot = self.b.verts.new(V(*fig_w(C + R @ Vector((0, 0, -r[2])))))
        top = self.b.verts.new(V(*fig_w(C + R @ Vector((0, 0, r[2])))))
        vr = [[self.b.verts.new(V(*fig_w(q))) for q in ring] for ring in rr]
        for r0, r1 in zip(vr[:-1], vr[1:]):
            for i in range(seg):
                j = (i + 1) % seg
                self.b.faces.new([r0[i], r0[j], r1[j], r1[i]])
        for i in range(seg):
            j = (i + 1) % seg
            self.b.faces.new([bot, vr[0][j], vr[0][i]])
            self.b.faces.new([top, vr[-1][i], vr[-1][j]])

    def ball(self, c, r, seg=14):
        self.ell(c, (r, r, r), seg=seg, rings=max(6, seg // 2))

    def shell(self, fn, nu, nv, t):
        """A casca de espessura t (o pano do casaco, o lenço): a superfície fn(u, v) afastada ± t/2 pela normal, fechada nas bordas."""
        P = [[Vector(fn(i / nu, j / nv)) for i in range(nu + 1)] for j in range(nv + 1)]
        N = []
        for j in range(nv + 1):
            row = []
            for i in range(nu + 1):
                du = P[j][min(i + 1, nu)] - P[j][max(i - 1, 0)]
                dv = P[min(j + 1, nv)][i] - P[max(j - 1, 0)][i]
                row.append(nrm(du.cross(dv)))
            N.append(row)
        top = [[self.b.verts.new(V(*fig_w(P[j][i] + N[j][i] * t / 2))) for i in range(nu + 1)] for j in range(nv + 1)]
        bot = [[self.b.verts.new(V(*fig_w(P[j][i] - N[j][i] * t / 2))) for i in range(nu + 1)] for j in range(nv + 1)]
        for j in range(nv):
            for i in range(nu):
                self.b.faces.new([top[j][i], top[j][i + 1], top[j + 1][i + 1], top[j + 1][i]])
                self.b.faces.new([bot[j + 1][i], bot[j + 1][i + 1], bot[j][i + 1], bot[j][i]])
        edge = ([(0, i) for i in range(nu)] + [(j, nu) for j in range(nv)] + [(nv, i) for i in range(nu, 0, -1)]
                + [(j, 0) for j in range(nv, 0, -1)])
        for (j0, i0), (j1, i1) in zip(edge, edge[1:] + edge[:1]):
            self.b.faces.new([top[j0][i0], bot[j0][i0], bot[j1][i1], top[j1][i1]])


def rot_ypr(yaw=0.0, pitch=0.0, roll=0.0):
    """O quadro (colunas = eixos x, y, z locais) no espaço da figura: yaw +X→+Y, pitch nariz pra cima, roll."""
    ex, ey, ez = vp.ypr_frame(yaw, pitch, roll)
    return Matrix((ex, ey, ez)).transposed()


# ===================================================================================================== a pose (o homem de 1,80 m)
PEL = Vector((0.0, 0.0, 0.92))
HIP_L, HIP_R = Vector((0.015, -0.095, 0.915)), Vector((-0.02, 0.095, 0.93))
ANK_L, ANK_R = Vector((0.31, -0.14, 0.215)), Vector((-0.28, 0.135, 0.12))   # o pé da frente na pedra; o de trás no chão, o calcanhar alto
KNEE_L = ik(HIP_L, ANK_L, 0.44, 0.43, (1.0, -0.15, 0.1))
KNEE_R = ik(HIP_R, ANK_R, 0.44, 0.43, (1.0, 0.1, 0.0))
SH_L, SH_R = Vector((0.055, -0.215, 1.445)), Vector((0.08, 0.212, 1.49))     # o direito sobe com o braço
EL_R = SH_R + nrm((0.25, 0.30, 0.92)) * 0.33
WR_R = EL_R + nrm((0.35, 0.16, 0.92)) * 0.27
FIST_R = WR_R + nrm((0.35, 0.16, 0.92)) * 0.065
EL_L = SH_L + nrm((-0.46, -0.32, -0.83)) * 0.33
WR_L = EL_L + nrm((-0.64, -0.18, -0.75)) * 0.27
FIST_L = WR_L + nrm((-0.64, -0.18, -0.75)) * 0.07
NECK0, NECK1 = Vector((0.075, 0.0, 1.49)), Vector((0.11, 0.008, 1.60))
HEAD = Vector((0.125, 0.012, 1.69))
HEAD_R = rot_ypr(9.0, 9.0, 0.0)       # a cabeça vira um pouco pro megafone e o queixo sobe (o olhar no horizonte)
TORSO = [  # (z, x do centro, meia-profundidade, meia-largura): o tronco no casaco, inclinado pra frente, o peito largo
    (0.84, -0.015, 0.112, 0.160), (0.92, -0.005, 0.13, 0.19), (1.00, 0.010, 0.126, 0.182), (1.07, 0.022, 0.122, 0.176),
    (1.15, 0.036, 0.132, 0.19), (1.24, 0.054, 0.148, 0.212), (1.33, 0.064, 0.152, 0.225), (1.41, 0.068, 0.138, 0.222),
    (1.465, 0.064, 0.11, 0.19), (1.505, 0.072, 0.072, 0.095)]


def spk_at():
    """O eixo do megafone (o cabo vertical no meio do punho direito), no mundo."""
    w = fig_w(FIST_R)
    return (round(w.x, 3), round(w.y, 3), round(w.z, 3))


SPK_AT = spk_at()


def figure():
    """A estátua (sem o megafone): os volumes do corpo, do casaco e do quepe, unidos e alisados. Devolve o objeto."""
    s = Sculpt()
    # --- a laje de bronze (o chão de pedra da estátua) e a pedra do pé da frente
    rr = []
    for z, sc in ((-0.10, 1.0), (-0.03, 1.0), (0.0, 0.94)):
        ring = []
        for k in range(28):
            a = 2 * math.pi * k / 28
            n = 1.0 + 0.06 * math.sin(3 * a + 0.7) + 0.04 * math.sin(7 * a + 2.1)
            ring.append((0.04 + 0.42 * sc * n * math.cos(a), 0.30 * sc * n * math.sin(a), z + 0.012 * math.sin(5 * a)))
        rr.append(ring)
    s.loft(rr)
    s.ell((0.33, -0.14, 0.045), (0.15, 0.12, 0.08), rot_ypr(18, 0, 4), seg=14, rings=8)
    s.ell((0.24, -0.06, 0.02), (0.09, 0.08, 0.04), rot_ypr(-30, 0, 0), seg=12, rings=6)
    # --- as pernas (a calça) e as botas
    for hip, knee, ank, side in ((HIP_L, KNEE_L, ANK_L, -1), (HIP_R, KNEE_R, ANK_R, 1)):
        s.limb(hip, knee, lambda t: (0.1 - 0.03 * t, 0.096 - 0.026 * t + 0.008 * math.sin(math.pi * t)), front=(1, 0, 0), n=8)
        s.ball(knee, 0.07)
        s.limb(knee, ank, lambda t: (0.068 - 0.012 * t + 0.008 * math.sin(math.pi * t * 0.8), 0.066 - 0.01 * t), front=(1, 0, 0), n=8)
        s.ball(ank, 0.058)
    # o cano das botas (um pouco mais grosso que a perna: a borda da bota)
    for knee, ank in ((KNEE_L, ANK_L), (KNEE_R, ANK_R)):
        top = ank + (knee - ank) * 0.42
        s.limb(ank, top, lambda t: (0.07, 0.068), front=(1, 0, 0), n=3)
    # os pés: o da frente chato na pedra, o de trás na ponta (o calcanhar alto)
    s.limb(ANK_L + Vector((-0.05, 0.0, -0.055)), ANK_L + Vector((0.2, -0.012, -0.075)), lambda t: (0.048 - 0.008 * t, 0.056 - 0.008 * t),
           front=(0, 0, 1), n=6)
    toe_r = Vector((ANK_R.x + 0.17, ANK_R.y + 0.01, 0.025))
    s.limb(ANK_R + Vector((-0.06, 0.0, -0.01)), toe_r, lambda t: (0.05 - 0.01 * t, 0.056 - 0.01 * t), front=(0.4, 0, 1), n=6)
    # --- o tronco (no casaco)
    rings = []
    for (z, cx, dx, wy) in TORSO:
        ring = []
        for k in range(24):
            a = 2 * math.pi * k / 24
            ca, sa = math.cos(a), math.sin(a)
            e = 2.4                                         # a superelipse: o peito largo e as costas chatas
            cu = math.copysign(abs(ca) ** (2 / e), ca)
            su = math.copysign(abs(sa) ** (2 / e), sa)
            ring.append((cx + dx * cu * (1.0 if ca > 0 else 0.88), wy * su, z))
        rings.append(ring)
    s.loft(rings)
    # o cinto (a correia larga com a fivela) e a bolsa da cartucheira
    zb = 1.07
    ring_o, ring_i = [], []
    for k in range(24):
        a = 2 * math.pi * k / 24
        ca, sa = math.cos(a), math.sin(a)
        cu = math.copysign(abs(ca) ** (2 / 2.4), ca)
        su = math.copysign(abs(sa) ** (2 / 2.4), sa)
        ring_o.append((0.022 + 0.132 * cu * (1.0 if ca > 0 else 0.9), 0.185 * su))
    s.loft([[(x, y, zb - 0.03) for (x, y) in ring_o], [(x, y, zb + 0.03) for (x, y) in ring_o]])
    s.ell((0.16, 0.0, zb), (0.018, 0.042, 0.036), seg=10, rings=6)
    for yy in (-0.11, 0.11):
        s.ell((0.11, yy, zb - 0.05), (0.035, 0.045, 0.05), rot_ypr(math.degrees(math.atan2(yy, 0.4)), 0, 0), seg=10, rings=6)
    # a gola do casaco (a lapela larga dobrada, aberta em V no peito) e os botões da frente
    for side in (-1, 1):
        R = rot_ypr(side * 18.0, -62.0, side * -12.0)
        s.ell((0.135, side * 0.06, 1.42), (0.11, 0.06, 0.016), R, seg=12, rings=6)
        s.ell((0.085, side * 0.125, 1.475), (0.07, 0.06, 0.022), rot_ypr(side * 70.0, -20.0, 0.0), seg=12, rings=6)
    s.ell((0.03, 0.0, 1.49), (0.05, 0.13, 0.045), rot_ypr(0, 10, 0), seg=14, rings=6)      # a gola atrás do pescoço
    for zz in (1.17, 1.25, 1.33):
        for yy in (-0.07, 0.07):
            s.ball((0.13 + 0.04 * (zz - 1.1), yy, zz), 0.017, seg=8)
    # o colete de cartucheiras do ELP no peito (a fileira de 4 bolsos e a alça de ombro): a marca do soldado chinês
    for k, yy in enumerate((-0.135, -0.045, 0.045, 0.135)):
        zc = 1.235 + 0.01 * (k % 2)
        x0 = 0.054 + 0.152 * math.cos(math.asin(min(0.95, yy / 0.235)))
        s.ell((x0, yy, zc), (0.03, 0.04, 0.05), rot_ypr(math.degrees(math.atan2(yy, 0.25)), 6, 0), seg=10, rings=6)
    s.ell((0.1, 0.0, 1.18), (0.05, 0.19, 0.018), rot_ypr(0, 4, 0), seg=14, rings=5)            # a cinta do colete
    for side in (-1, 1):
        a0, a1 = Vector((0.11, side * 0.15, 1.2)), Vector((0.05, side * 0.13, 1.47))
        s.limb(a0, a1, lambda t: (0.015, 0.03), front=(1, 0, 0), n=3)
    # o fuzil a tiracolo nas costas (a coronha no quadril esquerdo, o cano passando o ombro direito)
    b0, b1 = Vector((-0.17, -0.12, 0.98)), Vector((-0.13, 0.2, 1.62))
    s.limb(b0, b1, lambda t: (0.022 if t > 0.55 else 0.035, 0.016 if t > 0.55 else 0.022), front=(-1, 0, 0), n=5)
    s.ell(b0 + (b1 - b0) * 0.42 + Vector((-0.03, 0.0, 0.0)), (0.025, 0.03, 0.07), rot_ypr(0, 0, -30), seg=8, rings=5)
    s.limb(b1, b1 + nrm(b1 - b0) * 0.12, lambda t: (0.009, 0.009), front=(-1, 0, 0), n=2)
    # as dobras das mangas (o pano que junta no cotovelo e no punho)
    for (sh, el, wr) in ((SH_R, EL_R, WR_R), (SH_L, EL_L, WR_L)):
        for t, r_ in ((0.78, 0.07), (0.9, 0.066)):
            q = sh + (el - sh) * t
            s.ell(q, (r_, r_, 0.022), Matrix((nrm((el - sh).cross(Vector((0, 0, 1)))), nrm((el - sh).cross((el - sh).cross(Vector((0, 0, 1))))),
                                             nrm(el - sh))).transposed(), seg=12, rings=5)
        q = el + (wr - el) * 0.22
        s.ell(q, (0.06, 0.06, 0.02), Matrix((nrm((wr - el).cross(Vector((0, 0, 1)))), nrm((wr - el).cross((wr - el).cross(Vector((0, 0, 1))))),
                                            nrm(wr - el))).transposed(), seg=12, rings=5)
    # --- o casaco: a saia (a casca aberta na frente, alargando pra baixo, as pregas e o vento jogando pra trás)
    def skirt(u, v):
        al = math.radians(14.0 + 56.0 * v ** 1.1)           # a abertura da frente
        th = al + (2 * math.pi - 2 * al) * u
        rx = 0.135 + 0.22 * v ** 1.15
        ry = 0.186 + 0.17 * v ** 1.15
        fold = (1.0 + 0.12 * v ** 1.1 * math.sin(6 * th + 1.3 + 0.6 * math.sin(2 * th)) + 0.04 * v * math.sin(13 * th + 0.4)
                + 0.025 * v * math.sin(19 * th + 2.0))
        x = 0.02 + rx * math.cos(th) * fold * (1.0 if math.cos(th) > 0 else 0.92)
        y = ry * math.sin(th) * fold
        back = 0.5 - 0.5 * math.cos(th)                     # 0 na frente, 1 atrás
        x -= (0.08 + 0.26 * back) * v ** 1.5                # o vento (de frente) joga o casaco pra trás
        y += 0.05 * v ** 1.6 * math.sin(th) * back         # e abre as abas pros lados
        z = 1.04 - (0.66 - 0.1 * (1 - back) - 0.06 * back * math.sin(3 * th)) * v
        z += 0.07 * v ** 2 * back                           # a barra de trás levanta no vento
        return (x, y, z)
    s.shell(skirt, 44, 12, 0.032)
    # --- os braços (as mangas do casaco) e os punhos
    s.ball(SH_R, 0.09)
    s.ball(SH_L, 0.088)
    s.limb(SH_R, EL_R, lambda t: (0.076 - 0.016 * t, 0.08 - 0.02 * t), front=(1, -0.3, 0), n=8)
    s.ball(EL_R, 0.06)
    s.limb(EL_R, WR_R, lambda t: (0.064 - 0.014 * t + (0.007 if t > 0.8 else 0.0), 0.06 - 0.012 * t + (0.007 if t > 0.8 else 0.0)),
           front=(1, -0.5, 0), n=8)
    s.limb(SH_L, EL_L, lambda t: (0.075 - 0.016 * t, 0.078 - 0.018 * t), front=(1, 0, 0), n=8)
    s.ball(EL_L, 0.06)
    s.limb(EL_L, WR_L, lambda t: (0.062 - 0.013 * t + (0.007 if t > 0.8 else 0.0), 0.058 - 0.012 * t + (0.007 if t > 0.8 else 0.0)),
           front=(0, 0, 1), n=8)
    # os punhos fechados: o da direita em volta do cabo do megafone (o cabo é a peça que gira), o da esquerda cerrado
    dR = nrm(WR_R - EL_R)
    s.ell(FIST_R, (0.058, 0.054, 0.066), Matrix((nrm(Vector((1, -0.3, 0)) - dR * dR.dot(Vector((1, -0.3, 0)))),
                                               dR.cross(nrm(Vector((1, -0.3, 0)) - dR * dR.dot(Vector((1, -0.3, 0))))), dR)).transposed(),
          seg=14, rings=8)
    s.ell(FIST_R + Vector((0.035, -0.02, -0.01)), (0.022, 0.02, 0.035), seg=10, rings=6)      # o polegar por cima dos dedos
    dL = nrm(WR_L - EL_L)
    fL = nrm(Vector((0, 0, 1)) - dL * dL.z)
    s.ell(FIST_L, (0.058, 0.052, 0.064), Matrix((fL, dL.cross(fL), dL)).transposed(), seg=14, rings=8)
    s.ell(FIST_L + fL * 0.035, (0.02, 0.022, 0.03), seg=10, rings=6)
    # --- o pescoço, a cabeça (o crânio, a mandíbula, o queixo, o nariz, a sobrancelha, as maçãs, as orelhas) e o quepe
    s.limb(NECK0, NECK1, lambda t: (0.066, 0.064), front=(1, 0, 0), n=4)
    H, R = HEAD, HEAD_R

    def hp(x, y, z):
        return H + R @ Vector((x, y, z))
    s.ell(hp(0.0, 0.0, 0.01), (0.1, 0.08, 0.112), R, seg=18, rings=10)
    s.ell(hp(0.035, 0.0, -0.055), (0.072, 0.07, 0.058), R, seg=14, rings=8)          # a mandíbula larga
    s.ell(hp(0.078, 0.0, -0.09), (0.032, 0.038, 0.027), R, seg=12, rings=6)          # o queixo
    s.ell(hp(0.09, 0.0, -0.014), (0.018, 0.014, 0.027), R @ rot_ypr(0, -14, 0), seg=10, rings=6)   # o nariz
    s.ell(hp(0.083, 0.0, 0.03), (0.024, 0.066, 0.017), R, seg=12, rings=6)           # a sobrancelha (a testa franzida)
    for side in (-1, 1):
        s.ell(hp(0.07, side * 0.045, -0.02), (0.025, 0.026, 0.022), R, seg=10, rings=6)   # as maçãs
        s.ell(hp(-0.005, side * 0.079, -0.005), (0.026, 0.012, 0.033), R, seg=10, rings=6)  # as orelhas
    # o quepe mole do ELP: a copa achatada, a faixa, a pala curta (a estrela dourada é peça à parte)
    s.ell(hp(-0.005, 0.0, 0.085), (0.118, 0.098, 0.055), R @ rot_ypr(0, 6, 0), seg=18, rings=8)
    s.ell(hp(-0.004, 0.0, 0.058), (0.108, 0.091, 0.035), R, seg=18, rings=6)
    s.ell(hp(0.092, 0.0, 0.046), (0.045, 0.078, 0.009), R @ rot_ypr(0, -22, 0), seg=14, rings=5)
    # --- o lenço no pescoço, jogado pra trás pelo vento (a ponta em S)
    def scarf(u, v):
        x = -0.04 - 0.3 * u
        y = -0.1 - 0.06 * u + 0.025 * math.sin(math.pi * u * 2.0)
        z = 1.47 - 0.26 * u ** 1.2 + 0.03 * math.sin(math.pi * u * 1.6)
        w = 0.06 * (1 - 0.3 * u)
        return (x, y, z + (v - 0.5) * 2 * w)
    s.shell(scarf, 12, 3, 0.03)
    # o objeto, o remesh de voxel (a união), o alisado e a redução
    me = bpy.data.meshes.new('statue_tmp')
    s.b.to_mesh(me)
    s.b.free()
    o = bpy.data.objects.new('statue_tmp', me)
    bpy.context.scene.collection.objects.link(o)
    select_only(o)
    m = o.modifiers.new('rm', 'REMESH')
    m.mode = 'VOXEL'
    m.voxel_size = VOXEL
    m.adaptivity = 0.0
    bpy.ops.object.modifier_apply(modifier=m.name)
    m = o.modifiers.new('sm', 'SMOOTH')
    m.factor = 0.6
    m.iterations = 4
    bpy.ops.object.modifier_apply(modifier=m.name)
    n0 = sum(len(poly.vertices) - 2 for poly in o.data.polygons)
    m = o.modifiers.new('dc', 'DECIMATE')
    m.decimate_type = 'COLLAPSE'
    m.ratio = min(1.0, 52000.0 / max(n0, 1))
    bpy.ops.object.modifier_apply(modifier=m.name)
    print('STATUE', n0, '->', sum(len(poly.vertices) - 2 for poly in o.data.polygons), 'tris')
    return o


def put_object(p, key, o):
    """A malha de um objeto (no espaço do Blender) entra no Part como a bmesh do material 'key' (sem chanfro: a forma é lisa)."""
    b = bmesh.new()
    b.from_mesh(o.data)
    for f in b.faces:
        f.smooth = True
    p.bms[key] = b
    p.bevel[key] = 0.0
    bpy.data.objects.remove(o, do_unlink=True)


def verdigris(o, zmin, frac=0.07):
    """O verdete nas dobras: as faces do bronze da estátua (acima de zmin) mais côncavas (a média da curvatura de 2 anéis de vizinhos,
    com um ruído pra não sair liso) passam pro material do verdete. frac: a parte da estátua que fica verde."""
    me = o.data
    names = [m.name.split('.')[0] for m in me.materials]
    if 'CnMetal' not in names or 'GlaTile' not in names:
        return
    im, iv = names.index('CnMetal'), names.index('GlaTile')
    b = bmesh.new()
    b.from_mesh(me)
    b.verts.ensure_lookup_table()
    b.normal_update()
    cv = [0.0] * len(b.verts)
    for v in b.verts:
        if not v.link_edges:
            continue
        nb = [e.other_vert(v).co for e in v.link_edges]
        avg = sum(nb, Vector()) / len(nb)
        el = sum((q - v.co).length for q in nb) / len(nb)
        cv[v.index] = (avg - v.co).dot(v.normal) / max(el, 1e-6)
    for _ in range(2):
        cv2 = cv[:]
        for v in b.verts:
            if v.link_edges:
                cv2[v.index] = 0.5 * cv[v.index] + 0.5 * sum(cv[e.other_vert(v).index] for e in v.link_edges) / len(v.link_edges)
        cv = cv2
    fx, fy = FIG[0], -FIG[1]                               # (o Blender tem o Y ao contrário)
    fs = [f for f in b.faces if f.material_index == im and f.calc_center_median().z > zmin
          and abs(f.calc_center_median().x - fx) < 3.6 and abs(f.calc_center_median().y - fy) < 3.6]
    val = []
    for f in fs:
        c = f.calc_center_median()
        nz = math.sin(c.x * 3.1 + c.z * 1.7) * math.sin(c.y * 2.3 - c.z * 2.9)
        down = max(0.0, -f.normal.z) * 0.05                  # o que olha pra baixo (debaixo das abas) segura a umidade
        val.append(sum(cv[v.index] for v in f.verts) / len(f.verts) + 0.02 * nz + down)
    if not val:
        return
    thr = sorted(val)[int(len(val) * (1.0 - frac))]
    n = 0
    for f, x in zip(fs, val):
        if x >= thr:
            f.material_index = iv
            n += 1
    b.to_mesh(me)
    b.free()
    print('VERDIGRIS', n, 'de', len(fs), 'faces da estátua')


# ===================================================================================================== o pedestal
def stepped(p):
    """Os 3 degraus de granito cinza em volta (a pedra com o chanfro, o espelho com o friso escuro de sombra)."""
    z0 = -0.12
    for k, (hx, hy, zt) in enumerate(STEPS):
        p.box('CnPlinth#step', PX - hx, PX + hx, -hy, hy, z0 if k == 0 else zt - 0.25, zt, bevel=0.04)


def studio(p):
    """O bloco do estúdio: a parede de granito vermelho em talude (as fiadas de pedra desencontradas; o chanfro largo de cada pedra
    desenha a junta), o rodapé e a cornija de labradorito, a porta de bronze no batente fundo, as frestas acesas, o plinto de cima com a
    estrela do ELP e o friso dourado."""
    # o rodapé escuro (labradorito polido)
    p.box('Dark#plinth', SX0 - 0.12, SX1 + 0.12, -SY - 0.12, SY + 0.12, Z0 - 0.02, Z0 + 0.42, bevel=0.03)
    door = (-1.0, 1.0, Z0 + 0.42, Z0 + 2.75)
    slits_x = [-2.75, -2.15, 2.15, 2.75]
    slits_back = [-1.6, 0.0, 1.6]
    slits_y = [PX + d for d in (-2.4, -1.2, 0.0, 1.2, 2.4)]
    courses = splits(Z0 + 0.42, ZS, 0.55)
    zl0, zl1 = courses[1], courses[4]                     # as frestas ocupam 3 fiadas inteiras
    for k, (za, zb) in enumerate(zip(courses[:-1], courses[1:])):
        ins, ins2 = LEAN * (za - Z0), LEAN * (zb - Z0)
        in_slit = za < zl1 - 0.01 and zb > zl0 + 0.01
        for face in ('+x', '-x', '+y', '-y'):
            if face == '+x':
                holes = [(y - 0.16, y + 0.16) for y in slits_x] if in_slit else []
                if za < door[3]:
                    holes.append((door[0] - 0.35, door[1] + 0.35))
                lo, hi = -SY + ins, SY - ins
            elif face == '-x':
                holes = [(y - 0.16, y + 0.16) for y in slits_back] if in_slit else []
                lo, hi = -SY + ins, SY - ins
            else:
                holes = [(x - 0.16, x + 0.16) for x in slits_y] if in_slit else []
                lo, hi = SX0 + 0.4, SX1 - 0.4
            # as juntas verticais (a pedra de ~1,5 m, desencontrada a cada fiada) e os vãos
            joints = [u + (0.75 if k % 2 else 0.0) for u in splits(lo, hi, 1.5)[1:-1]]
            cuts = sorted(set([lo, hi] + [u for h in holes for u in h] + [u for u in joints if lo + 0.3 < u < hi - 0.3]))
            for a, b_ in zip(cuts[:-1], cuts[1:]):
                mid = (a + b_) / 2
                if b_ - a < 0.05 or any(h0 - 1e-6 <= mid <= h1 + 1e-6 for (h0, h1) in holes):
                    continue
                if face == '+x':
                    hexa_q(p, 'Red#course', SX1 - ins, SX1 - ins2, SX1 - 0.4, a, b_, za, zb, 'x')
                elif face == '-x':
                    hexa_q(p, 'Red#course', SX0 + ins, SX0 + ins2, SX0 + 0.4, a, b_, za, zb, 'x')
                elif face == '+y':
                    hexa_q(p, 'Red#course', SY - ins, SY - ins2, SY - 0.4, a, b_, za, zb, 'y')
                else:
                    hexa_q(p, 'Red#course', -SY + ins, -SY + ins2, -SY + 0.4, a, b_, za, zb, 'y')
    # as frestas acesas (o fundo de luz recuado, o vidro, o peitoril e a verga de granito claro)
    for face, us_, at in (('+x', slits_x, SX1), ('-x', slits_back, SX0), ('+y', slits_y, SY), ('-y', slits_y, -SY)):
        for u in us_:
            slit(p, face, at, u, zl0, zl1)
    # o fundo do vão da porta (o miolo escuro atrás das folhas)
    x = SX1 - LEAN * 1.2
    p.box('Dark#reveal', x - 0.62, x - 0.5, door[0] - 0.35, door[1] + 0.35, door[2], ZS, bevel=0.0)
    # a porta de bronze: o batente fundo de granito claro, as 2 folhas almofadadas, a bandeira de luz em cima
    p.box('CnPlinth#jamb', x - 0.55, x + 0.12, door[0] - 0.42, door[0] - 0.12, Z0 + 0.42, door[3] + 0.32, bevel=0.03)
    p.box('CnPlinth#jamb', x - 0.55, x + 0.12, door[1] + 0.12, door[1] + 0.42, Z0 + 0.42, door[3] + 0.32, bevel=0.03)
    p.box('CnPlinth#jamb', x - 0.55, x + 0.14, door[0] - 0.42, door[1] + 0.42, door[3] + 0.32, ZS + 0.02, bevel=0.03)
    p.box('CnPlinth#jamb', x - 0.55, x - 0.1, door[0] - 0.13, door[1] + 0.13, door[3] - 0.02, door[3] + 0.33, bevel=0.0)
    xd = x - 0.36
    for (y0, y1) in ((door[0], -0.02), (0.02, door[1])):
        p.box('CnMetal#door', xd - 0.08, xd, y0, y1, Z0 + 0.42, door[3] - 0.45, bevel=0.03)
        for (za, zb) in ((Z0 + 0.6, Z0 + 1.3), (Z0 + 1.45, door[3] - 0.62)):
            p.box('CnMetal#panel', xd, xd + 0.035, y0 + 0.12, y1 - 0.12, za, zb, bevel=0.012)
        p.cyl('CnGold#knob', (xd + 0.05, (y1 - 0.1) if y1 < 0 else (y0 + 0.1), Z0 + 1.3), 'x', 0.04, 0.05, seg=10, bevel=0.0)
    p.box('Light#fan', xd - 0.12, xd - 0.06, door[0], door[1], door[3] - 0.44, door[3] - 0.02, bevel=0.0)
    for y in splits(door[0] + 0.2, door[1] - 0.2, 0.3):
        p.box('CnMetal#grille', xd - 0.06, xd - 0.01, y - 0.02, y + 0.02, door[3] - 0.44, door[3] - 0.02, bevel=0.0)
    # a cornija de labradorito (2 frisos com a pingadeira) e o friso dourado embaixo dela
    ins = LEAN * (ZS - Z0)
    p.box('Dark#corn', SX0 + ins - 0.18, SX1 - ins + 0.18, -SY + ins - 0.18, SY - ins + 0.18, ZS - 0.02, ZS + 0.18, bevel=0.03)
    p.box('CnPlinth#corn', SX0 + ins - 0.3, SX1 - ins + 0.3, -SY + ins - 0.3, SY - ins + 0.3, ZS + 0.18, ZC, bevel=0.04)
    for (a, b_, c, d) in ((SX1 - ins, SX1 - ins + 0.03, -SY + ins, SY - ins), (SX0 + ins - 0.03, SX0 + ins, -SY + ins, SY - ins),
                          (SX0 + ins, SX1 - ins, SY - ins, SY - ins + 0.03), (SX0 + ins, SX1 - ins, -SY + ins - 0.03, -SY + ins)):
        p.box('CnGold#frieze', a, b_, c, d, ZS - 0.16, ZS - 0.08, bevel=0.0)
    # o plinto de cima (o granito vermelho, a capa escura) com a estrela do ELP na frente
    p.box('Red#top', TX0, TX1, -TY, TY, ZC - 0.02, ZT - 0.12, bevel=0.06)
    p.box('CnPlinth#cap', TX0 - 0.12, TX1 + 0.12, -TY - 0.12, TY + 0.12, ZT - 0.12, ZT, bevel=0.03)
    star_wall(p, (TX1, 0.0, (ZC + ZT) / 2 - 0.06), '+x', 0.25, t=0.02)
    # as escorridas de verdete que o bronze deixa no granito (do pé da estátua, pela capa e pelo plinto)
    for k, (y, L) in enumerate(((-1.6, 0.5), (-0.6, 0.38), (0.9, 0.55), (1.9, 0.32))):
        p.box('GlaTile#drip', TX1 + 0.12, TX1 + 0.13, y - 0.05, y + 0.05, ZT - min(L, 0.12), ZT, bevel=0.0)
        p.box('GlaTile#drip', TX1 + 0.005, TX1 + 0.015, y - 0.035, y + 0.035, ZT - 0.12 - L * 0.7, ZT - 0.12, bevel=0.0)


def hexa_q(p, m, n0, n1, nin, a, b_, za, zb, face):
    """A fiada de pedra da parede em talude: a face de fora vai de n0 (em za) a n1 (em zb), a de dentro fica em nin."""
    from model_building_parts import hexa
    if face == 'x':
        P = [(n0, a, za), (n0, b_, za), (nin, a, za), (nin, b_, za), (n1, a, zb), (n1, b_, zb), (nin, a, zb), (nin, b_, zb)]
    else:
        P = [(a, n0, za), (b_, n0, za), (a, nin, za), (b_, nin, za), (a, n1, zb), (b_, n1, zb), (a, nin, zb), (b_, nin, zb)]
    hexa(p, m, P, 0.03)


def slit(p, face, at, u, z0, z1):
    """A fresta acesa: o fundo de luz recuado 0,28 m, o vidro, o peitoril e a verga salientes de granito claro."""
    s = 1 if face[0] == '+' else -1
    ax = face[1]
    ins0 = LEAN * (z0 - Z0)

    def bx(m, n0, n1, u0, u1, za, zb, bevel=0.0):
        a0, a1 = sorted((at - s * (ins0 - n0), at - s * (ins0 - n1)))
        if ax == 'x':
            p.box(m, a0, a1, u0, u1, za, zb, bevel=bevel)
        else:
            p.box(m, u0, u1, a0, a1, za, zb, bevel=bevel)
    bx('Light#slit', -0.36, -0.3, u - 0.17, u + 0.17, z0 - 0.02, z1 + 0.02)
    for k in range(1, 4):
        z = z0 + (z1 - z0) * k / 4
        bx('CnMetal#mull', -0.26, -0.22, u - 0.17, u + 0.17, z - 0.02, z + 0.02)
    bx('CnPlinth#sill', -0.3, 0.1, u - 0.26, u + 0.26, z0 - 0.12, z0, bevel=0.02)
    bx('CnPlinth#sill', -0.3, 0.06, u - 0.24, u + 0.24, z1, z1 + 0.14, bevel=0.02)


# ===================================================================================================== a praça
def plaza(p):
    """O caminho de lajes da frente até os degraus (só onde se pisa), as bandeiras em fila dos 2 lados, os 4 postes de corneta com os
    fios até a cornija, os 4 frades de granito na boca do caminho."""
    x0, x1, w = PX + STEPS[0][0] - 0.05, HX - 0.1, 2.1
    for k, (a, b_) in enumerate(zip(splits(x0, x1, 1.1)[:-1], splits(x0, x1, 1.1)[1:])):
        for (c, d) in ((-w, -0.02), (0.02, w)):
            off = 0.3 if k % 2 else 0.0
            p.box('CnPlinth#slab', a + 0.02, b_ - 0.02, c + 0.02, d - 0.02, -0.08, 0.06 + 0.01 * _hash(k, int(c * 10), 3), bevel=0.03)
    p.box('Dark#kerb', x0, x1, -w - 0.18, -w, -0.08, 0.1, bevel=0.03)
    p.box('Dark#kerb', x0, x1, w, w + 0.18, -0.08, 0.1, bevel=0.03)
    # as bandeiras em fila: os mastros de aço com a base de granito, o pano vermelho (a cor do jogador) no vento
    k = 0
    for side in (-1, 1):
        for x in (PX + 5.0, PX + 6.1, PX + 7.2, PX + 8.2):
            y = side * (w + 0.55)
            p.box('CnPlinth#flagbase', x - 0.28, x + 0.28, y - 0.28, y + 0.28, -0.05, 0.32, bevel=0.04)
            p.cyl('CnMetal#mast', (x, y, 2.85), 'z', 0.05, 5.4, seg=10, bevel=0.0)
            p.cyl('CnGold#finial', (x, y, 5.65), 'z', 0.08, 0.2, seg=10, r2=0.015, bevel=0.0)
            flag(p, (x, y), 5.45, 1.6, 1.05, m='Player', phase=0.45 * k)
            k += 1
    # as bandeiras dos lados do pedestal (as grandes, 9 m)
    for side in (-1, 1):
        x, y = PX + 1.2, side * 6.2
        p.box('CnPlinth#flagbase', x - 0.4, x + 0.4, y - 0.4, y + 0.4, -0.05, 0.45, bevel=0.05)
        p.cyl('CnMetal#mast', (x, y, 4.9), 'z', 0.07, 9.0, seg=10, bevel=0.0)
        p.cyl('CnGold#finial', (x, y, 9.5), 'z', 0.12, 0.3, seg=10, r2=0.02, bevel=0.0)
        flag(p, (x, y), 9.2, 2.8, 1.8, m='Player', phase=1.1 + side)
    # os postes de corneta (o concreto afinando, a cruzeta, as 4 cornetas pra fora, a caixa do amplificador) e os fios
    for (x, y) in ((-5.6, -9.6), (-5.6, 9.6), (5.4, -9.8), (5.4, 9.8)):
        p.cyl('CnConcrete#pole', (x, y, 3.6), 'z', 0.16, 7.4, seg=12, r2=0.11, bevel=0.0)
        p.box('CnConcrete#poleft', x - 0.35, x + 0.35, y - 0.35, y + 0.35, -0.05, 0.25, bevel=0.04)
        p.box('CnMetal#arm', x - 0.06, x + 0.06, y - 0.7, y + 0.7, 6.55, 6.67, bevel=0.0)
        speaker_cluster(p, (x, y), 6.95, n=4, r_horn=0.42, length=1.0, tilt=-14.0, m='Paint#horn', yaw0=45.0)
        p.box('CnMetal#amp', x + 0.16, x + 0.36, y - 0.22, y + 0.22, 3.1, 3.6, bevel=0.02)
        tgt = (PX + (SX1 - PX) * 0.92 * (1 if x > PX else -1), 0.92 * SY * (1 if y > 0 else -1), ZS + 0.1)
        tube(p, 'Dark#cable', catenary((x, y, 6.6), tgt, 0.45, n=14), 0.022, seg=6)
        p.cyl('Dark#ins', (x, y, 6.62), 'z', 0.05, 0.12, seg=8, bevel=0.0)


# ===================================================================================================== o megafone (a peça que gira)
def speakers():
    """O megafone gigante (5,6× um de verdade: 1,7 m de corpo, a campânula de 1,3 m): o cabo de pistola vertical (o eixo do giro,
    dentro do punho), o corpo do driver com as aletas, o cone e a campânula com a borda enrolada, o gatilho, a alça de cima. Bronze, a
    boca escura por dentro, o friso dourado. Feito na origem (o meio do punho), apontando pra frente-direita e 18° pra cima."""
    p = BPart(PREFIX + '_Speakers', cull=False)
    k = 5.6
    yaw, pitch = 22.0, 18.0
    A = Vector(vp.ypr_frame(yaw, pitch, 0.0)[0])
    up = Vector(vp.ypr_frame(yaw, pitch, 0.0)[2])
    # o cabo (o eixo do giro): da base do punho até o corpo
    p.cyl('CnMetal#grip', (0.0, 0.0, 0.02 * k), 'z', 0.018 * k, 0.15 * k, seg=14, bevel=0.0)
    p.cyl('CnMetal#grip', (0.0, 0.0, -0.055 * k), 'z', 0.022 * k, 0.012 * k, seg=14, bevel=0.0)
    C0 = Vector((0.0, 0.0, 0.11 * k)) - A * 0.06 * k        # o corpo: a traseira do driver um pouco atrás do cabo
    prof = [(0.0, 0.0), (0.03, 0.0), (0.042, 0.01), (0.044, 0.07), (0.04, 0.08), (0.03, 0.09), (0.034, 0.12), (0.05, 0.17),
            (0.075, 0.23), (0.098, 0.275), (0.112, 0.295), (0.116, 0.305)]
    vp.lathe(p, 'CnMetal#body', tuple(C0), tuple(A), [(r * k, a * k) for (r, a) in prof], seg=28)
    # a borda enrolada da campânula e a boca escura (o fundo do cone)
    vp.lathe(p, 'CnMetal#lip', tuple(C0 + A * 0.302 * k), tuple(A), [(0.108 * k, 0.0), (0.122 * k, 0.004 * k), (0.12 * k, 0.012 * k),
                                                                     (0.106 * k, 0.008 * k)], seg=28, closed=True)
    vp.lathe(p, 'Dark#mouth', tuple(C0 + A * 0.16 * k), tuple(A), [(0.0, 0.0), (0.04 * k, 0.0), (0.1 * k, 0.135 * k), (0.0, 0.135 * k)],
             seg=24)
    # o friso dourado no driver e as aletas
    vp.lathe(p, 'CnGold#band', tuple(C0 + A * 0.03 * k), tuple(A), [(0.044 * k, 0.0), (0.047 * k, 0.0), (0.047 * k, 0.012 * k),
                                                                    (0.044 * k, 0.012 * k)], seg=28, closed=True)
    for j in range(3):
        a = 0.045 + 0.012 * j
        vp.lathe(p, 'CnMetal#fin', tuple(C0 + A * a * k), tuple(A), [(0.043 * k, 0.0), (0.05 * k, 0.0), (0.05 * k, 0.005 * k),
                                                                    (0.043 * k, 0.005 * k)], seg=28, closed=True)
    # a alça de cima (o arco do corpo) e o gatilho na frente do cabo
    q0 = C0 + A * 0.03 * k + up * 0.045 * k
    q1 = C0 + A * 0.13 * k + up * 0.04 * k
    vp.tube(p, 'CnMetal#handle', [tuple(q0), tuple((q0 + q1) / 2 + up * 0.04 * k), tuple(q1)], 0.008 * k, seg=8)
    p.obox('CnMetal#trig', (0.022 * k * A.x, 0.022 * k * A.y, 0.07 * k), (0.012 * k, 0.008 * k, 0.03 * k), yaw=yaw, bevel=0.0)
    # a estrela dourada na campânula (de cima e da câmera)
    c = C0 + A * 0.2 * k + up * 0.088 * k
    p.obox('CnGold#star', tuple(c), (0.04 * k, 0.04 * k, 0.006 * k), yaw=yaw + 45.0, pitch=pitch + 33.0, bevel=0.0)
    o = p.finish()
    return o


# ===================================================================================================== o prédio
def cap_star(p):
    """A estrela dourada na frente do quepe (fora do remesh: o dourado é outro material)."""
    H, R = HEAD, HEAD_R
    c = fig_w(H + R @ Vector((0.106, 0.0, 0.07)))
    yaw = FIG_YAW + 9.0
    p.obox('CnGold#capstar', tuple(c), (0.03, 0.12, 0.12), yaw=yaw, pitch=-10.0, roll=45.0, bevel=0.0)


def base():
    p = BPart(PREFIX + '_Base')
    stepped(p)
    studio(p)
    plaza(p)
    cap_star(p)
    put_object(p, 'CnMetal#statue', figure())
    o = p.finish()
    verdigris(o, ZT + 0.05)
    cnt = {}
    for poly in o.data.polygons:
        n = o.data.materials[poly.material_index].name
        cnt[n] = cnt.get(n, 0) + len(poly.vertices) - 2
    print('TRIS', o.name, sum(cnt.values()), sorted(cnt.items(), key=lambda kv: -kv[1]))
    return o


def main():
    clear_scene()
    b = base()
    total = export(b, OUT)
    s = speakers()
    bake_in_place(s, SPK_AT)
    total += export(s, OUT)
    sc = scaffold(PREFIX + '_Scaffold', PX - STEPS[0][0], PX + STEPS[0][0], -STEPS[0][1], STEPS[0][1], 14.5, post='CnMetal',
                  plank='Crate', stripe='Hazard')
    total += export(sc, OUT)
    print('MODEL total', total, 'tris (antes do detalhe)')
    top = fig_w(HEAD + HEAD_R @ Vector((0, 0, 0.14)))
    print('MODEL lote 14 x 24 m; speakers spin at_m', list(SPK_AT), '; topo da cabeça', round(top.z, 2), 'm; punho', round(SPK_AT[2], 2))
    print('MODEL fire_points_m', [[PX + 1.2, -2.2, ZS], [PX - 2.4, 2.4, ZS], [round(SPK_AT[0], 2), round(SPK_AT[1], 2), round(SPK_AT[2] - 4.0, 2)]])
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]], [PREFIX + '_Speakers', [SPK_AT[0], -SPK_AT[1], SPK_AT[2]]]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1100, 1600]],
                      ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1600, 1000]],
                      ['side', 'ortho', [0, 1, 0], [0, 0, 1], [1200, 1000]],
                      ['three_quarter', 'persp', 24.0, 215.0, [1600, 1000], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 22.0, 35.0, [1600, 1000], 30.0, 0.95],
                      ['statue_close', 'persp', 8.0, 200.0, [1000, 1400], 22.0, 0.62]],
            'usa_at': [11, -15, 0], 'tank_views': False,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, indent=1)


if __name__ == '__main__':
    main()
