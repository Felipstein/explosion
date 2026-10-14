"""Prepara as imagens do laboratório em HUDLab/LabAssets/ (PNG, que a Unreal lê direto do disco):
o fundo (uma das cenas favoritas do deserto), o mapa do minimapa (tirado da peça aprovada do minimapa
clássico, desgirado) e os emblemas das facções com o fundo transparente.

Uso: python HUDLab/Tools/prepare_assets.py
"""
import os
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT = os.path.join(ROOT, 'HUDLab', 'LabAssets')
os.makedirs(OUT, exist_ok=True)

# 1. o fundo: town_battle_top, a mais "de jogo" das sete favoritas (docs/concept/scenes/README.md)
bg = Image.open(os.path.join(ROOT, 'docs/concept/scenes/desert_golden_hour/best/town_battle_top_gemini.webp')).convert('RGB')
bg.resize((1920, int(1920 * bg.height / bg.width)), Image.LANCZOS).save(os.path.join(OUT, 'background.png'))

# 2. o minimapa: o mapa da peça aprovada (pieces/minimap_and_buttons.webp), girado de volta pra quadrado
#    (com a névoa nos 3 níveis, os pontos e a câmera que a arte aprovada já traz)
mm = Image.open(os.path.join(ROOT, 'docs/concept/hud/pieces/minimap_and_buttons.webp')).convert('RGB')
cx, cy, half = 147, 170, 84
rot = mm.rotate(45, resample=Image.BICUBIC, center=(cx, cy))
rot.crop((cx - half, cy - half, cx + half, cy + half)).resize((512, 512), Image.LANCZOS).save(os.path.join(OUT, 'minimap.png'))

# 3. os emblemas: já vêm com o fundo transparente; só centraliza num quadrado
for f in ('usa', 'china', 'gla'):
    im = Image.open(os.path.join(ROOT, f'docs/concept/factions/{f}_emblem.webp')).convert('RGBA')
    side = max(im.size)
    sq = Image.new('RGBA', (side, side), (0, 0, 0, 0))
    sq.paste(im, ((side - im.width) // 2, (side - im.height) // 2))
    sq.resize((256, 256), Image.LANCZOS).save(os.path.join(OUT, f'emblem_{f}.png'))
print('ok', OUT)
