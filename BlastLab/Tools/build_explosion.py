"""Explosão gravada tocando num Heterogeneous Volume, no campo de teste.

Pré-requisitos: convert_bake.py (quadros densos), make_noise.py (ruído) e o
mapa montado (build_scene.py). Rodar: powershell -File BlastLab/Tools/blast.ps1
build-explosion (que roda os pré-requisitos que faltarem).

Unidades (conferidas no fonte da 5.8):
- o Heterogeneous Volume marcha no espaço LOCAL da instância, em voxels
  (HeterogeneousVolumesRayMarchingUtils.ush: exp(-Extinction · passo local);
  a caixa local é [0, resolução], HeterogeneousVolumeComponent::CalcBounds) →
  extinção e emissão do material são POR VOXEL: valor por metro × tamanho do voxel;
- radiância do WebGL → cd/m²: × 128 000 (js/atmosphere.js: "radiância do
  render → cd/m²: E_lux / E_render", com 1 unidade de iluminância = SUN_LUX).
"""
import hashlib
import json
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ue_util import PROJECT, camera  # noqa: E402

# As 3 variantes gravadas no WebGL (sementes diferentes da mesma carga): o
# simulador sorteia uma por detonação.
VARIANTS = 3
BAKE_RAW = os.path.join(PROJECT, 'Intermediate', 'BakeRaw', 'r96_v%d')            # convert_bake.py
SVT = '/Game/Explosions/SVT_HeavyCharge_v%d'
BAKE = '/Game/Explosions/DA_HeavyCharge_v%d'      # UExplosionBake: volume + tempos + caixas + curva de luz
MATERIAL = '/Game/Explosions/M_Explosion'
POOL_FACTOR = 12.0
NOISE_RAW = os.path.join(PROJECT, 'Intermediate', 'Noise', 'curl_noise_64.bgra')  # make_noise.py
NOISE = '/Game/Explosions/T_CurlNoise'
MAP = '/Game/Maps/TestRange'
LUX_PER_UNIT = 128000.0        # js/celestial.js SUN_LUX
# Emissão = a do WebGL em cd/m², sem fator. (Até 05/10 ela levava 0.17/0.067 =
# 2.54× pra compensar a exposição "física" de então, 1.8 stop mais escura;
# com a exposição do WebGL de volta o fator fazia a bola de fogo emitir 2× o do
# WebGL, medido na mesma câmera aos 0.4/0.8/1.5 s, e à noite ela estourava.)
WEBGL_CONVENTION = 1.0

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
log = lambda *a: unreal.log('EXPLOSION ' + ' '.join(str(x) for x in a))

def read_meta(variant):
    with open(os.path.join(BAKE_RAW % variant, 'meta.json'), encoding='utf-8') as f:
        return json.load(f)


META = read_meta(0)
RES = META['resolution'][0]
DOMAIN_M = META['domain_m']
VOXEL_M = DOMAIN_M / RES


# ---- 1. volume animado + dados da gravação ---------------------------------------
def volume(variant):
    """O asset guarda a impressão do meta.json da conversão: se o bake bruto
    mudou (outro formato, outra faixa de velocidade), ele é refeito."""
    raw, svt = BAKE_RAW % variant, SVT % variant
    meta = read_meta(variant)
    with open(os.path.join(raw, 'meta.json'), 'rb') as f:
        fingerprint = hashlib.sha1(f.read()).hexdigest()[:16]
    lib = unreal.EditorAssetLibrary
    if lib.does_asset_exist(svt):
        v = unreal.load_asset(svt)
        if lib.get_metadata_tag(v, 'bake_raw') == fingerprint:
            if v.get_editor_property('streaming_pool_size_factor') != POOL_FACTOR:
                v.set_editor_property('streaming_pool_size_factor', POOL_FACTOR)
                lib.save_loaded_asset(v)
            return v
        for a in (MATERIAL, BAKE % variant, svt):      # o material e os dados referenciam o volume
            if lib.does_asset_exist(a) and not lib.delete_asset(a):
                raise RuntimeError('could not delete ' + a)
        log('raw bake changed: rebuilding', svt)
    v, error = unreal.BlastLabEditorLib.create_animated_volume(
        raw, svt, unreal.IntVector(RES, RES, RES), meta['frames'], DOMAIN_M * 100.0)
    if not v:
        raise RuntimeError('volume: ' + error)
    lib.set_metadata_tag(v, 'bake_raw', fingerprint)
    # o pool de streaming de cada volume guarda o maior quadro × este fator
    # (3 por padrão, SparseVolumeTextureStreamingManager.cpp): duas explosões
    # da mesma variante já não cabiam e saíam quadros borrados ou incompletos.
    # 12 = 2 quadros (a mistura) × até 6 explosões vivas da mesma variante.
    # (Com 56, a sequência inteira residente, as 3 variantes passavam 4 GB além
    # do orçamento de vídeo da 1660.)
    v.set_editor_property('streaming_pool_size_factor', POOL_FACTOR)
    lib.save_loaded_asset(v)
    log('volume created', svt, meta['frames'], 'frames')
    return v


def bake_asset(variant, svt):
    """UExplosionBake (ExplosionBake.h): tudo o que a reprodução precisa — o
    simulador detona qualquer variante em qualquer lugar sem a sequência."""
    path = BAKE % variant
    meta = read_meta(variant)
    lib = unreal.EditorAssetLibrary
    if lib.does_asset_exist(path):
        b = unreal.load_asset(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.ExplosionBake)
        b = asset_tools.create_asset(path.rsplit('/', 1)[1], path.rsplit('/', 1)[0], unreal.ExplosionBake, factory)
    b.set_editor_property('volume', svt)
    b.set_editor_property('domain_cm', DOMAIN_M * 100.0)
    b.set_editor_property('resolution', RES)
    b.set_editor_property('frame_times', [float(t) for t in meta['frame_times_s']])
    b.set_editor_property('box_min', [unreal.IntVector(*x[0]) for x in meta['boxes']])
    b.set_editor_property('box_size', [unreal.IntVector(*x[1]) for x in meta['boxes']])
    # luz: WebGL (x, y pra cima, z) em metros → Unreal (x, z, y) em cm, relativa ao pé
    b.set_editor_property('light_position', [unreal.Vector(p[0] * 100.0, p[2] * 100.0, p[1] * 100.0)
                                             for p in meta['light_pos_webgl']])
    b.set_editor_property('light_color', [unreal.LinearColor(c[0], c[1], c[2], 1.0) for c in meta['light_color']])
    vel, res, heights = bake_velocity(variant)
    b.set_editor_property('velocity', vel)
    b.set_editor_property('velocity_resolution', res)
    b.set_editor_property('soot_height', [h * 100.0 for h in heights])
    lib.save_loaded_asset(b)
    log('bake asset', path)
    return b


