#include "RTSTreadDust.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "RTSSunShadow.h"

// A sombra na poeira por pixel (o modo 2): a poeira lê o mapa do sol COMUM de todas as fumaças (o plugin RTSSunShadow, a
// regra do CLAUDE.md: um sistema só). Aqui só o que é da poeira: o modo, a conferência do material, os parâmetros dele a
// cada quadro (as duas texturas do mapa, a câmera no quadro do mapa, o eixo da luz, a janela dos veículos, o céu de volume)
// e os veículos que ela repassa ao mapa enquanto o jogo não manda todos. O material chama a MESMA função de shader que o
// raymarch das explosões (RTSSunVisibility, SunShadow.ush).

namespace
{
	// o peso do hemisfério de cima num eixo da base 6-way (pesos L²), com c = o cosseno entre o eixo e o "pra cima":
	// g(1) = 2/3, g(0) = 1/3, g(−1) = 0; o eixo do lado de lá leva 2/3 − g (a 2ª revisão, conferido por Monte Carlo)
	double HemiW(double C)
	{
		C = FMath::Clamp(C, -1.0, 1.0);
		return (2.0 / (3.0 * PI)) * (PI - FMath::Acos(C) + C * FMath::Sqrt(FMath::Max(0.0, 1.0 - C * C)));
	}
}

URTSSunShadowSubsystem* ARTSTreadDust::SunShadow() const
{
	const UWorld* W = GetWorld();
	return W ? W->GetSubsystem<URTSSunShadowSubsystem>() : nullptr;
}

int32 ARTSTreadDust::ShadowModeNow() const
{
	if (!bShadowMatOk)
	{
		return 0;                                 // o material velho (sem os parâmetros da sombra): a conta de antes
	}
	int32 M = P.ShadowMode;
	if (Quality <= 0)
	{
		M = FMath::Min(M, 1);                     // a baixa: por cartão (nada por quadro na placa)
	}
	const URTSSunShadowSubsystem* S = SunShadow();
	if (M >= 2 && (!S || !S->IsReady()))
	{
		M = 1;                                    // o mapa comum ainda não chegou (o 1º segundo), ou desistiu
	}
	return M;
}

bool ARTSTreadDust::IsShadowReady() const
{
	const URTSSunShadowSubsystem* S = SunShadow();
	return P.ShadowMode < 2 || Quality < 1 || !bShadowMatOk || !S || S->HasGivenUp() || S->IsReady();
}

bool ARTSTreadDust::ProbeShadowMaterial(UMaterialInterface* M)
{
	// a DLL nova com o material velho: o UV2.x dos modos 1 e 2 seria lido como a sombra de antes (a 2ª revisão)
	float V = 0.0f;
	bShadowMatOk = M && M->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("ShadowMode")), V);
	if (!bShadowMatOk && P.ShadowMode > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("RTSTreadDust: o M_TreadDust não tem o ShadowMode (material de antes): a sombra fica no modo 0"));
	}
	return bShadowMatOk;
}

void ARTSTreadDust::UpdateShadowParams(const FVector& CamM, const FVector& CamRight, const FVector& CamUp, const FVector& CamFwd)
{
	URTSSunShadowSubsystem* S = SunShadow();
	// os veículos desta poeira vão pro mapa comum (a camada dos veículos) como reserva: se o jogo manda todos, vale a dele
	if (S && bFeedSunShadow && P.ShadowMode >= 2)
	{
		TArray<FRTSShadowVehicle> Vs;
		Vs.Reserve(Sources.Num());
		for (const FRTSTreadSource& Src : Sources)
		{
			FRTSShadowVehicle V;
			V.Id = Src.Id;
			V.PosM = Src.PosM;
			V.FwdM = Src.FwdM;
			V.TurretFwdM = Src.TurretFwdM;
			V.HalfLenM = Src.HalfLenM;
			V.HalfWidthM = Src.HalfWidthM;
			V.HullTopM = Src.HullTopM;
			V.HeightM = Src.HeightM;
			V.bVisible = Src.bVisible;
			Vs.Add(V);
		}
		S->SetVehicles(Vs, false);                // reserva: a lista do jogo, quando vem, manda
	}
	if (!CardMid)
	{
		return;
	}
	const int32 Mode = ShadowModeNow();
	CardMid->SetScalarParameterValue(TEXT("ShadowMode"), float(Mode));
	if (Mode == 0)
	{
		return;
	}
	// o céu de volume na sombra: o "pra cima" do mundo na base do cartão (direita, cima, pra câmera; o giro de cada
	// cartão muda ~2 %), e o peso do hemisfério de cima em cada eixo (a 2ª revisão: a fórmula fixa supunha a câmera
	// deitada, e errava 13–19 % com a câmera do jogo)
	const FVector Nz(CamRight.Z, CamUp.Z, -CamFwd.Z);
	SkyW = FVector(HemiW(Nz.X), HemiW(Nz.Y), HemiW(Nz.Z));
	CardMid->SetVectorParameterValue(TEXT("SkyW"), FLinearColor(float(SkyW.X), float(SkyW.Y), float(SkyW.Z), 0.0f));
	if (Mode == 2 && S)
	{
		FLinearColor CamToMap, Axis, Window;
		float TanE = 0.0f;
		S->GetMaterialParams(CamM * 100.0, CamToMap, Axis, Window, TanE);
		CardMid->SetTextureParameterValue(TEXT("HMap"), S->GetMapTexture());
		if (S->GetVehicleTexture())
		{
			CardMid->SetTextureParameterValue(TEXT("TMap"), S->GetVehicleTexture());
		}
		CardMid->SetVectorParameterValue(TEXT("HCam"), CamToMap);
		CardMid->SetVectorParameterValue(TEXT("HAx"), Axis);
		CardMid->SetVectorParameterValue(TEXT("TBox"), Window);
		CardMid->SetScalarParameterValue(TEXT("HTan"), TanE);
	}
}
