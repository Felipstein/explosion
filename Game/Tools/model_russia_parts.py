"""As peças comuns dos prédios provisórios da Rússia no detalhe AAA (a facção nova, 01/11/2026: docs/FACCAO_RUSSIA.md).

O pedido do Felipe (01/11): "Você já pode montar provisórios da Rússia no jogo também, a facção, com mesmo nível de balanceamento dos
outros, mas claro, sempre fazendo com detalhes e qualidade como até agora". E antes: "nada de jogo de lego, TEXTURIZADO, DETALHADO [...]
as construções têm que degradar de forma linda [...] nada de quinas quadradas".

A assinatura de cima (docs/FACCAO_RUSSIA.md §3), o que estas peças dão a todos os prédios da Rússia:
- o CONCRETO PRÉ-MOLDADO DE PAINEL (ru_panel_wall): cada painel é UMA peça com o chanfro largo e redondo, com o vão da janela recortado
  nela (a janela fica recuada na espessura), e a JUNTA FUNDA ESCURA entre os painéis (a massa de vedação, o slot Rubber): de cima e de
  lado a grade escura das juntas é a cara do prédio soviético de painel, diferente das placas quadradas do concreto oliva da China;
- o TELHADO-SERRA dos pavilhões (sawtooth_roof) e a CHAPA TRAPEZOIDAL VERDE (prof_sheet, o "профнастил"): a onda trapezoidal de
  verdade, 0,32 m de passo e 4,5 cm de fundo (7–10 px por onda na câmera: lê como nervura);
- a CHAMINÉ LISTRADA vermelha e branca (striped_chimney: as faixas da sinalização de aviação, a de cima vermelha), com as plataformas,
  a escada com gaiola, as luzes de obstáculo e a fuligem na boca;
- a ESTRELA VERMELHA CONTORNADA de branco (ru_star; nunca no teto);
- os LETREIROS EM CIRÍLICO ESTÊNCIL (stencil, roof_slogan): letras de verdade (Ж, Ш, Д, Б, П, Л, Ы, Ц, Г, Я, Э, И) em ordem que não
  forma palavra; nunca Z nem V;
- a TORRE DE SHUKHOV (shukhov_tower): a treliça hiperboloide de vigas retas torcidas, em seções listradas (a engenharia russa que todo
  mundo reconhece; de cima, um círculo: a assinatura redonda num mundo de retângulos);
- o PORTÃO VERDE COM AS ESTRELAS (star_gate), a CERCA DE CONCRETO PO-2 com o relevo de losangos (po2_fence), a cancela listrada
  (boom_barrier), a guarita, o "cogumelo" da sentinela (sentry_mushroom), o cano de gás amarelo (o gás da Rússia corre por cima do chão,
  pintado de amarelo), o contêiner de chapa ondulada, a caixa de munição verde.

Os slots (Game/Tools/import_provisional.py, RUSSIA_BUILDING_SLOTS): Concrete (o painel), Roof (a chapa verde), Metal (o aço escuro),
Red (as listras e a estrela), White, Paint (o aço pintado azulado: os rufos, as portas, os mastros), Team (a cor do jogador: lonas,
bandeiras, faixas pequenas), Glass, Light, Dark, Trim, Hazard, Wood, Cloth, Rubber (a massa das juntas), Broken; e dois slots que o jogo
já tem fora da lista: Yellow (o cano de gás e a ponte rolante amarela) e Beacon (as luzes de obstáculo que piscam).

RuPart: o BPart do model_building_parts (o corte dos fundos escondidos) que troca os slots da China pelos da Rússia em todas as peças
emprestadas (cn_window, cn_rollup, interior_frame, a lâmpada do kit...): CnConcrete/CnPlinth → Concrete, CnMetal → Metal, Player → Team.

As 2 lições da grade do detalhe (01/11; o model_detail.grid corta toda aresta > 0,4 m pela metade, 9 passadas):
- PEÇA NÃO ENCOSTA EM PEÇA DO MESMO BALDE com vértice coincidente: o remove_doubles do Part.finish solda as 2, a aresta fica com 3–4
  faces e o chanfro do Blender joga vértices a milhões de metros; a grade então nunca acaba (a fábrica chegou a 18 GB e travou a
  máquina). O canto das paredes tem junta de verdade, os degraus são um prisma só (stairs_x), o painel com vão é um sólido de células
  (grid_slab). O RuPart avisa (NONMANIFOLD) antes do chanfro;
- SÓ RETÂNGULOS nas faces grandes (a face trapezoidal faz a grade cortar só a aresta comprida e abrir leques); e as medidas logo abaixo de
  0,4 × 2^k (3,2 m = 8 células; 3,3 m = 16): os painéis ≤ 3,2 × 1,6 m, as lajes e a fundação com a grade já feita (grid_box,
  ru_foundation), a chapa com o fundo liso.

Coordenadas da Unreal (X frente, Y direita, Z cima, metros); o model_lib.V troca o Y pro Blender.
"""
import math

from mathutils import Vector

import model_lib
from model_lib import COLORS, V
from model_building_parts import (BPart, WallFace, wbox, hexa, rect_minus, splits, tube, fillet, cable, pipe_run, sweep,  # noqa: F401
                                  interior_frame, cn_window, ring_boxes, open_box, EPS, _hash)
from model_faction_kit import star_points, disc_ring, revolve, lamp  # noqa: F401

# as cores de conferência (sRGB 0–255): as instâncias do jogo convertidas (RuConcrete 0,20; RuRoof; RuMetal; RuRed; RuWhite; RuPaint)
COLORS.update({
    'Concrete': (124, 124, 120), 'Roof': (66, 82, 69), 'Metal': (63, 64, 66), 'Red': (149, 48, 44), 'White': (206, 206, 204),
    'Paint': (96, 105, 111), 'Team': (176, 38, 34), 'Yellow': (192, 149, 38), 'Wood': (120, 88, 52), 'Cloth': (150, 128, 92),
    'Rubber': (28, 28, 28), 'Broken': (148, 146, 138), 'Dark': (40, 40, 38), 'Trim': (70, 70, 68), 'Glass': (30, 50, 80),
    'Light': (255, 230, 170), 'Beacon': (255, 170, 40), 'Hazard': (214, 170, 30),
    'Gold': (196, 150, 52), 'Navy': (24, 52, 128),   # (01/11) o escudo tricolor (ru_star) no lugar da estrela
})

REMAP = {'CnConcrete': 'Concrete', 'CnPlinth': 'Concrete', 'CnMetal': 'Metal', 'CnRoof': 'Roof', 'CnGold': 'White',
         'Player': 'Team', 'Crate': 'Wood', 'Plaster': 'Concrete'}
JOINT = 'Rubber#joint'          # a massa de vedação no fundo da junta dos painéis


class RuPart(BPart):
    """O BPart com os slots da Rússia: as peças emprestadas da China e do kit trocam de material sozinhas. Antes do chanfro, avisa
    (NONMANIFOLD) o balde chanfrado com aresta de 3+ faces depois da solda: é peça encostando em peça do mesmo balde, e o chanfro do
    Blender joga vértices pro infinito nela (a grade do detalhe explode depois)."""

    def finish(self):
        import bmesh
        for m, b in self.bms.items():
            if self.bevel[m] <= 0:
                continue
            c = b.copy()
            bmesh.ops.remove_doubles(c, verts=c.verts, dist=1e-5)
            bad = [e for e in c.edges if len(e.link_faces) > 2]
            if bad:
                q = bad[0].verts[0].co
                print('NONMANIFOLD', self.name, m, len(bad), 'arestas; a 1a em (%.2f, %.2f, %.2f)' % (q.x, -q.y, q.z))
            c.free()
        return super().finish()

    def bm(self, m, bevel=0.03):
        base, _, suf = m.partition('#')
        if base in REMAP:
            if base == 'CnPlinth' and not suf:
                suf = 'plinth'
            m = REMAP[base] + ('#' + suf if suf else '')
        return super().bm(m, bevel)


def lerp(a, b, t):
    return a + (b - a) * t


def stairs_x(p, m, x_top, x_bot, y0, y1, z0, z1, n, bevel=0.03):
    """A escada de concreto numa peça só (o perfil em degraus extrudado em Y), descendo de x_top (em cima, z1) até x_bot (no chão, z0):
    os degraus soltos encostados soldavam no remove_doubles."""
    pts = [(x_top, z0), (x_bot, z0)]
    for k in range(n):
        xa = x_bot + (x_top - x_bot) * k / n
        xb = x_bot + (x_top - x_bot) * (k + 1) / n
        z = z0 + (z1 - z0) * (k + 1) / n
        pts += [(xa, z), (xb, z)]
    p.prism_xz(m, pts[:-1] + [(x_top, z1)], y0, y1, bevel=bevel)


def grid_box(p, m, x0, x1, y0, y1, z0, z1, cell=0.398, bevel=0.03):
    """A laje (caixa) com as faces de cima e de baixo já em células de ≤ 0,4 m e os lados acompanhando (a grade do detalhe divide pela
    metade: a laje de 13,1 m virava 64 células; aqui, 33)."""
    nx, ny = max(1, int(math.ceil((x1 - x0) / cell))), max(1, int(math.ceil((y1 - y0) / cell)))
    xs = [x0 + (x1 - x0) * i / nx for i in range(nx + 1)]
    ys = [y0 + (y1 - y0) * j / ny for j in range(ny + 1)]
    b = p.bm(m, bevel)
    T = [[b.verts.new(V(x, y, z1)) for x in xs] for y in ys]
    B = [[b.verts.new(V(x, y, z0)) for x in xs] for y in ys]
    for j in range(ny):
        for i in range(nx):
            b.faces.new([T[j][i], T[j][i + 1], T[j + 1][i + 1], T[j + 1][i]])
            b.faces.new([B[j + 1][i], B[j + 1][i + 1], B[j][i + 1], B[j][i]])
    for i in range(nx):
        b.faces.new([T[0][i + 1], T[0][i], B[0][i], B[0][i + 1]])
        b.faces.new([T[ny][i], T[ny][i + 1], B[ny][i + 1], B[ny][i]])
    for j in range(ny):
        b.faces.new([T[j][0], T[j + 1][0], B[j + 1][0], B[j][0]])
        b.faces.new([T[j + 1][nx], T[j][nx], B[j][nx], B[j + 1][nx]])


def ru_foundation(p, hx, hy, z=0.14, skirt=0.5, cell=0.398, m='Concrete#found', rim=False):
    """A laje do lote com a grade já feita (as células de ≤ 0,4 m: a grade do detalhe divide pela metade, e a laje de 25,8 m virava 128
    células em vez de 65) e a saia rasa que entra no chão; o friso escuro no pé e a borda de 25 cm em cima (a do kit)."""
    nx, ny = int(math.ceil(2 * hx / cell)), int(math.ceil(2 * hy / cell))
    xs = [-hx + 2 * hx * i / nx for i in range(nx + 1)]
    ys = [-hy + 2 * hy * j / ny for j in range(ny + 1)]
    b = p.bm(m, 0.06)
    top = [[b.verts.new(V(x, y, z)) for x in xs] for y in ys]
    bot = {}

    def bv(i, j):
        if (i, j) not in bot:
            bot[(i, j)] = b.verts.new(V(xs[i], ys[j], z - skirt))
        return bot[(i, j)]
    for j in range(ny):
        for i in range(nx):
            b.faces.new([top[j][i], top[j][i + 1], top[j + 1][i + 1], top[j + 1][i]])
    for i in range(nx):
        b.faces.new([top[0][i + 1], top[0][i], bv(i, 0), bv(i + 1, 0)])
        b.faces.new([top[ny][i], top[ny][i + 1], bv(i + 1, ny), bv(i, ny)])
    for j in range(ny):
        b.faces.new([top[j][0], top[j + 1][0], bv(0, j + 1), bv(0, j)])
        b.faces.new([top[j + 1][nx], top[j][nx], bv(nx, j), bv(nx, j + 1)])
    ring_boxes(p, 'Dark#rim', -hx - 0.03, hx + 0.03, -hy - 0.03, hy + 0.03, z - 0.10, z - 0.07, 0.3, bevel=0.0)
    if rim:
        ring_boxes(p, m + 'r', -hx, hx, -hy, hy, z + 0.002, z + 0.08, 0.25, bevel=0.02)


# ===================================================================================================== sólidos no plano da parede

def face_prism(p, m, F, outline, n0, n1, bevel=None):
    """O polígono (u, z) da parede F extrudado de n0 a n1 (pode ser côncavo: o U do painel da porta, a estrela)."""
    p.loft(m, [[F.pt(u, z, n0) for (u, z) in outline], [F.pt(u, z, n1) for (u, z) in outline]], bevel=bevel)


