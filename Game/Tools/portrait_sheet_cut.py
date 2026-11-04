"""Recorta a FOLHA EM GRADE de retratos do Atelier e põe cada célula no kit do HUD (04/11/2026). A folha é o jeito barato de gerar
("vc torrou tudo, não tinha como gerar vários em uma imagem?"): várias peças numa imagem, separadas por calhas pretas; aqui cada
célula vira o botão (256 x 256, o quadrado do meio da célula) ou o cartão (728 x 300, o recorte de 2,43:1 da célula).

  python Game/Tools/portrait_sheet_cut.py <folha.png> buttons|card <nome1> <nome2> ... [--pos 0.5] [--skip nome]

- as células saem na ordem de leitura (linha por linha, da esquerda pra direita); "-" no lugar de um nome pula a célula;
- as calhas: as linhas e colunas inteiras escuras (a média < 22 de 255) separam as células; as faixas pretas das bordas saem;
- --pos: a altura do corte do cartão (0 = o alto da célula, 1 = o pé; 0,5 = o meio);
- o nome é o do PhotoName do RTSHUDNew.cpp (a chave da unidade; a Patriot é usa_missile_defense, o M1 usa_battle_tank).
Antes de sobrescrever, a peça antiga vai pra photos/_old/<pasta>/<nome>.png.
"""
import os
import shutil
import sys

import numpy as np
from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
KIT = os.path.join(REPO, 'docs', 'concept', 'hud', 'kit', 'photos')


def runs(mask):
    """Os trechos [a, b) onde mask é verdadeiro."""
    out, a = [], None
    for i, v in enumerate(list(mask) + [False]):
        if v and a is None:
            a = i
        elif not v and a is not None:
            out.append((a, i))
            a = None
    return out


def cells(img):
    """As linhas pelas calhas da largura inteira (uma faixa escura dentro de uma célula não corta a linha: o BMPT na sombra, 04/11) e,
    dentro de cada linha, as colunas (a célula preta vazia no fim da folha some sozinha)."""
    g = np.asarray(img.convert('L'), dtype=np.float32)
    out = []
    for y0, y1 in [r for r in runs(g.mean(axis=1) >= 6) if r[1] - r[0] > 60]:   # a calha é preta (~0–3); a linha com células pretas vazias fica acima de 6
        for x0, x1 in [r for r in runs(g[y0:y1].mean(axis=0) >= 22) if r[1] - r[0] > 60]:
            out.append((x0, y0, x1, y1))
    return out


def main():
    args = sys.argv[1:]
    pos = 0.5
    if '--pos' in args:
        i = args.index('--pos')
        pos = float(args[i + 1])
        del args[i:i + 2]
    sheet, kind, names = args[0], args[1], args[2:]
    img = Image.open(sheet).convert('RGB')
    cs = cells(img)
    if len(cs) != len(names):
        print('SHEET_CUT células', len(cs), 'nomes', len(names), cs)
        sys.exit(1)
    for (x0, y0, x1, y1), name in zip(cs, names):
        if name == '-':
            continue
        c = img.crop((x0 + 2, y0 + 2, x1 - 2, y1 - 2))
        w, h = c.size
        if kind == 'buttons':
            s = min(w, h)
            box = ((w - s) // 2, 0, (w - s) // 2 + s, s) if w >= h else (0, int((h - s) * pos), s, int((h - s) * pos) + s)
            out = c.crop(box).resize((256, 256), Image.LANCZOS)
        else:
            ch = min(h, int(round(w / (728 / 300))))
            cw = int(round(ch * 728 / 300))
            x = (w - cw) // 2
            y = int((h - ch) * pos)
            out = c.crop((x, y, x + cw, y + ch)).resize((728, 300), Image.LANCZOS)
        dst = os.path.join(KIT, kind, name + '.png')
        if os.path.exists(dst):
            old = os.path.join(KIT, '_old', kind)
            os.makedirs(old, exist_ok=True)
            shutil.copy2(dst, os.path.join(old, name + '.png'))
        out.save(dst)
        print('SHEET_CUT', name, (x0, y0, x1, y1), '->', dst)


if __name__ == '__main__':
    main()
