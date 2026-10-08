#include "RTSHUD.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "RTSGameData.h"
#include "RTSMapBaker.h"
#include "RTSMatchSubsystem.h"
#include "RTSPlayerController.h"

namespace
{
	const FLinearColor TextColor(0.92f, 0.93f, 0.95f, 1.0f);
	const FLinearColor DimText(0.62f, 0.66f, 0.72f, 1.0f);
	const FLinearColor Accent(0.95f, 0.62f, 0.18f, 1.0f);
	const FLinearColor ButtonFill(0.07f, 0.08f, 0.10f, 0.80f);
	const FLinearColor ButtonHover(0.22f, 0.16f, 0.08f, 0.90f);
	const FLinearColor MoveGreen(0.35f, 1.0f, 0.4f, 1.0f);
	const FLinearColor AttackRed(1.0f, 0.25f, 0.2f, 1.0f);

	/** O anel fica um pouco por fora do casco (o tanque tem 7,9 m: raio de colisão 3,6 m × 1,3). */
	constexpr float RingScale = 1.3f;
	constexpr float ResultDelaySeconds = 3.0f;
	constexpr float ObjectiveSeconds = 7.0f;
	constexpr float HelpSeconds = 30.0f;
	constexpr float FadeInSeconds = 1.2f;

	FLinearColor HealthColor(float Ratio)
	{
		// verde → amarelo → vermelho, nos limiares do ZH (danificado 70%, muito danificado 35%)
		if (Ratio > 0.7f) { return FLinearColor(0.25f, 0.9f, 0.3f); }
		if (Ratio > 0.35f) { return FLinearColor(0.95f, 0.8f, 0.15f); }
		return FLinearColor(0.95f, 0.2f, 0.15f);
	}

	FLinearColor Brighten(const FLinearColor& C, float T)
	{
		return FLinearColor(FMath::Lerp(C.R, 1.0f, T), FMath::Lerp(C.G, 1.0f, T), FMath::Lerp(C.B, 1.0f, T), C.A);
	}

	FLinearColor WithAlpha(const FLinearColor& C, float A)
	{
		return FLinearColor(C.R, C.G, C.B, A);
	}

	FString Clock(float Seconds)
	{
		const int32 S = FMath::FloorToInt(Seconds);
		return FString::Printf(TEXT("%d:%02d"), S / 60, S % 60);
	}

	/**
	 * A fonte da engine (Roboto, guardada em tempo de execução: desenhada já no tamanho
	 * pedido, sem borrar). A do estilo do Slate não aparecia no canvas (teste t2, 08/10).
	 * Size em pontos (96 dpi = 1,333 px por ponto).
	 */
	UFont* LabelFont()
	{
		return GEngine->GetLargeFont();
	}

	float LabelScale(const UFont* Font, int32 Size)
	{
		return float(Size) * 1.333f / float(FMath::Max(1, Font ? Font->LegacyFontSize : 16));
	}
}

FVector2D ARTSHUD::MousePos() const
{
	float X = -1.0f;
	float Y = -1.0f;
	if (PlayerOwner)
	{
		PlayerOwner->GetMousePosition(X, Y);
	}
	return FVector2D(X, Y);
}

FName ARTSHUD::ButtonAt(const FVector2D& Screen) const
{
	for (const FButton& B : Buttons)
	{
		if (B.Box.IsInside(Screen))
		{
			return B.Id;
		}
	}
	return NAME_None;
}

bool ARTSHUD::FindButton(FName Id, FBox2D& OutBox) const
{
	for (const FButton& B : Buttons)
	{
		if (B.Id == Id)
		{
			OutBox = B.Box;
			return true;
		}
	}
	return false;
}

void ARTSHUD::DrawPanel(float X, float Y, float W, float H, const FLinearColor& Color)
{
	DrawRect(Color, X, Y, W, H);
}

FVector2D ARTSHUD::MeasureLabel(const FString& Text, int32 Size, bool bBold) const
{
	UFont* Font = LabelFont();
	float W = 0.0f;
	float H = 0.0f;
	const_cast<ARTSHUD*>(this)->GetTextSize(Text, W, H, Font, LabelScale(Font, Size));
	return FVector2D(W, H);
}