def grid_slab(p, m, F, us, zs, keep, n0, n1, bevel=None):
    """Um sólido de células retangulares (a grade us × zs no plano da parede, keep(i, j) = a célula existe), fechado e com os vértices
    compartilhados: o painel com a janela (a grade 3 × 3 sem o meio), o U da porta, o L. Só retângulos: a grade do detalhe
    (model_detail.grid) divide retângulo em retângulo (a face trapezoidal fazia a grade cortar só a aresta comprida e explodir em leques:
    4 milhões de faces na fábrica, 01/11). O chanfro pega só as quinas (as arestas entre células do mesmo plano ficam lisas)."""
    bb = p.bm(m, bevel if bevel is not None else 0.1)
    vs = {}

    def vert(i, j, k):
        key = (i, j, k)
        if key not in vs:
            vs[key] = bb.verts.new(V(*F.pt(us[i], zs[j], n1 if k else n0)))
        return vs[key]
    ni, nj = len(us) - 1, len(zs) - 1
    on = {(i, j) for i in range(ni) for j in range(nj) if keep(i, j)}
    for (i, j) in on:
        bb.faces.new([vert(i, j, 1), vert(i + 1, j, 1), vert(i + 1, j + 1, 1), vert(i, j + 1, 1)])
        bb.faces.new([vert(i, j + 1, 0), vert(i + 1, j + 1, 0), vert(i + 1, j, 0), vert(i, j, 0)])
        if (i, j - 1) not in on:
            bb.faces.new([vert(i, j, 0), vert(i + 1, j, 0), vert(i + 1, j, 1), vert(i, j, 1)])
        if (i, j + 1) not in on:
            bb.faces.new([vert(i, j + 1, 1), vert(i + 1, j + 1, 1), vert(i + 1, j + 1, 0), vert(i, j + 1, 0)])
        if (i - 1, j) not in on:
            bb.faces.new([vert(i, j + 1, 0), vert(i, j, 0), vert(i, j, 1), vert(i, j + 1, 1)])
        if (i + 1, j) not in on:
            bb.faces.new([vert(i + 1, j, 0), vert(i + 1, j + 1, 0), vert(i + 1, j + 1, 1), vert(i + 1, j, 1)])


def ring_panel(p, m, F, a, b, c, d, h0, h1, h2, h3, n0, n1, bevel=None):
    """O painel com o vão dentro (a janela): o anel fechado, uma peça só (o chanfro corre por fora e pela boca do vão)."""
    grid_slab(p, m, F, [a, h0, h1, b], [c, h2, h3, d], lambda i, j: not (i == 1 and j == 1), n0, n1, bevel=bevel)
    return
    bb = p.bm(m, bevel if bevel is not None else 0.1)
    O = [(a, c), (b, c), (b, d), (a, d)]
    I = [(h0, h2), (h1, h2), (h1, h3), (h0, h3)]
    of = [bb.verts.new(V(*F.pt(u, z, n1))) for (u, z) in O]
    ob = [bb.verts.new(V(*F.pt(u, z, n0))) for (u, z) in O]
    inf = [bb.verts.new(V(*F.pt(u, z, n1))) for (u, z) in I]
    inb = [bb.verts.new(V(*F.pt(u, z, n0))) for (u, z) in I]
    for k in range(4):
        j = (k + 1) % 4
        bb.faces.new([of[k], of[j], inf[j], inf[k]])
        bb.faces.new([ob[j], ob[k], inb[k], inb[j]])
        bb.faces.new([of[j], of[k], ob[k], ob[j]])
        bb.faces.new([inf[k], inf[j], inb[j], inb[k]])


def stroke(p, m, F, A, B, w, n0, n1, cap=True):
    """Uma barra reta no plano da parede, de A a B (u, z), largura w (a pincelada do estêncil, a diagonal do portão)."""
    ax, az = A
    bx, bz = B
    L = math.hypot(bx - ax, bz - az)
    if L < 1e-6:
        return
    dx, dz = (bx - ax) / L, (bz - az) / L
    if cap:
        ax, az, bx, bz = ax - dx * w / 2, az - dz * w / 2, bx + dx * w / 2, bz + dz * w / 2
    px, pz = -dz * w / 2, dx * w / 2
    P = []
    for s in (-1, 1):
        for n in (n0, n1):
            for (x, z) in ((ax, az), (bx, bz)):
                P.append(F.pt(x + s * px, z + s * pz, n))
    hexa(p, m, P, bevel=0.0)


# ===================================================================================================== o painel pré-moldado

def ru_panel_wall(p, F, us, zs, holes=(), t=0.35, proud=0.08, groove=0.08, pm='Concrete#pnl', fill=JOINT, bevel=0.10, min_piece=0.22):
    """A parede de painéis pré-moldados soviética: a grade (us ao longo, zs na altura) de painéis, cada painel UMA peça com a espessura
    inteira (de n = −t até 'proud' pra fora) e o chanfro largo e redondo; o vão (holes = [(u0, u1, z0, z1)...]) recortado no próprio
    painel (o anel em volta da janela, o U da porta), nunca em pedaços; entre os painéis a junta funda de 'groove' com a massa escura no
    fundo (fill, o slot Rubber). Os vãos que passam por vários painéis (o portão) têm as bordas nas juntas da grade."""
    g = groove / 2
    nu, nz = len(us) - 1, len(zs) - 1
    for i in range(nu):
        a = us[i] + (g if i > 0 else 0.0)
        b = us[i + 1] - (g if i < nu - 1 else 0.0)
        for j in range(nz):
            c = zs[j] + (g if j > 0 else 0.0)
            d = zs[j + 1] - (g if j < nz - 1 else 0.0)
            if b - a < 0.05 or d - c < 0.05:
                continue
            hs = [(max(h[0], a), min(h[1], b), max(h[2], c), min(h[3], d)) for h in holes
                  if h[0] < b - 1e-6 and h[1] > a + 1e-6 and h[2] < d - 1e-6 and h[3] > c + 1e-6]
            if not hs:
                wbox(p, pm, F, a, b, c, d, -t, proud, bevel=bevel)
                continue
            # a borda do vão a menos de min_piece da borda do painel vai até ela (sem lasca fina)
            sh = []
            for (h0, h1, h2, h3) in hs:
                sh.append((a if h0 <= a + min_piece else h0, b if h1 >= b - min_piece else h1,
                           c if h2 <= c + min_piece else h2, d if h3 >= d - min_piece else h3))
            # o painel em volta dos vãos (o anel da janela, o U da porta, o L, as 2 sobras do vão que atravessa): a grade de retângulos sem
            # as células dos vãos, num sólido só (as peças que se encostavam soldavam no remove_doubles e o chanfro explodia)
            gu = sorted(set([a, b] + [q for h in sh for q in h[:2] if a + 1e-6 < q < b - 1e-6]))
            gz = sorted(set([c, d] + [q for h in sh for q in h[2:] if c + 1e-6 < q < d - 1e-6]))

            def keep(i, j, gu=gu, gz=gz, sh=sh):
                cu, cz = (gu[i] + gu[i + 1]) / 2, (gz[j] + gz[j + 1]) / 2
                return not any(h[0] - 1e-6 <= cu <= h[1] + 1e-6 and h[2] - 1e-6 <= cz <= h[3] + 1e-6 for h in sh)
            if any(keep(i, j) for i in range(len(gu) - 1) for j in range(len(gz) - 1)):
                grid_slab(p, pm, F, gu, gz, keep, -t, proud, bevel=bevel)
    # a massa no fundo das juntas (só onde não há vão)
    if fill:
        hh = [(h[0] - g - 0.01, h[1] + g + 0.01, h[2] - g - 0.01, h[3] + g + 0.01) for h in holes]
        for i in range(1, nu):
            for (a2, b2, c2, d2, _, _) in rect_minus(us[i] - g - 0.006, us[i] + g + 0.006, zs[0], zs[-1], hh, eps=0.0):
                if d2 - c2 > 0.02:
                    wbox(p, fill, F, a2, b2, c2, d2, -t + 0.012, 0.0, bevel=0.0)
        for j in range(1, nz):
            for (a2, b2, c2, d2, _, _) in rect_minus(us[0], us[-1], zs[j] - g - 0.006, zs[j] + g + 0.006, hh, eps=0.0):
                if b2 - a2 > 0.02:
                    wbox(p, fill, F, a2, b2, c2, d2, -t + 0.012, 0.0, bevel=0.0)


def cuts(a, b, step, keep=()):
    """As juntas de a até b a cada ~step, com as juntas obrigatórias (as bordas dos vãos grandes) no meio."""
    pts = sorted(set([a, b] + [k for k in keep if a + 0.3 < k < b - 0.3]))
    out = [pts[0]]
    for x0, x1 in zip(pts[:-1], pts[1:]):
        n = max(1, int(math.ceil((x1 - x0) / step - 0.08)))
        out += [x0 + (x1 - x0) * k / n for k in range(1, n + 1)]
    return out


def ru_block(p, x0, x1, y0, y1, z0, z1, t=0.35, holes=None, skin=('+x', '-x', '+y', '-y'), walls=None, pw=3.0, zs=None,
             ukeep=None, socle=0.62, parapet=0.6, par_faces=None, roof='flat', roof_t=0.28, interior=True, cols=None, floors=(),
             along=None, pm='Concrete#pnl', step=3.0, win=None, pwf=None, roof_skip=None):
    """O bloco oco de painéis soviético: as 4 paredes de painel (t de espessura, a grade pw × zs, os vãos), o soco de concreto mais
    saliente no pé (o цоколь), os painéis do topo subindo como platibanda (parapet) com o rufo de aço pintado em cima, a laje do teto
    com espessura e, por cima, a manta em faixas (roof='flat'); as lajes dos andares (floors), os pilares e as vigas por dentro (o furo
    do dano mostra um prédio). O canto: o painel da face ±y dá a volta e mostra a ponta (como no prédio de painel de verdade).
    holes = {face: [(u0, u1, z0, z1)...]} (u = Y nas faces ±x, X nas faces ±y); ukeep = {face: [juntas obrigatórias]};
    zs = as juntas horizontais (lista, ou {face: lista}); pwf = {face: largura do painel} (senão pw);
    win = {face: dict(floors=[z do piso...], w=1.5, h=1.45, sill=0.9, skip=[(coluna, andar)...], lit=[(coluna, andar)...])}: uma janela
    no meio de cada painel de cada andar (o ritmo do prédio de painel), recuada, com o caixilho branco.
    Devolve o z do topo da laje."""
    holes = holes or {}
    walls = walls or ('+x', '-x', '+y', '-y')
    ukeep = ukeep or {}
    par_faces = par_faces if par_faces is not None else walls
    at = {'+x': x1, '-x': x0, '+y': y1, '-y': y0}
    zt = z1 - roof_t
    zb = z0 + socle if socle else z0
    proud = 0.08
    for f in walls:
        F = WallFace(f, at[f], z0)
        top = z1 + (parapet if (parapet and f in par_faces) else 0.0)
        if f[1] == 'x':
            a, b = y0 + (t if '-y' in walls else 0.0), y1 - (t if '+y' in walls else 0.0)
            sa, sb = a, b
        else:
            ex0 = proud if '-x' in skin and '-x' in walls else 0.0
            ex1 = proud if '+x' in skin and '+x' in walls else 0.0
            a, b = x0 - ex0, x1 + ex1
            sa, sb = x0 - 0.15 * (1 if '-x' in walls else 0), x1 + 0.15 * (1 if '+x' in walls else 0)
        hs = list(holes.get(f, ()))
        if f[1] == 'x':
            # o canto: 1 junta entre a ponta do painel ±y (que dá a volta) e o primeiro painel ±x, com a massa no fundo. Encostados, os
            # vértices coincidiam, o remove_doubles soldava as 2 peças e o chanfro jogava vértices a milhões de metros (01/11)
            j0 = 0.04 if '-y' in walls else 0.003
            j1 = 0.04 if '+y' in walls else 0.003
            if f in skin:
                for (ja, jb) in ((a, a + j0), (b - j1, b)):
                    if jb - ja > 0.01:
                        wbox(p, JOINT, F, ja - 0.003, jb + 0.003, zb, top, -t + 0.012, 0.0, bevel=0.0)
            else:
                j0, j1 = 0.003, 0.003
            a, b = a + j0, b - j1
            sa, sb = sa + 0.003, sb - 0.003
        if f in skin:
            pwx = (pwf or {}).get(f, pw)
            us = cuts(a, b, pwx, [q for h in hs for q in h[:2] if h[1] - h[0] > pwx * 0.9] + list(ukeep.get(f, ())))
            zsrc = zs.get(f) if isinstance(zs, dict) else zs
            zz = [zb] + [q for q in (zsrc or splits(zb, top, 3.0)[1:-1]) if zb + 0.3 < q < top - 0.3] + [top]
            wins = []
            if win and f in win:
                wd = win[f]
                ww, wh, sill = wd.get('w', 1.5), wd.get('h', 1.45), wd.get('sill', 0.8)
                for j, zf in enumerate(wd['floors']):
                    for i in range(len(us) - 1):
                        if (i, j) in wd.get('skip', ()) or (i, None) in wd.get('skip', ()):
                            continue
                        c = (us[i] + us[i + 1]) / 2
                        wins.append(((c - ww / 2, c + ww / 2, zf + sill, zf + sill + wh), (i, j) in wd.get('lit', ())))
            ru_panel_wall(p, F, us, zz, hs + [w_[0] for w_ in wins], t=t, proud=proud, pm=pm)
            for (h_, lit) in wins:
                ru_window(p, F, h_, t=t, lit=lit)
        else:
            for (a2, b2, c2, d2, _, _) in rect_minus(a, b, zb, top, hs):
                wbox(p, 'Concrete#core', F, a2, b2, c2, d2, -t, 0.0)
        # o soco (mais saliente, dá a volta no canto), interrompido nas portas
        if socle:
            for (a2, b2, c2, d2, _, _) in rect_minus(sa, sb, z0, zb, [h for h in hs if h[2] < zb]):
                if b2 - a2 > 0.2:
                    wbox(p, 'Concrete#socle', F, a2, b2, c2, d2, -t, proud + 0.07, bevel=0.06)
        # o rufo de aço pintado em cima da platibanda (a dobra que pinga pros 2 lados: a linha fina que desenha o topo)
        if parapet and f in par_faces:
            ra, rb = (a - 0.04, b + 0.04) if f[1] == 'y' else (a - 0.02, b + 0.02)
            wbox(p, 'Paint#flash', F, ra, rb, top - 0.02, top + 0.05, -t - 0.05, proud + 0.07, bevel=0.02)
            wbox(p, 'Paint#flash', F, ra, rb, top - 0.14, top + 0.0, proud + 0.03, proud + 0.07, bevel=0.0)
    ix0, ix1 = x0 + (t if '-x' in walls else 0.0), x1 - (t if '+x' in walls else 0.0)
    iy0, iy1 = y0 + (t if '-y' in walls else 0.0), y1 - (t if '+y' in walls else 0.0)
    if roof:
        grid_box(p, 'Concrete#slab', ix0 - 0.02, ix1 + 0.02, iy0 - 0.02, iy1 + 0.02, zt, z1, bevel=0.03)
        if roof == 'flat':
            roof_rolls(p, ix0, ix1, iy0, iy1, z1, along='x' if (ix1 - ix0) >= (iy1 - iy0) else 'y', skip=roof_skip)
    for zf in floors:
        grid_box(p, 'Concrete#floor', ix0 - 0.02, ix1 + 0.02, iy0 - 0.02, iy1 + 0.02, zf - 0.11, zf + 0.11, bevel=0.0)
    if interior and ix1 - ix0 > 1.0 and iy1 - iy0 > 1.0:
        interior_frame(p, ix0, ix1, iy0, iy1, z0, zt if roof else z1, cols=cols, step=step,
                       along=along or ('x' if (x1 - x0) >= (y1 - y0) else 'y'), floor=None)
    return z1


