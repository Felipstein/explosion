#include "RTSHUD.h"

#include "Engine/Canvas.h"
#include "RTSMapBaker.h"
#include "RTSMatchSubsystem.h"
#include "RTSPlayerController.h"

// O HUD provisório da economia e da construção (docs/ECONOMIA.md). Fica até o HUD de verdade
// sair da arte (docs/HUD.md: a barra do ZH modernizada, em Slate). Tudo em inglês (o jogo é em
// inglês por padrão, 09/10), pelo LOCTEXT. Texto no mínimo Slate 14 (XAG 101, docs/HUD.md).

#define LOCTEXT_NAMESPACE "RTSHUD"

namespace
{
	const FLinearColor EcoText(0.92f, 0.93f, 0.95f, 1.0f);
	const FLinearColor EcoDim(0.62f, 0.66f, 0.72f, 1.0f);
	const FLinearColor EcoMoney(0.98f, 0.82f, 0.30f, 1.0f);
	const FLinearColor EcoGood(0.35f, 0.95f, 0.40f, 1.0f);
	const FLinearColor EcoWarn(0.98f, 0.80f, 0.20f, 1.0f);
	const FLinearColor EcoBad(1.0f, 0.30f, 0.22f, 1.0f);
	const FLinearColor EcoPanel(0.02f, 0.025f, 0.03f, 0.78f);
	const FLinearColor EcoButton(0.07f, 0.08f, 0.10f, 0.92f);
	const FLinearColor EcoButtonHover(0.22f, 0.16f, 0.08f, 0.95f);
	/** O "+$" sobe 3 m e some em 1,5 s. */
	constexpr float PopupSeconds = 1.5f;
	constexpr float PopupRiseCm = 300.0f;
	constexpr float PanelW = 470.0f;
	constexpr float PanelMargin = 16.0f;

	FLinearColor Alpha(const FLinearColor& C, float A)
	{
		return FLinearColor(C.R, C.G, C.B, C.A * A);
	}

	/** A cor do jogador clareada pro texto (o Brighten do RTSHUD.cpp; outro nome por causa do unity build). */
	FLinearColor Lighter(const FLinearColor& C, float T)
	{
		return FLinearColor(FMath::Lerp(C.R, 1.0f, T), FMath::Lerp(C.G, 1.0f, T), FMath::Lerp(C.B, 1.0f, T), C.A);
	}

	FString Dollars(int32 Amount)
	{
		return FText::Format(LOCTEXT("Dollars", "${0}"), FText::AsNumber(Amount)).ToString();
	}

	FLinearColor HealthTint(float Ratio)
	{
		// os limiares do ZH: danificado 70%, muito danificado 35%
		return Ratio > 0.7f ? FLinearColor(0.25f, 0.9f, 0.3f) : (Ratio > 0.35f ? FLinearColor(0.95f, 0.8f, 0.15f) : FLinearColor(0.95f, 0.2f, 0.15f));
	}
}

bool ARTSHUD::HasEconomy(URTSMatchSubsystem* Match) const
{
	TArray<FRTSUnitState> Units;
	Match->GetUnits(Units);
	for (const FRTSUnitState& U : Units)
	{
		if (U.bStructure)
		{
			return true;
		}
	}
	return Match->DockCount() > 0;
}

bool ARTSHUD::OwnsStructure(URTSMatchSubsystem* Match, uint8 Player) const
{
	TArray<FRTSUnitState> Units;
	Match->GetUnits(Units);
	return Units.ContainsByPredicate([Player](const FRTSUnitState& U) { return U.bStructure && U.Owner == Player; });
}

void ARTSHUD::DrawPanelButton(FName Id, const FString& Label, const FString& Sub, float X, float Y, float W, float H, bool bEnabled)
{
	const FBox2D Box(FVector2D(X, Y), FVector2D(X + W, Y + H));
	const bool bHover = bEnabled && Box.IsInside(MousePos());
	DrawPanel(X, Y, W, H, bHover ? EcoButtonHover : EcoButton);
	DrawRect(bHover ? EcoMoney : FLinearColor(1, 1, 1, bEnabled ? 0.18f : 0.06f), X, Y, 3.0f, H);
	const FLinearColor Main = bEnabled ? (bHover ? FLinearColor::White : EcoText) : Alpha(EcoDim, 0.6f);
	DrawLabel(Label, X + 12.0f, Y + 5.0f, 14, Main, false, true);
	if (!Sub.IsEmpty())
	{
		DrawLabel(Sub, X + 12.0f, Y + H - 21.0f, 12, bEnabled ? EcoDim : Alpha(EcoDim, 0.5f));
	}
	Buttons.Add({ Id, Box });
}

