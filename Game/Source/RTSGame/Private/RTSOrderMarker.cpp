#include "RTSOrderMarker.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RTSMapBaker.h"
#include "RTSMatchSubsystem.h"
#include "RTSPlayerController.h"
#include "RTSSoftDraw.h"
#include "RTSUnitActor.h"

namespace
{
	// a marca na grade de 64 dos cursores (o centro em 32, 32): 7,1 cm por unidade dá ~1,8 m de raio no começo do anel (26
	// unidades), a medida da prancha contra o tanque. De longe ela não fica menor que 0,95 px por unidade (S = 1 em 900 linhas)
	constexpr float UnitCm = 7.1f;
	constexpr float MinPxPerUnit = 0.95f;
	constexpr float LiftCm = 25.0f;
	constexpr float OutlineUnits = 2.6f;   // o contorno escuro do cursor (a cópia mais grossa embaixo)
	constexpr float RingUnits = 3.4f, GapUnits = 7.0f;   // o anel cortado da mira (cursors.html: ring(r, 3,4, ...))
	const FColor Ink(10, 11, 12);
	const FColor KindColor[3] = { FColor(79, 224, 90), FColor(255, 59, 48), FColor(255, 178, 31) };   // --G, --R, --A

	float Ease(float X)
	{
		X = FMath::Clamp(X, 0.0f, 1.0f);
		return 1.0f - FMath::Pow(1.0f - X, 3.0f);
	}

	float Ramp(float X, float A, float B)
	{
		return FMath::Clamp((X - A) / (B - A), 0.0f, 1.0f);
	}
}

void RTSOrderMarker::Draw(UCanvas* Canvas, ARTSPlayerController* PC, const FVector& World, float Age, uint8 Kind, float RadiusCm, bool bCross)
{
	if (!Canvas || !PC || !PC->PlayerCameraManager)
	{
		return;
	}
	const float Fade = 1.0f - Ramp(Age, 0.62f, Seconds);
	if (Fade <= 0.0f)
	{
		return;
	}
	// o chão em volta do ponto, com os eixos da câmera (a grade fica de frente pra tela, como o cursor)
	const float Yaw = FMath::DegreesToRadians(PC->PlayerCameraManager->GetCameraRotation().Yaw);
	const FVector Fwd(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0), Right(-FMath::Sin(Yaw), FMath::Cos(Yaw), 0.0);
	const FVector Base = World + FVector(0.0, 0.0, LiftCm);
	FVector2D C, Cr;
	if (!PC->ProjectWorldLocationToScreen(Base, C) || !PC->ProjectWorldLocationToScreen(Base + Right * 100.0, Cr))
	{
		return;
	}
	const float S = FMath::Max(0.5f, Canvas->SizeY / 900.0f);
	const float PxPerCm = float(FVector2D::Distance(C, Cr)) / 100.0f;
	const float CmPerUnit = FMath::Max(UnitCm, MinPxPerUnit * S / FMath::Max(PxPerCm, 1e-4f));
	const float Ppu = PxPerCm * CmPerUnit;
	const auto At = [&](const FVector2D& G)
	{
		FVector2D P;
		PC->ProjectWorldLocationToScreen(Base + (Right * G.X - Fwd * G.Y) * CmPerUnit, P);
		return P;
	};
	const FLinearColor Dark = FLinearColor(Ink).CopyWithNewOpacity(Fade);
	const FLinearColor Col = FLinearColor(KindColor[FMath::Min<uint8>(Kind, 2)]).CopyWithNewOpacity(Fade);
	// o anel encolhe em 0,4 s: no chão, de 26 a 11 unidades (o da prancha); no alvo, de 1,6 a 1,05 do raio dele
	const bool bTarget = RadiusCm > 0.0f;
	const float K = Ease(Age / 0.4f);
	const float R = bTarget ? (RadiusCm / CmPerUnit) * (1.6f - 0.55f * K) : 26.0f - 15.0f * K;
	// os quatro arcos (os cortes nas diagonais, onde as setinhas apontam pro meio) e as setinhas, o contorno embaixo de tudo
	TArray<TArray<FVector2D>> Arcs;
	const float Quarter = 2.0f * PI * R / 4.0f;
	const float Arc = Quarter - GapUnits - RingUnits;
	if (Arc > 0.5f)
	{
		const float Half = 0.5f * Arc / R;
		const int32 N = FMath::Clamp(FMath::CeilToInt32(Arc / 2.0f), 4, 48);
		for (int32 Q = 0; Q < 4; ++Q)
		{
			TArray<FVector2D>& Pts = Arcs.AddDefaulted_GetRef();
			const float Mid = 0.5f * PI * Q;
			for (int32 J = 0; J <= N; ++J)
			{
				const float A = Mid - Half + 2.0f * Half * float(J) / N;
				Pts.Add(At(FVector2D(R * FMath::Cos(A), R * FMath::Sin(A))));
			}
		}
	}
	TArray<TArray<FVector2D>> Tris;
	for (int32 Q = 0; Q < 4; ++Q)
	{
		const float A = 0.25f * PI + 0.5f * PI * Q;
		const FVector2D U(FMath::Cos(A), FMath::Sin(A)), Side(-U.Y, U.X);
		Tris.Add({ At(U * (R - 5.0f)), At(U * (R + 2.0f) + Side * 3.6f), At(U * (R + 2.0f) - Side * 3.6f) });
	}
	for (const TArray<FVector2D>& Pts : Arcs)
	{
		RTSSoftDraw::Stroke(Canvas, Pts, (RingUnits + OutlineUnits) * Ppu, Dark);
	}
	for (const TArray<FVector2D>& T : Tris)
	{
		RTSSoftDraw::Fill(Canvas, T, Dark);
		RTSSoftDraw::Outline(Canvas, T, OutlineUnits * Ppu, Dark);
	}
	for (const TArray<FVector2D>& Pts : Arcs)
	{
		RTSSoftDraw::Stroke(Canvas, Pts, RingUnits * Ppu, Col);
	}
	for (const TArray<FVector2D>& T : Tris)
	{
		RTSSoftDraw::Fill(Canvas, T, Col);
	}
	// no chão, o ponto fica (o tiro no chão: o X do cursor); no alvo, o próprio alvo é o meio
	if (!bTarget && Age > 0.3f)
	{
		if (bCross)
		{
			const TArray<FVector2D> L1 = { At(FVector2D(-4.5f, -4.5f)), At(FVector2D(4.5f, 4.5f)) };
			const TArray<FVector2D> L2 = { At(FVector2D(4.5f, -4.5f)), At(FVector2D(-4.5f, 4.5f)) };
			RTSSoftDraw::Stroke(Canvas, L1, (2.8f + OutlineUnits) * Ppu, Dark);
			RTSSoftDraw::Stroke(Canvas, L2, (2.8f + OutlineUnits) * Ppu, Dark);
			RTSSoftDraw::Stroke(Canvas, L1, 2.8f * Ppu, Col);
			RTSSoftDraw::Stroke(Canvas, L2, 2.8f * Ppu, Col);
		}
		else
		{
			const FVector2D P = At(FVector2D(0.0f, 0.0f));
			RTSSoftDraw::Disc(Canvas, P, (2.6f + 0.5f * OutlineUnits) * Ppu, Dark);
			RTSSoftDraw::Disc(Canvas, P, 2.6f * Ppu, Col);
		}
	}
}

