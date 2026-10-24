"""O medalhão e a luz azul do relógio da Supply Drop Zone (o painel do meio com a zona selecionada).

O alvo é a imagem 4 do GPT que o Felipe escolheu (docs/aprovados/jogo/2026-10-23_timer_drop_zone_gpt_b_ESQUERDA_APROVADA.jpg):
um medalhão redondo de aço escuro com o emblema do paraquedas com a caixa e uma luz azul suave atrás. As medidas saíram do
próprio alvo (o perfil pelo raio, a mediana de 4 setores sem o emblema; raio de fora R = 85,5 px no alvo):

  f = r / R   0,80..0,845  a sombra do aro no disco (quase preto)
              0,85..0,915  o cordão polido de dentro (prata, 110..185)
              0,92..0,965  o sulco escuro do aro (10..45)
              0,97..0,987  o lábio de fora (prata, 70..130)
              0,99..1      a quina escura
  o disco: azul-marinho escuro, mais claro em cima à esquerda (33,46,57) que embaixo à direita (12,22,28)
  o emblema: de -0,725 R a +0,76 R na altura, ±0,59 R na largura (a caixa de +0,25 R pra baixo)
  a luz: nasce colada no aro (o pico em 1,07 R), cai até o fundo do painel em ~1,6 R do lado de fora e em ~1,25 R do lado do
  texto (o alvo é mais forte à esquerda); azul puro, sem vermelho (4,74,136 no pico, sobre 11,21,30)

- O emblema é o do kit (docs/concept/hud/kit/icons/paradrop.png, a folha de ícones do GPT): o mesmo desenho do alvo (o GPT
  partiu dele), com mais resolução (203 px de altura no ícone; 127 no alvo). Sai do fundo escuro do ícone por uma máscara do
  claro (fechada pra pegar os vincos escuros de dentro), sem redesenhar nada.
- O aro é metal de verdade, não vetor: o relevo pelo raio aceso por uma luz de cima à esquerda, o reflexo do ambiente nos
  cordões polidos, o escovado circular e riscos finos.
- A luz azul é uma textura à parte: o painel desenha ela atrás do medalhão (apaga sem energia, respira com o avião a caminho).

python art/tools/make_timer_medallion.py [pasta]   (escreve timer_medallion_airlift.png e timer_glow.png; padrão: kit/skin)

No jogo: FRTSHudPainter::DrawTimer (Game/Plugins/RTSHud), preenchido no RTSHUDNew.cpp (a zona pronta, só ela selecionada).
"""
import sys
import os
import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
KIT = os.environ.get('HUD_KIT', os.path.join(ROOT, 'docs', 'concept', 'hud', 'kit'))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(KIT, 'skin')
N = 512                      # a textura (o painel mostra ~190 px em 1080p; mipmaps no jogo)
C = (N - 1) / 2.0
R0 = 250.0                   # o raio de fora (o resto é a sombra de fora)

rng = np.random.default_rng(23)
yy, xx = np.mgrid[0:N, 0:N].astype(np.float64)
dx, dy = xx - C, yy - C
r = np.hypot(dx, dy)
th = np.arctan2(dy, dx)
f = r / R0


def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def dome(lo, hi, x):
    """Um cordão de seção redonda entre lo e hi: a altura (0..1) e a inclinação (-1..1, + = a face que olha pra fora)."""
    t = np.clip((x - lo) / (hi - lo), 0.0, 1.0) * 2 - 1     # -1..1 na seção
    inside = (x >= lo) & (x <= hi)
    hgt = np.where(inside, np.sqrt(np.clip(1 - t * t, 0, 1)), 0.0)
    return hgt, np.where(inside, t, 0.0), inside


