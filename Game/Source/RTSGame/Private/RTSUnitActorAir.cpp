// (24/10, o escopo B do ar; docs/AEREO.md §3.10 e §4.5) O desenho que faz a aeronave viver, no ator da unidade (só desenho:
// nada volta pra simulação):
//  - o disco borrado dos rotores: as pás de verdade parado e devagar; no giro cheio a pá some (a geometria girando 30°+ por
//    quadro engana o TSR: a lição do ISM sem movimento) e entra um disco parado no cubo, cujo material desenha o borrão. A
//    cobertura média do disco é a da pá (N·c / 2πr: mais escura perto do cubo, como nas fotos), as pás fantasma andam devagar
//    (o efeito estroboscópico das câmeras) e a ponta pintada vira um anel. As pás escondidas seguem fazendo sombra (o disco
//    tremido no chão, como o de verdade);
//  - a pá que voa na queda (o AirBladeOff): o rotor principal se solta com a velocidade da aeronave, gira e cai;
//  - a rampa do Chinook (abre carregando e descarregando, a ponta desce até o chão);
//  - o pós-combustor do jato (a chama com os diamantes de choque nas bocas e a luz laranja no chão, que as fumaças leem).
#include "RTSUnitActor.h"
#include "BlastFxGround.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "RTSGameData.h"

namespace
{
	/** Sorteio só visual, fixo por unidade (o mesmo misturador do CosmeticHash do RTSUnitActor.cpp; nome próprio pro unity build). */
	uint32 AirHash(uint32 Id)
	{
		uint32 X = Id * 0x9E3779B9u;
		X ^= X >> 16; X *= 0x7feb352dU; X ^= X >> 15;
		return X;
	}

