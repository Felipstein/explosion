#include "RTSPowerPreview.h"

#include "Camera/PlayerCameraManager.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
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

namespace
{
	// as cores da prancha (mira.html): golpe o vermelho do atacar, apoio o azul, tropas o céu, olhos a térmica; o cinza do inválido
	const FColor KindColor[4] = { FColor(255, 59, 48), FColor(89, 178, 255), FColor(127, 230, 255), FColor(243, 241, 236) };
	const FColor Off(154, 160, 166);
	const FColor Ink(10, 11, 12);
	constexpr float LiftCm = 25.0f;        // acima do chão (o relevo não come a linha)
	constexpr float LineW = 2.4f;          // a linha clara (× a escala da tela) e o contorno escuro 2,4 mais grosso
	constexpr float InkW = 2.4f;

	FSpec GSpec;
	uint64 GShownFrame = 0;
	uint32 GTargetId = 0;
	double GTargetAt = 0.0;

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

	// o pulso calmo da área (o do ZH, mais lento: ±12% em 1,6 s)
	float Breathe(double T)
	{
		return 0.88f + 0.12f * FMath::Sin(float(T) * 2.0f * PI / 1.6f);
	}

	bool ToScreen(const FView& V, const FVector2D& Cm, FVector2D& Out)
	{
		return V.PC->ProjectWorldLocationToScreen(FVector(Cm.X, Cm.Y, V.Map->GroundZCm(Cm) + LiftCm), Out);
	}

	// os pontos no chão viram a polilinha da tela (sem os que ficam atrás da câmera)
	TArray<FVector2D> Project(const FView& V, const TArray<FVector2D>& Cm)
	{
		TArray<FVector2D> Out;
		Out.Reserve(Cm.Num());
		for (const FVector2D& P : Cm)
		{
			FVector2D S;
			if (ToScreen(V, P, S))
			{
				Out.Add(S);
			}
		}
		return Out;
	}

	// o anel no chão: o centro, o raio, o achatado na direção Dir (a elipse) e o arco de A0 a A1 (rad)
	TArray<FVector2D> RingCm(const FVector2D& C, float R, float Squash, const FVector2D& Dir, float A0 = 0.0f, float A1 = 2.0f * PI, int32 N = 0)
	{
		const FVector2D U = Dir.GetSafeNormal(), Nr(-U.Y, U.X);
		N = N > 0 ? N : FMath::Clamp(int32(R / 60.0f), 48, 160);
		TArray<FVector2D> Pts;
		Pts.Reserve(N + 1);
		for (int32 K = 0; K <= N; ++K)
		{
			const float A = A0 + (A1 - A0) * float(K) / float(N);
			Pts.Add(C + U * (FMath::Cos(A) * R) + Nr * (FMath::Sin(A) * R * Squash));
		}
		return Pts;
	}

	// o traço com o contorno escuro embaixo
	void Ink2(const FView& V, const TArray<FVector2D>& Pts, float W, const FLinearColor& Col, bool bClosed = false)
	{
		if (Pts.Num() < 2)
		{
			return;
		}
		RTSSoftDraw::Stroke(V.Canvas, Pts, (W + InkW) * V.S, FLinearColor(Ink).CopyWithNewOpacity(Col.A), bClosed);
		RTSSoftDraw::Stroke(V.Canvas, Pts, W * V.S, Col, bClosed);
	}

	// o tracejado na tela (Dash e Gap em px × a escala, Phase anda o padrão)
	void Dashed(const FView& V, const TArray<FVector2D>& Pts, float W, const FLinearColor& Col, float Dash, float Gap, float Phase = 0.0f)
	{
		const float D = Dash * V.S, G = Gap * V.S, Period = D + G;
		float Walk = FMath::Fmod(Phase * V.S, Period);
		Walk = Walk < 0.0f ? Walk + Period : Walk;
		TArray<TArray<FVector2D>> Pieces;
		TArray<FVector2D> Cur;
		for (int32 K = 0; K + 1 < Pts.Num(); ++K)
		{
			const FVector2D A = Pts[K], B = Pts[K + 1];
			const float L = float(FVector2D::Distance(A, B));
			float At = 0.0f;
			while (At < L)
			{
				const bool bOn = Walk < D;
				const float Left = bOn ? D - Walk : Period - Walk;
				const float Step = FMath::Min(Left, L - At);
				const FVector2D P0 = A + (B - A) * (At / FMath::Max(L, 1e-3f)), P1 = A + (B - A) * ((At + Step) / FMath::Max(L, 1e-3f));
				if (bOn)
				{
					if (Cur.IsEmpty())
					{
						Cur.Add(P0);
					}
					Cur.Add(P1);
				}
				At += Step;
				Walk += Step;
				if (Walk >= Period - 1e-3f)
				{
					Walk = 0.0f;
				}
				if (bOn && Walk >= D - 1e-3f && !Cur.IsEmpty())
				{
					Pieces.Add(MoveTemp(Cur));
					Cur.Reset();
				}
			}
		}
		if (Cur.Num() >= 2)
		{
			Pieces.Add(MoveTemp(Cur));
		}
		for (const TArray<FVector2D>& P : Pieces)
		{
			RTSSoftDraw::Stroke(V.Canvas, P, (W + InkW) * V.S, FLinearColor(Ink).CopyWithNewOpacity(Col.A));
		}
		for (const TArray<FVector2D>& P : Pieces)
		{
			RTSSoftDraw::Stroke(V.Canvas, P, W * V.S, Col);
		}
	}

