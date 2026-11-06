#include "RTSAlerts.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "GlobalRenderResources.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RTSMapBaker.h"
#include "RTSMatchSubsystem.h"
#include "RTSPlayerController.h"
#include "RTSSoftDraw.h"

namespace
{
	// as cores dos cursores e da prancha aprovada (sRGB)
	const FColor Ink(10, 11, 12);
	const FColor Red(255, 59, 48), RedLight(255, 154, 147), Amber(255, 178, 31), White(243, 241, 236), Gold(242, 193, 78), Ash(154, 161, 168);
	// a prancha tem o losango de ~226 px de diagonal (o lado do mapa ~160 px): as medidas dela valem M = lado / 160
	constexpr float BoardSide = 160.0f;

	float Clamp01(float X) { return FMath::Clamp(X, 0.0f, 1.0f); }
	float Ease(float X) { X = Clamp01(X); return 1.0f - FMath::Pow(1.0f - X, 3.0f); }
	float Ramp(float X, float A, float B) { return Clamp01((X - A) / (B - A)); }
	float Blink(float T, float Hz = 2.2f) { return 0.55f + 0.45f * FMath::Cos(T * Hz * 2.0f * PI); }
	FLinearColor Col(const FColor& C, float A) { return FLinearColor(C).CopyWithNewOpacity(Clamp01(A)); }

	TArray<FVector2D> Circle(const FVector2D& P, float R, int32 N = 40)
	{
		TArray<FVector2D> Pts;
		for (int32 K = 0; K < N; ++K)
		{
			const float A = 2.0f * PI * K / N;
			Pts.Add(P + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R);
		}
		return Pts;
	}

	/** O traço com o contorno escuro embaixo (o "ink" da prancha). */
	void Ink2(UCanvas* C, const TArray<FVector2D>& Pts, float W, const FColor& Color, float A, bool bClosed, float Dark = 2.2f)
	{
		RTSSoftDraw::Stroke(C, Pts, W + Dark, Col(Ink, A), bClosed);
		RTSSoftDraw::Stroke(C, Pts, W, Col(Color, A), bClosed);
	}

	void Ring(UCanvas* C, const FVector2D& P, float R, const FColor& Color, float W, float A)
	{
		if (R > 0.2f && A > 0.01f)
		{
			Ink2(C, Circle(P, R), W, Color, A, true);
		}
	}

	void Dot(UCanvas* C, const FVector2D& P, float R, const FColor& Color, float A)
	{
		RTSSoftDraw::Disc(C, P, R + 1.1f, Col(Ink, A));
		RTSSoftDraw::Disc(C, P, R, Col(Color, A));
	}

	void Poly(UCanvas* C, const TArray<FVector2D>& Pts, const FColor& Color, float A)
	{
		RTSSoftDraw::Fill(C, Pts, Col(Ink, A));
		RTSSoftDraw::Outline(C, Pts, 1.6f, Col(Ink, A));
		RTSSoftDraw::Fill(C, Pts, Col(Color, A));
	}

	/** A onda de sonar: N anéis que saem do ponto, um a cada meio período. */
	void Sonar(UCanvas* C, const FVector2D& P, float T, const FColor& Color, float R0, float R1, float Period, int32 N, float M)
	{
		for (int32 K = 0; K < N; ++K)
		{
			const float U = (T - K * Period * 0.5f) / Period;
			if (U > 0.0f && U < 1.0f)
			{
				Ring(C, P, (R0 + (R1 - R0) * Ease(U)) * M, Color, 1.6f * FMath::Max(1.0f, M), 1.0f - U);
			}
		}
	}

