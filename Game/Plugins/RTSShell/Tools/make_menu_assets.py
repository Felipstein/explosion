# As imagens fixas do menu nativo, com as MESMAS contas do mockup aprovado (docs/concept/ui/menu/menu.html), número por número:
#  - radar_returns.png: o eco do chão do tubo PPI (o relevo do teatro, a sombra do radar atrás dos morros, o grão, a vila), como
#    o bakeReturns() desenha: cada célula polar é um retângulo girado de alfa s, e as células se sobrepõem por "source-over".
#    Sai em alfa pré-multiplicado (R = G = B = A = cobertura), pra Slate somar luz com alfa zero (o "lighter" do canvas).
#  - backdrop.png e vignette.png: os dois gradientes radiais do CSS (o brilho âmbar do alto e a vinheta), em baixa resolução
#    (gradiente liso não perde nada esticado).
#  - os emblemas dos exércitos em PNG (a Slate não lê webp).
# Rodar: python make_menu_assets.py   (saída em ../Resources/Menu)
import math, os
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, '..', 'Resources', 'Menu'))
os.makedirs(OUT, exist_ok=True)

W, H = 1920, 1080
SCX, SCY, R, RANGE = 1312, 540, 424, 40
KM = R / RANGE
D2R = math.pi / 180
NB, NR = 1440, 300
DR = R / NR


# ---------- o hash do mockup, com a semântica do JavaScript (multiplicação em double, bit a bit em int32) ----------
def toint32(d):
    v = np.trunc(d).astype(np.float64)
    v = np.mod(v, 4294967296.0)
    v = np.where(v >= 2147483648.0, v - 4294967296.0, v)
    return v.astype(np.int64)


def jshash(x, y):
    h = np.asarray(x, np.float64) * 374761393.0 + np.asarray(y, np.float64) * 668265263.0
    t = toint32(h)
    t = t ^ (t >> 13)                                  # int32 com sinal: o >> é aritmético
    h2 = t.astype(np.float64) * 1274126177.0           # double, com o mesmo arredondamento do JS
    u = toint32(h2)
    u = u ^ (u >> 16)
    return (u & 0xffff).astype(np.float64) / 0xffff


def vn(x, y):
    xi, yi = np.floor(x), np.floor(y)
    xf, yf = x - xi, y - yi
    s = lambda t: t * t * (3 - 2 * t)
    a, b = jshash(xi, yi), jshash(xi + 1, yi)
    c, d = jshash(xi, yi + 1), jshash(xi + 1, yi + 1)
    return a + (b - a) * s(xf) + (c - a) * s(yf) + (a - b - c + d) * s(xf) * s(yf)


def fbm(x, y):
    return vn(x, y) * .5 + vn(x * 2.03, y * 2.03) * .25 + vn(x * 4.1, y * 4.1) * .125 + vn(x * 8.3, y * 8.3) * .0625


def polar(brg, r):
    return SCX + np.sin(brg) * r, SCY - np.cos(brg) * r


def river_y(x):
    return 60 + 70 * np.sin(x / 150 + .6) + 20 * np.sin(x / 47)


TOWN = (SCX + math.sin(35 * D2R) * .45 * R, SCY - math.cos(35 * D2R) * .45 * R)


def height(x, y):
    dx, dy = x - SCX, y - SCY
    h = 40 + fbm(dx / 160 + 11, dy / 160 + 3) * 160 + np.maximum(0, fbm(dx / 90 - 4, dy / 90 + 7) - .55) * 900
    dr = np.abs(dy - river_y(dx))
    h = h - 70 * np.exp(-dr * dr / 900)
    dt = np.hypot(x - TOWN[0], y - TOWN[1])
    h = h * (1 - np.exp(-dt * dt / 2500) * .7)
    h = h + 260 * np.exp(-(dx * dx + dy * dy) / 2600)
    return h


