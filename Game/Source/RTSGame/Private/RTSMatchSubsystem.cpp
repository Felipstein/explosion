#include "RTSMatchSubsystem.h"

#include "RTSTreadDust.h"
#include "Scalability.h"
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
#include "ImageUtils.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTSConvert.h"
#include "RTSDockActor.h"
#include "RTSCameraPawn.h"
#include "RTSEffect.h"
#include "RTSFogOfWar.h"
#include "RTSFogPass.h"
#include "RTSGroundMemory.h"
#include "RTSDebris.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
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
	/** As carcaças da simulação (13/10, com física) no passo anterior e no atual, pro desenho interpolar. */
	TMap<uint32, Sim::World::WreckView> PrevWrecks;
	TMap<uint32, Sim::World::WreckView> CurrWrecks;
	FVector2D LocalStartCm = FVector2D::ZeroVector;
};

URTSMatchSubsystem::URTSMatchSubsystem() = default;
URTSMatchSubsystem::~URTSMatchSubsystem()
{
	delete State;
	State = nullptr;
}

bool URTSMatchSubsystem::IsNetPaused() const
{
	const URTSNetSession* N = bNet ? Net() : nullptr;
	return N && N->IsNetPaused();
}

void URTSMatchSubsystem::OrderResign()
{
	if (!(State != nullptr) || !bReady || MatchResult != ERTSMatchResult::Playing || IsLocalDefeated())
	{
		return;
	}
	Sim::Command C;
	C.Player = LocalPlayer();
	C.Type = Sim::CommandType::Resign;
	Issue(C);
	UE_LOG(LogTemp, Log, TEXT("RTS: o jogador %d se rendeu"), int32(LocalPlayer()));
}

