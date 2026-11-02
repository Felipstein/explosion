"""Recorta as molduras grandes e os ícones de uma facção (01/11: China, GLA, Rússia, do GPT pelo Atelier).

Molduras (a folha sobre verde): o minimapa com a coluna dos 4 botões, o cartão da seleção e a caixinha da superarma. O miolo preto do
losango e a janela do cartão viram buraco (o mapa e o retrato vão por baixo), como na USA (`cut_hud_skin.py`). Cada peça vai ao tamanho
da peça da USA, pra o HUD encaixar igual.
Ícones (a grade 4x4 sobre fundo escuro, na ordem da folha da USA): cada casa vira `icons/<facção>/<nome>.png` em 256×256.

Topo 1x1 (a peça sobre verde, Folha I): vira `skin/<facção>/top_bar_1v1.png`, 1087×140 com margem pra peça sair da caixa (`TOP_*`),
os anéis no lugar dos emblemas; sem acalmar o vermelho (o vermelho dessa peça é o do adversário, significado).

Uso: python art/tools/cut_faction_frames.py <folha_molduras> <folha_icones> <facção>
     python art/tools/cut_faction_frames.py --top <folha_topo> <facção>
"""
import os
import sys
import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage
from scipy.spatial import ConvexHull

sys.path.insert(0, os.path.dirname(__file__))
from cut_faction_skin import key_green  # noqa: E402  (tira só o verde ligado à borda)

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
KIT = os.path.join(ROOT, 'docs/concept/hud/kit')
ICONS = ['attack_move', 'guard', 'stop', 'rally_point', 'sell', 'repair', 'armor_upgrade', 'locked',
         'settings', 'idle_units', 'beacon', 'chat', 'orbital_lance', 'artillery_strike', 'paradrop', 'spy_drone']


def hole(im, seed_frac, tol=14):
    """O miolo preto ligado ao ponto (fração da peça) vira transparente."""
    a = np.asarray(im).copy()
    h, w = a.shape[:2]
    dark = a[..., :3].max(axis=2) <= tol
    lab, _ = ndimage.label(dark)
    k = lab[int(h * seed_frac[1]), int(w * seed_frac[0])]
    if k:
        m = ndimage.binary_dilation(lab == k, iterations=1)
        a[m, 3] = 0
    return Image.fromarray(a, 'RGBA')


def calm_red(im, sat, val):
    """O esmalte vermelho mais escuro e menos saturado; o ouro, o aço e o preto não mudam."""
    a = np.asarray(im).astype(float)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    red = (r > 70) & (r > g * 1.9) & (r > b * 1.9) & (a[..., 3] > 200)
    m = ndimage.gaussian_filter(red.astype(float), 1.2)[..., None]
    hsv = np.asarray(Image.fromarray(a[..., :3].astype('uint8')).convert('HSV')).astype(float)
    hsv[..., 1] *= sat
    hsv[..., 2] *= val
    rgb = np.asarray(Image.fromarray(hsv.clip(0, 255).astype('uint8'), 'HSV').convert('RGB')).astype(float)
    a[..., :3] = a[..., :3] * (1 - m) + rgb * m
    return Image.fromarray(a.clip(0, 255).astype('uint8'), 'RGBA')


def frames(path, faction):
    rgba = key_green(np.asarray(Image.open(path).convert('RGB')).astype(float))
    lab, _ = ndimage.label(ndimage.binary_closing(rgba[..., 3] > 40, iterations=2))
    boxes = [s for s in ndimage.find_objects(lab) if (s[0].stop - s[0].start) * (s[1].stop - s[1].start) > 4000]
    ar = lambda s: (s[1].stop - s[1].start) / (s[0].stop - s[0].start)
    sw = max(boxes, key=ar)                                                     # a caixinha: a mais comprida
    rest = [s for s in boxes if s is not sw]
    mm = min(rest, key=lambda s: s[1].start)                                    # o minimapa: o da esquerda
    card = max(rest, key=lambda s: s[1].start)                                  # o cartão: o da direita
    out = os.path.join(KIT, 'skin', faction)
    os.makedirs(out, exist_ok=True)
    for name, s, seed in (('minimap_pod', mm, (0.47, 0.48)), ('unit_card', card, (0.5, 0.33)), ('superweapon_box', sw, None)):
        im = Image.fromarray(rgba[s], 'RGBA')
        if seed:
            im = hole(im, seed)
        im = im.resize(Image.open(os.path.join(KIT, 'skin', name + '.png')).size, Image.LANCZOS)
        if faction == 'china':
            im = calm_red(im, 0.62, 0.7)                                       # o esmalte vermelho calmo (01/11: "cores muito fortes")
        im.save(os.path.join(out, name + '.png'))
        print(name, im.size)