	/** A mira do atacar: o anel cortado em 4 e os triângulos que fecham. */
	void Aim(UCanvas* C, const FVector2D& P, float R, const FColor& Color, float A)
	{
		const float W = FMath::Max(1.6f, R * 0.22f), Gap = 0.5f;
		for (int32 Q = 0; Q < 4; ++Q)
		{
			const float A0 = Q * PI / 2.0f + Gap / 2.0f + PI / 4.0f, A1 = A0 + PI / 2.0f - Gap;
			TArray<FVector2D> Arc;
			for (int32 J = 0; J <= 10; ++J)
			{
				const float Ang = FMath::Lerp(A0, A1, J / 10.0f);
				Arc.Add(P + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * R);
			}
			Ink2(C, Arc, W, Color, A, false);
		}
		for (int32 Q = 0; Q < 4; ++Q)
		{
			const float Ang = Q * PI / 2.0f + PI / 2.0f;
			const FVector2D U(FMath::Cos(Ang), FMath::Sin(Ang)), S(-U.Y, U.X);
			const float K = FMath::Max(3.0f, R * 0.42f);
			Poly(C, { P + U * (R - K * 0.6f), P + U * (R + K * 0.5f) + S * K * 0.55f, P + U * (R + K * 0.5f) - S * K * 0.55f }, Color, A);
		}
	}

	/** Os cantos do prédio (o selecionar), meia largura H. */
	void Corners(UCanvas* C, const FVector2D& P, float H, const FColor& Color, float W, float A)
	{
		const float L = H * 0.45f;
		for (const FVector2D Sg : { FVector2D(-1, -1), FVector2D(1, -1), FVector2D(1, 1), FVector2D(-1, 1) })
		{
			Ink2(C, { P + FVector2D(Sg.X * H, Sg.Y * (H - L)), P + FVector2D(Sg.X * H, Sg.Y * H), P + FVector2D(Sg.X * (H - L), Sg.Y * H) }, W, Color, A, false);
		}
	}

	void Cross(UCanvas* C, const FVector2D& P, float X, const FColor& Color, float W, float A)
	{
		Ink2(C, { P + FVector2D(-X, -X), P + FVector2D(X, X) }, W, Color, A, false);
		Ink2(C, { P + FVector2D(X, -X), P + FVector2D(-X, X) }, W, Color, A, false);
	}

	void Eye(UCanvas* C, const FVector2D& P, float Open, float M, const FColor& Color, bool bFilled, float A)
	{
		const float Hw = 10.0f * M, H = 5.5f * Open * M * 1.6f;
		TArray<FVector2D> Pts;
		for (int32 J = 0; J <= 16; ++J)
		{
			const float U = J / 16.0f;
			Pts.Add(P + FVector2D(FMath::Lerp(-Hw, Hw, U), -H * 4.0f * U * (1.0f - U)));
		}
		for (int32 J = 15; J >= 1; --J)
		{
			const float U = J / 16.0f;
			Pts.Add(P + FVector2D(FMath::Lerp(-Hw, Hw, U), H * 4.0f * U * (1.0f - U)));
		}
		if (bFilled)
		{
			RTSSoftDraw::Fill(C, Pts, Col(Color, A));
			RTSSoftDraw::Outline(C, Pts, 1.6f, Col(Ink, A));
			Dot(C, P, 2.2f * M, FColor(34, 38, 43), A);
		}
		else
		{
			Ink2(C, Pts, 1.6f * FMath::Max(1.0f, M), Color, A, true);
			Dot(C, P, 2.4f * M * Open + 0.1f, Color, A);
		}
	}

	/** O gradiente de cor por vértice (o brilho da borda). */
	void Gradient(UCanvas* C, const FVector2D& A, const FVector2D& B, const FVector2D& Cc, const FVector2D& D, const FLinearColor& Ca, const FLinearColor& Cb)
	{
		// A-B o lado forte, C-D o lado que some
		TArray<FCanvasUVTri> Tris;
		FCanvasUVTri T;
		T.V0_Pos = A; T.V0_Color = Ca; T.V1_Pos = B; T.V1_Color = Ca; T.V2_Pos = Cc; T.V2_Color = Cb;
		Tris.Add(T);
		T.V0_Pos = Cc; T.V0_Color = Cb; T.V1_Pos = B; T.V1_Color = Ca; T.V2_Pos = D; T.V2_Color = Cb;
		Tris.Add(T);
		FCanvasTriangleItem Item(Tris, GWhiteTexture);
		Item.BlendMode = SE_BLEND_Translucent;
		C->DrawItem(Item);
	}

