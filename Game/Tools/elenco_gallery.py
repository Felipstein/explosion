"""A galeria de avaliação do elenco (01/11/2026): uma página com cada peça nova das 3 facções (a foto no jogo, a prévia do Blender e a
câmera do jogo de perto, a ficha resumida) e o veredito do Felipe por peça (aprovado, no rumo, refazer + o comentário), guardado no `db`
da página (a coleção "avaliacoes", um documento por chave), que a sessão lê de volta.

Rodar (Python do sistema, com o Pillow; sem a Unreal):
  python Game/Tools/elenco_gallery.py

Lê as fichas (docs/elenco/<facção>/<chave>.md), as fotos das cenas de vitrine (captures/game/elenco_<facção>_tour/*peca_<chave>.png:
game.ps1 view -Scenario elenco_china_tour etc.) e as prévias (captures/game/models/<chave>/preview/). Grava captures/game/elenco_galeria/
(index.html + img/), que o Artifact publica.
"""
import glob
import html
import json
import os
import re

from PIL import Image

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
OUT = os.path.join(REPO, 'captures', 'game', 'elenco_galeria')
FACTIONS = [('china', 'China'), ('gla', 'GLA'), ('usa', 'USA')]
NEW_USA = {'usa_strategy_center', 'usa_paladin', 'usa_javelin'}
# a ordem da página: prédios antes, pela árvore de cada facção; depois veículos e infantaria
ORDER = ['china_command_center', 'china_power_plant', 'china_supply_center', 'china_barracks', 'china_war_factory', 'china_propaganda_center',
         'china_gattling_cannon', 'china_dozer', 'china_supply_truck', 'china_type99', 'china_gattling_tank', 'china_inferno_cannon',
         'china_overlord', 'china_rifleman', 'china_tank_hunter',
         'gla_command_center', 'gla_supply_stash', 'gla_barracks', 'gla_arms_dealer', 'gla_palace', 'gla_stinger_site',
         'gla_supply_pickup', 'gla_quad_cannon', 'gla_toxin_tractor', 'gla_rocket_buggy', 'gla_worker', 'gla_rebel', 'gla_rpg',
         'usa_strategy_center', 'usa_paladin', 'usa_javelin', 'usa_mbt', 'usa_tomahawk', 'usa_dozer',
         'usa_supply_truck_v2', 'usa_stryker_v2', 'usa_linebacker_v2', 'usa_patriot_v2', 'usa_drop_zone_v2', 'usa_strategy_center_v2',
         'usa_airfield_v2', 'usa_raptor_v2', 'usa_comanche_v2', 'usa_chinook_v2', 'usa_cargo_plane_v2', 'usa_supply_drop_v2',
         'russia_command_center', 'russia_power_plant', 'russia_supply_center', 'russia_barracks', 'russia_war_factory',
         'russia_institute', 'russia_pantsir_site', 'russia_dozer', 'russia_supply_truck', 'russia_t90m', 'russia_bmpt', 'russia_pantsir',
         'russia_tos1a', 'russia_t14', 'russia_motostrelok', 'russia_kornet',
         'civ_carros', 'civ_casas', 'civ_miudezas']


def ficha(key):
    fac = key.split('_')[0]
    p = os.path.join(REPO, 'docs', 'elenco', fac, key + '.md')
    if not os.path.exists(p):
        return None
    t = open(p, encoding='utf-8').read()
    title = re.search(r'^# (.+)$', t, re.M)
    rows = dict(re.findall(r'^\| ([^|]+?) \| (.+?) \|$', t, re.M))
    idea = ''
    m = re.search(r'## 2\. A ideia\s+(.+?)(?:\n\n|\n- )', t, re.S)
    if m:
        idea = re.sub(r'\s+', ' ', m.group(1)).strip()
    return {'title': title.group(1).strip() if title else key, 'rows': rows, 'idea': idea,
            'path': os.path.relpath(p, REPO).replace('\\', '/')}


