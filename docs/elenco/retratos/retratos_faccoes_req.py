# (04/11, preparado na noite, NÃO rodado) Os retratos da China, da Rússia e da GLA (o botão da paleta e o cartão da seleção), pelo caminho
# aprovado na USA (docs/APROVADOS.md, "o caminho dos retratos do HUD"; a receita em retratos_usa_fechar_req.py): o Atelier (GPT pela
# conta do ChatGPT via Codex), em FOLHA EM GRADE (os botões 3×3 numa imagem 1:1, os cartões 2×3 numa 3:2), com a prévia de cada modelo do
# jogo como desenho e os retratos aprovados do M1 como estilo. O pedido dele (04/11): "amanhã continuemos a gerar [...] os unit portrait".
#
# ANTES DE RODAR, a decisão dele: a COR DE CADA FACÇÃO nos retratos (na USA, as faixas azuis). Hoje a cor do time da China é o
# vermelho; a da Rússia está em aberto (docs/elenco/ESTADO_VISUAL.md: "a cor do time da infantaria russa ainda é a da China"); a da GLA
# também. Preencher TEAM e rodar uma facção por vez:
#   python docs/elenco/retratos/retratos_faccoes_req.py china [--only buttons|cards]
# Depois: o recorte de cada folha com Game/Tools/portrait_sheet_cut.py (a ordem das células = a ordem de cada lista aqui).
import json
import os
import sys
import urllib.request

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
KIT = os.path.join(REPO, 'docs', 'concept', 'hud', 'kit', 'photos')
MODELS = os.path.join(REPO, 'captures', 'game', 'models')
INF = os.path.join(REPO, 'captures', 'concept', '_attach', 'retratos_infantaria_v3')
TEAM = {'china': 'deep red', 'russia': None, 'gla': None}   # a China: o vermelho (a cor do time dela no jogo; 04/11, "monta todos unit portrait da China")

# a chave do jogo, a pasta do modelo (a prévia em <pasta>/preview/three_quarter_masks_game.png) e o que é, em inglês
PIECES = {
    'china': [
        ('china_command_center', 'china_command_center', 'the Command Center headquarters building'),
        ('china_power_plant', 'china_power_plant', 'the nuclear Power Plant with the cooling towers'),
        ('china_supply_center', 'china_supply_center', 'the Supply Center building'),
        ('china_barracks', 'china_barracks', 'the Barracks building'),
        ('china_war_factory', 'china_war_factory', 'the War Factory building'),
        ('china_propaganda_center', 'china_propaganda_center_v5', 'the Propaganda Center building'),
        ('china_gattling_cannon', 'china_gattling_cannon_v2', 'the Gattling Cannon defensive turret'),
        ('china_dozer', 'china_dozer', 'the Dozer construction vehicle'),
        ('china_supply_truck', 'china_supply_truck', 'the Supply Truck'),
        ('china_type99', 'china_type99', 'the Type 99 main battle tank'),
        ('china_gattling_tank', 'china_gattling_tank', 'the Gattling Tank'),
        ('china_inferno_cannon', 'china_inferno_cannon', 'the Inferno Cannon artillery vehicle'),
        ('china_overlord', 'china_overlord', 'the Overlord super-heavy tank'),
        ('china_rifleman', 'INF', 'the rifleman infantry soldier'),
        ('china_tank_hunter', 'INF', 'the Tank Hunter anti-tank infantry soldier'),
    ],
    'russia': [
        ('russia_command_center', 'russia_command_center_v3', 'the Shtab headquarters fortress with the white onion-dome radar'),
        ('russia_power_plant', 'russia_power_plant', 'the Power Plant'),
        ('russia_supply_center', 'russia_supply_center', 'the Supply Center'),
        ('russia_barracks', 'russia_barracks_v2', 'the Kazarma barracks building'),
        ('russia_war_factory', 'russia_war_factory', 'the War Factory'),
        ('russia_institute', 'russia_institute', 'the Institute research building'),
        ('russia_pantsir_site', 'russia_pantsir_site', 'the fixed Pantsir air-defense site'),
        ('russia_dozer', 'russia_dozer', 'the IMR engineering vehicle'),
        ('russia_supply_truck', 'russia_supply_truck', 'the Supply Truck'),
        ('russia_t90m', 'russia_t90m', 'the T-90M main battle tank'),
        ('russia_bmpt', 'russia_bmpt_v2', 'the BMPT tank support vehicle'),
        ('russia_pantsir', 'russia_pantsir', 'the mobile Pantsir air-defense vehicle'),
        ('russia_tos1a', 'russia_tos1a', 'the TOS-1A thermobaric rocket launcher'),
        ('russia_t14', 'russia_t14_v2', 'the T-14 Armata tank'),
        ('russia_motostrelok', 'INF', 'the motor-rifle infantry soldier'),
        ('russia_kornet', 'INF', 'the Kornet anti-tank infantry soldier'),
    ],
    'gla': [
        ('gla_command_center', 'gla_command_center', 'the Command Center compound'),
        ('gla_supply_stash', 'gla_supply_stash_v5', 'the Supply Stash'),
        ('gla_barracks', 'gla_barracks_v5', 'the Barracks'),
        ('gla_arms_dealer', 'gla_arms_dealer', 'the Arms Dealer'),
        ('gla_palace', 'gla_palace_v2', 'the Palace with the domes'),
        ('gla_stinger_site', 'gla_stinger_site_v5', 'the Stinger Site'),
        ('gla_sam_site', 'gla_sam_site', 'the SAM site'),
        ('gla_bunker', 'gla_bunker', 'the Bunker'),
        ('gla_worker', 'INF', 'the Worker (a civilian builder with a shovel, no weapon)'),
        ('gla_supply_pickup', 'gla_supply_pickup', 'the supply pickup truck'),
        ('gla_technical', 'TOUR', 'the Technical pickup truck with the heavy machine gun'),
        ('gla_tank', 'gla_tank_v2', 'the GLA tank'),
        ('gla_quad_cannon', 'gla_quad_cannon', 'the Quad Cannon anti-air vehicle'),
        ('gla_toxin_tractor', 'gla_toxin_tractor', 'the Toxin Tractor'),
        ('gla_rocket_buggy', 'gla_rocket_buggy', 'the Rocket Buggy'),
        ('gla_rebel', 'INF', 'the Rebel rifleman (a guerrilla fighter, no military kit)'),
        ('gla_rpg', 'INF', 'the RPG trooper (a guerrilla fighter with the RPG launcher)'),
    ],
}