	UMaterialInterface* LoadAirMaterial(const TCHAR* Path)
	{
		return LoadObject<UMaterialInterface>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	/** O disco de um rotor no plano perpendicular ao eixo (0 x, 1 y, 2 z), do cubo à ponta: UV0 = (r ÷ R, ângulo ÷ 2π). */
	void BuildDisc(UProceduralMeshComponent* Mesh, int32 Axis, float RadiusCm, float HubCm)
	{
		constexpr int32 NA = 64, NR = 6;
		TArray<FVector> V;
		TArray<FVector> N;
		TArray<FVector2D> UV;
		TArray<int32> T;
		const FVector Normal = Axis == 0 ? FVector::XAxisVector : (Axis == 1 ? FVector::YAxisVector : FVector::ZAxisVector);
		const FVector A = Axis == 2 ? FVector::XAxisVector : (Axis == 1 ? FVector::XAxisVector : FVector::YAxisVector);
		const FVector B = FVector::CrossProduct(Normal, A);
		for (int32 J = 0; J <= NR; ++J)
		{
			const float R = FMath::Lerp(HubCm, RadiusCm, float(J) / float(NR));
			for (int32 I = 0; I <= NA; ++I)
			{
				const float Th = 2.0f * UE_PI * float(I) / float(NA);
				V.Add((A * FMath::Cos(Th) + B * FMath::Sin(Th)) * R);
				N.Add(Normal);
				UV.Add(FVector2D(R / RadiusCm, float(I) / float(NA)));
			}
		}
		for (int32 J = 0; J < NR; ++J)
		{
			for (int32 I = 0; I < NA; ++I)
			{
				const int32 P0 = J * (NA + 1) + I, P1 = P0 + 1, P2 = P0 + NA + 1, P3 = P2 + 1;
				T.Append({ P0, P2, P1, P1, P2, P3 });
			}
		}
		Mesh->CreateMeshSection_LinearColor(0, V, T, N, UV, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
	}

	/** A chama do pós-combustor: um cone de revolução da boca (origem) pra −X, o raio caindo até a ponta e um bojo depois da
	 *  boca (o jato sobre-expandido); UV0 = (x ÷ L, ângulo ÷ 2π). */
	void BuildFlame(UProceduralMeshComponent* Mesh, float RadiusCm, float LengthCm)
	{
		constexpr int32 NA = 20, NL = 24;
		TArray<FVector> V;
		TArray<FVector> N;
		TArray<FVector2D> UV;
		TArray<int32> T;
		for (int32 J = 0; J <= NL; ++J)
		{
			const float U = float(J) / float(NL);
			const float R = RadiusCm * (0.92f + 0.18f * FMath::Sin(UE_PI * FMath::Min(U * 2.2f, 1.0f))) * FMath::Pow(1.0f - U, 0.75f) + 2.0f;
			for (int32 I = 0; I <= NA; ++I)
			{
				const float Th = 2.0f * UE_PI * float(I) / float(NA);
				const FVector Radial(0.0f, FMath::Cos(Th), FMath::Sin(Th));
				V.Add(FVector(-U * LengthCm, 0.0f, 0.0f) + Radial * R);
				N.Add((Radial + FVector(-0.25f, 0.0f, 0.0f)).GetSafeNormal());
				UV.Add(FVector2D(U, float(I) / float(NA)));
			}
		}
		for (int32 J = 0; J < NL; ++J)
		{
			for (int32 I = 0; I < NA; ++I)
			{
				const int32 P0 = J * (NA + 1) + I, P1 = P0 + 1, P2 = P0 + NA + 1, P3 = P2 + 1;
				T.Append({ P0, P1, P2, P1, P3, P2 });
			}
		}
		Mesh->CreateMeshSection_LinearColor(0, V, T, N, UV, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
	}
}

void ARTSUnitActor::SetupAirFx(const FRTSUnitVisual& Visual)
{
	AirMassT = Visual.MassT;
	// o motor (a fumaça e o fogo da queda): o meio do casco, um pouco atrás e em cima
	if (HullParts.Num() && HullParts[0] && HullParts[0]->GetStaticMesh())
	{
		const FBox Box = HullParts[0]->GetStaticMesh()->GetBoundingBox();
		EngineLocalCm = FVector(Box.GetCenter().X - 0.12 * Box.GetExtent().X, 0.0, Box.Min.Z + 0.6 * (Box.Max.Z - Box.Min.Z));
	}
	// os discos dos rotores (o material M_RotorDisc de build_content.py; sem ele, só as pás de verdade)
	static TWeakObjectPtr<UMaterialInterface> DiscMat;
	if (!DiscMat.IsValid())
	{
		DiscMat = LoadAirMaterial(TEXT("/Game/Materials/M_RotorDisc.M_RotorDisc"));
	}
	for (int32 I = 0; I < SpinPivots.Num(); ++I)
	{
		if (!SpinIsRotor.IsValidIndex(I) || !SpinIsRotor[I] || !SpinBlades.IsValidIndex(I) || SpinBlades[I] <= 0 || SpinRadiusCm[I] <= 1.0f)
		{
			continue;
		}
		if (SpinParts.IsValidIndex(I) && SpinParts[I])
		{
			SpinParts[I]->SetCastHiddenShadow(true);   // a pá escondida no giro cheio continua fazendo a sombra tremida no chão
		}
		if (!DiscMat.IsValid())
		{
			continue;
		}
		UProceduralMeshComponent* Disc = NewObject<UProceduralMeshComponent>(this);
		Disc->SetupAttachment(Body);
		Disc->SetRelativeLocation(SpinPivots[I]->GetRelativeLocation());
		Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Disc->SetCastShadow(false);
		Disc->SetReceivesDecals(false);
		Disc->SetCanEverAffectNavigation(false);
		Disc->bUseAsyncCooking = false;
		const float R = SpinRadiusCm[I];
		const float Hub = FMath::Min(0.09f * R, 40.0f);
		BuildDisc(Disc, SpinAxis.IsValidIndex(I) ? SpinAxis[I] : 2, R, Hub);
		Disc->SetMaterial(0, DiscMat.Get());
		Disc->RegisterComponent();
		// os dados da primitiva: 0 a opacidade, 1 a fase das pás fantasma (rad), 2 as pás, 3 o cubo (÷ R), 4 a ponta pintada (÷ R),
		// 5 a solidez da pá na ponta (N·c ÷ 2πR: a cobertura média em r = R)
		Disc->SetCustomPrimitiveDataFloat(0, 0.0f);
		Disc->SetCustomPrimitiveDataFloat(1, 0.0f);
		Disc->SetCustomPrimitiveDataFloat(2, float(SpinBlades[I]));
		Disc->SetCustomPrimitiveDataFloat(3, Hub / R);
		Disc->SetCustomPrimitiveDataFloat(4, FMath::Clamp(SpinTipCm[I] / R, 0.0f, 0.3f));
		Disc->SetCustomPrimitiveDataFloat(5, float(SpinBlades[I]) * SpinChordCm[I] / (2.0f * UE_PI * R));
		Disc->SetVisibility(false);
		RotorDiscs.Add(Disc);
		RotorDiscSpin.Add(I);
		RotorDiscPhase.Add(float(AirHash(SimId + 7u * uint32(I)) % 628u) * 0.01f);
		RotorDiscBladeOff.Add(false);
	}
	// a rampa de trás (o Chinook): a malha com a origem na dobradiça, num pivô ali
	if (!Visual.RampMesh.IsEmpty())
	{
		if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Visual.RampMesh, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			RampPivot = NewObject<USceneComponent>(this);
			RampPivot->SetupAttachment(Body);
			RampPivot->SetRelativeLocation(Visual.RampHingeCm);
			RampPivot->RegisterComponent();
			HullParts.Add(AddPart(Mesh, RampPivot));
			RampOpenDeg = Visual.RampOpenDeg;
			RampOpenS = FMath::Max(Visual.RampOpenS, 0.1f);
		}
	}
	// o pós-combustor: uma chama por boca (o material M_Afterburner) e uma luz entre elas, 1,5 m atrás
	if (Visual.AfterburnerCm.Num())
	{
		static TWeakObjectPtr<UMaterialInterface> FlameMat;
		if (!FlameMat.IsValid())
		{
			FlameMat = LoadAirMaterial(TEXT("/Game/Materials/M_Afterburner.M_Afterburner"));
		}
		AfterburnerRadiance = Visual.AfterburnerRadiance;
		AfterburnerLightCd = Visual.AfterburnerLightCd;
		AfterburnerLenCm = Visual.AfterburnerLengthCm;
		FVector Mid = FVector::ZeroVector;
		for (const FVector& At : Visual.AfterburnerCm)
		{
			Mid += At / float(Visual.AfterburnerCm.Num());
			if (!FlameMat.IsValid())
			{
				continue;
			}
			UProceduralMeshComponent* F = NewObject<UProceduralMeshComponent>(this);
			F->SetupAttachment(Body);
			F->SetRelativeLocation(At);
			F->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			F->SetCastShadow(false);
			F->SetReceivesDecals(false);
			F->SetCanEverAffectNavigation(false);
			F->bUseAsyncCooking = false;
			BuildFlame(F, Visual.AfterburnerRadiusCm, Visual.AfterburnerLengthCm);
			F->SetMaterial(0, FlameMat.Get());
			F->RegisterComponent();
			F->SetCustomPrimitiveDataFloat(0, 0.0f);
			F->SetCustomPrimitiveDataFloat(1, AfterburnerRadiance);
			F->SetVisibility(false);
			Flames.Add(F);
		}
		FlameLight = NewObject<UPointLightComponent>(this);
		FlameLight->SetupAttachment(Body);
		FlameLight->SetRelativeLocation(Mid + FVector(-150.0, 0.0, 0.0));
		FlameLight->SetIntensityUnits(ELightUnits::Candelas);
		FlameLight->SetIntensity(0.0f);
		FlameLight->SetAttenuationRadius(2200.0f);
		FlameLight->SetLightColor(FLinearColor(1.0f, 0.55f, 0.24f));
		FlameLight->SetCastShadows(false);
		FlameLight->SetVisibility(false);
		FlameLight->RegisterComponent();
	}
	UE_LOG(LogTemp, Log, TEXT("RTS ar: %s com %d discos de rotor (material %s), rampa %d, %d chamas"), *Visual.Key, RotorDiscs.Num(),
		DiscMat.IsValid() ? TEXT("ok") : TEXT("FALTA"), RampPivot ? 1 : 0, Flames.Num());
}

void ARTSUnitActor::SetAfterburner(float Throttle01)
{
	AfterburnerWant = FMath::Clamp(Throttle01, 0.0f, 1.0f);
}

void ARTSUnitActor::SetRampOpen(bool bOpen)
{
	bRampWant = bOpen;
}

FVector ARTSUnitActor::GetEngineWorldCm() const
{
	return Body ? Body->GetComponentTransform().TransformPosition(EngineLocalCm) : GetActorLocation();
}

bool ARTSUnitActor::GetLiftRotors(TArray<FVector>& OutHubsCm, TArray<float>& OutRadiiCm) const
{
	OutHubsCm.Reset();
	OutRadiiCm.Reset();
	for (int32 I = 0; I < SpinPivots.Num(); ++I)
	{
		if (SpinIsRotor.IsValidIndex(I) && SpinIsRotor[I] && SpinAxis.IsValidIndex(I) && SpinAxis[I] == 2 && SpinRadiusCm.IsValidIndex(I)
			&& SpinRadiusCm[I] > 1.0f && SpinPivots[I])
		{
			OutHubsCm.Add(SpinPivots[I]->GetComponentLocation());
			OutRadiiCm.Add(SpinRadiusCm[I]);
		}
	}
	return OutHubsCm.Num() > 0;
}

bool ARTSUnitActor::IsRotorFrozen(int32 SpinIndex, float RateDps) const
{
	// (01/11, o A/B da leitura do Apache; o alvo: as 4 pás escuras e nítidas) a câmera de 24–60 qps "congela" o rotor (o efeito
	// estroboscópico das fotos e dos vídeos): no giro cheio a pá de verdade fica à vista, andando devagar com a pá fantasma do disco, em
	// vez de sumir no borrão. Só o rotor de 4 pás em cima (as do Apache a 45° + 90°·k no modelo, model_usa_comanche_v2.py main_rotor)
	static const bool bOn = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("RTSRotorFreeze="), V); return V != 0; }();
	return bOn && RateDps > 950.0f && SpinBlades.IsValidIndex(SpinIndex) && SpinBlades[SpinIndex] == 4 && SpinAxis.IsValidIndex(SpinIndex)
		&& SpinAxis[SpinIndex] == 2 && RotorDiscSpin.Contains(SpinIndex);
}

