"""Texturas de detalhe do chão do WorldForge, geradas por código (numpy).

Rodar com o Python do sistema (precisa de numpy e pillow):
  python WorldForge/Tools/make_textures.py
Gera WorldForge/Intermediate/Textures/*.png (o forge.ps1 build chama sozinho).

Cada camada é uma textura RGBA que se repete sem emenda (ruído filtrado no
espaço de Fourier é periódico por construção):
  R = variação do albedo (0.5 = cor do bioma; o material multiplica por 2·R)
  G, B = normal em X e Y (0.5 = plano), já nos eixos do mundo
  A = rugosidade
A cor vem do bioma (FWorldPalette, no C++): a textura só dá o desenho.
Tamanho de cada tile no mundo (material M_Terrain): grama 4 m, terra 6 m,
areia 8 m, rocha 10 m (triplanar). T_WaterN: normal de ondas, tile de 9 m.
"""
import os

import numpy as np
from PIL import Image

N = 1024
TOOLS = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(TOOLS, '..', 'Intermediate', 'Textures')

FREQ = np.fft.fftfreq(N) * N                      # ciclos por tile
K = np.sqrt(FREQ[None, :] ** 2 + FREQ[:, None] ** 2)


def band(lo, hi, seed, slope=-1.0):
    """Ruído periódico com energia entre lo e hi ciclos por tile (bordas suaves)."""
    rng = np.random.default_rng(seed)
    spec = np.fft.fft2(rng.standard_normal((N, N)))
    win = np.clip((K - lo * 0.7) / (lo * 0.3 + 1e-6), 0, 1) * np.clip((hi * 1.3 - K) / (hi * 0.3), 0, 1)
    out = np.real(np.fft.ifft2(spec * win * np.maximum(K, 1.0) ** slope))
    return (out / (out.std() + 1e-9)).astype(np.float32)


def worley(cells, seed):
    """Distância ao 1º e 2º ponto mais perto (periódico) e o id da célula do 1º."""
    rng = np.random.default_rng(seed)
    jit = rng.random((cells, cells, 2)).astype(np.float32)
    c = N / cells
    y, x = np.mgrid[0:N, 0:N].astype(np.float32)
    ci, cj = np.floor(y / c).astype(int), np.floor(x / c).astype(int)
    f1 = np.full((N, N), 1e9, np.float32)
    f2 = np.full((N, N), 1e9, np.float32)
    ident = np.zeros((N, N), np.int32)
    for di in (-1, 0, 1):
        for dj in (-1, 0, 1):
            ni, nj = ci + di, cj + dj
            j = jit[ni % cells, nj % cells]
            py, px = (ni + j[..., 0]) * c, (nj + j[..., 1]) * c
            d = np.sqrt((py - y) ** 2 + (px - x) ** 2) / c
            closer = d < f1
            f2 = np.where(closer, f1, np.minimum(f2, d))
            ident = np.where(closer, (ni % cells) * cells + (nj % cells), ident)
            f1 = np.where(closer, d, f1)
    return f1, f2, ident


def normals(h, tile_m, strength=1.0):
    """Normal (X, Y) de uma altura h em metros num tile de tile_m metros."""
    px = tile_m / N
    dx = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) / (2 * px)
    dy = (np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)) / (2 * px)
    n = np.stack([-dx * strength, -dy * strength, np.ones_like(h)], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return n


def save(name, albedo, n, rough):
    rgba = np.stack([np.clip(albedo, 0, 1), n[..., 0] * 0.5 + 0.5, n[..., 1] * 0.5 + 0.5, np.clip(rough, 0, 1)], -1)
    Image.fromarray((rgba * 255 + 0.5).astype(np.uint8), 'RGBA').save(os.path.join(OUT, name))
    print('texture', name, 'albedo mean %.3f' % albedo.mean())


def grass():
    clumps = band(6, 30, 1)
    tufts = band(30, 160, 2)
    blades = band(160, 420, 3, slope=0.0)
    h = 0.02 * clumps + 0.012 * tufts + 0.004 * blades       # m
    albedo = 0.5 + 0.09 * clumps + 0.06 * tufts + 0.05 * blades
    dry = np.clip(band(10, 60, 4) - 1.2, 0, 1)                # palha clara aqui e ali
    albedo += 0.12 * dry
    save('grass.png', albedo, normals(h, 4.0, 1.0), 0.88 + 0.05 * tufts)


def dirt():
    f1, f2, ident = worley(48, 5)
    pebble = np.clip(1.0 - f1 / 0.55, 0, 1) ** 0.6 * (np.random.default_rng(6).random(48 * 48)[ident] > 0.45)
    grain = band(80, 400, 7, slope=0.0)
    lumps = band(4, 24, 8)
    h = 0.012 * pebble + 0.002 * grain + 0.015 * lumps
    tone = np.random.default_rng(9).random(48 * 48)[ident]
    albedo = 0.5 + 0.06 * lumps + 0.04 * grain + pebble * (tone - 0.45) * 0.35
    save('dirt.png', albedo, normals(h, 6.0, 1.0), 0.92 - 0.12 * pebble)


def sand():
    warp = band(2, 8, 10)
    y, x = np.mgrid[0:N, 0:N].astype(np.float32) / N
    ripples = np.sin(2 * np.pi * (14 * y + 3 * x) + 1.6 * warp)    # 14 e 3 inteiros: periódico
    grain = band(150, 420, 11, slope=0.0)
    h = 0.006 * ripples + 0.001 * grain + 0.01 * band(3, 12, 12)
    albedo = 0.5 + 0.035 * ripples + 0.04 * grain + 0.05 * band(3, 20, 13)
    save('sand.png', albedo, normals(h, 8.0, 1.0), 0.95 + 0.02 * grain)


def rock():
    f1, f2, _ = worley(20, 14)
    cracks = np.clip(1.0 - (f2 - f1) / 0.08, 0, 1) ** 2
    y = np.mgrid[0:N, 0:N][0].astype(np.float32) / N
    strata = np.sin(2 * np.pi * 9 * y + 2.0 * band(2, 6, 15))       # camadas horizontais
    big = band(2, 12, 16)
    mid = band(12, 60, 17)
    fine = band(60, 300, 18)
    h = 0.12 * big + 0.04 * mid + 0.01 * fine + 0.03 * strata - 0.05 * cracks
    albedo = 0.5 + 0.08 * big + 0.06 * mid + 0.04 * fine + 0.04 * strata - 0.22 * cracks
    save('rock.png', albedo, normals(h, 10.0, 1.0), 0.78 + 0.06 * fine + 0.1 * cracks)


def water():
    # soma de ondas com número de onda inteiro (periódicas no tile)
    y, x = np.mgrid[0:N, 0:N].astype(np.float32) / N
    rng = np.random.default_rng(19)
    h = np.zeros((N, N), np.float32)
    for _ in range(24):
        kx, ky = rng.integers(-14, 15, 2)
        if kx == 0 and ky == 0:
            continue
        k = np.hypot(kx, ky)
        h += np.sin(2 * np.pi * (kx * x + ky * y) + rng.random() * 6.283) / k ** 1.4
    h *= 0.05 / h.std()
    n = normals(h, 9.0, 1.0)
    rgb = np.stack([n[..., 0] * 0.5 + 0.5, n[..., 1] * 0.5 + 0.5, n[..., 2] * 0.5 + 0.5], -1)
    Image.fromarray((rgb * 255 + 0.5).astype(np.uint8), 'RGB').save(os.path.join(OUT, 'water_n.png'))
    print('texture water_n.png')


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    grass()
    dirt()
    sand()
    rock()
    water()
