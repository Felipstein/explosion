"""As marcas de explosão no chão (13/10): os atlas de variantes que o jogo carimba no caderno do chão.

Pedido do Felipe: "vai ter que machucar o terreno, com os detalhes bem detalhados e lindos, bem estudado". O estudo,
a revisão e os números estão em docs/CHAO_DEFORMAVEL.md ("A cratera e o queimado de morte"). Dois tipos:

- **crater**: a explosão que não deixa carcaça em cima (o tiro no chão, o prédio, a artilharia que vier). Em camadas,
  do centro pra fora (a crítica do Look em 13/10):
  (a) a bacia ~parabólica (Glasstone & Dolan §6.70–6.75), com 2–3 lóbulos e alongada pro lado do tiro, nunca um
      círculo; a sombra do sol na parede de lá vende o "buraco escuro";
  (b) o halo de terra revirada ESCURA até ~1,6·R (no alvo o entorno é marrom-escuro), com a borda recortada e definida;
  (c) a borda levantada e quebrada, com os torrões claros;
  (d) os raios finos e desiguais até 2–3·R, mais longos pro lado do tiro, com falhas; uns claros (areia), uns escuros;
  (e) os torrões em lei de potência (poucos grandes, muitos pequenos, a partir de 20 cm: menor que isso cintila na
      câmera do RTS), em grupos, com falhas.
- **wreck**: a morte do tanque. A carcaça fica (é cobertura) e cobre o miolo: o que se lê é o avental de terra
  queimada e combustível 1,5–3 m além do casco, com a borda irregular e definida, a brasa nos primeiros ~30 s, a
  areia espirrada pra fora e os torrões e lascas (a revisão de 13/10: no alvo as bacias não ficam embaixo de carcaça).

Os canais do atlas (RGBA de 16 bits, variantes lado a lado; o +X do carimbo é pra longe de quem atirou, ou a
frente do casco no wreck):
  R = a altura com sinal (0,5 = zero, h = (R − 0,5)·2·RANGE_M), que o jogo soma no caderno de altura (R16F);
  G = o escuro (terra revirada, fuligem, combustível): a MF do Look escurece e tira a cor até ~45%;
  B = o calor (a brasa): o jogo apaga em ~30 s;
  A = o claro (a areia espirrada, a borda e os torrões).
O jogo compõe sem depender da ordem (altura somada, o resto pelo máximo): o redesenho dá o mesmo que a gravação.

Saída: Game/Config/RTS/Ground/<tipo>_<opção>.png + .json, e a prévia em captures/game/crateras/ (sombreada com o sol
do jogo, na inclinação da câmera do RTS, com o tanque de régua e o recorte do alvo ao lado).

Rodar: python Game/Tools/bake_craters.py [opção ...]   (sem nada: todas)
"""
import json
import math
import os
import struct
import sys
import zlib

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.dirname(HERE)
ROOT = os.path.dirname(GAME)
OUT = os.environ.get('BAKE_OUT') or os.path.join(GAME, 'Config', 'RTS', 'Ground')
# (13/10, o plano com o Look) --ramps: o G e o A como rampas de distância (G < 0,5 = fora, a rampa até ~12 cm da borda;
# G = 0,5 + 0,5·intensidade dentro), pra MF cortar a borda nítida com fwidth; o JSON diz "encoding": "ramp"
RAMPS = '--ramps' in sys.argv
RAMP_M = 0.12
PREVIEW = os.path.join(ROOT, 'captures', 'game', 'crateras')
TARGET = os.path.join(ROOT, 'docs', 'concept', 'scenes', 'desert_golden_hour', 'best', 'street_battle_usa_china_gemini.webp')

RANGE_M = 0.8           # a faixa da altura no atlas (±80 cm, 16 bits)
VARIANTS = 6
# o texel de antes (o borrão foi acertado nele)
KIND_NOW = ['crater']
BASE_PX = {'crater': 14.0 / 320, 'wreck': 20.0 / 384, 'blot': 2.0 / 160}
KINDS = {
    # SIZE texels cobrindo SPAN_M: ~4,4 cm por texel na cratera, ~5,2 no avental
    # (13/10: a 4,4 cm, carimbado girado no caderno de 4,7 cm e lido em bilinear, eram dois passa-baixa: ~9 cm
    # efetivos; a metade do texel do caderno faz o carimbo reduzir em vez de borrar)
    # (13/10: a 2 cm, a mesma resolução da prova aprovada na maquete do Look)
    'crater': dict(size=700, span=14.0),
    'wreck': dict(size=850, span=20.0),
    # a mancha do queimado sem cratera (o tiro que chamusca, a pegada do prédio): raio 1 m em escala 1 (o jogo escala pelo
    # raio da marca); antes era o carimbo do Canvas somado, que não convive com as rampas (13/10)
    'blot': dict(size=160, span=2.0),
}
HULL = (7.9, 3.6)       # o casco do tanque USA (m)

# as opções (como as trilhas A/B/C do Look); a B é a do estudo e das duas críticas
OPTIONS = {
    'A': dict(ra=1.15, depth=0.42, rim=0.10, halo=1.5, rays=(7, 11), ray_len=(2.0, 2.6), clusters=14, singles=25,
              apron=(1.4, 2.4), spray=2.5, note='menor e mais rasa: o buraco pequeno, o halo curto'),
    'B': dict(ra=1.3, depth=0.50, rim=0.12, halo=1.65, rays=(8, 13), ray_len=(2.2, 3.0), clusters=18, singles=35,
              apron=(1.6, 2.8), spray=3.0, note='o estudo: bacia de ~2,6 m e 50 cm, halo até 1,65·R, raios até 3·R'),
    'C': dict(ra=1.5, depth=0.55, rim=0.14, halo=1.8, rays=(9, 15), ray_len=(2.4, 3.2), clusters=24, singles=50,
              apron=(1.8, 3.2), spray=3.5, note='maior e mais revirada: mais torrão e raio'),
}


# ---------------------------------------------------------------- ruído (sem scipy)

def value_noise(x, y, seed):
    xi = np.floor(x).astype(np.int64)
    yi = np.floor(y).astype(np.int64)
    fx = x - xi
    fy = y - yi
    fx = fx * fx * (3 - 2 * fx)
    fy = fy * fy * (3 - 2 * fy)

    def h(ix, iy):
        v = (ix * 374761393 + iy * 668265263 + seed * 2246822519) & 0xFFFFFFFF
        v = ((v ^ (v >> 13)) * 1274126177) & 0xFFFFFFFF
        return ((v ^ (v >> 16)) & 0xFFFF) / 65535.0

    a = h(xi, yi) * (1 - fx) + h(xi + 1, yi) * fx
    b = h(xi, yi + 1) * (1 - fx) + h(xi + 1, yi + 1) * fx
    return a * (1 - fy) + b * fy