float ARTSUnitActor::RotorShownDeg(int32 SpinIndex, float SpinDeg, float RateDps) const
{
	const int32 K = RotorDiscSpin.Find(SpinIndex);
	if (K == INDEX_NONE || !IsRotorFrozen(SpinIndex, RateDps) || RotorDiscBladeOff[K])
	{
		return SpinDeg;
	}
	// a pá fantasma fica no ângulo da fase (o disco mede de +X, BuildDisc); a pá 0 do modelo está a 45°
	return FMath::RadiansToDegrees(RotorDiscPhase[K]) - 45.0f;
}

void ARTSUnitActor::TickAirFx(float DeltaSeconds)
{
	const float Dt = FMath::Max(DeltaSeconds, 0.0f);
	// os discos: a pá de verdade até ~950°/s (16° por quadro a 60 qps: o TSR ainda segue), o disco entra de 500 a 1.100°/s
	for (int32 K = 0; K < RotorDiscs.Num(); ++K)
	{
		UProceduralMeshComponent* Disc = RotorDiscs[K];
		const int32 I = RotorDiscSpin[K];
		if (!Disc || !SpinRpm.IsValidIndex(I))
		{
			continue;
		}
		const float Rate = RotorDiscBladeOff[K] ? 0.0f : FMath::Abs(SpinRpm[I]) * 6.0f * AirRotor;
		const float DiscK = FMath::SmoothStep(500.0f, 1100.0f, Rate);
		// as pás fantasma andam devagar pro lado do giro (a câmera de 24–60 qps vê o rotor quase parado, e lê o giro)
		RotorDiscPhase[K] = FMath::Fmod(RotorDiscPhase[K] + FMath::Sign(SpinRpm[I]) * 0.7f * DiscK * Dt + 2.0f * UE_PI, 2.0f * UE_PI);
		const bool bDisc = DiscK > 0.01f;
		if (Disc->IsVisible() != bDisc)
		{
			Disc->SetVisibility(bDisc);
		}
		if (bDisc)
		{
			Disc->SetCustomPrimitiveDataFloat(0, DiscK);
			Disc->SetCustomPrimitiveDataFloat(1, RotorDiscPhase[K]);
		}
		if (UStaticMeshComponent* Blades = SpinParts.IsValidIndex(I) ? SpinParts[I].Get() : nullptr; Blades && !RotorDiscBladeOff[K])
		{
			const bool bHide = Rate > 950.0f && !IsRotorFrozen(I, Rate);
			if (bool(Blades->bHiddenInGame) != bHide)
			{
				Blades->SetHiddenInGame(bHide);
			}
			// sob a névoa a sombra da pá escondida também some (a sombra entregaria a posição)
			const bool bShadow = !bFogHidden;
			if (bool(Blades->bCastHiddenShadow) != bShadow)
			{
				Blades->SetCastHiddenShadow(bShadow);
			}
		}
	}
	// a rampa: a ponta desce até o chão em RampOpenS (o atuador hidráulico, devagar no fim)
	if (RampPivot)
	{
		const float Want = bRampWant ? RampOpenDeg : 0.0f;
		const float Step = RampOpenDeg / RampOpenS * Dt;
		const float Before = RampAngle;
		RampAngle = FMath::Abs(Want - RampAngle) <= Step ? Want : RampAngle + FMath::Sign(Want - RampAngle) * Step;
		if (RampAngle != Before)
		{
			const float E = RampOpenDeg > 0.0f ? RampAngle / RampOpenDeg : 0.0f;
			RampPivot->SetRelativeRotation(FRotator(RampOpenDeg * FMath::SmoothStep(0.0f, 1.0f, E), 0.0f, 0.0f));
		}
	}
	// o pós-combustor: acende rápido (o bocal abre em ~0,3 s) e apaga mais devagar
	if (Flames.Num() || FlameLight)
	{
		const float Tau = AfterburnerWant > AfterburnerShown ? 0.12f : 0.45f;
		AfterburnerShown += (AfterburnerWant - AfterburnerShown) * (1.0f - FMath::Exp(-Dt / Tau));
		const float A = AfterburnerShown < 0.01f ? 0.0f : AfterburnerShown;
		const float Flicker = 0.92f + 0.08f * FMath::Sin(float(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) * 53.0f + float(SimId));
		for (UProceduralMeshComponent* F : Flames)
		{
			if (!F)
			{
				continue;
			}
			if (F->IsVisible() != (A > 0.0f))
			{
				F->SetVisibility(A > 0.0f);
			}
			if (A > 0.0f)
			{
				F->SetCustomPrimitiveDataFloat(0, A);
				// a chama cresce com o empuxo (o comprimento do núcleo ∝ a vazão)
				F->SetRelativeScale3D(FVector(0.45f + 0.55f * A, 0.85f + 0.15f * A, 0.85f + 0.15f * A));
			}
		}
		if (FlameLight)
		{
			if (FlameLight->IsVisible() != (A > 0.0f))
			{
				FlameLight->SetVisibility(A > 0.0f);
			}
			FlameLight->SetIntensity(AfterburnerLightCd * A * Flicker);
		}
	}
}

