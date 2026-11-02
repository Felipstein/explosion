"""As peças de detalhe das aeronaves provisórias por script, versão B (01/11/2026; o estudo: docs/ESTUDO_DETALHE_AAA.md §3, "Avião e
helicóptero"): Raptor, Apache (usa_comanche), Chinook e o avião de carga.

O que o estudo pede pra aeronave e o que cada peça daqui resolve:
  1. "fuselagem sem linhas de painel": Section + skin (a fuselagem por seções interpoladas, lisa, com os SULCOS de painel de verdade
     em anel, onde a máscara de fresta escurece) e ribbon (a linha de painel corrida na pele, a junta comprida);
  2. "pás retas de tábua": blade (a pá por perfis, que afina na ponta, torce e tem a ponta enflechada);
  3. "entrada de ar, carenagem e trem sem AO": duct (a boca com lábio e o duto escuro que entra no corpo), oleo (a perna com o
     amortecedor cromado e o compasso), air_wheel (o pneu de avião liso com os frisos e a roda de cubo);
  4. "janelas sem moldura": window_frame (a moldura escura em volta do vidro);
  5. os mísseis com forma: missile (a ogiva, o corpo, o fundo, as aletas de perfil afinado).

Tudo nas coordenadas da Unreal (X frente, Y direita, Z cima), em METROS DO DESENHO (o tamanho real da aeronave): o script multiplica
pela escala do ar (0,58) no fim. Por isso cada medida de detalhe que tem de ler na câmera vem do tamanho FINAL dividido pela escala
(o parâmetro F = 1 / escala nas funções que precisam). Não mexe no model_lib.py, no model_air_lib.py nem no model_vehicle_parts.py.
"""
import math

from mathutils import Vector

from model_lib import V
import model_vehicle_parts as vp

# o perfil das pás e das aletas: o NACA simétrico grosso na frente, fino atrás (fração da corda, meia espessura relativa)
FOIL = [(0.0, 0.0), (0.03, 0.42), (0.1, 0.78), (0.25, 1.0), (0.45, 0.92), (0.7, 0.58), (0.92, 0.2), (1.0, 0.04),
        (1.0, -0.04), (0.92, -0.2), (0.7, -0.58), (0.45, -0.92), (0.25, -1.0), (0.1, -0.78), (0.03, -0.42)]


# ===================================================================================================== interpolação
def catmull(rows, n):
    """As estações (tuplas de números, a 1ª coluna é o x) interpoladas por Catmull-Rom: n passos entre cada par (a fuselagem lisa no
    comprimento, sem os vincos das estações do A)."""
    out = []
    m = len(rows)
    for i in range(m - 1):
        a0, a1, a2, a3 = rows[max(i - 1, 0)], rows[i], rows[i + 1], rows[min(i + 2, m - 1)]
        for k in range(n):
            t = k / n
            out.append(tuple(0.5 * ((2 * a1[j]) + (-a0[j] + a2[j]) * t + (2 * a0[j] - 5 * a1[j] + 4 * a2[j] - a3[j]) * t * t
                                    + (-a0[j] + 3 * a1[j] - 3 * a2[j] + a3[j]) * t ** 3) for j in range(len(a1))))
    out.append(tuple(rows[-1]))
    return out


def lerp_rows(rows, x):
    """A linha da tabela (x decrescente na 1ª coluna) no x pedido, linear entre as vizinhas; fora da tabela, a da ponta."""
    if x >= rows[0][0]:
        return rows[0]
    if x <= rows[-1][0]:
        return rows[-1]
    for a, b in zip(rows[:-1], rows[1:]):
        if b[0] <= x <= a[0]:
            t = (a[0] - x) / max(a[0] - b[0], 1e-9)
            return tuple(a[j] + (b[j] - a[j]) * t for j in range(len(a)))
    return rows[-1]


def _se(c, p):
    return math.copysign(abs(c) ** (2.0 / p), c)