void URTSMatchSubsystem::SetPaused(bool bInPaused)
{
	if (bNet)
	{
		bInPaused = false;   // na rede o menu não para a partida: a pausa é o botão PAUSE GAME (para todos)
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
	// o teste rápido: a partida já pronta (as largadas e as unidades do cenário, sem base, sem IA)
	FString ScenarioName;
	bScenario = !bNet && FParse::Value(FCommandLine::Get(), TEXT("-RTSScenario="), ScenarioName);
	NextScenarioOrder = 0;
	FRTSMapInfo ScenarioInfo;
	if (bScenario)
	{
		if (!FRTSGameData::LoadScenario(ScenarioName, Scenario, OutError))
		{
			return false;
		}
		ScenarioInfo = *Info;
		ScenarioInfo.StartsM = Scenario.StartsM;
		ScenarioInfo.StartingUnits = Scenario.UnitsPerPlayer;
		ScenarioInfo.BaseStructure.Reset();
		ScenarioInfo.SkirmishGapM = 0.0f;
		Info = &ScenarioInfo;
		UE_LOG(LogTemp, Log, TEXT("RTS cenário %s: semente %d, névoa %s, %d ordens, fim em %.0f s"), *Scenario.Name, Scenario.Seed, *Scenario.Fog, Scenario.Orders.Num(), Scenario.EndS);
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
	SpawnVillage(*Info);
	{
		// o teste rápido: a grade do mapa como imagem (areia, penhasco, água; uma linha a cada 100 m e o
		// meio do mapa marcado), pra escolher as largadas dos cenários olhando, e não no chute.
		// O norte (+Y da Unreal) fica pra baixo da imagem, o leste (+X) pra direita
		FString DumpFolder;
		if (bScenario && FParse::Value(FCommandLine::Get(), TEXT("-RTSCapture="), DumpFolder))
		{
			const Sim::SimMap& G = S->Baked.Logic;
			TArray<FColor> Px;
			Px.SetNumUninitialized(G.Width * G.Height);
			for (int32 Y = 0; Y < G.Height; ++Y)
			{
				for (int32 X = 0; X < G.Width; ++X)
				{
					const uint8 Pass = G.Pass[G.Index(X, Y)];
					FColor C = Pass == uint8(Sim::CellPass::Water) ? FColor(40, 110, 200) : (Pass == uint8(Sim::CellPass::Open) ? FColor(214, 186, 140) : FColor(70, 55, 45));
					if (X % 25 == 3 || Y % 25 == 3)
					{
						C = FColor(C.R * 3 / 4, C.G * 3 / 4, C.B * 3 / 4);   // linha a cada 100 m (a célula 3 é o x = -500 m)
					}
					if (X == G.Width / 2 || Y == G.Height / 2)
					{
						C = FColor(200, 30, 30);
					}
					Px[Y * G.Width + X] = C;
				}
			}
			TArray64<uint8> Png;
			FImageUtils::PNGCompressImageArray(G.Width, G.Height, Px, Png);
			const FString File = FPaths::Combine(DumpFolder, TEXT("grade_do_mapa.png"));
			FFileHelper::SaveArrayToFile(Png, *File);
			UE_LOG(LogTemp, Log, TEXT("RTS cenário: a grade do mapa em %s (4 m por pixel, o canto de cima à esquerda é (-512, -512) m)"), *File);
			// o motivo de cada célula fechada e o caminho do tanque do meio da vila até cada base (13/10, pro WorldForge
			// acertar a estrada que o tanque não usava): 4 px por célula; areia livre, água azul, penhasco marrom, pedra ou
			// palmeira cinza, vila roxo; o caminho em vermelho
			if (S->Baked.CellReason.Num() == G.Width * G.Height)
			{
				constexpr int32 K = 4;
				const int32 PW = G.Width * K;
				TArray<FColor> Rs;
				Rs.SetNumUninitialized(PW * G.Height * K);
				static const FColor Colors[5] = { FColor(214, 186, 140), FColor(40, 110, 200), FColor(95, 60, 35), FColor(120, 120, 120), FColor(150, 70, 170) };
				for (int32 Y = 0; Y < G.Height * K; ++Y)
				{
					for (int32 X = 0; X < PW; ++X)
					{
						const int32 I = G.Index(X / K, Y / K);
						FColor C = Colors[FMath::Min<int32>(S->Baked.CellReason[I], 4)];
						if ((X / K) % 25 == 3 || (Y / K) % 25 == 3)
						{
							C = FColor(C.R * 3 / 4, C.G * 3 / 4, C.B * 3 / 4);
						}
						Rs[Y * PW + X] = C;
					}
				}
				int32 TankDef = -1;
				for (int32 D = 0; D < int32(S->Data.Rules.Units.size()) && TankDef < 0; ++D)
				{
					TankDef = !S->Data.Rules.Units[D].IsStructure() && S->Data.Rules.Units[D].HasWeapon() ? D : -1;
				}
				if (TankDef >= 0)
				{
					Sim::Pathfinder Finder;
					const int32 Clear = G.ClearanceFor(S->Data.Rules.Units[TankDef].PathRadius);
					// as bases do mapa (o cenário troca as largadas pelas dele)
					const FRTSMapInfo* MapBases = S->Data.FindMap(Info->Name);
					const TArray<FVector2D>& Bases = MapBases ? MapBases->StartsM : Info->StartsM;
					for (int32 B = 0; B < Bases.Num(); ++B)
					{
						std::vector<Sim::Vec2> Path;
						const Sim::Vec2 From = RTS::ToSim(FVector2D::ZeroVector);
						if (!Finder.FindPath(G, From, RTS::ToSim(Bases[B] * 100.0), Clear, Path))
						{
							UE_LOG(LogTemp, Warning, TEXT("RTS cenário: sem caminho do meio da vila até a base %d"), B + 1);
							continue;
						}
						double Length = 0.0;
						Sim::Vec2 Prev = From;
						for (const Sim::Vec2& P : Path)
						{
							// a linha entre os pontos, em passos de 1 px
							const FVector2D A = (RTS::ToWorld(Prev) / 100.0 + 512.0) * (K / 4.0);
							const FVector2D Bp = (RTS::ToWorld(P) / 100.0 + 512.0) * (K / 4.0);
							const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(A, Bp)));
							for (int32 T = 0; T <= Steps; ++T)
							{
								const FVector2D Q = FMath::Lerp(A, Bp, double(T) / Steps);
								const int32 QX = FMath::Clamp(FMath::RoundToInt(Q.X), 0, PW - 1);
								const int32 QY = FMath::Clamp(FMath::RoundToInt(Q.Y), 0, G.Height * K - 1);
								Rs[QY * PW + QX] = FColor(230, 20, 20);
							}
							Length += FVector2D::Distance(RTS::ToWorld(Prev), RTS::ToWorld(P)) / 100.0;
							Prev = P;
						}
						UE_LOG(LogTemp, Log, TEXT("RTS cenário: do meio da vila até a base %d (%.0f, %.0f) m, %.0f m de caminho (reta: %.0f m)"), B + 1,
							Bases[B].X, Bases[B].Y, Length, Bases[B].Size());
					}
				}
				TArray64<uint8> RPng;
				FImageUtils::PNGCompressImageArray(PW, G.Height * K, Rs, RPng);
				const FString RFile = FPaths::Combine(DumpFolder, TEXT("grade_motivos.png"));
				FFileHelper::SaveArrayToFile(RPng, *RFile);
				UE_LOG(LogTemp, Log, TEXT("RTS cenário: os motivos da grade em %s (1 px por metro; o canto de cima à esquerda é (-512, -512) m)"), *RFile);
			}
		}
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
		ERTSFogMode FogMode = bNet ? NetSession->GetFogMode() : (Session ? Session->FogMode : ERTSFogMode::Explored);
		if (bScenario)
		{
			FogMode = Scenario.Fog == TEXT("black") ? ERTSFogMode::Black : (Scenario.Fog == TEXT("explored") ? ERTSFogMode::Explored : ERTSFogMode::Off);
			Setup.Seed = uint64(Scenario.Seed);   // a mesma história a cada rodada
		}
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
		// os depósitos: os do WorldForge (o "supply_docks" do village.json, 13/10: o mapa e o jogo vendo os mesmos), ou
		// os fixos do Maps.json quando o mapa ainda não exporta; cada um conferido na grade, e o que cair em penhasco,
		// água ou pedra vai pro lugar livre mais perto (aviso no log)
		struct FDockSpec
		{
			FVector2D PosM;
			FVector2D SizeM;
			int32 Boxes = 0;
		};
		TArray<FDockSpec> DockSpecs;
		if (!Info->VillageFile.IsEmpty())
		{
			FString Text;
			TSharedPtr<FJsonObject> Village;
			const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
			if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("RTS"), Info->VillageFile))
				&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Village) && Village.IsValid()
				&& Village->TryGetArrayField(TEXT("supply_docks"), List))
			{
				for (const TSharedPtr<FJsonValue>& V : *List)
				{
					const TSharedPtr<FJsonObject> O = V->AsObject();
					FDockSpec D;
					D.PosM = FVector2D(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")));
					D.SizeM = Info->DockSizeM;
					D.Boxes = Info->DockBoxes;
					const TArray<TSharedPtr<FJsonValue>>* Size = nullptr;
					if (O->TryGetArrayField(TEXT("size_m"), Size) && Size->Num() >= 2)
					{
						D.SizeM = FVector2D((*Size)[0]->AsNumber(), (*Size)[1]->AsNumber());
					}
					O->TryGetNumberField(TEXT("boxes"), D.Boxes);
					// o depósito da simulação é uma caixa alinhada ao mundo: giro de 90° troca os lados; outro giro não cabe
					double Yaw = 0.0;
					O->TryGetNumberField(TEXT("yaw_deg"), Yaw);
					const int32 Quarter = FMath::RoundToInt(Yaw / 90.0);
					if (!FMath::IsNearlyEqual(Yaw, Quarter * 90.0, 1.0))
					{
						UE_LOG(LogTemp, Warning, TEXT("RTS depósito em (%.0f, %.0f): o giro %.1f° não é múltiplo de 90; fica alinhado ao mundo"), D.PosM.X, D.PosM.Y, Yaw);
					}
					else if (Quarter & 1)
					{
						D.SizeM = FVector2D(D.SizeM.Y, D.SizeM.X);
					}
					DockSpecs.Add(D);
				}
				UE_LOG(LogTemp, Log, TEXT("RTS depósitos: %d do WorldForge (%s)"), DockSpecs.Num(), *Info->VillageFile);
			}
		}
		if (DockSpecs.IsEmpty())
		{
			for (const FVector2D& P : Info->DocksM)
			{
				DockSpecs.Add({ P, Info->DockSizeM, Info->DockBoxes });
			}
		}
		for (const FDockSpec& Spec : DockSpecs)
		{
			const FVector2D DockM = Spec.PosM;
			Sim::SupplyDockSetup Dock;
			Dock.HalfX = Sim::Fix::FromMilli(RTS::Milli(Spec.SizeM.X * 0.5));
			Dock.HalfY = Sim::Fix::FromMilli(RTS::Milli(Spec.SizeM.Y * 0.5));
			Dock.Boxes = Spec.Boxes;
			const auto Fits = [&](const FVector2D& C)
			{
				const Sim::Cell Lo = Setup.Map.CellOf(RTS::ToSim((C - Spec.SizeM * 0.5) * 100.0));
				const Sim::Cell Hi = Setup.Map.CellOf(RTS::ToSim((C + Spec.SizeM * 0.5) * 100.0));
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
				SlotAI.Add(bNet ? NetSession->IsAISlot(uint8(P)) : (P != 0 && !bScenario));
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
		if (GroundMemory)
		{
			GroundMemory->Shutdown();
			GroundMemory = nullptr;
		}
		if (!FParse::Param(FCommandLine::Get(), TEXT("RTSNoGround")))
		{
			float ZMin = 0.0f, ZMax = 0.0f;
			bool bAny = false;
			for (const float Z : S->Baked.HeightsM)
			{
				if (Z > -1000.0f)   // as amostras fora do Landscape vêm como "sem chão" (-100 km)
				{
					ZMin = bAny ? FMath::Min(ZMin, Z * 100.0f) : Z * 100.0f;
					ZMax = bAny ? FMath::Max(ZMax, Z * 100.0f) : Z * 100.0f;
					bAny = true;
				}
			}
			GroundMemory = NewObject<URTSGroundMemory>(this);
			if (!GroundMemory->Init(GetWorld(), MinCm, MaxCm - MinCm, ZMin, ZMax, Info->WindYawDeg))
			{
				GroundMemory = nullptr;
			}
		}
		// o vento único do mapa: quem desenha (fumaça, poeira, bandeira, palmeira) lê o MPC_Wind
		if (UMaterialParameterCollection* Wind = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Materials/MPC_Wind.MPC_Wind"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			const float R = FMath::DegreesToRadians(Info->WindYawDeg);
			UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), Wind, TEXT("Wind"), FLinearColor(FMath::Cos(R), FMath::Sin(R), Info->WindSpeedMs, 0.3f));
		}
		if (ARTSDebrisField* Old = Debris.Get())
		{
			Old->Destroy();
		}
		Debris = nullptr;
		if (ARTSTreadDust* OldDust = TreadDust.Get())
		{
			OldDust->Destroy();
		}
		TreadDust = nullptr;
		DustMotion.Reset();
		if (GetWorld() && !FParse::Param(FCommandLine::Get(), TEXT("RTSNoDust")))
		{
			// a poeira das lagartas (13/10): o preset do jogo (0 baixa … 3 ultra) pelo nível da Unreal (1 … 4)
			FActorSpawnParameters DP;
			DP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			TreadDust = GetWorld()->SpawnActor<ARTSTreadDust>(ARTSTreadDust::StaticClass(), FTransform::Identity, DP);
			const Scalability::FQualityLevels Q = Scalability::GetQualityLevels();
			DustQuality = FMath::Clamp(Q.EffectsQuality - 1, 0, 3);
			if (ARTSTreadDust* D = TreadDust.Get())
			{
				D->SetQuality(DustQuality);
			}
			UE_LOG(LogTemp, Log, TEXT("RTS poeira das lagartas: ligada, nível %d"), DustQuality);
		}
		if (GroundMemory && GetWorld() && !FParse::Param(FCommandLine::Get(), TEXT("RTSNoDebris")))   // (-RTSNoDebris: o A/B das pedras e destroços)
		{
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Debris = GetWorld()->SpawnActor<ARTSDebrisField>(ARTSDebrisField::StaticClass(), FTransform::Identity, P);
			// as pedras de cada cratera nova (13/10, a prova aprovada do Look): os torrões do atlas viram pedra escaneada
			TWeakObjectPtr<ARTSDebrisField> WeakDebris = Debris;
			GroundMemory->OnCraterStones = [this, WeakDebris](const TArray<FVector>& Large, const TArray<FVector>& Small, uint32 Seed)
			{
				if (ARTSDebrisField* D = WeakDebris.Get())
				{
					D->AddStones(Large, Small, Seed, [this](const FVector2D& Pt) { return State ? State->Baked.GroundZCm(Pt) : 0.0f; });
				}
			};
		}
		LastHitFrom.Reset();
		HiddenMarks.Reset();
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
		UpdateNetTests(N);
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
			TArray<FLinearColor> Colors;
			for (int32 P = 0; P < PlayerCount() && P < 8; ++P)
			{
				Colors.Add(PlayerColor(uint8(P)));
			}
			Cam->SetSilhouetteColors(Colors);
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
	// a pausa em rede (13/10): o efeito para junto, e o log diz em que passo cada PC parou (tem que ser o mesmo)
	const bool bNetPaused = N->IsNetPaused();
	if (bNetPaused != bNetPausedShown)
	{
		bNetPausedShown = bNetPaused;
		bNetFrozenLogged = false;
		if (UBlastFxDirector* Director = UBlastFxDirector::Get(this))
		{
			Director->SetPaused(bNetPaused);
		}
		if (!bNetPaused)
		{
			UE_LOG(LogTemp, Log, TEXT("RTS LAN: a partida voltou no passo %d"), int32(State->World.Tick()));
		}
	}
	if (bNetPaused && N->IsHost())
	{
		Accumulator = 0.0;
		int32 Ran = 0;
		while (int64(State->World.Tick()) < int64(N->NextSealTick()) && Ran < MaxStepsPerFrame)
		{
			StepNet();
			++Ran;
		}
		if (!bNetFrozenLogged && int64(State->World.Tick()) == int64(N->NextSealTick()))
		{
			bNetFrozenLogged = true;
			UE_LOG(LogTemp, Log, TEXT("RTS LAN: parado no passo %d (hash %016llx)"), int32(State->World.Tick()), (unsigned long long)State->World.StateHash());
		}
		return;
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
		if (bNetPaused && !bNetFrozenLogged && N->Ahead(State->World.Tick()) == 0)
		{
			bNetFrozenLogged = true;
			UE_LOG(LogTemp, Log, TEXT("RTS LAN: parado no passo %d (hash %016llx)"), int32(State->World.Tick()), (unsigned long long)State->World.StateHash());
		}
	}
}