void ARTSUnitActor::SetBladeOff()
{
	// (v5, escopo B 24/10) a pá voou (o AirBladeOff): o rotor principal se solta e voa com a velocidade da aeronave, mais o empurrão
	// do giro (a pá sai pela tangente: ~1/4 da velocidade da ponta) e um pouco pra cima, girando no próprio eixo; cai e fica no chão
	for (int32 I = 0; I < SpinPivots.Num() && I < SpinParts.Num(); ++I)
	{
		if (!SpinIsRotor.IsValidIndex(I) || !SpinIsRotor[I] || !SpinAxis.IsValidIndex(I) || SpinAxis[I] != 2 || !SpinParts[I])
		{
			continue;
		}
		UStaticMeshComponent* Part = SpinParts[I];
		const FTransform W = Part->GetComponentTransform();
		Part->SetUsingAbsoluteLocation(true);
		Part->SetUsingAbsoluteRotation(true);
		Part->SetWorldTransform(W);
		Part->SetHiddenInGame(false);
		const uint32 H = AirHash(SimId * 31u + uint32(I));
		const float Ang = float(H % 360u) * UE_PI / 180.0f;
		const float TipMs = FMath::Abs(SpinRpm[I]) * 2.0f * UE_PI / 60.0f * (SpinRadiusCm.IsValidIndex(I) ? SpinRadiusCm[I] / 100.0f : 4.0f);
		FFlyingBlade& B = FlyingBlades.AddDefaulted_GetRef();
		B.Part = Part;
		B.Vel = AirVelCm + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.0f) * (0.25f * TipMs * 100.0f) + FVector(0.0f, 0.0f, 450.0f);
		B.Axis = FVector(0.15f * FMath::Cos(Ang * 3.0f), 0.15f * FMath::Sin(Ang * 3.0f), 1.0f).GetSafeNormal();
		B.SpinDps = FMath::Sign(SpinRpm[I]) * 720.0f;
		for (int32 K = 0; K < RotorDiscs.Num(); ++K)
		{
			if (RotorDiscSpin[K] == I)
			{
				RotorDiscBladeOff[K] = true;
				if (RotorDiscs[K])
				{
					RotorDiscs[K]->SetVisibility(false);
				}
			}
		}
	}
}