def fbm(x, y, seed, octaves=4):
    s = np.zeros_like(x)
    amp = 0.5
    norm = 0.0
    for o in range(octaves):
        s += value_noise(x, y, seed + o * 17) * amp
        norm += amp
        x = x * 2.03 + 3.1
        y = y * 2.03 + 1.7
        amp *= 0.5
    return s / norm


def blur(a, passes=1, kind=None):
    # o mesmo borrão em metros em qualquer resolução (a variância de uma passada é ~0,5 texel²)
    if kind is not None:
        passes = int(round(passes * (BASE_PX[kind] / (KINDS[kind]['span'] / KINDS[kind]['size'])) ** 2))
    for _ in range(passes):
        a = (4 * a + np.roll(a, 1, 0) + np.roll(a, -1, 0) + np.roll(a, 1, 1) + np.roll(a, -1, 1)) / 8.0
    return a


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def grid(kind):
    k = KINDS[kind]
    px = k['span'] / k['size']
    c = (np.arange(k['size']) + 0.5) * px - k['span'] / 2
    x, y = np.meshgrid(c, c)
    return x, y, px


# ---------------------------------------------------------------- as peças comuns

def rays_field(x, y, rng, n, r0, length_fn, width=(0.08, 0.22), seed=0):
    """Riscos finos e desiguais saindo do centro, com falhas; devolve (claros, escuros)."""
    r = np.hypot(x, y)
    th = np.arctan2(y, x)
    light = np.zeros_like(x)
    dark = np.zeros_like(x)
    for k in range(n):
        a = rng.normal(0.0, 1.0) if rng.random() < 0.6 else rng.uniform(-math.pi, math.pi)
        length = length_fn(a)
        w0 = rng.uniform(*width)
        d = np.angle(np.exp(1j * (th - a)))
        along = r * np.cos(d)
        across = np.abs(r * np.sin(d))
        w = w0 * (0.6 + 0.8 * np.clip(along / max(length, 1e-3), 0, 1))
        ray = (1 - smoothstep(0.0, 1.0, across / np.maximum(w, 1e-3))) * smoothstep(r0 * 0.9, r0 * 1.2, along) * (1 - smoothstep(length * 0.55, length, along))
        ray = ray * (along > 0)
        # as falhas: o risco quebra em pedaços (ruído ao longo dele, não em ângulo)
        ray *= smoothstep(0.38, 0.62, fbm(along * 2.2 + k * 11.0, across * 6.0 + k * 3.0, 61 + seed * 5 + k, 3))
        if rng.random() < 0.4:
            dark = np.maximum(dark, ray * rng.uniform(0.55, 0.9))
        else:
            light = np.maximum(light, ray * rng.uniform(0.6, 1.0))
    return light, dark


def soot_rays(x, y, rng, n, r0, length_fn, width=(0.6, 1.6), alpha=(0.35, 0.85), seed=0):
    """Os riscos de fuligem que saem do centro (13/10, a crítica do Look contra a street_battle: "é o que mais diz
    explodiu aqui"): largos na base (0,3–0,8 m), afinando pra ponta, de comprimentos diferentes, mais densos pro lado
    do tiro (+X), com a borda quebrada pelo ruído; devolve o escuro."""
    r = np.hypot(x, y)
    th = np.arctan2(y, x)
    dark = np.zeros_like(x)
    for k in range(n):
        # (2ª crítica do Look, mf13: saíam como pente, finos, retos e de borda dura, num leque só; agora ~35% em
        # toda a volta, os curtos mais grossos, a borda esfumada em ~60% da largura)
        a = rng.normal(0.0, 0.9) if rng.random() < 0.65 else rng.uniform(-math.pi, math.pi)
        length = length_fn(a)
        w0 = rng.uniform(*width) * 0.5 * float(np.clip(1.5 - 0.25 * (length / max(r0, 1e-3) - 2.0), 0.7, 1.5))
        d = np.angle(np.exp(1j * (th - a)))
        along = r * np.cos(d)
        across = np.abs(r * np.sin(d))
        t = np.clip(along / max(length, 1e-3), 0, 1)
        w = w0 * (1.0 - 0.6 * t) * (0.8 + 0.4 * fbm(along * 1.5 + k * 7.0, k * 3.0, 151 + seed * 7 + k, 2))
        ray = (1 - smoothstep(0.4, 1.0, across / np.maximum(w, 1e-3))) * smoothstep(r0 * 0.5, r0 * 0.9, along) * (1 - smoothstep(length * 0.5, length, along))
        ray *= (along > 0) * smoothstep(0.3, 0.55, fbm(along * 1.8 + k * 13.0, across * 5.0 + k, 163 + seed * 5 + k, 3))
        dark = np.maximum(dark, ray * rng.uniform(*alpha))
    return dark


