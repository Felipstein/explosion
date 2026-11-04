# A prévia de um som de hover do Atelier como ele soa no jogo (04/11): a música do menu no volume dela (0,55) e o hover na classe da
# interface (0,6), 6 dB acima da música na faixa dele (o centro do som ± 1 oitava), passando por 4 botões e mais 2. Mede o que
# importa (a duração, o centro, o agudo acima de 5 kHz, o ataque) e grava hover_na_musica.wav e hover_sozinho.wav na pasta do som.
#   python hover_preview.py <som.mp3>
import os, subprocess, sys, wave
import numpy as np
import imageio_ffmpeg

FF = imageio_ffmpeg.get_ffmpeg_exe()
SR = 48000
HERE = os.path.dirname(os.path.abspath(__file__))


def dec(path, extra=()):
    raw = subprocess.run([FF, '-v', 'quiet', *extra, '-i', path, '-f', 'f32le', '-ac', '2', '-ar', str(SR), '-'], capture_output=True, check=True).stdout
    return np.frombuffer(raw, np.float32).reshape(-1, 2).copy()


src = sys.argv[1]
x = dec(src)
m = x.mean(axis=1)
S = np.abs(np.fft.rfft(m)) ** 2
f = np.fft.rfftfreq(len(m), 1 / SR)
c = (S * f).sum() / S.sum()
env = np.abs(m)
print(f'{len(m) / SR:.2f} s, centro {int(c)} Hz, acima de 5 kHz {100 * S[f > 5000].sum() / S.sum():.1f}%, '
      f'ataque ate -6 dB do pico {1000 * np.argmax(env > env.max() * 0.5) / SR:.0f} ms')
music = dec(os.path.join(HERE, '..', 'Resources', 'UI', 'snd', 'Main_Menu_1.mp3'), ('-ss', '20', '-t', '6')) * 0.55


def brms(y):
    F = np.fft.rfftfreq(len(y), 1 / SR)
    Y = np.fft.rfft(y)
    Y[(F < c / 2) | (F > c * 2)] = 0
    return np.sqrt(np.mean(np.fft.irfft(Y, len(y)) ** 2) + 1e-12)


x *= 10 ** (6 / 20) * brms(music.mean(axis=1)) / brms(m * 0.6)
if np.abs(x).max() > 10 ** (-6 / 20):
    x *= 10 ** (-6 / 20) / np.abs(x).max()
mix = music.copy()
for t in (1.0, 1.55, 2.1, 2.65, 4.2, 4.7):
    i = int(SR * t)
    k = min(len(x), len(mix) - i)
    mix[i: i + k] += x[:k] * 0.6
out = os.path.dirname(os.path.abspath(src))
for name, y in (('hover_na_musica.wav', mix), ('hover_sozinho.wav', x)):
    with wave.open(os.path.join(out, name), 'wb') as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((np.clip(y, -1, 1) * 32767).astype('<i2').tobytes())
print(f'no jogo: pico {20 * np.log10(np.abs(x).max()):.1f} dBFS; prévias em {out}')