def roof_rolls(p, x0, x1, y0, y1, z, w=1.6, along='x', m='Paint#roof', gap=0.03, skip=None):
    """A manta do teto plano em faixas (o rolo de 1 m com a sobreposição: a costura rasa que lê como ritmo de cima), no aço frio
    azulado da facção (de cima o teto do prédio de painel é o azul-acinzentado dos tanques, com a moldura clara da platibanda).
    skip = (x0, x1, y0, y1): o retângulo onde o bloco de cima senta (sem manta embaixo dele)."""
    def emit(a0, a1, b0, b1, k):
        # a0..a1 ao longo da faixa, b0..b1 a largura; o furo do skip corta a faixa em 2
        segs = [(a0, a1)]
        if skip:
            s0, s1, t0, t1 = (skip[0], skip[1], skip[2], skip[3]) if along == 'x' else (skip[2], skip[3], skip[0], skip[1])
            if b1 > t0 and b0 < t1:
                segs = [(a0, min(a1, s0)), (max(a0, s1), a1)]
        for (c0, c1) in segs:
            if c1 - c0 < 0.2:
                continue
            # sem chanfro (as faixas compridas com 3 segmentos de chanfro custavam 20 mil faces no quartel) e com o fundo no plano da
            # laje (o corte dos fundos tira); a costura é o degrau de 1,5 cm entre uma faixa e a outra
            if along == 'x':
                p.box(m, c0, c1, b0, b1, z, z + 0.04 + 0.015 * (k % 2), bevel=0.0)
            else:
                p.box(m, b0, b1, c0, c1, z, z + 0.04 + 0.015 * (k % 2), bevel=0.0)
    if along == 'x':
        ys = splits(y0, y1, w)
        for k, (a, b) in enumerate(zip(ys[:-1], ys[1:])):
            emit(x0, x1, a + gap / 2, b - gap / 2, k)
    else:
        xs = splits(x0, x1, w)
        for k, (a, b) in enumerate(zip(xs[:-1], xs[1:])):
            emit(y0, y1, a + gap / 2, b - gap / 2, k)


def gable_end(p, F, u0, u1, z0, rise, t=0.35, proud=0.08, m='Concrete#gab'):
    """O oitão (o triângulo do telhado de duas águas) na parede F, em 2 painéis com a junta no meio."""
    um = (u0 + u1) / 2
    g = 0.04
    face_prism(p, m, F, [(u0, z0), (um - g, z0), (um - g, z0 + rise * (um - g - u0) / (um - u0))], -t, proud, bevel=0.08)
    face_prism(p, m, F, [(um + g, z0), (u1, z0), (um + g, z0 + rise * (u1 - um - g) / (u1 - um))], -t, proud, bevel=0.08)
    face_prism(p, JOINT, F, [(um - g - 0.01, z0), (um + g + 0.01, z0), (um + g + 0.01, z0 + rise * 0.97), (um - g - 0.01, z0 + rise * 0.97)],
               -t + 0.012, 0.0, bevel=0.0)


# ===================================================================================================== janelas e portas

def panel_windows(us, zs_floor, w=1.5, h=1.45, sill=0.9, skip=()):
    """Um vão de janela no meio de cada painel de cada andar (o ritmo do prédio de painel); skip = [(coluna, andar)...]."""
    out = []
    for j, zf in enumerate(zs_floor):
        for i in range(len(us) - 1):
            if (i, j) in skip or (i, None) in skip:
                continue
            c = (us[i] + us[i + 1]) / 2
            out.append((c - w / 2, c + w / 2, zf + sill, zf + sill + h))
    return out


def ru_window(p, F, h, t=0.35, lit=False, frame='White', mull=1):
    """A janela soviética no vão do painel: o caixilho branco recuado na espessura (uma peça só: o quadro com o vão), o montante do
    meio, o vidro escuro (ou aceso), o peitoril de chapa saindo pra fora com a pingadeira."""
    u0, u1, z0, z1 = h
    d = t * 0.55
    fw = 0.085
    grid_slab(p, frame + '#win', F, [u0, u0 + fw, u1 - fw, u1], [z0, z0 + fw, z1 - fw, z1], lambda i, j: not (i == 1 and j == 1),
              -d - 0.05, -d + 0.07, bevel=0.02)
    wbox(p, 'Light' if lit else 'Glass', F, u0 + fw - 0.01, u1 - fw + 0.01, z0 + fw - 0.01, z1 - fw + 0.01, -d - 0.03, -d - 0.01, bevel=0.0)
    for k in range(1, mull + 1):
        u = u0 + (u1 - u0) * k / (mull + 1)
        wbox(p, frame + '#mull', F, u - 0.035, u + 0.035, z0 + fw - 0.01, z1 - fw + 0.01, -d - 0.025, -d + 0.04, bevel=0.0)
    wbox(p, 'Paint#sill', F, u0 - 0.06, u1 + 0.06, z0 - 0.05, z0 + 0.01, -d + 0.06, 0.18, bevel=0.0)


def ribbon_glazing(p, F, u0, u1, z0, z1, t=0.35, step=1.5, frame='Metal', rows=2):
    """A janela em fita do pavilhão industrial (o vidro aramado nos caixilhos de aço): os montantes a cada ~step, as travessas, o vidro
    recuado, o peitoril de chapa; o ritmo dos caixilhos lê de longe."""
    d = t * 0.5
    fm = frame + '#rib'
    wbox(p, 'Glass', F, u0 + 0.02, u1 - 0.02, z0 + 0.02, z1 - 0.02, -d - 0.02, -d, bevel=0.0)
    for u in splits(u0, u1, step):
        wbox(p, fm, F, u - 0.05, u + 0.05, z0, z1, -d - 0.04, -d + 0.06, bevel=0.02)
    for k in range(rows + 1):
        z = z0 + (z1 - z0) * k / rows
        wbox(p, fm, F, u0, u1, z - 0.05, z + 0.05, -d - 0.04, -d + 0.06, bevel=0.02)
    for u in splits(u0, u1, step / 3):
        wbox(p, fm + 'l', F, u - 0.02, u + 0.02, z0, z1, -d - 0.01, -d + 0.02, bevel=0.0)
    wbox(p, 'Paint#flash', F, u0 - 0.06, u1 + 0.06, z0 - 0.06, z0 + 0.01, -d + 0.04, 0.16, bevel=0.0)


def entry_door(p, F, u0, u1, z0, z1, t=0.35, m='Paint#door', canopy=1.4, lamp_=True):
    """A porta de aço de duas folhas no vão (recuada, com o batente), e por cima a marquise fina de concreto em balanço (o козырёк),
    com a luminária embaixo."""
    d = t * 0.5
    um = (u0 + u1) / 2
    wbox(p, 'Dark', F, u0, u1, z0, z1, -t + 0.02, -t + 0.04, bevel=0.0)
    for (a, b) in ((u0 + 0.06, um - 0.015), (um + 0.015, u1 - 0.06)):
        wbox(p, m, F, a, b, z0 + 0.02, z1 - 0.06, -d - 0.04, -d, bevel=0.012)
        wbox(p, 'Dark#door', F, a + 0.12, b - 0.12, z1 - 0.75, z1 - 0.3, -d, -d + 0.01, bevel=0.0)
        wbox(p, 'Paint#door', F, (a + b) / 2 - 0.2, (a + b) / 2 + 0.2, z0 + 0.95, z0 + 1.05, -d, -d + 0.03, bevel=0.0)
    wbox(p, 'Metal#frame', F, u0 - 0.08, u0, z0, z1 + 0.01, -d - 0.055, -d + 0.055, bevel=0.012)
    wbox(p, 'Metal#frame', F, u1, u1 + 0.08, z0, z1 + 0.01, -d - 0.055, -d + 0.055, bevel=0.012)
    wbox(p, 'Metal#frame', F, u0 - 0.09, u1 + 0.09, z1, z1 + 0.08, -d - 0.06, -d + 0.06, bevel=0.012)
    if canopy:
        wbox(p, 'Concrete#canopy', F, u0 - 0.6, u1 + 0.6, z1 + 0.3, z1 + 0.48, -0.05, canopy, bevel=0.05)
        wbox(p, 'Paint#flash', F, u0 - 0.64, u1 + 0.64, z1 + 0.46, z1 + 0.52, -0.05, canopy + 0.04, bevel=0.0)
        if lamp_:
            c = F.pt(um, z1 + 0.27, canopy * 0.55)
            p.cyl('Dark#lamp', c, 'z', 0.16, 0.06, seg=12, bevel=0.0)
            p.cyl('Light', (c[0], c[1], c[2] - 0.035), 'z', 0.12, 0.02, seg=12, bevel=0.0)
    wbox(p, 'Concrete#step', F, u0 - 0.5, u1 + 0.5, z0 - 0.45, z0, 0.0, 1.1, bevel=0.04)