void ARTSUnitActor::TickFlyingBlades(float DeltaSeconds)
{
	const float Dt = FMath::Min(DeltaSeconds, 0.05f);
	for (FFlyingBlade& B : FlyingBlades)
	{
		UStaticMeshComponent* Part = B.Part;
		if (!Part || B.bLanded)
		{
			continue;
		}
		B.Vel.Z -= 981.0f * Dt;
		B.Vel *= FMath::Exp(-0.25f * Dt);   // o arrasto da pá larga
		FVector P = Part->GetComponentLocation() + B.Vel * Dt;
		const double Gz = BlastFxGround::HeightAtCached(GetWorld(), P.X, P.Y);
		Part->AddWorldRotation(FQuat(B.Axis, FMath::DegreesToRadians(B.SpinDps * Dt)));
		B.SpinDps *= FMath::Exp(-0.15f * Dt);
		if (P.Z <= Gz + 12.0)
		{
			// no chão: deitada (o giro que sobra vira o rumo dela)
			P.Z = Gz + 12.0;
			const float Yaw = Part->GetComponentRotation().Yaw;
			Part->SetWorldRotation(FRotator(0.0f, Yaw, float(AirHash(SimId) % 9u) - 4.0f));
			B.bLanded = true;
		}
		Part->SetWorldLocation(P);
	}
}

void ARTSUnitActor::StopAirFx()
{
	for (UProceduralMeshComponent* Disc : RotorDiscs)
	{
		if (Disc)
		{
			Disc->SetVisibility(false);
		}
	}
	for (int32 K = 0; K < RotorDiscs.Num(); ++K)
	{
		// a carcaça mostra as pás de verdade (paradas), as que não voaram
		const int32 I = RotorDiscSpin[K];
		if (SpinParts.IsValidIndex(I) && SpinParts[I] && !RotorDiscBladeOff[K])
		{
			SpinParts[I]->SetHiddenInGame(false);
		}
	}
	for (UProceduralMeshComponent* F : Flames)
	{
		if (F)
		{
			F->SetVisibility(false);
		}
	}
	if (FlameLight)
	{
		FlameLight->SetVisibility(false);
	}
	AfterburnerWant = AfterburnerShown = 0.0f;
}
