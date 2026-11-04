"""As peças em comum da família do SUPPLY BÁSICO (03/11/2026): o depósito neutro do mapa de onde os caminhões de todas as facções
tiram o dinheiro, nos 4 tamanhos do ZH (docs/ECONOMIA.md §4; o estudo docs/elenco/SUPPLY_DOCK_ESTUDO.md; o conceito escolhido
docs/concept/buildings/supply_basico_4_tamanhos_ESCOLHIDO.png):
  model_supply_pile_small.py (50 caixas, o desenho 1), model_supply_pile.py (150, o desenho 2), model_supply_dock_v2.py (400, o
  desenho 4) e model_supply_warehouse.py (400, o desenho 3, a variação da vila). A caixa do estoque: model_supply_stock_crate.py.

O ESTOQUE QUE SOME (o W3DSupplyDraw do ZH): cada variação sai em 2 partes, a malha do lugar SEM as caixas do estoque
(SM_<Variacao>_Base: os paletes vazios, a lona, a rede, a empilhadeira, as caixas de enfeite que não contam) e as vagas
(<pasta>/stock_layout.json: {"crate_size_m": [x, y, z], "pile_m": [[x, y, z, giro], ...]}), uma linha por caixa NA ORDEM EM QUE
SOMEM: as de cima primeiro (camada por camada, da mais alta pra baixo) e, em cada camada, saltando de pilha em pilha (a ordem das
pilhas é a do ponto mais longe: o pátio esvazia por igual). z = a base da caixa; metros da Unreal (X frente, Y direita, Z cima;
origem no chão no centro do lote).

Aqui: a régua da caixa (a do lançamento aéreo, model_supply_crate_v2.py, sem o saco: 1,2 × 1,0 × 0,93 m), o palete com as ripas, a
pilha (Stock.stack) e a ordem, a caixa de enfeite (crate_lite: a mesma cara, barata), a lona caída por cima da pilha com as dobras
(tarp_over), a rede de camuflagem nas varas, a empilhadeira, o poste de holofote, a placa pintada à mão, o estêncil gasto, o chão
batido que acompanha a forma e a faixa dos caminhões (ground_pad, lane_pad), e a folha da prévia (o estoque cheio e pela metade).

Materiais (os slots genéricos/civis do import_provisional.py): Concrete, Plaster, Wood, Metal, Cloth (os sacos), Net (a lona oliva
da pilha, como no Supply Stash v5 da GLA, e a rede), Rust (a chapa), Crate (o oliva da caixa), Yellow (a faixa e a empilhadeira),
Dark, Trim, Hazard, Rubber, Paint (o estêncil), Earth (o chão batido, o GlaEarth), Glass (as lentes).
"""
import json
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import model_lib  # noqa: E402
from model_lib import COLORS, V  # noqa: E402
import model_civ_house_parts as hp  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
from model_building_parts import hexa, gla_net_surface  # noqa: E402
from model_gla_palace_v2 import _hash, fbm, rod, bag, bag_row, stake, rope, surface, smooth, Frame  # noqa: E402,F401
import model_supply_crate_v2 as crate_v2  # noqa: E402

# (02/11, o defeito do exportador glTF: model_lib._VC_ALL) as máscaras do detalhe em TODOS os materiais (senão só o 1º material de cada
# malha leva a quina, a fresta e a oclusão; o resto sai liso, "plástico"). São modelos novos: não há aparência aprovada a preservar
model_lib._VC_ALL['on'] = True

# as cores de conferência (o jogo troca pelas instâncias): as da caixa do lançamento (Crate, Yellow, Dark, Trim, Paint) e as do kit
COLORS.update({'Crate': (96, 98, 62), 'Yellow': (214, 164, 32), 'Dark': (40, 40, 38), 'Trim': (70, 70, 68), 'Paint': (222, 222, 216),
               'Concrete': (156, 150, 138), 'Plaster': (205, 180, 135), 'Wood': (120, 88, 52), 'Metal': (132, 130, 122),
               'Cloth': (150, 128, 92), 'Net': (92, 98, 60), 'Rust': (118, 70, 40), 'Hazard': (210, 170, 30), 'Rubber': (40, 36, 33),
               'Earth': (161, 141, 113), 'Glass': (30, 34, 38)})

# o chanfro por chave (o 1º uso fixa; no modo de detalhe < 1,5 cm dobra, ≥ 1,5 cm vira o mínimo da classe: 9 cm no prédio)
BEVELS = {
    'Wood#pallet': 0.006, 'Wood#pboard': 0.0, 'Wood#pole': 0.0, 'Wood#plank': 0.006, 'Wood#post': 0.006, 'Wood#board': 0.006,
    'Crate#lite': 0.012, 'Crate#lid': 0.012, 'Dark#lite': 0.0, 'Yellow#lite': 0.0, 'Trim#lite': 0.006,
    'Net#tarp': 0.0, 'Net#flap': 0.0, 'Net#roll': 0.0, 'Dark#rope': 0.0, 'Dark#fill': 0.0, 'Cloth#bag': 0.0, 'Cloth#net': 0.0,
    'Concrete': 0.03, 'Concrete#lo': 0.012, 'Concrete#step': 0.012, 'Concrete#wall': 0.03, 'Concrete#ramp': 0.0,
    'Dark#joint': 0.0, 'Hazard': 0.0, 'Yellow#line': 0.0, 'Paint#sten': 0.0, 'Yellow#paint': 0.0, 'Crate#paint': 0.0,
    'Metal#s': 0.0, 'Metal#sheet': 0.0, 'Rust#sheet': 0.0, 'Metal#beam': 0.0, 'Metal#cleat': 0.0, 'Metal#rail': 0.0,
    'Rubber': 0.012, 'Rubber#fl': 0.0, 'Earth#pad': 0.0, 'Earth#lane': 0.0, 'Glass': 0.0,
    'Yellow#fl': 0.0, 'Dark#fl': 0.0, 'Metal#fl': 0.0, 'Hazard#fl': 0.0,
}

# ===================================================================================================== a régua da caixa e da pilha
CRATE = (crate_v2.CRATE_L, crate_v2.CRATE_W, crate_v2.LID_TOP)     # 1,2 × 1,0 × 0,93 m (sem o saco do paraquedas)
STACK_DZ = 0.955          # a caixa de cima senta nas cintas da de baixo (a tampa 0,93 + a faixa e as cintas até 0,952)
PX, PY = 1.44, 1.2        # o passo na pilha: as alças das cabeceiras saem 9 cm (x ±0,69) e as catracas 6 cm (y ±0,59)
PALLET_TOP = 0.144        # o palete de 1,2 × 1,0 × 0,144 m
STOCK_CRATE = '../supply_stock_crate/SM_SupplyStockCrate'   # a caixa do estoque (pras folhas da prévia)


def new_part(name, seed=0, dirt=0.9):
    """A peça do kit civil (os chanfros e a sujeira das casas da vila) com os chanfros da família por cima."""
    p = hp.new_part(name, seed=seed, dirt=dirt)
    for m, b in BEVELS.items():
        p.bm(m, b)
    return p


finish = hp.finish


def rot(x, y, yaw):
    a = math.radians(yaw)
    c, s = math.cos(a), math.sin(a)
    return (x * c - y * s, x * s + y * c)


