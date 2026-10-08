// Testes da simulação, sem a Unreal: compila com o cl.exe em segundos
// (Game/Tools/simtest.ps1) e roda sozinho.
//
// O que conferem:
//  1. a matemática exata (raiz, seno, atan2, multiplicação) contra double;
//  2. o caminho na grade (desvia de parede, acha a passagem);
//  3. os comandos e a montagem resistem a bytes quebrados (nada de travar);
//  4. uma batalha IA × IA inteira: roda duas vezes e pelo replay, e as
//     impressões digitais têm de ser idênticas passo a passo.
// No fim imprime FINAL_HASH: compilações diferentes (otimização, flags de
// ponto flutuante, compilador) têm de imprimir o mesmo número.

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "Sim/FixedMath.h"
#include "Sim/Lockstep.h"
#include "Sim/Pathfinder.h"
#include "Sim/Replay.h"
#include "Sim/SkirmishAI.h"
#include "Sim/World.h"

using namespace Sim;

namespace
{
	int Failures = 0;

	void Check(bool bOk, const char* What)
	{
		if (!bOk)
		{
			++Failures;
			std::printf("  FALHOU: %s\n", What);
		}
	}

	// ------------------------------------------------------------------ 1. matemática

	void TestMath()
	{
		std::printf("[matematica]\n");
		for (uint64_t V : { 0ull, 1ull, 2ull, 3ull, 4ull, 99ull, 100ull, 1ull << 40, (1ull << 62) + 12345ull, ~0ull })
		{
			const uint64_t R = ISqrt(V);
			const bool bExact = R * R <= V && (R + 1 > 0xFFFFFFFFull || (R + 1) * (R + 1) > V);
			Check(bExact, "ISqrt exato");
		}
		double WorstSqrt = 0;
		for (int64_t I = 1; I < 2000000; I += 997)
		{
			const double X = double(I) / 37.0;
			const double Got = Sqrt(Fix::FromRatio(I, 37)).ToDouble();
			WorstSqrt = std::fmax(WorstSqrt, std::fabs(Got - std::sqrt(X)) / std::sqrt(X));
		}
		std::printf("  raiz: erro relativo maximo %.2e\n", WorstSqrt);
		Check(WorstSqrt < 1e-4, "raiz com erro < 1e-4");

		double WorstSin = 0;
		for (int32_t B = 0; B < 65536; B += 7)
		{
			const double Rad = double(B) / 65536.0 * 2.0 * 3.14159265358979323846;
			WorstSin = std::fmax(WorstSin, std::fabs(Sin(Angle::FromBam(B)).ToDouble() - std::sin(Rad)));
			WorstSin = std::fmax(WorstSin, std::fabs(Cos(Angle::FromBam(B)).ToDouble() - std::cos(Rad)));
		}
		std::printf("  seno/cosseno: erro maximo %.2e\n", WorstSin);
		Check(WorstSin < 1e-4, "seno com erro < 1e-4");

		double WorstAtan = 0;
		for (int32_t Y = -50; Y <= 50; Y += 3)
		{
			for (int32_t X = -50; X <= 50; X += 3)
			{
				if (X == 0 && Y == 0)
				{
					continue;
				}
				const double Want = std::atan2(double(Y), double(X)) * 180.0 / 3.14159265358979323846;
				double Got = Atan2(Fix::FromInt(Y), Fix::FromInt(X)).ToDegrees();
				double Err = std::fabs(std::fmod(Got - Want + 540.0, 360.0) - 180.0);
				WorstAtan = std::fmax(WorstAtan, Err);
			}
		}
		std::printf("  atan2: erro maximo %.4f graus\n", WorstAtan);
		Check(WorstAtan < 0.01, "atan2 com erro < 0,01 grau");

		Check((Fix::FromInt(-3) * Fix::FromRatio(1, 2)) == Fix::FromRatio(-3, 2), "multiplicacao negativa");
		Check((Fix::FromInt(7) / Fix::FromInt(2)) == Fix::FromRatio(7, 2), "divisao");
		Check((Fix::FromInt(-7) / Fix::FromInt(2)) == Fix::FromRatio(-7, 2), "divisao negativa");
		Check((Fix::FromInt(100000) * Fix::FromInt(100000)) == Fix::FromInt(10000000000LL), "produto grande sem estouro");
		Check(Angle::FromBam(100).DeltaTo(Angle::FromBam(65000)) == -636, "diferenca de angulo pelo caminho curto");
	}

