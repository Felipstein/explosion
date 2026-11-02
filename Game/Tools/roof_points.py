# Os pontos de fogo no telhado de cada prédio (30/10, os estados de dano do prédio: o fogo dos 35% de vida).
#
# O ZH põe o fogo do prédio "muito danificado" em ossos que o artista colocou no modelo (TransitionDamageFX, os FXBONE).
# Aqui os modelos são os provisórios (Tools/blender_provisional.py), sem ossos: este script mede o telhado em cada um. Lê o
# .glb que o import_provisional.py importou (captures/game/prov/<nome>/<nome>.glb), rasteriza a malha vista de cima numa grade
# de 0,5 m (a altura mais alta em cada célula) e escolhe até 2 pontos onde o telhado é plano (a altura varia < 0,3 m em
# 1,5 m) e alto (≥ 55% da altura do prédio e ≥ 3 m), com folga da borda e longe um do outro (≥ 5 m). Grava em
# Config/RTS/Units.json, no visual de cada prédio: "fire_points_m" (no quadro da malha, metros, eixos da Unreal).
#   python Tools/roof_points.py [--dry]
import json, struct, sys
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
UNITS = ROOT / 'Config' / 'RTS' / 'Units.json'
PROV = ROOT.parent / 'captures' / 'game' / 'prov'
HEIGHTS = ROOT / 'Config' / 'RTS' / 'Heights'
CELL = 0.5


def read_glb(path):
    data = path.read_bytes()
    assert data[:4] == b'glTF'
    off = 12
    js, binc = None, None
    while off < len(data):
        n, kind = struct.unpack_from('<II', data, off)
        chunk = data[off + 8: off + 8 + n]
        if kind == 0x4E4F534A:
            js = json.loads(chunk)
        elif kind == 0x004E4942:
            binc = chunk
        off += 8 + n
    return js, binc


def accessor(js, binc, i):
    a = js['accessors'][i]
    bv = js['bufferViews'][a['bufferView']]
    comp = {5126: ('<f4', 4), 5125: ('<u4', 4), 5123: ('<u2', 2), 5121: ('<u1', 1)}[a['componentType']]
    ncomp = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[a['type']]
    start = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    stride = bv.get('byteStride', comp[1] * ncomp)
    raw = np.frombuffer(binc, dtype=np.uint8, count=stride * (a['count'] - 1) + comp[1] * ncomp, offset=start)
    out = np.lib.stride_tricks.as_strided(raw, shape=(a['count'], comp[1] * ncomp), strides=(stride, 1)).copy()
    return out.view(comp[0]).reshape(a['count'], ncomp)


