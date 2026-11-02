"""As peças de detalhe dos veículos provisórios por script (31/10/2026; o estudo: docs/ESTUDO_DETALHE_AAA.md §2.1 item 5 e §3).

O pedido do Felipe: "isso não é um jogo de lego [...] detalhes minuciosos como as lagartas, as rodas [...] curvas, texturas, nada de
quinas quadradas". Na câmera do jogo 1 px vale 3–5 cm (2–3 cm deitada): forma menor que ~6 cm não existe, então cada peça aqui já
nasce na escala de 2 px, com a forma feita no PERFIL (o arredondado desenhado nos pontos, não um chanfro de caixa) e com triângulos
contados (o modo de detalhe ainda passa a grade de 14 cm por cima: aresta longa vira várias).

  - lathe: o perfil girado (pneu, roda, cano, tambor, escotilha): a base de quase tudo que é redondo;
  - tyre: o pneu de verdade (o flanco abaulado, a banda com os blocos em V de 6–8 cm subindo no ombro, o aro rebaixado com as porcas);
  - road_wheel / idler / sprocket / return_roller: a rodagem da lagarta (a banda de borracha, o disco rebaixado, o anel de porcas, a
    motriz com os dentes);
  - track_links: a esteira de elos com forma (a sapata com as pontas redondas do pino, a garra, os conectores das pontas, o dente-guia)
    seguindo a curva das rodas, com a barriga entre os roletes de cima (sag);
  - gun_barrel: o cano por estações (as transições cônicas, o evacuador, a capa térmica com os anéis, a boca com o furo fundo, o freio
    de boca), e o fole do mantelete;
  - tarp_bundle / tarp_drape / rope / strap_band: a lona dobrada em camadas, apertada pelas cintas que abraçam a forma, a aba caída com
    as dobras e a corda amarrando;
  - tube, grab_handle, hinge, bolt_row, bolt_ring, hatch_round, jerrycan, ammo_box, shovel, pickaxe, crowbar, jack, tow_cable,
    spare_links, mudflap, drum: o equipamento de campanha, tudo ≥ 6 cm.

Coordenadas da Unreal (X frente, Y direita, Z cima, metros), como o model_lib (o V() vira pro Blender). Cada função recebe os slots de
material por parâmetro (cada facção tem os dela). Não mexe no model_lib.py nem no model_detail.py.
"""
import math

from mathutils import Vector

from model_lib import Part, V, track_path


# ===================================================================================================== referenciais
def axis_frame(axis):
    """(A, E1, E2): o eixo e dois perpendiculares (Unreal). axis: 'x', 'y', 'z' ou um vetor."""
    if axis == 'x':
        A = Vector((1, 0, 0))
    elif axis == 'y':
        A = Vector((0, 1, 0))
    elif axis == 'z':
        A = Vector((0, 0, 1))
    else:
        A = Vector(axis).normalized()
    ref = Vector((0, 0, 1)) if abs(A.z) < 0.9 else Vector((1, 0, 0))
    E1 = ref.cross(A).normalized()
    E2 = A.cross(E1).normalized()
    return A, E1, E2


def ypr_frame(yaw=0.0, pitch=0.0, roll=0.0):
    """O referencial do obox do model_lib (yaw +X→+Y, pitch nariz pra cima, roll): (ex, ey, ez)."""
    cy, sy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    cp, sp = math.cos(math.radians(pitch)), math.sin(math.radians(pitch))
    cr, sr = math.cos(math.radians(roll)), math.sin(math.radians(roll))
    ex = Vector((cp * cy, cp * sy, sp))
    ey0, ez0 = Vector((-sy, cy, 0.0)), Vector((-sp * cy, -sp * sy, cp))
    return ex, ey0 * cr + ez0 * sr, ez0 * cr - ey0 * sr


def face_frame(n, along=None):
    """Um referencial numa face de normal n: (eu, ev, n); eu ao longo de 'along' (projetado) ou horizontal."""
    n = Vector(n).normalized()
    if along is not None:
        eu = Vector(along) - n * Vector(along).dot(n)
    elif abs(n.z) > 0.7:
        eu = Vector((1, 0, 0)) - n * n.x
    else:
        eu = Vector((0, 0, 1)).cross(n)
    eu.normalize()
    ev = n.cross(eu)
    return eu, ev, n


def _k(m):
    """A chave da malha das peças daqui: o mesmo material, numa malha própria ('#vp', sem o chanfro do modificador). Sem isso a peça
    herdava o chanfro de 3 segmentos da malha do casco (a porca sextavada virava 24 lados). Chave com '#' fica como veio."""
    return m if '#' in m else m + '#vp'


def _quad(b, vs, smooth):
    f = b.faces.new(vs)
    f.smooth = smooth
    return f


# ===================================================================================================== primitivas
def lathe(p, m, c, axis, prof, seg=32, bevel=0.0, smooth=True, closed=False, phase=0.0, hard=35.0):
    """O perfil [(raio, a)] girado em volta do eixo que passa em c ('a' ao longo do eixo). Ponto com raio 0 vira o polo (leque de
    triângulos). closed=True: o perfil é um laço (o anel de borracha, o toro achatado). O arredondado sai dos pontos do perfil; onde o
    perfil dobra mais que 'hard' graus o anel vira aresta dura (a quina viva com a normal certa, sem o chanfro do modificador: zero
    triângulo a mais); bevel > 0 só se a peça precisar do chanfro do harden."""
    A, E1, E2 = axis_frame(axis)
    C = Vector(c)
    b = p.bm(_k(m), bevel)
    cs = [(math.cos(2 * math.pi * j / seg + phase), math.sin(2 * math.pi * j / seg + phase)) for j in range(seg)]
    rings = []
    for rho, a in prof:
        base = C + A * a
        if rho < 1e-7:
            rings.append([b.verts.new(V(*base))])
        else:
            rings.append([b.verts.new(V(*(base + (E1 * cc + E2 * ss) * rho))) for cc, ss in cs])
    pairs = list(zip(rings[:-1], rings[1:]))
    if closed:
        pairs.append((rings[-1], rings[0]))
    for r0, r1 in pairs:
        if len(r0) == 1 and len(r1) == 1:
            continue
        for j in range(seg):
            jj = (j + 1) % seg
            if len(r0) == 1:
                _quad(b, [r0[0], r1[j], r1[jj]], smooth)
            elif len(r1) == 1:
                _quad(b, [r0[j], r0[jj], r1[0]], smooth)
            else:
                _quad(b, [r0[j], r0[jj], r1[jj], r1[j]], smooth)
    # as quinas vivas do perfil: o anel de arestas duras
    n = len(prof)
    for i in range(n):
        if len(rings[i]) == 1:
            continue
        if not closed and i in (0, n - 1):
            _sharp_ring(b, rings[i])
            continue
        p0, p1, p2 = prof[(i - 1) % n], prof[i], prof[(i + 1) % n]
        d0 = Vector((p1[0] - p0[0], p1[1] - p0[1]))
        d1 = Vector((p2[0] - p1[0], p2[1] - p1[1]))
        if d0.length < 1e-9 or d1.length < 1e-9:
            continue
        if math.degrees(d0.angle(d1)) > hard:
            _sharp_ring(b, rings[i])


def _sharp_ring(b, ring):
    for j in range(len(ring)):
        e = b.edges.get([ring[j], ring[(j + 1) % len(ring)]])
        if e is not None:
            e.smooth = False


def prism(p, m, pts, origin, eu, ev, en, depth, bevel=0.0, smooth=False):
    """O polígono pts [(u, v)] (convexo ou não, sem furo) no plano (eu, ev) em 'origin', extrudado 'depth' ao longo de en (pode ser
    negativo). As faces de cima e de baixo são o polígono (n-gon); os lados são quads."""
    b = p.bm(_k(m), bevel)
    O = Vector(origin)
    lo = [b.verts.new(V(*(O + eu * u + ev * v))) for u, v in pts]
    hi = [b.verts.new(V(*(O + eu * u + ev * v + en * depth))) for u, v in pts]
    b.faces.new(lo)
    b.faces.new(hi[::-1])
    n = len(pts)
    for i in range(n):
        k = (i + 1) % n
        _quad(b, [lo[i], hi[i], hi[k], lo[k]], smooth)


def rounded_rect(w, h, r, k=3):
    """O retângulo de cantos redondos (centro na origem) como lista (u, v): k pontos por canto."""
    r = min(r, w / 2 - 1e-4, h / 2 - 1e-4)
    out = []
    for (cx, cy, a0) in ((w / 2 - r, h / 2 - r, 0), (-w / 2 + r, h / 2 - r, 90), (-w / 2 + r, -h / 2 + r, 180), (w / 2 - r, -h / 2 + r, 270)):
        for i in range(k + 1):
            a = math.radians(a0 + 90 * i / k)
            out.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return out


def rbox(p, m, c, size, yaw=0.0, pitch=0.0, roll=0.0, r=0.03, k=3, bevel=0.0):
    """A caixa de cantos redondos nas 4 arestas de pé (o polígono arredondado extrudado ao longo do z local) com o chanfro do
    harden nas de cima e de baixo: a lata, a caixa de munição, o bloco do farol. size = (x, y, z) local."""
    ex, ey, ez = ypr_frame(yaw, pitch, roll)
    C = Vector(c) - ez * (size[2] / 2)
    prism(p, m, rounded_rect(size[0], size[1], r, k), C, ex, ey, ez, size[2], bevel=bevel)


