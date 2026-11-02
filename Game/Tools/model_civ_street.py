"""As peças de rua do kit civil v2 (01/11/2026; docs/KIT_CIVIL_V2.md): meio-fio, calçada, bueiro e faixa de pedestre como malha.

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_civ_street.py C:/Users/luisf/explosion/captures/game/models/civ_v2/street

A textura do asfalto do mapa é de outra sessão e está aprovada (docs/APROVADOS.md, Mundos, 17/10): nada aqui mexe nela. Estas peças
sentam EM CIMA dela (o meio-fio e a calçada sobem 15 cm do asfalto; a faixa é uma película de 4 mm) e servem às ruas da cidade
(city_modern_noon_gemini: o meio-fio, a calçada de placas, a faixa de pedestre gasta, o bueiro; 16_beauty_pass/b_gpt: a calçada
quebrada da vila). Convenção (o kit civil): o comprido ao longo de X, a RUA do lado +Y, a origem no centro do pé e no nível do asfalto.

  SM_civ_curb          o meio-fio reto de 2 m (2 blocos de 1 m com a junta): a face da rua inclinada, a quina de cima redonda (3 cm),
                       a sarjeta de concreto rente ao asfalto, enterrado até −0,25 m;
  SM_civ_curb_corner   a curva do meio-fio (o raio de 1,5 m, 90°), com a sarjeta;
  SM_civ_sidewalk      a calçada de 2 × 2 m: as placas de 40 cm com o chanfro e a junta, umas soltas e quebradas, o meio-fio no lado da rua;
  SM_civ_drain         o bueiro: o meio-fio com a boca de lobo (o vão escuro) e a grelha de ferro na sarjeta;
  SM_civ_crosswalk     a faixa de pedestre: 6 listras de 0,5 × 3 m, GASTAS (a borda comida pela isolinha do ruído, os buracos onde
                       o pneu passa), 4 mm acima do asfalto.
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import Part, V, clear_scene, export  # noqa: E402
import model_civ_parts as cp  # noqa: E402
from model_building_parts import bake_in_place, hexa  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'civ_v2', 'street')
os.makedirs(OUT, exist_ok=True)

CH = 0.15           # a altura do meio-fio sobre o asfalto
# o perfil do meio-fio (y pra rua, z): o pé enterrado, a face da rua inclinada, a quina redonda, o topo, a face da calçada
CURB = [(-0.15, -0.25), (0.02, -0.25), (0.02, 0.0), (0.0, CH - 0.035), (-0.004, CH - 0.015), (-0.015, CH - 0.002), (-0.035, CH),
        (-0.135, CH), (-0.15, CH - 0.01)]


def curb_run(p, x0, x1, y=0.0, m='Concrete#curb', gutter=True):
    rings = [[(x, y + a, z) for (a, z) in CURB] for x in (x0, x1)]
    p.loft(m, rings, bevel=0.0, smooth=False)
    if gutter:
        p.box('Concrete#gutter', x0, x1, y + 0.02, y + 0.32, -0.08, 0.012, bevel=0.0)


def curb():
    p = Part('SM_civ_curb')
    curb_run(p, -1.0, -0.006)
    curb_run(p, 0.006, 1.0)
    return p.finish()


def curb_corner(R=1.5, n=12):
    """A curva: a rua do lado de fora (o raio R até a face da rua), o centro da curva em (−R, −R) (o canto da calçada)."""
    p = Part('SM_civ_curb_corner')
    C = Vector((-R, -R, 0.0))
    for k in range(n):
        a0, a1 = math.radians(90 * k / n), math.radians(90 * (k + 1) / n)
        rings = []
        for a in (a0 + 0.004, a1 - 0.004):
            d = Vector((math.cos(a), math.sin(a), 0.0))
            rings.append([tuple(C + d * (R + aa) + Vector((0, 0, z))) for (aa, z) in CURB])
        p.loft('Concrete#curb', rings, bevel=0.0, smooth=False)
        g = []
        for a in (a0, a1):
            d = Vector((math.cos(a), math.sin(a), 0.0))
            g.append([tuple(C + d * (R + aa) + Vector((0, 0, z))) for (aa, z) in ((0.02, -0.08), (0.32, -0.08), (0.32, 0.012), (0.02, 0.012))])
        p.loft('Concrete#gutter', g, bevel=0.0)
    return p.finish()


def sidewalk():
    """A calçada de 2 × 2 m (de y −1 até o meio-fio em y +0,85): as placas de 40 cm com o chanfro (lê o quadriculado com a luz no
    chanfro), umas afundadas, uma quebrada em 2, uma faltando (o escuro da base aparece)."""
    p = Part('SM_civ_sidewalk')
    p.bm('Concrete#tile', 0.012)
    curb_run(p, -1.0, -0.006, y=0.85)
    curb_run(p, 0.006, 1.0, y=0.85)
    p.box('Concrete#base', -1.0, 1.0, -1.0, 0.70, -0.25, CH - 0.06, bevel=0.0)
    s = 0.4
    for i in range(5):
        for j in range(4):
            x0, y0 = -1.0 + i * s, -1.0 + j * s
            x1, y1 = x0 + s, min(y0 + s, 0.70)
            k = i * 7 + j
            if k == 11:
                continue                                            # a placa que falta
            dz = -0.012 * cp.h3(k, 3, 5) if cp.h3(k, 3, 7) > 0.6 else 0.0
            tilt = (cp.h3(k, 3, 9) - 0.5) * 2.5 if dz else 0.0
            if k == 6:
                # a quebrada: 2 pedaços com a rachadura torta
                hexa(p, 'Concrete#tile', [(x0 + 0.004, y0 + 0.004, CH - 0.06), (x1 - 0.004, y0 + 0.004, CH - 0.06),
                                          (x0 + 0.004, y0 + 0.18, CH - 0.06), (x1 - 0.004, y0 + 0.24, CH - 0.06),
                                          (x0 + 0.004, y0 + 0.004, CH - 0.004), (x1 - 0.004, y0 + 0.004, CH - 0.004),
                                          (x0 + 0.004, y0 + 0.18, CH - 0.004), (x1 - 0.004, y0 + 0.24, CH - 0.004)])
                hexa(p, 'Concrete#tile', [(x0 + 0.004, y0 + 0.195, CH - 0.07), (x1 - 0.004, y0 + 0.255, CH - 0.07),
                                          (x0 + 0.004, y1 - 0.004, CH - 0.07), (x1 - 0.004, y1 - 0.004, CH - 0.07),
                                          (x0 + 0.004, y0 + 0.195, CH - 0.018), (x1 - 0.004, y0 + 0.255, CH - 0.012),
                                          (x0 + 0.004, y1 - 0.004, CH - 0.02), (x1 - 0.004, y1 - 0.004, CH - 0.014)])
                continue
            vp.rbox(p, 'Concrete#tile', ((x0 + x1) / 2, (y0 + y1) / 2, CH - 0.03 + dz), (s - 0.008, y1 - y0 - 0.008, 0.055), roll=tilt,
                    r=0.015, k=2)
    return p.finish()


def drain():
    """O bueiro: 1 m de meio-fio com a boca de lobo (o vão escuro na face da rua, a tampa de concreto por cima) e a grelha de ferro
    na sarjeta (as barras de 3 cm com o vão escuro embaixo)."""
    p = Part('SM_civ_drain')
    curb_run(p, -1.0, -0.5, gutter=False)
    curb_run(p, 0.5, 1.0, gutter=False)
    # a boca: o meio-fio mais alto por cima do vão (a viga), o escuro do vão
    p.box('Concrete#curb', -0.5, 0.5, -0.15, 0.02, 0.085, CH)
    p.box('Dark#hole', -0.48, 0.48, -0.14, 0.015, -0.1, 0.085, bevel=0.0)
    p.box('Concrete#gutter', -1.0, -0.47, 0.02, 0.32, -0.08, 0.012, bevel=0.0)
    p.box('Concrete#gutter', 0.47, 1.0, 0.02, 0.32, -0.08, 0.012, bevel=0.0)
    # a grelha: o quadro de cantoneira e as barras (no plano do asfalto)
    p.box('Dark#hole', -0.45, 0.45, 0.04, 0.48, -0.25, -0.02, bevel=0.0)
    for (a, b) in (((-0.47, 0.03), (0.47, 0.05)), ((-0.47, 0.47), (0.47, 0.5)), ((-0.47, 0.03), (-0.44, 0.5)), ((0.44, 0.03), (0.47, 0.5))):
        p.box('Metal#grate', a[0], b[0], a[1], b[1], -0.02, 0.008, bevel=0.0)
    for k in range(11):
        x = -0.4 + 0.8 * k / 10
        vp.rbox(p, 'Metal#grate', (x, 0.265, -0.005), (0.03, 0.43, 0.026), r=0.006, k=1)
    return p.finish()


def crosswalk():
    """A faixa de pedestre gasta: 6 listras de 0,5 × 3 m (o comprido ao longo de X: a rua corre em X), a película de 4 mm; a tinta
    comida pela isolinha do ruído (mais gasta no meio de cada faixa de rodagem, onde o pneu passa)."""
    p = Part('SM_civ_crosswalk')
    field = (lambda c: cp.fbm3(c.x * 2.2, c.y * 2.2, 0.0, 9) + 0.28 * math.exp(-((abs(c.y) - 1.4) / 0.5) ** 2))
    t = 0.004
    for s in range(6):
        y0 = -2.75 + s * 1.0
        nx, ny = 15, 3
        for i in range(nx):
            for j in range(ny):
                xa, xb = -1.5 + 3.0 * i / nx, -1.5 + 3.0 * (i + 1) / nx
                ya, yb = y0 + 0.5 * j / ny, y0 + 0.5 * (j + 1) / ny
                quad = [Vector((xa, ya, 0.0)), Vector((xb, ya, 0.0)), Vector((xb, yb, 0.0)), Vector((xa, yb, 0.0))]
                vals = [field(q) for q in quad]
                keep = cp.clip_poly(quad, vals, 0.66, False)
                if len(keep) < 3:
                    continue
                b = p.bm('Paint#stripe', 0.0)
                top = [b.verts.new(V(q.x, q.y, t)) for q in keep]
                bot = [b.verts.new(V(q.x, q.y, -0.002)) for q in keep]
                b.faces.new(top)
                b.faces.new(list(reversed(bot)))
                for k in range(len(keep)):
                    kk = (k + 1) % len(keep)
                    b.faces.new([top[k], bot[k], bot[kk], top[kk]])
    return p.finish()


clear_scene()
ALL = [curb, curb_corner, sidewalk, drain, crosswalk]
objs = []
for i, fn in enumerate(ALL):
    o = fn()
    bake_in_place(o, ((i % 3) * 8.0, (i // 3) * 8.0, 0.0))
    print('CIV', o.name, cp.tris(o), 'tris (antes da grade)')
    objs.append(o)
for o in objs:
    export(o, OUT)
