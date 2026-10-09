#include "FXWreckFire.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

namespace
{
	// as mesmas constantes do fxfire.js
	constexpr int32 MaxCards = 160;
	constexpr double LickLife = 0.5;      // s
	constexpr int32 LickSim = 31;         // a lambida toca até este quadro da simulação (a bola ainda amarela)
	constexpr double SmokeCool = 0.35;    // s: a brasa que sobra na casca apaga quando vira fumaça

	double SmoothStep(double X, double Lo, double Hi)
	{
		if (X <= Lo) return 0.0;
		if (X >= Hi) return 1.0;
		const double K = (X - Lo) / (Hi - Lo);
		return K * K * (3.0 - 2.0 * K);
	}

	FString RepoDir()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT(".."));
	}

	TSharedPtr<FJsonObject> ReadJson(const FString& Path)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			return nullptr;
		}
		TSharedPtr<FJsonObject> Obj;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		return FJsonSerializer::Deserialize(Reader, Obj) ? Obj : nullptr;
	}
}

bool FFXPreset::Load(const FString& Path)
{
	const TSharedPtr<FJsonObject> J = ReadJson(Path);
	if (!J)
	{
		return false;
	}
	J->TryGetStringField(TEXT("bake"), Bake);
	const TPair<const TCHAR*, double*> Fields[] = {
		{ TEXT("interval"), &Interval }, { TEXT("scale"), &Scale }, { TEXT("speed"), &Speed }, { TEXT("rise"), &Rise },
		{ TEXT("smokeLife"), &SmokeLife }, { TEXT("smokeGrow"), &SmokeGrow }, { TEXT("density"), &Density },
		{ TEXT("erode"), &Erode }, { TEXT("emberCut"), &EmberCut }, { TEXT("albedo"), &Albedo }, { TEXT("shadow"), &Shadow },
		{ TEXT("lickInterval"), &LickInterval }, { TEXT("baseR"), &BaseR }, { TEXT("fireLight"), &FireLight },
		{ TEXT("emit"), &Emit }, { TEXT("sun"), &Sun }, { TEXT("sky"), &Sky }, { TEXT("sunElev"), &SunElev },
		{ TEXT("sunAz"), &SunAz }, { TEXT("wind"), &Wind }, { TEXT("exposure"), &Exposure }, { TEXT("ueBloom"), &UeBloom },
	};
	for (const auto& F : Fields)
	{
		J->TryGetNumberField(F.Key, *F.Value);
	}
	// teste com outra luz sem mexer no ajuste aprovado: -SunElev=30 -Sun=5 (o ?sunElev=&sun= do laboratório)
	FParse::Value(FCommandLine::Get(), TEXT("-SunElev="), SunElev);
	FParse::Value(FCommandLine::Get(), TEXT("-Sun="), Sun);
	return true;
}

FVector FFXPreset::SunDir() const
{
	const double El = FMath::DegreesToRadians(SunElev), Az = FMath::DegreesToRadians(SunAz);
	return FVector(FMath::Sin(Az) * FMath::Cos(El), -FMath::Cos(Az) * FMath::Cos(El), FMath::Sin(El));
}

bool FFXBakeMeta::Load(const FString& Path)
{
	const TSharedPtr<FJsonObject> J = ReadJson(Path);
	if (!J)
	{
		return false;
	}
	Cols = J->GetIntegerField(TEXT("cols"));
	Rows = J->GetIntegerField(TEXT("rows"));
	Frames = J->GetIntegerField(TEXT("frames"));
	FireEnd = J->GetIntegerField(TEXT("fire_end"));
	SideM = J->GetNumberField(TEXT("side_m"));
	const TArray<TSharedPtr<FJsonValue>>& Times = J->GetArrayField(TEXT("times_s"));
	LastTime = Times.Num() ? Times.Last()->AsNumber() : 1.0;
	const TArray<TSharedPtr<FJsonValue>>& Pv = J->GetArrayField(TEXT("pivot"));
	Pivot = FVector2D(Pv[0]->AsNumber(), Pv[1]->AsNumber());
	BallBottom.Reset();
	for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("ball_bottom")))
	{
		BallBottom.Add(V->AsNumber());
	}
	SimFrames.Reset();
	for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("sim_frames")))
	{
		SimFrames.Add(int32(V->AsNumber()));
	}
	return BallBottom.Num() == Frames && SimFrames.Num() == Frames;
}

