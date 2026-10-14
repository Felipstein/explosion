"""As texturas padrão dos materiais do FXLab (13/10, otimização): o material-mãe segura na memória as texturas
padrão mesmo quando todo MID troca as suas. Antes eram as 3 folhas do gas_puff3 (302 MB) no M_FXSixWay e uma folha
da poeira na sombra. Agora uma textura de 4 × 4 (T_Tiny, linear). Remonta o M_FXSixWay com o MESMO HLSL de sempre
(setup_fxlab.sixway_material): o fogo e o tiro não mudam.
Uso: UnrealEditor-Cmd FXLab.uproject -run=pythonscript -script=FXLab/Tools/make_fx_defaults.py -nullrhi -unattended"""
import builtins
import os
import runpy
import tempfile
import zlib
import struct

import unreal

builtins.FXLAB_NO_BUILD = True
ns = runpy.run_path(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'setup_fxlab.py'))


def png4():
    raw = b''.join(b'\x00' + b'\x00\x00\x00\x00' * 4 for _ in range(4))
    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 4, 4, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')


path = os.path.join(tempfile.gettempdir(), 'T_Tiny.png')
open(path, 'wb').write(png4())
ns['import_file'](path, '/Game/FX', 'T_Tiny')
t = unreal.load_asset('/Game/FX/T_Tiny')
t.set_editor_property('srgb', False)
t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
t.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
ns['eal'].save_loaded_asset(t)
ns['sixway_material']({'T_SixPos': t, 'T_SixNeg': t, 'T_Emit': t})
ns['log']('T_Tiny e M_FXSixWay com padrão minúsculo')
