"""Prepara o arquivo de um modelo novo a partir do modelo-base (docs/PRODUCAO_3D.md §9).

O padrão de todo modelo: copia captures/art3d/RTS_Base.blend pra captures/art3d/<nome>/<nome>.blend
e põe a ficha aprovada dentro, na escala certa:
- as 5 vistas recortadas, cada uma atrás da peça na vista dela e só aparecendo nela:
  cima (Numpad 7, deitada no chão), frente (Numpad 1), trás (Ctrl+Numpad 1),
  direita (Numpad 3) e esquerda (Ctrl+Numpad 3), com a câmera ortográfica;
- a ficha inteira, em pé ao lado da peça, pra olhar em perspectiva (some nas vistas retas);
- a caixa de arame com o tamanho da peça.
Na câmera do jogo (Numpad 0) as referências somem: fica só o modelo e o tanque.

Uso:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python art/tools/start_model.py [-- force]
Se o arquivo já existe ele não é tocado (o que foi modelado fica); "force" refaz do zero.
"""
import math
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
BASE = os.path.join(REPO, 'captures', 'art3d', 'RTS_Base.blend')
ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
FORCE = 'force' in ARGS
OUT_DIR = next((a[4:] for a in ARGS if a.startswith('out=')), None)     # teste: grava em outra pasta

# nome → ficha, tamanho (largura X, profundidade Y, altura Z em m) e o recorte de cada vista na
# ficha em px (x0, y0, x1, y1), medido pelo fundo cinza, só o corpo da peça (a largura do recorte =
# a largura da peça naquela vista). As vistas laterais e de trás foram identificadas pela posição
# da caixinha do teto contra a vista de cima.
PIECES = {
    'civ_house_1story': dict(
        sheet='docs/concept/buildings/civil/civ_house_1story_gemini.webp', size=(8.0, 8.0, 4.5),
        views={'top': (58, 44, 299, 287), 'front': (388, 44, 635, 206), 'back': (58, 348, 298, 507),
               'left': (352, 348, 593, 507), 'right': (720, 45, 955, 206)}),
}

# onde fica cada vista (atrás da peça, olhando pra quem olha daquele lado) e a largura dela
#   giro: X 90° põe a imagem em pé virada pra −Y; o giro em Z vira pro lado certo
PLACE = {
    'front': lambda W, D: (((0.0, D / 2 + 0.5), (90, 0, 0)), W),
    'back': lambda W, D: (((0.0, -D / 2 - 0.5), (90, 0, 180)), W),
    'right': lambda W, D: (((-W / 2 - 0.5, 0.0), (90, 0, 90)), D),
    'left': lambda W, D: (((W / 2 + 0.5, 0.0), (90, 0, -90)), D),
}


def crop_to_png(src, box, out):
    img = bpy.data.images.load(src)
    w, h = img.size
    x0, y0, x1, y1 = box
    cw, ch = x1 - x0, y1 - y0
    px = list(img.pixels)
    data = [0.0] * (cw * ch * 4)
    for y in range(ch):
        sy = h - 1 - (y0 + (ch - 1 - y))          # o Blender guarda as linhas de baixo pra cima
        data[y * cw * 4:(y + 1) * cw * 4] = px[(sy * w + x0) * 4:(sy * w + x1) * 4]
    new = bpy.data.images.new(os.path.basename(out), cw, ch, alpha=True)
    new.pixels = data
    new.filepath_raw = out
    new.file_format = 'PNG'
    new.save()
    new.pack()
    bpy.data.images.remove(img)
    return new


def add_ref(col, name, image, width_m, loc, rot_deg, ortho=True):
    e = bpy.data.objects.new(name, None)
    e.empty_display_type = 'IMAGE'
    e.data = image
    w, h = image.size
    e.empty_display_size = width_m * max(w, h) / w        # o lado maior da imagem = este tamanho
    e.location = loc
    e.rotation_euler = tuple(math.radians(a) for a in rot_deg)
    e.use_empty_image_alpha = True
    e.color[3] = 0.75 if ortho else 1.0
    e.empty_image_side = 'FRONT'                           # só do lado de quem olha pra ela
    e.show_empty_image_perspective = not ortho
    e.show_empty_image_orthographic = ortho
    e.show_empty_image_only_axis_aligned = ortho
    e.hide_select = True
    col.objects.link(e)
    return e


def main():
    for name, p in PIECES.items():
        folder = OUT_DIR or os.path.join(REPO, 'captures', 'art3d', name)
        path = os.path.join(folder, name + '.blend')
        if os.path.exists(path) and not FORCE:
            print('MODEL já existe, não mexo:', path)
            continue
        os.makedirs(folder, exist_ok=True)
        bpy.ops.wm.open_mainfile(filepath=BASE)
        W, D, H = p['size']
        src = os.path.join(REPO, p['sheet'])
        ref = bpy.data.collections['Referencia (nao exportar)']
        for view, box in p['views'].items():
            img = crop_to_png(src, box, os.path.join(folder, f'ref_{view}.png'))
            if view == 'top':
                add_ref(ref, 'Ref_Cima', img, W, (0.0, 0.0, 0.02), (0, 0, 0))
                continue
            ((x, y), rot), width = PLACE[view](W, D)
            hgt = width * img.size[1] / img.size[0]
            add_ref(ref, f'Ref_{view.capitalize()}', img, width, (x, y, hgt / 2), rot)
        # a ficha inteira, em pé à esquerda da peça, virada pra câmera do jogo (só em perspectiva)
        sheet = bpy.data.images.load(src)
        sheet.pack()
        add_ref(ref, 'Ref_Ficha_Inteira', sheet, 20.0, (-W / 2 - 14.0, -4.0, 6.0), (70, 0, -40), ortho=False)
        box = next((o for o in bpy.data.objects if o.name.startswith('Planta_')), None)
        if box:
            box.name = f'Planta_{W:g}x{D:g}x{H:g}m'
            box.scale = (W, D, H)
            box.location = (0.0, 0.0, H / 2)
        bpy.ops.wm.save_as_mainfile(filepath=path)
        print('MODEL pronto:', path)


main()
