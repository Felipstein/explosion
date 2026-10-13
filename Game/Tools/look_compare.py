"""Folhas de comparação do look: a arte contra a maquete, na mesma câmera, com números.

Rodar: Game/Tools/look.ps1 compare   (Python do sistema: numpy + pillow)

Lê os quadros do Movie Render Queue (captures/look/Seq_<variante>/): a sequência
corta entre as câmeras a cada 45 quadros (arte, jogo padrão, jogo perto) e a foto
de cada câmera é o último quadro do trecho (o Lumen e o TSR já assentados).

Números (iguais pra arte e pro render, na mesma região: sem a explosão da arte):
- brilho médio e contraste (desvio ÷ média da luminância);
- saturação média;
- cor da sombra e da luz: média dos 15% mais escuros e dos 15% mais claros, e o
  vermelho ÷ azul de cada (acima de 1 = quente);
- o ar: a faixa de cima (fundo) contra a de baixo (frente), em brilho e contraste.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

REPO = sys.argv[1] if len(sys.argv) > 1 else os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
CAP = os.path.join(REPO, 'captures', 'look')
ART = os.path.join(REPO, 'docs', 'concept', 'scenes', 'desert_golden_hour', 'best', 'day_game_cam.webp')
VARIANTS = [('Before', 'MAQUETE · a luz do jogo hoje'), ('Golden', 'MAQUETE · a receita do deserto dourado')]
CAMS = [('Art', 'câmera da arte (29°, 65°, 26 m)'), ('GameDefault', 'câmera do jogo (37,5°, 50°, 61 m)'),
        ('GameNear', 'câmera do jogo no zoom perto (37,5°, 50°, 32 m)')]
PER = 45
W, H = 1024, 572
EXPLOSION = (330, 0, 640, 300)      # a região da explosão na arte (fora das contas)

F = lambda s: ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf', s)
Fr = lambda s: ImageFont.truetype('C:/Windows/Fonts/arial.ttf', s)
BG, TXT, HI, WARN = (18, 18, 18), (235, 235, 235), (120, 230, 140), (255, 200, 90)


def frames(variant):
    """A foto de cada câmera: o último quadro do trecho dela. Sem o render completo, usa o
    rápido (Seq_<variante>_Q: só a câmera da arte)."""
    subs = [(f'Seq_{variant}', CAMS), (f'Seq_{variant}_Q', CAMS[:1])]
    # o render mais novo primeiro (o rápido de uma volta de ajuste vale mais que o completo velho)
    subs.sort(key=lambda t: -os.path.getmtime(os.path.join(CAP, t[0])) if os.path.isdir(os.path.join(CAP, t[0])) else 0)
    for sub, cams in subs:
        d = os.path.join(CAP, sub)
        fs = sorted(f for f in os.listdir(d) if f.lower().endswith('.png')) if os.path.isdir(d) else []
        if not fs:
            continue
        out = {}
        for i, (cam, _) in enumerate(cams):
            want = (i + 1) * PER - 1
            pick = [f for f in fs if int(f.split('.')[-2]) <= want]
            if pick:
                out[cam] = os.path.join(d, pick[-1])
        return out
    return None


def metrics(img):
    a = np.asarray(img.convert('RGB').resize((W, H), Image.LANCZOS)).astype(np.float64) / 255.0
    mask = np.ones((H, W), bool)
    x0, y0, x1, y1 = EXPLOSION
    mask[y0:y1, x0:x1] = False
    L = 0.2126 * a[..., 0] + 0.7152 * a[..., 1] + 0.0722 * a[..., 2]
    mx, mn = a.max(-1), a.min(-1)
    sat = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-6), 0)
    Lm = L[mask]
    lo, hi = np.percentile(Lm, 15), np.percentile(Lm, 85)
    dark = a[mask & (L <= lo)].mean(0)
    light = a[mask & (L >= hi)].mean(0)

    def band(y0, y1):
        m = mask[y0:y1]
        l = L[y0:y1][m]
        return l.mean(), l.std() / max(l.mean(), 1e-6)
    tb, tc = band(0, int(H * 0.2))
    bb, bc = band(int(H * 0.8), H)
    return {
        'brilho': Lm.mean(), 'contraste': Lm.std() / Lm.mean(), 'saturação': sat[mask].mean(),
        'sombra': dark, 'luz': light,
        'sombra_rb': dark[0] / max(dark[2], 1e-6), 'luz_rb': light[0] / max(light[2], 1e-6),
        'sombra_luz': (0.2126 * dark[0] + 0.7152 * dark[1] + 0.0722 * dark[2]) /
                      max(0.2126 * light[0] + 0.7152 * light[1] + 0.0722 * light[2], 1e-6),
        'fundo_brilho': tb / max(bb, 1e-6), 'fundo_contraste': tc / max(bc, 1e-6),
    }


def fmt(v):
    return f'{v:.2f}'.replace('.', ',')


def text_block(d, x, y, m, ref=None):
    rows = [('brilho médio', 'brilho'), ('contraste', 'contraste'), ('saturação', 'saturação'),
            ('sombra: vermelho÷azul', 'sombra_rb'), ('luz: vermelho÷azul', 'luz_rb'),
            ('sombra ÷ luz (brilho)', 'sombra_luz'), ('fundo ÷ frente (brilho)', 'fundo_brilho'),
            ('fundo ÷ frente (contraste)', 'fundo_contraste')]
    for i, (label, key) in enumerate(rows):
        v = m[key]
        col = TXT
        s = f'{label}: {fmt(v)}'
        if ref is not None:
            diff = (v - ref[key]) / max(abs(ref[key]), 1e-6)
            s += f'   ({"+" if diff >= 0 else ""}{int(round(diff * 100))}% da arte)'
            col = HI if abs(diff) < 0.1 else (WARN if abs(diff) < 0.3 else (255, 120, 110))
        d.text((x, y + i * 24), s, fill=col, font=Fr(19))
    # amostras da cor da sombra e da luz
    for j, key in enumerate(('sombra', 'luz')):
        c = tuple(int(255 * min(1, max(0, t))) for t in m[key])
        d.rectangle((x + j * 90, y + 8 * 24 + 6, x + j * 90 + 80, y + 8 * 24 + 46), fill=c)
        d.text((x + j * 90, y + 8 * 24 + 50), key, fill=TXT, font=Fr(16))


def sheet_art_cam(art, shots):
    cols = [('SUA ARTE (Gemini, o alvo nº 1)', art)] + [(lab, Image.open(shots[v]['Art'])) for v, lab in VARIANTS if shots.get(v)]
    tw, th = 760, int(760 * H / W)
    im = Image.new('RGB', (20 + len(cols) * (tw + 20), 70 + th + 330), BG)
    d = ImageDraw.Draw(im)
    d.text((20, 18), 'O DIA NA CÂMERA DA ARTE · a maquete contra a arte (a luz; ainda sem picapes, blindados, o tanque da China, soldados e a explosão)',
           fill=HI, font=F(24))
    ref = metrics(art)
    for i, (lab, img) in enumerate(cols):
        x = 20 + i * (tw + 20)
        im.paste(img.convert('RGB').resize((tw, th), Image.LANCZOS), (x, 60))
        d.text((x, 60 + th + 10), lab, fill=HI if i == 0 else TXT, font=F(20))
        text_block(d, x, 60 + th + 44, metrics(img), None if i == 0 else ref)
    p = os.path.join(CAP, 'folha_camera_da_arte.png')
    im.save(p)
    return p


def sheet_game_cams(shots):
    rows = [(v, lab) for v, lab in VARIANTS if shots.get(v)]
    tw, th = 760, int(760 * H / W)
    im = Image.new('RGB', (20 + 3 * (tw + 20), 60 + len(rows) * (th + 50)), BG)
    d = ImageDraw.Draw(im)
    d.text((20, 16), 'AS TRÊS CÂMERAS · em cima a luz do jogo hoje, embaixo a receita do deserto dourado', fill=HI, font=F(24))
    for r, (v, lab) in enumerate(rows):
        for c, (cam, clab) in enumerate(CAMS):
            if cam not in shots[v]:
                continue
            x, y = 20 + c * (tw + 20), 56 + r * (th + 50)
            im.paste(Image.open(shots[v][cam]).convert('RGB').resize((tw, th), Image.LANCZOS), (x, y))
            d.text((x, y + th + 8), f'{lab.split("·")[1].strip()} · {clab}', fill=TXT, font=Fr(18))
    p = os.path.join(CAP, 'folha_tres_cameras.png')
    im.save(p)
    return p


def flicker(art, shots):
    """Arte e maquete alternando (a cada 0,6 s): pra conferir o alinhamento da câmera."""
    for v, _ in VARIANTS:
        if not shots.get(v):
            continue
        a = art.convert('RGB').resize((W, H))
        b = Image.open(shots[v]['Art']).convert('RGB').resize((W, H))
        p = os.path.join(CAP, f'alinhamento_{v}.gif')
        a.save(p, save_all=True, append_images=[b], duration=600, loop=0)


def main():
    art = Image.open(ART)
    shots = {v: frames(v) for v, _ in VARIANTS}
    if not any(shots.values()):
        print('sem renders em', CAP)
        return
    print(sheet_art_cam(art, shots))
    print(sheet_game_cams(shots))
    flicker(art, shots)
    m = metrics(art)
    print('arte:', {k: (round(v, 3) if np.isscalar(v) else np.round(v, 3).tolist()) for k, v in m.items()})
    for v, _ in VARIANTS:
        if shots.get(v):
            mm = metrics(Image.open(shots[v]['Art']))
            print(v + ':', {k: (round(x, 3) if np.isscalar(x) else np.round(x, 3).tolist()) for k, x in mm.items()})


main()
