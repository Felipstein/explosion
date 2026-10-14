// FXLab: o fogo da carcaça do jeito dos jogos (cartões 6-way de uma simulação gravada).
//
// A lógica (quando nasce cada bolota, como sobe, que quadro toca, sombra entre cartões)
// está em fxfire.js, que é a especificação: a Unreal (FXLab/Source) repete ela igual.
// Aqui fica só o desenho: cada cartão toca o cozimento do FXLab/Tools/bake_sixway.py
// (a Gasoline Explosion da JangaFX, CC0) e o material mistura as 6 respostas à luz
// pela direção do sol (six-way lighting: Battlefront II, Unity VFX, exportação do
// EmberGen). O ajuste vem do FXLab/preset.json, o mesmo que a Unreal lê, e a curva
// de tela é a da Unreal (uefilmic.js): o que aparece aqui é a cor do jogo.
//
// Eixos: a lógica usa os da Unreal em metros (z pra cima); o three.js tem y pra cima.
// Unreal (x, y, z) ↔ three (x, z, y).
import * as THREE from 'three';
import { EXRLoader } from 'three/addons/loaders/EXRLoader.js';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { EffectComposer } from 'three/addons/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/addons/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/addons/postprocessing/UnrealBloomPass.js';
import { OutputPass } from 'three/addons/postprocessing/OutputPass.js';
import { ShaderPass } from 'three/addons/postprocessing/ShaderPass.js';
import { FXFire, MAX_CARDS, sunDir } from './fxfire.js';
import { UEFilmicShader } from './uefilmic.js';
import { FXImpact, IMPACT_DEFAULTS } from './fximpact.js';
import { FXTread, TREAD_DEFAULTS } from './fxtread.js';

const ROOT = '../../';
const TANK = ROOT + 'captures/concept/30_to_3d/usa_tank/parts_v2/';
// a areia: a foto do WorldForge (Intermediate, pode ter sido limpa); senão, a areia procedural do laboratório
const SAND = ROOT + (Q_SAND() || 'captures/fxlab/sand_lab.jpg');
function Q_SAND() { return new URLSearchParams(location.search).get('sand'); }
const VIDEO = ROOT + 'captures/fxlab/ref_v3_fogo.mp4';      // o fogo da carcaça do gameplay_v3, de 6 a 10 s
const TANK_LEN = 7.9;
const Q = new URLSearchParams(location.search);
const FX = Q_FX();                  // 'fire' (fogo da carcaça), 'impact' (tiro no chão) ou 'tread' (poeira do andar)
function Q_FX() { return new URLSearchParams(location.search).get('fx') || 'fire'; }
const toThree = p => new THREE.Vector3(p[0], p[2], p[1]);
const toUE = v => [v.x, v.z, v.y];

// gaveta de depuração (o jogador não vê nada disso); os valores vêm do preset.json
const P = { erode: 0.25 };
const SLIDERS = [
  ['interval', 'intervalo s', 0.1, 1.0, 0.01], ['scale', 'tamanho', 0.2, 1.2, 0.01], ['speed', 'relógio', 0.5, 3, 0.05],
  ['rise', 'subida m/s', 0, 6, 0.1], ['smokeLife', 'fumaça s', 1, 15, 0.5], ['smokeGrow', 'fumaça cresce', 0, 1, 0.01],
  ['density', 'densidade', 0.5, 3, 0.05], ['erode', 'fumaça se desfaz', 0, 0.9, 0.01], ['emberCut', 'corte da brasa', 0, 3, 0.05],
  ['albedo', 'cor da fuligem', 0.05, 1.5, 0.01], ['shadow', 'sombra entre nuvens', 0, 1, 0.01],
  ['lickInterval', 'lambidas s', 0.02, 0.5, 0.01], ['baseR', 'anel m', 0, 3, 0.05], ['fireLight', 'luz do fogo', 0, 300, 1],
  ['emit', 'fogo', 0, 30, 0.1], ['sun', 'sol', 0, 8, 0.1], ['sky', 'céu', 0, 3, 0.05], ['sunElev', 'sol altura°', 2, 80, 1],
  ['sunAz', 'sol giro°', -180, 180, 1], ['wind', 'vento m/s', 0, 4, 0.1], ['exposure', 'exposição', 0.2, 3, 0.05],
];

const view = document.getElementById('view');
const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.setSize(innerWidth, innerHeight);
renderer.shadowMap.enabled = true;
renderer.shadowMap.type = THREE.PCFSoftShadowMap;
// curva de tela: a da Unreal 5.8 (Filmic, a do jogo), portada em uefilmic.js; ?curve=aces volta o ACES do three.js
const UE_CURVE = (new URLSearchParams(location.search).get('curve') || 'ue') !== 'aces';
renderer.toneMapping = UE_CURVE ? THREE.NoToneMapping : THREE.ACESFilmicToneMapping;
view.appendChild(renderer.domElement);