FVector2D ARTSHUD::DrawLabel(const FString& Text, float X, float Y, int32 Size, const FLinearColor& Color, bool bCenter, bool bBold)
{
	UFont* Font = LabelFont();
	const float Scale = LabelScale(Font, Size);
	const FVector2D Extent = MeasureLabel(Text, Size, bBold);
	const float DrawX = bCenter ? X - Extent.X * 0.5f : X;
	DrawText(Text, FLinearColor(0.0f, 0.0f, 0.0f, 0.65f * Color.A), DrawX + 1.5f, Y + 1.5f, Font, Scale);
	if (bBold)
	{
		DrawText(Text, Color, DrawX + 0.6f, Y, Font, Scale);   // negrito barato: o mesmo texto deslocado
	}
	DrawText(Text, Color, DrawX, Y, Font, Scale);
	return Extent;
}

void ARTSHUD::DrawButton(FName Id, const FString& Label, float X, float Y, float W, float H)
{
	const FBox2D Box(FVector2D(X, Y), FVector2D(X + W, Y + H));
	const bool bHover = Box.IsInside(MousePos());
	DrawPanel(X, Y, W, H, bHover ? ButtonHover : ButtonFill);
	DrawRect(bHover ? Accent : FLinearColor(1, 1, 1, 0.16f), X, Y, 3.0f, H);
	const FVector2D Extent = MeasureLabel(Label, 15, true);
	DrawLabel(Label, X + 24.0f, Y + (H - Extent.Y) * 0.5f, 15, bHover ? FLinearColor::White : TextColor, false, true);
	Buttons.Add({ Id, Box });
}

void ARTSHUD::DrawHUD()
{
	Super::DrawHUD();
	Buttons.Reset();
	ARTSPlayerController* PC = Cast<ARTSPlayerController>(PlayerOwner);
	if (!PC || !Canvas)
	{
		return;
	}
	if (PC->IsMenu())
	{
		DrawMenu(PC);
		return;
	}
	URTSMatchSubsystem* Match = GetWorld()->GetSubsystem<URTSMatchSubsystem>();
	if (!Match || !Match->IsRunning())
	{
		DrawPanel(0, 0, Canvas->SizeX, Canvas->SizeY, FLinearColor(0, 0, 0, 0.6f));
		const bool bError = !PC->GetError().IsEmpty();
		DrawLabel(bError ? PC->GetError() : TEXT("Montando a partida…"), Canvas->SizeX * 0.5f, Canvas->SizeY * 0.45f, 15, bError ? AttackRed : TextColor, true);
		DrawButton(TEXT("menu"), TEXT("VOLTAR AO MENU"), Canvas->SizeX * 0.5f - 170.0f, Canvas->SizeY * 0.55f, 340.0f, 52.0f);
		return;
	}
	DrawMatch(PC, Match);
}

void ARTSHUD::DrawMenu(ARTSPlayerController* PC)
{
	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	// painel à esquerda, escurecendo o mapa ao fundo; a borda some num degradê
	const float PanelW = FMath::Max(440.0f, W * 0.3f);
	DrawPanel(0, 0, PanelW, H, FLinearColor(0.015f, 0.018f, 0.022f, 0.66f));
	for (int32 K = 0; K < 24; ++K)
	{
		DrawPanel(PanelW + K * 4.0f, 0, 4.0f, H, FLinearColor(0.015f, 0.018f, 0.022f, 0.66f * (1.0f - (K + 1) / 25.0f)));
	}

	const float X = 64.0f;
	float Y = H * 0.20f;
	DrawLabel(TEXT("RTS"), X, Y, 54, TextColor, false, true);
	Y += 82.0f;
	DrawPanel(X, Y, 64.0f, 3.0f, Accent);
	Y += 16.0f;
	DrawLabel(TEXT("USA  ·  CHINA  ·  GLA"), X, Y, 13, Accent, false, true);
	Y += 26.0f;
	DrawLabel(TEXT("protótipo  ·  skirmish local"), X, Y, 12, DimText);

	Y = H * 0.50f;
	DrawButton(TEXT("skirmish"), TEXT("JOGAR SKIRMISH"), X, Y, PanelW - X * 2.0f, 58.0f);
	DrawLabel(TEXT("Deserto (salina)  ·  você × IA  ·  6 tanques cada"), X + 2.0f, Y + 68.0f, 11, DimText);
	Y += 124.0f;
	DrawButton(TEXT("quit"), TEXT("SAIR"), X, Y, PanelW - X * 2.0f, 58.0f);
}

