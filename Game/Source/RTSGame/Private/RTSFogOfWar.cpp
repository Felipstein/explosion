#include "RTSFogOfWar.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "TextureResource.h"

namespace
{
	/** Os níveis da tela do ZH (GlobalData: ShroudAlpha 0, FogAlpha 127, ClearAlpha 255). */
	constexpr float NeverSeenLevel = 0.0f;
	constexpr float ExploredLevel = 127.0f / 255.0f;
	constexpr float VisibleLevel = 1.0f;
	/** W3DShroud FOG_INTERPOLATION_RATE: 255 por segundo na escala de 0 a 255. */
	constexpr float LevelPerSecond = 1.0f;
	/** O minimapa refaz a névoa no máximo 10 vezes por segundo. */
	constexpr double ComposeEverySeconds = 0.1;
	/** Altura da câmera da foto acima do chão mais alto possível (o mapa vai até ~70 m). */
	constexpr float CaptureHeightCm = 50000.0f;
	/** Corte de distância das plantas pequenas (100–300 m no WorldForge) multiplicado na foto: a câmera fica longe. */
	constexpr float CaptureViewDistanceScale = 20.0f;

	TUniquePtr<FUpdateTextureRegion2D> FullRegion(int32 W, int32 H)
	{
		return MakeUnique<FUpdateTextureRegion2D>(0, 0, 0, 0, uint32(W), uint32(H));
	}

	UTexture2D* MakeTexture(int32 W, int32 H, const TCHAR* Name)
	{
		UTexture2D* T = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8, FName(Name));
		T->SRGB = true;
		T->Filter = TF_Bilinear;
		T->AddressX = TA_Clamp;
		T->AddressY = TA_Clamp;
		T->NeverStream = true;
		T->UpdateResource();
		return T;
	}

	/** Manda bytes BGRA pra textura (o render thread guarda o ponteiro: o buffer é novo e é apagado lá). */
	void Upload(UTexture2D* T, const TArray<FColor>& Pixels, int32 W, int32 H)
	{
		if (!T || Pixels.Num() != W * H)
		{
			return;
		}
		uint8* Data = new uint8[size_t(W) * size_t(H) * 4];
		FMemory::Memcpy(Data, Pixels.GetData(), size_t(W) * size_t(H) * 4);
		FUpdateTextureRegion2D* Region = FullRegion(W, H).Release();
		T->UpdateTextureRegions(0, 1, Region, uint32(W * 4), 4, Data,
			[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
			{
				delete[] SrcData;
				delete Regions;
			});
	}

	float AcesFilm(float X)
	{
		// a aproximação do ACES de Narkowicz (2015): a base da curva Filmic da Unreal
		X = FMath::Max(X, 0.0f);
		return FMath::Clamp((X * (2.51f * X + 0.03f)) / (X * (2.43f * X + 0.59f) + 0.14f), 0.0f, 1.0f);
	}

	float LinearToSRGB(float C)
	{
		return C <= 0.0031308f ? C * 12.92f : 1.055f * FMath::Pow(C, 1.0f / 2.4f) - 0.055f;
	}
}

void URTSFogOfWar::Init(int32 InWidth, int32 InHeight, const FVector2D& InMinCm, const FVector2D& InSizeCm)
{
	W = FMath::Max(InWidth, 1);
	H = FMath::Max(InHeight, 1);
	MinCm = InMinCm;
	SizeCm = FVector2D(FMath::Max(InSizeCm.X, 1.0), FMath::Max(InSizeCm.Y, 1.0));
	Level.Init(0.0f, W * H);
	Raw.Init(0, W * H);
	bHasLevels = false;
	Vision = MakeTexture(W, H, TEXT("RTS_FogVision"));
	Minimap = MakeTexture(MinimapSide, MinimapSide, TEXT("RTS_Minimap"));
	MinimapBase.Init(FLinearColor(0.04f, 0.04f, 0.045f), MinimapSide * MinimapSide);
	bMinimapReady = false;
	bMinimapDirty = true;
}

int32 URTSFogOfWar::CellIndex(const FVector2D& Cm) const
{
	const int32 X = FMath::FloorToInt32((Cm.X - MinCm.X) / SizeCm.X * W);
	const int32 Y = FMath::FloorToInt32((Cm.Y - MinCm.Y) / SizeCm.Y * H);
	return (X >= 0 && Y >= 0 && X < W && Y < H) ? Y * W + X : -1;
}

