# As folhas pra ele aprovar: o mockup (o Chrome) à esquerda e o jogo (a foto interna da tela nativa) à direita, cada estado numa
# linha, em 960 × 540 cada; uma folha por grupo de telas. Uso: python side_by_side.py [grupo ...] (sem nada: todas)
import os, sys
from PIL import Image, ImageDraw, ImageFont
N = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..', 'captures', 'concept', 'ui', 'shell', 'shots', 'native'))
GROUPS = {
    'skirmish': [('mock_skirmish_room.jpg', 'sk_room.png', 'SALA'), ('mock_skirmish_pop.jpg', 'sk_pop.png', 'MENU DO EXERCITO'),
                 ('mock_game_tip.jpg', 'sk_tip.png', 'CORES + DICA'), ('mock_game_browser.jpg', 'sk_bw.png', 'TROCAR O MAPA'),
                 ('mock_game_browser_q.jpg', 'sk_bw_q.png', 'BUSCA "du"')],
    'multiplayer': [('mock_mp_list.jpg', 'mp_list.png', 'LISTA DA LAN'), ('mock_mp_room_host.jpg', 'mp_room_host.png', 'SALA DA LAN (QUEM HOSPEDA)'),
                    ('mock_mp_room_guest.jpg', 'mp_room_guest.png', 'SALA DA LAN (CONVIDADO)')],
    'options': [('mock_opt_gfx.jpg', 'opt_gfx.png', 'GRAPHICS'), ('mock_opt_snd.jpg', 'opt_snd.png', 'SOUND'), ('mock_opt_keys.jpg', 'opt_keys.png', 'CONTROLS'),
                ('mock_opt_game.jpg', 'opt_game.png', 'GAME'), ('mock_opt_acc.jpg', 'opt_acc.png', 'ACCESSIBILITY')],
    'script': [('mock_script.jpg', 'script.png', 'SCRIPT'), ('mock_script_armed.jpg', 'script_armed.png', 'SCRIPT COM A CENA ARMADA')],
    # (06/11) a sala da LAN igual à do skirmish (o mockup ?mp=1) e as telas da partida (screens.html)
    'sala_lan': [('mock_room_host.jpg', 'lan_host.png', 'SALA DA LAN (QUEM HOSPEDA)'), ('mock_room_guest.jpg', 'lan_guest.png', 'SALA DA LAN (CONVIDADO)'),
                 ('mock_skirmish_room.jpg', 'sk_rot.png', 'SKIRMISH (O MAPA NO GIRO DA PARTIDA)')],
    'partida': [('mock_match_load.jpg', 'm_load.png', 'CARREGANDO'), ('mock_match_esc.jpg', 'm_pause.png', 'ESC (SKIRMISH)'),
                ('mock_match_esc.jpg', 'm_pause_lan.png', 'ESC (REDE)'), ('mock_match_win.jpg', 'm_win.png', 'VITORIA'),
                ('mock_match_lose.jpg', 'm_lose.png', 'DERROTA'), ('mock_match_net.jpg', 'm_net.png', 'AVISO DE REDE (ESPERANDO)'),
                ('mock_match_net.jpg', 'm_sync.png', 'AVISO DE REDE (DESSINCRONIA)')],
}
W, H, G, TOP = 960, 540, 8, 34
try:
    FONT = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 20)
except OSError:
    FONT = ImageFont.load_default()
for name in (sys.argv[1:] or GROUPS.keys()):
    pairs = [p for p in GROUPS[name] if os.path.exists(os.path.join(N, p[0])) and os.path.exists(os.path.join(N, p[1]))]
    if not pairs:
        print(name, 'sem fotos'); continue
    out = Image.new('RGB', (W * 2 + G, (H + TOP) * len(pairs)), (0, 0, 0))
    d = ImageDraw.Draw(out)
    for i, (m, n, t) in enumerate(pairs):
        y = i * (H + TOP)
        d.text((10, y + 6), f'{t}  ·  mockup', fill=(216, 173, 78), font=FONT)
        d.text((W + G + 10, y + 6), f'{t}  ·  no jogo (nativo)', fill=(216, 173, 78), font=FONT)
        out.paste(Image.open(os.path.join(N, m)).convert('RGB').resize((W, H), Image.LANCZOS), (0, y + TOP))
        out.paste(Image.open(os.path.join(N, n)).convert('RGB').resize((W, H), Image.LANCZOS), (W + G, y + TOP))
    path = os.path.join(N, f'lado_a_lado_{name}.jpg')
    out.save(path, quality=88)
    print(path)
