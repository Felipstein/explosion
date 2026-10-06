"""Texturas geradas por código pras fatias (o que não tem em CC0 no pacote baixado).

Rodar com o Python do sistema (numpy + pillow):
  python WorldForge/Tools/make_slice_textures.py
Gera WorldForge/Intermediate/Assets/Generated/<nome>/<nome>_{diff,nor_dx,arm,disp}_2k.png,
no mesmo formato dos assets do Poly Haven (o build_slice.py importa igual):
  diff (sRGB), nor_dx (normal DirectX, o da Unreal), arm (R oclusão, G
  rugosidade, B metal), disp (altura).

  asphalt_cracks  máscara de rachaduras do asfalto: couro de jacaré (células de
                  Voronoi de 25–45 cm, a fadiga clássica de pavimento) em manchas,
                  trincas longitudinais e transversais; R da "diff" = rachadura
  snow            neve: ondulações de vento (sastrugi baixas), grão fino,
                  rugosidade com pontos mais lisos (o brilho dos cristais)
  ice             gelo de lago: escuro, liso, bolhas e rachaduras finas, neve
                  soprada nas bordas das placas
Tudo periódico (ladrilha sem emenda): ruído filtrado no espaço de Fourier e
Voronoi com distância envolvente.
"""
import os

import numpy as np
from PIL import Image

N = 2048
TOOLS = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(TOOLS, '..', 'Intermediate', 'Assets', 'Generated')
FREQ = np.fft.fftfreq(N) * N
K = np.sqrt(FREQ[None, :] ** 2 + FREQ[:, None] ** 2)


def band(lo, hi, seed, slope=-1.0):
    rng = np.random.default_rng(seed)
    spec = np.fft.fft2(rng.standard_normal((N, N)))
    win = np.clip((K - lo * 0.7) / (lo * 0.3 + 1e-6), 0, 1) * np.clip((hi * 1.3 - K) / (hi * 0.3), 0, 1)
    out = np.real(np.fft.ifft2(spec * win * np.maximum(K, 1.0) ** slope))
    return (out / (out.std() + 1e-9)).astype(np.float32)


def worley(cells, seed, jitter=1.0):
    rng = np.random.default_rng(seed)
    jit = (0.5 + (rng.random((cells, cells, 2)) - 0.5) * jitter).astype(np.float32)
    c = N / cells
    y, x = np.mgrid[0:N, 0:N].astype(np.float32)
    ci, cj = np.floor(y / c).astype(int), np.floor(x / c).astype(int)
    f1 = np.full((N, N), 1e9, np.float32)
    f2 = np.full((N, N), 1e9, np.float32)
    for di in (-1, 0, 1):
        for dj in (-1, 0, 1):
            ni, nj = ci + di, cj + dj
            j = jit[ni % cells, nj % cells]
            d = np.sqrt(((ni + j[..., 0]) * c - y) ** 2 + ((nj + j[..., 1]) * c - x) ** 2) / c
            closer = d < f1
            f2 = np.where(closer, f1, np.minimum(f2, d))
            f1 = np.where(closer, d, f1)
    return f1, f2


def normals(h, tile_m, strength=1.0):
    px = tile_m / N
    dx = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) / (2 * px)
    dy = (np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)) / (2 * px)
    n = np.stack([-dx * strength, dy * strength, np.ones_like(h)], -1)   # DirectX: Y pra baixo
    return n / np.linalg.norm(n, axis=-1, keepdims=True)


def save(name, diff_rgb, n, ao, rough, height):
    folder = os.path.join(OUT, name)
    os.makedirs(folder, exist_ok=True)
    def png(arr, suffix, mode):
        Image.fromarray((np.clip(arr, 0, 1) * 255 + 0.5).astype(np.uint8), mode).save(
            os.path.join(folder, '%s_%s_2k.png' % (name, suffix)))
    png(diff_rgb ** (1 / 2.2), 'diff', 'RGB')                       # cor em sRGB
    png(np.stack([n[..., 0] * 0.5 + 0.5, n[..., 1] * 0.5 + 0.5, n[..., 2] * 0.5 + 0.5], -1), 'nor_dx', 'RGB')
    png(np.stack([ao, rough, np.zeros_like(ao)], -1), 'arm', 'RGB')
    hh = (height - height.min()) / (height.max() - height.min() + 1e-9)
    png(hh, 'disp', 'L')
    print('texture', name)


