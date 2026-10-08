#pragma once

#include "Sim/SimApi.h"

// Matemática exata da simulação: ponto fixo, ângulo binário e vetor 2D.
//
// Por que não float: no lockstep todo PC roda a mesma simulação e tem de chegar
// no mesmo resultado, bit por bit. Com float isso depende do compilador, das
// flags (o jogo da Unreal compila com /fp:fast, o editor com /fp:precise) e da
// CPU (sin/cos da CRT mudam com FMA3). Inteiro dá o mesmo resultado em qualquer
// lugar. É o caminho do OpenRA (docs/JOGO.md §3.3).
//
// Este arquivo é C++ puro: a simulação não sabe que a Unreal existe.

#include <cstdint>

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

namespace Sim
{
	/** Número em ponto fixo Q48.16: 16 bits de fração (1/65536). Em metros, a resolução é ~0,015 mm. */
	struct Fix
	{
		static constexpr int FracBits = 16;
		static constexpr int64_t OneRaw = int64_t(1) << FracBits;

		int64_t Raw = 0;

		static constexpr Fix FromRaw(int64_t InRaw) { Fix F; F.Raw = InRaw; return F; }
		static constexpr Fix FromInt(int64_t Value) { return FromRaw(Value * OneRaw); }
		/** Milésimos (ex.: 7900 = 7,9). Os dados entram assim, sem float. */
		static constexpr Fix FromMilli(int64_t Milli) { return FromRaw(RoundedDiv(Milli * OneRaw, 1000)); }
		static constexpr Fix FromRatio(int64_t Num, int64_t Den) { return FromRaw(RoundedDiv(Num * OneRaw, Den)); }

		static constexpr Fix Zero() { return FromRaw(0); }
		static constexpr Fix One() { return FromRaw(OneRaw); }
		static constexpr Fix Half() { return FromRaw(OneRaw / 2); }

		/** Parte inteira arredondada pra baixo (−1,5 → −2). */
		constexpr int64_t Floor() const { return Raw >> FracBits; }
		/** Arredondada pro mais perto (meio pra cima). */
		constexpr int64_t Round() const { return (Raw + OneRaw / 2) >> FracBits; }
		/** Só pra apresentação e testes: a simulação nunca lê isto. */
		constexpr double ToDouble() const { return double(Raw) / double(OneRaw); }

		constexpr Fix operator-() const { return FromRaw(-Raw); }
		constexpr Fix operator+(Fix B) const { return FromRaw(Raw + B.Raw); }
		constexpr Fix operator-(Fix B) const { return FromRaw(Raw - B.Raw); }
		Fix operator*(Fix B) const { return FromRaw(MulShift(Raw, B.Raw)); }
		Fix operator/(Fix B) const { return FromRaw(DivShift(Raw, B.Raw)); }
		constexpr Fix operator*(int64_t K) const { return FromRaw(Raw * K); }
		/** Divisão por inteiro: trunca em direção a zero (definido em C++). */
		constexpr Fix operator/(int64_t K) const { return FromRaw(Raw / K); }

		Fix& operator+=(Fix B) { Raw += B.Raw; return *this; }
		Fix& operator-=(Fix B) { Raw -= B.Raw; return *this; }
		Fix& operator*=(Fix B) { Raw = MulShift(Raw, B.Raw); return *this; }

		constexpr bool operator==(Fix B) const { return Raw == B.Raw; }
		constexpr bool operator!=(Fix B) const { return Raw != B.Raw; }
		constexpr bool operator<(Fix B) const { return Raw < B.Raw; }
		constexpr bool operator<=(Fix B) const { return Raw <= B.Raw; }
		constexpr bool operator>(Fix B) const { return Raw > B.Raw; }
		constexpr bool operator>=(Fix B) const { return Raw >= B.Raw; }

		/** (A·B) >> 16 com o produto em 128 bits (estouro de int64 com sinal é indefinido em C++). Arredonda meio pra cima. */
		static int64_t MulShift(int64_t A, int64_t B)
		{
#if defined(_MSC_VER) && !defined(__clang__)
			int64_t High = 0;
			const uint64_t Low = uint64_t(_mul128(A, B, &High));
			const uint64_t Rounded = Low + (uint64_t(1) << (FracBits - 1));
			High += Rounded < Low ? 1 : 0;
			return int64_t((Rounded >> FracBits) | (uint64_t(High) << (64 - FracBits)));
#else
			const __int128 Product = (__int128)A * B + (__int128(1) << (FracBits - 1));
			return int64_t(Product >> FracBits);
#endif
		}

		/** (A << 16) / B em 128 bits. Trunca em direção a zero. B = 0 dá 0 (a simulação nunca deve dividir por zero; o teste acusa). */
		static int64_t DivShift(int64_t A, int64_t B)
		{
			if (B == 0)
			{
				return 0;
			}
#if defined(_MSC_VER) && !defined(__clang__)
			const int64_t High = A >> (64 - FracBits);          // sinal estendido dos 16 bits que saem
			const uint64_t Low = uint64_t(A) << FracBits;
			int64_t Remainder = 0;
			return _div128(High, int64_t(Low), B, &Remainder);
#else
			return int64_t(((__int128)A << FracBits) / B);
#endif
		}

