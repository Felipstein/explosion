# Recorta os ícones de uma folha de estilos (4 colunas = estilos, 3 linhas = poderes) em PNG de 256 px.
#   python docs/poderes/tela/cut_styles.py <folha> <prefixo> -> docs/poderes/tela/estilos/<prefixo>_<estilo>_<poder>.png
# O emblema (a 3ª coluna) sai com o fundo transparente (o fundo da folha é tirado a partir das bordas); os outros saem quadrados.
import os, sys
from PIL import Image, ImageDraw

STYLES = ['pintado', 'sensor', 'emblema', 'silhueta']
POWERS = ['a10', 'awacs', 'naval']
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'estilos')

def runs(profile, thr, min_len):
    out, start = [], None
    for i, v in enumerate(profile + [0]):
        if v > thr and start is None: start = i
        elif v <= thr and start is not None:
            if i - start >= min_len: out.append((start, i))
            start = None
    return out

def boxes(im):
    g = im.convert('RGB'); w, h = g.size; px = g.load()
    bg = px[3, h // 2]
    def far(c): return max(abs(c[0] - bg[0]), abs(c[1] - bg[1]), abs(c[2] - bg[2])) > 26
    cols = [sum(1 for y in range(0, h, 2) if far(px[x, y])) for x in range(w)]
    rows = [sum(1 for x in range(0, w, 2) if far(px[x, y])) for y in range(h)]
    xs = runs(cols, h * 0.05, w // 12); ys = runs(rows, w * 0.05, h // 8)
    return xs, ys, bg

def cut(path, prefix):
    im = Image.open(path).convert('RGBA')
    xs, ys, bg = boxes(im)
    assert len(xs) == 4 and len(ys) == 3, f'achei {len(xs)} colunas e {len(ys)} linhas em {path}'
    os.makedirs(OUT, exist_ok=True)
    for ci, (x0, x1) in enumerate(xs):
        for ri, (y0, y1) in enumerate(ys):
            tile = im.crop((x0, y0, x1, y1))
            if STYLES[ci] == 'emblema':
                # o fundo sai pela borda: preenche a partir dos cantos com tolerância e vira transparente
                t = tile.copy(); ImageDraw.floodfill(t, (1, 1), (255, 0, 255, 0), thresh=40)
                for c in ((t.width - 2, 1), (1, t.height - 2), (t.width - 2, t.height - 2)):
                    ImageDraw.floodfill(t, c, (255, 0, 255, 0), thresh=40)
                side = max(t.size); sq = Image.new('RGBA', (side, side), (0, 0, 0, 0))
                sq.paste(t, ((side - t.width) // 2, (side - t.height) // 2)); tile = sq
            else:
                s = min(tile.size); l = (tile.width - s) // 2; u = (tile.height - s) // 2
                tile = tile.crop((l, u, l + s, u + s))
            tile.resize((256, 256), Image.LANCZOS).save(os.path.join(OUT, f'{prefix}_{STYLES[ci]}_{POWERS[ri]}.png'))
    print(prefix, 'ok', xs, ys)

if __name__ == '__main__':
    cut(sys.argv[1], sys.argv[2])
