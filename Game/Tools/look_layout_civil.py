"""O kit civil da vila (as malhas provisórias da sessão de gráficos pro mapa jogável; os finais são do Felipe).

Lido por look_houses.py quando LOOK_SCENE=civil_kit (look.ps1 civil). Os tipos e as plantas são os do
village.json do WorldForge (docs/WORLDFORGE.md §12), na grade de 4 m e no giro 0:
- a frente (a porta) olha +Y; a origem no centro da planta, no chão; a malha cabe dentro da planta;
- uma saia de 0,4 m abaixo da origem (o chão é aplainado por quarteirão; a borda pode deixar degrau);
- os nomes: SM_civ_<tipo>.glb em captures/look/civil_glb/ (as casas daqui; carros e miudezas pelo look_civil_props.py).
As fichas aprovadas: docs/concept/buildings/civil/ (as casas de adobe da cena day_game_cam).
"""
SKIRT = 0.4

# (X = largura, Y = fundo); o corpo é 0,4 m menor que a planta de cada lado (as calhas saem ~13 cm do corpo)
HOUSES = {
    'civ_house_1story': dict(x=0, y=0, sx=7.2, sy=7.2, h=4.5, door='+Y', ex={'seed': 101, 'roofbox': (1, -1)}),
    'civ_house_2story': dict(x=0, y=0, sx=7.2, sy=7.2, h=7.5, door='+Y', ex={'seed': 102, 'stories': 2, 'tank': (-1, -1)}),
    'civ_house_long': dict(x=0, y=0, sx=15.2, sy=7.2, h=4.5, door='+Y', ex={'seed': 103, 'beams': '+Y', 'roofbox': (-1, -1)}),
    # a varanda na frente (+Y): o corpo 7,5 × 8,6 recuado 1,5 m e o toldo de 3 m na frente, tudo dentro de 8 × 12
    'civ_house_porch': dict(x=0, y=0, sx=7.2, sy=8.6, h=4.5, door='+Y', ex={'seed': 104, 'canopy': '+Y', 'offset': (0.0, -1.5)}),
}
YARDS = {}
# os trechos de muro: 4 m ao longo de X, a origem no meio, sobre a linha do lote (um pilarete na ponta +X)
WALL_MESHES = {'civ_wall': (4.0, 2.3), 'civ_wall_gate': (4.0, 2.3)}
# o poste de madeira com a cruzeta (o fio entre postes é de quem liga um no outro)
EXTRA = ['civ_pole']
