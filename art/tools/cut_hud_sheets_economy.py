"""Recorta as peças novas do HUD (01/11: a economia, a patente e a tela do general) das folhas do GPT sobre verde.

Folha A (molduras): o brasão do contador, o cubo do general, o tubo da patente, a fileira de energia, as 3 plaquetas.
Folha B (fichas e medalhas): as 3 fichas de ponto e as 4 medalhas das comendas.
Cada peça sai pela mancha opaca (o verde vira transparente e o verde que vaza na borda é limpo), em ordem de leitura.

Uso: python art/tools/cut_hud_sheets_economy.py <folha_A> <folha_B>
"""
import os
import sys
import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT = os.path.join(ROOT, 'docs/concept/hud/kit/skin')

NAMES_A = ['crest_money', 'hub_general', 'rank_tube', 'power_conduit', 'plate_red', 'plate_amber', 'plate_gold']
NAMES_B = ['token_gold', 'token_dark', 'token_green', 'medal_first_strike', 'medal_tank_killer', 'medal_hold_the_line', 'medal_logistics']


def key_green(im):
    a = np.asarray(im.convert('RGB')).astype(float)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    spill = g - np.maximum(r, b)
    alpha = np.clip(1.0 - (spill - 40) / 80.0, 0, 1)
    g2 = np.where(spill > 0, np.maximum(r, b), g)          # o verde não passa do maior entre vermelho e azul
    return np.dstack([r, g2, b, alpha * 255]).clip(0, 255).astype('uint8')


def pieces(path, names, min_area=1500):
    rgba = key_green(Image.open(path))
    mask = rgba[..., 3] > 40
    mask = ndimage.binary_closing(mask, iterations=2)
    lab, n = ndimage.label(mask)
    boxes = [s for s in ndimage.find_objects(lab) if (s[0].stop - s[0].start) * (s[1].stop - s[1].start) >= min_area]
    # ordem de leitura: por faixa de altura (centro), depois da esquerda pra direita
    boxes.sort(key=lambda s: (round(((s[0].start + s[0].stop) / 2) / 120), s[1].start))
    return rgba, boxes


def save(rgba, boxes, names, order=None):
    if order:
        boxes = [boxes[i] for i in order]
    for nm, s in zip(names, boxes):
        y0, y1, x0, x1 = s[0].start, s[0].stop, s[1].start, s[1].stop
        Image.fromarray(rgba[y0:y1, x0:x1], 'RGBA').save(os.path.join(OUT, nm + '.png'))
        print(nm, (x0, y0, x1 - x0, y1 - y0))


if __name__ == '__main__':
    a, b = sys.argv[1], sys.argv[2]
    rgba, boxes = pieces(a, NAMES_A)
    # a folha A: brasão, cubo, tubo (de cima), depois a fileira, depois as 3 plaquetas
    by = {}
    for s in boxes:
        w, h = s[1].stop - s[1].start, s[0].stop - s[0].start
        by.setdefault('tube' if h > 3 * w else 'conduit' if w > 8 * h else 'other', []).append(s)
    other = sorted(by['other'], key=lambda s: (s[0].start > 600, s[1].start))
    ordered = [other[0], other[1], by['tube'][0], by['conduit'][0]] + other[2:5]
    save(rgba, ordered, NAMES_A)
    rgba, boxes = pieces(b, NAMES_B)
    top = sorted([s for s in boxes if s[0].stop < 360], key=lambda s: s[1].start)
    bot = sorted([s for s in boxes if s[0].stop >= 360], key=lambda s: s[1].start)
    save(rgba, top + bot, NAMES_B)
