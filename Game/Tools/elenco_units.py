"""As fichas do elenco das 3 facções no Units.json (31/10/2026, docs/ELENCO.md): os números de cada prédio, veículo e soldado novo
da China, da GLA e da USA (Strategy Center, Paladin, Javelin), o que cada um constrói e treina, e o desenho (os modelos provisórios
dos Game/Tools/model_<chave>.py, importados pelo game.ps1 factionmodels).

Rodar de novo é seguro: cada ficha daqui substitui a do arquivo pela chave (as outras ficam como estão, com o texto de antes:
units_json_io). Os números do ZH 1.04 convertidos (1 unidade = 0,2633 m) estão no docs/ELENCO.md com o motivo de cada mudança.
    python Game/Tools/elenco_units.py
"""
import copy
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from units_json_io import load_units, save_units  # noqa: E402

ZERO_MOVE = {'radius_m': 0, 'path_radius_m': 0, 'speed_mps': 0, 'accel_mps2': 0, 'brake_mps2': 0, 'turn_dps': 0,
             'turret_turn_dps': 0, 'turret_idle_return_s': 0}


def mesh(dest, name, inner=None):
    """O caminho da malha importada: <destino>/<SM>/StaticMeshes/<SM> (ou o nome de dentro, pra roda: wheel_*)."""
    return f'{dest}/{name}/StaticMeshes/{inner or name}'


def structure(key, name, source, hp, size, cost, build_s, sight=52.7, energy=0, requires=None, **extra):
    u = {'key': key, 'name': name, '_source': source, 'kind': 'structure', 'max_hp': hp, 'armor': 'structure'}
    u.update(ZERO_MOVE)
    u.update({'sight_m': sight, 'size_m': size, 'cost': cost, 'build_time_s': build_s, 'energy': energy,
              'counts_for_victory': extra.pop('counts_for_victory', True)})
    if requires:
        u['requires'] = requires
    u.update(extra)
    return u


MODELS = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'captures', 'game', 'models')


def damage_states(dest, prefix):
    """(31/10) Os estados de dano do prédio que o destrutor fez (Tools/model_damage.py: o _Damaged, o _Wrecked, a _Ruin e o fogo de cada
    estado no _damage.json); sem os arquivos, nada (o prédio fica como antes)."""
    key = dest.rsplit('/', 1)[-1]
    folder = os.path.join(MODELS, key)
    js = os.path.join(folder, prefix + '_damage.json')
    if not all(os.path.exists(os.path.join(folder, f'{prefix}_{s}.glb')) for s in ('Damaged', 'Wrecked', 'Ruin')) or not os.path.exists(js):
        return None
    import json
    d = json.load(open(js, encoding='utf-8'))
    return {'damaged': mesh(dest, f'{prefix}_Damaged'), 'wrecked': mesh(dest, f'{prefix}_Wrecked'), 'ruin': mesh(dest, f'{prefix}_Ruin'),
            'fire_damaged_m': d.get('fire_damaged_m', [])[:4], 'fire_wrecked_m': d.get('fire_wrecked_m', [])[:5]}


def model_flags(dest, prefix):
    """(01/11, o Felipe: "tudo que tem bandeira tem que ter física") as bandeiras que o model_run_detail.py tirou da malha (o flags.json:
    o mastro e o tamanho de cada uma, m) no jeito do visual.flags; as da torre vão na torre. O emblema pela facção (1 o círculo da GLA,
    2 a estrela da China; o M_FlagCloth desenha)."""
    js = os.path.join(MODELS, dest.rsplit('/', 1)[-1], 'flags.json')
    if not os.path.exists(js):
        return []
    import json
    emblem = {'gla': 1, 'china': 2, 'russia': 3, 'usa': 4}.get(prefix.split('_')[1].lower(), 0)   # (01/11) 3 e 4: docs/elenco/EMBLEMAS.md
    out = []
    for part, flags in json.load(open(js, encoding='utf-8')).items():
        if not part.startswith(prefix):
            continue
        for x, y, z, length, height in flags:
            f = {'mast_m': [x, y, z], 'size_m': [length, height], 'emblem': emblem}
            if part.endswith('_Turret'):
                f['turret'] = True
            out.append(f)
    return out


def building_visual(dest, prefix, fire_points, spin=None, turret=None, **extra):
    v = {'hull': [mesh(dest, f'{prefix}_Base')], 'fire_points_m': fire_points, 'turret': turret or []}
    if spin:
        v['spin'] = [{'mesh': mesh(dest, f'{prefix}_{p}'), 'at_m': at, 'rpm': rpm} for p, at, rpm in spin]
    v['scaffold'] = mesh(dest, f'{prefix}_Scaffold')
    ds = damage_states(dest, prefix)
    if ds:
        v['damage_states'] = ds
    flags = model_flags(dest, prefix)
    if flags:
        v['flags'] = flags
    v.update(extra)
    return v


def tracked_visual(dest, prefix, pivot, muzzle_h, tracks, turret_parts=('Turret', 'Barrel'), **extra):
    # (01/11, a esteira que anda) as rodas numa malha por roda (wheel_*: o jogo gira cada uma com o lado dela) e o passo do elo
    # (tracks.json do model_run_detail.py); sem o arquivo, a esteira fica parada como antes
    v = {'hull': [mesh(dest, f'{prefix}_Hull'), mesh(dest, f'{prefix}_Wheels', 'wheel_*')] +
                 [mesh(dest, f'{prefix}_{p}') for p in ('Track_l', 'Track_r')],
         'turret': [mesh(dest, f'{prefix}_{p}') for p in turret_parts], 'turret_pivot_m': pivot, 'muzzle_height_m': muzzle_h,
         'tracks_m': tracks}
    js = os.path.join(MODELS, dest.rsplit('/', 1)[-1], 'tracks.json')
    if os.path.exists(js):
        import json
        steps = list(json.load(open(js, encoding='utf-8')).values())
        if steps:
            v['track_step_m'] = steps[0]
    flags = model_flags(dest, prefix)
    if flags:
        v['flags'] = flags
    v.update(extra)
    return v


def wheeled_visual(dest, prefix, pivot=None, muzzle_h=None, turret_parts=('Turret',), tyre_w=0.3, **extra):
    v = {'hull': [mesh(dest, f'{prefix}_Hull'), mesh(dest, f'{prefix}_Wheels', 'wheel_*')],
         'turret': [mesh(dest, f'{prefix}_{p}') for p in turret_parts] if pivot is not None else []}
    if pivot is not None:
        v['turret_pivot_m'] = pivot
        v['muzzle_height_m'] = muzzle_h
    flags = model_flags(dest, prefix)
    if flags:
        v['flags'] = flags
    v['wheels'] = {'tyre_w_m': tyre_w, 'dust_scale': round(tyre_w / 0.274 * 0.65, 3), 'dust_amount': 1.0, 'dust_spacing': 1.5,
                   'dust_trail_s': 0.82, 'mark_dark': 0.35, 'mark_depth': 0.4, 'mark_lug': 1.5, 'mark_segment_m': 2.0}
    v.update(extra)
    return v


