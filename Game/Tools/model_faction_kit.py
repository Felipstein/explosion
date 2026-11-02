"""O kit das facções pros provisórios por script (o elenco das 3 facções, 31/10/2026: docs/ELENCO.md).

As peças que se repetem em cada facção, pra todos os prédios e veículos nascerem da mesma família (a bíblia de
docs/CONCEITO_CONSTRUCOES.md §2 e docs/CONCEITO_UNIDADES.md §3). Em cima do model_lib.py (Part, chanfro duro, uma cor por
slot, coordenadas da Unreal: X frente, Y direita, Z cima, metros).

Uso num modelo:
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from model_lib import COLORS, Part, V, clear_scene, export, export_group, flag, sheet, scaffold
    from model_faction_kit import *          # as cores de conferência de China e GLA já entram no COLORS

Os slots (o nome do material no Blender = a instância do jogo; Game/Tools/import_provisional.py, FACTION_SLOTS):
  China (prédio): CnConcrete (o concreto escuro puxado pro oliva), CnPlinth (a base mais clara), CnRoof (a telha verde
    vidrada), CnMetal (o aço escuro), CnGold (o latão da estrela e dos ornamentos), Player (o vermelho: beiral, pórtico,
    estandarte, estrela), Fire (a boca do forno, acesa), Dark, Trim, Glass, Light, Beacon, Hazard, Paint (o branco da
    chaminé listrada).
  China (veículo): CnTankPaint (o oliva escuro), CnTankCamo (a mancha da camuflagem digital, mais escura), CnTankMetal,
    Track, Rubber, Player, Lens, Dark.
  GLA (prédio): Plaster (o adobe), Earth (o chão batido do lote), Rust (a chapa enferrujada), Wood, Cloth (a lona), Net (a
    rede de camuflagem), Crate, Metal, Player (o verde: bandeira, rabisco, borda dos muros), Dark, Trim, Light, Hazard.
  GLA (veículo): GlaPaint (o amarelo-areia fosco), GlaStripe (a listra verde-escura gasta), Rust, Cloth, Wood, Track,
    Rubber, Player, Lens, Dark, Metal.
"""
import math

from model_lib import COLORS, V  # noqa: F401

# as cores de conferência (sRGB 0–255; o jogo troca pelas instâncias medidas, import_provisional.py)
COLORS.update({
    # China: escura, massiva (o concreto lê ~0,30 de luminância de cima contra 0,55 da USA: docs/CONCEITO_UNIDADES.md §3)
    'CnConcrete': (96, 100, 82), 'CnPlinth': (128, 128, 112), 'CnRoof': (66, 98, 70), 'CnMetal': (60, 62, 58),
    'CnGold': (182, 144, 62), 'Fire': (255, 140, 40), 'Red': (178, 22, 24),   # (01/11) o vermelho fixo da estrela do ELP
    'CnTankPaint': (82, 90, 62), 'CnTankCamo': (56, 64, 44), 'CnTankMetal': (52, 54, 50),
    # GLA: areia, ferrugem, lona (o adobe é o da vila: o CivPlaster do jogo)
    'Plaster': (205, 180, 135), 'Earth': (168, 142, 104), 'Rust': (118, 70, 40), 'Wood': (120, 88, 52),
    'Cloth': (150, 128, 92), 'Net': (96, 104, 62), 'Crate': (84, 86, 58),
    'GlaPaint': (196, 168, 104), 'GlaStripe': (70, 84, 50),
    # comuns
    'Player': (180, 40, 36), 'Dark': (40, 40, 38), 'Trim': (70, 70, 68), 'Metal': (92, 90, 82), 'Glass': (30, 50, 80),
    'Light': (255, 230, 170), 'Beacon': (255, 170, 40), 'Hazard': (214, 170, 30), 'Paint': (222, 222, 216),
    'Track': (40, 38, 34), 'Rubber': (28, 28, 28), 'Lens': (40, 60, 80), 'Yellow': (214, 164, 32),
})


# ===================================================================================================== comuns

def foundation(p, hx, hy, m='CnPlinth', z=0.14, skirt=2.5, rim=0.25, rim_m='Dark'):
    """A laje do lote (só a borda de fundação, a regra do §1 da bíblia) com a saia que entra no chão (desnível do terreno).
    (31/10, o detalhe) sem o fundo enterrado e o friso escuro só na borda (era uma chapa do lote inteiro, escondida: na grade do
    detalhe, dezenas de milhares de triângulos que nunca aparecem)."""
    from model_building_parts import open_box, ring_boxes
    open_box(p, m, -hx, hx, -hy, hy, -skirt, z, bevel=0.06)
    ring_boxes(p, rim_m, -hx - 0.03, hx + 0.03, -hy - 0.03, hy + 0.03, z - 0.10, z - 0.07, 0.3, bevel=0.0)
    if rim > 0:
        ring_boxes(p, m, -hx, hx, -hy, hy, z, z + 0.08, rim, bevel=0.02)


def band(p, x0, x1, y0, y1, z0, z1, faces='+x-x+y-y', m='Player', out=0.02):
    """A faixa (cor do jogador) colada nas faces de fora de um bloco."""
    if '+x' in faces:
        p.box(m, x1, x1 + out, y0, y1, z0, z1, bevel=0.0)
    if '-x' in faces:
        p.box(m, x0 - out, x0, y0, y1, z0, z1, bevel=0.0)
    if '+y' in faces:
        p.box(m, x0, x1, y1, y1 + out, z0, z1, bevel=0.0)
    if '-y' in faces:
        p.box(m, x0, x1, y0 - out, y0, z0, z1, bevel=0.0)


