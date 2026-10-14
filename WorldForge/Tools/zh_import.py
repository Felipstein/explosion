"""Traz um mapa do C&C Generals Zero Hour (.map) pro WorldForge.

  python WorldForge/Tools/zh_import.py "<arquivo .map>" [nome]

Lê o .map (chunks "CkMp" da EA, comprimido com RefPack "EAR\\0") e grava em
WorldForge/Saved/WorldForge/imported/<nome>/:
  map.json    bioma, tamanho, cota d'água, bases, campos de recurso, árvores,
              pedras e o que ficou de fora (prédios, sons, decalques)
  height.bin  relevo (float32, N×N, m; linha J = eixo Y do Unreal)
  layers.bin  camadas do gerador (RGBA uint8, N×N: R terra, G vegetação,
              B margem, A rocha), tiradas das texturas pintadas no mapa
  edits.json  o que vira edição à mão (FWorldEdits): as estradas e o concreto

O app abre pela lista de mapas ("... (Zero Hour)"); o relevo, as texturas e as
peças passam pelo nosso acabamento (chão, espalhamento, estradas de verdade).
Só pra estudo: os mapas têm dono (GeneralsRankedMaps).

Unidades: a célula do Zero Hour tem 10 unidades do mundo (≈ 10 pés, 3,048 m) e
a altura é byte × 0,625 unidades. O eixo y dele vira o X do Unreal e o x vira o
Y: visto de cima, o mapa fica igual ao do jogo (o minimapa do WorldForge tem +X
pra cima e +Y pra direita).
"""
import json
import os
import re
import struct
import sys
from collections import Counter, defaultdict

import numpy as np

FT = 0.3048           # metros por unidade do mundo do Zero Hour
CELL_UNITS = 10.0     # unidades por célula
HEIGHT_UNITS = 0.625  # unidades por degrau de altura
OUT_CELL = 2.0        # a grade do WorldForge (m)


# ---- leitura do .map (o mesmo de captures/reference/zh_maps.py) -------------

def refpack(data):
    pos = 0
    flags = (data[0] << 8) | data[1]
    pos = 2
    n = 4 if flags & 0x8000 else 3
    if flags & 0x0100:
        pos += n
    size = int.from_bytes(data[pos:pos + n], 'big')
    pos += n
    out = bytearray()
    while pos < len(data):
        b0 = data[pos]
        if b0 < 0x80:
            b1 = data[pos + 1]
            plain, copy, off = b0 & 3, ((b0 & 0x1C) >> 2) + 3, ((b0 & 0x60) << 3) + b1 + 1
            pos += 2
        elif b0 < 0xC0:
            b1, b2 = data[pos + 1], data[pos + 2]
            plain, copy, off = b1 >> 6, (b0 & 0x3F) + 4, ((b1 & 0x3F) << 8) + b2 + 1
            pos += 3
        elif b0 < 0xE0:
            b1, b2, b3 = data[pos + 1], data[pos + 2], data[pos + 3]
            plain, copy, off = b0 & 3, ((b0 & 0x0C) << 6) + b3 + 5, ((b0 & 0x10) << 12) + (b1 << 8) + b2 + 1
            pos += 4
        elif b0 < 0xFC:
            plain, copy, off = ((b0 & 0x1F) << 2) + 4, 0, 0
            pos += 1
        else:
            plain, copy, off = b0 & 3, 0, 0
            pos += 1
            out += data[pos:pos + plain]
            break
        out += data[pos:pos + plain]
        pos += plain
        for _ in range(copy):
            out.append(out[-off])
    return bytes(out[:size])


def load(path):
    raw = open(path, 'rb').read()
    if raw[:4] == b'EAR\x00':
        raw = refpack(raw[8:])
    assert raw[:4] == b'CkMp', path
    pos = 4
    count = struct.unpack_from('<i', raw, pos)[0]
    pos += 4
    names = {}
    for _ in range(count):
        ln = raw[pos]
        name = raw[pos + 1:pos + 1 + ln].decode('latin1')
        cid = struct.unpack_from('<I', raw, pos + 1 + ln)[0]
        names[cid] = name
        pos += 1 + ln + 4
    return raw, pos, names


