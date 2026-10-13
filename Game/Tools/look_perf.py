"""O tempo de GPU da luz na câmera do jogo, do CSV do profiler da Unreal (look.ps1 perf).

  python Game/Tools/look_perf.py <arquivo.csv>
Pula os quadros de carga e aquecimento (os 90 primeiros) e dá a mediana e o p95 do quadro de
GPU e dos passes que mais custam (o orçamento do Felipe: 60 qps = 16,7 ms no preset escolhido).
"""
import csv
import sys

import numpy as np

csv.field_size_limit(10 ** 9)
rows = list(csv.reader(open(sys.argv[1], encoding='utf-8', errors='ignore')))
if rows[-1] and rows[-1][0].startswith('[HasHeaderRowAtEnd]'):
    head, body = rows[-2], rows[1:-2]
else:
    head, body = rows[0], rows[1:]
body = [r for r in body if len(r) == len(head)]
# só os quadros de verdade: depois da carga e do aquecimento do Movie Render Queue (GPU > 5 ms e sem os picos de compilação)
_gi = head.index('GPUTime')
_v = [float(r[_gi]) if r[_gi] else 0.0 for r in body]
_start = max(200, next((i for i in range(len(_v)) if i > 40 and 5.0 < _v[i] < 200.0), 0))   # o jogo assenta (Lumen, shaders)
body = [r for r, g in zip(body[_start:], _v[_start:]) if 5.0 < g < 200.0]


def col(name):
    i = head.index(name)
    v = []
    for r in body:
        try:
            v.append(float(r[i]))
        except ValueError:
            pass
    return np.array(v)


total = None
for name in ('GPUTime', 'GPU/Total', 'FrameTime'):
    if name in head:
        total = (name, col(name))
        break
if total:
    print(f'quadro de GPU ({total[0]}): mediana {np.median(total[1]):.2f} ms, p95 {np.percentile(total[1], 95):.2f} ms '
          f'({len(total[1])} quadros)')
gpu = [h for h in head if h.startswith('GPU/') and h != 'GPU/Total']
means = sorted(((np.median(col(h)), h) for h in gpu), reverse=True)[:12]
for m, h in means:
    print(f'  {h[4:]:40s} {m:6.2f} ms')