def vehicle(key, name, source, hp, speed, cost, build_s, sight=39.5, requires=None, radius=3.6, path_radius=2.0, accel=263.3,
            turn=180, turret_turn=180, crusher=2, **extra):
    u = {'key': key, 'name': name, '_source': source, 'crusher_level': crusher, 'wreck_life_s': 48, 'wreck_sink_s': 7,
         'max_hp': hp, 'armor': 'tank', 'radius_m': radius, 'path_radius_m': path_radius, 'speed_mps': speed,
         'accel_mps2': accel, 'brake_mps2': accel, 'turn_dps': turn, 'turret_turn_dps': turret_turn,
         'turret_idle_return_s': 2.0 if turret_turn else 0, 'sight_m': sight, 'cost': cost, 'build_time_s': build_s}
    if requires:
        u['requires'] = requires
    u.update(extra)
    return u


def gun(range_m, damage, dtype, reload_s, proj=105.3, aim=2.0, **extra):
    w = {'range_m': range_m, 'damage': damage, 'damage_type': dtype, 'reload_s': reload_s, 'aim_tolerance_deg': aim,
         'projectile_mps': proj}
    w.update(extra)
    return w


# o foguete do soldado antitanque (o Javelin, o Caçador, o RPG): o míssil curto com o rastro fino do Stinger
AT_MISSILE = {'launch_mps': 30.0, 'accel_mps2': 160.0, 'turn_dps': 160.0, 'boost_s': 0.1, 'launch_pitch_deg': 6.0,
              'launch_height_m': 1.6, 'spread_deg': 2.0, 'life_s': 2.5}
AT_MISSILE_MODEL = {'mesh': '/Game/Effects/Missiles/SM_Missile_Stinger/StaticMeshes/SM_Missile_Stinger.SM_Missile_Stinger',
                    'length_m': 1.0, 'trail_width': 0.2, 'trail_density': 0.5}
LAUNCHERS = '/Game/Weapons/Launchers'


def soldier_visual(base, faction, rifle, team, cloth):
    """O desenho do soldado: o do Ranger (o corpo do modelo da Unreal, as animações) com a farda e as peças da facção
    (soldier_dress.py + import_soldier.py) e a arma na mão."""
    v = copy.deepcopy(base)
    F = faction.upper()
    s = v['soldier']
    s['rifle'] = rifle
    s['cloth'] = cloth
    s['team'] = team
    s['paint'] = [f'/Game/Characters/Soldier/{F}/T_Soldier_{F}_{i}.T_Soldier_{F}_{i}' for i in (0, 1)]
    for p in s['pieces']:
        b = p['bone']
        n = f'SM_Soldier_{F}_{b}'
        p['mesh'] = f'/Game/Characters/Soldier/{F}/{n}/StaticMeshes/{n}.{n}'
    return v


def infantry(key, name, source, hp, speed, cost, build_s, weapon, visual, requires, sight=26.3, **extra):
    u = {'key': key, 'name': name, '_source': source, 'infantry': True, 'max_hp': hp, 'armor': 'infantry', 'radius_m': 1.0,
         'path_radius_m': 0.4, 'speed_mps': speed, 'accel_mps2': 26.3, 'brake_mps2': 26.3, 'turn_dps': 500,
         'turret_turn_dps': 500, 'turret_idle_return_s': 2.0, 'sight_m': sight, 'requires': requires, 'cost': cost,
         'build_time_s': build_s, 'wreck_life_s': 0, 'wreck_sink_s': 0}
    if weapon:
        u['weapon'] = weapon
    u.update(extra)
    u['visual'] = visual
    return u