def tube(p, m, pts, r, seg=8, smooth=True, caps=True, bevel=0.0, r_fn=None):
    """O tubo ao longo da polilinha (barra, cabo, corda, alça), com o referencial transportado (não torce). r_fn(i) muda o raio
    por ponto (a corda que afina, o cabo com a luva)."""
    pts = [Vector(q) for q in pts]
    n = len(pts)
    if n < 2:
        return
    b = p.bm(_k(m), bevel)
    tans = []
    for i in range(n):
        d = (pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)])
        tans.append(d.normalized() if d.length > 1e-9 else Vector((1, 0, 0)))
    ref = Vector((0, 0, 1)) if abs(tans[0].z) < 0.9 else Vector((1, 0, 0))
    u = ref.cross(tans[0]).normalized()
    rings = []
    for i in range(n):
        t = tans[i]
        u = (u - t * u.dot(t))
        if u.length < 1e-6:
            u = (Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((1, 0, 0))).cross(t)
        u.normalize()
        w = t.cross(u)
        rr = r_fn(i) if r_fn else r
        rings.append([b.verts.new(V(*(pts[i] + (u * math.cos(2 * math.pi * j / seg) + w * math.sin(2 * math.pi * j / seg)) * rr)))
                      for j in range(seg)])
    for r0, r1 in zip(rings[:-1], rings[1:]):
        for j in range(seg):
            jj = (j + 1) % seg
            _quad(b, [r0[j], r0[jj], r1[jj], r1[j]], smooth)
    if caps:
        b.faces.new(rings[0][::-1])
        b.faces.new(rings[-1])
        _sharp_ring(b, rings[0])
        _sharp_ring(b, rings[-1])


def arc_pts(c, e1, e2, R, a0, a1, n):
    """Os pontos do arco de raio R no plano (e1, e2) em volta de c, de a0 a a1 graus."""
    c, e1, e2 = Vector(c), Vector(e1), Vector(e2)
    return [c + (e1 * math.cos(math.radians(a0 + (a1 - a0) * i / n)) + e2 * math.sin(math.radians(a0 + (a1 - a0) * i / n))) * R
            for i in range(n + 1)]


def bent_bar(corners, R=0.05, n=4):
    """A barra dobrada: a polilinha com as quinas arredondadas (raio R, n pontos por curva): alça, guarda, corrimão."""
    cs = [Vector(q) for q in corners]
    out = [cs[0]]
    for i in range(1, len(cs) - 1):
        a, b, c = cs[i - 1], cs[i], cs[i + 1]
        d0, d1 = (b - a).normalized(), (c - b).normalized()
        rr = min(R, (b - a).length / 2, (c - b).length / 2)
        p0, p1 = b - d0 * rr, b + d1 * rr
        for k in range(n + 1):
            t = k / n
            q = (p0 * (1 - t) + b * t) * (1 - t) + (b * (1 - t) + p1 * t) * t
            out.append(q)
    out.append(cs[-1])
    return out


# ===================================================================================================== parafusos e ferragens
def nut(p, m, c, n, r=0.022, h=0.02, seg=6):
    """A porca sextavada (ou cabeça de parafuso) assentada na face de normal n."""
    n = Vector(n).normalized()
    p.cyl(_k(m), tuple(Vector(c) + n * (h / 2 - 0.002)), tuple(n), r, h, seg=seg, bevel=0.0)


def bolt_row(p, m, a, b, count, n, r=0.022, h=0.018):
    """A fileira de parafusos de a até b (inclusive), na face de normal n: o ritmo que lê de longe (o parafuso solto não lê)."""
    a, b = Vector(a), Vector(b)
    for k in range(count):
        t = k / (count - 1) if count > 1 else 0.5
        nut(p, m, a + (b - a) * t, n, r, h)


def bolt_ring(p, m, c, axis, R, count, r=0.022, h=0.018, phase=0.0):
    """O anel de porcas (cubo, flange, mantelete) no plano do eixo, saindo pro lado de 'axis'."""
    A, E1, E2 = axis_frame(axis)
    for k in range(count):
        a = 2 * math.pi * k / count + phase
        nut(p, m, Vector(c) + (E1 * math.cos(a) + E2 * math.sin(a)) * R, A, r, h)


def hinge(p, m, c, axis, length, r=0.03, knuckles=3, leaf=None):
    """A dobradiça: os nós do pino (cilindros com a fresta entre eles) e, se leaf=(n, largura), a aba assentada."""
    A, _, _ = axis_frame(axis)
    C = Vector(c)
    kl = length / knuckles
    for k in range(knuckles):
        q = C + A * (-length / 2 + kl * (k + 0.5))
        p.cyl(_k(m), tuple(q), tuple(A), r, kl * 0.86, seg=10, bevel=0.0)
    if leaf:
        n, w = Vector(leaf[0]).normalized(), leaf[1]
        d = A.cross(n).normalized()
        frame_c = C + d * (w / 2) - n * (r * 0.6)
        eu, ev, en = A, d, n
        prism(p, m, rounded_rect(length * 0.9, w, 0.015, 2), frame_c, eu, ev, en, 0.018)


def grab_handle(p, m, a, b, n, height=0.09, r=0.016, R=0.035):
    """A alça em U soldada (o corrimão de mão da torre e do casco): as pernas de a e de b saindo pela normal n."""
    a, b, n = Vector(a), Vector(b), Vector(n).normalized()
    pts = bent_bar([a - n * 0.01, a + n * height, b + n * height, b - n * 0.01], R=R, n=3)
    tube(p, m, pts, r, seg=8)


# ===================================================================================================== rodas
def tyre(p, c, r, w, side, rim_r=None, rubber='Rubber', rim='Metal', hub='Metal', nut_m=None, seg=36, lugs=True, lug_h=0.024,
         nuts=8, holes_m=None, dish=None, axis='y', beadlock=False, style='mud', n_lugs=None):
    """O pneu de verdade com a roda de aço. c = o centro, r = o raio externo (no topo dos blocos), w = a largura, side = +1/−1 pro
    lado de fora (o lado que mostra o aro). O perfil: a banda reta, o ombro redondo, o FLANCO ABAULADO (mais largo no meio) que
    afunila no talão; os blocos da banda em V (6–8 cm, alternados, subindo no ombro: a silhueta serrilhada de lado); a roda rebaixada
    ('dish' pra dentro do flanco) com a aba, o cubo saliente e o anel de porcas; holes_m = os furos escuros do disco.
    style (a banda): 'mud' (o V militar de 22° com o ombro), 'at' (blocos menores, de estrada e terra), 'chevron' (o V fechado de
    32°), 'tractor' (as garras grandes de 42° desencontradas), 'paddle' (as pás de areia), 'rib' (os frisos da dianteira de
    areia). n_lugs: o número de blocos (arredondado pra múltiplo de 4: a caixa da roda fica centrada no eixo, o jogo gira por ela)."""
    A, E1, E2 = axis_frame(axis)
    s = side
    C = Vector(c)
    rim_r = rim_r if rim_r is not None else r * 0.58
    R0 = r - (lug_h if lugs else 0.0)               # a banda (a carcaça, embaixo dos blocos)
    hw = w / 2
    # ---- a carcaça: o laço do perfil (raio, a), com o flanco abaulado
    half = [(R0, 0.0), (R0, hw * 0.62), (R0 - 0.006, hw * 0.80), (R0 - 0.02, hw * 0.92), (R0 - 0.045, hw * 0.99),
            (R0 - 0.085, hw * 1.02), (rim_r + (R0 - rim_r) * 0.62, hw * 1.04), (rim_r + (R0 - rim_r) * 0.35, hw * 1.0),
            (rim_r + 0.03, hw * 0.9), (rim_r + 0.005, hw * 0.84)]
    loop = [(rr, -a) for rr, a in reversed(half[1:])] + half
    loop = loop + [(rim_r - 0.01, hw * 0.84), (rim_r - 0.01, -hw * 0.84)]
    lathe(p, rubber, C, A, [(rr, a) for rr, a in loop], seg=seg, smooth=True, closed=True)
    # ---- os blocos em V: duas fileiras alternadas, inclinados (o desenho direcional do pneu militar), e o pedaço no ombro
    if lugs:
        _tyre_lugs(p, rubber, C, A, E1, E2, R0, r, hw, w, lug_h, style, n_lugs)
    dish = dish if dish is not None else min(0.06, hw * 0.5)
    # ---- a roda de aço do lado de fora: a aba, o disco rebaixado, o cubo, as porcas (perfil do eixo ao eixo: sólido fechado)
    o = hw * 0.84                     # o plano do talão
    prof = [(0.0, o - dish + 0.075), (r * 0.085, o - dish + 0.075), (r * 0.11, o - dish + 0.06), (r * 0.14, o - dish + 0.045),
            (r * 0.25, o - dish + 0.042), (r * 0.28, o - dish + 0.02), (r * 0.31, o - dish), (rim_r * 0.80, o - dish),
            (rim_r * 0.88, o - dish + 0.012), (rim_r * 0.94, o - 0.012), (rim_r - 0.004, o), (rim_r + 0.012, o + 0.004),
            (rim_r + 0.016, o - 0.01), (rim_r - 0.006, o - 0.03), (rim_r - 0.006, -o + 0.03), (r * 0.3, -o + 0.04), (0.0, -o + 0.04)]
    lathe(p, rim, C, A * s, prof, seg=seg, smooth=True)
    if beadlock:                     # o anel de trava do talão com os parafusos (o pneu de fora de estrada)
        lathe(p, hub, C, A * s, [(rim_r + 0.004, o + 0.004), (rim_r + 0.03, o + 0.004), (rim_r + 0.034, o + 0.018),
                                  (rim_r + 0.006, o + 0.018)], seg=seg, smooth=False, closed=True)
        bolt_ring(p, nut_m or hub, C + A * s * (o + 0.018), A * s, rim_r + 0.018, max(10, int(rim_r * 40)), r=0.011, h=0.01)
    if nuts:
        bolt_ring(p, nut_m or hub, C + A * s * (o - dish + 0.042), A * s, r * 0.195, nuts, r=0.02, h=0.022, phase=0.3)
    if holes_m:
        for k in range(6):
            a = 2 * math.pi * k / 6 + 0.26
            q = C + A * s * (o - dish + 0.002) + (E1 * math.cos(a) + E2 * math.sin(a)) * ((r * 0.31 + rim_r * 0.8) / 2)
            p.cyl(_k(holes_m), tuple(q), tuple(A), r * 0.07, 0.006, seg=12, bevel=0.0)


