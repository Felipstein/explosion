// Cópia do BlastLab (commit a14c45a, 10/10/2026): a ponte até o plugin comum que a sessão do
// BlastLab vai fazer (docs/BASE_JOGAVEL.md). A original continua sendo a referência: mudanças
// de aparência nascem lá e são trazidas pra cá.
#include "BlastRaymarchSubsystem.h"

#include "Engine/Texture.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ExplosionBake.h"
#include "ExplosionVolume.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "BlastFxGround.h"
#include "BlastFxDirector.h"
#include "BlastFxHitLife.h"
#include "RTSDustField.h"
#include "RTSSmokeShadow.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "SceneViewExtension.h"
#include "ProfilingDebugging/CsvProfiler.h"

// o custo no perfil (-csvGpuStats -csvCaptureFrames=N): BlastFxRaymarch/Tick (a lista, os quadros, a névoa e a sombra)
CSV_DEFINE_CATEGORY(BlastFxRaymarch, true);
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
	// as gravações próprias que carregaram (a morte de tanque, 13/10)
	TSet<FName> GLoadedSources;
	// as gravações próprias do jogo: a morte de tanque da v2 (BlastLab, 13/10: eg_tq_v2, Game/Intermediate/Raymarch)
	// e a poeira do tiro (14/10, a v3: a areia do sopro do disparo, da onda de choque e do tiro no chão; a DustImpact da
	// JangaFX convertida em 96³, 40 m, 3 s)
	const TCHAR* OwnBakes[] = { TEXT("eg_tq_v2"), TEXT("jg_dust_impact_96"), TEXT("eg_poeira3_96"), TEXT("jg_ground_96"), TEXT("sim_chao_anel"), TEXT("sim_chao_cortina"), TEXT("sim_boca"), TEXT("sim_carcaca") };   // (16/10: o anel e o jato do tiro na areia; o anel simulado, v7; a cortina de ejeção, v8)
	// (23/10, a explosão com vida) as gravações dedicadas do acerto de míssil (o casco de lado, o teto, o chão e o chão do ataque), sendo
	// simuladas agora: entram só se o arquivo existe (sem o erro no registro); sem elas, o acerto toca a Carga Pesada em escala, como hoje
	// (UBlastFxDirector::MissileHitLife, a camada blast.HitLife.Bake). O game.ps1 liga os arquivos do BlastLab (LinkFile), como os outros
	// (23/10 noite) mais a warhead_lado_m (a espelhada: a outra mão do lobo) e a warhead_chao_s (só o fogo e a fuligem do chão: a ogiva da
	// metade leve do ataque em cima da strike_chao, -StrikeWarhead=1)
	const TCHAR* OptionalBakes[] = { TEXT("warhead_lado"), TEXT("warhead_lado_m"), TEXT("warhead_teto"), TEXT("warhead_chao"), TEXT("warhead_chao_s"), TEXT("strike_chao") };
	// (23/10 noite) quais carregam: as que têm o arquivo em Intermediate/Raymarch, e lá o game.ps1 Worlds só liga o que ganhou da reserva no
	// A/B do jogo (a prova: docs/ESTUDO_MISSIL_PATRIOT.md, "As gravações dedicadas no jogo"). -HitBakes=a,b,... escolhe (all = todas as que
	// existem, none = nenhuma: a reserva exata); -HitBakeFile=nome:arquivo,... toca outro arquivo (o nome em Intermediate/Raymarch ou o
	// caminho inteiro do BlastLab, sem link a mais no pacote) com o nome do jogo: o A/B das variantes na mesma corrente
	const TCHAR* DefaultHitBakes = TEXT("all");
	bool HitBakeWanted(const TCHAR* Own)
	{
		static const FString List = [] { FString V = DefaultHitBakes; FParse::Value(FCommandLine::Get(), TEXT("HitBakes="), V, false); return V; }();
		if (List == TEXT("all"))
		{
			return true;
		}
		TArray<FString> Names;
		List.ParseIntoArray(Names, TEXT(","));
		return Names.Contains(FString(Own));
	}
	FString HitBakeFile(const TCHAR* Own)
	{
		static const TMap<FString, FString> Files = []
		{
			TMap<FString, FString> M;
			FString V;
			if (FParse::Value(FCommandLine::Get(), TEXT("HitBakeFile="), V, false))
			{
				TArray<FString> Pairs;
				V.ParseIntoArray(Pairs, TEXT(","));
				for (const FString& P : Pairs)
				{
					FString K, F;
					if (P.Split(TEXT(":"), &K, &F))
					{
						M.Add(K, F);
					}
				}
			}
			return M;
		}();
		const FString* F = Files.Find(FString(Own));
		const FString Name = F ? *F : FString(Own);
		// um caminho inteiro (o A/B direto do arquivo do BlastLab: nada de link a mais em Intermediate/Raymarch, que vai inteira pro pacote)
		// ou o nome em Intermediate/Raymarch
		return FPaths::IsRelative(Name) ? FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("Raymarch"), Name + TEXT(".brm2")) : Name;
	}
	// (23/10 noite) o custo da subida (-RmUploadLog=1): a cada 1 s, os quadros que subiram pra placa, os MB no segundo e o pior quadro
	bool UploadLog()
	{
		static const bool bOn = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("RmUploadLog="), V); return V != 0; }();
		return bOn;
	}

	/** DA_HeavyCharge_v<n> → n; as outras gravações (os corpos da EmberGen) ficam no volume da Unreal. */
	int32 VariantOf(const AExplosionVolume* E)
	{
		if (!E || !E->Bake)
		{
			return -1;
		}
		// com a gravação própria carregada: 0 só pra passar no filtro, o Tick troca pela gravação dela
		if (!E->RaymarchSource.IsNone() && GLoadedSources.Contains(E->RaymarchSource))
		{
			return 0;
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
	// as gravações (~95 MB cada) ficam na memória a partida inteira e entre partidas: o jogo troca de
	// mapa do menu pra partida (OpenLevel), e reler ~280 MB a cada troca atrasava a tela de carregar
	auto LoadBake = [](const FString& File) -> BlastRaymarch::FBakeRef
	{
		static TMap<FString, BlastRaymarch::FBakeRef> Cache;
		if (const BlastRaymarch::FBakeRef* Hit = Cache.Find(File))
		{
			return *Hit;
		}
		FString Error;
		BlastRaymarch::FBakeRef B = BlastRaymarch::FBakeData::Load(FPaths::IsRelative(File) ? FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("Raymarch"), File) : File, Error);
		if (!B)
		{
			UE_LOG(LogTemp, Error, TEXT("BlastFx raymarch: %s"), *Error);
		}
		else
		{
			Cache.Add(File, B);
		}
		return B;
	};
	Sources.Reset();
	// a de 128 como padrão (-RmBake=128): as três variantes tocam a v0 dela
	const BlastRaymarch::FBakeRef B128 = BakeRes() == 128 ? LoadBake(TEXT("r128_v0.brm2")) : nullptr;
	for (int32 V = 0; V < 3; ++V)
	{
		Sources.Add(B128 ? B128 : LoadBake(FString::Printf(TEXT("r96_v%d.brm2"), V)));
		GLoaded[V] = Sources[V].IsValid() && Noise != nullptr;
	}
	SourceOf.Reset();
	GLoadedSources.Reset();
	for (const TCHAR* Own : OwnBakes)
	{
		if (BlastRaymarch::FBakeRef B = LoadBake(FString(Own) + TEXT(".brm2")))
		{
			SourceOf.Add(FName(Own), Sources.Add(B));
			GLoadedSources.Add(FName(Own));
			UE_LOG(LogTemp, Display, TEXT("BlastFx raymarch: gravação %s (%d³, %.1f m, %d quadros)"), Own, B->Res, B->Domain, B->Frames);
		}
	}
	// (a revisão da vida) só com a vida ligada (-HitLifeV=2 e a ogiva v2, a camada blast.HitLife.Bake) e fora da Baixa: cada gravação fica na
	// memória (70–350 MB) e a maior manda nas vagas e no atlas (o PoolRes e o MaxBricks) de todas as explosões. Com -HitLifeV=1 ou
	// -MissileBlastV=1 o raymarch fica o de antes, exato; trocar o preset ou a camada no meio da partida só vale na próxima (sem elas, a Carga
	// Pesada em escala)
	const bool bOptional = BlastFxHitLife::On(BlastFxHitLife::Bake) && BlastFxHitLife::Preset() >= 1;
	int64 OptionalBytes = 0;
	for (const TCHAR* Own : OptionalBakes)
	{
		if (!bOptional)
		{
			break;
		}
		if (!HitBakeWanted(Own))
		{
			UE_LOG(LogTemp, Display, TEXT("BlastFx raymarch: gravação dedicada %s desligada (-HitBakes=)"), Own);
			continue;
		}
		const FString File = HitBakeFile(Own);
		if (!FPaths::FileExists(File))
		{
			continue;
		}
		if (BlastRaymarch::FBakeRef B = LoadBake(File))
		{
			SourceOf.Add(FName(Own), Sources.Add(B));
			GLoadedSources.Add(FName(Own));
			OptionalBytes += B->Data.Num();
			UE_LOG(LogTemp, Display, TEXT("BlastFx raymarch: gravação dedicada %s = %s (%d³, %.1f m, %d quadros, %.2f s, até %d blocos, %.0f MB)"), Own, *File, B->Res, B->Domain,
				B->Frames, B->Duration, B->MaxBricks, double(B->Data.Num()) / (1024.0 * 1024.0));
		}
	}
	{
		// o atlas de blocos (o quadro mais cheio das gravações carregadas × 32 vagas; BlastRaymarch.h AtlasSide) e a RAM das dedicadas
		int32 MaxB = 0;
		for (const BlastRaymarch::FBakeRef& B : Sources)
		{
			MaxB = B ? FMath::Max(MaxB, B->MaxBricks) : MaxB;
		}
		const int32 Side = BlastRaymarch::AtlasSide(MaxB);
		const double AtlasMB = double(Side) * Side * Side * (BlastRaymarch::BrickA * BlastRaymarch::BrickA * BlastRaymarch::BrickA * 4
			+ BlastRaymarch::BrickF * BlastRaymarch::BrickF * BlastRaymarch::BrickF * 4) / (1024.0 * 1024.0);
		UE_LOG(LogTemp, Display, TEXT("BlastFx raymarch: custo das gravações: dedicadas %.0f MB de RAM; o atlas %d³ blocos (o maior quadro %d), %.0f MB de placa"),
			double(OptionalBytes) / (1024.0 * 1024.0), Side, MaxB, AtlasMB);
	}
	Extension = FSceneViewExtensions::NewExtension<FBlastRaymarchExtension>();
	UE_LOG(LogTemp, Display, TEXT("BlastFx raymarch: ligado (gravações %s %s %s)"),
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
	CSV_SCOPED_TIMING_STAT(BlastFxRaymarch, Tick);
	using namespace BlastRaymarch;
	if (!Extension || !Noise)
	{
		return;
	}
	++TickCount;
	FVector Eye = FVector::ZeroVector, ViewFwd = FVector::ForwardVector;
	double ViewHalf = PI;
	bool bHaveView = false;
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController(); PC && PC->PlayerCameraManager)
	{
		Eye = PC->PlayerCameraManager->GetCameraLocation();
		ViewFwd = PC->PlayerCameraManager->GetCameraRotation().Vector();
		// a meia diagonal da vista (16:9: tan × 1,15)
		ViewHalf = FMath::Atan(FMath::Tan(FMath::DegreesToRadians(0.5 * PC->PlayerCameraManager->GetFOVAngle())) * 1.15);
		bHaveView = true;
	}

	// as explosões vivas, da mais perto pra mais longe
	struct FLive { FInstance I; int32 Variant; double D2; bool bOff = false; };
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
		L.I.YawRad = E->RaymarchYawRad;
		L.I.Frame = L.I.Bake->FrameOfTime(E->BakeTime());
		L.I.NoiseExtraS = E->HoldExtraS();      // (23/10) a nuvem viva: o quadro parado, o ruído andando
		L.I.Seed = E->Seed;
		L.I.Keep = E->Opacity;
		L.I.Thickness = E->Thickness;
		L.I.SootLook = E->RaymarchSoot;
		L.I.DustLook = E->RaymarchDust;
		L.I.SunFull = E->RaymarchSunFull;
		L.I.SkyGain = E->RaymarchSkyGain;
		L.I.GroundAlbedo = E->RaymarchGround;
		L.I.bNuclear = E->bNuclear;
		L.I.EmitScale = E->RaymarchEmit;
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
		// (17/10 tarde) as fora da tela vão pro fim da fila (o render já não as desenha). (17/10 noite, o Felipe: a briga revelada
		// fora da câmera derrubava o quadro) e não pedem vaga nem sobem quadro: antes subiam ~1–3 MB por quadro cada uma, e com
		// só elas vivas o render saía antes de espalhar os blocos (o Pending crescia e caía de uma vez na próxima da tela)
		if (bHaveView)
		{
			const FVector C = L.I.FootCm + FVector(0.0, 0.0, 50.0 * L.I.Bake->Domain * L.I.Scale);
			const FVector D = C - Eye;
			const double Dist = D.Size(), R = 87.0 * L.I.Bake->Domain * L.I.Scale;
			if (Dist > R && FMath::Acos(FMath::Clamp(FVector::DotProduct(D / Dist, ViewFwd), -1.0, 1.0)) > ViewHalf + FMath::Asin(FMath::Min(R / Dist, 1.0)))
			{
				L.D2 += 1e14;
				L.bOff = true;
			}
		}
		Live.Add(MoveTemp(L));
	}
	// depuração (-ShotFxLog=1, 16/10, a moldura clara em volta da carcaça): as explosões vivas a cada 0,5 s
	{
		static const bool bLog = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("ShotFxLog="), V); return V != 0; }();
		static double LastLog = 0.0;
		const double Now = GetWorld()->GetTimeSeconds();
		if (bLog && Now - LastLog > 0.5)
		{
			LastLog = Now;
			for (TActorIterator<AExplosionVolume> It(GetWorld()); It; ++It)
			{
				const AExplosionVolume* E = *It;
				UE_LOG(LogTemp, Display, TEXT("BlastFx vivas: %s fonte %s t %.2f (idade %.2f) opac %.2f escala %.2f pé %s acabou %d entra %d"), *E->GetName(),
					*E->RaymarchSource.ToString(), E->BakeTime(), E->Age, E->Opacity, E->SizeScale, *E->GetActorLocation().ToString(),
					E->IsFinished() ? 1 : 0, Live.ContainsByPredicate([E](const FLive& L) { return L.I.FootCm == E->GetActorLocation() && L.I.Keep == E->Opacity; }) ? 1 : 0);
			}
		}
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
		if (L.bOff)
		{
			continue;
		}
		const int32 F0 = FMath::Clamp(FMath::FloorToInt32(L.I.Frame), 0, L.I.Bake->Frames - 1);
		Needed.Add(Key(L.Variant, F0));
		Needed.Add(Key(L.Variant, L.I.Bake->NextFrame(F0)));
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
		if (L.bOff)
		{
			continue;
		}
		const int32 F0 = FMath::Clamp(FMath::FloorToInt32(L.I.Frame), 0, L.I.Bake->Frames - 1);
		L.I.Slot0 = SlotFor(L.Variant, F0);
		L.I.Slot1 = SlotFor(L.Variant, L.I.Bake->NextFrame(F0));
		if (L.I.Slot0 >= 0 && L.I.Slot1 >= 0)
		{
			List.Add(L.I);
		}
	}
	// a poeira das lagartas em volume (16/10 noite; RTSDustField, a Arte) também é desenhada pelo raymarch e faz sombra no
	// chão por este mapa (no modo volume a Arte esconde a sombra própria dela): sem explosão, o passe e a sombra seguem
	// rodando enquanto houver poeira
	const bool bDust = RTSDustField::GetStats().X > 0;
	// (23/10) e o rastro do míssil (RTSSmokeShadow): a sombra dele entra no mesmo mapa, então o mapa e o passe seguem rodando
	// enquanto houver puff de rastro, mesmo sem explosão nem poeira
	const bool bTrail = RTSSmokeShadow::NumPuffs_GameThread() > 0;
	if (List.IsEmpty() && !bDust && !bTrail && bWasEmpty)
	{
		return;
	}
	bWasEmpty = List.IsEmpty() && !bDust && !bTrail;
	if (UploadLog())
	{
		// (23/10 noite) o que sobe pra placa neste quadro (os bytes do quadro na gravação: a tabela, o macro e os blocos das duas resoluções)
		static double WinStart = 0.0, WinMB = 0.0, WinPeak = 0.0, WinDt = 0.0, WinDtMax = 0.0;
		static int32 WinFrames = 0, WinTicks = 0, WinLive = 0;
		WinDt += DeltaTime;
		WinDtMax = FMath::Max(WinDtMax, double(DeltaTime));
		double MB = 0.0;
		for (const FUpload& U : Uploads)
		{
			if (!U.bEvict && U.Bake)
			{
				const int64 End = U.Frame + 1 < U.Bake->Frames ? U.Bake->Offsets[U.Frame + 1] : U.Bake->Data.Num();
				MB += double(End - U.Bake->Offsets[U.Frame]) / (1024.0 * 1024.0);
				++WinFrames;
			}
		}
		WinMB += MB;
		WinPeak = FMath::Max(WinPeak, MB);
		WinLive = FMath::Max(WinLive, List.Num());
		++WinTicks;
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now - WinStart >= 1.0)
		{
			UE_LOG(LogTemp, Display, TEXT("BlastFx raymarch subida: %d quadros em %d ticks, %.1f MB (%.2f MB/tick, pior %.2f MB), até %d explosões, blocos no atlas %d; quadro do jogo %.1f ms (pior %.1f)"),
				WinFrames, WinTicks, WinMB, WinMB / FMath::Max(WinTicks, 1), WinPeak, WinLive, UsedBricks, 1000.0 * WinDt / FMath::Max(WinTicks, 1), 1000.0 * WinDtMax);
			WinStart = Now;
			WinMB = WinPeak = WinDt = WinDtMax = 0.0;
			WinFrames = WinTicks = WinLive = 0;
		}
	}
	SubmitFog();
	SubmitShadow(!List.IsEmpty() || bDust || bTrail);
	Extension->Submit_GameThread(MoveTemp(List), MoveTemp(Uploads), Noise->GetResource(), Worley ? Worley->GetResource() : nullptr, PoolRes, MaxBricks);
}

