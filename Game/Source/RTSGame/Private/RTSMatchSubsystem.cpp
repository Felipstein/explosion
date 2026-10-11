#include "RTSMatchSubsystem.h"

#include <algorithm>
#include <vector>

#include "BlastFxDirector.h"
#include "Components/LightComponent.h"
#include "ContentStreaming.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Misc/DateTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTSConvert.h"
#include "RTSDockActor.h"
#include "RTSCameraPawn.h"
#include "RTSEffect.h"
#include "RTSFogOfWar.h"
#include "RTSFogPass.h"
#include "RTSSessionSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "RTSGameData.h"
#include "RTSMapBaker.h"
#include "RTSNetSession.h"
#include "RTSUnitActor.h"
#include "ShaderCompiler.h"
#include "Sim/Lockstep.h"
#include "Sim/Pathfinder.h"
#include "Sim/Replay.h"
#include "Sim/SkirmishAI.h"
#include "Sim/World.h"

namespace
{
	constexpr double StepSeconds = 1.0 / double(Sim::TicksPerSecond);
	/** No skirmish local o comando executa no passo seguinte (33 ms); o online usa o atraso medido. */
	constexpr int32 LocalCommandDelayTicks = 1;
	/** Teto de passos por quadro: se o PC engasgar, a partida desacelera em vez de pular. */
	constexpr int32 MaxStepsPerFrame = 8;
	constexpr float StartSpacingM = 11.0f;
	constexpr int32 StartColumns = 3;
}

struct FRTSMatchState
{
	FRTSGameData Data;
	FRTSBakedMap Baked;
	FString MapName;
	Sim::MatchSetup Setup;
	Sim::World World;
	Sim::CommandScheduler Queue{ LocalCommandDelayTicks };
	std::vector<Sim::SkirmishAI> Brains;
	Sim::ReplayWriter Replay;
	TMap<uint32, Sim::UnitView> Prev;
	TMap<uint32, Sim::UnitView> Curr;
	FVector2D LocalStartCm = FVector2D::ZeroVector;
};

URTSMatchSubsystem::URTSMatchSubsystem() = default;
URTSMatchSubsystem::~URTSMatchSubsystem()
{
	delete State;
	State = nullptr;
}

void URTSMatchSubsystem::SetPaused(bool bInPaused)
{
	if (bNet)
	{
		bInPaused = false;   // na rede a partida não para por um só (a pausa em rede vem depois)
	}
	bPaused = bInPaused;
	if (UBlastFxDirector* Director = UBlastFxDirector::Get(this))
	{
		Director->SetPaused(bInPaused);
	}
}

bool URTSMatchSubsystem::IsRunning() const
{
	return (State != nullptr);
}

TStatId URTSMatchSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URTSMatchSubsystem, STATGROUP_Tickables);
}

