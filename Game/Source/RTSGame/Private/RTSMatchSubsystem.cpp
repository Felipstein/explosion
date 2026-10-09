#include "RTSMatchSubsystem.h"

#include <vector>

#include "BlastFxDirector.h"
#include "Components/LightComponent.h"
#include "ContentStreaming.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RTSConvert.h"
#include "RTSCameraPawn.h"
#include "RTSEffect.h"
#include "RTSFogOfWar.h"
#include "RTSFogPass.h"
#include "RTSSessionSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "RTSGameData.h"
#include "RTSMapBaker.h"
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

FLinearColor URTSMatchSubsystem::PlayerColor(uint8 Player)
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
	return Colors[Player % 8];
}

bool URTSMatchSubsystem::StartSkirmish(const FString& MapName, FString& OutError)
{
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
	if (Info->StartsM.Num() < 2)
	{
		OutError = TEXT("o mapa precisa de 2 posições iniciais");
		return false;
	}
	if (!RTSMapBaker::Bake(GetWorld(), *Info, S->Baked, OutError))
	{
		return false;
	}

	// onde cada um começa: nas bases, ou (skirmish de só tanques) no caminho entre elas,
	// a SkirmishGapM um do outro: sempre em chão andável e com caminho até o inimigo
	TArray<FVector2D> Starts = Info->StartsM;
	if (Info->SkirmishGapM > 0.0f && S->Data.Rules.Units.size() > 0)
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

	// a montagem: você (time 0) contra a IA (time 1)
	Sim::MatchSetup& Setup = S->Setup;
	Setup.Seed = FPlatformTime::Cycles64();
	{
		const URTSSessionSubsystem* Session = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<URTSSessionSubsystem>() : nullptr;
		Setup.bFogOfWar = Session ? Session->bFogOfWar : true;
		Setup.bAllowDebugCommands = true;   // skirmish local: o F9 vale
	}
	Setup.Map = S->Baked.Logic;
	Setup.Map.ComputeClearance();
	Setup.Rules = S->Data.Rules;
	for (int32 P = 0; P < 2; ++P)
	{
		Sim::PlayerSetup Player;
		Player.Team = uint8(P);
		Player.bIsAI = P != 0;
		const FVector2D Base = Starts[P];
		// de frente pro inimigo, em fileiras de 3
		const FVector2D Forward = (Starts[1 - P] - Base).GetSafeNormal();
		const FVector2D Right(-Forward.Y, Forward.X);
		const Sim::Angle Facing = Sim::Atan2(Sim::Fix::FromRatio(RTS::Milli(Forward.Y), 1000), Sim::Fix::FromRatio(RTS::Milli(Forward.X), 1000));
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
				const FVector2D PosM = Base + Right * (Col * StartSpacingM) - Forward * (Row * StartSpacingM);
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
	if (!S->World.Init(Setup))
	{
		OutError = TEXT("a simulação recusou a montagem");
		return false;
	}
	for (size_t P = 0; P < Setup.Players.size(); ++P)
	{
		if (Setup.Players[P].bIsAI)
		{
			S->Brains.emplace_back(Sim::PlayerIndex(P));
			// a IA parte em 8 s: com o começo a ~450 m, o contato vem entre ~30 s (você avançando) e ~65 s
			S->Brains.back().FirstWaveDelayTicks = 8 * Sim::TicksPerSecond;
		}
	}
	S->Replay.Begin(Setup);
	S->LocalStartCm = Starts[0] * 100.0;
	{
		const Sim::SimMap& M = S->World.Map();
		const FVector2D MinCm = RTS::ToWorld(M.MinCorner());
		const FVector2D MaxCm = RTS::ToWorld(M.MaxCorner());
		Fog = NewObject<URTSFogOfWar>(this);
		Fog->Init(M.Width, M.Height, MinCm, MaxCm - MinCm);
	}
	bFogDisplay = Setup.bFogOfWar;
	bFogPassOn = false;
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
	if (!bPaused)
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
	StartWorldSeconds = GetWorld()->GetTimeSeconds();
	LastDeathWorldSeconds = StartWorldSeconds;
	UE_LOG(LogTemp, Log, TEXT("RTS cena pronta em %.1f s%s (shaders na fila %d, texturas faltando %d)"),
		Now - WarmStartSeconds, bSettled ? TEXT("") : TEXT(", pelo teto"), Shaders, Textures);
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
			(uint8(E.Value) == LocalPlayer() ? Lost : Killed) += 1;
			LastDeathWorldSeconds = World ? World->GetTimeSeconds() : 0.0;
			{
				// a explosão só se o jogador vê (a de dentro da névoa não acontece pra ele)
				const FVector2D At = RTS::ToWorld(E.Point);
				const bool bSeen = uint8(E.Value) == LocalPlayer() || IsPointShown(At);
				UBlastFxDirector* Director = UBlastFxDirector::Get(this);
				if (Director && bSeen)
				{
					Director->TankDeath(FVector(At.X, At.Y, S.Baked.GroundZCm(At)), E.Target);
				}
				if (TObjectPtr<ARTSUnitActor>* Dying = Views.Find(E.Target))
				{
					(*Dying)->bWreckKnown = bSeen;
				}
			}
			if (TObjectPtr<ARTSUnitActor>* View = Views.Find(E.Target))
			{
				(*View)->Die();
				Wrecks.Add(*View);
				Views.Remove(E.Target);
			}
			break;
		case Sim::EventType::MatchOver:
		{
			const int32 LocalTeam = S.Setup.Players[LocalPlayer()].Team;
			MatchResult = E.Value < 0 ? ERTSMatchResult::Draw : (E.Value == LocalTeam ? ERTSMatchResult::Victory : ERTSMatchResult::Defeat);
			EndWorldSeconds = World ? World->GetTimeSeconds() : 0.0;
			SaveReplay();
			break;
		}
		default:
			break;
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
			if (!bFog || !IsEnemyOfLocal(U.Owner) || (Fog && Fog->IsVisibleNow(U.PosCm)))
			{
				It.Value->LastSeenSeconds = Now;
			}
			// o ZH segue mostrando o objeto um instante depois de sair do claro (GameClient: 2 s)
			It.Value->SetFogHidden(bFog && Now - It.Value->LastSeenSeconds > FogGraceSeconds);
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
	return State != nullptr && bFogDisplay && MatchResult == ERTSMatchResult::Playing;
}

void URTSMatchSubsystem::ToggleFogReveal()
{
	if (State == nullptr)
	{
		return;
	}
	bFogDisplay = !bFogDisplay;
	// a regra acompanha pela fila (vale no passo seguinte, e até pausado ou depois do fim)
	Sim::Command C;
	C.Player = LocalPlayer();
	C.Type = Sim::CommandType::SetFog;
	C.Target = bFogDisplay ? 1 : 0;
	State->Queue.Issue(C, State->World.Tick());
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
	return true;
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
	return FString::Printf(TEXT("passo %u   impressão %016llx   %.2f ms/passo   %d unidades   névoa %s (F9)"),
		State->World.Tick(), (unsigned long long)State->World.StateHash(), LastStepMs, State->World.UnitCount(),
		bFogDisplay ? TEXT("ligada") : TEXT("desligada"));
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
	State->Queue.Issue(C, State->World.Tick());
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
	State->Queue.Issue(C, State->World.Tick());
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
	State->Queue.Issue(C, State->World.Tick());
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
	State->Queue.Issue(C, State->World.Tick());
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
	Super::Deinitialize();
}
