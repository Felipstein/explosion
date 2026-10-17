#include "Generation/WorldMap.h"

float FWorldMap::HeightAt(FVector2f P) const
{
	if (N < 2)
	{
		return 0.0f;
	}
	const FVector2f G = ToGrid(P);
	const float X = FMath::Clamp(G.X, 0.0f, float(N - 1)), Y = FMath::Clamp(G.Y, 0.0f, float(N - 1));
	const int32 I = FMath::Min(int32(X), N - 2), J = FMath::Min(int32(Y), N - 2);
	const float FX = X - I, FY = Y - J;
	const float H00 = Height[Index(I, J)], H10 = Height[Index(I + 1, J)];
	const float H01 = Height[Index(I, J + 1)], H11 = Height[Index(I + 1, J + 1)];
	return FMath::Lerp(FMath::Lerp(H00, H10, FX), FMath::Lerp(H01, H11, FX), FY);
}

float FWorldMap::FormAt(FVector2f P) const
{
	if (N < 2 || Forms.Num() != N * N)
	{
		return 0.0f;
	}
	const FVector2f G = ToGrid(P);
	const float X = FMath::Clamp(G.X, 0.0f, float(N - 1)), Y = FMath::Clamp(G.Y, 0.0f, float(N - 1));
	const int32 I = FMath::Min(int32(X), N - 2), J = FMath::Min(int32(Y), N - 2);
	const float FX = X - I, FY = Y - J;
	return FMath::Lerp(FMath::Lerp(Forms[Index(I, J)], Forms[Index(I + 1, J)], FX), FMath::Lerp(Forms[Index(I, J + 1)], Forms[Index(I + 1, J + 1)], FX), FY);
}

FVector3f FWorldMap::NormalAt(FVector2f P) const
{
	const float E = Cell;
	const float DX = (HeightAt(P + FVector2f(E, 0)) - HeightAt(P - FVector2f(E, 0))) / (2.0f * E);
	const float DY = (HeightAt(P + FVector2f(0, E)) - HeightAt(P - FVector2f(0, E))) / (2.0f * E);
	return FVector3f(-DX, -DY, 1.0f).GetSafeNormal();
}
