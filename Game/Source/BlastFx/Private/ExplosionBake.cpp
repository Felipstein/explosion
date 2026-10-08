// Cópia do BlastLab (BlastLab/Source/BlastLab/Private/ExplosionBake.cpp, 08/10/2026) pro jogo. A original continua
// sendo a referência: mudanças de aparência nascem lá e são trazidas pra cá.
#include "ExplosionBake.h"

#include "Algo/BinarySearch.h"

void UExplosionBake::FramesAt(float T, int32& F0, int32& F1, float& Alpha) const
{
	const int32 N = FrameTimes.Num();
	if (N == 0)
	{
		F0 = F1 = 0;
		Alpha = 0.0f;
		return;
	}
	if (T <= FrameTimes[0])
	{
		F0 = F1 = 0;
		Alpha = 0.0f;
		return;
	}
	if (T >= FrameTimes.Last())
	{
		F0 = F1 = N - 1;
		Alpha = 0.0f;
		return;
	}
	// último quadro com tempo <= T
	F0 = Algo::UpperBound(FrameTimes, T) - 1;
	F1 = F0 + 1;
	const float Span = FrameTimes[F1] - FrameTimes[F0];
	Alpha = Span > 0.0f ? (T - FrameTimes[F0]) / Span : 0.0f;
}
