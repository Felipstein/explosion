"""Texturas dos efeitos de look-dev (LookLab, a noite): o tufo de fumaça e a chama, geradas por código.

Rodar (Python do sistema, numpy + pillow; o look.ps1 fxtex chama):
  python Game/Tools/look_fx_textures.py <pasta de saída>

É o jeito dos jogos (11/10): a fumaça é um cartão virado pra câmera com um tufo de nuvem
desenhado e o relevo dele guardado na textura, pra a luz da cena acender o lado certo (o
"normal-mapped smoke sprite" de Killzone e Battlefield; o 6-way lightmap da Unity/CoD é o
mesmo princípio com mais direções). O fogo é a temperatura em tons de cinza e o material
pinta pela rampa do corpo negro (o "temperature map" que o EmberGen exporta).
Nada aqui é a explosão final: isso é das sessões de explosão.

Saída (atlas 2×2, 512 por quadro):
  T_LL_Puff.png   RGBA linear: R = densidade, G/B = a normal do tufo (x, y), A = espessura
  T_LL_Flame.png  RGBA linear: R = temperatura das línguas de fogo, G = a bola de fogo, B/A livres
"""
import os
import sys

import numpy as np
from PIL import Image

C = 512
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.getcwd(), 'textures')
os.makedirs(OUT, exist_ok=True)
FREQ = np.fft.fftfreq(C) * C
v_, u_ = np.mgrid[0:C, 0:C].astype(np.float32)
U = (u_ + 0.5) / C * 2 - 1          # −1..1, x pra direita
V = 1 - (v_ + 0.5) / C * 2          # −1..1, y pra cima


def noise(scale_px, seed):
    """Ruído suave periódico 0..1 (branco filtrado por gaussiana)."""
    r = np.random.default_rng(seed).standard_normal((C, C)).astype(np.float32)
    fx, fy = np.meshgrid(FREQ, FREQ)
    g = np.exp(-(fx ** 2 + fy ** 2) / (2 * (C / scale_px) ** 2))
    out = np.real(np.fft.ifft2(np.fft.fft2(r) * g)).astype(np.float32)
    out -= out.min()
    return out / max(out.max(), 1e-6)


def fbm(seed, base=40.0, octaves=5):
    out, amp, tot = 0.0, 1.0, 0.0
    for o in range(octaves):
        out = out + amp * noise(base / 2 ** o, seed + 17 * o)
        tot += amp
        amp *= 0.5
    return out / tot


def smooth(a, e0, e1):
    t = np.clip((a - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


def puff(seed):
    """Um tufo: bolhas (a couve-flor da fumaça) somadas, comidas na borda pelo ruído.
    A normal sai só das bolhas e do ruído grande (o volume do tufo); o ruído médio só rói a borda."""
    r = np.random.default_rng(seed)
    S = np.zeros((C, C), np.float32)
    n = r.integers(6, 10)
    K = 0.035                                                           # a união macia (sem vinco entre as bolhas)
    for i in range(n):
        a = r.uniform(0, 2 * np.pi)
        d = r.uniform(0.05, 0.38) if i else 0.0
        cx, cy = d * np.cos(a), d * np.sin(a) * 0.85
        rad = r.uniform(0.24, 0.4) * (1.2 if i == 0 else 1.0)
        q = 1 - ((U - cx) ** 2 + (V - cy) ** 2) / rad ** 2
        S += np.exp(np.sqrt(np.clip(q, 0, 1)) * rad / K) * (q > -0.6)
    H = np.clip(K * np.log(np.maximum(S, 1.0)), 0, None)
    big = fbm(seed + 100, 260.0, 3)
    mid = fbm(seed + 200, 90.0, 3)
    Hs = H + (big - 0.5) * 0.12                                         # o volume (a normal sai daqui)
    He = Hs + (mid - 0.5) * 0.12                                        # a borda roída
    dens = smooth(He, -0.01, 0.3) * (0.5 + 0.5 * smooth(mid, 0.3, 0.7))
    gy, gx = np.gradient(Hs * 1.6)
    nx, ny = -gx * C / 2, gy * C / 2
    k = 1 / np.sqrt(nx ** 2 + ny ** 2 + 1)
    return dens, nx * k, ny * k, np.clip(Hs / 0.42, 0, 1)


def stretched(scale_px, seed, sy):
    """Ruído esticado na vertical (sy > 1: as línguas de fogo sobem)."""
    n = noise(scale_px, seed)
    yy = ((v_ / sy) % C).astype(int)
    return n[yy, u_.astype(int)]


def flame(seed):
    """Línguas de fogo: a gota esticada pra cima, entortada pelo ruído, que se parte em pontas no alto."""
    y = (V + 1) / 2                                  # 0 na base, 1 no topo
    w1 = noise(380.0, seed + 1)
    w2 = noise(140.0, seed + 2)
    xw = U + (w1 - 0.5) * 0.9 * y + (w2 - 0.5) * 0.3 * y
    width = 0.5 * np.clip(1 - y, 0, 1) ** 0.5 * (0.75 + 0.5 * noise(300.0, seed + 3))
    body = np.clip(1 - np.abs(xw) / np.maximum(width, 1e-3), 0, 1)
    tongues = smooth(stretched(110.0, seed + 4, 3.0) * 0.9 + (1 - y) * 1.0, 0.6, 0.95)
    base = smooth(y, 0.0, 0.1)
    inner = smooth(stretched(70.0, seed + 5, 2.5), 0.25, 0.75)
    return np.clip(body ** 0.7 * tongues * base * (0.7 + 0.3 * inner) * (0.7 + 0.4 * (1 - y)), 0, 1)


def fireball(seed):
    """A bola de fogo: o tufo com a temperatura caindo do miolo pra borda e manchas de fuligem grandes."""
    dens, _, _, h = puff(seed)
    soot = fbm(seed + 300, 160.0, 3)
    return np.clip(h ** 0.6 * dens * (0.45 + 0.7 * smooth(soot, 0.3, 0.7)), 0, 1)


def atlas(cells):
    out = np.zeros((2 * C, 2 * C) + cells[0].shape[2:], np.float32)
    for i, c in enumerate(cells):
        y0, x0 = (i // 2) * C, (i % 2) * C
        out[y0:y0 + C, x0:x0 + C] = c
    return out


def main():
    p = [puff(s) for s in (3, 11, 29, 41)]
    puffs = atlas([np.dstack([d, nx * 0.5 + 0.5, ny * 0.5 + 0.5, h]) for d, nx, ny, h in p])
    Image.fromarray((np.clip(puffs, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGBA').save(os.path.join(OUT, 'T_LL_Puff.png'))
    f = [np.dstack([flame(s), fireball(s + 50), np.zeros((C, C)), np.ones((C, C))]) for s in (5, 13, 23, 37)]
    Image.fromarray((np.clip(atlas(f), 0, 1) * 255 + 0.5).astype(np.uint8), 'RGBA').save(os.path.join(OUT, 'T_LL_Flame.png'))
    print('fx textures', OUT)


main()