# ---------- o eco polar (rumo × distância), com a sombra do radar ----------
def returns_polar():
    h0 = float(height(np.float64(SCX), np.float64(SCY))) + 40
    hc = float(height(np.float64(SCX), np.float64(SCY)))
    b = np.arange(NB, dtype=np.float64)[:, None]
    i = np.arange(1, NR, dtype=np.float64)[None, :]
    brg = b / NB * math.pi * 2
    r = i * DR
    x, y = polar(brg, r)
    h = height(x, y)
    ang = (h - h0) / (r / KM * 1000)
    prevmax = np.concatenate([np.full((NB, 1), -1e9), np.maximum.accumulate(ang, axis=1)[:, :-1]], axis=1)
    vis = ang > prevmax
    prev = np.concatenate([np.full((NB, 1), hc), h[:, :-1]], axis=1)
    slope = (h - prev) / (DR / KM * 1000)
    bb = np.broadcast_to(b, ang.shape)
    ii = np.broadcast_to(i, ang.shape)
    sp = jshash(bb, ii * 7)
    s = np.minimum(1, np.minimum(1, np.maximum(0, (slope - .006) * 34)) * (.2 + .8 * sp ** 3) + np.where(sp > .93, .12, 0)
                   + np.where(jshash(bb * 3, ii) > .992, .7, 0))
    s = np.where(vis, s, 0)
    town = (np.hypot(x - TOWN[0], y - TOWN[1]) < 18) & vis
    s = np.where(town, np.maximum(s, .65 + .35 * jshash(bb, ii)), s)
    near = r < 8
    s = np.where(near, np.maximum(s, .5 - r / 16), s)
    ret = np.zeros((NB, NR))
    ret[:, 1:] = s
    return ret


# ---------- o bakeReturns: cada célula (b, i ≥ 2, s ≥ .06) é um retângulo girado; cobertura por supersampling 4×4 ----------
def bake(ret, ss=4):
    x0, y0 = SCX - R - 2, SCY - R - 2
    size = 2 * R + 4
    acc = np.ones((size, size))                         # o produto de (1 - s·cobertura) de cada pixel
    offs = (np.arange(ss) + .5) / ss
    halfbins = 4
    for row0 in range(0, size, 64):
        rows = np.arange(row0, min(size, row0 + 64))
        py = (y0 + rows[:, None] + offs[None, :]).reshape(-1)              # as amostras em y
        px = (x0 + np.arange(size)[:, None] + offs[None, :]).reshape(-1)  # as amostras em x
        PY, PX = np.meshgrid(py, px, indexing='ij')
        dx, dy = PX - SCX, PY - SCY
        rr = np.hypot(dx, dy)
        th = np.mod(np.arctan2(dx, -dy), 2 * math.pi)
        prod = np.ones_like(rr)
        b0 = np.round(th / (2 * math.pi) * NB).astype(np.int64)
        i0 = np.floor(rr / DR).astype(np.int64)
        for db in range(-halfbins, halfbins + 1):
            bi = np.mod(b0 + db, NB)
            thb = bi / NB * 2 * math.pi
            dlt = th - thb
            dlt = (dlt + math.pi) % (2 * math.pi) - math.pi
            lx = rr * np.sin(dlt)                       # no quadro girado da célula: x de lado, ao longo da distância
            ly = rr * np.cos(dlt)
            for di in (-1, 0, 1):
                ii = i0 + di
                ok = (ii >= 2) & (ii < NR)
                iic = np.clip(ii, 0, NR - 1)
                s = ret[bi, iic]
                ri = iic * DR
                w = np.maximum(1.2, ri * 1.1 * D2R)
                inside = ok & (s >= .06) & (np.abs(lx) <= w / 2) & (ly >= ri - .1 * DR) & (ly <= ri + DR)
                prod = prod * np.where(inside, 1 - np.minimum(1, s), 1)
        prod = prod.reshape(len(rows), ss, size, ss).mean(axis=(1, 3))     # a média das amostras ≈ a cobertura do AA
        acc[rows, :] = prod
    return 1 - acc


def radial(cx, cy, rx, ry, w, h, stops):
    ys, xs = np.mgrid[0:h, 0:w].astype(np.float64)
    t = np.sqrt(((xs + .5) * W / w - cx) ** 2 / rx ** 2 + ((ys + .5) * H / h - cy) ** 2 / ry ** 2)
    return np.interp(t, [s[0] for s in stops], [s[1] for s in stops])


