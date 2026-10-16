"""A ondulação da areia v3 (16/10, docs/CHAO_V3_ESTUDO.md §4 e §5.1): a luz do sol sobre a ondulação, calculada uma vez.

Rodar (Python do sistema, numpy + pillow):
  python Game/Tools/look_ripples_v3.py <pasta de saída> <azimute do sol (graus, 0 = +X da Unreal)> <altura do sol> <wind_yaw>

Na partida o sol não anda, então a luz da ondulação vira uma textura LINEAR com mips (a média de luz é a média certa):
  RippleLit = max(0, N·L) × V ÷ sen(altura)   (1 = igual ao chão liso; 0 = sombra; > 1 = rampa virada pro sol)
De perto a mip 0 desenha a crista acesa e a sombra; de longe sobra o tom (a areia ondulada um pouco mais escura).
Isso troca o que falhou na v2: o corte pelo fwidth apagava a onda (o "chão liso") e a normal menor que o pixel piscava
na luz de 8°. No material ela entra pela normal de iluminação (§4.3), nunca escurecendo a cor.

Duas escalas no mesmo ladrilho de 10 m, no espaço do vento (o vento ao longo de +X do ladrilho, como a v2):
- a física de ~22 cm (a simulação de Nishimori & Ouchi da v2, T_LL_Ripples.png, altura 1,25 cm);
- a de jogo, 0,7 a 1,4 m em duas oitavas (as megaondulações, altura 1/18 da onda), que lê no pixel de 7–12 cm do jogo:
  crista assimétrica (barlavento suave, sotavento íngreme), ondulada e interrompida. É decisão do Felipe (§8.1): sai
  também a versão só física pro A/B.
Saídas: T_LL_RippleLit_patch.png (a proposta: a física em manchas, como o game shot), T_LL_RippleLit_phys.png e
T_LL_RippleLit_game.png (16 bits, RippleLit ÷ 1,25), mais as prévias reduzidas ao
pixel do jogo (o que a câmera vai ver: 1,2 / 4,9 / 7,3 / 12 cm por pixel).
"""
import os
import sys

import numpy as np
from PIL import Image

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.getcwd(), 'textures')
SUN_AZ = float(sys.argv[2]) if len(sys.argv) > 2 else 87.0      # o mundo: o sol do desert_golden_dust na câmera inicial (giro 45, −42)
SUN_EL = float(sys.argv[3]) if len(sys.argv) > 3 else 8.0
WIND = float(sys.argv[4]) if len(sys.argv) > 4 else -82.9       # o World_TestSalt2
TILE_M = 10.0
N = 2048                     # 0,49 cm por texel
LIT_MAX = 1.25               # a escala da gravação (o claro passa de 1; o limite de arte ~1,15 fica no material)
CLAMP = 1.15                 # o limite do lado aceso (a 8° a rampa passa de 1,4; o game shot fica em ~1,13)
HERE = os.path.dirname(os.path.abspath(__file__))
PHYS_PNG = os.path.join(os.path.dirname(os.path.dirname(HERE)), 'captures', 'look', 'textures', 'T_LL_Ripples.png')


def fourier_up(a, n):
    """Interpolação periódica (sem costura) por Fourier."""
    m = a.shape[0]
    F = np.fft.fftshift(np.fft.fft2(a))
    G = np.zeros((n, n), complex)
    o = (n - m) // 2
    G[o:o + m, o:o + m] = F
    return np.real(np.fft.ifft2(np.fft.ifftshift(G))) * (n / m) ** 2


def periodic_noise(n, kmax, seed):
    """Ruído suave e periódico no ladrilho: senos com frequências inteiras até kmax (emenda sem costura)."""
    r = np.random.default_rng(seed)
    y, x = np.mgrid[0:n, 0:n].astype(np.float32) / n
    out = np.zeros((n, n), np.float32)
    for kx in range(-kmax, kmax + 1):
        for ky in range(0, kmax + 1):
            if (kx, ky) == (0, 0) or (ky == 0 and kx < 0):
                continue
            k = np.hypot(kx, ky)
            a = r.normal() / (k ** 1.5)
            out += a * np.cos(2 * np.pi * (kx * x + ky * y) + r.uniform(0, 2 * np.pi))
    return out / (out.std() + 1e-9)


def game_ripples(n):
    """As megaondulações: duas oitavas com onda inteira no ladrilho (8 e 13 por 10 m = 1,25 e 0,77 m)."""
    y, x = np.mgrid[0:n, 0:n].astype(np.float32) / n          # 0..1 no ladrilho; x = ao longo do vento
    h = np.zeros((n, n), np.float32)
    for waves, seed, weight in ((8, 11, 1.0), (13, 23, 0.55)):
        lam = TILE_M / waves
        warp = periodic_noise(n, 3, seed) * 0.35 + periodic_noise(n, 7, seed + 1) * 0.08    # a crista ondulada
        ph = (x * waves + warp) % 1.0
        # o perfil assimétrico: sobe devagar (barlavento, 70% da onda) e cai rápido (sotavento, 30%)
        prof = np.where(ph < 0.7, ph / 0.7, 1.0 - (ph - 0.7) / 0.3)
        prof = prof * prof * (3 - 2 * prof)                    # sem quina
        broken = np.clip(periodic_noise(n, 5, seed + 2) * 0.6 + 0.55, 0.0, 1.0)              # interrompida
        h += weight * (lam / 18.0) * prof * broken
    return h - h.mean()