const scene = new THREE.Scene();
const HAZE = new THREE.Color(0.78, 0.62, 0.48);
// céu de fim de tarde com poeira: névoa quente no horizonte, azul apagado em cima, brilho em volta do sol
const skyMat = new THREE.ShaderMaterial({
  uniforms: { uSun: { value: new THREE.Vector3(0, 1, 0) }, uHaze: { value: HAZE.clone() } },
  vertexShader: 'varying vec3 vD; void main(){ vD = normalize(position); gl_Position = projectionMatrix * modelViewMatrix * vec4(position,1.0); }',
  fragmentShader: `uniform vec3 uSun, uHaze; varying vec3 vD;
    void main(){
      float h = max(vD.y, 0.0);
      vec3 zen = vec3(0.30, 0.42, 0.62);
      vec3 c = mix(uHaze, zen, pow(h, 0.55));
      float g = max(dot(normalize(vD), uSun), 0.0);
      c += vec3(1.0, 0.62, 0.32) * (0.9 * pow(g, 64.0) + 0.35 * pow(g, 6.0));
      gl_FragColor = vec4(c, 1.0);
    }`,
  side: THREE.BackSide, depthWrite: false, fog: false,
});
scene.add(new THREE.Mesh(new THREE.SphereGeometry(900, 32, 16), skyMat));
scene.fog = new THREE.FogExp2(HAZE.clone(), 0.0045);
const camera = new THREE.PerspectiveCamera(32, innerWidth / innerHeight, 0.5, 2000);
const controls = new OrbitControls(camera, renderer.domElement);
controls.enableDamping = true;

const sunLight = new THREE.DirectionalLight(0xffc68c, 1);
sunLight.castShadow = true;
sunLight.shadow.mapSize.set(2048, 2048);
Object.assign(sunLight.shadow.camera, { left: -30, right: 30, top: 30, bottom: -30, near: 1, far: 300 });
sunLight.shadow.bias = -0.0005;
scene.add(sunLight, sunLight.target);
scene.add(new THREE.HemisphereLight(0x9fb4d0, 0xb08a64, 0.7));
const fireLight = new THREE.PointLight(0xff8a3a, 0, 30, 2);
scene.add(fireLight);

// partícula macia (soft particles): o cartão some perto de onde encosta no chão ou no tanque.
// A profundidade da cena opaca sai num alvo à parte a cada quadro (a Unreal usa o DepthFade).
const depthRT = new THREE.WebGLRenderTarget(4, 4);
depthRT.depthTexture = new THREE.DepthTexture(4, 4);
const composer = new EffectComposer(renderer, new THREE.WebGLRenderTarget(innerWidth, innerHeight, { type: THREE.HalfFloatType }));
composer.addPass(new RenderPass(scene, camera));
composer.addPass(new UnrealBloomPass(new THREE.Vector2(innerWidth, innerHeight), 0.15, 0.3, 2.0));
const filmic = new ShaderPass(UEFilmicShader);
composer.addPass(UE_CURVE ? filmic : new OutputPass());

// chão de areia
const sandTex = new THREE.TextureLoader().load(SAND);
sandTex.colorSpace = THREE.SRGBColorSpace;
sandTex.wrapS = sandTex.wrapT = THREE.RepeatWrapping;
sandTex.repeat.set(30, 30);
sandTex.anisotropy = 8;
const ground = new THREE.Mesh(new THREE.PlaneGeometry(400, 400),
  new THREE.MeshStandardMaterial({ map: sandTex, roughness: 1, color: 0xd8c0a0 }));
ground.rotation.x = -Math.PI / 2;
ground.receiveShadow = true;
scene.add(ground);
// a poeira do andar testa outros chãos: ?ground=snow|asphalt|mud (a areia é o padrão)
{
  const g = Q.get('ground');
  if (g === 'snow') { ground.material.color.set(0xf4f6fa); ground.material.map = null; }
  if (g === 'asphalt') { ground.material.color.set(0x5c5a57); ground.material.map = null; }
  if (g === 'mud') { ground.material.color.set(0x8a7058); }
}

// tanque (as 5 peças já vêm montadas no lugar); devolve o anel da torre nos eixos da Unreal.
// O pai (tankRoot) é o que anda na poeira do andar; parado, fica na origem.
const tankRoot = new THREE.Group();
scene.add(tankRoot);
async function loadTank() {
  const gl = new GLTFLoader();
  const group = new THREE.Group();
  let turret = null;
  for (const part of ['Hull', 'Turret', 'Barrel', 'Tracks', 'Wheels']) {
    const g = await gl.loadAsync(`${TANK}SM_USA_MBT_${part}.glb`);
    g.scene.traverse(o => { if (o.isMesh) { o.castShadow = true; o.receiveShadow = true; } });
    group.add(g.scene);
    if (part === 'Turret') turret = g.scene;
  }
  const size = new THREE.Box3().setFromObject(group).getSize(new THREE.Vector3());
  group.scale.setScalar(TANK_LEN / Math.max(size.x, size.z));      // comprimento com o cano: 7.9 m
  const b = new THREE.Box3().setFromObject(group);
  const c = b.getCenter(new THREE.Vector3());
  group.position.set(-c.x, -b.min.y, -c.z);
  tankRoot.add(group);
  tankRoot.updateMatrixWorld(true);
  const tb = new THREE.Box3().setFromObject(turret);
  // o anel da torre: o meio do topo dela, 25 cm pra dentro
  return toUE(new THREE.Vector3((tb.min.x + tb.max.x) / 2, tb.max.y - 0.25, (tb.min.z + tb.max.z) / 2));
}

// cartões 6-way: uma camada por cozimento (geometria, atributos e material próprios)
const quad = new THREE.InstancedBufferGeometry();
quad.setAttribute('position', new THREE.Float32BufferAttribute([-0.5, -0.5, 0, 0.5, -0.5, 0, 0.5, 0.5, 0, -0.5, 0.5, 0], 3));
quad.setIndex([0, 1, 2, 0, 2, 3]);
const attr = n => { const a = new THREE.InstancedBufferAttribute(new Float32Array(MAX_CARDS * 4), 4).setUsage(THREE.DynamicDrawUsage); quad.setAttribute(n, a); return a; };
const aA = attr('iA'), aB = attr('iB'), aC = attr('iC'), aD = attr('iD');
quad.instanceCount = 0;