def sliding_leaf(p, F, u0, u1, z0, z1, n0, m='Roof', frame='Paint#leaf', star=False):
    """A folha de porta de correr de chapa trapezoidal verde (as nervuras na vertical) no quadro de aço pintado com a diagonal, o rodízio
    de cima e a guia de baixo; star: a estrela contornada no meio da folha."""
    fw = 0.12
    # (os montantes entram 1 cm nas travessas: encostados exatos, os vértices soldavam e o chanfro estragava)
    wbox(p, frame, F, u0, u1, z0, z0 + fw, n0, n0 + 0.1, bevel=0.02)
    wbox(p, frame, F, u0, u1, z1 - fw, z1, n0, n0 + 0.1, bevel=0.02)
    wbox(p, frame, F, u0 + 0.003, u0 + fw, z0 + fw - 0.01, z1 - fw + 0.01, n0 + 0.003, n0 + 0.097, bevel=0.02)
    wbox(p, frame, F, u1 - fw, u1 - 0.003, z0 + fw - 0.01, z1 - fw + 0.01, n0 + 0.003, n0 + 0.097, bevel=0.02)
    wbox(p, frame, F, u0 + fw - 0.01, u1 - fw + 0.01, (z0 + z1) / 2 - fw / 2, (z0 + z1) / 2 + fw / 2, n0 + 0.006, n0 + 0.094, bevel=0.02)
    P0 = Vector(F.pt(u0 + fw, z1 - fw, n0 + 0.035))
    Pa = Vector(F.pt(u0 + fw, z0 + fw, n0 + 0.035))
    Pb = Vector(F.pt(u1 - fw, z1 - fw, n0 + 0.035))
    prof_sheet(p, m, P0, Pa, Pb, period=0.3, depth=0.04, th=0.015, outward=Vector(F.normal()))
    stroke(p, frame, F, (u0 + fw, z0 + fw), (u1 - fw, (z0 + z1) / 2 - fw / 2), 0.1, n0 + 0.1, n0 + 0.16)
    stroke(p, frame, F, (u0 + fw, (z0 + z1) / 2 + fw / 2), (u1 - fw, z1 - fw), 0.1, n0 + 0.1, n0 + 0.16)
    for uu in (u0 + 0.35, u1 - 0.35):                                           # os rodízios no trilho de cima
        c = F.pt(uu, z1 + 0.12, n0 + 0.05)
        p.cyl('Metal#roll', c, F.ax if F.ax == 'x' else 'y', 0.11, 0.08, seg=12, bevel=0.0)
    if star:
        ru_star(p, F, (u0 + u1) / 2, (z0 + z1) / 2 + (z1 - z0) * 0.18, min(u1 - u0, z1 - z0) * 0.2, n=n0 + 0.165)


# ===================================================================================================== a chapa trapezoidal

def prof_sheet(p, m, P0, Pa, Pb, period=0.36, depth=0.045, th=0.02, outward=None, drip=0.0, flat=True):
    """A chapa trapezoidal (o профнастил) no paralelogramo P0 + s (Pa − P0) + t (Pb − P0): as nervuras correm ao longo de Pa − P0
    (descendo o telhado), a onda trapezoidal atravessa Pb − P0 (40% vale, 10% subida, 40% crista, 10% descida). A crista sobe pela
    normal que aponta pra cima (ou pra 'outward'); a espessura th pra trás. drip: a ponta de baixo dobra pra baixo (a pingadeira).
    flat: o lado de baixo liso (o que não se vê; a grade do detalhe não corta as nervuras 2 vezes)."""
    if flat:
        _prof_flat(p, m, Vector(P0), Vector(Pa), Vector(Pb), period, depth, th, outward, drip)
        return
    P0, Pa, Pb = Vector(P0), Vector(Pa), Vector(Pb)
    A, B = Pa - P0, Pb - P0
    N = A.cross(B).normalized()
    ref = outward if outward is not None else Vector((0.0, 0.0, 1.0))
    if N.dot(ref) < 0:
        N = -N
    n = max(1, int(round(B.length / period)))
    prof = []
    for k in range(n):
        for (s, h) in ((0.0, 0.0), (0.38, 0.0), (0.5, depth), (0.88, depth)):
            prof.append(((k + s) / n, h))
    prof.append((1.0, 0.0))
    rows = [(0.0, Vector()), (1.0, Vector())]
    if drip:
        rows.append((1.0, -N * drip + A.normalized() * 0.02))
    b = p.bm(m, 0.0)
    top, bot = [], []
    for (tt, off) in rows:
        top.append([b.verts.new(V(*(P0 + A * tt + B * s + N * h + off))) for (s, h) in prof])
        bot.append([b.verts.new(V(*(P0 + A * tt + B * s + N * (h - th) + off))) for (s, h) in prof])
    K = len(prof)
    R = len(rows)
    for r in range(R - 1):
        for k in range(K - 1):
            b.faces.new([top[r][k], top[r][k + 1], top[r + 1][k + 1], top[r + 1][k]])
            b.faces.new([bot[r + 1][k], bot[r + 1][k + 1], bot[r][k + 1], bot[r][k]])
        b.faces.new([top[r][0], top[r + 1][0], bot[r + 1][0], bot[r][0]])
        b.faces.new([top[r][K - 1], bot[r][K - 1], bot[r + 1][K - 1], top[r + 1][K - 1]])
    for k in range(K - 1):
        b.faces.new([top[0][k], bot[0][k], bot[0][k + 1], top[0][k + 1]])
        b.faces.new([top[R - 1][k], top[R - 1][k + 1], bot[R - 1][k + 1], bot[R - 1][k]])


def _prof_flat(p, m, P0, Pa, Pb, period, depth, th, outward, drip):
    """A chapa trapezoidal com a face de baixo lisa: o perfil em cima, um quad liso embaixo por fileira, as pontas fechadas em leque
    (cada ponta é um polígono só)."""
    A, B = Pa - P0, Pb - P0
    N = A.cross(B).normalized()
    ref = outward if outward is not None else Vector((0.0, 0.0, 1.0))
    if N.dot(ref) < 0:
        N = -N
    n = max(1, int(round(B.length / period)))
    prof = []
    for k in range(n):
        for (s, h) in ((0.0, 0.0), (0.38, 0.0), (0.5, depth), (0.88, depth)):
            prof.append(((k + s) / n, h))
    prof.append((1.0, 0.0))
    rows = [(0.0, Vector()), (1.0, Vector())]
    if drip:
        rows.append((1.0, -N * drip + A.normalized() * 0.02))
    b = p.bm(m, 0.0)
    K = len(prof)
    top = [[b.verts.new(V(*(P0 + A * tt + B * s + N * h + off))) for (s, h) in prof] for (tt, off) in rows]
    bl = [b.verts.new(V(*(P0 + A * tt + N * (-th) + off))) for (tt, off) in rows]
    br = [b.verts.new(V(*(P0 + A * tt + B + N * (-th) + off))) for (tt, off) in rows]
    R = len(rows)
    for r in range(R - 1):
        for k in range(K - 1):
            b.faces.new([top[r][k], top[r][k + 1], top[r + 1][k + 1], top[r + 1][k]])
        b.faces.new([bl[r + 1], br[r + 1], br[r], bl[r]])
        b.faces.new([top[r][0], top[r + 1][0], bl[r + 1], bl[r]])
        b.faces.new([top[r][K - 1], br[r], br[r + 1], top[r + 1][K - 1]])
    b.faces.new([bl[0]] + [top[0][k] for k in range(K)] + [br[0]])
    b.faces.new([br[R - 1]] + [top[R - 1][k] for k in range(K - 1, -1, -1)] + [bl[R - 1]])


def flashing(p, m, pts, w=0.5, up=(0.0, 0.0, 1.0), h=0.12, th=0.03):
    """O rufo dobrado (o Λ de chapa na cumeeira, a calha de beiral) varrido pela polilinha."""
    prof = [(-w / 2, -0.01), (0.0, h), (w / 2, -0.01), (w / 2, -0.01 - th), (0.0, h - th * 1.4), (-w / 2, -0.01 - th)]
    sweep(p, m, pts, prof, up=up)


def gable_roof(p, x0, x1, y0, y1, z0, rise, along='x', over=0.45, eave=0.35, m='Roof', ridge_m='Paint#ridge', gutter=True,
               trusses=True, ends=True, t=0.35):
    """O telhado de duas águas de chapa trapezoidal verde sobre o bloco (x0..x1, y0..y1 = o contorno das paredes): a cumeeira
    'along' X ou Y, o beiral saindo 'eave' e o oitão 'over'; o rufo da cumeeira, as calhas e os tubos de descida, as tesouras de aço
    por dentro e os oitões de painel nas pontas (ends)."""
    if along == 'x':
        ym = (y0 + y1) / 2
        hw = (y1 - y0) / 2 + eave
        zr = z0 + rise
        drop = rise * eave / ((y1 - y0) / 2)
        for s in (-1, 1):
            P0 = Vector((x0 - over, ym, zr + 0.02))
            Pa = Vector((x0 - over, ym + s * hw, z0 - drop + 0.02))
            Pb = Vector((x1 + over, ym, zr + 0.02))
            prof_sheet(p, m, P0, Pa, Pb, drip=0.06)
        flashing(p, ridge_m, [(x0 - over - 0.05, ym, zr + 0.05), (x1 + over + 0.05, ym, zr + 0.05)], w=0.6, h=0.14)
        if gutter:
            for s in (-1, 1):
                yy = ym + s * (hw + 0.08)
                zz = z0 - drop - 0.1
                p.cyl('Paint#gut', (x0 + (x1 - x0) / 2, yy, zz), 'x', 0.09, x1 - x0 + 2 * over, seg=10, bevel=0.0)
                for xx in (x0 + 0.4, x1 - 0.4):
                    tube(p, 'Paint#gut', fillet([(xx, yy, zz), (xx, yy - s * 0.25, zz - 0.3), (xx, ym + s * ((y1 - y0) / 2 + 0.14), zz - 0.6),
                                                 (xx, ym + s * ((y1 - y0) / 2 + 0.14), 0.45)], 0.12), 0.055, seg=8)
        for xx in (x0 - over, x1 + over):                                       # as testeiras (a beirada de chapa no oitão)
            for s in (-1, 1):
                sweep(p, ridge_m, [(xx, ym, zr + 0.05), (xx, ym + s * hw, z0 - drop + 0.05)],
                      [(-0.03, -0.12), (0.03, -0.12), (0.03, 0.06), (-0.03, 0.06)])
        if ends:
            gable_end(p, WallFace('+x', x1, 0.0), y0, y1, z0, rise, t=t)
            gable_end(p, WallFace('-x', x0, 0.0), y0, y1, z0, rise, t=t)
        if trusses:
            for xx in splits(x0 + 1.5, x1 - 1.5, 3.0):
                truss(p, (xx, y0 + t, z0), (xx, y1 - t, z0), rise * 0.92)
    else:
        xm = (x0 + x1) / 2
        hw = (x1 - x0) / 2 + eave
        zr = z0 + rise
        drop = rise * eave / ((x1 - x0) / 2)
        for s in (-1, 1):
            P0 = Vector((xm, y0 - over, zr + 0.02))
            Pa = Vector((xm + s * hw, y0 - over, z0 - drop + 0.02))
            Pb = Vector((xm, y1 + over, zr + 0.02))
            prof_sheet(p, m, P0, Pa, Pb, drip=0.06)
        flashing(p, ridge_m, [(xm, y0 - over - 0.05, zr + 0.05), (xm, y1 + over + 0.05, zr + 0.05)], w=0.6, h=0.14)
        if gutter:
            for s in (-1, 1):
                xx = xm + s * (hw + 0.08)
                zz = z0 - drop - 0.1
                p.cyl('Paint#gut', (xx, (y0 + y1) / 2, zz), 'y', 0.09, y1 - y0 + 2 * over, seg=10, bevel=0.0)
                for yy in (y0 + 0.4, y1 - 0.4):
                    tube(p, 'Paint#gut', fillet([(xx, yy, zz), (xx - s * 0.25, yy, zz - 0.3), (xm + s * ((x1 - x0) / 2 + 0.14), yy, zz - 0.6),
                                                 (xm + s * ((x1 - x0) / 2 + 0.14), yy, 0.45)], 0.12), 0.055, seg=8)
        for yy in (y0 - over, y1 + over):
            for s in (-1, 1):
                sweep(p, ridge_m, [(xm, yy, zr + 0.05), (xm + s * hw, yy, z0 - drop + 0.05)],
                      [(-0.03, -0.12), (0.03, -0.12), (0.03, 0.06), (-0.03, 0.06)])
        if ends:
            gable_end(p, WallFace('+y', y1, 0.0), x0, x1, z0, rise, t=t)
            gable_end(p, WallFace('-y', y0, 0.0), x0, x1, z0, rise, t=t)
        if trusses:
            for yy in splits(y0 + 1.5, y1 - 1.5, 3.0):
                truss(p, (x0 + t, yy, z0), (x1 - t, yy, z0), rise * 0.92)