AFXWreckFire::AFXWreckFire()
{
	PrimaryActorTick.bCanEverTick = true;
	// depois da câmera do quadro: os cartões viram pra câmera desta imagem
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	Cards = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Cards"));
	RootComponent = Cards;
	Cards->SetCastShadow(false);
	Cards->bUseAsyncCooking = false;
	FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLight"));
	FireLight->SetupAttachment(Cards);
	FireLight->SetRelativeLocation(FVector(0.0, 0.0, 120.0));        // 1.2 m acima do anel, como no laboratório
	FireLight->SetIntensityUnits(ELightUnits::Candelas);
	FireLight->SetIntensity(0.0f);
	FireLight->SetAttenuationRadius(3000.0f);
	FireLight->bUseInverseSquaredFalloff = true;
	FireLight->SetLightColor(FColor(255, 138, 58));
	FireLight->SetCastShadows(false);
}

void AFXWreckFire::BeginPlay()
{
	Super::BeginPlay();
	Hatch = GetActorLocation() / 100.0;
	Light = FFXSceneLight::Find(GetWorld());
	LoadPreset(true);
	FString Fx;
	FParse::Value(FCommandLine::Get(), TEXT("-Fx="), Fx);
	if (Fx == TEXT("impact"))
	{
		// o teste é do tiro no chão: o fogo da carcaça não aparece nem fotografa
		SetActorHiddenInGame(true);
		SetActorTickEnabled(false);
		return;
	}
	FString At;
	if (FParse::Value(FCommandLine::Get(), TEXT("-Shot="), ShotName) && FParse::Value(FCommandLine::Get(), TEXT("-At="), At, false))     // false: a vírgula não corta a lista
	{
		TArray<FString> Items;
		At.ParseIntoArray(Items, TEXT(","));
		for (const FString& S : Items)
		{
			ShotAt.Add(FCString::Atod(*S));
		}
	}
	Restart();
}

void AFXWreckFire::Restart()
{
	CardList.Reset();
	RngState = uint32(Seed);
	SpawnClock = LickClock = SimTime = 0.0;
}

void AFXWreckFire::LoadPreset(bool bForce)
{
	const FString Path = FPaths::ProjectDir() / TEXT("preset.json");
	const FDateTime Stamp = IFileManager::Get().GetTimeStamp(*Path);
	if (!bForce && Stamp == PresetStamp)
	{
		return;
	}
	PresetStamp = Stamp;
	if (!P.Load(Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("FXLab: não li %s; fica o ajuste padrão"), *Path);
	}
	if (P.Bake != LoadedBake)
	{
		LoadBake();
	}
	ApplyPreset();
	UE_LOG(LogTemp, Display, TEXT("FXLab: ajuste carregado (%s)"), *P.Bake);
}

void AFXWreckFire::LoadBake()
{
	LoadedBake = P.Bake;
	const FString MetaPath = RepoDir() / TEXT("captures/fxlab/bake") / P.Bake / TEXT("atlas.json");
	if (!Meta.Load(MetaPath))
	{
		UE_LOG(LogTemp, Error, TEXT("FXLab: falta o atlas.json com as medidas (rode atlas_meta.py): %s"), *MetaPath);
	}
	Fps = (Meta.Frames - 1) / FMath::Max(Meta.LastTime, 1e-3);
	// quadro da lambida: o da simulação mais perto de LickSim (o primeiro, no empate)
	LickEnd = 0;
	for (int32 i = 1; i < Meta.SimFrames.Num(); ++i)
	{
		if (FMath::Abs(Meta.SimFrames[i] - LickSim) < FMath::Abs(Meta.SimFrames[LickEnd] - LickSim))
		{
			LickEnd = i;
		}
	}
	if (!Mid && CardMaterial)
	{
		Mid = UMaterialInstanceDynamic::Create(CardMaterial, this);
		Cards->SetMaterial(0, Mid);
	}
	if (Mid)
	{
		const FString Dir = FString::Printf(TEXT("/Game/FX/%s/"), *P.Bake);
		const TPair<const TCHAR*, const TCHAR*> Tex[] = {
			{ TEXT("SixPos"), TEXT("T_SixPos") }, { TEXT("SixNeg"), TEXT("T_SixNeg") }, { TEXT("EmitTex"), TEXT("T_Emit") } };
		for (const auto& T : Tex)
		{
			UTexture* Texture = LoadObject<UTexture>(nullptr, *(Dir + T.Value + TEXT(".") + T.Value));
			if (!Texture)
			{
				UE_LOG(LogTemp, Error, TEXT("FXLab: falta a textura %s%s (fxlab.ps1 setup)"), *Dir, T.Value);
			}
			Mid->SetTextureParameterValue(T.Key, Texture);
		}
		Mid->SetVectorParameterValue(TEXT("Grid"), FLinearColor(Meta.Cols, Meta.Rows, Meta.Frames, 0.0f));
	}
}