	float ExitT(const FVector2D& A, const FVector2D& D, const FVector2D& Lo, const FVector2D& Hi)
	{
		float T = TNumericLimits<float>::Max();
		if (D.X > UE_KINDA_SMALL_NUMBER) { T = FMath::Min(T, float((Hi.X - A.X) / D.X)); }
		if (D.X < -UE_KINDA_SMALL_NUMBER) { T = FMath::Min(T, float((Lo.X - A.X) / D.X)); }
		if (D.Y > UE_KINDA_SMALL_NUMBER) { T = FMath::Min(T, float((Hi.Y - A.Y) / D.Y)); }
		if (D.Y < -UE_KINDA_SMALL_NUMBER) { T = FMath::Min(T, float((Lo.Y - A.Y) / D.Y)); }
		return T;
	}

	// ——— o que o controle não sabe sozinho (um por mundo)
	struct FWatchState
	{
		TWeakObjectPtr<URTSMatchSubsystem> Match;
		TMap<uint32, FVector2D> Own;
		TSet<uint32> Selling;
		TMap<int32, int32> DockBoxes;
		TSet<uint32> SeenEnemyStructures;
		TSet<uint16> SeenEnemyDefs;
		TArray<bool> PowerReady;
		double LastAirliftAt = -1.0;
		int32 LastAirliftCount = 0;
	};
	FWatchState& Watching() { static FWatchState S; return S; }
	TMap<FIntPoint, float>& Heat() { static TMap<FIntPoint, float> H; return H; }
	constexpr float HeatCellCm = 2000.0f;
	/** Só a superarma (a espera de 3 min ou mais) avisa quando fica pronta: o poder do general não enche o minimapa. */
	constexpr float SuperweaponCooldownS = 180.0f;
}

float RTSAlerts::LifeOf(uint8 Kind)
{
	switch (Kind)
	{
	case BaseAttacked: return 6.2f;
	case StrikeIncoming: return 8.0f;
	case PowerReady: return 6.0f;
	case DockEmpty: return 3.4f;
	case Airlift: return 3.6f;
	case FirstContact: return 3.4f;
	case UnitLost: return 4.0f;
	default: return 4.2f;
	}
}

