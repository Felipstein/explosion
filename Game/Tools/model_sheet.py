"""Folha ficha × nosso de um modelo provisório (Python do sistema, com Pillow).

Uso: python Game/Tools/model_sheet.py <ficha.webp> <pasta_preview> <saida.jpg> [recortes.json]
Cada linha da folha é uma vista: o recorte da ficha à esquerda, o nosso render à direita, na mesma altura.
Os recortes da ficha (frações da imagem: x0, y0, x1, y1) vêm do JSON; sem ele, os da ficha do tanque do GLA.
"""
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ficha, prev, out = sys.argv[1], sys.argv[2], sys.argv[3]
crops = json.load(open(sys.argv[4])) if len(sys.argv) > 4 else {
    'top': (0.36, 0.03, 1.0, 0.30), 'left': (0.0, 0.33, 0.68, 0.58), 'front': (0.0, 0.03, 0.36, 0.30),
    'back': (0.68, 0.33, 1.0, 0.58), 'three_quarter': (0.0, 0.58, 1.0, 1.0),
}
ORDER = ['top', 'left', 'front', 'back', 'three_quarter']
LABEL = {'top': 'cima', 'left': 'lado esquerdo', 'front': 'frente', 'back': 'tras', 'three_quarter': '3/4'}
ROW_H = 420
PAD = 14
try:
    font = ImageFont.truetype('arial.ttf', 22)
except OSError:
    font = ImageFont.load_default()

src = Image.open(ficha).convert('RGB')
W, H = src.size
rows = []
for tag in ORDER:
    x0, y0, x1, y1 = crops[tag]
    a = src.crop((int(W * x0), int(H * y0), int(W * x1), int(H * y1)))
    b = Image.open(os.path.join(prev, tag + '.png')).convert('RGB')
    a = a.resize((int(a.width * ROW_H / a.height), ROW_H), Image.LANCZOS)
    b = b.resize((int(b.width * ROW_H / b.height), ROW_H), Image.LANCZOS)
    rows.append((tag, a, b))
extra = [os.path.join(prev, n + '.png') for n in ('three_quarter_right', 'game_cam', 'game_cam_close') if os.path.exists(os.path.join(prev, n + '.png'))]
width = max(a.width + b.width + 3 * PAD for _, a, b in rows)
height = sum(ROW_H + 2 * PAD + 30 for _ in rows)
extra_imgs = []
for pth in extra:
    im = Image.open(pth).convert('RGB')
    im = im.resize((min(width - 2 * PAD, im.width), int(im.height * min(width - 2 * PAD, im.width) / im.width)), Image.LANCZOS)
    extra_imgs.append(im)
    height += im.height + 2 * PAD + 30
sheet = Image.new('RGB', (width, height), (24, 24, 24))
d = ImageDraw.Draw(sheet)
y = 0
for tag, a, b in rows:
    d.text((PAD, y + 4), f'{LABEL[tag]}: ficha', fill=(255, 230, 40), font=font)
    d.text((PAD * 2 + a.width, y + 4), 'nosso (provisorio por script)', fill=(255, 230, 40), font=font)
    y += 30
    sheet.paste(a, (PAD, y))
    sheet.paste(b, (PAD * 2 + a.width, y))
    y += ROW_H + 2 * PAD
for pth, im in zip(extra, extra_imgs):
    n = os.path.basename(pth).replace('.png', '')
    d.text((PAD, y + 4), n + (': a camera do jogo (37,5 graus), tanque USA ao lado' if n.startswith('game') else ': o outro lado (a ficha nao tem)'), fill=(255, 230, 40), font=font)
    y += 30
    sheet.paste(im, (PAD, y))
    y += im.height + 2 * PAD
sheet.save(out, quality=90)
print('SHEET', out, sheet.size)
