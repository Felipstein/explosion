#include "Sim/Pathfinder.h"

#include <algorithm>

namespace Sim
{
	namespace
	{
		constexpr int32_t StraightCost = 10;
		constexpr int32_t DiagonalCost = 14;

		// Ordem fixa dos vizinhos: o desempate depende dela.
		constexpr int32_t DirX[8] = { 1, 0, -1, 0, 1, -1, -1, 1 };
		constexpr int32_t DirY[8] = { 0, 1, 0, -1, 1, 1, -1, -1 };

		int32_t Octile(int32_t AX, int32_t AY, int32_t BX, int32_t BY)
		{
			const int32_t DX = AX > BX ? AX - BX : BX - AX;
			const int32_t DY = AY > BY ? AY - BY : BY - AY;
			const int32_t Lo = DX < DY ? DX : DY;
			const int32_t Hi = DX < DY ? DY : DX;
			return StraightCost * Hi + (DiagonalCost - StraightCost) * Lo;
		}

		Cell ClampToMap(const SimMap& Map, Cell C)
		{
			C.X = C.X < 0 ? 0 : (C.X >= Map.Width ? Map.Width - 1 : C.X);
			C.Y = C.Y < 0 ? 0 : (C.Y >= Map.Height ? Map.Height - 1 : C.Y);
			return C;
		}
	}

	bool Pathfinder::NearestWalkable(const SimMap& Map, Cell C, int32_t MinClearance, int32_t Radius, Cell& Out)
	{
		if (Map.IsWalkable(C.X, C.Y, MinClearance))
		{
			Out = C;
			return true;
		}
		for (int32_t R = 1; R <= Radius; ++R)
		{
			bool bFound = false;
			int32_t BestD2 = 0;
			for (int32_t DY = -R; DY <= R; ++DY)
			{
				for (int32_t DX = -R; DX <= R; ++DX)
				{
					const bool bOnRing = DX == -R || DX == R || DY == -R || DY == R;
					if (!bOnRing || !Map.IsWalkable(C.X + DX, C.Y + DY, MinClearance))
					{
						continue;
					}
					const int32_t D2 = DX * DX + DY * DY;
					if (!bFound || D2 < BestD2)
					{
						bFound = true;
						BestD2 = D2;
						Out = Cell{ C.X + DX, C.Y + DY };
					}
				}
			}
			if (bFound)
			{
				return true;
			}
		}
		return false;
	}

	std::vector<int32_t> Pathfinder::LabelRegions(const SimMap& Map, int32_t MinClearance, std::vector<int32_t>& OutLabels)
	{
		OutLabels.assign(size_t(Map.Width) * size_t(Map.Height), 0);
		std::vector<int32_t> Sizes(1, 0);
		std::vector<int32_t> Stack;
		static constexpr int32_t SideX[4] = { 1, -1, 0, 0 };
		static constexpr int32_t SideY[4] = { 0, 0, 1, -1 };
		for (int32_t Y = 0; Y < Map.Height; ++Y)
		{
			for (int32_t X = 0; X < Map.Width; ++X)
			{
				const int32_t Seed = Map.Index(X, Y);
				if (OutLabels[size_t(Seed)] != 0 || !Map.IsWalkable(X, Y, MinClearance))
				{
					continue;
				}
				// inunda a partir daqui (pilha explícita: o mapa inteiro pode ser uma região só)
				const int32_t Label = int32_t(Sizes.size());
				int32_t Count = 0;
				OutLabels[size_t(Seed)] = Label;
				Stack.push_back(Seed);
				while (!Stack.empty())
				{
					const int32_t I = Stack.back();
					Stack.pop_back();
					++Count;
					const int32_t CX = I % Map.Width;
					const int32_t CY = I / Map.Width;
					for (int32_t D = 0; D < 4; ++D)
					{
						const int32_t NX = CX + SideX[D];
						const int32_t NY = CY + SideY[D];
						if (Map.IsWalkable(NX, NY, MinClearance) && OutLabels[size_t(Map.Index(NX, NY))] == 0)
						{
							OutLabels[size_t(Map.Index(NX, NY))] = Label;
							Stack.push_back(Map.Index(NX, NY));
						}
					}
				}
				Sizes.push_back(Count);
			}
		}
		return Sizes;
	}