void RTSAlerts::Watch(URTSMatchSubsystem* Match, TArray<FRTSMinimapPing>& Out)
{
	if (!Match)
	{
		return;
	}
	FWatchState& W = Watching();
	const bool bPrimed = W.Match.Get() == Match;
	if (!bPrimed)
	{
		W = FWatchState();
		W.Match = Match;
		Heat().Reset();
	}
	const auto Emit = [&](uint8 Kind, const FVector2D& Cm, float RadiusCm = 0.0f)
	{
		if (bPrimed)
		{
			FRTSMinimapPing P;
			P.Cm = Cm;
			P.Kind = Kind;
			P.RadiusCm = RadiusCm;
			P.Life = LifeOf(Kind);
			Out.Add(P);
		}
	};
	const uint8 Me = Match->LocalPlayer();
	TArray<FRTSUnitState> Units;
	Match->GetUnits(Units);
	TMap<uint32, FVector2D> Own;
	TSet<uint32> Selling;
	FVector2D Hq = FVector2D::ZeroVector;
	bool bHq = false;
	for (const FRTSUnitState& U : Units)
	{
		if (U.Owner == Me)
		{
			Own.Add(U.Id, U.PosCm);
			if (U.bSelling)
			{
				Selling.Add(U.Id);
			}
			FRTSDefInfo Info;
			if (U.bStructure && (!bHq || (Match->GetDef(U.Def, Info) && Info.Key.Contains(TEXT("command_center")))))
			{
				Hq = U.PosCm;
				bHq = true;
			}
		}
		else if (Match->IsEnemyOfLocal(U.Owner) && Match->IsUnitShown(U.Id))
		{
			// o primeiro contato: o prédio inimigo visto pela primeira vez, ou um tipo que nunca apareceu
			bool bNew = false;
			if (U.bStructure && !W.SeenEnemyStructures.Contains(U.Id))
			{
				W.SeenEnemyStructures.Add(U.Id);
				bNew = true;
			}
			if (!W.SeenEnemyDefs.Contains(U.Def))
			{
				W.SeenEnemyDefs.Add(U.Def);
				bNew = true;
			}
			if (bNew)
			{
				Emit(FirstContact, U.PosCm);
			}
		}
	}
	// a unidade perdida: a sua que sumiu (a vendida não conta)
	for (const TPair<uint32, FVector2D>& Was : W.Own)
	{
		if (!Own.Contains(Was.Key) && !W.Selling.Contains(Was.Key))
		{
			Emit(UnitLost, Was.Value);
		}
	}
	W.Own = MoveTemp(Own);
	W.Selling = MoveTemp(Selling);
	// a doca que acabou (a pilha que caiu não conta)
	for (int32 K = 0; K < Match->DockCount(); ++K)
	{
		FRTSDockState D;
		if (!Match->GetDock(K, D) || D.bPile)
		{
			continue;
		}
		const int32* Before = W.DockBoxes.Find(K);
		if (Before && *Before > 0 && D.Boxes <= 0)
		{
			Emit(DockEmpty, D.PosCm);
		}
		W.DockBoxes.Add(K, D.Boxes);
	}
	// a sua superarma pronta, no quartel-general
	W.PowerReady.SetNum(Match->PowerCount());
	for (int32 K = 0; K < Match->PowerCount(); ++K)
	{
		URTSMatchSubsystem::FPowerInfo P;
		const bool bReady = Match->GetPowerInfo(K, P) && P.bUnlocked && P.bGranter && P.bReady && P.CooldownS >= SuperweaponCooldownS;
		if (bReady && !W.PowerReady[K] && bHq)
		{
			Emit(PowerReady, Hq);
		}
		W.PowerReady[K] = bReady;
	}
	// o teste: um de cada a cada 8 s em volta do quartel-general (o ataque longe, fora da tela: a seta da borda)
	static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("RTSAlertTest")) || FCommandLine::Get() && FCString::Strifind(FCommandLine::Get(), TEXT("-RTSAlertTest="));
	static double NextTest = 2.0;
	const double Now = Match->GetWorld() ? Match->GetWorld()->GetTimeSeconds() : 0.0;
	if (bTest && bHq && Now >= NextTest)
	{
		NextTest = Now + 8.0;
		const auto At = [&](float Xm, float Ym) { return Hq + FVector2D(Xm, Ym) * 100.0; };
		// pro meio do mapa (a base de teste fica num canto): cada um no seu lugar, sem amontoar
		Emit(UnitAttacked, At(250.0f, 200.0f));
		Emit(BaseAttacked, Hq);
		Emit(StrikeIncoming, At(140.0f, 120.0f), 3500.0f);
		Emit(PowerReady, At(-30.0f, 40.0f));
		Emit(DockEmpty, At(60.0f, 280.0f));
		Emit(Airlift, At(320.0f, 60.0f));
		Emit(FirstContact, At(400.0f, 380.0f));
		Emit(UnitLost, At(90.0f, 30.0f));
		for (int32 K = 0; K < 6; ++K)
		{
			FeedHeat(At(245.0f + 8.0f * K, 195.0f + 5.0f * K));
			FeedHeat(At(85.0f + 6.0f * K, 30.0f));
		}
	}
	// (04/11, ele: "remover o alerta lixo completamente do jogo sobre o air supply drop, pra que?") a carga a caminho da zona de lançamento
	// não pisca mais no minimapa
}

void RTSAlerts::FeedHeat(const FVector2D& Cm)
{
	float& H = Heat().FindOrAdd(FIntPoint(FMath::FloorToInt32(Cm.X / HeatCellCm), FMath::FloorToInt32(Cm.Y / HeatCellCm)));
	H = FMath::Min(1.5f, H + 0.35f);
}