def patchy(phys, n):
    """A física em manchas (16/10, medido no game shot r1 na mesma escala): a ondulação do alvo tem ~30 cm, mas aparece em
    campos espalhados, irregulares e baixos, com muita areia lisa entre eles; a nossa contínua virava veludo cotelê.
    A altura varia de 0 a ~1,1 cm por um ruído de 2–5 m (os campos), e o veio do vento quebra as cristas."""
    # as cristas curvas: o campo da simulação entortado por um ruído de 1–3 m (±25 cm), periódico (sem costura)
    ys, xs = np.mgrid[0:n, 0:n].astype(np.float32)
    w = 0.25 / (TILE_M / n)
    phys = bilinear(phys, xs + periodic_noise(n, 6, 41) * w, ys + periodic_noise(n, 6, 42) * w * 0.6)
    field = periodic_noise(n, 4, 31) * 0.8 + periodic_noise(n, 9, 32) * 0.35
    amp = np.clip((field - 0.15) * 0.9, 0.0, 1.0) ** 1.3          # ~40% liso
    broken = np.clip(periodic_noise(n, 16, 33) * 0.7 + 0.75, 0.0, 1.0)
    return phys * (0.85 * amp * broken)


def bilinear(h, x, y):
    n = h.shape[0]
    x0, y0 = np.floor(x).astype(np.int64), np.floor(y).astype(np.int64)
    fx, fy = (x - x0).astype(np.float32), (y - y0).astype(np.float32)
    a = h[y0 % n, x0 % n]; b = h[y0 % n, (x0 + 1) % n]
    c = h[(y0 + 1) % n, x0 % n]; d = h[(y0 + 1) % n, (x0 + 1) % n]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def sun_lit(hm, px_m, az_tile, el):
    """RippleLit pelo horizonte na direção do sol, com a penumbra do disco do sol (0,53°)."""
    n = hm.shape[0]
    dx, dy = np.cos(az_tile), np.sin(az_tile)
    ys, xs = np.mgrid[0:n, 0:n].astype(np.float32)
    rng = hm.max() - hm.min()
    steps = int(min(rng / np.tan(el) / px_m + 2, n // 2))       # a sombra mais comprida possível
    vis = np.ones_like(hm)
    tan_el, tan_pen = np.tan(el), np.tan(np.radians(0.53))
    for k in range(1, steps + 1):
        hk = bilinear(hm, xs + dx * k, ys + dy * k)
        d = k * px_m
        over = (hk - hm - d * tan_el) / (d * tan_pen + 1e-4)      # quanto do disco do sol o relevo tapa
        vis = np.minimum(vis, np.clip(0.5 - over, 0.0, 1.0))
    gx = (np.roll(hm, -1, 1) - np.roll(hm, 1, 1)) / (2 * px_m)
    gy = (np.roll(hm, -1, 0) - np.roll(hm, 1, 0)) / (2 * px_m)
    k = 1 / np.sqrt(gx ** 2 + gy ** 2 + 1)
    nx, ny, nz = -gx * k, -gy * k, k
    L = np.array([np.cos(el) * dx, np.cos(el) * dy, np.sin(el)])
    ndl = np.maximum(0.0, nx * L[0] + ny * L[1] + nz * L[2])
    return np.clip(ndl * vis / np.sin(el), 0.0, CLAMP)


def preview(lit, name, cm_per_px):
    """O que a câmera vê: a média de luz reduzida ao pixel do jogo (a mip certa), num recorte de 4 m, ampliado."""
    n = lit.shape[0]
    f = max(1, int(round(cm_per_px / (TILE_M * 100 / n))))
    m = n // f * f
    small = lit[:m, :m].reshape(m // f, f, m // f, f).mean(axis=(1, 3))
    crop = small[: int(400 / cm_per_px), : int(400 / cm_per_px)]
    img = Image.fromarray((np.clip(crop / 1.25, 0, 1) ** (1 / 2.2) * 255).astype(np.uint8))
    return img.resize((400, 400), Image.NEAREST), small.std()


def main():
    os.makedirs(OUT, exist_ok=True)
    px = TILE_M / N
    phys = np.asarray(Image.open(PHYS_PNG)).astype(np.float32)[..., 0] / 255.0
    phys = fourier_up(phys - phys.mean(), N) * 0.0125                   # 1,25 cm de crista a cavado (a v2)
    game = game_ripples(N)
    az_tile = np.radians(SUN_AZ - WIND)       # o sol no espaço do ladrilho (o vento é o +X dele)
    el = np.radians(SUN_EL)
    print(f'LOOKRIPPLES3 sol {SUN_AZ:.1f}°/{SUN_EL:.1f}°, vento {WIND:.1f}°: o sol a {np.degrees(az_tile) % 360:.0f}° do vento; '
          f'megaondulação ±{100 * game.std():.1f} cm (pico a pico {100 * (game.max() - game.min()):.1f})', flush=True)
    sheet = []
    for name, hm in (('patch', patchy(phys, N)), ('phys', phys), ('game', game + phys)):
        lit = sun_lit(hm, px, az_tile, el)
        g16 = (np.clip(lit / LIT_MAX, 0, 1) * 65535 + 0.5).astype(np.uint16)
        Image.fromarray(g16, 'I;16').save(os.path.join(OUT, f'T_LL_RippleLit_{name}.png'))
        row = []
        for cm in (1.2, 4.9, 7.3, 12.0):
            img, sd = preview(lit, name, cm)
            row.append(img)
            print(f'  {name}: a {cm:4.1f} cm/px a luz varia ±{100 * sd:.1f}% (média {lit.mean():.3f}, sombra {100 * (lit < 0.3).mean():.0f}%)', flush=True)
        sheet.append(row)
    W = Image.new('L', (4 * 410, len(sheet) * 410), 20)
    for j, row in enumerate(sheet):
        for i, img in enumerate(row):
            W.paste(img, (i * 410, j * 410))
    W.save(os.path.join(OUT, 'ripplelit_preview.png'))
    print('LOOKRIPPLES3 ok', OUT)


main()