# ===================================================================================================== a fuselagem
class Section:
    """A fuselagem por seções de superelipse: estações (x, largura, altura, z do centro, expoente[, y do centro]), x decrescente
    (do nariz pra cauda). smooth=n: interpoladas por Catmull-Rom (n passos por vão). P(x, θ) é o ponto da pele (θ = 0 no lado +Y,
    sobe por cima: 90° = o topo, 270° = a barriga); N(x, θ) a normal pra fora (pela derivada: a pele inclinada do nariz conta)."""

    def __init__(self, stations, smooth=6):
        rows = [tuple(s) + ((0.0,) if len(s) == 5 else ()) for s in stations]
        self.rows = catmull(rows, smooth) if smooth else rows
        self.x0, self.x1 = self.rows[0][0], self.rows[-1][0]

    def at(self, x):
        return lerp_rows(self.rows, x)

    def P(self, x, th):
        _, w, h, zc, pe, yc = self.at(x)
        return Vector((x, yc + (w / 2) * _se(math.cos(th), pe), zc + (h / 2) * _se(math.sin(th), pe)))

    def N(self, x, th):
        e, ex = 0.004, 0.01
        tt = self.P(x, th + e) - self.P(x, th - e)
        tx = self.P(x + ex, th) - self.P(x - ex, th)
        n = tt.cross(tx)
        if n.length < 1e-9:
            n = Vector((0.0, math.cos(th), math.sin(th)))
        n.normalize()
        _, _, _, zc, _, yc = self.at(x)
        if n.dot(self.P(x, th) - Vector((x, yc, zc))) < 0:
            n = -n
        return n

    def Q(self, x, th, out=0.0):
        return self.P(x, th) + self.N(x, th) * out

    def ring(self, x, ths, out=0.0):
        return [tuple(self.Q(x, t, out)) for t in ths]


def angles(seg, a0=0.0, a1=2 * math.pi, closed=True):
    return [a0 + (a1 - a0) * k / seg for k in range(seg if closed else seg + 1)]


def sheet(p, m, rings, closed=True, smooth=True, caps=(False, False), hard_rows=(), hard_cols=(), bevel=0.0):
    """Os anéis ligados (closed: o anel fecha em volta; senão a tira aberta), com as tampas, as linhas duras de anel (hard_rows: o
    fundo e os lábios do sulco) e as de comprimento (hard_cols: a coluna i liga o ponto i ao i+1 de cada anel)."""
    b = p.bm(m, bevel)
    vr = [[b.verts.new(V(*q)) for q in ring] for ring in rings]
    n = len(rings[0])
    span = n if closed else n - 1
    for r0, r1 in zip(vr[:-1], vr[1:]):
        for i in range(span):
            j = (i + 1) % n
            f = b.faces.new([r0[i], r0[j], r1[j], r1[i]])
            f.smooth = smooth
    if caps[0]:
        b.faces.new(list(reversed(vr[0])))
    if caps[1]:
        b.faces.new(vr[-1])
    for ri in hard_rows:
        if 0 <= ri < len(vr):
            for i in range(span):
                e = b.edges.get([vr[ri][i], vr[ri][(i + 1) % n]])
                if e is not None:
                    e.smooth = False
    for ci in hard_cols:
        for r0, r1 in zip(vr[:-1], vr[1:]):
            e = b.edges.get([r0[ci % n], r1[ci % n]])
            if e is not None:
                e.smooth = False
    return vr


def skin(p, m, sec, xs, ths, grooves=(), closed=True, caps=(True, True), smooth=True, hard_cols=()):
    """A pele da fuselagem nos x pedidos, com os SULCOS de painel em anel: grooves = [(x, largura, fundura)], cada um vira 3 anéis (o
    lábio, o fundo afundado na normal, o lábio), com as linhas duras: a junta das chapas que pega o sol de um lado e a sombra do outro
    (e a máscara de fresta põe o pó). Devolve os x usados."""
    pts = [(x, 0.0, False) for x in xs]
    for xg, w, d in grooves:
        pts = [q for q in pts if abs(q[0] - xg) > w * 0.75]
        pts += [(xg + w / 2, 0.0, True), (xg, -d, True), (xg - w / 2, 0.0, True)]
    pts.sort(key=lambda q: -q[0])
    rings = [sec.ring(x, ths, out) for x, out, _ in pts]
    hard = [i for i, q in enumerate(pts) if q[2]]
    sheet(p, m, rings, closed=closed, smooth=smooth, caps=caps, hard_rows=hard, hard_cols=hard_cols)
    return [q[0] for q in pts]


