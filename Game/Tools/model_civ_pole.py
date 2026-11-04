"""O poste de madeira da rede, v3 (kit civil v3, 03/11/2026; docs/KIT_CIVIL_V2.md §2.3, docs/elenco/TAREFAS.md item 64) e o mesmo com o
transformador.

Rodar (Blender sem janela, só CPU), com o detalhe AAA e a máscara em todos os materiais:
  RTS_VC_FIX=1 "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_civ_pole.py C:/Users/luisf/explosion/captures/game/models/civ_v3/pole

O PEDIDO (o Felipe, 03/11, sobre o kit civil): "parece jogo de lego, muitas quinas, sem detalhes" e "as torres de energia ali, os
postes e tal". O v2 era um cilindro perfeito de 14 lados, reto como um cano, com os riscos de rachadura COLADOS por fora, a cruzeta uma
caixa atravessando o eixo do poste e o isolador uma peça lisa: lia "poste de brinquedo". O que mudou no v3:
- o TRONCO de tora de verdade: a seção irregular (oval, com os veios), afina pra cima, a CURVATURA leve (a tora não é reta) e o poste
  LEVEMENTE TORTO (~1,3° pra trás, ao longo da linha do fio); as RACHADURAS DE SOL ENTALHADAS (a fenda funda e escura, 1–2,5 cm, que
  abre no meio e fecha nas pontas), o pé escuro do creosoto com a borda irregular, a terra molhada no pé e o tronco que afina na linha
  do chão (a podridão de onde a tora entra na terra), o topo cortado em bisel com o topo de madeira escura;
- a CRUZETA NA FACE do tronco (como no poste de verdade: a cruzeta é presa de lado, não atravessa a tora), gasta: as quinas comidas
  e desiguais, a fenda comprida na frente, um lascado na quina de baixo, as pontas chanfradas, a barriga leve; a chapa de apoio
  galvanizada entre a cruzeta e a tora, o parafuso passante com a arruela quadrada e a porca;
- as mãos-francesas em V de ferro chato com as pontas redondas, o tirefão no tronco;
- os isoladores de pino de VIDRO com 2 saias, o pescoço do fio (o sulco onde o jogo amarra o fio, a 7,57 m) e a coroa; o pino de aço
  com a porca embaixo da cruzeta;
- os degraus de ferro (o grampo com a ponta virada pra cima, a arruela quadrada e a porca) em zigue-zague nas faces ±X;
- o fio de terra descendo pela face de trás com a proteção de chapa no pé; a plaqueta de número; o resto de um cartaz colado.
O transformador (SM_civ_pole_transformer): o tanque cinza (Metal) pendurado na face da frente, embaixo da cruzeta (os frisos, a tampa, as 2
baterias de aletas, as 2 buchas de alta na tampa, as 3 de baixa na frente, a plaqueta), os 2 suportes aparafusados na tora, a cruzeta
de baixo com as 2 CHAVES-FUSÍVEL (a porcelana inclinada com o tubo do fusível) e os 2 para-raios, os jumpers do isolador à chave e da
chave à bucha, os cabos de baixa saindo em laço pro eletroduto que desce pela tora.

AS MEDIDAS QUE O JOGO USA NÃO MUDARAM (look_houses.py pole(), docs/APROVADOS.md 17/10; WorldForge WorldDetail.h PoleInsulator(): o
RTSWireSystem liga o fio no isolador): 8,0 m de altura, a cruzeta ao longo de X a 7,25–7,40 m, os 4 isoladores em x = ±0,35 e ±0,95
(y = 0) com o topo a 7,60 m (o fio sai a 7,572 m, no pescoço), o pé de 0,26–0,27 m, a origem no pé, a pegada 0,4 × 0,4 m. A tora
inclina pra +Y (o centro dela fica a ~0,16 m atrás da cruzeta, que está presa na face −Y): o pé continua no centro da pegada.
Nada de ponta de fio modelada. Saída: SM_civ_pole.glb (SUBSTITUI o civ_pole) e SM_civ_pole_transformer.glb (novo, 1 a cada ~5 postes).
REGRA DO GERADOR (Felipe, 13/10): poste nunca em cima de rua nem de outra peça.
Materiais (os slots do kit civil, CIVIL_SLOTS): Wood (a tora, a cruzeta), Rust (o creosoto do pé, o topo), Dark (a terra, as fendas,
os cabos), Metal (as ferragens), Lens (o vidro dos isoladores e das buchas), Cloth (o cartaz).

O model_civ_pole_concrete.py importa daqui as medidas e o isolador (o poste de concreto é trocável com este).
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import Part, clear_scene, export, V  # noqa: E402
import model_civ_parts as cp  # noqa: E402
from model_building_parts import bake_in_place  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
from model_lib import COLORS  # noqa: E402

# (04/11) a madeira velha do poste (o slot WoodOld do jogo): cinza-marrom, gasta de sol, como nos game shots
COLORS.setdefault('WoodOld', (92, 82, 70))

# ----------------------------------------------------------------------------------------------- as medidas do jogo (não mudar)
ARM_Z0, ARM_Z1 = 7.25, 7.40
ARM_ZC = (ARM_Z0 + ARM_Z1) / 2
INS_X = (-0.95, -0.35, 0.35, 0.95)
INS_TOP = 7.60
WIRE_Z = 7.572
TOP = 8.0
# ----------------------------------------------------------------------------------------------- o desenho
ARM_HALF = 1.10            # a cruzeta de 2,2 m
ARM_W = 0.115              # a largura (Y) da cruzeta
LEAN_Y = 0.157             # o centro da tora na altura da cruzeta (a tora atrás, a cruzeta na face −Y)
N_AROUND = 28
CHECKS = [                 # as rachaduras de sol: (índice do vão entre os pontos da volta, z0, z1, a fundura no meio)
    (2, 1.25, 6.4, 0.024), (10, 2.1, 4.7, 0.016), (17, 4.6, 7.05, 0.017), (22, 0.85, 2.7, 0.014), (6, 5.4, 7.85, 0.013)]
A_CUT = math.pi / 2        # o bisel do topo desce pra trás (+Y)


# ===================================================================================================== a tora
def axis_c(z):
    """O centro da tora na altura z (Unreal): o pé no centro da pegada, a inclinação pra +Y até a cruzeta, a curva leve em X."""
    zz = min(max(z, 0.0), TOP)
    t = zz / ARM_ZC
    x = 0.024 * t + 0.03 * math.sin(math.pi * zz / 8.2) + 0.01 * (math.sin(2 * math.pi * zz / 8.2 + 0.7) - math.sin(0.7))
    y = LEAN_Y * t ** 1.15 + 0.014 * math.sin(math.pi * zz / 8.2)
    return x, y


def r_base(z):
    zz = min(max(z, 0.0), TOP)
    r = 0.134 - 0.0031 * zz
    return r - 0.007 * math.exp(-((z - 0.16) / 0.12) ** 2)      # a linha do chão comida (a podridão do pé)


def r_surf(a, z):
    """O raio da superfície no ângulo a (0 = +X, π/2 = +Y) e na altura z: a seção irregular e os veios (sem as fendas)."""
    r = r_base(z)
    n = cp.vnoise3(math.cos(a) * 1.4 + 3.0, math.sin(a) * 1.4 + 5.0, z * 0.55, 11)
    r *= 1.0 + 0.06 * (n - 0.5)
    return r + 0.0016 * math.sin(9 * a + 1.7 * math.sin(z * 0.8)) + 0.001 * math.sin(17 * a + z * 1.3)


def surf(a, z, off=0.0):
    """O ponto na superfície da tora (com 'off' pra fora), Unreal."""
    cx, cy = axis_c(z)
    r = r_surf(a, z) + off
    return Vector((cx + r * math.cos(a), cy + r * math.sin(a), z))


def radial(a):
    return Vector((math.cos(a), math.sin(a), 0.0))


def _faces_mat(p, rings, matfn, caps=None, smooth=True):
    """O loft de anéis fechados com o material por face (matfn(i, j) → chave), os vértices divididos por material (a emenda lê como a
    borda da mancha). caps = (chave de baixo, chave de cima) ou None. Cada material vira uma superfície aberta: quem usa confere a
    orientação no fim (fix_orientation)."""
    cache = {}
    n = len(rings[0])

    def vert(m, i, j):
        key = (m, i, j)
        if key not in cache:
            cache[key] = p.bm(m, 0.0).verts.new(V(*rings[i][j]))
        return cache[key]
    for i in range(len(rings) - 1):
        for j in range(n):
            jj = (j + 1) % n
            m = matfn(i, j)
            f = p.bm(m, 0.0).faces.new([vert(m, i, j), vert(m, i, jj), vert(m, i + 1, jj), vert(m, i + 1, j)])
            f.smooth = smooth
    if caps:
        for k, idx in ((0, 0), (1, len(rings) - 1)):
            m = caps[k]
            vs = [vert(m, idx, j) for j in range(n)]
            p.bm(m, 0.0).faces.new(vs if k else list(reversed(vs)))


def trunk(p):
    """A tora: os anéis com a seção irregular, as fendas entalhadas, o pé escuro, a linha do chão comida e o topo em bisel."""
    step = 2 * math.pi / N_AROUND
    angs, roles = [], []
    for j in range(N_AROUND):
        angs.append(j * step)
        roles.append(None)
        for k, (jj, z0, z1, d) in enumerate(CHECKS):
            if jj == j:
                ac = (j + 0.5) * step
                dl = 0.085
                angs += [ac - dl, ac, ac + dl]
                roles += [('lip', k), ('deep', k), ('lip', k)]
    zs = [-0.15, 0.0, 0.08, 0.16, 0.26, 0.38, 0.5, 0.63, 0.76, 0.88]
    z = 0.97
    while z < 1.4:
        zs.append(z)
        z += 0.09
    z = 1.5
    while z < 7.8:
        zs.append(round(z, 3))
        z += 0.27
    zs += [7.82, 7.88]

    def depth(k, z):
        jj, z0, z1, d = CHECKS[k]
        if not (z0 < z < z1):
            return 0.0
        u = (z - z0) / (z1 - z0)
        return d * max(0.0, math.sin(math.pi * u)) ** 0.55 * (0.8 + 0.4 * cp.vnoise3(z * 1.7, k * 3.1, 0.0, 5))

    rings = []
    for z in zs:
        ring = []
        for a, role in zip(angs, roles):
            q = surf(a, z)
            if role:
                d = depth(role[1], z) * (1.0 if role[0] == 'deep' else 0.15)
                q -= radial(a) * d
            ring.append(tuple(q))
        rings.append(ring)
    # o topo em bisel (desce pra trás, o ponto mais alto a 8,0 m): o anel 2 cm abaixo e a borda arredondada do corte
    for dz, dr in ((-0.02, -0.004), (0.0, -0.016)):
        ring = []
        for a in angs:
            zt = TOP - 0.045 * (1 + math.cos(a - A_CUT)) + dz
            cx, cy = axis_c(zt)
            r = r_surf(a, zt) + dr
            ring.append((cx + r * math.cos(a), cy + r * math.sin(a), zt))
        rings.append(ring)

    nring = len(rings)

    def soil(a):
        return 0.2 + 0.05 * math.sin(5 * a + 1.0)

    def creo(a):
        return 1.12 + 0.13 * math.sin(3 * a + 0.4) + 0.06 * math.sin(7 * a + 2.0)

    def matfn(i, j):
        jj = (j + 1) % len(angs)
        ra, rb = roles[j], roles[jj]
        zc = (rings[i][j][2] + rings[i + 1][j][2]) / 2
        if i >= nring - 3:
            return 'Rust#end' if i == nring - 2 else 'WoodOld#pole'
        if ra and rb and ra[1] == rb[1] and 'deep' in (ra[0], rb[0]):
            k = ra[1]
            if min(depth(k, rings[i][j][2]), depth(k, rings[i + 1][j][2])) > 0.003:
                return 'Dark#check'
        a = angs[j] + 0.5 * step / 2
        if zc < soil(a):
            return 'Dark#soil'
        if zc < creo(a):
            return 'Rust#creo'
        return 'WoodOld#pole'
    _faces_mat(p, rings, matfn, caps=('Dark#soil', 'Rust#end'))


def fix_orientation(o):
    """(03/11) A peça pronta com as emendas de material costuradas e cada ilha fechada virada pra fora: o finish() do Part reorienta
    cada material sozinho, e a superfície aberta (a lasca, a fenda) podia sair do avesso (o jogo apaga a face de costas). Medido com o
    conferidor: no poste de concreto uma aba inteira saía virada."""
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(o.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(o.data)
    bm.free()
    o.data.update()
    return o


# ===================================================================================================== a cruzeta e as ferragens
def arm_top(x):
    """A face de cima da cruzeta em x (a barriga leve nas pontas)."""
    return ARM_Z1 - 0.006 * (x / ARM_HALF) ** 2


def arm_section(x):
    """A seção da cruzeta em x (u = y, v = z − ARM_ZC): o retângulo de quinas comidas e desiguais, a fenda da frente (−Y), o lascado da
    quina de baixo da frente e o chanfro das pontas."""
    w, h = ARM_W, ARM_Z1 - ARM_Z0
    rc = 0.014 + 0.016 * cp.vnoise3(x * 2.3, 1.7, 0.0, 21)
    pts = vp.rounded_rect(w, h, rc, k=2)
    # a fenda da frente (entre o fim do canto (−,+) e o começo do canto (−,−): os índices 5 e 6)
    g = 0.012 * max(0.0, math.sin(math.pi * (x - 0.3) / 0.78)) if 0.3 < x < 1.08 else 0.0
    gz = -0.008 + 0.01 * math.sin(x * 3.0)
    pts = pts[:6] + [(-w / 2 + 0.2 * g, gz + 0.011), (-w / 2 + g, gz), (-w / 2 + 0.2 * g, gz - 0.011)] + pts[6:]
    out = []
    sag = -0.006 * (x / ARM_HALF) ** 2
    lim = h / 2 - max(0.0, abs(x) - 1.035) * 0.7            # o chanfro de cima nas pontas
    x0, x1 = -0.86, -0.6                                    # o lascado
    s = 0.017 * math.sin(math.pi * (x - x0) / (x1 - x0)) if x0 < x < x1 else 0.0
    for u, v in pts:
        if s and u < -w / 2 + 0.035 and v < -h / 2 + 0.035:
            u, v = u + s, v + s * 0.8
        out.append((u, min(v, lim) + sag))
    return out


def crossarm(p, arm_key='WoodOld#arm', zc=ARM_ZC):
    xs = [-1.10, -1.075, -1.035]
    x = -0.96
    while x < 0.96:
        xs.append(round(x, 3))
        x += 0.135
    xs += [0.96, 1.035, 1.075, 1.10]
    # as estações do lascado e da fenda caem certinho
    xs = sorted(set(xs + [-0.86, -0.82, -0.73, -0.64, -0.6, 0.3, 0.34, 1.06]))
    rings = [[(x, u, zc + v) for (u, v) in arm_section(x)] for x in xs]
    p.loft(arm_key, rings, bevel=0.0, smooth=False, caps=(True, True))


def flat_bar(p, m, a, b, w=0.038, t=0.007, thick_dir=(0.0, 1.0, 0.0), over=0.025):
    """O ferro chato de a até b (as pontas redondas), a espessura em 'thick_dir'."""
    a, b = Vector(a), Vector(b)
    ex = (b - a).normalized()
    ez = Vector(thick_dir) - ex * Vector(thick_dir).dot(ex)
    ez.normalize()
    ey = ez.cross(ex)
    vp.rbox_q(p, m, tuple((a + b) / 2), ((b - a).length + 2 * over, w, t), r=w * 0.45, k=2, frame=(ex, ey, ez))


def insulator(p, x, seat_z, y=0.0, pin_m='Metal#pin', glass_m='Lens#ins', nut_z=None):
    """O isolador de pino de vidro (o topo em INS_TOP): o pino de aço saindo da cruzeta (a porca embaixo dela), o corpo com 2 saias,
    o pescoço do fio (o sulco onde o jogo amarra o fio, a WIRE_Z) e a coroa."""
    H = 0.19
    z0 = INS_TOP - H
    vp.tube(p, pin_m, [(x, y, seat_z - 0.06), (x, y, z0 + 0.05)], 0.011, seg=6)
    vp.lathe(p, pin_m, (x, y, seat_z), 'z', [(0.0, 0.0), (0.024, 0.0), (0.024, 0.008), (0.016, 0.014), (0.012, 0.03), (0.0, 0.03)], seg=8)
    if nut_z is not None:
        vp.nut(p, pin_m, (x, y, nut_z - 0.004), (0, 0, -1), r=0.02, h=0.014)
        vp.nut(p, pin_m, (x, y, nut_z - 0.001), (0, 0, -1), r=0.03, h=0.004, seg=4)
    prof = [(0.0, 0.0), (0.02, 0.0), (0.058, 0.01), (0.066, 0.02), (0.062, 0.028), (0.046, 0.034), (0.04, 0.06), (0.05, 0.08),
            (0.056, 0.09), (0.052, 0.098), (0.038, 0.105), (0.033, 0.13), (0.038, 0.146), (0.04, 0.155), (0.031, 0.161),
            (0.028, 0.168), (0.031, 0.175), (0.034, 0.181), (0.026, 0.188), (0.0, H)]
    vp.lathe(p, glass_m, (x, y, z0), 'z', prof, seg=14, smooth=True, hard=50.0)


def wrap_patch(p, m, a0, a1, zb_fn, zt, z_of, nu=7, nv=4, off=0.004, thick=0.0025):
    """O papel colado na tora (o cartaz rasgado): a placa fina que segue a superfície entre os ângulos a0..a1, do pé rasgado zb_fn(u)
    até zt; fechada (as 2 faces e a borda), pra a normal sair certa."""
    top, bot = [], []
    b = p.bm(m, 0.0)
    for jv in range(nv + 1):
        rt, rb = [], []
        v = jv / nv
        for iu in range(nu + 1):
            u = iu / nu
            a = a0 + (a1 - a0) * u
            z = zb_fn(u) + (zt - zb_fn(u)) * v
            q = z_of(a, z, off)
            n = radial(a)
            rt.append(b.verts.new(V(*(q + n * thick / 2))))
            rb.append(b.verts.new(V(*(q - n * thick / 2))))
        top.append(rt)
        bot.append(rb)
    for j in range(nv):
        for i in range(nu):
            b.faces.new([top[j][i], top[j][i + 1], top[j + 1][i + 1], top[j + 1][i]])
            b.faces.new([bot[j + 1][i], bot[j + 1][i + 1], bot[j][i + 1], bot[j][i]])
    edge = [(0, i) for i in range(nu)] + [(j, nu) for j in range(nv)] + [(nv, i) for i in range(nu, 0, -1)] + [(j, 0) for j in range(nv, 0, -1)]
    for (j0, i0), (j1, i1) in zip(edge, edge[1:] + edge[:1]):
        b.faces.new([top[j0][i0], bot[j0][i0], bot[j1][i1], top[j1][i1]])


def hardware(p):
    """A chapa de apoio, o parafuso passante, as mãos-francesas, os pinos e os isoladores."""
    bx, by = axis_c(ARM_ZC)
    back = by + r_surf(math.pi / 2, ARM_ZC)
    # a chapa de apoio galvanizada (entre a cruzeta e a tora)
    vp.rbox_q(p, 'Metal#gain', (bx, ARM_W / 2 + 0.003, ARM_ZC), (0.2, 0.17, 0.008), r=0.02, k=2,
              frame=(Vector((1, 0, 0)), Vector((0, 0, 1)), Vector((0, 1, 0))))
    # o parafuso passante: a arruela quadrada e a porca na frente da cruzeta, a ponta e a porca atrás da tora
    vp.tube(p, 'Metal#bolt', [(bx, -ARM_W / 2 - 0.03, ARM_ZC), (bx, back + 0.04, ARM_ZC)], 0.0095, seg=6)
    vp.nut(p, 'Metal#bolt', (bx, -ARM_W / 2 - 0.001, ARM_ZC), (0, -1, 0), r=0.034, h=0.006, seg=4)
    vp.nut(p, 'Metal#bolt', (bx, -ARM_W / 2 - 0.007, ARM_ZC), (0, -1, 0), r=0.02, h=0.016)
    vp.nut(p, 'Metal#bolt', (bx, back - 0.004, ARM_ZC), (0, 1, 0), r=0.03, h=0.005, seg=10)
    vp.nut(p, 'Metal#bolt', (bx, back + 0.001, ARM_ZC), (0, 1, 0), r=0.02, h=0.016)
    # as mãos-francesas em V (ferro chato): o tirefão na face da frente da tora, as pontas embaixo da cruzeta
    zl = ARM_Z0 - 0.55
    lx, ly = axis_c(zl)
    L = Vector((lx, ly - r_surf(-math.pi / 2, zl) - 0.004, zl))
    for s in (-1, 1):
        U = Vector((s * 0.6, 0.0, ARM_Z0 - 0.006 * (0.6 / ARM_HALF) ** 2 - 0.004))
        flat_bar(p, 'Metal#brace', L, U, thick_dir=(0.0, 1.0, 0.0))
        vp.nut(p, 'Metal#brace', (U.x, 0.0, U.z - 0.008), (0, 0, -1), r=0.018, h=0.012)
    vp.nut(p, 'Metal#brace', tuple(L + Vector((0, -0.006, 0))), (0, -1, 0), r=0.022, h=0.016)
    for x in INS_X:
        insulator(p, x, arm_top(x), nut_z=ARM_Z0 - 0.006 * (x / ARM_HALF) ** 2)


def trunk_extras(p, transformer=False):
    """Os degraus, o fio de terra com a proteção do pé, a plaqueta e o cartaz."""
    for k in range(10):
        z = 2.45 + 0.46 * k
        a = (0.0 if k % 2 == 0 else math.pi) + (cp.h3(k, 3, 7) - 0.5) * 0.25
        d = radial(a)
        S0 = surf(a, z, -0.015)
        vp.tube(p, 'Metal#step', vp.bent_bar([S0, S0 + d * 0.175, S0 + d * 0.18 + Vector((0, 0, 0.05))], R=0.022, n=2), 0.0095, seg=6)
        q = surf(a, z, 0.0)
        vp.nut(p, 'Metal#step', tuple(q + d * 0.001), tuple(d), r=0.03, h=0.005, seg=4)
        vp.nut(p, 'Metal#step', tuple(q + d * 0.006), tuple(d), r=0.019, h=0.013)
    # o fio de terra pela face de trás (até o alto) e a proteção de chapa no pé
    ag = math.pi / 2 + 0.55
    pts = [surf(ag, z, 0.008) for z in [0.0] + [0.5 * k for k in range(1, 15)] + [7.55]]
    vp.tube(p, 'Dark#ground', pts, 0.0055, seg=5)
    for z0, z1 in ((0.03, 1.25), (1.25, 2.45)):
        a_, b_ = surf(ag, z0, 0.011), surf(ag, z1, 0.011)
        ex = (b_ - a_).normalized()
        ez = radial(ag)
        ey = ez.cross(ex)
        vp.rbox_q(p, 'Metal#guard', tuple((a_ + b_) / 2), ((b_ - a_).length, 0.036, 0.012), r=0.012, k=2, frame=(ex, ey, ez))
    # a plaqueta de número (alumínio pregado) na face −X
    at = math.pi + 0.35
    q = surf(at, 1.95, 0.003)
    ez = radial(at)
    ex = Vector((-math.sin(at), math.cos(at), 0.0))
    vp.rbox_q(p, 'Metal#tag', tuple(q), (0.085, 0.12, 0.004), r=0.008, k=1, frame=(ex, Vector((0, 0, 1)), ez))
    # o cartaz rasgado (o papel velho colado) na face da frente-direita
    ap = -math.pi / 2 + 0.75
    wrap_patch(p, 'Cloth#paper', ap - 0.85, ap + 0.85, lambda u: 1.48 + 0.07 * cp.h3(int(u * 9), 5, 3) + 0.04 * math.sin(u * 9.0),
               1.93, surf, nu=10)
    if not transformer:
        wrap_patch(p, 'Cloth#paper', ap + 1.0, ap + 1.3, lambda u: 1.66 + 0.03 * cp.h3(int(u * 5), 9, 1), 1.79, surf, nu=4, nv=2)


# ===================================================================================================== o transformador
def transformer(p):
    """O tanque pendurado na face da frente (−Y) embaixo da cruzeta, a cruzeta de baixo com as chaves-fusível e os para-raios, os
    jumpers e o cabo de baixa no eletroduto."""
    zc = 5.6
    tx, ty = axis_c(zc)
    R = 0.27
    face = ty - r_surf(-math.pi / 2, zc)
    C = Vector((tx, face - 0.075 - R, 0.0))
    prof = [(0.0, 5.10), (0.2, 5.10), (0.25, 5.115), (0.262, 5.13), (0.262, 5.16), (R, 5.17), (R, 5.42), (R + 0.008, 5.43),
            (R + 0.008, 5.45), (R, 5.46), (R, 5.72), (R + 0.008, 5.73), (R + 0.008, 5.75), (R, 5.76), (R, 5.98), (R + 0.016, 5.99),
            (R + 0.016, 6.02), (R - 0.008, 6.03), (0.2, 6.065), (0.0, 6.08)]
    vp.lathe(p, 'Metal#xfmr', (C.x, C.y, 0.0), 'z', prof, seg=26, smooth=True, hard=35.0)
    # as 2 baterias de aletas (nos lados ±X)
    for side in (0.0, math.pi):
        for k in range(-2, 3):
            a = side + k * 0.13
            q = C + Vector((math.cos(a), math.sin(a), 0.0)) * (R + 0.038) + Vector((0, 0, 5.57))
            vp.rbox_q(p, 'Metal#xfmr', tuple(q), (0.075, 0.012, 0.56), r=0.005, k=1,
                      frame=(Vector((math.cos(a), math.sin(a), 0)), Vector((-math.sin(a), math.cos(a), 0)), Vector((0, 0, 1))))
        for z in (5.27, 5.87):
            q = C + Vector((math.cos(side), math.sin(side), 0.0)) * (R + 0.02) + Vector((0, 0, z))
            vp.rbox_q(p, 'Metal#xfmr', tuple(q), (0.04, 0.6, 0.03), r=0.01, k=1,
                      frame=(Vector((math.cos(side), math.sin(side), 0)), Vector((-math.sin(side), math.cos(side), 0)), Vector((0, 0, 1))))
    # os 2 suportes (o ferro em U do tanque à tora) e os parafusos passantes
    for z in (5.83, 5.33):
        bx, by = axis_c(z)
        fy = by - r_surf(-math.pi / 2, z)
        vp.rbox_q(p, 'Metal#hanger', (C.x, (C.y + R - 0.01 + fy) / 2, z), (0.26, (fy - (C.y + R - 0.01)) + 0.02, 0.05), r=0.012, k=2)
        back = by + r_surf(math.pi / 2, z)
        for dx in (-0.08, 0.08):
            vp.tube(p, 'Metal#bolt', [(bx + dx, fy - 0.02, z), (bx + dx, back + 0.035, z)], 0.0085, seg=6)
            vp.nut(p, 'Metal#bolt', (bx + dx, back + 0.001, z), (0, 1, 0), r=0.018, h=0.014)
    # a plaqueta e o aterramento do tanque
    vp.rbox_q(p, 'Metal#tag', (C.x + 0.12, C.y - R - 0.003, 5.55), (0.13, 0.09, 0.004), r=0.008, k=1,
              frame=(Vector((1, 0, 0)), Vector((0, 0, 1)), Vector((0, -1, 0))))
    # as buchas de alta na tampa e as de baixa na frente
    hv = []
    for dx in (-0.1, 0.1):
        q = (C.x + dx, C.y - 0.06, 6.05)
        vp.lathe(p, 'Lens#bush', q, 'z', [(0.0, 0.0), (0.04, 0.0), (0.05, 0.03), (0.032, 0.05), (0.045, 0.075), (0.028, 0.095),
                                         (0.04, 0.12), (0.024, 0.14), (0.02, 0.17), (0.0, 0.175)], seg=12, hard=40.0)
        vp.tube(p, 'Metal#term', [(q[0], q[1], q[2] + 0.16), (q[0], q[1], q[2] + 0.23)], 0.008, seg=6)
        hv.append(Vector((q[0], q[1], q[2] + 0.22)))
    lv = []
    for dx in (-0.12, 0.0, 0.12):
        q = Vector((C.x + dx, C.y - R + 0.01, 5.88))
        vp.lathe(p, 'Lens#bush', tuple(q), (0, -1, 0), [(0.0, 0.0), (0.03, 0.0), (0.034, 0.02), (0.022, 0.035), (0.028, 0.055),
                                                        (0.016, 0.075), (0.0, 0.08)], seg=10)
        lv.append(q + Vector((0, -0.09, 0)))
        vp.tube(p, 'Metal#term', [tuple(q + Vector((0, -0.07, 0))), tuple(q + Vector((0, -0.1, 0)))], 0.007, seg=6)
    # a cruzeta de baixo (as chaves e os para-raios)
    z_lo = 6.68
    crossarm_lo(p, z_lo)
    for s, xi in ((-1, INS_X[1]), (1, INS_X[2])):
        x = s * 0.42
        # o suporte da chave saindo da frente da cruzeta, a porcelana inclinada e o tubo do fusível
        base = Vector((x, -0.06, z_lo))
        vp.rbox_q(p, 'Metal#hanger', tuple(base + Vector((0, -0.05, 0))), (0.05, 0.1, 0.04), r=0.01, k=1)
        tilt = Vector((0.0, -math.sin(math.radians(20)), math.cos(math.radians(20))))
        c0 = base + Vector((0, -0.11, -0.16))
        vp.lathe(p, 'Lens#cut', tuple(c0), tuple(tilt), [(0.0, 0.0), (0.03, 0.0), (0.042, 0.03), (0.026, 0.05), (0.042, 0.08),
                                                         (0.026, 0.1), (0.042, 0.13), (0.026, 0.15), (0.042, 0.18),
                                                         (0.026, 0.2), (0.042, 0.23), (0.026, 0.25), (0.03, 0.31), (0.0, 0.32)], seg=10)
        fwd = Vector((0.0, -math.cos(math.radians(20)), -math.sin(math.radians(20))))
        f0 = c0 + fwd * 0.07 - tilt * 0.02
        f1 = f0 + tilt * 0.36
        vp.tube(p, 'Dark#fuse', [tuple(f0), tuple(f1)], 0.016, seg=8)
        for q, sz in ((f1, 0.05), (f0, 0.045)):
            vp.rbox_q(p, 'Metal#term', tuple(q), (0.06, sz, 0.035), r=0.008, k=1, frame=(Vector((1, 0, 0)), fwd, tilt))
        # os jumpers: do pescoço do isolador da linha ao topo da chave, da chave à bucha de alta
        top = Vector((xi, 0.0, WIRE_Z + 0.01))
        jt = f1 + tilt * 0.04
        vp.tube(p, 'Dark#lead', vp.bent_bar([top + Vector((0, -0.02, 0)), top + Vector((s * 0.02, -0.12, -0.12)), jt + Vector((0, -0.03, 0.12)),
                                             jt], R=0.08, n=3), 0.0065, seg=5)
        jb = f0 - tilt * 0.03
        h = hv[0 if s < 0 else 1]
        vp.tube(p, 'Dark#lead', vp.bent_bar([jb, jb + Vector((0, -0.04, -0.15)), h + Vector((0, -0.05, 0.12)), h], R=0.1, n=3),
                0.0065, seg=5)
        # o para-raios na ponta da cruzeta de baixo e o fio dele até a chave e ao terra
        xa = s * 0.6
        vp.lathe(p, 'Dark#arrester', (xa, -0.02, z_lo + 0.065), 'z', [(0.0, 0.0), (0.03, 0.0), (0.03, 0.02), (0.045, 0.03),
                                                                    (0.024, 0.045), (0.045, 0.065), (0.024, 0.08), (0.045, 0.1),
                                                                    (0.024, 0.115), (0.04, 0.13), (0.024, 0.15), (0.02, 0.2),
                                                                    (0.0, 0.205)], seg=10)
        vp.tube(p, 'Dark#lead', vp.bent_bar([Vector((xa, -0.02, z_lo + 0.27)), Vector((xa, -0.06, z_lo + 0.33)),
                                             jb + Vector((s * 0.05, 0.0, 0.05)), jb], R=0.06, n=2), 0.005, seg=5)
    # os cabos de baixa: o laço de gotejamento e o eletroduto que desce pela frente da tora
    ac = -math.pi / 2 + 0.5
    cz = 4.75
    head = surf(ac, cz, 0.035)
    for k, q in enumerate(lv):
        # o laço desce na frente do tanque até abaixo do fundo dele (5,10 m) e volta pra cabeça do eletroduto
        vp.tube(p, 'Dark#lead', vp.bent_bar([q, q + Vector((0, -0.06, -0.08)), q + Vector((0, -0.05, -0.86 - 0.03 * k)),
                                             head + Vector((0, -0.03, 0.08)), head + Vector((0, 0, 0.02))], R=0.07, n=3), 0.009, seg=6)
    pts = [surf(ac, z, 0.035) for z in (cz, 4.2, 3.4, 2.6, 1.8, 0.9, 0.3, 0.0)]
    pts.append(pts[-1] + Vector((0, 0, -0.1)))
    vp.tube(p, 'Dark#conduit', pts, 0.026, seg=8)
    vp.lathe(p, 'Dark#conduit', tuple(head), 'z', [(0.0, 0.0), (0.036, 0.0), (0.04, 0.03), (0.03, 0.06), (0.0, 0.065)], seg=8)
    for z in (4.3, 3.3, 2.2, 1.0):
        q = surf(ac, z, 0.035)
        d = radial(ac)
        vp.rbox_q(p, 'Metal#strap', tuple(q - d * 0.01), (0.04, 0.09, 0.012), r=0.006, k=1,
                  frame=(Vector((0, 0, 1)), Vector((-math.sin(ac), math.cos(ac), 0)), d))


def crossarm_lo(p, zc):
    """A cruzeta de baixo do transformador (1,3 m, mais fina), presa na face da frente da tora, com o parafuso e as 2 mãos-francesas."""
    bx, by = axis_c(zc)
    face = by - r_surf(-math.pi / 2, zc)
    w, h = 0.095, 0.115
    yc = face + 0.006 - w / 2
    rings = []
    for x in (-0.65, -0.625, -0.58, -0.45, -0.3, -0.15, 0.0, 0.15, 0.3, 0.45, 0.58, 0.625, 0.65):
        rc = 0.013 + 0.012 * cp.vnoise3(x * 2.9, 4.1, 0.0, 23)
        lim = h / 2 - max(0.0, abs(x) - 0.585) * 1.1
        rings.append([(x, yc + u, zc + min(v, lim)) for (u, v) in vp.rounded_rect(w, h, rc, k=2)])
    p.loft('WoodOld#arm', rings, bevel=0.0, smooth=False, caps=(True, True))
    back = by + r_surf(math.pi / 2, zc)
    vp.tube(p, 'Metal#bolt', [(bx, yc - w / 2 - 0.03, zc), (bx, back + 0.035, zc)], 0.009, seg=6)
    vp.nut(p, 'Metal#bolt', (bx, yc - w / 2 - 0.001, zc), (0, -1, 0), r=0.03, h=0.006, seg=4)
    vp.nut(p, 'Metal#bolt', (bx, yc - w / 2 - 0.007, zc), (0, -1, 0), r=0.019, h=0.015)
    zl = zc - 0.42
    lx, ly = axis_c(zl)
    L = Vector((lx, ly - r_surf(-math.pi / 2, zl) - 0.004, zl))
    for s in (-1, 1):
        flat_bar(p, 'Metal#brace', L, Vector((s * 0.38, yc, zc - h / 2 - 0.004)), w=0.032, t=0.006)
    vp.nut(p, 'Metal#brace', tuple(L + Vector((0, -0.006, 0))), (0, -1, 0), r=0.02, h=0.014)


# ===================================================================================================== o poste
def pole(with_transformer=False):
    p = Part('SM_civ_pole_transformer' if with_transformer else 'SM_civ_pole')
    trunk(p)
    crossarm(p)
    hardware(p)
    trunk_extras(p, with_transformer)
    if with_transformer:
        transformer(p)
    return p.finish()


if __name__ == '__main__':
    OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'civ_v3', 'pole')
    os.makedirs(OUT, exist_ok=True)
    clear_scene()
    a = pole(False)
    b = pole(True)
    bake_in_place(b, (5.0, 0.0, 0.0))
    print('CIV', a.name, cp.tris(a), b.name, cp.tris(b))
    export(a, OUT)
    export(b, OUT)
