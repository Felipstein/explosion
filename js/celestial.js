// ---------------------------------------------------------------------------
// celestial.js — posição do Sol e da Lua a partir da hora do dia.
//
// Modelo solar padrão: declinação pelo dia do ano, ângulo horário pela hora
// local, elevação/azimute pela latitude. Dá arcos realistas — no verão em
// latitude alta o sol passa raso e demora pra se pôr, no inverno nasce ao
// sudeste. Não é um chute de "sol girando".
//
// Convenção do mundo: +X = leste, -Z = norte, +Y = cima.
// ---------------------------------------------------------------------------

const DEG = Math.PI / 180;

function dirFromAltAz(alt, az) {
  const ca = Math.cos(alt);
  return new Float32Array([ca * Math.sin(az), Math.sin(alt), -ca * Math.cos(az)]);
}

/**
 * @param {number} hours   hora local decimal [0,24)
 * @param {number} latDeg  latitude em graus
 * @param {number} day     dia do ano [1,365]
 * @param {number} hourShift deslocamento do ângulo horário (usado pela Lua)
 * @param {number} declFlip  inverte a declinação (usado pela Lua)
 */
export function celestialPosition(hours, latDeg, day = 172, hourShift = 0, declFlip = 1) {
  // declinação: ±23.44° com máximo no solstício de junho (dia ~172)
  const decl = 23.44 * DEG * Math.sin(2 * Math.PI * (day - 80) / 365.24) * declFlip;
  const H = ((hours + hourShift - 12) * 15) * DEG;    // ângulo horário
  const lat = latDeg * DEG;

  const sinAlt = Math.sin(lat) * Math.sin(decl) + Math.cos(lat) * Math.cos(decl) * Math.cos(H);
  const alt = Math.asin(Math.max(-1, Math.min(1, sinAlt)));

  const cosAz = (Math.sin(decl) - Math.sin(alt) * Math.sin(lat))
              / Math.max(Math.cos(alt) * Math.cos(lat), 1e-6);
  let az = Math.acos(Math.max(-1, Math.min(1, cosAz)));
  if (Math.sin(H) > 0) az = 2 * Math.PI - az;          // tarde → oeste

  return { dir: dirFromAltAz(alt, az), alt, az, altDeg: alt / DEG };
}

export function sunPosition(hours, latDeg, day) {
  return celestialPosition(hours, latDeg, day, 0, 1);
}

/** Lua cheia simplificada: nasce ao pôr do sol, oposta no céu. */
export function moonPosition(hours, latDeg, day) {
  return celestialPosition(hours, latDeg, day, 12, -1);
}

/**
 * Calibração de exposição. Com a iluminância solar normalizada em 1.0 no topo
 * da atmosfera, uma superfície de albedo A sob sol pleno tem radiância
 * A·E·cosθ/π — pro concreto da cena (A≈0.11) ao meio-dia isso dá ~0.017.
 * Pra isso cair em ~0.20 (cinza médio depois do ACES) a exposição base
 * precisa valer ~10. Esta constante é a ponte entre unidades físicas e
 * unidades de tela; é o único número arbitrário da cadeia de iluminação.
 */
export const EXPOSURE_BASE = 10.0;

/**
 * Exposição analítica (calculada na GPU, atmosphere.js texel 4):
 *
 *   exposição = BASE · (E_ref / E_render) · (E_lux / E_ref_lux)^α
 *
 * O primeiro fator é compensação total — a câmera mede a luz incidente e
 * tudo fica igual ao meio-dia. O segundo devolve parte da diferença: o brilho
 * exibido cai como E^α com a iluminância física. É o papel da "Exposure
 * Compensation Curve" (compensação por EV100) do Unreal, e o "day-for-night"
 * do cinema, que subexpõe 2–2.5 stops. O alvo é a imagem FINAL: o pé da
 * curva ACES ainda escurece os escuros (cinza médio −2.3 stops na cena vira
 * −3.1 na tela), e o Purkinje devolve ~0.35 stop. Com α = EXPOSURE_ALPHA o
 * chão sob lua cheia alta sai ~2.75 stops abaixo do meio-dia na tela — entre
 * o day-for-night (2–2.5) e Ghost of Tsushima (~4, medido nos slides), mais
 * claro porque num RTS a leitura das unidades vem primeiro (StarCraft II).
 *
 * E_ref é escolhida pra que o meio-dia da cena (sol a 43°, E_render 0.656
 * medido na LUT) tenha a mesma exposição da curva anterior (11.4) — o dia
 * aprovado fica igual. α: meio-dia 84 klux → lua cheia alta 0.29 lux são
 * 18.1 stops físicos; 2.29 stops na cena / 18.1 = 0.1265.
 */
export const EXPOSURE_REF = 0.768;
export const EXPOSURE_ALPHA = 0.1265;

/** Iluminância solar normalizada (1.0 = topo da atmosfera). */
export const SUN_ILLUMINANCE = 1.0;
/** a mesma coisa em lux (constante solar luminosa) */
export const SUN_LUX = 128000;

/**
 * Lua cheia. A real tem magnitude −12.74 contra −26.74 do Sol: 14 magnitudes,
 * 10^(−5.6) ≈ 2.5e-6 da iluminância solar — 0.32 lux no topo da atmosfera,
 * ~0.25–0.3 lux no chão com ela alta. É esse valor que a exposição e a visão
 * noturna enxergam (MOON_PHYS_RATIO).
 *
 * O render usa uma Lua mais forte (MOON_ILLUMINANCE) por dois motivos: a
 * precisão do half-float (a luz física cairia nos subnormais) e o contraste
 * fogo/noite — com o luar físico a explosão seria ~10⁵× mais forte que o
 * ambiente e o campo de batalha sumiria toda vez que a câmera fechasse.
 * Far Cry 5 chegou na mesma conclusão (valores físicos dão contraste demais).
 */
export const MOON_PHYS_RATIO = 2.5e-6;
export const MOON_ILLUMINANCE = 2.2e-3;

/**
 * Cor do luar: luz do Sol refletida por um solo levemente avermelhado.
 * Índice de cor B−V da Lua cheia 0.92 contra 0.656 do Sol (0.26 mag mais
 * vermelha); com refletância linear em λ que reproduz isso, integrada contra
 * D65 e os cones (tools/purkinje.py), a cor sai [1.165, 0.976, 0.753].
 * O azul que o olho vê no luar NÃO é do luar: é o Purkinje shift (post.js).
 * Jensen et al. 2001, "A Physically-Based Night Sky Model", faz o mesmo.
 */
export const MOON_TINT = [1.165, 0.976, 0.753];

/** céu sem lua (estrelas + airglow), lux — piso físico da noite */
export const NIGHT_GLOW_LUX = 0.002;
