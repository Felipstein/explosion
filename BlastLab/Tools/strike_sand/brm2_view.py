"""Folha de prova de uma gravação BRM2 (23/10): o que o jogo lê (fuligem, calor, poeira), desenhado fora do jogo.

  python brm2_view.py <gravacao.brm2> <saida.png> [tempos_s=0.1,0.3,...] [vistas=rts,lado,cima] [escala=1] [tanque=x,y]

Um raymarch simples em numpy (não é o do jogo; é pra conferir forma, tamanho e tempo contra os números do estudo):
- fuligem: extinção 13,5/m × √-decodificada, albedo 0,3 (a TankSoot); poeira: 5,5/m, cor (0,90; 0,72; 0,50), albedo 0,8 (a
  TankDust); o fogo: a fuligem quente emite (o corpo negro de T/1,6, como o raymarch: só a fuligem emite);
- o sol a 35° de elevação, de trás-esquerda da câmera de RTS, com a sombra marchada (a fumaça faz sombra no chão);
- o chão de areia com linhas a cada 5 m (a cada 10 m mais fortes) e o tanque (7,9 × 3,6 × 2,4 m) pra escala;
- as vistas: 'rts' (40° de cima, olhando pra +Y: o rumo do míssil, +X, sai pra direita), 'lado' (horizontal, o mesmo
  rumo) e 'cima' (de cima, +X pra direita, +Y pra cima).
"""
import json
import os
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

sys.path.insert(0, r'C:\Users\luisf\explosion\BlastLab\Tools')
from brm2_downsample import read_brm2  # noqa: E402

SOOT_EXT, DUST_EXT = 13.5, 5.5
SOOT_C = np.array([1.0, 0.97, 0.94]) * 0.30
DUST_C = np.array([0.90, 0.72, 0.50]) * 0.80
SUN = np.array([1.0, 0.96, 0.9]) * 2.2
AMB = np.array([0.55, 0.62, 0.75]) * 0.55


def blackbody(t):
    """t = T/1,6 em 0..1 → cor e intensidade aproximadas (laranja-escuro → amarelo-branco)."""
    t = np.clip(t, 0.0, 1.0)
    k = np.clip((t - 0.18) / 0.82, 0.0, 1.0)
    col = np.stack([np.ones_like(k), 0.25 + 0.65 * k, 0.05 + 0.6 * k ** 2], -1)
    return col * (k ** 2.2)[..., None] * 14.0


def times_of(head):
    n, dur, curve = head['frames'], head['duration'], head['curve']
    return np.array([dur * (q / (n - 1)) ** curve for q in range(n)])


def load(path, want):
    head, frames = read_brm2(path)
    ts = times_of(head)
    pick = {int(np.argmin(np.abs(ts - t))) for t in want}
    out = {}
    for q, (A, F) in enumerate(frames):
        if q in pick:
            out[q] = A.copy()
        if q >= max(pick):
            break
    return head, ts, out


def fields(A, head, tank, scale):
    """A [z][y][x] → os campos num grade com chão (z de -1 m): sigma, cor espalhada (3) e emissão (3); eixos x, y, z."""
    n, D = head['res'], head['domain'] * scale
    vm = D / n
    a = A.astype(np.float32) / 255.0
    soot = 4.0 * a[..., 0] ** 2
    temp = a[..., 1]
    dust = 2.5 * a[..., 2] ** 2
    nf = int(round(1.0 / vm)) + 1                        # o chão: ~1 m de laje opaca
    shape = (n + nf, n, n)
    sig = np.zeros(shape, np.float32)
    col = np.zeros(shape + (3,), np.float32)
    emi = np.zeros(shape + (3,), np.float32)
    ss, sd = SOOT_EXT * soot / scale, DUST_EXT * dust / scale     # a espessura óptica não cresce com a escala (a do jogo)
    st = ss + sd
    sig[nf:] = st
    w = np.where(st > 1e-6, ss / np.maximum(st, 1e-6), 0.0)[..., None]
    col[nf:] = w * SOOT_C + (1 - w) * DUST_C
    emi[nf:] = blackbody(temp) * (ss / np.maximum(st, 1e-6))[..., None]
    # o chão com as linhas
    c = (np.arange(n) + 0.5) * vm - D / 2
    X, Y = np.meshgrid(c, c)                              # [y][x]
    g = np.ones((n, n, 3), np.float32) * np.array([0.62, 0.50, 0.36])
    for step, k in ((5.0, 0.88), (10.0, 0.75)):
        line = (np.abs((X + step / 2) % step - step / 2) < vm * 0.6) | (np.abs((Y + step / 2) % step - step / 2) < vm * 0.6)
        g[line] *= k
    sig[:nf] = 40.0
    col[:nf] = g[None]
    # o tanque (caixa opaca cinza-oliva)
    tx, ty = tank[0], tank[1]
    hx, hy = (1.8, 3.95) if len(tank) > 2 and tank[2] else (3.95, 1.8)   # o terceiro valor 1: o casco deitado em Y
    inx = np.abs(X - tx) < hx
    iny = np.abs(Y - ty) < hy
    zt = int(round(2.4 / vm))
    box = (inx & iny)
    for z in range(nf, nf + zt):
        sig[z][box] = 40.0
        col[z][box] = np.array([0.26, 0.27, 0.20])
    return sig, col, emi, vm, nf


