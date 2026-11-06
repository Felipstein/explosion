# (03/11) O minimapa refeito, prancha v1: as três prévias do mapa (a foto tratada, a foto com o relevo, a carta tática) e os
# indicadores (hoje × o novo), tudo no losango de verdade do jogo, a 2×. A carta vem da grade da simulação (grade_do_mapa.png: o
# chão, o penhasco, a água) e dos motivos (grade_motivos.png: as estradas e os prédios), girada como o minimapa (o canto de cima à
# esquerda da grade vai pro N).
import math
from PIL import Image, ImageDraw, ImageFilter, ImageEnhance, ImageChops, ImageFont

G = "../../game/rally_ponto_v4/"
K = 2   # a escala da prancha
raw = Image.open(G + "lab_1_encontro.png").convert("RGB").crop((0, 610, 330, 900))
# os indicadores de hoje saem da foto (as docas, as unidades, a câmera): a máscara das cores deles, crescida, e a difusão do em volta
def clean(img):
    # só onde o jogo desenhou os indicadores (as docas, as unidades, a câmera, o quadradinho inimigo): o resto da foto fica intacto
    spots = [(140, 68), (121, 132), (87, 141), (208, 128), (169, 152), (151, 214), (141, 211), (133, 211), (125, 220)]
    m = Image.new("L", img.size, 0)
    d = ImageDraw.Draw(m)
    for x, y in spots:
        d.rectangle([x - 8, y - 8, x + 8, y + 8], fill=255)
    d.rectangle([124, 194, 156, 218], fill=255)
    px, mp = img.load(), m.load()
    for y in range(img.height):
        for x in range(img.width):
            if mp[x, y]:
                r, g, b = px[x, y]
                sand = r > g > b and r - b > 50 and r < 235 and not (g > 185 and b < 150 and r > 200)
                mp[x, y] = 0 if sand and not (b > 170) else 255
    m = m.filter(ImageFilter.MaxFilter(3))
    # o buraco se enche com a textura do lado (12 px pra esquerda ou pra cima, onde não tem marca): não borra
    out = img.copy(); po, pm = out.load(), m.load()
    for y in range(img.height):
        for x in range(img.width):
            if pm[x, y]:
                for dx, dy in ((0, 12), (0, -12), (-12, 0), (12, 0), (-18, -6), (18, 6)):
                    sx, sy = x + dx, y + dy
                    if 0 <= sx < img.width and 0 <= sy < img.height and not pm[sx, sy]:
                        po[x, y] = px[sx, sy]
                        break
    return out
raw = clean(raw)
frame = raw.resize((330 * K, 290 * K), Image.LANCZOS)
CX, CY, HX, HY = 145 * K, 142 * K, 113 * K, 109 * K   # o losango de dentro (o mapa), no recorte

def diamond_mask(shrink=0):
    m = Image.new("L", frame.size, 0)
    ImageDraw.Draw(m).polygon([(CX, CY - HY + shrink), (CX + HX - shrink, CY), (CX, CY + HY - shrink), (CX - HX + shrink, CY)], fill=255)
    return m

MASK = diamond_mask(1)

def place(img_square):
    # o quadrado da grade girado e espremido no losango
    side = img_square.size[0]
    r = img_square.rotate(-45, resample=Image.BICUBIC, expand=True)
    r = r.resize((2 * HX, 2 * HY), Image.LANCZOS)
    out = Image.new(img_square.mode, frame.size, 0 if img_square.mode == "L" else (0, 0, 0, 0))
    out.paste(r, (CX - HX, CY - HY))
    return out

# as classes da grade (256 × 256)
grid = Image.open(G + "grade_do_mapa.png").convert("RGB")
mot = Image.open(G + "grade_motivos.png").convert("RGB").resize((256, 256), Image.NEAREST)
def cls_mask(img, colors, tol=12):
    m = Image.new("L", img.size, 0)
    px, mp = img.load(), m.load()
    for y in range(img.size[1]):
        for x in range(img.size[0]):
            p = px[x, y]
            if any(all(abs(a - b) <= tol for a, b in zip(p, c)) for c in colors):
                mp[x, y] = 255
    return m
