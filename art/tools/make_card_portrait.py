"""O retrato largo do cartão (docs/concept/hud/kit/photos/card/<nome>.png, 728×300) a partir de uma foto do jogo.

As fotos do cartão do kit são do GPT (estendidas pelo Gemini): uma foto do modelo no deserto, com o fundo e a frente
desfocados (o tilt-shift das miniaturas), a luz quente e os cantos um pouco mais escuros. Medido nas quatro do kit: a média
quente (~170, 147, 130), o desfoque forte em cima (o fundo some) e médio embaixo, o meio nítido. Quando o modelo é nosso e o
GPT não tem a foto dele (a Supply Drop Zone, 23/10), o retrato sai do próprio jogo (a cena hud_drop_zone_portrait_b, sem o HUD,
a câmera perto e mais deitada) com o mesmo acabamento, pra ficar no padrão do cartão da War Factory.

python art/tools/make_card_portrait.py <foto.png> <centro_x> <centro_y> <largura_do_recorte> <saída.png> [<foco_de> <foco_até>]
  o recorte tem a proporção do cartão (728 × 300) e é reduzido pra 728 × 300 (recortar maior que 728 deixa a foto mais nítida);
  a faixa nítida vai de foco_de a foco_até (frações da altura; padrão 0,38 a 0,72, a das fotos do GPT). A Supply Drop Zone usa
  0,12 a 0,80: os paraquedas no alto da foto são o assunto e ficam nítidos
"""
import sys
import numpy as np
from PIL import Image
from scipy import ndimage

src, cx, cy, cw, out = sys.argv[1], float(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4]), sys.argv[5]
F0 = float(sys.argv[6]) if len(sys.argv) > 6 else 0.38
F1 = float(sys.argv[7]) if len(sys.argv) > 7 else 0.72
OW, OH = 728, 300
ch = cw * OH / OW
im = Image.open(src).convert('RGB')
box = (int(round(cx - cw / 2)), int(round(cy - ch / 2)), int(round(cx + cw / 2)), int(round(cy + ch / 2)))
img = np.asarray(im.crop(box).resize((OW, OH), Image.LANCZOS)).astype(np.float64)

# o tilt-shift: o desfoque cresce pra cima (o fundo) e um pouco pra baixo (a frente); o meio (F0..F1 da altura) fica nítido
y = np.linspace(0.0, 1.0, OH)[:, None, None]
top = np.clip((F0 - y) / max(F0, 1e-3), 0, 1) ** 1.3
bot = np.clip((y - F1) / max(1.0 - F1, 1e-3), 0, 1) ** 1.5
levels = [0.0, 1.2, 2.4, 3.8]
blurred = [img] + [np.stack([ndimage.gaussian_filter(img[..., c], s) for c in range(3)], -1) for s in levels[1:]]
amount = np.clip(np.maximum(top * 3.0, bot * 1.6), 0, 3.0)      # 0..3 = qual nível
res = np.zeros_like(img)
for k in range(len(levels)):
    w = np.clip(1.0 - np.abs(amount - k), 0, 1)
    res += blurred[k] * w
img = res

# o meio um pouco mais nítido (a foto do GPT é crocante no foco)
soft = np.stack([ndimage.gaussian_filter(img[..., c], 1.0) for c in range(3)], -1)
focus = (1 - np.clip(amount, 0, 1))
img = img + (img - soft) * 0.45 * focus

# a luz quente e o contraste das fotos do kit: leva a média pra perto delas sem achatar (um ganho por canal e uma curva em S)
target = np.array([170.0, 147.0, 130.0])
mean = img.reshape(-1, 3).mean(0)
gain = np.clip(target / np.maximum(mean, 1.0), 0.85, 1.2)
img = img * gain[None, None, :]
t = np.clip(img / 255.0, 0, 1)
t = t + 0.10 * (t - 0.5) * (1 - np.abs(2 * t - 1))       # o S leve
# a saturação das fotos do kit (o deserto do jogo é mais laranja que o das fotos do GPT: medido, 0,8 aproxima)
lum = (t * np.array([0.2126, 0.7152, 0.0722])[None, None, :]).sum(-1, keepdims=True)
t = lum + (t - lum) * 0.80
img = t * 255.0

# os cantos mais escuros (a vinheta da lente)
yy, xx = np.mgrid[0:OH, 0:OW].astype(np.float64)
d = np.hypot((xx - OW / 2) / (OW / 2), (yy - OH / 2) / (OH / 2))
img *= (1 - 0.16 * np.clip(d - 0.55, 0, 1) ** 1.4)[..., None]

Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).save(out)
print('ok', box, 'gain', gain.round(3))