def lamp(p, x, y, z, face='+x', m='Light', kind=None):
    """A luminária de parede, virada pra face. (31/10, o detalhe AAA) com o corpo: a base, o braço, a cúpula inclinada e a lente
    virada pra baixo (~0,45 m: lê como objeto na câmera, e a luz cai no reboco embaixo dela). Rente ao chão (z < 0,6 m) é a luz
    de rodapé embutida: a caixa chanfrada com a lente recuada."""
    from model_building_parts import WallFace, wall_lamp, wbox
    F = WallFace(face, x if face[1] == 'x' else y, 0.0)
    u = y if face[1] == 'x' else x
    if (kind or ('flush' if z < 0.6 else 'wall')) == 'flush':
        wbox(p, 'Dark#lamp', F, u - 0.15, u + 0.15, z - 0.1, z + 0.1, -0.02, 0.07, bevel=0.012)
        wbox(p, m, F, u - 0.1, u + 0.1, z - 0.06, z + 0.06, 0.03, 0.055, bevel=0.0)
    else:
        wall_lamp(p, F, u, z, m=m)


def ladder(p, x, y0, y1, z0, z1, m='Trim'):
    for y in (y0, y1):
        p.box(m, x - 0.03, x + 0.03, y - 0.03, y + 0.03, z0, z1, bevel=0.0)
    for k in range(int((z1 - z0) / 0.3)):
        z = z0 + 0.15 + k * 0.3
        p.box(m, x - 0.025, x + 0.025, y0, y1, z - 0.025, z + 0.025, bevel=0.0)


def railing(p, pts, z, h=1.0, m='Trim', post_step=1.5):
    """Corrimão: postes e dois tubos ao longo da polilinha [(x, y)...] na altura z."""
    for (ax, ay), (bx, by) in zip(pts[:-1], pts[1:]):
        L = math.hypot(bx - ax, by - ay)
        n = max(1, int(L / post_step))
        for k in range(n + 1):
            t = k / n
            p.cyl(m, (ax + (bx - ax) * t, ay + (by - ay) * t, z + h / 2), 'z', 0.03, h, seg=6, bevel=0.0)
        for zz in (z + h, z + h * 0.5):
            p.cyl(m, ((ax + bx) / 2, (ay + by) / 2, zz), (bx - ax, by - ay, 0.0), 0.028, L, seg=6, bevel=0.0)


def barrel(p, x, y, z, r=0.3, h=0.9, m='Rust', ring='Dark'):
    """O tambor de 200 L: o corpo, os 2 aros rolados (o relevo que pega luz), as bordas de cima e de baixo e a tampa rebaixada
    com o bujão (31/10, o detalhe: era um cilindro com 2 discos)."""
    p.cyl(m, (x, y, z + h / 2), 'z', r, h, seg=16, bevel=0.02, smooth=True)
    for dz in (0.33, 0.67):
        p.torus(m + '#hoop', (x, y, z + dz * h), 'z', r + 0.004, 0.022, seg=16, rseg=6)
    for zz in (z + 0.02, z + h - 0.02):
        p.torus(m + '#hoop', (x, y, zz), 'z', r - 0.01, 0.026, seg=16, rseg=6)
    p.cyl(ring, (x, y, z + h - 0.025), 'z', r - 0.035, 0.02, seg=16, bevel=0.0)
    p.cyl(m + '#hoop', (x + r * 0.5, y + r * 0.2, z + h - 0.01), 'z', 0.045, 0.04, seg=8, bevel=0.0)


def crate_stack(p, x, y, z, n=3, yaw=0.0, size=(1.6, 0.8, 0.5), m='Crate', strap='Dark'):
    """A pilha de caixotes: cada um com o chanfro, as 2 cintas e as ripas das pontas em relevo (31/10, o detalhe: o caixote lia
    como um bloco liso)."""
    for k in range(n):
        yy = yaw + (k % 2) * 4
        c = (x + (k % 2) * 0.05, y, z + size[2] / 2 + k * size[2])
        p.obox(m, c, size, yaw=yy, bevel=0.03)
        a = math.radians(yy)
        for s in (-1, 1):                                                   # as cintas
            o = s * size[0] * 0.3
            p.obox(strap, (c[0] + o * math.cos(a), c[1] + o * math.sin(a), c[2]), (0.06, size[1] + 0.025, size[2] + 0.025), yaw=yy, bevel=0.0)
        for s in (-1, 1):                                                   # as ripas das pontas (o quadro em relevo)
            o = s * (size[0] / 2 + 0.012)
            p.obox(m + '#batten', (c[0] + o * math.cos(a), c[1] + o * math.sin(a), c[2]), (0.03, size[1] * 0.92, size[2] * 0.9), yaw=yy, bevel=0.01)


def polygon_prism(p, m, pts, z0, z1, bevel=0.0):
    """Prisma vertical de um polígono [(x, y)...] (pode ser côncavo, como a estrela)."""
    p.loft(m, [[(x, y, z0) for (x, y) in pts], [(x, y, z1) for (x, y) in pts]], bevel=bevel)


def star_points(cx, cy, r_out, r_in=None, rot_deg=90.0):
    """Os 10 cantos da estrela de 5 pontas no plano XY (a ponta de cima em rot_deg)."""
    r_in = r_in if r_in is not None else r_out * 0.40
    pts = []
    for k in range(10):
        a = math.radians(rot_deg + 36.0 * k)
        r = r_out if k % 2 == 0 else r_in
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def star_floor(p, cx, cy, r, z, m='Player', h=0.012):
    """A estrela pintada no piso (a do pátio da China): prisma bem fino, a ponta pra frente (+X)."""
    polygon_prism(p, m, star_points(cx, cy, r, rot_deg=0.0), z, z + h)


def _bar2(u0, v0, u1, v1, w):
    """O traço reto (o quadrilátero de largura w em volta do segmento) no plano (u, v)."""
    du, dv = u1 - u0, v1 - v0
    L = math.hypot(du, dv) or 1.0
    nu, nv = -dv / L * w / 2, du / L * w / 2
    return [(u0 + nu, v0 + nv), (u1 + nu, v1 + nv), (u1 - nu, v1 - nv), (u0 - nu, v0 - nv)]


