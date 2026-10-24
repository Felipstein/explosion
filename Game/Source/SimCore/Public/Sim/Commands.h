#pragma once

#include "Sim/SimApi.h"

// Os comandos: a única coisa que entra na simulação e a única que viaja na rede.
// Um clique vira um comando marcado com o passo em que vai executar.
//
// Seleção replicada (docs/JOGO.md §3.2 item 6): o jogador manda o grupo uma vez
// (SetGroup) e as ordens seguintes valem pro grupo. Assim uma ordem pra 300
// tanques não carrega 300 IDs.

#include <cstdint>
#include <vector>

#include "Sim/FixedMath.h"
#include "Sim/Types.h"

namespace Sim
{
	enum class CommandType : uint8_t
	{
		SetGroup = 1,     // troca o grupo de comando do jogador (Units)
		Move = 2,         // grupo anda até Point
		Attack = 3,       // grupo ataca Target
		Stop = 4,         // grupo para e esquece ordens
		AttackMove = 5,   // grupo anda até Point atacando o que encontrar
		SetFog = 6,       // liga (Target = 1) ou desliga (0) a névoa de guerra: depuração, vale pra todos
		// economia e construção (ZH): o prédio vai em Target, o tipo em Value
		Produce = 7,      // o prédio Target põe o tipo Value na fila
		CancelProduce = 8,// o prédio Target tira o item Value da fila (0 = o primeiro) e devolve o dinheiro
		Build = 9,        // um Dozer do grupo constrói o prédio Value (BuildValue) com centro em Point;
		                  // com Target, retoma a obra parada desse prédio
		CancelBuild = 10, // desiste da obra Target (o prédio some e o dinheiro volta)
		SetRally = 11,    // o ponto de encontro do prédio Target vira Point
		Gather = 12,      // os caminhões do grupo vão coletar no depósito Value (índice)
		// o tiro forçado do C&C (Ctrl, 13/10): no chão e em qualquer unidade (a própria, o aliado)
		AttackGround = 13,// grupo atira no chão em Point até mandarem outra coisa
		ForceAttack = 14, // grupo ataca Target mesmo sendo dele ou aliado
		// a rede (13/10, N5): render-se, ou a queda que o anfitrião manda pela vaga de quem caiu
		Resign = 15,      // o jogador desiste: tudo dele cai (o ZH, VictoryConditions::killPlayer)
		// (v4, 22/10) o poder do general: o poder Value (o índice em Ruleset::Powers, até 255) no ponto Point
		UsePower = 16,
		// (v4, 23/10) vender o prédio pronto Target (o BuildAssistant::sellObject do ZH): desmonta e devolve parte do custo
		Sell = 17,
		// (v4, 23/10) o Dozer do grupo mais perto conserta o prédio Target, dele ou do aliado (o DozerAIUpdate do ZH)
		Repair = 18,
	};

	/** Build: o tipo nos 16 bits de baixo e o lado pra onde o prédio olha (0 +X, 1 +Y, 2 −X, 3 −Y) em cima. */
	constexpr int32_t BuildValue(uint16_t Def, int32_t Dir) { return int32_t(Def) | ((Dir & 3) << 16); }
	constexpr uint16_t BuildDefOf(int32_t Value) { return uint16_t(Value & 0xFFFF); }
	constexpr int32_t BuildDirOf(int32_t Value) { return (Value >> 16) & 3; }

	/** Move/AttackMove: bit 0 = em formação (F segurado); bit 1 = com direção (o arraste do esquerdo); bits 2..7 =
	 *  a largura da linha em passos de 2 m (0 = a do grupo); bits 16..31 = a direção (bam). */
	constexpr int32_t MoveValue(bool bFormation, bool bFacing, int32_t WidthHalfM, uint16_t FacingBam)
	{
		return (bFormation ? 1 : 0) | (bFacing ? 2 : 0) | ((WidthHalfM < 0 ? 0 : (WidthHalfM > 63 ? 63 : WidthHalfM)) << 2) | (int32_t(FacingBam) << 16);
	}
	constexpr bool MoveFormationOf(int32_t Value) { return (Value & 1) != 0; }
	constexpr bool MoveHasFacingOf(int32_t Value) { return (Value & 2) != 0; }
	constexpr int32_t MoveWidthHalfMOf(int32_t Value) { return (Value >> 2) & 63; }
	constexpr uint16_t MoveFacingBamOf(int32_t Value) { return uint16_t(uint32_t(Value) >> 16); }

	struct Command
	{
		TickNumber Tick = 0;          // passo em que executa
		PlayerIndex Player = 0;
		CommandType Type = CommandType::Stop;
		Vec2 Point;
		EntityId Target = NoEntity;
		int32_t Value = 0;            // tipo de unidade/prédio, item da fila ou depósito (economia);
		                              // Move/AttackMove: MoveValue (formação, direção e largura do arraste); UsePower: o poder
		std::vector<EntityId> Units;  // só no SetGroup
	};

	/** Escrita binária little-endian (independente da máquina). */
	class ByteWriter
	{
	public:
		void U8(uint8_t V) { Bytes.push_back(V); }
		void U16(uint16_t V) { U8(uint8_t(V)); U8(uint8_t(V >> 8)); }
		void U32(uint32_t V) { U16(uint16_t(V)); U16(uint16_t(V >> 16)); }
		void U64(uint64_t V) { U32(uint32_t(V)); U32(uint32_t(V >> 32)); }
		void I64(int64_t V) { U64(uint64_t(V)); }
		void Raw(const uint8_t* Data, size_t Size) { Bytes.insert(Bytes.end(), Data, Data + Size); }

		std::vector<uint8_t> Bytes;
	};

	/** Leitura com checagem de limite em tudo: dado de rede ou de arquivo nunca é confiável. */
	class ByteReader
	{
	public:
		ByteReader(const uint8_t* InData, size_t InSize) : Data(InData), Size(InSize) {}

		bool U8(uint8_t& V) { if (Pos + 1 > Size) { return Fail(); } V = Data[Pos++]; return true; }
		bool U16(uint16_t& V) { uint8_t A, B; if (!U8(A) || !U8(B)) { return false; } V = uint16_t(A | (B << 8)); return true; }
		bool U32(uint32_t& V) { uint16_t A, B; if (!U16(A) || !U16(B)) { return false; } V = uint32_t(A) | (uint32_t(B) << 16); return true; }
		bool U64(uint64_t& V) { uint32_t A, B; if (!U32(A) || !U32(B)) { return false; } V = uint64_t(A) | (uint64_t(B) << 32); return true; }
		bool I64(int64_t& V) { uint64_t U; if (!U64(U)) { return false; } V = int64_t(U); return true; }
		bool Raw(uint8_t* Out, size_t Count) { if (Pos + Count > Size) { return Fail(); } for (size_t I = 0; I < Count; ++I) { Out[I] = Data[Pos + I]; } Pos += Count; return true; }

		bool AtEnd() const { return Pos == Size; }
		bool Failed() const { return bFailed; }

	private:
		bool Fail() { bFailed = true; return false; }

		const uint8_t* Data = nullptr;
		size_t Size = 0;
		size_t Pos = 0;
		bool bFailed = false;
	};

	SIMCORE_API void WriteCommand(ByteWriter& W, const Command& C);
	/** Falso se os bytes não formam um comando válido (tipo desconhecido, grupo grande demais, fim antes da hora). */
	SIMCORE_API bool ReadCommand(ByteReader& R, Command& Out);
}