	// ------------------------------------------------------------------ 2. caminho

	SimMap MakeOpenMap(int32_t W, int32_t H)
	{
		SimMap M;
		M.Resize(W, H, Fix::FromInt(4), Vec2::Make(Fix::FromInt(-2 * W), Fix::FromInt(-2 * H)));
		return M;
	}

	void TestPathfinding()
	{
		std::printf("[caminho]\n");
		SimMap M = MakeOpenMap(64, 64);
		// parede vertical em X = 32 com uma passagem de 4 células em Y = 50..53
		for (int32_t Y = 0; Y < 64; ++Y)
		{
			if (Y < 50 || Y > 53)
			{
				M.Pass[M.Index(32, Y)] = uint8_t(CellPass::Blocked);
				M.Pass[M.Index(33, Y)] = uint8_t(CellPass::Blocked);
			}
		}
		M.ComputeClearance();
		Pathfinder P;
		std::vector<Vec2> Path;
		const Vec2 From = M.CellCenter(Cell{ 10, 10 });
		const Vec2 To = M.CellCenter(Cell{ 55, 10 });
		const bool bFound = P.FindPath(M, From, To, 1, Path);
		Check(bFound && !Path.empty(), "achou caminho");
		bool bThroughGap = false;
		for (const Vec2& W : Path)
		{
			const Cell C = M.CellOf(W);
			bThroughGap |= C.Y >= 49 && C.Y <= 54;
		}
		Check(bThroughGap, "passa pela abertura da parede");
		Check(!Path.empty() && Path.back() == To, "termina no destino exato");
		// cada trecho tem linha livre
		bool bClear = true;
		Cell Prev = M.CellOf(From);
		for (const Vec2& W : Path)
		{
			bClear &= Pathfinder::HasLineOfMovement(M, Prev, M.CellOf(W), 1);
			Prev = M.CellOf(W);
		}
		Check(bClear, "trechos do caminho sem atravessar parede");
		std::printf("  %zu pontos depois de puxar o barbante\n", Path.size());

		// destino dentro da parede: vai pra livre mais perto
		std::vector<Vec2> Path2;
		Check(P.FindPath(M, From, M.CellCenter(Cell{ 32, 20 }), 1, Path2) && !Path2.empty(), "destino bloqueado ainda da caminho");
		if (!Path2.empty())
		{
			const Cell End = M.CellOf(Path2.back());
			Check(M.IsWalkable(End.X, End.Y, 1), "termina em celula livre");
		}
	}

	// ------------------------------------------------------------------ 3. bytes quebrados

	void TestFuzz()
	{
		std::printf("[bytes quebrados]\n");
		Command C;
		C.Tick = 77;
		C.Player = 1;
		C.Type = CommandType::SetGroup;
		C.Units = { 3, 9, 12 };
		ByteWriter W;
		WriteCommand(W, C);
		ByteReader R(W.Bytes.data(), W.Bytes.size());
		Command Back;
		Check(ReadCommand(R, Back) && Back.Units == C.Units && Back.Tick == 77 && R.AtEnd(), "comando ida e volta");

		Rng Noise(12345);
		int32_t Accepted = 0;
		for (int32_t Trial = 0; Trial < 20000; ++Trial)
		{
			std::vector<uint8_t> Junk(size_t(Noise.Below(64)));
			for (uint8_t& B : Junk)
			{
				B = uint8_t(Noise.Next());
			}
			ByteReader JR(Junk.data(), Junk.size());
			Command Out;
			Accepted += ReadCommand(JR, Out) ? 1 : 0;
			MatchSetup S;
			ByteReader SR(Junk.data(), Junk.size());
			ReadSetup(SR, S);
			ReplayReader RR;
			RR.Load(Junk);
		}
		std::printf("  20000 entradas aleatorias sem travar (%d viraram comando valido)\n", Accepted);
	}