# ---- o escovado, a sujeira e os riscos (o aço não é liso) ---------------------------------------------------------
ang_n, rad_n = 2048, 512
polar = rng.normal(0, 1, (rad_n, ang_n))
polar = ndimage.gaussian_filter(polar, (0.6, 22.0), mode='wrap')
polar /= polar.std() + 1e-6
ri = np.clip((np.clip(r, 0, R0 * 1.05) / (R0 * 1.05) * (rad_n - 1)).astype(int), 0, rad_n - 1)
ai = (((th + np.pi) / (2 * np.pi)) * ang_n).astype(int) % ang_n
brushed = polar[ri, ai]
grime = ndimage.gaussian_filter(rng.normal(0, 1, (N, N)), 10.0)
grime /= grime.std() + 1e-6
mottle = ndimage.gaussian_filter(rng.normal(0, 1, (N, N)), 3.0)
mottle /= mottle.std() + 1e-6
fine = ndimage.gaussian_filter(rng.normal(0, 1, (N, N)), 0.7)
fine /= fine.std() + 1e-6

scratch = np.zeros((N, N))
for _ in range(60):          # riscos em arco no aro
    rr = rng.uniform(0.86, 0.99) * R0
    a0 = rng.uniform(-np.pi, np.pi)
    ts = np.linspace(a0, a0 + rng.uniform(0.04, 0.25), 160)
    px = (C + rr * np.cos(ts)).astype(int)
    py = (C + rr * np.sin(ts)).astype(int)
    scratch[py.clip(0, N - 1), px.clip(0, N - 1)] += rng.uniform(0.3, 1.0)
for _ in range(55):          # riscos retos no disco (o alvo tem o disco riscado de leve)
    x0, y0 = rng.uniform(-0.75, 0.75, 2) * R0 + C
    ang = rng.uniform(0, np.pi)
    ts = np.linspace(0, rng.uniform(10, 55), 140)
    px = (x0 + ts * np.cos(ang)).astype(int)
    py = (y0 + ts * np.sin(ang)).astype(int)
    scratch[py.clip(0, N - 1), px.clip(0, N - 1)] += rng.uniform(0.15, 0.5)
scratch = ndimage.gaussian_filter(scratch, 0.6)

# a luz (de cima à esquerda) e o ambiente: o "céu" claro em cima, o chão escuro embaixo
L2 = np.array([-0.55, -0.83])                       # a direção da luz na tela (de onde ela vem)
L2 = L2 / np.linalg.norm(L2)
radial = np.stack([dx / np.maximum(r, 1e-6), dy / np.maximum(r, 1e-6)])   # pra fora, pelo ângulo
facing = radial[0] * L2[0] + radial[1] * L2[1]       # +1 = o lado do aro virado pra luz (cima à esquerda)
TINT = np.array([0.90, 0.95, 1.0])                   # o prata frio do alvo (um pouco azulado)


def metal(tone):
    """Cinza (0..255) → o aço azulado do alvo, com o escovado."""
    t = tone * (1 + 0.06 * brushed + 0.04 * grime)
    return t[..., None] * TINT[None, None, :]


# ---- o cordão polido de dentro (0,85..0,915): reflexo do ambiente pela inclinação da seção ---------------------------
bh, bs, bin_ = dome(0.848, 0.902, f)
# a face da seção que olha pra luz (bs·facing > 0) acende; o topo do cordão reflete o céu; a face de baixo, o chão
spec_b = np.exp(-((bs - 0.35 * facing) / 0.32) ** 2)          # o filete de reflexo anda com o lado da luz
tone_b = 70 + 55 * bh + 60 * spec_b + 18 * facing * bh
tone_b *= np.where(bin_, 1.0, 0.0)
# ---- o sulco escuro (0,92..0,965): um pouco bojudo, quase sem luz ----------------------------------------------------
gh, gs, gin = dome(0.902, 0.964, f)
tone_g = (22 + 26 * gh - 14 * gs * facing + 12 * facing + 8 * brushed) * np.where(gin, 1.0, 0.0)
# ---- o lábio de fora (0,97..0,99) --------------------------------------------------------------------------------
lh, ls_, lin = dome(0.962, 0.990, f)
tone_l = (62 + 58 * lh + 34 * np.exp(-((ls_ - 0.4 * facing) / 0.4) ** 2) + 18 * facing) * np.where(lin, 1.0, 0.0)

