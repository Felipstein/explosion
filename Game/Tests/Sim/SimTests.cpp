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
	Ruleset EconomyRuleset()
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
		Dozer.Constructs = { "usa_power_plant", "usa_supply_center", "usa_war_factory", "usa_command_center" };
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
		WF.Produces = { "usa_mbt" };
		WF.Requires = { "usa_supply_center" };
		WF.ExitLaneMm = 6600;
		Specs.push_back(WF);

		ArmorTable Armor;
		Armor.Set(ArmorType::Tank, DamageType::ArmorPiercing, 100);
		Armor.Set(ArmorType::Structure, DamageType::ArmorPiercing, 100);
		Armor.Set(ArmorType::Structure, DamageType::SmallArms, 50);
		Armor.Set(ArmorType::Structure, DamageType::Explosive, 100);
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
		Ruleset R;
		std::string Error;
		Check(Ruleset::Build(Specs, Armor, E, R, &Error), "as regras da economia montam");
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
		int32_t Drops = 0;
		double MsPerTick = 0;
	};

	EconomyMatchResult RunEconomyMatch(const MatchSetup& Setup, TickNumber MaxTicks)
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
		ReplayWriter Rec;
		Rec.Begin(Setup);
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
				}
				if (E.Type == EventType::WeaponFired && R.FirstShot == 0)
				{
					R.FirstShot = T;
				}
				if (E.Type == EventType::SupplyDelivered && W.FindUnit(E.Source, V))
				{
					R.Income[V.Owner] += E.Value;
				}
				R.Drops += E.Type == EventType::CargoDropped ? 1 : 0;
			}
		}
		const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Begin).count();
		R.MsPerTick = R.Hashes.empty() ? 0 : Ms / double(R.Hashes.size());
		R.EndTick = W.Tick();
		R.Winner = W.WinningTeam();
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
}

int main(int Argc, char** Argv)
{
	const bool bScale = Argc > 1 && std::strcmp(Argv[1], "--scale") == 0;
	TestMath();
	TestPathfinding();
	TestFuzz();
	TestSpawn();
	TestGroupSlots();
	TestVision();
	TestFogBattle();
	TestEconomy();
	TestLotExitBlocked();
	TestEconomyMatch();
	const uint64_t Final = TestBattle();
	if (bScale)
	{
		TestScale();
	}
	std::printf("FINAL_HASH %016llx\n", (unsigned long long)Final);
	std::printf(Failures == 0 ? "TUDO CERTO\n" : "%d FALHA(S)\n", Failures);
	return Failures == 0 ? 0 : 1;
}
