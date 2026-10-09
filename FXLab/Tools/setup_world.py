"""Os efeitos aprovados no deserto do WorldForge, com a luz do jogo.

Uso: fxlab.ps1 worlds   (copia antes a fatia do WorldForge pra cá: Content/Slice e
Maps/World_Desert.umap; a do WorldForge não é tocada; roda na Unreal, sem janela)

Diferente do BlastLab (build_worlds.py), a luz da fatia FICA: sol de 120 mil lux a 20°,
atmosfera, céu da SkyLight, névoa e a exposição travada são as do jogo. O sol ganha a
marca "GameSun": com ela os cartões leem o sol e o céu do mapa (FXSceneLight.h) e o
controlador usa a câmera do jogo.

Monta três grupos iguais ao do laboratório (o tanque, o fogo no anel da torre e o tiro
no chão 9 m à frente e 2 m ao lado), em chão de areia plano e sem pedra (raios de cima,
só o Landscape conta). O grupo 1 tem a marca "Load1" (a carga 1+1), os outros "Load3"
(a carga 3+3). Cada fogo e cada tiro tem a sua semente, e os tiros saem desencontrados.
"""
import builtins
import math
import os
import sys

import unreal

builtins.FXLAB_NO_BUILD = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import setup_fxlab as fx  # noqa: E402

MAP = '/Game/Maps/World_Desert'
TANK_PARTS = ('Hull', 'Turret', 'Barrel', 'Tracks', 'Wheels')
IMPACT_OFFSET = (900.0, 200.0)       # cm, no giro do tanque (o laboratório: originX 9, originY 2)
# A cor da poeira do tiro = a do chão do mapa. Na arte 4.1 a poeira tem a MESMA cor da areia ao sol
# (matiz ~33°, saturação ~0,6 nas duas; medido 08/10), só um pouco mais clara. Então a poeira leva o
# tom do albedo da areia deste deserto, medido pelo BlastLab nas fotos do Poly Haven × a tinta do
# WorldForge (BlastLab/Tools/build_worlds.py, RANGES['Range_Desert'] = (0,35, 0,21, 0,11)),
# normalizado pro máximo de 0,95. A do impact.json foi afinada sob o sol laranja de fim de tarde do
# laboratório; com o sol branco do jogo ela saía cinza-branca, colada na areia (08/10).
SAND_ALBEDO = (0.35, 0.21, 0.11)
GROUND_TINT = tuple(round(c / max(SAND_ALBEDO) * 0.95, 3) for c in SAND_ALBEDO)
# a câmera do jogo olha pra 45°: a direita da tela é (−sen 45°, cos 45°); os grupos extras vão de
# cada lado do principal, a ~32 m (a vista do jogo cobre ~93 m de largura)
CAM_RIGHT = (-math.sin(math.radians(45.0)), math.cos(math.radians(45.0)))
GROUPS = (
    # (desvio na direita da tela em m, giro do tanque, semente do fogo, semente do tiro, 1º tiro em s)
    (0.0, 0.0, 7, 11, 0.0),
    (-32.0, 60.0, 1013, 1031, 1.6),
    (32.0, -50.0, 2029, 2053, 3.3),
)


def log(*a):
    unreal.log('FXLAB ' + ' '.join(str(x) for x in a))


def editor_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


_first = [True]