float RTSOrderMarker::RadiusOf(const URTSMatchSubsystem* Match, uint32 Id)
{
	const ARTSUnitActor* View = Match ? Match->ViewOf(Id) : nullptr;
	if (!View)
	{
		return 300.0f;
	}
	FRTSUnitState U;
	const FBox2D Foot = View->GetFootprintCm();
	if (Match->GetUnit(Id, U) && U.bStructure && Foot.bIsValid)
	{
		const FVector Scale = View->GetActorScale3D();
		return 0.5f * float(FMath::Max(Foot.GetSize().X * Scale.X, Foot.GetSize().Y * Scale.Y));
	}
	FBox Box(ForceInit);
	View->ForEachComponent<UStaticMeshComponent>(false, [&Box](const UStaticMeshComponent* Part)
	{
		if (Part->IsVisible() && Part->GetStaticMesh() && Part->Bounds.BoxExtent.GetMax() < 5000.0f)
		{
			Box += Part->Bounds.GetBox();
		}
	});
	return Box.IsValid ? FMath::Clamp(0.5f * float(FMath::Max(Box.GetSize().X, Box.GetSize().Y)), 150.0f, 2500.0f) : 300.0f;
}

void RTSOrderMarker::DrawTest(UCanvas* Canvas, ARTSPlayerController* PC)
{
	static FVector2D AtM;
	static const bool bOn = [] {
		FString Arg, X, Y;
		if (!FParse::Value(FCommandLine::Get(), TEXT("-RTSMarkerAt="), Arg, false) || !Arg.Split(TEXT(","), &X, &Y))
		{
			return false;
		}
		AtM = FVector2D(FCString::Atof(*X), FCString::Atof(*Y));
		return true;
	}();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	const URTSMatchSubsystem* Match = World ? World->GetSubsystem<URTSMatchSubsystem>() : nullptr;
	const FRTSBakedMap* Map = Match ? Match->BakedMap() : nullptr;
	if (!bOn || !Map)
	{
		return;
	}
	constexpr float Loop = 1.6f;
	const float T = float(World->GetTimeSeconds());
	const auto Life = [&](int32 K) { return FMath::Fmod(T + Loop * 4.0f - 0.15f * K, Loop); };
	const auto Ground = [&](const FVector2D& Cm) { return FVector(Cm.X, Cm.Y, Map->GroundZCm(Cm)); };
	// no chão: andar, o ataque andando e o tiro no chão
	Draw(Canvas, PC, Ground(AtM * 100.0), Life(0), 0, 0.0f, false);
	Draw(Canvas, PC, Ground((AtM + FVector2D(10.0, 0.0)) * 100.0), Life(1), 1, 0.0f, false);
	Draw(Canvas, PC, Ground((AtM + FVector2D(20.0, 0.0)) * 100.0), Life(2), 1, 0.0f, true);
	// no alvo: as três unidades mais perto (atacar, consertar, entrar)
	TArray<FRTSUnitState> Units;
	Match->GetUnits(Units);
	const FVector2D Origin = AtM * 100.0;
	Units.Sort([&Origin](const FRTSUnitState& A, const FRTSUnitState& B) { return FVector2D::DistSquared(A.PosCm, Origin) < FVector2D::DistSquared(B.PosCm, Origin); });
	static const uint8 Kinds[3] = { 1, 2, 0 };
	for (int32 K = 0; K < FMath::Min(3, Units.Num()); ++K)
	{
		Draw(Canvas, PC, Ground(Units[K].PosCm), Life(3 + K), Kinds[K], RadiusOf(Match, Units[K].Id), false);
	}
}