def truss(p, a, b, h, m='Metal#truss', r=0.05):
    """A tesoura de aço (o banzo de baixo, as 2 pernas, o pendural e as diagonais) de a até b, a altura h no meio."""
    A, B = Vector(a), Vector(b)
    M = (A + B) / 2 + Vector((0, 0, h))
    tube(p, m, [tuple(A), tuple(B)], r, seg=6)
    tube(p, m, [tuple(A), tuple(M)], r, seg=6)
    tube(p, m, [tuple(B), tuple(M)], r, seg=6)
    tube(p, m, [tuple((A + B) / 2), tuple(M)], r * 0.8, seg=6)
    for (q, e) in ((A + (B - A) * 0.25, A + (M - A) * 0.5), (A + (B - A) * 0.75, B + (M - B) * 0.5)):
        tube(p, m, [tuple(q), tuple(e)], r * 0.7, seg=6)


# ===================================================================================================== o telhado-serra

def sawtooth_roof(p, x0, x1, y0, y1, z0, n, rise, t=0.35, m='Roof', over=0.25, wall_proud=0.08, vents=(), trusses=True, front=0.35):
    """O TELHADO-SERRA (a assinatura dos pavilhões da Rússia): n dentes ao longo de X, as cumeeiras correndo em Y; cada dente sobe na
    vidraça vertical (virada pra −X, o shed) e desce na água de chapa trapezoidal verde (virada pra +X, pra câmera do jogo) até a calha
    do vale. Os oitões triangulares de painel nas faces ±y (o perfil da serra lê na lateral), o rufo da cumeeira, a calha de cada vale,
    as tesouras de aço por dentro e os exaustores redondos nos vales (vents = [(dente, y)...])."""
    w = (x1 - x0) / n
    for k in range(n):
        xa, xb = x0 + k * w, x0 + (k + 1) * w
        zt = z0 + rise
        # a água de chapa: do alto (xa) até o vale (xb), passando um pouco dos oitões
        last = k == n - 1
        xe = xb - (front + 0.05 if last else 0.12)
        sl = rise / w
        P0 = Vector((xa - 0.06, y0 - over, zt + 0.06))
        Pa = Vector((xe, y0 - over, zt + 0.06 - sl * (xe - xa + 0.06)))
        Pb = Vector((xa - 0.06, y1 + over, zt + 0.06))
        prof_sheet(p, m, P0, Pa, Pb, period=0.36, depth=0.045, drip=0.05)
        # a vidraça do shed (virada pra −X): os caixilhos de aço, o vidro aramado, a faixa de chapa embaixo
        F = WallFace('-x', xa, 0.0)
        g0, g1 = z0 + 0.35, zt - 0.08
        wbox(p, 'Concrete#gutw', F, y0 + t, y1 - t, z0 - 0.3, g0, -0.25, 0.0, bevel=0.03)                  # a mureta do vale
        wbox(p, 'Glass', F, y0 + t, y1 - t, g0, g1, -0.04, -0.02, bevel=0.0)
        for yy in splits(y0 + t, y1 - t, 1.6):
            wbox(p, 'Metal#shed', F, yy - 0.05, yy + 0.05, g0 + 0.05, g1 - 0.05, -0.05, 0.03, bevel=0.0)
        for zz in (g0, g1):
            wbox(p, 'Metal#shed', F, y0 + t, y1 - t, zz - 0.05, zz + 0.05, -0.06, 0.04, bevel=0.0)
        # a calha do vale (no pé da vidraça do dente seguinte, ou atrás da platibanda da frente)
        gx = xb - (front if last else 0.0)
        p.box('Paint#gutv', gx - 0.42, gx - 0.02, y0 + t - 0.05, y1 - t + 0.05, z0 - 0.18, z0 + 0.08, bevel=0.0)
        p.box('Dark', gx - 0.38, gx - 0.06, y0 + t, y1 - t, z0 + 0.04, z0 + 0.09, bevel=0.0)
        # o rufo da cumeeira (por cima do encontro da chapa com a vidraça, assentado nas cristas)
        sweep(p, 'Paint#ridge', [(xa - 0.02, y0 - over - 0.04, zt + 0.115), (xa - 0.02, y1 + over + 0.04, zt + 0.115)],
              [(-0.12, -0.10), (0.02, -0.01), (0.34, -0.01 - 0.34 * sl), (0.34, 0.05 - 0.34 * sl), (0.02, 0.05), (-0.12, 0.03)])
        # os oitões de painel nas faces ±y (o perfil da serra)
        for (yy0, yy1) in ((y0 - wall_proud, y0 + t), (y1 - t, y1 + wall_proud)):
            p.prism_xz('Concrete#gab', [(xa, z0 - 0.02), (xb - 0.04, z0 - 0.02), (xa, zt - 0.02)], yy0, yy1, bevel=0.08)
        # as tesouras do dente (a perna ao longo da água, o montante da vidraça, o banzo, as diagonais)
        if trusses:
            for yy in splits(y0 + 2.0, y1 - 2.0, 4.0):
                A, B, C = Vector((xa + 0.15, yy, zt - 0.25)), Vector((xb - 0.3, yy, z0 - 0.15)), Vector((xa + 0.15, yy, z0 - 0.75))
                D = Vector((xb - 0.3, yy, z0 - 0.75))
                for (q0, q1, rr) in ((A, B, 0.06), (A, C, 0.05), (C, D, 0.06), (D, B, 0.05), (C, (A + B) / 2, 0.04), ((A + B) / 2, (C + D) / 2, 0.04)):
                    tube(p, 'Metal#truss', [tuple(q0), tuple(q1)], rr, seg=6)
    for (k, yy) in vents:
        xb = x0 + (k + 1) * w
        roof_fan(p, xb - w * 0.45, yy, z0 + rise * 0.45 + 0.1)


def roof_fan(p, x, y, z, r=0.42):
    """O exaustor de telhado redondo (o defletor de cata-vento): o pescoço, a coroa de aletas e o chapéu."""
    p.cyl('Paint#fan', (x, y, z - 0.3), 'z', r * 0.7, 1.0, seg=16, bevel=0.0, smooth=True)
    p.cyl('Paint#fan', (x, y, z + 0.32), 'z', r, 0.42, seg=20, r2=r * 0.85, bevel=0.0, smooth=True)
    for k in range(10):
        a = 2 * math.pi * k / 10
        p.obox('Metal#fan', (x + r * math.cos(a), y + r * math.sin(a), z + 0.32), (0.04, 0.18, 0.4), yaw=math.degrees(a) + 30, bevel=0.0)
    p.cyl('Paint#fan', (x, y, z + 0.6), 'z', r * 1.15, 0.12, seg=20, r2=0.06, bevel=0.0, smooth=True)


# ===================================================================================================== estrela e letreiros

def ru_star(p, F, u, z, r, n=0.0, m='Red', rim='White', t=0.03, w=0.1, disc=None, disc_r=None, relief=0.0):
    """(01/11, APROVADA com a imagem `docs/aprovados/modelos/2026-11-01_estrelas_russia_china_APROVADAS.jpg`) a estrela da Rússia: a
    estrela vermelha com as bordas BRANCA, AZUL e VERMELHA (a insígnia das Forças Armadas russas nos aviões de hoje), na parede F em
    (u, z), saindo de n; r = o raio da estrela vermelha do meio. disc: o medalhão redondo atrás (o material), de raio disc_r."""
    if disc:
        R = disc_r or r * 1.9
        c = Vector(F.pt(u, z, n))
        nn = Vector(F.normal())
        p.cyl(disc, tuple(c + nn * 0.04), tuple(nn), R, 0.08, seg=40, bevel=0.0)
        p.torus('White#ring', tuple(c + nn * 0.085), F.ax, R - 0.06, 0.05, seg=40, rseg=6)
        n += 0.08
    for k, (mat, k_r) in enumerate((('Red', 1.53), ('Navy', 1.37), ('White', 1.2), ('Red', 1.0))):
        rr = r * k_r
        pts = star_points(0.0, 0.0, rr, rr * (0.42 if k < 3 else 0.382), rot_deg=90.0)
        if k == 3 and relief > 0.0:
            ru_star_relief(p, F, u, z, rr, n + k * t * 0.5 + t * 0.5, relief * rr)
            continue
        face_prism(p, mat, F, [(u + x, z + y) for (x, y) in pts], n + k * t * 0.5, n + (k + 1) * t * 0.5 + t * 0.5, bevel=0.0)


def ru_star_relief(p, F, u, z, r, n, h, m='Red#relief', r_in=0.382):
    """(04/11) A estrela vermelha do meio em RELEVO FACETADO, como a estrela de metal das Forças Armadas (e a do Kremlin): as 10 faces
    sobem das pontas e dos vãos até o centro, a h da base. A luz pega uma face de cada ponta e a outra fica na sombra: deixa de ser a
    estrela "chata" (o Felipe sobre o Shtab v3: "falta [...] a estrela chata"). O sólido fecha atrás (a base), sem chanfro (as arestas
    vivas é que desenham a estrela)."""
    bb = p.bm(m, 0.0)
    pts = star_points(0.0, 0.0, r, r * r_in, rot_deg=90.0)
    rim = [bb.verts.new(V(*F.pt(u + x, z + y, n))) for (x, y) in pts]
    apex = bb.verts.new(V(*F.pt(u, z, n + h)))
    for i in range(10):
        bb.faces.new([rim[i], rim[(i + 1) % 10], apex])
    bb.faces.new(list(reversed(rim)))


# os traços de cada letra numa caixa 1 × 1 (u, z); letras cirílicas reais, em ordem que não forma palavra; nunca Z nem V
GLYPHS = {
    'Ж': [((0.5, 0.0), (0.5, 1.0)), ((0.02, 0.0), (0.42, 0.5)), ((0.42, 0.5), (0.02, 1.0)), ((0.98, 0.0), (0.58, 0.5)),
          ((0.58, 0.5), (0.98, 1.0)), ((0.3, 0.5), (0.7, 0.5))],
    'Ш': [((0.0, 0.0), (0.0, 1.0)), ((0.5, 0.0), (0.5, 1.0)), ((1.0, 0.0), (1.0, 1.0)), ((0.0, 0.0), (1.0, 0.0))],
    'Д': [((0.18, 0.12), (0.18, 1.0)), ((0.82, 0.12), (0.82, 1.0)), ((0.18, 1.0), (0.82, 1.0)), ((0.0, 0.12), (1.0, 0.12)),
          ((0.0, 0.12), (0.0, -0.12)), ((1.0, 0.12), (1.0, -0.12))],
    'Б': [((0.0, 0.0), (0.0, 1.0)), ((0.0, 1.0), (0.9, 1.0)), ((0.0, 0.55), (0.8, 0.55)), ((0.0, 0.0), (0.8, 0.0)),
          ((0.92, 0.08), (0.92, 0.47))],
    'П': [((0.0, 0.0), (0.0, 1.0)), ((1.0, 0.0), (1.0, 1.0)), ((0.0, 1.0), (1.0, 1.0))],
    'Л': [((0.0, 0.0), (0.22, 1.0)), ((1.0, 0.0), (1.0, 1.0)), ((0.22, 1.0), (1.0, 1.0))],
    'Ы': [((0.0, 0.0), (0.0, 1.0)), ((0.0, 0.55), (0.5, 0.55)), ((0.0, 0.0), (0.5, 0.0)), ((0.6, 0.08), (0.6, 0.47)),
          ((1.0, 0.0), (1.0, 1.0))],
    'Ц': [((0.0, 0.1), (0.0, 1.0)), ((0.8, 0.1), (0.8, 1.0)), ((0.0, 0.1), (1.0, 0.1)), ((1.0, 0.1), (1.0, -0.15))],
    'Г': [((0.0, 0.0), (0.0, 1.0)), ((0.0, 1.0), (0.9, 1.0))],
    'Я': [((1.0, 0.0), (1.0, 1.0)), ((0.12, 1.0), (1.0, 1.0)), ((0.12, 0.52), (1.0, 0.52)), ((0.02, 0.6), (0.02, 0.92)),
          ((0.55, 0.52), (0.02, 0.0))],
    'Э': [((0.08, 1.0), (1.0, 1.0)), ((1.0, 0.0), (1.0, 1.0)), ((0.08, 0.0), (1.0, 0.0)), ((0.35, 0.5), (1.0, 0.5))],
    'И': [((0.0, 0.0), (0.0, 1.0)), ((1.0, 0.0), (1.0, 1.0)), ((0.0, 0.0), (1.0, 1.0))],
    '0': [((0.0, 0.0), (0.0, 1.0)), ((1.0, 0.0), (1.0, 1.0)), ((0.0, 0.0), (1.0, 0.0)), ((0.0, 1.0), (1.0, 1.0))],
    '1': [((0.6, 0.0), (0.6, 1.0)), ((0.6, 1.0), (0.2, 0.72))],
    '4': [((0.75, 0.0), (0.75, 1.0)), ((0.0, 0.35), (1.0, 0.35)), ((0.0, 0.35), (0.0, 1.0))],
    '6': [((0.0, 0.0), (0.0, 1.0)), ((0.0, 1.0), (0.9, 1.0)), ((0.0, 0.5), (1.0, 0.5)), ((1.0, 0.0), (1.0, 0.5)), ((0.0, 0.0), (1.0, 0.0))],
    '7': [((0.0, 1.0), (1.0, 1.0)), ((1.0, 1.0), (0.45, 0.0))],
    '-': [((0.15, 0.5), (0.85, 0.5))],
}