void ARTSHUD::DrawMatch(ARTSPlayerController* PC, URTSMatchSubsystem* Match)
{
	DrawUnits(PC, Match);
	DrawMarkers(PC);

	FVector2D A;
	FVector2D B;
	if (PC->IsDragging(A, B))
	{
		const FVector2D Lo(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
		const FVector2D Hi(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y));
		DrawPanel(Lo.X, Lo.Y, Hi.X - Lo.X, Hi.Y - Lo.Y, WithAlpha(MoveGreen, 0.07f));
		DrawLine(Lo.X, Lo.Y, Hi.X, Lo.Y, MoveGreen, 1.0f);
		DrawLine(Hi.X, Lo.Y, Hi.X, Hi.Y, MoveGreen, 1.0f);
		DrawLine(Hi.X, Hi.Y, Lo.X, Hi.Y, MoveGreen, 1.0f);
		DrawLine(Lo.X, Hi.Y, Lo.X, Lo.Y, MoveGreen, 1.0f);
	}
	if (PC->IsAttackMoveArmed())
	{
		const FVector2D M = MousePos();
		DrawLabel(TEXT("ATAQUE ANDANDO"), M.X + 20.0f, M.Y + 16.0f, 11, AttackRed, false, true);
	}

	DrawTopBar(Match);
	DrawEnemyArrow(Match);
	const bool bPlaying = Match->Result() == ERTSMatchResult::Playing;
	if (bPlaying && Match->SecondsSinceStart() < ObjectiveSeconds)
	{
		DrawObjective(Match);
	}
	if (bPlaying && Match->MatchSeconds() < HelpSeconds && PC->OrdersGiven() < 2)
	{
		DrawLabel(TEXT("Esquerdo: selecionar e ordenar   ·   Direito: tirar seleção / arrastar rola   ·   A: ataque andando   ·   S: parar   ·   Ctrl+número: grupo   ·   Esc: pausa"),
			Canvas->SizeX * 0.5f, Canvas->SizeY - 34.0f, 10, WithAlpha(DimText, 0.85f), true);
	}
	if (PC->ShowDebug())
	{
		DrawLabel(Match->DebugLine(), 16.0f, Canvas->SizeY - 56.0f, 10, Accent);
	}

	if (!bPlaying && Match->SecondsSinceEnd() > ResultDelaySeconds)
	{
		DrawResult(Match);
	}
	else if (PC->IsPauseMenuOpen())
	{
		DrawPause();
	}

	// entrada: o preto abre em 1,2 s (esconde as texturas ainda chegando)
	const float Fade = 1.0f - Match->SecondsSinceStart() / FadeInSeconds;
	if (Fade > 0.0f)
	{
		DrawPanel(0, 0, Canvas->SizeX, Canvas->SizeY, FLinearColor(0.01f, 0.01f, 0.012f, FMath::Clamp(Fade, 0.0f, 1.0f)));
	}
}

void ARTSHUD::DrawGroundRing(const FVector& Center, float RadiusCm, const FLinearColor& Color, float Thickness)
{
	constexpr int32 Segments = 28;
	FVector2D Prev = FVector2D::ZeroVector;
	bool bHavePrev = false;
	for (int32 K = 0; K <= Segments; ++K)
	{
		const float Ang = 2.0f * PI * float(K) / float(Segments);
		const FVector P = Center + FVector(FMath::Cos(Ang) * RadiusCm, FMath::Sin(Ang) * RadiusCm, 0.0f);
		FVector2D S;
		const bool bOk = PlayerOwner->ProjectWorldLocationToScreen(P, S);
		if (bOk && bHavePrev)
		{
			DrawLine(Prev.X, Prev.Y, S.X, S.Y, Color, Thickness);
		}
		Prev = S;
		bHavePrev = bOk;
	}
}

