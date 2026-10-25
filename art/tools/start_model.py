"""Prepara o arquivo de um modelo novo a partir do modelo-base (docs/PRODUCAO_3D.md §9).

O padrão de todo modelo: copia captures/art3d/RTS_Base.blend pra captures/art3d/<nome>/<nome>.blend
e põe a ficha aprovada dentro, na escala certa:
- as 5 vistas recortadas, cada uma atrás da peça na vista dela e só aparecendo nela:
  cima (Numpad 7, deitada no chão), frente (Numpad 1), trás (Ctrl+Numpad 1),
  direita (Numpad 3) e esquerda (Ctrl+Numpad 3), com a câmera ortográfica;
- a ficha inteira, em pé ao lado da peça, pra olhar em perspectiva (some nas vistas retas);
- a caixa de arame com o tamanho da peça.
Na câmera do jogo (Numpad 0) as referências somem: fica só o modelo e o tanque.

Peça sem ficha de 5 vistas (a picape do GLA): `refs` são recortes de arte em pé dos dois lados da peça,
virados pra câmera; `ruler_m` põe uma régua no chão; `detail_m` faz a Camera_Detalhe (o mesmo ângulo da
câmera do jogo, a essa distância da peça), que vira a câmera do render (F12).

Uso:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python art/tools/start_model.py [-- <peça> force]
Se o arquivo já existe ele não é tocado (o que foi modelado fica); "force" refaz do zero (só a <peça>, se der o nome).
"""
import math
import os
import sys

import bpy
from mathutils import Vector

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
    # a Technical do GLA, a 1ª peça do Felipe no Blender: a picape 1 da folha de escolha (24/10, a do ataque à
    # base, captures/concept/44_base_gameplay/r1_raid_settled_gemini.webp) e a ficha que o Gemini fez dela
    # (aprovada 25/10).
    # Na ficha as vistas não estão em quadrados iguais, então vale a escala dela: 93,5 px por metro, tirada
    # da largura da carroceria vista de cima (158 px = 1,69 m, a Hilux de cabine simples dos anos 90 que ela
    # imita). Dá 4,82 m de comprimento e 1,73 m até o teto, com a roda de 0,75 m. Os recortes têm o centro
    # no meio da picape e a borda de baixo no chão; o lado da ficha é o esquerdo (frente pra esquerda), que
    # é a vista Right do Blender; a de cima tem a frente pra direita (girada pra −Y).
    'gla_technical': dict(
        sheet='docs/concept/units/gla_technical_sheet.webp', size=(1.69, 4.82, 1.73),
        px_per_m=93.5, top_rot_deg=-90,
        views={'top': (40, 38, 491, 238), 'right': (549, 8, 990, 247), 'front': (169, 290, 370, 538),
               'back': (666, 290, 867, 538)},
        refs=[('captures/concept/44_base_gameplay/r1_raid_settled_gemini.webp', (48, 400, 232, 512), 'Ref_Picape_1'),
              ('captures/concept/44_base_gameplay/r1_raid_settled_gemini.webp', (298, 434, 452, 528), 'Ref_Picape_1_Atirador')],
        ruler_m=4.82, detail_m=13.0),
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


def game_view():
    """Pra onde a câmera do jogo do modelo-base olha, no chão: (frente, direita da tela, o lado em que a
    câmera está, em graus a partir de +X)."""
    cam, target = bpy.data.objects['Camera_Jogo'], bpy.data.objects['Alvo_Camera']
    f = target.location - cam.location
    f = Vector((f.x, f.y, 0.0)).normalized()
    return f, Vector((f.y, -f.x, 0.0)), math.degrees(math.atan2(-f.y, -f.x))


def ruler(col, length_m, x):
    """A régua no chão, ao longo da peça (Y), da frente (0) pra trás: metro a metro em amarelo e preto."""
    mats = [bpy.data.materials.new(n) for n in ('M_Regua_Amarela', 'M_Regua_Preta')]
    for m, rgb in zip(mats, ((0.8, 0.55, 0.02), (0.02, 0.02, 0.02))):
        m.diffuse_color = (*rgb, 1.0)
        m.use_nodes = True
        m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (*rgb, 1.0)
    root = bpy.data.objects.new(f'Regua_{length_m:g}m', None)
    col.objects.link(root)
    y0 = -length_m / 2
    marks = [float(i) for i in range(int(length_m) + 1)]
    if marks[-1] < length_m:
        marks.append(length_m)
    _, _, cam_deg = game_view()
    for i, (a, b) in enumerate(zip(marks, marks[1:])):
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(x, y0 + (a + b) / 2, 0.015))
        seg = bpy.context.active_object
        seg.name = f'Regua_{a:g}-{b:g}m'
        seg.scale = (0.15, b - a, 0.03)
        seg.data.materials.append(mats[i % 2])
        seg.parent = root
        for c in seg.users_collection:
            c.objects.unlink(seg)
        col.objects.link(seg)
    for v in marks:
        if v != marks[-1] and marks[-1] - v < 0.5:      # o "5" encavalaria no "5,2 m"
            continue
        body = f'{v:g}'.replace('.', ',') + (' m' if v == marks[-1] else '')
        txt = bpy.data.curves.new(f'Regua_{body}', 'FONT')
        txt.body = body
        txt.size = 0.3
        txt.align_x = 'RIGHT'                           # o texto cresce pra longe da régua
        txt.align_y = 'CENTER'
        t = bpy.data.objects.new(f'Regua_Texto_{body}', txt)
        t.location = (x - 0.3, y0 + v, 0.01)
        t.rotation_euler = (0.0, 0.0, math.radians(cam_deg - 270))    # o X do texto = a direita da tela
        t.data.materials.append(mats[1])
        t.parent = root
        col.objects.link(t)