void URTSMatchSubsystem::UpdateNetTests(URTSNetSession* N)
{
	if (!N || !State || !bReady)
	{
		return;
	}
	const int64 Tick = int64(State->World.Tick());
	int32 At = -1;
	if (bNetBot && FParse::Value(FCommandLine::Get(), TEXT("RTSNetBotQuitAt="), At) && Tick >= At)
	{
		UE_LOG(LogTemp, Log, TEXT("RTS LAN robô: sumindo sem avisar no passo %lld (o teste da queda)"), Tick);
		FPlatformMisc::RequestExit(true);   // sem o Bye: o anfitrião só percebe pelo silêncio
		return;
	}
	if (bNetBot && !bNetTestResigned && FParse::Value(FCommandLine::Get(), TEXT("RTSNetBotResignAt="), At) && Tick >= At)
	{
		bNetTestResigned = true;
		OrderResign();
	}
	if (!bNetTestPauseDone && FParse::Value(FCommandLine::Get(), TEXT("RTSNetTestPause="), At) && Tick >= At)
	{
		if (NetTestPausedAt < 0.0)
		{
			NetTestPausedAt = FPlatformTime::Seconds();
			N->RequestPause();
		}
		else if (FPlatformTime::Seconds() - NetTestPausedAt > 4.0)
		{
			bNetTestPauseDone = true;
			N->RequestResume();
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("RTSNetTestVote")) && N->GetPause().WaitingFor != 255 && N->GetPause().WaitLeft < 56.0f
		&& !(N->GetPause().Votes & (1u << N->LocalSlot())))
	{
		N->VoteDrop(N->GetPause().WaitingFor);
	}
}

void URTSMatchSubsystem::StepNet()
{
	FRTSMatchState& S = *State;
	URTSNetSession* N = Net();
	const double T0 = FPlatformTime::Seconds();
	const Sim::TickNumber Now = S.World.Tick();
	IssueScenarioOrders(Now);
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
	S.PrevWrecks = MoveTemp(S.CurrWrecks);
	S.CurrWrecks.Reset();
	for (int32 I = 0; I < S.World.WreckCount(); ++I)
	{
		const Sim::World::WreckView W = S.World.Wreck(I);
		S.CurrWrecks.Add(W.Id, W);
	}
	LastStepMs = (FPlatformTime::Seconds() - T0) * 1000.0;
	HandleEvents();
}

