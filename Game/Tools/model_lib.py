"""Biblioteca dos modelos provisórios por script (sessão Modelos 3D, 13/10/2026).

O padrão do tanque USA do Look (Game/Tools/look_tank.py): uma bmesh por material, chanfro duro com
normais endurecidas, peça juntada num objeto só, GLB por peça que se mexe. Coordenadas da Unreal
(X frente, Y direita, Z cima, metros); o Blender recebe (x, -y, z).

Uso num modelo: sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from model_lib import *
As cores de conferência de cada slot vêm de COLORS (o modelo acrescenta as dele antes de criar as peças).
"""
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

COLORS = {}
MATS = {}


def srgb(r, g, b):
    f = lambda c: (c / 255) / 12.92 if c / 255 <= 0.04045 else ((c / 255 + 0.055) / 1.055) ** 2.4
    return (f(r), f(g), f(b), 1.0)


# as cores só pra conferência (o jogo troca pelas instâncias dele; a Team vira a cor do jogador)
def mat(name):
    if name not in MATS:
        m = bpy.data.materials.new(name)
        m.diffuse_color = srgb(*COLORS[name])
        m.use_nodes = True
        bsdf = m.node_tree.nodes['Principled BSDF']
        bsdf.inputs['Base Color'].default_value = srgb(*COLORS[name])
        bsdf.inputs['Roughness'].default_value = 0.35 if name == 'Lens' else (0.55 if name in ('Metal', 'Dark') else 0.8)
        bsdf.inputs['Metallic'].default_value = 0.5 if name == 'Metal' else 0.0
        MATS[name] = m
    return MATS[name]


def V(x, y, z):
    return Vector((x, -y, z))