def ground(x, y):
    """(z, normal z) do Landscape em (x, y) cm, ou None se o raio bate em outra coisa (pedra, planta)."""
    hit = unreal.SystemLibrary.line_trace_single(
        editor_world(), unreal.Vector(x, y, 60000.0), unreal.Vector(x, y, -60000.0),
        unreal.TraceTypeQuery.ECC_VISIBILITY, True, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    # o FHitResult em tupla: os vetores vêm na ordem Location, ImpactPoint, Normal, ImpactNormal,
    # TraceStart, TraceEnd; o dono sai do componente atingido
    t = hit.to_tuple()
    if _first[0]:
        _first[0] = False
        log('raio de teste:', t)
    vecs = [e for e in t if isinstance(e, unreal.Vector)]
    comps = [e for e in t if isinstance(e, unreal.PrimitiveComponent)]
    actor = comps[0].get_owner() if comps else None
    if len(vecs) < 4 or actor is None:
        return None
    p, n = vecs[1], vecs[3]
    if not isinstance(actor, unreal.LandscapeProxy):
        return None
    return p.z, n.z


def flat(x, y, radius, rings=(0.5, 1.0), n=8, max_dz=35.0, min_nz=0.97):
    """Chão de Landscape plano num círculo: devolve a altura do centro ou None."""
    c = ground(x, y)
    if c is None or c[1] < min_nz:
        return None
    for r in rings:
        for k in range(n):
            a = 2 * math.pi * k / n
            g = ground(x + math.cos(a) * radius * r, y + math.sin(a) * radius * r)
            if g is None or abs(g[0] - c[0]) > max_dz or g[1] < min_nz:
                return None
    return c[0]


def rot2(x, y, deg):
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    return x * c - y * s, x * s + y * c


def group_fits(x, y, yaw):
    """O tanque (raio 5 m) e o tiro (raio 4 m) em chão plano de areia: (z do tanque, (x, y, z) do tiro)."""
    zt = flat(x, y, 500.0)
    if zt is None:
        return None
    ox, oy = rot2(*IMPACT_OFFSET, yaw)
    zi = flat(x + ox, y + oy, 400.0)
    if zi is None:
        return None
    return zt, (x + ox, y + oy, zi)


def find_spot(cx, cy, yaw, search_m, step_m=4.0):
    """O lugar que serve mais perto de (cx, cy) cm, numa espiral de quadrados."""
    best = None
    n = int(search_m / step_m)
    cands = []
    for i in range(-n, n + 1):
        for j in range(-n, n + 1):
            cands.append((i * i + j * j, cx + i * step_m * 100.0, cy + j * step_m * 100.0))
    cands.sort()
    for _, x, y in cands:
        fit = group_fits(x, y, yaw)
        if fit:
            best = (x, y, fit)
            break
    return best


def load_tank():
    meshes = {}
    for part in TANK_PARTS:
        folder = f'/Game/Units/USA_MBT/SM_USA_MBT_{part}/StaticMeshes'
        ms = [unreal.load_asset(p) for p in sorted(fx.eal.list_assets(folder, recursive=False))]
        meshes[part] = [m for m in ms if isinstance(m, unreal.StaticMesh)]
        if not meshes[part]:
            raise RuntimeError(f'sem o tanque em {folder} (rode fxlab.ps1 setup antes)')
    return meshes


def place_group(i, meshes, spot, yaw, tag):
    """O tanque montado como no laboratório, girado e posto no chão; devolve o anel da torre (cm)."""
    x, y, (zt, _) = spot
    hatch = fx.place_tank(meshes)              # monta na origem com os rótulos Tank_*
    parts = [a for a in fx.actors.get_all_level_actors() if a.get_actor_label().startswith('Tank_')]
    for a in parts:
        loc = a.get_actor_location()
        lx, ly = rot2(loc.x, loc.y, yaw)
        a.set_actor_location(unreal.Vector(x + lx, y + ly, zt - 5.0 + loc.z), False, False)
        r = a.get_actor_rotation()
        a.set_actor_rotation(unreal.Rotator(roll=r.roll, pitch=r.pitch, yaw=r.yaw + yaw), False)
        a.set_actor_label(f'G{i}_' + a.get_actor_label())
        a.set_folder_path(f'FX/G{i}')
        a.tags = [tag]
    hx, hy = rot2(hatch.x, hatch.y, yaw)
    return unreal.Vector(x + hx, y + hy, zt - 5.0 + hatch.z)


def build():
    # o mundo dentro do arquivo ainda se chama Slice_Desert: procura o pacote pelo arquivo
    if not os.path.exists(os.path.join(fx.PROJECT, 'Content', 'Maps', 'World_Desert.umap')):
        raise RuntimeError('sem Content/Maps/World_Desert.umap (fxlab.ps1 worlds copia do WorldForge)')
    fx.levels.load_level(MAP)
    # se já montado antes (a cópia é refeita a cada worlds, mas por garantia)
    for a in fx.actors.get_all_level_actors():
        if a.get_folder_path() and str(a.get_folder_path()).startswith('FX'):
            fx.actors.destroy_actor(a)
    suns = [a for a in fx.actors.get_all_level_actors() if isinstance(a, unreal.DirectionalLight)]
    if len(suns) != 1:
        raise RuntimeError(f'esperava 1 sol na fatia, achei {len(suns)}')
    suns[0].tags = ['GameSun']
    lc = suns[0].get_component_by_class(unreal.DirectionalLightComponent)
    log('sol do jogo:', suns[0].get_actor_label(), 'intensidade', lc.get_editor_property('intensity'),
        'giro', suns[0].get_actor_rotation())
    ws = editor_world().get_world_settings()
    ws.set_editor_property('default_game_mode', unreal.FXLabGameMode)

    six = unreal.load_asset('/Game/FX/M_FXSixWay')
    prim = unreal.load_asset('/Game/FX/M_FXPrim')
    if not six or not prim:
        raise RuntimeError('sem os materiais do FXLab (rode fxlab.ps1 setup antes)')
    meshes = load_tank()

    main = None
    for i, (side_m, yaw, fire_seed, imp_seed, first) in enumerate(GROUPS):
        if main is None:
            spot = find_spot(0.0, 0.0, yaw, 60.0)
        else:
            cx = main[0] + CAM_RIGHT[0] * side_m * 100.0
            cy = main[1] + CAM_RIGHT[1] * side_m * 100.0
            spot = find_spot(cx, cy, yaw, 14.0, 2.0)
        if not spot:
            raise RuntimeError(f'grupo {i}: não achei chão plano de areia')
        if main is None:
            main = spot
        tag = 'Load1' if i == 0 else 'Load3'
        hatch = place_group(i, meshes, spot, yaw, tag)
        fire = fx.actors.spawn_actor_from_class(unreal.FXWreckFire, hatch)
        fire.set_actor_label(f'G{i}_WreckFire')
        fire.set_editor_property('card_material', six)
        fire.set_editor_property('seed', fire_seed)
        fire.set_folder_path(f'FX/G{i}')
        fire.tags = [tag]
        ix, iy, iz = spot[2][1]
        imp = fx.actors.spawn_actor_from_class(unreal.FXImpact, unreal.Vector(ix, iy, iz))
        imp.set_actor_label(f'G{i}_Impact')
        imp.set_editor_property('card_material', six)
        imp.set_editor_property('prim_material', prim)
        imp.set_editor_property('placed_in_world', True)
        imp.set_editor_property('seed', imp_seed)
        imp.set_editor_property('first_shot', first)
        imp.set_editor_property('ground_tint', unreal.LinearColor(*GROUND_TINT, 1.0))
        imp.set_folder_path(f'FX/G{i}')
        imp.tags = [tag]
        log(f'grupo {i} ({tag}): tanque em ({spot[0] / 100:.1f}, {spot[1] / 100:.1f}, {spot[2][0] / 100:.2f}) m, '
            f'giro {yaw}, tiro em ({ix / 100:.1f}, {iy / 100:.1f}, {iz / 100:.2f}) m')
    fx.levels.save_current_level()
    log('mapa salvo', MAP)


build()