void UBlastRaymarchSubsystem::SubmitShadow(bool bAny)
{
	// A sombra da explosão no chão (13/10), nos mapas sem o ambiente do BlastLab (lá o sol já tem a sombra da fumaça de
	// campo, BattleSmokeSubsystem): a mesma light function (M_SmokeShadowLF) num mapa de sombra próprio. Só entra num sol
	// sem light function. -RmShadow=0 desliga.
	constexpr int32 Res = 256;
	constexpr float TexelM = 0.75f;
	static const int32 ShadowCfg = [] { int32 V = 1; FParse::Value(FCommandLine::Get(), TEXT("RmShadow="), V); return V; }();
	if (!bShadowSetup)
	{
		bShadowSetup = true;
		bool bLabEnv = false;
		for (TActorIterator<AActor> It(GetWorld()); It && !bLabEnv; ++It)
		{
			bLabEnv = It->GetClass()->GetName() == TEXT("BlastLabEnvironment");
		}
		UMaterialInterface* Function = ShadowCfg && !bLabEnv ? LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_SmokeShadowLF.M_SmokeShadowLF")) : nullptr;
		UDirectionalLightComponent* Best = nullptr;
		for (TActorIterator<ADirectionalLight> It(GetWorld()); It && Function; ++It)
		{
			UDirectionalLightComponent* C = Cast<UDirectionalLightComponent>(It->GetLightComponent());
			if (C && C->IsVisible() && (!Best || (C->bAtmosphereSunLight && C->AtmosphereSunLightIndex == 0) || C->Intensity > Best->Intensity))
			{
				Best = C;
			}
		}
		if (Function && Best && !Best->LightFunctionMaterial)
		{
			ShadowTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("BlastRaymarchShadow"));
			ShadowTarget->bCanCreateUAV = true;
			ShadowTarget->ClearColor = FLinearColor::Black;
			// (23/10) o alvo final em 512² (texel de 0,375 m na mesma janela de 192 m): a sombra do rastro do míssil (σ ~0,4 m) a
			// 0,75 m dobrava ~25% da gaussiana (e^(−π²σ²/2Δ²)) e tremia andando. A marcha das explosões segue em 256² (o mapa de
			// passagem); o ShadowMap da light function é 1/(janela), igual em qualquer resolução
			ShadowTarget->InitCustomFormat(RTSSmokeShadow::OutRes, RTSSmokeShadow::OutRes, PF_R32_FLOAT, true);
			ShadowTarget->UpdateResourceImmediate(true);
			ShadowFunction = UMaterialInstanceDynamic::Create(Function, this);
			ShadowFunction->SetTextureParameterValue(TEXT("Shadow"), ShadowTarget);
			ShadowFunction->SetVectorParameterValue(TEXT("ShadowLight"), FLinearColor(0.0f, 0.0f, 1.0f, 0.0f));
			// a sombra das nuvens (16/10, o ZH, pedido do Felipe pela sessão do Look): a força vem da receita do mapa
			// (LookDirector, no MID deste sol); -CloudShadow= força pro teste
			static const float CloudCfg = [] { float V = -1.0f; FParse::Value(FCommandLine::Get(), TEXT("CloudShadow="), V); return V; }();
			if (CloudCfg >= 0.0f)
			{
				ShadowFunction->SetScalarParameterValue(TEXT("CloudShadowStrength"), CloudCfg);
			}
			Best->SetLightFunctionMaterial(ShadowFunction);
			// sem isto o sol cai à metade além de 1 km da câmera (DisabledBrightness 0,5 e FadeDistance 1000 m, os
			// padrões da Unreal pra light function; pesquisa de 13/10, LightComponent.cpp)
			Best->DisabledBrightness = 1.0f;
			Best->LightFunctionFadeDistance = 1.0e7f;
			Best->MarkRenderStateDirty();
			ShadowSun = Best;
			UE_LOG(LogTemp, Display, TEXT("BlastFx raymarch: a sombra das explosões no sol %s"), *Best->GetOwner()->GetName());
		}
	}
	if (!ShadowFunction || !ShadowSun.IsValid())
	{
		return;
	}
	// a janela em volta do ponto do chão pra onde a câmera olha, presa à grade dos texels
	FVector Center = FVector::ZeroVector;
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController(); PC && PC->PlayerCameraManager)
	{
		const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
		const FVector Fwd = PC->PlayerCameraManager->GetCameraRotation().Vector();
		if (!BlastFxGround::Ray(GetWorld(), Eye, Fwd, 1.0e6, Center))
		{
			Center = Eye + FVector(Fwd.X, Fwd.Y, 0.0).GetSafeNormal() * 10000.0;
			Center.Z = 0.0;
		}
	}
	FVector ToSun = -ShadowSun->GetDirection();
	// (17/10 noite, as faixas cinza em degrau que o Felipe viu atrás do Patriot: a areia do tiro e do acerto com o sol a 8° do céu
	// "uma luz só") a sombra dos volumes usa no mínimo 20° de elevação, no mesmo rumo do sol: com o sol baixo de verdade ela
	// esticava ~7× a altura, atravessava a base em faixas e mostrava os degraus da grade grossa que a marcha pula (MacroEmpty); a
	// sombra das nuvens já é assim (a mancha no rumo do sol, M_SmokeShadowLF). -RmShadowMinElev= (graus) pro A/B; 0 = o sol de verdade
	static const double MinElevDeg = [] { float V = 20.0f; FParse::Value(FCommandLine::Get(), TEXT("RmShadowMinElev="), V); return double(V); }();
	const double MinZ = FMath::Sin(FMath::DegreesToRadians(MinElevDeg));
	if (ToSun.Z > 0.0 && ToSun.Z < MinZ)
	{
		const FVector2D Across = FVector2D(ToSun.X, ToSun.Y).GetSafeNormal() * FMath::Sqrt(1.0 - MinZ * MinZ);
		ToSun = FVector(Across.X, Across.Y, MinZ);
	}
	const bool bOn = bAny && ToSun.Z > 0.05;
	const float HalfM = 0.5f * Res * TexelM;
	FBlastRaymarchExtension::FShadow S;
	S.Res = Res;
	S.TexelM = TexelM;
	S.GroundZM = float(Center.Z / 100.0);
	S.OriginM = FVector2D(FMath::GridSnap(Center.X / 100.0, double(TexelM)) - HalfM, FMath::GridSnap(Center.Y / 100.0, double(TexelM)) - HalfM);
	S.ToSun = FVector3f(ToSun);
	S.SunHalfAngleRad = FMath::DegreesToRadians(0.5f * ShadowSun->LightSourceAngle);   // (23/10) a penumbra do rastro (o disco do sol)
	S.Out = bOn ? ShadowTarget->GameThread_GetRenderTargetResource() : nullptr;
	ShadowFunction->SetVectorParameterValue(TEXT("ShadowMap"), FLinearColor(float(S.OriginM.X), float(S.OriginM.Y), 1.0f / (Res * TexelM), S.GroundZM));
	ShadowFunction->SetVectorParameterValue(TEXT("ShadowLight"), FLinearColor(ToSun.X, ToSun.Y, ToSun.Z, bOn ? 1.0f : 0.0f));
	Extension->SetShadow_GameThread(S);
}