def roster(units):
    by = {u['key']: u for u in units}
    ranger_vis = by['usa_ranger']['visual']
    rifle = ranger_vis['soldier']['rifle']
    out = []

    # ---------------- China ----------------
    D = '/Game/Structures/china_command_center'
    out.append(structure('china_command_center', 'Command Center', 'ChinaCommandCenter (FactionBuilding.ini)', 5000, [24.0, 28.0],
                         2000, 45, sight=79.0, produces=['china_dozer'], exit_lane_m=6.6, grants_powers=True,
                         visual=building_visual(D, 'SM_China_CommandCenter', [[-6.0, 4.0, 9.0], [2.0, -5.0, 8.0]],
                                                spin=[('Radar', [-4.8, -9.95, 6.4], 8)])))
    D = '/Game/Structures/china_power_plant'
    out.append(structure('china_power_plant', 'Nuclear Reactor', 'ChinaPowerPlant (FactionBuilding.ini)', 1500, [12.0, 16.0], 1000,
                         10, energy=10,
                         _death_blast='(31/10, simulação 33) o reator que derrete: a ChinaPowerPlantDeathWeapon do ZH (400 explosivo até '
                                      '50 unidades = 13 m, 50 até 100 = 26 m: a borda 12%), em todo mundo em volta, de qualquer dono',
                         death_blast={'range_m': 26.3, 'damage': 400, 'damage_type': 'explosive', 'reload_s': 1.0,
                                      'aim_tolerance_deg': 180, 'projectile_mps': 0, 'splash_m': [13.2, 26.3], 'splash_edge_pct': 12},
                         visual=building_visual(D, 'SM_China_PowerPlant', [[0.0, -4.0, 9.0], [0.0, 4.0, 9.0]])))
    D = '/Game/Structures/china_supply_center'
    out.append(structure('china_supply_center', 'Supply Center', 'ChinaSupplyCenter (FactionBuilding.ini)', 2000, [22.0, 20.0], 1500,
                         10, energy=-1, requires=['china_power_plant'], supply_center=True, free_units=['china_supply_truck'],
                         produces=['china_supply_truck'], exit_lane_m=6.6,
                         visual=building_visual(D, 'SM_China_SupplyCenter', [[-5.0, -4.0, 6.0], [-2.0, 5.0, 5.0]])))
    D = '/Game/Structures/china_barracks'
    out.append(structure('china_barracks', 'Barracks', 'ChinaBarracks (FactionBuilding.ini)', 1000, [16.0, 20.0], 500, 10,
                         produces=['china_rifleman', 'china_tank_hunter'], exit_lane_m=6.6,
                         visual=building_visual(D, 'SM_China_Barracks', [[-4.0, 2.0, 4.5], [-3.0, -5.0, 4.5]])))
    D = '/Game/Structures/china_war_factory'
    out.append(structure('china_war_factory', 'War Factory', 'ChinaWarFactory (FactionBuilding.ini)', 2000, [24.0, 26.0], 2000, 15,
                         energy=-1, requires=['china_supply_center'],
                         produces=['china_type99', 'china_gattling_tank', 'china_inferno_cannon', 'china_overlord'], exit_lane_m=6.6,
                         visual=building_visual(D, 'SM_China_WarFactory', [[-7.0, -8.0, 9.0], [-5.0, 6.0, 8.0]])))
    D = '/Game/Structures/china_propaganda_center'
    out.append(structure('china_propaganda_center', 'Propaganda Center', 'ChinaPropagandaCenter (FactionBuilding.ini)', 1000,
                         [14.0, 24.0], 2000, 45, energy=-2, requires=['china_war_factory'],
                         _tech='(31/10) a tecnologia da China: destrava o Inferno Cannon e o Overlord',
                         visual=building_visual(D, 'SM_China_PropagandaCenter', [[-3.0, -6.0, 7.0], [-2.0, 6.0, 7.0]],
                                                spin=[('Speakers', [-1.0, 7.4, 11.05], 3)])))
    D = '/Game/Structures/china_gattling_cannon'
    out.append(structure('china_gattling_cannon', 'Gattling Cannon', 'ChinaGattlingCannon (FactionBuilding.ini; GattlingBuildingGun, '
                         'GattlingBuildingGunAir)', 1000, [8.0, 8.0], 1200, 25, sight=94.8, energy=-3, requires=['china_power_plant'],
                         counts_for_victory=False, needs_power=True,
                         weapon=gun(59.2, 10, 'gattling', 0.25, proj=0, aim=5),
                         weapon2=gun(105.0, 5, 'gattling', 0.25, proj=0, aim=5, targets=['air']),
                         visual=building_visual(D, 'SM_China_GattlingCannon', [[0.0, 0.0, 3.0]],
                                                turret=[mesh(D, 'SM_China_GattlingCannon_Turret')], turret_yaw_offset_deg=0,
                                                turret_pivot_m=[0.0, 0.0], muzzle_height_m=3.0, light_gun=True,
                                                light_gun_flash_m=0.4, light_gun_tracer_m=3.5, light_gun_spread_m=2.0)))
    out[-1]['turret_turn_dps'] = 180
    out[-1]['turret_idle_return_s'] = 2.0

    D = '/Game/Units/china_dozer'
    out.append(vehicle('china_dozer', 'Dozer', 'ChinaVehicleDozer (ChinaVehicle.ini)', 250, 7.9, 1000, 5, sight=52.7, accel=7.9,
                       turn=90, turret_turn=0, repair_pct_per_s=2.0,
                       constructs=['china_power_plant', 'china_supply_center', 'china_war_factory', 'china_barracks',
                                   'china_gattling_cannon', 'china_propaganda_center', 'china_command_center'],
                       visual=tracked_visual(D, 'SM_China_Dozer', None, None, [2.4, 0.6], turret_parts=(),
                                             blade={'mesh': mesh(D, 'SM_China_Dozer_Blade'), 'at_m': [-0.2, 0.0, 0.45]},
                                             spin=[{'mesh': mesh(D, 'SM_China_Dozer_Beacon'), 'at_m': [-1.28, -0.52, 2.3], 'rpm': 90}],
                                             mass_t=40)))
    out[-1]['visual'].pop('turret_pivot_m')
    out[-1]['visual'].pop('muzzle_height_m')
    D = '/Game/Units/china_supply_truck'
    out.append(vehicle('china_supply_truck', 'Supply Truck', 'ChinaVehicleSupplyTruck (ChinaVehicle.ini)', 300, 10.5, 600, 10,
                       requires=['china_supply_center'], radius=4.0, path_radius=1.9, accel=63.2, turn=90, turret_turn=0, crusher=1,
                       supply_boxes=4,
                       visual=wheeled_visual(D, 'SM_China_SupplyTruck', tyre_w=0.3, mass_t=15,
                                             cargo={'mesh': '/Game/Units/USA_SupplyTruck/SM_USA_SupplyCrate_Small/StaticMeshes/SM_USA_SupplyCrate_Small',
                                                    'slots_m': [[-1.6, -0.5, 1.0], [-1.6, 0.5, 1.0], [-0.5, -0.5, 1.0], [-0.5, 0.5, 1.0]]})))
    D = '/Game/Units/china_type99'
    out.append(vehicle('china_type99', 'Battlemaster', 'ChinaTankBattleMaster (ChinaVehicle.ini; BattleMasterTankGun)', 400, 6.6, 800,
                       10, requires=['china_war_factory'], hull_m=[7.8, 3.6],
                       weapon=gun(39.5, 60, 'armor_piercing', 2.0),
                       visual=tracked_visual(D, 'SM_China_Type99', [0.0, 0.0], 2.1, [2.8, 0.62], impact_crater='A', near_miss=0.2,
                                             mass_t=54)))
    D = '/Game/Units/china_gattling_tank'
    out.append(vehicle('china_gattling_tank', 'Gattling Tank', 'ChinaTankGattling (ChinaVehicle.ini; GattlingTankGun, '
                       'GattlingTankGunAir)', 300, 10.5, 800, 10, sight=52.7, requires=['china_war_factory'], hull_m=[7.0, 3.4],
                       weapon=gun(39.5, 15, 'gattling', 0.4, proj=0, aim=5),
                       weapon2=gun(92.2, 12, 'gattling', 0.4, proj=0, aim=5, targets=['air']),
                       visual=tracked_visual(D, 'SM_China_GattlingTank', [0.0, 0.0], 2.3, [2.6, 0.58], turret_parts=('Turret',),
                                             spin=[{'mesh': mesh(D, 'SM_China_GattlingTank_Radar'), 'at_m': [-1.55, 0.0, 2.94],
                                                    'rpm': 30, 'on_turret': True}],
                                             light_gun=True, light_gun_flash_m=0.4, light_gun_tracer_m=3.5, light_gun_spread_m=2.0,
                                             mass_t=35)))
    D = '/Game/Units/china_inferno_cannon'
    out.append(vehicle('china_inferno_cannon', 'Inferno Cannon', 'ChinaVehicleInfernoCannon (ChinaVehicle.ini; InfernoCannonGun)', 120,
                       7.9, 900, 15, requires=['china_war_factory', 'china_propaganda_center'], hull_m=[7.5, 3.4], turret_turn=90,
                       _artillery='(31/10) a artilharia da China: o obuseiro de cano longo (PLZ-05) com a granada incendiária; o fogo faz '
                                  '150% no soldado e 25% no tanque (Armor.ini)',
                       weapon=gun(79.0, 30, 'flame', 4.0, proj=45.0, aim=4, min_range_m=13.2, splash_m=[2.6, 5.3], splash_edge_pct=40),
                       visual=tracked_visual(D, 'SM_China_InfernoCannon', [-1.0, 0.0], 2.6, [2.6, 0.58], impact_crater='A',
                                             mass_t=33)))
    D = '/Game/Units/china_overlord'
    out.append(vehicle('china_overlord', 'Overlord', 'ChinaTankOverlord (ChinaVehicle.ini; OverlordTankGun)', 1100, 5.3, 2000, 20,
                       requires=['china_war_factory', 'china_propaganda_center'], radius=4.6, path_radius=2.6, turn=90, turret_turn=90,
                       crusher=3, hull_m=[10.5, 5.0],
                       _crusher='(31/10) o esmagador nível 3: passa por cima de carro, poste e árvore como o tanque, e da carcaça leve',
                       weapon=gun(46.0, 80, 'armor_piercing', 0.3, clip_size=2, clip_reload_s=2.0),
                       visual=tracked_visual(D, 'SM_China_Overlord', [0.0, 0.0], 2.6, [3.6, 0.8], impact_crater='A', near_miss=0.2,
                                             mass_t=90)))
    china_soldier = lambda r: soldier_visual(ranger_vis, 'china', r, team=[0.32, 0.03, 0.02], cloth=[0.12, 0.13, 0.08])
    out.append(infantry('china_rifleman', 'Red Guard', 'ChinaInfantryRedguard (ChinaInfantry.ini; RedguardMachineGun)', 120, 6.6, 300,
                        10, gun(26.3, 15, 'small_arms', 1.0, proj=0, aim=5), china_soldier(rifle), ['china_barracks'],
                        produce_count=2, _pair='(31/10) sai em par: o QuantityModifier do ZH (o preço é do par)'))
    out[-1]['visual'].update({'muzzle_forward_m': 1.05, 'light_gun': True, 'light_gun_flash_m': 0.15, 'light_gun_tracer_m': 2.6,
                              'light_gun_spread_m': 1.5})
    out.append(infantry('china_tank_hunter', 'Tank Hunter', 'ChinaInfantryTankHunter (ChinaInfantry.ini; TankHunterMissileWeapon)', 100,
                        5.3, 300, 5, gun(46.0, 40, 'armor_piercing', 1.0, proj=70.0, aim=4, targets=['ground', 'air'],
                                         missile=dict(AT_MISSILE)),
                        china_soldier(mesh(LAUNCHERS, 'SM_Launcher_PF98')), ['china_barracks']))
    out[-1]['visual'].update({'missile_model': dict(AT_MISSILE_MODEL), 'muzzle_forward_m': 0.9, 'warhead_kg': 1.0})

    # ---------------- GLA ----------------
    D = '/Game/Structures/gla_command_center'
    out.append(structure('gla_command_center', 'Command Center', 'GLACommandCenter (FactionBuilding.ini)', 5000, [28.0, 28.0], 2000, 45,
                         sight=79.0, produces=['gla_worker'], exit_lane_m=6.6, grants_powers=True,
                         visual=building_visual(D, 'SM_GLA_CommandCenter', [[-6.0, 5.0, 7.0], [3.0, -6.0, 6.0]],
                                                spin=[('Dish', [-8.92, -10.38, 13.25], 2)])))
    D = '/Game/Structures/gla_supply_stash'
    out.append(structure('gla_supply_stash', 'Supply Stash', 'GLASupplyStash (FactionBuilding.ini)', 2000, [16.0, 16.0], 1500, 10,
                         supply_center=True, free_units=['gla_supply_pickup'], produces=['gla_supply_pickup'], exit_lane_m=6.6,
                         visual=building_visual(D, 'SM_GLA_SupplyStash', [[-3.0, -3.0, 4.0], [2.0, 3.0, 3.5]])))
    D = '/Game/Structures/gla_barracks'
    out.append(structure('gla_barracks', 'Barracks', 'GLABarracks (FactionBuilding.ini)', 1000, [18.0, 22.0], 500, 10,
                         produces=['gla_rebel', 'gla_rpg'], exit_lane_m=6.6,
                         visual=building_visual(D, 'SM_GLA_Barracks', [[-4.0, -5.0, 6.0], [-2.0, 4.0, 4.0]])))
    D = '/Game/Structures/gla_arms_dealer'
    out.append(structure('gla_arms_dealer', 'Arms Dealer', 'GLAArmsDealer (FactionBuilding.ini)', 2000, [19.0, 26.0], 2500, 15,
                         requires=['gla_supply_stash'],
                         produces=['gla_tank', 'gla_technical', 'gla_quad_cannon', 'gla_toxin_tractor', 'gla_rocket_buggy'],
                         exit_lane_m=6.6,
                         visual=building_visual(D, 'SM_GLA_ArmsDealer', [[-6.0, -7.0, 7.0], [-4.0, 6.0, 6.0]],
                                                # (01/11) o túnel: o chão recortado sobre a rampa e o túnel (do meio dos muros de
                                                # arrimo; Tools/model_gla_arms_dealer.py na escala 0,79) e o veículo que sobe dele
                                                ground_cut_m=[[-8.93, -4.54, 3.16, 4.54]],
                                                tunnel_exit={'path_m': [[-7.74, -3.0], [-6.32, -3.0], [3.16, 0.0]], 'speed_mps': 4.0})))
    D = '/Game/Structures/gla_palace'
    out.append(structure('gla_palace', 'Palace', 'GLAPalace (FactionBuilding.ini)', 3000, [18.0, 24.0], 2500, 45,
                         requires=['gla_arms_dealer'], _tech='(31/10) a tecnologia da GLA: destrava o Rocket Buggy',
                         visual=building_visual(D, 'SM_GLA_Palace', [[-2.0, 0.0, 10.0], [-5.0, -8.0, 6.0]])))
    D = '/Game/Structures/gla_stinger_site'
    out.append(structure('gla_stinger_site', 'Stinger Site', 'GLAStingerSite (FactionBuilding.ini; StingerMissileWeapon, '
                         'StingerMissileWeaponAir)', 1000, [8.0, 8.0], 900, 15, sight=94.8, requires=['gla_barracks'],
                         counts_for_victory=False,
                         weapon=gun(59.2, 20, 'explosive', 2.0, proj=110.0, aim=4,
                                    missile={'launch_mps': 20.0, 'accel_mps2': 120.0, 'turn_dps': 200.0, 'boost_s': 0.15,
                                             'launch_pitch_deg': 20.0, 'launch_height_m': 1.6, 'spread_deg': 6.0, 'life_s': 3.5}),
                         weapon2=gun(105.0, 30, 'explosive', 2.0, proj=110.0, aim=4, targets=['air'], splash_m=[1.3, 1.3],
                                     missile={'launch_mps': 20.0, 'accel_mps2': 140.0, 'turn_dps': 240.0, 'boost_s': 0.15,
                                              'launch_pitch_deg': 50.0, 'launch_height_m': 1.6, 'spread_deg': 6.0, 'life_s': 4.0}),
                         visual=building_visual(D, 'SM_GLA_StingerSite', [[0.0, 0.0, 2.0]],
                                                turret=[mesh(D, 'SM_GLA_StingerSite_Turret')], turret_yaw_offset_deg=0,
                                                turret_pivot_m=[0.0, 0.0], muzzle_height_m=2.0,
                                                missile_model=dict(AT_MISSILE_MODEL, length_m=1.52, trail_width=0.25,
                                                                   trail_density=0.6))))
    out[-1]['turret_turn_dps'] = 180
    out[-1]['turret_idle_return_s'] = 2.0

    D = '/Game/Units/gla_supply_pickup'
    out.append(vehicle('gla_supply_pickup', 'Supply Pickup', '(31/10) nova: a coleta da GLA (no ZH o Worker coleta); mais barata e '
                       'frágil que o caminhão, 3 caixas', 200, 11.0, 400, 6, requires=['gla_supply_stash'], radius=3.0,
                       path_radius=1.5, accel=63.2, turn=120, turret_turn=0, crusher=1, supply_boxes=3,
                       visual=wheeled_visual(D, 'SM_GLA_SupplyPickup', tyre_w=0.28, mass_t=2.5, death_kind='gasoline',
                                             death_scale=0.4,
                                             cargo={'mesh': '/Game/Units/USA_SupplyTruck/SM_USA_SupplyCrate_Small/StaticMeshes/SM_USA_SupplyCrate_Small',
                                                    'slots_m': [[-1.4, -0.45, 0.9], [-1.4, 0.45, 0.9], [-0.5, -0.45, 0.9]]})))
    D = '/Game/Units/gla_quad_cannon'
    out.append(vehicle('gla_quad_cannon', 'Quad Cannon', 'GLAVehicleQuadCannon (GLAVehicle.ini; QuadCannonGun, QuadCannonGunAir)', 300,
                       10.5, 700, 6, sight=52.7, requires=['gla_arms_dealer'], radius=3.2, path_radius=1.6, accel=63.2,
                       turret_turn=240, crusher=1,
                       _damage='(31/10) o tiro rotativo do ZH (gattling: 10% no tanque); com o de fuzil ela fazia 25/s no tanque, 6,7× o Gattling Tank',
                       weapon=gun(39.5, 10, 'gattling', 0.1, proj=0, aim=5),
                       weapon2=gun(92.2, 5, 'gattling', 0.1, proj=0, aim=5, targets=['air']),
                       visual=wheeled_visual(D, 'SM_GLA_QuadCannon', pivot=[-1.5, 0.0], muzzle_h=2.6, tyre_w=0.32, mass_t=6,
                                             light_gun=True, light_gun_flash_m=0.4, light_gun_tracer_m=3.9, light_gun_spread_m=3.0,
                                             death_kind='gasoline', death_scale=0.5)))
    D = '/Game/Units/gla_toxin_tractor'
    out.append(vehicle('gla_toxin_tractor', 'Toxin Tractor', 'GLAVehicleToxinTruck (GLAVehicle.ini; ToxinTruckGun): no ZH é o '
                       'caminhão; aqui o trator agrícola', 240, 7.9, 600, 5, requires=['gla_arms_dealer'], radius=3.0, path_radius=1.6,
                       accel=26.3, turn=120, turret_turn=180, crusher=1,
                       weapon=gun(26.3, 10, 'poison', 0.25, proj=0, aim=8, splash_m=[2.6, 2.6]),
                       visual=wheeled_visual(D, 'SM_GLA_ToxinTractor', pivot=[1.5, 0.0], muzzle_h=1.6, tyre_w=0.45, mass_t=5,
                                             death_kind='gasoline', death_scale=0.5)))
    D = '/Game/Units/gla_rocket_buggy'
    out.append(vehicle('gla_rocket_buggy', 'Rocket Buggy', 'GLAVehicleRocketBuggy (GLAVehicle.ini; RocketBuggyRocketWeapon)', 120, 23.7,
                       900, 10, requires=['gla_arms_dealer', 'gla_palace'], radius=2.6, path_radius=1.3, accel=26.3, turret_turn=180,
                       crusher=0,
                       weapon=gun(79.0, 20, 'explosive', 0.25, proj=60.0, aim=6, min_range_m=13.2, splash_m=[1.3, 2.6], clip_size=6,
                                  clip_reload_s=6.0,
                                  missile={'launch_mps': 25.0, 'accel_mps2': 90.0, 'turn_dps': 60.0, 'boost_s': 0.2,
                                           'launch_pitch_deg': 40.0, 'launch_height_m': 2.0, 'spread_deg': 8.0, 'life_s': 4.5}),
                       visual=wheeled_visual(D, 'SM_GLA_RocketBuggy', pivot=[-1.0, 0.0], muzzle_h=2.0, tyre_w=0.4, mass_t=1.5,
                                             missile_model=dict(AT_MISSILE_MODEL), death_kind='gasoline', death_scale=0.35)))
    gla_soldier = lambda r: soldier_visual(ranger_vis, 'gla', r, team=[0.10, 0.18, 0.05], cloth=[0.21, 0.19, 0.14])
    out.append(infantry('gla_worker', 'Worker', 'GLAInfantryWorker (GLAInfantry.ini): constrói e desmancha carcaça em dinheiro', 100,
                        7.9, 200, 3, None, gla_soldier(''), [], repair_pct_per_s=2.0,
                        constructs=['gla_supply_stash', 'gla_barracks', 'gla_arms_dealer', 'gla_palace', 'gla_stinger_site',
                                    'gla_command_center']))
    out[-1].pop('requires')
    out.append(infantry('gla_rebel', 'Rebel', 'GLAInfantryRebel (GLAInfantry.ini; GLAInfantryRifle)', 120, 5.3, 150, 5,
                        gun(26.3, 5, 'small_arms', 0.1, proj=0, aim=5, clip_size=3, clip_reload_s=0.7), gla_soldier(rifle),
                        ['gla_barracks'], sight=39.5))
    out[-1]['visual'].update({'muzzle_forward_m': 1.05, 'light_gun': True, 'light_gun_flash_m': 0.15, 'light_gun_tracer_m': 2.6,
                              'light_gun_spread_m': 1.5})
    out.append(infantry('gla_rpg', 'RPG Trooper', 'GLAInfantryTunnelDefender (GLAInfantry.ini; TunnelDefenderRocketWeapon)', 100, 5.3,
                        300, 5, gun(46.0, 40, 'armor_piercing', 1.0, proj=70.0, aim=4, targets=['ground', 'air'],
                                    missile=dict(AT_MISSILE)),
                        gla_soldier(mesh(LAUNCHERS, 'SM_Launcher_RPG7')), ['gla_barracks']))
    out[-1]['visual'].update({'missile_model': dict(AT_MISSILE_MODEL), 'muzzle_forward_m': 0.9, 'warhead_kg': 1.0})

    # ---------------- USA (o que entra) ----------------
    D = '/Game/Structures/usa_strategy_center'
    out.append(structure('usa_strategy_center', 'Strategy Center', 'AmericaStrategyCenter (FactionBuilding.ini)', 1500, [20.0, 20.0],
                         2500, 60, sight=105.0, energy=-2, requires=['usa_war_factory'],
                         _tech='(31/10) a tecnologia da USA: destrava o Paladin e o Javelin',
                         visual=building_visual(D, 'SM_USA_StrategyCenter', [[-3.0, -4.0, 7.0], [2.0, 5.0, 6.0]],
                                                spin=[('Dish', [0.0, 0.0, 5.8], 3)])))   # (01/11) o eixo do prato é DISH_AT do modelo (5,8; o 9,0 deixava o prato 3,2 m no ar)
    D = '/Game/Units/usa_paladin'
    out.append(vehicle('usa_paladin', 'Paladin', 'AmericaTankPaladin (AmericaVehicle.ini; PaladinTankGun)', 650, 6.6, 1100, 12,
                       requires=['usa_war_factory', 'usa_strategy_center'], hull_m=[8.6, 3.9], ricochet=True,
                       _balance='(31/10) 650 de vida: com 500 e o mesmo canhão ele saía ~30% pior por dólar que o Battle Tank (Lanchester, ELENCO §8.1); o laser de defesa de ponto do ZH fica como ideia',
                       weapon=gun(39.5, 60, 'armor_piercing', 2.0),
                       visual=tracked_visual(D, 'SM_USA_Paladin', [0.0, 0.0], 2.2, [2.9, 0.66], impact_crater='A', near_miss=0.2,
                                             spin=[{'mesh': mesh(D, 'SM_USA_Paladin_Laser'), 'at_m': [-1.02, 0.0, 2.86], 'rpm': 20,
                                                    'on_turret': True}],
                                             mass_t=66)))
    usa_soldier = copy.deepcopy(ranger_vis)
    usa_soldier['soldier']['rifle'] = mesh(LAUNCHERS, 'SM_Launcher_Javelin')
    for k in ('light_gun', 'light_gun_flash_m', 'light_gun_tracer_m', 'light_gun_spread_m'):
        usa_soldier.pop(k, None)
    usa_soldier.update({'missile_model': dict(AT_MISSILE_MODEL), 'muzzle_forward_m': 0.9, 'warhead_kg': 1.0})
    out.append(infantry('usa_javelin', 'Javelin', 'AmericaInfantryMissileDefender (AmericaInfantry.ini; MissileDefenderMissileWeapon)', 100,
                        5.3, 300, 5, gun(46.0, 40, 'armor_piercing', 1.0, proj=70.0, aim=4, targets=['ground', 'air'],
                                         missile=dict(AT_MISSILE)),
                        usa_soldier, ['usa_barracks', 'usa_strategy_center']))
    out += russia(ranger_vis, rifle)
    # o desenho do fuzil do Ranger fica no soldado do fuzil (Rebel, Red Guard); no antitanque, o tiro é o míssil
    for u in out:
        if u.get('infantry') and 'missile' in u.get('weapon', {}):
            for k in ('light_gun', 'light_gun_flash_m', 'light_gun_tracer_m', 'light_gun_spread_m'):
                u['visual'].pop(k, None)
    return out


