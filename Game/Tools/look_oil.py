"""As manchas de óleo do asfalto (13/10): o atlas de carimbos que o asfalto v2 espalha de forma rara e aleatória.

O pedido do Felipe ("pega textura de óleo, inspiração, referências") e as referências que ele escolheu:
- o estacionamento visto de cima (Unsplash, Krzysztof Kotkowicz): manchas foscas tipo "nuvem", borda irregular e
  esfumada, miolo manchado, uma beirada um pouco mais escura, respingos em volta; "bem aleatórias e raras, ou um grupinho
  de manchas num canto, meio separadas";
- a mancha acumulada (Unsplash, "oil stain asphalt", e a do concreto): quase preta, borda mais nítida com respingos,
  viscosa, com reflexo.
Não existe mancha de óleo escaneada gratuita (procurei no ambientCG e no Poly Haven, 13/10): o atlas é gerado aqui.

Saída: captures/look/textures/T_LL_Oil.png, 2×2 variantes de 512² (cada uma cobre 2,4 m):
  0 = a mancha fosca grande (vaga de estacionamento), 1 = o grupinho de manchas separadas,
  2 = os pingos espalhados, 3 = a poça acumulada e viscosa.
  R = o escuro (0..1), G = o molhado/brilho (só o miolo da poça e das frescas), B = a beirada, A = a cobertura.
"""
import os
import sys

import numpy as np
from PIL import Image

N = 512
SPAN = 2.4          # metros por variante


def vnoise(x, y, seed):
    """Ruído de valor suave (interpolação cúbica), sem serrilhado."""
    xi, yi = np.floor(x).astype(np.int64), np.floor(y).astype(np.int64)
    xf, yf = x - xi, y - yi
    u, v = xf * xf * (3 - 2 * xf), yf * yf * (3 - 2 * yf)

    def h(i, j):
        k = (i * 374761393 + j * 668265263 + seed * 2147483647) & 0xFFFFFFFF
        k = (k ^ (k >> 13)) * 1274126177 & 0xFFFFFFFF
        return ((k ^ (k >> 16)) & 0xFFFF) / 65535.0
    return (h(xi, yi) * (1 - u) * (1 - v) + h(xi + 1, yi) * u * (1 - v) + h(xi, yi + 1) * (1 - u) * v
            + h(xi + 1, yi + 1) * u * v)


def fbm(x, y, seed, oct=5):
    s, a, t = 0.0, 1.0, 0.0
    for o in range(oct):
        s = s + a * vnoise(x * 2 ** o, y * 2 ** o, seed + 17 * o)
        t += a
        a *= 0.5
    return s / t


