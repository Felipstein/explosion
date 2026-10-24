"""Os grãos do ejecta_strike2.py na grade da gravação (23/10, strike_chao v2), e a prova rápida só dos grãos.

  python strike_grid.py prova <pasta_graos> <saida.png> [dominio_m=40] [res=128] [ganho_grosso=1] [ganho_fino=1]

A extinção é a física de cada grão (Q = 2: σ = 3·m/(ρ·d·V) por voxel), em unidades da poeira do raymarch (÷ 5,5/m):
- o grosso e os grumos ainda inteiros: sem borrão, só o tent trilinear e o rastro de 1/60 s (a borda dura: o miolo do dedo
  satura, como a areia de verdade);
- o fino solto (< 0,2 mm, depois que o grumo se desfez): borrado pela idade desde a quebra, em 4 classes (σ de 0,25 a 1 m):
  o véu translúcido que se solta e paira;
- a parte escura (o jato central e o miolo dos jatos: a terra úmida de baixo e a fuligem da carga) num canal à parte, que a
  montagem põe na fuligem;
- a velocidade (pra mistura com movimento entre os quadros do raymarch) pesada pela extinção, na meia resolução.
Eixos da grade: [z][y][x] com x, y centrados no impacto e z do chão; y do mundo (o da simulação, Z pra cima, míssil pra +X).
A montagem (assemble_strike.py) vira o y pro eixo da Unreal como o conversor.
"""
import os
import sys

import numpy as np
from scipy import ndimage

RHO_P = 2650.0
DUST_EXT = 5.5
FINE_D = 0.2e-3
CLASSES = ((0.0, 0.3, 0.25), (0.3, 1.0, 0.45), (1.0, 2.5, 0.70), (2.5, 99.0, 1.0))   # (idade de, até, σ do borrão em m)


def splat(pos, w, n, vm, nz=None, wv=None):
    """Tent trilinear de pesos w (e w·v, se wv) numa grade [z][y][x] de n (nz em z) voxels de vm m, x/y centrados, z do 0."""
    nz = nz or n
    half = n * vm / 2
    g = np.empty_like(pos)
    g[:, 0] = (pos[:, 0] + half) / vm - 0.5
    g[:, 1] = (pos[:, 1] + half) / vm - 0.5
    g[:, 2] = pos[:, 2] / vm - 0.5
    i0 = np.floor(g).astype(np.int64)
    fr = (g - i0).astype(np.float32)
    flats, ws = [], []
    for c in range(8):
        o = np.array([(c >> 0) & 1, (c >> 1) & 1, (c >> 2) & 1])
        ii = i0 + o
        wc = np.prod(np.where(o, fr, 1 - fr), axis=1)
        ok = (ii[:, 0] >= 0) & (ii[:, 0] < n) & (ii[:, 1] >= 0) & (ii[:, 1] < n) & (ii[:, 2] >= 0) & (ii[:, 2] < nz)
        flats.append((ii[ok, 2] * n + ii[ok, 1]) * n + ii[ok, 0])
        ws.append((wc, ok))
    flat = np.concatenate(flats)
    size = nz * n * n
    out = np.bincount(flat, weights=np.concatenate([(w * wc)[ok] for wc, ok in ws]), minlength=size).reshape(nz, n, n)
    if wv is None:
        return out
    mom = [np.bincount(flat, weights=np.concatenate([(w * wv[:, k] * wc)[ok] for wc, ok in ws]), minlength=size).reshape(nz, n, n)
           for k in range(3)]
    return out, np.stack(mom, -1)


def load(gdir):
    st = np.load(os.path.join(gdir, 'static.npz'))
    return {k: st[k] for k in ('d0', 't0', 'tb', 'kind', 'dark', 'times')}


