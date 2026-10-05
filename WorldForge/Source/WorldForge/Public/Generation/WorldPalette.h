#pragma once

#include "Generation/WorldMap.h"

/**
 * Cores do chão por bioma (albedo linear, valores de material real: solo nu
 * ~0.1–0.2, grama ~0.05–0.1 no verde, areia ~0.3–0.4, neve ~0.8). Uma fonte
 * só: o material do terreno recebe estas cores e o minimapa usa as mesmas.
 */
struct FWorldPalette
{
	FLinearColor Lush;     // camada G alta (vales úmidos): grama
	FLinearColor Dry;      // camada G baixa: campo seco
	FLinearColor Dirt;     // R: terra batida
	FLinearColor Sand;     // B: margem
	FLinearColor Rock;     // A: penhasco
	FLinearColor Water;    // cor da água vista de cima (minimapa)
	float Snow = 0.0f;     // neve no topo dos objetos (árvores, rochas)

	static FWorldPalette For(EWorldBiome Biome)
	{
		FWorldPalette P;
		switch (Biome)
		{
		case EWorldBiome::Desert:
			P.Lush = FLinearColor(0.20f, 0.16f, 0.08f);
			P.Dry = FLinearColor(0.38f, 0.28f, 0.16f);
			P.Dirt = FLinearColor(0.27f, 0.18f, 0.10f);
			P.Sand = FLinearColor(0.45f, 0.36f, 0.23f);
			P.Rock = FLinearColor(0.30f, 0.17f, 0.09f);
			P.Water = FLinearColor(0.03f, 0.10f, 0.12f);
			break;
		case EWorldBiome::Winter:
			P.Lush = FLinearColor(0.70f, 0.74f, 0.80f);
			P.Dry = FLinearColor(0.78f, 0.80f, 0.84f);
			P.Dirt = FLinearColor(0.16f, 0.14f, 0.13f);
			P.Sand = FLinearColor(0.30f, 0.30f, 0.30f);
			P.Rock = FLinearColor(0.09f, 0.09f, 0.10f);
			P.Water = FLinearColor(0.02f, 0.05f, 0.08f);
			P.Snow = 1.0f;
			break;
		default:
			P.Lush = FLinearColor(0.055f, 0.10f, 0.024f);
			P.Dry = FLinearColor(0.10f, 0.10f, 0.045f);     // grama seca, oliva
			P.Dirt = FLinearColor(0.13f, 0.095f, 0.065f);
			P.Sand = FLinearColor(0.32f, 0.27f, 0.18f);
			P.Rock = FLinearColor(0.17f, 0.16f, 0.15f);
			P.Water = FLinearColor(0.02f, 0.06f, 0.07f);
			break;
		}
		return P;
	}

	/** Cor do chão de uma célula a partir das camadas (a mesma mistura do material). */
	FLinearColor Blend(const FColor& L) const
	{
		const float G = L.G / 255.0f, R = L.R / 255.0f, B = L.B / 255.0f, A = L.A / 255.0f;
		FLinearColor C = FMath::Lerp(Dry, Lush, G);
		C = FMath::Lerp(C, Dirt, R);
		C = FMath::Lerp(C, Sand, B);
		C = FMath::Lerp(C, Rock, A);
		return C;
	}
};

/** Cores dos jogadores (as clássicas de RTS: vermelho, azul, verde, amarelo...). */
inline FLinearColor PlayerColor(int32 Player)
{
	static const FLinearColor Colors[] = {
		FLinearColor(0.85f, 0.06f, 0.04f), FLinearColor(0.05f, 0.25f, 0.95f),
		FLinearColor(0.10f, 0.75f, 0.10f), FLinearColor(0.95f, 0.75f, 0.05f),
		FLinearColor(0.85f, 0.30f, 0.02f), FLinearColor(0.50f, 0.10f, 0.80f),
		FLinearColor(0.05f, 0.75f, 0.80f), FLinearColor(0.90f, 0.40f, 0.60f),
	};
	return Colors[Player % 8];
}