def icons(path, faction):
    a = np.asarray(Image.open(path).convert('RGB')).astype(float)
    bgc = np.median(np.concatenate([a[:8].reshape(-1, 3), a[-8:].reshape(-1, 3)]), axis=0)
    diff = np.linalg.norm(a - bgc, axis=-1) > 28
    lab, _ = ndimage.label(ndimage.binary_closing(diff, iterations=3))
    cells = [s for s in ndimage.find_objects(lab) if (s[0].stop - s[0].start) > a.shape[0] / 8]
    if len(cells) > 16:                                            # algum risco solto: ficam as 16 maiores
        cells = sorted(cells, key=lambda s: -(s[0].stop - s[0].start) * (s[1].stop - s[1].start))[:16]
    cells.sort(key=lambda s: (int((s[0].start + s[0].stop) / 2 / (a.shape[0] / 4)), s[1].start))   # int: o round junta 1,5 e 2,5
    if len(cells) != 16:
        raise SystemExit(f'achei {len(cells)} casas, não 16')
    out = os.path.join(KIT, 'icons', faction)
    os.makedirs(out, exist_ok=True)
    src = Image.open(path).convert('RGBA')
    for name, s in zip(ICONS, cells):
        im = src.crop((s[1].start, s[0].start, s[1].stop, s[0].stop))
        m = Image.fromarray((ndimage.binary_fill_holes(lab[s] > 0) * 255).astype('uint8'))   # fora do botão, transparente
        im.putalpha(m)
        im.resize((256, 256), Image.LANCZOS).save(os.path.join(out, name + '.png'))
    print('ícones', len(cells))


def tube_conduit(path, faction):
    """A folha só com o tubo da patente (o alto) e a fileira de energia (a comprida)."""
    rgba = key_green(np.asarray(Image.open(path).convert('RGB')).astype(float))
    lab, _ = ndimage.label(ndimage.binary_closing(rgba[..., 3] > 40, iterations=2))
    boxes = [s for s in ndimage.find_objects(lab) if (s[0].stop - s[0].start) * (s[1].stop - s[1].start) > 3000]
    tube = max(boxes, key=lambda s: (s[0].stop - s[0].start) / (s[1].stop - s[1].start))
    cond = max(boxes, key=lambda s: (s[1].stop - s[1].start) / (s[0].stop - s[0].start))
    out = os.path.join(KIT, 'skin', faction)
    for name, s in (('rank_tube', tube), ('power_conduit', cond)):
        im = Image.fromarray(rgba[s], 'RGBA')
        if faction == 'china':
            im = calm_red(im, 0.62, 0.7)
        im.save(os.path.join(out, name + '.png'))
        print(name, im.size)