def h3(*a):
    """O sorteio fixo (o mesmo modelo sai sempre igual)."""
    return _hash(int(a[0] * 1000.0) if isinstance(a[0], float) else a[0], int(a[1] * 1000.0) if isinstance(a[1], float) else a[1],
                 a[2] if len(a) > 2 else 0)


# ===================================================================================================== o palete e a caixa de enfeite
def pallet(p, x, y, z=0.0, yaw=0.0, seed=0):
    """O palete de madeira de 1,2 × 1,0 × 0,144 m: as 3 tábuas de baixo, as 3 longarinas ao comprido (x local), as 7 tábuas de cima com a
    fresta de ~6 cm (as das pontas mais largas), cada uma com o seu desvio miúdo (a madeira de obra, pregada à mão)."""
    def P(lx, ly, lz):
        dx, dy = rot(lx, ly, yaw)
        return (x + dx, y + dy, z + lz)
    for k, lx in enumerate((-0.53, 0.0, 0.53)):
        p.obox('Wood#pboard', P(lx, 0.0, 0.011), (0.12, 1.0, 0.022), yaw=yaw)
    for k, ly in enumerate((-0.44, 0.0, 0.44)):
        p.obox('Wood#pboard', P(0.0, ly, 0.071), (1.2, 0.09, 0.098), yaw=yaw)     # (as longarinas quase não aparecem: sem o chanfro)
    for k in range(7):
        lx = -0.53 + 1.06 * k / 6
        w = 0.14 if k in (0, 6) else 0.1
        jy = (_hash(k, seed, 7) - 0.5) * 0.03
        p.obox('Wood#pallet', P(lx, jy, 0.132), (w, 1.0, 0.024), yaw=yaw + (_hash(k, seed, 9) - 0.5) * 2.0)


def pallet_pile(p, x, y, n, yaw=0.0, seed=0):
    """A pilha de paletes vazios (cada um um pouco torto em cima do outro)."""
    for k in range(n):
        dx, dy = (_hash(k, seed, 1) - 0.5) * 0.08, (_hash(k, seed, 2) - 0.5) * 0.08
        pallet(p, x + dx, y + dy, k * PALLET_TOP, yaw + (_hash(k, seed, 3) - 0.5) * 6.0, seed=seed * 10 + k)


def crate_lite(p, x, y, z, yaw=0.0, lid=True):
    """A caixa de ENFEITE (a que não conta: debaixo da lona, dentro do armazém): a mesma cara da do estoque (a plataforma de aço, o
    corpo oliva, as 4 cantoneiras escuras, a tampa com a aba, a faixa amarela, as 2 cintas), barata (~1/10 dos triângulos)."""
    def P(lx, ly, lz):
        dx, dy = rot(lx, ly, yaw)
        return (x + dx, y + dy, z + lz)
    p.obox('Trim#lite', P(0, 0, 0.05), (1.26, 1.06, 0.1), yaw=yaw)
    p.obox('Crate#lite', P(0, 0, 0.475), (1.16, 0.96, 0.75), yaw=yaw)
    for sx in (-1, 1):
        for sy in (-1, 1):
            p.obox('Dark#lite', P(sx * 0.545, sy * 0.445, 0.48), (0.13, 0.13, 0.77), yaw=yaw)
    if lid:
        p.obox('Crate#lid', P(0, 0, 0.89), (1.24, 1.04, 0.08), yaw=yaw)
        p.obox('Yellow#lite', P(0, 0, 0.934), (1.18, 0.32, 0.012), yaw=yaw)
        for lx in (-0.3, 0.3):
            p.obox('Dark#lite', P(lx, 0, 0.5), (0.075, 1.08, 0.88), yaw=yaw)
            p.obox('Dark#lite', P(lx, 0, 0.94), (0.075, 1.0, 0.016), yaw=yaw)


# ===================================================================================================== o estoque
class Stock:
    """As pilhas do estoque: cada pilha é uma lista de colunas, cada coluna as caixas de baixo pra cima (x, y, z, giro)."""

    def __init__(self):
        self.stacks = []

    def stack(self, p, c, heights, yaw=0.0, z0=0.0, pallets=True, seed=0, jitter=1.0):
        """A pilha em c = (x, y): heights[i][j] caixas na coluna (i ao longo do x local, j do y local), giro yaw. Um palete por
        coluna (fica na malha: o palete vazio é o que sobra), as caixas no passo PX × PY com o desvio miúdo de quem empilhou à mão
        (2–3 cm, 1–3°, mais nas de cima)."""
        nx, ny = len(heights), len(heights[0])
        cols = []
        for i in range(nx):
            for j in range(ny):
                lx = (i - (nx - 1) / 2.0) * PX
                ly = (j - (ny - 1) / 2.0) * PY
                dx, dy = rot(lx, ly, yaw)
                x, y = c[0] + dx, c[1] + dy
                k0 = seed * 100 + i * 10 + j
                if pallets:
                    pallet(p, x + (_hash(k0, 1, 5) - 0.5) * 0.04, y + (_hash(k0, 2, 5) - 0.5) * 0.04, z0,
                           yaw + (_hash(k0, 3, 5) - 0.5) * 3.0, seed=k0)
                zb = z0 + (PALLET_TOP if pallets else 0.0)
                col = []
                for k in range(heights[i][j]):
                    s = jitter * (1.0 + 0.6 * k)
                    col.append((x + (_hash(k0, k, 11) - 0.5) * 0.04 * s, y + (_hash(k0, k, 12) - 0.5) * 0.04 * s, zb + k * STACK_DZ,
                                yaw + (_hash(k0, k, 13) - 0.5) * 3.0 * s))
                cols.append(col)
        self.stacks.append(cols)

    def loose(self, items):
        """Caixas soltas no chão (sem palete): uma pilha de colunas [(x, y, z, giro)...]."""
        self.stacks.append([list(col) for col in items])

    def count(self):
        return sum(len(col) for st in self.stacks for col in st)

    def footprints(self, m=0.78):
        """As caixas de baixo de cada coluna como retângulos (x0, x1, y0, y1) com folga: onde o miúdo do chão não cai."""
        return [(c[0][0] - m, c[0][0] + m, c[0][1] - m, c[0][1] + m) for st in self.stacks for c in st if c]

    def ordered(self):
        """A ordem em que somem: camada por camada (a mais alta primeiro); em cada camada, uma caixa de cada pilha por volta, as pilhas
        na ordem do ponto mais longe (a que fica mais longe das já tiradas vem depois)."""
        S = self.stacks
        cents = []
        for st in S:
            pts = [c[0] for c in st if c] or [(0.0, 0.0, 0.0, 0.0)]
            cents.append((sum(q[0] for q in pts) / len(pts), sum(q[1] for q in pts) / len(pts)))
        gx = sum(c[0] for c in cents) / len(cents)
        gy = sum(c[1] for c in cents) / len(cents)
        left = list(range(len(S)))
        order = [max(left, key=lambda k: (cents[k][0] - gx) ** 2 + (cents[k][1] - gy) ** 2)]
        left.remove(order[0])
        while left:
            nxt = max(left, key=lambda k: min((cents[k][0] - cents[o][0]) ** 2 + (cents[k][1] - cents[o][1]) ** 2 for o in order))
            order.append(nxt)
            left.remove(nxt)
        top = max(len(col) for st in S for col in st)
        out = []
        for lv in range(top - 1, -1, -1):
            queues = []
            for s in order:
                q = [col[lv] for col in S[s] if len(col) > lv]
                q.sort(key=lambda t: _hash(int(t[0] * 100) + 5000, int(t[1] * 100) + 5000, lv))
                queues.append(q)
            while any(queues):
                for q in queues:
                    if q:
                        out.append(q.pop(0))
        return out

    def write(self, out_dir):
        order = self.ordered()
        pile = [[round(q[0], 3), round(q[1], 3), round(q[2], 3), round(q[3], 2)] for q in order]
        data = {'_about': 'As vagas do estoque (03/11/2026, model_supply_basic_parts.Stock): uma por caixa na ordem em que somem (as '
                          'de cima primeiro, saltando de pilha em pilha); x, y, z (a base da caixa) em metros da Unreal e o giro em graus. '
                          'A caixa: captures/game/models/supply_stock_crate/SM_SupplyStockCrate.',
                'crate_size_m': [CRATE[0], CRATE[1], CRATE[2]], 'pile_m': pile}
        with open(os.path.join(out_dir, 'stock_layout.json'), 'w', encoding='utf-8') as f:
            json.dump(data, f, ensure_ascii=False, indent=1)
        return pile


