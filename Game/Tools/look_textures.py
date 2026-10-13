"""Texturas do chão do look (LookLab), geradas por código: o asfalto rachado das artes.

Rodar (Python do sistema, numpy + pillow; o look.ps1 textures chama):
  python Game/Tools/look_textures.py <pasta de saída>

O alvo são as quatro cenas de docs/concept/scenes/desert_golden_hour/best/: o asfalto
velho do deserto rachado em placas. Medido nelas (o tanque de 7,9 m como régua):
- placas de 1 a 2 m (um tanque cobre umas 5 a 8), as rachaduras de bloco;
- manchas de "couro de jacaré" (a fadiga do pavimento, placas de 30 a 60 cm) em parte da rua;
- a fresta tem de 4 a 8 cm, escura (sombra dentro dela), muitas vezes com areia na borda;
- trechos quase sem rachadura entre as manchas.

Como é feito (tudo periódico: o ladrilho de 12 m repete sem emenda):
- Voronoi em grade com um ponto sorteado por célula (busca 3×3, com a volta do ladrilho);
  a fresta é onde a distância ao 1º e ao 2º ponto quase empata (F2 − F1 pequeno);
- o espaço é entortado por ruído antes (a rachadura de verdade não é reta);
- a largura da fresta varia e ela some em trechos (a rachadura nasce e morre);
- três escalas: blocos (7 por ladrilho), jacaré (24, só nas manchas) e fio de cabelo (52, fraco);
- a areia: dentro e em volta das frestas (a fresta segura a areia que o vento traz) e em
  manchas largas;
- a placa tem um tom próprio (cada uma envelheceu de um jeito) e a borda levanta um pouco.

Saída (2048², ladrilho de 12 m, ~6 mm por pixel):
  T_LL_Cracks.png    RGBA linear: R = fresta, G = areia, B = tom da placa, A = altura
  T_LL_Cracks_N.png  normal DirectX (o padrão da Unreal), tirada da altura
"""
import os
import sys

import numpy as np
from PIL import Image

N = 2048
TILE_M = 12.0
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.getcwd(), 'textures')
os.makedirs(OUT, exist_ok=True)
rng = np.random.default_rng(7)
FREQ = np.fft.fftfreq(N) * N


def noise(scale_px, seed):
    """Ruído periódico suave (branco filtrado por uma gaussiana no espaço de Fourier), 0..1."""
    r = np.random.default_rng(seed).standard_normal((N, N)).astype(np.float32)
    fx, fy = np.meshgrid(FREQ, FREQ)
    sigma = N / scale_px
    g = np.exp(-(fx ** 2 + fy ** 2) / (2 * sigma ** 2))
    out = np.real(np.fft.ifft2(np.fft.fft2(r) * g)).astype(np.float32)
    out -= out.min()
    return out / max(out.max(), 1e-6)


def streaks(scale_px, stretch, seed):
    """Ruído esticado na direção do vento (X do ladrilho): as faixas de areia soprada, 0..1."""
    r = np.random.default_rng(seed).standard_normal((N, N)).astype(np.float32)
    fx, fy = np.meshgrid(FREQ, FREQ)
    sx, sy = N / (scale_px * stretch), N / scale_px
    g = np.exp(-(fx ** 2 / (2 * sx ** 2) + fy ** 2 / (2 * sy ** 2)))
    out = np.real(np.fft.ifft2(np.fft.fft2(r) * g)).astype(np.float32)
    out -= out.min()
    return out / max(out.max(), 1e-6)


def blur(img, px):
    fx, fy = np.meshgrid(FREQ, FREQ)
    g = np.exp(-(fx ** 2 + fy ** 2) * (2 * np.pi ** 2) * (px / N) ** 2)
    return np.real(np.fft.ifft2(np.fft.fft2(img) * g)).astype(np.float32)