def _tyre_lugs(p, rubber, C, A, E1, E2, R0, r, hw, w, lug_h, style, n_lugs):
    """Os blocos da banda por estilo (ver tyre). Cada bloco é uma caixa de 12 triângulos (sem chanfro: a quina viva do bloco é o
    que pega o pó e o brilho do uso), ~metade do passo, com o vão escuro entre eles."""
    if style == 'rib':
        for off in (-0.3, 0.0, 0.3):
            lathe(p, rubber + '#lug', C + A * (off * w), A, [(R0 - 0.004, -lug_h * 0.6), (R0 + lug_h * 0.6, -lug_h * 0.45),
                                                            (R0 + lug_h, 0.0), (R0 + lug_h * 0.6, lug_h * 0.45), (R0 - 0.004, lug_h * 0.6)],
                  seg=40, smooth=True, closed=True)
        return
    step = {'mud': 0.135, 'at': 0.11, 'chevron': 0.13, 'tractor': 0.2, 'paddle': 0.2}.get(style, 0.135)
    nl = n_lugs if n_lugs else 2 * math.pi * R0 / step
    nl = max(8, 4 * int(round(nl / 4)))
    T = 2 * math.pi * R0 / nl
    ang_deg = {'mud': 22, 'at': 12, 'chevron': 32, 'tractor': 42}.get(style, 22)
    stagger = style in ('mud', 'at', 'tractor')
    for k in range(nl):
        for row in (-1, 1):
            t = 2 * math.pi * (k + (0.5 if (row > 0 and stagger) else 0.0)) / nl
            radial = E1 * math.cos(t) + E2 * math.sin(t)
            tang = A.cross(radial).normalized()
            if style == 'paddle':
                if row < 0:
                    _obox_frame(p, rubber + '#lug', C + radial * (R0 + lug_h / 2 - 0.004), A, tang, radial, w * 0.94, 0.045, lug_h + 0.008)
                continue
            ang = math.radians(ang_deg * row)
            ea = A * math.cos(ang) + tang * math.sin(ang)
            et = tang * math.cos(ang) - A * math.sin(ang)
            if style == 'tractor':
                L, lw, hh, ac = w * 0.62, T * 0.32, lug_h + 0.012, row * w * 0.24
            elif style == 'chevron':
                L, lw, hh, ac = w * 0.44, T * 0.38, lug_h + 0.004, row * w * 0.21
            elif style == 'at':
                L, lw, hh, ac = hw * 0.55, T * 0.46, lug_h, row * (hw * 0.86 - hw * 0.55 / 2)
            else:
                L, lw, hh, ac = hw * 0.62, T * 0.48, lug_h, row * (hw * 0.86 - hw * 0.62 / 2)
            _obox_frame(p, rubber + '#lug', C + A * ac + radial * (R0 + hh / 2 - 0.003), ea, et, radial, L, lw, hh)
            if style in ('mud', 'at'):
                # o pedaço do ombro (desce o flanco 45°: a borda serrilhada de lado)
                sh = C + A * (row * hw * 0.99) + radial * (R0 - 0.03)
                en = (radial + A * row).normalized()
                _obox_frame(p, rubber + '#lug', sh + en * (lug_h * 0.4), tang, en.cross(tang).normalized(), en, lw, 0.07, lug_h * 0.8)


def _obox_frame(p, m, c, ex, ey, ez, sx, sy, sz):
    """Caixa sem chanfro num referencial dado (os blocos do pneu, os conectores): 8 vértices, 12 triângulos."""
    b = p.bm(_k(m), 0.0)
    c = Vector(c)
    vs = [b.verts.new(V(*(c + ex * (sx / 2 * i) + ey * (sy / 2 * j) + ez * (sz / 2 * k)))) for k in (-1, 1) for j in (-1, 1) for i in (-1, 1)]
    for f in [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]:
        b.faces.new([vs[i] for i in f])


def _band(r, r_in, hw, k=0.022):
    """O laço do perfil de uma banda de borracha (roda de apoio): as quinas de fora arredondadas em 3 passos."""
    return [(r_in, -hw), (r - k * 1.4, -hw), (r - k * 0.55, -hw + k * 0.25), (r - k * 0.12, -hw + k * 0.75), (r, -hw + k * 1.5),
            (r, hw - k * 1.5), (r - k * 0.12, hw - k * 0.75), (r - k * 0.55, hw - k * 0.25), (r - k * 1.4, hw), (r_in, hw)]


def road_wheel(p, c, r, w, side, paint, metal, rubber='Rubber', gap=0.08, seg=24, nuts=8, dish=0.045, single=False):
    """A roda de apoio da lagarta (a dupla, com o vão do dente-guia no meio): em cada uma a banda de borracha com o ombro redondo e
    o disco de aço REBAIXADO (o prato fundo que a oclusão escurece), o cubo saliente com o anel de porcas e a tampa. side = o lado
    de fora. single=True: uma roda só da largura toda (o Paladin, a roda sem vão)."""
    A = Vector((0, 1, 0))
    C = Vector(c)
    ww = w if single else (w - gap) / 2
    hw = ww / 2
    offs = [0.0] if single else [side * (gap / 2 + hw), -side * (gap / 2 + hw)]
    for i, off in enumerate(offs):
        Cw = C + A * off
        band = _band(r, r * 0.83, hw)
        if i:                                   # a de dentro (só aparece pelo vão): o perfil curto e menos lados
            lathe(p, rubber, Cw, A, [band[0], band[2], band[4], band[5], band[7], band[9]], seg=max(16, seg - 8), smooth=True,
                  closed=True)
            lathe(p, paint, Cw, A, [(0.0, hw - 0.012), (r * 0.85, hw - 0.012), (r * 0.85, -hw + 0.012), (0.0, -hw + 0.012)],
                  seg=max(16, seg - 8), smooth=False)
            continue
        lathe(p, rubber, Cw, A, band, seg=seg, smooth=True, closed=True)
        # a de fora: o disco com o prato, o cubo e as porcas
        prof = [(0.0, hw + 0.035), (r * 0.12, hw + 0.035), (r * 0.15, hw + 0.02), (r * 0.18, hw + 0.004),
                (r * 0.33, hw + 0.004), (r * 0.37, hw - 0.02), (r * 0.42, hw - dish), (r * 0.66, hw - dish),
                (r * 0.72, hw - dish * 0.5), (r * 0.78, hw - 0.01), (r * 0.85, hw - 0.01), (r * 0.85, -hw + 0.01), (0.0, -hw + 0.01)]
        lathe(p, paint, Cw, A * side, prof, seg=seg, smooth=True)
        bolt_ring(p, metal, Cw + A * side * (hw + 0.004), A * side, r * 0.255, nuts, r=0.019, h=0.02, phase=0.2)
    # o eixo entre as duas (o cubo do braço)
    if not single:
        p.cyl(_k(metal), tuple(C), 'y', r * 0.2, w * 0.9, seg=12, bevel=0.0)


def idler(p, c, r, w, side, paint, metal, gap=0.08, seg=26, nuts=6):
    """A roda-guia (de aço, sem borracha): os dois discos com a aba que roda no elo, o prato, o cubo com as porcas e a manivela
    do tensionador por dentro."""
    A = Vector((0, 1, 0))
    C = Vector(c)
    hw = (w - gap) / 4
    for i, d in enumerate((side, -side)):
        Cw = C + A * d * (gap / 2 + hw)
        prof = [(0.0, hw + 0.03), (r * 0.16, hw + 0.03), (r * 0.2, hw + 0.012), (r * 0.36, hw + 0.012), (r * 0.42, hw - 0.025),
                (r * 0.78, hw - 0.025), (r * 0.86, hw - 0.004), (r - 0.012, hw - 0.004), (r, hw - 0.018), (r, -hw + 0.018),
                (r - 0.012, -hw + 0.004), (r * 0.5, -hw + 0.01), (0.0, -hw + 0.01)]
        lathe(p, metal if i else paint, Cw, A * d, prof, seg=seg, smooth=True)
        if i == 0:
            bolt_ring(p, metal, Cw + A * d * (hw + 0.012), A * d, r * 0.28, nuts, r=0.018, h=0.02)
    p.cyl(_k(metal), tuple(C), 'y', r * 0.22, w * 0.8, seg=12, bevel=0.0)


