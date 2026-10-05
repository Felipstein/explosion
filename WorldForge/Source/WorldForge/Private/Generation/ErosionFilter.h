// Phacelle Noise e Advanced Terrain Erosion Filter
// copyright (c) 2025 Rune Skovbo Johansen
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Porte pra C++ (CPU) do shader https://www.shadertoy.com/view/wXcfWn
// (aba Buffer A), descrito em
// https://blog.runevision.com/2026/03/fast-and-gorgeous-erosion-filter.html
// Única troca: o hash de vetor do original (fract em float, perde precisão
// com coordenadas grandes) virou o hash inteiro do gerador (WorldNoise::Hash).
// Este arquivo (e o .cpp) fica sob a MPL 2.0; o resto do projeto não.
#pragma once

#include "CoreMinimal.h"

namespace ErosionFilter
{
	struct FParams
	{
		float Strength = 0.22f;
		float GullyWeight = 0.5f;
		float Detail = 1.5f;
		FVector4f Rounding = FVector4f(0.1f, 0.0f, 0.1f, 2.0f);   // cume, vinco, multiplicador da base, por oitava
		FVector4f Onset = FVector4f(1.25f, 1.25f, 2.8f, 1.5f);
		FVector2f AssumedSlope = FVector2f(0.7f, 1.0f);
		float Scale = 0.15f;          // comprimento de onda das listras na oitava 0 (unidade da posição)
		int32 Octaves = 5;
		float Lacunarity = 2.0f;
		float Gain = 0.5f;
		float CellScale = 0.7f;
		float Normalization = 0.5f;
		uint32 Seed = 0;
	};

	/**
	 * Erosão no ponto P de um terreno com altura H e gradiente (DhDx, DhDy),
	 * na mesma unidade da posição. FadeTarget: −1 nos vales, +1 nos picos.
	 * Devolve a altura a somar (as derivadas do original são ignoradas: as
	 * normais saem da altura final). Em terreno plano devolve
	 * FadeTarget × magnitude.
	 */
	float Apply(FVector2f P, float DhDx, float DhDy, float FadeTarget, const FParams& Params);
}