def rings(piece):
    """Os dois anéis das pontas: a face escura e cinza mais redonda de cada quarto das pontas -> [(x, y, raio), (x, y, raio)]."""
    a = piece.astype(float)
    lum, sat = a[..., :3].mean(axis=2), a[..., :3].max(axis=2) - a[..., :3].min(axis=2)
    dark = ndimage.binary_opening((a[..., 3] > 200) & (lum < 75) & (sat < 40), iterations=4)   # solta o anel do painel encostado
    h, w = dark.shape
    out = []
    for x0, x1 in ((0, w // 4), (w - w // 4, w)):
        lab, _ = ndimage.label(dark[:, x0:x1])
        best, area = None, 0
        for k, sl in enumerate(ndimage.find_objects(lab), 1):                  # redonda: quase quadrada, ~78% da caixa, alta
            bh, bw = sl[0].stop - sl[0].start, sl[1].stop - sl[1].start
            n = (lab[sl] == k).sum()
            if bh > 0.35 * h and 0.8 < bw / bh < 1.25 and 0.65 < n / (bh * bw) < 0.92 and n > area:
                cy, cx = ndimage.center_of_mass(lab == k)
                best, area = (x0 + cx, cy, (n / np.pi) ** 0.5), n
        out.append(best)
    return out


# o topo na tela: 1087×140 com os anéis em x 83 e 1006; a peça pode sair da caixa (o pano da GLA, os dentes da China), então a
# textura tem margem: MX de cada lado e MB embaixo (em cima é a beira da tela). O HUD lê a margem pelo tamanho da textura.
TOP_W, TOP_H, TOP_MX, TOP_MB, TOP_RING_Y = 1087, 140, 110, 60, 68


def player_layers(canvas):
    """O azul e o vermelho da arte são a cor de cada jogador (ele, 01/11): o painel de cada um sai numa camada própria (cinza = o
    brilho da arte, alfa = o vidro inteiro um pouco e o contorno aceso todo) que o HUD pinta com a cor do jogador; na base o vidro
    fica neutro e escuro. O painel é achado pelo contorno aceso (azul à esquerda = você, vermelho à direita = o adversário),
    e o painel é o contorno convexo dele (o contorno nem sempre fecha): a janela do relógio, sem contorno aceso, fica de fora."""
    a = np.asarray(canvas).astype(float)
    hsv = np.asarray(canvas.convert('RGB').convert('HSV')).astype(float) / 255.0
    hue, sat, val = hsv[..., 0] * 360, hsv[..., 1], hsv[..., 2]
    x = np.arange(a.shape[1])[None, :] - TOP_MX
    w = np.clip((sat - 0.3) / 0.45, 0, 1) * (a[..., 3] > 40)
    lum = a[..., 0] * 0.3 + a[..., 1] * 0.59 + a[..., 2] * 0.11
    base, layers, kk = a.copy(), [], np.zeros(a.shape[:2])
    for tone, x0, x1 in ((((hue > 185) & (hue < 255)), 110, 560), (((hue > 335) | (hue < 22)), 530, 980)):
        y = np.arange(a.shape[0])[:, None]
        glow = (w > 0.5) & (val > 0.45) & tone & (x > x0) & (x < x1) & (y > 14) & (y < 124)   # longe das faixas da moldura
        m = ndimage.binary_closing(glow, iterations=4)
        lab, n = ndimage.label(m)
        sizes = ndimage.sum(m, lab, range(1, n + 1))
        comp = np.isin(lab, 1 + np.nonzero(sizes > 0.05 * sizes.max())[0])     # o contorno quebrado em pedaços conta inteiro
        ys, xs = np.nonzero(comp)                                                # o painel é um paralelogramo: o contorno convexo
        hull = ConvexHull(np.c_[xs, ys])
        poly = Image.new('L', canvas.size, 0)
        ImageDraw.Draw(poly).polygon([(float(xs[v]), float(ys[v])) for v in hull.vertices], fill=255)
        panel = np.asarray(poly) > 127
        soft = ndimage.gaussian_filter(panel.astype(float), 1.0)
        edge = w * tone * ndimage.binary_dilation(panel, iterations=3)
        alpha = np.clip(np.maximum(edge, 0.5 * soft), 0, 1)
        kk = np.maximum(kk, np.maximum(soft, edge))
        lay = np.zeros_like(a)
        lay[..., 0] = lay[..., 1] = lay[..., 2] = np.clip(val / 0.85, 0, 1) * 255   # o brilho da arte (o núcleo aceso fica branco)
        lay[..., 3] = alpha * 255
        layers.append(Image.fromarray(lay.astype('uint8'), 'RGBA'))
    k = np.clip(kk, 0, 1)[..., None]
    base[..., :3] = a[..., :3] * (1 - k) + (lum * 0.5)[..., None] * k           # o vidro sem cor, mais escuro
    return Image.fromarray(base.clip(0, 255).astype('uint8'), 'RGBA'), layers[0], layers[1]


def top_bar(path, faction):
    """A folha do topo 1x1: a peça larga mais alta da folha (o Gemini às vezes junta outras peças embaixo), na mesma escala nos dois
    eixos (o anel continua redondo), com os anéis em x 83 e 1006 e na altura TOP_RING_Y. Escreve o centro e o raio de cada face pro HUD
    pôr o emblema no meio (`DrawTop1v1`)."""
    rgba = key_green(np.asarray(Image.open(path).convert('RGB')).astype(float))
    lab, _ = ndimage.label(rgba[..., 3] > 40)                                   # sem fechar: a peça vizinha às vezes encosta a 5 px
    wide = [s for s in ndimage.find_objects(lab) if (s[1].stop - s[1].start) > rgba.shape[1] * 0.6]
    s = min(wide, key=lambda s: s[0].start)
    a = rgba[s].astype(float)                                                    # uma peça encostada embaixo: corta na faixa verde/vazia
    sep = ((a[..., 3] > 40).mean(axis=1) < 0.3) | (((a[..., 1] > 180) & (a[..., 0] < 110) & (a[..., 2] < 110)).mean(axis=1) > 0.4)
    cut = next((y for y in range(int(a.shape[0] * 0.3), a.shape[0]) if sep[y]), a.shape[0])
    s = (slice(s[0].start, s[0].start + cut), s[1])
    lab2, _ = ndimage.label(rgba[s][..., 3] > 40)                               # e a largura volta a ser só a da barra
    sl = max(ndimage.find_objects(lab2), key=lambda q: (q[0].stop - q[0].start) * (q[1].stop - q[1].start))
    s = (slice(s[0].start + sl[0].start, s[0].start + sl[0].stop), slice(s[1].start + sl[1].start, s[1].start + sl[1].stop))
    piece = rgba[s].copy()
    g = piece.astype(float)                                                      # o verde preso entre as peças e entre as ripas
    piece[(g[..., 1] > 100) & (g[..., 1] > 1.5 * g[..., 0]) & (g[..., 1] > 1.5 * g[..., 2]), 3] = 0
    edge = ndimage.binary_dilation(piece[..., 3] < 40, iterations=3)            # o respingo verde na borda que encosta no vazio
    m = np.maximum(g[..., 0], g[..., 2])
    spill = edge & (g[..., 1] > 1.15 * m)
    piece[..., 1][spill] = m[spill].astype(np.uint8)
    (xl, yl, rl), (xr, yr, rr) = rings(piece)
    k = (1006 - 83) / (xr - xl)
    im = Image.fromarray(piece, 'RGBA')
    im = im.resize((round(im.width * k), round(im.height * k)), Image.LANCZOS)
    canvas = Image.new('RGBA', (TOP_W + 2 * TOP_MX, TOP_H + TOP_MB), (0, 0, 0, 0))
    dx, dy = round(TOP_MX + 83 - xl * k), round(TOP_RING_Y - (yl + yr) / 2 * k)
    big = Image.new('RGBA', (canvas.width + im.width * 2, canvas.height + im.height * 2), (0, 0, 0, 0))   # cola com folga e corta
    big.alpha_composite(im, (dx + im.width, dy + im.height))
    canvas = big.crop((im.width, im.height, im.width + canvas.width, im.height + canvas.height))
    out = os.path.join(KIT, 'skin', faction)                                    # sem calm_red: o único vermelho aqui é o do adversário
    os.makedirs(out, exist_ok=True)
    base, me, foe = player_layers(canvas)
    base.save(os.path.join(out, 'top_bar_1v1.png'))
    me.save(os.path.join(out, 'top_bar_1v1_me.png'))
    foe.save(os.path.join(out, 'top_bar_1v1_foe.png'))
    geo = {'mx': TOP_MX, 'mb': TOP_MB, 'ring_y': TOP_RING_Y, 'r_left': round(rl * k, 1), 'r_right': round(rr * k, 1),
           'y_left': round(TOP_RING_Y + (yl - (yl + yr) / 2) * k, 1), 'y_right': round(TOP_RING_Y + (yr - (yl + yr) / 2) * k, 1)}
    body = np.where((np.asarray(canvas)[..., 3][:, TOP_MX:TOP_MX + TOP_W] > 40).mean(axis=1) > 0.6)[0]
    geo['bottom'] = int(body.max())                                              # o fim do corpo: vai na tabela Bottom do DrawTop1v1
    print('top_bar_1v1', canvas.size, 'escala', round(k, 3), geo)
    return geo

if __name__ == '__main__':
    if sys.argv[1] == '--top':
        top_bar(sys.argv[2], sys.argv[3])
        sys.exit()
    if sys.argv[1] == '--tube':
        tube_conduit(sys.argv[2], sys.argv[3])
        sys.exit()
    frames(sys.argv[1], sys.argv[3])
    icons(sys.argv[2], sys.argv[3])