const sixwayMat = new THREE.ShaderMaterial({
  uniforms: {
    tPos: { value: null }, tNeg: { value: null }, tEmit: { value: null },
    uGrid: { value: new THREE.Vector3(8, 8, 64) }, uBallBottom: { value: new Array(64).fill(0) },
    uSunDir: { value: new THREE.Vector3() }, uSunCol: { value: new THREE.Color() }, uSkyCol: { value: new THREE.Color() },
    uEmit: { value: 1 }, uDensity: { value: 1 }, uEmberCut: { value: 0.6 }, uAlbedo: { value: 1 },
    uTint: { value: new THREE.Color(1, 1, 1) }, uStem: { value: 1 },
    tDepth: { value: null }, uRes: { value: new THREE.Vector2(1, 1) }, uNearFar: { value: new THREE.Vector2(0.5, 2000) }, uSoft: { value: 0 },
    uFogCol: { value: HAZE.clone() }, uFogDensity: { value: 0.0045 },
  },
  vertexShader: /* glsl */`
    attribute vec4 iA;   // posição do pivô (m), lado do cartão (m)
    attribute vec4 iB;   // quadro (contínuo), giro (rad), opacidade, fogo (0–1)
    attribute vec4 iC;   // espelho (±1), deslocamento do pivô no cartão (x, y), sol que passa pelos outros
    attribute vec4 iD;   // erosão do alfa, deitado no chão (1) ou de frente pra câmera (0)
    uniform vec3 uSunDir;
    varying vec2 vUv; varying float vFrame, vAlpha, vEmit, vDepth, vT, vErode; varying vec3 vL;
    void main() {
      vec3 camR = vec3(viewMatrix[0][0], viewMatrix[1][0], viewMatrix[2][0]);
      vec3 camU = vec3(viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1]);
      vec3 camF = vec3(viewMatrix[0][2], viewMatrix[1][2], viewMatrix[2][2]);   // da cena pra câmera
      float c = cos(iB.y), s = sin(iB.y);
      vec3 R = c * camR + s * camU, U = -s * camR + c * camU;
      if (iD.y > 0.5) {
        // deitado no chão (anel de choque gravado de cima): o cartão é o plano do chão, a frente é o céu
        R = vec3(c, 0.0, s); U = vec3(-s, 0.0, c); camF = vec3(0.0, 1.0, 0.0);
      }
      vec2 q = position.xy - iC.yz;                       // o pivô (a bola no 1º quadro) fica em iA.xyz
      vec3 wp = iA.xyz + (R * q.x * max(iD.z, 1e-3) + U * q.y) * iA.w;     // iD.z: esticado ao longo da direita do cartão
      vUv = position.xy + 0.5;
      if (iC.x < 0.0) vUv.x = 1.0 - vUv.x;
      vFrame = iB.x; vAlpha = iB.z; vEmit = iB.w; vT = iC.w; vErode = iD.x;
      // luz no espaço do cartão: espelhado, a direita gravada vira a esquerda
      vL = vec3(dot(uSunDir, R) * iC.x, dot(uSunDir, U), dot(uSunDir, camF));
      vec4 mv = viewMatrix * vec4(wp, 1.0);
      vDepth = -mv.z;
      gl_Position = projectionMatrix * mv;
    }`,
  // o mesmo cálculo está no M_FXSixWay da Unreal (FXLab/Tools/setup_fxlab.py, SIXWAY_HLSL)
  fragmentShader: /* glsl */`
    uniform sampler2D tPos, tNeg, tEmit;
    uniform vec3 uGrid; uniform float uBallBottom[64];
    uniform vec3 uSunCol, uSkyCol, uFogCol, uTint; uniform float uEmit, uFogDensity, uDensity, uEmberCut, uAlbedo, uStem;
    uniform sampler2D tDepth; uniform vec2 uRes, uNearFar; uniform float uSoft;
    varying vec2 vUv; varying float vFrame, vAlpha, vEmit, vDepth, vT, vErode; varying vec3 vL;
    float softFade() {
      if (uSoft <= 0.0) return 1.0;
      float d = texture2D(tDepth, gl_FragCoord.xy / uRes).x;
      float n = uNearFar.x, f = uNearFar.y;
      float sceneZ = (n * f) / ((f - n) * d - f);          // profundidade de vista (negativa)
      return clamp((-sceneZ - vDepth) / uSoft, 0.0, 1.0);
    }
    vec2 cell(float f, vec2 uv) {
      // o EXRLoader entrega de baixo pra cima (como o GL): a linha 0 do atlas é a última
      float col = mod(f, uGrid.x), row = floor(f / uGrid.x);
      return vec2((col + uv.x) / uGrid.x, (uGrid.y - 1.0 - row + uv.y) / uGrid.y);
    }
    void main() {
      float f0 = floor(vFrame), f1 = min(f0 + 1.0, uGrid.z - 1.0), t = vFrame - f0;
      vec2 a = cell(f0, vUv), b = cell(f1, vUv);
      vec4 Pp = mix(texture2D(tPos, a), texture2D(tPos, b), t);
      vec4 Nn = mix(texture2D(tNeg, a), texture2D(tNeg, b), t);
      vec3 E = mix(texture2D(tEmit, a).rgb, texture2D(tEmit, b).rgb, t);
      // caule: a Gasoline Explosion traz a fumaça que sobe do chão por baixo da bola; o cartão
      // esconde abaixo do fundo da bola (medido por quadro: atlas.json, ball_bottom)
      float bb = mix(uBallBottom[int(f0)], uBallBottom[int(f1)], t) * uStem;   // a poeira não tem caule
      float keep = smoothstep(bb - 0.08, bb, vUv.y);
      // erosão: as bordas finas somem primeiro (a fumaça se desfaz em vez de só ficar transparente)
      float ae = clamp((Pp.a * keep - vErode) / max(1.0 - vErode, 1e-3), 0.0, 1.0);
      keep *= Pp.a > 1e-4 ? ae / max(Pp.a * keep, 1e-4) : 0.0;
      Pp *= keep; Nn *= keep; E *= keep;
      // densidade: cobertura a' = 1 − (1 − a)^k; a luz da fumaça escala junto (pré-multiplicado)
      float alpha = 1.0 - pow(max(1.0 - Pp.a, 0.0), uDensity);
      float dk = Pp.a > 1e-4 ? alpha / Pp.a : 1.0;
      // six-way: pesos = componentes² da direção do sol (somam 1)
      vec3 w = vL * vL;
      float sunR = (vL.x > 0.0 ? Pp.r : Nn.r) * w.x + (vL.y > 0.0 ? Pp.g : Nn.g) * w.y + (vL.z > 0.0 ? Pp.b : Nn.b) * w.z;
      // céu: mais de cima, um pouco dos lados, quase nada de baixo
      float skyR = 0.5 * Pp.g + 0.125 * (Pp.r + Nn.r + Pp.b + Nn.b) + 0.05 * Nn.g;
      // brasa: o corpo negro abaixo de ~1100 K só avermelha a fumaça; fica só a rachadura quente
      float eL = dot(E, vec3(0.2126, 0.7152, 0.0722));
      E *= smoothstep(uEmberCut * 0.5, uEmberCut * 1.5, eL * uEmit);
      // o céu chega menos no meio da coluna (metade do efeito da sombra do sol)
      vec3 col = (uSunCol * sunR * vT + uSkyCol * skyR * mix(0.5, 1.0, vT)) * dk * uAlbedo * uTint + E * uEmit * vEmit;
      float fog = 1.0 - exp(-uFogDensity * uFogDensity * vDepth * vDepth);
      col = mix(col, uFogCol * alpha, fog);
      gl_FragColor = vec4(col, alpha) * vAlpha * softFade();          // pré-multiplicado
    }`,
  transparent: true, depthWrite: false, side: THREE.DoubleSide,   // o anel deitado tem a frente pra baixo
  
  blending: THREE.CustomBlending, blendEquation: THREE.AddEquation,
  blendSrc: THREE.OneFactor, blendDst: THREE.OneMinusSrcAlphaFactor,
  blendSrcAlpha: THREE.OneFactor, blendDstAlpha: THREE.OneMinusSrcAlphaFactor,
});
function makeLayer(order) {
  const geo = new THREE.InstancedBufferGeometry();
  geo.setAttribute('position', quad.getAttribute('position'));
  geo.setIndex(quad.getIndex());
  const at = {};
  for (const n of ['iA', 'iB', 'iC', 'iD']) {
    at[n] = new THREE.InstancedBufferAttribute(new Float32Array(MAX_CARDS * 4), 4).setUsage(THREE.DynamicDrawUsage);
    geo.setAttribute(n, at[n]);
  }
  geo.instanceCount = 0;
  const mat = sixwayMat.clone();
  const mesh = new THREE.Mesh(geo, mat);
  mesh.frustumCulled = false;
  mesh.renderOrder = order;
  scene.add(mesh);
  return { geo, at, mat, mesh, pivot: [0.5, 0.5] };
}
// o fogo da carcaça usa a primeira; o tiro no chão, as três (anel por baixo, depois poeira, depois o jato)
const LAYERS = { fire: null, ring: null, dust: null, geyser: null };
const cardsMesh = { set visible(v) { for (const l of Object.values(LAYERS)) if (l) l.mesh.visible = v; } };

