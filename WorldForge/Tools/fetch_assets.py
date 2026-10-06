"""Baixa os assets das fatias (licença CC0) das APIs públicas.

Rodar com o Python do sistema:
  python WorldForge/Tools/fetch_assets.py
Vai pra WorldForge/Intermediate/Assets/<id>/ (fora do git: são ~1,3 GB, e o
script baixa de novo em qualquer PC). Confere o md5 que o Poly Haven informa
e pula o que já está baixado.

Fontes (CC0, uso comercial livre, sem crédito obrigatório):
  Poly Haven  https://polyhaven.com/license  — coleção Namaqualand (deserto
              real, Northern Cape, África do Sul): superfícies, rochas,
              paredões, arbustos e capim da mesma região
  ambientCG   https://docs.ambientcg.com/license/ — decalque de marcas de pneu
Escolha e tamanhos: pesquisa de 05/10/2026 (docs/WORLDFORGE.md).
"""
import hashlib
import json
import os
import sys
import urllib.request
import zipfile

TOOLS = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(os.path.join(TOOLS, '..', 'Intermediate', 'Assets'))
# a API do Poly Haven pede um User-Agent próprio (Public-API/ToS.md)
UA = {'User-Agent': 'WorldForge-asset-fetch/1.0 (github.com/Felipstein/explosion)'}

# superfícies: cor, normal DirectX (a da Unreal), AO/rugosidade/metal, altura
SURFACES = {
    'aerial_beach_01': '4k',      # areia vista de longe (30 m de foto)
    'sandy_gravel_02': '4k',      # areia/cascalho de perto (Namaqualand)
    'dry_ground_01': '4k',        # terra batida
    'gravelly_sand': '4k',        # cascalho (Namaqualand)
    'mud_cracked_dry_03': '4k',   # terra rachada
    'asphalt_04': '4k',           # asfalto gasto
    'cliff_side': '4k',           # paredão (Namaqualand)
    # fatias de floresta, neve e rua (aprovadas em 05/10/2026, ~670 MB com os modelos abaixo)
    'forest_leaves_02': '4k',     # folhiço
    'forest_ground_04': '4k',     # chão de mata
    'leaves_forest_ground': '4k', # folhas e terra
    'mud_forest': '4k',           # barro de mata
    'leafy_grass': '4k',          # grama com folhas
    'grass_path_2': '4k',         # trilha na grama
    'snow_02': '4k',              # neve
    'snow_04': '4k',              # neve pisada
    'snow_field_aerial': '4k',    # neve vista do alto
    'asphalt_snow': '4k',         # estrada com neve
    'road_damaged': '4k',         # asfalto quebrado
    'road_damaged_2': '4k',       # asfalto quebrado (buracos)
    'worn_asphalt': '4k',         # asfalto gasto
    'aerial_asphalt_01': '4k',    # asfalto visto do alto
}
SURFACE_MAPS = ('Diffuse', 'nor_dx', 'arm', 'Displacement')

# modelos: o FBX + os mapas em JPG (o pacote FBX do Poly Haven traz a normal
# no padrão OpenGL e a rugosidade em EXR; aqui vão os da Unreal)
MODELS = {
    'namaqualand_cliff_02': '4k', 'namaqualand_cliff_01': '4k',
    'namaqualand_boulder_02': '4k', 'namaqualand_boulder_03': '4k', 'namaqualand_boulder_04': '4k',
    'namaqualand_boulder_05': '2k', 'namaqualand_boulder_06': '2k',
    'namaqualand_boulders_01': '2k', 'namaqualand_rocks_01': '2k', 'namaqualand_stones_01': '2k',
    'wild_rooibos_bush': '4k', 'searsia_lucida': '2k',
    'grass_medium_01': '2k', 'grass_medium_02': '4k', 'dry_branches_medium_01': '2k',
    # floresta e beira de estrada (05/10/2026)
    'fern_02': '2k', 'shrub_01': '2k', 'shrub_02': '2k', 'tree_stump_01': '2k', 'dead_tree_trunk': '2k',
    'rock_moss_set_01': '2k', 'moss_01': '2k',
    'concrete_road_barrier': '2k', 'street_lamp_01': '2k', 'modular_electricity_poles': '2k',
}
MODEL_MAPS = ('Diffuse', 'nor_dx', 'arm')
EXTRA_MAPS = ('alpha', 'mask', 'translucency', 'dry_diff')   # quando existem (plantas)

AMBIENTCG = {'TireTracks001': '4K-JPG'}


def get_json(url):
    return json.load(urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=60))


def download(url, dest, md5=None, tries=3):
    if os.path.exists(dest) and (md5 is None or hashlib.md5(open(dest, 'rb').read()).hexdigest() == md5):
        return 0
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    tmp = dest + '.part'
    for attempt in range(tries):
        # uma transferência veio corrompida uma vez (05/10): tenta de novo
        with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=120) as r, open(tmp, 'wb') as f:
            while True:
                b = r.read(1 << 20)
                if not b:
                    break
                f.write(b)
        if not md5 or hashlib.md5(open(tmp, 'rb').read()).hexdigest() == md5:
            os.replace(tmp, dest)
            return os.path.getsize(dest)
        print('  md5 não bateu, tentando de novo (%d): %s' % (attempt + 1, os.path.basename(url)))
    os.remove(tmp)
    raise RuntimeError('md5 não bate: ' + url)


def pick(files, key, res):
    """O JPG (ou PNG) de um mapa nessa resolução, como {url, size, md5}."""
    entry = files.get(key, {}).get(res, {})
    return entry.get('jpg') or entry.get('png')


def fetch_polyhaven(asset_id, res, maps, model):
    files = get_json('https://api.polyhaven.com/files/' + asset_id)
    total = 0
    folder = os.path.join(OUT, asset_id)
    wanted = list(maps) + [k for k in files if k.lower() in EXTRA_MAPS]
    for key in wanted:
        f = pick(files, key, res)
        if not f:
            print('  (sem %s em %s)' % (key, res))
            continue
        total += download(f['url'], os.path.join(folder, os.path.basename(f['url'])), f.get('md5'))
    if model:
        fbx = files['fbx'][res]['fbx']
        total += download(fbx['url'], os.path.join(folder, os.path.basename(fbx['url'])), fbx.get('md5'))
    return total


def fetch_ambientcg(asset_id, variant):
    folder = os.path.join(OUT, asset_id)
    zip_path = os.path.join(folder, '%s_%s.zip' % (asset_id, variant))
    n = download('https://ambientcg.com/get?file=%s_%s.zip' % (asset_id, variant), zip_path)
    with zipfile.ZipFile(zip_path) as z:
        z.extractall(folder)
    return n


def main():
    os.makedirs(OUT, exist_ok=True)
    grand = 0
    for i, (aid, res) in enumerate(SURFACES.items()):
        n = fetch_polyhaven(aid, res, SURFACE_MAPS, model=False)
        grand += n
        print('surface %-24s %s  %6.1f MB' % (aid, res, n / 1e6))
    for aid, res in MODELS.items():
        n = fetch_polyhaven(aid, res, MODEL_MAPS, model=True)
        grand += n
        print('model   %-24s %s  %6.1f MB' % (aid, res, n / 1e6))
    for aid, var in AMBIENTCG.items():
        n = fetch_ambientcg(aid, var)
        grand += n
        print('decal   %-24s %s  %6.1f MB' % (aid, var, n / 1e6))
    print('total baixado agora: %.0f MB -> %s' % (grand / 1e6, OUT))


if __name__ == '__main__':
    sys.exit(main())
