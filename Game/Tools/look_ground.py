"""O chão em relevo da maquete do look (LookLab): malha de verdade e a planta pintada.

Rodar (Python do sistema, numpy + pillow; o look.ps1 ground chama):
  python Game/Tools/look_ground.py <pasta de saída>

Nas quatro cenas de docs/concept/scenes/desert_golden_hour/best/ a estrada não é uma pintura
no chão: o asfalto é uma capa mais alta que a areia, com a borda quebrada; a areia sobe por
cima dela em montes; encostada nas paredes ela se amontoa; a estrada de terra tem sulcos.
Com o sol rasante cada degrau desses acende de um lado e faz sombra do outro.

O que sai:
- SM_LL_Ground.glb: a malha do chão (grade de 20 cm, ~1,3 milhão de vértices; a Unreal
  liga o Nanite), da região que as câmeras veem, com a borda descendo pro plano de fora;
- T_LL_Layout.png: a planta (4096², ~5,6 cm por pixel) que o material lê pela posição:
  R = asfalto, G = areia por cima (montes e encostas), B = estrada de terra, A = faixa pintada.

As alturas (metros; o topo do asfalto é o zero, onde os tanques e as casas pousam):
- asfalto: 0, com as placas rachadas inclinadas ±1 cm (a altura da textura de rachadura);
- areia: 6 cm abaixo do asfalto perto da rua, ondulando até ±35 cm longe dela (nunca abaixo de −40 cm);
- borda do asfalto: quebrada por ruído (a capa se esfarela em pedaços de 30 cm a 2 m);
- montes de areia nas bordas da rua (até 26 cm, de 1 a 4 m de largura, em trechos) e
  encostados nas paredes (até 35 cm, caindo em ~1 m);
- estrada de terra: dois sulcos de 4 cm a 1,2 m do meio.
"""
import json
import math
import os
import struct
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from look_scene import ROAD, TANKS, PALMS, SHRUBS, POLES, HOUSES, footprints, GROUND_REGION, SUFFIX  # noqa: E402
import look_scene  # noqa: E402
SCATTER_KINDS = set(getattr(look_scene, 'SCATTER', {}).keys())

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.getcwd(), 'ground')
CRACKS = os.path.join(os.path.dirname(HERE), '..', 'captures', 'look', 'textures', 'T_LL_Cracks.png')
os.makedirs(OUT, exist_ok=True)

# a região (o que as três câmeras veem do chão) e a resolução
X0, Y0, SIZE = GROUND_REGION
MESH_STEP = 0.2
TEX = 4096
OUTSIDE_Z = -0.45            # o plano de fora fica 45 cm abaixo (some sob a borda)


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def value_noise(x, y, scale, seed):
    """Ruído suave (interpolação de uma grade aleatória), 0..1, em metros."""
    r = np.random.default_rng(seed)
    n = int(SIZE / scale) + 4
    g = r.random((n, n)).astype(np.float32)
    fx, fy = (x - X0) / scale + 1, (y - Y0) / scale + 1
    ix, iy = np.floor(fx).astype(int), np.floor(fy).astype(int)
    tx, ty = fx - ix, fy - iy
    tx, ty = tx * tx * (3 - 2 * tx), ty * ty * (3 - 2 * ty)
    ix, iy = np.clip(ix, 0, n - 2), np.clip(iy, 0, n - 2)
    a, b = g[iy, ix], g[iy, ix + 1]
    c, d = g[iy + 1, ix], g[iy + 1, ix + 1]
    return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty


def fbm(x, y, scale, seed, octaves=4):
    s, amp, tot = 0.0, 1.0, 0.0
    for o in range(octaves):
        s = s + amp * value_noise(x, y, scale / (2 ** o), seed + o)
        tot += amp
        amp *= 0.5
    return s / tot


def straight_fields(x, y):
    """A estrada reta: (distância com sinal até o meio, posição ao longo, meia largura)."""
    (ax, ay), (bx, by) = ROAD['line']
    ex, ey = bx - ax, by - ay
    L = math.hypot(ex, ey)
    ux, uy = ex / L, ey / L
    along = (x - ax) * ux + (y - ay) * uy
    across = -(x - ax) * uy + (y - ay) * ux
    return across, along, ROAD['half']


def road_fields(x, y):
    """Distância até a areia (>0 na areia), a distância até a rua de cima e o quanto o asfalto dela
    já virou terra. Duas plantas: o cruzamento do dia ("crossing") e a estrada reta ("straight")."""
    if ROAD.get('kind') == 'straight':
        across, along, half = straight_fields(x, y)
        return np.abs(across) - half, np.full(x.shape, 1e6, np.float32), across, along
    HB, CR, BEND = ROAD['b_half'], ROAD['corner'], ROAD['b_end']
    right = x >= 0
    cA = np.where(right, ROAD['a_right'][0], ROAD['a_left'][0])
    hA = np.where(right, ROAD['a_right'][1], ROAD['a_left'][1])
    pts = ROAD['upper']
    dU = np.full(x.shape, 1e6, np.float32)
    for (ax, ay), (bx, by) in zip(pts[:-1], pts[1:]):
        ex, ey = bx - ax, by - ay
        t = np.clip(((x - ax) * ex + (y - ay) * ey) / (ex * ex + ey * ey), 0, 1)
        dU = np.minimum(dU, np.hypot(x - ax - ex * t, y - ay - ey * t))
    halfU = HB + (ROAD['upper_half'] - HB) * np.clip((BEND - y) / 8.0, 0, 1)
    u = np.where(y < BEND, dU - halfU, np.abs(x) - HB)
    v = np.abs(y - cA) - hA
    qx, qy = u - CR, v - CR
    sdq = np.where((qx >= 0) & (qy >= 0), -np.minimum(qx, qy), np.hypot(np.minimum(qx, 0), np.minimum(qy, 0)))
    d_sand = -(sdq - CR)
    return d_sand, dU, cA, hA


def rect_dist(x, y, rects):
    d = np.full(x.shape, 1e6, np.float32)
    for x0, x1, y0, y1 in rects:
        dx = np.maximum(np.maximum(x0 - x, x - x1), 0)
        dy = np.maximum(np.maximum(y0 - y, y - y1), 0)
        d = np.minimum(d, np.hypot(dx, dy))
    return d


def crack_height(x, y):
    """A altura da textura de rachadura (ladrilho de 12 m), pra inclinar as placas."""
    if not os.path.exists(CRACKS):
        return np.full(x.shape, 0.5, np.float32)
    a = np.asarray(Image.open(CRACKS))[..., 3].astype(np.float32) / 255.0
    n = a.shape[0]
    u = ((x / 12.0) % 1.0) * n
    v = ((y / 12.0) % 1.0) * n
    return a[v.astype(int) % n, u.astype(int) % n]


def fields(x, y):
    d_sand, dU, cA, hA = road_fields(x, y)
    feet = rect_dist(x, y, footprints())

    # a borda do asfalto quebrada (pedaços de 30 cm a 2 m) e a rua de cima virando terra
    # (13/10: o contorno da rua; nas referências boas a beira é quase reta e limpa, o 1,4 dava o serrilhado. ROAD['edge_noise'])
    edge = (fbm(x, y, 2.5, 10, 3) - 0.5) * 1.4 * ROAD.get('edge_noise', 1.0)
    if ROAD.get('kind') == 'straight':
        dirt_t = np.zeros(x.shape, np.float32)
        asphalt = (d_sand + edge < 0).astype(np.float32)
        dirt = np.zeros(x.shape, np.float32)
    else:
        dirt_t = np.where(y < ROAD['b_end'],
                          smoothstep(ROAD['dirt_from'], ROAD['dirt_to'], y + (fbm(x, y, 6.0, 11, 3) - 0.5) * 30.0), 0.0)
        asphalt = (d_sand + edge < 0).astype(np.float32) * (1.0 - (dirt_t > 0.5))
        dirt = ((dU < ROAD['upper_half'] + 0.6 + edge * 0.5) & (y < ROAD['b_end']) & (dirt_t > 0.5)).astype(np.float32)

    # o chão de areia: 6 cm abaixo do asfalto perto da rua e das casas, ondulando longe delas
    # (13/10, crítica da arte: dentro do asfalto o chão não ondula; o relevo longe da borda fazia bolhas de areia no
    # meio da pista, "tinta derramada")
    near = np.minimum(np.maximum(d_sand, 0.0), feet)
    far_w = smoothstep(3.0, 30.0, near)
    base = np.maximum(-0.06 + far_w * (fbm(x, y, 40.0, 20, 4) - 0.5) * 0.2, -0.40)   # areia batida, quase lisa (crítica 11/10)
    base = np.where(feet < 3.0, base * smoothstep(0.0, 3.0, feet), base)      # as casas pousam no zero

    # os montes de areia nas bordas da rua (em trechos) e encostados nas paredes
    patchy = smoothstep(0.36, 0.56, fbm(x, y, 9.0, 30, 3))
    # as línguas de areia na borda (crítica 11/10), esticadas na direção do vento (13/10: "línguas finas na direção do
    # vento", a road_town_battle): um ruído 5× mais comprido ao longo do vento empurra a faixa até ~2 m pra dentro da pista
    wy = math.radians(getattr(look_scene, 'WIND_YAW', 0.0))
    su = (x * math.cos(wy) + y * math.sin(wy)) / 5.0
    sv = -x * math.sin(wy) + y * math.cos(wy)
    streak = fbm(su, sv, 1.2, 33, 3)
    # (volta 11, crítica da arte: "espuma, renda, borda em gomos": o ruído de 3 m fazia os gomos; agora 7 m, mais macio)
    band = np.exp(-((d_sand - 0.6 + (fbm(x, y, 7.0, 31, 2) - 0.5) * 2.4 + (streak - 0.5) * 4.0) / 2.4) ** 2)
    drift = 0.22 * band * patchy      # (volta 11: 0,32 levantava montes que pegavam sol e liam brancos)
    if SAND_OVER > 0:      # a areia que cobre a beira da estrada (13/10)
        drift = drift * (1.0 + 0.9 * SAND_OVER)
    pile = 0.35 * np.exp(-feet / 0.9) * (0.4 + 0.6 * fbm(x, y, 2.0, 32, 2)) * (feet > 0.02)

    h_sand = base + drift + pile
    h_asph = 0.012 * (crack_height(x, y) - 0.5)
    rut = 1.0 - smoothstep(0.25, 0.7, np.abs(dU - 1.2))
    h_dirt = base + 0.03 - 0.045 * rut
    h = np.where(asphalt > 0.5, np.maximum(h_asph, base + drift - 0.03), h_sand)
    h = np.where(dirt > 0.5, np.maximum(h_dirt, base + drift * 0.5), h)

    sand_cover = np.clip(np.where(asphalt > 0.5, (base + drift - 0.03 - h_asph) / 0.2 + 0.0, 0.0), 0, 1)   # (volta 11: degradê macio, era / 0,08)
    sand_cover = np.maximum(sand_cover, np.clip(pile / 0.2, 0, 1))
    return h.astype(np.float32), asphalt, sand_cover.astype(np.float32), dirt, cA, hA


