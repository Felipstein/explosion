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
		Crushable = 3, // (16/10) o carro de pé: fechada pra quem não esmaga, aberta pra quem esmaga (o crusherGround do ZH)
		Toppleable = 4,// (v4, 23/10) a árvore de pé: aberta também pra quem a derruba pelo choque (o caminhão, o Tomahawk: KnocksOver)
	};

	/** A água de cada célula (13/10, a grade do WorldForge, grid.png: R). */
	enum class WaterState : uint8_t
	{
		Dry = 0,
		Shore = 1,   // a margem: passa
		Ford = 2,    // o vau raso: passa
		Deep = 3,    // fundo: bloqueia a unidade de chão (CellPass::Water)
		Ice = 4,     // o lago congelado do inverno: passa
	};

	/** Um corpo d'água (o grid.json do WorldForge): o nível em mm inteiro e, no rio, a correnteza. */
	struct WaterBody
	{
		uint8_t Id = 0;
		uint8_t Kind = 0;      // 0 lago, 1 oásis, 2 rio, 3 mar, 4 gelo
		int32_t LevelMm = 0;
		int32_t FlowXMmS = 0;
		int32_t FlowYMmS = 0;
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
		/** (16/10) A folga de quem esmaga: a célula esmagável (o carro) e a da árvore contam como abertas. */
		std::vector<uint8_t> CrushClearance;
		/** (v4, 23/10) A folga de quem derruba a árvore pelo choque: só a célula da árvore conta como aberta (o carro não). */
		std::vector<uint8_t> ToppleClearance;
		/** O bit da mobilidade de quem esmaga (o carro e a árvore): IsWalkable lê a CrushClearance. As folgas cabem abaixo dele. */
		static constexpr int32_t CrusherBit = 0x40;
		/** (v4) O bit de quem derruba a árvore pelo choque (sem esmagar): IsWalkable lê a ToppleClearance. Com os dois, vale o de esmagar. */
		static constexpr int32_t ToppleBit = 0x80;
		/** A folga sem os bits da mobilidade. */
		static constexpr int32_t ClearanceOf(int32_t Mobility) { return Mobility & ~(CrusherBit | ToppleBit); }
		/** Altura do chão por célula, a menor e a maior (decímetros). Vazio = sem altura (chão plano). */
		std::vector<int16_t> MinDm;
		std::vector<int16_t> MaxDm;

		/** A água do WorldForge por célula (WaterState) e o id do corpo; vazio = sem a grade (a água vem da altura). */
		std::vector<uint8_t> Water;
		std::vector<uint8_t> WaterBodyId;
		std::vector<WaterBody> WaterBodies;

		bool HasHeights() const { return !MinDm.empty() && MinDm.size() == Pass.size() && MaxDm.size() == Pass.size(); }
		bool HasWater() const { return !Water.empty() && Water.size() == Pass.size() && WaterBodyId.size() == Pass.size(); }
		WaterState WaterAt(int32_t X, int32_t Y) const { return HasWater() && InBounds(X, Y) ? WaterState(Water[Index(X, Y)]) : WaterState::Dry; }

		void Resize(int32_t InWidth, int32_t InHeight, Fix InCellSize, Vec2 InOrigin);
		/** Recalcula a folga depois de mudar a passabilidade. */
		void ComputeClearance();
		/** Mode: 0 só o aberto, 1 quem esmaga (o carro e a árvore abertos), 2 quem derruba a árvore (só ela aberta). */
		void ComputeClearanceInto(std::vector<uint8_t>& Out, int32_t Mode) const;

		bool InBounds(int32_t X, int32_t Y) const { return X >= 0 && Y >= 0 && X < Width && Y < Height; }
		int32_t Index(int32_t X, int32_t Y) const { return Y * Width + X; }
		CellPass PassAt(int32_t X, int32_t Y) const { return InBounds(X, Y) ? CellPass(Pass[Index(X, Y)]) : CellPass::Blocked; }
		/** Livre pra uma unidade que precisa de MinClearance células de folga em volta (1 = só a própria). */
		bool IsWalkable(int32_t X, int32_t Y, int32_t MinClearance) const
		{
			if (!InBounds(X, Y))
			{
				return false;
			}
			const int32_t Need = ClearanceOf(MinClearance);
			// (a cópia do mapa sem a folga da árvore, a do caminho seguro da IA, lê a comum: quem derruba contorna, como antes)
			return (MinClearance & CrusherBit) ? CrushClearance[Index(X, Y)] >= Need
				: ((MinClearance & ToppleBit) && !ToppleClearance.empty() ? ToppleClearance[Index(X, Y)] >= Need : Clearance[Index(X, Y)] >= Need);
		}

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