	// o véu do anel: o miolo quase limpo (5%), 9% aos 3/4 e 20% na borda
	void Veil(const FView& V, const FVector2D& C, float R, float Squash, const FVector2D& Dir, const FLinearColor& Col, float Strength)
	{
		const TArray<FVector2D> Edge = Project(V, RingCm(C, R, Squash, Dir));
		const TArray<FVector2D> Mid = Project(V, RingCm(C, R * 0.75f, Squash, Dir));
		FVector2D Cs;
		if (Edge.Num() != Mid.Num() || Edge.Num() < 3 || !ToScreen(V, C, Cs))
		{
			return;
		}
		TArray<FVector2D> Center;
		Center.Init(Cs, Mid.Num());
		RTSSoftDraw::Band(V.Canvas, Center, Mid, Col.CopyWithNewOpacity(0.05f * Strength), Col.CopyWithNewOpacity(0.09f * Strength));
		RTSSoftDraw::Band(V.Canvas, Mid, Edge, Col.CopyWithNewOpacity(0.09f * Strength), Col.CopyWithNewOpacity(0.2f * Strength));
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
		Ink2(V, { FVector2D(Mx - 11.0f * V.S, My + 11.0f * V.S), FVector2D(Mx + 11.0f * V.S, My - 11.0f * V.S) }, LineW, Col);
		Label(V, FVector2D(Mx + 18.0f * V.S, My + 16.0f * V.S), Reason, Col);
	}

	void DrawCircle(const FView& V, const FSpec& P, const FLinearColor& Col, float B)
	{
		const FVector2D Dir = P.Dir.IsNearlyZero() ? FVector2D(1.0, 0.0) : P.Dir;
		Veil(V, P.CenterCm, P.RadiusCm, P.SquashY, Dir, Col, B);
		const TArray<FVector2D> Ring = Project(V, RingCm(P.CenterCm, P.RadiusCm, P.SquashY, Dir));
		if (!P.bValid)
		{
			Dashed(V, Ring, LineW, Col.CopyWithNewOpacity(B), 5.0f, 6.0f);   // o inválido: traços curtos, a cor ainda aparece na areia
			return;
		}
		Ink2(V, Ring, LineW, Col.CopyWithNewOpacity(B));
		// o espalhamento tracejado girando devagar
		if (P.ScatterCm > 0.0f)
		{
			const float Spin = float(V.T) * 0.25f;
			Dashed(V, Project(V, RingCm(P.CenterCm, P.ScatterCm, P.SquashY, Dir, Spin, Spin + 2.0f * PI)), 1.4f, Col.CopyWithNewOpacity(0.8f), 9.0f, 8.0f);
		}
		// os 4 dentes do golpe: a mira está viva
		if (P.Kind == EKind::Strike)
		{
			const float Out = P.RadiusCm + FMath::Max(150.0f, P.RadiusCm * 0.08f);
			for (int32 K = 0; K < 4; ++K)
			{
				const float A = float(V.T) * 0.35f + K * PI * 0.5f;
				Ink2(V, Project(V, RingCm(P.CenterCm, Out, P.SquashY, Dir, A - 0.12f, A + 0.12f, 8)), 3.2f, Col);
			}
		}
	}

