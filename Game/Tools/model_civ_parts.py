"""As peças do kit civil v2 (o pedido de 01/11/2026: "grandes recursos de civilidade [...] tudo bem detalhado, nada de cantos
retos feios"; a lista viva e as referências em docs/KIT_CIVIL_V2.md).

O método é o do elenco (docs/ESTUDO_DETALHE_AAA.md): a forma no PERFIL (seção arredondada, perfil girado, varredura), nunca caixa
crua nem remesh; o executor model_run_detail.py passa a grade, o UV e as máscaras (quina, contato, fresta) na cor de vértice.

Aqui:
- os carros civis (car_build): a lataria de baixo é um loft de SEÇÕES ao longo do X (o ombro redondo, o pé redondo, a lateral com
  a barriga leve e a parede que entra pra cima, o capô e a tampa com a coroa), cortada pelo arco das rodas; a estufa é um loft de
  ANÉIS em planta (o retângulo de cantos redondos subindo e encolhendo: o para-brisa inclinado, as colunas redondas, o teto com a
  coroa); os vidros são chapas que seguem a estufa, com a borracha escura em volta; para-choques varridos que dobram nos cantos,
  faróis, grade, maçanetas, retrovisores, placas, pneus de verdade com a calota; e a CARCAÇA QUEIMADA do mesmo carro (sem vidro,
  as colunas e o teto afundado, o carvão com a ferrugem de calor e a tinta que sobrou embaixo, os bancos em mola, sentada nos aros);
- (mais abaixo) as miudezas de rua e de telhado.

Convenção do kit civil (docs/WORLDFORGE.md §12): a malha com a frente em +Y (Unreal) no giro 0, a origem no centro da planta, no
chão. Os carros são modelados com a frente em +X (o padrão dos veículos) e girados no fim (to_kit).
Coordenadas da Unreal (X frente, Y direita, Z cima, metros); o model_lib.V troca o Y pro Blender.
Slots (o nome do material antes do '#'; o import_provisional.py troca pelo CIVIL_SLOTS ou pelo SLOTS): Paint (a lataria branca,
TankPaint), GlaPaint (bege), Crate (oliva), Lens (vidro, Glass), Dark, Rubber, Metal (CivMetal), Char, Rust, Cloth, Wood, Plaster.
"""
import math

import bmesh
import bpy
from mathutils import Matrix, Vector

import model_lib
from model_lib import COLORS, Part, V
import model_vehicle_parts as vp


# ===================================================================================================== cores de conferência
def _srgb8(c):
    return int(round(255 * ((12.92 * c) if c <= 0.0031308 else (1.055 * c ** (1 / 2.4) - 0.055))))


def _lin(*c):
    return tuple(_srgb8(x) for x in c)


# a cor de cada slot NO JOGO (as do SLOTS do import_provisional.py, em linear, convertidas): a prévia sai com a cor do jogo
COLORS.update({
    'Paint': _lin(0.40, 0.405, 0.38), 'GlaPaint': _lin(0.40, 0.31, 0.14), 'Crate': _lin(0.10, 0.10, 0.045),
    'Lens': _lin(0.008, 0.010, 0.013), 'Glass': _lin(0.008, 0.010, 0.013), 'Dark': _lin(0.025, 0.025, 0.025),
    'Rubber': _lin(0.012, 0.012, 0.012), 'Metal': _lin(0.188, 0.181, 0.155), 'Char': _lin(0.010, 0.008, 0.007),
    'Rust': _lin(0.068, 0.027, 0.013), 'Cloth': _lin(0.305, 0.216, 0.122), 'Wood': _lin(0.107, 0.048, 0.019),
    'Plaster': _lin(0.49, 0.35, 0.20), 'Concrete': _lin(0.25, 0.24, 0.22), 'Body': _lin(0.29, 0.284, 0.267),
    'Yellow': _lin(0.525, 0.30, 0.019), 'BrokenAdobe': _lin(0.40, 0.30, 0.18), 'Broken': _lin(0.30, 0.29, 0.26),
})


# ===================================================================================================== utilidades
def h1(x):
    return (math.sin(x * 127.1 + 311.7) * 43758.5453) % 1.0


