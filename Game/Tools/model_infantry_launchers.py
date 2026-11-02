"""Os lança-foguetes da infantaria antitanque, provisórios por script (31/10/2026, o elenco das 3 facções: docs/ELENCO.md).

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_infantry_launchers.py -- captures/game/models/infantry_launchers

O soldado segura a arma no lugar do fuzil (RTSUnitActor: a malha "rifle" do "soldier" vai no osso hand_r, girada com a torre
−90°): a arma é modelada com o cano ao longo de +Y (o eixo do fuzil do modelo da Unreal), o punho na origem, o tubo por cima do
ombro (o lançador antitanque se apoia no ombro, a mão no punho embaixo). Medidas reais:
- RPG-7 (a GLA): o tubo de 40 mm com 0,95 m, o bulbo da ogiva PG-7V de 85 mm saindo na frente (~0,6 m), o protetor de calor de
  madeira, os 2 punhos, a mira óptica PGO-7 do lado esquerdo;
- Javelin (a USA, o Missile Defender de hoje): o tubo de lançamento de 1,2 m × 0,14 m (com as tampas grossas nas pontas e a cinta), a
  unidade de comando (CLU) do lado esquerdo, a alça;
- PF-98 (o Caçador de tanque da China): o tubo de 120 mm com ~1,2 m, a mira grande em cima, o bipé dobrado, o punho.

Slots (os do import_provisional.py, FACTION_MODELS 'infantry_launchers'): Metal (o aço), Dark, Wood (a madeira do RPG), Paint
(o tubo: o oliva do Javelin e do PF-98; o RPG é aço escuro), Lens (a lente das miras).
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, clear_scene, export  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'infantry_launchers')
os.makedirs(OUT, exist_ok=True)
COLORS.update({'Metal': (60, 60, 58), 'Dark': (30, 30, 28), 'Wood': (120, 78, 42), 'Paint': (78, 84, 58), 'Lens': (40, 60, 80)})

UP = 0.13    # o eixo do tubo acima do punho (o tubo no ombro, a mão embaixo)


def rpg7():
    p = Part('SM_Launcher_RPG7')
    p.cyl('Metal', (0.0, -0.05, UP), 'y', 0.022, 0.95, seg=16, bevel=0.0, smooth=True)            # o tubo de 40 mm
    p.cyl('Metal', (0.0, -0.52, UP), 'y', 0.034, 0.12, seg=16, r2=0.026, bevel=0.0, smooth=True)  # a boca de trás (o sino)
    p.cyl('Wood', (0.0, -0.02, UP), 'y', 0.032, 0.30, seg=16, bevel=0.0, smooth=True)            # o protetor de calor de madeira
    # a ogiva PG-7V: o corpo e o cone
    p.cyl('Paint', (0.0, 0.50, UP), 'y', 0.042, 0.14, seg=20, bevel=0.0, smooth=True)
    p.cyl('Paint', (0.0, 0.64, UP), 'y', 0.042, 0.16, seg=20, r2=0.012, bevel=0.0, smooth=True)
    p.cyl('Dark', (0.0, 0.42, UP), 'y', 0.018, 0.10, seg=12, bevel=0.0)
    # os dois punhos e o gatilho
    p.obox('Wood', (0.0, 0.03, UP - 0.07), (0.03, 0.045, 0.12), roll=0, pitch=0, bevel=0.01)
    p.obox('Wood', (0.0, 0.0, 0.0), (0.03, 0.05, 0.12), bevel=0.01)
    p.box('Metal', -0.012, 0.012, -0.02, 0.05, 0.05, UP - 0.02, bevel=0.0)
    # a mira óptica do lado esquerdo (−X do soldado = o lado de fora)
    p.box('Dark', -0.07, -0.03, -0.08, 0.06, UP + 0.01, UP + 0.05, bevel=0.005)
    p.cyl('Lens', (-0.05, -0.085, UP + 0.03), 'y', 0.012, 0.01, seg=10, bevel=0.0)
    return p.finish()


def javelin():
    p = Part('SM_Launcher_Javelin')
    L, R = 1.2, 0.07
    p.cyl('Paint', (0.0, -0.1, UP + 0.02), 'y', R, L, seg=24, bevel=0.0, smooth=True)              # o tubo de lançamento
    for y in (-0.1 - L / 2 + 0.07, -0.1 + L / 2 - 0.07):
        p.cyl('Paint', (0.0, y, UP + 0.02), 'y', R + 0.018, 0.14, seg=24, bevel=0.0, smooth=True)   # as tampas grossas das pontas
    p.cyl('Dark', (0.0, -0.1 + L / 2 + 0.005, UP + 0.02), 'y', R * 0.8, 0.01, seg=20, bevel=0.0)
    p.cyl('Dark', (0.0, 0.05, UP + 0.02), 'y', R + 0.008, 0.04, seg=24, bevel=0.0)                  # a cinta
    # a unidade de comando (CLU): a caixa do lado esquerdo com a lente grande na frente e o visor atrás
    p.box('Paint', -0.25, -0.08, -0.12, 0.10, UP - 0.06, UP + 0.11, bevel=0.012)
    p.box('Dark', -0.25, -0.08, 0.10, 0.12, UP - 0.04, UP + 0.09, bevel=0.0)
    p.cyl('Lens', (-0.165, 0.125, UP + 0.025), 'y', 0.05, 0.012, seg=16, bevel=0.0)
    p.box('Dark', -0.21, -0.12, -0.18, -0.12, UP + 0.0, UP + 0.07, bevel=0.005)                     # o visor
    # o punho e a alça
    p.obox('Dark', (0.0, 0.0, 0.0), (0.035, 0.05, 0.12), bevel=0.01)
    p.box('Metal', -0.012, 0.012, -0.02, 0.04, 0.05, UP - R + 0.02, bevel=0.0)
    p.obox('Dark', (0.0, -0.1, UP + R + 0.03), (0.03, 0.5, 0.02), bevel=0.0)
    return p.finish()


def pf98():
    p = Part('SM_Launcher_PF98')
    L, R = 1.2, 0.068
    p.cyl('Paint', (0.0, -0.08, UP + 0.02), 'y', R, L, seg=24, bevel=0.0, smooth=True)              # o tubo de 120 mm
    p.cyl('Dark', (0.0, -0.08 - L / 2 - 0.03, UP + 0.02), 'y', R + 0.02, 0.08, seg=24, r2=R + 0.035, bevel=0.0)  # o sino de trás
    p.cyl('Paint', (0.0, -0.08 + L / 2 - 0.05, UP + 0.02), 'y', R + 0.012, 0.1, seg=24, bevel=0.0)
    p.cyl('Dark', (0.0, -0.08 + L / 2 + 0.005, UP + 0.02), 'y', R * 0.85, 0.01, seg=20, bevel=0.0)
    # a mira grande em cima (o corpo da mira com a lente na frente e o protetor de olho atrás)
    p.box('Dark', -0.035, 0.035, -0.06, 0.16, UP + R + 0.03, UP + R + 0.11, bevel=0.01)
    p.cyl('Lens', (0.0, 0.165, UP + R + 0.07), 'y', 0.03, 0.01, seg=14, bevel=0.0)
    p.cyl('Dark', (0.0, -0.08, UP + R + 0.07), 'y', 0.028, 0.06, seg=12, bevel=0.0)
    for y in (-0.04, 0.12):
        p.box('Metal', -0.012, 0.012, y - 0.012, y + 0.012, UP + R - 0.005, UP + R + 0.035, bevel=0.0)
    # o bipé dobrado embaixo da frente e o punho
    for s in (-1, 1):
        p.obox('Metal', (s * 0.03, 0.30, UP - R - 0.02), (0.012, 0.30, 0.012), roll=s * 8, bevel=0.0)
    p.obox('Dark', (0.0, 0.0, 0.0), (0.035, 0.05, 0.12), bevel=0.01)
    p.box('Metal', -0.012, 0.012, -0.02, 0.04, 0.05, UP - R + 0.02, bevel=0.0)
    return p.finish()


def main():
    clear_scene()
    for build in (rpg7, javelin, pf98):
        o = build()
        export(o, OUT)


main()