def ss(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


c = (np.arange(N) + 0.5) / N * SPAN - SPAN / 2
X, Y = np.meshgrid(c, c)


def stain(cx, cy, r, seed, warp=0.35, mottle=0.35, edge=0.12, stretch=1.0, ang=0.0):
    """Uma mancha "nuvem": distância deformada por ruído (borda torta), miolo manchado, beirada mais escura."""
    ca, sa = np.cos(ang), np.sin(ang)
    dx, dy = X - cx, Y - cy
    u, v = (dx * ca + dy * sa) / stretch, -dx * sa + dy * ca
    wx = (fbm(X * 1.6 / r, Y * 1.6 / r, seed) - 0.5) * 2 * warp
    wy = (fbm(X * 1.6 / r + 7.3, Y * 1.6 / r - 3.1, seed + 5) - 0.5) * 2 * warp
    d = np.hypot(u / r + wx, v / r + wy)
    d = d + (fbm(X * 9 / r, Y * 9 / r, seed + 11, 3) - 0.5) * edge          # a borda esfiapada
    cov = 1 - ss(0.82, 1.0, d)
    # o miolo manchado (v2: nas fotos o óleo é nuvem, com o grão do chão aparecendo: o encharcado varia em manchas de ~⅓ do
    # raio e em poros finos)
    mot = 1 - mottle * (1 - fbm(X * 3.5 / r, Y * 3.5 / r, seed + 23, 4)) * 2.2
    mot = mot * (0.78 + 0.22 * fbm(X * 60.0, Y * 60.0, seed + 29, 2))
    rim = np.exp(-((d - 0.9) / 0.07) ** 2)
    return cov, np.clip(mot, 0, 1), rim, d


def satellites(rng, cx, cy, r, n, seed, size=(0.02, 0.06), spread=(1.05, 1.8)):
    cov = np.zeros_like(X)
    for k in range(n):
        a = rng.uniform(0, 2 * np.pi)
        dd = r * rng.uniform(*spread)
        s, _, _, _ = stain(cx + dd * np.cos(a), cy + dd * np.sin(a), rng.uniform(*size), seed + 100 + k, warp=0.25, mottle=0.1)
        cov = np.maximum(cov, s)
    return cov


def fan(rng, cx, cy, r, ang, n, seed):
    """Os respingos em leque pra um lado (o óleo que pingou com o carro andando / espirrou): gotas que diminuem com a
    distância, num cone de ~50°."""
    cov = np.zeros_like(X)
    for j in range(n):
        a = ang + rng.normal(0, 0.45)
        dd = r * (1.0 + rng.gamma(1.5, 0.5))
        s_ = max(0.012, 0.07 * (1.0 - (dd / r - 1.0) / 2.5)) * rng.uniform(0.5, 1.1)
        c_, _, _, _ = stain(cx + dd * np.cos(a), cy + dd * np.sin(a), s_, seed * 31 + j, warp=0.3, mottle=0.1, stretch=1.4, ang=a)
        cov = np.maximum(cov, c_)
    return cov


def variant(k):
    rng = np.random.default_rng(31 + k)
    R = np.zeros_like(X); G = np.zeros_like(X); B = np.zeros_like(X); A = np.zeros_like(X)
    if k == 0:      # a mancha fosca grande (a vaga)
        cov, mot, rim, d0 = stain(0.0, 0.0, 0.55, 3, stretch=1.35, ang=0.4)
        # (v3, o Felipe: "os respingos em alguns casos") mais respingos e mais visíveis, uns em leque pra um lado
        sat = np.maximum(satellites(rng, 0.0, 0.0, 0.6, 16, 3, size=(0.02, 0.08), spread=(1.0, 2.0)),
                         fan(rng, 0.0, 0.0, 0.6, 1.1, 10, 7))
        core = 0.7 + 0.3 * (1 - ss(0.0, 0.8, d0))                           # o miolo mais escuro (v3)
        A = np.maximum(cov, sat); R = np.maximum(cov * (0.55 + 0.45 * mot) * core, sat * 0.85); B = rim * cov
        cov2, mot2, rim2, _ = stain(0.35, -0.25, 0.22, 9, mottle=0.25)     # uma mais nova por cima
        A = np.maximum(A, cov2); R = np.maximum(R, cov2 * (0.8 + 0.2 * mot2)); B = np.maximum(B, rim2 * cov2)
    elif k == 1:    # o grupinho de manchas separadas, de tamanhos diferentes
        for j in range(6):
            r = rng.uniform(0.08, 0.3) * (1.0 if j else 1.4)
            cx, cy = rng.normal(0, 0.45), rng.normal(0, 0.35)
            cov, mot, rim, _ = stain(cx, cy, r, 40 + j)
            A = np.maximum(A, cov); R = np.maximum(R, cov * (0.5 + 0.5 * mot) * rng.uniform(0.6, 1.0)); B = np.maximum(B, rim * cov)
            sat = satellites(rng, cx, cy, r, rng.integers(1, 4), 40 + j)
            A = np.maximum(A, sat); R = np.maximum(R, sat * 0.7)
    elif k == 2:    # os pingos espalhados (o concreto): muitos tamanhos, uns em fila
        for j in range(26):
            r = 0.03 + 0.12 * rng.random() ** 2.2
            cx, cy = rng.normal(0, 0.55), rng.normal(0, 0.4)
            cov, mot, rim, _ = stain(cx, cy, r, 60 + j, warp=0.3)
            A = np.maximum(A, cov); R = np.maximum(R, cov * (0.45 + 0.55 * mot) * rng.uniform(0.5, 1.0))
    else:           # a poça acumulada e viscosa: quase preta, borda mais nítida, respingos, brilho no miolo
        cov, mot, rim, d = stain(0.0, 0.0, 0.5, 80, warp=0.45, mottle=0.12, edge=0.06, stretch=1.2, ang=-0.3)
        halo = (1 - ss(1.0, 1.45, d)) * (1 - cov)                                   # o encharcado em volta
        sat = np.maximum(satellites(rng, 0.0, 0.0, 0.55, 24, 80, size=(0.015, 0.07), spread=(1.0, 1.9)),
                         fan(rng, 0.0, 0.0, 0.55, -2.0, 14, 9))
        A = np.maximum(np.maximum(cov, sat), halo * 0.6)
        R = np.maximum(np.maximum(cov * (0.85 + 0.15 * mot), sat * 0.9), halo * 0.45)
        # o molhado (v2): o reflexo do óleo viscoso não é a poça inteira clara; são faixas onde a superfície ondula e a beira
        # (o menisco), o resto é quase preto
        streak = ss(0.55, 0.8, fbm(X * 3.0 + Y * 1.2, Y * 9.0 - X * 0.6, 91, 4))
        # (v4c, o Felipe no 1:1: "a beira brilhante sai quase branca e por igual, como um contorno desenhado"): o menisco só
        # em trechos da volta (~35%), e um véu de brilho na superfície toda (o céu refletindo de leve), não só na beira
        ang = np.arctan2(Y, X)
        arc = ss(0.52, 0.66, fbm(np.cos(ang) * 2.2 + 3.1, np.sin(ang) * 2.2 - 1.7, 95, 3))
        G = cov * np.maximum(np.maximum(0.5, streak * 0.85), rim * arc * 0.85)
        B = rim * cov
    return np.stack([R, G, B, A], -1)


def main(out):
    tiles = [variant(k) for k in range(4)]
    atlas = np.concatenate([np.concatenate(tiles[:2], 1), np.concatenate(tiles[2:], 1)], 0)
    os.makedirs(out, exist_ok=True)
    p = os.path.join(out, 'T_LL_Oil.png')
    Image.fromarray((np.clip(atlas, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGBA').save(p)
    print('LOOKOIL', p, atlas.shape)
    return atlas


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', '..', 'captures', 'look', 'textures'))