	void DrawStrip(const FView& V, const FSpec& P, const FLinearColor& Col, float B)
	{
		const FVector2D U = P.Dir.GetSafeNormal(), N(-U.Y, U.X);
		const float L = P.LengthCm, W = P.WidthCm * 0.5f;
		if (L <= 0.0f || W <= 0.0f || U.IsNearlyZero())
		{
			return;
		}
		const auto At = [&](float A, float C) { return P.CenterCm + U * A + N * C; };
		// as bordas da faixa, amostradas a cada ~1 m (acompanham o relevo)
		TArray<FVector2D> Box;
		const int32 Along = FMath::Clamp(int32(L / 100.0f), 8, 120), Across = FMath::Clamp(int32(W / 50.0f), 2, 40);
		for (int32 K = 0; K <= Along; ++K) { Box.Add(At(L * K / Along, -W)); }
		for (int32 K = 1; K <= Across; ++K) { Box.Add(At(L, -W + 2.0f * W * K / Across)); }
		for (int32 K = Along - 1; K >= 0; --K) { Box.Add(At(L * K / Along, W)); }
		for (int32 K = Across - 1; K >= 1; --K) { Box.Add(At(0.0f, -W + 2.0f * W * K / Across)); }
		const TArray<FVector2D> Screen = Project(V, Box);
		// o véu chapado e leve (a faixa é estreita: o degradê não lê)
		TArray<FVector2D> Axis, Side;
		for (int32 K = 0; K <= Along; ++K)
		{
			Axis.Add(At(L * K / Along, -W));
			Side.Add(At(L * K / Along, W));
		}
		const TArray<FVector2D> A1 = Project(V, Axis), A2 = Project(V, Side);
		if (A1.Num() == A2.Num() && A1.Num() >= 2)
		{
			for (int32 K = 0; K + 1 < A1.Num(); ++K)
			{
				RTSSoftDraw::Triangle(V.Canvas, A1[K], A1[K + 1], A2[K + 1], Col.CopyWithNewOpacity(0.1f * B));
				RTSSoftDraw::Triangle(V.Canvas, A1[K], A2[K + 1], A2[K], Col.CopyWithNewOpacity(0.1f * B));
			}
		}
		if (!P.bValid)
		{
			Dashed(V, Screen, LineW, Col.CopyWithNewOpacity(B), 5.0f, 6.0f);   // o inválido: traços curtos, a cor ainda aparece na areia
			return;
		}
		Ink2(V, Screen, LineW, Col.CopyWithNewOpacity(B), true);
		// as fileiras lado a lado (o Naval II) e a fileira de impactos no eixo de cada uma
		const int32 Rows = FMath::Max(1, P.Rows);
		for (int32 R = 1; R < Rows; ++R)
		{
			const float C = -W + 2.0f * W * R / Rows;
			Dashed(V, Project(V, { At(0.0f, C), At(L * 0.5f, C), At(L, C) }), 1.2f, Col.CopyWithNewOpacity(0.7f), 6.0f, 6.0f);
		}
		for (int32 R = 0; R < Rows; ++R)
		{
			const float C = -W + 2.0f * W * (R + 0.5f) / Rows;
			for (int32 K = 1; K < 12; ++K)
			{
				FVector2D S;
				if (ToScreen(V, At(L * K / 12.0f, C), S))
				{
					RTSSoftDraw::Disc(V.Canvas, S, 2.0f * V.S, FLinearColor(Ink));
					RTSSoftDraw::Disc(V.Canvas, S, 1.2f * V.S, Col);
				}
			}
		}
		// as setas correndo no sentido do voo (de onde o avião vem). Desenhadas na tela, simétricas na direção da faixa: no chão, com a
		// faixa na diagonal da tela, a perspectiva entortava a seta num "visto" (o vídeo mira_poderes_v1, 05/11)
		for (int32 K = 0; K < 4; ++K)
		{
			const float A = FMath::Fmod(float(V.T) * 900.0f + K * L / 4.0f, L);
			const float Fade = FMath::Clamp(FMath::Min(A, L - A) / 600.0f, 0.0f, 1.0f);
			FVector2D Tip, Back, E0, E1;
			if (!ToScreen(V, At(A, 0.0f), Tip) || !ToScreen(V, At(A - 100.0f, 0.0f), Back) || !ToScreen(V, At(A, -W), E0) || !ToScreen(V, At(A, W), E1))
			{
				continue;
			}
			const FVector2D D = (Tip - Back).GetSafeNormal(), Wing(-D.Y, D.X);
			const float H = FMath::Clamp(float(FVector2D::Distance(E0, E1)) * 0.22f, 6.0f * V.S, 22.0f * V.S);
			Ink2(V, { Tip - D * H + Wing * H, Tip, Tip - D * H - Wing * H }, 2.0f, Col.CopyWithNewOpacity(Fade));
		}
	}