def paint(x, y, cA, hA):
    """As faixas pintadas, gravadas como RAMPA (0,5 = a borda da faixa; o material corta nítido com antisserrilhado).
    (13/10, o estudo nas favoritas: a road_town_battle e a close_game_cam) a faixa da beira a ~0,5 m do asfalto, 13 cm,
    acompanhando o contorno da rua (na esquina ela faz a curva); a tracejada no meio, 3 m a cada 9 m."""
    if ROAD.get('paint_v2', False):
        lw, soft = 0.065, 0.06
        prof = lambda dist: np.clip((lw + soft - dist) / (2 * soft), 0.0, 1.0)
        d_sand = road_fields(x, y)[0]                       # < 0 dentro da rua
        p = prof(np.abs(-d_sand - 0.55)) * (d_sand < -0.2)
        if ROAD.get('kind') == 'straight':
            across, along = cA, hA
            p = np.maximum(p, prof(np.abs(across)) * (np.mod(along / 9.0, 1.0) < 0.34))
            return p.astype(np.float32)
        HB, CR, BEND = ROAD['b_half'], ROAD['corner'], ROAD['b_end']
        outA = np.abs(x) > HB + CR + 1.0
        outB = (np.abs(y - cA) > hA + CR + 1.0) & (y > BEND)
        p = np.maximum(p, outA * (np.mod(x / 9.0, 1.0) < 0.34) * prof(np.abs(y - cA)))
        p = np.maximum(p, outB * (np.mod(y / 9.0, 1.0) < 0.34) * prof(np.abs(x)))
        return p.astype(np.float32)
    if ROAD.get('kind') == 'straight':
        across, along = cA, hA            # na estrada reta, road_fields devolve (através, ao longo)
        half, lw = ROAD['half'], 0.075
        dash = np.mod(along / 9.0, 1.0) < 0.34
        p = (np.abs(across) < lw) * dash
        p = np.maximum(p, np.abs(np.abs(across) - (half - 0.55)) < lw)
        return p.astype(np.float32)
    HB, CR, BEND = ROAD['b_half'], ROAD['corner'], ROAD['b_end']
    lw = 0.075
    p = np.zeros(x.shape, np.float32)
    outA = np.abs(x) > HB + CR + 1.0
    outB = (np.abs(y - cA) > hA + CR + 1.0) & (y > BEND)
    dashA = (np.mod(x / 9.0, 1.0) < 0.34)
    dashB = (np.mod(y / 9.0, 1.0) < 0.34)
    p = np.maximum(p, outA * dashA * (np.abs(y - cA) < lw))
    p = np.maximum(p, outA * (np.abs(np.abs(y - cA) - (hA - 0.55)) < lw))
    p = np.maximum(p, outB * dashB * (np.abs(x) < lw))
    p = np.maximum(p, outB * (np.abs(np.abs(x) - (HB - 0.55)) < lw))
    return p


def write_glb(path, pos, nrm, idx):
    """GLB mínimo: posições, normais e índices. Unreal (X, Y, Z) -> glTF (X, Z, Y)."""
    P = np.ascontiguousarray(pos[:, [0, 2, 1]], np.float32)
    Nn = np.ascontiguousarray(nrm[:, [0, 2, 1]], np.float32)
    I = np.ascontiguousarray(idx, np.uint32)
    blobs = [P.tobytes(), Nn.tobytes(), I.tobytes()]
    offs, cur = [], 0
    for b in blobs:
        offs.append(cur)
        cur += (len(b) + 3) // 4 * 4
    gl = {
        'asset': {'version': '2.0', 'generator': 'look_ground.py'},
        'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [{'mesh': 0, 'name': 'SM_LL_Ground'}],
        'materials': [{'name': 'Ground'}],
        'meshes': [{'name': 'SM_LL_Ground', 'primitives': [{'attributes': {'POSITION': 0, 'NORMAL': 1}, 'indices': 2, 'material': 0}]}],
        'buffers': [{'byteLength': cur}],
        'bufferViews': [{'buffer': 0, 'byteOffset': offs[0], 'byteLength': len(blobs[0]), 'target': 34962},
                        {'buffer': 0, 'byteOffset': offs[1], 'byteLength': len(blobs[1]), 'target': 34962},
                        {'buffer': 0, 'byteOffset': offs[2], 'byteLength': len(blobs[2]), 'target': 34963}],
        'accessors': [{'bufferView': 0, 'componentType': 5126, 'count': len(P), 'type': 'VEC3',
                       'min': P.min(0).tolist(), 'max': P.max(0).tolist()},
                      {'bufferView': 1, 'componentType': 5126, 'count': len(Nn), 'type': 'VEC3'},
                      {'bufferView': 2, 'componentType': 5125, 'count': I.size, 'type': 'SCALAR'}],
    }
    js = json.dumps(gl).encode()
    js += b' ' * ((4 - len(js) % 4) % 4)
    body = b''.join(b + b'\0' * ((4 - len(b) % 4) % 4) for b in blobs)
    with open(path, 'wb') as f:
        f.write(struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(body)))
        f.write(struct.pack('<II', len(js), 0x4E4F534A) + js)
        f.write(struct.pack('<II', len(body), 0x004E4942) + body)


