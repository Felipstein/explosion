#pragma once

#include "CoreMinimal.h"

/**
 * Sol e lua pela hora do dia: porte de reference/webgl/js/celestial.js.
 *
 * Hora solar local (H = 0 ao meio-dia, sem fuso nem longitude), declinação
 * ±23.44° com máximo no solstício de junho (dia ~172), azimute medido do norte
 * pro leste. Lua cheia simplificada: a mesma conta com +12 h e a declinação
 * invertida (nasce no pôr do sol, oposta no céu).
 *
 * Eixos: no WebGL +X = leste, −Z = norte, +Y = cima; na Unreal (x, z, y) do
 * WebGL → +X = leste, −Y = norte, +Z = cima.
 */
struct FCelestial
{
	FVector Direction = FVector::UpVector;   // pra o astro (unitário, eixos da Unreal)
	float AltitudeDeg = 90.0f;
	float AzimuthDeg = 0.0f;

	static FCelestial Position(float Hours, float LatitudeDeg, float DayOfYear, float HourShift, float DeclinationSign)
	{
		const float Decl = FMath::DegreesToRadians(23.44f) * FMath::Sin(2.0f * PI * (DayOfYear - 80.0f) / 365.24f) * DeclinationSign;
		const float H = FMath::DegreesToRadians((Hours + HourShift - 12.0f) * 15.0f);     // ângulo horário
		const float Lat = FMath::DegreesToRadians(LatitudeDeg);
		const float SinAlt = FMath::Sin(Lat) * FMath::Sin(Decl) + FMath::Cos(Lat) * FMath::Cos(Decl) * FMath::Cos(H);
		const float Alt = FMath::Asin(FMath::Clamp(SinAlt, -1.0f, 1.0f));
		const float CosAz = (FMath::Sin(Decl) - FMath::Sin(Alt) * FMath::Sin(Lat)) / FMath::Max(FMath::Cos(Alt) * FMath::Cos(Lat), 1e-6f);
		float Az = FMath::Acos(FMath::Clamp(CosAz, -1.0f, 1.0f));
		if (FMath::Sin(H) > 0.0f)
		{
			Az = 2.0f * PI - Az;                                  // tarde → oeste
		}
		FCelestial C;
		const float Ca = FMath::Cos(Alt);
		// WebGL (sin az·cos alt, sin alt, −cos az·cos alt) → Unreal (x, z, y)
		C.Direction = FVector(Ca * FMath::Sin(Az), -Ca * FMath::Cos(Az), FMath::Sin(Alt));
		C.AltitudeDeg = FMath::RadiansToDegrees(Alt);
		C.AzimuthDeg = FMath::RadiansToDegrees(Az);
		return C;
	}

	static FCelestial Sun(float Hours, float LatitudeDeg, float DayOfYear) { return Position(Hours, LatitudeDeg, DayOfYear, 0.0f, 1.0f); }
	static FCelestial Moon(float Hours, float LatitudeDeg, float DayOfYear) { return Position(Hours, LatitudeDeg, DayOfYear, 12.0f, -1.0f); }

	/** Nome da fase do dia pela altura do sol (o leitor do painel do WebGL, main.js solarPhase). */
	static const TCHAR* PhaseName(float SunAltitudeDeg)
	{
		if (SunAltitudeDeg > 25.0f) return TEXT("dia pleno");
		if (SunAltitudeDeg > 8.0f) return TEXT("manhã/tarde");
		if (SunAltitudeDeg > 0.0f) return TEXT("hora dourada");
		if (SunAltitudeDeg > -6.0f) return TEXT("crep. civil");
		if (SunAltitudeDeg > -12.0f) return TEXT("crep. náutico");
		if (SunAltitudeDeg > -18.0f) return TEXT("crep. astron.");
		return TEXT("noite");
	}
};
