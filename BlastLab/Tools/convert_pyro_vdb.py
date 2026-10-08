"""Converte o cache OpenVDB do Mantaflow (blender_pyro_tank.py) no bake bruto do BlastLab.

Uso (no Python do Blender, que traz o módulo openvdb e o numpy):
    blender -b --factory-startup -P convert_pyro_vdb.py -- <cache_dir> <nome> [resolucao]
    blender -b --factory-startup -P convert_pyro_vdb.py -- <pasta_vdb> <nome> <resolucao> embergen <m_por_voxel> [qps] [dust k]
    blender -b --factory-startup -P convert_pyro_vdb.py -- <cache_dir> <nome> <resolucao> loop <início_s> <duração_s> <emenda_s>

O modo loop (fogo da carcaça, blender_pyro_wreck.py) recorta a janela
[início, início + duração + emenda] em passos de 2 quadros (15 qps, os vetores
de movimento misturam) e emenda: os primeiros quadros da saída misturam o
começo com o trecho depois do fim, então o último quadro encosta no primeiro.

O segundo modo lê uma sequência do EmberGen (as simulações CC0 da JangaFX:
grades density, flames, temperature, sem velocidade), recorta a união das
caixas de todos os quadros num cubo com o pé no chão (índice z = 0) e reamostra
pra <resolucao>³. A escala (m por voxel) vem da arte: a bola tem de bater com
o vídeo (~1–1,2 tanque de largura no pico).

Sai em BlastLab/Intermediate/BakeRaw/<nome>/ no MESMO formato do convert_bake.py
(fNNN_a.bin RGBA8, fNNN_b.bin RGBA16F, meta.json), pra o resto do pipeline
(BlastLabEditorLib.create_animated_volume, o material, a luz) servir sem mudar:
- A.r² · 4 = fuligem ← density do Mantaflow × SOOT_K;
- A.g · 1.6 = temperatura ← calor + reação (flame), 1.6 = ~3300 K no material;
- A.b = poeira: zero (morte de tanque é combustível; a poeira é outra camada);
- B.r² = combustível ← flame; B.gba = velocidade em m/s, eixos da Unreal.

O domínio do Mantaflow (L × L no chão, H de altura) vira um cubo de lado H com
o pé no centro do chão, como o ExplosionVolume espera (canto em -D/2, -D/2, 0).
Eixos: Blender (x, y, z) → Unreal (x, -y, z) (a Unreal é de mão esquerda).

Quadros: todos no primeiro 0,5 s (o soco), um sim um não até 2 s, um a cada
três depois — os vetores de movimento do material misturam o intervalo.
"""
import glob
import json
import os
import sys

import numpy as np
import openvdb as vdb

argv = sys.argv[sys.argv.index('--') + 1:]
CACHE = os.path.abspath(argv[0])
NAME = argv[1]
RES = int(argv[2]) if len(argv) > 2 else 0
EMBERGEN = len(argv) > 3 and argv[3] == 'embergen'
LOOP = len(argv) > 3 and argv[3] == 'loop'
LOOP_START, LOOP_LEN, LOOP_XFADE = (float(argv[4]), float(argv[5]), float(argv[6])) if LOOP else (0.0, 0.0, 0.0)
EG_VOX = float(argv[4]) if EMBERGEN else 0.0
FPS = float(argv[5]) if EMBERGEN and len(argv) > 5 else 30.0
# 'dust': a densidade vira POEIRA (A.b, albedo alto, sem emissão), não fuligem
AS_DUST = EMBERGEN and len(argv) > 6 and argv[6] == 'dust'
# 'loop' <início_s> <duração_s> <emenda_s>: fumaça contínua do EmberGen (Smoke Plume) emendada como o fogo da carcaça
EG_LOOP = EMBERGEN and len(argv) > 6 and argv[6] == 'loop'
if EG_LOOP:
    LOOP, LOOP_START, LOOP_LEN, LOOP_XFADE = True, float(argv[7]), float(argv[8]), float(argv[9])
DUST_K = float(argv[7]) if AS_DUST and len(argv) > 7 else 2.0
TOOLS = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.abspath(os.path.join(TOOLS, '..'))
OUT = os.path.join(PROJECT, 'Intermediate', 'BakeRaw', NAME)