void RTSAlerts::DrawHeat(UCanvas* Canvas, const FView& View, float DeltaS)
{
	const float M = View.Side / BoardSide;
	for (auto It = Heat().CreateIterator(); It; ++It)
	{
		It->Value -= DeltaS * 1.5f / 20.0f;   // esfria em 20 s
		if (It->Value <= 0.0f)
		{
			It.RemoveCurrent();
			continue;
		}
		const float H = FMath::Min(1.0f, It->Value);
		const FVector2D P = View.ToScreen(FVector2D((It->Key.X + 0.5f) * HeatCellCm, (It->Key.Y + 0.5f) * HeatCellCm));
		const float R = 14.0f * M * (0.6f + 0.4f * H);
		RTSSoftDraw::Disc(Canvas, P, R, Col(FColor(255, 59, 48), 0.10f * H));
		RTSSoftDraw::Disc(Canvas, P, R * 0.6f, Col(FColor(255, 74, 42), 0.12f * H));
		RTSSoftDraw::Disc(Canvas, P, R * 0.3f, Col(FColor(255, 90, 50), 0.14f * H));
	}
}

void RTSAlerts::DrawPing(UCanvas* Canvas, const FView& View, const FRTSMinimapPing& Ping)
{
	const float M = View.Side / BoardSide, L = Ping.Age;
	const FVector2D P = View.ToScreen(Ping.Cm);
	switch (Ping.Kind)
	{
	case BaseAttacked:
	{
		// os cantos do prédio fecham, três ondas largas, o quadrado pisca até os 6 s
		const float K = Ease(L / 0.6f), Op = L > 5.6f ? 1.0f - (L - 5.6f) / 0.6f : 1.0f;
		Sonar(Canvas, P, L - 0.5f, Red, 8.0f, 50.0f, 1.3f, 3, M);
		if (L < 1.0f)
		{
			Corners(Canvas, P, (30.0f - 21.0f * K) * M, Red, 2.4f * FMath::Max(1.0f, M), Op);
		}
		else
		{
			const float A = Op * Blink(L, 1.6f), H = 5.5f * M;
			Poly(Canvas, { P + FVector2D(-H, -H), P + FVector2D(H, -H), P + FVector2D(H, H), P + FVector2D(-H, H) }, Red, A);
		}
		break;
	}
	case StrikeIncoming:
	{
		// o círculo do estrago no alvo, o relógio que esvazia e o ponteiro varrendo
		const float R = FMath::Max(10.0f * M, Ping.RadiusCm * View.PxPerCm), Cd = 1.0f - Clamp01(L / Ping.Life);
		const float Op = L > Ping.Life - 0.6f ? (Ping.Life - L) / 0.6f : 1.0f;
		RTSSoftDraw::Disc(Canvas, P, R, Col(Red, (0.10f + 0.08f * Blink(L, 1.4f)) * Op));
		Ring(Canvas, P, R, Red, 1.8f, Op);
		for (int32 I = 0; I < 12; ++I)
		{
			const float A = -PI / 2.0f + I * PI / 6.0f;
			const FVector2D U(FMath::Cos(A), FMath::Sin(A));
			RTSSoftDraw::Stroke(Canvas, { P + U * (R - 5.0f * M), P + U * (R - 1.5f * M) }, 1.6f, I / 12.0f < Cd ? Col(RedLight, Op) : FLinearColor(0, 0, 0, 0.4f * Op));
		}
		const float A = -PI / 2.0f + 2.0f * PI * (1.0f - Cd);
		RTSSoftDraw::Stroke(Canvas, { P, P + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (R - 2.0f * M) }, 1.8f, Col(RedLight, Op));
		Dot(Canvas, P, 2.6f * M, Red, Op);
		break;
	}
	case PowerReady:
	{
		// a estrela dourada pulsa no quartel-general
		const float Op = L > Ping.Life - 0.8f ? (Ping.Life - L) / 0.8f : 1.0f, K = 0.5f + 0.5f * FMath::Sin(L * 2.2f);
		Sonar(Canvas, P, FMath::Fmod(L, 2.6f), Gold, 6.0f, 22.0f, 2.0f, 1, M);
		TArray<FVector2D> Star;
		for (int32 I = 0; I < 10; ++I)
		{
			const float A = -PI / 2.0f + I * PI / 5.0f, R = (I % 2 ? 3.4f : 7.5f + 1.2f * K) * M;
			Star.Add(P + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R);
		}
		// a estrela é côncava: o leque sai do meio (cada ponta vê o centro)
		const FVector2D First = Star[0];   // (a cópia antes: a TArray não aceita somar um elemento dela mesma)
		Star.Insert(P, 0);
		Star.Add(First);
		RTSSoftDraw::Fill(Canvas, Star, Col(Ink, Op));
		TArray<FVector2D> Rim(Star);
		Rim.RemoveAt(0);
		Rim.Pop();
		RTSSoftDraw::Outline(Canvas, Rim, 1.6f, Col(Ink, Op));
		RTSSoftDraw::Fill(Canvas, Star, Col(Gold, Op));
		break;
	}
	case DockEmpty:
	{
		// o quadradinho âmbar da doca apaga com o X
		const float Op = 1.0f - Ramp(L, 2.6f, 3.2f), X = Ease(L / 0.35f) * 6.0f * M, H = 5.0f * M;
		Sonar(Canvas, P, L, Amber, 4.0f, 18.0f, 1.2f, 1, M);
		RTSSoftDraw::Stroke(Canvas, { P + FVector2D(-H, -H), P + FVector2D(H, -H), P + FVector2D(H, H), P + FVector2D(-H, H) }, 3.0f, Col(Ink, Op), true);
		RTSSoftDraw::Stroke(Canvas, { P + FVector2D(-H, -H), P + FVector2D(H, -H), P + FVector2D(H, H), P + FVector2D(-H, H) }, 1.4f, Col(Amber, Op), true);
		Cross(Canvas, P, X, Amber, 1.8f, Op);
		break;
	}
	case Airlift:
	{
		// a carga que cai: o arco tracejado da borda (do lado de fora, atrás da zona) até a zona e a seta descendo
		const FVector2D Out = (P - View.Center).GetSafeNormal();
		const FVector2D A = View.Center + (Out.IsNearlyZero() ? FVector2D(1.0, 0.0) : Out) * View.Side * 0.68f, B = P;
		const FVector2D Cc = (A + B) * 0.5f + FVector2D(10.0f * M, -FMath::Max(10.0f, float(FVector2D::Distance(A, B)) * 0.3f));
		const auto Q = [&](float U) { return A * (1 - U) * (1 - U) + Cc * (2 * (1 - U) * U) + B * (U * U); };
		const float Op = 1.0f - Ramp(L, 3.0f, 3.5f), K = Ease(L / 1.6f);
		for (int32 J = 0; J < 14; J += 2)
		{
			Ink2(Canvas, { Q(J / 14.0f), Q((J + 1) / 14.0f) }, 1.4f, Amber, Op * 0.9f, false, 1.8f);
		}
		const FVector2D H = Q(K);
		Poly(Canvas, { H + FVector2D(0.0f, 2.0f * M), H + FVector2D(-5.0f * M, -7.0f * M), H + FVector2D(5.0f * M, -7.0f * M) }, Amber, Op);
		if (L > 1.6f)
		{
			Sonar(Canvas, B, L - 1.6f, Amber, 3.0f, 18.0f, 1.0f, 1, M);
		}
		break;
	}
	case FirstContact:
	{
		// o olho branco onde o inimigo apareceu pela primeira vez
		const float O = Ease(L / 0.4f) * (1.0f - Ramp(L, 2.6f, 3.2f));
		Sonar(Canvas, P, L, White, 4.0f, 26.0f, 1.4f, 2, M);
		if (O > 0.02f)
		{
			Eye(Canvas, P, O, M, White, true, O);
		}
		break;
	}
	case UnitLost:
	{
		// o X cinza onde ela caiu: não pede ação, não pisca
		Cross(Canvas, P, 4.5f * M, Ash, 1.8f, 1.0f - Ramp(L, 1.5f, 3.6f));
		break;
	}
	default:
	{
		// a unidade sob ataque: a mira fecha no ponto, duas ondas, o ponto pisca até os 4 s
		const float K = Ease(L / 0.6f), Op = L > 3.6f ? 1.0f - (L - 3.6f) / 0.6f : 1.0f;
		Sonar(Canvas, P, L - 0.5f, Red, 6.0f, 34.0f, 1.1f, 2, M);
		if (L < 0.9f)
		{
			Aim(Canvas, P, (34.0f - 26.0f * K) * M, Red, Op);
		}
		else
		{
			Dot(Canvas, P, 3.2f * M, Red, Op * Blink(L));
		}
		break;
	}
	}
}

