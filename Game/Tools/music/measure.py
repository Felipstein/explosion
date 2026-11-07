# (06/11, a trilha da partida) Mede cada faixa: o volume percebido (EBU R128 pelo ffmpeg: integrado, faixa de volume, pico real),
# a curva de volume de 1 em 1 s (o short-term), o andamento e a grade das batidas (fluxo espectral + autocorrelação), o começo e o fim
# (silêncio, corte seco ou fim natural), a banda (o corte do mp3) e o clipe. Saída: um JSON por faixa e a tabela.
#   python Game/Tools/music/measure.py <pasta com facção/arquivo> <saída.json>   (o music_pack.py importa as funções daqui)
import json
import os
import re
import subprocess
import sys

import numpy as np
import imageio_ffmpeg

FF = imageio_ffmpeg.get_ffmpeg_exe()
SR = 22050
HOP = 256


def ebur(path):
    r = subprocess.run([FF, '-nostats', '-hide_banner', '-i', path, '-af', 'ebur128=peak=true:framelog=info', '-f', 'null', '-'],
                       capture_output=True, text=True, errors='replace')
    err = r.stderr
    st = []
    for m in re.finditer(r't:\s*([\d.]+)\s+TARGET:.*?M:\s*(-?[\d.]+|-inf)\s+S:\s*(-?[\d.]+|-inf)', err):
        st.append((float(m.group(1)), float(m.group(3)) if m.group(3) != '-inf' else -120.0))
    summ = err[err.rfind('Summary:'):]
    I = float(re.search(r'I:\s*(-?[\d.]+) LUFS', summ).group(1))
    LRA = float(re.search(r'LRA:\s*(-?[\d.]+) LU', summ).group(1))
    TP = float(re.search(r'Peak:\s*(-?[\d.]+|-inf) dBFS', summ).group(1))
    info = re.search(r'Stream #0:0.*?Audio: (\w+).*?(\d+) Hz, (\w+)[^\n]*?(?:, (\d+) kb/s)?\n', err)
    dur = re.search(r'Duration: (\d+):(\d+):([\d.]+)', err)
    d = int(dur.group(1)) * 3600 + int(dur.group(2)) * 60 + float(dur.group(3))
    # o short-term de 1 em 1 s (as linhas vêm de 0,1 em 0,1 s)
    sec = {}
    for t, s in st:
        sec[int(t)] = s   # o último valor de cada segundo (o short-term é a janela de 3 s que termina ali)
    curve = [sec.get(i, -120.0) for i in range(int(d) + 1)]
    return dict(I=I, LRA=LRA, TP=TP, dur=round(d, 2), codec=info.group(1) if info else '?', rate=int(info.group(2)) if info else 0,
                layout=info.group(3) if info else '?', kbps=int(info.group(4)) if info and info.group(4) else 0, curve=curve)


def decode(path, mono=True, sr=SR):
    args = [FF, '-v', 'quiet', '-i', path, '-ac', '1' if mono else '2', '-ar', str(sr), '-f', 'f32le', '-']
    raw = subprocess.run(args, capture_output=True).stdout
    x = np.frombuffer(raw, dtype=np.float32)
    return x if mono else x.reshape(-1, 2)


def stft_mag(x, n=1024, hop=HOP):
    w = np.hanning(n).astype(np.float32)
    frames = 1 + (len(x) - n) // hop
    idx = np.arange(n)[None, :] + hop * np.arange(frames)[:, None]
    return np.abs(np.fft.rfft(x[idx] * w, axis=1)).astype(np.float32)


def tempo_and_beats(mag):
    fps = SR / HOP
    lm = np.log1p(100.0 * mag)
    flux = np.maximum(0.0, np.diff(lm, axis=0)).sum(axis=1)
    # o fluxo grave (o bumbo, até ~150 Hz): pra achar o tempo forte do compasso
    nb = int(150 / (SR / 1024)) + 1
    low = np.maximum(0.0, np.diff(lm[:, :nb], axis=0)).sum(axis=1)
    env = flux - np.convolve(flux, np.ones(int(fps)) / int(fps), mode='same')
    env = np.maximum(env, 0.0)
    env /= env.std() + 1e-9
    # a autocorrelação nos atrasos de 60 a 190 BPM, com a preferência log-normal em torno de 120 (o costume da literatura: Ellis 2007)
    ac = np.correlate(env, env, mode='full')[len(env) - 1:]
    ac /= ac[0] + 1e-9
    best, bestv = 0, -1
    scores = []
    for bpm10 in range(600, 1901):
        bpm = bpm10 / 10
        lag = fps * 60 / bpm
        l0 = int(lag)
        v = ac[l0] * (1 - (lag - l0)) + ac[l0 + 1] * (lag - l0)
        # os múltiplos ajudam (a grade estável)
        for k in (2, 4):
            lk = lag * k
            if lk + 1 < len(ac):
                i0 = int(lk)
                v += 0.5 * (ac[i0] * (1 - (lk - i0)) + ac[i0 + 1] * (lk - i0))
        v *= np.exp(-0.5 * (np.log2(bpm / 120.0) / 0.9) ** 2)
        scores.append((bpm, v))
        if v > bestv:
            best, bestv = bpm, v
    period = fps * 60 / best
    # a fase: a que mais soma o envelope nas batidas
    ph_best, ph_v = 0.0, -1
    for ph in np.arange(0, period, 0.25):
        pos = np.arange(ph, len(env) - 1, period).astype(int)
        v = env[pos].sum()
        if v > ph_v:
            ph_best, ph_v = ph, v
    beats = np.arange(ph_best, len(env) - 1, period)
    # o tempo forte: das 4 fases do compasso, a de mais grave
    lown = low / (low.std() + 1e-9)
    bar_v = []
    for k in range(4):
        pos = beats[k::4].astype(int)
        bar_v.append(float(lown[np.clip(pos, 0, len(lown) - 1)].mean()) if len(pos) else 0)
    down = int(np.argmax(bar_v))
    # a confiança do tempo: o pico perto do melhor contra a média
    sv = np.array([v for _, v in scores])
    conf = float(bestv / (np.median(sv) + 1e-9))
    beat_t = beats / fps
    return dict(bpm=best, conf=round(conf, 2), beat0=round(float(beat_t[0]), 3), downbeat0=round(float(beat_t[down]), 3),
                bar_s=round(4 * 60 / best, 3), drive=round(float(flux.mean() / (lm.mean() + 1e-9)), 3))


