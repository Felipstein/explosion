# As telas aprovadas pro jogo: copia as páginas do protótipo pra Resources/UI do plugin RTSShell (o jogo serve em https://rts.ui/).
# Só vai o que o jogo usa (02/11, o Felipe: "tem que remover completamente, senão vai inflar"): corta o que está entre
# /*fora-do-jogo*/ ... /*/fora-do-jogo*/ e <!--fora-do-jogo--> ... <!--/fora-do-jogo--> (patch_fora_do_jogo.py marcou) e leva
# só os arquivos que as páginas do jogo pedem. Troca snd_dl/ -> snd/. Apaga a pasta antes, pra não sobrar nada velho.
# Rodar de novo a cada mudança aprovada no protótipo:  python export_game.py [pasta de destino]
import os, re, shutil, sys
S = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME_UI = os.path.normpath(os.path.join(S, '..', '..', '..', '..', 'Game', 'Plugins', 'RTSShell', 'Resources', 'UI'))
D = sys.argv[1] if len(sys.argv) > 1 else GAME_UI
if os.path.isdir(D):
    shutil.rmtree(D)
os.makedirs(D)
CUT = [re.compile(r'/\*fora-do-jogo\*/.*?/\*/fora-do-jogo\*/\n?', re.S), re.compile(r'<!--fora-do-jogo-->.*?<!--/fora-do-jogo-->\n?', re.S)]
for name in ['menu.html', 'skirmish.html', 'multiplayer.html', 'screens.html', 'script.html']:
    s = open(os.path.join(S, name), encoding='utf-8').read()
    for r in CUT:
        s = r.sub('', s)
    s = s.replace('snd_dl/', 'snd/')
    assert 'fora-do-jogo' not in s and 'snd_dl' not in s, name
    open(os.path.join(D, name), 'w', encoding='utf-8', newline='\n').write(s)
shutil.copy2(os.path.join(S, 'shared_maps.js'), D)
# os 3 mapas de verdade (o minimapa da sala e da lista da LAN)
os.makedirs(os.path.join(D, 'maps'))
for m in ['World_TestSalt2', 'World_TestDunesSim', 'World_TestHills']:
    for suf in ['.json', '.png', '_water.png']:
        shutil.copy2(os.path.join(S, 'maps', m + suf), os.path.join(D, 'maps'))
# o cursor em 48 px (o que as páginas usam), os emblemas, o clique e as 3 trilhas do menu
os.makedirs(os.path.join(D, 'cur'))
shutil.copytree(os.path.join(S, 'cur', '48'), os.path.join(D, 'cur', '48'))
shutil.copytree(os.path.join(S, 'a'), os.path.join(D, 'a'))
os.makedirs(os.path.join(D, 'snd'))
for f in ['Menu_Click.wav', 'Main_Menu_1.mp3', 'Main_Menu_2.mp3', 'Main_Menu_3.mp3']:
    shutil.copy2(os.path.join(S, 'snd_dl', f), os.path.join(D, 'snd', f))
# a sintaxe de todo script (03/11: um comentário no meio da linha quebrou o menu e ninguém viu até o teste no jogo)
import subprocess
NODE = os.path.join(os.environ['LOCALAPPDATA'], 'Volta', 'tools', 'image', 'node', '24.15.0', 'node.exe')
r = subprocess.run([NODE, os.path.join(S, 'tools', 'check_js.mjs'), D], capture_output=True, text=True)
print(r.stdout.strip())
if r.returncode != 0:
    raise SystemExit('as páginas do jogo não compilam: corrigir antes de mandar')
total = sum(os.path.getsize(os.path.join(r, f)) for r, _, fs in os.walk(D) for f in fs)
print(D, f'{total / 1e6:.1f} MB')
