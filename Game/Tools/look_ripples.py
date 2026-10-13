"""As ondulações de vento da areia, simuladas (12/10): o modelo de Nishimori & Ouchi (1993, Phys. Rev. Lett. 71, 197).

Rodar (Python do sistema, numpy + pillow):
  python Game/Tools/look_ripples.py <pasta de saída> [passos]

O vento salta grãos (saltação: o grão sai de cada ponto e cai L adiante, mais longe se o ponto é alto) e
o rastejamento alisa (os grãos rolam pro vizinho mais baixo). Disso nascem as ondulações com o lado de onde
vem o vento suave e o outro íngreme, as bifurcações e os defeitos de verdade, que um seno no shader não tem.
O grade é periódica: o mapa emenda sem costura.

Saída: T_LL_Ripples.png (RGBA linear, 1024², o ladrilho de TILE_M m, com o vento ao longo de +X do ladrilho):
  R = altura (0..1), G/B = a normal (x, y) do relevo na escala real, A = a visibilidade do sol ASSADA (macia):
  o sol é fixo por mapa, então a sombra das cristas vai pronta na textura (com mips, sem cintilar; a 2ª opinião
  de 12/10: nada de ray-march por pixel). O sol entra relativo ao vento: azimute (graus a partir de +X) e altura.

  python Game/Tools/look_ripples.py <pasta> [passos] [azimute do sol rel. ao vento] [altura do sol]
"""
import os
import sys

import numpy as np
from PIL import Image

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.getcwd(), 'textures')
STEPS = int(sys.argv[2]) if len(sys.argv) > 2 else 2500
SUN_AZ = float(sys.argv[3]) if len(sys.argv) > 3 else -25.0
SUN_EL = float(sys.argv[4]) if len(sys.argv) > 4 else 15.0
TILE_M = 10.0               # o ladrilho no mundo: a simulação dá ~8,5 cm de onda em 4 m; em 10 m vira ~21 cm (o que lê de cima)
AMP_M = 0.0125              # a altura de crista a cavado: o índice de ondulação ~17 (comprimento/altura, medido em campo)
N = 512                     # a simulação (4 m: 7,8 mm por célula); a saída sobe pra 1024


def simulate(seed, steps):
    r = np.random.default_rng(seed)
    h = r.normal(0.0, 0.05, (N, N)).astype(np.float32)
    q, L0, b, D = 0.1, 6.0, 4.0, 0.2          # quanto salta, o salto base (células), quanto a altura aumenta o salto, o rastejamento
    ys, xs = np.mgrid[0:N, 0:N]
    for i in range(steps):
        # o vento vem de −X: o salto anda +X, com um leve desvio em Y que muda devagar (o vento nunca é reto)
        L = L0 + b * h
        dy = 0.15 * np.sin(2 * np.pi * (xs / N) * 2 + i * 0.002)
        tx = xs + L
        ty = ys + dy
        x0 = np.floor(tx).astype(np.int64)
        fx = (tx - x0).astype(np.float32)
        y0 = np.rint(ty).astype(np.int64) % N
        h = h - q
        out = np.zeros_like(h)
        np.add.at(out, (y0, x0 % N), q * (1 - fx))
        np.add.at(out, (y0, (x0 + 1) % N), q * fx)
        h = h + out
        # o rastejamento (os 4 vizinhos e os 4 da diagonal, como no artigo)
        nn = np.roll(h, 1, 0) + np.roll(h, -1, 0) + np.roll(h, 1, 1) + np.roll(h, -1, 1)
        nd = (np.roll(np.roll(h, 1, 0), 1, 1) + np.roll(np.roll(h, 1, 0), -1, 1)
              + np.roll(np.roll(h, -1, 0), 1, 1) + np.roll(np.roll(h, -1, 0), -1, 1))
        h = h + D * (nn / 6.0 + nd / 12.0 - h)
        if i % 500 == 0:
            print(f'  passo {i}: desvio {h.std():.3f}', flush=True)
    return h