def analyse(path):
    e = ebur(path)
    x = decode(path)
    mag = stft_mag(x)
    tb = tempo_and_beats(mag)
    # o começo e o fim: o primeiro e o último segundo acima de −45 LUFS short-term (a curva de 1 s)
    c = np.array(e['curve'] or [-120.0])
    on = np.where(c > -45)[0]
    start = int(on[0]) if len(on) else 0
    end = int(on[-1]) + 1 if len(on) else len(c)
    med = float(np.median(c[start:end])) if end > start else -120
    # o fim: o short-term dos últimos 3 s com som contra a mediana; queda pequena = corte seco (a faixa acaba no meio)
    tail = float(np.mean(c[max(start, end - 3):end])) if end > start else -120
    # o fim real das amostras: os últimos 50 ms em RMS
    n50 = int(0.05 * SR)
    last_rms = 20 * np.log10(np.sqrt(np.mean(x[-n50:] ** 2)) + 1e-9)
    first_rms = 20 * np.log10(np.sqrt(np.mean(x[:n50] ** 2)) + 1e-9)
    # a entrada: o primeiro segundo que chega na mediana − 1 LU (onde a faixa já está cheia)
    full = np.where(c[start:end] >= med - 1)[0]
    full_at = start + int(full[0]) if len(full) else start
    # a banda: a frequência acima da qual o espectro médio cai 50 dB abaixo do máximo (o mp3 corta em ~16 kHz)
    xs = decode(path, mono=True, sr=44100)
    seg = xs[len(xs) // 4: len(xs) // 4 + 44100 * 20]
    sp = np.abs(np.fft.rfft(seg[:2 ** 20] if len(seg) >= 2 ** 20 else seg)) + 1e-9
    f = np.fft.rfftfreq(min(len(seg), 2 ** 20), 1 / 44100)
    sm = np.convolve(20 * np.log10(sp), np.ones(200) / 200, mode='same')
    top = sm[(f > 200) & (f < 4000)].max()
    above = np.where((sm > top - 50) & (f > 4000))[0]
    band = float(f[above[-1]]) if len(above) else 0
    st = decode(path, mono=False, sr=44100)
    clip = int((np.abs(st) >= 0.999).sum())
    corr = float(np.corrcoef(st[::7, 0], st[::7, 1])[0, 1]) if st.shape[0] > 10 else 1
    # o centróide (o brilho), em Hz
    fr = np.fft.rfftfreq(1024, 1 / SR)
    cen = float((mag * fr).sum() / (mag.sum() + 1e-9))
    return dict(**{k: v for k, v in e.items() if k != 'curve'}, curve=[round(v, 1) for v in e['curve']], start_s=start, end_s=end,
                median_S=round(med, 1), tail_S=round(tail, 1), last50ms_db=round(float(last_rms), 1), first50ms_db=round(float(first_rms), 1),
                full_at_s=full_at, band_hz=round(band), clip_samples=clip, lr_corr=round(corr, 3), centroid_hz=round(cen), **tb)


def main():
    root, out = sys.argv[1], sys.argv[2]
    res = {}
    for fac in sorted(os.listdir(root)):
        d = os.path.join(root, fac)
        if not os.path.isdir(d):
            continue
        for fn in sorted(os.listdir(d)):
            if not fn.lower().endswith(('.mp3', '.wav')):
                continue
            p = os.path.join(d, fn)
            a = analyse(p)
            res[f'{fac}/{fn}'] = a
            print(f"{fac:6} {fn[:44]:44} {a['dur']:6.1f}s {a['codec']:>9} {a['kbps']:4} I {a['I']:6.1f} LRA {a['LRA']:4.1f} TP {a['TP']:5.1f}"
                  f" | {a['bpm']:5.1f} bpm c{a['conf']:4.2f} | in {a['start_s']:3}s full {a['full_at_s']:3}s end {a['end_s']:3}s"
                  f" med {a['median_S']:6.1f} tail {a['tail_S']:6.1f} last {a['last50ms_db']:6.1f} | band {a['band_hz']:5} clip {a['clip_samples']:5}"
                  f" corr {a['lr_corr']:.2f} cen {a['centroid_hz']}", flush=True)
    json.dump(res, open(out, 'w', encoding='utf-8'), indent=1)


if __name__ == '__main__':
    main()