cliff = cls_mask(grid, [(70, 55, 45), (52, 41, 33)])
water = cls_mask(grid, [(40, 110, 200), (30, 82, 150)])
deep = cls_mask(grid, [(30, 82, 150)])
rock = cls_mask(mot, [(120, 120, 120), (90, 90, 90)])
build = cls_mask(mot, [(150, 70, 170)])
road = cls_mask(mot, [(230, 20, 20)], tol=30)

def smooth(m, up=8, blur=1.6):
    # a borda lisa: a grade em escada vira curva
    big = m.resize((256 * up, 256 * up), Image.BICUBIC).filter(ImageFilter.GaussianBlur(blur * up / 2))
    return big.point(lambda v: 255 if v > 127 else 0).resize((1024, 1024), Image.LANCZOS)

S_cliff, S_water, S_deep, S_rock = smooth(cliff), smooth(water), smooth(deep), smooth(rock, blur=1.2)
S_build = build.resize((1024, 1024), Image.NEAREST)
S_road = road.resize((1024, 1024), Image.NEAREST).filter(ImageFilter.MaxFilter(3))

def edge(m, w=3):
    return ImageChops.subtract(m.filter(ImageFilter.MaxFilter(w)), m.filter(ImageFilter.MinFilter(w)))

def layer(base, mask, color, alpha=1.0):
    col = Image.new("RGB", base.size, color)
    return Image.composite(col, base, mask.point(lambda v: int(v * alpha)))

# ——— A: a foto tratada (o contraste local, a nitidez, um pouco mais de cor): o que o jogo já tem, mais legível
photo = frame.copy()
a = ImageEnhance.Contrast(photo.filter(ImageFilter.UnsharpMask(radius=3, percent=90, threshold=2))).enhance(1.12)
a = ImageEnhance.Color(a).enhance(1.12)
A = Image.composite(a, frame, MASK)

# ——— B: a foto com o relevo: a borda do penhasco em traço escuro com a sombra pro sudeste, a margem da água clara, as estradas
b = a.copy()
cl, wa = place(S_cliff), place(S_water)
shadow = cl.filter(ImageFilter.GaussianBlur(3)).transform(cl.size, Image.AFFINE, (1, 0, -3, 0, 1, -3))
b = layer(b, ImageChops.multiply(shadow, MASK), (40, 28, 18), 0.35)
b = layer(b, ImageChops.multiply(place(edge(S_cliff, 7)), MASK), (38, 26, 18), 0.85)
b = layer(b, ImageChops.multiply(place(edge(S_water, 5)), MASK), (190, 235, 230), 0.7)
b = layer(b, ImageChops.multiply(place(S_road), MASK), (236, 222, 190), 0.55)
B = Image.composite(b, frame, MASK)

# ——— C: a carta tática: as cores chapadas da leitura (o chão, o penhasco, a rocha, a água, as estradas, os prédios), com o grão
c = Image.new("RGB", frame.size, (206, 176, 128))
grain = frame.convert("L").filter(ImageFilter.GaussianBlur(1.2))
c = Image.blend(c, Image.merge("RGB", [grain] * 3), 0.10)
c = layer(c, place(S_rock), (150, 126, 96))
c = layer(c, place(S_cliff), (92, 66, 46))
c = layer(c, place(edge(S_cliff, 5)), (52, 36, 26))
c = layer(c, place(S_water), (52, 128, 150))
c = layer(c, place(S_deep), (34, 98, 126))
c = layer(c, place(edge(S_water, 5)), (176, 226, 222))
c = layer(c, place(S_road), (240, 228, 200), 0.9)
c = layer(c, place(S_build), (120, 108, 92))
c = layer(c, place(edge(S_build, 3)), (60, 50, 40))
C = Image.composite(c, frame, MASK)

# ——— os indicadores (no recorte a 1×: as docas onde o jogo põe, a nossa base no sudeste, o inimigo no noroeste)
DOCKS = [(140, 68), (121, 132), (87, 141), (208, 128), (169, 152), (151, 214)]
OWN = (89, 178, 255); ENEMY = (240, 64, 52); INK = (10, 11, 12)
# a nossa base no sul (onde o jogo pôs a fábrica e os tanques), o inimigo no norte
STRUCT_OWN = [((141, 222), (9, 6)), ((131, 214), (6, 5)), ((152, 228), (5, 5))]
STRUCT_EN = [((150, 62), (9, 6)), ((161, 70), (6, 5))]
TANKS_OWN = [(126, 200), (131, 197), (136, 201), (122, 205), (140, 196)]
INF_OWN = [(150, 210), (152, 212), (148, 213)]
AIR_OWN = [((112, 180), -60)]
TANKS_EN = [(146, 86), (151, 89), (141, 91), (176, 112)]
CAM = [(118, 188), (146, 188), (141, 207), (123, 207)]

