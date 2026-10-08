#include "Sim/FixedMath.h"

namespace Sim
{
	namespace
	{
#include "TrigTables.inl"

		constexpr int32_t TableSteps = 1024;                              // amostras por quarto de onda / por 45°
		constexpr int32_t BamPerStep = Angle::QuarterTurn / TableSteps;   // 16

		/** Interpola a tabela com Index16 em [0, 16384] (4 bits de fração entre amostras). */
		int32_t Lookup(const int32_t* Table, uint32_t Index16)
		{
			const uint32_t Step = Index16 / BamPerStep;
			const int32_t Frac = int32_t(Index16 % BamPerStep);
			if (Step >= uint32_t(TableSteps))
			{
				return Table[TableSteps];
			}
			const int32_t A = Table[Step];
			const int32_t B = Table[Step + 1];
			return A + ((B - A) * Frac + BamPerStep / 2) / BamPerStep;
		}

		/** atan(Num/Den) em bam, com Num ≤ Den. */
		int32_t AtanRatio(uint64_t Num, uint64_t Den)
		{
			while (Den > (uint64_t(1) << 48))
			{
				Num >>= 1;
				Den >>= 1;
			}
			const uint64_t Index16 = Den == 0 ? 0 : (Num * uint64_t(Angle::QuarterTurn)) / Den;
			return Lookup(AtanTable, uint32_t(Index16));
		}
	}

	uint64_t ISqrt(uint64_t V)
	{
		uint64_t Result = 0;
		uint64_t Bit = uint64_t(1) << 62;
		while (Bit > V)
		{
			Bit >>= 2;
		}
		while (Bit != 0)
		{
			if (V >= Result + Bit)
			{
				V -= Result + Bit;
				Result = (Result >> 1) + Bit;
			}
			else
			{
				Result >>= 1;
			}
			Bit >>= 2;
		}
		return Result;
	}

	Fix Sqrt(Fix V)
	{
		if (V.Raw <= 0)
		{
			return Fix::Zero();
		}
		// sqrt(Raw · 2^16) = sqrt(valor) · 2^16: o resultado já sai em Q16
		if (V.Raw < (int64_t(1) << 47))
		{
			return Fix::FromRaw(int64_t(ISqrt(uint64_t(V.Raw) << Fix::FracBits)));
		}
		return Fix::FromRaw(int64_t(ISqrt(uint64_t(V.Raw))) << (Fix::FracBits / 2));
	}

	Fix Sin(Angle A)
	{
		const uint32_t Quadrant = A.Bam / uint32_t(Angle::QuarterTurn);
		const uint32_t InQuarter = A.Bam % uint32_t(Angle::QuarterTurn);
		switch (Quadrant)
		{
		case 0: return Fix::FromRaw(Lookup(SinQuarterTable, InQuarter));
		case 1: return Fix::FromRaw(Lookup(SinQuarterTable, Angle::QuarterTurn - InQuarter));
		case 2: return Fix::FromRaw(-Lookup(SinQuarterTable, InQuarter));
		default: return Fix::FromRaw(-Lookup(SinQuarterTable, Angle::QuarterTurn - InQuarter));
		}
	}

	Fix Cos(Angle A)
	{
		return Sin(Angle::FromBam(int32_t(A.Bam) + Angle::QuarterTurn));
	}

	Angle Atan2(Fix Y, Fix X)
	{
		if (X.Raw == 0 && Y.Raw == 0)
		{
			return Angle::FromBam(0);
		}
		const uint64_t AbsX = uint64_t(X.Raw < 0 ? -X.Raw : X.Raw);
		const uint64_t AbsY = uint64_t(Y.Raw < 0 ? -Y.Raw : Y.Raw);
		// ângulo no 1º quadrante, de 0 a 90°
		const int32_t InFirst = AbsX >= AbsY
			? AtanRatio(AbsY, AbsX)
			: Angle::QuarterTurn - AtanRatio(AbsX, AbsY);
		if (X.Raw >= 0)
		{
			return Angle::FromBam(Y.Raw >= 0 ? InFirst : Angle::FullTurn - InFirst);
		}
		return Angle::FromBam(Y.Raw >= 0 ? Angle::HalfTurn - InFirst : Angle::HalfTurn + InFirst);
	}

	Fix Vec2::Length() const
	{
		// √(Q32) = Q16
		return Fix::FromRaw(int64_t(ISqrt(uint64_t(LengthSqRaw()))));
	}

	Vec2 Vec2::Normalized() const
	{
		const Fix Len = Length();
		if (Len.Raw == 0)
		{
			return Zero();
		}
		return Make(X / Len, Y / Len);
	}
}
