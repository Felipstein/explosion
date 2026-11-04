"""A caixa do ESTOQUE do Supply básico (03/11/2026): a caixa do lançamento aéreo do jogo (model_supply_crate_v2.py, a versão B no
detalhe AAA) SEM o paraquedas e SEM o saco, pronta pra empilhar: a plataforma de aço com os 3 patins, o corpo oliva com o quadro de
ripas, as cantoneiras com os parafusos, a tampa com a FAIXA AMARELA que dobra nas pontas, as alças, as travas e as 2 cintas com a
catraca. É a instância que o jogo mostra e esconde pelo estoque nas vagas do stock_layout.json de cada variação (pile_m).

Rodar (Blender sem janela, só CPU), na classe VEHICLE (a grade de 14 cm e a sombra de contato de 1,1 m da peça pequena):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_supply_stock_crate.py C:/Users/luisf/explosion/captures/game/models/supply_stock_crate

Medidas: 1,2 × 1,0 m de planta (com as alças 1,42 × 1,18), 0,93 m de altura (a tampa; a faixa e as cintas até 0,952: a de cima senta
a 0,955). Origem no chão, no centro da caixa (Unreal X = o comprido, Y, Z cima). Peça: SM_SupplyStockCrate.
Slots (os da caixa do lançamento): Crate, Yellow, Dark, Trim, Paint.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import model_lib  # noqa: E402
from model_lib import Part, clear_scene, export  # noqa: E402
import model_supply_crate_v2 as cv2  # noqa: E402

model_lib._VC_ALL['on'] = True        # as máscaras em todos os materiais (o defeito do exportador glTF; model_lib._VC_ALL)


def main():
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'supply_stock_crate')
    os.makedirs(out, exist_ok=True)
    clear_scene()
    p = Part('SM_SupplyStockCrate')
    cv2.crate_into(p, bag=False)
    total = export(p.finish(), out)
    spec = {'parts': [['SM_SupplyStockCrate', [0, 0, 0]], ['SM_SupplyStockCrate', [1.44, 0, 0], 2.0],
                      ['SM_SupplyStockCrate', [0.02, 0.01, 0.955], -2.0]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [900, 900]],
                      ['three_quarter', 'persp', 30.0, 215.0, [1200, 900], 35.0, 0.9]],
            'usa_at': [0.0, 4.5, 0], 'game': [['game_cam_close', 37.5, 200.0, 28.0, 0.75]]}
    with open(os.path.join(out, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, indent=1)
    print('MODEL total', total, 'tris; caixa', cv2.CRATE_L, 'x', cv2.CRATE_W, 'x', cv2.LID_TOP, 'm (sem o saco)')


if __name__ == '__main__':
    main()
