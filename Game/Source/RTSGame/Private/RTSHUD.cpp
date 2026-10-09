#include "RTSHUD.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "CanvasItem.h"
#include "Engine/Texture2D.h"
#include "RTSCameraPawn.h"
#include "RTSFogOfWar.h"
#include "RTSGameData.h"
#include "RTSSessionSubsystem.h"
#include "TextureResource.h"
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
	/** O minimapa: moldura de 30% da altura da tela, a 16 px dos cantos (fora da faixa da rolagem pela borda). */
	constexpr float MinimapFrameFraction = 0.30f;
	constexpr float MinimapMarginPx = 16.0f;
	/** Cores da caixa da câmera do ZH (W3DRadar: topo 225,225,0 e o resto 158,158,0). */
	const FLinearColor ViewBoxTop(FColor(225, 225, 0));
	const FLinearColor ViewBoxRest(FColor(158, 158, 0));

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
	Y += 96.0f;
	const URTSSessionSubsystem* Session = GetGameInstance() ? GetGameInstance()->GetSubsystem<URTSSessionSubsystem>() : nullptr;
	const bool bFog = !Session || Session->bFogOfWar;
	DrawButton(TEXT("fogsetting"), bFog ? TEXT("NÉVOA DE GUERRA: LIGADA") : TEXT("NÉVOA DE GUERRA: DESLIGADA"), X, Y, PanelW - X * 2.0f, 44.0f);
	DrawLabel(TEXT("depuração: F9 liga e desliga dentro da partida"), X + 2.0f, Y + 52.0f, 10, DimText);
	Y += 96.0f;
	DrawButton(TEXT("quit"), TEXT("SAIR"), X, Y, PanelW - X * 2.0f, 58.0f);
}

void ARTSHUD::DrawMatch(ARTSPlayerController* PC, URTSMatchSubsystem* Match)
{
	if (!Match->IsReady())
	{
		// a tela de carregar continua (a mesma da troca de mapa) até a cena estar pronta
		DrawPanel(0, 0, Canvas->SizeX, Canvas->SizeY, FLinearColor(0.012f, 0.014f, 0.017f, 1.0f));
		DrawLabel(TEXT("CARREGANDO"), Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f - 30.0f, 26, FLinearColor(0.92f, 0.93f, 0.95f), true, true);
		const int32 Dots = int32(FPlatformTime::Seconds() * 3.0) % 4;
		DrawLabel(FString::ChrN(Dots, TEXT('.')), Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f + 12.0f, 20, FLinearColor(0.92f, 0.93f, 0.95f, 0.7f), true);
		return;
	}
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
	DrawMinimap(PC, Match);
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
		DrawLabel(Match->DebugLine(), 16.0f, 46.0f, 10, Accent);
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
		if (!Match->IsUnitShown(U.Id))
		{
			continue;   // na névoa: nem anel nem barra
		}
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
	const FString Right = Match->IsFogShown() ? FString::Printf(TEXT("Destruídos  %d"), Match->UnitsKilled()) : FString::Printf(TEXT("Inimigo  %d"), Theirs);
	DrawLabel(Right, W * 0.5f + 120.0f, 9.0f, 11, Brighten(URTSMatchSubsystem::PlayerColor(1), 0.35f), false, true);
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
		if (!Match->IsEnemyOfLocal(U.Owner) || !Match->IsUnitShown(U.Id))
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
	FVector2D TargetCm;
	bool bBase = false;
	if (Nearest)
	{
		TargetCm = Nearest->PosCm;
	}
	else
	{
		// nada à vista: com névoa, aponta a largada do inimigo (todo mundo conhece pelo mapa) até ela ser vista
		URTSFogOfWar* Fog = Match->GetFog();
		const FVector2D Start = Match->PlayerStartCm(1);
		if (!Match->IsFogShown() || !Fog || Fog->IsExplored(Start))
		{
			return;
		}
		TargetCm = Start;
		Best = FVector2D::Distance(Start, FVector2D(Center.X, Center.Y));
		bBase = true;
	}
	// a direção na tela: a do inimigo projetado (ou a do chão, girada pela vista)
	const FVector Target(TargetCm.X, TargetCm.Y, Map ? Map->GroundZCm(TargetCm) : 0.0f);
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
	DrawLabel(bBase ? FString::Printf(TEXT("base inimiga  %d m"), FMath::RoundToInt(Best / 100.0)) : FString::Printf(TEXT("%d m"), FMath::RoundToInt(Best / 100.0)),
		LabelAt.X, LabelAt.Y - 8.0f, 10, C, true, true);
}

