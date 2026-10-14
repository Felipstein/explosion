"""Folha ficha × nosso de um modelo provisório (Python do sistema, com Pillow).

Uso: python Game/Tools/model_sheet.py <ficha.webp> <pasta_preview> <saida.jpg> [recortes.json]
Cada linha da folha é uma vista: o recorte da ficha à esquerda, o nosso render à direita, na mesma altura;
embaixo, as vistas extras (o outro lado, a câmera do jogo). O JSON diz as linhas e os recortes da ficha
(frações da imagem: x0, y0, x1, y1):
  {"rows": [{"tag": "top", "label": "cima", "crop": [0.36, 0.03, 1.0, 0.30]}, ...],
   "extra": [{"tag": "game_cam", "label": "a camera do jogo"}, ...]}
Sem o JSON, os recortes da ficha do tanque do GLA (Game/Tools/model_sheets/gla_tank.json).
"""
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ficha, prev, out = sys.argv[1], sys.argv[2], sys.argv[3]
spec_path = sys.argv[4] if len(sys.argv) > 4 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'model_sheets', 'gla_tank.json')
spec = json.load(open(spec_path, encoding='utf-8'))
ROW_H = 420
PAD = 14
try:
    font = ImageFont.truetype('arial.ttf', 22)
except OSError:
    font = ImageFont.load_default()

src = Image.open(ficha).convert('RGB')
W, H = src.size
rows = []
for row in spec['rows']:
    x0, y0, x1, y1 = row['crop']
    a = src.crop((int(W * x0), int(H * y0), int(W * x1), int(H * y1)))
    b = Image.open(os.path.join(prev, row['tag'] + '.png')).convert('RGB')
    a = a.resize((int(a.width * ROW_H / a.height), ROW_H), Image.LANCZOS)
    b = b.resize((int(b.width * ROW_H / b.height), ROW_H), Image.LANCZOS)
    rows.append((row['label'], a, b))
extra = [(e['label'], os.path.join(prev, e['tag'] + '.png')) for e in spec.get('extra', []) if os.path.exists(os.path.join(prev, e['tag'] + '.png'))]
width = max(a.width + b.width + 3 * PAD for _, a, b in rows)
height = sum(ROW_H + 2 * PAD + 30 for _ in rows)
extra_imgs = []
for label, pth in extra:
    im = Image.open(pth).convert('RGB')
    w = min(width - 2 * PAD, im.width)
    im = im.resize((w, int(im.height * w / im.width)), Image.LANCZOS)
    extra_imgs.append((label, im))
    height += im.height + 2 * PAD + 30
sheet = Image.new('RGB', (width, height), (24, 24, 24))
d = ImageDraw.Draw(sheet)
y = 0
for label, a, b in rows:
    d.text((PAD, y + 4), f'{label}: ficha', fill=(255, 230, 40), font=font)
    d.text((PAD * 2 + a.width, y + 4), 'nosso (provisorio por script)', fill=(255, 230, 40), font=font)
    y += 30
    sheet.paste(a, (PAD, y))
    sheet.paste(b, (PAD * 2 + a.width, y))
    y += ROW_H + 2 * PAD
for label, im in extra_imgs:
    d.text((PAD, y + 4), label, fill=(255, 230, 40), font=font)
    y += 30
    sheet.paste(im, (PAD, y))
    y += im.height + 2 * PAD
sheet.save(out, quality=90)
print('SHEET', out, sheet.size)