def thumb(src, dst, width=1280):
    im = Image.open(src).convert('RGB')
    if im.width > width:
        im = im.resize((width, round(im.height * width / im.width)), Image.LANCZOS)
    im.save(dst, quality=84, optimize=True, progressive=True)
    return os.path.getsize(dst)


# (01/11) as versões B dos aprovados da USA (o agente do detalhe): a folha ficha × A × B e as prévias; ele escolhe qual fica
VARIANTS = {
    'usa_mbt': ('Tanque principal da USA, versão B', 'usa_mbt', ['folha_ficha_A_B.jpg']),
    'usa_tomahawk': ('Tomahawk, versão B', 'usa_tomahawk_v2', ['folha_A_x_B.jpg', 'fire_three_quarter_masks_game.png']),
    'usa_dozer': ('Dozer da USA, versão B', 'usa_dozer_v2', ['folha_arte_A_B.jpg']),
    # (01/11 manhã) o resto da USA no mesmo nível (o agente do resto): cada um como versão B, a A (a de hoje) guardada
    'usa_supply_truck_v2': ('Caminhão de suprimento da USA, versão B', 'usa_supply_truck_v2', ['folha_A_B.jpg']),
    'usa_stryker_v2': ('Stryker, versão B', 'usa_stryker_v2', ['folha_ficha_A_B.jpg']),
    'usa_linebacker_v2': ('Linebacker, versão B', 'usa_linebacker_v2', ['folha_A_B.jpg']),
    'usa_raptor_v2': ('Raptor, versão B', 'usa_raptor_v2', ['folha_A_B.jpg']),
    'usa_comanche_v2': ('Apache (Comanche), versão B', 'usa_comanche_v2', ['folha_A_B.jpg']),
    'usa_chinook_v2': ('Chinook, versão B', 'usa_chinook_v2', ['folha_ficha_A_B.jpg']),
    'usa_cargo_plane_v2': ('Avião de carga, versão B', 'cargo_plane_v2', ['folha_A_B.jpg']),
    'usa_supply_drop_v2': ('Caixa do lançamento + paraquedas, versão B', 'supply_drop_v2', ['folha_A_B.jpg']),
    'usa_patriot_v2': ('Patriot, versão B', 'usa_patriot_v2', ['folha_A_B.jpg']),
    'usa_drop_zone_v2': ('Drop Zone, versão B', 'usa_drop_zone_v2', ['folha_A_B.jpg']),
    'usa_strategy_center_v2': ('Strategy Center, versão B', 'usa_strategy_center_v2', ['folha_A_B.jpg']),
    'usa_airfield_v2': ('Airfield, versão B', 'usa_airfield_v2', ['folha_A_B.jpg']),
}


# (01/11) o kit civil v2 (o agente do kit: docs/KIT_CIVIL_V2.md): as folhas game shot × nosso e kit de hoje × v2, por grupo
SHEETS = os.path.join(REPO, 'captures', 'game', 'models', 'civ_v2', '_sheets')
CIVIL = {
    'civ_carros': ('Kit civil v2: carros', ['folha_gameshot_carros.jpg', 'folha_kit_atual_x_v2_1.jpg']),
    'civ_casas': ('Kit civil v2: casas e muros', ['folha_gameshot_casas.jpg', 'folha_kit_atual_x_v2_2.jpg']),
    'civ_miudezas': ('Kit civil v2: miudezas e rua', ['folha_gameshot_miudezas.jpg', 'folha_kit_atual_x_v2_3.jpg']),
}


