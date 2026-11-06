# (05/11) A simulação do clarão da seleção na foto sem clarão (como saíram os números da v5 do M_SelectFlash, docs/CURSORES.md):
# pega a foto do jogo, faz a máscara do tanque pela cor (cinza e azul; a areia é laranja; tira as linhas verdes da fila), converte pra
# linear e aplica cada candidato só no tanque. Aproximação: a foto já passou pela curva de tom, então serve pra comparar os jeitos,
# não pra medir o número final (esse vem da foto do jogo com -RTSFlashTest=hold).
# Uso: python Game/Tools/flash_sim.py <foto.png> <saida.png> [x0 y0 x1 y1]
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

src, dst = sys.argv[1], sys.argv[2]
box = tuple(int(v) for v in sys.argv[3:7]) if len(sys.argv) >= 7 else (880, 470, 1300, 690)
TEAM = np.array([0.10, 0.45, 1.00])   # URTSMatchSubsystem::PaletteColor(0), o azul
LUMA = np.array([0.2126, 0.7152, 0.0722])

img = np.asarray(Image.open(src).convert('RGB')).astype(float) / 255.0
lin = np.where(img <= 0.04045, img / 12.92, ((img + 0.055) / 1.055) ** 2.4)
R, G, B = img[..., 0], img[..., 1], img[..., 2]
mask = ((R - B) < 0.13) & ~((G - R > 0.12) & (G - B > 0.12))
keep = np.zeros_like(mask)
keep[box[1]:box[3], box[0]:box[2]] = True
mask &= keep
m = Image.fromarray((mask * 255).astype(np.uint8)).filter(ImageFilter.MinFilter(3)).filter(ImageFilter.MaxFilter(5))
mask = np.asarray(m.filter(ImageFilter.GaussianBlur(0.7))).astype(float)[..., None] / 255.0
lum = (lin * LUMA).sum(-1, keepdims=True)
hue = TEAM / TEAM.dot(LUMA) * lum
zh = 0.5 + TEAM   # o ZH: a cor do time por saturateRGB(c, 0,5) = 0,5c − 0,25 somada à luz (o difuso e o ambiente) ≈ cena × (0,5 + c)
CANDS = [
    ('sem clarao', lin),
    ('ZH puro: cena x (0,5 + c)', lin * zh),
    ('matiz 0,55 (v4)', lin * 0.45 + hue * 0.55),
    ('matiz 0,85', lin * 0.15 + hue * 0.85),
    ('matiz 0,85 + 25% de luz (v5)', (lin * 0.15 + hue * 0.85) * 1.25),
    ('ZH puro + 25% de luz', lin * zh / zh.dot(LUMA) * 1.25),
]
tiles = []
for name, y in CANDS:
    out = lin * (1 - mask) + y * mask
    s = np.where(out <= 0.0031308, out * 12.92, 1.055 * np.clip(out, 0, None) ** (1 / 2.4) - 0.055)
    t = Image.fromarray((np.clip(s, 0, 1) * 255).astype(np.uint8)).crop(box)
    d = ImageDraw.Draw(t)
    d.rectangle((0, 0, 250, 22), fill=(0, 0, 0))
    d.text((5, 4), name, fill=(255, 255, 255))
    tiles.append(t)
W, H = tiles[0].size
sheet = Image.new('RGB', (W * 3 + 20, H * 2 + 10), (20, 20, 20))
for i, t in enumerate(tiles):
    sheet.paste(t, ((i % 3) * (W + 10), (i // 3) * (H + 10)))
sheet.resize((sheet.width * 3 // 2, sheet.height * 3 // 2), Image.LANCZOS).save(dst)
print(dst)
