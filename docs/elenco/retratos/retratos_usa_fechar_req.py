# (04/11) fechar os retratos da USA ("pra fechar a USA, faltou os unit portrait da infantaria, faça isso, já joga no jogo"): o Ranger e o
# Javelin v3 (os de hoje no kit são o boneco de blocos antigo) e os cartões das unidades que ainda não têm (Stryker, Paladin, Linebacker,
# Tomahawk, Raptor, Comanche, Supply Truck, Supply Center). Pelo caminho aprovado (o Atelier, GPT pela conta do ChatGPT via Codex) e em
# FOLHA EM GRADE (3 pedidos pra 14 peças). O estilo: o botão do M1 (o cinza de estúdio) e os cartões do M1, do Dozer e do Chinook (o
# deserto padrão, parado, sem ação). As referências da infantaria: a frente e as costas recortadas da folha aprovada da infantaria v3
# (docs/aprovados/modelos/2026-11-03_infantaria_v3_9_tipos_APROVADA.jpg, as colunas 1 e 2).
import json
import urllib.request

SP = r'C:\Users\luisf\AppData\Local\Temp\claude\C--Users-luisf-explosion\12f1e755-ddf7-4e41-a834-371a3dfa6f7e\scratchpad\retratos_infantaria'
KIT = r'C:\Users\luisf\explosion\docs\concept\hud\kit\photos'
U9 = r'C:\Users\luisf\explosion\captures\concept\206_retratos_usa_folha_de_9_unidades\_anexos'
SC = r'C:\Users\luisf\explosion\captures\concept\190_retrato_supply_center_cartao\_anexos\1_lab_supply_center_100.png'
RANGER, JAV = SP + r'\ref_usa_ranger_v3_front_back.png', SP + r'\ref_usa_javelin_v3_front_back.png'
SOLDIER = ('Keep each soldier exactly as in his reference (front and back views of our game model): the US Army soldier in the tan-and-khaki '
           'desert camouflage uniform, the coyote-brown plate carrier with the three magazine pouches, the tan helmet with the goggles, the '
           'knee pads and gloves; add only a small dark-blue team patch on the helmet band and on the shoulder (our USA team color). '
           'Realistic human face, serious and calm, short stubble. ')


def req(title, aspect, atts, prompt):
    body = {'title': title, 'session': 'Elenco', 'kind': 'image', 'models': {'gpt': 1}, 'aspect': aspect,
            'attachments': [{'path': p, 'label': l} for p, l in atts], 'prompt': prompt}
    r = urllib.request.Request('http://127.0.0.1:8140/api/requests', data=json.dumps(body).encode(), headers={'Content-Type': 'application/json'})
    d = json.loads(urllib.request.urlopen(r, timeout=60).read().decode())
    print(d['id'], d['folder'])


# 1) os 2 botões da infantaria (o cinza de estúdio do botão do M1)
req('Retratos USA, botões da infantaria (Ranger e Javelin v3)', '3:2',
    [(RANGER, 'ranger_v3'), (JAV, 'javelin_v3'), (KIT + r'\buttons\usa_battle_tank.png', 'estilo_botao_m1')],
    'A sheet of two game unit portrait icons for a modern real-time strategy game: two equal SQUARE cells side by side, centered, '
    'separated by a thin black gutter, with plain black bars filling the rest of the image. Every cell is a separate icon in the exact '
    'style of image 3: a photorealistic AAA game asset render on a plain dark charcoal studio background with a soft lighter gradient '
    'behind the subject, warm key light from the upper left and a cool rim light. Each soldier is framed from the knees up, in a '
    'three-quarter view turned to the left, standing still, calm and ready, no action, no firing. ' + SOLDIER +
    '\nLeft cell: 1. the Ranger rifleman of image 1, holding his carbine at low ready across the chest.'
    '\nRight cell: 2. the Javelin anti-tank gunner of image 2, holding the Javelin missile launcher tube (olive drab, with the command '
    'launch unit sight box at its rear) resting on his right shoulder, pointing up and back, the tube clearly visible.'
    '\nNo text, no labels, no numbers in the image.')

