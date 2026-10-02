// ---------------------------------------------------------------------------
// glsl.js — snippets GLSL compartilhados (ruído, phase functions, tonemap).
// ---------------------------------------------------------------------------

export const COMMON = `
// Origem horizontal do domínio da simulação. Declarada aqui porque várias
// grades (principal, luz, macro, grossa) compartilham o mesmo domínio e
// seriam declarações duplicadas se ficasse no prelude sufixado.
// Permite a caixa de simulação SEGUIR o ponto da detonação em vez de ficar
// cravada na origem do mundo.
uniform vec3 uDomainOrigin;

#define PI 3.14159265359
#define TAU 6.28318530718

float saturate(float x){ return clamp(x, 0.0, 1.0); }
vec3  saturate(vec3 x){ return clamp(x, 0.0, 1.0); }
float sq(float x){ return x*x; }
float maxc(vec3 v){ return max(v.x, max(v.y, v.z)); }
float luma(vec3 c){ return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

// ---- hashes -------------------------------------------------------------
float hash11(float p){ p = fract(p*0.1031); p *= p+33.33; p *= p+p; return fract(p); }
float hash12(vec2 p){ vec3 p3 = fract(vec3(p.xyx)*0.1031); p3 += dot(p3, p3.yzx+33.33); return fract((p3.x+p3.y)*p3.z); }
vec3  hash33(vec3 p){
  p = vec3(dot(p, vec3(127.1, 311.7, 74.7)),
           dot(p, vec3(269.5, 183.3, 246.1)),
           dot(p, vec3(113.5, 271.9, 124.6)));
  return fract(sin(p)*43758.5453123)*2.0-1.0;
}

// ---- simplex noise 3D (Ashima Arts / Stefan Gustavson) -------------------
vec3 mod289(vec3 x){ return x - floor(x*(1.0/289.0))*289.0; }
vec4 mod289(vec4 x){ return x - floor(x*(1.0/289.0))*289.0; }
vec4 permute(vec4 x){ return mod289(((x*34.0)+1.0)*x); }
vec4 taylorInvSqrt(vec4 r){ return 1.79284291400159 - 0.85373472095314*r; }

float snoise(vec3 v){
  const vec2 C = vec2(1.0/6.0, 1.0/3.0);
  const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);
  vec3 i  = floor(v + dot(v, C.yyy));
  vec3 x0 = v - i + dot(i, C.xxx);
  vec3 g_ = step(x0.yzx, x0.xyz);
  vec3 l_ = 1.0 - g_;
  vec3 i1 = min(g_.xyz, l_.zxy);
  vec3 i2 = max(g_.xyz, l_.zxy);
  vec3 x1 = x0 - i1 + C.xxx;
  vec3 x2 = x0 - i2 + C.yyy;
  vec3 x3 = x0 - D.yyy;
  i = mod289(i);
  vec4 p = permute(permute(permute(
             i.z + vec4(0.0, i1.z, i2.z, 1.0))
           + i.y + vec4(0.0, i1.y, i2.y, 1.0))
           + i.x + vec4(0.0, i1.x, i2.x, 1.0));
  float n_ = 0.142857142857;
  vec3 ns = n_ * D.wyz - D.xzx;
  vec4 j = p - 49.0*floor(p*ns.z*ns.z);
  vec4 x_ = floor(j*ns.z);
  vec4 y_ = floor(j - 7.0*x_);
  vec4 x = x_*ns.x + ns.yyyy;
  vec4 y = y_*ns.x + ns.yyyy;
  vec4 h = 1.0 - abs(x) - abs(y);
  vec4 b0 = vec4(x.xy, y.xy);
  vec4 b1 = vec4(x.zw, y.zw);
  vec4 s0 = floor(b0)*2.0 + 1.0;
  vec4 s1 = floor(b1)*2.0 + 1.0;
  vec4 sh = -step(h, vec4(0.0));
  vec4 a0 = b0.xzyw + s0.xzyw*sh.xxyy;
  vec4 a1 = b1.xzyw + s1.xzyw*sh.zzww;
  vec3 p0 = vec3(a0.xy, h.x);
  vec3 p1 = vec3(a0.zw, h.y);
  vec3 p2 = vec3(a1.xy, h.z);
  vec3 p3 = vec3(a1.zw, h.w);
  vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2,p2), dot(p3,p3)));
  p0 *= norm.x; p1 *= norm.y; p2 *= norm.z; p3 *= norm.w;
  vec4 m = max(0.6 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
  m = m*m;
  return 42.0 * dot(m*m, vec4(dot(p0,x0), dot(p1,x1), dot(p2,x2), dot(p3,x3)));
}

float fbm(vec3 p, int oct, float lac, float gain){
  float s = 0.0, a = 0.5, n = 0.0;
  for (int i = 0; i < 8; i++){
    if (i >= oct) break;
    s += a * snoise(p);
    n += a;
    p *= lac;
    a *= gain;
  }
  return s / max(n, 1e-4);
}

// ruído "billowy" (valor absoluto) — a assinatura de nuvem de explosão
float billow(vec3 p, int oct, float lac, float gain){
  float s = 0.0, a = 0.5, n = 0.0;
  for (int i = 0; i < 8; i++){
    if (i >= oct) break;
    s += a * abs(snoise(p));
    n += a;
    p *= lac;
    a *= gain;
  }
  return s / max(n, 1e-4);
}

// ---- curl noise: campo vetorial divergence-free ∇×ψ ---------------------
// Bridson et al., "Curl-Noise for Procedural Fluid Flow", SIGGRAPH 2007.
vec3 curlNoise(vec3 p, float eps){
  vec3 dx = vec3(eps, 0.0, 0.0);
  vec3 dy = vec3(0.0, eps, 0.0);
  vec3 dz = vec3(0.0, 0.0, eps);
  // potencial vetorial ψ com offsets decorrelacionados
  vec3 o1 = vec3(  0.0,   0.0,  0.0);
  vec3 o2 = vec3( 31.4,  17.2, 53.9);
  vec3 o3 = vec3(-42.7,  88.1, -9.3);
  float p1x = snoise(p+o1+dx), p1X = snoise(p+o1-dx);
  float p1y = snoise(p+o1+dy), p1Y = snoise(p+o1-dy);
  float p1z = snoise(p+o1+dz), p1Z = snoise(p+o1-dz);
  float p2y = snoise(p+o2+dy), p2Y = snoise(p+o2-dy);
  float p2z = snoise(p+o2+dz), p2Z = snoise(p+o2-dz);
  float p2x = snoise(p+o2+dx), p2X = snoise(p+o2-dx);
  float p3x = snoise(p+o3+dx), p3X = snoise(p+o3-dx);
  float p3y = snoise(p+o3+dy), p3Y = snoise(p+o3-dy);
  // ψ = (n1, n2, n3);  ∇×ψ
  float dP3dy = (p3y - p3Y), dP2dz = (p2z - p2Z);
  float dP1dz = (p1z - p1Z), dP3dx = (p3x - p3X);
  float dP2dx = (p2x - p2X), dP1dy = (p1y - p1Y);
  return vec3(dP3dy - dP2dz, dP1dz - dP3dx, dP2dx - dP1dy) / (2.0*eps);
}

// ---- phase functions ----------------------------------------------------
float phaseHG(float cosT, float g){
  float g2 = g*g;
  float d = 1.0 + g2 - 2.0*g*cosT;
  return (1.0 - g2) / (4.0*PI*max(d*sqrt(max(d,1e-4)), 1e-4));
}
// mistura dupla-lobo: forward scatter forte + backscatter suave
float phaseDual(float cosT, float g0, float g1, float w){
  return mix(phaseHG(cosT, g0), phaseHG(cosT, -g1), w);
}

// ---- tonemap ------------------------------------------------------------
// ACES fit de Stephen Hill (sRGB <-> ACEScg via matrizes RRT/ODT)
const mat3 ACESInput = mat3(
  0.59719, 0.07600, 0.02840,
  0.35458, 0.90834, 0.13383,
  0.04823, 0.01566, 0.83777);
const mat3 ACESOutput = mat3(
   1.60475, -0.10208, -0.00327,
  -0.53108,  1.10813, -0.07276,
  -0.07367, -0.00605,  1.07602);
vec3 RRTAndODTFit(vec3 v){
  vec3 a = v*(v+0.0245786) - 0.000090537;
  vec3 b = v*(0.983729*v + 0.4329510) + 0.238081;
  return a/b;
}
vec3 tonemapACES(vec3 c){
  c = ACESInput * c;
  c = RRTAndODTFit(c);
  c = ACESOutput * c;
  return saturate(c);
}

vec3 linearToSRGB(vec3 c){
  c = max(c, vec3(0.0));
  return mix(c*12.92, 1.055*pow(max(c, vec3(1e-5)), vec3(1.0/2.4)) - 0.055, step(0.0031308, c));
}

// dithering ordenado interleaved-gradient (Jimenez) — mata banding
float ignoise(vec2 p){
  return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}
`;