void ARTSHUD::DrawGroundRect(const FVector2D& CenterCm, const FVector2D& HalfCm, const FLinearColor& Color, float Thickness)
{
	ARTSPlayerController* PC = Cast<ARTSPlayerController>(PlayerOwner);
	URTSMatchSubsystem* Match = GetWorld()->GetSubsystem<URTSMatchSubsystem>();
	const FRTSBakedMap* Map = Match ? Match->BakedMap() : nullptr;
	if (!PC || !Map)
	{
		return;
	}
	const FVector2D Corners[4] = {
		CenterCm + FVector2D(-HalfCm.X, -HalfCm.Y), CenterCm + FVector2D(HalfCm.X, -HalfCm.Y),
		CenterCm + FVector2D(HalfCm.X, HalfCm.Y), CenterCm + FVector2D(-HalfCm.X, HalfCm.Y) };
	for (int32 K = 0; K < 4; ++K)
	{
		const FVector2D A = Corners[K];
		const FVector2D B = Corners[(K + 1) % 4];
		// cada lado em pedaços de ~2 m, pra seguir o chão
		const int32 Steps = FMath::Clamp(int32(FVector2D::Distance(A, B) / 200.0), 2, 40);
		FVector2D Prev = FVector2D::ZeroVector;
		bool bPrev = false;
		for (int32 S = 0; S <= Steps; ++S)
		{
			const FVector2D P = FMath::Lerp(A, B, double(S) / Steps);
			FVector2D Screen;
			const bool bOk = PC->ProjectWorldLocationToScreen(FVector(P.X, P.Y, Map->GroundZCm(P) + 30.0f), Screen);
			if (bOk && bPrev)
			{
				DrawLine(Prev.X, Prev.Y, Screen.X, Screen.Y, Color, Thickness);
			}
			Prev = Screen;
			bPrev = bOk;
		}
	}
}

void ARTSHUD::DrawEconomyTopBar(URTSMatchSubsystem* Match)
{
	const float W = Canvas->SizeX;
	const uint8 Me = Match->LocalPlayer();
	const float BarW = 680.0f;
	const float X0 = W * 0.5f - BarW * 0.5f;
	DrawPanel(X0, 0.0f, BarW, 40.0f, FLinearColor(0.015f, 0.018f, 0.022f, 0.72f));
	// dinheiro
	DrawLabel(Dollars(Match->Money(Me)), X0 + 16.0f, 8.0f, 16, EcoMoney, false, true);
	// energia, nas cores do ZH (verde sobrando 5 ou mais, amarelo de 0 a 4, vermelho faltando)
	const int32 Made = Match->PowerMade(Me);
	const int32 Used = Match->PowerUsed(Me);
	const FLinearColor PowerColor = Used > Made ? EcoBad : (Made - Used >= 5 ? EcoGood : EcoWarn);
	const FString Power = FText::Format(LOCTEXT("Power", "POWER {0} / {1}"), Made, Used).ToString();
	const FVector2D PowerSize = DrawLabel(Power, X0 + 150.0f, 10.0f, 14, PowerColor, false, true);
	const float Speed = Match->ProductionSpeed(Me);
	if (Speed < 0.999f && OwnsStructure(Match, Me))
	{
		// sem energia tudo anda mais devagar (ZH): o HUD diz, pra barra não mentir (decisão do Felipe, 10/10); sem
		// prédio nenhum não há o que produzir, e o aviso só confundia (a arte, 13/10)
		const float Blink = 0.6f + 0.4f * FMath::Sin(float(FPlatformTime::Seconds()) * 6.0f);
		DrawLabel(FText::Format(LOCTEXT("LowPower", "LOW POWER  x{0}"), FText::AsNumber(Speed, &FNumberFormattingOptions::DefaultNoGrouping())).ToString(),
			X0 + 160.0f + PowerSize.X, 10.0f, 14, Alpha(EcoBad, Blink), false, true);
	}
	// relógio: o tempo de partida correndo
	DrawLabel(FString::Printf(TEXT("%d:%02d"), int32(Match->MatchSeconds()) / 60, int32(Match->MatchSeconds()) % 60), X0 + BarW * 0.5f + 70.0f, 8.0f, 16, EcoText, true, true);
	// (v4) a patente do general: "Rank 2 · 140/900 XP" (o XP dentro da patente; na última, o total) e, embaixo da faixa dos
	// jogadores, os poderes ligados com a tecla e o estado (o HUD de hoje não tem a coluna; F1–F3 valem nos dois)
	{
		const int32 Rank = Match->PlayerRank(Me);
		const int32 From = Match->RankXpThreshold(Rank);
		const int32 To = Match->RankXpThreshold(Rank + 1);
		const int32 Xp = Match->PlayerXp(Me);
		const FString RankText = Rank >= Sim::MaxRank || To <= From
			? FText::Format(LOCTEXT("OldRankMax", "Rank {0}  ·  {1} XP"), Rank, Xp).ToString()
			: FText::Format(LOCTEXT("OldRank", "Rank {0}  ·  {1}/{2} XP"), Rank, Xp - From, To - From).ToString();
		DrawLabel(RankText, X0 + BarW * 0.5f + 130.0f, 10.0f, 14, EcoMoney, false, true);
		float PY = 74.0f;
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			const int32 PowerIndex = Match->PowerAtSlot(Slot);
			URTSMatchSubsystem::FPowerInfo P;
			if (PowerIndex < 0 || !Match->GetPowerInfo(PowerIndex, P))
			{
				break;
			}
			const FText Why = Match->PowerRefusal(PowerIndex);
			const FString Line = FString::Printf(TEXT("F%d  %s  ·  %s"), Slot + 1, *P.Name,
				Why.IsEmpty() ? *LOCTEXT("OldPowerReady", "READY").ToString() : *Why.ToString());
			DrawLabel(Line, X0 + BarW * 0.5f + 130.0f, PY, 12, Why.IsEmpty() ? EcoMoney : EcoDim, false, Why.IsEmpty());
			PY += 18.0f;
		}
	}
	// quem está em campo, numa faixa embaixo da barra (DECISOES.md: o relógio no meio; você e os
	// aliados à esquerda, num bloco azul só; os inimigos à direita, na caixa escura; o derrotado
	// cinza com um X vermelho). Cada cartão: o quadrado e o nome da cor, na cor do jogador
	TArray<uint8> Friends;
	TArray<uint8> Foes;
	for (int32 P = 0; P < Match->PlayerCount(); ++P)
	{
		(P == Me || Match->IsAllyOfLocal(uint8(P)) ? Friends : Foes).Add(uint8(P));
	}
	Friends.Sort([Me](uint8 A, uint8 B) { return (A == Me) != (B == Me) ? A == Me : A < B; });
	const float CardW = 104.0f;
	const float StripY = 42.0f;
	const float Mid = W * 0.5f;
	const auto Card = [&](uint8 P, float X)
	{
		const bool bOut = Match->IsDefeated(P);
		const FLinearColor C = URTSMatchSubsystem::PlayerColor(P);
		DrawPanel(X + 8.0f, StripY + 8.0f, 10.0f, 10.0f, bOut ? FLinearColor(0.35f, 0.35f, 0.37f) : C);
		if (bOut)
		{
			DrawLine(X + 6.0f, StripY + 6.0f, X + 20.0f, StripY + 20.0f, EcoBad, 2.0f);
			DrawLine(X + 20.0f, StripY + 6.0f, X + 6.0f, StripY + 20.0f, EcoBad, 2.0f);
		}
		const FString Name = P == Me ? LOCTEXT("PlayerYou", "You").ToString() : URTSMatchSubsystem::PlayerColorName(P).ToString();
		DrawLabel(Name, X + 26.0f, StripY + 5.0f, 13, bOut ? Alpha(EcoDim, 0.6f) : Lighter(C, 0.35f), false, true);
	};
	const float FriendsW = CardW * Friends.Num();
	DrawPanel(Mid - 6.0f - FriendsW, StripY, FriendsW, 26.0f, FLinearColor(0.05f, 0.16f, 0.36f, 0.78f));
	for (int32 K = 0; K < Friends.Num(); ++K)
	{
		Card(Friends[K], Mid - 6.0f - FriendsW + CardW * K);
	}
	if (Foes.Num())
	{
		DrawPanel(Mid + 6.0f, StripY, CardW * Foes.Num(), 26.0f, FLinearColor(0.015f, 0.018f, 0.022f, 0.72f));
		for (int32 K = 0; K < Foes.Num(); ++K)
		{
			Card(Foes[K], Mid + 6.0f + CardW * K);
		}
	}
}