void URTSMatchSubsystem::SpawnVillage(const FRTSMapInfo& Info)
{
	// a vila do WorldForge (12/10): o mapa só tem o chão aplainado; quem desenha é o jogo, pelo JSON (o
	// mesmo que fecha as células na grade). Cada peça usa a malha do kit civil do Look (/Game/Village,
	// game.ps1 village); enquanto ela não existir, uma caixa provisória do tamanho e da cor da peça,
	// como no app do WorldForge (size_m já girado, height_m, a base no floor_cm)
	UWorld* World = GetWorld();
	if (!World || Info.VillageFile.IsEmpty() || VillageActor.IsValid())
	{
		return;
	}
	FString Text;
	TSharedPtr<FJsonObject> Village;
	const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("RTS"), Info.VillageFile);
	const TArray<TSharedPtr<FJsonValue>>* Pieces = nullptr;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Village) || !Village.IsValid()
		|| !Village->TryGetArrayField(TEXT("pieces"), Pieces))
	{
		return;   // o assador já avisou
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, P);
	if (!Actor)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Village"));
	Actor->SetRootComponent(Root);
	Root->RegisterComponent();
	VillageActor = Actor;

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* BoxBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Structures/Materials/MI_Body.MI_Body"));
	// as cores das caixas do app do WorldForge (as dele, em sRGB)
	const auto BoxColor = [](const FString& Type)
	{
		if (Type.Contains(TEXT("2story"))) { return FLinearColor::FromSRGBColor(FColor(204, 168, 128)); }
		if (Type.Contains(TEXT("house"))) { return FLinearColor::FromSRGBColor(FColor(189, 153, 115)); }
		if (Type.Contains(TEXT("gate"))) { return FLinearColor::FromSRGBColor(FColor(140, 107, 77)); }
		if (Type.Contains(TEXT("wall"))) { return FLinearColor::FromSRGBColor(FColor(163, 130, 94)); }
		if (Type.Contains(TEXT("car")) || Type.Contains(TEXT("pickup"))) { return FLinearColor::FromSRGBColor(FColor(41, 38, 36)); }
		return FLinearColor::FromSRGBColor(FColor(120, 104, 84));
	};
	TMap<FString, UInstancedStaticMeshComponent*> Layers;
	const auto Layer = [&](const FString& Key, UStaticMesh* Mesh, UMaterialInterface* Mat) -> UInstancedStaticMeshComponent*
	{
		if (UInstancedStaticMeshComponent** Found = Layers.Find(Key))
		{
			return *Found;
		}
		UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(Actor, *FString::Printf(TEXT("Village_%s"), *Key));
		C->SetupAttachment(Root);
		C->SetStaticMesh(Mesh);
		if (Mat)
		{
			for (int32 K = 0; K < Mesh->GetStaticMaterials().Num(); ++K)
			{
				C->SetMaterial(K, Mat);
			}
		}
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		C->SetMobility(EComponentMobility::Static);
		C->bReceivesDecals = false;   // o decal do chão pintaria as paredes esticado
		C->SetNumCustomDataFloats(1);   // o tom do lote (o M_Structure lê; 0 = o aleatório da instância)
		C->RegisterComponent();
		Layers.Add(Key, C);
		return C;
	};
	int32 Meshes = 0;
	int32 Boxes = 0;
	TSet<FString> MissingMesh;
	for (const TSharedPtr<FJsonValue>& V : *Pieces)
	{
		const TSharedPtr<FJsonObject> Piece = V->AsObject();
		if (!Piece.IsValid())
		{
			continue;
		}
		const FString Type = Piece->GetStringField(TEXT("type"));
		FString MeshName;
		Piece->TryGetStringField(TEXT("mesh"), MeshName);
		const double X = Piece->GetNumberField(TEXT("x")) * 100.0;
		const double Y = Piece->GetNumberField(TEXT("y")) * 100.0;
		double FloorCm = 0.0;
		Piece->TryGetNumberField(TEXT("floor_cm"), FloorCm);
		double ZM = 0.0;
		if (Piece->TryGetNumberField(TEXT("z"), ZM))
		{
			FloorCm = ZM * 100.0;   // (village.json v3: a peça na laje)
		}
		int32 Rot = 0;
		Piece->TryGetNumberField(TEXT("rot"), Rot);
		// (v3, 13/10) o giro livre de carcaça, picape, miudeza e poste; o rot segue sendo o passo de 90° mais perto
		double YawDeg = 90.0 * Rot;
		Piece->TryGetNumberField(TEXT("yaw_deg"), YawDeg);
		// o import da Unreal põe cada GLB numa pasta própria (/Game/Village/<nome>/StaticMeshes/<nome>, como o tanque)
		UStaticMesh* Mesh = MeshName.IsEmpty() ? nullptr
			: LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Village/%s/StaticMeshes/%s.%s"), *MeshName, *MeshName, *MeshName), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Mesh)
		{
			UInstancedStaticMeshComponent* L = Layer(MeshName, Mesh, nullptr);
			const int32 Index = L->AddInstance(FTransform(FRotator(0.0, YawDeg, 0.0), FVector(X, Y, FloorCm)), true);
			// o mesmo tom pra casa e muros do mesmo terreno (13/10, o Felipe pelo Look): o "lot" do village.json, um número
			// por lote; sem ele, o material cai no aleatório de cada instância
			int32 Lot = -1;
			if (Piece->TryGetNumberField(TEXT("lot"), Lot) && Lot >= 0)
			{
				const uint32 H = HashCombine(GetTypeHash(Lot), 0x9E3779B9u);
				L->SetCustomDataValue(Index, 0, 0.001f + 0.998f * float(H & 0xFFFFFFu) / float(0xFFFFFF), true);
			}
			++Meshes;
			continue;
		}
		MissingMesh.Add(MeshName);
		const TArray<TSharedPtr<FJsonValue>>* Size = nullptr;
		double HeightM = 2.0;
		Piece->TryGetNumberField(TEXT("height_m"), HeightM);
		if (!Cube || !Piece->TryGetArrayField(TEXT("size_m"), Size) || Size->Num() < 2)
		{
			continue;
		}
		UInstancedStaticMeshComponent* C = Layers.FindRef(TEXT("box_") + Type);
		if (!C)
		{
			UMaterialInstanceDynamic* M = BoxBase ? UMaterialInstanceDynamic::Create(BoxBase, Actor) : nullptr;
			if (M)
			{
				M->SetVectorParameterValue(TEXT("BaseColor"), BoxColor(Type));
			}
			C = Layer(TEXT("box_") + Type, Cube, M);
		}
		// o cubo da Unreal tem 1 m de lado, centrado: a escala é o tamanho, e a base vai no piso
		const FVector Scale((*Size)[0]->AsNumber(), (*Size)[1]->AsNumber(), HeightM);
		// (o size_m já vem girado pelo rot; o giro livre do v3 é o resto)
		C->AddInstance(FTransform(FRotator(0.0, YawDeg - 90.0 * Rot, 0.0), FVector(X, Y, FloorCm + HeightM * 50.0), Scale), true);
		++Boxes;
	}
	// os fios (village.json v3, 13/10): parados por enquanto, na parábola do WorldForge, P(t) = lerp(A, B, t) − 4·sag·t(1−t)·Z,
	// com 4,5 cm, escuros e sem sombra (a 80 m o fio de 2 cm pisca). A física e o poste que cai vêm depois (BASE_JOGAVEL).
	int32 WireCount = 0;
	const TArray<TSharedPtr<FJsonValue>>* PoleList = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* WireList = nullptr;
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder && Village->TryGetArrayField(TEXT("poles"), PoleList) && Village->TryGetArrayField(TEXT("wires"), WireList))
	{
		// as tomadas de cada poste no mundo (a cruzeta no X local, o fio no Y local)
		TArray<TArray<FVector>> Sockets;
		for (const TSharedPtr<FJsonValue>& PV : *PoleList)
		{
			TArray<FVector>& Out = Sockets.AddDefaulted_GetRef();
			const TSharedPtr<FJsonObject> Pole = PV->AsObject();
			const TArray<TSharedPtr<FJsonValue>>* Local = nullptr;
			if (!Pole.IsValid() || !Pole->TryGetArrayField(TEXT("sockets_local"), Local))
			{
				continue;
			}
			const FVector Base(Pole->GetNumberField(TEXT("x")) * 100.0, Pole->GetNumberField(TEXT("y")) * 100.0, Pole->GetNumberField(TEXT("z")) * 100.0);
			const FRotator Yaw(0.0, Pole->GetNumberField(TEXT("yaw_deg")), 0.0);
			for (const TSharedPtr<FJsonValue>& SV : *Local)
			{
				const TArray<TSharedPtr<FJsonValue>>& L = SV->AsArray();
				if (L.Num() >= 3)
				{
					Out.Add(Base + Yaw.RotateVector(FVector(L[0]->AsNumber(), L[1]->AsNumber(), L[2]->AsNumber()) * 100.0));
				}
			}
		}
		const auto End = [&Sockets](const TSharedPtr<FJsonObject>& W, const TCHAR* Key, FVector& Out)
		{
			const TArray<TSharedPtr<FJsonValue>>* Ref = nullptr;
			if (!W->TryGetArrayField(Key, Ref) || Ref->Num() < 2)
			{
				return false;
			}
			const int32 Pole = int32((*Ref)[0]->AsNumber());
			const int32 Socket = int32((*Ref)[1]->AsNumber());
			if (!Sockets.IsValidIndex(Pole) || !Sockets[Pole].IsValidIndex(Socket))
			{
				return false;
			}
			Out = Sockets[Pole][Socket];
			return true;
		};
		UMaterialInstanceDynamic* WireMat = BoxBase ? UMaterialInstanceDynamic::Create(BoxBase, Actor) : nullptr;
		if (WireMat)
		{
			WireMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.02f, 0.02f, 0.02f));
		}
		UInstancedStaticMeshComponent* WireISM = Layer(TEXT("wires"), Cylinder, WireMat);
		WireISM->SetCastShadow(false);
		for (const TSharedPtr<FJsonValue>& WV : *WireList)
		{
			const TSharedPtr<FJsonObject> W = WV->AsObject();
			FVector A, B;
			if (!W.IsValid() || !End(W, TEXT("a"), A))
			{
				continue;
			}
			const TArray<TSharedPtr<FJsonValue>>* Wall = nullptr;
			if (!End(W, TEXT("b"), B))
			{
				if (!W->TryGetArrayField(TEXT("b_wall"), Wall) || Wall->Num() < 3)
				{
					continue;
				}
				B = FVector((*Wall)[0]->AsNumber(), (*Wall)[1]->AsNumber(), (*Wall)[2]->AsNumber()) * 100.0;
			}
			const double Sag = W->GetNumberField(TEXT("sag_m")) * 100.0;
			const int32 N = FMath::Clamp(FMath::CeilToInt(FVector::Dist(A, B) / 150.0), 6, 24);
			FVector Prev = A;
			for (int32 K = 1; K <= N; ++K)
			{
				const double T = double(K) / N;
				const FVector Cur = FMath::Lerp(A, B, T) - FVector(0.0, 0.0, 4.0 * Sag * T * (1.0 - T));
				const FVector D = Cur - Prev;
				// o cilindro da Unreal tem 1 m de altura no Z e 1 m de diâmetro
				WireISM->AddInstance(FTransform(FRotationMatrix::MakeFromZ(D.GetSafeNormal()).Rotator(), (Prev + Cur) * 0.5, FVector(0.045, 0.045, D.Size() / 100.0)), true);
				Prev = Cur;
			}
			++WireCount;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("RTS vila: desenhada com %d peças do kit, %d caixas provisórias e %d fios (%d camadas)%s"), Meshes, Boxes, WireCount, Layers.Num(),
		MissingMesh.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("; sem malha ainda: %s (game.ps1 village, quando o kit do Look chegar)"), *FString::Join(MissingMesh.Array(), TEXT(" "))));
}

