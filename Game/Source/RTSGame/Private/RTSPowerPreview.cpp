#include "RTSPowerPreview.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RTSMapBaker.h"
#include "RTSMatchSubsystem.h"
#include "RTSOrderMarker.h"
#include "RTSPlayerController.h"
#include "RTSSoftDraw.h"

using RTSPowerPreview::EKind;
using RTSPowerPreview::EShape;
using RTSPowerPreview::FSpec;

// (06/11) A mira mora NO CHÃO: um pós-processo (/Game/UI/M_PowerPreview, gerado por Game/Tools/make_power_preview.py) desenha a forma
// pela profundidade da cena, colada no relevo, por baixo das unidades e dos prédios vivos, como luz no chão. A v1 (05/11) era no Canvas:
// linhas finas por cima de tudo (o Felipe: "parece que é um png em cima do chão"); o código dela ficou em docs/poderes/mira_v1_canvas.cpp. Aqui o C++
// só passa os números da forma ao material a cada quadro; o Canvas escreve o motivo do inválido no mouse e a régua do traçado.

namespace
{
	// as cores da prancha (mira.html): golpe o vermelho do atacar, apoio o azul, tropas o céu, olhos a térmica; o cinza do inválido
	const FColor KindColor[4] = { FColor(255, 59, 48), FColor(89, 178, 255), FColor(127, 230, 255), FColor(243, 241, 236) };
	const FColor Off(154, 160, 166);
	const FColor Ink(10, 11, 12);
	constexpr float LineW = 2.4f;   // o risco do inválido no mouse (× a escala da tela)
	constexpr int32 MaxPath = 24, MaxDrop = 10, MaxAffected = 16;

	FSpec GSpec;
	uint64 GShownFrame = 0;
	uint32 GTargetId = 0;
	double GTargetAt = 0.0;

	// o pós-processo na câmera do jogador (a câmera segura o material: aqui só o fraco)
	TWeakObjectPtr<UMaterialInstanceDynamic> GMid;
	TWeakObjectPtr<UCameraComponent> GCamera;

	struct FView
	{
		UCanvas* Canvas = nullptr;
		ARTSPlayerController* PC = nullptr;
		const URTSMatchSubsystem* Match = nullptr;
		const FRTSBakedMap* Map = nullptr;
		float S = 1.0f;
		double T = 0.0;
	};

	float Ease(float X)
	{
		X = FMath::Clamp(X, 0.0f, 1.0f);
		return 1.0f - FMath::Pow(1.0f - X, 3.0f);
	}

	FName Param(const TCHAR* Prefix, int32 K)
	{
		// os nomes das entradas do material (Q0..Q11, D0..D4, U0..U15), feitos uma vez
		static TMap<FString, FName> Names;
		const FString Key = FString::Printf(TEXT("%s%d"), Prefix, K);
		if (const FName* Found = Names.Find(Key))
		{
			return *Found;
		}
		return Names.Add(Key, FName(*Key));
	}

