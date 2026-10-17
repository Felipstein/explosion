"""Medidas do cozimento que o laboratório (FXLab/web) e a Unreal (FXLab) usam igual.

Uso: blender -b --factory-startup --python atlas_meta.py -- <pasta_do_cozimento> [segurar_o_fundo_a_partir_de_s]

Acrescenta ao atlas.json:
- pivot: [x, y] o centro da bola no 1º quadro (centroide da cobertura), fração do quadro com y pra cima.
  É o ponto do cartão que fica no anel da torre;
- ball_bottom: por quadro, o fundo da bola (y pra cima): de baixo pra cima, a 1ª linha com largura
  ≥ 45% da maior. Abaixo dele é o caule de fumaça da Gasoline Explosion, que o cartão esconde (ele
  tampava o fogo da bolota seguinte);
- emit_sum: por quadro, o fogo total (soma da luminância da emissão), e
- fire_end: o 1º quadro depois do pico com o fogo abaixo de 0.5% do pico (a casca com rachadura acesa
  ainda é fogo: é o que o Felipe aprovou; daí em diante é fumaça).
"""
import json
import os
import sys

import numpy as np
import OpenImageIO as oiio

d = os.path.abspath(sys.argv[sys.argv.index('--') + 1])
meta = json.load(open(os.path.join(d, 'atlas.json')))


def read(name):
    i = oiio.ImageInput.open(os.path.join(d, name))
    s = i.spec()
    a = i.read_image(oiio.FLOAT).reshape(s.height, s.width, s.nchannels)
    i.close()
    return a


pos, emit = read('six_pos.exr'), read('emit.exr')
R, C, N = meta['res'], meta['cols'], meta['frames']


def cell(img, k):
    r, c = divmod(k, C)
    return img[r * R:(r + 1) * R, c * R:(c + 1) * R]


a0 = cell(pos, 0)[..., 3]
ys, xs = np.mgrid[0:R, 0:R]
w = a0.sum()
pivot = [float((a0 * xs).sum() / w / R), float(1 - (a0 * ys).sum() / w / R)]

bottoms, sums = [], []
for k in range(N):
    a = cell(pos, k)[..., 3]
    width = (a > 0.05).sum(1)[::-1]                    # de baixo pra cima
    b = int(np.argmax(width >= 0.45 * width.max())) if width.max() > 0 else 0
    bottoms.append(round(b / R, 4))
    e = cell(emit, k)[..., :3]
    sums.append(float((e @ np.array([0.2126, 0.7152, 0.0722])).sum()))
# segurar o fundo a partir de um tempo (16/10, a gravação que se desfaz de baixo pra cima: a base some primeiro, o
# "fundo da bola" medido subiria e o cartão, assentado por ele, desceria junto: o "cair" que o Felipe reprovou)
argv = sys.argv[sys.argv.index('--') + 1:]
if len(argv) > 1:
    hold_t = float(argv[1])
    times = meta.get('times_s', [])
    k_hold = next((k for k in range(N) if k < len(times) and times[k] >= hold_t), None)
    if k_hold is not None:
        for k in range(k_hold + 1, N):
            bottoms[k] = bottoms[k_hold]
        print('ATLAS_META fundo segurado a partir do quadro', k_hold, '(%.2f s):' % times[k_hold], bottoms[k_hold])
peak = int(np.argmax(sums))
fire_end = next((k for k in range(peak, N) if sums[k] < 0.005 * sums[peak]), N - 1)

meta.update({'pivot': pivot, 'ball_bottom': bottoms, 'emit_sum': [round(s, 2) for s in sums], 'fire_end': fire_end})
json.dump(meta, open(os.path.join(d, 'atlas.json'), 'w'), indent=1)
print('ATLAS_META pivot', [round(p, 3) for p in pivot], 'fire_end', fire_end, 'pico', peak)
print('ATLAS_META fogo por quadro (% do pico):', ' '.join(f'{100 * s / max(sums[peak], 1e-9):.0f}' for s in sums))
