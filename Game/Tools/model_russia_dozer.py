"""O trator de engenharia da Rússia (IMR-3M, o construtor) provisório, modelado no Blender por script (01/11/2026; docs/FACCAO_RUSSIA.md
§5.2: "o trator blindado sobre o T-90, com a lâmina e o braço").

Rodar (Blender sem janela, só CPU; não usa a placa), pelo executor do detalhe:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_russia_dozer.py C:/Users/luisf/explosion/captures/game/models/russia_dozer

A base é o IMR-3M de verdade (o veículo de engenharia sobre o chassi do T-90): o casco do T-72/T-90 do kit (sem os tijolos, a saia de
borracha lisa), a LÂMINA EM V na frente (as 2 asas côncavas formando a ponta, o fio de corte, os braços curtos até o pivô no nariz), a
mesa giratória com a cabine blindada do operador (o hexágono com os periscópios, a sirene) e o BRAÇO TELESCÓPICO COM A GARRA dobrado
pra trás por cima do motor (a posição de marcha: a garra passa da traseira, como o de verdade, 9,3 m no total).
A palavra: o BRAÇO COM A GARRA e a lâmina em V. De cima: a ponta da lâmina na frente, a cabine hexagonal com a gaiola pequena e a rede,
o braço comprido correndo pra trás e a garra aberta passando da traseira.
Cor do jogador (slot Team): a rede na gaiola da cabine, a lona enrolada no motor e a lona dobrada na mesa.

Peças (origem no chão, no centro do casco, frente em +X; a mesma organização do Dozer da China):
  SM_Russia_Dozer_Hull     casco, a mesa, a cabine com a gaiola, o braço e a garra (em marcha), a cúpula da sirene, os cilindros;
  SM_Russia_Dozer_Blade    a lâmina em V com os braços: a ORIGEM no pivô dos braços (BLADE_PIVOT; blade.at_m; o jogo gira em Y);
  SM_Russia_Dozer_Beacon   a luz que gira dentro da cúpula (a ORIGEM no eixo: BEACON; spin at_m, ~90 rpm);
  SM_Russia_Dozer_Wheels   a rodagem (o executor separa em wheel_*);
  SM_Russia_Dozer_Track_l / _Track_r  as esteiras.
Slots: Paint, Metal, Team, Track, Lens, Cloth, Rubber, Dark, Light, Beacon (a cúpula âmbar que pisca, o slot do Dozer da USA e da China).
"""
import math
import os
import sys

from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import V, clear_scene, export  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_russia_vehicle_kit as rk  # noqa: E402
from model_russia_vehicle_kit import T72, bounds, frame_box, solid, tris  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'russia_dozer')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_Russia_Dozer'

BLADE_PIVOT = (3.22, 0.0, 0.62)          # o pivô dos braços da lâmina, no nariz de baixo (blade.at_m)
BLADE_TIP_X = 4.3                        # a ponta do V
BLADE_HW = 1.74                          # a meia-largura (as asas: ~3,9 m com as orelhas, um pouco mais que as esteiras, 3,4)
BLADE_Z0, BLADE_H = 0.2, 0.95            # o fio a 0,2 m do chão em marcha; a altura da lâmina
TT = (0.15, 0.0)                         # a mesa giratória (no lugar do anel da torre)
CAB = (0.62, -0.55, 1.92, 2.62)          # a cabine do operador: x, y, o piso (em cima da mesa), o teto
BOOM_PIVOT = (0.2, 0.5, 2.2)             # o pivô do braço (eixo Y)
BOOM_END = (-3.4, 0.5, 2.02)            # a ponta do braço (o punho da garra)
BEACON = (CAB[0] - 0.62, CAB[1] - 0.6, 3.16)        # a sirene no canto de trás da gaiola (por cima)
GAME_SCALE = 0.78                        # (01/11) o tamanho do jogo (main): o trator de apoio menor que o tanque


