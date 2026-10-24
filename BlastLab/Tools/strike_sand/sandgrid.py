"""A areia do strike_chao só de areia (23/10 noite): os grãos do ejecta_strike2.py na grade da gravação, em componentes
separados (o grosso duro e o fino solto, a parte clara e a escura), guardados por quadro, pra misturar com ganhos, borrão e
contraste diferentes sem refazer o splat.

Base: sims/tools/strike_grid.py (a extinção física de cada grão, σ = 3·m/(ρ·d·V), em unidades da poeira do raymarch ÷ 5,5/m).
O que muda aqui:
- o grosso sai cru (sem o contraste) e o contraste/borrão vão pra mistura (mix): o borrão de ~1 voxel ANTES do contraste tira os
  degraus de 0,31 m que a v3 mostrava ampliada (a borda dura num campo que pula de 0 a saturado em um voxel);
- os componentes são guardados (cache/<grãos>/c_QQ.npz), assim a varredura de parâmetros custa só a mistura.
Eixos [z][y][x] da simulação (y ainda não virado; a montagem vira pro y da Unreal).
"""
import os

import numpy as np
from scipy import ndimage

RHO_P = 2650.0
DUST_EXT = 5.5
FINE_D = 0.2e-3
CLASSES = ((0.0, 0.3, 0.25), (0.3, 1.0, 0.45), (1.0, 2.5, 0.70), (2.5, 99.0, 1.0))


def splat(pos, w, n, vm, nz=None, wv=None):
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


def load_static(gdir):
    st = np.load(os.path.join(gdir, 'static.npz'))
    return {k: st[k] for k in ('d0', 't0', 'tb', 'kind', 'dark', 'times')}


def components(gdir, st, q, n, dom, trail=1.0 / 60.0, nk=3, cache=None):
    """O instante q → dict: cs/cd (o grosso e os grumos inteiros, claro/escuro, crus), fs/fd (o fino solto, já borrado pela
    idade), W/MV (Σσ e Σσ·v na meia resolução, só o grosso e só o fino: Wc, MVc, Wf, MVf), t."""
    if cache:
        p = os.path.join(cache, f'c_{q:02d}.npz')
        if os.path.exists(p):
            z = np.load(p)
            return {k: z[k] for k in z.files}
    z = np.load(os.path.join(gdir, f'snap_{q:02d}.npz'))
    idx, pos, vel, d, m, t = z['idx'], z['pos'].astype(np.float64), z['vel'].astype(np.float64), z['d'], z['m'], float(z['t'])
    vm = dom / n
    sig = 3.0 * m / (RHO_P * d * vm ** 3) / DUST_EXT
    dk = st['dark'][idx]
    tb = st['tb'][idx]
    free_fine = (d < FINE_D) & (t >= tb)
    sharp = ~free_fine

    def add(sel, blur_m):
        a = np.zeros((n, n, n))
        b = np.zeros((n, n, n))
        if not sel.any():
            return a, b
        p, v, s, k = pos[sel], vel[sel], sig[sel], dk[sel]
        for j in range(nk):
            pj = p - v * (trail * j / max(nk - 1, 1))
            a += splat(pj, s * (1 - k) / nk, n, vm)
            b += splat(pj, s * k / nk, n, vm)
        if blur_m > 0:
            sv = blur_m / vm
            a = ndimage.gaussian_filter(a, sv, mode='constant', truncate=2.5)
            b = ndimage.gaussian_filter(b, sv, mode='constant', truncate=2.5)
        return a, b

    cs, cd = add(sharp, 0.0)
    fs = np.zeros((n, n, n))
    fd = np.zeros((n, n, n))
    age = t - tb
    for a0, a1, bl in CLASSES:
        a, b = add(free_fine & (age >= a0) & (age < a1), bl)
        fs += a
        fd += b
    nf = n // 2
    Wc, MVc = splat(pos[sharp], sig[sharp], nf, dom / nf, wv=vel[sharp]) if sharp.any() else (np.zeros((nf,) * 3), np.zeros((nf,) * 3 + (3,)))
    Wf, MVf = splat(pos[free_fine], sig[free_fine], nf, dom / nf, wv=vel[free_fine]) if free_fine.any() else (np.zeros((nf,) * 3), np.zeros((nf,) * 3 + (3,)))
    out = dict(cs=cs.astype(np.float32), cd=cd.astype(np.float32), fs=fs.astype(np.float32), fd=fd.astype(np.float32),
               Wc=Wc.astype(np.float32), MVc=MVc.astype(np.float32), Wf=Wf.astype(np.float32), MVf=MVf.astype(np.float32), t=np.float32(t))
    if cache:
        os.makedirs(cache, exist_ok=True)
        np.savez(os.path.join(cache, f'c_{q:02d}.npz'), **out)
    return out


def mix(c, vm, g_coarse, g_fine, blur_c=0.0, pot=1.0, c_ref=0.6):
    """Os componentes → (areia, escura, W, MV) em unidades da poeira. O grosso: borrão gaussiano de blur_c m (antes do contraste:
    a borda fica dura mas sem o degrau de um voxel), depois (s/c_ref)^pot na soma (o perfil bimodal: o miolo mais denso, a franja
    mais rala), a escura na mesma proporção."""
    cs, cd = c['cs'] * g_coarse, c['cd'] * g_coarse
    if blur_c > 0:
        sv = blur_c / vm
        cs = ndimage.gaussian_filter(cs, sv, mode='constant', truncate=2.5)
        cd = ndimage.gaussian_filter(cd, sv, mode='constant', truncate=2.5)
    if pot != 1.0:
        tot = cs + cd
        k = np.where(tot > 1e-6, c_ref * (np.maximum(tot, 0.0) / c_ref) ** pot / np.maximum(tot, 1e-6), 0.0)
        cs = cs * k
        cd = cd * k
    sand = cs + c['fs'] * g_fine
    dark = cd + c['fd'] * g_fine
    W = c['Wc'] * g_coarse + c['Wf'] * g_fine
    MV = c['MVc'] * g_coarse + c['MVf'] * g_fine
    return sand.astype(np.float32), dark.astype(np.float32), W, MV
