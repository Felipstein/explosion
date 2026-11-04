# As opções do hover do menu (04/11), desenhadas aqui depois de três reprovados: a tecla do console do gerador ("tá mais pra um
# clique"), o sopro do gerador ("machuca os ouvidos") e o tom afinado sintetizado ("parece que tô no Windows"). O que os designers de
# som de jogo dizem: os sons da interface são uma família só, o hover é o irmão leve da confirmação, curto, sem agudo que canse
# (ele toca centenas de vezes) e nada de tom solto (é o que soa como o sistema operacional). Três gestos do mesmo mundo (o console
# de radar), sem nada acima de 4 kHz, com um pouco de sala (o bunker). Cada um sai sozinho e numa prévia com a música do menu no
# volume do jogo (0,55; a interface a 0,6) e o mouse passando por quatro botões.
#   python hover_options.py
import os, subprocess, wave
import numpy as np
import imageio_ffmpeg

SR = 48000
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..', 'captures', 'audio', '55_menu_hover_opcoes'))
rng = np.random.default_rng(7)


def band(x, lo, hi):
    """Passa-faixa suave (as bordas de 2ª ordem), sem defasar."""
    f = np.fft.rfftfreq(len(x), 1 / SR)
    h = np.ones_like(f)
    if lo:
        h *= 1 / np.sqrt(1 + (lo / np.maximum(f, 1e-3)) ** 4)
    if hi:
        h *= 1 / np.sqrt(1 + (f / hi) ** 4)
    return np.fft.irfft(np.fft.rfft(x) * h, len(x))


def room(x, rt60=0.35, wet_db=-14):
    """A sala: a resposta de um bunker pequeno (ruído decaindo, abafado em 3 kHz) misturada baixa."""
    n = int(SR * rt60)
    ir = band(rng.standard_normal(n) * np.exp(-6.9 * np.arange(n) / n), 0, 3000)
    ir /= np.sqrt(np.sum(ir ** 2))
    wet = np.convolve(x, ir)
    dry = np.concatenate([x, np.zeros(len(wet) - len(x))])
    return dry + wet * 10 ** (wet_db / 20)


def env(n, attack_s, tau_s):
    t = np.arange(n) / SR
    return np.minimum(1.0, t / max(attack_s, 1e-4)) * np.exp(-np.maximum(0, t - attack_s) / tau_s)


def opt_a():
    # o eco do radar: um retorno de sinal curto (ruído numa faixa estreita em volta de 1,1 kHz) e o eco dele bem mais baixo
    n = int(SR * 0.12)
    burst = band(rng.standard_normal(n), 800, 1500) * env(n, 0.003, 0.018)
    x = burst.copy()
    d = int(SR * 0.07)
    x[d:] += burst[: n - d] * 0.25
    return room(x), -20


def opt_b():
    # o fósforo: a estática macia da tela reagindo (ruído de 1 a 4 kHz, curtíssimo) com três estalinhos dentro dele
    n = int(SR * 0.06)
    x = band(rng.standard_normal(n), 1000, 4000) * env(n, 0.001, 0.008)
    for at, g in ((0.002, 0.6), (0.007, 0.35), (0.013, 0.2)):
        i = int(SR * at)
        x[i: i + 24] += band(rng.standard_normal(24 + 400), 1000, 4000)[:24] * g
    return room(band(band(x, 0, 3200), 0, 3200), 0.25, -16), -22   # o filtro duas vezes: os estalinhos curtos vazavam agudo


def opt_c():
    # o toque: o baque grave e curto (a nota caindo de 170 pra 85 Hz em 60 ms) com uma pontinha de definição (1 a 2,5 kHz, 6 ms)
    n = int(SR * 0.12)
    t = np.arange(n) / SR
    freq = 85 + 85 * np.exp(-t / 0.02)
    body = np.sin(2 * np.pi * np.cumsum(freq) / SR) * env(n, 0.002, 0.035)
    tip = band(rng.standard_normal(n), 1000, 2500) * env(n, 0.0005, 0.003) * 0.3
    return room(body + tip, 0.3, -18), -16


def level(x, peak_db):
    return x * 10 ** (peak_db / 20) / (np.abs(x).max() + 1e-9)


def write(path, x):
    st = np.stack([x, x], axis=1) if x.ndim == 1 else x
    with wave.open(path, 'wb') as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((np.clip(st, -1, 1) * 32767).astype('<i2').tobytes())


os.makedirs(OUT, exist_ok=True)
raw = subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(), '-v', 'quiet', '-ss', '20', '-t', '6', '-i', os.path.join(HERE, '..', 'Resources', 'UI', 'snd', 'Main_Menu_1.mp3'),
                      '-f', 'f32le', '-ac', '2', '-ar', str(SR), '-'], capture_output=True, check=True).stdout
music = np.frombuffer(raw, np.float32).reshape(-1, 2) * 0.55
# o nível de cada um pela música (04/11, "não dá pra ouvir em nenhum momento"): na faixa onde o hover vive, a energia dele nos
# primeiros 60 ms fica 6 dB acima da média da música na mesma faixa (a música no volume do jogo); o pico nunca passa de −6 dBFS
BANDS = {'A_eco_do_radar': (800, 1500), 'B_fosforo': (1000, 3200), 'C_toque': (60, 250)}


def band_rms(x, lo, hi):
    return np.sqrt(np.mean(band(x, lo, hi) ** 2) + 1e-12)


for key, fn in (('A_eco_do_radar', opt_a), ('B_fosforo', opt_b), ('C_toque', opt_c)):
    x, _ = fn()
    lo, hi = BANDS[key]
    m = band_rms(music.mean(axis=1), lo, hi)
    h = band_rms(x[: int(SR * 0.06)] * 0.6, lo, hi)          # a classe da interface (0,6)
    x = x * 10 ** (6 / 20) * m / h
    if np.abs(x).max() > 10 ** (-6 / 20):
        x = level(x, -6)
    pk = 20 * np.log10(np.abs(x).max())
    write(os.path.join(OUT, f'{key}.wav'), x)
    mix = music.copy()
    for t in (1.0, 1.55, 2.1, 2.65, 4.2, 4.7):
        i = int(SR * t)
        k = min(len(x), len(mix) - i)
        mix[i: i + k] += (x[:k] * 0.6)[:, None]
    write(os.path.join(OUT, f'{key}_na_musica.wav'), mix)
    hh = band_rms(x[: int(SR * 0.06)] * 0.6, lo, hi)
    print(f'{key}: pico {pk:.1f} dBFS, na faixa {lo}-{hi} Hz o hover fica {20 * np.log10(hh / m):+.1f} dB sobre a música')