# ===================================================================================================== a lona caída por cima da pilha
def _profile(W, H, hem, o=0.03, r=0.11, n_side=9, n_arc=4, n_top=9):
    """O perfil da lona atravessando a pilha (y, z): do pé do lado −Y sobe, dobra a quina de cima (raio r), atravessa o topo e desce
    o lado +Y. Devolve [(y, z, ny, nz, lado)] com lado = 1 nas faces de pé, 0 no topo."""
    hw = W / 2 + o
    pts = []
    for k in range(n_side + 1):
        z = hem + (H - r - hem) * k / n_side
        pts.append((-hw, z, -1.0, 0.0, 1.0))
    for k in range(1, n_arc):
        a = math.pi - (math.pi / 2) * k / n_arc
        pts.append((-hw + r + r * math.cos(a), H - r + o + (r) * math.sin(a), math.cos(a), math.sin(a), 0.5))
    for k in range(n_top + 1):
        y = -hw + r + (2 * hw - 2 * r) * k / n_top
        pts.append((y, H + o, 0.0, 1.0, 0.0))
    for k in range(1, n_arc):
        a = math.pi / 2 - (math.pi / 2) * k / n_arc
        pts.append((hw - r + r * math.cos(a), H - r + o + r * math.sin(a), math.cos(a), math.sin(a), 0.5))
    for k in range(n_side + 1):
        z = H - r - (H - r - hem) * k / n_side
        pts.append((hw, z, 1.0, 0.0, 1.0))
    return pts


def tarp_over(p, c, L, W, H, yaw=0.0, cover=1.0, seed=0, hem=-0.1, ropes=(0.28, 0.72), m='Net#tarp', gaps_x=(), gaps_y=(),
              fill=True):
    """A lona oliva caída por cima da pilha (o desenho 1 e o 2): c = (x, y, z0) o meio da base da carga (o topo do palete), L ao longo
    do x local (yaw), W atravessado, H a altura da carga. A lona cobre do fundo (x local −L/2) até 'cover' do comprimento e o resto
    fica PUXADO: o pano embola na beira (as dobras grandes e o rolo). As dobras de verdade: as pregas de pé nos lados (3 frequências
    com fase sorteada, crescendo pra baixo), a barra que abre e ondula, o pano que afunda na fresta entre as caixas (gaps_x, gaps_y no
    quadro local), a aba do fundo caída por cima da cabeceira, as cordas por cima amarradas no palete. fill: o escuro por dentro (a
    fresta debaixo da barra não mostra o vazio)."""
    x0c, y0c, z0 = c
    ex = Vector((math.cos(math.radians(yaw)), math.sin(math.radians(yaw)), 0.0))
    ey = Vector((-ex.y, ex.x, 0.0))
    ez = Vector((0.0, 0.0, 1.0))
    C = Vector((x0c, y0c, z0))
    prof = _profile(W, H, hem)
    ph = [_hash(seed, k, 71) * 6.283 for k in range(8)]
    xa = -L / 2 - 0.06
    xb = -L / 2 + cover * L

    rope_xs = [xa + (xb - xa) * fr for fr in ropes]

    def wav(t, a, b, c_):
        return (math.sin(t / a + ph[0]) + 0.55 * math.sin(t / b + ph[1]) + 0.3 * math.sin(t / c_ + ph[2])) / 1.85

    def ridge(t, a, ph_):
        """A prega: a crista fina e o vale largo (o pano dobra, não ondula)."""
        return 1.0 - abs(math.sin(t / a + ph_))

    def disp(x, y, z, side, which):
        hang = max(0.0, min(1.0, (H - z) / max(0.2, H - hem)))
        d = 0.0
        if side > 0.0:
            # as pregas de pé (nascem finas na quina de cima e abrem pra barra), as diagonais que descem das quinas de cima (o pano
            # puxado pelo canto), a barra que abre e arrasta no chão, o cone caído no canto do fundo
            pleat = 0.62 * (ridge(x + which * 3.7, 0.1, ph[0]) - 0.36) + 0.38 * (ridge(x * 1.07 + which * 1.3, 0.055, ph[1]) - 0.36)
            diag = 0.0
            for (qx, sgn) in ((xa, 1.0), (xb, -1.0)):
                e = (x - qx) * sgn
                if 0.0 < e < 1.2:
                    diag += (1.0 - e / 1.2) * (ridge(e - (H - z) * 0.75, 0.07, ph[6] + qx) - 0.36)
            d += side * (0.13 * (0.1 + 0.9 * hang ** 1.1) * pleat + 0.09 * diag * hang + 0.12 * hang ** 2.2)
            d += side * 0.2 * hang ** 1.5 * math.exp(-((x - xa) / 0.34) ** 2)
        if side < 1.0:
            t = 1 - side
            d += t * (0.01 * math.sin(x * 5.1 + y * 3.3 + ph[3]) + 0.008 * math.sin(x * 11.0 - y * 7.0 + ph[4]))
            for g in gaps_x:
                d -= t * 0.085 * math.exp(-((x - g) / 0.18) ** 2)
            for g in gaps_y:
                d -= t * 0.07 * math.exp(-((y - g) / 0.17) ** 2)
            # as dobras de tensão em leque saindo das quinas de cima (o pano puxado pra baixo nos cantos)
            for (qx, qy) in ((xa + 0.05, -W / 2), (xa + 0.05, W / 2), (xb, -W / 2), (xb, W / 2)):
                r_ = math.hypot(x - qx, y - qy)
                a_ = math.atan2(y - qy, x - qx)
                d += t * 0.035 * (r_ / 0.25) * math.exp(-r_ / 0.35) * math.sin(7.0 * a_ + qx * 3.0 + qy)
        # a corda aperta o pano (o vinco e as 2 bordas estufadas)
        for xr in rope_xs:
            d -= 0.03 * math.exp(-((x - xr) / 0.05) ** 2)
            d += 0.014 * math.exp(-((abs(x - xr) - 0.15) / 0.07) ** 2)
        # o embolado na beira puxada
        if cover < 0.999:
            e = xb - x
            if e < 0.6:
                k = 1.0 - e / 0.6
                d += 0.13 * k * k * (0.5 + 0.5 * math.sin(x * 26.0 + y * 3.0 + ph[5]))
        return d

    def at(x, v_idx, extra=0.0):
        y, z, ny, nz, side = prof[v_idx]
        which = -1 if y < 0 else 1
        d = disp(x, y, z, side, which) + extra
        zz = z
        if v_idx == 0 or v_idx == len(prof) - 1:
            zz += 0.07 * (0.5 + 0.5 * math.sin(x * 3.7 + ph[6] * which)) + 0.03 * math.sin(x * 9.3 + ph[7])
        q = C + ex * x + ey * (y + ny * d) + ez * (zz + nz * d)
        return (q.x, q.y, q.z)

    nu = max(6, int((xb - xa) / 0.045))
    nv = len(prof) - 1

    def fn(u, v):
        return at(xa + (xb - xa) * u, int(round(v * nv)))
    surface(p, m, fn, nu, nv, thick=0.025)
    # a aba do fundo: do topo (por cima do pano) dobra a quina e cai na cabeceira, as pregas atravessadas, as pontas caídas
    hw = W / 2 + 0.06
    xe = -L / 2 - 0.035

    zt = H + 0.05
    rr = 0.12

    def fa(u, v):
        y = -hw + 2 * hw * u
        corner = 1.0 - math.sin(math.pi * u) ** 0.35
        if v <= 0.15:
            a = (math.pi / 2) * v / 0.15
            x = xe + 0.06 - rr * math.sin(a)
            z = zt - rr * (1 - math.cos(a))
        else:
            t = (v - 0.15) / 0.85
            x = xe + 0.06 - rr
            z = (zt - rr) - (zt - rr - hem) * t - 0.12 * corner * t
            x -= 0.05 * (0.3 + 0.7 * t) * wav(y * 1.3 + 2.0, 0.07, 0.045, 0.025) + 0.06 * t * t
        q = C + ex * x + ey * y + ez * (z + (0.04 * math.sin(y * 4.1 + ph[2]) if v > 0.98 else 0.0))
        return (q.x, q.y, q.z)
    surface(p, m.split('#')[0] + '#flap', fa, 22, 12, thick=0.025)
    # o rolo na beira puxada (o pano embolado atravessando a pilha)
    if cover < 0.999:
        for (dx_, off, rr_) in ((0.08, 0.07, 0.095), (0.3, 0.05, 0.065)):
            pts = [Vector(at(xb - dx_, k, off)) for k in range(0, len(prof))]
            vp.tube(p, m.split('#')[0] + '#roll', pts, rr_, seg=8,
                    r_fn=lambda i, rr_=rr_: rr_ * (0.8 + 0.4 * abs(math.sin(i * 1.3 + seed + rr_ * 40))))
    # as cordas por cima (seguem o pano), amarradas no palete
    for k_, fr in enumerate(ropes):
        x = xa + (xb - xa) * fr
        pts = [Vector(at(x, k, 0.02)) for k in range(len(prof))]
        for end, k in ((0, 0), (1, len(prof) - 1)):
            y_s = (-1 if end == 0 else 1) * (W / 2 + 0.02)
            q = C + ex * x + ey * y_s + ez * 0.03
            if end == 0:
                pts.insert(0, q)
            else:
                pts.append(q)
        vp.tube(p, 'Dark#rope', pts, 0.014, seg=5)
    if fill:
        # (só a metade de baixo: o pano afunda até 15 cm nas frestas de cima e o escuro furava a lona)
        q = C + ex * ((xa + xb) / 2 + 0.03) + ez * (H * 0.27)
        p.obox('Dark#fill', (q.x, q.y, q.z), (xb - xa - 0.1, W - 0.05, H * 0.54), yaw=yaw)


