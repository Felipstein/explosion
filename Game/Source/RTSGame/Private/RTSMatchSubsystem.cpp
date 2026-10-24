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
#include "LookDirector.h"
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
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameInstance.h"
#include "RTSGameData.h"
#include "RTSMapBaker.h"
#include "RTSWireSystem.h"
#include "Misc/PackageName.h"
#include "RTSMissileSmoke.h"
#include "RTSHitLife.h"
#include "BlastFxHitLife.h"
#include "RTSAirlift.h"
#include "RTSStrikeFx.h"
#include "RTSSunShadow.h"
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
	/** (16/10) Os tiros em voo no passo anterior e no atual (o míssil da Patriot anda em 3D), pelo Id. */
	TMap<uint32, Sim::World::ProjectileView> PrevShots;
	TMap<uint32, Sim::World::ProjectileView> CurrShots;
	FVector2D LocalStartCm = FVector2D::ZeroVector;
	/** (v4, 23/10) Só desenho: o anel de aviso do Cruise Missile Strike e o avião de carga das zonas de lançamento (os atores
	 *  saem junto com a partida). */
	FRTSStrikeRings StrikeRings;
	FRTSAirlift Airlift;
};

namespace
{
	/** O míssil no mundo (cm): a altura dele acima do chão da simulação, posta no chão de verdade. */
	FVector MissileCm(const FRTSMatchState& S, const Sim::World::ProjectileView& P)
	{
		const FVector2D Xy = RTS::ToWorld(P.Pos);
		const double Above = (P.Z - S.World.GroundHeightAt(P.Pos)).ToDouble() * 100.0;
		return FVector(Xy.X, Xy.Y, S.Baked.GroundZCm(Xy) + Above);
	}

	FVector MissileDir(const Sim::World::ProjectileView& P)
	{
		const double Yaw = FMath::DegreesToRadians(double(RTS::YawDegrees(P.Yaw)));
		const double Pitch = FMath::DegreesToRadians(double(RTS::YawDegrees(P.Pitch)));
		return FVector(FMath::Cos(Pitch) * FMath::Cos(Yaw), FMath::Cos(Pitch) * FMath::Sin(Yaw), FMath::Sin(Pitch));
	}

	/** (v4) O relógio do desenho em passos da simulação, contínuo: o quadro desenha entre o estado de antes e o atual (o mesmo
	 *  Alpha do GetUnit e do GetMissiles), então o estado do passo T aparece quando este relógio chega em T. */
	double DisplayTicks(const FRTSMatchState& S, double Accumulator)
	{
		return double(S.World.Tick()) - 1.0 + FMath::Clamp(Accumulator / StepSeconds, 0.0, 1.0);
	}

	/** (v4) A zona de lançamento como o avião de carga vê: o lote do ator, as caixas no pátio e as vagas delas (Units.json). */
	bool AirliftZoneOf(const FRTSMatchState& S, const ARTSUnitActor* View, uint32 Id, FRTSAirliftZone& Out)
	{
		const Sim::UnitView* V = S.Curr.Find(Id);
		if (!V || !View || V->Hp <= 0 || View->IsDead() || V->Def >= S.World.Rules().Units.size())
		{
			return false;
		}
		Out.Body = View->GetBodyTransform();
		Out.Cargo = V->Cargo;
		Out.BoxesPerPallet = FMath::Max(1, S.World.Rules().Units[V->Def].AirliftBoxesPerPallet);
		const FRTSUnitVisual* Vis = S.Data.FindVisual(V->Def);
		Out.SlotsCm = Vis ? Vis->CargoSlotsCm : TArray<FVector>();
		Out.CrateMesh = Vis ? Vis->CargoMesh : FString();
		return true;
	}
}

URTSMatchSubsystem::URTSMatchSubsystem() = default;
URTSMatchSubsystem::~URTSMatchSubsystem()
{
	delete State;
	State = nullptr;
	delete MissileSmoke;
	MissileSmoke = nullptr;
	delete HitLife;
	HitLife = nullptr;
}

void URTSMatchSubsystem::FillHitLifeCtx(FRTSHitLifeCtx& Out)
{
	// (23/10, a explosão com vida) o que as reações do acerto leem do jogo (RTSHitLife.h)
	Out.World = GetWorld();
	Out.Dust = TreadDust.Get();
	Out.Fx = UBlastFxDirector::ShotFxEnabled() ? UBlastFxDirector::Get(this) : nullptr;
	Out.Now = Out.World ? Out.World->GetTimeSeconds() : 0.0;
	Out.GroundZCm = [this](const FVector2D& P) { return State ? float(State->Baked.GroundZCm(P)) : 0.0f; };
	// (a areia do acerto; a revisão) o quanto o chão solta poeira: a tabela única, a mesma da poeira das lagartas (o asfalto e o molhado não
	// soltam; o resto, com a areia solta por cima, solta como a areia)
	Out.DustYield = [this](const FVector2D& P) { return State ? float(FRTSHitLife::DustYieldOf(State->Baked.SurfaceAt(P))) : 1.0f; };
	Out.IsShown = [this](const FVector2D& P) { return IsPointShown(P); };
	Out.UnitsNear = [this](const FVector2D& At, double RadiusCm, TArray<ARTSUnitActor*>& Found)
	{
		if (!State)
		{
			return;
		}
		for (const TPair<uint32, Sim::UnitView>& It : State->Curr)
		{
			if (FVector2D::DistSquared(RTS::ToWorld(It.Value.Pos), At) <= RadiusCm * RadiusCm && IsUnitShown(It.Key))
			{
				if (ARTSUnitActor* A = ViewOf(It.Key))
				{
					Found.Add(A);
				}
			}
		}
	};
}

