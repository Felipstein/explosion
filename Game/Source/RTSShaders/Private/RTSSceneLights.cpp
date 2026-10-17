#include "RTSSceneLights.h"

#include "RenderingThread.h"
#include "Engine/Texture2D.h"

namespace RTSSceneLights
{
	namespace
	{
		TArray<FLight> GGame;
		TArray<FLight> GRender;
		UTexture2D* GTex = nullptr;   // na raiz (AddToRoot): um TStrongObjectPtr estático morreria depois do coletor no fim

		void UpdateTexture()
		{
			UTexture2D* T = GetTexture();
			if (!T || !T->GetResource())
			{
				return;
			}
			FLinearColor* Data = new FLinearColor[MaxLights * 2];
			for (int32 k = 0; k < MaxLights; ++k)
			{
				const bool bOn = k < GGame.Num();
				Data[k] = bOn ? FLinearColor(float(GGame[k].PosM.X), float(GGame[k].PosM.Y), float(GGame[k].PosM.Z), GGame[k].RadiusM) : FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
				Data[MaxLights + k] = bOn ? GGame[k].Candela : FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
			}
			FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, MaxLights, 2);
			T->UpdateTextureRegions(0, 1, Region, MaxLights * sizeof(FLinearColor), sizeof(FLinearColor), reinterpret_cast<uint8*>(Data),
				[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
				{
					delete[] reinterpret_cast<FLinearColor*>(SrcData);
					delete Regions;
				});
		}
	}

	UTexture2D* GetTexture()
	{
		check(IsInGameThread());
		if (!GTex)
		{
			UTexture2D* T = UTexture2D::CreateTransient(MaxLights, 2, PF_A32B32G32R32F, TEXT("RTSSceneLights"));
			if (!T)
			{
				return nullptr;
			}
			T->SRGB = false;
			T->Filter = TF_Nearest;
			T->CompressionSettings = TC_HDR;
			T->AddressX = TA_Clamp;
			T->AddressY = TA_Clamp;
			T->UpdateResource();
			T->AddToRoot();
			GTex = T;
		}
		return GTex;
	}

	void Set_GameThread(TArray<FLight>&& Lights)
	{
		check(IsInGameThread());
		GGame = MoveTemp(Lights);
		UpdateTexture();
		ENQUEUE_RENDER_COMMAND(RTSSceneLightsSet)([Copy = GGame](FRHICommandListImmediate&) mutable
		{
			GRender = MoveTemp(Copy);
		});
	}

	const TArray<FLight>& Get_GameThread()
	{
		return GGame;
	}

	const TArray<FLight>& Get_RenderThread()
	{
		check(IsInRenderingThread());
		return GRender;
	}

	FLinearColor IlluminanceAt(const FVector& PosM)
	{
		FLinearColor E = FLinearColor::Black;
		for (const FLight& L : GGame)
		{
			const float D2 = FMath::Max(float(FVector::DistSquared(L.PosM, PosM)), 0.25f);
			const float R2 = FMath::Max(L.RadiusM * L.RadiusM, 1e-4f);
			const float Win = FMath::Square(FMath::Clamp(1.0f - FMath::Square(D2 / R2), 0.0f, 1.0f));
			E += L.Candela * (Win / D2);
		}
		return E;
	}
}