def beam(p, m, a, b, w, h, up=(0, 0, 1), r=0.03):
    """A viga de seção retangular de a até b (largura w, altura h), de cantos redondos."""
    a, b = Vector(a), Vector(b)
    ex = (b - a).normalized()
    ez = (Vector(up) - ex * Vector(up).dot(ex)).normalized()
    ey = ez.cross(ex)
    vp.prism(p, m, vp.rounded_rect(w, h, r, 2), a, ey, ez, ex, (b - a).length)


def cylinder(p, a, b, r_body, r_rod, split=0.55):
    """O cilindro hidráulico de a até b: o corpo (Paint) e a haste cromada (Metal), os olhais nas pontas."""
    a, b = Vector(a), Vector(b)
    m = a + (b - a) * split
    vp.tube(p, 'Paint', [a, m], r_body, seg=12)
    vp.tube(p, 'Metal', [m, b], r_rod, seg=10)
    for q in (a, b):
        p.cyl('Metal#vp', tuple(q), 'y', r_body * 1.1, r_body * 1.6, seg=10, bevel=0.0)


# ---------------------------------------------------------------------------------- casco
def hull():
    p = rk.new_part(PREFIX + '_Hull')
    p.bm('Beacon', 0.0)
    rk.t72_hull(p, era_glacis=False, era_skirt=False, rear_slats=False, tarp_roll=True, front_blade=False, fuel_drums=False)
    # o glacis do IMR: a chapa lisa com as orelhas dos cilindros da lâmina e os 2 cilindros (o corpo no casco; a haste vai na lâmina)
    for s in (-1, 1):
        x = 3.0
        z = rk.glacis_z(x)
        vp.rbox(p, 'Paint#plate', (x, 0.72 * s, z + 0.08), (0.24, 0.2, 0.16), r=0.04, k=2)
        a = Vector((x, 0.72 * s, z + 0.12))
        b = Vector((3.62, 0.72 * s, 0.98))
        vp.tube(p, 'Paint', [a, a + (b - a) * 0.6], 0.075, seg=12)
        p.cyl('Metal#vp', tuple(a), 'y', 0.08, 0.14, seg=10, bevel=0.0)
    # as orelhas do pivô dos braços na chapa de baixo do nariz
    for s in (-1, 1):
        vp.rbox(p, 'Paint#plate', (BLADE_PIVOT[0] - 0.02, 0.98 * s, BLADE_PIVOT[2]), (0.28, 0.08, 0.3), r=0.04, k=2)
    # ---------------------------------------------------------------- a mesa giratória (o hexágono baixo no lugar da torre)
    tx, ty = TT
    pts = []
    for k in range(6):
        a = math.radians(30 + 60 * k)
        for z in (T72.DECK + 0.04, 1.9):
            r = 1.15 if z < 1.6 else 1.08
            pts.append((tx + r * math.cos(a) * 1.05, ty + r * math.sin(a), z))
    solid(p, 'Paint#shell', pts, bevel=0.05)
    vp.lathe(p, 'Dark', (tx, ty, T72.DECK), 'z', [(1.0, 0.0), (1.12, 0.0), (1.12, 0.04), (1.0, 0.04)], seg=48, closed=True)
    # ---------------------------------------------------------------- a cabine blindada do operador (o hexágono alto com os periscópios)
    cx, cy, cz0, cz1 = CAB
    pts = []
    for k in range(6):
        a = math.radians(60 * k)
        pts += [(cx + 0.62 * math.cos(a), cy + 0.56 * math.sin(a), cz0), (cx + 0.52 * math.cos(a), cy + 0.47 * math.sin(a), cz1)]
    solid(p, 'Paint#shell', pts, bevel=0.05)
    for k in range(6):                                     # os periscópios em volta (a janela escura com o vidro)
        a = 60 * k
        if k == 3:
            continue
        ar = math.radians(a)
        q = Vector((cx + 0.585 * math.cos(ar), cy + 0.53 * math.sin(ar), cz1 - 0.2))
        vp.rbox(p, 'Paint#plate', q, (0.06, 0.32, 0.14), yaw=a, r=0.03, k=2)
        vp.rbox(p, 'Lens', q + Vector((math.cos(ar), math.sin(ar), 0)) * 0.032, (0.012, 0.26, 0.08), yaw=a, r=0.02, k=2)
    vp.hatch_round(p, (cx + 0.05, cy - 0.05, cz1), 0.28, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
    # a metralhadora remota pequena (a PKT) no teto e a sirene (a cúpula âmbar, pisca; a luz que gira é a peça _Beacon)
    rk.kord_rws(p, (cx + 0.2, cy - 0.3, cz1), yaw=0.0, k=0.75)
    bx, by, bz = BEACON
    vp.lathe(p, 'Metal', (bx, by, bz - 0.1), 'z', [(0.0, 0.0), (0.12, 0.0), (0.12, 0.04), (0.1, 0.06), (0.1, 0.1), (0.0, 0.1)], seg=16)
    p.cyl('Beacon', (bx, by, bz + 0.07), 'z', 0.1, 0.2, seg=18, bevel=0.0, smooth=True)
    vp.lathe(p, 'Metal', (bx, by, bz + 0.17), 'z', [(0.0, 0.0), (0.11, 0.0), (0.11, 0.02), (0.06, 0.05), (0.0, 0.06)], seg=16)
    # a gaiola pequena por cima da cabine (a assinatura de todo blindado) com a rede da cor do jogador jogada por cima
    gx0, gx1, ghw = cx - 0.72, cx + 0.78, 0.7
    posts = [(cx + 0.55, cy + 0.62, cz0), (cx + 0.55, cy - 0.62, cz0), (cx - 0.55, cy - 0.62, cz0), (cx - 0.3, cy + 0.62, cz0)]
    roof = rk.slat_cage(p, gx0, gx1, ghw, 3.06, 3.03, posts, visor=0.3, step=0.22, yc=cy)

    def zf(x, y):
        z = roof(min(x, gx1)) + 0.035 + 0.07 * math.exp(-((x - cx + 0.2) ** 2 + (y - cy - 0.1) ** 2) / 0.12)
        if y < cy - ghw:
            z -= min(0.28, (cy - ghw - y) * 1.4)
        return z
    rk.net_drape(p, gx0 + 0.1, cx + 0.45, cy - ghw - 0.2, cy + ghw - 0.1, zf, step=0.22, r=0.017, jitter=0.06, seed=9.0)
    # ---------------------------------------------------------------- o braço telescópico com a garra (dobrado pra trás em marcha)
    px, py, pz = BOOM_PIVOT
    ex, ey, ez = BOOM_END
    # o pé do braço: o bloco do pivô na mesa e as 2 orelhas
    vp.rbox(p, 'Paint#plate', (px + 0.05, py, 2.02), (0.6, 0.62, 0.24), r=0.05, k=3)
    for d in (-0.26, 0.26):
        vp.rbox(p, 'Paint#plate', (px, py + d, pz - 0.02), (0.36, 0.07, 0.42), r=0.05, k=2)
    p.cyl('Metal#vp', (px, py, pz), 'y', 0.09, 0.66, seg=14, bevel=0.0)
    # as 2 seções (a grossa e a de dentro, que sai dela) e o cilindro de levantar por baixo
    a, b = Vector((px, py, pz)), Vector((ex, ey, ez))
    mid = a + (b - a) * 0.58
    beam(p, 'Paint#plate', a, mid + (b - a).normalized() * 0.1, 0.36, 0.34)
    beam(p, 'Paint', mid, b, 0.27, 0.25)
    for t in (0.15, 0.32, 0.5):
        q = a + (b - a) * t
        vp.bolt_row(p, 'Metal', q + Vector((0, -0.185, 0.08)), q + Vector((0, -0.185, -0.08)), 2, (0, -1, 0), r=0.016, h=0.012)
    cylinder(p, (px + 0.4, py, 1.98), a + (b - a) * 0.35 + Vector((0, 0, -0.17)), 0.08, 0.05)
    # o descanso do braço no deck de trás (o cavalete em A)
    for s in (-1, 1):
        vp.tube(p, 'Metal', [(-2.55, py + 0.3 * s, T72.DECK), (-2.55, py + 0.12 * s, 1.86)], 0.035, seg=8)
    vp.rbox(p, 'Rubber', (-2.55, py, 1.88), (0.2, 0.36, 0.06), r=0.02, k=1)
    # a garra: o punho que gira, as 2 mandíbulas abertas com os dentes, o cilindro dela (pendurada, apoiada pra trás)
    g = b
    vp.lathe(p, 'Paint', g, (-1, 0, 0), [(0.0, 0.0), (0.16, 0.0), (0.16, 0.18), (0.12, 0.22), (0.0, 0.22)], seg=16)
    head = g + Vector((-0.3, 0.0, -0.08))
    vp.rbox(p, 'Paint#plate', head, (0.34, 0.44, 0.34), r=0.06, k=3)
    for s in (-1, 1):
        # cada mandíbula: 2 chapas curvas (o arco em parênteses, aberto) com os 3 dentes virados pra dentro e o pino no cabeçote
        pts = [head + Vector((0.1 * s, 0.0, -0.1)), head + Vector((0.26 * s, 0.0, -0.28)), head + Vector((0.3 * s, 0.0, -0.46)),
               head + Vector((0.2 * s, 0.0, -0.6))]
        for d in (-0.15, 0.15):
            vp.tube(p, 'Paint', vp.bent_bar([q + Vector((0, d, 0)) for q in pts], R=0.12, n=3), 0.045, seg=8)
        for k in (2, 3):
            q = pts[k]
            frame_box(p, 'Metal', q + Vector((-0.06 * s, 0.0, 0.0)), Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1)), 0.12, 0.36, 0.05)
        p.cyl('Metal#vp', tuple(head + Vector((0.1 * s, 0.0, -0.1))), 'y', 0.05, 0.44, seg=10, bevel=0.0)
    # as mangueiras hidráulicas ao longo do braço
    vp.tube(p, 'Dark', [a + Vector((0.0, 0.2, 0.2)), a + (b - a) * 0.5 + Vector((0, 0.17, 0.18)), b + Vector((0.1, 0.12, 0.14))], 0.018, seg=6)
    # ---------------------------------------------------------------- a lona dobrada na mesa (a cor do jogador) e as caixas
    vp.tarp_bundle(p, (-0.45, -0.45, 1.9), (0.62, 0.8, 0.18), m='Team', strap_m='Cloth', buckle_m='Metal', straps=2, layers=2, seed=5.0)
    vp.ammo_box(p, (-0.2, 0.95, 1.9 + 0.13), (0.55, 0.3, 0.26), yaw=90, m='Paint', latch_m='Metal')
    return p.finish()


