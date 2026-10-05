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

BAKE_RAW = os.path.join(PROJECT, 'Intermediate', 'BakeRaw', 'r96_v0')            # convert_bake.py
SVT = '/Game/Explosions/SVT_HeavyCharge_v0'
MATERIAL = '/Game/Explosions/M_Explosion'
NOISE_RAW = os.path.join(PROJECT, 'Intermediate', 'Noise', 'curl_noise_64.bgra')  # make_noise.py
NOISE = '/Game/Explosions/T_CurlNoise'
COLLECTION = '/Game/Explosions/MPC_Explosion'   # "Time" = segundos desde a detonação, animado pela sequência
MAP = '/Game/Maps/TestRange'
LUX_PER_UNIT = 128000.0        # js/celestial.js SUN_LUX
# A emissão da explosão foi afinada no WebGL contra uma exposição 2.54× maior
# que a daqui (lá o chão refletia 6.7% e a exposição compensava; aqui o chão é
# físico, 17%, e a exposição é a da câmera). Brilho emitido só existe em
# relação à exposição: pra o fogo sair na tela como o usuário aprovou, a
# emissão leva esse fator. O albedo da fumaça NÃO: fuligem a 0.3 e poeira a
# 0.78 já eram valores físicos no WebGL (só o cenário não era) — com o fator a
# fumaça saía cinza-clara (comparação lado a lado).
WEBGL_CONVENTION = 0.17 / 0.067

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
log = lambda *a: unreal.log('EXPLOSION ' + ' '.join(str(x) for x in a))

with open(os.path.join(BAKE_RAW, 'meta.json'), encoding='utf-8') as f:
    META = json.load(f)
RES = META['resolution'][0]
DOMAIN_M = META['domain_m']
VOXEL_M = DOMAIN_M / RES


# ---- 1. volume animado ---------------------------------------------------------
def volume():
    """O asset guarda a impressão do meta.json da conversão: se o bake bruto
    mudou (outro formato, outra faixa de velocidade), ele é refeito."""
    with open(os.path.join(BAKE_RAW, 'meta.json'), 'rb') as f:
        fingerprint = hashlib.sha1(f.read()).hexdigest()[:16]
    lib = unreal.EditorAssetLibrary
    if lib.does_asset_exist(SVT):
        v = unreal.load_asset(SVT)
        if lib.get_metadata_tag(v, 'bake_raw') == fingerprint:
            return v
        for a in (MATERIAL, SVT):                 # o material referencia o volume
            if lib.does_asset_exist(a) and not lib.delete_asset(a):
                raise RuntimeError('could not delete ' + a)
        log('raw bake changed: rebuilding', SVT)
    v, error = unreal.BlastLabEditorLib.create_animated_volume(
        BAKE_RAW, SVT, unreal.IntVector(RES, RES, RES), META['frames'], DOMAIN_M * 100.0)
    if not v:
        raise RuntimeError('volume: ' + error)
    lib.set_metadata_tag(v, 'bake_raw', fingerprint)
    lib.save_loaded_asset(v)
    log('volume created', SVT, META['frames'], 'frames')
    return v


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


def collection():
    """Parâmetros globais da explosão de teste. No simulador cada explosão terá
    o seu tempo (parâmetro da instância); aqui a sequência anima um só, e o
    mesmo quadro sai igual em toda renderização (o relógio do jogo começaria
    em outro instante a cada execução e estragaria a comparação A/B)."""
    if unreal.EditorAssetLibrary.does_asset_exist(COLLECTION):
        return unreal.load_asset(COLLECTION)
    c = asset_tools.create_asset('MPC_Explosion', '/Game/Explosions', unreal.MaterialParameterCollection,
                                 unreal.MaterialParameterCollectionFactoryNew())
    t = unreal.CollectionScalarParameter()
    t.set_editor_property('parameter_name', 'Time')
    t.set_editor_property('default_value', 0.0)
    c.set_editor_property('scalar_parameters', [t])
    unreal.EditorAssetLibrary.save_loaded_asset(c)
    log('collection created', COLLECTION)
    return c


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


def material():
    if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL):
        unreal.EditorAssetLibrary.delete_asset(MATERIAL)
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
    domain = E(unreal.MaterialExpressionDivide, -800, 100)
    link(add, '', domain, 'A')
    domain.set_editor_property('const_b', float(RES))

    def svt_sample(x, y, name='SparseVolumeTexture'):
        # o componente de quadros troca o quadro por esses nomes; os dois
        # apontam pro mesmo volume animado por padrão
        s = E(unreal.MaterialExpressionSparseVolumeTextureSampleParameter, x, y)
        s.set_editor_property('parameter_name', name)
        s.set_editor_property('sparse_volume_texture', unreal.load_asset(SVT))
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

    # 1ª leitura no ponto sem deslocar: só a velocidade, que leva o ruído junto
    svt0 = svt_sample(-1000, -300)
    link(domain, '', svt0, 'UVs')
    noise = E(unreal.MaterialExpressionTextureObject, -1000, -500)
    noise.set_editor_property('texture', noise_texture())
    noise.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    time = E(unreal.MaterialExpressionCollectionParameter, -1000, -650)
    time.set_editor_property('collection', collection())
    time.set_editor_property('parameter_name', 'Time')
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
    density = param('DensityScale', 1.0, 500)
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

    ok = [mel.connect_material_property(alb, '', unreal.MaterialProperty.MP_BASE_COLOR),
          mel.connect_material_property(ext, '', unreal.MaterialProperty.MP_SUBSURFACE_COLOR),
          mel.connect_material_property(emi, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)]
    if failed or not all(ok):
        raise RuntimeError('material: failed links %s %s' % (failed, ok))
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    log('material ok')
    return m