def ribbon(p, m, pts, nrms, w, lift=0.003, t=0.006):
    """A tira colada na pele (a linha de painel corrida, a faixa da cor do jogador fina): pts e nrms na pele, largura w; a face de
    cima fica 'lift' acima da pele e a de baixo 't' abaixo (não deixa fresta com a pele curva)."""
    pts = [Vector(q) for q in pts]
    nrms = [Vector(n).normalized() for n in nrms]
    n = len(pts)
    if n < 2:
        return
    rings = []
    for i in range(n):
        d = pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]
        s = d.cross(nrms[i])
        if s.length < 1e-9:
            continue
        s.normalize()
        P, N = pts[i], nrms[i]
        rings.append([tuple(P + s * (w / 2) + N * lift), tuple(P - s * (w / 2) + N * lift), tuple(P - s * (w / 2) - N * t),
                      tuple(P + s * (w / 2) - N * t)])
    sheet(p, vp._k(m), rings, closed=True, smooth=False, caps=(True, True))


def sec_ribbon(p, m, sec, x0, x1, th, w, n=None, lift=0.003):
    """A linha de painel ao comprido na fuselagem (no ângulo θ), de x0 a x1."""
    n = n or max(2, int(abs(x1 - x0) / 0.25) + 1)
    xs = [x0 + (x1 - x0) * k / (n - 1) for k in range(n)]
    ribbon(p, m, [sec.P(x, th) for x in xs], [sec.N(x, th) for x in xs], w, lift=lift)


def sec_arc(p, m, sec, x, th0, th1, w, n=12, lift=0.003):
    """A linha de painel em arco (a junta de anel feita como tira, onde não cabe sulco), de θ0 a θ1 no x."""
    ths = [th0 + (th1 - th0) * k / (n - 1) for k in range(n)]
    ribbon(p, m, [sec.P(x, t) for t in ths], [sec.N(x, t) for t in ths], w, lift=lift)


def sec_rect(p, m, sec, x0, x1, th0, th1, w, lift=0.003):
    """O painel de acesso (a tampa) desenhado na pele: o contorno em tira de x0..x1 × θ0..θ1."""
    sec_ribbon(p, m, sec, x0, x1, th0, w, lift=lift)
    sec_ribbon(p, m, sec, x0, x1, th1, w, lift=lift)
    sec_arc(p, m, sec, x0, th0, th1, w, lift=lift)
    sec_arc(p, m, sec, x1, th0, th1, w, lift=lift)


def sec_patch(p, m, sec, x0, x1, th0, th1, nx=8, nt=8, lift=0.004, t=0.008):
    """O retalho colado na pele (o painel da cor do jogador, a chapa antiderrapante): a pele de x0..x1 × θ0..θ1 levantada 'lift',
    com espessura (as bordas fecham)."""
    xs = [x0 + (x1 - x0) * k / nx for k in range(nx + 1)]
    ts = [th0 + (th1 - th0) * k / nt for k in range(nt + 1)]
    top = [[sec.Q(x, th, lift) for th in ts] for x in xs]
    bot = [[sec.Q(x, th, -t) for th in ts] for x in xs]
    b = p.bm(vp._k(m), 0.0)
    T = [[b.verts.new(V(*q)) for q in row] for row in top]
    B = [[b.verts.new(V(*q)) for q in row] for row in bot]
    for i in range(nx):
        for j in range(nt):
            f = b.faces.new([T[i][j], T[i][j + 1], T[i + 1][j + 1], T[i + 1][j]])
            f.smooth = True
            f = b.faces.new([B[i + 1][j], B[i + 1][j + 1], B[i][j + 1], B[i][j]])
            f.smooth = True
    edge = [(0, j) for j in range(nt)] + [(i, nt) for i in range(nx)] + [(nx, j) for j in range(nt, 0, -1)] + [(i, 0) for i in range(nx, 0, -1)]
    for (i0, j0), (i1, j1) in zip(edge, edge[1:] + edge[:1]):
        b.faces.new([T[i0][j0], B[i0][j0], B[i1][j1], T[i1][j1]])


