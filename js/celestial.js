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
 * Exposição analítica a partir da elevação solar.
 *
 * A iluminância real cai ~6 ordens de grandeza do meio-dia à noite fechada.
 * Compensar isso por inteiro deixaria a noite com a mesma aparência do dia
 * (e a explosão viraria um borrão branco). Cinema não faz isso: empurra
 * 4–5 stops e deixa a noite ler como noite. É o que esta curva faz —
 * fisicamente motivada no formato, mas limitada no alcance.
 */
/**
 * Calibração de exposição. Com a iluminância solar normalizada em 1.0 no topo
 * da atmosfera, uma superfície de albedo A sob sol pleno tem radiância
 * A·E·cosθ/π — pro concreto da cena (A≈0.11) ao meio-dia isso dá ~0.017.
 * Pra isso cair em ~0.20 (cinza médio depois do ACES) a exposição base
 * precisa valer ~10. Esta constante é a ponte entre unidades físicas e
 * unidades de tela; é o único número arbitrário da cadeia de iluminação.
 */
export const EXPOSURE_BASE = 10.0;

export function exposureForSun(sunAltDeg, moonAltDeg, bias = 1) {
  const e = sunAltDeg;
  // luz diurna: satura rápido acima de ~10°
  const day = Math.max(0, Math.sin(Math.max(e, 0) * DEG)) ** 0.42;
  // crepúsculo: decai suave até -18° (fim do crepúsculo astronômico)
  const twi = Math.exp(-Math.pow(Math.max(-e, 0) / 7.5, 1.35));
  // piso noturno, reforçado pela lua acima do horizonte
  const moon = 0.055 + 0.085 * Math.max(0, Math.sin(Math.max(moonAltDeg, 0) * DEG));
  const level = Math.max(day, twi * 0.85, moon);
  // mapeia nível de luz → multiplicador de exposição (1× dia, ~14× noite)
  return bias * EXPOSURE_BASE * (1.0 / Math.pow(Math.max(level, 0.045), 0.80));
}

/** Iluminância solar normalizada (1.0 = topo da atmosfera). */
export const SUN_ILLUMINANCE = 1.0;

/**
 * Iluminância lunar. A real é ~2.5e-6 da solar (19 stops abaixo) — com isso
 * a cena noturna seria matematicamente invisível. Este valor é o "luar de
 * cinema": exagerado de propósito, e exposto como parâmetro.
 */
export const MOON_ILLUMINANCE = 2.2e-3;
export const MOON_TINT = [0.62, 0.74, 1.0];
