"""As peças dos prédios da USA no detalhe AAA (01/11/2026; as versões B da Patriot, da Drop Zone, do Strategy Center e do Airfield).

O pedido do Felipe (31/10): "isso não é um jogo de lego [...] TEXTURIZADO, DETALHADO [...] curvas, nada de quinas quadradas". A
linguagem dos prédios da USA é a da ficha aprovada da War Factory (docs/concept/buildings/usa_war_factory_sheet.webp) e dos prédios
claros do game shot r1_raid_slabs: blocos modulares de PLACAS de metal claro com o chanfro largo e a junta funda, a faixa azul do
jogador, os ventiladores redondos no teto, as caixas de equipamento, os corrimãos de tubo, as escadas, os postes de luz com a base
listrada de perigo, o prato no teto. Tudo na escala de 2 px (docs/ESTUDO_DETALHE_AAA.md §0: 1 px = 3–5 cm; forma < 6 cm não lê).

Coordenadas da Unreal (X frente, Y direita, Z cima, metros), como o model_lib (o V() vira pro Blender). Os slots das construções
(import_provisional.py): Body, Trim, Dark, Player, Concrete, Hazard (o material já desenha as listras diagonais pela posição: aqui é
a peça inteira em Hazard), Glass, Light, Beacon, Yellow, Paint, Crate.

As chaves de malha (o que o harden do model_lib faz com elas, na classe 'building' do executor):
  'X#pnl' (0,04)  -> chanfro de 9 cm, 3 segmentos (a placa de parede, o bloco): a quina larga e redonda que pega o sol;
  'X#sm'  (0,012) -> 2,4 cm (a peça pequena de metal: o suporte, a caixa da luminária);
  'X#vp'  (0)     -> sem chanfro: as peças de perfil (lathe, tube, prism do model_vehicle_parts), a forma vem do desenho.
"""
import math

from mathutils import Vector

from model_lib import V
import model_vehicle_parts as vp
from model_vehicle_parts import axis_frame, bent_bar, lathe, prism, rbox, rounded_rect, tube


def keys(p, mats=('Body', 'Trim', 'Dark', 'Player', 'Concrete', 'Hazard', 'Yellow', 'Paint')):
    """Cria as chaves de malha com o chanfro certo antes de qualquer peça (o primeiro bm() de uma chave fixa o chanfro dela)."""
    for m in mats:
        p.bm(m + '#pnl', 0.04)
        p.bm(m + '#sm', 0.012)


def finish(p):
    """O p.finish() sem as malhas vazias (as chaves criadas pelo keys() e não usadas virariam slots de material a mais)."""
    p.bms = {k: b for k, b in p.bms.items() if len(b.faces)}
    p.bevel = {k: v for k, v in p.bevel.items() if k in p.bms}
    return p.finish()


def _bev(m):
    return 0.012 if m.endswith('#sm') else (0.0 if m.endswith('#vp') else 0.04)


def obox(p, m, c, size, ex, ey, ez):
    """A caixa num referencial qualquer (ortonormal), com o chanfro da chave ('#sm' 2,4 cm, '#vp' nenhum, o resto 9 cm)."""
    b = p.bm(m, _bev(m))
    c, ex, ey, ez = Vector(c), Vector(ex), Vector(ey), Vector(ez)
    vs = [b.verts.new(V(*(c + ex * (size[0] / 2 * i) + ey * (size[1] / 2 * j) + ez * (size[2] / 2 * k))))
          for k in (-1, 1) for j in (-1, 1) for i in (-1, 1)]
    for f in [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]:
        b.faces.new([vs[i] for i in f])


def _rect_minus(u0, u1, v0, v1, holes, gap):
    """O retângulo (u0..u1, v0..v1) menos os vãos [(a, b, c, d)], em retângulos inteiros, com 'gap' de folga em volta dos vãos."""
    hs = [(a - gap, b + gap, c - gap, d + gap) for (a, b, c, d) in holes]
    hs = [h for h in hs if h[0] < u1 and h[1] > u0 and h[2] < v1 and h[3] > v0]
    if not hs:
        return [(u0, u1, v0, v1)]
    us = sorted(set([u0, u1] + [min(max(h[0], u0), u1) for h in hs] + [min(max(h[1], u0), u1) for h in hs]))
    out = []
    for a, b in zip(us[:-1], us[1:]):
        if b - a < 1e-4:
            continue
        m = (a + b) / 2
        cuts = sorted((max(h[2], v0), min(h[3], v1)) for h in hs if h[0] < m < h[1])
        z = v0
        for c, d in cuts:
            if c > z + 1e-4:
                out.append((a, b, z, c))
            z = max(z, d)
        if v1 > z + 1e-4:
            out.append((a, b, z, v1))
    return out


