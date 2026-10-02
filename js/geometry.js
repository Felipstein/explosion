// ---------------------------------------------------------------------------
// geometry.js — malhas procedurais + instanciação.
// ---------------------------------------------------------------------------

export function boxMesh() {
  const v = [], n = [], idx = [];
  const faces = [
    [[ 1, 0, 0], [0, 1, 0], [0, 0, 1]],
    [[-1, 0, 0], [0, 1, 0], [0, 0,-1]],
    [[ 0, 1, 0], [0, 0, 1], [1, 0, 0]],
    [[ 0,-1, 0], [0, 0,-1], [1, 0, 0]],
    [[ 0, 0, 1], [0, 1, 0], [-1,0, 0]],
    [[ 0, 0,-1], [0, 1, 0], [ 1,0, 0]],
  ];
  for (const [nr, up, right] of faces) {
    const base = v.length / 3;
    for (const [su, sr] of [[-1,-1], [1,-1], [1,1], [-1,1]]) {
      v.push(
        (nr[0] + up[0] * su + right[0] * sr) * 0.5,
        (nr[1] + up[1] * su + right[1] * sr) * 0.5,
        (nr[2] + up[2] * su + right[2] * sr) * 0.5);
      n.push(nr[0], nr[1], nr[2]);
    }
    idx.push(base, base + 1, base + 2, base, base + 2, base + 3);
  }
  return { pos: new Float32Array(v), nrm: new Float32Array(n), idx: new Uint16Array(idx) };
}

/** cilindro unitário: raio 0.5, altura 1, base em y=0 */
export function cylinderMesh(seg = 24) {
  const v = [], n = [], idx = [];
  // lateral
  for (let i = 0; i <= seg; i++) {
    const a = (i / seg) * Math.PI * 2;
    const cx = Math.cos(a) * 0.5, cz = Math.sin(a) * 0.5;
    v.push(cx, 0, cz); n.push(Math.cos(a), 0, Math.sin(a));
    v.push(cx, 1, cz); n.push(Math.cos(a), 0, Math.sin(a));
  }
  for (let i = 0; i < seg; i++) {
    const b = i * 2;
    idx.push(b, b + 2, b + 3, b, b + 3, b + 1);
  }
  // tampas
  for (const [y, ny] of [[1, 1], [0, -1]]) {
    const c = v.length / 3;
    v.push(0, y, 0); n.push(0, ny, 0);
    for (let i = 0; i <= seg; i++) {
      const a = (i / seg) * Math.PI * 2;
      v.push(Math.cos(a) * 0.5, y, Math.sin(a) * 0.5); n.push(0, ny, 0);
    }
    for (let i = 0; i < seg; i++) {
      if (ny > 0) idx.push(c, c + 1 + i, c + 2 + i);
      else idx.push(c, c + 2 + i, c + 1 + i);
    }
  }
  return { pos: new Float32Array(v), nrm: new Float32Array(n), idx: new Uint16Array(idx) };
}

/** plano no XZ centrado na origem, subdividido (pra vertex fog estável) */
export function planeMesh(size, seg = 1) {
  const v = [], n = [], idx = [];
  for (let j = 0; j <= seg; j++) {
    for (let i = 0; i <= seg; i++) {
      v.push((i / seg - 0.5) * size, 0, (j / seg - 0.5) * size);
      n.push(0, 1, 0);
    }
  }
  for (let j = 0; j < seg; j++) {
    for (let i = 0; i < seg; i++) {
      const a = j * (seg + 1) + i;
      idx.push(a, a + seg + 1, a + seg + 2, a, a + seg + 2, a + 1);
    }
  }
  return { pos: new Float32Array(v), nrm: new Float32Array(n), idx: new Uint16Array(idx) };
}

export class Mesh {
  /**
   * @param {WebGL2RenderingContext} gl
   * @param {{pos:Float32Array,nrm:Float32Array,idx:Uint16Array}} data
   * @param {Float32Array|null} instances  4 vec4 por instância
   *   iA = vec4(pos.xyz, seed)     iB = vec4(scale.xyz, matId)
   *   iC = vec4(tint.rgb, rough)   iD = vec4(quat)
   */
  constructor(gl, data, instances = null) {
    this.gl = gl;
    this.count = data.idx.length;
    this.instCount = instances ? instances.length / 16 : 1;
    this.vao = gl.createVertexArray();
    gl.bindVertexArray(this.vao);

    const buf = (target, data, usage = gl.STATIC_DRAW) => {
      const b = gl.createBuffer();
      gl.bindBuffer(target, b);
      gl.bufferData(target, data, usage);
      return b;
    };
    buf(gl.ARRAY_BUFFER, data.pos);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 3, gl.FLOAT, false, 0, 0);
    buf(gl.ARRAY_BUFFER, data.nrm);
    gl.enableVertexAttribArray(1);
    gl.vertexAttribPointer(1, 3, gl.FLOAT, false, 0, 0);

    if (instances) {
      this.instBuf = buf(gl.ARRAY_BUFFER, instances, gl.DYNAMIC_DRAW);
      for (let i = 0; i < 4; i++) {
        gl.enableVertexAttribArray(2 + i);
        gl.vertexAttribPointer(2 + i, 4, gl.FLOAT, false, 64, i * 16);
        gl.vertexAttribDivisor(2 + i, 1);
      }
    }
    buf(gl.ELEMENT_ARRAY_BUFFER, data.idx);
    gl.bindVertexArray(null);
  }

  draw() {
    const gl = this.gl;
    gl.bindVertexArray(this.vao);
    if (this.instBuf) gl.drawElementsInstanced(gl.TRIANGLES, this.count, gl.UNSIGNED_SHORT, 0, this.instCount);
    else gl.drawElements(gl.TRIANGLES, this.count, gl.UNSIGNED_SHORT, 0);
  }
}