def sprocket(p, c, r, w, side, paint, metal, teeth=13, seg=26, nuts=8, holes_m=None):
    """A roda motriz: as duas coroas DENTADAS (o dente de ponta redonda que engata no pino do elo), o tambor entre elas, o cubo
    saliente com o anel de porcas e os furos de alívio."""
    A = Vector((0, 1, 0))
    _, E1, E2 = axis_frame(A)
    C = Vector(c)
    r_root, r_tip, r_in = r - 0.045, r + 0.03, r * 0.55
    for d in (-1, 1):
        a0 = d * w * 0.3 - w * 0.075
        gear_ring(p, metal, C, A, r_in, r_root, r_tip, teeth, a0, a0 + w * 0.15, phase=(math.pi / teeth if d > 0 else 0.0))
    # o tambor e o prato de fora
    prof = [(0.0, w * 0.5 + 0.03), (r * 0.15, w * 0.5 + 0.03), (r * 0.19, w * 0.5 + 0.012), (r * 0.34, w * 0.5 + 0.012),
            (r * 0.38, w * 0.5 - 0.01), (r * 0.62, w * 0.45 - 0.01), (r * 0.62, w * 0.22), (r * 0.6, -w * 0.22),
            (r * 0.6, -w * 0.45), (0.0, -w * 0.45)]
    lathe(p, paint, C, A * side, prof, seg=seg, smooth=True)
    bolt_ring(p, metal, C + A * side * (w * 0.5 + 0.012), A * side, r * 0.26, nuts, r=0.019, h=0.02)
    if holes_m:
        for k in range(5):
            a = 2 * math.pi * k / 5
            q = C + A * side * (w * 0.5 - 0.004) + (E1 * math.cos(a) + E2 * math.sin(a)) * (r * 0.49)
            p.cyl(_k(holes_m), tuple(q), 'y', r * 0.075, 0.012, seg=12, bevel=0.0)


def gear_ring(p, m, c, axis, r_in, r_root, r_tip, teeth, a0, a1, phase=0.0):
    """A coroa dentada: o anel (r_in até a raiz) com os dentes de flanco inclinado e ponta redonda, de a0 a a1 no eixo."""
    A, E1, E2 = axis_frame(axis)
    C = Vector(c)
    b = p.bm(_k(m), 0.0)
    prof = []
    d = 2 * math.pi / teeth
    for k in range(teeth):
        t0 = k * d + phase
        for f, rr in ((0.0, r_root), (0.22, r_root), (0.34, r_tip - 0.012), (0.41, r_tip), (0.59, r_tip), (0.66, r_tip - 0.012),
                      (0.78, r_root)):
            prof.append((t0 + f * d, rr))

    def P(t, rr, a):
        return b.verts.new(V(*(C + A * a + (E1 * math.cos(t) + E2 * math.sin(t)) * rr)))
    o0 = [P(t, rr, a0) for t, rr in prof]
    o1 = [P(t, rr, a1) for t, rr in prof]
    i0 = [P(t, r_in, a0) for t, _ in prof]
    i1 = [P(t, r_in, a1) for t, _ in prof]
    n = len(prof)
    for i in range(n):
        k = (i + 1) % n
        b.faces.new([o0[i], o0[k], i0[k], i0[i]])
        b.faces.new([o1[k], o1[i], i1[i], i1[k]])
        b.faces.new([o0[k], o0[i], o1[i], o1[k]])
        b.faces.new([i0[i], i0[k], i1[k], i1[i]])


def return_roller(p, c, r, w, metal, rubber='Rubber', seg=18):
    """O rolete de cima: a banda de borracha e o cubo."""
    A = Vector((0, 1, 0))
    lathe(p, rubber, c, A, _band(r, r * 0.7, w / 2, k=0.012), seg=seg, smooth=True, closed=True)
    lathe(p, metal, c, A, [(0.0, w / 2 + 0.04), (r * 0.4, w / 2 + 0.04), (r * 0.45, w / 2 + 0.02), (r * 0.72, w / 2 - 0.005),
                           (r * 0.72, -w / 2 + 0.005), (0.0, -w / 2 + 0.005)], seg=seg, smooth=True)


def track_roller(p, c, r, w, metal, side, seg=20):
    """O rolete de baixo de trator (flange dupla): o aro de rolagem no meio, as 2 flanges que seguram o elo, a tampa do eixo."""
    hw = w / 2
    lathe(p, metal, c, Vector((0, side, 0)), [(0.0, hw + 0.03), (r * 0.45, hw + 0.03), (r * 0.55, hw + 0.005), (r + 0.03, hw),
                                             (r + 0.036, hw - 0.025), (r + 0.02, hw - 0.06), (r, hw - 0.08), (r, -hw + 0.08),
                                             (r + 0.02, -hw + 0.06), (r + 0.036, -hw + 0.025), (r + 0.03, -hw), (r * 0.55, -hw + 0.005),
                                             (r * 0.45, -hw - 0.03), (0.0, -hw - 0.03)], seg=seg)
    bolt_ring(p, metal, Vector(c) + Vector((0, side * (hw + 0.03), 0)), Vector((0, side, 0)), r * 0.3, 6, r=0.016, h=0.014)


# ===================================================================================================== esteira
def _sag_path(samples, sags):
    """A barriga da esteira no trecho de cima: sags = [(x0, x1, z_topo, flecha)...]; os pontos do trecho (a normal pra cima) entre
    x0 e x1 descem a flecha × sen(π t). O ângulo de cada elo sai de novo dos vizinhos."""
    pts = [[x, z] for (x, z), _ in samples]
    n = len(pts)
    for i, ((x, z), ang) in enumerate(samples):
        if -math.cos(ang) < 0.97:          # só o trecho de cima (a normal pra fora aponta pra cima)
            continue
        for x0, x1, ztop, f in sags:
            if x0 < x < x1 and abs(z - ztop) < 0.08:
                t = (x - x0) / (x1 - x0)
                pts[i][1] = z - f * math.sin(math.pi * t)
    out = []
    for i in range(n):
        a, b = pts[(i - 1) % n], pts[(i + 1) % n]
        out.append(((pts[i][0], pts[i][1]), math.atan2(b[1] - a[1], b[0] - a[0])))
    return out


def track_links(name, circles, y, width, pitch=0.16, t_in=0.045, t_out=0.013, grouser=0.022, guide='single',
                m='Track', guide_m='Metal', conn_m=None, sags=(), gap=0.16, horn_h=0.07, conn=True, center_gap=None, corner_k=2,
                horn_w=0.054, web=True, web_gap=0.05):
    """A esteira de elos COM FORMA (o pedido: "detalhes minuciosos como as lagartas"): cada elo é a sapata em perfil (as quinas
    redondas, a garra de fora), dividida em 2 almofadas com o vão do meio onde passa o dente-guia (single), o conector do pino nas
    duas bordas (o ritmo do lado da esteira) e o dente-guia de dentro (single, double ou None). O caminho é o envoltório das rodas
    (track_path do model_lib, com o raio já somado da folga: a face de dentro do elo fica 'margem − t_in' fora da roda), com a
    barriga (sags) no trecho de cima. gap = a fresta entre os elos (fração do passo): na curva da motriz e da roda-guia ela abre.
    (01/11, o Felipe: "tem falhas na lagarta, buracos entre os pedaços dos lados e no centro") web = o CORPO de aço do elo, inteiro na
    largura, por dentro das almofadas (o elo de verdade é uma peça só; as almofadas e o vão do dente-guia são por fora): fecha o furo do
    meio e o dos lados; web_gap = a fresta entre os corpos (fração do passo; ~1 cm, abre só na curva)."""
    p = Part(name)
    samples = track_path(circles, pitch)
    if sags:
        samples = _sag_path(samples, sags)
    L = pitch * (1.0 - gap)
    T = t_in + t_out
    hw = width / 2
    cg = center_gap if center_gap is not None else (0.07 if guide == 'single' else 0.0)
    shoe = _shoe_pts(L, t_in, t_out, grouser, pitch * 0.22, k=corner_k)
    pads = [(-hw, hw)] if cg <= 0 else [(-hw, -cg / 2), (cg / 2, hw)]
    horn = [(-pitch * 0.3, t_out * 0.3), (pitch * 0.3, t_out * 0.3), (pitch * 0.3, -t_in + 0.004), (pitch * 0.18, -t_in - horn_h),
            (-pitch * 0.18, -t_in - horn_h), (-pitch * 0.3, -t_in + 0.004)]
    conn_m = conn_m or guide_m
    ey = Vector((0.0, 1.0, 0.0))
    for (x, z), ang in samples:
        eu = Vector((math.cos(ang), 0.0, math.sin(ang)))
        en = Vector((math.sin(ang), 0.0, -math.cos(ang)))          # pra fora (o chão embaixo)
        O = Vector((x, y, z))
        for y0, y1 in pads:
            prism(p, m, shoe, O + ey * y0, eu, en, ey, y1 - y0)
        if web:
            lw = pitch * (1.0 - web_gap)
            prism(p, m, [(lw / 2, -t_in * 0.25), (-lw / 2, -t_in * 0.25), (-lw / 2, -t_in), (lw / 2, -t_in)], O + ey * (-hw), eu, en, ey, width)
        # os conectores do pino nas duas bordas, na junta com o próximo elo
        if conn:
            J = O + eu * (pitch / 2) + en * ((t_out - t_in) / 2)
            for sgn in (-1, 1):
                p.cyl(_k(conn_m), tuple(J + ey * sgn * (hw + 0.014)), 'y', T * 0.42 + 0.004, 0.036, seg=8, bevel=0.0)
        # o dente-guia por dentro (a base dele fecha o vão das almofadas)
        if guide:
            offs = (0.0,) if guide == 'single' else (-width * 0.17, width * 0.17)
            for o in offs:
                prism(p, guide_m, horn, O + ey * (o - horn_w / 2), eu, en, ey, horn_w)
    o = p.finish()
    # (01/11, a esteira que anda) o centro e a tangente de cada elo e o passo médio da volta: o model_run_detail grava a tangente × passo
    # no UV2/UV3 de cada vértice, e o material (M_StructureTrack) desliza o elo por ela até o lugar do próximo (o jogo manda a fase)
    n = len(samples)
    step = sum(math.dist(samples[i][0], samples[(i + 1) % n][0]) for i in range(n)) / max(1, n)
    o['track_step'] = step
    o['track_frames'] = [c for (x, z), ang in samples for c in (x, y, z, math.cos(ang), math.sin(ang))]
    return o