# ---------------------------------------------------------------------------------- lâmina (gira em volta de BLADE_PIVOT)
def blade():
    p = rk.new_part(PREFIX + '_Blade')
    p.bm('Paint#blade', 0.02)
    # o perfil da asa (u pra frente pela normal da asa, z pra cima): a face côncava, o lábio de cima virado pra frente, as costas
    face = [(0.24, 0.0), (0.15, 0.1), (0.07, 0.24), (0.02, 0.4), (0.0, 0.56), (0.02, 0.72), (0.07, 0.85), (0.15, 0.95)]
    back = [(-0.1, BLADE_H), (-0.16, 0.7), (-0.16, 0.25), (-0.06, 0.0)]
    prof = face + back
    tip = Vector((BLADE_TIP_X, 0.0))
    for s in (-1, 1):
        end = Vector((BLADE_TIP_X - 0.55, BLADE_HW * s))
        d = (end - tip).normalized()
        n2 = Vector((-d.y, d.x)) if s > 0 else Vector((d.y, -d.x))
        if n2.x < 0:
            n2 = -n2
        rings = []
        N = 8
        for i in range(N + 1):
            t = i / N
            c = tip + (end - tip) * t
            rings.append([(c.x + n2.x * u, c.y + n2.y * u, BLADE_Z0 + z) for (u, z) in prof])
        p.loft('Paint#blade', rings, bevel=0.02)
        # o fio de corte (a régua de aço na borda de baixo) com os parafusos
        a3 = Vector((tip.x + n2.x * 0.24, tip.y + n2.y * 0.24, BLADE_Z0 + 0.02))
        b3 = Vector((end.x + n2.x * 0.24, end.y + n2.y * 0.24, BLADE_Z0 + 0.02))
        rk.flat_bar(p, 'Metal', a3 + Vector((0, 0, 0.04)), b3 + Vector((0, 0, 0.04)), 0.1, 0.03, up=(n2.x, n2.y, 0.6))
        vp.bolt_row(p, 'Dark', a3 + Vector((n2.x * 0.02, n2.y * 0.02, 0.08)), b3 + Vector((n2.x * 0.02, n2.y * 0.02, 0.08)), 9,
                    (n2.x, n2.y, 0.5), r=0.02, h=0.012)
        # a chapa da ponta de fora (a orelha) e as nervuras das costas
        vp.rbox(p, 'Paint#plate', (end.x - 0.02, end.y + 0.02 * s, BLADE_Z0 + BLADE_H / 2), (0.42, 0.05, BLADE_H + 0.04),
                yaw=math.degrees(math.atan2(d.y, d.x)), r=0.06, k=2)
        for t in (0.3, 0.65):
            c = tip + (end - tip) * t
            q = Vector((c.x - n2.x * 0.2, c.y - n2.y * 0.2, BLADE_Z0 + BLADE_H / 2))
            vp.rbox(p, 'Paint#plate', q, (0.12, 0.08, BLADE_H - 0.1), yaw=math.degrees(math.atan2(n2.y, n2.x)), r=0.03, k=2)
        # o braço de empurrar: das costas da asa até o pivô no nariz (a viga e o olhal)
        c = tip + (end - tip) * 0.42
        q = Vector((c.x - n2.x * 0.16, c.y - n2.y * 0.16, BLADE_Z0 + 0.42))
        beam(p, 'Paint#plate', q, Vector((BLADE_PIVOT[0] + 0.05, 0.98 * s, BLADE_PIVOT[2])), 0.16, 0.22)
        p.cyl('Metal#vp', (BLADE_PIVOT[0], 0.98 * s, BLADE_PIVOT[2]), 'y', 0.07, 0.14, seg=12, bevel=0.0)
        # a haste do cilindro (o corpo fica no casco) e o olhal nas costas
        vp.tube(p, 'Metal', [Vector((3.0, 0.72 * s, rk.glacis_z(3.0) + 0.12)) + (Vector((3.62, 0.72 * s, 0.98)) - Vector((3.0, 0.72 * s, rk.glacis_z(3.0) + 0.12))) * 0.55,
                             Vector((3.62, 0.72 * s, 0.98))], 0.045, seg=10)
        vp.rbox(p, 'Paint#plate', (3.66, 0.72 * s, 0.95), (0.14, 0.16, 0.14), r=0.03, k=2)
    # a ponta do V: a cunha em pé que fecha o encontro das 2 asas (as seções delas abrem um vão na ponta) e a quilha de aço na frente
    ns = []
    for s in (-1, 1):
        end = Vector((BLADE_TIP_X - 0.55, BLADE_HW * s))
        d = (end - tip).normalized()
        n2 = Vector((-d.y, d.x)) if s > 0 else Vector((d.y, -d.x))
        ns.append(n2 if n2.x > 0 else -n2)
    nl, nr = ns
    plan = [tip + nl * 0.2, tip + Vector((0.3, 0.0)), tip + nr * 0.2, tip + nl * -0.16, tip + nr * -0.16]   # (o polígono simples)
    vp.prism(p, 'Paint#plate', [(q.x, q.y) for q in plan], Vector((0.0, 0.0, BLADE_Z0 + 0.02)), Vector((1, 0, 0)), Vector((0, 1, 0)),
             Vector((0, 0, 1)), BLADE_H - 0.04)
    vp.rbox(p, 'Metal', (tip.x + 0.3, 0.0, BLADE_Z0 + BLADE_H / 2), (0.08, 0.08, BLADE_H), r=0.03, k=2)
    return p.finish()