	bool Pathfinder::NearestInRegion(const SimMap& Map, const std::vector<int32_t>& Labels, int32_t Region, Cell C, int32_t Radius, Cell& Out)
	{
		const auto InRegion = [&](int32_t X, int32_t Y) { return Map.InBounds(X, Y) && Labels[size_t(Map.Index(X, Y))] == Region; };
		if (InRegion(C.X, C.Y))
		{
			Out = C;
			return true;
		}
		for (int32_t R = 1; R <= Radius; ++R)
		{
			bool bFound = false;
			int32_t BestD2 = 0;
			for (int32_t DY = -R; DY <= R; ++DY)
			{
				for (int32_t DX = -R; DX <= R; ++DX)
				{
					const bool bOnRing = DX == -R || DX == R || DY == -R || DY == R;
					if (!bOnRing || !InRegion(C.X + DX, C.Y + DY))
					{
						continue;
					}
					const int32_t D2 = DX * DX + DY * DY;
					if (!bFound || D2 < BestD2)
					{
						bFound = true;
						BestD2 = D2;
						Out = Cell{ C.X + DX, C.Y + DY };
					}
				}
			}
			if (bFound)
			{
				return true;
			}
		}
		return false;
	}

	bool Pathfinder::HasLineOfMovement(const SimMap& Map, Cell A, Cell B, int32_t MinClearance)
	{
		// Percorre todas as células que a reta entre os centros toca (supercobertura de Bresenham).
		int32_t X = A.X;
		int32_t Y = A.Y;
		int32_t DX = B.X > A.X ? B.X - A.X : A.X - B.X;
		int32_t DY = B.Y > A.Y ? B.Y - A.Y : A.Y - B.Y;
		const int32_t SX = B.X > A.X ? 1 : -1;
		const int32_t SY = B.Y > A.Y ? 1 : -1;
		int32_t Steps = 1 + DX + DY;
		int32_t Error = DX - DY;
		DX *= 2;
		DY *= 2;
		while (Steps > 0)
		{
			if (!Map.IsWalkable(X, Y, MinClearance))
			{
				return false;
			}
			if (Error > 0)
			{
				X += SX;
				Error -= DY;
			}
			else if (Error < 0)
			{
				Y += SY;
				Error += DX;
			}
			else
			{
				// a reta passa exatamente pelo canto: as duas vizinhas têm de estar livres
				if (!Map.IsWalkable(X + SX, Y, MinClearance) || !Map.IsWalkable(X, Y + SY, MinClearance))
				{
					return false;
				}
				X += SX;
				Y += SY;
				Error += DX - DY;
				--Steps;
			}
			--Steps;
		}
		return true;
	}

	void Pathfinder::Prepare(const SimMap& Map)
	{
		const size_t N = size_t(Map.Width) * size_t(Map.Height);
		if (GCost.size() != N)
		{
			GCost.assign(N, 0);
			Parent.assign(N, -1);
			Visited.assign(N, 0);
			Closed.assign(N, 0);
			Generation = 0;
		}
		++Generation;
		if (Generation == 0)
		{
			// deu a volta no contador: zera as marcas
			std::fill(Visited.begin(), Visited.end(), 0u);
			std::fill(Closed.begin(), Closed.end(), 0u);
			Generation = 1;
		}
		Open.clear();
	}

	void Pathfinder::PushOpen(OpenEntry E)
	{
		// heap binário mínimo por (F, H, Index): ordem total, desempate fixo
		auto Less = [](const OpenEntry& A, const OpenEntry& B)
		{
			if (A.F != B.F) { return A.F < B.F; }
			if (A.H != B.H) { return A.H < B.H; }
			return A.Index < B.Index;
		};
		Open.push_back(E);
		size_t I = Open.size() - 1;
		while (I > 0)
		{
			const size_t P = (I - 1) / 2;
			if (!Less(Open[I], Open[P]))
			{
				break;
			}
			std::swap(Open[I], Open[P]);
			I = P;
		}
	}

	Pathfinder::OpenEntry Pathfinder::PopOpen()
	{
		auto Less = [](const OpenEntry& A, const OpenEntry& B)
		{
			if (A.F != B.F) { return A.F < B.F; }
			if (A.H != B.H) { return A.H < B.H; }
			return A.Index < B.Index;
		};
		const OpenEntry Top = Open.front();
		Open.front() = Open.back();
		Open.pop_back();
		size_t I = 0;
		const size_t N = Open.size();
		while (true)
		{
			const size_t L = 2 * I + 1;
			const size_t R = L + 1;
			size_t Best = I;
			if (L < N && Less(Open[L], Open[Best])) { Best = L; }
			if (R < N && Less(Open[R], Open[Best])) { Best = R; }
			if (Best == I)
			{
				break;
			}
			std::swap(Open[I], Open[Best]);
			I = Best;
		}
		return Top;
	}

