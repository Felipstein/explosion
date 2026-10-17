"""Centra a caixa de uma cache OpenVDB num ponto (16/10 noite): o build_raymarch_vdb.py põe o pé da gravação no meio
da caixa ativa (em XY) e no chão. Uma gravação direcional (o disparo: a boca numa ponta) sairia com o pé longe do ponto
de onde o fenômeno nasce. Aqui dois voxels quase vazios nas pontas espelhadas fazem a caixa ficar simétrica em volta
do ponto: o pé da gravação cai nele.

  blender -b --factory-startup -P BlastLab/Tools/vdb_anchor.py -- <pasta_vdb> <x_m> <y_m>
(muda os .vdb no lugar; o ponto em metros a partir do canto de baixo do domínio: o Mantaflow grava o índice 0 no canto,
sem translação na transformação. No disparo, a boca fica em x = 'atras', y = 'largura'/2)
"""
import glob
import os
import sys

import numpy as np
import openvdb as vdb

argv = sys.argv[sys.argv.index('--') + 1:]
src, x0, y0 = argv[0], float(argv[1]), float(argv[2])
files = sorted(glob.glob(os.path.join(src, '*.vdb')))
lo, hi = [10 ** 9] * 3, [-10 ** 9] * 3
xf = None
for f in files:
    g = next((g for g in vdb.readAll(f)[0] if g.name == 'density'), None)
    if g is not None and g.activeVoxelCount():
        xf = g.transform
        a, b = g.evalActiveVoxelBoundingBox()
        lo = [min(lo[i], a[i]) for i in range(3)]
        hi = [max(hi[i], b[i]) for i in range(3)]
vs = xf.voxelSize()[0]
c = (int(round(x0 / vs)), int(round(y0 / vs)), 0)
ex = max(c[0] - lo[0], hi[0] - c[0])
ey = max(c[1] - lo[1], hi[1] - c[1])
corners = [(c[0] - ex, c[1] - ey, lo[2]), (c[0] + ex, c[1] + ey, lo[2])]
for f in files:
    grids, meta = vdb.readAll(f)
    for g in grids:
        if g.name == 'density':
            acc = g.getAccessor()
            for ijk in corners:
                if acc.getValue(ijk) < 1e-4:
                    acc.setValueOn(ijk, 1e-4)
    vdb.write(f, grids=grids, metadata=meta)
print(f'ANCORA {len(files)} quadros: centro {c}, caixa ±{ex}, ±{ey} voxels')
