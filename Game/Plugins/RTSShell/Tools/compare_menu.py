# Compara a foto do menu nativo (a foto interna do jogo, 1920 × 1080) com o mockup no Chrome no mesmo instante do radar: a folha
# lado a lado (mockup | nativo | diferença ampliada) e a diferença média por região (o lado do texto, o painel do radar, tudo).
# Uso: python compare_menu.py <mockup.jpg> <nativo.png> <saida.jpg>
import sys
import numpy as np
from PIL import Image

a = np.asarray(Image.open(sys.argv[1]).convert('RGB').resize((1920, 1080)), np.float32)
b = np.asarray(Image.open(sys.argv[2]).convert('RGB').resize((1920, 1080)), np.float32)
d = np.abs(a - b).mean(axis=2)
regions = {'texto (0–760)': (slice(0, 1080), slice(0, 760)), 'radar (818–1866)': (slice(64, 1016), slice(818, 1866)), 'tudo': (slice(0, 1080), slice(0, 1920))}
for name, (ys, xs) in regions.items():
    la, lb = a[ys, xs].mean(), b[ys, xs].mean()
    print(f'{name}: diferença média {d[ys, xs].mean():.2f} (0–255), brilho mockup {la:.1f} nativo {lb:.1f}')
heat = np.clip(d * 4, 0, 255).astype(np.uint8)
sheet = Image.new('RGB', (1920 * 3 // 2, 1080 // 2 * 2 + 4), (0, 0, 0))
half = lambda x: Image.fromarray(x.astype(np.uint8)).resize((960, 540), Image.LANCZOS)
sheet.paste(half(a), (0, 0)); sheet.paste(half(b), (960, 0))
sheet.paste(half(np.dstack([heat, heat, heat])), (1920, 0)) if sheet.width > 1920 else None
# embaixo: o recorte do texto em tamanho real, lado a lado
crop = lambda x: Image.fromarray(x[130:710, 90:690].astype(np.uint8))
out = Image.new('RGB', (2880, 540 + 584), (0, 0, 0))
out.paste(half(a), (0, 0)); out.paste(half(b), (960, 0)); out.paste(half(np.dstack([heat, heat, heat])), (1920, 0))
out.paste(crop(a), (0, 544)); out.paste(crop(b), (604, 544))
out.save(sys.argv[3], quality=88)
print('folha:', sys.argv[3])