def clods(x, y, px, rng, centers, singles, rmin, rmax, keep_out=None, dark_share=0.35, big=0):
    """Torrões em lei de potência (N(>s) ∝ s^−2,5, de 20 a 60 cm), em grupos com falhas; devolve (altura, claro, escuro)."""
    h = np.zeros_like(x)
    light = np.zeros_like(x)
    dark = np.zeros_like(x)
    size = x.shape[0]
    pts = []
    for (cx, cy) in centers:
        for _ in range(rng.poisson(5.0)):
            pts.append((cx + rng.normal(0, 0.28), cy + rng.normal(0, 0.28)))
    for _ in range(singles):
        u = rng.random()
        rc = 1.0 / np.sqrt(1.0 / rmin ** 2 - u * (1.0 / rmin ** 2 - 1.0 / rmax ** 2))
        a = rng.normal(0.0, 1.0) if rng.random() < 0.55 else rng.uniform(-math.pi, math.pi)
        pts.append((rc * math.cos(a) * (1.15 if math.cos(a) > 0 else 1.0), rc * math.sin(a)))
    smin, smax = 0.20, 0.60
    # os 3–5 grandes (40–80 cm) e claros, a areia de baixo, sem anel: de 1 a 3 raios, mais longe pro lado do tiro
    nbig = len(pts)
    for _ in range(big):
        a = rng.normal(0.0, 1.0) if rng.random() < 0.6 else rng.uniform(-math.pi, math.pi)
        rc = rmin * rng.uniform(1.0, 3.0) * (1.0 + 0.3 * max(math.cos(a), 0.0))
        pts.append((rc * math.cos(a), rc * math.sin(a)))
    for k, (cx, cy) in enumerate(pts):
        if keep_out is not None and keep_out(cx, cy):
            continue
        u = rng.random()
        s = smin * (1 - u * (1 - (smin / smax) ** 2.5)) ** (-1 / 2.5)
        is_big = k >= nbig
        if is_big:
            s = rng.uniform(0.4, 0.8)
        i0 = int((cx - x[0, 0]) / px)
        j0 = int((cy - y[0, 0]) / px)
        rad = int(s / px * 1.4) + 2
        if i0 - rad < 0 or j0 - rad < 0 or i0 + rad >= size or j0 + rad >= size:
            continue
        sl = (slice(j0 - rad, j0 + rad + 1), slice(i0 - rad, i0 + rad + 1))
        dx = x[sl] - cx
        dy = y[sl] - cy
        # irregular: o raio empurrado pelo ruído, achatado num sentido sorteado
        ang = rng.uniform(0, math.pi)
        qx = dx * math.cos(ang) + dy * math.sin(ang)
        qy = (-dx * math.sin(ang) + dy * math.cos(ang)) * rng.uniform(1.0, 1.6)
        # (13/10: no jogo o relevo do Look acende o torrão redondo e alto como bolha; mais baixo e mais torto)
        # (13/10: anguloso, um polígono torto de 4–7 lados, não um círculo: no alvo são pedaços quebrados)
        sides = int(rng.integers(4, 8))
        phi = np.mod(np.arctan2(qy, qx) + rng.uniform(0, math.pi), 2 * math.pi / sides) - math.pi / sides
        poly = math.cos(math.pi / sides) / np.cos(phi)
        rq = np.hypot(qx, qy) / (s * 0.5 * poly) * (1.0 + (value_noise(dx * 30 + k, dy * 30 - k, 71 + k) - 0.5) * 0.5)
        bump = np.clip(1.0 - rq ** 4, 0, 1) ** 0.5
        if is_big:
            light[sl] = np.maximum(light[sl], bump * rng.uniform(0.85, 1.0))
        elif rng.random() < dark_share:
            # o torrão escuro (terra de baixo, pedaço queimado)
            dark[sl] = np.maximum(dark[sl], bump * rng.uniform(0.7, 1.0))
        else:
            light[sl] = np.maximum(light[sl], bump * rng.uniform(0.75, 1.0))
        h[sl] = np.maximum(h[sl], bump * s * rng.uniform(0.12, 0.25))
    return h, light, dark


def lobed_radius(x, y, seed, amp_low=0.30, amp_mid=0.12):
    """O raio torto: 2–3 lóbulos (ruído de baixa frequência no espaço) e o recorte fino."""
    r = np.hypot(x, y)
    w = (fbm(x * 0.45 + seed * 7.1, y * 0.45 - seed * 3.3, 11 + seed, 2) - 0.5) * 2 * amp_low
    w += (fbm(x * 1.8 - seed * 1.3, y * 1.8 + seed * 2.9, 13 + seed, 3) - 0.5) * 2 * amp_mid
    return r * (1.0 + w)


# ---------------------------------------------------------------- os dois tipos

# ---------------------------------------------------------------- a prova aprovada (13/10, o Felipe: "já ficou bom pra kct")
# A prova da maquete do Look (Game/Tools/look_ground.py, crater_v2_values e crater_v3_relief, por cima deste crater()), a
# 2 cm por texel, trazida pra cá com o mesmo desenho (as folhas captures/look/cratera_texturas_v6.jpg e
# cratera_prova_v5.jpg). O ruído dele, fbm(x, y, escala, semente), é o daqui com x/escala; as sementes andam com a
# variante, pra as 6 não saírem iguais. Os torrões de altura viram a lista dos pedaços 3D (m['clods']: u, v, largura em m),
# que o jogo põe de pedra escaneada no mesmo lugar.
def fbm_m(x, y, scale, seed, octaves=4):
    return fbm(x / scale, y / scale, seed, octaves)


def box_mean(a, r):
    """Média numa janela (2r+1)² por somas acumuladas (sem scipy)."""
    p = np.pad(a, r, mode='edge')
    c = p.cumsum(0).cumsum(1)
    c = np.pad(c, ((1, 0), (1, 0)))
    k = 2 * r + 1
    return (c[k:, k:] - c[:-k, k:] - c[k:, :-k] + c[:-k, :-k]) / (k * k)


