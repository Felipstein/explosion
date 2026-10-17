"""A cortina de ejeção do tiro na areia (16/10 noite, item 2 da lista do tiro; docs/TIRO_E_ACERTO.md, "O tiro na areia").

O leque escuro da arte 4.1 é a terra que a cratera joga pra fora. Um gás (o Mantaflow da v7) não carrega essa terra:
o jato perdia a força em 3–5 m. Aqui ela é simulada como grãos, cada parcela com o seu diâmetro, lançada pelas leis de
escala da ejeção e freada pelo ar:
- a velocidade de saída cai com a distância do centro: v = C1·√(gR)·(x/R)^(−1/μ), areia μ = 0,41 e C1 = 0,55
  (Housen e Holsapple 2011, Icarus 211); limitada a VMAX;
- o ângulo de saída fica entre 45° e 65° do chão (areia sob projétil, 56–63°: PLOS ONE 2022, e0265546);
- a cortina sai em raios (os "dedos" da ejeção, Bergeron et al. 1998, das cargas em areia): parte das parcelas se junta
  em torno de 9 a 13 rumos sorteados, o resto espalha;
- o arrasto do ar pela esfera de Schiller e Naumann (Cd = 24/Re·(1 + 0,15·Re^0,687), Re < 1000; 0,44 acima), integrado
  semi-implícito (os grãos finos respondem em ~10 ms);
- o gás quente do buraco sobe e puxa os finos: uma coluna de subida que decai em ~0,8 s.
Só a fração fina (40 µm a 1 mm) vira volume: é ela que escurece o ar (a extinção por massa ∝ 1/d). Os torrões e os
riscos grossos seguem como partículas nos destroços (BlastDebris::StartGroundImpact). A extinção de cada parcela é
Q·A/V (Q = 2), espalhada na grade e alisada por um núcleo que cresce com a idade (a turbulência mistura).

  python BlastLab/Tools/ejecta_curtain.py <pasta> [chave=valor ...]        → os quadros .npz e uma folha de prova
  blender -b --factory-startup -P BlastLab/Tools/ejecta_curtain.py -- <pasta> --vdb   → os .vdb pro build_raymarch_vdb.py

As chaves: parcelas, raio (m, a cratera), vmax, ang_min, ang_max, raios_frac, semente, quadros, qps, voxel, subida,
pico (o máximo da densidade no VDB, na escala do Mantaflow), d_min, d_max (m), massa (kg no ar), mist0 e mist1
(o núcleo da mistura turbulenta: mist0 + mist1·√t, em m).
"""
import glob
import os
import sys

import numpy as np

argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
OUT = argv[0]
VDB_MODE = '--vdb' in argv
P = dict(parcelas=80000, raio=1.5, vmax=42.0, ang_min=45.0, ang_max=65.0, raios_frac=0.65, semente=7, quadros=75, qps=30.0,
         voxel=0.2, subida=5.0, pico=2.0, d_min=40e-6, d_max=2.0e-3, meio=12.0, alto=18.0, massa=600.0, mist0=0.12, mist1=0.45)
for a in argv[1:]:
    if '=' in a:
        k, v = a.split('=', 1)
        P[k] = type(P[k])(float(v)) if k in P else float(v)

G = 9.81
RHO_P, RHO_A, NU = 2650.0, 1.2, 1.5e-5