	void DrawPath(const FView& V, const FSpec& P, const FLinearColor& Col)
	{
		if (P.PathCm.Num() < 2)
		{
			return;
		}
		// o que cabe no comprimento do poder na cor do tipo; o que passa, em cinza
		TArray<FVector2D> Fits, Over;
		float Used = 0.0f;
		Fits.Add(P.PathCm[0]);
		for (int32 K = 1; K < P.PathCm.Num(); ++K)
		{
			const float L = float(FVector2D::Distance(P.PathCm[K - 1], P.PathCm[K]));
			if (P.PathMaxCm > 0.0f && Used + L > P.PathMaxCm)
			{
				if (Over.IsEmpty())
				{
					const FVector2D Cut = P.PathCm[K - 1] + (P.PathCm[K] - P.PathCm[K - 1]) * ((P.PathMaxCm - Used) / FMath::Max(L, 1.0f));
					Fits.Add(Cut);
					Over.Add(Cut);
				}
				Over.Add(P.PathCm[K]);
			}
			else
			{
				Fits.Add(P.PathCm[K]);
			}
			Used += L;
		}
		// amostrado a cada ~1 m pra deitar no relevo
		const auto Dense = [](const TArray<FVector2D>& In)
		{
			TArray<FVector2D> Out;
			for (int32 K = 0; K + 1 < In.Num(); ++K)
			{
				const int32 N = FMath::Max(1, int32(FVector2D::Distance(In[K], In[K + 1]) / 100.0f));
				for (int32 J = 0; J < N; ++J) { Out.Add(In[K] + (In[K + 1] - In[K]) * (float(J) / N)); }
			}
			if (!In.IsEmpty()) { Out.Add(In.Last()); }
			return Out;
		};
		const FLinearColor Good = P.bValid ? Col : FLinearColor(Off);
		Ink2(V, Project(V, Dense(Fits)), 3.0f, Good);
		if (Over.Num() >= 2)
		{
			Dashed(V, Project(V, Dense(Over)), 2.0f, FLinearColor(Off), 4.0f, 6.0f);
		}
		// os postes a cada 4 m
		float Walk = 0.0f;
		for (int32 K = 1; K < Fits.Num(); ++K)
		{
			const float L = float(FVector2D::Distance(Fits[K - 1], Fits[K]));
			for (float D = FMath::Fmod(400.0f - FMath::Fmod(Walk, 400.0f), 400.0f); D <= L; D += 400.0f)
			{
				FVector2D S;
				if (ToScreen(V, Fits[K - 1] + (Fits[K] - Fits[K - 1]) * (D / FMath::Max(L, 1.0f)), S))
				{
					const float H = 3.0f * V.S;
					RTSSoftDraw::Fill(V.Canvas, { S + FVector2D(-H, -H), S + FVector2D(H, -H), S + FVector2D(H, H), S + FVector2D(-H, H) }, FLinearColor(Ink));
					const float I = 2.0f * V.S;
					RTSSoftDraw::Fill(V.Canvas, { S + FVector2D(-I, -I), S + FVector2D(I, -I), S + FVector2D(I, I), S + FVector2D(-I, I) }, Good);
				}
			}
			Walk += L;
		}
		// a régua: o que já gastou do comprimento do poder
		FVector2D End;
		if (P.PathMaxCm > 0.0f && ToScreen(V, P.PathCm.Last(), End))
		{
			Label(V, End + FVector2D(16.0f, 18.0f) * V.S, FString::Printf(TEXT("%d / %d m"), FMath::RoundToInt(FMath::Min(Used, P.PathMaxCm) / 100.0f), FMath::RoundToInt(P.PathMaxCm / 100.0f)), Good);
		}
	}

	void DrawDrop(const FView& V, const FSpec& P, const FLinearColor& Col, float B)
	{
		Veil(V, P.CenterCm, P.RadiusCm, 1.0f, FVector2D(1.0, 0.0), Col, B);
		Dashed(V, Project(V, RingCm(P.CenterCm, P.RadiusCm, 1.0f, FVector2D(1.0, 0.0))), 2.2f, Col.CopyWithNewOpacity(B), P.bValid ? 10.0f : 5.0f, 6.0f);
		if (!P.bValid)
		{
			return;
		}
		for (const FVector2D& At : P.DropCm)
		{
			FVector2D S;
			if (ToScreen(V, At, S))
			{
				RTSSoftDraw::Disc(V.Canvas, S, 3.4f * V.S, FLinearColor(Ink));
				RTSSoftDraw::Disc(V.Canvas, S, 2.3f * V.S, Col);
			}
		}
	}