class Part:
    """Uma peça: uma bmesh por material, chanfradas e juntadas no fim (o padrão do look_tank.py)."""

    def __init__(self, name):
        self.name = name
        self.bms = {}
        self.bevel = {}

    def bm(self, m, bevel=0.03):
        if m not in self.bms:
            self.bms[m] = bmesh.new()
            self.bevel[m] = bevel
        return self.bms[m]

    def box(self, m, x0, x1, y0, y1, z0, z1, bevel=None):
        b = self.bm(m, bevel if bevel is not None else 0.03)
        vs = [b.verts.new(V(x, y, z)) for z in (z0, z1) for y in (y0, y1) for x in (x0, x1)]
        for f in [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]:
            b.faces.new([vs[i] for i in f])

    def obox(self, m, c, size, yaw=0.0, pitch=0.0, roll=0.0, bevel=None):
        """Caixa girada: centro c, tamanho (sx, sy, sz), giro em graus (Unreal: yaw +X→+Y, pitch nariz pra cima).
        O eixo local x fica em (cos p cos yaw, cos p sin yaw, sin p); o z local em (−sin p cos yaw, −sin p sin yaw, cos p)."""
        b = self.bm(m, bevel if bevel is not None else 0.02)
        r = bmesh.ops.create_cube(b, size=1.0)
        M = (Matrix.Translation(V(*c)) @ Matrix.Rotation(math.radians(-yaw), 4, 'Z')
             @ Matrix.Rotation(math.radians(-pitch), 4, 'Y') @ Matrix.Rotation(math.radians(roll), 4, 'X')
             @ Matrix.Diagonal((size[0], size[1], size[2], 1.0)))
        bmesh.ops.transform(b, matrix=M, verts=r['verts'])

    def cyl(self, m, p, axis, r, length, seg=24, r2=None, bevel=None, smooth=False):
        b = self.bm(m, bevel if bevel is not None else 0.01)
        res = bmesh.ops.create_cone(b, cap_ends=True, segments=seg, radius1=r, radius2=r2 if r2 is not None else r,
                                    depth=length)
        if axis == 'x':
            R = Matrix.Rotation(math.radians(90), 4, 'Y')
        elif axis == 'y':
            R = Matrix.Rotation(math.radians(90), 4, 'X')
        elif axis == 'z':
            R = Matrix.Identity(4)
        else:
            d = V(*axis).normalized()
            R = Vector((0, 0, 1)).rotation_difference(d).to_matrix().to_4x4()
        bmesh.ops.transform(b, matrix=Matrix.Translation(V(*p)) @ R, verts=res['verts'])
        if smooth:                                   # só as faces do lado (4 vértices); as tampas ficam duras
            vs = set(res['verts'])
            for f in b.faces:
                if len(f.verts) == 4 and all(v in vs for v in f.verts):
                    f.smooth = True

    def prism_xz(self, m, pts, y0, y1, bevel=None):
        b = self.bm(m, bevel if bevel is not None else 0.03)
        a = [b.verts.new(V(x, y0, z)) for x, z in pts]
        c = [b.verts.new(V(x, y1, z)) for x, z in pts]
        b.faces.new(a)
        b.faces.new(list(reversed(c)))
        n = len(pts)
        for i in range(n):
            j = (i + 1) % n
            b.faces.new([a[i], a[j], c[j], c[i]])

    def loft(self, m, rings, bevel=None, smooth=False, caps=(True, True)):
        """Anéis (listas de (x, y, z) com o mesmo número de pontos) ligados, com tampa nas pontas."""
        b = self.bm(m, bevel if bevel is not None else 0.03)
        vr = [[b.verts.new(V(*p)) for p in ring] for ring in rings]
        if caps[0]:
            b.faces.new(list(reversed(vr[0])))
        if caps[1]:
            b.faces.new(vr[-1])
        n = len(rings[0])
        for r0, r1 in zip(vr[:-1], vr[1:]):
            for i in range(n):
                j = (i + 1) % n
                f = b.faces.new([r0[i], r0[j], r1[j], r1[i]])
                f.smooth = smooth

    def torus(self, m, c, axis, R, r, seg=28, rseg=12, bevel=0.0):
        b = self.bm(m, bevel)
        rings = []
        for k in range(seg):
            a = 2 * math.pi * k / seg
            ring = []
            for j in range(rseg):
                t = 2 * math.pi * j / rseg
                rr = R + r * math.cos(t)
                u, v, w = rr * math.cos(a), rr * math.sin(a), r * math.sin(t)
                if axis == 'x':
                    p = (w, u, v)
                elif axis == 'y':
                    p = (u, w, v)
                else:
                    p = (u, v, w)
                ring.append(b.verts.new(V(c[0] + p[0], c[1] + p[1], c[2] + p[2])))
            rings.append(ring)
        for k in range(seg):
            r0, r1 = rings[k], rings[(k + 1) % seg]
            for j in range(rseg):
                jj = (j + 1) % rseg
                f = b.faces.new([r0[j], r0[jj], r1[jj], r1[j]])
                f.smooth = True

    def finish(self):
        """Chave 'Paint#dome' = o mesmo material Paint, mas numa bmesh própria (outro chanfro, lisa)."""
        objs = []
        for m, b in self.bms.items():
            bmesh.ops.remove_doubles(b, verts=b.verts, dist=1e-5)
            bmesh.ops.recalc_face_normals(b, faces=b.faces)
            me = bpy.data.meshes.new(f'{self.name}_{m}')
            b.to_mesh(me)
            b.free()
            o = bpy.data.objects.new(f'{self.name}_{m}', me)
            bpy.context.scene.collection.objects.link(o)
            o.data.materials.append(mat(m.split('#')[0]))
            if self.bevel[m] > 0:
                harden(o, self.bevel[m])
            objs.append(o)
        select_only(objs[0])
        for o in objs:
            o.select_set(True)
        bpy.ops.object.join()
        o = bpy.context.view_layer.objects.active
        o.name = self.name
        return o


def select_only(o):
    bpy.context.view_layer.update()
    for x in bpy.context.view_layer.objects:
        if x:
            x.select_set(False)
    o.select_set(True)
    bpy.context.view_layer.objects.active = o


def harden(o, width):
    select_only(o)
    b = o.modifiers.new('bevel', 'BEVEL')
    b.width = width
    b.segments = 2 if width >= 0.02 else 1
    b.limit_method = 'ANGLE'
    b.angle_limit = math.radians(30)
    b.harden_normals = True
    b.miter_outer = 'MITER_ARC'
    bpy.ops.object.modifier_apply(modifier=b.name)
    w = o.modifiers.new('wn', 'WEIGHTED_NORMAL')
    w.keep_sharp = True
    w.weight = 50
    bpy.ops.object.modifier_apply(modifier=w.name)




def export(o, out_dir):
    select_only(o)
    path = os.path.join(out_dir, o.name + '.glb')
    bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_selection=True, export_apply=True,
                              export_yup=True, export_materials='EXPORT', export_normals=True, export_texcoords=False)
    tris = sum(len(poly.vertices) - 2 for poly in o.data.polygons)
    print('MODEL', o.name, tris, 'tris ->', path)
    return tris