VELOCITY_RAW = os.path.join(PROJECT, 'Intermediate', 'BakeVelocity')    # export_bake_velocity.py
VELOCITY = '/Game/Explosions/Interaction/T_BakeVelocity_v%d'
WARP_NONE = '/Game/Explosions/Interaction/T_WarpNone'


def bake_velocity(variant):
    """A velocidade gravada em baixa resolução, os quadros empilhados em z
    (ExplosionBake.h Velocity), e a altura média da fuligem de cada quadro."""
    with open(os.path.join(VELOCITY_RAW, 'velocity.json'), encoding='utf-8') as f:
        info = json.load(f)
    n, frames = info['resolution'], info['frames']
    t, error = unreal.BlastLabEditorLib.create_volume_texture_half(
        os.path.join(VELOCITY_RAW, 'v%d.bin' % variant), VELOCITY % variant, unreal.IntVector(n, n, n * frames))
    if not t:
        raise RuntimeError('bake velocity %d: %s' % (variant, error))
    unreal.EditorAssetLibrary.save_loaded_asset(t)
    return t, n, info['soot_height_m_v%d' % variant]


def warp_none():
    """A deformação padrão do material: nenhuma (deslocamento 0, nada recortado).
    O simulador troca pela textura da deformação quando algo passa pela explosão."""
    raw = os.path.join(VELOCITY_RAW, 'warp_none.bin')
    with open(raw, 'wb') as f:
        f.write(bytes(2 * 2 * 2 * 8))                  # RGBA16F zerado, 2³
    t, error = unreal.BlastLabEditorLib.create_volume_texture_half(raw, WARP_NONE, unreal.IntVector(2, 2, 2))
    if not t:
        raise RuntimeError('warp none: ' + error)
    unreal.EditorAssetLibrary.save_loaded_asset(t)
    return t

def noise_texture():
    if unreal.EditorAssetLibrary.does_asset_exist(NOISE):
        return unreal.load_asset(NOISE)
    if not os.path.exists(NOISE_RAW):
        raise RuntimeError('noise missing: run python BlastLab/Tools/make_noise.py')
    t, error = unreal.BlastLabEditorLib.create_volume_texture(NOISE_RAW, NOISE, unreal.IntVector(64, 64, 64))
    if not t:
        raise RuntimeError('noise: ' + error)
    unreal.EditorAssetLibrary.save_loaded_asset(t)
    log('noise created', NOISE)
    return t


# ---- 2. tabela de corpo negro (porte de js/blackbody.js) -----------------------
def blackbody_lut(n=64, max_kelvin=3600.0):
    """Planck × CIE 1931 (aproximação de Wyman, Sloan & Shirley, JCGT 2013) →
    sRGB linear normalizado pela luminância + luminância relativa ao topo."""
    def planck(l_nm, t):
        l = l_nm * 1e-9
        return 1.1910429723971884e-16 / (l ** 5 * (math.exp(1.4387768775039337e-2 / (l * t)) - 1))

    def g(x, mu, s1, s2):
        t = (x - mu) / (s1 if x < mu else s2)
        return math.exp(-0.5 * t * t)

    def xyz(t):
        X = Y = Z = 0.0
        for l in range(360, 831, 2):
            b = planck(l, t)
            X += b * (1.056 * g(l, 599.8, 37.9, 31.0) + 0.362 * g(l, 442.0, 16.0, 26.7) - 0.065 * g(l, 501.1, 20.4, 26.2)) * 2
            Y += b * (0.821 * g(l, 568.8, 46.9, 40.5) + 0.286 * g(l, 530.9, 16.3, 31.1)) * 2
            Z += b * (1.217 * g(l, 437.0, 11.8, 36.0) + 0.681 * g(l, 459.0, 26.0, 13.8)) * 2
        return X, Y, Z

    _, yref, _ = xyz(max_kelvin)
    lut = []
    for i in range(n):
        t = max(80.0, (i + 0.5) / n * max_kelvin)
        X, Y, Z = xyz(t)
        x, z = X / Y, Z / Y
        r = 3.2404542 * x - 1.5371385 - 0.4985314 * z
        gg = -0.9692660 * x + 1.8760108 + 0.0415560 * z
        b = 0.0556434 * x - 0.2040259 + 1.0572252 * z
        m = min(r, gg, b)
        if m < 0:
            r, gg, b = r - m, gg - m, b - m
        lum = 0.2126 * r + 0.7152 * gg + 0.0722 * b or 1.0
        lut.append((r / lum, gg / lum, b / lum, Y / yref))
    return lut


# ---- 3. material de volume -----------------------------------------------------
# Parâmetros de aparência = js/instances.js (inst.params), os que o usuário aprovou.
LOOK = dict(sootExt=13.5, dustExt=5.5, sootAlbedo=0.30, dustAlbedo=0.78,
            sootColor=(0.55, 0.545, 0.535), dustColor=(0.82, 0.755, 0.655),
            emissionGain=0.62, emissionCurve=0.44, tempScale=0.92, flameBoost=1.5)
# Detalhe abaixo da resolução do bake = js/instances.js (inst.params + FLOW_NOISE):
# amplitude do deslocamento (m), frequência (1/m), modulação da densidade,
# erosão das bordas finas e período das duas fases do flow noise (s).
DETAIL = dict(amp=0.42, scale=0.85, dens=0.34, erode=0.042, period=0.5)