void ARTSHUD::DrawUnits(ARTSPlayerController* PC, URTSMatchSubsystem* Match)
{
	const FRTSBakedMap* Map = Match->BakedMap();
	TArray<FRTSUnitState> Units;
	Match->GetUnits(Units);
	const TArray<uint32>& Selected = PC->GetSelection();
	for (const FRTSUnitState& U : Units)
	{
		const FRTSUnitVisual* Visual = Match->VisualOf(U.Def);
		const float RadiusCm = (Visual ? Visual->RadiusM : 3.0f) * 100.0f * RingScale;
		const FVector Ground(U.PosCm.X, U.PosCm.Y, (Map ? Map->GroundZCm(U.PosCm) : 0.0f) + 25.0f);
		const bool bSelected = Selected.Contains(U.Id);
		const bool bHover = PC->GetHoverId() == U.Id;
		const bool bEnemy = Match->IsEnemyOfLocal(U.Owner);
		const FLinearColor Team = URTSMatchSubsystem::PlayerColor(U.Owner);
		if (bSelected)
		{
			DrawGroundRing(Ground, RadiusCm, Brighten(Team, 0.45f), 2.5f);
		}
		else if (bHover)
		{
			DrawGroundRing(Ground, RadiusCm, Brighten(Team, 0.3f), 2.0f);
		}
		else
		{
			DrawGroundRing(Ground, RadiusCm, WithAlpha(Team, bEnemy ? 0.6f : 0.5f), bEnemy ? 2.0f : 1.5f);
		}

		const float Ratio = float(U.Hp) / float(U.MaxHp);
		if (!bSelected && !bHover && Ratio >= 1.0f)
		{
			continue;
		}
		FVector2D Top;
		if (!PlayerOwner->ProjectWorldLocationToScreen(Ground + FVector(0, 0, 430.0f), Top))
		{
			continue;
		}
		const float BarW = 54.0f;
		const float BarH = 5.0f;
		const float X = Top.X - BarW * 0.5f;
		const float Y = Top.Y - 12.0f;
		DrawPanel(X - 1.0f, Y - 1.0f, BarW + 2.0f, BarH + 2.0f, WithAlpha(Team, 0.95f));
		DrawPanel(X, Y, BarW, BarH, FLinearColor(0, 0, 0, 0.75f));
		DrawPanel(X, Y, BarW * FMath::Clamp(Ratio, 0.0f, 1.0f), BarH, HealthColor(Ratio));
	}
}

void ARTSHUD::DrawMarkers(ARTSPlayerController* PC)
{
	for (const FRTSOrderMarker& M : PC->GetMarkers())
	{
		const float T = FMath::Clamp(M.Age / 0.7f, 0.0f, 1.0f);
		const FLinearColor Base = M.bAttack ? AttackRed : MoveGreen;
		DrawGroundRing(M.World + FVector(0, 0, 30.0f), FMath::Lerp(280.0f, 70.0f, T), WithAlpha(Base, 1.0f - T), 2.0f);
	}
}

void ARTSHUD::DrawTopBar(URTSMatchSubsystem* Match)
{
	TArray<FRTSUnitState> Units;
	Match->GetUnits(Units);
	int32 Mine = 0;
	int32 Theirs = 0;
	for (const FRTSUnitState& U : Units)
	{
		(U.Owner == Match->LocalPlayer() ? Mine : Theirs) += 1;
	}
	const float W = Canvas->SizeX;
	DrawPanel(W * 0.5f - 240.0f, 0.0f, 480.0f, 36.0f, FLinearColor(0.015f, 0.018f, 0.022f, 0.6f));
	DrawLabel(Clock(Match->MatchSeconds()), W * 0.5f, 7.0f, 13, TextColor, true, true);
	DrawLabel(FString::Printf(TEXT("Seus tanques  %d"), Mine), W * 0.5f - 220.0f, 9.0f, 11, Brighten(URTSMatchSubsystem::PlayerColor(Match->LocalPlayer()), 0.35f), false, true);
	DrawLabel(FString::Printf(TEXT("Inimigo  %d"), Theirs), W * 0.5f + 120.0f, 9.0f, 11, Brighten(URTSMatchSubsystem::PlayerColor(1), 0.35f), false, true);
}