def asphalt_cracks():
    # fadiga (couro de jacaré): bordas das células, só onde a mancha deixa
    f1, f2 = worley(18, 31)                       # ~33 cm por célula num tile de 6 m
    edge = np.clip(1 - (f2 - f1) / 0.06, 0, 1) ** 2
    patch = np.clip((band(1, 4, 32) + 0.2) * 1.5, 0, 1)
    alligator = edge * patch
    # trincas longas: linhas onduladas em x e em y (periódicas)
    y, x = np.mgrid[0:N, 0:N].astype(np.float32) / N
    wob = band(3, 12, 33) * 0.01
    long_ = np.clip(1 - np.abs(np.sin(np.pi * (2 * x + wob))) / 0.004, 0, 1)
    trans = np.clip(1 - np.abs(np.sin(np.pi * (3 * y + band(3, 12, 34) * 0.01))) / 0.003, 0, 1) * (band(2, 6, 35) > 0.3)
    crack = np.clip(alligator + long_ * 0.9 + trans * 0.8, 0, 1)
    # alarga um pouco e quebra o traço (rachadura de verdade some e volta)
    crack = np.maximum(crack, np.roll(crack, 1, 0) * 0.7)
    crack *= np.clip(band(20, 80, 36) * 0.6 + 0.8, 0, 1)
    # a "diff" guarda a máscara (R = rachadura, G = sujeira dentro, B = remendo)
    dirt = np.clip(crack * 1.2 + np.clip(band(30, 120, 37), 0, None) * 0.15, 0, 1)
    patchwork = (band(1, 3, 38) > 1.1).astype(np.float32) * 0.8
    h = -0.01 * crack
    n = normals(h, 6.0, 1.0)
    save('asphalt_cracks', np.stack([crack, dirt, patchwork], -1) ** 2.2, n, 1 - 0.6 * crack, 0.85 + 0.1 * crack, h)


def snow():
    # sastrugi: ondulações alongadas pelo vento (anisotrópicas), grão fino
    rng_dir = 0.35
    y, x = np.mgrid[0:N, 0:N].astype(np.float32) / N
    warp = band(2, 8, 41)
    ripple = np.sin(2 * np.pi * (11 * y + 2 * x) + 1.8 * warp) * 0.5 + 0.5
    drifts = band(2, 10, 42)
    grain = band(150, 600, 43, slope=0.0)
    # ondulação fraca e quebrada por manchas (forte e regular, lia como caixa de ovo)
    # deriva só larga e baixa: 4 cm em ondas de 1–4 m faziam calombos regulares
    # com o sol baixo (vistos nas fotos de 05/10); o relevo grande é do terreno
    drifts = band(1, 3, 42)
    h = 0.012 * drifts + 0.004 * ripple ** 2 * np.clip(band(1, 4, 47) * 0.5 + 0.5, 0, 1) + 0.0012 * grain
    n = normals(h, 8.0, 1.0)
    # albedo de neve fresca ~0.85–0.9, levemente azul na sombra das ondas
    base = 0.86 + 0.03 * band(4, 30, 44) - 0.04 * (1 - ripple)
    diff = np.stack([base * 0.97, base * 0.985, base * 1.0], -1)
    # rugosidade ~0.6 com cristais (pontos mais lisos que brilham contra o sol)
    sparkle = (band(400, 900, 45, slope=0.0) > 2.6).astype(np.float32)
    rough = 0.62 + 0.08 * band(20, 80, 46) - 0.45 * sparkle
    save('snow', np.clip(diff, 0, 1), n, np.ones_like(base), np.clip(rough, 0.05, 1), h)


def ice():
    # placas com rachaduras finas, bolhas escuras, neve soprada nas bordas
    f1, f2 = worley(9, 51)
    plate_edge = np.clip(1 - (f2 - f1) / 0.03, 0, 1)
    fine_f1, fine_f2 = worley(40, 52)
    fine = np.clip(1 - (fine_f2 - fine_f1) / 0.02, 0, 1) * (band(2, 8, 53) > 0.2)
    bubbles = np.clip(band(60, 200, 54, slope=0.0) - 2.2, 0, 1)
    frost = np.clip(band(3, 20, 55) * 0.5 + plate_edge * 0.8, 0, 1)
    dark = np.array([0.035, 0.06, 0.08], np.float32)
    white = np.array([0.75, 0.8, 0.85], np.float32)
    t = np.clip(frost * 0.7 + fine * 0.35 + bubbles * 0.2, 0, 1)[..., None]
    diff = dark * (1 - t) + white * t
    h = 0.004 * frost - 0.002 * plate_edge - 0.001 * fine
    n = normals(h, 6.0, 1.0)
    rough = 0.08 + 0.5 * frost + 0.2 * fine
    save('ice', diff, n, np.ones_like(rough), np.clip(rough, 0, 1), h)


def blur(a, r):
    """Borra (periódico) com um gaussiano de raio r px, no espaço de Fourier."""
    g = np.exp(-0.5 * (K / (N / (2 * np.pi * r))) ** 2)
    return np.real(np.fft.ifft2(np.fft.fft2(a) * g)).astype(np.float32)