def _shoe_pts(L, t_in, t_out, g_h, g_w, rc=None, k=2):
    """O perfil da sapata (u, w; w > 0 pra fora): o retângulo de quinas redondas (raio rc, o olhal do pino) com a garra
    trapezoidal no meio da face de fora."""
    T = t_in + t_out
    rc = rc if rc is not None else min(0.016, T * 0.38)
    wc = (t_out - t_in) / 2
    pts = []
    corners = ((L / 2 - rc, t_out - rc, 0), (-L / 2 + rc, t_out - rc, 90), (-L / 2 + rc, -t_in + rc, 180), (L / 2 - rc, -t_in + rc, 270))
    for ci, (cx, cy, a0) in enumerate(corners):
        for i in range(k + 1):
            a = math.radians(a0 + 90 * i / k)
            pts.append((cx + rc * math.cos(a), cy + rc * math.sin(a)))
        if ci == 0:            # a face de fora, de +u pra −u: a garra
            gw = min(g_w, L - 2 * rc - 0.03)
            pts += [(gw / 2 + 0.013, t_out), (gw / 2, t_out + g_h), (-gw / 2, t_out + g_h), (-gw / 2 - 0.013, t_out)]
    return pts


def spare_links(p, c, n_face, along, count, pitch=0.16, width=0.5, m='Track', guide_m='Metal', t=0.05):
    """Os elos de reserva presos na chapa (a frente do casco, a torre): a fileira de sapatas com a garra pra fora e a barra que segura."""
    eu, ev, en = face_frame(n_face, along)
    C = Vector(c)
    for k in range(count):
        O = C + eu * ((k - (count - 1) / 2) * pitch)
        prism(p, m, _shoe_pts(pitch * 0.84, t * 0.5, t * 0.5, 0.02, pitch * 0.3), O - ev * (width / 2) + en * (t * 0.5),
              eu, en, ev, width)
    tube(p, guide_m, [C - eu * (count * pitch / 2 + 0.05) + en * (t + 0.035), C + eu * (count * pitch / 2 + 0.05) + en * (t + 0.035)],
         0.018, seg=8)
    for sgn in (-1, 1):
        q = C + eu * sgn * (count * pitch / 2 + 0.04)
        tube(p, guide_m, [q, q + en * (t + 0.05)], 0.02, seg=8)


# ===================================================================================================== canhão
def gun_barrel(p, stations, z, y=0.0, m='Paint', seg=32, bore=0.06, bore_depth=0.35, bore_m='Dark', bevel=0.0):
    """O cano por estações [(x, raio)] de trás pra frente (a última é a boca): as transições ficam cônicas (nada de degrau de
    cilindro), e a boca tem o FURO fundo (o perfil entra no cano 'bore_depth', com o fundo escuro). Um material só."""
    prof = [(0.0, stations[0][0])] + [(r, x) for x, r in stations]
    xm = stations[-1][0]
    if bore > 0:
        prof += [(bore, xm), (bore, xm - bore_depth), (0.0, xm - bore_depth)]
    else:
        prof += [(0.0, xm)]
    lathe(p, m, (0.0, y, z), 'x', prof, seg=seg, smooth=True, bevel=bevel)
    if bore_m and bore > 0:
        p.cyl(_k(bore_m), (xm - bore_depth + 0.004, y, z), 'x', bore * 0.98, 0.006, seg=16, bevel=0.0)


def ring_band(p, m, x0, x1, r, z, y=0.0, lip=0.006, seg=32, axis='x'):
    """O anel em volta do cano (a braçadeira da capa térmica, a faixa da cor do jogador): borda arredondada, 'lip' acima do cano."""
    R = r + lip
    k = min(0.008, (x1 - x0) / 4)
    prof = [(r - 0.004, x0), (R - k, x0), (R - k * 0.3, x0 + k * 0.3), (R, x0 + k), (R, x1 - k), (R - k * 0.3, x1 - k * 0.3),
            (R - k, x1), (r - 0.004, x1)]
    A = Vector((1, 0, 0)) if axis == 'x' else Vector(axis)
    lathe(p, m, (0.0, y, z), A, prof, seg=seg, smooth=True, closed=True)


def muzzle_brake(p, m, x0, x1, r, z, y=0.0, ports=2, port_m='Dark', seg=24):
    """O freio de boca: o bloco cilíndrico com as janelas laterais (escuras) e a ponta chanfrada."""
    lathe(p, m, (0.0, y, z), 'x', [(r * 0.7, x0), (r, x0 + 0.03), (r, x1 - 0.03), (r * 0.82, x1), (r * 0.5, x1), (r * 0.5, x1 - 0.02)],
          seg=seg, smooth=True, closed=True)
    L = (x1 - x0)
    for k in range(ports):
        xc = x0 + L * (k + 0.5) / ports
        for sgn in (-1, 1):
            p.box(_k(port_m), xc - L * 0.16, xc + L * 0.16, y + sgn * (r - 0.004) - 0.006, y + sgn * (r - 0.004) + 0.006, z - r * 0.5, z + r * 0.5, bevel=0.0)


def bellows(p, m, x0, x1, r0, r1, z, y=0.0, folds=4, depth=0.025, seg=28):
    """O fole de lona/borracha do mantelete (a capa entre o cano e a torre): as dobras em anel."""
    prof = []
    n = folds * 2
    for i in range(n + 1):
        t = i / n
        rr = r0 + (r1 - r0) * t + (depth if i % 2 else 0.0)
        prof.append((rr, x0 + (x1 - x0) * t))
    prof = [(r1 * 0.6, x1)] + prof[::-1] + [(r0 * 0.6, x0)]
    lathe(p, m, (0.0, y, z), 'x', prof, seg=seg, smooth=True, closed=True)


# ===================================================================================================== lona, cinta, corda
def _hash1(x):
    return (math.sin(x * 127.1 + 311.7) * 43758.5453) % 1.0


def tarp_bundle(p, c, size, yaw=0.0, pitch=0.0, roll=0.0, m='Team', strap_m='Cloth', buckle_m='Metal', straps=2, layers=3,
                seed=0.0, n=22, k=22, sag_ends=0.3, swap=False):
    """A lona DOBRADA em fardo, presa pelas cintas (o pedido: "a lona com dobra presa por cinta/corda, não caixa lisa"). Como se
    dobra lona de verdade: uma PILHA de camadas, cada uma com a DOBRA redonda de um lado (alternando) e a borda mais quadrada do
    outro, um pouco desencontradas (o degrau entre elas é a linha escura que lê de lado); as cintas ESTRANGULAM a pilha (estufa entre
    elas), as pontas murcham e caem, o topo tem rugas saindo das cintas. A cinta é uma faixa que abraça a forma, com a fivela.
    c = o centro da base (onde apoia); size = (comprimento ao longo do x local, largura, altura)."""
    L, W, H = size
    ex, ey, ez = ypr_frame(yaw, pitch, roll)
    if swap:                          # as cintas atravessam o lado curto: o comprimento do fardo é o lado longo
        ex, ey, L, W = ey, -ex, W, L
    C = Vector(c)
    su = [(i + 0.5) / straps for i in range(straps)]
    rnd = lambda i: _hash1(seed * 7.3 + i * 3.17)

    def pinch(u):
        return max([math.exp(-((u - s_) / 0.05) ** 2) for s_ in su] + [0.0])

    def droop(u):
        end = min(u, 1 - u)
        return 1.0 - sag_ends * max(0.0, 1.0 - end / 0.12) ** 2

    # as camadas: espessura, deslocamento de lado, o lado da dobra
    nl = max(1, layers)
    hs = [H / nl * (0.9 + 0.2 * rnd(i)) for i in range(nl)]
    sc = H / sum(hs)
    hs = [h * sc for h in hs]
    z0s = []
    z = 0.0
    for h in hs:
        z0s.append(z)
        z += h * 0.94
    for li in range(nl):
        h = hs[li]
        fold = 1 if li % 2 == 0 else -1
        off = (rnd(li + 10) - 0.5) * 0.05 * W
        wl = W * (1.0 - 0.06 * li / max(1, nl - 1)) * (0.97 + 0.04 * rnd(li + 20))
        top_layer = li == nl - 1
        rings = []
        for i in range(n + 1):
            u = i / n
            pc = pinch(u)
            dr = droop(u)
            sq = (1 - 0.16 * pc) * dr                     # o aperto da cinta e a ponta murcha
            zb = z0s[li] * (1 - 0.16 * pc) * dr
            hh = h * sq
            ww = wl / 2 * (1 - 0.08 * pc) * (0.9 + 0.1 * dr)
            ring = []
            for j in range(k):
                th = 2 * math.pi * j / k
                ct, st = math.cos(th), math.sin(th)
                e = 0.62 if ct * fold > 0 else 0.3            # a dobra (redonda) de um lado, a borda (quadrada) do outro
                yy = ww * math.copysign(abs(ct) ** e, ct) + off
                zz = zb + hh / 2 * (1 + math.copysign(abs(st) ** (0.62 if ct * fold > 0 else 0.35), st))
                if top_layer and st > 0.4:                    # as rugas do topo, saindo das cintas
                    zz += 0.01 * math.sin(u * 21.0 + yy * 11.0 + seed) * (1 - pc)
                x = L * (u - 0.5) + 0.012 * math.sin(yy * 7.0 + li + seed) * (1 if i in (0, n) else 0)
                ring.append(tuple(C + ex * x + ey * yy + ez * zz))
            rings.append(ring)

        def cap(ring, sgn):
            cen = sum((Vector(q) for q in ring), Vector()) / len(ring)
            return [tuple(cen + (Vector(q) - cen) * 0.5 + ex * (sgn * 0.015)) for q in ring]
        rings = [cap(rings[0], -1)] + rings + [cap(rings[-1], 1)]
        p.loft(m + '#cloth', rings, bevel=0.0, smooth=True)
    # as cintas: a faixa que abraça a pilha estrangulada, e a fivela
    Ht = z0s[-1] + hs[-1]
    for s_ in su:
        x = L * (s_ - 0.5)

        def env(grow):
            out = []
            hh = Ht * (1 - 0.16) * droop(s_) + grow
            ww = W / 2 * (1 - 0.08) + grow
            for j in range(k):
                th = 2 * math.pi * j / k
                ct, st = math.cos(th), math.sin(th)
                out.append((ww * math.copysign(abs(ct) ** 0.4, ct), hh / 2 * (1 + math.copysign(abs(st) ** 0.4, st)) - (grow if st < 0 else 0)))
            return out
        o, i_ = env(0.008), env(-0.004)
        _ring_band(p, strap_m, [tuple(C + ex * (x - 0.022) + ey * a_ + ez * b_) for a_, b_ in o],
                   [tuple(C + ex * (x + 0.022) + ey * a_ + ez * b_) for a_, b_ in o],
                   [tuple(C + ex * (x + 0.022) + ey * a_ + ez * b_) for a_, b_ in i_],
                   [tuple(C + ex * (x - 0.022) + ey * a_ + ez * b_) for a_, b_ in i_])
        top = C + ex * x + ez * (Ht * (1 - 0.16) * droop(s_) + 0.014) + ey * (W * 0.2)
        _obox_frame(p, buckle_m, top, ex, ey, ez, 0.065, 0.06, 0.016)