// primitivas do impacto (clarão, faíscas, jato de areia, grãos, cratera): um retângulo por peça,
// montado no vértice conforme o tipo. O mesmo cálculo está no M_FXPrim da Unreal (PRIM_HLSL).
// Mistura pré-multiplicada: as que somam luz (clarão, faísca) saem com alfa 0.
const MAX_PRIMS = 512;
const pquad = new THREE.InstancedBufferGeometry();
pquad.setAttribute('position', new THREE.Float32BufferAttribute([-0.5, -0.5, 0, 0.5, -0.5, 0, 0.5, 0.5, 0, -0.5, 0.5, 0], 3));
pquad.setIndex([0, 1, 2, 0, 2, 3]);
const pattr = n => { const a = new THREE.InstancedBufferAttribute(new Float32Array(MAX_PRIMS * 4), 4).setUsage(THREE.DynamicDrawUsage); pquad.setAttribute(n, a); return a; };
const pA = pattr('pA'), pB = pattr('pB'), pC = pattr('pC'), pD = pattr('pD');
pquad.instanceCount = 0;
const PRIM_KIND = { flash: 0, spark: 1, jet: 2, chunk: 3, crater: 4 };
const primMat = new THREE.ShaderMaterial({
  uniforms: { uSunCol: { value: new THREE.Color() }, uSkyCol: { value: new THREE.Color() },
    tDepth: { value: null }, uRes: { value: new THREE.Vector2(1, 1) }, uNearFar: { value: new THREE.Vector2(0.5, 2000) }, uSoft: { value: 0.4 } },
  vertexShader: /* glsl */`
    attribute vec4 pA;   // centro (m), tipo
    attribute vec4 pB;   // direção (riscos), comprimento
    attribute vec4 pC;   // largura, tamanho, ganho (luz), opacidade
    attribute vec4 pD;   // cor (riscos escuros e grãos)
    varying vec2 vUv; varying float vKind, vGain, vAlpha, vDepth; varying vec3 vCol;
    void main() {
      vec3 camR = vec3(viewMatrix[0][0], viewMatrix[1][0], viewMatrix[2][0]);
      vec3 camU = vec3(viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1]);
      vec3 camF = vec3(viewMatrix[0][2], viewMatrix[1][2], viewMatrix[2][2]);
      float k = pA.w;
      vec3 wp;
      if (k < 0.5 || (k > 2.5 && k < 3.5)) {
        float s = k < 0.5 ? pC.y : pC.x;                 // clarão: tamanho; grão: largura
        wp = pA.xyz + (camR * position.x + camU * position.y) * s;
      } else if (k < 2.5) {
        // risco: do ponto pra trás ao longo da velocidade, largura de frente pra câmera
        vec3 d = normalize(pB.xyz);
        vec3 side = normalize(cross(d, camF) + vec3(1e-5));
        wp = pA.xyz + d * (position.y - 0.5) * pB.w + side * position.x * pC.x;
      } else {
        wp = pA.xyz + vec3(position.x, 0.0, position.y) * pC.y;     // cratera: deitada no chão
      }
      vUv = position.xy + 0.5;
      vKind = k; vGain = pC.z; vAlpha = pC.w; vCol = pD.rgb;
      vec4 mv = viewMatrix * vec4(wp, 1.0);
      vDepth = -mv.z;
      gl_Position = projectionMatrix * mv;
    }`,
  fragmentShader: /* glsl */`
    uniform vec3 uSunCol, uSkyCol;
    uniform sampler2D tDepth; uniform vec2 uRes, uNearFar; uniform float uSoft;
    varying vec2 vUv; varying float vKind, vGain, vAlpha, vDepth; varying vec3 vCol;
    float softFade() {
      if (uSoft <= 0.0 || vKind > 3.5) return 1.0;          // a cratera é deitada no chão: sem fade
      float d = texture2D(tDepth, gl_FragCoord.xy / uRes).x;
      float n = uNearFar.x, f = uNearFar.y;
      float sceneZ = (n * f) / ((f - n) * d - f);
      return clamp((-sceneZ - vDepth) / uSoft, 0.0, 1.0);
    }
    float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
    float noise(vec2 p) { vec2 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
      return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y); }
    void main() {
      vec2 q = vUv * 2.0 - 1.0;
      float r2 = dot(q, q);
      vec4 o = vec4(0.0);
      if (vKind < 0.5) {                                   // clarão: miolo branco, borda amarela
        float g = (exp(-r2 * 9.0) + 0.35 * exp(-r2 * 3.5)) * (1.0 - smoothstep(0.55, 1.0, sqrt(r2)));
        o = vec4(vec3(1.0, 0.86, 0.62) * g * vGain, 0.0);
      } else if (vKind < 1.5) {                            // faísca: risco quente, ponta mais forte
        float a = (1.0 - q.x * q.x) * smoothstep(0.0, 0.6, vUv.y);
        o = vec4(vec3(1.0, 0.68, 0.32) * a * vGain, 0.0);
      } else if (vKind < 2.5) {                            // jato de areia: risco largo, borda rasgada
        float n = noise(vec2(q.x * 3.0, vUv.y * 9.0)) * 0.6 + noise(vec2(q.x * 7.0, vUv.y * 23.0)) * 0.4;
        float rays = smoothstep(0.35, 0.65, noise(vec2(q.x * 5.0 + 3.1, vUv.y * 1.5)));     // fios ao longo do risco
        float a = (1.0 - smoothstep(0.2, 1.0, abs(q.x) + 0.45 * n)) * smoothstep(0.0, 0.25, vUv.y) * (1.0 - smoothstep(0.7, 1.0, vUv.y));
        a *= 0.45 + 0.55 * rays;
        a *= vAlpha;
        vec3 lit = vCol * (uSunCol * 0.55 + uSkyCol * 0.35);
        o = vec4(lit * a, a);
      } else if (vKind < 3.5) {                            // grão: pedrinha escura redonda
        float a = (1.0 - smoothstep(0.6, 1.0, r2)) * vAlpha;
        vec3 lit = vCol * (uSunCol * (0.5 + 0.4 * q.y) + uSkyCol * 0.35);
        o = vec4(lit * a, a);
      } else {                                             // cratera: queimado no meio, borda rasgada
        float n = noise(q * 4.0) * 0.5 + noise(q * 11.0) * 0.5;
        float a = (1.0 - smoothstep(0.2, 1.0, sqrt(r2) + 0.25 * (n - 0.5))) * vAlpha;
        vec3 c = mix(vec3(0.012, 0.011, 0.01), vec3(0.04, 0.036, 0.032), smoothstep(0.0, 0.8, sqrt(r2)));
        o = vec4(c * (uSunCol * 0.2 + uSkyCol * 0.5) * a, a * 0.85);
      }
      gl_FragColor = o * softFade();
    }`,
  transparent: true, depthWrite: false,
  blending: THREE.CustomBlending, blendEquation: THREE.AddEquation,
  blendSrc: THREE.OneFactor, blendDst: THREE.OneMinusSrcAlphaFactor,
  blendSrcAlpha: THREE.OneFactor, blendDstAlpha: THREE.OneMinusSrcAlphaFactor,
});
primMat.side = THREE.DoubleSide;       // a cratera deitada tem a frente pra baixo
primMat.polygonOffset = true;          // a cratera rente ao chão não some na distância
primMat.polygonOffsetFactor = -4;
primMat.polygonOffsetUnits = -4;
const primsMesh = new THREE.Mesh(pquad, primMat);
primsMesh.frustumCulled = false;
primsMesh.renderOrder = 11;
scene.add(primsMesh);

