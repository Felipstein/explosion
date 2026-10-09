// A curva de tela da Unreal 5.8 (tonemapper "Filmic", o padrão; é a que o jogo usa no WorldForge),
// portada do fonte da engine pra o laboratório mostrar a mesma cor da Unreal:
//   Engine/Shaders/Private/PostProcessCombineLUTs.usf  (CombineLUTsCommon, ComputeFilmColorNoGamma)
//   Engine/Shaders/Private/TonemapCommon.ush           (FilmToneMap)
//   Engine/Shaders/Private/ACES/ACESCommon.ush         (matrizes, glow, red modifier)
// Parâmetros = os padrões do pós-processo: inclinação 0.88, pé 0.55, ombro 0.26, corte preto 0,
// corte branco 0.04, correção de azul 0.6, expansão de gama 1, gama de tela 2.2 (sem ajuste).
// Entrada: cor linear sRGB já exposta; saída: sRGB codificado. A Unreal faz isso numa LUT 32³;
// aqui é direto (a diferença é a interpolação da LUT).

// matrizes linha a linha, como no HLSL (mul(M, v) = M·v)
const AP0_2_XYZ = [[0.9525523959, 0.0, 0.0000936786], [0.3439664498, 0.7281660966, -0.0721325464], [0.0, 0.0, 1.0088251844]];
const XYZ_2_AP0 = [[1.0498110175, 0.0, -0.0000974845], [-0.4959030231, 1.3733130458, 0.0982400361], [0.0, 0.0, 0.9912520182]];
const AP1_2_XYZ = [[0.6624541811, 0.1340042065, 0.1561876870], [0.2722287168, 0.6740817658, 0.0536895174], [-0.0055746495, 0.0040607335, 1.0103391003]];
const XYZ_2_AP1 = [[1.6410233797, -0.3248032942, -0.2364246952], [-0.6636628587, 1.6153315917, 0.0167563477], [0.0117218943, -0.0082844420, 0.9883948585]];
const AP0_2_AP1 = [[1.4514393161, -0.2365107469, -0.2149285693], [-0.0765537734, 1.1762296998, -0.0996759264], [0.0083161484, -0.0060324498, 0.9977163014]];
const AP1_2_AP0 = [[0.6954522414, 0.1406786965, 0.1638690622], [0.0447945634, 0.8596711185, 0.0955343182], [-0.0055258826, 0.0040252103, 1.0015006723]];
const XYZ_2_sRGB = [[3.2409699419, -1.5373831776, -0.4986107603], [-0.9692436363, 1.8759675015, 0.0415550574], [0.0556300797, -0.2039769589, 1.0569715142]];
const sRGB_2_XYZ = [[0.4123907993, 0.3575843394, 0.1804807884], [0.2126390059, 0.7151686788, 0.0721923154], [0.0193308187, 0.1191947798, 0.9505321522]];
const D65_2_D60 = [[1.0130349146, 0.0061052578, -0.0149709436], [0.0076982301, 0.9981633521, -0.0050320385], [-0.0028413174, 0.0046851567, 0.9245061375]];
const D60_2_D65 = [[0.9872240087, -0.0061132286, 0.0159532883], [-0.0075983718, 1.0018614847, 0.0053300358], [0.0030725771, -0.0050959615, 1.0816806031]];
const WIDE_2_XYZ = [[0.5441691, 0.2395926, 0.1666943], [0.2394656, 0.7021530, 0.0583814], [-0.0023439, 0.0361834, 1.0552183]];
const BLUE = [[0.9404372683, -0.0183068787, 0.0778696104], [0.0083786969, 0.8286599939, 0.1629613092], [0.0005471261, -0.0008833746, 1.0003362486]];
const BLUE_INV = [[1.06318, 0.0233956, -0.0865726], [-0.0106337, 1.20632, -0.19569], [-0.000590887, 0.00105248, 0.999538]];

const mul = (A, B) => A.map(r => [0, 1, 2].map(j => r[0] * B[0][j] + r[1] * B[1][j] + r[2] * B[2][j]));
// mat3 do GLSL é por coluna: mat3(linha0, linha1, linha2) e v * M = M·v (o mul do HLSL)
const glsl = M => `mat3(${M.map(r => `vec3(${r.map(x => x.toFixed(10)).join(', ')})`).join(', ')})`;

const sRGB_2_AP1 = mul(XYZ_2_AP1, mul(D65_2_D60, sRGB_2_XYZ));
const AP1_2_sRGB = mul(XYZ_2_sRGB, mul(D60_2_D65, AP1_2_XYZ));
const EXPAND = mul(mul(XYZ_2_AP1, WIDE_2_XYZ), AP1_2_sRGB);
const BLUE_AP1 = mul(AP0_2_AP1, mul(BLUE, AP1_2_AP0));
const BLUE_INV_AP1 = mul(AP0_2_AP1, mul(BLUE_INV, AP1_2_AP0));

