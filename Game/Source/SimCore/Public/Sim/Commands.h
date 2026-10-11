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
	};

	/** Build: o tipo nos 16 bits de baixo e o lado pra onde o prédio olha (0 +X, 1 +Y, 2 −X, 3 −Y) em cima. */
	constexpr int32_t BuildValue(uint16_t Def, int32_t Dir) { return int32_t(Def) | ((Dir & 3) << 16); }
	constexpr uint16_t BuildDefOf(int32_t Value) { return uint16_t(Value & 0xFFFF); }
	constexpr int32_t BuildDirOf(int32_t Value) { return (Value >> 16) & 3; }

	struct Command
	{
		TickNumber Tick = 0;          // passo em que executa
		PlayerIndex Player = 0;
		CommandType Type = CommandType::Stop;
		Vec2 Point;
		EntityId Target = NoEntity;
		int32_t Value = 0;            // tipo de unidade/prédio, item da fila ou depósito (economia)
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