def crater_proof(m, seed):
    x, y, px = grid('crater')
    ra = m['ra']
    s7 = 1000 * seed
    # os valores medidos na street_battle: o halo escuro contínuo até ~1,9 R e o claro só nos torrões
    r = np.hypot(x, y) * (1.0 + (fbm_m(x, y, 1.6, 400 + s7, 3) - 0.5) * 0.5)
    halo = (1.0 - smoothstep(1.5 * ra, 1.95 * ra, r)) * (0.85 + 0.15 * fbm_m(x, y, 0.5, 410 + s7, 2))
    ring = (r > 0.85 * ra) & (r < 1.7 * ra)
    clod = smoothstep(0.035, 0.07, m['h'])
    m['light'] = np.where(ring, m['light'] * clod, m['light'])
    m['dark'] = np.maximum(m['dark'], halo) * (1.0 - 0.7 * m['light'])
    # o perfil da bacia: parede em pé e fundo largo, h = −d·(1 − (1 − t)^1,6), com um borrão leve (sem dobra)
    h = m['h'].astype(np.float64)
    d = float(m['depth'])
    t = np.clip(-h / d, 0.0, 1.0)
    h = np.where(h < 0, -d * (1.0 - (1.0 - t) ** 1.6), h)
    h = box_mean(h, 2)
    r = np.hypot(x, y)
    low = box_mean(box_mean(h, 6), 6)
    bump = h - low
    outside = r > 0.8 * ra
    mx = bump.copy()
    for dy in range(-4, 5):
        for dx in range(-4, 5):
            mx = np.maximum(mx, np.roll(np.roll(bump, dy, 0), dx, 1))
    # os picos do relevo (fora da bacia) viram pedras de 10–60 cm no mesmo lugar, e o calombo sai do relevo
    peaks = (bump >= mx) & (bump > 0.03) & (r > 1.05 * ra)
    ys, xs = np.nonzero(peaks)
    rk = np.random.default_rng(5 + seed)
    keep = rk.random(len(ys)) < 0.7
    clods = [(float(x[j, i]), float(y[j, i]), float(np.clip(bump[j, i] / 0.22, 0.1, 0.6))) for j, i, k in zip(ys, xs, keep) if k]
    h = np.where(outside, np.minimum(h, low + 0.008), h)
    # nenhum claro pintado onde vai pedra (lia "pipoca"); dentro da bacia, o claro fraco
    m['light'] = np.where(outside & (bump > 0.01), 0.0, m['light'])
    m['light'] = np.where(r < ra, m['light'] * 0.3, m['light'])
    # o fundo com textura: torrões e degraus de terra revirada de ~20–40 cm, ±3 cm
    inside = 1.0 - smoothstep(0.7 * ra, 1.0 * ra, r)
    h = h + inside * (fbm_m(x, y, 0.35, 520 + s7, 3) - 0.5) * 0.07
    # o escuro dentro da bacia só no núcleo torto e esfiapado e em manchas de carvão (a parede do lado do sol fica areia)
    core = 1.0 - smoothstep(0.15 * ra, 0.55 * ra, r * (1.0 + (fbm_m(x, y, 0.6, 530 + s7, 3) - 0.5) * 1.3))
    patch = smoothstep(0.45, 0.7, fbm_m(x, y, 0.4, 540 + s7, 3)) * 0.75
    inner = np.maximum(core * (0.75 + 0.25 * patch), patch)
    # (no jogo, sem o relevo de verdade da maquete, o fundo plano fica ao sol e lia disco claro dentro do halo escuro
    # (vila_v6): a terra úmida moderada no fundo todo, e a passagem pra parede macia, sem o corte em 0,95 R)
    # (a 0,5 o fundo ainda saía mais claro que o halo a 0,9, vila_v6b; no alvo o miolo é o mais escuro de tudo: 0,85)
    # (o fundo caía antes de o halo entrar e deixava um anel claro em 0,6–0,85 R, vila_v6c: vai até a borda e emenda)
    inner = np.maximum(inner, 0.85 * (1.0 - smoothstep(0.9 * ra, 1.1 * ra, r)) * (0.85 + 0.15 * fbm_m(x, y, 0.5, 545 + s7, 2)))
    w_in = 1.0 - smoothstep(0.85 * ra, 1.0 * ra, r)
    m['dark'] = m['dark'] * (1.0 - w_in) + inner * w_in
    # o manto de ejeção: de 1 a ~2,6 R, mosqueado, a borda de fora esfiapada e mais longa pro lado do tiro (+X)
    rmant = r * (1.0 + (fbm_m(x, y, 1.2, 550 + s7, 3) - 0.5) * 0.7) / (1.0 + 0.25 * np.clip(x / np.maximum(r, 1e-3), 0, 1))
    # (no jogo, de mais longe, o manto que vai sumindo de 1,6 a 2,6 R lia mancha curta em volta da bacia, a crítica do Look
    # na vila_v6c: vai até ~2,6 R inteiro e some até ~3 R)
    mant = (1.0 - smoothstep(2.2 * ra, 3.0 * ra, rmant)) * smoothstep(0.85 * ra, 1.05 * ra, r)
    mott = 0.4 + 0.35 * smoothstep(0.35, 0.7, fbm_m(x, y, 0.3, 560 + s7, 3))
    m['dark'] = np.maximum(m['dark'], mant * mott)
    # os 14 riscos de fuligem, de 1 a 3,5 R, finos e desiguais, ~65% pro lado do tiro
    rk2 = np.random.default_rng(31 + seed)
    th = np.arctan2(y, x)
    streak = np.zeros_like(h)
    for _ in range(14):
        a0 = rk2.normal(0.0, 1.1) if rk2.random() < 0.65 else rk2.uniform(-math.pi, math.pi)
        ln = ra * rk2.uniform(1.8, 3.5) * (1.0 + 0.3 * max(math.cos(a0), 0.0))
        wdt = rk2.uniform(0.04, 0.09)
        dth = np.angle(np.exp(1j * (th - a0)))
        streak = np.maximum(streak, (1.0 - smoothstep(wdt * 0.4, wdt, np.abs(dth))) * (1.0 - smoothstep(ln * 0.6, ln, r))
                            * smoothstep(0.9 * ra, 1.2 * ra, r) * rk2.uniform(0.5, 0.85))
    m['dark'] = np.maximum(m['dark'], streak)
    # os pedaços em raios (10–22 cm, até 3,5 R) e os graúdos do lábio (25–45 cm) em ~60% da volta
    span = KINDS['crater']['span']
    for _ in range(170):
        a0 = rk2.normal(0.0, 1.0) if rk2.random() < 0.6 else rk2.uniform(-math.pi, math.pi)
        rr_ = ra * (1.05 + rk2.gamma(1.6, 0.7)) * (1.0 + 0.25 * max(math.cos(a0), 0.0))
        if rr_ > 3.6 * ra or rr_ > span / 2 - 0.6:
            continue
        clods.append((rr_ * math.cos(a0), rr_ * math.sin(a0), float(rk2.uniform(0.1, 0.22))))
    for _ in range(26):
        a0 = rk2.uniform(-math.pi, math.pi)
        if math.cos(a0 + 1.3) < -0.3:
            continue
        rr_ = ra * rk2.uniform(0.95, 1.25)
        clods.append((rr_ * math.cos(a0), rr_ * math.sin(a0), float(rk2.uniform(0.25, 0.45))))
    # o relevo fino da terra revirada (o que tirou o "fofinho"): ±1,5 cm em 12 cm, ±2 cm em 30 cm e torrõezinhos de 9 cm
    churn = 1.0 - smoothstep(2.0 * ra, 2.8 * ra, r)
    fine = (fbm_m(x, y, 0.12, 570 + s7, 2) - 0.5) * 0.03 + (fbm_m(x, y, 0.3, 580 + s7, 2) - 0.5) * 0.04
    lumps = smoothstep(0.55, 0.75, fbm_m(x, y, 0.09, 590 + s7, 2)) * 0.025
    m['h'] = h + churn * (fine + lumps)
    # (as pedrinhas de 3–10 cm da prova ficam de fora: no jogo, a 5–8 cm por pixel, o mínimo de 20 cm vale)
    m['clods'] = clods
    return m