def panel_wall(p, origin, eu, ev, en, w, h, pw=2.0, ph=None, m='Body#pnl', gap=0.06, proud=0.07, embed=0.05, holes=(),
               min_piece=0.3, rows=None, row_mats=None):
    """A parede de PLACAS num plano qualquer: origin = o canto de baixo-esquerdo da face (no plano), eu ao longo (w), ev pra cima
    (h), en pra fora. As placas (proud pra fora, embed pra dentro do miolo) com o chanfro largo da chave e a junta de 'gap' entre
    elas (o miolo aparece no fundo: a fresta que a máscara escurece). holes = os vãos [(u0, u1, v0, v1)] (as placas desviam);
    rows = as alturas das juntas horizontais (senão a cada ph)."""
    O, eu, ev, en = Vector(origin), Vector(eu), Vector(ev), Vector(en)
    nu = max(1, int(round(w / pw)))
    us = [w * i / nu for i in range(nu + 1)]
    if rows is None:
        ph = ph or h
        nv = max(1, int(round(h / ph)))
        vs = [h * j / nv for j in range(nv + 1)]
    else:
        vs = [0.0] + [r for r in rows if 0 < r < h] + [h]
    g = gap / 2
    for i in range(len(us) - 1):
        for j in range(len(vs) - 1):
            a = us[i] + (g if i > 0 else 0.0)
            b = us[i + 1] - (g if i < len(us) - 2 else 0.0)
            c = vs[j] + (g if j > 0 else 0.0)
            d = vs[j + 1] - (g if j < len(vs) - 2 else 0.0)
            for (a2, b2, c2, d2) in _rect_minus(a, b, c, d, holes, g):
                if b2 - a2 < min_piece or d2 - c2 < min_piece:
                    continue
                cc = O + eu * ((a2 + b2) / 2) + ev * ((c2 + d2) / 2) + en * ((proud - embed) / 2)
                obox(p, (row_mats or {}).get(j, m), cc, (b2 - a2, d2 - c2, proud + embed), eu, ev, en)


def square_annulus(p, m, c, hx, hy, R, z, t, seg=32):
    """A tampa retangular (2hx × 2hy, espessura t, o topo em z) com o furo redondo de raio R no meio (o poço do ventilador):
    cada ponto do círculo ligado ao ponto do retângulo no mesmo ângulo; as faces de cima, de baixo, a borda de fora e a parede do
    furo."""
    b = p.bm(m, 0.0)
    C = Vector(c)
    ring_i, ring_o = [], []
    for k in range(seg):
        a = 2 * math.pi * (k + 0.5) / seg
        ca, sa = math.cos(a), math.sin(a)
        s = min(hx / abs(ca) if abs(ca) > 1e-6 else 1e9, hy / abs(sa) if abs(sa) > 1e-6 else 1e9)
        ring_i.append((C.x + R * ca, C.y + R * sa))
        ring_o.append((C.x + s * ca, C.y + s * sa))
    # os 4 cantos do retângulo entram no anel de fora (senão a borda corta o canto em diagonal)
    outer = []
    for k in range(seg):
        outer.append([ring_o[k]])
    corners = [(hx, hy), (-hx, hy), (-hx, -hy), (hx, -hy)]
    for (cx, cy) in corners:
        ac = math.atan2(cy, cx) % (2 * math.pi)
        k = int((ac / (2 * math.pi)) * seg - 0.5) % seg
        outer[k].append((C.x + cx, C.y + cy))
    top_i = [b.verts.new(V(x, y, z)) for x, y in ring_i]
    bot_i = [b.verts.new(V(x, y, z - t)) for x, y in ring_i]
    top_o, bot_o, owner = [], [], []
    for k in range(seg):
        for q in outer[k]:
            top_o.append(b.verts.new(V(q[0], q[1], z)))
            bot_o.append(b.verts.new(V(q[0], q[1], z - t)))
            owner.append(k)
    n_o = len(top_o)
    # as faces de cima e de baixo: um leque por segmento do círculo
    j = 0
    for k in range(seg):
        k2 = (k + 1) % seg
        js = [i for i in range(n_o) if owner[i] == k]
        jn = [i for i in range(n_o) if owner[i] == k2][0]
        poly_t = [top_i[k]] + [top_o[i] for i in js] + [top_o[jn], top_i[k2]]
        b.faces.new(poly_t[::-1])
        poly_b = [bot_i[k]] + [bot_o[i] for i in js] + [bot_o[jn], bot_i[k2]]
        b.faces.new(poly_b)
        b.faces.new([top_i[k], top_i[k2], bot_i[k2], bot_i[k]][::-1])
    for i in range(n_o):
        i2 = (i + 1) % n_o
        b.faces.new([top_o[i], bot_o[i], bot_o[i2], top_o[i2]][::-1])