void ARTSHUD::DrawDefeats(URTSMatchSubsystem* Match)
{
	const float W = Canvas->SizeX;
	const double Now = GetWorld()->GetTimeSeconds();
	// (v4) no HUD novo o topo 1x1 tem 80 px da planta (escala com a tela): os letreiros ficam logo embaixo dele
	float Y = bNewHud ? FMath::Max(96.0f, 92.0f * FMath::Min(Canvas->SizeY / 1080.0f, Canvas->SizeX / 1920.0f)) : 96.0f;
	// (v4) a patente nova, 6 s em dourado no meio, com o que ela liberou ("PROMOTED - RANK 2: Cruise Missile Strike"); o
	// aviso do HUD novo só tem lugar pra "PROMOTED - RANK 2"
	const URTSMatchSubsystem::FHudNotice* Promoted = nullptr;
	for (const URTSMatchSubsystem::FHudNotice& N : Match->HudNotices())
	{
		Promoted = N.Kind == URTSMatchSubsystem::FHudNotice::EKind::Promoted && Now - N.At < 6.0 ? &N : Promoted;
	}
	if (Promoted)
	{
		const float Age = float(Now - Promoted->At);
		const float A = FMath::Clamp(FMath::Min(Age / 0.3f, (6.0f - Age) / 0.8f), 0.0f, 1.0f);
		DrawLabel(PromotionText(Match, Promoted->Value), W * 0.5f, Y, 18, Alpha(EcoMoney, A), true, true);
		Y += 30.0f;
	}
	// "Vermelho foi derrotado" pra todos, 5 s, na cor de quem caiu
	for (const FRTSDefeatNotice& N : Match->DefeatNotices())
	{
		const float Age = float(Now - N.At);
		if (Age > 5.0f || N.Player == Match->LocalPlayer())
		{
			continue;
		}
		const float A = FMath::Clamp((5.0f - Age) / 0.8f, 0.0f, 1.0f);
		const FText Who = URTSMatchSubsystem::PlayerColorName(N.Player);
		const FText Line = Match->IsAllyOfLocal(N.Player) ? FText::Format(LOCTEXT("AllyDefeated", "Your ally {0} was defeated"), Who)
			: FText::Format(LOCTEXT("PlayerDefeated", "{0} was defeated"), Who);
		DrawLabel(Line.ToString(), W * 0.5f, Y, 16, Alpha(Lighter(URTSMatchSubsystem::PlayerColor(N.Player), 0.3f), A), true, true);
		Y += 26.0f;
	}
	if (!Match->IsLocalDefeated() || Match->Result() != ERTSMatchResult::Playing)
	{
		return;
	}
	// você caiu: o letreiro grande some sozinho; a faixa do espectador fica
	double MyDefeat = -1.0;
	for (const FRTSDefeatNotice& N : Match->DefeatNotices())
	{
		MyDefeat = N.Player == Match->LocalPlayer() ? N.At : MyDefeat;
	}
	const float Age = MyDefeat >= 0.0 ? float(Now - MyDefeat) : 99.0f;
	if (Age < 6.0f)
	{
		const float A = FMath::Clamp((6.0f - Age) / 1.0f, 0.0f, 1.0f) * FMath::Clamp(Age / 0.4f, 0.0f, 1.0f);
		DrawLabel(LOCTEXT("YouWereDefeated", "DEFEATED").ToString(), W * 0.5f, Canvas->SizeY * 0.28f, 44, Alpha(EcoBad, A), true, true);
		DrawLabel(LOCTEXT("NowWatching", "You are now watching the match").ToString(), W * 0.5f, Canvas->SizeY * 0.28f + 62.0f, 14, Alpha(EcoText, A), true);
	}
	const FString Mode = Match->IsSpectatingAll() ? LOCTEXT("SpectatingAll", "SPECTATING  ·  full map").ToString()
		: LOCTEXT("SpectatingTeam", "SPECTATING  ·  your team's vision while your ally fights").ToString();
	DrawPanel(W * 0.5f - 250.0f, 72.0f, 500.0f, 22.0f, FLinearColor(0.015f, 0.018f, 0.022f, 0.66f));
	DrawLabel(Mode + TEXT("   ·   ") + LOCTEXT("SpaceLastAction", "Space: last action").ToString(), W * 0.5f, 75.0f, 11, EcoDim, true);
}