namespace
{
	/** A cor de cada jogador da partida (o índice da paleta): na LAN, a escolhida na sala. */
	uint8 GPlayerPalette[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
}

FLinearColor URTSMatchSubsystem::PlayerColor(uint8 Player)
{
	return PaletteColor(GPlayerPalette[Player % 8]);
}

FText URTSMatchSubsystem::PlayerColorName(uint8 Player)
{
	static const FText Names[] = {
		NSLOCTEXT("RTSMatch", "ColorBlue", "Blue"), NSLOCTEXT("RTSMatch", "ColorRed", "Red"),
		NSLOCTEXT("RTSMatch", "ColorGreen", "Green"), NSLOCTEXT("RTSMatch", "ColorYellow", "Yellow"),
		NSLOCTEXT("RTSMatch", "ColorOrange", "Orange"), NSLOCTEXT("RTSMatch", "ColorCyan", "Cyan"),
		NSLOCTEXT("RTSMatch", "ColorPurple", "Purple"), NSLOCTEXT("RTSMatch", "ColorPink", "Pink"),
	};
	return Names[GPlayerPalette[Player % 8] % 8];
}

FLinearColor URTSMatchSubsystem::PaletteColor(uint8 Color)
{
	static const FLinearColor Colors[] = {
		FLinearColor(0.10f, 0.45f, 1.00f),   // azul
		FLinearColor(0.95f, 0.16f, 0.10f),   // vermelho
		FLinearColor(0.20f, 0.85f, 0.25f),   // verde
		FLinearColor(1.00f, 0.80f, 0.10f),   // amarelo
		FLinearColor(0.95f, 0.45f, 0.05f),   // laranja
		FLinearColor(0.10f, 0.85f, 0.90f),   // ciano
		FLinearColor(0.75f, 0.25f, 0.95f),   // roxo
		FLinearColor(0.95f, 0.45f, 0.70f),   // rosa
	};
	return Colors[Color % 8];
}

/** Depuração: -RTSDumpGrid=<arquivo.ppm> salva a grade da simulação como imagem. */
static void DumpGrid(const Sim::MatchSetup& Setup, const TArray<FVector2D>& Starts)
{
	// a grade da simulação como imagem, pra posicionar docas e bases sem chutar: areia = livre
	// (mais clara quanto mais alta), preto = penhasco ou pedra, azul = água, vermelho = largada,
	// amarelo = depósito. Uma célula = um pixel, o norte (+Y) pra cima.
	FString Path;
	if (!FParse::Value(FCommandLine::Get(), TEXT("-RTSDumpGrid="), Path))
	{
		return;
	}
	const Sim::SimMap& M = Setup.Map;
	TArray<uint8> Rgb;
	Rgb.SetNumZeroed(M.Width * M.Height * 3);
	int32 Lo = MAX_int32;
	int32 Hi = MIN_int32;
	for (int32 K = 0; K < M.Width * M.Height && M.HasHeights(); ++K)
	{
		Lo = FMath::Min(Lo, int32(M.MinDm[K]));
		Hi = FMath::Max(Hi, int32(M.MaxDm[K]));
	}
	const auto Put = [&](int32 X, int32 Y, uint8 R, uint8 Gc, uint8 B)
	{
		if (X >= 0 && Y >= 0 && X < M.Width && Y < M.Height)
		{
			const int32 Row = M.Height - 1 - Y;
			const int32 K = (Row * M.Width + X) * 3;
			Rgb[K] = R;
			Rgb[K + 1] = Gc;
			Rgb[K + 2] = B;
		}
	};
	for (int32 Y = 0; Y < M.Height; ++Y)
	{
		for (int32 X = 0; X < M.Width; ++X)
		{
			const Sim::CellPass P = M.PassAt(X, Y);
			const float T = M.HasHeights() && Hi > Lo ? float(M.MaxDm[M.Index(X, Y)] - Lo) / float(Hi - Lo) : 0.5f;
			const bool bSteep = M.HasHeights() && M.MaxDm[M.Index(X, Y)] - M.MinDm[M.Index(X, Y)] > 13;   // mais de 1,3 m dentro da célula
			if (P == Sim::CellPass::Water)
			{
				Put(X, Y, 40, 90, 200);
			}
			else if (P == Sim::CellPass::Blocked)
			{
				Put(X, Y, 20, 20, 20);
			}
			else
			{
				const uint8 V = uint8(90 + 150 * T);
				Put(X, Y, V, uint8(V * 0.85f), uint8(bSteep ? V * 0.3f : V * 0.6f));
			}
		}
	}
	const auto Mark = [&](Sim::Vec2 P, uint8 R, uint8 Gc, uint8 B, int32 Size)
	{
		const Sim::Cell C = M.CellOf(P);
		for (int32 DY = -Size; DY <= Size; ++DY)
		{
			for (int32 DX = -Size; DX <= Size; ++DX)
			{
				Put(C.X + DX, C.Y + DY, R, Gc, B);
			}
		}
	};
	for (const FVector2D& S : Starts)
	{
		Mark(RTS::ToSim(S * 100.0), 230, 30, 30, 2);
	}
	for (const Sim::SupplyDockSetup& D : Setup.Docks)
	{
		Mark(D.Pos, 250, 220, 40, 3);
	}
	FString Text = FString::Printf(TEXT("P6\n%d %d\n255\n"), M.Width, M.Height);
	TArray<uint8> Bytes;
	FTCHARToUTF8 Head(*Text);
	Bytes.Append(reinterpret_cast<const uint8*>(Head.Get()), Head.Length());
	Bytes.Append(Rgb);
	FFileHelper::SaveArrayToFile(Bytes, *Path);
	UE_LOG(LogTemp, Log, TEXT("RTS grade salva em %s (%d×%d células, altura de %.1f a %.1f m)"), *Path, M.Width, M.Height, Lo / 10.0, Hi / 10.0);
	if (FParse::Param(FCommandLine::Get(), TEXT("RTSDumpGridQuit")))
	{
		FPlatformMisc::RequestExit(false);   // só a grade: fecha logo
	}
}

URTSNetSession* URTSMatchSubsystem::Net() const
{
	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	URTSNetSession* N = GI ? GI->GetSubsystem<URTSNetSession>() : nullptr;
	return N && N->IsActive() ? N : nullptr;
}

bool URTSMatchSubsystem::IsWaitingForPlayers() const
{
	return bNet && (bNetAwaitSetup || (bReady && Net() && !Net()->CanStartTicking()));
}

void URTSMatchSubsystem::Issue(const Sim::Command& C)
{
	if (State == nullptr)
	{
		return;
	}
	if (bNet)
	{
		if (URTSNetSession* N = Net())
		{
			N->SubmitLocal(C);   // o anfitrião carimba o passo; ninguém executa antes de todos terem
		}
		return;
	}
	State->Queue.Issue(C, State->World.Tick());
}

bool URTSMatchSubsystem::StartSkirmish(const FString& MapName, FString& OutError, const Sim::MatchSetup* GivenSetup)
{
	URTSNetSession* NetSession = Net();
	bNet = NetSession != nullptr;
	LocalSlot = bNet ? NetSession->LocalSlot() : 0;
	bNetAwaitSetup = false;
	bNetBot = bNet && FParse::Param(FCommandLine::Get(), TEXT("RTSNetBot"));
	TUniquePtr<FRTSMatchState> S = MakeUnique<FRTSMatchState>();
	S->MapName = MapName;
	if (!S->Data.Load(OutError))
	{
		return false;
	}
	const FRTSMapInfo* Info = S->Data.FindMap(MapName);
	if (!Info)
	{
		OutError = FString::Printf(TEXT("o mapa %s não está no Maps.json"), *MapName);
		return false;
	}
	// quantos jogam: na LAN, todos da sala (um por base do mapa); no skirmish, você e a IA
	const int32 PlayerCount = bNet ? NetSession->PlayerCount() : 2;
	if (Info->StartsM.Num() < FMath::Max(2, PlayerCount))
	{
		OutError = FString::Printf(TEXT("o mapa tem %d posições iniciais; a partida precisa de %d"), Info->StartsM.Num(), FMath::Max(2, PlayerCount));
		return false;
	}
	for (int32 P = 0; P < 8; ++P)
	{
		GPlayerPalette[P] = bNet ? NetSession->ColorOfSlot(uint8(P)) : uint8(P);
	}
	if (!RTSMapBaker::Bake(GetWorld(), *Info, S->Baked, OutError))
	{
		return false;
	}

	// onde cada um começa: nas bases, ou (skirmish de só tanques) no caminho entre elas,
	// a SkirmishGapM um do outro: sempre em chão andável e com caminho até o inimigo
	TArray<FVector2D> Starts = Info->StartsM;
	const bool bBases = !Info->BaseStructure.IsEmpty();
	if (!bBases && PlayerCount == 2 && Info->SkirmishGapM > 0.0f && S->Data.Rules.Units.size() > 0)
	{
		Sim::Pathfinder Finder;
		std::vector<Sim::Vec2> Route;
		bool bReached = false;
		const int32 Clearance = S->Baked.Logic.ClearanceFor(S->Data.Rules.Units[0].PathRadius);
		if (Finder.FindPath(S->Baked.Logic, RTS::ToSim(Starts[0] * 100.0), RTS::ToSim(Starts[1] * 100.0), Clearance, Route, &bReached) && bReached)
		{
			TArray<FVector2D> Line;
			Line.Add(Starts[0]);
			for (const Sim::Vec2& P : Route)
			{
				Line.Add(RTS::ToWorld(P) / 100.0);
			}
			double Length = 0.0;
			for (int32 K = 1; K < Line.Num(); ++K)
			{
				Length += FVector2D::Distance(Line[K - 1], Line[K]);
			}
			auto PointAt = [&Line](double Dist)
			{
				for (int32 K = 1; K < Line.Num(); ++K)
				{
					const double Seg = FVector2D::Distance(Line[K - 1], Line[K]);
					if (Dist <= Seg && Seg > 0.0)
					{
						return FMath::Lerp(Line[K - 1], Line[K], Dist / Seg);
					}
					Dist -= Seg;
				}
				return Line.Last();
			};
			if (Length > Info->SkirmishGapM)
			{
				Starts[0] = PointAt((Length - Info->SkirmishGapM) * 0.5);
				Starts[1] = PointAt((Length + Info->SkirmishGapM) * 0.5);
			}
			UE_LOG(LogTemp, Log, TEXT("RTS largada no caminho entre as bases (%.0f m): (%.0f, %.0f) e (%.0f, %.0f)"),
				Length, Starts[0].X, Starts[0].Y, Starts[1].X, Starts[1].Y);
		}
	}

	// a montagem: você (time 0) contra a IA (time 1); na LAN, todos humanos, montada só no anfitrião
	Sim::MatchSetup& Setup = S->Setup;
	Setup.Seed = FPlatformTime::Cycles64();
	{
		const URTSSessionSubsystem* Session = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<URTSSessionSubsystem>() : nullptr;
		// a névoa: na LAN a da sala (o anfitrião escolhe); sozinho, a do menu
		const ERTSFogMode FogMode = bNet ? NetSession->GetFogMode() : (Session ? Session->FogMode : ERTSFogMode::Explored);
		Setup.bFogOfWar = FogMode != ERTSFogMode::Off;
		bFogExplored = FogMode == ERTSFogMode::Explored;
		Setup.bAllowDebugCommands = !bNet;   // skirmish local: o F9 vale; na rede, não (seria o mapa todo à vista)
	}
	if (GivenSetup)
	{
		// cliente da LAN: a montagem do anfitrião (grade, regras, jogadores); as largadas pra câmera
		// saem dela (a 1ª peça de cada jogador é o CC)
		Setup = *GivenSetup;
		Starts.Reset();
		for (const Sim::PlayerSetup& PS : Setup.Players)
		{
			Starts.Add(PS.Units.empty() ? FVector2D::ZeroVector : RTS::ToWorld(PS.Units[0].Pos) / 100.0);
		}
	}
	else
	{
		Setup.Map = S->Baked.Logic;
		Setup.Map.ComputeClearance();
		Setup.Rules = S->Data.Rules;
		// os depósitos (fixos no Maps.json até o WorldForge exportar os dele): cada um conferido na
		// grade; o que cair em penhasco, água ou pedra vai pro lugar livre mais perto (aviso no log)
		for (const FVector2D& DockM : Info->DocksM)
		{
			Sim::SupplyDockSetup Dock;
			Dock.HalfX = Sim::Fix::FromMilli(RTS::Milli(Info->DockSizeM.X * 0.5));
			Dock.HalfY = Sim::Fix::FromMilli(RTS::Milli(Info->DockSizeM.Y * 0.5));
			Dock.Boxes = Info->DockBoxes;
			const auto Fits = [&](const FVector2D& C)
			{
				const Sim::Cell Lo = Setup.Map.CellOf(RTS::ToSim((C - Info->DockSizeM * 0.5) * 100.0));
				const Sim::Cell Hi = Setup.Map.CellOf(RTS::ToSim((C + Info->DockSizeM * 0.5) * 100.0));
				for (int32 Y = Lo.Y - 1; Y <= Hi.Y + 1; ++Y)
				{
					for (int32 X = Lo.X - 1; X <= Hi.X + 1; ++X)
					{
						if (Setup.Map.PassAt(X, Y) != Sim::CellPass::Open)
						{
							return false;
						}
					}
				}
				return true;
			};
			FVector2D At = DockM;
			bool bFound = Fits(At);
			for (int32 Ring = 4; Ring <= 80 && !bFound; Ring += 4)
			{
				for (int32 K = 0; K < 16 && !bFound; ++K)
				{
					const double A = K * (UE_DOUBLE_PI / 8.0);
					const FVector2D Try = DockM + FVector2D(FMath::Cos(A), FMath::Sin(A)) * double(Ring);
					bFound = Fits(Try);
					At = bFound ? Try : At;
				}
			}
			if (!bFound || At != DockM)
			{
				UE_LOG(LogTemp, Warning, TEXT("RTS depósito em (%.0f, %.0f) não cabe ali; %s"), DockM.X, DockM.Y,
					bFound ? *FString::Printf(TEXT("foi pra (%.0f, %.0f)"), At.X, At.Y) : TEXT("ficou onde estava"));
			}
			Dock.Pos = RTS::ToSim(At * 100.0);
			Setup.Docks.push_back(Dock);
		}
		DumpGrid(Setup, Starts);
		// os times e quem é IA: na LAN, da sala ("sem time" vira um lado sozinho; os números ficam
		// contínuos 0..k-1, a simulação recusa time ≥ 8); sozinho, você contra a IA
		TArray<uint8> SlotTeam;
		TArray<bool> SlotAI;
		{
			TMap<uint8, uint8> LobbyToSim;
			uint8 NextTeam = 0;
			for (int32 P = 0; P < PlayerCount; ++P)
			{
				const uint8 LobbyTeam = bNet ? NetSession->TeamOfSlot(uint8(P)) : 0;
				if (LobbyTeam == 0)
				{
					SlotTeam.Add(NextTeam++);
				}
				else if (const uint8* T = LobbyToSim.Find(LobbyTeam))
				{
					SlotTeam.Add(*T);
				}
				else
				{
					LobbyToSim.Add(LobbyTeam, NextTeam);
					SlotTeam.Add(NextTeam++);
				}
				SlotAI.Add(bNet ? NetSession->IsAISlot(uint8(P)) : P != 0);
			}
		}
		// as largadas: com aliados, a distribuição em que os aliados ficam mais perto (força bruta;
		// no mapa de 4 bases dá sul contra norte). Só no anfitrião: vai pronta na montagem
		TArray<int32> StartOf;
		for (int32 P = 0; P < PlayerCount; ++P)
		{
			StartOf.Add(P);
		}
		bool bAllies = false;
		for (int32 A = 0; A < PlayerCount; ++A)
		{
			for (int32 B = A + 1; B < PlayerCount; ++B)
			{
				bAllies |= SlotTeam[A] == SlotTeam[B];
			}
		}
		if (bAllies)
		{
			std::vector<int32> Perm;
			for (int32 K = 0; K < Starts.Num(); ++K)
			{
				Perm.push_back(K);
			}
			double BestCost = DBL_MAX;
			do
			{
				double Cost = 0.0;
				for (int32 A = 0; A < PlayerCount; ++A)
				{
					for (int32 B = A + 1; B < PlayerCount; ++B)
					{
						Cost += SlotTeam[A] == SlotTeam[B] ? FVector2D::Distance(Starts[Perm[A]], Starts[Perm[B]]) : 0.0;
					}
				}
				if (Cost < BestCost - 1.0)
				{
					BestCost = Cost;
					for (int32 P = 0; P < PlayerCount; ++P)
					{
						StartOf[P] = Perm[P];
					}
				}
			} while (std::next_permutation(Perm.begin(), Perm.end()));
			UE_LOG(LogTemp, Log, TEXT("RTS largadas com times: %s"), *FString::JoinBy(StartOf, TEXT(" "), [&](int32 K)
				{ return FString::Printf(TEXT("(%.0f, %.0f)"), Starts[K].X, Starts[K].Y); }));
		}
		TArray<FVector2D> SlotStarts;
		for (int32 P = 0; P < PlayerCount; ++P)
		{
			SlotStarts.Add(Starts[StartOf[P]]);
		}
		// o centro das largadas em jogo: com 2, cada um olha pro outro; com mais, pro meio do mapa
		FVector2D Middle = FVector2D::ZeroVector;
		for (int32 P = 0; P < PlayerCount; ++P)
		{
			Middle += SlotStarts[P] / double(PlayerCount);
		}
		for (int32 P = 0; P < PlayerCount; ++P)
		{
			Sim::PlayerSetup Player;
			Player.Team = SlotTeam[P];
			Player.bIsAI = SlotAI[P];
			FVector2D Base = SlotStarts[P];
			// de frente pro inimigo, em fileiras de 3
			const FVector2D Forward = ((PlayerCount == 2 ? SlotStarts[1 - P] : Middle) - Base).GetSafeNormal();
			const FVector2D Right(-Forward.Y, Forward.X);
			const Sim::Angle Facing = Sim::Atan2(Sim::Fix::FromRatio(RTS::Milli(Forward.Y), 1000), Sim::Fix::FromRatio(RTS::Milli(Forward.X), 1000));
			FVector2D StartRowsDir = FVector2D::ZeroVector;
			if (bBases)
			{
				// com base (ZH): o CC pronto na largada, de frente pra quem joga como todo prédio novo
				// (a câmera padrão; pedido do Felipe, 10/10: "virado pra mim igual o default"), e as
				// unidades nascem na frente da porta
				const int32 BaseDef = Setup.Rules.FindUnit(TCHAR_TO_UTF8(*Info->BaseStructure));
				if (BaseDef < 0 || !Setup.Rules.Units[BaseDef].IsStructure())
				{
					OutError = FString::Printf(TEXT("o prédio de largada não é prédio: %s"), *Info->BaseStructure);
					return false;
				}
				Sim::StartingUnit CC;
				CC.Key = TCHAR_TO_UTF8(*Info->BaseStructure);
				CC.Pos = RTS::ToSim(Base * 100.0);
				const int32 Dir = RTS::DirFacingViewer(GetDefault<ARTSCameraPawn>()->DefaultYawDeg);
				CC.Facing = RTS::DirAngle(Dir);
				Player.Units.push_back(CC);
				const Sim::UnitDef& D = Setup.Rules.Units[BaseDef];
				const FVector2D Door = Dir == 0 ? FVector2D(1, 0) : Dir == 1 ? FVector2D(0, 1) : Dir == 2 ? FVector2D(-1, 0) : FVector2D(0, -1);
				const double Depth = (Dir & 1) ? D.HalfY.ToDouble() : D.HalfX.ToDouble();
				// as fileiras saem da porta pra fora (Forward vira a direção da porta só pra arrumar)
				Base = Base + Door * (Depth + 14.0);
				StartRowsDir = -Door;
			}
			int32 Placed = 0;
			for (const TPair<FString, int32>& Entry : Info->StartingUnits)
			{
				const int32 DefIndex = Setup.Rules.FindUnit(TCHAR_TO_UTF8(*Entry.Key));
				if (DefIndex < 0)
				{
					OutError = FString::Printf(TEXT("unidade inicial desconhecida: %s"), *Entry.Key);
					return false;
				}
				const int32 Clearance = Setup.Map.ClearanceFor(Setup.Rules.Units[DefIndex].PathRadius);
				for (int32 K = 0; K < Entry.Value; ++K, ++Placed)
				{
					const int32 Row = Placed / StartColumns;
					const int32 Col = Placed % StartColumns - StartColumns / 2;
					const FVector2D RowDir = StartRowsDir.IsZero() ? Forward : StartRowsDir;
					const FVector2D ColDir(-RowDir.Y, RowDir.X);
					const FVector2D PosM = Base + (StartRowsDir.IsZero() ? Right : ColDir) * (Col * StartSpacingM) - RowDir * (Row * StartSpacingM);
					Sim::Vec2 Pos = RTS::ToSim(PosM * 100.0);
					Sim::Cell C = Setup.Map.CellOf(Pos);
					if (!Setup.Map.IsWalkable(C.X, C.Y, Clearance) && Sim::Pathfinder::NearestWalkable(Setup.Map, C, Clearance, 12, C))
					{
						Pos = Setup.Map.CellCenter(C);
					}
					Sim::StartingUnit U;
					U.Key = TCHAR_TO_UTF8(*Entry.Key);
					U.Pos = Pos;
					U.Facing = Facing;
					Player.Units.push_back(U);
				}
			}
			Setup.Players.push_back(Player);
		}
		Starts = SlotStarts;
	}
	if (!S->World.Init(Setup))
	{
		OutError = TEXT("a simulação recusou a montagem");
		return false;
	}
	for (size_t P = 0; P < Setup.Players.size(); ++P)
	{
		if (Setup.Players[P].bIsAI && (!bNet || NetSession->IsHost()))
		{
			S->Brains.emplace_back(Sim::PlayerIndex(P));
			// a IA parte em 8 s: com o começo a ~450 m, o contato vem entre ~30 s (você avançando) e ~65 s
			S->Brains.back().FirstWaveDelayTicks = 8 * Sim::TicksPerSecond;
		}
	}
	if (bNetBot)
	{
		// o teste da LAN com uma placa só: o robô joga pelo jogador local do cliente sem tela
		S->Brains.emplace_back(Sim::PlayerIndex(LocalSlot));
		S->Brains.back().FirstWaveDelayTicks = 8 * Sim::TicksPerSecond;
	}
	S->Replay.Begin(Setup);
	S->LocalStartCm = (Starts.IsValidIndex(LocalSlot) ? Starts[LocalSlot] : Starts[0]) * 100.0;
	if (bNet && NetSession->IsHost() && !GivenSetup)
	{
		// a montagem vai inteira pros outros: ninguém mais monta (a semente, a grade e as
		// regras daqui, conferidas pelo hash da montagem)
		Sim::ByteWriter W;
		Sim::WriteSetup(W, Setup);
		NetSession->SetSetup(TArray<uint8>(W.Bytes.data(), int32(W.Bytes.size())));
		UE_LOG(LogTemp, Log, TEXT("RTS LAN: montagem %016llx (%d bytes), %d jogadores, grade %016llx, névoa %s"), (unsigned long long)Setup.Hash(), int32(W.Bytes.size()),
			int32(Setup.Players.size()), (unsigned long long)Setup.Map.Hash(), Setup.bFogOfWar ? TEXT("sim") : TEXT("não"));
	}
	else if (GivenSetup)
	{
		// a grade daqui só desenha; se for diferente da do anfitrião, o chão e o caminho não batem (aviso)
		Sim::SimMap Mine = S->Baked.Logic;
		Mine.ComputeClearance();
		const bool bSameGrid = Mine.Hash() == Setup.Map.Hash();
		UE_LOG(LogTemp, Log, TEXT("RTS LAN: partida pela montagem do anfitrião %016llx, %d jogadores, vaga %d; a grade daqui %s (%016llx)"),
			(unsigned long long)Setup.Hash(), int32(Setup.Players.size()), int32(LocalSlot), bSameGrid ? TEXT("igual") : TEXT("DIFERENTE"), (unsigned long long)Mine.Hash());
	}
	{
		const Sim::SimMap& M = S->World.Map();
		const FVector2D MinCm = RTS::ToWorld(M.MinCorner());
		const FVector2D MaxCm = RTS::ToWorld(M.MaxCorner());
		Fog = NewObject<URTSFogOfWar>(this);
		Fog->Init(M.Width, M.Height, MinCm, MaxCm - MinCm);
	}
	bFogDisplay = Setup.bFogOfWar;
	bFogPassOn = false;
	if (Fog)
	{
		Fog->SetStartExplored(bFogExplored && Setup.bFogOfWar);
	}
	DefeatedAt.Init(-1.0, int32(Setup.Players.size()));
	LastStructureDeath.Init(FVector2D::ZeroVector, int32(Setup.Players.size()));
	Defeats.Reset();
	DyingViews.Reset();
	DyingInfo.Reset();
	bHasLastAction = false;
	LastVisionVersion = MAX_uint32;
	DamageAlerts.Reset();
	for (int32 I = 0; I < S->World.UnitCount(); ++I)
	{
		const Sim::UnitView V = S->World.Unit(I);
		S->Curr.Add(V.Id, V);
	}
	S->Prev = S->Curr;
	delete State;
	State = S.Release();
	Accumulator = 0.0;
	bPaused = false;
	bReplaySaved = false;
	MatchResult = ERTSMatchResult::Playing;
	LastSentGroup.Reset();
	ShotsFired = 0;
	Lost = 0;
	Killed = 0;
	StartWorldSeconds = GetWorld()->GetTimeSeconds();
	LastDeathWorldSeconds = StartWorldSeconds;
	bReady = false;
	WarmStartSeconds = FPlatformTime::Seconds();
	SettledSince = -1.0;
	LastWantingTextures = -1;
	EndWorldSeconds = -1.0;
	for (const TPair<uint32, Sim::UnitView>& It : State->Curr)
	{
		SpawnView(It.Key);
	}
	for (const TObjectPtr<ARTSDockActor>& Dock : DockViews)
	{
		if (Dock)
		{
			Dock->Destroy();
		}
	}
	DockViews.Reset();
	PendingBlasts.Reset();
	UpdateDocks();
	// a fumaça das explosões só ouve o canal 1 (e a poeira o 2): o sol do mapa passa a
	// iluminar os dois, como o ambiente do BlastLab faz (BlastLabEnvironment SetVolumeChannel)
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		if (ULightComponent* L = It->GetLightComponent())
		{
			L->SetLightingChannels(true, true, true);
		}
	}
	if (UBlastFxDirector* Director = UBlastFxDirector::Get(this))
	{
		// as gravações compilam o shader agora (atrás do fade de entrada), não na 1ª morte
		const FVector2D Start = State->LocalStartCm;
		Director->Prewarm(FVector(Start.X, Start.Y, State->Baked.GroundZCm(Start) + 300.0f));
	}
	KeepLoaded.Reset();
	for (const TCHAR* Path : { TEXT("/Game/Materials/M_Glow.M_Glow"), TEXT("/Game/Materials/M_Emissive.M_Emissive"),
		TEXT("/Engine/BasicShapes/Sphere.Sphere"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder") })
	{
		if (UObject* Obj = LoadObject<UObject>(nullptr, Path))
		{
			KeepLoaded.Add(Obj);
		}
	}
	// os materiais da obra (o prédio troca pra eles enquanto constrói): carregados agora, os
	// shaders compilam na tela de carregar e a 1ª obra não engasga
	for (const TCHAR* Slot : { TEXT("Body"), TEXT("Trim"), TEXT("Dark"), TEXT("Yellow"), TEXT("Player"), TEXT("Concrete"), TEXT("Hazard"),
		TEXT("Glass"), TEXT("Light"), TEXT("Core"), TEXT("Beacon"), TEXT("Crate"), TEXT("Rubber"), TEXT("Paint") })
	{
		const FString Path = FString::Printf(TEXT("/Game/Structures/Materials/MI_%s_Build.MI_%s_Build"), Slot, Slot);
		if (UObject* Obj = LoadObject<UObject>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			KeepLoaded.Add(Obj);
		}
	}
	// o traçante e o clarão também: um de cada, invisível, no centro da vista
	{
		const FVector2D Start = State->LocalStartCm;
		const FVector At(Start.X, Start.Y, State->Baked.GroundZCm(Start) + 200.0f);
		ARTSEffect::SpawnFlash(GetWorld(), At, 50.0f, 0.0f, 0.5f, FLinearColor::Black);
		ARTSEffect::SpawnTracer(GetWorld(), At, At + FVector(300.0f, 0.0f, 0.0f), 600.0f);
	}
	UE_LOG(LogTemp, Log, TEXT("RTS skirmish em %s: %d unidades, semente %llu, montagem %016llx"),
		*MapName, State->World.UnitCount(), (unsigned long long)Setup.Seed, (unsigned long long)Setup.Hash());
	return true;
}

void URTSMatchSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bNetAwaitSetup)
	{
		URTSNetSession* N = Net();
		if (N && N->HasSetup())
		{
			const TArray<uint8>& Bytes = N->SetupBytes();
			Sim::ByteReader R(Bytes.GetData(), size_t(Bytes.Num()));
			Sim::MatchSetup Given;
			FString Error;
			if (!Sim::ReadSetup(R, Given))
			{
				UE_LOG(LogTemp, Error, TEXT("RTS LAN: a montagem do anfitrião não leu"));
				bNetAwaitSetup = false;
			}
			else if (!StartSkirmish(NetMapName, Error, &Given))
			{
				UE_LOG(LogTemp, Error, TEXT("RTS LAN: não deu pra montar a partida: %s"), *Error);
			}
			else if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				if (ARTSCameraPawn* Cam = Cast<ARTSCameraPawn>(PC->GetPawn()))
				{
					Cam->SetBounds(FBox2D(FVector2D(-46200.0, -46200.0), FVector2D(46200.0, 46200.0)));
					Cam->FocusOn(LocalStartCm());
				}
			}
		}
		return;
	}
	if (!(State != nullptr))
	{
		return;
	}
	if (!bReady)
	{
		UpdateWarmup();
		UpdateFog(DeltaTime);
		UpdateViews(DeltaTime);
		return;
	}
	if (bNet)
	{
		TickNet(DeltaTime);
		// o robô do teste (sem tela): sai se alguém sumiu; no fim, o anfitrião leva todos de volta
		// pra sala (como o Felipe faria no botão) e o cliente espera ser levado
		URTSNetSession* N = Net();
		if (bNetBot && N)
		{
			if (N->IsPeerLost())
			{
				UE_LOG(LogTemp, Log, TEXT("RTS LAN robô: fim (alguém sumiu)"));
				FPlatformMisc::RequestExit(false);
			}
			else if (MatchResult != ERTSMatchResult::Playing && N->IsHost() && N->GetPhase() == ERTSNetPhase::Ended && SecondsSinceEnd() > 8.0f)
			{
				UE_LOG(LogTemp, Log, TEXT("RTS LAN robô: a partida acabou; todos de volta pra sala"));
				N->BackToLobby();
				if (URTSSessionSubsystem* Session = GetWorld()->GetGameInstance()->GetSubsystem<URTSSessionSubsystem>())
				{
					Session->ReturnToLobby();
				}
			}
			else if (MatchResult != ERTSMatchResult::Playing && SecondsSinceEnd() > 120.0f)
			{
				UE_LOG(LogTemp, Warning, TEXT("RTS LAN robô: o anfitrião não voltou pra sala em 2 min; saindo"));
				FPlatformMisc::RequestExit(false);
			}
		}
	}
	else if (!bPaused)
	{
		Accumulator += FMath::Min(double(DeltaTime), 0.25);
		int32 Steps = 0;
		while (Accumulator >= StepSeconds && Steps < MaxStepsPerFrame)
		{
			StepOnce();
			Accumulator -= StepSeconds;
			++Steps;
		}
		if (Steps == MaxStepsPerFrame)
		{
			Accumulator = 0.0;   // não tenta recuperar o atraso de uma vez
		}
	}
	UpdateFog(DeltaTime);
	UpdateViews(DeltaTime);
}

