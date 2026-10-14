"""A planta da vila vista de cima, sem Unreal (pra conferir o lugar e o desenho).

  python WorldForge/Tools/village_preview.py <pasta dos mapas de depuração> [saída.png] [x0,y0,x1,y1]

Lê os mapas de depuração (debug_maps.py: shade.png e os pesos do chão) e o
village.json da mesma pasta. Desenha o relevo, as estradas, as pegadas de cada
peça na grade de 4 m (casa, muro, portão, carro, poste, miudeza), as palmeiras e
as áreas que a vila não pode tapar (do JSON dos controles, se passado em
WORLD_SETTINGS). O norte (+Y) fica em cima, como o minimapa.
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

folder = sys.argv[1]
out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(folder, 'village_plan.png')
crop = [float(v) for v in sys.argv[3].split(',')] if len(sys.argv) > 3 else None

sh = np.asarray(Image.open(os.path.join(folder, 'shade.png')).convert('RGB')).astype(float)
w0 = np.asarray(Image.open(os.path.join(folder, 'w012_sand_gravel_dirt.png')).convert('RGB')).astype(float) / 255
w3 = np.asarray(Image.open(os.path.join(folder, 'w345_rock_salt_asphalt.png')).convert('RGB')).astype(float) / 255
img = sh * 0.9 + 20
img = img * (1 - w0[..., 2:3] * 0.5) + np.array([170, 120, 75]) * w0[..., 2:3] * 0.5
img = img * (1 - w3[..., 2:3]) + np.array([45, 45, 50]) * w3[..., 2:3]
img = img * (1 - w3[..., 1:2] * 0.5) + np.array([255, 255, 255]) * w3[..., 1:2] * 0.5
R = img.shape[0]
size = float(R)   # 1 m por pixel
half = 0.5 * size
SC = 2   # aumenta pra ver as peças de 4 m
im = Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).resize((R * SC, R * SC), Image.NEAREST)
d = ImageDraw.Draw(im)
px = lambda x, y: ((x + half) * SC, (half - y) * SC)

v = json.load(open(os.path.join(folder, 'village.json'), encoding='utf-8'))
cell = v['cell_m']
ox, oy = v['origin_m']
colors = {
    'civ_house_1story': (230, 205, 160), 'civ_house_2story': (255, 240, 200), 'civ_house_porch': (215, 180, 120),
    'civ_house_long': (200, 170, 130), 'civ_wall': (120, 90, 60), 'civ_wall_gate': (40, 140, 220),
    'civ_car_burnt': (30, 30, 30), 'civ_pickup': (220, 60, 40), 'civ_pole': (255, 255, 0),
    'civ_sandbags': (150, 140, 80), 'civ_crates': (160, 110, 60), 'civ_barrels': (90, 120, 160), 'civ_ammo': (90, 110, 60),
}
for p in v['pieces']:
    i, j = p['cell']
    w, h = p['cells']
    x0, y0 = ox + i * cell, oy + j * cell
    a, b = px(x0, y0 + h * cell), px(x0 + w * cell, y0)
    col = colors.get(p['type'], (255, 0, 255))
    if p['type'].startswith('civ_house'):
        d.rectangle([a, b], fill=col, outline=(60, 40, 20))
        # a frente (porta): um traço no lado da rua
        fx, fy = [(0, 1), (-1, 0), (0, -1), (1, 0)][p['rot']]
        cx, cy = px(p['x'] + fx * 3.0, p['y'] + fy * 3.0)
        d.ellipse([cx - 3, cy - 3, cx + 3, cy + 3], fill=(200, 30, 30))
    elif p['type'].startswith('civ_wall'):
        cx, cy = px(p['x'], p['y'])
        horiz = p['rot'] % 2 == 0
        L = 2.0 * SC
        d.line([(cx - L, cy), (cx + L, cy)] if horiz else [(cx, cy - L), (cx, cy + L)], fill=col, width=3)
    else:
        d.rectangle([a, b], outline=col, width=2)
# os lotes da 1ª vila (depuração): verde ficou; os outros pela cor do motivo
WHY = {0: None, 1: (255, 0, 255), 2: (255, 0, 0), 3: (0, 120, 255), 4: (255, 140, 0), 5: (255, 220, 0), 6: (160, 0, 200), 7: (0, 0, 0)}
for x0, y0, x1, y1, why in v.get('lots_debug', []):
    if why:
        d.rectangle([px(x0, y1), px(x1, y0)], outline=WHY.get(why, (255, 255, 255)), width=2)
for x, y in v.get('palms', []):
    cx, cy = px(x, y)
    d.ellipse([cx - 4, cy - 4, cx + 4, cy + 4], fill=(40, 150, 50))
ws = os.environ.get('WORLD_SETTINGS')
if ws and os.path.exists(ws):
    for x, y, r in json.load(open(ws, encoding='utf-8')).get('keepout', []):
        cx, cy = px(x, y)
        d.ellipse([cx - r * SC, cy - r * SC, cx + r * SC, cy + r * SC], outline=(255, 0, 0) if r >= 60 else (255, 220, 0), width=2)
if crop:
    x0, y0, x1, y1 = crop
    a, b = px(x0, y1), px(x1, y0)
    im = im.crop((int(a[0]), int(a[1]), int(b[0]), int(b[1])))
im.save(out)
print(out, len(v['pieces']), 'peças')