def fan(p, c, R, z_top, depth=0.42, blades=7, frame='Trim', dark='Dark', guard=True):
    """O ventilador no poço (a assinatura do teto da USA): a parede do poço e o fundo escuro, o cubo, as pás inclinadas e a grade de
    proteção (o anel e as barras ≥ 4 cm), a moldura saliente do poço. c = o centro (x, y)."""
    x, y = c
    lathe(p, dark + '#vp', (x, y, z_top), 'z', [(R - 0.002, 0.0), (R - 0.002, -depth), (0.0, -depth)], seg=32, smooth=True)
    lathe(p, frame + '#vp', (x, y, z_top), 'z', [(R - 0.03, 0.0), (R - 0.03, 0.06), (R + 0.02, 0.09), (R + 0.09, 0.09),
                                                 (R + 0.12, 0.05), (R + 0.12, 0.0)], seg=40, smooth=True, closed=True)
    zf = z_top - depth * 0.45
    lathe(p, frame + '#vp', (x, y, zf - 0.12), 'z', [(0.0, 0.0), (R * 0.2, 0.0), (R * 0.22, 0.05), (R * 0.2, 0.16), (R * 0.12, 0.2),
                                                    (0.0, 0.21)], seg=20)
    for k in range(blades):
        a = 2 * math.pi * k / blades
        d = Vector((math.cos(a), math.sin(a), 0.0))
        s = Vector((-math.sin(a), math.cos(a), 0.0))
        tw = math.radians(28)
        e2 = s * math.cos(tw) + Vector((0, 0, 1)) * math.sin(tw)
        e3 = d.cross(e2).normalized()
        L = R * 0.74
        obox(p, frame + '#sm', Vector((x, y, zf)) + d * (R * 0.2 + L / 2), (L, R * 0.36, 0.025), d, e2, e3)
    if guard:
        p.torus(frame + '#vp', (x, y, z_top + 0.11), 'z', R * 0.98, 0.022, seg=40, rseg=6)
        p.torus(frame + '#vp', (x, y, z_top + 0.11), 'z', R * 0.55, 0.018, seg=28, rseg=6)
        for k in range(4):
            a = math.pi * k / 4
            d = Vector((math.cos(a), math.sin(a), 0.0))
            tube(p, frame, [Vector((x, y, z_top + 0.11)) - d * R, Vector((x, y, z_top + 0.11)) + d * R], 0.018, seg=6)


def fan_box(p, c, size, fans=1, m='Body', z0=0.0, R=None):
    """A caixa do ventilador do teto (o condensador): o corpo de placas, a tampa com o(s) poço(s) e os ventiladores, as venezianas
    dos lados, os pés. c = (x, y) o centro, size = (sx, sy, sz) com sz a altura até o topo da tampa; fans = 1 ou 2 (ao longo de x)."""
    x, y = c
    sx, sy, sz = size
    t = 0.46                     # a tampa grossa: o furo redondo dela é o poço do ventilador
    obox(p, m + '#pnl', (x, y, z0 + 0.08 + (sz - t + 0.02 - 0.08) / 2), (sx - 0.08, sy - 0.08, sz - t + 0.02 - 0.08), (1, 0, 0), (0, 1, 0),
         (0, 0, 1))
    for sxx in (-1, 1):
        for syy in (-1, 1):
            obox(p, 'Trim#sm', (x + sxx * (sx / 2 - 0.2), y + syy * (sy / 2 - 0.2), z0 + 0.05), (0.3, 0.3, 0.1), (1, 0, 0), (0, 1, 0), (0, 0, 1))
    R = R or min(sy * 0.38, sx / (2 * fans) * 0.78)
    cs = [(x + (k - (fans - 1) / 2) * sx / fans, y) for k in range(fans)]
    for (cx, cy) in cs:
        square_annulus(p, m + '#sm', (cx, cy), sx / fans / 2, sy / 2, R, z0 + sz, t)
        fan(p, (cx, cy), R, z0 + sz, depth=t - 0.04)
    # as venezianas dos dois lados compridos
    for s in (-1, 1):
        hb = sz - t - 0.08
        louvers(p, (x, y + s * (sy / 2 - 0.04), z0 + 0.08 + hb / 2), (0, s, 0), (1, 0, 0), sx * 0.7, hb * 0.6, max(3, int(hb * 0.6 / 0.09)))