# 2) cartões A: a infantaria + 4 veículos (o deserto padrão dos cartões do M1 e do Dozer)
req('Retratos USA, cartões A (Ranger, Javelin, Stryker, Paladin, Linebacker, Tomahawk)', '3:2',
    [(RANGER, 'ranger_v3'), (JAV, 'javelin_v3'), (U9 + r'\5_three_quarter_masks_game.png', 'stryker'),
     (U9 + r'\6_three_quarter_masks_game.png', 'paladin'), (U9 + r'\4_three_quarter_masks_game.png', 'linebacker'),
     (U9 + r'\3_three_quarter_masks_game.png', 'tomahawk'), (KIT + r'\card\usa_battle_tank.png', 'estilo_cartao_m1'),
     (KIT + r'\card\usa_dozer.png', 'estilo_cartao_dozer')],
    'A sheet of six selection portraits for a modern real-time strategy game, laid out as a grid of 2 columns and 3 rows of equal wide '
    'panoramic cells (each cell about 2.4 times wider than tall), separated by thin black gutters. Every cell is a separate portrait in '
    'the exact style and with the same standard background as images 7 and 8: one subject alone, standing still, no action; a flat dusty '
    'desert plain with small dry bushes and pebbles in focus in front, strongly blurred distant hills and a faint village on the horizon '
    '(shallow depth of field), hazy warm light, pale dusty sky; a low camera, three-quarter view from the front-left; the subject '
    'centered slightly left, with clear sky above it and ground below it (nothing touches the top or bottom edge of its cell). '
    'Photorealistic AAA game asset render with rich, weathered materials (worn light grey military paint with the blue team stripes, '
    'dust in the crevices, scratched metal). All six cells share the same background, light and camera. Keep each design exactly as in '
    'its reference from our game, do not add or remove parts; only make the materials rich and real. ' + SOLDIER +
    'The soldiers are seen full-length, standing, at a larger scale so the figure fills about two thirds of the cell height.'
    '\nTop row, left to right: 1. the Ranger rifleman of image 1, carbine at low ready; 2. the Javelin gunner of image 2, the olive '
    'Javelin launcher tube resting on his right shoulder.'
    '\nMiddle row, left to right: 3. the Stryker 8x8 armored carrier of image 3; 4. the Paladin self-propelled howitzer of image 4.'
    '\nBottom row, left to right: 5. the Linebacker anti-air vehicle of image 5; 6. the Tomahawk launcher of image 6, its launcher '
    'lowered for travel.'
    '\nNo text, no labels, no numbers in the image.')

# 3) cartões B: os aéreos (em voo, como o cartão do Chinook), o Supply Truck, o Supply Center e uma 2ª tomada da infantaria
req('Retratos USA, cartões B (Raptor, Comanche, Supply Truck, Supply Center, Ranger e Javelin 2)', '3:2',
    [(U9 + r'\7_three_quarter_masks_game.png', 'raptor'), (U9 + r'\8_three_quarter_masks_game.png', 'comanche'),
     (U9 + r'\2_three_quarter_masks_game.png', 'supply_truck'), (SC, 'supply_center'), (RANGER, 'ranger_v3'), (JAV, 'javelin_v3'),
     (KIT + r'\card\usa_battle_tank.png', 'estilo_cartao_m1'), (KIT + r'\card\usa_chinook.png', 'estilo_cartao_chinook')],
    'A sheet of six selection portraits for a modern real-time strategy game, laid out as a grid of 2 columns and 3 rows of equal wide '
    'panoramic cells (each cell about 2.4 times wider than tall), separated by thin black gutters. Every cell is a separate portrait in '
    'the exact style and with the same standard desert background as images 7 and 8: one subject alone, no action; a dusty desert plain '
    'with small dry bushes, strongly blurred distant hills and a faint village (shallow depth of field), hazy warm light, pale dusty sky; '
    'three-quarter view from the front-left; the subject centered slightly left, nothing touching the top or bottom edge of its cell. '
    'The aircraft fly calmly above the desert seen from slightly above, exactly like the Chinook in image 8 (no weapons firing); the '
    'ground subjects stand still with a low camera like image 7. Photorealistic AAA game asset render with rich, weathered materials '
    '(worn light grey military paint with the blue team stripes, dust, scratched metal). All six cells share the same background, light '
    'and camera height for their kind. Keep each design exactly as in its reference from our game, do not add or remove parts. ' + SOLDIER +
    'The soldiers are seen full-length, standing, filling about two thirds of the cell height.'
    '\nTop row, left to right: 1. the F-22 Raptor jet of image 1, in flight; 2. the attack helicopter of image 2 with its rotor blades, '
    'in flight.'
    '\nMiddle row, left to right: 3. the Supply Truck of image 3 (the 6x6 truck with the yellow crane behind the cab and the crates); '
    '4. the Supply Center building of image 4, standing on the desert like a building portrait.'
    '\nBottom row, left to right: 5. the Ranger rifleman of image 5, carbine at low ready; 6. the Javelin gunner of image 6, the olive '
    'Javelin launcher tube resting on his right shoulder.'
    '\nNo text, no labels, no numbers in the image.')