def h3(i, j=0, s=0):
    n = (int(i) * 374761393 + int(j) * 668265263 + int(s) * 1442695041) & 0xFFFFFFFF
    n = ((n ^ (n >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((n ^ (n >> 16)) & 0xFFFF) / 65535.0


def vnoise3(x, y, z, s=0):
    i, j, k = math.floor(x), math.floor(y), math.floor(z)
    fx, fy, fz = x - i, y - j, z - k
    fx, fy, fz = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy), fz * fz * (3 - 2 * fz)
    out = 0.0
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                w = (fx if dx else 1 - fx) * (fy if dy else 1 - fy) * (fz if dz else 1 - fz)
                out += w * h3(i + dx + 31 * (k + dz), j + dy, s)
    return out


def fbm3(x, y, z, s=0):
    return vnoise3(x, y, z, s) * 0.62 + vnoise3(x * 2.07 + 3.1, y * 2.07 + 1.3, z * 2.07 + 7.7, s + 1) * 0.38


def lerp(a, b, t):
    return a + (b - a) * t


def smooth01(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3 - 2 * t)


def interp(stations, x, k):
    """O valor k (índice na tupla) das estações [(x, ...)] em x: suavizado entre as vizinhas (smoothstep), constante fora."""
    st = stations
    if x >= st[0][0]:
        return st[0][k]
    if x <= st[-1][0]:
        return st[-1][k]
    for a, b in zip(st[:-1], st[1:]):
        if a[0] >= x >= b[0]:
            t = (a[0] - x) / max(1e-9, a[0] - b[0])
            return lerp(a[k], b[k], smooth01(t) * 0.5 + t * 0.5)
    return st[-1][k]


def join(objs, name):
    objs = [o for o in objs if o is not None]
    model_lib.select_only(objs[0])
    for o in objs:
        o.select_set(True)
    if len(objs) > 1:
        bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    o.name = name
    return o


def to_kit(o):
    """A frente de +X (como foi modelado) pra +Y da Unreal (a convenção do kit civil, docs/WORLDFORGE.md §12): +90° na Unreal = −90° no
    Blender (o Y do Blender é o contrário)."""
    o.data.transform(Matrix.Rotation(math.radians(-90.0), 4, 'Z'))
    o.data.update()


def tris(o):
    return sum(len(p.vertices) - 2 for p in o.data.polygons)


def loft_mat(p, rings, matfn, closed=True, smooth=True, caps=(False, False), cap_m=None):
    """O loft de anéis com o material escolhido por face (matfn(centro) → chave do Part): a carcaça queimada pinta o carvão, a
    ferrugem e a tinta que sobrou na própria lataria (a emenda entre materiais é uma aresta dura: lê como a borda da mancha)."""
    cache = {}
    n = len(rings[0])

    def vert(m, i, j):
        key = (m, i, j)
        if key not in cache:
            cache[key] = p.bm(m, 0.0).verts.new(V(*rings[i][j]))
        return cache[key]
    for i in range(len(rings) - 1):
        for j in range(n if closed else n - 1):
            jj = (j + 1) % n
            q = [rings[i][j], rings[i][jj], rings[i + 1][jj], rings[i + 1][j]]
            c = Vector((sum(a[0] for a in q) / 4, sum(a[1] for a in q) / 4, sum(a[2] for a in q) / 4))
            m = matfn(c)
            f = p.bm(m, 0.0).faces.new([vert(m, i, j), vert(m, i, jj), vert(m, i + 1, jj), vert(m, i + 1, j)])
            f.smooth = smooth
    for k, idx in ((0, 0), (1, len(rings) - 1)):
        if caps[k]:
            m = cap_m or matfn(Vector(rings[idx][0]))
            vs = [vert(m, idx, j) for j in range(n)]
            p.bm(m, 0.0).faces.new(vs if k else list(reversed(vs)))


def clip_poly(poly, vals, t, above):
    """O pedaço do polígono onde o campo (vals nos vértices, linear nas arestas) é ≥ t (above) ou < t: o recorte pela isolinha."""
    out = []
    n = len(poly)
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        va, vb = vals[i], vals[(i + 1) % n]
        ina = (va >= t) == above
        inb = (vb >= t) == above
        if ina:
            out.append(a)
        if ina != inb and abs(vb - va) > 1e-12:
            u = (t - va) / (vb - va)
            out.append(a + (b - a) * u)
    return out


def loft_layers(p, rings, base, layers, closed=True, smooth=True, caps=(False, False)):
    """O loft com as manchas recortadas pela isolinha de cada camada (layers = [(campo(c), limiar, material)...], na ordem: a de
    depois pinta por cima). A borda da mancha corta a face no meio (não segue a grade): a mancha sai orgânica, não em pixel."""
    n = len(rings[0])

    def emit(poly, m):
        if len(poly) < 3:
            return
        b = p.bm(m, 0.0)
        vs = [b.verts.new(V(*q)) for q in poly]
        try:
            f = b.faces.new(vs)
            f.smooth = smooth
        except ValueError:
            pass
    quads = []
    for i in range(len(rings) - 1):
        for j in range(n if closed else n - 1):
            jj = (j + 1) % n
            quads.append([Vector(rings[i][j]), Vector(rings[i][jj]), Vector(rings[i + 1][jj]), Vector(rings[i + 1][j])])
    for k, idx in ((0, 0), (1, len(rings) - 1)):
        if caps[k]:
            ring = [Vector(q) for q in rings[idx]]
            c = sum(ring, Vector()) / len(ring)
            for j in range(n):
                tri = [c, ring[j], ring[(j + 1) % n]] if k else [c, ring[(j + 1) % n], ring[j]]
                quads.append(tri)
    for q in quads:
        polys = [(q, base)]
        for (fn, t, m) in layers:
            new = []
            for (poly, mm) in polys:
                vals = [fn(v) for v in poly]
                if all(v >= t for v in vals):
                    new.append((poly, m))
                elif all(v < t for v in vals):
                    new.append((poly, mm))
                else:
                    a = clip_poly(poly, vals, t, True)
                    b = clip_poly(poly, vals, t, False)
                    if len(a) >= 3:
                        new.append((a, m))
                    if len(b) >= 3:
                        new.append((b, mm))
            polys = new
        for (poly, m) in polys:
            emit(poly, m)


def _rect_minus(u0, u1, z0, z1, holes):
    """O retângulo menos os vãos, em tiras sem sobreposição: [(a, b, c, d, _, _)...] (o rect_minus do model_building_parts, eps 0)."""
    hs = [(max(a, u0), min(b, u1), max(c, z0), min(d, z1)) for (a, b, c, d) in holes if a < u1 and b > u0 and c < z1 and d > z0]
    us = sorted(set([u0, u1] + [h[0] for h in hs] + [h[1] for h in hs]))
    out = []
    for a, b in zip(us[:-1], us[1:]):
        if b - a < 1e-4:
            continue
        mid = (a + b) / 2
        cuts = sorted((h[2], h[3]) for h in hs if h[0] < mid < h[1])
        z = z0
        for c, d in cuts:
            if c > z + 1e-4:
                out.append((a, b, z, c, 0, 0))
            z = max(z, d)
        if z1 > z + 1e-4:
            out.append((a, b, z, z1, 0, 0))
    return out


# ===================================================================================================== (v3) a sujeira na cor de vértice
# O pó, a sujeira do vidro e o escorrido não são forma nem tinta: vão na cor de vértice que o jogo já lê (B = 1 − fresta: o material
# puxa pra cor da duna com o CavityDirt; docs/ESTUDO_DETALHE_AAA.md §2.2). O executor (model_run_detail.py) coze as máscaras e chama o
# model_lib.export no fim: o gancho abaixo escurece a B onde a spec do modelo (o JSON em o['civ_dirt']) manda, DEPOIS do cozimento e
# antes de gravar o GLB. Sem a spec (as miudezas, os postes, a rua) nada muda.
def _vn2(x, y, s=0):
    return vnoise3(x, y, 0.37, s)


def car_dirt(spec, p, n, mat):
    """O pó do carro (no quadro do modelo: x frente, y direita, z cima): grosso embaixo (a areia da estrada sobe até a cintura, mais
    atrás das rodas), assentado no que olha pra cima (capô, teto, caçamba) e no vidro, menos no arco dos limpadores."""
    x, y, z = p
    zb = spec['zb']
    k = 1.0 - smooth01((z - zb + 0.10) / (spec.get('dust_h', 0.55)))
    k *= 0.65 + 0.55 * fbm3(x * 2.3, y * 2.3, z * 3.1, 5)
    arch = 0.0
    for ax in spec['axles']:
        dx, dz = x - ax, z - spec['zc']
        r = math.hypot(dx, dz)
        if abs(y) > spec['track'] - 0.35:
            w = 1.0 - smooth01((r - spec['arch_r'] + 0.05) / 0.45)
            arch = max(arch, w * (1.0 if dx < 0 else 0.6))           # o leque atrás da roda
    up = smooth01((n[2] - 0.55) / 0.35) * (0.30 + 0.45 * fbm3(x * 1.7 + 3.0, y * 1.7, 0.2, 9))
    d = max(k, 0.85 * arch) + up
    if mat == 'Lens':
        d = 0.55 + 0.30 * fbm3(x * 3.0, y * 3.0, z * 3.0, 13)
        ws = spec.get('ws')
        if ws and n[0] > 0.25:
            v = math.hypot(ws['x'] - x, z - ws['z'])
            clean = 0.0
            for (yp, L) in ws['pivots']:
                r = math.hypot(y - yp, v)
                clean = max(clean, smooth01((r - 0.10) / 0.06) * (1.0 - smooth01((r - L) / 0.07)))
            d *= 1.0 - 0.85 * clean
        elif n[2] < -0.1:
            d = 0.2
    if mat in ('Rubber', 'Metal') and z < spec['zb'] + 0.2:
        d = max(d, 0.75)
    cab = spec.get('cab')
    if cab and mat in ('Dark', 'Cloth', 'Rust') and cab[0] < x < cab[1] and abs(y) < cab[2] and cab[3] < z < cab[4]:
        d *= 0.2                                     # o interior (o banco, o painel, o piso): o pó de fora não entra

    if spec.get('burnt'):
        d *= 0.15                                    # o carvão e a ferrugem com areia por cima liam marrom, ferrugem em tudo
    return max(0.0, min(1.0, d * spec.get('dust', 0.85)))


def house_dirt(spec, p, n, mat):
    """O escorrido e a umidade da casa (no quadro do modelo): a mancha escura debaixo de cada janela (as estrias saindo das pontas
    do peitoril), da bica e da capa da mureta; o pé úmido de borda irregular; a laje com a mancha de água parada."""
    x, y, z = p
    if mat in ('Glass', 'Dark', 'Rubber'):
        return 0.0
    d = 0.0
    flat = mat in ('Cloth', 'BrokenAdobe')
    sd = spec.get('seed', 0)
    # o pé úmido (a borda de cima ondulada, 0,35–0,9 m)
    if n[2] < 0.5:
        hh = 0.35 + 0.55 * fbm3(x * 0.9, y * 0.9, 0.0, sd + 3)
        d = max(d, 0.75 * (1.0 - smooth01((z - hh * 0.5) / (hh * 0.6))))
    for (face, at, items, z_top) in spec.get('walls', ()):
        ax = 0 if face[1] == 'x' else 1
        sgn = 1.0 if face[0] == '+' else -1.0
        coord = p[ax]
        # a face de fora (e o que está nela: o remendo, a moldura): até 0,5 m pra dentro e 0,8 m pra fora do plano
        if not (-0.5 < (coord - at) * sgn < 0.8) or n[ax] * sgn < 0.35:
            continue
        u = p[1 - ax]
        # o escorrido debaixo da capa da mureta (as estrias ao longo da parede inteira, de 0,3 a 1,6 m)
        if 0.0 < z < z_top:
            col = int(math.floor(u / 0.13))
            hl = 0.25 + 1.45 * h3(col, int(at * 10), sd + 5) ** 2.2
            s_ = 0.35 + 0.6 * h3(col, int(at * 10), sd + 7)
            fu = u / 0.13 - col
            prof = 1.0 - smooth01(abs(fu - 0.5) / 0.5)
            d = max(d, s_ * (0.55 + 0.45 * prof) * (1.0 - smooth01((z_top - z) / hl)))
        for it in items:
            if it[0] == 'flat':
                # o relevo raso (o remendo, a placa de reboco caído com o tijolo): a máscara de quina pegava a peça inteira (só tem
                # vértice na borda) e ela saía clara, gasta como quina; aqui ela fica sem desgaste
                if it[1] < u < it[2] and it[3] < z < it[4] and (coord - at) * sgn < 0.09:
                    flat = True
                continue
            if it[0] == 'win':
                _, u0, u1, zs = it
                if z < zs and u0 - 0.15 < u < u1 + 0.15:
                    col = int(math.floor(u / 0.11))
                    hl = 0.55 + 1.2 * h3(col, int(zs * 10), sd + 11) ** 1.5
                    ends = max(1.0 - smooth01(abs(u - u0) / 0.22), 1.0 - smooth01(abs(u - u1) / 0.22))
                    s_ = 0.45 + 0.35 * ends + 0.2 * h3(col, 3, sd + 13)
                    edge = 1.0 - smooth01((max(u0 - u, u - u1, 0.0)) / 0.15)
                    d = max(d, s_ * edge * (1.0 - smooth01((zs - z) / hl)))
            elif it[0] == 'spout':
                _, us, zs = it
                if z < zs:
                    d = max(d, 0.95 * (1.0 - smooth01(abs(u - us) / 0.16)) * (1.0 - smooth01((zs - z) / 2.6)))
    if n[2] > 0.7 and z > 1.0:
        # a laje: as manchas da água que empoça (largas, rasas)
        d = max(d, 0.55 * smooth01((fbm3(x * 0.45, y * 0.45, 0.5, sd + 17) - 0.45) / 0.2))
    return (max(0.0, min(1.0, d * spec.get('dirt', 0.9))), flat)


def apply_dirt(o):
    """Escurece a B da cor de vértice (o pó/a sujeira que o material lê) pela spec do modelo. Roda no gancho do export, com as
    máscaras já cozidas."""
    import json
    spec = json.loads(o['civ_dirt'])
    me = o.data
    ca = me.color_attributes.get('Col')
    if ca is None:
        return
    cols = [0.0] * (len(me.loops) * 4)
    ca.data.foreach_get('color', cols)
    car = spec.get('frame') == 'car'

    def to_model(v):
        # o carro foi girado pro kit (to_kit: o Blender (x, y) = (−y, −x) do modelo); a casa só tem o Y do Blender trocado
        return (-v[1], -v[0], v[2]) if car else (v[0], -v[1], v[2])
    pos = [to_model(v.co) for v in me.vertices]
    nrm = [to_model(v.normal) for v in me.vertices]
    mats = [m.name.split('.')[0] if m else '' for m in me.materials]
    fn = car_dirt if car else house_dirt
    cache = {}
    k_ = spec.get('k', 0.88)
    for poly in me.polygons:
        mname = mats[poly.material_index] if mats else ''
        for li in poly.loop_indices:
            vi = me.loops[li].vertex_index
            key = (vi, mname)
            if key not in cache:
                cache[key] = fn(spec, pos[vi], nrm[vi], mname)
            d = cache[key]
            if isinstance(d, tuple):
                d, flat = d
                if flat:
                    cols[li * 4] = max(cols[li * 4], 0.85)
            cols[li * 4 + 2] *= 1.0 - k_ * d
    ca.data.foreach_set('color', cols)
    me.update()
    print('DIRT', o.name, len(cache), 'vértices sujos')


# (01/11 noite) O DEFEITO DO EXPORTADOR glTF do Blender 5.2 (io_scene_gltf2/blender/exp/primitive_extract.py, linhas ~571 e ~642): com
# export_vertex_color='ACTIVE', só o 1º material da malha recebe o COLOR_0; nos outros o mapa material → cor guarda a chave do nome
# ('ColCol') no lugar de 'COLOR_0', e o exportador PREENCHE A COR DELES COM 1,0 (branco = sem máscara). Medido nos GLB: a v2 do sedã
# só tinha máscara no Paint; o casco do tanque USA só no Paint (Lens, Team, Dark, Metal, Cloth brancos). Vale pro pipeline inteiro do
# detalhe (model_lib.export); aqui a correção liga SÓ na exportação das peças com a spec de sujeira (os carros e as casas v3): as
# miudezas aprovadas saem idênticas. O conserto de verdade é no model_lib (o mesmo dicionário, sempre ligado).
_VC_FIX = {'on': False}


class _VCMap(dict):
    def __setitem__(self, k, v):
        if _VC_FIX['on'] and isinstance(v, str) and not v.startswith('COLOR_'):
            v = 'COLOR_0'                      # a malha só tem a 'Col': todo material usa a mesma
        super().__setitem__(k, v)


def _install_vc_fix():
    try:
        from io_scene_gltf2.blender.exp import primitive_extract as pe
    except Exception as e:                     # sem o exportador (fora do Blender): nada a fazer
        print('VCFIX indisponível:', e)
        return
    cls = pe.PrimitiveCreator
    if getattr(cls, '_civ_vc_fix', False):
        return
    _init = cls.__init__

    def __init__(self, *a, **k):
        _init(self, *a, **k)
        self.material_idxs_using_vc = _VCMap(self.material_idxs_using_vc)
    cls.__init__ = __init__
    cls._civ_vc_fix = True


def _install_dirt_hook():
    if getattr(model_lib, '_civ_dirt_hook', False):
        return
    _orig = model_lib.export

    def export(o, out_dir):
        if model_lib.DETAIL.get('defer') is None and o.get('civ_dirt'):
            apply_dirt(o)
            _install_vc_fix()
            _VC_FIX['on'] = True
            try:
                return _orig(o, out_dir)
            finally:
                _VC_FIX['on'] = False
        return _orig(o, out_dir)
    model_lib.export = export
    model_lib._civ_dirt_hook = True


_install_dirt_hook()


# ===================================================================================================== a lataria de baixo (seções)
def body_section(hw, zb, zt, rs, rb, tumble, crown, bulge, nb=1, nc=4, nt=3, crease=0.0, tc=0.6, flare=0.0):
    """A seção da lataria em (y, z), um laço fechado com o número fixo de pontos: o pé redondo (rb), a lateral com a barriga (bulge)
    e a parede que entra pra cima (tumble), o ombro redondo (rs) e o topo com a coroa (crown). Começa no meio de baixo, vai pela
    direita (+y) até o meio de cima e volta pela esquerda.
    (v3, 01/11: "parece lego, muitas quinas") o VINCO da lateral (crease: a aresta viva que corre na altura tc da lateral e pega o
    sol numa linha) e o PARA-LAMA QUE SALTA (flare: a parte de baixo da lateral sai pra fora perto das rodas e volta no vinco)."""
    h = max(0.02, zt - crown - zb)
    rs = min(rs, h * 0.45, hw * 0.45)
    rb = min(rb, h * 0.45, hw * 0.45)
    ze = zt - crown                                 # a altura da quina de cima (o topo sobe a coroa até o meio)
    yt = hw - tumble
    hl = hw + flare
    R = []
    for k in range(nb + 1):                           # o fundo
        R.append(((hl - rb) * k / nb, zb))
    for k in range(1, nc):                            # o pé
        a = math.radians(-90 + 90 * k / nc)
        R.append((hl - rb + rb * math.cos(a), zb + rb + rb * math.sin(a)))
    tc = min(0.72, max(0.48, tc))
    for t in (0.0, 0.12, 0.26, 0.40, tc - 0.05, tc, tc + 0.05, 0.80, 0.90, 1.0):     # a lateral (os pontos do vinco fixos)
        z = lerp(zb + rb, ze - rs, t)
        y = lerp(hw, yt, t ** 1.6) + bulge * math.sin(math.pi * t)
        y += flare * (1.0 - smooth01(t / (tc - 0.05)))
        y += crease * max(0.0, 1.0 - abs(t - tc) / 0.05)
        R.append((y, z))
    for k in range(1, nc):                            # o ombro
        a = math.radians(90 * k / nc)
        R.append((yt - rs + rs * math.cos(a), ze - rs + rs * math.sin(a)))
    yc = yt - rs
    for k in range(nt + 1):                           # o topo (a coroa)
        y = yc * (1 - k / nt)
        R.append((y, ze + crown * (1 - (y / max(yc, 1e-6)) ** 2)))
    L = [(-y, z) for (y, z) in reversed(R[1:-1])]
    return R + L


def arch_z(x, axles, R_arch, zc):
    """O arco das rodas: a altura do pé da lataria sobre cada eixo (o círculo em volta do centro da roda)."""
    best = -1.0
    for ax in axles:
        d = x - ax
        if abs(d) < R_arch:
            best = max(best, zc + math.sqrt(R_arch * R_arch - d * d))
    return best


def body_xs(x0, x1, axles, R_arch, step=0.085, fine=0.04, extra=()):
    """As posições das seções: mais finas nos arcos (o arco redondo) e nas pontas (o nariz redondo)."""
    xs = set()
    n = int((x0 - x1) / step) + 1
    for k in range(n + 1):
        xs.add(round(x0 - (x0 - x1) * k / n, 4))
    for ax in axles:
        m = int(2 * (R_arch + 0.03) / fine)
        for k in range(m + 1):
            xs.add(round(ax - R_arch - 0.03 + 2 * (R_arch + 0.03) * k / m, 4))
    for k in range(7):
        xs.add(round(x0 - 0.15 * (k / 6) ** 1.5, 4))
        xs.add(round(x1 + 0.15 * (k / 6) ** 1.5, 4))
    for e in extra:
        xs.add(round(e, 4))
    return sorted((x for x in xs if x1 - 1e-6 <= x <= x0 + 1e-6), reverse=True)


# ===================================================================================================== a estufa (anéis em planta)
SEGS = (('front', 6), ('cFR', 4), ('right', 12), ('cRR', 4), ('rear', 6), ('cRL', 4), ('left', 12), ('cFL', 4))


def ring_point(r, seg, t):
    """O ponto (x, y, z) do anel r = (z, xf, xr, hw, rc) no segmento seg (0..7, ver SEGS) na fração t: o retângulo de cantos
    redondos em planta, começando na frente (y −→ +), dobrando pela direita (x +→ −), a traseira (y +→ −) e a esquerda (x −→ +)."""
    z, xf, xr, hw, rc = r
    rc = min(rc, hw * 0.9, (xf - xr) * 0.45)
    if seg == 0:
        return (xf, lerp(-(hw - rc), hw - rc, t), z)
    if seg == 1:
        a = math.radians(90 * t)
        return (xf - rc + rc * math.cos(a), hw - rc + rc * math.sin(a), z)
    if seg == 2:
        return (lerp(xf - rc, xr + rc, t), hw, z)
    if seg == 3:
        a = math.radians(90 + 90 * t)
        return (xr + rc + rc * math.cos(a), hw - rc + rc * math.sin(a), z)
    if seg == 4:
        return (xr, lerp(hw - rc, -(hw - rc), t), z)
    if seg == 5:
        a = math.radians(180 + 90 * t)
        return (xr + rc + rc * math.cos(a), -(hw - rc) + rc * math.sin(a), z)
    if seg == 6:
        return (lerp(xr + rc, xf - rc, t), -hw, z)
    a = math.radians(270 + 90 * t)
    return (xf - rc + rc * math.cos(a), -(hw - rc) + rc * math.sin(a), z)


def ring_loop(r):
    pts = []
    for seg, n in enumerate([s[1] for s in SEGS]):
        for k in range(n):
            pts.append(ring_point(r, seg, k / n))
    return pts


class Greenhouse:
    """A estufa: os anéis-chave [(s, z, xf, xr, hw, rc)...] de s = 0 (a cintura) a s = 1 (a quina do teto), interpolados; o teto
    com a coroa (crown) e o arredondado da quina (roof_round)."""

    def __init__(self, keys, crown=0.035, roof_round=0.06):
        self.keys = keys
        self.crown = crown
        self.roof_round = roof_round

    def ring(self, s):
        ks = self.keys
        if s <= ks[0][0]:
            return tuple(ks[0][1:])
        for a, b in zip(ks[:-1], ks[1:]):
            if a[0] <= s <= b[0]:
                t = (s - a[0]) / max(1e-9, b[0] - a[0])
                return tuple(lerp(a[i], b[i], t) for i in range(1, 6))
        return tuple(ks[-1][1:])

    def point(self, s, seg, t):
        return Vector(ring_point(self.ring(s), seg, t))

    def s_list(self, per=3):
        out = []
        for a, b in zip(self.keys[:-1], self.keys[1:]):
            for k in range(per):
                out.append(a[0] + (b[0] - a[0]) * k / per)
        out.append(self.keys[-1][0])
        return out

    def roof_rings(self, sag=0.0, s_top=1.0, noise=0.0, seed=0, dense=False):
        """Os anéis do teto: da quina (o último anel) pra dentro, subindo a coroa (e afundando sag no meio: a carcaça). dense (v3): mais
        anéis pro meio (a mancha de ferrugem e o amassado do teto têm vértice onde cair; a tampa do meio fica estreita)."""
        z, xf, xr, hw, rc = self.ring(s_top)
        rr = self.roof_round
        out = []
        dm = min(hw, (xf - xr) / 2)
        steps = ((rr * 0.35, rr * 0.25), (rr, rr * 0.5), (rr + 0.12, self.crown * 0.8), (dm * 0.55, self.crown))
        if dense:
            # a escada dos recuos do meio sempre crescente (no teto curto da picape o dm*0,5 caía antes do rr + 0,22)
            lim = min(hw * 0.8, (xf - xr) * 0.4)
            # (2ª volta: com o canto de 7–12 cm o 2º anel subia acima da coroa e o teto virava uma bandeja com a borda alta, o "quebra-sol"
            # em cima do para-brisa) a altura sempre crescente pro meio, e o meio pelo menos na altura do arredondado da borda
            cr = max(self.crown, rr * 0.55)
            base = [(rr * 0.35, rr * 0.25), (rr, rr * 0.45)]
            d0 = min(rr + 0.10, lim * 0.5)
            inner = [(lerp(d0, lim * 0.92, k / 4), cr * (0.84 + 0.04 * k)) for k in range(1, 5)]
            steps = base + [(d0, cr * 0.8)] + inner
            fixed = []
            for (d_, dz_) in steps:
                if fixed:
                    dz_ = max(dz_, fixed[-1][1] + 0.0015)
                fixed.append((d_, dz_))
            steps = fixed
        for d, dz in steps:
            d = min(d, hw * 0.8, (xf - xr) * 0.4)
            ring = ring_loop((z + dz, xf - d, xr + d, hw - d, max(0.03, rc - d * 0.5)))
            if sag or noise:
                cx, cy = (xf + xr) / 2, 0.0
                ring2 = []
                for (x, y, zz) in ring:
                    u = 1.0 - max(abs(x - cx) / max(0.1, (xf - xr) / 2), abs(y) / max(0.1, hw))
                    zz -= sag * max(0.0, u) ** 0.7 * 1.6
                    zz += noise * (fbm3(x * 3.1, y * 3.1, 0.3, seed) - 0.5)
                    ring2.append((x, y, zz))
                ring = ring2
            out.append(ring)
        return out


def surf_slab(p, m, fn, s0, s1, t0, t1, ns, nt, off=0.004, th=0.008, inward=None, smooth=True):
    """A chapa fina que segue uma superfície fn(s, t) → Vector: a grade (ns × nt), afastada 'off' pela normal (pra fora: a que se
    afasta de 'inward', o ponto de dentro) e com a espessura th (as duas faces e as bordas)."""
    P = [[fn(lerp(s0, s1, i / ns), lerp(t0, t1, j / nt)) for j in range(nt + 1)] for i in range(ns + 1)]
    N = []
    for i in range(ns + 1):
        row = []
        for j in range(nt + 1):
            ds = P[min(i + 1, ns)][j] - P[max(i - 1, 0)][j]
            dt = P[i][min(j + 1, nt)] - P[i][max(j - 1, 0)]
            n = ds.cross(dt)
            n = n.normalized() if n.length > 1e-9 else Vector((0, 0, 1))
            if inward is not None and n.dot(P[i][j] - Vector(inward)) < 0:
                n = -n
            row.append(n)
        N.append(row)
    b = p.bm(m, 0.0)
    top = [[b.verts.new(V(*(P[i][j] + N[i][j] * (off + th)))) for j in range(nt + 1)] for i in range(ns + 1)]
    bot = [[b.verts.new(V(*(P[i][j] + N[i][j] * off))) for j in range(nt + 1)] for i in range(ns + 1)]
    for i in range(ns):
        for j in range(nt):
            b.faces.new([top[i][j], top[i][j + 1], top[i + 1][j + 1], top[i + 1][j]]).smooth = smooth
            b.faces.new([bot[i + 1][j], bot[i + 1][j + 1], bot[i][j + 1], bot[i][j]]).smooth = smooth
    edge = [(0, j) for j in range(nt)] + [(i, nt) for i in range(ns)] + [(ns, j) for j in range(nt, 0, -1)] + [(i, 0) for i in range(ns, 0, -1)]
    for (i0, j0), (i1, j1) in zip(edge, edge[1:] + edge[:1]):
        b.faces.new([top[i0][j0], bot[i0][j0], bot[i1][j1], top[i1][j1]])


# ===================================================================================================== o carro
class Car:
    """Um carro civil por especificação (spec, um dict; ver model_civ_sedan.py): monta a lataria, a estufa, os detalhes e as rodas.
    burnt=True: a carcaça queimada do mesmo carro."""

    def __init__(self, spec, burnt=False, seed=1):
        self.s = spec
        self.burnt = burnt
        self.seed = seed
        self.paint = spec.get('paint', 'Paint')
        s = spec
        self.xf, self.xr = s['L'] / 2, -s['L'] / 2
        self.axles = (s['FA'], s['RA'])
        self.zc = s['R']                                     # o centro da roda (no chão: o raio)
        self.gh = Greenhouse(s['gh'], crown=s.get('crown', 0.035), roof_round=s.get('roof_round', 0.06))
        self.center = Vector(((s['gh'][0][2] + s['gh'][0][3]) / 2, 0.0, (s['gh'][0][1] + s['gh'][-1][1]) / 2))

    # -------------------------------------------------------------- materiais da carcaça
    def burn_field(self, c):
        """O campo da ferrugem de calor (≥ 0: ferrugem): onde o fogo foi mais quente a tinta queimou toda e o aço enferrujou (o alto:
        teto, capô, tampa, a parte de cima das portas); embaixo e nas bordas fica o carvão da fuligem. A borda ruidosa em 2 escalas
        (~25 cm e ~1 m): mancha de queimado, não camuflagem."""
        zb, belt = self.s['zb'], self.s['gh'][0][1]
        sd = self.seed
        # (01/11, a avaliação do kit: ainda lia como camuflagem, as manchas grandes de borda dura em marrom e preto pela lateral; o alvo,
        # a carcaça do 16_beauty_pass, é a lateral em carvão e a ferrugem no alto) a ferrugem só nas superfícies de cima (do friso pra
        # cima: o teto, o capô, a tampa), a borda ruidosa de ~20 cm; a lateral inteira é o carvão. A estria fina que escorre não se
        # resolve na malha do carro (a face tem 20–30 cm: o recorte vira bolha): o miúdo é do material de queimado do jogo
        return (c.z - (belt - 0.04)) / 0.06 + (fbm3(c.x * 5.0, c.y * 5.0, c.z * 5.0, sd) - 0.5) * 1.0

    def burn_layers(self, roof=False):
        """As camadas da carcaça (RECEITAS_DO_LOOK §4): o carvão de base; a ferrugem de calor em cima (burn_field); o aço oxidado cinza
        em manchas pequenas no teto; a tinta que sobrou só embaixo (a saia, pouca e miúda)."""
        sd = self.seed
        low = self.s['zb']
        roof_z = self.s['gh'][-1][1] - 0.04
        rust = (self.burn_field, 0.0, 'Rust#burnt')
        ash = (lambda c: fbm3(c.x * 3.0 + 11, c.y * 3.0, c.z * 3.0, sd + 3) - (0.0 if c.z > roof_z else 1.0), 0.66, 'Metal#burnt')
        paint = (lambda c: fbm3(c.x * 4.0 + 5, c.y * 4.0, c.z * 4.0, sd + 7) - (0.0 if c.z < low + 0.18 else 1.0), 0.70, 'Paint#burnt')
        # a fuligem que subiu pelas janelas e manchou o alto (o preto por cima da ferrugem, em manchas de ~40 cm)
        # (01/11) miúda, ~20 cm: a mancha de 40 cm por cima da ferrugem era o que mais lia como camuflagem
        soot = (lambda c: fbm3(c.x * 5.0 + 20, c.y * 5.0, c.z * 5.0, sd + 11), 0.68, 'Char#burnt')
        # (v3: o cinza largo no alto foi tentado e saiu camuflagem de novo, o cinza claro contra a ferrugem escura; volta o da v2)
        return [rust, soot, ash, paint] if not roof else [rust, soot, ash]

    def burn_mat(self, c, low=0.0):
        """O material de uma peça pequena inteira da carcaça (a coluna, o para-choque): pelo campo da ferrugem no centro dela."""
        return 'Rust#burnt' if self.burn_field(c) >= 0.0 else 'Char#burnt'

    def mat_body(self, c):
        return self.paint + '#body'

    # -------------------------------------------------------------- (v3) a tinta de guerra do carro inteiro
    def paint_layers(self, key='#body'):
        """As camadas da lataria do carro inteiro (v3, 01/11, a crítica: "carro de concessionária, branco limpo"): a PORTA DE OUTRA COR
        e o CAPÔ trocado (o painel de ferro-velho: o recorte cai exatamente na junta, x e y retos) e a FERRUGEM onde a lata apodrece (o
        pé da lataria e a volta dos arcos), recortada pela isolinha do ruído (a borda corta a face: mancha orgânica, não pixel). O pó e a
        sujeira não são camada: vão na cor de vértice (apply_dirt)."""
        s, d = self.s, self.s['details']
        sd = self.seed
        out = []
        z_roof = self.gh.keys[-1][1] - 0.03
        for (side, xa, xb, m) in d.get('door_swap', ()):
            # (na estufa a porta pega só o quadro da janela, não o teto)
            out.append((lambda c, side=side, xa=xa, xb=xb: min(c.x - xa, xb - c.x, c.y * side - 0.30,
                                                                (z_roof - c.z) if key == '#gh' else 1.0), 0.0, m + '#door'))
        hs = d.get('hood_swap')
        if hs and d.get('hood'):
            hx0, hx1, hy = d['hood']
            out.append((lambda c: min(c.x - hx0, hx1 - c.x, hy - abs(c.y), c.z - self.top_z(c.x, c.y) + 0.035), 0.0, hs + '#hood'))
        zb = s['zb']
        rust = s.get('rust', 0.5)

        def arch_near(c):
            best = 9.0
            for ax in self.axles:
                best = min(best, abs(math.hypot(c.x - ax, c.z - self.zc - s.get('arch_dz', 0.02)) - s['arch_r']))
            return 1.0 - smooth01(best / 0.10)

        # (v3, 2ª volta: a 1ª, manchas de ferrugem de 20–40 cm espalhadas pela lata inteira, leu CAMUFLAGEM, o defeito que a carcaça da v2
        # já tinha) o carro velho do deserto: a ferrugem SÓ onde a lata apodrece (a faixa de baixo das portas e da soleira, de borda
        # irregular, e a volta dos arcos). O resto é a tinta com o pó (cor de vértice).
        def rust_f(c):
            if key == '#gh':
                return -1.0
            n = fbm3(c.x * 4.0, c.y * 4.0, c.z * 4.0, sd + 31)
            low = 1.0 - smooth01((c.z - zb - 0.05) / 0.11)
            return (n - 0.5) * 0.6 + rust * (0.9 * low + 0.7 * arch_near(c)) - 0.36
        # (3ª volta: o verniz descascado no Metal saía brilhante e estufado na borda do teto, lia como bolha; tirado: o sol de anos fica
        # pro material, o tom por peça, e o que olha pra cima já leva o pó grosso na cor de vértice)
        out.append((rust_f, 0.0, 'Rust#paint'))
        return out

    def loft_body(self, p, rings, caps=(True, True), roof=False, key='#body'):
        if self.burnt:
            loft_layers(p, rings, 'Char#burnt', self.burn_layers(roof), closed=True, smooth=True, caps=caps)
        else:
            loft_layers(p, rings, self.paint + key, self.paint_layers(key), closed=True, smooth=True, caps=caps)

    # -------------------------------------------------------------- a lataria de baixo
    def body_stations(self):
        return self.s['body']             # [(x, zb, zt, hw)...] da frente pra trás

    def section_at(self, x):
        s = self.s
        st = self.body_stations()
        zb = interp(st, x, 1)
        zt = interp(st, x, 2)
        hw = interp(st, x, 3)
        a = arch_z(x, self.axles, s['arch_r'], self.zc + s.get('arch_dz', 0.02))
        if a > zb:
            zb = min(a, zt - 0.06)
        crown = interp(st, x, 4) if len(st[0]) > 4 else s.get('body_crown', 0.02)
        # (v3) o para-lama que salta: o máximo em cima de cada roda, morrendo ~0,3 m depois do arco
        fl = 0.0
        if s.get('flare'):
            for ax in self.axles:
                fl = max(fl, s['flare'] * (1.0 - smooth01((abs(x - ax) - (s['arch_r'] - 0.04)) / 0.30)))
        sec = body_section(hw, zb, zt, s.get('rs', 0.07), s.get('rb', 0.06), s.get('tumble', 0.03), crown, s.get('bulge', 0.012),
                           crease=s.get('crease', 0.0), tc=s.get('tc', 0.6), flare=fl)
        return [self.dent((x, y, z), zt - crown, hw) for (y, z) in sec]

    def dent(self, q, z_top=None, hw=None):
        """(v3) Os amassados da spec ('dents'), assimétricos: 'side' (x, lado, z, raio, fundo) empurra a lateral pra dentro; 'top'
        (x, y, raio, fundo) afunda o capô ou a tampa; 'corner' (x da ponta, lado, z, raio, fundo) a quina batida (a ponta recua e
        dobra pra dentro). O perfil (1 − d²)² com o miolo mais fundo: a chapa cede redonda, sem degrau."""
        x, y, z = q
        for dn in self.s.get('dents', ()):
            k = dn[0]
            if k == 'side':
                _, dx, side, dz, r, dep = dn
                if y * side <= 0.05:
                    continue
                d = math.hypot(x - dx, (z - dz) * 1.15) / r
                if d < 1.0:
                    y -= side * dep * (1.0 - d * d) ** 2
            elif k == 'top' and z_top is not None:
                _, dx, dy, r, dep = dn
                if z < z_top - 0.05:
                    continue
                d = math.hypot(x - dx, y - dy) / r
                if d < 1.0:
                    z -= dep * (1.0 - d * d) ** 2 * smooth01((z - z_top + 0.05) / 0.05)
            elif k == 'corner':
                _, xe, side, dz, r, dep = dn
                yy = side * (hw if hw is not None else 0.85)
                d = math.sqrt((x - xe) ** 2 + ((y - yy) * 0.8) ** 2 + ((z - dz) * 0.9) ** 2) / r
                if d < 1.0 and y * side > -0.2:
                    w = (1.0 - d * d) ** 2
                    x -= math.copysign(dep * w, xe)
                    y -= side * dep * 0.45 * w
        return (x, y, z)

    def lower_body(self, p, x0=None, x1=None):
        s = self.s
        x0 = self.xf if x0 is None else x0
        x1 = self.xr if x1 is None else x1
        xs = body_xs(x0, x1, self.axles, s['arch_r'], extra=s.get('body_extra_x', ()))
        rings = [self.section_at(x) for x in xs]
        if self.burnt:
            # a lata amassada pelo calor: o topo e a lateral ondulam (±1,5 cm)
            rings = [[(x, y, z + 0.03 * (fbm3(x * 2.4, y * 2.4, 0.5, self.seed + 9) - 0.5) * (1 if z > self.s['zb'] + 0.25 else 0.3))
                      for (x, y, z) in r] for r in rings]
        self.loft_body(p, rings, caps=(True, True))
        self.body_rings = (xs, rings)
        # (v3) a borda do arco da roda (a chapa dobrada pra dentro: a linha que pega o sol e desenha o para-lama de cima)
        for i in s.get('lips', (0, 1)):
            ax = self.axles[i]
            R = s['arch_r'] + 0.006
            zc = self.zc + s.get('arch_dz', 0.02)
            for side in (-1, 1):
                pts = []
                for k in range(19):
                    a = math.radians(4 + 172 * k / 18)
                    x, z = ax + R * math.cos(a), zc + R * math.sin(a)
                    pts.append((x, side * (self.side_y(x, z + 0.02) - 0.006), z))
                vp.tube(p, ('Rust#burnt' if self.burnt else self.paint + '#lip'), pts, 0.016, seg=6)

    def top_z(self, x, y):
        """A altura do topo da lataria em (x, y): o perfil do capô e da tampa com a coroa (pra assentar faixa, junta e friso)."""
        st = self.body_stations()
        zt = interp(st, x, 2)
        hw = interp(st, x, 3)
        crown = interp(st, x, 4) if len(st[0]) > 4 else self.s.get('body_crown', 0.02)
        yc = hw - self.s.get('tumble', 0.03) - self.s.get('rs', 0.07)
        return zt - crown + crown * max(0.0, 1 - (y / max(yc, 1e-6)) ** 2)

    def side_y(self, x, z):
        """A lateral (y > 0) da lataria em (x, z): o cruzamento mais de fora da metade direita da seção com a altura z (acima do ombro:
        a largura máxima)."""
        sec = self.section_at(x)
        n = len(sec) // 2
        R = [(y, zz) for (_, y, zz) in sec[:n + 1]]
        best = None
        for (ya, za), (yb, zb) in zip(R[:-1], R[1:]):
            if (za - z) * (zb - z) <= 0 and abs(zb - za) > 1e-9:
                y = ya + (yb - ya) * (z - za) / (zb - za)
                best = y if best is None else max(best, y)
        return best if best is not None else max(y for (y, _) in R)

    # -------------------------------------------------------------- a estufa
    def greenhouse(self, p):
        gh = self.gh
        if not self.burnt:
            # (v3) a estufa vazada pelos vãos das janelas (o vidro fechado vai por cima do vão; o aberto deixa ver o banco e o painel):
            # as colunas, as cintas e o quadro em chapas com 3 cm de espessura, recortadas pelo complemento das janelas em cada lado
            c = self.center
            wins = {}
            for w in self.s['windows']:
                wins.setdefault(w[0], []).append((w[1], w[2], w[3], w[4]))
            door = [(side, xa, xb, m) for (side, xa, xb, m) in self.s['details'].get('door_swap', ())]
            for seg, (_, nseg) in enumerate(SEGS):
                for (ta, tb, sa, sb, _, _) in _rect_minus(0.0, 1.0, 0.0, 1.0, wins.get(seg, [])):
                    fn = (lambda s, t, seg=seg: gh.point(s, seg, t))
                    mid = gh.point((sa + sb) / 2, seg, (ta + tb) / 2)
                    m = self.paint + '#gh'
                    for (side, xa, xb, mm) in door:
                        if seg in (2, 6) and xa < mid.x < xb and mid.y * side > 0.3:
                            m = mm + '#door'
                    surf_slab(p, m, fn, sa, sb, ta, tb, max(1, int(round((sb - sa) * 6))), max(1, int(round((tb - ta) * nseg))),
                              off=-0.03, th=0.03, inward=c)
            rings = [ring_loop(gh.ring(1.0))] + gh.roof_rings(dense=True)
            rings = [[self.roof_dent(q) for q in r] for r in rings]
            self.loft_body(p, rings, caps=(False, True), key='#gh')
            self.glass(p)
        else:
            self.burnt_frame(p)

    def roof_dent(self, q):
        """(v3) O amassado do teto ('roof': x, y, raio, fundo): só nos anéis do teto (o de cima da estufa)."""
        x, y, z = q
        zr = self.gh.keys[-1][1] - 0.01
        if z < zr:
            return q
        for dn in self.s.get('dents', ()):
            if dn[0] != 'roof':
                continue
            _, dx, dy, r, dep = dn
            d = math.hypot(x - dx, y - dy) / r
            if d < 1.0:
                z -= dep * (1.0 - d * d) ** 2
        return (x, y, z)

    def glass(self, p):
        """Os vidros: a chapa escura que segue a estufa, 5 mm pra fora, com a borracha (Dark) 2 cm maior por baixo.
        (v3) O 6º campo da janela é quanto ela está ABERTA (0..1): o vidro desce pra dentro da porta e sobra a beira dele no peitoril
        (o calor da região: o carro roda de janela aberta); o vão mostra o banco e o painel. O vidro que fica leva o pó na cor de
        vértice (apply_dirt), menos o arco dos limpadores."""
        gh = self.gh
        c = self.center
        win = self.s['windows']
        for w in win:
            seg, t0, t1, s0, s1 = w[:5]
            opened = w[5] if len(w) > 5 else 0.0
            fn = (lambda s, t, seg=seg: gh.point(s, seg, t))
            nt = max(3, int((t1 - t0) * {0: 6, 2: 12, 4: 6, 6: 12}.get(seg, 6)))
            if opened < 0.97:
                s1g = s0 + (s1 - s0) * (1.0 - opened)
                surf_slab(p, 'Lens', fn, s0, s1g, t0, t1, max(2, int(4 * (1.0 - opened)) + 1), nt, off=0.004 if opened < 0.05 else -0.02,
                          th=0.006, inward=c)
            # a borracha em volta (um pouco maior, rente); na janela aberta só o canal de baixo e as guias (o vão fica livre)
            if opened < 0.05:
                surf_slab(p, 'Dark#seal', fn, max(0.0, s0 - 0.04), min(1.0, s1 + 0.03), max(0.0, t0 - 0.015), min(1.0, t1 + 0.015),
                          4, nt, off=0.001, th=0.004, inward=c)
            else:
                surf_slab(p, 'Dark#seal', fn, max(0.0, s0 - 0.04), s0 + 0.03, max(0.0, t0 - 0.015), min(1.0, t1 + 0.015),
                          1, nt, off=0.001, th=0.006, inward=c)
                for (ta, tb) in ((max(0.0, t0 - 0.015), t0 + 0.012), (t1 - 0.012, min(1.0, t1 + 0.015))):
                    surf_slab(p, 'Dark#seal', fn, s0, min(1.0, s1 + 0.03), ta, tb, 3, 1, off=0.001, th=0.006, inward=c)

    def burnt_frame(self, p):
        """A estufa da carcaça: sem vidro, só o quadro (a cinta da cintura, as colunas, o trilho do teto) com espessura e o teto afundado."""
        gh = self.gh
        c = self.center
        m = lambda: None

        def slab(seg, t0, t1, s0, s1, ns=4, nt=3):
            fn = (lambda s, t, seg=seg: gh.point(s, seg, t))
            mid = gh.point((s0 + s1) / 2, seg, (t0 + t1) / 2)
            surf_slab(p, self.burn_mat(mid, self.s['zb']), fn, s0, s1, t0, t1, ns, nt, off=-0.035, th=0.035, inward=c)
        # as colunas: os cantos inteiros (A na frente, C atrás) e um pedaço do reto
        for seg in (1, 3, 5, 7):
            slab(seg, 0.0, 1.0, 0.0, 1.0, ns=6, nt=4)
        for seg in (0, 4):
            slab(seg, 0.0, 0.035, 0.0, 1.0, ns=6, nt=1)
            slab(seg, 0.965, 1.0, 0.0, 1.0, ns=6, nt=1)
        for (seg, ta, tb) in self.s.get('pillars', ((2, 0.47, 0.53), (6, 0.47, 0.53))):
            slab(seg, ta, tb, 0.0, 1.0, ns=6, nt=1)
        for seg in (2, 6):
            for (ta, tb) in ((0.0, 0.04), (0.92, 1.0)):
                slab(seg, ta, tb, 0.0, 1.0, ns=6, nt=1)
        # as cintas de baixo e de cima em volta
        for seg, n in enumerate([q[1] for q in SEGS]):
            slab(seg, 0.0, 1.0, 0.0, 0.07, ns=1, nt=max(1, n // 2))
            slab(seg, 0.0, 1.0, 0.93, 1.0, ns=1, nt=max(1, n // 2))
        # o teto afundado (a chapa cede no meio entre as colunas), com espessura
        rr = gh.roof_rings(sag=self.s.get('roof_sag', 0.09), noise=0.03, seed=self.seed)
        top = [ring_loop(gh.ring(1.0))] + rr
        self.loft_body(p, top, caps=(False, True), roof=True)
        inner = [[(x, y, z - 0.03) for (x, y, z) in r] for r in top]
        loft_mat(p, inner, lambda q: 'Char#burnt', closed=True, smooth=True, caps=(False, True))

    # -------------------------------------------------------------- frente e traseira
    def bumper(self, p, x, z0, z1, depth, wrap, m=None, y_half=None):
        """O para-choque varrido: o perfil arredondado (largura depth, de z0 a z1) correndo pela frente (x) e dobrando pelos cantos
        até 'wrap' pra trás, rente à lataria."""
        m = m or ('Dark#bump' if not self.burnt else 'Char#burnt')
        hw = y_half if y_half is not None else interp(self.body_stations(), x, 3) - 0.01
        sgn = 1 if x > 0 else -1
        h = z1 - z0
        prof = [(-depth * 0.2, z0 - (z0 + z1) / 2), (depth * 0.55, -h / 2 + 0.01), (depth * 0.8, -h / 2 + h * 0.2),
                (depth * 0.85, 0.0), (depth * 0.8, h / 2 - h * 0.2), (depth * 0.55, h / 2 - 0.005), (-depth * 0.2, h / 2)]
        prof = [(a, b) for (a, b) in prof]
        rcn = 0.16
        pts = []
        # de um lado ao outro, com a dobra dos cantos (o raio rcn) e o pedaço que corre pro lado
        for k in range(6, 0, -1):
            pts.append((x - sgn * (rcn + wrap * k / 6), -hw, 0.0))
        for k in range(7):
            a = math.radians(-90 + 90 * k / 6)
            pts.append((x - sgn * rcn + sgn * rcn * math.cos(a), -(hw - rcn) + rcn * math.sin(a) * 1.0, 0.0))
        for k in range(1, 6):
            pts.append((x, lerp(-(hw - rcn), hw - rcn, k / 6), 0.0))
        for k in range(7):
            a = math.radians(90 * k / 6)
            pts.append((x - sgn * rcn + sgn * rcn * math.cos(a), (hw - rcn) + rcn * math.sin(a), 0.0))
        for k in range(1, 7):
            pts.append((x - sgn * (rcn + wrap * k / 6), hw, 0.0))
        # o perfil: 'a' pra fora (normal em planta), 'b' na altura
        rings = []
        zm = (z0 + z1) / 2
        for i in range(len(pts)):
            P0 = Vector(pts[max(i - 1, 0)])
            P1 = Vector(pts[min(i + 1, len(pts) - 1)])
            t = (P1 - P0).normalized()
            nrm = Vector((t.y, -t.x, 0.0)) * (-sgn)
            if nrm.dot(Vector((sgn, 0, 0))) < -0.5 or (abs(pts[i][1]) > hw - 0.05 and nrm.y * pts[i][1] < 0):
                nrm = -nrm
            q = Vector(pts[i])
            rings.append([tuple(q + nrm * a + Vector((0, 0, zm + b))) for (a, b) in prof])
        p.loft(m, rings, bevel=0.0, smooth=True)

    def plate(self, p, x, z, sgn):
        if self.burnt:
            return
        vp.rbox(p, 'Metal#plate', (x + sgn * 0.012, 0.0, z), (0.02, 0.52, 0.12), r=0.012, k=2)
        vp.rbox(p, 'Dark', (x + sgn * 0.02, 0.0, z), (0.004, 0.44, 0.06), r=0.006, k=1)

    def lamp(self, p, x, y, z, w, h, sgn, lens='Lens#lamp', rim='Metal#chrome', depth=0.05, r=0.025):
        """O farol/lanterna: o aro e a lente embutida (a lente um pouco pra dentro do aro)."""
        if self.burnt:
            vp.rbox(p, 'Dark#hole', (x - sgn * 0.01, y, z), (0.03, w, h), pitch=0, r=r, k=2)
            return
        vp.rbox(p, rim, (x, y, z), (depth, w + 0.03, h + 0.03), r=r + 0.01, k=2)
        vp.rbox(p, lens, (x + sgn * depth * 0.42, y, z), (0.012, w, h), r=r, k=2)

    # -------------------------------------------------------------- rodas
    def wheel(self, p, x, side, R, tw, rim_r, style='rib', hubcap=True, rim_m='Metal#rim'):
        y = self.s['track'] * side
        if self.burnt:
            zc = rim_r + 0.012
            # o aro de aço nu (a borracha queimou): a aba, o disco rebaixado e o arame do talão
            o = tw / 2 * 0.84
            prof = [(0.0, o - 0.05), (rim_r * 0.35, o - 0.05), (rim_r * 0.75, o - 0.06), (rim_r * 0.92, o - 0.012),
                    (rim_r - 0.004, o), (rim_r + 0.014, o + 0.006), (rim_r + 0.016, o - 0.01), (rim_r - 0.006, o - 0.03),
                    (rim_r - 0.006, -o + 0.03), (rim_r + 0.016, -o + 0.01), (rim_r + 0.014, -o - 0.006), (rim_r - 0.004, -o),
                    (rim_r * 0.3, -o + 0.04), (0.0, -o + 0.04)]
            vp.lathe(p, 'Rust#rim', (x, y, zc), (0, side, 0), prof, seg=26, smooth=True)
            for s_ in (-1, 1):
                p.torus('Dark#bead', (x, y + side * 0.0 + s_ * o * 0.95, zc), 'y', rim_r + 0.02, 0.008, seg=24, rseg=4)
            vp.bolt_ring(p, 'Dark#nut', (x, y + side * (o - 0.045), zc), (0, side, 0), rim_r * 0.3, 4, r=0.016, h=0.016)
            return
        # o pneu de passeio: o flanco abaulado e o ombro redondo; a banda lisa (o friso não lê a 3 cm/px: 'style' None). O de carga
        # leva os blocos ('at')
        vp.tyre(p, (x, y, R), R, tw, side, rim_r=rim_r, rubber='Rubber', rim=rim_m, hub='Metal#hub', nut_m='Metal#hub', seg=24,
                lugs=style is not None, lug_h=0.016, nuts=4 if not hubcap else 0, holes_m=None, dish=0.03, style=style or 'at',
                n_lugs=None)
        if hubcap:
            # a calota: o disco abaulado com os raios em relevo (lê como roda de carro de passeio, não de caminhão)
            o = tw / 2 * 0.84 + 0.004
            prof = [(0.0, o + 0.032), (rim_r * 0.25, o + 0.03), (rim_r * 0.55, o + 0.022), (rim_r * 0.85, o + 0.01),
                    (rim_r * 0.98, o - 0.004), (rim_r * 0.98, o - 0.02), (0.0, o - 0.02)]
            vp.lathe(p, 'Metal#cap', (x, y, R), (0, side, 0), prof, seg=28, smooth=True)
            for k in range(8):
                a = 2 * math.pi * k / 8 + 0.2
                q = Vector((x, y, R)) + Vector((math.cos(a), 0, math.sin(a))) * rim_r * 0.62 + Vector((0, side * (o + 0.018), 0))
                vp.rbox(p, 'Dark#slot', tuple(q), (rim_r * 0.28, 0.01, 0.035), pitch=-math.degrees(a), r=0.012, k=1)

    def wheels(self, p):
        s = self.s
        for i, ax in enumerate(self.axles):
            for side in (-1, 1):
                dual = s.get('dual_rear') and i == 1
                if dual:
                    for k in (-1, 1):
                        yy = s['track'] + k * (s['tw'] / 2 + 0.012)
                        sv = dict(s)
                        old = s['track']
                        s['track'] = yy
                        self.wheel(p, ax, side, s['R'], s['tw'], s['rim_r'], style=s.get('tyre_style', 'at'),
                                   hubcap=False, rim_m='Metal#rim')
                        s['track'] = old
                else:
                    # (v3) a calota que caiu numa roda (o aro de aço com as porcas à mostra)
                    hc = s.get('hubcap', True) and (i, side) not in [tuple(q) for q in s.get('hubcap_missing', ())]
                    self.wheel(p, ax, side, s['R'], s['tw'], s['rim_r'], style=s.get('tyre_style', 'rib'),
                               hubcap=hc, rim_m=s.get('rim_m', 'Metal#rim'))

    def underbody(self, p, x0, x1, z0, z1, hw):
        """O chassi escuro embaixo e as caixas de roda (a meia-lua escura que fecha o vão por dentro)."""
        s = self.s
        m = 'Dark#under' if not self.burnt else 'Char#burnt'
        inner = s['track'] - s['tw'] / 2 - 0.03
        p.box(m, x1, x0, -inner, inner, z0, z1, bevel=0.0)
        for ax in self.axles:
            for side in (-1, 1):
                ra = s['arch_r'] - 0.006
                y0 = side * (s['track'] - s['tw'] / 2 - 0.05)
                y1 = side * (s.get('liner_out', 0.0) or (interp(self.body_stations(), ax, 3) - 0.015))
                arch_liner(p, 'Dark#liner' if not self.burnt else 'Char#burnt', ax, self.zc + s.get('arch_dz', 0.02), ra,
                           min(y0, y1), max(y0, y1))

    # -------------------------------------------------------------- miudezas da lataria
    def interior(self, p, x_front, x_rear, z_floor, rows=2, seat_m='Cloth#seat', cover=None):
        """(v3) O interior do carro inteiro (aparece pela janela aberta e de cima, na câmera do jogo): o piso, os forros de porta (sem
        eles a lata de lá não tem lado de dentro), o painel com a capota dos relógios, o volante e a coluna, os bancos estofados com
        o encosto inclinado e o apoio de cabeça, o banco de trás inteiriço e a tampa do porta-malas; cover = a capa de pano jogada
        num banco (a manta de lã da região)."""
        gh = self.gh
        z_belt = gh.keys[0][1]
        hw = gh.keys[0][4] - 0.07
        xd = x_front
        p.box('Dark#floor', x_rear, x_front, -hw, hw, z_floor - 0.04, z_floor, bevel=0.0)
        for side in (-1, 1):
            ya, yb = sorted((side * hw, side * (hw + 0.035)))
            p.box('Dark#card', x_rear + 0.04, x_front - 0.05, ya, yb, z_floor, z_belt - 0.02, bevel=0.0)
            vp.rbox(p, 'Dark#card', ((x_front + x_rear) / 2, side * (hw - 0.03), z_belt - 0.1), (x_front - x_rear - 0.2, 0.08, 0.05), r=0.02, k=1)
        # o painel e a capota dos relógios (o motorista à esquerda, −y)
        vp.rbox(p, 'Dark#dash', (xd - 0.20, 0.0, z_belt - 0.07), (0.34, 2 * hw - 0.02, 0.2), r=0.07, k=2)
        vp.rbox(p, 'Dark#dash', (xd - 0.28, -0.36, z_belt + 0.06), (0.16, 0.38, 0.09), r=0.04, k=2)
        vp.rbox(p, 'Dark#dash', (xd - 0.30, 0.0, z_belt - 0.20), (0.22, 0.30, 0.34), r=0.05, k=2)          # o console do meio
        wc = Vector((xd - 0.42, -0.36, z_belt + 0.02))
        vp.lathe(p, 'Dark#wheel', tuple(wc), (0.72, 0.0, 0.69), [(0.165, -0.014), (0.185, 0.0), (0.165, 0.014), (0.15, 0.0)], seg=20,
                 closed=True)
        vp.tube(p, 'Dark#wheel', [tuple(wc), (xd - 0.24, -0.36, z_belt - 0.12)], 0.025, seg=6)
        for a in (90.0, 210.0, 330.0):
            q = wc + Vector((-0.69 * math.sin(math.radians(a)), math.cos(math.radians(a)), 0.72 * math.sin(math.radians(a)))) * 0.15
            vp.tube(p, 'Dark#wheel', [tuple(wc), tuple(q)], 0.012, seg=4)
        vp.tube(p, 'Dark#lever', [(xd - 0.45, 0.0, z_floor + 0.32), (xd - 0.52, 0.0, z_floor + 0.52)], 0.012, seg=5)
        zf = z_floor
        # a altura que cabe (o apoio de cabeça a ~10 cm do teto)
        H = gh.keys[-1][1] - zf - 0.10

        def seat(sx, sy, w, m, back=0.55, tilt=-14.0, head=True):
            back = min(back, H - 0.40)
            vp.rbox(p, m, (sx, sy, zf + 0.16), (0.50, w, 0.13), r=0.05, k=2)
            vp.rbox(p, m, (sx - 0.28, sy, zf + 0.22 + back / 2), (0.14, w, back), pitch=tilt, r=0.05, k=2)
            if head:
                vp.rbox(p, m, (sx - 0.28 - back * 0.25, sy, zf + 0.22 + back + 0.08), (0.10, min(w, 0.28), 0.14), pitch=tilt, r=0.04, k=2)
        fs = xd - 0.80
        front = [(-0.37, 0.44), (0.37, 0.44)] if rows != 1 else [(0.0, 2 * hw - 0.08)]
        for i, (sy, w) in enumerate(front):
            seat(fs, sy, w, cover if (cover and i == 0) else seat_m, head=rows != 1)
        if rows >= 2 and fs - 0.85 > x_rear + 0.2:
            seat(fs - 0.85, 0.0, 2 * hw - 0.12, seat_m, back=0.55, tilt=-18.0, head=False)
            if rows >= 3 and fs - 1.7 > x_rear + 0.2:
                seat(fs - 1.7, 0.0, 2 * hw - 0.12, seat_m, back=0.55, tilt=-16.0, head=False)

    def round_lamp(self, p, x, y, z, r, sgn):
        """(v3) O farol redondo do hatch (o Golf, o 205, o Kia Pride): o aro cromado com a borda, a lente abaulada um pouco pra dentro e o
        refletor; na carcaça só o buraco escuro."""
        A = (sgn, 0.0, 0.0)
        if self.burnt:
            vp.lathe(p, 'Dark#hole', (x - sgn * 0.02, y, z), A, [(0.0, 0.0), (r, 0.0), (r, 0.03), (0.0, 0.03)], seg=18)
            return
        vp.lathe(p, 'Metal#chrome', (x - sgn * 0.05, y, z), A, [(0.0, 0.0), (r + 0.022, 0.0), (r + 0.026, 0.035), (r + 0.016, 0.055),
                                                                (r + 0.004, 0.05), (r + 0.002, 0.03), (0.0, 0.03)], seg=24, smooth=True)
        vp.lathe(p, 'Lens#head', (x - sgn * 0.022, y, z), A, [(0.0, 0.0), (r, 0.0), (r * 0.96, 0.018), (r * 0.6, 0.03), (0.0, 0.034)],
                 seg=24, smooth=True)

    def mirror(self, p, x, z, side):
        hw = self.side_y(x, z - 0.06)
        y = side * (hw + 0.075)
        m = 'Dark#mirror' if not self.burnt else 'Char#burnt'
        vp.rbox(p, m, (x, side * (hw - 0.005), z - 0.02), (0.1, 0.04, 0.07), r=0.015, k=2)          # o pé na porta
        if self.s['details'].get('mirror_missing') == side:
            # (v3) o retrovisor arrancado: só o pé com o toco do braço
            vp.tube(p, m, [(x - 0.01, side * (hw + 0.01), z - 0.01), (x - 0.015, side * (hw + 0.03), z + 0.005)], 0.012, seg=6)
            return
        vp.tube(p, m, [(x - 0.01, side * (hw + 0.01), z - 0.01), (x - 0.03, side * (hw + 0.05), z + 0.03)], 0.014, seg=6)
        vp.rbox(p, m, (x - 0.035, y, z + 0.05), (0.07, 0.15, 0.1), r=0.035, k=3)
        if not self.burnt:
            vp.rbox(p, 'Lens#mir', (x - 0.072, y, z + 0.05), (0.006, 0.12, 0.075), r=0.03, k=2)

    def handle(self, p, x, z, side):
        if self.burnt:
            return
        hw = self.side_y(x, z)
        vp.rbox(p, 'Dark#handle', (x, side * (hw + 0.006), z), (0.12, 0.018, 0.03), r=0.012, k=2)

    def seam_v(self, p, x, z0, z1, side, m='Dark#seam'):
        """A junta da porta (o friso escuro de 7 mm, rente à lateral, seguindo a barriga)."""
        if self.burnt:
            return
        pts = []
        for k in range(7):
            z = lerp(z0, z1, k / 6)
            pts.append((x, side * (self.side_y(x, z) + 0.0015), z))
        vp.tube(p, m, pts, 0.0045, seg=4)

    def seam_line(self, p, pts, r=0.0045, m='Dark#seam'):
        if self.burnt:
            return
        vp.tube(p, m, pts, r, seg=4)

    def trim_strip(self, p, x0, x1, z, side, m='Dark#trim', h=0.035):
        """O friso de borracha da porta (a faixa que corre na lateral: a linha que lê de longe)."""
        if self.burnt:
            return
        pts = []
        n = max(2, int(abs(x0 - x1) / 0.12))
        for k in range(n + 1):
            x = lerp(x0, x1, k / n)
            pts.append((x, side * (self.side_y(x, z) + 0.004), z))
        b = p.bm(m, 0.0)
        rings = []
        for (x, y, zz) in pts:
            rings.append([(x, y - side * 0.006, zz - h / 2), (x, y + side * 0.004, zz - h / 2 + 0.006), (x, y + side * 0.004, zz + h / 2 - 0.006),
                          (x, y - side * 0.006, zz + h / 2)])
        p.loft(m, rings, bevel=0.0, smooth=True)

    # -------------------------------------------------------------- o interior da carcaça (aparece sem vidro)
    def burnt_interior(self, p, x_front, x_rear, z_floor, rows=2):
        """Os bancos queimados: a armação de tubo e as molas (o estofado virou cinza), o volante, o painel em carvão e o piso."""
        gh = self.gh
        z_belt = gh.keys[0][1]
        hw = gh.keys[0][4] - 0.08
        p.box('Char#burnt', x_rear, x_front, -hw, hw, z_floor - 0.04, z_floor, bevel=0.0)
        # o painel (o carvão com a borda torta)
        xd = x_front
        vp.rbox(p, 'Char#burnt', (xd - 0.12, 0.0, z_belt - 0.08), (0.3, 2 * hw - 0.04, 0.2), r=0.06, k=2)
        vp.lathe(p, 'Dark#wheel', (xd - 0.33, -0.36, z_belt + 0.03), (0.75, 0.0, 0.66),
                 [(0.17, -0.012), (0.19, 0.0), (0.17, 0.012)], seg=20, closed=True)
        seats = [(xd - 0.75, -0.36), (xd - 0.75, 0.36)]
        if rows > 1:
            seats += [(xd - 1.55, -0.38), (xd - 1.55, 0.0), (xd - 1.55, 0.38)]
        for (sx, sy) in seats[:(2 if rows == 1 else 5)]:
            if sx < x_rear + 0.25:
                continue
            w = 0.46 if abs(sy) > 0.1 or rows == 1 else 0.36
            zf = z_floor
            # o assento: o quadro e as molas em zigue-zague
            vp.tube(p, 'Rust#frame', vp.bent_bar([(sx + 0.24, sy - w / 2, zf + 0.30), (sx - 0.22, sy - w / 2, zf + 0.28),
                                                  (sx - 0.22, sy + w / 2, zf + 0.28), (sx + 0.24, sy + w / 2, zf + 0.30),
                                                  (sx + 0.24, sy - w / 2, zf + 0.30)], R=0.04, n=2), 0.014, seg=6)
            for k in range(5):
                yy = sy - w / 2 + w * (k + 0.5) / 5
                zz = [(sx + 0.22 - 0.44 * i / 6, yy + (0.025 if i % 2 else -0.025), zf + 0.27 - 0.02 * math.sin(math.pi * i / 6)) for i in range(7)]
                vp.tube(p, 'Rust#spring', zz, 0.007, seg=4)
            # o encosto (a armação inclinada pra trás)
            top = (sx - 0.36, sy, zf + 0.85)
            vp.tube(p, 'Rust#frame', vp.bent_bar([(sx - 0.22, sy - w / 2, zf + 0.28), (top[0], sy - w / 2, top[2]), (top[0] - 0.02, sy + w / 2, top[2]),
                                                  (sx - 0.22, sy + w / 2, zf + 0.28)], R=0.05, n=2), 0.013, seg=6)
            for k in range(3):
                zz = zf + 0.4 + 0.15 * k
                xx = lerp(sx - 0.22, top[0], (zz - zf - 0.28) / 0.57)
                vp.tube(p, 'Rust#spring', [(xx, sy - w / 2 + 0.03, zz), (xx - 0.01, sy, zz - 0.02), (xx, sy + w / 2 - 0.03, zz)], 0.006, seg=4)
            # o que sobrou do estofado: a crosta de carvão rasa no fundo do assento
            vp.rbox(p, 'Char#ash', (sx + 0.0, sy, zf + 0.22), (0.34, w * 0.75, 0.03), r=0.08, k=2)

    # -------------------------------------------------------------- montagem
    def finish_part(self, p, name):
        o = p.finish()
        o.name = name
        return o


def arch_liner(p, m, x, zc, r, y0, y1, a0=-12.0, a1=192.0, t=0.02, seg=20):
    """A caixa de roda: a casca em meio cilindro por cima da roda (de a0 a a1 graus no plano XZ, 0 = pra frente), de y0 a y1, com
    espessura t pra fora; fecha o vão do arco por dentro (o escuro que a roda tem atrás de si)."""
    rings = []
    for k in range(seg + 1):
        a = math.radians(a0 + (a1 - a0) * k / seg)
        ca, sa = math.cos(a), math.sin(a)
        rings.append([(x + r * ca, y0, zc + r * sa), (x + r * ca, y1, zc + r * sa),
                      (x + (r + t) * ca, y1, zc + (r + t) * sa), (x + (r + t) * ca, y0, zc + (r + t) * sa)])
    p.loft(m, rings, bevel=0.0, smooth=True)


def burnt_drop(o, drop, roll=0.0, pitch=0.0):
    """Assenta a carcaça nos aros: desce 'drop' e inclina (roll em graus, de lado; pitch, frente)."""
    M = (Matrix.Translation((0, 0, -drop)) @ Matrix.Rotation(math.radians(roll), 4, 'X')
         @ Matrix.Rotation(math.radians(-pitch), 4, 'Y'))
    o.data.transform(M)
    o.data.update()


def debris_ring(p, cx, cy, r0, r1, n, seed, mats=('Char#burnt', 'Rust#burnt', 'Lens#shard')):
    """Os cacos e lascas em volta da carcaça (o vidro estourado no chão e os pedaços de lata), pequenos e chatos."""
    for k in range(n):
        a = 2 * math.pi * h3(k, seed, 3)
        r = lerp(r0, r1, h3(k, seed, 5) ** 0.7)
        x, y = cx + r * math.cos(a), cy + r * math.sin(a)
        m = mats[int(h3(k, seed, 7) * len(mats)) % len(mats)]
        sz = 0.03 + 0.07 * h3(k, seed, 11) ** 2
        vp.rbox(p, m, (x, y, 0.005), (sz, sz * (0.5 + 0.5 * h3(k, seed, 13)), 0.01), yaw=360 * h3(k, seed, 17),
                r=sz * 0.2, k=1)


# ===================================================================================================== carga e caçamba
def bag(p, c, size, yaw=0.0, pitch=0.0, roll=0.0, m='Cloth#bag', seed=0.0, pillow=False):
    """O saco cheio (juta, cimento, farinha): a seção superelipse, estufado no meio e murcho nas pontas amarradas, o fundo achatado
    no apoio (a cópia do saco do elenco, model_gla_supply_pickup.bag, com a semente)."""
    L, W, H = size
    hh = h1(c[0] * 12.9898 + c[1] * 78.233 + c[2] * 37.719 + seed)
    L *= 0.94 + 0.12 * hh
    H *= 0.9 + 0.2 * ((hh * 7.31) % 1.0)
    ex, ey, ez = vp.ypr_frame(yaw, pitch, roll)
    C = Vector(c)
    rings = []
    for i in range(11):
        u = i / 10
        bulge = math.sin(math.pi * u) ** (0.18 if pillow else 0.45)
        w = W / 2 * ((0.72 + 0.28 * bulge) if pillow else (0.42 + 0.58 * bulge))      # o saco de areia: o travesseiro de pontas cheias
        t = H / 2 * (0.25 + 0.75 * math.sin(math.pi * u) ** 0.7)
        x = L * (u - 0.5) * (1.0 - 0.06 * math.cos(math.pi * u) ** 2)
        ring = []
        for k in range(12):
            th = 2 * math.pi * k / 12
            ct, st = math.cos(th), math.sin(th)
            yy = w * math.copysign(abs(ct) ** 0.7, ct)
            zz = t * math.copysign(abs(st) ** 0.7, st)
            if zz < 0:
                zz *= 0.55
            ring.append(tuple(C + ex * x + ey * yy + ez * (zz + 0.12 * H * (1 - bulge) + H * 0.5 * 0.55)))
        rings.append(ring)
    p.loft(m, rings, bevel=0.0, smooth=True)


def bed_wall(car, p, x0, x1, hw, top, sill, t=0.05, m=None, arch=True):
    """A lateral da caçamba (y > 0 se hw > 0): o loft da seção (a chapa de t com a barriga leve por fora, o friso de cima redondo
    que sai pra fora) ao longo do X, com o pé cortado pelo arco da roda de trás."""
    s = car.s
    sg = 1 if hw > 0 else -1
    H = abs(hw)
    xs = body_xs(x0, x1, car.axles, s['arch_r'])
    rings = []
    for x in xs:
        zb = sill
        if arch:
            a = arch_z(x, car.axles, s['arch_r'], car.zc + s.get('arch_dz', 0.02))
            if a > zb:
                zb = min(a, top - 0.08)
        prof = [(H, zb), (H + 0.004, lerp(zb, top, 0.5)), (H, top - 0.06), (H + 0.012, top - 0.035), (H + 0.01, top - 0.01),
                (H - 0.01, top + 0.006), (H - t - 0.025, top + 0.004), (H - t - 0.03, top - 0.02), (H - t, top - 0.04), (H - t, zb + 0.01)]
        rings.append([car.dent((x, sg * y, z)) for (y, z) in prof])
    if m is None:
        car.loft_body(p, rings, caps=(True, True))
    else:
        loft_mat(p, rings, lambda c: m, closed=True, smooth=True, caps=(True, True))


def roof_rack(p, x0, x1, hw, z_roof, h=0.10, m='Dark#rack', bars=5, r=0.018):
    """O bagageiro de teto de tubo (o da van e do hatch: as 2 longarinas com os pés, as travessas, o arco de proteção na frente)."""
    zr = z_roof + h
    for sg in (-1, 1):
        y = sg * hw
        vp.tube(p, m, vp.bent_bar([(x1 + 0.05, y, z_roof), (x1, y, zr), (x0, y, zr), (x0 - 0.05, y, z_roof)], R=0.06, n=3), r, seg=8)
        for k in range(1, 4):
            x = lerp(x1, x0, k / 4)
            vp.tube(p, m, [(x, y, z_roof - 0.01), (x, y, zr)], r * 0.9, seg=6)
    for k in range(bars):
        x = lerp(x1 + 0.08, x0 - 0.08, k / (bars - 1))
        vp.tube(p, m, [(x, -hw, zr), (x, hw, zr)], r * 0.85, seg=6)


def rope_over(p, a, b, z_top, m='Wood#rope', r=0.012):
    """A corda que passa por cima da carga de a até b (as pontas presas), com a barriga."""
    A, B = Vector(a), Vector(b)
    pts = [A, A + (B - A) * 0.15 + Vector((0, 0, z_top - A.z)), A + (B - A) * 0.5 + Vector((0, 0, z_top - A.z + 0.02)),
           A + (B - A) * 0.85 + Vector((0, 0, z_top - A.z)), B]
    vp.tube(p, m, vp.bent_bar(pts, R=0.08, n=3), r, seg=6)


def carpet_roll(p, c, axis, length, r, m='Rust#rug', band_m='Cloth#rug'):
    """O tapete enrolado (a carga mais vista nos tetos e caçambas da região): o rolo com a espiral da ponta e 2 amarras."""
    A, E1, E2 = vp.axis_frame(axis)
    C = Vector(c)
    vp.lathe(p, m, tuple(C), tuple(A), [(0.0, -length / 2), (r * 0.6, -length / 2 - 0.004), (r * 0.97, -length / 2),
                                         (r, -length / 2 + 0.03), (r, length / 2 - 0.03), (r * 0.97, length / 2),
                                         (r * 0.6, length / 2 + 0.004), (0.0, length / 2)], seg=14, smooth=True)
    for u in (-0.3, 0.3):
        vp.lathe(p, band_m, tuple(C + A * (u * length)), tuple(A), [(r + 0.004, -0.025), (r + 0.012, 0.0), (r + 0.004, 0.025)],
                 seg=14, smooth=True, closed=True)


# ===================================================================================================== o carro de passeio inteiro
def passenger_car(spec, burnt, name, extra=None, seed=3):
    """Monta um carro pela spec (o sedã, o hatch, a picape, a van usam): a lataria, a estufa, o chassi, a frente (faróis, grade,
    setas, para-choque, placa), a traseira, as juntas e frisos, maçanetas, retrovisores, limpadores, antena e tampa do tanque; na
    carcaça, o interior queimado, o para-choque da frente pendurado, os aros e os cacos. extra(car, p) acrescenta o que é só do
    modelo (a caçamba, o bagageiro, a carga). Devolve (o objeto final com a frente em +Y no padrão do kit, o Car)."""
    from model_lib import Part
    s = spec
    d = s['details']
    car = Car(s, burnt=burnt, seed=seed)
    p = Part(name)
    car.lower_body(p, x1=d.get('body_x1'))
    car.greenhouse(p)
    ub = d.get('under', (s['FA'] + 0.55, s['RA'] - 0.6, 0.17, s['zb'] + 0.03))
    car.underbody(p, ub[0], ub[1], ub[2], ub[3], 0.85)
    xf = car.xf
    xr = d.get('rear_x', car.xr)
    # ---- a frente
    bf = d['bump_f']
    br = d['bump_r']
    # (v3) o para-choque caído de um lado (preso só numa ponta): no inteiro pela spec ('bump_hang': (ponta, lado do pivô, graus)); na
    # carcaça o da frente sempre
    hang = ('f', 1, 11.0) if burnt else d.get('bump_hang')
    car.hang_objs = []
    for end, (x_b, b_) in (('f', (xf + 0.02, bf)), ('r', (xr - 0.02, br))):
        m_b = 'Char#burnt' if burnt else d.get('bump_m')
        if hang and hang[0] == end:
            q = Part('bump_' + end)
            car.bumper(q, x_b, b_[0], b_[1], b_[2], b_[3], m=m_b)
            bo = q.finish()
            sgp = hang[1]
            piv = V(x_b, 0.78 * sgp, (b_[0] + b_[1]) / 2)
            bo.data.transform(Matrix.Translation(piv) @ Matrix.Rotation(math.radians(-hang[2] * sgp), 4, 'X') @ Matrix.Translation(-piv))
            car.hang_objs.append(bo)
        else:
            car.bumper(p, x_b, b_[0], b_[1], b_[2], b_[3], m=m_b)
    hy, hz, hwd, hh = d['head']
    front = d.get('front', 'rect')
    for sg in (-1, 1):
        if front == 'round':
            car.round_lamp(p, xf - d.get('head_in', 0.012), hy * sg, hz, d.get('head_round', 0.085), 1)
        else:
            car.lamp(p, xf - d.get('head_in', 0.012), hy * sg, hz, hwd, hh, 1, lens='Lens#head', rim='Metal#chrome', depth=0.06,
                     r=d.get('head_r', 0.02))
        if not burnt and d.get('sig'):
            sy, sz = d['sig']
            vp.rbox(p, 'Yellow#sig', (xf - 0.03, sy * sg, sz), (0.05, 0.07, 0.09), yaw=-20 * sg, r=0.02, k=2)
    gw, gh_, gz, gbars = d['grille']
    if not burnt and gw > 0:
        vp.rbox(p, 'Dark#grille', (xf - 0.03, 0.0, gz), (0.05, gw, gh_), r=0.02, k=2)
        if d.get('grille_v'):
            # (v3) a grade de lâminas em pé no quadro cromado (o sedã: a cara do W124 / 405), o friso de cima mais grosso
            vp.rbox(p, 'Metal#chrome', (xf - 0.006, 0.0, gz + gh_ / 2 - 0.01), (0.03, gw + 0.02, 0.035), r=0.012, k=2)
            vp.rbox(p, 'Metal#chrome', (xf - 0.006, 0.0, gz - gh_ / 2 + 0.008), (0.03, gw + 0.02, 0.02), r=0.008, k=2)
            for sgy in (-1, 1):
                vp.rbox(p, 'Metal#chrome', (xf - 0.006, sgy * (gw / 2 + 0.004), gz), (0.03, 0.025, gh_), r=0.01, k=2)
            n = gbars
            for k in range(1, n):
                yy = -gw / 2 + gw * k / n
                vp.rbox(p, 'Metal#slat', (xf - 0.012, yy, gz), (0.035, 0.012, gh_ - 0.04), r=0.004, k=1)
        else:
            for k in range(gbars):
                z = gz - gh_ / 2 + gh_ * (k + 0.5) / gbars
                vp.tube(p, d.get('grille_bar', 'Metal#chrome'), [(xf - 0.004, -gw / 2 + 0.02, z), (xf + 0.004, 0.0, z),
                                                                 (xf - 0.004, gw / 2 - 0.02, z)], 0.008, seg=5)
            vp.rbox(p, 'Metal#chrome', (xf - 0.004, 0.0, gz), (0.02, 0.09, 0.08), r=0.03, k=3)
    if d.get('intake'):
        # (v3) a tomada de ar no para-choque (a boca escura de baixo do hatch)
        iw, iz = d['intake']
        vp.rbox(p, 'Dark#intake', (xf + bf[2] * 0.75, 0.0, iz), (0.06, iw, 0.06), r=0.02, k=2)
    car.plate(p, xf + bf[2] * 0.9, (bf[0] + bf[1]) / 2 - 0.01, 1)
    # ---- a traseira
    ty, tz, twd, th_ = d['tail']
    for sg in (-1, 1):
        car.lamp(p, xr + 0.012, ty * sg, tz, twd, th_, -1, lens='Rust#tail', rim='Dark#tailrim', depth=0.05, r=0.02)
    if d.get('tail_strip') and not burnt:
        vp.rbox(p, 'Dark#tailstrip', (xr + 0.02, 0.0, tz), (0.04, 2 * ty - twd - 0.04, 0.07), r=0.02, k=2)
    car.plate(p, xr - 0.008, d.get('plate_r_z', br[1] + 0.12), -1)
    vp.tube(p, 'Dark#exhaust' if not burnt else 'Rust#burnt', [(xr + 0.6, 0.45, s['zb'] - 0.06), (xr - 0.04, 0.45, s['zb'] - 0.06)],
            0.028, seg=10)
    # ---- as juntas
    zt = car.top_z
    hood = d.get('hood')
    if hood:
        hx0, hx1, hy_ = hood
        car.seam_line(p, [(hx0, y, zt(hx0, y) + 0.002) for y in [lerp(-hy_, hy_, k / 15) for k in range(16)]])
        for sg in (-1, 1):
            car.seam_line(p, [(x, hy_ * sg, zt(x, hy_) + 0.002) for x in [lerp(hx0, hx1, k / 12) for k in range(13)]])
    trunk = d.get('trunk')
    if trunk:
        tx0, tx1, ty_ = trunk
        car.seam_line(p, [(tx0, y, zt(tx0, y) + 0.002) for y in [lerp(-ty_, ty_, k / 15) for k in range(16)]])
        for sg in (-1, 1):
            car.seam_line(p, [(x, ty_ * sg, zt(x, ty_) + 0.002) for x in [lerp(tx0, tx1, k / 6) for k in range(7)]])
    for side in (-1, 1):
        for (x, z0, z1) in d.get('door_v', ()):
            car.seam_v(p, x, z0, z1, side)
        for pts in d.get('door_lines', ()):
            car.seam_line(p, [(x, side * (car.side_y(x, z) + 0.0015), z) for (x, z) in pts])
        for (x0, x1, z) in d.get('trim', ()):
            car.trim_strip(p, x0, x1, z, side)
        for (x, z) in d.get('handles', ()):
            car.handle(p, x, z, side)
        mx, mz = d['mirror']
        car.mirror(p, mx, mz, side)
    if not burnt:
        fx, fz = d.get('fuel', (None, None))
        if fx is not None:
            y = car.side_y(fx, fz)
            vp.lathe(p, s.get('paint', 'Paint') + '#cap', (fx, y, fz), (0, 1, 0), [(0.0, 0.006), (0.06, 0.004), (0.065, -0.004),
                                                                                    (0.0, -0.004)], seg=16)
        if d.get('antenna'):
            ax_, ay_, az_ = d['antenna']
            vp.tube(p, 'Dark#ant', [(ax_, ay_, az_), (ax_ - 0.03, ay_, az_ + 0.68)], 0.004, seg=4)
        for t0 in d.get('wipers', (0.12, 0.52)):
            a, b = car.gh.point(0.2, 0, t0), car.gh.point(0.25, 0, t0 + 0.36)
            vp.tube(p, 'Dark#wiper', [tuple(a + Vector((0.012, 0, 0.012))), tuple(b + Vector((0.012, 0, 0.012)))], 0.008, seg=4)
            c = car.gh.point(0.08, 0, t0 + 0.05)
            vp.tube(p, 'Dark#wiper', [tuple(c + Vector((0.01, 0, 0.0))), tuple(a + Vector((0.012, 0, 0.012)))], 0.01, seg=4)
        ir = d.get('interior', (s['gh'][0][2] - 0.08, s['gh'][0][3] + 0.1, s['zb'] + 0.06, 2))
        car.interior(p, ir[0], ir[1], ir[2], rows=ir[3], seat_m=d.get('seat_m', 'Cloth#seat'), cover=d.get('seat_cover'))
    else:
        ir = d.get('interior', (s['gh'][0][2] - 0.08, s['gh'][0][3] + 0.1, s['zb'] + 0.06, 2))
        car.burnt_interior(p, ir[0], ir[1], ir[2], rows=ir[3])
    if extra:
        extra(car, p)
    o = p.finish()
    if getattr(car, 'hang_objs', None):
        o = join([o] + car.hang_objs, 'body')
    w = Part('wheels')
    car.wheels(w)
    wo = w.finish()
    objs = [o, wo]
    if burnt:
        burnt_drop(o, s['R'] - s['rim_r'] - 0.012, roll=1.8, pitch=-0.8)
        dp = Part('debris')
        debris_ring(dp, 0.0, 0.0, s['L'] * 0.25, s['L'] * 0.45, 30, seed + 2)
        objs.append(dp.finish())
    o = join(objs, name)
    to_kit(o)
    # (v3) a spec da sujeira (o gancho do export a aplica depois das máscaras): o pó da estrada, o leque atrás das rodas, o vidro sujo
    # com o arco dos limpadores
    import json
    gh = car.gh
    base = gh.point(0.08, 0, 0.5)
    top = gh.point(0.92, 0, 0.5)
    Lw = 0.82 * (top - base).length
    pivots = [(gh.point(0.08, 0, t0 + 0.05).y, Lw) for t0 in d.get('wipers', (0.12, 0.52))]
    irr = d.get('interior', (s['gh'][0][2] - 0.08, s['gh'][0][3] + 0.1, s['zb'] + 0.06, 2))
    cab = [irr[1] - 0.05, irr[0] + 0.05, s['gh'][0][4] - 0.04, irr[2] - 0.08, s['gh'][-1][1] - 0.02]
    o['civ_dirt'] = json.dumps({'frame': 'car', 'cab': cab, 'zb': s['zb'], 'zc': car.zc, 'axles': list(car.axles), 'arch_r': s['arch_r'],
                                'track': s['track'], 'dust': s.get('dust', 0.85), 'dust_h': s.get('dust_h', 0.55), 'burnt': burnt,
                                'ws': {'x': base.x, 'z': base.z, 'pivots': pivots}})
    return o, car
