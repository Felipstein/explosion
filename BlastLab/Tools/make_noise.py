"""Ruído de detalhe das explosões: porte do _buildNoise do WebGL (reference/webgl/js/fluid.js).

Campo curl de um potencial vetorial feito de fbm de value-noise PERIÓDICO
(hash no reticulado mod período): a textura ladrilha, e o curl não tem
divergência — o detalhe que ele desloca não "incha" nem "murcha" a fumaça.
Mesma resolução (64³), mesmo período (4), mesmas 3 oitavas e mesmo remapeamento
(c·0.06 + 0.5). O hash por seno em double dá outro sorteio que o da GPU, com
a mesma estatística.

E o ruído da erosão das bordas (o "upres" na hora do render, como as nuvens do
Horizon: Schneider, "Nubis", SIGGRAPH 2015/2017/2023): Worley periódico 128³,
uma oitava por canal, com 4, 8, 16 e 32 células por ladrilho. O material lê
1 − F1 (bolhas redondas, a "couve-flor") e come a casca rala da fumaça entre
elas. Cada canal sai equalizado (percentis 1–99 → 0–1), pra a mesma
"quantidade" de erosão valer em todas as oitavas.

Rodar: python BlastLab/Tools/make_noise.py
Saída (fora do git): Intermediate/Noise/curl_noise_64.bgra e worley_128.bgra
(BGRA8, x mais rápido)
"""
import os

import numpy as np

RES = 64
PERIOD = 4.0
TOOLS = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(TOOLS, '..', 'Intermediate', 'Noise')


def vhash(ix, iy, iz, per):
    ix, iy, iz = np.mod(ix, per), np.mod(iy, per), np.mod(iz, per)
    return np.mod(np.sin(ix * 127.1 + iy * 311.7 + iz * 74.7) * 43758.5453123, 1.0)


def vnoise(x, y, z, per):
    ix, iy, iz = np.floor(x), np.floor(y), np.floor(z)
    fx, fy, fz = x - ix, y - iy, z - iz
    fx, fy, fz = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy), fz * fz * (3 - 2 * fz)
    lerp = lambda a, b, t: a + (b - a) * t
    c = {}
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                c[dx, dy, dz] = vhash(ix + dx, iy + dy, iz + dz, per)
    x0 = lerp(lerp(c[0, 0, 0], c[1, 0, 0], fx), lerp(c[0, 1, 0], c[1, 1, 0], fx), fy)
    x1 = lerp(lerp(c[0, 0, 1], c[1, 0, 1], fx), lerp(c[0, 1, 1], c[1, 1, 1], fx), fy)
    return lerp(x0, x1, fz) * 2 - 1


def pfbm(x, y, z):
    s, a, n, sc = 0.0, 0.55, 0.0, 1.0
    for _ in range(3):
        s = s + a * vnoise(x * sc, y * sc, z * sc, PERIOD * sc)
        n += a
        sc *= 2
        a *= 0.5
    return s / n


def main():
    u = (np.arange(RES) + 0.5) / RES * PERIOD
    z, y, x = np.meshgrid(u, u, u, indexing='ij')       # [z][y][x]: x mais rápido
    e = PERIOD / RES
    o2 = np.array([1.7, 0.3, 2.9]) * PERIOD
    o3 = np.array([2.3, 3.1, 0.7]) * PERIOD
    p1 = lambda dx=0, dy=0, dz=0: pfbm(x + dx, y + dy, z + dz)
    p2 = lambda dx=0, dy=0, dz=0: pfbm(x + o2[0] + dx, y + o2[1] + dy, z + o2[2] + dz)
    p3 = lambda dx=0, dy=0, dz=0: pfbm(x + o3[0] + dx, y + o3[1] + dy, z + o3[2] + dz)
    cx = (p3(dy=e) - p3(dy=-e)) - (p2(dz=e) - p2(dz=-e))
    cy = (p1(dz=e) - p1(dz=-e)) - (p3(dx=e) - p3(dx=-e))
    cz = (p2(dx=e) - p2(dx=-e)) - (p1(dy=e) - p1(dy=-e))
    c = np.stack([cx, cy, cz], -1) / (2 * e)
    v = np.clip(c * 0.12 * 0.5 + 0.5, 0, 1)
    rgba = np.concatenate([v, np.ones(v.shape[:-1] + (1,))], -1)
    bgra = (rgba[..., [2, 1, 0, 3]] * 255 + 0.5).astype(np.uint8)
    os.makedirs(OUT, exist_ok=True)
    bgra.tofile(os.path.join(OUT, 'curl_noise_64.bgra'))
    print('noise', RES, 'mean', c.mean(axis=(0, 1, 2)).round(3), 'std', c.std(axis=(0, 1, 2)).round(3),
          'outside [0,1]: %.2f%%' % (100 * ((c * 0.06 + 0.5 < 0) | (c * 0.06 + 0.5 > 1)).mean()))
    worley()


WORLEY_RES = 128
WORLEY_CELLS = (4, 8, 16, 32)          # R, G, B, A


def worley_octave(cells, seed):
    """1 − F1 periódico: a distância ao ponto de semente mais próximo (um por
    célula, 27 vizinhas com volta no ladrilho), em unidades de célula."""
    rng = np.random.default_rng(seed)
    feat = rng.random((cells, cells, cells, 3)).astype(np.float32)
    u = ((np.arange(WORLEY_RES) + 0.5) / WORLEY_RES * cells).astype(np.float32)
    z, y, x = np.meshgrid(u, u, u, indexing='ij')
    p = np.stack([x, y, z], -1)
    c = np.floor(p).astype(np.int32)
    best = np.full(p.shape[:-1], np.inf, np.float32)
    for dz in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                n = c + np.array([dx, dy, dz], np.int32)
                f = n + feat[n[..., 2] % cells, n[..., 1] % cells, n[..., 0] % cells]
                best = np.minimum(best, ((p - f) ** 2).sum(-1))
    v = 1.0 - np.sqrt(best)
    lo, hi = np.percentile(v, (1, 99))
    return np.clip((v - lo) / (hi - lo), 0, 1)


def worley():
    ch = [worley_octave(n, 101 + i) for i, n in enumerate(WORLEY_CELLS)]
    rgba = np.stack(ch, -1)
    bgra = (rgba[..., [2, 1, 0, 3]] * 255 + 0.5).astype(np.uint8)
    os.makedirs(OUT, exist_ok=True)
    bgra.tofile(os.path.join(OUT, 'worley_128.bgra'))
    print('worley', WORLEY_RES, 'cells', WORLEY_CELLS, 'means', [round(float(c.mean()), 3) for c in ch])


if __name__ == '__main__':
    main()