def ref(key, folder):
    if folder == 'INF':
        return os.path.join(INF, f'ref_{key}_v3_front_back.png')
    if folder == 'TOUR':   # sem prévia do Blender: a foto da peça no jogo
        return os.path.join(REPO, 'captures', 'game', 'elenco_gla_tour_v63', f'lab_peca_{key}.png')
    return os.path.join(MODELS, folder, 'preview', 'three_quarter_masks_game.png')


def req(title, aspect, atts, prompt):
    body = {'title': title, 'session': 'Elenco', 'kind': 'image', 'models': {'gpt': 1}, 'aspect': aspect,
            'attachments': [{'path': p, 'label': l} for p, l in atts], 'prompt': prompt}
    r = urllib.request.Request('http://127.0.0.1:8140/api/requests', data=json.dumps(body).encode(), headers={'Content-Type': 'application/json'})
    d = json.loads(urllib.request.urlopen(r, timeout=60).read().decode())
    print(d['id'], d['folder'])


def main():
    fac = sys.argv[1]
    only = sys.argv[sys.argv.index('--only') + 1] if '--only' in sys.argv else None
    color = TEAM[fac]
    if not color:
        sys.exit(f'falta a decisão dele: a cor da {fac} nos retratos (TEAM no topo deste arquivo)')
    pieces = PIECES[fac]
    for p in pieces:
        assert os.path.exists(ref(p[0], p[1])), ('sem a referência', p)
    team = (f'Keep each design exactly as in its reference from our game, do not add or remove parts; only make the materials rich and '
            f'real; the team markings (stripes, panels, flags) are {color}. Soldiers keep the uniform and kit of their reference exactly, '
            f'with a realistic human face, serious and calm. ')
    # os botões: folhas de 9 (3×3, 1:1), no cinza de estúdio do botão do M1
    if only in (None, 'buttons'):
        for s in range(0, len(pieces), 9):
            part = pieces[s:s + 9]
            atts = [(ref(k, f), k) for k, f, _ in part] + [(os.path.join(KIT, 'buttons', 'usa_battle_tank.png'), 'estilo_botao_m1')]
            n = len(part)
            cells = '; '.join(f'{i + 1}. {d} of image {i + 1}' for i, (_, _, d) in enumerate(part))
            req(f'Retratos {fac}, botões {s // 9 + 1}', '1:1', atts,
                f'A sheet of {n} game unit portrait icons for a modern real-time strategy game, laid out as a grid of 3 columns and 3 rows of '
                f'equal square cells separated by thin black gutters (cells after the {n}th stay plain black). Every cell is a separate icon in '
                f'the exact style of image {n + 1}: one subject alone, a photorealistic AAA game asset render with rich, weathered materials, '
                f'three-quarter view from slightly above with the front pointing to the left, the whole subject inside its cell with a small '
                f'margin (soldiers framed from the knees up), on a plain dark charcoal studio background with a soft lighter gradient behind '
                f'the subject and a soft contact shadow, warm key light from the upper left and a cool rim light. All cells share the same '
                f'background, light and camera. ' + team + f'\nIn reading order: {cells}.\nNo text, no labels, no numbers in the image.')
    # os cartões: folhas de 6 (2×3, 3:2), no deserto padrão dos cartões do M1 e do Dozer
    if only in (None, 'cards'):
        for s in range(0, len(pieces), 6):
            part = pieces[s:s + 6]
            atts = [(ref(k, f), k) for k, f, _ in part] + [(os.path.join(KIT, 'card', 'usa_battle_tank.png'), 'estilo_cartao_m1'),
                                                          (os.path.join(KIT, 'card', 'usa_dozer.png'), 'estilo_cartao_dozer')]
            n = len(part)
            cells = '; '.join(f'{i + 1}. {d} of image {i + 1}' for i, (_, _, d) in enumerate(part))
            req(f'Retratos {fac}, cartões {s // 6 + 1}', '3:2', atts,
                f'A sheet of {n} selection portraits for a modern real-time strategy game, laid out as a grid of 2 columns and 3 rows of equal '
                f'wide panoramic cells (each cell about 2.4 times wider than tall), separated by thin black gutters (cells after the {n}th '
                f'stay plain black). Every cell is a separate portrait in the exact style and with the same standard background as images '
                f'{n + 1} and {n + 2}: one subject alone, standing still, no action; a flat dusty desert plain with small dry bushes and pebbles '
                f'in focus in front, strongly blurred distant hills and a faint village on the horizon (shallow depth of field), hazy warm '
                f'light, pale dusty sky; a low camera, three-quarter view from the front-left; the subject centered slightly left, nothing '
                f'touching the top or bottom edge of its cell (soldiers full-length, filling about two thirds of the cell height). '
                f'Photorealistic AAA game asset render with rich, weathered materials. All cells share the same background, light and '
                f'camera. ' + team + f'\nIn reading order: {cells}.\nNo text, no labels, no numbers in the image.')


