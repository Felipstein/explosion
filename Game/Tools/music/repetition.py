# (06/11/2026, a trilha da partida) Quanto a faixa se repete (ele, 06/11: "muito repetidas, péssimas, me enjoa de ouvir"): a faixa em
# janelas de 4 s (16 quadros de 0,25 s: o croma das 12 notas e o timbre em 24 bandas), e cada janela comparada com todas as que tocaram
# ao menos 4 s antes. "repete" = a parte da faixa que é quase cópia de um trecho anterior (cosseno ≥ 0,95); "cópia" = cópia exata (≥ 0,985,
# o loop colado); "maior volta" = o maior trecho seguido que é cópia. Música bem feita repete o tema, mas variado (outra orquestração,
# outra altura): a cópia exata é o que cansa.
#   python Game/Tools/music/repetition.py <arquivos ou pastas...>
import os
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import measure  # noqa: E402

SR = 22050
HOP = 512
FR = 0.25   # s por quadro


def features(path):
    raw = subprocess.run([measure.FF, '-v', 'quiet', '-i', path, '-ac', '1', '-ar', str(SR), '-f', 'f32le', '-'], capture_output=True).stdout
    x = np.frombuffer(raw, dtype=np.float32)
    mag = measure.stft_mag(x, n=4096, hop=HOP)
    f = np.fft.rfftfreq(4096, 1 / SR)
    # o croma: cada faixa de frequência (55 Hz a 5 kHz) na nota dela
    ok = (f > 55) & (f < 5000)
    pc = np.round(12 * np.log2(f[ok] / 440.0)).astype(int) % 12
    chroma = np.zeros((mag.shape[0], 12))
    for k in range(12):
        chroma[:, k] = mag[:, ok][:, pc == k].sum(axis=1)
    # o timbre: 24 bandas em escala logarítmica
    edges = np.geomspace(40, 11000, 25)
    bands = np.stack([mag[:, (f >= edges[i]) & (f < edges[i + 1])].sum(axis=1) for i in range(24)], axis=1)
    feat = np.concatenate([np.log1p(chroma), 0.5 * np.log1p(bands)], axis=1)
    # quadros de 0,25 s
    per = int(round(FR * SR / HOP))
    n = feat.shape[0] // per
    feat = feat[:n * per].reshape(n, per, -1).mean(axis=1)
    return feat, len(x) / SR


def repetition(path, win=16):
    feat, dur = features(path)
    n = feat.shape[0] - win
    if n < win * 2:
        return dict(dur=dur, repeat=0, copy=0, loop=0)
    W = np.stack([feat[i:i + win].ravel() for i in range(n)])
    W = W - W.mean(axis=1, keepdims=True)
    W /= np.linalg.norm(W, axis=1, keepdims=True) + 1e-9
    S = W @ W.T
    best = np.array([S[i, :max(0, i - win)].max() if i - win > 0 else 0.0 for i in range(n)])
    rep = best >= 0.95
    cop = best >= 0.985
    # o maior trecho seguido de cópia, em segundos
    run = mx = 0
    for v in cop:
        run = run + 1 if v else 0
        mx = max(mx, run)
    return dict(dur=dur, repeat=float(rep.mean()), copy=float(cop.mean()), loop=(mx + win) * FR if mx else 0.0)


def main():
    files = []
    for a in sys.argv[1:]:
        if os.path.isdir(a):
            files += [os.path.join(a, f) for f in sorted(os.listdir(a)) if f.lower().endswith(('.wav', '.mp3'))]
        else:
            files.append(a)
    for p in files:
        r = repetition(p)
        print(f"{os.path.basename(p)[:44]:44} {r['dur']:6.0f} s  repete {100 * r['repeat']:5.1f}%  cópia {100 * r['copy']:5.1f}%  maior volta {r['loop']:5.1f} s", flush=True)


if __name__ == '__main__':
    main()
