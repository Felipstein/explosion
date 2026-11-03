# Recorta uma folha de ícones em grade com as frestas de verde puro (#00FF00) e salva um PNG de 256 px por ícone, na ordem dada.
#   python docs/poderes/tela/cut_sheet.py <folha> <sufixo> id1,id2,...  -> docs/poderes/tela/icones/usa_<id>_<sufixo>.png
import os, sys
from PIL import Image

def runs(prof, thr, min_len):
    out, s = [], None
    for i, v in enumerate(prof + [0]):
        if v > thr and s is None: s = i
        elif v <= thr and s is not None:
            if i - s >= min_len: out.append((s, i))
            s = None
    return out

def cut(path, suffix, ids):
    im = Image.open(path).convert('RGB'); w, h = im.size; px = im.load()
    green = lambda c: c[1] > 180 and c[0] < 110 and c[2] < 110
    cols = [sum(1 for y in range(0, h, 3) if not green(px[x, y])) for x in range(w)]
    rows = [sum(1 for x in range(0, w, 3) if not green(px[x, y])) for y in range(h)]
    xs = runs(cols, (h / 3) * 0.15, w // 12); ys = runs(rows, (w / 3) * 0.15, h // 6)
    cells = []
    for (y0, y1) in ys:
        for (x0, x1) in xs:
            tile = im.crop((x0, y0, x1, y1))
            b = tile.resize((16, 16)).tobytes()
            if sum(1 for k in range(0, len(b), 3) if green(b[k:k + 3])) > 100: continue   # casa vazia (verde)
            cells.append(tile)
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'icones'); os.makedirs(out, exist_ok=True)
    for i, pid in enumerate(ids):
        t = cells[i]; s = min(t.size); l = (t.width - s) // 2; u = (t.height - s) // 2
        t.crop((l + 2, u + 2, l + s - 2, u + s - 2)).resize((256, 256), Image.LANCZOS).save(os.path.join(out, f'usa_{pid}_{suffix}.png'))
    print(suffix, len(cells), 'casas, salvos', len(ids))

if __name__ == '__main__':
    cut(sys.argv[1], sys.argv[2], sys.argv[3].split(','))
