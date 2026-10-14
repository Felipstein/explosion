"""A receita do look (JSON) vira o DataAsset do plugin RTSLook (docs/GRAFICOS.md §8).

Rodar na Unreal do jogo, sem janela, com o plugin RTSLook ligado e compilado:
  UnrealEditor-Cmd.exe Game/RTSGame.uproject -run=pythonscript -script=Game/Tools/look_recipe_import.py -nullrhi
Lê Game/Tools/look_recipes/*.json (a fonte, a mesma da maquete) e grava /RTSLook/Recipes/DA_Look_<cena>.
Os nomes seguem o look_lab.py (environment()): o que falta no JSON fica no padrão da estrutura.
"""
import glob
import json
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
FOLDER = '/RTSLook/Recipes'
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def col8(v):
    return unreal.Color(r=int(v[0]), g=int(v[1]), b=int(v[2]), a=255)


def lin(v):
    return unreal.LinearColor(float(v[0]), float(v[1]), float(v[2]), 1.0)


def put(struct, key, value):
    try:
        struct.set_editor_property(key, value)
    except Exception as e:
        unreal.log_warning(f'LOOKRECIPE {key}: {e}')


def build(R):
    sun, atm, sky, fog, post, world = (unreal.LookSun(), unreal.LookAtmosphere(), unreal.LookSkyLight(),
                                       unreal.LookFog(), unreal.LookPost(), unreal.LookWorld())
    S = R.get('sun', {})
    for k, f in (('elevation', 'elevation'), ('from_camera', 'from_camera'), ('lux', 'lux'),
                 ('source_angle', 'source_angle'), ('specular_scale', 'specular_scale')):
        if k in S:
            put(sun, f, float(S[k]))
    if S.get('temperature'):
        put(sun, 'temperature', float(S['temperature']))
    if S.get('color'):
        put(sun, 'color', col8(S['color']))
    A = R.get('atmosphere', {})
    for k in ('rayleigh_scattering_scale', 'mie_scattering_scale', 'mie_absorption_scale', 'mie_anisotropy',
              'mie_exponential_distribution'):
        if k in A:
            put(atm, k, float(A[k]))
    for k in ('mie_scattering', 'mie_absorption'):
        if k in A:
            put(atm, k, lin(A[k]))
    if 'ground_albedo' in A:
        put(atm, 'ground_albedo', col8(A['ground_albedo']))
    if 'aerial_distance_scale' in A:
        put(atm, 'aerial_distance_scale', float(A['aerial_distance_scale']))
    if 'aerial_start_ratio' in A:
        put(atm, 'aerial_start_ratio', float(A['aerial_start_ratio']))
    K = R.get('skylight', {})
    put(sky, 'intensity', float(K.get('intensity', 1.0)))
    if 'color' in K:
        put(sky, 'color', col8(K['color']))
    F = R.get('fog', {})
    put(fog, 'density', float(F.get('density', 0.0)))
    put(fog, 'falloff', float(F.get('falloff', 0.2)))
    put(fog, 'start_ratio', float(F.get('start_ratio', 0.75)))
    V = F.get('volumetric')
    put(fog, 'volumetric', bool(V))
    if V:
        put(fog, 'volumetric_albedo', col8(V.get('albedo', [255, 255, 255])))
        put(fog, 'volumetric_scattering_distribution', float(V.get('scattering_distribution', 0.2)))
        put(fog, 'volumetric_extinction_scale', float(V.get('extinction_scale', 1.0)))
        put(fog, 'volumetric_distance', float(V.get('distance_cm', 6000.0)))
        put(fog, 'volumetric_start_ratio', float(V.get('start_ratio', F.get('start_ratio', 0.75))))
    P = R.get('post', {})
    put(post, 'ev100', float(P.get('ev100', 14.0)))
    for k, f in (('bias', 'bias'), ('bloom', 'bloom'), ('local_exposure_shadow', 'local_exposure_shadow'),
                 ('local_exposure_highlight', 'local_exposure_highlight'), ('white_temp', 'white_temp'),
                 ('saturation', 'saturation'), ('contrast', 'contrast'), ('film_toe', 'film_toe'),
                 ('film_shoulder', 'film_shoulder')):
        if k in P and not isinstance(P[k], list):
            put(post, f, float(P[k]))
    for k in ('gain', 'shadow_gain', 'midtone_gain', 'highlight_gain'):
        if k in P:
            v = P[k] if isinstance(P[k], list) else [P[k]] * 3
            put(post, k, lin(v))
    raw = P.get('raw', {})
    if 'vignette_intensity' in raw:
        put(post, 'vignette', float(raw['vignette_intensity']))
    if 'color_correction_highlights_min' in raw:
        put(post, 'highlights_min', float(raw['color_correction_highlights_min']))
    if 'color_correction_shadows_max' in raw:
        put(post, 'shadows_max', float(raw['color_correction_shadows_max']))
    W = R.get('world', {})
    if 'dust' in W:
        put(world, 'dust_color', lin(W['dust']))
    for k, f in (('unit_dust_top', 'unit_dust_top'), ('unit_dust_low', 'unit_dust_low'), ('unit_dust_height', 'unit_dust_height')):
        if k in W:
            put(world, f, float(W[k]))
    # (a água entrou no plugin em 12/10: com o binário antigo, sem a estrutura, a receita segue sem ela)
    water = unreal.LookWater() if hasattr(unreal, 'LookWater') else None
    Wt = R.get('water')
    if Wt and water is not None:
        put(water, 'override', True)
        put(water, 'body_color', lin(Wt.get('body_color', [0.05, 0.35, 0.4])))
        put(water, 'body_strength', float(Wt.get('body_strength', 0.0)))
        put(water, 'sky_reflection', float(Wt.get('sky_reflection', 1.0)))
    presets = []
    for name in ('baixa', 'media', 'alta', 'ultra'):
        q = R.get('presets', {}).get(name)
        p = unreal.LookPreset()
        if q:
            put(p, 'lumen', bool(q.get('lumen', True)))
            put(p, 'volumetric_fog', bool(q.get('volumetric_fog', True)))
            put(p, 'sky_light_scale_without_lumen', float(q.get('sky_light_scale_without_lumen', 1.0)))
        presets.append(p)
    return sun, atm, sky, fog, post, world, presets, water


