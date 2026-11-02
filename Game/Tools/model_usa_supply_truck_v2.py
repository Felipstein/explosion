"""O caminhão de suprimento da USA (usa_supply_truck), VERSÃO B no detalhe AAA (01/11/2026): a versão A
(Game/Tools/model_usa_supply_truck.py, aprovada em 14/10: "agora sim") fica como está; esta é a que ele compara com ela.

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_usa_supply_truck_v2.py C:/Users/luisf/explosion/captures/game/models/usa_supply_truck_v2

O pedido (31/10 e 01/11): "isso não é um jogo de lego [...] DETALHES MINUCIOSOS [...] CAMINHÕES [...] nada de quinas quadradas"; e
sobre os da USA: "muito mais parecidos [com os] detalhes da ficha". Não há ficha de imagem do caminhão: o alvo é o desenho aprovado
(o A: o 6×6 tático de cabine avançada blindada, a grade de canos na frente, a caçamba curta com as 4 caixas em 2×2, o guindaste
amarelo dobrado por cima da carga, a lona azul enrolada na cabeceira, a faixa azul segmentada nas saias e o painel azul das portas),
a ficha de texto (docs/elenco/usa/usa_supply_truck.md §6) e a família de caminhões da USA no detalhe B (o Tomahawk B: o pneu de
verdade, a grade de canos de tubo dobrado, os para-lamas em arco, os retrovisores nos braços, a escada da porta, o para-choque com a
faixa de aviso). A USA é o exército limpo (o dono, 13/10): nada de desgaste pintado; a sujeira vem do material do jogo.

Escala: o A era desenhado e multiplicado por 0,72 (o tanque domina a cena). O B é desenhado JÁ NA ESCALA FINAL (os números do A × 0,72),
pra que as peças miúdas (porca, alça, dobradiça) tenham o tamanho certo de 2 px.

Números MANTIDOS (Units.json usa_supply_truck): cargo.slots_m [(−1,764; ±0,49; 0,936), (−0,612; ±0,49; 0,936)] (o assoalho da caçamba
em 0,936); wheels.tyre_w_m 0,274 (o pneu de 0,274 m de largura, raio 0,36, eixos x 1,62 / −0,936 / −1,872, y ±0,90: o jogo lê das
malhas wheel_*); a caixa da carga com a origem no centro do pé, 1,08 × 0,936 × 0,734 m (a do A).

Peças (origem no chão, no centro do chassi, frente em +X; os NOMES do A):
  SM_USA_SupplyTruck_Hull       tudo que não gira;
  SM_USA_SupplyTruck_Wheels     6 malhas wheel_001..006 (a ordem do A: eixo da frente pra trás, esquerda e direita);
  SM_USA_SupplyCrate_Small      a caixa da carga (cargo.mesh), refeita no detalhe: o estrado de metal, as tábuas, as cantoneiras, a
                                faixa amarela na tampa, as 2 cintas com a fivela, as alças de corda.
Slots (os do A): Paint, Team, Dark, Metal, Rubber, Lens, Light, Yellow; a caixa: Crate, Trim, Paint (os do A) + Yellow (a faixa da
tampa; o Yellow já existe na tabela do import_provisional.py).
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, export_group  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
from model_china_vehicle_kit import bounds, frame_box, solid, tris  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_supply_truck_v2')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_USA_SupplyTruck'
CRATE_NAME = 'SM_USA_SupplyCrate_Small'

COLORS.update({'Paint': (196, 198, 192), 'Team': (86, 108, 150), 'Yellow': (214, 164, 32), 'Dark': (40, 40, 38),
               'Metal': (95, 95, 92), 'Rubber': (32, 32, 31), 'Lens': (92, 128, 168), 'Light': (255, 230, 170),
               'Crate': (96, 98, 62), 'Trim': (70, 70, 68)})

# ---------------------------------------------------------------------------------- medidas (escala final, m: as do A × 0,72)
FRONT, REAR = 2.304, -2.448          # o para-choque da frente e o de trás (+ a grade de canos: 2,46)
WHEEL_R, WHEEL_W, WHEEL_Y = 0.36, 0.274, 0.90
AXLES_X = [1.62, -0.936, -1.872]     # 6×6: o eixo da frente e o par de trás
FRAME_Z = (0.518, 0.72)              # as longarinas
BED_Z = 0.936                        # o assoalho da caçamba (= z das vagas)
BED_X = (-2.376, 0.288)              # a caçamba, da tampa de trás à cabeceira
BED_HW = 1.022                       # a meia-largura de fora da caçamba
SIDE_H = 0.396                       # a altura das laterais baixas
HEAD_H = 0.684                       # a cabeceira
CAB_X = (0.576, 2.196)               # a cabine avançada (o pé de trás e o pé da frente)
CAB_Y = 0.878
CAB_Z0, CAB_Z = 0.756, 1.728
# o perfil da cabine (x, z): o nariz baixo que avança, a face dos faróis, o para-brisa inclinado (o do A × 0,72)
CAB_PROF = [(0.576, CAB_Z0), (2.196, CAB_Z0), (2.282, 1.152), (2.182, 1.404), (1.98, CAB_Z), (0.576, CAB_Z)]
ENG_X = (0.324, 0.576)               # a caixa do motor atrás da cabine
ENG_TOP = 1.44
SLOTS = [(-1.764, -0.49, BED_Z), (-1.764, 0.49, BED_Z), (-0.612, -0.49, BED_Z), (-0.612, 0.49, BED_Z)]
CRATE = (1.08, 0.936, 0.734)
CRATE_BAKE_AT = (0.0, 9.0, 0.0)      # a caixa cozinha longe do caminhão (só o chão faz sombra nela) e volta pra origem na exportação
CRANE = (0.504, 0.518)               # a coluna do guindaste (no deck do motor, à direita)


# ---------------------------------------------------------------------------------- utilidades
def tube(p, m, a, b, r, seg=10):
    vp.tube(p, m, [a, b], r, seg=seg)


def cylinder(p, a, b, r_body, r_rod, body_m='Yellow', rod_m='Metal', frac=0.55, seg=16):
    """O cilindro hidráulico de a até b: o corpo torneado com as tampas e a haste cromada saindo, os olhais das pontas."""
    a, b = Vector(a), Vector(b)
    d = b - a
    L = d.length
    u = d.normalized()
    Lb = L * frac
    vp.lathe(p, body_m, a, u, [(0.0, 0.0), (r_body * 0.8, 0.0), (r_body, 0.02), (r_body, Lb - 0.04), (r_body * 1.15, Lb - 0.03),
                               (r_body * 1.15, Lb), (r_rod * 1.3, Lb + 0.008), (0.0, Lb + 0.008)], seg=seg)
    vp.lathe(p, rod_m, a, u, [(0.0, Lb - 0.02), (r_rod, Lb - 0.02), (r_rod, L - 0.03), (0.0, L - 0.03)], seg=max(10, seg - 4))
    for q in (a, b):
        _, e1, _ = vp.axis_frame(u)
        vp.lathe(p, body_m if q is a else rod_m, q, e1, [(0.0, -0.028), (r_body * 0.95, -0.028), (r_body * 0.95, 0.028), (0.0, 0.028)],
                 seg=12)


def boom(p, m, a, b, w, h, side=(0, 1, 0), r=0.05):
    """A viga de quina redonda de a até b no plano xz (a lança do guindaste), largura w (ao longo de y), altura h; as faces de lado em
    quads (vp.rbox_q)."""
    a, b = Vector(a), Vector(b)
    if b.x < a.x:
        a, b = b, a
    d = b - a
    pitch = math.degrees(math.atan2(d.z, d.x))
    vp.rbox_q(p, m, (a + b) / 2, (d.length, h, w), pitch=pitch, roll=90, r=r, k=4)


def louvers(p, c, n_face, along, L, H, n, slat='Paint#thin', tilt=35.0):
    """A veneziana numa face (c = o centro na face, n_face = a normal): o fundo escuro, a moldura e as aletas inclinadas."""
    eu, ev, en = vp.face_frame(n_face, along)
    C = Vector(c)
    vp.prism(p, 'Dark#vp', vp.rounded_rect(L + 0.02, H + 0.02, 0.025, 2), C - en * 0.004, eu, ev, en, 0.01)
    fw = 0.035
    for sgn in (-1, 1):
        frame_box(p, 'Paint#plate', C + ev * (sgn * (H / 2 + fw / 2)) + en * 0.016, eu, ev, en, L + 2 * fw, fw, 0.032)
        frame_box(p, 'Paint#plate', C + eu * (sgn * (L / 2 + fw / 2)) + en * 0.016, eu, ev, en, fw, H, 0.032)
    step = H / n
    a = math.radians(tilt)
    e2 = ev * math.cos(a) + en * math.sin(a)
    e3 = en * math.cos(a) - ev * math.sin(a)
    for k in range(n):
        frame_box(p, slat, C + ev * (-H / 2 + step * (k + 0.5)) + en * 0.016, eu, e2, e3, L, step * 0.8, 0.01)


def strip_xz(p, m, path, yc, w, t, bevel_key=True):
    """A chapa dobrada (o para-lama): a seção retangular (largura w ao longo de y, espessura t) varrida pela polilinha do plano xz."""
    pts = [Vector(q) for q in path]
    n = len(pts)
    rings = []
    for i in range(n):
        d = pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]
        tx, tz = d.x, d.z
        L = math.hypot(tx, tz) or 1.0
        nx, nz = -tz / L, tx / L                      # a normal no plano xz (à esquerda da marcha)
        o = Vector((nx * t / 2, 0.0, nz * t / 2))
        P = pts[i]
        rings.append([(P.x + o.x, yc - w / 2, P.z + o.z), (P.x + o.x, yc + w / 2, P.z + o.z),
                      (P.x - o.x, yc + w / 2, P.z - o.z), (P.x - o.x, yc - w / 2, P.z - o.z)])
    p.loft(m, rings)


def plate_y(p, m, x0, x1, z0, z1, y, t, r=0.04, k=4):
    """A chapa em pé na face de lado (normal ±y): de x0 a x1, de z0 a z1, com a espessura t pra fora de y (t com sinal). As tampas em
    quads (vp.rbox_q): a grade enche a chapa de pontos e a máscara não risca."""
    vp.rbox_q(p, m, ((x0 + x1) / 2, y + t / 2, (z0 + z1) / 2), (x1 - x0, z1 - z0, abs(t)), roll=90, r=r, k=k)


def plate_x(p, m, y0, y1, z0, z1, x, t, r=0.04, k=4):
    """A chapa em pé na face da frente/de trás (normal ±x): de y0 a y1, de z0 a z1, a espessura t a partir de x (t com sinal)."""
    vp.rbox_q(p, m, (x + t / 2, (y0 + y1) / 2, (z0 + z1) / 2), (z1 - z0, y1 - y0, abs(t)), pitch=-90, r=r, k=k)


# ---------------------------------------------------------------------------------- a cabine
def cab(p):
    x0, x1 = CAB_X
    Y = CAB_Y
    pts = []
    for s in (-1, 1):
        for (x, z) in CAB_PROF:
            if x < 1.0:
                pts.append((x, s * Y, z))
            elif z >= CAB_Z - 1e-6:
                # o canto de cima da frente: quebrado em planta (a cabine blindada facetada)
                pts += [(x - 0.07, s * Y, z), (x, s * (Y - 0.06), z)]
            else:
                pts += [(x - 0.09, s * Y, z), (x, s * (Y - 0.09), z)]
    solid(p, 'Paint#shell', pts)
    # ---------------------------------------------------------------- o para-brisa de 2 vidros na face inclinada, na borracha
    ax_, az_ = 2.182, 1.404
    bx_, bz_ = 1.98, CAB_Z
    L = math.hypot(ax_ - bx_, bz_ - az_)
    eu = Vector((bx_ - ax_, 0.0, bz_ - az_)).normalized()              # ao longo da face, pra cima
    en = Vector((bz_ - az_, 0.0, ax_ - bx_)).normalized()              # pra fora
    for (ya, yb) in ((0.05, 0.68), (-0.68, -0.05)):
        c = Vector((ax_, (ya + yb) / 2, az_)) + eu * (L * 0.5)
        vp.prism(p, 'Lens#vp', vp.rounded_rect(L * 0.70, yb - ya, 0.035, 3), c + en * 0.006, eu, Vector((0, 1, 0)), en, 0.012)
        vp.prism(p, 'Dark#vp', vp.rounded_rect(L * 0.80, yb - ya + 0.05, 0.045, 3), c - en * 0.002, eu, Vector((0, 1, 0)), en, 0.012)
    vp.rbox(p, 'Dark', (ax_ + (bx_ - ax_) * 0.5 + 0.012, 0.0, (az_ + bz_) / 2 + 0.006), (0.06, 0.05, L * 0.84), pitch=32, r=0.015, k=2)
    # a viseira (o quebra-sol de chapa por cima do para-brisa) e os limpadores
    vp.rbox(p, 'Paint#plate', (1.99, 0.0, CAB_Z + 0.022), (0.14, 1.66, 0.036), pitch=-14, r=0.03, k=4)
    for y in (-0.36, 0.36):
        tube(p, 'Dark', (2.165, y - 0.02, 1.43), (2.06, y + 0.12, 1.62), 0.008, seg=5)
    # ---------------------------------------------------------------- o nariz: a grade do radiador na face baixa, os faróis em cima
    # (a face baixa vai de (2,196; 0,756) a (2,282; 1,152): inclinada 12° pra frente)
    vp.rbox(p, 'Dark', (2.245, 0.0, 0.97), (0.016, 0.98, 0.3), pitch=-12.25, r=0.035, k=3)
    for k in range(6):
        vp.rbox(p, 'Paint#thin', (2.252 + k * 0.0018, 0.0, 0.855 + k * 0.046), (0.02, 0.92, 0.018), pitch=-30, r=0.006, k=1)
    for s in (-1, 1):
        # o farol redondo na caixa escura, a seta âmbar ao lado, a guarda de tubo
        vp.rbox(p, 'Paint#plate', (2.25, 0.62 * s, 1.25), (0.13, 0.27, 0.19), r=0.045, k=4)
        vp.lathe(p, 'Metal', (2.31, 0.58 * s, 1.25), 'x', [(0.0, -0.01), (0.062, -0.01), (0.072, 0.008), (0.072, 0.028), (0.062, 0.036),
                                                           (0.0, 0.03)], seg=16)
        vp.lathe(p, 'Light', (2.31, 0.58 * s, 1.25), 'x', [(0.0, 0.042), (0.05, 0.04), (0.058, 0.034), (0.0, 0.034)], seg=14)
        vp.rbox(p, 'Yellow', (2.318, 0.71 * s, 1.25), (0.02, 0.05, 0.07), r=0.012, k=2)
        vp.tube(p, 'Metal', vp.bent_bar([(2.31, 0.5 * s, 1.17), (2.37, 0.5 * s, 1.17), (2.37, 0.5 * s, 1.33), (2.37, 0.66 * s, 1.33),
                                         (2.37, 0.66 * s, 1.17), (2.31, 0.66 * s, 1.17)], R=0.035, n=2), 0.009, seg=5)
    # ---------------------------------------------------------------- as portas: o recorte, a janela na moldura, o painel azul, a
    # maçaneta, as dobradiças; os retrovisores nos braços de tubo; a escada da porta
    for s in (-1, 1):
        yo = CAB_Y * s
        n = Vector((0, s, 0))
        for (a, b) in (((0.79, yo, 0.80), (0.79, yo, 1.68)), ((1.40, yo, 0.80), (1.40, yo, 1.68)), ((0.79, yo, 0.80), (1.40, yo, 0.80))):
            vp.panel_seam(p, 'Dark', a, b, n, w=0.018, d=0.01)
        plate_y(p, 'Dark', 0.83, 1.36, 1.33, 1.61, yo, 0.007 * s, r=0.045, k=3)
        plate_y(p, 'Lens', 0.85, 1.34, 1.35, 1.59, yo, 0.011 * s, r=0.035, k=3)
        plate_y(p, 'Team', 0.84, 1.35, 0.92, 1.24, yo, 0.007 * s, r=0.035, k=3)
        vp.rbox(p, 'Metal', (1.30, yo + 0.018 * s, 1.27), (0.1, 0.025, 0.03), r=0.01, k=1)
        for z in (0.95, 1.52):
            vp.hinge(p, 'Metal', (0.80, yo + 0.016 * s, z), 'z', 0.11, r=0.016, knuckles=2)
        vp.grab_handle(p, 'Metal', (1.47, yo, 0.92), (1.47, yo, 1.38), n, height=0.06, r=0.013)
        # o retrovisor grande no braço de tubo dobrado
        arm = vp.bent_bar([(2.10, (CAB_Y - 0.02) * s, 1.36), (2.10, (CAB_Y + 0.16) * s, 1.36), (2.08, (CAB_Y + 0.18) * s, 1.50)], R=0.05, n=3)
        vp.tube(p, 'Dark', arm, 0.014, seg=6)
        tube(p, 'Dark', (2.06, (CAB_Y - 0.02) * s, 1.58), (2.07, (CAB_Y + 0.18) * s, 1.62), 0.012, seg=6)
        vp.rbox(p, 'Dark', (2.07, (CAB_Y + 0.19) * s, 1.53), (0.045, 0.14, 0.24), r=0.035, k=3)
        vp.rbox(p, 'Lens', (2.045, (CAB_Y + 0.19) * s, 1.53), (0.006, 0.11, 0.2), r=0.025, k=2)
        # a escada da porta (2 degraus de chapa com o miolo escuro) e as barras
        for zz in (0.36, 0.58):
            vp.rbox(p, 'Paint#plate', (0.97, (CAB_Y - 0.03) * s, zz), (0.3, 0.17, 0.028), r=0.025, k=2)
            vp.rbox(p, 'Dark', (0.97, (CAB_Y - 0.03) * s, zz + 0.015), (0.24, 0.12, 0.005), r=0.015, k=2)
        for xx in (0.83, 1.11):
            tube(p, 'Metal', (xx, (CAB_Y + 0.04) * s, 0.33), (xx, (CAB_Y + 0.04) * s, 0.76), 0.014, seg=6)
    # ---------------------------------------------------------------- o teto: a tampa com aba, a escotilha, o sensor, a antena
    vp.rbox_q(p, 'Paint#plate', (1.245, 0.0, CAB_Z + 0.022), (1.39, 1.82, 0.045), r=0.07, k=4)
    vp.hatch_round(p, (0.97, 0.0, CAB_Z + 0.044), 0.2, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
    vp.lathe(p, 'Paint', (1.116, -0.486, CAB_Z + 0.044), 'z', [(0.0, 0.0), (0.11, 0.0), (0.11, 0.03), (0.08, 0.05), (0.07, 0.08),
                                                             (0.0, 0.08)], seg=16)
    vp.rbox(p, 'Paint#plate', (1.116, -0.486, CAB_Z + 0.22), (0.34, 0.31, 0.19), r=0.05, k=4)
    vp.rbox(p, 'Dark', (1.288, -0.486, CAB_Z + 0.225), (0.01, 0.25, 0.12), r=0.025, k=2)
    vp.rbox(p, 'Lens', (1.293, -0.486, CAB_Z + 0.225), (0.008, 0.21, 0.085), r=0.02, k=2)
    vp.rbox(p, 'Team', (1.116, -0.486, CAB_Z + 0.318), (0.24, 0.22, 0.006), r=0.03, k=2)
    vp.lathe(p, 'Dark', (0.72, 0.612, CAB_Z + 0.044), 'z', [(0.0, 0.0), (0.04, 0.0), (0.04, 0.04), (0.028, 0.05), (0.024, 0.12),
                                                           (0.0, 0.13)], seg=10)
    p.cyl('Dark#vp', (0.72, 0.612, CAB_Z + 0.17 + 0.33), 'z', 0.006, 0.66, seg=6, bevel=0.0)
    for s in (-1, 1):
        vp.grab_handle(p, 'Metal', (0.66, 0.8 * s, CAB_Z + 0.044), (0.98, 0.8 * s, CAB_Z + 0.044), (0, 0, 1), height=0.05, r=0.012)
    # as juntas e os parafusos das chapas da cabine blindada
    for s in (-1, 1):
        vp.bolt_row(p, 'Metal', (0.66, (CAB_Y + 0.003) * s, 0.79), (2.06, (CAB_Y + 0.003) * s, 0.79), 10, (0, s, 0), r=0.014, h=0.01)
        vp.bolt_row(p, 'Metal', (0.66, (CAB_Y + 0.003) * s, 1.69), (1.86, (CAB_Y + 0.003) * s, 1.69), 8, (0, s, 0), r=0.014, h=0.01)
        vp.panel_seam(p, 'Dark', (1.62, CAB_Y * s, 0.80), (1.62, CAB_Y * s, 1.68), (0, s, 0), w=0.016, d=0.008)


# ---------------------------------------------------------------------------------- o casco inteiro
def hull():
    p = Part(PREFIX + '_Hull')
    p.bm('Paint#shell', 0.06)
    p.bm('Paint#plate', 0.025)
    p.bm('Paint#thin', 0.006)
    p.bm('Dark#plate', 0.02)
    p.bm('Yellow#plate', 0.025)
    p.bm('Paint#fender', 0.012)
    p.bm('Lens', 0.0)
    # ---------------------------------------------------------------- o chassi: as longarinas, as travessas, os eixos, as molas
    for y in (-0.396, 0.396):
        vp.rbox(p, 'Dark#plate', ((REAR + 0.07 + FRONT - 0.18) / 2, y, (FRAME_Z[0] + FRAME_Z[1]) / 2),
                (FRONT - 0.18 - REAR - 0.07, 0.11, FRAME_Z[1] - FRAME_Z[0]), r=0.025, k=2)
    for x in (-2.088, -1.404, -0.432, 0.432, 1.224, 1.944):
        vp.rbox(p, 'Dark', (x, 0.0, 0.6), (0.07, 0.7, 0.1), r=0.02, k=2)
    for x in AXLES_X:
        tube(p, 'Dark', (x, -0.74, WHEEL_R), (x, 0.74, WHEEL_R), 0.05, seg=12)
        vp.lathe(p, 'Dark', (x, 0.0, WHEEL_R), 'y', [(0.0, -0.15), (0.1, -0.15), (0.15, -0.09), (0.15, 0.09), (0.1, 0.15), (0.0, 0.15)], seg=14)
        vp.lathe(p, 'Dark', (x + 0.12, 0.0, WHEEL_R), 'x', [(0.0, 0.0), (0.06, 0.0), (0.06, 0.08), (0.0, 0.08)], seg=10)
        for s in (-1, 1):                                                  # o feixe de molas e os grampos
            for k in range(3):
                vp.rbox(p, 'Dark', (x, 0.396 * s, WHEEL_R + 0.07 + k * 0.026), (0.62 - k * 0.16, 0.075, 0.022), r=0.01, k=1)
            for d in (-1, 1):
                tube(p, 'Metal', (x + 0.06 * d, 0.396 * s, WHEEL_R + 0.03), (x + 0.06 * d, 0.396 * s, WHEEL_R + 0.16), 0.01, seg=5)
    # o tanque de combustível (esquerda) com as cintas e a tampa; a caixa de ferramentas e a de baterias (direita); os tanques de ar
    vp.lathe(p, 'Paint', (0.22, -0.70, 0.6), 'x', [(0.0, -0.36), (0.15, -0.36), (0.175, -0.34), (0.18, -0.3), (0.18, 0.3), (0.175, 0.34),
                                                  (0.15, 0.36), (0.0, 0.36)], seg=22)
    for xx in (-0.02, 0.46):
        vp.lathe(p, 'Dark', (xx, -0.70, 0.6), 'x', [(0.178, -0.02), (0.196, -0.02), (0.196, 0.02), (0.178, 0.02)], seg=22, closed=True)
    vp.lathe(p, 'Metal', (0.36, -0.70, 0.775), 'z', [(0.0, 0.0), (0.045, 0.0), (0.045, 0.03), (0.0, 0.035)], seg=10)
    vp.rbox_q(p, 'Paint#plate', (0.22, 0.69, 0.62), (0.68, 0.28, 0.3), r=0.04, k=4)
    vp.panel_seam(p, 'Dark', (-0.1, 0.832, 0.72), (0.54, 0.832, 0.72), (0, 1, 0), w=0.014, d=0.008)
    for xx in (0.0, 0.44):
        vp.rbox(p, 'Metal', (xx, 0.84, 0.69), (0.05, 0.02, 0.05), r=0.01, k=1)
    for y in (-0.55, 0.55):
        vp.lathe(p, 'Dark', (-0.25, y, 0.44), 'x', [(0.0, -0.22), (0.07, -0.22), (0.085, -0.2), (0.085, 0.2), (0.07, 0.22), (0.0, 0.22)], seg=14)
    # ---------------------------------------------------------------- a cabine
    cab(p)
    # ---------------------------------------------------------------- a frente: o para-choque, o guincho, os ganchos, a grade de canos
    vp.rbox_q(p, 'Paint#plate', (FRONT - 0.045, 0.0, 0.53), (0.2, 1.96, 0.09), pitch=-90, r=0.045, k=4)
    vp.lathe(p, 'Dark', (FRONT + 0.03, 0.0, 0.53), 'y', [(0.0, -0.21), (0.065, -0.21), (0.085, -0.19), (0.085, 0.19), (0.065, 0.21),
                                                        (0.0, 0.21)], seg=16)
    for s in (-1, 1):
        vp.rbox(p, 'Dark', (FRONT + 0.03, 0.25 * s, 0.53), (0.1, 0.045, 0.19), r=0.015, k=2)
        for d in (-1, 1):
            vp.rbox(p, 'Metal', (FRONT + 0.02, 0.6 * s + d * 0.04, 0.47), (0.08, 0.024, 0.09), r=0.015, k=2)
        p.torus('Dark#vp', (FRONT + 0.06, 0.6 * s, 0.43), 'y', 0.038, 0.012, seg=12, rseg=6)
        vp.lathe(p, 'Metal', (FRONT, 0.86 * s, 0.6), 'x', [(0.0, -0.02), (0.05, -0.02), (0.06, 0.0), (0.06, 0.03), (0.05, 0.036), (0.0, 0.036)],
                 seg=12)
        vp.lathe(p, 'Light', (FRONT, 0.86 * s, 0.6), 'x', [(0.0, 0.04), (0.042, 0.038), (0.05, 0.036), (0.0, 0.036)], seg=10)
    gx = 2.434
    for s in (-1, 1):
        # o quadro de tubo dobrado da grade (preso no para-choque), as escoras até a cabine
        vp.tube(p, 'Metal', vp.bent_bar([(FRONT - 0.02, 0.756 * s, 0.6), (gx, 0.756 * s, 0.6), (gx, 0.756 * s, 1.44),
                                         (gx, 0.0, 1.44)], R=0.07, n=4), 0.022, seg=8)
        tube(p, 'Metal', (gx, 0.756 * s, 1.44), (2.15, 0.756 * s, 1.44), 0.02, seg=8)
        tube(p, 'Metal', (gx, 0.36 * s, 0.6), (gx, 0.36 * s, 1.44), 0.018, seg=8)
    tube(p, 'Metal', (gx, 0.0, 0.6), (gx, 0.0, 1.44), 0.018, seg=8)
    for z in (0.62, 0.92, 1.18):
        tube(p, 'Metal', (gx, -0.756, z), (gx, 0.756, z), 0.017, seg=8)
    for s in (-1, 1):                                                       # os pés da grade no para-choque
        vp.rbox(p, 'Dark', (FRONT + 0.06, 0.756 * s, 0.62), (0.12, 0.06, 0.05), r=0.015, k=2)
    # ---------------------------------------------------------------- os para-lamas: o arco sobre a roda da frente, a chapa dobrada
    # sobre o par de trás; os para-barros
    for s in (-1, 1):
        xw = AXLES_X[0]
        outer = vp.arc_pts((xw, 0.0, WHEEL_R), (1, 0, 0), (0, 0, 1), WHEEL_R + 0.08, 18, 162, 12)
        inner = vp.arc_pts((xw, 0.0, WHEEL_R), (1, 0, 0), (0, 0, 1), WHEEL_R + 0.055, 18, 162, 12)
        rings = []
        for qo, qi in zip(outer, inner):
            rings.append([(qo.x, (WHEEL_Y - 0.17) * s, qo.z), (qo.x, (WHEEL_Y + 0.17) * s, qo.z), (qi.x, (WHEEL_Y + 0.17) * s, qi.z),
                          (qi.x, (WHEEL_Y - 0.17) * s, qi.z)])
        p.loft('Paint#fender', rings)
        rear = vp.bent_bar([(-2.39, 0.0, 0.54), (-2.31, 0.0, 0.79), (-0.50, 0.0, 0.79), (-0.43, 0.0, 0.54)], R=0.08, n=4)
        strip_xz(p, 'Paint#fender', rear, WHEEL_Y * s, 0.32, 0.026)
        for xx in (-2.0, -1.4, -0.8):                                      # os suportes do para-lama na longarina
            tube(p, 'Dark', (xx, 0.45 * s, 0.68), (xx, 0.78 * s, 0.775), 0.016, seg=6)
        vp.mudflap(p, (1.16, WHEEL_Y * s, 0.76), 0.28, 0.34, yaw=0, m='Rubber')
        vp.mudflap(p, (-2.27, WHEEL_Y * s, 0.72), 0.3, 0.4, yaw=0, m='Rubber')
        # ---------------------------------------------------------------- a saia blindada entre a cabine e o par de trás, com a faixa azul
        # segmentada (a assinatura do A) e as fileiras de parafusos
        yo = 0.92 * s
        plate_y(p, 'Paint#plate', -0.38, 0.72, 0.64, 0.92, yo, 0.06 * s, r=0.045, k=4)
        yb = yo + 0.06 * s
        for (a, b) in ((-0.3, 0.16), (0.24, 0.64)):
            plate_y(p, 'Team', a, b, 0.72, 0.84, yb, 0.006 * s, r=0.02, k=2)
        vp.panel_seam(p, 'Dark', (0.2, yb, 0.65), (0.2, yb, 0.91), (0, s, 0), w=0.016, d=0.008)
        vp.bolt_row(p, 'Metal', (-0.32, yb, 0.885), (0.66, yb, 0.885), 9, (0, s, 0), r=0.014, h=0.01)
        vp.bolt_row(p, 'Metal', (-0.32, yb, 0.675), (0.66, yb, 0.675), 9, (0, s, 0), r=0.014, h=0.01)
    # ---------------------------------------------------------------- a caixa do motor atrás da cabine: as venezianas, a chaminé com a
    # capa de calor e o cotovelo, o filtro de ar
    ex0, ex1 = ENG_X
    vp.rbox_q(p, 'Paint#plate', ((ex0 + ex1) / 2, 0.0, (CAB_Z0 + ENG_TOP) / 2), (ENG_TOP - CAB_Z0, 1.52, ex1 - ex0), pitch=-90, r=0.05, k=4)
    louvers(p, (ex0 - 0.002, 0.0, 1.13), (-1, 0, 0), (0, 1, 0), 1.18, 0.42, 6, tilt=-30)
    sx, sy = 0.446, -0.634
    vp.lathe(p, 'Dark', (sx, sy, ENG_TOP), 'z', [(0.0, 0.0), (0.1, 0.0), (0.1, 0.03), (0.065, 0.05), (0.065, 0.56), (0.07, 0.6), (0.0, 0.6)],
             seg=14)
    vp.lathe(p, 'Metal', (sx, sy, ENG_TOP + 0.14), 'z', [(0.078, 0.0), (0.09, 0.016), (0.09, 0.36), (0.078, 0.376), (0.074, 0.376),
                                                        (0.074, 0.0)], seg=16, closed=True)
    for k in range(5):
        vp.rbox(p, 'Dark', (sx - 0.089, sy, ENG_TOP + 0.19 + k * 0.06), (0.012, 0.06, 0.024), r=0.004, k=1)
    elbow = vp.bent_bar([(sx, sy, ENG_TOP + 0.58), (sx, sy, ENG_TOP + 0.66), (sx - 0.12, sy, ENG_TOP + 0.71)], R=0.06, n=4)
    vp.tube(p, 'Dark', elbow, 0.062, seg=12)
    fx, fy = 0.50, -0.27
    vp.lathe(p, 'Paint', (fx, fy, ENG_TOP), 'z', [(0.0, 0.0), (0.12, 0.0), (0.13, 0.02), (0.13, 0.34), (0.11, 0.37), (0.06, 0.38),
                                                 (0.06, 0.42), (0.0, 0.42)], seg=18)
    vp.lathe(p, 'Dark', (fx, fy, ENG_TOP + 0.41), 'z', [(0.0, 0.0), (0.11, 0.0), (0.135, 0.015), (0.13, 0.04), (0.05, 0.055), (0.0, 0.055)],
             seg=18)
    for z in (0.08, 0.26):
        vp.lathe(p, 'Metal', (fx, fy, ENG_TOP + z), 'z', [(0.127, -0.016), (0.142, -0.016), (0.142, 0.016), (0.127, 0.016)], seg=18,
                 closed=True)
    tube(p, 'Dark', (fx + 0.13, fy, ENG_TOP + 0.2), (ex1 + 0.02, fy + 0.05, ENG_TOP + 0.2), 0.04, seg=10)
    # ---------------------------------------------------------------- a caçamba: o assoalho, as longarinas da caçamba, as laterais
    # baixas com as nervuras e os montantes, a cabeceira, a tampa de trás
    bx0, bx1 = BED_X
    vp.rbox_q(p, 'Paint#plate', ((bx0 + bx1) / 2, 0.0, BED_Z - 0.036), (bx1 - bx0, 2 * BED_HW - 0.1, 0.072), r=0.04, k=4)
    for k in range(7):
        x = bx0 + 0.24 + k * 0.36
        vp.panel_seam(p, 'Dark', (x, -BED_HW + 0.08, BED_Z + 0.001), (x, BED_HW - 0.08, BED_Z + 0.001), (0, 0, 1), w=0.014, d=0.008)
    for y in (-0.4, 0.4):
        vp.rbox(p, 'Dark#plate', ((bx0 + bx1) / 2, y, BED_Z - 0.1), (bx1 - bx0 - 0.1, 0.1, 0.07), r=0.02, k=2)
    for k in range(7):
        vp.rbox(p, 'Dark', (bx0 + 0.18 + k * 0.38, 0.0, BED_Z - 0.095), (0.06, 1.9, 0.055), r=0.015, k=2)
    for s in (-1, 1):
        yo = (BED_HW - 0.057) * s
        plate_y(p, 'Paint#plate', bx0, bx1, BED_Z - 0.07, BED_Z + SIDE_H, yo, 0.057 * s, r=0.03, k=4)
        yb = BED_HW * s
        # as 2 nervuras prensadas (as duas linhas que pegam o sol) e a régua escura de amarração
        for z in (BED_Z + 0.08, BED_Z + 0.3):
            vp.rbox(p, 'Paint#thin', ((bx0 + bx1) / 2, yb + 0.006 * s, z), (bx1 - bx0 - 0.16, 0.014, 0.034), r=0.012, k=2)
        vp.rbox(p, 'Dark', ((bx0 + bx1) / 2, yb + 0.004 * s, BED_Z + 0.19), (bx1 - bx0 - 0.3, 0.01, 0.05), r=0.012, k=2)
        # os montantes (os bolsos das estacas) e as dobradiças de baixo
        for x in (-2.196, -1.332, -0.468, 0.108):
            vp.rbox(p, 'Paint#plate', (x, yb + 0.016 * s, BED_Z + 0.16), (0.07, 0.034, SIDE_H + 0.17), r=0.02, k=3)
        for x in (-1.76, -0.9, -0.18):
            vp.hinge(p, 'Metal', (x, yb + 0.012 * s, BED_Z - 0.06), 'x', 0.16, r=0.016, knuckles=3)
        # o corrimão de cima e os ganchos de amarração por baixo
        tube(p, 'Paint', (bx0 + 0.03, (BED_HW - 0.028) * s, BED_Z + SIDE_H + 0.012), (bx1 - 0.03, (BED_HW - 0.028) * s, BED_Z + SIDE_H + 0.012),
             0.03, seg=10)
        for x in (-2.0, -1.55, -1.1, -0.65, -0.2):
            p.torus('Metal#vp', (x, (BED_HW + 0.02) * s, BED_Z - 0.1), 'x', 0.03, 0.009, seg=10, rseg=5)
    # a cabeceira: a chapa, os montantes, a barra de cima, os berços da lona; os galões e a caixa-forte no vão da frente das caixas
    hx = bx1 - 0.058
    plate_x(p, 'Paint#plate', -BED_HW, BED_HW, BED_Z - 0.07, BED_Z + HEAD_H, hx, 0.058, r=0.04, k=4)
    for y in (-0.62, 0.0, 0.62):
        vp.rbox(p, 'Paint#plate', (hx - 0.016, y, BED_Z + HEAD_H / 2), (0.034, 0.07, HEAD_H), r=0.02, k=3)
    vp.rbox(p, 'Paint#thin', (hx - 0.008, 0.0, BED_Z + 0.36), (0.016, 2 * BED_HW - 0.12, 0.034), r=0.012, k=2)
    tube(p, 'Paint', (hx + 0.03, -BED_HW + 0.03, BED_Z + HEAD_H + 0.012), (hx + 0.03, BED_HW - 0.03, BED_Z + HEAD_H + 0.012), 0.032, seg=10)
    for y in (-0.82, -0.22):                                                # os berços da lona enrolada
        vp.rbox(p, 'Dark', (hx + 0.03, y, BED_Z + HEAD_H + 0.04), (0.12, 0.05, 0.05), r=0.015, k=2)
    vp.tarp_roll(p, (hx + 0.03 - 0.04, -0.518, BED_Z + HEAD_H + 0.17), 'y', 0.92, 0.135, m='Team', straps=3, strap_m='Dark', buckle_m='Metal')
    for y in (-0.76, -0.40):
        vp.jerrycan(p, (0.08, y, BED_Z + 0.235), yaw=90, m='Metal', cap_m='Dark')
    tube(p, 'Dark', (0.17, -0.96, BED_Z + 0.32), (0.17, -0.3, BED_Z + 0.32), 0.012, seg=6)
    vp.ammo_box(p, (0.085, 0.62, BED_Z + 0.13), size=(0.46, 0.2, 0.26), yaw=90, m='Metal', latch_m='Dark')
    # a tampa de trás: a chapa, as nervuras, as travas dos cantos, as dobradiças
    plate_x(p, 'Paint#plate', -BED_HW, BED_HW, BED_Z - 0.07, BED_Z + SIDE_H, bx0, 0.058, r=0.03, k=4)
    for z in (BED_Z + 0.08, BED_Z + 0.3):
        vp.rbox(p, 'Paint#thin', (bx0 - 0.006, 0.0, z), (0.014, 2 * BED_HW - 0.2, 0.034), r=0.012, k=2)
    for s in (-1, 1):
        vp.rbox(p, 'Metal', (bx0 - 0.012, 0.9 * s, BED_Z + SIDE_H - 0.06), (0.03, 0.08, 0.05), r=0.012, k=2)
        vp.hinge(p, 'Metal', (bx0 - 0.012, 0.6 * s, BED_Z - 0.06), 'y', 0.2, r=0.016, knuckles=3)
    # ---------------------------------------------------------------- a traseira: o para-choque com a faixa de aviso, as lanternas, o engate
    vp.rbox_q(p, 'Paint#plate', (REAR + 0.05, 0.0, 0.53), (0.18, 1.86, 0.1), pitch=-90, r=0.04, k=4)
    for k in range(8):
        y0 = -0.8 + k * 0.2
        vp.rbox(p, 'Yellow' if k % 2 == 0 else 'Dark', (REAR - 0.002, y0 + 0.1, 0.53), (0.008, 0.2, 0.12), r=0.0005, k=1)
    for s in (-1, 1):
        vp.rbox(p, 'Dark', (REAR + 0.02, 0.8 * s, 0.68), (0.05, 0.24, 0.11), r=0.02, k=2)
        vp.rbox(p, 'Light', (REAR - 0.008, 0.75 * s, 0.68), (0.012, 0.08, 0.065), r=0.015, k=1)
        vp.rbox(p, 'Lens', (REAR - 0.008, 0.86 * s, 0.68), (0.012, 0.08, 0.065), r=0.015, k=1)
        tube(p, 'Dark', (REAR + 0.07, 0.8 * s, 0.62), (REAR + 0.07, 0.8 * s, 0.86), 0.014, seg=6)
    vp.rbox(p, 'Metal', (REAR - 0.06, 0.0, 0.5), (0.13, 0.13, 0.1), r=0.03, k=2)
    p.torus('Dark#vp', (REAR - 0.13, 0.0, 0.5), 'z', 0.05, 0.016, seg=14, rseg=6)
    # ---------------------------------------------------------------- o guindaste amarelo (a assinatura do coletor): a base giratória
    # com o anel de porcas, a coluna, a lança deitada pra trás por cima da carga com a extensão, o cilindro hidráulico de verdade, o
    # gancho no cabo, as mangueiras, o comando de alavancas e o berço da lança na tampa de trás
    cx, cy = CRANE
    vp.lathe(p, 'Yellow', (cx, cy, ENG_TOP), 'z', [(0.0, 0.0), (0.19, 0.0), (0.19, 0.035), (0.16, 0.05), (0.15, 0.1), (0.0, 0.1)], seg=22)
    vp.bolt_ring(p, 'Metal', (cx, cy, ENG_TOP + 0.035), 'z', 0.172, 10, r=0.012, h=0.01)
    vp.rbox_q(p, 'Yellow#plate', (cx, cy, ENG_TOP + 0.36), (0.24, 0.24, 0.52), r=0.05, k=4)
    vp.rbox(p, 'Yellow#plate', (cx + 0.02, cy, ENG_TOP + 0.16), (0.32, 0.3, 0.12), r=0.04, k=4)
    pin = Vector((cx, cy, 2.05))
    vp.lathe(p, 'Yellow', pin, (0, 1, 0), [(0.0, -0.15), (0.09, -0.15), (0.1, -0.13), (0.1, 0.13), (0.09, 0.15), (0.0, 0.15)], seg=18)
    vp.lathe(p, 'Metal', pin, (0, 1, 0), [(0.0, -0.17), (0.035, -0.17), (0.035, 0.17), (0.0, 0.17)], seg=10)
    boom(p, 'Yellow#plate', (cx + 0.08, cy, 2.04), (-1.96, cy, 1.975), 0.17, 0.18, r=0.05)
    boom(p, 'Yellow#plate', (-1.8, cy, 1.972), (-2.22, cy, 1.965), 0.13, 0.13, r=0.04)
    for x in (-0.2, -1.0, -1.7):                                            # as chapas de reforço na lança
        vp.rbox(p, 'Yellow#plate', (x, cy, 1.99), (0.05, 0.19, 0.2), r=0.02, k=2)
    vp.lathe(p, 'Metal', (-2.2, cy, 1.94), (0, 1, 0), [(0.0, -0.08), (0.055, -0.08), (0.06, -0.06), (0.06, 0.06), (0.055, 0.08), (0.0, 0.08)],
             seg=14)                                                         # a roldana da ponta
    cylinder(p, (cx - 0.08, cy, 1.70), (-0.92, cy, 1.885), 0.05, 0.026, frac=0.58, seg=14)
    vp.rbox(p, 'Yellow#plate', (-0.92, cy, 1.9), (0.1, 0.16, 0.06), r=0.02, k=2)
    tube(p, 'Dark', (-2.2, cy, 1.88), (-2.2, cy, 1.80), 0.008, seg=5)       # o cabo e o moitão com o gancho
    vp.lathe(p, 'Yellow', (-2.2, cy, 1.76), 'y', [(0.0, -0.03), (0.04, -0.03), (0.045, -0.02), (0.045, 0.02), (0.04, 0.03), (0.0, 0.03)], seg=12)
    vp.tube(p, 'Dark', vp.arc_pts((-2.2, cy, 1.69), (1, 0, 0), (0, 0, 1), 0.035, -200, 40, 7), 0.011, seg=6)
    tube(p, 'Dark', (-2.2, cy, 1.725), (-2.2, cy, 1.70), 0.012, seg=6)
    vp.tube(p, 'Dark', vp.bent_bar([(cx - 0.12, cy - 0.05, ENG_TOP + 0.12), (cx - 0.2, cy - 0.06, 1.62), (cx - 0.12, cy - 0.04, 1.68)],
                                   R=0.06, n=3), 0.012, seg=5)
    vp.rbox(p, 'Dark#plate', (cx - 0.08, cy + 0.2, ENG_TOP + 0.13), (0.14, 0.1, 0.14), r=0.025, k=2)   # o comando
    for k in range(3):
        tube(p, 'Metal', (cx - 0.12 + k * 0.04, cy + 0.2, ENG_TOP + 0.2), (cx - 0.12 + k * 0.04, cy + 0.22, ENG_TOP + 0.3), 0.008, seg=5)
        vp.lathe(p, 'Dark', (cx - 0.12 + k * 0.04, cy + 0.22, ENG_TOP + 0.3), 'z', [(0.0, 0.0), (0.014, 0.0), (0.016, 0.015), (0.0, 0.03)], seg=8)
    rx = bx0 - 0.03
    tube(p, 'Yellow', (rx, cy, BED_Z + SIDE_H), (rx, cy, 1.86), 0.028, seg=10)
    vp.tube(p, 'Yellow', vp.bent_bar([(rx, cy - 0.12, 1.94), (rx, cy - 0.12, 1.86), (rx, cy + 0.12, 1.86), (rx, cy + 0.12, 1.94)], R=0.04, n=3),
            0.018, seg=8)
    vp.poke_ngons(p)
    return p.finish()


# ---------------------------------------------------------------------------------- rodas
def wheels():
    """6 rodas (a ordem do A): o pneu de verdade (model_vehicle_parts.tyre), a roda de aço rebaixada com os furos, o cubo, as porcas."""
    objs = []
    n = 0
    for x in AXLES_X:
        for s in (-1, 1):
            n += 1
            w = Part(f'wheel_{n:03d}')
            vp.tyre(w, (x, WHEEL_Y * s, WHEEL_R), WHEEL_R, WHEEL_W, s, rim_r=0.215, rubber='Rubber', rim='Paint', hub='Metal', seg=32,
                    holes_m='Dark', style='mud', n_lugs=24, lug_h=0.022)
            objs.append(w.finish())
    return objs


# ---------------------------------------------------------------------------------- a caixa da carga
def crate():
    """A caixa da carga (1,08 × 0,936 × 0,734 m, origem no centro do pé): o estrado de metal com os 3 patins e o vão do garfo, o
    corpo de tábuas (as frestas) com a moldura de sarrafos e as cantoneiras de metal nas 4 quinas, a tampa com aba e a FAIXA AMARELA,
    as 2 cintas que abraçam a caixa com a fivela, as alças de corda nas pontas e a placa clara do estêncil dos lados."""
    p = Part(CRATE_NAME)
    p.bm('Crate#plate', 0.02)
    p.bm('Trim#plate', 0.012)
    L, W, H = CRATE
    hl, hw = L / 2, W / 2
    z0 = 0.105                                                              # o pé da caixa (em cima do estrado)
    zt = H - 0.035                                                          # o pé da tampa
    # o estrado: 3 patins ao longo de x (o vão do garfo entre eles) e as tábuas de cima
    for y in (-hw + 0.07, 0.0, hw - 0.07):
        vp.rbox(p, 'Trim', (0.0, y, 0.035), (L - 0.02, 0.12, 0.07), r=0.02, k=2)
    vp.rbox_q(p, 'Trim#plate', (0.0, 0.0, 0.088), (L, W, 0.034), r=0.025, k=3)
    # o corpo e as tábuas
    vp.rbox_q(p, 'Crate#plate', (0.0, 0.0, (z0 + zt) / 2), (L - 0.05, W - 0.05, zt - z0), r=0.03, k=4)
    for z in (z0 + 0.15, z0 + 0.3, z0 + 0.45):
        for s in (-1, 1):
            vp.panel_seam(p, 'Trim', (-hl + 0.08, (hw - 0.025) * s, z), (hl - 0.08, (hw - 0.025) * s, z), (0, s, 0), w=0.012, d=0.006)
            vp.panel_seam(p, 'Trim', ((hl - 0.025) * s, -hw + 0.08, z), ((hl - 0.025) * s, hw - 0.08, z), (s, 0, 0), w=0.012, d=0.006)
    # os sarrafos da moldura (a cinta de madeira do meio e os montantes) em relevo
    for s in (-1, 1):
        vp.rbox(p, 'Crate', (0.0, (hw - 0.012) * s, z0 + 0.3), (L - 0.12, 0.03, 0.07), r=0.015, k=2)
        vp.rbox(p, 'Crate', ((hl - 0.012) * s, 0.0, z0 + 0.3), (0.03, W - 0.12, 0.07), r=0.015, k=2)
        for x in (-0.18, 0.18):
            vp.rbox(p, 'Crate', (x, (hw - 0.012) * s, (z0 + zt) / 2), (0.07, 0.03, zt - z0 - 0.04), r=0.015, k=2)
    # as cantoneiras de metal nas 4 quinas de pé (o L de chapa), com os rebites
    for sx in (-1, 1):
        for sy in (-1, 1):
            c = Vector((sx * (hl - 0.025), sy * (hw - 0.025), (z0 + zt) / 2))
            ex, ey = Vector((sx, 0, 0)), Vector((0, sy, 0))
            pts = [(-0.07, 0.012), (0.012, 0.012), (0.012, -0.07), (0.0, -0.07), (0.0, 0.0), (-0.07, 0.0)]
            vp.prism(p, 'Trim', pts, c - Vector((0, 0, (zt - z0) / 2 - 0.01)), ex, ey, Vector((0, 0, 1)), zt - z0 - 0.02)
            for z in (zt - 0.08,):
                vp.nut(p, 'Trim', c + ex * 0.014 + ey * -0.04 + Vector((0, 0, z - (z0 + zt) / 2)), ex, r=0.01, h=0.008)
                vp.nut(p, 'Trim', c + ey * 0.014 + ex * -0.04 + Vector((0, 0, z - (z0 + zt) / 2)), ey, r=0.01, h=0.008)
    # a tampa com a aba e a faixa amarela atravessada (lê de cima: a caixa de suprimento do C&C)
    vp.rbox_q(p, 'Crate#plate', (0.0, 0.0, zt + 0.0175), (L - 0.02, W - 0.02, 0.035), r=0.035, k=4)
    vp.rbox(p, 'Yellow', (0.0, 0.0, H + 0.002), (0.2, W - 0.06, 0.006), r=0.008, k=2)
    # as 2 cintas (abraçam a caixa no sentido de y, por cima da tampa) e a fivela
    for x in (-0.3, 0.3):
        path = vp.bent_bar([(x, -hw - 0.004, z0 + 0.02), (x, -hw - 0.004, H + 0.006), (x, hw + 0.004, H + 0.006), (x, hw + 0.004, z0 + 0.02)],
                           R=0.03, n=3)
        rings = []
        for i, q in enumerate(path):
            d = path[min(i + 1, len(path) - 1)] - path[max(i - 1, 0)]
            nn = Vector((0.0, -d.z, d.y)).normalized()                    # a normal no plano yz
            rings.append([tuple(q + Vector((-0.024, 0, 0)) + nn * 0.004), tuple(q + Vector((0.024, 0, 0)) + nn * 0.004),
                          tuple(q + Vector((0.024, 0, 0)) - nn * 0.004), tuple(q + Vector((-0.024, 0, 0)) - nn * 0.004)])
        p.loft('Trim#strap', rings, bevel=0.0)
        vp.rbox(p, 'Trim', (x, hw + 0.012, z0 + 0.36), (0.07, 0.016, 0.06), r=0.012, k=2)
    # as alças de corda nas pontas (o laço preso em 2 olhais)
    for s in (-1, 1):
        for y in (-0.22, 0.22):
            vp.rbox(p, 'Trim', (s * (hl + 0.004), y, z0 + 0.42), (0.012, 0.05, 0.05), r=0.01, k=1)
        vp.tube(p, 'Trim', vp.bent_bar([(s * (hl + 0.008), -0.21, z0 + 0.42), (s * (hl + 0.02), -0.17, z0 + 0.32),
                                        (s * (hl + 0.02), 0.17, z0 + 0.32), (s * (hl + 0.008), 0.21, z0 + 0.42)], R=0.05, n=3), 0.011, seg=6)
    # a placa clara do estêncil (o símbolo da carga) nos 2 lados compridos
    for s in (-1, 1):
        plate_y(p, 'Paint', -0.12, 0.12, z0 + 0.16, z0 + 0.28, (hw - 0.002) * s, 0.006 * s, r=0.015, k=2)
    vp.poke_ngons(p)
    return p.finish()


# ---------------------------------------------------------------------------------- exportação
def write_spec():
    import json
    parts = [[PREFIX + '_Hull', [0, 0, 0]], [PREFIX + '_Wheels', [0, 0, 0]]]
    parts += [[CRATE_NAME, [x, -y, z]] for (x, y, z) in SLOTS]
    views = [['top', 'ortho', [0, 0, -1], [0, -1, 0], [1800, 1000]],
             ['left', 'ortho', [0, -1, 0], [0, 0, 1], [1800, 820]],
             ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [900, 900]],
             ['back', 'ortho', [1, 0, 0], [0, 0, 1], [900, 900]],
             ['three_quarter', 'persp', 27.0, 232.0, [1600, 1000], 30.0, 0.8],
             ['three_quarter_right', 'persp', 30.0, 40.0, [1600, 1000], 30.0, 0.8]]
    game = [['game_cam', 37.5, 35.0, 50.0, 0.8], ['game_cam_close', 37.5, 35.0, 28.0, 0.75]]
    spec = {'parts': parts, 'views': views, 'usa_at': [0, 6.0, 0], 'tank_views': False, 'game': game}
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)
    # a caixa sozinha (a vista de perto)
    spec_c = {'parts': [[CRATE_NAME, [0, 0, 0]]],
              'views': [['crate_three_quarter', 'persp', 30.0, 215.0, [1200, 900], 35.0, 0.9],
                        ['crate_top', 'ortho', [0, 0, -1], [-1, 0, 0], [900, 900]]],
              'usa_at': [0.0, 3.5, 0], 'tank_views': False, 'game': [['crate_game_cam_close', 37.5, 200.0, 28.0, 0.75]]}
    with open(os.path.join(OUT, 'preview_crate.json'), 'w', encoding='utf-8') as f:
        json.dump(spec_c, f, ensure_ascii=False, indent=1)


def main():
    clear_scene()
    h = hull()
    ws = wheels()
    c = crate()
    import model_lib
    detail = model_lib.DETAIL.get('defer') is not None
    # (o truque do Dozer B) a caixa cozinha longe do caminhão e volta pra origem só na hora de exportar
    if detail:
        c.data.transform(Matrix.Translation(V(*CRATE_BAKE_AT)))
        _orig = model_lib.export

        def _export(o, out_dir):
            if o.name == CRATE_NAME and model_lib.DETAIL.get('defer') is None:
                o.data.transform(Matrix.Translation(-V(*CRATE_BAKE_AT)))
            return _orig(o, out_dir)
        model_lib.export = _export
    total = export(h, OUT)
    total += export_group(ws, OUT, PREFIX + '_Wheels')
    total += export(c, OUT)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL tris por peca', {h.name: tris(h), 'wheels': sum(tris(w) for w in ws), c.name: tris(c)})
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h] + ws)
    print('MODEL medidas: comprimento %.2f m (x %.2f..%.2f), largura %.2f m (com os retrovisores), altura %.2f m'
          % (ax1 - ax0, ax0, ax1, ay1 - ay0, az1))
    print('MODEL cargo.slots_m', [list(s) for s in SLOTS], 'caixa', CRATE)
    print('MODEL pneu: raio %.3f, largura %.3f (tyre_w_m), eixos %s, centro da roda y +-%.2f' % (WHEEL_R, WHEEL_W, AXLES_X, WHEEL_Y))
    write_spec()


main()