def rot_rect(cx, cy, hw, hh, ang=45):
    a = math.radians(ang)
    pts = [(-hw, -hh), (hw, -hh), (hw, hh), (-hw, hh)]
    return [(cx + (x * math.cos(a) - y * math.sin(a)), cy + (x * math.sin(a) + y * math.cos(a))) for x, y in pts]

def indicators_today(img):
    d = ImageDraw.Draw(img)
    for x, y in DOCKS:
        d.rectangle([x * K - 6, y * K - 6, x * K + 6, y * K + 6], fill=(236, 220, 130), outline=(40, 40, 30))
    for (x, y), _ in STRUCT_OWN:
        d.rectangle([x * K - 6, y * K - 6, x * K + 6, y * K + 6], fill=OWN)
    for x, y in TANKS_OWN + INF_OWN:
        d.rectangle([x * K - 4, y * K - 4, x * K + 4, y * K + 4], fill=OWN)
    for (x, y), _ in STRUCT_EN:
        d.rectangle([x * K - 6, y * K - 6, x * K + 6, y * K + 6], fill=ENEMY)
    for x, y in TANKS_EN:
        d.rectangle([x * K - 4, y * K - 4, x * K + 4, y * K + 4], fill=ENEMY)
    d.line([(p[0] * K, p[1] * K) for p in CAM + CAM[:1]], fill=(210, 220, 40), width=2)
    return img

def indicators_new(img):
    # a 4× pra borda lisa, depois reduz
    S = 4
    big = img.resize((img.width * S, img.height * S), Image.LANCZOS)
    d = ImageDraw.Draw(big)
    k = K * S
    # o que é do mapa: a doca, a caixa âmbar com a faixa (não parece unidade)
    for x, y in DOCKS:
        r = 7.0 * k / 2
        d.rounded_rectangle([x * k - r, y * k - r, x * k + r, y * k + r], radius=2 * S, fill=(255, 178, 31), outline=INK, width=2 * S)
        d.line([x * k - r + 2 * S, y * k, x * k + r - 2 * S, y * k], fill=(120, 74, 10), width=2 * S)
    # os prédios: a planta de verdade, girada com o mapa, na cor do dono com o contorno escuro
    for (x, y), (hw, hh) in STRUCT_OWN:
        d.polygon([(px * k, py * k) for px, py in rot_rect(x, y, hw * 0.5, hh * 0.5)], fill=OWN, outline=INK, width=2 * S)
    for (x, y), (hw, hh) in STRUCT_EN:
        d.polygon([(px * k, py * k) for px, py in rot_rect(x, y, hw * 0.5, hh * 0.5)], fill=ENEMY, outline=INK, width=2 * S)
    # os veículos: o losango (o tanque pesa mais); a infantaria: o ponto; o avião: a setinha no rumo
    for pts, col in ((TANKS_OWN, OWN), (TANKS_EN, ENEMY)):
        for x, y in pts:
            r = 3.6 * k / 2
            d.polygon([(x * k, y * k - r * 1.2), (x * k + r, y * k), (x * k, y * k + r * 1.2), (x * k - r, y * k)], fill=col, outline=INK, width=2 * S)
    for x, y in INF_OWN:
        r = 1.9 * k / 2
        d.ellipse([x * k - r, y * k - r, x * k + r, y * k + r], fill=OWN, outline=INK, width=S)
    for (x, y), a in AIR_OWN:
        aa = math.radians(a)
        u, sd = (math.cos(aa), math.sin(aa)), (-math.sin(aa), math.cos(aa))
        r = 4.6 * k / 2
        d.polygon([(x * k + u[0] * r * 1.3, y * k + u[1] * r * 1.3), (x * k - u[0] * r + sd[0] * r, y * k - u[1] * r + sd[1] * r), (x * k - u[0] * r * 0.4, y * k - u[1] * r * 0.4), (x * k - u[0] * r - sd[0] * r, y * k - u[1] * r - sd[1] * r)], fill=OWN, outline=INK, width=2 * S)
    # a seleção: o anel branco em volta dos selecionados
    for x, y in TANKS_OWN[:3]:
        r = 3.9 * k / 2 + 2 * S
        d.ellipse([x * k - r, y * k - r, x * k + r, y * k + r], outline=(255, 255, 255), width=S + 1)
    # a câmera: o trapézio de verdade em branco com o contorno escuro e o véu claro por dentro
    P = [(x * k, y * k) for x, y in CAM]
    veil = Image.new("RGBA", big.size, (0, 0, 0, 0))
    ImageDraw.Draw(veil).polygon(P, fill=(255, 255, 255, 34))
    big = Image.alpha_composite(big.convert("RGBA"), veil).convert("RGB")
    d = ImageDraw.Draw(big)
    d.line(P + P[:1], fill=INK, width=5 * S, joint="curve")
    d.line(P + P[:1], fill=(255, 255, 255), width=2 * S, joint="curve")
    # o ponto de encontro da fábrica selecionada e a fila do Alt, finos, como no chão
    d.line([(141 * k, 222 * k), (112 * k, 196 * k)], fill=INK, width=4 * S)
    d.line([(141 * k, 222 * k), (112 * k, 196 * k)], fill=(150, 205, 255), width=2 * S)
    d.ellipse([112 * k - 3 * S * 2, 196 * k - 3 * S * 2, 112 * k + 3 * S * 2, 196 * k + 3 * S * 2], fill=OWN, outline=INK, width=S)
    return big.resize(img.size, Image.LANCZOS)