def main():
    only = [n for n in os.environ.get('LOOK_RECIPES', '').split(',') if n]   # só estas (o A/B no jogo, 13/10)
    for path in sorted(glob.glob(os.path.join(HERE, 'look_recipes', '*.json'))):
        if only and os.path.splitext(os.path.basename(path))[0] not in only:
            continue
        name = os.path.splitext(os.path.basename(path))[0]
        R = json.load(open(path, encoding='utf-8'))
        for sect, over in R.get('game', {}).items():      # o que muda só no jogo (relevo, água) vai por cima
            if isinstance(over, dict):
                R.setdefault(sect, {}).update(over)
        asset_path = f'{FOLDER}/DA_Look_{name}'
        # (o registro de assets do commandlet nem sempre conhece o conteúdo do plugin: tenta carregar direto do disco)
        da = unreal.load_asset(asset_path)
        if da is not None and not isinstance(da, unreal.LookRecipe):
            da = None
        if da is None and eal.does_asset_exist(asset_path):
            # (não carregou: a estrutura mudou no plugin e o pacote velho ficou inválido) refaz do zero
            unreal.log_warning(f'LOOKRECIPE {name}: o asset antigo não carregou, refazendo')
            eal.delete_asset(asset_path)
        if da is None:
            fac = unreal.DataAssetFactory()
            fac.set_editor_property('data_asset_class', unreal.LookRecipe)
            da = tools.create_asset(f'DA_Look_{name}', FOLDER, unreal.LookRecipe, fac)
        sun, atm, sky, fog, post, world, presets, water = build(R)
        for k, v in (('sun', sun), ('atmosphere', atm), ('sky_light', sky), ('fog', fog), ('post', post),
                     ('world', world), ('presets', presets), ('water', water)):
            if v is not None:
                da.set_editor_property(k, v)
        eal.save_loaded_asset(da)
        unreal.log(f'LOOKRECIPE {name} -> {asset_path}')


main()
