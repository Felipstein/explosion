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

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <map>
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
		// (16/10) esmaga o carro e derruba a árvore (o CrusherLevel 2 do Crusader); a caixa do casco 7,9 × 3,7 m
		S.CrusherLevel = 2;
		S.HullHalfLengthMm = 3950;
		S.HullHalfWidthMm = 1850;
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

		// sem cruzar (11/10, o "NoX" do BAR): 6 tanques numa diagonal, mandados pra longe na
		// outra diagonal; nenhum caminho (posição → vaga, em linha reta) cruza o de outro
		const auto Cross = [](Vec2 A, Vec2 B, Vec2 C, Vec2 D)
		{
			const auto Side = [](Vec2 P, Vec2 Q, Vec2 R)
			{
				const double V = (Q.X.ToDouble() - P.X.ToDouble()) * (R.Y.ToDouble() - P.Y.ToDouble())
					- (Q.Y.ToDouble() - P.Y.ToDouble()) * (R.X.ToDouble() - P.X.ToDouble());
				return V > 1e-6 ? 1 : (V < -1e-6 ? -1 : 0);
			};
			return Side(A, B, C) * Side(A, B, D) < 0 && Side(C, D, A) * Side(C, D, B) < 0;
		};
		std::vector<Vec2> Before;
		for (int32_t K = 1; K <= 6; ++K)
		{
			UnitView V;
			W.FindUnit(EntityId(K), V);
			Before.push_back(V.Pos);
		}
		const std::vector<Vec2> FarSlots = OrderSlots(W, 6, Vec2::Make(Fix::FromInt(-40), Fix::FromInt(70)));
		int32_t Crossings = 0;
		for (size_t A = 0; A < FarSlots.size(); ++A)
		{
			for (size_t B = A + 1; B < FarSlots.size(); ++B)
			{
				Crossings += Cross(Before[A], FarSlots[A], Before[B], FarSlots[B]) ? 1 : 0;
			}
		}
		std::printf("  sem cruzar: %d cruzamento(s) nos caminhos ate as vagas\n", Crossings);
		Check(Crossings == 0, "nenhum caminho ate a vaga cruza o de outro");
	}

	// ------------------------------------------------------------------ 4d. visão e névoa

	MatchSetup TwoTankSetup(Cell Mine, Cell Theirs, bool bFog)
	{
		MatchSetup S;
		S.Map = MakeOpenMap(64, 64);
		S.Rules.Units.push_back(UnitDef::FromSpec(TankSpec()));
		S.Rules.Armor.Set(ArmorType::Tank, DamageType::ArmorPiercing, 100);
		S.bFogOfWar = bFog;
		S.bAllowDebugCommands = true;
		const auto At = [&](Cell C)
		{
			StartingUnit U;
			U.Key = "usa_mbt";
			U.Pos = S.Map.CellCenter(C);
			return U;
		};
		PlayerSetup P0;
		P0.Units = { At(Mine) };
		PlayerSetup P1;
		P1.Team = 1;
		P1.bIsAI = true;
		P1.Units = { At(Theirs) };
		S.Players = { P0, P1 };
		return S;
	}

	void StepWith(World& W, std::vector<Command> Commands)
	{
		for (Command& C : Commands)
		{
			C.Tick = W.Tick();
		}
		W.Step(Commands);
	}

	/** Andar em formação (F, 11/10): 3 tanques e um rápido atrás. Com F, o rápido segue no passo dos
	 *  tanques, todos chegam juntos e o desenho do grupo fica; sem F, ele passa na frente. */
	struct FormationRun
	{
		double ArriveSpread = 0;   // s entre o primeiro e o último a parar
		double FastLead = 0;       // s que o rápido chega antes do último tanque
		double ShapeError = 0;     // m: o maior desvio do lugar que cada um tinha no grupo
		bool bAllArrived = false;
	};

	FormationRun RunFormation(bool bFormation)
	{
		MatchSetup S;
		S.Map = MakeOpenMap(64, 64);
		S.bFogOfWar = false;
		S.Rules.Units.push_back(UnitDef::FromSpec(TankSpec()));
		UnitSpec Fast = TankSpec();
		Fast.Key = "fast_scout";
		Fast.SpeedMmPerS = 15000;
		S.Rules.Units.push_back(UnitDef::FromSpec(Fast));
		PlayerSetup P0;
		const int32_t Xs[4] = { -60, -60, -68, -68 };
		const int32_t Ys[4] = { -4, 4, -4, 4 };
		for (int32_t K = 0; K < 4; ++K)
		{
			StartingUnit U;
			U.Key = K == 3 ? "fast_scout" : "usa_mbt";
			U.Pos = Vec2::Make(Fix::FromInt(Xs[K]), Fix::FromInt(Ys[K]));
			P0.Units.push_back(U);
		}
		PlayerSetup P1;
		P1.Team = 1;
		StartingUnit Far;
		Far.Key = "usa_mbt";
		Far.Pos = Vec2::Make(Fix::FromInt(110), Fix::FromInt(110));
		P1.Units.push_back(Far);
		S.Players = { P0, P1 };
		World W;
		W.Init(S);
		Vec2 Before[4];
		Vec2 Center0 = Vec2::Zero();
		for (int32_t K = 0; K < 4; ++K)
		{
			UnitView V;
			W.FindUnit(EntityId(K + 1), V);
			Before[K] = V.Pos;
			Center0 += V.Pos / Fix::FromInt(4);
		}
		Command G;
		G.Tick = W.Tick();
		G.Type = CommandType::SetGroup;
		G.Units = { 1, 2, 3, 4 };
		Command M;
		M.Tick = W.Tick();
		M.Type = CommandType::Move;
		M.Point = Vec2::Make(Fix::FromInt(60), Fix::Zero());
		M.Value = bFormation ? 1 : 0;
		W.Step({ G, M });
		int32_t Arrived[4] = { -1, -1, -1, -1 };
		for (int32_t T = 1; T < 40 * TicksPerSecond; ++T)
		{
			W.Step({});
			bool bAll = true;
			for (int32_t K = 0; K < 4; ++K)
			{
				UnitView V;
				W.FindUnit(EntityId(K + 1), V);
				if (Arrived[K] < 0 && V.Order == OrderType::Idle && V.Speed == Fix::Zero() && V.PathLeft == 0)
				{
					Arrived[K] = T;
				}
				bAll = bAll && Arrived[K] >= 0;
			}
			if (bAll)
			{
				break;
			}
		}
		FormationRun R;
		R.bAllArrived = Arrived[0] >= 0 && Arrived[1] >= 0 && Arrived[2] >= 0 && Arrived[3] >= 0;
		int32_t First = 1 << 30, Last = 0, LastTank = 0;
		for (int32_t K = 0; K < 4; ++K)
		{
			First = std::min(First, Arrived[K]);
			Last = std::max(Last, Arrived[K]);
			if (K < 3)
			{
				LastTank = std::max(LastTank, Arrived[K]);
			}
		}
		R.ArriveSpread = double(Last - First) / TicksPerSecond;
		R.FastLead = double(LastTank - Arrived[3]) / TicksPerSecond;
		Vec2 Center1 = Vec2::Zero();
		Vec2 After[4];
		for (int32_t K = 0; K < 4; ++K)
		{
			UnitView V;
			W.FindUnit(EntityId(K + 1), V);
			After[K] = V.Pos;
			Center1 += V.Pos / Fix::FromInt(4);
		}
		for (int32_t K = 0; K < 4; ++K)
		{
			R.ShapeError = std::max(R.ShapeError, Distance(After[K] - Center1, Before[K] - Center0).ToDouble());
		}
		return R;
	}

	void TestFormationMove()
	{
		std::printf("[formacao com F]\n");
		const FormationRun Free = RunFormation(false);
		const FormationRun Form = RunFormation(true);
		std::printf("  sem F: chegada espalhada em %.1f s, o rapido chega %.1f s antes, desenho %.1f m fora\n", Free.ArriveSpread, Free.FastLead, Free.ShapeError);
		std::printf("  com F: chegada espalhada em %.1f s, o rapido chega %.1f s antes, desenho %.1f m fora\n", Form.ArriveSpread, Form.FastLead, Form.ShapeError);
		Check(Free.bAllArrived && Form.bAllArrived, "formacao: todos chegam");
		Check(Free.FastLead > 3.0, "formacao: sem F o rapido passa na frente");
		Check(Form.FastLead < 1.0, "formacao: com F o rapido segue no passo dos tanques");
		Check(Form.ArriveSpread < 1.5, "formacao: com F chegam juntos");
		Check(Form.ShapeError < 2.0, "formacao: com F o desenho do grupo fica");
	}

	template <typename Fn>
	int32_t RunUntil(World& W, int32_t MaxTicks, Fn&& Done);

	/** O arraste com direção (11/10): 6 tanques em bloco a oeste vão pra (40, 0).
	 *  - pra frente (leste, contra o inimigo), 40 m: uma linha de frente só, de 42 m, de través da marcha;
	 *  - pra frente, arraste curto: a do grupo, 3 por fileira, 2 fileiras;
	 *  - de lado (norte), 40 m: a linha fica ao longo da marcha e os tanques entram no mesmo corredor; a regra
	 *    do ZH "empacou perto do destino = chegou" para alguns antes da vaga (limite conhecido: o certo é marchar
	 *    em formação e só abrir perto). Aqui só a direção e a largura mínima são cobradas. */
	void TestLineDrag()
	{
		std::printf("[arraste com direcao]\n");
		struct FCase
		{
			const char* Name;
			int32_t FacingBam;
			int32_t WidthHalfM;
		};
		const FCase Cases[3] = { { "pra frente, 40 m", 0, 20 }, { "pra frente, curto", 0, 0 }, { "de lado, 40 m", Angle::QuarterTurn, 20 } };
		for (const FCase& Case : Cases)
		{
			MatchSetup S;
			S.Map = MakeOpenMap(64, 64);
			S.bFogOfWar = false;
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
			Far.Pos = Vec2::Make(Fix::FromInt(110), Fix::FromInt(110));
			P1.Units.push_back(Far);
			S.Players = { P0, P1 };
			World W;
			W.Init(S);
			Command G;
			G.Tick = W.Tick();
			G.Type = CommandType::SetGroup;
			G.Units = { 1, 2, 3, 4, 5, 6 };
			Command M;
			M.Tick = W.Tick();
			M.Type = CommandType::Move;
			M.Point = Vec2::Make(Fix::FromInt(40), Fix::Zero());
			M.Value = MoveValue(false, true, Case.WidthHalfM, uint16_t(Case.FacingBam));
			W.Step({ G, M });
			RunUntil(W, 60 * TicksPerSecond, [&]()
			{
				for (int32_t K = 1; K <= 6; ++K)
				{
					UnitView V;
					W.FindUnit(EntityId(K), V);
					if (V.Order != OrderType::Idle || V.Speed != Fix::Zero())
					{
						return false;
					}
				}
				return true;
			});
			RunUntil(W, 3 * TicksPerSecond, []() { return false; });   // o giro final no lugar
			// largura = de través da direção pedida; fundo = ao longo dela
			const double FacingDeg = double(Case.FacingBam) * 360.0 / double(Angle::FullTurn);
			const double DX = std::cos(FacingDeg * 3.14159265358979 / 180.0), DY = std::sin(FacingDeg * 3.14159265358979 / 180.0);
			std::vector<Vec2> Ends;
			double A0 = 1e9, A1 = -1e9, L0 = 1e9, L1 = -1e9;
			int32_t Facing = 0;
			for (int32_t K = 1; K <= 6; ++K)
			{
				UnitView V;
				W.FindUnit(EntityId(K), V);
				Ends.push_back(V.Pos);
				const double X = V.Pos.X.ToDouble(), Y = V.Pos.Y.ToDouble();
				const double Along = X * DX + Y * DY, Across = -X * DY + Y * DX;
				A0 = std::min(A0, Along); A1 = std::max(A1, Along);
				L0 = std::min(L0, Across); L1 = std::max(L1, Across);
				double Off = std::fmod(std::abs(V.Hull.ToDegrees() - FacingDeg), 360.0);
				Off = std::min(Off, 360.0 - Off);
				Facing += Off < 10.0 ? 1 : 0;
			}
			const double Width = L1 - L0, Depth = A1 - A0;
			std::printf("  %s: linha de %.0f m de largura por %.0f m de fundo, %d de 6 virados, par mais perto a %.1f m\n",
				Case.Name, Width, Depth, Facing, MinGap(Ends));
			Check(Facing == 6, "arraste: todos chegam virados pra direcao puxada");
			Check(MinGap(Ends) > 7.0, "arraste: ninguem encostado");
			if (Case.FacingBam == 0 && Case.WidthHalfM > 0)
			{
				// (revisão de 23/10) 6 no passo mínimo (8,42 m) pedem 42,1 m: na linha de 40 m cabem 5 de ponta a ponta e o 6º
				// vai atrás, no meio (antes a fileira de 6 passava 1 m de cada ponta da linha puxada)
				Check(Width > 38.0 && Width < 41.0 && Depth > 6.0 && Depth < 11.0, "arraste longo pra frente: a fileira da frente da largura puxada, o que nao cabe atras");
			}
			else if (Case.FacingBam == 0)
			{
				Check(Width > 14.0 && Width < 22.0 && Depth > 6.0 && Depth < 13.0, "arraste curto: 3 por fileira, 2 fileiras");
			}
			else
			{
				Check(Width > 24.0, "arraste de lado: a linha abre (mesmo com o corredor)");
			}
		}
	}

	/** (v4, 23/10) A prévia da linha dos dois botões (World::PreviewLineSlots): as vagas que o HUD desenha são, bit a bit, as
	 *  que o Move com direção dá no mesmo passo, com o mesmo filtro do grupo, e pedir a prévia não muda nada (um gêmeo sem a
	 *  prévia anda junto e o hash bate). Os 3 casos do arraste e mais 4: a linha de 126 m (o teto do MoveValue), 1 tanque, o
	 *  grupo com um prédio, um ID que não existe (o caminho do morto), um repetido e o tanque inimigo, e a linha que corta um
	 *  bloco de pedra (o IntoRegion leva a vaga pro chão). E o conjunto das vagas não depende de onde os tanques estão (só
	 *  quem fica com qual): a ordem sai uns passos depois da prévia, na rede. */
	void TestLinePreview()
	{
		std::printf("[previa da linha]\n");
		struct FCase
		{
			const char* Name;
			int32_t FacingBam;
			int32_t WidthHalfM;
			int32_t Tanks;
			bool bMixed;
			bool bRocks;
		};
		const FCase Cases[] = {
			{ "pra frente, 40 m", 0, 20, 6, false, false },
			{ "pra frente, curto", 0, 0, 6, false, false },
			{ "de lado, 40 m", Angle::QuarterTurn, 20, 6, false, false },
			{ "126 m, o teto", 0, 63, 6, false, false },
			{ "1 tanque", Angle::QuarterTurn / 2, 20, 1, false, false },
			{ "grupo misturado", 0, 20, 6, true, false },
			{ "linha na pedra", 0, 30, 6, false, true },
		};
		const auto SortedSlots = [](std::vector<Vec2> V)
		{
			std::sort(V.begin(), V.end(), [](const Vec2& A, const Vec2& B) { return A.X.Raw != B.X.Raw ? A.X.Raw < B.X.Raw : A.Y.Raw < B.Y.Raw; });
			return V;
		};
		for (const FCase& Case : Cases)
		{
			std::vector<UnitSpec> Specs = { TankSpec() };
			UnitSpec Bunker;
			Bunker.Key = "test_bunker";
			Bunker.Kind = ObjectKind::Structure;
			Bunker.Armor = ArmorType::Structure;
			Bunker.MaxHp = 2000;
			Bunker.SizeXMm = 8000;
			Bunker.SizeYMm = 8000;
			Specs.push_back(Bunker);
			MatchSetup S;
			S.Map = MakeOpenMap(64, 64);
			S.bFogOfWar = false;
			Check(Ruleset::Build(Specs, ArmorTable(), EconomyRules(), S.Rules), "previa: as regras montam");
			if (Case.bRocks)
			{
				// a pedra nas células de X = 40..48 m e Y = 8..24 m: as duas vagas do norte da linha caem nela
				for (int32_t Y = 34; Y <= 37; ++Y)
				{
					for (int32_t X = 42; X <= 43; ++X)
					{
						S.Map.Pass[S.Map.Index(X, Y)] = uint8_t(CellPass::Blocked);
					}
				}
			}
			PlayerSetup P0;
			for (int32_t K = 0; K < Case.Tanks; ++K)
			{
				P0.Units.push_back({ "usa_mbt", Vec2::Make(Fix::FromInt(-60 - (K / 3) * 8), Fix::FromInt((K % 3) * 8 - 8)), Angle() });
			}
			if (Case.bMixed)
			{
				P0.Units.push_back({ "test_bunker", Vec2::Make(Fix::FromInt(-100), Fix::FromInt(60)), Angle() });
			}
			PlayerSetup P1;
			P1.Team = 1;
			P1.Units.push_back({ "usa_mbt", Vec2::Make(Fix::FromInt(110), Fix::FromInt(110)), Angle() });
			S.Players = { P0, P1 };
			World W;
			World Twin;
			Check(W.Init(S) && Twin.Init(S), "previa: a montagem");
			std::vector<EntityId> Own;
			EntityId Building = NoEntity;
			EntityId Enemy = NoEntity;
			for (int32_t I = 0; I < W.UnitCount(); ++I)
			{
				const UnitView V = W.Unit(I);
				if (V.Owner == 0 && !V.bStructure)
				{
					Own.push_back(V.Id);
				}
				Building = V.Owner == 0 && V.bStructure ? V.Id : Building;
				Enemy = V.Owner == 1 ? V.Id : Enemy;
			}
			Command G;
			G.Tick = W.Tick();
			G.Type = CommandType::SetGroup;
			G.Units = Own;
			if (Case.bMixed)
			{
				// fora de ordem, com o prédio, um ID que não existe, um repetido e o inimigo: o filtro tem de ser o do comando
				std::reverse(G.Units.begin(), G.Units.end());
				G.Units.insert(G.Units.begin() + 2, Building);
				G.Units.push_back(EntityId(999));
				G.Units.push_back(Own.front());
				G.Units.push_back(Enemy);
			}
			Command M;
			M.Tick = W.Tick();
			M.Type = CommandType::Move;
			M.Point = Vec2::Make(Fix::FromInt(40), Fix::Zero());
			M.Value = MoveValue(false, true, Case.WidthHalfM, uint16_t(Case.FacingBam));
			std::vector<EntityId> PIds;
			std::vector<Vec2> PSlots;
			const bool bPreview = W.PreviewLineSlots(0, G.Units, M.Point, M.Value, PIds, PSlots);
			Check(bPreview && PIds == Own && PSlots.size() == Own.size(), "previa: uma vaga por unidade nossa que anda, na ordem de ID (o filtro do comando)");
			W.Step({ G, M });
			Twin.Step({ G, M });
			Check(W.StateHash() == Twin.StateHash(), "previa: pedir a previa nao muda nada (o hash do gemeo bate)");
			int32_t Same = 0;
			bool bWalkable = true;
			for (size_t K = 0; K < PIds.size() && K < PSlots.size(); ++K)
			{
				UnitView V;
				Same += W.FindUnit(PIds[K], V) && V.Order == OrderType::Move && V.OrderPoint == PSlots[K] ? 1 : 0;
				const Cell C = W.Map().CellOf(PSlots[K]);
				bWalkable = bWalkable && W.Map().IsWalkable(C.X, C.Y, 1);
			}
			Check(Same == int32_t(Own.size()), "previa: cada vaga desenhada e a vaga da ordem, bit a bit");
			if (Case.bRocks)
			{
				Check(bWalkable, "previa: na pedra, a vaga vai pro chao andavel (a mesma do comando)");
			}
			// uns passos depois (a ordem na rede sai depois da prévia), com os tanques já andando: o mesmo conjunto de vagas
			RunUntil(W, 2 * TicksPerSecond, []() { return false; });
			RunUntil(Twin, 2 * TicksPerSecond, []() { return false; });
			Check(W.StateHash() == Twin.StateHash(), "previa: o gemeo segue igual depois de 2 s");
			std::vector<EntityId> LaterIds;
			std::vector<Vec2> Later;
			W.PreviewLineSlots(0, G.Units, M.Point, M.Value, LaterIds, Later);
			Check(SortedSlots(Later) == SortedSlots(PSlots), "previa: o conjunto de vagas nao depende de onde os tanques estao");
			std::printf("  %s: %zu vagas, %d iguais as da ordem, par mais perto a %.1f m\n", Case.Name, PSlots.size(), Same, PSlots.size() > 1 ? MinGap(PSlots) : 0.0);
		}
		// as portas do comando: sem a direção no valor não há linha
		{
			MatchSetup S;
			S.Map = MakeOpenMap(64, 64);
			S.Rules.Units.push_back(UnitDef::FromSpec(TankSpec()));
			PlayerSetup P0;
			P0.Units.push_back({ "usa_mbt", Vec2::Zero(), Angle() });
			PlayerSetup P1;
			P1.Team = 1;
			P1.Units.push_back({ "usa_mbt", Vec2::Make(Fix::FromInt(110), Fix::FromInt(110)), Angle() });
			S.Players = { P0, P1 };
			World W;
			W.Init(S);
			std::vector<EntityId> Ids;
			std::vector<Vec2> Slots;
			Check(!W.PreviewLineSlots(0, { 1 }, Vec2::Zero(), MoveValue(false, false, 20, 0), Ids, Slots) && Ids.empty() && Slots.empty(), "previa: sem a direcao, nenhuma vaga");
			Check(!W.PreviewLineSlots(0, { 2 }, Vec2::Zero(), MoveValue(false, true, 20, 0), Ids, Slots), "previa: so o inimigo no grupo, nenhuma vaga");
			Check(!W.PreviewLineSlots(5, { 1 }, Vec2::Zero(), MoveValue(false, true, 20, 0), Ids, Slots), "previa: jogador que nao existe, nenhuma vaga");
		}
	}

	void TestVision()
	{
		std::printf("[visao e nevoa]\n");
		// o tanque revela (39,5 + 3,6) m + 1 célula = 11,775 células de 4 m: o disco pega dx² + dy² <= 138
		World W;
		Check(W.Init(TwoTankSetup(Cell{ 10, 10 }, Cell{ 50, 50 }, true)), "montagem com nevoa");
		Check(W.IsCellVisible(0, 10, 10) && W.IsCellVisible(0, 21, 10) && W.IsCellVisible(0, 21, 14), "ve o proprio disco (11,4 = 137 entra)");
		Check(!W.IsCellVisible(0, 22, 10) && !W.IsCellVisible(0, 19, 19) && W.IsCellVisible(0, 18, 18), "fora do raio nao ve (12,0 = 144 e 9,9 = 162 passam de 138; 8,8 = 128 entra)");
		Check(W.VisionSelfCheck(), "a visao confere com uma refeita do zero");
		Check(!W.IsCellVisible(0, 50, 50) && W.IsCellVisible(1, 50, 50), "cada time ve so o seu");
		Check(W.IsCellExplored(0, 10, 10) && !W.IsCellExplored(0, 40, 40), "explorado = ja visto");
		UnitView Enemy;
		W.FindUnit(2, Enemy);
		Check(!W.IsVisibleTo(0, Enemy.Pos), "o inimigo longe esta escondido");

		// andar: o lugar de antes fica explorado, mas não mais à vista
		Command G;
		G.Type = CommandType::SetGroup;
		G.Units = { 1 };
		Command M;
		M.Type = CommandType::Move;
		M.Point = W.Map().CellCenter(Cell{ 40, 10 });
		StepWith(W, { G, M });
		for (int32_t T = 0; T < 30 * TicksPerSecond; ++T)
		{
			StepWith(W, {});
		}
		UnitView Me;
		W.FindUnit(1, Me);
		const Cell Now = W.Map().CellOf(Me.Pos);
		std::printf("  o tanque foi pra celula (%d, %d)\n", Now.X, Now.Y);
		Check(Now.X >= 38, "o tanque andou");
		Check(!W.IsCellVisible(0, 10, 10) && W.IsCellExplored(0, 10, 10), "o lugar de onde saiu ficou explorado e escuro");
		Check(W.IsCellVisible(0, Now.X, Now.Y), "ve onde esta agora");
		Check(W.VisionSelfCheck(), "a visao confere depois de andar");

		// ordem de atacar o que sumiu na névoa: vira ataque andando até onde ele está
		Command A;
		A.Type = CommandType::Attack;
		A.Target = 2;
		StepWith(W, { A });
		W.FindUnit(1, Me);
		W.FindUnit(2, Enemy);
		Check(Me.Order == OrderType::AttackMove && Me.OrderPoint == Enemy.Pos, "atacar o que nao se ve vira ataque andando ate la");

		// desligar a névoa pelo comando: o inimigo aparece pra todos, e o hash muda
		const uint64_t Before = W.StateHash();
		Command Off;
		Off.Type = CommandType::SetFog;
		Off.Target = 0;
		StepWith(W, { Off });
		W.FindUnit(2, Enemy);
		Check(!W.IsFogOfWar() && W.IsVisibleTo(0, Enemy.Pos), "SetFog 0 desliga a nevoa");
		Check(W.StateHash() != Before, "o liga/desliga entra na impressao digital");
		Command A2 = A;
		StepWith(W, { A2 });
		W.FindUnit(1, Me);
		Check(Me.Order == OrderType::Attack, "sem nevoa a ordem de atacar vale");

		// o comando viaja em bytes
		ByteWriter BW;
		Off.Tick = 9;
		WriteCommand(BW, Off);
		ByteReader BR(BW.Bytes.data(), BW.Bytes.size());
		Command Back;
		Check(ReadCommand(BR, Back) && Back.Type == CommandType::SetFog && Back.Target == 0 && Back.Tick == 9, "SetFog ida e volta em bytes");
		BW.Bytes.back() = 2;
		ByteReader Bad(BW.Bytes.data(), BW.Bytes.size());
		Check(!ReadCommand(Bad, Back), "SetFog com valor 2 e recusado");

		// sem a permissão de depuração na montagem, o SetFog não faz nada
		World W5;
		MatchSetup S5 = TwoTankSetup(Cell{ 10, 10 }, Cell{ 50, 50 }, true);
		S5.bAllowDebugCommands = false;
		W5.Init(S5);
		Command Off5;
		Off5.Type = CommandType::SetFog;
		Off5.Target = 0;
		StepWith(W5, { Off5 });
		Check(W5.IsFogOfWar(), "online (sem depuracao) o SetFog e ignorado");

		// time fora das vagas: a montagem é recusada (a grade de visão é por time)
		World W6;
		MatchSetup S6 = TwoTankSetup(Cell{ 10, 10 }, Cell{ 50, 50 }, true);
		S6.Players[1].Team = 200;
		Check(!W6.Init(S6), "time 200 e recusado");

		// a visão de quem morre fica 1 s e depois sai
		World W4;
		MatchSetup S4 = TwoTankSetup(Cell{ 30, 30 }, Cell{ 38, 30 }, true);
		S4.Players[0].Units.push_back(S4.Players[0].Units[0]);
		S4.Players[0].Units.back().Pos = S4.Map.CellCenter(Cell{ 30, 32 });
		S4.Players[0].Units.push_back(S4.Players[0].Units[0]);
		S4.Players[0].Units.back().Pos = S4.Map.CellCenter(Cell{ 30, 28 });
		W4.Init(S4);
		TickNumber Died = 0;
		Vec2 DeathAt;
		for (int32_t T = 0; T < 120 * TicksPerSecond && Died == 0; ++T)
		{
			StepWith(W4, {});
			for (const SimEvent& E : W4.LastEvents())
			{
				if (E.Type == EventType::UnitDied && E.Value == 1)
				{
					Died = W4.Tick();
					DeathAt = E.Point;
				}
			}
		}
		Check(Died > 0, "o tanque sozinho morreu");
		Check(W4.VisionSelfCheck(), "a visao confere depois da batalha");
		const Cell DC = W4.Map().CellOf(DeathAt);
		const Cell Far{ std::min(DC.X + 9, 63), DC.Y };
		const bool bRightAfter = W4.IsCellVisible(1, Far.X, Far.Y);
		for (int32_t T = 0; T < TicksPerSecond + 2; ++T)
		{
			StepWith(W4, {});
		}
		const bool bLater = W4.IsCellVisible(1, Far.X, Far.Y);
		std::printf("  morte no passo %u: o time dele ainda ve logo depois %d, 1 s depois %d\n", Died, int(bRightAfter), int(bLater));
		Check(bRightAfter && !bLater, "a visao do morto fica 1 s e sai");
	}

	void TestFogBattle()
	{
		// a batalha 12 × 12 com névoa: ninguém atira no que o time não vê, e a visão incremental
		// bate com uma refeita do zero a cada segundo
		std::printf("[batalha com nevoa]\n");
		MatchSetup S = MakeBattle(42, 12);
		Check(S.bFogOfWar, "a batalha de teste roda com nevoa");
		World W;
		W.Init(S);
		std::vector<SkirmishAI> Brains = { SkirmishAI(0), SkirmishAI(1) };
		CommandScheduler Queue(1);
		int32_t Shots = 0;
		int32_t Blind = 0;
		int32_t Checks = 0;
		int32_t BadChecks = 0;
		for (int32_t T = 0; T < 120 * TicksPerSecond && !W.IsMatchOver(); ++T)
		{
			for (SkirmishAI& B : Brains)
			{
				std::vector<Command> Out;
				B.Think(W, Out);
				for (const Command& C : Out)
				{
					Queue.Issue(C, W.Tick());
				}
			}
			W.Step(Queue.TakeFor(W.Tick()));
			for (const SimEvent& E : W.LastEvents())
			{
				if (E.Type != EventType::WeaponFired)
				{
					continue;
				}
				++Shots;
				UnitView Shooter;
				UnitView Target;
				if (W.FindUnit(E.Source, Shooter) && W.FindUnit(E.Target, Target) && !W.IsVisibleTo(Shooter.Owner, Target.Pos))
				{
					++Blind;
				}
			}
			if (W.Tick() % TicksPerSecond == 0)
			{
				++Checks;
				BadChecks += W.VisionSelfCheck() ? 0 : 1;
			}
		}
		std::printf("  %d tiros, %d em alvo invisivel; %d conferencias da visao, %d erradas\n", Shots, Blind, Checks, BadChecks);
		Check(Shots > 0 && Blind == 0, "nenhum tiro em alvo que o time nao ve");
		Check(Checks > 0 && BadChecks == 0, "a visao incremental bate com a refeita");
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

	// ------------------------------------------------------------------ 4e. economia e construção (ZH)

	/** As regras da base com os números do ZH 1.04 (docs/ECONOMIA.md; 1 u do ZH = 0,2633 m). */
	Ruleset EconomyRuleset(bool bPatriot = false)
	{
		std::vector<UnitSpec> Specs;
		UnitSpec Tank = TankSpec();
		Tank.Cost = 900;
		Tank.BuildTimeMs = 10000;
		Tank.Requires = { "usa_war_factory" };
		Specs.push_back(Tank);

		UnitSpec Dozer;
		Dozer.Key = "usa_dozer";
		Dozer.MaxHp = 250;
		Dozer.RadiusMm = 3600;
		Dozer.PathRadiusMm = 2000;
		Dozer.SpeedMmPerS = 7900;
		Dozer.AccelMmPerS2 = 7900;
		Dozer.BrakeMmPerS2 = 7900;
		Dozer.TurnMilliDegPerS = 90000;
		Dozer.SightMm = 52700;
		Dozer.Cost = 1000;
		Dozer.BuildTimeMs = 5000;
		Dozer.CrusherLevel = 2;   // (16/10) o trator de esteira não contorna palmeira
		Dozer.RepairMilliPctPerS = 2000;   // (v4, 23/10) o conserto do ZH: 2% da vida cheia por segundo
		Dozer.MassKg = 49000;
		Dozer.Constructs = { "usa_power_plant", "usa_supply_center", "usa_war_factory", "usa_command_center" };
		if (bPatriot)
		{
			Dozer.Constructs.push_back("usa_patriot");
		}
		Dozer.Constructs.push_back("usa_drop_zone");   // (v4) a zona de lançamento (a IA sem o papel dela não constrói)
		Specs.push_back(Dozer);

		UnitSpec Truck;
		Truck.Key = "usa_supply_truck";
		Truck.MaxHp = 300;
		Truck.RadiusMm = 4000;
		Truck.PathRadiusMm = 1900;
		Truck.SpeedMmPerS = 10500;
		Truck.AccelMmPerS2 = 63200;
		Truck.BrakeMmPerS2 = 63200;
		Truck.TurnMilliDegPerS = 90000;
		Truck.SightMm = 39500;
		Truck.Cost = 600;
		Truck.BuildTimeMs = 10000;
		Truck.SupplyBoxes = 4;
		Truck.CrusherLevel = 1;   // (16/10) só o que for 0: contorna o carro; (v4) a árvore ele derruba pelo choque (15 t)
		Truck.MassKg = 15000;
		Specs.push_back(Truck);

		const auto Building = [](const char* Key, int32_t Hp, int32_t SX, int32_t SY, int32_t Sight, int32_t Cost, int32_t Ms, int32_t Energy)
		{
			UnitSpec S;
			S.Key = Key;
			S.Kind = ObjectKind::Structure;
			S.Armor = ArmorType::Structure;
			S.MaxHp = Hp;
			S.SizeXMm = SX;
			S.SizeYMm = SY;
			S.SightMm = Sight;
			S.Cost = Cost;
			S.BuildTimeMs = Ms;
			S.Energy = Energy;
			S.bCountsForVictory = true;
			return S;
		};
		UnitSpec CC = Building("usa_command_center", 5000, 31600, 36900, 79000, 2000, 45000, 0);
		CC.Produces = { "usa_dozer" };
		CC.ExitLaneMm = 6600;
		CC.bGrantsPowers = true;   // (v4) o CC pronto e vivo liga os poderes do general
		Specs.push_back(CC);
		Specs.push_back(Building("usa_power_plant", 800, 11600, 15800, 52700, 800, 10000, 5));
		UnitSpec SC = Building("usa_supply_center", 2000, 23200, 23700, 52700, 1500, 10000, -1);
		SC.bSupplyCenter = true;
		SC.FreeUnits = { "usa_supply_truck" };
		SC.Produces = { "usa_supply_truck" };
		SC.Requires = { "usa_power_plant" };
		SC.ExitLaneMm = 6600;   // a porta do caminhão fica livre (nosso: o ZH não tem faixa no centro)
		Specs.push_back(SC);
		UnitSpec WF = Building("usa_war_factory", 2000, 27900, 31600, 52700, 2000, 15000, -1);
		WF.Produces = { "usa_mbt", "usa_tomahawk" };   // (v4) o Crusader segue o primeiro
		WF.Requires = { "usa_supply_center" };
		WF.ExitLaneMm = 6600;
		Specs.push_back(WF);
		if (bPatriot)
		{
			// a defesa de base do ZH (AmericaPatriotBattery, FB; PatriotMissileWeapon, Weapon.ini; 14/10): 4 mísseis a
			// cada 250 ms e 2 s de recarga, 30 de explosão, alcance 225 (59,2 m), sem energia fica parada (POWERED)
			UnitSpec P = Building("usa_patriot", 1000, 8000, 8000, 94800, 1000, 25000, -3);
			P.bCountsForVictory = false;
			P.bNeedsPower = true;
			P.TurretTurnMilliDegPerS = 180000;
			P.TurretIdleReturnMs = 2000;
			P.Weapon.RangeMm = 59200;
			P.Weapon.Damage = 30;
			P.Weapon.Type = DamageType::Explosive;
			P.Weapon.ReloadMs = 250;
			P.Weapon.AimToleranceMilliDeg = 2000;
			P.Weapon.ProjectileSpeedMmPerS = 105300;
			P.Weapon.ClipSize = 4;
			P.Weapon.ClipReloadMs = 2000;
			// (16/10) o míssil de verdade (os mesmos números do Units.json): sai a 20 m/s inclinado 35°, acelera 120 m/s²
			// até 110 m/s e guia a 200°/s depois de 0,15 s
			P.Weapon.ProjectileSpeedMmPerS = 110000;
			P.Weapon.MissileLaunchSpeedMmPerS = 20000;
			P.Weapon.MissileAccelMmPerS2 = 120000;
			P.Weapon.MissileTurnMilliDegPerS = 200000;
			P.Weapon.MissileBoostMs = 150;
			P.Weapon.MissileLaunchPitchMilliDeg = 35000;
			P.Weapon.MissileLaunchHeightMm = 2400;
			P.Weapon.MissileSpreadMilliDeg = 10000;
			P.Weapon.MissileLifeMs = 4000;
			P.AssistRangeMm = 60000;   // (16/10) a ajuda entre as baterias (o RequestAssistRange do ZH)
			P.Requires = { "usa_power_plant" };
			Specs.push_back(P);
		}

		// (v4, 22/10) o spec da v4 (os mesmos números do Units.json; os testes não leem o arquivo), no fim da lista: os índices
		// de antes não mudam. A zona de lançamento: $2 500, 30 s, 1 200 PV, 20 × 20 m, −3 de energia, até 2 por jogador; o
		// avião 45 s depois de pronta e a cada 90 s (o aviso 10 s antes), 3 → 5 paletes de 4 caixas, a empilhadeira 3 s
		UnitSpec Zone = Building("usa_drop_zone", 1200, 20000, 20000, 52700, 2500, 30000, -3);
		Zone.bNeedsPower = true;
		Zone.Requires = { "usa_war_factory" };
		Zone.MaxPerPlayer = 2;
		Zone.AirliftFirstMs = 45000;
		Zone.AirliftPeriodMs = 90000;
		Zone.AirliftLeadMs = 10000;
		Zone.AirliftPalletsMin = 3;
		Zone.AirliftPalletsMax = 5;
		Zone.AirliftBoxesPerPallet = 4;
		Zone.AirliftUnloadMsPerPallet = 3000;
		Specs.push_back(Zone);
		// o lançador Tomahawk (patente 3): o chassi do caminhão, 85 m de alcance e 20 m de mínimo, rajada de 2 mísseis de 110
		// com dano em área (cheio até 2 m, 50% em 5 m)
		UnitSpec Tomahawk;
		Tomahawk.Key = "usa_tomahawk";
		Tomahawk.MaxHp = 200;
		Tomahawk.RadiusMm = 4000;
		Tomahawk.PathRadiusMm = 1900;
		Tomahawk.SpeedMmPerS = 7000;
		Tomahawk.AccelMmPerS2 = 63200;
		Tomahawk.BrakeMmPerS2 = 63200;
		Tomahawk.TurnMilliDegPerS = 90000;
		Tomahawk.TurretTurnMilliDegPerS = 90000;
		Tomahawk.TurretIdleReturnMs = 2000;
		Tomahawk.SightMm = 70000;
		Tomahawk.CrusherLevel = 1;
		Tomahawk.MassKg = 30000;   // (v4, 23/10) o lançador 8x8 de ~30 t: derruba a palmeira pelo choque
		Tomahawk.Cost = 1200;
		Tomahawk.BuildTimeMs = 15000;
		Tomahawk.Requires = { "usa_war_factory" };
		Tomahawk.RequiresRank = 3;
		Tomahawk.Weapon.RangeMm = 85000;
		Tomahawk.Weapon.MinRangeMm = 20000;
		Tomahawk.Weapon.Damage = 110;
		Tomahawk.Weapon.Type = DamageType::Explosive;
		Tomahawk.Weapon.ReloadMs = 600;
		Tomahawk.Weapon.AimToleranceMilliDeg = 3000;
		Tomahawk.Weapon.ProjectileSpeedMmPerS = 55000;
		Tomahawk.Weapon.ClipSize = 2;
		Tomahawk.Weapon.ClipReloadMs = 9000;
		Tomahawk.Weapon.MissileLaunchSpeedMmPerS = 15000;
		Tomahawk.Weapon.MissileAccelMmPerS2 = 40000;
		Tomahawk.Weapon.MissileTurnMilliDegPerS = 100000;
		Tomahawk.Weapon.MissileBoostMs = 400;
		Tomahawk.Weapon.MissileLaunchPitchMilliDeg = 45000;   // (v4, 23/10) o TEL do BGM-109G e o modelo: 45°
		Tomahawk.Weapon.MissileLaunchHeightMm = 2600;
		// (v4, 23/10) a boca do tubo levantado: o pivô da torre 1,296 m atrás do meio do casco, a boca 0,463 m à frente dele,
		// ±0,302 m de lado e a 3,29 m do chão. A mira antes do tiro, a mesma do Units.json (pre_attack_s 1,0: 45° a 60°/s =
		// 0,75 s + o PreAttackDelay de 0,25 s do ZH): os testes da IA (o cerco do 24d) rodam com a mira do jogo. (revisão de
		// 23/10) Ela ficava de fora porque o 24d caía com ela; a causa era outra: o reforço empurrava o Tomahawk parado pra
		// dentro do alcance da Patriot (sem a mira passava por 0,4 s). Com quem atira parado segurando o lugar (o moveAllies
		// do ZH, World::SeparateUnits), o cerco derruba as duas Patriots sem perder lançador, com a mira e sem ela
		Tomahawk.Weapon.PreAttackMs = 1000;
		Tomahawk.Weapon.LaunchPivotXMm = -1296;
		Tomahawk.Weapon.LaunchForwardMm = 463;
		Tomahawk.Weapon.LaunchSideMm = 302;
		Tomahawk.Weapon.LaunchUpMm = 3290;
		Tomahawk.Weapon.MissileSpreadMilliDeg = 4000;
		Tomahawk.Weapon.MissileLifeMs = 6000;
		Tomahawk.Weapon.SplashFullMm = 2000;
		Tomahawk.Weapon.SplashMm = 5000;
		Tomahawk.Weapon.SplashEdgePct = 50;
		Specs.push_back(Tomahawk);
		// o míssil do Cruise Missile Strike: unidade só de dados (nunca sai de fábrica; custo 0), 150 de explosão em área
		// (cheio até 3 m, 50% em 6 m), 55 m/s do começo ao fim; sem alcance (não é "armada": a IA não a conta)
		UnitSpec Cruise;
		Cruise.Key = "usa_cruise_missile";
		Cruise.MaxHp = 1;
		Cruise.RadiusMm = 500;
		Cruise.PathRadiusMm = 500;
		Cruise.Weapon.Damage = 150;
		Cruise.Weapon.Type = DamageType::Explosive;
		Cruise.Weapon.ProjectileSpeedMmPerS = 55000;
		Cruise.Weapon.SplashFullMm = 3000;
		Cruise.Weapon.SplashMm = 6000;
		Cruise.Weapon.SplashEdgePct = 50;
		Cruise.Weapon.MissileLaunchSpeedMmPerS = 55000;
		Cruise.Weapon.MissileTurnMilliDegPerS = 90000;
		Cruise.Weapon.MissileBoostMs = 300;
		Cruise.Weapon.MissileLifeMs = 8000;
		Specs.push_back(Cruise);
		// o poder: na patente 2, 6 mísseis num círculo de 12 m; na 4, 9 em 14 m; 180 s de espera; um a cada 0,2 s, saindo 150 m
		// atrás do ponto e 45 m acima
		PowerSpec Strike;
		Strike.Key = "cruise_strike";
		Strike.RequiresRank = 2;
		Strike.UpgradeRank = 4;
		Strike.CooldownMs = 180000;
		Strike.Missiles = 6;
		Strike.MissilesUpgraded = 9;
		Strike.PatternRadiusMm = 12000;
		Strike.PatternRadiusUpgradedMm = 14000;
		Strike.IntervalMs = 200;
		Strike.SpawnBackMm = 150000;
		Strike.SpawnUpMm = 45000;
		Strike.Munition = "usa_cruise_missile";

		ArmorTable Armor;
		Armor.Set(ArmorType::Tank, DamageType::ArmorPiercing, 100);
		Armor.Set(ArmorType::Structure, DamageType::ArmorPiercing, 100);
		Armor.Set(ArmorType::Structure, DamageType::SmallArms, 50);
		Armor.Set(ArmorType::Structure, DamageType::Explosive, 100);
		// a explosão no tanque, como no Units.json (antes só com a Patriot; sem arma de explosão no jogo, nada muda)
		Armor.Set(ArmorType::Tank, DamageType::Explosive, 100);
		EconomyRules E;
		E.StartingMoney = 10000;
		E.BoxValue = 75;
		E.TicksPerBox = TicksFromMs(1000);
		E.CenterTicks = TicksFromMs(400);
		E.DockScanDistance = Fix::FromMilli(184300);
		E.MaxQueue = 9;
		E.MaxFlatnessCm = 263;
		E.EdgeMargin = Fix::FromMilli(7900);
		E.SupplyCenterDockGap = Fix::FromMilli(5270);
		E.DockClearCells = 1;
		E.DropPileHalf = Fix::FromMilli(4200);
		// (v4) a patente: 600 / 1 500 / 3 000 / 5 000 XP; 1 XP por $10 destruído, 1 por $30 perdido
		const int32_t RankXp[5] = { 0, 600, 1500, 3000, 5000 };
		std::copy(RankXp, RankXp + 5, E.RankXp);
		E.XpDealtDivisor = 10;
		E.XpTakenDivisor = 30;
		Ruleset R;
		std::string Error;
		Check(Ruleset::Build(Specs, { Strike }, Armor, E, R, &Error), "as regras da economia montam");
		if (!Error.empty())
		{
			std::printf("  erro: %s\n", Error.c_str());
		}
		return R;
	}

	SupplyDockSetup DockAt(Vec2 P, int32_t Boxes)
	{
		SupplyDockSetup D;
		D.Pos = P;
		D.HalfX = Fix::FromMilli(13165);   // SupplyDock: Geometry 50 × 61 u
		D.HalfY = Fix::FromMilli(16061);
		D.Boxes = Boxes;
		return D;
	}

	Vec2 Offset(Vec2 P, int32_t DXm, int32_t DYm)
	{
		return P + Vec2::Make(Fix::FromInt(DXm), Fix::FromInt(DYm));
	}

	int32_t CountOf(const World& W, PlayerIndex P, uint16_t Def)
	{
		int32_t N = 0;
		for (int32_t I = 0; I < W.UnitCount(); ++I)
		{
			const UnitView V = W.Unit(I);
			N += V.Owner == P && V.Def == Def ? 1 : 0;
		}
		return N;
	}

	EntityId FindOf(const World& W, PlayerIndex P, uint16_t Def)
	{
		for (int32_t I = 0; I < W.UnitCount(); ++I)
		{
			const UnitView V = W.Unit(I);
			if (V.Owner == P && V.Def == Def)
			{
				return V.Id;
			}
		}
		return NoEntity;
	}

	Command Group(PlayerIndex P, std::vector<EntityId> Units)
	{
		Command C;
		C.Player = P;
		C.Type = CommandType::SetGroup;
		C.Units = std::move(Units);
		return C;
	}

	Command Order(PlayerIndex P, CommandType Type, EntityId Target = NoEntity, int32_t Value = 0, Vec2 Point = Vec2::Zero())
	{
		Command C;
		C.Player = P;
		C.Type = Type;
		C.Target = Target;
		C.Value = Value;
		C.Point = Point;
		return C;
	}

	/** Anda até Done() ou o teto; devolve os passos andados. */
	template <typename Fn>
	int32_t RunUntil(World& W, int32_t MaxTicks, Fn&& Done)
	{
		int32_t T = 0;
		while (T < MaxTicks && !Done())
		{
			StepWith(W, {});
			++T;
		}
		return T;
	}

	struct EconomyRun
	{
		std::vector<uint64_t> Hashes;
		int32_t Income = 0;
	};

	/** A abertura da USA do ZH: usina, centro de suprimentos, War Factory e tanques. */
	EconomyRun RunEconomyScenario(bool bPrint)
	{
		EconomyRun Out;
		MatchSetup S;
		S.Seed = 5;
		S.Map = MakeOpenMap(160, 160);
		// chão com altura: um morro de 5 m ao norte do CC (o prédio ali tem de ser recusado)
		S.Map.MinDm.assign(S.Map.Pass.size(), 0);
		S.Map.MaxDm.assign(S.Map.Pass.size(), 0);
		for (int32_t Y = 50; Y < 56; ++Y)
		{
			for (int32_t X = 36; X < 44; ++X)
			{
				S.Map.MaxDm[size_t(S.Map.Index(X, Y))] = 50;
			}
		}
		S.Rules = EconomyRuleset();
		const Vec2 Base = S.Map.CellCenter(Cell{ 40, 40 });
		const Vec2 DockPos = Offset(Base, 0, 120);
		S.Docks = { DockAt(DockPos, 400) };
		PlayerSetup P0;
		P0.Units = { { "usa_command_center", Base, Angle() }, { "usa_dozer", Offset(Base, -40, 0), Angle() } };
		PlayerSetup P1;
		P1.Team = 1;
		P1.bIsAI = true;
		P1.Units = { { "usa_command_center", S.Map.CellCenter(Cell{ 130, 130 }), Angle::FromBam(Angle::HalfTurn) } };
		S.Players = { P0, P1 };

		World W;
		Check(W.Init(S), "montagem com base");
		const Ruleset& R = W.Rules();
		const uint16_t PP = uint16_t(R.FindUnit("usa_power_plant"));
		const uint16_t SC = uint16_t(R.FindUnit("usa_supply_center"));
		const uint16_t WF = uint16_t(R.FindUnit("usa_war_factory"));
		const uint16_t Truck = uint16_t(R.FindUnit("usa_supply_truck"));
		const uint16_t Mbt = uint16_t(R.FindUnit("usa_mbt"));
		const uint16_t DozerDef = uint16_t(R.FindUnit("usa_dozer"));
		const EntityId Dozer = FindOf(W, 0, DozerDef);
		const EntityId CC = FindOf(W, 0, uint16_t(R.FindUnit("usa_command_center")));
		Check(CC != NoEntity && Dozer != NoEntity, "comeca com o CC pronto e o Dozer (ZH)");
		Check(W.Money(0) == 10000, "comeca com 10 000 (ZH)");
		Check(W.PowerFactorPermille(0) == 500, "sem usina nenhuma (0/0) a producao anda a metade (ZH)");

		// onde dá e onde não dá (ZH BuildAssistant)
		Check(W.CanPlace(0, PP, 0, Offset(Base, 0, -45)) == PlaceResult::Ok, "usina ao sul do CC cabe");
		Check(W.CanPlace(0, PP, 0, Base) == PlaceResult::Blocked, "em cima do CC nao");
		Check(W.CanPlace(0, PP, 0, Offset(Base, 23, 0)) == PlaceResult::BlocksExit, "na faixa de saida do CC nao");
		Check(W.CanPlace(0, PP, 0, Offset(Base, 0, 50)) == PlaceResult::NotFlat, "no morro de 5 m nao (o ZH exige plano)");
		Check(W.CanPlace(0, PP, 0, S.Map.CellCenter(Cell{ 1, 40 })) == PlaceResult::NearEdge, "colado na borda nao");
		Check(W.CanPlace(0, PP, 0, S.Map.CellCenter(Cell{ 100, 60 })) == PlaceResult::NotVisible, "fora da visao nao");
		Check(W.CanPlace(0, SC, 0, Offset(Base, 0, -45)) == PlaceResult::MissingPrereq, "centro de suprimentos sem usina nao");

		// 1. a usina: o Dozer anda até lá e constrói; sem energia, 10 s viram 20 s
		StepWith(W, { Group(0, { Dozer }), Order(0, CommandType::Build, NoEntity, BuildValue(PP, 0), Offset(Base, 0, -45)) });
		Check(W.Money(0) == 9200, "a usina custa 800, pagos quando a obra marca o chao");
		const int32_t PlantTicks = RunUntil(W, 60 * TicksPerSecond, [&]() { return W.Player(0).PowerMade > 0; });
		if (bPrint)
		{
			std::printf("  usina pronta em %.1f s (10 s de obra a metade da velocidade + a viagem)\n", double(PlantTicks) / TicksPerSecond);
		}
		Check(PlantTicks >= 20 * TicksPerSecond && PlantTicks < 30 * TicksPerSecond, "a usina leva 20 s de obra sem energia (ZH 0/0) mais a viagem");
		Check(W.PowerFactorPermille(0) == 1000, "com a usina, ritmo normal");

		// 2. o centro de suprimentos perto do depósito: o Dozer vai primeiro (o ZH só constrói à vista)
		const Vec2 SCPos = Offset(DockPos, 13 + 8 + 12, 0);
		StepWith(W, { Order(0, CommandType::Move, NoEntity, 0, Offset(SCPos, 0, -20)) });
		RunUntil(W, 40 * TicksPerSecond, [&]() { UnitView D; W.FindUnit(Dozer, D); return D.Order == OrderType::Idle; });
		Check(W.CanPlace(0, SC, 0, Offset(DockPos, 30, 0)) == PlaceResult::NearSupplies, "o centro a 4,8 m do deposito nao (o ZH pede 5,3 m)");
		Check(W.CanPlace(0, SC, 0, SCPos) == PlaceResult::Ok, "o centro a 8 m do deposito cabe");
		StepWith(W, { Order(0, CommandType::Build, NoEntity, BuildValue(SC, 0), SCPos) });
		const int32_t CenterTicks = RunUntil(W, 40 * TicksPerSecond, [&]() { return CountOf(W, 0, Truck) > 0; });
		if (bPrint)
		{
			std::printf("  centro de suprimentos pronto em %.1f s, com o caminhao gratis\n", double(CenterTicks) / TicksPerSecond);
		}
		Check(CountOf(W, 0, Truck) == 1, "o centro pronto solta 1 caminhao gratis");

		// 3. o caminhão trabalha sozinho: 4 caixas × 75 por viagem
		const int32_t Before = W.Money(0);
		const int32_t FirstTicks = RunUntil(W, 60 * TicksPerSecond, [&]() { return W.Money(0) > Before; });
		Check(W.Money(0) == Before + 300, "a primeira viagem rende 300 (4 caixas x 75)");
		Check(W.Dock(0).Boxes == 396, "o deposito perdeu 4 caixas");
		if (bPrint)
		{
			std::printf("  primeira entrega %.1f s depois do caminhao nascer\n", double(FirstTicks) / TicksPerSecond);
		}

		// 4. a War Factory e os tanques
		const Vec2 WFPos = Offset(Base, 50, -45);
		StepWith(W, { Order(0, CommandType::Move, NoEntity, 0, Offset(WFPos, -25, 0)) });
		RunUntil(W, 40 * TicksPerSecond, [&]() { UnitView D; W.FindUnit(Dozer, D); return D.Order == OrderType::Idle; });
		Check(W.CanPlace(0, WF, 0, WFPos) == PlaceResult::Ok, "a War Factory cabe");
		StepWith(W, { Order(0, CommandType::Build, NoEntity, BuildValue(WF, 0), WFPos) });
		RunUntil(W, 60 * TicksPerSecond, [&]() { const EntityId F = FindOf(W, 0, WF); UnitView V; return F != NoEntity && W.FindUnit(F, V) && V.BuildLeft == 0; });
		const EntityId Factory = FindOf(W, 0, WF);
		const int32_t BeforeTanks = W.Money(0);
		StepWith(W, { Order(0, CommandType::Produce, Factory, Mbt), Order(0, CommandType::Produce, Factory, Mbt), Order(0, CommandType::Produce, Factory, Mbt) });
		Check(W.Money(0) == BeforeTanks - 2700, "3 tanques na fila: 2700 pagos na hora (ZH)");
		StepWith(W, { Order(0, CommandType::CancelProduce, Factory, 2) });
		Check(W.Money(0) == BeforeTanks - 1800, "cancelar o terceiro devolve 900");
		const int32_t TankTicks = RunUntil(W, 30 * TicksPerSecond, [&]() { return CountOf(W, 0, Mbt) > 0; });
		if (bPrint)
		{
			std::printf("  1o tanque %.1f s depois de entrar na fila\n", double(TankTicks) / TicksPerSecond);
		}
		Check(TankTicks >= 10 * TicksPerSecond - 2 && TankTicks <= 10 * TicksPerSecond + 2, "o tanque leva 10 s com energia (ZH)");
		Check(W.CanPlace(0, PP, 0, Offset(WFPos, 22, 0)) == PlaceResult::BlocksExit, "nada na frente da porta da War Factory");

		// 5. a obra cancelada devolve tudo e some sem explodir
		const int32_t BeforeCancel = W.Money(0);
		StepWith(W, { Order(0, CommandType::Build, NoEntity, BuildValue(PP, 0), Offset(Base, -30, -45)) });
		const EntityId Foundation = W.Unit(W.UnitCount() - 1).Id;
		StepWith(W, { Order(0, CommandType::CancelBuild, Foundation) });
		bool bCancelled = false;
		bool bExploded = false;
		for (const SimEvent& E : W.LastEvents())
		{
			bCancelled |= E.Type == EventType::StructureCancelled && E.Target == Foundation;
			bExploded |= E.Type == EventType::UnitDied && E.Target == Foundation;
		}
		StepWith(W, {});
		UnitView Gone;
		Check(W.Money(0) == BeforeCancel && bCancelled && !bExploded && !W.FindUnit(Foundation, Gone), "cancelar a obra devolve 100% e ela some sem explosao");

		// 5b. a obra em cima do próprio Dozer (o jogador clica onde ele está): ele sai pela borda e
		// constrói (no teste base1 de 10/10 ele ficou parado no lote esperando ele vagar)
		{
			const Vec2 Spot = Offset(Base, -30, -45);
			StepWith(W, { Order(0, CommandType::Move, NoEntity, 0, Spot) });
			RunUntil(W, 40 * TicksPerSecond, [&]() { UnitView D; W.FindUnit(Dozer, D); return D.Order == OrderType::Idle; });
			UnitView D;
			W.FindUnit(Dozer, D);
			Check(W.CanPlace(0, PP, 0, D.Pos) == PlaceResult::Ok, "a usina cabe em cima do proprio Dozer (amigo sai da frente)");
			const int32_t PowerBefore = W.Player(0).PowerMade;
			StepWith(W, { Order(0, CommandType::Build, NoEntity, BuildValue(PP, 0), D.Pos) });
			const int32_t OnTopTicks = RunUntil(W, 40 * TicksPerSecond, [&]() { return W.Player(0).PowerMade > PowerBefore; });
			if (bPrint)
			{
				std::printf("  usina em cima do Dozer pronta em %.1f s (ele sai pela borda e constroi)\n", double(OnTopTicks) / TicksPerSecond);
			}
			Check(W.Player(0).PowerMade > PowerBefore, "a obra em cima do Dozer termina");
		}

		// 6. renda: 2 minutos de um caminhão com o centro colado no depósito
		int32_t Income = 0;
		for (int32_t T = 0; T < 120 * TicksPerSecond; ++T)
		{
			StepWith(W, {});
			for (const SimEvent& E : W.LastEvents())
			{
				Income += E.Type == EventType::SupplyDelivered && E.Value > 0 ? E.Value : 0;
			}
			Out.Hashes.push_back(W.StateHash());
		}
		Out.Income = Income;
		if (bPrint)
		{
			std::printf("  renda de 1 caminhao com o centro a 8 m do deposito: %d $/min (a revisao estimou ~1450 a 15 m)\n", Income / 2);
		}
		Check(Income / 2 > 900 && Income / 2 < 2000, "a renda de um caminhao fica na faixa do ZH");
		return Out;
	}

	// 5c. o Dozer dentro do lote, com a borda mais perto colada noutro prédio: sai por outro lado e
	// constrói (lan2v2, 11/10: a saída caía dentro do centro de suprimentos e ele ficava parado)
	void TestLotExitBlocked()
	{
		std::printf("[saida do lote]\n");
		MatchSetup S;
		S.Seed = 5;
		S.Map = MakeOpenMap(160, 160);
		S.Rules = EconomyRuleset();
		S.bFogOfWar = false;
		const Vec2 C = S.Map.CellCenter(Cell{ 80, 80 });
		PlayerSetup P0;
		// a geometria do lan2v2: a usina pronta à esquerda (x de -23,8 a -12,2 m), uma fresta de 6,4 m
		// até o lote novo (x de -5,8 a 5,8 m), e o Dozer 1,8 m dentro dele. A saída pela esquerda caía
		// dentro da usina; o caminho terminava na fresta, a ~6 m, que já conta como chegada: ele não
		// andava e repetia a ordem pra sempre
		P0.Units = { { "usa_command_center", Offset(C, 0, -60), Angle() }, { "usa_power_plant", Offset(C, -18, 0), Angle() }, { "usa_dozer", Offset(C, -4, 0), Angle() } };
		PlayerSetup P1;
		P1.Team = 1;
		P1.Units = { { "usa_command_center", S.Map.CellCenter(Cell{ 140, 140 }), Angle() } };
		S.Players = { P0, P1 };
		World W;
		Check(W.Init(S), "montagem da saida do lote");
		const uint16_t PP = uint16_t(W.Rules().FindUnit("usa_power_plant"));
		const EntityId Dozer = FindOf(W, 0, uint16_t(W.Rules().FindUnit("usa_dozer")));
		Check(W.CanPlace(0, PP, 0, C) == PlaceResult::Ok, "a usina nova cabe com o Dozer em cima (amigo sai da frente)");
		const int32_t PowerBefore = W.Player(0).PowerMade;
		StepWith(W, { Group(0, { Dozer }) });
		StepWith(W, { Order(0, CommandType::Build, NoEntity, BuildValue(PP, 0), C) });
		const int32_t Ticks = RunUntil(W, 60 * TicksPerSecond, [&]() { return W.Player(0).PowerMade > PowerBefore; });
		std::printf("  usina com a saida mais perto bloqueada pronta em %.1f s\n", double(Ticks) / TicksPerSecond);
		Check(W.Player(0).PowerMade > PowerBefore, "o Dozer sai pelo lado livre e a obra termina");
	}

	// 5d. o "desfile" (11/10): os tanques da fábrica paravam em fila indiana no ponto de encontro,
	// cada um encostando no de trás. No ZH cada um acha a vaga livre mais perto (adjustDestination)
	void TestRallySpread()
	{
		std::printf("[ponto de encontro]\n");
		MatchSetup S;
		S.Seed = 6;
		S.Map = MakeOpenMap(160, 160);
		S.Rules = EconomyRuleset();
		S.bFogOfWar = false;
		const Vec2 C = S.Map.CellCenter(Cell{ 60, 60 });
		PlayerSetup P0;
		P0.Units = { { "usa_command_center", Offset(C, -70, 0), Angle() }, { "usa_power_plant", Offset(C, -40, -40), Angle() },
			{ "usa_power_plant", Offset(C, -25, -40), Angle() }, { "usa_war_factory", C, Angle() } };
		PlayerSetup P1;
		P1.Team = 1;
		P1.Units = { { "usa_command_center", S.Map.CellCenter(Cell{ 145, 145 }), Angle() } };
		S.Players = { P0, P1 };
		World W;
		Check(W.Init(S), "montagem do ponto de encontro");
		const uint16_t Mbt = uint16_t(W.Rules().FindUnit("usa_mbt"));
		const EntityId Factory = FindOf(W, 0, uint16_t(W.Rules().FindUnit("usa_war_factory")));
		const Vec2 Rally = Offset(C, 45, 0);
		std::vector<Command> Orders = { Order(0, CommandType::SetRally, Factory, 0, Rally) };
		for (int32_t K = 0; K < 6; ++K)
		{
			Orders.push_back(Order(0, CommandType::Produce, Factory, Mbt));
		}
		StepWith(W, Orders);
		const auto Tanks = [&]()
		{
			std::vector<UnitView> Out;
			for (int32_t I = 0; I < W.UnitCount(); ++I)
			{
				const UnitView V = W.Unit(I);
				if (V.Owner == 0 && V.Def == Mbt)
				{
					Out.push_back(V);
				}
			}
			return Out;
		};
		RunUntil(W, 180 * TicksPerSecond, [&]()
		{
			const std::vector<UnitView> T = Tanks();
			if (T.size() < 6)
			{
				return false;
			}
			for (const UnitView& V : T)
			{
				if (V.Order != OrderType::Idle)
				{
					return false;
				}
			}
			return true;
		});
		RunUntil(W, 3 * TicksPerSecond, []() { return false; });   // o giro final no lugar
		const std::vector<UnitView> T = Tanks();
		double MinGap = 1e9;
		double MaxFromRally = 0.0;
		double SpanX = 0.0;
		double SpanY = 0.0;
		double X0 = 1e9, X1 = -1e9, Y0 = 1e9, Y1 = -1e9;
		for (size_t A = 0; A < T.size(); ++A)
		{
			const double Ax = T[A].Pos.X.ToDouble(), Ay = T[A].Pos.Y.ToDouble();
			MaxFromRally = std::max(MaxFromRally, std::hypot(Ax - Rally.X.ToDouble(), Ay - Rally.Y.ToDouble()));
			X0 = std::min(X0, Ax); X1 = std::max(X1, Ax); Y0 = std::min(Y0, Ay); Y1 = std::max(Y1, Ay);
			for (size_t B = A + 1; B < T.size(); ++B)
			{
				MinGap = std::min(MinGap, std::hypot(Ax - T[B].Pos.X.ToDouble(), Ay - T[B].Pos.Y.ToDouble()));
			}
		}
		SpanX = X1 - X0;
		SpanY = Y1 - Y0;
		std::printf("  %zu tanques: o mais longe do ponto a %.1f m, o par mais perto a %.1f m, o bolo %.0f x %.0f m\n",
			T.size(), MaxFromRally, MinGap, SpanX, SpanY);
		Check(T.size() == 6, "os 6 tanques saem da fabrica");
		Check(MaxFromRally < 20.0, "todos param perto do ponto de encontro (bolo, nao fila)");
		Check(std::min(SpanX, SpanY) > 6.0, "espalhados nos dois sentidos (nao em fila indiana)");
		Check(MinGap > 6.0, "sem tanque encostado no outro");
		// a área de reunião: todos virados pra longe da fábrica (o ponto está a +X dela), ±10°
		int32_t Facing = 0;
		for (const UnitView& V : T)
		{
			const double Deg = V.Hull.ToDegrees();
			Facing += (Deg < 10.0 || Deg > 350.0) ? 1 : 0;
		}
		std::printf("  virados pra longe da fabrica: %d de %zu\n", Facing, T.size());
		Check(Facing == int32_t(T.size()), "chegam virados pra frente da area de reuniao");
		// o leque: as torres das pontas olham pra fora, nenhuma passa de 25°
		double Left = 0.0, Right = 0.0, Widest = 0.0;
		for (const UnitView& V : T)
		{
			const double Rel = double(V.Hull.DeltaTo(V.Turret)) * 360.0 / double(Angle::FullTurn);
			Left = std::max(Left, Rel);
			Right = std::min(Right, Rel);
			Widest = std::max(Widest, std::abs(Rel));
		}
		std::printf("  torres em leque: de %.0f a %.0f graus do casco\n", Right, Left);
		Check(Left > 10.0 && Right < -10.0, "as torres paradas abrem em leque (as pontas olham pra fora)");
		Check(Widest < 26.0, "o leque nao passa de 25 graus");
	}

	void TestEconomy()
	{
		std::printf("[economia e construcao]\n");
		const EconomyRun A = RunEconomyScenario(true);
		const EconomyRun B = RunEconomyScenario(false);
		Check(A.Hashes == B.Hashes && A.Income == B.Income, "a economia roda igual duas vezes, passo a passo");

		// a carga que cai (decisão do Felipe, 10/10): o caminhão cheio destruído solta as caixas
		MatchSetup S;
		S.Seed = 9;
		S.Map = MakeOpenMap(160, 160);
		S.Rules = EconomyRuleset();
		S.bFogOfWar = false;
		const Vec2 Dock = S.Map.CellCenter(Cell{ 40, 40 });
		S.Docks = { DockAt(Dock, 400) };
		PlayerSetup P0;
		P0.Units = {
			{ "usa_power_plant", Offset(Dock, -60, -60), Angle() },
			{ "usa_supply_center", Offset(Dock, 150, 0), Angle() },
			{ "usa_supply_truck", Offset(Dock, 0, -25), Angle() },
		};
		PlayerSetup P1;
		P1.Team = 1;
		P1.bIsAI = true;
		for (int32_t K = 0; K < 4; ++K)
		{
			P1.Units.push_back({ "usa_mbt", Offset(Dock, 70 + K * 8, 70), Angle() });
		}
		S.Players = { P0, P1 };
		World W;
		Check(W.Init(S), "montagem da carga que cai");
		const uint16_t TruckDef = uint16_t(W.Rules().FindUnit("usa_supply_truck"));
		const EntityId Truck = FindOf(W, 0, TruckDef);
		RunUntil(W, 40 * TicksPerSecond, [&]() { UnitView V; return W.FindUnit(Truck, V) && V.Task == UnitTask::ToCenter && V.Cargo == 4; });
		std::vector<EntityId> Tanks;
		for (int32_t I = 0; I < W.UnitCount(); ++I)
		{
			if (W.Unit(I).Owner == 1)
			{
				Tanks.push_back(W.Unit(I).Id);
			}
		}
		StepWith(W, { Group(1, Tanks), Order(1, CommandType::Attack, Truck) });
		int32_t PileIndex = -1;
		RunUntil(W, 60 * TicksPerSecond, [&]()
		{
			for (const SimEvent& E : W.LastEvents())
			{
				PileIndex = E.Type == EventType::CargoDropped ? E.Value : PileIndex;
			}
			return PileIndex >= 0;
		});
		Check(PileIndex == 1 && W.Dock(1).bPile && W.Dock(1).Boxes == 4, "o caminhao cheio destruido solta as 4 caixas no chao");
		// os tanques vão embora; um caminhão novo busca a pilha
		StepWith(W, { Order(1, CommandType::Move, NoEntity, 0, S.Map.CellCenter(Cell{ 150, 150 })) });
		const EntityId Center = FindOf(W, 0, uint16_t(W.Rules().FindUnit("usa_supply_center")));
		StepWith(W, { Order(0, CommandType::Produce, Center, TruckDef) });
		RunUntil(W, 20 * TicksPerSecond, [&]() { return CountOf(W, 0, TruckDef) > 0; });
		const EntityId Second = FindOf(W, 0, TruckDef);
		StepWith(W, { Group(0, { Second }), Order(0, CommandType::Gather, NoEntity, PileIndex) });
		const int32_t MoneyBefore = W.Money(0);
		RunUntil(W, 90 * TicksPerSecond, [&]() { return W.Money(0) > MoneyBefore; });
		Check(W.Dock(PileIndex).Boxes == 0 && W.Money(0) == MoneyBefore + 300, "outro caminhao busca a pilha e entrega os 300");

		// vitória pelo prédio pronto (ajuste ao ZH: a fundação não segura o jogador vivo)
		MatchSetup V;
		V.Seed = 11;
		V.Map = MakeOpenMap(160, 160);
		V.Rules = EconomyRuleset();
		V.bFogOfWar = false;
		const Vec2 Their = V.Map.CellCenter(Cell{ 100, 100 });
		PlayerSetup A0;
		for (int32_t K = 0; K < 6; ++K)
		{
			A0.Units.push_back({ "usa_mbt", Offset(Their, -70 + (K % 3) * 9, -70 + (K / 3) * 9), Angle() });
		}
		PlayerSetup A1;
		A1.Team = 1;
		A1.bIsAI = true;
		A1.Units = { { "usa_command_center", Their, Angle() }, { "usa_dozer", Offset(Their, 0, 40), Angle() } };
		V.Players = { A0, A1 };
		World VW;
		Check(VW.Init(V), "montagem da vitoria");
		const uint16_t PPDef = uint16_t(VW.Rules().FindUnit("usa_power_plant"));
		const EntityId TheirDozer = FindOf(VW, 1, uint16_t(VW.Rules().FindUnit("usa_dozer")));
		StepWith(VW, { Group(1, { TheirDozer }), Order(1, CommandType::Build, NoEntity, BuildValue(PPDef, 0), Offset(Their, 0, 60)) });
		Check(CountOf(VW, 1, PPDef) == 1, "o inimigo marcou uma fundacao");
		StepWith(VW, { Group(1, { TheirDozer }), Order(1, CommandType::Stop) });   // a fundação fica abandonada
		const EntityId TheirCC = FindOf(VW, 1, uint16_t(VW.Rules().FindUnit("usa_command_center")));
		std::vector<EntityId> Army;
		for (int32_t I = 0; I < VW.UnitCount(); ++I)
		{
			if (VW.Unit(I).Owner == 0)
			{
				Army.push_back(VW.Unit(I).Id);
			}
		}
		StepWith(VW, { Group(0, Army), Order(0, CommandType::Attack, TheirCC) });
		const int32_t KillTicks = RunUntil(VW, 300 * TicksPerSecond, [&]() { return VW.IsMatchOver(); });
		std::printf("  6 tanques derrubam o CC em %.1f s\n", double(KillTicks) / TicksPerSecond);
		Check(VW.IsMatchOver() && VW.WinningTeam() == 0, "sem o CC pronto o inimigo perde, mesmo com a fundacao de pe");
		RunUntil(VW, 3, []() { return false; });
		Check(CountOf(VW, 1, PPDef) == 0 && FindOf(VW, 1, uint16_t(VW.Rules().FindUnit("usa_dozer"))) == NoEntity, "quem perde a base perde o resto (ZH)");

		// a montagem com economia sobrevive à gravação
		ByteWriter BW;
		WriteSetup(BW, S);
		MatchSetup Back;
		ByteReader BR(BW.Bytes.data(), BW.Bytes.size());
		Check(ReadSetup(BR, Back) && Back.Hash() == S.Hash(), "a montagem com depositos e regras da economia volta igual do replay");
	}

	// ------------------------------------------------------------------ 4f. a partida inteira: robô × robô com base

	MatchSetup MakeEconomyMatch(uint64_t Seed)
	{
		MatchSetup S;
		S.Seed = Seed;
		S.Map = MakeOpenMap(256, 256);   // 1024 m, como o deserto
		Rng Rocks(Seed ^ 0x5EED);
		for (int32_t K = 0; K < 40; ++K)
		{
			// pedras só no meio, longe das bases (o lote da base fica livre, como o WorldForge vai garantir)
			const int32_t CX = 100 + Rocks.Below(56);
			const int32_t CY = 100 + Rocks.Below(56);
			for (int32_t DY = 0; DY < 3; ++DY)
			{
				for (int32_t DX = 0; DX < 3; ++DX)
				{
					S.Map.Pass[S.Map.Index(CX + DX, CY + DY)] = uint8_t(CellPass::Blocked);
				}
			}
		}
		S.Rules = EconomyRuleset();
		// as largadas do World_TestSalt2 (Maps.json) e as docas espelhadas pelo giro de 180°
		const Vec2 Start0 = Vec2::Make(Fix::FromMilli(-282600), Fix::FromMilli(-335700));
		const Vec2 Start1 = -Start0;
		S.Docks = { DockAt(Offset(Start0, 75, 0), 400), DockAt(Offset(Start1, -75, 0), 400),
			DockAt(Vec2::Make(Fix::FromInt(-70), Fix::FromInt(70)), 400), DockAt(Vec2::Make(Fix::FromInt(70), Fix::FromInt(-70)), 400) };
		for (int32_t P = 0; P < 2; ++P)
		{
			PlayerSetup PS;
			PS.Team = uint8_t(P);
			PS.bIsAI = true;
			const Vec2 Base = P == 0 ? Start0 : Start1;
			const Angle Facing = Angle::FromBam(P == 0 ? Angle::QuarterTurn : 3 * Angle::QuarterTurn);   // a porta pro inimigo
			PS.Units = { { "usa_command_center", Base, Facing }, { "usa_dozer", Offset(Base, P == 0 ? -30 : 30, 0), Facing } };
			S.Players.push_back(PS);
		}
		return S;
	}

	struct EconomyMatchResult
	{
		std::vector<uint64_t> Hashes;
		std::vector<uint8_t> Replay;
		TickNumber EndTick = 0;
		int32_t Winner = -1;
		TickNumber FirstTank[2] = { 0, 0 };
		TickNumber FirstShot = 0;
		int32_t Income[2] = { 0, 0 };
		int32_t Tanks[2] = { 0, 0 };
		int32_t Structures[2] = { 0, 0 };
		int32_t Defenses[2] = { 0, 0 };
		int32_t DefenseShots = 0;
		int32_t Drops = 0;
		/** Os centros de suprimentos prontos e o passo da última entrega de cada um (a expansão, 16/10). */
		int32_t Centers[2] = { 0, 0 };
		TickNumber LastIncome[2] = { 0, 0 };
		/** (v4) As fábricas prontas e o banco no fim (o dinheiro inicial maior vira fábrica). */
		int32_t Factories[2] = { 0, 0 };
		int32_t FinalMoney[2] = { 0, 0 };
		/** (v4) As fundações marcadas e as que morreram antes de ficar prontas (a IA recolocava sob fogo). */
		int32_t Placed[2] = { 0, 0 };
		int32_t LostSites[2] = { 0, 0 };
		double MsPerTick = 0;
	};

	/** WaveSize > 0 troca o tamanho da onda dos robôs (999 = nunca atacam: só a economia). */
	EconomyMatchResult RunEconomyMatch(const MatchSetup& Setup, TickNumber MaxTicks, int32_t WaveSize = 0)
	{
		EconomyMatchResult R;
		World W;
		if (!W.Init(Setup))
		{
			Check(false, "montagem da partida com base");
			return R;
		}
		CommandScheduler Queue(1);
		std::vector<SkirmishAI> Brains = { SkirmishAI(0), SkirmishAI(1) };
		for (SkirmishAI& B : Brains)
		{
			B.WaveSize = WaveSize > 0 ? WaveSize : B.WaveSize;
		}
		ReplayWriter Rec;
		Rec.Begin(Setup);
		std::map<EntityId, int32_t> OpenSites;   // a fundação ainda inacabada → o dono
		const auto Begin = std::chrono::steady_clock::now();
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
				UnitView V;
				if (E.Type == EventType::UnitProduced && W.FindUnit(E.Target, V) && W.Rules().Units[V.Def].HasWeapon())
				{
					R.FirstTank[V.Owner] = R.FirstTank[V.Owner] == 0 ? T : R.FirstTank[V.Owner];
					++R.Tanks[V.Owner];
				}
				if (E.Type == EventType::StructureCompleted && W.FindUnit(E.Target, V))
				{
					++R.Structures[V.Owner];
					R.Defenses[V.Owner] += W.Rules().Units[V.Def].HasWeapon() ? 1 : 0;
					R.Centers[V.Owner] += W.Rules().Units[V.Def].bSupplyCenter ? 1 : 0;
					bool bFactory = false;
					for (const uint16_t K : W.Rules().Units[V.Def].Produces)
					{
						bFactory |= W.Rules().Units[K].HasWeapon();
					}
					R.Factories[V.Owner] += bFactory ? 1 : 0;
					OpenSites.erase(E.Target);
				}
				if (E.Type == EventType::StructurePlaced && W.FindUnit(E.Target, V))
				{
					++R.Placed[V.Owner];
					OpenSites[E.Target] = V.Owner;
				}
				if ((E.Type == EventType::UnitDied || E.Type == EventType::StructureCancelled) && OpenSites.count(E.Target) > 0)
				{
					R.LostSites[OpenSites[E.Target]] += E.Type == EventType::UnitDied ? 1 : 0;
					OpenSites.erase(E.Target);
				}
				if (E.Type == EventType::WeaponFired && W.FindUnit(E.Source, V) && W.Rules().Units[V.Def].IsStructure())
				{
					++R.DefenseShots;
				}
				if (E.Type == EventType::WeaponFired && R.FirstShot == 0)
				{
					R.FirstShot = T;
				}
				if (E.Type == EventType::SupplyDelivered && W.FindUnit(E.Source, V))
				{
					R.Income[V.Owner] += E.Value;
					R.LastIncome[V.Owner] = T;
				}
				R.Drops += E.Type == EventType::CargoDropped ? 1 : 0;
			}
		}
		const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Begin).count();
		R.MsPerTick = R.Hashes.empty() ? 0 : Ms / double(R.Hashes.size());
		R.EndTick = W.Tick();
		R.Winner = W.WinningTeam();
		R.FinalMoney[0] = W.Money(0);
		R.FinalMoney[1] = W.Money(1);
		R.Replay = Rec.Finish();
		return R;
	}

	void TestEconomyMatch()
	{
		std::printf("[partida com base: robo x robo]\n");
		const MatchSetup Setup = MakeEconomyMatch(21);
		const TickNumber MaxTicks = 20 * 60 * TicksPerSecond;
		const EconomyMatchResult A = RunEconomyMatch(Setup, MaxTicks);
		const double Minutes = double(A.EndTick) / (60.0 * TicksPerSecond);
		std::printf("  %s em %.1f min de jogo (vencedor time %d), %.3f ms por passo\n", A.Winner >= 0 ? "acabou" : "nao acabou", Minutes, A.Winner, A.MsPerTick);
		std::printf("  1o tanque: %.1f s e %.1f s; 1o tiro: %.1f s; predios prontos: %d e %d; tanques feitos: %d e %d\n",
			double(A.FirstTank[0]) / TicksPerSecond, double(A.FirstTank[1]) / TicksPerSecond, double(A.FirstShot) / TicksPerSecond,
			A.Structures[0], A.Structures[1], A.Tanks[0], A.Tanks[1]);
		std::printf("  renda media: %.0f e %.0f $/min; cargas caidas: %d\n", A.Income[0] / Minutes, A.Income[1] / Minutes, A.Drops);
		Check(A.Structures[0] >= 3 && A.Structures[1] >= 3, "os dois robos montaram usina, centro e fabrica");
		Check(A.FirstTank[0] > 0 && A.FirstTank[1] > 0, "os dois produziram tanque");
		Check(A.Income[0] > 0 && A.Income[1] > 0, "os caminhoes dos dois trouxeram dinheiro");
		Check(A.FirstShot > 0, "a guerra aconteceu");

		const EconomyMatchResult B = RunEconomyMatch(Setup, MaxTicks);
		Check(A.Hashes == B.Hashes, "a partida com base roda igual duas vezes, passo a passo");
		ReplayReader Reader;
		const bool bLoaded = Reader.Load(A.Replay);
		Check(bLoaded, "o replay da partida com base carrega");
		if (bLoaded)
		{
			World W;
			W.Init(Reader.Setup());
			int32_t Mismatch = -1;
			for (TickNumber T = 0; T < Reader.LastTick() + 1 && !W.IsMatchOver(); ++T)
			{
				W.Step(Reader.CommandsFor(T));
				uint64_t Want = 0;
				if (Reader.ExpectedHash(T, Want) && Want != W.StateHash() && Mismatch < 0)
				{
					Mismatch = int32_t(T);
				}
			}
			Check(Mismatch < 0, "o replay da partida com base reproduz todas as impressoes");
		}
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

	// o tiro forçado do C&C (Ctrl, 13/10): no chão e na própria unidade
	void TestForceFire()
	{
		std::printf("tiro forcado (Ctrl)\n");
		MatchSetup S = TwoTankSetup(Cell{ 10, 10 }, Cell{ 60, 60 }, false);
		StartingUnit Mate = S.Players[0].Units[0];
		Mate.Pos = S.Map.CellCenter(Cell{ 14, 10 });
		S.Players[0].Units.push_back(Mate);
		World W;
		W.Init(S);
		UnitView A, B;
		Check(W.FindUnit(1, A) && W.FindUnit(2, B) && A.Owner == 0 && B.Owner == 0, "os dois tanques sao do jogador 0");

		// sem o Ctrl, atacar o colega é ignorado
		StepWith(W, { Group(0, { 1 }), Order(0, CommandType::Attack, 2) });
		W.FindUnit(1, A);
		Check(A.Order == OrderType::Idle, "atacar o proprio tanque sem o Ctrl e ignorado");

		// os comandos novos viajam em bytes
		{
			Command G = Order(0, CommandType::AttackGround, NoEntity, 0, S.Map.CellCenter(Cell{ 10, 20 }));
			G.Tick = 5;
			ByteWriter BW;
			WriteCommand(BW, G);
			WriteCommand(BW, Order(0, CommandType::ForceAttack, 2));
			ByteReader BR(BW.Bytes.data(), BW.Bytes.size());
			Command G2, F2;
			Check(ReadCommand(BR, G2) && G2.Type == CommandType::AttackGround && G2.Point == G.Point && G2.Tick == 5, "AttackGround ida e volta em bytes");
			Check(ReadCommand(BR, F2) && F2.Type == CommandType::ForceAttack && F2.Target == 2, "ForceAttack ida e volta em bytes");
		}

		// com o Ctrl, o colega leva tiro
		StepWith(W, { Order(0, CommandType::ForceAttack, 2) });
		const int32_t Ticks = RunUntil(W, 15 * TicksPerSecond, [&]() { UnitView V; return W.FindUnit(2, V) && V.Hp < V.MaxHp; });
		W.FindUnit(2, B);
		std::printf("  o colega levou o 1o tiro em %d passos (vida %d de %d)\n", Ticks, B.Hp, B.MaxHp);
		Check(B.Hp < B.MaxHp, "o ataque forcado fere o proprio tanque");

		// no chão: vai até o alcance, atira no ponto, e o tiro não fere ninguém
		const Vec2 Spot = S.Map.CellCenter(Cell{ 10, 40 });
		StepWith(W, { Order(0, CommandType::AttackGround, NoEntity, 0, Spot) });
		const int32_t HpB = B.Hp;
		int32_t GroundHits = 0, Damaged = 0;
		for (int32_t T = 0; T < 25 * TicksPerSecond; ++T)
		{
			StepWith(W, {});
			for (const SimEvent& E : W.LastEvents())
			{
				GroundHits += (E.Type == EventType::ProjectileHit && E.Target == NoEntity && E.Point == Spot) ? 1 : 0;
				Damaged += E.Type == EventType::UnitDamaged ? 1 : 0;
			}
		}
		W.FindUnit(1, A);
		W.FindUnit(2, B);
		std::printf("  tiros no chao: %d, unidades feridas: %d, o tanque ficou a %.1f m do ponto\n", GroundHits, Damaged, Distance(A.Pos, Spot).ToDouble());
		Check(GroundHits >= 3, "o tiro no chao chega no ponto, varias vezes");
		Check(Damaged == 0 && B.Hp == HpB, "o tiro no chao nao fere ninguem");
		Check(A.Order == OrderType::AttackGround, "a ordem de atirar no chao continua ate mandarem outra coisa");
		Check(W.VisionSelfCheck(), "a visao confere depois do tiro forcado");
	}

}