def sim():
    rng = np.random.default_rng(int(P['semente']))
    N = int(P['parcelas'])
    R = P['raio']
    # o lugar de saída: uniforme na área entre 0,15 R e R
    r0 = R * np.sqrt(rng.uniform(0.15 ** 2, 1.0, N))
    # os rumos: raios sorteados (9 a 13, ângulos e pesos ao acaso) mais o espalhado
    nr = int(rng.integers(9, 14))
    centers = rng.uniform(0, 2 * np.pi, nr)
    weights = rng.uniform(0.4, 1.0, nr)
    pick = rng.choice(nr, N, p=weights / weights.sum())
    in_ray = rng.uniform(0, 1, N) < P['raios_frac']
    az = np.where(in_ray, centers[pick] + rng.normal(0, np.radians(5.0), N), rng.uniform(0, 2 * np.pi, N))
    # a velocidade (Housen e Holsapple), com espalhamento de ±25 %
    v0 = 0.55 * np.sqrt(G * R) * (r0 / R) ** (-1.0 / 0.41) * rng.uniform(0.75, 1.25, N)
    v0 = np.minimum(v0, P['vmax'] * rng.uniform(0.85, 1.0, N))
    el = np.radians(rng.uniform(P['ang_min'], P['ang_max'], N))
    # os grãos: o diâmetro log-uniforme; a massa da parcela segue a do tamanho (a curva granulométrica plana em log)
    d = np.exp(rng.uniform(np.log(P['d_min']), np.log(P['d_max']), N))
    t0 = 0.12 * (r0 / R) ** 2                       # o miolo sai primeiro, a borda depois (a escavação dura ~0,1 s)
    pos = np.stack([r0 * np.cos(az), r0 * np.sin(az), np.full(N, 0.05)], 1)
    vel = np.stack([v0 * np.cos(el) * np.cos(az), v0 * np.cos(el) * np.sin(az), v0 * np.sin(el)], 1)
    alive = np.ones(N, bool)
    area_per_mass = 3.0 / (2.0 * RHO_P * d)          # m²/kg
    mass = np.ones(N) / N                             # a massa total = 1 (normalizada no fim pelo pico)

    def air(p, t):
        # a coluna do gás quente do buraco: sobe no meio e decai; perto do chão empurra pra fora
        rr = np.hypot(p[:, 0], p[:, 1])
        w = P['subida'] * np.exp(-t / 0.8) * np.exp(-(rr / 2.5) ** 2) * np.exp(-p[:, 2] / 10.0)
        out = 3.0 * np.exp(-t / 0.4) * np.exp(-p[:, 2] / 0.8) * np.clip(rr / 3.0, 0, 1) * np.exp(-(rr / 6.0) ** 2)
        ux = np.where(rr > 1e-3, out * p[:, 0] / np.maximum(rr, 1e-3), 0.0)
        uy = np.where(rr > 1e-3, out * p[:, 1] / np.maximum(rr, 1e-3), 0.0)
        return np.stack([ux, uy, w], 1)

    # o gás empoeirado (o acoplamento de duas vias, aproximado): onde a carga de grãos passa muito da massa do ar, o ar
    # anda junto com eles (a velocidade de equilíbrio do "dusty gas", (ρ_ar·u + L·v̄)/(ρ_ar + L)); é o que leva os finos
    # no jato denso. O jato abre, a carga cai e os finos se soltam e ficam pairando (a cortina escura que desce)
    CELL = 0.5
    half, top = P['meio'], P['alto']
    cn = np.array([int(2 * half / CELL), int(2 * half / CELL), int(top / CELL)])
    m_parcel = P['massa'] / N

    def coupled(p, v, ub):
        ci = np.floor((p + [half, half, 0.0]) / CELL).astype(int)
        ok = np.all((ci >= 0) & (ci < cn), axis=1)
        flat = np.where(ok, np.ravel_multi_index(tuple(np.clip(ci, 0, cn - 1).T), cn), 0)
        M = np.bincount(flat[ok], minlength=int(np.prod(cn))) * m_parcel
        MV = np.stack([np.bincount(flat[ok], weights=v[ok, k], minlength=int(np.prod(cn))) * m_parcel for k in range(3)], 1)
        L = M[flat] / CELL ** 3                                     # kg de grão por m³
        vbar = MV[flat] / np.maximum(M[flat], 1e-9)[:, None]
        c = np.where(ok, L / (L + RHO_A), 0.0)[:, None]
        return ub + c * (vbar - ub)

    frames = int(P['quadros'])
    fps = P['qps']
    sub = 12                                          # passos por quadro (dt ≈ 2,8 ms)
    dt = 1.0 / fps / sub
    t = 0.0
    snaps = []
    for f in range(frames):
        snaps.append((t, pos.copy(), vel.copy(), alive & (t >= t0)))
        for _ in range(sub):
            go = alive & (t >= t0)
            if go.any():
                u = coupled(pos[go], vel[go], air(pos[go], t))
                vr = vel[go] - u
                sp = np.linalg.norm(vr, axis=1) + 1e-9
                Re = sp * d[go] / NU
                Cd = np.where(Re < 1000.0, 24.0 / np.maximum(Re, 1e-6) * (1 + 0.15 * Re ** 0.687), 0.44)
                k = 0.75 * (RHO_A / RHO_P) * Cd * sp / d[go]
                g = np.array([0.0, 0.0, -G])
                v = (vel[go] + dt * (g + k[:, None] * u)) / (1.0 + dt * k[:, None])
                vel[go] = v
                pos[go] += v * dt
                landed = go.copy()
                landed[go] = pos[go][:, 2] <= 0.0
                alive &= ~landed
            t += dt
    return snaps, d, mass, area_per_mass


def box_blur(a, r, axis):
    if r <= 0:
        return a
    S = np.cumsum(np.pad(a, [(r + 1, r) if i == axis else (0, 0) for i in range(a.ndim)]), axis=axis)
    n = a.shape[axis]
    hi = np.take(S, np.arange(2 * r + 1, 2 * r + 1 + n), axis=axis)
    lo = np.take(S, np.arange(0, n), axis=axis)
    return (hi - lo) / (2 * r + 1)


def blur(a, sigma_vox):
    # três passes de caixa ≈ gaussiana (Wells 1986): a meia largura de cada caixa pela variância
    if sigma_vox < 0.5:
        return a
    r = max(1, int(round((np.sqrt(12 * sigma_vox ** 2 / 3 + 1) - 1) / 2)))
    for ax in range(3):
        for _ in range(3):
            a = box_blur(a, r, ax)
    return a