READ = {'+x': -1.0, '-x': 1.0, '+y': 1.0, '-y': -1.0}     # o sentido de leitura (u) de quem olha a face de fora


def stencil(p, F, u, z, text, h=0.6, m='White', n=0.0, depth=0.012, weight=0.15, aspect=0.62, center=True):
    """O letreiro em cirílico ESTÊNCIL pintado na face F: as letras de traço grosso (weight × altura), a altura h, o pé da linha em z.
    A linha lê da esquerda pra direita pra quem olha a face de fora (READ)."""
    rd = READ[F.face]
    w = h * aspect
    gap = h * 0.28
    total = len(text) * w + (len(text) - 1) * gap
    s0 = -total / 2 if center else 0.0
    sw = h * weight
    for i, ch in enumerate(text):
        if ch == ' ' or ch not in GLYPHS:
            continue
        ox = s0 + i * (w + gap)
        for (A, B) in GLYPHS[ch]:
            stroke(p, m, F, (u + rd * (ox + A[0] * w), z + A[1] * h), (u + rd * (ox + B[0] * w), z + B[1] * h), sw, n, n + depth)
    return total


def sign_board(p, F, u, z, text, h=0.5, board='Red', ink='White', n=0.0, pad=0.25, posts=0.0):
    """A placa de chapa com o letreiro estêncil (o quadro de aço, a chapa pintada, as letras), presa na parede (n) ou nos postes."""
    w = len(text) * h * 0.62 + (len(text) - 1) * h * 0.28 + 2 * pad
    zz = z - pad
    wbox(p, 'Metal#board', F, u - w / 2 - 0.05, u + w / 2 + 0.05, zz - 0.05, z + h + pad + 0.05, n, n + 0.05, bevel=0.012)
    wbox(p, board, F, u - w / 2, u + w / 2, zz, z + h + pad, n + 0.05, n + 0.07, bevel=0.0)
    stencil(p, F, u, z, text, h=h, m=ink, n=n + 0.07)
    return w


def roof_slogan(p, x, yc, z, text, h=1.2, m='Red', frame='Metal#slog'):
    """O letreiro de telhado soviético: as letras grandes em chapa (Red, 12 cm de espessura) na armação de aço, viradas pra +X (pra
    câmera). Na ordem que não forma palavra."""
    F = WallFace('+x', x, 0.0)
    total = stencil(p, F, yc, z + 0.35, text, h=h, m=m, n=0.0, depth=0.1, weight=0.17)
    for yy in splits(yc - total / 2 - 0.2, yc + total / 2 + 0.2, 1.6):
        p.box(frame, x - 0.6, x - 0.02, yy - 0.04, yy + 0.04, z, z + h + 0.5, bevel=0.0)
        stroke(p, frame, WallFace('+y', yy, 0.0), (x - 1.2, z), (x - 0.04, z + h + 0.4), 0.06, -0.03, 0.03)
    for zz in (z + 0.3, z + h + 0.45):
        p.box(frame, x - 0.12, x - 0.02, yc - total / 2 - 0.25, yc + total / 2 + 0.25, zz - 0.04, zz + 0.04, bevel=0.0)


# ===================================================================================================== chaminé, torres e mastros

def striped_chimney(p, c, z0, h, r0, r1, bands=7, m_a='Red', m_b='White', platforms=(0.62, 0.9), ladder_side=0.0, base_h=1.2):
    """A CHAMINÉ LISTRADA (a assinatura da usina): o cone de concreto afinando, as faixas da sinalização de aviação (ímpares, a de cima e
    a de baixo vermelhas), o pé mais largo com a porta de limpeza, a coroa de fuligem na boca e a boca escura, as plataformas em anel
    com o guarda-corpo e as luzes de obstáculo (Beacon), a escada com a gaiola e o para-raios."""
    cx, cy = c
    seg = 32
    # o pé: o soco largo chanfrado e o anel
    p.cyl('Concrete#chb', (cx, cy, z0 + base_h / 2), 'z', r0 + 0.35, base_h, seg=seg, r2=r0 + 0.2, bevel=0.06)
    wb = WallFace('+x', cx + r0 + 0.25, 0.0)
    wbox(p, 'Metal#door', wb, cy - 0.35, cy + 0.35, z0 + 0.05, z0 + 1.0, -0.05, 0.12, bevel=0.012)
    zb = z0 + base_h
    hh = h - base_h
    for k in range(bands):
        za, zc = zb + hh * k / bands, zb + hh * (k + 1) / bands
        ra, rc = lerp(r0, r1, k / bands), lerp(r0, r1, (k + 1) / bands)
        mm = m_a if (bands - 1 - k) % 2 == 0 else m_b
        p.cyl(mm + '#chim', (cx, cy, (za + zc) / 2), 'z', ra, zc - za + 0.004, seg=seg, r2=rc, bevel=0.0, smooth=True)
    zt = z0 + h
    p.cyl('Dark#soot', (cx, cy, zt + 0.25), 'z', r1 + 0.08, 0.5, seg=seg, r2=r1 + 0.04, bevel=0.0, smooth=True)
    p.torus('Dark#soot', (cx, cy, zt + 0.5), 'z', r1 - 0.02, 0.09, seg=seg, rseg=8)
    p.cyl('Dark', (cx, cy, zt + 0.1), 'z', r1 - 0.12, 0.8, seg=seg, bevel=0.0)
    # as plataformas (o anel de grade, as mãos-francesas, o guarda-corpo, as luzes)
    for fr in platforms:
        zp = zb + hh * fr
        rr = lerp(r0, r1, fr)
        disc_ring(p, 'Metal#plat', c, rr + 0.9, rr - 0.02, zp - 0.08, zp, seg=seg)
        for k in range(8):
            a = 2 * math.pi * (k + 0.5) / 8
            q0 = (cx + rr * math.cos(a), cy + rr * math.sin(a), zp - 0.9)
            q1 = (cx + (rr + 0.8) * math.cos(a), cy + (rr + 0.8) * math.sin(a), zp - 0.08)
            tube(p, 'Metal#plat', [q0, q1], 0.04, seg=6)
        for k in range(16):
            a = 2 * math.pi * (k + 0.5) / 16
            p.cyl('Metal#rail', (cx + (rr + 0.86) * math.cos(a), cy + (rr + 0.86) * math.sin(a), zp + 0.5), 'z', 0.025, 1.0, seg=6, bevel=0.0)
        p.torus('Metal#rail', (cx, cy, zp + 1.0), 'z', rr + 0.86, 0.035, seg=seg, rseg=6)
        p.torus('Metal#rail', (cx, cy, zp + 0.5), 'z', rr + 0.86, 0.025, seg=seg, rseg=6)
        for k in range(3):
            a = 2 * math.pi * k / 3 + 0.4
            q = (cx + (rr + 0.1) * math.cos(a), cy + (rr + 0.1) * math.sin(a), zp + 0.12)
            p.cyl('Dark', q, 'z', 0.09, 0.12, seg=10, bevel=0.0)
            p.cyl('Beacon', (q[0], q[1], q[2] + 0.13), 'z', 0.08, 0.14, seg=10, bevel=0.0, smooth=True)
    for k in range(3):                                                          # as luzes do topo
        a = 2 * math.pi * k / 3 + 1.0
        q = (cx + (r1 + 0.12) * math.cos(a), cy + (r1 + 0.12) * math.sin(a), zt - 0.25)
        p.cyl('Beacon', q, 'z', 0.08, 0.16, seg=10, bevel=0.0, smooth=True)
    # a escada com a gaiola (do pé até a boca), no lado ladder_side (graus)
    a = math.radians(ladder_side)
    ca, sa = math.cos(a), math.sin(a)
    tx, ty = -sa, ca

    def lp(fr, out, side):
        rr = lerp(r0, r1, fr) + out
        return (cx + rr * ca + tx * side, cy + rr * sa + ty * side, zb + hh * fr)
    for side in (-0.25, 0.25):
        tube(p, 'Metal#lad', [lp(0.0, 0.2, side), lp(1.0, 0.2, side)], 0.03, seg=6)
    for k in range(int(hh / 0.4)):
        fr = (k + 0.5) / int(hh / 0.4)
        tube(p, 'Metal#lad', [lp(fr, 0.2, -0.25), lp(fr, 0.2, 0.25)], 0.022, seg=4)
    for k in range(int(hh / 1.1)):
        fr = (k + 0.7) / int(hh / 1.1)
        q = Vector(lp(fr, 0.55, 0.0))
        p.torus('Metal#lad', tuple(q), 'z', 0.38, 0.025, seg=12, rseg=4)
    for ph in (-60.0, 0.0, 60.0):
        o, sd = 0.55 + 0.38 * math.cos(math.radians(ph)), 0.38 * math.sin(math.radians(ph))
        tube(p, 'Metal#lad', [lp(0.08, o, sd), lp(1.0, o, sd)], 0.022, seg=4)
    p.cyl('Metal#rod', (cx + r1 * 0.5, cy, zt + 1.2), 'z', 0.025, 2.0, seg=6, bevel=0.0)


def shukhov_tower(p, c, z0, sections, n=18, r_tube=0.06, ring_r=0.075, mats=('Red', 'White'), rings_per=4):
    """A TORRE DE SHUKHOV: seções de hiperboloide, cada uma feita de vigas RETAS ligando o anel de baixo ao de cima com um giro (as duas
    famílias cruzadas, a malha em losango que se estreita na cintura), os anéis horizontais, as cores alternadas por seção (a sinalização
    de aviação). sections = [(raio embaixo, raio em cima, altura, giro em graus)...]. Devolve o z do topo."""
    cx, cy = c
    z = z0
    for k, (rb, rt, h, tw) in enumerate(sections):
        m = mats[k % len(mats)] + '#shu'
        for s in (1, -1):
            for i in range(n):
                a0 = 2 * math.pi * i / n
                a1 = a0 + s * math.radians(tw)
                tube(p, m, [(cx + rb * math.cos(a0), cy + rb * math.sin(a0), z), (cx + rt * math.cos(a1), cy + rt * math.sin(a1), z + h)],
                     r_tube, seg=6, smooth=False)
        A = Vector((rb, 0.0))
        for j in range(rings_per + 1):
            tt = j / rings_per
            Bv = Vector((rt * math.cos(math.radians(tw)), rt * math.sin(math.radians(tw))))
            rr = (A * (1 - tt) + Bv * tt).length
            p.torus(m, (cx, cy, z + h * tt), 'z', rr, ring_r * (1.6 if j in (0, rings_per) else 1.0), seg=36, rseg=6)
        z += h
    return z


def lattice_mast(p, c, z0, z1, a0, a1, levels, m='Metal#mast', leg=0.07, brace=0.04, tri=True):
    """O mastro treliçado (triangular por padrão: o mastro de antenas russo), de meio-lado a0 no pé a a1 no topo, os anéis e o
    zigue-zague em cada face."""
    k_n = 3 if tri else 4

    def corner(k, z):
        t = (z - z0) / (z1 - z0)
        a = lerp(a0, a1, t)
        ang = 2 * math.pi * k / k_n + math.pi / 2
        return (c[0] + a * math.cos(ang), c[1] + a * math.sin(ang), z)
    for k in range(k_n):
        tube(p, m, [corner(k, z0), corner(k, z1)], leg, seg=8)
    for lv in range(levels + 1):
        z = lerp(z0, z1, lv / levels)
        for k in range(k_n):
            tube(p, m, [corner(k, z), corner((k + 1) % k_n, z)], brace * 1.2, seg=6)
    for lv in range(levels):
        za, zb = lerp(z0, z1, lv / levels), lerp(z0, z1, (lv + 1) / levels)
        for k in range(k_n):
            a, b = (k, (k + 1) % k_n) if lv % 2 == 0 else ((k + 1) % k_n, k)
            tube(p, m, [corner(a, za), corner(b, zb)], brace, seg=6)


