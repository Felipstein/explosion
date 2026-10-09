#include "Sim/Vision.h"

namespace Sim
{
	void VisionGrid::Init(int32_t InWidth, int32_t InHeight, int32_t InTeams)
	{
		W = InWidth > 0 ? InWidth : 0;
		H = InHeight > 0 ? InHeight : 0;
		TeamCount = InTeams > 0 ? InTeams : 0;
		const size_t Cells = size_t(W) * size_t(H) * size_t(TeamCount);
		Count.assign(Cells, 0);
		Seen.assign(Cells, 0);
		Versions.assign(size_t(TeamCount), 0);
		Discs.clear();
	}

	const VisionGrid::Disc& VisionGrid::DiscFor(int32_t RadiusCells, int64_t RadiusSqCells)
	{
		for (const Disc& D : Discs)
		{
			if (D.RadiusCells == RadiusCells && D.RadiusSqCells == RadiusSqCells)
			{
				return D;
			}
		}
		Disc D;
		D.RadiusCells = RadiusCells;
		D.RadiusSqCells = RadiusSqCells;
		for (int32_t Y = -RadiusCells; Y <= RadiusCells; ++Y)
		{
			for (int32_t X = -RadiusCells; X <= RadiusCells; ++X)
			{
				if (int64_t(X) * X + int64_t(Y) * Y <= RadiusSqCells)
				{
					D.DX.push_back(X);
					D.DY.push_back(Y);
				}
			}
		}
		Discs.push_back(D);
		return Discs.back();
	}

	void VisionGrid::Look(int32_t Team, int32_t CellIndex, int32_t RadiusCells, int64_t RadiusSqCells, int32_t Delta)
	{
		if (Team < 0 || Team >= TeamCount || CellIndex < 0 || CellIndex >= W * H || Delta == 0)
		{
			return;
		}
		const Disc& D = DiscFor(RadiusCells, RadiusSqCells);
		const int32_t CX = CellIndex % W;
		const int32_t CY = CellIndex / W;
		const size_t Base = size_t(Team) * size_t(W) * size_t(H);
		bool bChanged = false;
		for (size_t K = 0; K < D.DX.size(); ++K)
		{
			const int32_t X = CX + D.DX[K];
			const int32_t Y = CY + D.DY[K];
			if (X < 0 || Y < 0 || X >= W || Y >= H)
			{
				continue;
			}
			const size_t I = Base + size_t(Y) * size_t(W) + size_t(X);
			if (Delta > 0)
			{
				// teto do contador: mais de 65 mil olhos na mesma célula não existe, mas não estoura
				if (Count[I] < 0xFFFF)
				{
					bChanged |= Count[I] == 0;
					++Count[I];
				}
				bChanged |= Seen[I] == 0;
				Seen[I] = 1;
			}
			else if (Count[I] > 0)
			{
				--Count[I];
				bChanged |= Count[I] == 0;
			}
		}
		if (bChanged)
		{
			++Versions[size_t(Team)];
		}
	}

	bool VisionGrid::IsVisible(int32_t Team, int32_t CellIndex) const
	{
		if (Team < 0 || Team >= TeamCount || CellIndex < 0 || CellIndex >= W * H)
		{
			return false;
		}
		return Count[size_t(Team) * size_t(W) * size_t(H) + size_t(CellIndex)] > 0;
	}

	bool VisionGrid::IsExplored(int32_t Team, int32_t CellIndex) const
	{
		if (Team < 0 || Team >= TeamCount || CellIndex < 0 || CellIndex >= W * H)
		{
			return false;
		}
		return Seen[size_t(Team) * size_t(W) * size_t(H) + size_t(CellIndex)] != 0;
	}

	uint32_t VisionGrid::Version(int32_t Team) const
	{
		return Team >= 0 && Team < TeamCount ? Versions[size_t(Team)] : 0;
	}
}
