"""Leva a maquete do look (Game/Content/LookLab) pra outro projeto da Unreal por LINK FÍSICO, sem copiar bytes.

  python Game/Tools/look_share.py <Content do projeto de destino>
  (ex.: BlastLab/Content: o BlastLab abre /Game/LookLab/Maps/LL_DesertDay_Golden e a cena de perto LL_DesertGround_Ground)

- LookLab inteiro (os mapas, as casas, o tanque, os materiais, o chão em relevo, as texturas geradas);
- do Slice (as superfícies e as malhas do WorldForge que a maquete usa), só o que faltar no destino; nada é sobrescrito;
- é uma foto: quando a maquete for refeita (look.ps1 build troca os arquivos), rodar de novo pra atualizar.
No destino: abrir o mapa e "Salvar como" numa pasta própria antes de mexer; não salvar nada dentro de LookLab.
O que é classe só do jogo e não abre fora dele: a explosão da maquete (/Script/BlastFx.ExplosionVolume).
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.abspath(os.path.join(HERE, '..', 'Content'))
DST = os.path.abspath(sys.argv[1])


def link_tree(rel, overwrite):
    made = kept = 0
    for root, _, files in os.walk(os.path.join(SRC, rel)):
        for f in files:
            s = os.path.join(root, f)
            d = os.path.join(DST, os.path.relpath(s, SRC))
            os.makedirs(os.path.dirname(d), exist_ok=True)
            if os.path.exists(d):
                if not overwrite or os.path.samefile(s, d):
                    kept += 1
                    continue
                os.remove(d)
            os.link(s, d)
            made += 1
    return made, kept


def main():
    if not os.path.isdir(DST) or not DST.lower().endswith('content'):
        raise SystemExit('o destino tem de ser a pasta Content de um projeto')
    a = link_tree('LookLab', overwrite=True)        # a maquete é minha: atualiza
    b = link_tree('Slice', overwrite=False)         # o Slice é do WorldForge: só o que faltar
    print(f'LOOKSHARE LookLab: {a[0]} ligados ({a[1]} já iguais); Slice: {b[0]} que faltavam ligados, {b[1]} já existiam (não mexi)')


main()