def clear_scene():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)


def flag(p, pole, top, length, height, m='Team', trail=-1.0, phase=0.0):
    """Bandeira na cor do jogador: pano com onda (loft de seções finas), preso no mastro em (x, y), do topo pra baixo.
    trail −1: o pano vai pra trás (−X)."""
    rings = []
    n = 9
    for k in range(n + 1):
        u = k / n
        x = pole[0] + trail * length * u
        wave = 0.11 * math.sin(2 * math.pi * u * 1.1 + phase) * u
        y = pole[1] + wave
        h = height * (1 - 0.1 * u)
        z1 = top - 0.06 * u * u
        z0 = z1 - h
        t = 0.006
        rings.append([(x, y - t, z0), (x, y + t, z0), (x, y + t, z1), (x, y - t, z1)])
    p.loft(m, rings, bevel=0.0, smooth=True)


def sheet(p, m, corners, nx, ny, sag=0.0, z_fn=None, thick=0.02):
    """Um pano/rede esticado entre 4 cantos (x, y, z) no sentido horário, com a barriga no meio (sag) ou uma
    função z_fn(u, v, z_plano) própria; dupla face (uma camada em cima e outra embaixo, com espessura)."""
    (x0, y0, z0), (x1, y1, z1), (x2, y2, z2), (x3, y3, z3) = corners
    rows = []
    for j in range(ny + 1):
        v = j / ny
        row = []
        for i in range(nx + 1):
            u = i / nx
            # bilinear nos cantos: c0 (u=0,v=0), c1 (u=1,v=0), c2 (u=1,v=1), c3 (u=0,v=1)
            x = (1 - u) * (1 - v) * x0 + u * (1 - v) * x1 + u * v * x2 + (1 - u) * v * x3
            y = (1 - u) * (1 - v) * y0 + u * (1 - v) * y1 + u * v * y2 + (1 - u) * v * y3
            z = (1 - u) * (1 - v) * z0 + u * (1 - v) * z1 + u * v * z2 + (1 - u) * v * z3
            z -= sag * math.sin(math.pi * u) * math.sin(math.pi * v)
            if z_fn:
                z = z_fn(u, v, z)
            row.append((x, y, z))
        rows.append(row)
    b = p.bm(m, 0.0)
    top = [[b.verts.new(V(x, y, z + thick / 2)) for (x, y, z) in row] for row in rows]
    bot = [[b.verts.new(V(x, y, z - thick / 2)) for (x, y, z) in row] for row in rows]
    for j in range(ny):
        for i in range(nx):
            f = b.faces.new([top[j][i], top[j][i + 1], top[j + 1][i + 1], top[j + 1][i]])
            f.smooth = True
            f = b.faces.new([bot[j + 1][i], bot[j + 1][i + 1], bot[j][i + 1], bot[j][i]])
            f.smooth = True
    # as bordas (a espessura)
    edge = [(0, i) for i in range(nx)] + [(j, nx) for j in range(ny)] + [(ny, i) for i in range(nx, 0, -1)] + [(j, 0) for j in range(ny, 0, -1)]
    for (j0, i0), (j1, i1) in zip(edge, edge[1:] + edge[:1]):
        b.faces.new([top[j0][i0], bot[j0][i0], bot[j1][i1], top[j1][i1]])


def track_path(circles, pitch):
    """O contorno de uma esteira: o envoltório convexo das rodas [(x, z, raio)...], reamostrado a cada elo (pitch).
    Devolve [((x, z), ângulo da tangente)...]."""
    pts = []
    for cx, cz, r in circles:
        for k in range(64):
            a = 2 * math.pi * k / 64
            pts.append((cx + r * math.cos(a), cz + r * math.sin(a)))
    pts = sorted(set(pts))

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower, upper = [], []
    for q in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], q) <= 0:
            lower.pop()
        lower.append(q)
    for q in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], q) <= 0:
            upper.pop()
        upper.append(q)
    hull_ = lower[:-1] + upper[:-1]
    seg = list(zip(hull_, hull_[1:] + hull_[:1]))
    total = sum(math.dist(a, b) for a, b in seg)
    n = int(total / pitch)
    step = total / n
    out, t = [], 0.0
    for a, b in seg:
        L = math.dist(a, b)
        while t <= L:
            u = t / L
            out.append(((a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u), math.atan2(b[1] - a[1], b[0] - a[0])))
            t += step
        t -= L
    return out