def louvers(p, c, n_face, along, L, H, n, frame='Trim', slat='Trim', back='Dark', tilt=35.0):
    """A veneziana numa face (c = o centro na face, n_face = a normal): o fundo escuro, a moldura e as aletas inclinadas (≥ 4 cm)."""
    en = Vector(n_face).normalized()
    eu = Vector(along).normalized()
    ev = en.cross(eu).normalized()
    if ev.z < 0:
        ev = -ev
    C = Vector(c)
    prism(p, back + '#vp', rounded_rect(L + 0.02, H + 0.02, 0.03, 2), C - en * 0.004, eu, ev, en, 0.012)
    fw = 0.05
    for sgn in (-1, 1):
        obox(p, frame + '#sm', C + ev * (sgn * (H / 2 + fw / 2)) + en * 0.025, (L + 2 * fw, fw, 0.05), eu, ev, en)
        obox(p, frame + '#sm', C + eu * (sgn * (L / 2 + fw / 2)) + en * 0.025, (fw, H, 0.05), eu, ev, en)
    step = H / n
    a = math.radians(tilt)
    e2 = ev * math.cos(a) + en * math.sin(a)
    e3 = en * math.cos(a) - ev * math.sin(a)
    for k in range(n):
        obox(p, slat + '#sm', C + ev * (-H / 2 + step * (k + 0.5)) + en * 0.02, (L, step * 0.85, 0.014), eu, e2, e3)


def rail(p, m, pts, z, h=1.05, r=0.025, step=1.6, mid=True, toe=None):
    """O guarda-corpo de tubo ao longo da polilinha (x, y) na altura de piso z: o corrimão de cima com as curvas dobradas, o do meio,
    os montantes (nos cantos e a cada 'step') com a sapata; toe = o material do rodapé (a chapa de 10 cm no piso)."""
    top = bent_bar([Vector((x, y, z + h)) for x, y in pts], R=0.12, n=4)
    tube(p, m, top, r, seg=8)
    if mid:
        tube(p, m, bent_bar([Vector((x, y, z + h * 0.5)) for x, y in pts], R=0.12, n=4), r * 0.8, seg=8)
    posts = []
    for (ax, ay), (bx, by) in zip(pts[:-1], pts[1:]):
        L = math.hypot(bx - ax, by - ay)
        n = max(1, int(round(L / step)))
        for k in range(n + 1):
            q = (ax + (bx - ax) * k / n, ay + (by - ay) * k / n)
            if not any(abs(q[0] - o[0]) < 0.05 and abs(q[1] - o[1]) < 0.05 for o in posts):
                posts.append(q)
    for (x, y) in posts:
        tube(p, m, [(x, y, z), (x, y, z + h)], r, seg=8)
        lathe(p, m + '#vp', (x, y, z), 'z', [(0.0, 0.0), (0.06, 0.0), (0.06, 0.015), (0.035, 0.03), (0.0, 0.03)], seg=10)
    if toe:
        for (ax, ay), (bx, by) in zip(pts[:-1], pts[1:]):
            L = math.hypot(bx - ax, by - ay)
            d = Vector((bx - ax, by - ay, 0.0)).normalized()
            obox(p, toe + '#sm', ((ax + bx) / 2, (ay + by) / 2, z + 0.06), (L, 0.02, 0.12), d, Vector((-d.y, d.x, 0.0)), (0, 0, 1))


def ladder(p, m, x, y, z0, z1, n_dir, w=0.5, cage=True, wall=0.18, top_ext=0.9):
    """A escada de marinheiro encostada na parede: n_dir = a normal da parede (a escada fica pra fora dela, a 'wall' m), os 2 trilhos,
    os degraus de 30 cm, as mãos-francesas e (cage) a gaiola de aros a partir de 2,2 m."""
    n = Vector(n_dir).normalized()
    s = Vector((-n.y, n.x, 0.0))
    C = Vector((x, y, 0.0)) + n * wall
    for sg in (-1, 1):
        q = C + s * (sg * w / 2)
        tube(p, m, [(q.x, q.y, z0), (q.x, q.y, z1 + top_ext)], 0.025, seg=8)
        for zz in (z0 + 0.4, (z0 + z1) / 2, z1 - 0.2):
            a = Vector((q.x, q.y, zz))
            tube(p, m, [a, a - n * wall], 0.02, seg=6)
    k = 0
    zz = z0 + 0.3
    while zz < z1 + 0.05:
        a, b = C - s * (w / 2), C + s * (w / 2)
        tube(p, m, [(a.x, a.y, zz), (b.x, b.y, zz)], 0.016, seg=6)
        zz += 0.3
        k += 1
    if cage and z1 - z0 > 2.6:
        zz = z0 + 2.3
        rr = 0.36
        while zz < z1 + 0.9:
            arc = [C + n * 0.02 + s * (w / 2) * math.cos(math.radians(a)) + n * (rr * math.sin(math.radians(a))) for a in range(0, 181, 20)]
            arc = [Vector((q.x, q.y, zz)) for q in arc]
            tube(p, m, arc, 0.016, seg=6)
            zz += 0.9
        for a in (45, 90, 135):
            q = C + s * (w / 2) * math.cos(math.radians(a)) + n * (rr * math.sin(math.radians(a)))
            tube(p, m, [(q.x, q.y, z0 + 2.3), (q.x, q.y, z1 + 0.9)], 0.014, seg=6)


