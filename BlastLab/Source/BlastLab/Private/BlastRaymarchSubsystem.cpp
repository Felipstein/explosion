#include "BlastRaymarchSubsystem.h"

#include "Engine/Texture.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ExplosionBake.h"
#include "ExplosionVolume.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "SceneViewExtension.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

namespace
{
	const TCHAR* NoisePath = TEXT("/Game/Explosions/T_CurlNoise.T_CurlNoise");
	const TCHAR* WorleyPath = TEXT("/Game/Explosions/T_DetailWorley.T_DetailWorley");

	// ligado por padrão (08/10); -Raymarch=0 volta pro volume da Unreal (comparação)
	bool Enabled()
	{
		static const bool bOn = [] { int32 V = 1; FParse::Value(FCommandLine::Get(), TEXT("Raymarch="), V); return V != 0; }();
		return bOn;
	}

	// as gravações que carregaram: sem o arquivo, a explosão fica no volume da Unreal em vez de sumir
	bool GLoaded[3] = { false, false, false };

	/** DA_HeavyCharge_v<n> → n; as outras gravações (os corpos da EmberGen) ficam no volume da Unreal. */
	int32 VariantOf(const AExplosionVolume* E)
	{
		if (!E || !E->Bake)
		{
			return -1;
		}
		const FString Name = E->Bake->GetName();
		if (!Name.StartsWith(TEXT("DA_HeavyCharge_v")) || Name.Len() != 17)
		{
			return -1;
		}
		const int32 V = Name[16] - TEXT('0');
		// -RmVariant=<n>: todas tocam a variante n (A/B com a mesma explosão)
		static const int32 Forced = [] { int32 F = -1; FParse::Value(FCommandLine::Get(), TEXT("RmVariant="), F); return F; }();
		return V >= 0 && V < 3 ? (Forced >= 0 && Forced < 3 ? Forced : V) : -1;
	}

	/**
	 * A resolução da gravação (-RmBake=, padrão 96). A de 128 (08/10, o teste da
	 * maciez de perto) é a simulação do WebGL sem a redução pra 96; o navegador
	 * só aguenta gravar uma variante nela (1.45 GB numa textura com as três), então
	 * as três tocam a v0.
	 */
	int32 BakeRes()
	{
		static const int32 R = [] { int32 V = 96; FParse::Value(FCommandLine::Get(), TEXT("RmBake="), V); return V; }();
		return R;
	}
}

bool UBlastRaymarchSubsystem::CompareMode()
{
	static const bool bOn = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("RmCompare="), V); return V != 0; }();
	return bOn;
}

bool UBlastRaymarchSubsystem::Handles(const AExplosionVolume* E)
{
	const int32 V = VariantOf(E);
	return Enabled() && V >= 0 && GLoaded[V] && E->LoopPeriod <= 0.0f;
}

void UBlastRaymarchSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!Enabled())
	{
		return;
	}
	Noise = LoadObject<UTexture>(nullptr, NoisePath);
	Worley = LoadObject<UTexture>(nullptr, WorleyPath);
#if WITH_EDITOR
	// no editor a textura termina de compilar em segundo plano e troca o recurso
	for (UTexture* T : { Noise.Get(), Worley.Get() })
	{
		if (T)
		{
			FTextureCompilingManager::Get().FinishCompilation({ T });
		}
	}
#endif
	// a de 128: quando é a padrão (-RmBake=128) ou na comparação lado a lado
	if (BakeRes() == 128 || CompareMode())
	{
		FString Error;
		Bake128 = BlastRaymarch::FBakeData::Load(FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("Raymarch"), TEXT("r128_v0.bin")), Error);
		if (!Bake128)
		{
			UE_LOG(LogTemp, Error, TEXT("BlastLab raymarch: %s"), *Error);
		}
	}
	for (int32 V = 0; V < 3; ++V)
	{
		FString Error;
		if (BakeRes() == 128 && Bake128)
		{
			Bakes[V] = Bake128;
		}
		else
		{
			const FString Path = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("Raymarch"), FString::Printf(TEXT("r96_v%d.bin"), V));
			Bakes[V] = BlastRaymarch::FBakeData::Load(Path, Error);
		}
		GLoaded[V] = Bakes[V].IsValid() && Noise != nullptr;
		if (!Bakes[V])
		{
			UE_LOG(LogTemp, Error, TEXT("BlastLab raymarch: %s"), *Error);
		}
	}
	Extension = FSceneViewExtensions::NewExtension<FBlastRaymarchExtension>();
	UE_LOG(LogTemp, Display, TEXT("BlastLab raymarch: ligado (gravações %s %s %s)"),
		Bakes[0] ? TEXT("v0") : TEXT("-"), Bakes[1] ? TEXT("v1") : TEXT("-"), Bakes[2] ? TEXT("v2") : TEXT("-"));
}

void UBlastRaymarchSubsystem::Deinitialize()
{
	if (Extension)
	{
		Extension->Submit_GameThread({}, {}, nullptr);
		FlushRenderingCommands();
		Extension.Reset();
	}
	Super::Deinitialize();
}