void URTSMatchSubsystem::UpdateFog(float DeltaTime)
{
	if (!Fog || State == nullptr)
	{
		return;
	}
	const bool bShown = IsFogShown();
	const uint8 Team = State->World.TeamOf(LocalPlayer());
	const uint32 Version = State->World.VisionVersion(Team);
	static const std::vector<uint8> Unchanged;
	if (Version != LastVisionVersion)
	{
		LastVisionVersion = Version;
		std::vector<uint8> Vision;
		State->World.CopyVisibility(Team, Vision);
		Fog->Update(Vision, !bShown, DeltaTime);
	}
	else
	{
		Fog->Update(Unchanged, !bShown, DeltaTime);
	}
	if (bFogPassOn)
	{
		// a passada fica ligada; revelar é o nível subir pra 1 (no ritmo do ZH)
		RTSFogPass::SetStrength(1.0f);
	}
}

namespace
{
	/** Quanto tempo o inimigo continua desenhado depois de sair da visão (o ZH usa 2 s; a simulação já guarda 1 s). */
	constexpr double FogGraceSeconds = 1.0;
	/** Tudo calmo por este tanto antes de abrir: o Nanite busca as páginas finas pelo que é desenhado atrás da tela. */
	constexpr double WarmSettleSeconds = 1.0;
	/** Texturas pararam de chegar (o pool encheu): conta como pronto. */
	constexpr double TextureStallSeconds = 2.0;
	/** Teto: com o PC muito ocupado, abre assim mesmo. */
	constexpr double MaxWarmupSeconds = 25.0;
}

