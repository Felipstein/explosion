"""Recorta o kit de pele do HUD (as molduras vazias geradas no GPT sobre fundo verde, no estilo das peças
aprovadas) em PNGs com transparência: docs/concept/hud/kit/skin/<peça>.png.

- tira o verde (chroma key) e limpa o verde que vaza nas bordas;
- recorta cada peça pela caixa aproximada e aperta no que sobrou de opaco;
- no minimapa, o miolo preto do losango vira transparente (o mapa vai por baixo da moldura).

Uso: python art/tools/cut_hud_skin.py <folha_1> <folha_2>
"""
import os
import sys
import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT = os.path.join(ROOT, 'docs/concept/hud/kit/skin')
os.makedirs(OUT, exist_ok=True)


def key_green(im):
    a = np.asarray(im.convert('RGB')).astype(float)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    spill = g - np.maximum(r, b)
    alpha = np.clip(1.0 - (spill - 40) / 80.0, 0, 1)          # verde forte = transparente
    # tira o verde que sobrou na borda (despill): o verde não passa do maior entre vermelho e azul
    g2 = np.where(spill > 0, np.maximum(r, b) + np.minimum(spill, 0), g)
    out = np.dstack([r, g2, b, alpha * 255]).clip(0, 255).astype('uint8')
    return Image.fromarray(out, 'RGBA')


def tight(im, box):
    q = im.crop(box)
    a = np.asarray(q)[..., 3]
    ys, xs = np.where(a > 24)
    return q.crop((xs.min(), ys.min(), xs.max() + 1, ys.max() + 1))


def clear_inner_black(im, seed, tol=26):
    """O miolo preto (a partir do ponto seed) vira transparente: preenchimento por vizinhança."""
    a = np.asarray(im).copy()
    h, w = a.shape[:2]
    rgb = a[..., :3].astype(int)
    seen = np.zeros((h, w), bool)
    stack = [seed]
    while stack:
        x, y = stack.pop()
        if x < 0 or y < 0 or x >= w or y >= h or seen[y, x]:
            continue
        if rgb[y, x].max() > tol:
            continue
        seen[y, x] = True
        stack.extend(((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)))
    a[seen, 3] = 0
    return Image.fromarray(a, 'RGBA')