def flag_pole(p, x, y, z0, h, cloth=(1.8, 1.15), m='Paint#pole', phase=0.0, base=True):
    """O mastro com a bandeira na cor do jogador (o pano sai no executor e o jogo o desenha no vento): o pé de concreto, o tubo afinando,
    a esfera do topo, a adriça."""
    if base:
        p.cyl('Concrete#flagb', (x, y, z0 + 0.2), 'z', 0.38, 0.4, seg=16, r2=0.3, bevel=0.04)
    p.cyl(m, (x, y, z0 + h / 2), 'z', 0.075, h, seg=12, r2=0.045, bevel=0.0, smooth=True)
    p.cyl('White#ball', (x, y, z0 + h + 0.08), 'z', 0.09, 0.16, seg=12, r2=0.03, bevel=0.0, smooth=True)
    tube(p, 'Dark#cable', [(x + 0.08, y, z0 + 1.2), (x + 0.08, y, z0 + h - 0.1)], 0.012, seg=4)
    model_lib.flag(p, (x, y), z0 + h - 0.12, cloth[0], cloth[1], m='Team', phase=phase)


def floodlight_mast(p, x, y, z0, h, face=0.0, n=4):
    """O mastro de holofotes (o прожекторная мачта): o tubo afinando, a plataforma com o guarda-corpo e a fileira de holofotes virados pra
    'face' (graus, 0 = +X)."""
    p.cyl('Concrete#mastb', (x, y, z0 + 0.25), 'z', 0.35, 0.5, seg=16, bevel=0.04)
    p.cyl('Paint#mast', (x, y, z0 + h / 2), 'z', 0.14, h, seg=12, r2=0.09, bevel=0.0, smooth=True)
    zp = z0 + h
    p.box('Metal#plat', x - 0.7, x + 0.7, y - 0.45, y + 0.45, zp - 0.06, zp, bevel=0.0)
    a = math.radians(face)
    for k in range(n):
        off = (k - (n - 1) / 2) * 0.34
        q = (x + math.cos(a) * 0.3 - math.sin(a) * off, y + math.sin(a) * 0.3 + math.cos(a) * off, zp + 0.3)
        p.obox('Dark#lamp', q, (0.28, 0.3, 0.3), yaw=face, pitch=-25, bevel=0.012)
        q2 = (q[0] + math.cos(a) * 0.15, q[1] + math.sin(a) * 0.15, q[2] - 0.06)
        p.obox('Light', q2, (0.03, 0.24, 0.24), yaw=face, pitch=-25, bevel=0.0)
    for (dx, dy) in ((-0.7, -0.45), (0.7, -0.45), (0.7, 0.45), (-0.7, 0.45)):
        p.cyl('Metal#rail', (x + dx, y + dy, zp + 0.45), 'z', 0.025, 0.9, seg=6, bevel=0.0)
    ring_boxes(p, 'Metal#rail', x - 0.72, x + 0.72, y - 0.47, y + 0.47, zp + 0.86, zp + 0.92, 0.04, bevel=0.0)


# ===================================================================================================== o pátio

def star_gate(p, x, y0, y1, h=2.6, open_deg=0.0, m='Roof'):
    """O PORTÃO MILITAR soviético: as 2 folhas de chapa verde com a estrela vermelha contornada em cada uma, nos pilares de concreto com
    o capitel; open_deg: o quanto as folhas abrem pra fora (+X), 0 = fechado, ~95 = encostadas na cerca."""
    W = (y1 - y0) / 2
    for (yy, s) in ((y0, 1), (y1, -1)):
        p.box('Concrete#pier', x - 0.3, x + 0.3, yy - s * 0.33 - 0.3, yy - s * 0.33 + 0.3, 0.0, h + 0.5, bevel=0.06)
        p.box('Concrete#pier', x - 0.36, x + 0.36, yy - s * 0.33 - 0.36, yy - s * 0.33 + 0.36, h + 0.5, h + 0.62, bevel=0.03)
    for (yy, s) in ((y0, 1), (y1, -1)):
        oa = math.radians(open_deg)
        # a folha: o plano vertical que gira em volta da dobradiça (x, yy), abrindo pra fora (+X)
        dirx, diry = math.sin(oa), s * math.cos(oa)
        nrm = (diry, -dirx)                     # a normal da folha no plano XY
        L = W - 0.08
        hx, hy = x + 0.1 * math.sin(oa), yy

        def P(u, z, nn):
            return (hx + dirx * u + nrm[0] * nn, hy + diry * u + nrm[1] * nn, z)
        # o quadro e a chapa
        for (u0, u1, z0, z1, nb) in ((0.0, L, 0.15, 0.27, 0.05), (0.0, L, h - 0.12, h, 0.05), (0.003, 0.12, 0.26, h - 0.11, 0.047),
                                     (L - 0.12, L - 0.003, 0.26, h - 0.11, 0.047), (0.11, L - 0.11, h / 2 - 0.06, h / 2 + 0.06, 0.044)):
            hexa(p, 'Paint#gate', [P(u, z, nn) for z in (z0, z1) for nn in (-nb, nb) for u in (u0, u1)], bevel=0.02)
        P0 = Vector(P(0.12, h - 0.12, 0.0))
        Pa = Vector(P(0.12, 0.27, 0.0))
        Pb = Vector(P(L - 0.12, h - 0.12, 0.0))
        prof_sheet(p, m, P0, Pa, Pb, period=0.3, depth=0.035, th=0.012, outward=Vector((nrm[0], nrm[1], 0.0)))
        prof_sheet(p, m, P0, Pa, Pb, period=0.3, depth=0.035, th=0.012, outward=Vector((-nrm[0], -nrm[1], 0.0)))
        # a estrela contornada nas 2 faces da folha
        for sg in (1, -1):
            cu, cz = L / 2, h * 0.58
            R = min(L, h) * 0.22
            pw = star_points(0.0, 0.0, R * 1.32, R * 0.382 + 0.124 * R, rot_deg=90.0)
            pr = star_points(0.0, 0.0, R, R * 0.382, rot_deg=90.0)
            for (pts, mm, n0) in ((pw, 'White', 0.05), (pr, 'Red', 0.07)):
                ring = lambda nn: [P(cu + q[0], cz + q[1], sg * nn) for q in pts]
                p.loft(mm, [ring(n0), ring(n0 + 0.02)], bevel=0.0)
        for zz in (0.5, h - 0.5):                                               # as dobradiças
            p.cyl('Metal#hinge', (hx, hy, zz), 'z', 0.07, 0.25, seg=10, bevel=0.0)


def po2_fence(p, a, b, h=2.0, z0=0.0, panel=4.0, side=1, m='Concrete#po2'):
    """A CERCA DE CONCRETO PO-2 (a de todo quartel e fábrica soviética): os painéis de 4 m com o RELEVO DE LOSANGOS na face de fora
    (a face do lado (−dy, dx) × side do sentido a→b), entre os pilares, e o arame em cima."""
    (ax, ay), (bx, by) = a, b
    L = math.hypot(bx - ax, by - ay)
    dx, dy = (bx - ax) / L, (by - ay) / L
    nx, ny = -dy * side, dx * side
    k_n = max(1, int(round(L / panel)))
    t = 0.16
    for k in range(k_n):
        s0, s1 = L * k / k_n + 0.12, L * (k + 1) / k_n - 0.12

        def P(s, z, n):
            return (ax + dx * s + nx * n, ay + dy * s + ny * n, z)
        hexa(p, m, [P(s, z, n) for z in (z0 + 0.05, z0 + h) for n in (-t / 2, t / 2) for s in (s0, s1)], bevel=0.04)
        # os losangos em relevo (a pirâmide rasa de cada losango, a face de fora)
        dw, dh = 0.42, 0.3
        cols = int((s1 - s0 - 0.2) / dw)
        rows = int((h - 0.35) / dh)
        su0 = (s0 + s1) / 2 - cols * dw / 2
        bb = p.bm(m + 'r', 0.0)
        for i in range(cols):
            for j in range(rows):
                cs, cz = su0 + (i + 0.5) * dw, z0 + 0.22 + (j + 0.5) * dh
                base = [P(cs - dw / 2, cz, t / 2 - 0.004), P(cs, cz - dh / 2, t / 2 - 0.004), P(cs + dw / 2, cz, t / 2 - 0.004), P(cs, cz + dh / 2, t / 2 - 0.004)]
                top = [P(cs - dw * 0.18, cz, t / 2 + 0.035), P(cs, cz - dh * 0.18, t / 2 + 0.035), P(cs + dw * 0.18, cz, t / 2 + 0.035), P(cs, cz + dh * 0.18, t / 2 + 0.035)]
                vb = [bb.verts.new(V(*q)) for q in base]
                vt = [bb.verts.new(V(*q)) for q in top]
                bb.faces.new(vt)
                for q in range(4):
                    r = (q + 1) % 4
                    bb.faces.new([vb[q], vb[r], vt[r], vt[q]])
                bb.faces.new(list(reversed(vb)))
    for k in range(k_n + 1):                                                    # os pilares
        s = L * k / k_n
        cx, cy = ax + dx * s, ay + dy * s
        p.box('Concrete#post', cx - 0.15, cx + 0.15, cy - 0.15, cy + 0.15, z0, z0 + h + 0.25, bevel=0.04)
    for (zz, r) in ((z0 + h + 0.35, 0.012), (z0 + h + 0.5, 0.012)):            # o arame em cima
        tube(p, 'Metal#wire', [(ax, ay, zz), (bx, by, zz)], 0.015, seg=4)


def boom_barrier(p, x, y, length=5.0, angle=0.0, yaw=90.0):
    """A CANCELA listrada vermelha e branca: o pilar com o contrapeso, o braço em faixas alternadas, o descanso do outro lado e a placa
    redonda de 'proibido' (o círculo vermelho com a barra branca). angle: o quanto o braço levanta (graus). O braço corre em yaw."""
    a = math.radians(yaw)
    dx, dy = math.cos(a), math.sin(a)
    p.box('Paint#bpost', x - 0.25, x + 0.25, y - 0.25, y + 0.25, 0.0, 1.1, bevel=0.04)
    pv = Vector((x, y, 1.0))
    el = math.radians(angle)
    d = Vector((dx * math.cos(el), dy * math.cos(el), math.sin(el)))
    nb = 8
    for k in range(nb):
        q0 = pv + d * (0.2 + (length - 0.2) * k / nb)
        q1 = pv + d * (0.2 + (length - 0.2) * (k + 1) / nb)
        r = 0.07 * (1.0 - 0.35 * k / nb)
        tube(p, ('Red' if k % 2 == 0 else 'White') + '#boom', [tuple(q0), tuple(q1)], r, seg=10)
    p.obox('Metal#cw', tuple(pv - d * 0.45), (0.7, 0.3, 0.3), yaw=yaw, pitch=angle, bevel=0.03)
    p.cyl('Metal#hinge', (x, y, 1.0), (-dy, dx, 0.0), 0.12, 0.55, seg=12, bevel=0.0)
    e = (x + dx * length, y + dy * length)
    p.cyl('Paint#bpost', (e[0], e[1], 0.45), 'z', 0.06, 0.9, seg=8, bevel=0.0)
    p.box('Paint#bpost', e[0] - 0.12, e[0] + 0.12, e[1] - 0.12, e[1] + 0.12, 0.88, 0.95, bevel=0.0)
    # a placa de 'proibido' no pilar
    c = (x + 0.3 * math.cos(a + math.pi / 2), y + 0.3 * math.sin(a + math.pi / 2), 1.7)
    p.cyl('Paint#bpost', (c[0], c[1], 1.25), 'z', 0.04, 0.9, seg=6, bevel=0.0)
    p.cyl('Red', c, 'x', 0.32, 0.04, seg=24, bevel=0.0)
    p.box('White', c[0] + 0.02, c[0] + 0.03, c[1] - 0.22, c[1] + 0.22, c[2] - 0.05, c[2] + 0.05, bevel=0.0)


def sentry_mushroom(p, x, y, z0=0.0):
    """O 'cogumelo' da sentinela (o грибок): o poste listrado e o chapéu redondo em cone, com a faixa vermelha na borda."""
    for k in range(5):
        p.cyl(('Red' if k % 2 == 0 else 'White') + '#post', (x, y, z0 + 0.25 + k * 0.5), 'z', 0.09, 0.5, seg=12, bevel=0.0, smooth=True)
    revolve(p, 'Roof#mush', (x, y), [(0.12, z0 + 2.45), (1.0, z0 + 2.4), (1.0, z0 + 2.52), (0.7, z0 + 2.75), (0.08, z0 + 3.0)], seg=32,
            smooth=False)
    p.torus('Red#mush', (x, y, z0 + 2.45), 'z', 0.99, 0.06, seg=32, rseg=6)
    p.cyl('Concrete#flagb', (x, y, z0 + 0.06), 'z', 0.9, 0.12, seg=24, bevel=0.03)


