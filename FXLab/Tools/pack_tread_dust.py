"""Empacota a poeira das lagartas no plugin do jogo (FXLab/Plugins/RTSTreadDust, docs/POEIRA_DAS_LAGARTAS.md):
- Resources/tread_v12.json: o perfil aprovado como caminho (FXLab/tread_v12_ember.json);
- Resources/<cozimento>.atlas.json: o atlas do cozimento (grade, tempos, pivô, caixa e octógono de cada quadro);
- Content: T_Tiny, as duas folhas do cozimento (sem a do fogo), M_TreadDust (o M_FXSixWayDust) e M_TreadDustShadow
  (o M_FXDustShadow), montados pelos MESMOS scripts do FXLab: o visual é o mesmo do laboratório.
O jogo copia a pasta inteira do plugin pra Game/Plugins. Mudou o perfil ou o cozimento: rodar de novo e recopiar.
Uso: UnrealEditor-Cmd FXLab.uproject -run=pythonscript -script=FXLab/Tools/pack_tread_dust.py -nullrhi -unattended"""
import builtins
import json
import os
import runpy
import shutil
import struct
import tempfile
import zlib

import unreal

TOOLS = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.abspath(os.path.join(TOOLS, '..'))
REPO = os.path.abspath(os.path.join(PROJECT, '..'))
PLUGIN = os.path.join(PROJECT, 'Plugins', 'RTSTreadDust')
# o perfil que vai pro jogo: FXLAB_TREAD_PROFILE (um dos FXLab/tread_*.json), senão a v12 de 13/10
PROFILE = os.path.join(PROJECT, os.environ.get('FXLAB_TREAD_PROFILE', 'tread_v12_ember.json'))
ROOT = '/RTSTreadDust'

builtins.FXLAB_NO_BUILD = True
ns = runpy.run_path(os.path.join(TOOLS, 'setup_fxlab.py'))
log = ns['log']

bake = json.load(open(PROFILE, encoding='utf-8'))['bake']
res = os.path.join(PLUGIN, 'Resources')
os.makedirs(res, exist_ok=True)
shutil.copyfile(PROFILE, os.path.join(res, 'tread.json'))
old = os.path.join(res, 'tread_v12.json')
if os.path.exists(old):
    os.remove(old)                   # o nome antigo (o plugin agora lê tread.json)
shutil.copyfile(os.path.join(REPO, 'captures', 'fxlab', 'bake', bake, 'atlas.json'), os.path.join(res, bake + '.atlas.json'))

# a textura padrão minúscula (o material-mãe não segura folha grande; make_fx_defaults.py)
raw = b''.join(b'\x00' + b'\x00\x00\x00\x00' * 4 for _ in range(4))
chunk = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
png = os.path.join(tempfile.gettempdir(), 'T_Tiny.png')
open(png, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 4, 4, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
ns['import_file'](png, ROOT, 'T_Tiny')
t = unreal.load_asset(f'{ROOT}/T_Tiny')
t.set_editor_property('srgb', False)
t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
t.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
ns['eal'].save_loaded_asset(t)

# as folhas HDR (16 bits, ~150–200 MB) só servem pro A/B da compressão e iriam no pacote do jogo: FXLAB_SKIP_HDR=1 pula
if os.environ.get('FXLAB_SKIP_HDR') != '1':
    ns['import_bake'](bake, ROOT, emit=False)
# as folhas normalizadas em 8 bits (bake_norm8.py), comprimidas em BC7: /RTSTreadDust/<cozimento>_norm8 (o A/B da compressão)
norm8 = os.path.join(REPO, 'captures', 'fxlab', 'bake', bake, 'norm8')
if os.path.isdir(norm8):
    for file, name in (('six_pos.png', 'T_SixPos'), ('six_neg.png', 'T_SixNeg')):
        ns['import_file'](os.path.join(norm8, file), f'{ROOT}/{bake}_norm8', name)
        tex = unreal.load_asset(f'{ROOT}/{bake}_norm8/{name}')
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_BC7)
        tex.set_editor_property('srgb', False)
        tex.set_editor_property('never_stream', True)
        tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
        tex.set_editor_property('filter', unreal.TextureFilter.TF_TRILINEAR)
        ns['eal'].save_loaded_asset(tex)
    log('folhas norm8 (BC7) importadas')
# o ruído da erosão do sumiço (make_erode_noise.py: Worley em 3 oitavas, 9,6 m, 16 bits linear)
noise = os.path.join(REPO, 'captures', 'fxlab', 'bake', 'erode_noise', 'T_DustErodeNoise.png')
if os.path.exists(noise):
    ns['import_file'](noise, ROOT, 'T_DustErodeNoise')
    tn = unreal.load_asset(f'{ROOT}/T_DustErodeNoise')
    tn.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_GRAYSCALE)
    tn.set_editor_property('srgb', False)
    tn.set_editor_property('never_stream', True)
    tn.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
    tn.set_editor_property('filter', unreal.TextureFilter.TF_TRILINEAR)
    ns['eal'].save_loaded_asset(tn)
    log('ruído da erosão importado')
runpy.run_path(os.path.join(TOOLS, 'make_dust_material.py'), init_globals={'DUST_DEST': ROOT, 'DUST_NAME': 'M_TreadDust', 'DUST_GROUND': True,
                                                                         'DUST_NOISE': f'{ROOT}/T_DustErodeNoise'})
runpy.run_path(os.path.join(TOOLS, 'make_dust_shadow.py'),
               init_globals={'DUST_TINY': f'{ROOT}/T_Tiny', 'DUST_SHADOWS': [(f'{ROOT}/M_TreadDustShadow', False)]})
log('RTSTreadDust empacotado:', bake)