# ===================================================================================================== a rede de camuflagem
def camo_net(p, corners, seed=0, sag=0.3, holes=-1.0, light=0.52, poles=True, stakes_out=0.9, nu=None, nv=None):
    """A rede de camuflagem do DESERTO nas 4 varas (o desenho 1): os cantos (x, y, z) no sentido da volta; a barriga, a borda solta
    que cai. A rede inteira (sem rombos: holes −1) na cor de areia (Cloth#net) com as manchas oliva por cima (Net#patch) e os tufos
    de pano (o gla_net_surface: a rede com volume); as varas de madeira com a tampa e as cordas até as estacas."""
    c0, c1, c2, c3 = [Vector(q) for q in corners]
    L = (c1 - c0).length
    Wd = (c3 - c0).length
    nu = nu or max(8, int(L / 0.2))
    nv = nv or max(8, int(Wd / 0.2))

    def fn(u, v):
        q = (1 - u) * (1 - v) * c0 + u * (1 - v) * c1 + u * v * c2 + (1 - u) * v * c3
        cen = (c0 + c1 + c2 + c3) / 4
        k = 1.08
        q = Vector((cen.x + (q.x - cen.x) * k, cen.y + (q.y - cen.y) * k, q.z))
        edge = min(u, 1 - u, v, 1 - v)
        q.z -= sag * math.sin(math.pi * u) * math.sin(math.pi * v) + 0.32 * max(0.0, 0.1 - edge) / 0.1
        return (q.x, q.y, q.z)
    gla_net_surface(p, fn, nu, nv, seed=seed, blotch=0.9, holes=holes, light=light, ragged=0.3, thick=0.04, mats=('Cloth#net', 'Net#patch'))
    if poles:
        cen = (c0 + c1 + c2 + c3) / 4
        for k, q in enumerate((c0, c1, c2, c3)):
            rod(p, 'Wood#pole', (q.x, q.y, -0.1), (q.x + (_hash(k, seed, 3) - 0.5) * 0.08, q.y, q.z + 0.12), 0.055, seg=7)
            p.cyl('Dark#cap', (q.x, q.y, q.z + 0.14), 'z', 0.07, 0.05, seg=7, bevel=0.0)
            d = Vector((q.x - cen.x, q.y - cen.y, 0.0)).normalized()
            g = q + d * stakes_out
            st = stake(p, g.x, g.y, toward=(q.x, q.y))
            rope(p, (q.x, q.y, q.z + 0.05), st, sag=0.05, r=0.013, m='Dark#rope')


