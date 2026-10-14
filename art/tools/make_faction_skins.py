"""A pele do HUD da China e do GLA a partir da pele da USA: só o neon azul muda de cor; o metal, os rebites, o
verde da ação e o dourado do capitão ficam. É como no ZH, em que cada facção tem o painel na sua cor.

- China: o neon vermelho (a cor da facção);
- GLA: o neon âmbar de latão. O verde do GLA é a cor da ação no HUD inteiro, então a moldura normal não pode
  ser verde (proposta da arte, 13/10; o Felipe avalia).

Saída: docs/concept/hud/kit/skin/china/<peça>.png e skin/gla/<peça>.png. O painter usa essas quando o estado
pede a facção (FRTSHudState::SkinFaction) e volta pra da USA quando a peça não existe.
Uso: python art/tools/make_faction_skins.py
"""
import os
import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SKIN = os.path.join(ROOT, 'docs/concept/hud/kit/skin')
# as peças com o neon azul (o topo 1x1 fica: o azul ali é "você", não a facção)
PIECES = ['minimap_pod', 'unit_card', 'slot_normal', 'slot_locked', 'group_on', 'power', 'panel_wide', 'panel_long',
          'superweapon_box', 'commander_panel']
# o neon (matiz, ganho de saturação) e o metal (matiz, saturação máxima): a China em aço quente, o GLA em ferro marrom
FACTIONS = {'china': (0.0, 1.15, 8.0, 0.10), 'gla': (34.0, 1.0, 30.0, 0.16)}


def hsv_to_rgb(h, s, v):
    c = v * s
    x = c * (1 - np.abs((h / 60.0) % 2 - 1))
    z = np.zeros_like(c)
    k = (h // 60).astype(int) % 6
    return np.stack([
        np.choose(k, [c, x, z, z, x, c]),
        np.choose(k, [x, c, c, x, z, z]),
        np.choose(k, [z, z, x, c, c, x]),
    ], -1) + (v - c)[..., None]


def recolor(im, hue_to, sat_gain, metal_hue, metal_sat):
    """O neon azul (saturado e aceso) vira a cor da facção; o tom azulado do metal vira o metal da facção.
    A mistura é feita em RGB (girar o matiz pela metade passava pelo roxo)."""
    a = np.asarray(im.convert('RGBA')).astype(np.float32) / 255.0
    rgb, al = a[..., :3], a[..., 3:]
    mx, mn = rgb.max(-1), rgb.min(-1)
    d = mx - mn
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    h = np.zeros_like(mx)
    m = d > 1e-5
    rm = m & (mx == r)
    gm = m & (mx == g) & ~rm
    bm = m & ~rm & ~gm
    h[rm] = ((g - b)[rm] / d[rm]) % 6
    h[gm] = (b - r)[gm] / d[gm] + 2
    h[bm] = (r - g)[bm] / d[bm] + 4
    h = h * 60.0
    s = np.where(mx > 1e-5, d / np.maximum(mx, 1e-5), 0)
    v = mx
    wb = np.clip(1 - np.abs(h - 212.0) / 45.0, 0, 1) * np.clip(s / 0.08, 0, 1)        # o quanto é azul
    n = np.clip((s - 0.35) / 0.2, 0, 1) * np.clip((v - 0.30) / 0.2, 0, 1)            # o quanto é neon
    neon = hsv_to_rgb(np.full_like(h, hue_to), np.clip(s * sat_gain, 0, 1), v)
    metal = hsv_to_rgb(np.full_like(h, metal_hue), np.minimum(s, metal_sat), v)
    mix = neon * n[..., None] + metal * (1 - n)[..., None]
    out = rgb * (1 - wb)[..., None] + mix * wb[..., None]
    res = np.concatenate([out, al], -1)
    return Image.fromarray((np.clip(res, 0, 1) * 255 + 0.5).astype('uint8'), 'RGBA')


for f, (hue, gain, mhue, msat) in FACTIONS.items():
    os.makedirs(os.path.join(SKIN, f), exist_ok=True)
    for p in PIECES:
        src = os.path.join(SKIN, p + '.png')
        if os.path.exists(src):
            recolor(Image.open(src), hue, gain, mhue, msat).save(os.path.join(SKIN, f, p + '.png'))
    print(f, len(PIECES))