	bool Pathfinder::FindPath(const SimMap& Map, Vec2 From, Vec2 To, int32_t MinClearance, std::vector<Vec2>& OutWaypoints, bool* OutReached)
	{
		OutWaypoints.clear();
		if (OutReached)
		{
			*OutReached = false;
		}
		if (Map.Width <= 0 || Map.Height <= 0)
		{
			return false;
		}
		Cell Start = ClampToMap(Map, Map.CellOf(From));
		if (!NearestWalkable(Map, Start, MinClearance, 8, Start))
		{
			return false;
		}
		Cell Goal = ClampToMap(Map, Map.CellOf(To));
		bool bGoalExact = Map.CellOf(To) == Goal && Map.IsWalkable(Goal.X, Goal.Y, MinClearance);
		if (!bGoalExact && !NearestWalkable(Map, Goal, MinClearance, 24, Goal))
		{
			Goal = Start;
		}
		if (Start == Goal)
		{
			OutWaypoints.push_back(bGoalExact ? To : Map.CellCenter(Goal));
			if (OutReached)
			{
				*OutReached = bGoalExact;
			}
			return true;
		}

		Prepare(Map);
		const int32_t StartIndex = Map.Index(Start.X, Start.Y);
		const int32_t GoalIndex = Map.Index(Goal.X, Goal.Y);
		GCost[StartIndex] = 0;
		Parent[StartIndex] = -1;
		Visited[StartIndex] = Generation;
		const int32_t StartH = Octile(Start.X, Start.Y, Goal.X, Goal.Y);
		PushOpen({ StartH, StartH, StartIndex });

		int32_t Best = StartIndex;
		int32_t BestH = StartH;
		bool bReached = false;
		int32_t Expansions = 0;
		while (!Open.empty())
		{
			const OpenEntry E = PopOpen();
			if (Closed[E.Index] == Generation)
			{
				continue;
			}
			Closed[E.Index] = Generation;
			if (E.Index == GoalIndex)
			{
				bReached = true;
				break;
			}
			if (E.H < BestH || (E.H == BestH && E.Index < Best))
			{
				Best = E.Index;
				BestH = E.H;
			}
			if (++Expansions > MaxExpansions)
			{
				break;
			}
			const int32_t CX = E.Index % Map.Width;
			const int32_t CY = E.Index / Map.Width;
			for (int32_t D = 0; D < 8; ++D)
			{
				const int32_t NX = CX + DirX[D];
				const int32_t NY = CY + DirY[D];
				if (!Map.IsWalkable(NX, NY, MinClearance))
				{
					continue;
				}
				const bool bDiagonal = DirX[D] != 0 && DirY[D] != 0;
				if (bDiagonal && (!Map.IsWalkable(CX + DirX[D], CY, MinClearance) || !Map.IsWalkable(CX, CY + DirY[D], MinClearance)))
				{
					continue;   // não corta quina
				}
				const int32_t N = Map.Index(NX, NY);
				if (Closed[N] == Generation)
				{
					continue;
				}
				const int32_t G = GCost[E.Index] + (bDiagonal ? DiagonalCost : StraightCost);
				if (Visited[N] != Generation || G < GCost[N])
				{
					Visited[N] = Generation;
					GCost[N] = G;
					Parent[N] = E.Index;
					const int32_t H = Octile(NX, NY, Goal.X, Goal.Y);
					PushOpen({ G + H, H, N });
				}
			}
		}

		// refaz a cadeia de células, do fim pro começo
		const int32_t End = bReached ? GoalIndex : Best;
		std::vector<Cell> Cells;
		for (int32_t I = End; I != -1; I = Parent[I])
		{
			Cells.push_back(Cell{ I % Map.Width, I / Map.Width });
			if (I == StartIndex)
			{
				break;
			}
		}
		std::reverse(Cells.begin(), Cells.end());

		// puxa o barbante: só ficam os pontos onde a linha livre acaba
		size_t Anchor = 0;
		for (size_t I = 1; I + 1 < Cells.size(); ++I)
		{
			if (!HasLineOfMovement(Map, Cells[Anchor], Cells[I + 1], MinClearance))
			{
				OutWaypoints.push_back(Map.CellCenter(Cells[I]));
				Anchor = I;
			}
		}
		const bool bExactEnd = bReached && bGoalExact;
		OutWaypoints.push_back(bExactEnd ? To : Map.CellCenter(Cells.back()));
		if (OutReached)
		{
			*OutReached = bReached;
		}
		return true;
	}
}
