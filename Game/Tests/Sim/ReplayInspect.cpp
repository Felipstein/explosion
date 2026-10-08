// Inspetor de replay (depuração, fora do jogo): roda a partida gravada passo a
// passo, confere as impressões digitais e conta a história de uma unidade
// (cada mudança de ordem, alvo e caminho) e o mapa em volta dela.
//
//   replayinspect <arquivo.rtsreplay> <id da unidade> [passo do retrato]

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

#include "Sim/Pathfinder.h"
#include "Sim/Replay.h"
#include "Sim/World.h"

using namespace Sim;

namespace
{
	const char* OrderName(OrderType O)
	{
		switch (O)
		{
		case OrderType::Idle: return "parada";
		case OrderType::Move: return "andar";
		case OrderType::Attack: return "atacar";
		case OrderType::AttackMove: return "ataque andando";
		}
		return "?";
	}

	void PrintGrid(const World& W, const UnitView& V, int32_t Radius)
	{
		const SimMap& M = W.Map();
		const Cell C = M.CellOf(V.Pos);
		std::printf("  mapa em volta (celula %d,%d; folga pedida %d). '#' bloqueada, '~' agua, digito = folga, '@' a unidade, 'o' outra unidade\n",
			C.X, C.Y, M.ClearanceFor(W.Rules().Units[V.Def].PathRadius));
		std::vector<Cell> Others;
		for (int32_t I = 0; I < W.UnitCount(); ++I)
		{
			const UnitView O = W.Unit(I);
			if (O.Id != V.Id)
			{
				Others.push_back(M.CellOf(O.Pos));
			}
		}
		// Y cresce pra cima na tela (norte em cima)
		for (int32_t Y = C.Y + Radius; Y >= C.Y - Radius; --Y)
		{
			std::printf("  ");
			for (int32_t X = C.X - Radius; X <= C.X + Radius; ++X)
			{
				char Ch = '#';
				if (M.InBounds(X, Y))
				{
					const CellPass P = M.PassAt(X, Y);
					const int32_t F = M.Clearance[size_t(M.Index(X, Y))];
					Ch = P == CellPass::Water ? '~' : P == CellPass::Blocked ? '#' : char('0' + (F > 9 ? 9 : F));
				}
				for (const Cell& O : Others)
				{
					if (O.X == X && O.Y == Y)
					{
						Ch = 'o';
					}
				}
				if (X == C.X && Y == C.Y)
				{
					Ch = '@';
				}
				std::putchar(Ch);
			}
			std::putchar('\n');
		}
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::printf("uso: replayinspect <arquivo.rtsreplay> <id> [passo]\n");
		return 2;
	}
	std::ifstream File(Argv[1], std::ios::binary);
	const std::vector<uint8_t> Bytes((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
	ReplayReader Reader;
	if (!Reader.Load(Bytes))
	{
		std::printf("replay invalido (%zu bytes)\n", Bytes.size());
		return 1;
	}
	const EntityId Watch = EntityId(std::atoi(Argv[2]));
	const TickNumber Portrait = Argc > 3 ? TickNumber(std::atoi(Argv[3])) : 0;

	World W;
	if (!W.Init(Reader.Setup()))
	{
		std::printf("montagem invalida\n");
		return 1;
	}
	UnitView Last;
	bool bHasLast = false;
	int32_t Mismatches = 0;
	// mesma convenção do teste: os comandos do passo T rodam no passo que começa em T
	for (TickNumber T = 0; T < Reader.LastTick() + 1 && !W.IsMatchOver(); ++T)
	{
		W.Step(Reader.CommandsFor(T));
		uint64_t Expected = 0;
		if (Reader.ExpectedHash(T, Expected) && Expected != W.StateHash())
		{
			if (Mismatches++ == 0)
			{
				std::printf("DESSINCRONIA no passo %u\n", W.Tick());
			}
		}
		UnitView V;
		if (!W.FindUnit(Watch, V))
		{
			if (bHasLast)
			{
				std::printf("passo %5u  #%u morreu\n", W.Tick(), Watch);
				bHasLast = false;
			}
			continue;
		}
		const bool bChanged = !bHasLast || V.Order != Last.Order || V.Engage != Last.Engage || V.FireTarget != Last.FireTarget
			|| (V.PathLeft == 0) != (Last.PathLeft == 0) || V.OrderPoint.X.Raw != Last.OrderPoint.X.Raw || V.OrderPoint.Y.Raw != Last.OrderPoint.Y.Raw;
		if (bChanged)
		{
			std::printf("passo %5u  (%.1f, %.1f)  %s -> (%.0f, %.0f)  persegue %u  mira %u  caminho %d  vel %.2f\n",
				W.Tick(), V.Pos.X.ToDouble(), V.Pos.Y.ToDouble(), OrderName(V.Order), V.OrderPoint.X.ToDouble(), V.OrderPoint.Y.ToDouble(),
				V.Engage, V.FireTarget, V.PathLeft, V.Speed.ToDouble());
		}
		if (Portrait != 0 && W.Tick() == Portrait)
		{
			PrintGrid(W, V, 12);
			// o caminho que o A* acharia daqui até a ordem
			std::vector<Vec2> Points;
			bool bReached = false;
			const int32_t Clear = W.Map().ClearanceFor(W.Rules().Units[V.Def].PathRadius);
			Pathfinder Finder;
			const bool bFound = Finder.FindPath(W.Map(), V.Pos, V.OrderPoint, Clear, Points, &bReached);
			std::printf("  A* ate a ordem: achou %d, chegou %d, %zu pontos", int(bFound), int(bReached), Points.size());
			if (!Points.empty())
			{
				std::printf(", ultimo (%.1f, %.1f)", Points.back().X.ToDouble(), Points.back().Y.ToDouble());
			}
			std::printf("\n");
		}
		Last = V;
		bHasLast = true;
	}
	std::printf("fim no passo %u, %d dessincronia(s)\n", W.Tick(), Mismatches);
	return 0;
}