def crater(opt, seed):
    KIND_NOW[0] = 'crater'
    rng = np.random.default_rng(1000 + seed)
    x, y, px = grid('crater')
    ra = opt['ra'] * rng.uniform(0.88, 1.12)
    depth = opt['depth'] * (ra / opt['ra'])
    rim_h = opt['rim'] * (ra / opt['ra'])
    stretch = rng.uniform(1.12, 1.35)
    xe = (x - 0.10 * ra) / np.where(x > 0.10 * ra, stretch, 1.0 + (stretch - 1.0) * 0.3)
    xe = blur(xe, 2, 'crater')                            # sem a emenda da troca de esticada
    # (13/10, a crítica do Look: o disco quase perfeito lia como poço; 2–3 lobos grandes de ±25% e uma oitava fina de
    # ±8%, mais comprido pro lado do tiro pela esticada)
    tha = np.arctan2(y, xe)
    lobes = 1.0 + rng.uniform(0.10, 0.18) * np.cos(2 * tha + rng.uniform(0, 2 * math.pi)) \
                + rng.uniform(0.06, 0.12) * np.cos(3 * tha + rng.uniform(0, 2 * math.pi)) \
                + (fbm(np.cos(tha) * 2.5 + seed * 1.7, np.sin(tha) * 2.5 - seed, 171 + seed, 3) - 0.5) * 0.32
    rp = lobed_radius(xe, y, seed, 0.10, 0.06) / lobes
    down = np.cos(np.arctan2(y, x))
    rr = 1.25 * ra

    # (a) a bacia, áspera no fundo; (c) a borda quebrada
    bowl = -depth * (1.0 - (rp / ra) ** 2) * (1.0 + (fbm(x * 2.2, y * 2.2, 31 + seed, 3) - 0.5) * 0.4)
    # a borda levantada só em ~60% da volta (no alvo ela quebra em trechos)
    rim_arc = smoothstep(0.42, 0.55, fbm(np.cos(tha) * 1.6 + seed * 2.1, np.sin(tha) * 1.6 + seed, 181 + seed, 2))
    rim = rim_h * smoothstep(ra * 0.9, rr, rp) * (1 - smoothstep(rr, rr * 1.6, rp)) * (0.35 + 1.2 * fbm(x * 1.7 + seed * 2.3, y * 1.7 - seed, 41 + seed, 3)) * (0.15 + 0.85 * rim_arc)
    h = np.where(rp < ra, bowl, 0.0) + np.where(rp >= ra * 0.9, rim, 0.0)
    h += (rim_h * 0.5) * np.clip(rp / rr, 1.0, None) ** -3 * (1.0 + 0.5 * down) * (rp >= rr) * (1 - smoothstep(opt['halo'] * ra, opt['halo'] * ra * 1.4, rp))

    # (b) o halo escuro até ~halo·R, com a borda recortada e definida (1–3 texels)
    hr = opt['halo'] * ra * (1.0 + 0.25 * down)
    edge = lobed_radius(x, y, seed + 50, 0.18, 0.10) / lobes                 # o halo torto como a bacia
    patch = smoothstep(0.30, 0.60, fbm(x * 1.4 - 9, y * 1.4 + 4, 89 + seed, 4))
    # (13/10, no chão do Look: com o escuro a 0,14× da areia, a mancha de 0,55–1,0 virava pele de onça; o halo é
    # contínuo, só com uma variação leve, e o fundo da bacia é carvão até perto da borda, como na street_battle)
    # (a medida do Look na street_battle: o halo do alvo é maior e mais escuro; G alto até ~1,6 R)
    dark = (1 - smoothstep(hr * 0.93, hr, edge)) * (0.9 + 0.1 * patch)
    dark = np.maximum(dark, 1 - smoothstep(ra * 0.55, ra * 1.0, rp))        # o fundo da bacia, terra queimada e úmida

    # a borda: a areia clara empurrada pra cima, em pedaços
    # (no alvo o anel da borda não é mais claro que o chão em volta: o claro vem dos torrões soltos)
    light = np.where((rp > ra * 0.92) & (rp < rr * 1.25), 1.0, 0.0) * smoothstep(0.35, 0.6, fbm(x * 3.0, y * 3.0, 53 + seed, 3)) * rim_arc * 0.25

    # (d) os raios
    rl, rd = rays_field(x, y, rng, int(rng.integers(opt['rays'][0], opt['rays'][1] + 1)), rr,
                        lambda a: min(ra * rng.uniform(*opt['ray_len']) * (1.0 + 0.25 * math.cos(a)), KINDS['crater']['span'] / 2 - 0.8), seed=seed)
    light = np.maximum(light, rl)
    dark = np.maximum(dark, rd * 0.85)
    h += rl * 0.01
    # os riscos de fuligem do centro até 2–3 raios
    dark = np.maximum(dark, soot_rays(x, y, rng, int(rng.integers(8, 15)), ra,
                                      lambda a: min(ra * rng.uniform(1.6, 4.0) * (1.0 + 0.2 * math.cos(a)), KINDS['crater']['span'] / 2 - 0.8), seed=seed))

    # (e) os torrões: grupos na borda e nos raios, mais uns soltos; nenhum dentro da bacia funda
    centers = []
    for _ in range(opt['clusters']):
        a = rng.normal(0.0, 1.2) if rng.random() < 0.6 else rng.uniform(-math.pi, math.pi)
        # (sem anel: de 1 a 3 raios, mais longe pro lado do tiro)
        rc = ra * rng.uniform(1.0, 3.0) * (1.0 + 0.3 * max(math.cos(a), 0.0))
        centers.append((rc * math.cos(a) * 1.1, rc * math.sin(a)))
    # o miolo (2ª crítica do Look, mf13: um marrom chapado com um crescente): o núcleo quase preto e torto, manchas de
    # carvão de ~0,5 m no resto do fundo (0,7–1,0)
    inside = 1 - smoothstep(ra * 0.85, ra * 1.0, rp)
    core = 1 - smoothstep(ra * 0.25, ra * 0.5, lobed_radius(x, y, seed + 70, 0.25, 0.12))
    bowl_dark = np.maximum(0.7 + 0.3 * smoothstep(0.35, 0.65, fbm(x * 2.0 + 5, y * 2.0 - 5, 211 + seed, 3)), core)
    dark = dark * (1 - inside) + bowl_dark * inside
    # (a crítica do Look, 13/10: os claros em anel liam como pedrinhas brancas; a maioria é terra revirada escura)
    ch, cl, cd = clods(x, y, px, rng, centers, opt['singles'], rr, ra * opt['ray_len'][1],
                       keep_out=lambda cx, cy: math.hypot(cx, cy) < ra * 0.8, dark_share=0.6, big=int(rng.integers(3, 6)))
    # 4–8 pedaços claros de areia caídos dentro da bacia
    ih, il, _ = clods(x, y, px, rng, [], int(rng.integers(4, 9)), ra * 0.15, ra * 0.75, dark_share=0.0)
    h = h + ch + ih * 0.6
    light = np.maximum(np.maximum(light, cl), il)
    cl = np.maximum(cl, il)
    dark = np.maximum(dark, cd)
    # a areia clara cobre o escuro onde há torrão claro (no alvo: torrão claro sobre terra escura)
    dark = dark * (1 - 0.7 * cl)

    heat = (1 - smoothstep(0, ra * 1.1, rp)) * smoothstep(0.62, 0.76, fbm(x * 6.0, y * 6.0, 97 + seed, 3))   # brasas esparsas no fundo
    h = blur(h, 1, KIND_NOW[0])
    h = np.where(h > 0, 0.30 * np.tanh(h / 0.30), h)
    return dict(h=h, dark=np.clip(dark, 0, 1), heat=np.clip(heat, 0, 1), light=np.clip(light, 0, 1), ra=ra, depth=depth, kind='crater')


