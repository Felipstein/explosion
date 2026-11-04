"""Os retratos do HUD no kit (04/11/2026): o caminho APROVADO ("tá pode ser") é o Atelier (GPT) com a foto do nosso modelo no jogo
(o desenho) e os retratos aprovados do M1 (o estilo: o botão no cinza de estúdio, o cartão no deserto padrão, a peça parada). O
render direto do modelo no Blender foi REPROVADO ("parece jogo de brinquedo, de lego, de low poly"); os prompts que deram certo
estão em captures/concept/185_retrato_war_factory_botao/ e 186_retrato_war_factory_cartao/.

  python Game/Tools/portrait_kit.py <imagem do botão|-> <imagem do cartão|-> <nome da foto> [posição do corte do cartão, 0 a 1]

- o botão: a imagem 1:1 do gerador reduzida pra 256 x 256 (photos/buttons/<nome>.png);
- o cartão: o recorte de 2,43:1 da imagem 16:9 (o tamanho do kit, 728 x 300; photos/card/<nome>.png). O corte fica (04/11, ele: "deixa
  do jeito que tava antes, mesmo cortado fica melhor"); a posição sobe (0,2) quando o alto da peça é alto (a torre da usina). Pra não
  perder nada, o prompt pede a peça menor no quadro.
O nome é o do PhotoName do RTSHUDNew.cpp (a chave da unidade; a Patriot é usa_missile_defense, o M1 usa_battle_tank).
"""
import os
import sys

from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
KIT = os.path.join(REPO, 'docs', 'concept', 'hud', 'kit', 'photos')


def main():
    btn, crd, name = sys.argv[1], sys.argv[2], sys.argv[3]
    pos = float(sys.argv[4]) if len(sys.argv) > 4 else 0.5
    if btn != '-':
        Image.open(btn).convert('RGB').resize((256, 256), Image.LANCZOS).save(os.path.join(KIT, 'buttons', name + '.png'))
    if crd != '-':
        c = Image.open(crd).convert('RGB')
        w, h = c.size
        ch = int(w / 2.4267)
        y0 = int((h - ch) * pos)
        c.crop((0, y0, w, y0 + ch)).resize((728, 300), Image.LANCZOS).save(os.path.join(KIT, 'card', name + '.png'))
    print('ok', name)


if __name__ == '__main__':
    main()