if __name__ == '__main__':
    main()


# (04/11) o conserto: na 1ª folha de botões da China o GPT desenhou o Command Center igual ao Propaganda Center (a torre com as
# faixas). Um botão só, com a prévia do QG, o cartão certo dele (a mesma fortaleza) e o estilo do botão do M1:
#   python -c "import importlib.util as u; s=u.spec_from_file_location('r','docs/elenco/retratos/retratos_faccoes_req.py'); m=u.module_from_spec(s); s.loader.exec_module(m); m.fix_china_cc()"
def fix_china_cc():
    atts = [(ref('china_command_center', 'china_command_center'), 'china_command_center'),
            (os.path.join(KIT, 'card', 'china_command_center.png'), 'cartao_certo'),
            (os.path.join(KIT, 'buttons', 'usa_battle_tank.png'), 'estilo_botao_m1')]
    req('Retratos china, botão do Command Center (conserto)', '1:1', atts,
        'One single game unit portrait icon for a modern real-time strategy game, in the exact style of image 3: a photorealistic AAA game '
        'asset render on a plain dark charcoal studio background with a soft lighter gradient behind the subject and a soft contact '
        'shadow, three-quarter view from slightly above, the whole building inside the frame with a small margin, warm key light from the '
        'upper left and a cool rim light. The subject is the Chinese Command Center of image 1, exactly as it looks in image 2: the wide '
        'square stone fortress with the battlements, the three-tiered green pagoda roof in the middle, the stairs climbing the side wall, '
        'the lattice radio mast and the radar dish, the red banners and the red star. It is NOT a tall tower with loudspeakers. '
        'The team markings are deep red. No text, no labels, no numbers in the image.')
