"""Textura de UM elo da esteira (cor + normal), procedural, feita por nós.

Uso: python BlastLab/Tools/make_track_link.py [saida_dir]
     (padrão: captures/concept/31_manual_parts/usa_tank_v2)

U atravessa a largura do elo e V corre ao longo dele (1 elo = 0..1), como as
UVs de blender_track.py. O desenho segue a ficha do tanque USA: sapata de aço,
garra transversal no meio, dois blocos de borracha, guia central e pinos nas
pontas; areia acumulada nos vãos. A normal (espaço tangente, +Y do OpenGL) sai
da mesma altura.
"""
import os
import sys

import numpy as np
from PIL import Image

REPO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, 'captures', 'concept', '31_manual_parts', 'usa_tank_v2')
W = H = 256


def smooth_noise(scale, seed):
    r = np.random.default_rng(seed).random((H // scale + 2, W // scale + 2))
    im = Image.fromarray((r * 255).astype(np.uint8)).resize((W, H), Image.BICUBIC)
    return np.asarray(im).astype(np.float32) / 255


u = (np.arange(W) + 0.5) / W
v = (np.arange(H) + 0.5) / H
U, V = np.meshgrid(u, v)
n1, n2 = smooth_noise(8, 1), smooth_noise(32, 2)
# altura: sapata, garra, blocos de borracha, guia e pinos
h = np.full((H, W), 0.35)
h += 0.45 * np.exp(-((V - 0.5) / 0.09) ** 2) * (np.abs(U - 0.5) < 0.47)
pads = ((np.abs(U - 0.25) < 0.17) | (np.abs(U - 0.75) < 0.17)) & (np.abs(V - 0.5) < 0.36)
h = np.where(pads, np.maximum(h, 0.6), h)
h = np.where((np.abs(U - 0.5) < 0.05) & (np.abs(V - 0.5) < 0.3), 0.9, h)
h = np.where(np.minimum(V, 1 - V) < 0.06, 0.25 + 0.15 * np.cos((U * 14) * np.pi) ** 2, h)
h += 0.04 * (n1 - 0.5)
# cor: aço escuro gasto, borracha preta, topo da garra polido, areia nos vãos
steel, rubber, sand = np.array([0.20, 0.19, 0.18]), np.array([0.055, 0.055, 0.06]), np.array([0.55, 0.43, 0.30])
col = np.where(pads[..., None], rubber, steel) * (0.8 + 0.4 * n1[..., None])
col = col + (np.clip((h - 0.7) * 3, 0, 1) * (np.abs(V - 0.5) < 0.12))[..., None] * 0.18
dust = np.clip((0.5 - h) * 2.2 + (n2 - 0.5) * 0.8, 0, 1) * 0.75
col = col * (1 - dust[..., None]) + sand * dust[..., None]
gy, gx = np.gradient(h * 6.0)
nrm = np.stack([-gx, gy, np.ones_like(h)], -1)
nrm = nrm / np.linalg.norm(nrm, axis=-1, keepdims=True) * 0.5 + 0.5
os.makedirs(OUT, exist_ok=True)
alb = (np.clip(col, 0, 1) ** (1 / 2.2) * 255).astype(np.uint8)
Image.fromarray(alb).save(os.path.join(OUT, 'track_link_albedo.png'))
Image.fromarray((nrm * 255).astype(np.uint8)).save(os.path.join(OUT, 'track_link_normal.png'))
Image.fromarray(np.vstack([alb] * 6)).resize((128, 384)).save(os.path.join(OUT, 'track_link_preview.png'))
print('track link ->', OUT)