namespace
{
	// (17/10, a Arte) a explosão empurra a poeira das lagartas (o ARTSTreadDust::AddBlast, com a escala que o BlastFx desenha:
	// 1 = a Carga Pesada); quem chama já conferiu que o ponto está à vista (sob a névoa não empurra)
	void PushDust(ARTSTreadDust* D, const FVector& AtCm, double GroundZCm, double Scale, const FVector& DirW = FVector::ZeroVector, double HalfConeDeg = 180.0)
	{
		if (!D)
		{
			return;
		}
		FRTSDustBlast B;
		B.PosM = FVector(AtCm.X, AtCm.Y, GroundZCm) / 100.0;
		B.Scale = Scale;
		B.DirM = FVector(DirW.X, DirW.Y, 0.0).GetSafeNormal();
		B.HalfConeDeg = HalfConeDeg;
		B.bVisible = true;
		D->AddBlast(B);
	}
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
	// (v4) o dinheiro inicial escolhido (sem o × do -RTSFast): na LAN o da sala (o anfitrião), sozinho o do menu, na cena o dela
	int32 StartMoney = S->Data.DefaultStartingMoney;
	{
		const URTSSessionSubsystem* Session = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<URTSSessionSubsystem>() : nullptr;
		// a névoa: na LAN a da sala (o anfitrião escolhe); sozinho, a do menu
		ERTSFogMode FogMode = bNet ? NetSession->GetFogMode() : (Session ? Session->FogMode : ERTSFogMode::Explored);
		StartMoney = bNet ? NetSession->GetStartingMoney() : (Session ? Session->StartingMoney : StartMoney);
		if (bScenario)
		{
			FogMode = Scenario.Fog == TEXT("black") ? ERTSFogMode::Black : (Scenario.Fog == TEXT("explored") ? ERTSFogMode::Explored : ERTSFogMode::Off);
			Setup.Seed = uint64(Scenario.Seed);   // a mesma história a cada rodada
			StartMoney = Scenario.Money > 0 ? Scenario.Money : StartMoney;
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
		// (16/10) os carros e as árvores que o tanque esmaga e derruba (vão na montagem: a rede e o replay levam)
		Setup.Props.assign(S->Baked.Props.GetData(), S->Baked.Props.GetData() + S->Baked.Props.Num());
		Setup.Rules = S->Data.Rules;
		// (v4) o dinheiro inicial escolhido (× 5 no -RTSFast): vai nas regras da montagem, então todos (a rede, o replay) recebem
		Setup.Rules.Economy.StartingMoney = StartMoney * S->Data.MoneyScale;
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
				SlotAI.Add(bNet ? NetSession->IsAISlot(uint8(P)) : bScenario ? Scenario.AIPlayers.IsValidIndex(P) && Scenario.AIPlayers[P]
					: P != 0);
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
			// (v4, 22/10) a patente de largada da cena de teste ("start_rank")
			if (bScenario && Scenario.StartRanks.IsValidIndex(P))
			{
				Player.StartRank = uint8(FMath::Clamp(Scenario.StartRanks[P], 1, Sim::MaxRank));
			}
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
			// as unidades do cenário em pontos exatos ("units", 16/10: a cunha dos vídeos da poeira)
			for (const FRTSScenarioUnit& Un : (bScenario ? Scenario.Units : TArray<FRTSScenarioUnit>()))
			{
				if (Un.Player != P)
				{
					continue;
				}
				if (Setup.Rules.FindUnit(TCHAR_TO_UTF8(*Un.Key)) < 0)
				{
					OutError = FString::Printf(TEXT("unidade do cenário desconhecida: %s"), *Un.Key);
					return false;
				}
				Sim::StartingUnit U;
				U.Key = TCHAR_TO_UTF8(*Un.Key);
				U.Pos = RTS::ToSim(Un.AtM * 100.0);
				U.Facing = Sim::Angle::FromMilliDeg(RTS::Milli(Un.FacingDeg));
				Player.Units.push_back(U);
			}
			// os prédios prontos do cenário ("structures"), de frente pra câmera como todo prédio novo
			for (const FRTSScenarioStructure& St : (bScenario ? Scenario.Structures : TArray<FRTSScenarioStructure>()))
			{
				if (St.Player != P)
				{
					continue;
				}
				if (Setup.Rules.FindUnit(TCHAR_TO_UTF8(*St.Key)) < 0)
				{
					OutError = FString::Printf(TEXT("prédio do cenário desconhecido: %s"), *St.Key);
					return false;
				}
				Sim::StartingUnit U;
				U.Key = TCHAR_TO_UTF8(*St.Key);
				U.Pos = RTS::ToSim(St.AtM * 100.0);
				U.Facing = RTS::DirAngle(RTS::DirFacingViewer(GetDefault<ARTSCameraPawn>()->DefaultYawDeg));
				Player.Units.push_back(U);
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
	else if (!bNet && !bScenario && FParse::Param(FCommandLine::Get(), TEXT("RTSSelfAI")))
	{
		// (v4, 23/10) o teste IA x IA no jogo (game.ps1 view -Script aivsai): o robô joga pelo jogador local também
		S->Brains.emplace_back(Sim::PlayerIndex(LocalSlot));
		S->Brains.back().FirstWaveDelayTicks = 8 * Sim::TicksPerSecond;
		UE_LOG(LogTemp, Display, TEXT("RTS a IA joga pelo jogador local (-RTSSelfAI)"));
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
		UE_LOG(LogTemp, Log, TEXT("RTS LAN: montagem %016llx (%d bytes), %d jogadores, grade %016llx, névoa %s, dinheiro inicial %d"), (unsigned long long)Setup.Hash(), int32(W.Bytes.size()),
			int32(Setup.Players.size()), (unsigned long long)Setup.Map.Hash(), Setup.bFogOfWar ? TEXT("sim") : TEXT("não"), Setup.Rules.Economy.StartingMoney);
	}
	else if (GivenSetup)
	{
		// a grade daqui só desenha; se for diferente da do anfitrião, o chão e o caminho não batem (aviso)
		Sim::SimMap Mine = S->Baked.Logic;
		Mine.ComputeClearance();
		const bool bSameGrid = Mine.Hash() == Setup.Map.Hash();
		UE_LOG(LogTemp, Log, TEXT("RTS LAN: partida pela montagem do anfitrião %016llx, %d jogadores, vaga %d; a grade daqui %s (%016llx); dinheiro inicial %d"),
			(unsigned long long)Setup.Hash(), int32(Setup.Players.size()), int32(LocalSlot), bSameGrid ? TEXT("igual") : TEXT("DIFERENTE"), (unsigned long long)Mine.Hash(),
			Setup.Rules.Economy.StartingMoney);
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
			if (!GroundMemory->Init(GetWorld(), MinCm, MaxCm - MinCm, ZMin, ZMax, Info->WindYawDeg, Info->bGroundV3))
			{
				GroundMemory = nullptr;
			}
			else
			{
				// (16/10) o sulco de cada chão (a areia, a terra, o barro, a pedra e o asfalto) pelo surface.png do mapa
				GroundMemory->SetSurface(S->Baked.Surface, S->Baked.SurfaceRes, S->Baked.SurfaceMinM, S->Baked.SurfaceCellM);
			}
		}
		// o vento único do mapa: quem desenha (fumaça, poeira, bandeira, palmeira) lê o MPC_Wind
		if (UMaterialParameterCollection* Wind = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Materials/MPC_Wind.MPC_Wind"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			const float R = FMath::DegreesToRadians(Info->WindYawDeg);
			UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), Wind, TEXT("Wind"), FLinearColor(FMath::Cos(R), FMath::Sin(R), Info->WindSpeedMs, 0.3f));
		}
		// (17/10, o Fluxo) o mesmo vento pro BlastFx: a coluna de fumaça da carcaça em volume deita pra ele
		if (UBlastFxDirector* D = UBlastFxDirector::Get(this))
		{
			D->SetWind(Info->WindYawDeg, Info->WindSpeedMs);
		}
		// (16/10) a fumaça dos mísseis, no mesmo vento; a vida do rastro pelo preset dos efeitos (o rastro de verdade dura
		// minutos: o jogo corta antes de virar cortina)
		if (!MissileSmoke)
		{
			MissileSmoke = new FRTSMissileSmoke();
		}
		MissileSmoke->Shutdown();
		if (!FParse::Param(FCommandLine::Get(), TEXT("RTSNoMissileSmoke")) && MissileSmoke->Init(GetWorld()))
		{
			const float WindRad = FMath::DegreesToRadians(Info->WindYawDeg);
			MissileSmoke->WindDir = FVector2D(FMath::Cos(WindRad), FMath::Sin(WindRad));
			MissileSmoke->WindMs = Info->WindSpeedMs;
			// (16/10, o 1º vídeo: 16 s viraram cortina em cima das baterias) a vida pelo preset dos efeitos
			static const float LifeByPreset[4] = { 3.0f, 4.5f, 6.0f, 8.0f };
			// (17/10) o nível de efeitos do motor vai de 1 a 4 (baixa/média/alta/ultra = Médio/Alto/Épico/Cinematic)
			MissileSmoke->MaxLifeS = LifeByPreset[FMath::Clamp(Scalability::GetQualityLevels().EffectsQuality - 1, 0, 3)];
		}
		UBlastFxDirector::SetExternalMissileSmoke(MissileSmoke->IsReady());
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
				// (23/10, a areia do acerto) o vento único do mapa: a areia dos acertos deriva nele (a poeira das lagartas aprovada não: windGain 0)
				D->SetWind(Info->WindYawDeg, Info->WindSpeedMs);
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
		HiddenScars.Reset();
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
	UE_LOG(LogTemp, Log, TEXT("RTS skirmish em %s: %d unidades, semente %llu, montagem %016llx, dinheiro inicial %d"),
		*MapName, State->World.UnitCount(), (unsigned long long)Setup.Seed, (unsigned long long)Setup.Hash(), Setup.Rules.Economy.StartingMoney);
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
		// (17/10, o Look) a névoa de guerra com a câmera deitada: o Look recebe a visão (rts.FogStyle 1 ou 2)
		ALookDirector::SetFogOfWar(Fog->VisionTexture(), Fog->MapMinCm(), Fog->MapSizeCm());
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

// (v4, 23/10) as decisões da IA ("RTS AI p1: strike at x,y score s", a zona, o cerco, a expansão) no log da partida
static void LogBrain(Sim::SkirmishAI& Brain)
{
	std::vector<std::string> Lines;
	Brain.TakeLog(Lines);
	for (const std::string& L : Lines)
	{
		UE_LOG(LogTemp, Log, TEXT("%s"), UTF8_TO_TCHAR(L.c_str()));
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
		LogBrain(Brain);
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
	SnapshotShots();
	LastStepMs = (FPlatformTime::Seconds() - T0) * 1000.0;
	HandleEvents();
	HandleHudEvents();   // (v4) os avisos do HUD: a patente, o ataque chegando, o avião (o bloco da frente do HUD)
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
		{
			// (16/10) o raio-X: a casa e o muro tapam a unidade atrás deles (as células e o topo)
			bool bTaps = false;
			const TArray<TSharedPtr<FJsonValue>>* CellAt = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* CellsWH = nullptr;
			double HeightTap = 0.0, FloorTap = 0.0;
			Piece->TryGetNumberField(TEXT("height_m"), HeightTap);
			Piece->TryGetNumberField(TEXT("floor_cm"), FloorTap);
			if (Piece->TryGetBoolField(TEXT("blocks"), bTaps) && bTaps && HeightTap > 1.6 && Piece->TryGetArrayField(TEXT("cell"), CellAt) && CellAt->Num() >= 2
				&& Piece->TryGetArrayField(TEXT("cells"), CellsWH) && CellsWH->Num() >= 2)
			{
				const int32 I0 = int32((*CellAt)[0]->AsNumber()), J0 = int32((*CellAt)[1]->AsNumber());
				FXRayBlock Bl;
				Bl.Cells = FIntRect(I0, J0, I0 + int32((*CellsWH)[0]->AsNumber()), J0 + int32((*CellsWH)[1]->AsNumber()));
				Bl.TopCm = float(FloorTap + HeightTap * 100.0);
				XRayVillage.Add(Bl);
			}
		}
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
			int32 PieceId = -1;
			if (Piece->TryGetNumberField(TEXT("id"), PieceId) && PieceId >= 0)
			{
				VillagePieceInstance.Add(PieceId, TPair<TWeakObjectPtr<UInstancedStaticMeshComponent>, int32>(L, Index));   // (16/10) o carro esmagado
				// (16/10) a peça pequena que o veículo amassa (o esmagável 0 do ZH): o quanto ela achata, pelo tipo
				static const TMap<FString, float> SquashByType = {
					{ TEXT("LL_Prop_Crates"), 0.28f }, { TEXT("civ_crates"), 0.3f }, { TEXT("LL_Prop_Ammo"), 0.3f },
					{ TEXT("LL_Prop_Barrels"), 0.35f }, { TEXT("civ_barrels"), 0.35f }, { TEXT("LL_Prop_WaterTank"), 0.3f },
					{ TEXT("LL_Prop_Tires"), 0.6f }, { TEXT("LL_Prop_Dish"), 0.2f }, { TEXT("LL_Prop_AC"), 0.3f },
					{ TEXT("LL_Prop_Laundry"), 0.12f }, { TEXT("LL_Prop_Fence0"), 0.12f } };
				bool bBlocks = false;
				Piece->TryGetBoolField(TEXT("blocks"), bBlocks);
				if (const float* Sq = bBlocks ? nullptr : SquashByType.Find(Type))
				{
					FRTSSmallProp& Sp = SmallProps.AddDefaulted_GetRef();
					Sp.Piece = PieceId;
					Sp.PosCm = FVector2D(X, Y);
					Sp.SquashTo = *Sq;
					const TArray<TSharedPtr<FJsonValue>>* SizeM = nullptr;
					if (Piece->TryGetArrayField(TEXT("size_m"), SizeM) && SizeM->Num() >= 2)
					{
						Sp.RadiusCm = float(FMath::Max((*SizeM)[0]->AsNumber(), (*SizeM)[1]->AsNumber()) * 50.0);
					}
				}
			}
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
		Poles.Reset();
		WireFx.Reset();
		for (const TSharedPtr<FJsonValue>& PV : *PoleList)
		{
			TArray<FVector>& Out = Sockets.AddDefaulted_GetRef();
			const TSharedPtr<FJsonObject> Pole = PV->AsObject();
			FRTSPole& Pl = Poles.AddDefaulted_GetRef();   // (16/10) o poste que o tanque derruba
			if (Pole.IsValid())
			{
				Pole->TryGetNumberField(TEXT("piece"), Pl.Piece);
				Pl.PosCm = FVector2D(Pole->GetNumberField(TEXT("x")), Pole->GetNumberField(TEXT("y"))) * 100.0;
				double H = 8.0;
				Pole->TryGetNumberField(TEXT("height_m"), H);
				Pl.HeightM = float(H);
			}
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
		WireLayer = WireISM;
		for (int32 K = 0; K < Poles.Num() && K < Sockets.Num(); ++K)
		{
			Poles[K].Sockets = Sockets[K];
		}
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
			// (16/10) o fio e os postes dele: arrebenta quando um dos dois cai
			FRTSWire& Wf = WireFx.AddDefaulted_GetRef();
			Wf.EndA = A;
			Wf.EndB = B;
			Wf.SagCm = float(Sag);
			const TArray<TSharedPtr<FJsonValue>>* RefA = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* RefB = nullptr;
			if (W->TryGetArrayField(TEXT("a"), RefA) && RefA->Num() >= 2)
			{
				Wf.PoleA = int32((*RefA)[0]->AsNumber());
				Wf.SocketA = int32((*RefA)[1]->AsNumber());
			}
			if (W->TryGetArrayField(TEXT("b"), RefB) && RefB->Num() >= 2)
			{
				Wf.PoleB = int32((*RefB)[0]->AsNumber());
				Wf.SocketB = int32((*RefB)[1]->AsNumber());
			}
			for (const int32 Pk : { Wf.PoleA, Wf.PoleB })
			{
				if (Poles.IsValidIndex(Pk))
				{
					Poles[Pk].Wires.Add(WireFx.Num() - 1);
				}
			}
			FVector Prev = A;
			for (int32 K = 1; K <= N; ++K)
			{
				const double T = double(K) / N;
				const FVector Cur = FMath::Lerp(A, B, T) - FVector(0.0, 0.0, 4.0 * Sag * T * (1.0 - T));
				const FVector D = Cur - Prev;
				// o cilindro da Unreal tem 1 m de altura no Z e 1 m de diâmetro
				Wf.Segments.Add(WireISM->AddInstance(FTransform(FRotationMatrix::MakeFromZ(D.GetSafeNormal()).Rotator(), (Prev + Cur) * 0.5, FVector(0.045, 0.045, D.Size() / 100.0)), true));
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
	// os veículos de lagarta e (23/10) de rodas perto da câmera (os parados também: fazem sombra na poeira), na posição desenhada; a
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
		// (23/10, v4) o ator diz se levanta poeira e onde a areia é lida (cm no quadro do casco): a lagarta com o padrão do tanque aprovado; as
		// rodas (o caminhão de suprimento e o Tomahawk) com a geometria e a física delas (Plugins/RTSTreadDust/INTEGRACAO.md, "As rodas")
		FRTSTreadSource S;
		FVector2D SandRear, SandFront;
		if (!It.Value || !GetUnit(It.Key, U) || U.bStructure || !It.Value->FillDustSource(S, SandRear, SandFront) || FVector2D::Distance(U.PosCm, Focus) > 20000.0)
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
		S.Id = int32(It.Key);
		S.PosM = FVector(U.PosCm.X, U.PosCm.Y, State->Baked.GroundZCm(U.PosCm)) / 100.0;
		S.FwdM = FVector(Fwd.X, Fwd.Y, 0.0);
		S.Speed = Mo.Speed;
		S.YawRate = Mo.YawRate;
		const FVector2D R(Fwd.Y, -Fwd.X);
		// (a revisão) a tabela única do chão, a mesma do acerto de míssil (FRTSHitLife::DustYieldOf: 1 − max(asfalto, molhado), igual bit a bit)
		const auto Sand = [this](const FVector2D& P)
		{
			return FRTSHitLife::DustYieldOf(State->Baked.SurfaceAt(P));
		};
		S.Sand[0] = Sand(U.PosCm + (SandRear.X * Fwd - SandRear.Y * R));
		S.Sand[1] = Sand(U.PosCm + (SandRear.X * Fwd + SandRear.Y * R));
		S.SandFront[0] = Sand(U.PosCm + (SandFront.X * Fwd - SandFront.Y * R));
		S.SandFront[1] = Sand(U.PosCm + (SandFront.X * Fwd + SandFront.Y * R));
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

void URTSMatchSubsystem::UpdateSunShadow()
{
	UWorld* World = GetWorld();
	URTSSunShadowSubsystem* Sun = World ? World->GetSubsystem<URTSSunShadowSubsystem>() : nullptr;
	if (!Sun || !State)
	{
		return;
	}
	// o jogo manda todos os veículos (a lista do jogo manda; a da poeira, só os de lagarta dela, fica de reserva)
	FVector2D Focus = FVector2D::ZeroVector;
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (const ARTSCameraPawn* Cam = Cast<ARTSCameraPawn>(PC->GetPawn()))
		{
			const FVector F = Cam->GetLookFocusPoint();
			Focus = FVector2D(F.X, F.Y);
		}
	}
	// os veículos perto do foco (o mapa dos veículos é uma janela em volta dele), na pose desenhada; a caixa pelas malhas.
	// Sob a névoa não fazem sombra (a sombra entregaria a posição); a carcaça só depois de vista
	TArray<FRTSShadowVehicle> Out;
	const auto Add = [&Out, &Focus](const ARTSUnitActor* A, bool bVisible)
	{
		if (!A || A->IsStructure())
		{
			return;
		}
		const FVector L = A->GetActorLocation();
		if (FVector2D::Distance(FVector2D(L.X, L.Y), Focus) > 25000.0)
		{
			return;
		}
		FVector2D C;
		float HalfLen, HalfWidth, HullTop, Height;
		A->GetShadowBoxCm(C, HalfLen, HalfWidth, HullTop, Height);
		if (Height <= 1.0f)
		{
			return;   // (22/10) a carcaça que já afundou inteira
		}
		const FVector Fwd = A->HullForward();
		const FVector Right(-Fwd.Y, Fwd.X, 0.0);
		FRTSShadowVehicle V;
		V.Id = int32(A->GetSimId());
		V.PosM = (L + Fwd * C.X + Right * C.Y) / 100.0;
		V.FwdM = Fwd;
		V.TurretFwdM = A->TurretForward();
		V.HalfLenM = HalfLen / 100.0;
		V.HalfWidthM = HalfWidth / 100.0;
		V.HullTopM = HullTop / 100.0;
		V.HeightM = Height / 100.0;
		V.bVisible = bVisible;
		Out.Add(V);
	};
	for (const TPair<uint32, TObjectPtr<ARTSUnitActor>>& It : Views)
	{
		if (It.Value)
		{
			const FVector L = It.Value->GetActorLocation();
			Add(It.Value, !It.Value->IsFogHidden() && IsPointShown(FVector2D(L.X, L.Y)));
		}
	}
	for (const TObjectPtr<ARTSUnitActor>& A : DyingViews)
	{
		Add(A, A && !A->IsFogHidden());
	}
	for (const TObjectPtr<ARTSUnitActor>& A : Wrecks)
	{
		Add(A, A && A->bWreckKnown && !A->IsFogHidden());
	}
	Sun->SetVehicles(Out);
	// os prédios prontos e já vistos fazem sombra como cenário (o que cai ou é cancelado sai: Die, StructureCancelled). (17/10
	// noite, as faixas cinza em degrau atrás do Patriot do Felipe) só depois que o desenho trocou a obra pelo pronto: no quadro
	// em que a simulação termina, o andaime ainda está à vista, e a foto do mapa do sol o guardava (as grades viravam sombras de
	// 40 m com o sol a 8°, até o prédio cair); os prédios que já nascem prontos nunca tiveram o andaime
	for (const TPair<uint32, TObjectPtr<ARTSUnitActor>>& It : Views)
	{
		FRTSUnitState U;
		if (It.Value && It.Value->IsStructure() && !It.Value->IsBuildLookOn() && !SunCasters.Contains(It.Key) && !It.Value->IsFogHidden() && GetUnit(It.Key, U)
			&& U.BuildFraction >= 1.0f)
		{
			Sun->AddShadowCaster(It.Value);
			SunCasters.Add(It.Key);
		}
	}
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
		if (O.Type == TEXT("use_power"))
		{
			// (v4, 23/10) o poder do general (o índice em "unit", o alvo em to_m), pela fila como o clique do HUD
			Sim::Command Pw;
			Pw.Tick = Now;
			Pw.Player = uint8(O.Player);
			Pw.Type = Sim::CommandType::UsePower;
			Pw.Value = uint32(FMath::Max(O.Unit, 0));
			Pw.Point = RTS::ToSim(O.ToM * 100.0);
			S.Queue.Insert(Pw);
			UE_LOG(LogTemp, Log, TEXT("RTS cenário: passo %u, jogador %d usa o poder %d em (%.0f, %.0f) m"), Now, O.Player, Pw.Value, O.ToM.X, O.ToM.Y);
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
		if (O.Type == TEXT("sell") || O.Type == TEXT("repair") || O.Type == TEXT("force_attack_structure"))
		{
			// (v4, 23/10) o prédio do jogador mais perto de to_m: vendido (o Sell), consertado pelos Dozers dele (ou pelo N-ésimo
			// Dozer, "unit") ou alvo do tiro forçado dos armados dele (o Ctrl: avaria o prédio pro teste do conserto)
			const Sim::Vec2 At = RTS::ToSim(O.ToM * 100.0);
			Sim::EntityId Building = Sim::NoEntity;
			int64 Best = TNumericLimits<int64>::Max();
			for (int32 I = 0; I < S.World.UnitCount(); ++I)
			{
				const Sim::UnitView V = S.World.Unit(I);
				if (V.Owner == uint8(O.Player) && V.bStructure && V.Hp > 0 && Sim::DistSqRaw(V.Pos, At) < Best)
				{
					Best = Sim::DistSqRaw(V.Pos, At);
					Building = V.Id;
				}
			}
			if (Building == Sim::NoEntity)
			{
				UE_LOG(LogTemp, Warning, TEXT("RTS cenário: o jogador %d não tem prédio perto de (%.0f, %.0f) m pro %s"), O.Player, O.ToM.X, O.ToM.Y, *O.Type);
				continue;
			}
			Sim::Command C;
			C.Tick = Now;
			C.Player = uint8(O.Player);
			C.Target = Building;
			if (O.Type == TEXT("sell"))
			{
				C.Type = Sim::CommandType::Sell;
				S.Queue.Insert(C);
				UE_LOG(LogTemp, Log, TEXT("RTS cenário: passo %u, jogador %d vende o prédio %u"), Now, O.Player, Building);
				continue;
			}
			const bool bRepair = O.Type == TEXT("repair");
			Sim::Command Who;
			Who.Tick = Now;
			Who.Player = C.Player;
			Who.Type = Sim::CommandType::SetGroup;
			for (int32 I = 0; I < S.World.UnitCount(); ++I)
			{
				const Sim::UnitView V = S.World.Unit(I);
				const Sim::UnitDef& D = S.World.Rules().Units[V.Def];
				if (V.Owner == C.Player && !V.bStructure && V.Hp > 0 && (bRepair ? D.CanRepair() : D.HasWeapon()))
				{
					Who.Units.push_back(V.Id);
				}
			}
			if (O.Unit >= 0 && O.Unit < int32(Who.Units.size()))
			{
				const Sim::EntityId Only = Who.Units[size_t(O.Unit)];
				Who.Units.assign(1, Only);
			}
			C.Type = bRepair ? Sim::CommandType::Repair : Sim::CommandType::ForceAttack;
			S.Queue.Insert(Who);
			S.Queue.Insert(C);
			UE_LOG(LogTemp, Log, TEXT("RTS cenário: passo %u, jogador %d, %s no prédio %u com %d unidades"), Now, O.Player, *O.Type, Building, int32(Who.Units.size()));
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
		if (O.Unit >= 0)
		{
			// só a N-ésima (na ordem da criação, a mesma em toda rodada); sem ela, a ordem não sai
			if (O.Unit >= int32(G.Units.size()))
			{
				UE_LOG(LogTemp, Warning, TEXT("RTS cenário: a unidade %d do jogador %d não existe"), O.Unit, O.Player);
				continue;
			}
			const Sim::EntityId Only = G.Units[size_t(O.Unit)];
			G.Units.assign(1, Only);
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
		if (O.Type == TEXT("stop"))
		{
			C.Type = Sim::CommandType::Stop;   // (v4, 23/10) parar (o S): o tiro forçado do teste do conserto acaba aqui
			S.Queue.Insert(G);
			S.Queue.Insert(C);
			UE_LOG(LogTemp, Log, TEXT("RTS cenário: passo %u, jogador %d para %d unidades"), Now, O.Player, int32(G.Units.size()));
			continue;
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
		LogBrain(Brain);
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
	SnapshotShots();
	LastStepMs = (FPlatformTime::Seconds() - T0) * 1000.0;
	HandleEvents();
	HandleHudEvents();   // (v4) os avisos do HUD: a patente, o ataque chegando, o avião (o bloco da frente do HUD)
}

namespace
{
	/** (v4) O ataque do PowerUsed saiu melhorado? O da simulação (o ApplyPowerCommand olha a patente de antes do passo; o
	 *  CheckRanks do mesmo passo pode subir quem lançou depois): o número de mísseis do evento diz qual foi; com o mesmo número
	 *  nas duas patentes, a patente de antes do passo (a de agora menos os RankUp dele neste passo). */
	bool StrikeUpgraded(const Sim::World& W, const Sim::PowerDef& D, const Sim::SimEvent& Used)
	{
		if (D.UpgradeRank <= 0)
		{
			return false;
		}
		if (D.MissilesUpgraded != D.Missiles)
		{
			return ((Used.Value >> 16) & 0xFFFF) == D.MissilesUpgraded;
		}
		const uint8 Caster = uint8(Used.Value & 0xFF);
		int32 RankBefore = W.Rank(Caster);
		for (const Sim::SimEvent& R : W.LastEvents())
		{
			if (R.Type == Sim::EventType::RankUp && uint8(R.Value & 0xFF) == Caster)
			{
				--RankBefore;
			}
		}
		return RankBefore >= D.UpgradeRank;
	}

	/** O anel de quem lançou e dos aliados (o spec: "the caster sees the blue ring"): o azul da mira do poder no HUD (o V4Blue
	 *  do RTSHUDNew.cpp), qualquer que seja a cor do dono (o dono vermelho ficava com o anel igual ao aviso do inimigo). */
	FLinearColor StrikeFriendlyColor()
	{
		return FLinearColor(FColor(77, 143, 224));
	}
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
		case Sim::EventType::PropCrushed:
			OnPropCrushed(E);
			break;
		case Sim::EventType::PropToppled:
			OnPropToppled(E);
			// (16/10) a copa caída não tapa mais (o raio-X)
			if (bXRayGridBuilt && State && int32(E.Target) < State->World.PropCount())
			{
				const FVector2D C = RTS::ToWorld(State->World.PropAt(int32(E.Target)).Pos);
				const int32 X0 = FMath::FloorToInt((C.X - 300.0 - XRayMinCm.X) / XRayCellCm), X1 = FMath::FloorToInt((C.X + 300.0 - XRayMinCm.X) / XRayCellCm);
				const int32 Y0 = FMath::FloorToInt((C.Y - 300.0 - XRayMinCm.Y) / XRayCellCm), Y1 = FMath::FloorToInt((C.Y + 300.0 - XRayMinCm.Y) / XRayCellCm);
				for (int32 Y = FMath::Max(Y0, 0); Y <= FMath::Min(Y1, XRayH - 1); ++Y)
				{
					for (int32 X = FMath::Max(X0, 0); X <= FMath::Min(X1, XRayW - 1); ++X)
					{
						XRayCanopyLo[Y * XRayW + X] = XRayCanopyHi[Y * XRayW + X] = 0.0f;
					}
				}
			}
			break;
		case Sim::EventType::DefenseAssist:
		{
			// (16/10) a ajuda entre as baterias: o laser vermelho fino (o ZH: LaserFromAssisted, LaserToTarget), provisório
			const Sim::UnitView* From = S.Curr.Find(E.Source);
			const Sim::UnitView* Helper = S.Curr.Find(E.Target);
			if (From && Helper && IsUnitShown(E.Source))
			{
				const auto Top = [&S](const Sim::UnitView& V)
				{
					const FVector2D P = RTS::ToWorld(V.Pos);
					return FVector(P.X, P.Y, S.Baked.GroundZCm(P) + 320.0f);
				};
				const FVector2D AimXY = RTS::ToWorld(E.Point);
				const FVector Aim(AimXY.X, AimXY.Y, S.Baked.GroundZCm(AimXY) + 150.0f);
				// (16/10, o pm_missil2: com 6 de vermelho e 4 cm o brilho virava uma faixa borrada no chão) fino e fraco, só
				// entre as baterias
				if (UBlastFxDirector* AssistFx = UBlastFxDirector::ShotFxEnabled() ? UBlastFxDirector::Get(this) : nullptr)
				{
					AssistFx->DefenseAssist(Top(*From), Top(*Helper));   // (16/10) o laser de mira do Fluxo
				}
				else
				{
					const FLinearColor Red(1.6f, 0.05f, 0.03f);
					ARTSEffect::SpawnStreak(World, Top(*From), Top(*Helper), float(FVector::Dist(Top(*From), Top(*Helper)) / 0.25f), 900.0f, 2.0f, Red);
				}
			}
			break;
		}
		case Sim::EventType::WeaponFired:
		{
			if (E.Source == Sim::NoEntity)
			{
				// (v4, 23/10) o míssil sem atirador (o Cruise Missile Strike): nasce no ar, 150 m atrás e 45 m acima do ponto (o
				// SpawnBack e o SpawnUp do poder), já voando. A chama e a luz do Fluxo sem o clarão da caixa e sem a poeira do
				// chão (MissileInFlight); o rastro é o mesmo da Patriot (o UpdateMissileSmoke desenha todo míssil à vista); o
				// acerto e a cratera A vêm do ProjectileHit, que já aceita o tiro sem atirador. Nascendo na névoa, a chama acende
				// quando ele aparece (UpdateMissileFx)
				{
					// a ordem dele no ataque (a explosão grande do ProjectileHit vai no 1º, 3º, 5º...): o ataque cujo próximo ponto
					// de mira é este (cada ataque solta no máximo um míssil por passo, na ordem do uso). Antes, sai da conta o míssil
					// que já não voava nem no passo de antes (o acerto dele já passou; a saída de sempre é no ProjectileHit)
					for (auto It = StrikeMissileOrder.CreateIterator(); It; ++It)
					{
						if (!S.CurrShots.Contains(It.Key()) && !S.PrevShots.Contains(It.Key()))
						{
							It.RemoveCurrent();
						}
					}
					const FInt64Point Aim(E.Point.X.Raw, E.Point.Y.Raw);
					for (int32 I = 0; I < StrikeSalvos.Num(); ++I)
					{
						FStrikeSalvo& Salvo = StrikeSalvos[I];
						if (Salvo.Aims.IsValidIndex(Salvo.Fired) && Salvo.Aims[Salvo.Fired] == Aim)
						{
							StrikeMissileOrder.Add(uint32(E.Value), Salvo.Fired);
							if (++Salvo.Fired >= Salvo.Aims.Num())
							{
								StrikeSalvos.RemoveAt(I);
							}
							break;
						}
					}
				}
				const Sim::World::ProjectileView* Launched = S.CurrShots.Find(uint32(E.Value));
				if (Launched && Launched->bMissile)
				{
					if (ShotsFired == 0)
					{
						LastDeathWorldSeconds = World ? World->GetTimeSeconds() : LastDeathWorldSeconds;
					}
					++ShotsFired;
					UBlastFxDirector* StrikeFx = UBlastFxDirector::ShotFxEnabled() ? UBlastFxDirector::Get(this) : nullptr;
					if (StrikeFx && IsPointShown(RTS::ToWorld(Launched->Pos)))
					{
						StrikeFx->MissileInFlight(E.Value, MissileCm(S, *Launched), MissileDir(*Launched), uint32(E.Value));
					}
				}
				break;
			}
			ARTSUnitActor* Shooter = ViewOf(E.Source);
			if (!Shooter)
			{
				break;
			}
			// quem atira de dentro da névoa: nem clarão nem luz (entregariam a posição)
			const bool bShooterShown = IsUnitShown(E.Source);
			// (16/10) o míssil (a Patriot): o voo é da simulação (GetMissiles), sem o sopro de boca do tanque e sem traçante
			const Sim::UnitView* FiredBy = S.Curr.Find(E.Source);
			const bool bMissileShot = FiredBy && S.World.Rules().Units[FiredBy->Def].Weapon.IsMissile();
			// (23/10, v4) o lançador que levanta (o Tomahawk), em todo tiro dele, também na névoa: o tubo pelo lado de onde a simulação
			// soltou o míssil (a ponta dele some do tubo até a recarga, o ProjectileBoneFeedback do ZH) e a boca desenhada, de onde saem o
			// clarão e a nuvem e onde o voo novo começa (no chão inclinado o casco desenhado inclina; o ponto da simulação é plano)
			const Sim::World::ProjectileView* Launched = bMissileShot ? S.CurrShots.Find(uint32(E.Value)) : nullptr;
			FVector LaunchCm = Launched ? MissileCm(S, *Launched) : FVector::ZeroVector;
			FVector LaunchDir = Launched ? MissileDir(*Launched) : FVector::ForwardVector;
			if (Launched && Shooter->HasLauncher())
			{
				FVector MouthCm, MouthDir;
				Shooter->NoteLauncherShot(uint32(E.Value), LaunchCm, LaunchDir, MouthCm, MouthDir);
				LaunchCm = MouthCm;
				LaunchDir = MouthDir;
			}
			// (14/10) o tiro e o acerto do BlastFx (o Fluxo, a arte 4.2): o sopro, o traçante com luz, o acerto e o chão;
			// -ShotFx=0 volta pro provisório do ARTSEffect
			UBlastFxDirector* ShotFx = UBlastFxDirector::ShotFxEnabled() ? UBlastFxDirector::Get(this) : nullptr;
			if (bShooterShown)
			{
				Shooter->PlayFire(!bMissileShot);   // (v4) o lançador de mísseis sem o recuo e o tranco do canhão
				if (ShotFx && !bMissileShot)
				{
					const FVector Muzzle = Shooter->MuzzleLocation();
					ShotFx->MuzzleBlast(Muzzle, Shooter->MuzzleDirection(), S.Baked.GroundZCm(FVector2D(Muzzle.X, Muzzle.Y)), uint32(E.Source));
				}
				if (MissileSmoke && !bMissileShot)
				{
					MissileSmoke->Muzzle(Shooter->MuzzleLocation(), Shooter->MuzzleDirection());   // (16/10) o sopro abre a fumaça na frente
				}
				if (!bMissileShot)
				{
					// (17/10, a Arte) o sopro da boca empurra a poeira na frente do cano, num cone de 60°
					const FVector Mz = Shooter->MuzzleLocation();
					PushDust(TreadDust.Get(), Mz, S.Baked.GroundZCm(FVector2D(Mz.X, Mz.Y)), 0.15, Shooter->MuzzleDirection(), 60.0);
				}
			}
			if (ShotsFired == 0)
			{
				// o relógio do impasse começa no primeiro tiro, não na largada
				LastDeathWorldSeconds = World ? World->GetTimeSeconds() : LastDeathWorldSeconds;
			}
			++ShotsFired;
			if (bMissileShot)
			{
				// (16/10) o lançamento do Fluxo (o clarão, o jato de ré, a tampa e a poeira; a chama e o rastro nascem aqui),
				// na boca da caixa pela simulação (o lançador que levanta: na boca desenhada, LaunchCm acima)
				if (ShotFx && bShooterShown && Launched)
				{
					ShotFx->MissileLaunch(E.Value, LaunchCm, LaunchDir, uint32(E.Value));
				}
				if (MissileSmoke && bShooterShown && Launched)
				{
					MissileSmoke->Launch(LaunchCm, LaunchDir, uint32(E.Value));   // (16/10) a nuvem da ré da caixa
				}
				break;
			}
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
			const auto Trace = [&](const FVector& From)
			{
				if (ShotFx)
				{
					ShotFx->Tracer(From, Target, float(SpeedCmPerS), uint32(E.Source));
				}
				else
				{
					ARTSEffect::SpawnTracer(World, From, Target, float(SpeedCmPerS));
				}
			};
			if (bShooterShown)
			{
				Trace(Shooter->MuzzleLocation());
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
							Trace(P);
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
			LastHitCm = At;
			LastHitAtS = MatchSeconds();
			const Sim::UnitView* Shooter = S.Curr.Find(E.Source);
			FVector2D From = Shooter ? RTS::ToWorld(Shooter->Pos) : At;
			UBlastFxDirector* HitFx = UBlastFxDirector::ShotFxEnabled() ? UBlastFxDirector::Get(this) : nullptr;
			FVector From3(From.X, From.Y, S.Baked.GroundZCm(From) + 200.0f);
			const Sim::World::ProjectileView* Ms = E.Value != 0 ? S.PrevShots.Find(uint32(E.Value)) : nullptr;
			const bool bMissileHit = Ms && Ms->bMissile;
			// (v4, 23/10) a munição (o tipo do atirador, ou a do poder, que não tem atirador): a ogiva grande do Cruise Missile
			// Strike ganha a explosão clássica em volume e a cratera dela
			const FRTSUnitVisual* MunVis = Ms ? State->Data.FindVisual(int32(Ms->Def)) : nullptr;
			// a explosão grande em 1 de cada 2 mísseis do mesmo ataque (o 1º, o 3º, o 5º..., pela ordem do míssil no ataque, igual
			// em todo PC; o Id do tiro não servia: com outros tiros no meio, 5 de 6 grandes): com as 6 juntas a fuligem fechava ~35 m
			// por 3 s e cobria o alvo (a regra do jogo: efeito que cobre unidade é defeito); as outras ficam com o clarão e a
			// cratera. Sem ordem (o ataque de um míssil só, ou a arma de uma unidade), a grande
			int32 StrikeOrder = 0;
			StrikeMissileOrder.RemoveAndCopyValue(uint32(E.Value), StrikeOrder);
			const bool bHeavy = bMissileHit && MunVis && MunVis->bHeavyImpact && StrikeOrder % 2 == 0;
			// (23/10) a ogiva (Units.json "warhead_kg", só desenho; -MissileBlastV=1 volta a de antes): a explosão em volume do BlastFx
			// (a bola, a fuligem e o anel de areia pela carga); a metade pesada do ataque fica com a clássica (o TankDeath abaixo) e o
			// BlastFx só com o clarão e as faíscas. K = (W/4 kg)^⅓: a receita dos destroços e do empurrão na poeira é a da ogiva de 4 kg
			const bool bWarheadV2 = UBlastFxDirector::MissileBlastVer() >= 2;
			const float WarheadKg = bWarheadV2 && bMissileHit && MunVis ? MunVis->WarheadKg : 0.0f;
			const float BlastKg = bHeavy ? 0.0f : WarheadKg;
			// (23/10, a revisão) o visual da ogiva: a da metade leve do ataque com a fuligem, o sol, o céu e o chão da clássica ao lado
			const bool bClassicLook = MunVis && MunVis->bWarheadClassicLook;
			const double WarheadK = WarheadKg > 0.0f ? FMath::Pow(double(WarheadKg) / 4.0, 1.0 / 3.0) : 1.0;
			// (16/10) a ogiva de 3–5 kg: a bola de ~1,6 m de raio (~1 m por kg^⅓, LANL; o estudo do Fluxo) que sobe a ~4 m/s;
			// o tiro de canhão: a carga pequena, ~0,8 m [E]
			// (23/10) com a carga no Units.json, a bola dela (UBlastFxDirector::WarheadFireballM: 1,61 m na Patriot, 2,32 no Tomahawk,
			// 2,75 na clássica da metade pesada do ataque) e a subida por Froude (w ∝ √R, a de 4 kg a 4 m/s)
			const FVector TrailAt(At.X, At.Y, S.Baked.GroundZCm(At) + (E.Target == Sim::NoEntity ? 30.0f : 130.0f));
			float BlastR = bMissileHit ? 1.6f : 0.8f, BlastRise = bMissileHit ? 4.0f : 1.5f;
			if (bMissileHit && (WarheadKg > 0.0f || (bWarheadV2 && bHeavy)))
			{
				BlastR = UBlastFxDirector::WarheadFireballM(bHeavy ? 20.0f : WarheadKg);
				BlastRise = 4.0f * FMath::Sqrt(BlastR / 1.6f);
			}
			// (23/10, a revisão) com a ogiva v2 o empurrão e o calor no rastro ficam no centro da bola que se vê (o MissileHit devolve: o pé
			// fora da blindagem, virado pro míssil, na altura do miolo quente), não no centro do alvo; sem ela (o canhão, a metade pesada,
			// -MissileBlastV=1, sem o BlastFx) no lugar de sempre, e ainda antes do resto
			bool bTrailPending = MissileSmoke && IsPointShown(At);
			if (bTrailPending && !(bMissileHit && BlastKg > 0.0f && HitFx))
			{
				MissileSmoke->Blast(TrailAt, BlastR, BlastRise);
				bTrailPending = false;
			}
			// (23/10, a explosão com vida; RTSHitLife.h, BlastFxHitLife.h) o pulso do acerto de míssil: a munição entra no MissileHit com vida
			// (a direção, o leque dos estilhaços, a terra do chão) e o que ele desenhou volta pras reações do jogo (o tranco, a areia, a fumaça
			// do furo, o tremor, os vizinhos), cada uma atrás da sua variável blast.HitLife.<camada>. -HitLifeV=1: a de hoje, exata
			const bool bLife = bMissileHit && HitFx && bWarheadV2 && BlastFxHitLife::Version() >= 2;
			BlastFxHitLife::FHitIn LifeIn;
			BlastFxHitLife::FHitOut LifeOut;
			if (bLife && MunVis)
			{
				LifeIn.TiltDeg = MunVis->FragmentTiltDeg;
				LifeIn.Carry = MunVis->HitCarry;
				LifeIn.EjectaKg = MunVis->EjectaKg;
				LifeIn.bStrike = MunVis->bHeavyImpact;   // a munição do ataque de mísseis
			}
			// (a revisão) o véu do choque vai pra poeira em volume: o BlastFx não põe o anel de areia da ogiva por cima (uma representação só)
			LifeIn.bGameVeil = bLife && FRTSHitLife::DrawsShockVeil(TreadDust.Get());
			auto LifePulse = [&](uint32 VictimId, ARTSUnitActor* VictimActor)
			{
				if (!LifeOut.bValid)
				{
					return;
				}
				if (!HitLife)
				{
					HitLife = new FRTSHitLife();
				}
				FRTSBlastPulse Pulse;
				Pulse.Tick = int64(S.World.Tick());
				Pulse.Victim = VictimId;
				Pulse.Shooter = uint32(E.Source);
				Pulse.Kg = bHeavy ? 20.0f : FMath::Max(WarheadKg, 4.0f);
				Pulse.SandKg = MunVis && MunVis->SandKg > 0.0f ? MunVis->SandKg : 0.0f;   // (a areia do acerto) o ataque: 150 (o Tomahawk de verdade no chão)
				Pulse.RefDamage = Ms && Ms->Def < S.World.Rules().Units.size() ? float(S.World.Rules().Units[Ms->Def].Weapon.Damage) : 0.0f;
				const Sim::UnitView* TargetView = VictimId ? S.Curr.Find(VictimId) : nullptr;
				TargetView = TargetView ? TargetView : (VictimId ? S.Prev.Find(VictimId) : nullptr);
				const FRTSUnitVisual* TargetVis = TargetView ? State->Data.FindVisual(TargetView->Def) : nullptr;
				Pulse.MassT = TargetVis && TargetVis->MassT > 0.0f ? TargetVis->MassT : 60.0f;
				Pulse.bStrike = LifeIn.bStrike;
				Pulse.bVehicle = VictimActor && !VictimActor->IsStructure();
				Pulse.bKill = VictimId != 0 && S.Curr.Find(VictimId) == nullptr;
				Pulse.FromCm = From;
				Pulse.VictimActor = VictimActor;
				Pulse.Hit = LifeOut;
				FRTSHitLifeCtx Ctx;
				FillHitLifeCtx(Ctx);
				HitLife->OnPulse(Pulse, Ctx);
			};
			auto TrailBlast = [&](const FVector& Where)
			{
				if (bTrailPending)
				{
					// (23/10, a vida; a camada TrailRise) a subida que puxa a ponta do rastro dura o que a nuvem viva dura (τ = metade dela)
					const float RiseTau = LifeOut.bValid && LifeOut.CloudEndS > 0.0f && BlastFxHitLife::On(BlastFxHitLife::TrailRise) ? 0.5f * LifeOut.CloudEndS : 1.5f;
					MissileSmoke->Blast(Where, BlastR, BlastRise, RiseTau);
					bTrailPending = false;
				}
			};
			if (bMissileHit)
			{
				// (16/10) o míssil: o acerto na direção em que ele vinha (o ponto do passo de antes, no ar)
				From3 = MissileCm(S, *Ms);
				From = FVector2D(From3.X, From3.Y);
			}
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
					if (HitFx && bMissileHit)
					{
						TrailBlast(HitFx->MissileHit(E.Value, FVector(Hit.X, Hit.Y, S.Baked.GroundZCm(Hit)), From3, nullptr, S.Baked.GroundZCm(Hit), false, BlastKg,
							bClassicLook, bLife ? &LifeIn : nullptr, &LifeOut));   // (16/10) o míssil no chão
						if (bHeavy)
						{
							HitFx->TankDeath(FVector(Hit.X, Hit.Y, S.Baked.GroundZCm(Hit)), uint32(E.Value), true);   // (v4) a ogiva grande
						}
						LifePulse(0, nullptr);   // (23/10) a vida do acerto no chão
					}
					else if (HitFx)
					{
						HitFx->GroundHit(FVector(Hit.X, Hit.Y, S.Baked.GroundZCm(Hit)), From3, uint32(E.Source));
					}
					else
					{
						ARTSEffect::SpawnFlash(World, FVector(Hit.X, Hit.Y, S.Baked.GroundZCm(Hit) + 40.0f), 100.0f, 4.0e6f, 0.16f, FLinearColor(0.4f, 0.2f, 0.05f));
					}
					PushDust(TreadDust.Get(), FVector(Hit.X, Hit.Y, 0.0), S.Baked.GroundZCm(Hit), bHeavy ? 0.7 : bMissileHit ? 0.4 * WarheadK : 0.25);   // (17/10) o acerto no chão
					const FRTSUnitVisual* Vis = Shooter ? State->Data.FindVisual(Shooter->Def) : MunVis;
					if (GroundMemory && bLife && Vis && Vis->LifeCrater >= 0 && BlastFxHitLife::On(BlastFxHitLife::Strike))
					{
						// (23/10, a vida; o Felipe: "uma cratera bem maior") a cratera do míssil na escala dele (Units.json hit_life: a C, 1,6×, mais funda)
						GroundMemory->AddScar(Hit, From, Vis->LifeMarkM * 100.0f, 0.9f, Vis->LifeCraterDepth, Vis->LifeCrater);
					}
					else if (GroundMemory)
					{
						GroundMemory->AddScar(Hit, From, 420.0f, 0.9f, 0.7f, Vis && Vis->ImpactCrater >= 0 ? Vis->ImpactCrater : 0);
					}
				}
				else if (GroundMemory)
				{
					// (16/10) na névoa: a cratera fica guardada e aparece quando a área for vista (a batalha que ninguém viu
					// também deixa o chão marcado)
					const FRTSUnitVisual* Vis = Shooter ? State->Data.FindVisual(Shooter->Def) : MunVis;
					if (bLife && Vis && Vis->LifeCrater >= 0 && BlastFxHitLife::On(BlastFxHitLife::Strike))
					{
						HiddenScars.Add({ Hit, From, Vis->LifeMarkM * 100.0f, 0.9f, Vis->LifeCraterDepth, Vis->LifeCrater });   // (23/10) a cratera do míssil com vida
					}
					else
					{
						HiddenScars.Add({ Hit, From, 420.0f, 0.9f, 0.7f, Vis && Vis->ImpactCrater >= 0 ? Vis->ImpactCrater : 0 });
					}
				}
				TrailBlast(TrailAt);                // (23/10) o ponto espalhado fora da vista: o empurrão fica no lugar de sempre
				break;
			}
			LastHitFrom.Add(E.Target, From);
			if (IsPointShown(At))
			{
				if (ARTSUnitActor* Rocked = ViewOf(E.Target); Rocked && !(bLife && BlastFxHitLife::On(BlastFxHitLife::Kick)))
				{
					Rocked->PlayHit(From);   // o casco cede ao acerto, na mola (14/10); (23/10) com a vida, o tranco do míssil vai pelo pulso
				}
				if (HitFx)
				{
					// o acerto no casco (o ator ainda existe nesta hora; bKill = o tiro que matou)
					ARTSUnitActor* Victim = ViewOf(E.Target);
					if (bMissileHit)
					{
						// (16/10) a ogiva do míssil no casco (o Fluxo: a bola só no 1º acerto da rajada no mesmo alvo)
						TrailBlast(HitFx->MissileHit(E.Value, FVector(At.X, At.Y, S.Baked.GroundZCm(At) + 130.0f), From3, Victim, S.Baked.GroundZCm(At),
							S.Curr.Find(E.Target) == nullptr, BlastKg, bClassicLook, bLife ? &LifeIn : nullptr, &LifeOut));
						if (bHeavy)
						{
							HitFx->TankDeath(FVector(At.X, At.Y, S.Baked.GroundZCm(At)), uint32(E.Value), true);   // (v4) a ogiva grande no alvo
						}
						LifePulse(uint32(E.Target), Victim);   // (23/10) a vida do acerto no alvo
						PushDust(TreadDust.Get(), FVector(At.X, At.Y, 0.0), S.Baked.GroundZCm(At), bHeavy ? 0.7 : 0.4 * WarheadK);   // (17/10) a ogiva no casco
					}
					else
					{
						HitFx->ArmorHit(FVector(At.X, At.Y, S.Baked.GroundZCm(At) + 130.0f), From3, Victim, S.Baked.GroundZCm(At), uint32(E.Source), S.Curr.Find(E.Target) == nullptr);
						PushDust(TreadDust.Get(), FVector(At.X, At.Y, 0.0), S.Baked.GroundZCm(At), 0.28);   // (17/10) o acerto no casco
					}
				}
				else
				{
					// o clarão do impacto, provisório: laranja na exposição do look (com 3,2/1,9/0,7 era uma bola branca)
					ARTSEffect::SpawnFlash(World, FVector(At.X, At.Y, S.Baked.GroundZCm(At) + 130.0f), 100.0f, 4.0e6f, 0.16f, FLinearColor(0.4f, 0.2f, 0.05f));
				}
				if (GroundMemory)
				{
					// o tiro que acerta o tanque chamusca o chão embaixo (fraco): onde houve luta fica marcado
					if (LifeOut.bValid && !LifeOut.bTop && BlastFxHitLife::On(BlastFxHitLife::Ground))
					{
						// (23/10, a vida) o chamuscado do míssil no chão sob a bola (fora do casco), com o raio da bola primária, 1·W^⅓ m (1,6 m na
						// Patriot, 2,3 m no Tomahawk: a fuligem chega até ela, LANL), no lugar dos 1,3 m fracos embaixo do centro do tanque. (A
						// revisão) No teto a bola fica em cima do casco e o chão não queima: o de sempre
						const float Wkg = bHeavy ? 20.0f : FMath::Max(WarheadKg, 4.0f);
						GroundMemory->AddScar(FVector2D(LifeOut.BallCentreCm.X, LifeOut.BallCentreCm.Y), From, 100.0f * FMath::Pow(Wkg, 1.0f / 3.0f), 0.35f, 0.0f);
					}
					else
					{
						GroundMemory->AddScar(At, From, 130.0f, 0.35f, 0.0f);
					}
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
								if (HitFx)
								{
									HitFx->GroundHit(FVector(Miss.X, Miss.Y, S.Baked.GroundZCm(Miss)), From3, uint32(E.Source));
									PushDust(TreadDust.Get(), FVector(Miss.X, Miss.Y, 0.0), S.Baked.GroundZCm(Miss), 0.25);   // (17/10) o raspando
								}
							}
						}
					}
				}
			}
			TrailBlast(TrailAt);                    // (23/10) o empurrão que a bola não levou (não deveria sobrar: o acerto à vista chama o MissileHit)
			break;
		}
		case Sim::EventType::UnitDamaged:
		{
			const Sim::UnitView* Victim = S.Curr.Find(E.Target);
			if (Victim && Victim->Owner == LocalPlayer())
			{
				DamageAlerts.Add(RTS::ToWorld(Victim->Pos));
			}
			// (23/10, a explosão com vida; a camada Neighbors) o vizinho do dano em área reage pelo pulso do mesmo passo (o tranco e a marca)
			if (HitLife && BlastFxHitLife::On(BlastFxHitLife::Neighbors) && IsPointShown(RTS::ToWorld(E.Point)))
			{
				FRTSHitLifeCtx Ctx;
				FillHitLifeCtx(Ctx);
				HitLife->OnDamaged(int64(S.World.Tick()), uint32(E.Target), uint32(E.Source), float(E.Value), ViewOf(E.Target), Ctx);
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
			if (MissileSmoke && IsPointShown(At))
			{
				// (16/10) a morte: a bola de fogo do combustível e da munição (~3,5 m de raio; o prédio ~6 m) [E], subindo
				const bool bBig = Actor && Actor->IsStructure();
				MissileSmoke->Blast(FVector(At.X, At.Y, S.Baked.GroundZCm(At) + 150.0f), bBig ? 6.0f : 3.5f, bBig ? 6.0f : 5.0f);
			}
			if (IsPointShown(At) && !(Actor && Actor->IsStructure()))
			{
				// (17/10, a Arte) a morte do tanque empurra a poeira (0,55, a escala desenhada); a do prédio sai de cada explosão
				// dela (o StructureDeath e as PendingBlasts)
				PushDust(TreadDust.Get(), FVector(At.X, At.Y, 0.0), S.Baked.GroundZCm(At), 0.55);
			}
			if (Actor && Actor->IsStructure())
			{
				// (v4) as caixas do pátio da zona de lançamento caem como a pilha da "carga que cai" (o CargoDropped, o depósito
				// novo que o caminhão pega): não ficam desenhadas no prédio morto
				Actor->SetCargo(0);
			}
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
		case Sim::EventType::StructureSelling:
			// (v4, 23/10) a desmontagem começou: o desenho é a obra ao contrário (GetUnit: o andaime, o corte descendo e a solda)
			UE_LOG(LogTemp, Log, TEXT("RTS vender: o prédio %u começou a desmontar (%d passos)"), E.Target, E.Value);
			break;
		case Sim::EventType::StructureSold:
		{
			// (v4, 23/10) vendido: o "+$" de sempre pro dono (o do caminhão e da zona) e o prédio sai sem explodir, como a obra cancelada
			const int32 Refund = E.Value & 0xFFFFFF;
			if (uint8(uint32(E.Value) >> 24) == LocalPlayer() && Refund > 0)
			{
				FRTSMoneyPopup Pop;
				Pop.PosCm = RTS::ToWorld(E.Point);
				Pop.Amount = Refund;
				MoneyPopups.Add(Pop);
			}
			UE_LOG(LogTemp, Log, TEXT("RTS vender: o prédio %u vendido, $%d pro jogador %d"), E.Target, Refund, int32(uint32(E.Value) >> 24));
		}
			[[fallthrough]];
		case Sim::EventType::StructureCancelled:
			// a obra cancelada (e, v4, o prédio vendido) some sem explodir nem virar carcaça
			if (TObjectPtr<ARTSUnitActor>* View = Views.Find(E.Target))
			{
				if (SunCasters.Remove(E.Target) > 0)
				{
					if (URTSSunShadowSubsystem* Sun = World ? World->GetSubsystem<URTSSunShadowSubsystem>() : nullptr)
					{
						Sun->RemoveShadowCaster(View->Get());
					}
				}
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
		case Sim::EventType::WreckSinking:
			OnWreckSinking(E);
			break;
		case Sim::EventType::WreckRemoved:
			OnWreckRemoved(E);
			break;
		case Sim::EventType::PowerUsed:
		{
			// (v4, 23/10) o anel de aviso do Cruise Missile Strike (4 s no chão, só desenho): azul pra quem lançou e os aliados
			// dele, em vermelho pro inimigo que vê o ponto; o aviso escrito é do HUD (HandleHudEvents)
			const uint8 Caster = uint8(E.Value & 0xFF);
			const int32 Power = (E.Value >> 8) & 0xFF;
			const Sim::Ruleset& Rules = S.World.Rules();
			LastStrikeCm = RTS::ToWorld(E.Point);
			LastStrikeAtS = MatchSeconds();
			++StrikeSerial;
			if (Caster < S.World.PlayerCount() && Power < int32(Rules.Powers.size()))
			{
				const Sim::PowerDef& D = Rules.Powers[size_t(Power)];
				// a ordem dos mísseis deste ataque (a explosão grande no 1º, 3º, 5º...): os pontos de mira, da simulação. O ataque
				// entrou no ar neste passo e o 1º míssil já saiu: é o mais novo com o mesmo dono, poder, centro e número. O de um
				// míssil só já acabou e saiu da lista: fica sem ordem (a grande, é o 1º)
				const int32 Missiles = (E.Value >> 16) & 0xFFFF;
				const uint32 NowTick = S.World.Tick();
				StrikeSalvos.RemoveAll([NowTick](const FStrikeSalvo& Old) { return NowTick > Old.EndTick; });
				for (int32 I = S.World.StrikeCount() - 1; I >= 0; --I)
				{
					const Sim::World::StrikeView V = S.World.StrikeAt(I);
					if (V.Owner == Caster && V.Power == Power && V.Center == E.Point && V.Total == Missiles)
					{
						FStrikeSalvo& Salvo = StrikeSalvos.AddDefaulted_GetRef();
						for (int32 K = 0; K < V.Total; ++K)
						{
							const Sim::Vec2 Aim = S.World.StrikeAimPoint(I, K);
							Salvo.Aims.Add(FInt64Point(Aim.X.Raw, Aim.Y.Raw));
						}
						Salvo.EndTick = NowTick + uint32(V.Total * FMath::Max(D.IntervalTicks, 1) + Sim::TicksPerSecond);
						break;
					}
				}
				// o raio do girassol que a simulação usou (cresce na UpgradeRank; o mesmo do aviso do HUD)
				const bool bUp = StrikeUpgraded(S.World, D, E) && D.PatternRadiusUpgraded.Raw > 0;
				const float RadiusCm = float((bUp ? D.PatternRadiusUpgraded : D.PatternRadius).ToDouble() * 100.0);
				const FVector2D At = RTS::ToWorld(E.Point);
				const bool bFriendly = IsSpectatingAll() || !IsEnemyOfLocal(Caster);
				if (bFriendly || IsPointShown(At))
				{
					// a marca comprida do anel aponta pra largada de quem lançou: os mísseis chegam de lá (o From da simulação)
					const FVector2D Way = At - PlayerStartCm(Caster);
					const float FromYaw = Way.SizeSquared() > 1.0 ? float(FMath::RadiansToDegrees(FMath::Atan2(Way.Y, Way.X))) : 0.0f;
					const APlayerController* ViewPc = World ? World->GetFirstPlayerController() : nullptr;
					const FVector ViewCm = ViewPc && ViewPc->PlayerCameraManager ? ViewPc->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
					S.StrikeRings.Add(World, At, RadiusCm, bFriendly ? StrikeFriendlyColor() : FRTSStrikeRings::WarningColor(), FromYaw,
						double(S.World.Tick()) - 1.0, [&S](const FVector2D& P) { return S.Baked.GroundZCm(P); },
						ViewPc && ViewPc->PlayerCameraManager ? &ViewCm : nullptr);
				}
			}
			break;
		}
		case Sim::EventType::AirliftInbound:
		{
			// (v4, 23/10) o avião de carga da zona vem aí (só desenho; o aviso do HUD é do HandleHudEvents): os paraquedas tocam
			// o pátio LeadTicks depois deste passo, no relógio da simulação (o passo do AirliftLanded)
			const Sim::UnitView* Zone = S.Curr.Find(E.Source);
			FRTSAirliftZone Info;
			if (Zone && AirliftZoneOf(S, ViewOf(E.Source), E.Source, Info))
			{
				const Sim::UnitDef& D = S.World.Rules().Units[Zone->Def];
				const FRTSUnitVisual* Vis = S.Data.FindVisual(Zone->Def);
				S.Airlift.Inbound(World, E.Source, E.Value, double(S.World.Tick()) - 1.0 + double(D.AirliftLeadTicks), Info,
					PlayerStartCm(Zone->Owner), Vis ? Vis->AirliftAltitudeM : 70.0f, PlayerColor(Zone->Owner));
			}
			break;
		}
		case Sim::EventType::AirliftLanded:
			// os paraquedas somem no quadro em que as caixas aparecem no pátio (o SetCargo do prédio, no UpdateViews)
			S.Airlift.Landed(E.Source);
			break;
		default:
			break;
		}
	}
}

namespace
{
	// (22/10) a carcaça que some (o item 4 da v4; a simulação manda: 48 s sólida, 7 s afundando, os tetos de 40 e 64):
	// - o pulso de poeira no começo do afundar (o campo de poeira em volume aprovado; 0,15 contra 0,55 da morte de tanque);
	// - o fogo que ainda queima apaga em 3 s quando ela começa a afundar (a luz do ABlastDebris some nos últimos 3 s);
	// - a que saiu da simulação ainda inteira (o teto de 64) desce o resto em 2 s no desenho, e a da onda do derrotado espera
	//   3 s (a bola de fogo da morte) antes de descer;
	// - os fantasmas da névoa: no máximo 32 esperando o lugar ser visto de novo
	constexpr double WreckSinkDustScale = 0.15;
	constexpr float WreckFireFadeS = 3.0f;
	constexpr double WreckFinishSinkS = 2.0;
	constexpr double WreckGoneEarlyDelayS = 3.0;
	constexpr int32 MaxWreckGhosts = 32;
	/** A carcaça de veículo arrastada risca o chão com esta marca a mais no Id (a memória do chão). */
	constexpr uint32 WreckTrackBit = 0x40000000u;
}

void URTSMatchSubsystem::Die(ARTSUnitActor& View, const FVector2D& Cm, uint32 Id, uint8 Owner)
{
	if (View.IsStructure() && SunCasters.Remove(Id) > 0)
	{
		if (URTSSunShadowSubsystem* Sun = GetWorld() ? GetWorld()->GetSubsystem<URTSSunShadowSubsystem>() : nullptr)
		{
			Sun->RemoveShadowCaster(&View);
		}
	}
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
			// (v4, 23/10) a carcaça antes da explosão: o pivô da torre solta já vai pro centro de massa dela, onde o BlastFx prende o
			// rastro de fuligem (senão o rastro ganhava um risco de 2 m do chão do casco até a torre no 1º quadro)
			View.Die();
			if (UBlastFxDirector* Director = UBlastFxDirector::Get(this))
			{
				// (17/10, o Fluxo) o ator da carcaça vai junto: o fogo dela acende pela morte, qualquer que seja o caminho
				Director->TankDeath(FVector(Cm.X, Cm.Y, State ? State->Baked.GroundZCm(Cm) : 0.0f), Id, /*bClassic=*/false, &View);
			}
			if (GroundMemory)
			{
				// o avental da morte em volta do casco (13/10: a carcaça fica e cobre o miolo; a bacia é das
				// explosões sem carcaça em cima)
				const FVector2D* From = LastHitFrom.Find(Id);
				GroundMemory->AddWreck(Cm, float(View.GetActorRotation().Yaw), From ? *From : Cm);
				if (ARTSDebrisField* D = Debris.Get())
				{
					D->AddWreck(Cm, float(View.GetActorRotation().Yaw), From ? *From : Cm, [this](const FVector2D& P) { return State ? State->Baked.GroundZCm(P) : 0.0f; }, Id);
				}
			}
		}
	}
	if (!bSeen && !View.IsStructure() && GroundMemory)
	{
		const FVector2D* From = LastHitFrom.Find(Id);
		HiddenMarks.Add({ Cm, From ? *From : Cm, float(View.GetActorRotation().Yaw), Id });
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
		if (WreckGoneEarly.Remove(Id) > 0)
		{
			// (22/10) a simulação já tirou esta carcaça (o teto de 64, na onda do derrotado): ela afunda logo depois da bola
			Pose.RemovedAt = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + WreckGoneEarlyDelayS;
		}
	}
}

void URTSMatchSubsystem::SnapshotShots()
{
	FRTSMatchState& S = *State;
	CheckPoles();   // (16/10) o poste derrubado, pelas posições deste passo
	S.PrevShots = MoveTemp(S.CurrShots);
	S.CurrShots.Reset();
	for (int32 I = 0; I < S.World.ProjectileCount(); ++I)
	{
		const Sim::World::ProjectileView P = S.World.ProjectileAt(I);
		S.CurrShots.Add(P.Id, P);
		// o provisório do voo, até o míssil do Fluxo (BlastFx): um risco curto e quente por passo, do ponto de antes ao de
		// agora; na névoa, nada (o tiro entregaria a posição)
		const Sim::World::ProjectileView* Before = S.PrevShots.Find(P.Id);
		if (P.bMissile && Before && IsPointShown(RTS::ToWorld(P.Pos)) && !UBlastFxDirector::ShotFxEnabled())
		{
			const FVector A = MissileCm(S, *Before), B = MissileCm(S, P);
			// (o risco anda o passo em 3 quadros: com 1 ele era apagado no quadro em que nascia)
			ARTSEffect::SpawnStreak(GetWorld(), A, B, float(FVector::Dist(A, B) / (3.0 * StepSeconds)), 320.0f, 16.0f, FLinearColor(9.0f, 4.5f, 1.4f));
		}
	}
}

FRTSPropFx* URTSMatchSubsystem::PropFxFor(int32 Prop)
{
	for (FRTSPropFx& F : PropFx)
	{
		if (F.Prop == Prop)
		{
			return &F;
		}
	}
	if (!State || !State->Baked.Props.IsValidIndex(Prop) || !GetWorld())
	{
		return nullptr;
	}
	// a peça desenhada: a instância da vila (o carro) ou a do mapa (a árvore); some da camada e vira um ator móvel com a
	// mesma malha e os mesmos materiais (a camada é estática: não dá pra mexer na instância)
	UInstancedStaticMeshComponent* Ism = nullptr;
	int32 Index = -1;
	const FRTSBakedMap& B = State->Baked;
	if (B.PropPiece[Prop] >= 0)
	{
		if (const TPair<TWeakObjectPtr<UInstancedStaticMeshComponent>, int32>* Found = VillagePieceInstance.Find(B.PropPiece[Prop]))
		{
			Ism = Found->Key.Get();
			Index = Found->Value;
		}
	}
	else
	{
		Ism = B.PropIsm[Prop].Get();
		Index = B.PropIsmIndex[Prop];
	}
	FTransform T;
	if (!Ism || Index < 0 || !Ism->GetInstanceTransform(Index, T, /*bWorldSpace=*/true))
	{
		return nullptr;
	}
	AActor* Proxy = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), T);
	if (!Proxy)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Proxy, TEXT("Prop"));
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Ism->GetStaticMesh());
	for (int32 K = 0; K < Ism->GetNumMaterials(); ++K)
	{
		C->SetMaterial(K, Ism->GetMaterial(K));
	}
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->bReceivesDecals = false;
	Proxy->SetRootComponent(C);
	C->RegisterComponent();
	Proxy->SetActorTransform(T);
	FTransform Hidden = T;
	Hidden.SetScale3D(FVector(0.001));
	Ism->UpdateInstanceTransform(Index, Hidden, /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);
	FRTSPropFx F;
	F.Prop = Prop;
	F.Proxy = Proxy;
	F.Base = T;
	F.bTree = B.Props[Prop].Kind == Sim::PropKind::Tree;
	PropFx.Add(F);
	return &PropFx.Last();
}