void URTSMatchSubsystem::SetDustQuality(int32 Level)
{
	DustQuality = FMath::Clamp(Level, 0, 3);
	if (ARTSTreadDust* D = TreadDust.Get())
	{
		D->SetQuality(DustQuality);
	}
}

void URTSMatchSubsystem::UpdateTreadDust(float DeltaTime)
{
	ARTSTreadDust* D = TreadDust.Get();
	if (!D || !State || DeltaTime <= 0.0f)
	{
		return;
	}
	// os veículos de lagarta perto da câmera (os parados também: fazem sombra na poeira), na posição desenhada; a
	// velocidade com sinal e o giro saem do desenho de um quadro pro outro (suavizados); a areia pelo tipo de chão do
	// WorldForge nos 4 pontos das lagartas (1 − o asfalto ou o molhado, o INTEGRACAO.md da arte)
	FVector2D Focus = FVector2D::ZeroVector;
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (const ARTSCameraPawn* Cam = Cast<ARTSCameraPawn>(PC->GetPawn()))
		{
			const FVector F = Cam->GetLookFocusPoint();
			Focus = FVector2D(F.X, F.Y);
		}
	}
	TArray<FRTSTreadSource> Sources;
	TSet<uint32> Seen;
	for (const TPair<uint32, TObjectPtr<ARTSUnitActor>>& It : Views)
	{
		FRTSUnitState U;
		if (!It.Value || !GetUnit(It.Key, U) || U.bStructure || It.Value->GetTracksCm().X <= 0.0 || FVector2D::Distance(U.PosCm, Focus) > 20000.0)
		{
			continue;
		}
		Seen.Add(It.Key);
		FDustMotion& Mo = DustMotion.FindOrAdd(It.Key);
		const FVector2D Fwd(FMath::Cos(FMath::DegreesToRadians(U.HullYawDeg)), FMath::Sin(FMath::DegreesToRadians(U.HullYawDeg)));
		if (Mo.bInit)
		{
			const double Speed = FVector2D::DotProduct(U.PosCm - Mo.Cm, Fwd) / 100.0 / DeltaTime;
			const double YawRate = FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(Mo.YawDeg, U.HullYawDeg)) / DeltaTime;
			const double K = 1.0 - FMath::Exp(-12.0 * DeltaTime);
			Mo.Speed += (Speed - Mo.Speed) * K;
			Mo.YawRate += (YawRate - Mo.YawRate) * K;
		}
		Mo.Cm = U.PosCm;
		Mo.YawDeg = U.HullYawDeg;
		Mo.bInit = true;
		FRTSTreadSource S;
		S.Id = int32(It.Key);
		S.PosM = FVector(U.PosCm.X, U.PosCm.Y, State->Baked.GroundZCm(U.PosCm)) / 100.0;
		S.FwdM = FVector(Fwd.X, Fwd.Y, 0.0);
		S.Speed = Mo.Speed;
		S.YawRate = Mo.YawRate;
		const FVector2D R(Fwd.Y, -Fwd.X);
		const auto Sand = [this](const FVector2D& P)
		{
			const FVector4f W = State->Baked.SurfaceAt(P);
			return 1.0 - double(FMath::Max(W.X, W.W));
		};
		S.Sand[0] = Sand(U.PosCm + (-240.0 * Fwd - 142.5 * R));
		S.Sand[1] = Sand(U.PosCm + (-240.0 * Fwd + 142.5 * R));
		S.SandFront[0] = Sand(U.PosCm + (255.0 * Fwd - 142.5 * R));
		S.SandFront[1] = Sand(U.PosCm + (255.0 * Fwd + 142.5 * R));
		S.bVisible = !It.Value->IsFogHidden() && IsPointShown(U.PosCm);
		Sources.Add(S);
	}
	for (auto It = DustMotion.CreateIterator(); It; ++It)
	{
		if (!Seen.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
	D->SetSources(Sources);
}

void URTSMatchSubsystem::SetGroundDisplacement(bool bOn)
{
	if (GroundMemory)
	{
		GroundMemory->SetDisplacement(bOn);
	}
}

void URTSMatchSubsystem::DumpGroundMemory(const FString& Folder)
{
	if (GroundMemory)
	{
		GroundMemory->DebugDump(Folder);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("RTS chão: a memória do chão não está ligada"));
	}
}

