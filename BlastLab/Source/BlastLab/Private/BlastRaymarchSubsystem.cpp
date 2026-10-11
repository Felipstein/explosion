#include "BlastRaymarchSubsystem.h"

#include "Engine/Texture.h"
#include "HAL/IConsoleManager.h"
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

const TArray<FName>& UBlastRaymarchSubsystem::CompareSources()
{
	static const TArray<FName> Names = []
	{
		TArray<FName> Out;
		FString S;
		TArray<FString> P;
		if (FParse::Value(FCommandLine::Get(), TEXT("RmCompare="), S, false) && S != TEXT("0"))
		{
			S.ParseIntoArray(P, TEXT(","));
			for (const FString& N : P)
			{
				Out.Add(FName(N == TEXT("1") ? TEXT("r128_v0") : *N));
			}
		}
		return Out;
	}();
	return Names;
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
	auto LoadBake = [](const FString& File) -> BlastRaymarch::FBakeRef
	{
		FString Error;
		BlastRaymarch::FBakeRef B = BlastRaymarch::FBakeData::Load(FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("Raymarch"), File), Error);
		if (!B)
		{
			UE_LOG(LogTemp, Error, TEXT("BlastLab raymarch: %s"), *Error);
		}
		return B;
	};
	Sources.Reset();
	SourceOf.Reset();
	// a de 128 como padrão (-RmBake=128): as três variantes tocam a v0 dela
	const BlastRaymarch::FBakeRef B128 = BakeRes() == 128 ? LoadBake(TEXT("r128_v0.brm2")) : nullptr;
	for (int32 V = 0; V < 3; ++V)
	{
		Sources.Add(B128 ? B128 : LoadBake(FString::Printf(TEXT("r96_v%d.brm2"), V)));
		GLoaded[V] = Sources[V].IsValid() && Noise != nullptr;
	}
	// as gêmeas da comparação lado a lado
	for (const FName Cmp : CompareSources())
	{
		if (BlastRaymarch::FBakeRef B = LoadBake(Cmp.ToString() + TEXT(".brm2")))
		{
			SourceOf.Add(Cmp, Sources.Add(B));
			UE_LOG(LogTemp, Display, TEXT("BlastLab raymarch: comparação com %s (%d³, %.1f m, %d quadros)"), *Cmp.ToString(), B->Res, B->Domain, B->Frames);
		}
	}
	Extension = FSceneViewExtensions::NewExtension<FBlastRaymarchExtension>();
	UE_LOG(LogTemp, Display, TEXT("BlastLab raymarch: ligado (gravações %s %s %s)"),
		Sources[0] ? TEXT("v0") : TEXT("-"), Sources[1] ? TEXT("v1") : TEXT("-"), Sources[2] ? TEXT("v2") : TEXT("-"));
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
		if (V < 0 || !Sources.IsValidIndex(V) || !Sources[V] || !Handles(E) || E->Age < 0.0f || E->Opacity <= 0.0f || E->IsFinished())
		{
			continue;
		}
		FLive L;
		L.Variant = V;
		if (const int32* S = E->RaymarchSource.IsNone() ? nullptr : SourceOf.Find(E->RaymarchSource))
		{
			L.Variant = *S;          // a chave dos quadros no pool: separada das variantes
		}
		L.I.Bake = Sources[L.Variant];
		L.I.FootCm = E->GetActorLocation();
		L.I.Scale = E->SizeScale;
		L.I.Frame = L.I.Bake->FrameOfTime(E->BakeTime());
		L.I.Seed = E->Seed;
		L.I.Keep = E->Opacity;
		L.I.Thickness = E->Thickness;
		L.I.bNuclear = E->bNuclear;
		// O clarão (10/10): a fase branca da bola real dura ~15–20 ms numa carga de
		// 1–2 t, mas assim ele passava em 1 quadro (o Felipe: "mal vejo o branco");
		// os artistas esticam pra 2–3 quadros até ~0.1 s (o Zero Hour, ~0.17 s): τ 100 ms (ele escolheu, 10/10)
		// no relógio da gravação (escala com o tamanho). Num quadro só
		// ela se perderia a 30 qps ou num tranco: vai a média de exp(−t/τ) no
		// intervalo que o quadro cobriu, que conserva a energia do clarão.
		if (E->bFlash)
		{
			IConsoleVariable* CVarTau = IConsoleManager::Get().FindConsoleVariable(TEXT("blast.FlashTau"));   // BlastRaymarch.cpp, trocável ao vivo (G/J/K no app)
			const float FlashTau = FMath::Max(CVarTau ? CVarTau->GetFloat() : 0.10f, 1e-3f);
			const float T1 = E->BakeTime();
			const float T0 = FMath::Max(T1 - DeltaTime / FMath::Max(E->TimeScale > 0.0f ? E->TimeScale : E->SizeScale, 0.01f), 0.0f);
			L.I.Flash = T1 - T0 > 1e-5f ? FlashTau * (FMath::Exp(-T0 / FlashTau) - FMath::Exp(-T1 / FlashTau)) / (T1 - T0) : FMath::Exp(-T1 / FlashTau);
		}
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
	// a vaga do pool no tamanho da maior gravação carregada; o atlas de blocos pro quadro mais cheio delas
	FIntVector PoolRes(0);
	int32 MaxBricks = 0;
	for (const BlastRaymarch::FBakeRef& B : Sources)
	{
		if (B)
		{
			PoolRes = FIntVector(FMath::Max(PoolRes.X, B->Res), FMath::Max(PoolRes.Y, B->FuelRes), FMath::Max(PoolRes.Z, B->MacroRes));
			MaxBricks = FMath::Max(MaxBricks, B->MaxBricks);
		}
	}
	for (const FLive& L : Live)
	{
		const int32 F0 = FMath::Clamp(FMath::FloorToInt32(L.I.Frame), 0, L.I.Bake->Frames - 1);
		Needed.Add(Key(L.Variant, F0));
		Needed.Add(Key(L.Variant, FMath::Min(F0 + 1, L.I.Bake->Frames - 1)));
	}
	const int32 NumSlots = MaxSlots;
	// os blocos do atlas: o que não couber faz sair das vagas os quadros velhos (os menos usados que
	// nenhuma explosão precisa agora), como as próprias vagas já fazem
	const int32 Side = AtlasSide(MaxBricks);
	const int32 Capacity = Side * Side * Side;
	auto SlotFor = [&](int32 V, int32 F) -> int32
	{
		int32 Victim = -1;
		for (int32 s = 0; s < NumSlots; ++s)
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
			UsedBricks -= Slots[Victim].Bricks;
			const int32 Need = Sources[V]->NumBricks[F];
			while (UsedBricks + Need > Capacity)
			{
				int32 Old = -1;
				for (int32 s = 0; s < NumSlots; ++s)
				{
					if (s != Victim && Slots[s].Bricks > 0 && !Needed.Contains(Key(Slots[s].Variant, Slots[s].Frame))
						&& (Old < 0 || Slots[s].LastUse < Slots[Old].LastUse))
					{
						Old = s;
					}
				}
				if (Old < 0)
				{
					break;      // só sobraram quadros em uso: o render avisa que o atlas encheu
				}
				UsedBricks -= Slots[Old].Bricks;
				Slots[Old] = FSlot();
				FUpload Evict;
				Evict.Slot = Old;
				Evict.bEvict = true;
				Uploads.Add(Evict);
			}
			UsedBricks += Need;
			Slots[Victim] = { V, F, TickCount, Need };
			Uploads.Add({ Sources[V], F, Victim });
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
	Extension->Submit_GameThread(MoveTemp(List), MoveTemp(Uploads), Noise->GetResource(), Worley ? Worley->GetResource() : nullptr, PoolRes, MaxBricks);
}
