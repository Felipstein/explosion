# (04/11) O clique do HUD a partir do efeito gerado: o gerador (ElevenLabs) entrega no mínimo 0,5 s com silêncio e cauda; o clique de
# interface é só o transiente. Acha o começo (a primeira amostra acima de −30 dB do pico), guarda LEN ms a partir de 2 ms antes, entrada
# de 2 ms e saída em cosseno² nos últimos 40% (sem estalo no corte), pico em PEAK dBFS. Roda no Blender (o `aud` lê o mp3):
#   blender -b --factory-startup --python Game/Tools/sound_click.py -- <entrada.mp3> <saida.wav> [LEN_MS PEAK]
# O padrão: 90 ms, −14 dBFS (a interface fica abaixo das vozes e dos alertas). Receita em docs/elenco/SONS_UNIDADES.md.
import sys
import wave

import aud
import numpy as np

args = sys.argv[sys.argv.index('--') + 1:]
src, dst = args[0], args[1]
LEN_MS, PEAK = (float(v) for v in (args[2:4] if len(args) >= 4 else (90.0, -14.0)))
s = aud.Sound(src)
R = int(s.specs[0])
d = np.asarray(s.data(), dtype=np.float32)
x = d.mean(axis=1) if d.ndim == 2 else d
a = np.abs(x)
start = max(0, int(np.argmax(a > a.max() * 10 ** (-30 / 20))) - int(0.002 * R))
y = x[start:start + int(LEN_MS / 1000 * R)].copy()
n_in, n_out = int(0.002 * R), int(len(y) * 0.4)
y[:n_in] *= np.linspace(0, 1, n_in)
y[-n_out:] *= np.cos(0.5 * np.pi * np.linspace(0, 1, n_out)) ** 2
y *= 10 ** (PEAK / 20) / max(float(np.abs(y).max()), 1e-9)
with wave.open(dst, 'wb') as w:
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(R)
    w.writeframes((y * 32767).astype(np.int16).tobytes())
# a medida: onde a energia está (o clique "seco" fica no médio; agudo demais = estalo de plástico, grave demais = baque)
F = np.abs(np.fft.rfft(y * np.hanning(len(y)))) ** 2
f = np.fft.rfftfreq(len(y), 1 / R)
c = float((F * f).sum() / F.sum())
print('CLICK ok', dst, f'começo {start / R * 1000:.0f} ms', f'centro do espectro {c:.0f} Hz')
