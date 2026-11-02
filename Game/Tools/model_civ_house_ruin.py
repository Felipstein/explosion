"""A casa em ruína (kit civil v2, 01/11/2026; docs/KIT_CIVIL_V2.md): peça NOVA, a 3ª coisa que mais aparece nas vilas dos game shots.

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_civ_house_ruin.py C:/Users/luisf/explosion/captures/game/models/civ_v3/house_ruin

O que é (docs/concept/towns/_variacoes_novas.jpg, a fileira 3 "casa em ruínas"; 16_beauty_pass/b_gpt em cima à direita: o sobrado com
o canto caído e as lajes à mostra; 20_beauty_new_tanks/usa_china_gpt, à direita embaixo; o alvo "a cara do jogo" com as casas
abertas): a casa térrea do kit com o CANTO DA FRENTE-DIREITA desmoronado: as paredes descem em degraus quebrados até ~0,7 m no canto (o
topo cru do miolo, o reboco lascado), a laje do teto partida na diagonal (a metade de trás ainda no lugar com o ferro saindo da
quebra; a da frente caída por dentro, apoiada no entulho), o monte de entulho (os blocos e as lascas de laje) vazando pela quina até a
rua, a caixa d'água tombada em cima dele, o parapeito só no que ficou de pé, a parede de dentro partida, as janelas sem vidro.

Saída: SM_civ_house_ruin.glb (a pegada 8 × 8 m da casa térrea; a frente em +Y, origem no centro e no chão, a saia até −0,4 m). Serve de
estado "destruído" da civ_house_1story (a mesma planta e os mesmos vãos).

v3 (01/11 noite, "quase, muitas quinas, parece lego, sem detalhes, sem texturas"): as peças novas da casa nas paredes que ficaram de
pé (os pilares de canto gastos, o rodapé ondulando, o reboco caído com o tijolo, a mureta comida no que sobrou do parapeito), as
janelas sem vidro de casa velha, a parede tombando mais que a da casa inteira; o escorrido e o pé úmido na cor de vértice.
"""
import math
import os
import sys

from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import Part, V, clear_scene, export  # noqa: E402
import model_civ_house_parts as hp  # noqa: E402
import model_civ_parts as cp  # noqa: E402
from model_building_parts import WallFace, rect_minus, hexa, EPS  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'civ_v3', 'house_ruin')
os.makedirs(OUT, exist_ok=True)

X0, X1, Y0, Y1 = -3.7, 3.7, -3.7, 3.7
ZR = 3.35
T = 0.25
CORNER = Vector((X1, Y1, 0.0))            # o canto que caiu
REACH = 4.2                               # até onde a queda alcança ao longo das paredes


def top_at(x, y, seed):
    """A altura do topo da parede em (x, y): inteira longe do canto; descendo em degraus quebrados até ~0,7 m nele."""
    d = (Vector((x, y, 0.0)) - CORNER).length
    drop = max(0.0, 1.0 - d / REACH) ** 1.25 * 2.7
    jag = (cp.fbm3(x * 2.6, y * 2.6, 0.3, seed) - 0.5) * 0.55 * min(1.0, drop / 0.4 + 0.15)
    return ZR - drop + (jag if drop > 0.05 else 0.0)


