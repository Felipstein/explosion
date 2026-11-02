"""Os prompts do HUD de cada peça nova do elenco (31/10/2026; o pedido: "já prepara os prompts no final quando você fizer tudo pra eu gerar
as imagens no HUD"). Gera docs/elenco/PROMPTS_HUD.md (cada prompt inteiro, pronto pra colar: a regra "prompts sempre completos") e
uma pasta de anexos por peça em captures/concept/_attach/hud_<chave>/ (1_estilo, 2_raiz da facção, 3_forma do modelo provisório),
ancorados nas raízes aprovadas (nunca numa peça derivada: "ancorar na raiz").

    python Game/Tools/hud_prompts.py

O botão do kit: 256 × 134 (unidade) ou 256 × 116 (prédio), o fundo escuro do usa_battle_tank; o cartão: 728 × 300, a areia do
deserto com profundidade de campo do usa_dozer (docs/concept/hud/kit/photos). As regras dos prompts: docs/CONCEITO_UNIDADES.md §1
(inglês; nunca C&C/Generals/EA; fictício inspirado no real; cores desbotadas, nunca "bold").
"""
import os
import shutil

REPO = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
KIT = os.path.join(REPO, 'docs', 'concept', 'hud', 'kit', 'photos')
UNITS = os.path.join(REPO, 'docs', 'concept', 'units')
MODELS = os.path.join(REPO, 'captures', 'game', 'models')
ATTACH = os.path.join(REPO, 'captures', 'concept', '_attach')
OUT = os.path.join(REPO, 'docs', 'elenco', 'PROMPTS_HUD.md')

FACTION = {
    'china': ('the Chinese army faction: dark, massive, disciplined; dark olive-drab paint with a faded blocky digital camouflage, heavy '
              'square brutal shapes, a red star, the player color only in small folded red tarps and red bands; buildings in dark '
              'olive-tinted concrete with layered green-glazed pagoda roofs with red trim, loudspeaker towers and red banners',
              os.path.join(UNITS, 'china_mbt_player_color.webp')),
    'gla': ('the guerrilla faction: improvised, scrap, insurgent; faded sand-yellow paint with worn hand-painted dark green stripes, '
            'welded mismatched plates, sandbags, spare tires, tied cloths, small green flags, rust; buildings in sand-colored adobe '
            'with rusty corrugated sheet, tarps and camouflage nets, green hand-painted scribbles',
            os.path.join(UNITS, 'gla_technical_sheet.webp')),
    'usa': ('the American faction: light, cold, high-tech; light gray almost white paint with small faded blue details following the '
            'panels, clean angular forms, sensors with blue lenses, a blue tarp tied on the engine deck',
            os.path.join(UNITS, 'usa_mbt_player_color.webp')),
}

