"""As peças das casas e das miudezas do kit civil v2 (01/11/2026; a lista viva em docs/KIT_CIVIL_V2.md).

A casa da região nos game shots (16_beauty_pass/b_gpt, 15_three_factions/b_gpt, a vila do alvo "a cara do jogo"): o bloco rebocado de
teto plano, a parede de 25 cm com a QUINA GASTA (o chanfro redondo largo), o RODAPÉ de reboco mais grosso e a cinta entre os andares;
as janelas RECUADAS na espessura com a moldura de reboco, o peitoril e a grade; a porta de ferro recuada com a marquise; o parapeito
do teto com a capa e os DENTES (o ritmo dos blocos em cima), os pilares de quina mais altos com o ferro de espera saindo; e o que
lê de cima: a caixa d'água de plástico (o cilindro escuro com os frisos, em pé, às vezes no cavalete), a caixa d'água de aço deitada,
a antena parabólica, o ar-condicionado, a casinha da escada, o varal, o tapete pendurado no parapeito, a escada de fora, o toldo.

Usa o model_building_parts (BPart: o corte dos fundos escondidos; WallFace, wbox, rect_minus: a parede montada em volta do vão) e o
model_vehicle_parts (lathe, rbox, tube). Slots (o nome antes do '#'; CIVIL_SLOTS do import_provisional.py): Plaster, Wood, Metal,
Dark, Glass, Cloth, Rust. Coordenadas da Unreal (X, Y, Z, metros); a frente da casa é +Y (a convenção do kit, giro 0).
"""
import math

import bmesh
from mathutils import Vector

import model_lib
from model_lib import COLORS, V
import model_civ_parts as cp           # as cores de conferência do jogo (COLORS) e o ruído
import model_vehicle_parts as vp
from model_building_parts import BPart, WallFace, wbox, rect_minus, hexa, open_box, EPS

# o chanfro por chave (o 1º uso fixa). No modo prédio o chanfro ≥ 1,5 cm vira ≥ 9 cm (o mínimo que pega luz na câmera); < 1,5 cm dobra
BEVELS = {
    'Plaster': 0.12,          # a parede: a quina gasta, redonda e larga
    'Plaster#cap': 0.09,      # a capa do parapeito e do muro
    'Plaster#tooth': 0.014,   # os dentes do parapeito (o bloco quadrado de quina gasta curta: com 9 cm viravam travesseiros)
    'Plaster#band': 0.014,    # o rodapé e a cinta (a borda de ~3 cm)
    'Plaster#trim': 0.012,    # a moldura da janela, o peitoril
    'Plaster#slab': 0.0,      # a laje (a borda fica dentro do parapeito)
    'Plaster#skirt': 0.0,     # a saia enterrada
    'Plaster#step': 0.012,
    'Plaster#rub': 0.0,       # o entulho (as lascas têm a forma no próprio perfil)
    'Plaster#patch': 0.0,     # o remendo de reboco (a borda irregular está no contorno)
    'Wood': 0.012, 'Metal': 0.008, 'Rust': 0.008, 'Dark': 0.0, 'Glass': 0.0, 'Cloth': 0.0,
    # (v3) a mureta comida (o loft com o topo irregular: o chanfro arredonda a crista, 9 cm no modo prédio), o pilar de canto gasto (a
    # quina redonda está no perfil), a borda do reboco caído e o tijolo de adobe à mostra (a forma no contorno), a fresta
    'Plaster#par': 0.06, 'Plaster#post': 0.0, 'Plaster#rim': 0.0, 'Cloth#brick': 0.0, 'Dark#crack': 0.0, 'Plaster#pier': 0.05,
    # (v3, o orçamento) o caixilho, a grade e a veneziana sem o chanfro de 3 segmentos (recuados no vão, 5 cm de largura: o chanfro
    # mínimo de 9 cm do modo prédio os arredondava inteiros e custava ~1/3 dos triângulos da janela); a moldura e o peitoril de reboco,
    # que pegam o sol, seguem chanfrados
    'Wood#frame': 0.0, 'Metal#frame': 0.0, 'Metal#bar': 0.0, 'Rust#bar': 0.0, 'Wood#shut': 0.0,
}
# (v3) o passo das linhas finas da face de fora (o escorrido); a casa grande pode abrir (o orçamento)
FINE = {'u': 0.14}


# (v3) o que cada casa registra pra sujeira (o escorrido debaixo das janelas, das bicas e da capa da mureta: model_civ_parts.house_dirt,
# aplicado na cor de vértice depois do cozimento das máscaras): {face: [at, [itens], z_topo]}
DIRT = {'walls': {}, 'seed': 0, 'dirt': 0.9}


def dirt_wall(F, z_top=None):
    w = DIRT['walls'].setdefault((F.face, round(F.at, 3)), [F.face, F.at, [], 0.0])
    if z_top is not None:
        w[3] = max(w[3], z_top)
    return w


def new_part(name, cull=True, seed=0, dirt=0.9):
    p = BPart(name, cull=cull)
    for m, b in BEVELS.items():
        p.bm(m, b)
    DIRT['walls'] = {}
    DIRT['seed'] = seed
    DIRT['dirt'] = dirt
    return p


def finish(p):
    """Junta a peça e mostra os triângulos por material; a rede de segurança do chanfro (o vértice que foge volta aos vizinhos).
    (v3) Grava no objeto a spec da sujeira (o gancho do export a aplica depois das máscaras)."""
    o = p.finish()
    if DIRT['walls']:
        import json
        o['civ_dirt'] = json.dumps({'frame': 'house', 'seed': DIRT['seed'], 'dirt': DIRT['dirt'],
                                    'walls': list(DIRT['walls'].values())})
    bm = bmesh.new()
    bm.from_mesh(o.data)
    lim = 80.0
    bad = [v for v in bm.verts if v.co.length > lim]
    for v in bad:
        nb = [e.other_vert(v).co for e in v.link_edges if e.other_vert(v).co.length <= lim]
        if nb:
            v.co = sum(nb, Vector()) / len(nb)
    if bad:
        print('WARN', o.name, len(bad), 'vértices perdidos do chanfro, devolvidos aos vizinhos')
        bm.to_mesh(o.data)
    bm.free()
    cnt = {}
    for poly in o.data.polygons:
        k = o.data.materials[poly.material_index].name if o.data.materials else '?'
        cnt[k] = cnt.get(k, 0) + len(poly.vertices) - 2
    print('TRIS', o.name, sum(cnt.values()), sorted(cnt.items(), key=lambda kv: -kv[1]))
    return o


h3 = cp.h3
lerp = cp.lerp


def out_of_plumb(o, x0, x1, y0, y1, lean=(0.008, -0.006), twist=0.0018, bow=(0.018, -0.014), seed=0):
    """(v3) A parede que não é perfeitamente reta nem vertical: o campo suave (nada de ruído: "massa de modelar" é regra de 08/10) que
    entorta a casa inteira junto, com o pé parado: a INCLINAÇÃO de cada parede (lean: cm por m de subida em x e y, mais a torção
    twist, que faz as paredes opostas tombarem pra lados diferentes) e a BARRIGA em planta (bow: a parede ±x embarriga no meio, a ±y
    também; 1–2 cm). A pegada no chão muda só a barriga; a laje fica na mesma altura (nada se move em z)."""
    me = o.data
    xc, yc = (x0 + x1) / 2, (y0 + y1) / 2
    hx, hy = max(0.1, (x1 - x0) / 2), max(0.1, (y1 - y0) / 2)
    for v in me.vertices:
        x, y, z = v.co.x, -v.co.y, v.co.z
        zz = max(0.0, z)
        dx = zz * (lean[0] + twist * (y - yc) / hy)
        dy = zz * (lean[1] - twist * (x - xc) / hx)
        sy_ = math.sin(math.pi * min(1.0, max(0.0, (y - y0) / (y1 - y0))))
        sx_ = math.sin(math.pi * min(1.0, max(0.0, (x - x0) / (x1 - x0))))
        dx += bow[0] * sy_ * max(-1.0, min(1.0, (x - xc) / hx))
        dy += bow[1] * sx_ * max(-1.0, min(1.0, (y - yc) / hy))
        v.co.x += dx
        v.co.y -= dy
    me.update()


# ===================================================================================================== o corpo da casa
def house_block(p, x0, x1, y0, y1, z_roof, t=0.25, holes=None, slab_t=0.22, floors=(), skirt=0.4, plinth=0.32, band=True,
                interior=True, patches=3, seed=0, spalls=3, cracks=1, wear=1.0, mud=0.35, pocks=10):
    """O bloco rebocado oco: as 4 paredes com espessura t montadas em volta dos vãos (holes = {'+y': [(u0, u1, z0, z1)...]}; u = X
    nas faces ±y, Y nas faces ±x), a laje do teto (z_roof = o topo dela) e as lajes dos andares (floors = [z do piso...]), a cinta
    de reboco por fora em cada andar, o RODAPÉ (o reboco mais grosso no pé, plinth de altura, 3 cm pra fora, interrompido nas portas)
    e a SAIA enterrada até −skirt (o chão aplainado do lote deixa degrau: a casa nunca flutua).
    (v3, 01/11 noite: "quase, muitas quinas, parece lego, sem detalhes, sem texturas") os PILARES DE CANTO gastos (a quina redonda e
    larga, comida no pé, o raio mudando com a altura: corner_post), o rodapé que contorna a quina redonda, a parede com a grade fina
    na horizontal (o escorrido tem vértice onde cair), os REMENDOS (o reboco novo e o de barro, mud = a fração), o REBOCO CAÍDO em
    placas com o tijolo de adobe à mostra (spall), a FRESTA saindo da quina dos vãos (crack)."""
    holes = holes or {}
    at = {'+x': x1, '-x': x0, '+y': y1, '-y': y0}
    walls_with_posts(p, x0, x1, y0, y1, 0.0, z_roof, t, holes, seed=seed, plinth=plinth, wear=wear)
    # a laje do teto e as dos andares (por dentro das paredes), o piso
    p.box('Plaster#slab', x0 + t - 0.03, x1 - t + 0.03, y0 + t - 0.03, y1 - t + 0.03, z_roof - slab_t, z_roof - 0.005, bevel=0.0)
    for zf in floors:
        p.box('Plaster#slab', x0 + t - 0.03, x1 - t + 0.03, y0 + t - 0.03, y1 - t + 0.03, zf - 0.2, zf, bevel=0.0)
    if interior:
        # (v3) o piso sem o fundo (enterrado: a grade do detalhe enchia ele de triângulos)
        open_box(p, 'Plaster#slab', x0 + t - 0.03, x1 - t + 0.03, y0 + t - 0.03, y1 - t + 0.03, -0.05, 0.08, bevel=0.0)
        # a viga do meio debaixo da laje (o furo do dano mostra um prédio, não um papel)
        if (x1 - x0) >= (y1 - y0):
            p.box('Plaster#slab', (x0 + x1) / 2 - 0.12, (x0 + x1) / 2 + 0.12, y0 + t, y1 - t, z_roof - slab_t - 0.3, z_roof - slab_t + 0.01, bevel=0.0)
        else:
            p.box('Plaster#slab', x0 + t, x1 - t, (y0 + y1) / 2 - 0.12, (y0 + y1) / 2 + 0.12, z_roof - slab_t - 0.3, z_roof - slab_t + 0.01, bevel=0.0)
    # a saia enterrada (até −skirt, um pouco pra fora)
    if skirt:
        # (v3) só os 4 lados (o fundo e o topo ficam enterrados / debaixo do piso e das paredes)
        skirt_ring(p, x0 - 0.05, x1 + 0.05, y0 - 0.05, y1 + 0.05, -skirt, 0.02)
    # o rodapé (entre os pilares: na quina ele contorna o pilar redondo, corner_post) e as cintas
    for f in ('+x', '-x', '+y', '-y'):
        F = WallFace(f, at[f], 0.0)
        dirt_wall(F)
        u0, u1 = ((y0 + t, y1 - t) if f[1] == 'x' else (x0 + t, x1 - t))
        doors = [h for h in holes.get(f, ()) if h[2] < 0.1]
        if plinth:
            segs, u = [], u0
            for h in sorted(doors):
                if h[0] - 0.02 > u + 0.1:
                    segs.append((u, h[0] - 0.02))
                u = h[1] + 0.02
            if u1 > u + 0.1:
                segs.append((u, u1))
            for k, (a, b) in enumerate(segs):
                # o rodapé gasto: a altura ondula (o reboco grosso do pé caiu em pedaços)
                plinth_run(p, F, a, b, plinth, seed=seed * 7 + k * 3 + ord(f[1]) + (3 if f[0] == '+' else 0), wear=wear)
        if band:
            for zf in floors:
                wbox(p, 'Plaster#band', F, u0 - 0.25, u1 + 0.25, zf - 0.12, zf + 0.06, -0.02, 0.035)
        fs = seed * 10 + len(f) + ord(f[1]) + (1 if f[0] == '+' else 0) * 5
        hs = holes.get(f, ())
        if patches:
            wall_patches(p, F, u0 + 0.03, u1 - 0.03, 0.3, z_roof, patches * max(1, int((u1 - u0) / 7.0)), seed=fs, holes=hs, mud=mud)
        if spalls:
            n_sp = max(0, int(round(spalls * (u1 - u0) / 7.0 * (0.6 + 0.8 * h3(fs, 1, 77)))))
            wall_spalls(p, F, u0, u1, 0.25, z_roof, n_sp, seed=fs + 101, holes=hs)
        if pocks:
            # (v3) os buracos de tiro em rajada (a vila dos game shots está em guerra: as manchinhas escuras de todas as paredes do alvo)
            pockmarks(p, F, u0, u1, 0.3, z_roof, int(round(pocks * (u1 - u0) / 7.0 * (0.4 + 1.2 * h3(fs, 2, 79)))), seed=fs + 211, holes=hs)
        if cracks:
            for k, h in enumerate([h for h in hs if h[2] > 0.1][:cracks]):
                if h3(fs, k, 91) < 0.45:
                    continue
                crack(p, F, h[1] if h3(fs, k, 93) < 0.5 else h[0], h[3], up=True, seed=fs + k)
    # a laje manchada e rachada (o que mais aparece de cima)
    roof_wear(p, x0 + t, x1 - t, y0 + t, y1 - t, z_roof, seed=seed * 3 + 1, patches=int(round(3 * wear * (x1 - x0) * (y1 - y0) / 55.0)) + 1,
              cracks=int(round(4 * wear * (x1 - x0) * (y1 - y0) / 55.0)) + 1)