void ARTSHUD::DrawObserverPanel(URTSMatchSubsystem* Match)
{
	// a lista dos jogadores (o ZH troca a barra de comando pela do observador): cor, time, se
	// está vivo e o que tem. Enquanto o seu time luta, o inimigo só mostra se está vivo
	TArray<FRTSUnitState> Units;
	Match->GetUnits(Units);
	const int32 N = Match->PlayerCount();
	TArray<int32> UnitCount;
	TArray<int32> Buildings;
	UnitCount.Init(0, N);
	Buildings.Init(0, N);
	for (const FRTSUnitState& U : Units)
	{
		if (U.Owner < N)
		{
			(U.bStructure ? Buildings : UnitCount)[U.Owner] += 1;
		}
	}
	const bool bAll = Match->IsSpectatingAll();
	const float RowH = 30.0f;
	const float H = 46.0f + RowH * N;
	const float X0 = Canvas->SizeX - PanelW - PanelMargin;
	// (16/10) no HUD novo o canto de baixo à direita é do cartão do comandante (a lista caía por cima dele): ela vai pro
	// alto, embaixo do dinheiro (o HUD novo é desenhado em 1920 × 1080 e escala pela tela)
	const float NewHudS = FMath::Min(Canvas->SizeY / 1080.0f, Canvas->SizeX / 1920.0f);
	const float Y0 = bNewHud ? 110.0f * NewHudS : Canvas->SizeY - H - PanelMargin;   // (v4: o painel do comandante ganhou a linha da patente, 97 px)
	DrawPanel(X0, Y0, PanelW, H, EcoPanel);
	DrawLabel(LOCTEXT("ObserverTitle", "PLAYERS").ToString(), X0 + 14.0f, Y0 + 10.0f, 14, EcoText, false, true);
	DrawLabel(LOCTEXT("ObserverCols", "team        units   buildings   money").ToString(), X0 + 150.0f, Y0 + 14.0f, 10, EcoDim);
	for (int32 P = 0; P < N; ++P)
	{
		const float Y = Y0 + 40.0f + RowH * P;
		const bool bOut = Match->IsDefeated(uint8(P));
		const FLinearColor C = URTSMatchSubsystem::PlayerColor(uint8(P));
		DrawPanel(X0 + 14.0f, Y + 8.0f, 12.0f, 12.0f, bOut ? FLinearColor(0.35f, 0.35f, 0.37f) : C);
		const FString Name = P == Match->LocalPlayer() ? LOCTEXT("PlayerYou", "You").ToString() : URTSMatchSubsystem::PlayerColorName(uint8(P)).ToString();
		DrawLabel(Name, X0 + 34.0f, Y + 4.0f, 13, bOut ? EcoDim : Lighter(C, 0.35f), false, true);
		DrawLabel(FString::Printf(TEXT("%d"), int32(Match->TeamOf(uint8(P))) + 1), X0 + 160.0f, Y + 5.0f, 12, EcoText);
		const bool bDetails = bAll || P == Match->LocalPlayer() || Match->IsAllyOfLocal(uint8(P));
		if (bOut)
		{
			DrawLabel(LOCTEXT("ObserverOut", "defeated").ToString(), X0 + 220.0f, Y + 5.0f, 12, EcoBad);
		}
		else if (bDetails)
		{
			DrawLabel(FString::Printf(TEXT("%d"), UnitCount[P]), X0 + 226.0f, Y + 5.0f, 12, EcoText);
			DrawLabel(FString::Printf(TEXT("%d"), Buildings[P]), X0 + 290.0f, Y + 5.0f, 12, EcoText);
			DrawLabel(Dollars(Match->Money(uint8(P))), X0 + 370.0f, Y + 5.0f, 12, EcoMoney);
		}
		else
		{
			DrawLabel(LOCTEXT("ObserverHidden", "in the fight").ToString(), X0 + 220.0f, Y + 5.0f, 12, EcoDim);
		}
	}
}