def main():
    ret = returns_polar()
    ret.astype('<f4').tofile(os.path.join(OUT, 'radar_polar.bin'))   # o A-scope do jogo lê o eco ao longo do feixe (1440 × 300 float32)
    cov = bake(ret)
    a = np.clip(np.round(cov * 255), 0, 255).astype(np.uint8)
    Image.fromarray(np.dstack([a, a, a, a]), 'RGBA').save(os.path.join(OUT, 'radar_returns.png'))
    # o brilho do alto: radial-gradient(at 62% 0%, rgba(216,173,78,.07), transparent 60%), elipse até o canto mais longe
    gw, gh = 480, 270
    al = radial(.62 * W, 0, .62 * W * math.sqrt(2), H * math.sqrt(2), gw, gh, [(0, .07), (.6, 0), (9, 0)])
    al8 = np.clip(np.round(al * 255), 0, 255).astype(np.uint8)
    Image.fromarray(np.dstack([np.full_like(al8, 216), np.full_like(al8, 173), np.full_like(al8, 78), al8]), 'RGBA').save(os.path.join(OUT, 'backdrop_glow.png'))
    # a vinheta: radial-gradient(ellipse at 62% 50%, transparent 40%, rgba(0,0,0,.66)), elipse até o canto mais longe
    vi = radial(.62 * W, .5 * H, .62 * W * math.sqrt(2), .5 * H * math.sqrt(2), gw, gh, [(0, 0), (.4, 0), (1, .66), (9, .66)])
    v8 = np.clip(np.round(vi * 255), 0, 255).astype(np.uint8)
    Image.fromarray(np.dstack([np.zeros_like(v8), np.zeros_like(v8), np.zeros_like(v8), v8]), 'RGBA').save(os.path.join(OUT, 'vignette.png'))
    # os emblemas (o IFF do hover do MULTIPLAYER): 56 px, o dobro do desenhado (28), pra ficar nítido
    src = os.path.normpath(os.path.join(HERE, '..', '..', '..', '..', 'docs', 'concept', 'ui', 'menu', 'a'))
    for f in ['usa', 'russia', 'china', 'gla']:
        im = Image.open(os.path.join(src, f + '_emblem.webp')).convert('RGBA').resize((56, 56), Image.LANCZOS)
        im.save(os.path.join(OUT, f'emblem_{f}.png'))
    print('ok', OUT, 'cobertura média %.3f' % cov.mean())


def skirmish():
    """Os dois gradientes do fundo da sala do skirmish (04/11), pela mesma conta: o brilho âmbar no alto à direita,
    radial-gradient(at 66% 10%, rgba(216,173,78,.07), transparent 60%), e a vinheta centrada no mapa,
    radial-gradient(ellipse at 66% 50%, transparent 50%, rgba(0,0,0,.55)); as elipses até o canto mais longe."""
    gw, gh = 480, 270
    al = radial(.66 * W, .1 * H, .66 * W * math.sqrt(2), .9 * H * math.sqrt(2), gw, gh, [(0, .07), (.6, 0), (9, 0)])
    al8 = np.clip(np.round(al * 255), 0, 255).astype(np.uint8)
    Image.fromarray(np.dstack([np.full_like(al8, 216), np.full_like(al8, 173), np.full_like(al8, 78), al8]), 'RGBA').save(os.path.join(OUT, 'skirmish_glow.png'))
    vi = radial(.66 * W, .5 * H, .66 * W * math.sqrt(2), .5 * H * math.sqrt(2), gw, gh, [(0, 0), (.5, 0), (1, .55), (9, .55)])
    v8 = np.clip(np.round(vi * 255), 0, 255).astype(np.uint8)
    Image.fromarray(np.dstack([np.zeros_like(v8), np.zeros_like(v8), np.zeros_like(v8), v8]), 'RGBA').save(os.path.join(OUT, 'skirmish_vign.png'))
    print('ok', OUT, 'skirmish_glow.png, skirmish_vign.png')


def screens():
    """O brilho do alto das outras telas (06/11: o multiplayer, as opções, o SCRIPT), pela mesma conta:
    radial-gradient(at 50% 0%, rgba(216,173,78,.07), rgba(0,0,0,0) 60%), a elipse até o canto mais longe."""
    gw, gh = 480, 270
    al = radial(.5 * W, 0, .5 * W * math.sqrt(2), H * math.sqrt(2), gw, gh, [(0, .07), (.6, 0), (9, 0)])
    al8 = np.clip(np.round(al * 255), 0, 255).astype(np.uint8)
    Image.fromarray(np.dstack([np.full_like(al8, 216), np.full_like(al8, 173), np.full_like(al8, 78), al8]), 'RGBA').save(os.path.join(OUT, 'screens_glow.png'))
    # o SCRIPT (a tela do desenvolvimento) tem o mesmo brilho em ciano de planta: rgba(120,190,220,.06)
    al = radial(.5 * W, 0, .5 * W * math.sqrt(2), H * math.sqrt(2), gw, gh, [(0, .06), (.6, 0), (9, 0)])
    al8 = np.clip(np.round(al * 255), 0, 255).astype(np.uint8)
    Image.fromarray(np.dstack([np.full_like(al8, 120), np.full_like(al8, 190), np.full_like(al8, 220), al8]), 'RGBA').save(os.path.join(OUT, 'script_glow.png'))
    print('ok', OUT, 'screens_glow.png, script_glow.png')


if __name__ == '__main__':
    import sys
    skirmish() if '--skirmish' in sys.argv else screens() if '--screens' in sys.argv else main()