def beacon(p, c, r=0.09, h=0.2, cage=True):
    """A sirene âmbar: a base escura, a cúpula (Beacon pisca no jogo) e a gaiola de arame por cima."""
    x, y, z = c
    lathe(p, 'Dark#vp', (x, y, z), 'z', [(0.0, 0.0), (r * 1.25, 0.0), (r * 1.25, 0.03), (r * 1.05, 0.05), (0.0, 0.05)], seg=16)
    lathe(p, 'Beacon#vp', (x, y, z + 0.05), 'z', [(0.0, 0.0), (r, 0.0), (r, h * 0.7), (r * 0.85, h * 0.9), (r * 0.5, h), (0.0, h * 1.02)],
          seg=16)
    if cage:
        for k in range(4):
            a = math.radians(45 + 90 * k)
            tube(p, 'Trim', [(x + (r + 0.02) * math.cos(a), y + (r + 0.02) * math.sin(a), z + 0.04),
                             (x + (r + 0.02) * math.cos(a), y + (r + 0.02) * math.sin(a), z + 0.05 + h * 0.8),
                             (x, y, z + 0.05 + h + 0.04)], 0.008, seg=5)


def lamp_head(p, c, aim, w=0.5, h=0.32, body='Dark'):
    """O holofote: a caixa de quinas redondas virada pra 'aim' com a lente acesa (Light) e a aba de cima."""
    d = Vector(aim).normalized()
    ex = d
    ey = Vector((0, 0, 1)).cross(ex).normalized()
    ez = ex.cross(ey).normalized()
    C = Vector(c)
    obox(p, body + '#sm', C, (0.22, w, h), ex, ey, ez)
    obox(p, 'Light#vp', C + ex * 0.115, (0.012, w - 0.08, h - 0.08), ex, ey, ez)
    obox(p, body + '#sm', C + ex * 0.12 + ez * (h / 2 + 0.01), (0.16, w + 0.02, 0.02), ex, ey, ez)


def lamp_post(p, x, y, h=6.0, aim=(0.0, 0.0), heads=1, footing=0.45, head=(0.5, 0.32)):
    """O poste de luz do pátio (a ficha da War Factory): a sapata de concreto chanfrada, o pé listrado de perigo (o Hazard do jogo
    desenha as listras), o poste cônico, o braço e o(s) holofote(s) virado(s) pra 'aim'."""
    obox(p, 'Concrete#pnl', (x, y, 0.05), (2 * footing, 2 * footing, 0.5), (1, 0, 0), (0, 1, 0), (0, 0, 1))
    lathe(p, 'Hazard#vp', (x, y, 0.3), 'z', [(0.0, 0.0), (0.22, 0.0), (0.22, 0.62), (0.18, 0.68), (0.0, 0.68)], seg=16)
    lathe(p, 'Trim#vp', (x, y, 0.98), 'z', [(0.0, 0.0), (0.12, 0.0), (0.075, 0.2), (0.06, h - 1.0), (0.0, h - 1.0)], seg=12)
    d = Vector((aim[0] - x, aim[1] - y, 0.0))
    d = d.normalized() if d.length > 1e-6 else Vector((1, 0, 0))
    s = Vector((-d.y, d.x, 0.0))
    top = Vector((x, y, h - 0.05))
    if heads == 1:
        tube(p, 'Trim', bent_bar([top - Vector((0, 0, 0.3)), top, top + d * 0.6], R=0.2, n=4), 0.04, seg=8)
        lamp_head(p, top + d * 0.6 - Vector((0, 0, 0.12)), d + Vector((0, 0, -0.55)), w=head[0], h=head[1])
    else:
        tube(p, 'Trim', [top - s * (0.2 + head[0] * 0.8), top + s * (0.2 + head[0] * 0.8)], 0.045, seg=8)
        for sg in (-1, 1):
            lamp_head(p, top + s * (sg * (0.1 + head[0] * 0.75)) + d * 0.16 - Vector((0, 0, 0.12)), d + Vector((0, 0, -0.55)), w=head[0],
                      h=head[1])
    beacon(p, (x, y, h + 0.02), r=0.07, h=0.15, cage=False)


