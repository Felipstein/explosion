#include "BlastFlipbook.h"

#include "BlastLabSimulation.h"
#include "Components/MaterialBillboardComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const TCHAR* CardPath = TEXT("/Game/Explosions/Flipbook/M_FlipbookCard.M_FlipbookCard");    // build_flipbook.py
	// o atlas já em cor de tela (o AgX da prévia): o material acha a radiância que o nosso tonemapper devolve nela
	const TCHAR* DisplayCardPath = TEXT("/Game/Explosions/Flipbook/M_FlipbookCardDisplay.M_FlipbookCardDisplay");
	// a luz de 6 direções (Tools/build_sixway.py)
	const TCHAR* SixWayPath = TEXT("/Game/Explosions/SixWay/M_SixWay.M_SixWay");
	// os últimos 0.3 s somem (o atlas acaba e a coluna e o fogo da carcaça seguem)
	constexpr float FadeS = 0.3f;
}

ABlastFlipbook::ABlastFlipbook()
{
	PrimaryActorTick.bCanEverTick = true;
	Card = CreateDefaultSubobject<UMaterialBillboardComponent>(TEXT("Card"));
	RootComponent = Card;
	Card->SetCastShadow(false);
	Card->SetMobility(EComponentMobility::Movable);
}

void ABlastFlipbook::Start(UTexture* Atlas, UTexture* Emission, int32 InCols, int32 InRows, int32 InFrames, float InFps, float InSizeM, float InGroundM, bool bDisplay)
{
	Cols = FMath::Max(InCols, 1);
	Rows = FMath::Max(InRows, 1);
	Frames = FMath::Clamp(InFrames, 1, Cols * Rows);
	Fps = FMath::Max(InFps, 1.0f);
	SizeM = InSizeM;
	GroundM = InGroundM;
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, bDisplay ? DisplayCardPath : CardPath);
	if (!Base || !Atlas)
	{
		UE_LOG(LogTemp, Warning, TEXT("BlastLab: falta o flipbook (rode Tools/build_flipbook.py)"));
		Destroy();
		return;
	}
	Material = UMaterialInstanceDynamic::Create(Base, this);
	Material->SetTextureParameterValue(TEXT("Atlas"), Atlas);
	Material->SetScalarParameterValue(TEXT("Cols"), float(Cols));
	Material->SetScalarParameterValue(TEXT("Rows"), float(Rows));
	// o fogo em atlas próprio (sem ele, o padrão preto do material: só a fumaça)
	if (Emission)
	{
		Material->SetTextureParameterValue(TEXT("Emission"), Emission);
	}
	// os ganhos Cycles → cd/m² são os do material (build_flipbook.py);
	// -FlipbookGain=x (fumaça) e -FlipbookEmissionGain=x (fogo) pra A/B
	float Gain = 0.0f;
	if (FParse::Value(FCommandLine::Get(), TEXT("FlipbookGain="), Gain) && Gain > 0.0f)
	{
		Material->SetScalarParameterValue(TEXT("Gain"), Gain);
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("FlipbookEmissionGain="), Gain) && Gain > 0.0f)
	{
		Material->SetScalarParameterValue(TEXT("EmissionGain"), Gain);
	}
	// -FlipbookAlphaPow=x: a franja rala mais transparente (a borda dura da fumaça densa)
	if (FParse::Value(FCommandLine::Get(), TEXT("FlipbookAlphaPow="), Gain) && Gain > 0.0f)
	{
		Material->SetScalarParameterValue(TEXT("AlphaPow"), Gain);
	}
	// o cartão em pé com o chão do atlas no chão do mundo: o centro a meio quadro
	// menos a margem de baixo (o sprite fica virado pra câmera em volta do centro)
	Card->SetRelativeLocation(FVector(0.0, 0.0, (0.5f * SizeM - GroundM) * 100.0f));
	// o tamanho do sprite é o MEIO lado (o quad vai de −Size a +Size): com o lado
	// inteiro o cartão saía com 36 m e o chão do atlas 9 m abaixo do solo (a bola
	// nascia escondida e depois virava uma massa enorme; rodada fs de 07/10)
	Card->AddElement(Material, nullptr, false, SizeM * 50.0f, SizeM * 50.0f, nullptr);
	SetLifeSpan(Frames / Fps + 1.0f);
}

void ABlastFlipbook::StartSixWay(UTexture* A, UTexture* B, UTexture* MV, int32 InCols, int32 InRows, int32 InFrames, float InFps, float InSizeM, float InCenterM)
{
	Cols = FMath::Max(InCols, 1);
	Rows = FMath::Max(InRows, 1);
	Frames = FMath::Clamp(InFrames, 1, Cols * Rows);
	Fps = FMath::Max(InFps, 1.0f);
	SizeM = InSizeM;
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, SixWayPath);
	if (!Base || !A || !B || !MV)
	{
		UE_LOG(LogTemp, Warning, TEXT("BlastLab: falta o 6-way (rode Tools/build_sixway.py)"));
		Destroy();
		return;
	}
	Material = UMaterialInstanceDynamic::Create(Base, this);
	Material->SetTextureParameterValue(TEXT("A"), A);
	Material->SetTextureParameterValue(TEXT("B"), B);
	Material->SetTextureParameterValue(TEXT("MV"), MV);
	Material->SetScalarParameterValue(TEXT("Cols"), float(Cols));
	Material->SetScalarParameterValue(TEXT("Rows"), float(Rows));
	// A/B: -SixWayFire=x (o ganho do fogo), -SixWaySky=x, -SixWaySun=x
	float V = 0.0f;
	for (const TPair<const TCHAR*, const TCHAR*>& P : { TPair<const TCHAR*, const TCHAR*>(TEXT("SixWayFire="), TEXT("EmissionGain")),
		TPair<const TCHAR*, const TCHAR*>(TEXT("SixWaySky="), TEXT("SkyGain")), TPair<const TCHAR*, const TCHAR*>(TEXT("SixWaySun="), TEXT("DirectGain")),
		TPair<const TCHAR*, const TCHAR*>(TEXT("SixWayDebug="), TEXT("DebugView")) })
	{
		if (FParse::Value(FCommandLine::Get(), P.Key, V) && V >= 0.0f)
		{
			Material->SetScalarParameterValue(P.Value, V);
		}
	}
	// o cartão virado pra câmera em volta do alvo da câmera de bake
	Card->SetRelativeLocation(FVector(0.0, 0.0, InCenterM * 100.0f));
	Card->AddElement(Material, nullptr, false, SizeM * 50.0f, SizeM * 50.0f, nullptr);
	SetLifeSpan(Frames / Fps + 1.0f);
}

void ABlastFlipbook::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Material)
	{
		return;
	}
	const UBlastLabSimulation* Sim = UBlastLabSimulation::Get(this);
	Age += Sim ? Sim->SimDelta(DeltaSeconds) : DeltaSeconds;
	const float Duration = Frames / Fps;
	const float X = FMath::Clamp(Age * Fps, 0.0f, float(Frames - 1));
	const int32 F0 = FMath::Min(FMath::FloorToInt32(X), Frames - 1), F1 = FMath::Min(F0 + 1, Frames - 1);
	Material->SetScalarParameterValue(TEXT("Frame0"), float(F0));
	Material->SetScalarParameterValue(TEXT("Frame1"), float(F1));
	Material->SetScalarParameterValue(TEXT("FrameAlpha"), X - F0);
	Material->SetScalarParameterValue(TEXT("Fade"), 1.0f - FMath::SmoothStep(Duration - FadeS, Duration, Age));
}