def displace_code():
    """Flow noise de duas fases (Vlachos, "Water Flow in Portal 2", SIGGRAPH
    2010; regeneração de Neyret, "Advected textures", SCA 2003), porte de
    FLOW_NOISE: o ruído anda com a velocidade gravada no bake e cada fase
    recomeça com outro deslocamento quando o peso dela é zero. Devolve a
    coordenada deslocada (xyz) e o ruído escalar nm (w) da modulação."""
    d = DETAIL
    return (
        'float3 lp = UVW * %r;                         // m, espaço local\n' % DOMAIN_M +
        'float3 vel = B.gba;                           // m/s (eixos da Unreal, meia precisão)\n'
        'float ph = Time / %r;\n' % d['period'] +
        'float3 r = 0;\n'
        'float w2 = 0;\n'
        '[unroll] for (int k = 0; k < 2; k++) {\n'
        '  float pk = ph + 0.5 * k;\n'
        '  float c = floor(pk), tk = pk - c;\n'
        '  float w = 1.0 - abs(2.0 * tk - 1.0);\n'
        '  // hash33 de js/glsl.js\n'
        '  float3 h = float3(c, k, Seed);\n'
        '  h = float3(dot(h, float3(127.1, 311.7, 74.7)), dot(h, float3(269.5, 183.3, 246.1)),\n'
        '             dot(h, float3(113.5, 271.9, 124.6)));\n'
        '  h = frac(sin(h) * 43758.5453123) * 2.0 - 1.0;\n'
        '  float3 off = Seed * float3(7.3, 5.1, 3.1) + h * 17.0;\n'
        '  float3 fp = (lp - vel * (tk * %r)) * %r + off;\n' % (d['period'], d['scale']) +
        '  r += w * (Texture3DSampleLevel(Noise, NoiseSampler, fp, 0).xyz * 2.0 - 1.0);\n'
        '  w2 += w * w;\n'
        '}\n'
        'float3 n = r * rsqrt(max(w2, 1e-4));\n'
        'return float4(UVW + n * %r, (n.x + n.y + n.z) * 0.577);' % (d['amp'] / DOMAIN_M)
    )


def decode_code():
    a = LOOK
    d = DETAIL
    return (
        '// códigos do WebGL (js/bake.js): fuligem r²·4, temperatura g·1.6, poeira b²·2.5, combustível r²\n'
        'float soot = A.r * A.r * 4.0;\n'
        'float temp = A.g * 1.6;\n'
        'float dust = A.b * A.b * 2.5;\n'
        'float fuel = B.r * B.r;\n'
        '// detalhe (js/instances.js sampleInst): densidade modulada pelo ruído e\n'
        '// esvaecida no teto e nas laterais da caixa (boxFade), depois as bordas\n'
        '// finas erodidas (a fumaça densa quase não perde nada)\n'
        'float2 side = min(UVW.xy, 1.0 - UVW.xy);\n'
        'float boxFade = smoothstep(0.0, 0.2, 1.0 - UVW.z) * smoothstep(0.0, %r, min(side.x, side.y));\n'
        % (5.0 / RES) +
        'float modulate = (1.0 + %r * NM) * boxFade;\n' % d['dens'] +
        'soot *= modulate;\n'
        'dust *= modulate;\n'
        'float er = %r * (0.5 - 0.5 * NM);\n' % d['erode'] +
        'soot = max(soot - er / (1.0 + soot * 9.0), 0.0);\n'
        'dust = max(dust - er * 0.7 / (1.0 + dust * 9.0), 0.0);\n'
        'float sigS = %r * soot * DensityScale;\n' % a['sootExt'] +
        'float sigD = %r * dust * DensityScale;\n' % a['dustExt'] +
        'float sigT = sigS + sigD;\n'
        'float wS = sigS / max(sigT, 1e-5);\n'
    )


# Espalhamento múltiplo em oitavas (Wrenninge et al. 2013, "Oz: The Great and
# Volumetric", SIGGRAPH; o mesmo das nuvens do Frostbite, Hillaire 2016) =
# js/instances.js inst.params: oitava o com extinção ×0.52^o, espalhamento
# ×0.52^o e fase 0.55^o mais isotrópica, e a luz do sol mix(1, sombra, 0.55^o)
# — as oitavas altas "vazam" pela sombra. O volume da Unreal só faz a oitava 0
# (fase isotrópica, sombra cheia): o lado à sombra da fumaça saía preto e o
# rastro do míssil (Al₂O₃, albedo 0.93) escuro, contra o cinza do WebGL
# (A/B de 05/10). Aqui entra, como emissão, a parte das oitavas 1–2 que não
# depende da sombra: (1 − 0.55^o) da luz-chave, com a fonte exata do WebGL
# (peso 0.52^o·0.52^o). A Unreal atenua essa emissão com a extinção cheia, e
# não com a da oitava: na fumaça densa ela fica um pouco abaixo da do WebGL.
# Medido 05/10 (mesma câmera e história, o mais escuro da nuvem): sem isso
# 8× mais escuro que o WebGL; com o limite espesso (peso 0.52^o) 2.5× mais claro.
MS = dict(octaves=3, ext=0.52, scatter=0.52, phase=0.55, g=0.42, back=0.22, mix=0.32)


def ms_fill_code():
    m = MS
    return '\n'.join([
        '// fase dupla de Henyey–Greenstein (glsl.js phaseDual), pra cada luz-chave',
        'float phIso = 1.0 / (4.0 * 3.14159265);',
        'float3 rd = -CamVec;                                   // da câmera pro ponto',
        'float3 L = 0;',
        '[unroll] for (int i = 0; i < 2; i++) {',
        '  float3 dir = i == 0 ? SunDir : MoonDir;',
        '  float3 irr = i == 0 ? Sun : Moon;',
        '  float c = dot(rd, normalize(dir + 1e-6));',
        '  float gf = %r, gb = %r;' % (m['g'], m['back']),
        '  float hf = (1.0 - gf * gf) / (4.0 * 3.14159265 * pow(max(1.0 + gf * gf - 2.0 * gf * c, 1e-4), 1.5));',
        '  float hb = (1.0 - gb * gb) / (4.0 * 3.14159265 * pow(max(1.0 + gb * gb + 2.0 * gb * c, 1e-4), 1.5));',
        '  float phSun = lerp(hf, hb, %r);' % m['mix'],
        '  float ms = 0.0, ae = 1.0, ab = 1.0, ap = 1.0;',
        '  [unroll] for (int o = 1; o < %d; o++) {' % m['octaves'],
        '    ae *= %r; ab *= %r; ap *= %r;' % (m['ext'], m['scatter'], m['phase']),
        '    ms += ae * ab * (1.0 - ap) * lerp(phIso, phSun, ap);',
        '  }',
        '  L += irr * ms;',
        '}',
        '// coeficiente de espalhamento = albedo × extinção (na unidade do volume)',
        'return Albedo * Ext.x * L;',
    ])