// ---------------------------------------------------------------------------
// ENV — acesso à LUT 4×1 de ambiente produzida por atmosphere.js. Tudo que a
// cena precisa saber sobre a luz do ambiente vive numa textura, nunca passa
// pela CPU (um readPixels de 1 pixel custava 21ms de stall).
// ---------------------------------------------------------------------------
export const ENVLUT = `
uniform sampler2D uEnvLut;
vec3 envKeyColor(){ return texelFetch(uEnvLut, ivec2(0, 0), 0).rgb; }  // irradiância da luz-chave
vec3 envSkyUp(){    return texelFetch(uEnvLut, ivec2(1, 0), 0).rgb; }  // céu hemisfério superior / π
vec3 envSkyDn(){    return texelFetch(uEnvLut, ivec2(2, 0), 0).rgb; }  // rebote do chão
vec3 envSunColor(){ return texelFetch(uEnvLut, ivec2(3, 0), 0).rgb; }  // disco solar
`;

// ---------------------------------------------------------------------------
// ATMOS — céu lido da LUT sky-view (atmosphere.js) + corpos celestes.
// A MESMA função serve o passe de céu e a névoa da cena, então a distância
// casa com o horizonte automaticamente em qualquer hora do dia.
// ---------------------------------------------------------------------------
export const ATMOS = `
uniform sampler2D uSkyView;
uniform vec3 uSunDir, uMoonDir;
uniform float uSkyTime, uStarBright, uNightGlow, uMoonBright;

// mesma parametrização usada pra CONSTRUIR a LUT: azimute relativo ao sol
// (a atmosfera é simétrica em volta dele) e zênite com distorção sqrt pra
// concentrar resolução no horizonte
vec2 skyViewUV(vec3 dir){
  vec3 sunH = normalize(vec3(uSunDir.x, 0.0, uSunDir.z) + vec3(1e-5, 0.0, 0.0));
  vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), sunH));
  vec3 dh = vec3(dir.x, 0.0, dir.z);
  float az = length(dh) > 1e-5 ? atan(dot(dh, right), dot(dh, sunH)) : 0.0;
  float ang = acos(clamp(dir.y, -1.0, 1.0));
  float ss = 1.0 - 2.0 * (ang / PI);
  return vec2(abs(az) / PI, clamp(0.5 - 0.5 * sign(ss) * sqrt(abs(ss)), 0.0, 1.0));
}

// radiância pura do céu — é isto que a névoa usa
vec3 skyBase(vec3 dir){
  return texture(uSkyView, skyViewUV(dir)).rgb;
}

// ---- estrelas: distribuição em lei de potência numa grade de direções ----
vec3 starField(vec3 dir){
  vec3 p = dir * 290.0;
  vec3 id = floor(p), fp = fract(p) - 0.5;
  vec3 h = hash33(id);
  float d = length(fp - h * 0.36);
  float mag = hash12(id.xy * 1.7 + id.z * 31.7);
  // pow alto = poucas brilhantes, muitas fracas (como o céu real)
  float bright = pow(mag, 16.0);
  float core = smoothstep(0.085, 0.0, d) * bright;
  // cintilação: atmosfera, não ruído branco
  float tw = 0.70 + 0.30 * sin(uSkyTime * 2.7 + mag * 97.0)
                  * sin(uSkyTime * 1.31 + mag * 41.0);
  // temperatura de cor: azuladas e alaranjadas
  float ct = hash11(mag * 53.0);
  vec3 tint = mix(vec3(1.00, 0.78, 0.60), vec3(0.72, 0.84, 1.00), smoothstep(0.25, 0.8, ct));
  return tint * core * tw * 26.0;
}

// ---- Via Láctea: banda com faixas de poeira --------------------------
vec3 milkyWay(vec3 dir){
  vec3 gn = normalize(vec3(0.43, 0.56, -0.71));
  float b = 1.0 - abs(dot(dir, gn));
  float band = smoothstep(0.68, 0.98, b);
  if (band <= 0.0) return vec3(0.0);
  float glow = 0.45 + 0.55 * fbm(dir * 5.5, 4, 2.3, 0.55);
  float dust = 1.0 - 0.75 * smoothstep(0.35, 0.75, billow(dir * 9.0 + 3.1, 4, 2.4, 0.5));
  return vec3(0.72, 0.76, 0.95) * band * glow * dust * 0.055;
}

// céu completo pro passe de fundo
vec3 skyRadiance(vec3 dir){
  vec3 col = skyBase(dir);
  // estrelas somem sozinhas quando o céu clareia — sem curva de fade manual
  // o céu diurno tem radiância ~2e-2 aqui; 2500 garante que as estrelas
  // sumam completamente de dia e voltem sozinhas no crepúsculo
  float wash = 1.0 / (1.0 + luma(col) * 2500.0);
  if (wash > 0.004 && dir.y > -0.06){
    float h = smoothstep(-0.06, 0.08, dir.y);
    col += (starField(dir) + milkyWay(dir)) * wash * h * uStarBright;
    col += vec3(0.0022, 0.0032, 0.0050) * wash * h * uNightGlow;  // airglow
  }
  return col;
}

// discos do sol e da lua + halo lunar (só no passe de céu)
vec3 celestialDisks(vec3 dir){
  vec3 col = vec3(0.0);

  // sol: radiância = irradiância / ângulo sólido (6.8e-5 sr)
  float ds = dot(dir, uSunDir);
  float sunDisk = smoothstep(0.99996, 0.999985, ds);
  col += envSunColor() * sunDisk * 2600.0;

  // lua: disco com escurecimento de bordo e manchas (mares)
  float dm = dot(dir, uMoonDir);
  float moonDisk = smoothstep(0.999955, 0.999985, dm);
  if (moonDisk > 0.0){
    vec3 t = normalize(cross(uMoonDir, vec3(0.0, 1.0, 0.0)) + vec3(1e-4));
    vec3 b = cross(uMoonDir, t);
    vec2 uv = vec2(dot(dir, t), dot(dir, b)) * 160.0;
    float rr = clamp(length(uv), 0.0, 1.0);
    float limb = sqrt(max(1.0 - rr * rr, 0.0)) * 0.45 + 0.55;
    float maria = 0.76 + 0.24 * fbm(vec3(uv * 2.2, 0.0), 4, 2.3, 0.55);
    col += vec3(1.0, 0.97, 0.92) * moonDisk * limb * maria * uMoonBright;
  }
  // halo atmosférico em volta da lua
  col += vec3(0.55, 0.68, 1.0) * pow(max(dm, 0.0), 420.0) * uMoonBright * 0.030;
  col += vec3(0.45, 0.58, 0.95) * pow(max(dm, 0.0), 24.0) * uMoonBright * 0.0022;
  return col;
}
`;

