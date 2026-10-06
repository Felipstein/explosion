"""Tempos de GPU por passe do último CSV do profiler (fatias / mapa inteiro).

  powershell -File WorldForge/Tools/forge.ps1 view -Map Slice_Forest -Folder p -Extra '-ForgeProfile -csvGpuStats -ForgeNoMotion -ForgeViews="mid:10,-15,20,50,85"'
  python WorldForge/Tools/gpu_profile.py [n_passes]
Média dos quadros da medição (4 s parado na vista média). Mesmo formato do
BlastLab/Tools/gpu_profile.py.
"""
import csv
import glob
import os
import sys

import numpy as np

csv.field_size_limit(10 ** 9)
TOOLS = os.path.dirname(os.path.abspath(__file__))


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 18
    found = glob.glob(os.path.join(TOOLS, '..', 'Saved', 'Profiling', 'CSV', '*.csv'))
    f = sorted(found, key=os.path.getmtime)[-1]
    rows = list(csv.reader(open(f, encoding='utf-8', errors='ignore')))
    head = rows[0]

    def num(x):
        try:
            return float(x) if x else np.nan
        except ValueError:
            return None
    data = []
    for r in rows[1:]:
        if len(r) != len(head):
            continue
        v = [num(x) for x in r]
        if all(x is not None for x in v):
            data.append(v)
    data = np.array(data)
    print(os.path.basename(f), len(data), 'quadros')
    gpu = [i for i, c in enumerate(head) if c.startswith('GPU/')]
    for i in sorted(gpu, key=lambda i: -np.nanmean(data[:, i]))[:n]:
        print('%-44s %7.2f ms' % (head[i], np.nanmean(data[:, i])))
    for name in ('FrameTime', 'GPUTime', 'RenderThreadTime', 'GameThreadTime'):
        if name in head:
            print('%-44s %7.2f ms' % (name, np.nanmean(data[:, head.index(name)])))


if __name__ == '__main__':
    main()
