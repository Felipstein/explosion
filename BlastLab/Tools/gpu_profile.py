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
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    live = '--live' in sys.argv          # captura do simulador (blast.ps1 view -Folder x -ProfileGpu): começa na detonação
    n = int(args[0]) if args else 16
    found = []
    for pattern in (r'%LOCALAPPDATA%\UnrealEngine\5.8\Saved\Profiling\CSV\*.csv', r'BlastLab\Saved\Profiling\CSV\*.csv'):
        found += glob.glob(os.path.expandvars(pattern))
    f = sorted(found, key=os.path.getmtime)[-1]
    rows = list(csv.reader(open(f, encoding='utf-8', errors='ignore')))
    # A Unreal grava o cabeçalho completo no fim ([HasHeaderRowAtEnd]): o do
    # começo não tem as estatísticas que aparecem no meio da captura (a
    # deformação das explosões, por exemplo), e as linhas crescem junto
    if rows[-1] and rows[-1][0].startswith('[HasHeaderRowAtEnd]'):
        head, body = rows[-2], rows[1:-2]
    else:
        head, body = rows[0], rows[1:]
    data = np.array([[float(x) if x and is_number(x) else np.nan for x in r[:len(head)]] + [np.nan] * (len(head) - len(r))
                     for r in body if len(r) <= len(head)])
    print(os.path.basename(f), len(data), 'frames')
    if live:
        # quadros com tempo variável: os trechos vêm do tempo acumulado desde a detonação
        t = np.cumsum(data[:, head.index('FrameTime')]) / 1000.0
        spans = {k: (t >= a) & (t < b) for k, (a, b) in SPANS.items()}
        spans = {k: v for k, v in spans.items() if v.any()}
    else:
        spans = {k: slice(START + int(a * 30), START + int(b * 30)) for k, (a, b) in SPANS.items()}
    gpu = [i for i, c in enumerate(head) if c.startswith('GPU/')]
    order = sorted(gpu, key=lambda i: -np.nanmean(data[:, i] if live else data[START:START + 240, i]))
    print('%-40s ' % 'ms' + ''.join('%10s' % k for k in spans))
    for i in order[:n]:
        print('%-40s ' % head[i] + ''.join('%10.2f' % np.nanmean(data[s, i]) for s in spans.values()))
    for name in ('FrameTime', 'GPUTime', 'RenderThreadTime', 'GameThreadTime'):
        if name in head:
            i = head.index(name)
            print('%-40s ' % name + ''.join('%10.2f' % np.nanmean(data[s, i]) for s in spans.values()))


if __name__ == '__main__':
    main()
