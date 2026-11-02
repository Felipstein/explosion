"""A moldura e o fundo da tela de poderes (General's Command) de cada facção, no tamanho da janela (01/11, noite).

Antes a moldura era a peça larga do HUD (`skin/<facção>/panel_wide.png`, 928×223) fatiada em 9 pelo CSS com o mesmo corte nas quatro:
o corte passava no meio dos enfeites (as cantoneiras da China, as placas da Rússia, os rebites da GLA) e o resto esticava pela borda
(ele: "os cantos estão ESTICADOS"). Aqui a moldura é montada já no tamanho da janela na tela (1260×680 com zoom 1,25 = 1575×850):
os quatro cantos inteiros no tamanho natural, a borda entre eles esticada só ao longo dela (ou repetida, quando tem rebites), e o miolo
vazado. O fundo é uma textura gerada sem emenda e sem espelho (o espelhado fazia um X), no tom da face de cada facção.

Uso: python art/tools/make_gen_frame.py   (grava em captures/concept/hud/recursos2/a/<facção>/gen_frame.png e gen_bg.png)
"""
import os
import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
KIT = os.path.join(ROOT, 'docs/concept/hud/kit/skin')
OUT = os.path.join(ROOT, 'captures/concept/hud/recursos2/a')
W0, H0 = 1575, 850   # a janela na tela: 1260×680 CSS com o zoom 1,25 (o conteúdo igual nas quatro, com a margem de respiro dentro)

# por facção: a borda de verdade (até a face: cima, baixo, esquerda, direita), o canto inteiro (largura, altura), como a borda de cima e
# de baixo se estende ('stretch' ou 'tile' quando tem rebites espaçados), e a cor da face pro fundo
FAC = {
    'usa':    dict(t=(23, 18, 22, 20), corner=(40, 40), h='stretch', face=(14, 22, 31)),
    'china':  dict(t=(32, 32, 46, 46), corner=(112, 72), h='stretch', face=(13, 11, 11)),
    'gla':    dict(t=(35, 32, 39, 38), corner=(74, 74), h='tile', face=(46, 48, 30)),
    'russia': dict(t=(32, 30, 28, 28), corner=(206, 44), h='stretch', face=(22, 25, 29)),
}


def frame(fac, p):
    src = np.asarray(Image.open(os.path.join(KIT, '' if fac == 'usa' else fac, 'panel_wide.png')).convert('RGBA')).copy()
    sh, sw = src.shape[:2]
    T, B, L, R = p['t']
    cw, ch = p['corner']
    # o miolo vazado: tudo além da borda; nos cantos, só o que tem a cor da face (o enfeite fica)
    face = np.median(src[sh // 2 - 20:sh // 2 + 20, sw // 2 - 100:sw // 2 + 100, :3].reshape(-1, 3), axis=0)
    inner = np.zeros((sh, sw), bool)
    inner[T:sh - B, L:sw - R] = True
    close = np.abs(src[..., :3].astype(float) - face).sum(axis=2) < 60
    corner_zone = np.zeros((sh, sw), bool)
    for ys in (slice(0, ch), slice(sh - ch, sh)):
        for xs in (slice(0, cw), slice(sw - cw, sw)):
            corner_zone[ys, xs] = True
    hollow = inner & (~corner_zone | close)
    hollow = ndimage.binary_opening(hollow, iterations=1) | (inner & ~corner_zone)
    src[hollow, 3] = 0
    img = Image.fromarray(src, 'RGBA')

    # 01/11, 2ª volta (ele: cada facção com uma altura diferente do título até o topo): a moldura fica POR FORA da janela; a janela (e o
    # conteúdo) é igual nas quatro, e a imagem cresce a espessura da borda de cada facção pra fora
    W, H = W0 + L + R, H0 + T + B
    out = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    crop = lambda x0, y0, x1, y1: img.crop((x0, y0, x1, y1))
    # as bordas: de cima e de baixo na largura que sobra entre os cantos; dos lados na altura que sobra
    mid_w, mid_h = W - 2 * cw, H - 2 * ch
    for y0, y1, oy in ((0, ch, 0), (sh - ch, sh, H - ch)):
        strip = crop(cw, y0, sw - cw, y1)
        if p['h'] == 'tile':
            n = max(1, round(mid_w / strip.width))
            tile = strip.resize((mid_w // n, ch), Image.LANCZOS)
            for k in range(n):
                out.alpha_composite(tile, (cw + k * tile.width, oy))
            rest = mid_w - n * tile.width
            if rest:
                out.alpha_composite(tile.crop((0, 0, rest, ch)), (cw + n * tile.width, oy))
        else:
            out.alpha_composite(strip.resize((mid_w, ch), Image.LANCZOS), (cw, oy))
    for x0, x1, ox in ((0, cw, 0), (sw - cw, sw, W - cw)):
        # dos lados, uma faixa lisa do meio da borda estendida na altura (esticar a borda inteira levava junto a lâmpada e a faixa de
        # perigo da GLA)
        row = crop(x0, sh // 2 - 2, x1, sh // 2 + 2)
        out.alpha_composite(row.resize((cw, mid_h), Image.BILINEAR), (ox, ch))
    for x0, y0, ox, oy in ((0, 0, 0, 0), (sw - cw, 0, W - cw, 0), (0, sh - ch, 0, H - ch), (sw - cw, sh - ch, W - cw, H - ch)):
        out.alpha_composite(crop(x0, y0, x0 + cw, y0 + ch), (ox, oy))
    return out


def background(p, seed):
    """Metal escovado sem emenda: ruído esticado na horizontal com o embrulho das bordas, riscos finos e um mosqueado leve."""
    rng = np.random.default_rng(seed)
    n = 512
    brush = ndimage.gaussian_filter(rng.standard_normal((n, n)), (0.6, 30), mode='wrap')
    mottle = ndimage.gaussian_filter(rng.standard_normal((n, n)), 40, mode='wrap')
    brush /= brush.std(); mottle /= mottle.std()
    scr = np.zeros((n, n))
    for _ in range(70):
        x, y, ang, ln = rng.uniform(0, n), rng.uniform(0, n), rng.uniform(-0.5, 0.5) + rng.choice([0, np.pi / 2.6]), rng.uniform(20, 120)
        for t in np.linspace(0, ln, int(ln * 2)):
            scr[int(y + np.sin(ang) * t) % n, int(x + np.cos(ang) * t) % n] += rng.uniform(0.3, 1.0)
    scr = ndimage.gaussian_filter(scr, 0.5, mode='wrap')
    v = 1 + 0.05 * brush + 0.06 * mottle + 0.35 * np.clip(scr, 0, 1)
    rgb = np.clip(np.array(p['face'], float)[None, None, :] * v[..., None], 0, 255).astype('uint8')
    return Image.fromarray(rgb, 'RGB')


if __name__ == '__main__':
    for i, (fac, p) in enumerate(FAC.items()):
        d = OUT if fac == 'usa' else os.path.join(OUT, fac)
        frame(fac, p).save(os.path.join(d, 'gen_frame.png'))
        background(p, 7 + i).save(os.path.join(d, 'gen_bg.png'))
        T, B, L, R = p['t']
        print(fac, 'moldura', (W0 + L + R, H0 + T + B), 'borda em px de CSS (cima, baixo, esq., dir.)', [round(v * 0.8, 1) for v in (T, B, L, R)])
