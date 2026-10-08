"""Folha lado a lado: a arte conceitual (a referência aprovada) e os nossos quadros.

Regra do projeto: nenhum visual novo é "pronto" sem passar por esta folha, na
mesma câmera e no mesmo instante da arte (docs/EXPLOSOES.md).

    python BlastLab/Tools/concept_sheet.py saida.jpg docs/concept/scenes/game_shot_usa_vs_gla.webp \
        "deserto 1.5 s|captures/ue/range_desert_rts/sim_1.50.png" ["rótulo|imagem[|x0,y0,x1,y1]" ...]

Cada quadro pode levar um recorte (x0,y0,x1,y1 em pixels da imagem) pra pôr a
explosão no mesmo tamanho na tela que a da arte.
"""
import sys

from PIL import Image, ImageDraw, ImageFont

TILE_W = 960


def load(spec):
    parts = spec.split('|')
    label, path = (parts[0], parts[1]) if len(parts) > 1 else ('referência (arte conceitual)', parts[0])
    im = Image.open(path).convert('RGB')
    if len(parts) > 2:
        im = im.crop(tuple(int(v) for v in parts[2].split(',')))
    return label, im.resize((TILE_W, int(im.height * TILE_W / im.width)), Image.LANCZOS)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    out, ref, *shots = sys.argv[1:]
    tiles = [load(ref)] + [load(s) for s in shots]
    cols = 2
    rows = (len(tiles) + cols - 1) // cols
    h = max(t[1].height for t in tiles)
    sheet = Image.new('RGB', (cols * TILE_W + (cols - 1) * 8, rows * (h + 30)), (16, 16, 16))
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype('arial.ttf', 20)
    except OSError:
        font = ImageFont.load_default()
    for i, (label, im) in enumerate(tiles):
        x, y = (i % cols) * (TILE_W + 8), (i // cols) * (h + 30)
        draw.text((x + 6, y + 4), label, fill=(255, 200, 120), font=font)
        sheet.paste(im, (x, y + 30))
    sheet.save(out, quality=88)
    print(out, sheet.size)


if __name__ == '__main__':
    main()