def _ring_band(p, m, a0, a1, b1, b0):
    """A faixa fechada entre 4 anéis (fora em a0→a1, dentro em b0→b1): a cinta com espessura."""
    b = p.bm(_k(m), 0.0)
    A0 = [b.verts.new(V(*q)) for q in a0]
    A1 = [b.verts.new(V(*q)) for q in a1]
    B1 = [b.verts.new(V(*q)) for q in b1]
    B0 = [b.verts.new(V(*q)) for q in b0]
    n = len(a0)
    for i in range(n):
        k = (i + 1) % n
        for r0, r1 in ((A0, A1), (A1, B1), (B1, B0), (B0, A0)):
            f = b.faces.new([r0[i], r0[k], r1[k], r1[i]])
            f.smooth = True


def tarp_drape(p, top_left, top_right, drop, out_n, m='Team', folds=5, amp=0.02, thick=0.014, seed=0.0, nu=40, nv=9,
               hem=True, tie_m=None, ties=(0.1, 0.5, 0.9)):
    """A aba de lona CAÍDA (pendurada na grade): presa em cima nos nós (ties) entre top_left e top_right, faz a BARRIGA entre um nó e
    outro (o pano cede), cai 'drop' metros com as dobras de pé IRREGULARES (3 frequências com fase sorteada: nada de ripa), que
    crescem pra baixo, a barra ondulada e mais pesada; o lado de fora pra out_n. tie_m: os nós de corda."""
    A, B = Vector(top_left), Vector(top_right)
    n = Vector(out_n).normalized()
    down = Vector((0, 0, -1))
    width = (B - A).length
    eu = (B - A).normalized()
    ph = [_hash1(seed * 3.1 + i) * 6.283 for i in range(4)]

    def wav(u):
        return (math.sin(2 * math.pi * folds * u + ph[0]) + 0.5 * math.sin(2 * math.pi * folds * 1.73 * u + ph[1])
                + 0.25 * math.sin(2 * math.pi * folds * 3.1 * u + ph[2]))

    def swag(u):
        """A barriga da borda de cima entre os nós (0 no nó)."""
        ts = sorted(ties)
        if len(ts) < 2 or u <= ts[0] or u >= ts[-1]:
            return 0.0
        for a_, b_ in zip(ts[:-1], ts[1:]):
            if a_ <= u <= b_:
                return 0.035 * math.sin(math.pi * (u - a_) / (b_ - a_))
        return 0.0

    def pos(u, v, extra=0.0):
        d = amp * (0.35 + 0.9 * v) * wav(u) + swag(u) * 0.5 * (1 - v)
        hang = swag(u) * (1 - v) + drop * v * (1 - 0.05 * math.sin(math.pi * u) * v) + 0.015 * math.sin(7 * u + ph[3]) * v ** 2
        return A + eu * (width * u) + down * hang + n * (d + 0.012 * v ** 2 + 0.004 + extra)

    b = p.bm(m + '#cloth', 0.0)
    front, back = [], []
    for j in range(nv + 1):
        v = j / nv
        front.append([b.verts.new(V(*(pos(i / nu, v) + n * thick / 2))) for i in range(nu + 1)])
        back.append([b.verts.new(V(*(pos(i / nu, v) - n * thick / 2))) for i in range(nu + 1)])
    for j in range(nv):
        for i in range(nu):
            _quad(b, [front[j][i], front[j][i + 1], front[j + 1][i + 1], front[j + 1][i]], True)
            _quad(b, [back[j + 1][i], back[j + 1][i + 1], back[j][i + 1], back[j][i]], True)
    edge = [(0, i) for i in range(nu)] + [(j, nu) for j in range(nv)] + [(nv, i) for i in range(nu, 0, -1)] + [(j, 0) for j in range(nv, 0, -1)]
    for (j0, i0), (j1, i1) in zip(edge, edge[1:] + edge[:1]):
        _quad(b, [front[j0][i0], back[j0][i0], back[j1][i1], front[j1][i1]], True)
    if hem:            # a bainha (a barra dobrada, mais grossa)
        tube(p, m + '#cloth', [pos(i / nu, 1.0, 0.006) for i in range(nu + 1)], thick * 0.95, seg=6)
    if tie_m:
        for u in ties:
            q = pos(u, 0.0) + n * 0.012
            p.torus(_k(tie_m), tuple(q), 'x' if abs(eu.x) < 0.5 else 'y', 0.032, 0.01, seg=10, rseg=6)


def tarp_roll(p, c, axis, length, r, m='Team', straps=3, strap_m='Cloth', buckle_m='Metal', rope_ties=False):
    """A lona ENROLADA (o rolo amarrado), eixo 'x' ou 'y': estufa entre as amarras, a seção é a ESPIRAL (o degrau da última volta),
    as pontas com os anéis das voltas em relevo (lê como pano enrolado, não como cano). As amarras: a cinta com a fivela, ou
    rope_ties=True as voltas de corda."""
    ax = Vector((1, 0, 0)) if axis == 'x' else Vector((0, 1, 0))
    e1 = Vector((0, 0, 1))
    e2 = ax.cross(e1)
    C = Vector(c)
    n, ks = 18, 24
    su = [((s_ + 0.5) / straps) for s_ in range(straps)]
    rings = []
    for i in range(n + 1):
        u = i / n
        pc = max(math.exp(-((u - s_) / 0.05) ** 2) for s_ in su)
        rr = r * (0.97 + 0.05 * math.sin(math.pi * u) ** 0.5) * (1 - 0.1 * pc)
        x = (u - 0.5) * length
        ring = []
        for j in range(ks):
            th = 2 * math.pi * j / ks
            sp = 0.95 + 0.06 * (((th - 4.4) % (2 * math.pi)) / (2 * math.pi))
            ring.append(tuple(C + ax * x + e1 * (rr * sp * math.sin(th)) * 0.94 + e2 * (rr * sp * math.cos(th))))
        rings.append(ring)
    p.loft(m + '#cloth', rings, bevel=0.0, smooth=True)
    for s_ in su:
        x = (s_ - 0.5) * length
        rs = r * 0.9 * 0.97
        if rope_ties:
            for k in range(3):
                p.torus(_k(strap_m), tuple(C + ax * (x + (k - 1) * 0.026)), axis, rs + 0.008, 0.012, seg=18, rseg=5)
        else:
            lathe(p, strap_m, C + ax * x, ax, [(rs - 0.01, -0.022), (rs + 0.012, -0.022), (rs + 0.012, 0.022), (rs - 0.01, 0.022)],
                  seg=24, smooth=True, closed=True)
            rbox(p, buckle_m, C + ax * x + e1 * (rs + 0.016), (0.06, 0.06, 0.016), yaw=0 if axis == 'x' else 90, r=0.01, k=1)
    for sgn in (-1, 1):
        e = length / 2 * 0.985
        rr = r * 0.88
        prof = [(0.0, e - 0.03), (rr * 0.18, e - 0.028), (rr * 0.25, e - 0.006), (rr * 0.36, e - 0.02), (rr * 0.46, e - 0.004),
                (rr * 0.58, e - 0.018), (rr * 0.68, e - 0.002), (rr * 0.8, e - 0.015), (rr * 0.9, e), (rr, e - 0.012),
                (rr, e - 0.08), (0.0, e - 0.08)]
        lathe(p, m + '#cloth', C, ax * sgn, prof, seg=24, smooth=True)


def rope(p, m, pts, r=0.012, sag=0.03, seg=6, n=10):
    """A corda/cabo entre pontos, com a barriga (catenária aproximada) em cada vão."""
    pts = [Vector(q) for q in pts]
    out = [pts[0]]
    for a, b in zip(pts[:-1], pts[1:]):
        for i in range(1, n + 1):
            t = i / n
            q = a + (b - a) * t
            q.z -= sag * 4 * t * (1 - t) * min(1.0, (b - a).length)
            out.append(q)
    tube(p, m, out, r, seg=seg)


