"""Campos de teste com chão de verdade: as fatias do WorldForge (deserto com
estrada, neve, floresta com grama) com o céu, a luz e o simulador do BlastLab.

Rodar: powershell -File BlastLab/Tools/blast.ps1 worlds. Ele copia antes do
WorldForge o Content/Slice e cada mapa Slice_<bioma> já como
/Game/Maps/Range_<bioma> (fora do git, como lá), e este script edita a cópia
no lugar; a do WorldForge não é tocada. Duplicar o mapa pela Unreal
(duplicate_asset) estragava o terreno: preto, com uma camada branca riscada
por cima (06/10).

A luz da fatia (sol a 20°, névoa, exposição travada) sai: as explosões foram
calibradas com a do simulador, e a hora do dia é dele. O terreno, as rochas,
a vegetação, as árvores e os decalques (estrada, marcas de pneu) ficam.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_scene as scene  # noqa: E402

# Os campos feitos das fatias (a Arena, com os quatro chãos juntos, quando
# existir) e o albedo do chão além da borda: a média linear da foto da camada
# base de cada fatia × a tinta que o WorldForge aplica nela (build_slice.py),
# medida 06/10 nas fotos do Poly Haven em WorldForge/Intermediate/Assets:
# - deserto: sandy_gravel_02 (0.32, 0.17, 0.09) e aerial_beach_01 × (1.32, 1.02,
#   0.72) = (0.37, 0.24, 0.14), que o material mistura sempre → a média;
# - neve: snow_02 0.38 × 2.1 = 0.80 (a neve gerada por cima é 0.82);
# - grama: leafy_grass (0.32, 0.23, 0.10) × (0.45, 0.72, 0.55).
RANGES = {
    'Range_Desert': (0.35, 0.21, 0.11),
    'Range_Snow': (0.80, 0.80, 0.80),
    'Range_Forest': (0.14, 0.17, 0.06),
    'Range_Arena': (0.35, 0.21, 0.11),
}
# A cor da poeira que a explosão levanta (BlastLabEnvironment DustTint, × o
# albedo 0.78 da poeira no M_Explosion): a poeira fina é mais clara e menos
# saturada que o chão. Deserto: a da arte alvo (matiz ~29°, saturação ~0.55);
# neve: pó de neve com um pouco de terra; grama: terra marrom de baixo dela.
DUST = {
    'Range_Desert': (0.95, 0.74, 0.52),
    'Range_Snow': (0.92, 0.92, 0.95),
    'Range_Forest': (0.66, 0.53, 0.40),
    'Range_Arena': (0.95, 0.74, 0.52),
}
# a luz que a fatia traz; o simulador põe a dele (build_scene.environment)
LIGHTING = (unreal.DirectionalLight, unreal.SkyLight, unreal.SkyAtmosphere, unreal.ExponentialHeightFog,
            unreal.PostProcessVolume, unreal.VolumetricCloud)


def log(*a):
    unreal.log('WORLDS ' + ' '.join(str(x) for x in a))


def build(path, albedo):
    scene.levels.load_level(path)
    removed, low, half = 0, 0.0, 0.0
    for a in scene.actors.get_all_level_actors():
        if isinstance(a, LIGHTING) or a.get_actor_label() in ('Surroundings', 'SkyDome', 'Environment'):
            scene.actors.destroy_actor(a)
            removed += 1
        elif isinstance(a, unreal.LandscapeProxy):
            o, e = a.get_actor_bounds(False)
            low = min(low, (o.z - e.z) / 100.0)
            half = max(half, e.x / 100.0, e.y / 100.0)
    scene.environment(build_assets=False)
    name = path.rsplit('/', 1)[1]
    for a in scene.actors.get_all_level_actors():
        if isinstance(a, unreal.BlastLabEnvironment):
            a.set_editor_property('dust_tint', unreal.LinearColor(*DUST.get(name, (0.80, 0.68, 0.55)), 1.0))
    # Além da borda da fatia: um chão liso da cor média do chão dela, rente ao
    # ponto mais baixo do terreno. Sem ele, o terreno de ~250 m ficava solto no
    # céu; com o chão do campo de teste (cinza), a borda parecia um mar
    plane = unreal.load_asset('/Engine/BasicShapes/Plane.Plane')
    ground = scene.piece(plane, unreal.load_asset(scene.BASE_MATERIAL), [0, low - 0.3, 0],
                         [20000, 1, 20000], [0, 0, 0, 1], albedo, 0.9, 0.0, 'Scenery')
    ground.set_actor_label('Surroundings')
    scene.levels.save_current_level()
    log(path, 'removed', removed, 'terrain half %.0f m, lowest %.1f m' % (half, low))


def main():
    built = []
    for name, albedo in RANGES.items():
        # o mundo dentro do arquivo ainda se chama Slice_<bioma>: procura o pacote pelo arquivo
        if os.path.exists(os.path.join(scene.PROJECT, 'Content', 'Maps', name + '.umap')):
            build('/Game/Maps/' + name, albedo)
            built.append(name)
    if not built:
        raise RuntimeError('nenhum Range_* em /Game/Maps (rode blast.ps1 worlds, que copia as fatias do WorldForge)')
    log('built', built)


if __name__ == '__main__':
    main()