def wall_pieces(p, f, at, holes, seed):
    """As paredes da ruína: cada trecho entre os vãos inteiro se não foi tocado; senão UMA peça só com o topo serrilhado contínuo (o
    contorno da quebra extrudado na espessura: sem junta entre faixas), e por cima a lasca do miolo cru seguindo o contorno."""
    F = WallFace(f, at, 0.0)
    u0, u1 = ((Y0 + T - EPS, Y1 - T + EPS) if f[1] == 'x' else (X0 + T - EPS, X1 - T + EPS))
    eu = Vector(F.pt(1.0, 0.0, 0.0)) - Vector(F.pt(0.0, 0.0, 0.0))
    ev = Vector((0.0, 0.0, 1.0))
    en = Vector(F.normal())
    O = Vector(F.pt(0.0, 0.0, -T))
    if f in ('-x', '-y'):
        hp.wall_grid(p, 'Plaster', F, u0, u1, 0.0, ZR, T, holes)       # a face que não caiu: inteira, a quina gasta
    else:
        for (a, b, c, d, lo, hi) in rect_minus(u0, u1, 0.0, ZR, holes):
            n = max(2, int((b - a) / 0.18))
            us = [a + (b - a) * k / n for k in range(n + 1)]
            tops = []
            for u in us:
                q = F.pt(u, 0.0)
                tops.append(min(d, top_at(q[0], q[1], seed)))
            if max(tops) <= c + 0.08:
                continue
            tops = [max(c + 0.06, t) for t in tops]
            pts = [(a, c), (b, c)] + [(u, t) for (u, t) in zip(reversed(us), reversed(tops))]
            # a face que caiu: as peças com a quina VIVA (a quebra fresca; o chanfro abria a junta entre elas)
            vp.prism(p, 'Plaster#broken', pts, O, eu, ev, en, T)
            if min(tops) < d - 0.01:
                band = [(u, t + 0.006) for (u, t) in zip(us, tops)] + [(u, t - 0.035) for (u, t) in zip(reversed(us), reversed(tops))]
                vp.prism(p, 'BrokenAdobe#top', band, O + en * 0.03, eu, ev, en, T - 0.06)
    # os pilares de canto: o que caiu some, os outros ficam (a quina de fora redonda)
    k = 0
    for (xa, xb, cx) in ((X0, X0 + T, X0), (X1 - T, X1, X1)):
        for (ya, yb, cy) in ((Y0, Y0 + T, Y0), (Y1 - T, Y1, Y1)):
            h = top_at((xa + xb) / 2, (ya + yb) / 2, seed)
            k += 1
            if h >= ZR - 0.01 and f == '-x':
                hp.corner_post(p, xa, xb, ya, yb, (cx, cy), 0.0, ZR, seed=50 + k)


def mound(p, cx, cy, R, H, seed, m='BrokenAdobe#mound', clip=3.95):
    """O monte de pó grosso e caliça debaixo dos blocos (o entulho assenta num monte, não em caixas soltas): a grade polar com o raio
    e a altura ruidosos, a borda morrendo no chão."""
    na, nr = 20, 6
    rings = []
    for i in range(nr + 1):
        rr = i / nr
        ring = []
        for j in range(na):
            a = 2 * math.pi * j / na
            Rj = R * (0.75 + 0.45 * cp.h3(j, seed, 3))
            x = cx + Rj * rr * math.cos(a)
            y = cy + Rj * rr * math.sin(a)
            x, y = max(-clip, min(clip, x)), max(-clip, min(clip, y))
            z = H * (1 - rr ** 1.6) * (0.8 + 0.4 * cp.h3(i * 31 + j, seed, 5)) - 0.03
            ring.append((x, y, max(-0.03, z)))
        rings.append(ring)
    b = p.bm(m, 0.0)
    vs = [[b.verts.new(V(*q)) for q in ring] for ring in rings]
    top = b.verts.new(V(cx, cy, H))
    for j in range(na):
        b.faces.new([top, vs[0][j], vs[0][(j + 1) % na]]).smooth = True
    for i in range(nr):
        for j in range(na):
            jj = (j + 1) % na
            b.faces.new([vs[i][j], vs[i + 1][j], vs[i + 1][jj], vs[i][jj]]).smooth = True