void URTSMatchSubsystem::OnPropCrushed(const Sim::SimEvent& E)
{
	FRTSMatchState& S = *State;
	const int32 Prop = int32(E.Target);
	if (Prop >= S.World.PropCount())
	{
		return;
	}
	const Sim::World::PropView Pv = S.World.PropAt(Prop);
	const FVector2D At = RTS::ToWorld(E.Point);
	const bool bShown = IsPointShown(At);
	// o tranco no tanque: o nariz sobe no carro e o lado da lagarta que passou por cima (a mola desce sozinha)
	if (ARTSUnitActor* Tank = bShown ? ViewOf(E.Source) : nullptr)
	{
		Tank->PlayBump(55.0f, (E.Value & 4) ? -25.0f : 25.0f);
	}
	const bool bFirst = !PropFx.ContainsByPredicate([Prop](const FRTSPropFx& X) { return X.Prop == Prop; });
	FRTSPropFx* F = PropFxFor(Prop);
	if (!F)
	{
		return;
	}
	// (16/10, o Fluxo) o vidro, as lanternas, os pneus, a poeira e o raspão pela passagem da lagarta: (o comprimento do carro,
	// o +Y da malha, + 4,5 m de lagarta no chão) ÷ a velocidade do tanque; o pisca-alerta em metade dos carros (pelo índice)
	if (bFirst && bShown && UBlastFxDirector::ShotFxEnabled())
	{
		AActor* Car = F->Proxy.Get();
		const Sim::UnitView* Tank = S.Curr.Find(E.Source);
		UBlastFxDirector* Fx = UBlastFxDirector::Get(this);
		if (Car && Tank && Fx)
		{
			const double Yaw = FMath::DegreesToRadians(double(RTS::YawDegrees(Tank->Hull)));
			const FVector Dir(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0);
			const float SpeedMs = FMath::Max(float(Tank->Speed.ToDouble() * Sim::TicksPerSecond), 1.0f);
			const UStaticMeshComponent* Body = Cast<UStaticMeshComponent>(Car->GetRootComponent());
			const float LenM = Body && Body->GetStaticMesh() ? float(2.0 * Body->GetStaticMesh()->GetBounds().BoxExtent.Y * F->Base.GetScale3D().Y / 100.0) : 4.5f;
			const float Pass = (FMath::Clamp(LenM, 3.0f, 6.5f) + 4.5f) / SpeedMs;
			const bool bAlarm = ((uint32(Prop) * 2654435761u) >> 31) != 0;
			Fx->CarCrushStart(Car, Dir * 100.0, Pass, uint32(Prop) * 0x9E3779B9u, bAlarm);
			FCarPass& Cp = CarPasses.AddDefaulted_GetRef();
			Cp.Prop = Prop;
			Cp.Tank = E.Source;
			Cp.Until = GetWorld() ? GetWorld()->GetTimeSeconds() + Pass : 0.0;
		}
	}
	// o achatado (provisório, o Look refaz a malha amassada): os dois lados a 42% da altura, um só a 70% com o lado
	// esmagado mais baixo (a frente é o +Y da malha), torcido pro lado da lagarta
	const bool bFront = (Pv.State & 1) != 0, bBack = (Pv.State & 2) != 0;
	// a malha amassada do Look, quando existe (/Game/Village/<malha>_Crushed, _CrushedFront, _CrushedBack): fica inteira,
	// sem o achatado provisório
	if (UStaticMeshComponent* C = F->Proxy.IsValid() ? Cast<UStaticMeshComponent>(F->Proxy->GetRootComponent()) : nullptr)
	{
		const FString Base = F->BaseMesh.IsEmpty() && C->GetStaticMesh() ? C->GetStaticMesh()->GetName() : F->BaseMesh;
		F->BaseMesh = Base;
		const FString Name = Base + ((bFront && bBack) ? TEXT("_Crushed") : (bFront ? TEXT("_CrushedFront") : TEXT("_CrushedBack")));
		if (UStaticMesh* Crushed = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Village/%s/StaticMeshes/%s.%s"), *Name, *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			C->SetStaticMesh(Crushed);
			F->SquashTo = F->Squash = 1.0f;
			F->TiltTo = F->Tilt = 0.0f;
			F->RollTo = F->Roll = 0.0f;
			F->bDone = true;
			if (bShown && GroundMemory)
			{
				GroundMemory->AddScar(At, At, 160.0f, 0.25f, 0.0f);
			}
			return;
		}
	}
	F->SquashTo = (bFront && bBack) ? 0.42f : 0.7f;
	F->TiltTo = (bFront && bBack) ? 0.0f : (bFront ? -7.0f : 7.0f);
	F->RollTo = (Pv.Side ? 1.0f : -1.0f) * ((bFront && bBack) ? 3.0f : 5.0f);
	if (!bShown)
	{
		F->Squash = F->SquashTo;
		F->Tilt = F->TiltTo;
		F->Roll = F->RollTo;
	}
	F->bDone = false;
	if (bShown && GroundMemory)
	{
		GroundMemory->AddScar(At, At, 160.0f, 0.25f, 0.0f);   // o óleo e a sujeira do carro amassado no chão (fraco)
	}
}

