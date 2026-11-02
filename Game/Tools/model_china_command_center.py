"""O Command Center da China provisório, modelado no Blender por script (o elenco das 3 facções, 31/10/2026: docs/ELENCO.md §4).

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_china_command_center.py -- captures/game/models/china_command_center

A palavra: O SALÃO DE PAGODE DE 3 TELHADOS sobre o pódio-fortaleza. A referência é a Porta da Paz Celestial (Tiananmen) e o
Salão da Harmonia Suprema: o pódio pesado de paredes inclinadas com o portão no meio e o salão de colunas em cima, aqui em
concreto escuro puxado pro oliva (a bíblia da China, docs/CONCEITO_CONSTRUCOES.md §2) e estilizado como os prédios do ZH:
poucas formas grandes e uma assinatura forte. A linguagem vem da War Factory aprovada (docs/concept/buildings/
china_war_factory*.webp): blocos de concreto com as juntas das placas, os telhados verdes de telha com a borda vermelha e as
pontas levantadas, os estandartes vermelhos com a estrela e o fio dourado, as luzes amarelas de serviço.

- o pódio (a "fortaleza"): paredes inclinadas, rodapé claro, as juntas das placas, a cornija e as ameias no alto; em cima, o
  terraço de pedra clara (de cima, a moldura do salão escuro);
- o portão (a garagem do Dozer) no meio da frente (+X): 5,2 × 4,5 m, a porta de aço recuada 1,2 m, o emblema (a estrela no aro
  dourado) no lintel; na frente, o pórtico vermelho tipo paifang (2 colunas, as vigas, a placa dourada e o telhadinho);
- as duas escadarias na face da frente, subindo dos cantos até o balcão de cima do portão (a tribuna), com o corrimão
  vermelho; os dois leões de pedra flanqueando o paifang;
- o salão em cima: a plataforma clara, a colunata vermelha na frente, a porta vermelha com os cravos dourados, as janelas de
  treliça, as lanternas vermelhas penduradas no beiral, e os 3 telhados (dois em saia e o de cima com a cumeeira e os
  ornamentos dourados);
- o terraço da esquerda (−Y): o RADAR DE TRELIÇA girando sobre o tambor (a assinatura que se mexe); o da direita (+Y): o
  mastro de rádio treliçado com as cornetas e o estandarte, a baliza no topo;
- no pátio da frente: a grande estrela vermelha pintada no piso, por onde o Dozer sai.
Os estandartes: 2 na borda da frente do terraço (emolduram o salão sem tapar a fachada) e 1 no mastro (a bíblia: 3 ou 4 no
máximo).

O lote: 24 × 28 m (x −12..12, y −14..14), a frente em +X. O maior prédio da China: o corpo (pódio + salão) até ~9 m, o pagode
até ~13,5 m (cumeeira), o mastro até ~16,4 m.

Peças (origem no chão, no centro do lote; coordenadas da Unreal, X frente, Y direita, Z cima, metros):
  SM_China_CommandCenter_Base: tudo o que não se mexe;
  SM_China_CommandCenter_Radar (spin em Z, origem no eixo, no pé da mesa giratória: at_m = RADAR_AT, ~6 rpm): a mesa, a cabine,
    a treliça do refletor curvo, o alimentador e a baliza;
  SM_China_CommandCenter_Scaffold: o andaime da obra (tubos de aço, scaffold() do model_lib).
Materiais (slots do model_faction_kit.py): CnConcrete, CnPlinth, CnRoof, CnMetal, CnGold, Player (o vermelho: colunas,
pórtico, corrimão, estrela, estandartes, lanternas, borda dos telhados), Dark, Glass, Light, Beacon, Hazard.
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, scaffold  # noqa: E402,F401
from model_faction_kit import (circle_on_face, disc_ring, foundation, lamp, polygon_prism, revolve,  # noqa: E402
                               speaker_cluster, star_points, star_wall)
from model_building_parts import BPart, WallFace, wbox, cn_block, bake_in_place, cable  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'china_command_center')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_China_CommandCenter'

HX, HY = 12.0, 14.0                 # o lote 24 × 28
Z0 = 0.14                           # o topo da laje do lote
PZ = 5.6                            # o topo do pódio (o piso do terraço)
POD = (-11.4, 2.4, -12.4, 12.4)     # o pé do pódio
BAT = 0.8                           # o recuo da parede inclinada (do pé ao topo)
GATE = (1.8, 4.2, -4.0, 4.0)        # o bloco do portão (faces retas, salta da parede inclinada)
DOOR_W, DOOR_H = 5.2, 4.5           # o vão do portão (o Dozer sai por aqui)
DOOR_X = 3.0                        # o plano da porta de aço (recuada 1,2 m da frente do portão)
STAIR_X = (1.5, 4.4)                # as escadarias (o fundo entra na parede inclinada)
LAND_Y = (4.0, 5.2)                 # o patamar de cima, encostado no portão
STAIR_BOT = 12.2                    # o pé da escada (|y|)
PAI_X, PAI_Y = 6.4, 3.75            # o paifang: o eixo e as colunas (fora da faixa de saída de 6,6 m)
STAR_C, STAR_R = (9.2, 0.0), 2.5    # a estrela do pátio
# o salão
HALL = (-9.0, -1.4, -5.2, 5.2)      # as paredes do salão
STYLO = (-9.6, 0.6, -6.2, 6.2)      # a plataforma clara
COL_X = -0.15                       # a colunata da frente
COL_YS = (-5.0, -3.0, -1.0, 1.0, 3.0, 5.0)
BAYS = (-4.0, -2.0, 2.0, 4.0)        # os vãos entre as colunas (janelas e lanternas)
ZH0 = PZ + 0.45                     # o piso do salão
ZE1 = ZH0 + 2.75                    # o beiral do telhado 1 (o topo das colunas)
RADAR_AT = (-4.8, -9.95, PZ + 0.8)  # o eixo do radar, no topo do tambor
MAST_AT = (-4.8, 9.95)              # o mastro de rádio


# ===================================================================================================== utilidades
def sgn(v):
    return (v > 1e-9) - (v < -1e-9)


def lerp(a, b, t):
    return a + (b - a) * t


def prism_yz(p, m, pts, x0, x1, bevel=0.02):
    """Prisma de um polígono no plano YZ [(y, z)...] extrudado em X de x0 a x1."""
    p.loft(m, [[(x0, y, z) for (y, z) in pts], [(x1, y, z) for (y, z) in pts]], bevel=bevel)


def tube(p, m, a, b, r, seg=8):
    """Tubo reto de a até b (pontos (x, y, z))."""
    d = Vector(b) - Vector(a)
    if d.length < 1e-4:
        return
    c = (Vector(a) + Vector(b)) / 2
    p.cyl(m.split('#')[0] + '#t', (c.x, c.y, c.z), (d.x, d.y, d.z), r, d.length, seg=seg, bevel=0.0)


def report(o):
    """Os triângulos por material (pra saber onde o orçamento vai)."""
    cnt = {}
    for poly in o.data.polygons:
        n = o.data.materials[poly.material_index].name
        cnt[n] = cnt.get(n, 0) + len(poly.vertices) - 2
    print('TRIS', o.name, sorted(cnt.items(), key=lambda kv: -kv[1]))


# ------------------------------------------------------------------ o bloco de paredes inclinadas e as placas coladas nele
def batter(p, m, x0, x1, y0, y1, z0, z1, ins, bevel=0.05):
    """Bloco com as 4 paredes inclinadas: o pé (x0..x1, y0..y1) em z0, o topo recuado 'ins' em z1."""
    p.loft(m, [[(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)],
               [(x0 + ins, y0 + ins, z1), (x1 - ins, y0 + ins, z1), (x1 - ins, y1 - ins, z1), (x0 + ins, y1 - ins, z1)]], bevel=bevel)


def face_pt(blk, side, a, z, out=0.0):
    """Ponto na face inclinada 'side' do bloco blk = (x0, x1, y0, y1, z0, z1, ins): a = a coordenada ao longo da face."""
    x0, x1, y0, y1, z0, z1, ins = blk
    t = (z - z0) / (z1 - z0)
    h = z1 - z0
    n = math.hypot(h, ins)
    if side == '+x':
        return (x1 - ins * t + out * h / n, a, z + out * ins / n)
    if side == '-x':
        return (x0 + ins * t - out * h / n, a, z + out * ins / n)
    if side == '+y':
        return (a, y1 - ins * t + out * h / n, z + out * ins / n)
    return (a, y0 + ins * t - out * h / n, z + out * ins / n)


def face_range(blk, side, z):
    """O intervalo de 'a' (ao longo da face) na altura z."""
    x0, x1, y0, y1, z0, z1, ins = blk
    t = (z - z0) / (z1 - z0)
    if side in ('+x', '-x'):
        return y0 + ins * t, y1 - ins * t
    return x0 + ins * t, x1 - ins * t


def face_plate(p, m, blk, side, a0, a1, za, zb, t=0.02, bevel=0.0):
    """Placa fina colada na face inclinada (a junta, o rodapé, a luminária, o cartaz)."""
    q0 = [face_pt(blk, side, a0, za), face_pt(blk, side, a1, za), face_pt(blk, side, a1, zb), face_pt(blk, side, a0, zb)]
    q1 = [face_pt(blk, side, a0, za, t), face_pt(blk, side, a1, za, t), face_pt(blk, side, a1, zb, t), face_pt(blk, side, a0, zb, t)]
    p.loft(m, [q0, q1], bevel=bevel)


def seams(p, blk, sides, zs, step, skip=()):
    """As juntas das placas de concreto (escuras, finas) nas faces inclinadas: horizontais nas alturas zs e verticais a cada
    'step'. skip = [(side, a0, a1)] trechos sem junta vertical (atrás do portão, das escadas)."""
    for side in sides:
        for z in zs:
            a0, a1 = face_range(blk, side, z)
            face_plate(p, 'Dark', blk, side, a0 + 0.1, a1 - 0.1, z - 0.03, z + 0.03, t=0.012)
        a0, a1 = face_range(blk, side, blk[5])
        n = max(2, round((a1 - a0) / step))
        for k in range(1, n):
            a = a0 + (a1 - a0) * k / n
            if any(s == side and lo <= a <= hi for (s, lo, hi) in skip):
                continue
            face_plate(p, 'Dark', blk, side, a - 0.03, a + 0.03, blk[4] + 0.7, blk[5] - 0.45, t=0.012)


# (31/10, o detalhe AAA: docs/ESTUDO_DETALHE_AAA.md) a telha-canal redonda, a mesma da War Factory
from model_building_parts import round_prof as _round_prof, sweep as _sweep  # noqa: E402
_TILE = _round_prof(0.17, 0.085, base=-0.03, n=7)


def _tile(p, m, pts):
    """Uma fileira de telha-canal (o rolo redondo) com a telha de remate redonda na ponta do beiral."""
    _sweep(p, m, pts, _TILE, smooth=True)
    P0, P1 = Vector(pts[0]), Vector(pts[1])
    d = (P1 - P0).normalized()
    h = (Vector((0.0, 0.0, 1.0)) - d * d.z).normalized()
    p.cyl(m, tuple(P0 + h * 0.03 - d * 0.015), tuple(d), 0.1, 0.05, seg=12, bevel=0.0)


# ------------------------------------------------------------------ o telhado chinês
U_ROOF = (0.0, 0.10, 0.22, 0.36, 0.52, 0.70, 0.86, 1.0)
U_RIB = (0.0, 0.22, 0.52, 0.86)
PW = 2.6          # o quanto a subida das quinas se concentra perto delas


def ring_uv(nx, ny):
    """O contorno do retângulo unitário (as quinas em ±1), nx divisões nos lados paralelos a X e ny nos paralelos a Y."""
    out = []
    for k in range(nx):
        out.append((-1 + 2 * k / nx, -1.0))
    for k in range(ny):
        out.append((1.0, -1 + 2 * k / ny))
    for k in range(nx):
        out.append((1 - 2 * k / nx, 1.0))
    for k in range(ny):
        out.append((-1.0, 1 - 2 * k / ny))
    return out


def roof_point(R, u, ux, uy):
    """Ponto da superfície do telhado: u = 0 no beiral, 1 no topo; (ux, uy) na borda do retângulo unitário. O perfil é
    côncavo (sobe devagar no beiral e forte perto do topo: o 举折 dos telhados chineses) e as quinas sobem ('curl') e saem pra
    fora ('kick') no beiral, sumindo pro topo."""
    sx = lerp(R['SXe'], R['SXt'], u)
    sy = lerp(R['SYe'], R['SYt'], u)
    w = min(abs(ux), abs(uy)) ** PW
    decay = (1 - u) ** 2
    k = R['kick'] * w * decay
    return (R['cx'] + ux * sx + sgn(ux) * k, R['cy'] + uy * sy + sgn(uy) * k,
            R['z0'] + R['rise'] * u ** 1.5 + R['curl'] * w * decay)


def cn_roof(p, cx, cy, wx, wy, z0, rise, over, top, curl=0.55, kick=0.35, ridge_h=0.0, thick=0.35, pitch=0.34,
            m='CnRoof', edge='Player', orn='CnGold', ribs=True, corner_tips=True):
    """O telhado de um andar: o quadril côncavo de telha verde com o beiral a 'over' além da parede (meia-largura wx, wy, em
    z0), a borda vermelha (fáscia) seguindo o beiral curvo, as pontas das quinas levantadas, as 4 cumeeiras dos cantos e as
    fileiras de telha (nervuras do beiral ao topo). top = (meia-largura x, meia-largura y) do anel de cima: nos telhados em
    saia é a parede do andar de cima (o telhado entra nela); no último é a cumeeira (ridge_h > 0, com os ornamentos)."""
    R = {'cx': cx, 'cy': cy, 'SXe': wx + over, 'SYe': wy + over, 'SXt': top[0], 'SYt': top[1], 'z0': z0, 'rise': rise,
         'curl': curl, 'kick': kick}
    nx = max(6, 2 * round(R['SXe'] / 0.75))
    ny = max(6, 2 * round(R['SYe'] / 0.75))
    uv = ring_uv(nx, ny)
    rings = [[(cx + ux * (R['SXe'] - 0.55), cy + uy * (R['SYe'] - 0.55), z0 - thick) for (ux, uy) in uv]]
    rings.append([(q[0], q[1], q[2] - 0.12) for q in (roof_point(R, 0.0, ux, uy) for (ux, uy) in uv)])
    for u in U_ROOF:
        rings.append([roof_point(R, u, ux, uy) for (ux, uy) in uv])
    p.loft(m, rings, bevel=0.03)
    # a fáscia vermelha: uma tábua fechada seguindo o beiral (cobre a borda e um pouco do forro)
    b = p.bm(edge + '#fascia', 0.0)
    secs = []
    for (ux, uy) in uv:
        P = roof_point(R, 0.0, ux, uy)
        nx_ = sgn(ux) if abs(ux) > 0.999 else 0
        ny_ = sgn(uy) if abs(uy) > 0.999 else 0
        sec = [(P[0] + nx_ * 0.07, P[1] + ny_ * 0.07, P[2] + 0.05), (P[0] + nx_ * 0.07, P[1] + ny_ * 0.07, P[2] - 0.34),
               (P[0] - nx_ * 0.3, P[1] - ny_ * 0.3, P[2] - 0.34), (P[0] - nx_ * 0.3, P[1] - ny_ * 0.3, P[2] + 0.05)]
        secs.append([b.verts.new(V(*q)) for q in sec])
    for i in range(len(secs)):
        A, B = secs[i], secs[(i + 1) % len(secs)]
        for j in range(4):
            b.faces.new([A[j], A[(j + 1) % 4], B[(j + 1) % 4], B[j]])
    # as cumeeiras dos 4 cantos (do beiral até o anel de cima), com a ponta levantada vermelha na quina
    for (ux, uy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        sx_, sy_ = -uy / math.sqrt(2), ux / math.sqrt(2)        # o lado (perpendicular à diagonal, no plano)
        pts = [roof_point(R, u, ux, uy) for u in U_ROOF]
        _sweep(p, m + '#hip', pts, _round_prof(0.3, 0.24, base=-0.05, n=7), smooth=True)        # o rolo redondo da cumeeira
        if corner_tips:
            q = pts[0]
            yaw = math.degrees(math.atan2(uy, ux))
            dx, dy = ux / math.sqrt(2), uy / math.sqrt(2)
            p.obox(edge, (q[0] + dx * 0.32, q[1] + dy * 0.32, q[2] + 0.22), (0.75, 0.2, 0.16), yaw=yaw, pitch=38, bevel=0.03)
            p.obox(edge, (q[0] + dx * 0.62, q[1] + dy * 0.62, q[2] + 0.5), (0.3, 0.16, 0.16), yaw=yaw, pitch=68, bevel=0.03)
    # as fileiras de telha: nervuras retas do beiral até a cumeeira do canto (ou até o anel de cima)
    if ribs:
        rb = p.bm(m + '#ribs', 0.0)
        for (axis, sgn_side) in (('x', -1), ('x', 1), ('y', -1), ('y', 1)):
            se, st = (R['SXe'], R['SXt']) if axis == 'x' else (R['SYe'], R['SYt'])
            n = int(2 * se / pitch)
            for i in range(n):
                a = -se + (i + 0.5) * 2 * se / n
                u_end = 1.0 if abs(a) <= st else (se - abs(a) - 0.18) / (se - st)
                if u_end < 0.12:
                    continue
                us = [u for u in U_RIB if u < u_end - 0.05] + [min(u_end, 1.0)]
                ring_pts = []
                for u in us:
                    s = lerp(se, st, u)
                    t = max(-1.0, min(1.0, a / s))
                    q = roof_point(R, u, t, sgn_side) if axis == 'x' else roof_point(R, u, sgn_side, t)
                    ring_pts.append(q)
                # (31/10, o detalhe AAA) a telha-canal redonda (a onda que pega o sol em listras) e a telha de remate no beiral
                if len(ring_pts) >= 2:
                    _tile(p, m + '#ribs', ring_pts)
    ztop = z0 + rise
    # a cumeeira de cima com os ornamentos dourados (o chiwen estilizado: o bloco que sobe e a ponta que dobra pra dentro)
    if ridge_h > 0:
        along_y = top[1] >= top[0]
        L = top[1] if along_y else top[0]
        if along_y:
            p.box(m, cx - 0.2, cx + 0.2, cy - L - 0.12, cy + L + 0.12, ztop - 0.12, ztop + ridge_h, bevel=0.05)
            p.box(edge, cx - 0.21, cx + 0.21, cy - L - 0.12, cy + L + 0.12, ztop + ridge_h - 0.1, ztop + ridge_h - 0.02, bevel=0.0)
        else:
            p.box(m, cx - L - 0.12, cx + L + 0.12, cy - 0.2, cy + 0.2, ztop - 0.12, ztop + ridge_h, bevel=0.05)
            p.box(edge, cx - L - 0.12, cx + L + 0.12, cy - 0.21, cy + 0.21, ztop + ridge_h - 0.1, ztop + ridge_h - 0.02, bevel=0.0)
        for s in (-1, 1):
            ex, ey = (cx, cy + s * (L + 0.05)) if along_y else (cx + s * (L + 0.05), cy)
            zb = ztop + ridge_h
            if along_y:
                p.obox(orn, (ex, ey, zb + 0.28), (0.3, 0.32, 0.62), roll=s * 12, bevel=0.04)
                p.obox(orn, (ex, ey - s * 0.18, zb + 0.62), (0.28, 0.42, 0.18), roll=-s * 30, bevel=0.03)
            else:
                p.obox(orn, (ex, ey, zb + 0.28), (0.32, 0.3, 0.62), pitch=-s * 12, bevel=0.04)
                p.obox(orn, (ex - s * 0.18, ey, zb + 0.62), (0.42, 0.28, 0.18), pitch=s * 30, bevel=0.03)
        # a pérola no meio da cumeeira
        p.cyl(orn, (cx, cy, ztop + ridge_h + 0.16), 'z', 0.15, 0.3, seg=12, r2=0.05, bevel=0.0)
    return ztop


# ------------------------------------------------------------------ peças de detalhe
def banner2(p, pole, z0, top, height=3.0, width=1.1, face='+x', m='Player', star='CnGold', pole_m='CnMetal'):
    """O estandarte da War Factory aprovada: o pano vermelho no mastro com a travessa, a estrela dourada em cima e o fio
    dourado vertical embaixo dela. O mastro começa em z0 (no balcão), não no chão."""
    x, y = pole
    p.cyl(pole_m, (x, y, (z0 + top) / 2), 'z', 0.07, top - z0, seg=10, bevel=0.0)
    p.cyl(star, (x, y, top + 0.14), 'z', 0.11, 0.28, seg=10, r2=0.02, bevel=0.0)
    s = 1 if face[0] == '+' else -1
    off = 0.1 * s
    zt = top - 0.18
    if face in ('+x', '-x'):
        p.cyl(pole_m, (x + off, y, zt), 'y', 0.04, width + 0.24, seg=6, bevel=0.0)
        p.box(m, x + off - 0.02, x + off + 0.02, y - width / 2, y + width / 2, zt - height, zt, bevel=0.0)
        star_wall(p, (x + off + 0.02 * s, y, zt - height * 0.2), face, width * 0.27, m=star, t=0.012)
        p.box(star, x + off + 0.02 * s - 0.006, x + off + 0.02 * s + 0.006, y - 0.035, y + 0.035, zt - height * 0.86, zt - height * 0.36, bevel=0.0)
        p.box(star, x + off - 0.025, x + off + 0.025, y - width / 2, y + width / 2, zt - height - 0.06, zt - height, bevel=0.0)
    else:
        p.cyl(pole_m, (x, y + off, zt), 'x', 0.04, width + 0.24, seg=6, bevel=0.0)
        p.box(m, x - width / 2, x + width / 2, y + off - 0.02, y + off + 0.02, zt - height, zt, bevel=0.0)
        star_wall(p, (x, y + off + 0.02 * s, zt - height * 0.2), face, width * 0.27, m=star, t=0.012)
        p.box(star, x - 0.035, x + 0.035, y + off + 0.02 * s - 0.006, y + off + 0.02 * s + 0.006, zt - height * 0.86, zt - height * 0.36, bevel=0.0)


def lantern(p, x, y, z_hang, drop=0.55, r=0.3, h=0.52):
    """A lanterna vermelha pendurada: o cordão, as tampas douradas, o corpo de gomos e a borla."""
    zc = z_hang - drop - h / 2
    p.cyl('Dark', (x, y, z_hang - drop / 2), 'z', 0.012, drop, seg=6, bevel=0.0)
    p.cyl('CnGold', (x, y, zc + h / 2 + 0.03), 'z', r * 0.45, 0.07, seg=12, bevel=0.0)
    prof = [(r * 0.48, zc - h / 2), (r * 0.82, zc - h / 2 + 0.07), (r, zc - 0.07), (r, zc + 0.07), (r * 0.82, zc + h / 2 - 0.07),
            (r * 0.48, zc + h / 2)]
    revolve(p, 'Player#lantern', (x, y), prof, seg=16, smooth=True)
    p.cyl('CnGold', (x, y, zc - h / 2 - 0.03), 'z', r * 0.45, 0.07, seg=12, bevel=0.0)
    p.cyl('CnGold', (x, y, zc - h / 2 - 0.2), 'z', 0.05, 0.28, seg=8, r2=0.09, bevel=0.0)


def lion(p, x, y, z0, flip=1, m='CnPlinth#lion'):
    """O leão de pedra estilizado, sentado, olhando pra +X, no pedestal (o par do portão: o da direita com a bola sob a
    pata, flip = +1; o da esquerda com o filhote, flip = −1). Blocos chanfrados grandes, como pedra lavrada."""
    p.box('CnPlinth', x - 0.7, x + 0.7, y - 0.55, y + 0.55, z0, z0 + 0.25, bevel=0.04)
    p.box('CnPlinth', x - 0.58, x + 0.58, y - 0.44, y + 0.44, z0 + 0.25, z0 + 0.95, bevel=0.04)
    p.box('CnPlinth', x - 0.66, x + 0.66, y - 0.5, y + 0.5, z0 + 0.95, z0 + 1.08, bevel=0.03)
    zb = z0 + 1.08
    p.obox(m, (x - 0.24, y, zb + 0.3), (0.62, 0.66, 0.6), bevel=0.12)                    # as ancas
    p.obox(m, (x + 0.06, y, zb + 0.62), (0.5, 0.58, 0.95), pitch=-12, bevel=0.12)        # o peito
    for s in (-1, 1):
        p.obox(m, (x + 0.3, y + s * 0.17, zb + 0.32), (0.2, 0.17, 0.62), bevel=0.05)    # as patas da frente
        p.obox(m, (x + 0.38, y + s * 0.17, zb + 0.06), (0.32, 0.22, 0.12), bevel=0.04)
        p.obox(m, (x - 0.28, y + s * 0.28, zb + 0.1), (0.5, 0.14, 0.2), bevel=0.05)     # as patas de trás
    p.obox(m, (x + 0.12, y, zb + 1.18), (0.55, 0.72, 0.62), bevel=0.16)                  # a juba
    for s in (-1, 1):
        p.obox(m, (x + 0.02, y + s * 0.22, zb + 1.5), (0.2, 0.18, 0.16), bevel=0.05)    # os cachos de cima
    p.obox(m, (x + 0.36, y, zb + 1.14), (0.34, 0.48, 0.42), bevel=0.1)                   # a cara
    p.obox(m, (x + 0.54, y, zb + 1.05), (0.16, 0.3, 0.2), bevel=0.05)                    # o focinho
    p.obox('Dark', (x + 0.5, y, zb + 1.22), (0.04, 0.32, 0.05), bevel=0.0)               # os olhos (a sombra)
    p.obox(m, (x - 0.55, y - flip * 0.18, zb + 0.55), (0.12, 0.14, 0.55), pitch=20, bevel=0.04)   # o rabo enrolado
    p.obox(m, (x - 0.5, y - flip * 0.18, zb + 0.86), (0.24, 0.2, 0.16), bevel=0.06)
    if flip > 0:
        p.cyl(m.split('#')[0] + '#ball', (x + 0.5, y + 0.3, zb + 0.15), 'z', 0.15, 0.3, seg=12, bevel=0.0, smooth=True)
    else:
        p.obox(m, (x + 0.48, y - 0.3, zb + 0.14), (0.26, 0.2, 0.28), bevel=0.07)


def railing_red(p, pts, h=0.95, step=1.1, m='Player', post_m='Player', r=0.05):
    """O corrimão vermelho: postes e o tubo de cima (e o do meio) ao longo da polilinha [(x, y, z)...] (o z é o piso)."""
    for (a, b) in zip(pts[:-1], pts[1:]):
        L = math.dist(a, b)
        n = max(1, round(L / step))
        for k in range(n + 1):
            t = k / n
            q = (lerp(a[0], b[0], t), lerp(a[1], b[1], t), lerp(a[2], b[2], t))
            p.cyl(post_m + '#t', (q[0], q[1], q[2] + h / 2), 'z', 0.04, h, seg=8, bevel=0.0)
        tube(p, m, (a[0], a[1], a[2] + h), (b[0], b[1], b[2] + h), r, seg=10)
        tube(p, m, (a[0], a[1], a[2] + h * 0.5), (b[0], b[1], b[2] + h * 0.5), r * 0.7, seg=8)


def lattice_mast(p, c, z0, z1, a0, a1, levels, m='CnMetal', leg=0.07, brace=0.035):
    """A torre treliçada quadrada (rádio): 4 pernas de a0 (meio-lado no pé) a a1 (no topo), anéis e X em cada face."""
    def corner(k, z):
        t = (z - z0) / (z1 - z0)
        a = lerp(a0, a1, t)
        sx, sy = ((-1, -1), (1, -1), (1, 1), (-1, 1))[k]
        return (c[0] + sx * a, c[1] + sy * a, z)
    for k in range(4):
        tube(p, m, corner(k, z0), corner(k, z1), leg, seg=8)
    for lv in range(levels + 1):
        z = lerp(z0, z1, lv / levels)
        for k in range(4):
            tube(p, m, corner(k, z), corner((k + 1) % 4, z), brace * 1.2, seg=6)
    for lv in range(levels):
        za, zb = lerp(z0, z1, lv / levels), lerp(z0, z1, (lv + 1) / levels)
        for k in range(4):
            tube(p, m, corner(k, za), corner((k + 1) % 4, zb), brace, seg=6)
            tube(p, m, corner((k + 1) % 4, za), corner(k, zb), brace, seg=6)


# ===================================================================================================== o prédio
def podium(p):
    x0, x1, y0, y1 = POD
    blk = (x0, x1, y0, y1, Z0, PZ, BAT)
    # (31/10, o detalhe AAA) o pódio-fortaleza OCO (model_building_parts.cn_block): as 4 paredes inclinadas de 0,45 m com a pele de
    # placas pré-moldadas (o chanfro largo, a junta funda), os pilares de quina inclinados, a cornija em degraus com a pingadeira,
    # a laje do terraço com espessura e por dentro os salões (as vigas, os pilares): o furo do dano mostra a fortaleza por dentro
    lean = BAT / (PZ - Z0)
    cn_block(p, x0, x1, y0, y1, Z0 - 0.05, PZ, t=0.45, lean={f: lean for f in ('+x', '-x', '+y', '-y')},
             skin=('+x', '-x', '+y', '-y'), roof_t=0.4, parapet=0.0, corners=True, pier=1.0, cornice=True, interior=True, plinth=0,
             cols=[(-7.2, -6.0), (-7.2, 0.0), (-7.2, 6.0), (-2.6, -6.0), (-2.6, 6.0)], pw=3.1, zs=[Z0 - 0.05, 0.8, 2.3, 3.95])
    # o rodapé claro (inclinado também, saindo da pele de placas)
    batter(p, 'CnPlinth#rod', x0 - 0.3, x1 + 0.3, y0 - 0.3, y1 + 0.3, Z0 - 0.05, 0.8, 0.2, bevel=0.06)
    # o parapeito com as ameias (a "fortaleza"), aberto no balcão do portão e nos patamares
    tx0, tx1, ty0, ty1 = x0 + BAT, x1 - BAT, y0 + BAT, y1 - BAT
    p.box('CnPlinth', tx0 + 0.3, tx1 - 0.1, ty0 + 0.3, ty1 - 0.3, PZ, PZ + 0.04, bevel=0.0)          # o piso do terraço
    pw, ph, mh = 0.42, 0.55, 0.62
    segs = [((tx0, ty0), (tx1, ty0)), ((tx0, ty1), (tx1, ty1)), ((tx0, ty0), (tx0, ty1)),
            ((tx1, ty0), (tx1, -LAND_Y[1])), ((tx1, LAND_Y[1]), (tx1, ty1))]
    for (ax, ay), (bx, by) in segs:
        if ay == by:                    # ao longo de X
            yy = ay + (pw / 2 if ay < 0 else -pw / 2)
            p.box('CnConcrete', ax, bx, yy - pw / 2, yy + pw / 2, PZ, PZ + ph, bevel=0.04)
            n = max(1, round((bx - ax) / 1.6))
            for k in range(n):
                xm = ax + (k + 0.5) * (bx - ax) / n
                p.box('CnConcrete#merlon', xm - 0.4, xm + 0.4, yy - pw / 2 - 0.02, yy + pw / 2 + 0.02, PZ + ph, PZ + ph + mh, bevel=0.015)
                p.box('CnPlinth#cap', xm - 0.44, xm + 0.44, yy - pw / 2 - 0.05, yy + pw / 2 + 0.05, PZ + ph + mh, PZ + ph + mh + 0.08, bevel=0.015)
        else:                           # ao longo de Y
            xx = ax + (pw / 2 if ax < 0 else -pw / 2)
            p.box('CnConcrete', xx - pw / 2, xx + pw / 2, ay, by, PZ, PZ + ph, bevel=0.04)
            n = max(1, round((by - ay) / 1.6))
            for k in range(n):
                ym = ay + (k + 0.5) * (by - ay) / n
                p.box('CnConcrete#merlon', xx - pw / 2 - 0.02, xx + pw / 2 + 0.02, ym - 0.4, ym + 0.4, PZ + ph, PZ + ph + mh, bevel=0.015)
                p.box('CnPlinth#cap', xx - pw / 2 - 0.05, xx + pw / 2 + 0.05, ym - 0.44, ym + 0.44, PZ + ph + mh, PZ + ph + mh + 0.08, bevel=0.015)
    # as luzes de serviço amarelas no pé do pódio (lados e fundo) e as portinhas de serviço do fundo
    for side, aa in (('-y', (-9.0, -5.0, -1.0)), ('+y', (-9.0, -5.0, -1.0)), ('-x', (-9.0, -4.5, 4.5, 9.0))):
        for a in aa:
            face_plate(p, 'Dark#lamp', blk, side, a - 0.18, a + 0.18, 2.75, 3.08, t=0.22, bevel=0.012)
            face_plate(p, 'Light', blk, side, a - 0.14, a + 0.14, 2.77, 2.9, t=0.27)
    for a in (-2.2, 2.2):
        face_plate(p, 'CnPlinth#door', blk, '-x', a - 0.85, a + 0.85, Z0 + 0.62, 2.9, t=0.22, bevel=0.04)
        face_plate(p, 'CnMetal', blk, '-x', a - 0.65, a + 0.65, Z0 + 0.62, 2.7, t=0.25)
    return blk


def gate(p):
    gx0, gx1, gy0, gy1 = GATE
    hw = DOOR_W / 2
    zt = Z0 + DOOR_H
    for s in (-1, 1):                                                                   # os batentes
        p.box('CnConcrete', gx0, gx1, min(s * hw, s * gy1), max(s * hw, s * gy1), Z0 - 0.05, PZ, bevel=0.06)
    p.box('CnConcrete', gx0, gx1, -hw, hw, zt, PZ, bevel=0.06)                          # o lintel
    p.box('CnPlinth', gx0 - 0.1, gx1 + 0.12, gy0 - 0.12, gy1 + 0.12, PZ - 0.32, PZ, bevel=0.04)       # a cornija
    p.box('CnPlinth', gx0, gx1 + 0.22, gy0 - 0.22, gy1 + 0.22, Z0 - 0.05, 0.8, bevel=0.04)           # o rodapé
    # o vão: o fundo, o teto e a porta de aço de enrolar (as lâminas), as faixas de perigo no pé dos batentes
    p.box('Dark', DOOR_X - 0.3, DOOR_X, -hw, hw, Z0, zt, bevel=0.0)
    p.box('CnMetal', DOOR_X, DOOR_X + 0.06, -hw + 0.05, hw - 0.05, Z0, zt - 0.05, bevel=0.0)
    for k in range(15):
        z = Z0 + 0.25 + k * 0.29
        p.box('Dark', DOOR_X + 0.06, DOOR_X + 0.09, -hw + 0.1, hw - 0.1, z, z + 0.05, bevel=0.0)
    p.box('CnMetal', DOOR_X, DOOR_X + 0.35, -hw, hw, zt - 0.35, zt, bevel=0.02)                     # a caixa do rolo
    for s in (-1, 1):
        p.box('Hazard', gx1, gx1 + 0.03, min(s * hw, s * (hw + 0.45)), max(s * hw, s * (hw + 0.45)), 0.8, 1.9, bevel=0.0)
        p.box('Hazard', DOOR_X + 0.4, gx1, min(s * hw, s * (hw - 0.03)), max(s * hw, s * (hw - 0.03)), 0.8, 1.9, bevel=0.0)
        lamp(p, gx1, s * (hw + 0.75), zt - 0.4, '+x')
        p.box('CnPlinth', gx1, gx1 + 0.1, min(s * hw, s * (hw + 0.3)), max(s * hw, s * (hw + 0.3)), Z0 + 0.8, zt + 0.3, bevel=0.0)  # a moldura
    p.box('CnPlinth', gx1, gx1 + 0.1, -hw - 0.3, hw + 0.3, zt, zt + 0.3, bevel=0.0)
    # a luz do teto do vão: a calha escura encostada no lintel e a lâmpada embaixo dela (a lâmpada nunca encosta coplanar no
    # lintel: o Cycles vazava a luz por ele e acendia o balcão de cima)
    p.box('Dark', DOOR_X + 0.35, gx1 - 0.15, -hw + 0.25, hw - 0.25, zt - 0.08, zt, bevel=0.0)
    p.box('Light', DOOR_X + 0.4, gx1 - 0.2, -hw + 0.3, hw - 0.3, zt - 0.11, zt - 0.08, bevel=0.0)
    # o emblema no lintel: a estrela vermelha no aro dourado
    circle_on_face(p, 'CnGold', (gx1 + 0.1, 0.0, (zt + 0.3 + PZ - 0.32) / 2), '+x', 0.5, 0.05)
    circle_on_face(p, 'Player', (gx1 + 0.13, 0.0, (zt + 0.3 + PZ - 0.32) / 2), '+x', 0.42, 0.04)
    star_wall(p, (gx1 + 0.16, 0.0, (zt + 0.3 + PZ - 0.32) / 2), '+x', 0.36, m='CnGold', t=0.03)
    # o balcão (a tribuna) em cima do portão: o piso claro e o guarda-corpo vermelho na frente e nos lados dos patamares
    p.box('CnPlinth', gx0 - 0.2, gx1, -LAND_Y[1], LAND_Y[1], PZ, PZ + 0.04, bevel=0.0)
    railing_red(p, [(gx1 + 0.1, -LAND_Y[0], PZ), (gx1 + 0.1, LAND_Y[0], PZ)], step=1.0)
    for s in (-1, 1):
        # o patamar de cima da escada (encostado no portão)
        p.box('CnConcrete', 1.5, STAIR_X[1], min(s * LAND_Y[0], s * LAND_Y[1]), max(s * LAND_Y[0], s * LAND_Y[1]), Z0 - 0.05, PZ, bevel=0.05)
        p.box('CnPlinth', 1.5, STAIR_X[1] + 0.1, min(s * LAND_Y[0], s * LAND_Y[1]), max(s * LAND_Y[0], s * LAND_Y[1]), PZ - 0.3, PZ + 0.04, bevel=0.03)


def stairs(p):
    """As duas escadarias na face da frente: sobem dos cantos (|y| 12,2) até o patamar junto do portão (|y| 5,2)."""
    xa, xb = STAIR_X
    n = 20
    run = (STAIR_BOT - LAND_Y[1]) / n
    rise = (PZ - Z0) / n
    for s in (-1, 1):
        for k in range(n):
            y_out = STAIR_BOT - k * run
            y_in = y_out - run
            z = Z0 + (k + 1) * rise
            p.box('CnPlinth#step', xa, xb - 0.3, min(s * y_in, s * y_out), max(s * y_in, s * y_out), Z0 - 0.05, z, bevel=0.015)
        # o muro da escada (a mureta inclinada por fora) e o corrimão vermelho em cima dele
        poly = [(s * (STAIR_BOT + 0.3), Z0 - 0.05), (s * (STAIR_BOT + 0.3), Z0 + 0.55), (s * LAND_Y[1], PZ + 0.45),
                (s * LAND_Y[0], PZ + 0.45), (s * LAND_Y[0], Z0 - 0.05)]
        if s < 0:
            poly = list(reversed(poly))
        prism_yz(p, 'CnConcrete', poly, xb - 0.32, xb, bevel=0.03)
        railing_red(p, [(xb - 0.16, s * (STAIR_BOT + 0.2), Z0 + 0.55), (xb - 0.16, s * LAND_Y[1], PZ + 0.45),
                        (xb - 0.16, s * LAND_Y[0], PZ + 0.45)], h=0.85, step=1.3)
        # o pilar do pé da escada com a lanterna de pedra
        p.box('CnPlinth', xa + 0.4, xb + 0.1, s * (STAIR_BOT + 0.3) - 0.5, s * (STAIR_BOT + 0.3) + 0.5, Z0 - 0.05, Z0 + 1.2, bevel=0.05)
        p.box('CnPlinth', xa + 0.3, xb + 0.2, s * (STAIR_BOT + 0.3) - 0.6, s * (STAIR_BOT + 0.3) + 0.6, Z0 + 1.2, Z0 + 1.35, bevel=0.03)
        p.box('Light', xa + 1.1, xb - 0.6, s * (STAIR_BOT + 0.3) - 0.25, s * (STAIR_BOT + 0.3) + 0.25, Z0 + 1.35, Z0 + 1.75, bevel=0.0)
        p.box('CnRoof', xa + 0.8, xb - 0.3, s * (STAIR_BOT + 0.3) - 0.45, s * (STAIR_BOT + 0.3) + 0.45, Z0 + 1.75, Z0 + 1.9, bevel=0.03)


def paifang(p):
    """O pórtico vermelho na frente do portão (o paifang): 2 colunas nos tambores de pedra, as duas vigas com o friso dourado,
    a placa dourada com a estrela, os suportes (dougong) e o telhadinho de telha verde."""
    x, ys = PAI_X, PAI_Y
    zc = 5.25                                                                   # o topo das colunas
    for s in (-1, 1):
        y = s * ys
        p.box('CnPlinth', x - 0.45, x + 0.45, y - 0.45, y + 0.45, Z0 - 0.05, Z0 + 0.45, bevel=0.04)
        for d in (-1, 1):                                                       # os tambores que abraçam o pé da coluna
            p.cyl('CnPlinth', (x + d * 0.42, y, Z0 + 0.95), 'y', 0.42, 0.28, seg=20, bevel=0.0)
            p.box('CnPlinth', min(x + d * 0.2, x + d * 0.75), max(x + d * 0.2, x + d * 0.75), y - 0.14, y + 0.14, Z0 + 0.45, Z0 + 0.62, bevel=0.02)
        p.cyl('Player', (x, y, (Z0 + 0.45 + zc) / 2), 'z', 0.24, zc - Z0 - 0.45, seg=20, bevel=0.0, smooth=True)
        p.cyl('CnGold', (x, y, zc - 0.55), 'z', 0.26, 0.08, seg=20, bevel=0.0)
    L = ys + 0.75
    p.box('Player', x - 0.22, x + 0.22, -L, L, 4.55, 4.9, bevel=0.03)                           # a viga de baixo
    p.box('Player', x - 0.25, x + 0.25, -L - 0.1, L + 0.1, zc - 0.05, zc + 0.35, bevel=0.03)   # a viga de cima
    for zz in (4.7, zc + 0.12):
        p.box('CnGold', x + 0.22, x + 0.27, -L + 0.1, L - 0.1, zz - 0.03, zz + 0.03, bevel=0.0)
        p.box('CnGold', x - 0.27, x - 0.22, -L + 0.1, L - 0.1, zz - 0.03, zz + 0.03, bevel=0.0)
    # a placa no meio (entre as vigas): moldura dourada, fundo vermelho, a estrela dourada
    p.box('CnGold', x - 0.12, x + 0.12, -1.1, 1.1, 4.9, zc - 0.05, bevel=0.02)
    p.box('Player', x + 0.12, x + 0.15, -0.95, 0.95, 4.95, zc - 0.1, bevel=0.0)
    star_wall(p, (x + 0.15, 0.0, (4.95 + zc - 0.1) / 2), '+x', 0.17, m='CnGold', t=0.02)
    for s in (-1, 1):
        for k in range(3):                                                     # os painéis verdes entre as vigas
            y0 = s * (1.25 + k * 0.85)
            p.box('CnRoof', x - 0.1, x + 0.1, min(y0, y0 + s * 0.7), max(y0, y0 + s * 0.7), 4.92, zc - 0.07, bevel=0.0)
    for k in range(9):                                                         # os suportes (dougong) sob o beiral
        y = -L + 0.4 + k * (2 * L - 0.8) / 8
        p.box('CnGold', x - 0.32, x + 0.32, y - 0.13, y + 0.13, zc + 0.35, zc + 0.48, bevel=0.0)
        p.box('Player', x - 0.2, x + 0.2, y - 0.1, y + 0.1, zc + 0.48, zc + 0.58, bevel=0.0)
    cn_roof(p, x, 0.0, 0.35, L - 0.05, zc + 0.62, 0.72, 0.6, (0.1, L - 0.95), curl=0.32, kick=0.22, ridge_h=0.2, thick=0.22,
            pitch=0.5)
    for s in (-1, 1):                                                          # as lanternas nas pontas da viga
        lantern(p, x, s * (L - 0.25), 4.55, drop=0.3, r=0.24, h=0.42)


def courtyard(p):
    polygon_prism(p, 'Player', star_points(STAR_C[0], STAR_C[1], STAR_R, rot_deg=0.0), Z0, Z0 + 0.014)
    # as faixas de perigo dos lados da saída do portão (a faixa de 6,6 m)
    for s in (-1, 1):
        for k in range(5):
            xa = GATE[1] + 0.2 + k * 0.6
            p.box('Hazard' if k % 2 == 0 else 'Dark', xa, xa + 0.6, s * 3.3 - 0.12, s * 3.3 + 0.12, Z0, Z0 + 0.012, bevel=0.0)
    for s in (-1, 1):
        lion(p, PAI_X + 0.3, s * 5.2, Z0 - 0.05, flip=s)


def hall(p):
    hx0, hx1, hy0, hy1 = HALL
    sx0, sx1, sy0, sy1 = STYLO
    p.box('CnPlinth', sx0, sx1, sy0, sy1, PZ - 0.1, ZH0, bevel=0.05)                   # a plataforma
    p.box('Dark', sx0 - 0.02, sx1 + 0.02, sy0 - 0.02, sy1 + 0.02, ZH0 - 0.12, ZH0 - 0.09, bevel=0.0)
    for k in range(3):                                                                 # os degraus da frente
        p.box('CnPlinth', sx1, sx1 + 0.3 * (3 - k), -1.8, 1.8, PZ, PZ + 0.15 * (k + 1), bevel=0.02)
    # (31/10) o salão OCO: as paredes de 0,35 m com os vãos de verdade (a porta, as janelas de treliça recuadas na espessura), a
    # laje e o interior (as vigas); a fachada e os caixilhos ficam dentro do vão
    holes_x = [(-1.05, 1.05, ZH0, ZH0 + 2.25)] + [(yb - 0.72, yb + 0.72, ZH0 + 0.55, ZE1 - 0.65) for yb in BAYS]
    holes_y = [(x - 0.55, x + 0.55, ZH0 + 0.8, ZE1 - 0.7) for x in (-7.5, -5.2, -2.9)]
    holes_b = [(y - 0.55, y + 0.55, ZH0 + 0.8, ZE1 - 0.7) for y in (-3.6, -1.2, 1.2, 3.6)]
    cn_block(p, hx0, hx1, hy0, hy1, ZH0 - 0.05, ZE1, t=0.35, skin=(), holes={'+x': holes_x, '+y': holes_y, '-y': holes_y, '-x': holes_b},
             roof_t=0.3, parapet=0.0, corners=False, cornice=False, interior=True, plinth=0)
    # a colunata vermelha da frente, a viga sobre ela (vermelha com o friso dourado) e as bases de pedra
    for y in COL_YS:
        p.cyl('CnPlinth', (COL_X, y, ZH0 + 0.1), 'z', 0.34, 0.2, seg=16, bevel=0.0)
        p.cyl('Player', (COL_X, y, (ZH0 + ZE1) / 2), 'z', 0.2, ZE1 - ZH0, seg=16, bevel=0.0, smooth=True)
    for (yy0, yy1) in ((hy0 - 0.1, hy0 + 0.3), (hy1 - 0.3, hy1 + 0.1)):                   # as colunas engastadas dos cantos
        p.box('Player', hx1 - 0.1, hx1 + 0.12, yy0, yy1, ZH0, ZE1 - 0.4, bevel=0.0)
    p.box('Player', COL_X - 0.25, COL_X + 0.25, hy0 - 0.15, hy1 + 0.15, ZE1 - 0.42, ZE1, bevel=0.03)
    p.box('CnGold', COL_X + 0.25, COL_X + 0.29, hy0, hy1, ZE1 - 0.3, ZE1 - 0.2, bevel=0.0)
    for y in (hy0 - 0.15, hy1 + 0.15):                                                 # as vigas dos lados da varanda
        p.box('Player', hx1 - 0.1, COL_X, y - 0.2, y + 0.2, ZE1 - 0.42, ZE1, bevel=0.02)
    # a fachada: a porta vermelha com os cravos dourados, as janelas de treliça entre as colunas
    fx = hx1
    fr = fx - 0.16                                                                   # o plano da porta e das treliças, no vão
    for (a, b) in ((-1.25, -1.05), (1.05, 1.25)):                                     # a moldura clara da porta (ombreiras e verga)
        p.box('CnPlinth#jamb', fx - 0.05, fx + 0.12, a, b, ZH0, ZH0 + 2.45, bevel=0.03)
    p.box('CnPlinth#jamb', fx - 0.05, fx + 0.12, -1.25, 1.25, ZH0 + 2.25, ZH0 + 2.45, bevel=0.03)
    for s in (-1, 1):
        y0, y1 = (-1.05, -0.02) if s < 0 else (0.02, 1.05)
        p.box('Player', fr - 0.06, fr, y0, y1, ZH0, ZH0 + 2.25, bevel=0.0)
        for i in range(4):
            for j in range(5):
                p.cyl('CnGold', (fr + 0.02, y0 + 0.2 + i * (y1 - y0 - 0.4) / 3, ZH0 + 0.45 + j * 0.36), 'x', 0.04, 0.05, seg=6, bevel=0.0)
    for yb in BAYS:
        p.box('Dark', fr - 0.1, fr - 0.06, yb - 0.72, yb + 0.72, ZH0 + 0.55, ZE1 - 0.65, bevel=0.0)
        for k in range(5):
            yy = yb - 0.72 + k * 1.44 / 4
            p.box('CnGold', fr - 0.06, fr, yy - 0.03, yy + 0.03, ZH0 + 0.55, ZE1 - 0.65, bevel=0.0)
        for k in range(5):
            zz = ZH0 + 0.55 + k * (ZE1 - 1.2 - ZH0) / 4
            p.box('CnGold', fr - 0.06, fr, yb - 0.72, yb + 0.72, zz - 0.03, zz + 0.03, bevel=0.0)
        p.box('CnPlinth#sill', fx - 0.2, fx + 0.14, yb - 0.82, yb + 0.82, ZH0 + 0.43, ZH0 + 0.55, bevel=0.03)
    # as janelas dos lados e do fundo (escuras, com o peitoril)
    for s in (-1, 1):
        for x in (-7.5, -5.2, -2.9):
            yf = hy1 if s > 0 else hy0
            yg = yf - s * 0.18
            p.box('Glass', x - 0.55, x + 0.55, min(yg, yg - s * 0.03), max(yg, yg - s * 0.03), ZH0 + 0.8, ZE1 - 0.7, bevel=0.0)
            p.box('CnPlinth#sill', x - 0.65, x + 0.65, min(yf - s * 0.2, yf + s * 0.12), max(yf - s * 0.2, yf + s * 0.12), ZH0 + 0.68, ZH0 + 0.8, bevel=0.03)
    for y in (-3.6, -1.2, 1.2, 3.6):
        p.box('Glass', hx0 + 0.15, hx0 + 0.18, y - 0.55, y + 0.55, ZH0 + 0.8, ZE1 - 0.7, bevel=0.0)
        p.box('CnPlinth#sill', hx0 - 0.12, hx0 + 0.2, y - 0.65, y + 0.65, ZH0 + 0.68, ZH0 + 0.8, bevel=0.03)
    # as lanternas vermelhas no beiral da frente, entre as colunas
    for y in BAYS:
        lantern(p, COL_X + 0.75, y, ZE1 - 0.05, drop=0.35)
    # os 3 telhados: o 1 cobre o salão e a varanda; 2 e 3 em cima dos andares de janelinhas
    cx = (hx0 + COL_X + 0.25) / 2
    wx1, wy1 = (COL_X + 0.25 - hx0) / 2, hy1 + 0.15
    t2 = (2.7, 3.7)
    t3 = (1.75, 2.3)
    z1t = cn_roof(p, cx, 0.0, wx1, wy1, ZE1, 1.1, 1.25, (t2[0] + 0.05, t2[1] + 0.05), curl=0.6, kick=0.4)
    ze2 = z1t + 0.85
    tier_wall(p, cx, t2, z1t - 0.25, ze2)
    z2t = cn_roof(p, cx, 0.0, t2[0], t2[1], ze2, 0.9, 1.0, (t3[0] + 0.05, t3[1] + 0.05), curl=0.5, kick=0.35)
    ze3 = z2t + 0.6
    tier_wall(p, cx, t3, z2t - 0.2, ze3)
    z3t = cn_roof(p, cx, 0.0, t3[0], t3[1], ze3, 0.95, 0.85, (0.12, 1.3), curl=0.45, kick=0.3, ridge_h=0.32)
    return z3t + 0.32


def tier_wall(p, cx, half, z0, z1):
    """O andar curto entre dois telhados: parede, a fileira de janelinhas escuras e o friso dourado sob o beiral."""
    hx, hy = half
    p.box('CnConcrete', cx - hx, cx + hx, -hy, hy, z0, z1, bevel=0.04)
    for (side, L) in (('x', hy), ('y', hx)):
        n = max(2, int(2 * L / 1.25))
        for s in (-1, 1):
            for k in range(n):
                a = -L + (k + 0.5) * 2 * L / n
                at = (cx + s * hx) if side == 'x' else s * hy
                F = WallFace(('+' if s > 0 else '-') + side, at, 0.0)
                u = a if side == 'x' else cx + a
                za, zb = z0 + 0.35, z1 - 0.2
                wbox(p, 'Dark', F, u - 0.32, u + 0.32, za, zb, -0.04, 0.005, bevel=0.0)
                for (q0, q1) in ((u - 0.36, u - 0.3), (u + 0.3, u + 0.36), (u - 0.025, u + 0.025)):
                    wbox(p, 'Player#lat', F, q0, q1, za - 0.03, zb + 0.03, -0.01, 0.05, bevel=0.0)
                for zz in (za - 0.03, zb - 0.02):
                    wbox(p, 'Player#lat', F, u - 0.36, u + 0.36, zz, zz + 0.05, -0.01, 0.05, bevel=0.0)
    p.box('CnGold', cx - hx - 0.03, cx + hx + 0.03, -hy - 0.03, hy + 0.03, z1 - 0.14, z1 - 0.08, bevel=0.0)


def terraces(p):
    """Os terraços dos lados do salão: o tambor do radar (esquerda), o mastro de rádio (direita), os estandartes do balcão."""
    # o tambor do radar, com o anel escuro do trilho e o corrimão vermelho em volta
    rx, ry, rz = RADAR_AT
    p.cyl('CnPlinth', (rx, ry, (PZ + rz) / 2), 'z', 1.1, rz - PZ, seg=40, bevel=0.03)
    p.cyl('Dark', (rx, ry, rz + 0.01), 'z', 0.95, 0.02, seg=40, bevel=0.0)
    p.cyl('CnConcrete', (rx, ry, rz - 0.18), 'z', 1.16, 0.12, seg=40, bevel=0.0)
    for k in range(6):                                                          # os contrafortes do tambor
        a = 2 * math.pi * (k + 0.5) / 6
        p.obox('CnConcrete', (rx + 1.15 * math.cos(a), ry + 1.15 * math.sin(a), PZ + 0.3), (0.3, 0.3, 0.6),
               yaw=math.degrees(a), bevel=0.03)
    # a caixa de equipamento do radar (o gerador) e o cabo
    p.box('CnConcrete', -9.4, -7.6, -10.6, -9.0, PZ, PZ + 1.2, bevel=0.05)
    p.box('Dark', -7.62, -7.58, -10.3, -9.3, PZ + 0.3, PZ + 0.9, bevel=0.0)
    for k in range(4):
        p.box('CnMetal', -9.2 + k * 0.42, -9.0 + k * 0.42, -10.5, -9.1, PZ + 1.2, PZ + 1.3, bevel=0.0)
    tube(p, 'Dark', (-7.6, -9.8, PZ + 0.12), (rx - 1.25, ry, PZ + 0.12), 0.08)
    # o mastro de rádio (direita): a casa de equipamento no pé, a treliça, as cornetas, a antena e a baliza
    mx, my = MAST_AT
    p.box('CnConcrete', mx - 1.2, mx + 1.2, my - 1.2, my + 1.2, PZ, PZ + 1.0, bevel=0.05)
    p.box('CnPlinth', mx - 1.3, mx + 1.3, my - 1.3, my + 1.3, PZ + 1.0, PZ + 1.12, bevel=0.03)
    z0, z1 = PZ + 1.12, 15.0
    lattice_mast(p, (mx, my), z0, z1, 0.75, 0.28, 6)
    speaker_cluster(p, (mx, my), 12.6, n=4, r_horn=0.42, length=1.0, tilt=-14, m='CnMetal', yaw0=45)
    speaker_cluster(p, (mx, my), 11.6, n=4, r_horn=0.38, length=0.9, tilt=-18, m='CnMetal', yaw0=0)
    p.cyl('CnMetal', (mx, my, z1 + 0.15), 'z', 0.3, 0.3, seg=12, bevel=0.0)
    p.cyl('CnMetal', (mx, my, z1 + 0.85), 'z', 0.05, 1.4, seg=8, bevel=0.0)
    p.cyl('Beacon', (mx, my, z1 + 1.65), 'z', 0.1, 0.2, seg=12, bevel=0.0, smooth=True)
    for zz, L in ((13.6, 1.6), (14.3, 1.1)):                                  # os dipolos da antena
        p.cyl('CnMetal', (mx, my, zz), 'y', 0.03, L, seg=6, bevel=0.0)
    p.cyl('CnMetal', (mx + 0.45, my, 10.2), 'y', 0.04, 1.5, seg=6, bevel=0.0)  # a travessa do estandarte
    p.box('Player', mx + 0.47, mx + 0.5, my - 0.6, my + 0.6, 7.6, 10.15, bevel=0.0)
    star_wall(p, (mx + 0.5, my, 9.55), '+x', 0.3, m='CnGold', t=0.012)
    p.box('CnGold', mx + 0.5, mx + 0.512, my - 0.035, my + 0.035, 7.9, 9.15, bevel=0.0)
    for (sx, sy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):                     # os estais, presos nos cantos da casinha
        tube(p, 'Dark', (mx + sx * 0.36, my + sy * 0.36, 13.0), (mx + sx * 1.15, my + sy * 1.15, PZ + 1.12), 0.015, seg=4)
    # os 2 estandartes na borda da frente do terraço, emoldurando o salão (o 3º é o do mastro)
    for s in (-1, 1):
        banner2(p, (0.85, s * 7.3), PZ, PZ + 4.9, height=3.3, width=1.2, face='+x')
    # as luzes do terraço (postes baixos nos cantos da frente)
    for s in (-1, 1):
        x, y = 0.6, s * 10.6
        p.cyl('CnMetal', (x, y, PZ + 1.1), 'z', 0.06, 2.2, seg=8, bevel=0.0)
        p.box('Dark', x - 0.18, x + 0.18, y - 0.18, y + 0.18, PZ + 2.2, PZ + 2.45, bevel=0.0)
        p.box('Light', x - 0.14, x + 0.14, y - 0.14, y + 0.14, PZ + 2.12, PZ + 2.2, bevel=0.0)


def base():
    p = BPart(PREFIX + '_Base')
    foundation(p, HX, HY, m='CnPlinth', z=Z0)
    podium(p)
    gate(p)
    stairs(p)
    paifang(p)
    courtyard(p)
    top = hall(p)
    terraces(p)
    print('MODEL cumeeira do pagode', round(top, 2), 'm')
    return p.finish()


# ===================================================================================================== o radar (gira)
def radar():
    """O radar de treliça: o refletor curvo (cilindro parabólico de tubos) inclinado pra trás, a treliça de apoio, o braço do
    alimentador, a cabine e a mesa giratória. Origem no eixo, no pé da mesa; olha pra +X na pose de repouso."""
    p = BPart(PREFIX + '_Radar')
    p.cyl('CnMetal', (0.0, 0.0, 0.15), 'z', 0.95, 0.3, seg=32, bevel=0.02)
    p.cyl('Dark', (0.0, 0.0, 0.31), 'z', 0.8, 0.03, seg=32, bevel=0.0)
    # a cabine atrás do eixo (contrapeso), com a faixa vermelha, a porta e a janelinha
    p.box('CnConcrete', -1.25, 0.15, -0.65, 0.65, 0.3, 1.45, bevel=0.05)
    p.box('CnMetal', -1.3, 0.2, -0.7, 0.7, 1.45, 1.55, bevel=0.02)
    p.box('Player', -1.27, -1.25, -0.55, 0.55, 0.95, 1.2, bevel=0.0)
    p.box('Player', -1.25, 0.15, 0.65, 0.67, 0.95, 1.2, bevel=0.0)
    p.box('Player', -1.25, 0.15, -0.67, -0.65, 0.95, 1.2, bevel=0.0)
    p.box('Dark', -0.9, -0.3, -0.67, -0.65, 0.35, 1.15, bevel=0.0)
    p.box('Glass', -0.8, -0.2, 0.65, 0.68, 0.7, 1.05, bevel=0.0)
    # o refletor: largura W, altura H, a curva em planta (fundo 0,55 m) e a inclinação de 14° pra trás
    W, H, depth, tilt = 5.4, 2.8, 0.5, math.radians(14)
    C = Vector((0.3, 0.0, 3.05))                                        # o centro do refletor

    def rp(yy, vv, back=0.0):
        """Ponto do refletor: yy lateral (−W/2..W/2), vv vertical (−H/2..H/2); back = recuo pra trás."""
        xl = depth * (2 * yy / W) ** 2 - back                            # as bordas vêm pra frente (o foco fica na frente)
        x = xl * math.cos(tilt) - vv * math.sin(tilt)
        z = xl * math.sin(tilt) + vv * math.cos(tilt)
        return (C.x + x, C.y + yy, C.z + z)
    nh, nv = 6, 11
    ys = [-W / 2 + W * i / (nv - 1) for i in range(nv)]
    for j in range(nh):                                                  # as barras horizontais (arcos)
        vv = -H / 2 + H * j / (nh - 1)
        r = 0.07 if j in (0, nh - 1) else 0.032
        for a, b in zip(ys[:-1], ys[1:]):
            tube(p, 'Paint', rp(a, vv), rp(b, vv), r, seg=6)
    for i, yy in enumerate(ys):                                          # as verticais
        r = 0.07 if i in (0, nv - 1) else 0.032
        tube(p, 'Paint', rp(yy, -H / 2), rp(yy, H / 2), r, seg=6)
    # a treliça de trás: as costelas em X e o tubo de espinha até o mastro
    for i in range(0, nv - 1, 2):
        tube(p, 'CnMetal', rp(ys[i], -H / 2, 0.0), rp(ys[i + 1], H / 2, 0.0), 0.028, seg=4)
        tube(p, 'CnMetal', rp(ys[i], H / 2, 0.0), rp(ys[i + 1], -H / 2, 0.0), 0.028, seg=4)
    hub = Vector((-0.15, 0.0, 2.55))
    for yy in (-W * 0.36, -W * 0.12, W * 0.12, W * 0.36):
        for vv in (-H / 2, H / 2):
            tube(p, 'CnMetal', tuple(hub), rp(yy, vv, 0.0), 0.04, seg=6)
    p.cyl('CnMetal', (-0.15, 0.0, 2.0), 'z', 0.2, 1.2, seg=12, bevel=0.0)          # o mastro do refletor
    p.box('CnMetal', -0.45, 0.15, -0.35, 0.35, 1.55, 1.75, bevel=0.02)
    p.cyl('Dark', tuple(hub), 'y', 0.22, 0.7, seg=12, bevel=0.0)                   # o eixo de elevação
    # o braço do alimentador (do pé do refletor até o foco na frente) e a corneta do alimentador
    foot = Vector(rp(0.0, -H / 2))
    focus = Vector((C.x + 1.75, 0.0, C.z - 0.25))
    tube(p, 'CnMetal', tuple(foot), tuple(focus), 0.06, seg=8)
    tube(p, 'CnMetal', rp(-0.9, -H / 2), tuple(focus), 0.035, seg=6)
    tube(p, 'CnMetal', rp(0.9, -H / 2), tuple(focus), 0.035, seg=6)
    p.box('CnMetal', focus.x - 0.18, focus.x + 0.12, -0.42, 0.42, focus.z - 0.18, focus.z + 0.18, bevel=0.03)
    p.box('Dark', focus.x - 0.21, focus.x - 0.18, -0.36, 0.36, focus.z - 0.13, focus.z + 0.13, bevel=0.0)
    # a baliza vermelha no topo do refletor
    top = Vector(rp(0.0, H / 2))
    p.cyl('Beacon', (top.x, 0.0, top.z + 0.14), 'z', 0.09, 0.18, seg=12, bevel=0.0, smooth=True)
    return p.finish()


# ===================================================================================================== saída
def main():
    clear_scene()
    o = base()
    report(o)
    total = export(o, OUT)
    clear_scene()
    o = radar()
    report(o)
    bake_in_place(o, RADAR_AT)           # (31/10) no executor do detalhe, a sombra de contato com o radar no lugar dele
    total += export(o, OUT)
    clear_scene()
    sc = scaffold(PREFIX + '_Scaffold', POD[0] - 0.2, PAI_X + 1.3, POD[2] - 0.3, POD[3] + 0.3, 9.0,
                  post='CnMetal', plank='Crate', stripe='Hazard')
    total += export(sc, OUT)
    print('MODEL total', total, 'tris')
    print('MODEL lote 24 x 28 m; podio', PZ, 'm; beiral do salao', round(ZE1, 2), 'm; radar at_m', RADAR_AT, 'rpm 6')


main()