let fire = null, impact = null, PI = null, tread = null, PT = null;

function drawCards(layer, cards) {
  const { aA, aB, aC, aD } = { aA: layer.at.iA, aB: layer.at.iB, aC: layer.at.iC, aD: layer.at.iD };
  const PIVOT = layer.pivot;
  for (let i = 0; i < cards.length; i++) {
    const c = cards[i], p = toThree(c.p);
    aA.setXYZW(i, p.x, p.y, p.z, c.size);
    aB.setXYZW(i, c.frame, c.rot, c.alpha, c.emit);
    aC.setXYZW(i, c.flip, c.flip > 0 ? PIVOT[0] - 0.5 : 0.5 - PIVOT[0], PIVOT[1] - 0.5, c.T);
    aD.setXYZW(i, c.erode, c.ground ? 1 : 0, c.stretch || 1, 0);
  }
  layer.geo.instanceCount = cards.length;
  aA.needsUpdate = aB.needsUpdate = aC.needsUpdate = aD.needsUpdate = true;
}

// profundidade da cena opaca (sem cartões nem primitivas) pra partícula macia
function depthPrepass() {
  const size = renderer.getDrawingBufferSize(new THREE.Vector2());
  if (depthRT.width !== size.x || depthRT.height !== size.y) depthRT.setSize(size.x, size.y);
  cardsMesh.visible = primsMesh.visible = false;
  renderer.setRenderTarget(depthRT);
  renderer.render(scene, camera);
  renderer.setRenderTarget(null);
  cardsMesh.visible = primsMesh.visible = true;
  for (const m of [primMat, ...Object.values(LAYERS).filter(Boolean).map(l => l.mat)]) {
    m.uniforms.tDepth.value = depthRT.depthTexture;
    m.uniforms.uRes.value.copy(size);
    m.uniforms.uNearFar.value.set(camera.near, camera.far);
  }
}