def ms_fill(E, link, custom, alb, ext, x, y):
    """Nó da emissão do espalhamento múltiplo, ligado ao albedo e à extinção do material."""
    nodes = {}
    for i, (name, cls) in enumerate((('Sun', unreal.MaterialExpressionSkyAtmosphereLightIlluminanceOnGround),
                                     ('Moon', unreal.MaterialExpressionSkyAtmosphereLightIlluminanceOnGround),
                                     ('SunDir', unreal.MaterialExpressionSkyAtmosphereLightDirection),
                                     ('MoonDir', unreal.MaterialExpressionSkyAtmosphereLightDirection))):
        n = E(cls, x - 300, y + 90 * i)
        n.set_editor_property('light_index', 0 if i % 2 == 0 else 1)
        nodes[name] = n
    cam = E(unreal.MaterialExpressionCameraVectorWS, x - 300, y + 360)
    c = custom('MsFill (oitavas)', ms_fill_code(), unreal.CustomMaterialOutputType.CMOT_FLOAT3, y,
               ('Albedo', 'Ext', 'Sun', 'Moon', 'SunDir', 'MoonDir', 'CamVec'), x=x)
    link(alb, '', c, 'Albedo')
    link(ext, '', c, 'Ext')
    for name, n in nodes.items():
        link(n, '', c, name)
    link(cam, '', c, 'CamVec')
    return c


