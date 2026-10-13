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
    edge = (fbm(x, y, 2.5, 10, 3) - 0.5) * 1.4
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
    near = np.minimum(np.abs(d_sand), feet)
    far_w = smoothstep(3.0, 30.0, near)
    base = np.maximum(-0.06 + far_w * (fbm(x, y, 40.0, 20, 4) - 0.5) * 0.2, -0.40)   # areia batida, quase lisa (crítica 11/10)
    base = np.where(feet < 3.0, base * smoothstep(0.0, 3.0, feet), base)      # as casas pousam no zero

    # os montes de areia nas bordas da rua (em trechos) e encostados nas paredes
    patchy = smoothstep(0.36, 0.56, fbm(x, y, 9.0, 30, 3))
    band = np.exp(-((d_sand - 0.6 + (fbm(x, y, 3.0, 31, 2) - 0.5) * 3.2) / 2.4) ** 2)   # línguas de areia na borda (crítica 11/10)
    drift = 0.32 * band * patchy
    pile = 0.35 * np.exp(-feet / 0.9) * (0.4 + 0.6 * fbm(x, y, 2.0, 32, 2)) * (feet > 0.02)

    h_sand = base + drift + pile
    h_asph = 0.012 * (crack_height(x, y) - 0.5)
    rut = 1.0 - smoothstep(0.25, 0.7, np.abs(dU - 1.2))
    h_dirt = base + 0.03 - 0.045 * rut
    h = np.where(asphalt > 0.5, np.maximum(h_asph, base + drift - 0.03), h_sand)
    h = np.where(dirt > 0.5, np.maximum(h_dirt, base + drift * 0.5), h)

    sand_cover = np.clip(np.where(asphalt > 0.5, (base + drift - 0.03 - h_asph) / 0.08 + 0.0, 0.0), 0, 1)
    sand_cover = np.maximum(sand_cover, np.clip(pile / 0.2, 0, 1))
    return h.astype(np.float32), asphalt, sand_cover.astype(np.float32), dirt, cA, hA


def paint(x, y, cA, hA):
    """As faixas pintadas (as do material antigo): tracejada no meio e contínua nas bordas."""
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
        return fields(a[:, 0], a[:, 1])[0].tolist() if len(a) else []
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
    for kind, m, scale in (('tuft', pick_t, (2.4, 4.0)), ('bush', pick_b, (0.45, 0.95)),
                           ('pebble', pick_p, getattr(look_scene, 'PEBBLE_SCALE', (2.5, 6.0))), ('twig', pick_w, (0.3, 0.6))):
        if kind not in SCATTER_KINDS:
            continue
        for i in np.nonzero(m)[0]:
            sc.append({'kind': kind, 'x': float(cx[i]), 'y': float(cy[i]), 'z': float(h[i]),
                       'yaw': float(r.uniform(0, 360)), 's': float(r.uniform(*scale)), 'v': int(r.integers(0, 99))})
    out['scatter'] = sc
    with open(path, 'w') as f:
        json.dump(out, f)
    print(f'LOOKGROUND espalhados: {int(pick_t.sum())} tufos, {int(pick_b.sum())} arbustos, {int(pick_p.sum())} pedrinhas, '
          f'{int(pick_w.sum()) if "twig" in SCATTER_KINDS else 0} gravetos')


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
    lay = np.stack([asphalt, cover, dirt, pt], -1)
    Image.fromarray((lay * 255 + 0.5).astype(np.uint8), 'RGBA').save(os.path.join(OUT, f'T_LL_Layout{SUFFIX}.png'))

    # a malha
    n = int(round(SIZE / MESH_STEP)) + 1
    s = np.arange(n, dtype=np.float32) * MESH_STEP
    mx, my = np.meshgrid(X0 + s, Y0 + s)
    h = fields(mx, my)[0]
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
    # só regrava a malha se o relevo mudou (a Unreal reimporta e refaz o Nanite: ~1,5 min)
    import hashlib
    digest = hashlib.sha1(h.tobytes()).hexdigest()
    gj = os.path.join(OUT, 'ground.json')
    old = json.load(open(gj)).get('relief') if os.path.exists(gj) else None
    if old != digest or not os.path.exists(os.path.join(OUT, f'SM_LL_Ground{SUFFIX}.glb')):
        write_glb(os.path.join(OUT, f'SM_LL_Ground{SUFFIX}.glb'), pos, nr.reshape(-1, 3), idx)
    place(os.path.join(OUT, 'placements.json'))
    with open(os.path.join(OUT, 'ground.json'), 'w') as f:
        json.dump({'x0': X0, 'y0': Y0, 'size': SIZE, 'outside_z': OUTSIDE_Z, 'relief': digest}, f)
    print(f'LOOKGROUND ok: {n}×{n} vértices, {len(idx)} triângulos, altura {h.min():.2f}..{h.max():.2f} m, '
          f'asfalto {asphalt.mean():.2f}, areia por cima {cover.mean():.3f}')


main()