def skirt_ring(p, x0, x1, y0, y1, z0, z1, m='Plaster#skirt'):
    """(v3) A saia enterrada só com os 4 lados (as paredes de fora, viradas pra fora)."""
    b = p.bm(m, 0.0)
    c = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    lo = [b.verts.new(V(x, y, z0)) for (x, y) in c]
    hi = [b.verts.new(V(x, y, z1)) for (x, y) in c]
    for i in range(4):
        j = (i + 1) % 4
        b.faces.new([lo[j], lo[i], hi[i], hi[j]])          # (o Y do Blender é trocado: a ordem do V vira horária; assim a normal sai pra fora)


def plinth_run(p, F, u0, u1, h, seed=0, wear=1.0, out=0.03):
    """(v3) O rodapé gasto: a faixa de reboco grosso do pé com a altura ondulando e os pedaços que caíram (o topo cai até 14 cm em
    trechos), um loft ao longo da parede de 14 em 14 cm (o chanfro arredonda a crista)."""
    L = u1 - u0
    n = max(2, int(L / 0.14))
    rings = []
    for k in range(n + 1):
        u = u0 + L * k / n
        e = 0.0
        for j in range(max(1, int(L / 2.5))):
            uc = u0 + L * h3(j, seed, 3)
            w = 0.2 + 0.5 * h3(j, seed, 5)
            dd = 0.04 + 0.10 * h3(j, seed, 7) * wear
            e = max(e, dd * (1.0 - smooth01(abs(u - uc) / w)))
        zt = h - e + 0.012 * (cp.fbm3(u * 2.7, 0.3, 0.1, seed) - 0.5)
        rings.append([F.pt(u, -0.02, -0.02), F.pt(u, -0.02, out), F.pt(u, zt, out), F.pt(u, zt, -0.02)])
    p.loft('Plaster#band', rings, smooth=False)


def smooth01(t):
    return cp.smooth01(t)


def wall_grid(p, m, F, u0, u1, z0, z1, t, holes=(), n0=None, fine=0.14):
    """A parede numa malha contínua furada pelos vãos: a grade das linhas dos vãos, as células cheias com a face de fora e a de
    dentro, e o lado (o batente, a ponta, o topo) só onde a célula cheia encosta numa vazia ou na borda. Os vértices são
    compartilhados: o chanfro do harden só pega a quina de verdade (o batente do vão, a ponta, o topo), não a emenda entre pedaços
    (a parede em faixas soltas arredondava a ponta de cada faixa: o sulco do chão ao teto do lado das janelas).
    (v3) fine: a face de FORA com as linhas de pé a cada ~14 cm (a estria do escorrido tem vértice; a grade do detalhe só corta de 40
    em 40 cm); a de dentro fica na grade grossa (ninguém vê o escorrido por dentro: 1/3 do custo)."""
    n_in = -t if n0 is None else n0
    hs = [(max(a, u0), min(b, u1), max(c, z0), min(d, z1)) for (a, b, c, d) in holes if a < u1 and b > u0 and c < z1 and d > z0]
    uc = sorted(set([u0, u1] + [h[0] for h in hs] + [h[1] for h in hs]))
    sub = []                                     # as linhas finas de cada coluna grossa (com as pontas)
    for a, b in zip(uc[:-1], uc[1:]):
        k = int((b - a) / fine) if fine else 0
        sub.append([a + (b - a) * j / (k + 1) for j in range(k + 2)])
    zs = sorted(set([z0, z1] + [h[2] for h in hs] + [h[3] for h in hs]))
    nu, nz = len(uc) - 1, len(zs) - 1

    def solid(i, j):
        if i < 0 or j < 0 or i >= nu or j >= nz:
            return False
        um, zm = (uc[i] + uc[i + 1]) / 2, (zs[j] + zs[j + 1]) / 2
        return not any(h[0] <= um <= h[1] and h[2] <= zm <= h[3] for h in hs)
    b = p.bm(m, 0.12)
    cache = {}

    def vert(u, j, side):
        k = (round(u, 6), j, side)
        if k not in cache:
            cache[k] = b.verts.new(V(*F.pt(u, zs[j], 0.0 if side else n_in)))
        return cache[k]
    for i in range(nu):
        su = sub[i]
        for j in range(nz):
            if not solid(i, j):
                continue
            for a, c in zip(su[:-1], su[1:]):
                b.faces.new([vert(a, j, 1), vert(c, j, 1), vert(c, j + 1, 1), vert(a, j + 1, 1)])
            b.faces.new([vert(uc[i], j + 1, 0), vert(uc[i + 1], j + 1, 0), vert(uc[i + 1], j, 0), vert(uc[i], j, 0)])
            if not solid(i - 1, j):
                b.faces.new([vert(uc[i], j, 0), vert(uc[i], j, 1), vert(uc[i], j + 1, 1), vert(uc[i], j + 1, 0)])
            if not solid(i + 1, j):
                b.faces.new([vert(uc[i + 1], j + 1, 0), vert(uc[i + 1], j + 1, 1), vert(uc[i + 1], j, 1), vert(uc[i + 1], j, 0)])
            if not solid(i, j - 1):
                # a face de baixo (o vão de cima / o pé): os vértices finos de fora e os 2 grossos de dentro
                b.faces.new([vert(uc[i + 1], j, 0)] + [vert(u, j, 1) for u in reversed(su)] + [vert(uc[i], j, 0)])
            if not solid(i, j + 1):
                b.faces.new([vert(uc[i], j + 1, 0)] + [vert(u, j + 1, 1) for u in su] + [vert(uc[i + 1], j + 1, 0)])


def walls_with_posts(p, x0, x1, y0, y1, z0, z1, t, holes, m='Plaster', seed=0, plinth=0.32, wear=1.0):
    """As 4 paredes ENTRE os pilares de canto (t × t, a altura toda): a ponta de cada parede encosta no pilar (a face escondida sai
    no corte do BPart) e a quina de fora é uma só, a do pilar, redonda. (As paredes que se cruzavam no canto abriam um sulco escuro:
    o chanfro de uma encontrava o da outra.) (v3) O pilar é o corner_post: a quina gasta no perfil."""
    at = {'+x': x1, '-x': x0, '+y': y1, '-y': y0}
    for f in ('+x', '-x', '+y', '-y'):
        F = WallFace(f, at[f], 0.0)
        u0, u1 = ((y0 + t - EPS, y1 - t + EPS) if f[1] == 'x' else (x0 + t - EPS, x1 - t + EPS))
        wall_grid(p, m, F, u0, u1, z0, z1, t, holes.get(f, ()), fine=FINE['u'])
    k = 0
    for (xa, xb, cx) in ((x0, x0 + t, x0), (x1 - t, x1, x1)):
        for (ya, yb, cy) in ((y0, y0 + t, y0), (y1 - t, y1, y1)):
            corner_post(p, xa, xb, ya, yb, (cx, cy), z0, z1, seed=seed * 5 + k, plinth=plinth, wear=wear)
            k += 1


def corner_post(p, xa, xb, ya, yb, corner, z0, z1, seed=0, plinth=0.32, wear=1.0):
    """(v3) O pilar de canto gasto: a planta t × t com a QUINA DE FORA redonda e larga (raio 9–14 cm, mudando com a altura pelo ruído
    e comida no pé, até ~22 cm, onde o adobe mais gasta), em anéis de ~35 cm (a forma no perfil, sem chanfro); o rodapé contorna a
    quina com o mesmo perfil 3 cm pra fora."""
    cx, cy = corner
    sx = 1.0 if abs(cx - max(xa, xb)) < 1e-6 else -1.0          # pra onde a quina de fora aponta
    sy = 1.0 if abs(cy - max(ya, yb)) < 1e-6 else -1.0
    ix = cx - sx * abs(xb - xa)
    iy = cy - sy * abs(yb - ya)
    T = abs(xb - xa)

    def ring(z, grow=0.0):
        r = 0.09 + 0.05 * cp.fbm3(z * 1.4, cx * 3.1, cy * 2.7, seed) + 0.09 * (1.0 - smooth01(z / 0.55)) * wear
        r = min(r + grow, T + grow - 0.03)
        ox, oy = cx + sx * grow, cy + sy * grow
        pts = [(ix, iy), (ox, iy)]
        for kk in range(7):
            a = math.radians(90.0 * kk / 6)
            # o arco da quina: do lado x (ox, oy − sy·r) até o lado y (ox − sx·r, oy)
            pts.append((ox - sx * r + sx * r * math.cos(a), oy - sy * r + sy * r * math.sin(a)))
        pts.append((ix, oy))
        if sx * sy < 0:
            pts = list(reversed(pts))
        return [(x, y, z) for (x, y) in pts]
    nzs = max(1, int((z1 - z0) / 0.35))
    zs = sorted(set([z0 + (z1 - z0) * k / nzs for k in range(nzs + 1)] + [z0 + 0.15, z0 + 0.4]))
    p.loft('Plaster#post', [ring(z) for z in zs], smooth=True)
    if plinth:
        zp = [z0 - 0.02, z0 + plinth * 0.5, z0 + plinth - 0.03 * h3(seed, 1, 5)]
        p.loft('Plaster#band', [ring(max(z, 0.0), grow=0.03) for z in zp], smooth=True)