void AFXWreckFire::ApplyPreset()
{
	const FVector Sd = P.SunDir();
	if (Mid)
	{
		Mid->SetVectorParameterValue(TEXT("SunCol"), FLinearColor::FromSRGBColor(FColor(255, 198, 140)) * float(P.Sun));
		Mid->SetVectorParameterValue(TEXT("SkyCol"), FLinearColor::FromSRGBColor(FColor(159, 180, 208)) * float(P.Sky));
		Mid->SetScalarParameterValue(TEXT("EmitGain"), P.Emit);
		Mid->SetScalarParameterValue(TEXT("Density"), P.Density);
		Mid->SetScalarParameterValue(TEXT("EmberCut"), P.EmberCut);
		Mid->SetScalarParameterValue(TEXT("Albedo"), P.Albedo);
		// no mapa do jogo: o sol e o céu dele (o material lê), o fogo na escala da exposição dele
		Mid->SetScalarParameterValue(TEXT("SceneLight"), Light.bGame ? 1.0f : 0.0f);
		Mid->SetScalarParameterValue(TEXT("EmitScale"), float(Light.EmitScale));
	}
	if (Light.bGame)
	{
		return;                        // a luz, o céu e o pós do mapa são do jogo: não mexe
	}
	// o sol da cena e o céu acompanham o ajuste (o mesmo sol que ilumina os cartões)
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		// móvel: luz fixa ignora o giro (o sol ficava rente ao chão)
		It->GetRootComponent()->SetMobility(EComponentMobility::Movable);
		It->SetActorRotation((-Sd).Rotation());
		if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(It->GetLightComponent()))
		{
			L->SetIntensity(P.Sun);
			L->SetLightColor(FColor(255, 198, 140));
		}
	}
	for (TActorIterator<APostProcessVolume> It(GetWorld()); It; ++It)
	{
		It->Settings.bOverride_BloomIntensity = true;
		It->Settings.BloomIntensity = float(P.UeBloom);
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("Sky")))
		{
			if (UStaticMeshComponent* M = It->FindComponentByClass<UStaticMeshComponent>())
			{
				UMaterialInstanceDynamic* SkyMid = Cast<UMaterialInstanceDynamic>(M->GetMaterial(0));
				if (!SkyMid)
				{
					SkyMid = M->CreateAndSetMaterialInstanceDynamic(0);
				}
				if (SkyMid)
				{
					SkyMid->SetVectorParameterValue(TEXT("SunDir"), FLinearColor(Sd.X, Sd.Y, Sd.Z, 0.0f));
				}
			}
		}
	}
}

// mulberry32, igual ao fxfire.js (inteiros de 32 bits)
double AFXWreckFire::Rand()
{
	RngState += 0x6D2B79F5u;
	uint32 T = RngState;
	T = (T ^ (T >> 15)) * (T | 1u);
	T ^= T + (T ^ (T >> 7)) * (T | 61u);
	return double(T ^ (T >> 14)) / 4294967296.0;
}

double AFXWreckFire::Rnd(double A, double B)
{
	return A + Rand() * (B - A);
}

double AFXWreckFire::FireDur() const
{
	return Meta.FireEnd / (Fps * P.Speed);
}

void AFXWreckFire::SpawnPuff()
{
	if (CardList.Num() >= MaxCards)
	{
		return;
	}
	// uma chamada por linha: a ordem dos sorteios é a do laboratório
	const double X = Rnd(-0.35, 0.35);
	const double Y = Rnd(-0.35, 0.35);
	FFXCard C;
	C.P = Hatch + FVector(X, Y, 0.0);
	C.Rot = Rnd(-0.12, 0.12);
	C.RotV = Rnd(-0.06, 0.06);
	C.Flip = Rand() < 0.5 ? -1.0 : 1.0;
	C.Size = Rnd(0.85, 1.15);
	CardList.Add(C);
}

void AFXWreckFire::SpawnLick()
{
	if (CardList.Num() >= MaxCards)
	{
		return;
	}
	const double A = Rnd(0.0, PI * 2.0);
	const double R = Rnd(0.0, P.BaseR);
	const double Dz = Rnd(-0.3, 0.1);
	FFXCard C;
	C.P = Hatch + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Dz);
	C.Rot = Rnd(-0.2, 0.2);
	C.Flip = Rand() < 0.5 ? -1.0 : 1.0;
	C.Size = Rnd(0.4, 0.6);
	C.bLick = true;
	CardList.Add(C);
}

