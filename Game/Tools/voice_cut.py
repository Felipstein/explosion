# (04/11) As falas recortadas da AMOSTRA da voz aprovada (o Voice Design lê todas as falas daquela voz numa amostra só; aprovada a
# voz, as falas saem dela, sem gerar de novo: "por que você gerou uma por uma de novo?"). Acha os trechos de fala entre as pausas (a
# energia de 10 ms acima de −45 dB; pausas menores que GAP ms ficam dentro da frase), confere com a lista de nomes e grava um WAV mono
# por fala: 60 ms antes, 150 ms depois, entrada de 5 ms e saída de 80 ms. Um nome com ":N" junta N trechos (a fala de duas frases).
# Roda no Blender (o `aud` lê o mp3):
#   blender -b --factory-startup --python Game/Tools/voice_cut.py -- <amostra.mp3> <pasta_saida> [--gap 220] nome1 nome2:2 ...
# Se o número de trechos não bater com os nomes, lista os trechos e não grava nada (ajustar o --gap ou os ":N").
import os
import sys
import wave

import aud
import numpy as np

args = sys.argv[sys.argv.index('--') + 1:]
src, out = args[0], args[1]
rest = args[2:]
GAP = 220
if rest and rest[0] == '--gap':
    GAP, rest = int(rest[1]), rest[2:]
names = [(n.split(':')[0], int(n.split(':')[1]) if ':' in n else 1) for n in rest]
s = aud.Sound(src)
R = int(s.specs[0])
d = np.asarray(s.data(), dtype=np.float32)
x = d.mean(axis=1) if d.ndim == 2 else d
N = int(0.01 * R)
on = np.array([np.sqrt(np.mean(x[i:i + N] ** 2)) for i in range(0, len(x) - N, N)]) > 10 ** (-45 / 20)
seg = []
i = 0
while i < len(on):
    if on[i]:
        j = i
        while j < len(on) and on[j]:
            j += 1
        if seg and i * 10 - seg[-1][1] < GAP:
            seg[-1][1] = j * 10
        else:
            seg.append([i * 10, j * 10])
        i = j
    else:
        i += 1
seg = [g for g in seg if g[1] - g[0] > 60]
need = sum(k for _, k in names)
if need != len(seg):
    print('VOICE_CUT trechos', len(seg), 'nomes pedem', need, [tuple(g) for g in seg])
    sys.exit(1)
os.makedirs(out, exist_ok=True)
k = 0
for name, cnt in names:
    a, b = seg[k][0], seg[k + cnt - 1][1]
    k += cnt
    y = x[max(0, int((a - 60) / 1000 * R)):int((b + 150) / 1000 * R)].copy()
    n1, n2 = int(0.005 * R), int(0.08 * R)
    y[:n1] *= np.linspace(0, 1, n1)
    y[-n2:] *= np.cos(0.5 * np.pi * np.linspace(0, 1, n2)) ** 2
    with wave.open(os.path.join(out, name + '.wav'), 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(R)
        w.writeframes((np.clip(y, -1, 1) * 32767).astype(np.int16).tobytes())
    print('VOICE_CUT', name, a, b)
