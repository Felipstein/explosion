#include "ForgeDebugView.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** O trapézio da câmera e a marca do quadro, desenhados por cima do mapa pequeno. */
	class SForgeFrustumLines : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SForgeFrustumLines) {}
			SLATE_ATTRIBUTE(TArray<FVector2D>, Points)
			SLATE_ATTRIBUTE(FVector2D, Mark)
			SLATE_ARGUMENT(const TArray<FBox2D>*, Boxes)
			SLATE_ARGUMENT(const TArray<bool>*, Drawn)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			Points = Args._Points;
			Mark = Args._Mark;
			Boxes = Args._Boxes;
			Drawn = Args._Drawn;
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(256, 256); }

		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&, FSlateWindowElementList& Out,
			int32 Layer, const FWidgetStyle&, bool) const override
		{
			const FVector2D Size = Geo.GetLocalSize();
			// blocos de terreno: verde = a câmera desenha, cinza = não
			if (Boxes && Drawn && Boxes->Num() == Drawn->Num())
			{
				const FSlateBrush* White = FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));
				for (int32 I = 0; I < Boxes->Num(); ++I)
				{
					const FBox2D& B = (*Boxes)[I];
					const FVector2D Min = B.Min * Size, Ext = (B.Max - B.Min) * Size;
					FSlateDrawElement::MakeBox(Out, Layer + 1, Geo.ToPaintGeometry(FVector2f(Ext), FSlateLayoutTransform(FVector2f(Min))), White,
						ESlateDrawEffect::None, (*Drawn)[I] ? FLinearColor(0.1f, 0.9f, 0.2f, 0.28f) : FLinearColor(0.05f, 0.05f, 0.05f, 0.6f));
				}
			}
			TArray<FVector2D> P = Points.Get();
			if (P.Num() >= 3)
			{
				for (FVector2D& V : P) V *= Size;
				const FVector2D First = P[0];   // cópia: somar um elemento da própria lista é proibido (Array.h)
				P.Add(First);
				FSlateDrawElement::MakeLines(Out, Layer + 1, Geo.ToPaintGeometry(), P, ESlateDrawEffect::None, FLinearColor(1.0f, 0.85f, 0.2f), true, 2.0f);
			}
			const FVector2D M = Mark.Get() * Size;
			const TArray<FVector2D> Cross = { M + FVector2D(-5, 0), M + FVector2D(5, 0) };
			const TArray<FVector2D> Cross2 = { M + FVector2D(0, -5), M + FVector2D(0, 5) };
			FSlateDrawElement::MakeLines(Out, Layer + 2, Geo.ToPaintGeometry(), Cross, ESlateDrawEffect::None, FLinearColor(0.2f, 0.9f, 1.0f), true, 2.0f);
			FSlateDrawElement::MakeLines(Out, Layer + 2, Geo.ToPaintGeometry(), Cross2, ESlateDrawEffect::None, FLinearColor(0.2f, 0.9f, 1.0f), true, 2.0f);
			return Layer + 2;
		}

	private:
		TAttribute<TArray<FVector2D>> Points;
		TAttribute<FVector2D> Mark;
		const TArray<FBox2D>* Boxes = nullptr;
		const TArray<bool>* Drawn = nullptr;
	};
}

AForgeDebugView::AForgeDebugView()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;   // depois da câmera andar
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Probe = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Probe"));
	Probe->SetupAttachment(RootComponent);
	ProbeFar = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("ProbeFar"));
	ProbeFar->SetupAttachment(RootComponent);
	Overview = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Overview"));
	Overview->SetupAttachment(RootComponent);
}

