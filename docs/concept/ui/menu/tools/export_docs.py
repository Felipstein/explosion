# Copia o protótipo aprovado das telas pra docs/concept/ui/menu/ (o que vai pro git): as 4 páginas, os mapas e o código comum.
# Troca os caminhos: snd_dl/ -> snd/ (as trilhas não vão pro git, vêm do assgets), os emblemas pequenos -> docs/concept/factions/small/,
# o fundo da partida em jpg leve.
import os, shutil
from PIL import Image
S = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = os.path.join(S, '..', '..', '..', '..', 'docs', 'concept', 'ui', 'menu'); D = os.path.normpath(D)
for name in ['menu.html', 'skirmish.html', 'multiplayer.html', 'screens.html', 'script.html']:
    s = open(os.path.join(S, name), encoding='utf-8').read()
    s = s.replace('snd_dl/', 'snd/').replace('a/glyph/${', '../../factions/small/glyph_${').replace('a/token/${', '../../factions/small/token_${')
    s = s.replace('ref/game_bg.png', 'ref/game_bg.jpg')
    assert 'a/glyph/' not in s and 'a/token/' not in s and 'snd_dl' not in s, name
    open(os.path.join(D, name), 'w', encoding='utf-8', newline='\n').write(s)
shutil.copy2(os.path.join(S, 'shared_maps.js'), D)
shutil.copy2(os.path.join(S, 'tools', 'cap.mjs'), os.path.join(D, 'tools'))
os.makedirs(os.path.join(D, 'maps'), exist_ok=True); os.makedirs(os.path.join(D, 'ref'), exist_ok=True)
for f in os.listdir(os.path.join(S, 'maps')): shutil.copy2(os.path.join(S, 'maps', f), os.path.join(D, 'maps'))
shutil.copytree(os.path.join(S, 'cur'), os.path.join(D, 'cur'), dirs_exist_ok=True)
shutil.copytree(os.path.join(S, 'data'), os.path.join(D, 'data'), dirs_exist_ok=True)   # as cenas do SCRIPT (export_scenarios.py)
for t in ['export_game.py', 'export_docs.py', 'export_scenarios.py', 'export_maps.py']: shutil.copy2(os.path.join(S, 'tools', t), os.path.join(D, 'tools'))   # o cursor dos menus (cópia do art/cursors/menu_png)
Image.open(os.path.join(S, 'ref', 'game_bg.png')).convert('RGB').save(os.path.join(D, 'ref', 'game_bg.jpg'), quality=82)
print(D)