def splat(buf_h, buf_id, x, y, h, ids):
    """Pinta pontos (px, com a volta do ladrilho) guardando o mais alto: altura
    em buf_h e o dono (folha, folha de capim) em buf_id."""
    xi = np.mod(np.round(x).astype(np.int64), N)
    yi = np.mod(np.round(y).astype(np.int64), N)
    flat = yi * N + xi
    np.maximum.at(buf_h.reshape(-1), flat, h)
    win = h >= buf_h.reshape(-1)[flat] - 1e-7
    buf_id.reshape(-1)[flat[win]] = ids[win]


def forest_floor():
    # folhiço: folhas secas sobrepostas (as de cima por último), algumas
    # enroladas nas bordas, gravetos por cima e terra escura nos vãos
    rng = np.random.default_rng(71)
    tile, ppcm = 3.0, N / 300.0
    h = np.full((N, N), -1.0, np.float32)
    owner = np.full((N, N), -1, np.int64)
    shade = np.ones((N, N), np.float32)
    n = 21000
    cx, cy = rng.random(n) * N, rng.random(n) * N
    ang = rng.random(n) * 2 * np.pi
    half = (2.2 + 2.6 * rng.random(n)) * ppcm            # meio comprimento: folhas de 4,4–9,6 cm
    ratio = 0.38 + 0.25 * rng.random(n)
    layer = np.sort(rng.random(n))                        # a ordem de queda
    for s in range(0, n, 1500):
        e = min(n, s + 1500)
        for i in range(s, e):
            a = half[i]
            k = int(3 * a) + 2                                 # ~1,5 amostra por pixel: sem furos
            u, v = np.meshgrid(np.linspace(-1, 1, k), np.linspace(-1, 1, max(3, int(k * ratio[i]) + 2)))
            inside = np.abs(v) <= (1 - u ** 2) ** 0.8          # lâmina com ponta
            u, v = u[inside], v[inside]
            ca, sa = np.cos(ang[i]), np.sin(ang[i])
            x = cx[i] + a * (u * ca - v * ratio[i] * sa)
            y = cy[i] + a * (u * sa + v * ratio[i] * ca)
            curl = 0.25 * v ** 2 * (rng.random() < 0.5)
            hh = layer[i] + 0.02 * (1 - u ** 2) + curl * 0.05
            splat(h, owner, x, y, hh.astype(np.float32), np.full(u.shape, i, np.int64))
            # nervura central e o tom mudando da base pra ponta (guardado por pixel)
            xi, yi = np.mod(np.round(x).astype(np.int64), N), np.mod(np.round(y).astype(np.int64), N)
            mine = owner[yi, xi] == i
            shade[yi[mine], xi[mine]] = (1 - 0.35 * np.exp(-(v[mine] * ratio[i] * a / 0.8) ** 2)) * (0.9 + 0.2 * u[mine])
    # gravetos
    ntw = 260
    tx, ty, ta = rng.random(ntw) * N, rng.random(ntw) * N, rng.random(ntw) * 2 * np.pi
    tl = (8 + 22 * rng.random(ntw)) * ppcm
    tw = 1.5 + 2.5 * rng.random(ntw)
    for i in range(ntw):
        t = np.linspace(0, 1, int(tl[i]) + 2)
        off = np.linspace(-tw[i] / 2, tw[i] / 2, int(tw[i]) + 2)
        T, O = np.meshgrid(t, off)
        bend = 0.04 * tl[i] * np.sin(np.pi * T * (1 + rng.random()))
        x = tx[i] + np.cos(ta[i]) * T * tl[i] - np.sin(ta[i]) * (O + bend)
        y = ty[i] + np.sin(ta[i]) * T * tl[i] + np.cos(ta[i]) * (O + bend)
        splat(h, owner, x.ravel(), y.ravel(), np.full(x.size, 1.05 + 0.01 * (1 - (O.ravel() / (tw[i] / 2 + 1e-6)) ** 2), np.float32),
              np.full(x.size, n + i, np.int64))
    # cor: folhas (marrom, ferrugem, oliva, escuras, palha), gravetos cinza-marrom
    pal = np.array([[0.12, 0.06, 0.025], [0.19, 0.085, 0.03], [0.085, 0.065, 0.03], [0.055, 0.032, 0.016],
                    [0.21, 0.14, 0.06], [0.15, 0.09, 0.04]], np.float32)
    pick = rng.integers(0, len(pal), n)
    leaf_col = pal[pick] * (0.75 + 0.5 * rng.random((n, 1))).astype(np.float32)
    twig_col = np.array([0.09, 0.07, 0.05], np.float32) * (0.8 + 0.4 * rng.random((ntw, 1))).astype(np.float32)
    cols = np.concatenate([leaf_col, twig_col], 0)
    soil = np.stack([0.045, 0.03, 0.02]) * (0.8 + 0.25 * band(30, 200, 72)[..., None] * 0.3)
    has = owner >= 0
    diff = np.where(has[..., None], cols[np.maximum(owner, 0)] * shade[..., None], soil)
    # relevo: a pilha de folhas (ordem) vira altura de poucos mm; bordas escuras
    hr = np.where(has, h, -0.2)
    height = 0.006 * hr + 0.002 * band(4, 20, 73)
    edge = np.clip(np.abs(hr - blur(hr, 1.5)) * 6, 0, 1)
    diff = diff * (1 - 0.35 * edge[..., None])
    cav = np.clip(0.5 + (hr - blur(hr, 12)) * 1.5, 0, 1)
    ao = np.clip(0.45 + 0.55 * cav, 0, 1) * np.where(has, 1.0, 0.6)
    diff = diff * (0.55 + 0.45 * ao[..., None])
    rough = np.where(has, 0.82, 0.95) + 0.05 * band(50, 300, 74)        # folha seca é fosca (contra o sol brilhava demais)
    save('forest_floor', np.clip(diff, 0, 1), normals(height, tile, 1.0), ao, np.clip(rough, 0, 1), height)