	// ------------------------------------------------------------------ 4. batalha

	UnitSpec TankSpec()
	{
		// O Crusader do Zero Hour 1.04 (Game/Config/RTS/Units.json), 1 unidade do ZH = 0,2633 m.
		UnitSpec S;
		S.Key = "usa_mbt";
		S.MaxHp = 480;
		S.RadiusMm = 3600;
		S.PathRadiusMm = 2000;
		S.SpeedMmPerS = 7900;
		S.AccelMmPerS2 = 263300;
		S.BrakeMmPerS2 = 263300;
		S.TurnMilliDegPerS = 180000;
		S.TurretTurnMilliDegPerS = 180000;
		S.TurretIdleReturnMs = 2000;
		S.SightMm = 39500;
		S.Weapon.RangeMm = 39500;
		S.Weapon.Damage = 60;
		S.Weapon.Type = DamageType::ArmorPiercing;
		S.Weapon.ReloadMs = 2000;
		S.Weapon.AimToleranceMilliDeg = 2000;
		S.Weapon.ProjectileSpeedMmPerS = 105300;
		return S;
	}

	MatchSetup MakeBattle(uint64_t Seed, int32_t TanksPerSide)
	{
		MatchSetup S;
		S.Seed = Seed;
		S.Map = MakeOpenMap(256, 256);   // 1024 m, como o mapa do WorldForge
		// alguns blocos de pedra no meio pra o caminho ter trabalho
		Rng Rocks(Seed ^ 0xABCDEF);
		for (int32_t K = 0; K < 60; ++K)
		{
			const int32_t CX = 60 + Rocks.Below(136);
			const int32_t CY = 60 + Rocks.Below(136);
			for (int32_t DY = 0; DY < 3; ++DY)
			{
				for (int32_t DX = 0; DX < 3; ++DX)
				{
					S.Map.Pass[S.Map.Index(CX + DX, CY + DY)] = uint8_t(CellPass::Blocked);
				}
			}
		}
		S.Rules.Units.push_back(UnitDef::FromSpec(TankSpec()));
		S.Rules.Armor.Set(ArmorType::Tank, DamageType::ArmorPiercing, 100);
		S.Rules.Armor.Set(ArmorType::Tank, DamageType::SmallArms, 25);
		S.Rules.Armor.Set(ArmorType::Tank, DamageType::Explosive, 100);
		for (int32_t P = 0; P < 2; ++P)
		{
			PlayerSetup PS;
			PS.Team = uint8_t(P);
			PS.bIsAI = true;
			const int32_t Side = P == 0 ? -1 : 1;
			for (int32_t T = 0; T < TanksPerSide; ++T)
			{
				StartingUnit U;
				U.Key = "usa_mbt";
				U.Pos = Vec2::Make(Fix::FromInt(Side * 300 + (T % 4) * 12 * Side), Fix::FromInt(Side * 280 + (T / 4) * 12));
				U.Facing = Angle::FromBam(P == 0 ? 8192 : 8192 + 32768);
				PS.Units.push_back(U);
			}
			S.Players.push_back(PS);
		}
		return S;
	}

	// ------------------------------------------------------------------ 4b. onde nasce