def antenna(p, x, y, z, h, r=0.03):
    """A antena de chicote: a base torneada com a mola, o chicote e a ponta."""
    lathe(p, 'Dark#vp', (x, y, z), 'z', [(0.0, 0.0), (0.08, 0.0), (0.08, 0.04), (0.05, 0.07), (0.045, 0.22), (0.0, 0.22)], seg=10)
    tube(p, 'Trim', [(x, y, z + 0.2), (x, y, z + h)], r * 0.5, seg=6)
    lathe(p, 'Dark#vp', (x, y, z + h), 'z', [(0.0, -0.02), (0.035, -0.02), (0.035, 0.03), (0.0, 0.045)], seg=8)


def pipe(p, m, pts, r, clamp='Dark', step=1.6, bend=None, seg=14):
    """O cano com os cotovelos redondos, as flanges nas pontas e as braçadeiras."""
    P = bent_bar([Vector(q) for q in pts], R=bend if bend is not None else 2.5 * r, n=5)
    tube(p, m, P, r, seg=seg)
    for (a, b) in ((pts[0], pts[1]), (pts[-1], pts[-2])):
        A, B = Vector(a), Vector(b)
        d = (B - A).normalized()
        lathe(p, m + '#vp', A + d * 0.04, d, [(0.0, -0.035), (r * 1.45, -0.035), (r * 1.45, 0.035), (0.0, 0.035)], seg=seg)
    for (a, b) in zip(pts[:-1], pts[1:]):
        A, B = Vector(a), Vector(b)
        L = (B - A).length
        if L < 0.8:
            continue
        d = (B - A).normalized()
        n = max(1, int(L / step))
        for k in range(n):
            q = A + d * (L * (k + 0.5) / n)
            lathe(p, clamp + '#vp', q, d, [(r + 0.002, -0.035), (r + 0.025, -0.035), (r + 0.025, 0.035), (r + 0.002, 0.035)], seg=seg,
                  closed=True)


def h_tank(p, c, axis, r, L, m='Body', band='Player', saddles=2, foot_z=0.0):
    """O tanque deitado (combustível): o corpo com os tampos abaulados, a faixa da cor do jogador, os aros de reforço, os berços de
    concreto e a boca de visita em cima com a tampa e os parafusos."""
    A, E1, E2 = axis_frame(axis)
    C = Vector(c)
    hl = L / 2
    prof = [(0.0, -hl - r * 0.32), (r * 0.45, -hl - r * 0.27), (r * 0.78, -hl - r * 0.15), (r * 0.95, -hl - r * 0.04), (r, -hl + 0.02),
            (r, hl - 0.02), (r * 0.95, hl + r * 0.04), (r * 0.78, hl + r * 0.15), (r * 0.45, hl + r * 0.27), (0.0, hl + r * 0.32)]
    lathe(p, m + '#vp', C, A, prof, seg=36, smooth=True, hard=60.0)
    lathe(p, band + '#vp', C, A, [(r + 0.012, -0.28), (r + 0.012, 0.28), (r - 0.01, 0.28), (r - 0.01, -0.28)], seg=36, closed=True)
    for t in (-hl * 0.62, hl * 0.62):
        lathe(p, 'Trim#vp', C + A * t, A, [(r + 0.02, -0.05), (r + 0.02, 0.05), (r - 0.01, 0.05), (r - 0.01, -0.05)], seg=36, closed=True)
    zf = foot_z
    for t in [(-hl * 0.55) + k * (hl * 1.1) / (saddles - 1) for k in range(saddles)] if saddles > 1 else [0.0]:
        q = C + A * t
        side = A.cross(Vector((0, 0, 1))).normalized()
        obox(p, 'Concrete#pnl', (q.x, q.y, zf + (C.z - r * 0.55 - zf) / 2 + 0.02), (0.36 if abs(A.x) > 0.5 else r * 1.7,
                                                                                    r * 1.7 if abs(A.x) > 0.5 else 0.36,
                                                                                    C.z - r * 0.55 - zf + 0.04),
             (1, 0, 0), (0, 1, 0), (0, 0, 1))
        _ = side
    top = C + Vector((0, 0, r))
    lathe(p, 'Trim#vp', top - Vector((0, 0, 0.06)), 'z', [(0.0, 0.0), (0.26, 0.0), (0.26, 0.16), (0.3, 0.16), (0.3, 0.2), (0.0, 0.21)],
          seg=20)
    vp.bolt_ring(p, 'Dark', top + Vector((0, 0, 0.15)), 'z', 0.27, 10, r=0.018, h=0.012)


