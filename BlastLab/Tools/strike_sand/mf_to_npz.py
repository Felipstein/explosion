"""A areia em gás do Mantaflow (só areia: a simulação sem o fogo e sem o jato escuro) nos instantes da gravação final (23/10,
strike_chao v2): a densidade na grade do cubo final e a velocidade na meia resolução, em npz, pro assemble_strike.py.

  blender -b --factory-startup -P mf_to_npz.py -- <cache/data> <saida_dir> <dominio_m> <res> <quadros> <duracao_s> <curva> [qps=30]

O cubo: x, y centrados no centro do domínio do Mantaflow (o impacto), z do chão; [z][y][x] nos eixos da simulação (a montagem
vira o y pro da Unreal). A redução é a média da área (a mesma do build_raymarch_vdb.py: sem jogar fora o detalhe fino). Cada
instante t lê o quadro de cache mais perto (t·qps; o erro é de ≤ 1/60 s).
"""
import glob
import os
import re
import sys

import numpy as np
import openvdb as vdb

argv = sys.argv[sys.argv.index('--') + 1:]
SRC, OUT = argv[0], argv[1]
DOM, RES, NQ, DUR, CURVE = float(argv[2]), int(argv[3]), int(argv[4]), float(argv[5]), float(argv[6])
FPS = float(argv[7]) if len(argv) > 7 else 30.0
os.makedirs(OUT, exist_ok=True)
files = sorted(glob.glob(os.path.join(SRC, '*.vdb')), key=lambda f: int(re.findall(r'(\d+)\.vdb$', f)[0]))
g0 = {g.name: g for g in vdb.readAll(files[0])[0]}
lo, hi = g0['shadow'].evalActiveVoxelBoundingBox()
dims = [hi[i] - lo[i] + 1 for i in range(3)]
vs = g0['shadow'].transform.voxelSize()[0]
print(f'MF2NPZ {len(files)} quadros, domínio {dims} voxels de {vs:.4f} m ({dims[0] * vs:.2f} × {dims[1] * vs:.2f} × {dims[2] * vs:.2f} m)')


def box_matrix(n_src, v_src, o_src, n_dst, v_dst, o_dst):
    """W[i, j] = a fração da célula de destino i coberta pela célula de origem j (o começo de cada grade em o_*, em m)."""
    W = np.zeros((n_dst, n_src), np.float32)
    for i in range(n_dst):
        a, b = o_dst + i * v_dst, o_dst + (i + 1) * v_dst
        j0 = max(int(np.floor((a - o_src) / v_src)), 0)
        j1 = min(int(np.ceil((b - o_src) / v_src)), n_src)
        for j in range(j0, j1):
            s0, s1 = o_src + j * v_src, o_src + (j + 1) * v_src
            W[i, j] = max(0.0, min(b, s1) - max(a, s0)) / v_dst
    return W


def resample(a, Wz, Wy, Wx):
    """a [x][y][z] do VDB → [z][y][x] pelas matrizes de área."""
    a = np.tensordot(a, Wx, axes=([0], [1]))            # [y][z][X]
    a = np.tensordot(a, Wy, axes=([0], [1]))            # [z][X][Y]
    a = np.tensordot(a, Wz, axes=([0], [1]))            # [X][Y][Z]
    return np.ascontiguousarray(np.transpose(a, (2, 1, 0)))


def mats(n):
    v = DOM / n
    # o domínio do Mantaflow: o centro em x, y (o impacto) e o chão no índice lo[2]
    Wx = box_matrix(dims[0], vs, -dims[0] * vs / 2, n, v, -DOM / 2)
    Wy = box_matrix(dims[1], vs, -dims[1] * vs / 2, n, v, -DOM / 2)
    Wz = box_matrix(dims[2], vs, 0.0, n, v, 0.0)
    return Wz, Wy, Wx


A_M = mats(RES)
F_M = mats(RES // 2)
times = [DUR * (q / (NQ - 1)) ** CURVE for q in range(NQ)]
for q, t in enumerate(times):
    i = min(len(files) - 1, int(round(t * FPS)))
    gs = {g.name: g for g in vdb.readAll(files[i])[0]}
    den = np.zeros(dims, np.float32)
    if gs.get('density') is not None and gs['density'].activeVoxelCount():
        gs['density'].copyToArray(den, ijk=tuple(lo))
    vel = np.zeros(dims + [3], np.float32)
    if gs.get('velocity') is not None and gs['velocity'].activeVoxelCount():
        gs['velocity'].copyToArray(vel, ijk=tuple(lo))
    d = resample(den, *A_M)
    # a velocidade pesada pela densidade (no ar vazio o Mantaflow tem velocidade, mas lá não há o que mover)
    dh = resample(den, *F_M)
    vh = np.stack([resample(vel[..., k] * den, *F_M) for k in range(3)], -1) / np.maximum(dh, 1e-6)[..., None]
    np.savez_compressed(os.path.join(OUT, f'mf_{q:02d}.npz'), den=d.astype(np.float16), vel=vh.astype(np.float16),
                        t=np.float32(t), src=os.path.basename(files[i]))
    if q % 8 == 0 or q == NQ - 1:
        print(f'MF2NPZ q{q:02d} t {t:.2f} s ← {os.path.basename(files[i])}: Σ {float(d.sum()):.0f}, máx {float(d.max()):.3f}, '
              f'p99 {float(np.percentile(d[d > 1e-3], 99)) if (d > 1e-3).any() else 0:.3f}', flush=True)
print('MF2NPZ pronto', OUT)
