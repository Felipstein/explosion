# A conta dos poderes do general: o que um uso faz, na mesma base de referência das superarmas (rascunho de estudo).
# Reaproveita a base, o alcance do dano e o triângulo de blindagem de docs/superarmas/superarmas_conta.py.
# Os números dos poderes do ZH saem do docs/poderes/ESTUDO_ZH.md (1 unidade do ZH = 0,2633 m); os nossos, do Units.json.
import math, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'superarmas'))
from superarmas_conta import base, apply, ARMOR, falloff, dist_rect

ARMOR.setdefault('small_arms', {'structure': 0.5, 'veh': 0.25, 'inf': 1.0})   # o fuzil do ZH (ELENCO.md §2)
ARMOR.setdefault('gattling', {'structure': 0.1, 'veh': 0.1, 'inf': 1.0})      # a rotativa (ELENCO.md §2)

def splash(full, edge, dmg, edge_pct=50):
    """o dano em área do Units.json: cheio até full, edge_pct% na borda, nada depois."""
    return [(full, dmg), (edge, dmg * edge_pct / 100), (edge + 0.01, 0)]

def sunflower(cx, cy, n, r):
    g = math.pi * (3 - math.sqrt(5))
    return [(cx + r * math.sqrt((i + 0.5) / n) * math.cos(i * g), cy + r * math.sqrt((i + 0.5) / n) * math.sin(i * g)) for i in range(n)]

def line(cx, cy, n, length, ang=0.0):
    return [(cx + (i / (n - 1) - 0.5) * length * math.cos(ang), cy + (i / (n - 1) - 0.5) * length * math.sin(ang)) for i in range(n)]

# ---- os poderes da USA (o rumo de §8.1; os números são a faixa inicial) ----
CRUISE = splash(3, 6, 150)                    # usa_cruise_missile do Units.json: 150, cheio 3 m, 50% a 6 m
A10_MISSILE = [(13.2, 200), (13.21, 0)]       # ZH: 6 mísseis de 200 em raio 50 u por A-10
A10_VULCAN = [(1.5, 10), (1.51, 0)]           # ZH: 10 por tiro a cada 60 ms numa faixa de 118 m (~25 tiros por passada)
SPECTRE_HOW = [(6.6, 80), (6.61, 0)]          # ZH: obus de 80 em raio 25 u a cada 0,3 s, 15 s
SPECTRE_GAT = [(1.0, 90), (1.01, 0)]          # ZH: 90 por tiro a cada 0,1 s (até 3x no fogo contínuo)
DAISY = [(26.3, 2000), (26.31, 0)]            # ZH: a Fuel Air Bomb, 2000 em raio 100 u
MOAB = [(39.5, 2000), (52.7, 250), (52.71, 0)]  # ZH: a MOAB, 2000 em 150 u e a onda de 250 até 200 u
JDAM = [(3, 2600), (8, 900), (14, 0)]         # novo: uma bomba guiada de 2 000 lb, mata um prédio de 2 000 e arranha o vizinho
# a proposta (calibrada pela régua de §8.1: 1 ponto ~ um tanque, 2 ~ um prédio, 3 ~ metade de uma superarma)
A10_MISSILE_P = splash(3, 6, 200)             # o Maverick é antitanque e preciso: o raio de 13 m do ZH apagava um exército
A10_VULCAN_P = [(2.0, 30), (2.01, 0)]         # o GAU-8 na faixa: 40 tiros de 30 (perfurante) por passada
SPECTRE_GAT_P = [(1.0, 30), (1.01, 0)]        # a rotativa de 25 mm: 30 por tiro, o fogo contínuo sem o 3x
MOAB_P = [(14, 2400), (30, 700), (40, 0)]      # o poder máximo: metade de uma superarma, não uma superarma
# (03/11, a ideia dele no lugar do Cruise Missile) a artilharia naval: o nível muda o navio
NAVAL_127 = splash(2.5, 5, 130)               # a granada de 127 mm do destróier
NAVAL_406 = [(4, 800), (10, 250), (14, 0)]    # a granada de 406 mm do encouraçado (a salva de 9)

def cruise(x, y, n=6, r=12):
    return [(px, py, CRUISE, 'explosive') for (px, py) in sunflower(x, y, n, r)]