namespace
{
	// ------------------------------------------------------------------ a água do WorldForge (13/10)

	void TestWaterGrid()
	{
		std::printf("[agua]\n");
		SimMap M = MakeOpenMap(32, 32);
		M.ComputeClearance();
		const uint64_t Dry = M.Hash();
		// um lago em X = 12..19: margem nas bordas, vau no meio de cima, fundo no resto; o baker marca só o fundo como água
		M.Water.assign(M.Pass.size(), uint8_t(WaterState::Dry));
		M.WaterBodyId.assign(M.Pass.size(), 0);
		M.WaterBodies.push_back(WaterBody{ 1, 0, -1000, 0, 0 });
		for (int32_t Y = 0; Y < 32; ++Y)
		{
			for (int32_t X = 12; X < 20; ++X)
			{
				const WaterState W = (X == 12 || X == 19) ? WaterState::Shore : (Y >= 26 ? WaterState::Ford : WaterState::Deep);
				M.Water[size_t(M.Index(X, Y))] = uint8_t(W);
				M.WaterBodyId[size_t(M.Index(X, Y))] = 1;
				M.Pass[size_t(M.Index(X, Y))] = uint8_t(W == WaterState::Deep ? CellPass::Water : CellPass::Open);
			}
		}
		M.ComputeClearance();
		Check(M.Hash() != Dry, "a água entra no hash do mapa");
		Check(M.WaterAt(15, 28) == WaterState::Ford && M.WaterAt(15, 5) == WaterState::Deep && M.WaterAt(3, 3) == WaterState::Dry, "a água por célula");
		// o caminho de um lado ao outro passa pelo vau (o fundo bloqueia)
		Pathfinder P;
		std::vector<Vec2> Path;
		const bool bFound = P.FindPath(M, M.CellCenter(Cell{ 4, 10 }), M.CellCenter(Cell{ 27, 10 }), 1, Path);
		bool bDeep = false;
		int32_t MaxY = 0;
		for (const Vec2& Wp : Path)
		{
			const Cell C = M.CellOf(Wp);
			bDeep |= M.WaterAt(C.X, C.Y) == WaterState::Deep;
			MaxY = C.Y > MaxY ? C.Y : MaxY;
		}
		Check(bFound && !bDeep, "o caminho não entra no fundo");
		Check(bFound && MaxY >= 25, "o caminho atravessa pelo vau");
		// o mesmo mapa de novo dá o mesmo hash (a grade é determinística)
		SimMap N = M;
		Check(N.Hash() == M.Hash(), "a mesma água, o mesmo hash");
		N.WaterBodies[0].LevelMm = -900;
		Check(N.Hash() != M.Hash(), "o nível do corpo entra no hash");
	}
}