void URTSMatchSubsystem::OnPropToppled(const Sim::SimEvent& E)
{
	FRTSMatchState& S = *State;
	const int32 Prop = int32(E.Target);
	if (Prop >= S.World.PropCount())
	{
		return;
	}
	const Sim::World::PropView Pv = S.World.PropAt(Prop);
	FRTSPropFx* F = PropFxFor(Prop);
	if (!F)
	{
		return;
	}
	// a vara que tomba em volta do pé (o tempo sai da física, não de constante: θ'' = 3g/(2L)·sen θ), com o empurrão da
	// velocidade do tanque; para em 87° (o ZH) ou encostada no prédio (o comprimento livre da simulação), quicando a −0,3
	const double Yaw = FMath::DegreesToRadians(double(RTS::YawDegrees(Pv.FallDir)));
	const FVector Dir(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0);
	F->Axis = FVector::CrossProduct(FVector::UpVector, Dir).GetSafeNormal();
	F->Pivot = F->Base.GetLocation();
	F->LengthM = FMath::Max(float(Pv.Height.ToDouble()), 2.0f);
	const float Free = float(Pv.FallLength.ToDouble());
	F->MaxTheta = Free >= F->LengthM * 0.999f ? FMath::DegreesToRadians(87.0f) : FMath::Asin(FMath::Clamp(Free / F->LengthM, 0.05f, 1.0f));
	const float SpeedMs = float((E.Value >> 16) & 0x7FFF) / 100.0f;
	F->Theta = 0.03f;
	F->Omega = 0.6f * SpeedMs / F->LengthM + 0.1f;
	F->bDone = false;
	// (16/10, o WorldForge) o toco fica de pé e o tronco cai pelo pivô da quebra (a 0,8 m do pé) até a ponta encostar no
	// chão; a copa (atrasa na queda, chicoteia no impacto e assenta) é do material, pelo CustomPrimitiveData
	UStaticMeshComponent* C = F->Proxy.IsValid() ? Cast<UStaticMeshComponent>(F->Proxy->GetRootComponent()) : nullptr;
	if (C && C->GetStaticMesh() && GetWorld())
	{
		UStaticMesh* Whole = C->GetStaticMesh();
		const FString Folder = FPackageName::GetLongPackagePath(Whole->GetOutermost()->GetName());
		const auto Piece = [&Folder, Whole](const TCHAR* Suffix)
		{
			const FString N = Whole->GetName() + Suffix;
			return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *Folder, *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet);
		};
		UStaticMesh* Stump = Piece(TEXT("_Stump"));
		UStaticMesh* Fallen = Piece(TEXT("_Fallen"));
		if (Stump && Fallen)
		{
			if (AActor* StumpActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), F->Base))
			{
				UStaticMeshComponent* Sc = NewObject<UStaticMeshComponent>(StumpActor, TEXT("Stump"));
				Sc->SetMobility(EComponentMobility::Movable);
				Sc->SetStaticMesh(Stump);
				Sc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				StumpActor->SetRootComponent(Sc);
				Sc->RegisterComponent();
				StumpActor->SetActorTransform(F->Base);
			}
			C->SetStaticMesh(Fallen);
			C->EmptyOverrideMaterials();      // os materiais vêm na malha (a folhagem, o tronco e a quebra)
			const float BreakM = 0.8f * float(F->Base.GetScale3D().Z);
			F->Pivot = F->Base.GetLocation() + F->Base.GetRotation().GetUpVector() * (BreakM * 100.0);
			F->Base.SetLocation(F->Pivot);
			F->LengthM = FMath::Max(F->LengthM - BreakM, 2.0f);
			// deitada: a ponta encosta no chão de ONDE ela cai (16/10, o WorldForge: na duna mais alta que o pé a ponta e a
			// copa entravam na areia), cos θ = (z_chão − z_pivô)/L; no plano dá o 90° + asen(h/L) de antes. O prédio no
			// caminho para antes, como antes
			const FVector Tip = F->Pivot + Dir * (double(F->LengthM) * 100.0);
			const double TipGround = State->Baked.GroundZCm(FVector2D(Tip.X, Tip.Y));
			const float Lying = FMath::Acos(FMath::Clamp(float((TipGround - F->Pivot.Z) / (double(F->LengthM) * 100.0)), -1.0f, 0.99f));
			F->MaxTheta = Free >= F->LengthM * 0.999f ? Lying : FMath::Min(F->MaxTheta, Lying);
			// o plano do chão em volta da copa pro material (CPD 5–7: z = a + b·x + c·y, cm do mundo), por mínimos quadrados
			// em 5 pontos centrados na ponta (os de lado e os ao longo são simétricos: Σx = Σy = 0, a média é o a do centro)
			{
				const FVector Side(-Dir.Y, Dir.X, 0.0);
				const FVector Offs[5] = { FVector::ZeroVector, Side * 200.0, -Side * 200.0, Dir * 200.0, -Dir * 200.0 };
				double Sz = 0.0, Sxx = 0.0, Syy = 0.0, Sxy = 0.0, Sxz = 0.0, Syz = 0.0;
				for (const FVector& O : Offs)
				{
					const double Z = State->Baked.GroundZCm(FVector2D(Tip.X + O.X, Tip.Y + O.Y));
					Sz += Z;
					Sxx += O.X * O.X;
					Syy += O.Y * O.Y;
					Sxy += O.X * O.Y;
					Sxz += O.X * Z;
					Syz += O.Y * Z;
				}
				const double Mean = Sz / 5.0;
				const double Det = Sxx * Syy - Sxy * Sxy;
				const double B = FMath::Abs(Det) > 1e-6 ? (Sxz * Syy - Syz * Sxy) / Det : 0.0;
				const double Cs = FMath::Abs(Det) > 1e-6 ? (Syz * Sxx - Sxz * Sxy) / Det : 0.0;
				const double A = Mean - B * Tip.X - Cs * Tip.Y;
				C->SetCustomPrimitiveDataFloat(5, float(A));
				C->SetCustomPrimitiveDataFloat(6, float(B));
				C->SetCustomPrimitiveDataFloat(7, float(Cs));
			}
		}
		const FVector Local = F->Base.GetRotation().UnrotateVector(Dir);
		C->SetCustomPrimitiveDataFloat(1, float(GetWorld()->GetTimeSeconds()));
		C->SetCustomPrimitiveDataFloat(2, float(FMath::Atan2(Local.Y, Local.X)));
		C->SetCustomPrimitiveDataFloat(3, 1.0f);
		C->SetCustomPrimitiveDataFloat(4, 0.0f);
	}
	if (!IsPointShown(RTS::ToWorld(Pv.Pos)))
	{
		F->Theta = F->MaxTheta;   // na névoa: já caída quando aparecer
		F->Omega = 0.0f;
		if (C && GetWorld())
		{
			C->SetCustomPrimitiveDataFloat(1, float(GetWorld()->GetTimeSeconds()) - 10.0f);
			C->SetCustomPrimitiveDataFloat(4, float(GetWorld()->GetTimeSeconds()) - 9.0f);   // já assentada
		}
	}
}

namespace
{
	// (16/10) o cenário mudou numa área: o mapa do sol comum refaz a sombra dela sem travar (m, no chão)
	void RefreshSunArea(UWorld* World, const FVector& CenterCm, double RadiusM)
	{
		if (URTSSunShadowSubsystem* Sun = World ? World->GetSubsystem<URTSSunShadowSubsystem>() : nullptr)
		{
			Sun->RefreshShadowArea(FVector(CenterCm.X, CenterCm.Y, 0.0) / 100.0, RadiusM);
		}
	}
}

