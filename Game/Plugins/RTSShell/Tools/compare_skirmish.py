# Compara a foto do skirmish nativo (a foto interna do jogo, 1920 × 1080) com o mockup no Chrome: a folha com o mockup, o nativo e a
# diferença ampliada (4×), e embaixo dois recortes em tamanho real lado a lado (mockup | nativo); imprime a diferença média por região.
# Uso: python compare_skirmish.py <mockup.jpg> <nativo.png> <saida.jpg> [recorte1 x,y,w,h] [recorte2 x,y,w,h]
#   sala: os recortes padrão são a coluna das vagas e o losango; o navegador: os filtros e os cartões.
import sys
import numpy as np
from PIL import Image

a = np.asarray(Image.open(sys.argv[1]).convert('RGB').resize((1920, 1080)), np.float32)
b = np.asarray(Image.open(sys.argv[2]).convert('RGB').resize((1920, 1080)), np.float32)
d = np.abs(a - b).mean(axis=2)
regions = {'coluna (0–800)': (slice(0, 1080), slice(0, 800)), 'direita (800–1920)': (slice(0, 1080), slice(800, 1920)), 'tudo': (slice(0, 1080), slice(0, 1920))}
for name, (ys, xs) in regions.items():
    print(f'{name}: diferença média {d[ys, xs].mean():.2f} (0–255), brilho mockup {a[ys, xs].mean():.1f} nativo {b[ys, xs].mean():.1f}')
# as faixas de 120 px com mais diferença (onde olhar primeiro)
cells = []
for y in range(0, 1080, 120):
    for x in range(0, 1920, 120):
        cells.append((d[y:y + 120, x:x + 120].mean(), x, y))
cells.sort(reverse=True)
print('mais diferentes (x, y, média):', ', '.join(f'({x},{y}) {m:.1f}' for m, x, y in cells[:8]))
heat = np.clip(d * 4, 0, 255).astype(np.uint8)
half = lambda x: Image.fromarray(x.astype(np.uint8)).resize((960, 540), Image.LANCZOS)
boxes = [tuple(int(v) for v in s.split(',')) for s in sys.argv[4:6]] or [(100, 360, 700, 300), (900, 80, 900, 300)]
h = max(bh for _, _, _, bh in boxes)
out = Image.new('RGB', (2880, 544 + (h + 4) * len(boxes)), (0, 0, 0))
out.paste(half(a), (0, 0)); out.paste(half(b), (960, 0)); out.paste(half(np.dstack([heat, heat, heat])), (1920, 0))
y0 = 544
for (x, y, w, bh) in boxes:
    out.paste(Image.fromarray(a[y:y + bh, x:x + w].astype(np.uint8)), (0, y0))
    out.paste(Image.fromarray(b[y:y + bh, x:x + w].astype(np.uint8)), (w + 8, y0))
    y0 += bh + 4
out.save(sys.argv[3], quality=88)
print('folha:', sys.argv[3])