namespace
{
	// ------------------------------------------------------------------ render-se e a queda (13/10, N5 da rede)

	void TestResign()
	{
		std::printf("[render-se]\n");
		MatchSetup S = TwoTankSetup(Cell{ 10, 10 }, Cell{ 60, 60 }, false);
		World W;
		W.Init(S);
		{
			ByteWriter BW;
			WriteCommand(BW, Order(1, CommandType::Resign, NoEntity));
			ByteReader BR(BW.Bytes.data(), BW.Bytes.size());
			Command R2;
			Check(ReadCommand(BR, R2) && R2.Type == CommandType::Resign && R2.Player == 1, "Resign ida e volta em bytes");
		}
		StepWith(W, {});
		Check(!W.IsMatchOver(), "a partida segue antes do render-se");
		StepWith(W, { Order(1, CommandType::Resign, NoEntity) });
		RunUntil(W, 2 * TicksPerSecond, [&]() { return W.IsMatchOver(); });
		Check(W.IsMatchOver() && W.WinningTeam() == int32_t(W.TeamOf(0)), "quem se rende perde, e o outro ganha");
		UnitView V;
		Check(!W.FindUnit(2, V), "o tanque de quem se rendeu cai");
	}
}

namespace
{
	// ------------------------------------------------------------------ a carcaça com física (13/10)

	void TestWreckPush()
	{
		std::printf("[carcaca]\n");
		// o colega (tanque 2, do jogador 0) morre pelo tiro forçado do tanque 1 e vira carcaça no caminho dele
		MatchSetup S = TwoTankSetup(Cell{ 10, 30 }, Cell{ 60, 60 }, false);
		StartingUnit Mate = S.Players[0].Units[0];
		Mate.Pos = S.Map.CellCenter(Cell{ 20, 30 });
		S.Players[0].Units.push_back(Mate);
		World W;
		W.Init(S);
		const Vec2 Start = Mate.Pos;
		StepWith(W, { Group(0, { 1 }), Order(0, CommandType::ForceAttack, 2) });
		RunUntil(W, 40 * TicksPerSecond, [&]() { return W.WreckCount() > 0; });
		Check(W.WreckCount() == 1, "o tanque morto vira carcaça");
		if (W.WreckCount() != 1)
		{
			return;
		}
		const World::WreckView W0 = W.Wreck(0);
		Check(W0.Id == 2 && DistSqRaw(W0.Pos, Start) < RadiusSqRaw(Fix::FromInt(1)), "a carcaça fica onde ele morreu, com o id dele");
		// o tanque 1 passa pela carcaça: ela é empurrada (e o tanque segue)
		const Vec2 Goal = S.Map.CellCenter(Cell{ 34, 30 });
		StepWith(W, { Group(0, { 1 }), Order(0, CommandType::Move, NoEntity, 0, Goal) });
		RunUntil(W, 30 * TicksPerSecond, [&]() { UnitView A; return W.FindUnit(1, A) && DistSqRaw(A.Pos, Goal) < RadiusSqRaw(Fix::FromInt(3)); });
		UnitView A;
		W.FindUnit(1, A);
		const World::WreckView W1 = W.Wreck(0);
		const double Moved = std::sqrt(double(DistSqRaw(W1.Pos, W0.Pos))) / double(Fix::OneRaw);
		std::printf("  a carcaca andou %.1f m e girou %.0f graus; o tanque chegou a %.1f m do ponto\n", Moved,
			std::abs(double(int16_t(uint16_t(W1.Hull.Bam - W0.Hull.Bam)))) * 360.0 / 65536.0,
			std::sqrt(double(DistSqRaw(A.Pos, Goal))) / double(Fix::OneRaw));
		Check(Moved > 1.0, "o tanque empurra a carcaca");
		Check(Moved < 20.0, "a carcaca escorrega pro lado (nao vai na frente do tanque o caminho inteiro)");
		Check(DistSqRaw(A.Pos, Goal) < RadiusSqRaw(Fix::FromInt(3)), "o tanque passa (a carcaca nao o prende pra sempre)");
	}
}

namespace
{
	// ------------------------------------------------------------------ a carcaça some (22/10)

	/** Os eventos da carcaça somados passo a passo (e quantos vieram pelos tetos). */
	struct WreckEventCount
	{
		int32_t Sinking = 0;
		int32_t SinkingByCap = 0;
		int32_t Removed = 0;
		int32_t RemovedByCap = 0;

		void Add(const World& W)
		{
			for (const SimEvent& E : W.LastEvents())
			{
				if (E.Type == EventType::WreckSinking)
				{
					++Sinking;
					SinkingByCap += (E.Value >> 16) == 1 ? 1 : 0;
				}
				else if (E.Type == EventType::WreckRemoved)
				{
					++Removed;
					RemovedByCap += E.Value == 1 ? 1 : 0;
				}
			}
		}
	};

	void TestWreckLife()
	{
		std::printf("[carcaca vida]\n");
		// a luta do [carcaca]: o colega morre pelo tiro forçado e vira carcaça; ela vive 48 s sólida, afunda 7 s e sai
		MatchSetup S = TwoTankSetup(Cell{ 10, 30 }, Cell{ 60, 60 }, false);
		StartingUnit Mate = S.Players[0].Units[0];
		Mate.Pos = S.Map.CellCenter(Cell{ 20, 30 });
		S.Players[0].Units.push_back(Mate);
		World W;
		W.Init(S);
		StepWith(W, { Group(0, { 1 }), Order(0, CommandType::ForceAttack, 2) });
		RunUntil(W, 40 * TicksPerSecond, [&]() { return W.WreckCount() > 0; });
		if (W.WreckCount() != 1)
		{
			Check(false, "o tanque morto vira carcaca (vida)");
			return;
		}
		const World::WreckView Born = W.Wreck(0);
		Check(Born.Born + 1 == W.Tick() && Born.SinkStart == 0 && Born.SinkTicks == 7 * TicksPerSecond, "a carcaca nasce solida no passo da morte, com 7 s de afundar");
		WreckEventCount N;
		// anda até o passo AtTick da vida dela (inclusive)
		const auto RunTo = [&](int32_t AtTick)
		{
			while (W.Tick() <= Born.Born + TickNumber(AtTick))
			{
				StepWith(W, {});
				N.Add(W);
			}
		};
		RunTo(47 * TicksPerSecond);
		Check(W.WreckCount() == 1 && W.Wreck(0).SinkStart == 0 && N.Sinking == 0, "aos 47 s ela ainda esta solida");
		RunTo(48 * TicksPerSecond);
		Check(W.WreckCount() == 1 && W.Wreck(0).SinkStart == Born.Born + TickNumber(48 * TicksPerSecond) && N.Sinking == 1 && N.SinkingByCap == 0,
			"aos 48 s ela comeca a afundar (pela vida)");
		// afundando, o tanque 1 passa por cima: ela não sai do lugar
		const Vec2 SinkPos = W.Wreck(0).Pos;
		const Vec2 Goal = S.Map.CellCenter(Cell{ 34, 30 });
		StepWith(W, { Group(0, { 1 }), Order(0, CommandType::Move, NoEntity, 0, Goal) });
		N.Add(W);
		RunTo(55 * TicksPerSecond - 1);
		Check(W.WreckCount() == 1 && W.Wreck(0).Pos == SinkPos, "afundando, ela nao sai do lugar");
		Check(N.Removed == 0, "ate o fim do afundar ela nao sai");
		RunTo(55 * TicksPerSecond);
		Check(W.WreckCount() == 0 && N.Removed == 1 && N.RemovedByCap == 0, "aos 55 s ela sai do mapa, com um evento so");
		UnitView A;
		W.FindUnit(1, A);
		std::printf("  o tanque que passou pela carcaca afundando ficou a %.1f m do ponto\n", std::sqrt(double(DistSqRaw(A.Pos, Goal))) / double(Fix::OneRaw));
	}

	void TestWreckCaps()
	{
		std::printf("[carcaca teto]\n");
		// três exércitos aliados de 30 tanques caem de uma vez (render-se), a 13 s um do outro: passou de 40 sólidas, as mais
		// velhas (de 12 s ou mais) afundam; passou de 64 no total, a mais velha sai na hora
		MatchSetup S;
		S.Map = MakeOpenMap(128, 128);
		S.Rules.Units.push_back(UnitDef::FromSpec(TankSpec()));
		S.Rules.Armor.Set(ArmorType::Tank, DamageType::ArmorPiercing, 100);
		S.bFogOfWar = false;
		const auto Tank = [&](Cell C)
		{
			StartingUnit U0;
			U0.Key = "usa_mbt";
			U0.Pos = S.Map.CellCenter(C);
			return U0;
		};
		PlayerSetup Watcher;   // longe de todos (fora da visão), só pra partida seguir
		Watcher.Units = { Tank(Cell{ 4, 4 }) };
		S.Players.push_back(Watcher);
		for (int32_t P = 0; P < 3; ++P)
		{
			PlayerSetup Army;
			Army.Team = 1;
			for (int32_t K = 0; K < 30; ++K)
			{
				Army.Units.push_back(Tank(Cell{ 40 + 20 * P + 3 * (K % 5), 60 + 3 * (K / 5) }));
			}
			S.Players.push_back(Army);
		}
		World W;
		Check(W.Init(S), "montagem dos tres exercitos");
		WreckEventCount N;
		int32_t MaxCount = 0;
		bool bOldestFirst = true;
		const auto Step1 = [&](std::vector<Command> Commands)
		{
			StepWith(W, std::move(Commands));
			N.Add(W);
			MaxCount = std::max(MaxCount, W.WreckCount());
			// a mais velha afunda primeiro: na ordem de nascimento, nenhuma sólida antes de uma afundando
			bool bSeenSolid = false;
			for (int32_t K = 0; K < W.WreckCount(); ++K)
			{
				if (W.Wreck(K).SinkStart == 0)
				{
					bSeenSolid = true;
				}
				else if (bSeenSolid)
				{
					bOldestFirst = false;
				}
			}
		};
		const auto RunFor = [&](int32_t Ticks)
		{
			for (int32_t T = 0; T < Ticks; ++T)
			{
				Step1({});
			}
		};
		Step1({ Order(1, CommandType::Resign, NoEntity) });
		Check(W.WreckCount() == 30, "o primeiro exercito vira 30 carcacas");
		RunFor(13 * TicksPerSecond);
		Step1({ Order(2, CommandType::Resign, NoEntity) });
		Step1({});
		Check(W.WreckCount() == 60 && N.SinkingByCap == 20, "60 solidas: as 20 mais velhas afundam (o teto de 40)");
		RunFor(13 * TicksPerSecond);
		Step1({ Order(3, CommandType::Resign, NoEntity) });
		Check(W.WreckCount() == 64 && N.RemovedByCap > 0, "passou de 64: as mais velhas saem na hora");
		RunFor(70 * TicksPerSecond);
		std::printf("  no maximo %d carcacas; %d afundaram pelo teto de solidas, %d sairam pelo teto de todas, %d no total\n",
			MaxCount, N.SinkingByCap, N.RemovedByCap, N.Removed);
		Check(MaxCount <= 64, "nunca mais de 64 carcacas");
		Check(bOldestFirst, "a mais velha afunda primeiro");
		Check(W.WreckCount() == 0 && N.Removed == 90, "no fim as 90 sairam, cada uma com o seu evento");
	}
}

namespace
{
	// ------------------------------------------------------------------ a defesa de base (14/10)

	void TestAIExpansion()
	{
		std::printf("[a IA expande quando a base seca]\n");
		// os depósitos da base pequenos (secam em poucos minutos); os do meio, grandes, a ~450 m de cada base; sem guerra
		// (a onda nunca junta), pra medir só a economia
		MatchSetup S = MakeEconomyMatch(21);
		S.Docks[0].Boxes = 60;
		S.Docks[1].Boxes = 60;
		const EconomyMatchResult A = RunEconomyMatch(S, 20 * 60 * TicksPerSecond, 999);
		const double Minutes = double(A.EndTick) / (60.0 * TicksPerSecond);
		std::printf("  %s em %.1f min (vencedor time %d); centros prontos %d e %d; renda %d e %d; ultima entrega aos %.1f e %.1f min; tanques %d e %d\n",
			A.Winner >= 0 ? "acabou" : "nao acabou", Minutes, A.Winner, A.Centers[0], A.Centers[1], A.Income[0], A.Income[1],
			double(A.LastIncome[0]) / (60.0 * TicksPerSecond), double(A.LastIncome[1]) / (60.0 * TicksPerSecond), A.Tanks[0], A.Tanks[1]);
		Check(A.Centers[0] >= 2 && A.Centers[1] >= 2, "com a base seca, os dois robos montam um centro novo no deposito do meio");
		const TickNumber Late = TickNumber(6 * 60 * TicksPerSecond);
		Check(A.LastIncome[0] > Late && A.LastIncome[1] > Late, "a renda continua depois que os depositos da base secam");
		const EconomyMatchResult B = RunEconomyMatch(S, 20 * 60 * TicksPerSecond, 999);
		Check(A.Hashes == B.Hashes, "a partida com a expansao roda igual duas vezes, passo a passo");
	}

