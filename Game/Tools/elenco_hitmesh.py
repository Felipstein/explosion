# O relevo e os triângulos do tiro dos prédios do elenco (01/11/2026): o mesmo que o roof_points.py grava pros prédios da USA
# (Config/RTS/Heights/<malha>.json e <malha>.tris.bin; ARTSUnitActor::RayStructure marcha o raio do tiro neles), agora pros
# modelos do elenco (captures/game/models/<pasta>/<malha>.glb). Sem eles o tiro no prédio novo batia na caixa da malha, no ar
# na frente da fachada (o Felipe, 30/10: "o impacto na construção não tá no hitbox dos models dela").
#
# Os modelos do elenco têm 30 a 490 mil triângulos (a grade do detalhe): o tiro não precisa disso. A malha do tiro é a mesma
# simplificada por agrupamento de vértices (Rossignac e Borrel 1993): cada vértice vai pro centro da célula de CLUSTER m (30 cm) dele,
# os triângulos que viram linha ou ponto saem, os repetidos também. Erro máximo ~CLUSTER/2 no ponto do impacto.
#   python Tools/elenco_hitmesh.py [chave ...]
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from roof_points import CELL, height_grid, read_glb, triangles  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
UNITS = ROOT / 'Config' / 'RTS' / 'Units.json'
MODELS = ROOT.parent / 'captures' / 'game' / 'models'
HEIGHTS = ROOT / 'Config' / 'RTS' / 'Heights'
CLUSTER = 0.3


def simplify(t):
    """Agrupa os vértices na grade de CLUSTER m; tira os triângulos degenerados e os repetidos."""
    q = np.round(t / CLUSTER).astype(np.int64)
    ok = ~(np.all(q[:, 0] == q[:, 1], axis=1) | np.all(q[:, 1] == q[:, 2], axis=1) | np.all(q[:, 0] == q[:, 2], axis=1))
    q = q[ok]
    # o mesmo triângulo com outra ordem dos cantos (ou o verso) conta uma vez: os cantos em ordem lexicográfica viram a chave
    order = np.lexsort((q[:, :, 2], q[:, :, 1], q[:, :, 0]), axis=-1)
    key = np.take_along_axis(q, order[:, :, None], axis=1).reshape(len(q), 9)
    _, first = np.unique(key, axis=0, return_index=True)
    return q[np.sort(first)].astype(np.float64) * CLUSTER


def main():
    only = set(sys.argv[1:])
    units = json.loads(UNITS.read_bytes().decode('utf-8'))['units']
    HEIGHTS.mkdir(parents=True, exist_ok=True)
    for u in units:
        v = u.get('visual') or {}
        hull = v.get('hull') or []
        if not v.get('scaffold') or not hull or (only and u['key'] not in only):
            continue
        name = hull[0].rsplit('/', 1)[-1]
        # a pasta do caminho da malha (/Game/Structures/<pasta>/...): a do elenco é a chave; a da versão B é a <chave>_v2 (01/11)
        glb = MODELS / hull[0].split('/')[3] / f'{name}.glb'
        if not glb.exists():
            glb = MODELS / u['key'] / f'{name}.glb'
        if not glb.exists():
            continue
        js, binc = read_glb(glb)
        full = triangles(js, binc)
        t = simplify(full)
        h, gx, gy = height_grid(t)
        hc = np.where(np.isfinite(h), np.round(h * 100.0), -1).astype(int)
        (HEIGHTS / f'{name}.json').write_text(json.dumps({
            'about': 'Tools/elenco_hitmesh.py: a altura (cm) de cada célula de 0,5 m vista de cima, no quadro da malha (eixos da Unreal); '
                     'linha = x, coluna = y; -1 = sem nada',
            'min_cm': [round(float(gx[0] - CELL / 2) * 100.0, 1), round(float(gy[0] - CELL / 2) * 100.0, 1)],
            'cell_cm': CELL * 100.0, 'nx': int(h.shape[0]), 'ny': int(h.shape[1]),
            'h_cm': hc.reshape(-1).tolist()}, separators=(',', ':')) + '\n', encoding='utf-8')
        (HEIGHTS / f'{name}.tris.bin').write_bytes((t * 100.0).astype(np.float32).reshape(-1).tobytes())
        print(f'{u["key"]}: {len(full)} -> {len(t)} triângulos ({len(t) * 36 / 1e6:.1f} MB), altura {np.nanmax(np.where(np.isfinite(h), h, np.nan)):.1f} m')


if __name__ == '__main__':
    main()