# ===================================================================================================== a empilhadeira
def forklift(p, x, y, yaw=0.0, fork_z=0.06, mast_h=2.15, seed=0, load_pallet=False):
    """A empilhadeira amarela pequena (o desenho 2; ~2,9 m com os garfos, 1,1 m de largura, a grade de proteção a 2,1 m): as rodas
    (as da frente maiores), o chassi e o contrapeso redondo atrás com a faixa de perigo, o capô com o banco, o botijão de gás deitado
    em cima do contrapeso, o painel com o volante, a grade de proteção de 4 colunas com as ripas, a torre de 2 trilhos com o cilindro
    e as correntes, o carro com o encosto e os 2 garfos no chão. A frente (os garfos) no x local."""
    F = Frame(p, (x, y, 0.0), yaw)
    ex, ey, ez = vp.ypr_frame(yaw)

    def W(lx, ly, lz):
        return F.pt(lx, ly, lz)
    # as rodas (o eixo é o y local)
    for (lx, r, w) in ((0.42, 0.31, 0.24), (-0.78, 0.25, 0.2)):
        for s in (-1, 1):
            c = W(lx, s * (0.55 - w / 2), r)
            prof = [(r * 0.6, -w / 2), (r * 0.9, -w / 2), (r, -w / 2 + 0.04), (r, w / 2 - 0.04), (r * 0.9, w / 2), (r * 0.6, w / 2)]
            vp.lathe(p, 'Rubber#fl', c, tuple(ey), prof, seg=20, closed=True)
            vp.lathe(p, 'Yellow#rim' if lx > 0 else 'Dark#fl', c, tuple(ey), [(0.0, -w / 2 + 0.03), (r * 0.6, -w / 2 + 0.02),
                                                                            (r * 0.6, w / 2 - 0.02), (0.0, w / 2 - 0.03)], seg=14)
    # o chassi, os paralamas, o contrapeso (redondo atrás) com a faixa de perigo
    vp.rbox(p, 'Yellow#fl', W(-0.2, 0.0, 0.47), (1.5, 0.86, 0.5), yaw=yaw, r=0.06, k=2)
    for s in (-1, 1):
        vp.rbox(p, 'Yellow#fl', W(0.4, s * 0.47, 0.66), (0.72, 0.2, 0.08), yaw=yaw, r=0.03, k=1)
    vp.rbox(p, 'Yellow#fl', W(-0.98, 0.0, 0.6), (0.5, 1.08, 0.86), yaw=yaw, r=0.22, k=4)
    vp.rbox(p, 'Hazard#fl', W(-1.235, 0.0, 0.66), (0.012, 0.72, 0.16), yaw=yaw, r=0.004, k=1)
    # o capô com o banco e o encosto, o botijão deitado no contrapeso
    vp.rbox(p, 'Yellow#fl', W(-0.42, 0.0, 0.8), (0.82, 0.96, 0.18), yaw=yaw, r=0.08, k=3)
    vp.rbox(p, 'Dark#seat', W(-0.42, 0.0, 0.96), (0.42, 0.48, 0.14), yaw=yaw, r=0.05, k=2)
    vp.rbox(p, 'Dark#seat', W(-0.63, 0.0, 1.2), (0.1, 0.46, 0.42), yaw=yaw, pitch=-8.0, r=0.04, k=2)
    lp = W(-0.98, 0.0, 1.22)
    vp.lathe(p, 'Metal#fl', lp, tuple(ey), [(0.0, -0.4), (0.1, -0.4), (0.16, -0.34), (0.17, -0.28), (0.17, 0.28), (0.16, 0.34), (0.1, 0.4),
                                            (0.0, 0.4)], seg=16)
    for s in (-1, 1):
        vp.rbox(p, 'Dark#fl', W(-0.98, s * 0.2, 1.12), (0.3, 0.05, 0.14), yaw=yaw, r=0.02, k=1)
    # o painel e o volante
    vp.rbox(p, 'Yellow#fl', W(0.22, 0.0, 0.9), (0.24, 0.82, 0.32), yaw=yaw, r=0.05, k=2)
    vp.tube(p, 'Dark#fl', [Vector(W(0.18, 0.0, 1.02)), Vector(W(0.02, 0.0, 1.3))], 0.03, seg=6)
    p.torus('Dark#fl', W(0.0, 0.0, 1.32), 'z', 0.17, 0.018, seg=16, rseg=5)
    # a grade de proteção: 4 colunas (as da frente inclinadas), o quadro de cima e as ripas
    tops = []
    for s in (-1, 1):
        a, b = Vector(W(0.33, s * 0.47, 0.72)), Vector(W(0.14, s * 0.47, 2.08))
        vp.tube(p, 'Dark#og', [a, b], 0.035, seg=6)
        c_, d_ = Vector(W(-1.02, s * 0.47, 1.0)), Vector(W(-0.98, s * 0.47, 2.08))
        vp.tube(p, 'Dark#og', [c_, d_], 0.035, seg=6)
        tops += [b, d_]
    vp.tube(p, 'Dark#og', [tops[0], tops[1]], 0.03, seg=6)
    vp.tube(p, 'Dark#og', [tops[2], tops[3]], 0.03, seg=6)
    vp.tube(p, 'Dark#og', [tops[0], tops[2]], 0.03, seg=6)
    vp.tube(p, 'Dark#og', [tops[1], tops[3]], 0.03, seg=6)
    for k in range(6):
        lx = -0.9 + 1.0 * k / 5
        vp.rbox(p, 'Dark#og', W(lx, 0.0, 2.09), (0.05, 0.9, 0.025), yaw=yaw, r=0.01, k=1)
    for s in (-1, 1):
        vp.rbox(p, 'Glass', W(0.18, s * 0.47, 1.9), (0.06, 0.1, 0.08), yaw=yaw, r=0.02, k=1)
    # a torre: os 2 trilhos de fora, os de dentro, as travessas, o cilindro, as correntes, os cilindros de inclinação
    mx = 0.78
    for s in (-1, 1):
        vp.rbox(p, 'Dark#mast', W(mx, s * 0.34, 0.1 + mast_h / 2), (0.1, 0.07, mast_h), yaw=yaw, r=0.01, k=1)
        vp.rbox(p, 'Metal#mast', W(mx + 0.04, s * 0.26, 0.15 + mast_h * 0.45), (0.08, 0.06, mast_h * 0.9), yaw=yaw, r=0.01, k=1)
        vp.tube(p, 'Metal#fl', [Vector(W(0.28, s * 0.44, 0.72)), Vector(W(mx - 0.05, s * 0.38, 1.02))], 0.035, seg=8)
        vp.rbox(p, 'Dark#chain', W(mx + 0.02, s * 0.12, fork_z + 1.0), (0.03, 0.035, 1.8), yaw=yaw, r=0.005, k=1)
    for z in (0.2, mast_h - 0.02):
        vp.rbox(p, 'Dark#mast', W(mx, 0.0, z), (0.1, 0.76, 0.1), yaw=yaw, r=0.02, k=1)
    p.cyl('Metal#fl', W(mx - 0.06, 0.0, 0.25 + mast_h * 0.38), tuple(ez), 0.05, mast_h * 0.72, seg=12, bevel=0.0)
    # o carro e o encosto de grade, os garfos
    fx = mx + 0.11
    vp.rbox(p, 'Dark#car', W(fx, 0.0, fork_z + 0.3), (0.06, 1.0, 0.44), yaw=yaw, r=0.01, k=1)
    for ly in (-0.45, -0.15, 0.15, 0.45):
        vp.rbox(p, 'Dark#car', W(fx, ly, fork_z + 0.82), (0.04, 0.04, 0.62), yaw=yaw, r=0.008, k=1)
    vp.rbox(p, 'Dark#car', W(fx, 0.0, fork_z + 1.12), (0.04, 1.0, 0.05), yaw=yaw, r=0.01, k=1)
    for s in (-1, 1):
        vp.rbox(p, 'Metal#fork', W(fx + 0.04, s * 0.3, fork_z + 0.28), (0.045, 0.12, 0.54), yaw=yaw, r=0.01, k=1)
        vp.rbox(p, 'Metal#fork', W(fx + 0.6, s * 0.3, fork_z + 0.022), (1.12, 0.12, 0.045), yaw=yaw, r=0.01, k=1)
    if load_pallet:
        pallet(p, *W(fx + 0.66, 0.0, 0.0)[:2], z=fork_z + 0.045, yaw=yaw, seed=seed)
    # o escapamento atrás e a luz de ré
    vp.tube(p, 'Dark#fl', [Vector(W(-0.8, 0.4, 0.95)), Vector(W(-0.8, 0.4, 1.55))], 0.025, seg=6)
    vp.rbox(p, 'Glass', W(-1.235, 0.42, 1.0), (0.02, 0.1, 0.06), yaw=yaw, r=0.01, k=1)


