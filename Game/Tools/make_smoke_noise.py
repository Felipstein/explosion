"""O ruído da fumaça do míssil (16/10): uma textura de 256² que se repete sem emenda, gerada aqui (numpy) e importada pelo
build_content.py (missile_smoke) pra /Game/FX/MissileSmoke/T_SmokeNoise.

Os canais (todos lineares, 0..1, média ~0,5):
  R: Perlin-Worley (o "billow" das nuvens: o fBm do ruído de gradiente modulado pelo Worley invertido; Schneider 2015,
     "The Real-time Volumetric Cloudscapes of Horizon Zero Dawn", SIGGRAPH Advances): os rolos da borda da fumaça;
  G: fBm mais fino (o detalhe que gira dentro do puff);
  B: Worley invertido fino (as bolhas pequenas da erosão);
  A: 1.
Uso: python Game/Tools/make_smoke_noise.py  (escreve Game/Tools/data/T_SmokeNoise.png)
"""
import os

import numpy as np
from PIL import Image

N = 256
rng = np.random.default_rng(16102026)


def fbm(beta, octave_cut=2.0):
    # ruído com espectro de potência ∝ k^−β, pela FFT (periódico por construção: repete sem emenda)
    w = rng.standard_normal((N, N))
    f = np.fft.fft2(w)
    ky = np.fft.fftfreq(N)[:, None]
    kx = np.fft.fftfreq(N)[None, :]
    k = np.sqrt(kx * kx + ky * ky)
    k[0, 0] = 1.0
    amp = k ** (-beta / 2.0)
    amp[k < octave_cut / N] = 0.0
    amp[0, 0] = 0.0
    out = np.real(np.fft.ifft2(f * amp))
    out = (out - out.mean()) / (out.std() + 1e-9)
    return out


def worley(cells):
    # a distância ao ponto mais perto numa grade de cells² pontos, com volta (periódico)
    pts = (np.stack(np.meshgrid(np.arange(cells), np.arange(cells), indexing='xy'), -1).reshape(-1, 2) + rng.random((cells * cells, 2))) / cells
    yy, xx = np.mgrid[0:N, 0:N] / N
    best = np.full((N, N), 10.0)
    for px, py in pts:
        dx = np.abs(xx - px)
        dy = np.abs(yy - py)
        dx = np.minimum(dx, 1.0 - dx)
        dy = np.minimum(dy, 1.0 - dy)
        best = np.minimum(best, np.sqrt(dx * dx + dy * dy))
    best *= cells
    return np.clip(best, 0.0, 1.0)


def to01(a):
    lo, hi = np.percentile(a, 1.0), np.percentile(a, 99.0)
    return np.clip((a - lo) / (hi - lo + 1e-9), 0.0, 1.0)


perlin = to01(fbm(3.0, 2.0))
w_big = 1.0 - worley(5)
billow = to01(perlin * 0.55 + w_big * 0.45 + 0.35 * perlin * w_big)
fine = to01(fbm(3.2, 4.0))
w_fine = to01(1.0 - worley(12))
img = np.stack([billow, fine, w_fine, np.ones_like(billow)], -1)
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data', 'T_SmokeNoise.png')
os.makedirs(os.path.dirname(out), exist_ok=True)
Image.fromarray((img * 255.0 + 0.5).astype(np.uint8), 'RGBA').save(out)
print('ruído da fumaça:', out, 'médias', [round(float(img[..., c].mean()), 3) for c in range(3)])