ring_tone = tone_b + tone_g + tone_l
# as quinas entre as peças (o escuro fino entre o cordão, o sulco e o lábio)
seam = np.exp(-((f - 0.903) / 0.0045) ** 2) * 0.55 + np.exp(-((f - 0.963) / 0.0040) ** 2) * 0.6
ring_tone = ring_tone * (1 - seam)
ring_col = metal(ring_tone) + scratch[..., None] * 30.0 * ((f > 0.905) & (f < 0.962))[..., None]
# o filete de brilho na crista do cordão e do lábio no lado da luz (o contorno claro do alvo)
crest = (np.exp(-((f - 0.872) / 0.006) ** 2) * 0.7 + np.exp(-((f - 0.976) / 0.004) ** 2) * 0.6) * np.clip(0.35 + 0.65 * facing, 0, 1)
ring_col += crest[..., None] * np.array([70, 74, 80])[None, None, :]

# ---- o disco (rebaixado) -------------------------------------------------------------------------------------------
lit = np.clip(0.5 + 0.5 * (-(dx * 0.55 + dy * 0.83) / (0.8 * R0)), 0, 1)     # 1 em cima à esquerda, 0 embaixo à direita
dark = np.array([11, 21, 27], dtype=np.float64)
light = np.array([34, 48, 60], dtype=np.float64)
disk = dark[None, None, :] * (1 - lit[..., None]) + light[None, None, :] * lit[..., None]
# o miolo um pouco mais claro (o alvo clareia pro centro) e manchado
disk *= (1 + 0.18 * np.exp(-(f / 0.55) ** 2))[..., None]
disk *= (1 + 0.10 * grime[..., None] + 0.06 * mottle[..., None] + 0.03 * fine[..., None])
disk += scratch[..., None] * 14.0 * (f < 0.80)[..., None]
# a sombra do aro no disco (o aro é alto, a luz vem de cima à esquerda: mais larga embaixo à direita)
f_sh = np.hypot(dx - 6.0, dy - 9.0) / R0
ao = smooth(0.72, 0.84, f_sh) * 0.8 + smooth(0.77, 0.845, f) * 0.6
disk *= (1 - np.clip(ao, 0, 0.92))[..., None]

col = np.where((f >= 0.846)[..., None], ring_col, disk)

# ---- o emblema do kit ----------------------------------------------------------------------------------------------
icon = np.asarray(Image.open(os.path.join(KIT, 'icons/paradrop.png')).convert('RGB')).astype(np.float64)
lum = icon.mean(axis=2)
crop = (40, 22, 212, 232)    # x0, y0, x1, y1: o emblema (43..208 × 25..228) com folga; a moldura do ícone fica fora
sub = icon[crop[1]:crop[3], crop[0]:crop[2]]
sl = lum[crop[1]:crop[3], crop[0]:crop[2]]
bright = sl > 88
sil = ndimage.binary_closing(bright, structure=np.ones((5, 5)), iterations=1)   # os vincos escuros de dentro, não os vãos
sil = ndimage.binary_fill_holes(sil) & ndimage.binary_dilation(bright, iterations=3)
sil = ndimage.binary_dilation(sil, iterations=1)    # o contorno escuro do desenho (1 px) entra junto
alpha_e = ndimage.gaussian_filter(sil.astype(np.float64), 0.7)

# a escala e o lugar do alvo: 1,485 R de altura, de -0,725 R a +0,76 R
EH = 1.485 * R0
scale = EH / (228 - 25)
ew = int(round(sub.shape[1] * scale))
ehh = int(round(sub.shape[0] * scale))
emb = np.asarray(Image.fromarray(np.clip(sub, 0, 255).astype(np.uint8)).resize((ew, ehh), Image.LANCZOS)).astype(np.float64)
ea = np.asarray(Image.fromarray((np.clip(alpha_e, 0, 1) * 255).astype(np.uint8)).resize((ew, ehh), Image.LANCZOS)).astype(np.float64) / 255.0
top_icon = 25 - crop[1]
ox = int(round(C - (125.5 - crop[0]) * scale))
oy = int(round(C - 0.725 * R0 - top_icon * scale))
# o prata frio e um pouco mais de contraste (o alvo tem o emblema mais claro que o ícone)
emb = emb * np.array([0.96, 0.99, 1.03])[None, None, :]
emb = np.clip((emb - 120) * 1.12 + 132, 0, 255)