def flat_paint(p, m, pts, z0, z1):
    """A tinta no chão: o polígono pts [(x, y)] extrudado de z0 a z1 (sem chanfro)."""
    b = p.bm(m + '#vp', 0.0)
    lo = [b.verts.new(V(x, y, z0)) for x, y in pts]
    hi = [b.verts.new(V(x, y, z1)) for x, y in pts]
    b.faces.new(lo)
    b.faces.new(hi[::-1])
    n = len(pts)
    for i in range(n):
        k = (i + 1) % n
        b.faces.new([lo[i], hi[i], hi[k], lo[k]][::-1])


def ring_paint(p, m, c, r0, r1, z0, z1, seg=64):
    """O anel pintado (o alvo, o círculo do heliponto): a coroa de r0 a r1."""
    b = p.bm(m + '#vp', 0.0)
    rings = []
    for r, z in ((r1, z0), (r1, z1), (r0, z1), (r0, z0)):
        rings.append([b.verts.new(V(c[0] + r * math.cos(2 * math.pi * k / seg), c[1] + r * math.sin(2 * math.pi * k / seg), z))
                      for k in range(seg)])
    for i in range(4):
        ra, rb = rings[i], rings[(i + 1) % 4]
        for k in range(seg):
            b.faces.new([ra[k], ra[(k + 1) % seg], rb[(k + 1) % seg], rb[k]])


def runway_light(p, x, y, z, lens='Light', r=0.13):
    """A luz de pista embutida/de borda: o corpo torneado baixo e a cúpula de vidro acesa."""
    lathe(p, 'Trim#vp', (x, y, z), 'z', [(0.0, 0.0), (r, 0.0), (r, 0.05), (r * 0.8, 0.08), (0.0, 0.08)], seg=14)
    lathe(p, lens + '#vp', (x, y, z + 0.08), 'z', [(0.0, 0.0), (r * 0.62, 0.0), (r * 0.6, 0.05), (r * 0.35, 0.08), (0.0, 0.085)], seg=12)


def tarp_cover(p, x0, x1, y0, y1, z, h, m='Player', strap='Dark', seed=0.0, ties=3, box=4):
    """A lona amarrada sobre a carga, alinhada aos eixos (ver tarp_cover_at)."""
    tarp_cover_at(p, ((x0 + x1) / 2, (y0 + y1) / 2), x1 - x0, y1 - y0, 0.0, z, h, m=m, strap=strap, seed=seed, ties=ties, box=box)


def tarp_cover_at(p, c, L, W, yaw, z, h, m='Player', strap='Dark', seed=0.0, ties=3, box=4):
    """A lona amarrada sobre a carga (o volume embaixo): o pano com a barriga entre as cintas, os lados caindo (box alto: a carga
    caixote, o pano cai reto) e as cintas atravessadas com a fivela. c = o centro (x, y), L ao longo do giro 'yaw' (graus), W de lado,
    z = a altura da barra do pano, h = quanto ele sobe até o topo da carga."""
    import model_lib
    a = math.radians(yaw)
    ex = Vector((math.cos(a), math.sin(a), 0.0))
    ey = Vector((-math.sin(a), math.cos(a), 0.0))
    C = Vector((c[0], c[1], 0.0))
    nx, ny = 22, 14
    tx = [(i + 0.5) / ties for i in range(ties)]

    def zf(u, v, zz):
        pinch = max(math.exp(-((u - t) / 0.06) ** 2) for t in tx)
        dome = 1.0 - abs(2 * v - 1) ** box
        edge = 1.0 - abs(2 * u - 1) ** (box + 2)
        wr = 0.018 * math.sin(u * 37.0 + v * 9.0 + seed) * math.sin(v * 3.14)
        return z + h * (dome * edge) * (1 - 0.06 * pinch) + wr

    def P(u, v):
        return C + ex * (L * (u - 0.5)) + ey * (W * (v - 0.5))
    cs = [P(0, 0), P(1, 0), P(1, 1), P(0, 1)]
    model_lib.sheet(p, m + '#cloth', [(q.x, q.y, z) for q in cs], nx, ny, z_fn=zf, thick=0.025)
    for t in tx:
        rr = []
        for k in range(17):
            v = k / 16
            q = P(t, -0.02 + 1.04 * v)
            zz = zf(t, min(max(v, 0.0), 1.0), z) + 0.02
            if k in (0, 16):
                zz = z - 0.25
            rr.append(Vector((q.x, q.y, zz)))
        tube(p, strap, rr, 0.022, seg=6)
        q = P(t, 0.78)
        rbox(p, 'Trim#vp', (q.x, q.y, zf(t, 0.78, z) + 0.04), (0.08, 0.07, 0.025), yaw=yaw, r=0.012, k=1)