def main(sheet1, sheet2):
    s1 = key_green(Image.open(sheet1))
    # um fiapo da caixinha de cima encosta no topo da coluna de botões do minimapa: apaga antes de recortar
    arr1 = np.asarray(s1).copy(); arr1[255:405, 600:765, 3] = 0; s1 = Image.fromarray(arr1, 'RGBA')
    s2 = key_green(Image.open(sheet2))
    pieces = {
        # folha 1: as peças grandes
        'top_bar_1v1': (s1, (15, 35, 1120, 186)),
        'superweapon_box': (s1, (615, 190, 1050, 265)),
        'commander_panel': (s1, (1140, 30, 1660, 252)),
        'minimap_pod': (s1, (35, 258, 762, 892)),
        'unit_card': (s1, (830, 358, 1636, 858)),
        # folha 2: as peças pequenas
        'slot_normal': (s2, (85, 12, 300, 216)),
        'slot_action': (s2, (358, 12, 570, 216)),
        'slot_captain': (s2, (624, 12, 840, 216)),
        'slot_locked': (s2, (890, 12, 1106, 216)),
        'group_on': (s2, (84, 255, 330, 372)),
        'group_off': (s2, (362, 255, 606, 372)),
        'alert': (s2, (664, 244, 1622, 392)),
        'power': (s2, (84, 392, 300, 680)),
        'panel_wide': (s2, (364, 428, 1312, 666)),
        'panel_long': (s2, (14, 690, 1662, 922)),
    }
    for name, (sheet, box) in pieces.items():
        im = tight(sheet, box)
        if name == 'minimap_pod':
            # o centro do losango (no recorte apertado): o miolo preto vira o buraco do mapa
            im = clear_inner_black(im, (330, 300), tol=12)
        if name == 'unit_card':
            # a janela do retrato também vira buraco: o retrato vai por baixo e a moldura cobre a borda dele
            im = clear_inner_black(im, (390, 165), tol=12)
        if name == 'alert':
            # o fim do degradê vermelho se misturou com o verde do fundo: some aos poucos, sem o verde
            arr = np.asarray(im).astype(float)
            r, g, b, al = arr[..., 0], arr[..., 1], arr[..., 2], arr[..., 3]
            sp = g - np.maximum(r, b)
            al = np.where(sp > 0, al * np.clip(1 - sp / 120.0, 0, 1), al)
            g = np.minimum(g, np.maximum(r, b) * 0.6 + 4)
            w = al.shape[1]; ramp = np.clip((w * 0.97 - np.arange(w)) / (w * 0.14), 0, 1)[None, :]
            al = al * ramp
            # os parafusos da ponta vieram rosa/roxos da folha: viram aço como os das outras peças
            xs = np.arange(w)[None, :]
            mag = (b - g > 25) & (r - g > 25) & (xs < 80)
            lum = 0.3 * r + 0.59 * g + 0.11 * b
            r = np.where(mag, lum * 0.94, r); g = np.where(mag, lum * 0.98, g); b = np.where(mag, lum * 1.06, b)
            im = Image.fromarray(np.dstack([r, g, b, al]).clip(0, 255).astype('uint8'), 'RGBA')
        im.save(os.path.join(OUT, name + '.png'))
        print(name, im.size)
    # o botão de ação: o verde da moldura some com o fundo verde, então sai do botão normal com o azul virado verde
    base = Image.open(os.path.join(OUT, 'slot_normal.png'))
    arr = np.asarray(base).astype(float)
    r, g, b, al = arr[..., 0], arr[..., 1], arr[..., 2], arr[..., 3]
    blue = np.clip((b - np.maximum(r, g) - 10) / 60.0, 0, 1)
    ng = g * (1 - blue) + b * blue
    nb = b * (1 - blue) + g * 0.55 * blue
    nr = r * (1 - blue) + r * 0.6 * blue
    Image.fromarray(np.dstack([nr, ng, nb, al]).clip(0, 255).astype('uint8'), 'RGBA').save(os.path.join(OUT, 'slot_action.png'))
    print('slot_action (do normal, azul -> verde)')
    # o aviso amarelo ("Low power"): o mesmo aviso com o vermelho girado pro âmbar
    al_im = Image.open(os.path.join(OUT, 'alert.png'))
    alpha = al_im.getchannel('A')
    hsv = np.asarray(al_im.convert('RGB').convert('HSV')).astype(int)
    hsv[..., 0] = (hsv[..., 0] + 26) % 256      # 26/256 da volta ≈ 37°: vermelho → âmbar
    amber = Image.fromarray(hsv.astype('uint8'), 'HSV').convert('RGB')
    amber.putalpha(alpha)
    amber.save(os.path.join(OUT, 'alert_amber.png'))
    print('alert_amber (do vermelho, matiz girada)')
    # a tecla vermelha (a caixa de superarma que está pra sair): a azul com o neon girado pro vermelho
    on = Image.open(os.path.join(OUT, 'group_on.png'))
    hsv = np.asarray(on.convert('RGB').convert('HSV')).astype(int)
    hsv[..., 0] = (hsv[..., 0] + 101) % 256     # azul (~155) → vermelho (0)
    # o miolo escuro perde a cor (na peça aprovada só o aro é vermelho)
    v = hsv[..., 2].astype(float)
    hsv[..., 1] = (hsv[..., 1] * np.clip((v - 40) / 80.0, 0.15, 1.0)).astype(int)
    red = Image.fromarray(hsv.astype('uint8'), 'HSV').convert('RGB')
    red.putalpha(on.getchannel('A'))
    red.save(os.path.join(OUT, 'group_soon.png'))
    print('group_soon (da azul, matiz girada)')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