# ===================================================================================================== superfícies (asa, deriva)
class Wing:
    """A asa por estações (x do bordo de ataque, y da envergadura, z, corda, espessura), y crescente, com o perfil FOIL ao longo da
    normal n. P(y, u, top) = o ponto da face de cima (top) ou de baixo na fração u da corda. side = ±1 (o lado espelhado em y)."""

    def __init__(self, stations, n=(0.0, 0.0, 1.0), foil=FOIL):
        self.st = stations
        self.n = Vector(n).normalized()
        self.foil = foil

    def at(self, y):
        st = self.st
        if y <= st[0][1]:
            return st[0]
        for a, b in zip(st[:-1], st[1:]):
            if a[1] <= y <= b[1]:
                t = (y - a[1]) / max(b[1] - a[1], 1e-9)
                return tuple(a[j] + (b[j] - a[j]) * t for j in range(5))
        return st[-1]

    def half(self, u):
        top = [(uu, f) for uu, f in self.foil if f >= 0]
        top.sort()
        for (u0, f0), (u1, f1) in zip(top[:-1], top[1:]):
            if u0 <= u <= u1:
                return f0 + (f1 - f0) * (u - u0) / max(u1 - u0, 1e-9)
        return 0.0

    def P(self, y, u, top=True, side=1, lift=0.0):
        xl, yy, z, c, t = self.at(y)
        f = self.half(u) * (1 if top else -1)
        nn = Vector((self.n.x, self.n.y * side, self.n.z))
        return Vector((xl - u * c, side * yy, z)) + nn * (f * t / 2 + (lift if top else -lift))

    def normal(self, top=True, side=1):
        nn = Vector((self.n.x, self.n.y * side, self.n.z))
        return nn if top else -nn

    def build(self, p, m, sides=(1,), hard_te=True, smooth=True):
        for s in sides:
            nn = Vector((self.n.x, self.n.y * s, self.n.z))
            rings = []
            for xl, y, z, c, t in self.st:
                rings.append([(xl - u * c + nn.x * f * t / 2, s * y + nn.y * f * t / 2, z + nn.z * f * t / 2) for u, f in self.foil])
            hard = [i for i, (u, f) in enumerate(self.foil) if u >= 1.0]
            sheet(p, m, rings, closed=True, smooth=smooth, caps=(True, True), hard_cols=hard if hard_te else ())

    def chord_line(self, p, m, u, y0, y1, w, sides=(1,), top=True, n=6, lift=0.003):
        """A linha da superfície de comando (o flap, o aileron: a fresta ao comprido na fração u da corda), de y0 a y1."""
        for s in sides:
            ys = [y0 + (y1 - y0) * k / (n - 1) for k in range(n)]
            ribbon(p, m, [self.P(y, u, top, s) for y in ys], [self.normal(top, s)] * n, w, lift=lift)

    def span_line(self, p, m, y, u0, u1, w, sides=(1,), top=True, n=6, lift=0.003):
        """A linha que corta a corda em y (a ponta do flap, a junta entre as chapas)."""
        for s in sides:
            us = [u0 + (u1 - u0) * k / (n - 1) for k in range(n)]
            ribbon(p, m, [self.P(y, u, top, s) for u in us], [self.normal(top, s)] * n, w, lift=lift)

    def patch(self, p, m, y0, y1, u0, u1, sides=(1,), top=True, lift=0.004, t=0.006, ny=4, nu=6):
        """O retalho colado na face (a ponta azul, o antiderrapante)."""
        for s in sides:
            b = p.bm(vp._k(m), 0.0)
            ys = [y0 + (y1 - y0) * k / ny for k in range(ny + 1)]
            us = [u0 + (u1 - u0) * k / nu for k in range(nu + 1)]
            nn = self.normal(top, s)
            T = [[b.verts.new(V(*(self.P(y, u, top, s) + nn * lift))) for u in us] for y in ys]
            B = [[b.verts.new(V(*(self.P(y, u, top, s) - nn * t))) for u in us] for y in ys]
            for i in range(ny):
                for j in range(nu):
                    b.faces.new([T[i][j], T[i][j + 1], T[i + 1][j + 1], T[i + 1][j]]).smooth = True
                    b.faces.new([B[i + 1][j], B[i + 1][j + 1], B[i][j + 1], B[i][j]]).smooth = True
            edge = [(0, j) for j in range(nu)] + [(i, nu) for i in range(ny)] + [(ny, j) for j in range(nu, 0, -1)] + [(i, 0) for i in range(ny, 0, -1)]
            for (i0, j0), (i1, j1) in zip(edge, edge[1:] + edge[:1]):
                b.faces.new([T[i0][j0], B[i0][j0], B[i1][j1], T[i1][j1]])