def windsock(p, x, y, h=5.0, length=1.9, wind=(0.55, 0.75)):
    """A biruta: a sapata, o poste torneado, o pivô com o aro e a manga de 5 gomos (amarelo e branco) que afina e CAI com o peso."""
    obox(p, 'Concrete#pnl', (x, y, 0.02), (0.7, 0.7, 0.44), (1, 0, 0), (0, 1, 0), (0, 0, 1))
    lathe(p, 'Trim#vp', (x, y, 0.24), 'z', [(0.0, 0.0), (0.16, 0.0), (0.16, 0.03), (0.08, 0.06), (0.06, 0.2), (0.05, h - 0.24),
                                            (0.0, h - 0.24)], seg=12)
    lathe(p, 'Dark#vp', (x, y, h - 0.02), 'z', [(0.0, 0.0), (0.08, 0.0), (0.08, 0.1), (0.0, 0.12)], seg=12)
    w = Vector((wind[0], wind[1], 0.0)).normalized()
    r0, r1, seg = 0.3, 0.13, 5
    base = Vector((x, y, h + 0.04)) + w * 0.14
    lathe(p, 'Trim#vp', base, (w.x, w.y, 0.0), [(r0 - 0.005, -0.025), (r0 + 0.03, -0.025), (r0 + 0.03, 0.025), (r0 - 0.005, 0.025)],
          seg=20, closed=True)
    tube(p, 'Trim', [Vector((x, y, h + 0.04)), base], 0.02, seg=6)
    n = seg * 4

    def at(t):
        return base + w * (length * t) + Vector((0, 0, -0.42 * t ** 1.8))
    for i in range(seg):
        pts = [at((i * 4 + j) / n) for j in range(5)]
        tube(p, 'Yellow' if i % 2 == 0 else 'Paint', pts, r0, seg=16, caps=(i == seg - 1),
             r_fn=lambda j, i=i: r0 + (r1 - r0) * ((i * 4 + j) / n))


def turbine_vent(p, x, y, z, r=0.32):
    """O exaustor de teto (a "cebola" giratória): o pescoço, o rotor de aletas em gomos e o chapéu."""
    lathe(p, 'Trim#vp', (x, y, z), 'z', [(0.0, 0.0), (r * 1.1, 0.0), (r * 1.1, 0.04), (r * 0.62, 0.08), (r * 0.6, 0.3), (0.0, 0.3)], seg=16)
    prof = [(r * 0.6, 0.3), (r * 0.95, 0.42), (r * 1.05, 0.56), (r * 0.95, 0.7), (r * 0.55, 0.82), (r * 0.2, 0.86), (0.0, 0.87)]
    lathe(p, 'Body#vp', (x, y, z), 'z', prof, seg=24, smooth=True)
    for k in range(12):
        a = 2 * math.pi * k / 12
        pts = [Vector((x + rr * math.cos(a + 0.35 * t), y + rr * math.sin(a + 0.35 * t), z + zz))
               for t, (rr, zz) in enumerate([(r * 0.62, 0.31), (r * 0.98, 0.43), (r * 1.07, 0.56), (r * 0.97, 0.69), (r * 0.57, 0.81)])]
        tube(p, 'Trim', pts, 0.012, seg=4)


def slab(p, m, x0, x1, y0, y1, z0, z1):
    """A laje do chão SEM o fundo (5 faces): o fundo enterrado nunca aparece e, na grade do detalhe, custava ~1/3 dos triângulos da
    laje (o pátio de 40 × 32 m do Airfield: ~35 mil). As quinas de cima levam o chanfro da chave (a borda de fora fica viva: some no
    chão)."""
    b = p.bm(m, _bev(m))
    vs = [b.verts.new(V(x, y, z)) for z in (z0, z1) for y in (y0, y1) for x in (x0, x1)]
    for f in [(4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]:
        b.faces.new([vs[i] for i in f])
