#pragma once

// A "impressão digital" do estado: um hash de 64 bits que todo PC calcula a cada
// passo. Se dois PCs discordam, a partida dessincronizou (docs/JOGO.md §3.2 item 8).

#include <cstdint>
#include <string>

#include "Sim/FixedMath.h"

namespace Sim
{
	class Hasher
	{
	public:
		void Add(uint64_t V)
		{
			// mistura do splitmix64 por palavra: rápido e sem colisões triviais
			H ^= V + 0x9E3779B97F4A7C15ULL + (H << 6) + (H >> 2);
			H = Mix(H);
		}
		void Add(int64_t V) { Add(uint64_t(V)); }
		void Add(uint32_t V) { Add(uint64_t(V)); }
		void Add(int32_t V) { Add(uint64_t(uint32_t(V))); }
		void Add(uint16_t V) { Add(uint64_t(V)); }
		void Add(uint8_t V) { Add(uint64_t(V)); }
		void Add(bool V) { Add(uint64_t(V ? 1 : 0)); }
		void Add(Fix V) { Add(V.Raw); }
		void Add(Angle V) { Add(uint64_t(V.Bam)); }
		void Add(Vec2 V) { Add(V.X.Raw); Add(V.Y.Raw); }
		void Add(const std::string& S)
		{
			Add(uint64_t(S.size()));
			for (const char C : S)
			{
				Add(uint64_t(uint8_t(C)));
			}
		}
		void AddBytes(const uint8_t* Data, size_t Size)
		{
			Add(uint64_t(Size));
			size_t I = 0;
			for (; I + 8 <= Size; I += 8)
			{
				uint64_t Word = 0;
				for (int B = 0; B < 8; ++B)
				{
					Word |= uint64_t(Data[I + B]) << (8 * B);
				}
				Add(Word);
			}
			uint64_t Tail = 0;
			for (int B = 0; I < Size; ++I, ++B)
			{
				Tail |= uint64_t(Data[I]) << (8 * B);
			}
			Add(Tail);
		}

		uint64_t Value() const { return Mix(H ^ 0xD6E8FEB86659FD93ULL); }

	private:
		static uint64_t Mix(uint64_t X)
		{
			X ^= X >> 30; X *= 0xBF58476D1CE4E5B9ULL;
			X ^= X >> 27; X *= 0x94D049BB133111EBULL;
			X ^= X >> 31;
			return X;
		}

		uint64_t H = 0x243F6A8885A308D3ULL;
	};
}