	/** (v4, 22/10) O dinheiro inicial da sala (10 000 a 50 000): vai na montagem (as regras), todos começam com ele, e a
	 *  IA usa o banco grande (mais fábricas) sem mudar nada com os 10 000 de sempre. */
	void TestStartingMoney()
	{
		std::printf("[dinheiro inicial]\n");
		const MatchSetup S10 = MakeEconomyMatch(21);
		MatchSetup S50 = MakeEconomyMatch(21);
		S50.Rules.Economy.StartingMoney = 50000;
		World W;
		Check(W.Init(S50), "montagem com 50 000");
		Check(W.Money(0) == 50000 && W.Money(1) == 50000, "os dois jogadores comecam com 50 000");
		ByteWriter BW;
		WriteSetup(BW, S50);
		MatchSetup Back;
		ByteReader BR(BW.Bytes.data(), BW.Bytes.size());
		Check(ReadSetup(BR, Back) && Back.Rules.Economy.StartingMoney == 50000 && Back.Hash() == S50.Hash(), "o dinheiro inicial volta igual da gravacao (valor e impressao)");
		Check(S50.Hash() != S10.Hash(), "o dinheiro inicial entra na impressao da montagem");

		// a IA, sem guerra (a onda nunca junta), 8 min: com 50 000 o banco vira fábricas; com 10 000, 1 fábrica como antes
		const TickNumber Eight = TickNumber(8 * 60 * TicksPerSecond);
		const EconomyMatchResult Big = RunEconomyMatch(S50, Eight, 999);
		const EconomyMatchResult Small = RunEconomyMatch(S10, Eight, 999);
		std::printf("  50 000: fabricas %d e %d, predios %d e %d, tanques %d e %d, banco no fim %d e %d (impressao %016llx)\n",
			Big.Factories[0], Big.Factories[1], Big.Structures[0], Big.Structures[1], Big.Tanks[0], Big.Tanks[1], Big.FinalMoney[0], Big.FinalMoney[1],
			(unsigned long long)(Big.Hashes.empty() ? 0 : Big.Hashes.back()));
		std::printf("  10 000: fabricas %d e %d, predios %d e %d, tanques %d e %d, banco no fim %d e %d (impressao %016llx)\n",
			Small.Factories[0], Small.Factories[1], Small.Structures[0], Small.Structures[1], Small.Tanks[0], Small.Tanks[1], Small.FinalMoney[0], Small.FinalMoney[1],
			(unsigned long long)(Small.Hashes.empty() ? 0 : Small.Hashes.back()));
		Check(Big.Factories[0] >= 3 && Big.Factories[1] >= 3, "com 50 000 a IA monta pelo menos 3 fabricas");
		Check(Big.FinalMoney[0] < 25000 && Big.FinalMoney[1] < 25000, "com 50 000 a IA gasta o banco (menos da metade no fim)");
		Check(Small.Factories[0] == 1 && Small.Factories[1] == 1, "com 10 000 a IA fica com 1 fabrica, como antes");

		// com guerra e a defesa (a partida D do mapeamento de 22/10): a fundação de 1 PV morria sob fogo e ela recolocava a
		// cada segundo (9 Patriots em 20 s)
		MatchSetup War = S50;
		War.Rules = EconomyRuleset(true);
		War.Rules.Economy.StartingMoney = 50000;
		const EconomyMatchResult D = RunEconomyMatch(War, TickNumber(15 * 60 * TicksPerSecond));
		std::printf("  50 000 com guerra: %s em %.1f min (vencedor time %d); fundacoes %d e %d, perdidas inacabadas %d e %d; fabricas %d e %d; defesas %d e %d; tanques %d e %d\n",
			D.Winner >= 0 ? "acabou" : "nao acabou", double(D.EndTick) / (60.0 * TicksPerSecond), D.Winner, D.Placed[0], D.Placed[1], D.LostSites[0], D.LostSites[1],
			D.Factories[0], D.Factories[1], D.Defenses[0], D.Defenses[1], D.Tanks[0], D.Tanks[1]);
		// antes do conserto: 6 e 3 perdidas, e só 1 fábrica cada
		Check(D.LostSites[0] <= 2 && D.LostSites[1] <= 2, "a IA nao recoloca fundacao sob fogo (no maximo 2 perdidas inacabadas)");
		Check(D.Factories[0] >= 3 && D.Factories[1] >= 3 && D.FirstShot > 0, "com 50 000 e guerra, a IA ainda monta as fabricas e briga");
	}

	/** (16/10) O esmagar e o derrubar (docs/ESMAGAR_E_DERRUBAR.md). */
	struct CrushRun
	{
		uint8_t CarState = 0;
		int32_t CrushEvents = 0;
		bool bArrived = false;
		Fix MinSpeedCrushing;
		Fix MaxSpeed;
		bool bEnteredCar = false;      // o caminhão: passou por dentro do carro de pé
		uint8_t TreeState = 0;
		Angle TreeFall;
		Fix TreeFallLength;
		std::vector<uint64_t> Hashes;
	};

	CrushRun RunCrush(const char* Key, bool bTree, bool bWall)
	{
		MatchSetup S;
		S.Seed = 3;
		S.Map = MakeOpenMap(64, 64);
		S.Rules = EconomyRuleset();
		S.bFogOfWar = false;
		const Vec2 Mid = S.Map.CellCenter(Cell{ 32, 32 });
		PropSetup P;
		if (bTree)
		{
			P.Kind = PropKind::Tree;
			P.Pos = Mid;
			P.HalfLength = Fix::FromMilli(400);
			P.HalfWidth = Fix::FromMilli(400);
			P.Height = Fix::FromInt(10);
		}
		else
		{
			P.Kind = PropKind::Car;
			P.Pos = Mid;
			P.HalfLength = Fix::FromMilli(2300);
			P.HalfWidth = Fix::FromMilli(950);
		}
		S.Props.push_back(P);
		if (bWall)
		{
			// um muro de 3 células a 6 m na frente da árvore (a queda pra frente bate nele)
			for (int32_t Y = 31; Y <= 33; ++Y)
			{
				S.Map.Pass[size_t(S.Map.Index(34, Y))] = uint8_t(CellPass::Blocked);
			}
		}
		PlayerSetup P0;
		P0.Units.push_back({ Key, Offset(Mid, -40, bTree ? 1 : 0), Angle() });
		PlayerSetup P1;
		P1.Team = 1;
		P1.Units.push_back({ "usa_mbt", Offset(Mid, 100, 100), Angle() });
		S.Players = { P0, P1 };
		CrushRun Out;
		World W;
		if (!W.Init(S))
		{
			Check(false, "montagem do esmagar");
			return Out;
		}
		const EntityId Me = W.Unit(0).Id;
		const Vec2 Goal = bWall ? Offset(Mid, -2, 1) : Offset(Mid, 40, bTree ? 1 : 0);   // com o muro: o tanque vai até a árvore (o muro fica além)
		StepWith(W, { Group(0, { Me }), Order(0, CommandType::Move, NoEntity, 0, Goal) });
		Out.MinSpeedCrushing = Fix::FromInt(1000);
		int32_t SlowWindow = 0;
		RunUntil(W, 30 * TicksPerSecond, [&]()
		{
			Out.Hashes.push_back(W.StateHash());
			for (const SimEvent& E : W.LastEvents())
			{
				if (E.Type == EventType::PropCrushed)
				{
					++Out.CrushEvents;
					SlowWindow = 15;
				}
			}
			UnitView V;
			if (!W.FindUnit(Me, V))
			{
				return true;
			}
			Out.MaxSpeed = Max(Out.MaxSpeed, V.Speed);
			if (SlowWindow > 0)
			{
				--SlowWindow;
				Out.MinSpeedCrushing = Min(Out.MinSpeedCrushing, V.Speed);
			}
			const World::PropView Pv = W.PropAt(0);
			if (!bTree && Pv.State == 0 && DistSqRaw(V.Pos, Mid) <= RadiusSqRaw(Fix::FromMilli(1500)))
			{
				Out.bEnteredCar = true;
			}
			Out.bArrived = DistSqRaw(V.Pos, Goal) <= RadiusSqRaw(Fix::FromInt(5));
			return Out.bArrived && V.Speed.Raw == 0;
		});
		const World::PropView Pv = W.PropAt(0);
		Out.CarState = bTree ? 0 : Pv.State;
		Out.TreeState = bTree ? Pv.State : 0;
		Out.TreeFall = Pv.FallDir;
		Out.TreeFallLength = Pv.FallLength;
		return Out;
	}

	void TestCrush()
	{
		std::printf("[esmagar e derrubar]\n");
		const CrushRun A = RunCrush("usa_mbt", false, false);
		const CrushRun B = RunCrush("usa_mbt", false, false);
		std::printf("  tanque x carro: estado %d, %d esmagadas, chegou %s, velocidade %.2f -> %.2f m/s esmagando\n", int(A.CarState), A.CrushEvents,
			A.bArrived ? "sim" : "nao", A.MaxSpeed.ToDouble() * TicksPerSecond, A.MinSpeedCrushing.ToDouble() * TicksPerSecond);
		Check(A.CarState == 3, "o tanque atravessa o carro e achata a frente e a traseira");
		Check(A.bArrived, "o tanque chega do outro lado do carro (passa por cima, nao contorna)");
		Check(A.MinSpeedCrushing < A.MaxSpeed && A.MinSpeedCrushing * 100 >= A.MaxSpeed * 80, "esmagando, o tanque freia de leve (o teto de 85%)");
		Check(A.Hashes == B.Hashes, "o esmagar roda igual duas vezes, passo a passo");
		const CrushRun T = RunCrush("usa_supply_truck", false, false);
		std::printf("  caminhao x carro: estado %d, chegou %s, entrou no carro %s\n", int(T.CarState), T.bArrived ? "sim" : "nao", T.bEnteredCar ? "sim" : "nao");
		Check(T.CarState == 0 && !T.bEnteredCar && T.bArrived, "o caminhao contorna o carro de pe e chega");
		const CrushRun Tr = RunCrush("usa_mbt", true, false);
		const int32_t Fwd = AbsInt(Angle().DeltaTo(Tr.TreeFall));
		std::printf("  tanque x palmeira: derrubada %d, cai a %.0f graus do rumo do tanque, %.1f m livres\n", int(Tr.TreeState), Fwd * 360.0 / 65536.0, Tr.TreeFallLength.ToDouble());
		Check(Tr.TreeState == 1 && Fwd <= 8192 && Tr.TreeFallLength == Fix::FromInt(10), "a palmeira cai pra frente do tanque, inteira");
		const CrushRun Tw = RunCrush("usa_mbt", true, true);
		const int32_t Off = AbsInt(Angle().DeltaTo(Tw.TreeFall));
		std::printf("  com o muro na frente: cai a %.0f graus do rumo, %.1f m livres\n", Off * 360.0 / 65536.0, Tw.TreeFallLength.ToDouble());
		Check(Tw.TreeState == 1 && Off >= 4096, "com prédio na frente, a palmeira desvia dele");
		// a montagem com os objetos vai e volta pelo replay (a rede manda a montagem inteira)
		MatchSetup S;
		S.Seed = 3;
		S.Map = MakeOpenMap(64, 64);
		S.Rules = EconomyRuleset();
		PropSetup P;
		P.Kind = PropKind::Tree;
		P.Pos = S.Map.CellCenter(Cell{ 10, 12 });
		P.Facing = Angle::FromBam(12345);
		P.HalfLength = Fix::FromMilli(400);
		P.Height = Fix::FromInt(9);
		S.Props.push_back(P);
		PlayerSetup P0;
		P0.Units.push_back({ "usa_mbt", S.Map.CellCenter(Cell{ 20, 20 }), Angle() });
		S.Players = { P0 };
		ByteWriter Wr;
		WriteSetup(Wr, S);
		ByteReader Rd(Wr.Bytes.data(), Wr.Bytes.size());
		MatchSetup Back;
		Check(ReadSetup(Rd, Back) && Back.Hash() == S.Hash() && Back.Props.size() == 1, "a montagem com os objetos vai e volta pelo replay");
	}

	struct AssistRun
	{
		int32_t Assists = 0;
		int32_t ShotsA = 0;
		int32_t ShotsB = 0;
		bool bTankDead = false;
		std::vector<uint64_t> Hashes;
	};

	AssistRun RunAssist()
	{
		MatchSetup S;
		S.Seed = 7;
		S.Map = MakeOpenMap(96, 96);
		S.Rules = EconomyRuleset(true);
		S.bFogOfWar = false;
		const Vec2 Base = S.Map.CellCenter(Cell{ 30, 30 });
		PlayerSetup P0;
		P0.Units.push_back({ "usa_power_plant", Offset(Base, -30, -30), Angle() });
		P0.Units.push_back({ "usa_power_plant", Offset(Base, -30, 0), Angle() });   // (as duas Patriot gastam 6)
		P0.Units.push_back({ "usa_patriot", Base, Angle() });
		P0.Units.push_back({ "usa_patriot", Offset(Base, 0, 24), Angle() });
		PlayerSetup P1;
		P1.Team = 1;
		P1.Units.push_back({ "usa_mbt", Offset(Base, 100, 4), Angle() });
		S.Players = { P0, P1 };
		AssistRun Out;
		World W;
		if (!W.Init(S))
		{
			Check(false, "montagem da ajuda entre as defesas");
			return Out;
		}
		EntityId A = NoEntity, B = NoEntity, Tank = NoEntity;
		for (int32_t I = 0; I < W.UnitCount(); ++I)
		{
			const UnitView V = W.Unit(I);
			const std::string& Key = W.Rules().Units[V.Def].Key;
			if (Key == "usa_patriot") { (A == NoEntity ? A : B) = V.Id; }
			if (Key == "usa_mbt") { Tank = V.Id; }
		}
		// o tanque vem pelo lado da A: ela vê primeiro e chama a B
		StepWith(W, { Group(1, { Tank }), Order(1, CommandType::Move, NoEntity, 0, Offset(Base, 30, 0)) });
		RunUntil(W, 40 * TicksPerSecond, [&]()
		{
			Out.Hashes.push_back(W.StateHash());
			for (const SimEvent& E : W.LastEvents())
			{
				Out.Assists += E.Type == EventType::DefenseAssist ? 1 : 0;
				if (E.Type == EventType::WeaponFired)
				{
					Out.ShotsA += E.Source == A ? 1 : 0;
					Out.ShotsB += E.Source == B ? 1 : 0;
				}
			}
			UnitView V;
			Out.bTankDead = !W.FindUnit(Tank, V);
			return Out.bTankDead;
		});
		return Out;
	}

	void TestDefenseAssist()
	{
		std::printf("[ajuda entre as defesas]\n");
		const AssistRun R = RunAssist();
		const AssistRun R2 = RunAssist();
		std::printf("  %d pedidos de ajuda, %d tiros da A e %d da B, tanque %s\n", R.Assists, R.ShotsA, R.ShotsB, R.bTankDead ? "morto" : "vivo");
		Check(R.Assists > 0, "a defesa que atira chama a vizinha do mesmo tipo (o AssistedTargeting do ZH)");
		Check(R.ShotsA > 0 && R.ShotsB > 0 && R.bTankDead, "as duas baterias atiram no mesmo tanque e ele cai");
		Check(R.Hashes == R2.Hashes, "a ajuda roda igual duas vezes, passo a passo");
	}

	void TestPatriot()
	{
		std::printf("[defesa de base]\n");
		for (int32_t Powered = 1; Powered >= 0; --Powered)
		{
			MatchSetup S;
			S.Seed = 5;
			S.Map = MakeOpenMap(96, 96);
			S.Rules = EconomyRuleset(true);
			S.bFogOfWar = false;
			const Vec2 Base = S.Map.CellCenter(Cell{ 30, 30 });
			PlayerSetup P0;
			if (Powered)
			{
				P0.Units.push_back({ "usa_power_plant", Offset(Base, -30, -30), Angle() });
			}
			P0.Units.push_back({ "usa_patriot", Base, Angle() });
			PlayerSetup P1;
			P1.Team = 1;
			P1.bIsAI = true;
			P1.Units.push_back({ "usa_mbt", Offset(Base, 90, 0), Angle() });
			S.Players = { P0, P1 };
			World W;
			Check(W.Init(S), Powered ? "montagem da defesa com energia" : "montagem da defesa sem energia");
			EntityId Patriot = NoEntity, Tank = NoEntity;
			for (int32_t I = 0; I < W.UnitCount(); ++I)
			{
				const UnitView V = W.Unit(I);
				if (W.Rules().Units[V.Def].Key == "usa_patriot") { Patriot = V.Id; }
				if (W.Rules().Units[V.Def].Key == "usa_mbt") { Tank = V.Id; }
			}
			UnitView P;
			if (!W.FindUnit(Patriot, P) || Tank == NoEntity)
			{
				Check(false, "a defesa e o tanque nasceram");
				continue;
			}
			const Vec2 Home = P.Pos;
			// o tanque vem atacar a defesa
			StepWith(W, { Group(1, { Tank }), Order(1, CommandType::Attack, Patriot) });
			int32_t Shots = 0, FirstShot = -1, ShotsInFirst3s = 0;
			// (16/10) o voo dos mísseis: quantos passos no ar, a maior altura acima da caixa, o rumo da saída e os acertos
			int32_t MissileHits = 0, Flights = 0, FlightTicks = 0, MinFlight = 1 << 30;
			Fix MaxClimb;
			std::vector<int32_t> LaunchYaw;
			std::map<uint32_t, std::pair<int32_t, Fix>> Air;   // Id → (passos, Z da saída)
			RunUntil(W, 40 * TicksPerSecond, [&]()
			{
				for (const SimEvent& E : W.LastEvents())
				{
					if (E.Type == EventType::WeaponFired && E.Source == Patriot)
					{
						++Shots;
						if (FirstShot < 0) { FirstShot = int32_t(W.Tick()); }
						if (int32_t(W.Tick()) - FirstShot < 3 * TicksPerSecond) { ++ShotsInFirst3s; }
					}
					if (E.Type == EventType::ProjectileHit && E.Source == Patriot)
					{
						MissileHits += E.Target == Tank ? 1 : 0;
						const auto It = Air.find(uint32_t(E.Value));
						if (It != Air.end())
						{
							++Flights;
							FlightTicks += It->second.first;
							MinFlight = It->second.first < MinFlight ? It->second.first : MinFlight;
							Air.erase(It);
						}
					}
				}
				for (int32_t K = 0; K < W.ProjectileCount(); ++K)
				{
					const World::ProjectileView Pv = W.ProjectileAt(K);
					if (!Pv.bMissile || Pv.Shooter != Patriot)
					{
						continue;
					}
					auto It = Air.find(Pv.Id);
					if (It == Air.end())
					{
						It = Air.emplace(Pv.Id, std::make_pair(0, Pv.Z)).first;
						if (LaunchYaw.size() < 4) { LaunchYaw.push_back(int32_t(Pv.Yaw.Bam)); }
					}
					++It->second.first;
					MaxClimb = Max(MaxClimb, Pv.Z - It->second.second);
				}
				UnitView T, PP;
				return !W.FindUnit(Tank, T) || !W.FindUnit(Patriot, PP);
			});
			UnitView T, PP;
			const bool bTankAlive = W.FindUnit(Tank, T);
			const bool bPatriotAlive = W.FindUnit(Patriot, PP);
			std::printf("  %s: %d tiros (%d nos 3 s da 1a rajada), tanque %s, defesa %s (%d de vida)\n", Powered ? "com energia" : "sem energia",
				Shots, ShotsInFirst3s, bTankAlive ? "vivo" : "morto", bPatriotAlive ? "viva" : "caiu", bPatriotAlive ? PP.Hp : 0);
			if (Powered)
			{
				std::printf("  missil: %d voos, %.2f s em media (o mais curto %.2f s), subiu ate %.1f m acima da caixa, %d acertos no tanque\n",
					Flights, Flights > 0 ? double(FlightTicks) / Flights / TicksPerSecond : 0.0, double(MinFlight) / TicksPerSecond, MaxClimb.ToDouble(), MissileHits);
				bool bFan = LaunchYaw.size() == 4;
				for (size_t K = 1; bFan && K < LaunchYaw.size(); ++K)
				{
					bFan = LaunchYaw[K] != LaunchYaw[K - 1];
				}
				Check(Flights > 0 && MinFlight >= 10, "o missil voa varios passos (nao chega no passo seguinte como o tiro antigo)");
				Check(MaxClimb > Fix::FromInt(1), "o missil sai inclinado e sobe antes de descer no tanque");
				Check(MissileHits > 0 && MissileHits * 2 >= Flights, "o missil explode no contato com o tanque (a maioria acerta)");
				Check(bFan, "as 4 da rajada saem das 4 caixas com rumos diferentes (o leque)");
			}
			if (Powered)
			{
				Check(Shots > 0 && !bTankAlive && bPatriotAlive, "com energia, a defesa derruba o tanque que vem atacar");
				Check(ShotsInFirst3s >= 4 && ShotsInFirst3s <= 8, "a rajada: 4 tiros, 2 s de recarga, mais 4");
				Check(bPatriotAlive && DistSqRaw(PP.Pos, Home) == 0, "a defesa não sai do lugar");
			}
			else
			{
				Check(Shots == 0, "sem energia, a defesa não atira (o POWERED do ZH)");
			}
		}
		// a IA monta a defesa na partida com base (robô x robô, com a Patriot no Dozer)
		MatchSetup M = MakeEconomyMatch(21);
		M.Rules = EconomyRuleset(true);
		const EconomyMatchResult A = RunEconomyMatch(M, 15 * 60 * TicksPerSecond);
		std::printf("  robo x robo com a defesa: centros %d e %d; %d e %d defesas prontas, %d tiros delas, vencedor time %d em %.1f min; tanques %d e %d; 1o tiro %.0f s\n",
			A.Centers[0], A.Centers[1], A.Defenses[0], A.Defenses[1], A.DefenseShots, A.Winner, double(A.EndTick) / (60.0 * TicksPerSecond), A.Tanks[0], A.Tanks[1], double(A.FirstShot) / TicksPerSecond);
		Check(A.Defenses[0] > 0 && A.Defenses[1] > 0, "os dois robos montam a defesa de base");
		Check(A.Structures[0] >= 4 && A.Structures[1] >= 4, "com a defesa, os robos ainda montam usina, centro e fabrica");
		Check(A.Tanks[0] > 0 && A.Tanks[1] > 0, "com a defesa, os robos ainda fazem tanque");
	}
}

namespace
{
	// ------------------------------------------------------------------ a patente do general (v4, 22/10)

	/** As regras da economia com a Patriot (o Crusader custa 900), sem névoa, num mapa aberto de 384 m; as unidades por conta
	 *  de quem chama. */
	MatchSetup RankSetup()
	{
		MatchSetup S;
		S.Seed = 11;
		S.Map = MakeOpenMap(96, 96);
		S.Rules = EconomyRuleset(true);
		S.bFogOfWar = false;
		PlayerSetup P0;
		PlayerSetup P1;
		P1.Team = 1;
		S.Players = { P0, P1 };
		return S;
	}

	/** As subidas de patente vistas: quantas vezes cada (jogador, patente) e o XP antes e depois do passo de cada uma. */
	struct RankLog
	{
		int32_t Count[MaxPlayers][MaxRank + 2] = {};
		bool bCrossedRight = true;   // em cada subida, o XP do passo anterior estava abaixo do limiar e o de agora, no limiar ou acima
		bool bBadValue = false;

		void Add(const World& W, const std::vector<int64_t>& Before)
		{
			for (const SimEvent& E : W.LastEvents())
			{
				if (E.Type != EventType::RankUp)
				{
					continue;
				}
				const int32_t P = E.Value & 0xFF;
				const int32_t R = E.Value >> 8;
				if (P >= W.PlayerCount() || R < 2 || R > MaxRank)
				{
					bBadValue = true;
					continue;
				}
				++Count[P][R];
				const int64_t Limit = int64_t(W.Rules().Economy.RankXp[R - 1]) * 1000;
				bCrossedRight &= Before[size_t(P)] < Limit && W.Player(PlayerIndex(P)).XpMilli >= Limit;
			}
		}
	};

	void TestRank()
	{
		std::printf("[patente]\n");
		// 1. um Crusader mata outro: 8 acertos de 60 em 480 PV; quem matou ganha 900 / 10 = 90 XP e o dono do morto 900 / 30 = 30
		{
			MatchSetup S = RankSetup();
			const Vec2 Mid = S.Map.CellCenter(Cell{ 48, 48 });
			S.Players[0].Units = { { "usa_mbt", Offset(Mid, -16, 0), Angle() } };
			S.Players[1].Units = { { "usa_mbt", Offset(Mid, 16, 0), Angle::FromBam(Angle::HalfTurn) } };
			World W;
			Check(W.Init(S), "montagem do duelo da patente");
			const uint16_t Mbt = uint16_t(W.Rules().FindUnit("usa_mbt"));
			const EntityId A = FindOf(W, 0, Mbt);
			const EntityId B = FindOf(W, 1, Mbt);
			// o alvo não revida: atira no chão atrás dele (o tiro no chão não fere ninguém)
			StepWith(W, { Group(1, { B }), Order(1, CommandType::AttackGround, NoEntity, 0, Offset(Mid, 26, 0)), Group(0, { A }), Order(0, CommandType::Attack, B) });
			int32_t Hits = 0;
			int32_t RankUps = 0;
			RunUntil(W, 60 * TicksPerSecond, [&]()
			{
				for (const SimEvent& E : W.LastEvents())
				{
					Hits += E.Type == EventType::UnitDamaged && E.Target == B ? 1 : 0;
					RankUps += E.Type == EventType::RankUp ? 1 : 0;
				}
				UnitView V;
				return !W.FindUnit(B, V);
			});
			RunUntil(W, 3 * TicksPerSecond, [&]() { return false; });   // e nada mais entra depois da morte
			std::printf("  1. Crusader x Crusader: %d acertos; XP em milesimos %lld (quem matou) e %lld (quem perdeu)\n", Hits,
				(long long)W.Player(0).XpMilli, (long long)W.Player(1).XpMilli);
			Check(Hits == 8, "1. o Crusader cai com 8 acertos de 60");
			Check(W.Player(0).XpMilli == 90000 && W.Xp(0) == 90, "1. quem matou o Crusader ganha exatamente 90 XP (90 000 milesimos)");
			Check(W.Player(1).XpMilli == 30000 && W.Xp(1) == 30, "1. o dono do Crusader morto ganha exatamente 30 XP (30 000 milesimos)");
			Check(RankUps == 0 && W.Rank(0) == 1 && W.Rank(1) == 1, "1. 90 XP nao sobem patente");
		}
		// 2a. a fundação (1 PV, obra) morta pelo tanque não vale nada
		{
			MatchSetup S = RankSetup();
			const Vec2 Mid = S.Map.CellCenter(Cell{ 48, 48 });
			S.Players[0].Units = { { "usa_mbt", Offset(Mid, -20, 0), Angle() } };
			S.Players[1].Units = { { "usa_dozer", Offset(Mid, 120, 0), Angle::FromBam(Angle::HalfTurn) } };
			World W;
			Check(W.Init(S), "montagem da fundacao da patente");
			const Ruleset& R = W.Rules();
			const uint16_t PP = uint16_t(R.FindUnit("usa_power_plant"));
			const EntityId Tank = FindOf(W, 0, uint16_t(R.FindUnit("usa_mbt")));
			const EntityId Dozer = FindOf(W, 1, uint16_t(R.FindUnit("usa_dozer")));
			StepWith(W, { Group(1, { Dozer }), Order(1, CommandType::Build, NoEntity, BuildValue(PP, 0), Offset(Mid, 10, 0)) });
			const EntityId Site = FindOf(W, 1, PP);
			UnitView SiteView;
			Check(Site != NoEntity && W.FindUnit(Site, SiteView) && SiteView.BuildLeft > 0 && SiteView.Hp == 1, "2. a fundacao nasce com 1 PV");
			StepWith(W, { Group(0, { Tank }), Order(0, CommandType::Attack, Site) });
			bool bKilled = false;
			RunUntil(W, 10 * TicksPerSecond, [&]()
			{
				for (const SimEvent& E : W.LastEvents())
				{
					bKilled |= E.Type == EventType::UnitDied && E.Target == Site;
				}
				return bKilled;
			});
			UnitView DozerView;
			Check(bKilled && W.FindUnit(Dozer, DozerView), "2. o tanque derruba a fundacao (e o Dozer segue longe)");
			Check(W.Player(0).XpMilli == 0 && W.Player(1).XpMilli == 0, "2. a fundacao derrubada nao da XP pra ninguem");
		}
		// 2b. o tiro forçado (Ctrl) no próprio tanque e no do aliado não vale nada
		{
			MatchSetup S = RankSetup();
			const Vec2 Mid = S.Map.CellCenter(Cell{ 48, 48 });
			PlayerSetup Ally;   // o jogador 2, do time do 0
			Ally.Team = 0;
			S.Players.push_back(Ally);
			S.Players[0].Units = { { "usa_mbt", Offset(Mid, -10, 0), Angle() }, { "usa_mbt", Offset(Mid, 10, 0), Angle() } };
			S.Players[2].Units = { { "usa_mbt", Offset(Mid, 0, 16), Angle() } };
			S.Players[1].Units = { { "usa_mbt", Offset(Mid, -150, -150), Angle() } };   // o inimigo longe de tudo (a partida segue)
			World W;
			Check(W.Init(S), "montagem do tiro forcado da patente");
			const uint16_t Mbt = uint16_t(W.Rules().FindUnit("usa_mbt"));
			EntityId Mine[2] = { NoEntity, NoEntity };
			for (int32_t I = 0, K = 0; I < W.UnitCount(); ++I)
			{
				const UnitView V = W.Unit(I);
				if (V.Owner == 0 && V.Def == Mbt && K < 2)
				{
					Mine[K++] = V.Id;
				}
			}
			const EntityId Friend = FindOf(W, 2, Mbt);
			int32_t Damaged = 0;
			const auto KillWithCtrl = [&](EntityId Victim)
			{
				StepWith(W, { Group(0, { Mine[0] }), Order(0, CommandType::ForceAttack, Victim) });
				RunUntil(W, 40 * TicksPerSecond, [&]()
				{
					for (const SimEvent& E : W.LastEvents())
					{
						Damaged += E.Type == EventType::UnitDamaged && E.Target == Victim ? 1 : 0;
					}
					UnitView V;
					return !W.FindUnit(Victim, V);
				});
			};
			KillWithCtrl(Mine[1]);
			KillWithCtrl(Friend);
			UnitView V1, V2;
			Check(Damaged >= 16 && !W.FindUnit(Mine[1], V1) && !W.FindUnit(Friend, V2), "2. o tiro forcado derruba o proprio tanque e o do aliado");
			Check(W.Player(0).XpMilli == 0 && W.Player(1).XpMilli == 0 && W.Player(2).XpMilli == 0, "2. o dano no proprio e no aliado nao da XP");
		}
		// 3. os limiares: um alvo de $100 000 com 6 000 PV vale 100 XP por acerto de 60 (e 33,3 pro dono): a 6ª, a 15ª, a 30ª
		// e a 50ª pancada sobem o atirador, uma vez cada; o dono do alvo sobe na 19ª, na 46ª e na 91ª
		{
			std::vector<UnitSpec> Specs = { TankSpec() };
			UnitSpec Bunker;
			Bunker.Key = "test_bunker";
			Bunker.Kind = ObjectKind::Structure;
			Bunker.Armor = ArmorType::Structure;
			Bunker.MaxHp = 6000;
			Bunker.SizeXMm = 8000;
			Bunker.SizeYMm = 8000;
			Bunker.Cost = 100000;
			Specs.push_back(Bunker);
			ArmorTable Armor;
			Armor.Set(ArmorType::Tank, DamageType::ArmorPiercing, 100);
			Armor.Set(ArmorType::Structure, DamageType::ArmorPiercing, 100);
			MatchSetup S;
			S.Seed = 3;
			S.Map = MakeOpenMap(64, 64);
			Check(Ruleset::Build(Specs, Armor, EconomyRules(), S.Rules), "as regras do alvo caro montam (a patente padrao)");
			S.bFogOfWar = false;
			const Vec2 Mid = S.Map.CellCenter(Cell{ 32, 32 });
			PlayerSetup P0;
			P0.Units = { { "usa_mbt", Offset(Mid, -20, 0), Angle() } };
			PlayerSetup P1;
			P1.Team = 1;
			P1.Units = { { "test_bunker", Offset(Mid, 10, 0), Angle() } };
			S.Players = { P0, P1 };
			World W;
			Check(W.Init(S), "montagem dos limiares da patente");
			const EntityId Target = FindOf(W, 1, 1);
			RankLog Log;
			int32_t Hits = 0;
			int32_t ShooterRankAt[MaxRank + 1] = {};
			for (int32_t T = 0; T < 300 * TicksPerSecond && !W.IsMatchOver(); ++T)
			{
				const std::vector<int64_t> Before = { W.Player(0).XpMilli, W.Player(1).XpMilli };
				StepWith(W, {});
				for (const SimEvent& E : W.LastEvents())
				{
					Hits += E.Type == EventType::UnitDamaged && E.Target == Target ? 1 : 0;
				}
				for (const SimEvent& E : W.LastEvents())
				{
					if (E.Type == EventType::RankUp && (E.Value & 0xFF) == 0 && (E.Value >> 8) <= MaxRank)
					{
						ShooterRankAt[E.Value >> 8] = Hits;
					}
				}
				Log.Add(W, Before);
			}
			std::printf("  3. alvo caro: %d acertos; o atirador subiu nas pancadas %d, %d, %d e %d; patentes no fim %d e %d (XP %d e %d)\n", Hits,
				ShooterRankAt[2], ShooterRankAt[3], ShooterRankAt[4], ShooterRankAt[5], W.Rank(0), W.Rank(1), W.Xp(0), W.Xp(1));
			Check(Hits == 100, "3. o alvo de 6 000 PV cai com 100 acertos");
			Check(Log.Count[0][2] == 1 && Log.Count[0][3] == 1 && Log.Count[0][4] == 1 && Log.Count[0][5] == 1 && !Log.bBadValue,
				"3. o atirador sobe uma vez em cada limiar (600, 1 500, 3 000 e 5 000), e nunca alem da 5");
			Check(ShooterRankAt[2] == 6 && ShooterRankAt[3] == 15 && ShooterRankAt[4] == 30 && ShooterRankAt[5] == 50, "3. as subidas vem na pancada exata de cada limiar");
			Check(Log.Count[1][2] == 1 && Log.Count[1][3] == 1 && Log.Count[1][4] == 1 && Log.Count[1][5] == 0, "3. o dono do alvo sobe pelo XP de quem apanha (ate a 4)");
			Check(Log.bCrossedRight, "3. cada subida vem no passo em que o XP cruza o limiar");
			Check(W.Rank(0) == 5 && W.Xp(0) == 10000, "3. no fim o atirador esta na 5 com 10 000 XP");
		}
		// 4. o Tomahawk exige a patente 3: na 2 a fábrica recusa (e não cobra); na 3 aceita
		for (const uint8_t StartRank : { uint8_t(2), uint8_t(3) })
		{
			MatchSetup S = RankSetup();
			const Vec2 Mid = S.Map.CellCenter(Cell{ 48, 48 });
			S.Players[0].Units = { { "usa_war_factory", Mid, Angle() } };
			S.Players[0].StartRank = StartRank;
			S.Players[1].Units = { { "usa_mbt", Offset(Mid, -150, -150), Angle() } };
			World W;
			Check(W.Init(S), "montagem da fabrica da patente");
			const Ruleset& R = W.Rules();
			const uint16_t Tom = uint16_t(R.FindUnit("usa_tomahawk"));
			const uint16_t Mbt = uint16_t(R.FindUnit("usa_mbt"));
			const EntityId Factory = FindOf(W, 0, uint16_t(R.FindUnit("usa_war_factory")));
			std::vector<uint16_t> Queue;
			const int32_t Money = W.Money(0);
			StepWith(W, { Order(0, CommandType::Produce, Factory, Tom) });
			W.QueueOf(Factory, Queue);
			if (StartRank == 2)
			{
				Check(!W.HasPrerequisites(0, Tom) && W.HasPrerequisites(0, Mbt), "4. na patente 2 o Tomahawk esta trancado (o Crusader nao)");
				Check(Queue.empty() && W.Money(0) == Money, "4. na patente 2 a fabrica recusa o Tomahawk e nao cobra");
				StepWith(W, { Order(0, CommandType::Produce, Factory, Mbt) });
				W.QueueOf(Factory, Queue);
				Check(Queue.size() == 1 && W.Money(0) == Money - 900, "4. na patente 2 o Crusader entra na fila");
			}
			else
			{
				Check(W.HasPrerequisites(0, Tom), "4. na patente 3 o Tomahawk abre");
				Check(Queue.size() == 1 && Queue[0] == Tom && W.Money(0) == Money - 1200, "4. na patente 3 a fabrica aceita o Tomahawk ($1 200)");
				RunUntil(W, 40 * TicksPerSecond, [&]() { return CountOf(W, 0, Tom) > 0; });
				Check(CountOf(W, 0, Tom) == 1, "4. o Tomahawk sai da fabrica");
			}
		}
		// 5. a patente de largada: a 2 começa com 600 XP; a montagem vai e volta pela gravação (os poderes, a patente, os campos
		// novos das unidades)
		{
			MatchSetup S = RankSetup();
			const Vec2 Mid = S.Map.CellCenter(Cell{ 48, 48 });
			S.Players[0].Units = { { "usa_command_center", Mid, Angle() } };
			S.Players[0].StartRank = 2;
			S.Players[1].Units = { { "usa_command_center", Offset(Mid, -120, -120), Angle() } };
			World W;
			Check(W.Init(S), "montagem da patente de largada");
			Check(W.Rank(0) == 2 && W.Xp(0) == 600 && W.Player(0).XpMilli == 600000, "5. a patente de largada 2 comeca na 2 com 600 XP");
			Check(W.Rank(1) == 1 && W.Xp(1) == 0, "5. sem patente de largada, a 1 com 0 XP");
			// os poderes do general: na 2 com o CC pronto, o ataque está pronto; na 1, não; poder que não existe, nunca
			Check(W.Rules().Powers.size() == 1 && W.IsPowerReady(0, 0) && !W.IsPowerReady(1, 0) && !W.IsPowerReady(0, 1) && W.PowerReadyAt(0, 0) == 0,
				"5. o poder fica pronto na patente 2 com o CC (e nao na 1)");
			ByteWriter BW;
			WriteSetup(BW, S);
			MatchSetup Back;
			ByteReader BR(BW.Bytes.data(), BW.Bytes.size());
			const bool bRead = ReadSetup(BR, Back) && BR.AtEnd();
			Check(bRead && Back.Hash() == S.Hash() && Back.Rules.Hash() == S.Rules.Hash(), "5. a montagem v4 volta igual da gravacao (impressao das regras e da montagem)");
			Check(bRead && Back.Players[0].StartRank == 2 && Back.Rules.Powers.size() == 1 && Back.Rules.Powers[0].Munition == S.Rules.Powers[0].Munition
				&& Back.Rules.Economy.RankXp[4] == 5000, "5. a patente de largada, o poder e a escada voltam da gravacao");
			const uint16_t Tom = uint16_t(S.Rules.FindUnit("usa_tomahawk"));
			const uint16_t Zone = uint16_t(S.Rules.FindUnit("usa_drop_zone"));
			Check(bRead && Back.Rules.Units[Tom].RequiresRank == 3 && Back.Rules.Units[Tom].Weapon.MinRange == Fix::FromInt(20)
				&& Back.Rules.Units[Tom].Weapon.SplashEdgePct == 50 && Back.Rules.Units[Zone].MaxPerPlayer == 2
				&& Back.Rules.Units[Zone].AirliftPeriodTicks == 90 * TicksPerSecond && Back.Rules.Units[Zone].AirliftUnloadTicks == 3 * TicksPerSecond,
				"5. os campos novos das unidades voltam da gravacao");
			MatchSetup Bad = S;
			Bad.Players[1].StartRank = 6;
			World WB;
			Check(!WB.Init(Bad), "5. patente de largada acima de 5 e montagem invalida");
			// o comando novo viaja em bytes (e o poder acima de 255 é recusado)
			Command Use = Order(0, CommandType::UsePower, NoEntity, 0, Vec2::Make(Fix::FromRatio(25, 2), Fix::FromRatio(-13, 4)));
			Use.Tick = 9;
			ByteWriter CW;
			WriteCommand(CW, Use);
			Command Bad1 = Use;
			Bad1.Value = 300;
			WriteCommand(CW, Bad1);
			ByteReader CR(CW.Bytes.data(), CW.Bytes.size());
			Command U1, U2;
			Check(ReadCommand(CR, U1) && U1.Type == CommandType::UsePower && U1.Value == 0 && U1.Point == Use.Point && U1.Tick == 9, "5. UsePower ida e volta em bytes");
			Check(!ReadCommand(CR, U2), "5. UsePower com o poder acima de 255 e recusado");
		}
		// 6. o míssil da Patriot credita o dono mesmo com a bateria já morta (o dono se rende com o míssil no ar)
		{
			MatchSetup S = RankSetup();
			const Vec2 Base = S.Map.CellCenter(Cell{ 40, 40 });
			S.Players[0].Units = { { "usa_power_plant", Offset(Base, -30, -30), Angle() }, { "usa_patriot", Base, Angle() } };
			S.Players[1].Units = { { "usa_mbt", Offset(Base, 90, 0), Angle() } };
			World W;
			Check(W.Init(S), "montagem da bateria da patente");
			const Ruleset& R = W.Rules();
			const EntityId Patriot = FindOf(W, 0, uint16_t(R.FindUnit("usa_patriot")));
			const EntityId Tank = FindOf(W, 1, uint16_t(R.FindUnit("usa_mbt")));
			StepWith(W, { Group(1, { Tank }), Order(1, CommandType::Attack, Patriot) });
			bool bFired = false;
			RunUntil(W, 30 * TicksPerSecond, [&]()
			{
				for (const SimEvent& E : W.LastEvents())
				{
					bFired |= E.Type == EventType::WeaponFired && E.Source == Patriot;
				}
				return bFired;
			});
			const int64_t Xp0 = W.Player(0).XpMilli;
			const int64_t Xp1 = W.Player(1).XpMilli;
			StepWith(W, { Order(0, CommandType::Resign, NoEntity) });
			UnitView PV;
			const bool bGone = !W.FindUnit(Patriot, PV);
			int32_t Late = 0;
			RunUntil(W, 5 * TicksPerSecond, [&]()
			{
				for (const SimEvent& E : W.LastEvents())
				{
					Late += E.Type == EventType::UnitDamaged && E.Target == Tank && E.Source == Patriot ? 1 : 0;
				}
				return false;
			});
			std::printf("  6. bateria caida com o missil no ar: %d acerto(s) depois; XP %lld -> %lld (dono) e %lld -> %lld (tanque)\n", Late,
				(long long)Xp0, (long long)W.Player(0).XpMilli, (long long)Xp1, (long long)W.Player(1).XpMilli);
			Check(bFired && bGone && Late >= 1, "6. a bateria cai com o missil no ar e ele ainda acerta o tanque");
			// 30 de explosão no Crusader: 30 × 900 × 1000 ÷ (480 × 10) = 5 625 pro dono da bateria; ÷ 30 = 1 875 pro do tanque
			Check(W.Player(0).XpMilli - Xp0 == int64_t(Late) * 5625 && W.Player(1).XpMilli - Xp1 == int64_t(Late) * 1875,
				"6. o missil da bateria morta credita o dono dela (Projectile.Owner)");
		}
	}
}