def detail_camera(col, dist_m, target_z):
    """A câmera do jogo (mesmo ângulo e campo) perto da peça: é a do render."""
    base = bpy.data.objects['Camera_Jogo']
    target = bpy.data.objects.new('Alvo_Detalhe', None)
    col.objects.link(target)
    target.location = (0.0, 0.0, target_z)
    cam = base.data.copy()
    cam.name = 'Camera_Detalhe'
    ob = bpy.data.objects.new('Camera_Detalhe', cam)
    col.objects.link(ob)
    d = (base.location - bpy.data.objects['Alvo_Camera'].location).normalized()
    ob.location = target.location + d * dist_m
    t = ob.constraints.new('TRACK_TO')
    t.target = target
    t.track_axis = 'TRACK_NEGATIVE_Z'
    t.up_axis = 'UP_Y'
    bpy.context.scene.camera = ob
    # a janela 3D abre olhando a peça desse mesmo lado
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type == 'VIEW_3D':
                r3d = area.spaces[0].region_3d
                r3d.view_location = target.location
                r3d.view_rotation = (-d).to_track_quat('-Z', 'Y')
                r3d.view_distance = dist_m * 1.1


def main():
    only = [a for a in ARGS if a in PIECES]          # "-- gla_technical force" refaz só essa
    for name, p in PIECES.items():
        if only and name not in only:
            continue
        folder = OUT_DIR or os.path.join(REPO, 'captures', 'art3d', name)
        path = os.path.join(folder, name + '.blend')
        if os.path.exists(path) and not FORCE:
            print('MODEL já existe, não mexo:', path)
            continue
        os.makedirs(folder, exist_ok=True)
        bpy.ops.wm.open_mainfile(filepath=BASE)
        W, D, H = p['size']
        ref = bpy.data.collections['Referencia (nao exportar)']
        if 'sheet' in p:
            src = os.path.join(REPO, p['sheet'])
            for view, box in p['views'].items():
                img = crop_to_png(src, box, os.path.join(folder, f'ref_{view}.png'))
                # com px_per_m a largura vem do recorte; sem, o recorte é a largura da peça naquela vista
                span = (box[2] - box[0]) / p['px_per_m'] if 'px_per_m' in p else None
                if view == 'top':
                    add_ref(ref, 'Ref_Cima', img, span or W, (0.0, 0.0, 0.02), (0, 0, p.get('top_rot_deg', 0)))
                    continue
                ((x, y), rot), width = PLACE[view](W, D)
                width = span or width
                hgt = width * img.size[1] / img.size[0]
                add_ref(ref, f'Ref_{view.capitalize()}', img, width, (x, y, hgt / 2), rot)
            # a ficha inteira, em pé à esquerda da peça, virada pra câmera do jogo (só em perspectiva)
            sheet = bpy.data.images.load(src)
            sheet.pack()
            add_ref(ref, 'Ref_Ficha_Inteira', sheet, 20.0, (-W / 2 - 14.0, -4.0, 6.0), (70, 0, -40), ortho=False)
        # os recortes de arte: o 1º à esquerda da peça na tela, o 2º à direita, em pé e deitados pra trás
        # o tanto que a câmera do jogo olha de cima (37,5°), virados pra ela
        _, right, cam_deg = game_view()
        tilt = 90.0 - 37.5
        for i, (f, box, name) in enumerate(p.get('refs', [])):
            img = crop_to_png(os.path.join(REPO, f), box, os.path.join(folder, f'{name.lower()}.png'))
            width = 2.8
            hgt = width * img.size[1] / img.size[0]
            c = right * (5.0 if i else -5.0)
            add_ref(ref, name, img, width, (c.x, c.y, hgt / 2 * math.sin(math.radians(tilt))),
                    (tilt, 0, cam_deg + 90), ortho=False)
        if 'ruler_m' in p:
            ruler(ref, p['ruler_m'], -W / 2 - 0.7)
        if 'detail_m' in p:
            detail_camera(ref, p['detail_m'], H / 2)
        box = next((o for o in bpy.data.objects if o.name.startswith('Planta_')), None)
        if box:
            box.name = f'Planta_{W:g}x{D:g}x{H:g}m'
            box.scale = (W, D, H)
            box.location = (0.0, 0.0, H / 2)
        bpy.ops.wm.save_as_mainfile(filepath=path)
        print('MODEL pronto:', path)


main()