def pallet_jack(p, x, y, z=0.0, yaw=0.0):
    """A paleteira manual (a transpaleteira de aço vermelho-gasto no amarelo do kit): os 2 garfos com as rodinhas na ponta, a bomba
    hidráulica com a roda de esterço e o cabo em T inclinado. A frente (os garfos) no x local."""
    ex, ey, ez = vp.ypr_frame(yaw)
    C = Vector((x, y, z))
    for s_ in (-1, 1):
        vp.rbox(p, 'Yellow#fl', tuple(C + ex * 0.55 + ey * (s_ * 0.27) + ez * 0.055), (1.1, 0.16, 0.07), yaw=yaw, r=0.02, k=1)
        p.cyl('Dark#fl', tuple(C + ex * 1.03 + ey * (s_ * 0.27) + ez * 0.04), tuple(ey), 0.04, 0.1, seg=8, bevel=0.0)
    vp.rbox(p, 'Yellow#fl', tuple(C + ex * (-0.05) + ez * 0.18), (0.22, 0.66, 0.2), yaw=yaw, r=0.04, k=2)
    p.cyl('Metal#fl', tuple(C - ex * 0.12 + ez * 0.38), (0.0, 0.0, 1.0), 0.06, 0.32, seg=10, bevel=0.0)
    p.cyl('Dark#fl', tuple(C - ex * 0.16 + ez * 0.09), tuple(ey), 0.09, 0.12, seg=12, bevel=0.0)
    top = C - ex * 0.62 + ez * 1.05
    vp.tube(p, 'Yellow#fl', [C - ex * 0.14 + ez * 0.5, top], 0.022, seg=6)
    vp.tube(p, 'Dark#fl', [top - ey * 0.16, top + ey * 0.16], 0.02, seg=6)


# ===================================================================================================== o estêncil gasto
SEG = {'0': 'abcdef', '1': 'bc', '2': 'abged', '3': 'abgcd', '4': 'fgbc', '5': 'afgcd', '6': 'afgedc', '7': 'abc', '8': 'abcdefg',
       '9': 'abcdfg', '-': 'g'}


def stencil(p, origin, eu, ev, en, text, h, m='Paint#sten', seed=0, wear=0.18, t=0.004):
    """Os números de estêncil pintados e gastos (as pontes do molde abrem as quinas; uns pedaços saíram): origin = o canto de baixo
    à esquerda do texto na superfície, eu = a direita do texto, ev = o cima, en = pra fora. h = a altura do número."""
    O, U, Vv, N = Vector(origin), Vector(eu).normalized(), Vector(ev).normalized(), Vector(en).normalized()
    w = 0.58 * h
    st = 0.16 * h
    g = st * 0.55
    for ci, ch in enumerate(text):
        base = O + U * (ci * (w + 0.22 * h))
        rects = {
            'a': (g, w - g, h - st, h), 'd': (g, w - g, 0.0, st), 'g': (g, w - g, h / 2 - st / 2, h / 2 + st / 2),
            'f': (0.0, st, h / 2 + g, h - g), 'b': (w - st, w, h / 2 + g, h - g),
            'e': (0.0, st, g, h / 2 - g), 'c': (w - st, w, g, h / 2 - g)}
        for si, sname in enumerate(SEG.get(ch, '')):
            u0, u1, v0, v1 = rects[sname]
            k = seed * 37 + ci * 7 + si
            pieces = [(u0, u1, v0, v1)]
            if _hash(k, 1, 3) < 0.45:            # o segmento partido (a tinta que descascou no meio)
                if (u1 - u0) > (v1 - v0):
                    mid = u0 + (u1 - u0) * (0.35 + 0.3 * _hash(k, 2, 3))
                    pieces = [(u0, mid - 0.04 * h, v0, v1), (mid + 0.04 * h, u1, v0, v1)]
                else:
                    mid = v0 + (v1 - v0) * (0.35 + 0.3 * _hash(k, 2, 3))
                    pieces = [(u0, u1, v0, mid - 0.04 * h), (u0, u1, mid + 0.04 * h, v1)]
            for pi_, (a, b, c_, d) in enumerate(pieces):
                if _hash(k, pi_ + 5, 3) < wear:
                    continue
                P = []
                for vv in (c_, d):
                    for nn in (0.0, t):
                        for uu in (a, b):
                            q = base + U * uu + Vv * vv + N * nn
                            P.append((q.x, q.y, q.z))
                hexa(p, m, P, bevel=0.0)


# ===================================================================================================== a placa pintada à mão
def sign_board(p, x, y, yaw=0.0, w=1.5, h=0.9, z0=0.5, text='03', seed=0):
    """A placa de madeira pintada à mão no pé da pilha (o desenho 2): 2 paus fincados um pouco tortos, o tabuão de 3 tábuas com a
    fresta, pintado: a faixa amarela larga a pincel, a caixa oliva com a tampa amarela (o símbolo do suprimento) e o número de
    estêncil. A frente da placa olha pro x local (yaw)."""
    ex, ey, ez = vp.ypr_frame(yaw)
    C = Vector((x, y, 0.0))
    for s in (-1, 1):
        a = C + ey * (s * (w / 2 - 0.1)) - ex * 0.05
        lean = (_hash(seed, s + 2, 5) - 0.5) * 0.08
        rod(p, 'Wood#post', tuple(a + ez * -0.15), tuple(a + ez * (z0 + h + 0.12) + ey * lean), 0.05, seg=6)
    rod(p, 'Wood#post', tuple(C - ex * 0.05 + ey * (-w / 2 + 0.1) + ez * 0.35), tuple(C - ex * 0.05 + ey * (w / 2 - 0.1) + ez * (z0 - 0.05)),
        0.03, seg=5)
    for k in range(3):
        zc = z0 + h * (k + 0.5) / 3
        q = C + ez * zc + ey * ((_hash(seed, k, 7) - 0.5) * 0.04)
        p.obox('Wood#board', (q.x, q.y, q.z), (0.04, w + 0.06 * (_hash(seed, k, 8) - 0.5), h / 3 - 0.025), yaw=yaw,
               roll=(_hash(seed, k, 9) - 0.5) * 2.0)
    face = C + ex * 0.022
    # a faixa amarela a pincel (o contorno torto), a caixa oliva com a tampa amarela, o número
    rows = 6
    for k in range(rows):
        v0 = z0 + h * 0.62 + (h * 0.3) * k / rows
        j0 = (_hash(seed, k, 21) - 0.5) * 0.08
        j1 = (_hash(seed, k, 22) - 0.5) * 0.08
        a = face + ey * (-w / 2 + 0.08 + j0) + ez * v0
        P = []
        for vv in (0.0, h * 0.3 / rows + 0.004):
            for nn in (0.0, 0.004):
                for uu in (0.0, w - 0.16 - j0 + j1):
                    q = a + ey * uu + ez * vv + ex * nn
                    P.append((q.x, q.y, q.z))
        hexa(p, 'Yellow#paint', P, bevel=0.0)
    cc = face + ey * (-w / 2 + 0.36) + ez * (z0 + h * 0.3)
    for (m, du, dv, sw, sh) in (('Crate#paint', 0.0, 0.0, 0.42, 0.3), ('Yellow#paint', 0.0, 0.17, 0.46, 0.06)):
        P = []
        for vv in (-sh / 2, sh / 2):
            for nn in (0.004, 0.008):
                for uu in (-sw / 2, sw / 2):
                    q = cc + ey * (du + uu) + ez * (dv + vv) + ex * nn
                    P.append((q.x, q.y, q.z))
        hexa(p, m, P, bevel=0.0)
    stencil(p, face + ey * (w / 2 - 0.2) + ez * (z0 + 0.1) + ex * 0.004, -ey, ez, ex, text, 0.34, m='Paint#sten', seed=seed, wear=0.12)


