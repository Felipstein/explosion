"""O helicóptero de ataque da USA, o AH-64 APACHE (a chave do jogo é usa_comanche), VERSÃO B no detalhe AAA (01/11/2026): a versão A
(Game/Tools/model_usa_comanche.py, aprovada no sistema aéreo de 24/10: "meus parabéns, muita qualidade!" e "Apache") fica como está;
esta é a que ele compara com ela.

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_usa_comanche_v2.py C:/Users/luisf/explosion/captures/game/models/usa_comanche_v2

O pedido (31/10 e 01/11): "TEXTURIZADO, DETALHADO [...] CURVAS [...] NADA DE QUINAS QUADRADAS". Não há ficha de 5 vistas (a ficha de
texto docs/elenco/usa/usa_comanche.md §6: o disco do rotor cobrindo o corpo estreito, a bola do radar no centro, as asinhas com os 4
Hellfires, a cauda saindo do disco; a cor do jogador na faixa da cauda, no topo das naceles e nas asinhas). A base é o AH-64D/E de
verdade e o layout é o do A (as mesmas estações, os mesmos pilones), refeito no que o estudo pede (docs/ESTUDO_DETALHE_AAA.md §3):
  - a fuselagem interpolada lisa (a seção de lados retos do Apache) com os sulcos de painel em anel e as juntas corridas, a capa do
    eixo do rotor de cauda em cima da cauda, os degraus e as alças de subir, as antenas, o corta-fio em cima da cabine e no queixo;
  - a cabine em tandem FACETADA (as placas chatas do Apache) com os montantes escuros em cada junta de vidro e a divisória do meio;
  - o nariz dos sensores torneado (o TADS: o tambor com os dois casulos dos lados e as janelas; o PNVS em cima), o canhão de 30 mm
    com a torre, a capa de refrigeração e a boca;
  - as naceles lisas com a boca de ar funda e o escape "buraco negro" virado pra fora (o duto escuro), a carenagem até o corpo, as
    grelhas da corcunda, o mastro com o prato oscilante, o radar Longbow torneado;
  - as asinhas de perfil com os pilones de quina redonda, os lançadores M299 com os trilhos, os casulos M261 com as 19 bocas fundas,
    os Hellfires de verdade (o buscador de vidro, as asinhas e os lemes);
  - o rotor de 4 pás que AFINAM e TORCEM com a ponta enflechada amarela, o cubo com os punhos e as bielas; o rotor de cauda em tesoura;
  - o trem com o braço, o amortecedor e o pneu de avião; a bequilha.
A USA não tem desgaste pintado (13/10). Escala (como o A): desenhado no tamanho real e exportado × 0,58; os detalhes que têm de ler
na câmera medidos no final e divididos pela escala (F).

Peças (os NOMES do A; origem no chão, embaixo do mastro do rotor; frente em +X):
  SM_USA_Comanche_Body, SM_USA_Comanche_Rotor (origem no cubo, ROTOR_AT), SM_USA_Comanche_TailFan (origem no cubo, FAN_AT, eixo Y),
  SM_USA_Comanche_Pods (pod_001 à direita, pod_002 à esquerda), SM_USA_Comanche_Missiles (hellfire_001..004).
  Slots (os do A): Paint, Team, Dark, Metal, Rubber, Lens, Light, Yellow.
Pivôs MANTIDOS: o rotor no cubo (0; 0; 3,7 × 0,58 = 2,146 na malha; o Units.json, com a mesh_scale 1,2: 2,575), o de cauda em
(−10; −0,34; 3,55) × 0,58 (Units.json −6,96; −0,236; 2,471), o raio, a corda e a ponta das pás iguais (o disco borrado do jogo).
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import model_lib  # noqa: E402
from model_lib import COLORS, Part, V, clear_scene, export, export_group  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_air_parts as ap  # noqa: E402
from model_china_vehicle_kit import bounds, tris  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_comanche_v2')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_USA_Comanche'
SCALE = 0.58
F = 1.0 / SCALE

COLORS.update({'Paint': (196, 198, 192), 'Team': (86, 108, 150), 'Dark': (40, 40, 38), 'Metal': (95, 95, 92),
               'Rubber': (32, 32, 31), 'Lens': (92, 128, 168), 'Light': (255, 230, 170), 'Yellow': (214, 164, 32)})

# ---------------------------------------------------------------------------------- medidas do A (tamanho real, m)
FUS = [(5.12, 0.3, 0.5, 1.42, 2.2), (4.85, 0.62, 0.86, 1.45, 2.4), (4.3, 0.9, 1.22, 1.52, 2.8), (3.3, 1.0, 1.5, 1.62, 3.0),
       (2.2, 1.06, 1.86, 1.78, 3.0), (0.9, 1.12, 2.02, 1.85, 3.0), (-0.6, 1.16, 1.84, 1.95, 3.0), (-2.0, 0.96, 1.44, 2.02, 2.8),
       (-3.6, 0.6, 0.86, 2.02, 2.6), (-6.5, 0.46, 0.6, 1.92, 2.5), (-9.2, 0.38, 0.52, 1.85, 2.4), (-10.4, 0.2, 0.36, 1.86, 2.2)]
CANOPY = [(4.62, 0.36, 2.0, 2.06), (4.25, 0.8, 2.0, 2.32), (3.15, 0.92, 2.15, 2.52), (2.95, 0.94, 2.2, 2.8),
          (1.7, 0.96, 2.4, 3.0), (1.2, 0.8, 2.55, 2.94), (0.9, 0.4, 2.7, 2.8)]
ROTOR_AT = (0.0, 0.0, 3.7)
ROTOR_R, ROTOR_BLADES, ROTOR_CHORD = 7.315, 4, 0.533
TIP_LEN = 0.45                    # a ponta amarela (Units.json tip_m = 0,45 × 0,58 × 1,2)
RADAR_AT = (0.0, 0.0, 4.45)
FAN_AT = (-10.0, -0.34, 3.55)
FAN_R, FAN_CHORD = 1.395, 0.25
FAN_ANGLES = (0.0, 55.0, 180.0, 235.0)
NACELLE_Y, NACELLE_Z = 0.86, 2.28
WING_Z = 1.72
PYLON_IN, PYLON_OUT = 1.3, 2.18
POD_X = (1.0, -0.66)
POD_Z, POD_R = 1.3, 0.2
HELLFIRE_X = (1.12, -0.5)
HELLFIRE_Z, HELLFIRE_R = 1.36, 0.09
HELLFIRE_DY = 0.15
GUN_AT = (3.55, 0.0, 0.78)
GUN_MUZZLE = (5.25, 0.0, 0.62)

SEC = ap.Section(FUS, smooth=5)
SW = 0.015 * F                    # a linha de painel: 1,5 cm no final
GW, GD = 0.025 * F, 0.01 * F      # o sulco em anel: 2,5 × 1 cm no final


def deg(a):
    return math.radians(a)


# ---------------------------------------------------------------------------------- o corpo
def fuselage(p):
    ths = ap.angles(32)
    xs = [r[0] for r in SEC.rows]
    ap.skin(p, 'Paint#fus', SEC, xs, ths, grooves=[(x, GW, GD) for x in (4.45, -2.35, -4.6, -7.4)], caps=(True, True))
    # as juntas corridas (o lado de cima e a barriga), as tampas de acesso, a faixa azul em volta da cauda
    for s in (1, -1):
        def T(a):
            return deg(a) if s > 0 else math.pi - deg(a)
        ap.sec_ribbon(p, 'Dark', SEC, 2.6, -2.2, T(28), SW)
        ap.sec_ribbon(p, 'Dark', SEC, 4.2, -2.2, T(-20), SW)
        ap.sec_rect(p, 'Dark', SEC, 0.3, -1.0, T(-12), T(14), SW)            # a baia de aviônicos (as "bochechas" do Apache)
        ap.sec_rect(p, 'Dark', SEC, -5.6, -6.6, T(30), T(70), SW)
        # as alças de subir (U) e os degraus dobráveis do lado
        for x, z in ((2.6, 1.55), (1.0, 1.65)):
            th = math.atan2(z - SEC.at(x)[3], 0.5) if s > 0 else math.pi - math.atan2(z - SEC.at(x)[3], 0.5)
            q = SEC.P(x, th)
            n = SEC.N(x, th)
            vp.grab_handle(p, 'Metal', q - Vector((0.12, 0, 0)), q + Vector((0.12, 0, 0)), n, height=0.07, r=0.012 * F, R=0.03)
    ap.sec_patch(p, 'Team', SEC, -6.0, -6.9, 0.0, 2 * math.pi, nx=3, nt=32, lift=0.006)
    # a capa do eixo do rotor de cauda em cima da cauda (as tampas articuladas, a junta entre elas)
    rings = []
    for x, w, h in ((-2.3, 0.05, 0.02), (-2.8, 0.26, 0.12), (-4.0, 0.26, 0.13), (-6.5, 0.24, 0.12), (-8.6, 0.22, 0.12), (-9.2, 0.06, 0.03)):
        top = SEC.P(x, math.pi / 2).z
        rings.append([(x, (w / 2) * math.copysign(abs(math.cos(t)) ** (2 / 3), math.cos(t)), top - 0.04 + h * abs(math.sin(t)) ** (2 / 3))
                      for t in ap.angles(14, 0.0, math.pi, closed=False)])
    ap.sheet(p, 'Paint#cover', rings, closed=True, smooth=True, caps=(True, True))
    for x in (-4.9, -7.2):
        top = SEC.P(x, math.pi / 2).z
        vp.tube(p, 'Dark', [(x, -0.13, top + 0.02), (x, 0.0, top + 0.095), (x, 0.13, top + 0.02)], 0.012 * F, seg=5)
    # as antenas de lâmina (na cauda, em cima e embaixo) e a luz anticolisão da barriga
    for x, th, h in ((-3.3, 90, 0.3), (-1.6, 270, 0.22), (-7.9, 270, 0.2)):
        q = SEC.P(x, deg(th))
        n = SEC.N(x, deg(th))
        vp.prism(p, 'Paint', [(0.0, 0.0), (-0.3, 0.0), (-0.26, h), (-0.1, h)], q - n * 0.02 - Vector((0, 0.02, 0)),
                 Vector((1, 0, 0)), n, Vector((0, 1, 0)), 0.04)
    q = SEC.P(-0.7, deg(270))
    vp.lathe(p, 'Light', q + Vector((0, 0, 0.01)), (0, 0, -1), [(0.0, 0.0), (0.07, 0.0), (0.07, 0.03), (0.05, 0.06), (0.0, 0.07)], seg=12)


def cockpit(p):
    """A cabine em tandem facetada (as placas chatas), os montantes escuros nas juntas, os arcos da divisória e o corta-fio."""
    rings = []
    for x, w, z0, z1 in CANOPY:
        r = []
        for k in range(13):
            t = math.pi * k / 12
            c, s = math.cos(t), math.sin(t)
            r.append((x, (w / 2) * math.copysign(abs(c) ** (2 / 2.6), c), z0 + (z1 - z0) * math.copysign(abs(s) ** (2 / 2.6), s)))
        rings.append(r)
    ap.sheet(p, 'Lens#glass', rings, closed=True, smooth=False, caps=(True, True))
    # os montantes: em cada estação (o arco) e nas juntas de comprimento (as colunas 2, 4, 8, 10 e o lado de baixo)
    fr = 0.016 * F
    for i in range(1, len(rings) - 1):
        vp.tube(p, 'Dark', [Vector(q) + Vector((0, 0, 0.006)) for q in rings[i]], fr, seg=5)
    for k in (0, 3, 9, 12):
        vp.tube(p, 'Dark', [Vector(r[k]) + Vector((0, 0, 0.006)) for r in rings], fr, seg=5)
    # os arcos (a divisória das 2 cabines e o fim) na cor do corpo, grossos
    for x in (3.05, 1.45):
        w = 0.96
        z0, z1 = (2.18, 2.66) if x > 2 else (2.47, 2.97)
        pts = []
        for k in range(13):
            t = math.pi * k / 12
            c, s = math.cos(t), math.sin(t)
            pts.append(Vector((x, (w / 2 + 0.02) * math.copysign(abs(c) ** (2 / 2.6), c),
                               z0 + (z1 - z0 + 0.03) * math.copysign(abs(s) ** (2 / 2.6), s))))
        vp.tube(p, 'Paint', pts, 0.026, seg=8)
    # o corta-fio: a lâmina em cima da cabine e a do queixo
    vp.prism(p, 'Metal', [(0.0, 0.0), (0.42, 0.0), (0.36, 0.14), (0.08, 0.08)], Vector((3.2, -0.02, 2.66)), Vector((1, 0, 0)),
             Vector((0, 0, 1)), Vector((0, 1, 0)), 0.04)
    vp.prism(p, 'Metal', [(0.0, 0.0), (0.4, 0.0), (0.34, -0.14), (0.06, -0.06)], Vector((4.25, -0.02, 1.04)), Vector((1, 0, 0)),
             Vector((0, 0, 1)), Vector((0, 1, 0)), 0.04)


def sensors_and_gun(p):
    """O TADS/PNVS do nariz e o canhão M230 do queixo, torneados."""
    # o TADS: o tambor que gira, a cinta escura, os dois casulos dos lados com as janelas
    vp.lathe(p, 'Paint', (5.2, 0.0, 1.07), 'z', [(0.0, 0.0), (0.2, 0.0), (0.3, 0.04), (0.34, 0.12), (0.34, 0.5), (0.3, 0.58),
                                               (0.18, 0.62), (0.0, 0.62)], seg=24)
    vp.lathe(p, 'Dark', (5.2, 0.0, 1.07), 'z', [(0.343, 0.25), (0.352, 0.26), (0.352, 0.36), (0.343, 0.37)], seg=24, closed=True)
    for s in (-1, 1):
        c = (5.25, s * 0.34, 1.38)
        vp.lathe(p, 'Paint', c, (0, s, 0), [(0.0, 0.0), (0.2, 0.0), (0.23, 0.04), (0.23, 0.14), (0.19, 0.18), (0.0, 0.18)], seg=18)
        vp.lathe(p, 'Lens', (5.44, s * 0.4, 1.4), 'x', [(0.0, 0.012), (0.09, 0.01), (0.1, 0.0), (0.0, 0.0)], seg=14)
        vp.lathe(p, 'Dark', (5.43, s * 0.4, 1.4), 'x', [(0.09, 0.0), (0.12, 0.0), (0.12, 0.022), (0.1, 0.03), (0.09, 0.02)], seg=14, closed=True)
    # o PNVS em cima do nariz (o tambor deitado com a janela)
    vp.lathe(p, 'Dark', (4.82, 0.0, 1.98), 'x', [(0.0, -0.05), (0.15, -0.05), (0.17, 0.0), (0.17, 0.22), (0.15, 0.28), (0.0, 0.3)], seg=18)
    vp.lathe(p, 'Lens', (5.11, 0.0, 1.98), 'x', [(0.0, 0.008), (0.1, 0.006), (0.11, 0.0), (0.0, 0.0)], seg=14)
    # o canhão: a torre, o berço, o cano com a capa de refrigeração e a boca
    gx, gy, gz = GUN_AT
    vp.lathe(p, 'Dark', (gx, gy, gz + 0.02), 'z', [(0.0, 0.0), (0.2, 0.0), (0.25, 0.05), (0.25, 0.22), (0.21, 0.27), (0.0, 0.27)], seg=20)
    vp.lathe(p, 'Metal', (gx + 0.15, -0.18, gz - 0.08), (0, 1, 0), [(0.0, 0.0), (0.13, 0.0), (0.16, 0.03), (0.16, 0.33), (0.13, 0.36),
                                                                 (0.0, 0.36)], seg=16)
    L = GUN_MUZZLE[0] - gx
    vp.lathe(p, 'Metal', (gx + 0.2, 0.0, GUN_MUZZLE[2]), 'x', [(0.0, 0.0), (0.05, 0.0), (0.05, L - 0.12), (0.06, L - 0.1),
                                                              (0.06, L - 0.2), (0.0, L - 0.2)], seg=12)
    vp.lathe(p, 'Dark', (gx + 0.2, 0.0, GUN_MUZZLE[2]), 'x', [(0.0, 0.0), (0.085, 0.0), (0.095, 0.02), (0.095, 0.5), (0.085, 0.52),
                                                             (0.0, 0.52)], seg=14)
    for k in range(5):
        vp.lathe(p, 'Metal', (gx + 0.28 + k * 0.1, 0.0, GUN_MUZZLE[2]), 'x', [(0.095, 0.0), (0.102, 0.008), (0.102, 0.03), (0.095, 0.038)],
                 seg=14, closed=True)
    vp.lathe(p, 'Dark', (GUN_MUZZLE[0] - 0.12, 0.0, GUN_MUZZLE[2]), 'x', [(0.03, 0.0), (0.062, 0.0), (0.062, 0.1), (0.03, 0.1)], seg=12,
             closed=True)


def nacelles(p):
    """As naceles nos ombros: o corpo liso, a boca de ar funda (o duto escuro), o topo na cor do jogador, a carenagem até o corpo e o
    escape "buraco negro" virado pra fora e pra trás."""
    for s in (-1, 1):
        y = s * NACELLE_Y
        N = ap.Section([(0.82, 0.44, 0.44, NACELLE_Z, 2.2, y), (0.6, 0.7, 0.72, NACELLE_Z, 2.2, y), (0.1, 0.78, 0.78, NACELLE_Z, 2.2, y),
                        (-1.0, 0.76, 0.76, NACELLE_Z, 2.2, y), (-1.6, 0.66, 0.66, NACELLE_Z, 2.2, y), (-1.95, 0.5, 0.52, NACELLE_Z + 0.02, 2.4, y)],
                       smooth=4)
        ths = ap.angles(24)
        ap.skin(p, 'Paint#nac', N, [r[0] for r in N.rows], ths, grooves=[(-0.25, GW, GD)], caps=(False, True))
        # a boca: o copo escuro com o lábio de 5 cm e o cone do separador de partículas
        mouth = N.ring(0.82, ths)
        inner = [(q[0] - 0.02, y + (q[1] - y) * 0.76, NACELLE_Z + (q[2] - NACELLE_Z) * 0.76) for q in mouth]
        ap.duct(p, 'Dark#duct', [(q[0] - 0.004, q[1], q[2]) for q in mouth], inner, (-1, 0, 0), 0.55)
        vp.lathe(p, 'Metal', (0.78, y, NACELLE_Z), 'x', [(0.0, -0.25), (0.1, -0.25), (0.08, -0.05), (0.0, 0.02)], seg=14)
        # o topo azul (lê de cima) e a junta
        ap.sec_patch(p, 'Team', N, 0.3, -1.25, deg(62), deg(118), nx=8, nt=6)
        # as grelhas da carenagem de lado (as aletas escuras)
        for k in range(4):
            th = deg(-8) if s > 0 else math.pi + deg(8)
            q = N.P(-0.55 - k * 0.14, th)
            vp.rbox(p, 'Dark', tuple(q + N.N(-0.55 - k * 0.14, th) * 0.004), (0.06, 0.02, 0.26), r=0.008, k=1)
        # a carenagem lisa da nacele até o corpo
        rings = []
        for x, hh in ((0.45, 0.16), (0.0, 0.34), (-1.1, 0.34), (-1.5, 0.18)):
            rr = []
            for u, v in vp.rounded_rect(abs(y) - 0.3, hh * 2, 0.1, 3):
                rr.append((x, s * (0.3 + (abs(y) - 0.3) / 2) + u * s, NACELLE_Z - 0.06 + v))
            rings.append(rr)
        if s < 0:
            rings = [r[::-1] for r in rings]
        ap.sheet(p, 'Paint#pyl', rings, closed=True, smooth=True, caps=(True, True))
        # o escape "buraco negro": o duto de quina redonda virado pra fora e pra trás, com o fundo escuro
        d = Vector((-0.8, s * 0.6, 0.0)).normalized()
        side = Vector((0.0, 0.0, 1.0)).cross(d).normalized()
        base = Vector((-1.7, s * (NACELLE_Y + 0.05), NACELLE_Z + 0.05))
        secs = []
        for t, ww, hh in ((0.0, 0.32, 0.36), (0.35, 0.34, 0.38), (0.62, 0.36, 0.4)):
            c = base + d * t
            secs.append([tuple(c + side * u + Vector((0, 0, 1)) * v) for u, v in vp.rounded_rect(ww, hh, 0.09, 3)])
        ap.sheet(p, 'Paint#exh', secs, closed=True, smooth=True, caps=(True, False))
        o = [Vector(q) for q in secs[-1]]
        c = base + d * 0.62
        inner = [tuple(c + (q - c) * 0.78 - d * 0.01) for q in o]
        ap.duct(p, 'Dark#exh', [tuple(q - d * 0.004) for q in o], inner, -d, 0.45)


def hump_mast_radar(p):
    """A corcunda da transmissão com as grelhas, o mastro com o prato oscilante, o bloqueador de infravermelho atrás e o radar
    Longbow torneado em cima do cubo."""
    rings = []
    for x, w, h in ((1.1, 0.4, 0.05), (0.6, 0.9, 0.42), (-0.5, 1.0, 0.5), (-1.6, 0.7, 0.32), (-2.4, 0.3, 0.06)):
        hh, zc = SEC.at(x)[2], SEC.at(x)[3]
        r = []
        for k in range(17):
            t = math.pi * k / 16
            c, s = math.cos(t), math.sin(t)
            r.append((x, (w / 2) * math.copysign(abs(c) ** (2 / 2.6), c), zc + hh / 2 - 0.08 + h * math.copysign(abs(s) ** (2 / 2.6), s)))
        rings.append(r)
    dense = ap.catmull([tuple(c for q in r for c in q) for r in rings], 3)        # lisa no comprimento
    ap.sheet(p, 'Paint#hump', [[tuple(row[3 * k:3 * k + 3]) for k in range(17)] for row in dense], closed=True, smooth=True,
             caps=(True, True))
    # as grelhas dos lados da corcunda (as aletas)
    for s in (-1, 1):
        for k in range(6):
            x = 0.25 - k * 0.12
            hh, zc = SEC.at(x)[2], SEC.at(x)[3]
            vp.rbox(p, 'Dark', (x, s * 0.47, zc + hh / 2 + 0.18), (0.05, 0.03, 0.2), roll=s * 30, r=0.01, k=1)
    _, _, hh, zc, _, _ = SEC.at(0.0)
    top = zc + hh / 2 + 0.38
    mz = ROTOR_AT[2]
    vp.lathe(p, 'Paint', (0.0, 0.0, top - 0.05), 'z', [(0.0, 0.0), (0.32, 0.0), (0.3, 0.08), (0.22, 0.14), (0.2, mz - top - 0.55),
                                                     (0.0, mz - top - 0.55)], seg=20)
    vp.lathe(p, 'Metal', (0.0, 0.0, top - 0.05), 'z', [(0.0, mz - top - 0.6), (0.15, mz - top - 0.6), (0.15, mz - top + 0.05),
                                                     (0.0, mz - top + 0.05)], seg=14)
    # o prato oscilante (parado no corpo; as bielas giram com o rotor)
    vp.lathe(p, 'Metal', (0.0, 0.0, mz - 0.52), 'z', [(0.16, 0.0), (0.42, 0.0), (0.44, 0.03), (0.42, 0.07), (0.16, 0.07)], seg=24, closed=True)
    # o bloqueador de infravermelho (o "farol" do ALQ-144) atrás do mastro
    vp.lathe(p, 'Dark', (-1.05, 0.0, SEC.at(-1.05)[3] + SEC.at(-1.05)[2] / 2 + 0.3), 'z',
             [(0.0, 0.0), (0.13, 0.0), (0.13, 0.04), (0.11, 0.06), (0.11, 0.26), (0.08, 0.3), (0.0, 0.31)], seg=12)
    # o radar Longbow: o mastro curto, o domo achatado e o topo azul
    rx, ry, rz = RADAR_AT
    vp.lathe(p, 'Metal', (rx, ry, mz + 0.18), 'z', [(0.0, 0.0), (0.1, 0.0), (0.1, rz - mz - 0.3), (0.0, rz - mz - 0.3)], seg=12)
    vp.lathe(p, 'Paint', (rx, ry, rz - 0.16), 'z', [(0.0, 0.0), (0.44, 0.0), (0.56, 0.04), (0.61, 0.12), (0.61, 0.22), (0.57, 0.32),
                                                 (0.46, 0.39), (0.24, 0.43), (0.0, 0.44)], seg=32)
    vp.lathe(p, 'Team', (rx, ry, rz + 0.235), 'z', [(0.0, 0.0), (0.36, 0.0), (0.38, 0.012), (0.36, 0.03), (0.0, 0.04)], seg=32)
    vp.lathe(p, 'Dark', (rx, ry, rz - 0.04), 'z', [(0.608, 0.0), (0.618, 0.0), (0.618, 0.03), (0.608, 0.03)], seg=32, closed=True)


def wings(p):
    W = ap.Wing([(0.6, 0.5, WING_Z, 1.2, 0.17), (0.48, 2.6, WING_Z - 0.06, 0.95, 0.1)])
    W.build(p, 'Paint#stub', sides=(1, -1))
    Wt = ap.Wing([(0.5, 1.6, WING_Z + 0.004, 0.92, 0.155), (0.45, 2.45, WING_Z - 0.05, 0.87, 0.115)])
    Wt.build(p, 'Team#stub', sides=(1, -1))
    W.chord_line(p, 'Dark', 0.78, 0.6, 2.5, SW, sides=(1, -1), n=5)
    for s in (-1, 1):
        vp.lathe(p, 'Light', (0.1, s * 2.63, WING_Z - 0.06), (1, 0, 0), [(0.0, -0.1), (0.035, -0.08), (0.045, 0.0), (0.03, 0.08), (0.0, 0.1)],
                 seg=10)
        # os pilones de quina redonda; o lançador M299 dos Hellfires com os 2 trilhos
        for y in (PYLON_IN, PYLON_OUT):
            vp.rbox(p, 'Paint#plate', (0.08, s * y, WING_Z - 0.18), (0.82, 0.12, 0.3), r=0.04, k=3)
        y = s * PYLON_OUT
        vp.rbox(p, 'Dark#plate', ((HELLFIRE_X[1] + HELLFIRE_X[0] - 0.35) / 2, y, HELLFIRE_Z + HELLFIRE_R + 0.045),
                (HELLFIRE_X[0] - HELLFIRE_X[1] - 0.4, 0.6, 0.08), r=0.05, k=3)
        for dy in (-HELLFIRE_DY, HELLFIRE_DY):
            vp.rbox(p, 'Dark', ((HELLFIRE_X[1] + HELLFIRE_X[0] - 0.35) / 2, y + dy, HELLFIRE_Z + HELLFIRE_R - 0.0),
                    (HELLFIRE_X[0] - HELLFIRE_X[1] - 0.5, 0.06, 0.04), r=0.015, k=1)


def tail(p):
    """A deriva inclinada pra trás com a caixa do rotor de cauda, o estabilizador, as luzes e a bequilha."""
    fx, fy, fz = FAN_AT
    Fn = ap.Wing([(-8.55, 0.0, 1.95, 1.65, 0.2), (-8.95, 0.0, 3.1, 1.32, 0.17), (-9.7, 0.0, 4.35, 1.0, 0.14)], n=(0, 1, 0))
    Fn.build(p, 'Paint#fin', sides=(1,))
    # a caixa de engrenagem do rotor de cauda (torneada) e o eixo
    vp.lathe(p, 'Paint', (fx, 0.06, fz), (0, -1, 0), [(0.0, 0.0), (0.24, 0.0), (0.26, 0.04), (0.22, 0.16), (0.14, 0.24), (0.0, 0.26)], seg=18)
    vp.lathe(p, 'Metal', (fx, -0.2, fz), (0, -1, 0), [(0.0, 0.0), (0.09, 0.0), (0.09, abs(fy) - 0.22), (0.0, abs(fy) - 0.22)], seg=12)
    # o estabilizador e as luzes
    S = ap.Wing([(-9.0, 0.0, 1.88, 1.05, 0.11), (-9.25, 1.68, 1.88, 0.75, 0.07)])
    S.build(p, 'Paint#stab', sides=(1, -1))
    S.chord_line(p, 'Dark', 0.72, 0.4, 1.6, SW, sides=(1, -1), n=4)
    vp.lathe(p, 'Light', (-9.72, 0.0, 4.36), 'z', [(0.0, 0.0), (0.04, 0.0), (0.04, 0.04), (0.0, 0.06)], seg=10)
    vp.lathe(p, 'Light', (-10.38, 0.0, 1.86), (-1, 0, 0), [(0.0, 0.0), (0.04, 0.0), (0.04, 0.03), (0.0, 0.05)], seg=10)
    # a bequilha: o garfo, o amortecedor e o pneu
    ap.oleo(p, (-9.15, 0.0, 1.66), (-9.3, 0.0, 0.26), 0.05, m='Metal', F=1.0)
    for s in (-1, 1):
        vp.tube(p, 'Metal', [(-9.24, s * 0.05, 0.6), (-9.3, s * 0.1, 0.24)], 0.025, seg=6)
    ap.air_wheel(p, (-9.3, 0.0, 0.24), 0.24, 0.13, side=1, rubber='Rubber', rim='Metal', hub='Metal', seg=20)


def main_gear(p):
    """O trem principal do Apache: o braço que arrasta, o amortecedor inclinado, o pneu de avião."""
    for s in (-1, 1):
        ap.oleo(p, (2.62, s * 0.5, 1.12), (2.55, s * 0.95, 0.42), 0.075, m='Metal', F=1.0)
        vp.tube(p, 'Metal', [(1.9, s * 0.52, 1.0), (2.2, s * 0.75, 0.7), (2.5, s * 0.98, 0.42)], 0.045, seg=8)
        vp.lathe(p, 'Metal', (2.55, s * 0.95, 0.38), (0, s, 0), [(0.0, -0.02), (0.05, -0.02), (0.05, 0.1), (0.0, 0.1)], seg=10)
        ap.air_wheel(p, (2.55, s * 1.04, 0.38), 0.38, 0.2, side=s, rubber='Rubber', rim='Metal', hub='Metal', seg=24)


def body():
    p = Part(PREFIX + '_Body')
    p.bm('Paint#plate', 0.03)
    p.bm('Dark#plate', 0.02)
    fuselage(p)
    cockpit(p)
    sensors_and_gun(p)
    nacelles(p)
    hump_mast_radar(p)
    wings(p)
    tail(p)
    main_gear(p)
    return p.finish()


# ---------------------------------------------------------------------------------- os rotores (origem no cubo)
def main_rotor():
    """O rotor de 4 pás: o cubo torneado, os punhos (os pacotes de lâminas), as bielas de passo e as pás que afinam, torcem e têm a
    ponta enflechada amarela."""
    p = Part(PREFIX + '_Rotor')
    p.bm('Metal#plate', 0.02)
    vp.lathe(p, 'Metal', (0.0, 0.0, -0.2), 'z', [(0.0, 0.0), (0.3, 0.0), (0.42, 0.06), (0.45, 0.16), (0.42, 0.26), (0.32, 0.32),
                                               (0.2, 0.36), (0.12, 0.5), (0.0, 0.52)], seg=24)
    r0 = 0.82
    for k in range(ROTOR_BLADES):
        a = math.radians(45.0 + 360.0 * k / ROTOR_BLADES)
        d = Vector((math.cos(a), math.sin(a), 0.0))
        side = Vector((-math.sin(a), math.cos(a), 0.0))
        # o punho: a caixa de quina redonda do cubo à raiz da pá, e o chifre de passo com a biela até o prato
        c = d * 0.62
        vp.rbox(p, 'Metal#plate', tuple(c), (0.44, 0.22, 0.16), yaw=math.degrees(a), r=0.06, k=3)
        horn = d * 0.55 + side * 0.18
        vp.tube(p, 'Metal', [horn + Vector((0, 0, -0.02)), horn * 0.85 + Vector((0, 0, -0.32))], 0.025, seg=6)
        ap.blade(p, 'Dark', (0.0, 0.0, 0.0), d, r0, ROTOR_R, ROTOR_CHORD, 0.085, twist=(9.0, -1.0), taper=0.62, sweep=0.22,
                 tip_m='Yellow', tip_from=1.0 - TIP_LEN / (ROTOR_R - r0), n=12, droop=0.09, root_chord=0.3)
    return p.finish()


def tail_fan():
    p = Part(PREFIX + '_TailFan')
    vp.lathe(p, 'Metal', (0.0, -0.13, 0.0), (0, 1, 0), [(0.0, 0.0), (0.12, 0.0), (0.17, 0.05), (0.17, 0.2), (0.12, 0.25), (0.0, 0.26)], seg=18)
    for k, ang in enumerate(FAN_ANGLES):
        a = math.radians(ang)
        d = Vector((math.cos(a), 0.0, math.sin(a)))
        y = 0.05 if k % 2 == 0 else -0.05
        ap.blade(p, 'Dark', (0.0, y, 0.0), d, 0.16, FAN_R, FAN_CHORD, 0.045, twist=(12.0, 4.0), taper=0.85, sweep=0.0,
                 tip_m='Yellow', tip_from=1.0 - 0.2 / (FAN_R - 0.16), n=6, axis=(0, 1, 0))
    return p.finish()


# ---------------------------------------------------------------------------------- casulos e mísseis
def pods():
    """Os casulos M261 (19 foguetes): o corpo torneado com as pontas redondas, a faixa azul, as 19 bocas fundas na frente."""
    out = []
    for k, s in enumerate((1, -1)):
        p = Part('pod_%03d' % (k + 1))
        x0, x1 = POD_X
        y = s * PYLON_IN
        L = x0 - x1
        vp.lathe(p, 'Paint', (x0, y, POD_Z), (-1, 0, 0), [(0.0, 0.03), (POD_R * 0.8, 0.03), (POD_R * 0.86, 0.0), (POD_R * 0.97, 0.02),
                                                        (POD_R, 0.07), (POD_R, L - 0.18), (POD_R * 0.9, L - 0.06), (POD_R * 0.6, L),
                                                        (0.0, L)], seg=24)
        vp.lathe(p, 'Team', (x1 + 0.38, y, POD_Z), 'x', [(POD_R + 0.002, 0.0), (POD_R + 0.006, 0.01), (POD_R + 0.006, 0.15),
                                                       (POD_R + 0.002, 0.16)], seg=24)
        for ring_r, n in ((0.0, 1), (0.075, 6), (0.145, 12)):
            for j in range(n):
                a = 2 * math.pi * j / n
                q = (x0 - 0.02, y + ring_r * math.cos(a), POD_Z + ring_r * math.sin(a))
                vp.lathe(p, 'Dark', q, (1, 0, 0), [(0.0, -0.05), (0.03, -0.05), (0.03, 0.0), (0.0, 0.0)], seg=8)
        # as cintas de segurar e a ferragem em cima
        for xx in (x0 - 0.35, x1 + 0.25):
            vp.lathe(p, 'Metal', (xx, y, POD_Z), 'x', [(POD_R + 0.002, 0.0), (POD_R + 0.01, 0.006), (POD_R + 0.01, 0.034), (POD_R + 0.002, 0.04)],
                     seg=24)
        out.append(p.finish())
    return out


def hellfires():
    out = []
    n = 0
    for s in (1, -1):
        for dy in (HELLFIRE_DY, -HELLFIRE_DY):
            n += 1
            p = Part('hellfire_%03d' % n)
            x0, x1 = HELLFIRE_X
            y = s * PYLON_OUT + dy
            ap.missile(p, (x0, y, HELLFIRE_Z), x0 - x1, HELLFIRE_R, body='Paint', nose='Paint', fin='Paint', band='Team',
                       nose_len=0.2, band_at=0.42, seeker='Lens', fins_mid=(0.3, 0.2, 0.12, 0.085, 0.012 * F),
                       fins_tail=(x0 - x1 - 0.2, 0.18, 0.12, 0.08, 0.012 * F), roll=45.0, seg=14, F=F)
            out.append(p.finish())
    return out


# ---------------------------------------------------------------------------------- exportação
def write_spec():
    import json
    k = SCALE
    parts = [[PREFIX + '_Body', [0, 0, 0]], [PREFIX + '_Rotor', [0, 0, round(ROTOR_AT[2] * k, 4)]],
             [PREFIX + '_TailFan', [round(FAN_AT[0] * k, 4), round(-FAN_AT[1] * k, 4), round(FAN_AT[2] * k, 4)]],
             [PREFIX + '_Pods', [0, 0, 0]], [PREFIX + '_Missiles', [0, 0, 0]]]
    views = [['top', 'ortho', [0, 0, -1], [1, 0, 0], [1200, 1400]],
             ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1400, 600]],
             ['side', 'ortho', [0, 1, 0], [0, 0, 1], [1600, 500]],
             ['three_quarter', 'persp', 30.0, 215.0, [1600, 1000], 35.0, 0.6],
             ['back_three_quarter', 'persp', 25.0, 35.0, [1600, 1000], 35.0, 0.6]]
    spec = {'parts': parts, 'views': views, 'usa_at': [9.0, -5.0, 0], 'tank_views': False,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 26.0, 0.75]]}
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)


def main():
    clear_scene()
    b = body()
    r = main_rotor()
    f = tail_fan()
    ps = pods()
    hs = hellfires()
    for o in [b, r, f] + ps + hs:
        o.data.transform(Matrix.Scale(SCALE, 4))
    # (como a lâmina do Dozer B) os rotores ficam NO LUGAR pro cozimento das máscaras (a sombra do rotor no corpo e do corpo no rotor)
    # e voltam pra origem do cubo só na hora de exportar
    detail = model_lib.DETAIL.get('defer') is not None
    moved = {r.name: V(*[c * SCALE for c in ROTOR_AT]), f.name: V(*[c * SCALE for c in FAN_AT])}
    if detail:
        r.data.transform(Matrix.Translation(moved[r.name]))
        f.data.transform(Matrix.Translation(moved[f.name]))
        _orig = model_lib.export

        def _export(o, out_dir):
            if o.name in moved and model_lib.DETAIL.get('defer') is None:
                o.data.transform(Matrix.Translation(-moved[o.name]))
            return _orig(o, out_dir)
        model_lib.export = _export
    total = (export(b, OUT) + export(r, OUT) + export(f, OUT) + export_group(ps, OUT, PREFIX + '_Pods')
             + export_group(hs, OUT, PREFIX + '_Missiles'))
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL tris por peca', {b.name: tris(b), r.name: tris(r), f.name: tris(f), 'pods': sum(tris(q) for q in ps),
                                  'hellfires': sum(tris(q) for q in hs)})
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([b])
    print('MODEL corpo: comprimento %.2f m, largura %.2f m, altura %.2f m (com o radar)' % (ax1 - ax0, ay1 - ay0, az1))
    k = SCALE
    print('MODEL rotor at (malha)', [round(c * k, 3) for c in ROTOR_AT], 'raio %.3f, corda %.3f, ponta %.3f' % (ROTOR_R * k, ROTOR_CHORD * k, TIP_LEN * k))
    print('MODEL tailfan at (malha)', [round(c * k, 3) for c in FAN_AT], 'raio %.3f' % (FAN_R * k))
    write_spec()


main()