void AFXWreckFire::Step(double Dt, const FVector& CamM)
{
	SpawnClock += Dt;
	while (SpawnClock > P.Interval * Rnd(0.75, 1.25))
	{
		SpawnClock = 0.0;
		SpawnPuff();
	}
	LickClock += Dt;
	while (LickClock > P.LickInterval)
	{
		LickClock -= P.LickInterval * Rnd(0.6, 1.4);
		SpawnLick();
	}
	const double Fd = FireDur();
	for (int32 i = CardList.Num() - 1; i >= 0; --i)
	{
		FFXCard& C = CardList[i];
		C.Age += Dt;
		const double Life = C.bLick ? LickLife : Fd + P.SmokeLife;
		if (C.Age > Life)
		{
			CardList.RemoveAt(i);          // mantém a ordem (o splice do laboratório)
			continue;
		}
		if (C.bLick)
		{
			C.P.Z += 1.2 * Dt;
			continue;
		}
		C.P.Z += P.Rise * (C.Age < Fd ? 1.0 : 0.7) * Dt;
		C.P.X += P.Wind * FMath::Min(1.0, (C.P.Z - Hatch.Z) / 8.0) * Dt;
		C.Rot += C.RotV * Dt;
	}
	// de trás pra frente (alfa pré-multiplicado em ordem); ordenação estável como a do JavaScript
	CardList.StableSort([&CamM](const FFXCard& A, const FFXCard& B)
	{
		return FVector::DistSquared(A.P, CamM) > FVector::DistSquared(B.P, CamM);
	});
}

void AFXWreckFire::Draw(const FVector& CamM, const FVector& CamRight, const FVector& CamUp, const FVector& CamForward)
{
	const double Fd = FireDur();
	const double Side = Meta.SideM * P.Scale;
	const int32 N = Meta.Frames, Fe = Meta.FireEnd;
	// sombra entre cartões: quanto de fumaça dos outros fica entre o cartão e o sol
	const FVector Sd = SunDirection();
	for (FFXCard& C : CardList)
	{
		C.Rad = C.bLick ? 0.0 : 0.28 * Side * C.Size * (1.0 + P.SmokeGrow * FMath::Max(0.0, C.Age - Fd));
	}
	for (FFXCard& C : CardList)
	{
		double T = 1.0;
		for (const FFXCard& Q : CardList)
		{
			if (&Q == &C || Q.Rad <= 0.0)
			{
				continue;
			}
			const FVector D = Q.P - C.P;
			const double Along = FVector::DotProduct(D, Sd);
			if (Along <= 0.0)
			{
				continue;
			}
			const double Perp = FMath::Sqrt(FMath::Max(0.0, D.SizeSquared() - Along * Along));
			if (Perp >= Q.Rad)
			{
				continue;
			}
			T *= 1.0 - P.Shadow * (1.0 - Perp / Q.Rad);
		}
		C.T = T;
	}

	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0, UV1, UV2, UV3;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	const int32 Num = CardList.Num();
	Verts.Reserve(Num * 4);
	Tris.Reserve(Num * 6);
	double Heat = 0.0;
	const FVector ToCam = -CamForward;
	const FVector Origin = GetActorLocation();
	static const FVector2D Corners[4] = { { -0.5, -0.5 }, { 0.5, -0.5 }, { 0.5, 0.5 }, { -0.5, 0.5 } };
	for (const FFXCard& C : CardList)
	{
		double Size, Frame, Alpha, Emit, T, Erode;
		if (C.bLick)
		{
			const double K = C.Age / LickLife;
			Size = Side * C.Size;
			Frame = K * LickEnd;
			Alpha = FMath::Min(1.0, C.Age / 0.05) * (1.0 - SmoothStep(K, 0.5, 1.0));
			Emit = 1.0;
			T = 1.0;
			Erode = 0.0;
			Heat += 0.3;
		}
		else
		{
			const double Ts = FMath::Max(0.0, C.Age - Fd), Life = Fd + P.SmokeLife, U = FMath::Min(1.0, Ts / P.SmokeLife);
			// fogo: o relógio da gravação; fumaça: o resto da gravação (a nuvem se abrindo) espalhado na vida da fumaça
			Frame = C.Age < Fd ? C.Age * P.Speed * Fps : Fe + (N - 1 - Fe) * U;
			Size = Side * C.Size * (1.0 + P.SmokeGrow * Ts);
			Alpha = FMath::Min(1.0, C.Age / 0.08) * (1.0 - SmoothStep(C.Age, Life - 0.15 * P.SmokeLife, Life));
			Emit = 1.0 - SmoothStep(Ts, 0.0, SmokeCool);
			T = C.T;
			Erode = P.Erode * SmoothStep(U, 0.4, 1.0);
			if (C.Age < Fd)
			{
				Heat += FMath::Max(0.0, 1.0 - Frame / Fe);
			}
		}
		const int32 F0 = FMath::Clamp(FMath::FloorToInt32(Frame), 0, N - 1), F1 = FMath::Min(F0 + 1, N - 1);
		const double Ft = Frame - F0;
		const double BallBottom = Meta.BallBottom.IsValidIndex(F1) ? FMath::Lerp(Meta.BallBottom[F0], Meta.BallBottom[F1], Ft) : 0.0;
		// a base do cartão gira no plano da tela; espelhado, a direita gravada vira a esquerda
		const double Cs = FMath::Cos(C.Rot), Sn = FMath::Sin(C.Rot);
		const FVector R = Cs * CamRight + Sn * CamUp, Up = -Sn * CamRight + Cs * CamUp;
		const FVector L(FVector::DotProduct(Sd, R) * C.Flip, FVector::DotProduct(Sd, Up), FVector::DotProduct(Sd, ToCam));
		const FLinearColor Lc(L.X * 0.5 + 0.5, L.Y * 0.5 + 0.5, L.Z * 0.5 + 0.5, 1.0f);
		const FVector2D Pivot(C.Flip > 0 ? Meta.Pivot.X - 0.5 : 0.5 - Meta.Pivot.X, Meta.Pivot.Y - 0.5);
		const int32 Base = Verts.Num();
		for (const FVector2D& Q : Corners)
		{
			const FVector2D Qp = Q - Pivot;                 // o pivô (a bola no 1º quadro) fica no ponto do cartão
			const FVector World = C.P + (R * Qp.X + Up * Qp.Y) * Size;
			Verts.Add(World * 100.0 - Origin);
			Normals.Add(ToCam);
			UV0.Add(FVector2D(C.Flip < 0 ? 0.5 - Q.X : Q.X + 0.5, Q.Y + 0.5));
			UV1.Add(FVector2D(Frame, Alpha));
			UV2.Add(FVector2D(Emit, BallBottom));
			UV3.Add(FVector2D(Erode, T));
			Colors.Add(Lc);
		}
		Tris.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
	}
	Cards->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UV0, UV1, UV2, UV3, Colors, Tangents, false, false);
	FireLight->SetIntensity(float(P.FireLight * Heat * Light.EmitScale));
}