def naval(x, y, level=1, ang=0.0):
    """I: um destróier, 12 granadas numa fileira de 40 m que anda; II: três destróieres, 3 fileiras de 12 lado a lado;
    III: o encouraçado, a salva de 9 granadas de 406 mm numa elipse de ~30 x 18 m (a dispersão de uma salva)."""
    if level == 3:
        pts = [(x + 15 * math.cos(a) * r, y + 9 * math.sin(a) * r) for (a, r) in
               [(0, 0), (0.0, 1), (2.1, 1), (4.2, 1), (1.0, 0.55), (3.1, 0.55), (5.2, 0.55), (0.5, 0.85), (3.7, 0.8)]]
        return [(px, py, NAVAL_406, 'explosive') for (px, py) in pts]
    hits = []
    for k in range(1 if level == 1 else 3):
        off = (k - (0 if level == 1 else 1)) * 8
        hits += [(px - off * math.sin(ang), py + off * math.cos(ang), NAVAL_127, 'explosive') for (px, py) in line(x, y, 12, 40, ang)]
    return hits

def a10(x, y, planes=1, ang=0.0):
    hits = []
    for p in range(planes):
        off = (p - (planes - 1) / 2) * 9          # os aviões em fila lado a lado, 9 m entre eles
        ox, oy = x - off * math.sin(ang), y + off * math.cos(ang)
        hits += [(px, py, A10_MISSILE, 'explosive') for (px, py) in line(ox, oy, 6, 24, ang)]
        hits += [(px, py, A10_VULCAN, 'kinetic') for (px, py) in line(ox, oy, 25, 118, ang)]
    return hits

def a10p(x, y, planes=1, ang=0.0):
    hits = []
    for p in range(planes):
        off = (p - (planes - 1) / 2) * 9
        ox, oy = x - off * math.sin(ang), y + off * math.cos(ang)
        hits += [(px, py, A10_MISSILE_P, 'explosive') for (px, py) in line(ox, oy, 6, 24, ang)]
        hits += [(px, py, A10_VULCAN_P, 'kinetic') for (px, py) in line(ox, oy, 40, 80, ang)]
    return hits