# (01/11, a Rússia provisória: docs/FACCAO_RUSSIA.md; o Felipe: "você já pode montar provisórios da Rússia no jogo também, a facção, com
# mesmo nível de balanceamento dos outros") os pivôs e as medidas de cada modelo vêm do relatório dos agentes que modelam
# (RUSSIA_GEOM; o que falta fica no padrão do tanque da China). A facção só entra quando os modelos existem (a pasta de cada um)
RUSSIA_GEOM = {
    # (01/11, os relatórios dos agentes: as medidas dos modelos, em metros da Unreal; x pra frente, y pra direita)
    't90m': {'pivot': [0.35, 0.0], 'muzzle_h': 1.80, 'tracks': [2.84, 0.58], 'hull': [7.20, 3.86]},
    't14': {'pivot': [-0.35, 0.0], 'muzzle_h': 2.08, 'tracks': [2.80, 0.64], 'hull': [9.04, 3.80]},
    'bmpt': {'pivot': [0.35, 0.0], 'muzzle_h': 2.30, 'tracks': [2.84, 0.58], 'hull': [7.20, 3.86]},
    # o bloco de 24 tubos (6 × 4) na dobradiça, deitado; a salva levanta a 40°
    'tos1a': {'pivot': [0.35, 0.0], 'muzzle_h': 2.34, 'tracks': [2.84, 0.58], 'hull': [7.20, 3.86],
              'launcher': {'mesh': 'SM_Russia_TOS1A_Launcher', 'hinge_m': [-1.07, 0.0, 1.68], 'fire_pitch_deg': 40, 'raise_dps': 40,
                           'accel_dps2': 200, 'brake_dps2': 400, 'lower_dps': 25, 'lower_delay_s': 3.0, 'stop_hz': 2.5, 'stop_zeta': 0.5,
                           'tubes_m': [[3.68, y, z] for z in (1.905, 2.195, 2.485, 2.775)
                                       for y in (-0.725, -0.435, -0.145, 0.145, 0.435, 0.725)],
                           # (01/11) os cilindros de elevação em 3 estágios (model_russia_tos1a.py rams()): o jogo aponta as peças do pino da
                           # mesa (base_m) pro olhal do bloco (tip_m, deitado), e o estágio do meio desliza metade da extensão
                           'ram': {'cyl': 'SM_Russia_TOS1A_RamCyl', 'mid': 'SM_Russia_TOS1A_RamMid', 'rod': 'SM_Russia_TOS1A_RamRod',
                                   'base_m': [1.20, 0.0, 1.70], 'tip_m': [2.10, 0.0, 1.66]}}},
    'pantsir': {'pivot': [-1.62, 0.0], 'muzzle_h': 2.28, 'tyre_w': 0.38, 'radar_at': [-1.87, 0.0, 3.42], 'hull': [7.90, 2.42]},
    # o trator já na escala do jogo (model_russia_dozer.py GAME_SCALE 0,78: o apoio menor que o tanque)
    'dozer': {'tracks': [2.215, 0.452], 'blade_at': [2.512, 0.0, 0.484], 'beacon_at': [0.0, -0.897, 2.465]},
    'supply_truck': {'tyre_w': 0.36, 'slots': [[0.16, -0.5, 1.22], [0.16, 0.5, 1.22], [-1.0, -0.5, 1.22], [-1.0, 0.5, 1.22],
                                               [-2.16, 0.0, 1.22]]},
}