# fuligem: 1 m de casca grossa (density do Mantaflow ~1) deixa passar ~1%:
# σ = 13,5 · fuligem (LOOK sootExt) → fuligem ~0,35 por unidade de density
SOOT_K = 0.35
TEMP_HEAT, TEMP_FLAME = 0.30, 1.3
# EmberGen: temperatura normalizada (~2 no estouro, ~1,3 aos 2 s), flames ~0–1
EG_TEMP_HEAT, EG_TEMP_FLAME = 0.40, 0.55   # 07/10: 0.55/1.0 batia no teto 1.6 em milhares de voxels (bola branca chapada)
BOX_MARGIN = 2
# PYRO_META_ONLY=1: refaz só o meta.json (mesmos números), sem gravar os fNNN_a/b.bin
# (depois que o SVT existe o bruto pode sair, mas o build_explosion.py ainda lê o meta)
META_ONLY = os.environ.get('PYRO_META_ONLY') == '1'
SOOT_TOTALS = []
VELOCITY_BAND = 8


def pick_frames(n):
    keep = []
    for f in range(1, n + 1):
        t = (f - 1) / FPS
        if t <= 0.5 or (t <= 2.0 and (f - 1) % 2 == 0) or (f - 1) % 3 == 0:
            keep.append(f)
    return keep


def read(path, names):
    grids, _ = vdb.readAll(path)
    by = {g.name: g for g in grids}
    return {n: by.get(n) for n in names}


def dense(grid, shape, vec=False):
    a = np.zeros(shape + ((3,) if vec else ()), dtype=np.float32)
    if grid is not None:
        grid.copyToArray(a, ijk=(0, 0, 0))
    return a


def resample(a, n):
    """[z][y][x] → tamanhos n (um número ou um por eixo), interpolação linear separável entre centros."""
    sizes = n if isinstance(n, (tuple, list)) else (n, n, n)
    for axis in range(3):
        m, n = a.shape[axis], sizes[axis]
        if m == n:
            continue
        x = (np.arange(n) + 0.5) * m / n - 0.5
        i0 = np.clip(np.floor(x).astype(int), 0, m - 1)
        i1 = np.clip(i0 + 1, 0, m - 1)
        t = np.clip(x - np.floor(x), 0.0, 1.0).astype(np.float32)
        shape = [1] * a.ndim
        shape[axis] = -1
        t = t.reshape(shape)
        a = np.take(a, i0, axis=axis) * (1 - t) + np.take(a, i1, axis=axis) * t
    return a


