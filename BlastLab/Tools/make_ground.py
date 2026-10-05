"""Textura do chão: porte do _bakeGround do WebGL (reference/webgl/js/scene.js) pra numpy.

Mesmo campo de altura (grão em fbm periódico + duas famílias de rachaduras),
mesma tileabilidade (o hash é feito no reticulado mod período). Diferença: o
albedo sai em refletância física (solo nu ~0.17, rachadura escura), porque na
Unreal a exposição é física; o WebGL guardava um albedo de 6% escalado.

Rodar com o Python do sistema (precisa de numpy e pillow):
  python BlastLab/Tools/make_ground.py
Gera Intermediate/Ground/ground_A.png (cor sRGB + rugosidade no alfa) e
ground_N.png (normal em espaço de mundo: R = x, G = y, já no sistema da Unreal).
"""
import os

import numpy as np
from PIL import Image

RES = 1024
PERIOD = 8.0
TILE_M = 18.0                       # metros cobertos por um tile (groundTile)
TOOLS = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(TOOLS, '..', 'Intermediate', 'Ground')


def ph2(ix, iy, per):
    ix = np.mod(ix, per)
    iy = np.mod(iy, per)
    return np.mod(np.sin(ix * 127.1 + iy * 311.7) * 43758.5453123, 1.0)


def pn2(px, py, per):
    ix, iy = np.floor(px), np.floor(py)
    fx, fy = px - ix, py - iy
    fx = fx * fx * (3 - 2 * fx)
    fy = fy * fy * (3 - 2 * fy)
    a, b = ph2(ix, iy, per), ph2(ix + 1, iy, per)
    c, d = ph2(ix, iy + 1, per), ph2(ix + 1, iy + 1, per)
    return ((a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy) * 2 - 1


def pfbm2(px, py, per, octs):
    s = np.zeros_like(px)
    a, n, sc = 0.5, 0.0, 1.0
    for _ in range(octs):
        s += a * pn2(px * sc, py * sc, per * sc)
        n += a
        sc *= 2
        a *= 0.5
    return s / n


def fields(px, py):
    grit = pfbm2(px * 4, py * 4, PERIOD * 4, 3)
    cr = np.clip(1 - np.abs(pn2(px * 2, py * 2, PERIOD * 2)), 0, 1) ** 18
    cr2 = np.clip(1 - np.abs(pn2(px * 5, py * 5, PERIOD * 5)), 0, 1) ** 22
    return grit, cr, cr2


def height(px, py):
    grit, cr, cr2 = fields(px, py)
    return grit * 0.35 - (cr + cr2 * 0.7) * 1.4


def main():
    u = (np.arange(RES) + 0.5) / RES
    px, py = np.meshgrid(u * PERIOD, u * PERIOD)
    grit, cr, cr2 = fields(px, py)
    g01 = grit * 0.5 + 0.5
    cracks = np.clip(cr + cr2 * 0.7, 0, 1)

    # refletância: média do WebGL (0.058) levada a solo nu (0.17), mesma variação
    base = np.array([0.058, 0.055, 0.052]) * (0.17 / 0.058)
    alb = base[None, None, :] * (0.80 + 0.40 * g01)[..., None] * (1 - cracks * 0.70)[..., None]
    rough = 0.90 - 0.16 * g01

    # normal pelas diferenças do campo de altura, em unidades de mundo (o
    # mesmo fator 0.16 do WebGL)
    e = PERIOD / RES
    h0 = height(px, py)
    hx = height(px + e, py)
    hy = height(px, py + e)
    w = TILE_M / PERIOD
    nx = -(hx - h0) / e * 0.16 / w
    ny = -(hy - h0) / e * 0.16 / w
    inv = 1 / np.sqrt(nx * nx + ny * ny + 1)
    nx, ny = nx * inv, ny * inv

    def srgb(c):
        c = np.clip(c, 0, 1)
        return np.where(c <= 0.0031308, 12.92 * c, 1.055 * c ** (1 / 2.4) - 0.055)

    os.makedirs(OUT, exist_ok=True)
    A = np.dstack([srgb(alb), rough])
    Image.fromarray((A * 255 + 0.5).astype(np.uint8), 'RGBA').save(os.path.join(OUT, 'ground_A.png'))
    # textura: linha 0 é v=0. No WebGL o eixo v da textura é o z do mundo, que
    # na Unreal vira y: a normal (x, z) do WebGL é (x, y) aqui
    N = np.dstack([nx * 0.5 + 0.5, ny * 0.5 + 0.5, np.ones_like(nx)])
    Image.fromarray((N * 255 + 0.5).astype(np.uint8), 'RGB').save(os.path.join(OUT, 'ground_N.png'))
    print('ground', RES, 'mean albedo', float(alb.mean()), 'mean roughness', float(rough.mean()))


if __name__ == '__main__':
    main()