def wall_patches(p, F, u0, u1, z0, z1, n, seed=0, holes=(), raw=0.0, mud=0.35):
    """A vida no reboco: os REMENDOS (o reboco novo 12 mm pra fora, o contorno torto de 6–9 pontas); mud da conta é o remendo de
    BARRO (o BrokenAdobe, mais escuro e quente: a outra cor que o dono passou). Fora dos vãos (holes, com 15 cm de folga). (v3: o
    bloco cru de antes virou o reboco caído de verdade, wall_spalls.)"""
    nrm = Vector(F.normal())
    eu = Vector(F.pt(1.0, 0.0, 0.0)) - Vector(F.pt(0.0, 0.0, 0.0))
    ev = Vector((0.0, 0.0, 1.0))
    k = 0
    tries = 0
    while k < n and tries < n * 12:
        tries += 1
        u = lerp(u0 + 0.5, u1 - 0.5, h3(tries, seed, 3))
        z = lerp(z0 + 0.45, z1 - 0.35, h3(tries, seed, 5))
        R = 0.22 + 0.38 * h3(tries, seed, 7)
        if any(a - R - 0.15 < u < b + R + 0.15 and c - R - 0.15 < z < d + R + 0.15 for (a, b, c, d) in holes):
            continue
        ns = 6 + int(3 * h3(tries, seed, 9))
        pts = []
        for j in range(ns):
            ang = 2 * math.pi * j / ns + 0.4 * h3(j, tries, seed)
            rr = R * (0.6 + 0.5 * h3(j, tries, seed + 1))
            pts.append((rr * math.cos(ang) * 1.3, rr * math.sin(ang) * 0.8))
        crude = h3(tries, seed, 11) < raw
        muddy = h3(tries, seed, 13) < mud
        o = Vector(F.pt(u, z, -0.004))
        dirt_wall(F)[2].append(('flat', u - R * 1.4, u + R * 1.4, z - R, z + R))
        m = 'BrokenAdobe#raw' if crude else ('BrokenAdobe#patch' if muddy else 'Plaster#patch')
        vp.prism(p, m, pts, o, eu, ev, nrm, 0.008 if crude else 0.016)
        k += 1