void ARTSHUD::DrawObjective(URTSMatchSubsystem* Match)
{
	const float T = Match->SecondsSinceStart();
	const float Alpha = FMath::Clamp(FMath::Min((T - 0.6f) / 0.6f, (ObjectiveSeconds - T) / 1.0f), 0.0f, 1.0f);
	if (Alpha <= 0.0f)
	{
		return;
	}
	const float W = Canvas->SizeX;
	const float Y = Canvas->SizeY * 0.2f;
	DrawPanel(W * 0.5f - 300.0f, Y - 14.0f, 600.0f, 112.0f, FLinearColor(0.01f, 0.012f, 0.015f, 0.5f * Alpha));
	const float TitleY = Y + DrawLabel(TEXT("OBJETIVO"), W * 0.5f, Y, 11, WithAlpha(Accent, Alpha), true, true).Y + 2.0f;
	const float SubY = TitleY + DrawLabel(TEXT("Destrua os 6 tanques inimigos"), W * 0.5f, TitleY, 20, WithAlpha(TextColor, Alpha), true, true).Y + 6.0f;
	DrawLabel(TEXT("eles vêm de lá — siga a seta vermelha"), W * 0.5f, SubY, 11, WithAlpha(DimText, Alpha), true);
}

void ARTSHUD::DrawEnemyArrow(URTSMatchSubsystem* Match)
{
	// só quando nenhum inimigo aparece na tela: a seta na borda aponta pro mais perto do centro da vista
	const FRTSBakedMap* Map = Match->BakedMap();
	TArray<FRTSUnitState> Units;
	Match->GetUnits(Units);
	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	const FBox2D Screen(FVector2D(0, 0), FVector2D(W, H));
	FVector Origin;
	FVector Dir;
	if (!PlayerOwner->DeprojectScreenPositionToWorld(W * 0.5f, H * 0.5f, Origin, Dir) || FMath::Abs(Dir.Z) < 1e-3)
	{
		return;
	}
	const float GroundZ = Map ? Map->GroundZCm(FVector2D(Origin.X, Origin.Y)) : 0.0f;
	const FVector Center = Origin + Dir * ((GroundZ - Origin.Z) / Dir.Z);
	const FRTSUnitState* Nearest = nullptr;
	double Best = 0.0;
	for (const FRTSUnitState& U : Units)
	{
		if (!Match->IsEnemyOfLocal(U.Owner))
		{
			continue;
		}
		FVector2D S;
		const FVector P(U.PosCm.X, U.PosCm.Y, Map ? Map->GroundZCm(U.PosCm) : 0.0f);
		if (PlayerOwner->ProjectWorldLocationToScreen(P, S) && Screen.IsInside(S))
		{
			return;   // tem inimigo à vista
		}
		const double D = FVector2D::Distance(U.PosCm, FVector2D(Center.X, Center.Y));
		if (!Nearest || D < Best)
		{
			Nearest = &U;
			Best = D;
		}
	}
	if (!Nearest)
	{
		return;
	}
	// a direção na tela: a do inimigo projetado (ou a do chão, girada pela vista)
	const FVector Target(Nearest->PosCm.X, Nearest->PosCm.Y, Map ? Map->GroundZCm(Nearest->PosCm) : 0.0f);
	FVector2D S;
	FVector2D D2;
	if (PlayerOwner->ProjectWorldLocationToScreen(Target, S))
	{
		D2 = (S - FVector2D(W * 0.5f, H * 0.5f)).GetSafeNormal();
	}
	else
	{
		const FRotator CamRot = PlayerOwner->PlayerCameraManager ? PlayerOwner->PlayerCameraManager->GetCameraRotation() : FRotator::ZeroRotator;
		const FVector Local = FRotator(0.0f, CamRot.Yaw, 0.0f).UnrotateVector(Target - Center);
		D2 = FVector2D(Local.Y, -Local.X).GetSafeNormal();
	}
	if (D2.IsNearlyZero())
	{
		return;
	}
	// o ponto na borda (recuado 46 px) na direção do inimigo
	const float Inset = 46.0f;
	const double Sx = FMath::Abs(D2.X) > 1e-6 ? (W * 0.5f - Inset) / FMath::Abs(D2.X) : 1e9;
	const double Sy = FMath::Abs(D2.Y) > 1e-6 ? (H * 0.5f - Inset) / FMath::Abs(D2.Y) : 1e9;
	const FVector2D Tip = FVector2D(W * 0.5f, H * 0.5f) + D2 * FMath::Min(Sx, Sy);
	const FVector2D Side(-D2.Y, D2.X);
	const FVector2D Back = Tip - D2 * 26.0;
	const FLinearColor C = WithAlpha(AttackRed, 0.9f);
	DrawLine(Tip.X, Tip.Y, Back.X + Side.X * 13.0, Back.Y + Side.Y * 13.0, C, 3.0f);
	DrawLine(Tip.X, Tip.Y, Back.X - Side.X * 13.0, Back.Y - Side.Y * 13.0, C, 3.0f);
	DrawLine(Back.X + Side.X * 13.0, Back.Y + Side.Y * 13.0, Back.X - Side.X * 13.0, Back.Y - Side.Y * 13.0, C, 3.0f);
	const FVector2D LabelAt = Back - D2 * 18.0;
	DrawLabel(FString::Printf(TEXT("%d m"), FMath::RoundToInt(Best / 100.0)), LabelAt.X, LabelAt.Y - 8.0f, 10, C, true, true);
}