void ARTSHUD::DrawCommandPanel(ARTSPlayerController* PC, URTSMatchSubsystem* Match)
{
	const uint8 Me = Match->LocalPlayer();
	const uint32 StructureId = PC->SelectedStructure();
	TArray<uint32> Dozers;
	TArray<uint32> Trucks;
	PC->SelectedWith(true, Dozers);
	PC->SelectedWith(false, Trucks);
	if (StructureId == 0 && Dozers.IsEmpty() && Trucks.IsEmpty())
	{
		return;
	}
	const float H = 228.0f;
	const float X0 = Canvas->SizeX - PanelW - PanelMargin;
	const float Y0 = Canvas->SizeY - H - PanelMargin;
	DrawPanel(X0, Y0, PanelW, H, EcoPanel);
	const int32 Money = Match->Money(Me);
	const float BW = (PanelW - 36.0f) * 0.5f;
	const float BH = 48.0f;
	// (v4) o que falta pro botão apagado: a patente vem com o número ("requires Rank 3")
	const int32 MyRank = Match->PlayerRank(Me);
	const auto Missing = [MyRank](const FRTSDefInfo& D)
	{
		return D.RequiresRank > MyRank ? FText::Format(LOCTEXT("NeedsRank", "requires Rank {0}"), D.RequiresRank).ToString()
			: LOCTEXT("NeedsBuilding", "requires another building").ToString();
	};

	if (StructureId != 0)
	{
		FRTSUnitState U;
		FRTSDefInfo D;
		if (!Match->GetUnit(StructureId, U) || !Match->GetDef(U.Def, D))
		{
			return;
		}
		DrawLabel(D.Name, X0 + 14.0f, Y0 + 10.0f, 16, EcoText, false, true);
		const float Ratio = float(U.Hp) / float(FMath::Max(1, U.MaxHp));
		DrawPanel(X0 + 14.0f, Y0 + 38.0f, PanelW - 28.0f, 6.0f, FLinearColor(0, 0, 0, 0.7f));
		DrawPanel(X0 + 14.0f, Y0 + 38.0f, (PanelW - 28.0f) * FMath::Clamp(Ratio, 0.0f, 1.0f), 6.0f, HealthTint(Ratio));
		if (D.Energy != 0)
		{
			DrawLabel(FText::Format(LOCTEXT("StructurePower", "{0} power"), FText::FromString(D.Energy > 0 ? FString::Printf(TEXT("+%d"), D.Energy) : FString::Printf(TEXT("%d"), D.Energy))).ToString(),
				X0 + PanelW - 14.0f - 110.0f, Y0 + 12.0f, 13, D.Energy > 0 ? EcoGood : EcoDim);
		}
		if (U.BuildFraction < 1.0f)
		{
			DrawLabel(FText::Format(LOCTEXT("UnderConstruction", "Under construction  {0}%"), FMath::FloorToInt(U.BuildFraction * 100.0f)).ToString(), X0 + 14.0f, Y0 + 58.0f, 14, EcoWarn, false, true);
			DrawPanel(X0 + 14.0f, Y0 + 84.0f, PanelW - 28.0f, 8.0f, FLinearColor(0, 0, 0, 0.7f));
			DrawPanel(X0 + 14.0f, Y0 + 84.0f, (PanelW - 28.0f) * U.BuildFraction, 8.0f, EcoWarn);
			DrawPanelButton(TEXT("eco_cancelbuild_0"), LOCTEXT("CancelConstruction", "CANCEL CONSTRUCTION").ToString(), LOCTEXT("FullRefund", "full refund").ToString(),
				X0 + 14.0f, Y0 + 106.0f, BW, BH, true);
			DrawLabel(LOCTEXT("ResumeHint", "A Dozer clicked on it resumes the work").ToString(), X0 + 14.0f, Y0 + H - 28.0f, 12, EcoDim);
			return;
		}
		// produção: cada tipo que o prédio treina
		for (int32 K = 0; K < D.Produces.Num() && K < 4; ++K)
		{
			FRTSDefInfo P;
			Match->GetDef(D.Produces[K], P);
			const bool bPrereq = Match->HasPrerequisites(D.Produces[K]);
			const bool bAfford = Money >= P.Cost;
			const FString Sub = bPrereq ? FString::Printf(TEXT("%s  ·  %.0fs"), *Dollars(P.Cost), P.BuildSeconds) : Missing(P);
			DrawPanelButton(FName(*FString::Printf(TEXT("eco_produce_%d"), D.Produces[K])), P.Name.ToUpper(), Sub,
				X0 + 14.0f + (K % 2) * (BW + 8.0f), Y0 + 58.0f + (K / 2) * (BH + 6.0f), BW, BH, bPrereq && bAfford);
		}
		// (v4) a zona de lançamento: o relógio do avião de carga e o que ele traz
		URTSMatchSubsystem::FAirliftInfo Air;
		if (Match->GetAirlift(StructureId, Air))
		{
			const int32 Left = FMath::Max(0, FMath::CeilToInt(Air.LeftS));
			const FString When = !Air.bStarted ? LOCTEXT("OldAirliftWaiting", "Airlift: waiting for clearance").ToString()
				: FText::Format(Air.bInbound ? LOCTEXT("OldAirliftInbound", "Airlift inbound  {0}") : (Air.bPaused ? LOCTEXT("OldAirliftPaused", "Low power: airlift paused  {0}")
					: LOCTEXT("OldAirliftNext", "Next airlift  {0}")), FText::FromString(FString::Printf(TEXT("%d:%02d"), Left / 60, Left % 60))).ToString();
			DrawLabel(When, X0 + 14.0f, Y0 + 58.0f, 14, Air.bPaused ? EcoBad : EcoMoney, false, true);
			DrawLabel(FText::Format(LOCTEXT("OldAirliftLoad", "{0} pallets  ·  {1}  ·  {2} crates on the pad"), Air.NextPallets, FText::FromString(Dollars(Air.NextValue)), Air.BoxesOnPad).ToString(),
				X0 + 14.0f, Y0 + 84.0f, 12, EcoDim);
		}
		// a fila, em ordem: clique tira (e devolve o dinheiro, ZH)
		TArray<uint16> Queue;
		Match->GetQueue(StructureId, Queue);
		if (!Queue.IsEmpty())
		{
			const float QY = Y0 + 122.0f;
			DrawLabel(LOCTEXT("QueueTitle", "Queue  (click to cancel)").ToString(), X0 + 14.0f, QY, 12, EcoDim);
			for (int32 K = 0; K < Queue.Num(); ++K)
			{
				FRTSDefInfo Q;
				Match->GetDef(Queue[K], Q);
				const float BX = X0 + 14.0f + K * 48.0f;
				const float BY = QY + 22.0f;
				const FBox2D Box(FVector2D(BX, BY), FVector2D(BX + 44.0f, BY + 44.0f));
				const bool bHover = Box.IsInside(MousePos());
				DrawPanel(BX, BY, 44.0f, 44.0f, bHover ? EcoButtonHover : EcoButton);
				DrawLabel(Q.Name.Left(1).ToUpper(), BX + 22.0f, BY + 10.0f, 16, EcoText, true, true);
				if (K == 0)
				{
					// o relógio do ZH vira uma barra: quanto já andou
					DrawPanel(BX, BY + 40.0f, 44.0f * U.QueueFraction, 4.0f, EcoMoney);
				}
				Buttons.Add({ FName(*FString::Printf(TEXT("eco_cancelq_%d"), K)), Box });
			}
		}
		const FString Footer = Match->ProductionSpeed(Me) < 0.999f ? LOCTEXT("SlowNoPower", "Low power: production is slower").ToString()
			: LOCTEXT("RallyHint", "Click the ground to set a rally point").ToString();
		DrawLabel(Footer, X0 + 14.0f, Y0 + H - 28.0f, 12, Match->ProductionSpeed(Me) < 0.999f ? EcoBad : EcoDim);
		return;
	}

	if (!Dozers.IsEmpty())
	{
		FRTSUnitState U;
		FRTSDefInfo D;
		if (!Match->GetUnit(Dozers[0], U) || !Match->GetDef(U.Def, D))
		{
			return;
		}
		DrawLabel(Dozers.Num() > 1 ? FText::Format(LOCTEXT("DozerCount", "{0}  x{1}"), FText::FromString(D.Name), Dozers.Num()).ToString() : D.Name,
			X0 + 14.0f, Y0 + 10.0f, 16, EcoText, false, true);
		for (int32 K = 0; K < D.Constructs.Num() && K < 6; ++K)
		{
			FRTSDefInfo B;
			Match->GetDef(D.Constructs[K], B);
			const bool bPrereq = Match->HasPrerequisites(D.Constructs[K]);
			const bool bAfford = Money >= B.Cost;
			const FString Sub = bPrereq ? FString::Printf(TEXT("%s  ·  %.0fs"), *Dollars(B.Cost), B.BuildSeconds) : Missing(B);
			DrawPanelButton(FName(*FString::Printf(TEXT("eco_build_%d"), D.Constructs[K])), B.Name.ToUpper(), Sub,
				X0 + 14.0f + (K % 2) * (BW + 8.0f), Y0 + 40.0f + (K / 2) * (BH + 6.0f), BW, BH, bPrereq && bAfford);
		}
		const FString Hint = PC->GetPlacingDef() >= 0 ? LOCTEXT("PlaceHint", "Left-click: place  ·  R: rotate  ·  Shift: more  ·  Right-click: cancel").ToString()
			: LOCTEXT("BuildHint", "Pick a building, then place it on the ground").ToString();
		DrawLabel(Hint, X0 + 14.0f, Y0 + H - 28.0f, 12, EcoDim);
		return;
	}

	FRTSUnitState T;
	FRTSDefInfo D;
	if (!Match->GetUnit(Trucks[0], T) || !Match->GetDef(T.Def, D))
	{
		return;
	}
	DrawLabel(Trucks.Num() > 1 ? FText::Format(LOCTEXT("TruckCount", "{0}  x{1}"), FText::FromString(D.Name), Trucks.Num()).ToString() : D.Name,
		X0 + 14.0f, Y0 + 10.0f, 16, EcoText, false, true);
	DrawLabel(FText::Format(LOCTEXT("Cargo", "Cargo  {0} / {1} crates"), T.Cargo, D.SupplyBoxes).ToString(), X0 + 14.0f, Y0 + 46.0f, 14, EcoMoney, false, true);
	// na ordem do Sim::UnitTask
	const FText Tasks[] = { LOCTEXT("TaskIdle", "Idle"), LOCTEXT("TaskBuilding", "Building"), LOCTEXT("TaskToDock", "Going to the supplies"),
		LOCTEXT("TaskLoading", "Loading"), LOCTEXT("TaskLeaveDock", "Leaving the dock"), LOCTEXT("TaskToCenter", "Delivering"),
		LOCTEXT("TaskUnloading", "Unloading"), LOCTEXT("TaskWaitCenter", "No supply center"), LOCTEXT("TaskWaitDock", "No supplies in range") };
	const FString TaskName = T.Task < UE_ARRAY_COUNT(Tasks) ? Tasks[T.Task].ToString() : FString();
	DrawLabel(TaskName, X0 + 14.0f, Y0 + 74.0f, 14, EcoText);
	DrawLabel(LOCTEXT("GatherHint", "Click a supply dock to gather there").ToString(), X0 + 14.0f, Y0 + H - 28.0f, 12, EcoDim);
}