def material():
    # reaproveitado e limpo (apagar falha com o simulador aberto)
    if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL):
        m = unreal.load_asset(MATERIAL)
        mel.delete_all_material_expressions(m)
    else:
        m = asset_tools.create_asset('M_Explosion', '/Game/Explosions', unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('material_domain', unreal.MaterialDomain.MD_VOLUME)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
    E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
    failed = []

    def link(a, sa, b, eb):
        if not mel.connect_material_expressions(a, sa, b, eb):
            failed.append('%s.%s -> %s.%s' % (a.get_name(), sa, b.get_name(), eb))

    # coordenada: (posição local − mínimo dos limites) / extensão, como no
    # material padrão da Epic (/Engine/EngineMaterials/SparseVolumeMaterial)
    pos = E(unreal.MaterialExpressionMaterialFunctionCall, -1500, 0)
    pos.set_material_function(unreal.load_asset(
        '/Engine/Functions/Engine_MaterialFunctions02/WorldPositionOffset/LocalPosition.LocalPosition'))
    bounds = E(unreal.MaterialExpressionObjectLocalBounds, -1500, 200)
    sub = E(unreal.MaterialExpressionSubtract, -1300, 0)
    div = E(unreal.MaterialExpressionDivide, -1150, 0)
    link(pos, '', sub, 'A')
    link(bounds, 'Min', sub, 'B')
    link(sub, '', div, 'A')
    link(bounds, 'Extents', div, 'B')
    # ...e da caixa marchada pro domínio inteiro: o componente de quadros
    # (ExplosionVolume.cpp) encolhe a caixa do volume pra caixa justa da fumaça,
    # o dado continua nas coordenadas do domínio. Sem ele: caixa = domínio.
    box = {}
    for i, (name, default) in enumerate((('BoxMin', 0.0), ('BoxSize', float(RES)))):
        p = E(unreal.MaterialExpressionVectorParameter, -1500, 400 + 150 * i)
        p.set_editor_property('parameter_name', name)
        p.set_editor_property('default_value', unreal.LinearColor(default, default, default, 0.0))
        k = E(unreal.MaterialExpressionComponentMask, -1300, 400 + 150 * i)
        for c, v in (('r', True), ('g', True), ('b', True), ('a', False)):
            k.set_editor_property(c, v)
        link(p, '', k, '')
        box[name] = k
    in_box = E(unreal.MaterialExpressionMultiply, -1000, 100)
    link(div, '', in_box, 'A')
    link(box['BoxSize'], '', in_box, 'B')
    add = E(unreal.MaterialExpressionAdd, -900, 100)
    link(in_box, '', add, 'A')
    link(box['BoxMin'], '', add, 'B')
    plain = E(unreal.MaterialExpressionDivide, -800, 100)
    link(add, '', plain, 'A')
    plain.set_editor_property('const_b', float(RES))

    def svt_sample(x, y, name='SparseVolumeTexture'):
        # o componente de quadros troca o quadro por esses nomes; os dois
        # apontam pro mesmo volume animado por padrão
        s = E(unreal.MaterialExpressionSparseVolumeTextureSampleParameter, x, y)
        s.set_editor_property('parameter_name', name)
        s.set_editor_property('sparse_volume_texture', unreal.load_asset(SVT % 0))
        return s
    # pinos (mel.get_material_expression_output_names): 'Attributes A', 'Attributes B'
    out_b = 'Attributes B'

    def param(name, value, y, x=-950):
        p = E(unreal.MaterialExpressionScalarParameter, x, y)
        p.set_editor_property('parameter_name', name)
        p.set_editor_property('default_value', value)
        return p

    def custom(name, code, output, y, inputs, x=-500):
        c = E(unreal.MaterialExpressionCustom, x, y)
        c.set_editor_property('description', name)
        c.set_editor_property('output_type', output)
        ins = []
        for n in inputs:
            ci = unreal.CustomInput()
            ci.set_editor_property('input_name', n)
            ins.append(ci)
        c.set_editor_property('inputs', ins)
        c.set_editor_property('code', code)
        return c

    # Interação com o que passa (avião, míssil, disparo, outras explosões): a
    # gravação é lida no ponto deformado y = x − D(x), com D calculado no compute
    # shader (BattleSmoke.usf MainWarp, "rest field" / mapa característico,
    # Tessendorf & Pelfrey), e perde a fração recortada (túnel do corpo e do
    # projétil, a fumaça espalhada). A grade de D cobre o domínio gravado; sem
    # nada passando é a T_WarpNone (zero) e a gravação sai exatamente como é.
    warp_tex = E(unreal.MaterialExpressionTextureObjectParameter, -1000, 1400)
    warp_tex.set_editor_property('parameter_name', 'Warp')
    warp_tex.set_editor_property('texture', warp_none())
    warp_tex.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    warp = custom('Warp (deformação)', '\n'.join([
        'float4 w = Texture3DSampleLevel(Warp, WarpSampler, UVW, 0);',
        '// D em m locais (antes da escala do ator); a fração recortada no alfa',
        'return float4(UVW - w.xyz / %r, saturate(w.w));' % DOMAIN_M,
    ]), unreal.CustomMaterialOutputType.CMOT_FLOAT4, 1300, ('UVW', 'Warp'), x=-800)
    link(plain, '', warp, 'UVW')
    link(warp_tex, '', warp, 'Warp')
    domain = E(unreal.MaterialExpressionComponentMask, -650, 100)
    carved = E(unreal.MaterialExpressionComponentMask, -650, 1300)
    for k, v in (('r', True), ('g', True), ('b', True), ('a', False)):
        domain.set_editor_property(k, v)
        carved.set_editor_property(k, not v)
    link(warp, '', domain, '')
    link(warp, '', carved, '')

    # 1ª leitura no ponto sem deslocar: só a velocidade, que leva o ruído junto
    svt0 = svt_sample(-1000, -300)
    link(domain, '', svt0, 'UVs')
    noise = E(unreal.MaterialExpressionTextureObject, -1000, -500)
    noise.set_editor_property('texture', noise_texture())
    noise.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    # tempo da gravação (s), por explosão: o ruído anda com a fumaça
    time = param('Time', 0.0, -650, x=-1000)
    seed = param('Seed', 0.0, -750, x=-1000)   # uma por explosão: duas iguais não ficam idênticas
    displace = custom('Displace (flow noise)', displace_code(), unreal.CustomMaterialOutputType.CMOT_FLOAT4,
                      -400, ('UVW', 'B', 'Time', 'Seed', 'Noise'), x=-750)
    link(domain, '', displace, 'UVW')
    link(svt0, out_b, displace, 'B')
    link(time, '', displace, 'Time')
    link(seed, '', displace, 'Seed')
    link(noise, '', displace, 'Noise')
    uvw = E(unreal.MaterialExpressionComponentMask, -600, -250)
    nm = E(unreal.MaterialExpressionComponentMask, -600, -150)
    for k, v in (('r', True), ('g', True), ('b', True), ('a', False)):
        uvw.set_editor_property(k, v)
        nm.set_editor_property(k, not v)
    link(displace, '', uvw, '')
    link(displace, '', nm, '')

    # 2ª e 3ª leituras no ponto deslocado, nos dois quadros gravados em volta
    # do instante, cada uma levada pelos vetores de movimento até ele (porte
    # de bakeAtMV): A = quadro0(p − v·a·Δt), B = quadro1(p + v·(1−a)·Δt),
    # mistura(A, B, a). Sem o componente de quadros, alfa = Δt = 0 e só o
    # quadro de baixo conta.
    alpha = param('FrameAlpha', 0.0, -900, x=-1000)
    interval = param('FrameInterval', 0.0, -1000, x=-1000)
    carry = {}
    for name, sign, weight, y in (('Back', '-', 'FrameAlpha', -50), ('Forward', '+', '(1.0 - FrameAlpha)', 50)):
        c = custom(name, 'float3 vel = B.gba;  // m/s\n'
                         'return UVW %s vel * (%s * FrameInterval / %r);' % (sign, weight, DOMAIN_M),
                   unreal.CustomMaterialOutputType.CMOT_FLOAT3, y, ('UVW', 'B', 'FrameAlpha', 'FrameInterval'), x=-600)
        link(uvw, '', c, 'UVW')
        link(svt0, out_b, c, 'B')
        link(alpha, '', c, 'FrameAlpha')
        link(interval, '', c, 'FrameInterval')
        carry[name] = c
    svt_a = svt_sample(-450, -50)
    svt_b = svt_sample(-450, 50, 'SparseVolumeTextureNext')
    link(carry['Back'], '', svt_a, 'UVs')
    link(carry['Forward'], '', svt_b, 'UVs')
    mix = {}
    for out, y in (('', 0), (out_b, 100)):
        l = E(unreal.MaterialExpressionLinearInterpolate, -300, y)
        link(svt_a, out, l, 'A')
        link(svt_b, out, l, 'B')
        link(alpha, '', l, 'Alpha')
        mix[out] = l
    density_param = param('DensityScale', 1.0, 500)
    # A fração recortada pela passagem sai da densidade (fuligem, poeira e o fogo
    # junto), primeiro onde o ruído de detalhe é mais ralo: a borda do túnel sai
    # rasgada como a da fumaça, e não um tubo liso (a erosão do Nubis, Schneider
    # 2015; a mesma forma 0.5 − 0.5·n da erosão das bordas aqui)
    kept = custom('Kept (recorte)', 'return saturate(1.0 - C * (1.0 + (0.5 - 0.5 * NM)));',
                  unreal.CustomMaterialOutputType.CMOT_FLOAT1, 1300, ('C', 'NM'), x=-500)
    link(carved, '', kept, 'C')
    link(nm, '', kept, 'NM')
    density = E(unreal.MaterialExpressionMultiply, -400, 500)
    link(density_param, '', density, 'A')
    link(kept, '', density, 'B')
    emission = param('EmissionScale', 1.0, 600)

    def decode(name, code, output, y, extra=()):
        c = custom(name, code, output, y, ('A', 'B', 'UVW', 'NM', 'DensityScale') + tuple(extra), x=-200)
        link(mix[''], '', c, 'A')
        link(mix[out_b], '', c, 'B')
        link(uvw, '', c, 'UVW')
        link(nm, '', c, 'NM')
        link(density, '', c, 'DensityScale')
        return c

    a = LOOK
    vox = VOXEL_M
    ext = decode('Extinction', decode_code() +
                 '// por voxel: o Heterogeneous Volume marcha em unidades de voxel\n'
                 'return sigT * %r;' % vox,
                 unreal.CustomMaterialOutputType.CMOT_FLOAT3, -200)
    alb = decode('Albedo', decode_code() +
                 'float3 tint = lerp(float3%r, float3%r, wS);\n' % (a['dustColor'], a['sootColor']) +
                 'float alb = lerp(%r, %r, wS);\n' % (a['dustAlbedo'], a['sootAlbedo']) +
                 'return (sigT > 1e-6) ? tint * alb : float3(0, 0, 0);',
                 unreal.CustomMaterialOutputType.CMOT_FLOAT3, 0)
    lut = blackbody_lut()
    table = ',\n'.join('float4(%.6f, %.6f, %.6f, %.6f)' % e for e in lut)
    emi = decode('Emission', decode_code() +
                 'static const float4 LUT[%d] = {\n%s};\n' % (len(lut), table) +
                 'float x = saturate(temp * %r) * %d.0 - 0.5;\n' % (a['tempScale'], len(lut)) +
                 'int i0 = clamp((int)floor(x), 0, %d), i1 = min(i0 + 1, %d);\n' % (len(lut) - 1, len(lut) - 1) +
                 'float4 bb = lerp(LUT[i0], LUT[i1], saturate(x - floor(x)));\n'
                 '// frente de chama: 2.5x onde ainda há combustível queimando (inst.params.flameBoost)\n'
                 'float flame = temp > 0.12 ? 1.0 + %r * smoothstep(0.02, 0.35, fuel) : 1.0;\n' % a['flameBoost'] +
                 'float3 e = bb.rgb * pow(max(bb.a, 0.0), %r) * %r * flame * sigS * (1.0 - %r);\n'
                 % (a['emissionCurve'], a['emissionGain'], a['sootAlbedo']) +
                 '// radiância do WebGL → cd/m², por voxel\n'
                 'return e * %r * %r * %r * EmissionScale;' % (LUX_PER_UNIT, vox, WEBGL_CONVENTION),
                 unreal.CustomMaterialOutputType.CMOT_FLOAT3, 200, extra=('EmissionScale',))
    link(emission, '', emi, 'EmissionScale')

    # Teto da sombra da luz do fogo (js/instances.js instLightCache: tauCap 2.2 —
    # "a luz pontual no centróide atravessaria a bola de fogo inteira e apagaria
    # a fumaça colada"). A sombra do volume da Unreal não tem teto: a luz de
    # dentro leva 1 − e^−2.2 com sombra (ExplosionVolume.cpp) e a parte e^−2.2
    # entra aqui, espalhada sem sombra com a queda do WebGL, I/(20·s² + d²).
    fire_pos = E(unreal.MaterialExpressionVectorParameter, -1000, 750)
    fire_pos.set_editor_property('parameter_name', 'FireLocalM')       # m, no canto do domínio
    fire_int = E(unreal.MaterialExpressionVectorParameter, -1000, 900)
    fire_int.set_editor_property('parameter_name', 'FireFill')         # cd (já × e^−2.2), rgb
    fire_int.set_editor_property('default_value', unreal.LinearColor(0, 0, 0, 1))
    scale = param('SizeScale', 1.0, 1050, x=-1000)
    fill = decode('FireFill', decode_code() +
                  'float3 tint = lerp(float3%r, float3%r, wS);\n' % (a['dustColor'], a['sootColor']) +
                  'float alb = lerp(%r, %r, wS);\n' % (a['dustAlbedo'], a['sootAlbedo']) +
                  'float3 d = (UVW * %r - FirePos.xyz) * SizeScale;      // m do mundo\n' % DOMAIN_M +
                  'float3 irr = FireInt.rgb / (20.0 * SizeScale * SizeScale + dot(d, d));\n'
                  '// espalhamento isotrópico por voxel (mesma unidade da extinção)\n'
                  'return tint * alb * (sigT * %r) * irr * (1.0 / (4.0 * 3.14159265));' % vox,
                  unreal.CustomMaterialOutputType.CMOT_FLOAT3, 350, extra=('FirePos', 'FireInt', 'SizeScale'))
    link(fire_pos, '', fill, 'FirePos')
    link(fire_int, '', fill, 'FireInt')
    link(scale, '', fill, 'SizeScale')
    emission_fire = E(unreal.MaterialExpressionAdd, -50, 250)
    link(emi, '', emission_fire, 'A')
    link(fill, '', emission_fire, 'B')
    ms = ms_fill(E, link, custom, alb, ext, -200, 1200)
    emission_total = E(unreal.MaterialExpressionAdd, 50, 300)
    link(emission_fire, '', emission_total, 'A')
    link(ms, '', emission_total, 'B')

    ok = [mel.connect_material_property(alb, '', unreal.MaterialProperty.MP_BASE_COLOR),
          mel.connect_material_property(ext, '', unreal.MaterialProperty.MP_SUBSURFACE_COLOR),
          mel.connect_material_property(emission_total, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)]
    if failed or not all(ok):
        raise RuntimeError('material: failed links %s %s' % (failed, ok))
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    log('material ok')
    return m


# ---- 4. no mapa + sequência de teste ---------------------------------------------
FPS = 30
SEQ_SECONDS = 8.0


def remove_actor(label):
    for a in actors.get_all_level_actors():
        if a.get_actor_label() == label:
            actors.destroy_actor(a)


def scene(mat, bake):
    """Uma explosão parada no centro do campo, a câmera de teste e a sequência
    que a detona (Seq_Explosion anima só a idade dela, Age): é o que os
    renders de comparação com o WebGL usam."""
    levels.load_level(MAP)
    for label in ('Explosion', 'ExplosionCam', 'ExplosionDemo', 'FireLight', 'FireLightVolume'):
        remove_actor(label)
    # origem do ator = pé da explosão; volume, luzes e escala ficam por conta
    # dele (Source/BlastLab/Public/ExplosionVolume.h)
    ex = actors.spawn_actor_from_class(unreal.ExplosionVolume, unreal.Vector(0.0, 0.0, 0.0))
    ex.set_actor_label('Explosion')
    ex.set_folder_path('Explosions')
    ex.set_editor_property('bake', bake)
    ex.set_editor_property('age', -1.0)                      # antes da detonação: nada aparece
    ex.get_component_by_class(unreal.HeterogeneousVolumeComponent).set_material(0, mat)
    cam = camera(label='ExplosionCam', dist=55, azim=-0.62, elev=0.15, target_y=22.0)
    levels.save_current_level()

    path = '/Game/Sequences/Seq_Explosion'
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    seq = asset_tools.create_asset('Seq_Explosion', '/Game/Sequences', unreal.LevelSequence,
                                   unreal.LevelSequenceFactoryNew())
    seq.set_display_rate(unreal.FrameRate(FPS, 1))
    end = int(SEQ_SECONDS * FPS)
    seq.set_playback_start(0)
    seq.set_playback_end(end)
    cam_binding = seq.add_possessable(cam)
    cut = seq.add_track(unreal.MovieSceneCameraCutTrack)
    sec = cut.add_section()
    sec.set_range(0, end)
    sec.set_camera_binding_id(seq.get_binding_id(cam_binding))

    # idade da explosão = tempo da sequência (um key por quadro: a reta sai
    # exata, sem depender da tangente automática)
    binding = seq.add_possessable(ex)
    track = binding.add_track(unreal.MovieSceneFloatTrack)
    track.set_property_name_and_path('Age', 'Age')
    s = track.add_section()
    s.set_range(0, end)
    channel = s.get_all_channels()[0]
    for q in range(end + 1):
        channel.add_key(unreal.FrameNumber(q), q / FPS, interpolation=unreal.MovieSceneKeyInterpolation.LINEAR)
    unreal.EditorAssetLibrary.save_loaded_asset(seq)
    levels.save_current_level()
    log('sequence', path, end, 'frames')


# ---- 5. a fumaça que fica no campo (BattleSmokeSubsystem) ---------------------------
SMOKE_MATERIAL = '/Game/Explosions/M_BattleSmoke'
# grade fina do campo (BattleSmokeSim.h): 96 × 96 × 64 células de 1 m
FIELD_DIM = (96, 96, 64)
# detalhe do campo (battlesmoke.js BATTLE_DEFAULTS): o das explosões levado à grade de 1 m
FIELD_DETAIL = dict(amp=0.6, scale=0.45, dens=0.34, erode=0.025, period=0.8)
EXH_COLOR = (0.9, 0.9, 0.88)       # fumaça de propelente sólido (Al₂O₃): branca, quase não absorve
EXH_ALBEDO = 0.93


def battle_smoke_material():
    """O material do volume da fumaça do campo: lê a grade fina (render target
    3D que o compute shader escreve), com o detalhe e a aparência da
    explosão — na entrega as duas têm que bater (battlesmoke.js render)."""
    if unreal.EditorAssetLibrary.does_asset_exist(SMOKE_MATERIAL):
        m = unreal.load_asset(SMOKE_MATERIAL)
        mel.delete_all_material_expressions(m)
    else:
        m = asset_tools.create_asset('M_BattleSmoke', '/Game/Explosions', unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('material_domain', unreal.MaterialDomain.MD_VOLUME)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
    # o volume do campo é criado em jogo: a marca de uso não vem do editor (sem ela, material padrão)
    mel.set_material_usage(m, unreal.MaterialUsage.MATUSAGE_HETEROGENEOUS_VOLUMES)
    E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
    failed = []

    def link(a, sa, b, eb):
        if not mel.connect_material_expressions(a, sa, b, eb):
            failed.append('%s.%s -> %s.%s' % (a.get_name(), sa, b.get_name(), eb))

    def custom(name, code, output, y, inputs, x=-300):
        c = E(unreal.MaterialExpressionCustom, x, y)
        c.set_editor_property('description', name)
        c.set_editor_property('output_type', output)
        ins = []
        for n in inputs:
            ci = unreal.CustomInput()
            ci.set_editor_property('input_name', n)
            ins.append(ci)
        c.set_editor_property('inputs', ins)
        c.set_editor_property('code', code)
        return c

    # coordenada na caixa marchada [0, 1] (como M_Explosion)
    pos = E(unreal.MaterialExpressionMaterialFunctionCall, -1400, 0)
    pos.set_material_function(unreal.load_asset(
        '/Engine/Functions/Engine_MaterialFunctions02/WorldPositionOffset/LocalPosition.LocalPosition'))
    bounds = E(unreal.MaterialExpressionObjectLocalBounds, -1400, 200)
    sub = E(unreal.MaterialExpressionSubtract, -1200, 0)
    div = E(unreal.MaterialExpressionDivide, -1050, 0)
    link(pos, '', sub, 'A')
    link(bounds, 'Min', sub, 'B')
    link(sub, '', div, 'A')
    link(bounds, 'Extents', div, 'B')
    vecs = {}
    for i, (name, default) in enumerate((('BoxMin', 0.0), ('BoxSize', 1.0))):
        p = E(unreal.MaterialExpressionVectorParameter, -1400, 350 + 150 * i)
        p.set_editor_property('parameter_name', name)
        p.set_editor_property('default_value', unreal.LinearColor(default, default, default, 0.0))
        vecs[name] = p
    time = E(unreal.MaterialExpressionScalarParameter, -1400, 650)
    time.set_editor_property('parameter_name', 'Time')
    texs = {}
    for i, name in enumerate(('Field', 'Vel', 'Noise')):
        t = E(unreal.MaterialExpressionTextureObjectParameter, -1100, 400 + 150 * i)
        t.set_editor_property('parameter_name', name)
        t.set_editor_property('texture', noise_texture())          # volume; o real vem do subsistema
        t.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        texs[name] = t

    d = FIELD_DETAIL
    a = LOOK
    field = custom('Field (fumaça do campo)', '\n'.join([
        'float3 dim = float3%r;' % (FIELD_DIM,),
        'float3 dv = UVW * BoxSize.xyz + BoxMin.xyz;            // célula fina do domínio = metros do canto',
        'float3 vel = Texture3DSampleLevel(Vel, VelSampler, dv / dim, 0).xyz;',
        '// ruído de detalhe advectado em duas fases (battlesmoke.js flowNoise): a forma',
        '// grande vem da grade de 1 m; o ruído devolve a textura fina que ela não guarda',
        'float ph = Time / %r;' % d['period'],
        'float3 r = 0;',
        'float w2 = 0;',
        '[unroll] for (int k = 0; k < 2; k++) {',
        '  float pk = ph + 0.5 * k;',
        '  float c = floor(pk), tk = pk - c;',
        '  float wt = 1.0 - abs(2.0 * tk - 1.0);',
        '  float3 h = float3(c, k, 3.0);',
        '  h = float3(dot(h, float3(127.1, 311.7, 74.7)), dot(h, float3(269.5, 183.3, 246.1)), dot(h, float3(113.5, 271.9, 124.6)));',
        '  float3 off = (frac(sin(h) * 43758.5453123) * 2.0 - 1.0) * 17.0;',
        '  // deslocamento limitado a ~3 grãos do ruído (na esteira de um avião ele esticava em risco)',
        '  float3 adv = vel * (tk * %r);' % d['period'],
        '  adv *= min(1.0, 1.5 / max(length(adv), 1e-4));',
        '  r += wt * (Texture3DSampleLevel(Noise, NoiseSampler, (dv - adv) * %r + off, 0).xyz * 2.0 - 1.0);' % d['scale'],
        '  w2 += wt * wt;',
        '}',
        'float3 n = r * rsqrt(max(w2, 1e-4));',
        'float4 f = Texture3DSampleLevel(Field, FieldSampler, (dv + n * %r) / dim, 0);' % d['amp'],
        'float nm = (n.x + n.y + n.z) * 0.577;',
        'f.xzw *= 1.0 + %r * nm;' % d['dens'],
        '// erosão absoluta: a fumaça diluída é comida primeiro, a nuvem se desfaz pelas bordas',
        'float er = %r * (0.5 - 0.5 * nm);' % d['erode'],
        'f.x = max(f.x - er / (1.0 + f.x * 9.0), 0.0);',
        'f.z = max(f.z - er * 0.7 / (1.0 + f.z * 9.0), 0.0);',
        'f.w = max(f.w - er * 0.7 / (1.0 + f.w * 9.0), 0.0);',
        '// some rente ao teto da grade em vez de cortar reto',
        'f.xzw *= smoothstep(dim.z, dim.z * 0.8, dv.z);',
        '// extinção por metro (1 voxel do volume = 1 m): fuligem, poeira, fumaça de motor; e o calor',
        'return float4(%r * f.x, %r * f.z, %r * f.w, f.y);' % (a['sootExt'], a['dustExt'], a['dustExt']),
    ]), unreal.CustomMaterialOutputType.CMOT_FLOAT4, 0,
        ('UVW', 'BoxMin', 'BoxSize', 'Time', 'Field', 'Vel', 'Noise'), x=-700)
    link(div, '', field, 'UVW')
    link(vecs['BoxMin'], '', field, 'BoxMin')
    link(vecs['BoxSize'], '', field, 'BoxSize')
    link(time, '', field, 'Time')
    for name in ('Field', 'Vel', 'Noise'):
        link(texs[name], '', field, name)

    ext = custom('Extinction', 'return (S.x + S.y + S.z).xxx;', unreal.CustomMaterialOutputType.CMOT_FLOAT3, -200, ('S',))
    alb = custom('Albedo', '\n'.join([
        'float t = S.x + S.y + S.z;',
        'float3 wk = S.xyz / max(t, 1e-5);',
        'float3 col = float3%r * wk.x + float3%r * wk.y + float3%r * wk.z;' % (a['sootColor'], a['dustColor'], EXH_COLOR),
        'float alb = dot(wk, float3(%r, %r, %r));' % (a['sootAlbedo'], a['dustAlbedo'], EXH_ALBEDO),
        'return t > 1e-6 ? col * alb : float3(0, 0, 0);',
    ]), unreal.CustomMaterialOutputType.CMOT_FLOAT3, 0, ('S',))
    lut = blackbody_lut()
    table = ',\n'.join('float4(%.6f, %.6f, %.6f, %.6f)' % e for e in lut)
    emi = custom('Emission', '\n'.join([
        '// o calor que sobrou ainda brilha fraco (mesma emissão da explosão, sem frente de chama)',
        'static const float4 LUT[%d] = {\n%s};' % (len(lut), table),
        'float x = saturate(S.w * %r) * %d.0 - 0.5;' % (a['tempScale'], len(lut)),
        'int i0 = clamp((int)floor(x), 0, %d), i1 = min(i0 + 1, %d);' % (len(lut) - 1, len(lut) - 1),
        'float4 bb = lerp(LUT[i0], LUT[i1], saturate(x - floor(x)));',
        'float3 e = bb.rgb * pow(max(bb.a, 0.0), %r) * %r * S.x * (1.0 - %r);' % (a['emissionCurve'], a['emissionGain'], a['sootAlbedo']),
        'return e * %r;                 // radiância do WebGL → cd/m², por metro' % LUX_PER_UNIT,
    ]), unreal.CustomMaterialOutputType.CMOT_FLOAT3, 200, ('S',))
    for c in (ext, alb, emi):
        link(field, '', c, 'S')
    # o mesmo espalhamento múltiplo da explosão: na entrega as duas têm que bater
    ms = ms_fill(E, link, custom, alb, ext, 0, 500)
    emission_total = E(unreal.MaterialExpressionAdd, 150, 300)
    link(emi, '', emission_total, 'A')
    link(ms, '', emission_total, 'B')
    ok = [mel.connect_material_property(alb, '', unreal.MaterialProperty.MP_BASE_COLOR),
          mel.connect_material_property(ext, '', unreal.MaterialProperty.MP_SUBSURFACE_COLOR),
          mel.connect_material_property(emission_total, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)]
    if failed or not all(ok):
        raise RuntimeError('battle smoke material: failed links %s %s' % (failed, ok))
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    log('battle smoke material ok')
    return m


if __name__ == '__main__':
    bakes = [bake_asset(v, volume(v)) for v in range(VARIANTS)]
    mat = material()
    battle_smoke_material()
    scene(mat, bakes[0])
    unreal.EditorAssetLibrary.save_directory('/Game/', only_if_is_dirty=True, recursive=True)
    log('done')
