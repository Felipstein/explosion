"""Monta a cena herói (a tomada da câmera baixa do vídeo do Veo) no deserto.

Uso:
    cp Content/Maps/Range_Desert.umap Content/Maps/Range_Desert_Units.umap   (cópia em disco)
    blast.ps1 py Tools\\build_hero_scene.py

Põe tanques de verdade (/Game/Units/USA_MBT, importados por import_units.py)
no Range_Desert_Units:
- a vítima no ponto onde o teste automático detona (a origem), de lado pra
  câmera baixa como no vídeo;
- um tanque em primeiro plano à esquerda da câmera e dois atrás da vítima,
  pela composição.
Cada tanque é um ator por peça, preso ao casco, todos marcados com a tag
"HeroScene" (o script apaga e refaz). A vítima também leva "HeroVictim", e
cada peça leva o nome dela (Hull, Turret, Barrel, Tracks, Wheels), pra a
explosão achar a torre e arremessar.
"""
import math

import unreal

MAP = '/Game/Maps/Range_Desert_Units'
UNIT = '/Game/Units/USA_MBT'
PARTS = {
    'Hull': [f'{UNIT}/SM_USA_MBT_Hull/StaticMeshes/SM_USA_MBT_Hull'],
    'Turret': [f'{UNIT}/SM_USA_MBT_Turret/StaticMeshes/SM_USA_MBT_Turret'],
    'Barrel': [f'{UNIT}/SM_USA_MBT_Barrel/StaticMeshes/SM_USA_MBT_Barrel'],
    'Tracks': [f'{UNIT}/SM_USA_MBT_Tracks/StaticMeshes/track_l', f'{UNIT}/SM_USA_MBT_Tracks/StaticMeshes/track_r'],
    'Wheels': [f'{UNIT}/SM_USA_MBT_Wheels/StaticMeshes/wheel_{i:03d}' for i in range(1, 19)],
}

# A câmera baixa das folhas (-Cam=70,-0.62,...): ela fica em
# alvo + (cos A, sin A)·d no plano (BlastLabCamera::Tick, ToUE troca Y e Z).
AZIM = -0.62
F = (-math.cos(AZIM), -math.sin(AZIM))      # da câmera pro alvo, no chão
RIGHT = (-F[1], F[0])                        # direita da tela
LEFT = (-RIGHT[0], -RIGHT[1])


def yaw_of(d):
    return math.degrees(math.atan2(d[1], d[0]))


def at(back, side, side_dir):
    """back: metros do alvo na direção da câmera (negativo = atrás do alvo)."""
    return (-F[0] * back + side_dir[0] * side, -F[1] * back + side_dir[1] * side)


# (nome, posição em metros, yaw em graus, papel)
TANKS = [
    ('Victim', (0.0, 0.0), yaw_of(RIGHT) + 12.0, 'HeroVictim'),
    ('Foreground', at(26.0, 7.0, LEFT), yaw_of(F) - 25.0, 'HeroExtra'),
    ('BackRight', at(-15.0, 12.0, RIGHT), yaw_of(LEFT) + 10.0, 'HeroExtra'),
    ('BackLeft', at(-20.0, 14.0, LEFT), yaw_of(RIGHT) - 30.0, 'HeroExtra'),
]


def ground_z(world, x, y):
    start = unreal.Vector(x * 100.0, y * 100.0, 100000.0)
    end = unreal.Vector(x * 100.0, y * 100.0, -100000.0)
    hit = unreal.SystemLibrary.line_trace_single(world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                                                 False, [], unreal.DrawDebugTrace.NONE, True)
    if hit:
        t = hit.to_tuple()
        loc = t[4] if len(t) > 4 else None
        if isinstance(loc, unreal.Vector):
            return loc.z
    unreal.log_warning(f'HERO sem chão em ({x:.1f}, {y:.1f}): z = 0')
    return 0.0


def main():
    subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not subsystem.load_level(MAP):
        unreal.log_error(f'HERO não abriu {MAP} (copie Range_Desert.umap em disco antes)')
        return
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    old = [a for a in actors.get_all_level_actors() if 'HeroScene' in [str(t) for t in a.tags]]
    for a in old:
        actors.destroy_actor(a)
    for name, (x, y), yaw, role in TANKS:
        z = ground_z(world, x, y)
        loc = unreal.Vector(x * 100.0, y * 100.0, z)
        rot = unreal.Rotator(0.0, 0.0, yaw)
        hull = None
        for part, meshes in PARTS.items():
            for path in meshes:
                mesh = unreal.load_asset(path)
                if not mesh:
                    unreal.log_error(f'HERO falta {path}')
                    continue
                a = actors.spawn_actor_from_class(unreal.StaticMeshActor, loc, rot)
                a.static_mesh_component.set_static_mesh(mesh)
                a.set_actor_label(f'Hero_{name}_{path.rsplit("/", 1)[-1]}')
                a.set_folder_path(f'HeroScene/{name}')
                a.tags = ['HeroScene', role, name, part]
                if part == 'Hull' and hull is None:
                    hull = a
                elif hull:
                    a.attach_to_actor(hull, '', unreal.AttachmentRule.KEEP_WORLD,
                                      unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
        unreal.log(f'HERO {name} ({role}) em ({x:.1f}, {y:.1f}, {z / 100.0:.2f}) m, yaw {yaw:.0f}°')
    unreal.EditorLoadingAndSavingUtils.save_current_level()


main()