	void DrawTarget(const FView& V, const FSpec& P, const FLinearColor& Col)
	{
		if (P.TargetId == 0 || !P.bValid)
		{
			GTargetId = 0;
			return;
		}
		TArray<FRTSUnitState> Units;
		V.Match->GetUnits(Units);
		const FRTSUnitState* U = Units.FindByPredicate([&P](const FRTSUnitState& X) { return X.Id == P.TargetId; });
		FVector2D C, Side;
		const float R = RTSOrderMarker::RadiusOf(V.Match, P.TargetId);
		const FVector Right = V.PC->PlayerCameraManager ? V.PC->PlayerCameraManager->GetCameraRotation().RotateVector(FVector::RightVector) : FVector::RightVector;
		if (!U || !ToScreen(V, U->PosCm, C) || !V.PC->ProjectWorldLocationToScreen(FVector(U->PosCm.X, U->PosCm.Y, V.Map->GroundZCm(U->PosCm) + LiftCm) + Right * R, Side))
		{
			return;
		}
		// os cantos do C&C (os do selecionar) fecham no alvo num estalo de 0,25 s quando ele muda
		if (GTargetId != P.TargetId)
		{
			GTargetId = P.TargetId;
			GTargetAt = V.T;
		}
		const float K = Ease(float(V.T - GTargetAt) / 0.25f), Scale = 1.4f - 0.4f * K;
		const float Hw = float(FVector2D::Distance(C, Side)) * 1.1f * Scale, Hh = Hw * 0.72f, Arm = FMath::Min(Hw, Hh) * 0.45f;
		const FVector2D Mid = C - FVector2D(0.0f, Hh * 0.35f);
		for (const FVector2D& Sg : { FVector2D(-1, -1), FVector2D(1, -1), FVector2D(1, 1), FVector2D(-1, 1) })
		{
			const FVector2D Corner = Mid + FVector2D(Sg.X * Hw, Sg.Y * Hh);
			Ink2(V, { Corner - FVector2D(0.0f, Sg.Y * Arm), Corner, Corner - FVector2D(Sg.X * Arm, 0.0f) }, 2.6f, Col);
		}
	}

	void DrawOrbit(const FView& V, const FSpec& P, const FLinearColor& Col, float B)
	{
		Veil(V, P.CenterCm, P.RadiusCm, 1.0f, FVector2D(1.0, 0.0), Col, B * 0.35f);
		Dashed(V, Project(V, RingCm(P.CenterCm, P.RadiusCm, 1.0f, FVector2D(1.0, 0.0))), 2.2f, Col.CopyWithNewOpacity(B), 14.0f, 7.0f, float(V.T) * 12.0f);
	}

	// quem o poder pega: o anel fino no chão de cada um (do tamanho dele), respirando junto com a área
	void DrawAffected(const FView& V, const FSpec& P, const FLinearColor& Col, float B)
	{
		if (P.AffectedIds.IsEmpty() || !P.bValid)
		{
			return;
		}
		TArray<FRTSUnitState> Units;
		V.Match->GetUnits(Units);
		for (const FRTSUnitState& U : Units)
		{
			if (P.AffectedIds.Contains(U.Id))
			{
				const float R = FMath::Max(250.0f, RTSOrderMarker::RadiusOf(V.Match, U.Id) * 1.1f);
				Ink2(V, Project(V, RingCm(U.PosCm, R, 1.0f, FVector2D(1.0, 0.0), 0.0f, 2.0f * PI, 48)), 1.8f, Col.CopyWithNewOpacity(0.75f + 0.25f * B));
			}
		}
	}

	void DrawSpec(const FView& V, const FSpec& P)
	{
		const FLinearColor Col = P.bValid ? FLinearColor(KindColor[FMath::Min<uint8>(uint8(P.Kind), 3)]) : FLinearColor(Off);
		const float B = Breathe(V.T);
		DrawAffected(V, P, Col, B);
		switch (P.Shape)
		{
		case EShape::Circle: DrawCircle(V, P, Col, B); break;
		case EShape::Strip: DrawStrip(V, P, Col, B); break;
		case EShape::Path: DrawPath(V, P, Col); break;
		case EShape::Drop: DrawDrop(V, P, Col, B); break;
		case EShape::Target: DrawTarget(V, P, Col); break;
		case EShape::Orbit: DrawOrbit(V, P, Col, B); break;
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
	// sem o Show deste quadro (ou do anterior), a mira já saiu
	FView V;
	if (!IsShown() || !MakeView(Canvas, PC, V))
	{
		return;
	}
	DrawSpec(V, GSpec);
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
	// uma forma a cada 2,5 s, em 3 voltas: a 1ª válida, a 2ª válida com o círculo de apoio, a 3ª inválida
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
	DrawSpec(V, P);
}