namespace
{
	// ------------------------------------------------------------------ v4 (22/10): o dano em área, o alcance mínimo, a zona de
	// lançamento, o Cruise Missile Strike e o cerco do Tomahawk (os números do spec da v4, que estão no EconomyRuleset)

	uint16_t DefKey(const World& W, const char* Key)
	{
		return uint16_t(W.Rules().FindUnit(Key));
	}

	/** A unidade do jogador P mais perto de Pos (a montagem põe cada uma no ponto exato: acha a que o teste plantou ali). */
	EntityId UnitNear(const World& W, PlayerIndex P, Vec2 Pos)
	{
		EntityId Best = NoEntity;
		int64_t BestD2 = 0;
		for (int32_t I = 0; I < W.UnitCount(); ++I)
		{
			const UnitView V = W.Unit(I);
			const int64_t D2 = DistSqRaw(V.Pos, Pos);
			if (V.Owner == P && (Best == NoEntity || D2 < BestD2))
			{
				Best = V.Id;
				BestD2 = D2;
			}
		}
		return Best;
	}

	/** O dano que cada alvo levou vindo de Source (os eventos UnitDamaged) e quantos acertos. */
	struct HitLog
	{
		std::map<EntityId, int32_t> Damage;
		std::map<EntityId, int32_t> Hits;

		void Add(const World& W, EntityId Source)
		{
			for (const SimEvent& E : W.LastEvents())
			{
				if (E.Type == EventType::UnitDamaged && E.Source == Source)
				{
					Damage[E.Target] += E.Value;
					Hits[E.Target] += 1;
				}
			}
		}
		int32_t Of(EntityId T) const
		{
			const auto It = Damage.find(T);
			return It == Damage.end() ? 0 : It->second;
		}
		int32_t HitsOn(EntityId T) const
		{
			const auto It = Hits.find(T);
			return It == Hits.end() ? 0 : It->second;
		}
	};

	/** A 1ª explosão de um tiro de Shooter (o ProjectileHit dele): o ponto, o alvo do contato, o passo e o dano desse passo. */
	struct FirstHit
	{
		bool bHit = false;
		Vec2 Point;
		EntityId Target = NoEntity;
		TickNumber Tick = 0;
		HitLog Log;
	};

	FirstHit RunToFirstHit(World& W, EntityId Shooter, int32_t MaxTicks)
	{
		FirstHit R;
		for (int32_t K = 0; K < MaxTicks && !R.bHit; ++K)
		{
			const TickNumber T = W.Tick();
			StepWith(W, {});
			for (const SimEvent& E : W.LastEvents())
			{
				if (E.Type == EventType::ProjectileHit && E.Source == Shooter && !R.bHit)
				{
					R.bHit = true;
					R.Point = E.Point;
					R.Target = E.Target;
					R.Tick = T;
				}
			}
			if (R.bHit)
			{
				R.Log.Add(W, Shooter);
			}
		}
		return R;
	}

	/** A queda do dano em área, com a mesma conta inteira da simulação (pra conferir o número exato). */
	int32_t SplashOf(int32_t Damage, Fix D, const WeaponDef& Wp)
	{
		if (D > Wp.Splash)
		{
			return 0;
		}
		const int64_t Pct = D <= Wp.SplashFull ? 100 : 100 - int64_t(100 - Wp.SplashEdgePct) * (D - Wp.SplashFull).Raw / (Wp.Splash - Wp.SplashFull).Raw;
		return int32_t(int64_t(Damage) * Pct / 100);
	}

	/** A distância do ponto até a borda do retângulo (a mesma do World::RectDistance). */
	Fix RectGapTo(Vec2 P, Vec2 C, Fix HX, Fix HY)
	{
		const Fix DX = Max(Abs(P.X - C.X) - HX, Fix::Zero());
		const Fix DY = Max(Abs(P.Y - C.Y) - HY, Fix::Zero());
		return Vec2::Make(DX, DY).Length();
	}

	/** Um lançador Tomahawk do jogador 0 no meio do mapa, virado pra +X; o inimigo 1 com um tanque longe de tudo (a partida
	 *  segue). Sem névoa. */
	MatchSetup LauncherSetup(Vec2& OutMid)
	{
		MatchSetup S = RankSetup();
		OutMid = S.Map.CellCenter(Cell{ 48, 48 });
		S.Players[0].Units = { { "usa_tomahawk", OutMid, Angle() } };
		S.Players[1].Units = { { "usa_mbt", Offset(OutMid, -150, -150), Angle() } };
		return S;
	}

	/** Monta e manda o lançador do jogador 0 atirar no chão em Ground (o Ctrl); anda até a 1ª explosão. */
	FirstHit FireAtGround(World& W, const MatchSetup& S, Vec2 Ground)
	{
		if (!W.Init(S))
		{
			Check(false, "montagem do tiro no chao");
			return FirstHit();
		}
		const EntityId Tom = FindOf(W, 0, DefKey(W, "usa_tomahawk"));
		StepWith(W, { Group(0, { Tom }), Order(0, CommandType::AttackGround, NoEntity, 0, Ground) });
		return RunToFirstHit(W, Tom, 10 * TicksPerSecond);
	}

	void TestArea()
	{
		std::printf("[area]\n");
		Vec2 Mid;
		const MatchSetup Base = LauncherSetup(Mid);
		const Vec2 Ground = Offset(Mid, 60, 0);
		// 7. o míssil no chão: primeiro sem ninguém perto, pra achar onde ele explode; depois um tanque inimigo de cada vez a
		// 0 / 2 / 3,5 / 5 / 5,1 m da borda do casco até esse ponto (o mesmo voo: o míssil sem alvo não olha os tanques)
		World Probe;
		const FirstHit P0 = FireAtGround(Probe, Base, Ground);
		std::printf("  7. o missil no chao explodiu a %.2f m do ponto, %.2f s depois da ordem\n", Distance(P0.Point, Ground).ToDouble(), double(P0.Tick) / TicksPerSecond);
		Check(P0.bHit && Distance(P0.Point, Ground) <= Fix::FromInt(2), "7. o missil do Tomahawk explode no ponto do tiro no chao");
		const WeaponDef& Wp = Probe.Rules().Units[DefKey(Probe, "usa_tomahawk")].Weapon;
		const int32_t EdgeMm[5] = { 0, 2000, 3500, 5000, 5100 };
		const int32_t Want[5] = { 110, 110, 82, 55, 0 };
		int32_t Got[5] = {};
		bool bSamePoint = true;
		bool bExact = true;
		for (int32_t K = 0; K < 5; ++K)
		{
			MatchSetup S = Base;
			const Vec2 At = P0.Point + Vec2::Make(Fix::Zero(), Fix::FromMilli(3600 + EdgeMm[K]));
			S.Players[1].Units.push_back({ "usa_mbt", At, Angle() });
			World W;
			const FirstHit H = FireAtGround(W, S, Ground);
			Got[K] = H.Log.Of(UnitNear(W, 1, At));
			bSamePoint &= H.bHit && H.Point == P0.Point;
			bExact &= Got[K] == Want[K];
		}
		std::printf("  7. tanques a 0 / 2 / 3,5 / 5 / 5,1 m da borda: %d / %d / %d / %d / %d de dano\n", Got[0], Got[1], Got[2], Got[3], Got[4]);
		Check(bSamePoint, "7. o mesmo voo com os tanques em volta (explode no mesmo ponto)");
		Check(bExact, "7. a queda exata: 110 / 110 / 82 / 55 / 0");

		// 8. o próprio tanque e o do aliado no meio da explosão não levam nada; o inimigo a 3 m da borda leva a área
		{
			MatchSetup S = Base;
			PlayerSetup Ally;
			Ally.Team = 0;
			S.Players.push_back(Ally);
			const Vec2 OwnAt = P0.Point + Vec2::Make(Fix::Zero(), Fix::FromMilli(3600));
			const Vec2 AllyAt = P0.Point - Vec2::Make(Fix::Zero(), Fix::FromMilli(4600));
			const Vec2 EnemyAt = P0.Point - Vec2::Make(Fix::FromMilli(6600), Fix::Zero());
			S.Players[0].Units.push_back({ "usa_mbt", OwnAt, Angle() });
			S.Players[2].Units.push_back({ "usa_mbt", AllyAt, Angle() });
			S.Players[1].Units.push_back({ "usa_mbt", EnemyAt, Angle() });
			World W;
			const FirstHit H = FireAtGround(W, S, Ground);
			const int32_t Own = H.Log.Of(UnitNear(W, 0, OwnAt));
			const int32_t Friend = H.Log.Of(UnitNear(W, 2, AllyAt));
			const int32_t Enemy = H.Log.Of(UnitNear(W, 1, EnemyAt));
			std::printf("  8. na mesma explosao: o proprio %d, o aliado %d, o inimigo (3 m da borda) %d\n", Own, Friend, Enemy);
			Check(H.bHit && H.Point == P0.Point && Own == 0 && Friend == 0, "8. o proprio e o aliado no meio da explosao nao levam nada");
			Check(Enemy == SplashOf(110, Fix::FromInt(3), Wp) && Enemy > 0, "8. o inimigo ao lado leva a area (3 m da borda)");
		}

		// 9. o acerto direto: o alvo leva 110 uma vez só; o vizinho (8 m ao lado) leva só a área
		{
			MatchSetup S = Base;
			const Vec2 TargetAt = Offset(Mid, 60, 0);
			const Vec2 NeighbourAt = Offset(Mid, 60, 8);
			S.Players[1].Units.push_back({ "usa_mbt", TargetAt, Angle() });
			S.Players[1].Units.push_back({ "usa_mbt", NeighbourAt, Angle() });
			World W;
			Check(W.Init(S), "montagem do acerto direto");
			const EntityId Tom = FindOf(W, 0, DefKey(W, "usa_tomahawk"));
			const EntityId Target = UnitNear(W, 1, TargetAt);
			const EntityId Neighbour = UnitNear(W, 1, NeighbourAt);
			StepWith(W, { Group(0, { Tom }), Order(0, CommandType::Attack, Target) });
			const FirstHit H = RunToFirstHit(W, Tom, 10 * TicksPerSecond);
			std::printf("  9. acerto direto: o alvo %d (em %d acerto), o vizinho %d\n", H.Log.Of(Target), H.Log.HitsOn(Target), H.Log.Of(Neighbour));
			Check(H.bHit && H.Target == Target, "9. o missil acerta o tanque em cheio (o contato)");
			Check(H.Log.Of(Target) == 110 && H.Log.HitsOn(Target) == 1, "9. o alvo do acerto direto leva 110 uma vez so");
			Check(H.Log.Of(Neighbour) > 0 && H.Log.Of(Neighbour) < 110, "9. o vizinho leva so a area");
		}

		// 10. o tiro no chão (Ctrl) a 3 m da parede de uma usina inimiga, com um tanque inimigo do lado: os dois levam a área (o
		// prédio pela borda do lote)
		{
			MatchSetup S = Base;
			S.Players[1].Units.push_back({ "usa_power_plant", Offset(Mid, 60, 0), Angle() });
			World Look;
			Check(Look.Init(S), "montagem do tiro no chao perto do predio");
			const uint16_t PlantDef = DefKey(Look, "usa_power_plant");
			UnitView PV;
			Look.FindUnit(FindOf(Look, 1, PlantDef), PV);
			const Fix HX = Look.Rules().Units[PlantDef].HalfX;
			const Fix HY = Look.Rules().Units[PlantDef].HalfY;
			const Vec2 Near = PV.Pos - Vec2::Make(HX + Fix::FromInt(3), Fix::Zero());
			const Vec2 TankAt = Near + Vec2::Make(Fix::Zero(), Fix::FromMilli(4600));
			S.Players[1].Units.push_back({ "usa_mbt", TankAt, Angle() });
			World W;
			const FirstHit H = FireAtGround(W, S, Near);
			const EntityId Plant = FindOf(W, 1, PlantDef);
			const int32_t PlantDamage = H.Log.Of(Plant);
			const int32_t TankDamage = H.Log.Of(UnitNear(W, 1, TankAt));
			const int32_t PlantWant = SplashOf(110, RectGapTo(H.Point, PV.Pos, HX, HY), Wp);
			std::printf("  10. tiro no chao a 3 m da usina (explodiu a %.2f m do ponto, %.2f m da parede): a usina %d (a conta pela borda: %d), o tanque ao lado %d\n",
				Distance(H.Point, Near).ToDouble(), RectGapTo(H.Point, PV.Pos, HX, HY).ToDouble(), PlantDamage, PlantWant, TankDamage);
			Check(H.bHit && PlantDamage > 0 && PlantDamage == PlantWant && TankDamage > 0, "10. o tiro no chao (Ctrl) fere a area: o predio pela borda do lote e o tanque ao lado");
		}
	}

	void TestMinRange()
	{
		std::printf("[alcance minimo]\n");
		// 11. um Dozer inimigo (desarmado: não revida) a 15 m e a 25 m do lançador: parado (a mira sozinha), na ordem de ataque
		// e no tiro no chão em cima dele (cada um numa partida nova: a 25 m o Dozer cai com 3 mísseis)
		for (const int32_t Meters : { 15, 25 })
		{
			int32_t Shots[3] = {};
			bool bAimed = false;
			for (int32_t Phase = 0; Phase < 3; ++Phase)
			{
				Vec2 Mid;
				MatchSetup S = LauncherSetup(Mid);
				const Vec2 DozerAt = Offset(Mid, Meters, 0);
				S.Players[1].Units.push_back({ "usa_dozer", DozerAt, Angle() });
				World W;
				Check(W.Init(S), "montagem do alcance minimo");
				const EntityId Tom = FindOf(W, 0, DefKey(W, "usa_tomahawk"));
				const EntityId Dozer = FindOf(W, 1, DefKey(W, "usa_dozer"));
				if (Phase == 1)
				{
					StepWith(W, { Group(0, { Tom }), Order(0, CommandType::Attack, Dozer) });
				}
				else if (Phase == 2)
				{
					StepWith(W, { Group(0, { Tom }), Order(0, CommandType::AttackGround, NoEntity, 0, DozerAt) });
				}
				RunUntil(W, 8 * TicksPerSecond, [&]()
				{
					for (const SimEvent& E : W.LastEvents())
					{
						Shots[Phase] += E.Type == EventType::WeaponFired && E.Source == Tom ? 1 : 0;
					}
					UnitView V;
					bAimed |= Phase < 2 && W.FindUnit(Tom, V) && V.FireTarget == Dozer && V.FireTarget != NoEntity;
					return false;
				});
			}
			std::printf("  11. inimigo a %d m: tiros parado %d, na ordem de ataque %d, no chao %d; mirou nele: %s\n", Meters, Shots[0], Shots[1], Shots[2], bAimed ? "sim" : "nao");
			if (Meters == 15)
			{
				Check(Shots[0] + Shots[1] + Shots[2] == 0, "11. a 15 m o Tomahawk nunca atira (parado, na ordem de ataque e no chao)");
				Check(!bAimed, "11. a 15 m a mira pula o inimigo (o NearestEnemy e a ordem)");
			}
			else
			{
				Check(Shots[0] > 0 && bAimed, "11. a 25 m ele mira e atira sozinho");
				Check(Shots[1] > 0 && Shots[2] > 0, "11. a 25 m a ordem de ataque e o tiro no chao saem");
			}
		}
		// 12. um tanque a 40 m e uma Patriot a 65 m, os dois à vista: a artilharia escolhe a Patriot (prédio antes de unidade)
		{
			Vec2 Mid;
			MatchSetup S = LauncherSetup(Mid);
			const Vec2 TankAt = Offset(Mid, 40, 0);
			S.Players[1].Units.push_back({ "usa_mbt", TankAt, Angle::FromBam(Angle::HalfTurn) });
			S.Players[1].Units.push_back({ "usa_patriot", Offset(Mid, 0, 65), Angle() });   // sem usina: não atira
			World W;
			Check(W.Init(S), "montagem da preferencia da artilharia");
			const EntityId Tom = FindOf(W, 0, DefKey(W, "usa_tomahawk"));
			const EntityId Tank = UnitNear(W, 1, TankAt);
			const EntityId Patriot = FindOf(W, 1, DefKey(W, "usa_patriot"));
			UnitView PV;
			W.FindUnit(Patriot, PV);
			// o tanque não revida: atira no chão do outro lado (o tiro no chão não fere ninguém)
			StepWith(W, { Group(1, { Tank }), Order(1, CommandType::AttackGround, NoEntity, 0, Offset(Mid, 60, 0)) });
			EntityId FirstAim = NoEntity;
			EntityId FirstShotAt = NoEntity;
			RunUntil(W, 10 * TicksPerSecond, [&]()
			{
				UnitView V;
				if (FirstAim == NoEntity && W.FindUnit(Tom, V))
				{
					FirstAim = V.FireTarget;
				}
				for (const SimEvent& E : W.LastEvents())
				{
					if (E.Type == EventType::WeaponFired && E.Source == Tom && FirstShotAt == NoEntity)
					{
						FirstShotAt = E.Target;
					}
				}
				return FirstShotAt != NoEntity;
			});
			std::printf("  12. tanque a 40 m, Patriot a %.1f m: mirou primeiro em %s, 1o tiro em %s\n", Distance(PV.Pos, Mid).ToDouble(),
				FirstAim == Patriot ? "Patriot" : (FirstAim == Tank ? "tanque" : "nada"), FirstShotAt == Patriot ? "Patriot" : (FirstShotAt == Tank ? "tanque" : "nada"));
			Check(FirstAim == Patriot && FirstShotAt == Patriot, "12. com o tanque a 40 m e a Patriot a 65 m, o Tomahawk mira e atira na Patriot");
		}
	}

	/** A base do jogador 0 pro avião de carga: CC no meio, usina a oeste, War Factory ao sul e o Dozer; o inimigo 1 com o CC
	 *  longe. Sem névoa. */
	MatchSetup AirliftSetup(Vec2& OutMid)
	{
		MatchSetup S = RankSetup();
		OutMid = S.Map.CellCenter(Cell{ 48, 48 });
		S.Players[0].Units = { { "usa_command_center", OutMid, Angle() }, { "usa_power_plant", Offset(OutMid, -40, 0), Angle() },
			{ "usa_war_factory", Offset(OutMid, 0, -50), Angle() }, { "usa_dozer", Offset(OutMid, -30, 40), Angle() } };
		S.Players[1].Units = { { "usa_command_center", Offset(OutMid, -140, 140), Angle() } };
		return S;
	}

	/** Os eventos da zona Zone num passo: o aviso (paletes), o pouso (caixas) e a empilhadeira ($). */
	struct AirliftLog
	{
		std::vector<TickNumber> InboundAt;
		std::vector<int32_t> Pallets;
		std::vector<TickNumber> LandedAt;
		std::vector<int32_t> Boxes;
		std::vector<TickNumber> PaidAt;
		std::vector<int32_t> Paid;

		void Add(const World& W, EntityId Zone, TickNumber T)
		{
			for (const SimEvent& E : W.LastEvents())
			{
				if (E.Source != Zone)
				{
					continue;
				}
				if (E.Type == EventType::AirliftInbound)
				{
					InboundAt.push_back(T);
					Pallets.push_back(E.Value);
				}
				else if (E.Type == EventType::AirliftLanded)
				{
					LandedAt.push_back(T);
					Boxes.push_back(E.Value);
				}
				else if (E.Type == EventType::SupplyDelivered && E.Target == Zone)
				{
					PaidAt.push_back(T);
					Paid.push_back(E.Value);
				}
			}
		}
	};

	/** Anda até Done(T) (T = o passo que acabou de andar) ou o teto; devolve o passo em que parou (0 = não parou). */
	template <typename Fn>
	TickNumber StepUntil(World& W, int32_t MaxTicks, Fn&& Done)
	{
		for (int32_t K = 0; K < MaxTicks; ++K)
		{
			const TickNumber T = W.Tick();
			StepWith(W, {});
			if (Done(T))
			{
				return T;
			}
		}
		return 0;
	}

	bool HasEvent(const World& W, EventType Type, EntityId Source, EntityId Target)
	{
		for (const SimEvent& E : W.LastEvents())
		{
			if (E.Type == Type && (Source == NoEntity || E.Source == Source) && (Target == NoEntity || E.Target == Target))
			{
				return true;
			}
		}
		return false;
	}

