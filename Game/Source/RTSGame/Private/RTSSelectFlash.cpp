#include "RTSSelectFlash.h"

#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	struct FPart
	{
		TWeakObjectPtr<UPrimitiveComponent> Part;
		bool bHadCustomDepth = false;
	};
	struct FFlash
	{
		TWeakObjectPtr<AActor> Unit;
		TArray<FPart> Parts;
		double Born = 0.0;
	};
	TArray<FFlash>& Flashes() { static TArray<FFlash> F; return F; }
	// o pós-processo na câmera do jogador (a câmera segura o material: aqui só o fraco)
	TWeakObjectPtr<UMaterialInstanceDynamic>& PostMid() { static TWeakObjectPtr<UMaterialInstanceDynamic> M; return M; }
	TWeakObjectPtr<UCameraComponent>& PostCamera() { static TWeakObjectPtr<UCameraComponent> C; return C; }
	// -RTSFlashTest=hold: o clarão parado no pico (a foto da prova)
	const bool bHold = []
	{
		FString V;
		return FParse::Value(FCommandLine::Get(), TEXT("RTSFlashTest="), V) && V == TEXT("hold");
	}();

	UMaterialInstanceDynamic* EnsurePost(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		AActor* View = PC ? PC->GetViewTarget() : nullptr;
		UCameraComponent* Camera = View ? View->FindComponentByClass<UCameraComponent>() : nullptr;
		if (!Camera)
		{
			return nullptr;
		}
		if (PostCamera().Get() == Camera && PostMid().IsValid())
		{
			return PostMid().Get();
		}
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/UI/M_SelectFlash.M_SelectFlash"));
		if (!Base)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, Camera);
		Mid->SetScalarParameterValue(TEXT("Amount"), 0.0f);
		Camera->PostProcessSettings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, Mid));
		Camera->PostProcessBlendWeight = 1.0f;
		PostMid() = Mid;
		PostCamera() = Camera;
		return Mid;
	}

	// sobe em 30 ms (o clique responde na hora, sem o estalo de um quadro só) e cai em curva até apagar
	float Envelope(float T)
	{
		if (bHold)
		{
			return 1.0f;
		}
		constexpr float Rise = 0.03f;
		const float Sec = T * RTSSelectFlash::Seconds;
		if (Sec < Rise)
		{
			return Sec / Rise;
		}
		return FMath::Square(1.0f - FMath::Clamp((Sec - Rise) / (RTSSelectFlash::Seconds - Rise), 0.0f, 1.0f));
	}

	void Clear(FFlash& F)
	{
		// tira só o bit do clarão: o resto do stencil é da silhueta e da poeira (ARTSUnitActor::ApplyStencil)
		for (const FPart& P : F.Parts)
		{
			if (UPrimitiveComponent* Part = P.Part.Get())
			{
				const int32 Value = Part->CustomDepthStencilValue & ~RTSSelectFlash::StencilBit;
				Part->SetCustomDepthStencilValue(Value);
				Part->SetRenderCustomDepth(P.bHadCustomDepth || Value > 0);
			}
		}
		F.Parts.Reset();
	}
}

void RTSSelectFlash::Spawn(AActor* Unit, const FLinearColor& Team)
{
	UWorld* World = Unit ? Unit->GetWorld() : nullptr;
	UMaterialInstanceDynamic* Mid = EnsurePost(World);
	if (!Mid)
	{
		return;
	}
	Mid->SetVectorParameterValue(TEXT("FlashColor"), Team);
	for (FFlash& F : Flashes())
	{
		if (F.Unit.Get() == Unit)
		{
			F.Born = World->GetRealTimeSeconds();
			return;
		}
	}
	FFlash F;
	F.Unit = Unit;
	// as peças visíveis do modelo (o casco, a torre, as rodas, a tripulação); os efeitos (o disco do rotor, a chama) são de outro tipo
	Unit->ForEachComponent<UPrimitiveComponent>(false, [&F](UPrimitiveComponent* Part)
	{
		const bool bModel = (Cast<UStaticMeshComponent>(Part) && Cast<UStaticMeshComponent>(Part)->GetStaticMesh())
			|| (Cast<USkeletalMeshComponent>(Part) && Cast<USkeletalMeshComponent>(Part)->GetSkeletalMeshAsset());
		if (bModel && Part->IsVisible() && Part->Bounds.BoxExtent.GetMax() < 5000.0f)
		{
			FPart P;
			P.Part = Part;
			P.bHadCustomDepth = Part->bRenderCustomDepth;
			Part->SetRenderCustomDepth(true);
			Part->SetCustomDepthStencilValue(Part->CustomDepthStencilValue | StencilBit);
			F.Parts.Add(P);
		}
	});
	if (F.Parts.IsEmpty())
	{
		return;
	}
	F.Born = World->GetRealTimeSeconds();
	Flashes().Add(MoveTemp(F));
}

void RTSSelectFlash::Tick(UWorld* World)
{
	if (!World)
	{
		return;
	}
	const double Now = World->GetRealTimeSeconds();
	float Amount = 0.0f;
	for (int32 K = Flashes().Num() - 1; K >= 0; --K)
	{
		FFlash& F = Flashes()[K];
		const AActor* Unit = F.Unit.Get();
		if (Unit && Unit->GetWorld() != World)
		{
			continue;
		}
		const float T = float(Now - F.Born) / Seconds;
		if (!Unit || (T >= 1.0f && !bHold))
		{
			Clear(F);
			Flashes().RemoveAtSwap(K);
			continue;
		}
		// os do mesmo clique nascem juntos: um clarão só na tela, o maior
		Amount = FMath::Max(Amount, Envelope(FMath::Max(T, 0.0f)));
	}
	if (UMaterialInstanceDynamic* Mid = PostMid().Get(); Mid && PostCamera().IsValid() && PostCamera()->GetWorld() == World)
	{
		Mid->SetScalarParameterValue(TEXT("Amount"), Amount);
	}
}
