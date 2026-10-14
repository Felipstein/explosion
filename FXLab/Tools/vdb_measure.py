"""Mede uma simulação do EmberGen (VDB) em metros, só na CPU, antes de cozinhar: a altura, a largura e o
comprimento da nuvem e a silhueta de lado e de frente, com a mesma opacidade do cozimento 6-way
(bake_sixway.py: densidade × 6 por metro, alfa = 1 − e^(−6·Σρ·dx)).

Uso (o openvdb vem com o Blender):
    blender -b --factory-startup -P vdb_measure.py -- <pasta_vdb> <saida.png> <quadros: 3,6,12,30,...> [qps=30] [limiar=0.3]
Escreve as medidas no console (VDBM ...) e uma folha com as silhuetas (lado: X × Z; frente: Y × Z), 1 px = 1 voxel.
"""
import math
import os
import sys

import numpy as np
import openvdb

argv = sys.argv[sys.argv.index('--') + 1:]
folder, out, frames = argv[0], argv[1], [int(x) for x in argv[2].split(',')]
fps = float(argv[3]) if len(argv) > 3 else 30.0
thr = float(argv[4]) if len(argv) > 4 else 0.3
files = sorted([f for f in os.listdir(folder) if f.endswith('.vdb')], key=lambda f: int(''.join(c for c in f if c.isdigit()) or 0))

panels = []
for k in frames:
    if k >= len(files):
        continue
    grids, _ = openvdb.readAll(os.path.join(folder, files[k]))
    g = next(x for x in grids if x.name == 'density')
    dx = g.transform.voxelSize()[0]
    (x0, y0, z0), (x1, y1, z1) = g.evalActiveVoxelBoundingBox()
    a = np.zeros((x1 - x0 + 1, y1 - y0 + 1, z1 - z0 + 1), np.float32)
    g.copyToArray(a, ijk=(x0, y0, z0))
    side = 1.0 - np.exp(-6.0 * a.sum(axis=1) * dx)     # X × Z
    front = 1.0 - np.exp(-6.0 * a.sum(axis=0) * dx)    # Y × Z

    def ext(img, axis):
        m = (img > thr).any(axis=axis)
        idx = np.nonzero(m)[0]
        return (idx.min(), idx.max()) if len(idx) else (0, -1)
    xs, zs = ext(side, 1), ext(side, 0)
    ys = ext(front, 1)
    w_x = (xs[1] - xs[0] + 1) * dx
    w_y = (ys[1] - ys[0] + 1) * dx
    top = (zs[1] + z0 + 1) * dx
    print('VDBM quadro %d t=%.2fs: comprimento X %.2f m, largura Y %.2f m, topo %.2f m, alfa máx %.2f' % (
        k, k / fps, w_x, w_y, top, side.max()))
    panels.append((k, side, front, x0, y0, z0))

# as silhuetas vão num .npz (o Blender não tem PIL); a folha sai com tread_vdb_sheet.py no Python do sistema
np.savez_compressed(out, **{'side_%d' % k: side for k, side, front, x0, y0, z0 in panels},
                    **{'front_%d' % k: front for k, side, front, x0, y0, z0 in panels},
                    **{'o_%d' % k: np.array([x0, y0, z0, fps]) for k, side, front, x0, y0, z0 in panels})
print('VDBM silhuetas', out)