# ——— a prancha
F = ImageFont.truetype("segoeuib.ttf", 18); f = ImageFont.truetype("segoeui.ttf", 14)
W1, H1 = frame.size
out = Image.new("RGB", (3 * W1 + 4 * 16, 2 * (H1 + 70) + 120), (21, 23, 26))
d = ImageDraw.Draw(out)
d.text((16, 10), "O minimapa · v1: a prévia do mapa (3 jeitos) e os indicadores (hoje × o novo), no losango do jogo a 2×", fill=(233, 230, 223), font=F)
cells = [(A, "A · a foto tratada (recomendada)", "a foto de cima que o jogo já tira, com contraste local e nitidez: o mapa de verdade, mais legível"),
         (B, "B · a foto com o relevo", "a borda do penhasco em traço com a sombra, a margem da água clara e as estradas: lê o terreno de relance"),
         (C, "C · a carta tática", "as cores chapadas da leitura (o chão, o penhasco, a água, as estradas, os prédios): limpa, mas sai do look do jogo")]
for i, (img, cap, note) in enumerate(cells):
    x = 16 + i * (W1 + 16)
    d.text((x, 44), cap, fill=(233, 230, 223), font=F)
    out.paste(img, (x, 70))
    d.text((x, 70 + H1 + 6), note, fill=(154, 150, 140), font=f)
y2 = 70 + H1 + 50
cells2 = [(indicators_today(frame.copy()), "Hoje", "tudo é quadrado: a doca, o tanque e o prédio iguais; a câmera numa linha amarela fina"),
          (indicators_new(A.copy()), "O novo · na foto (A)", "o prédio é a planta, o veículo o losango, a infantaria o ponto, o avião a seta; a doca é a caixa âmbar; a câmera em branco"),
          (indicators_new(B.copy()), "O novo · na foto com o relevo (B)", "os mesmos sinais: o branco da seleção, o ponto de encontro e a fila do Alt finos, como no chão")]
for i, (img, cap, note) in enumerate(cells2):
    x = 16 + i * (W1 + 16)
    d.text((x, y2), cap, fill=(233, 230, 223), font=F)
    out.paste(img, (x, y2 + 26))
    d.text((x, y2 + 26 + H1 + 6), note, fill=(154, 150, 140), font=f)
out.save("minimapa_v1.png")
print(out.size)