def chunks(raw, pos, end, names):
    while pos + 10 <= end:
        cid, ver, size = struct.unpack_from('<IHi', raw, pos)
        pos += 10
        yield names.get(cid, '?'), ver, pos, size
        pos += size


def string16(raw, pos):
    ln = struct.unpack_from('<H', raw, pos)[0]
    return raw[pos + 2:pos + 2 + ln].decode('latin1'), pos + 2 + ln


def read_dict(raw, q, names):
    """As propriedades de um objeto (Dict da EA): chave → valor."""
    n = struct.unpack_from('<H', raw, q)[0]
    q += 2
    d = {}
    for _ in range(n):
        kt = struct.unpack_from('<i', raw, q)[0]
        q += 4
        key, t = names.get(kt >> 8, '?'), kt & 0xFF
        if t == 0:
            v = raw[q] != 0
            q += 1
        elif t in (1, 2):
            v = struct.unpack_from('<i' if t == 1 else '<f', raw, q)[0]
            q += 4
        elif t == 3:
            v, q = string16(raw, q)
        elif t == 4:
            ln = struct.unpack_from('<H', raw, q)[0]
            v = raw[q + 2:q + 2 + 2 * ln].decode('utf-16-le')
            q += 2 + 2 * ln
        else:
            raise ValueError('tipo de propriedade %d' % t)
        d[key] = v
    return d, q


