"""O Supply básico PEQUENO (o SupplyPileSmall do ZH, 50 caixas = 3 750 $), 03/11/2026: o desenho 1 do conceito escolhido
docs/concept/buildings/supply_basico_4_tamanhos_ESCOLHIDO.png. A família e o estoque que some: model_supply_basic_parts.py.

Rodar (Blender sem janela, só CPU), com o detalhe AAA (a classe prédio: a grade de 40 cm e a
sombra de contato de 3,2 m; a de 14 cm triplicava os triângulos dos paletes sem mudar nada na câmera do jogo):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_supply_pile_small.py C:/Users/luisf/explosion/captures/game/models/supply_pile_small

A palavra (o que lê em 1 segundo): AS CAIXAS NOS PALETES NO CHÃO DA VILA, UMA PILHA SOB A LONA E A REDE. As caixas do estoque nos
paletes num bloco baixo de 2 × 3 (2 de altura, uma coluna com 3 no canto de trás), uma coluna solta no canto −Y e umas caixas soltas no
chão na frente; atrás, à esquerda (+Y), a pilha de reserva coberta pela lona oliva MEIO PUXADA (o pano embolado na beira, as caixas
aparecendo na ponta, as cordas amarradas no palete) e a rede de camuflagem do deserto nas 4 varas por cima dela (a rede fica sobre a
reserva, que não conta: o estoque fica no aberto, a câmera conta as caixas). Os tambores e o palete vazio encostados.

Lote 9 × 8,4 m (x −4,5..4,5; y −4,2..4,2), frente em +X. A FAIXA DOS CAMINHÕES no lado +X (x 0,9..4,5, o meio em x 2,7): entra em
(2,7; −4,2) e sai em (2,7; +4,2), livre (só o chão batido com os 2 sulcos). O estoque fica em x −4,5..0,8.
Alturas: as pilhas 2,03 m (a coluna de 3: 2,98 m), a lona ~2,1 m, a rede 2,6–3,3 m.

Peças: SM_SupplyPileSmall_Base (tudo menos as caixas do estoque) e o stock_layout.json (as vagas, na ordem em que somem).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import clear_scene, export  # noqa: E402
import model_supply_basic_parts as sp  # noqa: E402
from model_supply_basic_parts import PX, PY, STACK_DZ, PALLET_TOP  # noqa: E402
from mathutils import Vector  # noqa: E402

PREFIX = 'SM_SupplyPileSmall'
LOT = (-4.5, 4.5, -4.2, 4.2)
LANE_X, LANE_HALF = 2.7, 1.8
BLOCK = (-2.55, -1.15)            # o bloco do estoque (2 × 3, yaw 0)
RESERVE = (-2.55, 2.55)           # a reserva debaixo da lona (2 × 2 × 2, o comprido em Y)


def reserve(p):
    """A pilha de reserva (não conta) debaixo da lona oliva meio puxada: os 4 paletes, as caixas da ponta descoberta (a de enfeite), a
    lona com o rolo e as cordas, o escuro por dentro; a rede de camuflagem nas 4 varas por cima."""
    yaw = -90.0                    # o x local corre pra −Y: o fundo coberto em +Y, a ponta puxada virada pro bloco do estoque
    cx, cy = RESERVE
    for i in range(2):
        for j in range(2):
            lx, ly = (i - 0.5) * PX, (j - 0.5) * PY
            dx, dy = sp.rot(lx, ly, yaw)
            sp.pallet(p, cx + dx, cy + dy, 0.0, yaw + (sp._hash(i, j, 3) - 0.5) * 3, seed=40 + i * 2 + j)
            if i == 1:                     # a coluna da ponta descoberta
                for k in range(2):
                    sp.crate_lite(p, cx + dx + (sp._hash(i, j * 3 + k, 5) - 0.5) * 0.04, cy + dy, PALLET_TOP + k * STACK_DZ,
                                  yaw + (sp._hash(i, j + k, 6) - 0.5) * 4)
    L, W, H = 2 * PX - 0.22, 2 * PY - 0.14, STACK_DZ + 0.93
    sp.tarp_over(p, (cx, cy, PALLET_TOP), L, W, H, yaw=yaw, cover=0.66, seed=7, gaps_x=(0.0,), gaps_y=(0.0,))
    # o número de estêncil na lateral da caixa descoberta (o lote da reserva)
    dx, dy = sp.rot(0.5 * PX, 0.5 * PY, yaw)
    face = Vector((cx + dx + 0.5 + 0.012, cy + dy + 0.14, PALLET_TOP + 0.36))
    sp.stencil(p, tuple(face), (0, -1, 0), (0, 0, 1), (1, 0, 0), '17', 0.2, seed=3, wear=0.15)
    # a rede de camuflagem por cima da reserva
    sp.camo_net(p, [(-4.35, 0.85, 2.95), (-1.25, 0.85, 3.3), (-1.25, 4.15, 3.25), (-4.35, 4.15, 2.9)], seed=5, sag=0.32)


def stock(p, st):
    """O estoque no aberto: o bloco 2 × 3 nos paletes, a coluna do canto −Y, as caixas soltas na frente."""
    st.stack(p, BLOCK, [[3, 2, 2], [2, 2, 2]], yaw=0.0, seed=1)
    st.stack(p, (-1.55, -3.55), [[2]], yaw=4.0, seed=2)
    st.loose([[(-0.25, 2.95, 0.0, 14.0), (-0.22, 2.98, STACK_DZ - 0.002, 31.0)], [(-0.1, 1.45, 0.0, -21.0)]])
    st.loose([[(0.05, -3.4, 0.0, -12.0)], [(0.1, -1.85, 0.0, 26.0)]])


def props(p):
    """O resto do canto: os tambores (um deitado) e o galão."""
    sp.drum(p, -0.85, 3.85, m='Rust#drum')
    sp.drum(p, -0.25, 3.95, m='Rust#drum')
    sp.drum(p, -4.0, -3.5, lying=True, yaw=70.0)
    p.obox('Metal#can', (-0.95, 3.2, 0.25), (0.34, 0.17, 0.48), yaw=20, bevel=0.03)


def base():
    p = sp.new_part(PREFIX + '_Base', seed=5, dirt=0.6)
    sp.ground_pad(p, -4.7, 0.9, -4.35, 4.35, r=1.8, h=0.04, fall=1.2, seed=3)
    sp.lane_pad(p, LANE_X, -6.6, 6.6, half=LANE_HALF, seed=3)
    st = sp.Stock()
    stock(p, st)
    reserve(p)
    props(p)
    sp.scatter(p, -4.6, 0.8, -4.3, 4.3, 10, seed=3, keep_out=st.footprints() + [(-3.7, -1.4, 1.2, 3.9), (-1.2, 0.1, 3.5, 4.3),
                                                                               (-4.6, -3.4, -4.0, -3.0)])
    return sp.finish(p), st


def main():
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'supply_pile_small')
    os.makedirs(out, exist_ok=True)
    clear_scene()
    o, st = base()
    total = export(o, out)
    pile = st.write(out)
    sp.write_preview(out, PREFIX, st, usa_at=(3.0, -8.2, 0.0), truck=(LANE_X, 0.3, 90.0))
    print('MODEL total', total, 'tris; vagas', len(pile), '; lote 9 x 8.4 m; faixa x', LANE_X - LANE_HALF, '..', LANE_X + LANE_HALF,
          'entra (%.1f, %.1f) sai (%.1f, %.1f)' % (LANE_X, LOT[2], LANE_X, LOT[3]))


if __name__ == '__main__':
    main()
