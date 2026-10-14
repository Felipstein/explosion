"""O custo das mortes de tanque no jogo, do perfil CSV da Unreal (game.ps1 view ... -Extra '-csvGpuStats -csvCaptureFrames=N').

  python BlastLab/Tools/game_perf.py [arquivo.csv] [--top 25]

Acha os quadros com o evento TankDeath (BlastFxDirector.cpp), compara a janela das mortes (do 1º evento até
WINDOW quadros depois) com a base (os BASE quadros antes da 1ª morte) e lista as colunas que mais subiram:
o tempo de quadro, o da thread do jogo, o da placa e cada passe da placa (GPU/...), e os marcadores da
explosão (BlastFxDebris/Tick, BlastFxRaymarch/Tick, GPU/BlastRaymarch).
"""
import csv
import glob
import os
import sys

import numpy as np

csv.field_size_limit(10 ** 9)
WINDOW = 150          # ~4–5 s depois da 1ª morte: a bola, a poeira e os destroços
BASE = 300            # a luta antes das mortes
KEY = ['FrameTime', 'GameThreadTime', 'RenderThreadTime', 'GPUTime', 'RHIThreadTime',
       'GPU/BlastRaymarch', 'BlastFxRaymarch/GameThread/Tick', 'BlastFxDebris/GameThread/Tick',
       'BlastFxDirector/GameThread/TankDeath', 'Ticks/BlastDebris']


def latest():
    here = os.path.dirname(os.path.abspath(__file__))
    # o jogo aberto pelo editor (-game) grava no Saved do usuário, não no do projeto
    files = glob.glob(os.path.join(here, '..', '..', 'Game', 'Saved', 'Profiling', 'CSV', '*.csv'))
    files += glob.glob(os.path.join(os.environ.get('LOCALAPPDATA', ''), 'UnrealEngine', '5.8', 'Saved', 'Profiling', 'CSV', '*.csv'))
    return max(files, key=os.path.getmtime)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    top = int(sys.argv[sys.argv.index('--top') + 1]) if '--top' in sys.argv else 25
    path = next((a for a in args if a.endswith('.csv')), None) or latest()
    with open(path, newline='', encoding='utf-8', errors='replace') as f:
        rows = list(csv.reader(f))
    # as colunas crescem quando um marcador aparece pela 1ª vez: o cabeçalho completo é o do fim
    # ([HasHeaderRowAtEnd]); cada linha é um prefixo dele. Depois do cabeçalho vêm os metadados
    hk = max(k for k, r in enumerate(rows) if 'FrameTime' in r)
    head = rows[hk]
    data = [r + [''] * (len(head) - len(r)) for r in rows[1:hk] if not (r and r[0].startswith('['))]
    col = {h: i for i, h in enumerate(head)}
    ev = col.get('EVENTS', col.get('events'))
    deaths = [k for k, r in enumerate(data) if ev is not None and 'TankDeath' in r[ev]]
    print(f'{os.path.basename(path)}: {len(data)} quadros, {len(deaths)} mortes de tanque', end='')
    if not deaths:
        print(' (sem o evento: o perfil não pegou as mortes)')
        return
    d0 = deaths[0]
    print(f' (quadros {deaths[0]}–{deaths[-1]})')

    def series(h):
        out = []
        for r in data:
            try:
                out.append(float(r[col[h]]) if r[col[h]] != '' else 0.0)
            except ValueError:
                out.append(0.0)
        return np.array(out)

    base = slice(max(0, d0 - BASE), d0)
    win = slice(d0, min(len(data), d0 + WINDOW))
    print(f'\n{"coluna":38s} {"base":>8s} {"mortes":>8s} {"p95":>8s} {"pico":>8s}  (ms)')
    for h in KEY:
        if h in col:
            s = series(h)
            print(f'{h:38s} {s[base].mean():8.2f} {s[win].mean():8.2f} {np.percentile(s[win], 95):8.2f} {s[win].max():8.2f}')
    # o pico quadro a quadro, nos primeiros 20 quadros depois da 1ª morte
    ft = series('FrameTime')
    gt = series('GameThreadTime') if 'GameThreadTime' in col else ft * 0
    gpu = series('GPUTime') if 'GPUTime' in col else ft * 0
    print('\nquadro a quadro desde a 1ª morte (quadro, jogo, placa):')
    print('  ' + '  '.join(f'{ft[k]:.0f}/{gt[k]:.0f}/{gpu[k]:.0f}' for k in range(d0, min(len(data), d0 + 20))))
    # o que mais subiu
    deltas = []
    for h in head:
        if h in ('events', 'EVENTS') or h in KEY:
            continue
        s = series(h)
        dm = s[win].mean() - s[base].mean()
        if abs(dm) > 0.05:
            deltas.append((dm, h, s[base].mean(), s[win].mean(), s[win].max()))
    deltas.sort(reverse=True)
    print(f'\nas {top} colunas que mais subiram na janela das mortes:')
    for dm, h, b, w, mx in deltas[:top]:
        print(f'  {h:60s} +{dm:6.2f}  ({b:.2f} → {w:.2f}, pico {mx:.2f})')


if __name__ == '__main__':
    main()
