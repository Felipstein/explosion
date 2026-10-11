"""Recorta cada peça aprovada do HUD das imagens de referência (docs/concept/hud/refs, 1672x941)
e salva em docs/concept/hud/pieces/, mais uma folha de conferência no scratchpad."""
import os, sys
from PIL import Image, ImageDraw, ImageFont

root = sys.argv[1]           # docs/concept/hud
out_sheet = sys.argv[2]      # folha de conferência
refs = os.path.join(root, 'refs')
dst = os.path.join(root, 'pieces')
os.makedirs(dst, exist_ok=True)

PIECES = [
    # (arquivo de saída, imagem de origem, caixa x0, y0, x1, y1)
    ('top_bar_1v1', 'clean_tanks', (560, 0, 1110, 92)),
    ('alert', 'clean_tanks', (4, 4, 285, 60)),
    ('commander_panel', 'clean_tanks', (1350, 0, 1672, 98)),
    ('offscreen_arrow', 'clean_tanks', (1600, 340, 1672, 430)),
    ('powers_column', 'clean_upgrades', (4, 76, 104, 522)),
    ('tooltip', 'clean_war_factory', (552, 676, 852, 742)),
    ('minimap_and_buttons', 'classic_tanks', (0, 588, 372, 941)),
    ('action_icons', 'classic_tanks', (686, 772, 988, 842)),
    ('unit_card', 'classic_tanks', (1278, 686, 1672, 941)),
    ('main_grid_palette', 'classic_war_factory', (542, 762, 1168, 941)),
    ('unit_card_queue', 'classic_war_factory', (1302, 668, 1672, 941)),
    ('command_panel_wide', 'classic_dozer', (456, 690, 1300, 941)),
    ('unit_card_dozer', 'classic_dozer', (1292, 690, 1672, 941)),
    ('faction_plate', 'classic_nothing_8p', (1300, 730, 1672, 941)),
    ('top_bar_8_players', 'mix_v9_nothing_8p', (286, 0, 1420, 148)),
    ('group_tabs_health', 'mix_v9_nothing_8p', (576, 872, 1092, 936)),
    ('multi_select_captain', 'mix_v9_tanks', (338, 768, 1320, 936)),
]

saved = []
for name, src, box in PIECES:
    im = Image.open(os.path.join(refs, src + '.webp')).convert('RGB').crop(box)
    im.save(os.path.join(dst, name + '.webp'), quality=92)
    saved.append((name, im))

# folha de conferência: cada peça com o nome em cima
f = ImageFont.truetype('C:/Windows/Fonts/bahnschrift.ttf', 20)
W = 1700
x = y = 10; rowh = 0; placed = []
for name, im in saved:
    s = min(1.0, 800 / im.width)
    t = im.resize((int(im.width * s), int(im.height * s)))
    if x + t.width > W:
        x = 10; y += rowh + 40; rowh = 0
    placed.append((name, t, x, y))
    x += t.width + 20; rowh = max(rowh, t.height)
sheet = Image.new('RGB', (W, y + rowh + 50), (24, 26, 30))
d = ImageDraw.Draw(sheet)
for name, t, px, py in placed:
    sheet.paste(t, (px, py + 26)); d.text((px, py + 2), name, fill=(255, 212, 0), font=f)
sheet.save(out_sheet)
print(len(saved), 'pieces', sheet.size)