# ---- 4. no mapa + sequência ----------------------------------------------------
FPS = 30
SEQ_SECONDS = 8.0


def frame_at(t):
    """Inverso de tempo(f) = duração · (f/(N−1))^curva (js/bake.js timeOfFrame)."""
    n = META['frames']
    t = min(max(t, 0.0), META['duration_s'])
    return (n - 1) * (t / META['duration_s']) ** (1.0 / META['curve'])


def remove_actor(label):
    for a in actors.get_all_level_actors():
        if a.get_actor_label() == label:
            actors.destroy_actor(a)


def scene(mat):
    levels.load_level(MAP)
    for label in ('Explosion', 'ExplosionCam'):
        remove_actor(label)
    # caixa local [0, res] voxels; o pé da explosão fica no centro da base
    half = DOMAIN_M * 50.0
    # volume + transição contínua entre os quadros gravados (Source/BlastLab/Public/ExplosionVolume.h)
    hv = actors.spawn_actor_from_class(unreal.ExplosionVolume, unreal.Vector(-half, -half, 0.0))
    hv.set_actor_label('Explosion')
    hv.set_folder_path('Explosions')
    blend = hv.get_component_by_class(unreal.FrameBlendComponent)
    blend.set_editor_property('frame_times', [float(t) for t in META['frame_times_s']])
    blend.set_editor_property('box_min', [unreal.IntVector(*b[0]) for b in META['boxes']])
    blend.set_editor_property('box_size', [unreal.IntVector(*b[1]) for b in META['boxes']])
    c = hv.get_component_by_class(unreal.HeterogeneousVolumeComponent)
    c.set_material(0, mat)
    c.set_playing(False)
    c.set_editor_property('issue_blocking_requests', True)   # render: nada de quadro faltando
    # escuta o sol (canal 0) e a luz do fogo de dentro do volume (canal 1)
    c.set_editor_property('lighting_channels', channels(True, True))
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

    # quadro do volume animado a cada quadro da sequência (curva não-uniforme do bake)
    hv_binding = seq.add_possessable(hv)
    comp_binding = seq.add_possessable(c)
    comp_binding.set_parent(hv_binding)
    track = comp_binding.add_track(unreal.MovieSceneFloatTrack)
    track.set_property_name_and_path('Frame', 'Frame')
    s = track.add_section()
    s.set_range(0, end)
    channel = s.get_all_channels()[0]
    for q in range(end + 1):          # tempo em quadros da sequência (display rate)
        channel.add_key(unreal.FrameNumber(q), frame_at(q / FPS),
                        interpolation=unreal.MovieSceneKeyInterpolation.LINEAR)
    fire_lights(seq, end)

    # tempo desde a detonação pro ruído de detalhe (um key por quadro: a reta
    # sai exata, sem depender da tangente automática)
    tm = seq.add_track(unreal.MovieSceneMaterialParameterCollectionTrack)
    tm.set_editor_property('mpc', collection())
    sm = tm.add_section()
    sm.set_range(0, end)
    for q in range(end + 1):
        sm.add_scalar_parameter_key('Time', unreal.FrameNumber(q), q / FPS)
    unreal.EditorAssetLibrary.save_loaded_asset(seq)

    # janela do simulador / Play no editor: toca a explosão em laço (ExplosionDemo.cpp)
    remove_actor('ExplosionDemo')
    demo = actors.spawn_actor_from_class(unreal.ExplosionDemo, unreal.Vector(0.0, 0.0, 0.0))
    demo.set_actor_label('ExplosionDemo')
    demo.set_folder_path('Explosions')
    demo.set_editor_property('sequence', seq)
    demo.set_editor_property('camera', cam)
    levels.save_current_level()
    log('sequence', path, end, 'frames')


# ---- 5. luz do fogo ------------------------------------------------------------
# A curva gravada no bake (posição relativa ao pé da explosão e cor/potência
# por quadro, js/bake.js) + o clarão da detonação (js/instances.js lights():
# flashGain·s²·exp(−t/flashTau), cor 1 : 0.86 : 0.64). Intensidade em
# candelas = fator da queda (ver LIGHTS) × cor × 128 000 lux por unidade.
FLASH_GAIN, FLASH_TAU = 180.0, 0.035            # js/main.js env.flashGain/flashTau


