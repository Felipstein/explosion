"""Recorta a folha das patentes e medalhas de uma facção (02/11: China, GLA, Rússia, do Gemini pelo Atelier).

A folha tem duas fileiras sobre verde: em cima as 5 patentes (medalhões redondos, 1 a 5 da esquerda pra direita), embaixo as 4 medalhas
das comendas (First Strike, Tank Killer, Hold the Line, Logistics). Cada peça sai pela mancha opaca e vai pro tamanho da da USA
(as patentes na largura de `rank_usa_N.png`, as medalhas na altura de `medal_*.png`), em `docs/concept/hud/kit/skin/<facção>/`
como `rank_<facção>_N.png` e `medal_<nome>.png`.

Uso: python art/tools/cut_faction_ranks.py <folha> <facção>
"""
import os
import sys
import numpy as np
from PIL import Image
from scipy import ndimage

sys.path.insert(0, os.path.dirname(__file__))
from cut_faction_skin import key_green  # noqa: E402  (tira só o verde ligado à borda)

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
KIT = os.path.join(ROOT, 'docs/concept/hud/kit/skin')
MEDALS = ['first_strike', 'tank_killer', 'hold_the_line', 'logistics']


def main(path, faction):
    rgba = key_green(np.asarray(Image.open(path).convert('RGB')).astype(float))
    g = rgba.astype(float)                                                       # o respingo verde na borda
    m = np.maximum(g[..., 0], g[..., 2])
    spill = (g[..., 1] > 1.1 * m) & (g[..., 3] > 0)
    rgba[..., 1][spill] = m[spill].astype(np.uint8)
    lab, _ = ndimage.label(ndimage.binary_closing(rgba[..., 3] > 40, iterations=2))
    boxes = [s for s in ndimage.find_objects(lab) if (s[0].stop - s[0].start) * (s[1].stop - s[1].start) > 3000]
    mid = rgba.shape[0] * 0.42                                                   # as patentes em cima, as medalhas embaixo
    top = sorted([s for s in boxes if (s[0].start + s[0].stop) / 2 < mid], key=lambda s: s[1].start)
    bot = sorted([s for s in boxes if (s[0].start + s[0].stop) / 2 >= mid], key=lambda s: s[1].start)
    if len(top) != 5 or len(bot) != 4:
        raise SystemExit(f'esperava 5 patentes e 4 medalhas, achei {len(top)} e {len(bot)}')
    out = os.path.join(KIT, faction)
    os.makedirs(out, exist_ok=True)
    rw = Image.open(os.path.join(KIT, 'rank_usa_1.png')).width
    for i, s in enumerate(top, 1):
        im = Image.fromarray(rgba[s], 'RGBA')
        im = im.resize((rw, round(im.height * rw / im.width)), Image.LANCZOS)
        im.save(os.path.join(out, f'rank_{faction}_{i}.png'))
    for name, s in zip(MEDALS, bot):
        mh = Image.open(os.path.join(KIT, f'medal_{name}.png')).height
        im = Image.fromarray(rgba[s], 'RGBA')
        im = im.resize((round(im.width * mh / im.height), mh), Image.LANCZOS)
        im.save(os.path.join(out, f'medal_{name}.png'))
    print(faction, '5 patentes e 4 medalhas')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