def wreck(opt, seed):
    KIND_NOW[0] = 'wreck'
    rng = np.random.default_rng(2000 + seed)
    x, y, px = grid('wreck')
    L, W = HULL
    # a distância até o casco (um retângulo de cantos redondos, +X = a frente)
    qx = np.abs(x) - (L / 2 - 0.6)
    qy = np.abs(y) - (W / 2 - 0.6)
    dhull = np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - 0.6
    out = np.maximum(dhull, 0)
    down = np.cos(np.arctan2(y, x))
    # o avental: de 1,5 a 3 m além do casco, com lóbulos (o combustível escorre e espirra), a borda definida
    a0, a1 = opt['apron']
    reach = a0 + (a1 - a0) * smoothstep(0.25, 0.75, fbm(x * 0.3 + seed * 3.1, y * 0.3 - seed, 101 + seed, 2))
    reach *= 1.0 + 0.25 * down * rng.uniform(0.5, 1.0)
    # 3–5 línguas de combustível espirrado, bem além do resto (o avental nunca é a sombra de uma caixa)
    th = np.arctan2(y, x)
    for k in range(int(rng.integers(3, 6))):
        a = rng.uniform(-math.pi, math.pi)
        d = np.abs(np.angle(np.exp(1j * (th - a))))
        reach = reach + rng.uniform(1.0, 2.2) * np.exp(-(d / rng.uniform(0.18, 0.4)) ** 2)
    # (13/10, a crítica do Look: a borda limpa lia como poça ou sombra; esfarrapada por uma oitava fina mais forte)
    edge = out + (fbm(x * 1.2 + 7, y * 1.2 - 3, 103 + seed, 3) - 0.5) * 1.5 + (fbm(x * 4.0, y * 4.0, 105 + seed, 2) - 0.5) * 0.9          + (fbm(x * 0.45 + 3, y * 0.45 - 8, 199 + seed, 2) - 0.5) * 3.0       # os dentes de 1–3 m (2ª crítica do Look)
    patch = smoothstep(0.28, 0.6, fbm(x * 1.3 - 9, y * 1.3 + 4, 107 + seed, 4))
    # (13/10: contínuo, como o halo da cratera; o 0,6–1,0 de antes virava pele de onça no tom do Look)
    dark = (1 - smoothstep(reach * 0.85, reach, edge)) * (0.85 + 0.15 * patch)
    # ~30% de areia voltando por dentro, em manchas largas e macias (não em pintas: a pele de onça de antes)
    # (2ª crítica do Look, mf13: em 0,8 m e a 60% não apareciam na câmera do jogo; manchas de 1–2 m. A ~90% (mf14) o
    # queimado sumia na cena inteira, e ele é o que marca onde o tanque morreu: a areia volta a ~65% em ~30% da área)
    dark *= 1.0 - 0.65 * smoothstep(0.50, 0.62, fbm(x * 0.4 + 21, y * 0.4 - 17, 191 + seed, 3)) * smoothstep(0.4, 1.0, dhull)
    # as manchas de combustível: poças escuras mais fechadas perto do casco
    pools = smoothstep(0.62, 0.72, fbm(x * 0.9 + 3, y * 0.9 + 5, 109 + seed, 3)) * (1 - smoothstep(0.3, reach * 0.8, out))
    dark = np.maximum(dark, pools)
    dark = np.maximum(dark, (dhull < 0.2) * 1.0)                            # embaixo do casco (escondido)
    # a brasa: no miolo do avental, perto do casco, em pedaços
    # (13/10: em manchas largas a brasa lia como buraco de areia no avental escuro; no alvo são pontos esparsos)
    heat = (1 - smoothstep(0.2, reach * 0.6, out)) * smoothstep(0.66, 0.78, fbm(x * 6.0, y * 6.0, 113 + seed, 3))
    # a areia espirrada pra fora pelo estouro: um leque de raios além do avental, mais pra frente e pros lados
    rl, rd = rays_field(x, y, rng, int(rng.integers(opt['rays'][0], opt['rays'][1] + 1)), L / 2,
                        lambda a: min(L / 2 + opt['spray'] * rng.uniform(0.8, 1.3), KINDS['wreck']['span'] / 2 - 0.8), width=(0.12, 0.3), seed=seed + 7)
    spray = smoothstep(0.35, 0.7, fbm(x * 2.5, y * 2.5, 117 + seed, 3)) * (1 - smoothstep(reach, reach + opt['spray'] * 0.7, edge)) * (edge > reach * 0.85)
    light = np.maximum(rl, spray * 0.7) * (dhull > 0.3)
    dark = np.maximum(dark, rd * 0.8)
    # riscos curtos de fuligem saindo da borda do casco (o estouro pelas escotilhas e pela saia)
    dark = np.maximum(dark, soot_rays(x, y, rng, int(rng.integers(6, 11)), L / 2,
                                      lambda a: L / 2 + rng.uniform(1.5, 3.5), width=(0.4, 0.9), alpha=(0.5, 0.75), seed=seed + 31) * (dhull > 0.0))
    # os torrões e lascas jogados pra fora, fora do casco
    centers = []
    for _ in range(opt['clusters']):
        a = rng.uniform(-math.pi, math.pi)
        rc = rng.uniform(0.4, 1.0) * (reach.mean() + opt['spray'])
        centers.append(((L / 2 + rc) * math.cos(a), (W / 2 + rc) * math.sin(a)))
    # (no alvo, em volta da carcaça os pedaços são escuros; os claros soltos por igual liam como pedrinhas brancas)
    # (2ª crítica do Look: mais destroços pequenos, a maioria escuros, até ~1,5× o avental)
    ch, cl, cd = clods(x, y, px, rng, centers, opt['singles'] // 2 + 6, L / 2 + 0.5, L / 2 + 1.5 * (a1 + opt['spray'] * 0.5),
                       keep_out=lambda cx, cy: abs(cx) < L / 2 + 0.2 and abs(cy) < W / 2 + 0.2, dark_share=0.75)
    h = ch   # (o afundado na beira do casco desenhava um retângulo em volta da carcaça: saiu)
    light = np.maximum(light, cl)
    dark = np.maximum(dark, cd) * (1 - 0.6 * cl)
    h = blur(h, 1, KIND_NOW[0])
    return dict(h=h, dark=np.clip(dark, 0, 1), heat=np.clip(heat, 0, 1), light=np.clip(light, 0, 1), ra=L / 2, depth=0.06, kind='wreck')


# ---------------------------------------------------------------- a prévia