void ARTSHUD::DrawPause()
{
	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	DrawPanel(0, 0, W, H, FLinearColor(0, 0, 0, 0.55f));
	DrawLabel(TEXT("PAUSADO"), W * 0.5f, H * 0.32f, 36, TextColor, true, true);
	DrawButton(TEXT("resume"), TEXT("CONTINUAR"), W * 0.5f - 170.0f, H * 0.45f, 340.0f, 54.0f);
	DrawButton(TEXT("menu"), TEXT("SAIR PARA O MENU"), W * 0.5f - 170.0f, H * 0.45f + 70.0f, 340.0f, 54.0f);
	const URTSMatchSubsystem* Match = GetWorld()->GetSubsystem<URTSMatchSubsystem>();
	const bool bFog = Match && Match->IsFogOn();
	DrawButton(TEXT("fog"), bFog ? TEXT("NÉVOA: LIGADA  (F9)") : TEXT("NÉVOA: DESLIGADA  (F9)"), W * 0.5f - 170.0f, H * 0.45f + 140.0f, 340.0f, 44.0f);
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

// ---------------------------------------------------------------- minimapa

FVector2D ARTSHUD::MinimapUVToScreen(const FVector2D& UV) const
{
	// o quadrado do mapa girado pelo giro padrão da câmera: "pra frente" na vista padrão é "pra
	// cima" no minimapa, e a caixa da câmera fica em pé (o radar do ZH fica alinhado com a
	// câmera padrão dele)
	const double X = (UV.X - 0.5) * MinimapSide;
	const double Y = (UV.Y - 0.5) * MinimapSide;
	return MinimapCenter + FVector2D(X * MinimapCos + Y * MinimapSin, -X * MinimapSin + Y * MinimapCos);
}

FVector2D ARTSHUD::MinimapScreenToUV(const FVector2D& Screen) const
{
	const FVector2D R = Screen - MinimapCenter;
	const double X = R.X * MinimapCos - R.Y * MinimapSin;
	const double Y = R.X * MinimapSin + R.Y * MinimapCos;
	return FVector2D(X / MinimapSide + 0.5, Y / MinimapSide + 0.5);
}

bool ARTSHUD::IsOverMinimap(const FVector2D& Screen) const
{
	return bMinimapDrawn && MinimapFrame.IsInside(Screen);
}

bool ARTSHUD::MinimapToWorld(const FVector2D& Screen, FVector2D& OutCm, bool bClamp) const
{
	if (!bMinimapDrawn)
	{
		return false;
	}
	FVector2D UV = MinimapScreenToUV(Screen);
	const bool bInside = UV.X >= 0.0 && UV.X <= 1.0 && UV.Y >= 0.0 && UV.Y <= 1.0;
	if (!bInside && !bClamp)
	{
		return false;
	}
	UV = FVector2D(FMath::Clamp(UV.X, 0.0, 1.0), FMath::Clamp(UV.Y, 0.0, 1.0));
	OutCm = FVector2D(MinimapMinCm.X + (1.0 - UV.Y) * MinimapSizeCm.X, MinimapMinCm.Y + UV.X * MinimapSizeCm.Y);
	return true;
}

bool ARTSHUD::WorldToMinimapScreen(const FVector2D& Cm, FVector2D& OutScreen) const
{
	if (!bMinimapDrawn)
	{
		return false;
	}
	const FVector2D UV((Cm.Y - MinimapMinCm.Y) / MinimapSizeCm.Y, 1.0 - (Cm.X - MinimapMinCm.X) / MinimapSizeCm.X);
	OutScreen = MinimapUVToScreen(UV);
	return true;
}

void ARTSHUD::DrawClippedLine(FVector2D A, FVector2D B, const FLinearColor& Color, float Thickness)
{
	// Liang–Barsky na moldura do minimapa (o ZH corta a caixa da câmera no retângulo do radar)
	const FVector2D D = B - A;
	double T0 = 0.0;
	double T1 = 1.0;
	const double P[4] = { -D.X, D.X, -D.Y, D.Y };
	const double Q[4] = { A.X - MinimapFrame.Min.X, MinimapFrame.Max.X - A.X, A.Y - MinimapFrame.Min.Y, MinimapFrame.Max.Y - A.Y };
	for (int32 K = 0; K < 4; ++K)
	{
		if (FMath::Abs(P[K]) < 1e-9)
		{
			if (Q[K] < 0.0)
			{
				return;
			}
			continue;
		}
		const double T = Q[K] / P[K];
		if (P[K] < 0.0) { T0 = FMath::Max(T0, T); } else { T1 = FMath::Min(T1, T); }
	}
	if (T0 > T1)
	{
		return;
	}
	const FVector2D C0 = A + D * T0;
	const FVector2D C1 = A + D * T1;
	DrawLine(C0.X, C0.Y, C1.X, C1.Y, Color, Thickness);
}

void ARTSHUD::DrawMinimap(ARTSPlayerController* PC, URTSMatchSubsystem* Match)
{
	URTSFogOfWar* Fog = Match->GetFog();
	UTexture2D* Tex = Fog ? Fog->MinimapTexture() : nullptr;
	if (!Fog || !Fog->IsMinimapReady() || !Tex || !Tex->GetResource())
	{
		bMinimapDrawn = false;
		return;
	}
	const float F = FMath::RoundToFloat(Canvas->SizeY * MinimapFrameFraction);
	const float X0 = MinimapMarginPx;
	const float Y0 = Canvas->SizeY - MinimapMarginPx - F;
	MinimapFrame = FBox2D(FVector2D(X0, Y0), FVector2D(X0 + F, Y0 + F));
	MinimapCenter = FVector2D(X0 + F * 0.5f, Y0 + F * 0.5f);
	MinimapSide = F / UE_SQRT_2 * 0.97f;
	const ARTSCameraPawn* Cam = Cast<ARTSCameraPawn>(PC->GetPawn());
	const float YawRad = FMath::DegreesToRadians(Cam ? Cam->DefaultYawDeg : 45.0f);
	MinimapCos = FMath::Cos(YawRad);
	MinimapSin = FMath::Sin(YawRad);
	MinimapMinCm = Fog->MapMinCm();
	MinimapSizeCm = Fog->MapSizeCm();
	bMinimapDrawn = true;

	// moldura e o mapa (dois triângulos com os cantos girados: o mesmo cálculo dos pontos)
	DrawPanel(X0, Y0, F, F, FLinearColor(0.012f, 0.014f, 0.017f, 0.82f));
	const FVector2D C00 = MinimapUVToScreen(FVector2D(0, 0));
	const FVector2D C10 = MinimapUVToScreen(FVector2D(1, 0));
	const FVector2D C11 = MinimapUVToScreen(FVector2D(1, 1));
	const FVector2D C01 = MinimapUVToScreen(FVector2D(0, 1));
	for (int32 T = 0; T < 2; ++T)
	{
		FCanvasTriangleItem Tri = T == 0
			? FCanvasTriangleItem(C00, C10, C11, FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, 1), Tex->GetResource())
			: FCanvasTriangleItem(C00, C11, C01, FVector2D(0, 0), FVector2D(1, 1), FVector2D(0, 1), Tex->GetResource());
		Tri.BlendMode = SE_BLEND_Opaque;
		Canvas->DrawItem(Tri);
	}
	const FLinearColor Edge(0.62f, 0.66f, 0.72f, 0.55f);
	DrawLine(C00.X, C00.Y, C10.X, C10.Y, Edge, 1.0f);
	DrawLine(C10.X, C10.Y, C11.X, C11.Y, Edge, 1.0f);
	DrawLine(C11.X, C11.Y, C01.X, C01.Y, Edge, 1.0f);
	DrawLine(C01.X, C01.Y, C00.X, C00.Y, Edge, 1.0f);

	// unidades (inimigo só à vista), na cor do jogador; selecionado com borda clara
	// ~5 px a 900 linhas: o tanque de 8 m tem 1,5 px no mapa de 1 km, então o ponto é maior que ele
	const float Dot = FMath::Max(4.0f, F / 55.0f);
	TArray<FRTSUnitState> Units;
	Match->GetUnits(Units);
	const TArray<uint32>& Selected = PC->GetSelection();
	for (const FRTSUnitState& U : Units)
	{
		if (!Match->IsUnitShown(U.Id))
		{
			continue;
		}
		FVector2D P;
		WorldToMinimapScreen(U.PosCm, P);
		// contorno: claro no selecionado, escuro no resto (lê em cima da areia e da salina)
		const FLinearColor Outline = Selected.Contains(U.Id) ? FLinearColor(0.95f, 0.97f, 1.0f, 0.95f) : FLinearColor(0.0f, 0.0f, 0.0f, 0.8f);
		DrawPanel(P.X - Dot * 0.5f - 1.0f, P.Y - Dot * 0.5f - 1.0f, Dot + 2.0f, Dot + 2.0f, Outline);
		DrawPanel(P.X - Dot * 0.5f, P.Y - Dot * 0.5f, Dot, Dot, Brighten(URTSMatchSubsystem::PlayerColor(U.Owner), 0.15f));
	}

	// a caixa da câmera: os 4 cantos da tela no plano do chão em volta do foco
	const FRTSBakedMap* Map = Match->BakedMap();
	const FVector2D Focus = Cam ? Cam->GetFocusCm() : FVector2D::ZeroVector;
	const float PlaneZ = Map ? Map->GroundZCm(Focus) : 0.0f;
	const FVector2D Corners[4] = { FVector2D(0, 0), FVector2D(Canvas->SizeX, 0), FVector2D(Canvas->SizeX, Canvas->SizeY), FVector2D(0, Canvas->SizeY) };
	FVector2D Box[4];
	for (int32 K = 0; K < 4; ++K)
	{
		FVector O;
		FVector D;
		if (!PlayerOwner->DeprojectScreenPositionToWorld(Corners[K].X, Corners[K].Y, O, D))
		{
			return;
		}
		// raio que não cruza o chão perto (câmera deitada): limita a 2 km
		double T = 200000.0;
		if (D.Z < -1e-3)
		{
			T = FMath::Min(T, (PlaneZ - O.Z) / D.Z);
		}
		const FVector Hit = O + D * T;
		WorldToMinimapScreen(FVector2D(Hit.X, Hit.Y), Box[K]);
	}
	DrawClippedLine(Box[0], Box[1], ViewBoxTop, 1.5f);
	DrawClippedLine(Box[1], Box[2], ViewBoxRest, 1.5f);
	DrawClippedLine(Box[2], Box[3], ViewBoxRest, 1.5f);
	DrawClippedLine(Box[3], Box[0], ViewBoxRest, 1.5f);

	// ordens dadas pelo minimapa: um pulso que fecha no destino
	for (const FRTSMinimapPulse& Pulse : PC->GetMinimapPulses())
	{
		FVector2D P;
		WorldToMinimapScreen(Pulse.Cm, P);
		const float T = FMath::Clamp(Pulse.Age / 0.7f, 0.0f, 1.0f);
		const float R = FMath::Lerp(F * 0.08f, 2.0f, T);
		const FLinearColor C = WithAlpha(Pulse.bAttack ? AttackRed : MoveGreen, 1.0f - T);
		for (int32 K = 0; K < 12; ++K)
		{
			const float A0 = K * PI / 6.0f;
			const float A1 = (K + 1) * PI / 6.0f;
			DrawLine(P.X + R * FMath::Cos(A0), P.Y + R * FMath::Sin(A0), P.X + R * FMath::Cos(A1), P.Y + R * FMath::Sin(A1), C, 1.5f);
		}
	}

	// "sob ataque" (ZH Radar.cpp): triângulo vermelho que gira e fecha da metade do radar até 6 px
	// em 1,5 s, fica até 3,5 s e some no último meio segundo
	for (const FRTSMinimapPing& Ping : PC->GetMinimapPings())
	{
		FVector2D P;
		WorldToMinimapScreen(Ping.Cm, P);
		const float Shrink = FMath::Clamp(Ping.Age / 1.5f, 0.0f, 1.0f);
		const float R = FMath::Lerp(MinimapSide * 0.5f, 6.0f, Shrink);
		const float Alpha = Ping.Age > 3.5f ? FMath::Clamp((4.0f - Ping.Age) / 0.5f, 0.0f, 1.0f) : 1.0f;
		const FLinearColor C = WithAlpha(FMath::Lerp(FLinearColor(FColor(255, 0, 0)), FLinearColor(FColor(255, 128, 128)), Shrink), Alpha);
		const float Spin = Ping.Age * 2.0f * PI * 0.5f;
		FVector2D V[3];
		for (int32 K = 0; K < 3; ++K)
		{
			const float A = Spin + K * 2.0f * PI / 3.0f - PI * 0.5f;
			V[K] = P + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R;
		}
		for (int32 K = 0; K < 3; ++K)
		{
			DrawClippedLine(V[K], V[(K + 1) % 3], C, 2.0f);
		}
	}
	if (PC->AttackMessageAlpha() > 0.0f)
	{
		DrawLabel(TEXT("Unidade sob ataque  (Espaço vai até lá)"), X0 + 2.0f, Y0 - 22.0f, 11, WithAlpha(AttackRed, PC->AttackMessageAlpha()), false, true);
	}
}
