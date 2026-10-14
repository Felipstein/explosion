#pragma once

#include "Generation/WorldMap.h"

/**
 * A árvore do mapa: o mesmo tipo e o mesmo tamanho no rascunho (o app) e na
 * qualidade real (as fatias e o mapa inteiro montados no editor). Uma regra só,
 * pra que o que o painel mostra seja o que sai.
 *
 * As árvores são as amostras do Procedural Vegetation Editor (Unreal 5.8)
 * convertidas pra malha estática (/Game/Slice/Trees): medidas pelos limites da
 * malha (BoxExtent ×2) em 06/10/2026.
 */
namespace WorldTrees
{
	enum class EKind : uint8
	{
		Broadleaf,   // árvore de folha larga (SM_broadleaf)
		Conifer,     // pinheiro (SM_conifer)
		Shrub,       // deserto: arbusto no lugar da árvore
	};

	/** Tipo da árvore pelo bioma e pelo sorteio do gerador (a floresta temperada mistura os dois). */
	inline EKind KindOf(EWorldBiome Biome, uint8 Variant)
	{
		// deserto: arbusto; Variant ≥ 128 marca a árvore do oásis (copa verde)
		if (Biome == EWorldBiome::Desert) return Variant >= 128 ? EKind::Broadleaf : EKind::Shrub;
		if (Biome == EWorldBiome::Winter) return EKind::Conifer;
		return (Variant & 8) ? EKind::Conifer : EKind::Broadleaf;
	}

	/** Altura e largura da malha na escala 1 (m). */
	inline FVector2f MeshSize(EKind Kind)
	{
		switch (Kind)
		{
		case EKind::Conifer: return FVector2f(23.4f, 9.1f);
		case EKind::Shrub: return FVector2f(1.6f, 2.2f);
		default: return FVector2f(12.2f, 9.5f);
		}
	}

	/**
	 * Escala base de cada tipo (× a escala do gerador, 0,75–1,25). O pinheiro de
	 * 23 m fica em ~13 m: a 50° de câmera, árvore mais alta que isso esconde as
	 * unidades atrás dela (jogabilidade antes do realismo).
	 */
	inline float BaseScale(EKind Kind)
	{
		switch (Kind)
		{
		case EKind::Conifer: return 0.55f;
		case EKind::Shrub: return 1.0f;
		default: return 0.95f;
		}
	}

	/** Escala final de uma árvore do gerador. */
	inline float ScaleOf(const FWorldProp& Prop, EWorldBiome Biome)
	{
		return Prop.Scale * BaseScale(KindOf(Biome, Prop.Variant));
	}
}