def place(path):
    """A altura de tudo que pousa no chão e o espalhamento (tufos e pedrinhas) pela planta."""
    def z_at(pts):
        a = np.array(pts, np.float32).reshape(-1, 2)
        return slab_top(a[:, 0], a[:, 1]).tolist() if len(a) else []
    out = {'tanks': z_at([(t[0], t[1]) for t in TANKS]),
           'palms': z_at([(p[0], p[1]) for p in PALMS]),
           'shrubs': z_at([(p[0], p[1]) for p in SHRUBS]),
           'poles': z_at([(p[0], p[1]) for p in POLES]),
           'wrecks': z_at([(w[0], w[1]) for w in getattr(look_scene, 'WRECKS', [])]),
           'props': z_at([(q[1], q[2]) for q in getattr(look_scene, 'PROPS', [])])}
    r = np.random.default_rng(5)
    # candidatos na região que a câmera da arte e as do jogo veem perto (o foco em ~(-6, 0))
    n = 26000
    sx0, sx1, sy0, sy1 = getattr(look_scene, 'SCATTER_REGION', (-60, 75, -110, 45))
    cx = r.uniform(sx0, sx1, n).astype(np.float32)
    cy = r.uniform(sy0, sy1, n).astype(np.float32)
    h, asphalt, cover, dirt, _, _ = fields(cx, cy)
    if ROAD_SLAB:
        h = slab_top(cx, cy)              # a pedrinha na rua fica em cima da laje
    d_sand = road_fields(cx, cy)[0]
    feet = rect_dist(cx, cy, footprints())
    tk = np.full(cx.shape, 1e6, np.float32)
    for t_ in TANKS:
        tx, ty = t_[0], t_[1]
        tk = np.minimum(tk, np.hypot(cx - tx, cy - ty))
    on_sand = (asphalt < 0.5) & (dirt < 0.5) & (feet > 0.4) & (tk > 6.0)
    # o vento deixa os tufos na beira da rua e no pé da parede, e em touceiras pelo campo
    clump = fbm(cx, cy, 7.0, 70, 3)
    w_tuft = (0.25 * smoothstep(0.5, 0.75, clump) + 0.5 * np.exp(-np.maximum(d_sand, 0) / 3.0)
              + 0.6 * np.exp(-feet / 1.5)) * on_sand
    w_peb = (0.6 * np.exp(-np.abs(d_sand) / 1.2) + 0.25 * np.exp(-feet / 2.0)) * ((asphalt < 0.5) | (cover > 0.3)) * (tk > 5.0) * (feet > 0.3)
    w_peb = w_peb + getattr(look_scene, 'PEBBLE_FIELD', 0.0) * on_sand * (tk > 5.0)    # pedrinhas soltas pelo campo todo (a cena de perto)
    dens = getattr(look_scene, 'SCATTER_DENSITY', 1.0)
    zw_t = zw_p = zw_w = 1.0
    if getattr(look_scene, 'ZONES', None) is not None:
        zs_, zr_, zg_, zd_ = zones(cx, cy)
        zl_ = np.clip(1 - zs_ - zr_ - zg_, 0, 1)                         # o pisado (a v1)
        zw_t = (0.15 + 1.6 * zl_ + 0.4 * zg_) * (1 - 0.85 * zs_) * (1 - 0.9 * zr_)   # o capim em touceiras no pisado
        zw_p = 0.12 + 3.2 * zg_ + 0.4 * zd_                                  # as pedrinhas no pedregoso
        zw_w = 0.2 + 2.4 * zg_ + 0.8 * zd_ + 0.4 * zl_                       # os gravetos no pedregoso e no revirado
    pick_t = r.random(n) < w_tuft * 0.4 * dens * zw_t
    pick_p = (r.random(n) < w_peb * 0.4 * dens * zw_p) & ~pick_t
    sc = []
    bushy = r.random(n) < getattr(look_scene, 'BUSH_FRACTION', 0.45)   # parte das touceiras vira arbusto seco
    pick_b = pick_t & bushy
    pick_t = pick_t & ~bushy
    # os gravetos secos soltos na areia (a arte de perto tem muitos): só se a cena tiver o tipo 'twig'
    pick_w = (r.random(n) < getattr(look_scene, 'TWIG_FIELD', 0.0) * on_sand * (tk > 5.0) * zw_w) & ~pick_t & ~pick_p
    for kind, m, scale in (('tuft', pick_t, getattr(look_scene, 'TUFT_SCALE', (2.4, 4.0))), ('bush', pick_b, getattr(look_scene, 'BUSH_SCALE', (0.45, 0.95))),
                           ('pebble', pick_p, getattr(look_scene, 'PEBBLE_SCALE', (2.5, 6.0))), ('twig', pick_w, (0.3, 0.6))):
        if kind not in SCATTER_KINDS:
            continue
        for i in np.nonzero(m)[0]:
            sc.append({'kind': kind, 'x': float(cx[i]), 'y': float(cy[i]), 'z': float(h[i]),
                       'yaw': float(r.uniform(0, 360)), 's': float(r.uniform(*scale)), 'v': int(r.integers(0, 99))})
    if CRATERS:      # nada de tufo nem pedrinha dentro da cratera (a cratera tem os torrões dela)
        sc = [it for it in sc if not crater_inside(np.array([it['x']]), np.array([it['y']]), 2.5)[0]]
        # os torrões da volta 3: as pedras escaneadas no lugar dos calombos (a pedra tem ~7 cm na foto)
        rr = np.random.default_rng(77)
        for c, m in zip(CRATERS, crater_maps()):
            a = math.radians(c.get('yaw', 0.0))
            for (u, v, w) in m.get('clods', []):
                wx = c['x'] + u * math.cos(a) - v * math.sin(a)
                wy = c['y'] + u * math.sin(a) + v * math.cos(a)
                z = float(fields(np.array([wx], np.float32), np.array([wy], np.float32))[0][0] + crater_height(np.array([wx]), np.array([wy]))[0])
                sc.append({'kind': 'pebble', 'x': wx, 'y': wy, 'z': z - 0.02 * w, 'yaw': float(rr.uniform(0, 360)),
                           's': w / 0.07, 'v': int(rr.integers(0, 99))})
    out['scatter'] = sc
    out['dressing'] = dressing() if getattr(look_scene, 'DRESSING', None) else []
    with open(path, 'w') as f:
        json.dump(out, f)
    print(f'LOOKGROUND espalhados: {int(pick_t.sum())} tufos, {int(pick_b.sum())} arbustos, {int(pick_p.sum())} pedrinhas, '
          f'{int(pick_w.sum()) if "twig" in SCATTER_KINDS else 0} gravetos')


def dressing():
    """O vestir da vila: (malha, x, y, z, giro) pelas regras do DRESSING da planta (semente fixa)."""
    D = look_scene.DRESSING
    r = np.random.default_rng(D.get('seed', 31))
    out = []
    keep = [(t[0], t[1], 6.5) for t in TANKS] + [(e[0], e[1], 9.0) for e in getattr(look_scene, 'EXPLOSIONS', [])]

    def free(x, y, rad=0.0):
        return all(math.hypot(x - kx, y - ky) > kr + rad for kx, ky, kr in keep)

    # os telhados: a laje a h − 0,55 (a mureta), dentro dela
    for name, h in list(look_scene.HOUSES.items()):
        sx, sy, hh = h['sx'], h['sy'], h['h']
        slab = hh - 0.55
        if h.get('ex', {}).get('stories', 1) > 1:
            slab = hh - 0.55
        n = int(r.integers(*D.get('roof_items', (1, 4))))
        for _ in range(n):
            kind = r.choice(D['roof_kinds'], p=D['roof_p'])
            px = h['x'] + r.uniform(-sx / 2 + 1.0, sx / 2 - 1.0)
            py = h['y'] + r.uniform(-sy / 2 + 1.0, sy / 2 - 1.0)
            out.append((str(kind), float(px), float(py), float(slab), float(r.choice((0, 90, 180, 270)) + r.uniform(-8, 8))))
            keep.append((px, py, 1.2))
    # o chão: candidatos pela planta, pesados pelo lugar
    n = D.get('candidates', 9000)
    sx0, sx1, sy0, sy1 = getattr(look_scene, 'SCATTER_REGION', (-60, 75, -110, 45))
    cx = r.uniform(sx0, sx1, n).astype(np.float32)
    cy = r.uniform(sy0, sy1, n).astype(np.float32)
    d_sand = road_fields(cx, cy)[0]                       # > 0 na areia, < 0 dentro da rua
    feet = rect_dist(cx, cy, footprints())
    near_wall = (feet > 0.5) & (feet < 2.2)
    curb = (d_sand > 0.4) & (d_sand < 3.5) & (feet > 0.5)
    lot = (d_sand > 3.5) & (feet > 2.2) & (feet < 14.0)
    w = np.where(near_wall, D['w_wall'], 0.0) + np.where(curb, D['w_curb'], 0.0) + np.where(lot, D['w_lot'], 0.0)
    pick = r.random(n) < w
    eps = 0.05
    for i in np.nonzero(pick)[0]:
        x, y = float(cx[i]), float(cy[i])
        if near_wall[i]:
            kind = r.choice(D['wall_kinds'], p=D['wall_p'])
        elif curb[i]:
            kind = r.choice(D['curb_kinds'], p=D['curb_p'])
        else:
            kind = r.choice(D['lot_kinds'], p=D['lot_p'])
        rad = {'Fence0': 2.2, 'Fence1': 2.2, 'BrokenWall0': 2.2, 'BrokenWall1': 2.2, 'Rubble2': 2.0}.get(str(kind), 1.2)
        if not free(x, y, rad):
            continue
        # o giro: os compridos (cerca, muro) acompanham a rua (a direção ao longo da borda); o resto sorteado
        if str(kind).startswith(('Fence', 'BrokenWall')):
            g = road_fields(np.array([x + eps, x - eps, x, x], np.float32), np.array([y, y, y + eps, y - eps], np.float32))[0]
            gx, gy = (g[0] - g[1]), (g[2] - g[3])
            yaw = math.degrees(math.atan2(gx, -gy)) if abs(gx) + abs(gy) > 1e-6 else r.uniform(0, 180)
        else:
            yaw = r.uniform(0, 360)
        z = float(fields(np.array([x], np.float32), np.array([y], np.float32))[0][0])
        out.append((str(kind), x, y, z, float(yaw)))
        keep.append((x, y, rad))
        # o grupo (o estudo de 13/10: 1 grande + médias em volta, nunca peça isolada em grade): 1–3 vizinhas a 1–2,5 m
        pool = D['wall_kinds'] if near_wall[i] else (D['curb_kinds'] if curb[i] else D['lot_kinds'])
        small = [k for k in pool if not str(k).startswith(('Fence', 'BrokenWall', 'Rubble2'))]
        for _ in range(int(r.integers(*D.get('group', (1, 4))))):
            a_ = r.uniform(0, 6.283); dd = r.uniform(1.0, 2.5)
            gx_, gy_ = x + dd * math.cos(a_), y + dd * math.sin(a_)
            if road_fields(np.array([gx_], np.float32), np.array([gy_], np.float32))[0][0] < 0.3 or not free(gx_, gy_, 0.6):
                continue
            gz = float(fields(np.array([gx_], np.float32), np.array([gy_], np.float32))[0][0])
            out.append((str(r.choice(small)), float(gx_), float(gy_), gz, float(r.uniform(0, 360))))
            keep.append((gx_, gy_, 0.7))
    print(f'LOOKGROUND vestir: {len(out)} peças')
    return out


