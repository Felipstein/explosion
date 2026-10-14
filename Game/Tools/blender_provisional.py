"""Os modelos PROVISÓRIOS da base (até a arte final): War Factory, Command Center, Usina,
Centro de Suprimentos, depósito, Dozer, caminhão e a caixa de suprimento.

Uso: blender -b --factory-startup -P Game/Tools/blender_provisional.py -- <saida_dir>
(PROV_BUILD=debris: só os destroços)
(o game.ps1 models chama isto e depois importa na Unreal).

Regras (docs/ECONOMIA.md, o estudo de arte de 10/10 e a memória "modelar"):
- quina dura com chanfro de 8 a 15 cm + normais "hardened" e ponderadas: as faces grandes
  ficam chapadas e a quina pega luz (o que lê de cima, a 37,5°, a ~20 px/m);
- NUNCA remesh, suavizar ou deformar com ruído (a casa "massa de modelar" de 08/10);
- a assinatura de cada prédio no telhado, que é o que a câmera do RTS mais vê;
- tudo em metros, X = frente (o lado da porta, o "Dir 0" da simulação), Z = cima, origem no
  chão no centro do lote; o tamanho de cada lote é o do ZH (Geometry × 2 × 0,2633);
- os materiais vão só com o nome (Body, Trim, Dark, Yellow, Player, Concrete, Hazard, Glass,
  Light, Core, Beacon, Crate, Rubber, Paint): a Unreal troca pelos de verdade (build_content.py),
  e "Player" é a cor do jogador (faixas e lonas, nunca o prédio inteiro: CONCEITO_CONSTRUCOES.md).
As peças que mexem (ventilador, antena, lâmina, rodas) saem em arquivos próprios, com a
origem no pivô; o Units.json diz onde cada uma vai.
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

OUT = sys.argv[sys.argv.index('--') + 1]
os.makedirs(OUT, exist_ok=True)

# cor só pra pré-visualização no Blender (a Unreal usa os materiais dela)
PREVIEW = {
    'Body': (0.58, 0.57, 0.54, 0.55, 0.0),
    'Trim': (0.22, 0.22, 0.22, 0.50, 0.3),
    'Dark': (0.05, 0.05, 0.05, 0.45, 0.6),
    'Yellow': (0.80, 0.42, 0.02, 0.50, 0.0),
    'Player': (0.05, 0.16, 0.55, 0.65, 0.0),
    'Concrete': (0.40, 0.39, 0.36, 0.85, 0.0),
    'Hazard': (0.80, 0.55, 0.02, 0.60, 0.0),
    'Glass': (0.02, 0.025, 0.03, 0.08, 0.0),
    'Light': (1.0, 0.75, 0.40, 0.40, 0.0),
    'Core': (1.0, 0.93, 0.80, 0.30, 0.0),
    'Beacon': (1.0, 0.45, 0.05, 0.40, 0.0),
    'Crate': (0.15, 0.14, 0.05, 0.75, 0.0),
    'Rubber': (0.02, 0.02, 0.02, 0.90, 0.0),
    'Paint': (0.75, 0.75, 0.72, 0.60, 0.0),
}


def material(name):
    m = bpy.data.materials.get(name)
    if m is None:
        m = bpy.data.materials.new(name)
        r, g, b, rough, metal = PREVIEW[name]
        m.use_nodes = True
        bsdf = m.node_tree.nodes.get('Principled BSDF')
        if bsdf:
            bsdf.inputs['Base Color'].default_value = (r, g, b, 1.0)
            bsdf.inputs['Roughness'].default_value = rough
            bsdf.inputs['Metallic'].default_value = metal
    return m


class Part:
    """Um grupo de peças que vira uma malha só (ou uma peça que mexe, num arquivo próprio)."""

    def __init__(self, name):
        self.name = name
        self.objects = []

    # ------------------------------------------------------------ primitivas
    def _object(self, bm, mat, bevel, segments=2, angle=30.0):
        mesh = bpy.data.meshes.new(f'{self.name}_{len(self.objects)}')
        bm.to_mesh(mesh)
        bm.free()
        ob = bpy.data.objects.new(mesh.name, mesh)
        bpy.context.scene.collection.objects.link(ob)
        ob.data.materials.append(material(mat))
        for p in ob.data.polygons:
            p.use_smooth = True
        if bevel > 0.0:
            b = ob.modifiers.new('bevel', 'BEVEL')
            b.width = bevel
            b.segments = segments
            b.limit_method = 'ANGLE'
            b.angle_limit = math.radians(angle)
            b.harden_normals = True
            b.use_clamp_overlap = True
        w = ob.modifiers.new('wn', 'WEIGHTED_NORMAL')
        w.keep_sharp = True
        w.weight = 50
        self.objects.append(ob)
        return ob

    def box(self, x0, y0, z0, x1, y1, z1, mat, bevel=0.10):
        """Caixa pelos cantos (metros). O chanfro nunca passa de 1/3 do lado menor."""
        sx, sy, sz = abs(x1 - x0), abs(y1 - y0), abs(z1 - z0)
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0)
        bmesh.ops.scale(bm, vec=Vector((sx, sy, sz)), verts=bm.verts)
        bmesh.ops.translate(bm, vec=Vector(((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)), verts=bm.verts)
        ob = self._object(bm, mat, min(bevel, min(sx, sy, sz) / 3.0))
        ob['aabb'] = (min(x0, x1), min(y0, y1), min(z0, z1), max(x0, x1), max(y0, y1), max(z0, z1))
        return ob

    def cyl(self, x, y, z0, r, h, mat, seg=32, bevel=0.06, axis='Z', r2=None):
        """Cilindro (ou cone, com r2) de pé em (x, y, z0); axis X ou Y deita ele."""
        bm = bmesh.new()
        bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=seg, radius1=r, radius2=r if r2 is None else r2, depth=h)
        rot = Matrix.Identity(4)
        if axis == 'X':
            rot = Matrix.Rotation(math.radians(90), 4, 'Y')
        elif axis == 'Y':
            rot = Matrix.Rotation(math.radians(90), 4, 'X')
        bmesh.ops.transform(bm, matrix=rot, verts=bm.verts)
        off = Vector((x, y, z0 + h / 2)) if axis == 'Z' else Vector((x, y, z0))
        bmesh.ops.translate(bm, vec=off, verts=bm.verts)
        return self._object(bm, mat, min(bevel, r / 3.0, h / 3.0))

    def sphere(self, x, y, z, r, mat, seg=32, z_cut=None):
        bm = bmesh.new()
        bmesh.ops.create_uvsphere(bm, u_segments=seg, v_segments=seg // 2, radius=r)
        if z_cut is not None:
            bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < z_cut - 1e-4], context='VERTS')
        bmesh.ops.translate(bm, vec=Vector((x, y, z)), verts=bm.verts)
        return self._object(bm, mat, 0.0)

    def pipe(self, p0, p1, r, mat, seg=12):
        """Cano de p0 a p1."""
        a, b = Vector(p0), Vector(p1)
        d = b - a
        L = d.length
        bm = bmesh.new()
        bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=seg, radius1=r, radius2=r, depth=L)
        rot = d.to_track_quat('Z', 'Y').to_matrix().to_4x4()
        bmesh.ops.transform(bm, matrix=rot, verts=bm.verts)
        bmesh.ops.translate(bm, vec=(a + b) / 2, verts=bm.verts)
        return self._object(bm, mat, min(0.02, r / 3.0))

    def rail(self, pts, z0, height, mat='Yellow', r=0.045, post_every=2.0):
        """Guarda-corpo: postes e o tubo de cima ao longo da linha."""
        for i in range(len(pts) - 1):
            a, b = Vector((*pts[i], z0)), Vector((*pts[i + 1], z0))
            self.pipe(a + Vector((0, 0, height)), b + Vector((0, 0, height)), r, mat)
            self.pipe(a + Vector((0, 0, height * 0.5)), b + Vector((0, 0, height * 0.5)), r * 0.8, mat)
            n = max(1, int((b - a).length / post_every))
            for k in range(n + 1):
                p = a.lerp(b, k / n)
                self.pipe(p, p + Vector((0, 0, height)), r, mat)

    def lattice(self, x, y, z0, half, h, mat='Trim', r=0.09, levels=4):
        """Torre treliçada de 4 pernas, com travessas em X (a do domo e a do radar)."""
        c = [Vector((x + sx * half, y + sy * half, z0)) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        for p in c:
            self.pipe(p, p + Vector((0, 0, h)), r, mat)
        for lv in range(levels):
            za, zb = z0 + h * lv / levels, z0 + h * (lv + 1) / levels
            for i in range(4):
                a, b = c[i], c[(i + 1) % 4]
                self.pipe(Vector((a.x, a.y, za)), Vector((b.x, b.y, zb)), r * 0.6, mat)
                self.pipe(Vector((a.x, a.y, zb)), Vector((b.x, b.y, zb)), r * 0.7, mat)

    def light(self, x, y, z, s=0.25, mat='Light'):
        """Luz de serviço: caixinha acesa com o suporte escuro."""
        self.box(x - s, y - s, z - s * 0.6, x + s, y + s, z + s * 0.6, mat, 0.03)

    def vent_grille(self, x0, y0, z, x1, y1, mat='Dark', slats=6, along='X'):
        """Grade de ventilação: lâminas escuras sobre um quadro."""
        self.box(x0, y0, z, x1, y1, z + 0.06, 'Trim', 0.02)
        for k in range(slats):
            t = (k + 0.5) / slats
            if along == 'X':
                xa = x0 + (x1 - x0) * t
                self.box(xa - 0.06, y0 + 0.1, z + 0.06, xa + 0.06, y1 - 0.1, z + 0.22, mat, 0.02)
            else:
                ya = y0 + (y1 - y0) * t
                self.box(x0 + 0.1, ya - 0.06, z + 0.06, x1 - 0.1, ya + 0.06, z + 0.22, mat, 0.02)

    def panels(self, x0, y0, z0, x1, y1, z1, face, step=2.4, depth=0.06, mat='Body'):
        """Placas de parede: faixas levemente saltadas numa face (+X, -X, +Y, -Y): dão a
        leitura de "chapa de metal" sem textura (o detalhe que faz sombra de verdade)."""
        if face in ('+X', '-X'):
            x = x1 if face == '+X' else x0
            s = 1 if face == '+X' else -1
            n = max(1, int(abs(y1 - y0) / step))
            for k in range(n):
                ya = y0 + (y1 - y0) * k / n + 0.08
                yb = y0 + (y1 - y0) * (k + 1) / n - 0.08
                self.box(x, ya, z0 + 0.15, x + s * depth, yb, z1 - 0.15, mat, 0.03)
        else:
            y = y1 if face == '+Y' else y0
            s = 1 if face == '+Y' else -1
            n = max(1, int(abs(x1 - x0) / step))
            for k in range(n):
                xa = x0 + (x1 - x0) * k / n + 0.08
                xb = x0 + (x1 - x0) * (k + 1) / n - 0.08
                self.box(xa, y, z0 + 0.15, xb, y + s * depth, z1 - 0.15, mat, 0.03)

    def band(self, x0, y0, x1, y1, z0, z1, out=0.10, mat='Player'):
        """Faixa em volta de um bloco (a cor do jogador nas paredes). Sai 10 cm, por fora das chapas
        (6 cm); os lados ±X ficam entre os ±Y, sem sobrepor nos cantos."""
        inner = 0.04
        self.box(x0 - out, y0 - out, z0, x1 + out, y0 + inner, z1, mat, 0.02)
        self.box(x0 - out, y1 - inner, z0, x1 + out, y1 + out, z1, mat, 0.02)
        self.box(x0 - out, y0 + inner, z0, x0 + inner, y1 - inner, z1, mat, 0.02)
        self.box(x1 - inner, y0 + inner, z0, x1 + out, y1 - inner, z1, mat, 0.02)

    def tarp(self, x0, y0, x1, y1, z, mat='Player'):
        """Lona amarrada no teto: o volume baixo e as cintas escuras (a cor do jogador de cima)."""
        self.box(x0, y0, z, x1, y1, z + 0.35, mat, 0.12)
        for k in range(3):
            t = (k + 1) / 4
            if abs(x1 - x0) > abs(y1 - y0):
                xa = x0 + (x1 - x0) * t
                self.box(xa - 0.06, y0 - 0.05, z, xa + 0.06, y1 + 0.05, z + 0.42, 'Dark', 0.02)
            else:
                ya = y0 + (y1 - y0) * t
                self.box(x0 - 0.05, ya - 0.06, z, x1 + 0.05, ya + 0.06, z + 0.42, 'Dark', 0.02)

    def hazard_frame(self, x0, y0, x1, y1, w=0.45, z=0.26):
        """Moldura de faixa de perigo pintada no piso (3 cm acima da laje)."""
        top = z + 0.04
        self.box(x0, y0, z, x1, y0 + w, top, 'Hazard', 0.0)
        self.box(x0, y1 - w, z, x1, y1, top, 'Hazard', 0.0)
        self.box(x0, y0 + w, z, x0 + w, y1 - w, top, 'Hazard', 0.0)
        self.box(x1 - w, y0 + w, z, x1, y1 - w, top, 'Hazard', 0.0)

    def lamp_post(self, x, y, h=6.0):
        self.cyl(x, y, 0.25, 0.12, h, 'Trim', 12)
        self.box(x - 0.35, y - 0.25, h + 0.1, x + 0.35, y + 0.25, h + 0.45, 'Dark', 0.04)
        self.box(x - 0.28, y - 0.18, h, x + 0.28, y + 0.18, h + 0.12, 'Light', 0.02)
        self.box(x - 0.35, y - 0.35, 0.25, x + 0.35, y + 0.35, 0.95, 'Hazard', 0.05)

    def flag(self, x, y, h=14.0):
        """Mastro com a bandeira (na cor do jogador: o pano é o que mais se vê de cima)."""
        self.cyl(x, y, 0.25, 0.16, h, 'Trim', 12)
        self.sphere(x, y, h + 0.35, 0.22, 'Paint', 12)
        self.box(x + 0.15, y - 0.03, h - 2.2, x + 3.6, y + 0.03, h - 0.1, 'Player', 0.01)
        self.box(x - 0.6, y - 0.6, 0.25, x + 0.6, y + 0.6, 0.6, 'Concrete', 0.08)

    def ring(self, x, y, z0, r_out, r_in, h, mat, seg=40, bevel=0.05):
        """Anel (a carcaça do ventilador): oco no meio, pra ver as pás de cima."""
        bm = bmesh.new()
        rings = []
        for rr, zz in ((r_out, 0.0), (r_out, h), (r_in, h), (r_in, 0.0)):
            rings.append([bm.verts.new((x + rr * math.cos(2 * math.pi * k / seg), y + rr * math.sin(2 * math.pi * k / seg), z0 + zz))
                          for k in range(seg)])
        for a in range(4):
            ra, rb = rings[a], rings[(a + 1) % 4]
            for k in range(seg):
                bm.faces.new((ra[k], ra[(k + 1) % seg], rb[(k + 1) % seg], rb[k]))
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        return self._object(bm, mat, min(bevel, (r_out - r_in) / 3.0, h / 3.0))

    # ------------------------------------------------------------ saída
    def check_coplanar(self):
        """Duas caixas com uma face no mesmo plano, virada pro mesmo lado e se sobrepondo: a placa
        não sabe qual desenhar e alterna a cada quadro (a linha preta piscando do portão e do anexo
        da War Factory, 10/10). Também acusa as quase coladas (menos de 2 cm), que piscam de longe.
        Erro, não aviso: o modelo não sai assim."""
        boxes = []
        for ob in self.objects:
            if 'aabb' in ob and ob.matrix_world == Matrix.Identity(4):
                boxes.append((ob, tuple(ob['aabb'])))
        problems = []
        for i in range(len(boxes)):
            a_ob, a = boxes[i]
            for j in range(i + 1, len(boxes)):
                b_ob, b = boxes[j]
                for axis in range(3):
                    u, v = [k for k in range(3) if k != axis]
                    # sobreposição na face (as outras duas direções)
                    du = min(a[u + 3], b[u + 3]) - max(a[u], b[u])
                    dv = min(a[v + 3], b[v + 3]) - max(a[v], b[v])
                    if du <= 0.01 or dv <= 0.01 or du * dv < 0.01:
                        continue
                    for side in (0, 3):
                        if axis == 2 and side == 0:
                            continue   # face de baixo: nunca aparece
                        gap = abs(a[axis + side] - b[axis + side])
                        if gap >= 0.015:
                            continue
                        # a sobreposição fica dentro de outra peça (ou encostada nela)? então não aparece
                        plane = a[axis + side]
                        lo_u, hi_u = max(a[u], b[u]), min(a[u + 3], b[u + 3])
                        lo_v, hi_v = max(a[v], b[v]), min(a[v + 3], b[v + 3])
                        hidden = False
                        for c_ob, c in boxes:
                            if c_ob is a_ob or c_ob is b_ob:
                                continue
                            if c[axis] - 1e-4 <= plane <= c[axis + 3] + 1e-4 and c[u] <= lo_u + 1e-4 and c[u + 3] >= hi_u - 1e-4 \
                                    and c[v] <= lo_v + 1e-4 and c[v + 3] >= hi_v - 1e-4:
                                hidden = True
                                break
                        if not hidden:
                            problems.append(f'{self.name}: {a_ob.data.materials[0].name} x {b_ob.data.materials[0].name} '
                                            f'na face {"-+"[side // 3]}{"XYZ"[axis]} = {a[axis + side]:.3f} (afastadas {gap * 100:.1f} cm, '
                                            f'sobreposição {du:.2f} x {dv:.2f} m)')
        return problems

    def bake(self):
        """Aplica o chanfro e as normais e junta tudo num objeto só (com o nome da peça)."""
        problems = self.check_coplanar()
        if problems and os.environ.get('PROV_ZFIGHT_LIST'):
            print('ZFIGHT ' + '\nZFIGHT '.join(problems))
        elif problems:
            raise RuntimeError('superfícies coladas no mesmo plano (piscam no jogo):\n  ' + '\n  '.join(problems))
        bpy.ops.object.select_all(action='DESELECT')
        for ob in self.objects:
            ob.select_set(True)
        bpy.context.view_layer.objects.active = self.objects[0]
        bpy.ops.object.convert(target='MESH')
        if len(self.objects) > 1:
            bpy.ops.object.join()
        ob = bpy.context.view_layer.objects.active
        ob.name = self.name
        ob.data.name = self.name
        self.objects = []
        return ob

    def export(self, filename):
        if self.objects:
            export_parts(filename, [self])


def export_parts(filename, parts):
    """Várias peças num GLB só, cada uma uma malha com o nome dela (a Unreal cria uma malha por
    peça: é assim que as rodas do caminhão viram wheel_001…, como as do tanque)."""
    obs = [part.bake() for part in parts if part.objects]
    path = os.path.join(OUT, filename)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.object.select_all(action='DESELECT')
    for ob in obs:
        ob.select_set(True)
    bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_selection=True, export_apply=True,
                              export_yup=True, export_materials='EXPORT')
    tris = sum(len(f.vertices) - 2 for ob in obs for f in ob.data.polygons)
    mats = {m.name for ob in obs for m in ob.data.materials}
    print(f'PROV {filename}: {len(obs)} malhas, {tris} triângulos, {len(mats)} materiais')
    for ob in obs:
        bpy.data.objects.remove(ob, do_unlink=True)


def scaffold(name, x0, y0, x1, y1, h, gap=0.9, bay=3.0, lift=2.0):
    """O andaime da obra em volta do bloco principal (some quando o prédio fica pronto):
    tubos galvanizados, tábuas em cada nível e o rodapé de perigo. É o que diz "em obra" de
    cima, junto com a parede subindo (o ZH sobe o prédio do chão; aqui ele cresce por dentro)."""
    s = Part(name)
    X0, Y0, X1, Y1 = x0 - gap, y0 - gap, x1 + gap, y1 + gap
    z0, top = 0.27, h + 1.0
    levels = max(2, int((top - z0) / lift))
    perim = [(X0, Y0), (X1, Y0), (X1, Y1), (X0, Y1)]
    outward = [(0.0, -1.0), (1.0, 0.0), (0.0, 1.0), (-1.0, 0.0)]
    for i in range(4):
        (ax, ay), (bx, by) = perim[i], perim[(i + 1) % 4]
        ox, oy = outward[i]
        L = math.hypot(bx - ax, by - ay)
        n = max(1, int(L / bay))
        # os postes: a linha de dentro e a de fora (1 m entre elas)
        for k in range(n + 1):
            t = k / n
            px, py = ax + (bx - ax) * t, ay + (by - ay) * t
            for d in (0.0, 1.0):
                s.pipe((px + ox * d, py + oy * d, z0), (px + ox * d, py + oy * d, top), 0.05, 'Trim', 8)
        for lv in range(1, levels + 1):
            z = z0 + (top - z0) * lv / levels
            for d in (0.0, 1.0):
                s.pipe((ax + ox * d, ay + oy * d, z), (bx + ox * d, by + oy * d, z), 0.045, 'Trim', 8)
            # a tábua do nível e o rodapé amarelo do lado de fora
            if i in (0, 2):
                yy = ay + oy * 0.5
                s.box(min(ax, bx), yy - 0.45, z - 0.06, max(ax, bx), yy + 0.45, z, 'Crate', 0.02)
                s.box(min(ax, bx), ay + oy - 0.02, z, max(ax, bx), ay + oy + 0.02, z + 0.18, 'Hazard', 0.0)
            else:
                xx = ax + ox * 0.5
                s.box(xx - 0.45, min(ay, by), z - 0.06, xx + 0.45, max(ay, by), z, 'Crate', 0.02)
                s.box(ax + ox - 0.02, min(ay, by), z, ax + ox + 0.02, max(ay, by), z + 0.18, 'Hazard', 0.0)
        # a diagonal de travamento na linha de fora, um nível por vão
        for k in range(n):
            t0, t1 = k / n, (k + 1) / n
            pa = (ax + (bx - ax) * t0 + ox, ay + (by - ay) * t0 + oy)
            pb = (ax + (bx - ax) * t1 + ox, ay + (by - ay) * t1 + oy)
            zl = z0 + (top - z0) * ((k % levels) / levels)
            zh = z0 + (top - z0) * ((k % levels + 1) / levels)
            s.pipe((pa[0], pa[1], zl), (pb[0], pb[1], zh), 0.035, 'Trim', 6)
    return s


def pad(p, hx, hy, h=0.25):
    """A laje de concreto do lote, com a borda mais escura (a "saia" desce 3 m: o ZH deixa
    construir com até 2,6 m de desnível, e o chão não aplaina no jogo)."""
    p.box(-hx, -hy, -3.0, hx, hy, h - 0.02, 'Concrete', 0.12)
    p.box(-hx - 0.05, -hy - 0.05, h - 0.1, hx + 0.05, hy + 0.05, h, 'Trim', 0.04)
    p.box(-hx + 0.3, -hy + 0.3, h, hx - 0.3, hy - 0.3, h + 0.02, 'Concrete', 0.0)


# ====================================================================== War Factory
def war_factory():
    """28 × 32 m (ZH 27,9 × 31,6), corpo ≤ 10,5 m. A arte aprovada (usa_war_factory.webp):
    galpão com o vão da porta e o pórtico amarelo, 2 ventiladores no teto, domo na torre
    treliçada, chaminés gêmeas com vapor, o pátio do braço robótico com faixa de perigo."""
    hx, hy = 13.95, 15.8
    p = Part('war_factory')
    pad(p, hx, hy)
    z = 0.27
    # o galpão: fundo X −11..+5, a moldura da porta X +5..+8, vão de 12 × 7 m
    p.box(-11.0, -9.0, z, 5.0, 9.0, 9.0, 'Body', 0.15)
    p.panels(-11.0, -9.0, z, 5.0, 9.0, 9.0, '+Y')
    p.panels(-11.0, -9.0, z, 5.0, 9.0, 9.0, '-Y')
    p.panels(-11.0, -9.0, z, 5.0, 9.0, 9.0, '-X')
    p.box(5.0, -9.0, z, 8.2, -6.0, 9.4, 'Body', 0.15)
    p.box(5.0, 6.0, z, 8.2, 9.0, 9.4, 'Body', 0.15)
    p.box(5.0, -6.0, 7.2, 8.2, 6.0, 9.4, 'Body', 0.15)
    p.box(4.6, -6.0, z, 5.03, 6.0, 7.2, 'Dark', 0.02)      # o fundo escuro do vão (3 cm à frente da parede)
    p.box(5.0, -6.0, z, 8.2, 6.0, z + 0.03, 'Trim', 0.0)  # o piso do vão
    for k in range(5):                                     # luzes do teto do vão
        y = -4.8 + k * 2.4
        p.light(6.6, y, 7.05, 0.22)
    # trilhos no piso do vão até a frente do lote
    for y in (-2.2, 2.2):
        p.box(5.0, y - 0.18, z + 0.02, hx - 0.4, y + 0.18, z + 0.06, 'Dark', 0.02)
    # o pórtico amarelo na frente da porta (a assinatura de quem chega), com o trole e os ganchos
    for y in (-7.4, 7.4):
        p.box(8.3, y - 0.55, z, 9.4, y + 0.55, 10.0, 'Yellow', 0.10)
        p.box(8.1, y - 0.75, z, 9.6, y + 0.75, z + 0.9, 'Hazard', 0.06)
    p.box(8.33, -7.92, 9.0, 9.37, 7.92, 9.97, 'Yellow', 0.10)
    p.box(8.0, -1.0, 8.5, 9.7, 1.0, 9.0, 'Dark', 0.05)
    p.pipe((8.85, -0.4, 8.5), (8.85, -0.4, 6.4), 0.04, 'Dark')
    p.pipe((8.85, 0.4, 8.5), (8.85, 0.4, 6.4), 0.04, 'Dark')
    p.box(8.6, -0.6, 6.0, 9.1, 0.6, 6.4, 'Yellow', 0.04)
    for y in (-7.4, 7.4):
        p.light(9.65, y, 6.0, 0.18)
    # o telhado: os 2 ventiladores (giram: arquivo próprio), caixas de máquina, lonas, guarda-corpo
    for y in (-3.4, 3.4):
        p.ring(-3.5, y, 9.0, 3.05, 2.7, 0.85, 'Trim', 48, 0.08)
        p.cyl(-3.5, y, 9.0, 2.7, 0.12, 'Dark', 40, 0.0)
        for a in (0.0, math.pi / 2):     # a grade de proteção em cruz
            ob = p.box(-2.7, -0.05, 9.78, 2.7, 0.05, 9.86, 'Trim', 0.02)
            ob.matrix_world = Matrix.Translation((-3.5, y, 0)) @ Matrix.Rotation(a + math.pi / 4, 4, 'Z')
    p.box(-10.6, -8.6, 9.0, -7.0, -3.6, 10.1, 'Body', 0.10)
    p.vent_grille(-10.3, -8.3, 10.1, -7.3, -3.9, slats=7)
    p.box(-10.6, 3.6, 9.0, -7.0, 8.6, 10.1, 'Body', 0.10)
    p.vent_grille(-10.3, 3.9, 10.1, -7.3, 8.3, slats=7)
    p.tarp(1.2, -8.4, 4.6, -4.0, 9.0)
    p.tarp(1.2, 4.0, 4.6, 8.4, 9.0)
    p.rail([(-11.0, -9.0), (5.0, -9.0), (5.0, 9.0), (-11.0, 9.0), (-11.0, -9.0)], 9.0, 1.0)
    # a faixa da cor do jogador nas paredes (e na moldura da porta)
    p.band(-11.0, -9.0, 5.0, 9.0, 5.6, 6.3)
    p.box(8.15, -9.05, 5.6, 8.3, -5.95, 6.3, 'Player', 0.01)
    p.box(8.15, 5.95, 5.6, 8.3, 9.05, 6.3, 'Player', 0.01)
    # anexo da esquerda (−Y), no fundo: chaminés gêmeas (o vapor branco sai daqui) e o maquinário
    p.box(-13.5, -15.4, z, -5.0, -9.2, 5.5, 'Body', 0.12)
    p.panels(-13.5, -15.4, z, -5.0, -9.2, 5.5, '-Y')
    for x in (-11.4, -8.6):
        p.cyl(x, -12.6, 5.5, 1.15, 6.8, 'Body', 32, 0.06)
        p.cyl(x, -12.6, 12.3, 1.25, 0.35, 'Trim', 32, 0.04)
        p.cyl(x, -12.6, 12.4, 0.9, 0.3, 'Dark', 24, 0.02)
        p.pipe((x, -11.4, 6.0), (x, -9.2, 6.0), 0.18, 'Trim')
    p.rail([(-13.5, -15.4), (-5.0, -15.4), (-5.0, -9.2)], 5.5, 1.0)
    # o domo na torre treliçada (fundo, lado +Y)
    p.box(-13.6, 9.4, z, -6.4, 15.5, 1.0, 'Concrete', 0.08)
    p.lattice(-10.0, 12.4, 1.0, 2.0, 5.2)
    p.box(-12.4, 10.0, 6.1, -7.6, 14.8, 6.4, 'Trim', 0.05)
    p.sphere(-10.0, 12.4, 8.6, 2.6, 'Body', 40)
    p.box(-10.2, 10.0, 8.4, -9.8, 14.8, 8.8, 'Trim', 0.02)   # a nervura do domo
    # o pátio de reparo (frente, −Y): faixa de perigo e o braço robótico parado
    p.hazard_frame(-1.5, -15.3, 12.8, -9.6)
    p.cyl(5.6, -12.4, z, 1.1, 0.9, 'Dark', 24, 0.05)
    p.cyl(5.6, -12.4, z + 0.9, 0.7, 1.6, 'Body', 24, 0.05)
    p.box(5.0, -12.9, 2.6, 8.8, -11.9, 3.4, 'Body', 0.10)
    p.box(8.4, -12.8, 1.4, 9.2, -12.0, 3.2, 'Body', 0.08)
    p.cyl(8.8, -12.4, 1.0, 0.25, 0.4, 'Dark', 12, 0.02)
    p.pipe((5.6, -11.7, 2.0), (8.6, -11.8, 3.0), 0.08, 'Player')
    p.box(10.6, -14.6, z, 12.2, -13.2, 1.3, 'Crate', 0.06)
    # anexo da direita (+Y), na frente: escritório com janelas, lona no teto, a bandeira
    p.box(-4.0, 9.4, z, 9.8, 15.4, 5.0, 'Body', 0.12)
    p.panels(-4.0, 9.4, z, 9.8, 15.4, 5.0, '+Y')
    p.box(9.8, 10.2, 1.8, 9.86, 14.6, 3.6, 'Glass', 0.01)
    p.box(-3.6, 15.4, 2.6, 9.4, 15.5, 3.4, 'Glass', 0.01)
    p.band(-4.0, 9.4, 9.8, 15.4, 4.0, 4.5)
    p.tarp(-2.4, 10.6, 4.4, 14.4, 5.0)
    p.box(5.6, 11.0, 5.0, 8.6, 13.6, 5.9, 'Trim', 0.06)
    p.flag(12.6, 13.6, 14.0)
    # postes de luz nos cantos da frente e as balizas da porta
    p.lamp_post(13.2, -15.0)
    p.lamp_post(13.2, 8.6)
    for y in (-8.6, 8.6):
        p.box(10.4, y - 0.4, z, 11.2, y + 0.4, 1.1, 'Hazard', 0.05)
    p.export('war_factory/war_factory.glb')
    scaffold('war_factory_scaffold', -11.0, -9.0, 8.2, 9.0, 9.4).export('war_factory/war_factory_scaffold.glb')

    fan = Part('war_factory_fan')
    # o ventilador: 7 pás em X, o cubo no meio; a origem é o eixo (gira no Z)
    fan.cyl(0, 0, 0.0, 0.45, 0.35, 'Trim', 16, 0.03)
    for k in range(7):
        a = 2 * math.pi * k / 7
        d = Vector((math.cos(a), math.sin(a), 0))
        side = Vector((-d.y, d.x, 0))
        ob = fan.box(-0.22, 0.4, 0.05, 0.22, 2.55, 0.12, 'Paint', 0.03)
        ob.matrix_world = Matrix.Rotation(a - math.pi / 2, 4, 'Z') @ Matrix.Rotation(math.radians(18), 4, 'Y')
    fan.export('war_factory/war_factory_fan.glb')


# ====================================================================== Command Center
def command_center():
    """32 × 37 m (ZH 31,6 × 36,9), corpo ≤ 12 m. Assinatura: a antena de radar girando numa
    torre treliçada (o radar vem com o CC, ZH) + a bandeira; garagem do Dozer com pórtico
    amarelo na frente; andar de comando com a faixa de vidro escuro."""
    hx, hy = 15.8, 18.45
    p = Part('command_center')
    pad(p, hx, hy)
    z = 0.27
    # o bloco baixo e largo, com placas
    p.box(-14.6, -16.6, z, 6.0, 16.6, 6.6, 'Body', 0.15)
    for face in ('+Y', '-Y', '-X'):
        p.panels(-14.6, -16.6, z, 6.0, 16.6, 6.6, face)
    p.band(-14.6, -16.6, 6.0, 16.6, 4.4, 5.0)
    # a garagem do Dozer na frente, com o pórtico amarelo e a faixa de perigo no pátio
    p.box(6.0, -16.6, z, 8.4, -5.6, 6.6, 'Body', 0.15)
    p.box(6.0, 5.6, z, 8.4, 16.6, 6.6, 'Body', 0.15)
    p.box(6.0, -5.6, 5.0, 8.4, 5.6, 6.6, 'Body', 0.15)
    p.box(5.6, -5.6, z, 6.03, 5.6, 4.97, 'Dark', 0.02)
    for y in (-5.9, 5.9):
        p.box(8.4, y - 0.5, z, 9.4, y + 0.5, 7.2, 'Yellow', 0.10)
    p.box(8.43, -6.37, 6.4, 9.37, 6.37, 7.17, 'Yellow', 0.10)
    for k in range(4):
        p.light(7.4, -4.2 + k * 2.8, 4.85, 0.2)
    p.hazard_frame(9.6, -6.0, hx - 0.6, 6.0)
    # janelas da frente (salas de cada lado da garagem)
    for y0, y1 in ((-15.6, -7.0), (7.0, 15.6)):
        p.box(8.4, y0, 2.4, 8.46, y1, 3.6, 'Glass', 0.01)
    # o andar de comando: vidro escuro em volta, teto com antenas
    p.box(-12.0, -8.0, 6.6, 2.4, 8.0, 10.4, 'Body', 0.15)
    p.box(2.4, -8.05, 7.9, 2.46, 8.05, 9.5, 'Glass', 0.01)
    p.box(-12.0, -8.06, 7.9, 2.4, -8.0, 9.5, 'Glass', 0.01)
    p.box(-12.0, 8.0, 7.9, 2.4, 8.06, 9.5, 'Glass', 0.01)
    p.box(-12.2, -8.2, 10.4, 2.6, 8.2, 10.8, 'Trim', 0.08)
    for x, y, h in ((-1.0, -6.0, 3.4), (-1.6, -5.2, 2.4), (-10.0, 6.4, 2.8)):
        p.cyl(x, y, 10.8, 0.06, h, 'Trim', 8, 0.0)
    p.box(-9.6, -5.6, 10.8, -6.0, -1.6, 11.6, 'Body', 0.08)
    p.vent_grille(-9.3, -5.3, 11.6, -6.3, -1.9, slats=5)
    # o teto do comando: o radome de comunicação, as caixas de ar, a faixa da cor do jogador
    # e as luzes de obstáculo nos cantos (piscam, como em torre de verdade)
    p.cyl(-3.0, 4.2, 10.8, 1.5, 0.5, 'Trim', 24, 0.05)
    p.sphere(-3.0, 4.2, 11.6, 1.45, 'Paint', 32)
    for x in (-4.4, -2.0):
        p.box(x - 0.9, -6.6, 10.8, x + 0.9, -4.6, 11.7, 'Body', 0.08)
        p.cyl(x, -5.6, 11.7, 0.7, 0.12, 'Dark', 20, 0.02)
    p.box(-12.25, -8.25, 10.0, 2.65, -8.05, 10.35, 'Player', 0.02)
    p.box(-12.25, 8.05, 10.0, 2.65, 8.25, 10.35, 'Player', 0.02)
    for x, y in ((-11.8, -7.8), (2.2, -7.8), (2.2, 7.8), (-11.8, 7.8)):
        p.light(x, y, 10.95, 0.12, 'Beacon')
    # o telhado baixo: lonas da cor do jogador, maquinário, guarda-corpo
    p.tarp(-13.6, -15.6, -4.4, -10.4, 6.6)
    p.tarp(-2.4, -15.6, 4.8, -10.8, 6.6)
    p.tarp(-2.4, 10.8, 4.8, 15.6, 6.6)
    p.box(-14.0, 10.2, 6.6, -9.8, 15.8, 7.8, 'Body', 0.1)
    p.vent_grille(-13.7, 10.5, 7.8, -10.1, 15.5, slats=6)
    p.rail([(-14.6, -16.6), (6.0, -16.6), (6.0, 16.6), (-14.6, 16.6), (-14.6, -16.6)], 6.6, 1.0)
    # a torre do radar no telhado (a antena gira: arquivo próprio, pivô no topo)
    p.lattice(-6.0, 12.6, 6.6, 1.6, 5.4, levels=5)
    p.box(-8.0, 10.6, 12.0, -4.0, 14.6, 12.3, 'Trim', 0.05)
    p.rail([(-8.0, 10.6), (-4.0, 10.6), (-4.0, 14.6), (-8.0, 14.6), (-8.0, 10.6)], 12.3, 0.9)
    p.cyl(-6.0, 12.6, 12.3, 0.6, 0.6, 'Dark', 16, 0.04)
    # a bandeira e os postes
    p.flag(hx - 1.6, -hy + 1.6, 15.0)
    p.lamp_post(hx - 1.2, hy - 1.2)
    p.lamp_post(-hx + 1.2, -hy + 1.2)
    p.export('command_center/command_center.glb')
    scaffold('command_center_scaffold', -14.4, -16.4, 8.4, 16.4, 7.0, gap=0.3).export('command_center/command_center_scaffold.glb')

    dish = Part('command_center_radar')
    # a parabólica grande (a assinatura do CC de cima: o radar vem com ele no ZH): a calota de
    # 5,9 m virada pra frente e 20° pra cima, o garfo, o braço do receptor e o contrapeso.
    # A origem é o eixo (gira no Z, em cima da torre).
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=48, v_segments=24, radius=4.2)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z > -3.0], context='VERTS')
    bmesh.ops.scale(bm, vec=Vector((1.0, 1.0, 0.7)), verts=bm.verts)
    bmesh.ops.translate(bm, vec=Vector((0.0, 0.0, 2.55)), verts=bm.verts)
    bmesh.ops.transform(bm, matrix=Matrix.Rotation(math.radians(70), 4, 'Y'), verts=bm.verts)
    bmesh.ops.translate(bm, vec=Vector((0.0, 0.0, 3.2)), verts=bm.verts)
    cap = dish._object(bm, 'Paint', 0.0)
    # a calota com espessura de 8 cm: casca de um lado só some vista por dentro (o Felipe viu: de
    # frente sobrava só a placa de trás, 10/10)
    solid = cap.modifiers.new('solid', 'SOLIDIFY')
    solid.thickness = 0.08
    solid.offset = -1.0
    solid.use_even_offset = True
    cap.modifiers.move(len(cap.modifiers) - 1, 0)
    nrm = Vector((math.sin(math.radians(70)), 0.0, math.cos(math.radians(70))))
    focus = Vector((0.0, 0.0, 3.2)) + nrm * 2.2
    dish.cyl(0, 0, 0.0, 0.45, 1.6, 'Trim', 16, 0.03)
    dish.box(-0.5, -1.4, 1.4, 0.5, 1.4, 1.8, 'Trim', 0.05)
    for y in (-1.25, 1.25):
        dish.box(-0.25, y - 0.15, 1.8, 0.25, y + 0.15, 3.4, 'Trim', 0.04)
    for y in (-1.2, 1.2):
        dish.pipe((0.3, y, 3.0), tuple(focus), 0.07, 'Trim')
    dish.pipe((0.3, 0.0, 1.8), tuple(focus), 0.07, 'Trim')
    dish.cyl(focus.x, focus.y, focus.z - 0.2, 0.3, 0.4, 'Dark', 12, 0.02)
    dish.light(focus.x + 0.1, 0.0, focus.z + 0.35, 0.08, 'Beacon')
    dish.box(-2.2, -0.5, 1.6, -1.0, 0.5, 2.6, 'Dark', 0.06)
    dish.export('command_center/command_center_radar.glb')


# ====================================================================== Usina
def power_plant():
    """12 × 16 m (ZH 11,6 × 15,8), ≤ 12 m. O reator de fusão fria: a única forma redonda grande,
    aletas de troca de calor em volta e o núcleo aceso no topo em branco quente (nunca neon azul:
    CONCEITO_CONSTRUCOES.md). Do lado, o bloco de controle com os isoladores."""
    hx, hy = 5.8, 7.9
    p = Part('power_plant')
    pad(p, hx, hy)
    z = 0.27
    cx, cy = -0.6, -2.4
    p.cyl(cx, cy, z, 4.0, 1.0, 'Trim', 48, 0.08)
    p.cyl(cx, cy, z + 1.0, 3.5, 7.4, 'Body', 48, 0.10)
    p.cyl(cx, cy, z + 5.2, 3.58, 0.7, 'Player', 48, 0.04)     # a faixa da cor do jogador
    for k in range(16):                                        # aletas de troca de calor
        a = 2 * math.pi * k / 16
        x, y = cx + math.cos(a) * 3.7, cy + math.sin(a) * 3.7
        ob = p.box(-0.12, -0.45, z + 1.2, 0.12, 0.45, z + 4.9, 'Trim', 0.04)
        ob.matrix_world = Matrix.Translation((x, y, 0)) @ Matrix.Rotation(a, 4, 'Z')
    p.cyl(cx, cy, z + 8.4, 3.7, 0.5, 'Trim', 48, 0.06)
    p.cyl(cx, cy, z + 8.9, 2.2, 0.35, 'Dark', 40, 0.04)
    p.cyl(cx, cy, z + 9.05, 1.75, 0.3, 'Core', 40, 0.02)      # o núcleo aceso (pulsa com a energia)
    p.rail([(cx + math.cos(a) * 3.55, cy + math.sin(a) * 3.55) for a in (2 * math.pi * k / 20 for k in range(21))],
           z + 8.9, 0.9, post_every=1.2)
    # o bloco de controle e os isoladores
    p.box(-5.4, 2.6, z, 5.4, 7.6, 4.2, 'Body', 0.12)
    p.panels(-5.4, 2.6, z, 5.4, 7.6, 4.2, '+Y')
    p.panels(-5.4, 2.6, z, 5.4, 7.6, 4.2, '+X')
    p.box(5.4, 3.4, 1.4, 5.5, 6.8, 2.6, 'Glass', 0.01)
    p.band(-5.4, 2.6, 5.4, 7.6, 3.1, 3.6)
    p.tarp(-4.6, 3.2, -0.6, 7.0, 4.2)
    for k in range(3):
        x = 0.8 + k * 1.5
        p.cyl(x, 5.0, 4.2, 0.28, 0.5, 'Dark', 12, 0.02)
        for r in range(3):
            p.cyl(x, 5.0, 4.75 + r * 0.32, 0.36, 0.14, 'Paint', 16, 0.02)
    # os canos do reator pro bloco
    for y in (0.6, 1.6):
        p.pipe((cx + 2.4, y, 2.4), (cx + 2.4, 2.6, 2.4), 0.35, 'Trim')
    p.pipe((cx - 2.2, 1.0, 3.2), (cx - 2.2, 2.6, 3.2), 0.28, 'Dark')
    # a chaminé de vapor e as luzes
    p.cyl(-4.4, 6.6, 4.2, 0.45, 2.6, 'Body', 16, 0.03)
    p.cyl(-4.4, 6.6, 6.8, 0.52, 0.2, 'Trim', 16, 0.02)
    p.light(hx - 0.4, -hy + 0.5, 1.0, 0.18)
    p.light(-hx + 0.4, -hy + 0.5, 1.0, 0.18)
    p.box(4.2, -7.4, z, 5.4, -6.2, 1.2, 'Hazard', 0.05)
    p.export('power_plant/power_plant.glb')
    scaffold('power_plant_scaffold', -4.4, -6.6, 4.4, 6.6, 9.4, gap=0.3).export('power_plant/power_plant_scaffold.glb')


# ====================================================================== Centro de suprimentos
def supply_center():
    """23 × 24 m (ZH 23,2 × 23,7), baixo (ZH 6,3 m). A baía de descarga na frente com a ponte
    rolante amarela sobre a vaga do caminhão; o H de pouso no teto (o Chinook vem depois);
    pilhas de caixas recebidas debaixo de lona da cor do jogador."""
    hx, hy = 11.6, 11.85
    p = Part('supply_center')
    pad(p, hx, hy)
    z = 0.27
    p.box(-11.0, -11.3, z, 3.6, 11.3, 6.0, 'Body', 0.15)
    for face in ('+Y', '-Y', '-X'):
        p.panels(-11.0, -11.3, z, 3.6, 11.3, 6.0, face)
    p.band(-11.0, -11.3, 3.6, 11.3, 4.2, 4.8)
    # a baía aberta na frente (vão de 9 × 5 m), o fundo escuro e as caixas lá dentro
    p.box(3.6, -11.3, z, 5.6, -4.6, 6.0, 'Body', 0.15)
    p.box(3.6, 4.6, z, 5.6, 11.3, 6.0, 'Body', 0.15)
    p.box(3.6, -4.6, 5.0, 5.6, 4.6, 6.0, 'Body', 0.15)
    p.box(3.2, -4.6, z, 3.63, 4.6, 5.0, 'Dark', 0.02)
    for k in range(3):
        p.light(4.6, -3.0 + k * 3.0, 4.85, 0.18)
    # a vaga do caminhão: setas e faixa no piso
    p.hazard_frame(5.8, -3.8, hx - 0.4, 3.8)
    for k in range(3):
        x = 7.0 + k * 1.4
        p.box(x, -0.25, z + 0.02, x + 0.9, 0.25, z + 0.04, 'Paint', 0.0)
    # a ponte rolante amarela sobre a vaga, com o trole e o gancho
    for y in (-4.4, 4.4):
        p.box(9.6, y - 0.45, z, 10.5, y + 0.45, 7.4, 'Yellow', 0.10)
        p.box(9.4, y - 0.65, z, 10.7, y + 0.65, z + 0.8, 'Hazard', 0.06)
    p.box(9.63, -4.82, 6.6, 10.47, 4.82, 7.37, 'Yellow', 0.10)
    p.box(5.6, -0.5, 6.7, 10.5, 0.5, 7.2, 'Yellow', 0.06)
    p.box(9.4, -0.8, 6.2, 10.7, 0.8, 6.6, 'Dark', 0.04)
    p.pipe((10.05, 0, 6.2), (10.05, 0, 4.4), 0.04, 'Dark')
    p.box(9.75, -0.3, 4.0, 10.35, 0.3, 4.4, 'Yellow', 0.04)
    # o teto: o H de pouso, maquinário, guarda-corpo
    p.box(-8.4, -4.4, 6.0, -0.4, 4.4, 6.04, 'Trim', 0.0)
    p.box(-6.8, -2.6, 6.04, -6.0, 2.6, 6.08, 'Paint', 0.0)
    p.box(-2.8, -2.6, 6.04, -2.0, 2.6, 6.08, 'Paint', 0.0)
    p.box(-6.0, -0.4, 6.04, -2.8, 0.4, 6.08, 'Paint', 0.0)
    p.box(-10.4, 6.8, 6.0, -6.6, 10.6, 7.0, 'Body', 0.08)
    p.vent_grille(-10.1, 7.1, 7.0, -6.9, 10.3, slats=5)
    p.rail([(-11.0, -11.3), (3.6, -11.3), (3.6, 11.3), (-11.0, 11.3), (-11.0, -11.3)], 6.0, 1.0)
    # o pátio do lado (−Y na frente): caixas recebidas debaixo de lona
    for x in (6.4, 8.6):
        p.box(x - 0.8, -11.2, z, x + 0.8, -9.6, z + 1.2, 'Crate', 0.05)
        p.box(x - 0.8, -11.2, z + 1.2, x + 0.8, -9.6, z + 2.4, 'Crate', 0.05)
    p.tarp(5.4, -11.4, 9.6, -9.4, z + 2.4)
    p.lamp_post(hx - 0.8, hy - 0.8)
    p.lamp_post(hx - 0.8, -hy + 0.8, 5.0)
    p.export('supply_center/supply_center.glb')
    scaffold('supply_center_scaffold', -10.2, -10.3, 5.6, 10.3, 6.0, gap=0.3).export('supply_center/supply_center_scaffold.glb')


# ====================================================================== Depósito
def supply_dock():
    """O depósito do mapa (ZH SupplyDock 26,3 × 32,1 × 3,4 m): pátio de concreto com barreiras,
    um galpão baixo e a empilhadeira. As caixas são instâncias (a caixa sai num arquivo
    próprio) e somem conforme o estoque baixa, como o W3DSupplyDraw do ZH."""
    hx, hy = 13.17, 16.06
    p = Part('supply_dock')
    pad(p, hx, hy, 0.2)
    z = 0.22
    # barreiras de concreto (jersey) nas bordas, com vãos pra entrar dos 4 lados
    for (x0, y0, x1, y1) in ((-hx + 0.2, -hy + 0.2, -3.0, -hy + 0.8), (3.0, -hy + 0.2, hx - 0.2, -hy + 0.8),
                             (-hx + 0.2, hy - 0.8, -3.0, hy - 0.2), (3.0, hy - 0.8, hx - 0.2, hy - 0.2),
                             (-hx + 0.2, -hy + 0.8, -hx + 0.8, -3.0), (-hx + 0.2, 3.0, -hx + 0.8, hy - 0.8),
                             (hx - 0.8, -hy + 0.8, hx - 0.2, -3.0), (hx - 0.8, 3.0, hx - 0.2, hy - 0.8)):
        p.box(x0, y0, z, x1, y1, z + 0.85, 'Concrete', 0.12)
    # o galpão no canto e a empilhadeira
    p.box(-hx + 1.2, hy - 7.0, z, -hx + 7.0, hy - 1.2, 3.4, 'Body', 0.12)
    p.box(-hx + 7.0, hy - 6.0, 1.2, -hx + 7.06, hy - 2.2, 2.4, 'Glass', 0.01)
    p.box(-hx + 1.0, hy - 7.2, 3.4, -hx + 7.2, hy - 1.0, 3.7, 'Trim', 0.06)
    p.box(4.0, 12.0, z, 6.2, 13.4, 1.5, 'Yellow', 0.08)
    p.box(4.4, 12.2, 1.5, 5.6, 13.2, 2.4, 'Glass', 0.04)
    p.box(6.2, 12.2, z, 6.4, 13.2, 2.6, 'Dark', 0.03)
    p.box(6.4, 12.3, z + 0.1, 7.4, 13.1, z + 0.2, 'Dark', 0.02)
    for x, y in ((4.4, 12.0), (5.8, 12.0), (4.4, 13.4), (5.8, 13.4)):
        p.cyl(x, y, z, 0.3, 0.25, 'Rubber', 12, 0.03, 'Y')
    # o piso: as vagas das pilhas pintadas
    for gx in range(5):
        for gy in range(6):
            x = -9.6 + gx * 4.8
            y = -12.6 + gy * 4.2
            if gx <= 1 and gy >= 4:
                continue   # o canto do galpão
            p.box(x - 1.3, y - 1.3, z, x + 1.3, y + 1.3, z + 0.02, 'Paint', 0.0)
    p.lamp_post(hx - 1.4, -hy + 1.4, 6.0)
    p.lamp_post(-hx + 1.4, -hy + 1.4, 6.0)
    p.export('supply_dock/supply_dock.glb')

    crate = Part('supply_crate')
    # a caixa de suprimento (1,4 × 1,2 × 1,0 m): o pallet, as tábuas com o quadro, o estêncil
    crate.box(-0.75, -0.65, 0.0, 0.75, 0.65, 0.14, 'Trim', 0.03)
    crate.box(-0.7, -0.6, 0.14, 0.7, 0.6, 1.0, 'Crate', 0.05)
    for sx in (-1, 1):
        crate.box(sx * 0.7 - 0.05, -0.62, 0.14, sx * 0.7 + 0.05, 0.62, 1.02, 'Crate', 0.02)
        crate.box(-0.72, sx * 0.6 - 0.05, 0.14, 0.72, sx * 0.6 + 0.05, 1.02, 'Crate', 0.02)
    crate.box(-0.72, -0.62, 0.55, 0.72, 0.62, 0.62, 'Crate', 0.02)
    crate.box(-0.3, -0.61, 0.38, 0.3, -0.6, 0.78, 'Paint', 0.0)
    crate.box(-0.3, 0.6, 0.38, 0.3, 0.61, 0.78, 'Paint', 0.0)
    crate.export('supply_crate/supply_crate.glb')


# ====================================================================== Dozer
def dozer():
    """7,9 × 5,3 × 3,9 m (ZH). A esteira e as rodas são as do nosso tanque (o mesmo tamanho);
    aqui sai o corpo: casco, cabine com vidro, giroflex âmbar, o escape, os braços da lâmina.
    A lâmina sai à parte (mexe quando ele trabalha), com o pivô na junta dos braços."""
    p = Part('dozer')
    p.box(-3.4, -1.55, 0.95, 2.3, 1.55, 2.0, 'Body', 0.12)
    p.box(-3.5, -1.65, 1.85, 2.4, 1.65, 2.05, 'Trim', 0.05)
    p.box(-3.3, -1.40, 2.05, -1.0, 1.40, 2.55, 'Body', 0.10)          # o capô do motor
    p.vent_grille(-3.1, -1.1, 2.55, -1.3, 1.1, slats=6)
    p.cyl(-2.8, -1.0, 2.55, 0.14, 1.1, 'Dark', 12, 0.02)               # o escape
    p.cyl(-2.8, -1.0, 3.65, 0.17, 0.12, 'Trim', 12, 0.02)
    # a cabine deslocada pra um lado (a silhueta do Dozer de cima)
    p.box(-0.9, -1.45, 2.05, 1.3, 0.55, 3.55, 'Body', 0.10)
    p.box(1.3, -1.3, 2.4, 1.36, 0.4, 3.3, 'Glass', 0.01)
    p.box(-0.8, -1.51, 2.4, 1.2, -1.45, 3.3, 'Glass', 0.01)
    p.box(-0.8, 0.55, 2.4, 1.2, 0.61, 3.3, 'Glass', 0.01)
    p.box(-1.0, -1.55, 3.55, 1.4, 0.65, 3.7, 'Trim', 0.05)
    p.cyl(0.2, -0.5, 3.7, 0.16, 0.22, 'Beacon', 12, 0.02)             # o giroflex
    # cor do jogador: faixa no casco e lona no capô
    p.box(-3.42, -1.57, 1.45, 2.32, -1.53, 1.7, 'Player', 0.01)
    p.box(-3.42, 1.53, 1.45, 2.32, 1.57, 1.7, 'Player', 0.01)
    p.tarp(-2.9, 0.3, -1.5, 1.3, 2.55, 'Player')
    # os braços da lâmina (presos no casco; a lâmina mexe no pivô deles)
    for y in (-1.9, 1.9):
        p.box(-0.6, y - 0.18, 0.9, 4.1, y + 0.18, 1.3, 'Yellow', 0.06)
        p.cyl(-0.6, y, 1.1, 0.25, 0.4, 'Dark', 12, 0.03, 'Y')
    p.light(2.35, -1.1, 1.75, 0.12)
    p.light(2.35, 1.1, 1.75, 0.12)
    p.export('dozer/dozer.glb')

    blade = Part('dozer_blade')
    # a lâmina curva: 3 faixas inclinadas, a lâmina de corte escura embaixo; a origem é o pivô
    # (no casco, x −0,6 m z 1,1 m): a lâmina fica à frente dos braços e da esteira, em x +4,7..+5,4 do pivô
    for k, (z0, z1, lean) in enumerate(((0.0, 0.55, 0.30), (0.55, 1.1, 0.12), (1.1, 1.65, -0.08))):
        blade.box(4.75 + lean, -2.85, z0 - 1.0, 5.1 + lean, 2.85, z1 - 1.0, 'Yellow', 0.06)
    blade.box(5.0, -2.9, -1.05, 5.35, 2.9, -0.92, 'Dark', 0.03)
    for y in (-2.9, 2.9):
        blade.box(4.65, y - 0.08, -1.0, 5.35, y + 0.08, 0.6, 'Yellow', 0.03)
    blade.box(5.11, -2.0, -0.6, 5.13, 2.0, -0.45, 'Hazard', 0.0)
    blade.export('dozer/dozer_blade.glb')


# ====================================================================== Caminhão
def supply_truck():
    """9,0 × 3,7 × 3,7 m (ZH Supply Truck). Cabine, chassi, caçamba com grade; 6 rodas (giram:
    cada uma num arquivo wheel_00N, como as do tanque); a carga (0 a 4 caixas) é instância."""
    p = Part('supply_truck')
    p.box(-4.4, -1.15, 0.85, 4.3, 1.15, 1.2, 'Dark', 0.06)            # o chassi
    p.box(4.2, -1.3, 0.75, 4.55, 1.3, 1.15, 'Dark', 0.06)             # o para-choque
    # a cabine e o capô
    p.box(2.4, -1.3, 1.2, 4.0, 1.3, 3.2, 'Body', 0.12)
    p.box(4.0, -1.25, 1.2, 4.45, 1.25, 2.15, 'Body', 0.10)
    p.vent_grille(4.45, -0.9, 1.35, 4.5, 0.9, slats=5, along='Y')
    p.box(3.98, -1.15, 2.3, 4.04, 1.15, 3.0, 'Glass', 0.01)           # o para-brisa
    p.box(2.6, -1.36, 2.3, 3.8, -1.3, 3.0, 'Glass', 0.01)
    p.box(2.6, 1.3, 2.3, 3.8, 1.36, 3.0, 'Glass', 0.01)
    p.box(2.3, -1.4, 3.2, 4.1, 1.4, 3.35, 'Trim', 0.05)
    p.light(4.48, -0.95, 1.85, 0.12)
    p.light(4.48, 0.95, 1.85, 0.12)
    p.cyl(2.25, 1.05, 1.2, 0.12, 2.6, 'Dark', 12, 0.02)               # o escape atrás da cabine
    p.box(2.55, -1.32, 1.6, 3.85, -1.29, 2.1, 'Player', 0.01)         # cor do jogador na porta
    p.box(2.55, 1.29, 1.6, 3.85, 1.32, 2.1, 'Player', 0.01)
    # a caçamba: assoalho, grade dos lados, a cabeceira com a lona enrolada
    p.box(-4.4, -1.35, 1.2, 2.3, 1.35, 1.38, 'Trim', 0.04)
    p.box(2.1, -1.35, 1.38, 2.3, 1.35, 2.6, 'Body', 0.05)
    p.cyl(1.9, 0.0, 2.45, 0.28, 2.5, 'Player', 16, 0.03, 'Y')
    for y in (-1.3, 1.3):
        p.pipe((-4.35, y, 2.15), (2.1, y, 2.15), 0.05, 'Body')
        p.pipe((-4.35, y, 1.75), (2.1, y, 1.75), 0.04, 'Body')
        for k in range(5):
            x = -4.35 + k * 1.6
            p.pipe((x, y, 1.38), (x, y, 2.2), 0.05, 'Body')
    p.box(-4.45, -1.35, 1.38, -4.3, 1.35, 2.1, 'Body', 0.04)
    p.light(-4.5, -1.1, 1.3, 0.08)
    p.light(-4.5, 1.1, 1.3, 0.08)
    p.export('supply_truck/supply_truck.glb')
    # as rodas, cada uma no lugar (o jogo acha o centro pela caixa e gira em volta dele)
    wheels = []
    for x in (3.0, -1.4, -3.0):
        for y in (-1.38, 1.38):
            w = Part(f'wheel_{len(wheels) + 1:03d}')
            w.cyl(x, y, 0.55, 0.55, 0.42, 'Rubber', 24, 0.08, 'Y')
            w.cyl(x, y + (0.215 if y > 0 else -0.215), 0.55, 0.28, 0.02, 'Trim', 16, 0.0, 'Y')
            w.cyl(x, y + (0.225 if y > 0 else -0.225), 0.55, 0.1, 0.04, 'Dark', 12, 0.0, 'Y')
            wheels.append(w)
    export_parts('supply_truck/supply_truck_wheels.glb', wheels)


def tilted_box(p, cx, cy, cz, sx, sy, sz, mat, yaw=0.0, pitch=0.0, roll=0.0, bevel=0.03):
    """Caixa girada (as lascas e as chapas tortas dos destroços): o giro vai na malha, não no objeto."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=Vector((sx, sy, sz)), verts=bm.verts)
    rot = Matrix.Rotation(math.radians(yaw), 4, 'Z') @ Matrix.Rotation(math.radians(pitch), 4, 'Y') @ Matrix.Rotation(math.radians(roll), 4, 'X')
    bmesh.ops.transform(bm, matrix=rot, verts=bm.verts)
    bmesh.ops.translate(bm, vec=Vector((cx, cy, cz)), verts=bm.verts)
    return p._object(bm, mat, min(bevel, min(sx, sy, sz) / 3.0))