	void TestSpawn()
	{
		// Teste t4 (08/10): um tanque nasceu num platô isolado e ficou preso a partida toda.
		std::printf("[onde nasce]\n");
		MatchSetup S;
		S.Map = MakeOpenMap(64, 64);
		const auto Block = [&](int32_t X, int32_t Y) { S.Map.Pass[S.Map.Index(X, Y)] = uint8_t(CellPass::Blocked); };
		// platô de uma célula livre em (10, 10), cercado
		for (int32_t DY = -2; DY <= 2; ++DY)
		{
			for (int32_t DX = -2; DX <= 2; ++DX)
			{
				if (DX != 0 || DY != 0)
				{
					Block(10 + DX, 10 + DY);
				}
			}
		}
		// ilha cercada de 8×8 livres em (44..51, 44..51): a base inteira do jogador 1 fica lá
		for (int32_t K = 42; K <= 53; ++K)
		{
			Block(K, 42);
			Block(K, 53);
			Block(42, K);
			Block(53, K);
		}
		S.Rules.Units.push_back(UnitDef::FromSpec(TankSpec()));
		S.Map.ComputeClearance();
		const auto At = [&](int32_t X, int32_t Y)
		{
			StartingUnit U;
			U.Key = "usa_mbt";
			U.Pos = S.Map.CellCenter(Cell{ X, Y });
			return U;
		};
		PlayerSetup P0;
		P0.Team = 0;
		P0.Units = { At(20, 20), At(22, 20), At(20, 22), At(10, 10) };
		PlayerSetup P1;
		P1.Team = 1;
		P1.Units = { At(46, 46), At(48, 46), At(46, 48) };
		S.Players = { P0, P1 };

		World W;
		Check(W.Init(S), "montagem com plato e ilha");
		UnitView Stuck;
		Check(W.FindUnit(4, Stuck), "a unidade do plato existe");
		const Cell C = W.Map().CellOf(Stuck.Pos);
		Check(!(C.X == 10 && C.Y == 10), "quem nasceu no plato isolado foi pro chao do exercito");
		Pathfinder P;
		std::vector<Vec2> Path;
		bool bReached = false;
		UnitView First;
		W.FindUnit(1, First);
		P.FindPath(W.Map(), Stuck.Pos, First.Pos, 1, Path, &bReached);
		Check(bReached, "e dali chega no resto do exercito");
		bool bIslandKept = true;
		for (EntityId Id = 5; Id <= 7; ++Id)
		{
			UnitView V;
			const Cell VC = W.FindUnit(Id, V) ? W.Map().CellOf(V.Pos) : Cell{ -1, -1 };
			bIslandKept = bIslandKept && VC.X > 42 && VC.X < 53 && VC.Y > 42 && VC.Y < 53;
		}
		Check(bIslandKept, "a base inteira numa ilha fica na ilha");
	}

	// ------------------------------------------------------------------ 4c. vagas do grupo

	/** Manda o grupo inteiro do jogador 0 pra Point e devolve as vagas que cada um ganhou. */
	std::vector<Vec2> OrderSlots(World& W, int32_t Count, Vec2 Point)
	{
		Command G;
		G.Tick = W.Tick();
		G.Type = CommandType::SetGroup;
		for (int32_t K = 1; K <= Count; ++K)
		{
			G.Units.push_back(EntityId(K));
		}
		Command M;
		M.Tick = W.Tick();
		M.Type = CommandType::Move;
		M.Point = Point;
		W.Step({ G, M });
		std::vector<Vec2> Slots;
		for (int32_t K = 1; K <= Count; ++K)
		{
			UnitView V;
			W.FindUnit(EntityId(K), V);
			Slots.push_back(V.OrderPoint);
		}
		return Slots;
	}

	double MinGap(const std::vector<Vec2>& Slots)
	{
		double Best = 1e9;
		for (size_t A = 0; A < Slots.size(); ++A)
		{
			for (size_t B = A + 1; B < Slots.size(); ++B)
			{
				Best = std::min(Best, Distance(Slots[A], Slots[B]).ToDouble());
			}
		}
		return Best;
	}

