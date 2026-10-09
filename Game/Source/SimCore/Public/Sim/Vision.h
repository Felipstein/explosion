#pragma once

#include "Sim/SimApi.h"

// A visão de cada time, pra névoa de guerra: na mesma grade do mapa (4 m), cada célula
// guarda quantos "olhos" do time a veem agora e se o time já a viu alguma vez. Cada
// unidade carimba um disco do raio de visão dela (+1) e, quando muda de célula, desfaz
// o disco velho (−1) e carimba o novo: o mesmo esquema de revelar/desfazer do Zero Hour
// (PartitionManager, "lookers"). Só inteiros: a visão é igual em todos os PCs.

#include <cstdint>
#include <vector>

namespace Sim
{
	class SIMCORE_API VisionGrid
	{
	public:
		void Init(int32_t InWidth, int32_t InHeight, int32_t InTeams);

		/**
		 * Soma Delta (+1 ou −1) no contador das células do disco de centro CellIndex.
		 * O disco tem as células cujo centro fica a até √RadiusSqCells células do centro;
		 * RadiusCells é o teto inteiro desse raio (até onde procurar).
		 */
		void Look(int32_t Team, int32_t CellIndex, int32_t RadiusCells, int64_t RadiusSqCells, int32_t Delta);

		bool IsVisible(int32_t Team, int32_t CellIndex) const;
		bool IsExplored(int32_t Team, int32_t CellIndex) const;
		/** Sobe toda vez que algo muda na visão do time. */
		uint32_t Version(int32_t Team) const;
		/** Mesma contagem de olhos em todas as células (pra conferir a grade contra uma refeita do zero). */
		bool SameCounts(const VisionGrid& Other) const { return W == Other.W && H == Other.H && TeamCount == Other.TeamCount && Count == Other.Count; }
		int32_t Teams() const { return TeamCount; }
		int32_t Width() const { return W; }
		int32_t Height() const { return H; }

	private:
		struct Disc
		{
			int32_t RadiusCells = 0;
			int64_t RadiusSqCells = 0;
			std::vector<int32_t> DX;
			std::vector<int32_t> DY;
		};
		const Disc& DiscFor(int32_t RadiusCells, int64_t RadiusSqCells);

		int32_t W = 0;
		int32_t H = 0;
		int32_t TeamCount = 0;
		std::vector<uint16_t> Count;    // TeamCount · W · H
		std::vector<uint8_t> Seen;      // TeamCount · W · H
		std::vector<uint32_t> Versions; // por time
		std::vector<Disc> Discs;        // um por raio usado (poucos tipos de unidade)
	};
}
