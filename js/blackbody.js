// ---------------------------------------------------------------------------
// blackbody.js — LUT de emissão de corpo negro fisicamente correta.
//
// Integra a lei de Planck B(λ,T) contra as funções de casamento de cor CIE
// 1931 x̄ȳz̄, usando a aproximação multi-lobo gaussiana de
//   Wyman, Sloan & Shirley, "Simple Analytic Approximations to the CIE XYZ
//   Color Matching Functions", JCGT 2(2), 2013.
// Depois XYZ → sRGB linear.
//
// Saída: textura RGBA32F 1xN onde
//   rgb = cromaticidade normalizada (luminância 1)
//   a   = luminância relativa integrada na banda visível (∝ T⁴ e além, porque
//         Wien empurra o pico pro visível conforme T sobe)
//
// É isso que faz o núcleo virar branco-azulado e a borda cair pra laranja
// profundo sem nenhum gradiente pintado à mão, e faz a bola de fogo apagar
// com a curva certa.
// ---------------------------------------------------------------------------

const PLANCK_C1 = 1.1910429723971884e-16; // 2hc²  [W·m²/sr]
const PLANCK_C2 = 1.4387768775039337e-2;  // hc/k  [m·K]

function planck(lambdaNm, T) {
  const l = lambdaNm * 1e-9;
  const l5 = l * l * l * l * l;
  return PLANCK_C1 / (l5 * (Math.exp(PLANCK_C2 / (l * T)) - 1));
}

// gaussiana assimétrica (sigma diferente abaixo/acima da média)
function g(x, mu, s1, s2) {
  const t = (x - mu) / (x < mu ? s1 : s2);
  return Math.exp(-0.5 * t * t);
}

function cieX(l) {
  return 1.056 * g(l, 599.8, 37.9, 31.0) + 0.362 * g(l, 442.0, 16.0, 26.7) - 0.065 * g(l, 501.1, 20.4, 26.2);
}
function cieY(l) {
  return 0.821 * g(l, 568.8, 46.9, 40.5) + 0.286 * g(l, 530.9, 16.3, 31.1);
}
function cieZ(l) {
  return 1.217 * g(l, 437.0, 11.8, 36.0) + 0.681 * g(l, 459.0, 26.0, 13.8);
}

// XYZ (D65) → sRGB linear
function xyzToRgb(X, Y, Z) {
  return [
     3.2404542 * X - 1.5371385 * Y - 0.4985314 * Z,
    -0.9692660 * X + 1.8760108 * Y + 0.0415560 * Z,
     0.0556434 * X - 0.2040259 * Y + 1.0572252 * Z,
  ];
}

function spectrumFor(T) {
  let X = 0, Y = 0, Z = 0;
  const step = 2;
  for (let l = 360; l <= 830; l += step) {
    const b = planck(l, T);
    X += b * cieX(l) * step;
    Y += b * cieY(l) * step;
    Z += b * cieZ(l) * step;
  }
  return [X, Y, Z];
}

/**
 * @param {number} maxKelvin temperatura correspondente a t = 1.0
 * @param {number} n         resolução da LUT
 */
export function buildBlackbodyLUT(maxKelvin = 3600, n = 1024) {
  const data = new Float32Array(n * 4);
  // referência de luminância: o topo da faixa
  const [, Yref] = spectrumFor(maxKelvin);

  for (let i = 0; i < n; i++) {
    const t = (i + 0.5) / n;
    const T = Math.max(80, t * maxKelvin);
    const [X, Y, Z] = spectrumFor(T);
    let [r, gg, b] = xyzToRgb(X / Y, 1.0, Z / Y); // normaliza pela luminância

    // clipa fora-de-gamut mantendo a luminância aproximada
    const m = Math.min(r, gg, b);
    if (m < 0) { r -= m; gg -= m; b -= m; }
    const lum = 0.2126 * r + 0.7152 * gg + 0.0722 * b || 1;
    r /= lum; gg /= lum; b /= lum;

    data[i * 4 + 0] = r;
    data[i * 4 + 1] = gg;
    data[i * 4 + 2] = b;
    data[i * 4 + 3] = Y / Yref; // intensidade relativa percebida
  }
  return { data, n, maxKelvin };
}

export function uploadBlackbodyLUT(gl, lut) {
  const t = gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D, t);
  gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA32F, lut.n, 1, 0, gl.RGBA, gl.FLOAT, lut.data);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  return t;
}
