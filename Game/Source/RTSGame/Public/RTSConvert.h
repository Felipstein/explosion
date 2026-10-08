#pragma once

// Conversões entre a simulação (metros em ponto fixo, ângulo binário) e a
// Unreal (centímetros em float, graus). Só o desenho e a entrada usam isto: a
// simulação nunca vê float.

#include "CoreMinimal.h"
#include "Sim/FixedMath.h"

namespace RTS
{
	/** Posição da simulação → mundo da Unreal (cm), no plano XY. */
	inline FVector2D ToWorld(const Sim::Vec2& P)
	{
		return FVector2D(P.X.ToDouble() * 100.0, P.Y.ToDouble() * 100.0);
	}

	/** Ponto clicado (cm) → simulação. O arredondamento acontece num PC só; o resultado viaja como inteiro. */
	inline Sim::Vec2 ToSim(const FVector2D& Cm)
	{
		return Sim::Vec2::Make(
			Sim::Fix::FromRaw(FMath::RoundToInt64(Cm.X / 100.0 * double(Sim::Fix::OneRaw))),
			Sim::Fix::FromRaw(FMath::RoundToInt64(Cm.Y / 100.0 * double(Sim::Fix::OneRaw))));
	}

	inline float YawDegrees(Sim::Angle A)
	{
		return float(A.Bam) * (360.0f / 65536.0f);
	}

	/** Metros (dado de design) → milésimos inteiros pra simulação. */
	inline int32 Milli(double Value)
	{
		return int32(FMath::RoundToInt64(Value * 1000.0));
	}
}