void URTSMatchSubsystem::UpdateWarmup()
{
	// Como os jogos fazem: a tela de carregar só sai com a cena pronta. Sem isso, com o PC
	// ocupado e o cache de shaders frio, a partida começava com o chão em xadrez (material
	// ainda compilando), pedras em malha grosseira (Nanite sem as páginas finas) e tanques
	// sem textura por quase um minuto (teste t6, 08/10). A simulação não anda enquanto isso.
	const double Now = FPlatformTime::Seconds();
	if (!FApp::CanEverRender())
	{
		// sem placa (-nullrhi, o 2º jogador do teste da LAN): não há cena pra esperar nem foto do minimapa
		bReady = true;
		if (URTSNetSession* N = Net())
		{
			N->SetLocalReady();
		}
		StartWorldSeconds = GetWorld()->GetTimeSeconds();
		LastDeathWorldSeconds = StartWorldSeconds;
		UE_LOG(LogTemp, Log, TEXT("RTS sem placa: a partida não espera a cena"));
		return;
	}
	const int32 Shaders = GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
	const int32 Textures = IStreamingManager::Get().GetNumWantingResources();
	if (LastWantingTextures < 0 || Textures < LastWantingTextures)
	{
		LastTextureProgress = Now;
	}
	LastWantingTextures = Textures;
	const bool bTexturesDone = Textures == 0 || Now - LastTextureProgress > TextureStallSeconds;
	const bool bClear = Shaders == 0 && !IsAsyncLoading() && bTexturesDone;
	SettledSince = bClear ? (SettledSince < 0.0 ? Now : SettledSince) : -1.0;
	const bool bSettled = SettledSince >= 0.0 && Now - SettledSince >= WarmSettleSeconds;
	const bool bCap = Now - WarmStartSeconds >= MaxWarmupSeconds;
	if (!bSettled && !bCap)
	{
		return;
	}
	if (Fog && !Fog->IsMinimapReady() && Now - WarmStartSeconds < MaxWarmupSeconds + 8.0)
	{
		// a foto de cima pro minimapa, com a cena já carregada e sem a névoa nem as unidades
		if (!Fog->IsMinimapCapturing())
		{
			TArray<AActor*> Hidden;
			for (const TPair<uint32, TObjectPtr<ARTSUnitActor>>& It : Views)
			{
				Hidden.Add(It.Value);
			}
			for (const TObjectPtr<ARTSUnitActor>& Wreck : Wrecks)
			{
				Hidden.Add(Wreck);
			}
			Fog->BeginMinimapCapture(GetWorld(), Hidden);
		}
		Fog->TickMinimapCapture();
		if (!Fog->IsMinimapReady())
		{
			return;
		}
	}
	if (Fog && !bFogPassOn)
	{
		RTSFogPass::Enable(Fog->VisionTexture(), Fog->MapMinCm(), Fog->MapSizeCm());
		bFogPassOn = true;
	}
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (ARTSCameraPawn* Cam = Cast<ARTSCameraPawn>(PC->GetPawn()))
		{
			Cam->SetFogExposureFix(true);
		}
	}
	bReady = true;
	if (URTSNetSession* N = Net())
	{
		N->SetLocalReady();
	}
	StartWorldSeconds = GetWorld()->GetTimeSeconds();
	LastDeathWorldSeconds = StartWorldSeconds;
	UE_LOG(LogTemp, Log, TEXT("RTS cena pronta em %.1f s%s (shaders na fila %d, texturas faltando %d)"),
		Now - WarmStartSeconds, bSettled ? TEXT("") : TEXT(", pelo teto"), Shaders, Textures);
}

void URTSMatchSubsystem::TickNet(float DeltaTime)
{
	URTSNetSession* N = Net();
	if (!N)
	{
		return;
	}
	if (!N->CanStartTicking())
	{
		Accumulator = 0.0;
		return;   // esperando os outros ficarem prontos
	}
	Accumulator += FMath::Min(double(DeltaTime), 0.25);
	int32 Steps = 0;
	if (N->IsHost())
	{
		// o relógio: fecha um passo a cada 1/30 s, e executa com 2 passos de folga (os clientes
		// recebem o passo com a ida da rede e tocam com a folga deles: fica justo)
		constexpr int32 HostDelayTicks = 2;
		while (Accumulator >= StepSeconds && Steps < MaxStepsPerFrame)
		{
			N->SealTick(N->NextSealTick());
			Accumulator -= StepSeconds;
			++Steps;
		}
		if (Steps == MaxStepsPerFrame)
		{
			Accumulator = 0.0;
		}
		int32 Ran = 0;
		while (int64(State->World.Tick()) + HostDelayTicks < int64(N->NextSealTick()) && Ran < MaxStepsPerFrame)
		{
			StepNet();
			++Ran;
		}
		return;
	}
	// cliente: toca no ritmo dele enquanto há passo; folga de 3 passos; alcança se ficou pra trás
	constexpr int32 TargetAhead = 3;
	while (Accumulator >= StepSeconds && N->HasTick(State->World.Tick()) && Steps < MaxStepsPerFrame)
	{
		StepNet();
		Accumulator -= StepSeconds;
		++Steps;
	}
	while (N->Ahead(State->World.Tick()) > TargetAhead + 3 && Steps < MaxStepsPerFrame)
	{
		StepNet();
		++Steps;
	}
	if (!N->HasTick(State->World.Tick()))
	{
		Accumulator = FMath::Min(Accumulator, StepSeconds);   // parado esperando: sem rajada depois
	}
}