// ---------------------------------------------------------------------------
// VOLUME_SHADOW — sombra volumétrica por raymarch. Requer o prelude da grade.
// Usado pelo chão, pelos props e pelas partículas: qualquer superfície pode
// perguntar "quanta luz sobra depois de atravessar a fumaça?".
// ---------------------------------------------------------------------------
export const VOLUME_SHADOW = `
vec2 rayBox(vec3 ro, vec3 rd, vec3 bmin, vec3 bmax){
  vec3 inv = 1.0 / rd;
  vec3 t0 = (bmin - ro) * inv;
  vec3 t1 = (bmax - ro) * inv;
  vec3 tn = min(t0, t1), tf = max(t0, t1);
  return vec2(max(max(tn.x, tn.y), tn.z), min(min(tf.x, tf.y), tf.z));
}

// extinção máxima do bloco macro que contém w (amostragem pontual: o valor
// já é um MÁXIMO dilatado, então é conservador por construção)
float macroAt(sampler2D m, vec3 w){
  return fetchVolM(m, ivec3(floor(worldToVoxelM(w)))).r;
}
float macroAtOrigin(sampler2D m, vec3 w, vec3 origin){
  return fetchVolM(m, ivec3(floor(worldToVoxelAtM(w, origin)))).r;
}

// distância (em metros) até sair do bloco macro atual, na direção dir.
// DDA exato: pular exatamente até a fronteira nunca atravessa um bloco
// ocupado, ao contrário de um "passo grande" às cegas.
float macroExitOrigin(vec3 w, vec3 dir, vec3 origin){
  vec3 vp = worldToVoxelAtM(w, origin);
  vec3 dv = dir * INV_CELLM;
  vec3 nb = floor(vp) + step(vec3(0.0), dv);
  vec3 tb = (nb - vp) / dv;
  return max(min(min(tb.x, tb.y), tb.z), 0.0);
}

float macroExit(vec3 w, vec3 dir){
  vec3 vp = worldToVoxelM(w);
  vec3 dv = dir * INV_CELLM;
  vec3 nb = floor(vp) + step(vec3(0.0), dv);
  vec3 tb = (nb - vp) / dv;
  return max(min(min(tb.x, tb.y), tb.z), 0.0);
}

// transmitância de wp na direção dir por até maxDist metros, pulando vazio
// Extinção EFETIVA — tem que casar com o que sampleMedium() faz no raymarch.
//
// O render aplica erosão modulada por ruído na densidade; a sombra marchava o
// campo CRU. Resultado: fumaça residual já apagada da imagem continuava
// projetando sombra no chão, e ela sumia devagar junto com o decaimento da
// fuligem — uma sombra sem nada que a produzisse.
//
// Aqui entra a MÉDIA da erosão (o ruído tem média zero, então o termo médio é
// uErode*0.5), que é o suficiente: a sombra não precisa do detalhe de alta
// frequência, só precisa concordar com o render em quanta densidade existe.
float effExtinction(vec4 f, float sootExt, float dustExt, float erodeMean){
  float r = max(f.r - erodeMean / (1.0 + f.r * 9.0), 0.0);
  float a = max(f.a - erodeMean * 0.7 / (1.0 + f.a * 9.0), 0.0);
  return sootExt * r + dustExt * a;
}

// tauCap limita a profundidade óptica acumulada. Serve pra luz do fogo: a
// aproximação de luz pontual no centróide faz o chão logo abaixo da bola de
// fogo receber zero (o raio atravessa a bola inteira), quando na realidade
// ele é iluminado pela SUPERFÍCIE PRÓXIMA do volume emissor. Limitar tau é a
// correção barata pra essa quebra do modelo pontual a curta distância.
float volumeShadowAt(sampler2D fields, sampler2D macro, vec3 origin,
                     vec3 wp, vec3 dir, float maxDist, float sootExt, float dustExt,
                     int steps, float jitter, float tauCap, float erodeMean){
  vec2 hit = rayBox(wp, dir, domainMinAt(origin), domainMaxAt(origin));
  hit.x = max(hit.x, 0.0);
  hit.y = min(hit.y, maxDist);
  if (hit.y <= hit.x) return 1.0;
  float dt = (hit.y - hit.x) / float(steps);
  float tau = 0.0;
  float t = hit.x + dt * jitter;
  for (int i = 0; i < 48; i++){
    if (t >= hit.y) break;
    vec3 w = wp + dir * t;
    if (macroAtOrigin(macro, w, origin) < 2e-3){
      t += macroExitOrigin(w, dir, origin) + 1e-3;
      continue;
    }
    vec4 f = sampleVol(fields, worldToVoxelAt(w, origin));
    tau += effExtinction(f, sootExt, dustExt, erodeMean) * dt;
    if (tau > tauCap) break;
    t += dt;
  }
  return exp(-min(tau, tauCap));
}

// compatibilidade: usa a origem corrente de uDomainOrigin
float volumeShadow(sampler2D fields, sampler2D macro, vec3 wp, vec3 dir,
                   float maxDist, float sootExt, float dustExt,
                   int steps, float jitter, float tauCap, float erodeMean){
  return volumeShadowAt(fields, macro, uDomainOrigin, wp, dir, maxDist,
                        sootExt, dustExt, steps, jitter, tauCap, erodeMean);
}
`;