# a peça: (facção, prédio?, o nome de tela, a descrição em inglês: o que lê de cima e a assinatura)
PIECES = {
    'china_command_center': ('china', True, 'Command Center', 'a heavy fortress-palace: a sloped-wall dark concrete podium with a three-tier green pagoda hall on top, a tall radio and loudspeaker mast, a rotating lattice radar on the podium roof, a red paifang gate over the big front garage door, a large red star painted on the front yard, stone lions by the stairs, red lanterns under the eaves'),
    'china_power_plant': ('china', True, 'Nuclear Reactor', 'two hourglass cooling towers releasing white steam, a concrete reactor dome between them with a red ring and a yellow-black radiation trefoil painted on top, thick pipes linking towers and dome, a low control block with lit windows, transformers with insulators, red warning lights on the tower rims, a pale blue glow through a round core window'),
    'china_supply_center': ('china', True, 'Supply Center', 'a long warehouse with a green pagoda eave and a big red gantry crane over the unloading dock, hazard-striped dock platform, stacks of olive crates with red stars, a small office with windows, a water tank and a thin striped chimney'),
    'china_barracks': ('china', True, 'Barracks', 'a walled training yard with a red pagoda gate (paifang) and a tall red flag mast, a long one-storey dormitory with a green pagoda roof and a row of windows, an obstacle course in the yard, a loudspeaker on a pole, straw training dummies and red banners'),
    'china_war_factory': ('china', True, 'War Factory', 'a dark industrial hangar with a layered green pagoda roof, a glowing open furnace, a big red gantry frame over the vehicle exit, a large red star painted on the floor of the exit ramp, a red-and-white striped chimney with a walkway, pipes and vents on the roof'),
    'china_propaganda_center': ('china', True, 'Propaganda Center', 'a giant loudspeaker tower (a lattice tower with a big cluster of horn speakers on top), a huge red propaganda screen with a golden star on the front facade, a concrete block with a pagoda eave, a speech balcony with a pulpit and red banners, posters, cables, a broadcast antenna'),
    'china_gattling_cannon': ('china', True, 'Gattling Cannon', 'a round dark concrete bunker with a sloped skirt and a low armored turret on top carrying a six-barrel rotary cannon, an ammunition box, a gun shield, an antenna'),
    'china_dozer': ('china', False, 'Dozer', 'a heavy tracked bulldozer with a large front blade, a low square cab with a grille and a red star on the door, a rear ripper, an orange beacon on the cab roof, dark olive paint'),
    'china_supply_truck': ('china', False, 'Supply Truck', 'a low six-wheel military cargo truck with a flat-nosed cab and a red star, the cargo bed holding four olive supply crates in two rows, a folded canvas tarp, dusty tires'),
    'china_type99': ('china', False, 'Battlemaster', 'a main battle tank with a long arrowhead shield-shaped turret pointed at the front, a single long gun with a red band, a U-shaped cargo cage around the turret rear holding a folded red tarp, side skirts with cutouts over each road wheel, blocky digital camouflage'),
    'china_gattling_tank': ('china', False, 'Gattling Tank', 'a lighter tracked anti-air vehicle with a turret carrying two six-barrel rotary cannon drums on its sides and a rotating search radar dish on top, blocky digital camouflage'),
    'china_inferno_cannon': ('china', False, 'Inferno Cannon', 'a tracked self-propelled howitzer with a big square turret at the rear and a very long gun with a muzzle brake, a spade at the back, heat-stained barrel tip, blocky digital camouflage'),
    'china_overlord': ('china', False, 'Overlord', 'a gigantic super-heavy tank one and a half times a normal tank: a huge blocky turret with TWO thick guns side by side, each with a red band, very wide tracks with many road wheels, heavy armored side skirts, a commander cupola with a machine gun, a folded red tarp behind the turret, a small red banner; it must look terrifying'),
    'china_rifleman': ('china', False, 'Red Guard', 'a pair of disciplined Chinese infantrymen in dark olive uniforms and helmets with a red band, carrying assault rifles, in a firing stance'),
    'china_tank_hunter': ('china', False, 'Tank Hunter', 'a Chinese anti-tank infantryman in a dark olive uniform with a large shoulder-fired rocket launcher tube with a big sight, kneeling to aim'),
    'gla_command_center': ('gla', True, 'Command Center', 'a walled adobe compound with a low-domed command building, a tall crooked scrap-lattice radio antenna with guy wires and an old satellite dish, a tarp and camouflage net over the yard, a scrapped pickup wreck in a corner, a diesel generator with drums, a water tank, outer stairs, green flags and hand-painted green scribbles'),
    'gla_supply_stash': ('gla', True, 'Supply Stash', 'a pile of supply crates under a tarp and a camouflage net, a dark tunnel mouth where crates go down, a scrap A-frame crane with chain and hoist, pallets, sacks, a corrugated sheet fence'),
    'gla_barracks': ('gla', True, 'Barracks', 'a two-storey adobe house next to a training yard with a tire course and sandbags, shooting targets, a climbing rope on a pole, a tarp shelter, fire barrels, green flags and scribbles, an outer stair to the roof'),
    'gla_arms_dealer': ('gla', True, 'Arms Dealer', 'a closed scrap warehouse with a weapons cage, a dirt ramp going down under a camouflage net where vehicles come out, crates of weapons, rusty corrugated roof, green flags'),
    'gla_palace': ('gla', True, 'Palace', 'a palace with a large worn turquoise tiled dome with a green band, a front arcade of columns, a courtyard with a dry fountain, rounded crenellated walls, potted palm trunks, carpets hanging from balconies, sandbags in the windows, a machine gun on the roof (never a mosque: no minaret, no crescent)'),
    'gla_stinger_site': ('gla', True, 'Stinger Site', 'a round sandbag nest with a rotating mount of three shoulder-fired anti-air missile tubes and a partial camouflage net'),
    'gla_supply_pickup': ('gla', False, 'Supply Pickup', 'an old patched civilian pickup truck loaded with supply crates in the bed, a tarp and ropes, a spare tire, a small green flag, a scrap bumper, dusty tires'),
    'gla_quad_cannon': ('gla', False, 'Quad Cannon', 'an old 4x4 truck carrying a four-barrel heavy machine gun mount with a seat and a gun shield in its bed, sandbags around it, ammunition boxes, green flags'),
    'gla_toxin_tractor': ('gla', False, 'Toxin Tractor', 'an old farm tractor with huge rear wheels carrying a large sickly yellow-green poison tank with a hazard symbol, a front spray boom with nozzles, hoses, an extra drum'),
    'gla_rocket_buggy': ('gla', False, 'Rocket Buggy', 'a tube-frame sand buggy with an exposed rear engine, two seats, big sand tires and a tall rocket launcher of two pods of tubes mounted high at the back, small green flags'),
    'gla_worker': ('gla', False, 'Worker', 'a guerrilla laborer in dusty work clothes and a headwrap carrying a shovel and a toolbag'),
    'gla_rebel': ('gla', False, 'Rebel', 'a guerrilla fighter in khaki clothes and a light headwrap with an assault rifle, in an ambush crouch'),
    'gla_rpg': ('gla', False, 'RPG Trooper', 'a guerrilla anti-tank fighter in khaki clothes and a headwrap aiming a shoulder-fired RPG with a bulbous warhead'),
    'usa_paladin': ('usa', False, 'Paladin', 'a heavy main battle tank, the bigger brother of the standard tank: a wedge-shaped faceted turret, thicker side skirts, a single long gun with two blue rings, and its signature: a small point-defense laser turret with a blue lens on the turret roof'),
    'usa_javelin': ('usa', False, 'Javelin', 'an American infantryman in a light desert uniform with a blue helmet band, aiming a fire-and-forget anti-tank missile launcher tube with a command launch unit on its side'),
}