# ===================================================================================================== o tambor e o poste de holofote
def drum(p, x, y, z=0.0, r=0.29, h=0.88, m='Rust#drum', lying=False, yaw=0.0):
    """O tambor de 200 L: o corpo com os 2 aros de rolar e as bordas viradas, a tampa com o bujão."""
    if lying:
        ex, _, _ = vp.ypr_frame(yaw)
        A = ex
        Cc = Vector((x, y, z + r + 0.01))
    else:
        A = Vector((0, 0, 1))
        Cc = Vector((x, y, z + h / 2))
    prof = [(0.0, -h / 2 + 0.015), (r - 0.02, -h / 2 + 0.015), (r - 0.012, -h / 2), (r + 0.004, -h / 2 + 0.005), (r + 0.004, -h / 2 + 0.03),
            (r, -h / 2 + 0.035)]
    for zz in (-h / 6, h / 6):
        prof += [(r, zz - 0.03), (r + 0.016, zz - 0.012), (r + 0.016, zz + 0.012), (r, zz + 0.03)]
    prof += [(r, h / 2 - 0.035), (r + 0.004, h / 2 - 0.03), (r + 0.004, h / 2 - 0.005), (r - 0.012, h / 2), (r - 0.02, h / 2 - 0.015),
             (0.0, h / 2 - 0.015)]
    vp.lathe(p, m, tuple(Cc), tuple(A), prof, seg=20, smooth=True, hard=40.0)
    if not lying:
        vp.lathe(p, 'Dark#bung', (x + 0.15, y + 0.04, z + h - 0.015), 'z', [(0.0, 0.0), (0.04, 0.0), (0.04, 0.012), (0.0, 0.012)], seg=8)


def flood_pole(p, x, y, z0=0.0, h=8.5, aim=0.0, n=4, footing=True):
    """O poste de holofote do pátio (o desenho 4): a sapata de concreto, o tubo de aço afinando com a porta de inspeção e o eletroduto,
    a escada de grampos, a cruzeta no alto com os holofotes (a caixa escura, a lente, a aleta, o suporte em U) virados pro pátio (aim =
    o giro pra onde olham)."""
    if footing:
        vp.rbox(p, 'Concrete#lo', (x, y, z0 + 0.2), (0.7, 0.7, 0.5), r=0.1, k=2)
        zb = z0 + 0.45
    else:
        zb = z0
    vp.lathe(p, 'Metal#pole', (x, y, zb), 'z', [(0.0, 0.0), (0.2, 0.0), (0.2, 0.03), (0.13, 0.05), (0.13, 0.4), (0.12, 0.45),
                                                 (0.085, h - 0.3), (0.075, h), (0.0, h)], seg=14)
    for (dx, dy) in ((0.15, 0.15), (-0.15, 0.15), (0.15, -0.15), (-0.15, -0.15)):
        vp.nut(p, 'Dark', (x + dx, y + dy, zb + 0.03), (0, 0, 1), r=0.025, h=0.04)
    ex, ey, ez = vp.ypr_frame(aim)
    vp.rbox(p, 'Metal#box', tuple(Vector((x, y, zb + 1.2)) + ex * 0.12), (0.08, 0.16, 0.3), yaw=aim, r=0.02, k=1)
    vp.tube(p, 'Dark#conduit', [Vector((x, y, zb + 0.5)) - ex * 0.13, Vector((x, y, zb + h - 0.4)) - ex * 0.1], 0.02, seg=5)
    for k in range(int((h - 2.5) / 0.38)):
        z = zb + 2.5 + k * 0.38
        q = Vector((x, y, z)) - ey * 0.11
        vp.tube(p, 'Metal#rung', vp.bent_bar([q - ex * 0.13, q - ex * 0.13 - ey * 0.1, q + ex * 0.13 - ey * 0.1, q + ex * 0.13],
                                              R=0.02, n=2), 0.012, seg=4)
    zt = zb + h
    arm = Vector((x, y, zt + 0.05))
    vp.tube(p, 'Metal#pole', [arm - ey * 0.75, arm + ey * 0.75], 0.04, seg=8)
    vp.tube(p, 'Metal#pole', [arm - ey * 0.5 - ez * 0.35, arm, arm + ey * 0.5 - ez * 0.35], 0.022, seg=5)
    for k in range(n):
        u = -0.6 + 1.2 * k / max(1, n - 1)
        c = arm + ey * u + ex * 0.16 + ez * 0.05
        vp.rbox(p, 'Dark#lamp', tuple(c), (0.22, 0.32, 0.26), yaw=aim, pitch=-35.0, r=0.03, k=2)
        fx, fy, fz = vp.ypr_frame(aim, -35.0)
        vp.rbox(p, 'Glass', tuple(c + fx * 0.115), (0.012, 0.26, 0.2), yaw=aim, pitch=-35.0, r=0.02, k=1)
        vp.rbox(p, 'Dark#lamp', tuple(c - fx * 0.13), (0.03, 0.3, 0.24), yaw=aim, pitch=-35.0, r=0.01, k=1)
        vp.tube(p, 'Dark#lamp', vp.bent_bar([c - ey * 0.19 - ez * 0.05, c - ey * 0.19 - ex * 0.12 + ez * 0.12,
                                             c + ey * 0.19 - ex * 0.12 + ez * 0.12, c + ey * 0.19 - ez * 0.05], R=0.03, n=2), 0.015, seg=4)