void URTSMatchSubsystem::StepNet()
{
	FRTSMatchState& S = *State;
	URTSNetSession* N = Net();
	const double T0 = FPlatformTime::Seconds();
	const Sim::TickNumber Now = S.World.Tick();
	for (Sim::SkirmishAI& Brain : S.Brains)
	{
		// a IA da sala (no anfitrião) e o robô do teste: decidem como um jogador e mandam pela rede
		std::vector<Sim::Command> Orders;
		Brain.Think(S.World, Orders);
		for (const Sim::Command& C : Orders)
		{
			if (N->IsHost())
			{
				N->SubmitAs(uint8(Brain.Player()), C);
			}
			else
			{
				N->SubmitLocal(C);
			}
		}
	}
	const std::vector<Sim::Command> Due = N->TickCommands(Now);
	S.World.Step(Due);
	const uint64 Hash = S.World.StateHash();
	S.Replay.AddTick(Now, Due, Hash);
	N->ReportHash(Now, Hash);

	S.Prev = MoveTemp(S.Curr);
	S.Curr.Reset();
	for (int32 I = 0; I < S.World.UnitCount(); ++I)
	{
		const Sim::UnitView V = S.World.Unit(I);
		S.Curr.Add(V.Id, V);
	}
	LastStepMs = (FPlatformTime::Seconds() - T0) * 1000.0;
	HandleEvents();
}

void URTSMatchSubsystem::StepOnce()
{
	FRTSMatchState& S = *State;
	const double T0 = FPlatformTime::Seconds();
	const Sim::TickNumber Now = S.World.Tick();
	for (Sim::SkirmishAI& Brain : S.Brains)
	{
		std::vector<Sim::Command> Orders;
		Brain.Think(S.World, Orders);
		for (const Sim::Command& C : Orders)
		{
			S.Queue.Issue(C, Now);
		}
	}
	const std::vector<Sim::Command> Due = S.Queue.TakeFor(Now);
	S.World.Step(Due);
	S.Replay.AddTick(Now, Due, S.World.StateHash());

	S.Prev = MoveTemp(S.Curr);
	S.Curr.Reset();
	for (int32 I = 0; I < S.World.UnitCount(); ++I)
	{
		const Sim::UnitView V = S.World.Unit(I);
		S.Curr.Add(V.Id, V);
	}
	LastStepMs = (FPlatformTime::Seconds() - T0) * 1000.0;
	HandleEvents();
}

void URTSMatchSubsystem::HandleEvents()
{
	FRTSMatchState& S = *State;
	UWorld* World = GetWorld();
	for (const Sim::SimEvent& E : S.World.LastEvents())
	{
		switch (E.Type)
		{
		case Sim::EventType::UnitSpawned:
			SpawnView(E.Target);
			break;
		case Sim::EventType::WeaponFired:
		{
			ARTSUnitActor* Shooter = ViewOf(E.Source);
			if (!Shooter)
			{
				break;
			}
			// quem atira de dentro da névoa: nem clarão nem luz (entregariam a posição)
			const bool bShooterShown = IsUnitShown(E.Source);
			if (bShooterShown)
			{
				Shooter->PlayFire();
			}
			if (ShotsFired == 0)
			{
				// o relógio do impasse começa no primeiro tiro, não na largada
				LastDeathWorldSeconds = World ? World->GetTimeSeconds() : LastDeathWorldSeconds;
			}
			++ShotsFired;
			const Sim::UnitView* V = S.Curr.Find(E.Source);
			const double SpeedCmPerS = V ? S.World.Rules().Units[V->Def].Weapon.ProjectileStep.ToDouble() * Sim::TicksPerSecond * 100.0 : 10000.0;
			const FVector2D TargetCm = RTS::ToWorld(E.Point);
			const FVector Target(TargetCm.X, TargetCm.Y, S.Baked.GroundZCm(TargetCm) + 120.0f);
			if (bShooterShown)
			{
				ARTSEffect::SpawnTracer(World, Shooter->MuzzleLocation(), Target, float(SpeedCmPerS));
			}
			else
			{
				// o traçante aparece de onde a linha entra na área à vista (ZH: o projétil surge na borda)
				const FVector From = Shooter->MuzzleLocation();
				const int32 Samples = 32;
				for (int32 K = 1; K <= Samples; ++K)
				{
					const FVector P = FMath::Lerp(From, Target, float(K) / Samples);
					if (IsPointShown(FVector2D(P.X, P.Y)))
					{
						if (K < Samples)
						{
							ARTSEffect::SpawnTracer(World, P, Target, float(SpeedCmPerS));
						}
						break;
					}
				}
			}
			break;
		}
		case Sim::EventType::ProjectileHit:
		{
			const FVector2D At = RTS::ToWorld(E.Point);
			if (IsPointShown(At))
			{
				ARTSEffect::SpawnFlash(World, FVector(At.X, At.Y, S.Baked.GroundZCm(At) + 130.0f), 130.0f, 4.0e6f, 0.16f, FLinearColor(3.2f, 1.9f, 0.7f));
			}
			break;
		}
		case Sim::EventType::UnitDamaged:
		{
			const Sim::UnitView* Victim = S.Curr.Find(E.Target);
			if (Victim && Victim->Owner == LocalPlayer())
			{
				DamageAlerts.Add(RTS::ToWorld(Victim->Pos));
			}
			break;
		}
		case Sim::EventType::UnitDied:
		{
			const uint8 Owner = uint8(E.Value);
			(Owner == LocalPlayer() ? Lost : Killed) += 1;
			const double WorldNow = World ? World->GetTimeSeconds() : 0.0;
			LastDeathWorldSeconds = WorldNow;
			const FVector2D At = RTS::ToWorld(E.Point);
			TObjectPtr<ARTSUnitActor>* View = Views.Find(E.Target);
			ARTSUnitActor* Actor = View ? View->Get() : nullptr;
			if (Actor && Actor->IsStructure() && LastStructureDeath.IsValidIndex(Owner))
			{
				LastStructureDeath[Owner] = At;
			}
			if (Actor && DefeatedAt.IsValidIndex(Owner) && DefeatedAt[Owner] >= 0.0 && WorldNow - DefeatedAt[Owner] < 3.0)
			{
				// o que sobrou do derrotado: cai em onda a partir do último prédio dele (~35 m/s),
				// em vez de tudo no mesmo quadro (a base inteira explodindo de uma vez trava o PC)
				const double Dist = FVector2D::Distance(At, LastStructureDeath[Owner]);
				const double Jitter = double((E.Target * 2654435761u) >> 24) / 255.0 * 0.25;
				FDyingInfo Info;
				Info.At = WorldNow + FMath::Clamp(Dist / 3500.0, 0.15, 3.5) + Jitter;
				Info.Cm = At;
				Info.Id = E.Target;
				Info.Owner = Owner;
				DyingViews.Add(Actor);
				DyingInfo.Add(Info);
				Views.Remove(E.Target);
				break;
			}
			if (Actor)
			{
				Views.Remove(E.Target);
				Die(*Actor, At, E.Target, Owner);
			}
			break;
		}
		case Sim::EventType::PlayerDefeated:
		{
			const uint8 Who = uint8(E.Value);
			const double WorldNow = World ? World->GetTimeSeconds() : 0.0;
			if (DefeatedAt.IsValidIndex(Who))
			{
				DefeatedAt[Who] = WorldNow;
			}
			FRTSDefeatNotice Notice;
			Notice.Player = Who;
			Notice.At = WorldNow;
			Defeats.Add(Notice);
			UE_LOG(LogTemp, Log, TEXT("RTS %s foi derrotado (vaga %d)%s"), *PlayerColorName(Who).ToString(), int32(Who), Who == LocalPlayer() ? TEXT(": o jogador local vira espectador") : TEXT(""));
			if (Who == LocalPlayer() && LastStructureDeath.IsValidIndex(Who))
			{
				// a câmera vai até onde caiu o último prédio (o "você foi derrotado" na cara)
				if (APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr)
				{
					if (ARTSCameraPawn* Cam = Cast<ARTSCameraPawn>(PC->GetPawn()))
					{
						Cam->GlideTo(LastStructureDeath[Who]);
					}
				}
			}
			break;
		}
		case Sim::EventType::SupplyDelivered:
		{
			// o "+$300" do ZH (SupplyCenterDockUpdate): só pro dono do caminhão
			const Sim::UnitView* Truck = S.Curr.Find(E.Source);
			const Sim::UnitView* Center = S.Curr.Find(E.Target);
			if (Truck && Truck->Owner == LocalPlayer() && E.Value > 0)
			{
				FRTSMoneyPopup Pop;
				Pop.PosCm = RTS::ToWorld(Center ? Center->Pos : E.Point);
				Pop.Amount = E.Value;
				MoneyPopups.Add(Pop);
			}
			break;
		}
		case Sim::EventType::StructureCancelled:
			// a obra cancelada some sem explodir nem virar carcaça
			if (TObjectPtr<ARTSUnitActor>* View = Views.Find(E.Target))
			{
				(*View)->Destroy();
				Views.Remove(E.Target);
			}
			break;
		case Sim::EventType::MatchOver:
		{
			const int32 LocalTeam = S.Setup.Players[LocalPlayer()].Team;
			MatchResult = E.Value < 0 ? ERTSMatchResult::Draw : (E.Value == LocalTeam ? ERTSMatchResult::Victory : ERTSMatchResult::Defeat);
			if (URTSNetSession* N = Net())
			{
				N->SetEnded();
				UE_LOG(LogTemp, Log, TEXT("RTS LAN fim no passo %u: vaga %d, %s, dessincronia %s, %lld impressões digitais iguais, estado %016llx"),
					unsigned(S.World.Tick()), int32(LocalSlot), MatchResult == ERTSMatchResult::Victory ? TEXT("vitória") : (MatchResult == ERTSMatchResult::Defeat ? TEXT("derrota") : TEXT("empate")),
					N->IsDesynced() ? TEXT("SIM") : TEXT("não"), N->HashMatches(), (unsigned long long)S.World.StateHash());
			}
			EndWorldSeconds = World ? World->GetTimeSeconds() : 0.0;
			SaveReplay();
			break;
		}
		default:
			break;
		}
	}
}

