#pragma once

// Sorteio da simulação: PCG32 (O'Neill 2014). Mesmo estado = mesma sequência em
// qualquer máquina. Só a simulação usa este; os efeitos visuais têm o deles
// (sorteio cosmético nunca toca a partida).

#include <cstdint>

namespace Sim
{
	class Rng
	{
	public:
		explicit Rng(uint64_t Seed = 1, uint64_t Stream = 0x5851f42d4c957f2dULL)
		{
			Inc = (Stream << 1u) | 1u;
			Next();
			State += Seed;
			Next();
		}

		uint32_t Next()
		{
			const uint64_t Old = State;
			State = Old * 6364136223846793005ULL + Inc;
			const uint32_t XorShifted = uint32_t(((Old >> 18u) ^ Old) >> 27u);
			const uint32_t Rot = uint32_t(Old >> 59u);
			return (XorShifted >> Rot) | (XorShifted << ((~Rot + 1u) & 31u));
		}

		/** Inteiro em [0, N). N ≤ 0 dá 0. */
		int32_t Below(int32_t N) { return N <= 0 ? 0 : int32_t(Next() % uint32_t(N)); }

		uint64_t StateForHash() const { return State ^ (Inc << 1); }

	private:
		uint64_t State = 0;
		uint64_t Inc = 1;
	};
}