void UBlastRaymarchSubsystem::Tick(float DeltaTime)
{
	using namespace BlastRaymarch;
	if (!Extension || !Noise)
	{
		return;
	}
	++TickCount;
	FVector Eye = FVector::ZeroVector;
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController(); PC && PC->PlayerCameraManager)
	{
		Eye = PC->PlayerCameraManager->GetCameraLocation();
	}

	// as explosões vivas, da mais perto pra mais longe
	struct FLive { FInstance I; int32 Variant; double D2; };
	TArray<FLive> Live;
	for (TActorIterator<AExplosionVolume> It(GetWorld()); It; ++It)
	{
		AExplosionVolume* E = *It;
		const int32 V = VariantOf(E);
		if (V < 0 || !Bakes[V] || !Handles(E) || E->Age < 0.0f || E->Opacity <= 0.0f || E->IsFinished())
		{
			continue;
		}
		FLive L;
		L.Variant = V;
		L.I.Bake = Bakes[V];
		if (E->RaymarchBakeRes == 128 && Bake128)
		{
			L.I.Bake = Bake128;
			L.Variant = 3;           // a chave dos quadros no pool: separada das de 96
		}
		L.I.FootCm = E->GetActorLocation();
		L.I.Scale = E->SizeScale;
		L.I.Frame = Bakes[V]->FrameOfTime(E->BakeTime());
		L.I.Seed = E->Seed;
		L.I.Keep = E->Opacity;
		L.I.Thickness = E->Thickness;
		L.I.bNuclear = E->bNuclear;
		L.I.Warp = E->GetWarp() ? E->GetWarp()->GetResource() : nullptr;
		if (L.I.Warp && !WarpLogged.Contains(E))
		{
			WarpLogged.Add(E);
			UE_LOG(LogTemp, Display, TEXT("BlastLab raymarch: deformação ligada (t = %.2f s da gravação)"), E->BakeTime());
		}
		L.D2 = FVector::DistSquared(Eye, L.I.FootCm);
		Live.Add(MoveTemp(L));
	}
	Live.Sort([](const FLive& A, const FLive& B) { return A.D2 < B.D2; });
	if (Live.Num() > MaxInstances)
	{
		Live.SetNum(MaxInstances);
	}

	// os quadros: cada explosão precisa dos dois em volta do instante
	TArray<FUpload> Uploads;
	TArray<FInstance> List;
	TSet<int32> Needed;
	auto Key = [](int32 V, int32 F) { return V * 1024 + F; };
	// a vaga do pool no tamanho da maior gravação carregada
	FIntVector PoolRes(0);
	for (const BlastRaymarch::FBakeRef& B : { Bakes[0], Bakes[1], Bakes[2], Bake128 })
	{
		if (B)
		{
			PoolRes = FIntVector(FMath::Max(PoolRes.X, B->Res), FMath::Max(PoolRes.Y, B->FuelRes), FMath::Max(PoolRes.Z, B->MacroRes));
		}
	}
	for (const FLive& L : Live)
	{
		const int32 F0 = FMath::Clamp(FMath::FloorToInt32(L.I.Frame), 0, L.I.Bake->Frames - 1);
		Needed.Add(Key(L.Variant, F0));
		Needed.Add(Key(L.Variant, FMath::Min(F0 + 1, L.I.Bake->Frames - 1)));
	}
	auto SlotFor = [&](int32 V, int32 F) -> int32
	{
		int32 Victim = -1;
		for (int32 s = 0; s < MaxSlots; ++s)
		{
			if (Slots[s].Variant == V && Slots[s].Frame == F)
			{
				Slots[s].LastUse = TickCount;
				return s;
			}
			const bool bFree = Slots[s].Variant < 0 || !Needed.Contains(Key(Slots[s].Variant, Slots[s].Frame));
			if (bFree && (Victim < 0 || Slots[s].LastUse < Slots[Victim].LastUse))
			{
				Victim = s;
			}
		}
		if (Victim >= 0)
		{
			Slots[Victim] = { V, F, TickCount };
			Uploads.Add({ Bakes[V], F, Victim });
		}
		return Victim;
	};
	for (FLive& L : Live)
	{
		const int32 F0 = FMath::Clamp(FMath::FloorToInt32(L.I.Frame), 0, L.I.Bake->Frames - 1);
		L.I.Slot0 = SlotFor(L.Variant, F0);
		L.I.Slot1 = SlotFor(L.Variant, FMath::Min(F0 + 1, L.I.Bake->Frames - 1));
		if (L.I.Slot0 >= 0 && L.I.Slot1 >= 0)
		{
			List.Add(L.I);
		}
	}
	if (List.IsEmpty() && bWasEmpty)
	{
		return;
	}
	bWasEmpty = List.IsEmpty();
	Extension->Submit_GameThread(MoveTemp(List), MoveTemp(Uploads), Noise->GetResource(), Worley ? Worley->GetResource() : nullptr, PoolRes);
}