void URTSMatchSubsystem::UpdatePropFx(float DeltaSeconds)
{
	const float Dt = FMath::Clamp(DeltaSeconds, 0.0f, 0.05f);
	for (FRTSPropFx& F : PropFx)
	{
		AActor* Proxy = F.Proxy.Get();
		if (!Proxy || F.bDone)
		{
			continue;
		}
		if (F.bTree)
		{
			const float G = 9.81f;
			for (int32 K = 0; K < 4; ++K)
			{
				const float H = Dt / 4.0f;
				F.Omega += 1.5f * G / F.LengthM * FMath::Sin(F.Theta) * H;
				F.Theta += F.Omega * H;
				if (F.Theta >= F.MaxTheta)
				{
					if (!F.bImpacted)
					{
						F.bImpacted = true;
						if (UStaticMeshComponent* Pc = Cast<UStaticMeshComponent>(Proxy->GetRootComponent()); Pc && GetWorld())
						{
							Pc->SetCustomPrimitiveDataFloat(4, float(GetWorld()->GetTimeSeconds()));   // (16/10) o chicote da copa
						}
					}
					F.Theta = F.MaxTheta;
					F.Omega = -0.3f * F.Omega;   // o quique no chão (o BounceVelocityPercent do ZH)
					if (FMath::Abs(F.Omega) < 0.08f)
					{
						F.Omega = 0.0f;
						F.bDone = true;
					}
				}
			}
			const FQuat Q(F.Axis, F.Theta);
			const FVector Loc = F.Pivot + Q.RotateVector(F.Base.GetLocation() - F.Pivot);
			Proxy->SetActorTransform(FTransform(Q * F.Base.GetRotation(), Loc, F.Base.GetScale3D()));
			if (F.bDone)
			{
				// deitou: a palmeira (ou o poste) sai da sombra de pé e entra a deitada
				RefreshSunArea(GetWorld(), F.Pivot, F.LengthM + 2.0);
			}
			continue;
		}
		const float K = 1.0f - FMath::Exp(-14.0f * Dt);
		F.Squash += (F.SquashTo - F.Squash) * K;
		F.Tilt += (F.TiltTo - F.Tilt) * K;
		F.Roll += (F.RollTo - F.Roll) * K;
		if (FMath::Abs(F.Squash - F.SquashTo) < 0.002f && FMath::Abs(F.Tilt - F.TiltTo) < 0.05f)
		{
			F.Squash = F.SquashTo;
			F.Tilt = F.TiltTo;
			F.Roll = F.RollTo;
			F.bDone = true;
			RefreshSunArea(GetWorld(), F.Base.GetLocation(), 4.0);   // o carro amassado: a sombra mais baixa
		}
		// o eixo do carro (o +Y da malha) e o de lado (o +X); a frente desce girando em volta do de lado
		const FQuat BaseQ = F.Base.GetRotation();
		const FVector Along = BaseQ.RotateVector(FVector::RightVector);
		const FQuat Q = FQuat(FVector::CrossProduct(FVector::UpVector, Along).GetSafeNormal(), FMath::DegreesToRadians(-F.Tilt)) * FQuat(Along, FMath::DegreesToRadians(F.Roll));
		FVector Scale = F.Base.GetScale3D();
		Scale.Z *= F.Squash;
		Proxy->SetActorTransform(FTransform(Q * BaseQ, F.Base.GetLocation(), Scale));
	}
}

void URTSMatchSubsystem::UpdateCarPasses(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (CarPasses.IsEmpty() || !World || !State)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	UBlastFxDirector* Fx = UBlastFxDirector::Get(this);
	for (int32 K = CarPasses.Num() - 1; K >= 0; --K)
	{
		FCarPass& Cp = CarPasses[K];
		if (Now >= Cp.Until)
		{
			CarPasses.RemoveAtSwap(K);   // a passagem acabou inteira
			continue;
		}
		const Sim::UnitView* Tank = State->Curr.Find(Cp.Tank);
		const float SpeedMs = Tank ? float(Tank->Speed.ToDouble() * Sim::TicksPerSecond) : 0.0f;
		Cp.Still = SpeedMs < 0.5f ? Cp.Still + DeltaSeconds : 0.0f;
		if (!Tank || Cp.Still > 0.5f)
		{
			// o tanque parou ou morreu em cima: o que já aconteceu fica, o resto não acontece
			const FRTSPropFx* Pf = PropFx.FindByPredicate([&Cp](const FRTSPropFx& X) { return X.Prop == Cp.Prop; });
			if (Fx && Pf && Pf->Proxy.IsValid())
			{
				Fx->CarCrushEnd(Pf->Proxy.Get());
			}
			CarPasses.RemoveAtSwap(K);
		}
	}
}

void URTSMatchSubsystem::CollectRocks()
{
	// uma vez por partida: toda instância de malha que tem a irmã <malha>_Broken no mesmo pacote (o WorldForge fez as do
	// RockSmall), numa grade de 10 m pra conta por veículo ficar curta
	bRocksCollected = true;
	Rocks.Reset();
	RockGrid.Reset();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	TMap<UStaticMesh*, bool> HasBroken;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TInlineComponentArray<UInstancedStaticMeshComponent*> Isms(*It);
		for (UInstancedStaticMeshComponent* Ism : Isms)
		{
			UStaticMesh* Mesh = Ism ? Ism->GetStaticMesh().Get() : nullptr;
			if (!Mesh || Mesh->GetName().EndsWith(TEXT("_Broken")))
			{
				continue;
			}
			bool* Known = HasBroken.Find(Mesh);
			if (!Known)
			{
				const FString Folder = FPackageName::GetLongPackagePath(Mesh->GetOutermost()->GetName());
				const FString N = Mesh->GetName() + TEXT("_Broken");
				const bool bHas = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *Folder, *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet) != nullptr;
				Known = &HasBroken.Add(Mesh, bHas);
			}
			if (!*Known)
			{
				continue;
			}
			const FVector Ext = Mesh->GetBounds().BoxExtent;
			for (int32 K = 0; K < Ism->GetInstanceCount(); ++K)
			{
				FTransform T;
				if (!Ism->GetInstanceTransform(K, T, /*bWorldSpace=*/true))
				{
					continue;
				}
				FRTSRockSpot& Rs = Rocks.AddDefaulted_GetRef();
				Rs.Ism = Ism;
				Rs.Index = K;
				Rs.PosCm = FVector2D(T.GetLocation().X, T.GetLocation().Y);
				Rs.RadiusCm = float(FMath::Max(Ext.X, Ext.Y) * T.GetScale3D().GetMax());
				RockGrid.FindOrAdd(FIntPoint(FMath::FloorToInt32(float(Rs.PosCm.X / 1000.0)), FMath::FloorToInt32(float(Rs.PosCm.Y / 1000.0)))).Add(Rocks.Num() - 1);
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("RTS pedras: %d pedras pequenas que partem sob o tanque (as malhas _Broken do WorldForge)"), Rocks.Num());
}

void URTSMatchSubsystem::BreakRock(int32 Rock)
{
	FRTSRockSpot& Rs = Rocks[Rock];
	Rs.bBroken = true;
	UInstancedStaticMeshComponent* Ism = Rs.Ism.Get();
	UStaticMesh* Whole = Ism ? Ism->GetStaticMesh().Get() : nullptr;
	FTransform T;
	if (!Whole || !GetWorld() || !Ism->GetInstanceTransform(Rs.Index, T, /*bWorldSpace=*/true))
	{
		return;
	}
	// a partida fica no lugar, numa camada por componente de origem (o tom do mapa do slot 0 vai junto; o slot 1, a face
	// nova da quebra, é o da malha)
	UInstancedStaticMeshComponent* Layer = BrokenRockLayer.FindRef(Ism).Get();
	if (!Layer)
	{
		const FString Folder = FPackageName::GetLongPackagePath(Whole->GetOutermost()->GetName());
		const FString N = Whole->GetName() + TEXT("_Broken");
		UStaticMesh* Broken = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *Folder, *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet);
		AActor* Holder = Broken ? GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity) : nullptr;
		if (!Holder)
		{
			return;
		}
		Layer = NewObject<UInstancedStaticMeshComponent>(Holder, TEXT("BrokenRocks"));
		Layer->SetMobility(EComponentMobility::Movable);
		Layer->SetStaticMesh(Broken);
		Layer->SetMaterial(0, Ism->GetMaterial(0));
		Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Holder->SetRootComponent(Layer);
		Layer->RegisterComponent();
		BrokenRockLayer.Add(Ism, Layer);
	}
	Layer->AddInstance(T, /*bWorldSpace=*/true);
	FTransform Hidden = T;
	Hidden.SetScale3D(FVector(0.001));
	Ism->UpdateInstanceTransform(Rs.Index, Hidden, /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);
	// o pó fino da pedra moída no chão, fraco
	if (GroundMemory)
	{
		GroundMemory->AddScar(Rs.PosCm, Rs.PosCm, Rs.RadiusCm * 1.6f, 0.12f, 0.0f);
	}
	RefreshSunArea(GetWorld(), FVector(Rs.PosCm.X, Rs.PosCm.Y, 0.0), Rs.RadiusCm / 100.0 + 1.0);
}

void URTSMatchSubsystem::UpdateWires()
{
	UWorld* World = GetWorld();
	URTSWireSystem* WireSim = World ? World->GetSubsystem<URTSWireSystem>() : nullptr;
	if (WireSim)
	{
		for (FRTSWire& Wf : WireFx)
		{
			if (Wf.Handle < 0)
			{
				continue;
			}
			for (const int32 End : { Wf.Follow, Wf.Follow2 })
			{
				const int32 Pole = End == 0 ? Wf.PoleA : (End == 1 ? Wf.PoleB : -1);
				const int32 Socket = End == 0 ? Wf.SocketA : Wf.SocketB;
				if (!Poles.IsValidIndex(Pole) || !Poles[Pole].Sockets.IsValidIndex(Socket))
				{
					continue;
				}
				// a cruzeta do poste caindo: a tomada de pé levada pela vara (o ator do poste que tomba)
				const FRTSPropFx* Pf = PropFx.FindByPredicate([Pole](const FRTSPropFx& X) { return X.Prop == -1 - Pole; });
				const AActor* Proxy = Pf ? Pf->Proxy.Get() : nullptr;
				if (!Proxy)
				{
					continue;
				}
				const FVector Now = Proxy->GetActorTransform().TransformPosition(Pf->Base.InverseTransformPosition(Poles[Pole].Sockets[Socket]));
				if (Pf->bDone)
				{
					// (17/10, o Fluxo) o poste deitou: a cruzeta e o fio vivo encostam no chão sem arrebentar (a falta à terra,
					// o caso mais comum do poste derrubado): o arco ali e o religador tentando de novo, uma vez por fio. O poste
					// que parou encostado num prédio fica com a cruzeta no alto e não faz arco
					const double Gz = State ? double(State->Baked.GroundZCm(FVector2D(Now.X, Now.Y))) : Now.Z;
					if (!WireGrounded.Contains(Wf.Handle) && Now.Z - Gz < 150.0)
					{
						WireGrounded.Add(Wf.Handle);
						WireArc(Now, 1);
					}
					continue;
				}
				WireSim->MoveSpanEnd(Wf.Handle, End, Now);
			}
		}
	}
	// a tomada do poste caindo, onde ela está agora (levada pela vara)
	auto SocketNow = [this](int32 Pole, int32 Socket, FVector& Out)
	{
		const FRTSPropFx* Pf = PropFx.FindByPredicate([Pole](const FRTSPropFx& X) { return X.Prop == -1 - Pole; });
		const AActor* Proxy = Pf ? Pf->Proxy.Get() : nullptr;
		if (!Proxy || !Poles.IsValidIndex(Pole) || !Poles[Pole].Sockets.IsValidIndex(Socket))
		{
			return false;
		}
		Out = Proxy->GetActorTransform().TransformPosition(Pf->Base.InverseTransformPosition(Poles[Pole].Sockets[Socket]));
		return true;
	};
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	// (17/10, o Fluxo; o Felipe: "quando bate no poste e ele começa a cair... é onde já dá o choque") o poste passou de ~17°:
	// os condutores balançam e batem um no outro lá em cima (a falta entre fases), o clarão na cruzeta ainda no ar, e o fio
	// batendo de novo ~0,15 s depois (noutra tomada da mesma cruzeta, se tiver), antes de o religador desarmar
	for (int32 P = 0; P < Poles.Num(); ++P)
	{
		FRTSPole& Pl = Poles[P];
		if (!Pl.bAirArcPending)
		{
			continue;
		}
		const FRTSPropFx* Pf = PropFx.FindByPredicate([P](const FRTSPropFx& X) { return X.Prop == -1 - P; });
		if (!Pf || Pf->bDone)
		{
			Pl.bAirArcPending = false;
			continue;
		}
		if (Pf->Theta < FMath::DegreesToRadians(17.0f))
		{
			continue;
		}
		Pl.bAirArcPending = false;
		TArray<int32, TInlineAllocator<4>> Socks;
		for (const int32 Wi : Pl.Wires)
		{
			const FRTSWire& Wf = WireFx[Wi];
			Socks.AddUnique(Wf.PoleA == P ? Wf.SocketA : Wf.SocketB);
		}
		FVector At;
		if (Socks.Num() > 0 && SocketNow(P, Socks[0], At))
		{
			WireArc(At, 0);
			WireBursts.Add({ Now + 0.15, At, 0, P, Socks.Last() });
		}
	}
	// as rajadas do religador: a rede tenta religar e a ponta caída faísca de novo (~0,6 s, ~2,2 s e ~5 s depois), depois
	// trava (as 3 religadas e o bloqueio do religador de distribuição)
	for (int32 K = WireBursts.Num() - 1; K >= 0; --K)
	{
		if (Now >= WireBursts[K].At)
		{
			FVector At = WireBursts[K].PointCm;
			if (WireBursts[K].Pole >= 0)
			{
				SocketNow(WireBursts[K].Pole, WireBursts[K].Socket, At);
			}
			WireArc(At, WireBursts[K].Kind);
			WireBursts.RemoveAtSwap(K);
		}
	}
}

void URTSMatchSubsystem::WireArc(const FVector& PointCm, int32 Kind)
{
	UWorld* World = GetWorld();
	if (!World || !IsPointShown(FVector2D(PointCm.X, PointCm.Y)))
	{
		return;
	}
	// (16/10 noite) o arco é do Fluxo: o clarão branco-azulado com luz (acende a fumaça perto pela RTSSceneLights), as
	// gotas de metal, as lascas do isolador e, nas rajadas do religador (Kind 2), a fumacinha em volume. Aqui ficam o
	// agendamento das rajadas e a marquinha queimada na areia
	// (17/10, o Fluxo) a falta no chão é uma por cruzeta (onde o fio encosta primeiro) e cada religada é uma tentativa só: os
	// outros fios que encostam no mesmo lugar (a 3 m) enquanto o religador ainda tenta são a mesma falta (4 juntos somavam
	// 4× a luz e viravam um borrão)
	if (Kind == 1 && WireBursts.ContainsByPredicate([&PointCm](const FWireBurst& B) { return B.Kind == 2 && FVector::DistSquared(B.PointCm, PointCm) < 300.0 * 300.0; }))
	{
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("RTS arco do fio: tipo %d em (%.1f, %.1f) m, %.1f m acima do chão, t %.2f s"), Kind, PointCm.X / 100.0, PointCm.Y / 100.0,
		(PointCm.Z - (State ? double(State->Baked.GroundZCm(FVector2D(PointCm.X, PointCm.Y))) : 0.0)) / 100.0, World->GetTimeSeconds());
	if (UBlastFxDirector* D = UBlastFxDirector::Get(this))
	{
		D->WireArc(PointCm, Kind, uint32(PointCm.X * 7.0 + PointCm.Y * 13.0));
	}
	if (Kind == 1)
	{
		const double Now = World->GetTimeSeconds();
		WireBursts.Add({ Now + 0.6, PointCm });
		WireBursts.Add({ Now + 2.2, PointCm });
		WireBursts.Add({ Now + 5.0, PointCm });   // (17/10, o Fluxo) a 3ª religada, antes de travar
	}
	if (Kind != 0 && GroundMemory)
	{
		GroundMemory->AddScar(FVector2D(PointCm.X, PointCm.Y), FVector2D(PointCm.X, PointCm.Y), 45.0f, 0.18f, 0.0f);
	}
}

void URTSMatchSubsystem::CheckPoles()
{
	if ((Poles.IsEmpty() && SmallProps.IsEmpty()) || !State)
	{
		return;
	}
	FRTSMatchState& S = *State;
	const Sim::SimMap& Map = S.World.Map();
	for (const TPair<uint32, Sim::UnitView>& It : S.Curr)
	{
		const Sim::UnitView& V = It.Value;
		const Sim::UnitDef& D = S.World.Rules().Units[V.Def];
		const Sim::UnitView* P = S.Prev.Find(It.Key);
		if (D.IsStructure() || D.CrusherLevel < 1 || !P || (P->Pos == V.Pos && P->Hull == V.Hull))
		{
			continue;   // o veículo se mexendo, como na árvore da simulação
		}
		const FVector2D C = RTS::ToWorld(V.Pos);
		const double Yaw = FMath::DegreesToRadians(double(RTS::YawDegrees(V.Hull)));
		const FVector2D Fwd(FMath::Cos(Yaw), FMath::Sin(Yaw)), Right(-Fwd.Y, Fwd.X);
		const double HL = D.HullHalfLength.ToDouble() * 100.0 + 15.0, HW = D.HullHalfWidth.ToDouble() * 100.0 + 15.0;
		// (16/10, com o WorldForge) as pedras pequenas partem sob o tanque e o Dozer (a do RockSmall; na grande ele sobe)
		if (D.CrusherLevel >= 2)
		{
			if (!bRocksCollected)
			{
				CollectRocks();
			}
			const FIntPoint G0(FMath::FloorToInt32(float(C.X / 1000.0)), FMath::FloorToInt32(float(C.Y / 1000.0)));
			for (int32 Gy = -1; Gy <= 1; ++Gy)
			{
				for (int32 Gx = -1; Gx <= 1; ++Gx)
				{
					const TArray<int32>* RockCell = RockGrid.Find(G0 + FIntPoint(Gx, Gy));
					if (!RockCell)
					{
						continue;
					}
					for (const int32 Ri : *RockCell)
					{
						if (Rocks[Ri].bBroken)
						{
							continue;
						}
						const FVector2D RockRel = Rocks[Ri].PosCm - C;
						if (FMath::Abs(FVector2D::DotProduct(RockRel, Fwd)) > HL - 15.0 || FMath::Abs(FVector2D::DotProduct(RockRel, Right)) > HW - 15.0)
						{
							continue;
						}
						BreakRock(Ri);
						if (ARTSUnitActor* Breaker = IsPointShown(Rocks[Ri].PosCm) ? ViewOf(It.Key) : nullptr)
						{
							Breaker->PlayBump(20.0f, FVector2D::DotProduct(RockRel, Right) > 0.0 ? -12.0f : 12.0f);
						}
					}
				}
			}
		}
		// (16/10) as peças pequenas amassam sob qualquer veículo que se mexe (o caminhão e o Dozer também)
		for (int32 K = 0; K < SmallProps.Num(); ++K)
		{
			FRTSSmallProp& Sp = SmallProps[K];
			const FVector2D Rel = Sp.PosCm - C;
			if (Sp.bCrushed || Rel.SizeSquared() > 900.0 * 900.0)
			{
				continue;
			}
			const double Pad = FMath::Max(Sp.RadiusCm * 0.5, 30.0);
			if (FMath::Abs(FVector2D::DotProduct(Rel, Fwd)) > HL - 15.0 + Pad || FMath::Abs(FVector2D::DotProduct(Rel, Right)) > HW - 15.0 + Pad)
			{
				continue;
			}
			Sp.bCrushed = true;
			const TPair<TWeakObjectPtr<UInstancedStaticMeshComponent>, int32>* Found = VillagePieceInstance.Find(Sp.Piece);
			UInstancedStaticMeshComponent* Ism = Found ? Found->Key.Get() : nullptr;
			FTransform T;
			UWorld* World = GetWorld();
			if (!World || !Ism || !Ism->GetInstanceTransform(Found->Value, T, /*bWorldSpace=*/true))
			{
				continue;
			}
			AActor* Proxy = World->SpawnActor<AActor>(AActor::StaticClass(), T);
			if (!Proxy)
			{
				continue;
			}
			UStaticMeshComponent* Cm = NewObject<UStaticMeshComponent>(Proxy, TEXT("Small"));
			Cm->SetMobility(EComponentMobility::Movable);
			UStaticMesh* Crushed = nullptr;
			if (UStaticMesh* Whole = Ism->GetStaticMesh())
			{
				const FString Name = Whole->GetName() + TEXT("_Crushed");
				Crushed = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Village/%s/StaticMeshes/%s.%s"), *Name, *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
			}
			Cm->SetStaticMesh(Crushed ? Crushed : Ism->GetStaticMesh().Get());
			for (int32 Mi = 0; Mi < Ism->GetNumMaterials(); ++Mi)
			{
				Cm->SetMaterial(Mi, Ism->GetMaterial(Mi));
			}
			Cm->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Cm->bReceivesDecals = false;
			Proxy->SetRootComponent(Cm);
			Cm->RegisterComponent();
			Proxy->SetActorTransform(T);
			FTransform Hidden = T;
			Hidden.SetScale3D(FVector(0.001));
			Ism->UpdateInstanceTransform(Found->Value, Hidden, /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);
			// o achatado pelo tipo, o tombo e a torção pelo sorteio da peça (o mesmo em todo PC)
			FRandomStream Rng(5381 + Sp.Piece);
			FRTSPropFx F;
			F.Prop = -100000 - K;
			F.Proxy = Proxy;
			F.Base = T;
			F.bTree = false;
			F.SquashTo = Crushed ? 1.0f : Sp.SquashTo;
			F.TiltTo = Crushed ? 0.0f : Rng.FRandRange(-9.0f, 9.0f);
			F.RollTo = Crushed ? 0.0f : Rng.FRandRange(-8.0f, 8.0f);
			if (!IsPointShown(Sp.PosCm))
			{
				F.Squash = F.SquashTo;
				F.Tilt = F.TiltTo;
				F.Roll = F.RollTo;
			}
			PropFx.Add(F);
		}
		if (D.CrusherLevel < 2 && !Sim::KnocksOver(D, D.MaxSpeed, Sim::PoleToppleEnergyJ))
		{
			// o poste cai pro tanque e pro Dozer (o nível do ZH) e, (v4, 23/10) pelo choque, pro caminhão e o Tomahawk (½·m·v² na
			// velocidade de cruzeiro, como a palmeira da simulação: o toque lento também derruba, nunca o casco dentro do poste)
			continue;
		}
		for (int32 K = 0; K < Poles.Num(); ++K)
		{
			FRTSPole& Pl = Poles[K];
			const FVector2D Rel = Pl.PosCm - C;
			if (Pl.bDown || Rel.SizeSquared() > 900.0 * 900.0)
			{
				continue;
			}
			const double A = FVector2D::DotProduct(Rel, Fwd), Sd = FVector2D::DotProduct(Rel, Right);
			if (FMath::Abs(A) > HL || FMath::Abs(Sd) > HW)
			{
				continue;
			}
			// a normal da face que bateu + 0,5 × a frente; o rumo livre de prédio (±22,5° ... ±90°) pela grade da simulação
			const bool bEnd = (FMath::Abs(A) - HL) > (FMath::Abs(Sd) - HW);
			const FVector2D Normal = bEnd ? (A >= 0.0 ? Fwd : -Fwd) : (Sd >= 0.0 ? Right : -Right);
			const FVector2D Want = (Normal + Fwd * 0.5).GetSafeNormal();
			FVector2D Dir = Want;
			for (const double Off : { 0.0, 22.5, -22.5, 45.0, -45.0, 67.5, -67.5, 90.0, -90.0 })
			{
				const FVector2D Try = Want.GetRotated(Off);
				bool bFree = true;
				for (double L = 100.0; L <= Pl.HeightM * 100.0 && bFree; L += 100.0)
				{
					const Sim::Cell Cc = Map.CellOf(RTS::ToSim(Pl.PosCm + Try * L));
					bFree = Map.PassAt(Cc.X, Cc.Y) != Sim::CellPass::Blocked;
				}
				if (bFree)
				{
					Dir = Try;
					break;
				}
			}
			Pl.bDown = true;
			TopplePole(K, Dir, float(V.Speed.ToDouble() * Sim::TicksPerSecond), IsPointShown(Pl.PosCm));
		}
	}
}

