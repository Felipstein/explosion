# As telas aprovadas pro jogo: copia as páginas do protótipo pra Resources/UI do plugin RTSShell (o jogo serve em https://rts.ui/).
# Troca os caminhos do protótipo: snd_dl/ -> snd/ (o clique e as 3 trilhas do menu em mp3), o fundo da partida em jpg.
# Rodar de novo a cada mudança aprovada no protótipo:  python export_game.py [pasta de destino]
import os, shutil, sys
from PIL import Image
S = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME_UI = os.path.normpath(os.path.join(S, '..', '..', '..', '..', 'Game', 'Plugins', 'RTSShell', 'Resources', 'UI'))
D = sys.argv[1] if len(sys.argv) > 1 else GAME_UI
os.makedirs(D, exist_ok=True)
for name in ['menu.html', 'skirmish.html', 'multiplayer.html', 'screens.html', 'script.html']:
    s = open(os.path.join(S, name), encoding='utf-8').read().replace('snd_dl/', 'snd/').replace('ref/game_bg.png', 'ref/game_bg.jpg')
    assert 'snd_dl' not in s, name
    open(os.path.join(D, name), 'w', encoding='utf-8', newline='\n').write(s)
shutil.copy2(os.path.join(S, 'shared_maps.js'), D)
for sub in ['maps', 'cur', 'a']:
    shutil.copytree(os.path.join(S, sub), os.path.join(D, sub), dirs_exist_ok=True)
os.makedirs(os.path.join(D, 'snd'), exist_ok=True)
for f in ['Menu_Click.wav', 'Main_Menu_1.mp3', 'Main_Menu_2.mp3', 'Main_Menu_3.mp3']:
    shutil.copy2(os.path.join(S, 'snd_dl', f), os.path.join(D, 'snd', f))
os.makedirs(os.path.join(D, 'ref'), exist_ok=True)
Image.open(os.path.join(S, 'ref', 'game_bg.png')).convert('RGB').save(os.path.join(D, 'ref', 'game_bg.jpg'), quality=82)
print(D)
