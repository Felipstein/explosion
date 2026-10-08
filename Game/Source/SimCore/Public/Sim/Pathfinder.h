#pragma once

#include "Sim/SimApi.h"

// Caminho na grade: A* em 8 direções com custos inteiros (10 reto, 14 diagonal),
// respeitando a folga da unidade, e depois "puxar o barbante" (corta os pontos
// que têm linha livre entre si). Tudo inteiro e com desempate fixo: o mesmo
// caminho em qualquer PC.

#include <cstdint>
#include <vector>

#include "Sim/SimMap.h"

namespace Sim
{
	class SIMCORE_API Pathfinder
	{
	public:
		/** Teto de células abertas por busca: o pior caso fica limitado no tempo do passo. */
		static constexpr int32_t MaxExpansions = 40000;

		/**
		 * Caminho de Start até Goal pra uma unidade com MinClearance de folga.
		 * Se Goal está bloqueado, vai pra célula livre mais perto dele. Se não há
		 * caminho, vai até a célula alcançada mais perto do alvo. Devolve os pontos
		 * (centros de célula, já puxados), sem incluir o ponto de partida.
		 * Falso se nem a partida tem saída.
		 */
		bool FindPath(const SimMap& Map, Vec2 From, Vec2 To, int32_t MinClearance, std::vector<Vec2>& OutWaypoints, bool* OutReached = nullptr);

		/** Linha livre entre duas células (todas as células cruzadas são andáveis). */
		static bool HasLineOfMovement(const SimMap& Map, Cell A, Cell B, int32_t MinClearance);

		/** A célula andável mais perto de C (busca em anéis), ou falso se não houver em Radius células. */
		static bool NearestWalkable(const SimMap& Map, Cell C, int32_t MinClearance, int32_t Radius, Cell& Out);

		/**
		 * Regiões ligadas: células por onde uma unidade com MinClearance consegue ir de uma
		 * pra outra recebem o mesmo rótulo (1, 2, ...; 0 = não andável). Como o caminho não
		 * corta quina, a ligação é pelos 4 lados. Devolve o tamanho de cada região (índice = rótulo).
		 */
		static std::vector<int32_t> LabelRegions(const SimMap& Map, int32_t MinClearance, std::vector<int32_t>& OutLabels);

		/** A célula da região Region mais perto de C (busca em anéis), ou falso se não houver em Radius células. */
		static bool NearestInRegion(const SimMap& Map, const std::vector<int32_t>& Labels, int32_t Region, Cell C, int32_t Radius, Cell& Out);

	private:
		struct OpenEntry
		{
			int32_t F;
			int32_t H;
			int32_t Index;
		};

		void Prepare(const SimMap& Map);
		void PushOpen(OpenEntry E);
		OpenEntry PopOpen();

		std::vector<int32_t> GCost;
		std::vector<int32_t> Parent;
		std::vector<uint32_t> Visited;    // geração em que a célula foi vista
		std::vector<uint32_t> Closed;
		std::vector<OpenEntry> Open;      // heap binário
		uint32_t Generation = 0;
	};
}