def light_at(t):
    f = frame_at(t)
    i0 = int(math.floor(f))
    i1 = min(i0 + 1, META['frames'] - 1)
    a = f - i0
    lp = [META['light_pos_webgl'][i0][k] * (1 - a) + META['light_pos_webgl'][i1][k] * a for k in range(3)]
    lc = [META['light_color'][i0][k] * (1 - a) + META['light_color'][i1][k] * a for k in range(3)]
    if t < 8 * FLASH_TAU:
        k = FLASH_GAIN * math.exp(-t / FLASH_TAU)
        lc = [lc[0] + k, lc[1] + 0.86 * k, lc[2] + 0.64 * k]
    return lp, lc


# Duas luzes, como no WebGL, que usava uma queda pro cenário e outra dentro do
# próprio volume. Uma luz só, a 1/d², iluminava a fumaça colada nela 15× mais
# que o WebGL e a bola de fogo estourava pra branco (a luz do fogo É a emissão
# do volume; contá-la de novo de perto é dupla contagem).
#  - cenário: 50·cor/d² ao longe (scene.js: cor/(s² + 0.02·d²)); não toca o
#    volume (Volumetric Scattering Intensity 0 — o Heterogeneous Volume
#    multiplica a luz por ele, HeterogeneousVolumesLightingUtils.ush);
#  - volume: 20·cor/d² ao longe (instances.js: cor/(1 + 0.05·d²)), canal de
#    iluminação 1, que só a explosão escuta; raio de fonte √20 m pra saturar
#    perto do centro como lá.
LIGHTS = (
    dict(label='FireLight', factor=50.0, radius_cm=400.0, volume=False, shadow=True),
    dict(label='FireLightVolume', factor=20.0, radius_cm=447.0, volume=True, shadow=False),
)


def channels(c0, c1):
    lc = unreal.LightingChannels()
    lc.set_editor_property('channel0', c0)
    lc.set_editor_property('channel1', c1)
    lc.set_editor_property('channel2', False)
    return lc


def fire_lights(seq, end):
    for cfg in LIGHTS:
        animated_light(seq, end, **cfg)


def animated_light(seq, end, label, factor, radius_cm, volume, shadow):
    remove_actor(label)
    light = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(0, 0, 200))
    light.set_actor_label(label)
    light.set_folder_path('Explosions')
    lc = light.get_component_by_class(unreal.PointLightComponent)
    lc.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    lc.set_editor_property('intensity_units', unreal.LightUnits.CANDELAS)
    lc.set_editor_property('intensity', 0.0)
    lc.set_editor_property('source_radius', radius_cm)
    lc.set_editor_property('attenuation_radius', 30000.0)
    lc.set_editor_property('cast_shadows', shadow)
    lc.set_editor_property('volumetric_scattering_intensity', 1.0 if volume else 0.0)
    lc.set_editor_property('lighting_channels', channels(not volume, volume))
    levels.save_current_level()
    candela_per_unit = factor * LUX_PER_UNIT

    binding = seq.add_possessable(light)
    comp = seq.add_possessable(lc)
    comp.set_parent(binding)
    tr = binding.add_track(unreal.MovieScene3DTransformTrack)
    st = tr.add_section()
    st.set_range(0, end)
    ct = st.get_all_channels()                       # x, y, z, rot, escala
    ti = comp.add_track(unreal.MovieSceneFloatTrack)
    ti.set_property_name_and_path('Intensity', 'Intensity')
    si = ti.add_section()
    si.set_range(0, end)
    ci = si.get_all_channels()[0]
    tc = comp.add_track(unreal.MovieSceneColorTrack)
    tc.set_property_name_and_path('LightColor', 'LightColor')
    sc = tc.add_section()
    sc.set_range(0, end)
    cc = sc.get_all_channels()                       # r, g, b, a
    lin = unreal.MovieSceneKeyInterpolation.LINEAR
    for q in range(end + 1):
        lp, color = light_at(q / FPS)
        pos = [lp[0] * 100.0, lp[2] * 100.0, lp[1] * 100.0]     # WebGL → Unreal
        for k in range(3):
            ct[k].add_key(unreal.FrameNumber(q), pos[k], interpolation=lin)
        m = max(max(color), 1e-6)
        ci.add_key(unreal.FrameNumber(q), m * candela_per_unit, interpolation=lin)
        for k in range(3):
            cc[k].add_key(unreal.FrameNumber(q), color[k] / m, interpolation=lin)
        cc[3].add_key(unreal.FrameNumber(q), 1.0, interpolation=lin)
    log(label, 'peak', '%.3g cd' % (max(max(light_at(q / FPS)[1]) for q in range(end + 1)) * candela_per_unit))


if __name__ == '__main__':
    mat = material() if volume() else None
    scene(mat)
    unreal.EditorAssetLibrary.save_directory('/Game/', only_if_is_dirty=True, recursive=True)
    log('done')