# ——— v2 (03/11, depois das referências aprovadas do HUD: docs/concept/hud/pieces/minimap_and_buttons.webp, "o conteúdo do
# minimapa" do clássico): a foto apagada e quase sem cor, a névoa nos três níveis (visto, explorado em cinza escuro, nunca visto
# preto), as bolinhas redondas e brilhantes na cor do jogador com o contorno, a câmera no trapézio branco
def fog_levels(img):
    vis = Image.new("L", img.size, 0); exp = Image.new("L", img.size, 0)
    dv, de = ImageDraw.Draw(vis), ImageDraw.Draw(exp)
    for (x, y), r in [((141, 210), 46), ((112, 185), 26), ((150, 70), 30)]:
        dv.ellipse([(x - r) * K, (y - r) * K, (x + r) * K, (y + r) * K], fill=255)
    for (x, y), r in [((141, 205), 78), ((150, 75), 50), ((205, 128), 34), ((90, 140), 34)]:
        de.ellipse([(x - r) * K, (y - r) * K, (x + r) * K, (y + r) * K], fill=255)
    vis = vis.filter(ImageFilter.GaussianBlur(6 * K)); exp = exp.filter(ImageFilter.GaussianBlur(7 * K))
    seen = ImageEnhance.Brightness(ImageEnhance.Color(img).enhance(0.25)).enhance(0.48)
    black = Image.new("RGB", img.size, (6, 7, 8))
    out = Image.composite(seen, black, exp)
    return Image.composite(img, out, vis)

def ref_tone(img):
    t = ImageEnhance.Color(img).enhance(0.62)
    t = ImageEnhance.Contrast(t.filter(ImageFilter.UnsharpMask(radius=3, percent=110, threshold=2))).enhance(1.10)
    return ImageEnhance.Brightness(t).enhance(0.86)

def dots_v2(img):
    S = 4; k = K * S
    big = img.resize((img.width * S, img.height * S), Image.LANCZOS).convert("RGBA")
    d = ImageDraw.Draw(big)
    def dot(x, y, r, col):
        r *= k
        d.ellipse([x * k - r - 1.2 * S, y * k - r - 1.2 * S, x * k + r + 1.2 * S, y * k + r + 1.2 * S], fill=INK)
        d.ellipse([x * k - r, y * k - r, x * k + r, y * k + r], fill=col)
        hl = tuple(min(255, int(c + (255 - c) * 0.55)) for c in col)
        d.ellipse([x * k - r * 0.55, y * k - r * 0.75, x * k + r * 0.25, y * k - r * 0.05], fill=hl)
    for x, y in DOCKS:
        r = 2.6 * k
        d.rounded_rectangle([x * k - r, y * k - r, x * k + r, y * k + r], radius=S * 2, fill=(255, 196, 64), outline=INK, width=int(1.2 * S * 2))
    for (x, y), _ in STRUCT_OWN:
        dot(x, y, 4.2, OWN)
    for (x, y), _ in STRUCT_EN:
        dot(x, y, 4.2, ENEMY)
    for x, y in TANKS_OWN + INF_OWN:
        dot(x, y, 3.0, OWN)
    for x, y in TANKS_EN:
        dot(x, y, 3.0, ENEMY)
    dot(AIR_OWN[0][0][0], AIR_OWN[0][0][1], 3.0, OWN)
    for x, y in TANKS_OWN[:3]:
        r = 3.0 * k + 2.4 * S
        d.ellipse([x * k - r, y * k - r, x * k + r, y * k + r], outline=(255, 255, 255), width=int(1.2 * S * 2))
    P = [(x * k, y * k) for x, y in CAM]
    d.line(P + P[:1], fill=(0, 0, 0, 160), width=int(3.4 * S * 2), joint="curve")
    d.line(P + P[:1], fill=(255, 255, 255), width=int(1.6 * S * 2), joint="curve")
    return big.convert("RGB").resize(img.size, Image.LANCZOS)

ref = Image.open("../../../docs/concept/hud/pieces/minimap_and_buttons.webp").convert("RGB")
ref = ref.resize((int(ref.width * frame.height / ref.height), frame.height), Image.LANCZOS)
v2 = Image.composite(fog_levels(ref_tone(frame)), frame, MASK)
v2 = dots_v2(v2)
v2_open = dots_v2(Image.composite(ref_tone(frame), frame, MASK))
today = Image.composite(frame, frame, MASK)
today = indicators_today(today)
o = Image.new("RGB", (ref.width + 3 * frame.width + 5 * 16, frame.height + 110), (21, 23, 26)); d = ImageDraw.Draw(o)
d.text((16, 10), "O minimapa · v2: alinhado à referência aprovada do HUD (o conteúdo do clássico)", fill=(233, 230, 223), font=F)
items = [(ref, "a referência aprovada", "a foto apagada, a névoa em 3 níveis, as bolinhas, o trapézio branco"),
         (today, "hoje", "a foto viva demais, tudo quadrado, a câmera amarela fina"),
         (v2_open, "v2 · sem a névoa", "a foto no tom da referência; as bolinhas na cor do jogador; a doca é a caixa"),
         (v2, "v2 · com a névoa", "o visto na cor; o explorado cinza escuro; o nunca visto preto")]