	void TestAirlift()
	{
		std::printf("[lancamento]\n");
		// 13. a zona construída pelo Dozer: o pouso em pronta + 1 350, depois a cada 2 700; 12 / 16 / 20 / 20 caixas; o aviso 300
		// passos antes; a empilhadeira paga $300 a cada 90 passos e o banco fecha exato
		{
			Vec2 Mid;
			const MatchSetup S = AirliftSetup(Mid);
			World W;
			Check(W.Init(S), "montagem do aviao de carga");
			const uint16_t ZoneDef = DefKey(W, "usa_drop_zone");
			const EntityId Dozer = FindOf(W, 0, DefKey(W, "usa_dozer"));
			const Vec2 Site = Offset(Mid, 0, 50);
			const PlaceResult Can = W.CanPlace(0, ZoneDef, 0, Site);
			Check(Can == PlaceResult::Ok, "13. a zona cabe atras do CC");
			StepWith(W, { Group(0, { Dozer }), Order(0, CommandType::Build, NoEntity, BuildValue(ZoneDef, 0), Site) });
			const EntityId Zone = FindOf(W, 0, ZoneDef);
			const TickNumber Done = StepUntil(W, 120 * TicksPerSecond, [&](TickNumber) { return HasEvent(W, EventType::StructureCompleted, NoEntity, Zone); });
			UnitView ZV;
			Check(Done > 0 && W.FindUnit(Zone, ZV) && ZV.IncomeLeft == 1350 && ZV.IncomeCount == 0 && ZV.Cargo == 0, "13. a zona pronta liga o relogio: 1 350 passos (45 s) ate o 1o pouso");
			const int32_t Money0 = W.Money(0);
			AirliftLog L;
			const TickNumber End = Done + 1350 + 3 * 2700 + 5 * 90 + 30;
			while (W.Tick() <= End)
			{
				const TickNumber T = W.Tick();
				StepWith(W, {});
				L.Add(W, Zone, T);
			}
			std::printf("  13. pronta no passo %u; pousos em +", Done);
			for (size_t K = 0; K < L.LandedAt.size(); ++K)
			{
				std::printf("%s%u (%d caixas, aviso %d antes, %d paletes)", K ? ", +" : "", L.LandedAt[K] - Done, L.Boxes[K],
					K < L.InboundAt.size() ? int32_t(L.LandedAt[K] - L.InboundAt[K]) : -1, K < L.Pallets.size() ? L.Pallets[K] : -1);
			}
			std::printf("; empilhadeira %zu x, banco +%d\n", L.PaidAt.size(), W.Money(0) - Money0);
			const int32_t WantBoxes[4] = { 12, 16, 20, 20 };
			bool bLanded = L.LandedAt.size() == 4 && L.InboundAt.size() == 4;
			std::vector<TickNumber> WantPaid;
			for (int32_t K = 0; bLanded && K < 4; ++K)
			{
				bLanded &= L.LandedAt[size_t(K)] == Done + 1350 + TickNumber(K) * 2700 && L.Boxes[size_t(K)] == WantBoxes[K];
				for (int32_t J = 1; J <= WantBoxes[K] / 4; ++J)
				{
					WantPaid.push_back(L.LandedAt[size_t(K)] + TickNumber(J) * 90);
				}
			}
			bool bInbound = bLanded;
			for (int32_t K = 0; bInbound && K < 4; ++K)
			{
				bInbound &= L.InboundAt[size_t(K)] == L.LandedAt[size_t(K)] - 300 && L.Pallets[size_t(K)] == WantBoxes[K] / 4;
			}
			bool bPaid = L.PaidAt == WantPaid;
			for (const int32_t V : L.Paid)
			{
				bPaid &= V == 300;
			}
			Check(bLanded, "13. os pousos em pronta + 1 350 e depois a cada 2 700, com 12 / 16 / 20 / 20 caixas (a rota amadurece)");
			Check(bInbound, "13. o aviso (o aviao aparece) 300 passos antes de cada pouso, com os paletes 3 / 4 / 5 / 5");
			Check(bPaid, "13. a empilhadeira leva um palete ($300, SupplyDelivered) a cada 90 passos depois do pouso");
			Check(W.Money(0) == Money0 + 5100 && W.FindUnit(Zone, ZV) && ZV.Cargo == 0 && ZV.IncomeCount == 4, "13. o banco fecha exato: 68 caixas x $75 = $5 100");
		}

		// 14. sem energia: o relógio para acima do aviso e a empilhadeira para; o pouso já anunciado acontece igual. A "tomada" de
		// teste gasta 10 de energia (60 PV: um tiro do tanque derruba; 1 s de obra): com ela de pé, a energia falta
		{
			Vec2 Mid;
			MatchSetup S = AirliftSetup(Mid);
			UnitSpec Drain;
			Drain.Key = "test_drain";
			Drain.Kind = ObjectKind::Structure;
			Drain.Armor = ArmorType::Structure;
			Drain.MaxHp = 60;
			Drain.SizeXMm = 8000;
			Drain.SizeYMm = 8000;
			Drain.SightMm = 20000;
			Drain.BuildTimeMs = 1000;
			Drain.Energy = -10;
			S.Rules.Units.push_back(UnitDef::FromSpec(Drain));
			const uint16_t DrainDef = uint16_t(S.Rules.Units.size() - 1);
			S.Rules.Units[size_t(S.Rules.FindUnit("usa_dozer"))].Constructs.push_back(DrainDef);
			const Vec2 DrainBSite = Offset(Mid, 30, 50);
			S.Players[0].Units = { { "usa_command_center", Mid, Angle() }, { "usa_power_plant", Offset(Mid, -40, 0), Angle() },
				{ "usa_drop_zone", Offset(Mid, 0, 50), Angle() }, { "test_drain", Offset(Mid, -40, 30), Angle() },
				{ "usa_mbt", Offset(Mid, -40, 55), Angle() }, { "usa_dozer", Offset(Mid, 30, 62), Angle() } };
			World W;
			Check(W.Init(S), "montagem da zona sem energia");
			const EntityId Zone = FindOf(W, 0, DefKey(W, "usa_drop_zone"));
			const EntityId DrainA = FindOf(W, 0, DrainDef);
			const EntityId Tank = FindOf(W, 0, DefKey(W, "usa_mbt"));
			const EntityId Dozer = FindOf(W, 0, DefKey(W, "usa_dozer"));
			AirliftLog L;
			for (int32_t K = 0; K < 1500; ++K)
			{
				const TickNumber T = W.Tick();
				StepWith(W, {});
				L.Add(W, Zone, T);
			}
			UnitView ZV;
			W.FindUnit(Zone, ZV);
			std::printf("  14. 50 s sem energia: relogio em %d passos, %zu avisos, %zu pousos\n", ZV.IncomeLeft, L.InboundAt.size(), L.LandedAt.size());
			Check(W.Player(0).IsLowPower() && ZV.IncomeLeft == 1350 && L.InboundAt.empty() && L.LandedAt.empty(), "14. sem energia o relogio para acima do aviso (nada de aviao)");
			// a energia volta (o tanque derruba a tomada com o Ctrl): o aviso vem 1 050 passos depois
			StepWith(W, { Group(0, { Tank }), Order(0, CommandType::ForceAttack, DrainA) });
			const TickNumber DrainDied = StepUntil(W, 30 * TicksPerSecond, [&](TickNumber) { return HasEvent(W, EventType::UnitDied, NoEntity, DrainA); });
			const TickNumber Inbound = StepUntil(W, 60 * TicksPerSecond, [&](TickNumber T) { L.Add(W, Zone, T); return !L.InboundAt.empty(); });
			Check(DrainDied > 0 && Inbound == DrainDied + 1050, "14. com a energia de volta o relogio anda: o aviso 1 050 passos depois (1 350 - 300)");
			// o avião já anunciado: a energia cai de novo (a 2ª tomada fica pronta) e ele pousa igual, na hora
			StepWith(W, { Group(0, { Dozer }), Order(0, CommandType::Build, NoEntity, BuildValue(DrainDef, 0), DrainBSite) });
			const EntityId DrainB = UnitNear(W, 0, DrainBSite);
			TickNumber DrainBDone = 0;
			const TickNumber Landed = StepUntil(W, 20 * TicksPerSecond, [&](TickNumber T)
			{
				L.Add(W, Zone, T);
				DrainBDone = DrainBDone == 0 && HasEvent(W, EventType::StructureCompleted, NoEntity, DrainB) ? T : DrainBDone;
				return !L.LandedAt.empty();
			});
			const bool bLowAtLanding = W.Player(0).IsLowPower();
			std::printf("  14. aviso no passo %u, a energia caiu de novo no %u, pouso no %u (%d caixas)\n", Inbound, DrainBDone, Landed, L.Boxes.empty() ? 0 : L.Boxes[0]);
			Check(DrainBDone > Inbound && DrainBDone < Landed && bLowAtLanding && Landed == Inbound + 300 && !L.Boxes.empty() && L.Boxes[0] == 12,
				"14. o pouso ja anunciado acontece sem energia, na hora (300 passos depois do aviso)");
			const size_t PaidBefore = L.PaidAt.size();
			for (int32_t K = 0; K < 600; ++K)
			{
				const TickNumber T = W.Tick();
				StepWith(W, {});
				L.Add(W, Zone, T);
			}
			W.FindUnit(Zone, ZV);
			Check(L.PaidAt.size() == PaidBefore && ZV.Cargo == 12 && ZV.IncomeLeft == 2700 && L.InboundAt.size() == 1,
				"14. sem energia a empilhadeira para (12 caixas no patio) e o relogio do proximo para acima do aviso");
			// a energia volta: a empilhadeira retoma (o 1º palete 90 passos depois)
			StepWith(W, { Group(0, { Tank }), Order(0, CommandType::ForceAttack, DrainB) });
			const TickNumber DrainBDied = StepUntil(W, 40 * TicksPerSecond, [&](TickNumber T) { L.Add(W, Zone, T); return HasEvent(W, EventType::UnitDied, NoEntity, DrainB); });
			const TickNumber Paid = StepUntil(W, 10 * TicksPerSecond, [&](TickNumber T) { L.Add(W, Zone, T); return L.PaidAt.size() > PaidBefore; });
			Check(DrainBDied > 0 && Paid == DrainBDied + 90, "14. com a energia de volta a empilhadeira retoma: $300 em 90 passos");
		}

		// 15. o teto: uma zona pronta e uma fundação; a 3ª dá LimitReached e o Build é recusado (nem obra nem cobrança)
		{
			Vec2 Mid;
			MatchSetup S = AirliftSetup(Mid);
			S.Players[0].Units.push_back({ "usa_drop_zone", Offset(Mid, 0, 50), Angle() });
			World W;
			Check(W.Init(S), "montagem do teto das zonas");
			const uint16_t ZoneDef = DefKey(W, "usa_drop_zone");
			const EntityId Dozer = FindOf(W, 0, DefKey(W, "usa_dozer"));
			const Vec2 Second = Offset(Mid, 40, 50);
			const Vec2 Third = Offset(Mid, 40, 85);
			Check(W.CanPlace(0, ZoneDef, 0, Second) == PlaceResult::Ok, "15. a 2a zona cabe");
			StepWith(W, { Group(0, { Dozer }), Order(0, CommandType::Build, NoEntity, BuildValue(ZoneDef, 0), Second) });
			Check(CountOf(W, 0, ZoneDef) == 2, "15. a 2a zona vira fundacao");
			const int32_t MoneyBefore = W.Money(0);
			const PlaceResult ThirdCan = W.CanPlace(0, ZoneDef, 0, Third);
			StepWith(W, { Group(0, { Dozer }), Order(0, CommandType::Build, NoEntity, BuildValue(ZoneDef, 0), Third) });
			std::printf("  15. a 3a zona: CanPlace %d (LimitReached = %d); zonas %d, banco %d -> %d\n", int32_t(ThirdCan), int32_t(PlaceResult::LimitReached),
				CountOf(W, 0, ZoneDef), MoneyBefore, W.Money(0));
			Check(ThirdCan == PlaceResult::LimitReached, "15. a 3a zona da LimitReached (a fundacao conta)");
			Check(CountOf(W, 0, ZoneDef) == 2 && W.Money(0) == MoneyBefore, "15. o Build da 3a e recusado (nem obra nem cobranca)");
		}

		// 16. a zona derrubada com 12 caixas no pátio solta a pilha de 12; o caminhão inimigo pega e entrega no centro dele; a zona
		// reconstruída começa de novo em 3 paletes. A empilhadeira devagar aqui (60 s por palete): as caixas ficam no pátio
		// enquanto os 6 tanques derrubam a zona
		{
			Vec2 Mid;
			MatchSetup S = AirliftSetup(Mid);
			const uint16_t ZoneDef = uint16_t(S.Rules.FindUnit("usa_drop_zone"));
			S.Rules.Units[ZoneDef].AirliftUnloadTicks = 60 * TicksPerSecond;
			const Vec2 Site = Offset(Mid, 0, 50);
			S.Players[0].Units.push_back({ "usa_drop_zone", Site, Angle() });
			const Vec2 CenterAt = Offset(Mid, 90, 120);
			const Vec2 TruckAt = Offset(Mid, 90, 95);
			S.Players[1].Units.push_back({ "usa_supply_center", CenterAt, Angle() });
			S.Players[1].Units.push_back({ "usa_supply_truck", TruckAt, Angle() });
			for (int32_t K = 0; K < 6; ++K)
			{
				S.Players[1].Units.push_back({ "usa_mbt", Offset(Mid, -12 + 12 * (K % 3), 115 + 12 * (K / 3)), Angle::FromBam(3 * Angle::QuarterTurn) });
			}
			World W;
			Check(W.Init(S), "montagem da zona derrubada");
			const uint16_t Mbt = DefKey(W, "usa_mbt");
			const EntityId Zone = FindOf(W, 0, ZoneDef);
			const EntityId Dozer = FindOf(W, 0, DefKey(W, "usa_dozer"));
			const EntityId Truck = FindOf(W, 1, DefKey(W, "usa_supply_truck"));
			const EntityId Center = FindOf(W, 1, DefKey(W, "usa_supply_center"));
			std::vector<EntityId> Tanks;
			for (int32_t I = 0; I < W.UnitCount(); ++I)
			{
				const UnitView V = W.Unit(I);
				if (V.Owner == 1 && V.Def == Mbt)
				{
					Tanks.push_back(V.Id);
				}
			}
			AirliftLog L;
			StepUntil(W, 50 * TicksPerSecond, [&](TickNumber T) { L.Add(W, Zone, T); return !L.LandedAt.empty(); });
			UnitView ZV;
			const bool bLoaded = W.FindUnit(Zone, ZV) && ZV.Cargo == 12 && ZV.IncomeCount == 1;
			StepWith(W, { Group(1, Tanks), Order(1, CommandType::Attack, Zone) });
			int32_t PileIndex = -1;
			const TickNumber Fell = StepUntil(W, 40 * TicksPerSecond, [&](TickNumber T)
			{
				L.Add(W, Zone, T);
				for (const SimEvent& E : W.LastEvents())
				{
					PileIndex = E.Type == EventType::CargoDropped ? E.Value : PileIndex;
				}
				return HasEvent(W, EventType::UnitDied, NoEntity, Zone);
			});
			const DockView Pile = W.Dock(PileIndex);
			std::printf("  16. a zona caiu no passo %u com %d caixas no patio: a pilha %d tem %d caixas em (%.1f, %.1f)\n", Fell, ZV.Cargo, PileIndex, Pile.Boxes,
				Pile.Pos.X.ToDouble(), Pile.Pos.Y.ToDouble());
			Check(bLoaded && L.PaidAt.empty() && Fell > 0 && PileIndex >= 0 && Pile.bPile && Pile.Boxes == 12 && Distance(Pile.Pos, ZV.Pos) <= Fix::One(),
				"16. a zona derrubada com 12 caixas no patio solta a pilha de 12 (a carga que cai)");
			// os tanques voltam pra longe; o caminhão inimigo leva a pilha pro centro dele
			StepWith(W, { Group(1, Tanks), Order(1, CommandType::Move, NoEntity, 0, Offset(Mid, 0, 170)) });
			int32_t Delivered = 0;
			int32_t ToCenter = 0;
			StepUntil(W, 150 * TicksPerSecond, [&](TickNumber)
			{
				for (const SimEvent& E : W.LastEvents())
				{
					if (E.Type == EventType::SupplyDelivered && E.Source == Truck)
					{
						Delivered += E.Value;
						ToCenter += E.Target == Center ? E.Value : 0;
					}
				}
				return W.Dock(PileIndex).Boxes == 0 && Delivered >= 900;
			});
			std::printf("  16. o caminhao inimigo entregou $%d no centro dele; a pilha ficou com %d\n", ToCenter, W.Dock(PileIndex).Boxes);
			Check(Delivered == 900 && ToCenter == 900 && W.Dock(PileIndex).Boxes == 0, "16. o caminhao inimigo pega a pilha e entrega no centro dele (12 x $75)");
			// a zona reconstruída no mesmo lugar: a rota começa de novo (3 paletes, 12 caixas)
			const PlaceResult Again = W.CanPlace(0, ZoneDef, 0, Site);
			StepWith(W, { Group(0, { Dozer }), Order(0, CommandType::Build, NoEntity, BuildValue(ZoneDef, 0), Site) });
			const EntityId Rebuilt = FindOf(W, 0, ZoneDef);
			AirliftLog L2;
			const TickNumber RebuiltDone = StepUntil(W, 120 * TicksPerSecond, [&](TickNumber) { return HasEvent(W, EventType::StructureCompleted, NoEntity, Rebuilt); });
			StepUntil(W, 50 * TicksPerSecond, [&](TickNumber T) { L2.Add(W, Rebuilt, T); return !L2.LandedAt.empty(); });
			std::printf("  16. reconstruida (CanPlace %d) pronta no passo %u: 1o pouso com %d paletes, %d caixas\n", int32_t(Again), RebuiltDone,
				L2.Pallets.empty() ? 0 : L2.Pallets[0], L2.Boxes.empty() ? 0 : L2.Boxes[0]);
			Check(Again == PlaceResult::Ok && Rebuilt != Zone && RebuiltDone > 0 && L2.Pallets.size() == 1 && L2.Pallets[0] == 3 && L2.Boxes.size() == 1 && L2.Boxes[0] == 12,
				"16. a zona reconstruida comeca de novo em 3 paletes (a rota volta ao comeco)");
		}
	}

	/** O jogador 0 na patente Rank com o CC a oeste do meio (a largada dele); o inimigo 1 com o CC longe. Sem névoa. */
	MatchSetup StrikeSetup(Vec2& OutMid, uint8_t Rank)
	{
		MatchSetup S = RankSetup();
		OutMid = S.Map.CellCenter(Cell{ 48, 48 });
		S.Players[0].Units = { { "usa_command_center", Offset(OutMid, -100, 0), Angle() } };
		S.Players[0].StartRank = Rank;
		S.Players[1].Units = { { "usa_command_center", Offset(OutMid, -140, 140), Angle() } };
		return S;
	}

	Command PowerOrder(PlayerIndex P, Vec2 At)
	{
		return Order(P, CommandType::UsePower, NoEntity, 0, At);
	}

	/** Os mísseis dos ataques (os tiros sem atirador): o passo de saída e o ponto de mira de cada um, o passo e o ponto da
	 *  explosão (pelo Id do tiro) e os PowerUsed. */
	struct StrikeLog
	{
		std::vector<TickNumber> FiredAt;
		std::vector<Vec2> Aims;
		std::vector<uint32_t> Ids;
		std::map<uint32_t, TickNumber> HitAt;
		std::map<uint32_t, Vec2> HitPoint;
		int32_t Used = 0;
		int32_t UsedValue = 0;

		void Add(const World& W, TickNumber T)
		{
			for (const SimEvent& E : W.LastEvents())
			{
				if (E.Type == EventType::WeaponFired && E.Source == NoEntity)
				{
					FiredAt.push_back(T);
					Aims.push_back(E.Point);
					Ids.push_back(uint32_t(E.Value));
				}
				else if (E.Type == EventType::ProjectileHit && E.Source == NoEntity)
				{
					HitAt[uint32_t(E.Value)] = T;
					HitPoint[uint32_t(E.Value)] = E.Point;
				}
				else if (E.Type == EventType::PowerUsed)
				{
					++Used;
					UsedValue = E.Value;
				}
			}
		}
		/** Um passo com os comandos, anotado. */
		void Step(World& W, std::vector<Command> Commands)
		{
			const TickNumber T = W.Tick();
			StepWith(W, std::move(Commands));
			Add(W, T);
		}
	};

	void TestStrike()
	{
		std::printf("[ataque]\n");
		// 17. as recusas não mexem na espera: a patente 1, a espera, sem CC, o ponto nunca visto (com névoa) e fora do mapa
		{
			Vec2 Mid;
			World W;
			Check(W.Init(StrikeSetup(Mid, 1)), "montagem do ataque na patente 1");
			StrikeLog L;
			L.Step(W, { PowerOrder(0, Offset(Mid, 40, 0)) });
			Check(L.Used == 0 && W.StrikeCount() == 0 && W.PowerReadyAt(0, 0) == 0 && !W.IsPowerReady(0, 0), "17. na patente 1 o ataque e recusado (e a espera nao muda)");
		}
		{
			Vec2 Mid;
			World W;
			Check(W.Init(StrikeSetup(Mid, 2)), "montagem do ataque na patente 2");
			StrikeLog L;
			const TickNumber T = W.Tick();
			L.Step(W, { PowerOrder(0, Offset(Mid, 40, 0)) });
			const TickNumber Ready = W.PowerReadyAt(0, 0);
			Check(L.Used == 1 && L.UsedValue == (0 | (0 << 8) | (6 << 16)) && Ready == T + 5400 && !W.IsPowerReady(0, 0),
				"17. na patente 2 o ataque sai (PowerUsed com 6 misseis) e a espera vai a 5 400 passos");
			StrikeLog L2;
			L2.Step(W, { PowerOrder(0, Offset(Mid, 20, 20)) });
			Check(L2.Used == 0 && W.PowerReadyAt(0, 0) == Ready, "17. na espera o ataque e recusado e a espera nao muda");
		}
		{
			Vec2 Mid;
			MatchSetup S = StrikeSetup(Mid, 2);
			S.Players[0].Units = { { "usa_power_plant", Offset(Mid, -100, 0), Angle() } };
			World W;
			Check(W.Init(S), "montagem do ataque sem CC");
			StrikeLog L;
			L.Step(W, { PowerOrder(0, Offset(Mid, 40, 0)) });
			Check(L.Used == 0 && W.StrikeCount() == 0 && W.PowerReadyAt(0, 0) == 0 && !W.IsPowerReady(0, 0), "17. sem CC o ataque e recusado");
		}
		{
			Vec2 Mid;
			MatchSetup S = StrikeSetup(Mid, 2);
			S.bFogOfWar = true;
			World W;
			Check(W.Init(S), "montagem do ataque com nevoa");
			const Vec2 Unseen = Offset(Mid, 150, 150);
			const Vec2 Outside = Offset(Mid, 400, 0);
			const Vec2 Seen = Offset(Mid, -60, 0);
			const Cell UC = W.Map().CellOf(Unseen);
			StrikeLog L;
			L.Step(W, { PowerOrder(0, Unseen) });
			L.Step(W, { PowerOrder(0, Outside) });
			const bool bRefused = L.Used == 0 && W.PowerReadyAt(0, 0) == 0 && W.StrikeCount() == 0;
			L.Step(W, { PowerOrder(0, Seen) });
			Check(!W.IsCellExplored(W.TeamOf(0), UC.X, UC.Y) && bRefused, "17. com nevoa o ponto nunca visto e o de fora do mapa sao recusados (sem mexer na espera)");
			Check(L.Used == 1 && W.PowerReadyAt(0, 0) > 0, "17. o ponto ja visto (perto do CC) vale");
		}

		// 18. os mísseis: na patente 2, 6 nos passos T a T+30 (um a cada 0,2 s) nos pontos fixos do girassol, explodindo entre 2,8
		// e 4,0 s depois do clique; na 4, 9 em 14 m (até 4,5 s)
		std::vector<Vec2> FirstAims;
		for (const uint8_t Rank : { uint8_t(2), uint8_t(4), uint8_t(2) })
		{
			Vec2 Mid;
			World W;
			Check(W.Init(StrikeSetup(Mid, Rank)), "montagem dos misseis do ataque");
			const Vec2 Target = Offset(Mid, 60, -60);
			StrikeLog L;
			const TickNumber T = W.Tick();
			L.Step(W, { PowerOrder(0, Target) });
			std::vector<Vec2> Planned;
			Fix Radius;
			if (W.StrikeCount() == 1)
			{
				const World::StrikeView SV = W.StrikeAt(0);
				Radius = SV.Radius;
				for (int32_t K = 0; K < SV.Total; ++K)
				{
					Planned.push_back(W.StrikeAimPoint(0, K));
				}
			}
			for (int32_t K = 0; K < 8 * TicksPerSecond; ++K)
			{
				L.Step(W, {});
			}
			const size_t Want = Rank == 2 ? 6 : 9;
			bool bTicks = L.FiredAt.size() == Want;
			for (size_t K = 0; bTicks && K < Want; ++K)
			{
				bTicks &= L.FiredAt[K] == T + TickNumber(6 * K);
			}
			bool bInside = L.Aims == Planned && L.Aims.size() == Want;
			for (const Vec2& A : L.Aims)
			{
				bInside &= Distance(A, Target) <= Radius;
			}
			TickNumber Soonest = 0xFFFFFFFFu;
			TickNumber Latest = 0;
			bool bOnPoint = L.HitAt.size() == Want;
			for (size_t K = 0; K < L.Ids.size(); ++K)
			{
				const auto It = L.HitAt.find(L.Ids[K]);
				if (It == L.HitAt.end())
				{
					bOnPoint = false;
					continue;
				}
				Soonest = std::min(Soonest, It->second - T);
				Latest = std::max(Latest, It->second - T);
				bOnPoint &= Distance(L.HitPoint[L.Ids[K]], L.Aims[K]) <= Fix::FromInt(2);
			}
			std::printf("  18. patente %d: %zu misseis em %.1f m; explosoes de %.2f a %.2f s; mira (relativa ao centro):", int32_t(Rank), L.Aims.size(), Radius.ToDouble(),
				double(Soonest) / TicksPerSecond, double(Latest) / TicksPerSecond);
			for (const Vec2& A : L.Aims)
			{
				std::printf(" (%.1f, %.1f)", (A.X - Target.X).ToDouble(), (A.Y - Target.Y).ToDouble());
			}
			std::printf("\n");
			Check(bTicks, Rank == 2 ? "18. na patente 2 saem 6 misseis, nos passos T a T+30" : "18. na patente 4 saem 9 misseis, um a cada 6 passos");
			Check(bInside, "18. os pontos de mira sao os do girassol (StrikeAimPoint), dentro do raio");
			Check(bOnPoint && Soonest >= TickNumber(28 * TicksPerSecond / 10) && Latest <= TickNumber((Rank == 2 ? 40 : 45) * TicksPerSecond / 10),
				Rank == 2 ? "18. as 6 explosoes no ponto, entre 2,8 e 4,0 s depois do clique" : "18. as 9 explosoes no ponto, ate 4,5 s");
			if (FirstAims.empty())
			{
				FirstAims = L.Aims;
			}
			else if (Rank == 2)
			{
				Check(L.Aims == FirstAims, "18. o mesmo clique no mesmo passo da os mesmos pontos (sem sorteio)");
			}
		}

		// 19. o estrago no centro: a Patriot sozinha (com energia) leva 630 ± 15%; a War Factory ~900 (os 6 dentro do lote). 20. a
		// espera de 5 400 passos exatos e o XP pro dono do ataque
		for (const char* Key : { "usa_patriot", "usa_war_factory" })
		{
			Vec2 Mid;
			MatchSetup S = StrikeSetup(Mid, 2);
			S.Players[1].Units.push_back({ Key, Offset(Mid, 60, -60), Angle() });
			S.Players[1].Units.push_back({ "usa_power_plant", Offset(Mid, 130, -60), Angle() });
			World W;
			Check(W.Init(S), "montagem do estrago do ataque");
			const uint16_t Def = DefKey(W, Key);
			const EntityId Target = FindOf(W, 1, Def);
			UnitView TV;
			W.FindUnit(Target, TV);
			const int64_t Xp0 = W.Player(0).XpMilli;
			const TickNumber T = W.Tick();
			StepWith(W, { PowerOrder(0, TV.Pos) });
			HitLog Dmg;
			Dmg.Add(W, NoEntity);
			for (int32_t K = 0; K < 6 * TicksPerSecond; ++K)
			{
				StepWith(W, {});
				Dmg.Add(W, NoEntity);
			}
			const int32_t Taken = Dmg.Of(Target);
			const UnitDef& D = W.Rules().Units[Def];
			const int64_t WantXp = int64_t(Taken) * D.Cost * 1000 / (int64_t(D.MaxHp) * 10);   // nenhum acerto passa da vida: a soma vale
			std::printf("  19. %s (com energia: %s): %d de %d de vida em %d acertos; XP do dono +%lld milesimos (a conta: %lld)\n", Key,
				W.Player(1).IsLowPower() ? "nao" : "sim", Taken, D.MaxHp, Dmg.HitsOn(Target), (long long)(W.Player(0).XpMilli - Xp0), (long long)WantXp);
			if (Def == DefKey(W, "usa_patriot"))
			{
				Check(!W.Player(1).IsLowPower() && Taken >= 535 && Taken <= 725, "19. a Patriot sozinha com energia leva 630 +-15%");
			}
			else
			{
				Check(Taken >= 855 && Taken <= 900, "19. a War Factory leva ~900 (os 6 dentro do lote)");
			}
			Check(W.Player(0).XpMilli - Xp0 == WantXp && WantXp > 0, "20. o XP do estrago vai pro dono do ataque (o missil sem atirador)");
			Check(W.PowerReadyAt(0, 0) == T + 5400, "20. a espera e de 5 400 passos exatos (180 s)");
			while (W.Tick() < T + 5399)
			{
				StepWith(W, {});
			}
			const bool bEarly = W.IsPowerReady(0, 0);
			StrikeLog Early;
			Early.Step(W, { PowerOrder(0, TV.Pos) });
			const bool bReady = W.IsPowerReady(0, 0) && W.Tick() == T + 5400;
			StrikeLog OnTime;
			OnTime.Step(W, { PowerOrder(0, TV.Pos) });
			Check(!bEarly && Early.Used == 0 && bReady && OnTime.Used == 1, "20. no passo T+5 399 ainda espera; no T+5 400 o ataque sai de novo");
		}

		// 21. o replay de uma partida com ataques (aceitos e recusados) e aviões de carga refaz as mesmas impressões
		{
			MatchSetup S = RankSetup();
			const Vec2 Mid = S.Map.CellCenter(Cell{ 48, 48 });
			S.Seed = 77;
			S.Players[0].Units = { { "usa_command_center", Offset(Mid, -100, 0), Angle() }, { "usa_power_plant", Offset(Mid, -100, -40), Angle() },
				{ "usa_drop_zone", Offset(Mid, -100, 45), Angle() } };
			S.Players[0].StartRank = 2;
			S.Players[1].Units = { { "usa_command_center", Offset(Mid, 100, 0), Angle() }, { "usa_power_plant", Offset(Mid, 100, -40), Angle() },
				{ "usa_power_plant", Offset(Mid, 120, -40), Angle() }, { "usa_drop_zone", Offset(Mid, 100, 45), Angle() }, { "usa_patriot", Offset(Mid, 70, 20), Angle() } };
			S.Players[1].StartRank = 4;
			const Vec2 ZoneOne = S.Map.CellCenter(S.Map.CellOf(Offset(Mid, 100, 45)));
			struct Planned
			{
				TickNumber Tick;
				PlayerIndex Player;
				Vec2 At;
			};
			const std::vector<Planned> Plan = { { 60, 0, ZoneOne }, { 61, 0, Offset(Mid, 90, 0) }, { 90, 1, Offset(Mid, -100, 0) },
				{ 2000, 1, Offset(Mid, -100, 45) }, { 5470, 0, Offset(Mid, 70, 20) } };
			struct Run
			{
				std::vector<uint64_t> Hashes;
				std::vector<uint8_t> Replay;
				int32_t Used = 0;
				int32_t Landed = 0;
				int32_t Missiles = 0;
			};
			const auto Play = [&]() -> Run
			{
				Run R;
				World W;
				if (!W.Init(S))
				{
					Check(false, "montagem do replay dos ataques");
					return R;
				}
				ReplayWriter Rec;
				Rec.Begin(S);
				while (W.Tick() < 6000)
				{
					const TickNumber T = W.Tick();
					std::vector<Command> Due;
					for (const Planned& P : Plan)
					{
						if (P.Tick == T)
						{
							Command C = PowerOrder(P.Player, P.At);
							C.Tick = T;
							Due.push_back(C);
						}
					}
					W.Step(Due);
					Rec.AddTick(T, Due, W.StateHash());
					R.Hashes.push_back(W.StateHash());
					for (const SimEvent& E : W.LastEvents())
					{
						R.Used += E.Type == EventType::PowerUsed ? 1 : 0;
						R.Landed += E.Type == EventType::AirliftLanded ? 1 : 0;
						R.Missiles += E.Type == EventType::WeaponFired && E.Source == NoEntity ? 1 : 0;
					}
				}
				R.Replay = Rec.Finish();
				return R;
			};
			const Run A = Play();
			const Run B = Play();
			std::printf("  21. %d ataques aceitos (de %zu pedidos), %d misseis, %d pousos; replay %zu bytes\n", A.Used, Plan.size(), A.Missiles, A.Landed, A.Replay.size());
			Check(A.Used == 3 && A.Missiles == 6 + 9 + 6 && A.Landed >= 3, "21. a partida tem os ataques (3 aceitos, 2 recusados) e os avioes de carga");
			Check(A.Hashes == B.Hashes, "21. a partida com ataques roda igual duas vezes, passo a passo");
			ReplayReader Reader;
			const bool bLoaded = Reader.Load(A.Replay);
			int32_t Mismatch = -1;
			if (bLoaded)
			{
				World W;
				W.Init(Reader.Setup());
				for (TickNumber T = 0; T < Reader.LastTick() + 1 && !W.IsMatchOver(); ++T)
				{
					W.Step(Reader.CommandsFor(T));
					uint64_t Want = 0;
					if (Reader.ExpectedHash(T, Want) && Want != W.StateHash() && Mismatch < 0)
					{
						Mismatch = int32_t(T);
					}
				}
				Check(W.StateHash() == A.Hashes.back(), "21. o replay termina no mesmo estado");
			}
			Check(bLoaded && Mismatch < 0, "21. o replay com UsePower e os avioes de carga refaz todas as impressoes");
		}
	}

	void TestSiege()
	{
		std::printf("[cerco]\n");
		// 22. 3 Tomahawks andando atacando (de 150 m) contra 1 Patriot com energia: param fora do alcance dela, derrubam a bateria
		// em até 18 s do 1º tiro e não perdem nada
		{
			MatchSetup S = RankSetup();
			S.bFogOfWar = true;
			const Vec2 Mid = S.Map.CellCenter(Cell{ 48, 48 });
			const Vec2 Battery = Offset(Mid, 60, 0);
			S.Players[0].Units = { { "usa_tomahawk", Offset(Battery, -150, -10), Angle() }, { "usa_tomahawk", Offset(Battery, -150, 0), Angle() },
				{ "usa_tomahawk", Offset(Battery, -150, 10), Angle() } };
			S.Players[1].Units = { { "usa_patriot", Battery, Angle() }, { "usa_power_plant", Offset(Battery, 80, 0), Angle() } };
			World W;
			Check(W.Init(S), "montagem do cerco");
			const uint16_t TomDef = DefKey(W, "usa_tomahawk");
			std::vector<EntityId> Launchers;
			for (int32_t I = 0; I < W.UnitCount(); ++I)
			{
				const UnitView V = W.Unit(I);
				if (V.Owner == 0 && V.Def == TomDef)
				{
					Launchers.push_back(V.Id);
				}
			}
			const EntityId Patriot = FindOf(W, 1, DefKey(W, "usa_patriot"));
			UnitView PV;
			W.FindUnit(Patriot, PV);
			StepWith(W, { Group(0, Launchers), Order(0, CommandType::AttackMove, NoEntity, 0, Offset(Battery, 40, 0)) });
			TickNumber FirstShot = 0;
			Fix Closest = Fix::FromInt(1000);
			int32_t LauncherHp = 0;
			int32_t PatriotShots = 0;
			const TickNumber Died = StepUntil(W, 90 * TicksPerSecond, [&](TickNumber T)
			{
				LauncherHp = 0;
				for (const EntityId L : Launchers)
				{
					UnitView V;
					if (W.FindUnit(L, V))
					{
						Closest = Min(Closest, Distance(V.Pos, PV.Pos));
						LauncherHp += V.Hp;
					}
				}
				for (const SimEvent& E : W.LastEvents())
				{
					const bool bMine = std::find(Launchers.begin(), Launchers.end(), E.Source) != Launchers.end();
					FirstShot = FirstShot == 0 && E.Type == EventType::WeaponFired && bMine ? T : FirstShot;
					PatriotShots += E.Type == EventType::WeaponFired && E.Source == Patriot ? 1 : 0;
				}
				return HasEvent(W, EventType::UnitDied, NoEntity, Patriot);
			});
			std::printf("  22. 1o tiro em %.1f s, a Patriot caiu %.1f s depois; o lancador mais perto chegou a %.1f m; vida dos 3: %d de 600; tiros da Patriot %d\n",
				double(FirstShot) / TicksPerSecond, double(Died - FirstShot) / TicksPerSecond, Closest.ToDouble(), LauncherHp, PatriotShots);
			Check(Died > 0 && FirstShot > 0 && Died - FirstShot <= TickNumber(18 * TicksPerSecond), "22. 3 Tomahawks derrubam a Patriot com energia em ate 18 s");
			Check(LauncherHp == 600, "22. os lancadores nao perdem nada");
			Check(Closest > Fix::FromMilli(63200), "22. os lancadores nunca chegam a 63,2 m da Patriot (o alcance dela)");
		}
		// 23. 2 Crusaders colados (15 m, dentro do mínimo) contra 1 Tomahawk: o lançador não atira e cai; os tanques não perdem nada
		{
			MatchSetup S = RankSetup();
			const Vec2 Mid = S.Map.CellCenter(Cell{ 48, 48 });
			S.Players[0].Units = { { "usa_tomahawk", Mid, Angle() } };
			S.Players[1].Units = { { "usa_mbt", Offset(Mid, 15, -5), Angle::FromBam(Angle::HalfTurn) }, { "usa_mbt", Offset(Mid, 15, 5), Angle::FromBam(Angle::HalfTurn) } };
			World W;
			Check(W.Init(S), "montagem do Tomahawk cercado");
			const EntityId Tom = FindOf(W, 0, DefKey(W, "usa_tomahawk"));
			int32_t Shots = 0;
			int32_t TankDamage = 0;
			const TickNumber Died = StepUntil(W, 30 * TicksPerSecond, [&](TickNumber)
			{
				for (const SimEvent& E : W.LastEvents())
				{
					Shots += E.Type == EventType::WeaponFired && E.Source == Tom ? 1 : 0;
					TankDamage +=E.Type == EventType::UnitDamaged && E.Target != Tom ? E.Value : 0;
				}
				return HasEvent(W, EventType::UnitDied, NoEntity, Tom);
			});
			std::printf("  23. o Tomahawk cercado caiu em %.1f s; tiros dele %d; dano nos tanques %d\n", double(Died) / TicksPerSecond, Shots, TankDamage);
			Check(Died > 0 && Shots == 0 && TankDamage == 0, "23. 2 Crusaders colados matam o Tomahawk sem perder nada");
		}
	}