def spectre(B, U, x, y, secs=15, radius=26, gat_prof=None):
    """o jogador escolhe: o obus nos veículos e nos prédios, a rotativa nos soldados, o mais caro por vida primeiro."""
    hits = []
    alvos = sorted([u for u in U if math.hypot(u[1] - x, u[2] - y) <= radius], key=lambda u: -u[4] / u[3])
    how = int(secs / 0.3); gat = int(secs / 0.1)
    inf = [u for u in alvos if u[5] == 'inf']; veh = [u for u in alvos if u[5] == 'veh']
    for i in range(how):
        if veh:
            u = veh[min(i // 7, len(veh) - 1)]; hits.append((u[1], u[2], SPECTRE_HOW, 'explosive'))
        else:
            hits.append((x, y, SPECTRE_HOW, 'explosive'))
    for i in range(gat):
        if inf:
            u = inf[min(i // 3, len(inf) - 1)]; hits.append((u[1], u[2], gat_prof or SPECTRE_GAT, 'gattling'))
    return hits

def bomb(x, y, prof):
    return [(x, y, prof, 'explosive')]

def campo():
    """o exército no campo: 8 tanques e 10 soldados juntos, longe de prédio (a luta do meio do mapa)."""
    U = [('tanque', 300 + (i % 4) * 8, 300 + (i // 4) * 9, 480, 900, 'veh') for i in range(8)]
    U += [('soldado', 296 + (i % 5) * 3, 318 + (i // 5) * 3, 120, 225, 'inf') for i in range(10)]
    return [('CC', 2000, 2000, 1, 1, 5000, 0)], U   # o CC longe, só pra conta não reclamar

def damage_value(hits, B, U):
    """o estrago em dólares: a fração da vida tirada de cada coisa × o custo dela (o que mata conta inteiro)."""
    hpB = {b[0]: float(b[5]) for b in B}; hpU = [float(u[3]) for u in U]
    for (x, y, prof, kind) in hits:
        for b in B: hpB[b[0]] -= falloff(dist_rect(x, y, b), prof) * ARMOR[kind]['structure']
        for i, u in enumerate(U): hpU[i] -= falloff(math.hypot(x - u[1], y - u[2]), prof) * ARMOR[kind][u[5]]
    v = sum(b[6] * min(1, max(0, (b[5] - hpB[b[0]]) / b[5])) for b in B)
    v += sum(u[4] * min(1, max(0, (u[3] - hpU[i]) / u[3])) for i, u in enumerate(U))
    return v

def report(name, hits, B, U, pts, recharge_s):
    val, total, dead, du, cc = apply(hits, B, U)
    dv = damage_value(hits, B, U)
    print(f'{name:40s} {pts} pt {recharge_s // 60}:{recharge_s % 60:02d} | mata ${val:5.0f}, estrago ${dv:5.0f}'
          f' (${dv / (recharge_s / 60):4.0f}/min) | {", ".join(dead) or "-"}; unidades {du}')

if __name__ == '__main__':
    B, U = base('usa'); Bf, Uf = campo()
    print('--- na base de referência (as fábricas, as usinas, 8 tanques e 10 soldados) ---')
    report('Cruise I (6 Tomahawks) na fábrica', cruise(42, -8), B, U, 2, 180)
    report('Cruise II (9) na fábrica', cruise(42, -8, 9, 14), B, U, 3, 180)
    report('Cruise I nas usinas', cruise(-8, -42), B, U, 2, 180)
    report('A-10 I na fábrica', a10(42, -8), B, U, 2, 240)
    report('A-10 II na fábrica', a10(42, -8, 2), B, U, 3, 240)
    report('A-10 III na fábrica', a10(42, -8, 3), B, U, 4, 240)
    report('A-10 I nas usinas (a fileira)', a10(-8, -42), B, U, 2, 240)
    report('Precision Strike na fábrica', bomb(42, -8, JDAM), B, U, 2, 240)
    report('Precision Strike na tecnologia', bomb(-42, -26, JDAM), B, U, 2, 240)
    report('AC-130 nos tanques da fábrica', spectre(B, U, 68, -20), B, U, 3, 300)
    report('Fuel Air Bomb no meio (CC e fábrica)', bomb(22, -6, DAISY), B, U, 3, 360)
    report('MOAB no meio (CC e fábrica)', bomb(22, -6, MOAB), B, U, 3, 420)
    report('MOAB nas usinas e na tecnologia', bomb(-20, -36, MOAB), B, U, 3, 420)
    print('--- a proposta, na base ---')
    report('A-10 I (proposta) na fábrica', a10p(42, -8), B, U, 2, 240)
    report('A-10 II (proposta) na fábrica', a10p(42, -8, 2), B, U, 3, 240)
    report('A-10 III (proposta) na fábrica', a10p(42, -8, 3), B, U, 4, 240)
    report('A-10 II (proposta) nos tanques', a10p(65, -20, 2), B, U, 3, 240)
    report('AC-130 12 s (proposta) nos tanques', spectre(B, U, 68, -20, 12, gat_prof=SPECTRE_GAT_P), B, U, 3, 300)
    report('MOAB (proposta) no meio', bomb(22, -6, MOAB_P), B, U, 3, 420)
    report('MOAB (proposta) nas usinas e na tecnologia', bomb(-20, -36, MOAB_P), B, U, 3, 420)
    report('Naval I (destróier) na fábrica', naval(42, -8), B, U, 1, 180)
    report('Naval I nas usinas', naval(-8, -42), B, U, 1, 180)
    report('Naval II (3 destróieres) na fábrica', naval(42, -8, 2), B, U, 2, 180)
    report('Naval II nas usinas', naval(-8, -42, 2), B, U, 2, 180)
    report('Naval III (encouraçado) no meio', naval(22, -6, 3), B, U, 3, 300)
    report('Naval III na fábrica', naval(42, -8, 3), B, U, 3, 300)
    report('Naval III nas usinas e na tecnologia', naval(-20, -36, 3), B, U, 3, 300)
    print('--- no campo (8 tanques e 10 soldados juntos) ---')
    report('Cruise I', cruise(312, 306), Bf, Uf, 2, 180)
    report('A-10 I', a10(312, 306), Bf, Uf, 2, 240)
    report('A-10 II', a10(312, 306, 2), Bf, Uf, 3, 240)
    report('AC-130', spectre(Bf, Uf, 312, 310), Bf, Uf, 3, 300)
    report('MOAB', bomb(312, 310, MOAB), Bf, Uf, 3, 420)
    report('A-10 I (proposta)', a10p(312, 306), Bf, Uf, 2, 240)
    report('A-10 II (proposta)', a10p(312, 306, 2), Bf, Uf, 3, 240)
    report('A-10 III (proposta)', a10p(312, 306, 3), Bf, Uf, 4, 240)
    report('AC-130 12 s (proposta)', spectre(Bf, Uf, 312, 310, 12, gat_prof=SPECTRE_GAT_P), Bf, Uf, 3, 300)
    report('MOAB (proposta)', bomb(312, 310, MOAB_P), Bf, Uf, 3, 420)
    report('Precision Strike (num tanque)', bomb(300, 300, JDAM), Bf, Uf, 2, 240)
    report('Naval I', naval(312, 306), Bf, Uf, 1, 180)
    report('Naval II', naval(312, 306, 2), Bf, Uf, 2, 180)
    report('Naval III', naval(312, 310, 3), Bf, Uf, 3, 300)
