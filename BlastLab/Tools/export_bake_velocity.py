"""Velocidade da explosão gravada em baixa resolução, todos os quadros.

A interação com o que passa (avião, míssil, disparo) deforma a própria
explosão gravada em vez de trocá-la por outra fumaça: um campo de
deslocamento, calculado no compute shader, empurra a leitura da gravação. Pra
esse campo andar junto com a fumaça (a nuvem sobe), o compute precisa da
velocidade gravada, e ele não lê a Sparse Volume Texture. Então ela vai também
como uma textura 3D comum por variante, os quadros empilhados em z:

  v<n>.bin  RGBA16F 32 × 32 × (32·quadros): velocidade (m/s, eixos da Unreal,
            ponderada pela fumaça), fumaça (fuligem + poeira, média)

1/3 da resolução da gravação (1.19 m por voxel na Carga Pesada, a ordem da
grade de velocidade do campo, 2 m). Fora da fumaça, a velocidade média.

velocity.json leva também a altura média da fuligem de cada quadro (m locais):
é por onde o avião e o míssil cruzam a nuvem.

Rodar com o Python do sistema (numpy): python BlastLab/Tools/export_bake_velocity.py
"""
import json
import os

import numpy as np

TOOLS = os.path.dirname(os.path.abspath(__file__))
RAW = os.path.join(TOOLS, '..', 'Intermediate', 'BakeRaw', 'r96_v%d')
OUT = os.path.join(TOOLS, '..', 'Intermediate', 'BakeVelocity')
VARIANTS = 3
RES = 32


def load(path, dtype, n):
    a = np.fromfile(path, dtype=dtype)
    return a.reshape(n, n, n, 4)        # [z][y][x][c]


def down(a, k):
    n = a.shape[0] // k
    return a.reshape(n, k, n, k, n, k, -1)


def main():
    os.makedirs(OUT, exist_ok=True)
    info = {'resolution': RES, 'variants': VARIANTS}
    for v in range(VARIANTS):
        meta = json.load(open(os.path.join(RAW % v, 'meta.json'), encoding='utf-8'))
        n = meta['resolution']
        n = int(n[0] if isinstance(n, list) else n)
        k = n // RES
        frames = len(meta['frame_times_s'])
        info['frames'] = frames
        info['frame_times_s'] = meta['frame_times_s']
        out = np.zeros((frames * RES, RES, RES, 4), np.float16)
        heights = []
        vox = meta['domain_m'] / n
        zc = (np.arange(n) + 0.5) * vox                              # altura do centro de cada fatia (m locais)
        for q in range(frames):
            a = load(os.path.join(RAW % v, 'f%03d_a.bin' % q), np.uint8, n).astype(np.float32) / 255.0
            b = load(os.path.join(RAW % v, 'f%03d_b.bin' % q), np.float16, n).astype(np.float32)
            smoke = a[..., 0] ** 2 * 4.0 + a[..., 2] ** 2 * 2.5      # fuligem + poeira (js/bake.js)
            vel = b[..., 1:4]
            w = down(smoke[..., None], k)
            wsum = w.sum(axis=(1, 3, 5))
            vw = (down(vel, k) * w).sum(axis=(1, 3, 5)) / np.maximum(wsum, 1e-6)
            vm = down(vel, k).mean(axis=(1, 3, 5))
            vel_lo = np.where(wsum > 1e-6, vw, vm)
            s = down(smoke[..., None], k).mean(axis=(1, 3, 5))
            out[q * RES:(q + 1) * RES] = np.concatenate([vel_lo, s], -1).astype(np.float16)
            # altura média da fuligem (m locais, desde o chão): é por onde o avião e o
            # míssil passam (battlesmoke.js smokeHeightAt fazia isso na grade do campo)
            col = (a[..., 0] ** 2 * 4.0).sum(axis=(1, 2))           # [z]
            heights.append(float((col * zc).sum() / max(col.sum(), 1e-6)))
        out.tofile(os.path.join(OUT, 'v%d.bin' % v))
        info['soot_height_m_v%d' % v] = heights
        print('variant', v, 'frames', frames, 'ok', 'soot height', round(heights[0], 1), '->', round(heights[-1], 1))
    with open(os.path.join(OUT, 'velocity.json'), 'w', encoding='utf-8') as f:
        json.dump(info, f)


if __name__ == '__main__':
    main()