def node_matrix(n):
    if 'matrix' in n:
        return np.array(n['matrix'], dtype=np.float64).reshape(4, 4).T
    t = np.array(n.get('translation', [0, 0, 0]), dtype=np.float64)
    q = np.array(n.get('rotation', [0, 0, 0, 1]), dtype=np.float64)
    s = np.array(n.get('scale', [1, 1, 1]), dtype=np.float64)
    x, y, z, w = q
    r = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                  [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                  [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    m = np.eye(4)
    m[:3, :3] = r * s
    m[:3, 3] = t
    return m


def triangles(js, binc):
    tris = []
    def walk(ni, parent):
        n = js['nodes'][ni]
        m = parent @ node_matrix(n)
        if 'mesh' in n:
            for p in js['meshes'][n['mesh']]['primitives']:
                if p.get('mode', 4) != 4:
                    continue
                v = accessor(js, binc, p['attributes']['POSITION']).astype(np.float64)
                v = (np.c_[v, np.ones(len(v))] @ m.T)[:, :3]
                idx = accessor(js, binc, p['indices']).reshape(-1) if 'indices' in p else np.arange(len(v))
                tris.append(v[idx.reshape(-1, 3)])
        for c in n.get('children', []):
            walk(c, m)
    for s in js['scenes'][js.get('scene', 0)]['nodes']:
        walk(s, np.eye(4))
    t = np.concatenate(tris)
    # glTF (Y pra cima) → Unreal (Z pra cima): x, z, y (o Y da Unreal é o contrário do Blender, como no Units.json)
    return t[:, :, [0, 2, 1]]


def height_grid(t):
    lo = t.reshape(-1, 3).min(0)
    hi = t.reshape(-1, 3).max(0)
    nx = int(np.ceil((hi[0] - lo[0]) / CELL)) + 1
    ny = int(np.ceil((hi[1] - lo[1]) / CELL)) + 1
    h = np.full((nx, ny), -np.inf)
    gx = lo[0] + (np.arange(nx) + 0.5) * CELL
    gy = lo[1] + (np.arange(ny) + 0.5) * CELL
    for a, b, c in t:
        x0, x1 = min(a[0], b[0], c[0]), max(a[0], b[0], c[0])
        y0, y1 = min(a[1], b[1], c[1]), max(a[1], b[1], c[1])
        i0, i1 = int((x0 - lo[0]) / CELL), int((x1 - lo[0]) / CELL)
        j0, j1 = int((y0 - lo[1]) / CELL), int((y1 - lo[1]) / CELL)
        px, py = np.meshgrid(gx[i0:i1 + 1], gy[j0:j1 + 1], indexing='ij')
        d = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
        if abs(d) < 1e-12:
            continue
        l1 = ((b[1] - c[1]) * (px - c[0]) + (c[0] - b[0]) * (py - c[1])) / d
        l2 = ((c[1] - a[1]) * (px - c[0]) + (a[0] - c[0]) * (py - c[1])) / d
        l3 = 1 - l1 - l2
        inside = (l1 >= -1e-6) & (l2 >= -1e-6) & (l3 >= -1e-6)
        z = l1 * a[2] + l2 * b[2] + l3 * c[2]
        blk = h[i0:i1 + 1, j0:j1 + 1]
        np.copyto(blk, np.maximum(blk, np.where(inside, z, -np.inf)))
    return h, gx, gy


def pick(h, gx, gy):
    top = np.nanmax(np.where(np.isfinite(h), h, np.nan))
    k = 1   # 3×3 = 1,5 m
    nx, ny = h.shape
    cand = []
    for i in range(k + 2, nx - k - 2):
        for j in range(k + 2, ny - k - 2):
            w = h[i - k:i + k + 1, j - k:j + k + 1]
            if not np.all(np.isfinite(w)):
                continue
            # folga da borda: 1 m em volta ainda é telhado (nada de céu do lado)
            ring = h[i - 2:i + 3, j - 2:j + 3]
            if not np.all(np.isfinite(ring)) or ring.min() < h[i, j] - 0.6:
                continue
            if w.max() - w.min() < 0.3 and h[i, j] >= max(3.0, 0.55 * top):
                cand.append((h[i, j], i, j))
    cand.sort(reverse=True)
    out = []
    for z, i, j in cand:
        p = (float(gx[i]), float(gy[j]), float(z))
        if all((p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 >= 25.0 for q in out):
            out.append(p)
        if len(out) == 2:
            break
    return top, out


def main():
    dry = '--dry' in sys.argv
    text = UNITS.read_bytes().decode('utf-8')
    crlf = '\r\n' in text
    text = text.replace('\r\n', '\n')
    units = json.loads(text)
    changed = 0
    for u in units['units']:
        v = u.get('visual') or {}
        hull = v.get('hull') or []
        if not u.get('structure', v.get('scaffold') is not None) or not hull:
            continue
        name = hull[0].rsplit('/', 1)[-1]
        glb = PROV / name / f'{name}.glb'
        if not glb.exists():
            print(f'{u["key"]}: sem {glb}')
            continue
        js, binc = read_glb(glb)
        h, gx, gy = height_grid(triangles(js, binc))
        top, pts = pick(h, gx, gy)
        print(f'{u["key"]}: altura {top:.1f} m, {len(pts)} ponto(s): ' + ', '.join(f'({x:.2f}, {y:.2f}, {z:.2f})' for x, y, z in pts))
        # (30/10) o relevo inteiro do prédio visto de cima (cm, -1 = sem nada), pro tiro bater na parede de verdade: o jogo marcha o
        # raio do tiro nele (ARTSUnitActor::RayStructure; a caixa da malha punha o acerto no ar, na frente da fachada)
        if not dry:
            HEIGHTS.mkdir(parents=True, exist_ok=True)
            hc = np.where(np.isfinite(h), np.round(h * 100.0), -1).astype(int)
            (HEIGHTS / f'{name}.json').write_text(json.dumps({
                'about': 'Tools/roof_points.py: a altura (cm) de cada célula de 0,5 m vista de cima, no quadro da malha (eixos da Unreal); '
                         'linha = x, coluna = y; -1 = sem nada',
                'min_cm': [round(float(gx[0] - CELL / 2) * 100.0, 1), round(float(gy[0] - CELL / 2) * 100.0, 1)],
                'cell_cm': CELL * 100.0, 'nx': int(h.shape[0]), 'ny': int(h.shape[1]),
                'h_cm': hc.reshape(-1).tolist()}, separators=(',', ':')) + '\n', encoding='utf-8')
        # (30/10, o Felipe: "o impacto na construção não tá no hitbox dos models dela") os triângulos do modelo (cm, float32, 9 por triângulo,
        # no quadro da malha, eixos da Unreal): o tiro bate na superfície de verdade (ARTSUnitActor::RayStructure), não no relevo visto de
        # cima, que não enxerga o portão recuado nem a parede embaixo de uma marquise
        if not dry:
            tris = (triangles(js, binc) * 100.0).astype(np.float32)
            (HEIGHTS / f'{name}.tris.bin').write_bytes(tris.reshape(-1).tobytes())
            print(f'  {len(tris)} triângulos -> {name}.tris.bin')
        line = json.dumps([[round(x, 2), round(y, 2), round(z, 2)] for x, y, z in pts])
        # no texto, sem reformatar o arquivo (é de todas as sessões): a linha logo depois do "hull" do visual do prédio,
        # trocando a que já existir
        k = text.index('"key": "%s"' % u['key'])
        h0 = text.index('"hull": [', k)
        h1 = text.index(']', h0) + 1
        indent = text[text.rindex('\n', 0, h0) + 1:h0]
        rest = text[h1:]
        if rest.startswith(',\n' + indent + '"fire_points_m"'):
            e = rest.index('\n', 2)
            rest = rest[e:]
            rest = ',' + rest if not rest.startswith(',') else rest
        text = text[:h1] + ',\n' + indent + '"fire_points_m": ' + line + rest
        changed += 1
    if not dry and changed:
        json.loads(text)
        UNITS.write_bytes((text.replace('\n', '\r\n') if crlf else text).encode('utf-8'))
        print('gravado', UNITS)


if __name__ == '__main__':
    main()