def ammo_crate(p, x, y, z, yaw=0.0, size=(1.2, 0.55, 0.42), m='Roof#crate'):
    """A caixa de munição de madeira pintada de verde (as ripas das tampas, as alças de corda, as travas) — a do exército russo."""
    p.obox(m, (x, y, z + size[2] / 2), size, yaw=yaw, bevel=0.025)
    a = math.radians(yaw)
    for s in (-1, 1):
        o = s * (size[0] / 2 - 0.06)
        p.obox('Dark#latch', (x + o * math.cos(a), y + o * math.sin(a), z + size[2] * 0.62), (0.05, size[1] + 0.02, 0.08), yaw=yaw, bevel=0.0)
        o2 = s * (size[0] / 2 + 0.015)
        p.obox(m + 'e', (x + o2 * math.cos(a), y + o2 * math.sin(a), z + size[2] / 2), (0.03, size[1] * 0.85, size[2] * 0.8), yaw=yaw, bevel=0.0)
    p.obox(m + 'l', (x, y, z + size[2] + 0.012), (size[0] * 0.92, size[1] * 0.86, 0.025), yaw=yaw, bevel=0.0)
    p.obox('White#sten', (x + 0.0, y + 0.0, z + size[2] * 0.5), (size[0] * 0.45, size[1] + 0.004, 0.06), yaw=yaw, bevel=0.0)


def container(p, x, y, z0=0.0, L=6.06, W=2.44, H=2.59, yaw=0.0, m='Paint#cont', doors='+x'):
    """O contêiner de aço (o de 20 pés soviético): as paredes de chapa ondulada (as nervuras verticais), o quadro das quinas, as portas
    com as 4 barras de trava na ponta, o teto liso."""
    a = math.radians(yaw)
    ca, sa = math.cos(a), math.sin(a)

    def R(u, v, z):
        return (x + u * ca - v * sa, y + u * sa + v * ca, z)
    p.obox(m + 'f', (x, y, z0 + H / 2), (L - 0.02, W - 0.2, H - 0.02), yaw=yaw, bevel=0.04)        # o miolo
    # as nervuras dos lados (a chapa trapezoidal vertical)
    for s in (-1, 1):
        P0 = Vector(R(-L / 2 + 0.16, s * (W / 2 - 0.04), z0 + H - 0.16))
        Pa = Vector(R(-L / 2 + 0.16, s * (W / 2 - 0.04), z0 + 0.16))
        Pb = Vector(R(L / 2 - 0.16, s * (W / 2 - 0.04), z0 + H - 0.16))
        prof_sheet(p, m, P0, Pa, Pb, period=0.28, depth=0.05, th=0.03, outward=Vector((-sa * s, ca * s, 0.0)))
    # o quadro (os pilares das quinas e as longarinas)
    for su in (-1, 1):
        for sv in (-1, 1):
            p.obox(m + 'c', R(su * (L / 2 - 0.08), sv * (W / 2 - 0.08), z0 + H / 2), (0.17, 0.17, H), yaw=yaw, bevel=0.02)
        for zz in (z0 + 0.08, z0 + H - 0.08):
            p.obox(m + 'c', R(0.0, su * (W / 2 - 0.06), zz), (L, 0.13, 0.16), yaw=yaw, bevel=0.02)
            p.obox(m + 'c', R(su * (L / 2 - 0.06), 0.0, zz), (0.13, W, 0.16), yaw=yaw, bevel=0.02)
    # as barras de trava da porta
    du = L / 2 + 0.03 if doors == '+x' else -L / 2 - 0.03
    for v in (-0.85, -0.35, 0.35, 0.85):
        p.cyl('Metal#bar', R(du, v, z0 + H / 2), 'z', 0.025, H - 0.3, seg=6, bevel=0.0)
        p.obox('Metal#bar', R(du + 0.02 * (1 if du > 0 else -1), v + 0.08, z0 + 1.1), (0.04, 0.2, 0.06), yaw=yaw, bevel=0.0)


def transformer(p, x, y, z0=0.0, sx=2.2, sy=1.6, h=2.0, m='Paint#trf'):
    """O transformador da subestação: o tanque chanfrado, os radiadores de aletas dos 2 lados (o ritmo), o conservador em cima, as 3
    buchas de porcelana branca em discos, a base de trilhos."""
    p.box('Concrete#trfb', x - sx / 2 - 0.4, x + sx / 2 + 0.4, y - sy / 2 - 0.9, y + sy / 2 + 0.9, z0, z0 + 0.3, bevel=0.05)
    zb = z0 + 0.3
    p.box(m, x - sx / 2, x + sx / 2, y - sy / 2, y + sy / 2, zb + 0.1, zb + h, bevel=0.06)
    for s in (-1, 1):
        for k in range(7):
            xx = x - sx / 2 + 0.25 + k * (sx - 0.5) / 6
            p.box(m + 'fin', xx - 0.03, xx + 0.03, y + s * (sy / 2) - (0.0 if s > 0 else 0.55), y + s * (sy / 2) + (0.55 if s > 0 else 0.0),
                  zb + 0.3, zb + h - 0.25, bevel=0.0)
        tube(p, m + 'fin', [(x - sx / 2 + 0.2, y + s * (sy / 2 + 0.3), zb + h - 0.2), (x + sx / 2 - 0.2, y + s * (sy / 2 + 0.3), zb + h - 0.2)], 0.06, seg=8)
    p.cyl(m, (x - sx * 0.2, y, zb + h + 0.55), 'x', 0.28, sx * 0.55, seg=16, bevel=0.0, smooth=True)
    tube(p, m, [(x - sx * 0.2, y, zb + h + 0.3), (x - sx * 0.2, y, zb + h)], 0.05, seg=8)
    for k in range(3):
        yy = y - sy * 0.3 + k * sy * 0.3
        for j in range(5):
            p.cyl('White#ins', (x + sx * 0.28, yy, zb + h + 0.1 + j * 0.12), 'z', 0.13 if j % 2 == 0 else 0.09, 0.08, seg=12, bevel=0.0)
        p.cyl('Metal#bus', (x + sx * 0.28, yy, zb + h + 0.75), 'z', 0.04, 0.2, seg=6, bevel=0.0)
    for s in (-1, 1):
        p.box('Metal#rail', x - sx / 2 - 0.2, x + sx / 2 + 0.2, y + s * 0.5 - 0.06, y + s * 0.5 + 0.06, zb, zb + 0.1, bevel=0.0)


def gas_pipe(p, pts, r=0.11, feet=0.0, step=2.2):
    """O CANO DE GÁS AMARELO (o gás na Rússia corre por cima do chão, pintado de amarelo): o tubo com os cotovelos redondos e as flanges,
    nos pés de aço (os suportes em T a cada 'step')."""
    pipe_run(p, pts, r, m='Yellow#gas', step=step, clamp='Metal#clamp', feet=feet, seg=14)


def valve(p, x, y, z, axis='x', r=0.11):
    """A válvula de gaveta no cano (o corpo, a haste e o volante vermelho)."""
    p.cyl('Yellow#gas', (x, y, z), axis, r * 1.6, 0.32, seg=14, bevel=0.0)
    p.cyl('Metal#valve', (x, y, z + r * 1.6 + 0.2), 'z', 0.04, 0.4, seg=6, bevel=0.0)
    p.torus('Red#valve', (x, y, z + r * 1.6 + 0.42), 'z', 0.2, 0.025, seg=16, rseg=5)
    for ang in (0, 90):
        p.obox('Red#valve', (x, y, z + r * 1.6 + 0.42), (0.38, 0.03, 0.03), yaw=ang, bevel=0.0)


def ladder_on(p, F, u, z0, z1, w=0.5, cage=True, n=0.12, m='Metal#lad'):
    """A escada de marinheiro na parede F (os montantes, os degraus, os suportes) com a gaiola de arcos acima de 2,2 m."""
    for s in (-1, 1):
        wbox(p, m, F, u + s * w / 2 - 0.03, u + s * w / 2 + 0.03, z0, z1 + 0.9, n, n + 0.06, bevel=0.0)
        for zz in splits(z0 + 0.4, z1 - 0.2, 1.5):
            wbox(p, m, F, u + s * w / 2 - 0.02, u + s * w / 2 + 0.02, zz - 0.02, zz + 0.02, 0.0, n, bevel=0.0)
    for k in range(int((z1 - z0) / 0.3)):
        zz = z0 + 0.25 + k * 0.3
        c = F.pt(u, zz, n + 0.03)
        p.cyl(m, c, 'y' if F.ax == 'x' else 'x', 0.02, w, seg=4, bevel=0.0)
    if cage and z1 - z0 > 2.6:
        for zz in splits(z0 + 2.3, z1 + 0.8, 0.9):
            pts = [F.pt(u - w / 2, zz, n + 0.03), F.pt(u - w / 2 - 0.05, zz, n + 0.55), F.pt(u, zz, n + 0.75),
                   F.pt(u + w / 2 + 0.05, zz, n + 0.55), F.pt(u + w / 2, zz, n + 0.03)]
            tube(p, m, fillet(pts, 0.2, n=3), 0.02, seg=4)
        for du in (-w / 2 - 0.05, 0.0, w / 2 + 0.05):
            nn = n + (0.75 if du == 0.0 else 0.55)
            tube(p, m, [F.pt(u + du, z0 + 2.3, nn), F.pt(u + du, z1 + 0.8, nn)], 0.018, seg=4)


def vent_shaft(p, x, y, z, sx=1.0, sy=0.7, h=0.9):
    """A saída de ventilação do teto do prédio de painel (a caixinha de concreto com a grelha e o chapéu de chapa)."""
    p.box('Concrete#vent', x - sx / 2, x + sx / 2, y - sy / 2, y + sy / 2, z, z + h, bevel=0.05)
    for s in (-1, 1):
        p.box('Dark', x - sx / 2 + 0.15, x + sx / 2 - 0.15, y + s * sy / 2 - 0.01, y + s * sy / 2 + 0.01, z + h - 0.45, z + h - 0.15, bevel=0.0)
    p.box('Paint#cap', x - sx / 2 - 0.1, x + sx / 2 + 0.1, y - sy / 2 - 0.1, y + sy / 2 + 0.1, z + h + 0.12, z + h + 0.18, bevel=0.0)
    for (dx, dy) in ((-1, -1), (1, 1), (-1, 1), (1, -1)):
        p.box('Paint#cap', x + dx * (sx / 2 - 0.06) - 0.03, x + dx * (sx / 2 - 0.06) + 0.03, y + dy * (sy / 2 - 0.06) - 0.03,
              y + dy * (sy / 2 - 0.06) + 0.03, z + h - 0.01, z + h + 0.13, bevel=0.0)


def ac_box(p, x, y, z, yaw=0.0):
    """O condensador do teto (a caixa chanfrada com o ventilador redondo em cima e a grade)."""
    p.obox('Paint#ac', (x, y, z + 0.4), (1.3, 0.9, 0.8), yaw=yaw, bevel=0.04)
    p.cyl('Dark', (x, y, z + 0.81), 'z', 0.33, 0.02, seg=20, bevel=0.0)
    p.torus('Metal#ac', (x, y, z + 0.82), 'z', 0.33, 0.025, seg=20, rseg=5)
    for ang in (0, 60, 120):
        p.obox('Metal#ac', (x, y, z + 0.83), (0.66, 0.03, 0.02), yaw=yaw + ang, bevel=0.0)


def tarp_stack(p, x, y, z, sx, sy, h, yaw=0.0, seed=0):
    """A pilha coberta com a lona na cor do jogador (o volume embaixo, a lona com as dobras caindo nos lados, as cordas)."""
    a = math.radians(yaw)
    ca, sa = math.cos(a), math.sin(a)
    ext = 0.22
    Lx, Ly = sx + 2 * ext, sy + 2 * ext

    def R(u, v, zz):
        return (x + u * ca - v * sa, y + u * sa + v * ca, zz)
    p.obox('Wood#stack', (x, y, z + h / 2 - 0.05), (sx - 0.1, sy - 0.1, h - 0.1), yaw=yaw, bevel=0.06)

    def zf(u, v, zz):
        d = min(min(u, 1 - u) * Lx, min(v, 1 - v) * Ly)
        k = max(0.0, 1.0 - d / (ext + 0.12))
        wob = 0.035 * math.sin(u * 23.0 + seed) * math.sin(v * 17.0 + seed * 0.3)
        return z + h + 0.03 - k * k * h * 0.62 + wob * (1 - k) + 0.04 * k * math.sin(u * 31 + v * 29 + seed)
    model_lib.sheet(p, 'Team#tarp', [R(-Lx / 2, -Ly / 2, z), R(Lx / 2, -Ly / 2, z), R(Lx / 2, Ly / 2, z), R(-Lx / 2, Ly / 2, z)],
                    max(6, int(Lx / 0.12)), max(4, int(Ly / 0.12)), z_fn=zf, thick=0.03)