x = 16
for img, cap, note in items:
    d.text((x, 44), cap, fill=(233, 230, 223), font=F)
    o.paste(img, (x, 70))
    d.text((x, 70 + frame.height + 6), note, fill=(154, 150, 140), font=f)
    x += img.width + 16
o.save("minimapa_v2.png")
print("v2", o.size)

# ——— v3 (03/11, ele: "as bolinhas estão mal feitas, parece jogo de criança, jogo antigo"): sinais chapados e nítidos, sem brilho,
# o contorno escuro de 1 px, a 4× e reduzidos com a caixa (a borda fica firme, como a UI de hoje)
def crisp(img, draw_fn, S=4):
    big = img.resize((img.width * S, img.height * S), Image.NEAREST).convert("RGBA")
    layer = Image.new("RGBA", big.size, (0, 0, 0, 0))
    draw_fn(ImageDraw.Draw(layer), K * S, S)
    big = Image.alpha_composite(big, layer)
    return big.convert("RGB").resize(img.size, Image.BOX)

def foot(d, k, S, x, y, hw, hh, col, ang=45):
    # o prédio: a planta girada com o mapa, no tom fundo da cor (a unidade é a cor viva), com o fio claro por dentro
    deep = tuple(int(c * 0.62) for c in col)
    pts = [(px * k, py * k) for px, py in rot_rect(x, y, hw, hh, ang)]
    d.polygon(pts, fill=deep, outline=INK, width=S * 2)
    inner = [(px * k, py * k) for px, py in rot_rect(x, y, hw * 0.55, hh * 0.45, ang)]
    d.polygon(inner, outline=col, width=S * 2)

def cam(d, k, S):
    P = [(x * k, y * k) for x, y in CAM]
    d.line(P + P[:1], fill=(0, 0, 0, 150), width=S * 6, joint="curve")
    d.line(P + P[:1], fill=(255, 255, 255, 255), width=S * 3, joint="curve")

def docks(d, k, S):
    for x, y in DOCKS:
        r = 2.4 * k
        d.rectangle([x * k - r, y * k - r, x * k + r, y * k + r], fill=(255, 190, 60), outline=INK, width=S * 2)
        d.line([x * k - r + S * 2, y * k, x * k + r - S * 2, y * k], fill=(110, 70, 10), width=S * 2)

def sel(d, k, S, pts, r):
    for x, y in pts:
        d.rectangle([x * k - r * k, y * k - r * k, x * k + r * k, y * k + r * k], outline=(255, 255, 255), width=S * 3)

def opt_A(d, k, S):
    # A: chapado, como o StarCraft II: o quadrado pelo tamanho (o tanque 3,4 px, a infantaria 2), o prédio pela planta
    docks(d, k, S)
    for (x, y), (hw, hh) in STRUCT_OWN: foot(d, k, S, x, y, hw * 0.55, hh * 0.55, OWN)
    for (x, y), (hw, hh) in STRUCT_EN: foot(d, k, S, x, y, hw * 0.55, hh * 0.55, ENEMY)
    for pts, col, r in ((TANKS_OWN, OWN, 2.2), (TANKS_EN, ENEMY, 2.2), (INF_OWN, OWN, 1.3), ([AIR_OWN[0][0]], OWN, 1.8)):
        for x, y in pts:
            d.rectangle([x * k - r * k, y * k - r * k, x * k + r * k, y * k + r * k], fill=col, outline=INK, width=S * 2)
    sel(d, k, S, TANKS_OWN[:3], 2.2)
    cam(d, k, S)

