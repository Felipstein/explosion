#include "Sim/SimMap.h"

#include "Sim/Hash.h"

namespace Sim
{
	void SimMap::Resize(int32_t InWidth, int32_t InHeight, Fix InCellSize, Vec2 InOrigin)
	{
		Width = InWidth;
		Height = InHeight;
		CellSize = InCellSize;
		Origin = InOrigin;
		Pass.assign(size_t(Width) * size_t(Height), uint8_t(CellPass::Open));
		Clearance.assign(Pass.size(), 0);
		MinDm.clear();
		MaxDm.clear();
	}

	void SimMap::ComputeClearance()
	{
		// Transformada de distância em duas passadas (métrica do xadrez): bloqueio = 0,
		// e fora do mapa conta como bloqueio (a borda fica com folga 1).
		constexpr int32_t Far = 255;
		Clearance.assign(Pass.size(), 0);
		auto At = [this](int32_t X, int32_t Y) -> int32_t
		{
			return InBounds(X, Y) ? int32_t(Clearance[Index(X, Y)]) : 0;
		};
		for (int32_t Y = 0; Y < Height; ++Y)
		{
			for (int32_t X = 0; X < Width; ++X)
			{
				if (CellPass(Pass[Index(X, Y)]) != CellPass::Open)
				{
					continue;
				}
				int32_t D = Far;
				D = D < At(X - 1, Y - 1) + 1 ? D : At(X - 1, Y - 1) + 1;
				D = D < At(X, Y - 1) + 1 ? D : At(X, Y - 1) + 1;
				D = D < At(X + 1, Y - 1) + 1 ? D : At(X + 1, Y - 1) + 1;
				D = D < At(X - 1, Y) + 1 ? D : At(X - 1, Y) + 1;
				Clearance[Index(X, Y)] = uint8_t(D);
			}
		}
		for (int32_t Y = Height - 1; Y >= 0; --Y)
		{
			for (int32_t X = Width - 1; X >= 0; --X)
			{
				if (CellPass(Pass[Index(X, Y)]) != CellPass::Open)
				{
					continue;
				}
				int32_t D = Clearance[Index(X, Y)];
				D = D < At(X + 1, Y + 1) + 1 ? D : At(X + 1, Y + 1) + 1;
				D = D < At(X, Y + 1) + 1 ? D : At(X, Y + 1) + 1;
				D = D < At(X - 1, Y + 1) + 1 ? D : At(X - 1, Y + 1) + 1;
				D = D < At(X + 1, Y) + 1 ? D : At(X + 1, Y) + 1;
				Clearance[Index(X, Y)] = uint8_t(D);
			}
		}
	}

	Cell SimMap::CellOf(Vec2 P) const
	{
		const Vec2 Local = P - Origin;
		return Cell{ int32_t((Local.X / CellSize).Floor()), int32_t((Local.Y / CellSize).Floor()) };
	}

	Vec2 SimMap::CellCenter(Cell C) const
	{
		return Origin + Vec2::Make(CellSize * C.X + CellSize / 2, CellSize * C.Y + CellSize / 2);
	}

	int32_t SimMap::ClearanceFor(Fix Radius) const
	{
		// a unidade no centro da célula precisa de ceil(raio / célula) células livres em volta,
		// mais a própria: folga mínima 1 (raio menor que meia célula)
		const int64_t Cells = (Radius.Raw + CellSize.Raw / 2 + CellSize.Raw - 1) / CellSize.Raw;
		return Cells < 1 ? 1 : int32_t(Cells);
	}

	uint64_t SimMap::Hash() const
	{
		Hasher H;
		H.Add(Width);
		H.Add(Height);
		H.Add(CellSize);
		H.Add(Origin);
		H.AddBytes(Pass.data(), Pass.size());
		H.Add(uint64_t(MinDm.size()));
		for (size_t I = 0; I < MinDm.size() && I < MaxDm.size(); ++I)
		{
			H.Add(uint64_t(uint16_t(MinDm[I])) | (uint64_t(uint16_t(MaxDm[I])) << 16));
		}
		return H.Value();
	}
}