void ARTSHUD::DrawPlacementGhost(ARTSPlayerController* PC, URTSMatchSubsystem* Match)
{
	const int32 Def = PC->GetPlacingDef();
	FVector Ground;
	if (Def < 0 || !PC->GetCursorGround(Ground))
	{
		return;
	}
	FVector2D Center;
	FVector2D Half;
	const ERTSPlace R = Match->CanPlace(uint16(Def), PC->GetPlacingDir(), FVector2D(Ground.X, Ground.Y), Center, Half);
	const bool bOk = R == ERTSPlace::Ok;
	const FLinearColor C = bOk ? EcoGood : EcoBad;
	DrawGroundRect(Center, Half, C, 2.5f);
	DrawGroundRect(Center, Half * 0.96, Alpha(C, 0.5f), 1.0f);
	// a frente: um traço do centro até a porta (o lado pra onde o prédio olha)
	const int32 Dir = PC->GetPlacingDir();
	const FVector2D Front = Center + FVector2D(Dir == 0 ? Half.X : (Dir == 2 ? -Half.X : 0.0), Dir == 1 ? Half.Y : (Dir == 3 ? -Half.Y : 0.0));
	const FRTSBakedMap* Map = Match->BakedMap();
	FVector2D A;
	FVector2D B;
	if (Map && PC->ProjectWorldLocationToScreen(FVector(Center.X, Center.Y, Map->GroundZCm(Center) + 30.0f), A)
		&& PC->ProjectWorldLocationToScreen(FVector(Front.X, Front.Y, Map->GroundZCm(Front) + 30.0f), B))
	{
		DrawLine(A.X, A.Y, B.X, B.Y, C, 3.0f);
	}
	FRTSDefInfo D;
	Match->GetDef(uint16(Def), D);
	const FVector2D M = MousePos();
	DrawLabel(FString::Printf(TEXT("%s  %s"), *D.Name, *Dollars(D.Cost)), M.X + 22.0f, M.Y + 14.0f, 14, EcoText, false, true);
	if (!bOk)
	{
		// (v4) o teto vem com o número: "Limit reached (2)"
		const FString Reason = R == ERTSPlace::LimitReached && D.MaxPerPlayer > 0
			? FText::Format(LOCTEXT("PlaceLimitReachedN", "Limit reached ({0})"), D.MaxPerPlayer).ToString() : URTSMatchSubsystem::PlaceReason(R);
		DrawLabel(Reason, M.X + 22.0f, M.Y + 36.0f, 14, EcoBad, false, true);
	}
}