void URTSMatchSubsystem::Die(ARTSUnitActor& View, const FVector2D& Cm, uint32 Id, uint8 Owner)
{
	// a explosão só se o jogador vê (a de dentro da névoa não acontece pra ele)
	const bool bSeen = Owner == LocalPlayer() || IsPointShown(Cm);
	if (bSeen)
	{
		LastAction = Cm;
		bHasLastAction = true;
		if (View.IsStructure())
		{
			StructureDeath(View, Cm, Id);
		}
		else if (UBlastFxDirector* Director = UBlastFxDirector::Get(this))
		{
			Director->TankDeath(FVector(Cm.X, Cm.Y, State ? State->Baked.GroundZCm(Cm) : 0.0f), Id);
		}
	}
	View.bWreckKnown = bSeen;
	View.Die();
	Wrecks.Add(&View);
}

void URTSMatchSubsystem::UpdateDying()
{
	if (DyingInfo.IsEmpty())
	{
		return;
	}
	const double WorldNow = GetWorld()->GetTimeSeconds();
	for (int32 K = DyingInfo.Num() - 1; K >= 0; --K)
	{
		if (DyingInfo[K].At <= WorldNow)
		{
			if (ARTSUnitActor* Actor = DyingViews[K].Get())
			{
				Die(*Actor, DyingInfo[K].Cm, DyingInfo[K].Id, DyingInfo[K].Owner);
			}
			DyingViews.RemoveAt(K);
			DyingInfo.RemoveAt(K);
		}
	}
}

uint8 URTSMatchSubsystem::TeamOf(uint8 Player) const
{
	return State != nullptr && Player < State->World.PlayerCount() ? State->World.TeamOf(Player) : Player;
}

bool URTSMatchSubsystem::IsAllyOfLocal(uint8 Owner) const
{
	return State != nullptr && Owner != LocalPlayer() && Owner < State->World.PlayerCount() && !State->World.AreEnemies(LocalPlayer(), Owner);
}

bool URTSMatchSubsystem::IsLocalDefeated() const
{
	return IsDefeated(LocalPlayer());
}

bool URTSMatchSubsystem::IsSpectatingAll() const
{
	if (!IsLocalDefeated())
	{
		return false;
	}
	for (int32 P = 0; P < State->World.PlayerCount(); ++P)
	{
		if (P != LocalPlayer() && !State->World.AreEnemies(LocalPlayer(), Sim::PlayerIndex(P)) && !State->World.Player(Sim::PlayerIndex(P)).bDefeated)
		{
			return false;   // o aliado segue vivo: só a visão do time
		}
	}
	return true;
}

void URTSMatchSubsystem::StructureDeath(const ARTSUnitActor& View, const FVector2D& CenterCm, uint32 Id)
{
	// o prédio não some numa explosão só (o ZH solta uma série pela área: CommandCenter
	// DestroyedFX, ExplosionStructure*): a grande no meio e 3 menores espalhadas em 1,5 s
	UBlastFxDirector* Director = UBlastFxDirector::Get(this);
	UWorld* World = GetWorld();
	if (!Director || !World || State == nullptr)
	{
		return;
	}
	Director->TankDeath(FVector(CenterCm.X, CenterCm.Y, State->Baked.GroundZCm(CenterCm)), Id);
	const FVector Extent = View.GetComponentsBoundingBox(true, false).GetExtent();   // true: as peças não têm colisão
	uint32 Seed = Id * 2654435761u | 1u;
	const auto Next = [&Seed]()
	{
		Seed ^= Seed << 13; Seed ^= Seed >> 17; Seed ^= Seed << 5;
		return float(Seed & 0xFFFF) / 65535.0f;
	};
	const double Now = World->GetTimeSeconds();
	for (int32 K = 0; K < 3; ++K)
	{
		const FVector2D P = CenterCm + FVector2D((Next() * 1.4f - 0.7f) * Extent.X, (Next() * 1.4f - 0.7f) * Extent.Y);
		FPendingBlast B;
		B.At = Now + 0.35 + 0.4 * K + 0.2 * Next();
		B.Ground = FVector(P.X, P.Y, State->Baked.GroundZCm(P));
		B.Variant = Id + 101u * uint32(K + 1);
		PendingBlasts.Add(B);
	}
}

void URTSMatchSubsystem::UpdateDocks()
{
	if (State == nullptr || !GetWorld())
	{
		return;
	}
	const FRTSDockVisual& Visual = State->Data.DockVisual;
	const int32 N = DockCount();
	for (int32 K = 0; K < N; ++K)
	{
		FRTSDockState D;
		if (!GetDock(K, D))
		{
			continue;
		}
		if (!DockViews.IsValidIndex(K) || !DockViews[K])
		{
			// os depósitos do mapa nascem com a partida; a pilha, quando o caminhão cai
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ARTSDockActor* Dock = GetWorld()->SpawnActor<ARTSDockActor>(FVector(D.PosCm.X, D.PosCm.Y, State->Baked.GroundZCm(D.PosCm)), FRotator::ZeroRotator, Params);
			if (!Dock)
			{
				continue;
			}
			Dock->Setup(Visual, D.bPile, uint32(K) * 2246822519u + 7u);
			if (DockViews.Num() <= K)
			{
				DockViews.SetNum(K + 1);
			}
			DockViews[K] = Dock;
		}
		DockViews[K]->SetBoxes(D.Boxes, D.StartBoxes);
		if (D.bPile)
		{
			// a pilha caída é coisa da batalha: só aparece onde o jogador vê
			const bool bHide = IsFogShown() && !IsPointShown(D.PosCm);
			DockViews[K]->SetActorHiddenInGame(bHide || D.Boxes <= 0);
		}
	}
}

void URTSMatchSubsystem::SpawnView(uint32 Id)
{
	if (Views.Contains(Id))
	{
		return;
	}
	const Sim::UnitView* V = State->Curr.Find(Id);
	const FRTSUnitVisual* Visual = V ? State->Data.FindVisual(V->Def) : nullptr;
	if (!V || !Visual)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector2D Cm = RTS::ToWorld(V->Pos);
	ARTSUnitActor* Actor = GetWorld()->SpawnActor<ARTSUnitActor>(FVector(Cm.X, Cm.Y, State->Baked.GroundZCm(Cm)), FRotator::ZeroRotator, Params);
	if (!Actor)
	{
		return;
	}
	Actor->Setup(Id, *Visual);
	Actor->SetPlayerColor(PlayerColor(V->Owner));
	Views.Add(Id, Actor);
}

void URTSMatchSubsystem::UpdateViews(float DeltaTime)
{
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bFog = IsFogShown();
	for (const TPair<uint32, TObjectPtr<ARTSUnitActor>>& It : Views)
	{
		FRTSUnitState U;
		if (It.Value && GetUnit(It.Key, U))
		{
			const FVector Ground(U.PosCm.X, U.PosCm.Y, State->Baked.GroundZCm(U.PosCm));
			It.Value->SetPose(Ground, U.HullYawDeg, U.TurretYawDeg, State->Baked.GroundNormal(U.PosCm), DeltaTime);
			if (U.bStructure)
			{
				// as luzes e o que gira apagam no apagão (consumo maior que a produção; o CC
				// sozinho, sem consumo, fica aceso)
				const int32 Used = PowerUsed(U.Owner);
				It.Value->SetStructureState(U.BuildFraction, Used <= 0 || PowerMade(U.Owner) >= Used);
			}
			else
			{
				It.Value->SetCargo(U.Cargo);
				It.Value->SetWorking(U.Task == uint8(Sim::UnitTask::Construct) && !U.bMoving);
			}
			if (!bFog || !IsEnemyOfLocal(U.Owner) || (Fog && Fog->IsVisibleNow(U.PosCm)))
			{
				It.Value->LastSeenSeconds = Now;
			}
			// o ZH segue mostrando o objeto um instante depois de sair do claro (GameClient: 2 s); o
			// prédio inimigo já visto fica como fantasma, no lugar onde foi visto (ZH)
			const bool bKnown = It.Value->LastSeenSeconds > -1.0e8;
			It.Value->SetFogHidden(bFog && (U.bStructure ? !bKnown : Now - It.Value->LastSeenSeconds > FogGraceSeconds));
		}
	}
	UpdateDocks();
	UpdateDying();
	if (PendingBlasts.Num())
	{
		const double WorldNow = GetWorld()->GetTimeSeconds();
		UBlastFxDirector* Director = UBlastFxDirector::Get(this);
		for (int32 K = PendingBlasts.Num() - 1; K >= 0; --K)
		{
			if (PendingBlasts[K].At <= WorldNow)
			{
				if (Director)
				{
					Director->TankDeath(PendingBlasts[K].Ground, PendingBlasts[K].Variant);
				}
				PendingBlasts.RemoveAtSwap(K);
			}
		}
	}
	for (const TObjectPtr<ARTSUnitActor>& Wreck : Wrecks)
	{
		if (!Wreck)
		{
			continue;
		}
		const FVector Loc = Wreck->GetActorLocation();
		const bool bVisible = !bFog || (Fog && Fog->IsVisibleNow(FVector2D(Loc.X, Loc.Y)));
		Wreck->bWreckKnown |= bVisible;
		Wreck->SetFogHidden(!Wreck->bWreckKnown);
		Wreck->SetWreckLive(bVisible);
	}
}

bool URTSMatchSubsystem::IsFogShown() const
{
	// o espectador que viu o time inteiro cair vê tudo (só desenho; a regra da simulação não muda)
	return State != nullptr && bFogDisplay && MatchResult == ERTSMatchResult::Playing && !IsSpectatingAll();
}