def debris():
    """Os destroços que ficam no chão em volta da carcaça (13/10, a revisão da cratera: "4–8 peças por morte").
    Baixos e chapados (nada em pé: de cima não podem ler como infantaria), queimados (Dark) com um pouco da
    tinta e da borracha; a origem no chão, no meio da peça. Uma malha por peça: o jogo espalha em instâncias."""
    parts = []
    # a roda arrancada, deitada (0,84 m, a do tanque)
    w = Part('SM_Debris_Wheel')
    w.cyl(0.0, 0.0, 0.0, 0.42, 0.24, 'Rubber', 24, 0.05)
    w.cyl(0.0, 0.0, 0.24, 0.30, 0.03, 'Dark', 20, 0.01)
    w.cyl(0.0, 0.0, 0.27, 0.12, 0.05, 'Trim', 12, 0.01)
    parts.append(w)
    # o pedaço de lagarta solto: as sapatas de 17 cm com a garra, dobrado no meio
    t = Part('SM_Debris_Track')
    for k in range(10):
        x = -0.8 + k * 0.17
        bend = 9.0 if x > 0.1 else 0.0
        z = 0.03 + (max(0.0, x - 0.1) * math.sin(math.radians(bend)))
        tilted_box(t, x, 0.0, z, 0.15, 0.64, 0.05, 'Dark', pitch=-bend)
        tilted_box(t, x, 0.0, z + 0.04, 0.04, 0.58, 0.04, 'Trim', pitch=-bend, bevel=0.01)
    parts.append(t)
    # a chapa da blindagem, torta em duas dobras, com um resto da tinta
    pl = Part('SM_Debris_Plate')
    tilted_box(pl, -0.35, 0.0, 0.05, 0.75, 0.95, 0.06, 'Body', roll=3.0)
    tilted_box(pl, 0.38, 0.05, 0.13, 0.75, 0.9, 0.06, 'Dark', yaw=6.0, pitch=-14.0)
    parts.append(pl)
    # a tampa da escotilha e um pedaço de caixa lateral
    h = Part('SM_Debris_Hatch')
    h.cyl(0.0, 0.0, 0.0, 0.36, 0.07, 'Body', 20, 0.02)
    tilted_box(h, 0.0, 0.0, 0.09, 0.28, 0.08, 0.05, 'Dark', bevel=0.01)
    parts.append(h)
    b = Part('SM_Debris_Box')
    tilted_box(b, 0.0, 0.0, 0.16, 0.9, 0.45, 0.3, 'Dark', roll=8.0)
    tilted_box(b, 0.1, 0.02, 0.33, 0.6, 0.35, 0.04, 'Body', yaw=4.0, roll=8.0, bevel=0.01)
    parts.append(b)
    # as lascas: blocos irregulares (cada um, três caixas giradas)
    for n, (sz, seed) in enumerate(((0.28, 3), (0.40, 7), (0.22, 11))):
        c = Part(f'SM_Debris_Chunk{n + 1}')
        r = [((seed * 37 + k * 71) % 97) / 97.0 for k in range(12)]
        tilted_box(c, 0.0, 0.0, sz * 0.25, sz, sz * 0.7, sz * 0.5, 'Dark', yaw=r[0] * 90, pitch=(r[1] - 0.5) * 30, roll=(r[2] - 0.5) * 30)
        tilted_box(c, sz * 0.25, sz * 0.1, sz * 0.2, sz * 0.6, sz * 0.5, sz * 0.4, 'Dark', yaw=r[3] * 90, pitch=(r[4] - 0.5) * 40)
        tilted_box(c, -sz * 0.2, -sz * 0.15, sz * 0.15, sz * 0.5, sz * 0.4, sz * 0.3, 'Trim', yaw=r[5] * 90, roll=(r[6] - 0.5) * 40)
        parts.append(c)
    export_parts('debris/SM_Debris.glb', parts)


bpy.ops.wm.read_factory_settings(use_empty=True)
BUILDS = {'war_factory': war_factory, 'command_center': command_center, 'power_plant': power_plant, 'supply_center': supply_center,
          'supply_dock': supply_dock, 'dozer': dozer, 'supply_truck': supply_truck, 'debris': debris}
# PROV_BUILD=debris (game.ps1 debris): só os da lista, sem refazer o resto
only = [k for k in os.environ.get('PROV_BUILD', '').split(',') if k]
for name, build in BUILDS.items():
    if not only or name in only:
        build()
print('PROV fim')