void AForgeDebugView::Setup(const FVector& Target, float YawDeg, const FBox2D& MapBounds)
{
	TargetPoint = Target;
	Bounds = MapBounds;

	// quadros: 9 m atrás do ponto, 3 m acima do chão de lá, olhando o chão, a
	// pedra e a árvore (o chão vem de um raio de cima: Landscape tem colisão)
	ProbeTarget = NewObject<UTextureRenderTarget2D>(this);
	ProbeTarget->InitCustomFormat(480, 270, PF_B8G8R8A8, false);
	const FVector Back = FRotator(0.0f, YawDeg, 0.0f).Vector() * -900.0f;
	FVector Eye = Target + Back + FVector(0, 0, 300.0f);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Eye + FVector(0, 0, 20000.0f), Eye - FVector(0, 0, 20000.0f), ECC_Visibility))
	{
		Eye.Z = Hit.ImpactPoint.Z + 300.0f;
	}
	for (USceneCaptureComponent2D* P : { Probe.Get(), ProbeFar.Get() })
	{
		P->SetWorldLocationAndRotation(Eye, (Target + FVector(0, 0, 100.0f) - Eye).Rotation());
		P->FOVAngle = 75.0f;
		P->CaptureSource = SCS_FinalColorLDR;
		P->bCaptureEveryFrame = false;   // 5× por segundo (Tick): cada foto é uma cena inteira desenhada de novo
		P->bCaptureOnMovement = false;
		// sem o estado guardado, cada foto avulsa recomeça a exposição do zero e,
		// com o sol de dia, sai branca (a mesma coisa do mapa pequeno)
		P->bAlwaysPersistRenderingState = true;
		// cada câmera extra guardava a própria iluminação global (Lumen) e
		// estourava a memória de vídeo da 1660 (1,2 GB acima, 4 qps): os quadros
		// ficam com sol, céu e sombra, sem Lumen nem reflexos
		P->PostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = true;
		P->PostProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
		P->PostProcessSettings.bOverride_ReflectionMethod = true;
		P->PostProcessSettings.ReflectionMethod = EReflectionMethod::None;
		P->PostProcessBlendWeight = 1.0f;
	}
	Probe->TextureTarget = ProbeTarget;
	for (int32 W = 480; W >= 15; W /= 2)
	{
		UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(this);
		RT->InitCustomFormat(W, FMath::Max(1, W * 9 / 16), PF_B8G8R8A8, false);
		RT->Filter = TF_Nearest;   // ampliado sem suavizar: os pixels que a câmera principal tem
		FarTargets.Add(RT);
	}
	ProbeFar->TextureTarget = FarTargets[0];

	// mapa pequeno: o mapa todo de cima (ortográfica), refeito a cada meio segundo
	OverviewTarget = NewObject<UTextureRenderTarget2D>(this);
	OverviewTarget->InitCustomFormat(256, 256, PF_B8G8R8A8, false);
	const FVector2D C = Bounds.GetCenter();
	Overview->SetWorldLocationAndRotation(FVector(C.X, C.Y, Target.Z + 60000.0f), FRotator(-90.0f, 0.0f, 0.0f));
	Overview->ProjectionType = ECameraProjectionMode::Orthographic;
	Overview->OrthoWidth = FMath::Max(Bounds.GetSize().X, Bounds.GetSize().Y);
	Overview->TextureTarget = OverviewTarget;
	// sem névoa e sem atmosfera (de 600 m de altura embranqueciam tudo) e com o
	// estado da vista guardado (sem ele a exposição recomeçava a cada foto: branco)
	Overview->CaptureSource = SCS_FinalColorLDR;
	Overview->bAlwaysPersistRenderingState = true;
	Overview->ShowFlags.SetFog(false);
	Overview->ShowFlags.SetAtmosphere(false);
	Overview->ShowFlags.SetCloud(false);
	Overview->ShowFlags.SetDynamicShadows(false);
	Overview->PostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = true;
	Overview->PostProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
	Overview->PostProcessSettings.bOverride_ReflectionMethod = true;
	Overview->PostProcessSettings.ReflectionMethod = EReflectionMethod::None;
	Overview->PostProcessBlendWeight = 1.0f;
	Overview->bCaptureEveryFrame = false;
	Overview->bCaptureOnMovement = false;
	Overview->CaptureScene();

	ProbeBrush = MakeShared<FSlateBrush>();
	ProbeBrush->SetResourceObject(ProbeTarget);
	ProbeBrush->ImageSize = FVector2D(480, 270);
	FarBrush = MakeShared<FSlateBrush>();
	FarBrush->SetResourceObject(FarTargets[0]);
	FarBrush->ImageSize = FVector2D(480, 270);
	OverviewBrush = MakeShared<FSlateBrush>();
	OverviewBrush->SetResourceObject(OverviewTarget);
	OverviewBrush->ImageSize = FVector2D(256, 256);

	TWeakObjectPtr<AForgeDebugView> Weak(this);
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 10);
	Overlay = SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
		// mapa pequeno, em cima à direita
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(12)
		[
			SNew(SBorder).Padding(2).BorderBackgroundColor(FLinearColor(0, 0, 0, 0.7f))
			[
				SNew(SBox).WidthOverride(256).HeightOverride(256)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[SNew(SImage).Image(OverviewBrush.Get())]
					+ SOverlay::Slot()
					[
						SNew(SForgeFrustumLines)
						.Boxes(&TerrainBoxes)
						.Drawn(&TerrainDrawn)
						.Points_Lambda([Weak]() { return Weak.IsValid() ? Weak->Frustum : TArray<FVector2D>(); })
						.Mark_Lambda([Weak]() { return Weak.IsValid() ? Weak->ProbeMark : FVector2D::ZeroVector; })
					]
				]
			]
		]
		// o quadro perto do chão, embaixo à direita, com a legenda
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(12)
		[
			SNew(SBorder).Padding(2).BorderBackgroundColor(FLinearColor(0, 0, 0, 0.7f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).WidthOverride(480).HeightOverride(270)[SNew(SImage).Image(ProbeBrush.Get())]]
						+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)
						[
							SNew(STextBlock).Font(Font).ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f))
							.Text(NSLOCTEXT("WorldForge", "ProbeNear", "perto: o detalhe que este ponto tem"))
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).WidthOverride(480).HeightOverride(270)[SNew(SImage).Image(FarBrush.Get())]]
						+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)
						[
							SNew(STextBlock).Font(Font).ColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.2f))
							.Text(NSLOCTEXT("WorldForge", "ProbeFar", "a sua tela ampliada: os pixels que este pedaço ocupa nela"))
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(6, 4)
				[
					SNew(STextBlock).Font(Font).ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f))
					.Text_Lambda([Weak]() { return FText::FromString(Weak.IsValid() ? Weak->Status : FString()); })
				]
			]
		];
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->AddViewportWidgetContent(Overlay.ToSharedRef(), 20);
	}
	const FVector2D Size = FVector2D(Overview->OrthoWidth, Overview->OrthoWidth);
	ProbeMark = FVector2D(0.5 + (Target.Y - C.Y) / Size.Y, 0.5 - (Target.X - C.X) / Size.X);

	// os blocos da cena: pedaços do terreno e os blocos de instâncias
	for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
	{
		UPrimitiveComponent* P = *It;
		if (!P || P->GetWorld() != GetWorld() || !P->IsRegistered() || P->GetOwner() == this) continue;
		FBlock B;
		B.Comp = P;
		B.bTerrain = P->GetClass()->GetName().Contains(TEXT("LandscapeComponent"));
		if (const UInstancedStaticMeshComponent* I = Cast<UInstancedStaticMeshComponent>(P))
		{
			B.CullCm = float(I->InstanceEndCullDistance);
		}
		else if (!B.bTerrain && !P->IsA<UStaticMeshComponent>())
		{
			continue;   // céu, névoa, decalques: fora do teste
		}
		Blocks.Add(B);
		if (B.bTerrain)
		{
			const FBox Box = P->Bounds.GetBox();
			TerrainBoxes.Add(FBox2D(FVector2D(0.5 + (Box.Min.Y - C.Y) / Size.Y, 0.5 - (Box.Max.X - C.X) / Size.X),
				FVector2D(0.5 + (Box.Max.Y - C.Y) / Size.Y, 0.5 - (Box.Min.X - C.X) / Size.X)));
			TerrainDrawn.Add(true);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("FORGE debug view: %d blocks (%d terrain)"), Blocks.Num(), TerrainBoxes.Num());
}