# ---------------------------------------------------------------- a cratera v2 (13/10, a prova na maquete)
# O MESMO crater() do jogo (Game/Tools/bake_craters.py), a 2 cm por texel em vez de 4,4, e o relevo de verdade: um
# pedaço de chão de 2,5 cm afundado pela altura (a bacia, a borda e os torrões viram malha, com sombra), por cima do
# chão normal recortado. A cor vem do material (T_LL_Crater: R altura, G escuro, B calor, A claro).
CRATERS = getattr(look_scene, 'CRATERS', [])
CRATER_SPAN = 14.0
CRATER_TEX = 700
CRATER_RANGE = 0.8          # a altura no PNG: (R − 0,5)·2·0,8 m, como o atlas do jogo
PATCH_STEP = 0.025
_CMAPS = None


def crater_maps():
    global _CMAPS
    if _CMAPS is None:
        _CMAPS = []
        if CRATERS:
            import bake_craters as bk
            bk.KINDS['crater'] = dict(size=CRATER_TEX, span=CRATER_SPAN)
            for c in CRATERS:
                opt = dict(bk.OPTIONS[c.get('opt', 'A')], **c.get('params', {}))
                m = bk.crater(opt, c.get('seed', 0))
                if c.get('v2_values', False):
                    crater_v2_values(m, c.get('seed', 0))
                if c.get('v3_relief', False):
                    crater_v3_relief(m)
                _CMAPS.append(m)
    return _CMAPS


def crater_v2_values(m, seed):
    """Os valores medidos na street_battle (13/10) por cima do desenho do jogo, enquanto o rebake não sai: o halo escuro
    contínuo até ~1,9 R (no alvo o entorno é marrom-escuro) e o claro só nos torrões (no alvo o anel da borda não é mais
    claro que o chão: o claro são os torrões soltos). É o que a sessão do jogo vai pôr no rebake do atlas."""
    n = m['h'].shape[0]
    c = (np.arange(n, dtype=np.float32) + 0.5) * CRATER_SPAN / n - CRATER_SPAN / 2
    x, y = np.meshgrid(c, c)
    ra = m['ra']
    r = np.hypot(x, y) * (1.0 + (fbm(x, y, 1.6, 400 + seed, 3) - 0.5) * 0.5)        # a borda do halo torta
    halo = (1.0 - smoothstep(1.5 * ra, 1.95 * ra, r)) * (0.85 + 0.15 * fbm(x, y, 0.5, 410 + seed, 2))
    ring = (r > 0.85 * ra) & (r < 1.7 * ra)
    clod = smoothstep(0.035, 0.07, m['h'])                                           # o torrão é alto; o anel não
    m['light'] = np.where(ring, m['light'] * clod, m['light'])
    m['dark'] = np.maximum(m['dark'], halo) * (1.0 - 0.7 * m['light'])


def _box(a, r):
    """Média numa janela (2r+1)² por somas acumuladas (sem scipy)."""
    p = np.pad(a, r, mode='edge')
    c = p.cumsum(0).cumsum(1)
    c = np.pad(c, ((1, 0), (1, 0)))
    k = 2 * r + 1
    return (c[k:, k:] - c[:-k, k:] - c[k:, :-k] + c[:-k, :-k]) / (k * k)


def crater_v3_relief(m):
    """Volta 3 da prova (13/10, o Felipe: "a nossa cratera parece modelada fofinha; os game shots têm traçados mais
    rígidos"): (a) a parede da bacia em pé e o fundo largo (o perfil t' = 1 − (1 − t)², a borda vira uma linha), e (b) os
    torrões saem do relevo (eram calombos redondos) e viram pedras escaneadas de verdade, com quina, no mesmo lugar: o
    caminho dos pedaços 3D que o jogo vai fazer. m['clods'] = [(u, v, largura em m)]."""
    h = m['h'].astype(np.float32)
    d = float(m['depth'])
    t = np.clip(-h / d, 0.0, 1.0)
    h = np.where(h < 0, -d * (1.0 - (1.0 - t) ** 1.6), h)
    h = _box(h, 2)                                         # (volta 6: o 1 − (1 − t)² dobrava num círculo nítido no fundo)
    n = h.shape[0]
    px = CRATER_SPAN / n
    c = (np.arange(n, dtype=np.float32) + 0.5) * px - CRATER_SPAN / 2
    x, y = np.meshgrid(c, c)
    r = np.hypot(x, y)
    low = _box(_box(h, 6), 6)                          # o relevo sem os torrões (~25 cm de janela, duas vezes)
    bump = h - low
    outside = r > 0.8 * m['ra']
    # os picos: o máximo numa janela 9×9 e acima de 2,5 cm de altura sobre o entorno
    mx = bump.copy()
    for dy in range(-4, 5):
        for dx in range(-4, 5):
            mx = np.maximum(mx, np.roll(np.roll(bump, dy, 0), dx, 1))
    # (volta 4: nada dentro da bacia, menos peças, tamanhos de 10 a 60 cm; a volta 3 enchia o buraco e lia campo de pedra)
    peaks = (bump >= mx) & (bump > 0.03) & (r > 1.05 * m['ra'])
    ys, xs = np.nonzero(peaks)
    rk = np.random.default_rng(5)
    keep = rk.random(len(ys)) < 0.7
    m['clods'] = [(float(x[j, i]), float(y[j, i]), float(np.clip(bump[j, i] / 0.22, 0.1, 0.6)))
                  for j, i, k in zip(ys, xs, keep) if k]
    h = np.where(outside, np.minimum(h, low + 0.008), h)     # o calombo sai; a pedra entra no lugar
    # (volta 5) o claro pintado onde era calombo sai (lia "pipoca" branca: a pedra 3D está no lugar), e o claro dentro da
    # bacia fica fraco
    m['light'] = np.where(outside & (bump > 0.01), 0.0, m['light'])
    m['light'] = np.where(r < m['ra'], m['light'] * 0.3, m['light'])
    # (volta 5) o fundo com textura: torrões e degraus de terra revirada de ~20–40 cm, ±3 cm (a sombra de verdade desenha)
    inside = 1.0 - smoothstep(0.7 * m['ra'], 1.0 * m['ra'], r)
    h = h + inside * (fbm(x, y, 0.35, 520, 3) - 0.5) * 0.07
    # (volta 5) o escuro dentro da bacia só no núcleo e em manchas (era a bacia inteira preta e lisa; a parede do lado do sol
    # tem de acender a areia, como na sand_gpt)
    # (volta 6: o núcleo torto e esfiapado, não um disco: o raio deformado forte e as manchas de carvão entrando nele)
    core = 1.0 - smoothstep(0.15 * m['ra'], 0.55 * m['ra'], r * (1.0 + (fbm(x, y, 0.6, 530, 3) - 0.5) * 1.3))
    patch = smoothstep(0.45, 0.7, fbm(x, y, 0.4, 540, 3)) * 0.75
    m['dark'] = np.where(r < 0.95 * m['ra'], np.maximum(core * (0.75 + 0.25 * patch), patch), m['dark'])
    # (volta 6) o manto de ejeção: de 1 a ~2,6 R, a areia revirada mais escura e mosqueada (no alvo, p25 0,31 contra 0,54
    # na volta 5), com a borda de fora esfiapada e mais longa pro lado do tiro
    rmant = r * (1.0 + (fbm(x, y, 1.2, 550, 3) - 0.5) * 0.7) / (1.0 + 0.25 * np.clip(x / np.maximum(r, 1e-3), 0, 1))
    mant = (1.0 - smoothstep(1.6 * m['ra'], 2.6 * m['ra'], rmant)) * smoothstep(0.85 * m['ra'], 1.05 * m['ra'], r)
    mott = 0.35 + 0.35 * smoothstep(0.35, 0.7, fbm(x, y, 0.3, 560, 3))
    m['dark'] = np.maximum(m['dark'], mant * mott)
    # (volta 5) os riscos de fuligem saindo do centro, de 1 a 3,5 R, mais densos pro lado do tiro (+X), finos e desiguais
    rk2 = np.random.default_rng(31)
    th = np.arctan2(y, x)
    streak = np.zeros_like(h)
    for _ in range(14):
        a0 = rk2.normal(0.0, 1.1) if rk2.random() < 0.65 else rk2.uniform(-math.pi, math.pi)
        ln = m['ra'] * rk2.uniform(1.8, 3.5) * (1.0 + 0.3 * max(math.cos(a0), 0.0))
        wdt = rk2.uniform(0.04, 0.09)
        dth = np.angle(np.exp(1j * (th - a0)))
        streak = np.maximum(streak, (1.0 - smoothstep(wdt * 0.4, wdt, np.abs(dth))) * (1.0 - smoothstep(ln * 0.6, ln, r))
                            * smoothstep(0.9 * m['ra'], 1.2 * m['ra'], r) * rk2.uniform(0.5, 0.85))
    m['dark'] = np.maximum(m['dark'], streak)
    # (volta 5) os pedaços pequenos em raios (a sand_gpt tem centenas): 10–22 cm, até 3,5 R, mais pro lado do tiro
    deb = []
    for _ in range(170):
        a0 = rk2.normal(0.0, 1.0) if rk2.random() < 0.6 else rk2.uniform(-math.pi, math.pi)
        rr_ = m['ra'] * (1.05 + rk2.gamma(1.6, 0.7)) * (1.0 + 0.25 * max(math.cos(a0), 0.0))
        if rr_ > 3.6 * m['ra'] or rr_ > CRATER_SPAN / 2 - 0.6:
            continue
        deb.append((rr_ * math.cos(a0), rr_ * math.sin(a0), float(rk2.uniform(0.1, 0.22))))
    # (volta 6) os pedaços graúdos na borda (25–45 cm) em ~60% da volta: o lábio quebrado do alvo; e mais pedrinhas miúdas
    # (6–10 cm) no manto
    for _ in range(26):
        a0 = rk2.uniform(-math.pi, math.pi)
        if math.cos(a0 * 1.0 + 1.3) < -0.3:          # ~40% da volta sem lábio
            continue
        rr_ = m['ra'] * rk2.uniform(0.95, 1.25)
        deb.append((rr_ * math.cos(a0), rr_ * math.sin(a0), float(rk2.uniform(0.25, 0.45))))
    for _ in range(220):
        a0 = rk2.normal(0.0, 1.3) if rk2.random() < 0.5 else rk2.uniform(-math.pi, math.pi)
        rr_ = m['ra'] * rk2.uniform(1.0, 2.6)
        deb.append((rr_ * math.cos(a0), rr_ * math.sin(a0), float(rk2.uniform(0.06, 0.1))))
    # (volta 6, a comparação na mesma escala: a terra entre as pedras era lisa, "massinha") o relevo fino da terra revirada,
    # torrõezinhos de 5–30 cm, ±1,5–3 cm, em toda a bacia e no manto, sumindo no chão normal
    churn = (1.0 - smoothstep(2.0 * m['ra'], 2.8 * m['ra'], r))
    fine = (fbm(x, y, 0.12, 570, 2) - 0.5) * 0.03 + (fbm(x, y, 0.3, 580, 2) - 0.5) * 0.04
    lumps = smoothstep(0.55, 0.75, fbm(x, y, 0.09, 590, 2)) * 0.025          # os torrõezinhos soltos
    h = h + churn * (fine + lumps)
    # as pedras: tamanhos em lei de potência (muitas miúdas de 3–6 cm, poucas graúdas)
    for _ in range(320):
        a0 = rk2.normal(0.0, 1.2) if rk2.random() < 0.55 else rk2.uniform(-math.pi, math.pi)
        rr_ = m['ra'] * rk2.uniform(0.3, 2.4)
        deb.append((rr_ * math.cos(a0), rr_ * math.sin(a0), float(0.03 + 0.04 * rk2.random() ** 2)))
    m['clods'] = m['clods'] + deb
    m['h'] = h


