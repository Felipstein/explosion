#pragma once

#include "CoreMinimal.h"

/**
 * Sorteio e ruído determinísticos do gerador: a mesma semente dá o mesmo mundo
 * em qualquer máquina (o multiplayer de RTS depende disso: cada jogador gera o
 * mapa a partir da semente). Nada aqui usa o relógio nem o rand() do sistema.
 */
namespace WorldNoise
{
	/** Embaralhador de 32 bits (finalizador do PCG / "lowbias32" de Chris Wellons). */
	FORCEINLINE uint32 Hash(uint32 X)
	{
		X ^= X >> 16; X *= 0x7feb352dU;
		X ^= X >> 15; X *= 0x846ca68bU;
		X ^= X >> 16;
		return X;
	}

	FORCEINLINE uint32 Hash(int32 X, int32 Y, uint32 Seed)
	{
		return Hash(uint32(X) * 0x8da6b343U ^ Hash(uint32(Y) * 0xd8163841U ^ Seed));
	}

	/** Valor em [0, 1) a partir do hash. */
	FORCEINLINE float Unit(uint32 H) { return float(H >> 8) * (1.0f / 16777216.0f); }

	/** Gerador PCG32 (O'Neill 2014): sequência longa e boa, estado de 64 bits. */
	struct FRng
	{
		uint64 State = 0;
		uint64 Inc = 1;

		explicit FRng(uint64 Seed, uint64 Stream = 0x5851f42d4c957f2dULL)
		{
			Inc = (Stream << 1u) | 1u;
			Next();
			State += Seed;
			Next();
		}

		uint32 Next()
		{
			const uint64 Old = State;
			State = Old * 6364136223846793005ULL + Inc;
			const uint32 XorShifted = uint32(((Old >> 18u) ^ Old) >> 27u);
			const uint32 Rot = uint32(Old >> 59u);
			return (XorShifted >> Rot) | (XorShifted << ((~Rot + 1u) & 31));
		}

		float Float() { return Unit(Next()); }                                   // [0, 1)
		float Range(float A, float B) { return A + (B - A) * Float(); }
		int32 Int(int32 N) { return int32(Next() % uint32(FMath::Max(N, 1))); }   // [0, N)
	};

	/**
	 * Ruído de gradiente 2D (Perlin 2002, interpolação quíntica), com o
	 * gradiente sorteado em ângulo contínuo pelo hash (menos viés de eixo que
	 * a tabela de 8/12 direções). Saída aproximadamente em [-1, 1].
	 */
	WORLDFORGE_API float Gradient(float X, float Y, uint32 Seed);

	/**
	 * Soma fractal (fBm): oitavas de frequência ×2 e amplitude ×Gain, cada uma
	 * girada ~37° (quebra o alinhamento da grade de todas caírem juntas).
	 * Normalizada pela soma das amplitudes: fica em ~[-1, 1].
	 */
	WORLDFORGE_API float Fbm(float X, float Y, uint32 Seed, int32 Octaves, float Gain = 0.5f);

	/**
	 * Multifractal "ridged" (Musgrave, Texturing & Modeling cap. 16): 1 − |ruído|
	 * ao quadrado vira cristas finas; cada oitava pesa pelo valor da anterior,
	 * então os detalhes se concentram nas cristas (vales lisos, picos ásperos).
	 * Saída em ~[0, 1].
	 */
	WORLDFORGE_API float Ridged(float X, float Y, uint32 Seed, int32 Octaves, float Gain = 0.5f);
}
