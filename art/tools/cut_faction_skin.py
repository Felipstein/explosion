"""Recorta a folha do visual de uma facção (01/11: China e GLA, do GPT sobre verde) nas peças da pele do HUD.

A folha repete as peças da USA (`skin_sheet_2_gpt` + o brasão e o cubo): os 4 botões, as 2 abas, o aviso, o botão de poder, o painel largo,
o painel comprido, o brasão do contador e o cubo do general. Cada peça sai pela mancha opaca e é reconhecida pela forma; as que o HUDLab
fatia (9 fatias) são levadas ao tamanho exato da peça da USA, pra as margens do fatiamento continuarem certas.

Uso: python art/tools/cut_faction_skin.py <folha> <facção> [notone]
Saída: docs/concept/hud/kit/skin/<facção>/<peça>.png
"""
import os
import sys
import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
USA = os.path.join(ROOT, 'docs/concept/hud/kit/skin')


def key_green(a):
    """Tira só o verde do FUNDO (o que está ligado à borda da folha). O verde de dentro das peças fica: o aro verde do botão de ação
    é a cor de "ação" em todas as facções (01/11: o chroma simples apagou esse aro)."""
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    spill = g - np.maximum(r, b)
    greenish = spill > 40
    lab, n = ndimage.label(greenish)
    edge = set(np.unique(np.concatenate([lab[0], lab[-1], lab[:, 0], lab[:, -1]]))) - {0}
    bg = np.isin(lab, list(edge))
    band = ndimage.binary_dilation(bg, iterations=3) & ~bg            # a borda das peças: o meio-tom e o verde que vazou
    alpha = np.ones(spill.shape)
    alpha[bg] = 0
    alpha[band] = np.clip(1.0 - (spill[band] - 40) / 80.0, 0, 1)
    g2 = np.where(band & (spill > 0), np.maximum(r, b), g)
    return np.dstack([r, g2, b, alpha * 255]).clip(0, 255).astype('uint8')


# a face dos painéis grandes mais escura e menos saturada onde a cor da facção ocupa muita área (01/11, ele: "cores muito fortes"):
# só a face (a cor parecida com a do miolo), nunca a moldura, o friso ou o neon
FACE_TONE = {'china': {'panel_long': (0.55, 0.72), 'panel_wide': (0.6, 0.75), 'power': (0.7, 0.85), 'group_on': (0.8, 0.9), 'group_off': (0.8, 0.9)}}


def face_tone(im, sat, val):
    import colorsys
    a = np.asarray(im).astype(float)
    h, w = a.shape[:2]
    core = a[h // 3:2 * h // 3, w // 4:3 * w // 4, :3].reshape(-1, 3)
    ref = np.median(core, axis=0)
    dist = np.linalg.norm(a[..., :3] - ref, axis=-1)
    face = ndimage.gaussian_filter((dist < 70).astype(float) * (a[..., 3] > 200), 1.5)[..., None]
    hsv = np.asarray(Image.fromarray(a[..., :3].astype('uint8')).convert('HSV')).astype(float)
    hsv[..., 1] *= sat
    hsv[..., 2] *= val
    rgb = np.asarray(Image.fromarray(hsv.clip(0, 255).astype('uint8'), 'HSV').convert('RGB')).astype(float)
    a[..., :3] = a[..., :3] * (1 - face) + rgb * face
    return Image.fromarray(a.clip(0, 255).astype('uint8'), 'RGBA')


def main(path, faction):
    rgba = key_green(np.asarray(Image.open(path).convert('RGB')).astype(float))
    lab, n = ndimage.label(ndimage.binary_closing(rgba[..., 3] > 40, iterations=2))
    boxes = []
    for s in ndimage.find_objects(lab):
        h, w = s[0].stop - s[0].start, s[1].stop - s[1].start
        if w * h > 4000:
            boxes.append((s, w, h))
    pick = {}
    left = list(boxes)

    def take(name, key, cond=lambda b: True):
        cands = [b for b in left if cond(b)]
        if not cands:
            raise SystemExit(f'não achei: {name}')
        b = max(cands, key=key)
        left.remove(b)
        pick[name] = b

    take('panel_long', key=lambda b: b[1])                                            # a mais comprida
    take('alert', key=lambda b: b[1] / b[2], cond=lambda b: b[2] < 150)                # comprida e baixa
    def trap(b):
        """O brasão é um trapézio: a largura opaca no alto (a 15%) bem menor que embaixo (a 85%)."""
        s, w, h = b
        m = rgba[s][..., 3] > 128
        top, bot = m[int(h * 0.15)].sum(), m[int(h * 0.85)].sum()
        return top / max(bot, 1)
    take('crest_money', key=lambda b: b[1] * b[2], cond=lambda b: 2.5 < b[1] / b[2] < 5.0 and trap(b) < 0.9)
    take('panel_wide', key=lambda b: b[1] * b[2], cond=lambda b: 3.0 < b[1] / b[2] < 6.5 and b[2] > 150)
    take('hub_general', key=lambda b: -abs(b[1] / b[2] - 0.89), cond=lambda b: 0.7 < b[1] / b[2] < 1.4)   # o medalhão: o mais redondo (a laca da China trocou com o poder)
    # os 4 botões: as 4 peças quase quadradas do mesmo tamanho na fileira de cima; o poder é a que sobra (alta ou quase quadrada)
    sq = sorted([b for b in left if 0.75 < b[1] / b[2] < 1.35], key=lambda b: b[0][0].start)
    slots = sorted(sq[:4], key=lambda b: b[0][1].start)
    for b in slots:
        left.remove(b)
    try:
        take('power', key=lambda b: b[1] * b[2], cond=lambda b: b[2] / b[1] > 0.8)
    except SystemExit:
        print('sem o botão de poder na folha: fica o que já existe')             # 01/11: a China v3 veio sem ele
    for name, b in zip(('slot_normal', 'slot_action', 'slot_captain', 'slot_locked'), sorted(slots, key=lambda b: b[0][1].start)):
        pick[name] = b
    small = min(b[2] for b in slots)                                                # as abas são mais baixas que os botões
    tabs = sorted([b for b in left if 1.8 < b[1] / b[2] < 3.4 and b[2] < small * 0.8], key=lambda b: -b[1] * b[2])[:2]
    for name, b in zip(('group_on', 'group_off'), sorted(tabs, key=lambda b: b[0][1].start)):
        pick[name] = b

    out = os.path.join(USA, faction)
    os.makedirs(out, exist_ok=True)
    for name, (s, w, h) in pick.items():
        im = Image.fromarray(rgba[s], 'RGBA')
        ref = os.path.join(USA, name + '.png')
        if name not in ('crest_money', 'hub_general') and os.path.exists(ref):
            im = im.resize(Image.open(ref).size, Image.LANCZOS)           # o tamanho da peça da USA (o fatiamento do HUDLab)
        if name in FACE_TONE.get(faction, {}) and 'notone' not in sys.argv[3:]:   # notone: a folha já veio com a face escura
            im = face_tone(im, *FACE_TONE[faction][name])
        im.save(os.path.join(out, name + '.png'))
        print(name, w, h, '->', im.size)


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
