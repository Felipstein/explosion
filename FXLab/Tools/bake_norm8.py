"""As folhas da poeira em 8 bits comprimidos (BC7), sem faixas: o passo da otimização "compressão" (docs/POEIRA_DAS_LAGARTAS.md).

O problema: as folhas do cozimento guardam a luz pré-multiplicada pelo alfa, em valores pequenos (p99,9 de 0,07 a 0,14;
máximo de 0,4 a 1,8 em poucos pixels). Em 8 bits direto, a luz típica teria ~20 degraus: faixas na poeira.
O jeito dos jogos (as folhas 6-way do EmberGen e da Unity saem normalizadas, em 8 bits): cada folha é multiplicada por
um ganho que leva o p99,99 da luz pra 1 (o resto satura: são pixels isolados), e o material divide de volta
(o LightScale do M_TreadDust). O alfa fica como está.

Sai em <cozimento>/norm8/: six_pos.png e six_neg.png (16 bits, a Unreal comprime em BC7 na importação) e o ganho no
atlas.json ("light_scale": [pos, neg], o que o material multiplica).
Uso: python bake_norm8.py <pasta_do_cozimento>"""
import json
import os
import subprocess
import sys

import imageio_ffmpeg
import numpy as np

d = sys.argv[1]
ff = imageio_ffmpeg.get_ffmpeg_exe()
meta = json.load(open(os.path.join(d, 'atlas.json')))
R = meta['res']
W, H = meta['cols'] * R, meta['rows'] * R
out = os.path.join(d, 'norm8')
os.makedirs(out, exist_ok=True)
scale = []
for name in ('six_pos', 'six_neg'):
    raw = subprocess.run([ff, '-loglevel', 'error', '-i', os.path.join(d, name + '.exr'), '-f', 'rawvideo', '-pix_fmt', 'gbrapf32le', '-'],
                         capture_output=True, check=True).stdout
    g, b, r, a = np.frombuffer(raw, np.float32).reshape(4, H, W)
    rgb = np.stack([r, g, b], -1)
    top = float(np.percentile(rgb[a > 1e-4], 99.99))
    gain = 1.0 / max(top, 1e-6)
    sat = float((rgb * gain > 1.0).mean() * 100.0)
    img = np.concatenate([np.clip(rgb * gain, 0, 1), np.clip(a, 0, 1)[..., None]], -1)
    u16 = np.round(img * 65535.0).astype('>u2')
    subprocess.run([ff, '-y', '-loglevel', 'error', '-f', 'rawvideo', '-pix_fmt', 'rgba64be', '-s', f'{W}x{H}', '-i', '-',
                    os.path.join(out, name + '.png')], input=u16.tobytes(), check=True)
    scale.append(round(1.0 / gain, 6))
    print('NORM8 %s: p99,99 da luz %.4f, ganho %.2f, %.4f %% dos pixels saturam' % (name, top, gain, sat))
meta['light_scale'] = scale
json.dump(meta, open(os.path.join(d, 'atlas.json'), 'w'), indent=1)
print('NORM8 light_scale', scale, '->', out)
