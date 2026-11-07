# (06/11/2026, a trilha da partida, docs/TRILHA_DA_PARTIDA.md) Leva as faixas do music_pack.py (captures/musica/pack/<facção>/MU_<id>.wav)
# pra Unreal como /Game/Audio/Music/Match/<facção>/MU_<id>: a classe SC_Music (o volume MUSIC das opções), o barramento SMX_Music (o das
# telas), o RAD Audio (o codec que deixa começar no meio da faixa: a batalha entra no trecho forte) e tudo na memória (Force Inline: a
# busca no meio é instantânea; o URTSMusic carrega só as da facção do jogador). Roda na Unreal sem janela, na vez da fila:
#   powershell -File Game/Tools/music/import_music.ps1
# Refazer é seguro: reimporta por cima. Só as faixas do Music.json (o argumento -ids=a,b importa só essas).
import json
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..', '..'))
PACK = os.path.join(ROOT, 'captures', 'musica', 'pack')
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary

music_class = lib.load_asset('/Game/Audio/Classes/SC_Music')
music_bus = lib.load_asset('/Game/Audio/Submix/SMX_Music')
if not music_class or not music_bus:
    raise RuntimeError('falta o SC_Music ou o SMX_Music (Plugins/RTSShell/Tools/import_menu_audio.py)')

only = set()
for a in sys.argv[1:]:
    if a.startswith('-ids='):
        only = set(a[5:].split(','))
data = json.load(open(os.path.join(ROOT, 'Game', 'Config', 'RTS', 'Music.json'), encoding='utf-8'))
done = 0
for t in data['tracks']:
    if only and t['id'] not in only:
        continue
    fac, name = t['faction'], f"MU_{t['id']}"
    path = f'/Game/Audio/Music/Match/{fac}'
    src = os.path.join(PACK, fac, f'{name}.wav')
    if not os.path.exists(src):
        unreal.log_warning(f'trilha: falta {src} (rodar o music_pack.py)')
        continue
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', os.path.normpath(src))
    task.set_editor_property('destination_path', path)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', False)
    tools.import_asset_tasks([task])
    w = lib.load_asset(f'{path}/{name}')
    w.set_editor_property('sound_class_object', music_class)
    w.set_editor_property('sound_submix_object', music_bus)
    for prop, val in (('sound_asset_compression_type', unreal.SoundAssetCompressionType.RAD_AUDIO),
                      ('compression_quality', 80),
                      ('loading_behavior', unreal.SoundWaveLoadingBehavior.FORCE_INLINE),
                      ('virtualization_mode', unreal.VirtualizationMode.PLAY_WHEN_SILENT)):   # (a subida do zero: o som calado no começo não pode ser descartado)
        try:
            w.set_editor_property(prop, val)
        except Exception as e:  # o nome mudou numa versão: o log diz qual
            unreal.log_warning(f'trilha: {name}: não consegui pôr {prop} ({e})')
    lib.save_asset(f'{path}/{name}', only_if_is_dirty=False)
    done += 1
    unreal.log(f'trilha importada: {path}/{name} ({w.get_editor_property("duration"):.1f} s)')
unreal.log(f'trilha: pronto ({done} faixas)')