	UMaterialInstanceDynamic* EnsureMid(ARTSPlayerController* PC)
	{
		AActor* View = PC ? PC->GetViewTarget() : nullptr;
		UCameraComponent* Camera = View ? View->FindComponentByClass<UCameraComponent>() : nullptr;
		if (!Camera)
		{
			return nullptr;
		}
		if (GCamera.Get() == Camera && GMid.IsValid())
		{
			return GMid.Get();
		}
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/UI/M_PowerPreview.M_PowerPreview"));
		if (!Base)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, Camera);
		Mid->SetVectorParameterValue(TEXT("Pk"), FLinearColor(0.0f, 0.0f, 0.0f, -1.0f));
		// peso 0 enquanto não mira: o pós-processo nem roda
		Camera->PostProcessSettings.WeightedBlendables.Array.Add(FWeightedBlendable(0.0f, Mid));
		Camera->PostProcessBlendWeight = 1.0f;
		GMid = Mid;
		GCamera = Camera;
		return Mid;
	}

	void SetOn(bool bOn)
	{
		UCameraComponent* Camera = GCamera.Get();
		UMaterialInstanceDynamic* Mid = GMid.Get();
		if (!Camera || !Mid)
		{
			return;
		}
		for (FWeightedBlendable& B : Camera->PostProcessSettings.WeightedBlendables.Array)
		{
			if (B.Object == Mid)
			{
				B.Weight = bOn ? 1.0f : 0.0f;
			}
		}
		if (!bOn)
		{
			Mid->SetVectorParameterValue(TEXT("Pk"), FLinearColor(0.0f, 0.0f, 0.0f, -1.0f));
		}
	}

	void Label(const FView& V, const FVector2D& At, const FString& Text, const FLinearColor& Col)
	{
		if (Text.IsEmpty() || !GEngine)
		{
			return;
		}
		FCanvasTextItem Item(At, FText::FromString(Text.ToUpper()), GEngine->GetMediumFont(), Col);
		Item.Scale = FVector2D(V.S, V.S);
		Item.bOutlined = true;
		Item.OutlineColor = FLinearColor(Ink);
		Item.EnableShadow(FLinearColor(Ink).CopyWithNewOpacity(0.8f));
		V.Canvas->DrawItem(Item);
	}

	// o corte do inválido no mouse e o motivo ao lado
	void Refuse(const FView& V, const FString& Reason)
	{
		float Mx = 0.0f, My = 0.0f;
		if (!V.PC->GetMousePosition(Mx, My))
		{
			return;
		}
		const FLinearColor Col(Off);
		const TArray<FVector2D> Cut = { FVector2D(Mx - 11.0f * V.S, My + 11.0f * V.S), FVector2D(Mx + 11.0f * V.S, My - 11.0f * V.S) };
		RTSSoftDraw::Stroke(V.Canvas, Cut, (LineW + 2.4f) * V.S, FLinearColor(Ink));
		RTSSoftDraw::Stroke(V.Canvas, Cut, LineW * V.S, Col);
		Label(V, FVector2D(Mx + 18.0f * V.S, My + 16.0f * V.S), Reason, Col);
	}

	// o traçado no comprimento de verdade (o que passa do poder vira cinza no material); até MaxPath pontos, pelo comprimento
	TArray<FVector2D> PathPoints(const TArray<FVector2D>& In, float& OutLength)
	{
		OutLength = 0.0f;
		for (int32 K = 1; K < In.Num(); ++K)
		{
			OutLength += float(FVector2D::Distance(In[K - 1], In[K]));
		}
		if (In.Num() <= MaxPath)
		{
			return In;
		}
		TArray<FVector2D> Out;
		Out.Add(In[0]);
		float Walk = 0.0f;
		int32 Seg = 1;
		for (int32 J = 1; J < MaxPath - 1; ++J)
		{
			const float Want = OutLength * J / (MaxPath - 1);
			while (Seg < In.Num() - 1 && Walk + float(FVector2D::Distance(In[Seg - 1], In[Seg])) < Want)
			{
				Walk += float(FVector2D::Distance(In[Seg - 1], In[Seg]));
				++Seg;
			}
			const float L = FMath::Max(float(FVector2D::Distance(In[Seg - 1], In[Seg])), 1.0f);
			Out.Add(In[Seg - 1] + (In[Seg] - In[Seg - 1]) * FMath::Clamp((Want - Walk) / L, 0.0f, 1.0f));
		}
		Out.Add(In.Last());
		return Out;
	}

	// os números da forma pro material (as entradas do make_power_preview.py)
	void Upload(const FView& V, const FSpec& P, UMaterialInstanceDynamic* Mid)
	{
		const FVector2D C = P.CenterCm;
		const float Z0 = V.Map->GroundZCm(C);
		const FLinearColor Col = P.bValid ? FLinearColor(KindColor[FMath::Min<uint8>(uint8(P.Kind), 3)]) : FLinearColor(Off);
		const FVector2D Dir = P.Dir.IsNearlyZero() ? FVector2D(1.0, 0.0) : P.Dir.GetSafeNormal();
		const float T = float(FMath::Fmod(V.T, 3600.0));
		float Bound = 600.0f;

		// quem o poder pega (o anel debaixo de cada um) e o alvo único
		TArray<FRTSUnitState> Units;
		if ((!P.AffectedIds.IsEmpty() && P.bValid) || P.Shape == EShape::Target)
		{
			V.Match->GetUnits(Units);
		}
		int32 NumAffected = 0;
		if (P.bValid)
		{
			for (const FRTSUnitState& U : Units)
			{
				if (NumAffected < MaxAffected && P.AffectedIds.Contains(U.Id))
				{
					const float R = FMath::Max(250.0f, RTSOrderMarker::RadiusOf(V.Match, U.Id) * 1.1f);
					const FVector2D Rel = U.PosCm - C;
					Mid->SetVectorParameterValue(Param(TEXT("U"), NumAffected++), FLinearColor(float(Rel.X), float(Rel.Y), R, 1.0f));
					Bound = FMath::Max(Bound, float(Rel.Size()) + R + 300.0f);
				}
			}
		}
		FLinearColor Target(0.0f, 0.0f, 0.0f, 0.0f);
		if (P.Shape == EShape::Target)
		{
			const FRTSUnitState* U = P.TargetId ? Units.FindByPredicate([&P](const FRTSUnitState& X) { return X.Id == P.TargetId; }) : nullptr;
			if (U)
			{
				// a trava fecha no alvo num estalo de 0,25 s quando ele muda
				if (GTargetId != P.TargetId)
				{
					GTargetId = P.TargetId;
					GTargetAt = V.T;
				}
				const float K = Ease(float(V.T - GTargetAt) / 0.25f);
				const float R = FMath::Max(250.0f, RTSOrderMarker::RadiusOf(V.Match, P.TargetId) * 1.15f) * (1.4f - 0.4f * K);
				const FVector2D Rel = U->PosCm - C;
				Target = FLinearColor(float(Rel.X), float(Rel.Y), R, 1.0f);
				Bound = FMath::Max(Bound, float(Rel.Size()) + R * 1.6f + 300.0f);
			}
			else
			{
				// sem alvo que vale: o anel cinza tracejado no mouse
				GTargetId = 0;
				Target = FLinearColor(0.0f, 0.0f, 300.0f, 1.0f);
			}
		}

		float PathLen = 0.0f;
		int32 Count = 0;
		switch (P.Shape)
		{
		case EShape::Circle:
		case EShape::Orbit:
			Bound = FMath::Max(Bound, FMath::Max(P.RadiusCm + FMath::Max(150.0f, P.RadiusCm * 0.08f), P.ScatterCm) + 400.0f);
			break;
		case EShape::Strip:
			Bound = FMath::Max(Bound, P.LengthCm + P.WidthCm + 400.0f);
			break;
		case EShape::Path:
		{
			const TArray<FVector2D> Pts = PathPoints(P.PathCm, PathLen);
			Count = Pts.Num();
			for (int32 K = 0; K < (MaxPath + 1) / 2; ++K)
			{
				const FVector2D A = Pts.IsValidIndex(2 * K) ? Pts[2 * K] - C : FVector2D::ZeroVector;
				const FVector2D B = Pts.IsValidIndex(2 * K + 1) ? Pts[2 * K + 1] - C : A;
				Mid->SetVectorParameterValue(Param(TEXT("Q"), K), FLinearColor(float(A.X), float(A.Y), float(B.X), float(B.Y)));
				Bound = FMath::Max(Bound, float(FMath::Max(A.Size(), B.Size())) + 500.0f);
			}
			break;
		}
		case EShape::Drop:
		{
			Count = FMath::Min(P.DropCm.Num(), MaxDrop);
			for (int32 K = 0; K < (MaxDrop + 1) / 2; ++K)
			{
				const FVector2D A = P.DropCm.IsValidIndex(2 * K) ? P.DropCm[2 * K] - C : FVector2D::ZeroVector;
				const FVector2D B = P.DropCm.IsValidIndex(2 * K + 1) ? P.DropCm[2 * K + 1] - C : A;
				Mid->SetVectorParameterValue(Param(TEXT("D"), K), FLinearColor(float(A.X), float(A.Y), float(B.X), float(B.Y)));
				Bound = FMath::Max(Bound, float(FMath::Max(A.Size(), B.Size())) + 800.0f);
			}
			Bound = FMath::Max(Bound, P.RadiusCm + 400.0f);
			break;
		}
		case EShape::Target:
			break;
		}

		// o chão em volta: o plano pelo relevo a ±Rf do centro (o que está bem acima dele, a copa, o telhado, não recebe a área)
		const float Rf = FMath::Clamp(Bound * 0.5f, 500.0f, 4000.0f);
		const float Gx = (V.Map->GroundZCm(C + FVector2D(Rf, 0.0)) - V.Map->GroundZCm(C - FVector2D(Rf, 0.0))) / (2.0f * Rf);
		const float Gy = (V.Map->GroundZCm(C + FVector2D(0.0, Rf)) - V.Map->GroundZCm(C - FVector2D(0.0, Rf))) / (2.0f * Rf);

		// cm por pixel no centro (a largura das linhas e o tamanho dos traços no material)
		float CmPerPx = 5.0f;
		FVector2D S0, S1;
		const FVector Right = V.PC->PlayerCameraManager ? V.PC->PlayerCameraManager->GetCameraRotation().RotateVector(FVector::RightVector) : FVector::RightVector;
		const FVector At(C.X, C.Y, Z0);
		if (V.PC->ProjectWorldLocationToScreen(At, S0) && V.PC->ProjectWorldLocationToScreen(At + Right * 100.0f, S1))
		{
			CmPerPx = 100.0f / FMath::Max(float(FVector2D::Distance(S0, S1)), 0.01f);
		}

		Mid->SetVectorParameterValue(TEXT("Pc"), FLinearColor(float(C.X), float(C.Y), Z0, float(uint8(P.Shape))));
		Mid->SetVectorParameterValue(TEXT("Pk"), FLinearColor(Col.R, Col.G, Col.B, P.bValid ? 1.0f : 0.0f));
		Mid->SetVectorParameterValue(TEXT("Pa"), FLinearColor(P.RadiusCm, P.ScatterCm, P.SquashY, T));
		Mid->SetVectorParameterValue(TEXT("Pd"), FLinearColor(float(Dir.X), float(Dir.Y), P.LengthCm, P.WidthCm));
		Mid->SetVectorParameterValue(TEXT("Pe"), FLinearColor(float(FMath::Max(1, P.Rows)), P.PathMaxCm, float(Count), float(NumAffected)));
		Mid->SetVectorParameterValue(TEXT("Pf"), FLinearColor(CmPerPx, float(V.Canvas->SizeY), Bound, float(uint8(P.Kind))));
		Mid->SetVectorParameterValue(TEXT("Pg"), FLinearColor(Gx, Gy, 0.0f, 0.0f));
		Mid->SetVectorParameterValue(TEXT("Tg"), Target);
	}

	// o que fica na tela (não é do chão): o motivo do inválido no mouse e a régua do traçado
	void DrawScreen(const FView& V, const FSpec& P)
	{
		if (P.Shape == EShape::Path && P.PathMaxCm > 0.0f && P.PathCm.Num() >= 2)
		{
			float Used = 0.0f;
			for (int32 K = 1; K < P.PathCm.Num(); ++K)
			{
				Used += float(FVector2D::Distance(P.PathCm[K - 1], P.PathCm[K]));
			}
			FVector2D End;
			const FVector2D Last = P.PathCm.Last();
			if (V.PC->ProjectWorldLocationToScreen(FVector(Last.X, Last.Y, V.Map->GroundZCm(Last)), End))
			{
				const FLinearColor Good = P.bValid ? FLinearColor(KindColor[FMath::Min<uint8>(uint8(P.Kind), 3)]) : FLinearColor(Off);
				Label(V, End + FVector2D(16.0f, 18.0f) * V.S, FString::Printf(TEXT("%d / %d m"), FMath::RoundToInt(FMath::Min(Used, P.PathMaxCm) / 100.0f), FMath::RoundToInt(P.PathMaxCm / 100.0f)), Used > P.PathMaxCm ? FLinearColor(Off) : Good);
			}
		}
		if (!P.bValid || (P.Shape == EShape::Target && P.TargetId == 0))
		{
			Refuse(V, P.Reason);
		}
	}

	bool MakeView(UCanvas* Canvas, ARTSPlayerController* PC, FView& V)
	{
		UWorld* World = PC ? PC->GetWorld() : nullptr;
		V.Match = World ? World->GetSubsystem<URTSMatchSubsystem>() : nullptr;
		V.Map = V.Match ? V.Match->BakedMap() : nullptr;
		if (!Canvas || !V.Map)
		{
			return false;
		}
		V.Canvas = Canvas;
		V.PC = PC;
		V.S = FMath::Max(0.5f, Canvas->SizeY / 900.0f);
		V.T = World->GetRealTimeSeconds();
		return true;
	}
}