def moss():
    # chão de grama baixa e musgo visto de cima: folhas de capim curtas em
    # traços, tufos mais escuros, manchas amareladas e terra nos vãos
    rng = np.random.default_rng(81)
    tile, ppcm = 3.0, N / 300.0
    h = np.full((N, N), -1.0, np.float32)
    owner = np.full((N, N), -1, np.int64)
    clump = band(3, 12, 82)
    n = 240000
    # mais folhas onde o tufo é denso (rejeição pela mancha); terra só em vãos pequenos
    x0, y0 = rng.random(n * 2) * N, rng.random(n * 2) * N
    keep = rng.random(n * 2) < np.clip(0.8 + 0.2 * clump[y0.astype(int) % N, x0.astype(int) % N], 0.05, 1)
    x0, y0 = x0[keep][:n], y0[keep][:n]
    n = len(x0)
    ang = rng.random(n) * 2 * np.pi
    ln = (1.5 + 4.0 * rng.random(n)) * ppcm
    S = 30
    t = np.linspace(0, 1, S, dtype=np.float32)
    for s in range(0, n, 20000):
        e = min(n, s + 20000)
        L = ln[s:e, None] * t[None, :]
        x = x0[s:e, None] + np.cos(ang[s:e])[:, None] * L
        y = y0[s:e, None] + np.sin(ang[s:e])[:, None] * L
        hh = (0.3 + 0.7 * t[None, :]) * (0.6 + 0.4 * rng.random((e - s, 1)))
        ids = np.repeat(np.arange(s, e), S)
        for dx, dy in ((0, 0), (0.7, 0.7)):
            splat(h, owner, (x + dx).ravel(), (y + dy).ravel(), hh.astype(np.float32).ravel(), ids)
    # cores: verde de grama (linear), algumas secas, mancha larga mais amarela
    greens = np.array([[0.045, 0.085, 0.02], [0.06, 0.10, 0.025], [0.035, 0.065, 0.018], [0.09, 0.11, 0.035], [0.12, 0.11, 0.05]], np.float32)
    pick = rng.choice(len(greens), n, p=[0.3, 0.25, 0.25, 0.12, 0.08])
    cols = greens[pick] * (0.75 + 0.5 * rng.random((n, 1))).astype(np.float32)
    has = owner >= 0
    soil = np.array([0.045, 0.036, 0.022], np.float32) * (0.8 + 0.4 * np.clip(band(20, 120, 85) * 0.3 + 0.5, 0, 1))[..., None]
    diff = np.where(has[..., None], cols[np.maximum(owner, 0)], soil)
    dry = np.clip(band(2, 8, 83) * 0.6, 0, 1)[..., None]
    diff = diff * (1 - dry) + diff * np.array([1.5, 1.2, 0.9], np.float32) * dry
    hr = np.where(has, h, -0.3)
    cav = np.clip(0.5 + (hr - blur(hr, 8)) * 1.2, 0, 1)
    ao = np.clip(0.4 + 0.6 * cav, 0, 1)
    diff = diff * (0.5 + 0.5 * ao[..., None]) * (0.85 + 0.3 * np.clip(clump * 0.3 + 0.5, 0, 1))[..., None]
    height = 0.01 * hr + 0.004 * clump
    rough = 0.9 + 0.05 * band(40, 200, 84)
    save('moss', np.clip(diff, 0, 1), normals(height, tile, 1.0), ao, np.clip(rough, 0, 1), height)


if __name__ == '__main__':
    import sys
    want = sys.argv[1:] or ['asphalt_cracks', 'snow', 'ice', 'forest_floor', 'moss']
    for name in want:
        globals()[name]()
