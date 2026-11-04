# (04/11) As falas recortadas da AMOSTRA da voz aprovada (o Voice Design lê todas as falas daquela voz numa amostra só; aprovada a
# voz, as falas saem dela, sem gerar de novo: "por que você gerou uma por uma de novo?"). Acha os trechos de fala entre as pausas (a
# energia de 10 ms acima de −45 dB; pausas menores que GAP ms ficam dentro da frase), confere com a lista de nomes e grava um WAV mono
# por fala: 60 ms antes, 150 ms depois, entrada de 5 ms e saída de 80 ms. Um nome com ":N" junta N trechos (a fala de duas frases).
# Roda no Blender (o `aud` lê o mp3):
#   blender -b --factory-startup --python Game/Tools/voice_cut.py -- <amostra.mp3> <pasta_saida> [--gap 220] [--longest] nome1 nome2:2 ...
# --longest: a amostra lida de corrido; as fronteiras são as N−1 pausas mais longas (conferir as durações contra o texto).
# --texts <arquivo>: as falas, uma por linha, na ordem dos nomes: o alinhamento pelo texto (o melhor pra amostra de corrido).
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
BY_LONGEST = False
if rest and rest[0] == '--gap':
    GAP, rest = int(rest[1]), rest[2:]
if rest and rest[0] == '--longest':
    BY_LONGEST, rest = True, rest[1:]
TEXTS = []
if rest and rest[0] == '--texts':
    TEXTS, rest = [t for t in open(rest[1], encoding='utf-8').read().splitlines() if t.strip()], rest[2:]
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
if TEXTS and len(TEXTS) == need and len(seg) != need:
    # (04/11) o alinhamento pelo texto: as fronteiras são pausas (≥ 50 ms) escolhidas por programação dinâmica pra que cada fala dure o
    # que o texto dela pede (as letras × a velocidade da amostra) e as pausas escolhidas sejam as mais longas possíveis. As N−1 pausas
    # mais longas sozinhas erram quando o leitor respira no meio de uma fala (o Ranger: uma fala de 0,01 s)
    gaps = []
    i = 0
    while i < len(on):
        if not on[i]:
            j = i
            while j < len(on) and not on[j]:
                j += 1
            if i > 0 and j < len(on) and j - i >= 5:
                gaps.append((i, j))
            i = j
        else:
            i += 1
    first = next(k for k in range(len(on)) if on[k])
    last = len(on) - next(k for k in range(len(on)) if on[len(on) - 1 - k])
    chars = [max(4, len(t)) for t in TEXTS]
    rate = float(on[first:last].sum()) / sum(chars)              # quadros de fala por letra
    ends = [(first, first)] + gaps + [(last, last)]               # a fronteira 0 é o começo, a última é o fim
    G = len(ends)
    INF = 1e18
    dp = [[INF] * G for _ in range(need + 1)]
    back = [[-1] * G for _ in range(need + 1)]
    dp[0][0] = 0.0
    for kk in range(1, need + 1):
        e = chars[kk - 1] * rate
        for jj in range(1, G):
            if kk == need and jj != G - 1:
                continue
            if kk < need and jj == G - 1:
                continue
            bonus = 0.0 if jj == G - 1 else -2.0 * np.log(ends[jj][1] - ends[jj][0])
            for ii in range(0, jj):
                if dp[kk - 1][ii] >= INF:
                    continue
                dur = ends[jj][0] - ends[ii][1]
                if dur <= 0:
                    continue
                speech = float(on[ends[ii][1]:ends[jj][0]].sum())
                c = ((speech - e) / (0.3 * e + 8.0)) ** 2 + bonus
                if dp[kk - 1][ii] + c < dp[kk][jj]:
                    dp[kk][jj] = dp[kk - 1][ii] + c
                    back[kk][jj] = ii
    picks = [G - 1]
    for kk in range(need, 0, -1):
        picks.append(back[kk][picks[-1]])
    picks = picks[::-1]
    seg = [[ends[picks[kk]][1] * 10, ends[picks[kk + 1]][0] * 10] for kk in range(need)]
elif BY_LONGEST and len(seg) != need:
    # (04/11) a amostra lida de corrido (as falas do Ranger e do Stryker: pausas de 150 a 400 ms entre uma e outra): as N−1 pausas mais
    # longas são as fronteiras das falas (a pausa entre frases é mais longa que a vírgula). Confere-se pela duração contra o texto (--texts)
    gaps = []
    i = 0
    while i < len(on):
        if not on[i]:
            j = i
            while j < len(on) and not on[j]:
                j += 1
            if i > 0 and j < len(on) and j - i >= 6:
                gaps.append((j - i, i, j))
            i = j
        else:
            i += 1
    cut = sorted(sorted(gaps, reverse=True)[:need - 1], key=lambda g: g[1])
    first = next(k for k in range(len(on)) if on[k])
    last = len(on) - next(k for k in range(len(on)) if on[len(on) - 1 - k])
    bounds = [first] + [x for g in cut for x in (g[1], g[2])] + [last]
    seg = [[bounds[2 * k] * 10, bounds[2 * k + 1] * 10] for k in range(len(bounds) // 2)]
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
