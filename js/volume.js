// ---------------------------------------------------------------------------
// volume.js — grade 3D empacotada num atlas 2D ("flattened volume").
//
// Uma textura 2D de tilesX*nx por tilesY*ny guarda nz fatias. Vantagem sobre
// texturas 3D reais no WebGL2: um passo do solver = UMA draw call fullscreen
// (renderizar pra layers de textura 3D exigiria uma draw por fatia, ~1000
// draws/frame). Custo: a interpolação trilinear é feita à mão — 2 fetches
// bilineares em hardware + lerp em Z. Mesma qualidade, custo previsível.
// ---------------------------------------------------------------------------

export class VolumeGrid {
  /**
   * @param {number} n     resolução (cúbica)
   * @param {number} size  aresta do domínio em metros
   */
  constructor(n, size) {
    this.nx = this.ny = this.nz = n;
    // escolhe o layout de tiles mais quadrado possível pro atlas
    let best = null;
    for (let tx = 1; tx <= n; tx++) {
      const ty = Math.ceil(n / tx);
      if (tx * ty < n) continue;
      const w = tx * n, h = ty * n;
      const waste = tx * ty - n;
      const score = Math.abs(Math.log(w / h)) + waste * 0.01;
      if (!best || score < best.score) best = { tx, ty, w, h, score };
    }
    this.tilesX = best.tx;
    this.tilesY = best.ty;
    this.atlasW = best.w;
    this.atlasH = best.h;

    this.domainSize = [size, size, size];
    this.domainMin = [-size / 2, 0, -size / 2];
    this.cell = size / n; // células cúbicas
  }

  /**
   * prelude GLSL com a geometria da grade cravada como #define.
   * @param {string} s sufixo dos identificadores (permite 2 grades no mesmo shader)
   */
  glsl(s = '') {
    const f = (x) => (Number.isInteger(x) ? x.toFixed(1) : String(x));
    return `
#define GRID${s} vec3(${f(this.nx)}, ${f(this.ny)}, ${f(this.nz)})
#define GRIDI${s} ivec3(${this.nx}, ${this.ny}, ${this.nz})
#define TILES_X${s} ${this.tilesX}
#define TILES_XF${s} ${f(this.tilesX)}
#define ATLAS${s} vec2(${f(this.atlasW)}, ${f(this.atlasH)})
#define INV_ATLAS${s} vec2(${1 / this.atlasW}, ${1 / this.atlasH})
#define BASE_MIN${s} vec3(${this.domainMin[0]}, ${this.domainMin[1]}, ${this.domainMin[2]})
#define DOMAIN_MIN${s} (vec3(uDomainOrigin.x, 0.0, uDomainOrigin.z) + BASE_MIN${s})
#define DOMAIN_SIZE${s} vec3(${this.domainSize[0]}, ${this.domainSize[1]}, ${this.domainSize[2]})
#define DOMAIN_MAX${s} (DOMAIN_MIN${s} + DOMAIN_SIZE${s})
#define CELL${s} ${this.cell}
#define INV_CELL${s} ${1 / this.cell}

// --- mapeamento atlas <-> voxel ---
// coordenadas de voxel: centro do voxel i fica em i+0.5, faixa [0, GRID${s}]

vec3 fragToVoxel${s}(vec2 frag){
  vec2 tile  = floor(frag / GRID${s}.xy);
  vec2 local = frag - tile * GRID${s}.xy;
  float z = tile.y * TILES_XF${s} + tile.x;
  return vec3(local, z + 0.5);
}

vec2 tileUV${s}(vec2 xy, float zSlice){
  float z = clamp(zSlice, 0.0, GRID${s}.z - 1.0);
  float tx = mod(z, TILES_XF${s});
  float ty = floor(z * (1.0/TILES_XF${s}));
  // clampa dentro do tile: impede sangramento entre fatias vizinhas no atlas
  vec2 c = clamp(xy, vec2(0.5), GRID${s}.xy - 0.5);
  return (vec2(tx, ty) * GRID${s}.xy + c) * INV_ATLAS${s};
}

// trilinear: bilinear em hardware nas duas fatias + lerp em Z
vec4 sampleVol${s}(sampler2D tex, vec3 p){
  float zc = clamp(p.z, 0.5, GRID${s}.z - 0.5);
  float z0 = floor(zc - 0.5);
  float fz = zc - 0.5 - z0;
  return mix(texture(tex, tileUV${s}(p.xy, z0)),
             texture(tex, tileUV${s}(p.xy, z0 + 1.0)), fz);
}

ivec2 voxelTexel${s}(ivec3 v){
  v = clamp(v, ivec3(0), GRIDI${s} - 1);
  int tx = v.z % TILES_X${s};
  int ty = v.z / TILES_X${s};
  return ivec2(tx * GRIDI${s}.x + v.x, ty * GRIDI${s}.y + v.y);
}
vec4 fetchVol${s}(sampler2D tex, ivec3 v){ return texelFetch(tex, voxelTexel${s}(v), 0); }

// --- mapeamento voxel <-> mundo ---
vec3 voxelToWorld${s}(vec3 p){ return DOMAIN_MIN${s} + (p / GRID${s}) * DOMAIN_SIZE${s}; }
vec3 worldToVoxel${s}(vec3 w){ return ((w - DOMAIN_MIN${s}) / DOMAIN_SIZE${s}) * GRID${s}; }

// Variantes com a origem EXPLÍCITA. Os passes da simulação operam num único
// domínio (uDomainOrigin basta), mas a cena e o raymarch precisam consultar
// VÁRIAS explosões simultâneas, cada uma com sua caixa.
vec3 domainMinAt${s}(vec3 o){ return vec3(o.x, 0.0, o.z) + BASE_MIN${s}; }
vec3 domainMaxAt${s}(vec3 o){ return domainMinAt${s}(o) + DOMAIN_SIZE${s}; }
vec3 worldToVoxelAt${s}(vec3 w, vec3 o){
  return ((w - domainMinAt${s}(o)) / DOMAIN_SIZE${s}) * GRID${s};
}
vec3 voxelToWorldAt${s}(vec3 p, vec3 o){
  return domainMinAt${s}(o) + (p / GRID${s}) * DOMAIN_SIZE${s};
}

// distância normalizada até a borda do domínio (0 na borda, 1 no centro)
float domainFade${s}(vec3 p){
  vec3 d = min(p, GRID${s} - p) / (GRID${s} * 0.5);
  return saturate(min(min(d.x, d.y), d.z) * 14.0);
}
`;
  }
}