void AForgeDebugView::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager || !Probe->TextureTarget)
	{
		return;
	}
	const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
	const FRotator Rot = PC->PlayerCameraManager->GetCameraRotation();
	const float Fov = PC->PlayerCameraManager->GetFOVAngle();

	// trapézio: os 4 cantos da tela batendo no plano do chão do ponto
	FVector2D View(16, 9);
	if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(View);
	const float TanH = FMath::Tan(FMath::DegreesToRadians(0.5f * Fov));
	const float TanV = TanH * View.Y / FMath::Max(View.X, 1.0);

	// "da sua câmera": quantos pixels a câmera principal gasta no ponto em
	// relação ao quadro (altura da tela, distância e campo de visão), e o LOD
	// dessa distância
	const float MainDist = FVector::Distance(Cam, TargetPoint);
	const float ProbeDist = FVector::Distance(Probe->GetComponentLocation(), TargetPoint);
	const float ProbeTanV = FMath::Tan(FMath::DegreesToRadians(0.5f * Probe->FOVAngle)) * 270.0f / 480.0f;
	const float PixelRatio = float(View.Y / 270.0) * (ProbeDist / FMath::Max(MainDist, 1.0f)) * (ProbeTanV / FMath::Max(TanV, 1e-3f));
	int32 Pick = 0;
	while (Pick + 1 < FarTargets.Num() && 480.0f * PixelRatio <= 0.75f * FarTargets[Pick]->SizeX) ++Pick;
	if (ProbeFar->TextureTarget != FarTargets[Pick])
	{
		ProbeFar->TextureTarget = FarTargets[Pick];
		FarBrush->SetResourceObject(FarTargets[Pick]);
	}
	ProbeFar->LODDistanceFactor = FMath::Clamp(MainDist / FMath::Max(ProbeDist, 1.0f), 1.0f, 100.0f);
	const FVector F = Rot.Vector(), R = FRotationMatrix(Rot).GetScaledAxis(EAxis::Y), U = FRotationMatrix(Rot).GetScaledAxis(EAxis::Z);
	const FVector2D C = Bounds.GetCenter();
	const double W = FMath::Max(Bounds.GetSize().X, Bounds.GetSize().Y);
	Frustum.Reset();
	for (const FVector2D Corner : { FVector2D(-1, 1), FVector2D(1, 1), FVector2D(1, -1), FVector2D(-1, -1) })
	{
		const FVector Dir = (F + R * (Corner.X * TanH) + U * (Corner.Y * TanV)).GetSafeNormal();
		double T = 300000.0;   // canto acima do horizonte: 3 km
		if (Dir.Z < -1e-3) T = FMath::Min(T, (TargetPoint.Z - Cam.Z) / Dir.Z);
		const FVector Hit = Cam + Dir * T;
		// preso à caixa do mapa pequeno (o trapézio de longe passa do mapa)
		Frustum.Add(FVector2D(FMath::Clamp(0.5 + (Hit.Y - C.Y) / W, 0.0, 1.0), FMath::Clamp(0.5 - (Hit.X - C.X) / W, 0.0, 1.0)));
	}

	// o que a câmera principal desenha: cada bloco contra os 4 planos laterais
	// da vista e a distância de corte (a mesma regra da Unreal: o corte por
	// distância é × r.ViewDistanceScale do preset)
	static const IConsoleVariable* ViewDist = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ViewDistanceScale"));
	const float DistScale = ViewDist ? ViewDist->GetFloat() : 1.0f;
	FVector Corner[4];
	int32 K = 0;
	for (const FVector2D Cn : { FVector2D(-1, 1), FVector2D(1, 1), FVector2D(1, -1), FVector2D(-1, -1) })
	{
		Corner[K++] = (F + R * (Cn.X * TanH) + U * (Cn.Y * TanV)).GetSafeNormal();
	}
	FVector Planes[4];
	for (int32 I = 0; I < 4; ++I)
	{
		Planes[I] = FVector::CrossProduct(Corner[I], Corner[(I + 1) % 4]).GetSafeNormal();
		if (FVector::DotProduct(Planes[I], F) < 0.0) Planes[I] = -Planes[I];   // pra dentro da vista
	}
	TArray<TWeakObjectPtr<UPrimitiveComponent>> Hidden;
	NumDrawn = 0;
	int32 TerrainIndex = 0;
	for (const FBlock& B : Blocks)
	{
		const UPrimitiveComponent* P = B.Comp.Get();
		if (!P) continue;
		const FVector Center = P->Bounds.Origin - Cam;
		const float Radius = P->Bounds.SphereRadius;
		bool bDrawn = FVector::DotProduct(F, Center) > -Radius;
		for (int32 I = 0; I < 4 && bDrawn; ++I)
		{
			bDrawn = FVector::DotProduct(Planes[I], Center) > -Radius;
		}
		if (bDrawn && B.CullCm > 0.0f)
		{
			bDrawn = FMath::Sqrt(P->Bounds.GetBox().ComputeSquaredDistanceToPoint(Cam)) < B.CullCm * DistScale;
		}
		if (!bDrawn) Hidden.Add(B.Comp);
		NumDrawn += bDrawn ? 1 : 0;
		if (B.bTerrain && TerrainDrawn.IsValidIndex(TerrainIndex))
		{
			TerrainDrawn[TerrainIndex++] = bDrawn;
		}
	}
	Probe->HiddenComponents = Hidden;
	ProbeFar->HiddenComponents = Hidden;
	ProbeWait -= DeltaSeconds;
	if (ProbeWait <= 0.0)
	{
		Probe->CaptureScene();
		ProbeFar->CaptureScene();
		ProbeWait = 0.2;
	}

	// o mapa não muda: fotografa nos primeiros 2 s (a exposição assenta) e
	// solta a câmera de cima, que guardava o estado dela na memória de vídeo
	if (Overview)
	{
		OverviewWait += DeltaSeconds;
		Overview->CaptureScene();
		if (OverviewWait > 2.0)
		{
			Overview->DestroyComponent();
			Overview = nullptr;
		}
	}
	Status = FString::Printf(TEXT("sua câmera a %.0f m do ponto: %d×%d pixels pra ele · desenhando %d de %d blocos · %.0f qps · F2 fecha"),
		MainDist / 100.0f, FarTargets[Pick]->SizeX, FarTargets[Pick]->SizeY, NumDrawn, Blocks.Num(), 1.0f / FMath::Max(DeltaSeconds, 1e-4f));
}

void AForgeDebugView::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Overlay.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
	}
	Overlay.Reset();
	Super::EndPlay(Reason);
}