def wavelength(h):
    f = np.abs(np.fft.fft2(h - h.mean()))
    f[0, 0] = 0
    ky, kx = np.unravel_index(np.argmax(f[: N // 2, : N // 2]), (N // 2, N // 2))
    k = max(np.hypot(kx, ky), 1e-6)
    return N / k * (4.0 / N)


def norm01(h):
    lo, hi = np.percentile(h, [0.5, 99.5])
    return np.clip((h - lo) / (hi - lo), 0, 1)


def up2(a):
    """De 512 pra 1024 com interpolação periódica (Fourier: emenda sem costura)."""
    F = np.fft.fftshift(np.fft.fft2(a))
    G = np.zeros((2 * N, 2 * N), complex)
    G[N // 2: N // 2 + N, N // 2: N // 2 + N] = F
    return np.real(np.fft.ifft2(np.fft.ifftshift(G))) * 4


def bilinear(h, x, y):
    n = h.shape[0]
    x0, y0 = np.floor(x).astype(np.int64), np.floor(y).astype(np.int64)
    fx, fy = x - x0, y - y0
    a = h[y0 % n, x0 % n]; b = h[y0 % n, (x0 + 1) % n]
    c = h[(y0 + 1) % n, x0 % n]; d = h[(y0 + 1) % n, (x0 + 1) % n]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def sun_visibility(hm, px_m):
    """A visibilidade do sol macia: anda até ~30 cm na direção do sol; a sombra é o quanto o relevo passa da linha do raio
    (penumbra de ~1 mm de folga), o que dá a borda macia e os mips limpos."""
    n = hm.shape[0]
    az, el = np.radians(SUN_AZ), np.radians(SUN_EL)
    dx, dy = np.cos(az), np.sin(az)
    ys, xs = np.mgrid[0:n, 0:n].astype(np.float32)
    occ = np.zeros_like(hm)
    steps = int(0.30 / px_m)
    for k in range(1, steps + 1, 2):
        hk = bilinear(hm, xs + dx * k, ys + dy * k)
        rise = k * px_m * np.tan(el)
        occ = np.maximum(occ, (hk - hm - rise) / 0.001)
    return np.clip(1.0 - occ, 0.0, 1.0)


def main():
    os.makedirs(OUT, exist_ok=True)
    a = simulate(7, STEPS)
    print(f'LOOKRIPPLES comprimento de onda: {wavelength(a) * TILE_M / 4.0 * 100:.1f} cm (no ladrilho de {TILE_M:.0f} m)')
    A = norm01(up2(a))
    n = A.shape[0]
    px = TILE_M / n
    hm = A * AMP_M                                     # metros
    gx = (np.roll(hm, -1, 1) - np.roll(hm, 1, 1)) / (2 * px)
    gy = (np.roll(hm, -1, 0) - np.roll(hm, 1, 0)) / (2 * px)
    k = 1 / np.sqrt(gx ** 2 + gy ** 2 + 1)
    nx, ny = -gx * k, -gy * k                          # a normal do relevo (y da imagem pra baixo = +Y do ladrilho)
    vis = sun_visibility(hm, px)
    print(f'LOOKRIPPLES inclinação máx {np.degrees(np.arctan(np.hypot(gx, gy).max())):.0f}°, sombra em {100 * (vis < 0.5).mean():.0f}% '
          f'(sol {SUN_AZ:.0f}° do vento, {SUN_EL:.0f}° de altura)')
    rgba = np.dstack([A, nx * 0.5 + 0.5, ny * 0.5 + 0.5, vis])
    Image.fromarray((rgba * 255 + 0.5).astype(np.uint8), 'RGBA').save(os.path.join(OUT, 'T_LL_Ripples.png'))
    sh = np.clip(0.35 + 0.65 * (nx * np.cos(np.radians(SUN_AZ)) * np.cos(np.radians(SUN_EL))
                                  + ny * np.sin(np.radians(SUN_AZ)) * np.cos(np.radians(SUN_EL)) + np.sqrt(np.clip(1 - nx ** 2 - ny ** 2, 0, 1)) * np.sin(np.radians(SUN_EL))) * vis * 2.2, 0, 1)
    Image.fromarray((sh * 255).astype(np.uint8)).resize((512, 512)).save(os.path.join(OUT, 'ripples_preview.png'))
    print('LOOKRIPPLES ok', OUT)


main()