// ---------------------------------------------------------------------------
// PBR — GGX isotrópico + Smith height-correlated + Fresnel Schlick.
// ---------------------------------------------------------------------------
export const PBR = `
float D_GGX(float NoH, float a){
  float a2 = a * a;
  float d = (NoH * a2 - NoH) * NoH + 1.0;
  return a2 / max(PI * d * d, 1e-7);
}
float V_SmithGGX(float NoV, float NoL, float a){
  float a2 = a * a;
  float lv = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
  float ll = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
  return 0.5 / max(lv + ll, 1e-6);
}
vec3 F_Schlick(vec3 f0, float u){
  float f = pow(1.0 - u, 5.0);
  return f0 + (1.0 - f0) * f;
}

// BRDF completa pra uma luz direcional/pontual
vec3 brdf(vec3 N, vec3 V, vec3 L, vec3 albedo, float rough, float metal){
  vec3 H = normalize(V + L);
  float NoV = max(dot(N, V), 1e-4);
  float NoL = max(dot(N, L), 0.0);
  float NoH = saturate(dot(N, H));
  float LoH = saturate(dot(L, H));
  float a = max(rough * rough, 2e-3);
  vec3 f0 = mix(vec3(0.04), albedo, metal);
  vec3 F = F_Schlick(f0, LoH);
  vec3 spec = F * (D_GGX(NoH, a) * V_SmithGGX(NoV, NoL, a));
  vec3 diff = albedo * (1.0 - metal) * (1.0 / PI);
  return (diff * (1.0 - F) + spec) * NoL;
}

// ambiente hemisférico com oclusão: aproximação de IBL sem cubemap
vec3 ambientIBL(vec3 N, vec3 V, vec3 albedo, float rough, float metal,
                vec3 skyUp, vec3 skyDown, float ao){
  float up = N.y * 0.5 + 0.5;
  vec3 irr = mix(skyDown, skyUp, up);
  vec3 diff = albedo * (1.0 - metal) * irr * ao;
  // specular ambiente aproximado pela reflexão
  vec3 R = reflect(-V, N);
  vec3 refl = mix(skyDown, skyUp, R.y * 0.5 + 0.5);
  float NoV = max(dot(N, V), 1e-4);
  vec3 f0 = mix(vec3(0.04), albedo, metal);
  vec3 F = F_Schlick(f0, NoV) * (1.0 - rough * 0.85);
  return diff + refl * F * mix(ao, 1.0, 0.4);
}
`;