SUN_ELEV = 20.0           # a receita aprovada (desert_golden_hour_v2_aprovada): 20°, contraluz de três-quartos
SAND = np.array([0.62, 0.47, 0.32])
SAND_DEEP = np.array([0.36, 0.26, 0.17])
SUN_COL = np.array([1.0, 0.86, 0.68])
SKY = np.array([0.30, 0.30, 0.34])


def shade(cr, sun_screen=(-0.55, 0.62), elev=SUN_ELEV):
    """Sombreado como a MF do Look vai fazer (aproximado): o escuro tira a cor até ~45%, o claro clareia, a brasa brilha."""
    h = cr['h']
    n_px = h.shape[0]
    px = KINDS[cr['kind']]['span'] / n_px
    gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) / (2 * px)
    gy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) / (2 * px)
    n = np.dstack([-gx, -gy, np.ones_like(h)])
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    sd = np.array([-sun_screen[0], -sun_screen[1]])
    sd = sd / np.linalg.norm(sd)
    el = math.radians(elev)
    Lv = np.array([sd[0] * math.cos(el), sd[1] * math.cos(el), math.sin(el)])
    ndl = np.clip((n * Lv).sum(2), 0, 1)
    occ = np.zeros_like(h)
    for step in np.arange(0.5, 60.0, 0.5):
        ox, oy = sd[0] * step, sd[1] * step
        x0, y0 = int(math.floor(ox)), int(math.floor(oy))
        ax, ay = ox - x0, oy - y0
        r00 = np.roll(np.roll(h, -y0, 0), -x0, 1)
        r10 = np.roll(r00, -1, 1)
        r01 = np.roll(r00, -1, 0)
        r11 = np.roll(r10, -1, 0)
        hk = (r00 * (1 - ax) + r10 * ax) * (1 - ay) + (r01 * (1 - ax) + r11 * ax) * ay
        occ = np.maximum(occ, (hk - h - step * px * math.tan(el)) / 0.02)
    vis = blur(np.clip(1 - occ, 0, 1), 1)
    deep = np.clip(-h / 0.5, 0, 1)[..., None]
    alb = SAND * (1 - deep * 0.7) + SAND_DEEP * deep * 0.7
    li = cr['light'][..., None]
    alb = alb * (1 + 0.22 * li)
    yy, xx = np.mgrid[0:n_px, 0:n_px].astype(np.float64)
    grain = fbm(xx * 0.9, yy * 0.9, 5, 3)[..., None]
    alb = alb * (0.9 + 0.2 * grain * (0.4 + li))
    s = cr['dark'][..., None]
    lum = alb.mean(2, keepdims=True)
    alb = (alb * (1 - 0.6 * s) + lum * 0.6 * s) * (1 - 0.55 * s)
    ao = np.clip(1.0 - deep[..., 0] * 0.35, 0, 1)[..., None]
    col = alb * (SUN_COL * 2.2 * (ndl * vis)[..., None] + SKY * ao * 1.4)
    col += np.array([1.0, 0.35, 0.08]) * (cr['heat'] ** 2)[..., None] * 0.6 * smoothstep(0.55, 0.75, grain)
    col = col / (1 + col)
    return np.clip(col ** (1 / 1.25), 0, 1)


def font(size=15):
    try:
        return ImageFont.truetype('arial.ttf', size)
    except OSError:
        return ImageFont.load_default()


def tilt(img, ppm, span, elev_deg=37.5):
    im = Image.fromarray((img * 255).astype(np.uint8))
    out_w = int(span * ppm)
    return im.resize((out_w, int(out_w * math.sin(math.radians(elev_deg)))), Image.LANCZOS)


def hull_overlay(t, ppm):
    """A carcaça por cima do avental (escura, queimada): o que sobra à vista é o que conta."""
    d = ImageDraw.Draw(t)
    cx, cy = t.width / 2, t.height / 2
    L, W = HULL[0] * ppm, HULL[1] * ppm * math.sin(math.radians(37.5))
    d.rectangle([cx - L / 2, cy - W / 2 - ppm * 1.2, cx + L / 2, cy + W / 2], fill=(38, 34, 30))
    d.rectangle([cx - L * 0.2, cy - W * 0.3 - ppm * 1.6, cx + L * 0.15, cy + W * 0.2 - ppm * 1.2], fill=(28, 25, 22))