def grids(snaps, d, mass, apm):
    vx = P['voxel']
    half, top = P['meio'], P['alto']
    nx = int(round(2 * half / vx))
    nz = int(round(top / vx))
    out = []
    for (t, pos, vel, on) in snaps:
        sig = np.zeros((nx, nx, nz), np.float32)
        mom = np.zeros((nx, nx, nz, 3), np.float32)
        if on.any():
            p = pos[on]
            g = np.stack([(p[:, 0] + half) / vx - 0.5, (p[:, 1] + half) / vx - 0.5, p[:, 2] / vx - 0.5], 1)
            i0 = np.floor(g).astype(int)
            fr = g - i0
            w_ext = (2.0 * apm[on] * mass[on] / vx ** 3).astype(np.float32)   # Q·A/V
            for c in range(8):
                o = np.array([(c >> 0) & 1, (c >> 1) & 1, (c >> 2) & 1])
                ii = i0 + o
                w = np.prod(np.where(o, fr, 1 - fr), axis=1)
                ok = np.all((ii >= 0) & (ii < [nx, nx, nz]), axis=1)
                np.add.at(sig, (ii[ok, 0], ii[ok, 1], ii[ok, 2]), (w * w_ext)[ok])
                for k in range(3):
                    np.add.at(mom[..., k], (ii[ok, 0], ii[ok, 1], ii[ok, 2]), (w * w_ext * vel[on][:, k])[ok])
        s = (P['mist0'] + P['mist1'] * np.sqrt(t)) / vx   # a mistura: 12 cm no começo, ~0,8 m aos 2,5 s (o padrão)
        sig = blur(sig, s)
        mom = np.stack([blur(mom[..., k], s) for k in range(3)], -1)
        v = np.where(sig[..., None] > 1e-6, mom / np.maximum(sig[..., None], 1e-6), 0.0)
        out.append((t, sig.astype(np.float32), v.astype(np.float32)))
    return out


def run_sim():
    os.makedirs(OUT, exist_ok=True)
    snaps, d, mass, apm = sim()
    g = grids(snaps, d, mass, apm)
    peak = max(float(s.max()) for _, s, _ in g)
    scale = P['pico'] / peak
    for f, (t, s, v) in enumerate(g):
        np.savez_compressed(os.path.join(OUT, f'ejecta_{f:04d}.npz'), sig=(s * scale).astype(np.float16), vel=v.astype(np.float16))
    print(f'EJECTA {len(g)} quadros, pico bruto {peak:.4g}, escala {scale:.4g}, parâmetros {P}')
    # a folha de prova: a espessura óptica vista de lado (y) e de cima (z), a cada 0,25 s
    try:
        from PIL import Image, ImageDraw
        ts = [k for k in range(len(g)) if k % max(1, int(P['qps'] * 0.25)) == 0][:10]
        tiles = []
        for k in ts:
            s = g[k][1] * scale * P['voxel']
            side = 1.0 - np.exp(-s.sum(axis=1).T[::-1])              # de lado: [z][x], o alto em cima
            topv = 1.0 - np.exp(-s.sum(axis=2).T)                    # de cima: [y][x]
            a = np.concatenate([side, np.full((3, side.shape[1]), 0.5), topv], 0) * 255
            im = Image.fromarray((255 - a).clip(0, 255).astype(np.uint8)).convert('RGB')
            ImageDraw.Draw(im).text((4, 4), f'{g[k][0]:.2f} s', fill=(200, 0, 0))
            tiles.append(im)
        W = max(i.width for i in tiles)
        H = sum(i.height for i in tiles[:5])
        sheet = Image.new('RGB', (W * 2, H), 'white')
        y = [0, 0]
        for n, im in enumerate(tiles):
            c = n // 5
            sheet.paste(im, (c * W, y[c]))
            y[c] += im.height
        sheet.save(os.path.join(OUT, 'prova.png'))
    except ImportError:
        pass


def run_vdb():
    import openvdb as vdb
    vx = P['voxel']
    files = sorted(glob.glob(os.path.join(OUT, 'ejecta_*.npz')))
    vdir = os.path.join(OUT, 'vdb')
    os.makedirs(vdir, exist_ok=True)
    half = P['meio']
    for f, path in enumerate(files):
        z = np.load(path)
        s = z['sig'].astype(np.float32)
        v = z['vel'].astype(np.float32)
        s[0, 0, 0] = max(s[0, 0, 0], 1e-3)          # os cantos opostos acesos: a caixa ativa fica centrada no impacto
        s[-1, -1, 0] = max(s[-1, -1, 0], 1e-3)
        dg = vdb.FloatGrid()
        dg.copyFromArray(s, tolerance=5e-4)
        dg.name = 'density'
        vg = vdb.Vec3SGrid()
        vg.copyFromArray(np.ascontiguousarray(v), tolerance=(1e-3, 1e-3, 1e-3))
        vg.name = 'velocity'
        for gr in (dg, vg):
            gr.transform = vdb.createLinearTransform(voxelSize=vx)
        vdb.write(os.path.join(vdir, f'ejecta_{f:04d}.vdb'), grids=[dg, vg])
    print(f'EJECTA VDB {len(files)} quadros em {vdir}')


if VDB_MODE:
    run_vdb()
else:
    run_sim()