def _hull(pts):
    pts = sorted(set(pts))
    if len(pts) < 3:
        return pts

    def cr(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lo, hi = [], []
    for q in pts:
        while len(lo) >= 2 and cr(lo[-2], lo[-1], q) <= 0:
            lo.pop()
        lo.append(q)
    for q in reversed(pts):
        while len(hi) >= 2 and cr(hi[-2], hi[-1], q) <= 0:
            hi.pop()
        hi.append(q)
    return lo[:-1] + hi[:-1]


def _clip_convex(poly, clip):
    """Sutherland–Hodgman: o polígono poly recortado pelo convexo clip (anti-horário)."""
    out = list(poly)
    n = len(clip)
    for i in range(n):
        a, b = clip[i], clip[(i + 1) % n]
        inp, out = out, []
        if not inp:
            break

        def inside(q, a=a, b=b):
            return (b[0] - a[0]) * (q[1] - a[1]) - (b[1] - a[1]) * (q[0] - a[0]) >= 0

        def cut(p1, p2, a=a, b=b):
            x1, y1 = p1
            x2, y2 = p2
            dx, dy = x2 - x1, y2 - y1
            den = (b[0] - a[0]) * dy - (b[1] - a[1]) * dx
            if abs(den) < 1e-12:
                return p2
            tt = ((b[0] - a[0]) * (a[1] - y1) - (b[1] - a[1]) * (a[0] - x1)) / den
            return (x1 + dx * tt, y1 + dy * tt)
        s_ = inp[-1]
        for e in inp:
            if inside(e):
                if not inside(s_):
                    out.append(cut(s_, e))
                out.append(e)
            elif inside(s_):
                out.append(cut(s_, e))
            s_ = e
    return out


def spall(p, F, u, z, R, seed=0, rim=0.06):
    """(v3) O REBOCO CAÍDO em placa (o pedido: "o reboco caído em placas mostrando o tijolo de adobe por baixo, geometria rasa, em
    relevo"): o contorno torto e convexo; a BORDA do reboco que ficou (1,4 cm de espessura na quebra, morrendo pra fora em ~6 cm: a
    sombra fina que desenha a placa); dentro, as fiadas de TIJOLO de adobe (34 × 8,5 cm com a junta de 2 cm, desencontradas, cortadas
    pelo contorno), 6–10 mm de relevo, mais baixas que a borda: lê como o buraco no reboco. O tijolo vai no Cloth (o marrom de barro
    que o kit tem; o slot próprio está pedido no relatório)."""
    nrm = Vector(F.normal())
    eu = Vector(F.pt(1.0, 0.0, 0.0)) - Vector(F.pt(0.0, 0.0, 0.0))
    ev = Vector((0.0, 0.0, 1.0))
    O = Vector(F.pt(u, z, 0.0))
    dirt_wall(F)[2].append(('flat', u - R * 1.7, u + R * 1.7, z - R * 1.2, z + R * 1.2))
    ns = 9
    raw = []
    for j in range(ns):
        a = 2 * math.pi * (j + 0.35 * h3(j, seed, 3)) / ns
        rr = R * (0.72 + 0.4 * h3(j, seed, 5))
        raw.append((rr * math.cos(a) * 1.35, rr * math.sin(a) * 0.85))
    poly = _hull(raw)
    n_ = len(poly)
    if n_ < 3:
        return
    c = (sum(q[0] for q in poly) / n_, sum(q[1] for q in poly) / n_)
    outer = []
    for i, q in enumerate(poly):
        d = Vector((q[0] - c[0], q[1] - c[1], 0.0))
        L = max(d.length, 1e-6)
        w = rim * (0.6 + 0.8 * h3(i, seed, 9))
        outer.append((q[0] + d.x / L * w, q[1] + d.y / L * w))
    # a borda: a face de cima rampa de 1,4 cm (na quebra) a 0,2 cm (fora); a parede de dentro em pé (o eu × ev pode apontar pra
    # dentro: a ordem dos vértices sai do bmesh.recalc_face_normals no fim da peça)
    b = p.bm('Plaster#rim', 0.0)

    def W(uv, h):
        return V(*(O + eu * uv[0] + ev * uv[1] + nrm * h))
    vi_lo = [b.verts.new(W(q, 0.0)) for q in poly]
    vi_hi = [b.verts.new(W(q, 0.014)) for q in poly]
    vo_hi = [b.verts.new(W(q, 0.002)) for q in outer]
    vo_lo = [b.verts.new(W(q, 0.0)) for q in outer]
    for i in range(n_):
        j = (i + 1) % n_
        b.faces.new([vi_hi[i], vi_hi[j], vo_hi[j], vo_hi[i]])
        b.faces.new([vi_lo[i], vi_lo[j], vi_hi[j], vi_hi[i]])
        b.faces.new([vo_hi[i], vo_hi[j], vo_lo[j], vo_lo[i]])
    # os tijolos (no polígono encolhido 3%, anti-horário)
    inner = [(c[0] + (q[0] - c[0]) * 0.97, c[1] + (q[1] - c[1]) * 0.97) for q in poly]
    if sum((inner[i][0] * inner[(i + 1) % n_][1] - inner[(i + 1) % n_][0] * inner[i][1]) for i in range(n_)) < 0:
        inner = list(reversed(inner))
    us_ = [q[0] for q in inner]
    vs_ = [q[1] for q in inner]
    BH, BL, J = 0.085, 0.34, 0.02
    row = 0
    v = min(vs_) + J / 2
    while v < max(vs_):
        off = (BL + J) * 0.5 * (row % 2) + (BL + J) * h3(row, seed, 13) * 0.3
        uu = min(us_) - off
        while uu < max(us_):
            rect = [(uu, v), (uu + BL, v), (uu + BL, v + BH), (uu, v + BH)]
            cl = _clip_convex(rect, inner)
            if len(cl) >= 3:
                area = 0.5 * abs(sum(cl[i][0] * cl[(i + 1) % len(cl)][1] - cl[(i + 1) % len(cl)][0] * cl[i][1] for i in range(len(cl))))
                if area > 0.003:
                    dz = 0.004 * h3(int(uu * 31), row, seed + 17)
                    vp.prism(p, 'Cloth#brick', cl, O, eu, ev, nrm, 0.006 + dz)
            uu += BL + J
        v += BH + J
        row += 1


def wall_spalls(p, F, u0, u1, z0, z1, n, seed=0, holes=()):
    """(v3) As placas de reboco caído de uma parede: mais no pé (a umidade sobe e solta o reboco) e perto dos vãos, nunca em cima deles."""
    k, tries = 0, 0
    while k < n and tries < n * 14:
        tries += 1
        R = 0.22 + 0.26 * h3(tries, seed, 7)
        u = lerp(u0 + R * 1.4 + 0.1, u1 - R * 1.4 - 0.1, h3(tries, seed, 3))
        zz = h3(tries, seed, 5)
        z = lerp(z0 + R + 0.15, z1 - R - 0.25, zz * zz if h3(tries, seed, 9) < 0.6 else zz)
        if any(a - R * 1.4 - 0.12 < u < b + R * 1.4 + 0.12 and c - R - 0.12 < z < d + R + 0.12 for (a, b, c, d) in holes):
            continue
        spall(p, F, u, z, R, seed=seed * 31 + tries)
        k += 1


def roof_wear(p, x0, x1, y0, y1, z, seed=0, patches=4, cracks=5):
    """(v3) A laje vista de cima (a maior área da casa na câmera do RTS; o alvo tem a laje manchada e rachada): os REMENDOS de massa de
    barro (1 cm, o contorno torto: a mancha escura de 0,5–1,2 m) e as RACHADURAS da laje (o zigue-zague escuro de 2–3 cm)."""
    eu, ev, nrm = Vector((1.0, 0.0, 0.0)), Vector((0.0, 1.0, 0.0)), Vector((0.0, 0.0, 1.0))
    for k in range(patches):
        cx = lerp(x0 + 0.8, x1 - 0.8, h3(k, seed, 3))
        cy = lerp(y0 + 0.8, y1 - 0.8, h3(k, seed, 5))
        R = 0.3 + 0.35 * h3(k, seed, 7)
        ns = 7 + int(3 * h3(k, seed, 9))
        pts = [(R * (0.6 + 0.5 * h3(j, k, seed + 1)) * math.cos(2 * math.pi * j / ns) * 1.4,
                R * (0.6 + 0.5 * h3(j, k, seed + 2)) * math.sin(2 * math.pi * j / ns)) for j in range(ns)]
        vp.prism(p, 'BrokenAdobe#screed', pts, Vector((cx, cy, z - 0.002)), eu, ev, nrm, 0.011)
    for k in range(cracks):
        cx = lerp(x0 + 0.5, x1 - 0.5, h3(k, seed, 13))
        cy = lerp(y0 + 0.5, y1 - 0.5, h3(k, seed, 15))
        crack(p, None, 0.0, 0.0, seed=seed * 17 + k, L=0.9 + 1.2 * h3(k, seed, 17),
              frame=(Vector((cx, cy, z)), eu, ev, nrm, 360.0 * h3(k, seed, 19)))


def crack(p, F, u, z, up=True, seed=0, L=None, frame=None):
    """(v3) A FRESTA que sai da quina do vão (a parede de adobe racha em diagonal a partir da janela): o zigue-zague escuro de 2–3 cm
    de largura (1 px na câmera de perto), afinando na ponta, 1 mm pra fora."""
    if frame is None:
        nrm = Vector(F.normal())
        eu = Vector(F.pt(1.0, 0.0, 0.0)) - Vector(F.pt(0.0, 0.0, 0.0))
        ev = Vector((0.0, 0.0, 1.0))
        O = Vector(F.pt(u, z, 0.0))
    else:
        # o plano dado (a laje: a origem, os 2 eixos e a normal; girado 'ang' graus)
        O, e1, e2, nrm, ang = frame
        a_ = math.radians(ang)
        eu, ev = e1 * math.cos(a_) + e2 * math.sin(a_), e2 * math.cos(a_) - e1 * math.sin(a_)
    L = L or 0.6 + 0.6 * h3(seed, 1, 3)
    n = 7
    dir_u = 1.0 if h3(seed, 2, 3) < 0.5 else -1.0
    pts = []
    for k in range(n + 1):
        t_ = k / n
        pts.append((dir_u * L * 0.55 * t_ + 0.06 * (h3(k, seed, 5) - 0.5), (1.0 if up else -1.0) * L * t_ + 0.04 * (h3(k, seed, 7) - 0.5)))
    left, right = [], []
    for k in range(n + 1):
        a = pts[max(0, k - 1)]
        b_ = pts[min(n, k + 1)]
        d = Vector((b_[0] - a[0], b_[1] - a[1], 0.0)).normalized()
        w = 0.013 * (1.0 - 0.8 * k / n)
        left.append((pts[k][0] - d.y * w, pts[k][1] + d.x * w))
        right.append((pts[k][0] + d.y * w, pts[k][1] - d.x * w))
    poly = left + list(reversed(right))
    vp.prism(p, 'Dark#crack', poly, O, eu, ev, nrm, 0.0012 if frame is None else 0.002)


def roof_junk(p, x0, x1, y0, y1, z, n=6, seed=0):
    """A tralha do teto (o que dá escala de cima): os blocos de cimento soltos, a pilha de tijolos, o balde, o galão."""
    for k in range(n):
        x = lerp(x0, x1, h3(k, seed, 3))
        y = lerp(y0, y1, h3(k, seed, 5))
        kind = int(h3(k, seed, 7) * 5)
        yaw = 360 * h3(k, seed, 9)
        if kind == 4:
            # os caixotes de ripa empilhados (a pilha do teto do 16_beauty_pass): o corpo, as ripas e as travessas em relevo
            for j, (dx, dz, s_) in enumerate(((0.0, 0.0, 0.55), (0.6, 0.0, 0.5), (0.25, 0.42, 0.48))):
                c = (x + dx * math.cos(math.radians(yaw)), y + dx * math.sin(math.radians(yaw)), z + dz + s_ * 0.38)
                vp.rbox(p, 'Wood#crate', c, (s_, s_ * 0.75, s_ * 0.76), yaw=yaw + 6 * j, r=0.02, k=1)
                for dzz in (-0.12, 0.0, 0.12):
                    vp.rbox(p, 'Wood#slat', (c[0], c[1], c[2] + dzz * s_ / 0.5), (s_ + 0.012, s_ * 0.75 + 0.012, 0.035), yaw=yaw + 6 * j,
                            r=0.008, k=1)
            continue
        if kind == 0:
            for j in range(3):
                vp.rbox(p, 'Plaster#block', (x + 0.02 * j, y, z + 0.1 + 0.2 * j), (0.4, 0.2, 0.2), yaw=yaw + 9 * j, r=0.02, k=1)
        elif kind == 1:
            for j in range(4):
                for i in range(3):
                    vp.rbox(p, 'BrokenAdobe#brick', (x + 0.23 * (i - 1), y + 0.11 * (j % 2), z + 0.035 + 0.07 * (j // 2)),
                            (0.22, 0.1, 0.065), yaw=yaw, r=0.01, k=1)
        elif kind == 2:
            vp.lathe(p, 'Dark#bucket', (x, y, z), 'z', [(0.0, 0.0), (0.12, 0.0), (0.15, 0.3), (0.14, 0.3), (0.11, 0.02), (0.0, 0.02)],
                     seg=14)
        else:
            vp.jerrycan(p, (x, y, z), yaw=yaw, m='Crate#can', cap_m='Dark')


def erosion(u, u0, u1, seed, wear=1.0, bites=None, deep=0.22):
    """(v3) Quanto o topo da mureta foi comido em u: as MORDIDAS redondas (a chuva e o tempo comendo o adobe) e os BLOCOS FALTANDO
    (o degrau seco de 1–2 fiadas), espalhados pelo comprimento, mais a ondulação miúda de ±1,5 cm (a mureta nunca é reta)."""
    L = u1 - u0
    n = bites if bites is not None else max(1, int(round(L / 2.0 * wear)))
    e = 0.0
    for j in range(n):
        uc = u0 + L * (0.08 + 0.84 * h3(j, seed, 3))
        w = 0.14 + 0.38 * h3(j, seed, 5)
        dd = (0.05 + deep * h3(j, seed, 7) ** 1.3) * wear
        if h3(j, seed, 9) < 0.35:
            # o bloco que caiu: o degrau quase em pé (2 cm de transição)
            e = max(e, dd * (1.0 - smooth01((abs(u - uc) - w * 0.6) / 0.03)))
        else:
            e = max(e, dd * max(0.0, 1.0 - ((u - uc) / w) ** 2) ** 0.6)
    return e - 0.015 * (cp.fbm3(u * 2.3, seed * 0.37, 0.4, seed) - 0.5)


def mureta(p, F, u0, u1, zb, h, t, seed=0, wear=1.0, cap=True, cap_over=0.03, step=0.13, bites=None, deep=0.22):
    """(v3) A mureta de borda irregular (o parapeito do teto, o muro do quintal): o corpo em loft ao longo da face (um anel a cada ~13
    cm: o topo segue a erosão, a face de fora tem vértice pro escorrido) com a crista arredondada pelo chanfro; a CAPA por cima em
    PEDAÇOS de ~0,7–1,4 m (a junta, o pedaço torto, o que caiu: some onde a mordida é funda e mostra o miolo comido). A face de fora em
    n = 0 (o plano de F), o corpo pra dentro até n = −t. Devolve o topo de cada u (pra assentar os dentes)."""
    L = u1 - u0
    n = max(2, int(math.ceil(L / step)))
    us = [u0 + L * k / n for k in range(n + 1)]
    es = [max(0.0, erosion(u, u0, u1, seed, wear, bites, deep)) for u in us]
    top = [zb + h - e for e in es]
    rings = [[F.pt(u, zb, 0.0), F.pt(u, zt, 0.0), F.pt(u, zt, -t), F.pt(u, zb, -t)] for u, zt in zip(us, top)]
    p.loft('Plaster#par', rings, smooth=False)
    if not cap:
        return us, top
    # a capa em pedaços sobre o trecho inteiro (onde a erosão passa de 3 cm a capa caiu)
    k = 0
    while k < n:
        if es[k] > 0.03:
            k += 1
            continue
        j = k
        lim = k + max(4, int((0.7 + 0.7 * h3(k, seed, 21)) / step))
        while j < n and j < lim and es[j + 1] <= 0.03:
            j += 1
        if j > k:
            # o pedaço: 1 cm de junta em cada ponta, um pouco torto (±1 cm de altura e de lado)
            dz = 0.012 * (h3(k, seed, 23) - 0.5)
            dn = 0.012 * (h3(k, seed, 25) - 0.5)
            ua, ub = us[k] + 0.006, us[j] - 0.006
            cr = []
            for i in range(k, j + 1):
                u = min(max(us[i], ua), ub)
                z = top[i] + dz
                cr.append([F.pt(u, z - 0.004, cap_over + dn), F.pt(u, z + 0.075, cap_over + dn), F.pt(u, z + 0.075, -t - cap_over + dn),
                           F.pt(u, z - 0.004, -t - cap_over + dn)])
            p.loft('Plaster#cap', cr, smooth=False)
        k = j + 1
    return us, top


def parapet(p, x0, x1, y0, y1, z, h=0.62, t=0.22, teeth=True, tooth_step=1.05, piers=True, gaps=None, seed=0, rebar_at=(),
            teeth_skip=None, wear=1.0, corners=None, pier_h=0.30):
    """O parapeito do teto (v3: a mureta comida, mureta()): a mureta de t por dentro do contorno com a borda irregular (as mordidas,
    os blocos faltando), a CAPA em pedaços que sai 3 cm dos dois lados, os DENTES (os blocos em cima, o ritmo que lê de cima; uns
    faltando, uns quebrados e tortos), os pilares de quina mais altos e GASTOS (a quina redonda, a altura de cada um), o ferro de espera
    saindo em rebar_at = [(x, y)...]. gaps = {face: [(u0, u1)...]}: as aberturas (a chegada da escada de fora); corners: as quinas que
    têm pilar (todas, sem a lista)."""
    gaps = gaps or {}
    # as corridas: as de x (faces ±y) cobrem as quinas; as de y (faces ±x) ficam entre elas
    segs = {'-y': ((x0, x1), y0), '+y': ((x0, x1), y1), '-x': ((y0 + t, y1 - t), x0), '+x': ((y0 + t, y1 - t), x1)}
    for fi, (f, ((a, b), fixed)) in enumerate(segs.items()):
        runs = [(a, b)]
        for (g0, g1) in gaps.get(f, ()):
            new = []
            for (r0, r1) in runs:
                if g1 <= r0 or g0 >= r1:
                    new.append((r0, r1))
                    continue
                if g0 > r0 + 0.1:
                    new.append((r0, g0))
                if g1 < r1 - 0.1:
                    new.append((g1, r1))
            runs = new
        F = WallFace(f, fixed, 0.0)
        dirt_wall(F, z + h + 0.075)
        for ri, (r0, r1) in enumerate(runs):
            sd = seed * 13 + fi * 5 + ri
            us, top = mureta(p, F, r0, r1, z - 0.02, h + 0.02, t, seed=sd, wear=wear)
            if not teeth:
                continue
            L = r1 - r0
            nt_ = max(1, int(L / tooth_step))
            for k in range(nt_):
                u = r0 + (k + 0.5 + 0.25 * (h3(k, sd, 31) - 0.5)) * L / nt_
                if piers and (abs(u - a) < 0.6 or abs(u - b) < 0.6):
                    continue
                if any(s0 - 0.25 < u < s1 + 0.25 for (s0, s1) in (teeth_skip or {}).get(f, ())):
                    continue
                i = min(len(us) - 1, max(0, int(round((u - r0) / L * (len(us) - 1)))))
                zt = top[i]
                if z + h - zt > 0.03 or h3(k, sd, 33) < 0.16 * wear:
                    continue                                    # o dente caiu (onde a mureta foi comida, e uns ao acaso)
                w = 0.30 + 0.12 * h3(k, sd, 35)
                hh = 0.14 + 0.12 * h3(k, sd, 37)
                broken = h3(k, sd, 39) < 0.22 * wear
                if broken:
                    hh *= 0.55
                tilt = (h3(k, sd, 41) - 0.5) * (9.0 if broken else 3.0)
                c = Vector(F.pt(u, zt + 0.075 + hh / 2, -t / 2 + 0.008 * (h3(k, sd, 43) - 0.5)))
                yaw = F.yaw() + 90.0 + (h3(k, sd, 45) - 0.5) * 5.0
                vp.rbox(p, 'Plaster#tooth', tuple(c), (w, t + 0.02, hh), yaw=yaw, roll=tilt, r=0.03, k=2)
    if piers:
        for ci, (x, y) in enumerate(((x0, y0), (x1, y0), (x0, y1), (x1, y1))):
            if corners is not None and ci not in corners:
                continue
            sx = 1 if x == x0 else -1
            sy = 1 if y == y0 else -1
            ph = pier_h + 0.14 * (h3(ci, seed, 51) - 0.4)
            c = (x + sx * 0.19, y + sy * 0.19, z - 0.02 + (h + ph) / 2)
            vp.rbox(p, 'Plaster#pier', c, (0.46, 0.46, h + ph), r=0.11, k=4)
            tilt = (h3(ci, seed, 53) - 0.5) * 3.0
            vp.rbox(p, 'Plaster#cap', (c[0], c[1], z - 0.02 + h + ph + 0.04), (0.52, 0.52, 0.08), roll=tilt, r=0.12, k=4)
    for (x, y) in rebar_at:
        rebar(p, x, y, z + h + pier_h + 0.08, seed=seed + int(x * 7 + y * 3))


def rebar(p, x, y, z, n=4, h=0.75, seed=0, m='Rust#rebar'):
    """O ferro de espera (a obra que parou: o 2º andar que nunca veio), as 4 barras tortas saindo do pilar."""
    for k in range(n):
        a = 2 * math.pi * k / n + 0.6
        bx, by = x + 0.12 * math.cos(a), y + 0.12 * math.sin(a)
        lean = 0.12 + 0.15 * h3(k, seed, 3)
        ang = a + (h3(k, seed, 5) - 0.5) * 1.2
        hh = h * (0.7 + 0.5 * h3(k, seed, 7))
        pts = [(bx, by, z - 0.05), (bx, by, z + hh * 0.5), (bx + lean * math.cos(ang) * 0.5, by + lean * math.sin(ang) * 0.5, z + hh * 0.8),
               (bx + lean * math.cos(ang), by + lean * math.sin(ang), z + hh * 0.95)]
        vp.tube(p, m, vp.bent_bar(pts, R=0.08, n=3), 0.011, seg=5)


# ===================================================================================================== vãos
def wquad(p, m, F, quad, n0, n1, bevel=None):
    """(v3) A peça de 4 cantos (u, z) no plano da parede (o torto: a verga caída de um lado, o batente fora de esquadro), de n0 a n1."""
    # a ordem do hexa (índice = z*4 + n*2 + u): os de baixo (quad[0], quad[1]) e os de cima (quad[3], quad[2])
    P = [F.pt(quad[0][0], quad[0][1], n0), F.pt(quad[1][0], quad[1][1], n0), F.pt(quad[0][0], quad[0][1], n1), F.pt(quad[1][0], quad[1][1], n1),
         F.pt(quad[3][0], quad[3][1], n0), F.pt(quad[2][0], quad[2][1], n0), F.pt(quad[3][0], quad[3][1], n1), F.pt(quad[2][0], quad[2][1], n1)]
    hexa(p, m, P, bevel)


def window(p, F, u0, u1, z0, z1, t=0.25, kind='glass', seed=0, shutters=False, wear=1.0, lintel=None):
    """A janela recuada na espessura da parede: a MOLDURA de reboco 3 cm pra fora em volta, o peitoril com a pingadeira, o caixilho
    no meio da parede (madeira ou ferro), o vidro escuro (Glass) com o montante, e conforme kind: 'grille' (a grade de ferro de
    barras, a de baixo), 'glass' (só o vidro com a cruz) ou 'dark' (o vão aberto, escuro: sem vidro). shutters: as venezianas de
    madeira abertas na face.
    (v3, "parece lego, sem detalhes") a janela de casa velha: a moldura LASCADA (o pedaço que caiu do topo, o lado que não chega no
    peitoril) ou, com lintel='wood', a VERGA DE MADEIRA torta no lugar dela (a do adobe); o peitoril com a ponta quebrada; o caixilho de
    madeira GASTO e fora de esquadro; um vidro QUEBRADO (o escuro de dentro) em ~1/4 dos panos; a grade com 1–2 barras ENTORTADAS pra
    fora, uma faltando e a ferrugem; a veneziana CAÍDA (presa numa dobradiça só) ou que falta. O escorrido debaixo do peitoril é da
    cor de vértice (registrado aqui pra sujeira)."""
    dirt_wall(F)[2].append(('win', u0, u1, z0 - 0.07))
    d = t * 0.55
    fw = 0.055
    w = 0.09
    j = lambda k: 0.009 * (h3(k, seed, 61) - 0.5) * 2 * wear          # o fora de esquadro (±9 mm)
    if lintel == 'wood':
        dz = 0.016 * (h3(seed, 1, 3) - 0.5) * 2 * wear
        wquad(p, 'Wood#lintel', F, [(u0 - 0.2, z1 + 0.02 + dz), (u1 + 0.2, z1 + 0.02 - dz), (u1 + 0.2, z1 + 0.17 - dz),
                                    (u0 - 0.2, z1 + 0.17 + dz)], -0.03, 0.035)
    else:
        # o topo da moldura: inteiro ou com a lasca que caiu (o miolo cru aparece pela cor da parede)
        if h3(seed, 2, 3) < 0.55 * wear:
            gu = lerp(u0 + 0.1, u1 - 0.1, h3(seed, 2, 5))
            gw = 0.08 + 0.12 * h3(seed, 2, 7)
            wbox(p, 'Plaster#trim', F, u0 - w, gu - gw / 2, z1, z1 + w, -0.02, 0.03)
            wbox(p, 'Plaster#trim', F, gu + gw / 2, u1 + w, z1, z1 + w * (0.7 + 0.3 * h3(seed, 2, 9)), -0.02, 0.03)
        else:
            wbox(p, 'Plaster#trim', F, u0 - w, u1 + w, z1, z1 + w, -0.02, 0.03)
        short = 0.04 + 0.12 * h3(seed, 3, 3) if h3(seed, 3, 5) < 0.5 * wear else 0.0
        wbox(p, 'Plaster#trim', F, u0 - w, u0, z0 + short, z1, -0.02, 0.03)
        wbox(p, 'Plaster#trim', F, u1, u1 + w, z0, z1, -0.02, 0.03)
    # o peitoril com a pingadeira; a ponta quebrada (um pedaço mais baixo e mais curto)
    chip = (0.06 + 0.1 * h3(seed, 4, 3)) if h3(seed, 4, 5) < 0.6 * wear else 0.0
    wbox(p, 'Plaster#trim', F, u0 - w - 0.04, u1 + w + 0.04 - chip, z0 - 0.07, z0 + 0.005, -0.03, 0.07)
    if chip:
        wbox(p, 'Plaster#trim', F, u1 + w + 0.04 - chip + 0.01, u1 + w + 0.02, z0 - 0.07, z0 - 0.03, -0.03, 0.045)
    # o caixilho no fundo (fora de esquadro: cada canto do quadro mexe uns mm)
    fm = 'Wood#frame' if kind != 'grille' else 'Metal#frame'
    wquad(p, fm, F, [(u0, z0 + j(1)), (u1, z0 + j(2)), (u1, z0 + fw + j(2)), (u0, z0 + fw + j(1))], -d - 0.04, -d + 0.03)
    wquad(p, fm, F, [(u0, z1 - fw + j(3)), (u1, z1 - fw + j(4)), (u1, z1 + j(4)), (u0, z1 + j(3))], -d - 0.04, -d + 0.03)
    wquad(p, fm, F, [(u0 + j(5), z0), (u0 + fw + j(5), z0), (u0 + fw + j(6), z1), (u0 + j(6), z1)], -d - 0.04, -d + 0.03)
    wquad(p, fm, F, [(u1 - fw + j(7), z0), (u1 + j(7), z0), (u1 + j(8), z1), (u1 - fw + j(8), z1)], -d - 0.04, -d + 0.03)
    if kind == 'dark':
        wbox(p, 'Dark', F, u0 + 0.01, u1 - 0.01, z0 + 0.01, z1 - 0.01, -t - 0.02, -t + 0.01)
    else:
        um = (u0 + u1) / 2
        zm = z0 + (z1 - z0) * 0.62 if z1 - z0 > 1.0 else None
        # os panos (2 ou 4): o vidro ou o QUEBRADO (o escuro do quarto, no fundo da parede)
        cols = [(u0 + 0.02, um), (um, u1 - 0.02)]
        rows = [(z0 + 0.02, zm), (zm, z1 - 0.02)] if zm else [(z0 + 0.02, z1 - 0.02)]
        for ci, (ua, ub) in enumerate(cols):
            for ri, (za, zb) in enumerate(rows):
                if h3(ci * 3 + ri, seed, 71) < 0.24 * wear:
                    wbox(p, 'Dark', F, ua, ub, za, zb, -t - 0.02, -t + 0.01)
                    # o caco que sobrou no canto do pano
                    wquad(p, 'Glass', F, [(ua, za), (ua + (ub - ua) * 0.45, za), (ua + (ub - ua) * 0.1, za + (zb - za) * 0.2),
                                          (ua, za + (zb - za) * 0.35)], -d - 0.025, -d - 0.012)
                else:
                    wbox(p, 'Glass', F, ua, ub, za, zb, -d - 0.025, -d - 0.012)
        wquad(p, fm, F, [(um - 0.025 + j(9), z0 + fw), (um + 0.025 + j(9), z0 + fw), (um + 0.025 + j(10), z1 - fw),
                         (um - 0.025 + j(10), z1 - fw)], -d - 0.03, -d + 0.015)
        if zm:
            wquad(p, fm, F, [(u0 + fw, zm - 0.022 + j(11)), (u1 - fw, zm - 0.022 + j(12)), (u1 - fw, zm + 0.022 + j(12)),
                             (u0 + fw, zm + 0.022 + j(11))], -d - 0.03, -d + 0.015)
    if kind == 'grille':
        # a grade de ferro: as barras de pé (4–5 cm, lê como ritmo), as 2 travessas; 1–2 barras entortadas pra fora (o que forçaram), uma
        # faltando, a ferrugem
        bm_ = 'Rust#bar' if h3(seed, 5, 3) < 0.6 * wear else 'Metal#bar'
        n = max(3, int((u1 - u0) / 0.13))
        miss = int(1 + (n - 2) * h3(seed, 5, 5)) if h3(seed, 5, 7) < 0.5 * wear else -1
        bent = {int(1 + (n - 2) * h3(seed, 5, 9)), int(1 + (n - 2) * h3(seed, 5, 11))} if wear > 0.3 else set()
        for k in range(1, n):
            if k == miss:
                continue
            u = u0 + (u1 - u0) * k / n
            if k in bent:
                zb_ = lerp(z0 + 0.3, z1 - 0.3, h3(k, seed, 13))
                out = 0.03 + 0.06 * h3(k, seed, 15)
                pts = vp.bent_bar([F.pt(u, z0 + 0.01, -0.05), F.pt(u + 0.03 * (h3(k, seed, 17) - 0.5), zb_, -0.05 + out),
                                   F.pt(u, z1 - 0.01, -0.05)], R=0.25, n=4)
                vp.tube(p, bm_, pts, 0.016, seg=6)
            else:
                vp.tube(p, bm_, [F.pt(u, z0 + 0.01, -0.05), F.pt(u, z1 - 0.01, -0.05)], 0.016, seg=6)
        tl = 0.02 * (h3(seed, 6, 3) - 0.5) * wear
        for zz in (z0 + 0.12, z1 - 0.12):
            wquad(p, bm_, F, [(u0 + 0.01, zz - 0.016 + tl), (u1 - 0.01, zz - 0.016 - tl), (u1 - 0.01, zz + 0.016 - tl),
                              (u0 + 0.01, zz + 0.016 + tl)], -0.065, -0.035)
    if shutters:
        state = h3(seed, 7, 3)
        for s in (-1, 1):
            ua = u0 - w - 0.02 if s < 0 else u1 + w + 0.02
            ub = ua + s * (u1 - u0) * 0.5
            lo, hi = min(ua, ub), max(ua, ub)
            if s > 0 and state < 0.3 * wear:
                continue                                         # a veneziana que falta (só as dobradiças)
            if s < 0 and state > 1.0 - 0.45 * wear:
                # a veneziana caída: presa só na dobradiça de cima, girada 10–16° pra fora do vão
                ang = math.radians(10 + 6 * h3(seed, 7, 5))
                pu, pz = (hi, z1 - 0.02)
                def rot(u, z, pu=pu, pz=pz, ang=ang):
                    du, dz_ = u - pu, z - pz
                    return (pu + du * math.cos(ang) - dz_ * math.sin(ang), pz + du * math.sin(ang) + dz_ * math.cos(ang))
                q = [rot(lo, z0 + 0.02), rot(hi, z0 + 0.02), rot(hi, z1 - 0.02), rot(lo, z1 - 0.02)]
                wquad(p, 'Wood#shut', F, q, 0.03, 0.065)
                continue
            wbox(p, 'Wood#shut', F, lo, hi, z0 + 0.02, z1 - 0.02, 0.03, 0.065)
            for k in range(1, 6):
                zz = z0 + (z1 - z0) * k / 6
                wbox(p, 'Dark', F, lo + 0.03, hi - 0.03, zz - 0.008, zz + 0.008, 0.065, 0.07)


def steel_door(p, F, u0, u1, z0, z1, t=0.25, m='Metal#door', canopy=True, step=True, double=False, seed=0):
    """A porta de ferro recuada (a mais vista nas casas da região): o batente, a folha com as almofadas em relevo, a maçaneta, a
    marquise de laje por cima (a sombra que marca a entrada de cima) e o degrau."""
    d = t * 0.5
    wbox(p, 'Dark', F, u0 + 0.01, u1 - 0.01, z0, z1 - 0.01, -t - 0.02, -t + 0.01)
    wbox(p, 'Metal#frame', F, u0, u0 + 0.06, z0, z1, -d - 0.04, -d + 0.04)
    wbox(p, 'Metal#frame', F, u1 - 0.06, u1, z0, z1, -d - 0.04, -d + 0.04)
    wbox(p, 'Metal#frame', F, u0, u1, z1 - 0.06, z1, -d - 0.04, -d + 0.04)
    leaves = [(u0 + 0.06, (u0 + u1) / 2 - 0.004), ((u0 + u1) / 2 + 0.004, u1 - 0.06)] if double else [(u0 + 0.06, u1 - 0.06)]
    for (a, b) in leaves:
        wbox(p, m, F, a, b, z0 + 0.02, z1 - 0.06, -d - 0.03, -d)
        # as almofadas (2 de pé em cima, 1 larga embaixo) e o friso
        for (za, zb) in ((z0 + 0.12, z0 + 0.85), (z0 + 0.98, z1 - 0.18)):
            wbox(p, m, F, a + 0.08, b - 0.08, za, zb, -d, -d + 0.018)
        wbox(p, 'Dark', F, a + 0.1, b - 0.1, z0 + 0.9, z0 + 0.93, -d, -d + 0.006)
    hu = u1 - 0.18 if not double else (u0 + u1) / 2 + 0.1
    wbox(p, 'Dark#handle', F, hu - 0.02, hu + 0.02, z0 + 0.95, z0 + 1.08, -d + 0.01, -d + 0.05)
    if canopy:
        # a marquise: a laje fina saindo da parede, com a pingadeira (a sombra dela marca a porta de cima)
        wbox(p, 'Plaster#cap', F, u0 - 0.35, u1 + 0.35, z1 + 0.18, z1 + 0.30, -0.05, 0.62)
    if step:
        wbox(p, 'Plaster#step', F, u0 - 0.15, u1 + 0.15, -0.05, 0.16, -0.02, 0.38)


def rollup(p, F, u0, u1, z0, z1, t=0.25, open_frac=0.0):
    """A porta de enrolar da loja, recuada: as guias, a caixa do rolo e a cortina com o ritmo das frestas (ou o vão aberto escuro)."""
    d = t * 0.45
    zt = z1 - (z1 - z0) * open_frac
    wbox(p, 'Metal#frame', F, u0, u0 + 0.1, z0, z1, -d - 0.06, -d + 0.05)
    wbox(p, 'Metal#frame', F, u1 - 0.1, u1, z0, z1, -d - 0.06, -d + 0.05)
    wbox(p, 'Metal#roll', F, u0, u1, z1 - 0.3, z1, -d - 0.1, -d + 0.07)
    wbox(p, 'Dark', F, u0 + 0.05, u1 - 0.05, z0, zt, -t - 0.02, -t + 0.01)
    if open_frac < 0.98:
        wbox(p, 'Metal#shutter', F, u0 + 0.08, u1 - 0.08, zt, z1 - 0.28, -d - 0.04, -d)
        z = z1 - 0.28
        while z > zt + 0.12:
            z -= 0.11
            wbox(p, 'Dark', F, u0 + 0.08, u1 - 0.08, z - 0.01, z + 0.01, -d, -d + 0.008)
        wbox(p, 'Metal#frame', F, u0 + 0.08, u1 - 0.08, zt, zt + 0.06, -d - 0.04, -d + 0.02)


# ===================================================================================================== o teto
def water_tank(p, c, r=0.55, h=1.15, stand=0.0, m='Dark#tank', stand_m='Metal#stand', ribs=6, seed=0, base=True):
    """A caixa d'água de polietileno (a "Rotoplas", o cilindro escuro mais visto nos tetos da região): o corpo com os FRISOS (os anéis
    em relevo que pegam a luz), o ombro redondo, a cúpula e a tampa de rosca, a saída e a boia; no cavalete de cantoneira com as
    diagonais (stand = a altura) ou direto na laje, numa base de blocos."""
    x, y, z = c
    zb = z + stand
    if stand:
        L = r + 0.08
        for (sx, sy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            vp.rbox(p, stand_m, (x + sx * L, y + sy * L, z + stand / 2), (0.05, 0.05, stand), r=0.008, k=1)
        for (a, b) in (((-1, -1), (1, -1)), ((1, -1), (1, 1)), ((1, 1), (-1, 1)), ((-1, 1), (-1, -1))):
            pa = Vector((x + a[0] * L, y + a[1] * L, z + stand))
            pb = Vector((x + b[0] * L, y + b[1] * L, z + stand))
            vp.tube(p, stand_m, [pa, pb], 0.025, seg=4)
            vp.tube(p, stand_m, [Vector((pa.x, pa.y, z + 0.1)), Vector((pb.x, pb.y, z + stand - 0.05))], 0.016, seg=4)
        vp.rbox(p, stand_m, (x, y, zb - 0.03), (2 * L, 2 * L - 0.1, 0.05), r=0.01, k=1)
        for (sx, sy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            vp.rbox(p, 'Plaster#step', (x + sx * L, y + sy * L, z + 0.04), (0.18, 0.18, 0.1), r=0.02, k=1)
    elif base:
        vp.rbox(p, 'Plaster#step', (x, y, z + 0.06), (2 * r + 0.1, 2 * r - 0.15, 0.12), r=0.04, k=2)
        zb = z + 0.12
    prof = [(0.0, 0.0), (r * 0.92, 0.0), (r, 0.03)]
    for k in range(ribs):
        zz = 0.08 + (h - 0.3) * (k + 0.5) / ribs
        prof += [(r, zz - 0.05), (r + 0.022, zz - 0.03), (r + 0.022, zz + 0.03), (r, zz + 0.05)]
    prof += [(r, h - 0.2), (r * 0.96, h - 0.1), (r * 0.82, h - 0.03), (r * 0.5, h + 0.02), (0.22, h + 0.04), (0.0, h + 0.045)]
    vp.lathe(p, m, (x, y, zb), 'z', prof, seg=28, smooth=True, hard=40.0)
    vp.lathe(p, m + 'lid', (x, y, zb + h + 0.04), 'z', [(0.0, 0.0), (0.2, 0.0), (0.21, 0.04), (0.19, 0.07), (0.0, 0.075)], seg=20)
    # a saída de baixo e o cano que desce
    vp.tube(p, 'Metal#pipe', vp.bent_bar([(x + r, y, zb + 0.12), (x + r + 0.15, y, zb + 0.12), (x + r + 0.15, y, z + 0.02)], R=0.06, n=3),
            0.025, seg=8)


def tank_horizontal(p, c, r=0.42, L=1.6, yaw=0.0, stand=0.55, m='Metal#tankh', stand_m='Rust#stand'):
    """A caixa d'água de aço deitada no cavalete (a outra que lê de cima: o cilindro claro deitado): o corpo com as tampas abauladas
    e as cintas, o bocal de cima, os 2 berços e as pernas de cantoneira."""
    x, y, z = c
    ex, ey, ez = vp.ypr_frame(yaw)
    C = Vector((x, y, z + stand + r + 0.03))
    prof = [(0.0, -L / 2 - 0.08), (r * 0.6, -L / 2 - 0.06), (r * 0.92, -L / 2 - 0.02), (r, -L / 2 + 0.02), (r, L / 2 - 0.02),
            (r * 0.92, L / 2 + 0.02), (r * 0.6, L / 2 + 0.06), (0.0, L / 2 + 0.08)]
    vp.lathe(p, m, tuple(C), tuple(ex), prof, seg=28, smooth=True)
    for u in (-0.3, 0.3):
        vp.lathe(p, 'Dark#band', tuple(C + ex * (u * L)), tuple(ex), [(r + 0.002, -0.03), (r + 0.014, 0.0), (r + 0.002, 0.03)],
                 seg=28, closed=True)
    vp.lathe(p, m + 'neck', tuple(C + ez * r), tuple(ez), [(0.0, -0.02), (0.11, -0.02), (0.11, 0.08), (0.13, 0.1), (0.0, 0.11)], seg=16)
    for u in (-0.32, 0.32):
        q = C + ex * (u * L)
        vp.rbox(p, stand_m, tuple(Vector((q.x, q.y, z + stand + 0.06))), (0.12, 2 * r, 0.12), yaw=yaw, r=0.01, k=1)
        for s in (-1, 1):
            leg = q + ey * (s * (r - 0.05))
            vp.rbox(p, stand_m, (leg.x, leg.y, z + stand / 2 + 0.03), (0.05, 0.05, stand + 0.06), yaw=yaw, r=0.008, k=1)


def sat_dish(p, c, r=0.42, yaw=0.0, tilt=32.0, mount=0.55, m='Metal#dish'):
    """A antena parabólica (o disco virado pro céu, um dos pontos claros que mais leem de cima): o prato raso (o perfil girado com a
    borda), o braço com o conversor na ponta, o mastro e o pé chumbado."""
    x, y, z = c
    vp.rbox(p, 'Metal#stand', (x, y, z + 0.02), (0.3, 0.3, 0.04), r=0.02, k=1)
    vp.tube(p, 'Metal#stand', [(x, y, z), (x, y, z + mount)], 0.03, seg=8)
    ex, ey, ez = vp.ypr_frame(yaw, tilt)
    C = Vector((x, y, z + mount + 0.06)) + ex * 0.08
    depth = r * 0.22
    prof = [(0.0, 0.0), (r * 0.3, depth * 0.09), (r * 0.6, depth * 0.36), (r * 0.85, depth * 0.72), (r, depth), (r + 0.012, depth - 0.006),
            (r * 0.98, depth - 0.02), (r * 0.6, depth * 0.36 - 0.02), (0.0, -0.02)]
    vp.lathe(p, m, tuple(C), tuple(ex), prof, seg=26, smooth=True)
    F = C + ex * (r * 0.95)
    vp.tube(p, 'Metal#stand', [C + ex * 0.02 - ez * r * 0.85, F - ez * 0.02], 0.012, seg=5)
    vp.rbox(p, 'Dark#lnb', tuple(F), (0.12, 0.06, 0.06), yaw=yaw, pitch=tilt, r=0.02, k=1)


def roof_ac(p, c, yaw=0.0, m='Metal#ac'):
    """A condensadora no teto (a caixa clara com o ventilador em cima: o disco escuro com a grade lê de cima)."""
    x, y, z = c
    ex, ey, ez = vp.ypr_frame(yaw)
    vp.rbox(p, 'Dark', (x, y, z + 0.05), (0.85, 0.55, 0.1), yaw=yaw, r=0.02, k=1)
    vp.rbox(p, m, (x, y, z + 0.36), (0.80, 0.50, 0.55), yaw=yaw, r=0.04, k=2)
    for k in range(5):
        q = Vector((x, y, z + 0.18 + k * 0.09)) - ey * 0.252
        vp.rbox(p, 'Dark', tuple(q), (0.66, 0.008, 0.035), yaw=yaw, r=0.008, k=1)
    q = Vector((x, y, z + 0.635))
    vp.lathe(p, 'Dark', tuple(q), 'z', [(0.0, 0.0), (0.2, 0.0), (0.2, 0.012), (0.0, 0.012)], seg=20)
    p.torus('Metal#acring', tuple(q + Vector((0, 0, 0.016))), 'z', 0.2, 0.012, seg=20, rseg=5)
    for a in (0, 45, 90, 135):
        vp.rbox(p, 'Metal#acring', tuple(q + Vector((0, 0, 0.018))), (0.4, 0.014, 0.012), yaw=yaw + a, r=0.004, k=1)


def laundry(p, a, b, z, seed=0, n=None, posts=True):
    """O varal do teto: os 2 postes de cano, o fio com a barriga e as roupas penduradas (o pano fino que cai, cada uma de um tamanho,
    com a dobra), o que dá vida ao teto de cima."""
    A, B = Vector((a[0], a[1], 0.0)), Vector((b[0], b[1], 0.0))
    if posts:
        for q in (A, B):
            vp.tube(p, 'Metal#pole', [Vector((q.x, q.y, z)), Vector((q.x, q.y, z + 1.65))], 0.022, seg=6)
            vp.rbox(p, 'Plaster#step', (q.x, q.y, z + 0.05), (0.16, 0.16, 0.1), r=0.02, k=1)
    zt = z + 1.6
    L = (B - A).length
    d = (B - A).normalized()
    nrm = Vector((-d.y, d.x, 0.0))
    pts = []
    for k in range(13):
        u = k / 12
        q = A + (B - A) * u
        pts.append(Vector((q.x, q.y, zt - 0.12 * 4 * u * (1 - u))))
    vp.tube(p, 'Dark#line', pts, 0.006, seg=4)
    n = n or max(2, int(L / 0.55))
    mats = ('Cloth#wash', 'Metal#wash', 'Cloth#wash2', 'Rust#wash', 'Cloth#wash')
    u = 0.08
    for k in range(n):
        w = 0.35 + 0.3 * h3(k, seed, 3)
        if u + w / L > 0.95:
            break
        hh = 0.45 + 0.4 * h3(k, seed, 5)
        q = A + (B - A) * (u + w / L / 2)
        zz = zt - 0.12 * 4 * (u + w / L / 2) * (1 - u - w / L / 2) - 0.01
        m = mats[int(h3(k, seed, 7) * len(mats)) % len(mats)]
        vp.tarp_drape(p, tuple(Vector((q.x, q.y, zz)) - d * (w / 2)), tuple(Vector((q.x, q.y, zz)) + d * (w / 2)), hh, tuple(nrm),
                      m=m.split('#')[0] + '#' + m.split('#')[1], folds=2, amp=0.012, thick=0.008, seed=seed + k, nu=8, nv=4, hem=False,
                      ties=())
        u += w / L + 0.06


def rug_over(p, a, b, z_top, out_n, drop=1.3, depth=0.32, m='Rust#rug', seed=0):
    """O tapete jogado no parapeito (o ponto de cor mais forte dos tetos da região): a parte deitada em cima da capa e a parte que cai
    por fora com as dobras, a barra mais pesada."""
    A, B = Vector(a), Vector(b)
    n = Vector(out_n).normalized()
    # a parte deitada sobre a capa (do lado de dentro até a borda de fora)
    d = (B - A).normalized()
    c0, c1 = A - n * depth, B - n * depth
    rings = []
    for k in range(9):
        u = k / 8
        q0 = c0 + (c1 - c0) * u
        q1 = A + (B - A) * u
        rings.append([(q0.x, q0.y, z_top + 0.01), (q1.x, q1.y, z_top + 0.01), (q1.x, q1.y, z_top + 0.022), (q0.x, q0.y, z_top + 0.022)])
    p.loft(m + 'top', rings, bevel=0.0, smooth=True)
    vp.tarp_drape(p, tuple(A + n * 0.01 + Vector((0, 0, z_top + 0.01 - A.z))), tuple(B + n * 0.01 + Vector((0, 0, z_top + 0.01 - B.z))),
                  drop, tuple(n), m=m.split('#')[0], folds=3, amp=0.016, thick=0.012, seed=seed, nu=14, nv=6, hem=True, ties=())
    # a franja de baixo (a borda desfiada, o ritmo de fios)
    L = (B - A).length
    for k in range(int(L / 0.06)):
        q = A + d * (0.03 + k * 0.06) + n * 0.03
        vp.tube(p, 'Cloth#fringe', [Vector((q.x, q.y, z_top - drop - 0.01)), Vector((q.x, q.y, z_top - drop - 0.07))], 0.006, seg=3)


def spout(p, F, u, z, L=0.42, m='Metal#spout'):
    """A bica do teto (o cano curto saindo do pé do parapeito, a mancha escura do escorrido embaixo dela: v3, na cor de vértice)."""
    dirt_wall(F)[2].append(('spout', u, z))
    a = Vector(F.pt(u, z, -0.05))
    b = Vector(F.pt(u, z - 0.04, L))
    vp.tube(p, m, [a, b], 0.045, seg=10)
    nrm = Vector(F.normal())
    vp.lathe(p, 'Dark', tuple(b + nrm * 0.005), tuple(nrm), [(0.0, 0.0), (0.035, 0.0), (0.035, 0.004), (0.0, 0.004)], seg=10)


def antenna(p, x, y, z, h=2.2, seed=0):
    """A antena de TV de mastro (as varetas em espinha de peixe: a linha fina que desenha o teto contra o chão)."""
    vp.tube(p, 'Metal#ant', [(x, y, z), (x, y, z + h)], 0.018, seg=6)
    vp.rbox(p, 'Plaster#step', (x, y, z + 0.05), (0.14, 0.14, 0.1), r=0.02, k=1)
    for k in range(5):
        zz = z + h - 0.12 - k * 0.13
        w = 0.65 - k * 0.08
        vp.tube(p, 'Metal#ant', [(x - 0.02, y - w / 2, zz), (x + 0.02, y + w / 2, zz)], 0.007, seg=4)
    vp.tube(p, 'Metal#ant', [(x, y, z + h - 0.12), (x + 0.75, y, z + h - 0.12)], 0.01, seg=4)
    for (dx, dy) in ((0.9, 0.9), (-0.9, 0.9), (0.0, -1.1)):
        vp.tube(p, 'Dark#line', [(x, y, z + h * 0.7), (x + dx, y + dy, z + 0.02)], 0.004, seg=3)


def stair_box(p, x0, x1, y0, y1, z, h=2.25, door='+y', t=0.18):
    """A casinha da escada no teto (o volume que mais quebra a silhueta da casa de cima): as paredes, a laje com a capa, a porta de
    ferro recuada e o rodapé."""
    holes = {}
    W = (x1 - x0) if door[1] == 'y' else (y1 - y0)
    c = ((x0 + x1) / 2) if door[1] == 'y' else ((y0 + y1) / 2)
    holes[door] = [(c - 0.45, c + 0.45, z, z + 2.0)]
    at = {'+x': x1, '-x': x0, '+y': y1, '-y': y0}
    walls_with_posts(p, x0, x1, y0, y1, z - 0.02, z + h, t, holes, plinth=0, wear=0.5)
    p.box('Plaster#cap', x0 - 0.06, x1 + 0.06, y0 - 0.06, y1 + 0.06, z + h, z + h + 0.14)
    p.box('Plaster#slab', x0 + t - 0.02, x1 - t + 0.02, y0 + t - 0.02, y1 - t + 0.02, z + h - 0.15, z + h + 0.01, bevel=0.0)
    F = WallFace(door, at[door], 0.0)
    steel_door(p, F, c - 0.45, c + 0.45, z, z + 2.0, t=t, canopy=False, step=False)


def ext_stair(p, x_bottom, x_top, y_wall, side, z_top, width=0.95, m='Plaster'):
    """A escada de fora encostada na parede (a da casa da região: maciça, de alvenaria rebocada, com a mureta inclinada do lado de
    fora e o corrimão de cano): os degraus, a mureta, o patamar de cima. Corre em X de x_bottom (o pé) a x_top, encostada na face
    de y_wall, pro lado side (+1: +y)."""
    n = max(6, int(z_top / 0.19))
    rise = z_top / n
    run = (x_top - x_bottom) / n
    ya, yb = sorted((y_wall, y_wall + side * width))
    for k in range(n):
        xa = x_bottom + k * run
        xs = sorted((xa, xa + run))
        p.box('Plaster#step', xs[0] - 0.01, xs[1] + 0.01, ya, yb, 0.0 if k == 0 else (k - 1) * rise, (k + 1) * rise)
    # a mureta inclinada do lado de fora (o perfil que desenha a escada na parede)
    yo = y_wall + side * width
    y0_, y1_ = sorted((yo, yo + side * 0.16))
    xd = 1 if x_top > x_bottom else -1
    hexa(p, m, [(x_bottom - xd * 0.05, y0_, -0.05), (x_top, y0_, -0.05), (x_bottom - xd * 0.05, y1_, -0.05), (x_top, y1_, -0.05),
                (x_bottom - xd * 0.05, y0_, 0.85), (x_top, y0_, z_top + 0.85), (x_bottom - xd * 0.05, y1_, 0.85), (x_top, y1_, z_top + 0.85)])
    # o corrimão de cano por cima da mureta
    vp.tube(p, 'Metal#rail', [(x_bottom, (y0_ + y1_) / 2, 0.98), (x_top, (y0_ + y1_) / 2, z_top + 0.98)], 0.022, seg=8)
    for k in range(4):
        u = (k + 0.5) / 4
        xx = lerp(x_bottom, x_top, u)
        zz = lerp(0.85, z_top + 0.85, u)
        vp.tube(p, 'Metal#rail', [(xx, (y0_ + y1_) / 2, zz), (xx, (y0_ + y1_) / 2, zz + 0.14)], 0.016, seg=6)


def awning(p, x0, x1, y_wall, depth, z_top, z_front, m='Cloth#awn', posts=True, frame_m='Wood#pole', seed=0, valance=True,
           stripes=None):
    """O toldo de pano da loja (o mercado): o pano esticado da parede até a travessa da frente com a barriga e as dobras, a sanefa
    que cai na frente (a borda ondulada), a travessa e os 2–3 paus até o chão, presos na parede pelo sarrafo. Sai pra +y."""
    yf = y_wall + depth
    if stripes:
        # o pano listrado (a lona de feira: as faixas de 2 cores, costuradas), a barriga e as ondas contínuas de faixa pra faixa
        mats = (m, stripes)
        W = x1 - x0
        n = max(2, int(round(W / 0.42)))
        for k in range(n):
            a_, b_ = x0 + W * k / n, x0 + W * (k + 1) / n
            ua, ub = k / n, (k + 1) / n

            def zf(u, v, z, ua=ua, ub=ub):
                U = ua + (ub - ua) * u
                return z - 0.09 * math.sin(math.pi * U) * math.sin(math.pi * v) - 0.025 * math.sin(U * 9.0 + seed) * v
            model_lib.sheet(p, mats[k % 2], [(a_, y_wall + 0.02, z_top), (b_, y_wall + 0.02, z_top), (b_, yf, z_front), (a_, yf, z_front)],
                            2, 6, z_fn=zf, thick=0.02)
    else:
        corners = [(x0, y_wall + 0.02, z_top), (x1, y_wall + 0.02, z_top), (x1, yf, z_front), (x0, yf, z_front)]
        from model_building_parts import tarp
        tarp(p, corners, m=m, sag=0.08, folds=3, amp=0.03, thick=0.02, seed=seed)
    vp.rbox(p, frame_m, ((x0 + x1) / 2, y_wall + 0.04, z_top + 0.02), (x1 - x0 + 0.1, 0.06, 0.08), r=0.01, k=1)
    vp.tube(p, frame_m, [(x0 - 0.05, yf, z_front), (x1 + 0.05, yf, z_front)], 0.04, seg=8)
    if valance:
        vp.tarp_drape(p, (x0, yf + 0.02, z_front + 0.01), (x1, yf + 0.02, z_front + 0.01), 0.25, (0, 1, 0), m=m.split('#')[0], folds=6,
                      amp=0.012, thick=0.01, seed=seed + 3, nu=24, nv=3, hem=True, ties=())
    if not posts:
        # as mãos-francesas de ferro da parede até a travessa (o toldo de sacada, sem pau no chão)
        for x in (x0 + 0.1, x1 - 0.1):
            vp.tube(p, 'Metal#arm', [(x, y_wall + 0.02, z_top - 0.55), (x, yf, z_front - 0.02)], 0.018, seg=6)
    if posts:
        n = max(2, int((x1 - x0) / 2.2) + 1)
        for k in range(n):
            x = lerp(x0 + 0.05, x1 - 0.05, k / (n - 1))
            vp.tube(p, frame_m, [(x, yf, 0.0), (x, yf, z_front + 0.03)], 0.045, seg=8)
            vp.rbox(p, 'Plaster#step', (x, yf, 0.06), (0.22, 0.22, 0.12), r=0.03, k=1)


def ladder(p, a, b, w=0.48, m='Wood#ladder'):
    """A escada de mão de madeira encostada (os 2 banzos e os degraus a cada 30 cm)."""
    A, B = Vector(a), Vector(b)
    d = (B - A)
    L = d.length
    d.normalize()
    side = Vector((-d.y, d.x, 0.0))
    if side.length < 1e-6:
        side = Vector((0, 1, 0))
    side.normalize()
    for s in (-1, 1):
        vp.tube(p, m, [A + side * (s * w / 2), B + side * (s * w / 2)], 0.03, seg=6)
    for k in range(1, int(L / 0.3)):
        q = A + d * (k * 0.3)
        vp.tube(p, m, [q - side * (w / 2), q + side * (w / 2)], 0.02, seg=5)


def corrugated(p, x0, x1, y0, y1, z0, z1, axis='x', m='Rust#sheet', pitch=0.09, amp=0.022, th=0.012):
    """A chapa ondulada (o portão e o tapume de ferro velho, a cobertura): a onda de verdade ao longo de 'axis' (o seno, 6 pontos por
    onda), espessura fina e fechada. Plano de z0 (em x0/y0) a z1 (em x1/y1) — de pé quando z0 = z1 e um dos lados é estreito."""
    b = p.bm(m, 0.0)
    L0, L1 = (y0, y1) if axis == 'x' else (x0, x1)
    waves = max(1, int(round((L1 - L0) / (2 * pitch))))
    n = waves * 6
    prof = [(L0 + (L1 - L0) * k / n, amp * math.sin(2 * math.pi * k / 6)) for k in range(n + 1)]

    def pt(s, t, dz):
        if axis == 'x':
            return V(x0 + (x1 - x0) * t, s, z0 + (z1 - z0) * t + dz)
        return V(s, y0 + (y1 - y0) * t, z0 + (z1 - z0) * t + dz)
    top = [[b.verts.new(pt(s, t, dz)) for (s, dz) in prof] for t in (0.0, 1.0)]
    bot = [[b.verts.new(pt(s, t, dz - th)) for (s, dz) in prof] for t in (0.0, 1.0)]
    for k in range(n):
        b.faces.new([top[0][k], top[0][k + 1], top[1][k + 1], top[1][k]]).smooth = True
        b.faces.new([bot[1][k], bot[1][k + 1], bot[0][k + 1], bot[0][k]]).smooth = True
    b.faces.new([top[0][0], top[1][0], bot[1][0], bot[0][0]])
    b.faces.new([top[0][n], bot[0][n], bot[1][n], top[1][n]])
    for r in (0, 1):
        for k in range(n):
            if r == 0:
                b.faces.new([top[0][k + 1], top[0][k], bot[0][k], bot[0][k + 1]])
            else:
                b.faces.new([top[1][k], top[1][k + 1], bot[1][k + 1], bot[1][k]])


def corr_panel(p, a, b, z0, z1, m='Rust#sheet', pitch=0.08, amp=0.02, th=0.012, lean=0.0):
    """A chapa ondulada DE PÉ de a até b (x, y), as ondas de pé (verticais: o tapume, o portão de ferro), inclinada 'lean' no topo."""
    A, B = Vector((a[0], a[1], 0.0)), Vector((b[0], b[1], 0.0))
    d = B - A
    L = d.length
    d.normalize()
    nrm = Vector((-d.y, d.x, 0.0))
    waves = max(1, int(round(L / (2 * pitch))))
    n = waves * 6
    bb = p.bm(m, 0.0)
    rows = []
    for zz in (z0, z1):
        lean_off = nrm * (lean * (zz - z0) / max(1e-6, z1 - z0))
        front, back = [], []
        for k in range(n + 1):
            q = A + d * (L * k / n) + nrm * (amp * math.sin(2 * math.pi * k / 6)) + lean_off
            front.append(bb.verts.new(V(q.x, q.y, zz)))
            q2 = q - nrm * th
            back.append(bb.verts.new(V(q2.x, q2.y, zz)))
        rows.append((front, back))
    (f0, b0), (f1, b1) = rows
    for k in range(n):
        bb.faces.new([f0[k], f0[k + 1], f1[k + 1], f1[k]]).smooth = True
        bb.faces.new([b1[k], b1[k + 1], b0[k + 1], b0[k]]).smooth = True
        bb.faces.new([f1[k], f1[k + 1], b1[k + 1], b1[k]])
        bb.faces.new([b0[k], b0[k + 1], f0[k + 1], f0[k]])
    bb.faces.new([f0[0], f1[0], b1[0], b0[0]])
    bb.faces.new([f0[n], b0[n], b1[n], f1[n]])


def chunk(p, c, s, seed, m='Plaster#rub'):
    """Um bloco de entulho: a caixa torta com os cantos puxados ao acaso (a quebra), girada."""
    x, y, z = c
    yaw = 360 * h3(seed, 1, 3)
    pitch = (h3(seed, 2, 3) - 0.5) * 50
    roll = (h3(seed, 3, 3) - 0.5) * 50
    ex, ey, ez = vp.ypr_frame(yaw, pitch, roll)
    sx, sy, sz = s * (0.7 + 0.6 * h3(seed, 4, 3)), s * (0.5 + 0.5 * h3(seed, 5, 3)), s * (0.35 + 0.4 * h3(seed, 6, 3))
    P = []
    for iz in (-1, 1):
        for iy in (-1, 1):
            for ix in (-1, 1):
                j = 0.75 + 0.5 * h3(seed, ix * 3 + iy * 5 + iz * 7 + 20, 3)
                q = Vector((x, y, z)) + ex * (ix * sx / 2 * j) + ey * (iy * sy / 2 * j) + ez * (iz * sz / 2 * j)
                P.append(tuple(q))
    hexa(p, m, P, bevel=0.0)


def loose_tyre(p, c, R=0.33, w=0.2, axis='z', m='Rubber#old', seg=24):
    """O pneu velho solto (sem a roda): a carcaça oca (o laço do perfil: a banda com os sulcos, o ombro, o flanco abaulado e o talão
    aberto no meio), girada em volta do eixo."""
    hw = w / 2
    ri = R * 0.56
    prof = [(R - 0.006, -hw * 0.75), (R, -hw * 0.55), (R - 0.012, -hw * 0.45), (R - 0.012, -hw * 0.35), (R, -hw * 0.25),
            (R, -hw * 0.06), (R - 0.012, 0.0), (R, hw * 0.06), (R, hw * 0.25), (R - 0.012, hw * 0.35), (R - 0.012, hw * 0.45),
            (R, hw * 0.55), (R - 0.006, hw * 0.75), (R - 0.03, hw * 0.95), (R - 0.07, hw * 1.02), ((R + ri) / 2, hw * 1.0),
            (ri + 0.02, hw * 0.78), (ri, hw * 0.6), (ri, -hw * 0.6), (ri + 0.02, -hw * 0.78), ((R + ri) / 2, -hw * 1.0),
            (R - 0.07, -hw * 1.02), (R - 0.03, -hw * 0.95)]
    vp.lathe(p, m, tuple(c), axis, prof, seg=seg, smooth=True, closed=True)


def pockmarks(p, F, u0, u1, z0, z1, n, seed=0, holes=()):
    """Os buracos de tiro e de estilhaço no reboco (as ruínas dos game shots são crivadas): a cratera rasa do reboco arrancado (o
    disco irregular cru, 8–16 cm) com o furo escuro no meio; em grupos (a rajada), fora dos vãos."""
    nrm = Vector(F.normal())
    eu = Vector(F.pt(1.0, 0.0, 0.0)) - Vector(F.pt(0.0, 0.0, 0.0))
    ev = Vector((0.0, 0.0, 1.0))
    k, tries = 0, 0
    while k < n and tries < n * 8:
        tries += 1
        gu = lerp(u0 + 0.3, u1 - 0.3, h3(tries // 5, seed, 3)) + (h3(tries, seed, 5) - 0.5) * 0.9
        gz = lerp(z0 + 0.3, z1 - 0.3, h3(tries // 5, seed, 7)) + (h3(tries, seed, 9) - 0.5) * 0.7
        if not (u0 + 0.1 < gu < u1 - 0.1 and z0 + 0.1 < gz < z1 - 0.1):
            continue
        if any(a - 0.12 < gu < b + 0.12 and c - 0.12 < gz < d + 0.12 for (a, b, c, d) in holes):
            continue
        R = 0.04 + 0.05 * h3(tries, seed, 11)
        pts = [(R * (0.7 + 0.5 * h3(j, tries, seed)) * math.cos(2 * math.pi * j / 7), R * (0.7 + 0.5 * h3(j, tries, seed + 1)) * math.sin(2 * math.pi * j / 7))
               for j in range(7)]
        o = Vector(F.pt(gu, gz, -0.006))
        vp.prism(p, 'BrokenAdobe#pock', pts, o, eu, ev, nrm, 0.009)
        vp.prism(p, 'Dark#pock', [(x * 0.35, y * 0.35) for (x, y) in pts], o, eu, ev, nrm, 0.011)
        k += 1