def images(key):
    out = []
    if key in CIVIL:
        for name in CIVIL[key][1]:
            p = os.path.join(SHEETS, name)
            if os.path.exists(p):
                out.append((name.split('.')[0], 'Game shot × nosso' if 'gameshot' in name else 'Kit de hoje × v2', p))
        return out
    if key in VARIANTS:
        prev = os.path.join(REPO, 'captures', 'game', 'models', VARIANTS[key][1], 'preview')
        for name in VARIANTS[key][2] + ['three_quarter_masks_game.png', 'game_cam_close_masks_game.png']:
            p = os.path.join(prev, name)
            if os.path.exists(p):
                label = {'folha': 'Ficha × A (aprovado) × B' if 'ficha' in name or 'arte' in name else 'A (o de hoje) × B',
                         'fire': 'Pronto pra atirar', 'three': 'Prévia B (3/4)', 'game': 'B na câmera do jogo'}
                out.append((name.split('.')[0], label.get(name.split('_')[0], name), p))
        return out
    fac = key.split('_')[0]
    shots = sorted(glob.glob(os.path.join(REPO, 'captures', 'game', 'elenco_%s_tour*' % fac, '*peca_%s.png' % key)), key=os.path.getmtime)
    if shots:
        out.append(('jogo', 'No jogo, de perto', shots[-1]))
    prev = os.path.join(REPO, 'captures', 'game', 'models', key, 'preview')
    for name, label in (('three_quarter_masks_game.png', 'Prévia (3/4)'), ('game_cam_close_masks_game.png', 'Prévia na câmera do jogo')):
        p = os.path.join(prev, name)
        if os.path.exists(p):
            out.append((name.split('_masks')[0], label, p))
    return out


def md_inline(s):
    s = html.escape(s)
    return re.sub(r'\*\*(.+?)\*\*', r'<strong>\1</strong>', s)


def main():
    os.makedirs(os.path.join(OUT, 'img'), exist_ok=True)
    units = json.load(open(os.path.join(REPO, 'Game', 'Config', 'RTS', 'Units.json'), encoding='utf-8'))
    units = units['units'] if isinstance(units, dict) and 'units' in units else units
    by = units if isinstance(units, dict) else {u.get('key'): u for u in units}
    pieces, total = [], 0
    for key in ORDER:
        f = ficha(key)
        u = by.get(key, {})
        imgs = []
        for tag, label, src in images(key):
            name = '%s_%s.jpg' % (key, tag)
            total += thumb(src, os.path.join(OUT, 'img', name))
            imgs.append({'src': 'img/' + name, 'label': label})
        name = VARIANTS[key][0] if key in VARIANTS else (CIVIL[key][0] if key in CIVIL else ((f or {}).get('title') or u.get('name', key)))
        if key in CIVIL:
            f = {'title': name, 'rows': {'Papel': 'o mapa: as peças que substituem as de hoje (mesmo nome e pegada) e as novas'},
                 'idea': 'Os pontos fracos que o agente apontou: a carcaça queimada ainda lê um pouco como camuflagem; picape, van e caminhão '
                         'passam do orçamento (~25% a cortar); faltam a mesquita e os prédios de cidade.', 'path': 'docs/KIT_CIVIL_V2.md'}
        pieces.append({'key': key, 'faction': key.split('_')[0], 'name': name,
                       'kind': u.get('kind', ''), 'role': (f or {}).get('rows', {}).get('Papel', ''),
                       'cost': (f or {}).get('rows', {}).get('Custo e tempo', ''), 'life': (f or {}).get('rows', {}).get('Vida e blindagem', ''),
                       'weapon': (f or {}).get('rows', {}).get('Arma', ''), 'idea': md_inline((f or {}).get('idea', '')),
                       'ficha': (f or {}).get('path', ''), 'imgs': imgs})
    tpl = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'elenco_gallery.html'), encoding='utf-8').read()
    page = tpl.replace('/*__DATA__*/[]', json.dumps(pieces, ensure_ascii=False))
    open(os.path.join(OUT, 'index.html'), 'w', encoding='utf-8').write(page)
    print('GALERIA', len(pieces), 'peças,', sum(len(p['imgs']) for p in pieces), 'imagens,', round(total / 1e6, 1), 'MB ->', OUT)


if __name__ == '__main__':
    main()