void AFXWreckFire::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	PresetCheck += DeltaSeconds;
	if (PresetCheck > 1.0 && ShotName.IsEmpty())
	{
		PresetCheck = 0.0;
		LoadPreset(false);             // o laboratório salvou um ajuste novo: pega sozinho
	}
	const APlayerCameraManager* Pcm = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Pcm || Meta.Frames <= 1)
	{
		return;
	}
	const FVector CamM = Pcm->GetCameraLocation() / 100.0;
	const FRotationMatrix Rm(Pcm->GetCameraRotation());
	// captura: o passo do laboratório (1/30 exato, em double), pra a história bater sorteio a sorteio
	const double Dt = ShotName.IsEmpty() ? DeltaSeconds * TimeScale : 1.0 / 30.0;
	if (!bPaused)
	{
		Step(Dt, CamM);
		SimTime += Dt;
	}
	Draw(CamM, Rm.GetScaledAxis(EAxis::Y), Rm.GetScaledAxis(EAxis::Z), Rm.GetScaledAxis(EAxis::X));
	Capture();
}

void AFXWreckFire::Capture()
{
	if (ShotName.IsEmpty() || Light.bGame)          // no mapa do jogo quem fotografa é o controlador
	{
		return;
	}
	if (ShotAt.Num() && SimTime >= ShotAt[0] - 1e-6)
	{
		const FString Path = RepoDir() / TEXT("captures/fxlab/shots_ue") / FString::Printf(TEXT("%s_%.2fs.png"), *ShotName, ShotAt[0]);
		FScreenshotRequest::RequestScreenshot(Path, false, false);
		UE_LOG(LogTemp, Display, TEXT("FXLab: foto %s"), *Path);
		ShotAt.RemoveAt(0);
		if (!ShotAt.Num())
		{
			ExitIn = 6;                // espera a última foto sair do quadro
		}
	}
	if (ExitIn > 0 && --ExitIn == 0)
	{
		FGenericPlatformMisc::RequestExit(false);
	}
}