void ARTSHUD::DrawRallyPoints(ARTSPlayerController* PC, URTSMatchSubsystem* Match)
{
	const FRTSBakedMap* Map = Match->BakedMap();
	if (!Map)
	{
		return;
	}
	for (const uint32 Id : PC->GetSelection())
	{
		FRTSUnitState U;
		if (!Match->GetUnit(Id, U) || !U.bStructure || !U.bHasRally || U.Owner != Match->LocalPlayer())
		{
			continue;
		}
		const FLinearColor C = URTSMatchSubsystem::PlayerColor(U.Owner);
		const FVector From(U.PosCm.X, U.PosCm.Y, Map->GroundZCm(U.PosCm) + 60.0f);
		const FVector To(U.RallyCm.X, U.RallyCm.Y, Map->GroundZCm(U.RallyCm) + 30.0f);
		FVector2D A;
		FVector2D B;
		FVector2D Top;
		if (PC->ProjectWorldLocationToScreen(From, A) && PC->ProjectWorldLocationToScreen(To, B) && PC->ProjectWorldLocationToScreen(To + FVector(0, 0, 600.0f), Top))
		{
			DrawLine(A.X, A.Y, B.X, B.Y, Alpha(C, 0.55f), 1.5f);
			// a bandeira do ponto de encontro (ZH): mastro e flâmula na cor do jogador
			DrawLine(B.X, B.Y, Top.X, Top.Y, FLinearColor(0.9f, 0.9f, 0.9f, 0.9f), 2.0f);
			const float FlagW = FMath::Max(10.0f, FVector2D::Distance(B, Top) * 0.45f);
			DrawPanel(Top.X, Top.Y, FlagW, FlagW * 0.6f, C);
		}
	}
}