# a sombra do emblema no disco (relevo): embaixo à direita, borrada
sh = np.zeros((N, N))
sh[oy:oy + ehh, ox:ox + ew] = ea
sh = ndimage.shift(sh, (8, 5), order=1)
sh = ndimage.gaussian_filter(sh, 5.0)
col *= (1 - 0.6 * sh * (f < 0.846))[..., None]
layer = np.zeros((N, N, 3))
la = np.zeros((N, N))
layer[oy:oy + ehh, ox:ox + ew] = emb
la[oy:oy + ehh, ox:ox + ew] = ea
la *= (f < 0.80)
col = col * (1 - la[..., None]) + layer * la[..., None]

# ---- o recorte redondo (antisserrilhado), a quina escura e a sombra de fora ------------------------------------------
cut = np.clip((R0 - r) + 0.5, 0, 1)
rim_dark = np.exp(-((f - 0.997) / 0.006) ** 2)
col *= (1 - 0.75 * rim_dark)[..., None]
outer_shadow = np.clip(1 - (r - R0) / 4.0, 0, 1) * (r >= R0) * 0.8
alpha = np.maximum(cut, outer_shadow)
rgb = np.where((r < R0)[..., None], col, 0.0)
out = np.dstack([np.clip(rgb, 0, 255), np.clip(alpha, 0, 1) * 255]).astype(np.uint8)
Image.fromarray(out, 'RGBA').save(os.path.join(OUT, 'timer_medallion_airlift.png'))

# ---- a luz azul atrás -----------------------------------------------------------------------------------------------
# A textura cobre ±1,7 R na largura e ±1,05 R na altura (o painel desenha 3,4 R × 2,1 R, centrada no medalhão). O pico
# fica colado no aro (1,07 R) e cai como no alvo: até ~1,6 R do lado de fora (esquerda), ~1,25 R do lado do texto.
GW, GH = 680, 420
gy2, gx2 = np.mgrid[0:GH, 0:GW].astype(np.float64)
u = (gx2 - (GW - 1) / 2) / (GW / 2) * 1.7          # em R
v = (gy2 - (GH - 1) / 2) / (GH / 2) * 1.05
d = np.hypot(u, v)
side = np.clip(0.5 - 0.5 * u / np.maximum(d, 1e-6), 0, 1)     # 1 do lado de fora (esquerda), 0 do lado do texto
reach = 0.20 + 0.20 * side                                   # quanto a luz anda além do aro (em R)
core = np.exp(-np.maximum(d - 1.07, 0.0) / (reach * 0.55)) * smooth(0.97, 1.07, d)
halo = np.exp(-np.maximum(d - 1.0, 0.0) / (reach * 1.6)) * 0.30 * (d > 0.97)
ang = np.arctan2(v, u)
rays_n = 900
rays = ndimage.gaussian_filter1d(rng.normal(0, 1, rays_n), 1.2, mode='wrap')
rays = (rays - rays.min()) / (rays.max() - rays.min())
rr = rays[(((ang + np.pi) / (2 * np.pi)) * rays_n).astype(int) % rays_n]
streak = np.exp(-np.maximum(d - 1.0, 0.0) / (reach * 1.3)) * rr ** 2.5 * 0.45 * (d > 0.97)
glow = core * (0.80 + 0.30 * rr) + halo + streak
edge_fade = (1 - smooth(0.80, 1.0, np.abs(v) / 1.05)) * (1 - smooth(0.85, 1.0, np.abs(u) / 1.7))
glow = np.clip(glow * edge_fade, 0, None)
glow = np.clip(glow / glow.max(), 0, 1)
gcol = np.array([8, 124, 255], dtype=np.float64)    # azul puro (o alvo não tem vermelho na luz: 2..10 no R)
gout = np.dstack([np.broadcast_to(gcol, (GH, GW, 3)), glow * 255]).astype(np.uint8)
Image.fromarray(gout, 'RGBA').save(os.path.join(OUT, 'timer_glow.png'))
print('ok', ew, ehh, ox, oy)