	void TestGroupSlots()
	{
		// 6 tanques em 2 fileiras de 3 (8 m), andando 120 m pro leste: 3 colunas a 8,4 m,
		// a frente no ponto, ninguém em cima de ninguém
		std::printf("[vagas do grupo]\n");
		MatchSetup S;
		S.Map = MakeOpenMap(64, 64);
		S.Rules.Units.push_back(UnitDef::FromSpec(TankSpec()));
		PlayerSetup P0;
		for (int32_t K = 0; K < 6; ++K)
		{
			StartingUnit U;
			U.Key = "usa_mbt";
			U.Pos = Vec2::Make(Fix::FromInt(-60 - (K / 3) * 8), Fix::FromInt((K % 3) * 8 - 8));
			P0.Units.push_back(U);
		}
		PlayerSetup P1;
		P1.Team = 1;
		StartingUnit Far;
		Far.Key = "usa_mbt";
		Far.Pos = Vec2::Make(Fix::FromInt(100), Fix::FromInt(100));
		P1.Units.push_back(Far);
		S.Players = { P0, P1 };

		World W;
		W.Init(S);
		const Vec2 Target = Vec2::Make(Fix::FromInt(60), Fix::Zero());
		const std::vector<Vec2> Slots = OrderSlots(W, 6, Target);
		double Nearest = 1e9;
		double Farthest = 0;
		for (const Vec2& V : Slots)
		{
			Nearest = std::min(Nearest, Distance(V, Target).ToDouble());
			Farthest = std::max(Farthest, Distance(V, Target).ToDouble());
		}
		std::printf("  colunas: menor distancia entre vagas %.1f m, frente a %.1f m do ponto, a ultima a %.1f m\n", MinGap(Slots), Nearest, Farthest);
		Check(MinGap(Slots) > 7.2, "colunas: ninguem encostado (mais que o diametro do tanque)");
		Check(Nearest < 9.0, "colunas: a frente chega no ponto");
		Check(Farthest < 20.0, "colunas: o grupo nao se espalha");
		// quem estava em cima (y maior) continua em cima: ninguém cruza o vizinho
		UnitView Top;
		UnitView Bottom;
		W.FindUnit(3, Top);
		W.FindUnit(1, Bottom);
		Check(Top.OrderPoint.Y > Bottom.OrderPoint.Y, "colunas: cada um fica do seu lado");

		// clique dentro do grupo: juntar em volta do ponto
		UnitView C;
		W.FindUnit(2, C);
		const std::vector<Vec2> Gather = OrderSlots(W, 6, C.Pos);
		double GatherFar = 0;
		for (const Vec2& V : Gather)
		{
			GatherFar = std::max(GatherFar, Distance(V, C.Pos).ToDouble());
		}
		std::printf("  juntar: menor distancia %.1f m, a mais longe a %.1f m do ponto\n", MinGap(Gather), GatherFar);
		Check(MinGap(Gather) > 7.2, "juntar: ninguem encostado");
		Check(GatherFar < 18.0, "juntar: todos em volta do ponto");
	}

	struct BattleResult
	{
		std::vector<uint64_t> Hashes;
		TickNumber EndTick = 0;
		int32_t Winner = -1;
		int32_t Shots = 0;
		int32_t Deaths = 0;
		std::vector<uint8_t> Replay;
		double MsPerTick = 0;
	};

	BattleResult RunBattle(const MatchSetup& Setup, TickNumber MaxTicks)
	{
		BattleResult R;
		World W;
		if (!W.Init(Setup))
		{
			std::printf("  montagem invalida\n");
			++Failures;
			return R;
		}
		CommandScheduler Queue(1);
		std::vector<SkirmishAI> Brains;
		for (size_t P = 0; P < Setup.Players.size(); ++P)
		{
			Brains.emplace_back(PlayerIndex(P));
			Brains.back().FirstWaveDelayTicks = 2 * TicksPerSecond;
		}
		ReplayWriter Rec;
		Rec.Begin(Setup);
		const auto Start = std::chrono::steady_clock::now();
		while (W.Tick() < MaxTicks && !W.IsMatchOver())
		{
			for (SkirmishAI& B : Brains)
			{
				std::vector<Command> Orders;
				B.Think(W, Orders);
				for (const Command& C : Orders)
				{
					Queue.Issue(C, W.Tick());
				}
			}
			const TickNumber T = W.Tick();
			const std::vector<Command> Due = Queue.TakeFor(T);
			W.Step(Due);
			Rec.AddTick(T, Due, W.StateHash());
			R.Hashes.push_back(W.StateHash());
			for (const SimEvent& E : W.LastEvents())
			{
				R.Shots += E.Type == EventType::WeaponFired ? 1 : 0;
				R.Deaths += E.Type == EventType::UnitDied ? 1 : 0;
			}
		}
		const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
		R.MsPerTick = R.Hashes.empty() ? 0 : Ms / double(R.Hashes.size());
		R.EndTick = W.Tick();
		R.Winner = W.WinningTeam();
		R.Replay = Rec.Finish();
		return R;
	}