uint32 URTSMatchSubsystem::SimTick() const
{
	return State != nullptr ? uint32(State->World.Tick()) : 0u;
}

void URTSMatchSubsystem::IssueScenarioOrders(uint32 Now)
{
	// no passo exato (não no ritmo dos quadros): toda rodada do cenário recebe as mesmas ordens nos mesmos passos
	if (!bScenario || State == nullptr)
	{
		return;
	}
	FRTSMatchState& S = *State;
	while (Scenario.Orders.IsValidIndex(NextScenarioOrder)
		&& Sim::TickNumber(FMath::RoundToInt(Scenario.Orders[NextScenarioOrder].AtS * Sim::TicksPerSecond)) <= Now)
	{
		const FRTSScenarioOrder& O = Scenario.Orders[NextScenarioOrder++];
		if (O.Type == TEXT("wreck"))
		{
			// só desenho (o teste da memória do chão): o avental de morte de tanque virado pra FacingDeg, sem carcaça
			if (GroundMemory)
			{
				const FVector2D At = O.ToM * 100.0;
				GroundMemory->AddWreck(At, O.FacingDeg, At);   // (cenário)
				if (ARTSDebrisField* D = Debris.Get())
				{
					D->AddWreck(At, O.FacingDeg, At, [this](const FVector2D& P) { return State ? State->Baked.GroundZCm(P) : 0.0f; });
				}
			}
			continue;
		}
		if (O.Type == TEXT("scar"))
		{
			// só desenho (o teste da memória do chão): queimado e cratera de morte de tanque, virados pra FacingDeg
			if (GroundMemory)
			{
				const FVector2D At = O.ToM * 100.0;
				const float R = FMath::DegreesToRadians(O.FacingDeg);
				GroundMemory->AddScar(At, At - FVector2D(FMath::Cos(R), FMath::Sin(R)) * 3000.0, O.WidthM > 0.0f ? O.WidthM * 100.0f : 420.0f, 0.9f, 0.7f, O.MarkSize);
			}
			continue;
		}
		Sim::Command G;
		G.Tick = Now;
		G.Player = uint8(O.Player);
		G.Type = Sim::CommandType::SetGroup;
		for (int32 I = 0; I < S.World.UnitCount(); ++I)
		{
			const Sim::UnitView V = S.World.Unit(I);
			if (V.Owner == G.Player && !V.bStructure && V.Hp > 0)
			{
				G.Units.push_back(V.Id);
			}
		}
		Sim::Command C;
		C.Tick = Now;
		C.Player = G.Player;
		C.Type = O.Type == TEXT("attack_move") ? Sim::CommandType::AttackMove : Sim::CommandType::Move;
		if (O.Type == TEXT("force_attack"))
		{
			// o tiro forçado (Ctrl) no próprio tanque mais perto de to_m, pelos outros (13/10: a carcaça pro teste da física)
			const Sim::Vec2 At = RTS::ToSim(O.ToM * 100.0);
			Sim::EntityId Victim = Sim::NoEntity;
			int64 Best = TNumericLimits<int64>::Max();
			for (const Sim::EntityId Id : G.Units)
			{
				Sim::UnitView V;
				if (S.World.FindUnit(Id, V) && Sim::DistSqRaw(V.Pos, At) < Best)
				{
					Best = Sim::DistSqRaw(V.Pos, At);
					Victim = Id;
				}
			}
			G.Units.erase(std::remove(G.Units.begin(), G.Units.end(), Victim), G.Units.end());
			C.Type = Sim::CommandType::ForceAttack;
			C.Target = Victim;
			S.Queue.Insert(G);
			S.Queue.Insert(C);
			UE_LOG(LogTemp, Log, TEXT("RTS cenário: passo %u, jogador %d atira no próprio tanque %u (o tiro forçado)"), Now, O.Player, Victim);
			continue;
		}
		if (O.Type == TEXT("attack_ground"))
		{
			C.Type = Sim::CommandType::AttackGround;   // o tiro forçado no chão (Ctrl), pro teste das crateras
		}
		C.Point = RTS::ToSim(O.ToM * 100.0);
		const bool bLine = O.Type == TEXT("line");
		const int32 FacingBam = FMath::RoundToInt(FMath::Fmod(FMath::Fmod(O.FacingDeg, 360.0f) + 360.0f, 360.0f) / 360.0f * float(Sim::Angle::FullTurn)) & 0xFFFF;
		C.Value = Sim::MoveValue(O.bFormation, bLine, bLine ? FMath::RoundToInt(O.WidthM * 0.5f) : 0, uint16_t(bLine ? FacingBam : 0));
		S.Queue.Insert(G);
		S.Queue.Insert(C);
		UE_LOG(LogTemp, Log, TEXT("RTS cenário: passo %u, jogador %d, %s pra (%.0f, %.0f) m com %d unidades"), Now, O.Player, *O.Type, O.ToM.X, O.ToM.Y, int32(G.Units.size()));
	}
}