def russia(ranger_vis, rifle):
    """O elenco mínimo da Rússia (o mesmo tamanho da China: 7 prédios, 7 veículos, 2 de infantaria), nas contas do ELENCO §8.1."""
    have = lambda k: os.path.isdir(os.path.join(MODELS, 'russia_' + k))
    if not have('t90m'):
        return []
    g = RUSSIA_GEOM
    out = []
    S = '/Game/Structures/russia_'
    U = '/Game/Units/russia_'
    out.append(structure('russia_command_center', 'Command Center', '(Rússia) o Shtab, o quartel-general de concreto de painel', 5000,
                         [26.0, 28.0], 2000, 45, sight=79.0, produces=['russia_dozer'], exit_lane_m=6.6, grants_powers=True,
                         visual=building_visual(S + 'command_center', 'SM_Russia_CommandCenter',
                                                [[-6.0, -10.0, 6.1], [-5.5, 5.0, 9.3], [-6.0, -1.5, 12.5]],
                                                spin=[('Radar', [-9.2, 10.0, 15.52], 6)])))
    out.append(structure('russia_power_plant', 'Gas Power Plant', '(Rússia) a usina a gás: +7, e a bola de fogo do gás quando cai', 1500,
                         [14.0, 16.0], 900, 10, energy=7,
                         _death_blast='(01/11) a usina a gás que cai: a bola de fogo do gás (menor que o reator da China: 250 até 9 m, a borda '
                                      'até 18 m)',
                         death_blast={'range_m': 18.0, 'damage': 250, 'damage_type': 'explosive', 'reload_s': 1.0,
                                      'aim_tolerance_deg': 180, 'projectile_mps': 0, 'splash_m': [9.0, 18.0], 'splash_edge_pct': 15},
                         visual=building_visual(S + 'power_plant', 'SM_Russia_PowerPlant', [[-4.0, -5.2, 6.3], [-0.6, -0.2, 6.3], [3.6, 5.8, 3.6]])))
    out.append(structure('russia_supply_center', 'Supply Depot', '(Rússia) o depósito de carga com a ponte rolante', 2000, [22.0, 20.0],
                         1500, 10, energy=-1, requires=['russia_power_plant'], supply_center=True, free_units=['russia_supply_truck'],
                         produces=['russia_supply_truck'], exit_lane_m=6.6,
                         visual=building_visual(S + 'supply_center', 'SM_Russia_SupplyCenter', [[-6.0, 6.3, 6.4], [-1.0, 4.6, 5.65], [6.0, 8.4, 3.5]])))
    # a ponte rolante amarela fica parada em cima do caminhão (o jogo ainda não corre ponte; afunda junto na ruína)
    out[-1]['visual']['hull'].append(mesh(S + 'supply_center', 'SM_Russia_SupplyCenter_Crane'))
    out.append(structure('russia_barracks', 'Barracks', '(Rússia) a Kazarma, o alojamento de painel com o pátio de formatura', 1000,
                         [16.0, 20.0], 500, 10, produces=['russia_motostrelok', 'russia_kornet'], exit_lane_m=6.6,
                         visual=building_visual(S + 'barracks', 'SM_Russia_Barracks', [[-4.5, -5.5, 7.5], [-4.5, 5.0, 7.5], [-1.0, 0.0, 7.45]])))
    out.append(structure('russia_war_factory', 'Tank Plant', '(Rússia) a fábrica de tanques de telhado-serra', 2000, [26.0, 28.0], 2000,
                         15, energy=-1, requires=['russia_supply_center'],
                         produces=['russia_t90m', 'russia_bmpt', 'russia_pantsir', 'russia_tos1a', 'russia_t14'], exit_lane_m=6.6,
                         visual=building_visual(S + 'war_factory', 'SM_Russia_WarFactory', [[-8.0, -3.5, 8.3], [-1.2, 3.8, 7.75], [-5.0, 10.2, 6.1]])))
    out.append(structure('russia_institute', 'Research Institute', '(Rússia) o NII: o radar e a guerra eletrônica; a tecnologia', 1000,
                         [16.0, 22.0], 2000, 45, energy=-2, requires=['russia_war_factory'],
                         _tech='(01/11) a tecnologia da Rússia: destrava o TOS-1A e o T-14 Armata',
                         visual=building_visual(S + 'institute', 'SM_Russia_Institute', [[-4.6, -3.0, 10.1], [-1.2, 0.6, 10.1], [0.4, 6.9, 6.9]])))
    out.append(structure('russia_pantsir_site', 'Pantsir Site', '(Rússia) a defesa: o Pantsir fixo (o míssil contra o ar, o canhão de '
                         '30 mm contra a infantaria)', 1000, [8.0, 8.0], 1200, 25, sight=94.8, energy=-3, requires=['russia_power_plant'],
                         counts_for_victory=False, needs_power=True,
                         _balance='(01/11) o par do Gattling Cannon da China ($1 200): menos contra a infantaria (24/s contra 40/s), mais '
                                  'contra o ar (o míssil a 110 m, 30/s)',
                         weapon=gun(55.0, 6, 'gattling', 0.25, proj=0, aim=5),
                         weapon2=gun(110.0, 45, 'explosive', 1.5, proj=160.0, aim=8, targets=['air']),
                         visual=building_visual(S + 'pantsir_site', 'SM_Russia_PantsirSite', [[-1.6, -1.8, 0.92], [1.5, 2.2, 0.92], [-0.3, 0.0, 3.12]],
                                                turret=[mesh(S + 'pantsir_site', 'SM_Russia_PantsirSite_Turret')], turret_yaw_offset_deg=0,
                                                spin=[('Radar', [0.0, 0.0, 3.42], 30)],
                                                turret_pivot_m=[0.0, 0.0], muzzle_height_m=2.3, light_gun=True,
                                                light_gun_flash_m=0.4, light_gun_tracer_m=3.5, light_gun_spread_m=2.0)))
    out[-1]['turret_turn_dps'] = 180
    out[-1]['turret_idle_return_s'] = 2.0

    D = U + 'dozer'
    out.append(vehicle('russia_dozer', 'IMR-3M', '(Rússia) o trator de engenharia sobre o chassi de tanque', 300, 7.2, 1000, 5,
                       sight=52.7, accel=7.9, turn=90, turret_turn=0, repair_pct_per_s=2.0,
                       constructs=['russia_power_plant', 'russia_supply_center', 'russia_war_factory', 'russia_barracks',
                                   'russia_pantsir_site', 'russia_institute', 'russia_command_center'],
                       visual=tracked_visual(D, 'SM_Russia_Dozer', None, None, g['dozer']['tracks'], turret_parts=(),
                                             blade={'mesh': mesh(D, 'SM_Russia_Dozer_Blade'), 'at_m': g['dozer']['blade_at']},
                                             spin=[{'mesh': mesh(D, 'SM_Russia_Dozer_Beacon'), 'at_m': g['dozer']['beacon_at'], 'rpm': 90}],
                                             mass_t=46)))
    out[-1]['visual'].pop('turret_pivot_m')
    out[-1]['visual'].pop('muzzle_height_m')
    D = U + 'supply_truck'
    out.append(vehicle('russia_supply_truck', 'KamAZ', '(Rússia) o KamAZ 6×6: carrega 5 caixas e anda mais devagar', 350, 8.9, 700, 10,
                       requires=['russia_supply_center'], radius=4.2, path_radius=2.0, accel=63.2, turn=90, turret_turn=0, crusher=1,
                       supply_boxes=5,
                       _balance='(01/11) 5 caixas a 8,9 m/s (0,85 da China) = +6% de carga por minuto que o caminhão da China, por +17% do preço',
                       visual=wheeled_visual(D, 'SM_Russia_SupplyTruck', tyre_w=g['supply_truck']['tyre_w'], mass_t=18,
                                             cargo={'mesh': '/Game/Units/USA_SupplyTruck/SM_USA_SupplyCrate_Small/StaticMeshes/SM_USA_SupplyCrate_Small',
                                                    'slots_m': g['supply_truck']['slots']})))
    D = U + 't90m'
    out.append(vehicle('russia_t90m', 'T-90M', '(Rússia) o tanque básico, com a gaiola e o Relikt', 560, 6.2, 950, 11,
                       requires=['russia_war_factory'], hull_m=g['t90m']['hull'],
                       _balance='(01/11) a força de Lanchester 560 × 30 ÷ 950² = 0,0186 (USA 0,0178, China 0,0188, GLA 0,0206): o mais '
                                'lento dos básicos e o de mais vida; o alcance dos básicos (39,5 m: a vantagem do longe é do T-14 e do Kornet)',
                       weapon=gun(39.5, 60, 'armor_piercing', 2.0),
                       visual=tracked_visual(D, 'SM_Russia_T90M', g['t90m']['pivot'], g['t90m']['muzzle_h'], g['t90m']['tracks'],
                                             impact_crater='A', near_miss=0.2, mass_t=48)))
    D = U + 'bmpt'
    out.append(vehicle('russia_bmpt', 'BMPT Terminator', '(Rússia) contra a infantaria: os 2 canhões de 30 mm e o míssil', 380, 7.9, 900,
                       10, sight=52.7, requires=['russia_war_factory'], hull_m=g['bmpt']['hull'],
                       _balance='(01/11) o par do Gattling Tank da China contra a infantaria (40/s, força 0,0188 contra 0,0176), sem o tiro no ar (o ar '
                                'da Rússia é do Pantsir), e com o míssil Ataka contra o tanque (12,5/s)',
                       weapon=gun(39.5, 12, 'gattling', 0.3, proj=0, aim=5),
                       weapon2=gun(50.0, 50, 'armor_piercing', 4.0, proj=70.0, aim=4, missile=dict(AT_MISSILE)),
                       visual=tracked_visual(D, 'SM_Russia_BMPT', g['bmpt']['pivot'], g['bmpt']['muzzle_h'], g['bmpt']['tracks'],
                                             turret_parts=('Turret', 'Barrel'), light_gun=True, light_gun_flash_m=0.4, light_gun_tracer_m=3.5,
                                             light_gun_spread_m=2.0, mass_t=44, missile_model=dict(AT_MISSILE_MODEL))))
    D = U + 'pantsir'
    out.append(vehicle('russia_pantsir', 'Pantsir-S1', '(Rússia) o antiaéreo: o radar, o míssil a 110 m e o canhão de 30 mm', 320, 9.2,
                       1000, 10, sight=79.0, requires=['russia_war_factory'], radius=4.0, path_radius=2.0, accel=63.2, crusher=1,
                       hull_m=g['pantsir']['hull'],
                       _balance='(01/11) o par do Avenger ($1 000): o míssil mais longe (110 m) e mais lento de recarregar',
                       weapons=[{'slot': 'cannon', 'targets': ['ground', 'air'], 'range_m': 50.0, 'damage': 6, 'damage_type': 'gattling',
                                 'reload_s': 0.15, 'aim_tolerance_deg': 5, 'clip_size': 10, 'clip_reload_s': 1.5, 'projectile_mps': 0},
                                {'slot': 'missiles', 'targets': ['air'], 'range_m': 110.0, 'damage': 45, 'damage_type': 'explosive',
                                 'splash_m': [1.3, 2.6], 'reload_s': 0.6, 'aim_tolerance_deg': 8, 'clip_size': 4, 'clip_reload_s': 12,
                                 'projectile_mps': 180, 'missile': {'launch_mps': 20.0, 'accel_mps2': 260, 'turn_dps': 300, 'boost_s': 0.3,
                                                                    'launch_pitch_deg': 60, 'launch_height_m': 3.4, 'spread_deg': 5,
                                                                    'life_s': 4.0}}],
                       visual=wheeled_visual(D, 'SM_Russia_Pantsir', pivot=g['pantsir']['pivot'], muzzle_h=g['pantsir']['muzzle_h'],
                                             tyre_w=g['pantsir']['tyre_w'], mass_t=30, missile_model=dict(AT_MISSILE_MODEL),
                                             spin=[{'mesh': mesh(D, 'SM_Russia_Pantsir_Radar'), 'at_m': g['pantsir']['radar_at'],
                                                    'rpm': 40, 'on_turret': True}],
                                             light_gun=True, light_gun_flash_m=0.4, light_gun_tracer_m=3.5, light_gun_spread_m=2.0)))
    D = U + 'tos1a'
    out.append(vehicle('russia_tos1a', 'TOS-1A', '(Rússia) a artilharia termobárica: a salva de 6 foguetes', 220, 6.2, 1200, 15,
                       requires=['russia_war_factory', 'russia_institute'], hull_m=g['tos1a']['hull'], turret_turn=60,
                       _artillery='(01/11) o termobárico: o fogo (flame) faz 150% no soldado e 25% no tanque; a salva de 6 em 1,25 s, 10 s pra '
                                  'recarregar (≈ 19/s em área, contra 7,5/s do Inferno Cannon, que alcança mais e custa menos)',
                       weapon=gun(70.0, 35, 'flame', 0.25, proj=50.0, aim=4, min_range_m=15.0, splash_m=[4.0, 8.0], splash_edge_pct=40,
                                  clip_size=6, clip_reload_s=10.0),
                       visual=tracked_visual(D, 'SM_Russia_TOS1A', g['tos1a']['pivot'], g['tos1a']['muzzle_h'], g['tos1a']['tracks'],
                                             turret_parts=('Turret', 'Launcher', 'RamCyl', 'RamMid', 'RamRod'), launcher=dict(g['tos1a']['launcher']),
                                             impact_crater='A',
                                             mass_t=44)))
    D = U + 't14'
    out.append(vehicle('russia_t14', 'T-14 Armata', '(Rússia) o pesado: a torre sem tripulação e o 125 mm novo', 1050, 7.5, 1800, 18,
                       requires=['russia_war_factory', 'russia_institute'], radius=4.2, path_radius=2.3, turn=110, turret_turn=110,
                       hull_m=g['t14']['hull'],
                       _balance='(01/11) a força 1 050 × 50 ÷ 1 800² = 0,0162, a da Paladin (0,0161; o Overlord 0,0191 é o superpesado): o pesado '
                                'rápido (7,5 m/s), alcance 52 m',
                       weapon=gun(52.0, 100, 'armor_piercing', 2.0),
                       visual=tracked_visual(D, 'SM_Russia_T14', g['t14']['pivot'], g['t14']['muzzle_h'], g['t14']['tracks'],
                                             impact_crater='A', near_miss=0.2, mass_t=55)))
    # a infantaria: o soldado provisório (as peças da China na farda cinza-esverdeada russa; a farda da Rússia é da sessão da infantaria)
    ru_soldier = lambda r: soldier_visual(ranger_vis, 'china', r, team=[0.30, 0.03, 0.03], cloth=[0.10, 0.11, 0.095])
    out.append(infantry('russia_motostrelok', 'Motostrelok', '(Rússia) o fuzileiro motorizado (AK-12)', 140, 6.6, 200, 8,
                        gun(26.3, 16, 'small_arms', 1.0, proj=0, aim=5), ru_soldier(rifle), ['russia_barracks']))
    out[-1]['visual'].update({'muzzle_forward_m': 1.05, 'light_gun': True, 'light_gun_flash_m': 0.15, 'light_gun_tracer_m': 2.6,
                              'light_gun_spread_m': 1.5})
    out.append(infantry('russia_kornet', 'Kornet Team', '(Rússia) o antitanque de tripé: o tiro mais longe da infantaria', 100, 5.0, 300, 6,
                        gun(55.0, 70, 'armor_piercing', 2.0, proj=70.0, aim=4, missile=dict(AT_MISSILE)),
                        ru_soldier(mesh(LAUNCHERS, 'SM_Launcher_Javelin')), ['russia_barracks']))
    out[-1]['visual'].update({'missile_model': dict(AT_MISSILE_MODEL), 'muzzle_forward_m': 0.9, 'warhead_kg': 1.2})
    out[-1]['_balance'] = ('(01/11) a força 100 × 35 ÷ 300² = 0,039, 12% abaixo do Tank Hunter, do RPG e do Javelin (0,044): o preço do '
                           'alcance (55 m contra 46) e do tiro mais pesado (70); não atira no ar')
    return out