def bayi_strokes(r):
    """Os traços do 八一 (o emblema do Exército de Libertação Popular) no plano (u, v) da estrela de raio r: o 八 à esquerda (os dois
    traços que abrem pra baixo, cada um em 2 pedaços pra curvar), o 一 à direita (a barra)."""
    w = 0.085 * r
    return [_bar2(-0.17 * r, 0.24 * r, -0.21 * r, 0.02 * r, w), _bar2(-0.21 * r, 0.02 * r, -0.36 * r, -0.22 * r, w),
            _bar2(-0.06 * r, 0.24 * r, -0.04 * r, 0.00 * r, w), _bar2(-0.04 * r, 0.00 * r, 0.06 * r, -0.22 * r, w),
            _bar2(0.13 * r, -0.02 * r, 0.42 * r, -0.02 * r, w * 1.05)]


def star_wall(p, c, face, r, m='Player', t=0.03, rim=None):
    """A estrela na parede, colada na face ('+x', '-x', '+y', '-y'), a ponta pra cima.
    (01/11, APROVADA com a imagem `docs/aprovados/modelos/2026-11-01_estrelas_russia_china_APROVADAS.jpg`) a estrela da China é o emblema do
    Exército de Libertação Popular: a estrela VERMELHA (o slot Red, fixo) com a BORDA DOURADA (CnGold) e o 八一 dourado no meio. O m de antes
    (a cor do jogador) não pinta mais a estrela; rim põe o disco atrás como antes."""
    x, y, z = c
    if face in ('+x', '-x'):
        s = 1 if face == '+x' else -1
        f = lambda u, v, d: (x + s * d, y + u, z + v)
    else:
        s = 1 if face == '+y' else -1
        f = lambda u, v, d: (x + u, y + s * d, z + v)

    def lay(mat, pts2, d0, d1):
        p.loft(mat, [[f(u, v, d0) for (u, v) in pts2], [f(u, v, d1) for (u, v) in pts2]], bevel=0.0)
    lay('CnGold', star_points(0.0, 0.0, r * 1.12, r_in=r * 1.12 * 0.40, rot_deg=90.0), 0.0, t)
    lay('Red', star_points(0.0, 0.0, r, rot_deg=90.0), t, t * 1.6)
    # o 八一 lido de frente: nas faces '+x' e '-y' o u da face anda pra esquerda de quem olha (a Unreal é de mão esquerda): espelha
    mir = -1.0 if face in ('+x', '-y') else 1.0
    for q in bayi_strokes(r):
        qm = [(mir * u, v) for (u, v) in q]
        lay('CnGold', qm if mir > 0 else qm[::-1], t * 1.6, t * 2.0)   # espelhado, a ordem inverte (a normal pra fora)
    if rim:
        circle_on_face(p, rim, (x, y, z), face, r * 1.32, t * 0.6)


def circle_on_face(p, m, c, face, r, t, seg=32):
    x, y, z = c
    axis = 'x' if face in ('+x', '-x') else 'y'
    s = 1 if face[0] == '+' else -1
    if axis == 'x':
        p.cyl(m, (x + s * t / 2 - s * 0.005, y, z), 'x', r, t, seg=seg, bevel=0.0)
    else:
        p.cyl(m, (x, y + s * t / 2 - s * 0.005, z), 'y', r, t, seg=seg, bevel=0.0)


def disc_ring(p, m, c, r_out, r_in, z0, z1, seg=40):
    """Um anel (arruela) horizontal: o aro de um domo, a borda de uma plataforma redonda."""
    outer = [(c[0] + r_out * math.cos(2 * math.pi * k / seg), c[1] + r_out * math.sin(2 * math.pi * k / seg)) for k in range(seg)]
    inner = [(c[0] + r_in * math.cos(2 * math.pi * k / seg), c[1] + r_in * math.sin(2 * math.pi * k / seg)) for k in range(seg)]
    b = p.bm(m, 0.0)
    vo0 = [b.verts.new(V(x, y, z0)) for (x, y) in outer]
    vo1 = [b.verts.new(V(x, y, z1)) for (x, y) in outer]
    vi0 = [b.verts.new(V(x, y, z0)) for (x, y) in inner]
    vi1 = [b.verts.new(V(x, y, z1)) for (x, y) in inner]
    for k in range(seg):
        j = (k + 1) % seg
        b.faces.new([vo0[k], vo0[j], vo1[j], vo1[k]])
        b.faces.new([vi0[j], vi0[k], vi1[k], vi1[j]])
        b.faces.new([vo1[k], vo1[j], vi1[j], vi1[k]])
        b.faces.new([vo0[j], vo0[k], vi0[k], vi0[j]])


def dome(p, m, c, r, h, z0, seg=36, rings=10, smooth=True, bevel=0.0, onion=0.0):
    """Domo de revolução (meia-esfera achatada na altura h). onion > 0 dá a barriga de cebola (o bulbo) e a ponta."""
    rr = []
    for j in range(rings + 1):
        t = j / rings
        a = t * math.pi / 2
        rad = r * math.cos(a)
        if onion:
            rad = r * (math.sin(math.pi * (0.35 + 0.65 * t)) * (1 + onion * math.sin(math.pi * t)) / (1 + onion * 0.5)) * (1 - t ** 3)
        z = z0 + h * math.sin(a)
        rad = max(rad, 0.02)
        rr.append([(c[0] + rad * math.cos(2 * math.pi * k / seg), c[1] + rad * math.sin(2 * math.pi * k / seg), z) for k in range(seg)])
    p.loft(m, rr, bevel=bevel, smooth=smooth, caps=(True, True))


def revolve(p, m, c, profile, seg=40, smooth=True, bevel=0.0, caps=(True, True)):
    """Sólido de revolução em volta do eixo z em c=(x, y): profile = [(raio, z)...] de baixo pra cima."""
    rr = [[(c[0] + max(r, 0.01) * math.cos(2 * math.pi * k / seg), c[1] + max(r, 0.01) * math.sin(2 * math.pi * k / seg), z)
           for k in range(seg)] for (r, z) in profile]
    p.loft(m, rr, bevel=bevel, smooth=smooth, caps=caps)


