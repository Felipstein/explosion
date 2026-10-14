"""Acrescenta ao atlas.json de um cozimento a caixa do alfa de cada quadro ("bbox": [u0, v0, u1, v1], fração do
quadro, v pra cima), com uma borda de folga: o jogo desenha só essa caixa (menos pixel pintado, o mesmo visual).
O quadro f e o f+1 são misturados no material, então a caixa usada no cartão é a união das duas (o FXTread faz).
Uso: python atlas_bbox.py <pasta_do_cozimento> [limiar=0.01] [folga_px=6]"""
import json
import os
import subprocess
import sys

import imageio_ffmpeg
import numpy as np

d = sys.argv[1]
thr = float(sys.argv[2]) if len(sys.argv) > 2 else 0.01
pad = int(sys.argv[3]) if len(sys.argv) > 3 else 6
meta = json.load(open(os.path.join(d, 'atlas.json')))
R, C, N = meta['res'], meta['cols'], meta['frames']
W, H = C * R, meta['rows'] * R
raw = subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(), '-loglevel', 'error', '-i', os.path.join(d, 'six_pos.exr'), '-f', 'rawvideo',
                      '-pix_fmt', 'rgba64le', '-'], capture_output=True).stdout
a = np.frombuffer(raw, np.uint16).reshape(H, W, 4)[..., 3].astype(np.float32) / 65535
boxes, area = [], []
for k in range(N):
    r, c = divmod(k, C)
    f = a[r * R:(r + 1) * R, c * R:(c + 1) * R]
    ys, xs = np.nonzero(f > thr)
    if len(xs) == 0:
        boxes.append([0.5, 0.5, 0.5, 0.5]); area.append(0.0); continue
    x0, x1 = max(xs.min() - pad, 0), min(xs.max() + pad, R - 1)
    y0, y1 = max(ys.min() - pad, 0), min(ys.max() + pad, R - 1)
    # v pra cima: a linha 0 da imagem é o topo do quadro
    boxes.append([round(x0 / R, 4), round(1 - (y1 + 1) / R, 4), round((x1 + 1) / R, 4), round(1 - y0 / R, 4)])
    area.append((x1 - x0 + 1) * (y1 - y0 + 1) / R / R)
meta['bbox'] = boxes
json.dump(meta, open(os.path.join(d, 'atlas.json'), 'w'), indent=1)
print('ATLAS_BBOX área média %.0f %% do quadro (mín %.0f %%, máx %.0f %%)' % (100 * np.mean(area), 100 * min(area), 100 * max(area)))