def tow_cable(p, m, pts, r=0.022, eye_m=None, seg=8):
    """O cabo de reboque (o cabo de aço grosso, ≥ 4 cm) com os olhais nas pontas."""
    pts = [Vector(q) for q in pts]
    tube(p, m, bent_bar(pts, R=0.12, n=5), r, seg=seg)
    em = eye_m or m
    for q, d in ((pts[0], pts[1] - pts[0]), (pts[-1], pts[-2] - pts[-1])):
        d = d.normalized()
        # a luva prensada e o olhal (o laço)
        p.cyl(_k(em), tuple(q + d * 0.06), tuple(d), r * 1.35, 0.1, seg=8, bevel=0.0)
        _, e1, _ = axis_frame(d)
        p.torus(_k(em), tuple(q - d * 0.05), _axis_name(e1), 0.065, r * 0.85, seg=14, rseg=6)


def _axis_name(e):
    e = Vector(e)
    k = max(range(3), key=lambda i: abs(e[i]))
    return 'xyz'[k]


# ===================================================================================================== equipamento
def jerrycan_frame(p, o, ex, ey, ez, m='Metal', cap_m='Dark', s=1.0):
    """O galão de 20 L num referencial dado, o PÉ em o: ex = o comprimento (0,34), ey = a espessura (0,165), ez = a altura (0,47)."""
    ex, ey, ez = Vector(ex).normalized(), Vector(ey).normalized(), Vector(ez).normalized()
    C = Vector(o) + ez * (0.235 * s)
    W, D, H = 0.34 * s, 0.165 * s, 0.47 * s
    prism(p, m + '#round' if '#' not in m else m, rounded_rect(W, D, 0.035 * s, 3), C - ez * (H / 2), ex, ey, ez, H * 0.93, bevel=0.01)
    for sgn in (-1, 1):
        for d in (-1, 1):
            q = C + ey * (sgn * (D / 2 + 0.004)) - ez * 0.02 * s
            a = math.atan2(H * 0.7, W * 0.75) * d
            eu = ex * math.cos(a) + ez * math.sin(a)
            ew = ez * math.cos(a) - ex * math.sin(a)
            _obox_frame(p, m, q, eu, ey, ew, math.hypot(W * 0.75, H * 0.7), 0.01, 0.03 * s)
    top = C + ez * (H * 0.43)
    for k in (-1, 0, 1):
        tube(p, m, bent_bar([top + ex * (k * 0.08 * s - 0.03 * s) - ez * 0.01, top + ex * (k * 0.08 * s - 0.03 * s) + ez * 0.05 * s,
                             top + ex * (k * 0.08 * s + 0.03 * s) + ez * 0.05 * s, top + ex * (k * 0.08 * s + 0.03 * s) - ez * 0.01],
                            R=0.015, n=2), 0.011 * s, seg=6)
    p.cyl(_k(cap_m), tuple(top + ex * (W * 0.36) + ez * 0.03 * s), tuple(ez), 0.03 * s, 0.06 * s, seg=10, bevel=0.0)


def jerrycan(p, c, yaw=0.0, m='Metal', cap_m='Dark', s=1.0, lying=False):
    """O galão de 20 L (0,34 × 0,165 × 0,47 m): o corpo de quinas redondas, o X estampado em relevo dos dois lados, as 3 alças em
    cima e a tampa. lying=True deita de lado."""
    ex, ey, ez = ypr_frame(yaw, 0.0, 90.0 if lying else 0.0)
    C = Vector(c)
    W, D, H = 0.34 * s, 0.165 * s, 0.47 * s
    prism(p, m + '#round', rounded_rect(W, D, 0.035 * s, 3), C - ez * (H / 2), ex, ey, ez, H * 0.93, bevel=0.01)
    # o X em relevo (as duas diagonais) nas faces largas
    for sgn in (-1, 1):
        for d in (-1, 1):
            q = C + ey * (sgn * (D / 2 + 0.004)) - ez * 0.02 * s
            a = math.atan2(H * 0.7, W * 0.75) * d
            eu = ex * math.cos(a) + ez * math.sin(a)
            ew = ez * math.cos(a) - ex * math.sin(a)
            _obox_frame(p, m, q, eu, ey, ew, math.hypot(W * 0.75, H * 0.7), 0.01, 0.03 * s)
    # as alças (3 barras) e a tampa
    top = C + ez * (H * 0.43)
    for k in (-1, 0, 1):
        tube(p, m, bent_bar([top + ex * (k * 0.08 * s - 0.03 * s) - ez * 0.01, top + ex * (k * 0.08 * s - 0.03 * s) + ez * 0.05 * s,
                             top + ex * (k * 0.08 * s + 0.03 * s) + ez * 0.05 * s, top + ex * (k * 0.08 * s + 0.03 * s) - ez * 0.01],
                            R=0.015, n=2), 0.011 * s, seg=6)
    p.cyl(_k(cap_m), tuple(top + ex * (W * 0.36) + ez * 0.03 * s), tuple(ez), 0.03 * s, 0.06 * s, seg=10, bevel=0.0)


def ammo_box(p, c, size=(0.42, 0.2, 0.26), yaw=0.0, pitch=0.0, m='Metal', latch_m='Dark'):
    """A caixa de munição: o corpo, a tampa com a aba (a fresta da tampa), a alça de lado e as travas."""
    ex, ey, ez = ypr_frame(yaw, pitch, 0.0)
    C = Vector(c)
    L, W, H = size
    prism(p, m + '#round', rounded_rect(L, W, 0.02, 2), C - ez * (H / 2), ex, ey, ez, H * 0.78, bevel=0.01)
    prism(p, m + '#round', rounded_rect(L + 0.016, W + 0.016, 0.024, 2), C + ez * (H * 0.3), ex, ey, ez, H * 0.2, bevel=0.01)  # a tampa
    tube(p, latch_m, bent_bar([C + ez * (H / 2) - ex * 0.08, C + ez * (H / 2 + 0.035) - ex * 0.06, C + ez * (H / 2 + 0.035) + ex * 0.06,
                               C + ez * (H / 2) + ex * 0.08], R=0.02, n=2), 0.011, seg=6)
    for sgn in (-1, 1):
        _obox_frame(p, latch_m, C + ex * (sgn * (L / 2 + 0.008)) + ez * (H * 0.22), ex, ey, ez, 0.02, 0.06, 0.08)


def shovel(p, c, yaw=0.0, pitch=0.0, roll=0.0, handle_m='Dark', blade_m='Metal', length=1.05):
    """A pá presa deitada (o cabo de 4 cm, a lâmina de bico com a borda virada) e as duas presilhas."""
    ex, ey, ez = ypr_frame(yaw, pitch, roll)
    C = Vector(c)
    h0, h1 = C - ex * (length / 2), C + ex * (length / 2 - 0.3)
    tube(p, handle_m, [h0, h1], 0.02, seg=8)
    # o punho em D
    tube(p, handle_m, bent_bar([h0, h0 - ex * 0.06 + ey * 0.06, h0 - ex * 0.14 + ey * 0.05, h0 - ex * 0.14 - ey * 0.05,
                                h0 - ex * 0.06 - ey * 0.06, h0], R=0.02, n=2), 0.013, seg=6)
    # a lâmina (o bico de pá cortadeira) levemente côncava
    bl = [(0.0, -0.11), (0.2, -0.12), (0.27, -0.06), (0.31, 0.0), (0.27, 0.06), (0.2, 0.12), (0.0, 0.11)]
    prism(p, blade_m, bl, h1 - ez * 0.006, ex, ey, ez, 0.012)
    p.cyl(_k(blade_m), tuple(h1 + ex * 0.02), tuple(ex), 0.03, 0.08, seg=8, bevel=0.0)


def pickaxe(p, c, yaw=0.0, pitch=0.0, roll=0.0, handle_m='Dark', head_m='Metal', length=0.9, head=0.3):
    """A picareta deitada: o cabo e a cabeça de duas pontas curvas."""
    ex, ey, ez = ypr_frame(yaw, pitch, roll)
    C = Vector(c)
    tube(p, handle_m, [C - ex * (length / 2), C + ex * (length / 2)], 0.022, seg=8)
    hc = C + ex * (length / 2 - 0.05)
    pts = [hc + ey * (head * math.sin(math.radians(a))) + ex * (-0.06 * (1 - math.cos(math.radians(a))) * 2.4) for a in range(-90, 91, 20)]
    tube(p, head_m, pts, 0.022, seg=6, r_fn=lambda i: 0.026 * (1 - abs(i - 4.5) / 6.0))


def crowbar(p, c, yaw=0.0, pitch=0.0, roll=0.0, m='Metal', length=1.2):
    ex, ey, ez = ypr_frame(yaw, pitch, roll)
    C = Vector(c)
    a, b = C - ex * (length / 2), C + ex * (length / 2)
    tube(p, m, bent_bar([a + ey * 0.06, a, b, b + ey * 0.05 + ex * 0.03], R=0.04, n=3), 0.016, seg=6)


def jack(p, c, m='Metal', dark='Dark', h=0.32):
    """O macaco de garrafa em pé: o corpo, o pistão e a base."""
    c = Vector(c)
    lathe(p, m, c, 'z', [(0.0, 0.0), (0.09, 0.0), (0.09, 0.03), (0.06, 0.04), (0.06, h * 0.7), (0.045, h * 0.72), (0.035, h * 0.72),
                         (0.035, h), (0.05, h), (0.05, h + 0.02), (0.0, h + 0.02)], seg=14, smooth=True)
    p.box(_k(dark), c.x - 0.09, c.x + 0.09, c.y - 0.09, c.y + 0.09, c.z, c.z + 0.03, bevel=0.0)


