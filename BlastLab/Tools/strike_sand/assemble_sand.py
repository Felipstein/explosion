"""A gravação strike_chao só de areia (23/10 noite): o contrato do jogo de hoje (BlastFxDirector MissileHitLife: a strike_chao toca à
parte pelo SpawnDust, em todo míssil do ataque, na escala 1, sem luz nem clarão, com a bola clássica ou a ogiva em cima). Sem a bola,
sem calor, sem combustível, sem luz gravada: a areia (os grãos + o gás do Mantaflow, sandfield.Field) no canal da poeira e a terra
escura (o jato central e o miolo dos jatos) no da fuligem.

  python assemble_sand.py <saida.brm2> <pasta_graos> <pasta_mf_npz> [chave=valor ...]   (as chaves: sandfield.DEFAULT)
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, r'C:\Users\luisf\explosion\BlastLab\Tools')
import brm2  # noqa: E402
from brm2_downsample import macro  # noqa: E402
import sandfield  # noqa: E402

DST, GDIR, MFDIR = sys.argv[1:4]
kw = {}
for a in sys.argv[4:]:
    k, v = a.split('=', 1)
    kw[k] = float(v)
F = sandfield.Field(GDIR, MFDIR, os.path.join(HERE, '..', 'cache', os.path.basename(os.path.normpath(GDIR))), **kw)
n, D, vm = F.n, F.D, F.vm
times = F.times
NQ = len(times)
bounds, stats = [], []


def gen():
    for q in range(NQ):
        soot, dust, vel, t = F.frame(q)
        A, Fh = F.encode(soot, dust, vel)
        occ = (A[..., 0] > 0) | (A[..., 2] > 0)
        if occ.any():
            e = []
            for axis in (2, 1, 0):
                i = np.where(occ.any(axis=tuple(k for k in range(3) if k != axis)))[0]
                e.append((int(i.min()), int(i.max()) + 1))
            bounds.append([round(v, 4) for v in [-D / 2 + e[0][0] * vm, -D / 2 + e[1][0] * vm, e[2][0] * vm,
                                                  -D / 2 + e[0][1] * vm, -D / 2 + e[1][1] * vm, e[2][1] * vm]])
        else:
            bounds.append([0.0] * 6)
        stats.append((q, t, float(soot.sum() * vm ** 3), float(dust.sum() * vm ** 3)))
        if q % 8 == 0 or q == NQ - 1:
            print('AREIA q%02d t %.2f s: escura Σ %.1f, areia Σ %.1f' % stats[-1], flush=True)
        yield A, Fh, macro(A, 4)


head = {'source': 'strike_chao so de areia: graos %s + Mantaflow %s (assemble_sand.py, 23/10 noite)' % (os.path.basename(os.path.normpath(GDIR)),
                                                                                                    os.path.basename(os.path.normpath(MFDIR))),
        'res': n, 'fuelRes': n // 2, 'macroRes': n // 4, 'frames': NQ, 'duration': float(times[-1]),
        'curve': 1.7, 'domain': D, 'velMax': sandfield.VEL_MAX, 'bounds': bounds,
        'lightPos': [[0.0, 0.0, 0.0]] * NQ, 'lightCol': [[0.0, 0.0, 0.0]] * NQ, 'lightPhysical': True, 'loop': False,
        'layers': {'grains': os.path.basename(os.path.normpath(GDIR)), 'mf': os.path.basename(os.path.normpath(MFDIR)), 'params': F.P}}
# a curva dos instantes dos grãos (ejecta: duracao·(q/(n−1))^curva): conferida
assert np.allclose(times, head['duration'] * (np.arange(NQ) / (NQ - 1)) ** 1.7, atol=1e-3), 'os instantes dos grãos não são os da curva 1,7'
counts = brm2.write(DST, head, gen())
print('AREIA pronto: %s, %d quadros, blocos por quadro %d–%d (soma %d), %.0f MB' % (DST, len(counts), min(counts), max(counts), sum(counts),
                                                                                  os.path.getsize(DST) / 2 ** 20))