function draw() {
  if (tread) {
    // o tanque na pose da volta (a frente do modelo é o +x do three; yaw da Unreal → giro -yaw no y do three)
    tankRoot.position.copy(toThree(tread.pos));
    tankRoot.rotation.y = -tread.yaw + (PT.tankYaw || 0) * Math.PI / 180;
    // a câmera segue o tanque (mesmo ângulo)
    if (PT.follow) {
      const tp = toThree(tread.pos), d = tp.clone().sub(controls.target); d.y = 0;
      camera.position.add(d); controls.target.add(d); controls.update();
    }
  }
  depthPrepass();
  if (tread) {
    drawCards(LAYERS.dust, tread.outputs(camUE()));
    fireLight.intensity = 0;
    return;
  }
  if (impact) {
    const cr = new THREE.Vector3().setFromMatrixColumn(camera.matrixWorld, 0);
    const { layers, prims, light } = impact.outputs(camUE(), toUE(cr));
    for (const k of ['ring', 'dust', 'geyser']) drawCards(LAYERS[k], layers[k]);
    for (let i = 0; i < prims.length; i++) {
      const q = prims[i], p = toThree(q.p), d = q.dir ? toThree(q.dir) : new THREE.Vector3(0, 1, 0), c = q.col || [0, 0, 0];
      pA.setXYZW(i, p.x, p.y, p.z, PRIM_KIND[q.kind]);
      pB.setXYZW(i, d.x, d.y, d.z, q.len || 0);
      pC.setXYZW(i, q.w || 0, q.size || 0, q.gain || 0, q.alpha === undefined ? 1 : q.alpha);
      pD.setXYZW(i, c[0], c[1], c[2], 0);
    }
    pquad.instanceCount = prims.length;
    pA.needsUpdate = pB.needsUpdate = pC.needsUpdate = pD.needsUpdate = true;
    fireLight.position.copy(toThree(impact.origin)).add(new THREE.Vector3(0, 1.0, 0));
    fireLight.intensity = light;
    return;
  }
  const { cards, heat } = fire.outputs();
  drawCards(LAYERS.fire, cards);
  fireLight.position.copy(toThree(fire.hatch)).add(new THREE.Vector3(0, 1.2, 0));
  fireLight.intensity = P.fireLight * heat;
}

function applyParams() {
  const d = toThree(sunDir(P));
  sunLight.position.copy(d).multiplyScalar(120);
  sunLight.intensity = P.sun;
  skyMat.uniforms.uSun.value.copy(d);
  for (const [name, layer] of Object.entries(LAYERS)) if (layer) applyLayer(name, layer.mat.uniforms, d);
  const u = sixwayMat.uniforms;
  primMat.uniforms.uSunCol.value.set(0xffc68c).multiplyScalar(P.sun);
  primMat.uniforms.uSkyCol.value.set(0x9fb4d0).multiplyScalar(P.sky);
  renderer.toneMappingExposure = P.exposure;
  filmic.uniforms.uExposure.value = P.exposure;
}