void URTSMatchSubsystem::ToggleFogReveal()
{
	if (State == nullptr || bNet)
	{
		return;   // na rede o F9 mostraria o mapa todo
	}
	bFogDisplay = !bFogDisplay;
	// a regra acompanha pela fila (vale no passo seguinte, e até pausado ou depois do fim)
	Sim::Command C;
	C.Player = LocalPlayer();
	C.Type = Sim::CommandType::SetFog;
	C.Target = bFogDisplay ? 1 : 0;
	Issue(C);
	UE_LOG(LogTemp, Log, TEXT("RTS névoa %s (depuração)"), bFogDisplay ? TEXT("ligada") : TEXT("desligada"));
}

bool URTSMatchSubsystem::IsUnitShown(uint32 Id) const
{
	if (const TObjectPtr<ARTSUnitActor>* View = Views.Find(Id))
	{
		return *View && !(*View)->IsFogHidden();
	}
	return false;
}

bool URTSMatchSubsystem::IsPointShown(const FVector2D& Cm) const
{
	return !IsFogShown() || (Fog && Fog->IsVisibleNow(Cm));
}

FVector2D URTSMatchSubsystem::PlayerStartCm(uint8 Player) const
{
	return State != nullptr && Player < State->World.PlayerCount() ? RTS::ToWorld(State->World.PlayerStart(Player)) : FVector2D::ZeroVector;
}

void URTSMatchSubsystem::TakeDamageAlerts(TArray<FVector2D>& Out)
{
	Out = MoveTemp(DamageAlerts);
	DamageAlerts.Reset();
}

bool URTSMatchSubsystem::GetUnit(uint32 Id, FRTSUnitState& Out) const
{
	if (!(State != nullptr))
	{
		return false;
	}
	const Sim::UnitView* C = State->Curr.Find(Id);
	if (!C)
	{
		return false;
	}
	const Sim::UnitView* P = State->Prev.Find(Id);
	if (!P)
	{
		P = C;
	}
	// desenha entre o passo anterior e o atual (o desenho anda um passo atrás da simulação)
	const float Alpha = float(FMath::Clamp(Accumulator / StepSeconds, 0.0, 1.0));
	Out.Id = Id;
	Out.Owner = C->Owner;
	Out.Def = C->Def;
	Out.PosCm = FMath::Lerp(RTS::ToWorld(P->Pos), RTS::ToWorld(C->Pos), double(Alpha));
	const float HullP = RTS::YawDegrees(P->Hull);
	const float TurretP = RTS::YawDegrees(P->Turret);
	Out.HullYawDeg = HullP + FMath::FindDeltaAngleDegrees(HullP, RTS::YawDegrees(C->Hull)) * Alpha;
	Out.TurretYawDeg = TurretP + FMath::FindDeltaAngleDegrees(TurretP, RTS::YawDegrees(C->Turret)) * Alpha;
	Out.Hp = C->Hp;
	Out.MaxHp = FMath::Max(1, C->MaxHp);
	Out.bMoving = C->Speed.Raw > 0;
	Out.bStructure = C->bStructure;
	Out.BuildFraction = C->BuildLeft > 0 && C->BuildTicks > 0 ? 1.0f - float(C->BuildLeft) / float(C->BuildTicks) : 1.0f;
	Out.Dir = C->Dir;
	Out.QueueSize = C->QueueSize;
	Out.QueueHead = C->QueueHead;
	Out.QueueFraction = C->QueuePermille / 1000.0f;
	Out.Task = uint8(C->Task);
	Out.Cargo = C->Cargo;
	Out.bHasRally = C->bHasRally;
	Out.RallyCm = RTS::ToWorld(C->Rally);
	return true;
}

// ---------------------------------------------------------------- economia e construção

int32 URTSMatchSubsystem::Money(uint8 Player) const
{
	return State != nullptr && Player < State->World.PlayerCount() ? State->World.Money(Player) : 0;
}

int32 URTSMatchSubsystem::PowerMade(uint8 Player) const
{
	return State != nullptr && Player < State->World.PlayerCount() ? State->World.Player(Player).PowerMade : 0;
}

int32 URTSMatchSubsystem::PowerUsed(uint8 Player) const
{
	return State != nullptr && Player < State->World.PlayerCount() ? State->World.Player(Player).PowerUsed : 0;
}

float URTSMatchSubsystem::ProductionSpeed(uint8 Player) const
{
	return State != nullptr && Player < State->World.PlayerCount() ? State->World.PowerFactorPermille(Player) / 1000.0f : 1.0f;
}

int32 URTSMatchSubsystem::PlayerCount() const
{
	return State != nullptr ? State->World.PlayerCount() : 0;
}

bool URTSMatchSubsystem::IsDefeated(uint8 Player) const
{
	return State != nullptr && Player < State->World.PlayerCount() && State->World.Player(Player).bDefeated;
}

int32 URTSMatchSubsystem::DefCount() const
{
	return State != nullptr ? int32(State->World.Rules().Units.size()) : 0;
}

bool URTSMatchSubsystem::GetDef(uint16 Def, FRTSDefInfo& Out) const
{
	if (State == nullptr || Def >= State->World.Rules().Units.size())
	{
		return false;
	}
	const Sim::UnitDef& D = State->World.Rules().Units[Def];
	Out = FRTSDefInfo();
	Out.Key = UTF8_TO_TCHAR(D.Key.c_str());
	const FRTSUnitVisual* Visual = State->Data.FindVisual(Def);
	Out.Name = Visual && !Visual->DisplayName.IsEmpty() ? Visual->DisplayName : Out.Key;
	Out.bStructure = D.IsStructure();
	Out.bArmed = D.HasWeapon();
	Out.Cost = D.Cost;
	Out.BuildSeconds = float(D.BuildTicks) / float(Sim::TicksPerSecond);
	Out.Energy = D.Energy;
	Out.SizeM = FVector2D(D.HalfX.ToDouble() * 2.0, D.HalfY.ToDouble() * 2.0);
	Out.SupplyBoxes = D.SupplyBoxes;
	Out.bSupplyCenter = D.bSupplyCenter;
	for (const uint16 K : D.Produces) { Out.Produces.Add(K); }
	for (const uint16 K : D.Constructs) { Out.Constructs.Add(K); }
	for (const uint16 K : D.Requires) { Out.Requires.Add(K); }
	return true;
}

bool URTSMatchSubsystem::HasPrerequisites(uint16 Def) const
{
	return State != nullptr && Def < State->World.Rules().Units.size() && State->World.HasPrerequisites(LocalPlayer(), Def);
}

void URTSMatchSubsystem::GetQueue(uint32 Structure, TArray<uint16>& Out) const
{
	Out.Reset();
	if (State == nullptr)
	{
		return;
	}
	std::vector<uint16_t> Q;
	State->World.QueueOf(Structure, Q);
	for (const uint16_t K : Q)
	{
		Out.Add(K);
	}
}

int32 URTSMatchSubsystem::BoxValue() const
{
	return State != nullptr ? State->World.Rules().Economy.BoxValue : 0;
}

int32 URTSMatchSubsystem::DockCount() const
{
	return State != nullptr ? State->World.DockCount() : 0;
}

bool URTSMatchSubsystem::GetDock(int32 Index, FRTSDockState& Out) const
{
	if (State == nullptr || Index < 0 || Index >= State->World.DockCount())
	{
		return false;
	}
	const Sim::DockView D = State->World.Dock(Index);
	Out.PosCm = RTS::ToWorld(D.Pos);
	Out.HalfCm = FVector2D(D.HalfX.ToDouble() * 100.0, D.HalfY.ToDouble() * 100.0);
	Out.Boxes = D.Boxes;
	Out.StartBoxes = D.StartBoxes;
	Out.bPile = D.bPile;
	return true;
}

ERTSPlace URTSMatchSubsystem::CanPlace(uint16 Def, int32 Dir, const FVector2D& Cm, FVector2D& OutCenterCm, FVector2D& OutHalfCm) const
{
	if (State == nullptr || Def >= State->World.Rules().Units.size())
	{
		return ERTSPlace::NotAStructure;
	}
	const Sim::UnitDef& D = State->World.Rules().Units[Def];
	const Sim::Vec2 At = RTS::ToSim(Cm);
	OutCenterCm = RTS::ToWorld(State->World.SnapStructure(Def, Dir, At));
	const double HX = ((Dir & 1) ? D.HalfY : D.HalfX).ToDouble() * 100.0;
	const double HY = ((Dir & 1) ? D.HalfX : D.HalfY).ToDouble() * 100.0;
	OutHalfCm = FVector2D(HX, HY);
	return ERTSPlace(uint8(State->World.CanPlace(LocalPlayer(), Def, Dir, At)));
}

FString URTSMatchSubsystem::PlaceReason(ERTSPlace Result)
{
	// o jogo é em inglês por padrão (09/10): a tradução entra pelo LOCTEXT
	switch (Result)
	{
	case ERTSPlace::Ok: return FString();
	case ERTSPlace::OutOfMap: return NSLOCTEXT("RTSHUD", "PlaceOutOfMap", "Outside the map").ToString();
	case ERTSPlace::NearEdge: return NSLOCTEXT("RTSHUD", "PlaceNearEdge", "Too close to the map edge").ToString();
	case ERTSPlace::Blocked: return NSLOCTEXT("RTSHUD", "PlaceBlocked", "Something is in the way").ToString();
	case ERTSPlace::NotFlat: return NSLOCTEXT("RTSHUD", "PlaceNotFlat", "Ground too uneven").ToString();
	case ERTSPlace::NearSupplies: return NSLOCTEXT("RTSHUD", "PlaceNearSupplies", "Too close to the supplies").ToString();
	case ERTSPlace::BlocksExit: return NSLOCTEXT("RTSHUD", "PlaceBlocksExit", "Blocks a factory exit").ToString();
	case ERTSPlace::Occupied: return NSLOCTEXT("RTSHUD", "PlaceOccupied", "Enemy units in the way").ToString();
	case ERTSPlace::NotVisible: return NSLOCTEXT("RTSHUD", "PlaceNotVisible", "Can't build where you can't see").ToString();
	case ERTSPlace::MissingPrereq: return NSLOCTEXT("RTSHUD", "PlaceMissingPrereq", "Requires another building first").ToString();
	case ERTSPlace::NoMoney: return NSLOCTEXT("RTSHUD", "PlaceNoMoney", "Insufficient funds").ToString();
	default: return NSLOCTEXT("RTSHUD", "PlaceInvalid", "Can't build here").ToString();
	}
}

