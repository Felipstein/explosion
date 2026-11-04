# O som das telas como asset do jogo (04/11, o menu nativo): as classes de som (uma por volume das opções aprovadas), o mix que
# abaixa a música nas telas de conversa, o barramento da música (o que o radar do menu escuta) e os arquivos do protótipo
# importados (as três trilhas do menu e o clique, do assgets). Roda na Unreal sem janela, na vez da fila:
#   UnrealEditor-Cmd.exe Game/RTSGame.uproject -run=pythonscript -script=Game/Plugins/RTSShell/Tools/import_menu_audio.py -nullrhi
# Refazer é seguro: cria o que falta e reescreve as propriedades.
import os
import unreal

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Resources', 'UI', 'snd')
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary


def make(name, path, cls, factory):
    full = f'{path}/{name}'
    if lib.does_asset_exist(full):
        return lib.load_asset(full)
    return tools.create_asset(name, path, cls, factory)


# as classes: o volume geral e os seis das opções (MASTER, MUSIC, EFFECTS, UNIT VOICES, INTERFACE, AMBIENCE). O volume de cada
# uma vem do jogador (o mix de volumes do jogo); o asset fica em 1
CLASSES = '/Game/Audio/Classes'
master = make('SC_Master', CLASSES, unreal.SoundClass, unreal.SoundClassFactory())
children = []
for name in ['SC_Music', 'SC_Effects', 'SC_Voice', 'SC_UI', 'SC_Ambience']:
    c = make(name, CLASSES, unreal.SoundClass, unreal.SoundClassFactory())
    c.set_editor_property('parent_class', master)
    children.append(c)
master.set_editor_property('child_classes', children)
music_class, ui_class = children[0], children[3]

# o barramento da música (sai no geral): o menu escuta as amostras dele pra cascata do ESM e o A-scope, como o analisador do
# navegador escutava a trilha do mockup
music_bus = make('SMX_Music', '/Game/Audio/Submix', unreal.SoundSubmix, unreal.SoundSubmixFactory())

# o skirmish, o multiplayer e o SCRIPT abaixam a música uns 8 dB (× 0,4 do mockup), descendo e voltando em 1,4 s
duck = make('SM_ShellDuck', '/Game/Audio/Mix', unreal.SoundMix, unreal.SoundMixFactory())
adj = unreal.SoundClassAdjuster()
adj.set_editor_property('sound_class_object', music_class)
adj.set_editor_property('volume_adjuster', 0.4)
adj.set_editor_property('apply_to_children', True)
duck.set_editor_property('sound_class_effects', [adj])
duck.set_editor_property('fade_in_time', 1.4)
duck.set_editor_property('fade_out_time', 1.4)
duck.set_editor_property('duration', -1.0)


# os volumes do jogador: o mix vazio que o jogo enche em tempo real (SetSoundMixClassOverride por classe) e deixa sempre ligado
make('SM_UserVolumes', '/Game/Audio/Mix', unreal.SoundMix, unreal.SoundMixFactory())


def wave(file, path, name, cls, bus=None):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', os.path.normpath(os.path.join(SRC, file)))
    task.set_editor_property('destination_path', path)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', False)
    tools.import_asset_tasks([task])
    w = lib.load_asset(f'{path}/{name}')
    w.set_editor_property('sound_class_object', cls)
    if bus:
        w.set_editor_property('sound_submix_object', bus)
    unreal.log(f'som importado: {path}/{name} ({w.get_editor_property("duration"):.1f} s)')
    return w


for i in range(1, 4):
    wave(f'Main_Menu_{i}.mp3', '/Game/Audio/Music', f'Main_Menu_{i}', music_class, music_bus)
wave('Menu_Click.wav', '/Game/Audio/UI', 'UI_Menu_Click', ui_class)
# (04/11) o hover, a entrada no skirmish e no multiplayer e a abertura: os WAV prontos do make_menu_sounds.py (Resources/Audio)
SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Resources', 'Audio')
for name in ['UI_Menu_Hover', 'UI_Menu_Enter', 'UI_Boot_Relay', 'UI_Boot_Riser', 'UI_Boot_Reveal']:
    wave(f'{name}.wav', '/Game/Audio/UI', name, ui_class)

# (04/11) o mapa do menu: vazio (o menu não desenha o mundo). O jogo abre nele em instantes e o mapa da batalha só carrega na
# partida; antes, o menu carregava o World_TestSalt2 inteiro (11 s) só pra ficar por trás
if not lib.does_asset_exist('/Game/Maps/FrontEnd'):
    tools.create_asset('FrontEnd', '/Game/Maps', unreal.World, unreal.WorldFactory())
    lib.save_asset('/Game/Maps/FrontEnd', only_if_is_dirty=False)
    unreal.log('mapa do menu criado: /Game/Maps/FrontEnd')

for p in lib.list_assets('/Game/Audio', recursive=True):
    lib.save_asset(p.split('.')[0], only_if_is_dirty=False)
unreal.log('som das telas: pronto')