def fields(gdir, st, q, n, dom, g_coarse=1.0, g_fine=1.0, trail=1.0 / 60.0, nk=3, pot=1.0, c_ref=0.6):
    """O instante q → (areia, escura) em unidades da poeira, [z][y][x] n³ no domínio dom; e (Σσ, Σσ·v) na meia resolução."""
    z = np.load(os.path.join(gdir, f'snap_{q:02d}.npz'))
    idx, pos, vel, d, m, t = z['idx'], z['pos'].astype(np.float64), z['vel'].astype(np.float64), z['d'], z['m'], float(z['t'])
    vm = dom / n
    sig = 3.0 * m / (RHO_P * d * vm ** 3) / DUST_EXT                  # unidades da poeira por parcela num voxel
    dk = st['dark'][idx]
    tb = st['tb'][idx]
    free_fine = (d < FINE_D) & (t >= tb)
    sharp = ~free_fine
    sand = np.zeros((n, n, n))
    dark = np.zeros((n, n, n))

    def add(sel, gain, blur_m):
        nonlocal sand, dark
        if not sel.any():
            return
        p, v, s, k = pos[sel], vel[sel], sig[sel] * gain, dk[sel]
        a = np.zeros((n, n, n))
        b = np.zeros((n, n, n))
        for j in range(nk):
            pj = p - v * (trail * j / max(nk - 1, 1))
            a += splat(pj, s * (1 - k) / nk, n, vm)
            b += splat(pj, s * k / nk, n, vm)
        if blur_m > 0:
            sv = blur_m / vm
            a = ndimage.gaussian_filter(a, sv, mode='constant', truncate=2.5)
            b = ndimage.gaussian_filter(b, sv, mode='constant', truncate=2.5)
        sand += a
        dark += b

    add(sharp, g_coarse, 0.0)
    if pot != 1.0:
        # o contraste do grosso (o perfil bimodal da areia: o miolo do dedo e do grumo mais denso, a franja mais rala; a borda fica dura
        # sem borrar nada): (s/c_ref)^pot na soma, a parte escura na mesma proporção
        tot = sand + dark
        k = np.where(tot > 1e-6, c_ref * (np.maximum(tot, 0.0) / c_ref) ** pot / np.maximum(tot, 1e-6), 0.0)
        sand *= k
        dark *= k
    age = t - tb
    for a0, a1, bl in CLASSES:
        add(free_fine & (age >= a0) & (age < a1), g_fine, bl)
    # a velocidade pesada pela extinção, na meia resolução
    nf = n // 2
    w = sig * np.where(sharp, g_coarse, g_fine)
    W, MV = splat(pos, w, nf, dom / nf, wv=vel)
    return sand.astype(np.float32), dark.astype(np.float32), W.astype(np.float32), MV.astype(np.float32), t


def prova(gdir, out, dom=40.0, n=128, gc=1.0, gf=1.0):
    from PIL import Image, ImageDraw
    st = load(gdir)
    times = st['times']
    want = [0.1, 0.3, 0.6, 1.0, 1.5, 2.2, 3.2, 4.5, 6.0]
    qs = sorted({int(np.argmin(np.abs(times - t))) for t in want})
    vm = dom / n
    tiles = []
    for q in qs:
        if not os.path.exists(os.path.join(gdir, f'snap_{q:02d}.npz')):
            continue
        sand, dark, _, _, t = fields(gdir, st, q, n, dom, gc, gf)
        ext_s = DUST_EXT * sand
        ext_d = DUST_EXT * dark

        def img(axis, flip):
            ts = np.exp(-ext_s.sum(axis) * vm)
            td = np.exp(-ext_d.sum(axis) * vm)
            bg = np.array([0.93, 0.95, 0.98])
            rgb = bg * ts[..., None] + np.array([0.78, 0.62, 0.42]) * (1 - ts[..., None])
            rgb = rgb * td[..., None] + np.array([0.25, 0.19, 0.13]) * (1 - td[..., None])
            a = (rgb.clip(0, 1) * 255).astype(np.uint8)
            return a[::-1] if flip else a
        side = img(1, True)                         # [z][x], z pra cima
        top = img(0, True)                          # [y][x], y pra cima
        im = Image.fromarray(np.concatenate([side, np.full((2, n, 3), 90, np.uint8), top], 0))
        im = im.resize((im.width * 2, im.height * 2), Image.NEAREST)
        ImageDraw.Draw(im).text((3, 3), f't {t:.2f}s  max {sand.max():.2f}/{dark.max():.2f}', fill=(200, 0, 0))
        tiles.append(im)
    W, H = tiles[0].size
    cols = 5
    sh = Image.new('RGB', (W * cols, H * ((len(tiles) + cols - 1) // cols)), 'white')
    for i, im in enumerate(tiles):
        sh.paste(im, ((i % cols) * W, (i // cols) * H))
    sh.save(out)
    print('PROVA', out, sh.size)


if __name__ == '__main__':
    if sys.argv[1] == 'prova':
        a = sys.argv[2:]
        prova(a[0], a[1], *(float(x) for x in a[2:3]), *(int(x) for x in a[3:4]), *(float(x) for x in a[4:6]))