void URTSMatchSubsystem::StepOnce()
{
	FRTSMatchState& S = *State;
	const double T0 = FPlatformTime::Seconds();
	const Sim::TickNumber Now = S.World.Tick();
	IssueScenarioOrders(Now);
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
	S.PrevWrecks = MoveTemp(S.CurrWrecks);
	S.CurrWrecks.Reset();
	for (int32 I = 0; I < S.World.WreckCount(); ++I)
	{
		const Sim::World::WreckView W = S.World.Wreck(I);
		S.CurrWrecks.Add(W.Id, W);
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
			FVector2D TargetCm = RTS::ToWorld(E.Point);
			if (E.Target == Sim::NoEntity)
			{
				// o tiro no chão espalha até 2,5 m, só no desenho (pelo hash do atirador e do passo: igual em todo PC; no
				// ponto exato as crateras iguais se empilhavam em anéis, tiro_chao1); o impacto usa o mesmo desvio
				const uint32 Hs = HashCombine(GetTypeHash(E.Source), GetTypeHash(SimTick()) * 2654435761u);
				const double Ang = 2.0 * PI * double(Hs & 0xFFFFu) / 65535.0;
				const double Rad = 250.0 * FMath::Sqrt(double((Hs >> 16) & 0xFFu) / 255.0);
				const FVector2D Off = FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * Rad;
				GroundShotSpread.FindOrAdd(E.Source).Add(Off);
				TargetCm += Off;
			}
			// (o tiro no chão, Ctrl, 13/10: termina no chão; no tanque, na altura do casco)
			const FVector Target(TargetCm.X, TargetCm.Y, S.Baked.GroundZCm(TargetCm) + (E.Target == Sim::NoEntity ? 10.0f : 120.0f));
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
			const Sim::UnitView* Shooter = S.Curr.Find(E.Source);
			const FVector2D From = Shooter ? RTS::ToWorld(Shooter->Pos) : At;
			if (E.Target == Sim::NoEntity)
			{
				// o tiro forçado no chão (Ctrl, 13/10): o clarão no chão e sempre a cratera da arma (a A do tiro de tanque),
				// no ponto espalhado do disparo (o mesmo do traçante)
				FVector2D Hit = At;
				if (TArray<FVector2D>* Spread = GroundShotSpread.Find(E.Source); Spread && !Spread->IsEmpty())
				{
					Hit += (*Spread)[0];
					Spread->RemoveAt(0);
				}
				if (IsPointShown(Hit))
				{
					ARTSEffect::SpawnFlash(World, FVector(Hit.X, Hit.Y, S.Baked.GroundZCm(Hit) + 40.0f), 100.0f, 4.0e6f, 0.16f, FLinearColor(0.4f, 0.2f, 0.05f));
					const FRTSUnitVisual* Vis = Shooter ? State->Data.FindVisual(Shooter->Def) : nullptr;
					if (GroundMemory)
					{
						GroundMemory->AddScar(Hit, From, 420.0f, 0.9f, 0.7f, Vis && Vis->ImpactCrater >= 0 ? Vis->ImpactCrater : 0);
					}
				}
				break;
			}
			LastHitFrom.Add(E.Target, From);
			if (IsPointShown(At))
			{
				// o clarão do impacto, provisório: laranja na exposição do look (com 3,2/1,9/0,7 era uma bola branca)
				ARTSEffect::SpawnFlash(World, FVector(At.X, At.Y, S.Baked.GroundZCm(At) + 130.0f), 100.0f, 4.0e6f, 0.16f, FLinearColor(0.4f, 0.2f, 0.05f));
				if (GroundMemory)
				{
					// o tiro que acerta o tanque chamusca o chão embaixo (fraco): onde houve luta fica marcado
					GroundMemory->AddScar(At, From, 130.0f, 0.35f, 0.0f);
					// e parte dos tiros passa raspando e cava a cratera da arma 6–12 m além do alvo, na linha do tiro
					// (13/10, o Felipe: o tiro de tanque é a A). Só desenho, pelo hash do lugar e do atirador: igual em
					// todo PC; a parte vem do Units.json (near_miss), pra a batalha não virar um campo de buracos
					const FRTSUnitVisual* Vis = Shooter ? State->Data.FindVisual(Shooter->Def) : nullptr;
					if (Vis && Vis->ImpactCrater >= 0 && Vis->NearMissShare > 0.0f && At != From)
					{
						const uint32 Hh = HashCombine(GetTypeHash(FMath::RoundToInt(At.X)), GetTypeHash(FMath::RoundToInt(At.Y)) ^ (uint32(E.Source) * 2654435761u));
						if (float(Hh & 0xFFFFu) / 65535.0f < Vis->NearMissShare)
						{
							const FVector2D Dir = (At - From).GetSafeNormal();
							const FVector2D Side(-Dir.Y, Dir.X);
							// (a 3–6 m caía embaixo do próprio alvo: o casco tem 7,9 m)
							const FVector2D Miss = At + Dir * (600.0 + 600.0 * double((Hh >> 16) & 0xFFu) / 255.0) + Side * (500.0 * double((Hh >> 24) & 0xFFu) / 255.0 - 250.0);
							if (IsPointShown(Miss))
							{
								GroundMemory->AddScar(Miss, From, 420.0f, 0.9f, 0.7f, Vis->ImpactCrater);
							}
						}
					}
				}
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
		// o tremor da explosão grande perto do centro (a escondida não treme: só a vista)
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (ARTSCameraPawn* Cam = Cast<ARTSCameraPawn>(PC->GetPawn()))
			{
				Cam->AddShake(Cm, View.IsStructure() ? 35.0f : 18.0f);
			}
		}
		if (View.IsStructure())
		{
			StructureDeath(View, Cm, Id);
		}
		else
		{
			if (UBlastFxDirector* Director = UBlastFxDirector::Get(this))
			{
				Director->TankDeath(FVector(Cm.X, Cm.Y, State ? State->Baked.GroundZCm(Cm) : 0.0f), Id);
			}
			if (GroundMemory)
			{
				// o avental da morte em volta do casco (13/10: a carcaça fica e cobre o miolo; a bacia é das
				// explosões sem carcaça em cima)
				const FVector2D* From = LastHitFrom.Find(Id);
				GroundMemory->AddWreck(Cm, float(View.GetActorRotation().Yaw), From ? *From : Cm);
				if (ARTSDebrisField* D = Debris.Get())
				{
					D->AddWreck(Cm, float(View.GetActorRotation().Yaw), From ? *From : Cm, [this](const FVector2D& P) { return State ? State->Baked.GroundZCm(P) : 0.0f; });
				}
			}
		}
	}
	if (!bSeen && !View.IsStructure() && GroundMemory)
	{
		const FVector2D* From = LastHitFrom.Find(Id);
		HiddenMarks.Add({ Cm, From ? *From : Cm, float(View.GetActorRotation().Yaw) });
	}
	LastHitFrom.Remove(Id);
	if (GroundMemory)
	{
		GroundMemory->ForgetUnit(Id);
	}
	View.bWreckKnown = bSeen;
	View.Die();
	Wrecks.Add(&View);
	if (!View.IsStructure())
	{
		FWreckPose& Pose = WreckPoses.Add(Id);
		Pose.Actor = &View;
	}
}

void URTSMatchSubsystem::UpdateWreckPoses()
{
	// a carcaça com física (13/10): o modelo, o fogo e as brasas (presos no ator) vão pra onde a simulação a empurrou,
	// interpolados entre os passos; a primeira vez guarda a base (o ator já está no lugar da morte, tombado)
	if (!State)
	{
		return;
	}
	const float Alpha = float(FMath::Clamp(Accumulator / StepSeconds, 0.0, 1.0));
	for (TPair<uint32, FWreckPose>& It : WreckPoses)
	{
		FWreckPose& P = It.Value;
		ARTSUnitActor* A = P.Actor.Get();
		const Sim::World::WreckView* C = State->CurrWrecks.Find(It.Key);
		if (!A || !C)
		{
			continue;
		}
		const Sim::World::WreckView* Pv = State->PrevWrecks.Find(It.Key);
		if (!Pv)
		{
			Pv = C;
		}
		const FVector2D Cm = FMath::Lerp(RTS::ToWorld(Pv->Pos), RTS::ToWorld(C->Pos), double(Alpha));
		const float YawP = RTS::YawDegrees(Pv->Hull);
		const float Yaw = YawP + FMath::FindDeltaAngleDegrees(YawP, RTS::YawDegrees(C->Hull)) * Alpha;
		if (!P.bInit)
		{
			P.bInit = true;
			P.BaseCm = Cm;
			P.BaseYaw = Yaw;
			P.LastCm = Cm;
			P.LastYaw = Yaw;
			P.BaseRot = A->GetActorQuat();
			P.ZOff = float(A->GetActorLocation().Z) - State->Baked.GroundZCm(Cm);
			P.LocalOffset = FVector(A->GetActorLocation().X - Cm.X, A->GetActorLocation().Y - Cm.Y, 0.0);
			continue;
		}
		if (FVector2D::DistSquared(Cm, P.LastCm) < 0.25 && FMath::Abs(FMath::FindDeltaAngleDegrees(P.LastYaw, Yaw)) < 0.05f)
		{
			continue;   // parada (quase sempre): nada a mexer
		}
		if (!P.bMovedLogged)
		{
			P.bMovedLogged = true;
			UE_LOG(LogTemp, Log, TEXT("RTS carcaça %u empurrada (de (%.0f, %.0f) m)"), It.Key, P.BaseCm.X / 100.0, P.BaseCm.Y / 100.0);
		}
		const float Turned = FMath::FindDeltaAngleDegrees(P.BaseYaw, Yaw);
		const FQuat Spin(FVector::UpVector, FMath::DegreesToRadians(Turned));
		const FVector Off = Spin.RotateVector(P.LocalOffset);
		A->SetActorLocationAndRotation(FVector(Cm.X + Off.X, Cm.Y + Off.Y, State->Baked.GroundZCm(Cm) + P.ZOff), Spin * P.BaseRot);
		// arrastada, ela risca o chão como esteira de lado (a memória do chão; só onde o jogador vê)
		const FVector2D Tracks = A->GetTracksCm();
		if (GroundMemory && Tracks.X > 0.0)
		{
			GroundMemory->TrackUnit(It.Key | 0x40000000u, Cm, Yaw, float(Tracks.X), float(Tracks.Y), !A->IsFogHidden() && IsPointShown(Cm));
		}
		P.LastCm = Cm;
		P.LastYaw = Yaw;
	}
}