void URTSFogOfWar::Update(const std::vector<uint8>& InVision, bool bReveal, float DeltaSeconds)
{
	if (int32(InVision.size()) == W * H)
	{
		for (int32 I = 0; I < W * H; ++I)
		{
			Raw[I] = InVision[size_t(I)];
		}
	}
	bRevealAll = bReveal;
	const float Step = LevelPerSecond * FMath::Max(DeltaSeconds, 0.0f);
	bool bChanged = false;
	for (int32 I = 0; I < W * H; ++I)
	{
		const float Target = bReveal ? VisibleLevel : (Raw[I] >= 2 ? VisibleLevel : (Raw[I] == 1 || bStartExplored ? ExploredLevel : NeverSeenLevel));
		// a 1ª vez já no valor final: os próprios tanques não surgem do preto na abertura
		const float Now = bHasLevels ? (Level[I] < Target ? FMath::Min(Level[I] + Step, Target) : FMath::Max(Level[I] - Step, Target)) : Target;
		if (Now != Level[I])
		{
			Level[I] = Now;
			bChanged = true;
		}
	}
	if (bChanged || !bHasLevels)
	{
		bHasLevels = true;
		UploadVision();
		bMinimapDirty = true;
	}
	const double T = FPlatformTime::Seconds();
	if (bMinimapReady && bMinimapDirty && T - LastCompose >= ComposeEverySeconds)
	{
		LastCompose = T;
		bMinimapDirty = false;
		ComposeMinimap();
	}
}

void URTSFogOfWar::UploadVision()
{
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(W * H);
	for (int32 I = 0; I < W * H; ++I)
	{
		// o byte é o nível da tela; a textura é sRGB, então a passada lê ~nível^2,2 (multiplicador linear)
		const uint8 B = uint8(FMath::Clamp(FMath::RoundToInt32(Level[I] * 255.0f), 0, 255));
		Pixels[I] = FColor(B, B, B, 255);
	}
	Upload(Vision, Pixels, W, H);
}

float URTSFogOfWar::LevelAt(const FVector2D& Cm) const
{
	const int32 I = CellIndex(Cm);
	return I >= 0 ? Level[I] : NeverSeenLevel;
}

bool URTSFogOfWar::IsVisibleNow(const FVector2D& Cm) const
{
	if (bRevealAll)
	{
		return true;
	}
	const int32 I = CellIndex(Cm);
	return I >= 0 && Raw[I] >= 2;
}

bool URTSFogOfWar::IsExplored(const FVector2D& Cm) const
{
	if (bRevealAll || bStartExplored)
	{
		return true;
	}
	const int32 I = CellIndex(Cm);
	return I >= 0 && Raw[I] >= 1;
}

FVector2D URTSFogOfWar::WorldToMinimapUV(const FVector2D& Cm) const
{
	// a foto olha pra baixo com o +X do mundo pra cima e o +Y pra direita
	return FVector2D((Cm.Y - MinCm.Y) / SizeCm.Y, 1.0 - (Cm.X - MinCm.X) / SizeCm.X);
}

FVector2D URTSFogOfWar::MinimapUVToWorld(const FVector2D& UV) const
{
	return FVector2D(MinCm.X + (1.0 - UV.Y) * SizeCm.X, MinCm.Y + UV.X * SizeCm.Y);
}

void URTSFogOfWar::ComposeMinimap()
{
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(MinimapSide * MinimapSide);
	for (int32 Py = 0; Py < MinimapSide; ++Py)
	{
		for (int32 Px = 0; Px < MinimapSide; ++Px)
		{
			const FVector2D UV((Px + 0.5) / MinimapSide, (Py + 0.5) / MinimapSide);
			const float L = LevelAt(MinimapUVToWorld(UV));
			// o ZH multiplica a imagem da tela pelo nível (W3DRadar: a mesma névoa do chão)
			const FLinearColor C = MinimapBase[Py * MinimapSide + Px] * L;
			Pixels[Py * MinimapSide + Px] = FColor(
				uint8(FMath::Clamp(FMath::RoundToInt32(C.R * 255.0f), 0, 255)),
				uint8(FMath::Clamp(FMath::RoundToInt32(C.G * 255.0f), 0, 255)),
				uint8(FMath::Clamp(FMath::RoundToInt32(C.B * 255.0f), 0, 255)), 255);
		}
	}
	Upload(Minimap, Pixels, MinimapSide, MinimapSide);
}