function applyLayer(name, u, d) {
  u.uSunDir.value.copy(d);
  u.uSunCol.value.set(0xffc68c).multiplyScalar(P.sun);
  u.uSkyCol.value.set(0x9fb4d0).multiplyScalar(P.sky);
  u.uEmit.value = P.emit;
  u.uDensity.value = P.density;
  u.uEmberCut.value = P.emberCut;
  u.uAlbedo.value = P.albedo;
  if (PT) {                       // poeira do andar: o tom, a claridade e a densidade do chão
    const G = PT[PT.ground] || PT.sand;
    u.uDensity.value = G.density;
    u.uAlbedo.value = G.albedo;
    u.uTint.value.setRGB(G.r, G.g, G.b);
    u.uStem.value = 0;
    u.uSoft.value = 1.0;
    u.uEmit.value = 0;
    return;
  }
  if (name !== 'fire') {           // tiro no chão: poeira clara e rala cor de areia; jato escuro; sem caule
    u.uDensity.value = PI.dustDensity;
    u.uAlbedo.value = name === 'geyser' ? PI.geyserAlbedo : name === 'ring' ? PI.ringAlbedo : PI.dustAlbedo;
    u.uTint.value.setRGB(PI.sandR, PI.sandG, PI.sandB);
    u.uStem.value = 0;
    u.uSoft.value = name === 'ring' ? 0 : PI.softDist;
    u.uEmit.value = 0;
  }
}

// câmeras (eixos do three): a de RTS (55°) e a baixa (enquadrada como o vídeo)
const CAMS = {
  rts: { pos: new THREE.Vector3(0, 36, 26), target: new THREE.Vector3(0, 5, 0) },
  low: { pos: new THREE.Vector3(-4, 3.0, 26), target: new THREE.Vector3(0, 5.5, 0) },
};
function setCam(name) {
  camera.position.copy(CAMS[name].pos);
  controls.target.copy(CAMS[name].target);
  controls.update();
  document.querySelectorAll('[data-cam]').forEach(b => b.classList.toggle('on', b.dataset.cam === name));
}

function buildSliders() {
  const sl = document.getElementById('sliders');
  sl.innerHTML = '';
  for (const [key, label, lo, hi, step] of SLIDERS) {
    const row = document.createElement('label');
    row.innerHTML = `<span>${label}</span><input type="range" min="${lo}" max="${hi}" step="${step}" value="${P[key]}"><span>${P[key]}</span>`;
    const [, input, out] = row.children;
    input.addEventListener('input', () => { P[key] = parseFloat(input.value); out.textContent = input.value; applyParams(); });
    sl.appendChild(row);
  }
}
document.querySelectorAll('[data-cam]').forEach(b => b.addEventListener('click', () => setCam(b.dataset.cam)));
let paused = false, slow = false;
document.getElementById('pause').addEventListener('click', e => { paused = !paused; e.target.classList.toggle('on', paused); });
document.getElementById('slow').addEventListener('click', e => { slow = !slow; e.target.classList.toggle('on', slow); });
document.getElementById('save').addEventListener('click', async e => {
  const keep = await fetch('../preset.json', { cache: 'no-store' }).then(r => r.json());
  const body = JSON.stringify({ ...keep, ...P }, null, 2);
  const r = await fetch('/preset', { method: 'POST', body });
  e.target.textContent = r.ok ? 'salvo ✓ (a Unreal pega sozinha)' : 'erro ao salvar';
  setTimeout(() => { e.target.textContent = 'salvar ajuste'; }, 2500);
});

// vídeo de referência
const vid = document.getElementById('refv');
vid.src = VIDEO;
vid.loop = true;
vid.play().catch(() => {});

addEventListener('resize', () => {
  camera.aspect = innerWidth / innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(innerWidth, innerHeight);
  composer.setSize(innerWidth, innerHeight);
});

function loadExr(url) {
  return new EXRLoader().setDataType(THREE.HalfFloatType).loadAsync(url).then(t => {
    t.generateMipmaps = true;
    t.minFilter = THREE.LinearMipmapLinearFilter;
    t.magFilter = THREE.LinearFilter;
    t.needsUpdate = true;
    return t;
  });
}

// ?shot=nome&at=2,4,6&cam=rts: passo fixo de 1/30 s (a mesma história), foto em cada instante;
// anda sem esperar a tela (funciona com o painel escondido)
const SHOT = Q.get('shot'), AT = (Q.get('at') || '').split(',').filter(Boolean).map(Number);
let simT = 0, refire = 0;
// o tiro no chão repete a cada 5 s no laboratório (na captura, um tiro só, em t = 0)
function stepFx(dt) {
  if (tread) { tread.step(dt); return; }
  if (impact) {
    impact.step(dt);
    refire += dt;
    if (!SHOT && refire > 5) { refire = 0; impact.fire(); }
    return;
  }
  fire.step(dt, camUE());
}
function camUE() { return toUE(camera.position); }
function save(name) {
  return new Promise(res => renderer.domElement.toBlob(b =>
    fetch('/save?name=' + encodeURIComponent(name), { method: 'POST', body: b }).then(res), 'image/png'));
}
async function runShots() {
  const [w, h] = (Q.get('size') || '1280x720').split('x').map(Number);   // o painel escondido tem 0 × 0
  renderer.setPixelRatio(1);
  renderer.setSize(w, h, false);
  composer.setPixelRatio(1);
  composer.setSize(w, h);
  camera.aspect = w / h;
  camera.updateProjectionMatrix();
  controls.update();
  for (const t of AT) {
    while (simT < t - 1e-6) { stepFx(1 / 30); simT += 1 / 30; }
    draw();
    composer.render();
    await save(`${SHOT}_${t.toFixed(2)}s.png`);
  }
  document.title = 'FXLab: capturas prontas';
}

