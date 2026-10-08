"""Velocidade da explosão gravada em baixa resolução, todos os quadros.

A interação com o que passa (avião, míssil, disparo) deforma a própria
explosão gravada em vez de trocá-la por outra fumaça: um campo de
deslocamento, calculado no compute shader, empurra a leitura da gravação. Pra
esse campo andar junto com a fumaça (a nuvem sobe), o compute precisa da
velocidade gravada, e ele não lê a Sparse Volume Texture. Então ela vai também
como uma textura 3D comum por variante, os quadros empilhados em z:

  v<n>.bin  RGBA16F 32 × 32 × (32·quadros): velocidade (m/s, eixos da Unreal,
            ponderada pela fumaça), extinção (1/m, média; a sombra da fumaça
            no chão, BattleSmoke.usf MainShadow)

1/3 da resolução da gravação (1.19 m por voxel na Carga Pesada, a ordem da
grade de velocidade do campo, 2 m). Fora da fumaça, a velocidade média.

velocity.json leva também a altura média da fuligem de cada quadro (m locais):
é por onde o avião e o míssil cruzam a nuvem.

  nrm<n>.bin  BGRA8 32 × 32 × (32·quadros): a normal pra fora da fumaça
            (−∇σ/|∇σ|, ·0.5 + 0.5) e a força da borda (0.3/texel = 1, a borda
            de verdade). É o deslocamento piroclástico do material da explosão
            (build_explosion.py): pré-calculada aqui, ela custa 2 leituras por
            amostra da marcha em vez de 8.

Rodar com o Python do sistema (numpy): python BlastLab/Tools/export_bake_velocity.py
Outra gravação (BakeRaw/<nome>, uma variante só, ex. a eg_gasoline do EmberGen):
python BlastLab/Tools/export_bake_velocity.py <nome> → BakeVelocity/<nome>/
"""
import json
import os
import sys

import numpy as np

TOOLS = os.path.dirname(os.path.abspath(__file__))
RAW = os.path.join(TOOLS, '..', 'Intermediate', 'BakeRaw', 'r96_v%d')
OUT = os.path.join(TOOLS, '..', 'Intermediate', 'BakeVelocity')
VARIANTS = 3
if len(sys.argv) > 1:
    RAW = os.path.join(TOOLS, '..', 'Intermediate', 'BakeRaw', sys.argv[1] + '%.0s')    # '%.0s' % v = ''
    OUT = os.path.join(OUT, sys.argv[1])
    VARIANTS = 1
RES = 32
# extinção do material da explosão (build_explosion.py LOOK sootExt/dustExt),
# por voxel da gravação: por metro, dividida pelo voxel local (38 m / 96). É a
# mesma em qualquer escala (DensityScale = S·opacidade e o voxel cresce com S)
SOOT_EXT, DUST_EXT = 13.5, 5.5


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
        frames = len(meta['frame_times_s'])
        # os quadros empilhados em z cabem numa textura 3D (2048 de profundidade no
        # D3D12): a eg_gasoline tem 74 quadros, 74 × 32 = 2368 derrubava o jogo
        res = RES
        while res * frames > 2048 or n % res:
            res -= 8
        info['resolution'] = res
        k = n // res
        info['frames'] = frames
        info['frame_times_s'] = meta['frame_times_s']
        out = np.zeros((frames * res, res, res, 4), np.float16)
        nrm = np.zeros((frames * res, res, res, 4), np.uint8)
        heights = []
        vox = meta['domain_m'] / n
        zc = (np.arange(n) + 0.5) * vox                              # altura do centro de cada fatia (m locais)
        for q in range(frames):
            a = load(os.path.join(RAW % v, 'f%03d_a.bin' % q), np.uint8, n).astype(np.float32) / 255.0
            b = load(os.path.join(RAW % v, 'f%03d_b.bin' % q), np.float16, n).astype(np.float32)
            smoke = a[..., 0] ** 2 * 4.0 + a[..., 2] ** 2 * 2.5      # fuligem + poeira (js/bake.js)
            ext = (SOOT_EXT * a[..., 0] ** 2 * 4.0 + DUST_EXT * a[..., 2] ** 2 * 2.5) / vox
            vel = b[..., 1:4]
            w = down(smoke[..., None], k)
            wsum = w.sum(axis=(1, 3, 5))
            vw = (down(vel, k) * w).sum(axis=(1, 3, 5)) / np.maximum(wsum, 1e-6)
            vm = down(vel, k).mean(axis=(1, 3, 5))
            vel_lo = np.where(wsum > 1e-6, vw, vm)
            s = down(ext[..., None], k).mean(axis=(1, 3, 5))
            out[q * res:(q + 1) * res] = np.concatenate([vel_lo, s], -1).astype(np.float16)
            # gradiente centrado (eixos z, y, x da grade) → normal pra fora e força
            g = np.stack(np.gradient(s[..., 0])[::-1], -1)               # (x, y, z)
            gl = np.linalg.norm(g, axis=-1, keepdims=True)
            outward = -g / np.maximum(gl, 1e-5)
            strength = np.clip(gl / 0.15, 0.0, 1.0)                      # centrado = metade da diferença de 1 texel
            rgba = np.concatenate([outward * 0.5 + 0.5, strength], -1)
            nrm[q * res:(q + 1) * res] = (rgba[..., [2, 1, 0, 3]] * 255 + 0.5).astype(np.uint8)   # BGRA
            # altura média da fuligem (m locais, desde o chão): é por onde o avião e o
            # míssil passam (battlesmoke.js smokeHeightAt fazia isso na grade do campo)
            col = (a[..., 0] ** 2 * 4.0).sum(axis=(1, 2))           # [z]
            heights.append(float((col * zc).sum() / max(col.sum(), 1e-6)))
        out.tofile(os.path.join(OUT, 'v%d.bin' % v))
        nrm.tofile(os.path.join(OUT, 'nrm%d.bin' % v))
        info['soot_height_m_v%d' % v] = heights
        print('variant', v, 'frames', frames, 'ok', 'soot height', round(heights[0], 1), '->', round(heights[-1], 1))
    with open(os.path.join(OUT, 'velocity.json'), 'w', encoding='utf-8') as f:
        json.dump(info, f)


if __name__ == '__main__':
    main()
