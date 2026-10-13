"""A leitura das facções à noite (11/10): o valor do teto de cada tanque, na arte e na maquete.

A crítica da sessão de arte: a facção se lê pelo valor em qualquer luz (a USA clara perto e longe do
fogo, a China escura). Aqui, pra cada tanque da planta, a média do teto da torre (a planta projetada na
câmera da arte, uma caixinha em volta do ponto a 2,5 m) nas duas imagens, com a distância até o fogo
mais perto.

  python Game/Tools/look_units.py <repo> <render.png>      (LOOK_SCENE=desert_night)
"""
import colorsys
import math
import os
import sys

import numpy as np
from PIL import Image

REPO, RENDER = sys.argv[1], sys.argv[2]
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import look_scene  # noqa: E402

W, H = 2048, 1144
CAM = dict(look_scene.CAM)
CAM['pitch'] = -CAM['pitch']


def project(P):
    t, p = math.radians(CAM['pitch']), math.radians(CAM['yaw'])
    F = np.array([math.cos(t) * math.cos(p), math.cos(t) * math.sin(p), -math.sin(t)])
    R = np.array([-math.sin(p), math.cos(p), 0.0])
    U = np.array([math.sin(t) * math.cos(p), math.sin(t) * math.sin(p), math.cos(t)])
    f = (W / 2) / math.tan(math.radians(CAM['hfov'] / 2))
    d = np.asarray(P, float) - np.array([CAM['x'], CAM['y'], CAM['z']])
    zc = d @ F
    return W / 2 + f * (d @ R) / zc, H / 2 - f * (d @ U) / zc


art = Image.open(os.path.join(REPO, look_scene.ART)).convert('RGB').resize((W, H), Image.LANCZOS)
ren = Image.open(RENDER).convert('RGB').resize((W, H), Image.LANCZOS)
fires = [(f[0], f[1]) for f in getattr(look_scene, 'FIRES', [])]
b = getattr(look_scene, 'BLAST', None)
if b:
    fires.append((b[0], b[1]))
rows = []
for t in look_scene.TANKS:
    x, y = t[0], t[1]
    paint = t[3] if len(t) > 3 else ''
    dmin = min(math.hypot(x - fx, y - fy) for fx, fy in fires) if fires else 99
    if dmin < 2.0:
        continue        # o tanque que está pegando fogo
    u, v = project((x, y, 2.5))
    box = (int(u - 9), int(v - 7), int(u + 9), int(v + 7))
    out = []
    for img in (art, ren):
        c = np.asarray(img.crop(box)).reshape(-1, 3).mean(0) / 255
        h, l, s = colorsys.rgb_to_hls(*c)
        out.append((l, h * 360, s))
    rows.append((paint, dmin, out))
for paint, d, (a, r) in sorted(rows, key=lambda q: (q[0], q[1])):
    print(f'{paint:12s} fogo a {d:4.1f} m | arte L {a[0]:.2f} matiz {a[1]:4.0f} | maquete L {r[0]:.2f} matiz {r[1]:4.0f}')
