"""A planta da rua do mapa Look_Desert (R asfalto, G areia por cima, B terra batida, A faixa pintada).
Python do sistema (numpy, PIL); o build_look_desert.py (Python da Unreal) só importa o PNG.
Rodar: python BlastLab/Tools/look_desert_layout.py"""
import os

import numpy as np
from PIL import Image

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
GEN = os.path.join(REPO, 'BlastLab', 'Intermediate', 'LookDesert')
LX, LY, LS, LPX = -80.0, -80.0, 160.0, 1024      # os mesmos do build_look_desert.py


def layout_png():
    os.makedirs(GEN, exist_ok=True)
    path = os.path.join(GEN, 'T_LookDesert_Layout.png')
    m = (np.arange(LPX) + 0.5) / LPX * LS
    x, y = np.meshgrid(LX + m, LY + m, indexing='xy')         # [linha = y][coluna = x]
    rng = np.random.default_rng(7)
    noise = rng.random((LPX // 16, LPX // 16))
    noise = np.kron(noise, np.ones((16, 16)))
    edge = 4.2 + 0.5 * (noise - 0.5)                            # a borda do asfalto gasta e torta
    road = (np.abs(y) < edge).astype(np.float32)
    # a rua de terra que sai da rua pra dentro da vila (as artes: os becos entre as casas)
    alley = ((np.abs(x - 22.0) < 3.0) & (y > 4.0) & (y < 40.0)).astype(np.float32)
    alley += ((np.abs(x + 30.0) < 3.0) & (y < -4.0) & (y > -40.0)).astype(np.float32)
    # a areia por cima do asfalto: nas bordas e em faixas de vento
    sandover = np.clip((np.abs(y) - (edge - 1.4)) / 1.4, 0, 1) * road
    sandover = np.maximum(sandover, road * np.clip((noise - 0.72) * 3.0, 0, 1))
    # a faixa pintada tracejada (3 m pintados, 3 m sem)
    paint = ((np.abs(y) < 0.12) & ((np.floor((x + 1000.0) / 3.0) % 2) == 0)).astype(np.float32)
    img = np.stack([road, sandover, np.clip(alley, 0, 1), paint], -1)
    Image.fromarray((img * 255).astype(np.uint8), 'RGBA').save(path)
    return path




if __name__ == '__main__':
    print(layout_png())