void URTSMatchSubsystem::TopplePole(int32 Pole, const FVector2D& DirXY, float SpeedMs, bool bShown)
{
	FRTSPole& Pl = Poles[Pole];
	UWorld* World = GetWorld();
	// o poste: um ator móvel no lugar da instância da vila, que cai pela vara (o mesmo do UpdatePropFx)
	const TPair<TWeakObjectPtr<UInstancedStaticMeshComponent>, int32>* Found = VillagePieceInstance.Find(Pl.Piece);
	UInstancedStaticMeshComponent* Ism = Found ? Found->Key.Get() : nullptr;
	FTransform T;
	if (World && Ism && Ism->GetInstanceTransform(Found->Value, T, /*bWorldSpace=*/true))
	{
		if (AActor* Proxy = World->SpawnActor<AActor>(AActor::StaticClass(), T))
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Proxy, TEXT("Pole"));
			C->SetMobility(EComponentMobility::Movable);
			C->SetStaticMesh(Ism->GetStaticMesh());
			for (int32 K = 0; K < Ism->GetNumMaterials(); ++K)
			{
				C->SetMaterial(K, Ism->GetMaterial(K));
			}
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->bReceivesDecals = false;
			Proxy->SetRootComponent(C);
			C->RegisterComponent();
			Proxy->SetActorTransform(T);
			FTransform Hidden = T;
			Hidden.SetScale3D(FVector(0.001));
			Ism->UpdateInstanceTransform(Found->Value, Hidden, /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);
			FRTSPropFx F;
			F.Prop = -1 - Pole;
			F.Proxy = Proxy;
			F.Base = T;
			F.bTree = true;
			F.Axis = FVector::CrossProduct(FVector::UpVector, FVector(DirXY, 0.0)).GetSafeNormal();
			F.Pivot = T.GetLocation();
			F.LengthM = FMath::Max(Pl.HeightM, 2.0f);
			F.MaxTheta = FMath::DegreesToRadians(87.0f);
			F.Theta = bShown ? 0.03f : F.MaxTheta;
			F.Omega = bShown ? 0.6f * SpeedMs / F.LengthM + 0.15f : 0.0f;
			PropFx.Add(F);
		}
	}
	// os fios: o vão vira fio solto (o RTSWireSystem do WorldForge: balança, arrebenta e deita no chão), a ponta deste
	// poste seguindo a cruzeta dele (UpdateWires); o arco sai na quebra e na ponta viva no chão (WireArc). Sem o plugin,
	// o vão some com a faísca (o provisório de antes)
	UInstancedStaticMeshComponent* Wires = WireLayer.Get();
	URTSWireSystem* WireSim = World ? World->GetSubsystem<URTSWireSystem>() : nullptr;
	if (WireSim && !bWireEventsBound)
	{
		bWireEventsBound = true;
		WireSim->SetGroundHeight([this](float X, float Y) { return State ? State->Baked.GroundZCm(FVector2D(X, Y)) : 0.0f; });
		WireSim->OnWireSnap.AddWeakLambda(this, [this](int32, const FVector& P) { WireArc(P, 0); });
		WireSim->OnWireGround.AddWeakLambda(this, [this](int32 H, const FVector& P)
		{
			if (!WireGrounded.Contains(H))
			{
				WireGrounded.Add(H);
				WireArc(P, 1);   // a 1ª vez que a ponta viva encosta: as rajadas do religador
			}
		});
	}
	FRandomStream Rng(9173 + Pole);
	// (17/10) o clarão no ar quando inclinar (o UpdateWires): só à vista e com algum vão ainda inteiro (o fio já solto pelo
	// outro poste não bate mais lá em cima)
	Pl.bAirArcPending = bShown && Pl.Wires.ContainsByPredicate([this](int32 Wi) { return WireFx.IsValidIndex(Wi) && !WireFx[Wi].bCut; });
	for (const int32 Wi : Pl.Wires)
	{
		FRTSWire& Wf = WireFx[Wi];
		if (Wf.bCut)
		{
			// o vão já solto e o outro poste dele caindo agora: essa ponta passa a seguir a cruzeta dele também
			if (Wf.Handle >= 0 && Wf.Follow2 < 0)
			{
				Wf.Follow2 = Wf.PoleA == Pole ? 0 : 1;
			}
			continue;
		}
		Wf.bCut = true;
		if (WireSim && bShown && Wires)
		{
			Wf.Follow = Wf.PoleA == Pole ? 0 : 1;
			Wf.Handle = WireSim->ReleaseSpan(Wf.EndA, Wf.EndB, Wf.SagCm / 100.0f, 2.25f, Wires->GetStaticMesh(), Wires->GetMaterial(0), Wf.Follow);
		}
		if (Wires)
		{
			for (int32 K = 0; K < Wf.Segments.Num(); ++K)
			{
				FTransform St;
				if (Wires->GetInstanceTransform(Wf.Segments[K], St, /*bWorldSpace=*/true))
				{
					St.SetScale3D(FVector(0.0001));
					Wires->UpdateInstanceTransform(Wf.Segments[K], St, /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/K + 1 == Wf.Segments.Num(), /*bTeleport=*/true);
				}
			}
		}
		if (!bShown || !World || Wf.Handle >= 0)
		{
			continue;   // o fio solto solta o arco quando arrebenta (o WireArc)
		}
		for (const TPair<int32, int32> End : { TPair<int32, int32>(Wf.PoleA, Wf.SocketA), TPair<int32, int32>(Wf.PoleB, Wf.SocketB) })
		{
			if (!Poles.IsValidIndex(End.Key) || !Poles[End.Key].Sockets.IsValidIndex(End.Value))
			{
				continue;
			}
			const FVector At = Poles[End.Key].Sockets[End.Value];
			ARTSEffect::SpawnFlash(World, At, 110.0f, 3.0e6f, 0.14f, FLinearColor(0.5f, 0.75f, 2.2f));
			for (int32 K = 0; K < 14; ++K)
			{
				const FVector Dir = (Rng.GetUnitVector() + FVector(0.0, 0.0, -0.6)).GetSafeNormal();
				ARTSEffect::SpawnStreak(World, At, At + Dir * Rng.FRandRange(150.0f, 420.0f), Rng.FRandRange(1500.0f, 3200.0f), 30.0f, 2.5f, FLinearColor(3.0f, 4.0f, 8.0f));
			}
		}
	}
}

void URTSMatchSubsystem::UpdateMissileSmoke(float DeltaSeconds)
{
	if (!MissileSmoke || !MissileSmoke->IsReady() || !State)
	{
		return;
	}
	FRTSMatchState& S = *State;
	FRTSSmokeFrame Frame;
	TArray<FRTSMissile> Flying;
	GetMissiles(Flying);
	for (const FRTSMissile& M : Flying)
	{
		FRTSSmokeMissile& O = Frame.Missiles.AddDefaulted_GetRef();
		O.Id = M.Id;
		O.PosCm = M.PosCm;
		O.Dir = M.Dir;
		O.SpeedCmS = M.SpeedCmS;
		O.AgeS = M.AgeS;
	}
	// (16/10) o diesel só perto do foco (a câmera do jogo vê ~100 m): o motor e as saídas de cada veículo à vista
	FVector2D SmokeFocus = FVector2D::ZeroVector;
	if (APlayerController* FocusPc = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (const ARTSCameraPawn* Cam = Cast<ARTSCameraPawn>(FocusPc->GetPawn()))
		{
			const FVector Fp = Cam->GetLookFocusPoint();
			SmokeFocus = FVector2D(Fp.X, Fp.Y);
		}
	}
	// os veículos à vista: o empurrão de quem anda e a leitura de todos (a fumaça na frente deles rareia); os prédios só
	// na leitura (a nuvem da largada não tapa a própria bateria)
	for (const TPair<uint32, Sim::UnitView>& It : S.Curr)
	{
		const Sim::UnitView& U = It.Value;
		if (!IsUnitShown(It.Key))
		{
			continue;
		}
		ARTSUnitActor* A = ViewOf(It.Key);
		if (!A)
		{
			continue;
		}
		const FVector At = A->GetActorLocation();
		Frame.UnitTopsCm.Add(At + FVector(0.0, 0.0, U.bStructure ? 400.0 : 250.0));
		if (U.bStructure)
		{
			continue;
		}
		const FRTSUnitVisual* Vis = S.Data.FindVisual(U.Def);
		if (Vis && Vis->ExhaustAtM.Num() && Vis->ExhaustPowerKw > 0.0f && !A->IsDead() && !A->IsFogHidden()
			&& FVector2D::Distance(FVector2D(At.X, At.Y), SmokeFocus) < 15000.0)
		{
			FRTSSmokeExhaust& X = Frame.Exhausts.AddDefaulted_GetRef();
			X.Id = It.Key;
			X.PosCm = At;
			X.FwdW = A->HullForward();
			X.SpeedMs = float(U.Speed.ToDouble() * Sim::TicksPerSecond);
			if (const Sim::UnitView* Was = S.Prev.Find(It.Key))
			{
				X.YawRateDps = FMath::FindDeltaAngleDegrees(RTS::YawDegrees(Was->Hull), RTS::YawDegrees(U.Hull)) * float(Sim::TicksPerSecond);
			}
			X.bWorking = (U.Task == Sim::UnitTask::Construct || U.Task == Sim::UnitTask::Repair) && X.SpeedMs < 0.3f;   // (v4) o conserto também força o motor
			const FTransform Bt = A->GetBodyTransform();
			for (int32 K = 0; K < Vis->ExhaustAtM.Num() && K < Vis->ExhaustDir.Num(); ++K)
			{
				X.Outlets.Add(TPair<FVector, FVector>(Bt.TransformPosition(Vis->ExhaustAtM[K] * 100.0), Bt.TransformVectorNoScale(Vis->ExhaustDir[K]).GetSafeNormal()));
			}
			X.PowerKw = Vis->ExhaustPowerKw;
			X.MassT = Vis->ExhaustMassT;
			X.Roll = Vis->ExhaustRoll;
			X.MaxSpeedMs = Vis->ExhaustMaxSpeedMs;
			X.TurnDps = Vis->ExhaustTurnDps;
			X.WorkLoad = Vis->ExhaustWorkLoad;
			X.Gears = Vis->ExhaustGears;
			X.JetSigmaM = Vis->ExhaustJetSigmaM;
			X.ExitMs = Vis->ExhaustExitMs;
		}
		if (const Sim::UnitView* Before = S.Prev.Find(It.Key))
		{
			const FVector2D V = (RTS::ToWorld(U.Pos) - RTS::ToWorld(Before->Pos)) * double(Sim::TicksPerSecond);
			if (V.SizeSquared() > 900.0)
			{
				FRTSSmokeMover& Mv = Frame.Movers.AddDefaulted_GetRef();
				Mv.PosCm = At;
				Mv.VelCmS = FVector(V.X, V.Y, 0.0);
				Mv.RadiusCm = 260.0f;
			}
		}
	}
	// os tiros de canhão em voo: o trecho do passo (o túnel; uma vez por passo da simulação)
	Frame.SimTick = int64(S.World.Tick());
	for (const TPair<uint32, Sim::World::ProjectileView>& It : S.CurrShots)
	{
		const Sim::World::ProjectileView* Before = S.PrevShots.Find(It.Key);
		if (It.Value.bMissile || !Before)
		{
			continue;
		}
		const FVector2D A2 = RTS::ToWorld(Before->Pos);
		const FVector2D B2 = RTS::ToWorld(It.Value.Pos);
		Frame.Shots.Add(TPair<FVector, FVector>(FVector(A2.X, A2.Y, S.Baked.GroundZCm(A2) + 180.0f), FVector(B2.X, B2.Y, S.Baked.GroundZCm(B2) + 180.0f)));
	}
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (PC->PlayerCameraManager)
		{
			const FRotationMatrix Rm(PC->PlayerCameraManager->GetCameraRotation());
			Frame.CamCm = PC->PlayerCameraManager->GetCameraLocation();
			Frame.CamForward = Rm.GetUnitAxis(EAxis::X);
			Frame.CamRight = Rm.GetUnitAxis(EAxis::Y);
			Frame.CamUp = Rm.GetUnitAxis(EAxis::Z);
		}
	}
	Frame.GroundZCm = [this](double X, double Y) { return State ? float(State->Baked.GroundZCm(FVector2D(X, Y))) : 0.0f; };
	if (HitLife)
	{
		HitLife->GetSmolders(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0, Frame.Smolders);   // (23/10) a fumaça do furo do acerto
	}
	MissileSmoke->Tick(DeltaSeconds, Frame);
}

void URTSMatchSubsystem::UpdateMissileFx()
{
	UBlastFxDirector* Fx = UBlastFxDirector::ShotFxEnabled() ? UBlastFxDirector::Get(this) : nullptr;
	if (!Fx)
	{
		return;
	}
	// a chama, a luz e o rastro do Fluxo andam com o míssil da simulação (interpolado no quadro; na névoa ele não vem e o
	// do Fluxo apaga sozinho em 1 s)
	TArray<FRTSMissile> Flying;
	GetMissiles(Flying);
	for (const FRTSMissile& M : Flying)
	{
		// (v4, 23/10) o míssil que entra na vista sem chama (o do ataque que nasceu na névoa ou fora da tela; o do Tomahawk
		// atirado de longe, de dentro da névoa): acende aqui, sem o clarão da caixa. O que já tem a chama (o lançamento à vista)
		// não muda
		Fx->MissileInFlight(int32(M.Id), M.PosCm, M.Dir, M.Id);
		Fx->MissileMove(int32(M.Id), M.PosCm, M.Dir, M.SpeedCmS);
	}
}

UTextureRenderTarget2D* URTSMatchSubsystem::GetBattleOverlay() const
{
	return GroundMemory ? GroundMemory->GetBattleOverlay() : nullptr;
}

void URTSMatchSubsystem::GetMissiles(TArray<FRTSMissile>& Out) const
{
	Out.Reset();
	if (!State)
	{
		return;
	}
	const float Alpha = float(FMath::Clamp(Accumulator / StepSeconds, 0.0, 1.0));
	for (const TPair<uint32, Sim::World::ProjectileView>& It : State->CurrShots)
	{
		const Sim::World::ProjectileView& C = It.Value;
		if (!C.bMissile || !IsPointShown(RTS::ToWorld(C.Pos)))
		{
			continue;
		}
		const Sim::World::ProjectileView* P = State->PrevShots.Find(It.Key);
		const Sim::World::ProjectileView& From = P ? *P : C;
		FRTSMissile M;
		M.Id = C.Id;
		M.Shooter = C.Shooter;
		M.PosCm = FMath::Lerp(MissileCm(*State, From), MissileCm(*State, C), double(Alpha));
		M.Dir = FMath::Lerp(MissileDir(From), MissileDir(C), double(Alpha)).GetSafeNormal();
		M.SpeedCmS = float(FMath::Lerp(From.Speed.ToDouble(), C.Speed.ToDouble(), double(Alpha)) * Sim::TicksPerSecond * 100.0);
		M.AgeS = (float(C.Age) - 1.0f + Alpha) / float(Sim::TicksPerSecond);
		// (23/10, v4) o voo novo do lançador que levanta sai da boca desenhada: a diferença dela pro ponto da simulação (zero no chão
		// plano; no inclinado o casco desenhado inclina) some em 0,6 s e a direção vai do tubo pra do voo (só desenho)
		if (M.AgeS < ARTSUnitActor::LaunchBlendSeconds)
		{
			const TObjectPtr<ARTSUnitActor>* Shooter = Views.Find(C.Shooter);
			FVector OffsetCm, TubeDir;
			if (Shooter && *Shooter && (*Shooter)->GetLaunchBlend(C.Id, OffsetCm, TubeDir))
			{
				const float K = FMath::SmoothStep(0.0f, ARTSUnitActor::LaunchBlendSeconds, FMath::Max(M.AgeS, 0.0f));
				M.PosCm += OffsetCm * double(1.0f - K);
				M.Dir = FMath::Lerp(TubeDir, M.Dir, double(K)).GetSafeNormal();
			}
		}
		Out.Add(M);
	}
}

void URTSMatchSubsystem::UpdateWreckPoses()
{
	// a carcaça com física (13/10): o modelo, o fogo e as brasas (presos no ator) vão pra onde a simulação a empurrou,
	// interpolados entre os passos; a primeira vez guarda a base (o ator já está no lugar da morte, tombado).
	// (22/10) E o afundar: o quanto afundou sai da simulação (o passo em que começou, entre o passo anterior e o atual como
	// a posição); o fantasma da névoa (já visto, fora da vista agora) fica parado como foi visto
	if (!State)
	{
		return;
	}
	const float Alpha = float(FMath::Clamp(Accumulator / StepSeconds, 0.0, 1.0));
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const int64 SimNow = int64(State->World.Tick());
	ARTSDebrisField* Field = Debris.Get();
	TArray<uint32> Gone;
	for (TPair<uint32, FWreckPose>& It : WreckPoses)
	{
		FWreckPose& P = It.Value;
		ARTSUnitActor* A = P.Actor.Get();
		if (!A)
		{
			Gone.Add(It.Key);
			continue;
		}
		const bool bFrozen = A->bWreckKnown && !A->IsWreckLive();
		const auto Sink = [&P, A, Field, &It](float Target)
		{
			if (Target > P.SinkP)
			{
				P.SinkP = Target;
				A->SetWreckSink(Target);
				if (Field)
				{
					Field->SinkWreck(It.Key, Target);   // as peças grandes dos destroços descem junto
				}
			}
		};
		if (P.RemovedAt >= 0.0)
		{
			// saiu da simulação: termina de afundar e some; a nunca vista some já; o fantasma espera o lugar ser visto
			if (P.bGhost)
			{
				continue;
			}
			if (!A->bWreckKnown)
			{
				Gone.Add(It.Key);
				continue;
			}
			if (bFrozen)
			{
				P.bGhost = true;
				WreckGhosts.Add(It.Key);
				continue;
			}
			if (P.SinkP <= 0.0f && A->IsLooseTurretMoving())
			{
				// (v4, 23/10) a torre solta ainda no ar ou rolando (a tirada ~3 s depois da morte, na onda do derrotado): o afundar
				// espera ela parar e as duas descem juntas, como na v2; senão ela sumia rolando ou parava com o casco quase todo
				// afundado e descia ~4,5 m em meio segundo
				P.RemovedAt = FMath::Max(P.RemovedAt, Now);
				continue;
			}
			Sink(FMath::Min(P.RemovedFromP + float(FMath::Max(Now - P.RemovedAt, 0.0) / WreckFinishSinkS), 1.0f));
			if (P.SinkP >= 1.0f)
			{
				Gone.Add(It.Key);
			}
			continue;
		}
		const Sim::World::WreckView* C = State->CurrWrecks.Find(It.Key);
		if (!C)
		{
			continue;
		}
		if (C->SinkStart > 0 && !bFrozen)
		{
			// no passo T o afundar está em (T − SinkStart) ÷ SinkTicks; o desenho fica entre o passo anterior e o atual, e o
			// World.Tick() já é o atual + 1
			const float Steps = float(SimNow - 2 - int64(C->SinkStart)) + Alpha;
			Sink(FMath::Clamp(Steps / float(FMath::Max(C->SinkTicks, 1)), 0.0f, 1.0f));
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
			GroundMemory->TrackUnit(It.Key | WreckTrackBit, Cm, Yaw, float(Tracks.X), float(Tracks.Y), !A->IsFogHidden() && IsPointShown(Cm));
		}
		P.LastCm = Cm;
		P.LastYaw = Yaw;
	}
	for (const uint32 Id : Gone)
	{
		FinishWreck(Id);
	}
	// os fantasmas da névoa: no máximo 32 (o mais velho some sem aviso)
	while (WreckGhosts.Num() > MaxWreckGhosts)
	{
		FinishWreck(WreckGhosts[0]);
	}
}

void URTSMatchSubsystem::FinishWreck(uint32 Id)
{
	// (22/10) a carcaça foi embora: sai das listas (o mapa do sol, a névoa, o empurrar), a esteira de quando foi arrastada
	// sai da memória do chão, as peças grandes dos destroços vão junto e o ator é destruído (o fogo dela, se ainda houver,
	// some com ele no BlastFx). O queimado no chão e as lascas ficam: são o histórico da batalha
	WreckGhosts.Remove(Id);
	FWreckPose Pose;
	if (!WreckPoses.RemoveAndCopyValue(Id, Pose))
	{
		return;
	}
	if (GroundMemory)
	{
		GroundMemory->ForgetUnit(Id | WreckTrackBit);
	}
	if (ARTSDebrisField* D = Debris.Get())
	{
		D->RemoveWreck(Id);
	}
	if (ARTSUnitActor* A = Pose.Actor.Get())
	{
		Wrecks.Remove(A);
		A->Destroy();
	}
}

