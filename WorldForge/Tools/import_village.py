"""O kit civil do Look na vista do WorldForge (forge.ps1 py Tools\\import_village.py).

As peças vêm de captures/look/civil_glb/SM_civ_<tipo>.glb (a sessão do Look faz), os mesmos tipos
do village.json. Importa em /Game/Village/SM_civ_<tipo>, no mesmo caminho e com o mesmo material
do jogo: roda as funções do Game/Tools/import_provisional.py (M_Structure e as instâncias por
nome de material, MI_Civ*), sem copiar o código, então uma mudança do jogo no material chega aqui
na próxima importação. O AWorldForgeTerrain::BuildVillage usa a malha quando ela existe e a caixa
provisória quando não.
"""
import glob
import os

import unreal

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
GAME_TOOL = os.path.join(REPO, 'Game', 'Tools', 'import_provisional.py')

with open(GAME_TOOL, encoding='utf-8') as f:
    src = f.read()
# só as funções: a última linha do arquivo do jogo roda o main() dele
cut = src.rstrip().rfind('\nmain()')
if cut < 0:
    raise RuntimeError('import_provisional.py mudou: não achei a chamada do main() no fim')
ns = {'__file__': GAME_TOOL, '__name__': 'import_provisional_lib'}
exec(compile(src[:cut], GAME_TOOL, 'exec'), ns)

files = sorted(glob.glob(os.path.join(ns['CIVIL_SRC'], 'SM_civ_*.glb')))
if not files:
    raise RuntimeError('nenhum SM_civ_*.glb em %s: o kit civil vem da sessão do Look' % ns['CIVIL_SRC'])
# as miudezas da maquete do Look (13/10: laje, chão e toldo; captures/look/kit_vila/manifest.json), com os
# mesmos materiais do kit civil (Plaster, Wood, Metal, Rust, Cloth, Dark)
files += sorted(glob.glob(os.path.join(REPO, 'captures', 'look', 'tank_glb', 'SM_LL_Prop_*.glb')))
opaque = ns['instances'](ns['master']('M_Structure', False), '')
ns['import_meshes'](files, lambda f: ns['CIVIL_ROOT'], lambda k: ns['CIVIL_SLOTS'].get(k, k), opaque)
unreal.log('FORGE vila: %d malhas do kit civil em %s' % (len(files), ns['CIVIL_ROOT']))