def tank_icon(ppm):
    w, hgt = int(HULL[0] * ppm), int(HULL[1] * ppm * math.sin(math.radians(37.5)))
    im = Image.new('RGBA', (w + 4, hgt + 4), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rectangle([2, 2, w, hgt], fill=(150, 145, 135, 255), outline=(60, 58, 55, 255))
    d.rectangle([int(w * 0.35), int(hgt * 0.25), int(w * 0.65), int(hgt * 0.75)], fill=(120, 116, 108, 255))
    return im


def target_crop():
    if not os.path.exists(TARGET):
        return None
    im = Image.open(TARGET).convert('RGB')
    w, h = im.size
    return im.crop((int(w * 0.10), int(h * 0.50), int(w * 0.55), int(h * 0.95)))


def preview(name, opt, marks_c, marks_w):
    os.makedirs(PREVIEW, exist_ok=True)
    ppm = 17.7                            # px por metro na câmera do jogo (o tanque de 7,9 m tem ~140 px)
    tc = [tilt(shade(m), ppm, KINDS['crater']['span']) for m in marks_c[:3]]
    tw = []
    for m in marks_w[:2]:
        t = tilt(shade(m), ppm, KINDS['wreck']['span'])
        hull_overlay(t, ppm)
        tw.append(t)
    crop = target_crop()
    k = ppm / 11.0                        # no alvo o tanque tem ~85 px (7,9 m): ~11 px/m
    crop = crop.resize((int(crop.width * k), int(crop.height * k)), Image.LANCZOS) if crop else None
    W = max(sum(t.width for t in tc), sum(t.width for t in tw)) + (crop.width + 16 if crop else 0)
    H = max(tc[0].height + tw[0].height, crop.height if crop else 0) + 50
    sheet = Image.new('RGB', (W, H), (24, 24, 24))
    x0 = 0
    if crop:
        sheet.paste(crop, (0, 50))
        x0 = crop.width + 16
    for i, t in enumerate(tc):
        sheet.paste(t, (x0 + i * tc[0].width, 50))
    for i, t in enumerate(tw):
        sheet.paste(t, (x0 + i * tw[0].width, 50 + tc[0].height))
    ic = tank_icon(ppm)
    sheet.paste(ic, (x0 + 6, 54), ic)
    d = ImageDraw.Draw(sheet)
    d.text((8, 6), f'opção {name}: {opt["note"]}', fill=(235, 235, 235), font=font(16))
    d.text((8, 28), 'esquerda: o alvo (street_battle) na escala do jogo  |  em cima: a cratera sem carcaça  |  embaixo: a morte do tanque, com a carcaça por cima  |  régua: o tanque de 7,9 m', fill=(200, 200, 200), font=font(13))
    out = os.path.join(PREVIEW, f'alvo_vs_{name}.png')
    sheet.save(out)
    return out


def ramp(c, px):
    """A rampa de distância: dentro (c > 0,02) 0,5 + 0,5·c; fora, de 0,5 na borda a 0 a RAMP_M (chanfro 3×3)."""
    inside = c > 0.02
    d = np.where(inside, 0.0, 1e9)
    for _ in range(int(math.ceil(RAMP_M / px)) + 1):
        for (dy, dx, w) in ((1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0), (1, 1, 1.414), (1, -1, 1.414), (-1, 1, 1.414), (-1, -1, 1.414)):
            d = np.minimum(d, np.roll(np.roll(d, dy, 0), dx, 1) + w)
    out = np.clip(0.5 * (1.0 - d * px / RAMP_M), 0.0, 0.5)
    return np.where(inside, 0.5 + 0.5 * np.clip(c, 0, 1), out)


def blot(seed):
    """A mancha do queimado (a mesma receita do carimbo do Canvas de antes, RTSGroundMemory::MakeStamp): a borda
    recortada por ruído fractal perto de r = 0,55, o miolo mais escuro, a fuligem em pontinhos e 5 riscos do centro."""
    KIND_NOW[0] = 'blot'
    rng = np.random.default_rng(3000 + seed)
    x, y, px = grid('blot')
    r = np.hypot(x, y)
    n = fbm(x * 2.5 + seed * 5.3, y * 2.5 - seed * 2.1, 7 + seed, 4)
    rp = r + (n - 0.5) * 0.42
    core = (1 - smoothstep(0.50, 0.60, rp)) * (0.62 + 0.38 * (1 - smoothstep(0.0, 0.5, r)))
    fleck = smoothstep(0.62, 0.72, fbm(x * 11.0, y * 11.0, 29 + seed, 3)) * (1 - smoothstep(0.55, 0.95, r)) * 0.6
    th = np.arctan2(y, x)
    ray = np.zeros_like(x)
    for _ in range(5):
        a = rng.uniform(-math.pi, math.pi)
        ray = np.maximum(ray, np.maximum(np.cos(th - a), 0) ** 60 * (1 - smoothstep(0.4, rng.uniform(0.8, 1.0), r)))
    ray *= 0.5 * smoothstep(0.35, 0.65, fbm(x * 6.0, y * 6.0, 41 + seed, 3))
    dark = np.clip(np.maximum(np.maximum(core, fleck), ray), 0, 1)
    z = np.zeros_like(x)
    return dict(h=z, dark=dark, heat=z, light=z, ra=0.6, depth=0.0, kind='blot')


def save_atlas(kind, name, opt, marks):
    os.makedirs(OUT, exist_ok=True)
    px = KINDS[kind]['span'] / KINDS[kind]['size']
    enc = (lambda c: ramp(c, px)) if RAMPS else (lambda c: c)
    rows = [np.dstack([np.clip(0.5 + m['h'] / (2 * RANGE_M), 0, 1), enc(m['dark']), m['heat'], enc(m['light'])]) for m in marks]
    atlas = np.concatenate(rows, axis=1)
    a16 = (np.clip(atlas, 0, 1) * 65535 + 0.5).astype(np.uint16)
    h, w = a16.shape[:2]
    raw = b''.join(b'\x00' + a16[y].astype('>u2').tobytes() for y in range(h))

    def chunk(t, data):
        c = struct.pack('>I', len(data)) + t + data
        return c + struct.pack('>I', zlib.crc32(t + data) & 0xFFFFFFFF)

    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 16, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
    stem = f'{kind}_{name}' if name else kind
    path = os.path.join(OUT, f'{stem}.png')
    open(path, 'wb').write(png)
    meta = dict(kind=kind, option=name, variants=len(marks), size=KINDS[kind]['size'], span_m=KINDS[kind]['span'], range_m=RANGE_M,
                channels='R altura com sinal (0,5 = zero), G escuro, B calor, A claro', params=opt,
                encoding='ramp' if RAMPS else 'linear',
                # os pedaços 3D de cada variante (u, v, largura em m), no espaço do carimbo, antes do giro e da escala:
                # as graúdas (≥ 20 cm) e as miúdas (8–20 cm, que o jogo só mostra no zoom mais perto)
                clods=[export_clods(m.get('clods', []), i) for i, m in enumerate(marks)])
    json.dump(meta, open(os.path.join(OUT, f'{stem}.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    return path


def export_clods(clods, seed):
    """As pedras de uma variante pro jogo (13/10, o Look: com 45 graúdas a cratera lia canteiro de pedra): no máximo 14
    graúdas (≥ 20 cm), um sorteio fixo que guarda a mistura de tamanhos, e todas as miúdas de 8–20 cm."""
    big = [c for c in clods if c[2] >= 0.2]
    small = [c for c in clods if 0.08 <= c[2] < 0.2]
    if len(big) > 14:
        rk = np.random.default_rng(77 + seed)
        big = [big[i] for i in sorted(rk.choice(len(big), 14, replace=False))]
    # (a miúda de 0,1996 arredondada pra 0,2 virava graúda no jogo, que corta em 20 cm)
    return [[round(u, 3), round(v, 3), round(s, 3)] for (u, v, s) in big] + [[round(u, 3), round(v, 3), min(round(s, 3), 0.199)] for (u, v, s) in small]


def main():
    # a mancha não tem opção: sai uma vez (junto da A, ou quando pedida com "blot")
    names = [a for a in sys.argv[1:] if not a.startswith('--')]
    if not names or 'A' in names or 'blot' in names:
        print('MARCAS mancha:', save_atlas('blot', '', {}, [blot(s) for s in range(4)]))
    names = [n for n in names if n != 'blot']
    if [a for a in sys.argv[1:] if not a.startswith('--')] == ['blot']:
        return
    for name in (names or list(OPTIONS)):
        opt = OPTIONS[name]
        mc = [crater_proof(crater(opt, s), s) for s in range(VARIANTS)]
        save_atlas('crater', name, opt, mc)
        if '--crater-only' in sys.argv:
            print(f'MARCAS {name}: só a cratera')
            continue
        mw = [wreck(opt, s) for s in range(VARIANTS)]
        save_atlas('wreck', name, opt, mw)
        print(f'MARCAS {name}: prévia {preview(name, opt, mc, mw)}')


if __name__ == '__main__':
    main()
