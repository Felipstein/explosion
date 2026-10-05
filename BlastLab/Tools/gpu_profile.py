"""Tempos de GPU por passe, do último CSV do profiler (blast.ps1 render ... -Profile).

  python BlastLab/Tools/gpu_profile.py [n_passes]
Médias por trecho da explosão. A captura começa no boot: 4 quadros de carga
+ 60 de aquecimento do Movie Render Queue antes do quadro 0 da sequência.
"""
import csv
import glob
import os
import sys

import numpy as np

csv.field_size_limit(10 ** 9)
START = 64
SPANS = {'0.5-1.5s': (0.5, 1.5), '1.5-3s': (1.5, 3.0), '3-5s': (3.0, 5.0), '6-8s': (6.0, 8.0)}


def is_number(x):
    try:
        float(x)
        return True
    except ValueError:
        return x == ''


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 16
    f = sorted(glob.glob(os.path.expandvars(r'%LOCALAPPDATA%\UnrealEngine\5.8\Saved\Profiling\CSV\*.csv')),
               key=os.path.getmtime)[-1]
    rows = list(csv.reader(open(f, encoding='utf-8', errors='ignore')))
    head = rows[0]
    data = np.array([[float(x) if x else np.nan for x in r] for r in rows[1:]
                     if len(r) == len(head) and all(is_number(x) for x in r)])
    print(os.path.basename(f), len(data), 'frames')
    spans = {k: slice(START + int(a * 30), START + int(b * 30)) for k, (a, b) in SPANS.items()}
    gpu = [i for i, c in enumerate(head) if c.startswith('GPU/')]
    order = sorted(gpu, key=lambda i: -np.nanmean(data[START:START + 240, i]))
    print('%-40s ' % 'ms' + ''.join('%10s' % k for k in spans))
    for i in order[:n]:
        print('%-40s ' % head[i] + ''.join('%10.2f' % np.nanmean(data[s, i]) for s in spans.values()))
    for name in ('FrameTime', 'GPUTime', 'RenderThreadTime', 'GameThreadTime'):
        if name in head:
            i = head.index(name)
            print('%-40s ' % name + ''.join('%10.2f' % np.nanmean(data[s, i]) for s in spans.values()))


if __name__ == '__main__':
    main()