void ARTSHUD::DrawMoneyPopups(URTSMatchSubsystem* Match, float DeltaSeconds)
{
	TArray<FRTSMoneyPopup> Fresh;
	Match->TakeMoneyPopups(Fresh);
	for (const FRTSMoneyPopup& P : Fresh)
	{
		Popups.Add({ P.PosCm, P.Amount, 0.0f });
	}
	ARTSPlayerController* PC = Cast<ARTSPlayerController>(PlayerOwner);
	const FRTSBakedMap* Map = Match->BakedMap();
	for (int32 K = Popups.Num() - 1; K >= 0; --K)
	{
		FPopup& P = Popups[K];
		P.Age += DeltaSeconds;
		if (P.Age > PopupSeconds)
		{
			Popups.RemoveAt(K);
			continue;
		}
		FVector2D S;
		const float Rise = P.Age / PopupSeconds;
		if (PC && Map && PC->ProjectWorldLocationToScreen(FVector(P.Cm.X, P.Cm.Y, Map->GroundZCm(P.Cm) + 900.0f + Rise * PopupRiseCm), S))
		{
			DrawLabel(FString::Printf(TEXT("+%s"), *Dollars(P.Amount)), S.X, S.Y, 16, Alpha(EcoMoney, 1.0f - Rise * Rise), true, true);
		}
	}
}

void ARTSHUD::DrawDockHover(ARTSPlayerController* PC, URTSMatchSubsystem* Match)
{
	FVector Ground;
	if (PC->GetPlacingDef() >= 0 || PC->GetTargetingPower() >= 0 || !PC->GetCursorGround(Ground))
	{
		return;
	}
	for (int32 K = 0; K < Match->DockCount(); ++K)
	{
		FRTSDockState D;
		if (Match->GetDock(K, D) && D.Boxes > 0 && FMath::Abs(Ground.X - D.PosCm.X) <= D.HalfCm.X && FMath::Abs(Ground.Y - D.PosCm.Y) <= D.HalfCm.Y)
		{
			const FVector2D M = MousePos();
			const FString Label = D.bPile ? LOCTEXT("DroppedCargo", "Dropped cargo").ToString() : LOCTEXT("SupplyDock", "Supply dock").ToString();
			DrawLabel(FString::Printf(TEXT("%s  %s"), *Label, *Dollars(D.Boxes * Match->BoxValue())), M.X + 22.0f, M.Y + 14.0f, 14, EcoMoney, false, true);
			DrawGroundRect(D.PosCm, D.HalfCm, Alpha(EcoMoney, 0.7f), 1.5f);
			return;
		}
	}
}

#undef LOCTEXT_NAMESPACE