	// ------------------------------------------------------------------ (v4, 23/10) vender, consertar, a linha espalhada, o
	// caminhão que derruba e a mira do lançador (os números do ZH e do spec, no EconomyRuleset)

	/** A base do jogador 0 pra vender e consertar: CC, usina, War Factory e Patriot prontos (energia 5 de 4), 2 Dozers e 4
	 *  tanques; o inimigo 1 com o CC longe. Sem névoa. */
	MatchSetup SellBaseSetup(Vec2& OutBase)
	{
		MatchSetup S = RankSetup();
		OutBase = S.Map.CellCenter(Cell{ 40, 40 });
		S.Players[0].Units = { { "usa_command_center", OutBase, Angle() }, { "usa_power_plant", Offset(OutBase, -40, 0), Angle() },
			{ "usa_war_factory", Offset(OutBase, 0, -50), Angle() }, { "usa_patriot", Offset(OutBase, 40, 30), Angle() },
			{ "usa_dozer", Offset(OutBase, 30, -20), Angle() }, { "usa_dozer", Offset(OutBase, 30, -32), Angle() } };
		for (int32_t K = 0; K < 4; ++K)
		{
			S.Players[0].Units.push_back({ "usa_mbt", Offset(OutBase, -60 + K * 9, 30), Angle() });
		}
		S.Players[1].Units = { { "usa_command_center", S.Map.CellCenter(Cell{ 85, 85 }), Angle() } };
		return S;
	}

	std::vector<EntityId> AllOf(const World& W, PlayerIndex P, uint16_t Def)
	{
		std::vector<EntityId> Out;
		for (int32_t I = 0; I < W.UnitCount(); ++I)
		{
			const UnitView V = W.Unit(I);
			if (V.Owner == P && V.Def == Def)
			{
				Out.push_back(V.Id);
			}
		}
		return Out;
	}

	int32_t HpOf(const World& W, EntityId Id)
	{
		UnitView V;
		return W.FindUnit(Id, V) ? V.Hp : 0;
	}

	/** [vender] O BuildAssistant::sellObject do ZH: a fila volta na hora, a energia sai já, a desmontagem leva 6 s e no fim
	 *  voltam 50% do custo, sem explosão; destruído no meio da venda, nada volta. Os comandos vão e voltam em bytes. */
	void TestSell()
	{
		std::printf("[vender]\n");
		Vec2 Base;
		const MatchSetup S0 = SellBaseSetup(Base);
		World W;
		Check(W.Init(S0), "vender: a montagem");
		const EntityId WF = FindOf(W, 0, DefKey(W, "usa_war_factory"));
		const EntityId EnemyCC = FindOf(W, 1, DefKey(W, "usa_command_center"));
		const uint16_t Mbt = DefKey(W, "usa_mbt");
		StepWith(W, { Order(0, CommandType::Produce, WF, Mbt), Order(0, CommandType::Produce, WF, Mbt) });
		const int32_t MoneyQueued = W.Money(0);
		const int32_t UsedBefore = W.Player(0).PowerUsed;
		UnitView WV;
		W.FindUnit(WF, WV);
		Check(W.CanSell(0, WF) && !W.CanSell(1, WF) && !W.CanSell(0, EnemyCC), "vender: so o proprio predio pronto");
		Check(W.SellRefund(WF) == 1000, "vender: a War Factory de $2 000 devolve $1 000 (o SellPercentage de 50% do ZH)");
		const TickNumber SellAt = W.Tick();
		StepWith(W, { Order(0, CommandType::Sell, WF) });
		const bool bSellingEvent = HasEvent(W, EventType::StructureSelling, NoEntity, WF);
		StepWith(W, {});
		UnitView Selling;
		W.FindUnit(WF, Selling);
		Check(bSellingEvent && Selling.SellLeft > 0 && Selling.SellTicks == W.Rules().Economy.SellTicks && Selling.QueueSize == 0,
			"vender: comeca a desmontar e a fila sai");
		Check(W.Money(0) == MoneyQueued + 1800, "vender: os 2 tanques da fila voltam na hora ($1 800)");
		Check(W.Player(0).PowerUsed == UsedBefore - 1, "vender: a fabrica sai da energia ja no comeco");
		Check(!W.CanSell(0, WF), "vender: o predio sendo vendido nao vende de novo");
		const int32_t MoneyBefore = W.Money(0);
		StepWith(W, { Order(0, CommandType::Sell, WF) });   // de novo: nada muda
		bool bSold = false;
		bool bExploded = false;
		int32_t SoldValue = 0;
		TickNumber SoldAt = 0;
		RunUntil(W, 10 * TicksPerSecond, [&]()
		{
			for (const SimEvent& E : W.LastEvents())
			{
				if (E.Target == WF && E.Type == EventType::StructureSold)
				{
					bSold = true;
					SoldValue = E.Value;
					SoldAt = W.Tick() - 1;
				}
				bExploded |= E.Target == WF && E.Type == EventType::UnitDied;
			}
			return bSold;
		});
		UnitView Gone;
		const Cell WC = W.Map().CellOf(WV.Pos);
		std::printf("  a War Factory vendida em %.2f s, $%d de volta, a celula do meio %d\n", double(SoldAt - SellAt + 1) / TicksPerSecond, SoldValue & 0xFFFFFF,
			int32_t(W.Map().PassAt(WC.X, WC.Y)));
		Check(bSold && !bExploded && !W.FindUnit(WF, Gone), "vender: no fim ela sai sem explosao");
		Check(SoldAt - SellAt + 1 == TickNumber(W.Rules().Economy.SellTicks), "vender: a desmontagem leva os 6 s do ZH (1,5 + 3 + 1,5)");
		Check((SoldValue & 0xFFFFFF) == 1000 && (SoldValue >> 24) == 0 && W.Money(0) == MoneyBefore + 1000, "vender: no fim voltam $1 000 pro dono");
		Check(W.Map().PassAt(WC.X, WC.Y) == CellPass::Open, "vender: a grade abre onde ela estava");

		// destruída no meio da venda (a desmontagem longa aqui: 20 s), nada volta e ela explode como sempre
		{
			MatchSetup S = SellBaseSetup(Base);
			S.Rules.Economy.SellTicks = 20 * TicksPerSecond;
			World W2;
			Check(W2.Init(S), "vender: a montagem do destruido no meio");
			const EntityId Plant = FindOf(W2, 0, DefKey(W2, "usa_power_plant"));
			const std::vector<EntityId> Tanks = AllOf(W2, 0, DefKey(W2, "usa_mbt"));
			StepWith(W2, { Order(0, CommandType::Sell, Plant) });
			const int32_t Money = W2.Money(0);
			StepWith(W2, { Group(0, Tanks), Order(0, CommandType::ForceAttack, Plant) });
			bool bDied = false;
			bool bSoldToo = false;
			RunUntil(W2, 19 * TicksPerSecond, [&]()
			{
				bDied |= HasEvent(W2, EventType::UnitDied, NoEntity, Plant);
				bSoldToo |= HasEvent(W2, EventType::StructureSold, NoEntity, Plant);
				return bDied;
			});
			Check(bDied && !bSoldToo && W2.Money(0) == Money, "vender: destruido no meio da venda, nada volta");
		}
		// os comandos novos vão e voltam em bytes (a rede e o replay)
		for (const CommandType T : { CommandType::Sell, CommandType::Repair })
		{
			Command C;
			C.Tick = 77;
			C.Player = 1;
			C.Type = T;
			C.Target = 4242;
			ByteWriter Wb;
			WriteCommand(Wb, C);
			ByteReader Rb(Wb.Bytes.data(), Wb.Bytes.size());
			Command Back;
			Check(ReadCommand(Rb, Back) && Back.Type == T && Back.Target == 4242 && Back.Tick == 77 && Back.Player == 1 && Rb.AtEnd(), "vender: Sell e Repair ida e volta em bytes");
		}
	}

	/** [conserto do Dozer] O DozerAIUpdate do ZH: o Dozer vai até o prédio avariado e devolve 2% da vida cheia por segundo, de
	 *  graça, até encher; um Dozer por prédio; a ordem de andar larga o conserto; o prédio cheio, o inimigo e o que está sendo
	 *  vendido não se consertam. */
	void TestRepair()
	{
		std::printf("[conserto do Dozer]\n");
		Vec2 Base;
		World W;
		Check(W.Init(SellBaseSetup(Base)), "conserto: a montagem");
		const EntityId Plant = FindOf(W, 0, DefKey(W, "usa_power_plant"));
		const EntityId CC = FindOf(W, 0, DefKey(W, "usa_command_center"));
		const EntityId EnemyCC = FindOf(W, 1, DefKey(W, "usa_command_center"));
		const std::vector<EntityId> Dozers = AllOf(W, 0, DefKey(W, "usa_dozer"));
		const std::vector<EntityId> Tanks = AllOf(W, 0, DefKey(W, "usa_mbt"));
		const int32_t MaxHp = W.Rules().Units[DefKey(W, "usa_power_plant")].MaxHp;
		Check(Dozers.size() == 2 && Tanks.size() == 4, "conserto: 2 Dozers e 4 tanques");
		Check(!W.CanRepair(0, Plant) && !W.CanRepair(0, CC), "conserto: o predio cheio nao se conserta");
		// os tanques atiram na própria usina (o Ctrl) até metade da vida, e param
		StepWith(W, { Group(0, Tanks), Order(0, CommandType::ForceAttack, Plant) });
		RunUntil(W, 30 * TicksPerSecond, [&]() { return HpOf(W, Plant) <= MaxHp / 2; });
		StepWith(W, { Order(0, CommandType::Stop) });
		RunUntil(W, TicksPerSecond, []() { return false; });
		const int32_t HpStart = HpOf(W, Plant);
		Check(HpStart > 0 && HpStart <= MaxHp / 2 && W.CanRepair(0, Plant) && !W.CanRepair(1, Plant) && !W.CanRepair(0, EnemyCC),
			"conserto: a usina avariada se conserta (pelo dono; o inimigo nao)");
		const int32_t Money = W.Money(0);
		StepWith(W, { Group(0, { Dozers[0] }), Order(0, CommandType::Repair, Plant) });
		UnitView D0;
		W.FindUnit(Dozers[0], D0);
		Check(D0.Task == UnitTask::Repair && D0.TaskTarget == Plant, "conserto: o Dozer pega o conserto");
		// o 2º Dozer na mesma usina: um por prédio (o sole benefactor do ZH)
		StepWith(W, { Group(0, { Dozers[1] }), Order(0, CommandType::Repair, Plant) });
		UnitView D1;
		W.FindUnit(Dozers[1], D1);
		Check(D1.Task == UnitTask::None, "conserto: o 2o Dozer nao pega a usina que o 1o ja conserta");
		TickNumber FirstHeal = 0;
		int32_t HpFirst = 0;
		int32_t LastHp = HpOf(W, Plant);
		const TickNumber Full = StepUntil(W, 90 * TicksPerSecond, [&](TickNumber T)
		{
			const int32_t Hp = HpOf(W, Plant);
			if (FirstHeal == 0 && Hp > LastHp)
			{
				FirstHeal = T;
				HpFirst = Hp;
			}
			LastHp = Hp;
			return Hp >= MaxHp;
		});
		W.FindUnit(Dozers[0], D0);
		const double Rate = Full > FirstHeal ? double(MaxHp - HpFirst) * TicksPerSecond / double(Full - FirstHeal) : 0.0;
		std::printf("  usina de %d a %d PV: o conserto comecou %.1f s depois da ordem e encheu em %.1f s (%.2f PV/s; o ZH: 2%% de %d = %.1f)\n",
			HpStart, MaxHp, double(FirstHeal) / TicksPerSecond, double(Full - FirstHeal) / TicksPerSecond, Rate, MaxHp, MaxHp * 0.02);
		Check(Full > 0 && HpOf(W, Plant) == MaxHp, "conserto: a usina enche");
		Check(Rate > MaxHp * 0.02 * 0.98 && Rate < MaxHp * 0.02 * 1.02, "conserto: 2% da vida cheia por segundo (o RepairHealthPercentPerSecond do ZH)");
		Check(W.Money(0) == Money, "conserto: de graca (o ZH nao cobra)");
		Check(D0.Task == UnitTask::None, "conserto: cheia, o Dozer larga");
		// a ordem de andar larga o conserto; o 2º Dozer pega depois que o 1º larga
		StepWith(W, { Group(0, Tanks), Order(0, CommandType::ForceAttack, Plant) });
		RunUntil(W, 20 * TicksPerSecond, [&]() { return HpOf(W, Plant) <= MaxHp * 3 / 4; });
		StepWith(W, { Order(0, CommandType::Stop) });
		RunUntil(W, TicksPerSecond, []() { return false; });
		const int32_t HpDamaged = HpOf(W, Plant);
		StepWith(W, { Group(0, { Dozers[0] }), Order(0, CommandType::Repair, Plant) });
		RunUntil(W, 15 * TicksPerSecond, [&]() { return HpOf(W, Plant) >= HpDamaged + 10; });
		StepWith(W, { Group(0, { Dozers[0] }), Order(0, CommandType::Move, NoEntity, 0, Offset(Base, 60, -60)) });
		const int32_t HpLeft = HpOf(W, Plant);
		RunUntil(W, 3 * TicksPerSecond, []() { return false; });
		W.FindUnit(Dozers[0], D0);
		Check(D0.Task == UnitTask::None && HpOf(W, Plant) == HpLeft, "conserto: a ordem de andar larga o conserto");
		StepWith(W, { Group(0, { Dozers[1] }), Order(0, CommandType::Repair, Plant) });
		W.FindUnit(Dozers[1], D1);
		Check(D1.Task == UnitTask::Repair, "conserto: com o 1o fora, o 2o Dozer pega");
		// vendida no meio do conserto: o Dozer larga
		StepWith(W, { Order(0, CommandType::Sell, Plant) });
		StepWith(W, {});
		W.FindUnit(Dozers[1], D1);
		Check(D1.Task == UnitTask::None && !W.CanRepair(0, Plant), "conserto: a usina vendida nao se conserta (o Dozer larga)");
	}

	/** [linha espalhada] A linha dos dois botões espalha pelo comprimento puxado (o BAR e o Total War): menos unidades que a
	 *  linha comporta = uma fileira de ponta a ponta; mais = fileiras cheias de ponta a ponta, nunca abaixo do passo mínimo, e a
	 *  de trás no meio. A prévia é a vaga da ordem, bit a bit. */
	void TestLineSpread()
	{
		std::printf("[linha espalhada]\n");
		struct FCase
		{
			const char* Name;
			int32_t Tanks;
			int32_t WidthHalfM;
			double WantWidth;   // a largura da fileira da frente (m)
			int32_t WantRows;
		};
		const double Step = 3.6 * 2.34;   // o passo mínimo do tanque (2,34 raios)
		const FCase Cases[] = {
			{ "3 tanques numa linha de 60 m", 3, 30, 60.0, 1 },
			{ "10 tanques numa linha de 100 m", 10, 50, 100.0, 1 },
			{ "6 tanques numa linha de 30 m", 6, 15, 30.0, 2 },   // (revisão) cabem floor(30 / 8,42) + 1 = 4: 4 + 2, nunca além da ponta
			{ "8 tanques numa linha de 30 m", 8, 15, 30.0, 2 },
			{ "2 tanques numa linha de 126 m", 2, 63, 126.0, 1 },
			{ "3 tanques, arraste curto", 3, 0, 2 * Step, 1 },
		};
		for (const FCase& Case : Cases)
		{
			MatchSetup S;
			S.Map = MakeOpenMap(96, 96);
			S.bFogOfWar = false;
			S.Rules.Units.push_back(UnitDef::FromSpec(TankSpec()));
			PlayerSetup P0;
			for (int32_t K = 0; K < Case.Tanks; ++K)
			{
				P0.Units.push_back({ "usa_mbt", Vec2::Make(Fix::FromInt(-100 - (K / 4) * 9), Fix::FromInt((K % 4) * 9 - 13)), Angle() });
			}
			PlayerSetup P1;
			P1.Team = 1;
			P1.Units.push_back({ "usa_mbt", Vec2::Make(Fix::FromInt(170), Fix::FromInt(170)), Angle() });
			S.Players = { P0, P1 };
			World W;
			Check(W.Init(S), "linha: a montagem");
			std::vector<EntityId> Own;
			for (int32_t I = 0; I < W.UnitCount(); ++I)
			{
				if (W.Unit(I).Owner == 0)
				{
					Own.push_back(W.Unit(I).Id);
				}
			}
			const Vec2 Point = Vec2::Make(Fix::FromInt(20), Fix::Zero());
			const int32_t Value = MoveValue(false, true, Case.WidthHalfM, 0);   // de frente pra +X: a linha corre em Y
			std::vector<EntityId> PIds;
			std::vector<Vec2> PSlots;
			const bool bPreview = W.PreviewLineSlots(0, Own, Point, Value, PIds, PSlots);
			Command G = Group(0, Own);
			Command M = Order(0, CommandType::Move, NoEntity, Value, Point);
			StepWith(W, { G, M });
			int32_t Same = 0;
			double Front = -1e9;
			for (size_t K = 0; K < PIds.size(); ++K)
			{
				UnitView V;
				Same += W.FindUnit(PIds[K], V) && V.OrderPoint == PSlots[K] ? 1 : 0;
				Front = std::max(Front, PSlots[K].X.ToDouble());
			}
			double Lo = 1e9, Hi = -1e9;
			int32_t Rows = 0;
			std::vector<double> RowXs;
			double Outside = 0.0;   // o quanto a vaga mais de fora passa da ponta da linha puxada (a linha corre em Y, no meio em 0)
			for (const Vec2& P : PSlots)
			{
				Outside = Case.WidthHalfM > 0 ? std::max(Outside, std::abs(P.Y.ToDouble()) - Case.WidthHalfM) : 0.0;
				if (std::abs(P.X.ToDouble() - Front) < 0.01)
				{
					Lo = std::min(Lo, P.Y.ToDouble());
					Hi = std::max(Hi, P.Y.ToDouble());
				}
				if (std::none_of(RowXs.begin(), RowXs.end(), [&](double X) { return std::abs(X - P.X.ToDouble()) < 0.01; }))
				{
					RowXs.push_back(P.X.ToDouble());
					++Rows;
				}
			}
			std::printf("  %s: fileira da frente de %.1f m (quer %.1f), %d fileira(s), par mais perto a %.1f m, %.2f m alem da ponta, previa = ordem em %d de %zu\n",
				Case.Name, Hi - Lo, Case.WantWidth, Rows, PSlots.size() > 1 ? MinGap(PSlots) : 0.0, Outside, Same, PSlots.size());
			Check(bPreview && Same == int32_t(Own.size()) && PSlots.size() == Own.size(), "linha: a previa e a vaga da ordem, bit a bit");
			Check(std::abs((Hi - Lo) - Case.WantWidth) < 0.05, "linha: a fileira da frente vai de ponta a ponta da linha puxada");
			Check(Outside < 0.01, "linha: nenhuma vaga passa da ponta da linha puxada (a de tras desencontrada tambem)");
			Check(Rows == Case.WantRows, "linha: as fileiras certas (menos unidades que a linha comporta: uma so)");
			Check(MinGap(PSlots) > Step - 0.05, "linha: nunca abaixo do passo minimo (2,34 raios)");
		}
	}

	/** [caminhao derruba] O choque (½·m·v²) contra a energia que a palmeira e o poste absorvem: o caminhão de 15 t e o Tomahawk
	 *  de 30 t derrubam na velocidade de cruzeiro e não derrubam devagar; o caminhão passa pela palmeira (a célula da árvore abre
	 *  pra quem a derruba) e continua contornando o carro (a roda não sobe nele). */
	void TestTruckTopple()
	{
		std::printf("[caminhao derruba]\n");
		const Ruleset R = EconomyRuleset();
		const UnitDef& Truck = R.Units[size_t(R.FindUnit("usa_supply_truck"))];
		const UnitDef& Tom = R.Units[size_t(R.FindUnit("usa_tomahawk"))];
		const auto Mps = [](int32_t Milli) { return PerTickFromMilliPerSecond(Milli); };
		Check(KnocksOver(Truck, Truck.MaxSpeed, PoleToppleEnergyJ) && KnocksOver(Truck, Truck.MaxSpeed, TreeToppleEnergyJ), "derrubar: o caminhao em cruzeiro derruba o poste e a palmeira");
		Check(KnocksOver(Truck, Mps(2500), PoleToppleEnergyJ) && !KnocksOver(Truck, Mps(2500), TreeToppleEnergyJ), "derrubar: o caminhao a 2,5 m/s parte o poste e nao a palmeira");
		Check(!KnocksOver(Truck, Mps(2000), PoleToppleEnergyJ), "derrubar: o caminhao a 2 m/s nao parte o poste (30 kJ)");
		Check(KnocksOver(Tom, Tom.MaxSpeed, TreeToppleEnergyJ) && !KnocksOver(Tom, Mps(1500), PoleToppleEnergyJ), "derrubar: o Tomahawk derruba em cruzeiro e nao a 1,5 m/s");
		Check(!KnocksOver(UnitDef::FromSpec(TankSpec()), Fix::FromInt(1), TreeToppleEnergyJ), "derrubar: sem massa nos dados, nunca pelo choque");
		const CrushRun A = RunCrush("usa_supply_truck", true, false);
		const CrushRun B = RunCrush("usa_supply_truck", true, false);
		const int32_t Off = AbsInt(Angle().DeltaTo(A.TreeFall));
		std::printf("  caminhao x palmeira: derrubada %d, cai a %.0f graus do rumo, chegou %s\n", int(A.TreeState), Off * 360.0 / 65536.0, A.bArrived ? "sim" : "nao");
		Check(A.TreeState == 1 && A.bArrived && Off <= 8192, "derrubar: o caminhao passa pela palmeira e ela cai pra frente dele");
		Check(A.Hashes == B.Hashes, "derrubar: roda igual duas vezes, passo a passo");
		const CrushRun L = RunCrush("usa_tomahawk", true, false);
		Check(L.TreeState == 1 && L.bArrived, "derrubar: o Tomahawk tambem derruba a palmeira");
		const CrushRun C = RunCrush("usa_supply_truck", false, false);
		Check(C.CarState == 0 && !C.bEnteredCar && C.bArrived, "derrubar: o caminhao segue contornando o carro de pe");
		// (revisão de 23/10) o toque lento: o caminhão parado (velocidade 0) a 0,6 m do tronco, empurrado de trás por outro
		// caminhão que passa pela palmeira (a célula aberta pra quem a derruba). Pela velocidade da hora ele não derrubava e o
		// casco entrava no tronco de pé; agora derruba no toque, e nenhum casco cobre o tronco enquanto ela está de pé
		{
			MatchSetup S;
			S.Seed = 3;
			S.Map = MakeOpenMap(64, 64);
			S.Rules = EconomyRuleset();
			S.bFogOfWar = false;
			const Vec2 Mid = S.Map.CellCenter(Cell{ 32, 32 });
			PropSetup P;
			P.Kind = PropKind::Tree;
			P.Pos = Mid;
			P.HalfLength = Fix::FromMilli(400);
			P.HalfWidth = Fix::FromMilli(400);
			P.Height = Fix::FromInt(10);
			S.Props.push_back(P);
			const double Front = Truck.HullHalfLength.ToDouble() + 0.4 + 0.6;   // o meio do parado: a frente a 0,6 m do tronco
			PlayerSetup P0;
			P0.Units.push_back({ "usa_supply_truck", Vec2::Make(Mid.X - Fix::FromMilli(int64_t(Front * 1000.0)), Mid.Y), Angle() });
			P0.Units.push_back({ "usa_supply_truck", Vec2::Make(Mid.X - Fix::FromMilli(int64_t((Front + 12.0) * 1000.0)), Mid.Y), Angle() });
			PlayerSetup P1;
			P1.Team = 1;
			P1.Units.push_back({ "usa_mbt", Offset(Mid, 100, 100), Angle() });
			S.Players = { P0, P1 };
			World W;
			Check(W.Init(S), "derrubar: a montagem do empurrado");
			const EntityId Parked = W.Unit(0).Id;
			const EntityId Pusher = W.Unit(1).Id;
			StepWith(W, { Group(0, { Pusher }), Order(0, CommandType::Move, NoEntity, 0, Offset(Mid, 25, 0)) });
			bool bClipped = false;
			Fix ParkedSpeedAtFall = Fix::FromInt(-1);
			RunUntil(W, 10 * TicksPerSecond, [&]()
			{
				const World::PropView Pv = W.PropAt(0);
				for (const EntityId Id : { Parked, Pusher })
				{
					UnitView V;
					if (Pv.State == 0 && W.FindUnit(Id, V))
					{
						// o tronco dentro da caixa do casco, com o tronco de pé
						const Vec2 Fwd = Vec2::FromAngle(V.Hull);
						const Vec2 Rel = Mid - V.Pos;
						const Fix Along = Rel.Dot(Fwd);
						const Fix Side = Rel.X * (-Fwd.Y) + Rel.Y * Fwd.X;
						bClipped |= Abs(Along) < Truck.HullHalfLength && Abs(Side) < Truck.HullHalfWidth;
					}
				}
				UnitView V;
				if (Pv.State != 0 && ParkedSpeedAtFall.Raw < 0 && W.FindUnit(Parked, V))
				{
					ParkedSpeedAtFall = V.Speed;
				}
				return Pv.State != 0;
			});
			const World::PropView Pv = W.PropAt(0);
			std::printf("  caminhao parado empurrado contra a palmeira: derrubada %d por %s (a %.1f m/s), casco dentro do tronco de pe: %s\n", int(Pv.State),
				Pv.By == Parked ? "o parado" : (Pv.By == Pusher ? "o que empurra" : "ninguem"), ParkedSpeedAtFall.ToDouble() * TicksPerSecond, bClipped ? "sim" : "nao");
			Check(Pv.State == 1 && Pv.By == Parked && !bClipped, "derrubar: o caminhao empurrado devagar derruba a palmeira no toque (nunca o casco dentro do tronco de pe)");
		}
	}

	/** [mira do lancador] O AIM do TurretAI do ZH com o PreAttackDelay: o relógio conta desde o alvo no alcance (não do
	 *  alinhamento), o tiro sai alinhado e com o relógio cheio (1 s), o 2º da rajada sem esperar de novo; sem alvo o relógio
	 *  segura 2 s e desce à metade; o míssil nasce na boca do tubo levantado, alternando os tubos. */
	void TestLauncherAim()
	{
		std::printf("[mira do lancador]\n");
		Vec2 Mid;
		MatchSetup S = LauncherSetup(Mid);   // a mira de 1 s do Units.json (pre_attack_s), a mesma do EconomyRuleset
		World W;
		Check(W.Init(S), "mira: a montagem");
		const EntityId Tom = FindOf(W, 0, DefKey(W, "usa_tomahawk"));
		const WeaponDef& Wp = W.Rules().Units[DefKey(W, "usa_tomahawk")].Weapon;
		Check(Wp.PreAttackTicks == TicksPerSecond && Wp.MissileLaunchPitch.Bam == Angle::FromMilliDeg(45000).Bam, "mira: 1 s de mira e o tubo a 45 graus");
		const TickNumber OrderAt = W.Tick();
		StepWith(W, { Group(0, { Tom }), Order(0, CommandType::AttackGround, NoEntity, 0, Offset(Mid, 60, 0)) });
		std::vector<TickNumber> Shots;
		std::vector<World::ProjectileView> Born;
		int32_t AimAtShot = -1;
		StepUntil(W, 5 * TicksPerSecond, [&](TickNumber T)
		{
			for (const SimEvent& E : W.LastEvents())
			{
				if (E.Type == EventType::WeaponFired && E.Source == Tom)
				{
					Shots.push_back(T);
					for (int32_t K = 0; K < W.ProjectileCount(); ++K)
					{
						if (W.ProjectileAt(K).Id == uint32_t(E.Value))
						{
							Born.push_back(W.ProjectileAt(K));
						}
					}
					UnitView V;
					AimAtShot = Shots.size() == 1 && W.FindUnit(Tom, V) ? V.AimTicks : AimAtShot;
				}
			}
			return Shots.size() >= 2;
		});
		std::printf("  tiro no chao a 60 m, a torre ja virada: 1o tiro %.2f s depois da ordem (relogio em %d), 2o %.2f s depois do 1o\n",
			Shots.empty() ? -1.0 : double(Shots[0] - OrderAt + 1) / TicksPerSecond, AimAtShot,
			Shots.size() < 2 ? -1.0 : double(Shots[1] - Shots[0]) / TicksPerSecond);
		Check(Shots.size() == 2 && Shots[0] - OrderAt + 1 == TickNumber(Wp.PreAttackTicks) && AimAtShot == Wp.PreAttackTicks, "mira: o 1o tiro sai com o relogio cheio (1 s)");
		Check(Shots.size() == 2 && Shots[1] - Shots[0] == TickNumber(Wp.ReloadTicks), "mira: o 2o da rajada nao espera de novo");
		// a boca do tubo: o pivô 1,296 m atrás do meio, a boca 0,463 m à frente dele e ±0,302 m de lado, a 3,29 m do chão
		bool bMouth = Born.size() == 2;
		for (size_t K = 0; K < Born.size(); ++K)
		{
			const double Side = K == 0 ? -0.302 : 0.302;
			const double DX = Born[K].Pos.X.ToDouble() - (Mid.X.ToDouble() - 1.296 + 0.463);
			const double DY = Born[K].Pos.Y.ToDouble() - (Mid.Y.ToDouble() + Side);
			const double DZ = (Born[K].Z - W.GroundHeightAt(Mid)).ToDouble() - 3.29;
			std::printf("  missil %zu: nasceu a (%.3f, %.3f, %.3f) m da boca esperada, inclinado %.1f graus\n", K + 1, DX, DY, DZ, Born[K].Pitch.ToDegrees());
			bMouth &= std::abs(DX) < 0.002 && std::abs(DY) < 0.002 && std::abs(DZ) < 0.002 && std::abs(Born[K].Pitch.ToDegrees() - 45.0) < 0.1;
		}
		Check(bMouth, "mira: o missil nasce na boca do tubo levantado, alternando os tubos, a 45 graus");
		// sem alvo: segura 2 s (o RecenterTime do ZH) e desce 1 a cada 2 passos; o alvo que volta sobe dali
		StepWith(W, { Order(0, CommandType::Stop) });
		RunUntil(W, 2 * TicksPerSecond - 1, []() { return false; });
		UnitView V;
		W.FindUnit(Tom, V);
		const int32_t AfterHold = V.AimTicks;
		RunUntil(W, 20, []() { return false; });
		W.FindUnit(Tom, V);
		const int32_t AfterDecay = V.AimTicks;
		StepWith(W, { Order(0, CommandType::AttackGround, NoEntity, 0, Offset(Mid, 60, 0)) });
		RunUntil(W, 4, []() { return false; });
		W.FindUnit(Tom, V);
		std::printf("  sem alvo: %d depois de 2 s, %d depois de mais 20 passos; o alvo de volta: %d em 5 passos\n", AfterHold, AfterDecay, V.AimTicks);
		Check(AfterHold == Wp.PreAttackTicks && AfterDecay == Wp.PreAttackTicks - 10, "mira: sem alvo o relogio segura 2 s e desce a metade da velocidade");
		Check(V.AimTicks == Wp.PreAttackTicks - 10 + 5, "mira: o alvo de volta, o relogio sobe de onde estava");
		// o alvo de lado (90°): a torre gira (90°/s) enquanto o relógio conta; o tiro sai em ~1 s, não em 2
		{
			World W2;
			W2.Init(S);
			const EntityId Tom2 = FindOf(W2, 0, DefKey(W2, "usa_tomahawk"));
			const TickNumber At2 = W2.Tick();
			StepWith(W2, { Group(0, { Tom2 }), Order(0, CommandType::AttackGround, NoEntity, 0, Offset(Mid, 0, 60)) });
			const TickNumber First = StepUntil(W2, 5 * TicksPerSecond, [&](TickNumber) { return HasEvent(W2, EventType::WeaponFired, Tom2, NoEntity); });
			std::printf("  alvo a 90 graus: 1o tiro %.2f s depois da ordem\n", double(First - At2 + 1) / TicksPerSecond);
			Check(First > 0 && First - At2 + 1 <= TickNumber(Wp.PreAttackTicks + 2), "mira: o relogio conta junto com o giro da torre (nao depois dele)");
		}
	}