def parse(path):
    raw, pos, names = load(path)
    out = {'objects': [], 'water': []}
    for name, ver, p, size in chunks(raw, pos, len(raw), names):
        if name == 'HeightMapData':
            w, h, border = struct.unpack_from('<iii', raw, p)
            q = p + 12
            out['playable'] = (w - 2 * border, h - 2 * border)
            if ver >= 4:
                nb = struct.unpack_from('<i', raw, q)[0]
                if nb:
                    out['playable'] = struct.unpack_from('<ii', raw, q + 4)   # a 1ª fronteira (células)
                q += 4 + 8 * nb
            dsize = struct.unpack_from('<i', raw, q)[0]
            q += 4
            out['w'], out['h'], out['border'] = w, h, border
            out['heights'] = np.frombuffer(raw[q:q + dsize], dtype=np.uint8).reshape(h, w).astype(np.float32)
        elif name == 'BlendTileData':
            out['tiles'] = (ver, p)
        elif name == 'ObjectsList':
            for oname, over, op, osize in chunks(raw, p, p + size, names):
                if oname != 'Object':
                    continue
                x, y, z, ang, flags = struct.unpack_from('<ffffi', raw, op)
                tname, q = string16(raw, op + 20)
                props, _ = read_dict(raw, q, names)
                out['objects'].append({'x': x, 'y': y, 'z': z, 'angle': ang, 'flags': flags, 'type': tname, 'props': props})
        elif name == 'PolygonTriggers':
            q = p
            n = struct.unpack_from('<i', raw, q)[0]
            q += 4
            for _ in range(n):
                _tname, q = string16(raw, q)
                if ver >= 4:
                    _layer, q = string16(raw, q)
                q += 4
                is_water = False
                if ver >= 2:
                    is_water = raw[q] != 0
                    q += 1
                if ver >= 3:
                    q += 1 + 4
                npts = struct.unpack_from('<i', raw, q)[0]
                q += 4
                pts = [struct.unpack_from('<iii', raw, q + 12 * i) for i in range(npts)]
                q += 12 * npts
                if is_water and pts:
                    out['water'].append(pts)
    # texturas: o índice do ladrilho de cada vértice → a classe (o nome da textura)
    ver, p = out['tiles']
    w, h = out['w'], out['h']
    q = p
    n = struct.unpack_from('<i', raw, q)[0]
    q += 4
    tiles = np.frombuffer(raw[q:q + 2 * n], dtype=np.uint16).reshape(h, w)
    q += 2 * n          # ladrilhos
    q += 2 * n          # mistura
    if ver >= 6:
        q += 2 * n      # mistura extra
    if ver >= 5:
        q += 2 * n      # penhasco
    if ver >= 7:
        q += h * ((w + 7) // 8)   # bits de estado do penhasco
    _nbitmap, _nblend, ncliff, nclasses = struct.unpack_from('<iiii', raw, q)
    if ver < 5:
        q -= 4
    q += 16
    classes = []
    for _ in range(nclasses):
        first, num, _width, _legacy = struct.unpack_from('<iiii', raw, q)
        q += 16
        cname, q = string16(raw, q)
        classes.append((first, num, cname))
    cls = np.full((h, w), -1, dtype=np.int32)
    t = tiles.astype(np.int32) >> 2
    for k, (first, num, _cname) in enumerate(classes):
        cls[(t >= first) & (t < first + num)] = k
    out['classes'] = [c[2] for c in classes]
    out['class_grid'] = cls
    return out


# ---- o que cada coisa do Zero Hour vira aqui ---------------------------------

SAND, DIRT, GRASS, ROCK, CONCRETE, SNOW = range(6)
MARGIN_CELLS = 30     # borda em volta da área jogável que vem junto (~90 m de moldura)


def texture_kind(name):
    n = name.lower()
    if 'concrete' in n or 'asphalt' in n or 'road' in n or 'pavement' in n:
        return CONCRETE
    if 'cliff' in n or 'rock' in n or 'stone' in n:
        return ROCK
    if 'snow' in n or 'ice' in n:
        return SNOW
    if 'dirt' in n or 'mud' in n or 'crack' in n:
        return DIRT
    if 'grass' in n and 'grassy' not in n:
        return GRASS
    return SAND


def object_kind(name):
    n = name.lower()
    if n.startswith('amb_'):
        return 'sound'
    if 'waypoint' in n:
        return 'waypoint'
    if 'supply' in n:
        return 'supply'
    if 'oilderrick' in n:
        return 'oil'
    if 'shrub' in n or 'bush' in n:
        return 'shrub'
    if 'palm' in n:
        return 'palm'
    if 'tree' in n or 'pine' in n or 'fir' in n:
        return 'tree'
    if 'rock' in n or 'boulder' in n or 'stone' in n:
        return 'rock'
    return None


def blur(a, passes):
    """[1 2 1]/4 nos dois eixos, Passes vezes (borda repetida)."""
    for _ in range(passes):
        p = np.pad(a, 1, mode='edge')
        a = (p[:-2, 1:-1] + 2 * p[1:-1, 1:-1] + p[2:, 1:-1]) / 4.0
        p = np.pad(a, 1, mode='edge')
        a = (p[1:-1, :-2] + 2 * p[1:-1, 1:-1] + p[1:-1, 2:]) / 4.0
    return a


def bilinear(grid, y, x):
    """Amostra grid[y, x] (frações) com interpolação bilinear."""
    h, w = grid.shape
    x = np.clip(x, 0, w - 1.0001)
    y = np.clip(y, 0, h - 1.0001)
    x0, y0 = np.floor(x).astype(int), np.floor(y).astype(int)
    fx, fy = x - x0, y - y0
    a = grid[y0, x0] * (1 - fx) + grid[y0, x0 + 1] * fx
    b = grid[y0 + 1, x0] * (1 - fx) + grid[y0 + 1, x0 + 1] * fx
    return a * (1 - fy) + b * fy


def inside(px, py, pts):
    c = False
    for k in range(len(pts)):
        ax, ay = pts[k]
        bx, by = pts[k - 1]
        if (ay > py) != (by > py) and px < (bx - ax) * (py - ay) / ((by - ay) or 1e-9) + ax:
            c = not c
    return c


def convert(path, slug):
    m = parse(path)
    w, h, border = m['w'], m['h'], m['border']
    pw, ph = m['playable']
    # o recorte: a área jogável + ~90 m de moldura (a borda larga do Zero Hour é
    # cenário que a câmera não alcança; os autores escrevem nela, de concreto)
    x0, y0 = max(0, border - MARGIN_CELLS), max(0, border - MARGIN_CELLS)
    x1, y1 = min(w - 1, border + pw + MARGIN_CELLS), min(h - 1, border + ph + MARGIN_CELLS)
    hc = m['heights'][y0:y1 + 1, x0:x1 + 1]
    cg = m['class_grid'][y0:y1 + 1, x0:x1 + 1]
    ch, cw = hc.shape
    S = max(cw, ch)
    extent = (S - 1) * CELL_UNITS * FT          # lado do mapa (m)
    N = int(round(extent / OUT_CELL)) + 1
    cell = extent / (N - 1)
    half = 0.5 * extent
    # centraliza a área do recorte no quadrado
    ox, oy = 0.5 * (S - cw), 0.5 * (S - ch)

    # do mundo do Zero Hour (unidades, origem no canto da área jogável) pro
    # nosso (m, centro no meio): x → Y, y → X
    def to_ue(x, y):
        return ((y / CELL_UNITS + border - y0 + oy) * CELL_UNITS * FT - half, (x / CELL_UNITS + border - x0 + ox) * CELL_UNITS * FT - half)

    def in_play(x, y, pad=0.0):
        return -pad <= x <= pw * CELL_UNITS + pad and -pad <= y <= ph * CELL_UNITS + pad

    # relevo: completa o quadrado repetindo a borda, reamostra na grade de 2 m
    # (eixos trocados) e suaviza ~2 m (o byte de altura dá degraus de 19 cm a
    # cada 3 m, que viravam terraços com o sol baixo)
    py0, px0 = int(np.floor(oy)), int(np.floor(ox))
    hz = np.pad(hc * HEIGHT_UNITS * FT, ((py0, S - ch - py0), (px0, S - cw - px0)), mode='edge')
    J, I = np.mgrid[0:N, 0:N].astype(np.float64)
    zh_x = J * cell / (CELL_UNITS * FT)   # coluna do Zero Hour (x) = nosso Y
    zh_y = I * cell / (CELL_UNITS * FT)   # linha do Zero Hour (y) = nosso X
    height = blur(bilinear(hz, zh_y, zh_x), 1)

    # texturas → camadas do gerador (o mais perto, depois borda suave de ~4 m)
    kinds_full = np.vectorize(lambda k: texture_kind(m['classes'][k]) if k >= 0 else SAND)(m['class_grid'])
    # concreto só dentro da área jogável (fora dela são as anotações do autor)
    jy, ix = np.mgrid[0:h, 0:w]
    outside = (ix < border) | (ix > border + pw) | (jy < border) | (jy > border + ph)
    kinds_full[outside & (kinds_full == CONCRETE)] = SAND
    kinds = np.pad(kinds_full[y0:y1 + 1, x0:x1 + 1], ((py0, S - ch - py0), (px0, S - cw - px0)), mode='edge')
    nearest = kinds[np.clip(np.rint(zh_y).astype(int), 0, S - 1), np.clip(np.rint(zh_x).astype(int), 0, S - 1)]
    count = Counter(nearest.ravel().tolist())
    ground = [count.get(k, 0) for k in (SAND, DIRT, GRASS, SNOW)]
    biome = 0 if ground[2] == max(ground) else (2 if ground[3] == max(ground) else 1)   # 0 temperado, 1 deserto, 2 neve
    one = lambda k: blur((nearest == k).astype(np.float64), 2)
    layers = np.zeros((N, N, 4), dtype=np.uint8)
    layers[..., 0] = np.clip(255 * (one(DIRT) + one(CONCRETE)), 0, 255)
    layers[..., 1] = np.clip(255 * one(GRASS), 0, 255)
    layers[..., 3] = np.clip(255 * one(ROCK), 0, 255)

    # água: a cota do polígono mais alto. A nossa água é uma cota só (o plano
    # cobre o mapa); no Zero Hour o polígono diz onde tem água e o chão fora dele
    # pode ficar abaixo da cota. Pouco chão assim: sobe pra ficar seco; muito
    # (o Desert Fury tem 23% do mapa abaixo da cota do laguinho): a água fica de
    # fora, senão o vale inteiro alagava
    water_level, raised, water_note = -1000.0, 0, ''
    if m['water']:
        level = max(max(z for _, _, z in pts) for pts in m['water']) * FT
        polys = [[to_ue(x, y) for x, y, _ in pts] for pts in m['water']]
        low = np.argwhere(height < level + 0.3)
        dry = [(jj, ii) for jj, ii in low if not any(inside(ii * cell - half, jj * cell - half, poly) for poly in polys)]
        if len(dry) <= 0.03 * N * N:
            water_level = level
            for jj, ii in dry:
                height[jj, ii] = level + 0.3
            raised = len(dry)
        else:
            water_note = 'água de fora: %.0f%% do mapa fica abaixo da cota (%.1f m) fora dos polígonos' % (100.0 * len(dry) / (N * N), level)

    # peças
    starts, resources, props = [], [], []
    dropped = Counter()
    road_pts = []
    rng = np.random.default_rng(7)
    for o in m['objects']:
        X, Y = to_ue(o['x'], o['y'])
        if not in_play(o['x'], o['y'], MARGIN_CELLS * CELL_UNITS - 5.0):
            continue   # fora do recorte
        if o['flags'] & 0x6:
            road_pts.append((o, X, Y))
            continue
        k = object_kind(o['type'])
        if k == 'waypoint':
            wn = o['props'].get('waypointName', '')
            g = re.match(r'Player_(\d+)_Start', wn)
            if g:
                starts.append([int(g.group(1)) - 1, X, Y])
        elif k == 'supply':
            resources.append([X, Y, 18.0, 1.0])
        elif k == 'oil':
            resources.append([X, Y, 8.0, 0.5])
        elif k in ('palm', 'tree', 'shrub'):
            # deserto: árvore vira tamareira (a nossa árvore de lá; Variant ≥ 128),
            # moita vira arbusto. Temperado/neve: o tipo do gerador (pinheiro pelo bit 8)
            if biome == 1:
                variant = 128 + int(rng.integers(0, 64)) if k != 'shrub' else int(rng.integers(0, 64))
            else:
                variant = (8 if 'pine' in o['type'].lower() or 'fir' in o['type'].lower() else 0) | int(rng.integers(0, 8)) * 16
            props.append(['tree', X, Y, float(rng.uniform(0.85, 1.15)), float(rng.uniform(0, 360)), variant])
        elif k == 'rock':
            big = 'cluster' in o['type'].lower() or 'large' in o['type'].lower()
            props.append(['rock', X, Y, 1.4 if big else 1.0, float(rng.uniform(0, 360)), int(rng.integers(0, 255))])
        elif k == 'sound':
            pass
        else:
            dropped[o['type']] += 1

    # estradas: cada par (ponta 1, ponta 2) é um trecho; os trechos que se
    # encostam viram uma linha só. Decalque de rachadura fica de fora
    segs = defaultdict(list)
    for a, b in zip(road_pts, road_pts[1:]):
        if a[0]['flags'] & 0x2 and b[0]['flags'] & 0x4 and a[0]['type'] == b[0]['type']:
            segs[a[0]['type']].append(((a[1], a[2]), (b[1], b[2])))
    roads = []
    for rtype, ss in segs.items():
        lt = rtype.lower()
        if 'crack' in lt:
            dropped[rtype] += len(ss)
            continue
        paved = 'dirt' not in lt
        key = lambda p: (round(p[0] * 2), round(p[1] * 2))   # 0,5 m
        ends = defaultdict(list)
        for i, (a, b) in enumerate(ss):
            ends[key(a)].append(i)
            ends[key(b)].append(i)
        used = [False] * len(ss)

        def walk(i, p):
            # segue a linha a partir do ponto p do trecho i enquanto o nó tiver 2 trechos
            line = [p]
            while True:
                used[i] = True
                a, b = ss[i]
                q = b if key(a) == key(p) else a
                line.append(q)
                nxt = [j for j in ends[key(q)] if not used[j]]
                if len(ends[key(q)]) != 2 or not nxt:
                    return line
                i, p = nxt[0], q

        # começa pelas pontas (nó com 1 trecho) e pelos cruzamentos; o que sobrar é anel
        order = sorted(range(len(ss)), key=lambda i: -max(len(ends[key(ss[i][0])]) != 2, len(ends[key(ss[i][1])]) != 2))
        for i in order:
            if used[i]:
                continue
            a, b = ss[i]
            start = a if len(ends[key(a)]) != 2 else b
            line = walk(i, start)
            pts = [line[0]]
            for p in line[1:]:
                if (p[0] - pts[-1][0]) ** 2 + (p[1] - pts[-1][1]) ** 2 > 1.0:
                    pts.append(p)
            if len(pts) >= 2:
                roads.append({'paved': paved, 'fade': [False, False], 'points': [[round(x, 2), round(y, 2)] for x, y in pts]})

    # concreto (pátio): pintura de asfalto (camada 5) nas células de 1 m
    paint = []
    conc = np.argwhere(kinds_full == CONCRETE)
    cells = set()
    for jy, ix in conc:   # linha (y) e coluna (x) do Zero Hour
        X0, Y0 = to_ue((ix - border) * CELL_UNITS, (jy - border) * CELL_UNITS)
        r = 0.5 * CELL_UNITS * FT
        for cx in range(int(np.floor(X0 - r)), int(np.ceil(X0 + r))):
            for cy in range(int(np.floor(Y0 - r)), int(np.ceil(Y0 + r))):
                cells.add((cx, cy))
    paint = [[cx, cy, 5, 255] for cx, cy in sorted(cells)]

    folder = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Saved', 'WorldForge', 'imported', slug)
    os.makedirs(folder, exist_ok=True)
    height.astype(np.float32).tofile(os.path.join(folder, 'height.bin'))
    layers.tofile(os.path.join(folder, 'layers.bin'))
    title = re.sub(r'\s*(ZH)?\s*v\d+$', '', re.sub(r'^\[RANK\]\s*', '', os.path.splitext(os.path.basename(path))[0])).strip()
    meta = {
        'name': slug,
        'title': title,
        'source': os.path.basename(path),
        'biome': biome,
        'size_m': extent,
        'n': N,
        'cell': cell,
        'water_level': water_level,
        'playable_m': [pw * CELL_UNITS * FT, ph * CELL_UNITS * FT],
        'starts': starts,
        'resources': resources,
        'props': props,
        'dropped': dict(dropped.most_common()),
        'raised_dry_cells': int(raised),
        'water_note': water_note,
        'textures': {c: texture_kind(c) for c in m['classes']},
    }
    with open(os.path.join(folder, 'map.json'), 'w', encoding='utf-8') as f:
        json.dump(meta, f, indent=1)
    edits = {'deleted': [], 'paint': paint, 'height': [], 'roads': roads, 'removed_roads': [], 'moved': []}
    with open(os.path.join(folder, 'edits.json'), 'w', encoding='utf-8') as f:
        json.dump(edits, f)
    print('%s: %.0f m (%d×%d, célula %.2f m), bioma %d, água %.1f m (%d células secas), %d bases, %d campos, %d árvores, %d pedras, %d estradas, %d m² de concreto'
          % (slug, extent, N, N, cell, biome, water_level, raised, len(starts), len(resources),
             sum(1 for p in props if p[0] == 'tree'), sum(1 for p in props if p[0] == 'rock'), len(roads), len(paint)))
    print('de fora:', dict(dropped.most_common()), water_note)
    print('relevo: %.1f–%.1f m' % (height.min(), height.max()))
    return folder


if __name__ == '__main__':
    src = sys.argv[1]
    slug = sys.argv[2] if len(sys.argv) > 2 else re.sub(r'[^A-Za-z0-9]+', '_', os.path.splitext(os.path.basename(src))[0]).strip('_')
    convert(src, slug)