def crater_local(x, y, c):
    a = math.radians(c.get('yaw', 0.0))
    dx, dy = x - c['x'], y - c['y']
    return dx * math.cos(a) + dy * math.sin(a), -dx * math.sin(a) + dy * math.cos(a)


def crater_sample(img, u, v):
    """Bilinear no mapa da cratera (linhas = y, colunas = x, como o grid() do bake), 0 fora."""
    n = img.shape[0]
    px = CRATER_SPAN / n
    fx = (u + CRATER_SPAN / 2) / px - 0.5
    fy = (v + CRATER_SPAN / 2) / px - 0.5
    inside = (fx >= 0) & (fx <= n - 1) & (fy >= 0) & (fy <= n - 1)
    fx = np.clip(fx, 0, n - 1.001)
    fy = np.clip(fy, 0, n - 1.001)
    x0, y0 = fx.astype(np.int32), fy.astype(np.int32)
    ax, ay = fx - x0, fy - y0
    val = (img[y0, x0] * (1 - ax) + img[y0, x0 + 1] * ax) * (1 - ay) + (img[y0 + 1, x0] * (1 - ax) + img[y0 + 1, x0 + 1] * ax) * ay
    return np.where(inside, val, 0.0).astype(np.float32)


def crater_height(x, y):
    h = np.zeros(np.shape(x), np.float32)
    for c, m in zip(CRATERS, crater_maps()):
        u, v = crater_local(x, y, c)
        h += crater_sample(m['h'], u, v)
    return h


def crater_inside(x, y, margin):
    """Dentro do quadrado de alguma cratera, encolhido pela margem (m)."""
    ins = np.zeros(np.shape(x), bool)
    for c in CRATERS:
        u, v = crater_local(x, y, c)
        ins |= (np.abs(u) < CRATER_SPAN / 2 - margin) & (np.abs(v) < CRATER_SPAN / 2 - margin)
    return ins