def drum(p, c, axis, r, length, m='Metal', hoop_m=None, seg=28, lying=True, cap_m=None):
    """O tambor de 200 L: o corpo com os dois aros de rolamento, as bordas dobradas e o bujão."""
    A, E1, E2 = axis_frame(axis)
    hl = length / 2
    prof = [(0.0, -hl + 0.01), (r * 0.93, -hl + 0.01), (r * 0.97, -hl - 0.004), (r + 0.006, -hl + 0.01), (r, -hl + 0.03),
            (r, -hl * 0.36 - 0.02), (r + 0.016, -hl * 0.36), (r, -hl * 0.36 + 0.02), (r, hl * 0.36 - 0.02), (r + 0.016, hl * 0.36),
            (r, hl * 0.36 + 0.02), (r, hl - 0.03), (r + 0.006, hl - 0.01), (r * 0.97, hl + 0.004), (r * 0.93, hl - 0.01), (0.0, hl - 0.01)]
    lathe(p, m, c, A, prof, seg=seg, smooth=True)
    if cap_m:
        q = Vector(c) + A * (hl - 0.005) + E1 * (r * 0.6)
        p.cyl(_k(cap_m), tuple(q), tuple(A), 0.035, 0.02, seg=8, bevel=0.0)


def hatch_round(p, c, r, m, rim_m, n=(0, 0, 1), hinge_dir=(0, 1, 0), handle_m=None, dome=0.035, seg=28):
    """A escotilha redonda: o aro em relevo (a borda soldada), a tampa abaulada com a aba, a dobradiça de nós e a alça em U."""
    A, _, _ = axis_frame(n)
    C = Vector(c)
    lathe(p, rim_m, C, A, [(r - 0.01, 0.0), (r + 0.05, 0.0), (r + 0.045, 0.028), (r + 0.03, 0.04), (r + 0.004, 0.04)], seg=seg,
          smooth=True, closed=True)
    lathe(p, m, C, A, [(0.0, 0.065 + dome), (r * 0.5, 0.062 + dome * 0.8), (r * 0.85, 0.06 + dome * 0.3), (r - 0.008, 0.058),
                       (r, 0.05), (r, 0.03), (0.0, 0.03)], seg=seg, smooth=True)
    hd = Vector(hinge_dir).normalized()
    side = hd.cross(A).normalized()
    hinge(p, rim_m, C + hd * (r + 0.04) + A * 0.06, side, r * 0.9, r=0.028, knuckles=3)
    if handle_m:
        grab_handle(p, handle_m, C - side * 0.1 + A * (0.06 + dome * 0.6), C + side * 0.1 + A * (0.06 + dome * 0.6), A, height=0.05, r=0.012, R=0.02)


def mudflap(p, c, width, height, yaw=0.0, m='Rubber', t=0.02, folds=2):
    """O para-barro de borracha pendurado (com espessura e uma onda leve), preso na barra de cima."""
    ex, ey, ez = ypr_frame(yaw, 0.0, 0.0)
    C = Vector(c)
    b = p.bm(_k(m), 0.0)
    nu, nv = 10, 4
    F, Bk = [], []
    for j in range(nv + 1):
        v = j / nv
        rf, rb = [], []
        for i in range(nu + 1):
            u = i / nu
            q = C + ey * (width * (u - 0.5)) - ez * (height * v) + ex * (0.012 * math.sin(math.pi * folds * u) * v + 0.02 * v * v)
            rf.append(b.verts.new(V(*(q + ex * t / 2))))
            rb.append(b.verts.new(V(*(q - ex * t / 2))))
        F.append(rf)
        Bk.append(rb)
    for j in range(nv):
        for i in range(nu):
            _quad(b, [F[j][i], F[j][i + 1], F[j + 1][i + 1], F[j + 1][i]], True)
            _quad(b, [Bk[j + 1][i], Bk[j + 1][i + 1], Bk[j][i + 1], Bk[j][i]], True)
    edge = [(0, i) for i in range(nu)] + [(j, nu) for j in range(nv)] + [(nv, i) for i in range(nu, 0, -1)] + [(j, 0) for j in range(nv, 0, -1)]
    for (j0, i0), (j1, i1) in zip(edge, edge[1:] + edge[:1]):
        _quad(b, [F[j0][i0], Bk[j0][i0], Bk[j1][i1], F[j1][i1]], True)


def panel_seam(p, m, a, b, n, w=0.025, d=0.012):
    """O sulco entre chapas (2–3 cm, escuro): a tira afundada na face de normal n, de a até b (a máscara de fresta escurece)."""
    a, b, n = Vector(a), Vector(b), Vector(n).normalized()
    eu = (b - a).normalized()
    ev = n.cross(eu).normalized()
    _obox_frame(p, m, (a + b) / 2 - n * (d / 2 - 0.002), eu, ev, n, (b - a).length, w, d)


def chute(p, m, pts, w, h, pitch=0.08):
    """A calha de munição articulada: os gomos (caixas de quinas redondas, com a fresta entre eles) ao longo da polilinha."""
    pts = [Vector(q) for q in pts]
    seglen = [(b - a).length for a, b in zip(pts[:-1], pts[1:])]
    total = sum(seglen)
    n = max(2, int(total / pitch))
    for i in range(n):
        t = (i + 0.5) / n * total
        k = 0
        while k < len(seglen) - 1 and t > seglen[k]:
            t -= seglen[k]
            k += 1
        a, b = pts[k], pts[k + 1]
        d = (b - a).normalized()
        q = a + d * min(t, seglen[k])
        ref = Vector((0, 0, 1)) if abs(d.z) < 0.9 else Vector((1, 0, 0))
        e2 = ref.cross(d).normalized()
        e3 = d.cross(e2).normalized()
        prism(p, m, rounded_rect(w, h, 0.02, 2), q - d * (pitch * 0.42), e2, e3, d, pitch * 0.84)


def weld_bead(p, m, pts, r=0.016):
    """O cordão de solda achatado ao longo da junta (3 cm), só nas juntas grandes."""
    tube(p, m, pts, r, seg=6)


# ===================================================================================================== chapas grandes (01/11, os B da USA)
def rbox_q(p, m, c, size, yaw=0.0, pitch=0.0, roll=0.0, r=0.03, k=3, bevel=0.0, frame=None):
    """O rbox com as TAMPAS EM QUADS (o retângulo do meio, as 4 faixas da borda e os cantos em leque), no lugar do polígono único:
    a grade do executor (model_detail.grid) só enche de pontos as faces de 4 lados, e a tampa de n lados virava um leque de
    triângulos compridos que riscava a máscara (a quina pintava a chapa inteira em listras). Mesma assinatura e a mesma forma do
    rbox; use nas chapas grandes (lado > ~0,3 m). frame=(ex, ey, ez): o referencial dado (no lugar do giro)."""
    ex, ey, ez = (Vector(frame[0]), Vector(frame[1]), Vector(frame[2])) if frame else ypr_frame(yaw, pitch, roll)
    C = Vector(c)
    w, h, d = size
    r = max(0.004, min(r, w / 2 - 1e-3, h / 2 - 1e-3))
    b = p.bm(_k(m), bevel)
    cs = ((w / 2 - r, h / 2 - r, 0), (-w / 2 + r, h / 2 - r, 90), (-w / 2 + r, -h / 2 + r, 180), (w / 2 - r, -h / 2 + r, 270))

    def P(u, v, s):
        return b.verts.new(V(*(C + ex * u + ey * v + ez * (s * d / 2))))
    outs, ins = {}, {}
    for s in (-1, 1):
        o_ring, i_pts = [], []
        for (cx, cy, a0) in cs:
            arc = [P(cx + r * math.cos(math.radians(a0 + 90 * j / k)), cy + r * math.sin(math.radians(a0 + 90 * j / k)), s)
                   for j in range(k + 1)]
            o_ring.append(arc)
            i_pts.append(P(cx, cy, s))
        outs[s], ins[s] = o_ring, i_pts
        fl = (lambda q: q) if s > 0 else (lambda q: q[::-1])
        for i in range(4):
            arc, I = o_ring[i], i_pts[i]
            for j in range(k):
                b.faces.new(fl([I, arc[j], arc[j + 1]]))
            nxt = (i + 1) % 4
            b.faces.new(fl([arc[k], o_ring[nxt][0], i_pts[nxt], I]))
        b.faces.new(fl(i_pts))
    # as paredes (o contorno de cima ligado ao de baixo)
    lo = [v for arc in outs[-1] for v in arc]
    hi = [v for arc in outs[1] for v in arc]
    n = len(lo)
    for i in range(n):
        j = (i + 1) % n
        if (lo[i].co - lo[j].co).length < 1e-7:
            continue
        b.faces.new([lo[i], lo[j], hi[j], hi[i]])


def poke_ngons(p, min_area=0.02):
    """Cutuca (um ponto no centro, leque de triângulos) as faces planas de mais de 4 lados e área > min_area de todas as malhas da
    peça, ANTES do finish (do chanfro): a grade do executor corta os raios compridos e enche a face de pontos (a máscara não risca).
    Só pra faces convexas (as tampas do solid, do prism de retângulo redondo)."""
    import bmesh
    for key, b in p.bms.items():
        fs = [f for f in b.faces if len(f.verts) > 4 and f.calc_area() > min_area]
        if fs:
            bmesh.ops.poke(b, faces=fs)