void URTSMatchSubsystem::OnWreckSinking(const Sim::SimEvent& E)
{
	FWreckPose* P = WreckPoses.Find(E.Target);
	ARTSUnitActor* A = P ? P->Actor.Get() : nullptr;
	if (!A)
	{
		return;
	}
	// à vista: o pulso de poeira no pé do casco (a areia que ele empurra ao assentar)
	const FVector L = A->GetActorLocation();
	const FVector2D Cm(L.X, L.Y);
	if (A->bWreckKnown && IsPointShown(Cm))
	{
		PushDust(TreadDust.Get(), FVector(Cm.X, Cm.Y, 0.0), State->Baked.GroundZCm(Cm), WreckSinkDustScale);
	}
	// o fogo que ainda queima (a afundada pelo teto de sólidas, de 12 s pra cima) apaga enquanto ela desce
	if (UBlastFxDirector* Director = UBlastFxDirector::Get(this))
	{
		Director->WreckGone(A, WreckFireFadeS);
	}
	UE_LOG(LogTemp, Log, TEXT("RTS carcaça %u afundando (%s)"), E.Target, (E.Value >> 16) == 1 ? TEXT("sólidas demais no mapa") : TEXT("a vida acabou"));
}

void URTSMatchSubsystem::TurretImpact(const FVector& AtCm, const FVector2D& SlideDir, float NormalSpeedCmS, float SpreadCm, bool bWreckShown, float TangentSpeedCmS)
{
	// (v4, 23/10) a torre solta batendo no chão. O empurrão pela lei do próprio AddBlast (W = 36·S³ kg de TNT): a energia da
	// batida, ½·m·v² com a torre de 15 t (12–20 t: a raiz cúbica muda só ±9%), em kg de TNT (4,184 MJ/kg): 13 m/s → 0,20,
	// 5 m/s → 0,11, 2 m/s → 0,06 (o alcance de 1 cm do plugin corta as fracas sozinho). Ele só empurra a poeira das lagartas
	// que já está no ar: o sopro em volume da batida é da fase 2 (no BlastFx). Só chega aqui batida de carcaça conhecida: a de
	// uma morte que ninguém viu espera no ator até a carcaça aparecer e vem com bWreckShown falso (só a marca, sem a poeira)
	if (!State)
	{
		return;
	}
	constexpr double TurretMassKg = 15000.0;
	const FVector2D At(AtCm.X, AtCm.Y);
	const double SpeedMs = double(NormalSpeedCmS) / 100.0;
	const double Scale = FMath::Pow(0.5 * TurretMassKg * SpeedMs * SpeedMs / 4.184e6 / 36.0, 1.0 / 3.0);
	// a marca: o queimado fraco (0,25, o do carro esmagado, acima de 5 m/s; senão 0,12, o da pedra quebrada) esticado pra onde
	// ela ia, do tamanho dos pontos que bateram; sem cratera (a bacia baixaria o chão sob a torre parada e ela flutuaria)
	const FVector2D From = At - SlideDir * 200.0;
	const float Radius = FMath::Clamp(SpreadCm, 60.0f, 250.0f);
	const float Scorch = SpeedMs >= 5.0 ? 0.25f : 0.12f;
	if (IsPointShown(At))
	{
		if (bWreckShown && State->Baked.SurfaceAt(At).W < 0.5f)
		{
			PushDust(TreadDust.Get(), FVector(At.X, At.Y, 0.0), State->Baked.GroundZCm(At), Scale);   // (no molhado a areia não sobe)
		}
		if (bWreckShown && TreadDust.Get())
		{
			// (23/10, a areia do acerto, caso 3) o respingo de areia da batida, pela energia dela (rts.Turret.Sand; RTSHitLife.cpp)
			if (!HitLife)
			{
				HitLife = new FRTSHitLife();
			}
			FRTSHitLifeCtx Ctx;
			FillHitLifeCtx(Ctx);
			HitLife->OnTurretImpact(AtCm, SlideDir, NormalSpeedCmS, TangentSpeedCmS, SpreadCm, Ctx);
		}
		if (GroundMemory)
		{
			GroundMemory->AddScar(At, From, Radius, Scorch, 0.0f);
		}
	}
	else if (GroundMemory)
	{
		HiddenScars.Add({ At, From, Radius, Scorch, 0.0f, 1 });
	}
}

void URTSMatchSubsystem::TurretScrape(const FVector& AtCm, const FVector2D& Dir, float SpeedCmS, int32 Index)
{
	// (23/10, a areia do acerto, caso 3) a torre deslizando ou rolando na areia à vista: a poeira da raspada (RTSHitLife.cpp)
	if (!State || !TreadDust.Get() || !IsPointShown(FVector2D(AtCm.X, AtCm.Y)))
	{
		return;
	}
	if (!HitLife)
	{
		HitLife = new FRTSHitLife();
	}
	FRTSHitLifeCtx Ctx;
	FillHitLifeCtx(Ctx);
	HitLife->OnTurretScrape(AtCm, Dir, SpeedCmS, Index, Ctx);
}