def main():
    d = load_units()
    units = d['units']
    new = roster(units)
    index = {u['key']: i for i, u in enumerate(units)}
    for u in new:
        if u['key'] in index:
            units[index[u['key']]] = u
        else:
            index[u['key']] = len(units)
            units.append(u)
    by = {u['key']: u for u in units}
    # o que já existia e muda: o Dozer, a War Factory e o quartel da USA ganham o novo; a Technical e o Scorpion saem do Arms Dealer
    for key, field, add in (('usa_dozer', 'constructs', 'usa_strategy_center'), ('usa_war_factory', 'produces', 'usa_paladin'),
                            ('usa_barracks', 'produces', 'usa_javelin')):
        if add not in by[key][field]:
            by[key][field].append(add)
    for key in ('gla_technical', 'gla_tank'):
        by[key]['requires'] = ['gla_arms_dealer']
    # (01/11) a Rússia no menu só quando os modelos dela já estão IMPORTADOS na Unreal (o Command Center pronto e 1 trator, como a
    # China): com a pasta do Blender e sem o import, o menu oferecia uma facção de prédios invisíveis
    content = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Content')
    imported = all(os.path.isdir(os.path.join(content, p)) for p in ('Structures/russia_command_center', 'Units/russia_dozer'))
    facs = d.setdefault('factions', [])
    if 'russia_command_center' in by and imported:
        if not any(f.get('key') == 'russia' for f in facs):
            facs.append({'key': 'russia', 'name': 'Russia', 'base': 'russia_command_center', 'start': [['russia_dozer', 1]]})
    else:
        facs[:] = [f for f in facs if f.get('key') != 'russia']
    save_units(d)
    print('ELENCO', len(new), 'fichas;', len(units), 'unidades no arquivo')


if __name__ == '__main__':
    main()