# ===================================================================================================== pás
def blade(p, m, hub, d, r0, r1, chord, thick, twist=(10.0, -2.0), taper=0.7, sweep=0.0, tip_m=None, tip_from=0.92, n=10,
          lead=1, droop=0.0, foil=FOIL, root_chord=None, axis=None):
    """A pá de verdade: perfis FOIL de r0 a r1 ao longo de d (no plano do rotor), a corda que afina pra ponta (taper = a corda da
    ponta ÷ a do meio), a TORÇÃO de twist[0] na raiz a twist[1] na ponta, a ponta enflechada pra trás (sweep, m, nos últimos 8%), o
    bordo de ataque pro lado 'lead' (o sentido do giro), a pá caindo (droop, m na ponta). root_chord: a corda no punho (a raiz mais
    estreita e grossa). tip_m: o material da ponta (a faixa amarela) de tip_from até a ponta."""
    d = Vector(d).normalized()
    if axis is not None:                              # o eixo do rotor dado (o rotor de cauda gira em Y)
        ax = Vector(axis)
    else:
        ax = Vector((0.0, 0.0, 1.0)) if abs(d.z) < 0.9 else Vector((1.0, 0.0, 0.0))
    ax = (ax - d * ax.dot(d)).normalized()           # o eixo do rotor (o "cima" da pá)
    side = ax.cross(d).normalized() * lead            # o bordo de ataque
    H = Vector(hub)
    us = [k / n for k in range(n + 1)]
    if tip_m:
        us = sorted(set([u for u in us if u < tip_from - 0.02] + [tip_from] + [tip_from + (1 - tip_from) * k / 3 for k in range(1, 4)]))
    rings = []
    for u in us:
        r = r0 + (r1 - r0) * u
        c = chord * (1.0 - (1.0 - taper) * max(0.0, (u - 0.75) / 0.25) ** 1.5)
        if root_chord is not None and u < 0.12:
            c = root_chord + (chord - root_chord) * (u / 0.12)
        th = thick * (1.0 - 0.45 * u) * (1.6 if u < 0.06 else 1.0)
        a = math.radians(twist[0] + (twist[1] - twist[0]) * u)
        ch = side * math.cos(a) + ax * math.sin(a)
        up = ax * math.cos(a) - side * math.sin(a)
        sw = sweep * max(0.0, (u - 0.92) / 0.08) ** 1.5
        C = H + d * r - side * sw + ax * (-droop * u * u)
        ring = []
        for fu, ff in foil:
            q = C + ch * ((0.3 - fu) * c) + up * (ff * th / 2)
            ring.append(tuple(q))
        rings.append(ring)
    if tip_m:
        k = us.index(tip_from)
        sheet(p, m, rings[:k + 1], closed=True, smooth=True, caps=(True, False), hard_cols=(7,))
        sheet(p, tip_m, rings[k:], closed=True, smooth=True, caps=(False, True), hard_cols=(7,))
    else:
        sheet(p, m, rings, closed=True, smooth=True, caps=(True, True), hard_cols=(7,))