void RTSPowerPreview::Show(const FSpec& Spec)
{
	GSpec = Spec;
	GShownFrame = GFrameCounter;
}

void RTSPowerPreview::Hide()
{
	GShownFrame = 0;
}

bool RTSPowerPreview::IsShown()
{
	return GShownFrame != 0 && GFrameCounter - GShownFrame <= 1;
}

void RTSPowerPreview::Draw(UCanvas* Canvas, ARTSPlayerController* PC)
{
	// sem o Show deste quadro (ou do anterior), a mira já saiu: o pós-processo desliga
	FView V;
	UMaterialInstanceDynamic* Mid = EnsureMid(PC);
	if (!IsShown() || !MakeView(Canvas, PC, V) || !Mid)
	{
		SetOn(false);
		return;
	}
	Upload(V, GSpec, Mid);
	SetOn(true);
	DrawScreen(V, GSpec);
}

void RTSPowerPreview::DrawTest(UCanvas* Canvas, ARTSPlayerController* PC)
{
	static FVector2D AtM;
	static const bool bOn = [] {
		FString Arg, X, Y;
		if (!FParse::Value(FCommandLine::Get(), TEXT("-RTSPowerPreviewAt="), Arg, false) || !Arg.Split(TEXT(","), &X, &Y))
		{
			return false;
		}
		AtM = FVector2D(FCString::Atof(*X), FCString::Atof(*Y));
		return true;
	}();
	FView V;
	if (!bOn || !MakeView(Canvas, PC, V))
	{
		return;
	}
	// uma forma a cada 2,5 s, em 3 voltas: a 1ª válida, a 2ª válida com o círculo de apoio, a 3ª inválida. O Show vale pro Draw do
	// quadro seguinte (o HUD chama o Draw antes deste)
	const int32 Step = int32(V.T / 2.5);
	FSpec P;
	P.CenterCm = AtM * 100.0;
	P.Shape = EShape(Step % 6);
	P.bValid = (Step / 6) % 3 != 2;
	P.Reason = TEXT("No vision");
	switch (P.Shape)
	{
	case EShape::Circle:
	{
		// a 1ª volta: o golpe com o espalhamento; a 2ª: o apoio (o Emergency Repair, 26 m) acendendo quem está dentro
		const bool bSupport = (Step / 6) % 3 == 1;
		P.Kind = bSupport ? EKind::Support : EKind::Strike;
		P.RadiusCm = bSupport ? 2600.0f : 1800.0f;
		P.ScatterCm = bSupport ? 0.0f : 1100.0f;
		if (bSupport)
		{
			TArray<FRTSUnitState> Units;
			V.Match->GetUnits(Units);
			for (const FRTSUnitState& U : Units)
			{
				if (!U.bStructure && FVector2D::Distance(U.PosCm, P.CenterCm) <= P.RadiusCm) { P.AffectedIds.Add(U.Id); }
			}
		}
		break;
	}
	case EShape::Strip: P.Kind = EKind::Strike; P.LengthCm = 5000.0f; P.WidthCm = 1000.0f; P.Rows = 3; P.Dir = FVector2D(FMath::Cos(float(V.T) * 0.3f), FMath::Sin(float(V.T) * 0.3f)); break;
	case EShape::Path:
		P.Kind = EKind::Support; P.PathMaxCm = 12000.0f;
		for (int32 K = 0; K <= 40; ++K)
		{
			const float X = K * 350.0f * FMath::Min(1.0f, FMath::Fmod(float(V.T), 2.5f) / 2.0f);
			P.PathCm.Add(P.CenterCm + FVector2D(X, 600.0f * FMath::Sin(X / 1100.0f)));
		}
		break;
	case EShape::Drop:
		P.Kind = EKind::Troops; P.RadiusCm = 1100.0f;
		for (const FVector2D& D : { FVector2D(0, 0), FVector2D(500, 200), FVector2D(-400, 400), FVector2D(200, -600), FVector2D(-600, -300), FVector2D(700, -400), FVector2D(-200, 800), FVector2D(800, 600), FVector2D(-800, 200) })
		{
			P.DropCm.Add(P.CenterCm + D);
		}
		break;
	case EShape::Target:
	{
		P.Kind = EKind::Strike;
		TArray<FRTSUnitState> Units;
		V.Match->GetUnits(Units);
		float Best = TNumericLimits<float>::Max();
		for (const FRTSUnitState& U : Units)
		{
			const float D = float(FVector2D::DistSquared(U.PosCm, P.CenterCm));
			if (D < Best) { Best = D; P.TargetId = U.Id; }
		}
		P.Reason = TEXT("No target");
		break;
	}
	case EShape::Orbit: P.Kind = EKind::Strike; P.RadiusCm = 3000.0f; break;
	}
	// a órbita da 2ª volta vira os olhos (o raio de visão do drone: a varredura do radar)
	if (P.Shape == EShape::Orbit && (Step / 6) % 3 == 1)
	{
		P.Shape = EShape::Circle;
		P.Kind = EKind::Eyes;
		P.RadiusCm = 2200.0f;
	}
	Show(P);
}