def opt_B(d, k, S):
    # B: a simbologia militar (OTAN, APP-6): o amigo é o retângulo, o inimigo o losango, o neutro o quadrado; o avião a seta;
    # o prédio pela planta. Lê de quem é pela forma também (o daltônico lê)
    docks(d, k, S)
    for (x, y), (hw, hh) in STRUCT_OWN: foot(d, k, S, x, y, hw * 0.55, hh * 0.55, OWN)
    for (x, y), (hw, hh) in STRUCT_EN: foot(d, k, S, x, y, hw * 0.55, hh * 0.55, ENEMY)
    for x, y in TANKS_OWN:
        d.rectangle([x * k - 2.8 * k, y * k - 1.8 * k, x * k + 2.8 * k, y * k + 1.8 * k], fill=OWN, outline=INK, width=S * 2)
    for x, y in INF_OWN:
        d.rectangle([x * k - 1.3 * k, y * k - 0.9 * k, x * k + 1.3 * k, y * k + 0.9 * k], fill=OWN, outline=INK, width=S * 2)
    for x, y in TANKS_EN:
        r = 3.0 * k
        d.polygon([(x * k, y * k - r), (x * k + r, y * k), (x * k, y * k + r), (x * k - r, y * k)], fill=ENEMY, outline=INK, width=S * 2)
    (x, y), a = AIR_OWN[0]
    aa = math.radians(a); u = (math.cos(aa), math.sin(aa)); sd = (-u[1], u[0]); r = 2.6 * k
    d.polygon([(x * k + u[0] * r * 1.2, y * k + u[1] * r * 1.2), (x * k - u[0] * r + sd[0] * r, y * k - u[1] * r + sd[1] * r), (x * k - u[0] * r * 0.3, y * k - u[1] * r * 0.3), (x * k - u[0] * r - sd[0] * r, y * k - u[1] * r - sd[1] * r)], fill=OWN, outline=INK, width=S * 2)
    for x, y in TANKS_OWN[:3]:
        d.rectangle([x * k - 2.8 * k, y * k - 1.8 * k, x * k + 2.8 * k, y * k + 1.8 * k], outline=(255, 255, 255), width=S * 3)
    cam(d, k, S)

def opt_C(d, k, S):
    # C: o rumo (o sinal aponta pra onde vai: a coluna lê o movimento de relance), o prédio pela planta
    docks(d, k, S)
    for (x, y), (hw, hh) in STRUCT_OWN: foot(d, k, S, x, y, hw * 0.55, hh * 0.55, OWN)
    for (x, y), (hw, hh) in STRUCT_EN: foot(d, k, S, x, y, hw * 0.55, hh * 0.55, ENEMY)
    def arrow(x, y, a, col, r):
        aa = math.radians(a); u = (math.cos(aa), math.sin(aa)); sd = (-u[1], u[0])
        d.polygon([(x * k + u[0] * r * 1.3, y * k + u[1] * r * 1.3), (x * k - u[0] * r + sd[0] * r * 0.9, y * k - u[1] * r + sd[1] * r * 0.9), (x * k - u[0] * r - sd[0] * r * 0.9, y * k - u[1] * r - sd[1] * r * 0.9)], fill=col, outline=INK, width=S * 2)
    for x, y in TANKS_OWN: arrow(x, y, -120, OWN, 2.9 * k)
    for x, y in INF_OWN: arrow(x, y, -100, OWN, 1.4 * k)
    for x, y in TANKS_EN: arrow(x, y, 70, ENEMY, 2.9 * k)
    arrow(AIR_OWN[0][0][0], AIR_OWN[0][0][1], AIR_OWN[0][1], OWN, 2.6 * k)
    sel(d, k, S, TANKS_OWN[:3], 2.0)
    cam(d, k, S)

base = Image.composite(ImageEnhance.Brightness(ImageEnhance.Color(ref_tone(frame)).enhance(0.7)).enhance(0.78), frame, MASK)
opts = [("A · chapado (o StarCraft II)", "o quadrado pelo tamanho, o prédio pela planta, sem brilho", opt_A),
        ("B · a simbologia militar (recomendada)", "amigo retângulo, inimigo losango (a OTAN): de quem é pela forma e pela cor", opt_B),
        ("C · o rumo", "o sinal aponta pra onde vai: a coluna se lê andando", opt_C)]
