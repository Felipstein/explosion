"""Recorta as fotos dos botões que ainda não têm foto própria direto das peças aprovadas do HUD
(docs/concept/hud/pieces/main_grid_palette.webp = os veículos; refs/classic_dozer.webp = os prédios) e
salva em docs/concept/hud/kit/photos/buttons/. São de baixa resolução (vêm de botões de ~80 px):
servem até sair a foto em alta, no mesmo estilo.

Uso: python art/tools/crop_palette_photos.py
"""
import os
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT = os.path.join(ROOT, 'docs/concept/hud/kit/photos/buttons')
os.makedirs(OUT, exist_ok=True)

def save(im, box, name):
    # a área da foto: abaixo da linha da tecla e do cadeado, acima do preço (que vêm pintados na peça)
    q = im.crop(box)
    w = 256
    q.resize((w, int(w * q.height / q.width)), Image.LANCZOS).save(os.path.join(OUT, name + '.png'))

pal = Image.open(os.path.join(ROOT, 'docs/concept/hud/pieces/main_grid_palette.webp')).convert('RGB')
xs = [14, 99, 184, 271, 358, 445, 532]
row1 = ['usa_battle_tank_pal', 'usa_heavy_tank', 'usa_humvee', 'usa_rocket_launcher', 'usa_ambulance', 'usa_anti_air', 'usa_drone']
for x, n in zip(xs, row1):
    save(pal, (x + 3, 13 + 19, x + 77, 13 + 58), n)
row2 = {0: 'usa_ew_tank', 1: 'upg_missile_kit', 2: 'upg_battle_drone'}
for i, n in row2.items():
    save(pal, (xs[i] + 3, 93 + 19, xs[i] + 77, 93 + 58), n)

ref = Image.open(os.path.join(ROOT, 'docs/concept/hud/refs/classic_dozer.webp')).convert('RGB')
ox, oy = 456, 690   # o recorte do painel dentro da imagem de referência
bx = [30, 133, 235, 336, 437, 540, 641]
r1 = ['usa_command_center', 'usa_power_plant', 'usa_barracks', 'usa_supply_center', 'usa_war_factory_pal', 'usa_airfield', 'usa_tech_center']
for x, n in zip(bx, r1):
    save(ref, (ox + x + 3, oy + 64 + 21, ox + x + 93, oy + 64 + 62), n)
r2 = {0: 'usa_missile_defense', 2: 'usa_fire_base', 3: 'usa_radar', 4: 'usa_drop_zone', 5: 'usa_orbital_lance'}
for i, n in r2.items():
    save(ref, (ox + bx[i] + 3, oy + 150 + 21, ox + bx[i] + 93, oy + 150 + 62), n)

print('ok', OUT)
