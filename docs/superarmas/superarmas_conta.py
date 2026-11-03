# A conta das superarmas: o que uma salva faz numa base de referência (rascunho de estudo, não é o jogo).
# Base: o layout típico de uma partida (prédios do Units.json, posições em metros, CC na origem).
import math, random, json, sys

def base(faction='usa'):
    reactor_hp = {'usa': 800, 'china': 1500, 'russia': 1500, 'gla': 0}[faction]
    B = [  # nome, x, y, largura, profundidade, vida, custo
        ('CC', 0, 0, 25, 29, 5000, 2000),
        ('War Factory', 42, -8, 22, 25, 2000, 2000),
        ('Supply Center', -46, 16, 19, 19, 2000, 1500),
        ('Barracks', 36, 36, 23, 19, 1000, 600),
        ('Tech', -42, -26, 20, 20, 1500, 2500),
        ('Airfield', 84, 34, 40, 32, 2000, 2500),
        ('Defesa A', 64, -46, 8, 8, 1000, 1000),
        ('Defesa B', -72, 48, 8, 8, 1000, 1000),
        ('Superarma', -80, -12, 30, 30, 4000, 5000),   # 4 000 de vida, como no ZH
    ]
    if reactor_hp:
        for i, x in enumerate((-22, -8, 6)):
            B.append((f'Usina {i+1}', x, -42, 10, 13, reactor_hp, 900))
    U = []
    for i in range(8):   # tanques parados perto da fábrica
        U.append(('tanque', 58 + (i % 4) * 7, -24 + (i // 4) * 9, 480, 900, 'veh'))
    for i in range(10):  # soldados perto do quartel
        U.append(('soldado', 30 + (i % 5) * 3, 52 + (i // 5) * 3, 120, 225, 'inf'))
    return B, U

def dist_rect(px, py, b):
    _, x, y, w, d = b[:5]
    dx = max(abs(px - x) - w / 2, 0); dy = max(abs(py - y) - d / 2, 0)
    return math.hypot(dx, dy)

def falloff(d, prof):
    # prof: lista de (raio, dano) decrescente em dano; linear entre os pontos
    if d <= prof[0][0]: return prof[0][1]
    for (r0, a0), (r1, a1) in zip(prof, prof[1:]):
        if d <= r1: return a0 + (a1 - a0) * (d - r0) / (r1 - r0)
    return 0.0

ARMOR = {  # multiplicador por tipo de dano (o triângulo do ELENCO.md §2 e os novos)
    'explosive': {'structure': 1.0, 'veh': 1.0, 'inf': 1.0},
    'kinetic':   {'structure': 1.0, 'veh': 1.0, 'inf': 0.6},
    'poison':    {'structure': 0.02, 'veh': 0.25, 'inf': 1.0},
    'radiation': {'structure': 0.1, 'veh': 0.4, 'inf': 1.0},
    'drone':     {'structure': 0.8, 'veh': 1.0, 'inf': 1.0},
}

def apply(hits, B, U):
    """hits: lista de (x, y, perfil, tipo). Devolve o valor destruído e a vida que sobra."""
    hpB = {b[0]: float(b[5]) for b in B}; hpU = [float(u[3]) for u in U]
    for (x, y, prof, kind) in hits:
        for b in B:
            hpB[b[0]] -= falloff(dist_rect(x, y, b), prof) * ARMOR[kind]['structure']
        for i, u in enumerate(U):
            d = math.hypot(x - u[1], y - u[2])
            hpU[i] -= falloff(d, prof) * ARMOR[kind][u[5]]
    deadB = [b for b in B if hpB[b[0]] <= 0]
    deadU = [u for i, u in enumerate(U) if hpU[i] <= 0]
    val = sum(b[6] for b in deadB) + sum(u[4] for u in deadU)
    total = sum(b[6] for b in B) + sum(u[4] for u in U)
    cc = max(hpB['CC'], 0) / 5000
    return val, total, [b[0] for b in deadB], len(deadU), cc

# ---- as armas (a proposta; os números são a faixa inicial, pra medir no jogo) ----
RV = [(6, 1300), (16, 350), (20, 0)]                       # USA: cada ogiva de reentrada
NUKE = [(39, 4000), (78, 1300), (148, 0)]                  # ~5 t: os anéis 1 : 2 : 3,8 da física       # Rússia: o ar-explosão tático
NUKE_RAD = [(40, 15 * 60), (52, 0)]                         # radiação: 60 s a 15/s dentro de 40 m
SCUD = [(10, 1000), (21, 350), (40, 0)]                    # ~100 kg de TNT por SCUD                       # GLA: cada SCUD (explosivo)
TOXIN = [(20, 15 * 40), (40, 0)]                            # a nuvem: 25 s a 22/s
DRONE = [(2.5, 100), (6, 0)]                                 # China: cada drone

def usa(B, U, pattern):
    return [(x, y, RV, 'kinetic') for (x, y) in pattern]

def russia(B, U, x, y):
    return [(x, y, NUKE, 'explosive'), (x, y, NUKE_RAD, 'radiation')]

def gla(B, U, aim, n=9, scatter=11.0, intercept=0, seed=1):
    rnd = random.Random(seed); hits = []
    pts = [(aim[0] + dx, aim[1] + dy) for dx in (-20, 0, 20) for dy in (-20, 0, 20)][:n]
    pts = pts[intercept:]
    for (x, y) in pts:
        x += rnd.gauss(0, scatter); y += rnd.gauss(0, scatter)
        hits += [(x, y, SCUD, 'explosive'), (x, y, TOXIN, 'poison')]
    return hits

def china(B, U, aim, drones=70, radius=75, shot_down=0):
    """os drones escolhem os alvos dentro do raio: o mais caro por vida primeiro, o bastante pra matar."""
    n = drones - shot_down
    alvos = []
    for b in B:
        if math.hypot(b[1] - aim[0], b[2] - aim[1]) <= radius:
            alvos.append((b[6] / b[5], b[1], b[2], b[5], ARMOR['drone']['structure']))
    for u in U:
        if math.hypot(u[1] - aim[0], u[2] - aim[1]) <= radius:
            alvos.append((u[4] / u[3], u[1], u[2], u[3], ARMOR['drone'][u[5]]))
    alvos.sort(reverse=True); hits = []
    for (_, x, y, hp, mul) in alvos:
        k = math.ceil(hp / (DRONE[0][1] * mul))
        k = min(k, n)
        hits += [(x, y, DRONE, 'drone')] * k; n -= k
        if n <= 0: break
    return hits

def report(name, hits, B, U, custo):
    val, total, dead, du, cc = apply(hits, B, U)
    print(f'{name:44s} destruiu ${val:6.0f} de ${total} ({100*val/total:4.1f}%), custo da salva ${custo}; '
          f'prédios: {", ".join(dead) or "-"}; unidades {du}; CC com {100*cc:.0f}%')

if __name__ == '__main__':
    B, U = base('usa')
    # USA: o jogador bom põe as 6 ogivas nos prédios caros (a fábrica, a tecnologia, as usinas)
    bom = [(36, -8), (48, -8), (-15, -42), (-1, -42)]   # 4 ogivas: a fábrica e as usinas
    anel = [(22 * math.cos(a), 22 * math.sin(a)) for a in [i * math.pi / 2 for i in range(4)]]
    report('USA, 4 ogivas bem miradas (fábrica, usinas)', usa(B, U, bom), B, U, 2500)
    report('USA, anel no CC', usa(B, U, anel), B, U, 2500)
    report('USA, nos tanques', usa(B, U, [(58, -24), (72, -24), (58, -15), (72, -15)]), B, U, 2500)
    report('Rússia, no meio da base (0,-10)', russia(B, U, 0, -10), B, U, 3500)
    report('Rússia, na fábrica e nos tanques', russia(B, U, 55, -15), B, U, 3500)
    for icpt in (0, 2, 4):
        report(f'GLA, 9 SCUDs na fábrica, {icpt} derrubados', gla(B, U, (45, -10), intercept=icpt), B, U, 1800)
    report('GLA, 9 SCUDs no quartel (soldados)', gla(B, U, (36, 45)), B, U, 1800)
    for sd in (0, 20, 35):
        report(f'China, 70 drones no meio, {sd} derrubados', china(B, U, (10, -10), shot_down=sd), B, U, 1500)
    Bc, Uc = base('china')
    report('Rússia na base da China (usinas 1500)', russia(Bc, Uc, 0, -10), Bc, Uc, 3500)
    report('USA bem mirada na base da China', usa(Bc, Uc, bom), Bc, Uc, 2500)
