#include "ExplosionVolume.h"

#include "Components/HeterogeneousVolumeComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SparseVolumeTexture/SparseVolumeTexture.h"

namespace
{
	const FName ParamFrame(TEXT("SparseVolumeTexture"));
	const FName ParamNextFrame(TEXT("SparseVolumeTextureNext"));
	const FName ParamFrameAlpha(TEXT("FrameAlpha"));
	const FName ParamFrameInterval(TEXT("FrameInterval"));
	const FName ParamBoxMin(TEXT("BoxMin"));
	const FName ParamBoxSize(TEXT("BoxSize"));
}

UFrameBlendComponent::UFrameBlendComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;   // depois do Sequencer, que anima o Frame do volume
	bTickInEditor = true;
}

UHeterogeneousVolumeComponent* UFrameBlendComponent::Volume() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UHeterogeneousVolumeComponent>() : nullptr;
}

void UFrameBlendComponent::OnRegister()
{
	Super::OnRegister();
	if (UHeterogeneousVolumeComponent* V = Volume())
	{
		// o tick do volume reescreveria a caixa inteira e o quadro dele a cada quadro
		V->SetComponentTickEnabled(false);
		AddTickPrerequisiteComponent(V);
	}
}

void UFrameBlendComponent::ApplyBox(UHeterogeneousVolumeComponent* V, UMaterialInstanceDynamic* MID, const FTransform& Base, int32 Frame)
{
	if (!BoxMin.IsValidIndex(Frame) || !BoxSize.IsValidIndex(Frame))
	{
		return;
	}
	const FIntVector Min = BoxMin[Frame];
	const FIntVector Size = BoxSize[Frame];
	// mesma escala do quadro gravado (1 voxel = domínio/resolução), deslocada pro canto da caixa
	FTransform Box = Base;
	Box.SetTranslation(Base.GetTranslation() + Base.GetScale3D() * FVector(Min));
	MID->SetVectorParameterValue(ParamBoxMin, FLinearColor(Min.X, Min.Y, Min.Z, 0.0f));
	MID->SetVectorParameterValue(ParamBoxSize, FLinearColor(Size.X, Size.Y, Size.Z, 0.0f));
	if (V->VolumeResolution != Size || !V->FrameTransform.Equals(Box))
	{
		V->VolumeResolution = Size;
		V->FrameTransform = Box;
		V->UpdateBounds();
		V->MarkRenderStateDirty();     // o proxy guarda a resolução e a transformação do quadro
	}
}

void UFrameBlendComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UHeterogeneousVolumeComponent* V = Volume();
	if (!V)
	{
		return;
	}
	// a instância dinâmica que o volume criou vem depois do material base
	// (UHeterogeneousVolumeComponent::GetUsedMaterials)
	TArray<UMaterialInterface*> Materials;
	V->GetUsedMaterials(Materials);
	UMaterialInstanceDynamic* MID = nullptr;
	for (UMaterialInterface* M : Materials)
	{
		MID = MID ? MID : Cast<UMaterialInstanceDynamic>(M);
	}
	USparseVolumeTexture* Animated = nullptr;
	if (!MID || !MID->Parent || !MID->Parent->GetSparseVolumeTextureParameterValue(FHashedMaterialParameterInfo(ParamFrame), Animated) || !Animated)
	{
		return;
	}

	const int32 N = Animated->GetNumFrames();
	const float Frame = FMath::Clamp(V->Frame, 0.0f, float(FMath::Max(N - 1, 0)));
	const int32 F0 = FMath::FloorToInt32(Frame);
	const int32 F1 = FMath::Min(F0 + 1, N - 1);
	const bool bBlocking = V->bIssueBlockingRequests != 0;
	const bool bPlaying = V->bPlaying != 0;
	const uint32 Key = GetTypeHash(this);
	// mip 0: a escolha de mip por distância (GetOptimalStreamingMipLevel) não é exportada pelo motor
	USparseVolumeTextureFrame* Lower = USparseVolumeTextureFrame::GetFrameAndIssueStreamingRequest(Animated, Key, V->FrameRate, float(F0), 0.0f, bBlocking, bPlaying);
	USparseVolumeTextureFrame* Upper = USparseVolumeTextureFrame::GetFrameAndIssueStreamingRequest(Animated, Key + 1, V->FrameRate, float(F1), 0.0f, bBlocking, bPlaying);
	if (!Lower || !Upper)
	{
		return;
	}
	MID->SetSparseVolumeTextureParameterValue(ParamFrame, Lower);
	MID->SetSparseVolumeTextureParameterValue(ParamNextFrame, Upper);
	MID->SetScalarParameterValue(ParamFrameAlpha, Frame - F0);
	const float Interval = FrameTimes.IsValidIndex(F1) && FrameTimes.IsValidIndex(F0)
		? FrameTimes[F1] - FrameTimes[F0] : 0.0f;
	MID->SetScalarParameterValue(ParamFrameInterval, Interval);
	ApplyBox(V, MID, Lower->GetFrameTransform(), F0);
}

AExplosionVolume::AExplosionVolume()
{
	Volume = CreateDefaultSubobject<UHeterogeneousVolumeComponent>(TEXT("HeterogeneousVolumeComponent"));
	RootComponent = Volume;
	FrameBlend = CreateDefaultSubobject<UFrameBlendComponent>(TEXT("FrameBlend"));
}
