#pragma once

#include "Sim/SimApi.h"

// O mapa como a simulação vê: uma grade de células com passabilidade.
// O visual (Landscape, pedras, árvores) é da Unreal; a simulação só lê isto.

#include <cstdint>
#include <vector>

#include "Sim/FixedMath.h"

namespace Sim
{
	enum class CellPass : uint8_t
	{
		Open = 0,
		Blocked = 1,   // penhasco, pedra grande, borda do mapa
		Water = 2,     // fundo demais pra unidade de chão
	};

	struct Cell
	{
		int32_t X = 0;
		int32_t Y = 0;
		constexpr bool operator==(Cell B) const { return X == B.X && Y == B.Y; }
		constexpr bool operator!=(Cell B) const { return !(*this == B); }
	};

	struct SIMCORE_API SimMap
	{
		int32_t Width = 0;
		int32_t Height = 0;
		Fix CellSize = Fix::FromInt(4);
		/** Canto mínimo da célula (0, 0), no mundo (m). */
		Vec2 Origin;
		std::vector<uint8_t> Pass;        // CellPass por célula, linha a linha (Y·Width + X)
		/** Distância (em células, métrica do xadrez) até a célula bloqueada ou borda mais próxima. 1 = vizinha de bloqueio. */
		std::vector<uint8_t> Clearance;
		/** Altura do chão por célula, a menor e a maior (decímetros). Vazio = sem altura (chão plano). */
		std::vector<int16_t> MinDm;
		std::vector<int16_t> MaxDm;

		bool HasHeights() const { return !MinDm.empty() && MinDm.size() == Pass.size() && MaxDm.size() == Pass.size(); }

		void Resize(int32_t InWidth, int32_t InHeight, Fix InCellSize, Vec2 InOrigin);
		/** Recalcula a folga depois de mudar a passabilidade. */
		void ComputeClearance();

		bool InBounds(int32_t X, int32_t Y) const { return X >= 0 && Y >= 0 && X < Width && Y < Height; }
		int32_t Index(int32_t X, int32_t Y) const { return Y * Width + X; }
		CellPass PassAt(int32_t X, int32_t Y) const { return InBounds(X, Y) ? CellPass(Pass[Index(X, Y)]) : CellPass::Blocked; }
		/** Livre pra uma unidade que precisa de MinClearance células de folga em volta (1 = só a própria). */
		bool IsWalkable(int32_t X, int32_t Y, int32_t MinClearance) const { return InBounds(X, Y) && Clearance[Index(X, Y)] >= MinClearance; }

		Cell CellOf(Vec2 P) const;
		Vec2 CellCenter(Cell C) const;
		/** Folga em células pra um raio (m): quantas células a unidade ocupa em volta do centro, no mínimo 1. */
		int32_t ClearanceFor(Fix Radius) const;
		/** Retângulo do mundo coberto pela grade. */
		Vec2 MinCorner() const { return Origin; }
		Vec2 MaxCorner() const { return Origin + Vec2::Make(CellSize * Width, CellSize * Height); }

		uint64_t Hash() const;
	};
}