# ===================================================================================================== bocas, trem, vidro
def duct(p, m_in, outer, inner, depth_dir, depth, back=0.03):
    """A boca com profundidade (a entrada de ar, o bocal): o COPO fechado de parede grossa no material escuro, com a borda na boca
    (o anel de 'outer' a 'inner', dois laços com o mesmo número de pontos) e o túnel que entra 'depth' na direção depth_dir até o
    fundo. Fechado de propósito: as normais saem certas (a parede de dentro olha pro túnel, a de fora fica escondida no corpo)."""
    dd = Vector(depth_dir).normalized()
    o = [Vector(q) for q in outer]
    i = [Vector(q) for q in inner]
    rings = [[tuple(q + dd * (depth + back)) for q in o], [tuple(q) for q in o], [tuple(q) for q in i],
             [tuple(q + dd * depth * 0.4) for q in i], [tuple(q + dd * depth) for q in i]]
    sheet(p, m_in, rings, closed=True, smooth=True, caps=(True, True), hard_rows=(1, 2))


def oleo(p, top, bot, r, m='Metal', chrome=None, link=True, seg=12, F=1.0):
    """A perna do trem: o cilindro do amortecedor (de top até 60%), a haste cromada mais fina saindo embaixo, os anéis e o compasso
    (as duas barras dobradas na frente). chrome: o material da haste (padrão Metal)."""
    top, bot = Vector(top), Vector(bot)
    d = bot - top
    L = d.length
    u = d.normalized()
    cm = chrome or m
    vp.lathe(p, m, top, u, [(0.0, -0.02 * F), (r * 1.15, -0.02 * F), (r * 1.15, 0.05 * F), (r, 0.08 * F), (r, L * 0.6),
                            (r * 1.12, L * 0.6 + 0.01 * F), (r * 1.12, L * 0.6 + 0.04 * F), (r * 0.62, L * 0.6 + 0.05 * F),
                            (0.0, L * 0.6 + 0.05 * F)], seg=seg)
    vp.lathe(p, cm, top, u, [(0.0, L * 0.55), (r * 0.6, L * 0.55), (r * 0.6, L - r * 0.4), (r * 0.75, L - r * 0.3),
                             (r * 0.75, L), (0.0, L)], seg=max(8, seg - 2))
    if link:
        _, e1, _ = vp.axis_frame(u)
        a = top + u * (L * 0.55)
        b = bot - u * (r * 0.5)
        mid = (a + b) / 2 + e1 * (r * 2.2)
        vp.tube(p, m, [a + e1 * r, mid, b + e1 * r * 0.8], r * 0.28, seg=6)


def air_wheel(p, c, r, w, axis='y', side=1, rubber='Rubber', rim='Metal', hub='Metal', seg=28):
    """O pneu de avião: a carcaça abaulada lisa com os 3 frisos da banda, a roda de cubo rebaixada e a tampa do eixo."""
    vp.tyre(p, c, r, w, side, rim_r=r * 0.56, rubber=rubber, rim=rim, hub=hub, seg=seg, lugs=True, lug_h=min(0.012, r * 0.04),
            style='rib', nuts=0, dish=min(0.05, w * 0.25), axis=axis)


def window_frame(p, m, pts, nrm, r, lift=0.0):
    """A moldura escura em volta de um vidro (o laço pts na pele, normal nrm): o tubo de borracha fechado."""
    nrm = Vector(nrm)
    q = [Vector(a) + nrm * lift for a in pts]
    vp.tube(p, m, q + [q[0]], r, seg=6)


def round_window(p, glass, frame, c, n, r, F=1.0):
    """A janela redonda (o Chinook): o vidro levemente fundo e o aro de borracha escuro com a borda."""
    n = Vector(n).normalized()
    C = Vector(c)
    vp.lathe(p, frame, C - n * 0.02 * F, n, [(r * 0.84, 0.0), (r * 1.18, 0.0), (r * 1.22, 0.012 * F), (r * 1.16, 0.026 * F),
                                            (r * 0.95, 0.03 * F), (r * 0.86, 0.022 * F)], seg=20, closed=True)
    vp.lathe(p, glass, C - n * 0.02 * F, n, [(0.0, 0.006 * F), (r * 0.9, 0.006 * F), (r * 0.9, 0.0), (0.0, 0.0)], seg=20)