void UBlastRaymarchSubsystem::SubmitFog()
{
	// A névoa do mapa na explosão (13/10): nos mapas do look (sem o ambiente do BlastLab, que tem a névoa do WebGL
	// calibrada à parte). -RmFog=0|1 força.
	static const int32 FogCfg = [] { int32 V = -1; FParse::Value(FCommandLine::Get(), TEXT("RmFog="), V); return V; }();
	if (!FogComp.IsValid() && !bFogSearched)
	{
		bFogSearched = true;
		bool bLabEnv = false;
		for (TActorIterator<AActor> It(GetWorld()); It && !bLabEnv; ++It)
		{
			bLabEnv = It->GetClass()->GetName() == TEXT("BlastLabEnvironment");
		}
		if (FogCfg == 1 || (FogCfg != 0 && !bLabEnv))
		{
			for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It)
			{
				FogComp = It->GetComponent();
				break;
			}
		}
	}
	FBlastRaymarchExtension::FFog F;
	if (const UExponentialHeightFogComponent* C = FogComp.Get())
	{
		const bool bVol = C->bEnableVolumetricFog;
		F.bOn = true;
		F.DensityPerM = C->FogDensity / 1000.0f * 100.0f * (bVol ? C->VolumetricFogExtinctionScale : 1.0f);
		F.FalloffPerM = C->FogHeightFalloff / 1000.0f * 100.0f;
		F.HeightCm = float(C->GetComponentLocation().Z);
		F.StartM = (bVol ? C->VolumetricFogStartDistance : C->StartDistance) * 0.01f;
		const FLinearColor Alb = bVol ? FLinearColor(C->VolumetricFogAlbedo) : C->FogInscatteringLuminance;
		F.Albedo = FVector3f(Alb.R, Alb.G, Alb.B);
		F.G = bVol ? C->VolumetricFogScatteringDistribution : 0.0f;
	}
	Extension->SetFog_GameThread(F);
}

float UBlastRaymarchSubsystem::SourceDuration(FName Source) const
{
	// a duração (s da gravação) de uma gravação própria: o diretor tira de cena a poeira do tiro quando ela acaba
	const int32* S = SourceOf.Find(Source);
	if (!S || !Sources.IsValidIndex(*S) || !Sources[*S])
	{
		return 0.0f;
	}
	return Sources[*S]->TimeOfFrame(float(Sources[*S]->Frames - 1));
}
