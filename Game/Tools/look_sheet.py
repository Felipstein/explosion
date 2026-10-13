"""A folha final de uma cena do look, pra o Felipe avaliar: arte × maquete e as câmeras do jogo.

  python Game/Tools/look_sheet.py <repo> <cena> <variante>
  (ex.: desert_golden_hour Golden · desert_night Night)

Em captures/look/final_<cena>.jpg:
- em cima, grandes: a arte (o alvo) e a maquete na mesma câmera;
- embaixo: a maquete na câmera do jogo (61 m) e no zoom perto (32 m), e o resumo do ponto a
  ponto (look_match.py: a distância de cor de cada material e a curva de tom).
"""
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFont

REPO, SCENE, VARIANT = sys.argv[1], sys.argv[2], sys.argv[3]
os.environ['LOOK_SCENE'] = SCENE
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import look_scene  # noqa: E402

CAP = os.path.join(REPO, 'captures', 'look')
F = lambda s: ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf', s)
Fr = lambda s: ImageFont.truetype('C:/Windows/Fonts/arial.ttf', s)
BG, TXT, HI = (18, 18, 18), (235, 235, 235), (120, 230, 140)


def frame(seq, i):
    d = os.path.join(CAP, seq)
    fs = sorted(f for f in os.listdir(d) if f.endswith('.png'))
    return Image.open(os.path.join(d, fs[min(i, len(fs) - 1)])).convert('RGB')


art = Image.open(os.path.join(REPO, look_scene.ART)).convert('RGB')
full = f'Seq_{VARIANT}'
shots = [frame(full, 44), frame(full, 89), frame(full, 134)]
W1, H1 = 1180, 659
W2, H2 = 780, 436
im = Image.new('RGB', (40 + 2 * W1 + 20, 70 + H1 + 60 + H2 + 60), BG)
d = ImageDraw.Draw(im)
d.text((20, 18), f'{SCENE} · a arte (o alvo) × a maquete na mesma câmera; embaixo, as câmeras do jogo e o ponto a ponto',
       fill=HI, font=F(26))
for i, (img, t) in enumerate(((art, 'ALVO · a arte aprovada'), (shots[0], 'MAQUETE · a receita, na câmera da arte'))):
    x = 20 + i * (W1 + 20)
    im.paste(img.resize((W1, H1), Image.LANCZOS), (x, 64))
    d.text((x + 8, 70), t, fill=HI, font=F(22))
y = 64 + H1 + 40
for i, (img, t) in enumerate(((shots[1], 'câmera do jogo (37,5°, 61 m)'), (shots[2], 'câmera do jogo no zoom perto (32 m)'))):
    x = 20 + i * (W2 + 20)
    im.paste(img.resize((W2, H2), Image.LANCZOS), (x, y))
    d.text((x + 8, y + 6), t, fill=HI, font=F(20))
m = os.path.join(CAP, f'match_{VARIANT}.json')
if os.path.exists(m):
    M = json.load(open(m))
    x = 20 + 2 * (W2 + 20)
    d.text((x, y), 'PONTO A PONTO (ΔE: < 5 igual a olho)', fill=HI, font=F(20))
    yy = y + 34
    for k, v in M['samples'].items():
        dE = v['dE']
        col = HI if dE < 5 else ((255, 200, 90) if dE < 10 else (255, 120, 110))
        d.rectangle((x, yy, x + 34, yy + 20), fill=tuple(int(255 * c) for c in v['art']))
        d.rectangle((x + 38, yy, x + 72, yy + 20), fill=tuple(int(255 * c) for c in v['rnd']))
        d.text((x + 82, yy), f'{k}: ΔE {dE:.1f}'.replace('.', ','), fill=col, font=Fr(18))
        yy += 26
    ca, cr = M['curve_art'], M['curve_rnd']
    d.text((x, yy + 8), 'curva (p5/p50/p95): arte ' + ' / '.join(f'{ca[i]:.2f}' for i in (1, 4, 7)).replace('.', ','),
           fill=TXT, font=Fr(18))
    d.text((x, yy + 32), 'maquete ' + ' / '.join(f'{cr[i]:.2f}' for i in (1, 4, 7)).replace('.', ','), fill=TXT, font=Fr(18))
out = os.path.join(CAP, f'final_{SCENE}.jpg')
im.save(out, quality=92)
print(out)