void ARTSHUD::DrawPause()
{
	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	DrawPanel(0, 0, W, H, FLinearColor(0, 0, 0, 0.55f));
	DrawLabel(TEXT("PAUSADO"), W * 0.5f, H * 0.32f, 36, TextColor, true, true);
	DrawButton(TEXT("resume"), TEXT("CONTINUAR"), W * 0.5f - 170.0f, H * 0.45f, 340.0f, 54.0f);
	DrawButton(TEXT("menu"), TEXT("SAIR PARA O MENU"), W * 0.5f - 170.0f, H * 0.45f + 70.0f, 340.0f, 54.0f);
}

void ARTSHUD::DrawResult(URTSMatchSubsystem* Match)
{
	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	const float Alpha = FMath::Clamp((Match->SecondsSinceEnd() - ResultDelaySeconds) / 0.6f, 0.0f, 1.0f);
	// no terço de cima: a última explosão continua à vista embaixo
	const float Top = H * 0.12f;
	DrawPanel(0, Top, W, H * 0.24f, FLinearColor(0, 0, 0, 0.5f * Alpha));
	const ERTSMatchResult R = Match->Result();
	const FString Title = R == ERTSMatchResult::Victory ? TEXT("VITÓRIA") : (R == ERTSMatchResult::Defeat ? TEXT("DERROTA") : TEXT("EMPATE"));
	const FLinearColor Color = R == ERTSMatchResult::Victory ? FLinearColor(0.45f, 0.95f, 0.5f) : (R == ERTSMatchResult::Defeat ? AttackRed : TextColor);
	DrawLabel(Title, W * 0.5f, Top + 18.0f, 44, WithAlpha(Color, Alpha), true, true);
	DrawLabel(FString::Printf(TEXT("%s de partida   ·   %d destruídos   ·   %d perdidos   ·   replay salvo"),
		*Clock(Match->MatchSeconds()), Match->UnitsKilled(), Match->UnitsLost()), W * 0.5f, Top + 92.0f, 12, WithAlpha(DimText, Alpha), true);
	DrawButton(TEXT("menu"), TEXT("VOLTAR AO MENU"), W * 0.5f - 170.0f, Top + H * 0.24f + 18.0f, 340.0f, 54.0f);
}