let last = performance.now(), fpsAvg = 60;
const stats = document.getElementById('stats');
function loop(now) {
  const dt = Math.min(0.05, (now - last) / 1000);
  last = now;
  fpsAvg += (1 / Math.max(dt, 1e-3) - fpsAvg) * 0.05;
  controls.update();
  if (!paused) stepFx(dt * (slow ? 0.25 : 1));
  draw();
  composer.render();
  if (tread) stats.textContent = `poeira do andar · ${tread.cards.length} cartões · ${PT.speed} m/s · ${fpsAvg.toFixed(0)} qps`;
  else if (impact) stats.textContent = `tiro no chão · ${impact.t.toFixed(2)} s · ${fpsAvg.toFixed(0)} qps`;
  else stats.textContent = `${fire.cards.length} cartões · ${fpsAvg.toFixed(0)} qps · bolota nasce em (${fire.hatch.map(x => x.toFixed(1)).join(', ')}) m`;
  requestAnimationFrame(loop);
}

async function main() {
  Object.assign(P, await fetch('../preset.json', { cache: 'no-store' }).then(r => r.json()));
  for (const [k, v] of Q) if (k in P && k !== 'bake') P[k] = parseFloat(v);
  if (FX === 'tread') {
    // o ajuste: tread.json (a versão EmberGen) ou ?treadProfile=tread_handmade.json (o plano B, feito à mão)
    const prof = await fetch('../' + (Q.get('treadProfile') || 'tread.json'), { cache: 'no-store' }).then(r => r.ok ? r.json() : {}).catch(() => ({}));
    PT = { ...TREAD_DEFAULTS, ...prof, follow: 1, tankYaw: 0 };
    for (const [k, v] of Q) if (k in PT && typeof PT[k] === 'number') PT[k] = parseFloat(v);
    if (Q.get('ground')) PT.ground = Q.get('ground');
  }
  if (FX === 'impact') {
    const impactProfile = Q.get('impactProfile') || 'impact.json';
    PI = { ...IMPACT_DEFAULTS, ...(await fetch('../' + impactProfile, { cache: 'no-store' }).then(r => r.json())) };
    for (const [k, v] of Q) if (k in PI && typeof PI[k] === 'number') PI[k] = parseFloat(v);
  }
  const loadLayer = async (name, bakeName, order) => {
    const bake = ROOT + 'captures/fxlab/bake/' + bakeName + '/';
    const [meta, tPos, tNeg, tEmit] = await Promise.all([
      fetch(bake + 'atlas.json', { cache: 'no-store' }).then(r => r.json()),
      loadExr(bake + 'six_pos.exr'), loadExr(bake + 'six_neg.exr'), loadExr(bake + 'emit.exr')]);
    const layer = makeLayer(order);
    const u = layer.mat.uniforms;
    u.tPos.value = tPos; u.tNeg.value = tNeg; u.tEmit.value = tEmit;
    u.uGrid.value.set(meta.cols, meta.rows, meta.frames);
    u.uBallBottom.value = meta.ball_bottom;
    layer.pivot = meta.pivot;
    LAYERS[name] = layer;
    return meta;
  };
  const metas = {};
  const hatchP = loadTank();
  if (PT) {
    metas.dust = await loadLayer('dust', PT.bake, 10);
  } else if (PI) {
    metas.ring = await loadLayer('ring', PI.ringBake, 9);
    metas.dust = await loadLayer('dust', PI.dustBake, 10);
    metas.geyser = await loadLayer('geyser', PI.geyserBake, 10.5);
  } else {
    metas.fire = await loadLayer('fire', Q.get('bake') || P.bake, 10);
  }
  const hatch = await hatchP;
  const meta = metas.fire || metas.dust;
  if (PT) {
    tread = new FXTread(metas.dust, PT, parseInt(Q.get('seed') || '5', 10));
    // aquece: a volta já com o rastro formado (a foto não começa com o chão limpo)
    for (let i = 0; i < 6 * 30; i++) tread.step(1 / 30);
    document.querySelector('#panel h1').textContent = 'Poeira do andar · 6-way';
    document.getElementById('ref').style.display = 'none';
  } else if (PI) {
    impact = new FXImpact(metas, PI, [PI.originX, PI.originY, 0], parseInt(Q.get('seed') || '11', 10));
    impact.fire();
    document.querySelector('#panel h1').textContent = 'Tiro de tanque no chão · 6-way';
  } else {
    fire = new FXFire(meta, P, hatch, parseInt(Q.get('seed') || '7', 10));
  }
  document.getElementById('msg').remove();
  buildSliders();
  applyParams();
  setCam(Q.get('cam') || 'rts');
  if (SHOT) return runShots();
  requestAnimationFrame(loop);
}

main().catch(e => {
  console.error(e);
  const m = document.getElementById('msg');
  if (m) m.textContent = 'erro: ' + e.message; else document.title = 'erro: ' + e.message;
});