void URTSMatchSubsystem::OnWreckRemoved(const Sim::SimEvent& E)
{
	FWreckPose* P = WreckPoses.Find(E.Target);
	if (!P)
	{
		// ainda na onda do derrotado (a morte desenhada vem depois): afunda logo depois dela (Die)
		if (DyingInfo.ContainsByPredicate([&E](const FDyingInfo& D) { return D.Id == E.Target; }))
		{
			WreckGoneEarly.Add(E.Target);
		}
		return;
	}
	if (P->RemovedAt < 0.0)
	{
		P->RemovedAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		P->RemovedFromP = P->SinkP;
	}
	if (E.Value == 1)
	{
		// a do teto saiu ainda inteira: o fogo apaga antes de ela terminar de descer
		if (UBlastFxDirector* Director = UBlastFxDirector::Get(this))
		{
			if (ARTSUnitActor* A = P->Actor.Get())
			{
				Director->WreckGone(A, float(WreckFinishSinkS) * 0.5f);
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("RTS carcaça %u saiu da simulação (%s)"), E.Target, E.Value == 1 ? TEXT("o teto de 64") : TEXT("afundou"));
}

void URTSMatchSubsystem::RevealHiddenMarks()
{
	// duas vezes por segundo: a morte que estava na névoa ganha o avental e os destroços quando a área aparece; (22/10) e o
	// fantasma da carcaça que a simulação já tirou some quando o lugar dele é visto de novo
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if ((HiddenMarks.IsEmpty() && HiddenScars.IsEmpty() && WreckGhosts.IsEmpty()) || Now < NextHiddenCheck)
	{
		return;
	}
	NextHiddenCheck = Now + 0.5;
	for (int32 K = WreckGhosts.Num() - 1; K >= 0; --K)
	{
		const FWreckPose* P = WreckPoses.Find(WreckGhosts[K]);
		const ARTSUnitActor* A = P ? P->Actor.Get() : nullptr;
		if (!A || IsPointShown(FVector2D(A->GetActorLocation().X, A->GetActorLocation().Y)))
		{
			FinishWreck(WreckGhosts[K]);
		}
	}
	if (!GroundMemory)
	{
		return;
	}
	for (int32 K = HiddenScars.Num() - 1; K >= 0; --K)
	{
		const FHiddenScar& H = HiddenScars[K];
		if (IsPointShown(H.At))
		{
			GroundMemory->AddScar(H.At, H.From, H.RadiusCm, H.Scorch, H.Crater, H.Size);
			HiddenScars.RemoveAtSwap(K);
		}
	}
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
			D->AddWreck(H.Cm, H.YawDeg, H.From, [this](const FVector2D& P) { return State ? State->Baked.GroundZCm(P) : 0.0f; }, H.Id);
			if (!WreckPoses.Contains(H.Id))
			{
				D->RemoveWreck(H.Id);   // (22/10) a carcaça já foi embora antes de alguém ver: só as lascas ficam
			}
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
	// (17/10, o Fluxo) cada explosão da morte do prédio empurra a poeira: a Carga Pesada a 0,55 por chamada
	PushDust(TreadDust.Get(), FVector(CenterCm.X, CenterCm.Y, 0.0), State->Baked.GroundZCm(CenterCm), 0.55);
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
	// a máscara da poeira em toda unidade viva (14/10, a Arte); -RTSNoUnitMask = o A/B do custo do custom depth
	static const bool bNoMask = FParse::Param(FCommandLine::Get(), TEXT("RTSNoUnitMask"));
	Actor->SetUnitMask(!bNoMask);
	Views.Add(Id, Actor);
}

void URTSMatchSubsystem::BuildXRayGrid()
{
	bXRayGridBuilt = true;
	if (!State)
	{
		return;
	}
	const Sim::SimMap& M = State->World.Map();
	XRayW = M.Width;
	XRayH = M.Height;
	XRayMinCm = RTS::ToWorld(M.MinCorner());
	XRayCellCm = float(M.CellSize.ToDouble() * 100.0);
	const int32 N = XRayW * XRayH;
	XRaySolidTop.SetNumZeroed(N);
	XRayCanopyLo.SetNumZeroed(N);
	XRayCanopyHi.SetNumZeroed(N);
	XRayMaxTopCm = -1.0e9f;
	// o relevo: a maior altura da célula (as 4 quinas e o meio)
	for (int32 Y = 0; Y < XRayH; ++Y)
	{
		for (int32 X = 0; X < XRayW; ++X)
		{
			float Top = -1.0e9f;
			for (const FVector2D& O : { FVector2D(0.0, 0.0), FVector2D(1.0, 0.0), FVector2D(0.0, 1.0), FVector2D(1.0, 1.0), FVector2D(0.5, 0.5) })
			{
				Top = FMath::Max(Top, State->Baked.GroundZCm(XRayMinCm + (FVector2D(X, Y) + O) * XRayCellCm));
			}
			XRaySolidTop[Y * XRayW + X] = Top;
			XRayMaxTopCm = FMath::Max(XRayMaxTopCm, Top);
		}
	}
	// as casas e os muros da vila
	for (const FXRayBlock& B : XRayVillage)
	{
		for (int32 Y = FMath::Max(B.Cells.Min.Y, 0); Y < FMath::Min(B.Cells.Max.Y, XRayH); ++Y)
		{
			for (int32 X = FMath::Max(B.Cells.Min.X, 0); X < FMath::Min(B.Cells.Max.X, XRayW); ++X)
			{
				float& T = XRaySolidTop[Y * XRayW + X];
				T = FMath::Max(T, B.TopCm);
				XRayMaxTopCm = FMath::Max(XRayMaxTopCm, T);
			}
		}
	}
	// as copas das palmeiras (as de pé; a caída sai na medida): de 55% a 100% da altura, num raio de ~3 m
	for (int32 K = 0; K < State->World.PropCount(); ++K)
	{
		const Sim::World::PropView P = State->World.PropAt(K);
		if (P.Kind != Sim::PropKind::Tree)
		{
			continue;
		}
		const FVector2D C = RTS::ToWorld(P.Pos);
		const float Ground = State->Baked.GroundZCm(C);
		const float H = float(P.Height.ToDouble() * 100.0);
		const int32 X0 = FMath::FloorToInt((C.X - 300.0 - XRayMinCm.X) / XRayCellCm), X1 = FMath::FloorToInt((C.X + 300.0 - XRayMinCm.X) / XRayCellCm);
		const int32 Y0 = FMath::FloorToInt((C.Y - 300.0 - XRayMinCm.Y) / XRayCellCm), Y1 = FMath::FloorToInt((C.Y + 300.0 - XRayMinCm.Y) / XRayCellCm);
		for (int32 Y = FMath::Max(Y0, 0); Y <= FMath::Min(Y1, XRayH - 1); ++Y)
		{
			for (int32 X = FMath::Max(X0, 0); X <= FMath::Min(X1, XRayW - 1); ++X)
			{
				XRayCanopyLo[Y * XRayW + X] = Ground + 0.55f * H;
				XRayCanopyHi[Y * XRayW + X] = FMath::Max(XRayCanopyHi[Y * XRayW + X], Ground + H);
				XRayMaxTopCm = FMath::Max(XRayMaxTopCm, Ground + H);
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("RTS raio-X: grade de %d × %d, %d peças da vila, o mais alto a %.0f m (o contorno só com ≥ 70%% escondido)"), XRayW, XRayH, XRayVillage.Num(), XRayMaxTopCm / 100.0f);
}

void URTSMatchSubsystem::UpdateXRay(float DeltaSeconds)
{
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("RTSNoSilhouette"));
	XRayClock += DeltaSeconds;
	if (bOff || !State || XRayClock < 0.12f)
	{
		return;
	}
	XRayClock = 0.0f;
	if (!bXRayGridBuilt)
	{
		BuildXRayGrid();
	}
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->PlayerCameraManager || XRaySolidTop.IsEmpty())
	{
		return;
	}
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
	// os prédios da partida tapam como bloco de ~8 m (o pé deles na grade)
	TMap<int32, float> Buildings;
	for (const TPair<uint32, Sim::UnitView>& It : State->Curr)
	{
		const Sim::UnitView& V = It.Value;
		if (!V.bStructure)
		{
			continue;
		}
		int32 X0, Y0, X1, Y1;
		State->World.FootprintCells(V.Def, V.Dir, V.Pos, X0, Y0, X1, Y1);
		for (int32 Y = FMath::Max(Y0, 0); Y < FMath::Min(Y1, XRayH); ++Y)
		{
			for (int32 X = FMath::Max(X0, 0); X < FMath::Min(X1, XRayW); ++X)
			{
				Buildings.Add(Y * XRayW + X, XRaySolidTop[Y * XRayW + X] + 800.0f);
			}
		}
	}
	const float MaxTop = XRayMaxTopCm + (Buildings.IsEmpty() ? 0.0f : 800.0f);
	const auto Blocked = [&](const FVector& P)
	{
		// do ponto do casco pra câmera, de 1,5 em 1,5 m, até passar de tudo o que tapa
		const FVector D = Eye - P;
		const double L = D.Size();
		if (L < 1.0)
		{
			return false;
		}
		const FVector U = D / L;
		for (double S = 120.0; S < FMath::Min(L, 30000.0); S += 150.0)
		{
			const FVector Q = P + U * S;
			if (Q.Z > MaxTop)
			{
				return false;
			}
			const int32 X = FMath::FloorToInt((Q.X - XRayMinCm.X) / XRayCellCm), Y = FMath::FloorToInt((Q.Y - XRayMinCm.Y) / XRayCellCm);
			if (X < 0 || Y < 0 || X >= XRayW || Y >= XRayH)
			{
				continue;
			}
			const int32 K = Y * XRayW + X;
			const float* Bt = Buildings.Find(K);
			if (Q.Z < (Bt ? *Bt : XRaySolidTop[K]) - 15.0f || (XRayCanopyHi[K] > 0.0f && Q.Z > XRayCanopyLo[K] && Q.Z < XRayCanopyHi[K]))
			{
				return true;
			}
		}
		return false;
	};
	for (const TPair<uint32, TObjectPtr<ARTSUnitActor>>& It : Views)
	{
		ARTSUnitActor* A = It.Value.Get();
		const Sim::UnitView* V = State->Curr.Find(It.Key);
		if (!A || !V || V->bStructure || IsEnemyOfLocal(V->Owner))
		{
			continue;
		}
		const Sim::UnitDef& D = State->World.Rules().Units[V->Def];
		const FVector Base = A->GetActorLocation();
		const FVector Fwd = A->GetActorForwardVector(), Right = A->GetActorRightVector();
		const double HL = D.HullHalfLength.ToDouble() * 85.0, HW = D.HullHalfWidth.ToDouble() * 80.0;
		const FVector Pts[9] = {
			Base + FVector(0, 0, 120), Base + FVector(0, 0, 240),
			Base + Fwd * HL + Right * HW + FVector(0, 0, 100), Base + Fwd * HL - Right * HW + FVector(0, 0, 100),
			Base - Fwd * HL + Right * HW + FVector(0, 0, 100), Base - Fwd * HL - Right * HW + FVector(0, 0, 100),
			Base + Fwd * HL + FVector(0, 0, 180), Base - Fwd * HL + FVector(0, 0, 180), Base + Right * HW + FVector(0, 0, 180) };
		int32 Hidden = 0;
		for (const FVector& P : Pts)
		{
			Hidden += Blocked(P) ? 1 : 0;
		}
		const float Frac = float(Hidden) / 9.0f;
		// liga com ≥ 70% escondido, desliga abaixo de 45% (a borda não pisca)
		A->SetXRay(A->IsXRayOn() ? Frac >= 0.45f : Frac >= 0.7f);
	}
}

void URTSMatchSubsystem::UpdateViews(float DeltaTime)
{
	RevealHiddenMarks();
	UpdateXRay(DeltaTime);
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
				// (v4) a carga do prédio (a zona de lançamento: os paletes que o avião pousou no pátio, palete por palete até a
				// empilhadeira levar): a do inimigo só muda à vista (o fantasma da névoa fica com o pátio que foi visto)
				if (!bFog || !IsEnemyOfLocal(U.Owner) || (Fog && Fog->IsVisibleNow(U.PosCm)))
				{
					It.Value->SetCargo(U.Cargo);
				}
			}
			else
			{
				It.Value->SetCargo(U.Cargo);
				// (v4) o conserto usa a lida da obra (o ZH empresta o ACTIVELY_CONSTRUCTING): a lâmina e a sirene
				It.Value->SetWorking((U.Task == uint8(Sim::UnitTask::Construct) || U.Task == uint8(Sim::UnitTask::Repair)) && !U.bMoving);
			}
			if (!bFog || !IsEnemyOfLocal(U.Owner) || (Fog && Fog->IsVisibleNow(U.PosCm)))
			{
				It.Value->LastSeenSeconds = Now;
			}
			// o ZH segue mostrando o objeto um instante depois de sair do claro (GameClient: 2 s); o
			// prédio inimigo já visto fica como fantasma, no lugar onde foi visto (ZH)
			const bool bKnown = It.Value->LastSeenSeconds > -1.0e8;
			It.Value->SetFogHidden(bFog && (U.bStructure ? !bKnown : Now - It.Value->LastSeenSeconds > FogGraceSeconds));
			FVector2D Tracks = It.Value->GetTracksCm();
			// (23/10, v4) o pneu também risca: mais claro, raso, estreito e de garra miúda (o bloco "wheels" do Units.json)
			URTSGroundMemory::FTrackStyle MarkStyle;
			if (Tracks.X <= 0.0)
			{
				Tracks = It.Value->GetTyreMarksCm(MarkStyle.Dark, MarkStyle.Depth, MarkStyle.WidthK, MarkStyle.LugK, MarkStyle.SegmentK);
			}
			if (GroundMemory && !U.bStructure && Tracks.X > 0.0)
			{
				// a lagarta risca o chão onde o jogador vê (a de dentro da névoa não); o giro de nascimento (a ordem "line" antes
				// da partida) não risca: não aconteceu na história do jogador (o retângulo de borracha do raiox_atras_alta1)
				const bool bSpawnTurn = MatchSeconds() < 1.0f;
				GroundMemory->TrackUnit(It.Key, U.PosCm, U.HullYawDeg, float(Tracks.X), float(Tracks.Y), !It.Value->IsFogHidden() && IsPointShown(U.PosCm) && !bSpawnTurn,
					MarkStyle);
			}
			// (c) (23/10, v4) o lançador que levanta (o Tomahawk): o relógio da mira da simulação em segundos (AimFraction × o pre-attack: o
			// tubo sobe por ele e o tiro sai com ele cheio; caindo, desce junto) e, de reserva sem o relógio, mirando alguém (a simulação só
			// mira quem está no alcance, fora do mínimo) ou o tiro no chão no alcance, a mesma regra dele; e a arma sem espera (com a rajada
			// toda atirada, a recarga pronta: as pontas dos mísseis voltam nos tubos). Só desenho
			if (It.Value->HasLauncher())
			{
				bool bAim = false, bWeaponReady = true;
				float AimS = 0.0f, PreAttackS = 0.0f;
				if (const Sim::UnitView* Sv = State->Curr.Find(It.Key))
				{
					const Sim::WeaponDef& W = State->World.Rules().Units[Sv->Def].Weapon;
					const int64 D2 = Sim::DistSqRaw(Sv->Pos, Sv->OrderPoint);
					bAim = Sv->FireTarget != Sim::NoEntity
						|| (Sv->Order == Sim::OrderType::AttackGround && D2 <= Sim::RadiusSqRaw(W.Range) && D2 >= Sim::RadiusSqRaw(W.MinRange));
					PreAttackS = float(W.PreAttackTicks) / float(Sim::TicksPerSecond);
					AimS = U.AimFraction * PreAttackS;
					bWeaponReady = Sv->Cooldown <= 0;
				}
				It.Value->SetLauncherAim(bAim, AimS, PreAttackS, bWeaponReady);
			}
		}
	}
	UpdateDocks();
	UpdateTreadDust(DeltaTime);
	UpdateSunShadow();
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
					PushDust(TreadDust.Get(), PendingBlasts[K].Ground, PendingBlasts[K].Ground.Z, 0.55);
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
	UpdatePropFx(DeltaTime);
	UpdateWires();
	UpdateCarPasses(DeltaTime);
	UpdateMissileFx();
	if (HitLife)
	{
		// (23/10, a explosão com vida) a fila de pousos, os tremores atrasados e os pontos que fumegam
		FRTSHitLifeCtx HitCtx;
		FillHitLifeCtx(HitCtx);
		HitLife->Tick(HitCtx);
	}
	UpdateMissileSmoke(DeltaTime);
	{
		// (v4, 23/10) o anel do ataque e o avião de carga, no relógio do desenho (a pausa congela os dois)
		FRTSMatchState& S = *State;
		const double NowTicks = DisplayTicks(S, Accumulator);
		S.StrikeRings.Tick(NowTicks);
		const Sim::SimMap& Map = S.World.Map();
		const FBox2D MapCm(RTS::ToWorld(Map.MinCorner()), RTS::ToWorld(Map.MaxCorner()));
		S.Airlift.Tick(NowTicks,
			[this, &S](uint32 Id, FRTSAirliftZone& Out) { return AirliftZoneOf(S, ViewOf(Id), Id, Out); },
			[this](const FVector2D& P) { return IsPointShown(P); }, MapCm);
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
	// (v4, 23/10) vender: a desmontagem do ZH na conta da obra ao contrário (o mesmo corte, andaime e solda do desenho da obra):
	// o 1º quarto só o andaime (os 1,5 s de FRAMES_TO_ALLOW_SCAFFOLD), a metade do meio desmonta de cima pra baixo (os 3 s até 0%)
	// e o último quarto fica a laje (até −50%); entre os passos, interpolada como a posição
	if (C->SellTicks > 0)
	{
		const auto Done = [](const Sim::UnitView& V) { return V.SellTicks > 0 ? 1.0f - float(V.SellLeft) / float(V.SellTicks) : 0.0f; };
		const float SellDone = FMath::Lerp(P->SellTicks > 0 ? Done(*P) : Done(*C), Done(*C), Alpha);
		Out.bSelling = true;
		Out.SellFraction = FMath::Clamp(SellDone, 0.0f, 1.0f);
		Out.BuildFraction = FMath::Min(0.999f, 1.0f - FMath::Clamp((Out.SellFraction - 0.25f) / 0.5f, 0.0f, 1.0f));
	}
	// (v4) a mira antes do tiro (o lançador que levanta): o relógio da simulação, entre os passos
	{
		const Sim::UnitDef& D = State->World.Rules().Units[C->Def];
		const int32 Need = D.Weapon.PreAttackTicks;
		Out.AimTicks = C->AimTicks;
		Out.AimFraction = Need > 0 ? FMath::Clamp(FMath::Lerp(float(P->AimTicks), float(C->AimTicks), Alpha) / float(Need), 0.0f, 1.0f) : 0.0f;
	}
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
	// (v4, 22/10) a patente, o teto por jogador e o avião de carga
	Out.RequiresRank = D.RequiresRank;
	Out.MaxPerPlayer = D.MaxPerPlayer;
	Out.AirliftPeriodS = float(D.AirliftPeriodTicks) / float(Sim::TicksPerSecond);
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
	case ERTSPlace::LimitReached: return NSLOCTEXT("RTSHUD", "PlaceLimitReached", "Limit reached").ToString();   // (v4) o HUD põe o teto, "(2)"
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

// ---------------------------------------------------------------- (v4, 23/10) vender e consertar

void URTSMatchSubsystem::OrderSell(const TArray<uint32>& Structures)
{
	if (!CanOrder(State, bReady, MatchResult) || IsLocalDefeated())
	{
		return;
	}
	for (const uint32 Id : Structures)
	{
		if (!CanSell(Id))
		{
			continue;
		}
		Sim::Command C;
		C.Player = LocalPlayer();
		C.Type = Sim::CommandType::Sell;
		C.Target = Id;
		Issue(C);
		UE_LOG(LogTemp, Log, TEXT("RTS vender: o jogador %d vende o prédio %u ($%d de volta) no passo %u"), int32(LocalPlayer()), Id, SellRefund(Id), unsigned(State->World.Tick()));
	}
}

void URTSMatchSubsystem::OrderRepair(const TArray<uint32>& Dozers, uint32 Structure)
{
	if (!CanOrder(State, bReady, MatchResult) || Dozers.IsEmpty() || !CanRepair(Structure))
	{
		return;
	}
	IssueGroupIfChanged(Dozers);
	Sim::Command C;
	C.Player = LocalPlayer();
	C.Type = Sim::CommandType::Repair;
	C.Target = Structure;
	Issue(C);
	UE_LOG(LogTemp, Log, TEXT("RTS consertar: o jogador %d manda %d Dozer(s) consertar o prédio %u no passo %u"), int32(LocalPlayer()), Dozers.Num(), Structure, unsigned(State->World.Tick()));
}

bool URTSMatchSubsystem::CanSell(uint32 Structure) const
{
	return State != nullptr && State->World.CanSell(LocalPlayer(), Structure);
}

int32 URTSMatchSubsystem::SellRefund(uint32 Structure) const
{
	return State != nullptr ? State->World.SellRefund(Structure) : 0;
}

bool URTSMatchSubsystem::CanRepair(uint32 Structure) const
{
	return State != nullptr && State->World.CanRepair(LocalPlayer(), Structure);
}

// ---------------------------------------------------------------- (v4, 23/10) a patente e os poderes do general
// O bloco da frente do HUD: o pedido do poder (UsePower pela fila, como as outras ordens), o que o HUD lê (a patente, o
// relógio de cada poder, o avião da zona de lançamento) e os avisos que nascem dos eventos (HandleHudEvents, chamado logo
// depois do HandleEvents). Nada aqui decide: a simulação confere tudo de novo no passo.

void URTSMatchSubsystem::OrderUsePower(int32 Power, const FVector2D& PointCm)
{
	if (!CanOrder(State, bReady, MatchResult) || IsLocalDefeated() || Power < 0 || Power > 0xFF)
	{
		return;
	}
	Sim::Command C;
	C.Player = LocalPlayer();
	C.Type = Sim::CommandType::UsePower;
	C.Value = Power;
	C.Point = RTS::ToSim(PointCm);
	Issue(C);
	UE_LOG(LogTemp, Log, TEXT("RTS poder: o jogador %d pediu o poder %d em (%.0f, %.0f) m no passo %u"),
		int32(LocalPlayer()), Power, PointCm.X / 100.0, PointCm.Y / 100.0, unsigned(State->World.Tick()));
}

int32 URTSMatchSubsystem::PlayerXp(uint8 Player) const
{
	return State != nullptr && Player < State->World.PlayerCount() ? State->World.Xp(Player) : 0;
}

int32 URTSMatchSubsystem::PlayerRank(uint8 Player) const
{
	return State != nullptr && Player < State->World.PlayerCount() ? State->World.Rank(Player) : 1;
}

int32 URTSMatchSubsystem::RankXpThreshold(int32 Rank) const
{
	return State != nullptr && Rank > 1 ? State->World.Rules().Economy.RankXp[FMath::Min(Rank, Sim::MaxRank) - 1] : 0;
}

int32 URTSMatchSubsystem::PowerCount() const
{
	return State != nullptr ? FMath::Min(int32(State->World.Rules().Powers.size()), Sim::MaxPowers) : 0;
}

bool URTSMatchSubsystem::GetPowerInfo(int32 Power, FPowerInfo& Out) const
{
	if (Power < 0 || Power >= PowerCount())
	{
		return false;
	}
	const Sim::World& W = State->World;
	const Sim::PowerDef& D = W.Rules().Powers[size_t(Power)];
	Out = FPowerInfo();
	Out.Key = UTF8_TO_TCHAR(D.Key.c_str());
	const FRTSPowerVisual* Visual = State->Data.FindPowerVisual(Power);
	Out.Name = Visual && !Visual->DisplayName.IsEmpty() ? Visual->DisplayName : Out.Key;
	Out.Icon = Visual && !Visual->Icon.IsEmpty() ? FName(*(TEXT("icon:") + Visual->Icon)) : FName();
	Out.RequiresRank = D.RequiresRank;
	Out.UpgradeRank = D.UpgradeRank;
	Out.CooldownS = float(D.CooldownTicks) / float(Sim::TicksPerSecond);
	Out.Missiles = D.Missiles;
	Out.RadiusM = float(D.PatternRadius.ToDouble());
	Out.SplashM = D.Munition < W.Rules().Units.size() ? float(W.Rules().Units[D.Munition].Weapon.Splash.ToDouble()) : 0.0f;
	const uint8 Me = LocalPlayer();
	if (Me >= W.PlayerCount())
	{
		return true;
	}
	const int32 Rank = W.Rank(Me);
	Out.bUnlocked = Rank >= D.RequiresRank;
	Out.bUpgraded = D.UpgradeRank > 0 && Rank >= D.UpgradeRank;
	if (Out.bUpgraded)
	{
		Out.Missiles = D.MissilesUpgraded > 0 ? D.MissilesUpgraded : D.Missiles;
		Out.RadiusM = float((D.PatternRadiusUpgraded.Raw > 0 ? D.PatternRadiusUpgraded : D.PatternRadius).ToDouble());
	}
	Out.bGranter = W.HasPowerGranter(Me);
	Out.bReady = W.IsPowerReady(Me, Power);
	const int64 Left = FMath::Max<int64>(0, int64(W.PowerReadyAt(Me, Power)) - int64(W.Tick()));
	Out.ReadyInS = float(Left) / float(Sim::TicksPerSecond);
	Out.Remaining = D.CooldownTicks > 0 ? FMath::Clamp(float(Left) / float(D.CooldownTicks), 0.0f, 1.0f) : 0.0f;
	return true;
}

int32 URTSMatchSubsystem::PowerAtSlot(int32 Slot) const
{
	int32 Seen = 0;
	for (int32 K = 0; K < PowerCount(); ++K)
	{
		FPowerInfo P;
		if (GetPowerInfo(K, P) && P.bUnlocked && Seen++ == Slot)
		{
			return K;
		}
	}
	return -1;
}

FText URTSMatchSubsystem::PowerRefusal(int32 Power, const FVector2D* PointCm) const
{
	FPowerInfo P;
	if (!CanOrder(State, bReady, MatchResult) || IsLocalDefeated() || !GetPowerInfo(Power, P))
	{
		return NSLOCTEXT("RTSHUD", "PowerUnavailable", "Not available");
	}
	if (!P.bUnlocked)
	{
		return FText::Format(NSLOCTEXT("RTSHUD", "PowerNeedsRank", "Requires: Rank {0}"), P.RequiresRank);
	}
	const Sim::World& W = State->World;
	if (!P.bGranter)
	{
		// o nome do prédio que liga os poderes sai dos dados (o Command Center)
		FString Granter;
		for (int32 K = 0; K < int32(W.Rules().Units.size()) && Granter.IsEmpty(); ++K)
		{
			if (W.Rules().Units[size_t(K)].bGrantsPowers)
			{
				const FRTSUnitVisual* Visual = State->Data.FindVisual(K);
				Granter = Visual && !Visual->DisplayName.IsEmpty() ? Visual->DisplayName : FString(UTF8_TO_TCHAR(W.Rules().Units[size_t(K)].Key.c_str()));
			}
		}
		return FText::Format(NSLOCTEXT("RTSHUD", "PowerNeedsGranter", "Requires: {0}"), FText::FromString(Granter));
	}
	if (P.ReadyInS > 0.0f)
	{
		const int32 S = FMath::CeilToInt(P.ReadyInS);
		return FText::Format(NSLOCTEXT("RTSHUD", "PowerRecharging", "Ready in {0}"), FText::FromString(FString::Printf(TEXT("%d:%02d"), S / 60, S % 60)));
	}
	if (!P.bReady)
	{
		return NSLOCTEXT("RTSHUD", "PowerUnavailable", "Not available");
	}
	if (PointCm)
	{
		// o ponto: dentro do mapa e, com a névoa, já visto pelo time (a mesma conta do comando; a névoa "explorada" do menu
		// é só desenho, o que vale é o que o time viu de verdade)
		const Sim::Cell C = W.Map().CellOf(RTS::ToSim(*PointCm));
		if (!W.Map().InBounds(C.X, C.Y))
		{
			return NSLOCTEXT("RTSHUD", "PowerOutsideMap", "Outside the map");
		}
		if (W.IsFogOfWar() && !W.IsCellExplored(W.TeamOf(LocalPlayer()), C.X, C.Y))
		{
			return NSLOCTEXT("RTSHUD", "PowerUnexplored", "Unexplored");
		}
	}
	return FText::GetEmpty();
}

bool URTSMatchSubsystem::GetAirlift(uint32 Id, FAirliftInfo& Out) const
{
	const Sim::UnitView* V = State != nullptr ? State->Curr.Find(Id) : nullptr;
	if (!V || V->Owner != LocalPlayer() || V->BuildLeft > 0 || V->Def >= State->World.Rules().Units.size())
	{
		return false;
	}
	const Sim::UnitDef& D = State->World.Rules().Units[V->Def];
	if (!D.HasAirlift())
	{
		return false;
	}
	Out = FAirliftInfo();
	// IncomeLeft = os passos até o próximo pouso; o 1º conta da obra pronta (AirliftFirst), os outros o intervalo
	const int32 Period = V->IncomeCount == 0 ? D.AirliftFirstTicks : D.AirliftPeriodTicks;
	Out.bStarted = V->IncomeLeft > 0;
	Out.LeftS = float(FMath::Max(0, V->IncomeLeft)) / float(Sim::TicksPerSecond);
	Out.PeriodS = float(Period) / float(Sim::TicksPerSecond);
	Out.Progress = Period > 0 && Out.bStarted ? FMath::Clamp(1.0f - float(V->IncomeLeft) / float(Period), 0.0f, 1.0f) : 0.0f;
	Out.NextPallets = FMath::Min(D.AirliftPalletsMin + V->IncomeCount, D.AirliftPalletsMax);
	Out.NextValue = Out.NextPallets * D.AirliftBoxesPerPallet * State->World.Rules().Economy.BoxValue;
	Out.bInbound = Out.bStarted && V->IncomeLeft <= D.AirliftLeadTicks;
	Out.bPaused = Out.bStarted && !Out.bInbound && D.bNeedsPower && State->World.Player(V->Owner).IsLowPower();
	Out.BoxesOnPad = V->Cargo;
	return true;
}

FText URTSMatchSubsystem::UnlocksAtRank(int32 Rank) const
{
	if (State == nullptr)
	{
		return FText::GetEmpty();
	}
	const Sim::Ruleset& R = State->World.Rules();
	TArray<FString> Items;
	for (int32 K = 0; K < PowerCount(); ++K)
	{
		FPowerInfo P;
		GetPowerInfo(K, P);
		const Sim::PowerDef& D = R.Powers[size_t(K)];
		if (D.RequiresRank == Rank)
		{
			Items.Add(P.Name);
		}
		else if (D.UpgradeRank == Rank && D.MissilesUpgraded > 0)
		{
			Items.Add(FText::Format(NSLOCTEXT("RTSHUD", "PowerUpgradedTo", "{0} ({1} missiles)"), FText::FromString(P.Name), D.MissilesUpgraded).ToString());
		}
	}
	for (int32 K = 0; K < int32(R.Units.size()); ++K)
	{
		if (R.Units[size_t(K)].RequiresRank == Rank && Rank > 1)
		{
			const FRTSUnitVisual* Visual = State->Data.FindVisual(K);
			Items.Add(Visual && !Visual->DisplayName.IsEmpty() ? Visual->DisplayName : FString(UTF8_TO_TCHAR(R.Units[size_t(K)].Key.c_str())));
		}
	}
	return FText::FromString(FString::Join(Items, TEXT(", ")));
}

void URTSMatchSubsystem::HandleHudEvents()
{
	if (State == nullptr)
	{
		return;
	}
	const FRTSMatchState& S = *State;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	HudNoticeList.RemoveAll([Now](const FHudNotice& N) { return Now - N.At > 20.0; });
	const uint8 Me = LocalPlayer();
	const bool bMeValid = Me < S.World.PlayerCount();
	for (const Sim::SimEvent& E : S.World.LastEvents())
	{
		if (E.Type == Sim::EventType::RankUp)
		{
			const uint8 P = uint8(E.Value & 0xFF);
			const int32 NewRank = (E.Value >> 8) & 0xFF;
			UE_LOG(LogTemp, Log, TEXT("RTS patente: %s subiu pra patente %d (%d XP) no passo %u"), *PlayerColorName(P).ToString(), NewRank,
				P < S.World.PlayerCount() ? S.World.Xp(P) : 0, unsigned(S.World.Tick()));
			if (P == Me)
			{
				FHudNotice N;
				N.Kind = FHudNotice::EKind::Promoted;
				N.At = Now;
				N.Value = NewRank;
				N.Player = P;
				HudNoticeList.Add(N);
			}
		}
		else if (E.Type == Sim::EventType::PowerUsed)
		{
			// o ataque chegando: o aviso vermelho pra quem tem alguma coisa a R + 20 m do ponto (o inimigo de quem usou)
			const uint8 P = uint8(E.Value & 0xFF);
			const int32 Power = (E.Value >> 8) & 0xFF;
			const int32 Missiles = (E.Value >> 16) & 0xFFFF;
			const FVector2D At = RTS::ToWorld(E.Point);
			UE_LOG(LogTemp, Log, TEXT("RTS poder: %s usou o poder %d (%d mísseis) em (%.0f, %.0f) m no passo %u"), *PlayerColorName(P).ToString(), Power, Missiles,
				At.X / 100.0, At.Y / 100.0, unsigned(S.World.Tick()));
			if (!bMeValid || P >= S.World.PlayerCount() || !S.World.AreEnemies(P, Me) || S.World.Player(Me).bDefeated || Power >= int32(S.World.Rules().Powers.size()))
			{
				continue;
			}
			const Sim::PowerDef& D = S.World.Rules().Powers[size_t(Power)];
			const bool bUp = StrikeUpgraded(S.World, D, E) && D.PatternRadiusUpgraded.Raw > 0;   // o raio que a simulação usou
			const double RadiusCm = (bUp ? D.PatternRadiusUpgraded : D.PatternRadius).ToDouble() * 100.0;
			const double Reach = RadiusCm + 2000.0;
			bool bThreat = false;
			for (const TPair<uint32, Sim::UnitView>& It : S.Curr)
			{
				const Sim::UnitView& V = It.Value;
				if (V.Owner != Me || V.Hp <= 0 || V.Def >= S.World.Rules().Units.size())
				{
					continue;
				}
				const Sim::UnitDef& UD = S.World.Rules().Units[V.Def];
				const double Pad = UD.IsStructure() ? FMath::Max(UD.HalfX.ToDouble(), UD.HalfY.ToDouble()) * 100.0 : 0.0;
				if (FVector2D::Distance(RTS::ToWorld(V.Pos), At) - Pad <= Reach)
				{
					bThreat = true;
					break;
				}
			}
			if (bThreat)
			{
				FHudNotice N;
				N.Kind = FHudNotice::EKind::StrikeIncoming;
				N.At = Now;
				N.Value = Power;
				N.Cm = At;
				N.RadiusCm = float(RadiusCm);
				N.Player = P;
				HudNoticeList.Add(N);
			}
		}
		else if (E.Type == Sim::EventType::AirliftInbound)
		{
			const Sim::UnitView* Zone = S.Curr.Find(E.Source);
			if (Zone && Zone->Owner == Me)
			{
				FHudNotice N;
				N.Kind = FHudNotice::EKind::AirliftInbound;
				N.At = Now;
				N.Value = E.Value;
				N.Cm = RTS::ToWorld(Zone->Pos);
				N.Player = Me;
				HudNoticeList.Add(N);
			}
		}
	}
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

// ---------------------------------------------------------------- (v4, 23/10) a ordem de andar e a prévia da linha: uma conta só

namespace
{
	/** O grupo que o comando leva: em ordem de ID, cortado no teto do comando (a rede e o replay recusam acima disso). */
	TArray<uint32> RTSOrderGroup(const TArray<uint32>& Units)
	{
		TArray<uint32> Sorted = Units;
		Sorted.Sort();
		if (Sorted.Num() > Sim::MaxGroupSize)
		{
			Sorted.SetNum(Sim::MaxGroupSize);
		}
		return Sorted;
	}

	/** O valor do Move: a direção vira inteiro aqui, no PC de quem manda (todos recebem o mesmo número), e a largura em passos
	 *  de 2 m. A ordem e a prévia passam por aqui: o arredondamento das duas não diverge. */
	int32 RTSPackMoveValue(bool bFormation, bool bFacing, float FacingDeg, float WidthM)
	{
		const int32 FacingBam = FMath::RoundToInt(FMath::Fmod(FMath::Fmod(FacingDeg, 360.0f) + 360.0f, 360.0f) / 360.0f * float(Sim::Angle::FullTurn)) & 0xFFFF;
		return Sim::MoveValue(bFormation, bFacing, FMath::RoundToInt(WidthM * 0.5f), uint16_t(FacingBam));
	}
}

bool URTSMatchSubsystem::PreviewLineMove(const TArray<uint32>& Units, const FVector2D& TargetCm, bool bFormation, float FacingDeg, float WidthM,
	TArray<FLineSlot>& Out) const
{
	Out.Reset();
	if (!(State != nullptr) || !bReady || Units.IsEmpty() || MatchResult != ERTSMatchResult::Playing)
	{
		return false;
	}
	const TArray<uint32> Group = RTSOrderGroup(Units);
	const std::vector<Sim::EntityId> Ids(Group.GetData(), Group.GetData() + Group.Num());
	std::vector<Sim::EntityId> SlotIds;
	std::vector<Sim::Vec2> Slots;
	if (!State->World.PreviewLineSlots(LocalPlayer(), Ids, RTS::ToSim(TargetCm), RTSPackMoveValue(bFormation, true, FacingDeg, WidthM), SlotIds, Slots))
	{
		return false;
	}
	Out.Reserve(int32(SlotIds.size()));
	for (size_t K = 0; K < SlotIds.size() && K < Slots.size(); ++K)
	{
		FLineSlot& Slot = Out.AddDefaulted_GetRef();
		Slot.Id = SlotIds[K];
		Slot.Cm = RTS::ToWorld(Slots[K]);
		Sim::UnitView V;
		Slot.RadiusCm = State->World.FindUnit(SlotIds[K], V) ? float(State->World.Rules().Units[V.Def].Radius.ToDouble() * 100.0) : 360.0f;
	}
	return !Out.IsEmpty();
}

bool URTSMatchSubsystem::GetOrderPointCm(uint32 Id, FVector2D& Out) const
{
	Sim::UnitView V;
	if (!(State != nullptr) || !State->World.FindUnit(Id, V))
	{
		return false;
	}
	Out = RTS::ToWorld(V.OrderPoint);
	return true;
}

void URTSMatchSubsystem::IssueGroupIfChanged(const TArray<uint32>& Units)
{
	TArray<uint32> Sorted = RTSOrderGroup(Units);
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
	C.Value = RTSPackMoveValue(bFormation, bFacing, FacingDeg, WidthM);   // a mesma conta da prévia da linha
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
	ALookDirector::SetFogOfWar(nullptr, FVector2D::ZeroVector, FVector2D::ZeroVector);
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
	if (MissileSmoke)
	{
		MissileSmoke->Shutdown();
		delete MissileSmoke;
		MissileSmoke = nullptr;
		UBlastFxDirector::SetExternalMissileSmoke(false);
	}
	SaveReplay();
	delete State;
	State = nullptr;
	Views.Reset();
	Wrecks.Reset();
	WreckPoses.Reset();
	WreckGhosts.Reset();
	WreckGoneEarly.Reset();
	DockViews.Reset();
	PendingBlasts.Reset();
	DyingViews.Reset();
	DyingInfo.Reset();
	Super::Deinitialize();
}