def voronoi(G, u, v, seed):
    """F1, F2 (em fração do ladrilho) e o índice da célula mais perto; grade G×G periódica."""
    r = np.random.default_rng(seed)
    pts = (r.random((G, G, 2)) * 0.8 + 0.1).astype(np.float32)     # ponto dentro de cada célula
    ids = r.random((G, G)).astype(np.float32)
    cu, cv = np.floor(u * G).astype(np.int32), np.floor(v * G).astype(np.int32)
    f1 = np.full(u.shape, 9.0, np.float32)
    f2 = np.full(u.shape, 9.0, np.float32)
    idn = np.zeros(u.shape, np.float32)
    for di in (-1, 0, 1):
        for dj in (-1, 0, 1):
            ci, cj = cu + di, cv + dj
            wi, wj = ci % G, cj % G
            px = (ci + pts[wj, wi, 0]) / G
            py = (cj + pts[wj, wi, 1]) / G
            d = np.sqrt((u - px) ** 2 + (v - py) ** 2)
            closer = d < f1
            f2 = np.where(closer, f1, np.minimum(f2, d))
            idn = np.where(closer, ids[wj, wi], idn)
            f1 = np.where(closer, d, f1)
    return f1, f2, idn


def main():
    y, x = np.mgrid[0:N, 0:N].astype(np.float32) / N
    # o espaço entortado: a rachadura serpenteia (duas escalas de ruído)
    w1x, w1y = noise(140, 1) - 0.5, noise(140, 2) - 0.5
    w2x, w2y = noise(40, 3) - 0.5, noise(40, 4) - 0.5
    u = (x + 0.035 * w1x + 0.008 * w2x) % 1.0
    v = (y + 0.035 * w1y + 0.008 * w2y) % 1.0
    m = 1.0 / TILE_M            # 1 m em fração do ladrilho

    def crack(G, seed, width_m, gaps, wvar_scale):
        f1, f2, idn = voronoi(G, u, v, seed)
        e = f2 - f1
        wv = 0.55 + 0.9 * noise(wvar_scale, seed + 10)                    # a largura muda ao longo
        w = width_m * m * wv
        c = 1.0 - np.clip((e - w * 0.35) / (w * 0.65 + 1e-6), 0, 1)       # miolo escuro, borda macia
        alive = np.clip((noise(220, seed + 20) - gaps) / 0.12, 0, 1)        # a rachadura nasce e morre
        return c * alive, idn, f1

    block, plate_id, f1b = crack(7, 11, 0.065, 0.25, 90)
    gator, _, _ = crack(24, 12, 0.035, 0.18, 60)
    hair, _, _ = crack(52, 13, 0.016, 0.45, 40)
    patch = np.clip((noise(420, 30) - 0.48) / 0.12, 0, 1)                   # as manchas de jacaré (~35%)
    cr = np.clip(np.maximum.reduce([block, gator * patch, hair * 0.45 * (0.3 + 0.7 * patch)]), 0, 1)

    # a areia: em parte das frestas (não em todas: a fresta continua escura) e em faixas de vento
    halo = np.clip(blur(cr, 5) * 2.0, 0, 1)
    fine = noise(25, 41)
    wind = np.clip((streaks(420, 5.0, 40) - 0.62) / 0.2, 0, 1)      # poucas faixas largas, não um padrão
    sand = np.clip(0.35 * cr * (fine > 0.55) + 0.12 * halo * fine + 0.3 * wind * (0.45 + 0.55 * fine), 0, 1)

    # o tom de cada placa e a altura: placa um pouco abaulada, borda levantada, fresta funda
    tone = 0.5 + 0.35 * (plate_id - 0.5) + 0.15 * (noise(60, 50) - 0.5)
    lip = np.clip(blur(cr, 3) * 2.0, 0, 1) - cr
    h = 0.55 + 0.12 * (noise(70, 60) - 0.5) + 0.18 * lip - 0.55 * cr - 0.08 * sand
    h = np.clip(h, 0, 1)

    rgba = np.stack([cr, sand, np.clip(tone, 0, 1), h], -1)
    Image.fromarray((rgba * 255 + 0.5).astype(np.uint8), 'RGBA').save(os.path.join(OUT, 'T_LL_Cracks.png'))

    # normal da altura (DirectX: o verde aponta pra baixo da imagem). Profundidade da fresta ~2 cm.
    depth_m = 0.02
    px_m = TILE_M / N
    dhdx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * depth_m / (2 * px_m)
    dhdy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * depth_m / (2 * px_m)
    nrm = np.stack([-dhdx, -dhdy, np.ones_like(h)], -1)
    nrm /= np.linalg.norm(nrm, axis=-1, keepdims=True)
    Image.fromarray(((nrm * 0.5 + 0.5) * 255 + 0.5).astype(np.uint8), 'RGB').save(os.path.join(OUT, 'T_LL_Cracks_N.png'))
    print('LOOKTEX ok', OUT, f'fresta média {cr.mean():.3f}, areia {sand.mean():.3f}')


main()