	uint64_t TestBattle()
	{
		std::printf("[batalha IA x IA]\n");
		const MatchSetup Setup = MakeBattle(42, 12);
		const TickNumber MaxTicks = 6 * 60 * TicksPerSecond;
		const BattleResult A = RunBattle(Setup, MaxTicks);
		const BattleResult B = RunBattle(Setup, MaxTicks);
		std::printf("  terminou no passo %u (%.1f s de jogo), vencedor time %d, %d tiros, %d mortes, %.3f ms por passo\n",
			A.EndTick, double(A.EndTick) / TicksPerSecond, A.Winner, A.Shots, A.Deaths, A.MsPerTick);
		Check(A.Shots > 0 && A.Deaths > 0, "houve combate");
		Check(A.Winner == 0 || A.Winner == 1, "alguem venceu antes do tempo acabar");
		Check(A.Hashes == B.Hashes, "duas rodadas iguais passo a passo");

		// replay: carrega e roda só com os comandos gravados, conferindo cada hash
		ReplayReader Reader;
		const bool bLoaded = Reader.Load(A.Replay);
		Check(bLoaded, "replay carrega");
		if (bLoaded)
		{
			World W;
			W.Init(Reader.Setup());
			int32_t Mismatch = -1;
			int32_t Checked = 0;
			for (TickNumber T = 0; T < Reader.LastTick() + 1 && !W.IsMatchOver(); ++T)
			{
				W.Step(Reader.CommandsFor(T));
				uint64_t Want = 0;
				if (Reader.ExpectedHash(T, Want))
				{
					++Checked;
					if (Want != W.StateHash() && Mismatch < 0)
					{
						Mismatch = int32_t(T);
					}
				}
			}
			std::printf("  replay: %zu bytes, %d impressoes conferidas\n", A.Replay.size(), Checked);
			Check(Mismatch < 0, "replay reproduz todas as impressoes");
			Check(W.StateHash() == A.Hashes.back(), "replay termina no mesmo estado");
		}

		// detector: um comando a mais tem de mudar a impressão
		const MatchSetup Setup2 = MakeBattle(43, 12);
		const BattleResult C = RunBattle(Setup2, 600);
		Check(C.Hashes.size() > 10 && A.Hashes[10] != C.Hashes[10], "partida diferente da impressao diferente");
		return A.Hashes.empty() ? 0 : A.Hashes.back();
	}

	void TestScale()
	{
		std::printf("[escala]\n");
		const MatchSetup Setup = MakeBattle(7, 400);
		const BattleResult R = RunBattle(Setup, 30 * TicksPerSecond);
		std::printf("  800 tanques, 30 s: %.3f ms por passo\n", R.MsPerTick);
	}
}

int main(int Argc, char** Argv)
{
	const bool bScale = Argc > 1 && std::strcmp(Argv[1], "--scale") == 0;
	TestMath();
	TestPathfinding();
	TestFuzz();
	TestSpawn();
	TestGroupSlots();
	const uint64_t Final = TestBattle();
	if (bScale)
	{
		TestScale();
	}
	std::printf("FINAL_HASH %016llx\n", (unsigned long long)Final);
	std::printf(Failures == 0 ? "TUDO CERTO\n" : "%d FALHA(S)\n", Failures);
	return Failures == 0 ? 0 : 1;
}