def model_image(key):
    for name in ('three_quarter_masks_game.png', 'game_cam_close_masks_game.png', 'three_quarter.png'):
        p = os.path.join(MODELS, key, 'preview', name)
        if os.path.exists(p):
            return p
    return None


def button_prompt(key, fac, bld, name, desc):
    size = '256 × 116' if bld else '256 × 134'
    style = FACTION[fac][0]
    return (f"Create a game HUD button portrait of a fictional {'building' if bld else 'unit'} for a serious modern military real-time "
            f"strategy game: the {name}, {desc}. Faction style: {style}. Match EXACTLY the style of attached image 1 (an approved HUD "
            f"button of this game): a three-quarter view from slightly above, the subject centered and filling the frame with a small "
            f"margin, a plain dark charcoal studio background with a soft vignette, soft key light from the upper left, realistic "
            f"weathered materials with dust, worn paint on the edges and dirt in the crevices, high detail but readable shapes. Use "
            f"attached image 2 only as the reference for this faction's paint, colors and player-color details. Use attached image 3 "
            f"only as the reference for the shape, silhouette and layout (it is a plain gray blockout from the game, ignore its colors "
            f"and lighting). Wide landscape framing, final image {size} pixels. Faded, dusty colors, never bold or saturated. No text, "
            f"no logos, no frame, no UI, no people unless the subject is infantry.")


def card_prompt(key, fac, bld, name, desc):
    style = FACTION[fac][0]
    return (f"Create a wide cinematic game HUD card picture of a fictional {'building' if bld else 'unit'} for a serious modern military "
            f"real-time strategy game: the {name}, {desc}. Faction style: {style}. Match EXACTLY the style of attached image 1 (an "
            f"approved HUD card of this game): the subject in a three-quarter view from a low-high angle standing on a dusty desert "
            f"plain with small dry shrubs and stones, a hazy desert horizon with distant hills, shallow depth of field, warm late-"
            f"afternoon light, realistic weathered materials with dust, worn paint on the edges and dirt in the crevices. Use attached "
            f"image 2 only as the reference for this faction's paint, colors and player-color details. Use attached image 3 only as the "
            f"reference for the shape, silhouette and layout (it is a plain gray blockout from the game, ignore its colors). Very wide "
            f"landscape framing, final image 728 × 300 pixels, the subject in the center third. Faded, dusty colors, never bold or "
            f"saturated. No text, no logos, no frame, no UI.")


def main():
    lines = ['# Os prompts do HUD do elenco novo (31/10/2026)', '',
             'Um prompt por imagem, inteiro e pronto pra colar (um chat novo por imagem; GPT pra o detalhe, Gemini pra a ficha limpa).',
             'Os anexos de cada peça estão numerados em `captures/concept/_attach/hud_<chave>/`: **1_** o estilo (a foto aprovada do kit:',
             'o botão do Battle Tank ou o cartão do Dozer), **2_** a raiz da facção (a ficha com a cor do jogador), **3_** a forma (a prévia do',
             'modelo provisório do jogo). O botão sai em `docs/concept/hud/kit/photos/buttons/<chave>.png` e o cartão em `.../card/<chave>.png`',
             '(o HUD acha pela chave do Units.json).', '']
    for key, (fac, bld, name, desc) in PIECES.items():
        folder = os.path.join(ATTACH, 'hud_' + key)
        os.makedirs(folder, exist_ok=True)
        style_btn = os.path.join(KIT, 'buttons', 'usa_war_factory.png' if bld else 'usa_battle_tank.png')
        style_card = os.path.join(KIT, 'card', 'usa_war_factory.png' if bld else 'usa_dozer.png')
        shutil.copy2(style_btn, os.path.join(folder, '1_estilo_botao.png'))
        shutil.copy2(style_card, os.path.join(folder, '1_estilo_cartao.png'))
        root = FACTION[fac][1]
        if os.path.exists(root):
            shutil.copy2(root, os.path.join(folder, '2_raiz_' + fac + os.path.splitext(root)[1]))
        img = model_image(key)
        if img:
            shutil.copy2(img, os.path.join(folder, '3_forma.png'))
        lines += [f'## {key}', '', f'**{name}** ({fac.upper()}). Anexos: `captures/concept/_attach/hud_{key}/` (o 1_ do botão no botão, o 1_ do '
                  f'cartão no cartão; o 2_ e o 3_ nos dois).', '', '**Botão:**', '', '```', button_prompt(key, fac, bld, name, desc), '```', '',
                  '**Cartão:**', '', '```', card_prompt(key, fac, bld, name, desc), '```', '']
    with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines))
    print('HUD', len(PIECES), 'peças ->', OUT)


if __name__ == '__main__':
    main()