export const UEFilmicShader = {
  name: 'UEFilmic',
  uniforms: { tDiffuse: { value: null }, uExposure: { value: 1.0 } },
  vertexShader: 'varying vec2 vUv; void main(){ vUv = uv; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }',
  fragmentShader: /* glsl */`
    uniform sampler2D tDiffuse; uniform float uExposure; varying vec2 vUv;
    const mat3 sRGB_2_AP1 = ${glsl(sRGB_2_AP1)};
    const mat3 AP1_2_sRGB = ${glsl(AP1_2_sRGB)};
    const mat3 EXPAND = ${glsl(EXPAND)};
    const mat3 BLUE_AP1 = ${glsl(BLUE_AP1)};
    const mat3 BLUE_INV_AP1 = ${glsl(BLUE_INV_AP1)};
    const mat3 AP1_2_AP0 = ${glsl(AP1_2_AP0)};
    const mat3 AP0_2_AP1 = ${glsl(AP0_2_AP1)};
    const vec3 AP1_RGB2Y = vec3(0.2722287168, 0.6740817658, 0.0536895174);
    const float FilmSlope = 0.88, FilmToe = 0.55, FilmShoulder = 0.26, FilmBlackClip = 0.0, FilmWhiteClip = 0.04;

    float rgb_2_saturation(vec3 c) {
      float mn = min(min(c.r, c.g), c.b), mx = max(max(c.r, c.g), c.b);
      return (max(mx, 1e-10) - max(mn, 1e-10)) / max(mx, 1e-2);
    }
    float rgb_2_hue(vec3 c) {
      float h = (c.r == c.g && c.g == c.b) ? 0.0 : (180.0 / 3.14159265) * atan(sqrt(3.0) * (c.g - c.b), 2.0 * c.r - c.g - c.b);
      if (h < 0.0) h += 360.0;
      return clamp(h, 0.0, 360.0);
    }
    float center_hue(float h, float c) { float x = h - c; if (x < -180.0) x += 360.0; else if (x > 180.0) x -= 360.0; return x; }
    float glow_fwd(float yc, float gain, float mid) {
      if (yc <= 2.0 / 3.0 * mid) return gain;
      if (yc >= 2.0 * mid) return 0.0;
      return gain * (mid / yc - 0.5);
    }
    float rgb_2_yc(vec3 c) {
      float chroma = sqrt(max(c.b * (c.b - c.g) + c.g * (c.g - c.r) + c.r * (c.r - c.b), 0.0));
      return (c.b + c.g + c.r + 1.75 * chroma) / 3.0;
    }
    float sigmoid_shaper(float x) { float t = max(1.0 - abs(0.5 * x), 0.0); return 0.5 * (1.0 + sign(x) * (1.0 - t * t)); }

    vec3 FilmToneMap(vec3 colorAP1) {
      vec3 ap0 = colorAP1 * AP1_2_AP0;
      float sat = rgb_2_saturation(ap0);
      float s = sigmoid_shaper((sat - 0.4) / 0.2);
      ap0 *= 1.0 + glow_fwd(rgb_2_yc(ap0), 0.05 * s, 0.08);
      float hw = smoothstep(0.0, 1.0, 1.0 - abs(2.0 * center_hue(rgb_2_hue(ap0), 0.0) / 135.0));
      ap0.r += hw * hw * sat * (0.03 - ap0.r) * (1.0 - 0.82);
      vec3 w = max(vec3(0.0), ap0 * AP0_2_AP1);
      w = mix(vec3(dot(w, AP1_RGB2Y)), w, 0.96);
      float toeScale = 1.0 + FilmBlackClip - FilmToe, shoulderScale = 1.0 + FilmWhiteClip - FilmShoulder;
      float bt = (0.18 + FilmBlackClip) / toeScale - 1.0;
      float toeMatch = log(0.18) / log(10.0) - 0.5 * log((1.0 + bt) / (1.0 - bt)) * (toeScale / FilmSlope);
      float straightMatch = (1.0 - FilmToe) / FilmSlope - toeMatch;
      float shoulderMatch = FilmShoulder / FilmSlope - straightMatch;
      vec3 lc = log(max(w, vec3(1e-10))) / log(10.0);
      vec3 straight = FilmSlope * (lc + straightMatch);
      vec3 toe = -FilmBlackClip + (2.0 * toeScale) / (1.0 + exp((-2.0 * FilmSlope / toeScale) * (lc - toeMatch)));
      vec3 shoulder = (1.0 + FilmWhiteClip) - (2.0 * shoulderScale) / (1.0 + exp((2.0 * FilmSlope / shoulderScale) * (lc - shoulderMatch)));
      toe = mix(straight, toe, vec3(lessThan(lc, vec3(toeMatch))));
      shoulder = mix(straight, shoulder, vec3(greaterThan(lc, vec3(shoulderMatch))));
      vec3 t = clamp((lc - toeMatch) / (shoulderMatch - toeMatch), 0.0, 1.0);
      if (shoulderMatch < toeMatch) t = 1.0 - t;
      t = (3.0 - 2.0 * t) * t * t;
      vec3 tone = mix(toe, shoulder, t);
      tone = mix(vec3(dot(tone, AP1_RGB2Y)), tone, 0.93);
      return max(vec3(0.0), tone);
    }

    vec3 linearToSrgb(vec3 c) {
      return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, vec3(greaterThan(c, vec3(0.0031308))));
    }

    void main() {
      vec3 lin = max(texture2D(tDiffuse, vUv).rgb * uExposure, vec3(0.0));
      vec3 ap1 = lin * sRGB_2_AP1;
      // expande cor saturada e forte pra fora do sRGB (CombineLUTsCommon)
      float luma = dot(ap1, AP1_RGB2Y);
      vec3 chroma = ap1 / max(luma, 1e-10);
      float d2 = dot(chroma - 1.0, chroma - 1.0);
      float expand = (1.0 - exp2(-4.0 * d2)) * (1.0 - exp2(-4.0 * 1.0 * luma * luma));
      ap1 = mix(ap1, ap1 * EXPAND, expand);
      // ComputeFilmColorNoGamma: correção de azul, curva, desfaz a correção
      ap1 = mix(ap1, ap1 * BLUE_AP1, 0.6);
      ap1 = FilmToneMap(ap1);
      ap1 = mix(ap1, ap1 * BLUE_INV_AP1, 0.6);
      vec3 film = max(vec3(0.0), ap1 * AP1_2_sRGB);
      gl_FragColor = vec4(linearToSrgb(clamp(film, 0.0, 1.0)), 1.0);
    }`,
};