void URTSMatchSubsystem::RevealHiddenMarks()
{
	// duas vezes por segundo: a morte que estava na névoa ganha o avental e os destroços quando a área aparece
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (HiddenMarks.IsEmpty() || Now < NextHiddenCheck || !GroundMemory)
	{
		return;
	}
	NextHiddenCheck = Now + 0.5;
	for (int32 K = HiddenMarks.Num() - 1; K >= 0; --K)
	{
		const FHiddenMark& H = HiddenMarks[K];
		if (!IsPointShown(H.Cm))
		{
			continue;
		}
		GroundMemory->AddWreck(H.Cm, H.YawDeg, H.From);
		if (ARTSDebrisField* D = Debris.Get())
		{
			D->AddWreck(H.Cm, H.YawDeg, H.From, [this](const FVector2D& P) { return State ? State->Baked.GroundZCm(P) : 0.0f; });
		}
		HiddenMarks.RemoveAtSwap(K);
	}
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
	Director->TankDeath(FVector(CenterCm.X, CenterCm.Y, State->Baked.GroundZCm(CenterCm)), Id, /*bClassic=*/true);   // (o prédio: a clássica)
	const FVector Extent = View.GetComponentsBoundingBox(true, false).GetExtent();   // true: as peças não têm colisão
	if (GroundMemory)
	{
		// o prédio deixa o chão queimado na pegada dele (sem cratera: a base de concreto segura)
		GroundMemory->AddScar(CenterCm, CenterCm, float(FMath::Max(Extent.X, Extent.Y)) * 0.95f, 0.85f, 0.0f);
	}
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
	// a silhueta atrás das coisas só pra quem é seu ou aliado (13/10, o Felipe: "nenhum tanque inimigo pode ser exibido
	// atrás das coisas": o inimigo escondido atrás da árvore fica escondido)
	if (!V->bStructure && !IsEnemyOfLocal(V->Owner))
	{
		Actor->SetSilhouette(int32(V->Owner) + 1);
	}
	Views.Add(Id, Actor);
}

void URTSMatchSubsystem::UpdateViews(float DeltaTime)
{
	RevealHiddenMarks();
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bFog = IsFogShown();
	for (const TPair<uint32, TObjectPtr<ARTSUnitActor>>& It : Views)
	{
		FRTSUnitState U;
		if (It.Value && GetUnit(It.Key, U))
		{
			const FVector Ground(U.PosCm.X, U.PosCm.Y, State->Baked.GroundZCm(U.PosCm));
			It.Value->SetPose(Ground, U.HullYawDeg, U.TurretYawDeg, State->Baked.GroundNormal(U.PosCm), DeltaTime);
			It.Value->SetDamage(U.MaxHp > 0 ? 1.0f - float(U.Hp) / float(U.MaxHp) : 0.0f);
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
			const FVector2D Tracks = It.Value->GetTracksCm();
			if (GroundMemory && !U.bStructure && Tracks.X > 0.0)
			{
				// a lagarta risca o chão onde o jogador vê (a de dentro da névoa não)
				GroundMemory->TrackUnit(It.Key, U.PosCm, U.HullYawDeg, float(Tracks.X), float(Tracks.Y), !It.Value->IsFogHidden() && IsPointShown(U.PosCm));
			}
		}
	}
	UpdateDocks();
	UpdateTreadDust(DeltaTime);
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
					Director->TankDeath(PendingBlasts[K].Ground, PendingBlasts[K].Variant, /*bClassic=*/true);
				}
				if (GroundMemory)
				{
					const FVector2D P(PendingBlasts[K].Ground.X, PendingBlasts[K].Ground.Y);
					// (13/10) o prédio que cai: as três explosões deixam a cratera média, a B
					GroundMemory->AddScar(P, P, 420.0f, 0.8f, 0.6f, 1);
				}
				PendingBlasts.RemoveAtSwap(K);
			}
		}
	}
	UpdateWreckPoses();
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
	if (GroundMemory)
	{
		// o caderno de detalhe segue o que a câmera olha
		FVector2D Focus = FVector2D::ZeroVector;
		float Yaw = 0.0f;
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (const ARTSCameraPawn* Cam = Cast<ARTSCameraPawn>(PC->GetPawn()))
			{
				const FVector F = Cam->GetLookFocusPoint();
				Focus = FVector2D(F.X, F.Y);
				Yaw = Cam->GetLookYaw();
			}
		}
		GroundMemory->Update(DeltaTime, Focus, Yaw);
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

void URTSMatchSubsystem::OrderMove(const TArray<uint32>& Units, const FVector2D& TargetCm, bool bAttackMove, bool bFormation,
	bool bFacing, float FacingDeg, float WidthM)
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
	// a direção vira inteiro aqui, no PC de quem manda: todos recebem o mesmo número
	const int32 FacingBam = FMath::RoundToInt(FMath::Fmod(FMath::Fmod(FacingDeg, 360.0f) + 360.0f, 360.0f) / 360.0f * float(Sim::Angle::FullTurn)) & 0xFFFF;
	C.Value = Sim::MoveValue(bFormation, bFacing, FMath::RoundToInt(WidthM * 0.5f), uint16_t(FacingBam));
	Issue(C);
}

void URTSMatchSubsystem::OrderAttack(const TArray<uint32>& Units, uint32 TargetId, bool bForced)
{
	if (!(State != nullptr) || !bReady || Units.IsEmpty() || MatchResult != ERTSMatchResult::Playing)
	{
		return;
	}
	IssueGroupIfChanged(Units);
	Sim::Command C;
	C.Player = LocalPlayer();
	C.Type = bForced ? Sim::CommandType::ForceAttack : Sim::CommandType::Attack;
	C.Target = TargetId;
	Issue(C);
}

void URTSMatchSubsystem::OrderAttackGround(const TArray<uint32>& Units, const FVector2D& PointCm)
{
	if (!(State != nullptr) || !bReady || Units.IsEmpty() || MatchResult != ERTSMatchResult::Playing)
	{
		return;
	}
	IssueGroupIfChanged(Units);
	Sim::Command C;
	C.Player = LocalPlayer();
	C.Type = Sim::CommandType::AttackGround;
	C.Point = RTS::ToSim(PointCm);
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
	if (ARTSDebrisField* D = Debris.Get())
	{
		D->Destroy();
	}
	Debris = nullptr;
	if (GroundMemory)
	{
		GroundMemory->Shutdown();
		GroundMemory = nullptr;
	}
	SaveReplay();
	delete State;
	State = nullptr;
	Views.Reset();
	Wrecks.Reset();
	WreckPoses.Reset();
	DockViews.Reset();
	PendingBlasts.Reset();
	DyingViews.Reset();
	DyingInfo.Reset();
	Super::Deinitialize();
}