void URTSFogOfWar::BeginMinimapCapture(UWorld* World, const TArray<AActor*>& Hidden)
{
	if (!World || CaptureStage != 0 || bMinimapReady)
	{
		return;
	}
	CaptureTarget = NewObject<UTextureRenderTarget2D>(this);
	CaptureTarget->RenderTargetFormat = RTF_RGBA16f;
	CaptureTarget->ClearColor = FLinearColor::Black;
	CaptureTarget->InitAutoFormat(CaptureSide, CaptureSide);
	CaptureTarget->UpdateResourceImmediate(true);

	const FVector Center(MinCm.X + SizeCm.X * 0.5, MinCm.Y + SizeCm.Y * 0.5, CaptureHeightCm);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	CaptureActor = World->SpawnActor<ASceneCapture2D>(Center, FRotator(-90.0f, 0.0f, 0.0f), Params);
	if (!CaptureActor)
	{
		return;
	}
	USceneCaptureComponent2D* Cap = CaptureActor->GetCaptureComponent2D();
	// ortográfica: pixel ↔ XY do mundo exato (a perspectiva desalinha os pontos e a névoa do
	// minimapa); em HDR o pós-processo inteiro fica de fora (névoa, vinheta, exposição local)
	Cap->ProjectionType = ECameraProjectionMode::Orthographic;
	Cap->OrthoWidth = float(SizeCm.X);
	Cap->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	Cap->bCaptureEveryFrame = false;
	Cap->bCaptureOnMovement = false;
	Cap->TextureTarget = CaptureTarget;
	Cap->ShowFlags.SetFog(false);
	for (AActor* A : Hidden)
	{
		if (A)
		{
			Cap->HiddenActors.Add(A);
		}
	}
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ViewDistanceScale")))
	{
		SavedViewDistanceScale = CVar->GetFloat();
		CVar->Set(CaptureViewDistanceScale, ECVF_SetByCode);
	}
	// o mesmo bias de exposição do mapa (o build_slice.py do WorldForge grava no PostProcess)
	float Bias = 0.0f;
	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		if (It->bUnbound && It->Settings.bOverride_AutoExposureBias)
		{
			Bias = It->Settings.AutoExposureBias;
			break;
		}
	}
	ExposureScale = FMath::Pow(2.0f, Bias);
	CaptureStage = 1;
	CaptureFrames = 0;
}

void URTSFogOfWar::TickMinimapCapture()
{
	if (CaptureStage == 0 || !CaptureActor)
	{
		return;
	}
	++CaptureFrames;
	if (CaptureStage == 1 && CaptureFrames >= 2)
	{
		// o corte de distância novo só vale no começo do quadro seguinte
		CaptureActor->GetCaptureComponent2D()->CaptureScene();
		CaptureStage = 2;
		CaptureFrames = 0;
	}
	else if (CaptureStage == 2 && CaptureFrames >= 2)
	{
		FinishCapture();
	}
}

void URTSFogOfWar::FinishCapture()
{
	TArray<FLinearColor> Pixels;
	FTextureRenderTargetResource* Res = CaptureTarget ? CaptureTarget->GameThread_GetRenderTargetResource() : nullptr;
	const bool bRead = Res && Res->ReadLinearColorPixels(Pixels) && Pixels.Num() == CaptureSide * CaptureSide;
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ViewDistanceScale")))
	{
		CVar->Set(SavedViewDistanceScale, ECVF_SetByCode);
	}
	if (CaptureActor)
	{
		CaptureActor->Destroy();
	}
	CaptureActor = nullptr;
	CaptureStage = 0;
	if (bRead)
	{
		// reduz 1024 → 256 pela média (a saída em HDR não tem antisserrilhado)
		const int32 K = CaptureSide / MinimapSide;
		TArray<FLinearColor> Small;
		Small.Init(FLinearColor::Black, MinimapSide * MinimapSide);
		double LogSum = 0.0;
		for (int32 Y = 0; Y < MinimapSide; ++Y)
		{
			for (int32 X = 0; X < MinimapSide; ++X)
			{
				FLinearColor Sum = FLinearColor::Black;
				for (int32 J = 0; J < K; ++J)
				{
					for (int32 I = 0; I < K; ++I)
					{
						Sum += Pixels[(Y * K + J) * CaptureSide + X * K + I];
					}
				}
				const FLinearColor C = Sum / float(K * K);
				Small[Y * MinimapSide + X] = C;
				LogSum += FMath::Loge(FMath::Max(double(C.GetLuminance()), 1e-6));
			}
		}
		// exposição: a média geométrica da luminância vai pro cinza médio (18%), com o bias do
		// mapa: a chave de Reinhard (2002), que é o que a exposição automática faz numa cena só
		const double GeoMean = FMath::Exp(LogSum / double(MinimapSide * MinimapSide));
		const float Exposure = float(0.18 / FMath::Max(GeoMean, 1e-6)) * ExposureScale;
		for (int32 I = 0; I < MinimapSide * MinimapSide; ++I)
		{
			const FLinearColor C = Small[I] * Exposure;
			MinimapBase[I] = FLinearColor(LinearToSRGB(AcesFilm(C.R)), LinearToSRGB(AcesFilm(C.G)), LinearToSRGB(AcesFilm(C.B)), 1.0f);
		}
		UE_LOG(LogTemp, Log, TEXT("RTS minimapa capturado (média geométrica %.4g, exposição %.4g)"), GeoMean, Exposure);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("RTS minimapa: a foto do mapa falhou; fundo liso"));
	}
	CaptureTarget = nullptr;
	bMinimapReady = true;
	bMinimapDirty = true;
	LastCompose = -1.0;
	ComposeMinimap();
}