# ===================================================================================================== o chão
def ground_pad(p, x0, x1, y0, y1, r=1.5, h=0.045, fall=1.2, step=0.6, seed=0, m='Earth#pad', ruts=(), z0=-0.05):
    """O chão batido que ACOMPANHA A FORMA (nada de laje): a lente de terra no retângulo de cantos redondos (r), alta no miolo (h) e
    morrendo no chão na borda (enterrada em z0, 'fall' metros de rampa, com o contorno ondulado pelo ruído). Só o tampo (o fundo nunca
    aparece). ruts = [('y', x_centro, meia_bitola, y_a, y_b)...]: os 2 sulcos rasos das rodas e a crista entre eles (a faixa dos
    caminhões)."""
    nx = max(2, int((x1 - x0) / step))
    ny = max(2, int((y1 - y0) / step))
    b = p.bm(m, 0.0)
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    hx, hy = (x1 - x0) / 2, (y1 - y0) / 2

    def sd(x, y):
        qx, qy = abs(x - cx) - (hx - r), abs(y - cy) - (hy - r)
        out = math.hypot(max(qx, 0.0), max(qy, 0.0)) + min(max(qx, qy), 0.0) - r
        return out + 0.35 * (fbm(x * 0.35 + seed, y * 0.35 - seed, seed) - 0.5)
    grid = []
    for j in range(ny + 1):
        row = []
        for i in range(nx + 1):
            x = x0 + (x1 - x0) * i / nx
            y = y0 + (y1 - y0) * j / ny
            d = -sd(x, y)
            z = z0 + (h - z0) * smooth(0.0, fall, d) + 0.012 * (fbm(x * 0.9, y * 0.9, seed + 3) - 0.5) * smooth(0.0, fall, d)
            for (ax, c, g, a, bb) in ruts:
                if ax == 'y' and a - 1.0 <= y <= bb + 1.0:
                    t = smooth(a - 1.0, a + 1.0, y) * (1.0 - smooth(bb - 1.0, bb + 1.0, y))
                    for s in (-1, 1):
                        z -= t * 0.022 * math.exp(-((x - (c + s * g)) / 0.2) ** 2)
                        z += t * 0.01 * math.exp(-((x - (c + s * (g + 0.42))) / 0.18) ** 2)
                    z += t * 0.006 * math.exp(-((x - c) / 0.4) ** 2)
            row.append(b.verts.new(V(x, y, z)))
        grid.append(row)
    for j in range(ny):
        for i in range(nx):
            b.faces.new([grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]]).smooth = True


def lane_pad(p, xc, y0, y1, half=2.4, seed=0):
    """A faixa de terra batida dos caminhões (entra numa ponta, sai na outra): a lente comprida com os 2 sulcos das rodas (a bitola do
    caminhão, ~1,9 m) e as cristas de areia empurrada; morre no chão nas duas pontas."""
    ground_pad(p, xc - half, xc + half, y0, y1, r=min(half, 2.0), h=0.025, fall=2.2, step=0.5, seed=seed + 40, m='Earth#lane',
               ruts=[('y', xc, 0.95, y0 + 2.0, y1 - 2.0)])


def scatter(p, x0, x1, y0, y1, n, seed=0, keep_out=()):
    """O miúdo do chão de um pátio de carga (o que tira o "chão de maquete"): as tábuas soltas e quebradas dos paletes, as lascas, as
    pedras, uma cinta de carga arrebentada. keep_out = [(x0, x1, y0, y1)...]: onde não cai nada (as pilhas, a faixa)."""
    k = 0
    tries = 0
    while k < n and tries < n * 12:
        tries += 1
        x = x0 + (x1 - x0) * _hash(tries, seed, 61)
        y = y0 + (y1 - y0) * _hash(tries, seed, 62)
        if any(a - 0.3 <= x <= b + 0.3 and c - 0.3 <= y <= d + 0.3 for (a, b, c, d) in keep_out):
            continue
        r = _hash(tries, seed, 63)
        yaw = _hash(tries, seed, 64) * 180.0
        if r < 0.45:
            L = 0.5 + 0.7 * _hash(tries, seed, 65)
            p.obox('Wood#plank', (x, y, 0.012), (L, 0.1, 0.024), yaw=yaw, roll=(_hash(tries, seed, 66) - 0.5) * 6)
            if _hash(tries, seed, 67) < 0.4:
                p.obox('Wood#plank', (x + 0.1, y - 0.06, 0.036), (L * 0.6, 0.1, 0.024), yaw=yaw + 25, pitch=4.0)
        elif r < 0.8:
            s_ = 0.08 + 0.12 * _hash(tries, seed, 68) ** 2
            p.obox('Plaster#stone', (x, y, s_ * 0.3), (s_ * 1.3, s_, s_ * 0.8), yaw=yaw, pitch=(_hash(tries, seed, 69) - 0.5) * 30, bevel=0.03)
        else:
            a = Vector((x, y, 0.01))
            d = Vector((math.cos(math.radians(yaw)), math.sin(math.radians(yaw)), 0.0))
            pts = [a + d * (0.25 * i) + Vector((-d.y, d.x, 0.0)) * (0.08 * math.sin(i * 1.7 + seed)) for i in range(6)]
            vp.tube(p, 'Dark#strap', [Vector((q.x, q.y, 0.012)) for q in pts], 0.012, seg=4)
        k += 1


# ===================================================================================================== a folha da prévia
def write_preview(out, prefix, stock, three_quarter=(32.0, 215.0), usa_at=(0.0, 0.0, 0.0), truck=None, game_fov=(50.0, 30.0),
                  extra_views=()):
    """As folhas da prévia (Game/Tools/model_preview.py com PREVIEW_SPEC): preview.json = o estoque CHEIO (as vistas e a câmera do
    jogo), preview_half.json = o estoque PELA METADE (só a câmera do jogo; o que sobra é o fim da ordem: o jogo esconde as primeiras).
    truck = (x, y, giro): o caminhão de suprimento da USA na faixa (a régua da faixa)."""
    order = stock.ordered()
    full = [[round(q[0], 3), round(q[1], 3), round(q[2], 3), round(q[3], 2)] for q in order]
    half = full[len(full) // 2:]
    for name, lst in (('preview_stock_full.json', full), ('preview_stock_half.json', half)):
        with open(os.path.join(out, name), 'w', encoding='utf-8') as f:
            json.dump({'crate': STOCK_CRATE, 'heap_m': lst}, f)
    parts = [[prefix + '_Base', [0, 0, 0]]]
    if truck:
        tx, ty, tyaw = truck
        for n in ('Hull', 'Wheels'):
            parts.append(['captures/game/models/usa_supply_truck_v2/SM_USA_SupplyTruck_' + n, [tx, -ty, 0.0], tyaw])
    views = [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1400, 1400]],
             ['three_quarter', 'persp', three_quarter[0], three_quarter[1], [1500, 1100], 30.0, 0.92]] + [list(v) for v in extra_views]
    game = [['game_cam_close', 37.5, 200.0, game_fov[1], 0.8], ['game_cam_high', 55.0, 200.0, game_fov[1], 0.8]]
    spec = {'parts': parts, 'layout': 'preview_stock_full.json', 'views': views, 'usa_at': list(usa_at), 'game': game}
    with open(os.path.join(out, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, indent=1)
    spec = dict(spec, layout='preview_stock_half.json', views=[], game=game[:1])
    with open(os.path.join(out, 'preview_half.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, indent=1)