def beacon():
    """A luz que gira dentro da cúpula da sirene (o refletor e a lâmpada), feita no lugar (BEACON)."""
    p = rk.new_part(PREFIX + '_Beacon')
    bx, by, bz = BEACON
    p.cyl('Metal', (bx, by, bz), 'z', 0.03, 0.04, seg=10, bevel=0.0)
    p.obox('Metal', (bx - 0.04, by, bz + 0.08), (0.02, 0.1, 0.12), bevel=0.0)
    p.obox('Light', (bx - 0.015, by, bz + 0.08), (0.03, 0.06, 0.08), bevel=0.0)
    return p.finish()


def wheels():
    p = rk.new_part(PREFIX + '_Wheels')
    rk.t72_wheels(p)
    return p.finish()


def main():
    global BLADE_PIVOT, BEACON
    clear_scene()
    h, bl, be, w = hull(), blade(), beacon(), wheels()
    tl, tr = rk.t72_track(PREFIX + '_Track_l', 'l'), rk.t72_track(PREFIX + '_Track_r', 'r')
    # (01/11) a escala do jogo: o tanque domina (memória feedback-tanque-domina-a-escala; o apoio ~60–70% do tanque). No tamanho real
    # o IMR-3M dava 8,67 m = 0,89 do T-90M (o Dozer da China: 6,8 m = 0,68 do Type 99); × 0,78 = 6,8 m. Escala aqui, antes da grade do
    # detalhe (a densidade fica em metros) e com o passo do elo junto (a esteira que anda lê o track_step e os quadros)
    for o in (h, bl, be, w, tl, tr):
        o.data.transform(Matrix.Scale(GAME_SCALE, 4))
    for t in (tl, tr):
        if t.get('track_step'):
            t['track_step'] = t['track_step'] * GAME_SCALE
            t['track_frames'] = [c * GAME_SCALE if i % 5 < 3 else c for i, c in enumerate(t['track_frames'])]
    BLADE_PIVOT = tuple(round(c * GAME_SCALE, 3) for c in BLADE_PIVOT)
    BEACON = tuple(round(c * GAME_SCALE, 3) for c in BEACON)
    # a lâmina e a luz ficam no lugar pro cozimento das máscaras e vão pra origem delas só na exportação
    rk.keep_in_place_until_export([(bl, BLADE_PIVOT), (be, BEACON)])
    total = 0
    for o in (h, bl, be, w, tl, tr):
        total += export(o, OUT)
    print('MODEL total', total, 'tris')
    import model_lib
    placed = model_lib.DETAIL.get('defer') is not None
    if not placed:
        bl.data.transform(Matrix.Translation(V(*BLADE_PIVOT)))
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h, bl, w, tl, tr])
    (bx0, bx1), (by0, by1), (bz0, bz1) = bounds([bl])
    (hx0, hx1), (hy0, hy1), _ = bounds([h, w, tl, tr])
    print('MODEL medidas: comprimento total %.2f m (x %.2f..%.2f, da lamina a garra), largura %.2f m (a lamina), altura %.2f m'
          % (ax1 - ax0, ax0, ax1, ay1 - ay0, az1))
    print('MODEL corpo (sem a lamina): %.2f x %.2f m; a lamina %.2f x %.2f m (x %.2f..%.2f)' % (hx1 - hx0, hy1 - hy0, by1 - by0, bz1 - bz0, bx0, bx1))
    print('MODEL blade.at_m', list(BLADE_PIVOT), '(o pivo dos bracos; gira em Y)', 'beacon spin at_m', list(BEACON), 'rpm 90')
    print('MODEL tracks_m', [round(2 * T72.TRACK_Y * GAME_SCALE, 3), round(T72.TRACK_W * GAME_SCALE, 3)])
    print('MODEL tris por peca', {o.name: tris(o) for o in (h, bl, be, w, tl, tr)})
    rk.write_spec(OUT, [(PREFIX + '_Hull', (0, 0, 0)), (PREFIX + '_Blade', (BLADE_PIVOT[0], -BLADE_PIVOT[1], BLADE_PIVOT[2])),
                        (PREFIX + '_Beacon', (BEACON[0], -BEACON[1], BEACON[2])), (PREFIX + '_Wheels', (0, 0, 0)),
                        (PREFIX + '_Track_l', (0, 0, 0)), (PREFIX + '_Track_r', (0, 0, 0))], usa_at=(0, 6.5, 0))


main()