		static constexpr int64_t RoundedDiv(int64_t Num, int64_t Den)
		{
			// arredonda pro mais perto, simétrico em torno de zero
			return (Num >= 0) == (Den >= 0) ? (Num + Den / 2) / Den : (Num - Den / 2) / Den;
		}
	};

	constexpr Fix Abs(Fix A) { return A.Raw < 0 ? -A : A; }
	constexpr Fix Min(Fix A, Fix B) { return A < B ? A : B; }
	constexpr Fix Max(Fix A, Fix B) { return A > B ? A : B; }
	constexpr Fix Clamp(Fix V, Fix Lo, Fix Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }

	/** Raiz quadrada inteira de 64 bits (maior R com R² ≤ V). */
	SIMCORE_API uint64_t ISqrt(uint64_t V);
	/** Raiz em ponto fixo. Negativo dá 0. */
	SIMCORE_API Fix Sqrt(Fix V);

	/**
	 * Ângulo binário: 65536 = 360°, dá a volta sozinho no estouro de uint16.
	 * 0 = eixo +X; cresce em direção a +Y (o mesmo sentido do yaw da Unreal).
	 */
	struct Angle
	{
		static constexpr int32_t FullTurn = 65536;
		static constexpr int32_t HalfTurn = 32768;
		static constexpr int32_t QuarterTurn = 16384;

		uint16_t Bam = 0;

		static constexpr Angle FromBam(int32_t InBam) { Angle A; A.Bam = uint16_t(InBam & 0xFFFF); return A; }
		/** Graus em milésimos (ex.: 6000 = 6°). */
		static constexpr Angle FromMilliDeg(int64_t MilliDeg) { return FromBam(int32_t(Fix::RoundedDiv(MilliDeg * FullTurn, 360000))); }
		constexpr double ToDegrees() const { return double(Bam) * 360.0 / double(FullTurn); }

		/** Diferença com sinal (Target − this) em [−32768, 32767]: o caminho mais curto. */
		constexpr int32_t DeltaTo(Angle Target) const { return int32_t(int16_t(uint16_t(Target.Bam - Bam))); }
		/** Gira em direção ao alvo no máximo MaxStep (em bam). */
		constexpr Angle RotatedTowards(Angle Target, int32_t MaxStep) const
		{
			const int32_t Delta = DeltaTo(Target);
			const int32_t Step = Delta > MaxStep ? MaxStep : (Delta < -MaxStep ? -MaxStep : Delta);
			return FromBam(int32_t(Bam) + Step);
		}
		constexpr bool operator==(Angle B) const { return Bam == B.Bam; }
		constexpr bool operator!=(Angle B) const { return Bam != B.Bam; }
	};

	constexpr int32_t AbsInt(int32_t V) { return V < 0 ? -V : V; }

	/** Seno e cosseno pela tabela de quarto de onda (Private/TrigTables.inl), com interpolação linear. */
	SIMCORE_API Fix Sin(Angle A);
	SIMCORE_API Fix Cos(Angle A);
	/** Ângulo do vetor (X, Y). (0, 0) dá 0. */
	SIMCORE_API Angle Atan2(Fix Y, Fix X);

	struct SIMCORE_API Vec2
	{
		Fix X;
		Fix Y;

		static constexpr Vec2 Make(Fix InX, Fix InY) { Vec2 V; V.X = InX; V.Y = InY; return V; }
		static constexpr Vec2 Zero() { return Make(Fix::Zero(), Fix::Zero()); }
		static Vec2 FromAngle(Angle A) { return Make(Cos(A), Sin(A)); }

		constexpr Vec2 operator+(Vec2 B) const { return Make(X + B.X, Y + B.Y); }
		constexpr Vec2 operator-(Vec2 B) const { return Make(X - B.X, Y - B.Y); }
		constexpr Vec2 operator-() const { return Make(-X, -Y); }
		Vec2 operator*(Fix K) const { return Make(X * K, Y * K); }
		Vec2 operator/(Fix K) const { return Make(X / K, Y / K); }
		Vec2& operator+=(Vec2 B) { X += B.X; Y += B.Y; return *this; }
		Vec2& operator-=(Vec2 B) { X -= B.X; Y -= B.Y; return *this; }
		constexpr bool operator==(Vec2 B) const { return X == B.X && Y == B.Y; }
		constexpr bool operator!=(Vec2 B) const { return !(*this == B); }

		Fix Dot(Vec2 B) const { return X * B.X + Y * B.Y; }
		Fix Length() const;
		/** Comprimento ao quadrado em Q32 (raw × raw), sem perder precisão: bom pra comparar distâncias. */
		constexpr int64_t LengthSqRaw() const { return X.Raw * X.Raw + Y.Raw * Y.Raw; }
		/** Vetor unitário; o vetor nulo continua nulo. */
		Vec2 Normalized() const;
		Angle Heading() const { return Atan2(Y, X); }
	};

	/** Distância ao quadrado em Q32. Vale até ~2900 m (2^54 de Q32 cabe em int64 com folga). */
	constexpr int64_t DistSqRaw(Vec2 A, Vec2 B) { return (A - B).LengthSqRaw(); }
	/** R² em Q32, pra comparar com DistSqRaw. */
	constexpr int64_t RadiusSqRaw(Fix R) { return R.Raw * R.Raw; }
	inline Fix Distance(Vec2 A, Vec2 B) { return (A - B).Length(); }
}