class EmberGenSource:
    """Sequência do EmberGen: união das caixas ativas → cubo com o pé no chão."""

    def __init__(self, folder):
        import re
        self.files = sorted(glob.glob(os.path.join(folder, '*.vdb')),
                            key=lambda f: int(re.findall(r'(\d+)\.vdb$', f)[0]))
        lo, hi = [10 ** 9] * 3, [-10 ** 9] * 3
        for f in self.files[::max(1, len(self.files) // 12)] + self.files[-1:]:
            g = read(f, ['density'])['density']
            a, b = g.evalActiveVoxelBoundingBox()
            lo = [min(lo[i], a[i]) for i in range(3)]
            hi = [max(hi[i], b[i]) for i in range(3)]
        lo[2] = min(lo[2], 0)
        self.lo, self.dims = lo, [hi[i] - lo[i] + 1 for i in range(3)]
        self.side = max(self.dims)
        self.voxel = EG_VOX

    def count(self):
        return len(self.files)

    def fields(self, f, n):
        """(density, flame, heat) já reamostrados num cubo [z][y][x] de lado n, eixos da Unreal.
        Reamostra só a caixa ativa (em z, só a altura usada): um anel achatado de 975×975×137
        não vira um cubo de 975³ na memória."""
        g = read(self.files[f - 1], ['density', 'flames', 'temperature'])
        nz = max(1, int(round(n * self.dims[2] / self.side)))
        nx = max(1, int(round(n * self.dims[0] / self.side)))
        ny = max(1, int(round(n * self.dims[1] / self.side)))
        ox, oy = (n - nx) // 2, (n - ny) // 2
        out = []
        for name in ('density', 'flames', 'temperature'):
            c = np.zeros((n, n, n), dtype=np.float32)
            if g[name] is not None:
                a = np.zeros(self.dims, dtype=np.float32)
                g[name].copyToArray(a, ijk=tuple(self.lo))
                small = resample(np.ascontiguousarray(np.transpose(a, (2, 1, 0))[:, ::-1]), (nz, ny, nx))
                c[:nz, oy:oy + ny, ox:ox + nx] = small
            out.append(c)
        return out[0], out[1], out[2]


def dilate(m, r):
    m = m.copy()
    for axis in range(3):
        for _ in range(r):
            d = m.copy()
            s0 = [slice(None)] * 3
            s1 = [slice(None)] * 3
            s0[axis], s1[axis] = slice(1, None), slice(None, -1)
            d[tuple(s0)] |= m[tuple(s1)]
            d[tuple(s1)] |= m[tuple(s0)]
            m = d
    return m


def tight_box(m):
    """[mín, máx) por eixo x, y, z de uma máscara [z][y][x]."""
    if not m.any():
        return None
    ext = []
    for axis in (2, 1, 0):
        i = np.where(m.any(axis=tuple(j for j in range(3) if j != axis)))[0]
        ext.append((int(i.min()), int(i.max()) + 1))
    return ext


def main_embergen():
    src = EmberGenSource(CACHE)
    n = RES or 192
    domain_m = src.side * src.voxel
    os.makedirs(OUT, exist_ok=True)
    frames = pick_frames(src.count())
    loop = None
    if EG_LOOP:
        step = 2
        f0 = int(round(LOOP_START * FPS)) + 1
        n_out, n_x = int(round(LOOP_LEN * FPS / step)), int(round(LOOP_XFADE * FPS / step))
        frames = [f0 + k * step for k in range(n_out)]
        loop = (step, n_out, n_x)
        if f0 + (n_out + n_x) * step > src.count():
            sys.exit('sequência curta pro loop pedido')
    print(f'PYROCONV EmberGen: {src.count()} quadros, caixa {src.dims} voxels, cubo {src.side} → {n}³, '
          f'domínio {domain_m:.1f} m, {FPS:g} qps')
    times, light_pos, light_col, occ, bocc, tight = [], [], [], [], [], []
    for q, f in enumerate(frames):
        den, fl, heat = src.fields(f, n)
        if loop and q < loop[2]:
            w = q / loop[2]
            d2, f2, h2 = src.fields(f + loop[1] * loop[0], n)
            den, fl, heat = w * den + (1 - w) * d2, w * fl + (1 - w) * f2, w * heat + (1 - w) * h2
        if AS_DUST:
            z = np.zeros_like(den)
            write_frame(q, n, z, z.copy(), z.copy(), None, domain_m, light_pos, light_col, occ, bocc, tight,
                        dust_c=np.clip(den * DUST_K, 0.0, 2.5))
        else:
            soot_c = np.clip(den * SOOT_K, 0.0, 4.0)
            temp_c = np.clip(heat * EG_TEMP_HEAT + fl * EG_TEMP_FLAME, 0.0, 1.6)
            fuel_c = np.clip(fl, 0.0, 1.0)
            write_frame(q, n, soot_c, temp_c, fuel_c, None, domain_m, light_pos, light_col, occ, bocc, tight)
        times.append((f - frames[0]) / FPS if loop else (f - 1) / FPS)
        print(f'PYROCONV quadro {q} (sim {f}, {times[-1]:.2f} s): ocupação {100 * occ[-1]:.2f}%')
    finish(n, domain_m, frames, times, light_pos, light_col, occ, bocc, tight,
           {'embergen_voxel_m': src.voxel, 'as_dust': AS_DUST, 'dust_k': DUST_K if AS_DUST else None,
            'temp_heat': EG_TEMP_HEAT, 'temp_flame': EG_TEMP_FLAME,
            'loop': {'start_s': LOOP_START, 'length_s': LOOP_LEN, 'crossfade_s': LOOP_XFADE} if loop else None})


def write_frame(q, n, soot_c, temp_c, fuel_c, vel_c, domain_m, light_pos, light_col, occ, bocc, tight, dust_c=None):
    """Grava fNNN_a/b.bin (cubo [z][y][x] nos eixos da Unreal) e junta luz, ocupação e caixa."""
    # névoa rala some (a 1 m ela deixa passar >96%): a caixa marchada fica justa na
    # fumaça visível, como na gravação antiga (~6% do domínio)
    empty = (soot_c < 0.003) & (fuel_c < 0.05)
    if dust_c is not None:
        empty &= dust_c < 0.003
        dust_c[empty] = 0.0
    soot_c[empty] = 0.0
    band = dilate(~empty, VELOCITY_BAND)
    A = np.zeros((n, n, n, 4), dtype=np.uint8)
    A[..., 0] = np.round(np.sqrt(soot_c / 4.0) * 255).astype(np.uint8)
    A[..., 1] = np.round(temp_c / 1.6 * 255).astype(np.uint8)
    if dust_c is not None:
        A[..., 2] = np.round(np.sqrt(np.clip(dust_c, 0.0, 2.5) / 2.5) * 255).astype(np.uint8)
    A[empty] = 0
    B = np.zeros((n, n, n, 4), dtype='<f2')
    B[..., 0] = np.sqrt(fuel_c)
    if vel_c is not None:
        B[..., 1:] = vel_c
    B[~band] = 0
    B[empty, 0] = 0
    if not META_ONLY:
        A.tofile(os.path.join(OUT, 'f%03d_a.bin' % q))
        B.tofile(os.path.join(OUT, 'f%03d_b.bin' % q))
    SOOT_TOTALS.append(float(((A[..., 0].astype(np.float32) / 255) ** 2 * 4).sum()))
    # luz: centro da emissão (peso fogo · T⁴) relativo ao pé, eixos do WebGL (x, y pra cima, z)
    w = fuel_c * temp_c ** 4
    tot = float(w.sum())
    if tot > 1e-6:
        zz, yy, xx = np.indices(w.shape, sparse=True)
        cx = float((w * xx).sum() / tot) + 0.5
        cy = float((w * yy).sum() / tot) + 0.5
        cz = float((w * zz).sum() / tot) + 0.5
        vm = domain_m / n
        light_pos.append([(cx - n / 2) * vm, cz * vm, (cy - n / 2) * vm])
    else:
        light_pos.append([0.0, 2.5, 0.0])
    light_col.append(tot)
    occ.append(float((~empty).mean()))
    bocc.append(float(band.mean()))
    tight.append(tight_box(~empty))


def finish(n, domain_m, frames, times, light_pos, light_col, occ, bocc, tight, extra):
    # potência da luz: pico ~83 como na Carga Pesada, cor quente do WebGL
    peak = max(light_col) or 1.0   # poeira pura: tudo zero, luz zero
    light_col = [[83.0 * c / peak, 83.0 * c / peak * 0.587, 83.0 * c / peak * 0.277] for c in light_col]
    boxes = []
    for q in range(len(frames)):
        pair = [c for c in (tight[q], tight[min(q + 1, len(frames) - 1)]) if c]
        if not pair:
            boxes.append([[0, 0, 0], [1, 1, 1]])
            continue
        lo3 = [max(min(c[k][0] for c in pair) - BOX_MARGIN, 0) for k in range(3)]
        hi3 = [min(max(c[k][1] for c in pair) + BOX_MARGIN, n) for k in range(3)]
        boxes.append([lo3, [hi3[k] - lo3[k] for k in range(3)]])
    info = {
        'source': os.path.relpath(CACHE, os.path.join(PROJECT, '..')).replace(os.sep, '/'),
        'resolution': [n, n, n], 'domain_m': domain_m,
        'frames': len(frames), 'variant': 0, 'duration_s': times[-1], 'curve': 1.0,
        'frame_times_s': times, 'light_pos_webgl': light_pos, 'light_color': light_col,
        'occupancy': occ, 'velocity_occupancy': bocc, 'velocity_band_voxels': VELOCITY_BAND, 'boxes': boxes,
        'encoding': {'soot': 'A.r^2*4', 'temperature': 'A.g*1.6', 'dust': 'A.b^2*2.5',
                     'fuel': 'B.r^2', 'velocity': 'B.gba in m/s (half float)'},
        'conversion': dict({'soot_k': SOOT_K}, **extra),
    }
    # o EmberGen dissolve a fumaça depois do pico; ganho sugerido (teto 6×) pra manter a massa de fuligem
    # (só quando pedido: PYRO_SOOT_GAIN=1, como no eg_gasoline_b; loops e poeira não levam)
    if os.environ.get('PYRO_SOOT_GAIN') == '1' and SOOT_TOTALS and max(SOOT_TOTALS) > 0:
        peak = max(SOOT_TOTALS)
        ip = SOOT_TOTALS.index(peak)
        info['soot_total'] = SOOT_TOTALS
        info['soot_gain_suggested'] = [1.0 if q <= ip else float(min(6.0, peak / max(SOOT_TOTALS[q], 1e-6)))
                                       for q in range(len(SOOT_TOTALS))]
    with open(os.path.join(OUT, 'meta.json'), 'w', encoding='utf-8') as fh:
        json.dump(info, fh)
    print(f'PYROCONV pronto: {len(frames)} quadros, {n}³, domínio {domain_m:.1f} m -> {OUT}')


def main():
    if EMBERGEN:
        return main_embergen()
    files = sorted(glob.glob(os.path.join(CACHE, 'data', 'fluid_data_*.vdb')))
    if not files:
        sys.exit('sem cache em ' + CACHE)
    first = read(files[0], ['shadow'])['shadow']
    lo, hi = first.evalActiveVoxelBoundingBox()
    shape = (hi[0] + 1, hi[1] + 1, hi[2] + 1)                 # x, y, z do Mantaflow
    voxel = first.transform.voxelSize()[0]
    n = RES or shape[2]
    if n != shape[2]:
        sys.exit(f'resolução {n} ≠ altura do cache {shape[2]}: rode o Mantaflow com resolution_max = {n}')
    domain_m = voxel * n
    ox, oy = (n - shape[0]) // 2, (n - shape[1]) // 2          # centra o chão do Mantaflow no cubo
    os.makedirs(OUT, exist_ok=True)
    frames = pick_frames(len(files))
    times, light_pos, light_col, occ, bocc, tight = [], [], [], [], [], []
    loop = None
    if LOOP:
        step = 2
        f0 = int(round(LOOP_START * FPS)) + 1
        n_out = int(round(LOOP_LEN * FPS / step))
        n_x = int(round(LOOP_XFADE * FPS / step))
        frames = [f0 + k * step for k in range(n_out)]
        loop = (step, n_out, n_x)
        if frames[-1] + n_out * 0 + n_x * step > len(files) or f0 + (n_out + n_x) * step > len(files):
            sys.exit('cache curto pro loop pedido')

    def cube(a, ch=None):
        """cubo [z][y][x] da Unreal: y da Unreal = -y do Blender."""
        c = np.zeros((n, n, n) + (() if ch is None else (ch,)), dtype=np.float32)
        c[:, oy:oy + shape[1], ox:ox + shape[0]] = np.transpose(a, (2, 1, 0) + ((3,) if ch else ()))[:, ::-1]
        return c

    def load(f):
        g = read(files[f - 1], ['density', 'flame', 'temperature', 'velocity'])
        den = dense(g['density'], shape)
        fl = dense(g['flame'], shape)
        heat = dense(g['temperature'], shape)
        # o VDB do Mantaflow já sai em m/s (medido: ~24 m/s no estouro com 14 + 10 m/s de
        # velocidade inicial; em células por quadro daria centenas de m/s)
        vel = cube(dense(g['velocity'], shape, vec=True), 3)
        vel[..., 1] *= -1.0
        return (cube(np.clip(den * SOOT_K, 0.0, 4.0)), cube(np.clip(heat * TEMP_HEAT + fl * TEMP_FLAME, 0.0, 1.6)),
                cube(np.clip(fl, 0.0, 1.0)), vel)

    for q, f in enumerate(frames):
        soot_c, temp_c, fuel_c, vel_c = load(f)
        if loop and q < loop[2]:
            # emenda: no começo, mistura com o trecho logo depois do fim da janela
            w = q / loop[2]
            s2, t2, u2, v2 = load(f + loop[1] * loop[0])
            soot_c = w * soot_c + (1 - w) * s2
            temp_c = w * temp_c + (1 - w) * t2
            fuel_c = w * fuel_c + (1 - w) * u2
            vel_c = w * vel_c + (1 - w) * v2
        write_frame(q, n, soot_c, temp_c, fuel_c, vel_c, domain_m, light_pos, light_col, occ, bocc, tight)
        times.append((f - frames[0]) / FPS if loop else (f - 1) / FPS)
        print(f'PYROCONV quadro {q} (sim {f}, {times[-1]:.2f} s): ocupação {100 * occ[-1]:.2f}%')
    finish(n, domain_m, frames, times, light_pos, light_col, occ, bocc, tight,
           {'temp_heat': TEMP_HEAT, 'temp_flame': TEMP_FLAME, 'voxel_m': voxel,
            'loop': {'start_s': LOOP_START, 'length_s': LOOP_LEN, 'crossfade_s': LOOP_XFADE} if loop else None})


main()
