"""Prévia de um modelo nas vistas da ficha (frente, cima, trás, direita, 3/4) e na câmera do jogo,
Cycles na CPU. Sai em <pasta do modelo>/preview_<vista>.png.

  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b <arquivo.blend> --python art/tools/render_views.py
"""
import math
import os

import bpy
from mathutils import Vector

S = bpy.context.scene
FOLDER = os.path.dirname(bpy.data.filepath)


def look(cam, target):
    cam.rotation_euler = (Vector(target) - cam.location).to_track_quat('-Z', 'Y').to_euler()


def ortho(name, loc, target, scale):
    c = bpy.data.objects.new(name, bpy.data.cameras.new(name))
    S.collection.objects.link(c)
    c.data.type = 'ORTHO'
    c.data.ortho_scale = scale
    c.location = loc
    look(c, target)
    return c


def persp(name, loc, target, lens=50):
    c = bpy.data.objects.new(name, bpy.data.cameras.new(name))
    S.collection.objects.link(c)
    c.data.lens = lens
    c.location = loc
    look(c, target)
    return c


def main():
    # luz quente e baixa vindo da frente-esquerda, como na ficha
    sun = bpy.data.objects.get('Sol_20graus')
    if sun:
        sun.data.energy = 4.5
        sun.data.color = (1.0, 0.86, 0.7)
        sun.rotation_euler = (math.radians(55), 0.0, math.radians(-35))
    S.render.engine = 'CYCLES'
    S.cycles.device = 'CPU'
    S.cycles.samples = 32
    S.cycles.use_denoising = True
    S.view_settings.view_transform = 'AgX'
    views = {
        'front': (ortho('cam_front', (0, -40, 2.4), (0, 0, 2.4), 9.6), 1200, 760),
        'back': (ortho('cam_back', (0, 40, 2.4), (0, 0, 2.4), 9.6), 1200, 760),
        'right': (ortho('cam_right', (40, 0, 2.4), (0, 0, 2.4), 9.6), 1200, 760),
        'top': (ortho('cam_top', (0, 0, 40), (0, 0.0001, 0), 9.2), 1000, 1000),
        '34': (persp('cam_34', (14, -16, 11.5), (0.3, 0, 1.6), 45), 1400, 1000),
    }
    for name, (cam, w, h) in views.items():
        S.camera = cam
        S.render.resolution_x, S.render.resolution_y = w, h
        S.render.filepath = os.path.join(FOLDER, f'preview_{name}.png')
        bpy.ops.render.render(write_still=True)
        print('VIEW', name)
    cam = bpy.data.objects.get('Camera_Jogo')
    if cam:
        S.camera = cam
        S.render.resolution_x, S.render.resolution_y = 1920, 1080
        S.render.filepath = os.path.join(FOLDER, 'preview_game.png')
        bpy.ops.render.render(write_still=True)
        print('VIEW game')


main()