def cone_horn(p, m, base, direction, r0, r1, length, seg=16, bevel=0.0):
    """A corneta (o alto-falante): cone de r0 (na base) a r1 (na boca) ao longo de direction."""
    d = math.sqrt(sum(v * v for v in direction))
    dx, dy, dz = (v / d for v in direction)
    mid = (base[0] + dx * length / 2, base[1] + dy * length / 2, base[2] + dz * length / 2)
    p.cyl(m, mid, (dx, dy, dz), r0, length, seg=seg, r2=r1, bevel=bevel)


# ===================================================================================================== China

def hip_roof(p, x0, x1, y0, y1, z0, rise, overhang=1.0, top_frac=0.35, m='CnRoof', edge='Player', lip=0.28, curl=0.55,
             ridge='CnRoof', ridge_h=0.35, ornaments=True):
    """O telhado de pagode de um tier: o quadril com o beiral saindo 'overhang' além da parede, a borda vermelha (fáscia) no
    beiral, as pontas dos cantos levantadas ('curl', a assinatura chinesa) e a cumeeira com os ornamentos nas pontas.
    O beiral fica em z0; o topo (o retângulo menor, top_frac do de baixo, centrado) em z0 + rise."""
    ex0, ex1, ey0, ey1 = x0 - overhang, x1 + overhang, y0 - overhang, y1 + overhang
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    hx, hy = (ex1 - ex0) / 2 * top_frac, (ey1 - ey0) / 2 * top_frac
    # a concavidade do telhado chinês: 3 anéis (o beiral, o meio mais baixo que a reta, o topo)
    def ring(sx, sy, z):
        return [(cx - sx, cy - sy, z), (cx + sx, cy - sy, z), (cx + sx, cy + sy, z), (cx - sx, cy + sy, z)]
    sx0, sy0 = (ex1 - ex0) / 2, (ey1 - ey0) / 2
    mid_t = 0.5
    sxm, sym = sx0 + (hx - sx0) * mid_t, sy0 + (hy - sy0) * mid_t
    p.loft(m, [ring(sx0, sy0, z0 - 0.05), ring(sx0, sy0, z0 + 0.12), ring(sxm, sym, z0 + rise * 0.38), ring(hx, hy, z0 + rise)],
           bevel=0.04)
    # a fáscia vermelha em volta do beiral
    for (a0, a1, b0, b1) in ((ex0, ex1, ey0 - 0.04, ey0 + 0.10), (ex0, ex1, ey1 - 0.10, ey1 + 0.04),
                             (ex0 - 0.04, ex0 + 0.10, ey0, ey1), (ex1 - 0.10, ex1 + 0.04, ey0, ey1)):
        p.box(edge, a0, a1, b0, b1, z0 - lip, z0 + 0.06, bevel=0.0)
    # as pontas levantadas nos 4 cantos (a "asa": uma lâmina que sobe pra fora)
    for (sx, sy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        px, py = cx + sx * sx0, cy + sy * sy0
        yaw = math.degrees(math.atan2(sy, sx))
        p.obox(m, (px + sx * 0.35, py + sy * 0.35, z0 + curl * 0.45), (1.3, 0.34, 0.18), yaw=yaw, pitch=32, bevel=0.03)
        p.obox(edge, (px + sx * 0.78, py + sy * 0.78, z0 + curl * 0.85), (0.45, 0.22, 0.22), yaw=yaw, pitch=40, bevel=0.02)
    # a cumeeira no topo
    if ridge_h > 0:
        p.box(ridge, cx - hx, cx + hx, cy - 0.16, cy + 0.16, z0 + rise, z0 + rise + ridge_h, bevel=0.04)
        if ornaments:
            for s in (-1, 1):
                p.obox(edge, (cx + s * hx, cy, z0 + rise + ridge_h + 0.18), (0.35, 0.3, 0.55), pitch=-s * 18, bevel=0.03)
    return z0 + rise


def pagoda(p, x0, x1, y0, y1, z0, tiers, wall_m='CnConcrete', roof_m='CnRoof', edge='Player', wall_h=2.2, rise=1.5,
           overhang=1.1, shrink=0.72, window='Dark'):
    """Telhados de pagode empilhados: tier k = parede curta (com a faixa de janelinhas) + telhado; cada tier encolhe 'shrink'.
    Devolve o z do topo."""
    z = z0
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    hx, hy = (x1 - x0) / 2, (y1 - y0) / 2
    for k in range(tiers):
        if k > 0:
            hx, hy = hx * shrink, hy * shrink
        a0, a1, b0, b1 = cx - hx, cx + hx, cy - hy, cy + hy
        h = wall_h if k > 0 else 0.0
        if h > 0:
            p.box(wall_m, a0, a1, b0, b1, z, z + h, bevel=0.05)
            # a fileira de janelinhas (escuras) nas 4 faces
            for s in (-1, 1):
                for t in range(max(2, int(2 * hx / 1.6))):
                    u = a0 + (t + 0.5) * (a1 - a0) / max(2, int(2 * hx / 1.6))
                    p.box(window, u - 0.28, u + 0.28, (b0 if s < 0 else b1) - 0.02, (b0 if s < 0 else b1) + 0.02, z + 0.6, z + h - 0.5, bevel=0.0)
                for t in range(max(2, int(2 * hy / 1.6))):
                    u = b0 + (t + 0.5) * (b1 - b0) / max(2, int(2 * hy / 1.6))
                    p.box(window, (a0 if s < 0 else a1) - 0.02, (a0 if s < 0 else a1) + 0.02, u - 0.28, u + 0.28, z + 0.6, z + h - 0.5, bevel=0.0)
            z += h
        z = hip_roof(p, a0, a1, b0, b1, z, rise * (0.85 ** k), overhang=overhang * (0.9 ** k), m=roof_m, edge=edge,
                     ridge_h=0.35 if k == tiers - 1 else 0.0, ornaments=(k == tiers - 1)) - rise * (0.85 ** k) * 0.45
    return z + rise * 0.45


def cooling_tower(p, c, r_base, r_waist, r_top, h, z0=0.0, m='CnConcrete', rim='CnPlinth', stripe='Player', seg=40):
    """A torre de resfriamento em ampulheta (hiperboloide) com o aro em cima, os pés (a fila de colunas inclinadas) e a
    faixa vermelha perto da boca. Proibida na USA (é da China: docs/CONCEITO_CONSTRUCOES.md §3b)."""
    waist_z = 0.68
    prof = []
    for j in range(13):
        t = j / 12
        if t <= waist_z:
            u = t / waist_z
            r = r_base + (r_waist - r_base) * (1 - (1 - u) ** 2)
        else:
            u = (t - waist_z) / (1 - waist_z)
            r = r_waist + (r_top - r_waist) * u * u
        prof.append((r, z0 + 1.6 + t * (h - 1.6)))
    revolve(p, m, c, prof, seg=seg, smooth=True, caps=(False, False))
    revolve(p, 'Dark', c, [(r * 0.97, z) for (r, z) in reversed(prof)], seg=seg, smooth=True, caps=(False, False))  # o lado de dentro
    disc_ring(p, rim, c, r_top + 0.12, r_top - 0.12, z0 + h - 0.05, z0 + h + 0.3, seg=seg)
    # a faixa vermelha (um anel baixo colado por fora, perto da boca)
    zr = z0 + h - 1.6
    rr = prof[-2][0] + 0.03
    disc_ring(p, stripe, c, rr + 0.04, rr - 0.08, zr, zr + 0.7, seg=seg)
    # os pés: a fila de colunas em V que seguram a casca (o ar entra por baixo)
    n = 24
    for k in range(n):
        a = 2 * math.pi * k / n
        lean = 0.35 if k % 2 == 0 else -0.35
        x, y = c[0] + r_base * math.cos(a), c[1] + r_base * math.sin(a)
        p.obox(rim, (x, y, z0 + 0.8), (0.28, 0.28, 1.75), yaw=math.degrees(a) + 90, roll=math.degrees(lean), bevel=0.02)
    disc_ring(p, rim, c, r_base + 0.25, r_base - 0.3, z0, z0 + 0.15, seg=seg)


def chimney_striped(p, c, r0, r1, h, z0, bands=4, m='Paint', stripe='Player', cap='Dark'):
    """A chaminé listrada de vermelho e branco (a da bíblia da China)."""
    seg = 20
    for k in range(bands * 2):
        za, zb = z0 + h * k / (bands * 2), z0 + h * (k + 1) / (bands * 2)
        ra, rb = r0 + (r1 - r0) * k / (bands * 2), r0 + (r1 - r0) * (k + 1) / (bands * 2)
        p.cyl(stripe if k % 2 else m, (c[0], c[1], (za + zb) / 2), 'z', ra, zb - za, seg=seg, r2=rb, bevel=0.0, smooth=True)
    p.cyl(cap, (c[0], c[1], z0 + h + 0.12), 'z', r1 + 0.08, 0.25, seg=seg, bevel=0.0)
    p.cyl('Dark', (c[0], c[1], z0 + h + 0.2), 'z', r1 * 0.75, 0.12, seg=seg, bevel=0.0)


def banner(p, pole, top, height=3.2, width=1.1, m='Player', star='CnGold', pole_m='CnMetal', face='+x'):
    """O estandarte vertical (pano vermelho no mastro com a travessa) com a estrela dourada. face: pra onde o pano olha."""
    x, y = pole
    p.cyl(pole_m, (x, y, top / 2), 'z', 0.06, top, seg=8, bevel=0.0)
    p.cyl(star, (x, y, top + 0.12), 'z', 0.1, 0.24, seg=8, r2=0.02, bevel=0.0)
    along_y = face in ('+x', '-x')
    s = 1 if face[0] == '+' else -1
    off = 0.08 * s
    if along_y:
        p.cyl(pole_m, (x + off, y, top - 0.15), 'y', 0.035, width + 0.2, seg=6, bevel=0.0)
        p.box(m, x + off - 0.015, x + off + 0.015, y - width / 2, y + width / 2, top - 0.15 - height, top - 0.15, bevel=0.0)
        star_wall(p, (x + off + 0.016 * s, y, top - 0.15 - height * 0.32), face, width * 0.28, m=star, t=0.012)
    else:
        p.cyl(pole_m, (x, y + off, top - 0.15), 'x', 0.035, width + 0.2, seg=6, bevel=0.0)
        p.box(m, x - width / 2, x + width / 2, y + off - 0.015, y + off + 0.015, top - 0.15 - height, top - 0.15, bevel=0.0)
        star_wall(p, (x, y + off + 0.016 * s, top - 0.15 - height * 0.32), face, width * 0.28, m=star, t=0.012)


def speaker_cluster(p, c, z, n=4, r_horn=0.55, length=1.2, tilt=-12.0, m='CnMetal', yaw0=0.0):
    """O cacho de cornetas de alto-falante da propaganda: n cornetas em volta de um poste, inclinadas pra baixo."""
    for k in range(n):
        a = math.radians(yaw0 + 360.0 * k / n)
        d = (math.cos(a) * math.cos(math.radians(tilt)), math.sin(a) * math.cos(math.radians(tilt)), math.sin(math.radians(tilt)))
        base = (c[0] + math.cos(a) * 0.18, c[1] + math.sin(a) * 0.18, z)
        cone_horn(p, m, base, d, 0.12, r_horn, length, seg=16)
        cone_horn(p, 'Dark', (base[0] + d[0] * (length - 0.04), base[1] + d[1] * (length - 0.04), base[2] + d[2] * (length - 0.04)),
                  d, r_horn * 0.88, r_horn * 0.9, 0.06, seg=16)
    p.cyl(m, (c[0], c[1], z), 'z', 0.24, 0.5, seg=12, bevel=0.0)


def furnace_mouth(p, c, face, w, h, m_frame='CnMetal'):
    """A boca do forno acesa (laranja) numa parede: moldura de aço, a grade e o fogo atrás."""
    x, y, z = c
    if face in ('+x', '-x'):
        s = 1 if face == '+x' else -1
        p.box(m_frame, x - 0.05 * s if s > 0 else x, x + 0.25 * s if s > 0 else x + 0.05, y - w / 2 - 0.25, y + w / 2 + 0.25, z - 0.25, z + h + 0.25, bevel=0.02)
        p.box('Fire', x + 0.24 * s - 0.02, x + 0.24 * s + 0.02, y - w / 2, y + w / 2, z, z + h, bevel=0.0)
        for k in range(1, 5):
            yy = y - w / 2 + k * w / 5
            p.box('Dark', x + 0.27 * s - 0.03, x + 0.27 * s + 0.03, yy - 0.05, yy + 0.05, z, z + h, bevel=0.0)
    else:
        s = 1 if face == '+y' else -1
        p.box(m_frame, x - w / 2 - 0.25, x + w / 2 + 0.25, y if s > 0 else y - 0.25, y + 0.25 if s > 0 else y, z - 0.25, z + h + 0.25, bevel=0.02)
        p.box('Fire', x - w / 2, x + w / 2, y + 0.24 * s - 0.02, y + 0.24 * s + 0.02, z, z + h, bevel=0.0)
        for k in range(1, 5):
            xx = x - w / 2 + k * w / 5
            p.box('Dark', xx - 0.05, xx + 0.05, y + 0.27 * s - 0.03, y + 0.27 * s + 0.03, z, z + h, bevel=0.0)


# ===================================================================================================== GLA

def adobe_wall(p, x0, x1, y0, y1, h, z0=0.2, top_band=True, m='Plaster', round_top=True):
    """Um trecho de muro de adobe (com o topo arredondado, a quina gasta) e a borda verde (cor do jogador) no alto."""
    p.box(m, x0, x1, y0, y1, z0, z0 + h, bevel=0.07)
    if round_top:
        along_x = (x1 - x0) > (y1 - y0)
        r = (y1 - y0) / 2 if along_x else (x1 - x0) / 2
        if along_x:
            p.cyl(m, ((x0 + x1) / 2, (y0 + y1) / 2, z0 + h), 'x', r * 0.98, x1 - x0, seg=10, bevel=0.0, smooth=True)
        else:
            p.cyl(m, ((x0 + x1) / 2, (y0 + y1) / 2, z0 + h), 'y', r * 0.98, y1 - y0, seg=10, bevel=0.0, smooth=True)
    if top_band:
        along_x = (x1 - x0) > (y1 - y0)
        band(p, x0, x1, y0, y1, z0 + h - 0.5, z0 + h - 0.08, faces='+y-y' if along_x else '+x-x')


def pilaster(p, x, y, h, z0=0.2, s=0.45, m='Plaster'):
    p.box(m, x - s, x + s, y - s, y + s, z0, z0 + h, bevel=0.05)
    p.box(m, x - s - 0.08, x + s + 0.08, y - s - 0.08, y + s + 0.08, z0 + h - 0.25, z0 + h, bevel=0.03)


def scribble(p, c, face, size=1.9, m='Player'):
    """O rabisco verde à mão na parede (três pinceladas cruzadas), colado na face."""
    x, y, z = c
    for ang in (28, -22, 75):
        if face in ('+x', '-x'):
            p.obox(m, (x + (0.015 if face == '+x' else -0.015), y, z), (0.01, size, 0.32), roll=ang, bevel=0.0)
        else:
            p.obox(m, (x, y + (0.015 if face == '+y' else -0.015), z), (size, 0.01, 0.32), pitch=ang, bevel=0.0)


def parapet(p, x0, x1, y0, y1, z, h=0.5, t=0.4, m='Plaster'):
    """O parapeito do teto plano de adobe."""
    for (a0, a1, b0, b1) in ((x0, x1, y0 - 0.05, y0 + t), (x0, x1, y1 - t, y1 + 0.05), (x0 - 0.05, x0 + t, y0, y1), (x1 - t, x1 + 0.05, y0, y1)):
        p.box(m, a0, a1, b0, b1, z, z + h, bevel=0.04)


def adobe_block(p, x0, x1, y0, y1, h, z0=0.2, m='Plaster', band_z=None, corners=True):
    """Um bloco de adobe de teto plano com o parapeito, os pilaretes gastos nos cantos e a faixa verde."""
    p.box(m, x0, x1, y0, y1, z0, z0 + h, bevel=0.08)
    parapet(p, x0, x1, y0, y1, z0 + h, m=m)
    if band_z:
        band(p, x0, x1, y0, y1, z0 + band_z[0], z0 + band_z[1])
    if corners:
        for (x, y) in ((x0, y0), (x0, y1), (x1, y0), (x1, y1)):
            p.box(m, x - 0.45, x + 0.45, y - 0.45, y + 0.45, z0, z0 + h + 0.7, bevel=0.05)


def window(p, x, y, z, face, w=1.1, h=1.0, frame='Plaster', m='Dark', shutter=None):
    """Janela escura recuada com o lintel; shutter: material da veneziana aberta ao lado.
    (31/10, o detalhe AAA) o vão com fundo de verdade na parede maciça: a moldura saliente de 0,16 m (as ombreiras, o peitoril que
    sai mais, a verga de madeira por cima) e o escuro recuado atrás dela; a sombra da moldura dentro do vão é o que lê de cima."""
    from model_building_parts import WallFace, wbox
    F = WallFace(face, x if face[1] == 'x' else y, 0.0)
    u = y if face[1] == 'x' else x
    o = 0.16
    fr = frame.split('#')[0] + '#jamb'
    wbox(p, m, F, u - w / 2, u + w / 2, z, z + h, -0.03, 0.01, bevel=0.0)
    wbox(p, fr, F, u - w / 2 - 0.16, u - w / 2, z - 0.05, z + h + 0.05, -0.05, o, bevel=0.03)
    wbox(p, fr, F, u + w / 2, u + w / 2 + 0.16, z - 0.05, z + h + 0.05, -0.05, o, bevel=0.03)
    wbox(p, fr, F, u - w / 2 - 0.2, u + w / 2 + 0.2, z - 0.14, z + 0.0, -0.05, o + 0.07, bevel=0.03)
    wbox(p, 'Wood#lintel', F, u - w / 2 - 0.3, u + w / 2 + 0.3, z + h, z + h + 0.17, -0.05, o + 0.04, bevel=0.03)
    wbox(p, 'Wood#lintel', F, u - 0.025, u + 0.025, z, z + h, -0.03, 0.05, bevel=0.0)
    if shutter:
        wbox(p, shutter, F, u + w / 2 + 0.18, u + w / 2 + 0.72, z + 0.02, z + h - 0.02, 0.0, 0.05, bevel=0.015)
        for k in range(1, 4):
            uu = u + w / 2 + 0.18 + 0.54 * k / 4
            wbox(p, 'Dark', F, uu - 0.012, uu + 0.012, z + 0.05, z + h - 0.05, 0.05, 0.056, bevel=0.0)


def arch_door(p, x, y, z, face, w=1.8, h=2.6, m='Wood', frame='Plaster'):
    """A porta em arco (o arco redondo de adobe), virada pra face."""
    s = 1 if face[0] == '+' else -1
    if face in ('+x', '-x'):
        p.box(frame, min(x, x + 0.25 * s), max(x, x + 0.25 * s), y - w / 2 - 0.25, y + w / 2 + 0.25, z, z + h, bevel=0.03)
        p.cyl(frame, (x + 0.12 * s, y, z + h), 'x', w / 2 + 0.25, 0.26, seg=20)
        p.box(m, min(x + 0.2 * s, x + 0.28 * s), max(x + 0.2 * s, x + 0.28 * s), y - w / 2 + 0.1, y + w / 2 - 0.1, z, z + h, bevel=0.0)
        p.cyl(m, (x + 0.24 * s, y, z + h), 'x', w / 2 - 0.1, 0.08, seg=20, bevel=0.0)
    else:
        p.box(frame, x - w / 2 - 0.25, x + w / 2 + 0.25, min(y, y + 0.25 * s), max(y, y + 0.25 * s), z, z + h, bevel=0.03)
        p.cyl(frame, (x, y + 0.12 * s, z + h), 'y', w / 2 + 0.25, 0.26, seg=20)
        p.box(m, x - w / 2 + 0.1, x + w / 2 - 0.1, min(y + 0.2 * s, y + 0.28 * s), max(y + 0.2 * s, y + 0.28 * s), z, z + h, bevel=0.0)
        p.cyl(m, (x, y + 0.24 * s, z + h), 'y', w / 2 - 0.1, 0.08, seg=20, bevel=0.0)


def sandbag_row(p, pts, z0=0.0, layers=3, m='Cloth', bag=(0.62, 0.36, 0.22)):
    """A mureta de sacos de areia ao longo da polilinha [(x, y)...]: os sacos arredondados, desencontrados por fileira."""
    for (ax, ay), (bx, by) in zip(pts[:-1], pts[1:]):
        L = math.hypot(bx - ax, by - ay)
        yaw = math.degrees(math.atan2(by - ay, bx - ax))
        n = max(1, int(L / (bag[0] * 0.95)))
        for lv in range(layers):
            off = 0.5 if lv % 2 else 0.0
            for k in range(n):
                t = (k + 0.5 + off * 0.5) / n
                if t > 1:
                    continue
                x, y = ax + (bx - ax) * t, ay + (by - ay) * t
                p.obox(m, (x, y, z0 + bag[2] * (lv + 0.5)), (bag[0], bag[1], bag[2]), yaw=yaw + (k * 7 % 9 - 4), bevel=0.08)


def sandbag_ring(p, c, r, z0=0.0, layers=3, gap_deg=None, m='Cloth', bag=(0.62, 0.36, 0.22)):
    """O ninho redondo de sacos de areia; gap_deg = (de, até) pra abertura da entrada."""
    n = max(8, int(2 * math.pi * r / (bag[0] * 0.95)))
    for lv in range(layers):
        for k in range(n):
            a = 2 * math.pi * (k + (0.5 if lv % 2 else 0.0)) / n
            deg = math.degrees(a) % 360
            if gap_deg and gap_deg[0] <= deg <= gap_deg[1]:
                continue
            p.obox(m, (c[0] + r * math.cos(a), c[1] + r * math.sin(a), z0 + bag[2] * (lv + 0.5)), bag,
                   yaw=math.degrees(a) + 90, bevel=0.08)


def tire(p, c, R=0.42, r=0.14, axis='z', m='Rubber'):
    p.torus(m, c, axis, R, r, seg=20, rseg=8)


def tire_stack(p, x, y, z0=0.0, n=3, R=0.42, r=0.14):
    for k in range(n):
        tire(p, (x + 0.03 * (k % 2), y, z0 + r + k * 2 * r * 0.95), R, r)


def camo_net(p, corners, nx=14, ny=10, sag=0.6, posts=True, post_m='Wood'):
    """A rede de camuflagem esticada entre 4 cantos (x, y, z) no sentido horário, com os postes nos cantos."""
    from model_lib import sheet
    sheet(p, 'Net', corners, nx, ny, sag=sag, thick=0.03)
    if posts:
        for (x, y, z) in corners:
            p.cyl(post_m, (x, y, z / 2), 'z', 0.07, z, seg=8, bevel=0.0)


def tarp_roof(p, x0, x1, y0, y1, z_hi, z_lo, sag=0.15, m='Cloth', poles=True, pole_m='Wood'):
    """A lona esticada em caimento (alta em x0, baixa em x1) com os paus de apoio."""
    from model_lib import sheet
    sheet(p, m, [(x0, y0, z_hi), (x1, y0, z_lo), (x1, y1, z_lo), (x0, y1, z_hi)], 6, 4, sag=sag, thick=0.03)
    if poles:
        for (x, y, z) in ((x1, y0, z_lo), (x1, y1, z_lo)):
            p.cyl(pole_m, (x, y, z / 2), 'z', 0.05, z, seg=8, bevel=0.0)


def corrugated(p, x0, x1, y0, y1, z0, z1, axis='x', m='Rust', pitch=0.18, amp=0.035, bend=0.09, patch=False, seed=0):
    """A chapa ondulada (o telhado e o remendo de ferro velho): a onda de verdade (seno, 6 pontos por onda; a onda tem 2×pitch),
    espessura fina e fechada, ao longo de 'axis'. Plano inclinado de z0 (em x0 ou y0) a z1 (em x1 ou y1).
    (31/10, o detalhe AAA) a borda de baixo dobrada pra baixo (a pingadeira da chapa, 'bend'), e patch=True põe o remendo: um
    pedaço menor por cima, torto, de outra chapa."""
    b = p.bm(m, 0.0)
    th = 0.025
    L0, L1 = (y0, y1) if axis == 'x' else (x0, x1)
    waves = max(1, int(round((L1 - L0) / (2 * pitch))))
    n = waves * 6
    prof = [(L0 + (L1 - L0) * k / n, amp * math.sin(2 * math.pi * k / 6)) for k in range(n + 1)]
    rows = []
    if bend and z0 <= z1:
        rows.append((-0.02, z0 - bend))
    rows += [(0.0, z0), (1.0, z1)]
    if bend and z0 > z1:
        rows.append((1.02, z1 - bend))

    def pt(s, zz, t, dz):
        if axis == 'x':
            return V(x0 + (x1 - x0) * t, s, zz + dz)
        return V(s, y0 + (y1 - y0) * t, zz + dz)
    top = [[b.verts.new(pt(s, zz, t, dz)) for (s, dz) in prof] for (t, zz) in rows]
    bot = [[b.verts.new(pt(s, zz, t, dz - th)) for (s, dz) in prof] for (t, zz) in rows]
    R = len(rows)
    for r in range(R - 1):
        for k in range(n):
            b.faces.new([top[r][k], top[r][k + 1], top[r + 1][k + 1], top[r + 1][k]]).smooth = True
            b.faces.new([bot[r + 1][k], bot[r + 1][k + 1], bot[r][k + 1], bot[r][k]]).smooth = True
        b.faces.new([top[r][0], top[r + 1][0], bot[r + 1][0], bot[r][0]])
        b.faces.new([top[r][n], bot[r][n], bot[r + 1][n], top[r + 1][n]])
    for k in range(n):
        b.faces.new([top[0][k], bot[0][k], bot[0][k + 1], top[0][k + 1]])
        b.faces.new([top[R - 1][k], top[R - 1][k + 1], bot[R - 1][k + 1], bot[R - 1][k]])
    if patch:
        h = ((seed * 7919 + 13) % 97) / 97.0
        u0, u1 = 0.2 + 0.3 * h, 0.5 + 0.3 * h
        t0, t1 = 0.25 + 0.2 * h, 0.6 + 0.2 * h
        za, zb = z0 + (z1 - z0) * t0 + amp + 0.03, z0 + (z1 - z0) * t1 + amp + 0.03
        if axis == 'x':
            corrugated(p, x0 + (x1 - x0) * t0, x0 + (x1 - x0) * t1, y0 + (y1 - y0) * u0, y0 + (y1 - y0) * u1, za, zb, axis,
                       m.split('#')[0] + '#patch', pitch * 0.8, amp, 0.0)
        else:
            corrugated(p, x0 + (x1 - x0) * u0, x0 + (x1 - x0) * u1, y0 + (y1 - y0) * t0, y0 + (y1 - y0) * t1, za, zb, axis,
                       m.split('#')[0] + '#patch', pitch * 0.8, amp, 0.0)


def tunnel_ramp(p, x0, x1, half_w, z_floor=0.2, depth=2.6, m_dark='Dark', m_wall='Plaster', lintel_h=3.0, band_player=True):
    """A rampa do túnel da GLA (a marca da facção: a unidade sobe de baixo da terra). A boca escura em x0, a rampa descendo de
    x1 até x0; os ombros e o lintel de adobe sobre a boca."""
    p.obox(m_dark, ((x0 + x1) / 2, 0.0, z_floor - depth / 2), (math.hypot(x1 - x0, depth), 2 * half_w - 0.1, 0.1),
           pitch=-math.degrees(math.atan2(depth, x1 - x0)), bevel=0.0)
    p.box(m_dark, x0 - 0.6, x0 + 0.2, -half_w + 0.05, half_w - 0.05, z_floor - depth, z_floor + lintel_h - 0.1, bevel=0.0)
    for s in (-1, 1):
        p.box(m_wall, x0 - 0.8, x1, s * half_w if s > 0 else -half_w - 0.5, s * half_w + 0.5 if s > 0 else -half_w, z_floor, z_floor + 0.9, bevel=0.05)
    p.box(m_wall, x0 - 1.4, x0 + 0.4, -half_w - 0.5, half_w + 0.5, z_floor + lintel_h - 0.1, z_floor + lintel_h + 0.9, bevel=0.06)
    for s in (-1, 1):
        p.box(m_wall, x0 - 1.4, x0 + 0.4, s * (half_w + 0.25) - 0.25, s * (half_w + 0.25) + 0.25, z_floor, z_floor + lintel_h, bevel=0.05)
    if band_player:
        band(p, x0 - 1.4, x0 + 0.4, -half_w - 0.5, half_w + 0.5, z_floor + lintel_h + 0.25, z_floor + lintel_h + 0.75, faces='+x')
