# (04/11) O envelope do som de selecionar construção (a War Factory aprovada no rumo: "tá pode ser, tá no rumo"). O gerador entrega o
# ambiente do prédio no volume cheio, começando e acabando de repente ("começa já alto e termina alto"); o som de seleção cresce macio,
# segura um instante e morre devagar até o silêncio, baixo (toca toda vez que se clica no prédio):
#   entrada em seno² até ATTACK s, segura até HOLD s, saída em cosseno² até zerar no fim (END s, corta o resto); pico em PEAK dBFS.
# Roda no Blender (o `aud` lê o mp3; não há ffmpeg aqui), sai em WAV mono 16 bits:
#   blender -b --factory-startup --python Game/Tools/sound_envelope.py -- <entrada.mp3> <saida.wav> [ATTACK HOLD END PEAK]
# O padrão (o da War Factory): 0,35 / 0,70 / 2,0 / −16 dBFS. Receita e decisões em docs/elenco/SONS_UNIDADES.md.
import sys
import wave

import aud
import numpy as np

args = sys.argv[sys.argv.index('--') + 1:]
src, dst = args[0], args[1]
ATTACK, HOLD, END, PEAK = (float(v) for v in (args[2:6] if len(args) >= 6 else (0.35, 0.70, 2.0, -16.0)))
s = aud.Sound(src)
R = int(s.specs[0])
d = np.asarray(s.data(), dtype=np.float32)
x = d.mean(axis=1) if d.ndim == 2 else d   # mono: o som do prédio sai do ponto dele no mundo
x = x[:int(min(END, len(x) / R) * R)]
end = len(x) / R
t = np.arange(len(x)) / R
env = np.ones_like(x)
env[t < ATTACK] = np.sin(0.5 * np.pi * t[t < ATTACK] / ATTACK) ** 2
m = t > HOLD
env[m] = np.cos(0.5 * np.pi * np.clip((t[m] - HOLD) / (end - HOLD), 0, 1)) ** 2
y = x * env
y *= 10 ** (PEAK / 20) / max(float(np.abs(y).max()), 1e-9)
with wave.open(dst, 'wb') as w:
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(R)
    w.writeframes((y * 32767).astype(np.int16).tobytes())
print('ENVELOPE ok', dst, round(end, 2), 's')