void RTSAlerts::DrawRadarOffline(UCanvas* Canvas, const FView& View, double Now, TFunctionRef<void(const FString&, const FVector2D&)> Label)
{
	// o mapa coberto e o chuvisco: as linhas horizontais cortadas no losango
	const FVector2D* C = View.Corners;
	TArray<FCanvasUVTri> Tris;
	FCanvasUVTri T;
	const FLinearColor Veil = FLinearColor(FColor(10, 14, 18)).CopyWithNewOpacity(0.62f);
	T.V0_Pos = C[0]; T.V1_Pos = C[1]; T.V2_Pos = C[2]; T.V0_Color = T.V1_Color = T.V2_Color = Veil;
	Tris.Add(T);
	T.V0_Pos = C[0]; T.V1_Pos = C[2]; T.V2_Pos = C[3];
	Tris.Add(T);
	FCanvasTriangleItem Item(Tris, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
	float Top = C[0].Y, Bottom = C[0].Y;
	for (int32 K = 1; K < 4; ++K)
	{
		Top = FMath::Min(Top, float(C[K].Y));
		Bottom = FMath::Max(Bottom, float(C[K].Y));
	}
	const int32 Frame = FMath::FloorToInt32(float(Now) * 24.0f);
	for (int32 I = 0; I < 46; ++I)
	{
		const float Y = Top + float((I * 37 + Frame * 13) % FMath::Max(1, int32(Bottom - Top)));
		float X0 = 1e9f, X1 = -1e9f;
		for (int32 K = 0; K < 4; ++K)
		{
			const FVector2D A = C[K], B = C[(K + 1) % 4];
			if ((A.Y - Y) * (B.Y - Y) <= 0.0 && FMath::Abs(B.Y - A.Y) > 1e-3)
			{
				const float X = float(A.X + (B.X - A.X) * (Y - A.Y) / (B.Y - A.Y));
				X0 = FMath::Min(X0, X);
				X1 = FMath::Max(X1, X);
			}
		}
		if (X1 > X0)
		{
			const float Alpha = 0.05f + 0.08f * float((I * 7) % 5) / 5.0f;
			RTSSoftDraw::Stroke(Canvas, { FVector2D(X0, Y), FVector2D(X1, Y) }, 1.0f + (I % 3), FLinearColor(0.58f, 0.68f, 0.75f, Alpha));
		}
	}
	Label(TEXT("RADAR OFFLINE"), View.Center);
}

void RTSAlerts::DrawEdgeArrows(UCanvas* Canvas, ARTSPlayerController* PC, const TArray<FRTSMinimapPing>& Pings)
{
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	const URTSMatchSubsystem* Match = World ? World->GetSubsystem<URTSMatchSubsystem>() : nullptr;
	const FRTSBakedMap* Map = Match ? Match->BakedMap() : nullptr;
	if (!Canvas || !Map)
	{
		return;
	}
	// (04/11, ele: "a seta ainda não tá no canto da tela, e a borda brilhante tá embaixo do placar do HUD [...] tem que ficar por
	// cima do HUD mesmo") a seta encosta na borda da tela e, com o brilho, sai depois das peças do HUD (o HUD chama isto no fim)
	const float S = FMath::Max(0.5f, Canvas->SizeY / 900.0f), W = Canvas->SizeX, H = Canvas->SizeY;
	const FVector2D Mid(W * 0.5f, H * 0.5f);
	const float Inset = 26.0f * S;   // a ponta a 26 px da borda: a seta inteira fica na tela
	TArray<float> Done;
	for (int32 I = Pings.Num() - 1; I >= 0; --I)
	{
		const FRTSMinimapPing& Pg = Pings[I];
		if (Pg.Kind != RTSAlerts::UnitAttacked && Pg.Kind != RTSAlerts::BaseAttacked)
		{
			continue;
		}
		FVector2D Sc;
		if (!PC->ProjectWorldLocationToScreen(FVector(Pg.Cm.X, Pg.Cm.Y, Map->GroundZCm(Pg.Cm)), Sc)
			|| (Sc.X >= 0 && Sc.Y >= 0 && Sc.X <= W && Sc.Y <= H))
		{
			continue;   // à vista: a seta some (a câmera chegou)
		}
		const FVector2D Dir = (Sc - Mid).GetSafeNormal();
		const float Ang = FMath::Atan2(Dir.Y, Dir.X);
		bool bNear = false;
		for (const float A : Done)
		{
			bNear |= FMath::Abs(FMath::FindDeltaAngleRadians(A, Ang)) < FMath::DegreesToRadians(12.0f);
		}
		if (bNear || Dir.IsNearlyZero())
		{
			continue;   // duas na mesma direção viram uma (a mais nova)
		}
		Done.Add(Ang);
		const float L = Pg.Age, Op = L > Pg.Life - 1.0f ? Clamp01(Pg.Life - L) : 1.0f;
		const FVector2D E = Mid + Dir * ExitT(Mid, Dir, FVector2D(0, 0), FVector2D(W, H));
		const FVector2D At = Mid + Dir * ExitT(Mid, Dir, FVector2D(Inset, Inset), FVector2D(W - Inset, H - Inset));
		// o brilho da borda (a C): a luz vermelha colada na borda da tela, no lado do problema, pulsando
		const float Glow = 0.75f * (0.55f + 0.45f * Blink(L, 1.2f)) * Op;
		const FLinearColor Strong = Col(Red, Glow), Clear = Col(Red, 0.0f);
		const bool bSide = FMath::Abs(E.X) < 1.0f || FMath::Abs(E.X - W) < 1.0f;
		if (bSide)
		{
			const float X0 = E.X < W * 0.5f ? 0.0f : W, X1 = E.X < W * 0.5f ? 46.0f * S : W - 46.0f * S;
			const float Y0 = FMath::Clamp(float(E.Y) - 110.0f * S, 0.0f, H), Y1 = FMath::Clamp(float(E.Y) + 110.0f * S, 0.0f, H);
			Gradient(Canvas, FVector2D(X0, Y0), FVector2D(X0, Y1), FVector2D(X1, Y0), FVector2D(X1, Y1), Strong, Clear);
		}
		else
		{
			const float Y0 = E.Y < H * 0.5f ? 0.0f : H, Y1 = E.Y < H * 0.5f ? 40.0f * S : H - 40.0f * S;
			const float X0 = FMath::Clamp(float(E.X) - 160.0f * S, 0.0f, W), X1 = FMath::Clamp(float(E.X) + 160.0f * S, 0.0f, W);
			Gradient(Canvas, FVector2D(X0, Y0), FVector2D(X1, Y0), FVector2D(X0, Y1), FVector2D(X1, Y1), Strong, Clear);
		}
		// a seta (a B): a ponta vermelha grande com o brilho que pulsa, a ponta pra fora da tela
		const FVector2D U = Dir, Sd(-Dir.Y, Dir.X);
		const auto Pt = [&](float A, float B, float K) { return At + (U * (A - 18.0f) + Sd * B) * S * K; };
		const float Pulse = 0.35f + 0.35f * FMath::Max(0.0f, FMath::Sin(L * 2.0f * PI * 1.4f));
		RTSSoftDraw::Fill(Canvas, { Pt(18, 0, 1.3f), Pt(-14, 20, 1.3f), Pt(-6, 0, 1.3f), Pt(-14, -20, 1.3f) }, Col(Red, Pulse * Op));
		const TArray<FVector2D> Arrow = { Pt(18, 0, 1.0f), Pt(-14, 20, 1.0f), Pt(-6, 0, 1.0f), Pt(-14, -20, 1.0f) };
		RTSSoftDraw::Outline(Canvas, Arrow, 2.4f * S, Col(Ink, Op));
		RTSSoftDraw::Fill(Canvas, Arrow, Col(Red, Op));
	}
}

bool RTSAlerts::TestRadarOff()
{
	static const bool bRadar = [] {
		FString V;
		return FParse::Value(FCommandLine::Get(), TEXT("-RTSAlertTest="), V) && V == TEXT("radar");
	}();
	return bRadar;
}
