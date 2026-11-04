# folha lado a lado (recortes ampliados) pros testes do T0: python t0_sheet.py saida.jpg x0 y0 x1 y1 escala "rotulo=caminho" ...
import sys
from PIL import Image, ImageDraw, ImageFont
out, x0, y0, x1, y1, sc = sys.argv[1], *map(int, sys.argv[2:6]), float(sys.argv[6])
items = [a.split('=', 1) for a in sys.argv[7:]]
tiles = []
for lab, p in items:
    im = Image.open(p).convert('RGB').crop((x0, y0, x1, y1))
    im = im.resize((int(im.width * sc), int(im.height * sc)), Image.LANCZOS)
    d = ImageDraw.Draw(im)
    try:
        f = ImageFont.truetype('arial.ttf', 22)
    except Exception:
        f = ImageFont.load_default()
    d.rectangle((0, 0, im.width, 32), fill=(0, 0, 0))
    d.text((8, 4), lab, fill=(255, 255, 255), font=f)
    tiles.append(im)
cols = 2 if len(tiles) > 2 else len(tiles)
rows = (len(tiles) + cols - 1) // cols
W, H = tiles[0].size
sheet = Image.new('RGB', (W * cols + 6 * (cols - 1), H * rows + 6 * (rows - 1)), (40, 40, 40))
for i, t in enumerate(tiles):
    sheet.paste(t, ((i % cols) * (W + 6), (i // cols) * (H + 6)))
sheet.save(out, quality=88)
print(out, sheet.size)
