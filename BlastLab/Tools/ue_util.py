"""Funções comuns das ferramentas: caminhos, conversão WebGL → Unreal e câmeras.

WebGL: destro, y pra cima, metros. Unreal: canhoto, z pra cima, centímetros.
Trocar y↔z faz as duas coisas (a troca de dois eixos inverte a orientação):
  posição (x, y, z) → (x, z, y)·100
  rotação: a mesma rotação vista nos eixos trocados tem ângulo invertido →
  quaternion (x, y, z, w) → (−x, −z, −y, w)
"""
import math
import os

import unreal

TOOLS = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.abspath(os.path.join(TOOLS, '..'))
REPO = os.path.abspath(os.path.join(PROJECT, '..'))


def ue_pos(p):
    return unreal.Vector(p[0] * 100.0, p[2] * 100.0, p[1] * 100.0)


def ue_quat(q):
    return unreal.Quat(-q[0], -q[2], -q[1], q[3])


def camera(label, dist, azim, elev, target_y, fov_v=0.58):
    """Câmera de cinema no enquadramento do WebGL (js/camera.js: órbita em
    torno da origem; fov vertical em 16:9)."""
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    target = [0.0, target_y, 0.0]
    p = [target[0] + math.cos(azim) * math.cos(elev) * dist,
         target[1] + math.sin(elev) * dist,
         target[2] + math.sin(azim) * math.cos(elev) * dist]
    pos, look = ue_pos(p), ue_pos(target)
    rot = unreal.MathLibrary.find_look_at_rotation(pos, look)
    cam = actors.spawn_actor_from_class(unreal.CineCameraActor, pos, rot)
    cam.set_actor_label(label)
    cam.set_folder_path('Cameras')
    cc = cam.get_cine_camera_component()
    fb = cc.get_editor_property('filmback')
    fb.set_editor_property('sensor_width', 36.0)
    fb.set_editor_property('sensor_height', 20.25)
    cc.set_editor_property('filmback', fb)
    fov_h = 2 * math.atan(math.tan(fov_v / 2) * 16 / 9)
    cc.set_editor_property('current_focal_length', 18.0 / math.tan(fov_h / 2))
    fs = cc.get_editor_property('focus_settings')
    fs.set_editor_property('focus_method', unreal.CameraFocusMethod.DISABLE)
    cc.set_editor_property('focus_settings', fs)
    return cam
