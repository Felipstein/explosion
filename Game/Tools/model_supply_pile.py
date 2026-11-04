"""O Supply básico MÉDIO (o SupplyPile do ZH, 150 caixas = 11 250 $), 03/11/2026: o desenho 2 do conceito escolhido
docs/concept/buildings/supply_basico_4_tamanhos_ESCOLHIDO.png. A família e o estoque que some: model_supply_basic_parts.py.

Rodar (Blender sem janela, só CPU), com o detalhe AAA (a classe prédio: a grade de 40 cm; a de
14 cm triplicava os triângulos dos paletes sem mudar nada na câmera do jogo):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_supply_pile.py C:/Users/luisf/explosion/captures/game/models/supply_pile

A palavra (o que lê em 1 segundo): O QUADRADO DE PILHAS NOS PALETES COM A EMPILHADEIRA AMARELA. O bloco grande do meio (2 × 3, uma
coluna de 3), a pilha da esquerda (girada), a do fundo, as 2 da direita, umas caixas soltas na frente; no canto de trás à esquerda a
reserva debaixo da lona oliva amarrada (não conta); a empilhadeira amarela pequena estacionada na frente à esquerda; no canto da frente
à direita o L de sacos de areia com a placa de madeira pintada à mão virada pra faixa; os tambores, a pilha de paletes vazios.

Lote 14 × 14 m (x, y −7..7), frente em +X. A FAIXA DOS CAMINHÕES no lado +X (x 2,6..7,0, o meio em x 4,8): entra em (4,8; −7) e sai
em (4,8; +7), livre. O estoque fica em x −7..2,2.
Alturas: as pilhas 2,03 m (as colunas de 3: 2,98 m), a lona ~2,1 m, a grade da empilhadeira 2,1 m, a torre 2,25 m.

Peças: SM_SupplyPile_Base (tudo menos as caixas do estoque) e o stock_layout.json (as vagas, na ordem em que somem).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import clear_scene, export  # noqa: E402
import model_supply_basic_parts as sp  # noqa: E402
from model_supply_basic_parts import PX, PY, STACK_DZ, PALLET_TOP  # noqa: E402
from model_gla_palace_v2 import bag_row  # noqa: E402

PREFIX = 'SM_SupplyPile'
LOT = (-7.0, 7.0, -7.0, 7.0)
LANE_X, LANE_HALF = 4.8, 2.2
RESERVE = (-5.2, 4.3)


def stock(p, st):
    """As pilhas do estoque nos paletes e as caixas soltas da frente."""
    st.stack(p, (-1.3, -0.4), [[2, 2, 2], [2, 3, 2]], yaw=0.0, seed=1)          # o bloco do meio
    st.stack(p, (-1.8, 3.8), [[2, 2], [2, 2]], yaw=90.0, seed=2)                # a da esquerda (girada)
    st.stack(p, (-5.0, 0.0), [[2, 2], [2, 1]], yaw=0.0, seed=3)                 # a do fundo
    st.stack(p, (-5.2, -4.6), [[2, 2], [2, 2]], yaw=90.0, seed=4)               # a do fundo à direita
    st.stack(p, (-1.4, -4.6), [[2, 2], [1, 2]], yaw=0.0, seed=5)                # a da direita
    st.loose([[(1.1, 0.35, 0.0, 17.0)], [(1.05, -1.5, 0.0, -9.0)]])


def reserve(p):
    """A reserva (não conta) debaixo da lona oliva amarrada, puxada só na ponta da frente; os tambores no pé."""
    cx, cy = RESERVE
    yaw = 0.0
    for i in range(2):
        for j in range(2):
            lx, ly = (i - 0.5) * PX, (j - 0.5) * PY
            sp.pallet(p, cx + lx, cy + ly, 0.0, yaw + (sp._hash(i, j, 3) - 0.5) * 3, seed=60 + i * 2 + j)
            if i == 1:
                for k in range(2):
                    sp.crate_lite(p, cx + lx + (sp._hash(i, j * 3 + k, 5) - 0.5) * 0.04, cy + ly, PALLET_TOP + k * STACK_DZ,
                                  yaw + (sp._hash(i, j + k, 6) - 0.5) * 4)
    L, W, H = 2 * PX - 0.22, 2 * PY - 0.14, STACK_DZ + 0.93
    sp.tarp_over(p, (cx, cy, PALLET_TOP), L, W, H, yaw=yaw, cover=0.8, seed=11, gaps_x=(0.0,), gaps_y=(0.0,))
    sp.stencil(p, (cx + 0.72 + 0.6 + 0.012, cy + 0.6 + 0.14, PALLET_TOP + 0.36), (0, -1, 0), (0, 0, 1), (1, 0, 0), '42', 0.2, seed=8,
               wear=0.15)
    sp.drum(p, -3.3, 6.25, m='Rust#drum')
    sp.drum(p, -2.7, 6.4, m='Rust#drum')
    sp.drum(p, -3.0, 5.75, m='Rust#drum')


def corner(p):
    """O canto da frente à direita: o L de sacos de areia (3 fiadas), a placa pintada à mão virada pra faixa, a pilha de paletes vazios
    no fundo, o galão."""
    bag_row(p, [(1.9, -4.2), (1.9, -6.5), (-0.4, -6.5)], 0.0, layers=3, bag_size=(0.62, 0.36, 0.24), seed=3)
    sp.sign_board(p, 2.25, -2.95, yaw=0.0, w=1.45, h=0.9, z0=0.45, text='03', seed=4)
    sp.pallet_pile(p, -5.3, -2.3, 4, yaw=90.0, seed=9)
    p.obox('Metal#can', (1.35, -5.75, 0.25), (0.34, 0.17, 0.48), yaw=-25, bevel=0.03)
    sp.drum(p, 0.6, -5.85, lying=True, yaw=10.0)


def base():
    p = sp.new_part(PREFIX + '_Base', seed=7, dirt=0.6)
    sp.ground_pad(p, -7.2, 2.5, -7.1, 7.1, r=2.4, h=0.04, fall=1.4, seed=5)
    sp.lane_pad(p, LANE_X, -9.6, 9.6, half=LANE_HALF, seed=5)
    st = sp.Stock()
    stock(p, st)
    reserve(p)
    corner(p)
    sp.forklift(p, 1.35, 5.15, yaw=-95.0, seed=3)
    sp.scatter(p, -7.0, 2.3, -7.0, 7.0, 18, seed=5, keep_out=st.footprints() + [
        (-6.6, -3.8, 3.1, 5.5), (-3.6, -2.4, 5.4, 6.8), (0.8, 2.0, 2.9, 6.6), (-0.5, 2.2, -6.9, -4.0), (1.8, 2.5, -3.8, -2.1),
        (-5.9, -4.7, -2.9, -1.7)])
    return sp.finish(p), st


def main():
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'supply_pile')
    os.makedirs(out, exist_ok=True)
    clear_scene()
    o, st = base()
    total = export(o, out)
    pile = st.write(out)
    sp.write_preview(out, PREFIX, st, usa_at=(5.5, -12.5, 0.0), truck=(LANE_X, 0.6, 90.0))
    print('MODEL total', total, 'tris; vagas', len(pile), '; lote 14 x 14 m; faixa x', LANE_X - LANE_HALF, '..', LANE_X + LANE_HALF,
          'entra (%.1f, %.1f) sai (%.1f, %.1f)' % (LANE_X, LOT[2], LANE_X, LOT[3]))


if __name__ == '__main__':
    main()