# ===================================================================================================== mísseis
def missile(p, tip, L, r, body='Paint', nose='Dark', fin='Paint', band='Team', axis=(1.0, 0.0, 0.0), nose_len=None,
            fins_mid=None, fins_tail=None, roll=45.0, band_at=None, seg=16, boat=0.85, seeker=None, F=1.0):
    """O míssil de verdade: a ogiva tangente (nose_len), o corpo, o fundo afunilado com o bocal escuro, a faixa (band_at = a distância
    da ponta), as aletas do meio e as de trás de perfil (fins_* = (distância da ponta à raiz, corda da raiz, corda da ponta, altura,
    espessura)). seeker = o material da cúpula do buscador (o Hellfire)."""
    A = Vector(axis).normalized()
    T = Vector(tip)
    nl = nose_len if nose_len is not None else r * 4.0
    # o perfil girado da ponta (a = 0) pro fundo (a = −L): a ogiva tangente
    prof = []
    for k in range(8):
        t = k / 7
        prof.append((r * math.sqrt(max(0.0, 1.0 - (1.0 - t) ** 2)) if k else 0.0, -nl * t))
    nose_prof = prof
    if seeker:
        vp.lathe(p, seeker, T, A, nose_prof[:4] + [(0.0, nose_prof[3][1])], seg=seg)
        vp.lathe(p, nose, T, A, [(0.0, nose_prof[3][1])] + nose_prof[3:] + [(r, -nl - 0.01 * F), (0.0, -nl - 0.01 * F)], seg=seg)
    else:
        vp.lathe(p, nose, T, A, nose_prof + [(r, -nl - 0.01 * F), (0.0, -nl - 0.01 * F)], seg=seg)
    vp.lathe(p, body, T, A, [(0.0, -nl), (r, -nl), (r, -L + r * 1.2), (r * boat, -L + r * 0.2), (r * boat * 0.9, -L), (0.0, -L)],
             seg=seg)
    vp.lathe(p, 'Dark', T, A, [(r * boat * 0.75, -L + 0.005), (r * boat * 0.75, -L - 0.03 * F), (r * 0.4, -L - 0.03 * F),
                               (r * 0.4, -L + 0.005)], seg=seg, closed=True)
    if band and band_at is not None:
        vp.lathe(p, band, T, A, [(r + 0.002, -band_at), (r + 0.004, -band_at - 0.01 * F), (r + 0.004, -band_at - 0.06 * F),
                                 (r + 0.002, -band_at - 0.07 * F)], seg=seg)
    _, E1, E2 = vp.axis_frame(A)
    for spec in (fins_mid, fins_tail):
        if not spec:
            continue
        at, c0, c1, h, t = spec
        for k in range(4):
            a = math.radians(roll + 90 * k)
            rad = E1 * math.cos(a) + E2 * math.sin(a)
            O = T - A * at + rad * (r * 0.9)
            # a aleta em trapézio (o bordo de ataque enflechado), de perfil afinado: o prisma de espessura t centrado
            pts = [(0.0, 0.0), (-c0, 0.0), (-c0 + (c0 - c1) * 0.15, h), (-(c0 - c1) * 0.85, h)]
            pts = [(-u, v) for u, v in pts]
            # eu = −A (pra trás), ev = rad (pra fora), en = o lado
            en = A.cross(rad).normalized()
            vp.prism(p, fin, [(u, v) for u, v in pts], O - en * (t / 2), -A, rad, en, t)


def th_for(sec, x, coord, target, lo, hi, it=40):
    """O ângulo θ na seção (em x) onde a coordenada do ponto da pele (coord 1 = y, 2 = z) vale target, por bisseção entre lo e hi
    (o trecho tem de ser monotônico): a faixa que começa numa altura ou numa largura dada."""
    a, b = lo, hi
    fa = sec.P(x, a)[coord] - target
    for _ in range(it):
        m = (a + b) / 2
        fm = sec.P(x, m)[coord] - target
        if (fm > 0) == (fa > 0):
            a, fa = m, fm
        else:
            b = m
    return (a + b) / 2