void URTSMatchSubsystem::TakeMoneyPopups(TArray<FRTSMoneyPopup>& Out)
{
	Out = MoneyPopups;
	MoneyPopups.Reset();
}

namespace
{
	bool CanOrder(const FRTSMatchState* State, bool bReady, ERTSMatchResult Result)
	{
		return State != nullptr && bReady && Result == ERTSMatchResult::Playing;
	}
}

void URTSMatchSubsystem::OrderProduce(uint32 Structure, uint16 Def)
{
	if (!CanOrder(State, bReady, MatchResult))
	{
		return;
	}
	Sim::Command C;
	C.Player = LocalPlayer();
	C.Type = Sim::CommandType::Produce;
	C.Target = Structure;
	C.Value = Def;
	Issue(C);
}

void URTSMatchSubsystem::OrderCancelProduce(uint32 Structure, int32 Index)
{
	if (!CanOrder(State, bReady, MatchResult))
	{
		return;
	}
	Sim::Command C;
	C.Player = LocalPlayer();
	C.Type = Sim::CommandType::CancelProduce;
	C.Target = Structure;
	C.Value = Index;
	Issue(C);
}

void URTSMatchSubsystem::OrderBuild(const TArray<uint32>& Dozers, uint16 Def, int32 Dir, const FVector2D& Cm)
{
	if (!CanOrder(State, bReady, MatchResult) || Dozers.IsEmpty())
	{
		return;
	}
	IssueGroupIfChanged(Dozers);
	Sim::Command C;
	C.Player = LocalPlayer();
	C.Type = Sim::CommandType::Build;
	C.Value = Sim::BuildValue(Def, Dir);
	C.Point = RTS::ToSim(Cm);
	Issue(C);
}

void URTSMatchSubsystem::OrderResumeBuild(const TArray<uint32>& Dozers, uint32 Foundation)
{
	if (!CanOrder(State, bReady, MatchResult) || Dozers.IsEmpty())
	{
		return;
	}
	IssueGroupIfChanged(Dozers);
	Sim::Command C;
	C.Player = LocalPlayer();
	C.Type = Sim::CommandType::Build;
	C.Target = Foundation;
	Issue(C);
}

void URTSMatchSubsystem::OrderCancelBuild(uint32 Structure)
{
	if (!CanOrder(State, bReady, MatchResult))
	{
		return;
	}
	Sim::Command C;
	C.Player = LocalPlayer();
	C.Type = Sim::CommandType::CancelBuild;
	C.Target = Structure;
	Issue(C);
}

void URTSMatchSubsystem::OrderRally(const TArray<uint32>& Structures, const FVector2D& Cm)
{
	if (!CanOrder(State, bReady, MatchResult))
	{
		return;
	}
	for (const uint32 Id : Structures)
	{
		Sim::Command C;
		C.Player = LocalPlayer();
		C.Type = Sim::CommandType::SetRally;
		C.Target = Id;
		C.Point = RTS::ToSim(Cm);
		Issue(C);
	}
}

void URTSMatchSubsystem::OrderGather(const TArray<uint32>& Trucks, int32 Dock)
{
	if (!CanOrder(State, bReady, MatchResult) || Trucks.IsEmpty())
	{
		return;
	}
	IssueGroupIfChanged(Trucks);
	Sim::Command C;
	C.Player = LocalPlayer();
	C.Type = Sim::CommandType::Gather;
	C.Value = Dock;
	Issue(C);
}

void URTSMatchSubsystem::GetUnits(TArray<FRTSUnitState>& Out) const
{
	Out.Reset();
	if (!(State != nullptr))
	{
		return;
	}
	for (const TPair<uint32, Sim::UnitView>& It : State->Curr)
	{
		FRTSUnitState U;
		if (GetUnit(It.Key, U))
		{
			Out.Add(U);
		}
	}
}

bool URTSMatchSubsystem::IsEnemyOfLocal(uint8 Owner) const
{
	return (State != nullptr) && Owner < State->World.PlayerCount() && State->World.AreEnemies(LocalPlayer(), Owner);
}

const FRTSUnitVisual* URTSMatchSubsystem::VisualOf(uint16 Def) const
{
	return (State != nullptr) ? State->Data.FindVisual(Def) : nullptr;
}

ARTSUnitActor* URTSMatchSubsystem::ViewOf(uint32 Id) const
{
	const TObjectPtr<ARTSUnitActor>* View = Views.Find(Id);
	return View ? View->Get() : nullptr;
}

const FRTSBakedMap* URTSMatchSubsystem::BakedMap() const
{
	return (State != nullptr) ? &State->Baked : nullptr;
}

FVector2D URTSMatchSubsystem::LocalStartCm() const
{
	return (State != nullptr) ? State->LocalStartCm : FVector2D::ZeroVector;
}

float URTSMatchSubsystem::MatchSeconds() const
{
	if (State == nullptr)
	{
		return 0.0f;
	}
	const Sim::TickNumber T = State->World.IsMatchOver() ? State->World.EndTick() : State->World.Tick();
	return float(T) / float(Sim::TicksPerSecond);
}

float URTSMatchSubsystem::SecondsSinceStart() const
{
	return State != nullptr ? float(GetWorld()->GetTimeSeconds() - StartWorldSeconds) : 0.0f;
}

float URTSMatchSubsystem::SecondsSinceEnd() const
{
	return EndWorldSeconds >= 0.0 ? float(GetWorld()->GetTimeSeconds() - EndWorldSeconds) : 0.0f;
}

FString URTSMatchSubsystem::DebugLine() const
{
	if (!(State != nullptr))
	{
		return FString();
	}
	return FString::Printf(TEXT("tick %u   hash %016llx   %.2f ms/tick   %d units   fog %s (F9)"),
		State->World.Tick(), (unsigned long long)State->World.StateHash(), LastStepMs, State->World.UnitCount(),
		bFogDisplay ? TEXT("on") : TEXT("off"));
}

void URTSMatchSubsystem::LogUnits(const TCHAR* Why) const
{
	if (State == nullptr)
	{
		return;
	}
	static const TCHAR* Orders[] = { TEXT("parada"), TEXT("andar"), TEXT("atacar"), TEXT("ataque andando") };
	UE_LOG(LogTemp, Display, TEXT("RTS UNIDADES (%s) no passo %u:"), Why, State->World.Tick());
	for (int32 I = 0; I < State->World.UnitCount(); ++I)
	{
		const Sim::UnitView V = State->World.Unit(I);
		const FVector2D P = RTS::ToWorld(V.Pos) / 100.0;
		const FVector2D O = RTS::ToWorld(V.OrderPoint) / 100.0;
		UE_LOG(LogTemp, Display, TEXT("RTS   #%u jogador %u  (%.0f, %.0f)  vida %d  %s -> (%.0f, %.0f)  persegue %u  mira %u  caminho %d  vel %.2f"),
			V.Id, V.Owner, P.X, P.Y, V.Hp, Orders[int32(V.Order) & 3], O.X, O.Y, V.Engage, V.FireTarget, V.PathLeft, V.Speed.ToDouble());
	}
}

void URTSMatchSubsystem::IssueGroupIfChanged(const TArray<uint32>& Units)
{
	TArray<uint32> Sorted = Units;
	Sorted.Sort();
	if (Sorted.Num() > Sim::MaxGroupSize)
	{
		Sorted.SetNum(Sim::MaxGroupSize);   // o teto do comando (a rede e o replay recusam acima disso)
	}
	if (Sorted == LastSentGroup)
	{
		return;
	}
	Sim::Command C;
	C.Player = LocalPlayer();
	C.Type = Sim::CommandType::SetGroup;
	for (const uint32 Id : Sorted)
	{
		C.Units.push_back(Id);
	}
	Issue(C);
	LastSentGroup = MoveTemp(Sorted);
}

void URTSMatchSubsystem::OrderMove(const TArray<uint32>& Units, const FVector2D& TargetCm, bool bAttackMove)
{
	if (!(State != nullptr) || !bReady || Units.IsEmpty() || MatchResult != ERTSMatchResult::Playing)
	{
		return;
	}
	IssueGroupIfChanged(Units);
	Sim::Command C;
	C.Player = LocalPlayer();
	C.Type = bAttackMove ? Sim::CommandType::AttackMove : Sim::CommandType::Move;
	C.Point = RTS::ToSim(TargetCm);
	Issue(C);
}

void URTSMatchSubsystem::OrderAttack(const TArray<uint32>& Units, uint32 TargetId)
{
	if (!(State != nullptr) || !bReady || Units.IsEmpty() || MatchResult != ERTSMatchResult::Playing)
	{
		return;
	}
	IssueGroupIfChanged(Units);
	Sim::Command C;
	C.Player = LocalPlayer();
	C.Type = Sim::CommandType::Attack;
	C.Target = TargetId;
	Issue(C);
}

void URTSMatchSubsystem::OrderStop(const TArray<uint32>& Units)
{
	if (!(State != nullptr) || !bReady || Units.IsEmpty() || MatchResult != ERTSMatchResult::Playing)
	{
		return;
	}
	IssueGroupIfChanged(Units);
	Sim::Command C;
	C.Player = LocalPlayer();
	C.Type = Sim::CommandType::Stop;
	Issue(C);
}

void URTSMatchSubsystem::SaveReplay()
{
	if (!(State != nullptr) || bReplaySaved)
	{
		return;
	}
	bReplaySaved = true;
	const std::vector<uint8_t> Bytes = State->Replay.Finish();
	const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Replays"));
	const FString File = FPaths::Combine(Dir, FString::Printf(TEXT("skirmish_%s.rtsreplay"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))));
	TArray<uint8> Data(Bytes.data(), int32(Bytes.size()));
	if (FFileHelper::SaveArrayToFile(Data, *File))
	{
		UE_LOG(LogTemp, Log, TEXT("RTS replay gravado: %s (%d bytes)"), *File, Data.Num());
	}
}

void URTSMatchSubsystem::Deinitialize()
{
	RTSFogPass::Disable();
	bFogPassOn = false;
	Fog = nullptr;
	SaveReplay();
	delete State;
	State = nullptr;
	Views.Reset();
	Wrecks.Reset();
	DockViews.Reset();
	PendingBlasts.Reset();
	DyingViews.Reset();
	DyingInfo.Reset();
	Super::Deinitialize();
}