def build():
    seed = 5
    p = hp.new_part('SM_civ_house_ruin', seed=5, dirt=0.95)
    p.bm('Plaster#broken', 0.0)
    p.bm('BrokenAdobe#top', 0.0)
    holes = {
        '+y': [(-1.25, -0.25, 0.0, 2.15), (0.85, 2.15, 1.0, 2.2), (-3.05, -2.25, 1.15, 2.05)],
        '+x': [(-2.0, -0.9, 1.1, 2.1), (0.9, 2.0, 1.1, 2.1)],
        '-x': [(-0.5, 0.5, 1.2, 2.05)],
        '-y': [(1.4, 2.1, 1.5, 2.1), (-2.2, -1.2, 1.15, 2.05)],
    }
    at = {'+x': X1, '-x': X0, '+y': Y1, '-y': Y0}
    for f in at:
        wall_pieces(p, f, at[f], holes[f], seed)
    p.bm('BrokenAdobe#mound', 0.0)
    # a saia, o piso, o rodapé (só onde a parede ficou)
    p.box('Plaster#skirt', X0 - 0.05, X1 + 0.05, Y0 - 0.05, Y1 + 0.05, -0.4, 0.02, bevel=0.0)
    p.box('Plaster#slab', X0 + T - 0.03, X1 - T + 0.03, Y0 + T - 0.03, Y1 - T + 0.03, -0.05, 0.08, bevel=0.0)
    for f in at:
        F = WallFace(f, at[f], 0.0)
        u0, u1 = ((Y0 - 0.03, Y1 + 0.03) if f[1] == 'x' else (X0 - 0.03, X1 + 0.03))
        u0, u1 = ((Y0 + T, Y1 - T) if f[1] == 'x' else (X0 + T, X1 - T))
        segs = [(u0, u1)] if f in ('-x', '-y') else ([(u0, -0.2)] if f == '+x' else [(u0, -1.3)])
        for k, (a, b) in enumerate(segs):
            hp.plinth_run(p, F, a, b, 0.32, seed=60 + k + ord(f[1]), wear=1.2)
        hp.dirt_wall(F, ZR + 0.7 if f in ('-x', '-y') else None)
        if f in ('-x', '-y'):
            hp.wall_spalls(p, F, u0, u1, 0.25, ZR, 4, seed=70 + ord(f[1]), holes=holes[f])
    # as janelas (sem vidro: o vão escuro e o caixilho que sobrou) e a porta
    for f, hs in holes.items():
        F = WallFace(f, at[f], 0.0)
        for (u0, u1, z0, z1) in hs:
            pa = F.pt((u0 + u1) / 2, 0.0)
            if z0 < 0.1 or top_at(pa[0], pa[1], seed) < z1 + 0.25:
                continue
            hp.window(p, F, u0, u1, z0, z1, t=T, kind='dark', seed=3 + int(u0 * 7) + ord(f[1]), wear=1.3)
    hp.steel_door(p, WallFace('+y', Y1, 0.0), -1.25, -0.25, 0.0, 2.15, t=T, canopy=False)
    # os buracos de tiro (as rajadas no reboco das faces de pé) e os remendos
    for k, f in enumerate(('-x', '-y', '+y', '+x')):
        F = WallFace(f, at[f], 0.0)
        u0, u1 = ((Y0, Y1) if f[1] == 'x' else (X0, X1))
        zmax = ZR if f in ('-x', '-y') else 1.0
        hp.pockmarks(p, F, u0, u1 if f in ('-x', '-y') else -0.6, 0.3, zmax + 1.0 if f in ('+y', '+x') else zmax, 24, seed=20 + k,
                     holes=holes[f])
    # ---- a laje: a metade de trás no lugar (a quebra na diagonal, serrilhada), o ferro saindo da quebra
    xa, xb, ya, yb = X0 + T - 0.03, X1 - T + 0.03, Y0 + T - 0.03, Y1 - T + 0.03
    brk = []
    n = 16
    A, B = Vector((-0.6, yb)), Vector((xb, -0.9))
    for k in range(n + 1):
        u = k / n
        q = A + (B - A) * u
        nrm = Vector((1, 1)).normalized()
        q = q + nrm * ((cp.h3(k, seed, 9) - 0.5) * 0.35 if 0 < k < n else 0.0)
        brk.append((q.x, q.y))
    keep = [(xa, ya), (xb, ya), (xb, -0.9)] + list(reversed(brk[1:-1])) + [(-0.6, yb), (xa, yb)]
    vp.prism(p, 'Plaster#slab', keep, Vector((0, 0, ZR - 0.22)), Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1)), 0.215)
    for k in range(1, n):
        x, y = brk[k]
        d = Vector((1, 1, -0.6 - 0.8 * cp.h3(k, seed, 11))).normalized()
        L = 0.25 + 0.35 * cp.h3(k, seed, 13)
        bend = Vector((cp.h3(k, seed, 15) - 0.5, cp.h3(k, seed, 17) - 0.5, -0.5)) * 0.3
        a = Vector((x, y, ZR - 0.11)) - Vector((1, 1, 0)).normalized() * 0.05
        vp.tube(p, 'Rust#rebar', vp.bent_bar([a, a + d * L * 0.5, a + d * L + bend], R=0.06, n=2), 0.01, seg=5)
    # a metade da frente caída por dentro: girada pela linha da quebra, a ponta no entulho (2 pedaços)
    # a metade da frente caída por dentro, partida em 2 (a rachadura paralela à quebra): o pedaço de perto gira 36° na linha da
    # quebra, o de longe dobra mais 24° na rachadura e encosta no entulho
    c0_, c1_ = Vector((1.04, yb)), Vector((xb, 0.86))
    crack = []
    for k in range(9):
        u = k / 8
        q = c0_ + (c1_ - c0_) * u + Vector((1, 1)).normalized() * ((cp.h3(k, seed, 61) - 0.5) * 0.22 if 0 < k < 8 else 0.0)
        crack.append((q.x, q.y))
    near = [(-0.6, yb)] + brk[1:-1] + [(xb, -0.9)] + list(reversed(crack))
    far = list(crack) + [(xb, yb)]
    piv = V(-0.6, yb, ZR - 0.11)
    ax = (V(xb, -0.9, 0) - V(-0.6, yb, 0)).normalized()
    M1 = Matrix.Translation(piv) @ Matrix.Rotation(math.radians(36), 4, ax) @ Matrix.Translation(-piv)
    piv2 = V(c0_.x, c0_.y, ZR - 0.11)
    ax2 = (V(c1_.x, c1_.y, 0) - V(c0_.x, c0_.y, 0)).normalized()
    M2 = M1 @ Matrix.Translation(piv2) @ Matrix.Rotation(math.radians(24), 4, ax2) @ Matrix.Translation(-piv2)
    falls = []
    for poly, M in ((near, M1), (far, M2)):
        q = Part('fall')
        q.bm('Plaster#slab', 0.0)
        vp.prism(q, 'Plaster#slab', poly, Vector((0, 0, ZR - 0.22)), Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1)), 0.2)
        fo_ = q.finish()
        fo_.data.transform(M)
        falls.append(fo_)
    fo = cp.join(falls, 'fall')
    # ---- a parede de dentro, partida
    hp.wbox(p, 'Plaster#broken', WallFace('+x', 0.62, 0.0), Y0 + T, -0.4, 0.0, 2.1, -0.15, 0.0)
    # ---- o parapeito no que ficou de pé (fundo e esquerda) e os pilares de quina inteiros
    # (v3) a mureta comida (o hp.parapet) só no que ficou de pé: as aberturas tiram a frente, a direita e as pontas que caíram
    hp.parapet(p, X0, X1, Y0, Y1, ZR, h=0.62, t=0.22, seed=17, wear=1.3, corners=[0],
               gaps={'+y': [(X0 - 1, X1 + 1)], '+x': [(Y0 - 1, Y1 + 1)], '-y': [(X1 - 1.6, X1 + 1)], '-x': [(Y1 - 2.2, Y1 + 1)]})
    # ---- o entulho: o monte no canto caído (dentro e vazando pra rua), as lascas de laje, o tijolo solto
    c0 = Vector((2.4, 2.4, 0.0))
    R = 2.6
    mound(p, c0.x, c0.y, R, 0.95, seed)
    mound(p, 3.3, 3.3, 1.3, 0.35, seed + 1)
    for i in range(190):
        a = 2 * math.pi * cp.h3(i, seed, 21)
        r = R * cp.h3(i, seed, 23) ** 0.6
        x, y = c0.x + r * math.cos(a), c0.y + r * math.sin(a)
        if abs(x) > 3.95 or abs(y) > 3.95:
            continue
        hm = 0.95 * max(0.0, 1.0 - (r / R) ** 1.6)
        s_ = 0.1 + 0.42 * cp.h3(i, seed, 27) ** 2.2
        z = hm * (0.7 + 0.35 * cp.h3(i, seed, 25)) - s_ * 0.25
        mm = 'Plaster#rub' if cp.h3(i, seed, 29) > 0.4 else 'BrokenAdobe#rub'
        hp.chunk(p, (x, y, z), s_, i + seed * 1000, m=mm)
    # a caliça em cima da laje que ficou, junto da quebra
    for i in range(26):
        u = cp.h3(i, seed, 51)
        q = Vector((-0.6, Y1 - T, 0)) + (Vector((X1 - T, -0.9, 0)) - Vector((-0.6, Y1 - T, 0))) * u
        q += Vector((-1, -1, 0)).normalized() * (0.15 + 0.6 * cp.h3(i, seed, 53))
        hp.chunk(p, (q.x, q.y, ZR + 0.03), 0.08 + 0.18 * cp.h3(i, seed, 55) ** 2, i + 9000, m='BrokenAdobe#rub')
    for i in range(7):
        a = 2 * math.pi * cp.h3(i, seed, 31)
        r = R * 0.8 * cp.h3(i, seed, 33) ** 0.5
        x, y = c0.x + r * math.cos(a), c0.y + r * math.sin(a)
        if abs(x) > 3.7 or abs(y) > 3.7:
            continue
        L = 0.6 + 0.6 * cp.h3(i, seed, 35)
        ex, ey, ez = vp.ypr_frame(360 * cp.h3(i, seed, 37), (cp.h3(i, seed, 39) - 0.5) * 40, (cp.h3(i, seed, 41) - 0.5) * 30)
        C = Vector((x, y, 0.35 + 0.5 * max(0.0, 1 - r / R)))
        pts = [(-L / 2, -L * 0.35), (L / 2, -L * 0.4), (L * 0.42, L * 0.33), (-L * 0.1, L * 0.42), (-L / 2, L * 0.2)]
        vp.prism(p, 'Plaster#rub', pts, C, ex, ey, ez, 0.16)
        vp.tube(p, 'Rust#rebar', [C + ex * (L * 0.45) + ez * 0.08, C + ex * (L * 0.75) + ez * 0.2 + ey * 0.1], 0.01, seg=4)
    # a caixa d'água tombada em cima do monte
    t = Part('tank')
    hp.water_tank(t, (0.0, 0.0, 0.0), r=0.52, h=1.1, stand=0.0, seed=3, base=False)
    to = t.finish()
    to.data.transform(Matrix.Translation(V(2.2, 1.6, 1.0)) @ Matrix.Rotation(math.radians(80), 4, 'X')
                      @ Matrix.Rotation(math.radians(30), 4, 'Z') @ Matrix.Translation(V(0, 0, -0.6)))
    o = hp.finish(p)
    # (v3) a ruína tomba mais que a casa inteira (o canto que caiu puxou as paredes)
    hp.out_of_plumb(o, X0, X1, Y0, Y1, lean=(0.016, 0.012), twist=0.006, bow=(0.02, 0.02))
    o = cp.join([o, fo, to], 'SM_civ_house_ruin')
    return o


clear_scene()
o = build()
print('CIV', o.name, cp.tris(o), 'tris (antes da grade)')
export(o, OUT)
