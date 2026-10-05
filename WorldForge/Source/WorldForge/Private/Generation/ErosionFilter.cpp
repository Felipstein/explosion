// Phacelle Noise e Advanced Terrain Erosion Filter
// copyright (c) 2025 Rune Skovbo Johansen
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Porte pra C++ de https://www.shadertoy.com/view/wXcfWn (ver ErosionFilter.h).
#include "Generation/ErosionFilter.h"

#include "Generation/WorldNoise.h"

namespace ErosionFilter
{
	static constexpr float Tau = 6.28318530717959f;

	static FORCEINLINE float Clamp01(float T) { return FMath::Clamp(T, 0.0f, 1.0f); }
	static FORCEINLINE float PowInv(float T, float Power) { return 1.0f - FMath::Pow(1.0f - Clamp01(T), Power); }
	static FORCEINLINE float EaseOut(float T) { const float V = 1.0f - Clamp01(T); return 1.0f - V * V; }
	static FORCEINLINE float SmoothStart(float T, float Smoothing)
	{
		if (T >= Smoothing) return T - 0.5f * Smoothing;
		return 0.5f * T * T / FMath::Max(Smoothing, 1e-12f);
	}
	static FORCEINLINE FVector2f SafeNormalize(FVector2f N)
	{
		const float L = N.Size();
		return L > 1e-10f ? N / L : N;
	}

	/** Deslocamento do ponto da célula em [-1, 1)² (o original: hash(gridPoint)). */
	static FORCEINLINE FVector2f CellHash(int32 X, int32 Y, uint32 Seed)
	{
		const uint32 H = WorldNoise::Hash(X, Y, Seed);
		return FVector2f(WorldNoise::Unit(H) * 2.0f - 1.0f, WorldNoise::Unit(WorldNoise::Hash(H ^ 0x68e31da4u)) * 2.0f - 1.0f);
	}

	/** Phacelle Noise: (cos, sin) da fase interpolada + vetor lateral. */
	static FVector4f Phacelle(FVector2f P, FVector2f NormDir, float Freq, float Offset, float Normalization, uint32 Seed)
	{
		const FVector2f Side = FVector2f(-NormDir.Y, NormDir.X) * (Freq * Tau);
		Offset *= Tau;
		const float FX = FMath::FloorToFloat(P.X), FY = FMath::FloorToFloat(P.Y);
		const FVector2f Frac(P.X - FX, P.Y - FY);
		FVector2f Phase(0.0f, 0.0f);
		float WeightSum = 0.0f;
		for (int32 I = -1; I <= 2; ++I)
		{
			for (int32 J = -1; J <= 2; ++J)
			{
				const FVector2f Rand = CellHash(int32(FX) + I, int32(FY) + J, Seed) * 0.5f;
				const FVector2f V = Frac - FVector2f(float(I), float(J)) - Rand;
				const float W = FMath::Max(0.0f, FMath::Exp(-(V | V) * 2.0f) - 0.01111f);
				if (W <= 0.0f)
				{
					continue;   // peso zero: mesmo resultado, sem o seno/cosseno
				}
				WeightSum += W;
				const float Wave = (V | Side) + Offset;
				float S, C;
				FMath::SinCos(&S, &C, Wave);
				Phase += FVector2f(C, S) * W;
			}
		}
		const FVector2f Interp = Phase / FMath::Max(WeightSum, 1e-12f);
		const float Mag = FMath::Max(1.0f - Normalization, Interp.Size());
		return FVector4f(Interp.X / Mag, Interp.Y / Mag, Side.X, Side.Y);
	}

	float Apply(FVector2f P, float DhDx, float DhDy, float FadeTarget, const FParams& Prm)
	{
		float Strength = Prm.Strength * Prm.Scale;
		FadeTarget = FMath::Clamp(FadeTarget, -1.0f, 1.0f);
		FVector3f HS(0.0f, DhDx, DhDy);                 // altura (delta) e inclinação acumuladas
		float Freq = 1.0f / (Prm.Scale * Prm.CellScale);
		const FVector2f Slope(DhDx, DhDy);
		const float SlopeLength = FMath::Max(Slope.Size(), 1e-10f);
		float RoundingMult = 1.0f;
		const float RoundingForInput = FMath::Lerp(Prm.Rounding.Y, Prm.Rounding.X, Clamp01(FadeTarget + 0.5f)) * Prm.Rounding.Z;
		float CombiMask = EaseOut(SmoothStart(SlopeLength * Prm.Onset.X, RoundingForInput * Prm.Onset.X));
		FVector2f GullySlope = FMath::Lerp(Slope, Slope / SlopeLength * Prm.AssumedSlope.X, Prm.AssumedSlope.Y);
		for (int32 Oct = 0; Oct < Prm.Octaves; ++Oct)
		{
			FVector4f Ph = Phacelle(P * Freq, SafeNormalize(GullySlope), Prm.CellScale, 0.25f, Prm.Normalization, Prm.Seed + uint32(Oct) * 7717u);
			Ph.Z *= -Freq;
			Ph.W *= -Freq;
			const float Sloping = FMath::Abs(Ph.Y);
			const float SignY = Ph.Y > 0.0f ? 1.0f : (Ph.Y < 0.0f ? -1.0f : 0.0f);
			GullySlope += FVector2f(Ph.Z, Ph.W) * (SignY * Strength * Prm.GullyWeight);
			const FVector3f Gullies(Ph.X, Ph.Y * Ph.Z, Ph.Y * Ph.W);
			const FVector3f Faded = FMath::Lerp(FVector3f(FadeTarget, 0.0f, 0.0f), Gullies * Prm.GullyWeight, CombiMask);
			HS += Faded * Strength;
			FadeTarget = Faded.X;
			const float RoundingForOctave = FMath::Lerp(Prm.Rounding.Y, Prm.Rounding.X, Clamp01(Ph.X + 0.5f)) * RoundingMult;
			const float NewMask = EaseOut(SmoothStart(Sloping * Prm.Onset.Y, RoundingForOctave * Prm.Onset.Y));
			CombiMask = PowInv(CombiMask, Prm.Detail) * NewMask;
			Strength *= Prm.Gain;
			Freq *= Prm.Lacunarity;
			RoundingMult *= Prm.Rounding.W;
		}
		return HS.X;
	}
}