def sun_transmittance(sig, vm, sun_dir):
    """A transmitância de cada voxel até o sol: varre de cima pra baixo, cada camada lê a de cima deslocada pro lado do sol."""
    sx, sy, sz = sun_dir
    acc = np.zeros_like(sig)
    L = vm * np.linalg.norm(sun_dir) / sz
    for z in range(sig.shape[0] - 2, -1, -1):
        up = ndimage.shift(acc[z + 1], (-sy / sz, -sx / sz), order=1, mode='constant', cval=0.0, prefilter=False)
        upsig = ndimage.shift(sig[z + 1], (-sy / sz, -sx / sz), order=1, mode='constant', cval=0.0, prefilter=False)
        acc[z] = up + 0.5 * (sig[z] + upsig) * L
    return np.exp(-acc)


def render(sig, col, emi, vm, nf, view, sun_dir, width_m, out_w=400):
    n_z, n_y, n_x = sig.shape
    Dx = n_x * vm
    # a luz por voxel: o sol com sombra mais o céu
    Ts = sun_transmittance(sig, vm, sun_dir)
    rad = col * (SUN[None, None, None] * Ts[..., None] * 0.5 + AMB[None, None, None]) + emi
    W = width_m
    H = W * 0.72
    if view == 'rts':
        pitch = np.radians(40.0)
        center = np.array([0.0, 0.0, 0.10 * W])
    elif view == 'lado':
        pitch = np.radians(6.0)
        center = np.array([0.0, 0.0, 0.33 * H])
    else:
        pitch = np.radians(89.9)
        center = np.array([0.0, 0.0, 0.0])
    fwd = np.array([0.0, np.cos(pitch), -np.sin(pitch)])
    up = np.array([0.0, np.sin(pitch), np.cos(pitch)])
    right = np.cross(fwd, up)
    nu, nv = out_w, int(out_w * 0.72)
    us = (np.arange(nu) + 0.5) / nu * W - W / 2
    vs = (np.arange(nv) + 0.5) / nv * H - H / 2
    ds = vm * 0.8
    reach = 0.75 * np.sqrt(3.0) * max(Dx, n_z * vm)
    ss = np.arange(-reach, reach, ds)
    img = np.zeros((nv, nu, 3), np.float32)
    trans = np.ones((nv, nu), np.float32)
    U, V = np.meshgrid(us, vs)
    for s in ss:
        p = center + U[..., None] * right + V[..., None] * up + s * fwd     # [v][u][3] mundo (x, y, z)
        ix = (p[..., 0] + Dx / 2) / vm - 0.5
        iy = (p[..., 1] + Dx / 2) / vm - 0.5
        iz = (p[..., 2]) / vm - 0.5 + nf
        coords = np.stack([iz.ravel(), iy.ravel(), ix.ravel()])
        sg = ndimage.map_coordinates(sig, coords, order=1, mode='constant', cval=0.0).reshape(nv, nu)
        if not sg.any():
            continue
        a = 1.0 - np.exp(-sg * ds)
        rr = np.stack([ndimage.map_coordinates(rad[..., c], coords, order=1, mode='constant', cval=0.0).reshape(nv, nu) for c in range(3)], -1)
        img += (trans * a)[..., None] * rr
        trans *= 1.0 - a
        if trans.max() < 0.01:
            break
    sky = np.array([0.70, 0.78, 0.88])
    img += trans[..., None] * sky
    img = img / (1.0 + img)                              # o tom (Reinhard)
    img = np.clip(img ** (1 / 2.2), 0, 1)
    return (img[::-1] * 255).astype(np.uint8)


def main():
    src, out = sys.argv[1], sys.argv[2]
    want = [float(x) for x in sys.argv[3].split(',')] if len(sys.argv) > 3 else [0.05, 0.15, 0.3, 0.6, 1.0, 1.6, 2.5, 4.0]
    views = sys.argv[4].split(',') if len(sys.argv) > 4 else ['rts', 'lado', 'cima']
    scale = float(sys.argv[5]) if len(sys.argv) > 5 else 1.0
    tank = tuple(float(x) for x in sys.argv[6].split(',')) if len(sys.argv) > 6 else (-16.0, 0.0)
    width = float(sys.argv[7]) if len(sys.argv) > 7 else 0.0
    head, ts, frames = load(src, want)
    sun = np.array([-0.45, -0.55, 0.70])
    sun /= np.linalg.norm(sun)
    rows = []
    for q in sorted(frames):
        sig, col, emi, vm, nf = fields(frames[q], head, tank, scale)
        tiles = []
        for v in views:
            im = Image.fromarray(render(sig, col, emi, vm, nf, v, sun, width if width > 0 else 0.8 * head['domain'] * scale))
            ImageDraw.Draw(im).text((5, 4), f'{v}  t={ts[q]:.2f}s  q{q}', fill=(255, 255, 255))
            tiles.append(im)
        W = sum(t.width for t in tiles)
        Hh = max(t.height for t in tiles)
        row = Image.new('RGB', (W, Hh), (30, 30, 30))
        x = 0
        for t in tiles:
            row.paste(t, (x, 0))
            x += t.width
        rows.append(row)
        print('VIEW', q, f'{ts[q]:.2f}s')
    sheet = Image.new('RGB', (max(r.width for r in rows), sum(r.height for r in rows)), (30, 30, 30))
    y = 0
    for r in rows:
        sheet.paste(r, (0, y))
        y += r.height
    sheet.save(out, quality=88)
    print('FOLHA', out, sheet.size, f'domínio {head["domain"] * scale:.1f} m, res {head["res"]}, {head["frames"]} quadros, {head["duration"]:.2f} s')


if __name__ == '__main__':
    main()