	/** [quem atira segura o lugar] (revisão de 23/10) O Pathfinder::moveAllies do ZH não mexe no aliado que está atacando ("Don't
	 *  move units that are attacking"): o tanque atirando parado não é empurrado pelo aliado que passa (de frente, o pior caso),
	 *  e o que passa contorna e chega; o parado à toa continua cedendo o lugar, como antes. */
	void TestHoldGround()
	{
		std::printf("[quem atira segura o lugar]\n");
		struct FRun
		{
			double MaxShift = 0.0;   // o quanto o parado saiu do lugar (m)
			bool bArrived = false;
			int32_t Shots = 0;
		};
		const auto Run = [](bool bFiring)
		{
			FRun Out;
			MatchSetup S;
			S.Map = MakeOpenMap(96, 96);
			S.bFogOfWar = false;
			S.Rules.Units.push_back(UnitDef::FromSpec(TankSpec()));
			const Vec2 Mid = S.Map.CellCenter(Cell{ 48, 48 });
			PlayerSetup P0;
			P0.Units.push_back({ "usa_mbt", Mid, Angle() });
			P0.Units.push_back({ "usa_mbt", Offset(Mid, -20, 0), Angle() });
			PlayerSetup P1;
			P1.Team = 1;
			P1.Units.push_back({ "usa_mbt", Offset(Mid, 170, 170), Angle() });
			S.Players = { P0, P1 };
			World W;
			if (!W.Init(S))
			{
				Check(false, "segura: a montagem");
				return Out;
			}
			const EntityId Holder = W.Unit(0).Id;
			const EntityId Mover = W.Unit(1).Id;
			if (bFiring)
			{
				StepWith(W, { Group(0, { Holder }), Order(0, CommandType::AttackGround, NoEntity, 0, Offset(Mid, 0, 30)) });   // o tiro de lado
			}
			const Vec2 Goal = Offset(Mid, 20, 0);
			StepWith(W, { Group(0, { Mover }), Order(0, CommandType::Move, NoEntity, 0, Goal) });   // de frente, pelo meio do parado
			RunUntil(W, 10 * TicksPerSecond, [&]()
			{
				UnitView H;
				UnitView M;
				if (!W.FindUnit(Holder, H) || !W.FindUnit(Mover, M))
				{
					return true;
				}
				Out.MaxShift = std::max(Out.MaxShift, std::hypot(H.Pos.X.ToDouble() - Mid.X.ToDouble(), H.Pos.Y.ToDouble() - Mid.Y.ToDouble()));
				Out.Shots += HasEvent(W, EventType::WeaponFired, Holder, NoEntity) ? 1 : 0;
				Out.bArrived = DistSqRaw(M.Pos, Goal) <= RadiusSqRaw(Fix::FromInt(3)) && M.Speed.Raw == 0;
				return Out.bArrived;
			});
			return Out;
		};
		const FRun Firing = Run(true);
		const FRun Idle = Run(false);
		std::printf("  atirando parado: saiu %.3f m do lugar, %d tiros, o aliado chegou %s; a toa: saiu %.2f m, o aliado chegou %s\n",
			Firing.MaxShift, Firing.Shots, Firing.bArrived ? "sim" : "nao", Idle.MaxShift, Idle.bArrived ? "sim" : "nao");
		Check(Firing.MaxShift < 0.01 && Firing.Shots > 0, "segura: o tanque atirando parado nao e empurrado pelo aliado que passa");
		Check(Firing.bArrived, "segura: o aliado contorna quem atira (de frente, pela batida torta) e chega");
		Check(Idle.MaxShift > 0.5 && Idle.bArrived, "segura: o parado a toa continua cedendo o lugar");
	}
}

namespace
{
	// ------------------------------------------------------------------ a IA da v4 (23/10)

	/** [ia] 24. Robô × robô com o elenco novo (a zona, o Tomahawk, o Cruise Missile Strike), até 20 min: cada IA monta a zona,
	 *  usa o poder do general e faz o Tomahawk (pela patente), as decisões saem no log dela, e nenhuma perde mais de 2
	 *  fundações do mesmo tipo numa janela de 5 min. Duas vezes: a mesma impressão no fim. */
	void TestAIv4()
	{
		std::printf("[ia v4]\n");
		MatchSetup S = MakeEconomyMatch(21);
		S.Rules = EconomyRuleset(true);
		const TickNumber Cap = TickNumber(20 * 60 * TicksPerSecond);
		struct AIRun
		{
			int32_t Zones[2] = {};
			int32_t Strikes[2] = {};
			int32_t Tomahawks[2] = {};
			int32_t PeakRank[2] = { 1, 1 };
			int32_t WorstLost = 0;
			int32_t ZoneLogs = 0;
			int32_t StrikeLogs = 0;
			int32_t ArtilleryLogs = 0;
			TickNumber End = 0;
			int32_t Winner = -1;
			uint64_t Hash = 0;
		};
		const auto Play = [&]()
		{
			AIRun Out;
			World W;
			if (!W.Init(S))
			{
				Check(false, "montagem da partida da IA v4");
				return Out;
			}
			const Ruleset& R = W.Rules();
			CommandScheduler Queue(1);
			std::vector<SkirmishAI> Brains = { SkirmishAI(0), SkirmishAI(1) };
			std::map<EntityId, std::pair<int32_t, int32_t>> Open;   // a fundação inacabada → (dono, tipo)
			std::map<std::vector<int32_t>, int32_t> Lost;           // (dono, tipo, janela de 5 min) → perdidas
			std::vector<std::string> Lines;
			while (W.Tick() < Cap && !W.IsMatchOver())
			{
				for (SkirmishAI& B : Brains)
				{
					std::vector<Command> Orders;
					B.Think(W, Orders);
					for (const Command& C : Orders)
					{
						Queue.Issue(C, W.Tick());
					}
					B.TakeLog(Lines);
				}
				const TickNumber T = W.Tick();
				W.Step(Queue.TakeFor(T));
				for (const SimEvent& E : W.LastEvents())
				{
					UnitView V;
					if (E.Type == EventType::StructurePlaced && W.FindUnit(E.Target, V))
					{
						Open[E.Target] = { int32_t(V.Owner), int32_t(V.Def) };
						Out.Zones[V.Owner] += R.Units[V.Def].HasAirlift() ? 1 : 0;
					}
					if (E.Type == EventType::StructureCompleted || E.Type == EventType::StructureCancelled)
					{
						Open.erase(E.Target);   // a obra cancelada devolve o dinheiro: não é perdida
					}
					if (E.Type == EventType::UnitDied && Open.count(E.Target) > 0)
					{
						const std::pair<int32_t, int32_t> Site = Open[E.Target];
						Out.WorstLost = std::max(Out.WorstLost, ++Lost[{ Site.first, Site.second, int32_t(T / TickNumber(5 * 60 * TicksPerSecond)) }]);
						Open.erase(E.Target);
					}
					if (E.Type == EventType::PowerUsed && (E.Value & 0xFF) < 2)
					{
						++Out.Strikes[E.Value & 0xFF];
					}
					if (E.Type == EventType::UnitProduced && W.FindUnit(E.Target, V) && R.Units[V.Def].Weapon.MinRange.Raw > 0)
					{
						++Out.Tomahawks[V.Owner];
					}
				}
				for (int32_t P = 0; P < 2; ++P)
				{
					Out.PeakRank[P] = std::max(Out.PeakRank[P], W.Rank(PlayerIndex(P)));
				}
			}
			for (const std::string& L : Lines)
			{
				Out.ZoneLogs += L.find("drop zone at") != std::string::npos ? 1 : 0;
				Out.StrikeLogs += L.find("strike at") != std::string::npos ? 1 : 0;
				Out.ArtilleryLogs += L.find("usa_tomahawk queued") != std::string::npos ? 1 : 0;
			}
			Out.End = W.Tick();
			Out.Winner = W.WinningTeam();
			Out.Hash = W.StateHash();
			return Out;
		};
		const AIRun A = Play();
		std::printf("  %s em %.1f min (vencedor time %d): zonas %d e %d, ataques do poder %d e %d, Tomahawks %d e %d, patente maxima %d e %d; "
			"no log: %d zonas, %d ataques, %d Tomahawks; pior perda de fundacoes do mesmo tipo em 5 min: %d\n",
			A.Winner >= 0 ? "acabou" : "nao acabou", double(A.End) / (60.0 * TicksPerSecond), A.Winner, A.Zones[0], A.Zones[1], A.Strikes[0], A.Strikes[1],
			A.Tomahawks[0], A.Tomahawks[1], A.PeakRank[0], A.PeakRank[1], A.ZoneLogs, A.StrikeLogs, A.ArtilleryLogs, A.WorstLost);
		Check(A.Zones[0] >= 1 && A.Zones[1] >= 1, "24. as duas IAs montam a zona de lancamento");
		Check(A.PeakRank[0] >= 2 && A.PeakRank[1] >= 2 && A.Strikes[0] >= 1 && A.Strikes[1] >= 1, "24. as duas chegam na patente 2 e usam o Cruise Missile Strike");
		Check(A.PeakRank[0] >= 3 && A.PeakRank[1] >= 3 && A.Tomahawks[0] >= 1 && A.Tomahawks[1] >= 1, "24. as duas chegam na patente 3 e fazem o Tomahawk");
		Check(A.ZoneLogs >= 2 && A.StrikeLogs >= A.Strikes[0] + A.Strikes[1] && A.ArtilleryLogs >= 2, "24. a zona, o ataque e o Tomahawk saem no log da IA");
		Check(A.WorstLost <= 2, "24. nenhuma IA perde mais de 2 fundacoes do mesmo tipo em 5 min");
		const AIRun B = Play();
		Check(A.Hash == B.Hash && A.End == B.End, "24. a partida da IA v4 roda igual duas vezes");

		// (revisão de 23/10) a IA do jogador 0 sozinha (o 1 parado) até Cap ou o fim; Each(W) a cada passo; as linhas de log dela
		const auto Solo = [&](const MatchSetup& Setup, TickNumber Cap, auto&& Each, std::vector<std::string>& Lines)
		{
			World W;
			if (!W.Init(Setup))
			{
				Check(false, "montagem da IA sozinha");
				return;
			}
			CommandScheduler Queue(1);
			SkirmishAI Brain(0);
			while (W.Tick() < Cap && !W.IsMatchOver())
			{
				std::vector<Command> Orders;
				Brain.Think(W, Orders);
				for (const Command& C : Orders)
				{
					Queue.Issue(C, W.Tick());
				}
				Brain.TakeLog(Lines);
				W.Step(Queue.TakeFor(W.Tick()));
				Each(W);
			}
		};
		// 24b. Sem CC nem Dozer (ninguém ergue o centro), com usina, War Factory e $10 000: a fábrica faz tanque (antes guardava
		// pra sempre o dinheiro de um centro que nunca ia sair: 0 tanques em 4 min)
		{
			MatchSetup M = MakeEconomyMatch(21);
			M.Rules = EconomyRuleset(true);
			const Vec2 Base = M.Players[0].Units[0].Pos;
			const Angle Facing = M.Players[0].Units[0].Facing;
			M.Players[0].Units = { { "usa_power_plant", Offset(Base, -40, -30), Facing }, { "usa_war_factory", Base, Facing } };
			M.Players[1].Units = { { "usa_command_center", M.Players[1].Units[0].Pos, M.Players[1].Units[0].Facing } };
			int32_t Tanks = 0;
			std::vector<std::string> Lines;
			Solo(M, TickNumber(120 * TicksPerSecond), [&](const World& W)
			{
				for (const SimEvent& E : W.LastEvents())
				{
					UnitView V;
					Tanks += E.Type == EventType::UnitProduced && W.FindUnit(E.Target, V) && V.Owner == 0 && W.Rules().Units[V.Def].HasWeapon() ? 1 : 0;
				}
			}, Lines);
			std::printf("  24b. sem CC nem Dozer, com $10 000: %d armados em 2 min\n", Tanks);
			Check(Tanks >= 3, "24b. sem quem erga o centro, a fabrica nao fica parada guardando o dinheiro dele");
		}
		// 24c. Uma Patriot inimiga com energia a 95 m da largada, 30° pro lado: nenhuma fundação morre no alcance dela e a base sobe
		// (antes os 60 m fixos deixavam a usina a 64 m: 11 fundações perdidas, $8 800, e 0 tanques)
		{
			MatchSetup M = MakeEconomyMatch(21);
			M.Rules = EconomyRuleset(true);
			const Vec2 Home = M.Players[0].Units[0].Pos;
			const Vec2 Enemy = M.Players[1].Units[0].Pos;
			const Angle Side = Angle::FromBam(int32_t((Enemy - Home).Heading().Bam) + Angle::FullTurn / 12);
			const Angle F1 = M.Players[1].Units[0].Facing;
			M.Players[1].Units = { { "usa_command_center", Enemy, F1 }, { "usa_power_plant", Offset(Enemy, -40, -30), F1 },
				{ "usa_patriot", Home + Vec2::FromAngle(Side) * Fix::FromInt(95), F1 } };
			std::map<EntityId, int32_t> Open;
			int32_t Lost = 0;
			int32_t Tanks = 0;
			std::vector<std::string> Lines;
			Solo(M, TickNumber(240 * TicksPerSecond), [&](const World& W)
			{
				for (const SimEvent& E : W.LastEvents())
				{
					UnitView V;
					if (E.Type == EventType::StructurePlaced && W.FindUnit(E.Target, V) && V.Owner == 0 && V.BuildLeft > 0)
					{
						Open[E.Target] = 1;
					}
					if (E.Type == EventType::StructureCompleted || E.Type == EventType::StructureCancelled)
					{
						Open.erase(E.Target);
					}
					if (E.Type == EventType::UnitDied && Open.erase(E.Target) > 0)
					{
						++Lost;
					}
					Tanks += E.Type == EventType::UnitProduced && W.FindUnit(E.Target, V) && V.Owner == 0 && W.Rules().Units[V.Def].HasWeapon() ? 1 : 0;
				}
			}, Lines);
			std::printf("  24c. Patriot inimiga a 95 m da largada: %d fundacoes perdidas, %d armados em 4 min\n", Lost, Tanks);
			Check(Lost == 0 && Tanks >= 3, "24c. a obra nova fica fora do alcance de verdade da Patriot (alcance + raio da obra) e a base sobe");
		}
		// 24d. O cerco segura: base pronta (CC, usina, centro, War Factory), 2 Tomahawks e 4 Crusaders em casa, 2 Patriots inimigas
		// com energia a 300 m. A onda sai e cerca uma Patriot de cada vez; os tanques novos da fábrica vão de reforço e cruzam os
		// 200 m de casa no meio do cerco, e o cerco não cai (antes a frente ia pra trás e o cerco caía em 3 a 15 s com a defesa
		// viva). Cada "siege on" acaba com a defesa morta, nenhuma é cercada duas vezes
		{
			MatchSetup M = MakeEconomyMatch(21);
			M.Rules = EconomyRuleset(true);
			const Vec2 Base = M.Players[0].Units[0].Pos;
			const Angle F0 = M.Players[0].Units[0].Facing;
			const Angle F1 = M.Players[1].Units[0].Facing;
			const Vec2 Battery = Offset(Base, 500, 250);
			M.Players[0].Units = { { "usa_command_center", Base, F0 }, { "usa_power_plant", Offset(Base, -40, -30), F0 },
				{ "usa_supply_center", Offset(Base, 45, -45), F0 }, { "usa_war_factory", Offset(Base, -45, 40), F0 },
				{ "usa_tomahawk", Offset(Base, 30, 40), F0 }, { "usa_tomahawk", Offset(Base, 40, 40), F0 }, { "usa_mbt", Offset(Base, 30, 55), F0 },
				{ "usa_mbt", Offset(Base, 40, 55), F0 }, { "usa_mbt", Offset(Base, 50, 40), F0 }, { "usa_mbt", Offset(Base, 50, 55), F0 } };
			M.Players[1].Units = { { "usa_command_center", M.Players[1].Units[0].Pos, F1 }, { "usa_power_plant", Offset(M.Players[1].Units[0].Pos, -40, -30), F1 },
				{ "usa_patriot", Battery, F1 }, { "usa_patriot", Offset(Battery, 0, 24), F1 } };
			std::vector<EntityId> Patriots;
			std::vector<EntityId> Late;   // os tanques que a fábrica fez (o reforço)
			std::map<EntityId, bool> Inside;
			int32_t PatriotsDead = 0;
			TickNumber BothDied = 0;
			TickNumber SiegeAt = 0;
			TickNumber CrossedAt = 0;
			std::vector<std::string> Lines;
			Solo(M, TickNumber(240 * TicksPerSecond), [&](const World& W)
			{
				for (int32_t I = 0; I < W.UnitCount() && Patriots.size() < 2; ++I)
				{
					const UnitView V = W.Unit(I);
					if (V.Owner == 1 && W.Rules().Units[V.Def].Key == "usa_patriot")
					{
						Patriots.push_back(V.Id);
					}
				}
				for (const SimEvent& E : W.LastEvents())
				{
					UnitView V;
					if (E.Type == EventType::UnitProduced && W.FindUnit(E.Target, V) && V.Owner == 0 && W.Rules().Units[V.Def].HasWeapon())
					{
						Late.push_back(E.Target);
					}
					PatriotsDead += E.Type == EventType::UnitDied && std::find(Patriots.begin(), Patriots.end(), E.Target) != Patriots.end() ? 1 : 0;
				}
				BothDied = BothDied == 0 && PatriotsDead == 2 ? W.Tick() : BothDied;
				SiegeAt = SiegeAt == 0 && std::any_of(Lines.begin(), Lines.end(), [](const std::string& L) { return L.find("siege on") != std::string::npos; }) ? W.Tick() : SiegeAt;
				for (const EntityId Id : Late)
				{
					// a casa da IA é a média da largada (World::PlayerStart); vale quem estava dentro dos 200 m com o cerco já de pé
					UnitView V;
					const bool bOut = W.FindUnit(Id, V) && DistSqRaw(V.Pos, W.PlayerStart(0)) > RadiusSqRaw(Fix::FromInt(200));
					CrossedAt = CrossedAt == 0 && bOut && Inside.count(Id) > 0 ? W.Tick() : CrossedAt;
					if (SiegeAt > 0 && !bOut && W.FindUnit(Id, V))
					{
						Inside[Id] = true;
					}
				}
			}, Lines);
			int32_t On = 0;
			int32_t OffAlive = 0;
			for (const std::string& L : Lines)
			{
				On += L.find("siege on") != std::string::npos ? 1 : 0;
				OffAlive += L.find("siege off") != std::string::npos && L.find("defense alive") != std::string::npos ? 1 : 0;
			}
			std::printf("  24d. cerco: %d 'siege on' (o 1o em %.1f s), %d acabaram com a defesa viva; o reforco cruzou os 200 m em %.1f s; as 2 Patriots cairam em %.1f s\n",
				On, double(SiegeAt) / TicksPerSecond, OffAlive, double(CrossedAt) / TicksPerSecond, double(BothDied) / TicksPerSecond);
			Check(On >= 1 && On <= 2 && OffAlive == 0 && SiegeAt > 0 && CrossedAt > SiegeAt && BothDied > CrossedAt,
				"24d. o cerco segue ate a defesa cair (o reforco que cruza os 200 m nao derruba)");
		}
		// (2ª revisão de 23/10) uma Patriot inimiga com energia a DistM da largada, SideDeg graus pro lado (o 24c é 95 m e 30°), e a
		// IA sozinha por Seconds: as fundações perdidas, os Dozers perdidos, os armados feitos e quando o 1º centro foi marcado
		struct ForwardRun
		{
			int32_t Lost = 0;
			int32_t DozersLost = 0;
			int32_t Tanks = 0;
			TickNumber CenterAt = 0;
		};
		const auto RunForward = [&](const MatchSetup& M, int32_t Seconds)
		{
			ForwardRun Out;
			std::map<EntityId, bool> Open;
			std::map<EntityId, bool> Dozers;
			std::vector<std::string> Lines;
			Solo(M, TickNumber(Seconds * TicksPerSecond), [&](const World& W)
			{
				for (const SimEvent& E : W.LastEvents())
				{
					UnitView V;
					const bool bMine = W.FindUnit(E.Target, V) && V.Owner == 0;
					if (E.Type == EventType::StructurePlaced && bMine && V.BuildLeft > 0)
					{
						Open[E.Target] = true;
						Out.CenterAt = Out.CenterAt == 0 && W.Rules().Units[V.Def].bSupplyCenter ? W.Tick() : Out.CenterAt;
					}
					if (E.Type == EventType::StructureCompleted || E.Type == EventType::StructureCancelled)
					{
						Open.erase(E.Target);
					}
					if (E.Type == EventType::UnitDied)
					{
						Out.Lost += Open.erase(E.Target) > 0 ? 1 : 0;
						Out.DozersLost += Dozers.count(E.Target) > 0 ? 1 : 0;
					}
					Out.Tanks += E.Type == EventType::UnitProduced && bMine && W.Rules().Units[V.Def].HasWeapon() ? 1 : 0;
				}
				Dozers.clear();
				for (int32_t I = 0; I < W.UnitCount(); ++I)
				{
					const UnitView V = W.Unit(I);
					if (V.Owner == 0 && W.Rules().Units[V.Def].CanConstruct())
					{
						Dozers[V.Id] = true;
					}
				}
			}, Lines);
			return Out;
		};
		const auto ForwardPatriot = [&](int32_t DistM, int32_t SideDeg)
		{
			MatchSetup M = MakeEconomyMatch(21);
			M.Rules = EconomyRuleset(true);
			const Vec2 Home = M.Players[0].Units[0].Pos;
			const Vec2 Enemy = M.Players[1].Units[0].Pos;
			const Angle Side = Angle::FromBam(int32_t((Enemy - Home).Heading().Bam) + int32_t(int64_t(SideDeg) * Angle::FullTurn / 360));
			const Angle F1 = M.Players[1].Units[0].Facing;
			M.Players[1].Units = { { "usa_command_center", Enemy, F1 }, { "usa_power_plant", Offset(Enemy, -40, -30), F1 },
				{ "usa_patriot", Home + Vec2::FromAngle(Side) * Fix::FromInt(DistM), F1 } };
			return M;
		};
		// 24e. A Patriot a 95 m, 45° pro outro lado, cobre o lugar do centro no depósito de casa (na névoa): o centro vai pra outro
		// depósito e a base sobe (antes: uma usina e mais nada, $9 200 parados e 0 tanques pra sempre)
		{
			const ForwardRun F = RunForward(ForwardPatriot(95, -45), 240);
			std::printf("  24e. Patriot cobrindo o deposito de casa: centro marcado em %.0f s, %d armados em 4 min, %d fundacoes e %d Dozers perdidos\n",
				double(F.CenterAt) / TicksPerSecond, F.Tanks, F.Lost, F.DozersLost);
			Check(F.CenterAt > 0 && F.Tanks >= 3 && F.Lost == 0 && F.DozersLost <= 1, "24e. o deposito coberto por uma defesa sai da escolha e a base sobe");
		}
		// 24f. A Patriot a 80 m, 60° pro lado, cobre a porta do CC: o Dozer não vai pelo alcance dela e o CC não repõe Dozer pra morrer
		// na porta (antes: 31 Dozers mortos em 8 min, $31 000, e 0 tanques)
		{
			const ForwardRun F = RunForward(ForwardPatriot(80, 60), 240);
			std::printf("  24f. Patriot cobrindo a porta do CC: %d Dozers e %d fundacoes perdidos, %d armados em 4 min\n", F.DozersLost, F.Lost, F.Tanks);
			Check(F.DozersLost <= 2 && F.Lost == 0 && F.Tanks >= 3, "24f. sem Dozer morrendo na porta do CC nem no caminho pelo alcance da Patriot");
		}
		// 24g. A base sem centro (o único morreu), com CC, Dozer, usinas, War Factory, uma zona pronta e $0: só a renda do avião. O
		// centro novo sai antes de 3 min (com a janela de 60 s, a fábrica comia a renda da zona e o centro saía aos 414 s)
		{
			MatchSetup M = MakeEconomyMatch(21);
			M.Rules = EconomyRuleset(true);
			M.Rules.Economy.StartingMoney = 0;
			const Vec2 Base = M.Players[0].Units[0].Pos;
			const Angle F0 = M.Players[0].Units[0].Facing;
			M.Players[0].Units = { { "usa_command_center", Base, F0 }, { "usa_dozer", Offset(Base, -30, 0), F0 },
				{ "usa_power_plant", Offset(Base, -40, -30), F0 }, { "usa_power_plant", Offset(Base, -55, -30), F0 },
				{ "usa_power_plant", Offset(Base, -70, -30), F0 }, { "usa_war_factory", Offset(Base, -45, 40), F0 },
				{ "usa_drop_zone", Offset(Base, -60, -70), F0 } };
			M.Players[1].Units = { { "usa_command_center", M.Players[1].Units[0].Pos, M.Players[1].Units[0].Facing } };
			const ForwardRun F = RunForward(M, 240);
			std::printf("  24g. sem centro, so com a renda da zona: centro novo marcado em %.0f s, %d armados em 4 min\n", double(F.CenterAt) / TicksPerSecond, F.Tanks);
			Check(F.CenterAt > 0 && F.CenterAt <= TickNumber(180 * TicksPerSecond), "24g. a base so com a zona guarda o preco do centro e ele volta em menos de 3 min");
		}
		// (3ª revisão de 23/10) as Patriots inimigas com energia em (DistM, SideDeg) a partir da largada (o rumo pro inimigo = 0°);
		// com Geo, o depósito de casa a 75 m pro +X e o CC e o Dozer virados pro +Y, e a base do jogador 0 deslocada Shift
		const auto Patriots = [&](std::vector<std::pair<int32_t, int32_t>> At, bool bGeo, Vec2 Shift, int32_t Money)
		{
			MatchSetup M = MakeEconomyMatch(21);
			M.Rules = EconomyRuleset(true);
			M.Rules.Economy.StartingMoney = Money;
			for (StartingUnit& U : M.Players[0].Units)
			{
				U.Pos = U.Pos + Shift;
			}
			M.Docks[0].Pos = M.Docks[0].Pos + Shift;
			const Vec2 Home = M.Players[0].Units[0].Pos;
			const Vec2 Enemy = M.Players[1].Units[0].Pos;
			if (bGeo)
			{
				M.Docks[0] = DockAt(Home + Vec2::Make(Fix::FromInt(75), Fix::Zero()), 400);
				M.Players[0].Units[0].Facing = Angle::FromBam(Angle::QuarterTurn);
				M.Players[0].Units[1].Facing = M.Players[0].Units[0].Facing;
			}
			const Angle F1 = M.Players[1].Units[0].Facing;
			M.Players[1].Units = { { "usa_command_center", Enemy, F1 }, { "usa_power_plant", Offset(Enemy, -40, -30), F1 }, { "usa_power_plant", Offset(Enemy, 40, -30), F1 } };
			for (const auto& [DistM, SideDeg] : At)
			{
				const Angle Side = Angle::FromBam(int32_t((Enemy - Home).Heading().Bam) + int32_t(int64_t(SideDeg) * Angle::FullTurn / 360));
				M.Players[1].Units.push_back({ "usa_patriot", Home + Vec2::FromAngle(Side) * Fix::FromInt(DistM), F1 });
			}
			return M;
		};
		// 24h. Duas Patriots a 95 m (90° e 180° do rumo pro inimigo): o Dozer nasce num bolso entre os dois alcances e o CC, e todo
		// caminho pra fora passa na folga deles. Sem caminho fora por 30 s, ele sai raspando (antes: 20 min com $10 000 e 0 tanques)
		{
			const ForwardRun F = RunForward(Patriots({ { 95, 90 }, { 95, -180 } }, false, Vec2::Zero(), 10000), 240);
			std::printf("  24h. Dozer no bolso entre duas Patriots: %d armados em 4 min, %d Dozers e %d fundacoes perdidos\n", F.Tanks, F.DozersLost, F.Lost);
			Check(F.Tanks >= 3 && F.DozersLost == 0 && F.Lost == 0, "24h. sem caminho fora dos alcances, o Dozer sai do bolso raspando e a base sobe");
		}
		// 24i. A base no canto (deslocada 150 m e 130 m pra borda), a Patriot a 65 m, 60° pro lado: o depósito de casa coberto, o
		// centro vai pro do meio, a Patriot mata o CC e a casa fica sem lugar (o alcance e a névoa). A War Factory vai pra perto do
		// centro vivo (antes: 13,6 min sem tanque, o dinheiro subindo a $24 800)
		{
			const ForwardRun F = RunForward(Patriots({ { 65, 60 } }, true, Vec2::Make(Fix::FromInt(-150), Fix::FromInt(-130)), 10000), 300);
			std::printf("  24i. base no canto com a Patriot a 65 m: centro marcado em %.0f s, %d armados em 5 min\n", double(F.CenterAt) / TicksPerSecond, F.Tanks);
			Check(F.CenterAt > 0 && F.Tanks >= 3, "24i. sem lugar em volta de casa, a obra vai pra perto do centro vivo");
		}
		// 24j. $20 000 (2 Dozers), a Patriot a 60 m, 150° pro lado: os dois Dozers vão pra obras trocadas e cada um para no lote da
		// obra do outro (o jogo só tira do lote o amigo sem tarefa). Um larga a obra e as duas sobem (antes: 20 min com $48 800 e
		// 0 tanques). (4ª revisão de 23/10) Com o lugar escolhido guardado (o 2º Dozer espera o 1º chegar nele), as obras
		// trocadas não se formam mais aqui: a conferência fica na base que sobe (o "off the lot" segue no código e no log)
		{
			std::vector<std::string> Lines;
			int32_t Tanks = 0;
			Solo(Patriots({ { 60, 150 } }, true, Vec2::Zero(), 20000), TickNumber(240 * TicksPerSecond), [&](const World& W)
			{
				for (const SimEvent& E : W.LastEvents())
				{
					UnitView V;
					Tanks += E.Type == EventType::UnitProduced && W.FindUnit(E.Target, V) && V.Owner == 0 && W.Rules().Units[V.Def].HasWeapon() ? 1 : 0;
				}
			}, Lines);
			const int32_t OffLot = int32_t(std::count_if(Lines.begin(), Lines.end(), [](const std::string& L) { return L.find("off the lot") != std::string::npos; }));
			std::printf("  24j. dois Dozers cada um no lote do outro: %d 'off the lot', %d armados em 4 min\n", OffLot, Tanks);
			Check(Tanks >= 3, "24j. os dois Dozers nao travam um no lote do outro e a base sobe");
		}
		// 24k. $20 000, Patriots a 90 m (75°) e 95 m (-165°): o ponto atrás do CC no alcance, o 2º Dozer fica na porta, no lote da
		// usina; o jogo o manda sair pra dentro do CC e ele não sai, e a usina nunca começa. Ele vai pra volta do CC fora do alcance
		// (antes: 8 min com $18 200 e 0 tanques). (4ª revisão de 23/10) Com a obra indo pro Dozer mais perto do lugar escolhido, o
		// 2º Dozer não para mais no lote da usina aqui: a conferência fica na base que sobe (o "out of the way" segue no código)
		{
			std::vector<std::string> Lines;
			int32_t Tanks = 0;
			Solo(Patriots({ { 90, 75 }, { 95, -165 } }, false, Vec2::Zero(), 20000), TickNumber(240 * TicksPerSecond), [&](const World& W)
			{
				for (const SimEvent& E : W.LastEvents())
				{
					UnitView V;
					Tanks += E.Type == EventType::UnitProduced && W.FindUnit(E.Target, V) && V.Owner == 0 && W.Rules().Units[V.Def].HasWeapon() ? 1 : 0;
				}
			}, Lines);
			const int32_t OutOfWay = int32_t(std::count_if(Lines.begin(), Lines.end(), [](const std::string& L) { return L.find("out of the way") != std::string::npos; }));
			std::printf("  24k. Dozer a toa no lote da usina com o ponto de tras coberto: %d 'out of the way', %d armados em 4 min\n", OutOfWay, Tanks);
			Check(Tanks >= 3, "24k. o Dozer a toa nao trava a obra de outro e a base sobe");
		}
		// (4ª revisão de 23/10, a base cercada) 24l. A base no canto entre duas Patriots (80 m a -45° e 85 m a 105°): todo lugar da
		// War Factory e do centro em volta de casa fica atrás de um alcance, e o Dozer, ferido na saída, não passa raspando em
		// nenhum. A procura só aceita o lote aonde algum Dozer chega, e o CC faz o Dozer da porta livre (antes: 30 min com $9 200 e
		// 0 tanques)
		{
			const ForwardRun F = RunForward(Patriots({ { 80, -45 }, { 85, 105 } }, false, Vec2::Make(Fix::FromInt(-150), Fix::FromInt(-130)), 10000), 360);
			std::printf("  24l. base no canto entre duas Patriots: centro marcado em %.0f s, %d armados em 6 min, %d fundacoes e %d Dozers perdidos\n",
				double(F.CenterAt) / TicksPerSecond, F.Tanks, F.Lost, F.DozersLost);
			Check(F.Tanks >= 3 && F.Lost == 0, "24l. a base cercada no canto acha o lote ao alcance de um Dozer e sobe");
		}
		// 24m. Quatro Patriots em anel a 90 m da largada: os alcances se juntam e o lugar livre dentro do anel não cabe nada. O Dozer
		// passa raspando pela junta menos exposta até o lote do lado de fora (antes: 30 min com $9 200 e 0 tanques)
		{
			const ForwardRun F = RunForward(Patriots({ { 90, 15 }, { 90, 105 }, { 90, -165 }, { 90, -75 } }, false, Vec2::Zero(), 10000), 480);
			std::printf("  24m. anel de 4 Patriots a 90 m: centro marcado em %.0f s, %d armados em 8 min, %d fundacoes perdidas\n",
				double(F.CenterAt) / TicksPerSecond, F.Tanks, F.Lost);
			Check(F.Tanks >= 3 && F.Lost == 0, "24m. dentro do anel de alcances, a obra sai do lado de fora e a base sobe");
		}
		// 24n. $50 000, a base no canto com a porta do CC coberta e a fuga da porta pelo alcance de outra Patriot: o CC não repõe
		// Dozer pra morrer (com Dozer vivo, ou sem a fuga sobrevivível) e para depois de 3 mortos no alcance sem a base andar
		// (antes: 13 Dozers em 10 min e 32 em 30, $32 000). A base fica cercada (nenhum lote da fábrica ao alcance nem raspando)
		{
			const ForwardRun F = RunForward(Patriots({ { 80, 91 }, { 107, -42 }, { 79, 0 } }, false, Vec2::Make(Fix::FromInt(-150), Fix::FromInt(-130)), 50000), 600);
			std::printf("  24n. porta do CC coberta, fuga pelo alcance de outra Patriot: %d Dozers perdidos em 10 min, %d armados\n", F.DozersLost, F.Tanks);
			Check(F.DozersLost <= 3, "24n. sem Dozer novo morrendo sem fim na saida da porta coberta");
		}
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
	TestFormationMove();
	TestLineDrag();
	TestLinePreview();
	TestVision();
	TestFogBattle();
	TestEconomy();
	TestLotExitBlocked();
	TestRallySpread();
	TestEconomyMatch();
	TestForceFire();
	TestWaterGrid();
	TestResign();
	TestWreckPush();
	TestWreckLife();
	TestWreckCaps();
	TestPatriot();
	TestCrush();
	TestDefenseAssist();
	TestAIExpansion();
	TestStartingMoney();
	TestRank();
	TestArea();
	TestMinRange();
	TestAirlift();
	TestStrike();
	TestSiege();
	TestSell();
	TestRepair();
	TestLineSpread();
	TestTruckTopple();
	TestLauncherAim();
	TestHoldGround();
	TestAIv4();
	const uint64_t Final = TestBattle();
	if (bScale)
	{
		TestScale();
	}
	std::printf("FINAL_HASH %016llx\n", (unsigned long long)Final);
	std::printf(Failures == 0 ? "TUDO CERTO\n" : "%d FALHA(S)\n", Failures);
	return Failures == 0 ? 0 : 1;
}
