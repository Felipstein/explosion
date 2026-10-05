#include "Generation/WorldNoise.h"

namespace WorldNoise
{
	static FORCEINLINE float Fade(float T) { return T * T * T * (T * (T * 6.0f - 15.0f) + 10.0f); }

	static FORCEINLINE float Corner(int32 IX, int32 IY, float DX, float DY, uint32 Seed)
	{
		const float A = Unit(Hash(IX, IY, Seed)) * 6.28318531f;
		float S, C;
		FMath::SinCos(&S, &C, A);
		return C * DX + S * DY;
	}

	float Gradient(float X, float Y, uint32 Seed)
	{
		const float FX = FMath::FloorToFloat(X), FY = FMath::FloorToFloat(Y);
		const int32 IX = int32(FX), IY = int32(FY);
		const float DX = X - FX, DY = Y - FY;
		const float U = Fade(DX), V = Fade(DY);
		const float N00 = Corner(IX, IY, DX, DY, Seed);
		const float N10 = Corner(IX + 1, IY, DX - 1.0f, DY, Seed);
		const float N01 = Corner(IX, IY + 1, DX, DY - 1.0f, Seed);
		const float N11 = Corner(IX + 1, IY + 1, DX - 1.0f, DY - 1.0f, Seed);
		const float NX0 = N00 + U * (N10 - N00);
		const float NX1 = N01 + U * (N11 - N01);
		// gradientes unitários em 2D: o máximo teórico é √½; ×√2 leva a ~[-1, 1]
		return (NX0 + V * (NX1 - NX0)) * 1.41421356f;
	}

	// rotação de ~37° entre oitavas + deslocamento irracional (as origens não coincidem)
	static constexpr float RotC = 0.79863551f, RotS = 0.60181502f;

	float Fbm(float X, float Y, uint32 Seed, int32 Octaves, float Gain)
	{
		float Sum = 0.0f, Amp = 1.0f, Norm = 0.0f;
		for (int32 O = 0; O < Octaves; ++O)
		{
			Sum += Amp * Gradient(X, Y, Seed + uint32(O) * 1013u);
			Norm += Amp;
			const float NX = (RotC * X - RotS * Y) * 2.0f + 17.13f;
			const float NY = (RotS * X + RotC * Y) * 2.0f + 5.71f;
			X = NX; Y = NY;
			Amp *= Gain;
		}
		return Sum / Norm;
	}

	float Ridged(float X, float Y, uint32 Seed, int32 Octaves, float Gain)
	{
		float Sum = 0.0f, Amp = 1.0f, Norm = 0.0f, Weight = 1.0f;
		for (int32 O = 0; O < Octaves; ++O)
		{
			float R = 1.0f - FMath::Abs(Gradient(X, Y, Seed + uint32(O) * 7919u));
			R *= R;
			R *= Weight;
			Weight = FMath::Clamp(R * 2.0f, 0.0f, 1.0f);
			Sum += Amp * R;
			Norm += Amp;
			const float NX = (RotC * X - RotS * Y) * 2.0f + 3.37f;
			const float NY = (RotS * X + RotC * Y) * 2.0f + 11.9f;
			X = NX; Y = NY;
			Amp *= Gain;
		}
		return Sum / Norm;
	}
}