def save_crater_texture():
    """T_LL_Crater: as crateras lado a lado (R altura, G escuro, B calor, A claro), 2 cm por texel."""
    ms = crater_maps()
    if not ms:
        return
    tiles = []
    for m in ms:
        r = np.clip(m['h'] / (2 * CRATER_RANGE) + 0.5, 0, 1)
        tiles.append(np.stack([r, m['dark'], m['heat'], m['light']], -1))
    a = np.concatenate(tiles, 1)
    Image.fromarray((np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGBA').save(os.path.join(OUT, f'T_LL_Crater{SUFFIX}.png'))
    print(f'LOOKGROUND crateras: {len(ms)} ({CRATER_TEX}² cada, {100 * CRATER_SPAN / CRATER_TEX:.1f} cm/texel), '
          f'bacia {min(float(m["h"].min()) for m in ms):.2f} m')


def crater_patches():
    """As malhas de 2,5 cm das crateras: (posições, normais, índices). A borda de 0,4 m fica 1 cm abaixo, por baixo do chão."""
    out = []
    n = int(round(CRATER_SPAN / PATCH_STEP)) + 1
    s = np.arange(n, dtype=np.float32) * PATCH_STEP - CRATER_SPAN / 2
    lu, lv = np.meshgrid(s, s)
    for c in CRATERS:
        a = math.radians(c.get('yaw', 0.0))
        wx = c['x'] + lu * math.cos(a) - lv * math.sin(a)
        wy = c['y'] + lu * math.sin(a) + lv * math.cos(a)
        band = smoothstep(CRATER_SPAN / 2 - 0.5, CRATER_SPAN / 2 - 0.2, np.maximum(np.abs(lu), np.abs(lv)))
        h = fields(wx, wy)[0] + crater_height(wx, wy) - 0.01 * band
        du = np.gradient(h, PATCH_STEP, axis=1)
        dv = np.gradient(h, PATCH_STEP, axis=0)
        gx = du * math.cos(a) - dv * math.sin(a)          # o gradiente de volta pro mundo
        gy = du * math.sin(a) + dv * math.cos(a)
        nr = np.stack([-gx, -gy, np.ones_like(h)], -1)
        nr /= np.linalg.norm(nr, axis=-1, keepdims=True)
        pos = np.stack([wx, wy, h], -1).reshape(-1, 3)
        j, i = np.mgrid[0:n - 1, 0:n - 1]
        aa = (j * n + i).ravel(); bb = ((j + 1) * n + i).ravel(); cc = (j * n + i + 1).ravel(); dd = ((j + 1) * n + i + 1).ravel()
        idx = np.stack([np.stack([aa, bb, cc], 1), np.stack([cc, bb, dd], 1)], 1).reshape(-1, 3)
        out.append((pos.astype(np.float32), nr.reshape(-1, 3).astype(np.float32), idx))
    return out


# ---------------------------------------------------------------- a estrada com volume (13/10, o protótipo)
# O Felipe: "a nossa é uma textura pintada no chão; a dos game shots é uma estrada em cima do chão, uma camadinha bonita,
# com um vãozinho". O estudo (Battlefield/Ghost Recon/Arma/Cities/Anno; a doc da Unreal): a malha da laje gerada a partir
# do traçado, opaca, com a borda quebrada na geometria; a areia encostada é o próprio chão levantado no lado do vento.
# Aqui: um campo de altura de 6 cm só na faixa da rua (o contorno quebrado do asfalto da planta), o topo a THICK acima do
# asfalto, o abaulado no meio, e fora da laje a malha afunda por baixo do chão (o chanfro sai da última fileira).
ROAD_SLAB = getattr(look_scene, 'ROAD_SLAB', None)
# a estrada plana (13/10, o Felipe: "gostei desse exemplo sem elevação, dá pra botar como mais uma variação"): sem laje, o
# asfalto no nível do chão; só a areia da beira continua (sand_over)
ROAD_FLAT = bool(ROAD_SLAB) and ROAD_SLAB.get('type') == 'flat'
SAND_OVER = ROAD_SLAB.get('sand_over', 0.0) if ROAD_SLAB else 0.0
if ROAD_FLAT:
    ROAD_SLAB = None


def road_slab():
    S = ROAD_SLAB
    st = S.get('step', 0.06)
    x0, x1, y0, y1 = S['region']
    xs = np.arange(x0, x1 + st / 2, st, dtype=np.float32)
    ys = np.arange(y0, y1 + st / 2, st, dtype=np.float32)
    X, Y = np.meshgrid(xs, ys)
    d = np.empty(X.shape, np.float32)
    for r0 in range(0, X.shape[0], 256):                   # em blocos (a rua de cima é uma polilinha)
        d[r0:r0 + 256] = road_fields(X[r0:r0 + 256], Y[r0:r0 + 256])[0]
    cand = d < 1.0
    z = np.full(X.shape, np.nan, np.float32)
    xc, yc = X[cand], Y[cand]
    hg, asphalt, cover, dirt, _, _ = fields(xc, yc)
    # (v2: o topo sem a altura das placas rachadas: a 6 cm ela virava uma ondulação na pista toda; a rachadura fica na normal)
    h_asph = 0.0
    crown = S.get('crown', 0.025) * smoothstep(0.0, 4.0, -d[cand])
    on = (asphalt > 0.5)
    top = h_asph + S.get('thick', 0.08) + crown
    # (v3) a borda quebrada de verdade (as imagens 14 e 12): o asfalto perto da beira racha em placas de 30–70 cm (Voronoi);
    # umas faltam (aparece a areia de baixo), outras afundaram e inclinaram, e a fresta entre elas é um sulco
    dd = np.clip(-d[cand], 0.0, None)
    # (v5: a borda quebrada só em trechos de 5–15 m, o resto limpo, como na 14 e na 12; era quebrada em toda a volta e lia muro)
    zone = (1.0 - smoothstep(0.2, slab_opt('broken', 1.4), dd)) * edge_break(xc, yc)
    f1, f2, cid, ox, oy = voronoi(xc, yc, S.get('plate', 0.5))
    h1 = _hash2(cid, 1); h2 = _hash2(cid, 2); h3 = _hash2(cid, 3); h4 = _hash2(cid, 4)
    miss = (h1 < slab_opt('miss', 0.38) * zone) & (dd < 0.9)
    drop = (~miss) & (h2 < slab_opt('drop', 0.55) * zone)
    tilt = ((xc - ox) * np.cos(h4 * 6.283) + (yc - oy) * np.sin(h4 * 6.283)) * 0.08
    top = top - np.where(drop, 0.02 + 0.06 * h3 + tilt * zone, 0.0)
    top = top - 0.035 * zone * (1.0 - smoothstep(0.015, 0.05, f2 - f1))      # a fresta entre as placas
    on = on & ~miss
    # (a areia cobrindo a beira, 13/10: na 14 e na 12 a areia invade a estrada por cima da beira) onde o monte de areia do
    # chão chega ao topo da laje, a superfície é a areia (o material pinta areia pela planta, ver slab_sand)
    sand_up = hg + 0.01 > top
    top = np.where(sand_up, hg + 0.005, top)
    z[cand] = np.where(on, top, hg - 0.06)
    lay = np.zeros(X.shape, bool)
    lay[cand] = on
    # as células com pelo menos um canto na laje e os quatro com altura
    ok = ~np.isnan(z)
    cell = ok[:-1, :-1] & ok[1:, :-1] & ok[:-1, 1:] & ok[1:, 1:]
    anyon = lay[:-1, :-1] | lay[1:, :-1] | lay[:-1, 1:] | lay[1:, 1:]
    j, i = np.nonzero(cell & anyon)
    nx = X.shape[1]
    used = np.zeros(X.shape, bool)
    used[j, i] = used[j + 1, i] = used[j, i + 1] = used[j + 1, i + 1] = True
    vid = np.full(X.shape, -1, np.int64)
    vid[used] = np.arange(int(used.sum()))
    a = vid[j, i]; b = vid[j + 1, i]; c = vid[j, i + 1]; dd = vid[j + 1, i + 1]
    idx = np.stack([np.stack([a, b, c], 1), np.stack([c, b, dd], 1)], 1).reshape(-1, 3)
    zf = np.where(np.isnan(z), -0.2, z)
    gx = np.gradient(zf, st, axis=1)
    gy = np.gradient(zf, st, axis=0)
    nr = np.stack([-gx, -gy, np.ones_like(zf)], -1)
    nr /= np.linalg.norm(nr, axis=-1, keepdims=True)
    pos = np.stack([X[used], Y[used], zf[used]], -1).astype(np.float32)
    print(f'LOOKGROUND laje da estrada: {len(pos)} vértices, {len(idx)} triângulos, {st * 100:.0f} cm, '
          f'degrau {S.get("thick", 0.08) * 100:.0f} cm acima do asfalto')
    return pos, nr[used].astype(np.float32), idx


def _hash2(cid, k):
    v = np.sin(cid[..., 0] * 127.1 + cid[..., 1] * 311.7 + k * 74.7) * 43758.5453
    return (v - np.floor(v)).astype(np.float32)


def voronoi(x, y, size):
    """F1, F2 (m), a célula mais perto (i, j) e o centro dela, num Voronoi de pontos sorteados por célula."""
    gx, gy = x / size, y / size
    ix, iy = np.floor(gx), np.floor(gy)
    f1 = np.full(x.shape, 9e9, np.float32); f2 = np.full(x.shape, 9e9, np.float32)
    cid = np.zeros(x.shape + (2,), np.float32); ox = np.zeros(x.shape, np.float32); oy = np.zeros(x.shape, np.float32)
    for dj in (-1, 0, 1):
        for di in (-1, 0, 1):
            c = np.stack([ix + di, iy + dj], -1)
            px = (c[..., 0] + _hash2(c, 11)) * size
            py = (c[..., 1] + _hash2(c, 12)) * size
            dist = np.hypot(x - px, y - py)
            closer = dist < f1
            f2 = np.where(closer, f1, np.minimum(f2, dist))
            f1 = np.where(closer, dist, f1)
            cid = np.where(closer[..., None], c, cid)
            ox = np.where(closer, px, ox); oy = np.where(closer, py, oy)
    return f1, f2, cid, ox, oy


# os três tipos de borda da estrada com volume (13/10, o Felipe: "gostei da bem quebrada, e queria uma opção sem quebrada
# igual a 14 e a 12; é uma possibilidade termos todas as opções na construção do mundo"): o WorldForge escolhe por estrada
EDGE_TYPES = {
    'intact': dict(edge='none'),                                                    # a 14 e a 12: borda limpa
    'worn': dict(edge='patches', break_patches=0.45, broken=0.9, miss=0.2, drop=0.4),   # gasta: quebra em trechos (v5)
    'ruined': dict(edge='full', broken=1.4, miss=0.38, drop=0.55),                 # destruída: a borda toda (v4)
}


def slab_opt(key, default=None):
    t = EDGE_TYPES.get(ROAD_SLAB.get('type', ''), {}) if ROAD_SLAB else {}
    return ROAD_SLAB.get(key, t.get(key, default)) if ROAD_SLAB else default


def edge_break(x, y):
    """Os trechos da beira da laje que quebraram (0..1): nenhum (intacta), manchas de 5–15 m (gasta) ou tudo (destruída)."""
    mode = slab_opt('edge', 'patches')
    if not ROAD_SLAB or mode == 'none':
        return np.zeros(np.shape(x), np.float32)
    if mode == 'full':
        return np.ones(np.shape(x), np.float32)
    bp = slab_opt('break_patches', 0.45)
    return smoothstep(0.5 - 0.5 * bp, 0.62 - 0.5 * bp, fbm(x, y, 9.0, 610, 3)).astype(np.float32)


def road_frame(x, y):
    """O referencial da rua por ponto (pro asfalto v2, look_asphalt.ASPHALT_V2_CODE): a distância com sinal até o eixo
    (m), a largura do asfalto (m) e a direção da pista (rad). O mesmo que o WorldForge assa no RoadDir."""
    if ROAD.get('kind') == 'straight':
        across, along, half = straight_fields(x, y)
        (ax, ay), (bx, by) = ROAD['line']
        return across, np.full(x.shape, 2 * half, np.float32), np.full(x.shape, math.atan2(by - ay, bx - ax), np.float32)
    d_sand, dU, cA, hA = road_fields(x, y)
    HB, BEND = ROAD['b_half'], ROAD['b_end']
    halfU = HB + (ROAD['upper_half'] - HB) * np.clip((BEND - y) / 8.0, 0, 1)
    dA = np.abs(y - cA) - hA
    dB = np.where(y < BEND, dU - halfU, np.abs(x) - HB)
    useA = dA < dB
    across = np.where(useA, y - cA, np.where(y < BEND, dU, x))
    width = np.where(useA, 2 * hA, np.where(y < BEND, 2 * halfU, 2 * HB))
    ang = np.where(useA, 0.0, math.pi / 2)
    return across.astype(np.float32), width.astype(np.float32), ang.astype(np.float32)


def save_road_frame(tx, ty):
    """T_LL_RoadFrame: R = across/25,6 + 0,5, G = largura/32, B = direção (mod π)/π."""
    ac, w, ang = road_frame(tx, ty)
    rgba = np.dstack([np.clip(ac / 25.6 + 0.5, 0, 1), np.clip(w / 32.0, 0, 1), np.mod(ang, math.pi) / math.pi,
                      np.ones_like(ac)])
    Image.fromarray((rgba * 255 + 0.5).astype(np.uint8), 'RGBA').save(os.path.join(OUT, f'T_LL_RoadFrame{SUFFIX}.png'))


def slab_top(x, y):
    """A altura onde as coisas pousam: o chão, ou o topo da laje da estrada onde ela existe (o tanque sobe nela)."""
    f = fields(x, y)
    z = f[0]
    if ROAD_SLAB:
        d = road_fields(x, y)[0]
        top = ROAD_SLAB.get('thick', 0.08) + ROAD_SLAB.get('crown', 0.025) * smoothstep(0.0, 4.0, -d)
        x0, x1, y0, y1 = ROAD_SLAB['region']
        inr = (x >= x0) & (x <= x1) & (y >= y0) & (y <= y1)
        z = np.where((f[1] > 0.5) & inr, np.maximum(top, f[0] + 0.005), z)   # (a areia por cima da laje)
    return z


def track_paths():
    """As marcas de esteira na areia (a noite, 11/10: a arte risca a areia com rastros curvos atrás dos
    tanques): de cada tanque pra trás, 30 m, a direção girando devagar (semente fixa)."""
    if not getattr(look_scene, 'TRACKS_FROM_TANKS', False):
        return []
    import random
    r = random.Random(17)
    out = []
    for t in TANKS:
        if len(t) < 3:
            continue
        a = math.radians(t[2] + 180.0)
        x, y = t[0] + 4.0 * math.cos(a), t[1] + 4.0 * math.sin(a)
        turn = r.uniform(-0.035, 0.035)
        pts = [(x, y)]
        for _ in range(30):
            a += turn + r.uniform(-0.01, 0.01)
            x, y = x + math.cos(a), y + math.sin(a)
            pts.append((x, y))
        out.append(pts)
    return out


def tracks(x, y):
    """0..1: as duas faixas da esteira (55 cm, 2,7 m entre elas) com as sapatas marcando, sumindo no fim.
    x, y: a grade regular da planta (meshgrid); cada trecho só mexe na janelinha dele."""
    out = np.zeros(x.shape, np.float32)
    xs_, ys_ = x[0, :], y[:, 0]
    for pts in track_paths():
        for i, ((ax, ay), (bx, by)) in enumerate(zip(pts[:-1], pts[1:])):
            c0, c1 = np.searchsorted(xs_, min(ax, bx) - 3), np.searchsorted(xs_, max(ax, bx) + 3)
            r0, r1 = np.searchsorted(ys_, min(ay, by) - 3), np.searchsorted(ys_, max(ay, by) + 3)
            if c1 <= c0 or r1 <= r0:
                continue
            xs, ys = x[r0:r1, c0:c1], y[r0:r1, c0:c1]
            ex, ey = bx - ax, by - ay
            L = math.hypot(ex, ey)
            t = np.clip(((xs - ax) * ex + (ys - ay) * ey) / (L * L), 0, 1)
            side = ((xs - ax) * ey - (ys - ay) * ex) / L                     # distância com sinal ao eixo
            along = (i + t) * L
            band = np.maximum(1 - np.abs(np.abs(side) - 1.35) / 0.3, 0) * (t > 0) * (t < 1)
            shoe = 0.75 + 0.25 * (np.mod(along / 0.16, 1.0) < 0.5)
            fade = np.clip(1 - along / 30.0, 0, 1) ** 0.7
            o = out[r0:r1, c0:c1]
            out[r0:r1, c0:c1] = np.maximum(o, np.clip(band * 1.6, 0, 1) * shoe * fade)
    return out


def zones(x, y):
    """As zonas do chão (0..1 cada): liso, ondulado, pedregoso, revirado; o resto é o pisado (a v1). Formas de 3–15 m
    com a borda entortada (o vento e o pisoteio não fazem círculos), mesma semente → o mesmo mundo em todo PC."""
    wx = x + (fbm(x, y, 9.0, 301, 3) - 0.5) * 7.0
    wy = y + (fbm(x, y, 9.0, 302, 3) - 0.5) * 7.0
    nA, nB = fbm(wx, wy, 11.0, 311, 3), fbm(wx, wy, 6.5, 312, 3)
    nC, nD = fbm(wx, wy, 14.0, 313, 3), fbm(wx, wy, 4.5, 314, 3)
    zs = getattr(look_scene, 'ZONES', {})
    smooth = smoothstep(*zs.get('smooth', (0.56, 0.66)), nA)
    gravel = smoothstep(*zs.get('gravel', (0.58, 0.68)), nB) * (1 - smooth)
    ripple = smoothstep(*zs.get('ripple', (0.64, 0.72)), nC) * (1 - smooth) * (1 - gravel)
    stir = smoothstep(*zs.get('stir', (0.66, 0.74)), nD) * (1 - ripple)
    return smooth, ripple, gravel, stir


def save_zones_texture(tx, ty):
    s, r, g, d = zones(tx[::2, ::2], ty[::2, ::2])                         # 2048² (4 cm/px numa planta de 80 m)
    rgba = np.dstack([s, r, g, d])
    Image.fromarray((np.clip(rgba, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGBA').save(os.path.join(OUT, f'T_LL_Zones{SUFFIX}.png'))
    print(f'LOOKGROUND zonas: liso {100 * (s > 0.5).mean():.0f}%, ondulado {100 * (r > 0.5).mean():.0f}%, '
          f'pedregoso {100 * (g > 0.5).mean():.0f}%, revirado {100 * (d > 0.5).mean():.0f}%')


def track_relief(x, y):
    """A trilha da esteira na areia (12/10, o estudo + a 2ª opinião): o relevo raso de verdade, em metros, e a idade.
    Na areia firme a esteira não afunda: as sapatas (17 cm) deixam o sulco de ~2 cm com as garras marcando mais fundo,
    e a areia empurrada vira uma bordinha de ~2 cm pros lados. Com a idade (o vento), o sulco enche, a bordinha amolece
    e a ondulação volta por cima. Devolve (altura, frescor 0..1, máscara 0..1)."""
    h = np.zeros(x.shape, np.float32)
    fresh = np.zeros(x.shape, np.float32)
    mask = np.zeros(x.shape, np.float32)
    xs_, ys_ = x[0, :], y[:, 0]
    paths = [(pts, 0.0) for pts in track_paths()] + list(getattr(look_scene, 'TRACKS_EXTRA', []))
    # da mais velha pra mais nova: a esteira que passou depois redesenha o chão por cima (no cruzamento, a mais nova
    # manda; o "maior em módulo" de antes desenhava um anel de sombra onde duas trilhas se cruzavam)
    paths.sort(key=lambda q: -q[1])
    for pts, age in paths:
        depth = 0.022 * (1.0 - 0.45 * age)      # a velha ainda marca (o Felipe, 13/10: "a C mais marcada")
        berm = 0.02 * (1.0 - 0.3 * age)
        bw = 0.09 * (1.0 + 1.2 * age)
        P = np.array(pts, np.float64)
        c0 = np.searchsorted(xs_, P[:, 0].min() - 2.5); c1 = np.searchsorted(xs_, P[:, 0].max() + 2.5)
        r0 = np.searchsorted(ys_, P[:, 1].min() - 2.5); r1 = np.searchsorted(ys_, P[:, 1].max() + 2.5)
        if c1 <= c0 or r1 <= r0:
            continue
        xs, ys = x[r0:r1, c0:c1], y[r0:r1, c0:c1]
        # a linha contínua (12/10: o perfil por trecho deixava um degrau em cada emenda, e a sombra assada virava um
        # pontinho preto a cada ~1 m): pra cada ponto, o trecho mais perto dá a distância com sinal e o "ao longo"
        best = np.full(xs.shape, 1e9); side_b = np.zeros(xs.shape); along_b = np.zeros(xs.shape)
        along0 = 0.0
        for (ax, ay), (bx, by) in zip(P[:-1], P[1:]):
            ex, ey = bx - ax, by - ay
            L = math.hypot(ex, ey)
            t = ((xs - ax) * ex + (ys - ay) * ey) / (L * L)
            tc = np.clip(t, 0, 1)
            dist = np.hypot(xs - (ax + ex * tc), ys - (ay + ey * tc))
            take = dist < best
            best[take] = dist[take]
            side_b[take] = (((xs - ax) * ey - (ys - ay) * ex) / L)[take]
            along_b[take] = (along0 + tc * L)[take]
            along0 += L
        total = along0
        # as pontas da trilha somem macias (o corte seco desenhava um anel de sombra em cada ponta)
        inside = (best < 2.5) * smoothstep(0.0, 1.2, along_b) * smoothstep(total, total - 1.2, along_b)
        fade = np.clip(1 - along_b / 34.0, 0, 1) ** 0.6
        d = np.abs(np.abs(side_b) - 1.425)                                     # da linha do meio de cada esteira (bitola 2,85 m, 64 cm: o tanque)
        band = smoothstep(0.34, 0.27, d)
        shoe = np.mod(along_b / 0.17, 1.0)
        grouser = np.exp(-((shoe - 0.5) / 0.09) ** 2)                          # a garra da sapata, mais funda
        core = smoothstep(0.30, 0.16, d)                                       # a garra só no miolo da fita: na borda a marca é contínua
        hh = -depth * band * (0.75 + 0.45 * grouser * core * (1.0 - 0.8 * age))  # (a garra na borda fazia a sombra pontilhada)
        hh += berm * np.exp(-((d - 0.31 - bw * 0.6) / bw) ** 2)                # a bordinha de areia empurrada
        hh *= fade * inside * (0.65 + 0.7 * fbm(xs, ys, 1.6, 91, 2))           # a marca nunca é uniforme (areia mais firme ou mais solta)
        cover = (smoothstep(0.6, 0.38, d) * fade * inside).astype(np.float32)
        h[r0:r1, c0:c1] = h[r0:r1, c0:c1] * (1.0 - cover) + hh.astype(np.float32)
        fr = band * fade * inside * (1.0 - age)
        np.maximum(fresh[r0:r1, c0:c1], fr.astype(np.float32), out=fresh[r0:r1, c0:c1])
        np.maximum(mask[r0:r1, c0:c1], (smoothstep(0.5, 0.3, d) * fade * inside).astype(np.float32), out=mask[r0:r1, c0:c1])
    return h, fresh, mask


def sun_dir():
    """Pra onde está o sol (x, y no chão) e a altura: da receita da cena e do giro da câmera da arte (como o look_lab)."""
    import json
    rname = list(getattr(look_scene, 'VARIANTS', {'x': 'desert_golden_hour'}).values())[0]
    R = json.load(open(os.path.join(HERE, 'look_recipes', rname + '.json'), encoding='utf-8'))
    S = R['sun']
    cam = look_scene.CAM
    yaw = math.radians(cam['yaw'] - S['from_camera'])
    return math.cos(yaw), math.sin(yaw), math.radians(S['elevation'])


def save_track_texture(tx, ty):
    """T_LL_Tracks: a trilha assada (R, G = a normal x, y; B = a visibilidade do sol, macia; A = o frescor da areia revirada)."""
    h, fresh, mask = track_relief(tx, ty)
    px = SIZE / TEX
    for _ in range(2):                                                     # tira os degraus de 1 px nas emendas dos trechos
        h = (4 * h + np.roll(h, 1, 0) + np.roll(h, -1, 0) + np.roll(h, 1, 1) + np.roll(h, -1, 1)) / 8.0
    gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) / (2 * px)
    gy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) / (2 * px)
    k = 1 / np.sqrt(gx ** 2 + gy ** 2 + 1)
    nx, ny = -gx * k, -gy * k
    sx, sy, el = sun_dir()
    vis = np.ones_like(h)
    occ = np.zeros_like(h)
    for step in np.arange(0.5, 16.0, 0.5):                                 # até 30 cm na direção do sol, em meio pixel
        fx_, fy_ = sx * step, sy * step                                    # (13/10: o passo inteiro com a borda dura desenhava a
        x0, y0_ = int(math.floor(fx_)), int(math.floor(fy_))               #  sombra da bordinha pontilhada: os "pontinhos")
        ax_, ay_ = fx_ - x0, fy_ - y0_
        r00 = np.roll(np.roll(h, -y0_, 0), -x0, 1); r10 = np.roll(r00, -1, 1)
        r01 = np.roll(r00, -1, 0); r11 = np.roll(r10, -1, 0)
        hk = (r00 * (1 - ax_) + r10 * ax_) * (1 - ay_) + (r01 * (1 - ax_) + r11 * ax_) * ay_
        occ = np.maximum(occ, (hk - h - step * px * math.tan(el)) / 0.006)  # penumbra de ~6 mm: a sombra da areia é macia
    vis = np.clip(1 - occ, 0, 1)
    for _ in range(2):
        vis = (4 * vis + np.roll(vis, 1, 0) + np.roll(vis, -1, 0) + np.roll(vis, 1, 1) + np.roll(vis, -1, 1)) / 8.0
    rgba = np.dstack([nx * 0.5 + 0.5, ny * 0.5 + 0.5, vis, np.clip(fresh, 0, 1)])
    Image.fromarray((np.clip(rgba, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGBA').save(os.path.join(OUT, f'T_LL_Tracks{SUFFIX}.png'))
    print(f'LOOKGROUND trilhas: {int((mask > 0.5).sum() * px * px)} m², sombra em {100 * ((vis < 0.5) & (mask > 0.1)).sum() / max((mask > 0.1).sum(), 1):.0f}% da trilha')


def main():
    # a planta pintada (o material lê pela posição)
    s = (np.arange(TEX, dtype=np.float32) + 0.5) / TEX * SIZE
    tx, ty = np.meshgrid(X0 + s, Y0 + s)
    _, asphalt, cover, dirt, cA, hA = fields(tx, ty)
    if getattr(look_scene, 'ZONES', None) is not None:
        save_zones_texture(tx, ty)
    if getattr(look_scene, 'TRACK_RELIEF', False):
        save_track_texture(tx, ty)             # a trilha com relevo (a versão B/C da areia); a planta não pinta mais a esteira
    else:
        dirt = np.maximum(dirt, 0.9 * tracks(tx, ty) * (asphalt < 0.5))    # as marcas de esteira escurecem a areia
    pt = paint(tx, ty, cA, hA) * asphalt * (1.0 - cover)
    if ROAD_SLAB:      # a areia por cima da laje onde o monte de areia passa do topo dela (a planta pinta areia ali)
        hg_t = fields(tx, ty)[0]
        cover = np.maximum(cover, np.clip((hg_t - slab_top(tx, ty) + 0.03) / 0.05, 0, 1) * (asphalt > 0.5))
    if ROAD_SLAB:      # (v5) a faixa pintada some onde a beira da laje quebrou
        dk = road_fields(tx, ty)[0]
        pt = pt * (1.0 - edge_break(tx, ty) * (1.0 - smoothstep(0.6, 1.6, -dk)))
    lay = np.stack([asphalt, cover, dirt, pt], -1)
    if getattr(look_scene, 'ASPHALT_V2', False):
        save_road_frame(tx, ty)
    Image.fromarray((lay * 255 + 0.5).astype(np.uint8), 'RGBA').save(os.path.join(OUT, f'T_LL_Layout{SUFFIX}.png'))

    # a malha
    n = int(round(SIZE / MESH_STEP)) + 1
    s = np.arange(n, dtype=np.float32) * MESH_STEP
    mx, my = np.meshgrid(X0 + s, Y0 + s)
    h = fields(mx, my)[0] + (crater_height(mx, my) if CRATERS else 0.0)
    rim = smoothstep(0.0, 6.0, np.minimum.reduce([mx - X0, X0 + SIZE - mx, my - Y0, Y0 + SIZE - my]))
    h = h * rim + OUTSIDE_Z * (1.0 - rim)
    dhdx = np.gradient(h, MESH_STEP, axis=1)
    dhdy = np.gradient(h, MESH_STEP, axis=0)
    nr = np.stack([-dhdx, -dhdy, np.ones_like(h)], -1)
    nr /= np.linalg.norm(nr, axis=-1, keepdims=True)
    pos = np.stack([mx, my, h], -1).reshape(-1, 3)
    j, i = np.mgrid[0:n - 1, 0:n - 1]
    a = (j * n + i).ravel()
    b = ((j + 1) * n + i).ravel()     # +Y da Unreal (= +Z do glTF)
    c = (j * n + i + 1).ravel()       # +X
    d = ((j + 1) * n + i + 1).ravel()
    idx = np.stack([np.stack([a, b, c], 1), np.stack([c, b, d], 1)], 1).reshape(-1, 3)
    pos = pos.astype(np.float32)
    nr = nr.reshape(-1, 3)
    if CRATERS:
        save_crater_texture()
        ins = crater_inside(pos[:, 0], pos[:, 1], 0.4)
        idx = idx[~(ins[idx[:, 0]] & ins[idx[:, 1]] & ins[idx[:, 2]])]         # o buraco: só os triângulos todo dentro
        for pp, nn, ii in crater_patches():
            idx = np.concatenate([idx, ii + len(pos)])
            pos = np.concatenate([pos, pp])
            nr = np.concatenate([nr, nn])
        h = np.concatenate([h.ravel(), pos[len(h.ravel()):, 2]])
    if ROAD_SLAB:
        pp, nn, ii = road_slab()
        idx = np.concatenate([idx, ii + len(pos)])
        pos = np.concatenate([pos, pp])
        nr = np.concatenate([nr, nn])
        h = np.concatenate([np.ravel(h), pp[:, 2]])
    # só regrava a malha se o relevo mudou (a Unreal reimporta e refaz o Nanite: ~1,5 min)
    import hashlib
    digest = hashlib.sha1(h.tobytes()).hexdigest()
    gj = os.path.join(OUT, 'ground.json')
    old = json.load(open(gj)).get('relief') if os.path.exists(gj) else None
    if old != digest or not os.path.exists(os.path.join(OUT, f'SM_LL_Ground{SUFFIX}.glb')):
        write_glb(os.path.join(OUT, f'SM_LL_Ground{SUFFIX}.glb'), pos, nr, idx)
    place(os.path.join(OUT, 'placements.json'))
    with open(os.path.join(OUT, 'ground.json'), 'w') as f:
        json.dump({'x0': X0, 'y0': Y0, 'size': SIZE, 'outside_z': OUTSIDE_Z, 'relief': digest,
                   'craters': [dict(c, span=CRATER_SPAN, range=CRATER_RANGE) for c in CRATERS]}, f)
    print(f'LOOKGROUND ok: {n}×{n} vértices, {len(idx)} triângulos, altura {h.min():.2f}..{h.max():.2f} m, '
          f'asfalto {asphalt.mean():.2f}, areia por cima {cover.mean():.3f}')


main()