ZX, ZY, ZW, ZH = 86 * K, 150 * K, 120 * K, 90 * K   # o recorte ampliado: a nossa base e a coluna
o = Image.new("RGB", (3 * frame.width + 4 * 16, frame.height + ZH * 2 + 140), (21, 23, 26)); d = ImageDraw.Draw(o)
d.text((16, 10), "O minimapa · v3: os sinais chapados e nítidos (sem a bolinha brilhante); em cima no tamanho do jogo a 2×, embaixo ampliado mais 2×", fill=(233, 230, 223), font=F)
for i, (cap, note, fn) in enumerate(opts):
    x = 16 + i * (frame.width + 16)
    img = crisp(base, fn)
    d.text((x, 44), cap, fill=(233, 230, 223), font=F)
    o.paste(img, (x, 70))
    d.text((x, 70 + frame.height + 6), note, fill=(154, 150, 140), font=f)
    zoom = img.crop((ZX, ZY, ZX + ZW, ZY + ZH)).resize((ZW * 2 + 140, ZH * 2), Image.NEAREST)
    o.paste(zoom.resize((frame.width, int(frame.width * ZH * 2 / (ZW * 2 + 140))), Image.LANCZOS) if False else zoom.crop((0, 0, min(frame.width, zoom.width), zoom.height)), (x, 70 + frame.height + 34))
o.save("minimapa_v3.png")
print("v3", o.size)

# ——— v4 (03/11, ele: "regrediu, o de hoje que tá no jogo tá melhor do que essa bosta"): o de hoje é o piso. A foto viva fica como
# está (o que ele gosta); só os três pontos fracos medidos mudam: o sinal ganha o contorno escuro (no mapa claro o quadrado azul
# claro some), a doca deixa de ser igual à unidade, a câmera fica branca e mais grossa (a amarela de 1 px some na areia)
def opt_v4(d, k, S):
    for x, y in DOCKS:
        r = 2.6 * k
        d.rectangle([x * k - r, y * k - r, x * k + r, y * k + r], fill=(255, 226, 120), outline=INK, width=S * 2)
        d.rectangle([x * k - r * 0.35, y * k - r * 0.35, x * k + r * 0.35, y * k + r * 0.35], fill=(150, 104, 30))
    for (x, y), _ in STRUCT_OWN:
        r = 3.4 * k; d.rectangle([x * k - r, y * k - r, x * k + r, y * k + r], fill=OWN, outline=INK, width=S * 2)
    for (x, y), _ in STRUCT_EN:
        r = 3.4 * k; d.rectangle([x * k - r, y * k - r, x * k + r, y * k + r], fill=ENEMY, outline=INK, width=S * 2)
    for pts, col in ((TANKS_OWN + INF_OWN + [AIR_OWN[0][0]], OWN), (TANKS_EN, ENEMY)):
        for x, y in pts:
            r = 2.2 * k; d.rectangle([x * k - r, y * k - r, x * k + r, y * k + r], fill=col, outline=INK, width=S * 2)
    for x, y in TANKS_OWN[:3]:
        r = 2.2 * k; d.rectangle([x * k - r, y * k - r, x * k + r, y * k + r], outline=(255, 255, 255), width=S * 2)
    P = [(x * k, y * k) for x, y in CAM]
    d.line(P + P[:1], fill=(0, 0, 0, 150), width=S * 6, joint="curve")
    d.line(P + P[:1], fill=(255, 255, 255, 255), width=S * 3, joint="curve")

photo_today = Image.composite(frame, frame, MASK)
t4 = indicators_today(photo_today.copy())
n4 = crisp(photo_today, opt_v4)
o = Image.new("RGB", (2 * frame.width + 3 * 16, frame.height + ZH * 2 + 120), (21, 23, 26)); d = ImageDraw.Draw(o)
d.text((16, 10), "O minimapa · v4: o de hoje é o piso (a foto viva igual); só o contorno escuro nos sinais, a doca diferente e a câmera branca", fill=(233, 230, 223), font=F)
for i, (img, cap) in enumerate(((t4, "hoje"), (n4, "v4 · o mínimo que muda"))):
    x = 16 + i * (frame.width + 16)
    d.text((x, 44), cap, fill=(233, 230, 223), font=F)
    o.paste(img, (x, 70))
    z = img.crop((ZX, ZY, ZX + ZW, ZY + ZH)).resize((ZW * 2 + 140, ZH * 2), Image.NEAREST)
    o.paste(z.crop((0, 0, min(frame.width, z.width), z.height)), (x, 70 + frame.height + 16))
o.save("minimapa_v4.png")
print("v4", o.size)
